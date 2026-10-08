// NPC shops and the forge (loot-items-inventory.md 12-13; I9, I10). STUB: owner area items.
#include "abyss/base/Platform.h"

#include "abyss/items/Shop.h"

#include "abyss/base/Assert.h"
#include "abyss/data/DataStore.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

ShopSystem::ShopSystem(SimContext& ctx) : ctx_(ctx) {}

void ShopSystem::Open(std::string_view npcId, bool blacksmith) {
  ABYSS_UNIMPLEMENTED();
  state_.open = true;
  state_.npcId = std::string(npcId);
  state_.blacksmith = blacksmith;
  ctx_.events.Emit(EvShopOpened{state_.npcId, blacksmith});
}

void ShopSystem::OpenWanderingMerchant(const std::vector<std::string>& baseIds, double priceMultiplier) {
  ABYSS_UNIMPLEMENTED();
}

void ShopSystem::Close() {
  if (!state_.open) return;
  ctx_.events.Emit(EvShopClosed{state_.npcId});
  state_ = ShopState{};
}

InvResult ShopSystem::Buy(int32_t wareIndex) {
  ABYSS_UNIMPLEMENTED();
  return InvResult::InvalidIndex;
}

InvResult ShopSystem::Sell(std::string_view uid) {
  ABYSS_UNIMPLEMENTED();
  return InvResult::UnknownItem;
}

InvResult ShopSystem::Buyback(int32_t index) {
  ABYSS_UNIMPLEMENTED();
  return InvResult::InvalidIndex;
}

CraftResult ShopSystem::Craft(CraftAction action, std::string_view itemUid) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

void ShopSystem::FillSnapshot(Snapshot& out) const { out.shop = &state_; }

}  // namespace abyss
