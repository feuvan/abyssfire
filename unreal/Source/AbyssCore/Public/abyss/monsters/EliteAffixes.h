// Elite affix rolling and behaviours (combat-feel.md 17.2, 17.4; monsters-ai.md 1.3 "who rolls affixes").
//
// Owner area: monsters. Pure functions; MonsterSystem drives the per-step behaviours and CombatSystem calls the on-hit
// helpers after a landed monster hit (combat 5.3).
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Rng.h"
#include "abyss/base/Types.h"
#include "abyss/data/CombatData.h"

namespace abyss {

struct MonsterInstance;

// rollAffixes (17.2): count = rng.RandomInt(min, max) for the zone (CountFor), then without replacement:
// idx = RandomInt(0, remaining - 1) over table.order, splice. Rng = RngStream::Ai.
ABYSS_API std::vector<EliteAffixType> RollEliteAffixes(const EliteAffixTable& table, std::string_view zoneId, Rng& rng);

// On-hit results of a landed monster hit (17.4), given the final damage dealt to the hero.
struct EliteOnHit {
  double extraFireDamage = 0;  // floor(finalDmg * extraFire), applied raw to the hero (own EvHit)
  double lifestealHeal = 0;    // floor(finalDmg * lifesteal), MonsterSystem::Heal (FIX: refreshes the bar)
  bool freezeSlow = false;     // rng.Float01() < freezeChance -> hero slow (status_effects.json elite frozen rule)
};
// Draws one Float01 only when the monster has a freezeChance > 0. Rng = RngStream::Combat.
ABYSS_API EliteOnHit EvaluateEliteOnHit(const MonsterInstance& m, const EliteAffixTable& table, double finalDamage, Rng& rng);

// Teleporting (17.4): every teleportCooldownMs (first check immediate) for an aggro monster; if
// 4 < dist^2 < 225 -> hero +/- (1 + rand) per axis with random sign, clamped to [1, size - 2], rounded; returns true
// and the tile when it should blink (caller checks walkability; no safe-zone check). Rng = RngStream::Ai.
ABYSS_API bool EliteTeleportTarget(MonsterInstance& m, const EliteAffixTable& table, Vec2 heroPos, double nowMs, int32_t cols,
                                   int32_t rows, Rng& rng, TilePos& outTile);

// Curse aura (17.4): hero within curseAuraRadius -> true (caller refreshes the tagged damageAmplify buff on the hero);
// `logNow` is set at most every curseLogIntervalMs per monster.
ABYSS_API bool EliteCurseAuraInRange(MonsterInstance& m, const EliteAffixTable& table, Vec2 heroPos, double nowMs, bool& logNow);

}  // namespace abyss
