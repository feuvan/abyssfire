// Shared vocabulary of the input layer (input agent: Input/). No UObjects here.
//
// * EAbyssInputAction: one runtime-created UInputAction each (UAbyssInputConfig, ue58-platform.md 8.1, save-ui-input.md
//   5.6). Keyboard / mouse / gamepad keys are mapped to them; touch widgets and HUD buttons INJECT into the same actions
//   (ue58-platform.md 8.2), so the player controller has a single gameplay path for every device.
// * EAbyssInputDevice: the device that produced the last meaningful input (button hints, teleport aim source).
// * FAbyssUiInputRequest: what input asks of the Slate UI (panel hotkeys, story overlay advance / skip, touch log
//   toggle, touch menu button). The UI agent answers through UAbyssInputSubsystem::SetUiInputHandler.
#pragma once

#include "CoreMinimal.h"

#include "abyss/base/Types.h"
#include "abyss/sim/SimTypes.h"

ABYSSFIRE_API DECLARE_LOG_CATEGORY_EXTERN(LogAbyssInput, Log, All);

/** Every runtime input action. Order is the index into UAbyssInputConfig's action table. */
enum class EAbyssInputAction : uint8
{
	// ---- analog ----
	Move,            // Axis2D, UE convention: X = screen right, Y = screen up (WASD / arrows / left stick / joystick)
	Zoom,            // Axis1D, mouse wheel: +1 per notch = zoom in (W1)
	ZoomStick,       // Axis1D, gamepad right stick Y (continuous zoom)
	// ---- pointer ----
	Click,           // LMB: world press / hold-to-move (world-map-nav 7.1 / 7.2)
	AltClick,        // RMB: town portal (web parity, save-ui-input 5.3 step 2)
	// ---- flow ----
	Back,            // Esc / Android back / gamepad Start (U4) -> IAbyssUiRoot::HandleBack
	StoryAdvance,    // Space / Enter / LMB / pad A while a story beat plays (IMC_Story only)
	StorySkip,       // Esc / pad Start / Android back while a story beat plays (IMC_Story only)
	// ---- combat ----
	Skill1,
	Skill2,
	Skill3,
	Skill4,
	Skill5,
	Skill6,
	Dodge,
	TargetCycle,     // "lock" (combat-feel 9.3)
	ToggleAutoCombat,
	CycleAutoLoot,
	TownPortal,      // W3
	Interact,        // world-map-nav 7.4 (nearest in-range target)
	PotionHp,        // I4
	PotionMp,        // I4
	// ---- HUD panels (UE-owned, toggled by the UI; U6 gating in the controller) ----
	PanelInventory,
	PanelCharacter,
	PanelSkills,
	PanelQuestLog,
	PanelWorldMap,
	PanelAchievements,
	PanelSettings,
	PanelPets,

	Count
};

inline constexpr int32 AbyssInputActionCount = static_cast<int32>(EAbyssInputAction::Count);

/** Skill slot (0..5) of a Skill1..Skill6 action, else INDEX_NONE. */
inline int32 AbyssInputSkillSlot(EAbyssInputAction Action)
{
	const int32 Value = static_cast<int32>(Action);
	const int32 First = static_cast<int32>(EAbyssInputAction::Skill1);
	return (Value >= First && Value < First + 6) ? Value - First : INDEX_NONE;
}

/** The UE-owned panel a Panel* action toggles; false for other actions. */
ABYSSFIRE_API bool AbyssInputActionToPanel(EAbyssInputAction Action, abyss::PanelId& OutPanel);

/** Stable asset-style name of an action (IA_Move, IA_Skill1, ...). */
ABYSSFIRE_API const TCHAR* AbyssInputActionName(EAbyssInputAction Action);

/** The device that produced the last meaningful input. */
enum class EAbyssInputDevice : uint8
{
	KeyboardMouse,
	Gamepad,
	Touch,
};

ABYSSFIRE_API const TCHAR* LexToString(EAbyssInputDevice Device);

/** What the input layer asks of the UI (UAbyssInputSubsystem::RouteUiRequest). */
enum class EAbyssUiRequest : uint8
{
	/** Toggle a UE-owned HUD panel (Panel). The UI owns open state + CmdOpenPanel / CmdClosePanel (SimTypes.h). */
	TogglePanel,
	/**
	 * Story overlay input (StoryDirector.h): the UI completes a running reveal itself and sends CmdStoryAdvance only for
	 * the input that advances. Fallback without a handler: CmdStoryAdvance.
	 */
	StoryAdvance,
	/** Esc / Start / skip during a story beat. Fallback: CmdStorySkip. */
	StorySkip,
	/** Touch top-left "log" toggle: expanded / collapsed combat log (save-ui-input 6.10). No fallback. */
	ToggleCombatLog,
	/** Touch "menu" button (Q30 / U9: system menu with settings and achievements). Fallback: IAbyssUiRoot::HandleBack. */
	OpenSystemMenu,
};

ABYSSFIRE_API const TCHAR* LexToString(EAbyssUiRequest Request);

struct FAbyssUiInputRequest
{
	EAbyssUiRequest Kind = EAbyssUiRequest::TogglePanel;
	/** TogglePanel only. */
	abyss::PanelId Panel = abyss::PanelId::Inventory;
	/** Device that produced the request (the UI may show device-specific hints). */
	EAbyssInputDevice Device = EAbyssInputDevice::KeyboardMouse;
};

/** Return true when the UI consumed the request (no fallback runs). */
DECLARE_DELEGATE_RetVal_OneParam(bool, FAbyssUiInputHandler, const FAbyssUiInputRequest& /*Request*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAbyssInputDeviceChanged, EAbyssInputDevice /*NewDevice*/);
/** Desktop hover target changed (kNoEntity = nothing under the cursor). */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAbyssHoveredEntityChanged, abyss::EntityId /*Entity*/);

/** What a press on a picked world entity does (AbyssPicking). */
enum class EAbyssPickAction : uint8
{
	None,      // nothing pickable under the pointer: ground press
	Attack,    // living monster: CmdAttackTarget (C7 approach stops at attack range)
	Interact,  // NPC / hidden-reward chest / event prop: CmdInteract{id} (walk-then-act, W8 / Q6)
	PickUp,    // ground item: CmdPickUp (walk-then-act, Q22)
	WalkTo,    // proximity-only objects (potions, lore, gather nodes, clues, escort, soul echo): press at their tile
};

struct FAbyssPickResult
{
	abyss::EntityId Id = abyss::kNoEntity;
	abyss::EntityKind Kind = abyss::EntityKind::None;
	EAbyssPickAction Action = EAbyssPickAction::None;
	/** The entity's tile-space position (snapshot). */
	abyss::Vec2 Tile;

	bool IsValid() const { return Action != EAbyssPickAction::None; }
};
