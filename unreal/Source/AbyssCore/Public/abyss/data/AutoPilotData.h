// The bot's routes (implementation-plan.md 6): autopilot_routes.json, exported from unreal/GameData/autopilot/*.ts.
// abyss::AutoPilot (abyss/tools/AutoPilot.h) interprets a route step by step; each step kind maps onto one of its
// tasks (talk, dialogue tree, quest card, shop, gather, hunt, clues, explore, escort, defend, ...). The route is
// content: every id it names is checked by the exporter and again by DataStore::Finalize, and no Chapter id is spelled
// in the core.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"

namespace abyss {

// Same spelling and order as the exporter's STEP_KINDS (Tools/export-data/src/tables/autopilot.ts) and the
// `stepKinds` list of the file (the loader checks they agree).
enum class AutoPilotStepKind : uint8_t {
  Checkpoint,     // id: a named stop (costs no frame)
  Note,           // id: a diary line
  WaitIdle,       // maxSec
  Talk,           // npc
  ClosePanels,
  Tree,           // npc, ids = the node path
  Visit,          // npc, ids = the quests to accept
  Shop,           // npc, hp / mp (-1: the class's targets)
  TownTrip,       // npc
  Gather,         // quest, obj, count
  Hunt,           // ids = monster / hunt def ids, tile = anchor, until, maxSec
  Boss,           // role, tile = anchor, until, maxSec
  Clues,          // quest, tiles
  Explore,        // quest, obj, tile
  Escort,         // quest, tries
  Defend,         // quest, tries, standOff
  GoTo,           // tile, arrive, maxSec
  PortalHome,
  EquipUpgrades,
  SmokeEnd,       // the Smoke scenario ends here
};
ABYSS_ENUM_STRINGS(AutoPilotStepKind, "checkpoint", "note", "waitIdle", "talk", "closePanels", "tree", "visit", "shop",
                   "townTrip", "gather", "hunt", "boss", "clues", "explore", "escort", "defend", "goTo",
                   "portalHome", "equipUpgrades", "smokeEnd")

// The monster roles a `boss` step hunts by (MonsterRole StoryBoss / ZoneMiniBoss / HuntLeader).
enum class AutoPilotBossRole : uint8_t { StoryBoss, MiniBoss, HuntLeader };
ABYSS_ENUM_STRINGS(AutoPilotBossRole, "storyBoss", "miniBoss", "huntLeader")

enum class AutoPilotConditionKind : uint8_t {
  QuestDone,  // quest completed or turned in
  Objective,  // objective `obj` of `quest` >= atLeast
};
ABYSS_ENUM_STRINGS(AutoPilotConditionKind, "questDone", "objective")

struct AutoPilotCondition {
  AutoPilotConditionKind kind = AutoPilotConditionKind::QuestDone;
  std::string quest;
  int32_t obj = 0;
  int32_t atLeast = 0;
};

struct AutoPilotStep {
  AutoPilotStepKind kind = AutoPilotStepKind::Note;
  std::string id;                 // checkpoint id / note text
  std::string npc;
  std::string quest;
  std::vector<std::string> ids;   // tree path / quests to accept / monsters to hunt
  TilePos tile;                   // hunt / boss anchor, explore centre, goTo target
  std::vector<TilePos> tiles;     // clue marks
  int32_t obj = 0, count = 0;
  int32_t hp = -1, mp = -1;       // shop potion targets (-1: the class's)
  int32_t tries = 1;
  double maxSec = 0, arrive = 0, standOff = 0;
  AutoPilotBossRole role = AutoPilotBossRole::StoryBoss;
  std::vector<AutoPilotCondition> until;  // all must hold
};

struct AutoPilotRoute {
  std::string id;
  std::string zone;
  std::vector<AutoPilotStep> steps;
};

struct ABYSS_API AutoPilotRouteTables {
  std::vector<AutoPilotRoute> routes;  // file order
  const AutoPilotRoute* Find(std::string_view id) const;
  const AutoPilotRoute* ForZone(std::string_view zone) const;  // the first route that plays `zone`
};

}  // namespace abyss
