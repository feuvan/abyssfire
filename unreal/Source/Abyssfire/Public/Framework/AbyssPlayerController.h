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
// input agent: vocabulary of the input half (EAbyssInputAction / EAbyssInputDevice in the input agent region below).
#include "Input/AbyssInputTypes.h"

#include "AbyssPlayerController.generated.h"

class UAbyssGameInstance;
class UAbyssInputConfig;   // input agent
class AAbyssCameraRig;     // input agent (zoom target)

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
	//
	// Devices -> core commands (save-ui-input.md 5, world-map-nav.md 7, DECISIONS I4 / P13 / Q6 / S5 / U4 / U6 / W1 / W3 / W8):
	// * Enhanced Input objects are built at runtime by UAbyssInputConfig (owned by UAbyssInputSubsystem); contexts follow
	//   the app state: IMC_Global always, IMC_MenuPad on the title, IMC_Gameplay_KBM + IMC_Gameplay_Gamepad in game,
	//   IMC_Story on top while a story beat plays (Snapshot::cinematic).
	// * Movement (WASD / arrows / left stick / injected touch joystick) -> CmdSetMoveInput in TILE space, mapped with the
	//   live camera yaw (re-sent every frame while held, zero once on release and again after the input block lifts).
	// * World press (LMB / finger): projected-bounds entity pick (AbyssPicking) -> CmdAttackTarget / CmdInteract /
	//   CmdPickUp (walk-then-act in the core), else the ground tile -> CmdPointerPress + CmdPointerHold every time the
	//   pointer's tile changes while held, CmdPointerHold{down = false} on release (hold-to-move, world 7.2).
	// * RMB / R / D-pad down / touch portal -> CmdTownPortal; wheel / pinch / right stick -> AAbyssCameraRig::AddZoomInput.
	// * Discrete actions (skills, dodge, potions, interact, target cycle, auto toggles) -> their commands; panel hotkeys
	//   and story advance / skip -> UAbyssInputSubsystem::RouteUiRequest; Back -> IAbyssUiRoot::HandleBack (U4).
	// The core rejects hero commands while Snapshot::inputBlocked / Dying, so the controller does not gate them.
public:
	/**
	 * One discrete press of Action injected into Enhanced Input (virtual controls, HUD buttons; ue58-platform.md 8.2).
	 * Handled on the next input tick exactly like the bound key; Device records the source (no cursor aim for HUD / touch).
	 * Use UAbyssInputSubsystem::PressAction rather than calling this directly.
	 */
	void InjectAbyssPress(EAbyssInputAction Action, EAbyssInputDevice Device);
	/** Releases every world pointer (hold-to-move, pinch): e.g. the UI took over the screen. */
	void CancelAbyssWorldPointers();
	/** The runtime Enhanced Input objects (owned by UAbyssInputSubsystem; null before SetupInputComponent). */
	UAbyssInputConfig* GetAbyssInputConfig() const { return AbyssInputConfig; }

protected:
	/** BeginPlay: resolve the runtime UInputActions / UInputMappingContexts (the contexts follow the app state). */
	void InitAbyssInput();
	/** EndPlay: release world pointers, remove the contexts, drop references. */
	void ShutdownAbyssInput();
	/** Every PlayerTick (after input processing): movement, hold-to-move, zoom, hover, story context. */
	void TickAbyssInput(float DeltaTime);
	/** App state / touch layout changed: switch mapping contexts (menu vs gameplay), drop stale pointer state. */
	void OnAbyssInputContextChanged(EAbyssAppState NewState, bool bTouch);

public:
	/** Injects the touch joystick into IA_Move right before Enhanced Input evaluates this frame. */
	virtual void PreProcessInput(const float DeltaTime, const bool bGamePaused) override;
	/** Forgets the sources of this frame's injected presses (consumed by the action handlers). */
	virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;

