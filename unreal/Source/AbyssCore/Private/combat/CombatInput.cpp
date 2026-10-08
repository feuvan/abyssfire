// Input buffer, dodge controller, target cycling (combat-feel.md 6.1, 8.1, 9.3; C8). STUB: owner area hero+combat.
#include "abyss/base/Platform.h"

#include "abyss/combat/CombatInput.h"

#include "abyss/base/Assert.h"

namespace abyss {

InputBuffer::RequestResult InputBuffer::Request(int32_t skillIndex, double nowMs, bool executableNow) {
  ABYSS_UNIMPLEMENTED();
  (void)windowMs_;
  return executableNow ? RequestResult::ExecuteNow : RequestResult::Buffered;
}

int32_t InputBuffer::ConsumeReady(double nowMs, const std::function<bool(int32_t)>& canExecute) {
  ABYSS_UNIMPLEMENTED();
  return -1;
}

void DodgeController::Start(double nowMs) {
  ABYSS_UNIMPLEMENTED();
  (void)lastStartMs_;
  (void)rewardAvailable_;
}

bool DodgeController::ClaimAvoidanceReward() {
  ABYSS_UNIMPLEMENTED();
  return false;
}

double DodgeController::CooldownRemainingMs(double nowMs) const {
  return cooldownEndsAtMs_ > nowMs ? cooldownEndsAtMs_ - nowMs : 0.0;
}

double DodgeController::CooldownProgress(double nowMs) const {
  ABYSS_UNIMPLEMENTED();
  return 1.0;
}

void DodgeController::Reset() {
  lastStartMs_ = -1e300;
  cooldownEndsAtMs_ = 0;
  invulnerableUntilMs_ = 0;
  rewardAvailable_ = false;
}

bool ComputeDodgeDestination(const CombatInputDef& def, ClassId cls, Vec2 heroPos, Vec2 requestedDir, Vec2 facing,
                             const std::function<bool(int32_t col, int32_t row)>& walkable, Vec2& outDest) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

EntityId CycleTarget(std::span<const TargetCandidate> monsters, Vec2 heroPos, double rangeTiles, EntityId current) {
  ABYSS_UNIMPLEMENTED();
  return kNoEntity;
}

}  // namespace abyss
