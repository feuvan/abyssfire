// Session-level flow state that no single gameplay area owns (save-ui-input.md section 2 "port: move into the core
// session"), plus the difficulty-ladder helpers of save-ui-input.md 1.2 / combat-feel.md section 14.
//
// Owner area: world (flow). Everything area-specific lives in that area's system (inventory in InventorySystem, quest
// progress in QuestSystem, ...); this struct only holds the cross-cutting bits GameSim and several areas read.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/sim/SimTypes.h"

namespace abyss {

// GameSession.abyss (save-ui-input section 2; labyrinth is a later milestone, the record is saved now).
struct AbyssRecord {
  int32_t unlockedTier = 1;
  int32_t bestTier = 0;
  bool hasBestTime = false;
  double bestTimeMs = 0;
};

enum class DifficultyState : uint8_t { Completed, Available, Locked };
ABYSS_ENUM_STRINGS(DifficultyState, "completed", "available", "locked")

struct DifficultyStates {
  std::array<DifficultyState, EnumCount<Difficulty>()> states{};
  bool hasNextUnlocked = false;
  Difficulty nextUnlocked = Difficulty::Normal;
  DifficultyState Of(Difficulty d) const { return states[static_cast<size_t>(d)]; }
};

// getDifficultyStates (combat-feel 14): normal always available; completed list -> completed / available / locked.
ABYSS_API DifficultyStates GetDifficultyStates(std::span<const Difficulty> completed);
// Migration (14): empty completed list + saved nightmare -> [normal]; hell -> [normal, nightmare]; else unchanged.
ABYSS_API std::vector<Difficulty> DeriveCompletedDifficulties(Difficulty current, std::span<const Difficulty> completed);
// Selector shown if difficulty != normal or any completed.
ABYSS_API bool ShouldShowDifficultySelector(Difficulty current, std::span<const Difficulty> completed);

// Open UI panels as reported by UE through OpenPanel/ClosePanel commands (U7 / S2 freeze rules).
struct ABYSS_API PanelState {
  std::vector<PanelId> open;  // open order (top = back)
  bool IsOpen(PanelId p) const;
  bool AnyModalOpen() const;
  bool AnyHudPanelOpen() const;  // non-modal (pause on touch, U7)
};

struct ABYSS_API SessionState {
  int32_t slot = 0;                       // U1 character slot (0..2), informational for the save id
  Difficulty difficulty = Difficulty::Normal;
  std::vector<Difficulty> completedDifficulties;
  std::string currentMap;                 // zone id of the live zone ("" before the first zone entry)
  std::vector<std::string> visitedZones;  // first-visit detection (chapter cards, achievements Q2)
  bool transitioning = false;             // zone change in progress (CanSave false)
  bool savePending = false;               // save-ui-input 3.4 rule 2 (deferred while Dying)
  double playTimeMs = 0;                  // v4 playTimeMs (sim time while not frozen)
  double nextAutosaveAtMs = 0;            // U1 / FIX Q4: 60 s timer (not while in combat or cinematic)
  int64_t totalKills = 0;
  AbyssRecord abyss;
  PanelState panels;
  bool touchMode = false;                 // U7: HUD panels pause on touch
  bool appBackground = false;
  bool newGame = false;                   // set by NewGame until the first zone entry finished (prologue)

  bool HasVisited(std::string_view mapId) const;
};

}  // namespace abyss
