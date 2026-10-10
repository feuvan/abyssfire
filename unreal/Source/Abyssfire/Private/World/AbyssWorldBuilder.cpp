#include "World/AbyssWorldBuilder.h"

#include "Abyssfire.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"

#include <string>
#include <string_view>

#include "abyss/base/Enums.h"
#include "abyss/combat/HitFeedback.h"
#include "abyss/data/CombatData.h"
#include "abyss/data/DataStore.h"
#include "abyss/data/ItemData.h"
#include "abyss/data/MapData.h"
#include "abyss/data/MonsterData.h"
#include "abyss/data/NpcData.h"
#include "abyss/data/PetData.h"
#include "abyss/data/StoryData.h"
#include "abyss/data/UiData.h"
#include "abyss/items/Inventory.h"
#include "abyss/items/Item.h"
#include "abyss/quests/QuestGuide.h"
#include "abyss/sim/Events.h"
#include "abyss/sim/Snapshot.h"

#include "Actors/AbyssCharacterActor.h"
#include "Actors/AbyssPropActor.h"
#include "Camera/AbyssCameraRig.h"
#include "Framework/AbyssActorRegistry.h"
#include "Framework/AbyssGameInstance.h"
#include "Framework/AbyssSimDriver.h"
#include "Framework/AbyssText.h"
#include "Framework/AbyssUiRoot.h"
#include "Framework/AbyssUnits.h"
#include "Platform/AbyssPlatform.h"
#include "Platform/AbyssSettings.h"
#include "Vfx/AbyssVfxSystem.h"
#include "World/AbyssArtManifest.h"
#include "World/AbyssAssetLibrary.h"
#include "World/AbyssZoneActor.h"

namespace AbyssWorldBuilderPrivate
{
	constexpr float NearMonsterOcclusionTiles = 6.f;   // world-map-nav.md 15.6: monsters within ~6 tiles fade props too
	constexpr float GuideRadiusCm = 110.f;             // art-inventory-ch1.md 6.8: ~1.1 m ring around the hero
	constexpr float GuideHeightCm = 30.f;
	constexpr float FlyingHoverCm = 45.f;
	constexpr float LootFlySec = 0.3f;                 // ZoneScene.ts:4030-4065
	constexpr float QuestItemFlySec = 0.42f;           // QuestWorld.ts:286-330

	FString ToF(std::string_view Text)
	{
		return AbyssText::ToFString(Text);
	}

	FName N(const FString& Text)
	{
		return Text.IsEmpty() ? NAME_None : FName(*Text);
	}

	bool StartsWith(const std::string& Text, std::string_view Prefix)
	{
		return Text.size() >= Prefix.size() && std::string_view(Text).substr(0, Prefix.size()) == Prefix;
	}

	FLinearColor QualityTint(abyss::ItemQuality Quality)
	{
		switch (Quality)
		{
		case abyss::ItemQuality::Magic: return AbyssWorldUtil::ColorFromRgb(0x4F8CFF);
		case abyss::ItemQuality::Rare: return AbyssWorldUtil::ColorFromRgb(0xFFD84A);
		case abyss::ItemQuality::Legendary: return AbyssWorldUtil::ColorFromRgb(0xFF8A2A);
		case abyss::ItemQuality::Set: return AbyssWorldUtil::ColorFromRgb(0x3ECF6A);
		case abyss::ItemQuality::Normal: break;
		}
		return FLinearColor::White;
	}

	/** Lore pool colours of the web lore props (LoreProps.ts; art-inventory-ch1.md 6.7). */
	FLinearColor LorePoolColor(const std::string& SpriteType)
	{
		if (SpriteType == "ancient_tablet") return AbyssWorldUtil::ColorFromRgb(0xFFC850);
		if (SpriteType == "crystal_shard") return AbyssWorldUtil::ColorFromRgb(0x66D8FF);
		if (SpriteType == "carved_stone") return AbyssWorldUtil::ColorFromRgb(0xB0D0FF);
		if (SpriteType == "rune_pillar") return AbyssWorldUtil::ColorFromRgb(0x9A7CFF);
		return AbyssWorldUtil::ColorFromRgb(0xFFE6A8);
	}

	FVector2D ToFlat(const abyss::Vec2& V)
	{
		return FVector2D(V.x, V.y);
	}

	FVector2D DirectionTo(const abyss::Vec2& From, const abyss::Vec2& To)
	{
		const FVector2D Delta(To.x - From.x, To.y - From.y);
		return Delta.IsNearlyZero(1e-6) ? FVector2D::ZeroVector : Delta.GetSafeNormal();
	}
}

// =====================================================================================================================
// Lifecycle
// =====================================================================================================================

UAbyssWorldBuilder* UAbyssWorldBuilder::Get(const UObject* WorldContextObject)
{
	const UWorld* OwningWorld = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return OwningWorld ? OwningWorld->GetSubsystem<UAbyssWorldBuilder>() : nullptr;
}

bool UAbyssWorldBuilder::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UAbyssWorldBuilder::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UAbyssActorRegistry>();
	Collection.InitializeDependency<UAbyssVfxSystem>();
	if (UAbyssSimDriver* Driver = Collection.InitializeDependency<UAbyssSimDriver>())
	{
		Driver->RegisterWorldView(this);
		bRegistered = true;
	}
	BindEvents();
}

void UAbyssWorldBuilder::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	BindEvents();
	RefreshQuality();
	EnsureCameraRig();
	EnsureHelpers();
}

void UAbyssWorldBuilder::Deinitialize()
{
	if (bRegistered)
	{
		if (UAbyssSimDriver* Driver = UAbyssSimDriver::Get(this))
		{
			Driver->UnregisterWorldView(this);
		}
		bRegistered = false;
	}
	UnbindEvents();
	DestroyAllEntities(/*bKeepHero*/ false);
	if (ZoneActor != nullptr)
	{
		ZoneActor->Destroy();
		ZoneActor = nullptr;
	}
	if (CameraRig != nullptr)
	{
		CameraRig->Destroy();
		CameraRig = nullptr;
	}
	if (HelperActor != nullptr)
	{
		HelperActor->Destroy();
		HelperActor = nullptr;
	}
	TargetRing = nullptr;
	GuideArrow = nullptr;
	WorldUi.Reset();
	Super::Deinitialize();
}

UAbyssGameInstance* UAbyssWorldBuilder::GetAbyssGameInstance() const
{
	const UWorld* OwningWorld = GetWorld();
	return OwningWorld ? Cast<UAbyssGameInstance>(OwningWorld->GetGameInstance()) : nullptr;
}

const abyss::DataStore* UAbyssWorldBuilder::GetData() const
{
	const UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
	return GameInstance != nullptr && GameInstance->IsDataReady() ? GameInstance->GetData() : nullptr;
}

UAbyssAssetLibrary* UAbyssWorldBuilder::GetAssets() const
{
	return UAbyssAssetLibrary::Get(this);
}

void UAbyssWorldBuilder::BindEvents()
{
	if (bEventsBound)
	{
		return;
	}
	UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
	if (GameInstance == nullptr)
	{
		return;
	}
	FAbyssEventRouter& Router = GameInstance->GetEventRouter();
	Router.On<abyss::EvCameraShake>().AddUObject(this, &UAbyssWorldBuilder::HandleCameraShake);
	Router.On<abyss::EvCameraFlash>().AddUObject(this, &UAbyssWorldBuilder::HandleCameraFlash);
	Router.On<abyss::EvZone>().AddUObject(this, &UAbyssWorldBuilder::HandleZone);
	Router.On<abyss::EvHeroDied>().AddUObject(this, &UAbyssWorldBuilder::HandleHeroDied);
	Router.On<abyss::EvHeroRespawned>().AddUObject(this, &UAbyssWorldBuilder::HandleHeroRespawned);
	Router.On<abyss::EvLevelUp>().AddUObject(this, &UAbyssWorldBuilder::HandleLevelUp);
	Router.On<abyss::EvLootDropped>().AddUObject(this, &UAbyssWorldBuilder::HandleLootDropped);
	Router.On<abyss::EvPotionDropped>().AddUObject(this, &UAbyssWorldBuilder::HandlePotionDropped);
	Router.On<abyss::EvFloatingText>().AddUObject(this, &UAbyssWorldBuilder::HandleFloatingText);
	Router.On<abyss::EvHit>().AddUObject(this, &UAbyssWorldBuilder::HandleHit);
	Router.On<abyss::EvDodgeStarted>().AddUObject(this, &UAbyssWorldBuilder::HandleDodge);
	Router.On<abyss::EvStoryStep>().AddUObject(this, &UAbyssWorldBuilder::HandleStoryStep);
	Router.On<abyss::EvStoryBeat>().AddUObject(this, &UAbyssWorldBuilder::HandleStoryBeat);
	Router.On<abyss::EvStoryDecorFocus>().AddUObject(this, &UAbyssWorldBuilder::HandleStoryDecorFocus);
	Router.On<abyss::EvEquipmentChanged>().AddUObject(this, &UAbyssWorldBuilder::HandleEquipment);
	Router.On<abyss::EvPet>().AddUObject(this, &UAbyssWorldBuilder::HandlePet);
	Router.On<abyss::EvMonsterRenamed>().AddUObject(this, &UAbyssWorldBuilder::HandleRenamed);
	Router.OnSessionEnded.AddUObject(this, &UAbyssWorldBuilder::HandleSessionEnded);
	SettingsHandle = GameInstance->OnSettingsChanged.AddUObject(this, &UAbyssWorldBuilder::ApplySettings);
	if (UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this))
	{
		VfxShakeHandle = Vfx->OnShake.AddUObject(this, &UAbyssWorldBuilder::HandleVfxShake);
	}
	ApplySettings(GameInstance->GetUserSettings());
	bEventsBound = true;
}

void UAbyssWorldBuilder::UnbindEvents()
{
	if (!bEventsBound)
	{
		return;
	}
	if (UAbyssGameInstance* GameInstance = GetAbyssGameInstance())
	{
		GameInstance->GetEventRouter().RemoveAll(this);
		GameInstance->OnSettingsChanged.Remove(SettingsHandle);
	}
	if (UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this))
	{
		Vfx->OnShake.Remove(VfxShakeHandle);
	}
	SettingsHandle.Reset();
	VfxShakeHandle.Reset();
	bEventsBound = false;
}

void UAbyssWorldBuilder::ApplySettings(const FAbyssUserSettings& Settings)
{
	bCameraShakeEnabled = Settings.bCameraShake;
	bDamageNumbers = Settings.bDamageNumbers;
	if (CameraRig != nullptr)
	{
		CameraRig->SetShakeEnabled(bCameraShakeEnabled);
		if (!bCameraShakeEnabled)
		{
			CameraRig->StopShake();
		}
	}
	RefreshQuality();
}

