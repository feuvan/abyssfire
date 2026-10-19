// Chapter 1 content pins (implementation-plan.md 8 "T0", work package 0B): every assertion of the core tests that
// names Chapter 1 content - the emerald_plains quests, NPCs, monsters, hunts, beats, dialogue paths, lore, coordinates,
// exp / gold / ember totals and the generated map's golden facts - lives here, moved verbatim from the area test files
// (each keeps its original doctest suite, so `ctest -R core.<area>` still runs it). The content switch (C1) re-pins
// this file, the `ch1` block of TestContent.h and playthrough_test.cpp; the area files test systems through
// TestContent.h's handles and synthetic fixtures and do not change.
//
// The last section checks the layer itself: the TestContent.h handles name the roles they claim, the synthetic fixtures
// were cut from today's roster, and no other test file spells a Chapter 1 content id.
#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "ItemsFixtures.h"
#include "MonstersFixtures.h"
#include "PetsFixtures.h"
#include "QuestsFixtures.h"
#include "SimFixtures.h"
#include "SimHarness.h"
#include "TestContent.h"
#include "TestUtil.h"
#include "WorldFixtures.h"
#include "abyss/data/DataStore.h"
#include "doctest/doctest.h"

using namespace abyss;

// =====================================================================================================================
// Monsters: the Chapter 1 roster, hunts, mini-boss, story boss, zone population (from monsters_test.cpp)
// Moved verbatim; the systems tests there run on the synthetic fixtures of TestContent.h.
// =====================================================================================================================

