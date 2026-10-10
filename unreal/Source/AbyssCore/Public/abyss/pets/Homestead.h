// Homestead (Ember Tower) state: building levels, embers, tower / wing unlocks from turned-in quests, herb garden,
// caravan expedition, altar blessing, tower return point - plus the pure tower rules (gem workshop, expedition rewards,
// blessing costs) so saves are forward compatible and the later-milestone tower UI only has to call in.
// Spec: quests-story-ch1.md 4.4 (embers: quest turn-in 4.4.1, kills 4.4.2), 4.6 (unlocks triggered by Ch1 turn-ins),
// 4.7 (garden growth, expedition counter), OQ8 (unlock log lines suppressed in milestone 1), OQ10 (the +N kill float is
// shown), Q16 / Q17 quirks kept; save-ui-input.md 3.2-3.3 (homestead save fields and load normalisation);
// docs/homestead-pets.md; web src/data/homestead.ts, src/systems/{HomesteadTower,HomesteadSystem,EmberTower}.ts.
// DECISIONS Q3: embers are earned and saved; the Ember Tower zone, its UI and its unlock log lines are hidden in
// milestone 1 (SimConfig::milestone1 && HomesteadTables::towerHiddenInMilestone1).
//
// Owner area: quests+story+pets. `HomesteadTower` is pure (the web's HomesteadTower + HomesteadSystem building levels);
// `HomesteadSystem` is the runtime wrapper (SimContext): kill / turn-in hooks, timers, logs, tower actions.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Rng.h"
#include "abyss/base/Stats.h"
#include "abyss/data/PetData.h"
#include "abyss/data/QuestData.h"
#include "abyss/sim/GameplayBus.h"

namespace abyss {

class DataStore;
struct SimContext;
struct Snapshot;
struct SaveData;
struct SaveHomestead;

// 4.4.2: elite 5 / mini-boss 3 / affixed 1 / plain 0, first match wins (table values from homestead.json).
ABYSS_API int32_t EmbersForKill(const HomesteadTables& t, bool elite, bool isMiniBoss, int32_t eliteAffixCount);
// 4.4.1: reward.embers ?? (main 2 : side 1).
ABYSS_API int32_t EmbersForQuest(const HomesteadTables& t, QuestCategory category, bool hasRewardEmbers, int32_t rewardEmbers);
// 4.7.1: kills per yield max(6, 16 - 2 lv); capacity lv <= 0 ? 0 : 4 + 4 lv.
ABYSS_API int32_t GardenInterval(const HomesteadTables& t, int32_t level);
ABYSS_API int32_t GardenCapacity(const HomesteadTables& t, int32_t level);
// rollGardenYield (1 or 2 draws): ley fruit when rng < 0.12 + 0.03 lv; else rng < 0.6 -> HP potion (l >= 4, m >= 2,
// else s), otherwise MP potion (m >= 2, else s).
ABYSS_API std::string RollGardenYield(const HomesteadTables& t, int32_t level, Rng& rng);
// buildingStage (homestead.ts): 0 ruin (locked / level 0), 1 restored, 2 thriving (level >= 3, or >= 2 for a wing
// whose maxLevel < 5).
ABYSS_API int32_t BuildingStage(const BuildingDef& def, int32_t level, bool unlocked);

// ---- gem workshop (炉台) ----
// nextGemId: "g_<line>_<n>" -> "g_<line>_<n+1>" when that gem exists in the item data, else "".
ABYSS_API std::string NextGemId(const DataStore& data, std::string_view gemId);
ABYSS_API int32_t MaxCombineTier(int32_t workshopLevel);  // lv <= 0 ? 0 : min(5, lv + 1)
ABYSS_API int64_t GemCombineGold(int32_t targetTier);     // 40 tier^2
enum class GemCombineBlock : uint8_t { None, NoNext, Workshop, Level, Count, Gold };
ABYSS_ENUM_STRINGS(GemCombineBlock, "none", "noNext", "workshop", "level", "count", "gold")
ABYSS_API GemCombineBlock GemCombineBlockFor(const DataStore& data, std::string_view gemId, int32_t have,
                                             int32_t workshopLevel, int32_t playerLevel, int64_t gold);

// ---- caravan expeditions (商队驿站) ----
struct ExpeditionReward {
  int32_t embers = 0;
  int64_t gold = 0;
  std::vector<std::pair<std::string, int32_t>> items;  // itemId -> count
};
// rollExpeditionReward (homestead.ts): embers long 10 + 4 lv / short 4 + 2 lv (lv = max(1, post level)); gold
// round((long 60 : 25) lv + playerLevel (long 12 : 5) (0.8 + rng 0.4)); long: one gem g_<ruby|sapphire|emerald|topaz>_
// <min(3, 1 + floor(L / 15))> (second draw) + 2 ley fruit; short: rng < 0.5 ? ley fruit : (L >= 25 ? hp_l : hp_m).
ABYSS_API ExpeditionReward RollExpeditionReward(const HomesteadTables& t, std::string_view optionId, int32_t postLevel,
                                                int32_t playerLevel, Rng& rng);
enum class ExpeditionBlock : uint8_t { None, Locked, Busy, NoPet, Active, Option };
ABYSS_ENUM_STRINGS(ExpeditionBlock, "none", "locked", "busy", "noPet", "active", "option")

// ---- altar blessings (心焰祭坛) ----
ABYSS_API int32_t BlessingCost(int32_t altarLevel);                    // 10 + 5 max(0, lv - 1)
ABYSS_API StatBag BlessingStatsFor(const HomesteadTables& t, std::string_view id, int32_t altarLevel);  // round(v x (1 + 0.5 (lv - 1)))
enum class BlessingBlock : uint8_t { None, Locked, Embers, Unknown };
ABYSS_ENUM_STRINGS(BlessingBlock, "none", "locked", "embers", "unknown")

struct GardenState {
  int32_t progress = 0;
  std::vector<std::pair<std::string, int32_t>> stock;  // itemId -> count (> 0), insertion order
  bool operator==(const GardenState& o) const = default;
};
struct ExpeditionState {
  std::string petId, optionId;
  int32_t kills = 0, killsRequired = 1;
  double remainingMs = 0;
  bool operator==(const ExpeditionState& o) const = default;
};
struct BlessingState {
  std::string id;
  int32_t level = 1;
  double remainingMs = 0;
  bool operator==(const BlessingState& o) const = default;
};
struct TowerReturn {
  std::string mapId;
  double col = 0, row = 0;
  bool operator==(const TowerReturn& o) const = default;
};

struct ClaimedExpedition {
  ExpeditionReward reward;
  std::string petId;
};

class ABYSS_API HomesteadTower {
 public:
  explicit HomesteadTower(const HomesteadTables& t);

