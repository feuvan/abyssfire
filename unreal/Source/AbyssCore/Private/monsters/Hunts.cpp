// Quest hunts due logic (monsters-ai.md 9.2, 9.4). STUB: owner area monsters.
#include "abyss/base/Platform.h"

#include "abyss/monsters/Hunts.h"

#include "abyss/base/Assert.h"
#include "abyss/quests/QuestSystem.h"

namespace abyss {

int32_t HuntObjectiveIndex(const QuestDef& quest, std::string_view huntId) {
  for (size_t i = 0; i < quest.objectives.size(); ++i) {
    const QuestObjectiveDef& o = quest.objectives[i];
    if (o.type == ObjectiveType::Kill && o.targetId == huntId) return static_cast<int32_t>(i);
  }
  return -1;
}

bool IsHuntDue(const QuestDef& quest, const QuestProgress& progress, const HuntDef& hunt) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

std::vector<DueHunt> HuntsToSpawn(std::span<const std::pair<const QuestDef*, const QuestProgress*>> openQuests,
                                  std::string_view zoneId, std::span<const std::string> presentHuntIds,
                                  std::span<const HuntDef> hunts) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

bool FindWalkableNear(TilePos centre, int32_t radius, const std::function<bool(int32_t, int32_t)>& walkable,
                      TilePos& out) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

}  // namespace abyss