TEST_SUITE("monsters") {
  using namespace abyss::test::monsters_fx;

  TEST_CASE("Chapter 1 roster 2.1: base stats, speed in tiles/s, ranged rule (M5 shaman ranged at 3.0)") {
    struct Row {
      const char* id;
      int32_t level;
      double hp, dmg, def, speed, aggro, range, atkSpeed, exp, g0, g1;
      bool elite;
      double tilesPerSec;
    };
    const Row rows[] = {
        {"slime_green", 1, 30, 5, 2, 40, 4, 1.2, 1500, 12, 2, 4, false, 1.20},
        {"goblin", 3, 55, 8, 4, 55, 5, 1.5, 1200, 18, 3, 6, false, 1.65},
        {"goblin_chief", 5, 160, 14, 8, 50, 6, 1.8, 1000, 55, 8, 15, true, 1.50},
        {"miniboss_goblin_shaman", 6, 280, 16, 10, 45, 7, 3.0, 1300, 90, 15, 30, true, 1.35},
    };
    for (const Row& r : rows) {
      CAPTURE(r.id);
      const MonsterDef& d = MDef(r.id);
      CHECK(d.level == r.level);
      CHECK(d.hp == r.hp);
      CHECK(d.damage == r.dmg);
      CHECK(d.defense == r.def);
      CHECK(d.speed == r.speed);
      CHECK(d.aggroRange == r.aggro);
      CHECK(d.attackRange == r.range);
      CHECK(d.attackSpeedMs == r.atkSpeed);
      CHECK(d.expReward == r.exp);
      CHECK(d.goldMin == r.g0);
      CHECK(d.goldMax == r.g1);
      CHECK(d.elite == r.elite);
      CHECK(MonsterTopSpeed(d, MAi()) == doctest::Approx(r.tilesPerSec));
      CHECK(d.nameKey == "data.monster." + std::string(r.id));
    }
    CHECK(MDef("miniboss_goblin_shaman").isMiniBoss);
    CHECK(MDef("miniboss_goblin_shaman").isRanged);  // M5
    CHECK_FALSE(MDef("goblin_chief").isRanged);
    // Ranged iff attackRange > 2.5 for every definition (vector 11: 2.5 is melee).
    for (const MonsterDef& d : MD().Monsters().defs) {
      CAPTURE(d.id);
      CHECK(d.isRanged == (d.attackRange > MAi().rangedThreshold));
    }
    CHECK_FALSE(MDef("mountain_troll").isRanged);  // exactly 2.5
    CHECK(MDef("fire_elemental").isRanged);
  }

  TEST_CASE("derived per-instance numbers 2.2: stats, chase drop, attack exit, melee reach") {
    struct Row {
      const char* id;
      int32_t str, dex, vit;
      double chaseDrop, attackExit, reach;
    };
    const Row rows[] = {
        {"slime_green", 4, 4, 3, 6.0, 1.44, 2.12},
        {"goblin", 6, 5, 5, 7.5, 1.8, 2.525},
        {"goblin_chief", 11, 5, 16, 9.0, 2.16, 2.93},
        {"miniboss_goblin_shaman", 12, 4, 28, 10.5, 3.6, 4.55},  // M5: range 3.0 (web 2.5: 3.0 / 3.875)
        {"hunt_pendant_thief", 8, 5, 16, 10.5, 1.8, 2.525},
        {"hunt_redcap_gruk", 10, 5, 27, 10.5, 1.8, 2.525},
    };
    for (const Row& r : rows) {
      CAPTURE(r.id);
      const MonsterDef d = MNormalDef(r.id);
      const PrimaryStats s = MonsterBaseStats(d, MAi());
      CHECK(s.str == r.str);
      CHECK(s.dex == r.dex);
      CHECK(s.vit == r.vit);
      CHECK(s.int_ == 3);
      CHECK(s.spi == 3);
      CHECK(s.lck == 3);
      CHECK(d.aggroRange * MAi().chaseDropMul == doctest::Approx(r.chaseDrop));
      CHECK(d.attackRange * MAi().attackExitMul == doctest::Approx(r.attackExit));
      CHECK(d.attackRange * MAi().meleeReachMul + MAi().meleeReachAdd == doctest::Approx(r.reach));
    }
  }

  TEST_CASE("difficulty variants 2.3 (vector 17): Chapter 1 roster on nightmare and hell") {
    struct Row {
      const char* id;
      Difficulty diff;
      double hp, dmg, def, exp, g0, g1;
    };
    const Row rows[] = {
        {"slime_green", Difficulty::Nightmare, 45, 8, 3, 24, 4, 8},
        {"slime_green", Difficulty::Hell, 60, 10, 3, 36, 6, 12},
        {"goblin", Difficulty::Nightmare, 83, 12, 5, 36, 6, 12},
        {"goblin", Difficulty::Hell, 110, 16, 6, 54, 9, 18},
        {"goblin_chief", Difficulty::Nightmare, 240, 21, 10, 110, 16, 30},
        {"goblin_chief", Difficulty::Hell, 320, 28, 13, 165, 24, 45},
        {"miniboss_goblin_shaman", Difficulty::Nightmare, 420, 24, 13, 180, 30, 60},
        {"miniboss_goblin_shaman", Difficulty::Hell, 560, 32, 16, 270, 45, 90},
        {"hunt_pendant_thief", Difficulty::Nightmare, 248, 15, 7, 108, 18, 36},
        {"hunt_pendant_thief", Difficulty::Hell, 330, 20, 8, 162, 27, 54},
        {"hunt_redcap_gruk", Difficulty::Nightmare, 413, 20, 7, 180, 18, 36},
        {"hunt_redcap_gruk", Difficulty::Hell, 550, 26, 8, 270, 27, 54},
    };
    for (const Row& r : rows) {
      CAPTURE(r.id);
      CAPTURE(EnumName(r.diff));
      const MonsterDef d = ScaleMonsterForDifficulty(MNormalDef(r.id), MDiff(), r.diff);
      CHECK(d.hp == r.hp);
      CHECK(d.damage == r.dmg);
      CHECK(d.defense == r.def);
      CHECK(d.expReward == r.exp);
      CHECK(d.goldMin == r.g0);
      CHECK(d.goldMax == r.g1);
    }
  }

  TEST_CASE("makeHuntDefinition (vector 12): Gruk and Sneek; every exported hunt matches the TS defsNormal") {
    const MonsterDef& goblin = MDef("goblin");
    const MonsterDef gruk = MakeHuntDefinition(goblin, MHunt("hunt_redcap_gruk"), MAi(), "Gruk");
    CHECK(gruk.id == "hunt_redcap_gruk");
    CHECK(gruk.name == "Gruk");
    CHECK(gruk.nameKey == "data.monster.hunt_redcap_gruk");
    CHECK(gruk.hp == 275);
    CHECK(gruk.damage == 13);
    CHECK(gruk.defense == 5);
    CHECK(gruk.expReward == 90);
    CHECK(gruk.goldMin == 9);
    CHECK(gruk.goldMax == 18);
    CHECK(gruk.aggroRange == 7);
    CHECK(gruk.elite);
    CHECK(gruk.isMiniBoss);
    CHECK(gruk.spriteKey == goblin.spriteKey);
    CHECK(gruk.speed == goblin.speed);
    CHECK(gruk.attackRange == goblin.attackRange);
    CHECK(gruk.attackSpeedMs == goblin.attackSpeedMs);
    CHECK(gruk.level == goblin.level);
    const MonsterDef sneek = MakeHuntDefinition(goblin, MHunt("hunt_pendant_thief"), MAi(), "Sneek");
    CHECK(sneek.hp == 165);
    CHECK(sneek.damage == 10);
    CHECK(sneek.defense == 5);
    CHECK(sneek.expReward == 54);
    CHECK(ScaleMonsterForDifficulty(sneek, MDiff(), Difficulty::Nightmare).hp == 248);
    // Defaults: hpMul 4, dmgMul 1.5 when the hunt omits them.
    HuntDef bare = MHunt("hunt_redcap_gruk");
    bare.hasHpMul = bare.hasDmgMul = false;
    const MonsterDef def = MakeHuntDefinition(goblin, bare, MAi(), "x");
    CHECK(def.hp == 220);
    CHECK(def.damage == 12);
    CHECK(def.expReward == 72);
    // Cross-check every hunt of every zone against the exporter's TS-computed definition.
    for (const HuntDef& h : MD().Monsters().hunts) {
      CAPTURE(h.huntId);
      const MonsterDef* base = MD().Monsters().FindForZone(h.zone, h.monsterId);
      REQUIRE(base != nullptr);
      const MonsterDef d = MakeHuntDefinition(*base, h, MAi(), h.name);
      CHECK(d.hp == h.defNormal.hp);
      CHECK(d.damage == h.defNormal.damage);
      CHECK(d.defense == h.defNormal.defense);
      CHECK(d.expReward == h.defNormal.expReward);
      CHECK(d.goldMin == h.defNormal.goldMin);
      CHECK(d.goldMax == h.defNormal.goldMax);
      CHECK(d.aggroRange == h.defNormal.aggroRange);
      CHECK(d.speed == h.defNormal.speed);
      CHECK(d.elite == h.defNormal.elite);
      CHECK(d.isMiniBoss == h.defNormal.isMiniBoss);
    }
  }

  TEST_CASE("isHuntDue (vector 13): bounty on accept, tracked quarry after its trail, done / inactive -> not due") {
    const QuestDef* pendant = MD().FindQuest("q_lost_pendant");
    const QuestDef* bounty = MD().FindQuest("q_bandit_trouble");
    REQUIRE(pendant != nullptr);
    REQUIRE(bounty != nullptr);
    auto progressOf = [](const QuestDef& q, std::initializer_list<int32_t> done, QuestStatus st = QuestStatus::Active) {
      QuestProgress p;
      p.questId = q.id;
      p.status = st;
      p.objectives.assign(q.objectives.size(), 0);
      for (int32_t i : done) p.objectives[static_cast<size_t>(i)] = q.objectives[static_cast<size_t>(i)].required;
      return p;
    };
    const HuntDef& gruk = MHunt("hunt_redcap_gruk");
    const HuntDef& thief = MHunt("hunt_pendant_thief");
    CHECK(IsHuntDue(*bounty, progressOf(*bounty, {}), gruk));
    CHECK_FALSE(IsHuntDue(*bounty, progressOf(*bounty, {0}), gruk));
    CHECK_FALSE(IsHuntDue(*bounty, progressOf(*bounty, {}, QuestStatus::Completed), gruk));
    CHECK_FALSE(IsHuntDue(*pendant, progressOf(*pendant, {0, 1}), thief));
    CHECK(IsHuntDue(*pendant, progressOf(*pendant, {0, 1, 2}), thief));
    CHECK_FALSE(IsHuntDue(*pendant, progressOf(*pendant, {0, 1, 2, 3}), thief));  // kill objective done
    CHECK(HuntObjectiveIndex(*pendant, "hunt_pendant_thief") == 3);
    CHECK(HuntObjectiveIndex(*pendant, "no_such_hunt") == -1);
    // A short (migrated) progress record counts missing objectives as 0.
    QuestProgress shortRec;
    shortRec.questId = bounty->id;
    CHECK(IsHuntDue(*bounty, shortRec, gruk));

    // huntsToSpawn: this zone only, not already out, quest order then hunt order.
    const QuestProgress pp = progressOf(*pendant, {0, 1, 2});
    const QuestProgress bp = progressOf(*bounty, {});
    const std::vector<std::pair<const QuestDef*, const QuestProgress*>> open{{pendant, &pp}, {bounty, &bp}};
    std::vector<std::string> present;
    std::vector<DueHunt> due = HuntsToSpawn(open, "emerald_plains", present, MD().Monsters().hunts);
    REQUIRE(due.size() == 2);
    CHECK(due[0].hunt->huntId == "hunt_pendant_thief");
    CHECK(due[1].hunt->huntId == "hunt_redcap_gruk");
    CHECK(due[0].quest == pendant);
    present = {"hunt_redcap_gruk"};
    due = HuntsToSpawn(open, "emerald_plains", present, MD().Monsters().hunts);
    REQUIRE(due.size() == 1);
    CHECK(due[0].hunt->huntId == "hunt_pendant_thief");
    CHECK(HuntsToSpawn(open, "twilight_forest", {}, MD().Monsters().hunts).empty());
  }

  TEST_CASE("elite affix application 2.4 (vector 16): every affix on the chief, shaman and both hunt leaders") {
    struct Row {
      const char* id;
      std::array<std::array<int32_t, 5>, 7> v;  // hp, dmg, speed, def, str per affix in table order
    };
    const Row rows[] = {
        {"goblin_chief",
         {{{192, 14, 50, 8, 11}, {160, 14, 80, 8, 11}, {184, 15, 50, 8, 12}, {208, 18, 50, 9, 14}, {192, 15, 50, 8, 12},
           {200, 15, 50, 8, 12}, {192, 14, 45, 8, 11}}}},
        {"miniboss_goblin_shaman",
         {{{336, 16, 45, 10, 12}, {280, 16, 72, 10, 12}, {322, 17, 45, 10, 13}, {364, 21, 45, 11, 16},
           {336, 17, 45, 10, 13}, {350, 17, 45, 10, 13}, {336, 16, 40, 11, 12}}}},
        {"hunt_pendant_thief",
         {{{198, 10, 55, 5, 8}, {165, 10, 88, 5, 8}, {189, 11, 55, 5, 8}, {214, 13, 55, 5, 10}, {198, 11, 55, 5, 8},
           {206, 11, 55, 5, 8}, {198, 10, 49, 5, 8}}}},
        {"hunt_redcap_gruk",
         {{{330, 13, 55, 5, 10}, {275, 13, 88, 5, 10}, {316, 14, 55, 5, 11}, {357, 17, 55, 5, 13},
           {330, 14, 55, 5, 11}, {343, 14, 55, 5, 11}, {330, 13, 49, 5, 10}}}},
    };
    for (const Row& r : rows) {
      const MonsterDef base = MNormalDef(r.id);
      for (size_t i = 0; i < 7; ++i) {
        const EliteAffixType ty = MAffixes().order[i];
        CAPTURE(r.id);
        CAPTURE(EnumName(ty));
        MonsterInstance m = MMake(base, {5, 5});
        const int32_t dexBefore = m.stats.dex;
        ApplyEliteAffixes(m, MOne(ty), MAffixes(), MAi());
        CHECK(m.maxHp == r.v[i][0]);
        CHECK(m.hp == r.v[i][0]);
        CHECK(m.def.damage == r.v[i][1]);
        CHECK(m.def.speed == r.v[i][2]);
        CHECK(m.def.defense == r.v[i][3]);
        CHECK(m.stats.str == r.v[i][4]);
        CHECK(m.stats.dex == dexBefore);  // Q18 parity: dex not recomputed
      }
    }
  }

  TEST_CASE("M7 story boss is data: id, quests and the storyBoss role") {
    const MonsterAiDef& ai = MAi();
    CHECK(ai.storyBossId == "goblin_chief");
    CHECK(ai.storyBossNotAfterQuestTurnIn == "q_find_goblin_chief");
    CHECK(ai.chapterCompleteQuest == "q_secure_plains");
    CHECK(ai.storyBossOncePerVisit);
    CHECK(ai.storyBossFarmableAfterChapter);
    CHECK(std::find(ai.noRespawnRoles.begin(), ai.noRespawnRoles.end(), "storyBoss") != ai.noRespawnRoles.end());
    MonsterRole r{};
    CHECK(ParseEnum("storyBoss", r));
    CHECK(r == MonsterRole::StoryBoss);
    const QuestDef* q = MD().FindQuest(ai.storyBossNotAfterQuestTurnIn);
    REQUIRE(q != nullptr);
    CHECK(q->questArea.col == 15);
    CHECK(q->questArea.row == 95);
  }

  TEST_CASE("zone population 6.1 / 6.3: 55 monsters, jitter <= 3, walkable, outside camps, chief = story boss") {
    MonsterRig rig(11);
    REQUIRE(rig.ok);
    rig.monsters.SpawnZonePopulation();
    const MapDef& map = rig.zone.Map();
    int32_t total = 0;
    for (const MapSpawnDef& s : map.spawns) total += s.count;
    CHECK(total == 55);
    const auto all = rig.monsters.All();
    CHECK(static_cast<int32_t>(all.size()) == total);  // M10: 8 tries then the (walkable) anchor
    std::map<std::string, int32_t> perDef;
    for (const MonsterInstance& m : all) {
      CAPTURE(m.def.id);
      ++perDef[m.def.id];
      CHECK(m.IsAlive());
      CHECK(m.state == MonsterState::Idle);
      CHECK(rig.zone.Walkable(m.spawnAnchor.col, m.spawnAnchor.row));
      CHECK(m.pos == m.spawnAnchor.Center());
      // Within +-3 of its spawn entry anchor (or on it).
      bool nearEntry = false;
      for (const MapSpawnDef& s : map.spawns) {
        if (s.monsterId == m.def.id && std::abs(s.pos.col - m.spawnAnchor.col) <= 3 &&
            std::abs(s.pos.row - m.spawnAnchor.row) <= 3) {
          nearEntry = true;
        }
      }
      CHECK(nearEntry);
      CHECK_FALSE(rig.zone.InSafeZone(m.pos));
      if (m.def.id == "goblin_chief") {
        CHECK(m.role == MonsterRole::StoryBoss);
        CHECK(m.noRespawn);
        CHECK(m.AffixCount() == 1);  // emerald_plains: exactly one affix
      } else {
        CHECK(m.role == MonsterRole::Regular);
        CHECK_FALSE(m.noRespawn);
        CHECK(m.affixes.empty());
        CHECK(m.maxHp == MDef(m.def.id).hp);
      }
    }
    CHECK(perDef["slime_green"] == 19);
    CHECK(perDef["goblin"] == 35);
    CHECK(perDef["goblin_chief"] == 1);
    CHECK(MEvents<EvEntitySpawned>(rig.h.events).size() == all.size());
    // Deterministic for a seed.
    MonsterRig again(11);
    again.monsters.SpawnZonePopulation();
    REQUIRE(again.monsters.All().size() == all.size());
    for (size_t i = 0; i < all.size(); ++i) {
      CHECK(again.monsters.All()[i].pos == all[i].pos);
      CHECK(again.monsters.All()[i].def.id == all[i].def.id);
    }
    // Nightmare spawns scaled definitions.
    MonsterRig nm(11);
    nm.h.session.difficulty = Difficulty::Nightmare;
    nm.monsters.SpawnZonePopulation();
    for (const MonsterInstance& m : nm.monsters.All()) {
      if (m.def.id == "goblin") CHECK(m.maxHp == 83);
    }
  }

  TEST_CASE("M7 story boss: skipped after q_find_goblin_chief, back once the chapter is complete") {
    MonsterRig rig(3);
    QuestProgress chief;
    chief.questId = "q_find_goblin_chief";
    chief.status = QuestStatus::TurnedIn;
    chief.objectives.assign(MD().FindQuest(chief.questId)->objectives.size(), 1);
    rig.quests.Load({chief});
    if (!rig.quests.IsTurnedIn("q_find_goblin_chief")) {
      MESSAGE("QuestSystem::Load is not implemented yet (quests area): M7 runtime part skipped");
      return;
    }
    rig.monsters.SpawnZonePopulation();
    CHECK(rig.monsters.NearestAliveOfDef("goblin_chief", {15, 95}) == kNoEntity);
    QuestProgress fin;
    fin.questId = "q_secure_plains";
    fin.status = QuestStatus::TurnedIn;
    fin.objectives.assign(MD().FindQuest(fin.questId)->objectives.size(), 1);
    rig.quests.Load({chief, fin});
    rig.monsters.OnZoneExit();
    rig.monsters.SpawnZonePopulation();
    const EntityId boss = rig.monsters.NearestAliveOfDef("goblin_chief", {15, 95});
    REQUIRE(boss != kNoEntity);
    CHECK(rig.M(boss).role == MonsterRole::StoryBoss);
  }

  TEST_CASE("mini-boss 8.2: (60,55), elite with one affix, once per visit, back on the next visit") {
    MonsterRig rig(5);
    rig.monsters.SpawnZonePopulation();
    rig.monsters.SpawnMiniBoss();
    const EntityId boss = rig.monsters.MiniBoss();
    REQUIRE(boss != kNoEntity);
    const MonsterInstance& b = rig.M(boss);
    CHECK(b.def.id == "miniboss_goblin_shaman");
    CHECK(b.pos == Vec2(60, 55));
    CHECK(b.role == MonsterRole::ZoneMiniBoss);
    CHECK(b.noRespawn);
    CHECK(b.AffixCount() == 1);
    CHECK(b.def.isMiniBoss);
    const size_t count = rig.monsters.All().size();
    rig.monsters.SpawnMiniBoss();  // M7: once per visit
    CHECK(rig.monsters.All().size() == count);
    CHECK(rig.monsters.MiniBoss() == boss);
    rig.monsters.OnZoneExit();
    CHECK(rig.monsters.All().empty());
    CHECK(rig.monsters.MiniBoss() == kNoEntity);
    rig.monsters.SpawnZonePopulation();
    rig.monsters.SpawnMiniBoss();
    CHECK(rig.monsters.MiniBoss() != kNoEntity);
    CHECK(rig.monsters.MiniBoss() != boss);  // fresh id
  }

  TEST_CASE("mini-boss dialogue 8.3: within aggro, once per save, boss frozen while open, chase on dismiss") {
    MonsterRig rig(5, Vec2(60, 70));
    rig.OpenWorld();
    rig.monsters.SpawnMiniBoss();
    const EntityId boss = rig.monsters.MiniBoss();
    REQUIRE(boss != kNoEntity);
    rig.h.events.Clear();
    rig.Step();
    CHECK_FALSE(rig.monsters.MiniBossDialogueActive());  // 15 tiles away
    rig.hero->SetPosition(Vec2(60, 62.01));               // 7.01 > 7
    rig.Step();
    CHECK_FALSE(rig.monsters.MiniBossDialogueActive());
    rig.hero->SetPosition(Vec2(60, 62));  // exactly 7: dSq 49 is not > 49
    rig.Step();
    REQUIRE(rig.monsters.MiniBossDialogueActive());
    const std::vector<EvMiniBossDialogue> opened = MEvents<EvMiniBossDialogue>(rig.h.events);
    REQUIRE(opened.size() == 1);
    CHECK(opened[0].opened);
    CHECK(opened[0].monster == boss);
    CHECK(opened[0].monsterId == "miniboss_goblin_shaman");
    CHECK(rig.M(boss).state == MonsterState::Idle);
    const Vec2 frozenAt = rig.M(boss).pos;
    rig.hero->SetPosition(Vec2(60, 57));
    rig.Step(30);
    CHECK(rig.M(boss).state == MonsterState::Idle);  // the AI holds the boss
    CHECK(rig.M(boss).pos == frozenAt);
    SaveData save;
    rig.monsters.WriteSave(save);
    CHECK(save.miniBossDialogueSeen == std::vector<std::string>{"miniboss_goblin_shaman"});
    rig.h.events.Clear();
    rig.monsters.DismissMiniBossDialogue();
    CHECK_FALSE(rig.monsters.MiniBossDialogueActive());
    CHECK(rig.M(boss).state == MonsterState::Chase);
    REQUIRE(MEvents<EvMiniBossDialogue>(rig.h.events).size() == 1);
    CHECK_FALSE(MEvents<EvMiniBossDialogue>(rig.h.events)[0].opened);
    CHECK_FALSE(rig.aggros.empty());
    rig.Step(5);
    CHECK_FALSE(rig.monsters.MiniBossDialogueActive());  // seen: never again this save
    // A loaded save that has seen it: no dialogue on the next visit either.
    MonsterRig other(6, Vec2(60, 58));
    other.OpenWorld();
    other.monsters.ReadSave(save);
    other.monsters.SpawnMiniBoss();
    other.Step();
    CHECK_FALSE(other.monsters.MiniBossDialogueActive());
    // A dead hero never opens it.
    MonsterRig dead(6, Vec2(60, 58));
    dead.OpenWorld();
    dead.monsters.SpawnMiniBoss();
    dead.hero->SetHp(0);
    dead.Step();
    CHECK_FALSE(dead.monsters.MiniBossDialogueActive());
  }

  TEST_CASE("hunts 9.4: leader (hunt def, difficulty, affixes, x1.25), minions without affixes, announce") {
    MonsterRig rig(21, Vec2(60, 60));
    rig.OpenWorld();
    const QuestDef* pendant = MD().FindQuest("q_lost_pendant");
    const QuestDef* bounty = MD().FindQuest("q_bandit_trouble");
    const std::vector<DueHunt> due{{pendant, &MHunt("hunt_pendant_thief")}, {bounty, &MHunt("hunt_redcap_gruk")}};
    rig.monsters.SpawnHunts(due, true);
    CHECK(rig.monsters.IsHuntPresent("hunt_pendant_thief"));
    CHECK(rig.monsters.IsHuntPresent("hunt_redcap_gruk"));
    const auto all = rig.monsters.All();
    REQUIRE(all.size() == 1 + 2 + 1 + 4);  // open world: every minion placement succeeds
    const MonsterInstance& thief = all[0];
    CHECK(thief.def.id == "hunt_pendant_thief");
    CHECK(thief.huntId == "hunt_pendant_thief");
    CHECK(thief.role == MonsterRole::HuntLeader);
    CHECK(thief.noRespawn);
    CHECK(thief.pos == Vec2(60, 48));
    CHECK(thief.visualScale == doctest::Approx(1.25));
    CHECK(thief.AffixCount() == 1);
    CHECK(thief.originalDef.hp == 165);
    CHECK(thief.def.nameKey == "data.monster.hunt_pendant_thief");
    for (size_t i = 1; i <= 2; ++i) {
      CHECK(all[i].def.id == "goblin");
      CHECK(all[i].role == MonsterRole::HuntMinion);
      CHECK(all[i].affixes.empty());
      CHECK(std::abs(all[i].spawnAnchor.col - 60) <= 3);
      CHECK(std::abs(all[i].spawnAnchor.row - 48) <= 3);
    }
    CHECK(all[3].def.id == "hunt_redcap_gruk");
    CHECK(all[3].pos == Vec2(82, 72));
    CHECK(all[3].originalDef.hp == 275);
    // Announce: one log + shake + reveal event per hunt.
    size_t logs = 0;
    for (const EvLog& l : MEvents<EvLog>(rig.h.events)) {
      if (l.text.key == "zone.quest.huntRevealed") {
        ++logs;
        REQUIRE(l.text.args.size() == 1);
        CHECK(l.text.args[0].isKey);
      }
    }
    CHECK(logs == 2);
    const std::vector<EvCameraShake> shakes = MEvents<EvCameraShake>(rig.h.events);
    REQUIRE(shakes.size() == 2);
    CHECK(shakes[0].durationMs == 260);
    CHECK(shakes[0].intensity == doctest::Approx(0.004));
    CHECK(MEvents<EvHuntRevealed>(rig.h.events).size() == 2);
    // Kill credit uses the hunt id; leaders never respawn; a dead leader is no longer present.
    rig.monsters.ApplyDamage(thief.id, 1e6, DamageFlags{});
    REQUIRE(rig.kills.size() == 1);
    CHECK(rig.kills[0].defId == "hunt_pendant_thief");
    CHECK(rig.kills[0].isMiniBoss);
    CHECK(rig.kills[0].role == MonsterRole::HuntLeader);
    CHECK_FALSE(rig.monsters.IsHuntPresent("hunt_pendant_thief"));
    CHECK(rig.h.timers.Empty());
    // A silent spawn (zone entry / accept) has no announcement; a blocked spot falls back to the nearest ring tile.
    MonsterRig quiet(22, Vec2(60, 60));
    auto grid = std::make_shared<MTestGrid>(120, 120);
    grid->Block(82, 72);
    quiet.monsters.SetWorldForTesting(MGridWorld(grid, false));
    const std::vector<DueHunt> one{{bounty, &MHunt("hunt_redcap_gruk")}};
    quiet.monsters.SpawnHunts(one, false);
    REQUIRE_FALSE(quiet.monsters.All().empty());
    CHECK(quiet.monsters.All()[0].pos == Vec2(81, 71));  // ring 1, dr = -1, dc = -1
    CHECK_FALSE(MHasLog(quiet.h.events, "zone.quest.huntRevealed"));
    CHECK(MEvents<EvHuntRevealed>(quiet.h.events).empty());
  }

  TEST_CASE("SpawnDueHunts through the QuestSystem: accept -> bounty out (silent); not twice; zone filter") {
    MonsterRig rig(23, Vec2(60, 60));
    rig.OpenWorld();
    QuestProgress p;
    p.questId = "q_bandit_trouble";
    p.status = QuestStatus::Active;
    p.objectives.assign(1, 0);
    rig.quests.Load({p});
    if (rig.quests.OpenQuests().empty()) {
      MESSAGE("QuestSystem::Load is not implemented yet (quests area): SpawnDueHunts runtime part skipped");
      return;
    }
    rig.monsters.SpawnDueHunts(false);
    CHECK(rig.monsters.IsHuntPresent("hunt_redcap_gruk"));
    const size_t n = rig.monsters.All().size();
    rig.monsters.SpawnDueHunts(true);  // already out: nothing new
    CHECK(rig.monsters.All().size() == n);
    CHECK(MEvents<EvHuntRevealed>(rig.h.events).empty());
  }

  TEST_CASE("event spawns 6.4: ambush around a point, chasing, no affixes, never respawn, Events stream") {
    MonsterRig rig(31, Vec2(60, 60));
    rig.OpenWorld();
    const RngState aiBefore = rig.h.rng.Get(RngStream::Ai).GetState();
    const std::vector<std::string> ids{"slime_green", "goblin"};
    const std::vector<EntityId> spawned =
        rig.monsters.SpawnAmbush(ids, 4, Vec2(60, 60), 3, 2, MonsterRole::AmbushSpawn);
    REQUIRE(spawned.size() == 4);
    CHECK(rig.h.rng.Get(RngStream::Ai).GetState() == aiBefore);
    for (EntityId id : spawned) {
      const MonsterInstance& m = rig.M(id);
      CHECK(m.state == MonsterState::Chase);
      CHECK(m.role == MonsterRole::AmbushSpawn);
      CHECK(m.noRespawn);
      CHECK(m.affixes.empty());
      const double d = Dist(m.pos, Vec2(60, 60));
      CHECK(d >= 3 - 0.71);
      CHECK(d <= 5 + 0.71);
    }
    CHECK(rig.aggros.size() == 4);
    // Scripted draws: index, angle, distance per monster.
    MonsterRig s(32, Vec2(60, 60));
    s.OpenWorld();
    s.h.rng.Get(RngStream::Events).Script({0.6, 0.0, 0.5});  // goblin; angle 0; dist 2 + 0.5 * 3 = 3.5
    const std::vector<EntityId> one = s.monsters.SpawnAmbush(ids, 1, Vec2(40, 40), 2, 3, MonsterRole::RescueSpawn);
    REQUIRE(one.size() == 1);
    CHECK(s.M(one[0]).def.id == "goblin");
    CHECK(s.M(one[0]).pos == Vec2(JsRoundInt(43.5), 40));
    CHECK(s.M(one[0]).role == MonsterRole::RescueSpawn);
    // Blocked preferred tile: the ring search (row-major per ring) inside [1, size - 2].
    MonsterRig b(33, Vec2(60, 60));
    auto grid = std::make_shared<MTestGrid>(120, 120);
    grid->Block(44, 40);
    b.monsters.SetWorldForTesting(MGridWorld(grid, false));
    b.h.rng.Get(RngStream::Events).Script({0.6, 0.0, 0.5});
    const std::vector<EntityId> moved = b.monsters.SpawnAmbush(ids, 1, Vec2(40, 40), 2, 3, MonsterRole::AmbushSpawn);
    REQUIRE(moved.size() == 1);
    CHECK(b.M(moved[0]).pos == Vec2(43, 39));
  }

  TEST_CASE("GameSim integration: NewGame spawns the plains population and the mini-boss; the AI runs in the step") {
    auto sim = GameSim::Create(MD(), SimConfig{});
    REQUIRE(sim != nullptr);
    REQUIRE(sim->NewGame(ClassId::Warrior, Difficulty::Normal, 77));
    sim->Step();
    size_t alive = 0, shamans = 0;
    for (const MonsterView& v : sim->View().monsters) {
      if (v.alive) ++alive;
      if (v.defId == "miniboss_goblin_shaman") ++shamans;
    }
    CHECK(alive >= 56);  // 55 zone spawns + the mini-boss (no hunt is due at a new game)
    CHECK(shamans == 1);
    for (int i = 0; i < 20 && sim->WorldFrozen(); ++i) {  // a prologue, if the story plays one
      sim->Submit(CmdStorySkip{});
      sim->Step();
    }
    std::map<EntityId, Vec2> start;
    for (const MonsterView& v : sim->View().monsters) start[v.id] = v.pos;
    for (int i = 0; i < 400; ++i) sim->Step();
    if (sim->WorldFrozen()) {
      MESSAGE("the world is still frozen after NewGame: AI movement part skipped");
      return;
    }
    size_t moved = 0;
    for (const MonsterView& v : sim->View().monsters) {
      const auto it = start.find(v.id);
      if (it != start.end() && it->second != v.pos) ++moved;
    }
    CHECK(moved > 0);  // patrols start after 3 s of idling
  }

  TEST_CASE("defend waves (quests 3.10 / monsters 6.4): 3 + w on a radius-8 circle, scaled, chasing, never respawn") {
    MonsterRig rig(53, Vec2(100, 100));
    rig.OpenWorld();
    // emerald_plains list [slime_green, goblin, goblin_chief]: RandomInt(0, 2) picks.
    rig.h.rng.Get(RngStream::Ai).Script({0.0, 0.5, 0.9});
    const std::vector<EntityId> w0 = rig.monsters.SpawnDefendWave(Vec2(60, 60), 0);
    REQUIRE(w0.size() == 3);
    CHECK(rig.M(w0[0]).def.id == "slime_green");
    CHECK(rig.M(w0[1]).def.id == "goblin");
    CHECK(rig.M(w0[2]).def.id == "goblin_chief");  // bosses included (quests Q8, later)
    CHECK(rig.M(w0[0]).pos == Vec2(68, 60));
    CHECK(rig.M(w0[1]).pos == Vec2(56, 67));  // 60 + 8 cos 120 = 56, 60 + 8 sin 120 = 66.93
    CHECK(rig.M(w0[2]).pos == Vec2(56, 53));
    for (EntityId id : w0) {
      const MonsterInstance& m = rig.M(id);
      CHECK(m.state == MonsterState::Chase);
      CHECK(m.role == MonsterRole::DefendWave);
      CHECK(m.noRespawn);
      CHECK(m.affixes.empty());  // even the elite chief
    }
    CHECK(rig.M(w0[2]).maxHp == 160);
    CHECK(rig.aggros.size() == 3);
    // Wave 2 on nightmare: n = 5, difficulty then wave scaling.
    rig.h.session.difficulty = Difficulty::Nightmare;
    rig.h.rng.Get(RngStream::Ai).Script({0.5, 0.5, 0.5, 0.5, 0.5});
    const std::vector<EntityId> w2 = rig.monsters.SpawnDefendWave(Vec2(60, 60), 2);
    REQUIRE(w2.size() == 5);
    for (EntityId id : w2) {
      CHECK(rig.M(id).def.id == "goblin");
      CHECK(rig.M(id).maxHp == std::floor(83 * 1.6));    // nightmare 83, x(1 + 0.3 x 2)
      CHECK(rig.M(id).def.damage == std::floor(12 * 1.4));  // nightmare 12, x(1 + 0.2 x 2)
      CHECK(Dist(rig.M(id).pos, Vec2(60, 60)) == doctest::Approx(8).epsilon(0.1));
    }
    // Clamped to [2, size - 3] without a walkability check.
    MonsterRig edge(54, Vec2(100, 100));
    auto grid = std::make_shared<MTestGrid>(120, 120);
    grid->Block(2, 2);
    edge.monsters.SetWorldForTesting(MGridWorld(grid, false));
    const std::vector<EntityId> e = edge.monsters.SpawnDefendWave(Vec2(1, 1), 0);
    REQUIRE(e.size() == 3);
    CHECK(edge.M(e[0]).pos == Vec2(9, 2));
    CHECK(edge.M(e[2]).pos == Vec2(2, 2));  // (-3, -6) clamped onto a blocked tile (web: no check)
    // Killed wave monsters never respawn.
    edge.monsters.ApplyDamage(e[0], 1e6, DamageFlags{});
    CHECK(edge.h.timers.Empty());
  }

  TEST_CASE("boss rename hook: once per live instance, the snapshot shows the intro name, a respawn starts unmarked") {
    MonsterRig rig(56, Vec2(100, 100));
    rig.OpenWorld();
    const EntityId chief = rig.Spawn("goblin_chief", {15, 95});
    CHECK(rig.monsters.MarkStoryNamed(chief));
    CHECK_FALSE(rig.monsters.MarkStoryNamed(chief));  // already renamed
    CHECK_FALSE(rig.monsters.MarkStoryNamed(999999));
    Snapshot snap;
    rig.monsters.FillSnapshot(snap);
    REQUIRE(snap.monsters.size() == 1);
    CHECK(snap.monsters[0].storyNamed);
    const BossIntroDef* intro = MD().Story().BossIntroFor("goblin_chief");
    if (intro != nullptr) CHECK(snap.monsters[0].nameKey == intro->name);
    rig.monsters.ApplyDamage(chief, 1e6, DamageFlags{});
    CHECK_FALSE(rig.monsters.MarkStoryNamed(chief));  // dead
    rig.h.Step(static_cast<int>(MAi().respawnDelayMs / kSimStepMs) + 2);
    const MonsterInstance& again = rig.monsters.All()[0];
    REQUIRE(again.IsAlive());
    CHECK_FALSE(again.storyNameShown);
    CHECK(rig.monsters.MarkStoryNamed(again.id));
  }
}  // TEST_SUITE

