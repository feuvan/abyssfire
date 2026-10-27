// Boss phases and arenas: the pure rules (dark remake K4, implementation-plan.md 3 K4; monsters.md 1.3 "no invulnerable
// phases; soft enrages shrink space, they never one-shot").
//
// Owner area: monsters. Pure functions over MonsterPhaseDef (MonsterData.h); MonsterSystem runs them (MonsterSystem.h
// "K4": TickPhases enters the rows, ticks the arenas, stands the phase summons up and resolves Feed). No RNG here.
//
// * Entry: MonsterInstance::phase counts the rows entered (0 = the monster as defined). Row i is entered once, after
//   row i - 1, when the monster's life drops strictly below its hpBelowPct (the abilities' hpBelowPct rule); a hit that
//   crosses two thresholds enters both, in order. Nothing goes back but the M1 reset (the monster walks home: full life,
//   phase 0).
// * Abilities: one whose first switch (in row order) is an abilitiesOn starts switched off (one first switched off starts
//   on); entering rows 0..phase-1 switches theirs on / off in order. The ability's own phases[] must list the phase too
//   (or list none).
// * Arena (ringShrink): its radius goes linearly from fromR to toR over overMs, then stays; the hero hurts strictly
//   outside it (the edge is inside), every arenaTickMs, for dpsPctMaxHp percent of its maximum life per second.
// * Tether: a goal inside the circle stays; one outside becomes the circle's point nearest it.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/data/MonsterData.h"
#include "abyss/sim/SimTypes.h"

namespace abyss {

// The row a monster that entered `phase` rows enters next at `hpPct` percent of its life, -1 = none.
ABYSS_API int32_t NextPhaseToEnter(std::span<const MonsterPhaseDef> phases, int32_t phase, double hpPct);

// Abilities switched off in `phase` (per def.abilities index, 1 = off; header rule). Empty when the monster has no
// phases (nothing is ever switched).
ABYSS_API void PhaseAbilitiesOff(const MonsterDef& def, int32_t phase, std::vector<uint8_t>& out);

// Ability `index` may be picked in `phase` as far as the phase table goes: not switched off (`off`, empty = none) and
// listed by its own phases[] (empty = every phase). Cooldowns, ranges and life are the picker's.
ABYSS_API bool AbilityEnabledInPhase(const MonsterAbilityDef& a, size_t index, std::span<const uint8_t> off,
                                     int32_t phase);

// A ringShrink arena's radius `elapsedMs` after it lit: fromR + (toR - fromR) x clamp(elapsed / overMs, 0, 1).
ABYSS_API double ArenaRadiusAt(double fromR, double toR, double overMs, double elapsedMs);
// The point `pos` lies outside the arena (strictly beyond `radius` of `centre`).
ABYSS_API bool ArenaOutside(Vec2 centre, double radius, Vec2 pos);
// One arena tick on the hero before its resistance: floor(heroMaxHp x dpsPctMaxHp / 100 x tickMs / 1000 x
// difficultyDamageMul), at least 1.
ABYSS_API double ArenaTickDamage(double heroMaxHp, double dpsPctMaxHp, double tickMs, double difficultyDamageMul);

// The tether's goal for `target`: the target when it lies within `radius` of `centre`, else the circle's point nearest
// it (the centre itself for a target on the centre).
ABYSS_API Vec2 TetherGoal(Vec2 centre, double radius, Vec2 target);
// A tethered monster attacks only from within radius + this of the centre (the arrival epsilon, separation pushes), so
// it can stand up to |centre - home| + radius + slack from its home (PhaseFightReach counts it).
inline constexpr double kTetherSlackTiles = 0.25;

// 2F review (DECISIONS M1 amendment): a bound fight - a phase table with an arena or a tether in any row (Valdras's
// dais). Such a monster measures its M1 leash from its authored spawn tile (MonsterInstance::homeTile, the dais; not
// the M10-jittered spawn anchor) over at least PhaseFightReach, so a hero who stays inside the ring (or the monster
// inside its tether) never sends it home to heal and reset (MonsterAI.h MonsterLeashCentre / MonsterLeashRange).
ABYSS_API bool PhaseTableBindsFight(const MonsterDef& def);
// The reach of a bound fight from `home`, tiles (0 when not bound): per row, an arena's |centre - home| + fromR + the
// monster's swing reach (attackRange x attackExitMul: it fights a hero standing on the ring's edge from inside), a
// tether's |centre - home| + radius + kTetherSlackTiles; the largest.
ABYSS_API double PhaseFightReach(const MonsterDef& def, TilePos home, double attackExitMul);

// Damage a monster takes through its phase's damageTakenMul: `amount` itself at x1, else floor(amount x mul) and never
// below min(amount, 1) (a landed hit stays a hit).
ABYSS_API double PhaseScaledDamage(double amount, double mul);

// The clip an enter action plays: Attack01 for Attack, Cast01 for Cast, else the action's own name (Roar, Taunt, Stun,
// Attack02, Cast02, Rise, ...; the AnimAction contract of SimTypes.h).
ABYSS_API std::string_view PhaseActionClip(AnimAction action);

}  // namespace abyss
