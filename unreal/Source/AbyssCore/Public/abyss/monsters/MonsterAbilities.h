// Monster abilities and telegraphs: the pure rules (dark remake K2, implementation-plan.md 3 K2; monsters.md 1.3
// fairness; DECISIONS M9 activates bossSkills, S7 abort rule).
//
// Owner area: monsters. Pure functions over MonsterAbilityDef (MonsterData.h); CombatSystem runs them through the
// monster swing path (Combat.h "K2 monster abilities") and HazardField (combat/HazardField.h) keeps what they leave on
// the ground. No RNG here.
//
// * Pick: a swing that comes due (Attack state, attack interval elapsed) becomes the first ready ability by priority
//   (higher first, ties in data order) whose kind runs, cooldown is over, hero distance lies in [rangeMin, rangeMax],
//   life is below hpBelowPct and phase is listed; none -> the basic swing.
// * Shapes: every body is a disc of kAbilityBodyRadiusTiles; a shape hits a body that touches it (the boundary counts).
// * Contact: windupMs when authored, else the clip's Contact (else Release) notify, never below the fairness floor
//   (600 ms, bosses 700 ms); the clip plays at the rate that puts its beat on the contact.
// * Hit cap: a monster with hitCapHeroMaxHpPct never deals more than that percent of the hero's maximum life in one hit
//   on Normal; the percent scales with the difficulty's damage multiplier (Nightmare x1.5, Hell x2), as the monster's
//   damage does.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/data/AssetManifest.h"
#include "abyss/data/CombatData.h"
#include "abyss/data/MonsterData.h"
#include "abyss/data/SkillData.h"
#include "abyss/monsters/Monster.h"
#include "abyss/sim/SimTypes.h"

namespace abyss {

// Radius of every body a shape tests (the hero and monsters), tiles.
inline constexpr double kAbilityBodyRadiusTiles = 0.35;
// Fairness floors of a windup (monsters.md 1.3): every special >= 600 ms on Normal, bosses >= 700 ms.
inline constexpr double kAbilityMinWindupMs = 600;
inline constexpr double kAbilityBossMinWindupMs = 700;

// The kinds whose runtime has landed (K2: circle, cone, line, ring, bolt, hazard; K3: raise, summon). buff / call (K5)
// are parsed and validated but never picked until their package flips them here.
ABYSS_API bool AbilityKindRuns(AbilityKind kind);
// K3: kinds that stand bodies up (raise, summon); they are usable only when the runtime found bodies to stand up
// (AbilityPickInput::available).
ABYSS_API bool AbilityKindSummons(AbilityKind kind);
// Kinds that draw a ground telegraph (circle, cone, line, ring, hazard).
ABYSS_API bool AbilityHasGroundShape(AbilityKind kind);

struct AbilityPickInput {
  double nowMs = 0;
  double heroDist = 0;               // tiles, caster to hero
  double hpPct = 100;                // caster life, percent of max
  int32_t phase = 0;                 // MonsterInstance::phase (K4)
  std::span<const double> readyAtMs; // per ability index (MonsterInstance::abilityReadyAtMs)
  // K3, per ability index: nonzero when a raise / summon has bodies to stand up now (CombatSystem's plan: corpses in
  // reach, room under its caps). A raise / summon without an entry is not usable; other kinds ignore it.
  std::span<const uint8_t> available;
  // K3: the cast between swings (Chase, or Attack before the swing is due): only raise / summon kinds are usable.
  bool summonsOnly = false;
};

// The conditions of one ability (kind runs, ready, range, life, phase; K3: a raise / summon has bodies to stand up).
ABYSS_API bool AbilityUsable(const MonsterAbilityDef& a, size_t index, const AbilityPickInput& in);
// The ability a due swing becomes (index into `abilities`), -1 = the basic swing (with `summonsOnly`: the raise / summon
// cast between swings, -1 = none).
ABYSS_API int32_t PickMonsterAbility(std::span<const MonsterAbilityDef> abilities, const AbilityPickInput& in);

// Whether a body of `bodyRadius` at `point` touches the shape placed at `origin` facing `dir` (unit; cones and lines).
ABYSS_API bool ShapeContains(const AbilityShapeDef& s, Vec2 origin, Vec2 dir, Vec2 point,
                             double bodyRadius = kAbilityBodyRadiusTiles);
// Radius around the origin that holds the whole shape (monster queries).
ABYSS_API double ShapeReach(const AbilityShapeDef& s);

struct AbilityTiming {
  double contactMs = 0;   // from windup start
  double durationMs = 0;  // whole clip at the play rate (at least the contact)
  double playRate = 1;    // puts the clip's beat on contactMs
  bool fromClip = false;  // the clip carries a Contact / Release beat
};
// Contact timing (header comment). `clip` = the manifest clip of the caster's art (nullptr when unknown); `minWindupMs`
// = the caster's floor (kAbilityMinWindupMs, kAbilityBossMinWindupMs for TelegraphStyle::Boss). Without a clip length the
// duration keeps the rig preset's swing share (attackDuration x contact / frameContact).
ABYSS_API AbilityTiming ComputeAbilityTiming(const MonsterAbilityDef& a, const AnimClipDef* clip, double minWindupMs,
                                             const AnimTimingTable& t, AnimRig rig);
// The EvPlayAnim action of a clip: Attack02 / Cast02 / Roar by name, Cast for Cast01 (and other Cast* clips), Attack for
// the rest (the event carries the clip, which UE plays by name: Lunge, Consume, ...).
ABYSS_API AnimAction AbilityAnimAction(std::string_view clip);

// Where an ability lands, resolved once at windup start (MonsterData.h AbilityAim). Ground shapes without a direction
// (circle, ring) sit on the aim point; cones and lines start at the caster (at the anchor for aim `anchor`) and point at
// the hero's position at windup start (aim self: along the caster's heading). A zero direction falls back to the
// heading, then +col.
struct AbilityAimInput {
  Vec2 casterPos;
  Vec2 casterHeading{1, 0};
  TilePos anchor;
  Vec2 heroPos;
};
ABYSS_API MonsterAbilityCast ResolveAbilityCast(const MonsterAbilityDef& a, const AbilityAimInput& in, double startMs,
                                                double contactMs);

// The telegraph style of a monster instance (SimTypes.h TelegraphStyle) and its windup floor.
ABYSS_API TelegraphStyle TelegraphStyleOf(const MonsterInstance& m);
ABYSS_API double AbilityMinWindupMs(TelegraphStyle style);

// floor(heroMaxHp x capPct x difficultyDamageMul / 100), at least 1; 0 = no cap (capPct <= 0).
ABYSS_API double MonsterHitCap(double heroMaxHp, double capPct, double difficultyDamageMul);
// min(damage, MonsterHitCap(...)) when the monster has a cap, else `damage`.
ABYSS_API double ApplyMonsterHitCap(double damage, double heroMaxHp, double capPct, double difficultyDamageMul);

// The status an ability (or its hazard) applies, as a C6 rule: fixed value, no draw at chance 1.
ABYSS_API StatusRule AbilityStatusRule(const AbilityStatusDef& s);
// Hazard tick damage: floor(monsterDamage x tickDmgMul), at least 1 when tickDmgMul > 0; 0 = a status-only hazard.
ABYSS_API double HazardTickDamage(double monsterDamage, double tickDmgMul);

}  // namespace abyss