// ---- split from mixed tests (their systems part stays in monsters_test.cpp) -----------------------------------------

TEST_SUITE("monsters") {
  using namespace abyss::test::monsters_fx;

  // Split from "mini-boss data (mini-bosses-lore.test.ts)": the Chapter 1 entry.
  TEST_CASE("mini-boss data: the emerald_plains mini-boss is the goblin shaman at (60,55)") {
    const MonsterTables& t = MD().Monsters();
    const MiniBossEntry* ep = t.MiniBossFor("emerald_plains");
    REQUIRE(ep != nullptr);
    CHECK(ep->monsterId == "miniboss_goblin_shaman");
    CHECK(ep->spawn == TilePos{60, 55});
  }

  // Split from "queries: ... snapshot views": once the boss label is renamed the view carries the intro name key.
  TEST_CASE("queries: the renamed goblin chief's snapshot view carries its intro name key (story 8.3)") {
    MonsterRig rig(51, Vec2(60, 60));
    rig.OpenWorld();
    const EntityId chief = rig.Spawn("goblin_chief", {5, 110});
    rig.M(chief).storyNameShown = true;
    Snapshot named;
    rig.monsters.FillSnapshot(named);
    REQUIRE(named.monsters.size() == 1);
    CHECK(named.monsters[0].storyNamed);
    CHECK(named.monsters[0].nameKey == "story.boss.goblin_chief.name");
  }

  // Split from "loot-floor flags (story / endgame scrutiny)": regular zone monsters have no loot floor.
  TEST_CASE("loot-floor flags: the Chapter 1 regulars and the chief have no loot floor") {
    for (const char* id : {"slime_green", "goblin", "goblin_chief"}) {
      CHECK_FALSE(MDef(id).isMiniBoss);
      CHECK_FALSE(MDef(id).isSubDungeonMiniBoss);
    }
  }

  // Split from "buildAffixName: base name without affixes, [a] base, [a<middle dot>b] base, both locales": the goblin
  // chief's labels and the Gruk hunt key.
  TEST_CASE("buildAffixName: the goblin chief's labels in both locales; hunt leaders are named by their hunt id") {
    I18n zh, en;
    REQUIRE(zh.LoadTable(test::ReadFile(test::DataDir() + "/i18n_zh-CN.json")));
    REQUIRE(en.LoadTable(test::ReadFile(test::DataDir() + "/i18n_en.json")));
    en.SetLocale(LocaleId::En);
    REQUIRE(en.Current() == LocaleId::En);
    const std::string dot = "\xC2\xB7";
    const MonsterDef& chief = MDef("goblin_chief");
    CHECK(MonsterNameKey(chief) == "data.monster.goblin_chief");
    CHECK(MonsterLabelText(en, MonsterNameKey(chief), {}) == en.T("data.monster.goblin_chief"));
    const std::vector<EliteAffixType> one{EliteAffixType::FireEnhanced};
    CHECK(MonsterLabelText(en, MonsterNameKey(chief), one) ==
          "[" + en.T("sys.eliteAffix.name.fire_enhanced") + "] " + en.T("data.monster.goblin_chief"));
    const std::vector<EliteAffixType> three{EliteAffixType::Swift, EliteAffixType::Vampiric, EliteAffixType::Frozen};
    CHECK(MonsterLabelText(zh, MonsterNameKey(chief), three) ==
          "[" + zh.T("sys.eliteAffix.name.swift") + dot + zh.T("sys.eliteAffix.name.vampiric") + dot +
              zh.T("sys.eliteAffix.name.frozen") + "] " + zh.T("data.monster.goblin_chief"));
    CHECK(zh.T("data.monster.goblin_chief") == chief.name);  // FIX Q11: the key resolves to the zh name
    // Hunt leaders are named by their hunt id (localized), an empty base name keeps the bracket.
    const MonsterDef gruk = MNormalDef("hunt_redcap_gruk");
    CHECK(MonsterNameKey(gruk) == "data.monster.hunt_redcap_gruk");
    CHECK(en.Has("data.monster.hunt_redcap_gruk"));
  }

  // Split from "mini-boss lines are i18n keys (Q10 / M8)": the opened event carries the header key and the lines.
  TEST_CASE("mini-boss lines: the goblin shaman's opened event carries its header key and its three line keys") {
    MonsterRig rig(5, Vec2(60, 60));
    rig.OpenWorld();
    rig.monsters.SpawnMiniBoss();
    rig.h.events.Clear();
    rig.Step();
    REQUIRE(rig.monsters.MiniBossDialogueActive());
    const std::vector<EvMiniBossDialogue> ev = MEvents<EvMiniBossDialogue>(rig.h.events);
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].nameKey == "data.monster.miniboss_goblin_shaman");
    CHECK(ev[0].lineKeys == MiniBossDialogueLineKeys(*MD().Monsters().MiniBossDialogue("miniboss_goblin_shaman"),
                                                     "miniboss_goblin_shaman"));
    CHECK(ev[0].lineKeys.size() == 3);
  }
}  // TEST_SUITE

