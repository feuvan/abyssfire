// Damage formula and proc helpers (classes-stats-skills.md section 12, combat-feel.md sections 2, 4.3).
// STUB: owner area hero+combat.
#include "abyss/base/Platform.h"

#include "abyss/combat/Damage.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/Assert.h"
#include "abyss/base/Math.h"
#include "abyss/hero/Skills.h"

namespace abyss {

DamageResult CalculateDamage(const DamageRules& rules, const Combatant& attacker, const Combatant& defender,
                             const SkillHitInput& skill, bool forceCrit, Rng& rng) {
  ABYSS_UNIMPLEMENTED();
  DamageResult r;
  r.damage = 1;
  return r;
}

bool CheckCritDoubleStrike(double pct, bool isCrit, Rng& rng) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

bool CheckDoubleShot(double pct, double attackRange, Rng& rng) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

bool CheckFreeCast(double pct, Rng& rng) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

int32_t PercentOfMaxHp(double maxHp, double pct) { return SaturatingInt32(std::floor(maxHp * pct / 100.0)); }

CombatSummary SummarizeCombatant(const DamageRules& rules, const Combatant& c, const SkillHitInput& skill) {
  CombatSummary out;
  if (rules.formulas == nullptr) return out;
  const HeroFormulas& f = *rules.formulas;
  const auto eqv = [&](Stat s) { return c.eq != nullptr ? c.eq->Get(s) : 0.0; };
  const auto buff = [&](BuffStat s) {
    return c.buffs != nullptr && rules.caps != nullptr ? c.buffs->Value(s, *rules.caps) : 0.0;
  };
  const double dex = c.stats.dex + eqv(Stat::Dex);
  const double lck = c.stats.lck + eqv(Stat::Lck);
  const double skillCrit = skill.skill != nullptr ? skill.skill->critBonus : 0.0;
  out.critChancePercent =
      Clamp(dex * f.critPerDex + lck * f.critPerLck + skillCrit + eqv(Stat::CritRate) + c.extraCritPercent, 0.0,
            f.critCapPercent);
  out.critMultiplier = f.critMultiplierBase + lck * f.critMultiplierPerLck + eqv(Stat::CritDamage) / 100.0;
  out.dodgeChancePercent = Clamp(dex * f.dodgePerDex, 0.0, f.dodgeCapPercent);

  // classes 12, defender-free: def 0, DR 0, no amplify, no resistance.
  const DamageType type = skill.skill != nullptr ? skill.skill->damageType : DamageType::Physical;
  double base = 0;
  double mult = 1;
  if (skill.skill != nullptr) {
    base = c.baseDamage + (type == DamageType::Physical ? c.stats.str : c.stats.int_) * f.statToDamage;
    mult = rules.skillRules != nullptr ? SkillDamageMultiplier(*rules.skillRules, *skill.skill, skill.level) *
                                             skill.synergyFactor
                                       : skill.skill->damageMultiplier;
  } else {
    base = c.baseDamage + c.stats.str * f.statToDamage;
  }
  base += eqv(Stat::Damage);
  if (eqv(Stat::DamagePercent) > 0) base *= 1 + eqv(Stat::DamagePercent) / 100.0;
  double elemFlat = eqv(Stat::FireDamage) + eqv(Stat::IceDamage) + eqv(Stat::LightningDamage) + eqv(Stat::PoisonDamage);
  if (eqv(Stat::ElementalDamagePercent) > 0) elemFlat *= 1 + eqv(Stat::ElementalDamagePercent) / 100.0;
  elemFlat += c.baseDamage * buff(BuffStat::PoisonDamage);
  const double outMul = Clamp(c.outgoingMultiplier, f.outgoingMultiplierMin, f.outgoingMultiplierMax);
  const auto finalFor = [&](double critMul) {
    const double raw = (base * mult * critMul + elemFlat) * outMul;
    double fin = (std::max)(f.minDamage, raw);
    if (buff(BuffStat::DamageBonus) > 0) fin *= 1 + buff(BuffStat::DamageBonus);
    if (buff(BuffStat::StealthDamage) > 0) fin *= 1 + buff(BuffStat::StealthDamage);
    return SaturatingInt32((std::max)(f.minDamage, std::floor(fin)));
  };
  out.damageMin = finalFor(1.0);
  out.damageMax = finalFor(out.critMultiplier);
  return out;
}

}  // namespace abyss
