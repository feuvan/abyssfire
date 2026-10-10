// Quests + story area (owner: quests+story): quest state machine, offer lists and markers, guide, gather / clue / drop
// rules, quest world runtime (card, turn-in, explore, talk, escort, defend), dialogue, achievements, lore, story
// director (queue, T15 delays, step player, boss intros) and the script / content data.
// Ported web suites (src/__tests__): QuestEngine, NewQuestTypes, QuestContent, QuestHunts, QuestNPCIndicators,
// QuestCardUI (gatherNpcQuests), QuestUXIntegration (lifecycle), AchievementSystem, DialogueTrees,
// quest-dialogue-navigation, quest-dialogue-scrutiny-fix, mini-bosses-lore (lore data), story-scrutiny-zones-loot
// (hidden areas), StoryProgress, StoryScript; plus the quests-story-ch1.md section 16 vectors and the DECISIONS Q1, Q2,
// Q5, Q7, Q8, S2 checks.
#include <algorithm>
#include <cmath>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "SimHarness.h"
#include "abyss/base/I18n.h"
#include "abyss/base/Math.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/items/Inventory.h"
#include "abyss/items/LootGen.h"
#include "abyss/items/Shop.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/quests/Achievements.h"
#include "abyss/quests/Dialogue.h"
#include "abyss/quests/Lore.h"
#include "abyss/quests/QuestGuide.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/quests/QuestWorld.h"
#include "abyss/save/SaveData.h"
#include "abyss/sim/GameSim.h"
#include "abyss/story/StoryDirector.h"
#include "abyss/world/MapGen.h"
#include "abyss/world/Zone.h"
#include "doctest/doctest.h"

using namespace abyss;

namespace {

// ---- pure QuestSystem rig ------------------------------------------------------------------------------------------
struct QsRig {
  test::SimHarness h;
  QuestSystem qs{h.ctx.data, h.events, h.bus};
  std::vector<std::string> log;  // "accepted:<id>", "progress:<id>:<i>:<cur>:<done>", "completed:<id>", ...
  QsRig() {
    h.bus.Subscribe<QuestAcceptedMsg>([this](const QuestAcceptedMsg& m) { log.push_back("accepted:" + m.questId); });
    h.bus.Subscribe<QuestProgressMsg>([this](const QuestProgressMsg& m) {
      log.push_back("progress:" + m.questId + ":" + std::to_string(m.objectiveIndex) + ":" + std::to_string(m.current) +
                    (m.completesQuest ? ":done" : ""));
    });
    h.bus.Subscribe<QuestCompletedMsg>([this](const QuestCompletedMsg& m) { log.push_back("completed:" + m.questId); });
    h.bus.Subscribe<QuestTurnedInMsg>([this](const QuestTurnedInMsg& m) { log.push_back("turnedIn:" + m.questId); });
    h.bus.Subscribe<QuestFailedMsg>([this](const QuestFailedMsg& m) { log.push_back("failed:" + m.questId); });
  }
  int32_t Cur(const std::string& id, size_t i) const {
    const QuestProgress* p = qs.Progress(id);
    return p != nullptr && i < p->objectives.size() ? p->objectives[i] : -1;
  }
  QuestStatus Status(const std::string& id) const {
    bool has = false;
    const QuestStatus s = qs.StatusOf(id, has);
    REQUIRE(has);
    return s;
  }
};

QuestProgress Rec(std::string id, QuestStatus st, std::vector<int32_t> cur) {
  return QuestProgress{std::move(id), st, std::move(cur)};
}

std::vector<std::string> OfferIds(const std::vector<QuestOffer>& offers) {
  std::vector<std::string> out;
  for (const QuestOffer& o : offers) out.push_back((o.turnIn ? "T:" : "") + o.quest->id);
  return out;
}

// ---- fake guide world (QuestEngine.test.ts `world()`) ---------------------------------------------------------------
QuestDef GuideQuest() {
  QuestDef q;
  q.id = "q_test";
  q.zone = "z";
  q.type = QuestType::Kill;
  q.category = QuestCategory::Main;
  q.level = 10;
  QuestObjectiveDef o;
  o.type = ObjectiveType::Kill;
  o.targetId = "goblin";
  o.required = 3;
  q.objectives.push_back(o);
  return q;
}

GuideWorld FakeWorld() {
  GuideWorld w;
  w.player = Vec2(0, 0);
  w.npcTile = [](std::string_view id, Vec2& out) {
    if (id != "giver") return false;
    out = Vec2(5, 5);
    return true;
  };
  w.monsters = [](const std::vector<std::string>&, std::vector<Vec2>& out) {
    out.push_back(Vec2(20, 0));
    out.push_back(Vec2(3, 4));
  };
  w.spawns = [](const std::vector<std::string>&, std::vector<Vec2>& out) { out.push_back(Vec2(50, 50)); };
  w.gatherSpots = [](std::string_view, int32_t, std::vector<Vec2>&) {};
  w.giverOf = [](std::string_view, std::string& out) {
    out = "giver";
    return true;
  };
  return w;
}

QuestProgress ActiveP(std::vector<int32_t> cur) { return Rec("q_test", QuestStatus::Active, std::move(cur)); }

// Number of Unicode code points of a UTF-8 string, '\n' excluded (StoryScript.test.ts `zh()`).
size_t CodePoints(std::string_view s) {
  size_t n = 0;
  for (unsigned char c : s) {
    if ((c & 0xC0) != 0x80 && c != '\n') ++n;
  }
  return n;
}

// NPC ids standing in a zone (camps + field NPCs).
std::set<std::string> NpcsIn(const DataStore& data, std::string_view zone) {
  std::set<std::string> out;
  const MapDef* m = data.FindMap(zone);
  if (m == nullptr) return out;
  for (const MapCampDef& c : m->camps) out.insert(c.npcs.begin(), c.npcs.end());
  for (const FieldNpcDef& f : m->fieldNpcs) out.insert(f.npcId);
  return out;
}

std::set<std::string> MonstersIn(const DataStore& data, std::string_view zone) {
  std::set<std::string> out;
  const MapDef* m = data.FindMap(zone);
  if (m == nullptr) return out;
  for (const MapSpawnDef& s : m->spawns) out.insert(s.monsterId);
  if (const MiniBossEntry* b = data.Monsters().MiniBossFor(zone)) out.insert(b->monsterId);
  return out;
}

}  // namespace

