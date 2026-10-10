#include "UI/AbyssUiSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "TextureResource.h"
#include "UObject/UObjectGlobals.h"

#include <string>

#include "abyss/data/DataStore.h"

#include "Abyssfire.h"
#include "Framework/AbyssGameInstance.h"
#include "Framework/AbyssText.h"
#include "Input/AbyssInputSubsystem.h"
#include "Input/Touch/AbyssTouchStyle.h"
#include "UI/AbyssUiStyle.h"
#include "UI/Core/AbyssUiContext.h"
#include "UI/Hud/AbyssMinimapTexture.h"
#include "UI/Root/SAbyssUiRoot.h"
#include "UI/World/SAbyssWorldLayer.h"
#include "World/AbyssWorldBuilder.h"

namespace
{
	/** "potion_hp" -> "PotionHp" (HUD icon asset names, T_UI_HudIcon_<Pascal>). */
	FString AbyssUiSubsystem_Pascal(const FString& Id)
	{
		FString Out;
		bool bUpper = true;
		for (const TCHAR Character : Id)
		{
			if (Character == TEXT('_') || Character == TEXT('-') || Character == TEXT(' '))
			{
				bUpper = true;
				continue;
			}
			Out.AppendChar(bUpper ? FChar::ToUpper(Character) : Character);
			bUpper = false;
		}
		return Out;
	}

	/** Content folders searched for UI textures, in order (art-inventory 9 / R10 / R11 import layout). */
	const TCHAR* const GAbyssUiTextureFolders[] = {
		TEXT("UI/Icons"), TEXT("UI/Portraits"), TEXT("UI"), TEXT("Icons"), TEXT("Portraits"), TEXT("Textures"),
	};
}

UAbyssUiSubsystem::UAbyssUiSubsystem()
{
}

UAbyssUiSubsystem::~UAbyssUiSubsystem() = default;

UAbyssUiSubsystem* UAbyssUiSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine != nullptr
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UAbyssUiSubsystem>() : nullptr;
}

// =====================================================================================================================
// USubsystem
// =====================================================================================================================

bool UAbyssUiSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (IsRunningDedicatedServer() || IsRunningCommandlet())
	{
		return false;
	}
	return Cast<UAbyssGameInstance>(Outer) != nullptr && Super::ShouldCreateSubsystem(Outer);
}

void UAbyssUiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// The input bridge must exist first: the UI answers its requests and hosts its touch layer.
	Collection.InitializeDependency<UAbyssInputSubsystem>();

	// The theme and locale are applied once the data tables are loaded (GameInstance::Init runs after the subsystems).
	Style = MakeShared<FAbyssUiStyle>();
	CreateSolidWhiteTexture();
	Context = MakeShared<FAbyssUiContext>(*this, Style.ToSharedRef());
	Minimap = MakeShared<FAbyssMinimapTexture>(*this);

	UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
	if (GameInstance != nullptr)
	{
		FAbyssEventRouter& Router = GameInstance->GetEventRouter();
		Router.OnAnyEvent.AddUObject(this, &UAbyssUiSubsystem::HandleCoreEvent);
		Router.OnSessionStarted.AddUObject(this, &UAbyssUiSubsystem::HandleSessionStarted);
		Router.OnSessionEnded.AddUObject(this, &UAbyssUiSubsystem::HandleSessionEnded);
		bEventsBound = true;
		SettingsHandle = GameInstance->OnSettingsChanged.AddUObject(this, &UAbyssUiSubsystem::HandleSettingsChanged);
	}
	BindInputHandler();
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &UAbyssUiSubsystem::HandleWorldCleanup);

	if (GameInstance != nullptr)
	{
		GameInstance->RegisterUiRoot(this);  // calls OnAppStateChanged with the current state
	}
}

void UAbyssUiSubsystem::Deinitialize()
{
	UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
	if (GameInstance != nullptr)
	{
		GameInstance->UnregisterUiRoot(this);
		if (bEventsBound)
		{
			GameInstance->GetEventRouter().RemoveAll(this);
			bEventsBound = false;
		}
		GameInstance->OnSettingsChanged.Remove(SettingsHandle);
		if (bInputHandlerBound)
		{
			if (UAbyssInputSubsystem* Input = GameInstance->GetSubsystem<UAbyssInputSubsystem>())
			{
				Input->ClearUiInputHandler();
				Input->OnHoveredEntityChanged.Remove(HoverHandle);
			}
			bInputHandlerBound = false;
		}
	}
	SettingsHandle.Reset();
	HoverHandle.Reset();
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	WorldCleanupHandle.Reset();

	DetachRootFromViewport();
	if (Minimap.IsValid())
	{
		Minimap->Reset();
	}
	RootWidget.Reset();
	if (Context.IsValid())
	{
		Context->SetHost(nullptr);
	}
	Minimap.Reset();
	Context.Reset();
	Style.Reset();
	TextureCache.Reset();
	MissingTextures.Reset();
	KeptAssets.Reset();
	Super::Deinitialize();
}

