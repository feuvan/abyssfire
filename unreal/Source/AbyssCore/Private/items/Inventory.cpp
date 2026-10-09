// Bag, equipment, stash, buyback, gear stats (loot-items-inventory.md 7-10, 12.4-12.5, 16; I3-I10, C11, C12).
// Owner area: items.
#include "abyss/base/Platform.h"

#include "abyss/items/Inventory.h"

#include <algorithm>
#include <set>

#include "abyss/base/Math.h"
#include "abyss/base/StrUtil.h"
#include "abyss/combat/StatusEffects.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/items/ItemCompare.h"
#include "abyss/pets/Homestead.h"
#include "abyss/save/SaveData.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

namespace {

int32_t InvMaxStack(const ItemBaseDef* base) { return base != nullptr ? (std::max)(1, base->maxStack) : 1; }

// sortInventory comparator (7.3): quality order, type order, stored name. Unknown base type sorts last (7).
struct InvSortKey {
  int32_t quality = 0;
  int32_t type = 0;
};

InvSortKey InvKeyOf(const DataStore& data, const ItemInstance& it) {
  const EconomyDef& e = data.Items().economy;
  InvSortKey k;
  k.quality = e.sortOrderQuality[EnumIndex(it.quality)];
  const ItemBaseDef* base = data.Items().FindBase(it.baseId);
  k.type = base != nullptr ? e.sortOrderType[EnumIndex(base->type)] : static_cast<int32_t>(EnumCount<ItemType>());
  return k;
}

void InvSort(const DataStore& data, std::vector<ItemInstance>& list) {
  std::stable_sort(list.begin(), list.end(), [&data](const ItemInstance& a, const ItemInstance& b) {
    const InvSortKey ka = InvKeyOf(data, a);
    const InvSortKey kb = InvKeyOf(data, b);
    if (ka.quality != kb.quality) return ka.quality < kb.quality;
    if (ka.type != kb.type) return ka.type < kb.type;
    return a.name < b.name;
  });
}

}  // namespace

// ---- Inventory (pure) ----
Inventory::Inventory(const DataStore& data) : data_(&data) {}

int32_t Inventory::Capacity() const {
  const int32_t cap = data_->Items().economy.bagCapacity;
  return cap > 0 ? cap : kMaxInventoryEntries;
}

bool Inventory::CanAdd(const ItemInstance& item) const {
  const ItemBaseDef* base = data_->Items().FindBase(item.baseId);
  int32_t remaining = item.quantity;
  if (base != nullptr && base->stackable) {
    const int32_t cap = InvMaxStack(base);
    for (const ItemInstance& ex : bag_) {
      if (ex.baseId == item.baseId && ex.quantity < cap) {
        remaining -= (std::min)(remaining, cap - ex.quantity);
        if (remaining <= 0) return true;
        break;  // only the first partial stack is topped up (Q21)
      }
    }
  }
  return !IsFull();
}

AddResult Inventory::AddItem(ItemInstance item) {
  AddResult r;
  if (!CanAdd(item)) return r;  // atomic (port fix)
  const ItemBaseDef* base = data_->Items().FindBase(item.baseId);
  if (base != nullptr && base->stackable) {
    const int32_t cap = InvMaxStack(base);
    for (ItemInstance& ex : bag_) {
      if (ex.baseId != item.baseId || ex.quantity >= cap) continue;
      const int32_t add = (std::min)(item.quantity, cap - ex.quantity);
      ex.quantity += add;
      item.quantity -= add;
      r.stackedQuantity = add;
      if (item.quantity <= 0) {
        r.ok = true;
        return r;
      }
      break;
    }
  }
  bag_.push_back(std::move(item));
  r.ok = true;
  r.newEntry = true;
  return r;
}

