// NPC shops and the blacksmith forge as runtime actions.
// Spec: loot-items-inventory.md 12 (opening, stock, buy = sellPrice * 3, sell, buyback, layout fix Q7), 13.5 (forge UI
// contract), 12.7 (wandering merchant); save-ui-input.md 7.7; DECISIONS I9, I10 (FIX Q6: a purchase into a full bag is
// refused before the gold is spent), U7 (shop is a modal panel).
//
// Owner area: items. Runtime system (SimContext). The shop session is opened by NPC interaction (QuestWorld /
// ZoneRuntime call Open) and closed by the ClosePanel command.
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

struct ShopWare {
  std::string baseId;
  int64_t price = 0;  // base.sellPrice * 3
};

struct ShopState {
  bool open = false;
  std::string npcId;
  bool blacksmith = false;
  std::vector<ShopWare> wares;  // list order, infinite stock
};

class ABYSS_API ShopSystem {
 public:
  explicit ShopSystem(SimContext& ctx);

  // SHOP_OPEN: stock from ShopDef (npcId), or the wandering-merchant event stock. Emits EvShopOpened.
  void Open(std::string_view npcId, bool blacksmith);
  void OpenWanderingMerchant(const std::vector<std::string>& baseIds, double priceMultiplier);
  void Close();  // EvShopClosed
  const ShopState& State() const { return state_; }

  // One unit per call; createItem(id, heroLevel, normal). FIX Q6: refused (no gold spent) when the bag cannot take it.
  InvResult Buy(int32_t wareIndex);
  InvResult Sell(std::string_view uid);
  InvResult Buyback(int32_t index);
  // Forge actions (blacksmith only); logs ui.forge.log.<action>, SFX anvil / error.
  CraftResult Craft(CraftAction action, std::string_view itemUid);

  void FillSnapshot(Snapshot& out) const;

 private:
  SimContext& ctx_;
  ShopState state_;
};

}  // namespace abyss
