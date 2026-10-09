// Monster definition pipeline (pure): base def -> role transform -> difficulty scale -> instance -> elite affixes.
// Spec: monsters-ai.md 1.2-1.3 (pipeline order and rounding), 9.3 (makeHuntDefinition), 13.2 (labyrinth scaling, later);
// combat-feel.md 14 (DifficultySystem.scaleMonster), 17.3 (affix application).
//
// Owner area: monsters.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/I18n.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Stats.h"
#include "abyss/data/CombatData.h"
#include "abyss/data/DialogueData.h"
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

// Defend wave scaling (monsters 6.4, after difficulty): hp floor(hp * (1 + 0.3 w)), damage floor(damage * (1 + 0.2 w))
// (ai.defendWaveHpPerWave / defendWaveDmgPerWave).
ABYSS_API MonsterDef ScaleDefendWave(const MonsterDef& def, int32_t waveIndex, const MonsterAiDef& ai);

// ---- Labyrinth scaling (monsters 13.2, later milestone; DungeonSystem.raiseToLevel / scaleMonster / makeGatekeeper) ----
// raiseToLevel: level <= 0 or def.level >= level - raiseLevelWindow -> unchanged copy; else m = level / max(1, def.level):
// level = level, hp JsRound(hp m^1.1), damage JsRound(damage m^0.95), defense JsRound(defense m^0.9), expReward
// JsRound(exp m^1.1), both gold bounds JsRound(gold m).
ABYSS_API MonsterDef RaiseToLevel(const MonsterDef& def, int32_t level, const MonsterAiDef& ai);

// One labyrinth floor's monster scaling inputs (DungeonFloorConfig + the floor's curse speedMul, 1 without one).
struct LabyrinthFloorScale {
  int32_t levelTarget = 0;
  int32_t floorNumber = 1;
  double hpMul = 1, damageMul = 1, defenseMul = 1;
  double curseSpeedMul = 1;
};
// scaleMonster (replaces the difficulty step for labyrinth spawns): RaiseToLevel(levelTarget); speed JsRound(speed s),
// attackSpeed JsRound(attackSpeed / s); hp JsRound(hp hpMul diff.hp), damage / defense likewise; expReward
// JsRound(exp (1 + (floor - 1) 0.15) diff.exp); gold bounds JsRound(g (1 + (floor - 1) 0.1)) (no difficulty on gold).
ABYSS_API MonsterDef ScaleLabyrinthMonster(const MonsterDef& def, const LabyrinthFloorScale& floor,
                                           const DifficultyTable& table, Difficulty difficulty, const MonsterAiDef& ai);
// makeGatekeeper: the scaled base; hp JsRound(hp (base isMiniBoss || elite ? 1.6 : 4)), damage JsRound(damage 1.3),
// expReward JsRound(exp 4), gold bounds x 3 (not rounded), aggroRange max(aggro, 8), elite + isMiniBoss, id
// ai.gatekeeperId, nameKey given (dungeon.gatekeeper.<themeId>).
ABYSS_API MonsterDef MakeGatekeeper(const MonsterDef& base, const LabyrinthFloorScale& floor, const DifficultyTable& table,
                                    Difficulty difficulty, const MonsterAiDef& ai, std::string_view nameKey);

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

// The i18n key of a monster's base name (M8 / FIX Q11: data.monster.<id>; hunt leaders data.monster.<huntId>).
ABYSS_API std::string_view MonsterNameKey(const MonsterDef& def);
// The i18n key of an elite affix name: sys.eliteAffix.name.<type> (combat 17.3).
ABYSS_API std::string EliteAffixNameKey(EliteAffixType type);
// buildAffixName (combat 17.3, EliteAffixSystem.buildAffixName) resolved in the current locale: no affix -> the base
// name; else "[name1<sep>name2] base" with the U+00B7 middle dot separator (the in-world label text, M8).
ABYSS_API std::string MonsterLabelText(const I18n& i18n, std::string_view nameKey,
                                       std::span<const EliteAffixType> affixes);
// The linear mini-boss pre-fight lines (monsters 8.3, Q10 / M8): walk startNodeId -> nextNodeId until isEnd (or a
// missing / repeated node) and return data.miniBossDialogue.<monsterId>.<nodeId> per node, in order.
ABYSS_API std::vector<std::string> MiniBossDialogueLineKeys(const DialogueTree& tree, std::string_view monsterId);

}  // namespace abyss
