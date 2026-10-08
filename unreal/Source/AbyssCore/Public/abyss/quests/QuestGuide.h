// Quest guidance (pure): where the guide arrow points and deterministic gather spots.
// Spec: quests-story-ch1.md 5.1 (computeGuideTarget), 5.2 (refresh), 3.4 (gather spots, resolved deterministically on
// walkable tiles, bit-exact), monsters-ai.md 9.4 (kill objectives point at the nearest living monster of that id, else
// the nearest map spawn, else the hunt spot, else the quest area); DECISIONS Q4 (q_find_goblin_chief area fix, applied
// in the data).
//
// Owner area: quests+story+pets.
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

enum class GuideTargetKind : uint8_t { None, Monster, Spawn, Npc, Area, Point, GatherNode, Clue, Escort, Defend };

struct GuideTarget {
  GuideTargetKind kind = GuideTargetKind::None;
  Vec2 pos;
  std::string id;  // npc id / monster def id / node id
};

// World queries the guide needs (implemented by QuestWorld over the live zone).
struct GuideWorld {
  std::string zoneId;
  Vec2 heroPos;
  std::function<bool(std::string_view monsterId, Vec2 from, Vec2& out)> nearestLivingMonster;
  std::function<bool(std::string_view monsterId, Vec2 from, Vec2& out)> nearestSpawn;
  std::function<bool(std::string_view npcId, Vec2& out)> npcPosition;
  std::function<bool(std::string_view questId, int32_t objectiveIndex, Vec2 from, Vec2& out)> nearestGatherNode;
  std::function<bool(std::string_view clueId, Vec2& out)> cluePosition;
};

// computeGuideTarget (5.1): tracked quest -> nearest target of its first unfinished objective, or the giver once
// complete.
ABYSS_API GuideTarget ComputeGuideTarget(const QuestDef& quest, const QuestProgress& progress, const GuideWorld& world);

// resolveGatherSpots (3.4): bit-exact deterministic spots inside `area` on walkable tiles, keyed by `seedKey`
// (quest id + objective index).
ABYSS_API std::vector<TilePos> ResolveGatherSpots(const TileCircle& area, int32_t count,
                                                  const std::function<bool(int32_t, int32_t)>& walkable,
                                                  std::string_view seedKey);

}  // namespace abyss
