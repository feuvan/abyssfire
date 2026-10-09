// Per-tick monster AI (monsters-ai.md 3.1-3.7; DECISIONS M1, M2, M6, M10). Owner area: monsters.
//
// UpdateMonsterAI follows the web's Monster.update exactly (distances measured once at the top of the tick, the
// transition order of 3.3, no same-tick move on a transition into chase / attack, the patrol RNG draws col then row)
// with the port decisions layered on:
//   M1  leash -> Returning (walk home with the normal ramp, heal leashHealFractionPerSecond * maxHp / s, ignore the
//       hero, idle on arrival or within returnHomeRadius). ai.leashMode == WebParity keeps the one-tick web leash (Q1).
//   M2  a provoked chase ignores the chaseDropMul drop until provokedUntilMs (ProvokeMonster).
//   M6  chase / return use A* waypoints while the straight line is blocked, a blocked step slides along the free axis,
//       and nearby monsters push apart (separation; attackers never leave their attack range).
//   M10 a patrol that has not arrived after patrolTimeoutMs gives up (Q13).
#include "abyss/base/Platform.h"

#include "abyss/monsters/MonsterAI.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/Math.h"

namespace abyss {

namespace {

// Tile walkability through the world service; out of bounds is blocked when the bounds are known, a world without a
// walkability callback is open ground (pure tests).
bool MonsterAiWalkable(const MonsterWorld& w, int32_t col, int32_t row) {
  if (w.cols > 0 && w.rows > 0 && (col < 0 || row < 0 || col >= w.cols || row >= w.rows)) return false;
  if (!w.walkable) return true;
  return w.walkable(col, row);
}

// M6 line-of-walk: every sample along the segment (step ai.lineOfWalkStep) lies on a walkable tile.
bool MonsterAiLineOfWalk(Vec2 from, Vec2 to, const MonsterWorld& w, double step) {
  const Vec2 d = to - from;
  const double len = d.Length();
  if (!(len > 0)) return true;
  const int32_t n = (std::max)(1, CeilInt(len / (step > 0 ? step : 0.25)));
  for (int32_t i = 1; i <= n; ++i) {
    const Vec2 p = from + d * (static_cast<double>(i) / static_cast<double>(n));
    if (!MonsterAiWalkable(w, JsRoundInt(p.x), JsRoundInt(p.y))) return false;
  }
  return true;
}

void MonsterAiSetIdle(MonsterInstance& m) {
  m.state = MonsterState::Idle;
  m.moveSpeed = 0;
  m.hasPatrolTarget = false;
  m.path.clear();
}

// Chase / return movement (M6): straight moveToward while the line is clear, else follow A* waypoints (tile centres)
// re-planned every repathIntervalMs. Returns moveToward's "arrived" for the final target.
bool MonsterAiNavigate(MonsterInstance& m, const MonsterAiDef& ai, Vec2 target, const MonsterAiInput& in,
                       const MonsterWorld& w) {
  if (w.findPath && !MonsterAiLineOfWalk(m.pos, target, w, ai.lineOfWalkStep)) {
    if (m.path.empty() || in.nowMs >= m.repathAtMs) {
      m.repathAtMs = in.nowMs + ai.repathIntervalMs;
      std::vector<TilePos> p;
      if (w.findPath(m.pos, target, p)) {
        m.path = std::move(p);
      } else {
        m.path.clear();
      }
    }
    while (!m.path.empty() && Dist(m.pos, m.path.front().Center()) < ai.arriveEpsilon) m.path.erase(m.path.begin());
    if (!m.path.empty()) {
      MonsterMoveToward(m, ai, m.path.front().Center(), in.dtMs, in.speedMul, w);
      return false;
    }
  } else {
    m.path.clear();
  }
  return MonsterMoveToward(m, ai, target, in.dtMs, in.speedMul, w);
}

// M6 separation steering: neighbours closer than separationRadius push this monster away (linear falloff), at most
// separationMaxSpeed tiles/s. Coincident monsters split along a per-id golden-angle direction (deterministic).
void MonsterAiSeparate(MonsterInstance& m, const MonsterAiDef& ai, const MonsterAiInput& in, const MonsterWorld& w) {
  const double radius = ai.separationRadius;
  if (in.neighbours.empty() || !(radius > 0) || !(ai.separationMaxSpeed > 0)) return;
  Vec2 push;
  for (const Vec2& n : in.neighbours) {
    const Vec2 d = m.pos - n;
    const double dist = d.Length();
    if (dist >= radius) continue;
    Vec2 dir;
    if (dist > 1e-9) {
      dir = d / dist;
    } else {
      const double angle = static_cast<double>(m.id) * 2.39996322972865332;
      dir = Vec2(std::cos(angle), std::sin(angle));
    }
    push += dir * ((radius - dist) / radius);
  }
  const double len = push.Length();
  if (!(len > 0)) return;
  const double maxStep = ai.separationMaxSpeed * in.dtMs / 1000.0;
  const Vec2 step = len > 1 ? push * (maxStep / len) : push * maxStep;
  const Vec2 next = m.pos + step;
  if (m.state == MonsterState::Attack && in.heroVisible && Dist(next, in.heroPos) > m.def.attackRange) return;
  if (!MonsterAiWalkable(w, JsRoundInt(next.x), JsRoundInt(next.y))) return;
  m.pos = next;
}

}  // namespace

MonsterAiResult UpdateMonsterAI(MonsterInstance& m, const MonsterAiDef& ai, const MonsterAiInput& in,
                                const MonsterWorld& world, Rng& rng) {
  MonsterAiResult r;
  r.previous = m.state;
  if (m.state == MonsterState::Dead) return r;
  const Vec2 start = m.pos;
  const Vec2 hero = in.heroVisible ? in.heroPos : Vec2(ai.hiddenHeroCoord, ai.hiddenHeroCoord);
  const Vec2 home = m.spawnAnchor.Center();
  // Distances are measured once, before any movement this tick (3.2).
  const double dP = Dist(m.pos, hero);
  const double dS = Dist(m.pos, home);

  bool webLeashTick = false;
  if (dS > ai.leashRange && m.state != MonsterState::Idle && m.state != MonsterState::Returning) {
    r.leashed = true;
    m.provokedUntilMs = 0;
    m.hasPatrolTarget = false;
    m.path.clear();
    if (ai.leashMode == LeashMode::WebParity) {
      // Q1 parity: idle, one smoothed step home from rest, heal 1 % maxHp, tick ends.
      m.state = MonsterState::Idle;
      m.moveSpeed = 0;
      MonsterMoveToward(m, ai, home, in.dtMs, in.speedMul, world);
      m.hp = (std::min)(m.maxHp, m.hp + m.maxHp * ai.leashHealFractionPerTick);
      webLeashTick = true;
    } else {
      m.state = MonsterState::Returning;  // M1: handled below in the same tick
    }
  }

  if (!webLeashTick) {
    switch (m.state) {
      case MonsterState::Idle: {
        m.patrolTimerMs += in.dtMs;
        if (dP <= m.def.aggroRange) {
          m.state = MonsterState::Chase;  // no movement this tick
        } else if (m.patrolTimerMs > ai.patrolIntervalMs) {
          m.patrolTimerMs = 0;
          const int32_t pc = m.spawnAnchor.col + rng.RandomInt(-ai.patrolRadius, ai.patrolRadius);  // col first
          const int32_t pr = m.spawnAnchor.row + rng.RandomInt(-ai.patrolRadius, ai.patrolRadius);
          if (MonsterAiWalkable(world, pc, pr)) {
            m.state = MonsterState::Patrol;
            m.hasPatrolTarget = true;
            m.patrolTarget = TilePos{pc, pr};
            m.patrolStartedMs = in.nowMs;
          }
        }
        break;
      }
      case MonsterState::Patrol: {
        if (dP <= m.def.aggroRange) {
          m.state = MonsterState::Chase;
          m.hasPatrolTarget = false;
        } else if (m.hasPatrolTarget) {
          if (ai.patrolTimeoutMs > 0 && in.nowMs - m.patrolStartedMs >= ai.patrolTimeoutMs) {
            MonsterAiSetIdle(m);  // M10: a blocked patrol target gives up (Q13)
          } else if (MonsterMoveToward(m, ai, m.patrolTarget.Center(), in.dtMs, in.speedMul, world)) {
            MonsterAiSetIdle(m);
          }
        } else {
          MonsterAiSetIdle(m);
        }
        break;
      }
      case MonsterState::Chase: {
        const bool provoked = in.nowMs < m.provokedUntilMs;  // M2
        if (!provoked && dP > m.def.aggroRange * ai.chaseDropMul) {
          MonsterAiSetIdle(m);
        } else if (dP <= m.def.attackRange) {
          m.state = MonsterState::Attack;  // no movement this tick
          m.path.clear();
        } else {
          MonsterAiNavigate(m, ai, hero, in, world);
        }
        break;
      }
      case MonsterState::Attack: {
        if (dP > m.def.attackRange * ai.attackExitMul) m.state = MonsterState::Chase;  // no movement this tick
        const Vec2 d = hero - m.pos;  // squared up to the hero every tick, even on the exit tick
        if (d.LengthSq() > 0) m.heading = d.Normalized();
        break;
      }
      case MonsterState::Returning: {
        // M1: heal over time, ignore the hero, walk home; idle on arrival or once within returnHomeRadius.
        m.hp = (std::min)(m.maxHp, m.hp + m.maxHp * ai.leashHealFractionPerSecond * (in.dtMs / 1000.0));
        if (dS <= ai.returnHomeRadius || MonsterAiNavigate(m, ai, home, in, world)) {
          MonsterAiSetIdle(m);
          r.arrivedHome = true;
        }
        break;
      }
      case MonsterState::Dead:
        break;
    }
    MonsterAiSeparate(m, ai, in, world);
  }

  r.stateChanged = m.state != r.previous;
  r.moved = m.pos != start;
  return r;
}

bool MonsterMoveToward(MonsterInstance& m, const MonsterAiDef& ai, Vec2 target, double dtMs, double speedMul,
                       const MonsterWorld& world) {
  const double dx = target.x - m.pos.x;
  const double dy = target.y - m.pos.y;
  const double d = std::sqrt(dx * dx + dy * dy);
  if (d < ai.arriveEpsilon) return true;  // arrived: no snap, no move
  const double dtS = dtMs / 1000.0;
  const double top = m.def.speed * ai.moveSpeedScale * speedMul;
  m.moveSpeed += (top - m.moveSpeed) * ai.moveAccel * dtS;
  const double nx = dx / d;
  const double ny = dy / d;
  const double step = m.moveSpeed * dtS;
  const double newCol = m.pos.x + nx * step;
  const double newRow = m.pos.y + ny * step;
  if (MonsterAiWalkable(world, JsRoundInt(newCol), JsRoundInt(newRow))) {
    m.pos = Vec2(newCol, newRow);
  } else {
    // M6 wall sliding: keep the component along the free axis (dominant axis first) instead of cancelling both.
    const bool colFirst = std::fabs(nx) >= std::fabs(ny);
    const Vec2 alongCol(newCol, m.pos.y);
    const Vec2 alongRow(m.pos.x, newRow);
    const Vec2 first = colFirst ? alongCol : alongRow;
    const Vec2 second = colFirst ? alongRow : alongCol;
    if (first != m.pos && MonsterAiWalkable(world, JsRoundInt(first.x), JsRoundInt(first.y))) {
      m.pos = first;
    } else if (second != m.pos && MonsterAiWalkable(world, JsRoundInt(second.x), JsRoundInt(second.y))) {
      m.pos = second;
    }
  }
  m.heading = Vec2(nx, ny);  // the intended heading, even when blocked
  return false;
}

bool ProvokeMonster(MonsterInstance& m, const MonsterAiDef& ai, double nowMs, double heroDist) {
  const bool provokes = m.state == MonsterState::Idle || m.state == MonsterState::Patrol ||
                        (m.state == MonsterState::Chase && heroDist > m.def.aggroRange);
  if (!provokes || !m.IsAlive()) return false;  // Returning ignores provokes (M1); Attack / Dead unchanged
  m.state = MonsterState::Chase;
  m.hasPatrolTarget = false;
  m.provokedUntilMs = nowMs + ai.provokeDurationMs;
  return true;
}

double MonsterTopSpeed(const MonsterDef& def, const MonsterAiDef& ai) { return def.speed * ai.moveSpeedScale; }

}  // namespace abyss
