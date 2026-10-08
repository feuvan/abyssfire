// Bag (100-entry list), equipment (10 slots), stash, buyback, gear stat aggregation, consumables and sockets.
// Spec: loot-items-inventory.md 7 (bag: addItem, removal, sort, discard, destroy normals, consumables), 8 (equip /
// unequip, getEquipmentStats, set bonuses), 9 (sockets), 10 (stash), 12.4-12.5 (sell / buyback), 16 (save);
// DECISIONS I3 (levelReq enforced on equip), I4 (HP/MP potion quick slots; antidote cleanses poison; TP scroll removed),
// I5, I7, I9 (quality sell multipliers), I10 (Q4/Q6/Q7/Q13/Q15/Q18/Q19/Q22 fixes), C12 (no item actions while Dying).
//
// Owner area: items. `Inventory` is the pure container (unit-testable alone); `InventorySystem` is the runtime system
// (SimContext) that wraps it with logs, events, hero effects and EquipStatsDirtyMsg.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Stats.h"
#include "abyss/items/Item.h"
#include "abyss/sim/SimTypes.h"

namespace abyss {

class DataStore;
struct SimContext;
struct Snapshot;
struct SaveData;

inline constexpr int32_t kMaxInventoryEntries = 100;
inline constexpr int32_t kBaseStashSlots = 80;
inline constexpr int32_t kMaxBuyback = 5;

enum class InvResult : uint8_t {
  Ok,
  UnknownItem,
  BagFull,        // sys.inventory.bagFull / swapBagFull
  StashFull,
  NotEquipment,
  LevelTooLow,    // I3
  NoSockets,      // sys.inventory.gem.noSlots
  NotAGem,
  NotUsable,
  NotEnoughGold,
  InvalidIndex,
  Dying,          // C12
  StashClosed,    // StashPut / StashTake without an open stash session (stash keeper NPC, I7)
};

// Result of a reward / pickup grant (InventorySystem::Grant, RewardService::GrantItem).
enum class ItemGrantOutcome : uint8_t { Bag, Stash, Lost, Refused };
ABYSS_ENUM_STRINGS(ItemGrantOutcome, "bag", "stash", "lost", "refused")

// The stash is a modal panel opened only by the stash keeper NPC (I7; QuestWorld::InteractNpc of a `stash` NPC calls
// InventorySystem::OpenStash). Stash transfers are refused while it is closed. It closes with CmdClosePanel{Stash},
// ExitZone and the hero's death.
struct StashSession {
  bool open = false;
  std::string npcId;
};

// HUD potion quick slot (I4) as shown: the bound base (or the best potion of that kind in the bag when unbound) and the
// bag count of that base (0 = dimmed).
struct PotionSlotView {
  std::string baseId;
  int32_t count = 0;
  bool bound = false;  // the player bound this base (CmdSetPotionSlot); false = best available
};

struct BuybackEntry {
  ItemInstance item;
  int64_t price = 0;  // sellPrice * 5
};

struct AddResult {
  bool ok = false;
  int32_t stackedQuantity = 0;  // units merged into an existing stack
  bool newEntry = false;
};

// Pure container. Stack matching by baseId, one partial stack topped up (7.2), the remainder appended.
class ABYSS_API Inventory {
 public:
  explicit Inventory(const DataStore& data);

  // ---- bag ----
  AddResult AddItem(ItemInstance item);
  // FIX (7.2 port): the overflow does not reduce the caller's copy when the add fails.
  bool CanAdd(const ItemInstance& item) const;
  std::optional<ItemInstance> RemoveItem(std::string_view uid, int32_t qty = 1);  // split gets a new uid (uids)
  std::optional<ItemInstance> TakeEntry(std::string_view uid);                     // whole entry
  const ItemInstance* FindInBag(std::string_view uid) const;
  ItemInstance* FindInBagMutable(std::string_view uid);
  int32_t BagIndex(std::string_view uid) const;
  std::span<const ItemInstance> Bag() const { return bag_; }
  int32_t CountOf(std::string_view baseId) const;  // summed over stacks
  void SortBag();      // quality, type, then display name (caller-provided collation: stored zh name for now)
  int32_t DestroyNormalItems();  // 7.3
  bool IsFull() const { return static_cast<int32_t>(bag_.size()) >= kMaxInventoryEntries; }

  // ---- equipment ----
  // equip (8.1): ring rule, levelReq (I3), swap refused when the bag is exactly full (Q13 kept), old item appended.
  InvResult Equip(std::string_view uid, int32_t heroLevel);
  InvResult Unequip(EquipSlot slot);
  const ItemInstance* Equipped(EquipSlot slot) const;
  ItemInstance* EquippedMutable(EquipSlot slot);
  // getEquipmentStats (8.2) as a StatBag (incl. weaponDamageMin/Max, defense, allStats expanded) and the typed bag.
  StatBag EquipmentStatBag() const;
  EquipStats GearStats() const;                // getTypedEquipStats
  int32_t EquippedSetCount(std::string_view setId) const;

