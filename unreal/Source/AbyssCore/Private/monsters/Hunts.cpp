// Quest hunts due logic and spot search (monsters-ai.md 9.2, 9.4). Owner area: monsters.
#include "abyss/base/Platform.h"

#include "abyss/monsters/Hunts.h"

#include <algorithm>
#include <cstdlib>

#include "abyss/quests/QuestSystem.h"

namespace abyss {

int32_t HuntObjectiveIndex(const QuestDef& quest, std::string_view huntId) {
  for (size_t i = 0; i < quest.objectives.size(); ++i) {
    const QuestObjectiveDef& o = quest.objectives[i];
    if (o.type == ObjectiveType::Kill && o.targetId == huntId) return static_cast<int32_t>(i);
  }
  return -1;
}

namespace {
// A progress record may be shorter than the definition (fresh / migrated saves): missing entries count as 0.
int32_t HuntObjectiveCurrent(const QuestProgress& progress, size_t index) {
  return index < progress.objectives.size() ? progress.objectives[index] : 0;
}
}  // namespace

bool IsHuntDue(const QuestDef& quest, const QuestProgress& progress, const HuntDef& hunt) {
  if (progress.status != QuestStatus::Active) return false;
  const int32_t idx = HuntObjectiveIndex(quest, hunt.huntId);
  if (idx < 0) return false;
  const size_t at = static_cast<size_t>(idx);
  if (HuntObjectiveCurrent(progress, at) >= quest.objectives[at].required) return false;
  if (hunt.revealAfterPrevious) {
    for (size_t i = 0; i < at; ++i) {
      if (HuntObjectiveCurrent(progress, i) < quest.objectives[i].required) return false;
    }
  }
  return true;
}

std::vector<DueHunt> HuntsToSpawn(std::span<const std::pair<const QuestDef*, const QuestProgress*>> openQuests,
                                  std::string_view zoneId, std::span<const std::string> presentHuntIds,
                                  std::span<const HuntDef> hunts) {
  std::vector<DueHunt> out;
  for (const auto& [quest, progress] : openQuests) {
    if (quest == nullptr || progress == nullptr || quest->zone != zoneId || quest->hunts.empty()) continue;
    for (const QuestHuntRef& ref : quest->hunts) {
      const HuntDef* hunt = nullptr;
      for (const HuntDef& h : hunts) {
        if (h.huntId == ref.huntId) {
          hunt = &h;
          break;
        }
      }
      if (hunt == nullptr) continue;
      bool present = false;
      for (const std::string& id : presentHuntIds) {
        if (id == hunt->huntId) {
          present = true;
          break;
        }
      }
      if (present || !IsHuntDue(*quest, *progress, *hunt)) continue;
      out.push_back(DueHunt{quest, hunt});
    }
  }
  return out;
}

bool FindWalkableNear(TilePos centre, int32_t radius, const std::function<bool(int32_t, int32_t)>& walkable,
                      TilePos& out) {
  if (!walkable) return false;
  for (int32_t r = 1; r <= radius; ++r) {
    for (int32_t dr = -r; dr <= r; ++dr) {
      for (int32_t dc = -r; dc <= r; ++dc) {
        if ((std::max)(std::abs(dc), std::abs(dr)) != r) continue;
        if (walkable(centre.col + dc, centre.row + dr)) {
          out = TilePos{centre.col + dc, centre.row + dr};
          return true;
        }
      }
    }
  }
  return false;
}

}  // namespace abyss
