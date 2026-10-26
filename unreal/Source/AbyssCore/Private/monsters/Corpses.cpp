// The zone's corpse registry (dark remake K3; header Corpses.h). Owner area: monsters.
#include "abyss/base/Platform.h"

#include "abyss/monsters/Corpses.h"

#include <algorithm>

#include "abyss/base/Math.h"

namespace abyss {

namespace {

// (death time, id): the eviction order of kill bodies.
bool CorpseOlder(const CorpseEntry& a, const CorpseEntry& b) {
  if (a.deathMs != b.deathMs) return a.deathMs < b.deathMs;
  return a.id < b.id;
}

}  // namespace

uint32_t Corpses::Add(CorpseEntry e, int32_t cap, std::vector<uint32_t>& evicted) {
  e.id = nextId_++;
  const uint32_t id = e.id;
  const bool kill = !e.seeded;
  entries_.push_back(std::move(e));
  if (!kill || cap <= 0) return id;
  while (KillBodies() > cap) {
    const CorpseEntry* oldest = nullptr;
    for (const CorpseEntry& c : entries_) {
      if (c.seeded) continue;
      if (oldest == nullptr || CorpseOlder(c, *oldest)) oldest = &c;
    }
    if (oldest == nullptr) break;
    const uint32_t out = oldest->id;
    evicted.push_back(out);
    Remove(out);
  }
  return id;
}

bool Corpses::Remove(uint32_t id) {
  for (size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].id == id) {
      entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(i));
      return true;
    }
  }
  return false;
}

const CorpseEntry* Corpses::Find(uint32_t id) const {
  for (const CorpseEntry& c : entries_) {
    if (c.id == id) return &c;
  }
  return nullptr;
}

CorpseEntry* Corpses::FindMutable(uint32_t id) {
  for (CorpseEntry& c : entries_) {
    if (c.id == id) return &c;
  }
  return nullptr;
}

void Corpses::CollectExpired(double nowMs, std::vector<uint32_t>& out) const {
  out.clear();
  for (const CorpseEntry& c : entries_) {
    if (!c.seeded && c.expiresMs > 0 && c.expiresMs <= nowMs) out.push_back(c.id);
  }
}

void Corpses::Query(Vec2 centre, double radius, CorpseFilter filter, std::vector<uint32_t>& out) const {
  out.clear();
  const double r2 = radius * radius;
  struct Hit {
    double d2;
    double deathMs;
    uint32_t id;
  };
  std::vector<Hit> hits;
  for (const CorpseEntry& c : entries_) {
    if (filter == CorpseFilter::Raisable && !c.Raisable()) continue;
    if (filter == CorpseFilter::Consumable && !c.Consumable()) continue;
    const double d2 = DistSq(c.pos, centre);
    if (d2 > r2) continue;
    hits.push_back(Hit{d2, c.deathMs, c.id});
  }
  std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
    if (a.d2 != b.d2) return a.d2 < b.d2;
    if (a.deathMs != b.deathMs) return a.deathMs < b.deathMs;
    return a.id < b.id;
  });
  for (const Hit& h : hits) out.push_back(h.id);
}

bool Corpses::Hold(uint32_t id, EntityId holder) {
  CorpseEntry* c = FindMutable(id);
  if (c == nullptr || c->holder != kNoEntity) return false;
  c->holder = holder;
  return true;
}

void Corpses::Release(uint32_t id, EntityId holder) {
  CorpseEntry* c = FindMutable(id);
  if (c != nullptr && c->holder == holder) c->holder = kNoEntity;
}

void Corpses::ReleaseAll(EntityId holder) {
  for (CorpseEntry& c : entries_) {
    if (c.holder == holder) c.holder = kNoEntity;
  }
}

int32_t Corpses::KillBodies() const {
  int32_t n = 0;
  for (const CorpseEntry& c : entries_) n += c.seeded ? 0 : 1;
  return n;
}

}  // namespace abyss