std::optional<ItemInstance> Inventory::RemoveItem(std::string_view uid, int32_t qty, ItemUidGenerator* uids) {
  const int32_t idx = BagIndex(uid);
  if (idx < 0 || qty < 1) return std::nullopt;
  ItemInstance& e = bag_[static_cast<size_t>(idx)];
  if (e.quantity > qty) {
    e.quantity -= qty;
    ItemInstance split = e;
    split.quantity = qty;
    if (uids != nullptr) split.uid = uids->Next();
    return split;
  }
  return TakeEntry(uid);
}

std::optional<ItemInstance> Inventory::TakeEntry(std::string_view uid) {
  const int32_t idx = BagIndex(uid);
  if (idx < 0) return std::nullopt;
  ItemInstance out = std::move(bag_[static_cast<size_t>(idx)]);
  bag_.erase(bag_.begin() + idx);
  return out;
}

const ItemInstance* Inventory::FindInBag(std::string_view uid) const {
  const int32_t idx = BagIndex(uid);
  return idx < 0 ? nullptr : &bag_[static_cast<size_t>(idx)];
}

ItemInstance* Inventory::FindInBagMutable(std::string_view uid) {
  const int32_t idx = BagIndex(uid);
  return idx < 0 ? nullptr : &bag_[static_cast<size_t>(idx)];
}

int32_t Inventory::BagIndex(std::string_view uid) const {
  for (size_t i = 0; i < bag_.size(); ++i) {
    if (bag_[i].uid == uid) return static_cast<int32_t>(i);
  }
  return -1;
}

int32_t Inventory::CountOf(std::string_view baseId) const {
  int32_t n = 0;
  for (const ItemInstance& it : bag_) {
    if (it.baseId == baseId) n += (std::max)(0, it.quantity);
  }
  return n;
}

void Inventory::SortBag() { InvSort(*data_, bag_); }

int32_t Inventory::DestroyNormalItems() {
  const size_t before = bag_.size();
  const DataStore& d = *data_;
  bag_.erase(std::remove_if(bag_.begin(), bag_.end(),
                            [&d](const ItemInstance& it) {
                              if (it.quality != ItemQuality::Normal) return false;
                              const ItemBaseDef* base = d.Items().FindBase(it.baseId);
                              if (base == nullptr) return false;
                              return base->type == ItemType::Weapon || base->type == ItemType::Armor ||
                                     base->type == ItemType::Accessory;
                            }),
             bag_.end());
  return static_cast<int32_t>(before - bag_.size());
}

InvResult Inventory::IdentifyItem(std::string_view uid) {
  ItemInstance* it = FindInBagMutable(uid);
  if (it == nullptr) return InvResult::UnknownItem;
  if (it->identified) return InvResult::NotUsable;
  int32_t scroll = -1;
  for (size_t i = 0; i < bag_.size(); ++i) {
    if (bag_[i].baseId == "c_id_scroll" && bag_[i].quantity > 0) {
      scroll = static_cast<int32_t>(i);
      break;
    }
  }
  if (scroll < 0) return InvResult::NeedScroll;
  it->identified = true;
  ItemInstance& s = bag_[static_cast<size_t>(scroll)];
  if (--s.quantity <= 0) bag_.erase(bag_.begin() + scroll);
  return InvResult::Ok;
}

InvResult Inventory::Equip(std::string_view uid, int32_t heroLevel, EquipSlot* outSlot) {
  const int32_t idx = BagIndex(uid);
  if (idx < 0) return InvResult::UnknownItem;
  const ItemBaseDef* base = data_->Items().FindBase(bag_[static_cast<size_t>(idx)].baseId);
  if (base == nullptr || !base->hasSlot) return InvResult::NotEquipment;
  if (heroLevel < base->levelReq) return InvResult::LevelTooLow;  // I3
  EquipSlot slot = base->slot;
  if (slot == EquipSlot::Ring1 || slot == EquipSlot::Ring2) {
    // FIX Q18: free ring1, else free ring2, else the weaker ring (the slot the tooltip compares against).
    const std::optional<CompareTarget> ct = FindCompareTarget(*data_, bag_[static_cast<size_t>(idx)], equipment_);
    if (ct.has_value()) slot = ct->slot;
  }
  std::optional<ItemInstance>& dst = equipment_[EnumIndex(slot)];
  ItemInstance item = std::move(bag_[static_cast<size_t>(idx)]);
  if (dst.has_value()) {
    bag_[static_cast<size_t>(idx)] = std::move(*dst);  // FIX Q13: swap in place
  } else {
    bag_.erase(bag_.begin() + idx);
  }
  dst = std::move(item);
  if (outSlot != nullptr) *outSlot = slot;
  return InvResult::Ok;
}