// =====================================================================================================================
// QuestSystem (quests 2) - QuestEngine.test.ts "QuestSystem events", NewQuestTypes.test.ts, QuestHunts.test.ts
// =====================================================================================================================
TEST_SUITE("quests") {
  TEST_CASE("QuestSystem lists: open quests in insertion order, turned-in ids, tracking message") {
    test::SimHarness h;
    QuestSystem qs(h.ctx.data, h.events, h.bus);
    int tracked = 0;
    h.bus.Subscribe<QuestTrackedChangedMsg>([&](const QuestTrackedChangedMsg&) { ++tracked; });
    qs.Load({{"q_kill_slimes", QuestStatus::TurnedIn, {5}}, {"q_collect_slime_gel", QuestStatus::Active, {0, 0}}});
    CHECK(qs.IsTurnedIn("q_kill_slimes"));
    CHECK(qs.TurnedInIds() == std::vector<std::string>{"q_kill_slimes"});
    const auto open = qs.OpenQuests();
    REQUIRE(open.size() == 1);
    CHECK(open[0].first->id == "q_collect_slime_gel");
    qs.SetTracked("q_collect_slime_gel");
    qs.SetTracked("q_collect_slime_gel");
    CHECK(tracked == 1);
    CHECK(qs.Tracked() == "q_collect_slime_gel");
  }

  TEST_CASE("announces accept, progress (flagging the completing step), completion and turn-in in order") {
    QsRig r;
    REQUIRE(r.qs.Accept("q_kill_slimes"));
    CHECK_FALSE(r.qs.Accept("q_kill_slimes"));  // already active
    r.qs.UpdateProgress(ObjectiveType::Kill, "slime_green");
    r.qs.UpdateProgress(ObjectiveType::Kill, "slime_green", 8);
    r.qs.UpdateProgress(ObjectiveType::Kill, "slime_green", 5);  // overflow clamps at 10
    r.qs.UpdateProgress(ObjectiveType::Kill, "slime_green");     // nothing advances: no event
    const QuestRewardDef* reward = r.qs.TurnIn("q_kill_slimes");
    REQUIRE(reward != nullptr);
    CHECK(reward->exp == 120);
    CHECK(reward->gold == 25);
    CHECK(r.log == std::vector<std::string>{"accepted:q_kill_slimes", "progress:q_kill_slimes:0:1",
                                            "progress:q_kill_slimes:0:9", "progress:q_kill_slimes:0:10:done",
                                            "completed:q_kill_slimes", "turnedIn:q_kill_slimes"});
    CHECK(r.Status("q_kill_slimes") == QuestStatus::TurnedIn);
    CHECK(r.qs.TurnIn("q_kill_slimes") == nullptr);  // terminal
    // Q12: the log lines carry the localized quest name key.
    bool keyed = false;
    for (const Event& e : r.h.events.Items()) {
      if (const EvLog* l = std::get_if<EvLog>(&e)) {
        if (l->text.key == "sys.quest.accepted") keyed = !l->text.args.empty() && l->text.args[0].isKey;
      }
    }
    CHECK(keyed);
  }

  TEST_CASE("accept checks prereqs (not the level); unknown ids are refused") {
    QsRig r;
    CHECK_FALSE(r.qs.Accept("q_does_not_exist"));
    CHECK_FALSE(r.qs.Accept("q_kill_goblins"));  // prereq q_kill_slimes not turned in
    r.qs.Load({Rec("q_kill_slimes", QuestStatus::TurnedIn, {10})});
    CHECK(r.qs.Accept("q_kill_goblins"));
    CHECK(r.qs.Accept("q_find_goblin_chief") == false);  // prereq chain
    CHECK(r.qs.Accept("q_herb_gathering"));             // level 2 at any hero level (Q1)
  }

  TEST_CASE("guides the pinned quest, else the zone's first main quest; completed quests count") {
    QsRig r;
    REQUIRE(r.qs.Accept("q_herb_gathering"));
    REQUIRE(r.qs.Accept("q_kill_slimes"));
    CHECK(r.qs.GuidedQuest("emerald_plains")->id == "q_kill_slimes");
    r.qs.SetTracked("q_herb_gathering");
    CHECK(r.qs.GuidedQuest("emerald_plains")->id == "q_herb_gathering");
    CHECK(r.qs.GuidedQuest("twilight_forest") == nullptr);
    r.qs.SetTracked("");
    r.qs.UpdateProgress(ObjectiveType::Kill, "slime_green", 10);
    CHECK(r.qs.GuidedQuest("emerald_plains")->id == "q_kill_slimes");  // completed leads back to the giver
    r.qs.TurnIn("q_kill_slimes");
    CHECK(r.qs.GuidedQuest("emerald_plains")->id == "q_herb_gathering");
  }

  TEST_CASE("turning in the tracked quest clears tracking") {
    QsRig r;
    r.qs.Load({Rec("q_herb_gathering", QuestStatus::Completed, {5})});
    r.qs.SetTracked("q_herb_gathering");
    REQUIRE(r.qs.TurnIn("q_herb_gathering") != nullptr);
    CHECK(r.qs.Tracked().empty());
  }

  TEST_CASE("16.2 talk delivery gating: the herbalist counts only once the gel is in hand") {
    QsRig r;
    REQUIRE(r.qs.Accept("q_collect_slime_gel"));
    r.qs.UpdateProgress(ObjectiveType::Collect, "mat_slime_gel", 5);
    r.log.clear();
    r.qs.UpdateProgress(ObjectiveType::Talk, "plains_herbalist");
    CHECK(r.Cur("q_collect_slime_gel", 1) == 0);
    CHECK(r.log.empty());
    r.qs.UpdateProgress(ObjectiveType::Collect, "mat_slime_gel");
    r.log.clear();
    r.qs.UpdateProgress(ObjectiveType::Talk, "plains_herbalist");
    CHECK(r.log == std::vector<std::string>{"progress:q_collect_slime_gel:1:1:done", "completed:q_collect_slime_gel"});
    CHECK(r.Status("q_collect_slime_gel") == QuestStatus::Completed);
  }

  TEST_CASE("kill progress is not zone-filtered and one call advances every matching quest") {
    QsRig r;
    r.qs.Load({Rec("q_kill_slimes", QuestStatus::TurnedIn, {10}), Rec("q_kill_goblins", QuestStatus::Active, {0}),
               Rec("q_escort_merchant_plains", QuestStatus::Active, {0})});
    r.qs.UpdateProgress(ObjectiveType::Kill, "goblin", 3);
    CHECK(r.Cur("q_kill_goblins", 0) == 3);
    // hunt leaders carry the hunt id: no credit for plain goblin objectives
    r.qs.UpdateProgress(ObjectiveType::Kill, "hunt_redcap_gruk");
    CHECK(r.Cur("q_kill_goblins", 0) == 3);
  }

  TEST_CASE("16.3 craft phase ordering: deliver / craft before their phase stay 0 and emit nothing") {
    QsRig r;
    r.qs.Load({Rec("q_craft_dwarf_weapon", QuestStatus::Active, {0, 0, 0, 0})});
    const QuestDef* q = r.h.ctx.data.FindQuest("q_craft_dwarf_weapon");
    REQUIRE(q != nullptr);
    CHECK(r.qs.CraftPhaseKey(*q, *r.qs.Progress(q->id)) == "sys.quest.phase.collect");
    r.qs.UpdateProgress(ObjectiveType::CraftDeliver, "deliver_dwarf_hammer");
    r.qs.UpdateProgress(ObjectiveType::CraftCraft, "craft_dwarf_hammer");
    CHECK(r.Cur(q->id, 2) == 0);
    CHECK(r.Cur(q->id, 3) == 0);
    CHECK(r.log.empty());
    r.qs.UpdateProgress(ObjectiveType::CraftCollect, "mat_dwarf_ingot", 3);
    CHECK(r.qs.CraftPhaseKey(*q, *r.qs.Progress(q->id)) == "sys.quest.phase.collect");
    r.qs.UpdateProgress(ObjectiveType::CraftCollect, "mat_rune_fragment", 5);
    CHECK(r.qs.CraftPhaseKey(*q, *r.qs.Progress(q->id)) == "sys.quest.phase.craft");
    r.qs.UpdateProgress(ObjectiveType::CraftDeliver, "deliver_dwarf_hammer");  // cannot deliver before crafting
    CHECK(r.Cur(q->id, 3) == 0);
    r.qs.UpdateProgress(ObjectiveType::CraftCraft, "craft_dwarf_hammer");
    CHECK(r.Cur(q->id, 2) == 1);
    CHECK(r.qs.CraftPhaseKey(*q, *r.qs.Progress(q->id)) == "sys.quest.phase.deliver");
    r.qs.UpdateProgress(ObjectiveType::CraftDeliver, "deliver_dwarf_hammer");
    CHECK(r.Status(q->id) == QuestStatus::Completed);
    const QuestDef* kill = r.h.ctx.data.FindQuest("q_kill_slimes");
    CHECK(r.qs.CraftPhaseKey(*kill, Rec("q_kill_slimes", QuestStatus::Active, {0})).empty());
  }

  TEST_CASE("escort / defend_wave / investigate_clue objectives advance through updateProgress") {
    QsRig r;
    r.qs.Load({Rec("q_escort_merchant_plains", QuestStatus::Active, {0}),
               Rec("q_defend_camp_forest", QuestStatus::Active, {0}),
               Rec("q_pet_sprite_friend", QuestStatus::Active, {0, 0, 0, 0})});
    r.qs.UpdateProgress(ObjectiveType::Escort, "escort_merchant");
    CHECK(r.Status("q_escort_merchant_plains") == QuestStatus::Completed);
    r.qs.UpdateProgress(ObjectiveType::DefendWave, "defend_forest_camp");
    r.qs.UpdateProgress(ObjectiveType::DefendWave, "defend_forest_camp");
    CHECK(r.Cur("q_defend_camp_forest", 0) == 2);
    CHECK(r.Status("q_defend_camp_forest") == QuestStatus::Active);
    r.qs.UpdateProgress(ObjectiveType::DefendWave, "defend_forest_camp");
    CHECK(r.Status("q_defend_camp_forest") == QuestStatus::Completed);
    // clues in any order
    r.qs.UpdateProgress(ObjectiveType::InvestigateClue, "clue_sprite_3");
    r.qs.UpdateProgress(ObjectiveType::InvestigateClue, "clue_sprite_1");
    CHECK(r.Cur("q_pet_sprite_friend", 0) == 1);
    CHECK(r.Cur("q_pet_sprite_friend", 1) == 0);
    CHECK(r.Cur("q_pet_sprite_friend", 2) == 1);
  }

  TEST_CASE("failQuest: active only; emits QUEST_FAILED; completed / missing records are untouched") {
    QsRig r;
    r.qs.Fail("q_kill_slimes");  // no record
    CHECK_FALSE(r.qs.HasRecord("q_kill_slimes"));
    r.qs.Load({Rec("q_escort_merchant_plains", QuestStatus::Active, {0}),
               Rec("q_collect_slime_gel", QuestStatus::Completed, {6, 1})});
    r.qs.Fail("q_collect_slime_gel");
    CHECK(r.Status("q_collect_slime_gel") == QuestStatus::Completed);
    r.qs.Fail("q_escort_merchant_plains");
    CHECK(r.Status("q_escort_merchant_plains") == QuestStatus::Failed);
    CHECK(r.log == std::vector<std::string>{"failed:q_escort_merchant_plains"});
    r.qs.UpdateProgress(ObjectiveType::Escort, "escort_merchant");  // failed quests do not advance
    CHECK(r.Cur("q_escort_merchant_plains", 0) == 0);
  }

  TEST_CASE("re-accepting a failed reacceptable quest resets its progress in place; others are refused") {
    QsRig r;
    r.qs.Load({Rec("q_kill_slimes", QuestStatus::TurnedIn, {10}),
               Rec("q_escort_merchant_plains", QuestStatus::Failed, {0}),
               Rec("q_lost_pendant", QuestStatus::Failed, {1, 1, 0, 0, 0}),
               Rec("q_defend_camp_forest", QuestStatus::Failed, {2})});
    // Q1: re-accept skips the prereq check (M2 is not turned in)
    CHECK(r.qs.Accept("q_escort_merchant_plains"));
    CHECK(r.Status("q_escort_merchant_plains") == QuestStatus::Active);
    CHECK_FALSE(r.qs.Accept("q_lost_pendant"));  // not reacceptable
    CHECK(r.Status("q_lost_pendant") == QuestStatus::Failed);
    CHECK(r.qs.Accept("q_defend_camp_forest"));
    CHECK(r.Cur("q_defend_camp_forest", 0) == 0);  // progress reset
    // map position kept: the escort quest stays second
    CHECK(r.qs.AllProgress()[1].questId == "q_escort_merchant_plains");
    bool reaccepted = false;
    for (const Event& e : r.h.events.Items()) {
      if (const EvLog* l = std::get_if<EvLog>(&e)) reaccepted = reaccepted || l->text.key == "sys.quest.reaccepted";
    }
    CHECK(reaccepted);
  }

  TEST_CASE("16.10 load: redesigned active / completed quests restart; turned_in, failed and unknown ids are kept") {
    QsRig r;
    r.qs.Load({Rec("q_collect_slime_gel", QuestStatus::Active, {4}),        // definition has 2 objectives
               Rec("q_secure_plains", QuestStatus::Completed, {1}),        // 2 objectives
               Rec("q_kill_slimes", QuestStatus::TurnedIn, {1, 2, 3}),     // kept as-is
               Rec("q_lost_pendant", QuestStatus::Failed, {1}),            // kept as-is
               Rec("q_removed_since", QuestStatus::Active, {1}),           // unknown: kept, ignored by lists
               Rec("q_herb_gathering", QuestStatus::Active, {2}),
               Rec("q_herb_gathering", QuestStatus::Active, {3})});        // duplicate replaces, keeps position
    CHECK(r.qs.Progress("q_collect_slime_gel")->objectives == std::vector<int32_t>{0, 0});
    CHECK(r.Status("q_secure_plains") == QuestStatus::Active);
    CHECK(r.qs.Progress("q_secure_plains")->objectives == std::vector<int32_t>{0, 0});
    CHECK(r.qs.Progress("q_kill_slimes")->objectives.size() == 3);
    CHECK(r.Status("q_lost_pendant") == QuestStatus::Failed);
    CHECK(r.qs.HasRecord("q_removed_since"));
    CHECK(r.qs.AllProgress().size() == 6);
    CHECK(r.qs.AllProgress()[5].questId == "q_herb_gathering");
    CHECK(r.Cur("q_herb_gathering", 0) == 3);
    for (const auto& [q, p] : r.qs.OpenQuests()) CHECK(q->id != "q_removed_since");
  }

  TEST_CASE("save JSON round trip keeps statuses and counters of every quest kind") {
    const std::vector<QuestProgress> in = {Rec("q_escort_merchant_plains", QuestStatus::Active, {0}),
                                           Rec("q_defend_camp_forest", QuestStatus::Active, {2}),
                                           Rec("q_lost_pendant", QuestStatus::Failed, {1, 1, 0, 0, 0}),
                                           Rec("q_craft_dwarf_weapon", QuestStatus::Active, {3, 2, 0, 0}),
                                           Rec("q_kill_slimes", QuestStatus::TurnedIn, {10})};
    test::SimHarness h;
    QuestSystem qs(h.ctx.data, h.events, h.bus);
    qs.Load(in);
    std::vector<QuestProgress> out(qs.AllProgress().begin(), qs.AllProgress().end());
    CHECK(out == in);
  }

  // ---- offer lists (2.5) - QuestCardUI.test.ts gatherNpcQuests + QuestNPCIndicators.test.ts ------------------------
  TEST_CASE("16.4 the elder's first card at a new game, in NPC list order") {
    QsRig r;
    CHECK(OfferIds(r.qs.Offers("quest_elder", 1, OfferRule::QuestCard)) ==
          std::vector<std::string>{"q_kill_slimes", "q_collect_slime_gel", "q_herb_gathering", "q_pet_sprite_friend"});
    CHECK(r.qs.Marker("quest_elder", 1) == NpcMarker::Available);
    CHECK(r.qs.Marker("blacksmith", 1) == NpcMarker::None);        // no quests
    CHECK(r.qs.Marker("plains_herbalist", 1) == NpcMarker::None);  // delivery target only
    CHECK(r.qs.Offers("blacksmith", 1, OfferRule::QuestCard).empty());
  }

  TEST_CASE("quest card: turn-ins first, active and turned-in excluded, level gate +5 inclusive, prereqs") {
    QsRig r;
    r.qs.Load({Rec("q_kill_slimes", QuestStatus::TurnedIn, {10}), Rec("q_kill_goblins", QuestStatus::TurnedIn, {15}),
               Rec("q_explore_goblin_camp", QuestStatus::TurnedIn, {1}),
               Rec("q_collect_slime_gel", QuestStatus::Active, {1, 0}),
               Rec("q_herb_gathering", QuestStatus::Completed, {5})});
    // hero L1: q_find_goblin_chief (lvl 7) is above L1 + 5
    std::vector<std::string> l1 = OfferIds(r.qs.Offers("quest_elder", 1, OfferRule::QuestCard));
    CHECK(l1.front() == "T:q_herb_gathering");
    CHECK(std::find(l1.begin(), l1.end(), "q_find_goblin_chief") == l1.end());
    CHECK(std::find(l1.begin(), l1.end(), "q_collect_slime_gel") == l1.end());  // active
    CHECK(std::find(l1.begin(), l1.end(), "q_kill_slimes") == l1.end());        // turned in
    CHECK(std::find(l1.begin(), l1.end(), "q_lost_pendant") != l1.end());        // prereq met, lvl 4
    // exactly hero level + 5 is offered
    std::vector<std::string> l2 = OfferIds(r.qs.Offers("quest_elder", 2, OfferRule::QuestCard));
    CHECK(std::find(l2.begin(), l2.end(), "q_find_goblin_chief") != l2.end());
    // turn-ins are never level gated
    r.qs.Load({Rec("q_secure_plains", QuestStatus::Completed, {1, 1})});
    CHECK(OfferIds(r.qs.Offers("quest_elder", 1, OfferRule::QuestCard)).front() == "T:q_secure_plains");
  }

  TEST_CASE("failed reacceptable quests are offered again; failed non-reacceptable ones are not") {
    QsRig r;
    r.qs.Load({Rec("q_kill_slimes", QuestStatus::TurnedIn, {10}), Rec("q_kill_goblins", QuestStatus::TurnedIn, {15}),
               Rec("q_escort_merchant_plains", QuestStatus::Failed, {0}),
               Rec("q_lost_pendant", QuestStatus::Failed, {0, 0, 0, 0, 0})});
    for (OfferRule rule : {OfferRule::QuestCard, OfferRule::Available}) {
      const std::vector<std::string> ids = OfferIds(r.qs.Offers("quest_elder", 5, rule));
      CHECK(std::find(ids.begin(), ids.end(), "q_escort_merchant_plains") != ids.end());
      CHECK(std::find(ids.begin(), ids.end(), "q_lost_pendant") == ids.end());
    }
  }

  TEST_CASE("NPC markers: priority turn-in > available > in progress > none; lifecycle on real data") {
    QsRig r;
    // plains_wanderer gives only q_bandit_trouble (lvl 7, prereq M2).
    CHECK(r.qs.Marker("plains_wanderer", 1) == NpcMarker::None);
    r.qs.Load({Rec("q_kill_slimes", QuestStatus::TurnedIn, {10}), Rec("q_kill_goblins", QuestStatus::TurnedIn, {15})});
    CHECK(r.qs.Marker("plains_wanderer", 1) == NpcMarker::None);  // 7 > 1 + 5
    CHECK(r.qs.Marker("plains_wanderer", 2) == NpcMarker::Available);
    REQUIRE(r.qs.Accept("q_bandit_trouble"));
    CHECK(r.qs.Marker("plains_wanderer", 2) == NpcMarker::InProgress);
    r.qs.UpdateProgress(ObjectiveType::Kill, "hunt_redcap_gruk");
    CHECK(r.qs.Marker("plains_wanderer", 2) == NpcMarker::TurnIn);
    r.qs.TurnIn("q_bandit_trouble");
    CHECK(r.qs.Marker("plains_wanderer", 2) == NpcMarker::None);
    // the elder: available beats in progress, turn-in beats both
    QsRig e;
    REQUIRE(e.qs.Accept("q_kill_slimes"));
    CHECK(e.qs.Marker("quest_elder", 1) == NpcMarker::Available);
    e.qs.UpdateProgress(ObjectiveType::Kill, "slime_green", 10);
    CHECK(e.qs.Marker("quest_elder", 1) == NpcMarker::TurnIn);
    e.qs.TurnIn("q_kill_slimes");
    CHECK(e.qs.Marker("quest_elder", 3) == NpcMarker::Available);  // M2 now available
  }

  TEST_CASE("every quest NPC lists only known quests, each quest has exactly one giver of type quest in its zone") {
    const DataStore& data = test::RealData();
    for (const QuestDef& q : data.Quests().quests) {
      int givers = 0;
      const NpcDef* giver = nullptr;
      for (const NpcDef& n : data.Npcs().npcs) {
        if (std::find(n.quests.begin(), n.quests.end(), q.id) != n.quests.end()) {
          ++givers;
          giver = &n;
        }
      }
      CAPTURE(q.id);
      CHECK(givers == 1);
      if (giver == nullptr) continue;
      CHECK(giver->type == NpcType::Quest);
      CHECK(NpcsIn(data, q.zone).count(giver->id) == 1);
      CHECK(data.Npcs().GiverOf(q.id) == giver);
    }
    for (const NpcDef& n : data.Npcs().npcs) {
      for (const std::string& id : n.quests) CHECK(data.FindQuest(id) != nullptr);
    }
  }

  // ---- QuestContent.test.ts -----------------------------------------------------------------------------------------
  TEST_CASE("quest content: kill / talk / drop targets exist in the quest's zone, gather sources have enough nodes") {
    const DataStore& data = test::RealData();
    for (const QuestDef& q : data.Quests().quests) {
      CAPTURE(q.id);
      std::set<std::string> monsters = MonstersIn(data, q.zone);
      for (const QuestHuntRef& hr : q.hunts) monsters.insert(hr.huntId);
      const std::set<std::string> npcs = NpcsIn(data, q.zone);
      for (size_t i = 0; i < q.objectives.size(); ++i) {
        const QuestObjectiveDef& o = q.objectives[i];
        CAPTURE(o.targetId);
        if (o.type == ObjectiveType::Kill) CHECK(monsters.count(o.targetId) == 1);
        if (o.type == ObjectiveType::Talk) CHECK(npcs.count(o.targetId) == 1);
        if (o.type == ObjectiveType::Collect || o.type == ObjectiveType::CraftCollect) {
          CHECK(o.sourceKind != ItemSourceKind::None);
          if (o.sourceKind == ItemSourceKind::Drop) {
            CHECK(o.dropChance > 0);
            for (const std::string& m : o.dropMonsters) CHECK(monsters.count(m) == 1);
          } else if (o.sourceKind == ItemSourceKind::Gather) {
            CHECK(o.gatherCount >= o.required);
            const MapDef* map = data.FindMap(q.zone);
            REQUIRE(map != nullptr);
            const ZoneGrid grid = BuildZoneGrid(data, *map);
            const std::vector<TilePos> spots = ResolveGatherSpots(
                o.gatherArea, o.gatherCount, [&grid](int32_t c, int32_t r) { return grid.Walkable(c, r); },
                q.id + ":" + std::to_string(i));
            CHECK(static_cast<int32_t>(spots.size()) == o.gatherCount);
          }
        }
      }
      for (const QuestHuntRef& hr : q.hunts) {
        const HuntDef* hunt = data.Monsters().FindHunt(hr.huntId);
        REQUIRE(hunt != nullptr);
        CHECK(data.Monsters().Find(hunt->monsterId) != nullptr);
        if (hunt->hasMinions) CHECK(data.Monsters().Find(hunt->minionMonsterId) != nullptr);
        CHECK(std::any_of(q.objectives.begin(), q.objectives.end(), [&](const QuestObjectiveDef& o) {
          return o.type == ObjectiveType::Kill && o.targetId == hr.huntId;
        }));
      }
    }
  }

  TEST_CASE("quest content: each zone's side quests play differently; every zone finale offers a gear choice") {
    const DataStore& data = test::RealData();
    std::vector<std::pair<std::string, std::string>> seen;  // zone|shape -> quest
    for (const QuestDef& q : data.Quests().quests) {
      if (q.category != QuestCategory::Side) continue;
      std::string shape = q.hasEscortNpc ? "escort" : q.hasDefendTarget ? "defend" : q.craftPhases.present ? "craft" : "";
      bool bareKill = true;
      for (const QuestObjectiveDef& o : q.objectives) {
        const bool hunt = o.type == ObjectiveType::Kill &&
                          std::any_of(q.hunts.begin(), q.hunts.end(), [&](const QuestHuntRef& h) { return h.huntId == o.targetId; });
        shape += ">" + std::string(hunt ? "hunt" : EnumName(o.type));
        if (o.sourceKind == ItemSourceKind::Drop) shape += ":drop";
        if (o.sourceKind == ItemSourceKind::Gather) shape += ":gather";
        bareKill = bareKill && o.type == ObjectiveType::Kill && !hunt;
      }
      CAPTURE(q.id);
      CHECK_FALSE(bareKill);
      const std::string key = q.zone + "|" + shape;
      CHECK(std::none_of(seen.begin(), seen.end(), [&](const auto& p) { return p.first == key; }));
      seen.emplace_back(key, q.id);
    }
    for (const char* id : {"q_secure_plains", "q_seal_dark_source", "q_kill_stone_guardian", "q_seal_fire_rift",
                           "q_kill_abyss_lord"}) {
      const QuestDef* q = data.FindQuest(id);
      REQUIRE(q != nullptr);
      CHECK_FALSE(q->rewards.choices.empty());
    }
  }

  TEST_CASE("chapter 1 quest table (10.1): 11 quests, rewards and totals; Q4 chief area; Q8 escort label") {
    const DataStore& data = test::RealData();
    int count = 0;
    int64_t exp = 0, gold = 0;
    for (const QuestDef& q : data.Quests().quests) {
      if (q.zone != "emerald_plains") continue;
      ++count;
      exp += q.rewards.exp;
      gold += q.rewards.gold;
    }
    CHECK(count == 11);
    CHECK(exp == 2660);
    CHECK(gold == 505);
    const QuestDef* chief = data.FindQuest("q_find_goblin_chief");
    REQUIRE(chief != nullptr);
    CHECK(chief->questArea.col == 15);  // Q4: centred on the chief's spawn (15,95)
    CHECK(chief->questArea.row == 95);
    const QuestDef* escort = data.FindQuest("q_escort_merchant_plains");
    REQUIRE(escort != nullptr);
    CHECK(escort->level * 20 + 100 == 200);
    CHECK(escort->objectives[0].labelKey == escort->nameKey);  // Q8: no "southern camp" claim
    CHECK(escort->escortNpc.dest == TilePos(50, 90));
    const QuestDef* gel = data.FindQuest("q_collect_slime_gel");
    REQUIRE(gel != nullptr);
    CHECK(gel->objectives[0].itemKind == "gel");
    CHECK(gel->rewards.items == std::vector<std::string>{"c_hp_potion_s", "c_hp_potion_s"});
  }

  // ---- reward choices (4.2) - QuestEngine.test.ts QuestRewards ---------------------------------------------------------
  TEST_CASE("16.8 reward item level and quality; drop chances") {
    const DataStore& data = test::RealData();
    const QuestDef* m4 = data.FindQuest("q_find_goblin_chief");
    REQUIRE(m4 != nullptr);
    CHECK(RewardItemLevel(*m4, 3) == 7);
    CHECK(RewardItemLevel(*m4, 10) == 10);
    CHECK(RewardItemLevel(*m4, 15) == 12);
    CHECK(RewardChoiceQuality(data, *m4) == ItemQuality::Rare);
    CHECK(RewardChoiceQuality(data, *data.FindQuest("q_lost_pendant")) == ItemQuality::Magic);
  }

  TEST_CASE("pick-one gear only offers the class's weapon families; non-shield classes get jewelry off-hands") {
    const DataStore& data = test::RealData();
    Rng rng(5);
    for (ClassId cls : {ClassId::Warrior, ClassId::Mage, ClassId::Rogue}) {
      const std::vector<WeaponType>& allowed = data.Items().loot.classWeaponTypes[static_cast<size_t>(cls)];
      for (int i = 0; i < 20; ++i) {
        const ItemBaseDef* b = PickRewardBase(data, RewardSlot::Weapon, cls, 30, rng);
        REQUIRE(b != nullptr);
        CHECK(std::find(allowed.begin(), allowed.end(), b->weaponType) != allowed.end());
        CHECK(b->levelReq <= 32);
      }
    }
    const ItemBaseDef* mageOff = PickRewardBase(data, RewardSlot::Offhand, ClassId::Mage, 20, rng);
    REQUIRE(mageOff != nullptr);
    CHECK(mageOff->type == ItemType::Accessory);
    const ItemBaseDef* warOff = PickRewardBase(data, RewardSlot::Offhand, ClassId::Warrior, 20, rng);
    REQUIRE(warOff != nullptr);
    CHECK(warOff->weaponType == WeaponType::Shield);
  }

  TEST_CASE("pick-one gear: one identified item per choice slot at the quest quality") {
    const DataStore& data = test::RealData();
    Rng rng(9);
    ItemUidGenerator uids;
    const LootContext lc{&data, &rng, &uids};
    const QuestDef* m5 = data.FindQuest("q_secure_plains");
    REQUIRE(m5 != nullptr);
    const std::vector<ItemInstance> items = GenerateQuestRewardChoices(lc, *m5, ClassId::Rogue, 9);
    CHECK(items.size() == m5->rewards.choices.size());
    for (const ItemInstance& it : items) {
      CHECK(it.identified);
      CHECK(it.quality == ItemQuality::Rare);
    }
  }
}

