// Per-tick monster AI (pure): state machine, leash, patrol, movement.
// Spec: monsters-ai.md 3.1-3.7 (states, exact per-tick algorithm, transition table, moveToward, leash, patrol);
// DECISIONS M1 (Returning leash: walk home at normal speed, heal 0.6 maxHp/s, ignore the hero), M2 (provoked: 5 s forced
// chase that ignores the 1.5x aggro drop), M6 (A* when line-of-walk is blocked + separation steering + wall sliding),
// M10 (4000 ms patrol timeout, 8 placement tries), S1 (fixed 60 Hz dt).
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
};

struct MonsterAiInput {
  double nowMs = 0;
  double dtMs = 0;
  bool heroVisible = true;  // false = the hero is in a safe zone and this monster is not aggro (hero at -999)
  Vec2 heroPos;
  double speedMul = 1;      // StatusEffectSystem::SpeedMultiplier (slow); immobilized monsters are not updated
  // M6 separation: positions of nearby alive monsters (excluding this one), may be empty.
  std::span<const Vec2> neighbours;
};

struct MonsterAiResult {
  bool stateChanged = false;
  MonsterState previous = MonsterState::Idle;
  bool moved = false;
  bool leashed = false;  // entered Returning / web leash this tick
  bool arrivedHome = false;
};

// One AI tick (3.2) with the port decisions selected by ai.leashMode (WebParity reproduces Q1 exactly for A/B tests).
ABYSS_API MonsterAiResult UpdateMonsterAI(MonsterInstance& m, const MonsterAiDef& ai, const MonsterAiInput& in,
                                          const MonsterWorld& world, Rng& rng);

// moveToward (3.4) in velocity form: v += (speed * 0.03 * speedMul - v) * 6 * dt_s; pos += dir * v * dt_s; a blocked
// step (rounded tile not walkable) slides along the free axis, dominant axis first (M6), instead of cancelling both.
// heading = the intended direction even when blocked. Returns true (no move) when already within arriveEpsilon.
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

}  // namespace abyss
