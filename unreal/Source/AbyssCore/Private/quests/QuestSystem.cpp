// Quest state machine (quests-story-ch1.md section 2). STUB: owner area quests+story+pets. Lookups are real.
#include "abyss/base/Platform.h"

#include "abyss/quests/QuestSystem.h"

#include "abyss/base/Assert.h"
#include "abyss/data/DataStore.h"

namespace abyss {

QuestSystem::QuestSystem(const DataStore& data, EventSink& events, const GameplayBus& bus)
    : data_(&data), events_(&events), bus_(&bus) {}

bool QuestSystem::Accept(std::string_view questId) {
  ABYSS_UNIMPLEMENTED();
  (void)events_;
  (void)bus_;
  return false;
}

void QuestSystem::UpdateProgress(ObjectiveType type, std::string_view targetId, int32_t amount) {
  ABYSS_UNIMPLEMENTED();
}

void QuestSystem::Fail(std::string_view questId) { ABYSS_UNIMPLEMENTED(); }

const QuestRewardDef* QuestSystem::TurnIn(std::string_view questId) {
  ABYSS_UNIMPLEMENTED();
  return nullptr;
}

QuestProgress* QuestSystem::FindMutable(std::string_view questId) {
  for (QuestProgress& p : progress_) {
    if (p.questId == questId) return &p;
  }
  return nullptr;
}

const QuestProgress* QuestSystem::Progress(std::string_view questId) const {
  for (const QuestProgress& p : progress_) {
    if (p.questId == questId) return &p;
  }
  return nullptr;
}

QuestStatus QuestSystem::StatusOf(std::string_view questId, bool& hasRecord) const {
  const QuestProgress* p = Progress(questId);
  hasRecord = p != nullptr;
  return p != nullptr ? p->status : QuestStatus::Active;
}

bool QuestSystem::IsTurnedIn(std::string_view questId) const {
  const QuestProgress* p = Progress(questId);
  return p != nullptr && p->status == QuestStatus::TurnedIn;
}

std::vector<std::pair<const QuestDef*, const QuestProgress*>> QuestSystem::OpenQuests() const {
  std::vector<std::pair<const QuestDef*, const QuestProgress*>> out;
  for (const QuestProgress& p : progress_) {
    if (p.status != QuestStatus::Active && p.status != QuestStatus::Completed) continue;
    const QuestDef* q = data_->FindQuest(p.questId);
    if (q != nullptr) out.emplace_back(q, &p);
  }
  return out;
}

std::vector<std::string> QuestSystem::TurnedInIds() const {
  std::vector<std::string> out;
  for (const QuestProgress& p : progress_) {
    if (p.status == QuestStatus::TurnedIn) out.push_back(p.questId);
  }
  return out;
}

std::vector<QuestOffer> QuestSystem::Offers(std::string_view npcId, int32_t heroLevel, OfferRule rule) const {
  ABYSS_UNIMPLEMENTED();
  return {};
}

NpcMarker QuestSystem::Marker(std::string_view npcId, int32_t heroLevel) const {
  ABYSS_UNIMPLEMENTED();
  return NpcMarker::None;
}

void QuestSystem::SetTracked(std::string_view questId) {
  if (tracked_ == questId) return;
  tracked_ = std::string(questId);
  bus_->Publish(QuestTrackedChangedMsg{tracked_});
}

const QuestDef* QuestSystem::GuidedQuest(std::string_view zoneId) const {
  ABYSS_UNIMPLEMENTED();
  return nullptr;
}

std::string_view QuestSystem::CraftPhaseKey(const QuestDef& q, const QuestProgress& p) const {
  ABYSS_UNIMPLEMENTED();
  return {};
}

void QuestSystem::Load(std::vector<QuestProgress> records) {
  ABYSS_UNIMPLEMENTED();
  progress_ = std::move(records);
}

void QuestSystem::Clear() {
  progress_.clear();
  tracked_.clear();
}

}  // namespace abyss
