// Backbone part of AAbyssPlayerController. The input agent's part (SetupInputComponent and the Abyss*Input hooks) is in
// Private/Input/AbyssPlayerControllerInput.cpp.
#include "Framework/AbyssPlayerController.h"

#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"

#include "Framework/AbyssGameInstance.h"
#include "Framework/AbyssSimDriver.h"
#include "Framework/AbyssUnits.h"
#include "Framework/AbyssWorldView.h"

AAbyssPlayerController::AAbyssPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = false;   // no collision picking (ue58-platform.md 8.4)
	bEnableMouseOverEvents = false;
	bEnableTouchEvents = true;
	bEnableTouchOverEvents = false;
}

UAbyssGameInstance* AAbyssPlayerController::GetAbyssGameInstance() const
{
	return UAbyssGameInstance::Get(this);
}

void AAbyssPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	if (UAbyssGameInstance* GameInstance = GetAbyssGameInstance())
	{
		AppStateHandle = GameInstance->OnAppStateChanged.AddUObject(this, &AAbyssPlayerController::HandleAppStateChanged);
		SettingsHandle = GameInstance->OnSettingsChanged.AddWeakLambda(this, [this](const FAbyssUserSettings&) { HandleSettingsApplied(); });
	}
	ApplyCursorAndInputMode();
	InitAbyssInput();
	if (const UAbyssGameInstance* GameInstance = GetAbyssGameInstance())
	{
		OnAbyssInputContextChanged(GameInstance->GetAppState(), GameInstance->IsTouchMode());
	}
}

void AAbyssPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsLocalController())
	{
		ShutdownAbyssInput();
	}
	if (UAbyssGameInstance* GameInstance = GetAbyssGameInstance())
	{
		GameInstance->OnAppStateChanged.Remove(AppStateHandle);
		GameInstance->OnSettingsChanged.Remove(SettingsHandle);
	}
	AppStateHandle.Reset();
	SettingsHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

void AAbyssPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	TickAbyssInput(DeltaTime);
}

void AAbyssPlayerController::HandleAppStateChanged(EAbyssAppState NewState)
{
	ApplyCursorAndInputMode();
	OnAbyssInputContextChanged(NewState, IsTouchMode());
}

void AAbyssPlayerController::HandleSettingsApplied()
{
	ApplyCursorAndInputMode();
	if (const UAbyssGameInstance* GameInstance = GetAbyssGameInstance())
	{
		OnAbyssInputContextChanged(GameInstance->GetAppState(), GameInstance->IsTouchMode());
	}
}

void AAbyssPlayerController::ApplyCursorAndInputMode()
{
	// ue58-platform.md 8.5: game + UI input (Slate widgets get first refusal), cursor visible on desktop, never locked.
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = !IsTouchMode();
}

void AAbyssPlayerController::SubmitCommand(const abyss::Command& Command) const
{
	if (UAbyssGameInstance* GameInstance = GetAbyssGameInstance())
	{
		GameInstance->Submit(Command);
	}
}

bool AAbyssPlayerController::IsTouchMode() const
{
	const UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
	return GameInstance != nullptr && GameInstance->IsTouchMode();
}

float AAbyssPlayerController::GetUiDpiScale() const
{
	int32 SizeX = 0;
	int32 SizeY = 0;
	GetViewportSize(SizeX, SizeY);
	if (SizeX <= 0 || SizeY <= 0)
	{
		return 1.0f;
	}
	const float Scale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(SizeX, SizeY));
	return Scale > 0.0f ? Scale : 1.0f;
}

bool AAbyssPlayerController::DeprojectToGround(const FVector2D& ScreenPosition, FVector& OutWorld) const
{
	FVector RayOrigin;
	FVector RayDirection;
	if (!DeprojectScreenPositionToWorld(static_cast<float>(ScreenPosition.X), static_cast<float>(ScreenPosition.Y), RayOrigin, RayDirection))
	{
		return false;
	}
	if (RayDirection.Z > -UE_KINDA_SMALL_NUMBER)
	{
		return false;   // looking at or above the horizon
	}
	const UAbyssSimDriver* Driver = UAbyssSimDriver::Get(this);
	const IAbyssWorldView* View = Driver ? Driver->GetWorldView() : nullptr;

	// Ray / height-field intersection by fixed-point iteration from the Z = 0 plane (the terrain is near flat,
	// world-map-nav.md 1.3); three refinements are plenty for gentle slopes.
	double GroundZ = 0.0;
	FVector Hit = RayOrigin;
	for (int32 Iteration = 0; Iteration < 3; ++Iteration)
	{
		const double T = (GroundZ - RayOrigin.Z) / RayDirection.Z;
		if (T < 0.0)
		{
			return false;
		}
		Hit = RayOrigin + RayDirection * T;
		if (View == nullptr)
		{
			break;
		}
		GroundZ = View->GetGroundHeight(Hit.X, Hit.Y);
	}
	OutWorld = Hit;
	return true;
}

bool AAbyssPlayerController::DeprojectToTile(const FVector2D& ScreenPosition, abyss::Vec2& OutTile) const
{
	FVector Hit;
	if (!DeprojectToGround(ScreenPosition, Hit))
	{
		return false;
	}
	OutTile = AbyssUnits::WorldToTile(Hit);
	return true;
}
