// Monster definition pipeline (pure): base def -> role transform -> difficulty scale -> instance -> elite affixes.
// Spec: monsters-ai.md 1.2-1.3 (pipeline order and rounding), 9.3 (makeHuntDefinition), 13.2 (labyrinth scaling, later);
// combat-feel.md 14 (DifficultySystem.scaleMonster), 17.3 (affix application).
//
// Owner area: monsters.
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Stats.h"
#include "abyss/data/CombatData.h"
#include "abyss/data/MonsterData.h"

namespace abyss {

struct MonsterInstance;

// scaleMonster (combat 14): normal -> unchanged copy; else JsRound of hp, damage, defense, expReward and both gold
// bounds (gold uses the exp multiplier).
ABYSS_API MonsterDef ScaleMonsterForDifficulty(const MonsterDef& def, const DifficultyTable& table, Difficulty difficulty);

// makeHuntDefinition (monsters 9.3): id = huntId, name = localized name, hp round(hp * hpMul), damage
// round(damage * dmgMul), defense round(defense * 1.2), expReward round(exp * max(3, hpMul)), gold x3,
// aggroRange max(base, 7), elite + isMiniBoss true. Difficulty scaling is applied by the caller afterwards.
ABYSS_API MonsterDef MakeHuntDefinition(const MonsterDef& base, const HuntDef& hunt, const MonsterAiDef& ai,
                                        std::string_view localizedName);

// Defend wave scaling (monsters 6.4, after difficulty): hp floor(hp * (1 + 0.3 w)), damage floor(damage * (1 + 0.2 w)).
ABYSS_API MonsterDef ScaleDefendWave(const MonsterDef& def, int32_t waveIndex);

// Monster combat stats (monsters 1.2): {str floor(dmg * 0.8), dex floor(speed * 0.1), vit floor(hp * 0.1), 3, 3, 3}.
ABYSS_API PrimaryStats MonsterBaseStats(const MonsterDef& def, const MonsterAiDef& ai);

// Combined affix stats (combat 17.3 getCombinedStats): multipliers multiply; extra fire, loot bonus and lifesteal add;
// freezeChance = min(cap, sum).
struct CombinedAffixStats {
  double damageMult = 1, speedMult = 1, hpMult = 1, defenseMult = 1;
  double extraFireDamage = 0;
  double lootQualityBonus = 0;
  double lifestealFraction = 0;
  double freezeChance = 0;
  bool teleporting = false;
  bool curseAura = false;
  double curseAuraRadius = 0, curseAuraReduction = 0;
};
ABYSS_API CombinedAffixStats CombineAffixes(std::span<const EliteAffixType> affixes, const EliteAffixTable& table);

// applyEliteAffixes (combat 17.3): maxHp = hp = floor(maxHp * hpx); def = {damage floor, speed floor, defense floor};
// stats.str = floor(damage * 0.8) (dex not recomputed). Stores the affixes on the instance. originalDef untouched.
ABYSS_API void ApplyEliteAffixes(MonsterInstance& m, std::span<const EliteAffixType> affixes, const EliteAffixTable& table,
                                 const MonsterAiDef& ai);

// Display name key/args for a monster label: "[affix1.affix2] name" (M8: i18n keys data.monster.<id>,
// data.eliteAffix.<type>.name). Returns the i18n key of the base name; affix keys are appended to `affixKeys`.
ABYSS_API std::string_view MonsterNameKey(const MonsterDef& def);

}  // namespace abyss
