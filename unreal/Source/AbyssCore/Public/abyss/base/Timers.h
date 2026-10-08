// Sim-clock timer queue (classes-stats-skills.md 19.1 step algorithm: "timers.DrainDue(now) - ascending due time;
// equal times in scheduling order").
//
// One queue per GameSim holds every delayed gameplay action that the web did with time.delayedCall: strike contacts,
// skill releases, projectile arrivals, AoE delays, respawns, despawns, the death respawn, town-portal channel, story
// trigger delays (T1..T21). Timers are typed by owner + kind with a small inline payload; an owner that needs a richer
// payload keeps a side table keyed by the timer id. GameSim drains due timers at the start of each unfrozen step and
// routes each one to its owner. Freeze-begin rule F2 uses CancelIf to drop monster strikes/projectiles.
// Timers are not saved (nothing pending survives a save/load; zone unload clears the zone-owned ones).
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"

namespace abyss {

// Which subsystem a timer belongs to (GameSim dispatches on it).
enum class TimerOwner : uint8_t { Sim, Hero, Combat, Projectiles, Monsters, Items, Quests, Story, Pets, World };

using TimerId = uint64_t;
inline constexpr TimerId kNoTimer = 0;

struct Timer {
  TimerId id = kNoTimer;
  double dueMs = 0.0;
  TimerOwner owner = TimerOwner::Sim;
  uint16_t kind = 0;        // owner-defined enum value
  EntityId entity = kNoEntity;  // primary subject (attacker, monster to respawn, ...)
  EntityId other = kNoEntity;   // secondary subject (target, ...)
  int32_t param = 0;        // owner-defined (skill index, slot, ...)
  double value = 0.0;       // owner-defined
};

class ABYSS_API TimerQueue {
 public:
  // Schedules a timer; returns its id (ids increase monotonically, so they also encode scheduling order).
  TimerId Schedule(double dueMs, TimerOwner owner, uint16_t kind, EntityId entity = kNoEntity,
                   EntityId other = kNoEntity, int32_t param = 0, double value = 0.0);
  // Removes a pending timer. Returns false if it was not pending.
  bool Cancel(TimerId id);
  // Removes every pending timer for which pred(timer) is true; returns how many were removed. The removed timers are
  // appended to `removed` (in queue order) when it is not null.
  size_t CancelIf(const std::function<bool(const Timer&)>& pred, std::vector<Timer>* removed = nullptr);
  // Pops the earliest timer with dueMs <= nowMs (ties: lowest id first). Returns false when none is due.
  bool PopDue(double nowMs, Timer& out);
  // Earliest pending timer (nullptr when empty).
  const Timer* Peek() const;
  bool IsPending(TimerId id) const;
  const Timer* Find(TimerId id) const;
  size_t Size() const { return timers_.size(); }
  bool Empty() const { return timers_.empty(); }
  void Clear() { timers_.clear(); }
  // Pending timers in due order (read-only; for tests and debugging).
  const std::vector<Timer>& Pending() const { return timers_; }

 private:
  std::vector<Timer> timers_;  // kept sorted by (dueMs, id)
  TimerId nextId_ = 1;
};

}  // namespace abyss