void UAbyssWorldBuilder::RefreshQuality()
{
	const UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
	const abyss::DataStore* Data = GetData();
	if (GameInstance == nullptr)
	{
		return;
	}
	const EAbyssQualityTier Tier = GameInstance->GetQualityTier();
	FAbyssQualityProfile Profile;
	Profile.Tier = Tier == EAbyssQualityTier::Low ? 0 : (Tier == EAbyssQualityTier::High ? 2 : 1);
	Profile.bMobile = AbyssPlatform::IsMobilePlatform();
	if (Data != nullptr)
	{
		const abyss::RenderQualityProfileDef& Def = Data->RenderQualityProfiles().profiles[static_cast<size_t>(Profile.Tier)];
		Profile.MaxDynamicLights = Def.maxDynamicLights;
		Profile.LightingUpdateIntervalMs = static_cast<float>(Def.lightingUpdateIntervalMs);
		Profile.ParticleFrequencyMultiplier = static_cast<float>(Def.particleFrequencyMultiplier);
		Profile.bBloom = Def.bloom;
		Profile.bColorGrading = Def.colorGrading;
		Profile.ResolutionScale = static_cast<float>(Def.resolutionScale);
	}
	else
	{
		static const float Scales[] = { 1.f, 1.5f, 2.f };
		Profile.ResolutionScale = Scales[Profile.Tier];
		Profile.bBloom = Profile.bColorGrading = Profile.Tier > 0;
	}
	const float Override = AbyssPlatform::GetRenderScaleOverride();
	if (Override > 0.f)
	{
		Profile.ResolutionScale = Override;
	}
	Quality = Profile;

	// 3D resolution cap: 720 x scale px of height (ue58-platform.md 6.8); UI stays native.
	int32 ViewportHeight = 0;
	if (GEngine != nullptr && GEngine->GameViewport != nullptr)
	{
		FVector2D Size;
		GEngine->GameViewport->GetViewportSize(Size);
		ViewportHeight = static_cast<int32>(Size.Y);
	}
	if (ViewportHeight > 0)
	{
		const float Percentage = 100.f * FMath::Min(1.f, 720.f * Quality.ResolutionScale / static_cast<float>(ViewportHeight));
		if (IConsoleVariable* ScreenPercentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
		{
			ScreenPercentage->Set(FMath::Clamp(Percentage, 25.f, 100.f), ECVF_SetByCode);
		}
	}
	if (!Quality.bMobile)
	{
		// FXAA on low, TSR otherwise (desktop deferred; mobile keeps MSAA from the config).
		if (IConsoleVariable* AntiAliasing = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod")))
		{
			AntiAliasing->Set(Quality.Tier == 0 ? 1 : 4, ECVF_SetByCode);
		}
	}

	if (UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this))
	{
		Vfx->ApplyQuality(Quality);
	}
	if (ZoneActor != nullptr)
	{
		ZoneActor->ApplyQuality(Quality);
	}
	const bool bBlob = Quality.bMobile && Quality.Tier < 2;
	for (const TPair<uint32, TObjectPtr<AAbyssCharacterActor>>& Pair : Characters)
	{
		if (Pair.Value != nullptr)
		{
			Pair.Value->SetShadowMode(bBlob, !bBlob);
		}
	}
}

void UAbyssWorldBuilder::EnsureCameraRig()
{
	UWorld* OwningWorld = GetWorld();
	if (CameraRig != nullptr || OwningWorld == nullptr || !OwningWorld->HasBegunPlay())
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	CameraRig = OwningWorld->SpawnActor<AAbyssCameraRig>(AAbyssCameraRig::StaticClass(), FTransform::Identity, Params);
	if (CameraRig == nullptr)
	{
		return;
	}
	const abyss::DataStore* Data = GetData();
	const UAbyssAssetLibrary* Assets = GetAssets();
	FAbyssArtShading Shading;
	if (Assets != nullptr)
	{
		Shading = Assets->GetManifest().GetShading();
	}
	CameraRig->Configure(Shading, Data != nullptr ? &Data->World().constants : nullptr);
	CameraRig->SetShakeEnabled(bCameraShakeEnabled);
	if (Data != nullptr)
	{
		ZoneFadeSec = static_cast<float>(Data->World().constants.zoneFadeInMs / 1000.0);
	}
}

void UAbyssWorldBuilder::EnsureHelpers()
{
	UWorld* OwningWorld = GetWorld();
	UAbyssAssetLibrary* Assets = GetAssets();
	if (HelperActor != nullptr || OwningWorld == nullptr || Assets == nullptr || !OwningWorld->HasBegunPlay())
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	HelperActor = OwningWorld->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (HelperActor == nullptr)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(HelperActor, TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	HelperActor->SetRootComponent(Root);
	Root->RegisterComponent();

	auto MakeComponent = [this, Root](const TCHAR* Name, UStaticMesh* Mesh, UMaterialInterface* Material)
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(HelperActor, Name);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
		Component->SetCastShadow(false);
		Component->SetStaticMesh(Mesh);
		if (Material != nullptr)
		{
			Component->SetMaterial(0, Material);
		}
		Component->SetupAttachment(Root);
		Component->SetUsingAbsoluteLocation(true);
		Component->SetUsingAbsoluteRotation(true);
		Component->SetUsingAbsoluteScale(true);
		Component->RegisterComponent();
		Component->SetVisibility(false);
		return Component;
	};
	static const TCHAR* const FxFolders[] = { TEXT("FX"), TEXT("FX/Meshes") };
	UStaticMesh* Quad = Cast<UStaticMesh>(Assets->LoadFromFolders(FxFolders, FName(TEXT("SM_FX_Quad")), UStaticMesh::StaticClass()));
	UMaterialInterface* RingMaterial = Assets->LoadMaterial(TEXT("M_AF_TargetRing"));
	if (Quad != nullptr && RingMaterial != nullptr)
	{
		TargetRing = MakeComponent(TEXT("TargetRing"), Quad, RingMaterial);
		TargetRing->SetCustomPrimitiveDataVector4(AbyssCpd::TintR, FVector4(1.0, 0.07, 0.07, 0.6));   // #FF4444 alpha .6
	}
	if (UStaticMesh* Arrow = Assets->LoadStaticMeshByName(FName(TEXT("SM_FX_GuideArrow"))))
	{
		GuideArrow = MakeComponent(TEXT("GuideArrow"), Arrow, nullptr);
	}
	GhostMaterial = Assets->LoadMaterial(TEXT("M_AF_Ghost"));
}

// =====================================================================================================================
// IAbyssWorldView: zone
// =====================================================================================================================

void UAbyssWorldBuilder::BuildZone(const abyss::Snapshot& Snap)
{
	using namespace AbyssWorldBuilderPrivate;
	UWorld* OwningWorld = GetWorld();
	UAbyssAssetLibrary* Assets = GetAssets();
	const abyss::DataStore* Data = GetData();
	if (OwningWorld == nullptr || Assets == nullptr || Data == nullptr)
	{
		return;
	}
	EnsureCameraRig();
	EnsureHelpers();
	if (ZoneActor != nullptr)
	{
		ZoneActor->ClearZone();
		ZoneActor->Destroy();
		ZoneActor = nullptr;
	}
	Assets->BeginZone();

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ZoneActor = OwningWorld->SpawnActor<AAbyssZoneActor>(AAbyssZoneActor::StaticClass(), FTransform::Identity, Params);
	if (ZoneActor == nullptr)
	{
		UE_LOG(LogAbyss, Error, TEXT("Could not spawn the zone actor"));
		return;
	}
	ZoneActor->BuildZone(Snap, *Data, *Assets, Quality);
	const UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
	Assets->TrimToRecentZones(GameInstance != nullptr && GameInstance->IsTouchMode() ? 1 : 2);

	// Zone loops: campfires, torches, exits, the ambience volume (world-map-nav.md 14.2).
	if (UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this))
	{
		for (const FAbyssZoneEffectSpot& Spot : ZoneActor->GetEffectSpots())
		{
			FAbyssVfxContext Context;
			Context.Origin = Context.Target = Context.Point = Spot.Location;
			Context.bHasColor = Spot.bHasColor;
			Context.Color = Spot.Color;
			if (const int32 Handle = Vfx->Play(Spot.RecipeId, Context))
			{
				ZoneVfxHandles.Add(Handle);
			}
		}
		if (!ZoneActor->GetAmbienceRecipe().IsNone())
		{
			FAbyssVfxContext Context;
			Context.Origin = Context.Target = Context.Point = GroundAt(Snap.hero.pos);
			if (const int32 Handle = Vfx->Play(ZoneActor->GetAmbienceRecipe(), Context))
			{
				ZoneVfxHandles.Add(Handle);
			}
		}
	}

	// Actors spawned before the zone existed sit at Z = 0: they are placed on the terrain by SyncFrame. The camera
	// snaps onto the hero and the zone fades in (zoneFadeInMs).
	if (CameraRig != nullptr)
	{
		const float FocusZ = Assets->GetManifest().GetShading().CameraFocusZCm;
		CameraRig->EndStoryFocus();
		CameraRig->SnapToFocus(GroundAt(Snap.hero.pos) + FVector(0.0, 0.0, FocusZ));
		CameraRig->FadeTo(0.f, FLinearColor::Black, ZoneFadeSec);
	}
	bStoryFocusActive = false;
	bZoneBuilt = true;
	ReconcileMarkers(Snap);
	RefreshQuality();
}

void UAbyssWorldBuilder::ClearZone()
{
	if (UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this))
	{
		Vfx->ClearAll();
	}
	ZoneVfxHandles.Reset();
	DestroyAllEntities(/*bKeepHero*/ true);
	if (ZoneActor != nullptr)
	{
		ZoneActor->ClearZone();
		ZoneActor->Destroy();
		ZoneActor = nullptr;
	}
	Tracks.Reset();
	HitSlots.Reset();
	FocusedStoryDecor = NAME_None;
	bStoryFocusActive = false;
	StoryFocusEntity = abyss::kNoEntity;
	if (TargetRing != nullptr)
	{
		TargetRing->SetVisibility(false);
	}
	if (GuideArrow != nullptr)
	{
		GuideArrow->SetVisibility(false);
	}
	bZoneBuilt = false;
}

double UAbyssWorldBuilder::GetGroundHeight(double WorldX, double WorldY) const
{
	return ZoneActor != nullptr ? ZoneActor->GetGroundHeight(WorldX, WorldY) : 0.0;
}

FVector UAbyssWorldBuilder::GroundAt(const abyss::Vec2& Tile) const
{
	FVector Location = AbyssUnits::TileToWorld(Tile);
	Location.Z = GetGroundHeight(Location.X, Location.Y);
	return Location;
}

bool UAbyssWorldBuilder::RaycastGround(const FVector& Origin, const FVector& Direction, FVector& OutHit) const
{
	if (ZoneActor != nullptr && ZoneActor->RaycastGround(Origin, Direction, OutHit))
	{
		return true;
	}
	if (Direction.Z > -UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}
	const double T = -Origin.Z / Direction.Z;
	if (T < 0.0)
	{
		return false;
	}
	OutHit = Origin + Direction * T;
	return true;
}

bool UAbyssWorldBuilder::PickGround(const APlayerController& Controller, const FVector2D& ScreenPosition, FVector& OutWorld,
	abyss::Vec2& OutTile) const
{
	FVector Origin;
	FVector Direction;
	if (!Controller.DeprojectScreenPositionToWorld(static_cast<float>(ScreenPosition.X), static_cast<float>(ScreenPosition.Y),
			Origin, Direction))
	{
		return false;
	}
	if (!RaycastGround(Origin, Direction, OutWorld))
	{
		return false;
	}
	OutTile = AbyssUnits::WorldToTile(OutWorld);
	return true;
}

