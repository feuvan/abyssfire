// Damage formula and proc helpers (classes-stats-skills.md section 12, combat-feel.md sections 2, 4.3).
#include "abyss/base/Platform.h"

#include "abyss/combat/Damage.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/Assert.h"
#include "abyss/base/Math.h"
#include "abyss/hero/Skills.h"

namespace abyss {

namespace {

const HeroFormulas& DamageFormulasOrDefault(const DamageRules& rules) {
  static const HeroFormulas kDefaults{};
  ABYSS_ASSERT(rules.formulas != nullptr, "CalculateDamage: DamageRules::formulas is null");
  return rules.formulas != nullptr ? *rules.formulas : kDefaults;
}

double DamageEq(const Combatant& c, Stat s) { return c.eq != nullptr ? c.eq->Get(s) : 0.0; }

// getBuffValue: the capped sum (uncapped when the rules carry no caps table).
double DamageBuff(const DamageRules& rules, const Combatant& c, BuffStat s) {
  if (c.buffs == nullptr) return 0.0;
  return rules.caps != nullptr ? c.buffs->Value(s, *rules.caps) : c.buffs->RawSum(s);
}

// getResistance (CombatSystem.ts:200): allResist + the type's own resist (arcane: allResist only), clamped.
double DamageResist(const HeroFormulas& f, const Combatant& d, DamageType type) {
  if (d.eq == nullptr) return 0.0;
  double res = d.eq->Get(Stat::AllResist);
  switch (type) {
    case DamageType::Fire: res += d.eq->Get(Stat::FireResist); break;
    case DamageType::Ice: res += d.eq->Get(Stat::IceResist); break;
    case DamageType::Lightning: res += d.eq->Get(Stat::LightningResist); break;
    case DamageType::Poison: res += d.eq->Get(Stat::PoisonResist); break;
    case DamageType::Physical:
    case DamageType::Arcane: break;
  }
  return Clamp(res, 0.0, f.resistCapPercent);
}

}  // namespace

DamageResult CalculateDamage(const DamageRules& rules, const Combatant& attacker, const Combatant& defender,
                             const SkillHitInput& skill, bool forceCrit, Rng& rng) {
  const HeroFormulas& f = DamageFormulasOrDefault(rules);
  DamageResult r;

  // 1. Dodge: always one draw.
  const double dodgeRate =
      Clamp((defender.stats.dex + DamageEq(defender, Stat::Dex)) * f.dodgePerDex, 0.0, f.dodgeCapPercent);
  if (rng.Chance(dodgeRate)) {
    r.damage = 0;
    r.isDodged = true;
    r.type = DamageType::Physical;
    return r;
  }

  // 2-3. Crit (no draw when forced).
  const double skillCrit = skill.skill != nullptr ? skill.skill->critBonus : 0.0;
  const double atkLck = attacker.stats.lck + DamageEq(attacker, Stat::Lck);
  const double critRate =
      Clamp((attacker.stats.dex + DamageEq(attacker, Stat::Dex)) * f.critPerDex + atkLck * f.critPerLck + skillCrit +
                DamageEq(attacker, Stat::CritRate) + attacker.extraCritPercent,
            0.0, f.critCapPercent);
  const bool isCrit = forceCrit || rng.Chance(critRate);
  const double critMul =
      isCrit ? f.critMultiplierBase + atkLck * f.critMultiplierPerLck + DamageEq(attacker, Stat::CritDamage) / 100.0
             : 1.0;

  // 4-5. Type, base and multiplier (raw allocated stats, not gear).
  const DamageType type = skill.skill != nullptr ? skill.skill->damageType : DamageType::Physical;
  double base = 0;
  double mult = 1;
  if (skill.skill != nullptr) {
    const double statBonus = type == DamageType::Physical ? attacker.stats.str : attacker.stats.int_;
    base = attacker.baseDamage + statBonus * f.statToDamage;
    mult = rules.skillRules != nullptr ? SkillDamageMultiplier(*rules.skillRules, *skill.skill, skill.level)
                                       : skill.skill->damageMultiplier;
    mult *= skill.synergyFactor;
  } else {
    base = attacker.baseDamage + attacker.stats.str * f.statToDamage;
  }

  // 6-7. Flat and percent gear damage.
  base += DamageEq(attacker, Stat::Damage);
  const double dmgPct = DamageEq(attacker, Stat::DamagePercent);
  if (dmgPct > 0) base *= 1 + dmgPct / 100.0;

  // 8. Flat elemental (not crit-multiplied) + poison blade.
  double elem = DamageEq(attacker, Stat::FireDamage) + DamageEq(attacker, Stat::IceDamage) +
                DamageEq(attacker, Stat::LightningDamage) + DamageEq(attacker, Stat::PoisonDamage);
  const double elemPct = DamageEq(attacker, Stat::ElementalDamagePercent);
  if (elemPct > 0) elem *= 1 + elemPct / 100.0;
  const double poisonBuff = DamageBuff(rules, attacker, BuffStat::PoisonDamage);
  if (poisonBuff > 0) elem += attacker.baseDamage * poisonBuff;

  // 9. Attacker multipliers.
  const double stealth = DamageBuff(rules, attacker, BuffStat::StealthDamage);
  const double dmgBonus = DamageBuff(rules, attacker, BuffStat::DamageBonus);

  // 10. Damage reduction (buffs + gear, capped).
  double dr = DamageBuff(rules, defender, BuffStat::DamageReduction);
  const double gearDr = DamageEq(defender, Stat::DamageReduction);
  if (gearDr > 0) dr += gearDr / 100.0;
  dr = (std::min)(dr, f.damageReductionCap);

  // 11-12. Effective defense.
  double def = defender.defense;
  const double gearDef = DamageEq(defender, Stat::Defense);
  if (gearDef > 0) def += gearDef;
  const double defPct = DamageEq(defender, Stat::DefensePercent);
  if (defPct > 0) def *= 1 + defPct / 100.0;
  const double defBonus = DamageBuff(rules, defender, BuffStat::DefenseBonus);
  if (defBonus > 0) def *= 1 + defBonus;
  const double ignoreDef = DamageEq(attacker, Stat::IgnoreDefense);
  if (ignoreDef > 0) def *= 1 - ignoreDef / 100.0;

  // 13-14.
  const double out = Clamp(attacker.outgoingMultiplier, f.outgoingMultiplierMin, f.outgoingMultiplierMax);
  const double raw = (base * mult * critMul + elem) * out;
  double fin = (std::max)(f.minDamage, raw - def * f.defenseFactor) * (1 - dr);
  if (dmgBonus > 0) fin *= 1 + dmgBonus;
  if (stealth > 0) fin *= 1 + stealth;
  const double amplify = DamageBuff(rules, defender, BuffStat::DamageAmplify);
  if (amplify > 0) fin *= 1 + amplify;

  // 15. Resistance (non-physical only).
  if (type != DamageType::Physical) fin *= 1 - DamageResist(f, defender, type) / 100.0;

  // 16.
  int32_t finalDamage = SaturatingInt32((std::max)(f.minDamage, std::floor(fin)));

  // 17. Mana shield (FIX Q16: the caller drains manaDamage from the defender's mana). The absorbed amount is whole:
  // a fractional mana pool absorbs floor(mana) so the HP damage stays an integer.
  const double manaShield = DamageBuff(rules, defender, BuffStat::ManaShield);
  if (manaShield > 0 && defender.mana > 0) {
    const double redirect = std::floor(finalDamage * manaShield);
    const double absorb = (std::min)(redirect, std::floor(defender.mana));
    finalDamage = SaturatingInt32((std::max)(f.minDamage, finalDamage - absorb));
    r.manaDamage = SaturatingInt32(absorb);
  }

  // 18. Steal.
  const double lifeSteal = DamageEq(attacker, Stat::LifeSteal);
  const double manaSteal = DamageEq(attacker, Stat::ManaSteal);
  r.lifeStolen = lifeSteal > 0 ? SaturatingInt32(std::floor(finalDamage * lifeSteal / 100.0)) : 0;
  r.manaStolen = manaSteal > 0 ? SaturatingInt32(std::floor(finalDamage * manaSteal / 100.0)) : 0;

  r.damage = finalDamage;
  r.isCrit = isCrit;
  r.isDodged = false;
  r.type = type;
  return r;
}

bool CheckCritDoubleStrike(double pct, bool isCrit, Rng& rng) {
  if (!isCrit || !(pct > 0)) return false;
  return rng.Float01() * 100.0 < pct;
}

bool CheckDoubleShot(double pct, double attackRange, Rng& rng) {
  if (!(pct > 0) || !(attackRange > 2)) return false;
  return rng.Float01() * 100.0 < pct;
}

bool CheckFreeCast(double pct, Rng& rng) {
  if (!(pct > 0)) return false;
  return rng.Float01() * 100.0 < pct;
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
