// Monster definition pipeline (monsters-ai.md 1.2-1.3, 6.4, 9.3; combat-feel.md 14, 17.3). Owner area: monsters.
#include "abyss/base/Platform.h"

#include "abyss/monsters/MonsterDefs.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "abyss/base/Math.h"
#include "abyss/monsters/Monster.h"

namespace abyss {

MonsterDef ScaleMonsterForDifficulty(const MonsterDef& def, const DifficultyTable& table, Difficulty difficulty) {
  if (difficulty == Difficulty::Normal) return def;  // the web returns the same object
  const DifficultyDef& m = table.Def(difficulty);
  MonsterDef out = def;
  out.hp = JsRound(def.hp * m.hpMul);
  out.damage = JsRound(def.damage * m.damageMul);
  out.defense = JsRound(def.defense * m.defenseMul);
  out.expReward = JsRound(def.expReward * m.expMul);
  out.goldMin = JsRound(def.goldMin * m.expMul);  // gold uses the exp multiplier
  out.goldMax = JsRound(def.goldMax * m.expMul);
  return out;
}

MonsterDef MakeHuntDefinition(const MonsterDef& base, const HuntDef& hunt, const MonsterAiDef& ai,
                              std::string_view localizedName) {
  const double hpMul = hunt.hasHpMul ? hunt.hpMul : ai.huntHpMul;
  const double dmgMul = hunt.hasDmgMul ? hunt.dmgMul : ai.huntDmgMul;
  MonsterDef d = base;  // speed, attackRange, attackSpeed, level, spriteKey, animCategory, derived rules inherited
  d.id = hunt.huntId;
  d.name = std::string(localizedName);
  d.nameKey = "data.monster." + hunt.huntId;
  d.hp = JsRound(base.hp * hpMul);
  d.damage = JsRound(base.damage * dmgMul);
  d.defense = JsRound(base.defense * ai.huntDefMul);
  d.expReward = JsRound(base.expReward * (std::max)(ai.huntExpMulMin, hpMul));
  d.goldMin = base.goldMin * ai.huntGoldMul;  // not rounded (web: [g0 * 3, g1 * 3])
  d.goldMax = base.goldMax * ai.huntGoldMul;
  d.aggroRange = (std::max)(base.aggroRange, ai.huntAggroMin);
  d.elite = true;
  d.isMiniBoss = true;
  d.sources = {MonsterSource::Hunt};
  return d;
}

MonsterDef ScaleDefendWave(const MonsterDef& def, int32_t waveIndex) {
  MonsterDef out = def;
  const double w = static_cast<double>(waveIndex);
  out.hp = std::floor(def.hp * (1 + 0.3 * w));
  out.damage = std::floor(def.damage * (1 + 0.2 * w));
  return out;
}

PrimaryStats MonsterBaseStats(const MonsterDef& def, const MonsterAiDef& ai) {
  PrimaryStats s;
  s.str = FloorInt(def.damage * ai.strPerDamage);
  s.dex = FloorInt(def.speed * ai.dexPerSpeed);
  s.vit = FloorInt(def.hp * ai.vitPerHp);
  s.int_ = ai.statInt;
  s.spi = ai.statSpi;
  s.lck = ai.statLck;
  return s;
}

CombinedAffixStats CombineAffixes(std::span<const EliteAffixType> affixes, const EliteAffixTable& table) {
  CombinedAffixStats c;
  for (EliteAffixType t : affixes) {
    const EliteAffixDef& d = table.Def(t);
    c.damageMult *= d.damageMult;
    c.speedMult *= d.speedMult;
    c.hpMult *= d.hpMult;
    c.defenseMult *= d.defenseMult;
    c.extraFireDamage += d.extraFireDamage;
    c.lootQualityBonus += d.lootQualityBonus;
    c.lifestealFraction += d.lifestealFraction;
    c.freezeChance = (std::min)(table.freezeChanceCap, c.freezeChance + d.freezeChance);  // capped per step (web)
    if (t == EliteAffixType::Teleporting) c.teleporting = true;
    if (t == EliteAffixType::CurseAura) {
      c.curseAura = true;
      c.curseAuraRadius = (std::max)(c.curseAuraRadius, d.curseAuraRadius);
      c.curseAuraReduction += d.curseAuraReduction;
    }
  }
  return c;
}

void ApplyEliteAffixes(MonsterInstance& m, std::span<const EliteAffixType> affixes, const EliteAffixTable& table,
                       const MonsterAiDef& ai) {
  m.affixes.clear();
  for (EliteAffixType t : affixes) m.affixes.push_back(MonsterAffix{t});
  if (affixes.empty()) return;
  const CombinedAffixStats c = CombineAffixes(affixes, table);
  m.maxHp = std::floor(m.maxHp * c.hpMult);
  m.hp = m.maxHp;
  m.def.damage = std::floor(m.def.damage * c.damageMult);
  m.def.speed = std::floor(m.def.speed * c.speedMult);
  m.def.defense = std::floor(m.def.defense * c.defenseMult);
  m.stats.str = FloorInt(m.def.damage * ai.strPerDamage);  // dex not recomputed (Q18, parity)
}

std::string_view MonsterNameKey(const MonsterDef& def) { return def.nameKey; }

}  // namespace abyss