// =====================================================================================================================
// IAbyssWorldView: entities
// =====================================================================================================================

const FAbyssArtAsset* UAbyssWorldBuilder::ResolveCharacterArt(abyss::EntityKind Kind, const std::string& DefId,
	const std::string& ArtId) const
{
	using namespace AbyssWorldBuilderPrivate;
	const UAbyssAssetLibrary* Assets = GetAssets();
	if (Assets == nullptr)
	{
		return nullptr;
	}
	const FString Art = ToF(ArtId);
	const FString Def = ToF(DefId);
	TArray<FName, TInlineAllocator<8>> Ids;
	switch (Kind)
	{
	case abyss::EntityKind::Hero:
		Ids = { N(TEXT("player_") + Art), N(Art), N(TEXT("player_") + Def) };
		break;
	case abyss::EntityKind::Monster:
		Ids = { N(Art), N(TEXT("monster_") + Art), N(Def), N(TEXT("monster_") + Def) };
		break;
	case abyss::EntityKind::Npc:
	{
		const abyss::DataStore* Data = GetData();
		const abyss::NpcDef* Npc = Data != nullptr ? Data->FindNpc(DefId) : nullptr;
		if (Npc != nullptr && !Npc->spriteId.empty())
		{
			const FString Sprite = ToF(Npc->spriteId);
			Ids.Add(N(TEXT("npc_") + Sprite));
			Ids.Add(N(Sprite));
		}
		Ids.Add(N(TEXT("npc_") + Def));
		Ids.Add(N(Def));
		Ids.Add(N(Art));
		break;
	}
	case abyss::EntityKind::Pet:
		Ids = { N(Art), N(TEXT("beast_") + Def), N(TEXT("pet_") + Def), N(Def) };
		break;
	default:
		Ids = { N(Art), N(TEXT("npc_") + Art), N(Def), N(TEXT("decor_") + Art) };
		break;
	}
	Ids.RemoveAll([](FName Name) { return Name.IsNone(); });
	return Assets->GetManifest().ResolveFirst(Ids, AbyssWorldUtil::TileHash(static_cast<int32>(DefId.size()), 7, 41));
}

void UAbyssWorldBuilder::SpawnEntity(const abyss::EvEntitySpawned& Event, const abyss::Snapshot& Snap)
{
	switch (Event.kind)
	{
	case abyss::EntityKind::Hero:
	case abyss::EntityKind::Monster:
	case abyss::EntityKind::Npc:
	case abyss::EntityKind::Pet:
	case abyss::EntityKind::Escort:
		SpawnCharacter(Event, Snap);
		return;
	case abyss::EntityKind::DefendTarget:
	{
		const FAbyssArtAsset* Art = ResolveCharacterArt(Event.kind, Event.defId, Event.artId);
		if (Art != nullptr && Art->IsSkeletal())
		{
			SpawnCharacter(Event, Snap);
		}
		else
		{
			SpawnProp(Event.id, Event.kind, Event.defId, Event.artId, Event.pos, &Snap);
		}
		return;
	}
	case abyss::EntityKind::Prop:
	case abyss::EntityKind::GroundItem:
	case abyss::EntityKind::PotionDrop:
		SpawnProp(Event.id, Event.kind, Event.defId, Event.artId, Event.pos, &Snap);
		return;
	case abyss::EntityKind::Projectile:
	case abyss::EntityKind::GroundEffect:
	case abyss::EntityKind::None:
		return;   // the VFX system presents them
	}
}

AAbyssCharacterActor* UAbyssWorldBuilder::SpawnCharacter(const abyss::EvEntitySpawned& Event, const abyss::Snapshot& Snap)
{
	using namespace AbyssWorldBuilderPrivate;
	UWorld* OwningWorld = GetWorld();
	UAbyssAssetLibrary* Assets = GetAssets();
	const abyss::DataStore* Data = GetData();
	UAbyssActorRegistry* Registry = UAbyssActorRegistry::Get(this);
	if (OwningWorld == nullptr || Assets == nullptr || Data == nullptr || Registry == nullptr)
	{
		return nullptr;
	}
	const FVector Location = GroundAt(Event.pos);
	const FVector2D Facing = ToFlat(Event.facing).GetSafeNormal();

	// The hero is re-announced on every zone entry without a despawn: reuse its actor.
	if (TObjectPtr<AAbyssCharacterActor>* Existing = Characters.Find(Event.id))
	{
		if (AAbyssCharacterActor* Actor = Existing->Get())
		{
			Actor->ResetPresentation();
			Actor->SnapTo(Location, Facing);
			Registry->Register(Event.id, Actor, Event.kind);
			Tracks.Remove(Event.id);
			if (Event.kind == abyss::EntityKind::Hero)
			{
				ApplyHeroEquipment(Snap);
			}
			return Actor;
		}
	}

	FAbyssCharacterSetup Setup;
	Setup.Id = Event.id;
	Setup.Kind = Event.kind;
	Setup.DefId = ToF(Event.defId);
	Setup.ArtId = ToF(Event.artId);
	Setup.Art = ResolveCharacterArt(Event.kind, Event.defId, Event.artId);
	Setup.VisualScale = static_cast<float>(Event.visualScale > 0.0 ? Event.visualScale : 1.0);
	Setup.AnimTiming = &Data->Combat().anim;
	const abyss::HitFeedbackTable& Feedback = Data->Combat().hitFeedback;
	Setup.PainTintColor = AbyssWorldUtil::ColorFromRgb(Feedback.painTintColor != 0 ? Feedback.painTintColor : 0xFF6B6B);
	Setup.PainTintMs = static_cast<float>(Feedback.painTintMs);

	abyss::AnimRig Rig = abyss::AnimRig::Humanoid;
	switch (Event.kind)
	{
	case abyss::EntityKind::Hero:
		Rig = abyss::HeroRig(Snap.hero.cls);
		break;
	case abyss::EntityKind::Monster:
		if (const abyss::MonsterDef* Def = Data->FindMonster(Event.defId))
		{
			Rig = Def->animCategory;
		}
		Setup.bOptimizeAnimation = true;
		break;
	case abyss::EntityKind::Pet:
		if (const abyss::PetDef* Def = Data->FindPet(Event.defId))
		{
			Rig = Def->animCategory;
			Setup.bFlying = Def->flying;
		}
		break;
	case abyss::EntityKind::Npc:
		Setup.bOptimizeAnimation = true;
		break;
	default:
		break;
	}
	const abyss::AnimConfigDef& Preset = Data->Combat().anim.Preset(Rig);
	Setup.HurtKnockbackPx = static_cast<float>(Preset.hurtKnockback > 0.0 ? Preset.hurtKnockback : 8.0);
	Setup.HurtDurationMs = static_cast<float>(Preset.hurtDuration > 0.0 ? Preset.hurtDuration : 200.0);
	Setup.DeathDurationMs = static_cast<float>(Preset.deathDuration > 0.0 ? Preset.deathDuration : 500.0);
	Setup.DodgeDurationMs = static_cast<float>(Preset.dodgeDuration > 0.0 ? Preset.dodgeDuration : 260.0);
	if (Rig == abyss::AnimRig::Flying)
	{
		Setup.bFlying = true;
	}
	if (Setup.bFlying)
	{
		Setup.HoverCm = FlyingHoverCm;
	}
	if (Setup.Art != nullptr && Setup.Art->bHasSpiritColor)
	{
		Setup.SpiritColor = Setup.Art->SpiritColor;
	}
	const bool bBlob = Quality.bMobile && Quality.Tier < 2;
	Setup.bCastShadow = !bBlob;

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FRotator Rotation(0.0, Facing.IsNearlyZero() ? 0.0 : FMath::RadiansToDegrees(FMath::Atan2(Facing.Y, Facing.X)), 0.0);
	AAbyssCharacterActor* Actor = OwningWorld->SpawnActor<AAbyssCharacterActor>(AAbyssCharacterActor::StaticClass(), Location,
		Rotation, Params);
	if (Actor == nullptr)
	{
		return nullptr;
	}
	Actor->InitCharacter(Setup, *Assets);
	Actor->SetShadowMode(bBlob, !bBlob);
	Actor->SnapTo(Location, Facing);
	Actor->OnCharacterNotify.AddUObject(this, &UAbyssWorldBuilder::HandleCharacterNotify);
	Characters.Add(Event.id, Actor);
	Registry->Register(Event.id, Actor, Event.kind);

	switch (Event.kind)
	{
	case abyss::EntityKind::Hero:
		ApplyHeroEquipment(Snap);
		break;
	case abyss::EntityKind::Monster:
		for (const abyss::MonsterView& Monster : Snap.monsters)
		{
			if (Monster.id == Event.id)
			{
				UpdateElite(*Actor, Monster);
				break;
			}
		}
		AddWidget(Event.id, EAbyssWorldWidgetKind::Monster, Event.kind, Setup.DefId, Actor);
		break;
	case abyss::EntityKind::Npc:
		AddWidget(Event.id, EAbyssWorldWidgetKind::Npc, Event.kind, Setup.DefId, Actor);
		break;
	case abyss::EntityKind::Pet:
		AddWidget(Event.id, EAbyssWorldWidgetKind::Pet, Event.kind, Setup.DefId, Actor);
		break;
	case abyss::EntityKind::Escort:
		AddWidget(Event.id, EAbyssWorldWidgetKind::Escort, Event.kind, Setup.DefId, Actor);
		break;
	case abyss::EntityKind::DefendTarget:
		AddWidget(Event.id, EAbyssWorldWidgetKind::DefendTarget, Event.kind, Setup.DefId, Actor);
		break;
	default:
		break;
	}
	return Actor;
}

void UAbyssWorldBuilder::UpdateElite(AAbyssCharacterActor& Actor, const abyss::MonsterView& Monster)
{
	UAbyssAssetLibrary* Assets = GetAssets();
	const abyss::DataStore* Data = GetData();
	if (Assets == nullptr || Data == nullptr)
	{
		return;
	}
	const bool bMarked = Monster.elite || Monster.miniBoss;
	TArray<FLinearColor> Colors;
	for (const abyss::EliteAffixType Affix : Monster.affixes)
	{
		Colors.Add(AbyssWorldUtil::ColorFromRgb(Data->Combat().eliteAffixes.Def(Affix).vfxColor));
	}
	UStaticMesh* Crown = bMarked ? Assets->LoadStaticMeshByName(FName(TEXT("SM_FX_EliteCrown"))) : nullptr;
	static const TCHAR* const FxFolders[] = { TEXT("FX"), TEXT("FX/Meshes") };
	UStaticMesh* Quad = Colors.Num() > 0
		? Cast<UStaticMesh>(Assets->LoadFromFolders(FxFolders, FName(TEXT("SM_FX_Quad")), UStaticMesh::StaticClass()))
		: nullptr;
	UMaterialInterface* AuraMaterial = Colors.Num() > 0 ? Assets->LoadMaterial(TEXT("M_AF_AffixAura")) : nullptr;
	Actor.SetElite(bMarked, Colors, Crown, Quad, AuraMaterial);
}

