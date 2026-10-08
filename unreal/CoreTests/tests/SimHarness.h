// Test harness for runtime systems: the shared services of one GameSim (clock, RNG streams, timers, events, bus, ids,
// session) around the real exported data, without any subsystem. A test constructs only the systems it needs with
// `h.ctx`, registers them in `h.ctx.sys`, and drives the clock with Step() (timers are routed to `onTimer`).
#pragma once

#include <functional>

#include "TestUtil.h"
#include "abyss/base/Rng.h"
#include "abyss/base/SimClock.h"
#include "abyss/base/Timers.h"
#include "abyss/sim/Events.h"
#include "abyss/sim/GameplayBus.h"
#include "abyss/sim/Session.h"
#include "abyss/sim/SimContext.h"

namespace abyss::test {

struct SimHarness {
  explicit SimHarness(uint64_t seed = 1, const DataStore& data = RealData())
      : ctx{data, config, clock, rng, timers, events, bus, ids, session, SimSystems{}, EquipStats{}} {
    rng.SeedAll(seed);
  }

  // Advances the sim clock by `steps` fixed steps, draining due timers into `onTimer` (ascending due, then id).
  void Step(int steps = 1) {
    for (int i = 0; i < steps; ++i) {
      clock.AdvanceStep();
      Timer t;
      while (timers.PopDue(clock.NowMs(), t)) {
        if (onTimer) onTimer(t);
      }
    }
  }

  SimConfig config;
  SimClock clock;
  RngSet rng;
  TimerQueue timers;
  EventSink events;
  GameplayBus bus;
  EntityIdAllocator ids;
  SessionState session;
  SimContext ctx;
  std::function<void(const Timer&)> onTimer;
};

// Counts events of one type in a sink.
template <class E>
size_t CountEvents(const EventSink& sink) {
  size_t n = 0;
  for (const Event& e : sink.Items()) {
    if (std::holds_alternative<E>(e)) ++n;
  }
  return n;
}

}  // namespace abyss::test
