// Homestead state used by Chapter 1 (quests-story-ch1.md 4.4, 4.6, 4.7; save-ui-input.md 3.2-3.3). STUB: owner area
// quests+story+pets.
#include "abyss/base/Platform.h"

#include "abyss/pets/Homestead.h"

#include <algorithm>

#include "abyss/base/Assert.h"
#include "abyss/data/DataStore.h"
#include "abyss/save/SaveData.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

int32_t EmbersForKill(const HomesteadTables& t, bool elite, bool isMiniBoss, int32_t eliteAffixCount) {
  ABYSS_UNIMPLEMENTED();
  return 0;
}

int32_t EmbersForQuest(const HomesteadTables& t, QuestCategory category, bool hasRewardEmbers, int32_t rewardEmbers) {
  if (hasRewardEmbers) return rewardEmbers;
  return category == QuestCategory::Main ? t.embersQuestMain : t.embersQuestSide;
}

int32_t GardenInterval(const HomesteadTables& t, int32_t level) {
  ABYSS_UNIMPLEMENTED();
  return 0;
}

int32_t GardenCapacity(const HomesteadTables& t, int32_t level) {
  ABYSS_UNIMPLEMENTED();
  return 0;
}

std::string RollGardenYield(const HomesteadTables& t, int32_t level, Rng& rng) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

HomesteadTower::HomesteadTower(const HomesteadTables& t) : t_(&t) {}

int32_t HomesteadTower::AddEmbers(int32_t n) {
  const int32_t add = std::max(0, n);
  embers_ += add;
  return add;
}

std::vector<std::string> HomesteadTower::SyncUnlocks(std::span<const std::string> turnedInQuests,
                                                     std::vector<std::pair<std::string, int32_t>>& buildingLevels) {
  ABYSS_UNIMPLEMENTED();
  (void)t_;
  return {};
}

bool HomesteadTower::TowerUnlocked(std::span<const std::string> turnedInQuests) const {
  return std::find(turnedInQuests.begin(), turnedInQuests.end(), t_->towerUnlockQuest) != turnedInQuests.end();
}

std::string HomesteadTower::OnKillGarden(int32_t gardenLevel, Rng& rng) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

bool HomesteadTower::OnKillExpedition() {
  if (expedition_.has_value()) ABYSS_UNIMPLEMENTED();
  return false;
}

void HomesteadTower::Reset() {
  embers_ = 0;
  garden_ = GardenState{};
  expedition_.reset();
  blessing_.reset();
  towerReturn_.reset();
}

HomesteadSystem::HomesteadSystem(SimContext& ctx) : ctx_(ctx), tower_(ctx.data.Homestead()) {}

int32_t HomesteadSystem::BuildingLevel(std::string_view buildingId) const {
  for (const auto& [id, level] : buildings_) {
    if (id == buildingId) return level;
  }
  return 0;
}

StatBag HomesteadSystem::TotalBonuses() const {
  ABYSS_UNIMPLEMENTED();
  return {};
}

StatBag HomesteadSystem::BlessingStats() const {
  ABYSS_UNIMPLEMENTED();
  return {};
}

bool HomesteadSystem::IsPetAway(std::string_view petId) const {
  ABYSS_UNIMPLEMENTED();
  return false;
}

void HomesteadSystem::OnMonsterKilled(const MonsterKilledMsg& m) { ABYSS_UNIMPLEMENTED(); }

void HomesteadSystem::OnQuestTurnedIn(const QuestTurnedInMsg& m) { ABYSS_UNIMPLEMENTED(); }

void HomesteadSystem::Tick(double dtMs) { ABYSS_UNIMPLEMENTED(); }

void HomesteadSystem::FillSnapshot(Snapshot& out) const { out.homestead = &tower_; }

void HomesteadSystem::WriteSave(SaveData& out) const {
  ABYSS_UNIMPLEMENTED();
  out.homestead.buildings = buildings_;
  out.homestead.embers = tower_.Embers();
}

void HomesteadSystem::ReadSave(const SaveData& in) {
  ABYSS_UNIMPLEMENTED();
  (void)ctx_;
}

}  // namespace abyss