AAbyssPropActor* UAbyssWorldBuilder::SpawnProp(abyss::EntityId Id, abyss::EntityKind Kind, const std::string& DefId,
	const std::string& ArtId, const abyss::Vec2& Pos, const abyss::Snapshot* Snap, abyss::ItemQuality DropQuality)
{
	using namespace AbyssWorldBuilderPrivate;
	UWorld* OwningWorld = GetWorld();
	UAbyssAssetLibrary* Assets = GetAssets();
	const abyss::DataStore* Data = GetData();
	UAbyssActorRegistry* Registry = UAbyssActorRegistry::Get(this);
	if (OwningWorld == nullptr || Assets == nullptr || Registry == nullptr || Id == abyss::kNoEntity)
	{
		return nullptr;
	}
	if (TObjectPtr<AAbyssPropActor>* Existing = Props.Find(Id))
	{
		if (Existing->Get() != nullptr)
		{
			return Existing->Get();
		}
	}

	const FString Art = ToF(ArtId);
	const FString Def = ToF(DefId);
	FAbyssPropSetup Setup;
	Setup.Id = Id;
	Setup.Kind = Kind;
	TArray<FName, TInlineAllocator<8>> Ids;
	FName LoopRecipe;
	FLinearColor LoopColor = FLinearColor::White;
	bool bLoopColor = false;
	EAbyssWorldWidgetKind WidgetKind = EAbyssWorldWidgetKind::Prop;
	bool bWidget = false;
	FString WidgetDef = Def;

	if (Kind == abyss::EntityKind::GroundItem)
	{
		abyss::ItemQuality ItemQuality = DropQuality;
		if (Snap != nullptr)
		{
			for (const abyss::GroundItemView& Item : Snap->groundItems)
			{
				if (Item.id == Id)
				{
					ItemQuality = Item.quality;
					WidgetDef = ToF(Item.baseId);
				}
			}
		}
		Setup.Style = EAbyssPropStyle::Loot;
		Ids = { FName(TEXT("loot_bag")), FName(TEXT("SM_Loot_Bag")) };
		if (ItemQuality != abyss::ItemQuality::Normal)
		{
			Setup.Tint = QualityTint(ItemQuality);
			Setup.TintAmount = 0.85f;
		}
		if (ItemQuality >= abyss::ItemQuality::Rare)
		{
			Setup.PoolRadiusCm = 31.f;
			Setup.PoolColor = QualityTint(ItemQuality);
			Setup.PoolAlphaMin = 0.1f;
			Setup.PoolAlphaMax = 0.2f;
			Setup.PoolPeriodSec = ItemQuality >= abyss::ItemQuality::Legendary ? 1.4f : 1.8f;
		}
		WidgetKind = EAbyssWorldWidgetKind::GroundItem;
		bWidget = true;
	}
	else if (Kind == abyss::EntityKind::PotionDrop)
	{
		const bool bMana = Def == TEXT("mp") || Art == TEXT("mp");
		Setup.Style = EAbyssPropStyle::Potion;
		Ids = bMana ? TArray<FName, TInlineAllocator<8>>{ FName(TEXT("potion_drop_mp")), FName(TEXT("SM_Pickup_PotionMP")) }
					: TArray<FName, TInlineAllocator<8>>{ FName(TEXT("potion_drop_hp")), FName(TEXT("SM_Pickup_PotionHP")) };
		Setup.PoolRadiusCm = 22.f;
		Setup.PoolColor = AbyssWorldUtil::ColorFromRgb(bMana ? 0x4A8AFF : 0xFF4A4A);
		Setup.PoolAlphaMin = 0.3f;
		Setup.PoolAlphaMax = 0.45f;
		Setup.PoolPeriodSec = 1.2f;
	}
	else if (DefId == "soul_echo")
	{
		Setup.Style = EAbyssPropStyle::Ghost;
		Ids = { N(TEXT("player_") + Art), N(Art) };
		Setup.Tint = AbyssWorldUtil::ColorFromRgb(0x9FE6FF);
		Setup.TintAmount = 1.f;
		LoopRecipe = FName(TEXT("prop.soul_echo"));
		bWidget = true;
	}
	else if (DefId == "quest_clue")
	{
		Setup.Style = EAbyssPropStyle::Pickup;
		Ids = { N(Art), FName(TEXT("clue_mark")), FName(TEXT("fx_clue")), FName(TEXT("SM_FX_ClueMagnifier")) };
		LoopRecipe = FName(TEXT("prop.clue"));
	}
	else if (StartsWith(DefId, "quest_node:"))
	{
		Setup.Style = EAbyssPropStyle::Pickup;
		Ids = { N(TEXT("gather_") + Art), N(TEXT("quest_item_") + Art), N(Art), FName(TEXT("gather_node")) };
		Setup.PhaseSec = static_cast<float>(Id % 7u) * 0.173f;
		LoopRecipe = FName(TEXT("prop.gather"));
	}
	else if (StartsWith(DefId, "lore:"))
	{
		Setup.Style = EAbyssPropStyle::Lore;
		Ids = { N(TEXT("lore_") + Art), N(Art), N(TEXT("decor_") + Art) };
		LoopRecipe = FName(TEXT("prop.lore"));
		LoopColor = LorePoolColor(ArtId);
		bLoopColor = true;
		WidgetKind = EAbyssWorldWidgetKind::Lore;
		WidgetDef = ToF(std::string_view(DefId).substr(5));
		bWidget = true;
	}
	else if (StartsWith(DefId, "hidden_reward:"))
	{
		const std::string Type = DefId.substr(14);
		if (Type == "chest")
		{
			Setup.Style = EAbyssPropStyle::Chest;
			Ids = { N(Art), FName(TEXT("decor_treasure_chest")), FName(TEXT("treasure_chest")) };
			LoopRecipe = FName(TEXT("prop.chest"));
		}
		else
		{
			Setup.Style = EAbyssPropStyle::Static;
			Ids = { N(Art), N(TEXT("decor_") + ToF(Type)), N(ToF(Type)) };
		}
		bWidget = true;
	}
	else if (DefId == "treasure_cache")
	{
		Setup.Style = EAbyssPropStyle::Chest;
		Ids = { N(Art), FName(TEXT("decor_treasure_chest")), FName(TEXT("treasure_chest")) };
		LoopRecipe = FName(TEXT("prop.chest"));
		bWidget = true;
	}
	else if (DefId == "environmental_puzzle")
	{
		Setup.Style = EAbyssPropStyle::Static;
		Ids = { N(Art), FName(TEXT("decor_event_puzzle_rune_pillar")) };
		LoopRecipe = FName(TEXT("prop.event"));
		bWidget = true;
	}
	else if (Kind == abyss::EntityKind::DefendTarget)
	{
		Setup.Style = EAbyssPropStyle::Static;
		Ids = { N(Art), N(TEXT("decor_") + Art), N(Def) };
		WidgetKind = EAbyssWorldWidgetKind::DefendTarget;
		bWidget = true;
	}
	else
	{
		// Random-event NPC-likes (wandering merchant, rescue) and any other prop: their art id, NPC looks first.
		Setup.Style = EAbyssPropStyle::Static;
		Ids = { N(Art), N(TEXT("npc_") + Art), N(TEXT("decor_") + Art), N(Def) };
		bWidget = DefId == "wandering_merchant" || DefId == "rescue";
	}
	Ids.RemoveAll([](FName Name) { return Name.IsNone(); });
	Setup.Art = Assets->GetManifest().ResolveFirst(Ids, Id);
	Setup.YawDeg = Setup.Style == EAbyssPropStyle::Ghost ? 0.f : static_cast<float>(AbyssWorldUtil::TileHash(static_cast<int32>(Id), 3, 19) % 360u);
	Setup.bCastShadow = !Quality.bMobile && (Setup.Style == EAbyssPropStyle::Static || Setup.Style == EAbyssPropStyle::Chest);

	const FVector Location = GroundAt(Pos);
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAbyssPropActor* Actor = OwningWorld->SpawnActor<AAbyssPropActor>(AAbyssPropActor::StaticClass(), Location, FRotator::ZeroRotator,
		Params);
	if (Actor == nullptr)
	{
		return nullptr;
	}
	Actor->InitProp(Setup, *Assets);
	Props.Add(Id, Actor);
	Registry->Register(Id, Actor, Kind);
	if (bWidget)
	{
		AddWidget(Id, WidgetKind, Kind, WidgetDef, Actor);
	}
	if (!LoopRecipe.IsNone())
	{
		if (UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this))
		{
			FAbyssVfxContext Context;
			Context.Origin = Context.Target = Context.Point = Location;
			Context.OriginActor = Actor;
			Context.TargetActor = Actor;
			Context.bHasColor = bLoopColor;
			Context.Color = LoopColor;
			if (const int32 Handle = Vfx->Play(LoopRecipe, Context))
			{
				Actor->Tags.Add(FName(*FString::Printf(TEXT("vfx:%d"), Handle)));
			}
		}
	}
	return Actor;
}

void UAbyssWorldBuilder::DespawnEntity(const abyss::EvEntityDespawned& Event)
{
	const bool bImmediate = Event.reason == abyss::DespawnReason::ZoneUnload || Event.reason == abyss::DespawnReason::Removed;
	RemoveEntity(Event.id, bImmediate, Event.reason);
}

void UAbyssWorldBuilder::RemoveEntity(abyss::EntityId Id, bool bImmediate, abyss::DespawnReason Reason)
{
	using namespace AbyssWorldBuilderPrivate;
	if (Id == abyss::kHeroEntityId)
	{
		return;   // the hero actor lives for the whole session
	}
	if (UAbyssActorRegistry* Registry = UAbyssActorRegistry::Get(this))
	{
		Registry->Unregister(Id);
	}
	RemoveWidget(Id);
	Tracks.Remove(Id);
	ReconciledProps.Remove(Id);

	TObjectPtr<AAbyssCharacterActor> Character;
	if (Characters.RemoveAndCopyValue(Id, Character) && Character != nullptr)
	{
		if (bImmediate || Character->IsDeathFinished() || !Character->HasArt())
		{
			Character->Destroy();
		}
		else
		{
			FLeaving& Entry = Leaving.AddDefaulted_GetRef();
			Entry.Actor = Character;
			Entry.Start = Character->GetActorLocation();
		}
		return;
	}

	TObjectPtr<AAbyssPropActor> Prop;
	if (Props.RemoveAndCopyValue(Id, Prop) && Prop != nullptr)
	{
		// The prop's looping VFX (tagged at spawn).
		if (UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this))
		{
			for (const FName& Tag : Prop->Tags)
			{
				const FString Text = Tag.ToString();
				if (Text.StartsWith(TEXT("vfx:")))
				{
					Vfx->Stop(FCString::Atoi(*Text.RightChop(4)), Reason != abyss::DespawnReason::Collected
						&& Reason != abyss::DespawnReason::PickedUp);
				}
			}
		}
		if (bImmediate)
		{
			Prop->Destroy();
			return;
		}
		FLeaving& Entry = Leaving.AddDefaulted_GetRef();
		Entry.Actor = Prop;
		Entry.Start = Prop->GetActorLocation();
		switch (Reason)
		{
		case abyss::DespawnReason::PickedUp:
			Prop->BeginCollect(LootFlySec);
			Entry.FlySec = Prop->GetStyle() == EAbyssPropStyle::Chest ? 0.f : LootFlySec;
			break;
		case abyss::DespawnReason::Collected:
			Prop->BeginCollect(Prop->GetStyle() == EAbyssPropStyle::Pickup ? QuestItemFlySec : 0.5f);
			Entry.FlySec = Prop->GetStyle() == EAbyssPropStyle::Pickup ? QuestItemFlySec : 0.f;
			break;
		case abyss::DespawnReason::Expired:
		case abyss::DespawnReason::Died:
		default:
			Prop->BeginExpire(Prop->GetStyle() == EAbyssPropStyle::Chest ? 1.2f : 0.25f);
			break;
		}
	}
}

