// Quest guidance (quests-story-ch1.md 3.4, 5.1-5.2). STUB: owner area quests+story+pets.
#include "abyss/base/Platform.h"

#include "abyss/quests/QuestGuide.h"

#include "abyss/base/Assert.h"
#include "abyss/quests/QuestSystem.h"

namespace abyss {

GuideTarget ComputeGuideTarget(const QuestDef& quest, const QuestProgress& progress, const GuideWorld& world) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

std::vector<TilePos> ResolveGatherSpots(const TileCircle& area, int32_t count,
                                        const std::function<bool(int32_t, int32_t)>& walkable,
                                        std::string_view seedKey) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

}  // namespace abyss
