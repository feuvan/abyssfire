// Achievements (quests-story-ch1.md section 9, 10.5; DECISIONS Q2: one count per kill, ach_explore_all counts distinct
// story zones; FIX Q11: log names are i18n keys; FIX Q13: AchievementUnlockedMsg rebuilds the equip stats).
// Owner area: quests+story+pets.
#include "abyss/base/Platform.h"

#include "abyss/quests/Achievements.h"

#include <algorithm>

#include "abyss/base/I18n.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/save/SaveData.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

namespace {

// key(a) = type or type:targetId.
std::string AchKey(const AchievementDef& a) {
  std::string key(EnumName(a.type));
  if (!a.targetId.empty()) {
    key += ':';
    key += a.targetId;
  }
  return key;
}

bool AchMatches(const AchievementDef& a, AchievementType type, std::string_view targetId) {
  return a.type == type && (a.targetId.empty() || a.targetId == targetId);
}

}  // namespace

AchievementState::AchievementState(const DataStore& data) : data_(&data) {}

// update(type, targetId?, amount): each distinct key of the still-locked matching achievements is incremented once,
// then every matching locked achievement whose counter reached `required` unlocks (table order).
std::vector<std::string> AchievementState::Update(AchievementType type, std::string_view targetId, int32_t amount) {
  const std::vector<AchievementDef>& all = data_->Quests().achievements;
  std::vector<std::string> keys;
  for (const AchievementDef& a : all) {
    if (IsUnlocked(a.id) || !AchMatches(a, type, targetId)) continue;
    std::string key = AchKey(a);
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) keys.push_back(std::move(key));
  }
  for (const std::string& key : keys) {
    bool found = false;
    for (auto& [k, v] : progress_) {
      if (k == key) {
        v += amount;
        found = true;
        break;
      }
    }
    if (!found) progress_.emplace_back(key, static_cast<int64_t>(amount));
  }
  std::vector<std::string> unlocked;
  for (const AchievementDef& a : all) {
    if (IsUnlocked(a.id) || !AchMatches(a, type, targetId)) continue;
    if (ProgressOf(AchKey(a)) >= a.required) {
      unlocked_.push_back(a.id);
      unlocked.push_back(a.id);
    }
  }
  return unlocked;
}