void UAbyssWorldBuilder::DestroyAllEntities(bool bKeepHero)
{
	UAbyssActorRegistry* Registry = UAbyssActorRegistry::Get(this);
	for (auto It = Characters.CreateIterator(); It; ++It)
	{
		if (bKeepHero && It.Key() == abyss::kHeroEntityId)
		{
			continue;
		}
		if (Registry != nullptr)
		{
			Registry->Unregister(It.Key());
		}
		RemoveWidget(It.Key());
		if (It.Value() != nullptr)
		{
			It.Value()->Destroy();
		}
		It.RemoveCurrent();
	}
	for (auto It = Props.CreateIterator(); It; ++It)
	{
		if (Registry != nullptr)
		{
			Registry->Unregister(It.Key());
		}
		RemoveWidget(It.Key());
		if (It.Value() != nullptr)
		{
			It.Value()->Destroy();
		}
		It.RemoveCurrent();
	}
	for (const FLeaving& Entry : Leaving)
	{
		if (AActor* Actor = Entry.Actor.Get())
		{
			Actor->Destroy();
		}
	}
	Leaving.Reset();
	ReconciledProps.Reset();
	if (!bKeepHero)
	{
		Widgets.Reset();
		if (IAbyssWorldUi* Ui = WorldUi.Get())
		{
			Ui->ClearWorldWidgets();
		}
	}
}

void UAbyssWorldBuilder::ReconcileMarkers(const abyss::Snapshot& Snap)
{
	// Entity markers the core never announced (e.g. quest-item pickups) get a prop; reconciled props whose marker left
	// the snapshot are removed. Announced props are handled by Spawn / DespawnEntity.
	if (!bZoneBuilt)
	{
		return;
	}
	TSet<uint32> Present;
	for (const abyss::WorldMarkerView& Marker : Snap.markers)
	{
		if (Marker.id == abyss::kNoEntity || Marker.kind == abyss::MarkerKind::Exit || Marker.kind == abyss::MarkerKind::Camp
			|| Marker.kind == abyss::MarkerKind::StoryDecoration)
		{
			continue;
		}
		Present.Add(Marker.id);
		if (Characters.Contains(Marker.id) || Props.Contains(Marker.id))
		{
			continue;
		}
		std::string DefId;
		switch (Marker.kind)
		{
		case abyss::MarkerKind::GatherNode: DefId = "quest_node:" + Marker.key; break;
		case abyss::MarkerKind::Clue: DefId = "quest_clue"; break;
		case abyss::MarkerKind::QuestItemPickup: DefId = "quest_node:" + Marker.key; break;
		case abyss::MarkerKind::LorePickup: DefId = "lore:" + Marker.key; break;
		case abyss::MarkerKind::HiddenReward: DefId = "hidden_reward:" + Marker.key; break;
		case abyss::MarkerKind::SoulEcho: DefId = "soul_echo"; break;
		default: DefId = Marker.key; break;
		}
		const abyss::EntityKind Kind = Marker.kind == abyss::MarkerKind::EscortNpc ? abyss::EntityKind::Escort
			: (Marker.kind == abyss::MarkerKind::DefendTarget ? abyss::EntityKind::DefendTarget : abyss::EntityKind::Prop);
		if (Kind == abyss::EntityKind::Escort)
		{
			continue;   // escorts are announced (EvEntitySpawned); never invent a character
		}
		std::string ArtId = Marker.key;
		if (Marker.kind == abyss::MarkerKind::SoulEcho)
		{
			ArtId = std::string(abyss::EnumName(Snap.hero.cls));
		}
		if (SpawnProp(Marker.id, Kind, DefId, ArtId, Marker.pos, &Snap) != nullptr)
		{
			ReconciledProps.Add(Marker.id);
		}
	}
	TArray<uint32> Gone;
	for (const uint32 Id : ReconciledProps)
	{
		if (!Present.Contains(Id))
		{
			Gone.Add(Id);
		}
	}
	for (const uint32 Id : Gone)
	{
		RemoveEntity(Id, /*bImmediate*/ false, abyss::DespawnReason::Collected);
	}
}

void UAbyssWorldBuilder::ApplyHeroEquipment(const abyss::Snapshot& Snap)
{
	using namespace AbyssWorldBuilderPrivate;
	AAbyssCharacterActor* Hero = FindCharacter(abyss::kHeroEntityId);
	const abyss::DataStore* Data = GetData();
	const UAbyssAssetLibrary* Assets = GetAssets();
	UAbyssAssetLibrary* MutableAssets = GetAssets();
	if (Hero == nullptr || Data == nullptr || Assets == nullptr || Snap.inventory == nullptr)
	{
		return;
	}
	const FAbyssArtAsset* HeroArt = ResolveCharacterArt(abyss::EntityKind::Hero, std::string(abyss::EnumName(Snap.hero.cls)),
		std::string(abyss::EnumName(Snap.hero.cls)));
	const FName HeldBy = HeroArt != nullptr ? HeroArt->Name : NAME_None;
	// I6 / R4: the equipped weapon / offhand base type picks the mesh; nullptr keeps the class default attachment.
	auto MeshFor = [&](abyss::EquipSlot Slot, FName& OutSocket) -> UStaticMesh*
	{
		const abyss::ItemInstance* Item = Snap.inventory->Equipped(Slot);
		const abyss::ItemBaseDef* Base = Item != nullptr ? Data->FindItemBase(Item->baseId) : nullptr;
		if (Base == nullptr || !Base->hasWeaponType)
		{
			return nullptr;
		}
		const FAbyssArtAsset* WeaponArt = Assets->GetManifest().FindWeapon(N(ToF(abyss::EnumName(Base->weaponType))), HeldBy);
		if (WeaponArt == nullptr)
		{
			return nullptr;
		}
		OutSocket = WeaponArt->AttachSocket;
		return MutableAssets->LoadStaticMesh(*WeaponArt);
	};
	FName MainSocket;
	FName OffSocket;
	UStaticMesh* Main = MeshFor(abyss::EquipSlot::Weapon, MainSocket);
	UStaticMesh* Off = MeshFor(abyss::EquipSlot::Offhand, OffSocket);
	Hero->SetWeaponMeshes(Main, MainSocket, Off, OffSocket);
}

AAbyssCharacterActor* UAbyssWorldBuilder::FindCharacter(abyss::EntityId Id) const
{
	const TObjectPtr<AAbyssCharacterActor>* Found = Characters.Find(Id);
	return Found != nullptr ? Found->Get() : nullptr;
}

AAbyssPropActor* UAbyssWorldBuilder::FindProp(abyss::EntityId Id) const
{
	const TObjectPtr<AAbyssPropActor>* Found = Props.Find(Id);
	return Found != nullptr ? Found->Get() : nullptr;
}

void UAbyssWorldBuilder::HandleCharacterNotify(AAbyssCharacterActor* Actor, FName Notify)
{
	if (UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this))
	{
		Vfx->PlayNotify(Actor, Notify);
	}
}

// =====================================================================================================================
// World UI
// =====================================================================================================================

void UAbyssWorldBuilder::SetWorldUi(IAbyssWorldUi* Ui)
{
	WorldUi = TWeakInterfacePtr<IAbyssWorldUi>(Ui);
	if (Ui != nullptr)
	{
		Ui->ClearWorldWidgets();
		for (const TPair<uint32, FWidgetEntry>& Pair : Widgets)
		{
			Ui->AddWorldWidget(Pair.Value.Desc);
		}
	}
}

IAbyssWorldUi* UAbyssWorldBuilder::GetWorldUi() const
{
	return WorldUi.Get();
}

void UAbyssWorldBuilder::AddWidget(abyss::EntityId Id, EAbyssWorldWidgetKind Kind, abyss::EntityKind EntityKind, const FString& DefId,
	AActor* Actor)
{
	FWidgetEntry& Entry = Widgets.FindOrAdd(Id);
	Entry.Desc.Id = Id;
	Entry.Desc.Kind = Kind;
	Entry.Desc.EntityKind = EntityKind;
	Entry.Desc.DefId = DefId;
	Entry.Actor = Actor;
	if (IAbyssWorldUi* Ui = GetWorldUi())
	{
		Ui->AddWorldWidget(Entry.Desc);
	}
}

void UAbyssWorldBuilder::RemoveWidget(abyss::EntityId Id)
{
	if (Widgets.Remove(Id) > 0)
	{
		if (IAbyssWorldUi* Ui = GetWorldUi())
		{
			Ui->RemoveWorldWidget(Id);
		}
	}
}

void UAbyssWorldBuilder::UpdateWorldWidgets()
{
	// Automatic discovery: a UI root object that also implements IAbyssWorldUi.
	if (!WorldUi.IsValid())
	{
		if (const UAbyssGameInstance* GameInstance = GetAbyssGameInstance())
		{
			if (IAbyssUiRoot* Root = GameInstance->GetUiRoot())
			{
				if (IAbyssWorldUi* Ui = Cast<IAbyssWorldUi>(Root->_getUObject()))
				{
					SetWorldUi(Ui);
				}
			}
		}
	}
	IAbyssWorldUi* Ui = GetWorldUi();
	if (Ui == nullptr)
	{
		return;
	}
	TArray<FAbyssWorldWidgetFrame, TInlineAllocator<64>> Frames;
	for (const TPair<uint32, FWidgetEntry>& Pair : Widgets)
	{
		const AActor* Actor = Pair.Value.Actor.Get();
		if (Actor == nullptr)
		{
			continue;
		}
		FAbyssWorldWidgetFrame& Frame = Frames.AddDefaulted_GetRef();
		Frame.Id = Pair.Key;
		Frame.FeetLocation = Actor->GetActorLocation();
		if (const AAbyssCharacterActor* Character = Cast<AAbyssCharacterActor>(Actor))
		{
			Frame.OverheadLocation = Character->GetAnchorLocation(EAbyssAnchor::Overhead);
			Frame.Opacity = Character->GetPresentationOpacity();
		}
		else if (const AAbyssPropActor* Prop = Cast<AAbyssPropActor>(Actor))
		{
			Frame.OverheadLocation = Prop->GetAnchorLocation(EAbyssAnchor::Overhead);
			Frame.Opacity = Prop->IsFading() ? 0.f : 1.f;
		}
		else
		{
			Frame.OverheadLocation = Frame.FeetLocation + FVector(0.0, 0.0, 200.0);
		}
		Frame.bVisible = !Actor->IsHidden() && Frame.Opacity > 0.01f && !LastFrame.bCinematic;
	}
	Ui->UpdateWorldWidgets(Frames);
}

// =====================================================================================================================
// Per frame
// =====================================================================================================================