// =====================================================================================================================
// Quests + story: the Chapter 1 quest chain, the elder, beats, lore (from quests_test.cpp)
// Moved verbatim (QuestEngine / QuestUX / StoryScript ports and the quests-story-ch1.md section 16 vectors on the
// Chapter 1 data); the engine tests that need no Chapter 1 content stay in quests_test.cpp.
// =====================================================================================================================

TEST_SUITE("quests") {
  using namespace abyss::test::quests_fx;

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
}  // TEST_SUITE
TEST_SUITE("story") {
  using namespace abyss::test::quests_fx;

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
}  // TEST_SUITE
TEST_SUITE("quests") {
  using namespace abyss::test::quests_fx;

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

  TEST_CASE("Q11: escort / defend names resolve through data.escortNpc.<id> / data.defendTarget.<id> without q_") {
    const DataStore& data = test::RealData();
    CHECK(EscortNpcNameKey("q_escort_merchant_plains") == "data.escortNpc.escort_merchant_plains");
    CHECK(DefendTargetNameKey("q_defend_camp_forest") == "data.defendTarget.defend_camp_forest");
    CHECK(EscortNpcNameKey("escort_x") == "data.escortNpc.escort_x");
    // Every escort / defend quest either has its key in both locales, or is a known key-less one (zh name).
    const I18n en = EnglishStrings();
    int keyed = 0;
    for (const QuestDef& q : data.Quests().quests) {
      if (!q.hasEscortNpc && !q.hasDefendTarget) continue;
      const std::string key = q.hasEscortNpc ? EscortNpcNameKey(q.id) : DefendTargetNameKey(q.id);
      CAPTURE(q.id);
      if (q.id == "q_trapped_miners") {
        CHECK_FALSE(data.Strings().Has(key));
        continue;
      }
      CHECK(data.Strings().Has(key));
      CHECK(en.Lookup(LocaleId::En, key) != nullptr);
      ++keyed;
    }
    CHECK(keyed == 4);
  }

  TEST_CASE("Q11: the escort logs and its label use the key, so English shows 'Traveling Merchant'") {
    QwSim s;
    s.Qs().Load({Rec("q_kill_slimes", QuestStatus::TurnedIn, {10}), Rec("q_kill_goblins", QuestStatus::TurnedIn, {15})});
    s.sim->ClearEvents();
    REQUIRE(s.Qs().Accept("q_escort_merchant_plains"));
    const std::vector<Event> ev(s.ctx->events.Items().begin(), s.ctx->events.Items().end());
    const EvLog* appeared = FindLog(ev, "zone.escort.npcAppeared");
    REQUIRE(appeared != nullptr);
    REQUIRE(appeared->text.args.size() == 1);
    CHECK(appeared->text.args[0].isKey);
    CHECK(appeared->text.args[0].value == "data.escortNpc.escort_merchant_plains");
    const I18n en = EnglishStrings();
    CHECK(en.T(appeared->text) == "Traveling Merchant appeared! Escort target marked.");
    // the label (snapshot EscortState): NameOr(nameKey, name)
    const EscortState& e = s.Qw().Escort();
    CHECK(e.nameKey == "data.escortNpc.escort_merchant_plains");
    CHECK(e.name == s.ctx->data.FindQuest("q_escort_merchant_plains")->escortNpc.name);
    CHECK(en.NameOr(e.nameKey, e.name) == "Traveling Merchant");
    s.Place(Vec2(32, 40));
    const std::vector<Event> joined = s.StepsCollect(1);
    const EvLog* j = FindLog(joined, "zone.escort.joined");
    REQUIRE(j != nullptr);
    CHECK(en.T(j->text) == "Traveling Merchant is following you.");
  }

  TEST_CASE("3.5: an examined clue floats '<targetName>\\n<note>'; progress carries targetId and amount (13)") {
    QwSim s;
    s.Qs().Load({Rec("q_kill_slimes", QuestStatus::TurnedIn, {10})});
    REQUIRE(s.Qs().Accept("q_lost_pendant"));
    const QuestNode* cloth = NodeOf(s.Qw(), true, "q_lost_pendant", 1);
    REQUIRE(cloth != nullptr);
    const Vec2 at = cloth->pos;
    s.Place(Vec2(at.x + 1.5, at.y));
    const std::vector<Event> ev = s.StepsCollect(1);
    const EvFloatingText* note = nullptr;
    for (const EvFloatingText* f : Of<EvFloatingText>(ev)) {
      if (f->kind == FloatingTextKind::Custom && f->text.key == "zone.quest.clueNote") note = f;
    }
    REQUIRE(note != nullptr);
    CHECK(note->pos == at);
    const I18n& zh = s.ctx->data.Strings();
    CHECK(zh.T(note->text) ==
          zh.T("data.questTarget.clue_pendant_cloth") + "\n" + zh.T("data.questClue.clue_pendant_cloth"));
    CHECK(EnglishStrings().T(note->text) ==
          "Rag Snagged on Thorns\nA green rag reeking of goblin hangs on the thorns. The trail bends southeast.");
    bool progress = false;
    for (const EvQuestUpdate* u : Of<EvQuestUpdate>(ev)) {
      if (u->kind != EvQuestUpdate::Kind::Progress || u->questId != "q_lost_pendant") continue;
      progress = u->objectiveIndex == 1 && u->targetId == "clue_pendant_cloth" && u->amount == 1;
    }
    CHECK(progress);
  }

  TEST_CASE("13: kill progress events name the target and the requested amount") {
    QwSim s;
    REQUIRE(s.Qs().Accept("q_kill_slimes"));
    s.sim->ClearEvents();
    REQUIRE(s.Kill(s.FindAlive("slime_green")));
    const std::vector<Event> ev(s.ctx->events.Items().begin(), s.ctx->events.Items().end());
    const EvQuestUpdate* kill = nullptr;
    for (const EvQuestUpdate* u : Of<EvQuestUpdate>(ev)) {
      if (u->kind == EvQuestUpdate::Kind::Progress && u->questId == "q_kill_slimes") kill = u;
    }
    REQUIRE(kill != nullptr);
    CHECK(kill->targetId == "slime_green");
    CHECK(kill->amount == 1);
    CHECK(kill->current == 1);
    CHECK_FALSE(kill->hasFrom);
    // an amount above what is left is reported as requested (the counter clamps)
    s.sim->ClearEvents();
    s.Qs().UpdateProgress(ObjectiveType::Kill, "slime_green", 50);
    const std::vector<Event> ev2(s.ctx->events.Items().begin(), s.ctx->events.Items().end());
    bool clamped = false;
    for (const EvQuestUpdate* u : Of<EvQuestUpdate>(ev2)) {
      if (u->kind == EvQuestUpdate::Kind::Progress && u->questId == "q_kill_slimes") {
        clamped = u->amount == 50 && u->current == u->required && u->completesQuest;
      }
    }
    CHECK(clamped);
  }

  TEST_CASE("world 10.3: hidden-area rewards log what the hero got (chest item, gold pile, lore scroll)") {
    QwSim s;
    s.Place(Vec2(108, 108));
    s.Steps(2);
    LoreSystem& lore = *s.ctx->sys.lore;
    REQUIRE(lore.IsDiscovered("hidden_ep_elven_cache"));
    s.sim->ClearEvents();
    REQUIRE(lore.ClaimHiddenReward("hidden_ep_elven_cache", 1));  // gold pile 200
    std::vector<Event> ev(s.ctx->events.Items().begin(), s.ctx->events.Items().end());
    const EvLog* gold = FindLog(ev, "zone.hiddenArea.gotGold");
    REQUIRE(gold != nullptr);
    CHECK(gold->type == LogType::Loot);
    CHECK(EnglishStrings().T(gold->text) == "Gained 200 gold");
    s.sim->ClearEvents();
    const size_t bag0 = s.ctx->sys.inventory->Items().Bag().size();
    REQUIRE(lore.ClaimHiddenReward("hidden_ep_elven_cache", 0));  // rare chest
    ev.assign(s.ctx->events.Items().begin(), s.ctx->events.Items().end());
    const EvLog* item = FindLog(ev, "zone.hiddenArea.gotItem");
    REQUIRE(item != nullptr);
    CHECK(item->type == LogType::Loot);
    REQUIRE(item->text.args.size() == 1);
    CHECK(item->text.args[0].name == "itemName");
    CHECK_FALSE(item->text.args[0].value.empty());
    CHECK(s.ctx->sys.inventory->Items().Bag().size() == bag0 + 1);
    // the twilight forest shrine's second reward is a lore scroll: log only
    SaveData save = SaveIn(s, "twilight_forest", 110, 15, {},
                           {"prologue", "chapter_emerald_plains", "chapter_twilight_forest"});
    std::string err;
    REQUIRE(s.sim->LoadGame(save, &err) == SaveError::None);
    s.ctx = &s.sim->Context();
    s.Steps(2);
    LoreSystem& tf = *s.ctx->sys.lore;
    REQUIRE(tf.IsDiscovered("hidden_tf_moonlight_shrine"));
    s.sim->ClearEvents();
    const int64_t gold0 = s.H().Gold();
    REQUIRE(tf.ClaimHiddenReward("hidden_tf_moonlight_shrine", 1));
    ev.assign(s.ctx->events.Items().begin(), s.ctx->events.Items().end());
    const EvLog* scroll = FindLog(ev, "zone.hiddenArea.gotScroll");
    REQUIRE(scroll != nullptr);
    CHECK(scroll->type == LogType::System);
    CHECK(s.H().Gold() == gold0);
    CHECK(tf.IsRewardClaimed("hidden_tf_moonlight_shrine", 1));
  }
}  // TEST_SUITE
TEST_SUITE("story") {
  using namespace abyss::test::quests_fx;

  TEST_CASE("8.5: the chapter card step carries number, title, subtitle and text keys") {
    QwSim s(7, /*skipStory=*/false);
    s.PlayBeat();  // the prologue
    REQUIRE(s.Story().Playback().beat.id == "chapter_emerald_plains");
    // the chapter card's step was emitted when the beat started (inside the prologue's last advance)
    const ChapterCard* ch = s.ctx->data.Story().ChapterFor("emerald_plains");
    REQUIRE(ch != nullptr);
    const EvStoryStep* card = nullptr;
    for (const EvStoryStep* st : Of<EvStoryStep>(s.ctx->events.Items())) {
      if (st->beatId == "chapter_emerald_plains") card = st;
    }
    REQUIRE(card != nullptr);
    CHECK(card->slide.heading == ch->number);
    CHECK(card->slide.title == ch->title);
    CHECK(card->slide.subtitle == "story.chapter.emerald_plains.subtitle");
    CHECK(card->slide.text == ch->text);
    CHECK(EnglishStrings().T(card->slide.subtitle) == "The Emerald Plains");
  }

  TEST_CASE("Q7: a turn-in cutscene lost to a quit replays on the next entry of the quest's zone, with its pet") {
    QwSim s;
    s.Qs().Load({Rec("q_kill_slimes", QuestStatus::Completed, {10})});
    REQUIRE(s.Qw().TurnIn("q_kill_slimes", 0));
    REQUIRE(s.Story().IsBusy());
    SaveData save;  // the turn-in autosave: turned in, cs_ep_mark not seen yet
    s.sim->BuildSave(save, 0);
    REQUIRE(save.hasStorySeen);
    REQUIRE(std::find(save.storySeen.begin(), save.storySeen.end(), "cs_ep_mark") == save.storySeen.end());
    std::string err;
    REQUIRE(s.sim->LoadGame(save, &err) == SaveError::None);
    s.ctx = &s.sim->Context();
    CHECK(s.Story().IsBusy());
    CHECK_FALSE(s.Story().IsCinematic());  // the normal 650 ms turn-in delay, world live
    s.Steps(40);
    REQUIRE(s.Story().IsCinematic());
    CHECK(s.Story().Playback().beat.id == "cs_ep_mark");
    s.Story().FinishAllBeats();
    CHECK(s.Story().Progress().Has("cs_ep_mark"));
    // seen now: the next load plays nothing
    SaveData again;
    s.sim->BuildSave(again, 0);
    REQUIRE(s.sim->LoadGame(again, &err) == SaveError::None);
    s.ctx = &s.sim->Context();
    CHECK_FALSE(s.Story().IsBusy());

    // Only the quest's own zone replays it; a save without storySeen never does.
    SaveData elsewhere = SaveIn(s, "twilight_forest", 20, 20, {Rec("q_kill_slimes", QuestStatus::TurnedIn, {10})},
                                {"prologue", "chapter_emerald_plains", "chapter_twilight_forest"});
    REQUIRE(s.sim->LoadGame(elsewhere, &err) == SaveError::None);
    s.ctx = &s.sim->Context();
    CHECK_FALSE(s.Story().IsBusy());
    SaveData legacy = save;
    legacy.hasStorySeen = false;
    legacy.storySeen.clear();
    REQUIRE(s.sim->LoadGame(legacy, &err) == SaveError::None);
    s.ctx = &s.sim->Context();
    s.Story().FinishAllBeats();  // the chapter card a pre-story save still shows
    CHECK_FALSE(s.Story().Progress().Has("cs_ep_mark"));

    // A lost grantPet beat comes back too: q_seal_fire_rift -> cs_sd_finale, then cs_sd_helia (pet_phoenix). The pet
    // itself is already back at the load (EmberTower.retroGrantPets, homestead.json storyPets: every zone entry, like
    // the web), so the replayed beat adds nothing and logs no sys.pet.duplicate.
    SaveData desert = SaveIn(s, "scorching_desert", 15, 18, {Rec("q_seal_fire_rift", QuestStatus::TurnedIn, {1})},
                             {"prologue", "chapter_emerald_plains", "chapter_scorching_desert"});
    REQUIRE(s.sim->LoadGame(desert, &err) == SaveError::None);
    s.ctx = &s.sim->Context();
    CHECK(s.ctx->sys.pets->Has("pet_phoenix"));
    std::vector<std::string> finished;
    s.ctx->bus.Subscribe<StoryBeatFinishedMsg>([&finished](const StoryBeatFinishedMsg& m) { finished.push_back(m.beatId); });
    s.sim->ClearEvents();
    s.Story().FinishAllBeats();
    CHECK(finished == std::vector<std::string>{"cs_sd_finale", "cs_sd_helia"});
    CHECK(s.ctx->sys.pets->Has("pet_phoenix"));
    CHECK(s.ctx->sys.pets->Owned().size() == 1);
    const std::vector<Event> ev(s.ctx->events.Items().begin(), s.ctx->events.Items().end());
    CHECK_FALSE(HasLog(ev, "sys.pet.duplicate"));
  }
}  // TEST_SUITE

