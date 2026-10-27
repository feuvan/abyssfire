// Dark remake K6 world-state data (implementation-plan.md 3 K6; chapter1.md 3 quest-gated lines, 8 saved world state):
// the conditions NPC lines and NPC visibility are gated on, and the NPC line records. Pure data; evaluation, the world
// flags and the "an NPC never hides while an open quest needs it" validator live in world/WorldState.h.
//
// A condition (npcs.json `lines[].when`, `visibleWhen`) is an object whose keys all hold together:
//   flag: id                     the world flag is set           notFlag: id    it is not set
//   quest: id, state: QuestGate  the quest is in that state
//   any: [leaf, ...]             at least one leaf holds (a leaf is one of flag / notFlag / quest + state; no nesting)
// At least one key; unknown keys are refused by the loader.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"

namespace abyss {

// A quest's state as a condition reads it (QuestSystem: no record, active, completed, turned_in, failed).
//   notStarted: no record (never accepted)       active: accepted, objectives under way
//   completed: objectives done, not turned in    open: active or completed
//   turnedIn: turned in                          notTurnedIn: anything but turned in (no record, active, completed,
//                                                failed)
enum class QuestGate : uint8_t { NotStarted, Active, Completed, Open, TurnedIn, NotTurnedIn };
ABYSS_ENUM_STRINGS(QuestGate, "notStarted", "active", "completed", "open", "turnedIn", "notTurnedIn")

enum class WorldLeafKind : uint8_t { Flag, NotFlag, Quest };

struct WorldConditionLeaf {
  WorldLeafKind kind = WorldLeafKind::Flag;
  std::string id;  // the flag, or the quest
  QuestGate state = QuestGate::TurnedIn;  // Quest leaves
};

// All of `all` hold, and (when `any` is not empty) at least one of `any`.
struct WorldCondition {
  std::vector<WorldConditionLeaf> all;
  std::vector<WorldConditionLeaf> any;
  bool Empty() const { return all.empty() && any.empty(); }
};

// One NPC line (npcs.json `lines[]`): an i18n key, said while `when` holds (no `when`: always).
struct NpcLineDef {
  std::string key;
  bool hasWhen = false;
  WorldCondition when;
};

}  // namespace abyss
