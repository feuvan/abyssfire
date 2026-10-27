// Per-tick monster AI (pure): state machine, leash, patrol, movement.
// Spec: monsters-ai.md 3.1-3.7 (states, exact per-tick algorithm, transition table, moveToward, leash, patrol);
// DECISIONS M1 (Returning leash: walk home at normal speed, heal 0.6 maxHp/s, ignore the hero), M2 (provoked: 5 s forced
// chase that ignores the 1.5x aggro drop), M6 (A* when line-of-walk is blocked + separation steering + wall sliding),
// M10 (4000 ms patrol timeout, 8 placement tries), S1 (fixed 60 Hz dt).
// Dark remake K4: a phase's tether (MonsterInstance::hasTether) holds the chase and patrol goals inside its circle
// (Phases.h TetherGoal) and lets the monster attack only from inside it (0.25 tiles of slack): one standing outside walks
// back in first. The M1 leash and Returning are unchanged (the walk home ignores the row's speedMul).
// Dark remake K5 (implementation-plan.md 3 K5; MonsterInstance "K5"):
//   Flee   (MonsterState::Flee, entered by MonsterSystem's morale): until fleeUntilMs the monster steps away from
//          fleeFrom (MonsterFleeStep); when no step increases that distance it fights instead (Chase, FleeEnd::Blocked);
//          when its time runs out it fights again (Chase, FleeEnd::Expired). No movement on either transition.
//   Kite   (def.kite): in Chase or Attack, with the hero visible and closer than kite.min, no strike of its own on the
//          way (MonsterAiInput::strikePending) and a strike since its last kite (MonsterInstance::lastStrikeMs: a swing
//          or a damaging ability; a support cast between swings does not count), it picks a kite target
//          (MonsterKiteTarget; none: it fights) and walks there in Chase (no movement on the tick it starts); the step
//          ends on arrival, when blocked (no movement) or at kiteUntilMs (the later of kiteTimeoutMs and twice the
//          straight walk at its top speed): Attack when the hero is in reach (and the tether allows), else Chase. A leash
//          ends it too.
//   Speed  the K5 moveSpeed buff / aura multiplies the top speed (MonsterInstance::buffMoveSpeedMul; also while
//          Returning: it is not a phase rule).
//
// Owner area: monsters. `UpdateMonsterAI` mutates one MonsterInstance; MonsterSystem runs it for the activity set in
// list order (3.8) with the safe-zone and immobilized rules applied before the call. RNG = RngStream::Ai
// (patrol: 2 draws, col first).
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Rng.h"
#include "abyss/base/Types.h"
#include "abyss/data/MonsterData.h"
#include "abyss/monsters/Monster.h"

