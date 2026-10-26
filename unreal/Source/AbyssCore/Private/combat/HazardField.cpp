// Ground hazards of monster abilities (dark remake K2): the per-zone HazardField (HazardField.h schedule).
#include "abyss/base/Platform.h"

#include "abyss/combat/HazardField.h"

#include <algorithm>

namespace abyss {

int32_t HazardField::TickCount(double durationMs, double tickMs, bool pulseAtStart) {
  if (!(durationMs > 0) || !(tickMs > 0)) return 0;
  // Counted by whole multiples (no division): exact for the ms values data authors; bounded by kHazardMaxTicks.
  int32_t n = 0;
  if (pulseAtStart) {
    while (n < kHazardMaxTicks && n * tickMs < durationMs) ++n;
  } else {
    while (n < kHazardMaxTicks && (n + 1) * tickMs <= durationMs) ++n;
  }
  return n;
}

uint32_t HazardField::Add(HazardEntry e, double durationMs) {
  e.id = nextId_++;
  e.untilMs = e.startMs + durationMs;
  e.ticksTotal = TickCount(durationMs, e.tickMs, e.pulseAtStart);
  e.ticksDone = 0;
  entries_.push_back(std::move(e));
  return entries_.back().id;
}

void HazardField::CollectDue(double nowMs, std::vector<HazardTick>& out) {
  out.clear();
  for (HazardEntry& e : entries_) {
    while (e.ticksDone < e.ticksTotal && e.TickAtMs(e.ticksDone) <= nowMs) {
      out.push_back(HazardTick{e.id, e.ticksDone, e.TickAtMs(e.ticksDone)});
      ++e.ticksDone;
    }
  }
  std::stable_sort(out.begin(), out.end(), [](const HazardTick& a, const HazardTick& b) {
    if (a.atMs != b.atMs) return a.atMs < b.atMs;
    if (a.hazard != b.hazard) return a.hazard < b.hazard;
    return a.index < b.index;
  });
}

void HazardField::DropExpired(double nowMs) {
  const auto expired = [nowMs](const HazardEntry& e) { return e.ticksDone >= e.ticksTotal && nowMs >= e.untilMs; };
  entries_.erase(std::remove_if(entries_.begin(), entries_.end(), expired), entries_.end());
}

const HazardEntry* HazardField::Find(uint32_t id) const {
  for (const HazardEntry& e : entries_) {
    if (e.id == id) return &e;
  }
  return nullptr;
}

}  // namespace abyss