// checkLevel(level): every locked 'level' achievement with level >= required.
std::vector<std::string> AchievementState::CheckLevel(int32_t level) {
  std::vector<std::string> unlocked;
  for (const AchievementDef& a : data_->Quests().achievements) {
    if (IsUnlocked(a.id) || a.type != AchievementType::Level) continue;
    if (level >= a.required) {
      unlocked_.push_back(a.id);
      unlocked.push_back(a.id);
    }
  }
  return unlocked;
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

// getBonuses(): sum of reward.value per reward.stat over the unlocked achievements (table order).
EquipStats AchievementState::Bonuses() const {
  EquipStats out;
  for (const AchievementDef& a : data_->Quests().achievements) {
    if (!a.hasReward || !IsUnlocked(a.id)) continue;
    out.Add(a.rewardStat, a.rewardValue);
  }
  return out;
}

// getUnlockedData(): {...progress, ...{<achId>: 1}} - progress keys in insertion order, then the unlocked ids (a key in
// both keeps its first position with the value 1, JS object spread semantics).
std::vector<std::pair<std::string, int64_t>> AchievementState::ToSave() const {
  std::vector<std::pair<std::string, int64_t>> out = progress_;
  for (const std::string& id : unlocked_) {
    bool found = false;
    for (auto& [k, v] : out) {
      if (k == id) {
        v = 1;
        found = true;
        break;
      }
    }
    if (!found) out.emplace_back(id, 1);
  }
  return out;
}

// loadData(data): keys equal to a known achievement id unlock it; every other key is a progress counter.
void AchievementState::Load(const std::vector<std::pair<std::string, int64_t>>& entries) {
  progress_.clear();
  unlocked_.clear();
  for (const auto& [key, value] : entries) {
    if (data_->Quests().FindAchievement(key) != nullptr) {
      if (!IsUnlocked(key)) unlocked_.push_back(key);
      continue;
    }
    bool found = false;
    for (auto& [k, v] : progress_) {
      if (k == key) {
        v = value;
        found = true;
        break;
      }
    }
    if (!found) progress_.emplace_back(key, value);
  }
}

AchievementSystem::AchievementSystem(SimContext& ctx) : ctx_(ctx), state_(ctx.data) {}

// Kill hook step 3 (monsters-ai 11): Q2 FIX - one update('kill', defId) counts the generic 'kill' key once and the
// 'kill:<defId>' key once (the web called update twice and counted 'kill' double); then checkLevel.
void AchievementSystem::OnMonsterKilled(const MonsterKilledMsg& m) {
  if (ctx_.data.Quests().achievementsCountKillOnce) {
    Announce(state_.Update(AchievementType::Kill, m.defId, 1));
  } else {
    Announce(state_.Update(AchievementType::Kill, std::string_view(), 1));
    Announce(state_.Update(AchievementType::Kill, m.defId, 1));
  }
  if (ctx_.sys.hero != nullptr) Announce(state_.CheckLevel(ctx_.sys.hero->Level()));
}

void AchievementSystem::OnLevelUp(const HeroLevelUpMsg& m) { Announce(state_.CheckLevel(m.level)); }

// Q2 FIX: 'explore' counts each story zone (the world map's MapOrder) once per save, not every zone-scene creation.
void AchievementSystem::OnZoneEntered(const ZoneEnteredMsg& m) {
  if (!ctx_.data.Quests().achievementsExploreDistinct) {
    Announce(state_.Update(AchievementType::Explore, m.mapId, 1));
    return;
  }
  const MapDef* map = ctx_.data.FindMap(m.mapId);
  if (map == nullptr || map->orderIndex < 0) return;
  const std::vector<std::string>& seen = state_.ExploredZones();
  if (std::find(seen.begin(), seen.end(), m.mapId) != seen.end()) return;
  std::vector<std::string> next = seen;
  next.push_back(m.mapId);
  state_.SetExploredZones(std::move(next));
  Announce(state_.Update(AchievementType::Explore, m.mapId, 1));
}

void AchievementSystem::OnQuestTurnedIn(const QuestTurnedInMsg& m) {
  Announce(state_.Update(AchievementType::Quest, m.questId, 1));
}

// update('collect') when a legendary item is picked up (click or auto-loot).
void AchievementSystem::OnItemPicked(const ItemPickedMsg& m) {
  if (m.quality != ItemQuality::Legendary) return;
  Announce(state_.Update(AchievementType::Collect, m.baseId, 1));
}

// unlock(a): EvAchievementUnlocked (toast), the log line (Q11: localized name / title keys), AchievementUnlockedMsg
// (GameSim rebuilds the merged EquipStats at once, FIX Q13).
void AchievementSystem::Announce(const std::vector<std::string>& unlocked) {
  for (const std::string& id : unlocked) {
    const AchievementDef* a = ctx_.data.Quests().FindAchievement(id);
    ctx_.events.Emit(EvAchievementUnlocked{id});
    const std::string base = "data.achievement." + id;
    if (a != nullptr && !a->title.empty()) {
      ctx_.events.Log(MakeLoc("sys.achievement.unlockedWithTitle",
                              {KeyArg("name", base + ".name"), KeyArg("title", base + ".title")}),
                      LogType::System);
    } else {
      ctx_.events.Log(MakeLoc("sys.achievement.unlocked", {KeyArg("name", base + ".name")}), LogType::System);
    }
    ctx_.bus.Publish(AchievementUnlockedMsg{id});
  }
}

void AchievementSystem::FillSnapshot(Snapshot& out) const { out.achievements = &state_; }

void AchievementSystem::WriteSave(SaveData& out) const { out.achievements = state_.ToSave(); }

void AchievementSystem::ReadSave(const SaveData& in) {
  state_.Load(in.achievements);
  // Q2 across save / load: zones already counted (story zones only; the set is rebuilt from the saved visits).
  std::vector<std::string> zones;
  for (const std::string& z : in.visitedZones) {
    const MapDef* map = ctx_.data.FindMap(z);
    if (map != nullptr && map->orderIndex >= 0) zones.push_back(z);
  }
  state_.SetExploredZones(std::move(zones));
}

}  // namespace abyss
