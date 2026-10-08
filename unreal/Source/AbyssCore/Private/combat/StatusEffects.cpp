// Status effects (classes-stats-skills.md section 13; FIX Q21). STUB: owner area hero+combat. Storage and queries
// are real; Apply / Tick / RollStatusRule are stubs.
#include "abyss/base/Platform.h"

#include "abyss/combat/StatusEffects.h"

#include <algorithm>

#include "abyss/base/Assert.h"

namespace abyss {

StatusEffectSystem::StatusEffectSystem(const StatusEffectRules& rules) : rules_(&rules) {}

StatusApplyResult StatusEffectSystem::Apply(EntityId target, StatusType type, double value, double durationMs,
                                            EntityId source, double nowMs) {
  ABYSS_UNIMPLEMENTED();
  (void)diminish_;
  return {};
}

void StatusEffectSystem::Tick(double nowMs, std::vector<StatusTick>& outTicks, std::vector<StatusExpiry>& outExpired) {
  ABYSS_UNIMPLEMENTED();
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

double StatusEffectSystem::Diminish(EntityId target, StatusType type, double durationMs, double nowMs) {
  ABYSS_UNIMPLEMENTED();
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
  ABYSS_UNIMPLEMENTED();
  return IsImmobilized(target) ? 0.0 : 1.0;
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
  return e->effects.size() != before;
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
  ABYSS_UNIMPLEMENTED();
  return {};
}

}  // namespace abyss
