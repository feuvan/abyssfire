#include "abyss/base/Platform.h"

#include "abyss/base/Timers.h"

#include <algorithm>

namespace abyss {
namespace {

bool Before(const Timer& a, const Timer& b) {
  if (a.dueMs != b.dueMs) return a.dueMs < b.dueMs;
  return a.id < b.id;
}

}  // namespace

TimerId TimerQueue::Schedule(double dueMs, TimerOwner owner, uint16_t kind, EntityId entity, EntityId other,
                             int32_t param, double value) {
  Timer t;
  t.id = nextId_++;
  t.dueMs = dueMs;
  t.owner = owner;
  t.kind = kind;
  t.entity = entity;
  t.other = other;
  t.param = param;
  t.value = value;
  auto it = std::upper_bound(timers_.begin(), timers_.end(), t, Before);
  timers_.insert(it, t);
  return t.id;
}

bool TimerQueue::Cancel(TimerId id) {
  for (size_t i = 0; i < timers_.size(); ++i) {
    if (timers_[i].id == id) {
      timers_.erase(timers_.begin() + static_cast<std::ptrdiff_t>(i));
      return true;
    }
  }
  return false;
}

size_t TimerQueue::CancelIf(const std::function<bool(const Timer&)>& pred, std::vector<Timer>* removed) {
  size_t n = 0;
  std::vector<Timer> keep;
  keep.reserve(timers_.size());
  for (const Timer& t : timers_) {
    if (pred(t)) {
      ++n;
      if (removed) removed->push_back(t);
    } else {
      keep.push_back(t);
    }
  }
  timers_.swap(keep);
  return n;
}

bool TimerQueue::PopDue(double nowMs, Timer& out) {
  if (timers_.empty() || timers_.front().dueMs > nowMs) return false;
  out = timers_.front();
  timers_.erase(timers_.begin());
  return true;
}

const Timer* TimerQueue::Peek() const { return timers_.empty() ? nullptr : &timers_.front(); }

bool TimerQueue::IsPending(TimerId id) const { return Find(id) != nullptr; }

const Timer* TimerQueue::Find(TimerId id) const {
  for (const Timer& t : timers_) {
    if (t.id == id) return &t;
  }
  return nullptr;
}

}  // namespace abyss
