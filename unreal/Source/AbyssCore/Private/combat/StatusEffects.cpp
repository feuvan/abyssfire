// Status effects (classes-stats-skills.md section 13; combat-feel.md section 7; FIX Q21).
#include "abyss/base/Platform.h"

#include "abyss/combat/StatusEffects.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/Assert.h"
#include "abyss/base/Math.h"

namespace abyss {

namespace {
// Tolerance for the DoT tick boundary: the sim clock is steps * 1000 / 60 while lastTick advances by whole intervals,
// so the two can differ by an ulp at the exact boundary step. Without it a tick due at the expiry step could slip past
// the expiry check (ticks run before expiry) and be lost, which is exactly what FIX Q21 removes.
constexpr double kStatusTickEpsilonMs = 1e-6;
}  // namespace

StatusEffectSystem::StatusEffectSystem(const StatusEffectRules& rules) : rules_(&rules) {}

StatusApplyResult StatusEffectSystem::Apply(EntityId target, StatusType type, double value, double durationMs,
                                            EntityId source, double nowMs) {
  StatusApplyResult res;
  if (!(durationMs > 0) || !(value > 0)) return res;  // Rejected (also NaN)

  double duration = durationMs;
  if (rules_->Diminishes(type)) {
    duration = Diminish(target, type, durationMs, nowMs);
    if (!(duration > 0)) {
      res.outcome = StatusApplyOutcome::Blocked;
      return res;
    }
  }

  EntityEffects* ent = FindEntity(target);
  const StatusStacking stacking = rules_->Stacking(type);
  if (stacking == StatusStacking::RefreshKeepStronger && ent != nullptr) {
    for (StatusEffect& e : ent->effects) {
      if (e.type != type) continue;
      // Refresh: start and duration replaced, the stronger value kept, source updated, lastTick kept. The Q21 tick cap
      // restarts with the new window (ticks remain on the lastTick + n * interval grid).
      e.startMs = nowMs;
      e.durationMs = duration;
      e.value = (std::max)(e.value, value);
      e.source = source;
      e.ticksApplied = 0;
      res.outcome = StatusApplyOutcome::Refreshed;
      res.effectiveDurationMs = duration;
      return res;
    }
  }
  if (stacking == StatusStacking::Replace && ent != nullptr) {
    for (size_t i = 0; i < ent->effects.size(); ++i) {
      if (ent->effects[i].type == type) {
        ent->effects.erase(ent->effects.begin() + static_cast<std::ptrdiff_t>(i));
        break;  // only one instance exists
      }
    }
  }

  if (ent == nullptr) {
    entities_.push_back(EntityEffects{target, {}});
    ent = &entities_.back();
  }
  StatusEffect e;
  e.type = type;
  e.value = value;
  e.durationMs = duration;
  e.tickIntervalMs = rules_->TickInterval(type);
  e.startMs = nowMs;
  e.lastTickMs = nowMs;
  e.ticksApplied = 0;
  e.source = source;
  ent->effects.push_back(e);
  res.outcome = StatusApplyOutcome::Applied;
  res.effectiveDurationMs = duration;
  return res;
}

void StatusEffectSystem::Tick(double nowMs, std::vector<StatusTick>& outTicks, std::vector<StatusExpiry>& outExpired) {
  for (EntityEffects& ent : entities_) {
    // Ticks (before expiry, FIX Q21: remainder kept, total capped).
    for (StatusEffect& e : ent.effects) {
      if (e.tickIntervalMs <= 0) continue;
      const double interval = static_cast<double>(e.tickIntervalMs);
      const double elapsed = nowMs - e.lastTickMs;
      if (elapsed + kStatusTickEpsilonMs < interval) continue;
      int32_t n = FloorInt((elapsed + kStatusTickEpsilonMs) / interval);
      const int32_t left = (std::max)(0, e.MaxTicks() - e.ticksApplied);
      n = (std::min)(n, left);
      if (n <= 0) continue;
      e.lastTickMs += n * interval;
      e.ticksApplied += n;
      outTicks.push_back(StatusTick{ent.id, e.type, e.value, n, e.source});
    }
    // Expiry.
    for (size_t i = 0; i < ent.effects.size();) {
      if (ent.effects[i].ExpiredAt(nowMs)) {
        outExpired.push_back(StatusExpiry{ent.id, ent.effects[i].type});
        ent.effects.erase(ent.effects.begin() + static_cast<std::ptrdiff_t>(i));
      } else {
        ++i;
      }
    }
  }
  // An entity whose list became empty leaves the tracking order (web Map.delete): a later application re-enters it at
  // the end.
  entities_.erase(std::remove_if(entities_.begin(), entities_.end(),
                                 [](const EntityEffects& e) { return e.effects.empty(); }),
                  entities_.end());
}

StatusEffectSystem::EntityEffects* StatusEffectSystem::FindEntity(EntityId id) {
  for (EntityEffects& e : entities_) {
    if (e.id == id) return &e;
  }
  return nullptr;
}

const StatusEffectSystem::EntityEffects* StatusEffectSystem::FindEntity(EntityId id) const {
  for (const EntityEffects& e : entities_) {
    if (e.id == id) return &e;
  }
  return nullptr;
}

// diminish() (classes 13.2): blocked attempts during immunity do not touch the record.
double StatusEffectSystem::Diminish(EntityId target, StatusType type, double durationMs, double nowMs) {
  DiminishRecord* rec = nullptr;
  for (DiminishRecord& d : diminish_) {
    if (d.id == target && d.type == type) {
      rec = &d;
      break;
    }
  }
  if (rec == nullptr) {
    diminish_.push_back(DiminishRecord{target, type, 0, 0, 0});
    rec = &diminish_.back();
  }
  if (nowMs < rec->immuneUntilMs) return 0;
  if (nowMs - rec->lastApplyMs > rules_->diminishingWindowMs) rec->applyCount = 0;
  rec->applyCount += 1;
  rec->lastApplyMs = nowMs;
  if (rec->applyCount == 2) {
    const double d = std::floor(durationMs * rules_->diminishingFactor);
    rec->immuneUntilMs = nowMs + d + rules_->diminishingImmunityMs;
    return d;
  }
  if (rec->applyCount > 2) {
    rec->immuneUntilMs = nowMs + rules_->diminishingImmunityMs;
    return 0;
  }
  return durationMs;
}

bool StatusEffectSystem::Has(EntityId target, StatusType type) const {
  const EntityEffects* e = FindEntity(target);
  if (e == nullptr) return false;
  for (const StatusEffect& s : e->effects) {
    if (s.type == type) return true;
  }
  return false;
}

bool StatusEffectSystem::IsImmobilized(EntityId target) const {
  return Has(target, StatusType::Freeze) || Has(target, StatusType::Stun);
}

double StatusEffectSystem::SpeedMultiplier(EntityId target) const {
  const EntityEffects* e = FindEntity(target);
  if (e == nullptr) return 1.0;
  if (IsImmobilized(target)) return 0.0;
  for (const StatusEffect& s : e->effects) {
    if (s.type != StatusType::Slow) continue;
    const double mul = 1.0 - Clamp(s.value, 0.0, 100.0) / 100.0;
    return (std::max)(rules_->slowMinSpeedFactor, mul);
  }
  return 1.0;
}

std::span<const StatusEffect> StatusEffectSystem::EffectsOf(EntityId target) const {
  const EntityEffects* e = FindEntity(target);
  if (e == nullptr) return {};
  return e->effects;
}

uint32_t StatusEffectSystem::StatusMask(EntityId target) const {
  uint32_t mask = 0;
  for (const StatusEffect& s : EffectsOf(target)) mask |= 1u << static_cast<uint32_t>(s.type);
  return mask;
}

bool StatusEffectSystem::Remove(EntityId target, StatusType type) {
  EntityEffects* e = FindEntity(target);
  if (e == nullptr) return false;
  const size_t before = e->effects.size();
  e->effects.erase(std::remove_if(e->effects.begin(), e->effects.end(),
                                  [type](const StatusEffect& s) { return s.type == type; }),
                   e->effects.end());
  const bool removed = e->effects.size() != before;
  if (e->effects.empty()) {
    entities_.erase(std::remove_if(entities_.begin(), entities_.end(),
                                   [target](const EntityEffects& x) { return x.id == target; }),
                    entities_.end());
  }
  return removed;
}

void StatusEffectSystem::ClearEntity(EntityId target) {
  entities_.erase(std::remove_if(entities_.begin(), entities_.end(),
                                 [target](const EntityEffects& e) { return e.id == target; }),
                  entities_.end());
  diminish_.erase(std::remove_if(diminish_.begin(), diminish_.end(),
                                 [target](const DiminishRecord& d) { return d.id == target; }),
                  diminish_.end());
}

void StatusEffectSystem::ClearAll() {
  entities_.clear();
  diminish_.clear();
}

StatusRuleRoll RollStatusRule(const StatusRule& rule, const StatusRuleInput& in, Rng& rng) {
  StatusRuleRoll roll;
  roll.status = rule.status;
  if (rule.hasChance && !(rng.Float01() < rule.chance)) return roll;  // exactly one draw for a chance rule
  switch (rule.valueKind) {
    case StatusValueKind::Fixed:
      roll.value = rule.amount;
      break;
    case StatusValueKind::Damage:
      roll.value = (std::max)(rule.min, std::floor(in.dealtDamage * rule.fraction));
      break;
    case StatusValueKind::BuffPercent:
      roll.value = JsRound(in.buffValue * 100.0);
      break;
    case StatusValueKind::MonsterDamage:
      roll.value = (std::max)(rule.min, std::floor(in.monsterDamage * rule.fraction));
      break;
  }
  roll.durationMs = rule.hasDuration ? static_cast<double>(rule.durationMs) : in.buffDurationMs;
  roll.applies = true;
  return roll;
}

}  // namespace abyss
