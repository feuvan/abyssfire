// QuestWorld: the live, zone-bound side of quests: NPC interaction routing, quest card, turn-in pipeline and rewards,
// progress sources (kill, drops, gather nodes, clues, explore, talk), escort / defend / craft runtimes, guide arrow.
// Spec: quests-story-ch1.md 3 (progress sources: 3.1 kill, 3.3 drops, 3.4 gather, 3.5 clues, 3.6 explore, 3.7 talk,
// 3.9 escort, 3.10 defend, 3.11 craft), 4 (turn-in exact order, pick-one gear, fixed items, embers, pet reward),
// 5.1-5.3 (guide, quest card), 6 (NPC types, placement, interaction trigger), 12 (event flows), 15 (quirks);
// DECISIONS Q1-Q8, W8 (walk-then-act: NPC talk on arrival), I10 (turn-in overflow to the stash).
//
// Owner area: quests+story+pets. Runtime system (SimContext). Timers: TimerOwner::Quests. RNG: RngStream::Quests
// (drops). Turn-in order (4.1): reward choices generated -> QuestSystem::TurnIn (QuestTurnedInMsg listeners run first:
// story, embers, tracker) -> exp -> gold -> items (bag else stash) -> pet -> achievements 'quest' -> SaveRequestMsg.
// Exp / gold / items go through RewardService (ExpSource::Quest, GoldReason::QuestReward, OverflowPolicy::Stash).
// Progress from drops / gather nodes / clues emits EvQuestUpdate with hasFrom / from / itemKind (quests 3.2 fly-in).
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Timers.h"
#include "abyss/base/Types.h"
#include "abyss/items/Item.h"
#include "abyss/quests/QuestGuide.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/sim/GameplayBus.h"
#include "abyss/sim/SimTypes.h"

namespace abyss {

struct SimContext;
struct Snapshot;

enum class QuestTimerKind : uint16_t { ObserverTick = 1, DefendWave = 2 };

// A gather node / clue mark in the world (QuestWorld 3.4-3.5). Each one is an EntityKind::Prop entity (EvEntitySpawned
// on creation; EvEntityDespawned{Collected} when gathered / examined, {Removed} when a sync drops it) and a snapshot
// marker (GatherNode / Clue).
struct QuestNode {
  EntityId id = kNoEntity;
  std::string questId;
  int32_t objectiveIndex = -1;
  int32_t spotIndex = -1;  // gather spot index (resolveGatherSpots order); -1 for a clue
  std::string itemKind;    // icon / node look
  Vec2 pos;
  bool clue = false;
};

// Display-name keys of the quest-spawned escort NPC and defend target (Q11 FIX: always resolve keys). The exported keys
// drop the quest id's "q_" prefix: q_escort_merchant_plains -> data.escortNpc.escort_merchant_plains, q_defend_camp_forest
// -> data.defendTarget.defend_camp_forest. A quest without such a key (q_trapped_miners) shows the data's zh name.
ABYSS_API std::string EscortNpcNameKey(std::string_view questId);
ABYSS_API std::string DefendTargetNameKey(std::string_view questId);

// Escort runtime (3.9; scene-local, not saved: a zone entry / reload respawns it at its start, full HP, not joined).
// nameKey / name: the label (render I18n::NameOr(nameKey, name)); nameKey is "" when the key does not resolve.
struct EscortState {
  bool active = false;
  std::string questId;
  std::string nameKey;
  std::string name;  // zh-CN fallback (escortNpc.name)
  EntityId entity = kNoEntity;  // EntityKind::Escort
  Vec2 pos;
  TilePos dest;
  double hp = 0, maxHp = 0;
  bool joined = false;
  std::vector<TilePos> path;
  double repathAtMs = 0;
};

// Defend runtime (3.10; scene-local, not saved: the wave count resumes from the quest progress).
struct DefendState {
  bool active = false;
  std::string questId;
  std::string nameKey;  // data.defendTarget.<id> when it resolves, else "" (see EscortState)
  std::string name;     // zh-CN fallback (defendTarget.name)
  EntityId entity = kNoEntity;  // EntityKind::DefendTarget
  Vec2 pos;
  double hp = 0, maxHp = 0;
  int32_t wave = 0, totalWaves = 0;
  bool waveActive = false;
  bool timerStarted = false;    // the web's `defendWaveTimer != 0`
  double timerMs = 0;
  std::vector<EntityId> waveMonsters;
};

// Quest card view (5.3): one entry per offer, in Offers(QuestCard) order (turn-ins first).
struct QuestCardEntry {
  const QuestDef* quest = nullptr;
  bool turnIn = false;
  // Turn-in card with rewards.choices: the generated pick-one gear (loot 5.6), shown as selectable slots; the index is
  // CmdQuestTurnIn::choice. Generated (RngStream::Loot) when the card opens or refreshes, cached per quest for the
  // session, so the card shows exactly the items the turn-in grants. Empty on accept cards (they show the preview line).
  std::vector<ItemInstance> choices;
};
struct QuestCardState {
  bool open = false;
  std::string npcId;
  std::vector<QuestCardEntry> entries;
};

class ABYSS_API QuestWorld {
 public:
  explicit QuestWorld(SimContext& ctx);

