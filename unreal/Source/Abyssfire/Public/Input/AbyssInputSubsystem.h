// UAbyssInputSubsystem: the app-lifetime bridge of the input layer (input agent: Input/).
//
//   producers                          UAbyssInputSubsystem                       consumers
//   AAbyssPlayerController  ---------> device / hover state  ---------------------> world view (OnHoveredEntityChanged),
//   SAbyssTouchControls     -- Press/  virtual stick                              UI (SetUiInputHandler, hints)
//   UI HUD buttons             Stick -> PressAction -> controller injection
//
// For the other agents:
// * UI agent
//   - SetUiInputHandler: answers FAbyssUiInputRequest (panel hotkeys I C K J M V O P and the touch panel row, story
//     advance / skip, touch log toggle, touch menu button). Return true when consumed; unconsumed story requests fall
//     back to CmdStoryAdvance / CmdStorySkip, OpenSystemMenu to IAbyssUiRoot::HandleBack. Esc / Android back / pad Start
//     outside a story beat call IAbyssUiRoot::HandleBack directly (U4).
//   - Desktop HUD buttons must not submit gameplay commands themselves: call PressAction (skill bar slot click ->
//     Skill1..6, potion slots -> PotionHp / PotionMp, auto-combat / auto-loot buttons -> ToggleAutoCombat /
//     CycleAutoLoot, bag button -> PanelInventory), so they share the keyboard path (ue58-platform.md 8.2).
//   - Touch layout: host CreateTouchControls() full screen on top of the HUD widgets (inside the SSafeZone, below the
//     panels) when the HUD is built; it shows itself only in game, in the touch layout and outside story beats, and reads
//     its own state. SAbyssTouchControls::GetOccupiedRects tells the HUD where the controls sit (target frame / log).
//   - Story overlay: either handle its taps / clicks itself, or leave it hit-test invisible - a world tap / click during
//     a beat becomes a StoryAdvance request, Space / Enter / pad A an advance and Esc / Start / Android back a skip (the
//     story mapping context consumes those keys). Do not also read those keys in Slate (double advance).
//   - Full-screen HUD roots must be SelfHitTestInvisible (else no world click / tap / hover gets through); panel bodies
//     return FReply::Handled() so clicks on them never walk the hero (Q12).
//   - GetKeyHint(Action) for button hints ("F", "RT"); GetLastInputDevice / OnInputDeviceChanged to switch them.
//   - After closing a panel return keyboard focus to the game viewport (FSlateApplication::SetAllUserFocusToGameViewport)
//     or keys stay with the focused widget.
//   - C12: HandleBack must not open the system menu while Snapshot::hero.life is Dying (the core rejects
//     CmdOpenPanel{SystemMenu} then); closing open panels is fine.
// * World agent
//   - Zoom (wheel notches, pinch, right stick Y) goes straight to AAbyssCameraRig::AddZoomInput on the player's view
//     target (the rig clamps to W1's 0.75..1.25); the rig must stay the PlayerController's view target.
//   - OnHoveredEntityChanged / GetHoveredEntity: desktop mouse-over target for highlights (save-ui-input 5.3 "add in UE").
//   - Implement IAbyssWorldView::GetGroundHeight (ground picking) and register every pickable actor with
//     UAbyssActorRegistry; picking projects the visible UMeshComponent bounds of those actors (meshes that are hidden or
//     unregistered are skipped; an actor without meshes gets a kind-sized cylinder).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/ObjectPtr.h"

#include "Input/AbyssInputTypes.h"

#include "AbyssInputSubsystem.generated.h"

class AAbyssPlayerController;
class FAbyssInputDeviceTracker;
class SWidget;
class UAbyssGameInstance;
class UAbyssInputConfig;
struct FAbyssTouchVisuals;

UCLASS()
class ABYSSFIRE_API UAbyssInputSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UAbyssInputSubsystem* Get(const UObject* WorldContextObject);

	// ---- USubsystem ----
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The runtime Enhanced Input objects (built in Initialize). */
	UAbyssInputConfig* GetConfig() const { return Config; }
	UAbyssGameInstance* GetAbyssGameInstance() const;
	/** The local Abyss player controller of the current game world, or nullptr (main menu before BeginPlay, no world). */
	AAbyssPlayerController* GetPlayerController() const;

	// ---- virtual controls (touch widgets, desktop HUD buttons) ----
	/**
	 * A discrete press of Action, injected into Enhanced Input like a key press (ue58-platform.md 8.2) and handled on the
	 * next input tick by the player controller exactly like the key. Device = where the press came from: HUD / touch
	 * presses never aim with the mouse cursor (a skill-bar click must not teleport to the skill bar).
	 */
	void PressAction(EAbyssInputAction Action, EAbyssInputDevice Device = EAbyssInputDevice::Touch);
	/**
	 * Touch joystick state, screen space: X right, Y DOWN, each in [-1, 1] after its dead zone (Q15: 0.15). The controller
	 * injects it into IA_Move every frame while active and fresh (the joystick refreshes it each Slate tick).
	 */
	void SetVirtualStick(const FVector2D& ScreenValue, bool bActive);
	bool IsVirtualStickActive() const;
	FVector2D GetVirtualStick() const;

	// ---- UI bridge ----
	void SetUiInputHandler(FAbyssUiInputHandler Handler) { UiInputHandler = MoveTemp(Handler); }
	void ClearUiInputHandler() { UiInputHandler.Unbind(); }
	/** Offers Request to the UI handler; applies the fallback when unconsumed. Returns true when anything handled it. */
	bool RouteUiRequest(const FAbyssUiInputRequest& Request);
	/** Short label of the key bound to Action on the last used device ("" on touch / unbound). */
	FString GetKeyHint(EAbyssInputAction Action) const;
	FString GetKeyHint(EAbyssInputAction Action, EAbyssInputDevice Device) const;
	/** The full-screen touch layer for the HUD to host (see SAbyssTouchControls). */
	TSharedRef<SWidget> CreateTouchControls(const FAbyssTouchVisuals& Visuals);

	// ---- device ----
	EAbyssInputDevice GetLastInputDevice() const { return LastDevice; }
	void NotifyInputDevice(EAbyssInputDevice Device);
	FOnAbyssInputDeviceChanged OnInputDeviceChanged;

	// ---- hover (desktop) ----
	abyss::EntityId GetHoveredEntity() const { return HoveredEntity; }
	void SetHoveredEntity(abyss::EntityId Entity);
	FOnAbyssHoveredEntityChanged OnHoveredEntityChanged;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbyssInputConfig> Config;

	TSharedPtr<FAbyssInputDeviceTracker> DeviceTracker;
	FAbyssUiInputHandler UiInputHandler;
	FVector2D VirtualStick = FVector2D::ZeroVector;
	double VirtualStickUpdatedSeconds = -1.0;
	abyss::EntityId HoveredEntity = abyss::kNoEntity;
	EAbyssInputDevice LastDevice = EAbyssInputDevice::KeyboardMouse;
	bool bVirtualStickActive = false;
};