// ---- split from mixed tests (their systems part stays in quests_test.cpp) -------------------------------------------

TEST_SUITE("quests") {
  using namespace abyss::test::quests_fx;

  // Split from "dialogue data: 5 NPC trees on quest NPCs; ...": the elder's one-time reward choice (Q1).
  TEST_CASE("dialogue data: the elder's reward_ask choice pays 30 gold and 50 exp") {
    const DataStore& data = test::RealData();
    const DialogueTree* elder = data.Dialogues().Find("quest_elder");
    REQUIRE(elder != nullptr);
    const DialogueNode* ask = elder->FindNode("reward_ask");
    REQUIRE(ask != nullptr);
    CHECK(ask->choices[0].reward.present);
    CHECK(ask->choices[0].reward.gold == 30);
    CHECK(ask->choices[0].reward.exp == 50);
  }
}  // TEST_SUITE
TEST_SUITE("story") {
  using namespace abyss::test::quests_fx;

  // Split from "script: prologue / five chapters / epilogue / credits; ...": the prologue's slide count, the boss-intro
  // roster and the Chapter 1 triggers (10.2).
  TEST_CASE("script: six prologue slides, the boss-intro roster and the Chapter 1 quest triggers (10.2)") {
    const DataStore& data = test::RealData();
    const StoryScript& s = data.Story();
    CHECK(s.prologue.slides.size() == 6);
    std::vector<std::string> bosses;
    for (const BossIntroDef& b : s.bossIntros) bosses.push_back(b.monsterId);
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
  }
}  // TEST_SUITE

