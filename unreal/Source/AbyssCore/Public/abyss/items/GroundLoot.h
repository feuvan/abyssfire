// Items and potions on the ground: drops, despawn timers, click pickup, auto-loot, potion auto-collect.
// Spec: loot-items-inventory.md 5.1 (kill -> loot, potion pickups), 6.1-6.5 (ground items, potions, click pickup,
// auto-loot, feedback); world-map-nav.md 12.3; DECISIONS I10 (FIX Q22: auto-pickup on arrival, walk-then-act W8),
// D13 T10 (despawn timers on the sim clock).
//
// Owner area: items. Runtime system (SimContext). Timers: TimerOwner::Items. Ground entities: EntityKind::GroundItem /
// PotionDrop. Not saved; cleared on zone exit.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Timers.h"
#include "abyss/base/Types.h"
#include "abyss/data/ItemData.h"
#include "abyss/items/Item.h"
#include "abyss/sim/GameplayBus.h"

namespace abyss {

struct SimContext;
struct Snapshot;

enum class GroundLootTimerKind : uint16_t { ItemDespawn = 1, PotionDespawn = 2 };

struct GroundItem {
  EntityId id = kNoEntity;
  ItemInstance item;
  Vec2 pos;            // logical tile position (monster tile)
  Vec2 visualOffset;   // +U[0, 0.5) tiles (render only; drawn from RngStream::Loot)
  double droppedAtMs = 0;
  bool despawns = true;  // treasure-cache drops do not
  TimerId timer = kNoTimer;
};

struct PotionDrop {
  EntityId id = kNoEntity;
  PotionKind kind = PotionKind::Hp;
  int32_t amount = 0;
  Vec2 pos;
  TimerId timer = kNoTimer;
};

class ABYSS_API GroundLootSystem {
 public:
  explicit GroundLootSystem(SimContext& ctx);

  // dropLoot (6.1): 60 s despawn, EvLootDropped (legendary/set flash + shake are presentation rules on the event).
  EntityId Drop(ItemInstance item, Vec2 pos, bool despawns = true);
  // dropPotion (6.2): 30 s despawn.
  EntityId DropPotion(PotionKind kind, int32_t amount, Vec2 pos);

  // Kill pipeline (loot 5.1): ley-fruit roll, generateLoot with the merged luck, potions vs items.
  void OnMonsterKilled(const MonsterKilledMsg& m);

  // Per step: potion auto-collect within 2 tiles (6.2; alive hero only, C12), auto-loot every 300 ms (6.4).
  void Tick();
  void OnTimer(const Timer& t);
  void OnZoneExit();

  // Click pickup (6.3 + Q22 fix): in range -> pick up now; else the caller walks and calls TryPickUp on arrival.
  bool TryPickUp(EntityId drop);
  // findLootAt (6.3): first ground item with |dcol| < 1.5 && |drow| < 1.5 in drop order.
  EntityId LootAt(Vec2 tile) const;
  const GroundItem* Find(EntityId id) const;
  std::span<const GroundItem> Items() const { return items_; }
  std::span<const PotionDrop> Potions() const { return potions_; }

  void FillSnapshot(Snapshot& out) const;

 private:
  SimContext& ctx_;
  std::vector<GroundItem> items_;  // drop order
  std::vector<PotionDrop> potions_;
  double nextAutoLootMs_ = 0;
};

}  // namespace abyss