InvResult Inventory::Unequip(EquipSlot slot) {
  std::optional<ItemInstance>& e = equipment_[EnumIndex(slot)];
  if (!e.has_value()) return InvResult::UnknownItem;
  if (IsFull()) return InvResult::BagFull;
  bag_.push_back(std::move(*e));
  e.reset();
  return InvResult::Ok;
}

const ItemInstance* Inventory::Equipped(EquipSlot slot) const {
  const auto& e = equipment_[static_cast<size_t>(slot)];
  return e.has_value() ? &*e : nullptr;
}

ItemInstance* Inventory::EquippedMutable(EquipSlot slot) {
  auto& e = equipment_[static_cast<size_t>(slot)];
  return e.has_value() ? &*e : nullptr;
}

StatBag Inventory::EquipmentStatBag() const {
  const ItemTables& t = data_->Items();
  StatBag s;
  for (const auto& e : equipment_) {
    if (!e.has_value()) continue;
    const ItemInstance& item = *e;
    const ItemBaseDef* base = t.FindBase(item.baseId);
    if (base != nullptr && base->hasBaseDamage) {
      s.Add(Stat::WeaponDamageMin, base->baseDamageMin);  // shields add 0
      s.Add(Stat::WeaponDamageMax, base->baseDamageMax);
    }
    if (base != nullptr && base->hasBaseDefense) s.Add(Stat::Defense, base->baseDefense);
    for (const StatValue& sv : item.stats.Items()) s.Add(sv.stat, sv.value);
    // C11: a legendary special effect that combat reads is an item stat.
    if (!item.legendaryId.empty()) {
      const LegendaryDef* l = t.FindLegendary(item.legendaryId);
      if (l != nullptr && l->hasSpecialEffectValue) {
        Stat st{};
        const bool applied = std::find(t.appliedSpecialEffects.begin(), t.appliedSpecialEffects.end(),
                                       l->specialEffect) != t.appliedSpecialEffects.end();
        if (applied && ParseEnum(l->specialEffect, st)) s.Add(st, l->specialEffectValue);
      }
    }
  }
  // Set bonuses: pieces counted by setId (Q14), cumulative bonuses, sets in table order.
  for (const SetDef& set : t.sets) {
    const int32_t count = EquippedSetCount(set.id);
    if (count <= 0) continue;
    for (const SetBonusDef& b : set.bonuses) {
      if (count >= b.count) s.AddAll(b.stats);
    }
  }
  const double all = s.Get(Stat::AllStats);
  if (all != 0) {
    for (Stat k : t.allStatsExpandsTo) s.Add(k, all);
    s.Remove(Stat::AllStats);
  }
  return s;
}

EquipStats Inventory::GearStats() const {
  EquipStats e;
  EquipmentStatBag().AddTo(e);
  return e;
}

int32_t Inventory::EquippedSetCount(std::string_view setId) const {
  int32_t n = 0;
  for (const auto& e : equipment_) {
    if (e.has_value() && !setId.empty() && e->setId == setId) ++n;
  }
  return n;
}

int32_t Inventory::SocketCapacity(EquipSlot slot) const {
  const ItemInstance* it = Equipped(slot);
  if (it == nullptr || data_->Items().FindBase(it->baseId) == nullptr) return 0;
  return ItemSocketCapacity(*it, *data_);
}

