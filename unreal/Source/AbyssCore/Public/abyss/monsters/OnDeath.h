// On-death effects and the Unburied rise: the pure rules (dark remake K3, implementation-plan.md 3 K3; monsters.md 1.3
// fairness, 1.5 corpses, 2.2 / 3 the Unburied rise, 6 the Labyrinth's volatile curse).
//
// Owner area: monsters. Pure functions over MonsterOnDeathDef / MonsterRiseAgainDef (MonsterData.h); CombatSystem runs the
// on-death effects (Combat.h "K3"), MonsterSystem the rise and the corpses (Corpses.h). No RNG here.
//
// * Damage of an on-death effect: `dmg` x the difficulty's monster damage multiplier (floored, >= 1), or `hpPct` percent
//   of the dying monster's maximum life (rounded half up, >= 1: the web's volatile Math.round(maxHp x fraction)).
// * On the hero: the resistance of a non-physical type (floor(d x (1 - resist / 100)), >= 1), then the smaller of the
//   effect's cap (capHeroMaxHpPct) and the monster's own hit cap (hitCapHeroMaxHpPct), each MonsterHitCap (a percent of the
//   hero's max life x the difficulty's damage multiplier). On a monster: floor(damage x monsterDmgMul) (>= 1), raw.
// * The volatile curse (abyss_run.json `curses[].deathBurst`): a fire burst of round(maxHp x deathBurst) on the hero
//   within 2 tiles (the web's radius), on the fairness fuse (800 ms; the web's 650 ms fuse is under the floor) and the
//   exploder cap (20 % of the hero's max life); monsters are spared (the web's curse punishes the hero only).
#pragma once

#include <cstdint>

#include "abyss/base/Platform.h"
#include "abyss/data/MonsterData.h"
#include "abyss/data/UiData.h"

namespace abyss {

// monsters.md 1.3: an exploder's fuse >= 800 ms, its damage capped at 20 % of the hero's maximum life.
inline constexpr double kOnDeathBurstMinFuseMs = 800;
inline constexpr double kOnDeathExploderCapPct = 20;
// The volatile curse's burst radius (ZoneScene.volatileBurst: Math.hypot(...) > 2 misses).
inline constexpr double kVolatileBurstRadiusTiles = 2;

// The effect's damage before resistances and caps (header rule); 0 for a status-only ring / puddle and a spawn.
ABYSS_API double OnDeathBaseDamage(const MonsterOnDeathDef& d, double monsterMaxHp, double difficultyDamageMul);
// The hero cap of an on-death hit: the smaller positive MonsterHitCap of `capPct` and `monsterCapPct`; 0 = no cap.
ABYSS_API double OnDeathHeroCap(double heroMaxHp, double capPct, double monsterCapPct, double difficultyDamageMul);
// floor(base x monsterDmgMul), at least 1 when both are positive; 0 otherwise.
ABYSS_API double OnDeathMonsterDamage(double base, double monsterDmgMul);
// The telegraph / view kind of an on-death shape (circle -> Circle, ring -> Ring, ...).
ABYSS_API AbilityKind OnDeathTelegraphKind(const AbilityShapeDef& s);
// The Labyrinth's volatile curse as an on-death burst (header rule); `present` false without a deathBurst.
ABYSS_API MonsterOnDeathDef VolatileCurseOnDeath(const CurseDef& curse);

// Whether a death may start the rise `r` (before its chance roll): a rise is defined, the killing blow was not fire,
// and not a critical one when the rise says notOnCrit.
ABYSS_API bool RiseAgainApplies(const MonsterRiseAgainDef& r, DamageType killingElement, bool killingCrit);

}  // namespace abyss
