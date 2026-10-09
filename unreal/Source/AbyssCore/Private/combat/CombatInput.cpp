// Input buffer, dodge controller, target cycling (combat-feel.md 6.1, 8.1, 9.3; C8).
#include "abyss/base/Platform.h"

#include "abyss/combat/CombatInput.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/Math.h"

namespace abyss {

InputBuffer::RequestResult InputBuffer::Request(int32_t skillIndex, double nowMs, bool executableNow) {
  if (executableNow) {
    has_ = false;  // an executable request replaces (clears) an older buffered one
    return RequestResult::ExecuteNow;
  }
  has_ = true;
  skill_ = skillIndex;
  expiresAt_ = nowMs + (std::max)(0.0, windowMs_);
  return RequestResult::Buffered;
}

int32_t InputBuffer::ConsumeReady(double nowMs, const std::function<bool(int32_t)>& canExecute) {
  if (!has_) return -1;
  if (nowMs > expiresAt_) {
    has_ = false;
    return -1;
  }
  if (!canExecute || !canExecute(skill_)) return -1;
  has_ = false;
  return skill_;
}

void DodgeController::Start(double nowMs) {
  lastStartMs_ = nowMs;
  cooldownEndsAtMs_ = nowMs + (std::max)(0.0, def_->dodgeCooldownMs);
  invulnerableUntilMs_ = nowMs + (std::max)(0.0, def_->dodgeInvulnerabilityMs);
  rewardAvailable_ = true;
}

bool DodgeController::ClaimAvoidanceReward(double nowMs) {
  if (!IsInvulnerable(nowMs) || !rewardAvailable_) return false;
  rewardAvailable_ = false;
  return true;
}

double DodgeController::CooldownRemainingMs(double nowMs) const {
  return cooldownEndsAtMs_ > nowMs ? cooldownEndsAtMs_ - nowMs : 0.0;
}

double DodgeController::CooldownProgress(double nowMs) const {
  const double cd = def_->dodgeCooldownMs;
  if (!(cd > 0) || lastStartMs_ <= -1e299) return 1.0;
  return Clamp((nowMs - lastStartMs_) / cd, 0.0, 1.0);
}

void DodgeController::Reset() {
  lastStartMs_ = -1e300;
  cooldownEndsAtMs_ = 0;
  invulnerableUntilMs_ = 0;
  rewardAvailable_ = false;
}

bool ComputeDodgeDestination(const CombatInputDef& def, ClassId cls, Vec2 heroPos, Vec2 requestedDir, Vec2 facing,
                             const std::function<bool(int32_t col, int32_t row)>& walkable, Vec2& outDest) {
  Vec2 dir = requestedDir;
  if (!(dir.Length() > 0.001)) dir = facing;  // C8: never the web's screen-right fallback
  if (!(dir.Length() > 0.001)) dir = Vec2(def.dodgeDefaultDirX, def.dodgeDefaultDirY);
  dir = dir.Normalized();
  if (!(dir.LengthSq() > 0)) return false;
  const double maxDist = def.dodgeDistanceTiles[static_cast<size_t>(cls)];
  const double step = def.dodgeStepTiles > 0 ? def.dodgeStepTiles : 0.25;
  for (double d = maxDist; d >= def.dodgeMinTiles; d -= step) {
    const Vec2 p = heroPos + dir * d;
    const TilePos t = RoundToTile(p);
    if (walkable && walkable(t.col, t.row)) {
      outDest = p;
      return true;
    }
  }
  return false;
}

EntityId CycleTarget(std::span<const TargetCandidate> monsters, Vec2 heroPos, double rangeTiles, EntityId current) {
  struct Entry {
    EntityId id;
    double distSq;
  };
  std::vector<Entry> valid;
  const double rangeSq = rangeTiles * rangeTiles;
  for (const TargetCandidate& c : monsters) {
    if (!c.alive) continue;
    const double d = DistSq(heroPos, c.pos);
    if (d <= rangeSq) valid.push_back({c.id, d});
  }
  if (valid.empty()) return kNoEntity;
  std::stable_sort(valid.begin(), valid.end(), [](const Entry& a, const Entry& b) {
    if (a.distSq != b.distSq) return a.distSq < b.distSq;
    return a.id < b.id;
  });
  for (size_t i = 0; i < valid.size(); ++i) {
    if (valid[i].id == current) return valid[(i + 1) % valid.size()].id;
  }
  return valid.front().id;
}

}  // namespace abyss
