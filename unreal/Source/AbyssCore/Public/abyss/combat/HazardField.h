// Ground hazards of monster abilities (dark remake K2, implementation-plan.md 3 K2): the per-zone HazardField, ticked on
// the sim clock. A pure container: CombatSystem adds an entry at an ability's contact (EvHazard), collects the due ticks
// every step (TickHazards, after the status effects) and applies them; a zone change clears the field.
//
// Tick schedule (tick k counted from 0, start = placement):
//   pulseAtStart (an ability of kind `hazard`: the ground itself is the attack, its first pulse lands at contact):
//     at start + k x tickMs for every k with k x tickMs < durationMs;
//   otherwise (a hazard a strike leaves behind, after the strike's own hit):
//     at start + (k + 1) x tickMs for every k with (k + 1) x tickMs <= durationMs.
// The entry ends at untilMs = start + durationMs, once every tick is done. Ticks of one step resolve in (time, id)
// order.
//
// Owner area: hero+combat. No RNG (status chances are rolled by CombatSystem on RngStream::Combat).
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/data/MonsterData.h"
#include "abyss/data/SkillData.h"

namespace abyss {

// Ticks of one hazard at most (TickCount saturates there; the loader's bounds, 60 s at >= 100 ms, give <= 600).
inline constexpr int32_t kHazardMaxTicks = 1000;

struct HazardEntry {
  uint32_t id = 0;
  EntityId source = kNoEntity;  // the caster (it may be dead or gone by a tick)
  std::string abilityId;
  AbilityShapeDef shape;
  Vec2 origin;
  Vec2 dir{1, 0};
  double startMs = 0;
  double untilMs = 0;
  double tickMs = 0;
  bool pulseAtStart = false;
  int32_t ticksTotal = 0;
  int32_t ticksDone = 0;
  double heroTickDamage = 0;     // per tick, before the hero's resistance and the caster's hit cap (HazardTickDamage)
  bool hitsMonsters = false;
  double monsterTickDamage = 0;  // per tick on each monster inside (hitsMonsters)
  DamageType damageType = DamageType::Physical;
  bool hasStatus = false;
  StatusRule status;             // rolled per tick on the hero inside
  double hitCapPct = 0;          // the caster's hitCapHeroMaxHpPct (copied: the caster may die)
  double extraCapPct = 0;        // K3: an on-death puddle's own cap (capHeroMaxHpPct); the smaller cap binds

  double TickAtMs(int32_t k) const { return startMs + (pulseAtStart ? k : k + 1) * tickMs; }
};

struct HazardTick {
  uint32_t hazard = 0;
  int32_t index = 0;  // 0-based tick index
  double atMs = 0;
};

class ABYSS_API HazardField {
 public:
  // Ticks of one hazard (header schedule); 0 for a non-positive duration or tick, at most kHazardMaxTicks.
  static int32_t TickCount(double durationMs, double tickMs, bool pulseAtStart);

  // Adds an entry placed at e.startMs lasting `durationMs`: sets its id, untilMs, ticksTotal and ticksDone (0) and
  // returns the id (ids count up for the whole session, so a zone change never reuses one).
  uint32_t Add(HazardEntry e, double durationMs);
  // Every tick due at or before nowMs, in (time, id, index) order, marked done (the entries stay until DropExpired, so
  // the caller can read them while it applies the ticks).
  void CollectDue(double nowMs, std::vector<HazardTick>& out);
  // Drops the entries at or past untilMs whose ticks are all done.
  void DropExpired(double nowMs);
  void Clear() { entries_.clear(); }

  const HazardEntry* Find(uint32_t id) const;
  std::span<const HazardEntry> Entries() const { return entries_; }

 private:
  std::vector<HazardEntry> entries_;  // add order
  uint32_t nextId_ = 1;
};

}  // namespace abyss
