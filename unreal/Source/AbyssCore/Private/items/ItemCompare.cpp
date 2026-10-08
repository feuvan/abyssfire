// Item comparison (loot-items-inventory.md section 14). STUB: owner area items.
#include "abyss/base/Platform.h"

#include "abyss/items/ItemCompare.h"

#include "abyss/base/Assert.h"
#include "abyss/data/DataStore.h"

namespace abyss {

std::vector<StatTotal> ItemStatTotals(const DataStore& data, const ItemInstance& item) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

std::optional<CompareTarget> FindCompareTarget(
    const DataStore& data, const ItemInstance& item,
    const std::array<std::optional<ItemInstance>, EnumCount<EquipSlot>()>& equipment) {
  ABYSS_UNIMPLEMENTED();
  return std::nullopt;
}

std::vector<StatDelta> StatDeltas(const DataStore& data, const ItemInstance& candidate, const ItemInstance* equipped) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

}  // namespace abyss