// =====================================================================================================================
// World: the generated emerald_plains map, its routes, camp NPCs, event data, hidden area (from world_test.cpp)
// The re-baseline of chapter1.md 17 re-pins these (golden facts, RNG checkpoints, A* routes, walkable counts).
// =====================================================================================================================

TEST_SUITE("world") {
  using namespace abyss::test::world_fx;

  TEST_CASE("emerald_plains golden facts: tile counts, decor counts and RNG checkpoints (world 3.3)") {
    const DataStore& data = test::RealData();
    const MapDef* map = data.FindMap("emerald_plains");
    REQUIRE(map != nullptr);
    MapGenTrace trace;
    MapGenInput in;
    in.map = map;
    in.avoid = map->generatorAvoid;
    REQUIRE(map->generatorAvoid.size() == 28);
    const ZoneGrid g = GenerateMap(data.World().mapGen, in, &trace);
    CHECK(trace.afterSecondary == 114752593);
    CHECK(trace.drawsSecondary == 13924);
    CHECK(trace.afterWalls == 1438865421);
    CHECK(trace.drawsWalls == 27380);
    CHECK(trace.afterPaths == 804188721);
    CHECK(trace.drawsPaths == 31383);
    CHECK(trace.afterLakes == 2123587627);
    CHECK(trace.drawsLakes == 31440);
    CHECK(trace.afterDecorations == 1699819340);
    CHECK(trace.drawsDecorations == 43822);
    std::array<int32_t, 7> counts{};
    for (TileType t : g.Tiles()) ++counts[static_cast<size_t>(t)];
    CHECK(counts == std::array<int32_t, 7>{11003, 1972, 0, 363, 820, 192, 50});
    CHECK(WorldWalkableCount(g) == 13167);
    CHECK(g.Decorations().size() == 1269);
    auto countDecor = [&g](const char* type) {
      return std::count_if(g.Decorations().begin(), g.Decorations().end(), [type](const Decoration& d) { return d.type == type; });
    };
    CHECK(countDecor("grass") == 538);
    CHECK(countDecor("flower") == 260);
    CHECK(countDecor("bush") == 203);
    CHECK(countDecor("rock") == 105);
    CHECK(countDecor("tree") == 66);
    CHECK(countDecor("tree_round") == 51);
    CHECK(countDecor("mushroom_red") == 45);
    CHECK(countDecor("boulder") == 1);
    // Anchor tiles after generation (3.3).
    CHECK(g.Tile(15, 22) == TileType::Dirt);
    for (const MapSpawnDef& s : map->spawns) CHECK(g.Tile(s.pos.col, s.pos.row) == TileType::Dirt);
    CHECK(g.Tile(50, 30) == TileType::Grass);
    CHECK(g.Tile(70, 65) == TileType::Grass);
    CHECK(g.Tile(108, 108) == TileType::Grass);
    CHECK(g.Tile(110, 107) == TileType::Grass);
    CHECK(g.Tile(60, 55) == TileType::Grass);
    CHECK(g.Tile(119, 60) == TileType::Wall);
    CHECK(g.Tile(118, 60) == TileType::Grass);
    CHECK(g.Tile(12, 13) == TileType::Camp);
    // Decorations carry the 15.5 jitter; tall types are flagged for W5.
    for (const Decoration& d : g.Decorations()) {
      CHECK(d.scale >= 0.9);
      CHECK(d.scale < 1.1);
      CHECK(std::fabs(d.offsetCol) < 0.3);
      CHECK(std::fabs(d.offsetRow) < 0.3);
      CHECK(d.blocking == (d.type == "tree" || d.type == "tree_round" || d.type == "boulder"));
    }
  }

  TEST_CASE("A* golden routes on the shipped emerald_plains map, node for node (world 3.5)") {
    const DataStore& data = test::RealData();
    const ZoneGrid g = BuildRawZoneGrid(data, *data.FindMap("emerald_plains"));
    Pathfinder pf(g);
    struct Route {
      TilePos from, to;
      size_t steps, diagonals;
      double cost;
      std::vector<TilePos> first;
    };
    const Route routes[] = {
        {{15, 22}, {15, 15}, 7, 0, 7.000, {{15, 21}, {15, 20}, {15, 19}}},
        {{15, 22}, {118, 60}, 103, 38, 118.732, {{16, 22}, {17, 23}, {18, 24}}},
        {{15, 22}, {108, 108}, 98, 81, 131.534, {{16, 22}, {17, 23}, {18, 24}}},
        {{15, 15}, {95, 100}, 86, 79, 118.706, {{16, 16}, {17, 17}, {18, 18}}},
        {{15, 22}, {60, 55}, 45, 33, 58.662, {{16, 22}, {17, 22}, {18, 23}}},
        {{15, 22}, {18, 25}, 4, 2, 4.828, {{16, 23}, {17, 23}, {18, 24}, {18, 25}}},
    };
    for (const Route& rt : routes) {
      CAPTURE(rt.to.col);
      CAPTURE(rt.to.row);
      std::vector<TilePos> path;
      REQUIRE(pf.FindPath(rt.from.col, rt.from.row, rt.to.col, rt.to.row, path));
      CHECK(path.size() == rt.steps);
      size_t diag = 0;
      TilePos prev = rt.from;
      for (const TilePos& p : path) {
        diag += (p.col != prev.col && p.row != prev.row) ? 1 : 0;
        prev = p;
      }
      CHECK(diag == rt.diagonals);
      CHECK(std::fabs(WorldPathCost(rt.from, path) - rt.cost) < 1e-9);
      for (size_t i = 0; i < rt.first.size(); ++i) CHECK(path[i] == rt.first[i]);
      CHECK(WorldPathValid(g, rt.from, path));
    }
    // (17,25) is the wall that blocks the direct diagonal of the last route.
    CHECK_FALSE(g.Walkable(17, 25));
  }

  TEST_CASE("A* routes re-baselined on the full W5 grid (camp blockers + tall decor) stay valid and optimal-ish") {
    const DataStore& data = test::RealData();
    const MapDef& map = *data.FindMap("emerald_plains");
    const ZoneGrid raw = BuildRawZoneGrid(data, map);
    const ZoneGrid g = BuildZoneGrid(data, map);
    Pathfinder praw(raw), pf(g);
    const std::pair<TilePos, TilePos> routes[] = {{{15, 22}, {15, 15}},   {{15, 22}, {118, 60}}, {{15, 22}, {108, 108}},
                                                  {{15, 15}, {95, 100}},  {{15, 22}, {60, 55}},  {{15, 22}, {18, 25}}};
    for (const auto& [from, to] : routes) {
      std::vector<TilePos> a, b;
      REQUIRE(praw.FindPath(from.col, from.row, to.col, to.row, a));
      REQUIRE(pf.FindPath(from.col, from.row, to.col, to.row, b));
      CHECK(WorldPathValid(g, from, b));
      CHECK(b.back() == to);
      CHECK(WorldPathCost(from, b) >= WorldPathCost(from, a) - 1e-9);
      CHECK(WorldPathCost(from, b) <= WorldPathCost(from, a) * 1.25);
    }
  }

  TEST_CASE("zone entry: hero at playerStart, camp NPC slots, field NPCs, exits sealed in milestone 1") {
    WorldRig w;
    REQUIRE(w.ok);
    const MapDef& map = *WD().FindMap("emerald_plains");
    CHECK(w.hero->Position() == Vec2(map.playerStart.col, map.playerStart.row));
    CHECK(w.hero->Position() == Vec2(15, 22));
    auto at = [&w](const char* id) { return w.zone.FindNpc(id) != nullptr ? w.zone.FindNpc(id)->pos : Vec2(-1, -1); };
    // Camp layout v2 slots (W11, CampLayout.h): eighth tiles, exact in binary.
    CHECK(at("blacksmith") == Vec2(15, 15) + camp::kCampNpcSlots[0]);
    CHECK(at("blacksmith") == Vec2(16.875, 17.375));
    CHECK(at("merchant") == Vec2(16.375, 13.625));  // camp 1 first; camp 2's merchant also exists
    CHECK(at("quest_elder") == Vec2(13.125, 17.375));
    CHECK(at("stash") == Vec2(12.25, 14.25));       // I7: the stash keeper is the 4th camp-1 NPC
    // Idle facing (W11): every camp NPC looks at the fire (no tents).
    auto facing = [&w](const char* id) { return w.zone.FindNpc(id) != nullptr ? w.zone.FindNpc(id)->facing : Vec2(); };
    CHECK(facing("blacksmith") == camp::CampNpcFacing(0));
    CHECK(facing("merchant") == camp::CampNpcFacing(1));
    CHECK(facing("quest_elder") == camp::CampNpcFacing(2));
    CHECK(facing("stash") == camp::CampNpcFacing(3));
    CHECK(facing("blacksmith").Dot((Vec2(15, 15) - at("blacksmith")).Normalized()) > 1.0 - 1e-12);
    CHECK(facing("merchant").Dot(Vec2(-1.0, 1.0).Normalized()) > 1.0 - 1e-12);  // slot 1 (1.375, -1.375) -> the fire
    CHECK(facing("stash").Dot((Vec2(15, 15) - at("stash")).Normalized()) > 1.0 - 1e-12);
    CHECK(facing("plains_herbalist") == Vec2(0, 1));  // field NPCs keep the default
    CHECK(at("plains_herbalist") == Vec2(50, 30));
    CHECK(at("plains_wanderer") == Vec2(70, 65));
    size_t merchants = 0;
    for (const NpcPlacement& n : w.zone.Npcs()) {
      if (n.npcId == "merchant") {
        ++merchants;
        if (merchants == 2) CHECK(n.pos == Vec2(96.875, 102.375));  // camp 2 (95,100), slot 0
      }
    }
    CHECK(merchants == 2);
    REQUIRE(w.zone.Exits().size() == 1);
    CHECK(w.zone.Exits()[0].sealed);  // W7
    CHECK(w.zone.SafeZoneRadius() == 9);
    CHECK(w.zone.InSafeZone({15, 22}));
    CHECK_FALSE(w.zone.InSafeZone({15, 24}));  // distSq 81: strict
    CHECK(w.zone.NearCampfire({15, 20}));
    CHECK_FALSE(w.zone.NearCampfire({15, 20.01}));
    CHECK(w.zone.CampPosition(0) == Vec2(15, 15));
    CHECK(w.zone.CampPosition(7) == Vec2(15, 22));  // missing camp -> playerStart
    // Camp blockers (W11) are baked into the grid; every camp NPC stands on a walkable tile.
    CHECK_FALSE(w.zone.Walkable(19, 15));  // well (4, -0.15)
    CHECK_FALSE(w.zone.Walkable(11, 15));  // crate (-3.9, -0.1)
    CHECK_FALSE(w.zone.Walkable(19, 11));  // barrel (3.6, -4.25)
    CHECK(w.zone.Walkable(18, 19));        // (3,4): no tent there any more (user request)
    CHECK(w.zone.Walkable(15, 15));
    for (const NpcPlacement& n : w.zone.Npcs()) {
      const TilePos t = RoundToTile(n.pos);
      CHECK(w.zone.Walkable(t.col, t.row));
    }
    // Unknown map ids fall back to the default map (9.1 step 1).
    WorldRig u(5, true, "no_such_zone");
    CHECK(u.ok);
    CHECK(u.zone.MapId() == "emerald_plains");
  }

  TEST_CASE("exploration 10.3: standing at (108,108) sees the whole elven cache; (104,102) misses two corners") {
    WorldRig w;
    REQUIRE(w.ok);
    REQUIRE(w.zone.Map().hiddenAreas.size() == 1);
    const HiddenAreaDef& cache = w.zone.Map().hiddenAreas[0];
    CHECK(cache.id == "hidden_ep_elven_cache");
    CHECK_FALSE(cache.hasBounds);  // bounds = centre (108,108) +- radius 6
    w.Place({104, 102});
    w.Step();
    CHECK_FALSE(w.explore.AreaFullyExplored(cache));
    CHECK_FALSE(w.explore.Grid().IsExplored(102, 114));
    CHECK_FALSE(w.explore.Grid().IsExplored(114, 114));
    w.Place({108, 108});
    w.Step();
    CHECK(w.explore.AreaFullyExplored(cache));
    // The grid is per visit: a zone re-entry resets it.
    w.zone.ExitZone();
    REQUIRE(w.zone.EnterZone("emerald_plains", false, {}));
    w.explore.OnZoneEnter();
    CHECK_FALSE(w.explore.Grid().IsExplored(108, 108));
  }
}  // TEST_SUITE

