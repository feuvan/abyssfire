// MonsterInstance helpers (monsters-ai.md 1.2). Owner area: monsters.
#include "abyss/base/Platform.h"

#include "abyss/monsters/Monster.h"

#include <algorithm>
#include <utility>

namespace abyss {

double MonsterInstance::AffixLootBonus(const EliteAffixTable& table) const {
  double total = 0;
  for (const MonsterAffix& a : affixes) total += table.Def(a.type).lootQualityBonus;
  return total;
}

void NormalizeBossAffixPins(std::vector<BossAffixPin>& pins) {
  std::vector<BossAffixPin> kept;
  for (BossAffixPin& p : pins) {
    if (p.zone.empty() || p.monsterId.empty()) continue;
    bool seen = false;
    for (const BossAffixPin& k : kept) seen = seen || (k.zone == p.zone && k.monsterId == p.monsterId);
    if (!seen) kept.push_back(std::move(p));
  }
  std::sort(kept.begin(), kept.end(), [](const BossAffixPin& a, const BossAffixPin& b) {
    return a.zone != b.zone ? a.zone < b.zone : a.monsterId < b.monsterId;
  });
  pins = std::move(kept);
}

}  // namespace abyss