// =====================================================================================================================
// Root widget and viewport
// =====================================================================================================================

void UAbyssUiSubsystem::EnsureRootWidget()
{
	const UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
	if (!bThemeApplied && GameInstance != nullptr && GameInstance->IsDataReady())
	{
		// ui_theme.json colours (Data agent export); the fallbacks are the web values.
		Style->Initialize(&GameInstance->GetData()->UiTheme());
		bThemeApplied = true;
	}
	RefreshStyleLocale();
	if (RootWidget.IsValid())
	{
		return;
	}
	SAssignNew(RootWidget, SAbyssUiRoot, Context.ToSharedRef(), Minimap.ToSharedRef());

	// The touch layer lives inside the HUD's safe zone and draws its icons / font from the UI style (ue58 8.2).
	if (GameInstance != nullptr)
	{
		if (UAbyssInputSubsystem* Input = GameInstance->GetSubsystem<UAbyssInputSubsystem>())
		{
			FAbyssTouchVisuals Visuals;
			const TWeakPtr<FAbyssUiContext> WeakContext = Context;
			Visuals.SkillIcon = [WeakContext](const FString& SkillId) -> const FSlateBrush*
			{
				const TSharedPtr<FAbyssUiContext> Pinned = WeakContext.Pin();
				return Pinned.IsValid() ? Pinned->SkillIcon(AbyssText::ToStd(SkillId)) : nullptr;
			};
			Visuals.HudIcon = [WeakContext](FName IconId) -> const FSlateBrush*
			{
				const TSharedPtr<FAbyssUiContext> Pinned = WeakContext.Pin();
				if (!Pinned.IsValid() || IconId.IsNone())
				{
					return nullptr;
				}
				return Pinned->TextureBrush(FName(*(TEXT("T_UI_HudIcon_") + AbyssUiSubsystem_Pascal(IconId.ToString()))));
			};
			Visuals.Font = Style->Body(16.f, true, 1);
			RootWidget->SetTouchControls(Input->CreateTouchControls(Visuals));
		}
	}
}

void UAbyssUiSubsystem::AttachRootToViewport()
{
	if (!RootWidget.IsValid())
	{
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	UGameViewportClient* Viewport = GameInstance != nullptr ? GameInstance->GetGameViewportClient() : nullptr;
	if (Viewport == nullptr || AttachedViewport.Get() == Viewport)
	{
		return;
	}
	DetachRootFromViewport();
	// Content added here sits under the viewport's DPI scaler (ScaleToFit 1280x720, ue58-platform.md 9.1 / 9.2).
	Viewport->AddViewportWidgetContent(RootWidget.ToSharedRef(), 10);
	AttachedViewport = Viewport;
	if (PendingError.IsSet())
	{
		RootWidget->ShowSystemError(PendingError->Key, PendingError->Value);
		PendingError.Reset();
	}
}

void UAbyssUiSubsystem::DetachRootFromViewport()
{
	if (UGameViewportClient* Viewport = AttachedViewport.Get())
	{
		if (RootWidget.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(RootWidget.ToSharedRef());
		}
	}
	AttachedViewport.Reset();
}

void UAbyssUiSubsystem::RegisterWithWorld()
{
	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
	if (World == nullptr || RegisteredWorld.Get() == World)
	{
		return;
	}
	if (UAbyssWorldBuilder* Builder = World->GetSubsystem<UAbyssWorldBuilder>())
	{
		Builder->SetWorldUi(this);  // replays the live world widgets into the layer
		RegisteredWorld = World;
	}
}

void UAbyssUiSubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (World == nullptr || World != RegisteredWorld.Get())
	{
		return;
	}
	RegisteredWorld.Reset();
	if (RootWidget.IsValid())
	{
		RootWidget->GetWorldLayer().ClearWidgets();
	}
	// The viewport content does not survive a world teardown: attach again on the next app state / frame.
	DetachRootFromViewport();
}

