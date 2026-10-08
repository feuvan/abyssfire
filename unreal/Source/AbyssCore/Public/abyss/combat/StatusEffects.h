// Status effects: burn / freeze / poison / bleed / slow / stun on the hero and on monsters.
// Spec: classes-stats-skills.md section 13 (model, apply, tick, queries, rules), combat-feel.md section 7;
// DECISIONS C1 (FIX Q21: DoT ticks keep the remainder, total ticks capped), C5 (hero freeze/stun/slow affect
// movement and attacks), C6 (explicit StatusRule data instead of id substrings), D13 T6 (sim clock, no burst).
//
// Owner area: hero+combat. Pure state keyed by EntityId (the hero is kHeroEntityId). One instance per GameSim; GameSim
// calls ClearAll() on zone unload. The system never deals damage itself: Tick() returns the ticks and the callers
// (CombatSystem for the hero, MonsterSystem::ApplyDamage for monsters) apply them in the returned order.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Rng.h"
#include "abyss/base/Types.h"
#include "abyss/data/SkillData.h"

namespace abyss {

struct StatusEffect {
  StatusType type = StatusType::Burn;
  double value = 0;            // DoT damage per tick / slow percent / 1 for freeze+stun
  double durationMs = 0;
  int32_t tickIntervalMs = 0;  // 0 = non-ticking
  double startMs = 0;
  double lastTickMs = 0;       // FIX Q21: advanced by n * interval, never snapped to now
  int32_t ticksApplied = 0;    // FIX Q21: capped at floor(duration / interval)
  EntityId source = kNoEntity;

  bool ExpiredAt(double nowMs) const { return nowMs - startMs >= durationMs; }
  int32_t MaxTicks() const { return tickIntervalMs > 0 ? static_cast<int32_t>(durationMs / tickIntervalMs) : 0; }
};

// One DoT tick batch of one effect (`ticks` ticks of `damagePerTick`, already capped).
struct StatusTick {
  EntityId target = kNoEntity;
  StatusType type = StatusType::Burn;
  double damagePerTick = 0;
  int32_t ticks = 0;
  EntityId source = kNoEntity;
  double Total() const { return damagePerTick * ticks; }
};

struct StatusExpiry {
  EntityId target = kNoEntity;
  StatusType type = StatusType::Burn;
};

enum class StatusApplyOutcome : uint8_t { Rejected, Blocked, Applied, Refreshed };

struct StatusApplyResult {
  StatusApplyOutcome outcome = StatusApplyOutcome::Rejected;
  double effectiveDurationMs = 0;  // 0 when Rejected (value/duration <= 0) or Blocked (diminishing returns)
};

class ABYSS_API StatusEffectSystem {
 public:
  explicit StatusEffectSystem(const StatusEffectRules& rules);

  // apply (classes 13.2): rejects duration <= 0 || value <= 0; freeze/stun go through diminishing returns (keyed
  // (target, type)); poison/slow refresh (start = now, duration replaced, value = max, source updated; lastTick kept);
  // freeze/stun replace; burn/bleed stack. Logging is the caller's job (sys.statusEffect.applied / refreshed).
  StatusApplyResult Apply(EntityId target, StatusType type, double value, double durationMs, EntityId source,
                          double nowMs);

  // Tick then expire every tracked entity, in tracking order (first application order), effects in list order.
  // FIX Q21: n = floor((now - lastTick) / interval) limited to MaxTicks() - ticksApplied; lastTick += n * interval.
  // Ticks of an effect are reported before its expiry in the same call.
  void Tick(double nowMs, std::vector<StatusTick>& outTicks, std::vector<StatusExpiry>& outExpired);

  bool Has(EntityId target, StatusType type) const;
  // freeze or stun.
  bool IsImmobilized(EntityId target) const;
  // 0 if immobilized; else max(slowMinSpeedFactor, 1 - clamp(slow, 0, 100) / 100); 1 without slow.
  double SpeedMultiplier(EntityId target) const;
  std::span<const StatusEffect> EffectsOf(EntityId target) const;
  // Bit (1 << StatusType) per active type (Snapshot / render tints).
  uint32_t StatusMask(EntityId target) const;

  // Removes every effect of one type from the target (I4: the antidote cleanses poison), keeping the diminishing-returns
  // records. Returns whether anything was removed (the caller emits EvStatusExpired for it).
  bool Remove(EntityId target, StatusType type);
  // Death of an entity: removes its effects and its diminishing-returns records.
  void ClearEntity(EntityId target);
  // Zone unload.
  void ClearAll();

  const StatusEffectRules& Rules() const { return *rules_; }

 private:
  struct EntityEffects {
    EntityId id = kNoEntity;
    std::vector<StatusEffect> effects;
  };
  struct DiminishRecord {
    EntityId id = kNoEntity;
    StatusType type = StatusType::Freeze;
    int32_t applyCount = 0;
    double lastApplyMs = 0;
    double immuneUntilMs = 0;
  };
  EntityEffects* FindEntity(EntityId id);
  const EntityEffects* FindEntity(EntityId id) const;
  // Returns the diminished duration (0 = blocked), classes 13.2 diminish().
  double Diminish(EntityId target, StatusType type, double durationMs, double nowMs);

  const StatusEffectRules* rules_;
  std::vector<EntityEffects> entities_;  // tracking order
  std::vector<DiminishRecord> diminish_;
};

// Inputs to evaluate one StatusRule (C6) after a hit.
struct StatusRuleInput {
  double dealtDamage = 0;          // final damage of the hit (after combustion)       -> StatusValueKind::Damage
  double buffValue = 0;            // skill buffValue(L)                                -> StatusValueKind::BuffPercent
  double buffDurationMs = 0;       // skill buffDuration(L) (rules without a duration)
  double monsterDamage = 0;        // spawn-scaled def.damage                            -> StatusValueKind::MonsterDamage
};

struct StatusRuleRoll {
  bool applies = false;
  StatusType status = StatusType::Burn;
  double value = 0;
  double durationMs = 0;
};

// Evaluates one rule: a chance rule draws exactly one rng.Float01() (`< chance` applies); a no-chance rule draws
// nothing. Value per StatusValueKind (classes 13.5/13.6). Callers evaluate a skill's / monster's rules in array order
// and skip all of them for a dodged hit (C1 / FIX Q9) or a dead target.
ABYSS_API StatusRuleRoll RollStatusRule(const StatusRule& rule, const StatusRuleInput& in, Rng& rng);

}  // namespace abyss
