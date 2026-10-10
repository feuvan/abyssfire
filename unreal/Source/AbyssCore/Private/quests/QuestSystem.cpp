// Quest state machine (quests-story-ch1.md section 2: 2.1 states, 2.2 accept, 2.3 updateProgress, 2.4 craft phase
// ordering, 2.5 offer lists, 2.6 lists / tracking / guided quest, 2.7 fail, 2.8 turn in, 2.9 load; 5.6 NPC markers;
// 5.8 quest-complete banner). Owner area: quests+story.
//
// Iteration over `progress_` is by index and records are re-fetched after every bus publish: listeners run
// synchronously (hunts, story, quest world) and must never leave a dangling reference behind.
#include "abyss/base/Platform.h"

#include "abyss/quests/QuestSystem.h"

#include <algorithm>

#include "abyss/base/I18n.h"
#include "abyss/base/StrUtil.h"
#include "abyss/data/DataStore.h"

namespace abyss {

namespace {

// Q12 FIX: log lines carry the localized quest name (resolved by UE), never the raw zh name.
I18nArg QsQuestNameArg(const QuestDef& q) { return KeyArg("name", q.nameKey); }

bool QsObjectiveDone(const QuestDef& q, const QuestProgress& p, size_t i) {
  return i < p.objectives.size() && p.objectives[i] >= q.objectives[i].required;
}

bool QsAllDone(const QuestDef& q, const QuestProgress& p) {
  for (size_t i = 0; i < q.objectives.size(); ++i) {
    if (!QsObjectiveDone(q, p, i)) return false;
  }
  return true;
}

std::vector<int32_t> QsZeros(const QuestDef& q) { return std::vector<int32_t>(q.objectives.size(), 0); }

}  // namespace

QuestSystem::QuestSystem(const DataStore& data, EventSink& events, const GameplayBus& bus)
    : data_(&data), events_(&events), bus_(&bus) {}

bool QuestSystem::PrereqsMet(const QuestDef& q) const {
  for (const std::string& pre : q.prereqQuests) {
    if (!IsTurnedIn(pre)) return false;
  }
  return true;
}

bool QuestSystem::Accept(std::string_view questId) {
  const QuestDef* q = data_->FindQuest(questId);
  if (q == nullptr) return false;
  if (QuestProgress* existing = FindMutable(questId)) {
    // Q1 (keep): a failed reacceptable quest restarts in place (map position kept); prereqs are not re-checked.
    if (existing->status != QuestStatus::Failed || !q->reacceptable) return false;
    existing->status = QuestStatus::Active;
    existing->objectives = QsZeros(*q);
    events_->Log(MakeLoc("sys.quest.reaccepted", {QsQuestNameArg(*q)}), LogType::System);
  } else {
    if (!PrereqsMet(*q)) return false;
    progress_.push_back(QuestProgress{q->id, QuestStatus::Active, QsZeros(*q)});
    events_->Log(MakeLoc("sys.quest.accepted", {QsQuestNameArg(*q)}), LogType::System);
  }
  EvQuestUpdate ev;
  ev.kind = EvQuestUpdate::Kind::Accepted;
  ev.questId = q->id;
  events_->Emit(std::move(ev));
  bus_->Publish(QuestAcceptedMsg{q->id});
  return true;
}

bool QuestSystem::CraftPhaseDone(const QuestDef& q, const QuestProgress& p, ObjectiveType phase) {
  for (size_t i = 0; i < q.objectives.size(); ++i) {
    if (q.objectives[i].type == phase && !QsObjectiveDone(q, p, i)) return false;
  }
  return true;
}

void QuestSystem::UpdateProgress(ObjectiveType type, std::string_view targetId, int32_t amount,
                                 const QuestProgressSource* source) {
  const std::string target(targetId);
  // Records accepted by a listener during this call are not visited (the web iterates a live Map, but no listener
  // accepts quests; the bound keeps the loop finite either way).
  const size_t count = progress_.size();
  for (size_t qi = 0; qi < count && qi < progress_.size(); ++qi) {
    if (progress_[qi].status != QuestStatus::Active) continue;
    const QuestDef* q = data_->FindQuest(progress_[qi].questId);
    if (q == nullptr) continue;
    std::vector<size_t> advanced;
    {
      QuestProgress& prog = progress_[qi];
      for (size_t i = 0; i < q->objectives.size() && i < prog.objectives.size(); ++i) {
        const QuestObjectiveDef& obj = q->objectives[i];
        if (obj.type != type || obj.targetId != target) continue;
        if (type == ObjectiveType::Talk) {
          // Delivery gating: a talk objective counts once every earlier objective is complete.
          bool earlierDone = true;
          for (size_t j = 0; j < i; ++j) earlierDone = earlierDone && QsObjectiveDone(*q, prog, j);
          if (!earlierDone) continue;
        }
        const int32_t before = prog.objectives[i];
        const int64_t sum = static_cast<int64_t>(before) + static_cast<int64_t>(amount);
        prog.objectives[i] = static_cast<int32_t>((std::min)(sum, static_cast<int64_t>(obj.required)));
        if (prog.objectives[i] > before) advanced.push_back(i);
      }
      if (q->type == QuestType::Craft) {
        // 2.4: collect -> craft -> deliver.
        const bool collectDone = CraftPhaseDone(*q, prog, ObjectiveType::CraftCollect);
        const bool craftDone = CraftPhaseDone(*q, prog, ObjectiveType::CraftCraft);
        for (size_t i = 0; i < q->objectives.size() && i < prog.objectives.size(); ++i) {
          const ObjectiveType t = q->objectives[i].type;
          if (!collectDone && (t == ObjectiveType::CraftCraft || t == ObjectiveType::CraftDeliver)) prog.objectives[i] = 0;
          if (!craftDone && t == ObjectiveType::CraftDeliver) prog.objectives[i] = 0;
        }
      }
    }
    const bool allDone = QsAllDone(*q, progress_[qi]);
    for (size_t i : advanced) {
      const int32_t current = progress_[qi].objectives[i];
      if (current <= 0) continue;  // undone by craft ordering
      EvQuestUpdate ev;
      ev.kind = EvQuestUpdate::Kind::Progress;
      ev.questId = q->id;
      ev.objectiveIndex = static_cast<int32_t>(i);
      ev.current = current;
      ev.required = q->objectives[i].required;
      ev.completesQuest = allDone;
      ev.targetId = q->objectives[i].targetId;
      ev.amount = amount;
      if (source != nullptr) {
        ev.hasFrom = source->hasFrom;
        ev.from = source->from;
        ev.itemKind = source->itemKind;
      }
      events_->Emit(std::move(ev));
      bus_->Publish(QuestProgressMsg{q->id, static_cast<int32_t>(i), current, q->objectives[i].required,
                                     q->objectives[i].targetId, amount, allDone});
      if (qi >= progress_.size()) return;  // defensive: a listener cleared the records
    }
    if (allDone && progress_[qi].status == QuestStatus::Active) {
      progress_[qi].status = QuestStatus::Completed;
      EvQuestUpdate ev;
      ev.kind = EvQuestUpdate::Kind::Completed;
      ev.questId = q->id;
      events_->Emit(std::move(ev));
      bus_->Publish(QuestCompletedMsg{q->id});
      events_->Log(MakeLoc("sys.quest.completed", {QsQuestNameArg(*q)}), LogType::System);
      // 5.8 quest-complete banner: the quest name and "return to <giver>" (empty when no NPC gives it).
      const NpcDef* giver = data_->Npcs().GiverOf(q->id);
      LocText hint;
      if (giver != nullptr) hint = MakeLoc("zone.quest.returnTo", {KeyArg("npc", giver->nameKey)});
      events_->Emit(EvBanner{BannerKind::QuestComplete, MakeLoc(q->nameKey), std::move(hint)});
    }
  }
}

void QuestSystem::Fail(std::string_view questId) {
  QuestProgress* p = FindMutable(questId);
  if (p == nullptr || p->status != QuestStatus::Active) return;
  const QuestDef* q = data_->FindQuest(questId);
  if (q == nullptr) return;
  p->status = QuestStatus::Failed;
  EvQuestUpdate ev;
  ev.kind = EvQuestUpdate::Kind::Failed;
  ev.questId = q->id;
  events_->Emit(std::move(ev));
  bus_->Publish(QuestFailedMsg{q->id});
  events_->Log(MakeLoc("sys.quest.failed", {QsQuestNameArg(*q)}), LogType::System);
}

const QuestRewardDef* QuestSystem::TurnIn(std::string_view questId) {
  QuestProgress* p = FindMutable(questId);
  if (p == nullptr || p->status != QuestStatus::Completed) return nullptr;
  const QuestDef* q = data_->FindQuest(questId);
  if (q == nullptr) return nullptr;
  p->status = QuestStatus::TurnedIn;
  if (tracked_ == q->id) SetTracked("");
  events_->Log(MakeLoc("sys.quest.turnedIn", {QsQuestNameArg(*q), {"exp", ToStr(q->rewards.exp)},
                                              {"gold", ToStr(q->rewards.gold)}}),
               LogType::System);
  EvQuestUpdate ev;
  ev.kind = EvQuestUpdate::Kind::TurnedIn;
  ev.questId = q->id;
  events_->Emit(std::move(ev));
  bus_->Publish(QuestTurnedInMsg{q->id});
  return &q->rewards;
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

// 2.5 offer lists. Available / Indicator = getAvailableQuests (level gate, no record or failed + reacceptable, prereqs
// turned in - also re-checked for the failed branch, as the web does there). QuestCard = gatherNpcQuests: turn-ins
// first (never level-gated), then available (failed + reacceptable WITHOUT a prereq re-check), NPC list order.
std::vector<QuestOffer> QuestSystem::Offers(std::string_view npcId, int32_t heroLevel, OfferRule rule) const {
  std::vector<QuestOffer> out;
  const NpcDef* npc = data_->FindNpc(npcId);
  if (npc == nullptr) return out;
  const int32_t gate = data_->Quests().tuning.levelGateAbove;
  if (rule == OfferRule::QuestCard) {
    std::vector<QuestOffer> available;
    for (const std::string& id : npc->quests) {
      const QuestDef* q = data_->FindQuest(id);
      if (q == nullptr) continue;
      const QuestProgress* p = Progress(id);
      if (p != nullptr && p->status == QuestStatus::Completed) {
        out.push_back(QuestOffer{q, true});
        continue;
      }
      if (q->level > heroLevel + gate) continue;
      if (p == nullptr) {
        if (PrereqsMet(*q)) available.push_back(QuestOffer{q, false});
        continue;
      }
      if (p->status == QuestStatus::Failed && q->reacceptable) available.push_back(QuestOffer{q, false});
    }
    out.insert(out.end(), available.begin(), available.end());
    return out;
  }
  for (const std::string& id : npc->quests) {
    const QuestDef* q = data_->FindQuest(id);
    if (q == nullptr) continue;
    if (q->level > heroLevel + gate) continue;
    const QuestProgress* p = Progress(id);
    if (p != nullptr && !(p->status == QuestStatus::Failed && q->reacceptable)) continue;
    if (!PrereqsMet(*q)) continue;
    out.push_back(QuestOffer{q, false});
  }
  return out;
}

// computeNPCIndicator (5.6): completed -> yellow '?', else available (failed + reacceptable without a prereq check)
// -> '!', else active -> grey '?', else none.
NpcMarker QuestSystem::Marker(std::string_view npcId, int32_t heroLevel) const {
  const NpcDef* npc = data_->FindNpc(npcId);
  if (npc == nullptr || npc->quests.empty()) return NpcMarker::None;
  bool completed = false, active = false, available = false;
  for (const std::string& id : npc->quests) {
    const QuestProgress* p = Progress(id);
    if (p == nullptr) continue;
    if (p->status == QuestStatus::Completed) completed = true;
    if (p->status == QuestStatus::Active) active = true;
  }
  if (completed) return NpcMarker::TurnIn;
  const int32_t gate = data_->Quests().tuning.levelGateAbove;
  for (const std::string& id : npc->quests) {
    const QuestDef* q = data_->FindQuest(id);
    if (q == nullptr || q->level > heroLevel + gate) continue;
    const QuestProgress* p = Progress(id);
    if (p == nullptr) {
      if (!PrereqsMet(*q)) continue;
      available = true;
      break;
    }
    if (p->status == QuestStatus::Failed && q->reacceptable) {
      available = true;
      break;
    }
  }
  if (available) return NpcMarker::Available;
  if (active) return NpcMarker::InProgress;
  return NpcMarker::None;
}

void QuestSystem::SetTracked(std::string_view questId) {
  if (tracked_ == questId) return;
  tracked_ = std::string(questId);
  EvQuestUpdate ev;
  ev.kind = EvQuestUpdate::Kind::Tracked;
  ev.questId = tracked_;
  events_->Emit(std::move(ev));
  bus_->Publish(QuestTrackedChangedMsg{tracked_});
}

// getGuidedQuest (2.6): open quests of the zone (completed ones lead back to the giver); the pinned one, else the first
// main quest, else the first open quest.
const QuestDef* QuestSystem::GuidedQuest(std::string_view zoneId) const {
  const QuestDef* firstMain = nullptr;
  const QuestDef* first = nullptr;
  for (const auto& [q, p] : OpenQuests()) {
    if (q->zone != zoneId) continue;
    if (!tracked_.empty() && q->id == tracked_) return q;
    if (first == nullptr) first = q;
    if (firstMain == nullptr && q->category == QuestCategory::Main) firstMain = q;
  }
  return firstMain != nullptr ? firstMain : first;
}

std::string_view QuestSystem::CraftPhaseKey(const QuestDef& q, const QuestProgress& p) const {
  if (q.type != QuestType::Craft) return {};
  if (!CraftPhaseDone(q, p, ObjectiveType::CraftCollect)) return "sys.quest.phase.collect";
  if (!CraftPhaseDone(q, p, ObjectiveType::CraftCraft)) return "sys.quest.phase.craft";
  return "sys.quest.phase.deliver";
}

void QuestSystem::Load(std::vector<QuestProgress> records) {
  progress_.clear();
  tracked_.clear();
  for (QuestProgress& p : records) {
    const QuestDef* q = data_->FindQuest(p.questId);
    if (q != nullptr && (p.status == QuestStatus::Active || p.status == QuestStatus::Completed) &&
        p.objectives.size() != q->objectives.size()) {
      p.status = QuestStatus::Active;
      p.objectives = QsZeros(*q);
    }
    // Map.set semantics: a duplicate id replaces the value but keeps the first position.
    if (QuestProgress* existing = FindMutable(p.questId)) {
      *existing = std::move(p);
    } else {
      progress_.push_back(std::move(p));
    }
  }
}

void QuestSystem::Clear() {
  progress_.clear();
  tracked_.clear();
}

}  // namespace abyss
