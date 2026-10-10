// NPC shops and the forge (loot-items-inventory.md 12-13; I9, I10 FIX Q6/Q7/Q12, C12). Owner area: items.
#include "abyss/base/Platform.h"

#include "abyss/items/Shop.h"

#include "abyss/base/Math.h"
#include "abyss/base/StrUtil.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/hero/Rewards.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

namespace {

int64_t ShpPrice(const ItemBaseDef& base, double buyMul, double extraMul) {
  return static_cast<int64_t>(JsRound(static_cast<double>(base.sellPrice) * buyMul * extraMul));
}

}  // namespace

ShopSystem::ShopSystem(SimContext& ctx) : ctx_(ctx) {}

bool ShopSystem::Blocked() const {
  if (!state_.open) return true;
  return ctx_.sys.hero != nullptr && ctx_.sys.hero->Life() != HeroLife::Alive;
}

void ShopSystem::Open(std::string_view npcId, bool blacksmith) {
  const ItemTables& t = ctx_.data.Items();
  state_ = ShopState{};
  state_.open = true;
  state_.npcId = std::string(npcId);
  state_.blacksmith = blacksmith;
  if (const ShopDef* shop = t.FindShop(npcId)) {
    for (const std::string& id : shop->items) {
      const ItemBaseDef* base = t.FindBase(id);
      if (base == nullptr || t.IsRemovedItem(id)) continue;
      state_.wares.push_back(ShopWare{id, ShpPrice(*base, t.economy.buyPriceMultiplier, 1.0)});
    }
  }
  ctx_.events.Emit(EvShopOpened{state_.npcId, blacksmith});
}

std::string ShopSystem::ResolveWanderingMerchantId(const DataStore& data, std::string_view zoneId, std::string_view id) {
  const ItemTables& t = data.Items();
  if (t.FindBase(id) != nullptr) return std::string(id);
  // FIX Q12: the event's web ids map to real bases (shops.json idMap), the current zone's map first.
  if (const WanderingMerchantDef* w = t.FindWanderingMerchant(zoneId)) {
    const std::string_view mapped = w->MapId(id);
    if (!mapped.empty()) return std::string(mapped);
  }
  for (const WanderingMerchantDef& w : t.wanderingMerchant) {
    const std::string_view mapped = w.MapId(id);
    if (!mapped.empty()) return std::string(mapped);
  }
  return std::string();
}

void ShopSystem::OpenWanderingMerchant(const std::vector<std::string>& baseIds, double priceMultiplier) {
  const ItemTables& t = ctx_.data.Items();
  state_ = ShopState{};
  state_.open = true;
  state_.npcId = std::string(kWanderingMerchantShopId);
  state_.blacksmith = false;
  const double mul = priceMultiplier > 0 ? priceMultiplier : 1.0;
  for (const std::string& raw : baseIds) {
    const std::string id = ResolveWanderingMerchantId(ctx_.data, ctx_.session.currentMap, raw);
    const ItemBaseDef* base = id.empty() ? nullptr : t.FindBase(id);
    if (base == nullptr || t.IsRemovedItem(id)) continue;
    state_.wares.push_back(ShopWare{id, ShpPrice(*base, t.economy.buyPriceMultiplier, mul)});
  }
  ctx_.events.Emit(EvShopOpened{state_.npcId, false});
}

void ShopSystem::Close() {
  if (!state_.open) return;
  ctx_.events.Emit(EvShopClosed{state_.npcId});
  state_ = ShopState{};
}

InvResult ShopSystem::Buy(int32_t wareIndex) {
  if (!state_.open) return InvResult::ShopClosed;
  if (Blocked()) return InvResult::Dying;
  if (wareIndex < 0 || wareIndex >= static_cast<int32_t>(state_.wares.size())) return InvResult::InvalidIndex;
  const ShopWare ware = state_.wares[static_cast<size_t>(wareIndex)];
  RewardService* rewards = ctx_.sys.rewards;
  InventorySystem* inv = ctx_.sys.inventory;
  if (rewards == nullptr || inv == nullptr || ctx_.sys.hero == nullptr) return InvResult::UnknownItem;
  if (!rewards->CanAfford(ware.price)) return InvResult::NotEnoughGold;
  ItemInstance probe;
  probe.baseId = ware.baseId;
  probe.quantity = 1;
  if (!inv->Items().CanAdd(probe)) {  // FIX Q6: refuse before paying
    ctx_.events.Log(MakeLoc("sys.inventory.bagFull"), LogType::System);
    return InvResult::BagFull;
  }
  const LootContext lc{&ctx_.data, &ctx_.Rand(RngStream::Loot), &inv->Uids()};
  std::optional<ItemInstance> item = CreateItem(lc, ware.baseId, ctx_.sys.hero->Level(), ItemQuality::Normal);
  if (!item.has_value()) return InvResult::UnknownItem;
  if (!rewards->SpendGold(ware.price, GoldReason::ShopBuy)) return InvResult::NotEnoughGold;
  item->identified = true;
  rewards->GrantItem(*item, OverflowPolicy::Refuse, ItemSource::Shop);
  return InvResult::Ok;
}