InvResult Inventory::SocketGem(EquipSlot slot, std::string_view gemUid) {
  ItemInstance* eq = EquippedMutable(slot);
  if (eq == nullptr) return InvResult::UnknownItem;
  if (data_->Items().FindBase(eq->baseId) == nullptr) return InvResult::UnknownItem;
  if (static_cast<int32_t>(eq->sockets.size()) >= ItemSocketCapacity(*eq, *data_)) return InvResult::NoSockets;
  const int32_t gi = BagIndex(gemUid);
  if (gi < 0) return InvResult::UnknownItem;
  ItemInstance& gem = bag_[static_cast<size_t>(gi)];
  const ItemBaseDef* gb = data_->Items().FindBase(gem.baseId);
  if (gb == nullptr || gb->type != ItemType::Gem || !gb->isGem) return InvResult::NotAGem;
  GemInstance g{gb->id, gb->name, gb->gemStat, gb->gemValue, gb->gemTier};
  if (--gem.quantity <= 0) bag_.erase(bag_.begin() + gi);
  eq->sockets.push_back(std::move(g));
  ComputeItemStats(*eq);
  return InvResult::Ok;
}

InvResult Inventory::UnsocketGem(EquipSlot slot, int32_t index, ItemUidGenerator& uids) {
  ItemInstance* eq = EquippedMutable(slot);
  if (eq == nullptr) return InvResult::UnknownItem;
  if (index < 0 || index >= static_cast<int32_t>(eq->sockets.size())) return InvResult::InvalidIndex;
  const GemInstance gem = eq->sockets[static_cast<size_t>(index)];
  const ItemBaseDef* gb = data_->Items().FindBase(gem.gemId);
  const int32_t cap = gb != nullptr ? InvMaxStack(gb) : 10;  // web default maxStack 10 for gems
  ItemInstance* stack = nullptr;
  for (ItemInstance& it : bag_) {
    if (it.baseId == gem.gemId && it.quantity < cap) {
      stack = &it;
      break;
    }
  }
  if (stack == nullptr && IsFull()) return InvResult::BagFull;  // FIX Q19: a stackable gem still fits
  eq->sockets.erase(eq->sockets.begin() + index);
  if (stack != nullptr) {
    ++stack->quantity;
  } else {
    ItemInstance g;
    g.uid = uids.Next();
    g.baseId = gem.gemId;
    g.name = gem.name;
    g.quality = ItemQuality::Normal;
    g.level = 1;
    g.identified = true;
    g.quantity = 1;
    bag_.push_back(std::move(g));
  }
  ComputeItemStats(*eq);
  return InvResult::Ok;
}

InvResult Inventory::MoveToStash(std::string_view uid, int32_t capacity) {
  if (static_cast<int32_t>(stash_.size()) >= capacity) return InvResult::StashFull;
  std::optional<ItemInstance> it = TakeEntry(uid);
  if (!it.has_value()) return InvResult::UnknownItem;
  stash_.push_back(std::move(*it));
  return InvResult::Ok;
}

InvResult Inventory::MoveFromStash(std::string_view uid) {
  for (size_t i = 0; i < stash_.size(); ++i) {
    if (stash_[i].uid != uid) continue;
    if (!AddItem(stash_[i]).ok) return InvResult::BagFull;
    stash_.erase(stash_.begin() + static_cast<std::ptrdiff_t>(i));
    return InvResult::Ok;
  }
  return InvResult::UnknownItem;
}

void Inventory::PushStashOverflow(ItemInstance item) { stash_.push_back(std::move(item)); }

const ItemInstance* Inventory::FindInStash(std::string_view uid) const {
  for (const ItemInstance& it : stash_) {
    if (it.uid == uid) return &it;
  }
  return nullptr;
}

void Inventory::SortStash() { InvSort(*data_, stash_); }

std::optional<int64_t> Inventory::Sell(std::string_view uid) {
  const int32_t idx = BagIndex(uid);
  if (idx < 0) return std::nullopt;
  const EconomyDef& e = data_->Items().economy;
  const int64_t price = ItemSellPrice(bag_[static_cast<size_t>(idx)], *data_);
  BuybackEntry entry;
  entry.price = static_cast<int64_t>(JsRound(static_cast<double>(price) * e.buybackPriceMultiplier));
  entry.item = std::move(bag_[static_cast<size_t>(idx)]);
  bag_.erase(bag_.begin() + idx);
  buyback_.push_back(std::move(entry));
  const size_t slots = static_cast<size_t>((std::max)(0, e.buybackSlots));
  while (buyback_.size() > slots) buyback_.erase(buyback_.begin());  // FIFO: keeps the most recent
  return price;
}

