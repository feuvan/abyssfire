// Achievements (quests-story-ch1.md section 9; Q2). STUB: owner area quests+story+pets.
#include "abyss/base/Platform.h"

#include "abyss/quests/Achievements.h"

#include <algorithm>

#include "abyss/base/Assert.h"
#include "abyss/data/DataStore.h"
#include "abyss/save/SaveData.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

AchievementState::AchievementState(const DataStore& data) : data_(&data) {}

std::vector<std::string> AchievementState::Update(AchievementType type, std::string_view targetId, int32_t amount) {
  ABYSS_UNIMPLEMENTED();
  (void)data_;
  (void)exploredZones_;
  return {};
}

std::vector<std::string> AchievementState::CheckLevel(int32_t level) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

bool AchievementState::IsUnlocked(std::string_view id) const {
  return std::find(unlocked_.begin(), unlocked_.end(), id) != unlocked_.end();
}

int64_t AchievementState::ProgressOf(std::string_view key) const {
  for (const auto& [k, v] : progress_) {
    if (k == key) return v;
  }
  return 0;
}

EquipStats AchievementState::Bonuses() const {
  ABYSS_UNIMPLEMENTED();
  return {};
}

std::vector<std::pair<std::string, int64_t>> AchievementState::ToSave() const {
  ABYSS_UNIMPLEMENTED();
  return progress_;
}

void AchievementState::Load(const std::vector<std::pair<std::string, int64_t>>& entries) { ABYSS_UNIMPLEMENTED(); }

AchievementSystem::AchievementSystem(SimContext& ctx) : ctx_(ctx), state_(ctx.data) {}

void AchievementSystem::OnMonsterKilled(const MonsterKilledMsg& m) { ABYSS_UNIMPLEMENTED(); }

void AchievementSystem::OnLevelUp(const HeroLevelUpMsg& m) { ABYSS_UNIMPLEMENTED(); }

void AchievementSystem::OnZoneEntered(const ZoneEnteredMsg& m) { ABYSS_UNIMPLEMENTED(); }

void AchievementSystem::OnQuestTurnedIn(const QuestTurnedInMsg& m) { ABYSS_UNIMPLEMENTED(); }

void AchievementSystem::OnItemPicked(const ItemPickedMsg& m) { ABYSS_UNIMPLEMENTED(); }

void AchievementSystem::Announce(const std::vector<std::string>& unlocked) {
  for (const std::string& id : unlocked) {
    ctx_.events.Emit(EvAchievementUnlocked{id});
    ctx_.bus.Publish(AchievementUnlockedMsg{id});
  }
}

void AchievementSystem::FillSnapshot(Snapshot& out) const { out.achievements = &state_; }

void AchievementSystem::WriteSave(SaveData& out) const { out.achievements = state_.ToSave(); }

void AchievementSystem::ReadSave(const SaveData& in) {
  state_.Load(in.achievements);
  state_.SetExploredZones(in.visitedZones);  // Q2 across save / load
}

}  // namespace abyss