private:
	/** A finger that went down on the world (not on a Slate control). */
	struct FAbyssWorldTouch
	{
		ETouchIndex::Type Finger = ETouchIndex::Touch1;
		FVector2D Position = FVector2D::ZeroVector;
		/** The press started hold-to-move in the core (ground press). */
		bool bHolding = false;
		/** Part of a pinch (or left over from one): never moves the hero until lifted. */
		bool bInert = false;
		/** Last tile sent with CmdPointerHold (rounded); INT32_MIN = none yet. */
		FIntPoint LastHoldTile = FIntPoint(MIN_int32, MIN_int32);
	};

	// ---- Enhanced Input handlers (bound in SetupInputComponent) ----
	void HandleAbyssActionStarted(EAbyssInputAction Action);
	void HandleAbyssClickStarted();
	void HandleAbyssClickCompleted();
	void HandleAbyssTouchPressed(ETouchIndex::Type FingerIndex, FVector Location);
	void HandleAbyssTouchMoved(ETouchIndex::Type FingerIndex, FVector Location);
	void HandleAbyssTouchReleased(ETouchIndex::Type FingerIndex, FVector Location);

	// ---- helpers ----
	/** The running session's snapshot while in game, else nullptr (valid until the next GameSim call). */
	const abyss::Snapshot* GetAbyssGameSnapshot() const;
	class UEnhancedInputLocalPlayerSubsystem* GetAbyssEnhancedInputSubsystem() const;
	class UAbyssInputSubsystem* GetAbyssInputSubsystem() const;
	/** Adds / removes mapping contexts so the applied set matches the app state and the story flag. */
	void RefreshAbyssMappingContexts(bool bStoryActive);
	/** Live camera yaw (degrees) for screen-relative directions. */
	double GetAbyssCameraYaw() const;
	/** Current move input in screen space (X right, Y up): keys + stick + injected joystick, as evaluated this frame. */
	FVector2D GetAbyssMoveInput() const;
	/** The move input as a "core screen" direction (CmdDodge::dir, CmdCastSkill::stickDir); zero when idle. */
	abyss::Vec2 GetAbyssCoreScreenDir() const;
	bool GetAbyssMouseViewportPosition(FVector2D& OutPosition) const;
	/** Ground tile under a viewport position, clamped to the zone. */
	bool PickAbyssGroundTile(const FVector2D& ScreenPosition, const abyss::Snapshot& Snap, abyss::Vec2& OutTile) const;
	/** True when the mouse is over the game viewport itself, not over a Slate widget on top of it. */
	bool IsAbyssCursorOverWorld() const;
	/**
	 * A press on the world at ScreenPosition (viewport px): entity pick, else ground press. Returns true when the press
	 * started hold-to-move (the caller then streams CmdPointerHold for PointerId); OutHoldTile = the pressed tile.
	 */
	bool PressAbyssWorld(const FVector2D& ScreenPosition, int32 PointerId, bool bTouchPress, FIntPoint& OutHoldTile);
	/** Streams the hold tile under ScreenPosition when its rounded tile changed. */
	void UpdateAbyssHold(const FVector2D& ScreenPosition, int32 PointerId, FIntPoint& InOutLastTile);
	void ReleaseAbyssHold(int32 PointerId);
	void EndAbyssMouseHold();
	void BeginAbyssPinch();
	void CastAbyssSkill(int32 Slot, EAbyssInputDevice Device, bool bFromInjection);
	void ToggleAbyssPanel(EAbyssInputAction Action, EAbyssInputDevice Device);
	void ApplyAbyssZoom(float Notches);
	AAbyssCameraRig* FindAbyssCameraRig();
	void TickAbyssMovement(const abyss::Snapshot& Snap);
	void TickAbyssZoom(float DeltaTime);
	void TickAbyssHolds(const abyss::Snapshot& Snap);
	void TickAbyssHover(const abyss::Snapshot& Snap);
	void ClearAbyssHover();
	void ResetAbyssMovementState();

	/** Keeps the runtime-created Enhanced Input objects alive while referenced (also held by UAbyssInputSubsystem). */
	UPROPERTY(Transient)
	TObjectPtr<UAbyssInputConfig> AbyssInputConfig;

	/** The camera rig zoom goes to (the view target, cached). */
	TWeakObjectPtr<AAbyssCameraRig> AbyssCameraRig;

	EAbyssAppState AbyssInputAppState = EAbyssAppState::Boot;
	/** Bit i set = mapping context i applied (see AbyssPlayerControllerInput.cpp). */
	uint8 AbyssAppliedContexts = 0;
	/** The input component the Enhanced Input handlers were bound to (rebinds if the engine recreates it). */
	TWeakObjectPtr<UInputComponent> AbyssBoundInputComponent;
	bool bAbyssTouchLayout = false;

	// movement (S5)
	bool bAbyssMoveSentNonZero = false;
	bool bAbyssMoveResyncPending = false;
	bool bAbyssWasInputBlocked = false;

	// mouse hold-to-move (world 7.2)
	bool bAbyssMouseHold = false;
	FIntPoint AbyssMouseHoldTile = FIntPoint(MIN_int32, MIN_int32);
	/** Last pointer whose hold was released; the release is re-sent once the input block lifts (it may have been rejected). */
	int32 AbyssLastReleasedPointer = INDEX_NONE;

	// touch world fingers + pinch zoom (W1)
	TArray<FAbyssWorldTouch, TInlineAllocator<4>> AbyssWorldTouches;
	bool bAbyssPinching = false;
	double AbyssPinchDistance = 0.0;

	/** Per action: 0 = no injected press pending, else 1 + EAbyssInputDevice of the injected press (cleared per frame). */
	uint8 AbyssInjectedSource[AbyssInputActionCount] = {};
	// ---- end of input agent region ----
};
