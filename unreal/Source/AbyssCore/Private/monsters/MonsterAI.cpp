// Per-tick monster AI (monsters-ai.md 3.1-3.7; M1, M2, M6, M10). STUB: owner area monsters. ProvokeMonster (M2) and
// MonsterTopSpeed are implemented.
#include "abyss/base/Platform.h"

#include "abyss/monsters/MonsterAI.h"

#include "abyss/base/Assert.h"

namespace abyss {

MonsterAiResult UpdateMonsterAI(MonsterInstance& m, const MonsterAiDef& ai, const MonsterAiInput& in,
                                const MonsterWorld& world, Rng& rng) {
  ABYSS_UNIMPLEMENTED();
  MonsterAiResult r;
  r.previous = m.state;
  return r;
}

bool MonsterMoveToward(MonsterInstance& m, const MonsterAiDef& ai, Vec2 target, double dtMs, double speedMul,
                       const MonsterWorld& world) {
  ABYSS_UNIMPLEMENTED();
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
