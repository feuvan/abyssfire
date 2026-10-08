// Active buffs on the hero and on monsters (classes-stats-skills.md 11, combat-feel.md 1.3).
//
// Owner area: hero+combat. getBuffValue = sum of every entry of a stat (no expiry check), capped by buff_caps.json;
// expiry happens by pruning (hero every step; monsters too - FIX Q19). Buff start times are absolute SimClock ms and
// survive zone changes (the hero's list is carried; classes 11.2). Buffs are not saved.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/data/ClassData.h"
#include "abyss/data/SkillData.h"

namespace abyss {

// Identifies buffs that are refreshed in place instead of stacked.
enum class BuffTag : uint8_t {
  None,
  CurseAura,         // elite curse aura on the hero (refreshed while in range)
  DualWieldMastery,  // passive (classes 6.6)
  PetShield,         // ley ward
  PetHowl,           // wolf moon howl
  PetMark,           // owl moon mark (on a monster)
};

struct ActiveBuff {
  BuffStat stat = BuffStat::DamageReduction;
  double value = 0;
  double durationMs = 0;
  double startMs = 0;
  BuffTag tag = BuffTag::None;
  EntityId source = kNoEntity;

  bool ActiveAt(double nowMs) const { return nowMs - startMs < durationMs; }
  double RemainingMs(double nowMs) const { return durationMs - (nowMs - startMs); }
};

class ABYSS_API BuffList {
 public:
  void Add(const ActiveBuff& b) { buffs_.push_back(b); }
  // Refreshes the first entry with the tag (startMs = now, value/duration replaced) or adds a new one.
  void RefreshTagged(const ActiveBuff& b);
  // Sum of every entry of the stat (uncapped).
  double RawSum(BuffStat s) const;
  // getBuffValue: capped sum (BuffCaps.Apply).
  double Value(BuffStat s, const BuffCaps& caps) const { return caps.Apply(s, RawSum(s)); }
  bool Has(BuffStat s) const;
  // Removes entries with now - start >= duration. Returns how many were removed.
  size_t Prune(double nowMs);
  // Removes every entry of a stat (vanish's stealthDamage after a landed basic attack). Returns the count.
  size_t RemoveStat(BuffStat s);
  size_t RemoveTag(BuffTag t);
  const ActiveBuff* FindTag(BuffTag t) const;
  std::span<const ActiveBuff> Items() const { return buffs_; }
  void Clear() { buffs_.clear(); }
  bool Empty() const { return buffs_.empty(); }

 private:
  std::vector<ActiveBuff> buffs_;
};

}  // namespace abyss