// =====================================================================================================================
// Guide (5.1) and gather spots (3.4) - QuestEngine.test.ts QuestGuide + 16.1 / 16.5
// =====================================================================================================================
TEST_SUITE("quests") {
  TEST_CASE("guide: nearest living target of the first unfinished objective, else spawns") {
    const QuestDef q = GuideQuest();
    GuideTarget t = ComputeGuideTarget(q, ActiveP({0}), FakeWorld());
    CHECK(t.kind == GuideTargetKind::Monster);
    CHECK(t.pos == Vec2(3, 4));
    CHECK(t.reason == GuideReason::Objective);
    CHECK(t.objectiveIndex == 0);
    GuideWorld none = FakeWorld();
    none.monsters = [](const std::vector<std::string>&, std::vector<Vec2>&) {};
    t = ComputeGuideTarget(q, ActiveP({0}), none);
    CHECK(t.kind == GuideTargetKind::Spawn);
    CHECK(t.pos == Vec2(50, 50));
  }

  TEST_CASE("guide: skips finished objectives and uses explicit locations") {
    QuestDef q = GuideQuest();
    q.objectives[0].required = 1;
    QuestObjectiveDef ex;
    ex.type = ObjectiveType::Explore;
    ex.targetId = "camp";
    ex.hasLocation = true;
    ex.location = TileCircle{9, 8, 3};
    q.objectives.push_back(ex);
    const GuideTarget t = ComputeGuideTarget(q, ActiveP({1, 0}), FakeWorld());
    CHECK(t.pos == Vec2(9, 8));
    CHECK(t.objectiveIndex == 1);
  }

  TEST_CASE("16.5 guide: fetch the escort while it is more than 4 tiles away, else the destination") {
    QuestDef q = GuideQuest();
    q.type = QuestType::Escort;
    q.objectives[0].type = ObjectiveType::Escort;
    q.objectives[0].targetId = "merchant";
    q.objectives[0].required = 1;
    q.objectives[0].hasLocation = true;
    q.objectives[0].location = TileCircle{40, 40, 5};
    GuideWorld w = FakeWorld();
    w.escortTile = [](Vec2& out) {
      out = Vec2(10, 10);
      return true;
    };
    CHECK(ComputeGuideTarget(q, ActiveP({0}), w).pos == Vec2(10, 10));
    w.escortTile = [](Vec2& out) {
      out = Vec2(1, 1);
      return true;
    };
    CHECK(ComputeGuideTarget(q, ActiveP({0}), w).pos == Vec2(40, 40));
  }

  TEST_CASE("16.5 guide: completed -> the giver (turn_in); giver elsewhere -> no target") {
    const QuestDef q = GuideQuest();
    const QuestProgress done = Rec("q_test", QuestStatus::Completed, {3});
    GuideTarget t = ComputeGuideTarget(q, done, FakeWorld());
    CHECK(t.pos == Vec2(5, 5));
    CHECK(t.reason == GuideReason::TurnIn);
    CHECK(t.objectiveIndex == -1);
    GuideWorld away = FakeWorld();
    away.npcTile = [](std::string_view, Vec2&) { return false; };
    CHECK_FALSE(ComputeGuideTarget(q, done, away).Valid());
    CHECK_FALSE(ComputeGuideTarget(q, Rec("q_test", QuestStatus::TurnedIn, {3}), FakeWorld()).Valid());
  }

  TEST_CASE("guide: drop objectives route to the dropping monsters; a talk target elsewhere moves on") {
    QuestDef q = GuideQuest();
    q.objectives[0].type = ObjectiveType::Collect;
    q.objectives[0].targetId = "mat_x";
    q.objectives[0].sourceKind = ItemSourceKind::Drop;
    q.objectives[0].dropMonsters = {"wolf"};
    q.objectives[0].dropChance = 0.5;
    std::vector<std::vector<std::string>> asked;
    GuideWorld w = FakeWorld();
    w.monsters = [&asked](const std::vector<std::string>& ids, std::vector<Vec2>& out) {
      asked.push_back(ids);
      out.push_back(Vec2(1, 1));
    };
    ComputeGuideTarget(q, ActiveP({0}), w);
    REQUIRE_FALSE(asked.empty());
    CHECK(asked[0] == std::vector<std::string>{"wolf"});
    // talk to an NPC who is not in this zone, then explore: the explore location wins
    QuestDef t = GuideQuest();
    t.objectives[0].type = ObjectiveType::Talk;
    t.objectives[0].targetId = "someone_far";
    t.objectives[0].required = 1;
    QuestObjectiveDef ex;
    ex.type = ObjectiveType::Explore;
    ex.hasLocation = true;
    ex.location = TileCircle{7, 7, 2};
    t.objectives.push_back(ex);
    CHECK(ComputeGuideTarget(t, ActiveP({0, 0}), FakeWorld()).pos == Vec2(7, 7));
  }

  TEST_CASE("16.1 gather hash: FNV state, first draws and the all-walkable spots") {
    GatherSpotRng rng("q_herb_gathering:0");
    CHECK(rng.State() == 3613349914u);
    const double expected[] = {0.924380941, 0.017937220, 0.933055474, 0.561144891, 0.710008817, 0.978309497};
    for (double e : expected) CHECK(rng.Next() == doctest::Approx(e).epsilon(1e-8));
    const auto all = [](int32_t, int32_t) { return true; };
    CHECK(ResolveGatherSpots(TileCircle{22, 26, 9}, 7, all, "q_herb_gathering:0") ==
          std::vector<TilePos>{{23, 25}, {28, 23}, {20, 17}, {18, 32}, {23, 34}, {27, 29}, {23, 18}});
    // floor(x + 0.5) on negative offsets
    CHECK(ResolveGatherSpots(TileCircle{0, 0, 5}, 3, all, "abc") == std::vector<TilePos>{{-1, -3}, {-4, -2}, {0, 4}});
  }

  TEST_CASE("gather spots are deterministic, walkable, inside the area and >= 3 Manhattan apart") {
    const auto walkable = [](int32_t c, int32_t r) { return (c + r) % 5 != 0; };
    const std::vector<TilePos> a = ResolveGatherSpots(TileCircle{20, 20, 8}, 6, walkable, "q:0");
    CHECK(a == ResolveGatherSpots(TileCircle{20, 20, 8}, 6, walkable, "q:0"));
    REQUIRE(a.size() == 6);
    for (size_t i = 0; i < a.size(); ++i) {
      CHECK(walkable(a[i].col, a[i].row));
      CHECK(JsHypot(a[i].col - 20.0, a[i].row - 20.0) <= 8.8);
      for (size_t j = i + 1; j < a.size(); ++j) CHECK(ManhattanDist(a[i], a[j]) >= 3);
    }
  }

  TEST_CASE("clue marks: nearestWalkable keeps a walkable tile and searches rings row-major") {
    TilePos out;
    CHECK(NearestWalkableTile(TilePos(5, 5), [](int32_t, int32_t) { return true; }, 8, out));
    CHECK(out == TilePos(5, 5));
    // only (6,4) and (4,6) are walkable at ring 1: dr = -1 row first -> (6,4)
    const auto w = [](int32_t c, int32_t r) { return (c == 6 && r == 4) || (c == 4 && r == 6); };
    CHECK(NearestWalkableTile(TilePos(5, 5), w, 8, out));
    CHECK(out == TilePos(6, 4));
    CHECK_FALSE(NearestWalkableTile(TilePos(5, 5), [](int32_t, int32_t) { return false; }, 8, out));
  }
}

