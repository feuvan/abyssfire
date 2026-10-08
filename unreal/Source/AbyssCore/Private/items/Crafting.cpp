// Blacksmith crafting (loot-items-inventory.md section 13). STUB: owner area items.
#include "abyss/base/Platform.h"

#include "abyss/items/Crafting.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/Assert.h"
#include "abyss/data/DataStore.h"
#include "abyss/items/Inventory.h"

namespace abyss {

int64_t CraftGoldUnit(int32_t level) { return 6 * (static_cast<int64_t>(std::max(1, level)) + 5); }

CraftCost ComputeCraftCost(const DataStore& data, CraftAction action, const ItemInstance& item) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

std::vector<MaterialYield> SalvageYield(const DataStore& data, const ItemInstance& item) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

CraftCheck CheckCraft(const DataStore& data, CraftAction action, std::string_view itemUid, const Inventory& inv,
                      int64_t gold) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

CraftResult PerformCraft(const LootContext& ctx, CraftAction action, std::string_view itemUid, Inventory& inv,
                         int64_t& gold) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

}  // namespace abyss
