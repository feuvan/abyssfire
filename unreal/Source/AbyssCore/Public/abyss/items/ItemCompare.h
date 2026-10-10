// Item comparison for tooltips (pure). Spec: loot-items-inventory.md section 14 (itemStatTotals, compareTarget,
// statDeltas), 15.2 (tooltip content order).
//
// Owner area: items.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/items/Item.h"

namespace abyss {

class DataStore;

// A comparable stat key: an item Stat or one of the two pseudo keys (__avgDamage, __baseDefense).
struct CompareKey {
  enum class Kind : uint8_t { AvgDamage, BaseDefense, StatKey } kind = Kind::StatKey;
  Stat stat = Stat::Damage;
  bool operator==(const CompareKey& o) const { return kind == o.kind && (kind != Kind::StatKey || stat == o.stat); }
};

struct StatTotal {
  CompareKey key;
  double value = 0;
};

// itemStatTotals (14): insertion-ordered - __avgDamage, __baseDefense, affixes, the C11 legendary special effect
// (ItemSpecialEffectStat; port: it is a gear stat since C11), socketed gems.
ABYSS_API std::vector<StatTotal> ItemStatTotals(const DataStore& data, const ItemInstance& item);

struct CompareTarget {
  EquipSlot slot = EquipSlot::Weapon;
  const ItemInstance* equipped = nullptr;  // nullptr = empty slot
};
// compareTarget (14): nullopt if the base has no slot or the item is worn (by uid).
ABYSS_API std::optional<CompareTarget> FindCompareTarget(
    const DataStore& data, const ItemInstance& item,
    const std::array<std::optional<ItemInstance>, EnumCount<EquipSlot>()>& equipment);

struct StatDelta {
  CompareKey key;
  double delta = 0;  // JsRound(d * 10) / 10, never 0
};
// statDeltas (14): stable sort by rank (avgDamage, baseDefense, others) then |d| desc.
ABYSS_API std::vector<StatDelta> StatDeltas(const DataStore& data, const ItemInstance& candidate, const ItemInstance* equipped);

}  // namespace abyss
