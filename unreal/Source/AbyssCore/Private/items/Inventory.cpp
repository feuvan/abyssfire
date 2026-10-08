// Bag, equipment, stash, buyback, gear stats (loot-items-inventory.md 7-10, 12.4-12.5, 16; I3-I10). STUB: owner area
// items. Lookups and save plumbing are real; the rules are stubs.
#include "abyss/base/Platform.h"

#include "abyss/items/Inventory.h"

#include <algorithm>

#include "abyss/base/Assert.h"
#include "abyss/base/Math.h"
#include "abyss/data/DataStore.h"
#include "abyss/pets/Homestead.h"
#include "abyss/save/SaveData.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

// ---- Inventory (pure) ----
Inventory::Inventory(const DataStore& data) : data_(&data) {}

AddResult Inventory::AddItem(ItemInstance item) {
  ABYSS_UNIMPLEMENTED();
  (void)data_;
  return {};
}

bool Inventory::CanAdd(const ItemInstance& item) const {
  ABYSS_UNIMPLEMENTED();
  return !IsFull();
}

std::optional<ItemInstance> Inventory::RemoveItem(std::string_view uid, int32_t qty) {
  ABYSS_UNIMPLEMENTED();
  return std::nullopt;
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
    if (it.baseId == baseId) n += it.quantity;
  }
  return n;
}

void Inventory::SortBag() { ABYSS_UNIMPLEMENTED(); }

int32_t Inventory::DestroyNormalItems() {
  ABYSS_UNIMPLEMENTED();
  return 0;
}

InvResult Inventory::Equip(std::string_view uid, int32_t heroLevel) {
  ABYSS_UNIMPLEMENTED();
  return InvResult::UnknownItem;
}

InvResult Inventory::Unequip(EquipSlot slot) {
  ABYSS_UNIMPLEMENTED();
  return InvResult::UnknownItem;
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
  ABYSS_UNIMPLEMENTED();
  return {};
}

EquipStats Inventory::GearStats() const {
  ABYSS_UNIMPLEMENTED();
  return {};
}

int32_t Inventory::EquippedSetCount(std::string_view setId) const {
  int32_t n = 0;
  for (const auto& e : equipment_) {
    if (e.has_value() && !setId.empty() && e->setId == setId) ++n;
  }
  return n;
}

int32_t Inventory::SocketCapacity(EquipSlot slot) const {
  ABYSS_UNIMPLEMENTED();
  return 0;
}

InvResult Inventory::SocketGem(EquipSlot slot, std::string_view gemUid) {
  ABYSS_UNIMPLEMENTED();
  return InvResult::NoSockets;
}

InvResult Inventory::UnsocketGem(EquipSlot slot, int32_t index, ItemUidGenerator& uids) {
  ABYSS_UNIMPLEMENTED();
  return InvResult::InvalidIndex;
}

InvResult Inventory::MoveToStash(std::string_view uid, int32_t capacity) {
  ABYSS_UNIMPLEMENTED();
  return InvResult::UnknownItem;
}

InvResult Inventory::MoveFromStash(std::string_view uid) {
  ABYSS_UNIMPLEMENTED();
  return InvResult::UnknownItem;
}

void Inventory::PushStashOverflow(ItemInstance item) { stash_.push_back(std::move(item)); }

void Inventory::SortStash() { ABYSS_UNIMPLEMENTED(); }

std::optional<int64_t> Inventory::Sell(std::string_view uid) {
  ABYSS_UNIMPLEMENTED();
  return std::nullopt;
}

InvResult Inventory::Buyback(int32_t index, int64_t& outCost, ItemInstance& outItem) {
  ABYSS_UNIMPLEMENTED();
  return InvResult::InvalidIndex;
}

void Inventory::Clear() {
  bag_.clear();
  for (auto& e : equipment_) e.reset();
  stash_.clear();
  buyback_.clear();
}

// ---- InventorySystem (runtime) ----
InventorySystem::InventorySystem(SimContext& ctx) : ctx_(ctx), inv_(ctx.data) {}