// ---- split from mixed tests (their systems part stays in world_test.cpp) --------------------------------------------

TEST_SUITE("world") {
  using namespace abyss::test::world_fx;

  // Split from "camp blockers (W11)": the generated emerald_plains grid the camp-blocker vectors were cut on.
  TEST_CASE("camp blockers: the raw emerald_plains grid has 13167 walkable tiles; the two camps' blockers take 2 x 8") {
    const DataStore& data = test::RealData();
    const MapDef& map = *data.FindMap("emerald_plains");
    REQUIRE(WorldWalkableCount(BuildRawZoneGrid(data, map)) == 13167);
    ZoneGrid full = BuildRawZoneGrid(data, map);
    ApplyCampBlockers(full, map);
    CHECK(WorldWalkableCount(full) == 13167 - 2 * 8);
  }

  // Split from "random events 13.1: exported config, definitions and zone data": the emerald_plains event data.
  TEST_CASE("random events 13.1: the emerald_plains event data (ambush pool and counts, merchant, puzzle, rescue)") {
    const RandomEventsDef& def = WD().World().randomEvents;
    const ZoneEventDataDef* ep = def.ForZone("emerald_plains");
    REQUIRE(ep != nullptr);
    CHECK(ep->ambushMonsters == std::vector<std::string>{"slime_green", "goblin"});
    CHECK(ep->ambushCountMin == 3);
    CHECK(ep->ambushCountMax == 5);
    CHECK(ep->merchantItems == std::vector<std::string>{"iron_sword", "leather_armor", "hp_potion", "mp_potion"});
    REQUIRE(ep->puzzles.size() == 1);
    CHECK(ep->puzzles[0].rewardGold == 50);
    CHECK(ep->puzzles[0].rewardExp == 30);
    CHECK(ep->rescueRewardGold == 30);
    CHECK(ep->rescueRewardExp == 25);
  }

  // Split from "random events 13.3: a zone without event data gets createEvent's fallbacks": the zone's first monster
  // def (the fallback ambush / rescue monster) is the green slime.
  TEST_CASE("random events 13.3: the emerald_plains zone list starts with slime_green (the fallback monster)") {
    const std::string first = WD().Monsters().ZoneList("emerald_plains")->monsterIds.front();
    CHECK(first == "slime_green");
  }
}  // TEST_SUITE

// =====================================================================================================================
// Pets / homestead: Chapter 1 embers (from pets_test.cpp)
// =====================================================================================================================

TEST_SUITE("pets") {
  using namespace abyss::test::pets_fx;

  TEST_CASE("embers: the Chapter 1 quests pay 20 in total (4.4.1)") {
    int32_t total = 0;
    for (const QuestDef& q : PD().Quests().quests) {
      if (q.zone != "emerald_plains") continue;
      total += EmbersForQuest(HT(), q.category, q.rewards.hasEmbers, q.rewards.embers);
    }
    CHECK(total == 20);
  }
}  // TEST_SUITE

// =====================================================================================================================
// Items: the Chapter 1 wandering merchant (from items_test.cpp)
// =====================================================================================================================

// ---- split from mixed tests (their systems part stays in items_test.cpp) --------------------------------------------

TEST_SUITE("items") {
  using namespace abyss::test::items_fx;

  // Split from "wandering merchant (12.7, FIX Q12): every event id maps to a base of the zone's tier": the Chapter 1
  // zone's wares, and buying from it works like any shop.
  TEST_CASE("wandering merchant: the emerald_plains wares (12.7) and buying from it") {
    ItmWorld w;
    w.h.session.currentMap = "emerald_plains";
    w.shop.OpenWanderingMerchant(D().World().randomEvents.ForZone("emerald_plains")->merchantItems, 1.0);
    std::vector<std::string> got;
    for (const ShopWare& ware : w.shop.State().wares) got.push_back(ware.baseId);
    CHECK(got == std::vector<std::string>{"w_short_sword", "a_leather_armor", "c_hp_potion_s", "c_mp_potion_s"});
    // Buying from it works like any shop.
    w.hero.SetGold(1000);
    CHECK(w.shop.Buy(2) == InvResult::Ok);
    CHECK(w.inv.Items().CountOf("c_hp_potion_s") == 1);
  }
}  // TEST_SUITE

// =====================================================================================================================
// GameSim: the Chapter 1 hidden-area rewards (from sim_test.cpp)
// =====================================================================================================================

TEST_SUITE("sim") {
  using namespace abyss::test::sim_fx;

  TEST_CASE("hidden-area reward props respawn for discovered areas and claim through RewardService (Q5)") {
    auto sim = StartSim(ClassId::Warrior, 5);
    SaveData save;
    sim->BuildSave(save, 0);
    REQUIRE(save.player.currentMap == "emerald_plains");
    save.discoveredHiddenAreas = {"hidden_ep_elven_cache"};
    std::string err;
    REQUIRE(sim->LoadGame(save, &err) == SaveError::None);
    const LoreSystem& lore = *sim->Context().sys.lore;
    REQUIRE(lore.HiddenRewards().size() == 2);
    size_t markers = 0;
    for (const WorldMarkerView& m : sim->View().markers) {
      if (m.kind == MarkerKind::HiddenReward) ++markers;
    }
    CHECK(markers == 2);
    const int64_t gold = sim->View().hero.gold;
    CHECK(sim->Context().sys.lore->ClaimHiddenReward("hidden_ep_elven_cache", 1));
    CHECK(sim->Context().sys.hero->Gold() == gold + 200);
    CHECK(lore.IsRewardClaimed("hidden_ep_elven_cache", 1));
    CHECK_FALSE(sim->Context().sys.lore->ClaimHiddenReward("hidden_ep_elven_cache", 1));
    CHECK(lore.HiddenRewards().size() == 1);

    SaveData after;
    sim->BuildSave(after, 0);
    CHECK(after.hiddenRewardsClaimed == std::vector<std::string>{"hidden_ep_elven_cache#1"});
    REQUIRE(sim->LoadGame(after, &err) == SaveError::None);
    REQUIRE(sim->Context().sys.lore->HiddenRewards().size() == 1);  // only the unclaimed chest comes back
    CHECK(sim->Context().sys.lore->HiddenRewards()[0].rewardIndex == 0);
  }
}  // TEST_SUITE

// =====================================================================================================================
// Data: table sizes and the Chapter 1 spot checks (from data_test.cpp)
// =====================================================================================================================

// ---- split from mixed tests (their systems part stays in data_test.cpp) ---------------------------------------------

TEST_SUITE("data") {
  // Split from "monster tables": the roster size and the Chapter 1 spot checks.
  TEST_CASE("monster tables: roster size, goblin, shaman (M5), the emerald_plains list, mini-boss and hunt") {
    const MonsterTables& m = test::RealData().Monsters();
    CHECK(m.defs.size() == 29);
    const MonsterDef* goblin = m.Find("goblin");
    REQUIRE(goblin);
    CHECK(goblin->hp == 55);
    CHECK(goblin->damage == 8);
    CHECK(goblin->defense == 4);
    CHECK(goblin->attackSpeedMs == 1200);
    CHECK_FALSE(goblin->isRanged);
    CHECK(goblin->nameKey == "data.monster.goblin");
    const MonsterDef* shaman = m.Find("miniboss_goblin_shaman");
    REQUIRE(shaman);
    CHECK(shaman->attackRange == 3);  // M5
    CHECK(shaman->isRanged);
    CHECK(m.FindForZone("emerald_plains", "slime_green") != nullptr);
    const MiniBossEntry* mb = m.MiniBossFor("emerald_plains");
    REQUIRE(mb);
    CHECK(mb->spawn == TilePos{60, 55});
    CHECK(m.MiniBossDialogue("miniboss_goblin_shaman")->nodes.size() == 3);
    CHECK(m.hunts.size() == 17);
    const HuntDef* thief = m.FindHunt("hunt_pendant_thief");
    REQUIRE(thief);
    CHECK(thief->defNormal.hp == 165);
    CHECK(thief->defNormal.damage == 10);
    CHECK(thief->revealAfterPrevious);
  }

  // Split from "world tables": the emerald_plains map facts, its random-event pool and the first camp's NPCs.
  TEST_CASE("world tables: emerald_plains map facts, ambush count, the first camp's NPCs (I7)") {
    const WorldTables& w = test::RealData().World();
    const MapDef* ep = w.FindMap("emerald_plains");
    REQUIRE(ep);
    CHECK(ep->cols == 120);
    CHECK(ep->generated);
    CHECK(ep->generatorSeed == 12345);
    CHECK(ep->generatorTheme == MapTheme::Plains);
    CHECK(ep->generatorAvoid.size() == 28);
    REQUIRE(ep->camps.size() == 2);
    CHECK(ep->camps[0].pos == TilePos{15, 15});
    CHECK(ep->playerStart == TilePos{15, 22});
    CHECK(ep->levelMin == 1);
    CHECK(ep->safeZoneRadiusEffective == 9);
    CHECK(ep->orderIndex == 0);
    CHECK(w.randomEvents.ForZone("emerald_plains")->ambushCountMin == 3);
    // I7: the stash keeper stands in the Chapter 1 camp (appended slot).
    CHECK(ep->camps[0].npcs == std::vector<std::string>{"blacksmith", "merchant", "quest_elder", "stash"});
  }

  // Split from "quests, npcs, dialogue, story, lore": the table sizes and the Chapter 1 spot checks.
  TEST_CASE("quests, npcs, dialogue, story, lore: table sizes and the Chapter 1 quests, elder, beats and lore") {
    const DataStore& d = test::RealData();
    const QuestTables& q = d.Quests();
    CHECK(q.quests.size() == 58);
    const QuestDef* slimes = q.Find("q_kill_slimes");
    REQUIRE(slimes);
    CHECK(slimes->category == QuestCategory::Main);
    REQUIRE(slimes->objectives.size() == 1);
    CHECK(slimes->objectives[0].type == ObjectiveType::Kill);
    CHECK(slimes->objectives[0].targetId == "slime_green");
    CHECK(slimes->objectives[0].required == 10);
    CHECK(slimes->giverNpcId == "quest_elder");
    CHECK(slimes->embersOnTurnIn == 2);
    const QuestDef* chief = q.Find("q_find_goblin_chief");
    REQUIRE(chief);
    CHECK(chief->questArea.col == 15);  // Q4 data fix: centred on the chief's spawn
    CHECK(chief->questArea.row == 95);
    CHECK(chief->rewards.choices.size() == 2);
    const QuestDef* herbs = q.Find("q_herb_gathering");
    REQUIRE(herbs);
    CHECK(herbs->objectives[0].sourceKind == ItemSourceKind::Gather);
    CHECK(herbs->objectives[0].gatherCount == 7);
    CHECK(q.achievements.size() == 12);
    CHECK(q.ItemKindFor("mat_herb") == "herb");

    CHECK(d.Npcs().npcs.size() == 27);
    CHECK(d.Npcs().GiverOf("q_kill_slimes")->id == "quest_elder");
    CHECK(d.Dialogues().trees.size() == 10);
    const DialogueTree* elder = d.Dialogues().Find("quest_elder");
    REQUIRE(elder);
    CHECK(elder->FindNode(elder->startNodeId) != nullptr);

    const StoryScript& s = d.Story();
    CHECK(s.cutscenes.size() == 22);
    CHECK(s.triggers.size() == 16);
    CHECK(s.triggers[0].subjectId == "q_kill_slimes");
    CHECK(s.BossIntroFor("goblin_chief")->cutscene == "cs_boss_goblin_chief");
    const Cutscene* mark = s.FindCutscene("cs_ep_mark");
    REQUIRE(mark);
    CHECK(mark->steps[0].kind == StoryStepKind::Focus);
    CHECK(mark->steps[0].target.kind == StoryActorKind::Npc);
    CHECK(s.ChapterFor("emerald_plains") != nullptr);
    CHECK(d.Lore().entries.size() == 21);
    CHECK(d.Lore().ForZone("emerald_plains").size() == 4);
  }
}  // TEST_SUITE