InvResult Inventory::Buyback(int32_t index, int64_t& outCost, ItemInstance& outItem) {
  if (index < 0 || index >= static_cast<int32_t>(buyback_.size())) return InvResult::InvalidIndex;
  if (IsFull()) return InvResult::BagFull;
  BuybackEntry entry = std::move(buyback_[static_cast<size_t>(index)]);
  buyback_.erase(buyback_.begin() + index);
  outCost = entry.price;
  outItem = entry.item;
  bag_.push_back(std::move(entry.item));  // no stacking, uid preserved
  return InvResult::Ok;
}

void Inventory::Clear() {
  bag_.clear();
  for (auto& e : equipment_) e.reset();
  stash_.clear();
  buyback_.clear();
}

// ---- InventorySystem (runtime) ----
InventorySystem::InventorySystem(SimContext& ctx) : ctx_(ctx), inv_(ctx.data) {}

bool InventorySystem::HeroDying() const {
  return ctx_.sys.hero != nullptr && ctx_.sys.hero->Life() != HeroLife::Alive;
}

void InventorySystem::BagChanged() { ctx_.events.Emit(EvInventoryChanged{}); }

void InventorySystem::EquipmentChanged(EquipSlot slot) {
  ctx_.events.Emit(EvEquipmentChanged{slot});
  ctx_.events.Emit(EvInventoryChanged{});
  ctx_.bus.Publish(EquipStatsDirtyMsg{});
}

InvResult InventorySystem::Equip(std::string_view uid) {
  if (HeroDying()) return InvResult::Dying;
  const int32_t level = ctx_.sys.hero != nullptr ? ctx_.sys.hero->Level() : 1;
  EquipSlot slot = EquipSlot::Weapon;
  const InvResult r = inv_.Equip(uid, level, &slot);
  if (r == InvResult::LevelTooLow) {
    // No dedicated key exists yet (sys.inventory.levelTooLow would be a port key): the hero-level message.
    ctx_.events.Log(MakeLoc("homestead.workshop.block.level"), LogType::System);
    ctx_.events.Sfx(SfxId::Error);
  }
  if (r != InvResult::Ok) return r;
  if (const ItemInstance* it = inv_.Equipped(slot)) {
    ctx_.events.Log(MakeLoc("sys.inventory.equipped", {ItemNameArg("name", *it, ctx_.data)}), LogType::Info);
  }
  EquipmentChanged(slot);
  return r;
}

InvResult InventorySystem::Unequip(EquipSlot slot) {
  if (HeroDying()) return InvResult::Dying;
  const InvResult r = inv_.Unequip(slot);
  if (r == InvResult::BagFull) ctx_.events.Log(MakeLoc("sys.inventory.bagFull"), LogType::System);
  if (r == InvResult::Ok) EquipmentChanged(slot);
  return r;
}

