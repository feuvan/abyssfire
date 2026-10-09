// QuestSystem: the quest state machine (pure, data-driven, deterministic, no I/O).
// Spec: quests-story-ch1.md 1.5 (QuestProgress save format), 2.1-2.9 (states, accept, updateProgress, craft phase
// ordering, offer lists, lists/tracking/guided quest, fail, turn in, save/load), 5.6 (NPC markers), 13 (API proposal),
// 15 (quirks); DECISIONS Q-series.
//
// Owner area: quests+story+pets. Session-level (survives zone changes). Emits GameplayBus quest messages and
// presentation EvQuestUpdate / EvLog through the sink passed at construction; it never pays rewards (QuestWorld does,
// quests 4.1).
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/data/QuestData.h"
#include "abyss/sim/Events.h"
#include "abyss/sim/GameplayBus.h"

namespace abyss {

class DataStore;

enum class QuestStatus : uint8_t { Active, Completed, TurnedIn, Failed };
ABYSS_ENUM_STRINGS(QuestStatus, "active", "completed", "turned_in", "failed")

// QuestProgress (1.5): {questId, status, objectives: [{current}]}.
struct QuestProgress {
  std::string questId;
  QuestStatus status = QuestStatus::Active;
  std::vector<int32_t> objectives;  // current per objective
  bool operator==(const QuestProgress& o) const = default;
};

enum class OfferRule : uint8_t { Available, QuestCard, Indicator };

// NPC overhead marker (5.6).
enum class NpcMarker : uint8_t { None, Available, AvailableLocked, InProgress, TurnIn };
ABYSS_ENUM_STRINGS(NpcMarker, "none", "available", "availableLocked", "inProgress", "turnIn")

struct QuestOffer {
  const QuestDef* quest = nullptr;
  bool turnIn = false;  // quest card: turn-ins first
};

// Where a progress step came from (quests 3.2 pickup visual): drops, gather nodes and clues pass the source point and
// the quest-item kind; EvQuestUpdate{Progress} carries them (hasFrom / from / itemKind).
struct QuestProgressSource {
  bool hasFrom = false;
  Vec2 from;
  std::string itemKind;
};

class ABYSS_API QuestSystem {
 public:
  QuestSystem(const DataStore& data, EventSink& events, const GameplayBus& bus);

  // acceptQuest (2.2): no level check here; re-accept of a failed reacceptable quest keeps its map position.
  bool Accept(std::string_view questId);
  // updateProgress (2.3): every active quest in insertion order; talk delivery gating; craft ordering; events.
  // Per quest: EvQuestUpdate{Progress} + QuestProgressMsg for every advanced objective, then (all done) status
  // completed, EvQuestUpdate{Completed} + QuestCompletedMsg, log sys.quest.completed and the quest-complete banner.
  void UpdateProgress(ObjectiveType type, std::string_view targetId, int32_t amount = 1,
                      const QuestProgressSource* source = nullptr);
  void Fail(std::string_view questId);        // 2.7: active only
  // turnInQuest (2.8): completed only -> turned_in, untracks, logs, QuestTurnedInMsg; returns the reward def.
  const QuestRewardDef* TurnIn(std::string_view questId);

  const QuestProgress* Progress(std::string_view questId) const;
  bool HasRecord(std::string_view questId) const { return Progress(questId) != nullptr; }
  QuestStatus StatusOf(std::string_view questId, bool& hasRecord) const;
  bool IsTurnedIn(std::string_view questId) const;
  std::span<const QuestProgress> AllProgress() const { return progress_; }  // insertion order
  // getActiveQuests (2.6): active + completed, insertion order, definitions found.
  std::vector<std::pair<const QuestDef*, const QuestProgress*>> OpenQuests() const;
  std::vector<std::string> TurnedInIds() const;

  // Offer lists (2.5) and markers (5.6).
  std::vector<QuestOffer> Offers(std::string_view npcId, int32_t heroLevel, OfferRule rule) const;
  NpcMarker Marker(std::string_view npcId, int32_t heroLevel) const;

  // Tracking (2.6; not saved).
  void SetTracked(std::string_view questId);  // "" = none; QuestTrackedChangedMsg when changed
  const std::string& Tracked() const { return tracked_; }
  const QuestDef* GuidedQuest(std::string_view zoneId) const;
  // Craft phase label key (2.4): sys.quest.phase.collect / .craft / .deliver; "" for non-craft quests.
  std::string_view CraftPhaseKey(const QuestDef& q, const QuestProgress& p) const;
  // Every craft_collect (craft_craft) objective of `q` complete in `p` (2.4; vacuously true when there is none).
  static bool CraftPhaseDone(const QuestDef& q, const QuestProgress& p, ObjectiveType phase);

  // Save / load (2.9). Load: records of known quests that are active / completed with an objective count different
  // from the definition restart as active with zeros (quest redesigned since the save); unknown ids are kept (ignored
  // by every list); a duplicate id replaces the earlier value at its position (Map.set). Tracking is cleared (not
  // saved).
  void Load(std::vector<QuestProgress> records);
  void Clear();

 private:
  QuestProgress* FindMutable(std::string_view questId);
  bool PrereqsMet(const QuestDef& q) const;

  const DataStore* data_;
  EventSink* events_;
  const GameplayBus* bus_;
  std::vector<QuestProgress> progress_;
  std::string tracked_;
};

}  // namespace abyss
