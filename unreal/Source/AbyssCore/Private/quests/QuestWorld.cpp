// Live quest world (quests-story-ch1.md 3 progress sources, 4.1 turn-in order, 5.1-5.3 guide and quest card, 6 NPC
// interaction; DECISIONS Q5 (sync on progress too), Q6 / W8 (ZoneRuntime walks to the NPC first), Q8, Q11 (keys, never
// raw zh), Q12, I10 (turn-in overflow to the stash)). Owner area: quests+story+pets.
//
// Runtime notes:
// * Every list of open quests is re-read by id after each UpdateProgress / Fail: bus listeners run synchronously.
// * Escort and defend run inside Tick (after the hero / monster updates of the step; the web ran them right after the
//   pet update, before the status ticks - presentation-equivalent, both per step).
// * RNG: quest drops draw RngStream::Quests (one draw per qualifying objective, active-quest insertion order then
//   objective order); fixed reward items and the pick-one gear draw RngStream::Loot.
#include "abyss/base/Platform.h"

#include "abyss/quests/QuestWorld.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/I18n.h"
#include "abyss/base/Math.h"
#include "abyss/base/StrUtil.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/hero/Rewards.h"
#include "abyss/items/Inventory.h"
#include "abyss/items/LootGen.h"
#include "abyss/items/Shop.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/quests/Achievements.h"
#include "abyss/quests/Dialogue.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"
#include "abyss/world/Zone.h"