  const HomesteadTables& Tables() const { return *t_; }

  // ---- buildings (HomesteadSystem.buildings) ----
  int32_t BuildingLevel(std::string_view buildingId) const;  // missing = 0
  void SetBuildingLevel(std::string_view buildingId, int32_t level);
  const std::vector<std::pair<std::string, int32_t>>& Buildings() const { return buildings_; }
  void SetBuildings(std::vector<std::pair<std::string, int32_t>> levels) { buildings_ = std::move(levels); }
  // getTotalBonuses: sum over the saved buildings (unknown ids skipped) of bonusPerLevel x level, in building order.
  StatBag BuildingBonuses() const;
  int32_t TrainingGroundBonus() const;  // mercenary exp bonus: training_ground level x 5
  // getUpgradeCost: the next level's {gold, embers}; false when maxed or unknown.
  bool UpgradeCost(std::string_view buildingId, BuildingLevelCost& out) const;
  // canUpgrade: affordable (gold, and the tower's embers), not maxed, unlocked.
  bool CanUpgrade(std::string_view buildingId, int64_t gold) const;
  bool CanUpgrade(std::string_view buildingId, int64_t gold, int32_t embers) const;
  // upgrade: spends the embers, raises the level; returns the gold cost for the caller to deduct (0 = nothing happened).
  int64_t Upgrade(std::string_view buildingId);

  // ---- unlocks (4.6) ----
  // syncUnlocks: remembers the turned-in set; every story wing whose unlockQuest is turned in is raised to Lv1 when it
  // is at 0 (never lowered); returns the wings newly unlocked by this call (building order).
  std::vector<std::string> SyncUnlocks(std::span<const std::string> turnedInQuests);
  bool TowerUnlocked() const;
  bool IsBuildingUnlocked(std::string_view buildingId) const;  // known and (no unlockQuest or it is turned in)

  // ---- embers ----
  int32_t AddEmbers(int32_t n);  // max(0, n); returns the amount added
  int32_t Embers() const { return embers_; }

  // ---- herb garden (4.7.1) ----
  int32_t GardenStockCount() const;
  // A kill anywhere feeds the garden; returns the item grown this kill, or "".
  std::string OnKillGarden(Rng& rng);
  std::vector<std::pair<std::string, int32_t>> Harvest();  // empties the stock
  void ReturnToGarden(std::string_view itemId, int32_t count);

  // ---- caravan expedition (4.7.3) ----
  ExpeditionBlock ExpeditionBlockFor(std::string_view petId, std::span<const std::string> ownedPets,
                                     std::string_view activePet, std::string_view optionId) const;
  bool SendExpedition(std::string_view petId, std::string_view optionId, std::span<const std::string> ownedPets,
                      std::string_view activePet);
  bool IsPetAway(std::string_view petId) const;
  bool ExpeditionDone() const;
  // Brings the pet home and rolls what it found (nullopt while it is still out); the embers go to the purse.
  std::optional<ClaimedExpedition> ClaimExpedition(int32_t playerLevel, Rng& rng);
  bool OnKillExpedition();  // a kill counts; true when this kill brought it home

  // ---- altar blessing ----
  BlessingBlock BlessingBlockFor(std::string_view blessingId) const;
  bool BuyBlessing(std::string_view blessingId);  // replaces the current one
  StatBag BlessingStats() const;

