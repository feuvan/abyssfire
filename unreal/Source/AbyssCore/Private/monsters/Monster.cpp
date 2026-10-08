// MonsterInstance helpers (monsters-ai.md 1.2). Owner area: monsters.
#include "abyss/base/Platform.h"

#include "abyss/monsters/Monster.h"

namespace abyss {

double MonsterInstance::AffixLootBonus(const EliteAffixTable& table) const {
  double total = 0;
  for (const MonsterAffix& a : affixes) total += table.Def(a.type).lootQualityBonus;
  return total;
}

}  // namespace abyss
