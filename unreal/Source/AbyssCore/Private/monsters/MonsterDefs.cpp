// Monster definition pipeline (monsters-ai.md 1.3, 9.3; combat-feel.md 14, 17.3). STUB: owner area monsters.
#include "abyss/base/Platform.h"

#include "abyss/monsters/MonsterDefs.h"

#include "abyss/base/Assert.h"
#include "abyss/monsters/Monster.h"

namespace abyss {

MonsterDef ScaleMonsterForDifficulty(const MonsterDef& def, const DifficultyTable& table, Difficulty difficulty) {
  ABYSS_UNIMPLEMENTED();
  return def;
}

MonsterDef MakeHuntDefinition(const MonsterDef& base, const HuntDef& hunt, const MonsterAiDef& ai,
                              std::string_view localizedName) {
  ABYSS_UNIMPLEMENTED();
  return hunt.defNormal;
}

MonsterDef ScaleDefendWave(const MonsterDef& def, int32_t waveIndex) {
  ABYSS_UNIMPLEMENTED();
  return def;
}

PrimaryStats MonsterBaseStats(const MonsterDef& def, const MonsterAiDef& ai) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

CombinedAffixStats CombineAffixes(std::span<const EliteAffixType> affixes, const EliteAffixTable& table) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

void ApplyEliteAffixes(MonsterInstance& m, std::span<const EliteAffixType> affixes, const EliteAffixTable& table,
                       const MonsterAiDef& ai) {
  ABYSS_UNIMPLEMENTED();
}

std::string_view MonsterNameKey(const MonsterDef& def) { return def.nameKey; }

}  // namespace abyss
