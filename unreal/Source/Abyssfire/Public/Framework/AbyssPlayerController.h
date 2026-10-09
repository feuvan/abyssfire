// AAbyssPlayerController: input -> core commands (ue58-platform.md 8, save-ui-input.md 5, world-map-nav.md 1.4 / 7).
//
// Ownership:
// * backbone (this file outside the marked region, Framework/AbyssPlayerController.cpp): construction, cursor and input
//   mode, BeginPlay / EndPlay / PlayerTick plumbing, command submission and coordinate helpers.
// * input agent: the region marked "input agent" below (add members / functions there) and the whole file
//   Private/Input/AbyssPlayerControllerInput.cpp, which defines SetupInputComponent and the Abyss* input hooks
//   (runtime Enhanced Input objects, KBM / gamepad / touch bindings, picking, hold-to-move, virtual controls injection).
//
// Rules: every gameplay action becomes an abyss::Command through SubmitCommand (never touch core state); world taps are
// resolved to tiles / entity ids here (ground plane intersection + projected actor bounds, no collision traces); the core
// rejects hero commands while input is blocked (Snapshot::inputBlocked), so the controller need not gate them.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "abyss/base/Types.h"
#include "abyss/sim/Commands.h"

#include "Framework/AbyssTypes.h"

#include "AbyssPlayerController.generated.h"

class UAbyssGameInstance;

UCLASS()
class ABYSSFIRE_API AAbyssPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AAbyssPlayerController();

	/** Queues a command for the next sim step (no-op without a session). Game thread only. */
	void SubmitCommand(const abyss::Command& Command) const;
	/** Touch layout active (settings / platform). */
	bool IsTouchMode() const;
	/** Slate DPI scale of the viewport (ScaleToFit 1280x720): viewport px / DpiScale = Slate units (logicalPointer). */
	float GetUiDpiScale() const;
	/**
	 * Screen position (viewport px) -> point on the ground (UE world) via deproject + intersection with the world view's
	 * ground height (Z = 0 when no world view). False when the ray does not hit the ground in front of the camera.
	 */
	bool DeprojectToGround(const FVector2D& ScreenPosition, FVector& OutWorld) const;
	/** Same, in tile space (Vec2 col,row, fractional). */
	bool DeprojectToTile(const FVector2D& ScreenPosition, abyss::Vec2& OutTile) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PlayerTick(float DeltaTime) override;
	/** Defined by the input agent (Private/Input/AbyssPlayerControllerInput.cpp). */
	virtual void SetupInputComponent() override;

	UAbyssGameInstance* GetAbyssGameInstance() const;

private:
	void HandleAppStateChanged(EAbyssAppState NewState);
	void HandleSettingsApplied();
	void ApplyCursorAndInputMode();

	FDelegateHandle AppStateHandle;
	FDelegateHandle SettingsHandle;

	// =====================================================================================================================
	// ---- input agent region (owned by the input agent; implemented in Private/Input/AbyssPlayerControllerInput.cpp) ----
	// =====================================================================================================================
protected:
	/** BeginPlay: create the runtime UInputActions / UInputMappingContexts, add the contexts for the current layout. */
	void InitAbyssInput();
	/** EndPlay: remove contexts, drop references. */
	void ShutdownAbyssInput();
	/** Every PlayerTick: hold-to-move (CmdPointerHold), stick / keyboard (CmdSetMoveInput), touch joystick, ... */
	void TickAbyssInput(float DeltaTime);
	/** App state / touch layout changed: switch mapping contexts (menu vs gameplay, KBM vs touch). */
	void OnAbyssInputContextChanged(EAbyssAppState NewState, bool bTouch);

	/** Keeps the runtime-created Enhanced Input objects alive (GC). Replace the type with the input agent's config class. */
	UPROPERTY(Transient)
	TObjectPtr<UObject> AbyssInputConfig;
	// ---- end of input agent region ----
};