namespace abyss {

struct MonsterInstance;

// Walkability + optional path service for M6 chasing.
struct MonsterWorld {
  int32_t cols = 0, rows = 0;
  std::function<bool(int32_t col, int32_t row)> walkable;
  // A* from -> to on the zone grid (world/Pathfinding). Empty result = unreachable. May be null (no A*).
  std::function<bool(Vec2 from, Vec2 to, std::vector<TilePos>& out)> findPath;
  // K5: `p` lies in a camp's safe zone (MonsterSystem fills it from the zone). May be null (no camps). The flee and kite
  // steps never walk a monster into one: the safe-zone repel would drop it to Idle and, with the hero outside the camp
  // within its aggro range, the aggro scan would put it back in Chase on a no-movement tick, every tick (frozen).
  std::function<bool(Vec2 p)> safeZone;
};

struct MonsterAiInput {
  double nowMs = 0;
  double dtMs = 0;
  bool heroVisible = true;  // false = the hero is in a safe zone and this monster is not aggro (hero at -999)
  Vec2 heroPos;
  double speedMul = 1;      // StatusEffectSystem::SpeedMultiplier (slow); immobilized monsters are not updated
  // M6 separation: positions of nearby alive monsters (excluding this one), may be empty.
  std::span<const Vec2> neighbours;
  // K5: a swing or ability of this monster is on its way to contact (CombatSystem::HasPendingStrike): no kite starts.
  bool strikePending = false;
};

struct MonsterAiResult {
  bool stateChanged = false;
  MonsterState previous = MonsterState::Idle;
  bool moved = false;
  bool leashed = false;  // entered Returning / web leash this tick
  bool arrivedHome = false;
  // K5: a kite step started / ended this tick; a flee ended this tick (and why).
  bool kiteStarted = false;
  bool kiteEnded = false;
  bool fleeEnded = false;
  FleeEnd fleeEndReason = FleeEnd::Expired;
};

// One AI tick (3.2) with the port decisions selected by ai.leashMode (WebParity reproduces Q1 exactly for A/B tests).
ABYSS_API MonsterAiResult UpdateMonsterAI(MonsterInstance& m, const MonsterAiDef& ai, const MonsterAiInput& in,
                                          const MonsterWorld& world, Rng& rng);

// moveToward (3.4) in velocity form: v += (speed * 0.03 * speedMul * phase * buff - v) * 6 * dt_s; pos += dir * v * dt_s;
// a blocked step (rounded tile not walkable) slides along the free axis, dominant axis first (M6), instead of cancelling
// both. heading = the intended direction even when blocked. Returns true (no move) when already within arriveEpsilon.
// phase = the K4 row's phaseSpeedMul, except 1 while Returning (M1: the walk home is at normal speed); buff = the K5
// moveSpeed buff / aura (MonsterInstance::buffMoveSpeedMul).
ABYSS_API bool MonsterMoveToward(MonsterInstance& m, const MonsterAiDef& ai, Vec2 target, double dtMs, double speedMul,
                                 const MonsterWorld& world);

// M2 provoke (binding contract; MonsterSystem::ApplyDamage calls it for hero hits, DamageFlags::provokes):
//   provokes  = state == Idle || state == Patrol || (state == Chase && heroDist > def.aggroRange)
//   effect    -> state Chase, provokedUntilMs = nowMs + ai.provokeDurationMs (5000), refreshed by every provoking hit;
//                while nowMs < provokedUntilMs the chase ignores the chaseDropMul (1.5 x aggro) drop to Idle.
//   Returning (M1 leash) IGNORES provokes: the monster walks home and heals first ("ignore the hero until home").
//   Attack and Dead are unchanged. Returns true when the monster was provoked (the caller publishes MonsterAggroMsg when
//   the state changed from Idle / Patrol).
ABYSS_API bool ProvokeMonster(MonsterInstance& m, const MonsterAiDef& ai, double nowMs, double heroDist);

// Ground speed in tiles/s at full speed (def.speed * 0.03).
ABYSS_API double MonsterTopSpeed(const MonsterDef& def, const MonsterAiDef& ai);

// K5 kite target (header "Kite"): the first point, from kite.max tiles from the hero down to heroDist + kiteSampleTiles
// (every kiteSampleTiles; at each distance along the away vector - monster - hero, else the reverse heading, else +col -
// then that vector turned 30 and 60 degrees each way about the hero, in that order) whose tile is walkable, that lies
// within the M1 leash range of the spawn anchor and within the tether (K4), outside every camp's safe zone
// (MonsterWorld::safeZone), and that the monster reaches in a straight walkable line (M6 line of walk) that stays out of
// the safe zones. So it keeps the band's far edge, circling the hero when straight back is closed. False = no such point.
ABYSS_API bool MonsterKiteTarget(const MonsterInstance& m, const MonsterAiDef& ai, Vec2 heroPos, double heroDist,
                                 const MonsterWorld& world, Vec2& out);
// K5 flee step (header "Flee"): the first direction among the away vector (monster - fleeFrom; else monster - hero when
// `heroVisible`, else the reverse heading, else +col) and it turned 45 and 90 degrees each way (that order) whose point
// fleeProbeTiles ahead lies on a walkable tile in a straight walkable line (M6 line of walk), within the M1 leash range
// and the tether, outside every camp's safe zone (point and line, MonsterWorld::safeZone), and at least fleeMinGainTiles
// farther from fleeFrom than the monster stands (a sidestep that only circles the body does not count), skipping a
// direction turned more than 120 degrees from its last flee step (MonsterInstance::fleeDir: no dithering between the two
// sidesteps at a curved edge). False = none (it fights instead); so is a step that does not move it.
ABYSS_API bool MonsterFleeStep(const MonsterInstance& m, const MonsterAiDef& ai, Vec2 heroPos, bool heroVisible,
                               const MonsterWorld& world, Vec2& dir);

}  // namespace abyss
