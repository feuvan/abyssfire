// BuffList (classes-stats-skills.md section 11, combat-feel.md 1.3). Implemented (shared by hero, monsters and pets).
#include "abyss/base/Platform.h"

#include "abyss/hero/Buffs.h"

#include <algorithm>

namespace abyss {

void BuffList::RefreshTagged(const ActiveBuff& b) {
  for (ActiveBuff& e : buffs_) {
    if (e.tag == b.tag && b.tag != BuffTag::None) {
      e.startMs = b.startMs;
      e.value = b.value;
      e.durationMs = b.durationMs;
      e.stat = b.stat;
      e.source = b.source;
      return;
    }
  }
  buffs_.push_back(b);
}

double BuffList::RawSum(BuffStat s) const {
  double total = 0;
  for (const ActiveBuff& b : buffs_) {
    if (b.stat == s) total += b.value;
  }
  return total;
}

bool BuffList::Has(BuffStat s) const {
  for (const ActiveBuff& b : buffs_) {
    if (b.stat == s) return true;
  }
  return false;
}

size_t BuffList::Prune(double nowMs) {
  const size_t before = buffs_.size();
  buffs_.erase(std::remove_if(buffs_.begin(), buffs_.end(),
                              [nowMs](const ActiveBuff& b) { return nowMs - b.startMs >= b.durationMs; }),
               buffs_.end());
  return before - buffs_.size();
}

size_t BuffList::RemoveStat(BuffStat s) {
  const size_t before = buffs_.size();
  buffs_.erase(std::remove_if(buffs_.begin(), buffs_.end(), [s](const ActiveBuff& b) { return b.stat == s; }),
               buffs_.end());
  return before - buffs_.size();
}

size_t BuffList::RemoveTag(BuffTag t) {
  const size_t before = buffs_.size();
  buffs_.erase(std::remove_if(buffs_.begin(), buffs_.end(), [t](const ActiveBuff& b) { return b.tag == t; }),
               buffs_.end());
  return before - buffs_.size();
}

const ActiveBuff* BuffList::FindTag(BuffTag t) const {
  for (const ActiveBuff& b : buffs_) {
    if (b.tag == t) return &b;
  }
  return nullptr;
}

}  // namespace abyss
