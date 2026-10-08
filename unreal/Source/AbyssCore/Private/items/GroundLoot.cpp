// Ground items and potion pickups (loot-items-inventory.md 5.1, 6; I10; D13 T10). STUB: owner area items.
#include "abyss/base/Platform.h"

#include "abyss/items/GroundLoot.h"

#include "abyss/base/Assert.h"
#include "abyss/data/DataStore.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

GroundLootSystem::GroundLootSystem(SimContext& ctx) : ctx_(ctx) {}

EntityId GroundLootSystem::Drop(ItemInstance item, Vec2 pos, bool despawns) {
  ABYSS_UNIMPLEMENTED();
  return kNoEntity;
}

EntityId GroundLootSystem::DropPotion(PotionKind kind, int32_t amount, Vec2 pos) {
  ABYSS_UNIMPLEMENTED();
  return kNoEntity;
}

void GroundLootSystem::OnMonsterKilled(const MonsterKilledMsg& m) { ABYSS_UNIMPLEMENTED(); }

void GroundLootSystem::Tick() {
  if (!items_.empty() || !potions_.empty()) ABYSS_UNIMPLEMENTED();
  (void)nextAutoLootMs_;
}

void GroundLootSystem::OnTimer(const Timer& t) { ABYSS_UNIMPLEMENTED(); }

void GroundLootSystem::OnZoneExit() {
  items_.clear();
  potions_.clear();
  ctx_.timers.CancelIf([](const Timer& t) { return t.owner == TimerOwner::Items; });
}

bool GroundLootSystem::TryPickUp(EntityId drop) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

EntityId GroundLootSystem::LootAt(Vec2 tile) const {
  ABYSS_UNIMPLEMENTED();
  return kNoEntity;
}

const GroundItem* GroundLootSystem::Find(EntityId id) const {
  for (const GroundItem& g : items_) {
    if (g.id == id) return &g;
  }
  return nullptr;
}

void GroundLootSystem::FillSnapshot(Snapshot& out) const {
  for (const GroundItem& g : items_) {
    GroundItemView v;
    v.id = g.id;
    v.uid = g.item.uid;
    v.baseId = g.item.baseId;
    v.quality = g.item.quality;
    v.quantity = g.item.quantity;
    v.pos = g.pos;
    v.visualOffset = g.visualOffset;
    out.groundItems.push_back(std::move(v));
  }
  for (const PotionDrop& p : potions_) out.potions.push_back({p.id, p.kind, p.amount, p.pos});
}

}  // namespace abyss
