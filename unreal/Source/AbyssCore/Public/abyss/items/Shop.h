// NPC shops and the blacksmith forge as runtime actions.
// Spec: loot-items-inventory.md 12 (opening, stock, buy = sellPrice * 3, sell, buyback, layout fix Q7), 13.5 (forge UI
// contract), 12.7 (wandering merchant); save-ui-input.md 7.7; DECISIONS I9, I10 (FIX Q6: a purchase into a full bag is
// refused before the gold is spent), U7 (shop is a modal panel), C12 (no transactions while Dying).
//
// Owner area: items. Runtime system (SimContext). The shop session is opened by NPC interaction (QuestWorld /
// ZoneRuntime call Open) and closed by the ClosePanel command. Gold moves through RewardService (EvGoldChanged with
// ShopBuy / ShopSell / ShopBuyback / Craft).
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/data/ItemData.h"
#include "abyss/items/Crafting.h"
#include "abyss/items/Inventory.h"

namespace abyss {

struct SimContext;
struct Snapshot;

inline constexpr std::string_view kWanderingMerchantShopId = "wandering_merchant";

struct ShopWare {
  std::string baseId;
  int64_t price = 0;  // base.sellPrice * 3 (x the wandering merchant's priceMultiplier)
};

struct ShopState {
  bool open = false;
  std::string npcId;
  bool blacksmith = false;
  std::vector<ShopWare> wares;  // list order, infinite stock (UE shows them in a scrolling list, FIX Q7)
};

class ABYSS_API ShopSystem {
 public:
  explicit ShopSystem(SimContext& ctx);

  // SHOP_OPEN: stock from ShopDef (npcId); unknown and removed bases (I4 / I2) are skipped. Emits EvShopOpened.
  void Open(std::string_view npcId, bool blacksmith);
  // Wandering-merchant event (12.7, FIX Q12): each id is an item base, or a web merchantItems id resolved through
  // shops.json wanderingMerchant.<zone>.idMap (the current zone first, then the other zones in table order); ids that
  // resolve to nothing, unknown or removed bases are skipped. price = round(sellPrice * 3 * priceMultiplier).
  void OpenWanderingMerchant(const std::vector<std::string>& baseIds, double priceMultiplier);
  // The item base a wandering-merchant id stands for in `zoneId` ("" when none; see OpenWanderingMerchant).
  static std::string ResolveWanderingMerchantId(const DataStore& data, std::string_view zoneId, std::string_view id);
  void Close();  // EvShopClosed
  const ShopState& State() const { return state_; }

  // One unit per call; createItem(id, heroLevel, normal). FIX Q6: refused (no gold spent) when the bag cannot take it.
  InvResult Buy(int32_t wareIndex);
  // sellItem (I9 price) -> gold. Any bag item while a shop is open.
  InvResult Sell(std::string_view uid);
  InvResult Buyback(int32_t index);
  // Forge actions (blacksmith only); logs ui.forge.log.<action> / ui.forge.block.<reason>, SFX anvil / error,
  // EvCraftPerformed. A closed shop, a merchant or a Dying hero returns {ok = false, reason = None} without effects.
  CraftResult Craft(CraftAction action, std::string_view itemUid);

  void FillSnapshot(Snapshot& out) const;

 private:
  bool Blocked() const;  // shop closed or hero not alive

  SimContext& ctx_;
  ShopState state_;
};

}  // namespace abyss
