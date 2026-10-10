// Bag (100-entry list), equipment (10 slots), stash, buyback, gear stat aggregation, consumables and sockets.
// Spec: loot-items-inventory.md 7 (bag: addItem, removal, sort, discard, destroy normals, consumables), 8 (equip /
// unequip, getEquipmentStats, set bonuses), 9 (sockets), 10 (stash), 12.4-12.5 (sell / buyback), 16 (save);
// DECISIONS I3 (levelReq enforced on equip), I4 (HP/MP potion quick slots; antidote cleanses poison; TP scroll removed),
// I5, I7, I9 (quality sell multipliers), I10 (Q4/Q6/Q7/Q13/Q15/Q18/Q19/Q22 fixes), C11 (legendary special effects that
// combat reads are item stats), C12 (no item actions while Dying).
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
  ShopClosed,     // Buy / Sell / Buyback / Craft without an open shop (or a forge action at a merchant)
  NeedScroll,     // identifyItem without a c_id_scroll (vestigial, 7.6)
};
ABYSS_ENUM_STRINGS(InvResult, "ok", "unknownItem", "bagFull", "stashFull", "notEquipment", "levelTooLow", "noSockets",
                   "notAGem", "notUsable", "notEnoughGold", "invalidIndex", "dying", "stashClosed", "shopClosed",
                   "needScroll")

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
  int64_t price = 0;  // sell price * 5
};

struct AddResult {
  bool ok = false;
  int32_t stackedQuantity = 0;  // units merged into an existing stack
  bool newEntry = false;
};

// Pure container. Stack matching by baseId, one partial stack topped up (7.2, Q21 kept), the remainder appended.
class ABYSS_API Inventory {
 public:
  explicit Inventory(const DataStore& data);

  // ---- bag ----
  // addItem (7.2). FIX (port): atomic - when the whole quantity cannot fit (bag full after the top-up) nothing changes
  // and ok = false (the web topped up and then lost track of the remainder).
  AddResult AddItem(ItemInstance item);
  // True iff AddItem would succeed: the first partial stack takes everything, or the bag has a free entry.
  bool CanAdd(const ItemInstance& item) const;
  // removeItem (7.3): quantity > qty -> decrement and return a copy with quantity = qty (a NEW uid from `uids` when
  // given, port fix); else the whole entry. nullopt for an unknown uid.
  std::optional<ItemInstance> RemoveItem(std::string_view uid, int32_t qty = 1, ItemUidGenerator* uids = nullptr);
  std::optional<ItemInstance> TakeEntry(std::string_view uid);                     // whole entry
  const ItemInstance* FindInBag(std::string_view uid) const;
  ItemInstance* FindInBagMutable(std::string_view uid);
  int32_t BagIndex(std::string_view uid) const;
  std::span<const ItemInstance> Bag() const { return bag_; }
  int32_t CountOf(std::string_view baseId) const;  // summed over stacks
  // sortInventory (7.3): stable by quality order, then type order (economy.json sortOrder), then the localised display
  // name (ItemDisplayName in `names`' locale, default the data's current locale; the port rule of 7.3 - the web compared
  // the stored zh name with localeCompare). Names compare by UTF-8 byte order: the core has no collation tables.
  void SortBag(const I18n* names = nullptr);
  int32_t DestroyNormalItems();  // 7.3: normal weapons / armour / accessories
  int32_t Capacity() const;  // economy.json bagCapacity (100 entries)
  bool IsFull() const { return static_cast<int32_t>(bag_.size()) >= Capacity(); }
  // identifyItem (7.6, vestigial): consumes one c_id_scroll. UnknownItem / NotUsable (already identified) / NeedScroll.
  InvResult IdentifyItem(std::string_view uid);

  // ---- equipment ----
  // equip (8.1) with the port rules: levelReq (I3); rings go to FindCompareTarget's slot (free ring1, free ring2, else
  // the weaker ring, FIX Q18); a swap puts the worn item where the new one was in the bag (FIX Q13: allowed with a
  // full bag). `outSlot` receives the slot used.
  InvResult Equip(std::string_view uid, int32_t heroLevel, EquipSlot* outSlot = nullptr);
  // unequip: appended to the bag; BagFull with 100 entries.
  InvResult Unequip(EquipSlot slot);
  const ItemInstance* Equipped(EquipSlot slot) const;
  ItemInstance* EquippedMutable(EquipSlot slot);
  // getEquipmentStats (8.2) as a StatBag: weapon base damage (weaponDamageMin/Max), armour baseDefense -> defense,
  // item stats (affixes + gems), C11 legendary special effects, set bonuses; allStats expanded into the 6 primaries.
  StatBag EquipmentStatBag() const;
  EquipStats GearStats() const;                // getTypedEquipStats: EquipStats keys only
  int32_t EquippedSetCount(std::string_view setId) const;  // by setId (Q14 kept)

