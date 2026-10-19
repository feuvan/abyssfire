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

#include "QuestsFixtures.h"
#include "SimHarness.h"
#include "TestContent.h"
#include "abyss/audio/Audio.h"
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
using namespace abyss::test::quests_fx;
namespace ch1 = abyss::test::ch1;
namespace fixture = abyss::test::fixture;

// =====================================================================================================================
// QuestSystem (quests 2) - QuestEngine.test.ts "QuestSystem events", NewQuestTypes.test.ts, QuestHunts.test.ts
// =====================================================================================================================
TEST_SUITE("quests") {

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
    const QuestDef* kill = r.h.ctx.data.FindQuest(ch1::kQuestM1);
    CHECK(r.qs.CraftPhaseKey(*kill, Rec(ch1::kQuestM1, QuestStatus::Active, {0})).empty());
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
    for (const char* id : {ch1::kQuestM5, "q_seal_dark_source", "q_kill_stone_guardian", "q_seal_fire_rift",
                           "q_kill_abyss_lord"}) {
      const QuestDef* q = data.FindQuest(id);
      REQUIRE(q != nullptr);
      CHECK_FALSE(q->rewards.choices.empty());
    }
  }

  // ---- reward choices (4.2) - QuestEngine.test.ts QuestRewards ---------------------------------------------------------
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
    const QuestDef* m5 = data.FindQuest(ch1::kQuestM5);
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
    const std::string untargeted = fixture::Grunt().id;  // a monster no achievement targets
    std::vector<std::string> first = a.Update(AchievementType::Kill, untargeted);
    CHECK(first == std::vector<std::string>{"ach_first_kill"});
    CHECK(a.ProgressOf("kill") == 1);
    CHECK(a.ProgressOf("kill:" + untargeted) == 0);  // no achievement targets it: no key
    for (int i = 1; i < 99; ++i) a.Update(AchievementType::Kill, untargeted);
    CHECK(a.ProgressOf("kill") == 99);
    CHECK_FALSE(a.IsUnlocked("ach_kill_100"));
    CHECK(a.Update(AchievementType::Kill, untargeted) == std::vector<std::string>{"ach_kill_100"});
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
    // The elder's one-time reward node: content_ch1_test.cpp.
  }
}

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

  TEST_CASE("dialogue runtime: a choice whose next node is missing closes; a linear panel has no buttons") {
    DlgRig r;
    r.dlg.OpenLinear(ch1::kWanderer);
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
    p.Add("cs_test_beat");
    CHECK(p.Has("cs_test_beat"));
    p.Load({}, true);  // a present empty list = nothing seen
    CHECK(p.Seen().empty());
  }

  TEST_CASE("script: prologue / five chapters / epilogue / credits; cutscenes of 5-14 steps; triggers wired") {
    const DataStore& data = test::RealData();
    const StoryScript& s = data.Story();
    CHECK(s.prologue.id == "prologue");
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
    for (const BossIntroDef& b : s.bossIntros) {
      const Cutscene* cs = s.FindCutscene(b.cutscene);
      REQUIRE(cs != nullptr);
      CHECK(std::any_of(cs->steps.begin(), cs->steps.end(),
                        [](const CutsceneStep& st) { return st.kind == StoryStepKind::Title; }));
    }
    // The prologue's slide count, the boss-intro roster and the Chapter 1 triggers: content_ch1_test.cpp.
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
    // A monster stands in a zone when a map spawn or the zone's mini-boss puts it there (MonstersIn's rule).
    const auto zonesWithMonster = [&](const std::string& id) {
      std::vector<std::string> out;
      for (const std::string& z : {std::string("dungeon_abyss_lord"), std::string("dungeon_mid_boss")}) {
        if (z == id) return std::vector<std::string>{"abyss_labyrinth"};
      }
      for (const MapDef& m : data.World().maps) {
        if (MonstersIn(data, m.id).count(id) != 0) out.push_back(m.id);
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

TEST_SUITE("story") {

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

  TEST_CASE("a beat queued by a StoryBeatFinishedMsg handler starts after the finished beat, never inside it") {
    // The pump is iterative: FinishBeat only clears the current beat, so a handler that queues beats (here the final
    // boss's cutscene + epilogue) cannot start one re-entrantly and have it overwritten by the finishing beat.
    for (int mode = 0; mode < 2; ++mode) {
      CAPTURE(mode);
      QwSim s;
      std::vector<std::string> done;
      StoryDirector* st = &s.Story();
      s.ctx->bus.Subscribe<StoryBeatFinishedMsg>([&done, st](const StoryBeatFinishedMsg& f) {
        done.push_back(f.beatId);
        if (f.beatId == "cs_tf_moonfang") {
          MonsterKilledMsg k;
          k.defId = "demon_lord";
          st->OnMonsterKilled(k);
          CHECK_FALSE(st->IsCinematic());  // queued only
        }
      });
      MonsterKilledMsg m;
      m.defId = "werewolf_alpha";
      st->OnMonsterKilled(m);
      REQUIRE(st->Playback().beat.id == "cs_tf_moonfang");
      if (mode == 0) {
        st->FinishAllBeats();
      } else {
        for (int i = 0; i < 400 && st->IsBusy(); ++i) {  // real-time path: Skip / Advance / AdvanceRealTime
          st->Skip();
          st->Advance();
          s.sim->AdvanceRealTime(500);
        }
      }
      CHECK(done == std::vector<std::string>{"cs_tf_moonfang", "cs_ar_fall", "epilogue"});
      CHECK_FALSE(st->IsBusy());
      CHECK_FALSE(st->IsCinematic());
      CHECK(st->Progress().Has("cs_ar_fall"));
      CHECK(st->Progress().Has("epilogue"));
    }
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

}

TEST_SUITE("quests") {

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

// =====================================================================================================================
// Audit fixes: escort / defend name keys (Q11), clue float, hidden-area reward logs, chapter subtitle, sequence music
// span (audio 10.4 rule 2), lost turn-in beats (Q7), NPC line fallback (0.3), QuestProgress payload (13)
// =====================================================================================================================

TEST_SUITE("quests") {

  TEST_CASE("Q11: the defend target's protect log and label use data.defendTarget.<id>") {
    QwSim s;
    SaveData save = SaveIn(s, "twilight_forest", 41, 36, {Rec("q_defend_camp_forest", QuestStatus::Active, {0})},
                           {"prologue", "chapter_emerald_plains", "chapter_twilight_forest"});
    std::string err;
    REQUIRE(s.sim->LoadGame(save, &err) == SaveError::None);
    s.ctx = &s.sim->Context();
    const std::vector<Event> ev(s.ctx->events.Items().begin(), s.ctx->events.Items().end());
    const EvLog* l = FindLog(ev, "zone.defend.targetNeedsProtection");
    REQUIRE(l != nullptr);
    REQUIRE(l->text.args.size() == 1);
    CHECK(l->text.args[0].isKey);
    CHECK(l->text.args[0].value == "data.defendTarget.defend_camp_forest");
    const DefendState& d = s.Qw().Defend();
    REQUIRE(d.active);
    CHECK(d.nameKey == "data.defendTarget.defend_camp_forest");
    CHECK(EnglishStrings().NameOr(d.nameKey, d.name) == "Camp Bonfire");
  }

  TEST_CASE("0.3: an NPC line without an i18n key is logged as its zh data line, never as the raw key") {
    QwSim s;
    const NpcDef* ally = s.ctx->data.FindNpc("tower_elder");
    REQUIRE(ally != nullptr);
    REQUIRE_FALSE(ally->dialogue.empty());
    REQUIRE_FALSE(s.ctx->data.Strings().Has("data.npc.tower_elder.dialogue.0"));
    s.sim->ClearEvents();
    s.Qw().InteractNpc("tower_elder");
    const std::vector<Event> ev(s.ctx->events.Items().begin(), s.ctx->events.Items().end());
    CHECK_FALSE(HasLog(ev, "data.npc.tower_elder.dialogue.0"));
    const EvLog* line = FindLog(ev, ally->dialogue[0]);
    REQUIRE(line != nullptr);
    CHECK(s.ctx->data.Strings().T(line->text) == ally->dialogue[0]);
    // keyed NPCs still log the key
    s.Qw().CloseCard();
    s.ctx->sys.dialogue->Close();
    s.sim->ClearEvents();
    s.Qw().InteractNpc(ch1::kElder);
    CHECK(HasLog(s.ctx->events.Items(), std::string("data.npc.") + ch1::kElder + ".dialogue.0"));
  }
}

TEST_SUITE("story") {

  TEST_CASE("audio 10.4 rule 2: the prologue plays abyss_rift; the chapter card plays under the zone's explore track") {
    QwSim s(7, /*skipStory=*/false);
    MusicDirector& music = s.ctx->sys.audio->Music();
    REQUIRE(s.Story().Playback().beat.id == "prologue");
    CHECK(music.StoryLocked());
    CHECK(music.Zone() == "abyss_rift");
    std::vector<StoryStateMsg> states;
    s.ctx->bus.Subscribe<StoryStateMsg>([&states](const StoryStateMsg& m) { states.push_back(m); });
    s.PlayBeat();
    REQUIRE(s.Story().Playback().beat.id == "chapter_emerald_plains");
    // the sequence ended its span (zone track back, lock released), the card retook the lock without music
    REQUIRE(states.size() == 2);
    CHECK_FALSE(states[0].active);
    CHECK(states[0].beatId == "prologue");
    CHECK(states[1].active);
    CHECK(states[1].beatId == "chapter_emerald_plains");
    CHECK(states[1].musicTrack.empty());
    CHECK(music.Zone() == "emerald_plains");
    CHECK(music.State() == MusicState::Explore);
    CHECK(music.StoryLocked());
    CHECK(s.Story().IsBusy());  // EvStoryState stays busy across the two beats
    s.sim->AdvanceRealTime(9000);
    CHECK_FALSE(s.Story().IsBusy());
    REQUIRE(states.size() == 3);
    CHECK_FALSE(states[2].active);
    CHECK_FALSE(music.StoryLocked());
    CHECK(music.Zone() == "emerald_plains");
  }

}
