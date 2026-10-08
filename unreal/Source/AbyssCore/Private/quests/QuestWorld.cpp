// Live quest world: NPC interaction, quest card, turn-in, progress sources, escort / defend, guide (quests-story-ch1.md
// 3-6). STUB: owner area quests+story+pets. The quest card (OpenCard / reward-choice cache / view-story) is implemented.
#include "abyss/base/Platform.h"

#include "abyss/quests/QuestWorld.h"

#include "abyss/base/Assert.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/items/Inventory.h"
#include "abyss/items/LootGen.h"
#include "abyss/quests/Dialogue.h"
#include "abyss/world/Zone.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

QuestWorld::QuestWorld(SimContext& ctx) : ctx_(ctx) {}

void QuestWorld::OnZoneEnter() {
  ABYSS_UNIMPLEMENTED();
  (void)gatheredSpots_;
}

void QuestWorld::OnZoneExit() {
  nodes_.clear();
  escort_ = EscortState{};
  defend_ = DefendState{};
  card_ = QuestCardState{};
  guide_ = GuideTarget{};
}

void QuestWorld::InteractNpc(std::string_view npcId) { ABYSS_UNIMPLEMENTED(); }

bool QuestWorld::OpenCard(std::string_view npcId) {
  const bool refresh = card_.open && card_.npcId == npcId;
  if (!refresh && (card_.open || (ctx_.sys.dialogue != nullptr && ctx_.sys.dialogue->View().open))) return false;
  if (ctx_.sys.zone == nullptr || ctx_.sys.zone->FindNpc(npcId) == nullptr) return false;
  const int32_t heroLevel = ctx_.sys.hero != nullptr ? ctx_.sys.hero->Level() : 1;
  const std::vector<QuestOffer> offers = ctx_.sys.quests->Offers(npcId, heroLevel, OfferRule::QuestCard);
  if (offers.empty()) {
    if (refresh) CloseCard();
    return false;
  }
  QuestCardState next;
  next.open = true;
  next.npcId = std::string(npcId);
  for (const QuestOffer& o : offers) {
    QuestCardEntry e;
    e.quest = o.quest;
    e.turnIn = o.turnIn;
    if (o.turnIn && o.quest != nullptr) e.choices = EnsureRewardChoices(*o.quest);
    next.entries.push_back(std::move(e));
  }
  card_ = std::move(next);
  if (!refresh) ctx_.events.Emit(EvQuestCardOpened{card_.npcId});
  return true;
}

bool QuestWorld::OpenDialogueTree(std::string_view npcId) {
  const NpcDef* npc = ctx_.data.FindNpc(npcId);
  if (npc == nullptr || npc->dialogueTreeId.empty() || ctx_.sys.dialogue == nullptr) return false;
  const DialogueTree* tree = ctx_.data.Dialogues().Find(npc->dialogueTreeId);
  if (tree == nullptr) return false;
  CloseCard();
  ctx_.sys.dialogue->OpenTree(npcId, *tree);
  return true;
}

const std::vector<ItemInstance>& QuestWorld::EnsureRewardChoices(const QuestDef& quest) {
  for (const auto& [id, items] : rewardCache_) {
    if (id == quest.id) return items;
  }
  std::vector<ItemInstance> items;
  if (ctx_.sys.hero != nullptr && ctx_.sys.inventory != nullptr) {
    const LootContext lc{&ctx_.data, &ctx_.Rand(RngStream::Loot), &ctx_.sys.inventory->Uids()};
    items = GenerateQuestRewardChoices(lc, quest, ctx_.sys.hero->Class(), ctx_.sys.hero->Level());
  }
  rewardCache_.emplace_back(quest.id, std::move(items));
  return rewardCache_.back().second;
}

bool QuestWorld::AcceptFromCard(std::string_view questId) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

bool QuestWorld::TurnIn(std::string_view questId, int32_t choiceIndex) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

void QuestWorld::CloseCard() { card_ = QuestCardState{}; }

std::span<const ItemInstance> QuestWorld::RewardChoices(std::string_view questId) const {
  for (const auto& [id, items] : rewardCache_) {
    if (id == questId) return items;
  }
  return {};
}

void QuestWorld::OnMonsterKilled(const MonsterKilledMsg& m) { ABYSS_UNIMPLEMENTED(); }

void QuestWorld::RollQuestDrops(const MonsterKilledMsg& m) { ABYSS_UNIMPLEMENTED(); }

void QuestWorld::OnQuestAccepted(const QuestAcceptedMsg& m) { ABYSS_UNIMPLEMENTED(); }

void QuestWorld::OnQuestProgress(const QuestProgressMsg& m) { ABYSS_UNIMPLEMENTED(); }

void QuestWorld::Tick(double dtMs) {
  ABYSS_UNIMPLEMENTED();
  (void)nextObserverMs_;
}

void QuestWorld::OnTimer(const Timer& t) { ABYSS_UNIMPLEMENTED(); }

void QuestWorld::FillSnapshot(Snapshot& out) const {
  out.questCard = &card_;
  out.guide = guide_;
  out.escort = &escort_;
  out.defend = &defend_;
  for (const QuestNode& n : nodes_) {
    WorldMarkerView v;
    v.id = n.id;
    v.kind = n.clue ? MarkerKind::Clue : MarkerKind::GatherNode;
    v.pos = n.pos;
    v.key = n.itemKind;
    out.markers.push_back(std::move(v));
  }
}

}  // namespace abyss