  // ---- sockets (9) ----
  int32_t SocketCapacity(EquipSlot slot) const;  // getMaxSockets: 0 for an empty slot / unknown base
  InvResult SocketGem(EquipSlot slot, std::string_view gemUid);
  // unsocketGem (9.3) with FIX Q19: refused only when the gem needs a new bag entry and the bag is full.
  InvResult UnsocketGem(EquipSlot slot, int32_t index, ItemUidGenerator& uids);

  // ---- stash (10) ----
  InvResult MoveToStash(std::string_view uid, int32_t capacity);  // whole entry, no stacking in the stash
  InvResult MoveFromStash(std::string_view uid);                  // addItem (stacks into the bag)
  void PushStashOverflow(ItemInstance item);  // quest turn-in / Q5 chest overflow (ignores capacity)
  std::span<const ItemInstance> Stash() const { return stash_; }
  const ItemInstance* FindInStash(std::string_view uid) const;
  void SortStash(const I18n* names = nullptr);  // sortStash: the bag comparator

  // ---- trade (12.4-12.5) ----
  // sellItem: price = ItemSellPrice (I9); pushes buyback (price * 5, FIFO 5); removes the entry. Equipped items cannot be
  // sold (nullopt for a uid not in the bag).
  std::optional<int64_t> Sell(std::string_view uid);
  // buybackItem: invalid index -> InvalidIndex; bag full -> BagFull; appended without stacking (uid preserved); returns
  // the cost (the caller has already checked the gold, BuybackList()[index].price).
  InvResult Buyback(int32_t index, int64_t& outCost, ItemInstance& outItem);
  std::span<const BuybackEntry> BuybackList() const { return buyback_; }

  // ---- whole-state access (save / load, tooltip compare) ----
  using EquipmentArray = std::array<std::optional<ItemInstance>, EnumCount<EquipSlot>()>;
  // Read-only equipment in EquipSlot order (FindCompareTarget, character panel; reachable from Snapshot::inventory).
  const EquipmentArray& Equipment() const { return equipment_; }
  std::vector<ItemInstance>& MutableBag() { return bag_; }
  EquipmentArray& MutableEquipment() { return equipment_; }
  std::vector<ItemInstance>& MutableStash() { return stash_; }
  void ClearBuyback() { buyback_.clear(); }
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
  // 7.4 + I4: heal / mana potions restore the hero (Hero::Heal / RestoreMana); antidote: StatusEffectSystem::Remove(hero,
  // Poison) (keeps the diminishing-returns records) and, when a poison was removed, EvStatusExpired{hero, Poison}.
  // Removed items (TP scroll, I4) and items without an effect (ley fruit, ID scroll) are NotUsable and not consumed.
  InvResult UseItem(std::string_view uid);
  InvResult UsePotionSlot(PotionSlot slot);       // I4: ResolvePotionSlot(slot), one unit of the first stack
  // Binds a quick slot ("" = best available; saved: potionSlots). Only a potion of the slot's kind binds (Hp: a heal
  // consumable, Mp: a mana one); any other id is refused (false) and the slot keeps its binding.
  bool SetPotionSlot(PotionSlot slot, std::string_view baseId);
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
  // then `policy` (OverflowPolicy). Moves from `item` on Bag / Stash / Lost and leaves it untouched on Refused. Logs
  // (sys.inventory.obtained / obtainedQty, bagFull when lost) + EvInventoryChanged / EvStashChanged; a Pickup also
  // publishes ItemPickedMsg and emits EvItemPicked.
  ItemGrantOutcome Grant(ItemInstance& item, OverflowPolicy policy, ItemSource source);

  void FillSnapshot(Snapshot& out) const;
  // Bag, equipment, stash, the uid counter and the potion quick slots (I4).
  void WriteSave(SaveData& out) const;
  // Restores bag / equipment / stash, the uid counter and the potion slots: identified = true, stats recomputed,
  // quantity >= 1, legendaryId / setPieceId filled when missing, empty or duplicate uids re-issued, invalid potion-slot
  // ids dropped (best available); buyback and the stash session cleared.
  void ReadSave(const SaveData& in);

 private:
  bool HeroDying() const;
  void BagChanged();
  void EquipmentChanged(EquipSlot slot);

  SimContext& ctx_;
  Inventory inv_;
  ItemUidGenerator uids_;
  std::array<std::string, 2> potionSlots_{};
  StashSession stash_;
};

}  // namespace abyss
