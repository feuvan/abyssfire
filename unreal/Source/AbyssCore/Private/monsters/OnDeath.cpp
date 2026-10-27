// On-death effects and the Unburied rise: pure rules (dark remake K3; header OnDeath.h). Owner area: monsters.
#include "abyss/base/Platform.h"

#include "abyss/monsters/OnDeath.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/Math.h"
#include "abyss/monsters/MonsterAbilities.h"

namespace abyss {

double OnDeathBaseDamage(const MonsterOnDeathDef& d, double monsterMaxHp, double difficultyDamageMul) {
  if (!d.present || d.kind == OnDeathKind::Spawn) return 0;
  if (d.hasDmg) {
    if (!(d.dmg > 0)) return 0;
    const double mul = difficultyDamageMul > 0 ? difficultyDamageMul : 1.0;
    return (std::max)(1.0, std::floor(d.dmg * mul));
  }
  if (d.hasHpPct) {
    if (!(d.hpPct > 0) || !(monsterMaxHp > 0)) return 0;
    return (std::max)(1.0, JsRound(monsterMaxHp * d.hpPct / 100.0));
  }
  return 0;
}

double OnDeathHeroCap(double heroMaxHp, double capPct, double monsterCapPct, double difficultyDamageMul) {
  const double a = MonsterHitCap(heroMaxHp, capPct, difficultyDamageMul);
  const double b = MonsterHitCap(heroMaxHp, monsterCapPct, difficultyDamageMul);
  if (a > 0 && b > 0) return (std::min)(a, b);
  return a > 0 ? a : b;
}

double OnDeathMonsterDamage(double base, double monsterDmgMul) {
  if (!(base > 0) || !(monsterDmgMul > 0)) return 0;
  return (std::max)(1.0, std::floor(base * monsterDmgMul));
}

AbilityKind OnDeathTelegraphKind(const AbilityShapeDef& s) {
  switch (s.kind) {
    case AbilityShapeKind::Circle: return AbilityKind::Circle;
    case AbilityShapeKind::Ring: return AbilityKind::Ring;
    case AbilityShapeKind::Cone: return AbilityKind::Cone;
    case AbilityShapeKind::Line: return AbilityKind::Line;
  }
  return AbilityKind::Circle;
}

MonsterOnDeathDef VolatileCurseOnDeath(const CurseDef& curse) {
  MonsterOnDeathDef d;
  if (!(curse.deathBurst > 0)) return d;
  d.present = true;
  d.kind = OnDeathKind::Burst;
  d.delayMs = kOnDeathBurstMinFuseMs;
  d.shape.kind = AbilityShapeKind::Circle;
  d.shape.radius = kVolatileBurstRadiusTiles;
  d.hasHpPct = true;
  d.hpPct = JsRound(curse.deathBurst * 100.0 * 1e6) / 1e6;  // .12 -> 12 exactly (not 12.000000000000002)
  d.hasDamageType = true;
  d.damageType = DamageType::Fire;
  d.capHeroMaxHpPct = kOnDeathExploderCapPct;
  return d;
}

double OnDeathWindowRoom(double cap, double nowMs, const std::vector<std::pair<double, double>>& taken) {
  double sum = 0;
  for (const auto& [t, amount] : taken) {
    if (t > nowMs - kOnDeathWindowMs) sum += amount;
  }
  return (std::max)(0.0, cap - sum);
}

bool RiseAgainApplies(const MonsterRiseAgainDef& r, DamageType killingElement, bool killingCrit) {
  if (!r.present) return false;
  if (killingElement == DamageType::Fire) return false;  // a fire killing blow burns the body
  return !(r.notOnCrit && killingCrit);
}

bool MonsterMayRise(MonsterRole role, const MonsterDef& def) {
  switch (role) {
    case MonsterRole::Raised:
    case MonsterRole::ZoneMiniBoss:
    case MonsterRole::SubDungeonMiniBoss:
    case MonsterRole::StoryBoss:
    case MonsterRole::SealKeeper:
    case MonsterRole::HuntLeader:
      return false;
    default:
      break;
  }
  return def.corpse.leaves == CorpseLeaves::Corpse;
}

}  // namespace abyss
