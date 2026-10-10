#include "Input/AbyssInputSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "Widgets/SWidget.h"

#include "abyss/sim/Commands.h"

#include "AbyssInputDeviceTracker.h"
#include "Framework/AbyssGameInstance.h"
#include "Framework/AbyssPlayerController.h"
#include "Framework/AbyssUiRoot.h"
#include "Input/AbyssInputConfig.h"
#include "Input/Touch/AbyssTouchStyle.h"
#include "Input/Touch/SAbyssTouchControls.h"
#include "Platform/AbyssPlatform.h"

namespace
{
	// The joystick refreshes its state every Slate tick while held; older state is stale (widget hidden / destroyed).
	constexpr double AbyssInputSubsystem_StickStaleSeconds = 0.25;
}

UAbyssInputSubsystem* UAbyssInputSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* OwningWorld = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = OwningWorld ? OwningWorld->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UAbyssInputSubsystem>() : nullptr;
}

void UAbyssInputSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Config = NewObject<UAbyssInputConfig>(this, TEXT("AbyssInputConfig"), RF_Transient);
	Config->Build();

	if (FSlateApplication::IsInitialized())
	{
		DeviceTracker = MakeShared<FAbyssInputDeviceTracker>(*this);
		FSlateApplication::Get().RegisterInputPreProcessor(DeviceTracker);
	}
	// The GameInstance resolves its touch layout after its subsystems initialise; the platform is the best first guess.
	LastDevice = AbyssPlatform::IsMobilePlatform() ? EAbyssInputDevice::Touch : EAbyssInputDevice::KeyboardMouse;
}

void UAbyssInputSubsystem::Deinitialize()
{
	if (DeviceTracker.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(DeviceTracker);
	}
	DeviceTracker.Reset();
	UiInputHandler.Unbind();
	OnInputDeviceChanged.Clear();
	OnHoveredEntityChanged.Clear();
	Config = nullptr;
	Super::Deinitialize();
}

UAbyssGameInstance* UAbyssInputSubsystem::GetAbyssGameInstance() const
{
	return Cast<UAbyssGameInstance>(GetGameInstance());
}

AAbyssPlayerController* UAbyssInputSubsystem::GetPlayerController() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? Cast<AAbyssPlayerController>(GameInstance->GetFirstLocalPlayerController()) : nullptr;
}

// =====================================================================================================================
// Virtual controls
// =====================================================================================================================

void UAbyssInputSubsystem::PressAction(EAbyssInputAction Action, EAbyssInputDevice Device)
{
	NotifyInputDevice(Device);
	if (AAbyssPlayerController* Controller = GetPlayerController())
	{
		Controller->InjectAbyssPress(Action, Device);
	}
}

void UAbyssInputSubsystem::SetVirtualStick(const FVector2D& ScreenValue, bool bActive)
{
	bVirtualStickActive = bActive;
	VirtualStick = bActive ? ScreenValue : FVector2D::ZeroVector;
	VirtualStickUpdatedSeconds = FPlatformTime::Seconds();
	if (bActive)
	{
		NotifyInputDevice(EAbyssInputDevice::Touch);
	}
}

bool UAbyssInputSubsystem::IsVirtualStickActive() const
{
	return bVirtualStickActive && (FPlatformTime::Seconds() - VirtualStickUpdatedSeconds) < AbyssInputSubsystem_StickStaleSeconds;
}

FVector2D UAbyssInputSubsystem::GetVirtualStick() const
{
	return IsVirtualStickActive() ? VirtualStick : FVector2D::ZeroVector;
}

// =====================================================================================================================
// UI bridge
// =====================================================================================================================

bool UAbyssInputSubsystem::RouteUiRequest(const FAbyssUiInputRequest& Request)
{
	if (UiInputHandler.IsBound() && UiInputHandler.Execute(Request))
	{
		return true;
	}
	UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
	if (GameInstance == nullptr)
	{
		return false;
	}
	switch (Request.Kind)
	{
	case EAbyssUiRequest::StoryAdvance:
		GameInstance->Submit(abyss::CmdStoryAdvance{});
		return true;
	case EAbyssUiRequest::StorySkip:
		GameInstance->Submit(abyss::CmdStorySkip{});
		return true;
	case EAbyssUiRequest::OpenSystemMenu:
		if (IAbyssUiRoot* Root = GameInstance->GetUiRoot())
		{
			return Root->HandleBack();
		}
		return false;
	case EAbyssUiRequest::TogglePanel:
	case EAbyssUiRequest::ToggleCombatLog:
		break;
	}
	UE_LOG(LogAbyssInput, Verbose, TEXT("UI request %s not handled (no UI handler)"), LexToString(Request.Kind));
	return false;
}

FString UAbyssInputSubsystem::GetKeyHint(EAbyssInputAction Action) const
{
	return GetKeyHint(Action, LastDevice);
}

FString UAbyssInputSubsystem::GetKeyHint(EAbyssInputAction Action, EAbyssInputDevice Device) const
{
	return Config ? Config->GetKeyHint(Action, Device) : FString();
}

TSharedRef<SWidget> UAbyssInputSubsystem::CreateTouchControls(const FAbyssTouchVisuals& Visuals)
{
	return SNew(SAbyssTouchControls).InputSubsystem(this).Visuals(Visuals);
}

// =====================================================================================================================
// Device, hover
// =====================================================================================================================

void UAbyssInputSubsystem::NotifyInputDevice(EAbyssInputDevice Device)
{
	if (Device == LastDevice)
	{
		return;
	}
	LastDevice = Device;
	OnInputDeviceChanged.Broadcast(Device);
}

void UAbyssInputSubsystem::SetHoveredEntity(abyss::EntityId Entity)
{
	if (Entity == HoveredEntity)
	{
		return;
	}
	HoveredEntity = Entity;
	OnHoveredEntityChanged.Broadcast(Entity);
}
