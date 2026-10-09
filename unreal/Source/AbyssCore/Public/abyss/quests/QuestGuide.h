// Quest guidance (pure): where the guide arrow points, deterministic gather spots, the clue-mark nudge.
// Spec: quests-story-ch1.md 5.1 (computeGuideTarget, objectiveTarget, questGiverOf), 5.2 (refresh), 3.4 (gather spots,
// resolved deterministically on walkable tiles, bit-exact), 3.5 (nearestWalkable for clue marks), monsters-ai.md 9.4
// (kill objectives point at the nearest living monster of that id, else the nearest map spawn, else the hunt spot, else
// the quest area); DECISIONS Q4 (q_find_goblin_chief area fix, applied by DataStore::Finalize).
//
// Owner area: quests+story.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/data/QuestData.h"

namespace abyss {

struct QuestProgress;

// What the guide points at (render hint: the minimap star and the arrow tint follow `reason`).
enum class GuideTargetKind : uint8_t { None, Monster, Spawn, Npc, Area, Point, GatherNode, Clue, Escort, Defend };
ABYSS_ENUM_STRINGS(GuideTargetKind, "none", "monster", "spawn", "npc", "area", "point", "gatherNode", "clue", "escort",
                   "defend")

enum class GuideReason : uint8_t { TurnIn, Objective };
ABYSS_ENUM_STRINGS(GuideReason, "turn_in", "objective")

struct GuideTarget {
  GuideTargetKind kind = GuideTargetKind::None;  // None = no target (the arrow hides)
  Vec2 pos;                                      // tile
  GuideReason reason = GuideReason::Objective;   // turn_in: brighter arrow, gold minimap star
  int32_t objectiveIndex = -1;                   // -1 for a turn-in
  std::string questId;                           // the guided quest
  bool Valid() const { return kind != GuideTargetKind::None; }
};

// World queries the guide needs (QuestWorld implements them over the live zone; tests use fakes). Every function may be
// left empty (= nothing there). Positions are tiles.
struct GuideWorld {
  Vec2 player;
  // Tile of an NPC standing in this zone (false when it is not here).
  std::function<bool(std::string_view npcId, Vec2& out)> npcTile;
  // Living monsters whose def id is one of `ids`, in monster-list order.
  std::function<void(const std::vector<std::string>& ids, std::vector<Vec2>& out)> monsters;
  // Map spawn points of those monster ids (map order).
  std::function<void(const std::vector<std::string>& ids, std::vector<Vec2>& out)> spawns;
  // Ungathered node tiles of a gather objective (node order).
  std::function<void(std::string_view questId, int32_t objectiveIndex, std::vector<Vec2>& out)> gatherSpots;
  // The NPC that gives the quest (questGiverOf: first NPC in key order listing it); false when none.
  std::function<bool(std::string_view questId, std::string& out)> giverOf;
  // Where the escorted NPC is (false when none is out).
  std::function<bool(Vec2& out)> escortTile;
  // Where an open clue's mark sits (it may be nudged off a wall); false when there is no mark.
  std::function<bool(std::string_view questId, int32_t objectiveIndex, Vec2& out)> clueTile;
  // The tile of a quest hunt (hunt data; false for an unknown hunt id).
  std::function<bool(std::string_view huntId, Vec2& out)> huntTile;
};

// computeGuideTarget (5.1): completed -> the giver's tile (none when the giver is not in this zone); active -> the first
// unfinished objective with a known target (later objectives are tried when one has none); else none.
ABYSS_API GuideTarget ComputeGuideTarget(const QuestDef& quest, const QuestProgress& progress, const GuideWorld& world);

// objectiveMonsters (5.1): kill -> [targetId]; collect / craft_collect with a drop source -> its monsters; else none.
ABYSS_API std::vector<std::string> ObjectiveMonsters(const QuestObjectiveDef& obj);

// resolveGatherSpots (3.4): bit-exact deterministic spots inside `area` on walkable tiles (FNV-1a of the UTF-16 code
// units of `seedKey` = "<questId>:<objectiveIndex>", then a 32-bit mix per draw), >= 3 Manhattan apart, at most
// count * 60 attempts.
ABYSS_API std::vector<TilePos> ResolveGatherSpots(const TileCircle& area, int32_t count,
                                                  const std::function<bool(int32_t, int32_t)>& walkable,
                                                  std::string_view seedKey);

// The gather-spot hash exposed for the spec's test vectors (3.4): FNV state after the key, and the mixer's draws.
class ABYSS_API GatherSpotRng {
 public:
  explicit GatherSpotRng(std::string_view seedKey);
  uint32_t State() const { return h_; }
  double Next();  // [0, 1)

 private:
  uint32_t h_ = 2166136261u;
};

// nearestWalkable (3.5): the tile itself if walkable, else rings 1..maxRing (dr = -r..r outer, dc = -r..r inner, only
// cells with max(|dc|,|dr|) == r); first walkable wins. False when none.
ABYSS_API bool NearestWalkableTile(TilePos tile, const std::function<bool(int32_t, int32_t)>& walkable, int32_t maxRing,
                                   TilePos& out);

}  // namespace abyss
