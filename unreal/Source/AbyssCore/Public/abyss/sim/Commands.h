// Presentation -> core commands (ARCHITECTURE 3.1). Every UI / input action is a command; UE never writes core state.
//
// * Commands are queued by GameSim::Submit and applied at the start of the next unfrozen step, in submission order
//   (S1). While the world is frozen (S2 / D13) the queue is drained too, but hero gameplay commands are REJECTED
//   (move, skill, dodge, target, interact, portal, pickup: D13 "input while frozen"); UI / story / panel commands are
//   applied because they are what unfreezes the world. Hero gameplay commands are also rejected while any modal panel
//   is open (U7) - core-owned modals count from the moment their system opens them, so a command later in the same
//   batch as the interaction that opened a dialogue is already rejected. While the hero is Dying every state-changing
//   gameplay or UI action is rejected, including the system menu (save-ui-input 5.1.1, C12).
// * Panels (SimTypes.h PanelId ownership): CmdOpenPanel / CmdClosePanel report UE-owned panels (HUD panels,
//   SystemMenu, Socket, Confirm). Core-owned modals (dialogue, quest card, shop / forge, stash, mini-boss, lore text,
//   puzzle) are opened by the core; CmdOpenPanel for them is ignored and CmdClosePanel closes them through their owner.
// * Positions are tile space (Vec2{col,row}); UE does the ground / actor picking (world 1.4) and hands tiles in.
// * Directions from keyboard / stick are screen space and are mapped by ScreenDirToTile in the core.
//
// Owner: lead (shared contract). Adding a command = append a struct + add it to the variant + route it in
// Private/sim/SimCommands.cpp (coordinate in review).
#pragma once

#include <cstdint>
#include <string>
#include <variant>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Stats.h"
#include "abyss/base/Types.h"
#include "abyss/data/ItemData.h"
#include "abyss/sim/SimTypes.h"