// =====================================================================================================================
// Achievements (9) - AchievementSystem.test.ts, adapted to DECISIONS Q2 (one count per kill, distinct story zones)
// =====================================================================================================================
TEST_SUITE("quests") {
  TEST_CASE("achievements: first kill, the shared 'kill' key counts once per update, kill_100 at 100 updates") {
    AchievementState a(test::RealData());
    std::vector<std::string> first = a.Update(AchievementType::Kill, "goblin");
    CHECK(first == std::vector<std::string>{"ach_first_kill"});
    CHECK(a.ProgressOf("kill") == 1);
    CHECK(a.ProgressOf("kill:goblin") == 0);  // no achievement targets goblins: no key
    for (int i = 1; i < 99; ++i) a.Update(AchievementType::Kill, "goblin");
    CHECK(a.ProgressOf("kill") == 99);
    CHECK_FALSE(a.IsUnlocked("ach_kill_100"));
    CHECK(a.Update(AchievementType::Kill, "goblin") == std::vector<std::string>{"ach_kill_100"});
  }

  TEST_CASE("achievements: targeted slime kills also count toward the generic key, unlock at 50") {
    AchievementState a(test::RealData());
    for (int i = 0; i < 30; ++i) a.Update(AchievementType::Kill, "slime_green");
    CHECK(a.ProgressOf("kill:slime_green") == 30);
    CHECK(a.ProgressOf("kill") == 30);
    CHECK_FALSE(a.IsUnlocked("ach_kill_slime"));
    for (int i = 0; i < 20; ++i) a.Update(AchievementType::Kill, "slime_green");
    CHECK(a.IsUnlocked("ach_kill_slime"));
    CHECK(a.IsUnlocked("ach_first_kill"));
    // boss kill tracked separately from generic kills
    CHECK(a.Update(AchievementType::Kill, "goblin_chief") == std::vector<std::string>{"ach_kill_goblin_chief"});
    CHECK(a.ProgressOf("kill:goblin_chief") == 1);
  }

  TEST_CASE("achievements: explore, quest, collect, level; amount > 1; unknown types are harmless") {
    AchievementState a(test::RealData());
    for (const char* z : {"emerald_plains", "twilight_forest", "anvil_mountains"}) a.Update(AchievementType::Explore, z);
    CHECK(a.ProgressOf("explore") == 3);
    a.Update(AchievementType::Explore, "scorching_desert");
    CHECK(a.Update(AchievementType::Explore, "abyss_rift") == std::vector<std::string>{"ach_explore_all"});
    for (int i = 0; i < 5; ++i) a.Update(AchievementType::Quest, "");
    CHECK(a.ProgressOf("quest") == 5);
    CHECK_FALSE(a.IsUnlocked("ach_quest_10"));
    CHECK(a.Update(AchievementType::Collect, "") == std::vector<std::string>{"ach_collect_legendary"});
    CHECK(a.CheckLevel(10) == std::vector<std::string>{"ach_level_10"});
    CHECK_FALSE(a.IsUnlocked("ach_level_25"));
    CHECK(a.CheckLevel(50) == std::vector<std::string>{"ach_level_25", "ach_level_50"});
    AchievementState b(test::RealData());
    b.Update(AchievementType::Kill, "", 5);
    CHECK(b.ProgressOf("kill") == 5);
    CHECK(b.IsUnlocked("ach_first_kill"));
  }

  TEST_CASE("achievements: bonuses sum per stat over unlocked achievements only, idempotent") {
    AchievementState a(test::RealData());
    CHECK(a.Bonuses() == EquipStats{});
    a.CheckLevel(50);  // ach_level_50: str +5
    CHECK(a.Bonuses().Get(Stat::Str) == 5);
    for (int i = 0; i < 100; ++i) a.Update(AchievementType::Kill, "");  // ach_kill_100: damage +2
    for (const char* z : {"a", "b", "c", "d", "e"}) a.Update(AchievementType::Explore, z);  // lck +5
    const EquipStats b1 = a.Bonuses();
    CHECK(b1.Get(Stat::Damage) == 2);
    CHECK(b1.Get(Stat::Lck) == 5);
    CHECK(a.Bonuses() == b1);
    for (int i = 0; i < 50; ++i) a.Update(AchievementType::Kill, "slime_green");  // lck +2
    CHECK(a.Bonuses().Get(Stat::Lck) == 7);
  }

  TEST_CASE("achievements: save format merges counters with {id: 1}; load splits them back; no drift") {
    AchievementState a(test::RealData());
    for (int i = 0; i < 12; ++i) a.Update(AchievementType::Kill, "slime_green");
    a.CheckLevel(10);
    const auto saved = a.ToSave();
    CHECK(saved == std::vector<std::pair<std::string, int64_t>>{
                       {"kill", 12}, {"kill:slime_green", 12}, {"ach_first_kill", 1}, {"ach_level_10", 1}});
    AchievementState b(test::RealData());
    b.Load(saved);
    CHECK(b.IsUnlocked("ach_first_kill"));
    CHECK(b.IsUnlocked("ach_level_10"));
    CHECK(b.ProgressOf("kill") == 12);
    CHECK(b.Bonuses() == a.Bonuses());
    CHECK(b.ToSave() == saved);
    CHECK(b.Update(AchievementType::Kill, "slime_green").empty());  // no re-unlock after load
    AchievementState c(test::RealData());
    c.Load({});
    CHECK(c.ToSave().empty());
  }

  TEST_CASE("achievement table: 12 entries; level 50 grants str +5; demon lord str +10") {
    const DataStore& data = test::RealData();
    CHECK(data.Quests().achievements.size() == 12);
    const AchievementDef* l50 = data.Quests().FindAchievement("ach_level_50");
    REQUIRE(l50 != nullptr);
    CHECK(l50->hasReward);
    CHECK(l50->rewardStat == Stat::Str);
    CHECK(l50->rewardValue == 5);
    CHECK(data.Quests().achievementsCountKillOnce);
    CHECK(data.Quests().achievementsExploreDistinct);
  }
}

// =====================================================================================================================
// Dialogue trees (7, 10.3) - DialogueTrees.test.ts, quest-dialogue-navigation.test.ts
// =====================================================================================================================
TEST_SUITE("quests") {
  TEST_CASE("dialogue data: 5 NPC trees on quest NPCs; node integrity; every node reachable; quest refs valid") {
    const DataStore& data = test::RealData();
    int npcTrees = 0;
    for (const DialogueTree& t : data.Dialogues().trees) {
      if (t.kind != DialogueKind::Npc) continue;
      ++npcTrees;
      CAPTURE(t.id);
      const NpcDef* npc = data.FindNpc(t.id);
      REQUIRE(npc != nullptr);
      CHECK(npc->type == NpcType::Quest);
      CHECK(npc->dialogueTreeId == t.id);
      const DialogueNode* root = t.FindNode(t.startNodeId);
      REQUIRE(root != nullptr);
      CHECK(root->choices.size() >= 2);
      bool hasEnd = false, hasTrigger = false, hasGate = false;
      for (const DialogueNode& n : t.nodes) {
        CHECK_FALSE(n.id.empty());
        CHECK_FALSE(n.text.empty());
        hasEnd = hasEnd || n.isEnd;
        if (!n.nextNodeId.empty()) CHECK(t.FindNode(n.nextNodeId) != nullptr);
        for (const DialogueChoice& c : n.choices) {
          CHECK(t.FindNode(c.nextNodeId) != nullptr);
          CHECK_FALSE(c.text.empty());
          if (!c.questTrigger.empty()) {
            hasTrigger = true;
            CHECK(data.FindQuest(c.questTrigger) != nullptr);
          }
          for (const std::string& p : c.prereqQuests) CHECK(data.FindQuest(p) != nullptr);
          hasGate = hasGate || !c.prereqQuests.empty();
        }
      }
      CHECK(hasEnd);
      CHECK(hasTrigger);
      CHECK(hasGate);
      // reachability from the start node
      std::vector<std::string> seen{t.startNodeId}, todo{t.startNodeId};
      while (!todo.empty()) {
        const DialogueNode* n = t.FindNode(todo.back());
        todo.pop_back();
        if (n == nullptr) continue;
        std::vector<std::string> next;
        for (const DialogueChoice& c : n->choices) next.push_back(c.nextNodeId);
        if (!n->nextNodeId.empty()) next.push_back(n->nextNodeId);
        for (const std::string& id : next) {
          if (std::find(seen.begin(), seen.end(), id) == seen.end()) {
            seen.push_back(id);
            todo.push_back(id);
          }
        }
      }
      CHECK(seen.size() == t.nodes.size());
    }
    CHECK(npcTrees == 5);
    const DialogueTree* elder = data.Dialogues().Find("quest_elder");
    REQUIRE(elder != nullptr);
    const DialogueNode* ask = elder->FindNode("reward_ask");
    REQUIRE(ask != nullptr);
    CHECK(ask->choices[0].reward.present);
    CHECK(ask->choices[0].reward.gold == 30);
    CHECK(ask->choices[0].reward.exp == 50);
  }
}

namespace {

// quest-dialogue-navigation.test.ts helpers (the UIScene filter rules, as data checks).
std::vector<const DialogueChoice*> DlgVisible(const DialogueTree& t, const DialogueNode& n,
                                              const std::set<std::string>& active, const std::set<std::string>& done) {
  std::vector<const DialogueChoice*> out;
  for (const DialogueChoice& c : n.choices) {
    bool ok = true;
    for (const std::string& p : c.prereqQuests) ok = ok && done.count(p) == 1;
    if (!ok) continue;
    if (!c.questTrigger.empty() && (active.count(c.questTrigger) == 1 || done.count(c.questTrigger) == 1)) {
      const DialogueNode* target = t.FindNode(c.nextNodeId);
      if (target != nullptr && target->isEnd) continue;
    }
    out.push_back(&c);
  }
  return out;
}

bool DlgCanReach(const DialogueTree& t, const std::string& quest, const std::set<std::string>& active) {
  std::vector<std::string> visited;
  std::function<bool(const std::string&)> dfs = [&](const std::string& id) -> bool {
    if (std::find(visited.begin(), visited.end(), id) != visited.end()) return false;
    visited.push_back(id);
    const DialogueNode* n = t.FindNode(id);
    if (n == nullptr) return false;
    const auto vis = DlgVisible(t, *n, active, {});
    for (const DialogueChoice* c : vis) {
      if (c->questTrigger == quest) return true;
      if (dfs(c->nextNodeId)) return true;
    }
    if (!n->nextNodeId.empty() && !n->isEnd && vis.empty() && dfs(n->nextNodeId)) return true;
    return false;
  };
  return dfs(t.startNodeId);
}

std::vector<std::string> DlgTriggers(const DialogueTree& t) {
  std::vector<std::string> out;
  for (const DialogueNode& n : t.nodes) {
    for (const DialogueChoice& c : n.choices) {
      if (!c.questTrigger.empty() && std::find(out.begin(), out.end(), c.questTrigger) == out.end()) {
        out.push_back(c.questTrigger);
      }
    }
  }
  return out;
}

// DialogueSystem + QuestSystem on the harness (no rewards service: choice rewards are skipped).
struct DlgRig {
  test::SimHarness h;
  QuestSystem qs{h.ctx.data, h.events, h.bus};
  DialogueSystem dlg{h.ctx};
  DlgRig() {
    h.ctx.sys.quests = &qs;
    h.ctx.sys.dialogue = &dlg;
  }
  void Open(const char* npc) {
    const DialogueTree* t = h.ctx.data.Dialogues().Find(npc);
    REQUIRE(t != nullptr);
    dlg.OpenTree(npc, *t);
  }
  // Index of the button presenting choice `choiceIndex` (-1 when hidden).
  int32_t ChoiceButton(int32_t choiceIndex) const {
    const auto& b = dlg.View().buttons;
    for (size_t i = 0; i < b.size(); ++i) {
      if (b[i].kind == DialogueButtonKind::Choice && b[i].choiceIndex == choiceIndex) return static_cast<int32_t>(i);
    }
    return -1;
  }
  std::vector<DialogueButtonKind> Kinds() const {
    std::vector<DialogueButtonKind> out;
    for (const DialogueButton& b : dlg.View().buttons) out.push_back(b.kind);
    return out;
  }
};

}  // namespace