InvResult InventorySystem::UseItem(std::string_view uid) {
  if (HeroDying()) return InvResult::Dying;
  const ItemInstance* it = inv_.FindInBag(uid);
  if (it == nullptr) return InvResult::UnknownItem;
  const ItemTables& t = ctx_.data.Items();
  const ItemBaseDef* base = t.FindBase(it->baseId);
  if (base == nullptr || (base->type != ItemType::Consumable && base->type != ItemType::Scroll)) {
    return InvResult::NotUsable;
  }
  if (t.IsRemovedItem(base->id)) return InvResult::NotUsable;  // I4: the TP scroll is gone (the portal is free)
  Hero* hero = ctx_.sys.hero;
  switch (base->consumableEffect) {
    case ConsumableEffect::Heal:
      if (hero != nullptr) hero->Heal(base->consumableValue);
      break;
    case ConsumableEffect::Mana:
      if (hero != nullptr) hero->RestoreMana(base->consumableValue);
      break;
    case ConsumableEffect::Antidote:
      // FIX Q8 (I4): the antidote cleanses poison.
      if (ctx_.sys.status != nullptr && ctx_.sys.status->Remove(kHeroEntityId, StatusType::Poison)) {
        ctx_.events.Emit(EvStatusExpired{kHeroEntityId, StatusType::Poison});
      }
      break;
    case ConsumableEffect::Teleport:
    case ConsumableEffect::None:
      return InvResult::NotUsable;  // not consumed
  }
  inv_.RemoveItem(uid, 1);
  BagChanged();
  return InvResult::Ok;
}

InvResult InventorySystem::UsePotionSlot(PotionSlot slot) {
  if (HeroDying()) return InvResult::Dying;
  const std::string baseId = ResolvePotionSlot(slot);
  if (baseId.empty()) return InvResult::NotUsable;
  for (const ItemInstance& it : inv_.Bag()) {
    if (it.baseId == baseId && it.quantity > 0) {
      const std::string uid = it.uid;
      return UseItem(uid);
    }
  }
  return InvResult::NotUsable;
}

void InventorySystem::SetPotionSlot(PotionSlot slot, std::string_view baseId) {
  potionSlots_[static_cast<size_t>(slot)] = std::string(baseId);
}

std::string InventorySystem::ResolvePotionSlot(PotionSlot slot) const {
  const std::string& bound = potionSlots_[static_cast<size_t>(slot)];
  if (!bound.empty()) return bound;
  const ConsumableEffect wanted = slot == PotionSlot::Hp ? ConsumableEffect::Heal : ConsumableEffect::Mana;
  const ItemBaseDef* best = nullptr;
  for (const ItemInstance& it : inv_.Bag()) {
    const ItemBaseDef* b = ctx_.data.Items().FindBase(it.baseId);
    if (b == nullptr || b->consumableEffect != wanted || it.quantity <= 0) continue;
    if (best == nullptr || b->consumableValue > best->consumableValue) best = b;
  }
  return best != nullptr ? best->id : std::string();
}

PotionSlotView InventorySystem::PotionSlotState(PotionSlot slot) const {
  PotionSlotView v;
  v.bound = !potionSlots_[static_cast<size_t>(slot)].empty();
  v.baseId = ResolvePotionSlot(slot);
  v.count = v.baseId.empty() ? 0 : inv_.CountOf(v.baseId);
  return v;
}

InvResult InventorySystem::Discard(std::string_view uid) {
  if (HeroDying()) return InvResult::Dying;
  std::optional<ItemInstance> it = inv_.TakeEntry(uid);
  if (!it.has_value()) return InvResult::UnknownItem;
  ctx_.events.Log(MakeLoc("sys.inventory.discarded", {ItemNameArg("name", *it, ctx_.data)}), LogType::System);
  BagChanged();
  return InvResult::Ok;
}

int32_t InventorySystem::DestroyNormals() {
  if (HeroDying()) return 0;
  const int32_t n = inv_.DestroyNormalItems();
  if (n > 0) {
    ctx_.events.Log(MakeLoc("sys.inventory.bulkDestroy", {{"count", ToStr(n)}}), LogType::System);
    BagChanged();
  }
  return n;
}

void InventorySystem::SortBag() {
  if (HeroDying()) return;
  inv_.SortBag();
  BagChanged();
}

void InventorySystem::SortStash() {
  if (HeroDying()) return;
  inv_.SortStash();
  ctx_.events.Emit(EvStashChanged{});
}