namespace abyss {

// ---- movement / pointer (world 6.3, 7.1-7.2; save-ui-input 5.2-5.3) ----
struct CmdMoveTo {
  Vec2 tile;
};
// Keyboard / gamepad stick / touch joystick. screenSpace: (x right, y down) mapped with ScreenDirToTile; zero = none.
struct CmdSetMoveInput {
  Vec2 dir;
  bool screenSpace = true;
};
struct CmdStop {};
// Pointer press on the world (7.1 chain). Touch never sends Secondary.
struct CmdPointerPress {
  Vec2 tile;
  PointerButton button = PointerButton::Primary;
  int32_t pointerId = 0;
};
// Hold-to-move update, sent every frame while a pointer that began on the world is down, once with down = false.
struct CmdPointerHold {
  Vec2 tile;
  int32_t pointerId = 0;
  bool down = true;
};

// ---- combat (combat-feel 6, 8, 9) ----
struct CmdAttackTarget {
  EntityId target = kNoEntity;
};
struct CmdClearTarget {};
struct CmdCycleTarget {};
struct CmdCastSkill {
  int32_t slot = 0;               // hotbar slot 0..5 (C3)
  EntityId target = kNoEntity;    // explicit target (touch tap); kNoEntity = preferred target
  bool hasPoint = false;          // pointer ground tile (teleport on desktop)
  Vec2 point;
  Vec2 stickDir;                  // touch joystick at press time (C8), screen space
};
struct CmdDodge {
  Vec2 dir;                       // screen space; zero = facing (C8)
};
struct CmdToggleAutoCombat {};
struct CmdCycleAutoLoot {};
struct CmdUsePotion {
  PotionSlot slot = PotionSlot::Hp;  // I4
};

// ---- world (world 7.4, 9.4) ----
struct CmdInteract {
  EntityId target = kNoEntity;  // kNoEntity = FindInteractTarget (E / RT / touch Talk-Use button)
};
// Explicit pickup (loot nameplate click / tap): routed through ZoneRuntime::InteractWith, so an out-of-range drop is
// walked to and picked up on arrival (W8 / I10 Q22 walk-then-act).
struct CmdPickUp {
  EntityId drop = kNoEntity;
};
struct CmdTownPortal {};
// Answer to the open puzzle prompt (EvPuzzlePrompt, PanelId::Puzzle). Allowed while that modal is open.
inline constexpr int32_t kPuzzleChoiceSolve = 0;  // solution: reward (RewardService), event resolved, prompt closed
inline constexpr int32_t kPuzzleChoiceLeave = 1;  // leave: prompt closed, the event stays unresolved (world 13.3)
struct CmdPuzzleAnswer {
  EntityId prop = kNoEntity;
  int32_t choice = kPuzzleChoiceSolve;
};

// ---- items (loot 7-13) ----
struct CmdEquip {
  std::string uid;
};
struct CmdUnequip {
  EquipSlot slot = EquipSlot::Weapon;
};
struct CmdUseItem {
  std::string uid;
};
struct CmdDiscardItem {
  std::string uid;
};
struct CmdDestroyNormals {};
struct CmdSortBag {};
struct CmdSortStash {};
struct CmdSocketGem {
  EquipSlot slot = EquipSlot::Weapon;
  std::string gemUid;
};
struct CmdUnsocketGem {
  EquipSlot slot = EquipSlot::Weapon;
  int32_t index = 0;
};
struct CmdBuy {
  int32_t wareIndex = 0;
};
struct CmdSell {
  std::string uid;
};
struct CmdBuyback {
  int32_t index = 0;
};
struct CmdStashPut {
  std::string uid;
};
struct CmdStashTake {
  std::string uid;
};
struct CmdCraft {
  CraftAction action = CraftAction::Salvage;
  std::string uid;
};
struct CmdSetPotionSlot {
  PotionSlot slot = PotionSlot::Hp;
  std::string baseId;  // "" = best available
};

// ---- hero progression (classes 6-7, C3) ----
struct CmdLearnSkill {
  int32_t skillIndex = 0;
};
struct CmdAllocStat {
  PrimaryStat stat = PrimaryStat::Str;
  int32_t points = 1;
};
struct CmdSetHotbar {
  int32_t slot = 0;
  int32_t skillIndex = -1;  // -1 clears
};

// ---- pets (quests 18) ----
struct CmdSetActivePet {
  std::string petId;  // "" = rest
};
struct CmdFeedPet {
  std::string petId;
};

// ---- quests / dialogue / story (quests 2, 4, 5.3, 7, 8.5) ----
struct CmdQuestAccept {
  std::string questId;
};
struct CmdQuestTurnIn {
  std::string questId;
  int32_t choice = 0;
};
struct CmdQuestTrack {
  std::string questId;  // "" = untrack
};
struct CmdQuestCardClose {};
// Opens (or refreshes) the quest card of an NPC without the interaction side effects (no dialogue[0] log, no talk
// progress): the T17 chain offer after a turn-in (UE times it: 900 ms, or STORY_STATE{false} + 300 ms while a turn-in
// cutscene plays). Ignored when a card or dialogue is open, the NPC is not in the zone, or it has no card entries.
struct CmdQuestCardOpen {
  std::string npcId;
};
struct CmdDialogueChoose {
  int32_t button = 0;  // index into DialogueView::buttons
};
struct CmdDialogueClose {};
// The quest card's ui.questCard.viewStory button: closes the card, opens the NPC's dialogue tree (quests 5.3 / 7.1).
struct CmdDialogueOpenTree {
  std::string npcId;
};
struct CmdMiniBossDialogueDismiss {};
struct CmdStoryAdvance {};
struct CmdStorySkip {};

// ---- UI / flow (save-ui-input 1, 7; U4, U7) ----
struct CmdOpenPanel {
  PanelId panel = PanelId::Inventory;
};
struct CmdClosePanel {
  PanelId panel = PanelId::Inventory;
};
struct CmdSetTouchMode {
  bool touch = false;
};
// App lifecycle (ue58-platform 13): background -> ResolvePendingDeath + autosave request; foreground clears it.
struct CmdAppBackground {
  bool background = true;
};
// Return to menu / quit: finish a Dying hero's respawn now (save 3.4 rule 3), then UE calls SaveGame().
struct CmdResolvePendingDeath {};
// Debug / test hooks (SimConfig::enableDebugCommands): "giveItem", "addExp", "teleport", "killAll", ...
struct CmdDebug {
  std::string op;
  std::string arg;
  double value = 0;
  Vec2 pos;
};

using Command =
    std::variant<CmdMoveTo, CmdSetMoveInput, CmdStop, CmdPointerPress, CmdPointerHold, CmdAttackTarget, CmdClearTarget,
                 CmdCycleTarget, CmdCastSkill, CmdDodge, CmdToggleAutoCombat, CmdCycleAutoLoot, CmdUsePotion,
                 CmdInteract, CmdPickUp, CmdTownPortal, CmdPuzzleAnswer, CmdEquip, CmdUnequip, CmdUseItem,
                 CmdDiscardItem, CmdDestroyNormals, CmdSortBag, CmdSortStash, CmdSocketGem, CmdUnsocketGem, CmdBuy,
                 CmdSell, CmdBuyback, CmdStashPut, CmdStashTake, CmdCraft, CmdSetPotionSlot, CmdLearnSkill,
                 CmdAllocStat, CmdSetHotbar, CmdSetActivePet, CmdFeedPet, CmdQuestAccept, CmdQuestTurnIn,
                 CmdQuestTrack, CmdQuestCardClose, CmdQuestCardOpen, CmdDialogueChoose, CmdDialogueClose,
                 CmdDialogueOpenTree, CmdMiniBossDialogueDismiss, CmdStoryAdvance, CmdStorySkip, CmdOpenPanel,
                 CmdClosePanel, CmdSetTouchMode, CmdAppBackground, CmdResolvePendingDeath, CmdDebug>;

// True for commands that drive the hero in the world (rejected while frozen, D13, or under a modal, U7).
ABYSS_API bool IsHeroGameplayCommand(const Command& c);

}  // namespace abyss