void UAbyssUiSubsystem::RefreshStyleLocale()
{
	const UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
	if (Style.IsValid() && GameInstance != nullptr && GameInstance->IsDataReady())
	{
		Style->SetLocale(GameInstance->GetStrings().Current());
	}
}

// =====================================================================================================================
// IAbyssUiRoot
// =====================================================================================================================

void UAbyssUiSubsystem::OnAppStateChanged(EAbyssAppState NewState)
{
	AppState = NewState;
	if (NewState == EAbyssAppState::Boot)
	{
		return;  // no viewport yet: the title appears with MainMenu (GameMode BeginPlay)
	}
	EnsureRootWidget();
	AttachRootToViewport();
	RegisterWithWorld();
	RootWidget->SetAppState(NewState);
}

void UAbyssUiSubsystem::SyncFrame(const abyss::Snapshot& Snap, const FAbyssFrameInfo& Frame)
{
	if (!RootWidget.IsValid())
	{
		return;
	}
	AttachRootToViewport();
	RegisterWithWorld();
	RootWidget->SyncFrame(Snap, Frame);
}

bool UAbyssUiSubsystem::HandleBack()
{
	return RootWidget.IsValid() && RootWidget->HandleBack();
}

void UAbyssUiSubsystem::OnLocaleChanged()
{
	RefreshStyleLocale();
	if (RootWidget.IsValid())
	{
		RootWidget->RequestLocaleRefresh();
	}
}

void UAbyssUiSubsystem::ShowSystemError(const FText& Title, const FText& Message)
{
	UE_LOG(LogAbyss, Warning, TEXT("UI system error: %s - %s"), *Title.ToString(), *Message.ToString());
	if (!RootWidget.IsValid() || !AttachedViewport.IsValid())
	{
		PendingError = TPair<FText, FText>(Title, Message);  // shown once the root is on screen
		return;
	}
	RootWidget->ShowSystemError(Title, Message);
}

// =====================================================================================================================
// IAbyssWorldUi
// =====================================================================================================================

void UAbyssUiSubsystem::AddWorldWidget(const FAbyssWorldWidgetDesc& Desc)
{
	EnsureRootWidget();
	RootWidget->GetWorldLayer().AddWidget(Desc);
}

void UAbyssUiSubsystem::RemoveWorldWidget(abyss::EntityId Id)
{
	if (RootWidget.IsValid())
	{
		RootWidget->GetWorldLayer().RemoveWidget(Id);
	}
}

void UAbyssUiSubsystem::ClearWorldWidgets()
{
	if (RootWidget.IsValid())
	{
		RootWidget->GetWorldLayer().ClearWidgets();
	}
}

void UAbyssUiSubsystem::UpdateWorldWidgets(TConstArrayView<FAbyssWorldWidgetFrame> Frames)
{
	if (RootWidget.IsValid())
	{
		RootWidget->GetWorldLayer().UpdateFrames(Frames);
	}
}

void UAbyssUiSubsystem::ShowFloatingText(const FAbyssFloatingTextRequest& Request)
{
	if (RootWidget.IsValid())
	{
		RootWidget->GetWorldLayer().AddFloatingText(Request);
	}
}

// =====================================================================================================================
// Router / input / settings
// =====================================================================================================================

void UAbyssUiSubsystem::HandleCoreEvent(const abyss::Event& Event)
{
	if (!RootWidget.IsValid())
	{
		return;
	}
	const UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
	RootWidget->HandleCoreEvent(Event, GameInstance != nullptr ? GameInstance->GetSnapshot() : nullptr);
}

void UAbyssUiSubsystem::HandleSessionStarted()
{
	EnsureRootWidget();
	if (Minimap.IsValid())
	{
		Minimap->Reset();
	}
	RootWidget->OnSessionStarted();
}

void UAbyssUiSubsystem::HandleSessionEnded()
{
	if (RootWidget.IsValid())
	{
		RootWidget->OnSessionEnded();
	}
	if (Minimap.IsValid())
	{
		Minimap->Reset();
	}
}

void UAbyssUiSubsystem::HandleSettingsChanged(const FAbyssUserSettings& NewSettings)
{
	if (RootWidget.IsValid())
	{
		RootWidget->OnSettingsChanged();
	}
}

void UAbyssUiSubsystem::HandleHoveredEntityChanged(abyss::EntityId Entity)
{
	if (RootWidget.IsValid())
	{
		RootWidget->SetHoveredEntity(Entity);
	}
}