namespace abyss {

namespace {

// Web constants that are not data tables (quests 14 tuning list; QuestWorld.ts / ZoneScene.ts).
constexpr double kQwObserverMs = 500;      // questObserverMs (explore check; SimulationScheduler 'quest-observers')
constexpr double kQwGuideRefreshMs = 250;  // guideRefreshMs
constexpr int32_t kQwClueSearchRings = 8;  // clueSearchRings
constexpr double kQwEscortJoinRange = 5;   // escort.joinRange
constexpr double kQwEscortFollowMin = 2;   // escort.followMin
constexpr double kQwEscortRepathMs = 400;  // escort.repathMs
constexpr int32_t kQwEscortCatchUpRings = 2;
constexpr double kQwNpcAlertRange = 3;     // npcAlertRange (6.4)

bool QwIsCollect(ObjectiveType t) { return t == ObjectiveType::Collect || t == ObjectiveType::CraftCollect; }

int32_t QwCurrent(const QuestProgress& p, size_t i) { return i < p.objectives.size() ? p.objectives[i] : 0; }

// getQuestTargetName (1.2): labelKey if it resolves, else data.questTarget.<targetId> if it resolves, else the zh
// fallback text (Q11: always through keys when they exist).
I18nArg QwTargetNameArg(const DataStore& data, const QuestObjectiveDef& obj) {
  const I18n& s = data.Strings();
  if (!obj.labelKey.empty() && s.Has(obj.labelKey)) return KeyArg("targetName", obj.labelKey);
  const std::string key = "data.questTarget." + obj.targetId;
  if (s.Has(key)) return KeyArg("targetName", key);
  return I18nArg{"targetName", obj.targetName, false};
}

std::string QwItemKind(const DataStore& data, const QuestObjectiveDef& obj) {
  if (!obj.itemKind.empty()) return obj.itemKind;
  return std::string(data.Quests().ItemKindFor(obj.targetId));
}

// Escort NPC name (Q11: data.escortNpc.<questId>, else the data's zh name).
I18nArg QwEscortNameArg(const DataStore& data, const QuestDef& q) {
  const std::string key = "data.escortNpc." + q.id;
  if (data.Strings().Has(key)) return KeyArg("npcName", key);
  return I18nArg{"npcName", q.escortNpc.name, false};
}

std::string QwSpotKey(std::string_view questId, int32_t objectiveIndex) { return StrCat(questId, ":", objectiveIndex); }

// Ids of the open quests (getActiveQuests order) - re-read after every progress call.
std::vector<std::string> QwOpenQuestIds(const QuestSystem& qs) {
  std::vector<std::string> out;
  for (const auto& [q, p] : qs.OpenQuests()) out.push_back(q->id);
  return out;
}

}  // namespace

QuestWorld::QuestWorld(SimContext& ctx) : ctx_(ctx) {}

// ---------------------------------------------------------------------------------------------------------------------
// Zone lifecycle
// ---------------------------------------------------------------------------------------------------------------------

void QuestWorld::OnZoneEnter() {
  nodes_.clear();
  gathered_.clear();
  spotCache_.clear();
  escort_ = EscortState{};
  defend_ = DefendState{};
  card_ = QuestCardState{};
  guide_ = GuideTarget{};
  guideAccMs_ = 0;  // the first step refreshes the guide
  guideTracked_ = ctx_.sys.quests != nullptr ? ctx_.sys.quests->Tracked() : std::string();
  nextObserverMs_ = 0;  // SimulationScheduler: the first check is due at once
  if (ctx_.sys.zone == nullptr || !ctx_.sys.zone->HasZone() || ctx_.sys.quests == nullptr) return;
  Sync();
  SpawnEscort();
  SpawnDefend();
}

void QuestWorld::OnZoneExit() {
  nodes_.clear();
  gathered_.clear();
  spotCache_.clear();
  escort_ = EscortState{};
  defend_ = DefendState{};
  card_ = QuestCardState{};
  guide_ = GuideTarget{};
}

// ---------------------------------------------------------------------------------------------------------------------
// NPC interaction (6.1) and the quest card (5.3)
// ---------------------------------------------------------------------------------------------------------------------

// interactNPC: log dialogue[0] (info; Q11 key), talk progress (any NPC type, before anything opens), craft phases, then
// by type: shop (blacksmith = shop + forge) / quest (card, else tree, else the linear panel) / stash keeper (I7).
void QuestWorld::InteractNpc(std::string_view npcId) {
  const NpcDef* def = ctx_.data.FindNpc(npcId);
  if (def == nullptr) return;
  const std::string id = def->id;
  if (!def->dialogue.empty()) ctx_.events.Log(MakeLoc("data.npc." + id + ".dialogue.0"), LogType::Info);
  if (ctx_.sys.quests != nullptr) ctx_.sys.quests->UpdateProgress(ObjectiveType::Talk, id);
  AdvanceCraftFromNpc(id);
  const NpcPlacement* placement = ctx_.sys.zone != nullptr && ctx_.sys.zone->HasZone() ? ctx_.sys.zone->FindNpc(id)
                                                                                       : nullptr;
  const EntityId entity = placement != nullptr ? placement->id : kNoEntity;
  ctx_.bus.Publish(NpcInteractedMsg{id, entity});
  // NPC_INTERACT (web: quest NPCs only; shops announce themselves with SHOP_OPEN, the stash with its panel).
  if (def->type == NpcType::Quest) ctx_.events.Emit(EvNpcInteracted{id, entity});
  switch (def->type) {
    case NpcType::Blacksmith:
    case NpcType::Merchant:
      if (ctx_.sys.shop != nullptr) ctx_.sys.shop->Open(id, def->type == NpcType::Blacksmith);
      break;
    case NpcType::Quest: {
      if (OpenCard(id)) break;
      if (card_.open) break;  // a card of this NPC is already up (refreshed)
      DialogueSystem* dialogue = ctx_.sys.dialogue;
      if (dialogue == nullptr || dialogue->View().open) break;
      const DialogueTree* tree =
          def->dialogueTreeId.empty() ? nullptr : ctx_.data.Dialogues().Find(def->dialogueTreeId);
      if (tree != nullptr) {
        dialogue->OpenTree(id, *tree);
      } else {
        dialogue->OpenLinear(id);
      }
      break;
    }
    case NpcType::Stash:
      if (ctx_.sys.inventory != nullptr) ctx_.sys.inventory->OpenStash(id);
      break;
  }
}

bool QuestWorld::OpenCard(std::string_view npcId) {
  const bool refresh = card_.open && card_.npcId == npcId;
  if (!refresh && (card_.open || (ctx_.sys.dialogue != nullptr && ctx_.sys.dialogue->View().open))) return false;
  if (ctx_.sys.zone == nullptr || !ctx_.sys.zone->HasZone() || ctx_.sys.zone->FindNpc(npcId) == nullptr) return false;
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

// getQuestRewardChoices (4.2): generated once per session (the card shows exactly what the turn-in grants; a reload
// rerolls - Q9 kept).
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

// Accept button (5.3): acceptQuest, the quest becomes the guided one (setTracked - the web pins it even when the accept
// fails), toast sys.questCard.accepted, the card closes.
bool QuestWorld::AcceptFromCard(std::string_view questId) {
  const QuestDef* q = ctx_.data.FindQuest(questId);
  if (q == nullptr || ctx_.sys.quests == nullptr) return false;
  const bool ok = ctx_.sys.quests->Accept(questId);
  ctx_.sys.quests->SetTracked(q->id);
  if (ok) ctx_.events.Emit(EvBanner{BannerKind::Toast, MakeLoc("sys.questCard.accepted", {KeyArg("name", q->nameKey)}),
                                    LocText{}});
  CloseCard();
  return ok;
}

// turnInQuest(questId, choiceIndex) (4.1), exact order: choices (generate-or-cache before the status change) ->
// QuestSystem::TurnIn (QuestTurnedInMsg listeners: story cutscene enqueue, embers + unlocks) -> exp -> gold -> fixed
// items then the chosen gear (bag, else the stash ignoring its capacity) each logged zone.quest.rewardItem -> cache
// dropped -> pet -> achievements 'quest' -> autosave. The card's toast + close follow.
bool QuestWorld::TurnIn(std::string_view questId, int32_t choiceIndex) {
  const QuestDef* q = ctx_.data.FindQuest(questId);
  if (q == nullptr || ctx_.sys.quests == nullptr) return false;
  const std::vector<ItemInstance> choices = EnsureRewardChoices(*q);
  const QuestRewardDef* reward = ctx_.sys.quests->TurnIn(questId);
  if (reward == nullptr) return false;
  Sync();  // QUEST_TURNED_IN -> sync + guide refresh
  guideAccMs_ = 0;
  RewardService* rewards = ctx_.sys.rewards;
  Hero* hero = ctx_.sys.hero;
  if (rewards != nullptr) {
    rewards->GrantExp(reward->exp, ExpSource::Quest);
    rewards->ChangeGold(reward->gold, GoldReason::QuestReward);
  }
  std::vector<ItemInstance> granted;
  if (hero != nullptr && ctx_.sys.inventory != nullptr) {
    const LootContext lc{&ctx_.data, &ctx_.Rand(RngStream::Loot), &ctx_.sys.inventory->Uids()};
    for (const std::string& baseId : reward->items) {
      std::optional<ItemInstance> item = CreateItem(lc, baseId, hero->Level(), ItemQuality::Normal);
      if (!item.has_value()) continue;
      item->identified = true;
      granted.push_back(std::move(*item));
    }
  }
  if (!choices.empty()) {
    const int32_t last = static_cast<int32_t>(choices.size()) - 1;
    const int32_t pick = (std::max)(0, (std::min)(last, choiceIndex));
    granted.push_back(choices[static_cast<size_t>(pick)]);
  }
  for (ItemInstance& item : granted) {
    const I18nArg name = ItemNameArg("name", item, ctx_.data);
    if (rewards != nullptr) rewards->GrantItem(item, OverflowPolicy::Stash, ItemSource::QuestReward);
    ctx_.events.Log(MakeLoc("zone.quest.rewardItem", {name}), LogType::Loot);
  }
  for (size_t i = 0; i < rewardCache_.size(); ++i) {
    if (rewardCache_[i].first == q->id) {
      rewardCache_.erase(rewardCache_.begin() + static_cast<std::ptrdiff_t>(i));
      break;
    }
  }
  if (!reward->petReward.empty() && ctx_.sys.pets != nullptr) ctx_.sys.pets->AddPet(reward->petReward);
  if (ctx_.sys.achievements != nullptr) ctx_.sys.achievements->OnQuestTurnedIn(QuestTurnedInMsg{q->id});
  ctx_.bus.Publish(SaveRequestMsg{SaveReason::QuestTurnIn});
  ctx_.events.Emit(EvBanner{BannerKind::Toast, MakeLoc("sys.questCard.turnedIn", {KeyArg("name", q->nameKey)}),
                            LocText{}});
  CloseCard();
  return true;
}

void QuestWorld::CloseCard() { card_ = QuestCardState{}; }

std::span<const ItemInstance> QuestWorld::RewardChoices(std::string_view questId) const {
  for (const auto& [id, items] : rewardCache_) {
    if (id == questId) return items;
  }
  return {};
}

// advanceCraftQuestFromNpc (3.11): craft NPC with every craft_collect done -> the first craft_craft objective; deliver
// NPC with every craft_craft done -> the first craft_deliver objective (both in one interaction when one NPC holds both
// roles).
void QuestWorld::AdvanceCraftFromNpc(std::string_view npcId) {
  QuestSystem* qs = ctx_.sys.quests;
  if (qs == nullptr) return;
  for (const std::string& qid : QwOpenQuestIds(*qs)) {
    const QuestDef* q = ctx_.data.FindQuest(qid);
    const QuestProgress* p = qs->Progress(qid);
    if (q == nullptr || p == nullptr || q->type != QuestType::Craft || p->status != QuestStatus::Active) continue;
    if (!q->craftPhases.present) continue;
    const auto firstOf = [q](ObjectiveType t) -> int32_t {
      for (size_t i = 0; i < q->objectives.size(); ++i) {
        if (q->objectives[i].type == t) return static_cast<int32_t>(i);
      }
      return -1;
    };
    if (q->craftPhases.craftNpc == npcId && QuestSystem::CraftPhaseDone(*q, *p, ObjectiveType::CraftCollect)) {
      const int32_t i = firstOf(ObjectiveType::CraftCraft);
      if (i >= 0 && QwCurrent(*p, static_cast<size_t>(i)) < q->objectives[static_cast<size_t>(i)].required) {
        const QuestObjectiveDef& obj = q->objectives[static_cast<size_t>(i)];
        qs->UpdateProgress(ObjectiveType::CraftCraft, obj.targetId);
        ctx_.events.Log(MakeLoc("zone.craft.complete", {QwTargetNameArg(ctx_.data, obj)}), LogType::System);
      }
    }
    p = qs->Progress(qid);
    if (p == nullptr) continue;
    if (q->craftPhases.deliverNpc == npcId && QuestSystem::CraftPhaseDone(*q, *p, ObjectiveType::CraftCraft)) {
      const int32_t i = firstOf(ObjectiveType::CraftDeliver);
      if (i >= 0 && QwCurrent(*p, static_cast<size_t>(i)) < q->objectives[static_cast<size_t>(i)].required) {
        const QuestObjectiveDef& obj = q->objectives[static_cast<size_t>(i)];
        qs->UpdateProgress(ObjectiveType::CraftDeliver, obj.targetId);
        ctx_.events.Log(MakeLoc("zone.deliver.complete", {QwTargetNameArg(ctx_.data, obj)}), LogType::System);
      }
    }
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Progress sources: kills (3.1), drops (3.3), quest events
// ---------------------------------------------------------------------------------------------------------------------

// Kill hook step 4: updateProgress('kill', defId) - not zone-filtered; hunt leaders carry the hunt id.
void QuestWorld::OnMonsterKilled(const MonsterKilledMsg& m) {
  if (ctx_.sys.quests != nullptr) ctx_.sys.quests->UpdateProgress(ObjectiveType::Kill, m.defId);
}

// rollQuestDrops (3.3): active quests of this zone, unfinished collect objectives; gather sources never drop;
// source-less craft_collect never drops; source-less collect objectives use the fallback chance, one per quest.
void QuestWorld::RollQuestDrops(const MonsterKilledMsg& m) {
  QuestSystem* qs = ctx_.sys.quests;
  if (qs == nullptr || ctx_.sys.zone == nullptr || !ctx_.sys.zone->HasZone()) return;
  const std::string& mapId = ctx_.sys.zone->MapId();
  const double fallback = ctx_.data.Quests().tuning.fallbackCollectChance;
  for (const std::string& qid : QwOpenQuestIds(*qs)) {
    const QuestDef* q = ctx_.data.FindQuest(qid);
    const QuestProgress* p0 = qs->Progress(qid);
    if (q == nullptr || p0 == nullptr || p0->status != QuestStatus::Active || q->zone != mapId) continue;
    bool legacyRolled = false;
    for (size_t i = 0; i < q->objectives.size(); ++i) {
      const QuestObjectiveDef& obj = q->objectives[i];
      const QuestProgress* p = qs->Progress(qid);
      if (p == nullptr) break;
      if (!QwIsCollect(obj.type) || QwCurrent(*p, i) >= obj.required) continue;
      if (obj.sourceKind == ItemSourceKind::Gather) continue;
      if (obj.sourceKind == ItemSourceKind::None && obj.type == ObjectiveType::CraftCollect) continue;
      double chance = fallback;
      if (obj.sourceKind == ItemSourceKind::Drop) {
        chance = std::find(obj.dropMonsters.begin(), obj.dropMonsters.end(), m.defId) != obj.dropMonsters.end()
                     ? obj.dropChance
                     : 0.0;
      }
      if (obj.sourceKind == ItemSourceKind::None) {
        if (legacyRolled) continue;
        legacyRolled = true;
      }
      if (chance <= 0) continue;
      const double roll = ctx_.Rand(RngStream::Quests).Float01();
      if (roll >= (obj.sourceKind != ItemSourceKind::None ? chance : fallback)) continue;
      QuestProgressSource src;
      src.hasFrom = true;
      src.from = m.pos;
      src.itemKind = QwItemKind(ctx_.data, obj);
      qs->UpdateProgress(obj.type, obj.targetId, 1, &src);
    }
  }
}

void QuestWorld::OnQuestAccepted(const QuestAcceptedMsg& m) {
  Sync();
  guideAccMs_ = 0;
  const QuestDef* q = ctx_.data.FindQuest(m.questId);
  if (q == nullptr || ctx_.sys.zone == nullptr || !ctx_.sys.zone->HasZone() || q->zone != ctx_.sys.zone->MapId()) {
    return;
  }
  if (q->type == QuestType::Escort && !escort_.active) SpawnEscort();
  if (q->type == QuestType::Defend && !defend_.active) SpawnDefend();
}

void QuestWorld::OnQuestProgress(const QuestProgressMsg& m) {
  Sync();
  if (m.completesQuest) guideAccMs_ = 0;
}

// ---------------------------------------------------------------------------------------------------------------------
// Gather nodes and clue marks (3.4, 3.5)
// ---------------------------------------------------------------------------------------------------------------------

const std::vector<TilePos>& QuestWorld::SpotsFor(const QuestDef& quest, int32_t objectiveIndex) {
  const std::string key = QwSpotKey(quest.id, objectiveIndex);
  for (const auto& [k, spots] : spotCache_) {
    if (k == key) return spots;
  }
  const QuestObjectiveDef& obj = quest.objectives[static_cast<size_t>(objectiveIndex)];
  const ZoneRuntime* zone = ctx_.sys.zone;
  std::vector<TilePos> spots = ResolveGatherSpots(
      obj.gatherArea, obj.gatherCount, [zone](int32_t c, int32_t r) { return zone->Walkable(c, r); }, key);
  spotCache_.emplace_back(key, std::move(spots));
  return spotCache_.back().second;
}

bool QuestWorld::IsGathered(std::string_view questId, int32_t objectiveIndex, int32_t spot) const {
  const std::string key = QwSpotKey(questId, objectiveIndex);
  for (const auto& [k, set] : gathered_) {
    if (k == key) return std::find(set.begin(), set.end(), spot) != set.end();
  }
  return false;
}

void QuestWorld::RemoveNode(EntityId id, DespawnReason reason) {
  for (size_t i = 0; i < nodes_.size(); ++i) {
    if (nodes_[i].id != id) continue;
    nodes_.erase(nodes_.begin() + static_cast<std::ptrdiff_t>(i));
    ctx_.events.Emit(EvEntityDespawned{id, EntityKind::Prop, reason});
    return;
  }
}

// sync(): wanted nodes = every active quest of this zone x gather objective (current < required) x spot not gathered
// in this visit; wanted marks = every investigate_clue objective with a location (current < required), placed on the
// nearest walkable tile (rings 1..8). Existing ones keep their order, new ones are appended, unwanted ones removed.
void QuestWorld::Sync() {
  QuestSystem* qs = ctx_.sys.quests;
  const ZoneRuntime* zone = ctx_.sys.zone;
  if (qs == nullptr || zone == nullptr || !zone->HasZone()) return;
  const std::string& mapId = zone->MapId();
  std::vector<EntityId> wanted;
  const auto find = [this](const std::string& questId, int32_t obj, int32_t spot, bool clue) -> QuestNode* {
    for (QuestNode& n : nodes_) {
      if (n.clue == clue && n.questId == questId && n.objectiveIndex == obj && n.spotIndex == spot) return &n;
    }
    return nullptr;
  };
  const auto spawn = [this](QuestNode n) {
    n.id = ctx_.ids.Next();
    if (n.clue) {
      ctx_.events.Emit(EvEntitySpawned{n.id, EntityKind::Prop, "quest_clue", "clue_mark", n.pos, Vec2(1, 0), 1.0});
    } else {
      ctx_.events.Emit(
          EvEntitySpawned{n.id, EntityKind::Prop, "quest_node:" + n.itemKind, n.itemKind, n.pos, Vec2(1, 0), 1.0});
    }
    nodes_.push_back(std::move(n));
    return nodes_.back().id;
  };
  // Gather nodes.
  for (const auto& [q, p] : qs->OpenQuests()) {
    if (p->status != QuestStatus::Active || q->zone != mapId) continue;
    for (size_t i = 0; i < q->objectives.size(); ++i) {
      const QuestObjectiveDef& obj = q->objectives[i];
      if (!QwIsCollect(obj.type) || obj.sourceKind != ItemSourceKind::Gather) continue;
      if (QwCurrent(*p, i) >= obj.required) continue;
      const int32_t oi = static_cast<int32_t>(i);
      const std::vector<TilePos> spots = SpotsFor(*q, oi);
      for (size_t s = 0; s < spots.size(); ++s) {
        const int32_t si = static_cast<int32_t>(s);
        if (IsGathered(q->id, oi, si)) continue;
        if (QuestNode* existing = find(q->id, oi, si, false)) {
          wanted.push_back(existing->id);
          continue;
        }
        QuestNode n;
        n.questId = q->id;
        n.objectiveIndex = oi;
        n.spotIndex = si;
        n.itemKind = QwItemKind(ctx_.data, obj);
        n.pos = spots[s].Center();
        wanted.push_back(spawn(std::move(n)));
      }
    }
  }
  // Clue marks.
  for (const auto& [q, p] : qs->OpenQuests()) {
    if (p->status != QuestStatus::Active || q->zone != mapId) continue;
    for (size_t i = 0; i < q->objectives.size(); ++i) {
      const QuestObjectiveDef& obj = q->objectives[i];
      if (obj.type != ObjectiveType::InvestigateClue || !obj.hasLocation) continue;
      if (QwCurrent(*p, i) >= obj.required) continue;
      const int32_t oi = static_cast<int32_t>(i);
      if (QuestNode* existing = find(q->id, oi, -1, true)) {
        wanted.push_back(existing->id);
        continue;
      }
      TilePos spot;
      const TilePos at(obj.location.col, obj.location.row);
      if (!NearestWalkableTile(at, [zone](int32_t c, int32_t r) { return zone->Walkable(c, r); }, kQwClueSearchRings,
                               spot)) {
        continue;
      }
      QuestNode n;
      n.questId = q->id;
      n.objectiveIndex = oi;
      n.clue = true;
      n.pos = spot.Center();
      wanted.push_back(spawn(std::move(n)));
    }
  }
  std::vector<EntityId> drop;
  for (const QuestNode& n : nodes_) {
    if (std::find(wanted.begin(), wanted.end(), n.id) == wanted.end()) drop.push_back(n.id);
  }
  for (EntityId id : drop) RemoveNode(id, DespawnReason::Removed);
}

bool QuestWorld::ClueTile(std::string_view questId, int32_t objectiveIndex, Vec2& out) const {
  for (const QuestNode& n : nodes_) {
    if (n.clue && n.questId == questId && n.objectiveIndex == objectiveIndex) {
      out = n.pos;
      return true;
    }
  }
  return false;
}

// update(): every node within gatherRange (hypot) is gathered, then every clue mark within clueRange is examined.
void QuestWorld::TickGatherAndClues() {
  QuestSystem* qs = ctx_.sys.quests;
  const Hero* hero = ctx_.sys.hero;
  if (qs == nullptr || hero == nullptr) return;
  const Vec2 hp = hero->Position();
  const QuestTuningDef& tun = ctx_.data.Quests().tuning;
  const std::vector<QuestNode> snapshot = nodes_;
  for (const QuestNode& n : snapshot) {
    if (n.clue) continue;
    if (JsHypot(n.pos.x - hp.x, n.pos.y - hp.y) > tun.gatherRange) continue;
    if (std::none_of(nodes_.begin(), nodes_.end(), [&n](const QuestNode& x) { return x.id == n.id; })) continue;
    const QuestDef* q = ctx_.data.FindQuest(n.questId);
    if (q == nullptr || n.objectiveIndex < 0 || static_cast<size_t>(n.objectiveIndex) >= q->objectives.size()) continue;
    const QuestObjectiveDef& obj = q->objectives[static_cast<size_t>(n.objectiveIndex)];
    const std::string key = QwSpotKey(n.questId, n.objectiveIndex);
    bool found = false;
    for (auto& [k, set] : gathered_) {
      if (k == key) {
        set.push_back(n.spotIndex);
        found = true;
      }
    }
    if (!found) gathered_.emplace_back(key, std::vector<int32_t>{n.spotIndex});
    RemoveNode(n.id, DespawnReason::Collected);
    QuestProgressSource src;
    src.hasFrom = true;
    src.from = n.pos;
    src.itemKind = n.itemKind;
    qs->UpdateProgress(obj.type == ObjectiveType::CraftCollect ? ObjectiveType::CraftCollect : ObjectiveType::Collect,
                       obj.targetId, 1, &src);
  }
  const std::vector<QuestNode> clues = nodes_;
  for (const QuestNode& n : clues) {
    if (!n.clue) continue;
    if (JsHypot(n.pos.x - hp.x, n.pos.y - hp.y) > tun.clueRange) continue;
    if (std::none_of(nodes_.begin(), nodes_.end(), [&n](const QuestNode& x) { return x.id == n.id; })) continue;
    RemoveNode(n.id, DespawnReason::Collected);
    const QuestDef* q = ctx_.data.FindQuest(n.questId);
    if (q == nullptr || n.objectiveIndex < 0 || static_cast<size_t>(n.objectiveIndex) >= q->objectives.size()) continue;
    const QuestObjectiveDef& obj = q->objectives[static_cast<size_t>(n.objectiveIndex)];
    const std::string noteKey = "data.questClue." + obj.targetId;
    const bool hasNote = ctx_.data.Strings().Has(noteKey);
    ctx_.events.Log(MakeLoc("zone.quest.clueFound", {QwTargetNameArg(ctx_.data, obj)}), LogType::System);
    if (hasNote) ctx_.events.Log(MakeLoc(noteKey), LogType::System);
    // The note floats above the spot (UE shows the target label over it from the clue's EvQuestUpdate).
    EvFloatingText ft;
    ft.kind = FloatingTextKind::Custom;
    ft.pos = n.pos;
    if (hasNote) {
      ft.text = MakeLoc(noteKey);
    } else {
      const I18nArg name = QwTargetNameArg(ctx_.data, obj);
      ft.text = name.isKey ? MakeLoc(name.value) : MakeLoc(obj.targetName);
    }
    ctx_.events.Emit(std::move(ft));
    QuestProgressSource src;
    src.hasFrom = false;
    src.from = n.pos;
    qs->UpdateProgress(ObjectiveType::InvestigateClue, obj.targetId, 1, &src);
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Explore (3.6)
// ---------------------------------------------------------------------------------------------------------------------

void QuestWorld::CheckExplore() {
  QuestSystem* qs = ctx_.sys.quests;
  const Hero* hero = ctx_.sys.hero;
  if (qs == nullptr || hero == nullptr || ctx_.sys.zone == nullptr || !ctx_.sys.zone->HasZone()) return;
  const std::string& mapId = ctx_.sys.zone->MapId();
  const Vec2 hp = hero->Position();
  for (const std::string& qid : QwOpenQuestIds(*qs)) {
    const QuestDef* q = ctx_.data.FindQuest(qid);
    const QuestProgress* p0 = qs->Progress(qid);
    if (q == nullptr || p0 == nullptr || p0->status != QuestStatus::Active || q->zone != mapId) continue;
    for (size_t i = 0; i < q->objectives.size(); ++i) {
      const QuestObjectiveDef& obj = q->objectives[i];
      const QuestProgress* p = qs->Progress(qid);
      if (p == nullptr) break;
      if (obj.type != ObjectiveType::Explore || !obj.hasLocation || QwCurrent(*p, i) >= obj.required) continue;
      const double dx = hp.x - obj.location.col;
      const double dy = hp.y - obj.location.row;
      if (std::sqrt(dx * dx + dy * dy) > obj.location.radius) continue;
      qs->UpdateProgress(ObjectiveType::Explore, obj.targetId);
      ctx_.events.Log(MakeLoc("zone.quest.exploreFound", {QwTargetNameArg(ctx_.data, obj)}), LogType::System);
    }
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Escort (3.9)
// ---------------------------------------------------------------------------------------------------------------------

// spawnEscortNpc: the first open escort quest of this zone (only one escort at a time) at its start tile, not joined,
// hp = level * 20 + 100.
void QuestWorld::SpawnEscort() {
  if (escort_.active) RemoveEscort(DespawnReason::Removed);
  QuestSystem* qs = ctx_.sys.quests;
  if (qs == nullptr || ctx_.sys.zone == nullptr || !ctx_.sys.zone->HasZone()) return;
  const std::string& mapId = ctx_.sys.zone->MapId();
  const QuestTuningDef& tun = ctx_.data.Quests().tuning;
  for (const auto& [q, p] : qs->OpenQuests()) {
    if (q->type != QuestType::Escort || p->status != QuestStatus::Active || q->zone != mapId || !q->hasEscortNpc) {
      continue;
    }
    escort_ = EscortState{};
    escort_.active = true;
    escort_.questId = q->id;
    escort_.entity = ctx_.ids.Next();
    escort_.pos = q->escortNpc.start.Center();
    escort_.dest = q->escortNpc.dest;
    escort_.maxHp = q->level * tun.escortHpPerLevel + tun.escortHpBase;
    escort_.hp = escort_.maxHp;
    ctx_.events.Emit(EvEntitySpawned{escort_.entity, EntityKind::Escort, q->id, q->escortNpc.spriteKey, escort_.pos,
                                     Vec2(0, 1), 1.0});
    ctx_.events.Log(MakeLoc("zone.escort.npcAppeared", {QwEscortNameArg(ctx_.data, *q)}), LogType::System);
    return;
  }
}

void QuestWorld::RemoveEscort(DespawnReason reason) {
  if (!escort_.active) return;
  ctx_.events.Emit(EvEntityDespawned{escort_.entity, EntityKind::Escort, reason});
  escort_ = EscortState{};
}

void QuestWorld::TickEscort(double dtMs) {
  if (!escort_.active) return;
  QuestSystem* qs = ctx_.sys.quests;
  const Hero* hero = ctx_.sys.hero;
  const ZoneRuntime* zone = ctx_.sys.zone;
  const QuestDef* q = ctx_.data.FindQuest(escort_.questId);
  if (qs == nullptr || hero == nullptr || zone == nullptr || !zone->HasZone() || q == nullptr) return;
  const QuestTuningDef& tun = ctx_.data.Quests().tuning;
  const double now = ctx_.Now();
  const Vec2 hp = hero->Position();
  const double dx = hp.x - escort_.pos.x, dy = hp.y - escort_.pos.y;
  const double dist = std::sqrt(dx * dx + dy * dy);
  if (!escort_.joined) {
    if (dist > kQwEscortJoinRange) return;  // waits at the start tile (no damage, no arrival check)
    escort_.joined = true;
    ctx_.events.Log(MakeLoc("zone.escort.joined", {QwEscortNameArg(ctx_.data, *q)}), LogType::System);
  }
  if (dist > tun.escortCatchUpTiles) {
    // Left far behind: catch up out of sight (rings 1..2 around the hero's rounded tile, centre excluded).
    TilePos spot;
    if (zone->Paths().FindWalkableNear(TilePos(JsRoundInt(hp.x), JsRoundInt(hp.y)), kQwEscortCatchUpRings, spot)) {
      const Vec2 from = escort_.pos;
      escort_.pos = spot.Center();
      escort_.path.clear();
      ctx_.events.Emit(EvEntityTeleported{escort_.entity, from, escort_.pos, TeleportReason::CatchUp});
    }
  } else if (dist > kQwEscortFollowMin) {
    if (now >= escort_.repathAtMs || escort_.path.empty()) {
      escort_.repathAtMs = now + kQwEscortRepathMs;
      std::vector<TilePos> path;
      zone->Paths().FindPath(JsRound(escort_.pos.x), JsRound(escort_.pos.y), JsRound(hp.x), JsRound(hp.y), path);
      // Stop a tile or two short of the hero: path.slice(1, max(1, len - 1)).
      escort_.path.clear();
      const size_t end = (std::max)(static_cast<size_t>(1), path.empty() ? static_cast<size_t>(0) : path.size() - 1);
      for (size_t i = 1; i < end && i < path.size(); ++i) escort_.path.push_back(path[i]);
    }
    double budget = (hero->Derived().moveSpeed / tun.escortSpeedDivisor) * tun.escortSpeedFactor * (dtMs / 1000.0);
    while (budget > 0 && !escort_.path.empty()) {
      const Vec2 next = escort_.path.front().Center();
      const double sx = next.x - escort_.pos.x, sy = next.y - escort_.pos.y;
      const double sd = std::sqrt(sx * sx + sy * sy);
      if (sd <= budget) {
        escort_.pos = next;
        escort_.path.erase(escort_.path.begin());
        budget -= sd;
      } else {
        escort_.pos.x += (sx / sd) * budget;
        escort_.pos.y += (sy / sd) * budget;
        budget = 0;
      }
    }
  } else {
    escort_.path.clear();
  }
  // Abstract chip damage: aggro monsters within 4 tiles, one hit per monster per 2 s (monsters 4.4).
  bool died = false;
  if (ctx_.sys.monsters != nullptr) {
    const EntityId self = escort_.entity;
    ctx_.sys.monsters->ChipEscort(escort_.pos, [this, self, &died](EntityId, double dmg) {
      if (!escort_.active || escort_.entity != self) return false;
      escort_.hp -= dmg;
      EvFloatingText ft;
      ft.kind = FloatingTextKind::HeroDamage;
      ft.anchor = self;
      ft.pos = escort_.pos;
      ft.value = dmg;
      ctx_.events.Emit(std::move(ft));
      if (escort_.hp <= 0) {
        died = true;
        return false;
      }
      return true;
    });
  }
  if (died) {
    // handleEscortNpcDeath: fail (reacceptable -> the giver offers it again), log; the actor plays its 600 ms death fade
    // from EvEntityDespawned{Died} (the web kept updating the dying escort for those 600 ms and could log twice).
    const std::string questId = escort_.questId;
    RemoveEscort(DespawnReason::Died);
    qs->Fail(questId);
    ctx_.events.Log(MakeLoc("zone.escort.npcDied"), LogType::System);
    Sync();
    guideAccMs_ = 0;
    return;
  }
  // Arrival: the escort within 5 tiles of the destination and the hero within 6.
  const Vec2 dest = escort_.dest.Center();
  if (DistSq(escort_.pos, dest) <= tun.escortArriveEscortSq && DistSq(hp, dest) <= tun.escortArriveHeroSq) {
    const std::string questId = escort_.questId;
    const I18nArg name = QwEscortNameArg(ctx_.data, *q);
    RemoveEscort(DespawnReason::Removed);
    for (const QuestObjectiveDef& obj : q->objectives) {
      if (obj.type == ObjectiveType::Escort) qs->UpdateProgress(ObjectiveType::Escort, obj.targetId);
    }
    ctx_.events.Log(MakeLoc("zone.escort.complete", {name}), LogType::System);
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Defend (3.10)
// ---------------------------------------------------------------------------------------------------------------------

void QuestWorld::SpawnDefend() {
  if (defend_.active) RemoveDefend(DespawnReason::Removed);
  QuestSystem* qs = ctx_.sys.quests;
  if (qs == nullptr || ctx_.sys.zone == nullptr || !ctx_.sys.zone->HasZone()) return;
  const std::string& mapId = ctx_.sys.zone->MapId();
  const QuestTuningDef& tun = ctx_.data.Quests().tuning;
  for (const auto& [q, p] : qs->OpenQuests()) {
    if (q->type != QuestType::Defend || p->status != QuestStatus::Active || q->zone != mapId || !q->hasDefendTarget) {
      continue;
    }
    defend_ = DefendState{};
    defend_.active = true;
    defend_.questId = q->id;
    defend_.entity = ctx_.ids.Next();
    defend_.pos = q->defendTarget.pos.Center();
    defend_.totalWaves = q->defendTarget.totalWaves;
    for (size_t i = 0; i < q->objectives.size(); ++i) {  // resume the wave count from the progress
      if (q->objectives[i].type == ObjectiveType::DefendWave) {
        defend_.wave = QwCurrent(*p, i);
        break;
      }
    }
    defend_.maxHp = q->level * tun.defendHpPerLevel + tun.defendHpBase;
    defend_.hp = defend_.maxHp;
    ctx_.events.Emit(EvEntitySpawned{defend_.entity, EntityKind::DefendTarget, q->id, q->defendTarget.spriteKey,
                                     defend_.pos, Vec2(0, 1), 1.0});
    const std::string key = "data.defendTarget." + q->id;
    const I18nArg name = ctx_.data.Strings().Has(key) ? KeyArg("targetName", key)
                                                      : I18nArg{"targetName", q->defendTarget.name, false};
    ctx_.events.Log(MakeLoc("zone.defend.targetNeedsProtection", {name}), LogType::System);
    return;
  }
}

void QuestWorld::RemoveDefend(DespawnReason reason) {
  if (!defend_.active) return;
  ctx_.events.Emit(EvEntityDespawned{defend_.entity, EntityKind::DefendTarget, reason});
  defend_ = DefendState{};
}

void QuestWorld::TickDefend() {
  if (!defend_.active) return;
  QuestSystem* qs = ctx_.sys.quests;
  const Hero* hero = ctx_.sys.hero;
  const QuestDef* q = ctx_.data.FindQuest(defend_.questId);
  if (qs == nullptr || hero == nullptr || q == nullptr) return;
  const QuestTuningDef& tun = ctx_.data.Quests().tuning;
  const double now = ctx_.Now();
  if (defend_.wave >= defend_.totalWaves && !defend_.waveActive) {
    ctx_.events.Log(MakeLoc("zone.defend.allWavesCleared"), LogType::System);
    RemoveDefend(DespawnReason::Removed);
    return;
  }
  if (!defend_.waveActive && DistSq(hero->Position(), defend_.pos) < tun.defendStartRangeSq) {
    if (!defend_.timerStarted) {
      defend_.timerStarted = true;
      defend_.timerMs = now;
    }
    if (now - defend_.timerMs > tun.defendWaveDelayMs) {
      defend_.waveMonsters.clear();
      if (ctx_.sys.monsters != nullptr) defend_.waveMonsters = ctx_.sys.monsters->SpawnDefendWave(defend_.pos, defend_.wave);
      defend_.waveActive = true;
      defend_.timerStarted = false;
      ctx_.events.Log(MakeLoc("zone.defend.waveIncoming",
                              {{"current", ToStr(defend_.wave + 1)}, {"total", ToStr(defend_.totalWaves)}}),
                      LogType::System);
    }
  }
  if (!defend_.waveActive) return;
  std::vector<EntityId> alive;
  if (ctx_.sys.monsters != nullptr) {
    for (EntityId id : defend_.waveMonsters) {
      const MonsterInstance* m = ctx_.sys.monsters->Find(id);
      if (m != nullptr && m->IsAlive()) alive.push_back(id);
    }
  }
  if (alive.empty()) {
    defend_.wave += 1;
    defend_.waveActive = false;
    defend_.timerStarted = true;  // the next wave's delay starts now
    defend_.timerMs = now;
    for (const QuestObjectiveDef& obj : q->objectives) {
      if (obj.type == ObjectiveType::DefendWave) {
        qs->UpdateProgress(ObjectiveType::DefendWave, obj.targetId);
        break;
      }
    }
    return;
  }
  bool destroyed = false;
  const EntityId self = defend_.entity;
  ctx_.sys.monsters->ChipDefendTarget(alive, defend_.pos, [this, self, &destroyed](EntityId, double dmg) {
    if (!defend_.active || defend_.entity != self) return false;
    defend_.hp -= dmg;
    EvFloatingText ft;
    ft.kind = FloatingTextKind::HeroDamage;
    ft.anchor = self;
    ft.pos = defend_.pos;
    ft.value = dmg;
    ctx_.events.Emit(std::move(ft));
    if (defend_.hp <= 0) {
      destroyed = true;
      return false;
    }
    return true;
  });
  if (destroyed) {
    const std::string questId = defend_.questId;
    RemoveDefend(DespawnReason::Died);
    qs->Fail(questId);
    ctx_.events.Log(MakeLoc("zone.defend.targetDestroyed"), LogType::System);
    Sync();
    guideAccMs_ = 0;
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Guide (5.1-5.2)
// ---------------------------------------------------------------------------------------------------------------------

GuideWorld QuestWorld::MakeGuideWorld() const {
  GuideWorld w;
  const Hero* hero = ctx_.sys.hero;
  w.player = hero != nullptr ? hero->Position() : Vec2();
  const ZoneRuntime* zone = ctx_.sys.zone;
  const MonsterSystem* monsters = ctx_.sys.monsters;
  const DataStore* data = &ctx_.data;
  w.npcTile = [zone](std::string_view npcId, Vec2& out) {
    const NpcPlacement* p = zone != nullptr && zone->HasZone() ? zone->FindNpc(npcId) : nullptr;
    if (p == nullptr) return false;
    out = p->pos;
    return true;
  };
  w.monsters = [monsters](const std::vector<std::string>& ids, std::vector<Vec2>& out) {
    if (monsters == nullptr) return;
    for (const MonsterInstance& m : monsters->All()) {
      if (!m.IsAlive()) continue;
      if (std::find(ids.begin(), ids.end(), m.def.id) != ids.end()) out.push_back(m.pos);
    }
  };
  w.spawns = [zone](const std::vector<std::string>& ids, std::vector<Vec2>& out) {
    if (zone == nullptr || !zone->HasZone()) return;
    for (const MapSpawnDef& s : zone->Map().spawns) {
      if (std::find(ids.begin(), ids.end(), s.monsterId) != ids.end()) out.push_back(s.pos.Center());
    }
  };
  w.gatherSpots = [this](std::string_view questId, int32_t objectiveIndex, std::vector<Vec2>& out) {
    for (const QuestNode& n : nodes_) {
      if (!n.clue && n.questId == questId && n.objectiveIndex == objectiveIndex) out.push_back(n.pos);
    }
  };
  w.giverOf = [data](std::string_view questId, std::string& out) {
    const NpcDef* giver = data->Npcs().GiverOf(questId);
    if (giver == nullptr) return false;
    out = giver->id;
    return true;
  };
  w.escortTile = [this](Vec2& out) {
    if (!escort_.active) return false;
    out = escort_.pos;
    return true;
  };
  w.clueTile = [this](std::string_view questId, int32_t objectiveIndex, Vec2& out) {
    return ClueTile(questId, objectiveIndex, out);
  };
  w.huntTile = [data](std::string_view huntId, Vec2& out) {
    const HuntDef* h = data->Monsters().FindHunt(huntId);
    if (h == nullptr) return false;
    out = h->spawn.Center();
    return true;
  };
  return w;
}

// updateGuide: every 250 ms (accumulator), forced on the next step after a quest change (accept, completion, turn-in,
// failure, tracking): guided = getGuidedQuest(zone), target = computeGuideTarget. The arrow hides within guideNear of
// the target (UE); the minimap star uses the same target.
void QuestWorld::TickGuide(double dtMs) {
  QuestSystem* qs = ctx_.sys.quests;
  if (qs == nullptr || ctx_.sys.zone == nullptr || !ctx_.sys.zone->HasZone()) return;
  if (qs->Tracked() != guideTracked_) {
    guideTracked_ = qs->Tracked();
    guideAccMs_ = 0;
  }
  guideAccMs_ -= dtMs;
  if (guideAccMs_ > 0) return;
  guideAccMs_ = kQwGuideRefreshMs;
  guide_ = GuideTarget{};
  const QuestDef* guided = qs->GuidedQuest(ctx_.sys.zone->MapId());
  if (guided == nullptr) return;
  const QuestProgress* p = qs->Progress(guided->id);
  if (p == nullptr) return;
  guide_ = ComputeGuideTarget(*guided, *p, MakeGuideWorld());
}

// ---------------------------------------------------------------------------------------------------------------------
// Step
// ---------------------------------------------------------------------------------------------------------------------

void QuestWorld::Tick(double dtMs) {
  if (ctx_.sys.zone == nullptr || !ctx_.sys.zone->HasZone() || ctx_.sys.quests == nullptr) return;
  TickEscort(dtMs);
  TickDefend();
  TickGatherAndClues();
  TickGuide(dtMs);
  const double now = ctx_.Now();
  if (now >= nextObserverMs_) {
    nextObserverMs_ = now + kQwObserverMs;
    CheckExplore();
  }
}

void QuestWorld::OnTimer(const Timer& t) { (void)t; }

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
    v.key = n.clue ? n.questId : n.itemKind;
    out.markers.push_back(std::move(v));
  }
  if (escort_.active) {
    WorldMarkerView v;
    v.id = escort_.entity;
    v.kind = MarkerKind::EscortNpc;
    v.pos = escort_.pos;
    v.key = escort_.questId;
    v.hp = escort_.hp;
    v.maxHp = escort_.maxHp;
    out.markers.push_back(std::move(v));
  }
  if (defend_.active) {
    WorldMarkerView v;
    v.id = defend_.entity;
    v.kind = MarkerKind::DefendTarget;
    v.pos = defend_.pos;
    v.key = defend_.questId;
    v.hp = defend_.hp;
    v.maxHp = defend_.maxHp;
    out.markers.push_back(std::move(v));
  }
  // NPC overhead markers (5.6), alert range and talking state (6.4).
  const QuestSystem* qs = ctx_.sys.quests;
  const Hero* hero = ctx_.sys.hero;
  const int32_t level = hero != nullptr ? hero->Level() : 1;
  const Vec2 hp = hero != nullptr ? hero->Position() : Vec2();
  std::string talking;
  if (card_.open) talking = card_.npcId;
  if (talking.empty() && ctx_.sys.dialogue != nullptr && ctx_.sys.dialogue->View().open) {
    talking = ctx_.sys.dialogue->View().npcId;
  }
  if (talking.empty() && ctx_.sys.shop != nullptr && ctx_.sys.shop->State().open) talking = ctx_.sys.shop->State().npcId;
  if (talking.empty() && ctx_.sys.inventory != nullptr && ctx_.sys.inventory->Stash().open) {
    talking = ctx_.sys.inventory->Stash().npcId;
  }
  for (NpcView& v : out.npcs) {
    if (qs != nullptr) v.marker = qs->Marker(v.npcId, level);
    v.heroNear = hero != nullptr && JsHypot(v.pos.x - hp.x, v.pos.y - hp.y) <= kQwNpcAlertRange;
    v.talking = !talking.empty() && v.npcId == talking;
  }
}

}  // namespace abyss
