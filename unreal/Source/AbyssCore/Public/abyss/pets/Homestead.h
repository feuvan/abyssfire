// Homestead state that Chapter 1 already mutates (embers, tower unlock records, herb garden, expedition counter,
// blessing / tower return records, building levels) so saves are forward compatible. The Ember Tower zone, its UI and
// its actions are a later milestone (Q3: hidden in milestone 1).
// Spec: quests-story-ch1.md 4.4 (embers: quest turn-in 4.4.1, kills 4.4.2), 4.6 (unlocks triggered by Ch1 turn-ins),
// 4.7 (garden growth, expedition counter), 13 (home API proposal); save-ui-input.md 3.2-3.3 (homestead save fields and
// load normalisation); docs/homestead-pets.md.
//
// Owner area: quests+story+pets. `HomesteadTower` is pure; `HomesteadSystem` is the runtime wrapper (SimContext).
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
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

// 4.4.2: elite 5 / mini-boss 3 / per affix 1 / plain 0, first match wins (table values from homestead.json).
ABYSS_API int32_t EmbersForKill(const HomesteadTables& t, bool elite, bool isMiniBoss, int32_t eliteAffixCount);
// 4.4.1: reward.embers ?? (main 2 : side 1).
ABYSS_API int32_t EmbersForQuest(const HomesteadTables& t, QuestCategory category, bool hasRewardEmbers, int32_t rewardEmbers);
ABYSS_API int32_t GardenInterval(const HomesteadTables& t, int32_t level);
ABYSS_API int32_t GardenCapacity(const HomesteadTables& t, int32_t level);
ABYSS_API std::string RollGardenYield(const HomesteadTables& t, int32_t level, Rng& rng);

struct GardenState {
  int32_t progress = 0;
  std::vector<std::pair<std::string, int32_t>> stock;  // itemId -> count (> 0)
};
struct ExpeditionState {
  std::string petId, optionId;
  int32_t kills = 0, killsRequired = 1;
  double remainingMs = 0;
};
struct BlessingState {
  std::string id;
  int32_t level = 1;
  double remainingMs = 0;
};
struct TowerReturn {
  std::string mapId;
  double col = 0, row = 0;
};

class ABYSS_API HomesteadTower {
 public:
  explicit HomesteadTower(const HomesteadTables& t);

  int32_t AddEmbers(int32_t n);  // max(0, n); returns the amount added
  int32_t Embers() const { return embers_; }
  // syncUnlocks: wings whose unlockQuest is turned in; a newly unlocked wing starts at Lv1 (returned).
  std::vector<std::string> SyncUnlocks(std::span<const std::string> turnedInQuests,
                                       std::vector<std::pair<std::string, int32_t>>& buildingLevels);
  bool TowerUnlocked(std::span<const std::string> turnedInQuests) const;
  // 4.7.1 garden growth on a kill (garden level from the building levels); returns the yield item id or "".
  std::string OnKillGarden(int32_t gardenLevel, Rng& rng);
  bool OnKillExpedition();  // 4.7.3

  GardenState& Garden() { return garden_; }
  std::optional<ExpeditionState>& Expedition() { return expedition_; }
  std::optional<BlessingState>& Blessing() { return blessing_; }
  std::optional<TowerReturn>& Return() { return towerReturn_; }
  const GardenState& Garden() const { return garden_; }

  void Reset();

 private:
  const HomesteadTables* t_;
  int32_t embers_ = 0;
  GardenState garden_;
  std::optional<ExpeditionState> expedition_;
  std::optional<BlessingState> blessing_;
  std::optional<TowerReturn> towerReturn_;
};

class ABYSS_API HomesteadSystem {
 public:
  explicit HomesteadSystem(SimContext& ctx);

  int32_t BuildingLevel(std::string_view buildingId) const;
  // getTotalBonuses: building bonuses (stashSlots, potionDiscount, magicFind, expBonus...) + blessing stats.
  StatBag TotalBonuses() const;
  StatBag BlessingStats() const;  // merged into EquipStats (loot 8.3)
  bool IsPetAway(std::string_view petId) const;

  // Kill pipeline: embers (EvEmbersGained, fromKill), garden, expedition (4.4.2 / 4.7).
  void OnMonsterKilled(const MonsterKilledMsg& m);
  // Quest turn-in (QuestTurnedInMsg, before exp/gold, quests 4.1): embers + SyncUnlocks.
  void OnQuestTurnedIn(const QuestTurnedInMsg& m);
  void Tick(double dtMs);  // expedition / blessing timers (sim time)

  const HomesteadTower& Tower() const { return tower_; }
  void FillSnapshot(Snapshot& out) const;
  void WriteSave(SaveData& out) const;
  void ReadSave(const SaveData& in);  // HomesteadTower.load normalisation (save 3.3)

 private:
  SimContext& ctx_;
  std::vector<std::pair<std::string, int32_t>> buildings_;  // id -> level
  HomesteadTower tower_;
};

}  // namespace abyss
