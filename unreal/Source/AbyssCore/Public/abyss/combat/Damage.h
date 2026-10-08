// The damage formula (classes-stats-skills.md 12, combat-feel.md 2) and proc helpers (combat-feel.md 4.3).
//
// Owner area: hero+combat. Pure functions over Combatant views; RNG draws in the documented order:
// draw 1 = dodge (always), draw 2 = crit (skipped when forceCrit).
#pragma once

#include <cstdint>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Rng.h"
#include "abyss/base/Stats.h"
#include "abyss/data/ClassData.h"
#include "abyss/data/SkillData.h"
#include "abyss/hero/Buffs.h"

namespace abyss {

// CombatEntity view (combat-feel.md 1.2). Hero: raw allocated stats, derived baseDamage/defense (gear included),
// merged EquipStats, outgoing = spirit damage multiplier. Monster: construction stats, def damage/defense, no eq.
struct Combatant {
  PrimaryStats stats;
  double baseDamage = 0;
  double defense = 0;
  double mana = 0;
  double maxHp = 0;
  const BuffList* buffs = nullptr;
  const EquipStats* eq = nullptr;  // nullptr = no equip stats (monsters)
  double outgoingMultiplier = 1.0;
  double extraCritPercent = 0;     // shadow_step critBonus buff converted to points (FIX Q18)
};

struct DamageResult {
  int32_t damage = 0;
  bool isCrit = false;
  bool isDodged = false;
  DamageType type = DamageType::Physical;
  int32_t lifeStolen = 0;
  int32_t manaStolen = 0;
  int32_t manaDamage = 0;  // mana shield absorb (FIX Q16: the caller drains it from the defender's mana)
};

struct DamageRules {
  const HeroFormulas* formulas = nullptr;
  const BuffCaps* caps = nullptr;
  const SkillRules* skillRules = nullptr;
};

// Optional skill input: skill + level + synergy factor (1 when the caller passes no skill levels).
struct SkillHitInput {
  const SkillDef* skill = nullptr;
  int32_t level = 1;
  double synergyFactor = 1.0;
};

// calculateDamage (classes 12) with exactly the web's pipeline and draw order. Dodged results (damage 0) are
// returned as such; callers apply the C1/Q9 fix (a dodged skill hit applies nothing).
ABYSS_API DamageResult CalculateDamage(const DamageRules& rules, const Combatant& attacker, const Combatant& defender,
                                       const SkillHitInput& skill, bool forceCrit, Rng& rng);

// The character panel's "real formula" numbers (save-ui-input Q19 fix), from the same terms as CalculateDamage, with no
// RNG draw: the panel never re-implements the formula.
struct CombatSummary {
  double critChancePercent = 0;   // clamp((dex + eq.dex) * 0.2 + (lck + eq.lck) * 0.5 + skill.critBonus + eq.critRate
                                  //       + extraCritPercent, 0, 75)
  double critMultiplier = 1;      // 1.5 + (lck + eq.lck) * 0.01 + eq.critDamage / 100
  double dodgeChancePercent = 0;  // this combatant's chance to dodge an incoming hit: clamp((dex + eq.dex) * 0.3, 0, 30)
  // The hit (basic attack, or `skill` at its level) against a target with no defense, damage reduction, resistance or
  // amplify, including outgoing multiplier, elemental flat damage and damageBonus / stealth buffs: without and with a
  // crit (the formula has no other variance).
  int32_t damageMin = 0;
  int32_t damageMax = 0;
};
ABYSS_API CombatSummary SummarizeCombatant(const DamageRules& rules, const Combatant& c, const SkillHitInput& skill = {});

// Proc helpers (combat-feel.md 4.3). Each consumes one draw when it can trigger.
ABYSS_API bool CheckCritDoubleStrike(double pct, bool isCrit, Rng& rng);
ABYSS_API bool CheckDoubleShot(double pct, double attackRange, Rng& rng);
ABYSS_API bool CheckFreeCast(double pct, Rng& rng);
// floor(maxHp * pct / 100) (killHealPercent, thornsHeal).
ABYSS_API int32_t PercentOfMaxHp(double maxHp, double pct);

}  // namespace abyss
