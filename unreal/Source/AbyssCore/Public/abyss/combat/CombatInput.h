// Skill input buffer, dodge roll controller and target cycling.
// Spec: combat-feel.md sections 6.1 (input buffer), 8.1 (dodge), 9.3 (target cycling); classes-stats-skills.md 9.1,
// 15 (per-class dodge distance); DECISIONS C8 (dodge falls back to the facing direction, never screen-right),
// D13 F1 (buffer cleared at freeze begin), T5 (sim clock).
//
// Owner area: hero+combat. Pure state machines on SimClock ms.
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/data/CombatData.h"

namespace abyss {

// CombatInputBuffer (6.1): one slot; the newest request replaces an older one; expires after inputBufferMs.
class ABYSS_API InputBuffer {
 public:
  explicit InputBuffer(double windowMs = 180) : windowMs_(windowMs) {}

  enum class RequestResult : uint8_t { ExecuteNow, Buffered };
  // executableNow -> ExecuteNow (nothing stored); else stores {skill, now + window} and returns Buffered.
  RequestResult Request(int32_t skillIndex, double nowMs, bool executableNow);
  // consumeReady: drops an expired request (now > expiresAt); if canExecute(skill) -> clears and returns the skill.
  // Returns -1 when nothing fires.
  int32_t ConsumeReady(double nowMs, const std::function<bool(int32_t)>& canExecute);
  void Clear() { has_ = false; }
  bool HasPending() const { return has_; }
  int32_t PendingSkill() const { return has_ ? skill_ : -1; }
  double ExpiresAtMs() const { return expiresAt_; }

 private:
  double windowMs_;
  bool has_ = false;
  int32_t skill_ = -1;
  double expiresAt_ = 0;
};

// DodgeController (8.1): cooldown 900, i-frames 220, one Spirit 'dodge' per i-frame window.
class ABYSS_API DodgeController {
 public:
  explicit DodgeController(const CombatInputDef& def) : def_(&def) {}

  bool CanStart(double nowMs) const { return nowMs >= cooldownEndsAtMs_; }
  // tryStart: cooldownEnds = now + cooldown, invulnerableUntil = now + iframes, avoidance reward re-armed.
  void Start(double nowMs);
  bool IsInvulnerable(double nowMs) const { return nowMs < invulnerableUntilMs_; }
  // claimAvoidanceReward: true once per window (the first hit avoided by i-frames grants Spirit 'dodge').
  bool ClaimAvoidanceReward();
  double CooldownRemainingMs(double nowMs) const;
  double CooldownProgress(double nowMs) const;  // clamp((now - lastStart) / cooldown, 0, 1)
  double CooldownMs() const { return def_->dodgeCooldownMs; }
  double InvulnerabilityMs() const { return def_->dodgeInvulnerabilityMs; }
  void Reset();

 private:
  const CombatInputDef* def_;
  double lastStartMs_ = -1e300;
  double cooldownEndsAtMs_ = 0;
  double invulnerableUntilMs_ = 0;
  bool rewardAvailable_ = false;
};

// Dodge destination (8.1 + C8): direction = requested (stick/joystick) if longer than 0.001, else the hero facing.
// Normalised. Tries d = maxDist, maxDist - step, ... >= minDist; the first whose ROUNDED landing tile is in bounds and
// walkable wins (no line check). Returns false when none (no dodge, cooldown not spent).
ABYSS_API bool ComputeDodgeDestination(const CombatInputDef& def, ClassId cls, Vec2 heroPos, Vec2 requestedDir, Vec2 facing,
                                       const std::function<bool(int32_t col, int32_t row)>& walkable, Vec2& outDest);

// cycleTargetId (9.3): candidates alive within range (dist^2 <= range^2), sorted by dist^2 then id; returns the entry
// after `current` (wrap; first when current is not in the list), kNoEntity when none.
struct TargetCandidate {
  EntityId id = kNoEntity;
  Vec2 pos;
  bool alive = true;
};
ABYSS_API EntityId CycleTarget(std::span<const TargetCandidate> monsters, Vec2 heroPos, double rangeTiles, EntityId current);

}  // namespace abyss