  // ---- time ----
  struct TickResult {
    bool blessingEnded = false;
    bool expeditionReturned = false;
  };
  TickResult Tick(double dtMs);  // play time outside the tower
  bool OnEnterTower();           // entering the tower ends the blessing; true when one was active

  GardenState& Garden() { return garden_; }
  std::optional<ExpeditionState>& Expedition() { return expedition_; }
  std::optional<BlessingState>& Blessing() { return blessing_; }
  std::optional<TowerReturn>& Return() { return towerReturn_; }
  const GardenState& Garden() const { return garden_; }
  const std::optional<ExpeditionState>& Expedition() const { return expedition_; }
  const std::optional<BlessingState>& Blessing() const { return blessing_; }
  const std::optional<TowerReturn>& Return() const { return towerReturn_; }

  // ---- save ----
  // toSave: buildings + embers, garden, expedition, blessing, towerReturn.
  void WriteSave(SaveHomestead& out) const;
  // HomesteadTower.load (save 3.3) on top of the JSON reader's num() normalisation: reset (the turned-in set too), then
  // building levels verbatim, embers, garden (stock entries > 0), expedition only for a known optionId with a petId
  // (killsRequired >= 1), blessing only for a known id with remainingMs > 0 (level >= 1), towerReturn with a mapId.
  void Load(const SaveHomestead& in);
  void Reset();  // a new game: buildings and tower state cleared

 private:
  const HomesteadTables* t_;
  std::vector<std::pair<std::string, int32_t>> buildings_;  // id -> level (save order)
  std::vector<std::string> turnedIn_;                         // last SyncUnlocks set
  int32_t embers_ = 0;
  GardenState garden_;
  std::optional<ExpeditionState> expedition_;
  std::optional<BlessingState> blessing_;
  std::optional<TowerReturn> towerReturn_;
};

class ABYSS_API HomesteadSystem {
 public:
  explicit HomesteadSystem(SimContext& ctx);

  int32_t BuildingLevel(std::string_view buildingId) const { return tower_.BuildingLevel(buildingId); }
  // Building bonuses + blessing stats (the port's merged view; potionDiscount / stashSlots / ... and the blessing's
  // EquipStats keys). The web's homeBonus at the kill (getTotalBonuses) is BuildingBonuses(): the blessing reaches
  // the hero through EquipStats only.
  StatBag TotalBonuses() const;
  StatBag BuildingBonuses() const { return tower_.BuildingBonuses(); }
  StatBag BlessingStats() const { return tower_.BlessingStats(); }  // merged into EquipStats (loot 8.3)
  bool IsPetAway(std::string_view petId) const { return tower_.IsPetAway(petId); }
  // The Ember Tower UI / unlock lines are hidden (Q3 / OQ8): milestone-1 build and the data flag.
  bool TowerHidden() const;
  bool InTower() const;  // the live zone is the tower zone

  // Kill pipeline: embers (EvEmbersGained, fromKill), garden (RngStream::Pets), expedition (4.4.2 / 4.7).
  void OnMonsterKilled(const MonsterKilledMsg& m);
  // Quest turn-in (QuestTurnedInMsg, before exp/gold, quests 4.1): embers (log homestead.log.questEmbers, loot) +
  // SyncUnlocks (wingUnlocked / towerUnlocked lines unless hidden).
  void OnQuestTurnedIn(const QuestTurnedInMsg& m);
  // Expedition / blessing timers (sim time; in the tower only the expedition travels). Blessing end -> log
  // homestead.log.blessingFaded + EquipStatsDirtyMsg; expedition back -> homestead.log.expeditionBack.
  void Tick(double dtMs);
  // Tower zone entry (later milestone wiring): the blessing ends (homestead.log.blessingHome + EquipStatsDirtyMsg).
  void OnEnterTower();

  // ---- tower actions (later milestone; the web gates harvest / combine / expedition / blessing on being in the
  // tower, upgrades on the panel only) ----
  bool UpgradeBuilding(std::string_view buildingId);      // gold via RewardService (GoldReason::Homestead)
  int32_t HarvestGarden();                                 // items into the bag; leftovers return to the garden
  bool CombineGem(std::string_view gemId);                 // 3 -> 1 next tier, gold cost, bag (else stash)
  bool SendExpedition(std::string_view petId, std::string_view optionId);
  bool ClaimExpedition();                                  // embers, gold, items (bag, else stash)
  bool BuyBlessing(std::string_view blessingId);

  const HomesteadTower& Tower() const { return tower_; }
  HomesteadTower& MutableTower() { return tower_; }
  void FillSnapshot(Snapshot& out) const;
  void WriteSave(SaveData& out) const;
  // Tower load (save 3.3), then SyncUnlocks with the already loaded quests (the web's EmberTower constructor: nothing
  // is logged).
  void ReadSave(const SaveData& in);

 private:
  std::vector<std::string> TurnedInQuests() const;
  std::vector<std::string> SyncUnlocks();
  bool GrantItem(std::string_view baseId);  // createItem(id, hero level, normal), quantity 1, bag else stash

  SimContext& ctx_;
  HomesteadTower tower_;
};

}  // namespace abyss