InvResult InventorySystem::SocketGem(EquipSlot slot, std::string_view gemUid) {
  if (HeroDying()) return InvResult::Dying;
  const ItemInstance* gem = inv_.FindInBag(gemUid);
  const std::string gemBase = gem != nullptr ? gem->baseId : std::string();
  const InvResult r = inv_.SocketGem(slot, gemUid);
  if (r == InvResult::NoSockets) ctx_.events.Log(MakeLoc("sys.inventory.gem.noSlots"), LogType::System);
  if (r != InvResult::Ok) return r;
  ctx_.events.Log(MakeLoc("sys.inventory.gem.socketed", {KeyArg("gemName", "data.item." + gemBase + ".name")}),
                  LogType::Info);
  EquipmentChanged(slot);
  return r;
}

InvResult InventorySystem::UnsocketGem(EquipSlot slot, int32_t index) {
  if (HeroDying()) return InvResult::Dying;
  const ItemInstance* eq = inv_.Equipped(slot);
  std::string gemBase;
  if (eq != nullptr && index >= 0 && index < static_cast<int32_t>(eq->sockets.size())) {
    gemBase = eq->sockets[static_cast<size_t>(index)].gemId;
  }
  const InvResult r = inv_.UnsocketGem(slot, index, uids_);
  if (r == InvResult::BagFull) ctx_.events.Log(MakeLoc("sys.inventory.gem.bagFullRemove"), LogType::System);
  if (r != InvResult::Ok) return r;
  ctx_.events.Log(MakeLoc("sys.inventory.gem.removed", {KeyArg("gemName", "data.item." + gemBase + ".name")}),
                  LogType::Info);
  EquipmentChanged(slot);
  return r;
}

void InventorySystem::OpenStash(std::string_view npcId) {
  stash_.open = true;
  stash_.npcId = std::string(npcId);
}

void InventorySystem::CloseStash() { stash_ = StashSession{}; }

InvResult InventorySystem::StashPut(std::string_view uid) {
  if (!stash_.open) return InvResult::StashClosed;
  if (HeroDying()) return InvResult::Dying;
  const InvResult r = inv_.MoveToStash(uid, StashCapacity());
  if (r == InvResult::StashFull) ctx_.events.Log(MakeLoc("sys.inventory.stashFull"), LogType::System);
  if (r != InvResult::Ok) return r;
  ctx_.events.Emit(EvInventoryChanged{});
  ctx_.events.Emit(EvStashChanged{});
  return r;
}

InvResult InventorySystem::StashTake(std::string_view uid) {
  if (!stash_.open) return InvResult::StashClosed;
  if (HeroDying()) return InvResult::Dying;
  const InvResult r = inv_.MoveFromStash(uid);
  if (r == InvResult::BagFull) ctx_.events.Log(MakeLoc("ui.stash.bagFull"), LogType::System);
  if (r != InvResult::Ok) return r;
  ctx_.events.Emit(EvInventoryChanged{});
  ctx_.events.Emit(EvStashChanged{});
  return r;
}

int32_t InventorySystem::StashCapacity() const {
  // 80 + homestead stashSlots (warehouse levels; loot 10).
  const double extra = ctx_.sys.homestead != nullptr ? ctx_.sys.homestead->TotalBonuses().Get(Stat::StashSlots) : 0.0;
  const int32_t base = ctx_.data.Items().economy.stashBaseSlots;
  return (base > 0 ? base : kBaseStashSlots) + (std::max)(0, SaturatingInt32(extra));
}