InvResult ShopSystem::Sell(std::string_view uid) {
  if (!state_.open) return InvResult::ShopClosed;
  if (Blocked()) return InvResult::Dying;
  InventorySystem* inv = ctx_.sys.inventory;
  RewardService* rewards = ctx_.sys.rewards;
  if (inv == nullptr || rewards == nullptr) return InvResult::UnknownItem;
  const std::optional<int64_t> price = inv->Items().Sell(uid);
  if (!price.has_value()) return InvResult::UnknownItem;
  rewards->ChangeGold(*price, GoldReason::ShopSell);
  ctx_.events.Emit(EvInventoryChanged{});
  return InvResult::Ok;
}

InvResult ShopSystem::Buyback(int32_t index) {
  if (!state_.open) return InvResult::ShopClosed;
  if (Blocked()) return InvResult::Dying;
  InventorySystem* inv = ctx_.sys.inventory;
  RewardService* rewards = ctx_.sys.rewards;
  if (inv == nullptr || rewards == nullptr) return InvResult::UnknownItem;
  const std::span<const BuybackEntry> list = inv->Items().BuybackList();
  if (index < 0 || index >= static_cast<int32_t>(list.size())) return InvResult::InvalidIndex;
  if (!rewards->CanAfford(list[static_cast<size_t>(index)].price)) return InvResult::NotEnoughGold;
  int64_t cost = 0;
  ItemInstance item;
  const InvResult r = inv->Items().Buyback(index, cost, item);
  if (r == InvResult::BagFull) ctx_.events.Log(MakeLoc("sys.inventory.bagFull"), LogType::System);
  if (r != InvResult::Ok) return r;
  rewards->SpendGold(cost, GoldReason::ShopBuyback);
  ctx_.events.Log(MakeLoc("ui.shop.buybackLog", {ItemNameArg("name", item, ctx_.data)}), LogType::Loot);
  ctx_.events.Emit(EvInventoryChanged{});
  return InvResult::Ok;
}

CraftResult ShopSystem::Craft(CraftAction action, std::string_view itemUid) {
  CraftResult none;
  none.itemUid = std::string(itemUid);
  if (!state_.open || !state_.blacksmith || Blocked()) return none;
  InventorySystem* inv = ctx_.sys.inventory;
  RewardService* rewards = ctx_.sys.rewards;
  Hero* hero = ctx_.sys.hero;
  if (inv == nullptr || rewards == nullptr || hero == nullptr) return none;

  // Names for the log: salvage / socket use the item before the action, reforge / upgrade the result.
  const I18nArg before = [&]() {
    const ItemInstance* it = inv->Items().FindInBag(itemUid);
    return it != nullptr ? ItemNameArg("name", *it, ctx_.data) : I18nArg{"name", std::string(itemUid), false};
  }();

  int64_t gold = hero->Gold();
  const LootContext lc{&ctx_.data, &ctx_.Rand(RngStream::Loot), &inv->Uids()};
  CraftResult r = PerformCraft(lc, action, itemUid, inv->Items(), gold);
  if (!r.ok) {
    ctx_.events.Sfx(SfxId::Error);
    const LocText reason = MakeLoc("ui.forge.block." + std::string(EnumName(r.reason)));
    ctx_.events.Log(reason, LogType::System);
    ctx_.events.Emit(EvCraftPerformed{action, false, r.itemUid, reason});
    return r;
  }
  if (r.goldSpent > 0) rewards->SpendGold(r.goldSpent, GoldReason::Craft);
  ctx_.events.Sfx(SfxId::Anvil);
  switch (action) {
    case CraftAction::Salvage: {
      const I18n& i18n = ctx_.data.Strings();
      std::string list;
      for (const MaterialYield& y : r.yields) {
        if (!list.empty()) list += ' ';
        const ItemBaseDef* mb = ctx_.data.Items().FindBase(y.baseId);
        const std::string matName = i18n.NameOr("data.item." + y.baseId + ".name", mb != nullptr ? mb->name : y.baseId);
        const std::vector<I18nArg> args{I18nArg{"name", matName, false}, I18nArg{"qty", ToStr(y.quantity), false}};
        list += i18n.T("ui.forge.qty", args);
      }
      ctx_.events.Log(MakeLoc("ui.forge.log.salvage", {before, {"list", list}}), LogType::Loot);
      break;
    }
    case CraftAction::Socket:
      ctx_.events.Log(MakeLoc("ui.forge.log.socket", {before}), LogType::Loot);
      break;
    case CraftAction::Reforge:
    case CraftAction::Upgrade:
      if (const ItemInstance* it = inv->Items().FindInBag(itemUid)) {
        ctx_.events.Log(MakeLoc("ui.forge.log." + std::string(EnumName(action)), {ItemNameArg("name", *it, ctx_.data)}),
                        LogType::Loot);
      }
      break;
  }
  ctx_.events.Emit(EvCraftPerformed{action, true, r.itemUid, LocText{}});
  ctx_.events.Emit(EvInventoryChanged{});
  return r;
}

void ShopSystem::FillSnapshot(Snapshot& out) const { out.shop = &state_; }

}  // namespace abyss