  // ---- zone lifecycle ----
  void OnZoneEnter();  // gather nodes, clues, escort / defend actors of the zone's open quests; guide refresh
  void OnZoneExit();   // escort / defend runtime dropped (not saved, quests 2.9)

  // ---- NPC interaction (6.1, Q6 / W8: the caller walks first) ----
  // log dialogue[0], talk progress, craft phase hook, then by type: shop (ShopSystem::Open) / quest card (OpenCard) /
  // dialogue tree (DialogueSystem::OpenTree) / linear / stash (InventorySystem::OpenStash). Every one of those is a
  // core-owned modal (SimTypes.h): GameSim derives the freeze / input block from the owners' state.
  void InteractNpc(std::string_view npcId);
  const QuestCardState& Card() const { return card_; }
  // Opens (or refreshes) the NPC's quest card WITHOUT the interaction side effects (no dialogue[0] log, no talk
  // progress): CmdQuestCardOpen (T17 chain offer) and InteractNpc's quest branch. Generates + caches the turn-in
  // choices. False (nothing opens) when the NPC is not in the zone, has no card entries, or a card / dialogue is open
  // (T17: "offer only if no card or dialogue is open"; refresh = the same NPC's open card).
  bool OpenCard(std::string_view npcId);
  bool AcceptFromCard(std::string_view questId);  // accept + track + toast; refreshes or closes the card
  // turnInQuest(questId, choiceIndex) (4.1): the cached choices are the ones granted. False when not completed.
  bool TurnIn(std::string_view questId, int32_t choiceIndex);
  void CloseCard();  // EvPanelRequest close follows from GameSim's modal diff
  // The card's ui.questCard.viewStory button (CmdDialogueOpenTree): closes the card, opens the NPC's dialogue tree.
  bool OpenDialogueTree(std::string_view npcId);
  // Reward choices of a quest from the session cache (empty when not generated yet / none). Read-only.
  std::span<const ItemInstance> RewardChoices(std::string_view questId) const;

  // ---- progress sources ----
  void OnMonsterKilled(const MonsterKilledMsg& m);  // 3.1 kill progress then 3.3 drops (two pipeline slots, see wiring)
  void RollQuestDrops(const MonsterKilledMsg& m);   // 3.3
  void OnQuestAccepted(const QuestAcceptedMsg& m);  // sync nodes, escort / defend of this zone, guide refresh
  void OnQuestProgress(const QuestProgressMsg& m);  // Q5 FIX: sync on progress too; completion refreshes the guide
  // Per step (not while cinematic): escort / defend runtime, gather / clue pickups in range, guide refresh (250 ms or
  // the next step after a quest change), 500 ms observer (explore objectives).
  void Tick(double dtMs);
  void OnTimer(const Timer& t);  // no timer kinds in use (the runtimes are per-step); ignored

  // ---- views ----
  GuideTarget Guide() const { return guide_; }
  std::span<const QuestNode> Nodes() const { return nodes_; }
  const EscortState& Escort() const { return escort_; }
  const DefendState& Defend() const { return defend_; }
  // Where an open clue's mark sits (3.5 nudge), false without a mark.
  bool ClueTile(std::string_view questId, int32_t objectiveIndex, Vec2& out) const;
  // The guide's world queries over the live zone (5.1; also used by tests).
  GuideWorld MakeGuideWorld() const;
  // Snapshot: quest card / guide / escort / defend views, gather + clue + escort + defend markers, and per NPC the
  // overhead quest marker (5.6, live), heroNear (within 3 tiles, 6.4) and talking (its card / dialogue / shop / stash).
  void FillSnapshot(Snapshot& out) const;

 private:
  // Generates (RngStream::Loot, loot 5.6) and caches the pick-one gear of a quest once per session.
  const std::vector<ItemInstance>& EnsureRewardChoices(const QuestDef& quest);
  // sync() (3.4 / 3.5): gather nodes and clue marks of this zone's active quests; drops the unwanted ones.
  void Sync();
  const std::vector<TilePos>& SpotsFor(const QuestDef& quest, int32_t objectiveIndex);
  bool IsGathered(std::string_view questId, int32_t objectiveIndex, int32_t spot) const;
  void RemoveNode(EntityId id, DespawnReason reason);
  void TickGatherAndClues();
  void TickGuide(double dtMs);
  void CheckExplore();                     // 3.6 (500 ms observer)
  void AdvanceCraftFromNpc(std::string_view npcId);  // 3.11
  void SpawnEscort();                      // 3.9
  void TickEscort(double dtMs);
  void RemoveEscort(DespawnReason reason);
  void SpawnDefend();                      // 3.10
  void TickDefend();
  void RemoveDefend(DespawnReason reason);

  SimContext& ctx_;
  QuestCardState card_;
  std::vector<std::pair<std::string, std::vector<ItemInstance>>> rewardCache_;
  std::vector<QuestNode> nodes_;
  // Per zone visit: spot indices gathered per "<questId>:<objectiveIndex>" (3.4) and the resolved spots.
  std::vector<std::pair<std::string, std::vector<int32_t>>> gathered_;
  std::vector<std::pair<std::string, std::vector<TilePos>>> spotCache_;
  EscortState escort_;
  DefendState defend_;
  GuideTarget guide_;
  double guideAccMs_ = 0;
  std::string guideTracked_;
  double nextObserverMs_ = 0;
};

}  // namespace abyss