ItemGrantOutcome InventorySystem::Grant(ItemInstance& item, OverflowPolicy policy, ItemSource source) {
  if (item.uid.empty()) item.uid = uids_.Next();
  if (item.quantity < 1) item.quantity = 1;
  if (inv_.CanAdd(item)) {
    const AddResult r = inv_.AddItem(item);  // copy: `item` stays intact if the add fails
    if (r.ok) {
      if (r.newEntry) {
        ctx_.events.Log(MakeLoc("sys.inventory.obtained", {ItemNameArg("name", item, ctx_.data, true)}),
                        LogType::Loot);
      } else {
        ctx_.events.Log(MakeLoc("sys.inventory.obtainedQty",
                                {ItemNameArg("name", item, ctx_.data), {"qty", ToStr(r.stackedQuantity)}}),
                        LogType::Loot);
      }
      if (source == ItemSource::Pickup) {
        ctx_.bus.Publish(ItemPickedMsg{item.uid, item.baseId, item.quality});
        ctx_.events.Emit(EvItemPicked{item.uid, item.baseId, item.quality, item.quantity});
      }
      item = ItemInstance{};
      BagChanged();
      return ItemGrantOutcome::Bag;
    }
  }
  switch (policy) {
    case OverflowPolicy::Refuse:
      return ItemGrantOutcome::Refused;
    case OverflowPolicy::Stash:
      inv_.PushStashOverflow(std::move(item));
      item = ItemInstance{};
      ctx_.events.Emit(EvStashChanged{});
      return ItemGrantOutcome::Stash;
    case OverflowPolicy::Lose:
      break;
  }
  ctx_.events.Log(MakeLoc("sys.inventory.bagFull"), LogType::System);
  item = ItemInstance{};
  return ItemGrantOutcome::Lost;
}

void InventorySystem::FillSnapshot(Snapshot& out) const {
  out.inventory = &inv_;
  out.stash = &stash_;
  out.stashCapacity = StashCapacity();
  for (size_t i = 0; i < out.potionSlots.size(); ++i) out.potionSlots[i] = PotionSlotState(static_cast<PotionSlot>(i));
}

void InventorySystem::WriteSave(SaveData& out) const {
  out.inventory.assign(inv_.Bag().begin(), inv_.Bag().end());
  out.equipment = inv_.Equipment();
  out.stash.assign(inv_.Stash().begin(), inv_.Stash().end());
  out.itemUidCounter = uids_.Counter();
}

void InventorySystem::ReadSave(const SaveData& in) {
  inv_.Clear();
  stash_ = StashSession{};
  inv_.MutableBag() = in.inventory;
  inv_.MutableEquipment() = in.equipment;
  inv_.MutableStash() = in.stash;

  // The counter never re-issues a uid that is already in the save.
  uint64_t next = (std::max)(uint64_t{1}, in.itemUidCounter);
  auto bump = [&next](const ItemInstance& it) {
    const uint64_t v = ItemUidCounterValue(it.uid);
    if (v >= next) next = v + 1;
  };
  for (const ItemInstance& it : inv_.Bag()) bump(it);
  for (const auto& e : inv_.Equipment()) {
    if (e.has_value()) bump(*e);
  }
  for (const ItemInstance& it : inv_.Stash()) bump(it);
  uids_.SetCounter(next);

  const ItemTables& t = ctx_.data.Items();
  std::set<std::string> seen;
  auto normalise = [&](ItemInstance& it) {
    it.identified = true;  // save-ui-input 3.3 / loot 7.6
    if (it.quantity < 1) it.quantity = 1;
    if (it.level < 1) it.level = 1;
    if (it.uid.empty() || seen.count(it.uid) != 0) it.uid = uids_.Next();
    seen.insert(it.uid);
    if (it.quality == ItemQuality::Legendary && it.legendaryId.empty()) {
      const LegendaryDef* l = t.FindLegendaryForBase(it.baseId);
      if (l != nullptr && (it.name == l->name || it.legendaryEffect == l->specialEffectDescription)) {
        it.legendaryId = l->id;
      }
    }
    if (it.quality == ItemQuality::Set && !it.setId.empty() && it.setPieceId.empty()) {
      if (const SetDef* s = t.FindSet(it.setId)) {
        for (const std::string& pieceId : s->pieces) {
          const SetPieceBase* pb = t.FindSetPiece(pieceId);
          if (pb != nullptr && pb->baseId == it.baseId) {
            it.setPieceId = pieceId;
            break;
          }
        }
      }
    }
    ComputeItemStats(it);
  };
  for (ItemInstance& it : inv_.MutableBag()) normalise(it);
  for (auto& e : inv_.MutableEquipment()) {
    if (e.has_value()) normalise(*e);
  }
  for (ItemInstance& it : inv_.MutableStash()) normalise(it);
}

}  // namespace abyss
