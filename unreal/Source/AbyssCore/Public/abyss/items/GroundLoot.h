// Items and potions on the ground: drops, despawn timers, click pickup, auto-loot, potion auto-collect.
// Spec: loot-items-inventory.md 5.1 (kill -> loot, potion pickups), 6.1-6.5 (ground items, potions, click pickup,
// auto-loot, feedback); world-map-nav.md 12.3; DECISIONS I1 (loot luck), I10 (FIX Q22: auto-pickup on arrival,
// walk-then-act W8), C12 (pickups pause while Dying), D13 T10 (despawn timers on the sim clock).
//
// Owner area: items. Runtime system (SimContext). Timers: TimerOwner::Items. Ground entities: EntityKind::GroundItem /
// PotionDrop (EvLootDropped / EvPotionDropped on spawn, EvEntityDespawned with PickedUp / Collected / Expired /
// ZoneUnload on removal). Not saved; cleared on zone exit. RNG: RngStream::Loot.
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
#include "abyss/sim/SimTypes.h"

namespace abyss {

struct SimContext;
struct Snapshot;

enum class GroundLootTimerKind : uint16_t { ItemDespawn = 1, PotionDespawn = 2 };

struct GroundItem {
  EntityId id = kNoEntity;
  ItemInstance item;
  Vec2 pos;            // logical tile position (monster tile / chest tile)
  // Render offset in tiles, drawn from RngStream::Loot: monster drops +U[0, 0.5) per axis (dropLoot); treasure-cache
  // drops the web's screen jitter (U - 0.5) x 20 px / (U - 0.5) x 10 px converted from iso px to tiles.
  Vec2 visualOffset;
  double droppedAtMs = 0;
  bool despawns = true;    // treasure-cache drops do not
  bool cacheDrop = false;  // dropLootAtPosition (treasure cache): fall-in, no ITEM_DROPPED feedback (6.1 / 6.5)
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

  // dropLoot (6.1, despawns = true): 60 s despawn timer, EvLootDropped (UE plays the 6.5 legendary / set flash + shake).
  // dropLootAtPosition (despawns = false, treasure caches only): no despawn timer and EvLootDropped with cacheDrop = true
  // and fallInMs (the web emitted no ITEM_DROPPED for it, so no flash / shake) and the cache's own jitter.
  // Both draw the two visual-offset values from RngStream::Loot.
  EntityId Drop(ItemInstance item, Vec2 pos, bool despawns = true);
  // dropPotion (6.2): 30 s despawn, EvPotionDropped.
  EntityId DropPotion(PotionKind kind, int32_t amount, Vec2 pos);

  // Kill pipeline (loot 5.1): ley-fruit roll, generateLoot with LootLuck() and the session difficulty, potions vs items.
  void OnMonsterKilled(const MonsterKilledMsg& m);
  // Loot luck (5.1 + I1): raw hero lck + homestead building magicFind + active pet magicFind + gear lck + gear
  // magicFind (the altar blessing is not part of it, loot Q3). Monster kills.
  double LootLuck() const;
  // Treasure-cache luck (5.5 + I1): the web passed raw player.stats.lck only (no homestead / pet magicFind); I1 adds
  // gear lck + gear magicFind. RandomEventSystem's treasure cache should roll with this, not LootLuck().
  double TreasureCacheLuck() const;

  // Per step: potion auto-collect within 2 tiles (6.2; alive hero only, C12), auto-loot every 300 ms (6.4).
  void Tick();
  void OnTimer(const Timer& t);
  void OnZoneExit();

  // Click pickup (6.3 + Q22 fix): true when the drop was picked up now (in range, bag has room, hero alive). The caller
  // (ZoneRuntime) walks to an out-of-range drop (InPickupRange false) and calls TryPickUp again on arrival. A full bag
  // logs sys.inventory.bagFull and leaves the drop on the ground - minus the units a partial stack took (7.2 / 20.8:
  // the web's addItem topped up the first partial stack and left the rest on the ground). Auto-loot does the same.
  bool TryPickUp(EntityId drop);
  bool InPickupRange(EntityId drop) const;  // distSq(hero, drop) <= 4
  // findLootAt (6.3): first ground item with |dcol| < 1.5 && |drow| < 1.5 in drop order; kNoEntity when none.
  EntityId LootAt(Vec2 tile) const;
  const GroundItem* Find(EntityId id) const;
  std::span<const GroundItem> Items() const { return items_; }
  std::span<const PotionDrop> Potions() const { return potions_; }

  void FillSnapshot(Snapshot& out) const;

 private:
  void AutoLoot();
  // Grants a copy of the drop at `index`; false keeps the drop with the quantity the bag did not take.
  bool GrantPickupAt(size_t index);
  double HeroAndGearLuck() const;
  void RemoveItemAt(size_t index, DespawnReason reason);
  void RemovePotionAt(size_t index, DespawnReason reason);

  SimContext& ctx_;
  std::vector<GroundItem> items_;  // drop order
  std::vector<PotionDrop> potions_;
  double lastAutoLootMs_ = 0;      // web lastAutoLootCheck: a scan runs when now - last > 300
  bool autoLootBlocked_ = false;   // bag-full log is edge-triggered (the web logged it every 300 ms)
};

}  // namespace abyss