  // ---- sockets (9) ----
  int32_t SocketCapacity(EquipSlot slot) const;
  InvResult SocketGem(EquipSlot slot, std::string_view gemUid);
  InvResult UnsocketGem(EquipSlot slot, int32_t index, ItemUidGenerator& uids);

  // ---- stash (10) ----
  InvResult MoveToStash(std::string_view uid, int32_t capacity);
  InvResult MoveFromStash(std::string_view uid);
  void PushStashOverflow(ItemInstance item);  // quest turn-in / Q5 chest overflow (ignores capacity)
  std::span<const ItemInstance> Stash() const { return stash_; }
  void SortStash();

  // ---- trade (12.4-12.5) ----
  // sellItem: price = ItemSellPrice (I9); pushes buyback (FIFO 5); removes the entry. Equipped items cannot be sold.
  std::optional<int64_t> Sell(std::string_view uid);
  // buybackItem: bag full -> BagFull; appended without stacking (uid preserved); returns the cost.
  InvResult Buyback(int32_t index, int64_t& outCost, ItemInstance& outItem);
  std::span<const BuybackEntry> BuybackList() const { return buyback_; }

  // ---- whole-state access (save / load, tooltip compare) ----
  using EquipmentArray = std::array<std::optional<ItemInstance>, EnumCount<EquipSlot>()>;
  // Read-only equipment in EquipSlot order (FindCompareTarget, character panel; reachable from Snapshot::inventory).
  const EquipmentArray& Equipment() const { return equipment_; }
  std::vector<ItemInstance>& MutableBag() { return bag_; }
  EquipmentArray& MutableEquipment() { return equipment_; }
  std::vector<ItemInstance>& MutableStash() { return stash_; }
  void Clear();

 private:
  const DataStore* data_;
  std::vector<ItemInstance> bag_;
  EquipmentArray equipment_{};
  std::vector<ItemInstance> stash_;
  std::vector<BuybackEntry> buyback_;  // session only, not saved
};

// Runtime wrapper: logs (sys.inventory.*), EvInventoryChanged / EvEquipmentChanged / EvStashChanged, EquipStatsDirtyMsg,
// consumables applied to the hero, HUD potion slots (I4), the stash session (I7), item uid generator (saved).
class ABYSS_API InventorySystem {
 public:
  explicit InventorySystem(SimContext& ctx);

  Inventory& Items() { return inv_; }
  const Inventory& Items() const { return inv_; }
  ItemUidGenerator& Uids() { return uids_; }

  // Commands (each rejected while Dying, C12).
  InvResult Equip(std::string_view uid);
  InvResult Unequip(EquipSlot slot);
  // 7.4 + I4. Antidote: StatusEffectSystem::Remove(hero, Poison) (keeps the diminishing-returns records) and, when a
  // poison was removed, EvStatusExpired{hero, Poison}.
  InvResult UseItem(std::string_view uid);
  InvResult UsePotionSlot(PotionSlot slot);       // I4: ResolvePotionSlot(slot), one unit
  void SetPotionSlot(PotionSlot slot, std::string_view baseId);  // "" = best available (saved: potionSlots)
  const std::array<std::string, 2>& PotionSlots() const { return potionSlots_; }
  // The base a quick slot uses now: the bound base, else the bag potion of that kind (Hp = heal, Mp = mana) with the
  // highest restore value (first in bag order on ties); "" when there is none.
  std::string ResolvePotionSlot(PotionSlot slot) const;
  PotionSlotView PotionSlotState(PotionSlot slot) const;
  InvResult Discard(std::string_view uid);
  int32_t DestroyNormals();
  void SortBag();
  void SortStash();
  InvResult SocketGem(EquipSlot slot, std::string_view gemUid);
  InvResult UnsocketGem(EquipSlot slot, int32_t index);
  // Stash session (I7): opened by the stash keeper NPC interaction, closed by CmdClosePanel{Stash}, ExitZone and the
  // hero's death (GameSim). Transfers need an open session (InvResult::StashClosed otherwise).
  void OpenStash(std::string_view npcId);
  void CloseStash();
  const StashSession& Stash() const { return stash_; }
  InvResult StashPut(std::string_view uid);
  InvResult StashTake(std::string_view uid);
  int32_t StashCapacity() const;                  // 80 + homestead stashSlots

  // The one item grant path (RewardService::GrantItem calls it; ground pickups use source Pickup + Refuse): bag first,
  // then `policy` (OverflowPolicy). Moves from `item` on Bag / Stash / Lost and leaves it untouched on Refused. Logs +
  // EvInventoryChanged / EvStashChanged; a Pickup also publishes ItemPickedMsg and emits EvItemPicked.
  ItemGrantOutcome Grant(ItemInstance& item, OverflowPolicy policy, ItemSource source);

  void FillSnapshot(Snapshot& out) const;
  void WriteSave(SaveData& out) const;
  void ReadSave(const SaveData& in);  // forces identified = true, recomputes stats

 private:
  void Changed(bool equipment);

  SimContext& ctx_;
  Inventory inv_;
  ItemUidGenerator uids_;
  std::array<std::string, 2> potionSlots_{};
  StashSession stash_;
};

}  // namespace abyss