InvResult InventorySystem::Equip(std::string_view uid) {
  ABYSS_UNIMPLEMENTED();
  return InvResult::UnknownItem;
}

InvResult InventorySystem::Unequip(EquipSlot slot) {
  ABYSS_UNIMPLEMENTED();
  return InvResult::UnknownItem;
}

InvResult InventorySystem::UseItem(std::string_view uid) {
  ABYSS_UNIMPLEMENTED();
  return InvResult::NotUsable;
}

InvResult InventorySystem::UsePotionSlot(PotionSlot slot) {
  ABYSS_UNIMPLEMENTED();
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
  ABYSS_UNIMPLEMENTED();
  return InvResult::UnknownItem;
}

int32_t InventorySystem::DestroyNormals() {
  ABYSS_UNIMPLEMENTED();
  return 0;
}

void InventorySystem::SortBag() { ABYSS_UNIMPLEMENTED(); }

void InventorySystem::SortStash() { ABYSS_UNIMPLEMENTED(); }

InvResult InventorySystem::SocketGem(EquipSlot slot, std::string_view gemUid) {
  ABYSS_UNIMPLEMENTED();
  return InvResult::NoSockets;
}

InvResult InventorySystem::UnsocketGem(EquipSlot slot, int32_t index) {
  ABYSS_UNIMPLEMENTED();
  return InvResult::InvalidIndex;
}

void InventorySystem::OpenStash(std::string_view npcId) {
  stash_.open = true;
  stash_.npcId = std::string(npcId);
}

void InventorySystem::CloseStash() { stash_ = StashSession{}; }

InvResult InventorySystem::StashPut(std::string_view uid) {
  if (!stash_.open) return InvResult::StashClosed;
  ABYSS_UNIMPLEMENTED();
  return InvResult::UnknownItem;
}

InvResult InventorySystem::StashTake(std::string_view uid) {
  if (!stash_.open) return InvResult::StashClosed;
  ABYSS_UNIMPLEMENTED();
  return InvResult::UnknownItem;
}

int32_t InventorySystem::StashCapacity() const {
  // 80 + homestead stashSlots (warehouse levels; loot 10).
  const double extra = ctx_.sys.homestead != nullptr ? ctx_.sys.homestead->TotalBonuses().Get(Stat::StashSlots) : 0.0;
  return kBaseStashSlots + (std::max)(0, SaturatingInt32(extra));
}

ItemGrantOutcome InventorySystem::Grant(ItemInstance& item, OverflowPolicy policy, ItemSource source) {
  if (inv_.CanAdd(item)) {
    const AddResult r = inv_.AddItem(item);  // copy: `item` stays intact if the add fails
    if (r.ok) {
      if (source == ItemSource::Pickup) {
        ctx_.bus.Publish(ItemPickedMsg{item.uid, item.baseId, item.quality});
        ctx_.events.Emit(EvItemPicked{item.uid, item.baseId, item.quality, item.quantity});
      }
      item = ItemInstance{};
      Changed(false);
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
  ctx_.events.Log(MakeLoc("sys.inventory.bagFull"), LogType::Loot);
  item = ItemInstance{};
  return ItemGrantOutcome::Lost;
}

void InventorySystem::Changed(bool equipment) {
  ctx_.events.Emit(EvInventoryChanged{});
  if (equipment) ctx_.bus.Publish(EquipStatsDirtyMsg{});
}

void InventorySystem::FillSnapshot(Snapshot& out) const {
  out.inventory = &inv_;
  out.stash = &stash_;
  out.stashCapacity = StashCapacity();
  for (size_t i = 0; i < out.potionSlots.size(); ++i) out.potionSlots[i] = PotionSlotState(static_cast<PotionSlot>(i));
}

void InventorySystem::WriteSave(SaveData& out) const {
  ABYSS_UNIMPLEMENTED();
  out.itemUidCounter = uids_.Counter();
}

void InventorySystem::ReadSave(const SaveData& in) {
  ABYSS_UNIMPLEMENTED();
  uids_.SetCounter(in.itemUidCounter);
}

}  // namespace abyss