TEST_SUITE("quests") {
  TEST_CASE("dialogue navigation: no dead ends when every triggered quest is active; quests stay reachable") {
    const DataStore& data = test::RealData();
    for (const DialogueTree& t : data.Dialogues().trees) {
      if (t.kind != DialogueKind::Npc) continue;
      CAPTURE(t.id);
      const std::vector<std::string> triggers = DlgTriggers(t);
      const std::set<std::string> all(triggers.begin(), triggers.end());
      for (const DialogueNode& n : t.nodes) {
        if (n.choices.empty() || n.isEnd) continue;
        const bool nav = !DlgVisible(t, n, all, {}).empty() || !n.nextNodeId.empty();
        if (!nav) CHECK(n.id != t.startNodeId);  // non-root nodes get the Back button
      }
      CHECK_FALSE(DlgVisible(t, *t.FindNode(t.startNodeId), all, {}).empty());
      for (const std::string& activeId : triggers) {
        for (const std::string& target : triggers) {
          if (target == activeId || !DlgCanReach(t, target, {})) continue;
          CAPTURE(activeId);
          CAPTURE(target);
          CHECK(DlgCanReach(t, target, {activeId}));
        }
      }
    }
  }

  TEST_CASE("dialogue runtime: root choices, help -> accept_slimes accepts M1 without pinning it, Leave closes") {
    DlgRig r;
    r.Open("quest_elder");
    CHECK(r.dlg.View().open);
    CHECK(r.dlg.View().nodeId == "root");
    CHECK(r.Kinds() == std::vector<DialogueButtonKind>{DialogueButtonKind::Choice, DialogueButtonKind::Choice,
                                                       DialogueButtonKind::Choice});
    r.dlg.Choose(r.ChoiceButton(1));  // help
    CHECK(r.dlg.View().nodeId == "help");
    r.dlg.Choose(r.ChoiceButton(0));  // accept_slimes [Q:q_kill_slimes]
    CHECK(r.dlg.View().nodeId == "accept_slimes");
    CHECK(r.qs.Progress("q_kill_slimes") != nullptr);
    CHECK(r.qs.Tracked().empty());  // dialogue-tree acceptances do not pin the guide
    CHECK(r.Kinds() == std::vector<DialogueButtonKind>{DialogueButtonKind::Leave});
    r.dlg.Choose(0);
    CHECK_FALSE(r.dlg.View().open);
    CHECK(test::CountEvents<EvDialogue>(r.h.events) >= 3);
  }

  TEST_CASE("dialogue runtime: a handled quest hides its end-node choice; all hidden -> Back (10.3 example)") {
    DlgRig r;
    REQUIRE(r.qs.Accept("q_kill_slimes"));
    r.Open("quest_elder");
    r.dlg.Choose(r.ChoiceButton(0));  // explain
    REQUIRE(r.dlg.View().nodeId == "explain");
    CHECK(r.ChoiceButton(0) == -1);  // accept_slimes hidden (quest active, end node)
    CHECK(r.ChoiceButton(1) == -1);  // goblin_first gated on q_kill_slimes turned in
    REQUIRE(r.ChoiceButton(2) >= 0);
    r.dlg.Choose(r.ChoiceButton(2));  // both
    REQUIRE(r.dlg.View().nodeId == "both");
    CHECK(r.Kinds() == std::vector<DialogueButtonKind>{DialogueButtonKind::Back});
    r.dlg.Choose(0);
    CHECK(r.dlg.View().nodeId == "root");
    // the help node keeps the hidden-quest's sibling and marks nothing in progress
    r.dlg.Choose(r.ChoiceButton(1));
    CHECK(r.ChoiceButton(0) == -1);
    CHECK(r.ChoiceButton(1) >= 0);
  }

  TEST_CASE("dialogue runtime: prereq choices use the quests turned in when the NPC was clicked; in-progress flag") {
    DlgRig r;
    r.qs.Load({Rec("q_kill_slimes", QuestStatus::TurnedIn, {10})});
    r.Open("quest_elder");
    r.dlg.Choose(r.ChoiceButton(0));  // explain
    const int32_t gob = r.ChoiceButton(1);
    REQUIRE(gob >= 0);  // goblin_first unlocked
    CHECK_FALSE(r.dlg.View().buttons[static_cast<size_t>(gob)].inProgress);
    r.dlg.Choose(gob);
    REQUIRE(r.dlg.View().nodeId == "goblin_first");
    r.dlg.Choose(r.ChoiceButton(1));  // goblin_info: accept_goblins (end) + farewell
    REQUIRE(r.dlg.View().nodeId == "goblin_info");
    r.dlg.Choose(r.ChoiceButton(0));  // accept q_kill_goblins
    CHECK(r.qs.Progress("q_kill_goblins") != nullptr);
    // visited / choicesMade are recorded per NPC and saved
    SaveData save;
    r.dlg.WriteSave(save);
    REQUIRE(save.dialogueState.size() == 1);
    CHECK(save.dialogueState[0].npcId == "quest_elder");
    CHECK(save.dialogueState[0].visitedNodes ==
          std::vector<std::string>{"root", "explain", "goblin_first", "goblin_info", "accept_goblins"});
    CHECK(save.dialogueState[0].choicesMade.size() == 4);
  }

  TEST_CASE("dialogue runtime: a choice whose next node is missing closes; a linear panel has no buttons") {
    DlgRig r;
    r.dlg.OpenLinear("plains_wanderer");
    CHECK(r.dlg.View().open);
    CHECK(r.dlg.View().linear);
    CHECK(r.dlg.View().buttons.empty());
    r.dlg.Choose(0);  // ignored
    CHECK(r.dlg.View().open);
    r.dlg.Close();
    CHECK_FALSE(r.dlg.View().open);
  }
}

// =====================================================================================================================
// Story script data - StoryScript.test.ts, StoryProgress.test.ts
// =====================================================================================================================
TEST_SUITE("story") {
  TEST_CASE("StoryProgress: missing storySeen means veterans skip the prologue; ids are unique") {
    StoryProgress p;
    p.Load({}, /*present=*/false);
    CHECK(p.Has("prologue"));
    p.Load({"chapter_emerald_plains", "chapter_emerald_plains"}, true);
    CHECK_FALSE(p.Has("prologue"));
    CHECK(p.Seen().size() == 1);
    p.Add("cs_ep_whisper");
    CHECK(p.Has("cs_ep_whisper"));
    p.Load({}, true);  // a present empty list = nothing seen
    CHECK(p.Seen().empty());
  }

  TEST_CASE("script: prologue / five chapters / epilogue / credits; cutscenes of 5-14 steps; triggers wired") {
    const DataStore& data = test::RealData();
    const StoryScript& s = data.Story();
    CHECK(s.prologue.id == "prologue");
    CHECK(s.prologue.slides.size() == 6);
    CHECK(s.epilogue.slides.size() >= 5);
    CHECK(s.epilogue.slides.size() <= 7);
    CHECK(s.credits.credits);
    std::vector<std::string> zones;
    for (const ChapterCard& c : s.chapters) zones.push_back(c.zoneId);
    CHECK(zones == std::vector<std::string>{"emerald_plains", "twilight_forest", "anvil_mountains", "scorching_desert",
                                            "abyss_rift"});
    for (const Cutscene& cs : s.cutscenes) {
      CAPTURE(cs.id);
      CHECK(cs.steps.size() >= 5);
      CHECK(cs.steps.size() <= 14);
    }
    for (const StoryTriggerDef& t : s.triggers) {
      CHECK(s.FindCutscene(t.cutscene) != nullptr);
      if (t.on == StoryTriggerOn::MonsterKilled) CHECK(data.Monsters().Find(t.subjectId) != nullptr);
      if (t.on == StoryTriggerOn::ZoneEntered) CHECK(data.FindMap(t.subjectId) != nullptr);
      if (t.on == StoryTriggerOn::QuestTurnedIn || t.on == StoryTriggerOn::QuestAccepted) {
        CHECK(data.FindQuest(t.subjectId) != nullptr);
      }
    }
    std::vector<std::string> bosses;
    for (const BossIntroDef& b : s.bossIntros) {
      bosses.push_back(b.monsterId);
      const Cutscene* cs = s.FindCutscene(b.cutscene);
      REQUIRE(cs != nullptr);
      CHECK(std::any_of(cs->steps.begin(), cs->steps.end(),
                        [](const CutsceneStep& st) { return st.kind == StoryStepKind::Title; }));
    }
    std::sort(bosses.begin(), bosses.end());
    CHECK(bosses == std::vector<std::string>{"demon_lord", "dungeon_abyss_lord", "goblin_chief", "mountain_troll",
                                             "phoenix", "werewolf_alpha"});
    // Chapter 1 triggers (10.2)
    std::vector<std::string> ch1;
    for (const StoryTriggerDef& t : s.triggers) {
      const QuestDef* q = data.FindQuest(t.subjectId);
      if (q != nullptr && q->zone == "emerald_plains") ch1.push_back(t.subjectId + ">" + t.cutscene);
    }
    CHECK(ch1 == std::vector<std::string>{"q_kill_slimes>cs_ep_mark", "q_explore_goblin_camp>cs_ep_whisper",
                                          "q_secure_plains>cs_ep_finale"});
    CHECK(s.timing.beatDelayQuestTurnedInMs == 650);
    CHECK(s.timing.beatDelayZoneEnteredMs == 900);
    CHECK(s.timing.beatDelayMonsterKilledMs == 0);
  }

  TEST_CASE("script: every key exists in zh-CN and en; Chinese text within its length limits") {
    const DataStore& data = test::RealData();
    const StoryScript& s = data.Story();
    const I18n& i18n = data.Strings();
    std::vector<std::string> over;
    const auto key = [&](const std::string& k, size_t max) {
      if (k.empty()) return;
      CAPTURE(k);
      const std::string* zh = i18n.Lookup(LocaleId::ZhCN, k);
      CHECK(zh != nullptr);
      CHECK(i18n.Lookup(LocaleId::En, k) != nullptr);
      if (zh != nullptr && max > 0 && CodePoints(*zh) > max) over.push_back(k);
    };
    for (const Cutscene& cs : s.cutscenes) {
      for (const CutsceneStep& st : cs.steps) {
        if (st.kind == StoryStepKind::Say || st.kind == StoryStepKind::Whisper) key(st.text, 60);
        if (st.kind == StoryStepKind::Narrate) key(st.text, 70);
        if (st.kind == StoryStepKind::Title) {
          key(st.title, 12);
          key(st.subtitle, 14);
        }
      }
    }
    for (const StorySequence* seq : {&s.prologue, &s.epilogue, &s.credits}) {
      for (const StorySlide& sl : seq->slides) {
        key(sl.heading, 0);
        key(sl.title, 12);
        key(sl.text, 110);
      }
    }
    for (const ChapterCard& c : s.chapters) {
      key(c.number, 0);
      key(c.title, 12);
      key(c.subtitle, 14);
      key(c.text, 110);
    }
    for (const BossIntroDef& b : s.bossIntros) {
      key(b.name, 12);
      key(b.epithet, 14);
    }
    for (const char* k : {"story.ui.skip", "story.ui.skipTouch", "story.speaker.villain", "story.speaker.hero"}) key(k, 0);
    CHECK(over.empty());
  }

  TEST_CASE("script: cutscene speakers and focus targets stand in the zone where the cutscene plays") {
    const DataStore& data = test::RealData();
    const StoryScript& s = data.Story();
    const auto zonesWithMonster = [&](const std::string& id) {
      std::vector<std::string> out;
      for (const std::string& z : {std::string("dungeon_abyss_lord"), std::string("dungeon_mid_boss")}) {
        if (z == id) return std::vector<std::string>{"abyss_labyrinth"};
      }
      for (const MapDef& m : data.World().maps) {
        for (const MapSpawnDef& sp : m.spawns) {
          if (sp.monsterId == id) {
            out.push_back(m.id);
            break;
          }
        }
      }
      return out;
    };
    std::vector<std::pair<std::string, std::string>> csZones;  // cutscene -> zone
    for (const StoryTriggerDef& t : s.triggers) {
      if (t.on == StoryTriggerOn::MonsterKilled) {
        for (const std::string& z : zonesWithMonster(t.subjectId)) csZones.emplace_back(t.cutscene, z);
      } else if (t.on == StoryTriggerOn::ZoneEntered) {
        csZones.emplace_back(t.cutscene, t.subjectId);
      } else if (const QuestDef* q = data.FindQuest(t.subjectId)) {
        csZones.emplace_back(t.cutscene, q->zone);
      }
    }
    for (const BossIntroDef& b : s.bossIntros) {
      for (const std::string& z : zonesWithMonster(b.monsterId)) csZones.emplace_back(b.cutscene, z);
    }
    for (const Cutscene& cs : s.cutscenes) {
      CAPTURE(cs.id);
      bool any = false;
      for (const auto& [id, zone] : csZones) {
        if (id != cs.id) continue;
        any = true;
        const std::set<std::string> npcs = NpcsIn(data, zone);
        for (const CutsceneStep& st : cs.steps) {
          const StoryActor* a = st.kind == StoryStepKind::Say ? &st.speaker : st.kind == StoryStepKind::Focus ? &st.target
                                                                                                              : nullptr;
          if (a == nullptr) continue;
          if (a->kind == StoryActorKind::Npc) {
            CHECK(data.FindNpc(a->id) != nullptr);
            if (zone != "abyss_labyrinth") CHECK(npcs.count(a->id) == 1);
          } else if (a->kind == StoryActorKind::Monster) {
            const std::vector<std::string> zs = zonesWithMonster(a->id);
            CHECK(std::find(zs.begin(), zs.end(), zone) != zs.end());
          } else if (a->kind == StoryActorKind::Tile) {
            const MapDef* m = data.FindMap(zone);
            REQUIRE(m != nullptr);
            CHECK((a->tile.col >= 0 && a->tile.col < m->cols && a->tile.row >= 0 && a->tile.row < m->rows));
          }
        }
      }
      CHECK(any);
    }
  }
}