bool UAbyssUiSubsystem::HandleUiInputRequest(const FAbyssUiInputRequest& Request)
{
	return RootWidget.IsValid() && RootWidget->HandleUiInput(Request);
}

void UAbyssUiSubsystem::BindInputHandler()
{
	UGameInstance* GameInstance = GetGameInstance();
	UAbyssInputSubsystem* Input = GameInstance != nullptr ? GameInstance->GetSubsystem<UAbyssInputSubsystem>() : nullptr;
	if (Input == nullptr)
	{
		return;
	}
	Input->SetUiInputHandler(FAbyssUiInputHandler::CreateUObject(this, &UAbyssUiSubsystem::HandleUiInputRequest));
	HoverHandle = Input->OnHoveredEntityChanged.AddUObject(this, &UAbyssUiSubsystem::HandleHoveredEntityChanged);
	bInputHandlerBound = true;
}

// =====================================================================================================================
// Services
// =====================================================================================================================

UAbyssGameInstance* UAbyssUiSubsystem::GetAbyssGameInstance() const
{
	return Cast<UAbyssGameInstance>(GetGameInstance());
}

const FAbyssUiStyle& UAbyssUiSubsystem::GetStyle() const
{
	check(Style.IsValid());
	return *Style;
}

UTexture2D* UAbyssUiSubsystem::FindUiTexture(FName AssetName)
{
	check(IsInGameThread());
	if (AssetName.IsNone())
	{
		return nullptr;
	}
	if (const TWeakObjectPtr<UTexture2D>* Cached = TextureCache.Find(AssetName))
	{
		if (UTexture2D* Texture = Cached->Get())
		{
			return Texture;
		}
	}
	if (MissingTextures.Contains(AssetName))
	{
		return nullptr;
	}
	const FString Name = AssetName.ToString();
	const FString Root(TEXT("/Game/Abyssfire"));
	for (const TCHAR* Folder : GAbyssUiTextureFolders)
	{
		const FString PackageName = Root / Folder / Name;
		if (!FPackageName::DoesPackageExist(PackageName))
		{
			continue;
		}
		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackageName, *Name);
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			KeepAlive(Texture);
			TextureCache.Add(AssetName, Texture);
			return Texture;
		}
	}
	// Missing art is expected while the pipeline catches up: the widgets draw their procedural fallback.
	UE_LOG(LogAbyss, Verbose, TEXT("UI texture %s not found (fallback glyph)"), *Name);
	MissingTextures.Add(AssetName);
	return nullptr;
}

void UAbyssUiSubsystem::CreateSolidWhiteTexture()
{
	// 1x1 white texture behind FAbyssUiStyle::SolidTexture(): the custom-vertex shapes (diamonds, glows, pies, orbs) need
	// a brush with a real resource proxy for FSlateRenderer::GetResourceHandle (a colour brush has none).
	UTexture2D* Texture = UTexture2D::CreateTransient(1, 1, PF_B8G8R8A8, FName(TEXT("AbyssUiSolidWhite")));
	if (Texture == nullptr || Texture->GetPlatformData() == nullptr || Texture->GetPlatformData()->Mips.Num() == 0)
	{
		UE_LOG(LogAbyss, Warning, TEXT("UI: could not create the solid white texture; custom-vertex shapes fall back to Slate's default texture"));
		return;
	}
	Texture->Filter = TF_Nearest;
	Texture->SRGB = true;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	Texture->LODGroup = TEXTUREGROUP_UI;
	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	const FColor White = FColor::White;
	FMemory::Memcpy(Data, &White, sizeof(FColor));
	Mip.BulkData.Unlock();
	Texture->UpdateResource();
	KeepAlive(Texture);
	Style->SetSolidTexture(Texture);
}

void UAbyssUiSubsystem::KeepAlive(UObject* Object)
{
	if (Object != nullptr)
	{
		KeptAssets.AddUnique(Object);
	}
}

void UAbyssUiSubsystem::ReleaseKeptAlive(UObject* Object)
{
	if (Object != nullptr)
	{
		KeptAssets.Remove(Object);
	}
}

void UAbyssUiSubsystem::PlayUiSound(abyss::SfxId Cue) const
{
	UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
	if (GameInstance == nullptr)
	{
		return;
	}
	abyss::EvSfx Event;
	Event.cue = Cue;
	Event.spatial = false;
	Event.source = abyss::kNoEntity;
	GameInstance->GetEventRouter().On<abyss::EvSfx>().Broadcast(Event);
}
