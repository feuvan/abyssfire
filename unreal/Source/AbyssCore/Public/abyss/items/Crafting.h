// Blacksmith crafting (pure): costs, salvage yields, check, perform.
// Spec: loot-items-inventory.md section 13 (13.1 costs, 13.2 salvage yield, 13.3 check order, 13.4 perform);
// 9.1 (socket capacity caps). Tables: crafting.json.
//
// Owner area: items. Acts on bag items only; RNG = RngStream::Loot (the affix roller and the upgrade target: the web's
// injected `rng` is the same stream here, drawn BEFORE the new affixes).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/data/ItemData.h"
#include "abyss/items/Item.h"
#include "abyss/items/LootGen.h"

namespace abyss {

class Inventory;

enum class CraftFail : uint8_t { None, UnknownBase, NotEquipment, Quality, NotInBag, MaxSockets, Gold, Materials,
                                 BagFull };
ABYSS_ENUM_STRINGS(CraftFail, "none", "unknownBase", "notEquipment", "quality", "notInBag", "maxSockets", "gold",
                   "materials", "bagFull")

// craftGoldUnit (13): 6 * (max(1, floor(L)) + 5) - the web constants; the data-driven form reads crafting.json goldUnit.
ABYSS_API int64_t CraftGoldUnit(int32_t level);
ABYSS_API int64_t CraftGoldUnit(const DataStore& data, int32_t level);

struct CraftCost {
  bool applies = false;  // false = the action does not apply (null cost)
  int64_t gold = 0;
  std::vector<CraftMaterialCost> materials;
};
// craftCost (13.1): applies only to equipment; reforge magic / rare, upgrade normal / magic, socket weapon / armour.
ABYSS_API CraftCost ComputeCraftCost(const DataStore& data, CraftAction action, const ItemInstance& item);
// upgradeTarget: normal -> magic, magic -> rare, else nullopt.
ABYSS_API std::optional<ItemQuality> CraftUpgradeTarget(ItemQuality q);

struct MaterialYield {
  std::string baseId;
  int32_t quantity = 0;
};
// salvageYield (13.2), materials in crafting.json order (scrap, dust, essence), zero yields omitted.
ABYSS_API std::vector<MaterialYield> SalvageYield(const DataStore& data, const ItemInstance& item);

struct CraftCheck {
  bool ok = false;
  CraftFail reason = CraftFail::None;
  CraftCost cost;
};
// check (13.3): first failing reason wins. The item is looked up by uid in the bag (an equipped / unknown uid is
// NotInBag once the earlier checks pass; a uid found nowhere is NotInBag).
ABYSS_API CraftCheck CheckCraft(const DataStore& data, CraftAction action, std::string_view itemUid, const Inventory& inv,
                                int64_t gold);

struct CraftResult {
  bool ok = false;
  CraftFail reason = CraftFail::None;
  int64_t goldSpent = 0;
  std::vector<MaterialYield> yields;      // salvage
  std::vector<std::string> gemsReturned;  // salvage: gem base ids pulled out of the sockets
  std::string itemUid;
};
// perform (13.4): spends gold (out param) and materials (smallest stacks first) then applies the action.
ABYSS_API CraftResult PerformCraft(const LootContext& ctx, CraftAction action, std::string_view itemUid, Inventory& inv,
                                   int64_t& gold);

}  // namespace abyss