FVector2D UAbyssWorldBuilder::TrackedTile(abyss::EntityId Id, const abyss::Vec2& Current, double Alpha, float& OutSpeedTilesPerSec)
{
	FTrack& Track = Tracks.FindOrAdd(Id);
	const FVector2D Now(Current.x, Current.y);
	if (!Track.bValid)
	{
		Track.Prev = Track.Last = Now;
		Track.bValid = true;
	}
	else if (!Track.Last.Equals(Now, 1e-6))
	{
		Track.Prev = Track.Last;
		Track.Last = Now;
	}
	else if (LastFrame.StepsThisFrame > 0)
	{
		Track.Prev = Track.Last;   // it did not move this step
	}
	OutSpeedTilesPerSec = static_cast<float>(FVector2D::Distance(Track.Prev, Track.Last) * abyss::kSimTickHz);
	return FMath::Lerp(Track.Prev, Track.Last, Alpha);
}

void UAbyssWorldBuilder::PresentCharacters(const abyss::Snapshot& Snap, const FAbyssFrameInfo& Frame, float VisualDelta,
	float RealDelta)
{
	using namespace AbyssWorldBuilderPrivate;
	const UAbyssSimDriver* Driver = UAbyssSimDriver::Get(this);
	const FAbyssSnapshotIndex* Index = Driver != nullptr ? &Driver->GetSnapshotIndex() : nullptr;
	const double Alpha = Frame.Alpha;
	const abyss::Vec2 HeroTile = AbyssUnits::LerpTile(Snap.hero.prevPos, Snap.hero.pos, Alpha);

	for (const TPair<uint32, TObjectPtr<AAbyssCharacterActor>>& Pair : Characters)
	{
		AAbyssCharacterActor* Actor = Pair.Value.Get();
		if (Actor == nullptr)
		{
			continue;
		}
		const abyss::EntityId Id = Pair.Key;
		FAbyssCharacterFrame CharacterFrame;
		abyss::Vec2 Tile;
		switch (Actor->GetEntityKind())
		{
		case abyss::EntityKind::Hero:
		{
			Tile = HeroTile;
			CharacterFrame.Facing = ToFlat(Snap.hero.facing).GetSafeNormal();
			CharacterFrame.SpeedCmS = static_cast<float>(Snap.hero.speedTilesPerSec * AbyssUnits::TileUU);
			CharacterFrame.bMoving = Snap.hero.moving;
			CharacterFrame.bAlive = Snap.hero.life == abyss::HeroLife::Alive;
			CharacterFrame.StatusMask = Snap.hero.statusMask;
			break;
		}
		case abyss::EntityKind::Monster:
		{
			const abyss::MonsterView* Monster = Index != nullptr ? Index->FindMonster(Id) : nullptr;
			if (Monster == nullptr)
			{
				continue;
			}
			Tile = AbyssUnits::LerpTile(Monster->prevPos, Monster->pos, Alpha);
			const bool bAttacking = Monster->state == abyss::MonsterState::Attack;
			CharacterFrame.Facing = bAttacking ? DirectionTo(Tile, HeroTile) : ToFlat(Monster->heading).GetSafeNormal();
			CharacterFrame.SpeedCmS = static_cast<float>(Monster->speedTilesPerSec * AbyssUnits::TileUU);
			CharacterFrame.bMoving = Monster->speedTilesPerSec > 0.05
				&& (Monster->state == abyss::MonsterState::Chase || Monster->state == abyss::MonsterState::Patrol
					|| Monster->state == abyss::MonsterState::Returning);
			CharacterFrame.bAlive = Monster->alive;
			CharacterFrame.bWindingUp = Monster->windingUp;
			CharacterFrame.StatusMask = Monster->statusMask;
			CharacterFrame.bElite = Monster->elite;
			break;
		}
		case abyss::EntityKind::Npc:
		{
			const abyss::NpcView* Npc = Index != nullptr ? Index->FindNpc(Id) : nullptr;
			if (Npc == nullptr)
			{
				continue;
			}
			Tile = Npc->pos;
			CharacterFrame.Facing = Npc->heroNear ? DirectionTo(Npc->pos, HeroTile) : ToFlat(Npc->facing).GetSafeNormal();
			CharacterFrame.bTalking = Npc->talking;
			break;
		}
		case abyss::EntityKind::Pet:
		{
			if (!Snap.pet.present || Snap.pet.id != Id)
			{
				continue;
			}
			Tile = AbyssUnits::LerpTile(Snap.pet.prevPos, Snap.pet.pos, Alpha);
			CharacterFrame.Facing = ToFlat(Snap.pet.facing).GetSafeNormal();
			const double StepTiles = FVector2D::Distance(ToFlat(Snap.pet.prevPos), ToFlat(Snap.pet.pos));
			CharacterFrame.SpeedCmS = static_cast<float>(StepTiles * abyss::kSimTickHz * AbyssUnits::TileUU);
			CharacterFrame.bMoving = StepTiles > 1e-4;
			CharacterFrame.bExhausted = Snap.pet.exhausted;
			break;
		}
		default:
		{
			// Escort / defend targets: markers without prevPos -> tracked interpolation.
			const abyss::WorldMarkerView* Marker = Index != nullptr ? Index->FindMarker(Id) : nullptr;
			if (Marker == nullptr)
			{
				continue;
			}
			float SpeedTiles = 0.f;
			const FVector2D Tracked = TrackedTile(Id, Marker->pos, Alpha, SpeedTiles);
			Tile = abyss::Vec2(Tracked.X, Tracked.Y);
			const FTrack* Track = Tracks.Find(Id);
			if (Track != nullptr && !Track->Prev.Equals(Track->Last, 1e-6))
			{
				CharacterFrame.Facing = (Track->Last - Track->Prev).GetSafeNormal();
			}
			CharacterFrame.SpeedCmS = SpeedTiles * static_cast<float>(AbyssUnits::TileUU);
			CharacterFrame.bMoving = SpeedTiles > 0.05f;
			CharacterFrame.bAlive = Marker->maxHp <= 0.0 || Marker->hp > 0.0;
			break;
		}
		}
		CharacterFrame.Location = GroundAt(Tile);
		Actor->PresentFrame(CharacterFrame, Frame, VisualDelta, RealDelta);
	}
}

void UAbyssWorldBuilder::PresentProps(const abyss::Snapshot& Snap, const FAbyssFrameInfo& Frame, float VisualDelta)
{
	const UAbyssSimDriver* Driver = UAbyssSimDriver::Get(this);
	const FAbyssSnapshotIndex* Index = Driver != nullptr ? &Driver->GetSnapshotIndex() : nullptr;
	for (const TPair<uint32, TObjectPtr<AAbyssPropActor>>& Pair : Props)
	{
		AAbyssPropActor* Actor = Pair.Value.Get();
		if (Actor == nullptr)
		{
			continue;
		}
		FVector Location = Actor->GetActorLocation();
		if (Index != nullptr)
		{
			if (const abyss::GroundItemView* Item = Index->FindGroundItem(Pair.Key))
			{
				Location = GroundAt(Item->pos + Item->visualOffset);
			}
			else if (const abyss::PotionDropView* Potion = Index->FindPotion(Pair.Key))
			{
				Location = GroundAt(Potion->pos);
			}
			else if (const abyss::WorldMarkerView* Marker = Index->FindMarker(Pair.Key))
			{
				Location = GroundAt(Marker->pos);
			}
			else
			{
				Location.Z = GetGroundHeight(Location.X, Location.Y);
			}
		}
		Actor->PresentFrame(Location, VisualDelta, TimeSec);
	}
}