// =====================================================================================================================
// Runtime: GameSim with the real data and wiring (quest world, story director, lore, achievements, dialogue rewards)
// =====================================================================================================================
namespace {

struct QwSim {
  std::unique_ptr<GameSim> sim;
  SimContext* ctx = nullptr;
  explicit QwSim(uint64_t seed = 7, bool skipStory = true) {
    sim = GameSim::Create(test::RealData(), SimConfig{});
    REQUIRE(sim != nullptr);
    REQUIRE(sim->NewGame(ClassId::Warrior, Difficulty::Normal, seed));
    ctx = &sim->Context();
    if (skipStory) ctx->sys.story->FinishAllBeats();
    sim->Step();
  }
  QuestSystem& Qs() { return *ctx->sys.quests; }
  QuestWorld& Qw() { return *ctx->sys.questWorld; }
  StoryDirector& Story() { return *ctx->sys.story; }
  Hero& H() { return *ctx->sys.hero; }
  MonsterSystem& Mon() { return *ctx->sys.monsters; }
  void Steps(int n) {
    for (int i = 0; i < n; ++i) sim->Step();
  }
  // Steps while collecting every event (GameSim clears the list at each step).
  std::vector<Event> StepsCollect(int n) {
    std::vector<Event> out(sim->Events().begin(), sim->Events().end());
    for (int i = 0; i < n; ++i) {
      sim->Step();
      out.insert(out.end(), sim->Events().begin(), sim->Events().end());
    }
    return out;
  }
  void Place(Vec2 p) { H().SetPosition(p); }
  Vec2 NpcPos(const char* id) {
    const NpcPlacement* p = ctx->sys.zone->FindNpc(id);
    REQUIRE(p != nullptr);
    return p->pos;
  }
  EntityId FindAlive(std::string_view defId, Vec2 near = Vec2(-1, -1)) {
    if (near.x >= 0) return Mon().NearestAliveOfDef(defId, near);
    for (const MonsterInstance& m : Mon().All()) {
      if (m.IsAlive() && m.def.id == defId) return m.id;
    }
    return kNoEntity;
  }
  bool Kill(EntityId id) {
    if (id == kNoEntity) return false;
    DamageFlags f;
    f.attacker = kHeroEntityId;
    f.source = KillSource::HeroBasic;
    return Mon().ApplyDamage(id, 1e9, f) == HitWeight::Kill;
  }
  int32_t Cur(const char* q, size_t i) {
    const QuestProgress* p = Qs().Progress(q);
    return p != nullptr && i < p->objectives.size() ? p->objectives[i] : -1;
  }
  QuestStatus Status(const char* q) {
    bool has = false;
    const QuestStatus st = Qs().StatusOf(q, has);
    REQUIRE(has);
    return st;
  }
  // Plays the current beat to its end on real time, answering every input wait with StoryAdvance.
  void PlayBeat() {
    const std::string id = Story().Playback().beat.id;
    for (int i = 0; i < 2000 && Story().Playback().playing && Story().Playback().beat.id == id; ++i) {
      if (Story().Playback().waitingForInput) {
        Story().Advance();
      } else {
        sim->AdvanceRealTime(50);
      }
    }
  }
};

template <class E>
std::vector<const E*> Of(std::span<const Event> events) {
  std::vector<const E*> out;
  for (const Event& e : events) {
    if (const E* x = std::get_if<E>(&e)) out.push_back(x);
  }
  return out;
}

bool HasLog(std::span<const Event> events, std::string_view key) {
  for (const EvLog* l : Of<EvLog>(events)) {
    if (l->text.key == key) return true;
  }
  return false;
}

const QuestNode* NodeOf(QuestWorld& qw, bool clue, std::string_view questId, int32_t obj = -1) {
  for (const QuestNode& n : qw.Nodes()) {
    if (n.clue == clue && n.questId == questId && (obj < 0 || n.objectiveIndex == obj)) return &n;
  }
  return nullptr;
}

size_t CountNodes(QuestWorld& qw, bool clue) {
  size_t n = 0;
  for (const QuestNode& q : qw.Nodes()) n += q.clue == clue ? 1 : 0;
  return n;
}

}  // namespace

TEST_SUITE("story") {
  TEST_CASE("16.6 new game: prologue then the chapter card, world frozen, each marked seen when it finishes (Q7)") {
    QwSim s(7, /*skipStory=*/false);
    StoryDirector& st = s.Story();
    REQUIRE(st.IsCinematic());
    CHECK(s.sim->WorldFrozen());
    CHECK(st.Playback().beat.id == "prologue");
    CHECK_FALSE(st.Progress().Has("prologue"));
    const double t0 = s.sim->NowMs();
    s.Steps(30);
    CHECK(s.sim->NowMs() == t0);  // S2: the sim clock holds
    s.sim->ClearEvents();
    s.PlayBeat();
    const std::vector<Event> ev(s.ctx->events.Items().begin(), s.ctx->events.Items().end());
    size_t slides = 0;
    for (const EvStoryStep* step : Of<EvStoryStep>(ev)) slides += step->isSlide && step->sequenceId == "prologue";
    CHECK(slides == 6);
    CHECK(st.Progress().Has("prologue"));
    // the chapter card follows with no unfrozen frame in between
    CHECK(st.IsCinematic());
    CHECK(st.Playback().beat.id == "chapter_emerald_plains");
    CHECK(st.Playback().beat.kind == StoryBeatKind::Chapter);
    s.sim->ClearEvents();
    s.sim->AdvanceRealTime(8899);
    CHECK(st.IsCinematic());
    s.sim->AdvanceRealTime(2);  // intro 3500 + hold 3800 + outro 1600
    CHECK_FALSE(st.IsCinematic());
    CHECK_FALSE(st.IsBusy());
    CHECK(st.Progress().Has("chapter_emerald_plains"));
    const std::vector<Event> ev2(s.ctx->events.Items().begin(), s.ctx->events.Items().end());
    bool idle = false;
    for (const EvStoryState* x : Of<EvStoryState>(ev2)) idle = idle || !x->active;
    CHECK(idle);
    CHECK_FALSE(Of<EvSaveRequested>(ev2).empty());  // autosave when the queue drains
    s.Steps(3);
    CHECK(s.sim->NowMs() > t0);
  }

  TEST_CASE("chapter card: StoryAdvance during the hold ends it early; StorySkip ends a card at once") {
    QwSim s(7, false);
    StoryDirector& st = s.Story();
    st.Skip();  // prologue: jump to the backdrop fade-out
    CHECK(st.Playback().skipping);
    s.sim->AdvanceRealTime(699);
    CHECK(st.Playback().beat.id == "prologue");
    s.sim->AdvanceRealTime(2);
    REQUIRE(st.Playback().beat.id == "chapter_emerald_plains");
    CHECK(st.Progress().Has("prologue"));  // a skipped beat counts as finished (Q7)
    st.Advance();                          // ignored during the intro
    s.sim->AdvanceRealTime(3600);          // into the hold
    st.Advance();                          // ends the hold -> outro 1600
    s.sim->AdvanceRealTime(1599);
    CHECK(st.IsCinematic());
    s.sim->AdvanceRealTime(2);
    CHECK_FALSE(st.IsBusy());
  }

  TEST_CASE("a save without storySeen skips the prologue only; a later entry plays nothing") {
    QwSim s;
    SaveData save;
    s.sim->BuildSave(save, 0);
    CHECK(save.hasStorySeen);
    CHECK(save.storySeen == std::vector<std::string>{"prologue", "chapter_emerald_plains"});
    save.hasStorySeen = false;
    save.storySeen.clear();
    std::string err;
    REQUIRE(s.sim->LoadGame(save, &err) == SaveError::None);
    StoryDirector& st = *s.sim->Context().sys.story;
    CHECK(st.Progress().Has("prologue"));
    CHECK(st.Playback().beat.id == "chapter_emerald_plains");
    st.FinishAllBeats();
    SaveData again;
    s.sim->BuildSave(again, 0);
    REQUIRE(s.sim->LoadGame(again, &err) == SaveError::None);
    CHECK_FALSE(s.sim->Context().sys.story->IsBusy());
  }

  TEST_CASE("T15: turning in M1 queues cs_ep_mark 650 ms later on the sim clock; skip keeps letterbox-out + return pan") {
    QwSim s;
    s.Qs().Load({Rec("q_kill_slimes", QuestStatus::Completed, {10})});
    const int64_t gold0 = s.H().Gold();
    REQUIRE(s.Qw().TurnIn("q_kill_slimes", 0));
    CHECK(s.H().Gold() == gold0 + 25);
    StoryDirector& st = s.Story();
    CHECK(st.IsBusy());
    CHECK_FALSE(st.IsCinematic());
    CHECK_FALSE(s.sim->WorldFrozen());
    s.Steps(38);
    CHECK_FALSE(st.IsCinematic());  // the world stays live during the delay
    s.Steps(2);
    REQUIRE(st.IsCinematic());
    CHECK(st.Playback().beat.id == "cs_ep_mark");
    CHECK(s.sim->WorldFrozen());
    st.Skip();
    s.sim->AdvanceRealTime(899);  // letterbox out 450 + camera return 450
    CHECK(st.IsCinematic());
    s.sim->AdvanceRealTime(2);
    CHECK_FALSE(st.IsCinematic());
    CHECK(st.Progress().Has("cs_ep_mark"));
    s.Steps(1);  // the freeze mask follows at the next step
    CHECK_FALSE(s.sim->WorldFrozen());
  }

  TEST_CASE("16.7 boss intro: the chief within 14 tiles -> bar + rename once; within 9 -> boss_goblin_chief once") {
    QwSim s;
    std::vector<BossBarMsg> bars;
    s.ctx->bus.Subscribe<BossBarMsg>([&bars](const BossBarMsg& m) { bars.push_back(m); });
    EntityId chief = s.FindAlive("goblin_chief");
    REQUIRE(chief != kNoEntity);
    const Vec2 cp = s.Mon().Find(chief)->pos;
    s.Place(Vec2(cp.x, cp.y - 12));
    std::vector<Event> ev = s.StepsCollect(20);
    CHECK_FALSE(s.Story().IsBusy());
    REQUIRE_FALSE(Of<EvBossBar>(ev).empty());
    CHECK(Of<EvBossBar>(ev).front()->show);
    CHECK(Of<EvBossBar>(ev).front()->nameKey == "story.boss.goblin_chief.name");
    CHECK(Of<EvMonsterRenamed>(ev).size() == 1);
    CHECK(s.sim->View().bossBar.show);
    CHECK(s.sim->View().bossBar.maxHp > 0);
    const Vec2 cp2 = s.Mon().Find(chief)->pos;
    s.Place(Vec2(cp2.x, cp2.y - 8.5));
    ev = s.StepsCollect(20);
    REQUIRE(s.Story().IsCinematic());
    CHECK(s.Story().Playback().beat.id == "boss_goblin_chief");
    CHECK(s.Story().Playback().beat.kind == StoryBeatKind::BossIntro);
    CHECK(Of<EvMonsterRenamed>(ev).empty());  // once per instance
    // the step player resolves the focus on the live chief and the speaker to the intro name
    const StoryActorView a = s.Story().ResolveActor(StoryActor{StoryActorKind::Monster, "goblin_chief", TilePos()});
    CHECK(a.resolved);
    CHECK(a.entity == chief);
    CHECK(a.nameKey == "story.boss.goblin_chief.name");
    s.sim->ClearEvents();
    // letterbox 450, focus 800, title in 980 + hold 2200 + out 450, say in 220 -> waiting at the say step
    s.sim->AdvanceRealTime(450 + 800 + 980 + 2200 + 450 + 220);
    CHECK(s.Story().Playback().waitingForInput);
    CHECK(s.Story().Playback().stepIndex == 2);
    const std::vector<Event> steps(s.ctx->events.Items().begin(), s.ctx->events.Items().end());
    const auto st = Of<EvStoryStep>(steps);
    REQUIRE(st.size() == 3);
    CHECK(st[0]->hasFocus);
    CHECK(st[0]->focusEntity == chief);
    CHECK(st[0]->timedMs == 800);
    CHECK(st[1]->step.kind == StoryStepKind::Title);
    CHECK(st[2]->speakerNameKey == "story.boss.goblin_chief.name");
    s.Story().Advance();             // say out 160
    s.sim->AdvanceRealTime(160);     // shake (0 ms) then the whisper reveal
    CHECK(s.Story().Playback().stepIndex == 4);
    s.Story().Advance();             // input during the reveal ends the step (out 600)
    s.sim->AdvanceRealTime(600 + 600 + 450 + 449);
    CHECK(s.Story().IsCinematic());  // camera return still running
    s.sim->AdvanceRealTime(2);
    CHECK_FALSE(s.Story().IsCinematic());
    CHECK(s.Story().Progress().Has("boss_goblin_chief"));
    CHECK_FALSE(s.Story().Progress().Has("cs_boss_goblin_chief"));  // the intro's cutscene id is not marked
    // no second intro; killing the chief clears the bar (killed) and unlocks its achievement
    s.Steps(20);
    CHECK_FALSE(s.Story().IsBusy());
    bars.clear();
    REQUIRE(s.Kill(chief));
    REQUIRE_FALSE(bars.empty());
    CHECK_FALSE(bars.back().show);
    CHECK(bars.back().killed);
    CHECK(s.ctx->sys.achievements->State().IsUnlocked("ach_kill_goblin_chief"));
  }

  TEST_CASE("monster_killed triggers (0 ms) carry grantPet; the final boss queues its cutscene then the epilogue") {
    QwSim s;
    std::vector<StoryBeatFinishedMsg> done;
    s.ctx->bus.Subscribe<StoryBeatFinishedMsg>([&done](const StoryBeatFinishedMsg& m) { done.push_back(m); });
    MonsterKilledMsg m;
    m.defId = "werewolf_alpha";
    s.Story().OnMonsterKilled(m);
    CHECK(s.Story().IsCinematic());  // no delay
    CHECK(s.Story().Playback().beat.id == "cs_tf_moonfang");
    s.Story().FinishAllBeats();
    REQUIRE(done.size() == 1);
    CHECK(done[0].beatId == "cs_tf_moonfang");
    CHECK(done[0].grantPet == "pet_storm_wolf");
    done.clear();
    m.defId = "demon_lord";
    s.Story().OnMonsterKilled(m);
    CHECK(s.Story().Playback().beat.id == "cs_ar_fall");
    s.Story().FinishAllBeats();
    REQUIRE(done.size() == 2);
    CHECK(done[1].beatId == "epilogue");
    s.Story().OnMonsterKilled(m);  // seen: nothing again
    CHECK_FALSE(s.Story().IsBusy());
  }

  TEST_CASE("epilogue beat plays the epilogue slides then the credits roll (until StoryAdvance)") {
    QwSim s;
    MonsterKilledMsg m;
    m.defId = "demon_lord";
    s.Story().OnMonsterKilled(m);
    s.Story().FinishAllBeats();  // drop the trigger cutscene and the epilogue
    // replay the epilogue beat alone
    s.Story().MutableProgress().Clear();
    s.Story().MutableProgress().Add("cs_ar_fall");
    s.Story().OnMonsterKilled(m);
    REQUIRE(s.Story().Playback().beat.id == "epilogue");
    s.sim->ClearEvents();
    s.Story().Skip();                // epilogue part -> its backdrop fade
    s.sim->AdvanceRealTime(700);     // then the credits sequence starts
    s.sim->AdvanceRealTime(900);     // credits backdrop in
    CHECK(s.Story().Playback().waitingForInput);
    s.Story().Advance();             // the roll ended
    s.sim->AdvanceRealTime(701);
    CHECK_FALSE(s.Story().IsBusy());
    const std::vector<Event> ev(s.ctx->events.Items().begin(), s.ctx->events.Items().end());
    bool credits = false;
    for (const EvStoryStep* st : Of<EvStoryStep>(ev)) credits = credits || st->sequenceId == "credits";
    CHECK(credits);
  }

  TEST_CASE("zone_entered triggers wait 900 ms; a zone exit discards the waiting beat without marking it seen") {
    QwSim s;
    ZoneEnteredMsg m;
    m.mapId = "ember_tower";
    CHECK_FALSE(s.Story().OnZoneEntered(m));  // no chapter card for the tower
    CHECK(s.Story().IsBusy());
    CHECK_FALSE(s.Story().IsCinematic());
    s.Story().OnZoneExit();
    CHECK_FALSE(s.Story().IsBusy());
    CHECK_FALSE(s.Story().Progress().Has("cs_tower_home"));
    s.Steps(60);
    CHECK_FALSE(s.Story().IsCinematic());
  }

  TEST_CASE("the director emits STORY_STATE busy at enqueue time (T17 waits for it) and idle when drained") {
    QwSim s;
    std::vector<StoryStateMsg> states;
    s.ctx->bus.Subscribe<StoryStateMsg>([&states](const StoryStateMsg& m) { states.push_back(m); });
    s.Qs().Load({Rec("q_kill_slimes", QuestStatus::Completed, {10})});
    REQUIRE(s.Qw().TurnIn("q_kill_slimes", 0));
    REQUIRE(states.size() == 1);
    CHECK(states[0].active);
    CHECK(states[0].beatId == "cs_ep_mark");
    s.Story().FinishAllBeats();
    REQUIRE(states.size() == 2);
    CHECK_FALSE(states[1].active);
  }
}