// =====================================================================================================================
// The content layer itself: the TestContent.h handles, the synthetic fixtures' provenance, the isolation gate
// =====================================================================================================================

namespace {

bool SrcIdChar(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }

// One string literal of a C++ source: its contents (escapes kept as written) and whether it is a doctest title (the
// first argument of TEST_SUITE / TEST_CASE / SUBCASE), which names a test rather than pinning data.
struct SrcLiteral {
  std::string text;
  bool title = false;
};

// The string literals of a C++ source, raw strings included; comments and character literals are skipped.
std::vector<SrcLiteral> SrcStringLiterals(const std::string& src) {
  std::vector<SrcLiteral> out;
  const size_t n = src.size();
  const auto titled = [&src](size_t quote) {
    size_t k = quote;
    while (k > 0 && (src[k - 1] == ' ' || src[k - 1] == '\n')) --k;
    if (k == 0 || src[k - 1] != '(') return false;
    --k;
    for (const std::string_view macro : {std::string_view("TEST_SUITE"), std::string_view("TEST_CASE"),
                                         std::string_view("SUBCASE")}) {
      if (k >= macro.size() && src.compare(k - macro.size(), macro.size(), macro) == 0) return true;
    }
    return false;
  };
  size_t i = 0;
  while (i < n) {
    const char c = src[i];
    const char next = i + 1 < n ? src[i + 1] : '\0';
    if (c == '/' && next == '/') {
      i = src.find('\n', i);
      if (i == std::string::npos) break;
    } else if (c == '/' && next == '*') {
      i = src.find("*/", i + 2);
      if (i == std::string::npos) break;
      i += 2;
    } else if (c == '\'') {
      if (i > 0 && SrcIdChar(src[i - 1])) {  // a digit separator (1'000)
        ++i;
        continue;
      }
      size_t j = i + 1;
      while (j < n && src[j] != '\'') j += src[j] == '\\' ? 2 : 1;
      i = j + 1;
    } else if (c == 'R' && next == '"' && (i == 0 || !SrcIdChar(src[i - 1]))) {
      const size_t open = src.find('(', i + 2);
      if (open == std::string::npos) break;
      const std::string close = ")" + src.substr(i + 2, open - i - 2) + "\"";
      const size_t end = src.find(close, open + 1);
      if (end == std::string::npos) break;
      out.push_back(SrcLiteral{src.substr(open + 1, end - open - 1), titled(i)});
      i = end + close.size();
    } else if (c == '"') {
      size_t j = i + 1;
      while (j < n && src[j] != '"' && src[j] != '\n') j += src[j] == '\\' ? 2 : 1;
      out.push_back(SrcLiteral{src.substr(i + 1, (std::min)(j, n) - i - 1), titled(i)});
      i = j + 1;
    } else {
      ++i;
    }
  }
  return out;
}

// `id` stands in `text` as a whole token: "goblin", "kill:goblin", "data.monster.goblin" and "hidden_x#1" spell their
// ids; "monster_goblin" (an art key) and "goblin_chief" do not spell "goblin".
bool SrcHasToken(std::string_view text, std::string_view id) {
  for (size_t p = text.find(id); p != std::string_view::npos; p = text.find(id, p + 1)) {
    const size_t end = p + id.size();
    if ((p == 0 || !SrcIdChar(text[p - 1])) && (end == text.size() || !SrcIdChar(text[end]))) return true;
  }
  return false;
}

// The Chapter 1 ids a source spells inside its string literals (doctest titles excepted).
std::vector<std::string> SrcSpelledIds(const std::string& src) {
  std::vector<std::string> out;
  for (const SrcLiteral& lit : SrcStringLiterals(src)) {
    if (lit.title) continue;
    for (const std::string_view id : test::ch1::kPinnedIds) {
      if (SrcHasToken(lit.text, id)) out.emplace_back(id);
    }
  }
  return out;
}

}  // namespace

TEST_SUITE("data") {
  TEST_CASE("TestContent.h: the ch1 handles name the roles the systems tests use them for") {
    const DataStore& d = test::RealData();
    CHECK(d.World().defaultMap == test::ch1::kZone);
    const MapDef* map = d.FindMap(test::ch1::kZone);
    REQUIRE(map != nullptr);
    REQUIRE_FALSE(map->camps.empty());
    CHECK(map->camps[0].pos == test::ch1::kCamp);
    CHECK(map->playerStart == test::ch1::kPlayerStart);
    REQUIRE(map->exits.size() == 1);
    CHECK(map->exits[0].pos == test::ch1::kExit);
    // The elder stands in camp 1 with the dialogue tree; the herbalist (a shop) and the wanderer (linear lines) in the
    // field.
    const std::vector<std::string>& camp1 = map->camps[0].npcs;
    CHECK(std::find(camp1.begin(), camp1.end(), test::ch1::kElder) != camp1.end());
    CHECK(d.Dialogues().Find(test::ch1::kElder) != nullptr);
    std::set<std::string> field;
    for (const FieldNpcDef& f : map->fieldNpcs) field.insert(f.npcId);
    CHECK(field.count(test::ch1::kHerbalist) == 1);
    CHECK(field.count(test::ch1::kWanderer) == 1);
    const NpcDef* herbalist = d.FindNpc(test::ch1::kHerbalist);
    REQUIRE(herbalist != nullptr);
    CHECK_FALSE(herbalist->shopItems.empty());
    const NpcDef* wanderer = d.FindNpc(test::ch1::kWanderer);
    REQUIRE(wanderer != nullptr);
    CHECK(wanderer->dialogueTreeId.empty());
    CHECK_FALSE(wanderer->dialogue.empty());
    // The main-chain slots are the zone's main quests in data order; M3 opens the tower, M5 completes the chapter and
    // sends the herb_garden ally, M4 is the story boss quest.
    std::vector<std::string> mains;
    for (const QuestDef& q : d.Quests().quests) {
      if (q.zone == test::ch1::kZone && q.category == QuestCategory::Main) mains.push_back(q.id);
    }
    CHECK(mains == std::vector<std::string>{test::ch1::kQuestM1, test::ch1::kQuestM2, test::ch1::kQuestM3,
                                            test::ch1::kQuestM4, test::ch1::kQuestM5});
    CHECK(d.Homestead().towerUnlockQuest == test::ch1::kQuestM3);
    REQUIRE(d.Homestead().FindBuilding("herb_garden") != nullptr);
    CHECK(d.Homestead().FindBuilding("herb_garden")->unlockQuest == test::ch1::kQuestM5);
    const MonsterAiDef& ai = d.Monsters().ai;
    CHECK(ai.storyBossId == test::ch1::kStoryBoss);
    CHECK(ai.storyBossNotAfterQuestTurnIn == test::ch1::kQuestM4);
    CHECK(ai.chapterCompleteQuest == test::ch1::kQuestM5);
    REQUIRE(d.Audio().music.BossScore(test::ch1::kStoryBoss) != nullptr);
    // (from world_test.cpp "10.4 rule 4") the story boss is the one scored boss, with the Chapter 1 boss track.
    REQUIRE(d.Audio().music.bossMusic.size() == 1);
    CHECK(d.Audio().music.bossMusic[0].first == test::ch1::kStoryBoss);
    CHECK(*d.Audio().music.BossScore(test::ch1::kStoryBoss) == "boss_ch1");
  }

  // Every test file outside the content layer (this file, playthrough_test.cpp, TestContent.h) tests systems: it may
  // use the TestContent.h handles and fixtures, never spell a Chapter 1 content id, alone or inside a key, in a string
  // literal (comments and test titles may name content). The content switch then re-pins one place.
  TEST_CASE("content isolation: no test file outside the content layer spells a Chapter 1 content id") {
    // The scanner: literals and raw strings count, comments, character literals and titles do not; ids are tokens.
    const std::string sample =
        "// \"goblin\" in a comment\n"
        "TEST_CASE(\"goblin hits\") {\n"
        "  const char q = '\"'; Spawn(\"monster_goblin\"); Find(\"goblin_chief\");\n"
        "  Key(\"data.monster.slime_green\"); Raw(R\"x(\"cs_ep_mark\":1)x\"); /* \"lore_ep_01\" */\n"
        "}\n";
    const std::vector<SrcLiteral> lits = SrcStringLiterals(sample);
    REQUIRE(lits.size() == 5);
    CHECK(lits[0].title);
    CHECK(lits[1].text == "monster_goblin");
    CHECK(lits[4].text == "\"cs_ep_mark\":1");
    CHECK(SrcSpelledIds(sample) == std::vector<std::string>{"goblin_chief", "slime_green", "cs_ep_mark"});

    namespace fs = std::filesystem;
    const fs::path dir = fs::path(test::UnrealDir()) / "CoreTests" / "tests";
    std::error_code ec;
    std::vector<fs::path> files;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
      const fs::path p = it->path();
      if (p.extension() == ".cpp" || p.extension() == ".h") files.push_back(p);
    }
    REQUIRE_FALSE(ec);
    std::sort(files.begin(), files.end());
    const std::set<std::string> layer{"content_ch1_test.cpp", "playthrough_test.cpp", "TestContent.h"};
    std::vector<std::string> unreadable, spelled;
    size_t scanned = 0;
    for (const fs::path& p : files) {
      const std::string name = p.filename().string();
      if (layer.count(name) != 0) continue;
      const std::string text = test::ReadFile(p.string());
      if (text.empty()) {
        unreadable.push_back(name);
        continue;
      }
      ++scanned;
      for (const std::string& id : SrcSpelledIds(text)) spelled.push_back(name + ": " + id);
    }
    std::string list;
    for (const std::string& s : spelled) list += s + "\n";
    INFO("Chapter 1 content ids spelled outside the content layer:\n" << list);
    CHECK(scanned >= 17);  // the area tests, the fixture headers and the harness
    CHECK(unreadable.empty());
    CHECK(spelled.empty());
  }
}  // TEST_SUITE

TEST_SUITE("monsters") {
  using namespace abyss::test::monsters_fx;

  // T0 provenance: the combat and AI vectors were cut on these definitions; the fixtures keep their numbers when the
  // roster changes (the content switch drops or re-points this check, never the fixtures).
  TEST_CASE("TestContent.h: the synthetic fixtures carry today's goblin, chief, slime and shaman numbers") {
    struct Row {
      const MonsterDef* fx;
      const char* from;
    };
    const Row rows[] = {{&test::fixture::Grunt(), "goblin"},
                        {&test::fixture::Brute(), "goblin_chief"},
                        {&test::fixture::Ooze(), "slime_green"},
                        {&test::fixture::Caster(), "miniboss_goblin_shaman"}};
    for (const Row& r : rows) {
      CAPTURE(r.from);
      const MonsterDef& d = MDef(r.from);
      const MonsterDef& f = *r.fx;
      CHECK(f.id != d.id);
      CHECK(f.nameKey == "data.monster." + f.id);
      CHECK(f.level == d.level);
      CHECK(f.hp == d.hp);
      CHECK(f.damage == d.damage);
      CHECK(f.defense == d.defense);
      CHECK(f.speed == d.speed);
      CHECK(f.aggroRange == d.aggroRange);
      CHECK(f.attackRange == d.attackRange);
      CHECK(f.attackSpeedMs == d.attackSpeedMs);
      CHECK(f.expReward == d.expReward);
      CHECK(f.goldMin == d.goldMin);
      CHECK(f.goldMax == d.goldMax);
      CHECK(f.spriteKey == d.spriteKey);
      CHECK(f.elite == d.elite);
      CHECK(f.isMiniBoss == d.isMiniBoss);
      CHECK(f.isSubDungeonMiniBoss == d.isSubDungeonMiniBoss);
      CHECK(f.animCategory == d.animCategory);
      CHECK(f.isRanged == d.isRanged);
      CHECK(f.projectileColor == d.projectileColor);
      CHECK(f.onHitStatus.empty());
      CHECK(d.onHitStatus.empty());
    }
  }
}  // TEST_SUITE
