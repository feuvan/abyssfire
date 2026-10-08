// Achievements: progress counters, unlocks and stat bonuses.
// Spec: quests-story-ch1.md section 9 (update / checkLevel / unlock / getBonuses / save format), 10.5;
// DECISIONS Q2 (one count per kill; ach_explore_all counts distinct zones). Bonuses feed the merged EquipStats
// (loot 8.3) and invalidate them on unlock (FIX Q13: EquipStatsDirtyMsg).
//
// Owner area: quests+story+pets. `AchievementState` is pure; `AchievementSystem` is the runtime wrapper.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Stats.h"
#include "abyss/data/QuestData.h"
#include "abyss/sim/GameplayBus.h"

namespace abyss {

class DataStore;
struct SimContext;
struct Snapshot;
struct SaveData;

class ABYSS_API AchievementState {
 public:
  explicit AchievementState(const DataStore& data);

  // update(type, targetId?, amount): each distinct key once per call; returns newly unlocked ids in table order.
  std::vector<std::string> Update(AchievementType type, std::string_view targetId, int32_t amount = 1);
  std::vector<std::string> CheckLevel(int32_t level);
  bool IsUnlocked(std::string_view id) const;
  int64_t ProgressOf(std::string_view key) const;  // key = type or type:targetId
  EquipStats Bonuses() const;

  // Save: progress counters merged with {<achId>: 1} for unlocked (insertion order).
  std::vector<std::pair<std::string, int64_t>> ToSave() const;
  void Load(const std::vector<std::pair<std::string, int64_t>>& entries);
  // Q2 distinct zones for ach_explore_all, rebuilt on load from SaveData::visitedZones (the 'explore' counter itself is a
  // saved progress key; this set keeps a reload from counting the same zone again).
  void SetExploredZones(std::vector<std::string> zones) { exploredZones_ = std::move(zones); }
  const std::vector<std::string>& ExploredZones() const { return exploredZones_; }

 private:
  const DataStore* data_;
  std::vector<std::pair<std::string, int64_t>> progress_;
  std::vector<std::string> unlocked_;
  std::vector<std::string> exploredZones_;  // Q2 distinct zones
};

class ABYSS_API AchievementSystem {
 public:
  explicit AchievementSystem(SimContext& ctx);

  void OnMonsterKilled(const MonsterKilledMsg& m);    // 'kill' once (Q2) + 'kill:defId', checkLevel
  void OnLevelUp(const HeroLevelUpMsg& m);
  void OnZoneEntered(const ZoneEnteredMsg& m);        // 'explore' once per distinct zone (Q2; survives save / load)
  void OnQuestTurnedIn(const QuestTurnedInMsg& m);    // 'quest'
  void OnItemPicked(const ItemPickedMsg& m);          // 'collect' for legendary
  const AchievementState& State() const { return state_; }

  void FillSnapshot(Snapshot& out) const;
  void WriteSave(SaveData& out) const;
  void ReadSave(const SaveData& in);

 private:
  void Announce(const std::vector<std::string>& unlocked);

  SimContext& ctx_;
  AchievementState state_;
};

}  // namespace abyss