void UAbyssWorldBuilder::UpdateLeaving(float VisualDelta)
{
	using namespace AbyssWorldBuilderPrivate;
	const AAbyssCharacterActor* Hero = FindCharacter(abyss::kHeroEntityId);
	const FVector HeroChest = Hero != nullptr ? Hero->GetAnchorLocation(EAbyssAnchor::Chest) : FVector::ZeroVector;
	for (int32 Index = 0; Index < Leaving.Num();)
	{
		FLeaving& Entry = Leaving[Index];
		AActor* Actor = Entry.Actor.Get();
		bool bDone = Actor == nullptr;
		if (AAbyssCharacterActor* Character = Cast<AAbyssCharacterActor>(Actor))
		{
			FAbyssCharacterFrame Frame;
			Frame.Location = Character->GetActorLocation();
			Frame.bAlive = false;
			Character->PresentFrame(Frame, LastFrame, VisualDelta, VisualDelta);
			Entry.ElapsedSec += VisualDelta;
			bDone = Character->IsDeathFinished() || Entry.ElapsedSec > 5.f;
		}
		else if (AAbyssPropActor* Prop = Cast<AAbyssPropActor>(Actor))
		{
			Entry.ElapsedSec += VisualDelta;
			FVector Location = Entry.Start;
			if (Entry.FlySec > 0.f && Hero != nullptr)
			{
				const float T = FMath::Clamp(Entry.ElapsedSec / Entry.FlySec, 0.f, 1.f);
				Location = FMath::Lerp(Entry.Start, HeroChest - FVector(0.0, 0.0, Prop->GetVisualHeightCm() * 0.5f),
					AbyssWorldUtil::EaseInCubic(T));
				Location.Z += 18.f * AbyssWorldUtil::VfxPxToCm * FMath::Sin(UE_PI * T);   // 18 px arc
			}
			Prop->PresentFrame(Location, VisualDelta, TimeSec);
			bDone = Prop->IsFadeFinished() || Entry.ElapsedSec > 5.f;
		}
		if (bDone)
		{
			if (Actor != nullptr)
			{
				Actor->Destroy();
			}
			Leaving.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
		else
		{
			++Index;
		}
	}
}

void UAbyssWorldBuilder::UpdateCamera(const abyss::Snapshot& Snap, float RealDelta)
{
	EnsureCameraRig();
	if (CameraRig == nullptr)
	{
		return;
	}
	const UAbyssAssetLibrary* Assets = GetAssets();
	const float FocusZ = Assets != nullptr ? Assets->GetManifest().GetShading().CameraFocusZCm : 50.f;
	const AAbyssCharacterActor* Hero = FindCharacter(abyss::kHeroEntityId);
	const bool bHasHero = Hero != nullptr;
	const FVector HeroFocus = (bHasHero ? Hero->GetActorLocation() : GroundAt(Snap.hero.pos)) + FVector(0.0, 0.0, FocusZ);
	if (bStoryFocusActive && StoryFocusEntity != abyss::kNoEntity)
	{
		if (StoryFocusEntity == abyss::kHeroEntityId)
		{
			CameraRig->UpdateStoryFocusTarget(HeroFocus);
		}
		else if (const UAbyssActorRegistry* Registry = UAbyssActorRegistry::Get(this))
		{
			if (const AActor* Target = Registry->Find(StoryFocusEntity))
			{
				CameraRig->UpdateStoryFocusTarget(Target->GetActorLocation() + FVector(0.0, 0.0, FocusZ));
			}
		}
	}
	CameraRig->UpdateRig(RealDelta, HeroFocus, true);
}

void UAbyssWorldBuilder::UpdateIndicators(const abyss::Snapshot& Snap, float RealDelta)
{
	using namespace AbyssWorldBuilderPrivate;
	// Target ring under the attack lock / nearest aggro (combat-feel.md 9.5, art-inventory-ch1.md 6.8).
	if (TargetRing != nullptr)
	{
		const AAbyssCharacterActor* Target = Snap.hero.indicator != abyss::kNoEntity ? FindCharacter(Snap.hero.indicator) : nullptr;
		const bool bShow = Target != nullptr && !Target->IsHidden() && !LastFrame.bCinematic
			&& Target->GetEntityKind() == abyss::EntityKind::Monster;
		TargetRing->SetVisibility(bShow);
		if (bShow)
		{
			const float Diameter = FMath::Max(80.f, Target->GetBlobRadiusCm() * 2.6f);
			TargetRing->SetWorldLocation(Target->GetActorLocation() + FVector(0.0, 0.0, 2.5));
			TargetRing->SetWorldScale3D(FVector(Diameter / 100.f, Diameter / 100.f, 1.f));
		}
	}
	// Quest guide arrow orbiting the hero (QuestGuide target; turn-ins brighter).
	if (GuideArrow != nullptr)
	{
		const AAbyssCharacterActor* Hero = FindCharacter(abyss::kHeroEntityId);
		const bool bShow = Snap.guide.Valid() && Hero != nullptr && !LastFrame.bCinematic && !LastFrame.bFrozen
			&& Snap.hero.life == abyss::HeroLife::Alive;
		GuideArrow->SetVisibility(bShow);
		if (bShow)
		{
			const FVector HeroLocation = Hero->GetActorLocation();
			const FVector Goal = GroundAt(Snap.guide.pos);
			FVector2D Direction(Goal.X - HeroLocation.X, Goal.Y - HeroLocation.Y);
			const double Distance = Direction.Size();
			GuideArrow->SetVisibility(Distance > 150.0);
			if (Distance > 1.0)
			{
				Direction /= Distance;
				const float Pulse = 3.f * AbyssWorldUtil::VfxPxToCm * (0.5f + 0.5f * FMath::Sin(static_cast<float>(TimeSec) * 5.f));
				const FVector Location = HeroLocation + FVector(Direction.X, Direction.Y, 0.0) * (GuideRadiusCm + Pulse)
					+ FVector(0.0, 0.0, GuideHeightCm);
				const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
				const float YawOffset = GetAssets() != nullptr ? GetAssets()->GetManifest().GetShading().MeshYawOffsetDeg : -90.f;
				GuideArrow->SetWorldLocationAndRotation(Location, FRotator(0.f, Yaw + YawOffset, 0.f));
				const bool bTurnIn = Snap.guide.reason == abyss::GuideReason::TurnIn;
				const FLinearColor Color = bTurnIn ? FLinearColor::White : AbyssWorldUtil::ColorFromRgb(0xFFF2D0);
				GuideArrow->SetCustomPrimitiveDataVector4(AbyssCpd::TintR, FVector4(Color.R, Color.G, Color.B, 1.0));
				GuideArrow->SetCustomPrimitiveDataFloat(AbyssCpd::TintAmount, bTurnIn ? 0.95f : 0.8f);
			}
		}
	}
}

void UAbyssWorldBuilder::UpdateLightingCollection(float RealDelta)
{
	LightingRefreshSec -= RealDelta;
	if (LightingRefreshSec > 0.f)
	{
		return;
	}
	LightingRefreshSec = FMath::Max(0.f, Quality.LightingUpdateIntervalMs / 1000.f);
	UAbyssAssetLibrary* Assets = GetAssets();
	UWorld* OwningWorld = GetWorld();
	UMaterialParameterCollection* Collection = Assets != nullptr ? Assets->GetLightingCollection() : nullptr;
	UMaterialParameterCollectionInstance* Instance = Collection != nullptr && OwningWorld != nullptr
		? OwningWorld->GetParameterCollectionInstance(Collection)
		: nullptr;
	if (Instance == nullptr)
	{
		return;
	}
	auto SetVector = [Collection, Instance](const TCHAR* Name, const FLinearColor& Value)
	{
		const FName ParameterName(Name);
		if (Collection->GetVectorParameterByName(ParameterName) != nullptr)
		{
			Instance->SetVectorParameterValue(ParameterName, Value);
		}
	};
	auto SetScalar = [Collection, Instance](const TCHAR* Name, float Value)
	{
		const FName ParameterName(Name);
		if (Collection->GetScalarParameterByName(ParameterName) != nullptr)
		{
			Instance->SetScalarParameterValue(ParameterName, Value);
		}
	};
	FVector DirToLight = Assets->GetManifest().GetShading().DirToLight;
	FLinearColor SunColor = FLinearColor::White;
	FLinearColor Ambient(0.55f, 0.55f, 0.6f);
	FLinearColor Rim = Assets->GetManifest().GetShading().RimColor;
	if (ZoneActor != nullptr)
	{
		DirToLight = ZoneActor->GetDirToLight();
		SunColor = ZoneActor->GetSunColor();
		Ambient = ZoneActor->GetAmbientFill();
		Rim = ZoneActor->GetRimColor();
	}
	const FLinearColor Dir(static_cast<float>(DirToLight.X), static_cast<float>(DirToLight.Y), static_cast<float>(DirToLight.Z), 0.f);
	SetVector(TEXT("SunDir"), Dir);
	SetVector(TEXT("KeyLightDir"), Dir);
	SetVector(TEXT("SunColor"), SunColor);
	SetVector(TEXT("Ambient"), Ambient);
	SetVector(TEXT("RimColor"), Rim);
	SetScalar(TEXT("CameraTanHalfFovY"), CameraRig != nullptr ? CameraRig->GetTanHalfFovY() : 0.1773f);
	SetScalar(TEXT("TimeSec"), static_cast<float>(FMath::Fmod(TimeSec, 3600.0)));
}

void UAbyssWorldBuilder::SyncFrame(const abyss::Snapshot& Snap, const FAbyssFrameInfo& Frame)
{
	const float RealDelta = static_cast<float>(FMath::Clamp(Frame.RealDeltaMs / 1000.0, 0.0, 0.25));
	const float VisualDelta = RealDelta * FMath::Max(0.f, Frame.VisualTimeDilation);
	TimeSec += RealDelta;
	LastFrame = Frame;
	if (HitSlotsFrame != Frame.FrameNumber)
	{
		HitSlots.Reset();
		HitSlotsFrame = Frame.FrameNumber;
	}

	ReconcileMarkers(Snap);
	PresentCharacters(Snap, Frame, VisualDelta, RealDelta);
	PresentProps(Snap, Frame, VisualDelta);
	UpdateLeaving(VisualDelta);
	UpdateCamera(Snap, RealDelta);

	// Occlusion fade (R1): the hero (chest, head) and living monsters near it.
	if (ZoneActor != nullptr && CameraRig != nullptr && CameraRig->GetCamera() != nullptr)
	{
		TArray<FVector, TInlineAllocator<16>> Targets;
		if (const AAbyssCharacterActor* Hero = FindCharacter(abyss::kHeroEntityId))
		{
			Targets.Add(Hero->GetActorLocation() + FVector(0.0, 0.0, 30.0));
			Targets.Add(Hero->GetAnchorLocation(EAbyssAnchor::Chest));
			Targets.Add(Hero->GetAnchorLocation(EAbyssAnchor::Head));
			const FVector HeroLocation = Hero->GetActorLocation();
			const float Range = AbyssWorldBuilderPrivate::NearMonsterOcclusionTiles * static_cast<float>(AbyssUnits::TileUU);
			for (const TPair<uint32, TObjectPtr<AAbyssCharacterActor>>& Pair : Characters)
			{
				const AAbyssCharacterActor* Actor = Pair.Value.Get();
				if (Actor != nullptr && Actor->GetEntityKind() == abyss::EntityKind::Monster && !Actor->IsHidden()
					&& FVector::DistSquared2D(Actor->GetActorLocation(), HeroLocation) < Range * Range && Targets.Num() < 16)
				{
					Targets.Add(Actor->GetAnchorLocation(EAbyssAnchor::Chest));
				}
			}
		}
		ZoneActor->UpdateOcclusion(Targets, CameraRig->GetCamera()->GetComponentLocation(), VisualDelta);
	}

	UpdateIndicators(Snap, RealDelta);
	UpdateLightingCollection(RealDelta);
	if (UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this))
	{
		Vfx->Tick(VisualDelta, Frame, Snap);
	}
	UpdateWorldWidgets();

	// The screen-percentage cap follows the viewport height (window resize / rotation).
	if ((Frame.FrameNumber % 60u) == 0u)
	{
		RefreshQuality();
	}
}

// =====================================================================================================================
// Events
// =====================================================================================================================

void UAbyssWorldBuilder::HandleCameraShake(const abyss::EvCameraShake& Event)
{
	if (CameraRig != nullptr && bCameraShakeEnabled)
	{
		CameraRig->StartShake(static_cast<float>(Event.durationMs), static_cast<float>(Event.intensity));
	}
}

void UAbyssWorldBuilder::HandleVfxShake(float DurationMs, float Intensity)
{
	if (CameraRig != nullptr && bCameraShakeEnabled)
	{
		CameraRig->StartShake(DurationMs, Intensity);
	}
}

void UAbyssWorldBuilder::HandleCameraFlash(const abyss::EvCameraFlash& Event)
{
	if (CameraRig != nullptr)
	{
		CameraRig->Flash(AbyssWorldUtil::ColorFromRgb(Event.color), static_cast<float>(Event.durationMs), static_cast<float>(Event.alpha));
	}
}

void UAbyssWorldBuilder::HandleZone(const abyss::EvZone& Event)
{
	if (Event.phase != abyss::EvZone::Phase::TransitionBegan)
	{
		return;
	}
	// Fade out (VFXManager.zoneTransition 400 ms) and warm the next zone's character art.
	if (CameraRig != nullptr)
	{
		CameraRig->FadeTo(1.f, FLinearColor::Black, ZoneFadeSec);
	}
	UAbyssAssetLibrary* Assets = GetAssets();
	const abyss::DataStore* Data = GetData();
	const abyss::MapDef* Map = Data != nullptr ? Data->FindMap(Event.mapId) : nullptr;
	if (Assets == nullptr || Map == nullptr)
	{
		return;
	}
	TArray<FSoftObjectPath> Paths;
	auto AddAsset = [&Paths](const FAbyssArtAsset* Asset)
	{
		if (Asset == nullptr)
		{
			return;
		}
		Paths.AddUnique(Asset->GetObjectPath());
		for (const FAbyssArtClip& Clip : Asset->Clips)
		{
			if (!Clip.bStaticPose)
			{
				Paths.AddUnique(Clip.GetObjectPath());
			}
		}
	};
	for (const abyss::MapSpawnDef& Spawn : Map->spawns)
	{
		if (const abyss::MonsterDef* Monster = Data->FindMonster(Spawn.monsterId))
		{
			AddAsset(ResolveCharacterArt(abyss::EntityKind::Monster, Monster->id, Monster->spriteKey));
		}
	}
	for (const abyss::MapCampDef& Camp : Map->camps)
	{
		for (const std::string& NpcId : Camp.npcs)
		{
			AddAsset(ResolveCharacterArt(abyss::EntityKind::Npc, NpcId, NpcId));
		}
	}
	Assets->PreloadAsync(Paths);
}