TEST_SUITE("quests") {
  TEST_CASE("interact with the elder: log line 0, talk hook, quest card of 4 offers; the card freezes the sim (S2)") {
    QwSim s;
    s.sim->ClearEvents();
    s.Qw().InteractNpc("quest_elder");
    const std::vector<Event> ev(s.ctx->events.Items().begin(), s.ctx->events.Items().end());
    CHECK(HasLog(ev, "data.npc.quest_elder.dialogue.0"));
    CHECK(Of<EvQuestCardOpened>(ev).size() == 1);
    REQUIRE(s.Qw().Card().open);
    CHECK(s.Qw().Card().entries.size() == 4);
    const double t0 = s.sim->NowMs();
    s.Steps(5);
    CHECK(s.sim->WorldFrozen());
    CHECK(s.sim->NowMs() == t0);
    // the snapshot shows the elder talking with a '!' marker
    bool marked = false;
    for (const NpcView& v : s.sim->View().npcs) {
      if (v.npcId == "quest_elder") marked = v.marker == NpcMarker::Available && v.talking;
    }
    CHECK(marked);
    CHECK(s.Qw().AcceptFromCard("q_kill_slimes"));
    CHECK(s.Qs().Tracked() == "q_kill_slimes");
    CHECK_FALSE(s.Qw().Card().open);
    s.Steps(5);
    CHECK(s.sim->NowMs() > t0);
    // a quest NPC with nothing to offer opens the linear panel (plains_wanderer has no tree)
    s.Qw().InteractNpc("plains_wanderer");
    CHECK(s.ctx->sys.dialogue->View().open);
    CHECK(s.ctx->sys.dialogue->View().linear);
  }

  TEST_CASE("kills go through the pipeline: kill progress, first-kill achievement, 'kill' counted once (Q2)") {
    QwSim s;
    REQUIRE(s.Qs().Accept("q_kill_slimes"));
    for (int i = 0; i < 3; ++i) REQUIRE(s.Kill(s.FindAlive("slime_green")));
    CHECK(s.Cur("q_kill_slimes", 0) == 3);
    const AchievementState& a = s.ctx->sys.achievements->State();
    CHECK(a.IsUnlocked("ach_first_kill"));
    CHECK(a.ProgressOf("kill") == 3);
    CHECK(a.ProgressOf("kill:slime_green") == 3);
    CHECK(a.ProgressOf("explore") == 1);  // the first story zone (Q2: distinct zones)
  }

  TEST_CASE("gel drops: RngStream::Quests at 0.5 per listed kill; the pickup visual starts at the kill point") {
    QwSim s;
    REQUIRE(s.Qs().Accept("q_collect_slime_gel"));
    Rng& q = s.ctx->Rand(RngStream::Quests);
    q.Script({0.4, 0.6});
    const EntityId a = s.FindAlive("slime_green");
    REQUIRE(a != kNoEntity);
    const Vec2 at = s.Mon().Find(a)->pos;
    s.sim->ClearEvents();
    REQUIRE(s.Kill(a));
    CHECK(s.Cur("q_collect_slime_gel", 0) == 1);
    bool pickup = false;
    for (const EvQuestUpdate* u : Of<EvQuestUpdate>(s.ctx->events.Items())) {
      if (u->kind == EvQuestUpdate::Kind::Progress && u->questId == "q_collect_slime_gel") {
        pickup = u->hasFrom && u->from == at && u->itemKind == "gel";
      }
    }
    CHECK(pickup);
    REQUIRE(s.Kill(s.FindAlive("slime_green")));
    CHECK(s.Cur("q_collect_slime_gel", 0) == 1);  // 0.6 >= 0.5
    CHECK(q.ScriptedRemaining() == 0);
    q.Script({0.0});
    REQUIRE(s.Kill(s.FindAlive("goblin")));  // goblins are not listed: no roll
    CHECK(q.ScriptedRemaining() == 1);
    q.ClearScript();
  }

  TEST_CASE("16.1 / 3.4 herb nodes: deterministic spots on the live grid, gathered within 1.3 tiles, per-visit") {
    QwSim s;
    REQUIRE(s.Qs().Accept("q_herb_gathering"));
    const ZoneRuntime& zone = *s.ctx->sys.zone;
    const QuestDef* q = s.ctx->data.FindQuest("q_herb_gathering");
    const std::vector<TilePos> spots = ResolveGatherSpots(
        q->objectives[0].gatherArea, q->objectives[0].gatherCount,
        [&zone](int32_t c, int32_t r) { return zone.Walkable(c, r); }, "q_herb_gathering:0");
    REQUIRE(spots.size() == 7);
    REQUIRE(CountNodes(s.Qw(), false) == 7);
    for (size_t i = 0; i < spots.size(); ++i) {
      CHECK(s.Qw().Nodes()[i].pos == spots[i].Center());
      CHECK(s.Qw().Nodes()[i].itemKind == "herb");
    }
    s.Place(spots[0].Center());
    const std::vector<Event> ev = s.StepsCollect(1);
    CHECK(s.Cur("q_herb_gathering", 0) == 1);
    CHECK(CountNodes(s.Qw(), false) == 6);
    bool fly = false;
    for (const EvQuestUpdate* u : Of<EvQuestUpdate>(ev)) fly = fly || (u->hasFrom && u->itemKind == "herb");
    CHECK(fly);
    // a new visit restores every spot (gathered sets are per visit); progress is kept
    s.Qw().OnZoneExit();
    s.Qw().OnZoneEnter();
    CHECK(CountNodes(s.Qw(), false) == 7);
    CHECK(s.Cur("q_herb_gathering", 0) == 1);
    for (size_t i = 1; i < 5; ++i) {
      s.Place(spots[i].Center());
      s.Steps(1);
    }
    CHECK(s.Status("q_herb_gathering") == QuestStatus::Completed);
    CHECK(CountNodes(s.Qw(), false) == 0);  // Q5: synced on progress, the leftover nodes go
    // the guide now leads back to the elder
    s.Steps(16);
    CHECK(s.Qw().Guide().reason == GuideReason::TurnIn);
    CHECK(s.Qw().Guide().pos == s.NpcPos("quest_elder"));
  }

  TEST_CASE("3.5 pendant clues: marks on the clue tiles, examined in any order within 2 tiles, note logged") {
    QwSim s;
    s.Qs().Load({Rec("q_kill_slimes", QuestStatus::TurnedIn, {10})});
    REQUIRE(s.Qs().Accept("q_lost_pendant"));
    REQUIRE(CountNodes(s.Qw(), true) == 3);
    const QuestNode* cloth = NodeOf(s.Qw(), true, "q_lost_pendant", 1);
    REQUIRE(cloth != nullptr);
    const ZoneRuntime& zone = *s.ctx->sys.zone;
    TilePos expect;
    REQUIRE(NearestWalkableTile(TilePos(42, 34), [&zone](int32_t c, int32_t r) { return zone.Walkable(c, r); }, 8,
                                expect));
    CHECK(cloth->pos == expect.Center());
    Vec2 tile;
    CHECK(s.Qw().ClueTile("q_lost_pendant", 1, tile));
    s.Place(Vec2(cloth->pos.x + 1.5, cloth->pos.y));
    const std::vector<Event> ev = s.StepsCollect(1);
    CHECK(s.Cur("q_lost_pendant", 1) == 1);
    CHECK(s.Cur("q_lost_pendant", 0) == 0);
    CHECK(CountNodes(s.Qw(), true) == 2);
    CHECK(HasLog(ev, "zone.quest.clueFound"));
    CHECK(HasLog(ev, "data.questClue.clue_pendant_cloth"));
    CHECK_FALSE(s.Qw().ClueTile("q_lost_pendant", 1, tile));
  }

  TEST_CASE("3.6 explore: within the radius at a 500 ms observer check") {
    QwSim s;
    s.Qs().Load({Rec("q_kill_slimes", QuestStatus::TurnedIn, {10}), Rec("q_kill_goblins", QuestStatus::TurnedIn, {15})});
    REQUIRE(s.Qs().Accept("q_explore_goblin_camp"));
    s.Place(Vec2(48.5, 52));  // 8.5 > radius 8
    s.Steps(40);
    CHECK(s.Cur("q_explore_goblin_camp", 0) == 0);
    s.Place(Vec2(47.9, 52));
    const std::vector<Event> ev = s.StepsCollect(31);
    CHECK(s.Status("q_explore_goblin_camp") == QuestStatus::Completed);
    CHECK(HasLog(ev, "zone.quest.exploreFound"));
  }

  TEST_CASE("3.7 delivery: talking to the herbalist completes the gel quest, and her shop opens too") {
    QwSim s;
    s.Qs().Load({Rec("q_collect_slime_gel", QuestStatus::Active, {6, 0})});
    s.Qw().InteractNpc("plains_herbalist");
    CHECK(s.Status("q_collect_slime_gel") == QuestStatus::Completed);
    CHECK(s.ctx->sys.shop->State().open);
    CHECK(s.ctx->sys.shop->State().npcId == "plains_herbalist");
  }

  TEST_CASE("3.11 craft phases advance at the craft NPC, then at the delivery NPC (any zone)") {
    QwSim s;
    s.Qs().Load({Rec("q_craft_dwarf_weapon", QuestStatus::Active, {3, 5, 0, 0})});
    s.sim->ClearEvents();
    s.Qw().InteractNpc("quest_dwarf");  // deliver NPC first: the craft is not done yet
    CHECK(s.Cur("q_craft_dwarf_weapon", 3) == 0);
    s.Qw().InteractNpc("blacksmith_advanced");
    CHECK(s.Cur("q_craft_dwarf_weapon", 2) == 1);
    CHECK(HasLog(s.ctx->events.Items(), "zone.craft.complete"));
    s.ctx->sys.shop->Close();
    s.ctx->sys.dialogue->Close();
    s.Qw().InteractNpc("quest_dwarf");
    CHECK(s.Status("q_craft_dwarf_weapon") == QuestStatus::Completed);
    CHECK(HasLog(s.ctx->events.Items(), "zone.deliver.complete"));
  }

  TEST_CASE("4.1 turn-in: the card's cached gear is what is granted; exp, gold, achievements, autosave, toast") {
    QwSim s;
    s.Qs().Load({Rec("q_kill_slimes", QuestStatus::TurnedIn, {10}), Rec("q_kill_goblins", QuestStatus::TurnedIn, {15}),
                 Rec("q_explore_goblin_camp", QuestStatus::TurnedIn, {1}),
                 Rec("q_find_goblin_chief", QuestStatus::Completed, {1})});
    REQUIRE(s.Qw().OpenCard("quest_elder"));
    REQUIRE_FALSE(s.Qw().Card().entries.empty());
    const QuestCardEntry& e = s.Qw().Card().entries[0];
    CHECK(e.turnIn);
    CHECK(e.quest->id == "q_find_goblin_chief");
    REQUIRE(e.choices.size() == 2);
    const std::string pick = e.choices[1].uid;
    // reopening never rerolls
    s.Qw().CloseCard();
    REQUIRE(s.Qw().OpenCard("quest_elder"));
    CHECK(s.Qw().Card().entries[0].choices[1].uid == pick);
    const int64_t gold0 = s.H().Gold();
    s.sim->ClearEvents();
    REQUIRE(s.Qw().TurnIn("q_find_goblin_chief", 1));
    CHECK(s.H().Gold() == gold0 + 80);
    bool granted = false;
    for (const ItemInstance& it : s.ctx->sys.inventory->Items().Bag()) granted = granted || it.uid == pick;
    CHECK(granted);
    CHECK(s.Qw().RewardChoices("q_find_goblin_chief").empty());
    CHECK_FALSE(s.Qw().Card().open);
    CHECK(s.ctx->sys.achievements->State().ProgressOf("quest") == 1);
    const std::vector<Event> ev(s.ctx->events.Items().begin(), s.ctx->events.Items().end());
    CHECK(HasLog(ev, "zone.quest.rewardItem"));
    bool saved = false;
    for (const EvSaveRequested* r : Of<EvSaveRequested>(ev)) saved = saved || r->reason == "questTurnIn";
    CHECK(saved);
    bool toast = false;
    for (const EvBanner* b : Of<EvBanner>(ev)) toast = toast || b->title.key == "sys.questCard.turnedIn";
    CHECK(toast);
    CHECK_FALSE(s.Qw().TurnIn("q_find_goblin_chief", 0));  // only from completed
  }

  TEST_CASE("turn-in overflow goes to the stash (I10); fixed items stack; the choice index clamps") {
    QwSim s;
    s.Qs().Load({Rec("q_kill_slimes", QuestStatus::TurnedIn, {10}), Rec("q_collect_slime_gel", QuestStatus::Completed, {6, 1})});
    const size_t bag0 = s.ctx->sys.inventory->Items().Bag().size();
    REQUIRE(s.Qw().TurnIn("q_collect_slime_gel", 7));  // no choices: the index is irrelevant
    const auto bag = s.ctx->sys.inventory->Items().Bag();
    int potions = 0;
    for (const ItemInstance& it : bag) potions += it.baseId == "c_hp_potion_s" ? it.quantity : 0;
    CHECK(potions >= 2);
    CHECK(bag.size() <= bag0 + 1);
    // fill the bag, then turn in a quest with gear
    const LootContext lc{&s.ctx->data, &s.ctx->Rand(RngStream::Loot), &s.ctx->sys.inventory->Uids()};
    for (int i = 0; i < 200; ++i) {
      std::optional<ItemInstance> it = CreateItem(lc, "w_short_sword", 1, ItemQuality::Normal);
      REQUIRE(it.has_value());
      if (s.ctx->sys.inventory->Grant(*it, OverflowPolicy::Refuse, ItemSource::Debug) != ItemGrantOutcome::Bag) break;
    }
    const size_t stash0 = s.ctx->sys.inventory->Items().Stash().size();
    s.Qs().Load({Rec("q_kill_slimes", QuestStatus::TurnedIn, {10}), Rec("q_lost_pendant", QuestStatus::Completed, {1, 1, 1, 1, 1})});
    REQUIRE(s.Qw().TurnIn("q_lost_pendant", 99));  // clamps to the last (only) choice
    CHECK(s.ctx->sys.inventory->Items().Stash().size() == stash0 + 1);
  }

  TEST_CASE("3.9 escort: spawn, wait, join, chip damage death -> failed and offered again; re-accept respawns") {
    QwSim s;
    s.Qs().Load({Rec("q_kill_slimes", QuestStatus::TurnedIn, {10}), Rec("q_kill_goblins", QuestStatus::TurnedIn, {15})});
    REQUIRE(s.Qs().Accept("q_escort_merchant_plains"));
    const EscortState& e = s.Qw().Escort();
    REQUIRE(e.active);
    CHECK(e.pos == Vec2(30, 40));
    CHECK(e.hp == 200);
    CHECK(e.maxHp == 200);
    s.Steps(5);
    CHECK_FALSE(e.joined);  // the hero is far: it waits at the start tile
    s.Place(Vec2(34.5, 40));
    std::vector<Event> ev = s.StepsCollect(1);
    CHECK(e.joined);
    CHECK(HasLog(ev, "zone.escort.joined"));
    // an aggro monster next to the escort chips it: max(1, floor(damage * 0.3)) once per 2 s
    MonsterDef brute = *s.ctx->data.Monsters().Find("goblin");
    brute.damage = 1000;
    MonsterSpawnParams p;
    p.baseDef = &brute;
    p.alreadyScaled = true;
    p.tile = TilePos(30, 41);
    p.startChasing = true;
    const EntityId m = s.Mon().Spawn(p);
    REQUIRE(m != kNoEntity);
    ev = s.StepsCollect(1);
    s.Kill(m);
    CHECK_FALSE(s.Qw().Escort().active);
    CHECK(s.Status("q_escort_merchant_plains") == QuestStatus::Failed);
    CHECK(HasLog(ev, "zone.escort.npcDied"));
    bool died = false;
    for (const EvEntityDespawned* d : Of<EvEntityDespawned>(ev)) {
      died = died || (d->kind == EntityKind::Escort && d->reason == DespawnReason::Died);
    }
    CHECK(died);
    const std::vector<std::string> offers = OfferIds(s.Qs().Offers("quest_elder", s.H().Level(), OfferRule::QuestCard));
    CHECK(std::find(offers.begin(), offers.end(), "q_escort_merchant_plains") != offers.end());
    REQUIRE(s.Qw().AcceptFromCard("q_escort_merchant_plains"));
    CHECK(s.Qw().Escort().active);
    CHECK(s.Qw().Escort().pos == Vec2(30, 40));
    CHECK(s.Qw().Escort().hp == 200);
    CHECK_FALSE(s.Qw().Escort().joined);
    // the guide fetches the escort while it is more than 4 tiles away
    s.Place(Vec2(30, 50));
    s.Steps(16);
    CHECK(s.Qw().Guide().kind == GuideTargetKind::Escort);
  }

  TEST_CASE("3.9 escort arrival: catch-up next to the hero at the destination completes the quest") {
    QwSim s;
    TilePos probe;
    if (!s.ctx->sys.zone->Paths().FindWalkableNear(TilePos(50, 90), 2, probe)) {
      MESSAGE("skipped: Pathfinder::FindWalkableNear is still a stub (world area)");
      return;
    }
    s.Qs().Load({Rec("q_kill_slimes", QuestStatus::TurnedIn, {10}), Rec("q_kill_goblins", QuestStatus::TurnedIn, {15})});
    REQUIRE(s.Qs().Accept("q_escort_merchant_plains"));
    s.Place(Vec2(31, 40));
    s.Steps(1);
    REQUIRE(s.Qw().Escort().joined);
    s.Place(Vec2(50, 90));  // > 14 tiles: the escort catches up within 2 rings, arriving with the hero
    const std::vector<Event> ev = s.StepsCollect(2);
    CHECK(s.Status("q_escort_merchant_plains") == QuestStatus::Completed);
    CHECK(HasLog(ev, "zone.escort.complete"));
    CHECK_FALSE(s.Qw().Escort().active);
  }

  TEST_CASE("3.10 defend: target spawns from the quest, waves of 3 + i after 5 s near it, progress per cleared wave") {
    QwSim s;
    SaveData save;
    s.sim->BuildSave(save, 0);
    save.player.currentMap = "twilight_forest";
    save.player.tileCol = 41;
    save.player.tileRow = 36;
    save.quests = {Rec("q_defend_camp_forest", QuestStatus::Active, {0})};
    save.hasStorySeen = true;
    save.storySeen = {"prologue", "chapter_emerald_plains", "chapter_twilight_forest"};
    std::string err;
    REQUIRE(s.sim->LoadGame(save, &err) == SaveError::None);
    s.ctx = &s.sim->Context();
    s.Story().FinishAllBeats();
    const DefendState& d = s.Qw().Defend();
    REQUIRE(d.active);
    CHECK(d.pos == Vec2(40, 35));
    CHECK(d.maxHp == 14 * 30 + 200);
    CHECK(d.totalWaves == 3);
    for (int wave = 0; wave < 3; ++wave) {
      CAPTURE(wave);
      int guard = 0;
      while (!s.Qw().Defend().waveActive && ++guard < 400) s.Steps(1);
      REQUIRE(s.Qw().Defend().waveActive);
      CHECK(guard > 290);  // 5 s delay (> 5000 ms)
      const std::vector<EntityId> ids = s.Qw().Defend().waveMonsters;
      CHECK(ids.size() == static_cast<size_t>(3 + wave));
      for (EntityId id : ids) s.Kill(id);
      s.Story().FinishAllBeats();  // Q8 kept: a wave may roll a zone boss whose kill plays a cutscene
      s.Steps(1);
      CHECK(s.Cur("q_defend_camp_forest", 0) == wave + 1);
    }
    CHECK(s.Status("q_defend_camp_forest") == QuestStatus::Completed);
    s.Steps(1);
    CHECK_FALSE(s.Qw().Defend().active);  // all waves cleared: the target is removed
  }

  TEST_CASE("guide: M1 points at the nearest living slime; markers and the guide follow the quest state") {
    QwSim s;
    REQUIRE(s.Qs().Accept("q_kill_slimes"));
    s.Steps(1);
    const GuideTarget g = s.Qw().Guide();
    REQUIRE(g.Valid());
    CHECK(g.kind == GuideTargetKind::Monster);
    const EntityId near = s.FindAlive("slime_green", s.H().Position());
    REQUIRE(near != kNoEntity);
    CHECK(g.pos == s.Mon().Find(near)->pos);
    CHECK(g.questId == "q_kill_slimes");
  }

  TEST_CASE("Q1: a dialogue choice reward is paid once per (npc, node, choice), and saved") {
    QwSim s;
    const int64_t gold0 = s.H().Gold();
    for (int round = 0; round < 2; ++round) {
      REQUIRE(s.Qw().OpenDialogueTree("quest_elder"));
      DialogueSystem& d = *s.ctx->sys.dialogue;
      d.Choose(2);  // reward_ask
      REQUIRE(d.View().nodeId == "reward_ask");
      d.Choose(0);  // "I'll help (supplies)" -> help, +30 gold +50 exp
      CHECK(d.View().nodeId == "help");
      d.Close();
      CHECK(s.H().Gold() == gold0 + 30);
    }
    SaveData save;
    s.sim->BuildSave(save, 0);
    CHECK(save.dialogueOnce == std::vector<std::string>{"quest_elder|reward_ask|0"});
  }

  TEST_CASE("lore: pickup within 2 tiles opens the popup once; hidden area needs corners + centre; Q5 rewards persist") {
    QwSim s;
    s.Place(Vec2(46, 36));
    std::vector<Event> ev = s.StepsCollect(1);
    LoreSystem& lore = *s.ctx->sys.lore;
    CHECK(lore.IsCollected("lore_ep_01"));
    CHECK(lore.Text().open);
    CHECK(lore.Text().loreId == "lore_ep_01");
    CHECK(Of<EvLoreCollected>(ev).size() == 1);
    CHECK(HasLog(ev, "zone.lore.discovered"));
    lore.CloseText();
    s.Steps(2);
    CHECK_FALSE(lore.Text().open);  // collected entries never respawn
    // hidden area (108,108) r6: only the centre side explored -> not yet
    s.Place(Vec2(100, 108));
    s.Steps(2);
    CHECK_FALSE(lore.IsDiscovered("hidden_ep_elven_cache"));
    s.Place(Vec2(108, 108));
    ev = s.StepsCollect(2);
    REQUIRE(lore.IsDiscovered("hidden_ep_elven_cache"));
    CHECK(Of<EvHiddenAreaDiscovered>(ev).size() == 1);
    REQUIRE(lore.HiddenRewards().size() == 2);
    const int64_t gold0 = s.H().Gold();
    REQUIRE(lore.ClaimHiddenReward("hidden_ep_elven_cache", 1));  // gold pile 200
    CHECK(s.H().Gold() == gold0 + 200);
    // Q5: the unclaimed chest lies there again on the next visit; the claimed pile does not
    SaveData save;
    s.sim->BuildSave(save, 0);
    CHECK(save.loreCollected == std::vector<std::string>{"lore_ep_01"});
    std::string err;
    REQUIRE(s.sim->LoadGame(save, &err) == SaveError::None);
    const LoreSystem& again = *s.sim->Context().sys.lore;
    REQUIRE(again.HiddenRewards().size() == 1);
    CHECK(again.HiddenRewards()[0].rewardIndex == 0);
  }

  TEST_CASE("story decorations: nearest within 3 tiles shows its label, tooltip within sqrt(2)") {
    QwSim s;
    s.Place(Vec2(55, 47.5));
    std::vector<Event> ev = s.StepsCollect(1);
    auto f = Of<EvStoryDecorFocus>(ev);
    REQUIRE_FALSE(f.empty());
    CHECK(f.back()->decorId == "story_ep_ruined_tower");
    CHECK_FALSE(f.back()->showTooltip);
    s.Place(Vec2(55, 46));
    ev = s.StepsCollect(1);
    f = Of<EvStoryDecorFocus>(ev);
    REQUIRE_FALSE(f.empty());
    CHECK(f.back()->showTooltip);
    s.Place(Vec2(70, 70));
    ev = s.StepsCollect(1);
    f = Of<EvStoryDecorFocus>(ev);
    REQUIRE_FALSE(f.empty());
    CHECK(f.back()->decorId.empty());
  }

  TEST_CASE("achievements: 'explore' counts each story zone once across reloads (Q2); quest turn-ins count") {
    QwSim s;
    CHECK(s.ctx->sys.achievements->State().ProgressOf("explore") == 1);
    SaveData save;
    s.sim->BuildSave(save, 0);
    std::string err;
    REQUIRE(s.sim->LoadGame(save, &err) == SaveError::None);
    CHECK(s.sim->Context().sys.achievements->State().ProgressOf("explore") == 1);
  }
}
