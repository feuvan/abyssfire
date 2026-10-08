// Quest hunts: due logic (pure) and spot search (monsters-ai.md 9.2, 9.4; quests-story-ch1.md 3.8).
//
// Owner area: monsters (QuestSystem supplies the quest progress views).
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/data/MonsterData.h"
#include "abyss/data/QuestData.h"

namespace abyss {

struct QuestProgress;

// huntObjectiveIndex (9.2): first kill objective whose targetId == huntId, or -1.
ABYSS_API int32_t HuntObjectiveIndex(const QuestDef& quest, std::string_view huntId);
// isHuntDue (9.2): quest active; index >= 0; that objective unfinished; revealAfterPrevious -> all earlier done.
ABYSS_API bool IsHuntDue(const QuestDef& quest, const QuestProgress& progress, const HuntDef& hunt);

struct DueHunt {
  const QuestDef* quest = nullptr;
  const HuntDef* hunt = nullptr;
};
// huntsToSpawn (9.2): open quests in QuestSystem order with quest.zone == zoneId, hunts in definition order, not present
// and due.
ABYSS_API std::vector<DueHunt> HuntsToSpawn(std::span<const std::pair<const QuestDef*, const QuestProgress*>> openQuests,
                                            std::string_view zoneId, std::span<const std::string> presentHuntIds,
                                            std::span<const HuntDef> hunts);

// findWalkableNear (9.4): rings r = 1..radius, dr outer, dc inner, only max(|dc|, |dr|) == r; the centre never.
ABYSS_API bool FindWalkableNear(TilePos centre, int32_t radius, const std::function<bool(int32_t, int32_t)>& walkable,
                                TilePos& out);

}  // namespace abyss