void UAbyssWorldBuilder::HandleHeroDied(const abyss::EvHeroDied& Event)
{
	// combat-feel.md 11.8: camera fade to rgb(80, 10, 10) over 220 ms (the white flash comes as EvCameraFlash).
	if (CameraRig != nullptr)
	{
		CameraRig->FadeTo(0.55f, FLinearColor::FromSRGBColor(FColor(80, 10, 10)), 0.22f);
	}
}

void UAbyssWorldBuilder::HandleHeroRespawned(const abyss::EvHeroRespawned& Event)
{
	if (AAbyssCharacterActor* Hero = FindCharacter(abyss::kHeroEntityId))
	{
		Hero->OnHeroRespawned();
		Hero->SnapTo(GroundAt(Event.pos), FVector2D::ZeroVector);
	}
	if (CameraRig != nullptr)
	{
		const UAbyssAssetLibrary* Assets = GetAssets();
		const float FocusZ = Assets != nullptr ? Assets->GetManifest().GetShading().CameraFocusZCm : 50.f;
		CameraRig->SnapToFocus(GroundAt(Event.pos) + FVector(0.0, 0.0, FocusZ));
		CameraRig->FadeTo(0.f, FLinearColor::FromSRGBColor(FColor(80, 10, 10)), 0.3f);
	}
}

void UAbyssWorldBuilder::HandleLevelUp(const abyss::EvLevelUp& Event)
{
	// combat-feel.md 11.8: gold flash 200 ms alpha .5, shake 100 / 0.004 (recipe), zoom pulse to 1.5 / 1.8 over 200 ms,
	// back in 120 ms.
	if (CameraRig != nullptr)
	{
		CameraRig->Flash(AbyssWorldUtil::ColorFromRgb(0xFFD700), 200.f, 0.5f);
		CameraRig->PulseZoom(1.8f / 1.5f, 0.2f, 0.12f);
	}
}

void UAbyssWorldBuilder::HandleLootDropped(const abyss::EvLootDropped& Event)
{
	const UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
	const abyss::Snapshot* Snap = GameInstance != nullptr ? GameInstance->GetSnapshot() : nullptr;
	// The snapshot may not list the drop yet: the event's quality tints the bag.
	SpawnProp(Event.drop, abyss::EntityKind::GroundItem, Event.baseId, "loot_bag", Event.pos, Snap, Event.quality);
	// combat-feel.md 11.8: legendary / set drops flash 220 ms alpha .35 in the quality colour and shake 160 / 0.005.
	if ((Event.quality == abyss::ItemQuality::Legendary || Event.quality == abyss::ItemQuality::Set) && CameraRig != nullptr)
	{
		CameraRig->Flash(AbyssWorldBuilderPrivate::QualityTint(Event.quality), 220.f, 0.35f);
		if (bCameraShakeEnabled)
		{
			CameraRig->StartShake(160.f, 0.005f);
		}
	}
}

void UAbyssWorldBuilder::HandlePotionDropped(const abyss::EvPotionDropped& Event)
{
	const UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
	SpawnProp(Event.drop, abyss::EntityKind::PotionDrop, Event.kind == abyss::PotionKind::Mp ? "mp" : "hp",
		Event.kind == abyss::PotionKind::Mp ? "mp" : "hp", Event.pos, GameInstance != nullptr ? GameInstance->GetSnapshot() : nullptr);
}

void UAbyssWorldBuilder::HandleHit(const abyss::EvHit& Event)
{
	HitSlots.Add(Event.target, Event.numberSlot);
}

void UAbyssWorldBuilder::HandleFloatingText(const abyss::EvFloatingText& Event)
{
	IAbyssWorldUi* Ui = GetWorldUi();
	if (Ui == nullptr)
	{
		return;
	}
	const bool bDamageNumber = Event.kind == abyss::FloatingTextKind::MonsterDamage || Event.kind == abyss::FloatingTextKind::HeroDamage
		|| Event.kind == abyss::FloatingTextKind::Miss;
	if (bDamageNumber && !bDamageNumbers)
	{
		return;   // U9 "damage numbers off"
	}
	FAbyssFloatingTextRequest Request;
	Request.Kind = Event.kind;
	Request.Anchor = Event.anchor;
	Request.Value = Event.value;
	Request.bCrit = Event.crit;
	Request.Element = Event.element;
	Request.Text = Event.text;
	if (const abyss::HitNumberSlot* Slot = HitSlots.Find(Event.anchor))
	{
		Request.Slot = *Slot;
	}
	const UAbyssActorRegistry* Registry = UAbyssActorRegistry::Get(this);
	const AActor* Actor = Registry != nullptr && Event.anchor != abyss::kNoEntity ? Registry->Find(Event.anchor) : nullptr;
	if (const AAbyssCharacterActor* Character = Cast<AAbyssCharacterActor>(Actor))
	{
		Request.WorldLocation = Character->GetActorLocation();
		Request.OverheadLocation = Character->GetAnchorLocation(EAbyssAnchor::Overhead);
	}
	else if (const AAbyssPropActor* Prop = Cast<AAbyssPropActor>(Actor))
	{
		Request.WorldLocation = Prop->GetActorLocation();
		Request.OverheadLocation = Prop->GetAnchorLocation(EAbyssAnchor::Overhead);
	}
	else
	{
		Request.WorldLocation = GroundAt(Event.pos);
		Request.OverheadLocation = Request.WorldLocation + FVector(0.0, 0.0, 180.0);
	}
	Ui->ShowFloatingText(Request);
}

void UAbyssWorldBuilder::HandleDodge(const abyss::EvDodgeStarted& Event)
{
	AAbyssCharacterActor* Hero = FindCharacter(abyss::kHeroEntityId);
	if (Hero != nullptr && GhostMaterial != nullptr)
	{
		Hero->SpawnAfterimages(GroundAt(Event.from), GroundAt(Event.to), GhostMaterial);
	}
}

void UAbyssWorldBuilder::HandleStoryStep(const abyss::EvStoryStep& Event)
{
	if (Event.isSlide || CameraRig == nullptr)
	{
		return;
	}
	switch (Event.step.kind)
	{
	case abyss::StoryStepKind::Focus:
	{
		if (!Event.hasFocus)
		{
			return;
		}
		const UAbyssAssetLibrary* Assets = GetAssets();
		const float FocusZ = Assets != nullptr ? Assets->GetManifest().GetShading().CameraFocusZCm : 50.f;
		FVector Target = GroundAt(Event.focusPos) + FVector(0.0, 0.0, FocusZ);
		StoryFocusEntity = Event.focusEntity;
		if (Event.focusEntity != abyss::kNoEntity)
		{
			if (const UAbyssActorRegistry* Registry = UAbyssActorRegistry::Get(this))
			{
				if (const AActor* Actor = Registry->Find(Event.focusEntity))
				{
					Target = Actor->GetActorLocation() + FVector(0.0, 0.0, FocusZ);
				}
			}
		}
		// quests-story-ch1.md 8.5: pan over the step's ms with Sine in-out, hold until the next focus / the beat end.
		CameraRig->BeginStoryFocus(Target, static_cast<float>(Event.timedMs / 1000.0));
		bStoryFocusActive = true;
		break;
	}
	case abyss::StoryStepKind::Shake:
		if (bCameraShakeEnabled)
		{
			CameraRig->StartShake(static_cast<float>(Event.timedMs),
				static_cast<float>(Event.step.hasIntensity ? Event.step.intensity : 0.01));
		}
		break;
	case abyss::StoryStepKind::Flash:
		CameraRig->Flash(AbyssWorldUtil::ColorFromRgb(Event.step.hasColor ? Event.step.color : 0xFFFFFF),
			static_cast<float>(Event.timedMs), 1.f);
		break;
	default:
		break;
	}
}

void UAbyssWorldBuilder::HandleStoryBeat(const abyss::EvStoryBeat& Event)
{
	if (Event.phase == abyss::EvStoryBeat::Phase::Ended && CameraRig != nullptr)
	{
		CameraRig->EndStoryFocus();
		bStoryFocusActive = false;
		StoryFocusEntity = abyss::kNoEntity;
	}
}

void UAbyssWorldBuilder::HandleStoryDecorFocus(const abyss::EvStoryDecorFocus& Event)
{
	FocusedStoryDecor = Event.decorId.empty() ? NAME_None : FName(*AbyssText::ToFString(Event.decorId));
	if (ZoneActor != nullptr)
	{
		ZoneActor->SetStoryFocus(FocusedStoryDecor);
	}
}

void UAbyssWorldBuilder::HandleEquipment(const abyss::EvEquipmentChanged& Event)
{
	if (Event.slot != abyss::EquipSlot::Weapon && Event.slot != abyss::EquipSlot::Offhand)
	{
		return;
	}
	if (const UAbyssGameInstance* GameInstance = GetAbyssGameInstance())
	{
		if (const abyss::Snapshot* Snap = GameInstance->GetSnapshot())
		{
			ApplyHeroEquipment(*Snap);
		}
	}
}

void UAbyssWorldBuilder::HandlePet(const abyss::EvPet& Event)
{
	// Evolution changes the beast's look (beast_<id>_e<stage>): rebuild its actor in place.
	if (Event.kind != abyss::EvPet::Kind::Evolved)
	{
		return;
	}
	const UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
	const abyss::Snapshot* Snap = GameInstance != nullptr ? GameInstance->GetSnapshot() : nullptr;
	if (Snap == nullptr || !Snap->pet.present)
	{
		return;
	}
	RemoveEntity(Snap->pet.id, /*bImmediate*/ true, abyss::DespawnReason::Removed);
	abyss::EvEntitySpawned Spawn;
	Spawn.id = Snap->pet.id;
	Spawn.kind = abyss::EntityKind::Pet;
	Spawn.defId = Snap->pet.petId;
	Spawn.artId = Snap->pet.stage <= 0 ? "beast_" + Snap->pet.petId : "beast_" + Snap->pet.petId + "_e" + std::to_string(Snap->pet.stage);
	Spawn.pos = Snap->pet.pos;
	Spawn.facing = Snap->pet.facing;
	SpawnCharacter(Spawn, *Snap);
}

void UAbyssWorldBuilder::HandleRenamed(const abyss::EvMonsterRenamed& Event)
{
	// Re-send the widget so the UI re-reads the (story) name and colour.
	if (FWidgetEntry* Entry = Widgets.Find(Event.monster))
	{
		if (IAbyssWorldUi* Ui = GetWorldUi())
		{
			Ui->AddWorldWidget(Entry->Desc);
		}
	}
}

void UAbyssWorldBuilder::HandleSessionEnded()
{
	if (UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this))
	{
		Vfx->ClearAll();
	}
	ZoneVfxHandles.Reset();
	DestroyAllEntities(/*bKeepHero*/ false);
	if (ZoneActor != nullptr)
	{
		ZoneActor->ClearZone();
		ZoneActor->Destroy();
		ZoneActor = nullptr;
	}
	if (CameraRig != nullptr)
	{
		CameraRig->ClearFades();
		CameraRig->StopShake();
		CameraRig->EndStoryFocus();
	}
	Tracks.Reset();
	bZoneBuilt = false;
	bStoryFocusActive = false;
}
