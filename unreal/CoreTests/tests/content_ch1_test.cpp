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
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
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
#include "SourceScan.h"
#include "TestContent.h"
#include "TestUtil.h"
#include "WorldFixtures.h"
#include "abyss/base/Json.h"
#include "abyss/combat/HitFeedback.h"
#include "abyss/data/DataStore.h"
#include "abyss/monsters/MonsterAbilities.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/world/WorldState.h"
#include "doctest/doctest.h"

using namespace abyss;

// =====================================================================================================================
// Monsters: the Chapter 1 roster, hunts, mini-boss, story boss, zone population (from monsters_test.cpp)
// Moved verbatim at T0; the systems tests there run on the synthetic fixtures of TestContent.h. Re-pinned by package 1B
// part 1 (dark remake world data, Docs/dark-remake/monsters.md 2): the four families, the Corpseherd (the shaman's slot,
// range 3.0 authored), Lord-Marshal Valdras (the story boss, Wave 1 single phase) and the hunts Hungerbell, Dreck and
// Harke (the slots of Gruk and Sneek).
// =====================================================================================================================

TEST_SUITE("monsters") {
  using namespace abyss::test::monsters_fx;

  TEST_CASE("Chapter 1 roster 2.1: base stats, speed in tiles/s, ranged rule (M5: the Corpseherd ranged at 3.0)") {
    struct Row {
      const char* id;
      int32_t level;
      double hp, dmg, def, speed, aggro, range, atkSpeed, exp, g0, g1;
      bool elite;
      double tilesPerSec;
    };
    const Row rows[] = {
        // monsters.md 2.0; exp as built (1B review, DECISIONS DR9): round(0.75 x the 2.0 Exp) for the regulars, Valdras
        // 110. Valdras as tuned by package 2F and its review (Wave 2, three phases: 9000 HP / 20 dmg, every class's
        // median fight in DR9's 30-180 s band; implementation-plan.md "2F review outcomes"); the Corpseherd 600 HP (2F
        // review: Keen at the main path's Lv 10); the roll-call standard-bearer is his phase-2 summon.
        {"breachclot", 1, 28, 5, 2, 38, 4, 1.2, 1500, 8, 2, 4, false, 1.14},
        {"risen_footman", 2, 50, 7, 5, 32, 4, 1.5, 1600, 11, 2, 5, false, 0.96},
        {"gravegnawer", 3, 42, 7, 3, 66, 6, 1.3, 950, 13, 1, 4, false, 1.98},
        {"cinderbloat", 4, 40, 4, 2, 30, 5, 1.2, 1800, 14, 1, 3, false, 0.90},
        {"risen_arbalest", 5, 46, 11, 4, 34, 7, 6.0, 2200, 18, 3, 6, false, 1.02},
        {"oath_zealot", 6, 90, 11, 6, 52, 6, 1.5, 1100, 23, 4, 8, false, 1.56},
        {"deserter", 6, 80, 12, 7, 56, 5, 1.6, 1150, 21, 6, 12, false, 1.68},
        {"deserter_drummer", 6, 70, 8, 5, 50, 6, 1.5, 1400, 21, 4, 9, false, 1.50},
        {"risen_shieldbearer", 7, 130, 12, 14, 30, 5, 1.6, 1700, 26, 3, 7, false, 0.90},
        {"dead_standard_bearer", 7, 140, 8, 8, 30, 6, 1.6, 1700, 30, 4, 8, false, 0.90},
        {"bonegorger", 7, 210, 17, 9, 42, 6, 2.0, 1700, 36, 6, 12, false, 1.26},
        {"miniboss_corpseherd", 6, 600, 15, 10, 45, 7, 3.0, 1300, 95, 15, 30, true, 1.35},
        {"valdras", 9, 9000, 20, 12, 48, 8, 2.0, 1300, 110, 30, 60, true, 1.44},
        {"rollcall_standard_bearer", 7, 140, 8, 8, 30, 6, 1.6, 1700, 30, 4, 8, false, 0.90},
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
    CHECK(MDef("miniboss_corpseherd").isMiniBoss);
    CHECK(MDef("miniboss_corpseherd").isRanged);  // M5's role, authored (no exporter override any more)
    CHECK(MD().Monsters().overrides.empty());
    CHECK(MDef("risen_arbalest").isRanged);        // the first ranged regular
    CHECK_FALSE(MDef("valdras").isRanged);
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
        {"breachclot", 4, 3, 2, 6.0, 1.44, 2.12},
        {"risen_footman", 5, 3, 5, 6.0, 1.8, 2.525},
        {"gravegnawer", 5, 6, 4, 9.0, 1.56, 2.255},
        {"cinderbloat", 3, 3, 4, 7.5, 1.44, 2.12},
        {"risen_arbalest", 8, 3, 4, 10.5, 7.2, 8.6},
        {"oath_zealot", 8, 5, 9, 9.0, 1.8, 2.525},
        {"deserter", 9, 5, 8, 7.5, 1.92, 2.66},
        {"deserter_drummer", 6, 5, 7, 9.0, 1.8, 2.525},
        {"risen_shieldbearer", 9, 3, 13, 7.5, 1.92, 2.66},
        {"dead_standard_bearer", 6, 3, 14, 9.0, 1.92, 2.66},
        {"bonegorger", 13, 4, 21, 9.0, 2.4, 3.2},
        {"miniboss_corpseherd", 12, 4, 60, 10.5, 3.6, 4.55},  // range 3.0
        {"valdras", 16, 4, 900, 12.0, 2.4, 3.2},
        {"rollcall_standard_bearer", 6, 3, 14, 9.0, 1.92, 2.66},
        {"hunt_hungerbell", 13, 4, 42, 10.5, 2.4, 3.2},  // 420 HP / 17 dmg (hp x2, dmg x1.0)
        {"hunt_dreck", 11, 5, 24, 10.5, 1.92, 2.66},
        {"hunt_harke", 15, 5, 40, 10.5, 1.92, 2.66},
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
    // Math.round(x * mul): hp / damage x1.5 / x2, defense x1.3 / x1.6, exp and gold x2 / x3.
    const Row rows[] = {
        {"breachclot", Difficulty::Nightmare, 42, 8, 3, 16, 4, 8},
        {"breachclot", Difficulty::Hell, 56, 10, 3, 24, 6, 12},
        {"risen_footman", Difficulty::Nightmare, 75, 11, 7, 22, 4, 10},
        {"risen_footman", Difficulty::Hell, 100, 14, 8, 33, 6, 15},
        {"gravegnawer", Difficulty::Nightmare, 63, 11, 4, 26, 2, 8},
        {"gravegnawer", Difficulty::Hell, 84, 14, 5, 39, 3, 12},
        {"cinderbloat", Difficulty::Nightmare, 60, 6, 3, 28, 2, 6},
        {"cinderbloat", Difficulty::Hell, 80, 8, 3, 42, 3, 9},
        {"risen_arbalest", Difficulty::Nightmare, 69, 17, 5, 36, 6, 12},
        {"risen_arbalest", Difficulty::Hell, 92, 22, 6, 54, 9, 18},
        {"oath_zealot", Difficulty::Nightmare, 135, 17, 8, 46, 8, 16},
        {"oath_zealot", Difficulty::Hell, 180, 22, 10, 69, 12, 24},
        {"deserter", Difficulty::Nightmare, 120, 18, 9, 42, 12, 24},
        {"deserter", Difficulty::Hell, 160, 24, 11, 63, 18, 36},
        {"risen_shieldbearer", Difficulty::Nightmare, 195, 18, 18, 52, 6, 14},
        {"risen_shieldbearer", Difficulty::Hell, 260, 24, 22, 78, 9, 21},
        {"dead_standard_bearer", Difficulty::Nightmare, 210, 12, 10, 60, 8, 16},
        {"dead_standard_bearer", Difficulty::Hell, 280, 16, 13, 90, 12, 24},
        {"bonegorger", Difficulty::Nightmare, 315, 26, 12, 72, 12, 24},
        {"bonegorger", Difficulty::Hell, 420, 34, 14, 108, 18, 36},
        {"miniboss_corpseherd", Difficulty::Nightmare, 900, 23, 13, 190, 30, 60},
        {"miniboss_corpseherd", Difficulty::Hell, 1200, 30, 16, 285, 45, 90},
        {"valdras", Difficulty::Nightmare, 13500, 30, 16, 220, 60, 120},
        {"valdras", Difficulty::Hell, 18000, 40, 19, 330, 90, 180},
        {"hunt_hungerbell", Difficulty::Nightmare, 630, 26, 14, 216, 36, 72},
        {"hunt_hungerbell", Difficulty::Hell, 840, 34, 18, 324, 54, 108},
        {"hunt_dreck", Difficulty::Nightmare, 360, 21, 10, 126, 36, 72},
        {"hunt_dreck", Difficulty::Hell, 480, 28, 13, 189, 54, 108},
        {"hunt_harke", Difficulty::Nightmare, 600, 29, 10, 210, 36, 72},
        {"hunt_harke", Difficulty::Hell, 800, 38, 13, 315, 54, 108},
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

  TEST_CASE("makeHuntDefinition (vector 12): Harke, Dreck and Hungerbell; every exported hunt matches the TS defsNormal") {
    const MonsterDef& deserter = MDef("deserter");
    const MonsterDef harke = MakeHuntDefinition(deserter, MHunt("hunt_harke"), MAi(), "Harke");
    CHECK(harke.id == "hunt_harke");
    CHECK(harke.name == "Harke");
    CHECK(harke.nameKey == "data.monster.hunt_harke");
    CHECK(harke.hp == 400);        // 80 x 5
    CHECK(harke.damage == 19);     // round(12 x 1.6)
    CHECK(harke.defense == 8);     // round(7 x 1.2)
    CHECK(harke.expReward == 105); // 21 x max(3, 5)
    CHECK(harke.goldMin == 18);
    CHECK(harke.goldMax == 36);
    CHECK(harke.aggroRange == 7);
    CHECK(harke.elite);
    CHECK(harke.isMiniBoss);
    CHECK(harke.spriteKey == deserter.spriteKey);
    CHECK(harke.speed == deserter.speed);
    CHECK(harke.attackRange == deserter.attackRange);
    CHECK(harke.attackSpeedMs == deserter.attackSpeedMs);
    CHECK(harke.level == deserter.level);
    const MonsterDef dreck = MakeHuntDefinition(deserter, MHunt("hunt_dreck"), MAi(), "Dreck");
    CHECK(dreck.hp == 240);
    CHECK(dreck.damage == 14);
    CHECK(dreck.defense == 8);
    CHECK(dreck.expReward == 63);
    CHECK(ScaleMonsterForDifficulty(dreck, MDiff(), Difficulty::Nightmare).hp == 360);
    const MonsterDef hungerbell = MakeHuntDefinition(MDef("bonegorger"), MHunt("hunt_hungerbell"), MAi(), "Hungerbell");
    CHECK(hungerbell.hp == 420);     // 210 x 2 (1B review: x2, not x3)
    CHECK(hungerbell.damage == 17);  // 17 x 1.0
    CHECK(hungerbell.defense == 11);
    CHECK(hungerbell.expReward == 108);  // 36 x max(3, 2)
    CHECK(hungerbell.aggroRange == 7);
    CHECK(hungerbell.level == 7);
    // Defaults: hpMul 4, dmgMul 1.5 when the hunt omits them.
    HuntDef bare = MHunt("hunt_harke");
    bare.hasHpMul = bare.hasDmgMul = false;
    const MonsterDef def = MakeHuntDefinition(deserter, bare, MAi(), "x");
    CHECK(def.hp == 320);
    CHECK(def.damage == 18);
    CHECK(def.expReward == 84);
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
      // Its base's abilities, then its own (2F quest.huntAbilities: Hungerbell's supper bell).
      REQUIRE(d.abilities.size() == h.defNormal.abilities.size());
      for (size_t i = 0; i < d.abilities.size(); ++i) CHECK(d.abilities[i].id == h.defNormal.abilities[i].id);
    }
    CHECK(MHunt("hunt_hungerbell").abilities.size() == 1);
    CHECK(MHunt("hunt_harke").abilities.empty());
    CHECK(MHunt("hunt_dreck").abilities.empty());
  }

  TEST_CASE("isHuntDue (vector 13): bounty on accept, tracked quarry after its trail, done / inactive -> not due") {
    const QuestDef* pendant = MD().FindQuest("q_rf_tag_thief");
    const QuestDef* bounty = MD().FindQuest("q_rf_oathless");
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
    const HuntDef& harke = MHunt("hunt_harke");
    const HuntDef& dreck = MHunt("hunt_dreck");
    CHECK(IsHuntDue(*bounty, progressOf(*bounty, {}), harke));
    CHECK_FALSE(IsHuntDue(*bounty, progressOf(*bounty, {0}), harke));
    CHECK_FALSE(IsHuntDue(*bounty, progressOf(*bounty, {}, QuestStatus::Completed), harke));
    CHECK_FALSE(IsHuntDue(*pendant, progressOf(*pendant, {0, 1}), dreck));
    CHECK(IsHuntDue(*pendant, progressOf(*pendant, {0, 1, 2}), dreck));
    CHECK_FALSE(IsHuntDue(*pendant, progressOf(*pendant, {0, 1, 2, 3}), dreck));  // kill objective done
    CHECK(HuntObjectiveIndex(*pendant, "hunt_dreck") == 3);
    CHECK(HuntObjectiveIndex(*pendant, "no_such_hunt") == -1);
    // A short (migrated) progress record counts missing objectives as 0.
    QuestProgress shortRec;
    shortRec.questId = bounty->id;
    CHECK(IsHuntDue(*bounty, shortRec, harke));

    // huntsToSpawn: this zone only, not already out, quest order then hunt order.
    const QuestProgress pp = progressOf(*pendant, {0, 1, 2});
    const QuestProgress bp = progressOf(*bounty, {});
    const std::vector<std::pair<const QuestDef*, const QuestProgress*>> open{{pendant, &pp}, {bounty, &bp}};
    std::vector<std::string> present;
    std::vector<DueHunt> due = HuntsToSpawn(open, "emerald_plains", present, MD().Monsters().hunts);
    REQUIRE(due.size() == 2);
    CHECK(due[0].hunt->huntId == "hunt_dreck");
    CHECK(due[1].hunt->huntId == "hunt_harke");
    CHECK(due[0].quest == pendant);
    present = {"hunt_harke"};
    due = HuntsToSpawn(open, "emerald_plains", present, MD().Monsters().hunts);
    REQUIRE(due.size() == 1);
    CHECK(due[0].hunt->huntId == "hunt_dreck");
    CHECK(HuntsToSpawn(open, "twilight_forest", {}, MD().Monsters().hunts).empty());
  }

  TEST_CASE("elite affix application 2.4 (vector 16): every affix on Valdras, the Corpseherd and two hunt leaders") {
    struct Row {
      const char* id;
      std::array<std::array<int32_t, 5>, 8> v;  // hp, dmg, speed, def, str per affix in table order
    };
    // floor(x * mult) per affix (the mechanics are the web's; the remake re-voices and recolours them only). The eighth,
    // Unburied (Wave 2, package 2F), changes no number: it only makes the monster rise again once (K3).
    const Row rows[] = {
        {"valdras",
         {{{10800, 20, 48, 12, 16}, {9000, 20, 76, 12, 16}, {10350, 22, 48, 12, 17}, {11700, 27, 48, 13, 21},
           {10800, 22, 48, 12, 17}, {11250, 22, 48, 12, 17}, {10800, 21, 43, 13, 16}, {9000, 20, 48, 12, 16}}}},
        {"miniboss_corpseherd",
         {{{720, 15, 45, 10, 12}, {600, 15, 72, 10, 12}, {690, 16, 45, 10, 12}, {780, 20, 45, 11, 16},
           {720, 16, 45, 10, 12}, {750, 16, 45, 10, 12}, {720, 15, 40, 11, 12}, {600, 15, 45, 10, 12}}}},
        {"hunt_dreck",
         {{{288, 14, 56, 8, 11}, {240, 14, 89, 8, 11}, {276, 15, 56, 8, 12}, {312, 18, 56, 9, 14}, {288, 15, 56, 8, 12},
           {300, 15, 56, 8, 12}, {288, 14, 50, 8, 11}, {240, 14, 56, 8, 11}}}},
        {"hunt_harke",
         {{{480, 19, 56, 8, 15}, {400, 19, 89, 8, 15}, {459, 20, 56, 8, 16}, {520, 25, 56, 9, 20},
           {480, 20, 56, 8, 16}, {500, 20, 56, 8, 16}, {480, 19, 50, 8, 15}, {400, 19, 56, 8, 15}}}},
    };
    REQUIRE(MAffixes().order.size() == 8);
    CHECK(MAffixes().order[7] == EliteAffixType::Unburied);
    for (const Row& r : rows) {
      const MonsterDef base = MNormalDef(r.id);
      for (size_t i = 0; i < 8; ++i) {
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
    CHECK(ai.storyBossId == "valdras");
    CHECK(ai.storyBossNotAfterQuestTurnIn == "q_rf_marshal");  // M4, the Sleepless Marshal (slot q_find_goblin_chief)
    CHECK(ai.chapterCompleteQuest == "q_rf_last_toll");
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

  TEST_CASE("zone population 6.1 / 6.3: 55 monsters, jitter <= 3, walkable, outside camps, Valdras = story boss") {
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
    int32_t leaders = 0;
    for (const MonsterInstance& m : all) {
      CAPTURE(m.def.id);
      ++perDef[m.def.id];
      CHECK(m.IsAlive());
      CHECK(m.state == MonsterState::Idle);
      CHECK(rig.zone.Walkable(m.spawnAnchor.col, m.spawnAnchor.row));
      CHECK(m.pos == m.spawnAnchor.Center());
      // Within +-3 of its spawn entry's anchor (or on it), a member of that entry (its monsterId or one of its mix).
      REQUIRE(m.spawnEntry >= 0);
      REQUIRE(static_cast<size_t>(m.spawnEntry) < map.spawns.size());
      const MapSpawnDef& s = map.spawns[static_cast<size_t>(m.spawnEntry)];
      bool member = s.mix.empty() && s.monsterId == m.def.id;
      for (const MapSpawnMixDef& x : s.mix) member = member || x.monsterId == m.def.id;
      CHECK(member);
      CHECK(std::abs(s.pos.col - m.spawnAnchor.col) <= 3);
      CHECK(std::abs(s.pos.row - m.spawnAnchor.row) <= 3);
      CHECK_FALSE(rig.zone.InSafeZone(m.pos));
      if (m.packLeader) ++leaders;
      if (m.def.id == "valdras") {
        CHECK(m.role == MonsterRole::StoryBoss);
        CHECK(m.noRespawn);
        CHECK(m.AffixCount() == 1);  // emerald_plains: exactly one affix
      } else if (m.packLeader && m.def.id == "deserter") {
        // The Hollow Watch's leader (chapter1.md 17 W2): an elite with the zone's one affix.
        CHECK(m.role == MonsterRole::Regular);
        CHECK_FALSE(m.noRespawn);
        CHECK(m.AffixCount() == 1);
      } else {
        CHECK(m.role == MonsterRole::Regular);
        CHECK_FALSE(m.noRespawn);
        CHECK(m.affixes.empty());
        CHECK(m.maxHp == MDef(m.def.id).hp);
      }
    }
    // chapter1.md 17: 55 in the same 10 entries; Wave 2 mixes (package 2F) - the Weeping Ditch 3 bloats + 3 clots, the
    // Broken Redoubt 3 arbalests + 2 shieldbearers, the Dead Ranks 3 shieldbearers + 2 footmen + their standard-bearer
    // (leader), the Hollow 5 deserters (an elite leader) + the drummer, the midden a bonegorger + 2 gnawers.
    CHECK(map.spawns.size() == 10);
    CHECK(leaders == 2);
    CHECK(perDef["risen_footman"] == 10);
    CHECK(perDef["gravegnawer"] == 10);
    CHECK(perDef["breachclot"] == 10);
    CHECK(perDef["cinderbloat"] == 3);
    CHECK(perDef["oath_zealot"] == 5);
    CHECK(perDef["risen_arbalest"] == 3);
    CHECK(perDef["risen_shieldbearer"] == 5);
    CHECK(perDef["dead_standard_bearer"] == 1);
    CHECK(perDef["valdras"] == 1);
    CHECK(perDef["deserter"] == 5);
    CHECK(perDef["deserter_drummer"] == 1);
    CHECK(perDef["bonegorger"] == 1);
    CHECK(perDef.size() == 12);
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
      if (m.def.id == "gravegnawer") CHECK(m.maxHp == 63);
    }
  }

  TEST_CASE("M7 story boss: skipped after q_rf_marshal, back once the chapter is complete") {
    MonsterRig rig(3);
    QuestProgress chief;
    chief.questId = "q_rf_marshal";
    chief.status = QuestStatus::TurnedIn;
    chief.objectives.assign(MD().FindQuest(chief.questId)->objectives.size(), 1);
    rig.quests.Load({chief});
    if (!rig.quests.IsTurnedIn("q_rf_marshal")) {
      MESSAGE("QuestSystem::Load is not implemented yet (quests area): M7 runtime part skipped");
      return;
    }
    rig.monsters.SpawnZonePopulation();
    CHECK(rig.monsters.NearestAliveOfDef("valdras", {15, 95}) == kNoEntity);
    QuestProgress fin;
    fin.questId = "q_rf_last_toll";
    fin.status = QuestStatus::TurnedIn;
    fin.objectives.assign(MD().FindQuest(fin.questId)->objectives.size(), 1);
    rig.quests.Load({chief, fin});
    rig.monsters.OnZoneExit();
    rig.monsters.SpawnZonePopulation();
    const EntityId boss = rig.monsters.NearestAliveOfDef("valdras", {15, 95});
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
    CHECK(b.def.id == "miniboss_corpseherd");
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

  TEST_CASE("M5 as shipped: every authored wave of the Boneyard Gate spawns on walkable ground outside camp 1") {
    // chapter1.md M5 / DECISIONS DR4: the quest's own target, waves and approachDeg (not a synthetic arc), each wave's
    // spawn tiles on the r 8 half ring: walkable (a wave on a wall would stand stuck) and outside the camp's safe zone.
    const QuestDef* q = MD().FindQuest(test::ch1::kQuestM5);
    REQUIRE(q != nullptr);
    REQUIRE(q->hasDefendTarget);
    const DefendTargetDef& dt = q->defendTarget;
    REQUIRE(dt.hasApproach);
    REQUIRE(dt.waves.size() == static_cast<size_t>(dt.totalWaves));
    MonsterRig rig(91, Vec2(60, 60));
    REQUIRE(rig.ok);
    const Vec2 gate = dt.pos.Center();
    const Vec2 camp = test::ch1::kCamp.Center();
    for (size_t w = 0; w < dt.waves.size(); ++w) {
      DefendWaveSpec spec;
      spec.composition = dt.waves[w];
      spec.hasApproach = true;
      spec.approachDeg = dt.approachDeg;
      int32_t expected = 0;
      for (const DefendWaveEntry& e : dt.waves[w]) expected += e.count;
      const std::vector<EntityId> ids = rig.monsters.SpawnDefendWave(gate, static_cast<int32_t>(w), spec);
      CHECK(static_cast<int32_t>(ids.size()) == expected);
      for (EntityId id : ids) {
        const Vec2 pos = rig.M(id).pos;
        CAPTURE(w);
        CAPTURE(pos.x);
        CAPTURE(pos.y);
        CHECK(rig.zone.Walkable(static_cast<int32_t>(std::floor(pos.x)), static_cast<int32_t>(std::floor(pos.y))));
        CHECK_FALSE(rig.zone.InSafeZone(pos));
        CHECK(Dist(pos, camp) > rig.zone.SafeZoneRadius());
        CHECK(Dist(pos, gate) == doctest::Approx(MAi().defendWaveRadius).epsilon(0.15));
      }
    }
  }

  TEST_CASE("M3 needs the Corpseherd: killed before M3 is taken, she comes back when it is (SpawnMiniBossForQuests)") {
    // The Sheepfold (60,55) lies 7 tiles from Dreck's hunt (60,48): a hero who kills her before M3 could never finish
    // the chapter (once per visit, M7; the Red Field cannot be left in milestone 1).
    MonsterRig rig(5);
    rig.monsters.SpawnZonePopulation();
    rig.monsters.SpawnMiniBoss();
    const EntityId first = rig.monsters.MiniBoss();
    REQUIRE(first != kNoEntity);
    rig.monsters.SpawnMiniBossForQuests();  // nobody needs her yet: nothing happens
    CHECK(rig.monsters.MiniBoss() == first);
    const size_t before = rig.monsters.All().size();
    rig.monsters.ApplyDamage(first, 1e6, DamageFlags{});
    CHECK_FALSE(rig.M(first).IsAlive());
    rig.monsters.SpawnMiniBossForQuests();  // still nobody: once per visit holds
    CHECK(rig.monsters.MiniBoss() == kNoEntity);
    CHECK(rig.monsters.All().size() == before);
    QuestProgress m3;
    m3.questId = test::ch1::kQuestM3;
    m3.status = QuestStatus::Active;
    m3.objectives.assign(MD().FindQuest(m3.questId)->objectives.size(), 0);
    rig.quests.Load({m3});
    rig.monsters.SpawnMiniBossForQuests();
    const EntityId again = rig.monsters.MiniBoss();
    REQUIRE(again != kNoEntity);
    CHECK(again != first);
    const MonsterInstance& b = rig.M(again);
    CHECK(b.IsAlive());
    CHECK(b.def.id == "miniboss_corpseherd");
    CHECK(b.pos == Vec2(60, 55));
    CHECK(b.role == MonsterRole::ZoneMiniBoss);
    const size_t count = rig.monsters.All().size();
    rig.monsters.SpawnMiniBossForQuests();  // alive: no second one
    CHECK(rig.monsters.All().size() == count);
    // Once her objective is done nobody needs her any more.
    rig.monsters.ApplyDamage(again, 1e6, DamageFlags{});
    m3.objectives[1] = 1;
    rig.quests.Load({m3});
    const size_t afterKill = rig.monsters.All().size();
    rig.monsters.SpawnMiniBossForQuests();
    CHECK(rig.monsters.MiniBoss() == kNoEntity);
    CHECK(rig.monsters.All().size() == afterKill);
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
    CHECK(opened[0].monsterId == "miniboss_corpseherd");
    CHECK(rig.M(boss).state == MonsterState::Idle);
    const Vec2 frozenAt = rig.M(boss).pos;
    rig.hero->SetPosition(Vec2(60, 57));
    rig.Step(30);
    CHECK(rig.M(boss).state == MonsterState::Idle);  // the AI holds the boss
    CHECK(rig.M(boss).pos == frozenAt);
    SaveData save;
    rig.monsters.WriteSave(save);
    CHECK(save.miniBossDialogueSeen == std::vector<std::string>{"miniboss_corpseherd"});
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
    const QuestDef* pendant = MD().FindQuest("q_rf_tag_thief");
    const QuestDef* bounty = MD().FindQuest("q_rf_oathless");
    const std::vector<DueHunt> due{{pendant, &MHunt("hunt_dreck")}, {bounty, &MHunt("hunt_harke")}};
    rig.monsters.SpawnHunts(due, true);
    CHECK(rig.monsters.IsHuntPresent("hunt_dreck"));
    CHECK(rig.monsters.IsHuntPresent("hunt_harke"));
    const auto all = rig.monsters.All();
    REQUIRE(all.size() == 1 + 2 + 1 + 4);  // open world: every minion placement succeeds
    const MonsterInstance& thief = all[0];
    CHECK(thief.def.id == "hunt_dreck");
    CHECK(thief.huntId == "hunt_dreck");
    CHECK(thief.role == MonsterRole::HuntLeader);
    CHECK(thief.noRespawn);
    CHECK(thief.pos == Vec2(60, 48));
    CHECK(thief.visualScale == doctest::Approx(1.25));
    CHECK(thief.AffixCount() == 1);
    CHECK(thief.originalDef.hp == 240);
    CHECK(thief.def.nameKey == "data.monster.hunt_dreck");
    for (size_t i = 1; i <= 2; ++i) {
      CHECK(all[i].def.id == "deserter");
      CHECK(all[i].role == MonsterRole::HuntMinion);
      CHECK(all[i].affixes.empty());
      CHECK(std::abs(all[i].spawnAnchor.col - 60) <= 3);
      CHECK(std::abs(all[i].spawnAnchor.row - 48) <= 3);
    }
    CHECK(all[3].def.id == "hunt_harke");
    CHECK(all[3].pos == Vec2(82, 72));
    CHECK(all[3].originalDef.hp == 400);
    for (size_t i = 4; i <= 7; ++i) CHECK(all[i].def.id == "deserter");
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
    CHECK(rig.kills[0].defId == "hunt_dreck");
    CHECK(rig.kills[0].isMiniBoss);
    CHECK(rig.kills[0].role == MonsterRole::HuntLeader);
    CHECK_FALSE(rig.monsters.IsHuntPresent("hunt_dreck"));
    CHECK(rig.h.timers.Empty());
    // A silent spawn (zone entry / accept) has no announcement; a blocked spot falls back to the nearest ring tile.
    MonsterRig quiet(22, Vec2(60, 60));
    auto grid = std::make_shared<MTestGrid>(120, 120);
    grid->Block(82, 72);
    quiet.monsters.SetWorldForTesting(MGridWorld(grid, false));
    const std::vector<DueHunt> one{{bounty, &MHunt("hunt_harke")}};
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
    p.questId = "q_rf_oathless";
    p.status = QuestStatus::Active;
    p.objectives.assign(1, 0);
    rig.quests.Load({p});
    if (rig.quests.OpenQuests().empty()) {
      MESSAGE("QuestSystem::Load is not implemented yet (quests area): SpawnDueHunts runtime part skipped");
      return;
    }
    rig.monsters.SpawnDueHunts(false);
    CHECK(rig.monsters.IsHuntPresent("hunt_harke"));
    const size_t n = rig.monsters.All().size();
    rig.monsters.SpawnDueHunts(true);  // already out: nothing new
    CHECK(rig.monsters.All().size() == n);
    CHECK(MEvents<EvHuntRevealed>(rig.h.events).empty());
  }

  TEST_CASE("event spawns 6.4: ambush around a point, chasing, no affixes, never respawn, Events stream") {
    MonsterRig rig(31, Vec2(60, 60));
    rig.OpenWorld();
    const RngState aiBefore = rig.h.rng.Get(RngStream::Ai).GetState();
    const std::vector<std::string> ids{"risen_footman", "gravegnawer"};  // the Red Field's ambush pool (chapter1.md 13)
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
    s.h.rng.Get(RngStream::Events).Script({0.6, 0.0, 0.5});  // gravegnawer; angle 0; dist 2 + 0.5 * 3 = 3.5
    const std::vector<EntityId> one = s.monsters.SpawnAmbush(ids, 1, Vec2(40, 40), 2, 3, MonsterRole::RescueSpawn);
    REQUIRE(one.size() == 1);
    CHECK(s.M(one[0]).def.id == "gravegnawer");
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
    size_t alive = 0, herds = 0;
    for (const MonsterView& v : sim->View().monsters) {
      if (v.alive) ++alive;
      if (v.defId == "miniboss_corpseherd") ++herds;
    }
    CHECK(alive >= 56);  // 55 zone spawns + the mini-boss (no hunt is due at a new game)
    CHECK(herds == 1);
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
    // Without a wave composition (K1 `waves`), the web rule: RandomInt(0, 12) picks from the emerald_plains list
    // [breachclot, risen_footman, gravegnawer, cinderbloat, risen_arbalest, oath_zealot, deserter, deserter_drummer,
    //  risen_shieldbearer, dead_standard_bearer, bonegorger, valdras, rollcall_standard_bearer] (2F appended Valdras's
    //  phase-2 summon; 0.9 -> 11 = valdras).
    rig.h.rng.Get(RngStream::Ai).Script({0.0, 0.5, 0.9});
    const std::vector<EntityId> w0 = rig.monsters.SpawnDefendWave(Vec2(60, 60), 0);
    REQUIRE(w0.size() == 3);
    CHECK(rig.M(w0[0]).def.id == "breachclot");
    CHECK(rig.M(w0[1]).def.id == "deserter");
    CHECK(rig.M(w0[2]).def.id == "valdras");  // bosses included (quests Q8, later)
    CHECK(rig.M(w0[0]).pos == Vec2(68, 60));
    CHECK(rig.M(w0[1]).pos == Vec2(56, 67));  // 60 + 8 cos 120 = 56, 60 + 8 sin 120 = 66.93
    CHECK(rig.M(w0[2]).pos == Vec2(56, 53));
    for (EntityId id : w0) {
      const MonsterInstance& m = rig.M(id);
      CHECK(m.state == MonsterState::Chase);
      CHECK(m.role == MonsterRole::DefendWave);
      CHECK(m.noRespawn);
      CHECK(m.affixes.empty());  // even the elite boss
    }
    CHECK(rig.M(w0[2]).maxHp == 9000);
    CHECK(rig.aggros.size() == 3);
    // Wave 2 on nightmare: n = 5, difficulty then wave scaling.
    rig.h.session.difficulty = Difficulty::Nightmare;
    rig.h.rng.Get(RngStream::Ai).Script({0.5, 0.5, 0.5, 0.5, 0.5});
    const std::vector<EntityId> w2 = rig.monsters.SpawnDefendWave(Vec2(60, 60), 2);
    REQUIRE(w2.size() == 5);
    for (EntityId id : w2) {
      CHECK(rig.M(id).def.id == "deserter");
      CHECK(rig.M(id).maxHp == std::floor(120 * 1.6));    // nightmare 120, x(1 + 0.3 x 2)
      CHECK(rig.M(id).def.damage == std::floor(18 * 1.4));  // nightmare 18, x(1 + 0.2 x 2)
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
    const EntityId chief = rig.Spawn("valdras", {15, 95});
    CHECK(rig.monsters.MarkStoryNamed(chief));
    CHECK_FALSE(rig.monsters.MarkStoryNamed(chief));  // already renamed
    CHECK_FALSE(rig.monsters.MarkStoryNamed(999999));
    Snapshot snap;
    rig.monsters.FillSnapshot(snap);
    REQUIRE(snap.monsters.size() == 1);
    CHECK(snap.monsters[0].storyNamed);
    const BossIntroDef* intro = MD().Story().BossIntroFor("valdras");
    REQUIRE(intro != nullptr);
    CHECK(snap.monsters[0].nameKey == intro->name);
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
  TEST_CASE("mini-boss data: the emerald_plains mini-boss is the Corpseherd at (60,55)") {
    const MonsterTables& t = MD().Monsters();
    const MiniBossEntry* ep = t.MiniBossFor("emerald_plains");
    REQUIRE(ep != nullptr);
    CHECK(ep->monsterId == "miniboss_corpseherd");
    CHECK(ep->spawn == TilePos{60, 55});
  }

  // Split from "queries: ... snapshot views": once the boss label is renamed the view carries the intro name key.
  TEST_CASE("queries: the renamed story boss's snapshot view carries its intro name key (story 8.3)") {
    MonsterRig rig(51, Vec2(60, 60));
    rig.OpenWorld();
    const EntityId chief = rig.Spawn("valdras", {5, 110});
    rig.M(chief).storyNameShown = true;
    Snapshot named;
    rig.monsters.FillSnapshot(named);
    REQUIRE(named.monsters.size() == 1);
    CHECK(named.monsters[0].storyNamed);
    CHECK(named.monsters[0].nameKey == "story.boss.valdras.name");
  }

  // Split from "loot-floor flags (story / endgame scrutiny)": regular zone monsters have no loot floor.
  TEST_CASE("loot-floor flags: the Chapter 1 regulars and the story boss have no loot floor") {
    for (const char* id : {"breachclot", "risen_footman", "gravegnawer", "cinderbloat", "risen_arbalest", "oath_zealot",
                           "deserter", "deserter_drummer", "risen_shieldbearer", "dead_standard_bearer", "bonegorger",
                           "valdras"}) {
      CHECK_FALSE(MDef(id).isMiniBoss);
      CHECK_FALSE(MDef(id).isSubDungeonMiniBoss);
    }
  }

  // Split from "buildAffixName: base name without affixes, [a] base, [a<middle dot>b] base, both locales": the story
  // boss's labels and the Harke hunt key.
  TEST_CASE("buildAffixName: the story boss's labels in both locales; hunt leaders are named by their hunt id") {
    I18n zh, en;
    REQUIRE(zh.LoadTable(test::ReadFile(test::DataDir() + "/i18n_zh-CN.json")));
    REQUIRE(en.LoadTable(test::ReadFile(test::DataDir() + "/i18n_en.json")));
    en.SetLocale(LocaleId::En);
    REQUIRE(en.Current() == LocaleId::En);
    const std::string dot = "\xC2\xB7";
    const MonsterDef& chief = MDef("valdras");
    CHECK(MonsterNameKey(chief) == "data.monster.valdras");
    CHECK(MonsterLabelText(en, MonsterNameKey(chief), {}) == en.T("data.monster.valdras"));
    CHECK(en.T("data.monster.valdras") == "Lord-Marshal Valdras");
    const std::vector<EliteAffixType> one{EliteAffixType::FireEnhanced};
    CHECK(MonsterLabelText(en, MonsterNameKey(chief), one) ==
          "[" + en.T("sys.eliteAffix.name.fire_enhanced") + "] " + en.T("data.monster.valdras"));
    CHECK(MonsterLabelText(en, MonsterNameKey(chief), one) == "[Scorched] Lord-Marshal Valdras");  // monsters.md 3 (1B review names)
    const std::vector<EliteAffixType> three{EliteAffixType::Swift, EliteAffixType::Vampiric, EliteAffixType::Frozen};
    CHECK(MonsterLabelText(zh, MonsterNameKey(chief), three) ==
          "[" + zh.T("sys.eliteAffix.name.swift") + dot + zh.T("sys.eliteAffix.name.vampiric") + dot +
              zh.T("sys.eliteAffix.name.frozen") + "] " + zh.T("data.monster.valdras"));
    CHECK(zh.T("data.monster.valdras") == chief.name);  // FIX Q11: the key resolves to the zh name
    // Hunt leaders are named by their hunt id (localized), an empty base name keeps the bracket.
    const MonsterDef harke = MNormalDef("hunt_harke");
    CHECK(MonsterNameKey(harke) == "data.monster.hunt_harke");
    CHECK(en.T("data.monster.hunt_harke") == "Harke the Oathless");
  }

  // 1B part 1 (monsters.md 2, Wave 1 fields): the authored on-hit statuses and bolt colours (they replace the keyword
  // rule in `derived`), the art ids the art lane ships (`monster_<id>`; until an asset lands the core's art chain misses
  // and the rig fallback times the swing) and the names in both locales.
  TEST_CASE("Chapter 1 roster W1 fields: authored on-hit statuses, breach-violet bolts, art ids, names") {
    struct Hit {
      const char* id;
      size_t slot;  // index into onHitStatus
      size_t count; // onHitStatus.size()
      StatusType status;
      double chance;
      StatusValueKind kind;
      double value;  // fraction (MonsterDamage) or amount (Fixed)
      int32_t durationMs;
    };
    // Wave 2 (package 2F, chapter1.md 7): the Breach's ash numbs - every breachclot and cinderbloat hit lays one Ashnumb
    // stack (the clot's W1 poison gave way to it; the bloat keeps its burn).
    const Hit hits[] = {
        {"breachclot", 0, 1, StatusType::Ashnumb, 1.0, StatusValueKind::Fixed, 1, 6000},
        {"cinderbloat", 0, 2, StatusType::Burn, 0.3, StatusValueKind::MonsterDamage, 0.2, 3000},
        {"cinderbloat", 1, 2, StatusType::Ashnumb, 1.0, StatusValueKind::Fixed, 1, 6000},
        {"oath_zealot", 0, 1, StatusType::Bleed, 0.15, StatusValueKind::MonsterDamage, 0.25, 5000},
        {"miniboss_corpseherd", 0, 1, StatusType::Slow, 1.0, StatusValueKind::Fixed, 20, 2000},  // the bell-toll bolt
    };
    for (const Hit& h : hits) {
      CAPTURE(h.id);
      CAPTURE(h.slot);
      const MonsterDef& d = MDef(h.id);
      REQUIRE(d.onHitStatus.size() == h.count);
      const StatusRule& r = d.onHitStatus[h.slot];
      CHECK(r.status == h.status);
      CHECK(r.hasChance);
      CHECK(r.chance == doctest::Approx(h.chance));
      CHECK(r.valueKind == h.kind);
      CHECK((h.kind == StatusValueKind::Fixed ? r.amount : r.fraction) == doctest::Approx(h.value));
      CHECK(r.durationMs == h.durationMs);
    }
    I18n en;
    REQUIRE(en.LoadTable(test::ReadFile(test::DataDir() + "/i18n_en.json")));
    en.SetLocale(LocaleId::En);
    for (const MonsterDef& d : MD().Monsters().defs) {
      if (d.homeZone != "emerald_plains") continue;
      CAPTURE(d.id);
      // The art lane's plain remake ids (march_palettes.py); Valdras's roll-call summon wears the standard-bearer's.
      CHECK(d.spriteKey == "monster_" + (d.id == "rollcall_standard_bearer" ? std::string("dead_standard_bearer") : d.id));
      // Graceful art fallback: with or without the asset in the manifest the swing gets a contact beat (the clip's when
      // shipped, else the anim_timing rig beat).
      const ActionTiming at = ComputeAttackTiming(MD().Combat().anim, MD().Assets(),
                                                  MonsterArtGameId(MD().Assets(), d.spriteKey, d.id), d.animCategory,
                                                  d.attackSpeedMs);
      CHECK(at.contactMs > 0);
      CHECK(at.contactMs < at.durationMs);
      CHECK(en.Has(d.nameKey));
      CHECK(MD().Strings().Lookup(LocaleId::ZhCN, d.nameKey) != nullptr);
      const bool authored = d.id == "breachclot" || d.id == "cinderbloat" || d.id == "oath_zealot" || d.id == "miniboss_corpseherd";
      if (!authored) CHECK(d.onHitStatus.empty());  // no keyword rule fires on a remake id
      // DR5: an enemy bolt is breach-violet (never the hero's ember); the melee roster keeps the default.
      CHECK(d.projectileColor == (d.isRanged ? 0xB02FD0u : 0xcc44ccu));
    }
    CHECK(MDef("risen_arbalest").isRanged);
    CHECK(MDef("risen_arbalest").projectileColor == 0xB02FD0u);
    CHECK(en.T("data.monster.risen_arbalest") == "Risen Arbalester");
    CHECK(en.T("data.monster.miniboss_corpseherd") == "The Corpseherd");
  }

  // monsters.md 3: the seven affixes keep their mechanics; the remake re-voices them and moves their glow off the
  // hero's ember (DR5); Wave 2 adds Unburied. `emerald_plains` keeps exactly one affix.
  TEST_CASE("elite affix look (remake): re-voiced names in both locales, recoloured glows; one affix in Chapter 1") {
    struct Row {
      EliteAffixType type;
      const char* zh;
      const char* en;
      uint32_t color;
    };
    const Row rows[] = {
        {EliteAffixType::FireEnhanced, "\xE7\x84\xA6\xE7\x81\xBC", "Scorched", 0xB02FD0},
        {EliteAffixType::Swift, "\xE9\xA9\xB1\xE7\xAD\x96", "Driven", 0xD8CCB0},
        {EliteAffixType::Teleporting, "\xE6\xB8\x8A\xE8\xA1\x8C", "Breachwalker", 0xD08CFF},
        {EliteAffixType::ExtraStrong, "\xE8\x9B\xAE\xE5\x8A\x9B", "Brutish", 0xC0283A},
        {EliteAffixType::CurseAura, "\xE5\x93\x80\xE5\x92\x92", "Dirgecursed", 0x7A2A90},
        {EliteAffixType::Vampiric, "\xE5\x99\xAC\xE6\xB8\x8A", "Breach-Drinker", 0x6A0F2A},
        {EliteAffixType::Frozen, "\xE5\xA2\x93\xE5\xAF\x92", "Gravecold", 0xCFE8FF},
        {EliteAffixType::Unburied, "\xE4\xB8\x8D\xE8\x91\xAC", "Unburied", 0xD08CFF},  // W2 (2F): the Unburied eyes
    };
    I18n en;
    REQUIRE(en.LoadTable(test::ReadFile(test::DataDir() + "/i18n_en.json")));
    en.SetLocale(LocaleId::En);
    for (const Row& r : rows) {
      CAPTURE(EnumName(r.type));
      const EliteAffixDef& d = MAffixes().Def(r.type);
      CHECK(d.name == r.zh);
      CHECK(d.nameEn == r.en);
      CHECK(d.vfxColor == r.color);
      CHECK(en.T(EliteAffixNameKey(r.type)) == r.en);
      CHECK(*MD().Strings().Lookup(LocaleId::ZhCN, EliteAffixNameKey(r.type)) == r.zh);
    }
    MonsterRig rig(5);
    rig.monsters.SpawnZonePopulation();
    for (const MonsterInstance& m : rig.monsters.All()) {
      if (m.role == MonsterRole::StoryBoss) CHECK(m.AffixCount() == 1);
    }
  }

  // Split from "mini-boss lines are i18n keys (Q10 / M8)": the opened event carries the header key and the lines.
  TEST_CASE("mini-boss lines: the Corpseherd's opened event carries its header key and its three line keys") {
    MonsterRig rig(5, Vec2(60, 60));
    rig.OpenWorld();
    rig.monsters.SpawnMiniBoss();
    rig.h.events.Clear();
    rig.Step();
    REQUIRE(rig.monsters.MiniBossDialogueActive());
    const std::vector<EvMiniBossDialogue> ev = MEvents<EvMiniBossDialogue>(rig.h.events);
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].nameKey == "data.monster.miniboss_corpseherd");
    CHECK(ev[0].lineKeys == MiniBossDialogueLineKeys(*MD().Monsters().MiniBossDialogue("miniboss_corpseherd"),
                                                     "miniboss_corpseherd"));
    CHECK(ev[0].lineKeys == std::vector<std::string>{"data.miniBossDialogue.miniboss_corpseherd.line1",
                                                     "data.miniBossDialogue.miniboss_corpseherd.line2",
                                                     "data.miniBossDialogue.miniboss_corpseherd.line3"});
  }

  // ===================================================================================================================
  // Wave 2 content (package 2F; monsters.md 2, chapter1.md 7 / 9 / 11 / 17): the abilities, phases, on-death effects,
  // corpses, packs, morale, kite, block and auras the K2-K5 engines run on the Chapter 1 roster. The engines are tested on
  // synthetic fixtures (monster_abilities / corpses / phases / packs tests); these pin the authored numbers.
  // ===================================================================================================================

  // Every Chapter 1 definition (the roster, the Corpseherd, the hunts): the 35 % hit cap (plan 6: no death to one hit)
  // and an authored wind-up on every ability (>= 600 ms; >= 700 ms for a boss special: DataStore refuses less).
  TEST_CASE("Chapter 1 W2 roster: hit caps, authored wind-ups, the ability table") {
    std::vector<MonsterDef> defs;
    for (const MonsterDef& d : MD().Monsters().defs) {
      if (d.homeZone == "emerald_plains" || d.id == "miniboss_corpseherd") defs.push_back(d);
    }
    for (const HuntDef& h : MD().Monsters().hunts) {
      if (h.zone == "emerald_plains") defs.push_back(MNormalDef(h.huntId));
    }
    CHECK(defs.size() == 17);  // 13 roster (with the roll-call bearer) + the Corpseherd + 3 hunts
    size_t abilities = 0;
    for (const MonsterDef& d : defs) {
      CAPTURE(d.id);
      CHECK(d.hitCapHeroMaxHpPct == 35);
      const bool boss = d.isMiniBoss || d.id == "valdras";
      for (const MonsterAbilityDef& a : d.abilities) {
        CAPTURE(a.id);
        ++abilities;
        CHECK(a.hasWindupMs);
        CHECK(a.windupMs >= (boss ? 700 : 600));
      }
    }
    CHECK(abilities == 13);  // drummer, standard-bearer, bonegorger 1 each; Valdras 4; roll-call 1; Corpseherd 3; Hungerbell 2
    struct Row {
      const char* monster;
      const char* id;
      AbilityKind kind;
      const char* clip;
      double windupMs, cooldownMs, initialCooldownMs;
      int32_t priority;
      AbilityAim aim;
    };
    const Row rows[] = {
        {"deserter_drummer", "war_drum", AbilityKind::Call, "Cast01", 2000, 8000, 1000, 0, AbilityAim::Self},
        {"dead_standard_bearer", "muster", AbilityKind::Raise, "Cast01", 900, 10000, 2000, 0, AbilityAim::Corpse},
        {"bonegorger", "clapper_slam", AbilityKind::Circle, "Attack02", 800, 6000, 1500, 0, AbilityAim::HeroAtWindup},
        {"hunt_hungerbell", "clapper_slam", AbilityKind::Circle, "Attack02", 800, 6000, 1500, 0, AbilityAim::HeroAtWindup},
        {"hunt_hungerbell", "supper_bell", AbilityKind::Call, "", 800, 9000, 0, 0, AbilityAim::Self},  // 2F review: no clip
        {"miniboss_corpseherd", "call_the_flock_pull", AbilityKind::Raise, "Cast01", 900, 600000, 0, 2, AbilityAim::Corpse},
        {"miniboss_corpseherd", "call_the_flock", AbilityKind::Raise, "Cast01", 900, 8000, 8000, 1, AbilityAim::Corpse},
        {"miniboss_corpseherd", "keen", AbilityKind::Circle, "Cast02", 800, 12000, 0, 0, AbilityAim::Self},
        {"valdras", "banner_sweep", AbilityKind::Cone, "Attack02", 700, 5000, 2000, 1, AbilityAim::HeroAtWindup},
        {"valdras", "oath_brand", AbilityKind::Circle, "Cast01", 900, 8000, 4000, 2, AbilityAim::HeroAtWindup},
        {"valdras", "breach_seep", AbilityKind::Hazard, "", 700, 6000, 1500, 3, AbilityAim::Anchor},  // the dais pulses
        {"valdras", "lunge", AbilityKind::Line, "Lunge", 800, 7000, 1000, 2, AbilityAim::HeroAtWindup},
        {"rollcall_standard_bearer", "roll_call", AbilityKind::Summon, "Cast01", 900, 10000, 3000, 0, AbilityAim::Self},
    };
    for (const Row& r : rows) {
      CAPTURE(r.monster);
      CAPTURE(r.id);
      const MonsterDef d = MNormalDef(r.monster);
      const MonsterAbilityDef* a = nullptr;
      for (const MonsterAbilityDef& x : d.abilities) a = x.id == r.id ? &x : a;
      REQUIRE(a != nullptr);
      CHECK(a->kind == r.kind);
      CHECK(a->clip == r.clip);
      CHECK(a->windupMs == r.windupMs);
      CHECK(a->cooldownMs == r.cooldownMs);
      CHECK(a->initialCooldownMs == r.initialCooldownMs);
      CHECK(a->priority == r.priority);
      CHECK(a->aim == r.aim);
    }
    // The regulars and the bosses' telegraphed shapes, damage and leftovers.
    const MonsterDef& valdras = MDef("valdras");
    REQUIRE(valdras.abilities.size() == 4);
    const MonsterAbilityDef& sweep = valdras.abilities[0];
    CHECK(sweep.shape.kind == AbilityShapeKind::Cone);
    CHECK(sweep.shape.radius == 2.6);
    CHECK(sweep.shape.arcDeg == 120);
    CHECK(sweep.dmgMul == 1.3);
    CHECK(sweep.damageType == DamageType::Fire);  // 2F review (2F-D): his fire specials are fire (chapter1.md 11)
    const double valdrasReach = valdras.attackRange * MAi().attackExitMul;
    const MonsterAbilityDef& brand = valdras.abilities[1];
    CHECK(brand.shape.radius == 1.8);
    CHECK(brand.dmgMul == 1.8);
    CHECK(brand.damageType == DamageType::Fire);
    CHECK(brand.hazard.present);
    CHECK(brand.hazard.durationMs == 4000);
    CHECK(brand.hazard.tickMs == 500);
    CHECK(brand.hazard.tickDmgMul == 0.15);
    const MonsterAbilityDef& seep = valdras.abilities[2];  // P2 only; one crack line from the dais (2F outcomes)
    CHECK(seep.phases == std::vector<int32_t>{1});
    CHECK(seep.shape.kind == AbilityShapeKind::Line);
    CHECK(seep.shape.length == 8);
    CHECK(seep.shape.width == 1);
    CHECK(seep.damageType == DamageType::Fire);
    CHECK(seep.hazard.durationMs == 1000);
    CHECK(seep.hazard.tickMs == 500);
    CHECK(seep.hazard.tickDmgMul == 0.5);
    CHECK(seep.hazard.status.present);
    CHECK(seep.hazard.status.type == StatusType::Slow);
    CHECK(seep.hazard.status.ms == 2000);
    CHECK(seep.hazard.status.value == 30);
    // 2F review (CORE-R1): a hazard of the dais itself (no clip: the cracks are the tell, he is not rooted), cast at a hero
    // up to 10 tiles off - a ranged special, so it comes between swings while he is held on the dais.
    CHECK(seep.clip.empty());
    CHECK(seep.hasWindupMs);
    CHECK(seep.hasRangeMax);
    CHECK(seep.rangeMax == 10);
    CHECK(AbilityIsRangedSpecial(seep, valdrasReach));
    CHECK_FALSE(AbilityIsRangedSpecial(sweep, valdrasReach));
    CHECK_FALSE(AbilityIsRangedSpecial(brand, valdrasReach));
    const MonsterAbilityDef& lunge = valdras.abilities[3];
    CHECK(lunge.shape.length == 5);
    CHECK(lunge.shape.width == 1.2);
    CHECK(lunge.dmgMul == 1.5);
    // 2F review: the charge (chapter1.md 11 "Lunge"): at a hero 2.5 - 5 tiles off, fire, and he runs the line.
    CHECK(lunge.rangeMin == 2.5);
    CHECK(lunge.hasRangeMax);
    CHECK(lunge.rangeMax == 5);
    CHECK(lunge.damageType == DamageType::Fire);
    CHECK(lunge.dash);
    CHECK(AbilityIsRangedSpecial(lunge, valdrasReach));
    for (const MonsterDef& m : MD().Monsters().defs) {
      for (const MonsterAbilityDef& a : m.abilities) {
        CAPTURE(m.id);
        CAPTURE(a.id);
        CHECK(a.dash == (m.id == "valdras" && a.id == "lunge"));  // the only charge of the chapter
        CHECK(a.clip.empty() == ((m.id == "valdras" && a.id == "breach_seep") || a.id == "supper_bell"));
      }
    }
    const MonsterAbilityDef& slam = MDef("bonegorger").abilities.at(0);
    CHECK(slam.shape.radius == 2);
    CHECK(slam.dmgMul == 1.6);
    const MonsterDef& herd = MDef("miniboss_corpseherd");
    REQUIRE(herd.abilities.size() == 3);
    CHECK(herd.abilities[0].summon.monsterId == "risen_footman");
    CHECK(herd.abilities[0].summon.count == 2);
    CHECK(herd.abilities[0].summon.capAlive == 3);
    CHECK(herd.abilities[1].summon.count == 1);
    CHECK(herd.abilities[1].summon.perWindowCap == 1);
    CHECK(herd.abilities[1].summon.windowMs == 8000);
    for (size_t i = 0; i < 2; ++i) {
      CHECK(herd.abilities[i].summon.at == AbilitySummonAt::Corpse);
      CHECK(herd.abilities[i].summon.radius == 6);
      CHECK(herd.abilities[i].summon.hpPct == 40);
    }
    CHECK(herd.abilities[2].hasHpBelowPct);
    CHECK(herd.abilities[2].hpBelowPct == 50);
    CHECK(herd.abilities[2].shape.radius == 4);
    CHECK(herd.abilities[2].dmgMul == 0);  // 2F review: Keen slows only (chapter1.md 10), a status-only strike
    CHECK(herd.abilities[2].status.type == StatusType::Slow);
    CHECK(herd.abilities[2].status.ms == 3000);
    CHECK(herd.abilities[2].status.value == 25);
    const AbilitySummonDef& muster = MDef("dead_standard_bearer").abilities.at(0).summon;
    CHECK(muster.monsterId == "risen_footman");
    CHECK(muster.count == 1);
    CHECK(muster.at == AbilitySummonAt::Corpse);
    CHECK(muster.radius == 6);
    CHECK(muster.capAlive == 2);
    CHECK(muster.perWindowCap == 1);
    CHECK(muster.windowMs == 10000);
    CHECK(muster.hpPct == 40);
    const AbilitySummonDef& roll = MDef("rollcall_standard_bearer").abilities.at(0).summon;
    CHECK(roll.monsterId == "risen_footman");
    CHECK(roll.count == 2);
    CHECK(roll.at == AbilitySummonAt::Anchors);
    CHECK(roll.anchors == std::vector<TilePos>{{11, 91}, {19, 99}});  // the two ash circles by the dais
    CHECK(roll.capAlive == 4);
    CHECK(roll.hpPct == 40);
    const AbilityBuffDef& drum = MDef("deserter_drummer").abilities.at(0).buff;
    CHECK(drum.hasStat);
    CHECK(drum.stat == MonsterBuffStat::FearImmune);
    CHECK(drum.ms == 6000);
    CHECK(drum.radius == 8);
    CHECK(drum.family == "living");
    const MonsterDef hungerbell = MNormalDef("hunt_hungerbell");  // a copy: the hunt's definition is built here
    const AbilityBuffDef& bell = hungerbell.abilities.at(1).buff;
    CHECK_FALSE(bell.hasStat);  // the call only provokes: every gnawer within 10 tiles comes
    CHECK(bell.radius == 10);
    CHECK(bell.family == "gravegnawers");
    // Only Hungerbell rings the bell: the midden's bonegorger has the slam alone.
    CHECK(MDef("bonegorger").abilities.size() == 1);
  }

  TEST_CASE("Chapter 1 W2 roster: families, corpses, the Unburied trait, the bloat's burst, packs, morale, kite, block, auras") {
    struct Row {
      const char* id;
      const char* family;
      CorpseLeaves leaves;
      bool consumes, rises;
    };
    const Row rows[] = {
        {"breachclot", "breach", CorpseLeaves::None, false, false},
        {"risen_footman", "unburied", CorpseLeaves::Corpse, false, true},
        {"gravegnawer", "gravegnawers", CorpseLeaves::Remains, true, false},
        {"cinderbloat", "breach", CorpseLeaves::None, false, false},
        {"risen_arbalest", "unburied", CorpseLeaves::Corpse, false, true},
        {"oath_zealot", "living", CorpseLeaves::Corpse, false, false},
        {"deserter", "living", CorpseLeaves::Corpse, false, false},
        {"deserter_drummer", "living", CorpseLeaves::Corpse, false, false},
        {"risen_shieldbearer", "unburied", CorpseLeaves::Corpse, false, true},
        {"dead_standard_bearer", "unburied", CorpseLeaves::Corpse, false, true},
        {"bonegorger", "gravegnawers", CorpseLeaves::Remains, false, false},
        {"valdras", "unburied", CorpseLeaves::Remains, false, false},
        {"rollcall_standard_bearer", "unburied", CorpseLeaves::Corpse, false, false},
        {"miniboss_corpseherd", "unburied", CorpseLeaves::Remains, false, false},
    };
    for (const Row& r : rows) {
      CAPTURE(r.id);
      const MonsterDef& d = MDef(r.id);
      CHECK(d.family == r.family);
      CHECK(d.corpse.leaves == r.leaves);
      CHECK(d.corpse.consumes == r.consumes);
      CHECK(d.riseAgain.present == r.rises);
      if (r.rises) {  // monsters.md 2.2: 25 %, 3 s after death, at 40 %; never after a crit
        CHECK(d.riseAgain.chance == 0.25);
        CHECK(d.riseAgain.delayMs == 3000);
        CHECK(d.riseAgain.hpPct == 40);
        CHECK(d.riseAgain.notOnCrit);
      }
      CHECK(d.onDeath.present == (std::string_view(r.id) == "cinderbloat"));
      CHECK(d.packAggro == (std::string_view(r.id) == "gravegnawer" ? 6 : 0));
      CHECK(d.kite.present == (std::string_view(r.id) == "risen_arbalest" || std::string_view(r.id) == "miniboss_corpseherd"));
      CHECK(d.block.present == (std::string_view(r.id) == "risen_shieldbearer"));
      CHECK(d.morale.present == (std::string_view(r.id) == "deserter"));
    }
    const MonsterOnDeathDef& burst = MDef("cinderbloat").onDeath;
    CHECK(burst.kind == OnDeathKind::Burst);
    CHECK(burst.delayMs == 800);  // the fuse
    CHECK(burst.shape.kind == AbilityShapeKind::Circle);
    CHECK(burst.shape.radius == 2.2);
    CHECK(burst.dmg == 18);
    CHECK(burst.damageType == DamageType::Fire);
    CHECK(burst.hitsMonsters);
    CHECK(burst.monsterDmgMul == 2);
    CHECK(burst.capHeroMaxHpPct == 20);
    CHECK(MDef("risen_arbalest").kite.min == 3);
    CHECK(MDef("risen_arbalest").kite.max == 6);
    CHECK(MDef("miniboss_corpseherd").kite.min == 2);  // M5: range 3.0 kept, so the band is [2, 3] (monsters.md 1.4)
    CHECK(MDef("miniboss_corpseherd").kite.max == 3);
    const MonsterBlockDef& block = MDef("risen_shieldbearer").block;
    CHECK(block.arcDeg == 100);
    CHECK(block.damageMul == 0.5);
    CHECK(block.wall == 2);
    const MonsterMoraleDef& morale = MDef("deserter").morale;
    CHECK(morale.fleeOnLeaderDeathMs == 5000);
    CHECK(morale.fleeRadius == 8);
    CHECK(morale.sustainFamily == "living");
    CHECK(morale.sustainRadius == 8);
    struct Aura {
      const char* id;
      MonsterBuffStat stat;
      double mul, radius;
      const char* family;
    };
    const Aura auras[] = {
        {"deserter_drummer", MonsterBuffStat::AttackSpeed, 1.1, 8, "living"},
        {"dead_standard_bearer", MonsterBuffStat::Damage, 1.15, 5, "unburied"},
        {"rollcall_standard_bearer", MonsterBuffStat::Damage, 1.15, 5, "unburied"},
    };
    for (const Aura& a : auras) {
      CAPTURE(a.id);
      const MonsterAuraDef& x = MDef(a.id).aura;
      CHECK(x.present);
      CHECK(x.stat == a.stat);
      CHECK(x.mul == a.mul);
      CHECK(x.radius == a.radius);
      CHECK(x.family == a.family);
    }
    // The zealot's frenzy (monsters.md 2.3): below half, swings faster at once and takes a little more (2F review: no
    // rooting enter action; UE plays the frenzy look on its EvMonsterPhase).
    const MonsterDef& zealot = MDef("oath_zealot");
    REQUIRE(zealot.phases.size() == 1);
    CHECK(zealot.phases[0].hpBelowPct == 50);
    CHECK_FALSE(zealot.phases[0].hasEnterAction);
    CHECK(zealot.phases[0].attackSpeedMul == 1.35);
    CHECK(zealot.phases[0].speedMul == 1.2);
    CHECK(zealot.phases[0].damageTakenMul == 1.1);
  }

  // chapter1.md 11 (the as-built numbers: 2F outcomes): P1 Banner Sweep + Oath-Brand; P2 Roll Call below 65 % (held on
  // the dais, the standard-bearer at the west ash circle, Feed, Breach-seep); P3 No Retreat below 30 % (frenzy, Lunge, the
  // closing ring).
  TEST_CASE("Chapter 1 W2 Valdras: three phases, the roll call, Feed, the closing ring") {
    const MonsterDef& v = MDef("valdras");
    CHECK(v.hp == 9000);
    CHECK(v.damage == 20);  // 2F review (BOT-R2 / BAL-R3 / 2F-D2): 14000 / 32 -> 9000 / 20 (the design's damage)
    REQUIRE(v.phases.size() == 2);
    const MonsterPhaseDef& p2 = v.phases[0];
    CHECK(p2.hpBelowPct == 65);
    CHECK(p2.enterAction == AnimAction::Roar);
    CHECK(p2.speedMul == 0.6);
    CHECK(p2.attackSpeedMul == 1);
    CHECK(p2.leash.present);
    CHECK_FALSE(p2.leash.hasCentre);  // the dais: his authored spawn tile (15, 95)
    CHECK(p2.leash.radius == 3);
    REQUIRE(p2.summons.size() == 1);
    CHECK(p2.summons[0].monsterId == "rollcall_standard_bearer");
    CHECK(p2.summons[0].count == 1);
    CHECK(p2.summons[0].at == AbilitySummonAt::Anchors);
    CHECK(p2.summons[0].anchors == std::vector<TilePos>{{11, 91}});
    CHECK(p2.summons[0].respawnMs == 20000);  // one new bearer 20 s later, once
    CHECK(p2.summons[0].respawns == 1);
    CHECK(p2.feed.present);
    CHECK(p2.feed.radius == 4);
    CHECK(p2.feed.healPct == 3);
    CHECK(p2.feed.delayMs == 1000);
    CHECK(p2.stinger == "stinger_valdras_roll_call");
    CHECK_FALSE(p2.arena.present);
    const MonsterPhaseDef& p3 = v.phases[1];
    CHECK(p3.hpBelowPct == 30);
    CHECK(p3.enterAction == AnimAction::Roar);
    CHECK(p3.attackSpeedMul == 1.25);
    CHECK(p3.speedMul == 1.15);
    CHECK_FALSE(p3.leash.present);
    CHECK(p3.abilitiesOn == std::vector<std::string>{"lunge"});
    CHECK(p3.arena.present);
    CHECK(p3.arena.kind == ArenaKind::RingShrink);
    CHECK(p3.arena.fromR == 9);
    CHECK(p3.arena.toR == 5);
    CHECK(p3.arena.overMs == 40000);
    CHECK(p3.arena.dpsPctMaxHp == 4);
    CHECK(p3.arena.damageType == DamageType::Fire);
    CHECK(p3.stinger == "stinger_valdras_no_retreat");
    // Valdras's Breach-seep belongs to P2 (phase index 1), his Lunge is switched on by P3; the dais and the circles are
    // walkable on the generated map.
    MonsterRig rig(7);
    REQUIRE(rig.ok);
    for (TilePos t : {TilePos{15, 95}, TilePos{11, 91}, TilePos{19, 99}}) {
      CAPTURE(t.col);
      CAPTURE(t.row);
      CHECK(rig.zone.Walkable(t.col, t.row));
    }
  }

  TEST_CASE("Chapter 1 W2 world: pack mixes and leaders, dawn variants, the Sheepfold's seeds, the Muster ambush") {
    const MapDef* map = MD().FindMap(test::ch1::kZone);
    REQUIRE(map != nullptr);
    REQUIRE(map->spawns.size() == 10);
    const auto mixOf = [](const MapSpawnDef& s) {
      std::vector<std::pair<std::string, int32_t>> out;
      for (const MapSpawnMixDef& m : s.mix) out.emplace_back(m.monsterId, m.count);
      return out;
    };
    using Mix = std::vector<std::pair<std::string, int32_t>>;
    // The trenches: footmen until dawn, then gnawers (rf_chapter_dawn, chapter1.md 8).
    const MapSpawnDef& trench = map->spawns[0];
    CHECK(trench.pos == TilePos{25, 20});
    CHECK(trench.mix.empty());
    REQUIRE(trench.variants.size() == 1);
    CHECK(trench.variants[0].whenFlag == "rf_chapter_dawn");
    CHECK(trench.variants[0].monsterId == "gravegnawer");
    CHECK(trench.variants[0].count == 8);
    CHECK(mixOf(map->spawns[3]) == Mix{{"cinderbloat", 3}, {"breachclot", 3}});         // the Weeping Ditch
    CHECK(mixOf(map->spawns[5]) == Mix{{"risen_arbalest", 3}, {"risen_shieldbearer", 2}});  // the Broken Redoubt
    // 2F review (2F-D): the Redoubt falls to the gnawers at dawn too (chapter1.md 8: the dead lie down, the feeders come).
    REQUIRE(map->spawns[5].variants.size() == 1);
    CHECK(map->spawns[5].variants[0].whenFlag == "rf_chapter_dawn");
    CHECK(map->spawns[5].variants[0].monsterId == "gravegnawer");
    CHECK(map->spawns[5].variants[0].count == 5);
    const MapSpawnDef& ranks = map->spawns[6];  // the Dead Ranks
    CHECK(mixOf(ranks) == Mix{{"risen_shieldbearer", 3}, {"risen_footman", 2}, {"dead_standard_bearer", 1}});
    CHECK(ranks.leader.present);
    CHECK(ranks.leader.monsterId == "dead_standard_bearer");
    CHECK_FALSE(ranks.leader.elite);
    REQUIRE(ranks.variants.size() == 1);
    CHECK(ranks.variants[0].whenFlag == "rf_chapter_dawn");
    CHECK(ranks.variants[0].monsterId == "gravegnawer");
    CHECK(ranks.variants[0].count == 6);
    const MapSpawnDef& hollow = map->spawns[8];  // the Hollow Watch
    CHECK(mixOf(hollow) == Mix{{"deserter", 5}, {"deserter_drummer", 1}});
    CHECK(hollow.leader.present);
    CHECK(hollow.leader.monsterId == "deserter");
    CHECK(hollow.leader.elite);
    CHECK(mixOf(map->spawns[9]) == Mix{{"bonegorger", 1}, {"gravegnawer", 2}});  // the Crowmarket midden
    for (size_t i : {size_t{1}, size_t{2}, size_t{4}, size_t{7}}) {
      CAPTURE(i);
      CHECK(map->spawns[i].mix.empty());
      CHECK_FALSE(map->spawns[i].leader.present);
      CHECK(map->spawns[i].variants.empty());
    }
    // The Sheepfold's six seated crusaders (chapter1.md 10): the Corpseherd's first raise.
    REQUIRE(map->corpseSeeds.size() == 6);
    const std::vector<TilePos> seeds{{57, 54}, {58, 57}, {61, 58}, {63, 56}, {62, 52}, {59, 52}};
    for (size_t i = 0; i < seeds.size(); ++i) {
      CAPTURE(i);
      CHECK(map->corpseSeeds[i].pos == seeds[i]);
      CHECK(map->corpseSeeds[i].monsterId == "risen_footman");
    }
    // The champion Muster (an ambush event, chapter1.md 9): a standard-bearer leading three footmen, one shared affix, a
    // magic item from the leader.
    const ZoneEventDataDef* z = MD().World().randomEvents.ForZone(test::ch1::kZone);
    REQUIRE(z != nullptr);
    const auto& champ = z->ambushChampion;
    CHECK(champ.present);
    CHECK(champ.chance == 0.1);
    REQUIRE(champ.mix.size() == 2);
    CHECK(champ.mix[0].monsterId == "dead_standard_bearer");
    CHECK(champ.mix[0].count == 1);
    CHECK(champ.mix[1].monsterId == "risen_footman");
    CHECK(champ.mix[1].count == 3);
    CHECK(champ.leader == "dead_standard_bearer");
    CHECK(champ.sharedAffix);
    CHECK(champ.leaderLoot == ItemQuality::Magic);
    // 2F review (2F-D): after dawn the dead no longer muster, and an ambush is the gnawers' (chapter1.md 8).
    CHECK(champ.notFlag == "rf_chapter_dawn");
    REQUIRE(z->ambushVariants.size() == 1);
    CHECK(z->ambushVariants[0].whenFlag == "rf_chapter_dawn");
    CHECK(z->ambushVariants[0].monsters == std::vector<std::string>{"gravegnawer"});
    for (const ZoneEventDataDef& e : MD().World().randomEvents.zones) {
      if (e.zoneId == test::ch1::kZone) continue;
      CHECK_FALSE(e.ambushChampion.present);
      CHECK(e.ambushVariants.empty());
    }
  }

  TEST_CASE("Chapter 1 W2 world state: flags, permanent rewards, quest-gated NPC lines, Ashnumb, the remade affixes") {
    for (const QuestDef& q : MD().Quests().quests) {
      CAPTURE(q.id);
      if (q.id == "q_rf_gnawed_field") {
        CHECK(q.setsFlags == std::vector<std::string>{"rf_hungerbell_silenced"});
      } else if (q.id == "q_rf_last_toll") {
        CHECK(q.setsFlags == std::vector<std::string>{"rf_chapter_dawn"});
      } else {
        CHECK(q.setsFlags.empty());
      }
      const QuestPermanentRewardDef& p = q.rewards.permanent;
      if (q.id == "q_rf_black_blood") {  // S1: Marl's Bitter Draught
        CHECK(p.present);
        CHECK(p.maxLife == 15);
        CHECK(p.statPoints == 0);
      } else if (q.id == "q_rf_hallowed_ground") {  // S6
        CHECK(p.present);
        CHECK(p.maxLife == 0);
        CHECK(p.statPoints == 3);
      } else {
        CHECK_FALSE(p.present);
      }
    }
    // The after lines (chapter1.md 3): said once the quest is turned in; Brother Aldwin's first line gives way to his.
    test::SimHarness h(3, MD());
    QuestSystem qs(h.ctx.data, h.events, h.bus);
    const auto turnedIn = [&](const char* id) {
      QuestProgress p;
      p.questId = id;
      p.status = QuestStatus::TurnedIn;
      p.objectives.assign(MD().FindQuest(id)->objectives.size(), 1);
      return p;
    };
    std::vector<std::string> flags;
    const WorldStateView before{flags, &qs};
    struct Npc {
      const char* id;
      const char* quest;
      std::vector<std::string> before, after;
    };
    const Npc npcs[] = {
        {"blacksmith", "q_rf_tag_thief", {"data.npc.blacksmith.dialogue.0"},
         {"data.npc.blacksmith.after.q_rf_tag_thief", "data.npc.blacksmith.dialogue.0"}},
        {"quest_elder", "q_rf_oathless", {"data.npc.quest_elder.dialogue.0"},
         {"data.npc.quest_elder.after.q_rf_oathless", "data.npc.quest_elder.dialogue.0"}},
        {"plains_herbalist", "q_rf_black_blood", {"data.npc.plains_herbalist.dialogue.0"},
         {"data.npc.plains_herbalist.after.q_rf_black_blood", "data.npc.plains_herbalist.dialogue.0"}},
        {"knell_chaplain", "q_rf_last_toll", {"data.npc.knell_chaplain.dialogue.0", "data.npc.knell_chaplain.dialogue.1"},
         {"data.npc.knell_chaplain.after.q_rf_last_toll", "data.npc.knell_chaplain.dialogue.1"}},
    };
    I18n en;
    REQUIRE(en.LoadTable(test::ReadFile(test::DataDir() + "/i18n_en.json")));
    en.SetLocale(LocaleId::En);
    // 2F review (2F-D): S1's permanent reward has its name (the toast UE shows on EvPermanentReward); S6 shows the
    // generic reward text.
    CHECK(en.T("data.quest.q_rf_black_blood.permanent") == "Marl's Bitter Draught");
    CHECK(MD().Strings().Lookup(LocaleId::ZhCN, "data.quest.q_rf_black_blood.permanent") != nullptr);
    for (const Npc& n : npcs) {
      CAPTURE(n.id);
      const NpcDef* npc = MD().FindNpc(n.id);
      REQUIRE(npc != nullptr);
      CHECK(npc->hasVisibleWhen == (n.id == std::string("plains_herbalist")));  // 2F review: Marl leaves in the finale
      CHECK(NpcLinesNow(*npc, before) == n.before);
      QuestSystem done(h.ctx.data, h.events, h.bus);
      done.Load({turnedIn(n.quest)});
      const WorldStateView after{flags, &done};
      CHECK(NpcLinesNow(*npc, after) == n.after);
      CHECK(NpcPanelLineNow(*npc, after) == n.after[1]);  // the panel keeps the standing line
      for (const std::string& k : n.after) {
        CHECK(en.Has(k));
        CHECK(MD().Strings().Lookup(LocaleId::ZhCN, k) != nullptr);
      }
    }
    // 2F review (2F-D): Marl leaves once the last toll is turned in (the finale; chapter1.md 7), unless her own quest is
    // still open (S1 stays finishable).
    {
      const NpcDef* marl = MD().FindNpc("plains_herbalist");
      REQUIRE(marl != nullptr);
      const auto rec = [&](const char* id, QuestStatus st) {
        QuestProgress p;
        p.questId = id;
        p.status = st;
        p.objectives.assign(MD().FindQuest(id)->objectives.size(), st == QuestStatus::Active ? 0 : 1);
        return p;
      };
      const auto visible = [&](std::vector<QuestProgress> recs) {
        QuestSystem q(h.ctx.data, h.events, h.bus);
        q.Load(std::move(recs));
        return NpcVisibleNow(*marl, WorldStateView{flags, &q});
      };
      CHECK(visible({}));
      CHECK(visible({rec("q_rf_last_toll", QuestStatus::Completed)}));
      CHECK_FALSE(visible({rec("q_rf_last_toll", QuestStatus::TurnedIn)}));
      CHECK(visible({rec("q_rf_last_toll", QuestStatus::TurnedIn), rec("q_rf_black_blood", QuestStatus::Active)}));
      CHECK_FALSE(visible({rec("q_rf_last_toll", QuestStatus::TurnedIn), rec("q_rf_black_blood", QuestStatus::TurnedIn)}));
    }
    size_t withLines = 0;
    for (const NpcDef& n : MD().Npcs().npcs) withLines += n.lines.empty() ? 0 : 1;
    CHECK(withLines == 4);
    // Ashnumb (monsters.md 4): -10 % healing per stack, at most 5, slowed 20 % at 5; laid by the Breach's clots and bloats.
    const StatusEffectRules& rules = MD().Classes().statusRules;
    CHECK(rules.Lands(StatusType::Ashnumb));
    CHECK(rules.Stacking(StatusType::Ashnumb) == StatusStacking::StackCount);
    CHECK(rules.Stacks(StatusType::Ashnumb).maxStacks == 5);
    CHECK(rules.Stacks(StatusType::Ashnumb).healingReceivedPerStack == 0.1);
    CHECK(rules.Stacks(StatusType::Ashnumb).slowAtMaxStacksPct == 20);
    CHECK(en.T("sys.statusEffect.name.ashnumb") == "Ashnumb");
    // The remade affixes (monsters.md 3): Scorched bursts, Gravecold rings, Unburied rises (its family only).
    const EliteAffixDef& scorched = MAffixes().Def(EliteAffixType::FireEnhanced);
    CHECK(scorched.onDeath.present);
    CHECK(scorched.onDeath.kind == OnDeathKind::Burst);
    CHECK(scorched.onDeath.delayMs == 1000);
    CHECK(scorched.onDeath.shape.radius == 2);
    CHECK(scorched.onDeath.dmg == 20);
    CHECK(scorched.onDeath.damageType == DamageType::Fire);
    CHECK(scorched.onDeath.capHeroMaxHpPct == 20);
    CHECK(scorched.onDeath.hitsMonsters);  // 2F review (2F-D): the blast scorches the pack too (monsters.md 3)
    CHECK(scorched.onDeath.monsterDmgMul == 2);
    const EliteAffixDef& gravecold = MAffixes().Def(EliteAffixType::Frozen);
    CHECK(gravecold.onDeath.present);
    CHECK(gravecold.onDeath.kind == OnDeathKind::Ring);
    CHECK(gravecold.onDeath.delayMs == 600);
    CHECK(gravecold.onDeath.shape.radius == 2.5);
    CHECK(gravecold.onDeath.damageType == DamageType::Ice);
    CHECK(gravecold.onDeath.status.type == StatusType::Slow);
    CHECK(gravecold.onDeath.status.ms == 2500);
    CHECK(gravecold.onDeath.status.value == 30);
    CHECK_FALSE(gravecold.onDeath.hasDmg);
    const EliteAffixDef& unburied = MAffixes().Def(EliteAffixType::Unburied);
    CHECK(unburied.riseAgain.present);
    CHECK(unburied.riseAgain.chance == 1);
    CHECK(unburied.riseAgain.delayMs == 3000);
    CHECK(unburied.riseAgain.hpPct == 50);
    CHECK(unburied.families == std::vector<std::string>{"unburied"});
    CHECK(unburied.lootQualityBonus == 3);
    CHECK(unburied.hpMult == 1);
    CHECK(unburied.damageMult == 1);
    for (EliteAffixType t : MAffixes().order) {
      if (t != EliteAffixType::FireEnhanced && t != EliteAffixType::Frozen) CHECK_FALSE(MAffixes().Def(t).onDeath.present);
      if (t != EliteAffixType::Unburied) CHECK_FALSE(MAffixes().Def(t).riseAgain.present);
    }
  }
}  // TEST_SUITE

// =====================================================================================================================
// Quests + story: the Chapter 1 quest chain, the elder, beats, lore (from quests_test.cpp)
// Moved verbatim (QuestEngine / QuestUX / StoryScript ports and the quests-story-ch1.md section 16 vectors on the
// Chapter 1 data); the engine tests that need no Chapter 1 content stay in quests_test.cpp. Re-pinned by package 1B
// part 2 (dark remake narrative, Docs/dark-remake/chapter1.md 3-8): the eleven q_rf_* quests in their web slots, the
// Captain (quest_elder) giving M1-M5 and S1, Brother Aldwin (knell_chaplain) S2, S3, S5, S6, Corran (plains_wanderer)
// S4; the Captain's 13-node tree; the cs_rf_* beats.
// =====================================================================================================================

namespace {

// The strings Chapter 1 shows (1B review): a key that names a Chapter 1 id (quest, objective target, hunt, monster of
// the zone, mini-boss, NPC of a camp or the field, hidden area, story decoration, sub-dungeon entrance, lore, a beat
// those trigger), the zone, the prologue, the chapter card, the jukebox rows of the Chapter 1 soundtrack, the elite
// affixes and every achievement (the panel lists them all).
std::function<bool(const std::string&)> Ch1Shown(const DataStore& d) {
  const std::string zone = test::ch1::kZone;
  std::set<std::string> ids = {zone};
  for (const QuestDef& q : d.Quests().quests) {
    if (q.zone != zone) continue;
    ids.insert(q.id);
    for (const QuestObjectiveDef& o : q.objectives) ids.insert(o.targetId);
    for (const QuestHuntRef& h : q.hunts) ids.insert(h.huntId);
  }
  if (const ZoneMonsterList* z = d.Monsters().ZoneList(zone)) ids.insert(z->monsterIds.begin(), z->monsterIds.end());
  if (const MiniBossEntry* m = d.Monsters().MiniBossFor(zone)) ids.insert(m->monsterId);
  const MapDef* map = d.FindMap(zone);
  REQUIRE(map != nullptr);
  for (const MapCampDef& c : map->camps) ids.insert(c.npcs.begin(), c.npcs.end());
  for (const FieldNpcDef& f : map->fieldNpcs) ids.insert(f.npcId);
  for (const HiddenAreaDef& h : map->hiddenAreas) ids.insert(h.id);
  for (const StoryDecorationDef& sd : map->storyDecorations) ids.insert(sd.id);
  for (const SubDungeonEntranceDef& e : map->subDungeonEntrances) ids.insert(e.id);
  for (const LoreEntryDef& l : d.Lore().entries) {
    if (l.zone == zone) ids.insert(l.id);
  }
  for (const StoryTriggerDef& t : d.Story().triggers) {
    if (ids.count(t.subjectId) != 0) ids.insert(t.cutscene);
  }
  for (const BossIntroDef& b : d.Story().bossIntros) {
    if (ids.count(b.monsterId) != 0) ids.insert(b.cutscene);
  }
  return [ids = std::move(ids)](const std::string& key) {
    for (const char* prefix : {"story.prologue.", "story.chapter.emerald_plains.", "menu.jukebox.track.emerald_plains.",
                               "menu.jukebox.track.abyss_rift.explore", "data.achievement.", "sys.eliteAffix.",
                               "data.eliteAffix."}) {
      if (key.rfind(prefix, 0) == 0) return true;
    }
    size_t start = 0;
    while (start <= key.size()) {
      const size_t dot = key.find('.', start);
      const std::string seg = key.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
      if (ids.count(seg) != 0) return true;
      if (dot == std::string::npos) break;
      start = dot + 1;
    }
    return false;
  };
}

// The `strings` table of an exported i18n file (Data/i18n_<locale>.json).
JsonValue Ch1Strings(const char* file) {
  JsonValue doc;
  REQUIRE(ParseJson(test::ReadFile(test::DataDir() + "/" + file), doc));
  const JsonValue* table = doc.Find("strings");
  return table != nullptr ? *table : doc;
}

// UTF-8 -> code points (the exported tables are valid UTF-8; a stray byte decodes as itself).
std::vector<uint32_t> Ch1CodePoints(std::string_view s) {
  std::vector<uint32_t> out;
  for (size_t i = 0; i < s.size();) {
    const auto b = static_cast<unsigned char>(s[i]);
    const int extra = b >= 0xF0 ? 3 : b >= 0xE0 ? 2 : b >= 0xC0 ? 1 : 0;
    uint32_t cp = extra == 3 ? (b & 0x07u) : extra == 2 ? (b & 0x0Fu) : extra == 1 ? (b & 0x1Fu) : b;
    for (int k = 1; k <= extra && i + static_cast<size_t>(k) < s.size(); ++k) {
      cp = (cp << 6) | (static_cast<unsigned char>(s[i + static_cast<size_t>(k)]) & 0x3Fu);
    }
    out.push_back(cp);
    i += static_cast<size_t>(extra) + 1;
  }
  return out;
}

}  // namespace

TEST_SUITE("quests") {
  using namespace abyss::test::quests_fx;

  TEST_CASE("QuestSystem lists: open quests in insertion order, turned-in ids, tracking message") {
    test::SimHarness h;
    QuestSystem qs(h.ctx.data, h.events, h.bus);
    int tracked = 0;
    h.bus.Subscribe<QuestTrackedChangedMsg>([&](const QuestTrackedChangedMsg&) { ++tracked; });
    qs.Load({{"q_rf_burial_detail", QuestStatus::TurnedIn, {5}}, {"q_rf_black_blood", QuestStatus::Active, {0, 0}}});
    CHECK(qs.IsTurnedIn("q_rf_burial_detail"));
    CHECK(qs.TurnedInIds() == std::vector<std::string>{"q_rf_burial_detail"});
    const auto open = qs.OpenQuests();
    REQUIRE(open.size() == 1);
    CHECK(open[0].first->id == "q_rf_black_blood");
    qs.SetTracked("q_rf_black_blood");
    qs.SetTracked("q_rf_black_blood");
    CHECK(tracked == 1);
    CHECK(qs.Tracked() == "q_rf_black_blood");
  }

  TEST_CASE("announces accept, progress (flagging the completing step), completion and turn-in in order") {
    QsRig r;
    REQUIRE(r.qs.Accept("q_rf_burial_detail"));
    CHECK_FALSE(r.qs.Accept("q_rf_burial_detail"));  // already active
    r.qs.UpdateProgress(ObjectiveType::Kill, "risen_footman");
    r.qs.UpdateProgress(ObjectiveType::Kill, "risen_footman", 8);
    r.qs.UpdateProgress(ObjectiveType::Kill, "risen_footman", 5);  // overflow clamps at 10
    r.qs.UpdateProgress(ObjectiveType::Kill, "risen_footman");     // nothing advances: no event
    const QuestRewardDef* reward = r.qs.TurnIn("q_rf_burial_detail");
    REQUIRE(reward != nullptr);
    CHECK(reward->exp == 120);
    CHECK(reward->gold == 25);
    CHECK(r.log == std::vector<std::string>{"accepted:q_rf_burial_detail", "progress:q_rf_burial_detail:0:1",
                                            "progress:q_rf_burial_detail:0:9", "progress:q_rf_burial_detail:0:10:done",
                                            "completed:q_rf_burial_detail", "turnedIn:q_rf_burial_detail"});
    CHECK(r.Status("q_rf_burial_detail") == QuestStatus::TurnedIn);
    CHECK(r.qs.TurnIn("q_rf_burial_detail") == nullptr);  // terminal
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
    CHECK_FALSE(r.qs.Accept("q_rf_gnawed_field"));  // prereq q_rf_burial_detail not turned in
    r.qs.Load({Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {10})});
    CHECK(r.qs.Accept("q_rf_gnawed_field"));
    CHECK(r.qs.Accept("q_rf_marshal") == false);  // prereq chain
    CHECK(r.qs.Accept("q_rf_barrowmoss"));  // level 2 at any hero level (Q1)
  }

  TEST_CASE("guides the pinned quest, else the zone's first main quest; completed quests count") {
    QsRig r;
    REQUIRE(r.qs.Accept("q_rf_barrowmoss"));
    REQUIRE(r.qs.Accept("q_rf_burial_detail"));
    CHECK(r.qs.GuidedQuest("emerald_plains")->id == "q_rf_burial_detail");
    r.qs.SetTracked("q_rf_barrowmoss");
    CHECK(r.qs.GuidedQuest("emerald_plains")->id == "q_rf_barrowmoss");
    CHECK(r.qs.GuidedQuest("twilight_forest") == nullptr);
    r.qs.SetTracked("");
    r.qs.UpdateProgress(ObjectiveType::Kill, "risen_footman", 10);
    CHECK(r.qs.GuidedQuest("emerald_plains")->id == "q_rf_burial_detail");  // completed leads back to the giver
    r.qs.TurnIn("q_rf_burial_detail");
    CHECK(r.qs.GuidedQuest("emerald_plains")->id == "q_rf_barrowmoss");
  }

  TEST_CASE("turning in the tracked quest clears tracking") {
    QsRig r;
    r.qs.Load({Rec("q_rf_barrowmoss", QuestStatus::Completed, {5})});
    r.qs.SetTracked("q_rf_barrowmoss");
    REQUIRE(r.qs.TurnIn("q_rf_barrowmoss") != nullptr);
    CHECK(r.qs.Tracked().empty());
  }

  TEST_CASE("16.2 talk delivery gating: Marl counts only once the breach-blood samples are in hand") {
    QsRig r;
    REQUIRE(r.qs.Accept("q_rf_black_blood"));
    r.qs.UpdateProgress(ObjectiveType::Collect, "mat_breach_ichor", 5);
    r.log.clear();
    r.qs.UpdateProgress(ObjectiveType::Talk, "plains_herbalist");
    CHECK(r.Cur("q_rf_black_blood", 1) == 0);
    CHECK(r.log.empty());
    r.qs.UpdateProgress(ObjectiveType::Collect, "mat_breach_ichor");
    r.log.clear();
    r.qs.UpdateProgress(ObjectiveType::Talk, "plains_herbalist");
    CHECK(r.log == std::vector<std::string>{"progress:q_rf_black_blood:1:1:done", "completed:q_rf_black_blood"});
    CHECK(r.Status("q_rf_black_blood") == QuestStatus::Completed);
  }

  TEST_CASE("kill progress is not zone-filtered and one call advances every matching quest") {
    QsRig r;
    r.qs.Load({Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {10}),
               Rec("q_rf_gnawed_field", QuestStatus::Active, {0, 0}),
               Rec("q_rf_hallowed_ground", QuestStatus::Active, {0})});
    r.qs.UpdateProgress(ObjectiveType::Kill, "gravegnawer", 3);
    CHECK(r.Cur("q_rf_gnawed_field", 0) == 3);
    // hunt leaders carry the hunt id: no credit for plain objectives on their base monster
    r.qs.UpdateProgress(ObjectiveType::Kill, "hunt_harke");
    CHECK(r.Cur("q_rf_gnawed_field", 0) == 3);
  }

  TEST_CASE("escort / defend_wave / investigate_clue objectives advance through updateProgress") {
    QsRig r;
    r.qs.Load({Rec("q_rf_hallowed_ground", QuestStatus::Active, {0}),
               Rec("q_defend_camp_forest", QuestStatus::Active, {0}),
               Rec("q_rf_lanternwisp", QuestStatus::Active, {0, 0, 0, 0})});
    r.qs.UpdateProgress(ObjectiveType::Escort, "escort_rf_bern");
    CHECK(r.Status("q_rf_hallowed_ground") == QuestStatus::Completed);
    r.qs.UpdateProgress(ObjectiveType::DefendWave, "defend_forest_camp");
    r.qs.UpdateProgress(ObjectiveType::DefendWave, "defend_forest_camp");
    CHECK(r.Cur("q_defend_camp_forest", 0) == 2);
    CHECK(r.Status("q_defend_camp_forest") == QuestStatus::Active);
    r.qs.UpdateProgress(ObjectiveType::DefendWave, "defend_forest_camp");
    CHECK(r.Status("q_defend_camp_forest") == QuestStatus::Completed);
    // clues in any order
    r.qs.UpdateProgress(ObjectiveType::InvestigateClue, "clue_rf_lantern_3");
    r.qs.UpdateProgress(ObjectiveType::InvestigateClue, "clue_rf_lantern_1");
    CHECK(r.Cur("q_rf_lanternwisp", 0) == 1);
    CHECK(r.Cur("q_rf_lanternwisp", 1) == 0);
    CHECK(r.Cur("q_rf_lanternwisp", 2) == 1);
  }

  TEST_CASE("failQuest: active only; emits QUEST_FAILED; completed / missing records are untouched") {
    QsRig r;
    r.qs.Fail("q_rf_burial_detail");  // no record
    CHECK_FALSE(r.qs.HasRecord("q_rf_burial_detail"));
    r.qs.Load({Rec("q_rf_hallowed_ground", QuestStatus::Active, {0}),
               Rec("q_rf_black_blood", QuestStatus::Completed, {6, 1})});
    r.qs.Fail("q_rf_black_blood");
    CHECK(r.Status("q_rf_black_blood") == QuestStatus::Completed);
    r.qs.Fail("q_rf_hallowed_ground");
    CHECK(r.Status("q_rf_hallowed_ground") == QuestStatus::Failed);
    CHECK(r.log == std::vector<std::string>{"failed:q_rf_hallowed_ground"});
    r.qs.UpdateProgress(ObjectiveType::Escort, "escort_rf_bern");  // failed quests do not advance
    CHECK(r.Cur("q_rf_hallowed_ground", 0) == 0);
  }

  TEST_CASE("re-accepting a failed reacceptable quest resets its progress in place; others are refused") {
    QsRig r;
    r.qs.Load({Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {10}),
               Rec("q_rf_hallowed_ground", QuestStatus::Failed, {0}),
               Rec("q_rf_tag_thief", QuestStatus::Failed, {1, 1, 0, 0, 0}),
               Rec("q_defend_camp_forest", QuestStatus::Failed, {2})});
    // Q1: re-accept skips the prereq check (M2 is not turned in)
    CHECK(r.qs.Accept("q_rf_hallowed_ground"));
    CHECK(r.Status("q_rf_hallowed_ground") == QuestStatus::Active);
    CHECK_FALSE(r.qs.Accept("q_rf_tag_thief"));  // not reacceptable
    CHECK(r.Status("q_rf_tag_thief") == QuestStatus::Failed);
    CHECK(r.qs.Accept("q_defend_camp_forest"));
    CHECK(r.Cur("q_defend_camp_forest", 0) == 0);  // progress reset
    // map position kept: the escort quest stays second
    CHECK(r.qs.AllProgress()[1].questId == "q_rf_hallowed_ground");
    bool reaccepted = false;
    for (const Event& e : r.h.events.Items()) {
      if (const EvLog* l = std::get_if<EvLog>(&e)) reaccepted = reaccepted || l->text.key == "sys.quest.reaccepted";
    }
    CHECK(reaccepted);
  }

  TEST_CASE("16.10 load: redesigned active / completed quests restart; turned_in, failed and unknown ids are kept") {
    QsRig r;
    r.qs.Load({Rec("q_rf_black_blood", QuestStatus::Active, {4}),          // definition has 2 objectives
               Rec("q_rf_gnawed_field", QuestStatus::Completed, {1}),      // 2 objectives (gnawers + Hungerbell)
               Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {1, 2, 3}),  // kept as-is
               Rec("q_rf_tag_thief", QuestStatus::Failed, {1}),            // kept as-is
               Rec("q_removed_since", QuestStatus::Active, {1}),           // unknown: kept, ignored by lists
               Rec("q_rf_barrowmoss", QuestStatus::Active, {2}),
               Rec("q_rf_barrowmoss", QuestStatus::Active, {3})});         // duplicate replaces, keeps position
    CHECK(r.qs.Progress("q_rf_black_blood")->objectives == std::vector<int32_t>{0, 0});
    CHECK(r.Status("q_rf_gnawed_field") == QuestStatus::Active);
    CHECK(r.qs.Progress("q_rf_gnawed_field")->objectives == std::vector<int32_t>{0, 0});
    CHECK(r.qs.Progress("q_rf_burial_detail")->objectives.size() == 3);
    CHECK(r.Status("q_rf_tag_thief") == QuestStatus::Failed);
    CHECK(r.qs.HasRecord("q_removed_since"));
    CHECK(r.qs.AllProgress().size() == 6);
    CHECK(r.qs.AllProgress()[5].questId == "q_rf_barrowmoss");
    CHECK(r.Cur("q_rf_barrowmoss", 0) == 3);
    for (const auto& [q, p] : r.qs.OpenQuests()) CHECK(q->id != "q_removed_since");
  }

  TEST_CASE("save JSON round trip keeps statuses and counters of every quest kind") {
    const std::vector<QuestProgress> in = {Rec("q_rf_hallowed_ground", QuestStatus::Active, {0}),
                                           Rec("q_defend_camp_forest", QuestStatus::Active, {2}),
                                           Rec("q_rf_tag_thief", QuestStatus::Failed, {1, 1, 0, 0, 0}),
                                           Rec("q_craft_dwarf_weapon", QuestStatus::Active, {3, 2, 0, 0}),
                                           Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {10})};
    test::SimHarness h;
    QuestSystem qs(h.ctx.data, h.events, h.bus);
    qs.Load(in);
    std::vector<QuestProgress> out(qs.AllProgress().begin(), qs.AllProgress().end());
    CHECK(out == in);
  }

  // ---- offer lists (2.5) - QuestCardUI.test.ts gatherNpcQuests + QuestNPCIndicators.test.ts ------------------------
  TEST_CASE("16.4 the elder's first card at a new game, in NPC list order") {
    QsRig r;
    // The Captain: M1 and S1 (M2-M5 wait for their prereqs); Brother Aldwin: S2 and S5 (S3 needs M1, S6 needs M2).
    CHECK(OfferIds(r.qs.Offers("quest_elder", 1, OfferRule::QuestCard)) ==
          std::vector<std::string>{"q_rf_burial_detail", "q_rf_black_blood"});
    CHECK(OfferIds(r.qs.Offers("knell_chaplain", 1, OfferRule::QuestCard)) ==
          std::vector<std::string>{"q_rf_barrowmoss", "q_rf_lanternwisp"});
    CHECK(r.qs.Marker("quest_elder", 1) == NpcMarker::Available);
    CHECK(r.qs.Marker("knell_chaplain", 1) == NpcMarker::Available);
    CHECK(r.qs.Marker("plains_wanderer", 1) == NpcMarker::None);  // S4 needs M2
    CHECK(r.qs.Marker("blacksmith", 1) == NpcMarker::None);        // no quests
    CHECK(r.qs.Marker("plains_herbalist", 1) == NpcMarker::None);  // delivery target only
    CHECK(r.qs.Offers("blacksmith", 1, OfferRule::QuestCard).empty());
  }

  TEST_CASE("quest card: turn-ins first, active and turned-in excluded, level gate +5 inclusive, prereqs") {
    QsRig r;
    r.qs.Load({Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {10}),
               Rec("q_rf_gnawed_field", QuestStatus::TurnedIn, {12, 1}),
               Rec("q_rf_ashwheat", QuestStatus::TurnedIn, {1, 1}),
               Rec("q_rf_black_blood", QuestStatus::Completed, {6, 1}),
               Rec("q_rf_barrowmoss", QuestStatus::Active, {1})});
    // hero L1: q_rf_marshal (lvl 7) is above L1 + 5
    std::vector<std::string> l1 = OfferIds(r.qs.Offers("quest_elder", 1, OfferRule::QuestCard));
    CHECK(l1.front() == "T:q_rf_black_blood");
    CHECK(std::find(l1.begin(), l1.end(), "q_rf_marshal") == l1.end());
    CHECK(std::find(l1.begin(), l1.end(), "q_rf_burial_detail") == l1.end());  // turned in
    // Brother Aldwin's card: the active quest is left out, the prereq-met one offered
    std::vector<std::string> aldwin = OfferIds(r.qs.Offers("knell_chaplain", 1, OfferRule::QuestCard));
    CHECK(std::find(aldwin.begin(), aldwin.end(), "q_rf_barrowmoss") == aldwin.end());  // active
    CHECK(std::find(aldwin.begin(), aldwin.end(), "q_rf_tag_thief") != aldwin.end());   // prereq met, lvl 4
    // exactly hero level + 5 is offered
    std::vector<std::string> l2 = OfferIds(r.qs.Offers("quest_elder", 2, OfferRule::QuestCard));
    CHECK(std::find(l2.begin(), l2.end(), "q_rf_marshal") != l2.end());
    // turn-ins are never level gated
    r.qs.Load({Rec("q_rf_last_toll", QuestStatus::Completed, {3})});  // the M5 slot defends 3 waves (1B part 1)
    CHECK(OfferIds(r.qs.Offers("quest_elder", 1, OfferRule::QuestCard)).front() == "T:q_rf_last_toll");
  }

  TEST_CASE("failed reacceptable quests are offered again; failed non-reacceptable ones are not") {
    QsRig r;
    r.qs.Load({Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {10}),
               Rec("q_rf_gnawed_field", QuestStatus::TurnedIn, {12, 1}),
               Rec("q_rf_hallowed_ground", QuestStatus::Failed, {0}),
               Rec("q_rf_tag_thief", QuestStatus::Failed, {0, 0, 0, 0, 0})});
    for (OfferRule rule : {OfferRule::QuestCard, OfferRule::Available}) {
      const std::vector<std::string> ids = OfferIds(r.qs.Offers("knell_chaplain", 5, rule));
      CHECK(std::find(ids.begin(), ids.end(), "q_rf_hallowed_ground") != ids.end());
      CHECK(std::find(ids.begin(), ids.end(), "q_rf_tag_thief") == ids.end());
    }
  }

  TEST_CASE("NPC markers: priority turn-in > available > in progress > none; lifecycle on real data") {
    QsRig r;
    // plains_wanderer gives only q_rf_oathless (lvl 7, prereq M2).
    CHECK(r.qs.Marker("plains_wanderer", 1) == NpcMarker::None);
    r.qs.Load({Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {10}),
               Rec("q_rf_gnawed_field", QuestStatus::TurnedIn, {12, 1})});
    CHECK(r.qs.Marker("plains_wanderer", 1) == NpcMarker::None);  // 7 > 1 + 5
    CHECK(r.qs.Marker("plains_wanderer", 2) == NpcMarker::Available);
    REQUIRE(r.qs.Accept("q_rf_oathless"));
    CHECK(r.qs.Marker("plains_wanderer", 2) == NpcMarker::InProgress);
    r.qs.UpdateProgress(ObjectiveType::Kill, "hunt_harke");
    CHECK(r.qs.Marker("plains_wanderer", 2) == NpcMarker::TurnIn);
    r.qs.TurnIn("q_rf_oathless");
    CHECK(r.qs.Marker("plains_wanderer", 2) == NpcMarker::None);
    // the elder: available beats in progress, turn-in beats both
    QsRig e;
    REQUIRE(e.qs.Accept("q_rf_burial_detail"));
    CHECK(e.qs.Marker("quest_elder", 1) == NpcMarker::Available);
    e.qs.UpdateProgress(ObjectiveType::Kill, "risen_footman", 10);
    CHECK(e.qs.Marker("quest_elder", 1) == NpcMarker::TurnIn);
    e.qs.TurnIn("q_rf_burial_detail");
    CHECK(e.qs.Marker("quest_elder", 3) == NpcMarker::Available);  // M2 now available
  }

  TEST_CASE("chapter 1 quest table (10.1): 11 quests, rewards and totals; Q4 Marshal area; Q8 escort label") {
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
    CHECK(exp == 2740);  // chapter1.md 7 (DR9): today's 2660 / 505 plus the remake's re-tuned main line
    CHECK(gold == 510);
    const QuestDef* chief = data.FindQuest("q_rf_marshal");
    REQUIRE(chief != nullptr);
    CHECK(chief->questArea.col == 15);  // Q4: centred on Valdras's spawn (15,95); the data keeps the slot's (25,65)
    CHECK(chief->questArea.row == 95);
    const QuestDef* escort = data.FindQuest("q_rf_hallowed_ground");
    REQUIRE(escort != nullptr);
    CHECK(escort->level * 20 + 100 == 200);
    CHECK(escort->objectives[0].labelKey == escort->nameKey);  // Q8: no "southern camp" claim
    CHECK(escort->escortNpc.dest == TilePos(50, 90));
    const QuestDef* ichor = data.FindQuest("q_rf_black_blood");
    REQUIRE(ichor != nullptr);
    CHECK(ichor->objectives[0].itemKind == "ichor");  // art lane quest item (A11); UE falls back while it is missing
    CHECK(ichor->rewards.items == std::vector<std::string>{"c_hp_potion_s", "c_hp_potion_s"});
  }

  // 1B review: the strings a Chapter 1 player can see (Ch1Shown) keep no web plains name (翠绿平原 / Emerald Plains, the
  // jukebox rows included) and none of story-bible.md 12's never-used words (Fallen, Butcher, ... and Diablo III's "Demon
  // Hunter"); final review: and no exposed skeleton or skull (story-bible.md 12 Gore; the Relic Peddler's tray once held
  // 白骨 / "white bones").
  TEST_CASE("Chapter 1 words: no web plains name and no never-used word in the strings Chapter 1 shows") {
    const DataStore& d = test::RealData();
    const auto shown = Ch1Shown(d);
    // Whole words, ignoring ASCII case.
    const auto hasWord = [](const std::string& text, std::string_view word) {
      const auto lower = [](char c) { return static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c); };
      const auto isAlpha = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };
      for (size_t i = 0; i + word.size() <= text.size(); ++i) {
        bool eq = true;
        for (size_t j = 0; j < word.size() && eq; ++j) eq = lower(text[i + j]) == lower(word[j]);
        if (!eq) continue;
        const bool left = i == 0 || !isAlpha(text[i - 1]);
        const bool right = i + word.size() == text.size() || !isAlpha(text[i + word.size()]);
        if (left && right) return true;
      }
      return false;
    };
    const std::vector<std::string_view> enAvoid = {
        "Emerald", "Plains", "Diablo", "Sanctuary", "Tristram", "Deckard", "Leoric", "Skeleton King", "Lachdanan",
        "Lazarus", "Butcher", "Fallen", "Lilith", "Andariel", "Duriel", "Mephisto", "Baal", "Belial", "Horadric",
        "Horadrim", "Zakarum", "Nephalem", "Worldstone", "Kashya", "Akara", "Ogden", "Griswold", "Adria", "Wirt",
        "Blood Raven", "Helltide", "Wraeclast", "Exile", "Cairn", "Aetherial", "Chthonian", "Eterra", "Coven", "Nevesk",
        "Stay awhile", "Fresh meat", "Demon Hunter", "skeleton", "skeletons", "skull", "skulls", "white bones"};
    const std::vector<std::string> zhAvoid = {"\xE5\xB9\xB3\xE5\x8E\x9F", "\xE7\xBF\xA1\xE7\xBF\xA0",
                                              "\xE7\xBF\xA0\xE7\xBB\xBF",  // 平原, 翡翠, 翠绿
                                              "\xE7\x99\xBD\xE9\xAA\xA8", "\xE9\xAA\xB7\xE9\xAB\x85",
                                              "\xE5\xA4\xB4\xE9\xAA\xA8", "\xE9\xA2\x85\xE9\xAA\xA8"};  // 白骨, 骷髅, 头骨, 颅骨
    const JsonValue zh = Ch1Strings("i18n_zh-CN.json");
    const JsonValue en = Ch1Strings("i18n_en.json");
    std::vector<std::string> bad;
    size_t checked = 0;
    for (const auto& [key, value] : zh.Members()) {
      if (!value.IsString() || !shown(key)) continue;
      ++checked;
      for (const std::string& w : zhAvoid) {
        if (std::string(value.AsString()).find(w) != std::string::npos) bad.push_back("zh " + key);
      }
    }
    for (const auto& [key, value] : en.Members()) {
      if (!value.IsString() || !shown(key)) continue;
      ++checked;
      for (std::string_view w : enAvoid) {
        if (hasWord(std::string(value.AsString()), w)) bad.push_back("en " + key + " (" + std::string(w) + ")");
      }
    }
    INFO(bad.size() << " hit(s), e.g. " << (bad.empty() ? std::string() : bad.front()));
    for (const std::string& b : bad) MESSAGE(b);
    CHECK(bad.empty());
    CHECK(checked > 400);  // the selection reaches the chapter's strings
    // The jukebox rows of the Chapter 1 soundtrack name the Red Field; the prologue's recording is titled after its abyss
    // slide (the title-menu track is already "Abyssfire · Prologue").
    const I18n en2 = EnglishStrings();
    CHECK(en2.T("menu.jukebox.track.emerald_plains.explore") == "The Red Field \xC2\xB7 Explore");
    CHECK(en2.T("menu.jukebox.track.abyss_rift.explore") == "Abyssfire \xC2\xB7 The Whisper");
    CHECK(std::string(zh.Get("menu.jukebox.track.abyss_rift.explore").AsString()) ==
          "\xE6\xB8\x8A\xE7\x81\xAB \xC2\xB7 \xE4\xBD\x8E\xE8\xAF\xAD");  // 渊火 · 低语
    CHECK(en2.T("data.storyDeco.story_rf_belfry.name") == "Ashwheat's Broken Belfry");
  }

  // Final review: every row of the title menu's jukebox (Audio/Export/audio_manifest.json jukebox.tracks, the table the
  // UE audio manifest reads) has a title in each locale, and no two rows read the same - the abyss_rift row once repeated
  // the title-menu track's 渊火 · 序章 / Abyssfire · Prologue.
  TEST_CASE("Chapter 1 jukebox: every row's titleKey resolves, to a distinct title in zh-CN, zh-TW and en") {
    JsonValue manifest;
    REQUIRE(ParseJson(test::ReadFile(test::UnrealDir() + "/Audio/Export/audio_manifest.json"), manifest));
    const JsonValue* tracks = manifest.Get("jukebox").Find("tracks");
    REQUIRE(tracks != nullptr);
    std::vector<std::string> keys;
    for (const JsonValue& t : tracks->Items()) keys.push_back(std::string(t.Get("titleKey").AsString()));
    REQUIRE(keys.size() >= 4);
    CHECK(std::find(keys.begin(), keys.end(), "menu.jukebox.track.menu") != keys.end());
    CHECK(std::find(keys.begin(), keys.end(), "menu.jukebox.track.abyss_rift.explore") != keys.end());
    for (const char* file : {"i18n_zh-CN.json", "i18n_zh-TW.json", "i18n_en.json"}) {
      INFO(file);
      const JsonValue table = Ch1Strings(file);
      std::set<std::string> titles;
      for (const std::string& k : keys) {
        INFO(k);
        const JsonValue* v = table.Find(k);
        REQUIRE(v != nullptr);
        REQUIRE(v->IsString());
        const std::string title(v->AsString());
        CHECK_FALSE(title.empty());
        CHECK(titles.insert(title).second);  // distinct
      }
    }
  }

  // Final review R4: the remake's own Chapter 1 zh-TW strings are Traditional throughout (shared web strings are not checked). The web converter's map (src/i18n/converter.ts) misses
  // many characters (131 of the 315 remake strings kept Simplified ones: 军 尸 麦 边 没 着 ...), so the exporter runs the
  // remake's strings through GameData/i18n/zhTWExtra.ts as well (web-sourced strings keep the web output). No zh-TW string
  // Chapter 1 shows (Ch1Shown) contains a Simplified-only code point (golden/zh_simplified_only.txt: characters whose every
  // Traditional / Taiwan form differs, derived from OpenCC; 后 里 于 余 ... are Traditional too and not listed). The shared
  // UI (`ui.*`, which Ch1Shown reaches only through the stash NPC's id: ui.stash.bagFull 背包已满) stays the web
  // converter's byte for byte: the supplement covers the remake's strings, not the web UI.
  TEST_CASE("Chapter 1 zh-TW: no string Chapter 1 shows keeps a Simplified-only character") {
    std::set<uint32_t> simplifiedOnly;
    const std::string list = test::ReadFile(test::GoldenDir() + "/zh_simplified_only.txt");
    REQUIRE_FALSE(list.empty());
    size_t lineStart = 0;
    while (lineStart < list.size()) {
      size_t lineEnd = list.find('\n', lineStart);
      if (lineEnd == std::string::npos) lineEnd = list.size();
      const std::string_view line(list.data() + lineStart, lineEnd - lineStart);
      if (!line.empty() && line.front() != '#') {
        for (uint32_t cp : Ch1CodePoints(line)) {
          if (cp > 0x7F) simplifiedOnly.insert(cp);
        }
      }
      lineStart = lineEnd + 1;
    }
    // The list is the careful one: the characters the review named are in it, the shared ones are not.
    CHECK(simplifiedOnly.size() > 2000);
    for (uint32_t cp : {0x519Bu, 0x9EA6u, 0x8FB9u, 0x6CA1u, 0x7740u, 0x70ECu, 0x7ED1u, 0x5BFBu}) {  // 军 麦 边 没 着 烬 绑 寻
      CHECK(simplifiedOnly.count(cp) == 1);
    }
    for (uint32_t cp : {0x540Eu, 0x91CCu, 0x4E8Eu, 0x4F59u, 0x91C7u, 0x5C38u, 0x53EAu, 0x53F0u, 0x5E72u, 0x9762u,
                        0x7CFBu, 0x5236u, 0x5E8Au, 0x7FA4u}) {  // 后 里 于 余 采 尸 只 台 干 面 系 制 床 群
      CHECK(simplifiedOnly.count(cp) == 0);
    }
    const DataStore& d = test::RealData();
    const auto shown = Ch1Shown(d);
    const JsonValue tw = Ch1Strings("i18n_zh-TW.json");
    std::vector<std::string> bad;
    size_t checked = 0;
    for (const auto& [key, value] : tw.Members()) {
      if (!value.IsString() || !shown(key) || key.rfind("ui.", 0) == 0) continue;
      ++checked;
      std::string hits;
      for (uint32_t cp : Ch1CodePoints(value.AsString())) {
        if (simplifiedOnly.count(cp) == 0) continue;
        char hex[16];
        std::snprintf(hex, sizeof hex, " U+%04X", static_cast<unsigned>(cp));
        hits += hex;
      }
      if (!hits.empty()) bad.push_back(key + ":" + hits);
    }
    INFO(bad.size() << " string(s), e.g. " << (bad.empty() ? std::string() : bad.front()));
    for (const std::string& b : bad) MESSAGE(b);
    CHECK(bad.empty());
    CHECK(checked > 200);
    // A default and three context phrases (zhTWExtra.ts): the Corpseherd's 尸 is 屍, the Captain's 十二只 (M2) is 十二隻, the
    // dais is 石臺, the dried blood 乾.
    CHECK(std::string(tw.Get("data.monster.miniboss_corpseherd").AsString()) ==
          "\xE5\xB1\x8D\xE7\x89\xA7\xE5\xA9\x86");  // 屍牧婆
    CHECK(std::string(tw.Get("data.dialogue.quest_elder.accept_gnawers.text").AsString()).find(
              "\xE5\x8D\x81\xE4\xBA\x8C\xE9\x9A\xBB") != std::string::npos);  // 十二隻
    CHECK(std::string(tw.Get("data.lore.lore_rf_echo_03.name").AsString()).find("\xE7\x9F\xB3\xE8\x87\xBA") !=
          std::string::npos);  // 石臺
    CHECK(std::string(tw.Get("data.storyDeco.story_rf_penitents_altar.desc").AsString()).find("\xE4\xB9\xBE") !=
          std::string::npos);  // 乾
  }

  // chapter1.md 3-7 (package 1B part 2, narrative): who gives what, the lines every quest carries, the labelled moral
  // choices, the re-cast NPCs, the Captain's tree and the story-bible zh limits (story-bible.md 11: offer <= 70,
  // progress and complete <= 50, dialogue <= 60 code points; the script's limits are quests_test's).
  TEST_CASE("Chapter 1 narrative: givers, quest lines, labelled choices, re-cast NPCs, the Captain's tree, zh limits") {
    const DataStore& d = test::RealData();
    const I18n& s = d.Strings();
    const I18n en = EnglishStrings();
    std::vector<std::string> over;
    const auto line = [&](const std::string& key, size_t max) {
      CAPTURE(key);
      const std::string* zh = s.Lookup(LocaleId::ZhCN, key);
      REQUIRE(zh != nullptr);
      CHECK(s.Lookup(LocaleId::En, key) != nullptr);
      if (max > 0 && CodePoints(*zh) > max) over.push_back(key);
    };
    // Givers (chapter1.md 7): the Captain 6 (the main line and S1), Brother Aldwin 4, Corran 1.
    CHECK(d.FindNpc("quest_elder")->quests == std::vector<std::string>{"q_rf_burial_detail", "q_rf_black_blood",
                                                                       "q_rf_gnawed_field", "q_rf_ashwheat",
                                                                       "q_rf_marshal", "q_rf_last_toll"});
    CHECK(d.FindNpc("knell_chaplain")->quests == std::vector<std::string>{"q_rf_barrowmoss", "q_rf_tag_thief",
                                                                          "q_rf_lanternwisp", "q_rf_hallowed_ground"});
    CHECK(d.FindNpc("plains_wanderer")->quests == std::vector<std::string>{"q_rf_oathless"});
    // Every quest: name, quest-log line, offer, progress line (K1 progressKey) and turn-in line.
    std::vector<std::string> order;
    for (const QuestDef& q : d.Quests().quests) {
      if (q.zone != "emerald_plains") continue;
      CAPTURE(q.id);
      order.push_back(q.id);
      CHECK(q.progressKey == "data.quest." + q.id + ".progress");
      line(q.nameKey, 12);
      line(q.descKey, 70);
      line(q.offerKey, 70);
      line(q.progressKey, 50);
      line(q.completeKey, 50);
    }
    // The web slots' order (DR3): the main line and S1-S4 first; S5 / S6 hold the pet and escort slots further down.
    CHECK(order == std::vector<std::string>{"q_rf_burial_detail", "q_rf_gnawed_field", "q_rf_ashwheat", "q_rf_marshal",
                                            "q_rf_last_toll", "q_rf_black_blood", "q_rf_barrowmoss", "q_rf_tag_thief",
                                            "q_rf_oathless", "q_rf_lanternwisp", "q_rf_hallowed_ground"});
    // M3 explores Ashwheat, then kills the Corpseherd; the prereq chain M1 -> M2 -> M3 -> M4 -> M5.
    const QuestDef* ash = d.FindQuest("q_rf_ashwheat");
    REQUIRE(ash->objectives.size() == 2);
    CHECK(ash->objectives[0].targetId == "zone_rf_ashwheat");
    CHECK(ash->objectives[1].targetId == "miniboss_corpseherd");
    CHECK(d.FindQuest("q_rf_last_toll")->prereqQuests == std::vector<std::string>{"q_rf_marshal"});
    // The moral choices (S3, S4): one label per slot, by slot.
    const QuestDef* tag = d.FindQuest("q_rf_tag_thief");
    CHECK(tag->rewards.choices == std::vector<RewardSlot>{RewardSlot::Jewelry, RewardSlot::Weapon});
    CHECK(tag->rewards.choiceLabels == std::vector<std::string>{"data.questChoice.q_rf_tag_thief.jewelry",
                                                               "data.questChoice.q_rf_tag_thief.weapon"});
    CHECK(en.T(tag->rewards.choiceLabels[0]) == "Keep the melted silver");
    CHECK(en.T(tag->rewards.choiceLabels[1]) == "Give it to Hald");
    const QuestDef* bounty = d.FindQuest("q_rf_oathless");
    CHECK(bounty->rewards.choices == std::vector<RewardSlot>{RewardSlot::Weapon, RewardSlot::Offhand});
    CHECK(en.T(bounty->rewards.choiceLabels[0]) == "The head to Corran");
    CHECK(en.T(bounty->rewards.choiceLabels[1]) == "The head to the Captain");
    for (const QuestDef& q : d.Quests().quests) {
      if (q.zone == "emerald_plains" && q.id != tag->id && q.id != bounty->id) CHECK(q.rewards.choiceLabels.empty());
    }
    // The Lanternwisp (S5): Pell's lantern, the chapter's only companion.
    const QuestDef* wisp = d.FindQuest("q_rf_lanternwisp");
    CHECK(wisp->rewards.petReward == "pet_sprite");
    CHECK(en.T("data.pet.pet_sprite.name") == "Lanternwisp");
    // The escort and the gate (Q11 keys) and the clue notes.
    CHECK(en.T(EscortNpcNameKey("q_rf_hallowed_ground")) == "Old Bern");
    CHECK(en.T(DefendTargetNameKey("q_rf_last_toll")) == "The Boneyard Gate");
    for (const QuestDef& q : d.Quests().quests) {
      if (q.zone != "emerald_plains") continue;
      for (const QuestObjectiveDef& o : q.objectives) {
        line("data.questTarget." + o.targetId, 0);
        if (o.type == ObjectiveType::InvestigateClue) line("data.questClue." + o.targetId, 60);
      }
    }
    // Knellwatch and the field, re-cast (chapter1.md 3): names, one Wave 1 line each (Aldwin two), the gated lines.
    const std::pair<const char*, const char*> cast[] = {
        {"quest_elder", "Captain Serin Harrow"}, {"knell_chaplain", "Brother Aldwin"},
        {"blacksmith", "Hald Swordmelter"},      {"merchant", "Quartermaster Vira"},
        {"stash", "Tamsin the Gravewarden"},     {"plains_herbalist", "Mother Marl"},
        {"plains_wanderer", "Corran the Deserter"}, {"crow_trader", "Mag Crowe"}};
    for (const auto& [id, name] : cast) {
      CAPTURE(id);
      const NpcDef* n = d.FindNpc(id);
      REQUIRE(n != nullptr);
      CHECK(en.T(n->nameKey) == name);
      CHECK(n->dialogue.size() == (std::string_view(id) == "knell_chaplain" ? 2u : 1u));
      for (size_t i = 0; i < n->dialogue.size(); ++i) {
        line("data.npc." + std::string(id) + ".dialogue." + std::to_string(i), 60);
      }
      CHECK_FALSE(s.Has("data.npc." + std::string(id) + ".dialogue." + std::to_string(n->dialogue.size())));
    }
    for (const char* k : {"data.npc.blacksmith.after.q_rf_tag_thief", "data.npc.quest_elder.after.q_rf_oathless",
                          "data.npc.knell_chaplain.after.q_rf_last_toll"}) {
      line(k, 60);
    }
    // The Captain's tree (chapter1.md 4): 13 nodes, three quest triggers, the bot's walk reward_ask -> help ->
    // other_work -> accept_ichor (implementation-plan.md 6).
    const DialogueTree* tree = d.Dialogues().Find("quest_elder");
    REQUIRE(tree != nullptr);
    CHECK(tree->nodes.size() == 13);
    std::vector<std::string> triggers;
    for (const DialogueNode& n : tree->nodes) {
      line("data.dialogue.quest_elder." + n.id + ".text", 60);
      for (size_t i = 0; i < n.choices.size(); ++i) {
        line("data.dialogue.quest_elder." + n.id + ".choice." + std::to_string(i), 12);
        if (!n.choices[i].questTrigger.empty()) triggers.push_back(n.choices[i].questTrigger);
      }
    }
    CHECK(triggers == std::vector<std::string>{"q_rf_burial_detail", "q_rf_gnawed_field", "q_rf_black_blood"});
    const char* walk[] = {"root", "reward_ask", "help", "other_work", "accept_ichor"};
    for (size_t i = 0; i + 1 < std::size(walk); ++i) {
      CAPTURE(walk[i]);
      const DialogueNode* n = tree->FindNode(walk[i]);
      REQUIRE(n != nullptr);
      CHECK(std::any_of(n->choices.begin(), n->choices.end(),
                        [&](const DialogueChoice& c) { return c.nextNodeId == walk[i + 1]; }));
    }
    CHECK(tree->FindNode("accept_ichor")->isEnd);
    INFO("zh lines over the story-bible limit: " << over.size());
    CHECK(over.empty());
  }

  // ---- reward choices (4.2) - QuestEngine.test.ts QuestRewards ---------------------------------------------------------
  TEST_CASE("16.8 reward item level and quality; drop chances") {
    const DataStore& data = test::RealData();
    const QuestDef* m4 = data.FindQuest("q_rf_marshal");
    REQUIRE(m4 != nullptr);
    CHECK(RewardItemLevel(*m4, 3) == 7);
    CHECK(RewardItemLevel(*m4, 10) == 10);
    CHECK(RewardItemLevel(*m4, 15) == 12);
    CHECK(RewardChoiceQuality(data, *m4) == ItemQuality::Rare);
    CHECK(RewardChoiceQuality(data, *data.FindQuest("q_rf_tag_thief")) == ItemQuality::Magic);
  }

  TEST_CASE("16.1 gather hash: FNV state, first draws and the all-walkable spots") {
    // chapter1.md S2: the barrowmoss spots are seeded by "q_rf_barrowmoss:0" (the web key "q_herb_gathering:0" gave
    // state 3613349914 and the spots (23,25) (28,23) (20,17) (18,32) (23,34) (27,29) (23,18)); area and count kept.
    GatherSpotRng rng("q_rf_barrowmoss:0");
    CHECK(rng.State() == 3706193809u);
    const double expected[] = {0.257193835, 0.840027817, 0.115078570, 0.432658153, 0.268157336, 0.646995244};
    for (double e : expected) CHECK(rng.Next() == doctest::Approx(e).epsilon(1e-8));
    const auto all = [](int32_t, int32_t) { return true; };
    CHECK(ResolveGatherSpots(TileCircle{22, 26, 9}, 7, all, "q_rf_barrowmoss:0") ==
          std::vector<TilePos>{{22, 34}, {26, 30}, {18, 21}, {30, 23}, {20, 33}, {21, 23}, {25, 35}});
    // floor(x + 0.5) on negative offsets
    CHECK(ResolveGatherSpots(TileCircle{0, 0, 5}, 3, all, "abc") == std::vector<TilePos>{{-1, -3}, {-4, -2}, {0, 4}});
  }

  TEST_CASE("achievements: targeted breachclot kills also count toward the generic key, unlock at 50") {
    AchievementState a(test::RealData());
    for (int i = 0; i < 30; ++i) a.Update(AchievementType::Kill, "breachclot");
    CHECK(a.ProgressOf("kill:breachclot") == 30);
    CHECK(a.ProgressOf("kill") == 30);
    CHECK_FALSE(a.IsUnlocked("ach_kill_breachclot"));
    for (int i = 0; i < 20; ++i) a.Update(AchievementType::Kill, "breachclot");
    CHECK(a.IsUnlocked("ach_kill_breachclot"));  // "Stanch the Seep"
    CHECK(a.IsUnlocked("ach_first_kill"));
    // boss kill tracked separately from generic kills
    CHECK(a.Update(AchievementType::Kill, "valdras") == std::vector<std::string>{"ach_kill_valdras"});
    CHECK(a.ProgressOf("kill:valdras") == 1);
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
    for (int i = 0; i < 50; ++i) a.Update(AchievementType::Kill, "breachclot");  // ach_kill_breachclot: lck +2 (kept)
    CHECK(a.Bonuses().Get(Stat::Lck) == 7);
  }

  TEST_CASE("achievements: save format merges counters with {id: 1}; load splits them back; no drift") {
    AchievementState a(test::RealData());
    for (int i = 0; i < 12; ++i) a.Update(AchievementType::Kill, "breachclot");
    a.CheckLevel(10);
    const auto saved = a.ToSave();
    CHECK(saved == std::vector<std::pair<std::string, int64_t>>{
                       {"kill", 12}, {"kill:breachclot", 12}, {"ach_first_kill", 1}, {"ach_level_10", 1}});
    AchievementState b(test::RealData());
    b.Load(saved);
    CHECK(b.IsUnlocked("ach_first_kill"));
    CHECK(b.IsUnlocked("ach_level_10"));
    CHECK(b.ProgressOf("kill") == 12);
    CHECK(b.Bonuses() == a.Bonuses());
    CHECK(b.ToSave() == saved);
    CHECK(b.Update(AchievementType::Kill, "breachclot").empty());  // no re-unlock after load
    AchievementState c(test::RealData());
    c.Load({});
    CHECK(c.ToSave().empty());
  }

  // The Captain's tree (chapter1.md 4): root -> who / bell / help / reward_ask / farewell; help -> accept_burial
  // [Q: M1], gnawers_first (after M1) -> accept_gnawers [Q: M2], other_work -> accept_ichor [Q: S1] / chaplain_pointer.
  TEST_CASE("dialogue runtime: root choices, help -> accept_burial accepts M1 without pinning it, Leave closes") {
    DlgRig r;
    r.Open("quest_elder");
    CHECK(r.dlg.View().open);
    CHECK(r.dlg.View().nodeId == "root");
    CHECK(r.Kinds() == std::vector<DialogueButtonKind>(5, DialogueButtonKind::Choice));
    r.dlg.Choose(r.ChoiceButton(2));  // help
    CHECK(r.dlg.View().nodeId == "help");
    r.dlg.Choose(r.ChoiceButton(0));  // accept_burial [Q:q_rf_burial_detail]
    CHECK(r.dlg.View().nodeId == "accept_burial");
    CHECK(r.qs.Progress("q_rf_burial_detail") != nullptr);
    CHECK(r.qs.Tracked().empty());  // dialogue-tree acceptances do not pin the guide
    CHECK(r.Kinds() == std::vector<DialogueButtonKind>{DialogueButtonKind::Leave});
    r.dlg.Choose(0);
    CHECK_FALSE(r.dlg.View().open);
    CHECK(test::CountEvents<EvDialogue>(r.h.events) >= 3);
  }

  TEST_CASE("dialogue runtime: a handled quest hides its end-node choice; all hidden -> Back (10.3 example)") {
    DlgRig r;
    REQUIRE(r.qs.Accept("q_rf_burial_detail"));
    r.Open("quest_elder");
    r.dlg.Choose(r.ChoiceButton(2));  // help
    REQUIRE(r.dlg.View().nodeId == "help");
    CHECK(r.ChoiceButton(0) == -1);  // accept_burial hidden (quest active, end node)
    CHECK(r.ChoiceButton(1) == -1);  // gnawers_first gated on q_rf_burial_detail turned in
    REQUIRE(r.ChoiceButton(2) >= 0);  // other_work: the hidden quest's sibling, nothing in progress
    CHECK_FALSE(r.dlg.View().buttons[static_cast<size_t>(r.ChoiceButton(2))].inProgress);
    // M1 turned in and M2 active: gnawers_first opens, but its only choice (accept_gnawers) is handled -> Back
    DlgRig g;
    g.qs.Load({Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {10}),
               Rec("q_rf_gnawed_field", QuestStatus::Active, {0, 0})});
    g.Open("quest_elder");
    g.dlg.Choose(g.ChoiceButton(2));  // help
    REQUIRE(g.ChoiceButton(1) >= 0);
    g.dlg.Choose(g.ChoiceButton(1));  // gnawers_first
    REQUIRE(g.dlg.View().nodeId == "gnawers_first");
    CHECK(g.ChoiceButton(0) == -1);
    CHECK(g.Kinds() == std::vector<DialogueButtonKind>{DialogueButtonKind::Back});
    g.dlg.Choose(0);
    CHECK(g.dlg.View().nodeId == "root");
  }

  TEST_CASE("dialogue runtime: prereq choices use the quests turned in when the NPC was clicked; in-progress flag") {
    DlgRig r;
    r.qs.Load({Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {10})});
    r.Open("quest_elder");
    r.dlg.Choose(r.ChoiceButton(2));  // help
    const int32_t gnaw = r.ChoiceButton(1);
    REQUIRE(gnaw >= 0);  // gnawers_first unlocked
    CHECK_FALSE(r.dlg.View().buttons[static_cast<size_t>(gnaw)].inProgress);
    r.dlg.Choose(gnaw);
    REQUIRE(r.dlg.View().nodeId == "gnawers_first");
    r.dlg.Choose(r.ChoiceButton(0));  // accept q_rf_gnawed_field
    CHECK(r.dlg.View().nodeId == "accept_gnawers");
    CHECK(r.qs.Progress("q_rf_gnawed_field") != nullptr);
    // visited / choicesMade are recorded per NPC and saved
    SaveData save;
    r.dlg.WriteSave(save);
    REQUIRE(save.dialogueState.size() == 1);
    CHECK(save.dialogueState[0].npcId == "quest_elder");
    CHECK(save.dialogueState[0].visitedNodes ==
          std::vector<std::string>{"root", "help", "gnawers_first", "accept_gnawers"});
    CHECK(save.dialogueState[0].choicesMade.size() == 3);
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

  TEST_CASE("T15: turning in M1 queues cs_rf_brand 650 ms later on the sim clock; skip keeps letterbox-out + return") {
    QwSim s;
    s.Qs().Load({Rec("q_rf_burial_detail", QuestStatus::Completed, {10})});
    const int64_t gold0 = s.H().Gold();
    REQUIRE(s.Qw().TurnIn("q_rf_burial_detail", 0));
    CHECK(s.H().Gold() == gold0 + 25);
    StoryDirector& st = s.Story();
    CHECK(st.IsBusy());
    CHECK_FALSE(st.IsCinematic());
    CHECK_FALSE(s.sim->WorldFrozen());
    s.Steps(38);
    CHECK_FALSE(st.IsCinematic());  // the world stays live during the delay
    s.Steps(2);
    REQUIRE(st.IsCinematic());
    CHECK(st.Playback().beat.id == "cs_rf_brand");
    CHECK(s.sim->WorldFrozen());
    st.Skip();
    s.sim->AdvanceRealTime(899);  // letterbox out 450 + camera return 450
    CHECK(st.IsCinematic());
    s.sim->AdvanceRealTime(2);
    CHECK_FALSE(st.IsCinematic());
    CHECK(st.Progress().Has("cs_rf_brand"));
    s.Steps(1);  // the freeze mask follows at the next step
    CHECK_FALSE(s.sim->WorldFrozen());
  }

  TEST_CASE("16.7 boss intro: Valdras within 14 tiles -> bar + rename once; within 9 -> boss_valdras once, with his Roar") {
    QwSim s;
    std::vector<BossBarMsg> bars;
    s.ctx->bus.Subscribe<BossBarMsg>([&bars](const BossBarMsg& m) { bars.push_back(m); });
    EntityId chief = s.FindAlive("valdras");
    REQUIRE(chief != kNoEntity);
    const Vec2 cp = s.Mon().Find(chief)->pos;
    s.Place(Vec2(cp.x, cp.y - 12));
    std::vector<Event> ev = s.StepsCollect(20);
    CHECK_FALSE(s.Story().IsBusy());
    REQUIRE_FALSE(Of<EvBossBar>(ev).empty());
    CHECK(Of<EvBossBar>(ev).front()->show);
    CHECK(Of<EvBossBar>(ev).front()->nameKey == "story.boss.valdras.name");
    CHECK(Of<EvMonsterRenamed>(ev).size() == 1);
    CHECK(s.sim->View().bossBar.show);
    CHECK(s.sim->View().bossBar.maxHp > 0);
    const Vec2 cp2 = s.Mon().Find(chief)->pos;
    s.Place(Vec2(cp2.x, cp2.y - 8.5));
    ev = s.StepsCollect(20);
    REQUIRE(s.Story().IsCinematic());
    CHECK(s.Story().Playback().beat.id == "boss_valdras");
    CHECK(s.Story().Playback().beat.kind == StoryBeatKind::BossIntro);
    CHECK(Of<EvMonsterRenamed>(ev).empty());  // once per instance
    // the step player resolves the focus on the live boss and the speaker to the intro name
    const StoryActorView a = s.Story().ResolveActor(StoryActor{StoryActorKind::Monster, "valdras", TilePos()});
    CHECK(a.resolved);
    CHECK(a.entity == chief);
    CHECK(a.nameKey == "story.boss.valdras.name");
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
    CHECK(st[2]->speakerNameKey == "story.boss.valdras.name");
    // K1 focus `anim`: his Roar plays on the focused instance right after the focus step, under the title.
    const auto roars = Of<EvPlayAnim>(steps);
    REQUIRE(roars.size() == 1);
    CHECK(roars[0]->entity == chief);
    CHECK(roars[0]->action == AnimAction::Roar);
    s.Story().Advance();               // say out 160, the Marshal's second line in 220
    s.sim->AdvanceRealTime(160 + 220);
    CHECK(s.Story().Playback().waitingForInput);
    CHECK(s.Story().Playback().stepIndex == 3);
    s.Story().Advance();               // say out 160, then the whisper reveal
    s.sim->AdvanceRealTime(160);
    CHECK(s.Story().Playback().stepIndex == 4);
    s.Story().Advance();               // input during the reveal ends the step (out 600); the hero's line in 220
    s.sim->AdvanceRealTime(600 + 220);
    CHECK(s.Story().Playback().waitingForInput);
    CHECK(s.Story().Playback().stepIndex == 5);
    s.Story().Advance();               // say out 160, the letterbox out 450, then the 450 ms camera return
    s.sim->AdvanceRealTime(160 + 450 + 449);
    CHECK(s.Story().IsCinematic());  // camera return still running
    s.sim->AdvanceRealTime(2);
    CHECK_FALSE(s.Story().IsCinematic());
    CHECK(s.Story().Progress().Has("boss_valdras"));
    CHECK_FALSE(s.Story().Progress().Has("cs_boss_valdras"));  // the intro's cutscene id is not marked
    // no second intro; killing the boss clears the bar (killed) and unlocks its achievement
    s.Steps(20);
    CHECK_FALSE(s.Story().IsBusy());
    bars.clear();
    REQUIRE(s.Kill(chief));
    REQUIRE_FALSE(bars.empty());
    CHECK_FALSE(bars.back().show);
    CHECK(bars.back().killed);
    CHECK(s.ctx->sys.achievements->State().IsUnlocked("ach_kill_valdras"));
  }

  TEST_CASE("the director emits STORY_STATE busy at enqueue time (T17 waits for it) and idle when drained") {
    QwSim s;
    std::vector<StoryStateMsg> states;
    s.ctx->bus.Subscribe<StoryStateMsg>([&states](const StoryStateMsg& m) { states.push_back(m); });
    s.Qs().Load({Rec("q_rf_burial_detail", QuestStatus::Completed, {10})});
    REQUIRE(s.Qw().TurnIn("q_rf_burial_detail", 0));
    REQUIRE(states.size() == 1);
    CHECK(states[0].active);
    CHECK(states[0].beatId == "cs_rf_brand");
    s.Story().FinishAllBeats();
    REQUIRE(states.size() == 2);
    CHECK_FALSE(states[1].active);
  }
}  // TEST_SUITE
TEST_SUITE("quests") {
  using namespace abyss::test::quests_fx;

  TEST_CASE("interact with the elder: log line 0, talk hook, quest card of 2 offers; the card freezes the sim (S2)") {
    QwSim s;
    s.sim->ClearEvents();
    s.Qw().InteractNpc("quest_elder");
    const std::vector<Event> ev(s.ctx->events.Items().begin(), s.ctx->events.Items().end());
    CHECK(HasLog(ev, "data.npc.quest_elder.dialogue.0"));
    CHECK(Of<EvQuestCardOpened>(ev).size() == 1);
    REQUIRE(s.Qw().Card().open);
    CHECK(s.Qw().Card().entries.size() == 2);  // the Captain at L1: M1 and S1
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
    CHECK(s.Qw().AcceptFromCard("q_rf_burial_detail"));
    CHECK(s.Qs().Tracked() == "q_rf_burial_detail");
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
    REQUIRE(s.Qs().Accept("q_rf_burial_detail"));
    for (int i = 0; i < 3; ++i) REQUIRE(s.Kill(s.FindAlive("risen_footman")));
    CHECK(s.Cur("q_rf_burial_detail", 0) == 3);
    const AchievementState& a = s.ctx->sys.achievements->State();
    CHECK(a.IsUnlocked("ach_first_kill"));
    CHECK(a.ProgressOf("kill") == 3);
    CHECK(a.ProgressOf("kill:risen_footman") == 0);  // a per-target counter only for a targeted achievement
    REQUIRE(s.Kill(s.FindAlive("breachclot")));
    CHECK(a.ProgressOf("kill") == 4);
    CHECK(a.ProgressOf("kill:breachclot") == 1);
    CHECK(a.ProgressOf("explore") == 1);  // the first story zone (Q2: distinct zones)
  }

  TEST_CASE("ichor drops: RngStream::Quests at 0.5 per listed kill; the pickup visual starts at the kill point") {
    QwSim s;
    REQUIRE(s.Qs().Accept("q_rf_black_blood"));
    Rng& q = s.ctx->Rand(RngStream::Quests);
    q.Script({0.4, 0.6});
    const EntityId a = s.FindAlive("breachclot");  // S1's ichor drops from breachclots (chance 0.5)
    REQUIRE(a != kNoEntity);
    const Vec2 at = s.Mon().Find(a)->pos;
    s.sim->ClearEvents();
    REQUIRE(s.Kill(a));
    CHECK(s.Cur("q_rf_black_blood", 0) == 1);
    bool pickup = false;
    for (const EvQuestUpdate* u : Of<EvQuestUpdate>(s.ctx->events.Items())) {
      if (u->kind == EvQuestUpdate::Kind::Progress && u->questId == "q_rf_black_blood") {
        pickup = u->hasFrom && u->from == at && u->itemKind == "ichor";
      }
    }
    CHECK(pickup);
    REQUIRE(s.Kill(s.FindAlive("breachclot")));
    CHECK(s.Cur("q_rf_black_blood", 0) == 1);  // 0.6 >= 0.5
    CHECK(q.ScriptedRemaining() == 0);
    q.Script({0.0});
    REQUIRE(s.Kill(s.FindAlive("risen_footman")));  // footmen are not listed: no roll
    CHECK(q.ScriptedRemaining() == 1);
    q.ClearScript();
  }

  TEST_CASE("16.1 / 3.4 barrowmoss nodes: deterministic spots on the live grid, gathered within 1.3 tiles, per-visit") {
    QwSim s;
    REQUIRE(s.Qs().Accept("q_rf_barrowmoss"));
    const ZoneRuntime& zone = *s.ctx->sys.zone;
    const QuestDef* q = s.ctx->data.FindQuest("q_rf_barrowmoss");
    const std::vector<TilePos> spots = ResolveGatherSpots(
        q->objectives[0].gatherArea, q->objectives[0].gatherCount,
        [&zone](int32_t c, int32_t r) { return zone.Walkable(c, r); }, "q_rf_barrowmoss:0");
    REQUIRE(spots.size() == 7);
    REQUIRE(CountNodes(s.Qw(), false) == 7);
    for (size_t i = 0; i < spots.size(); ++i) {
      CHECK(s.Qw().Nodes()[i].pos == spots[i].Center());
      CHECK(s.Qw().Nodes()[i].itemKind == "moss");
    }
    s.Place(spots[0].Center());
    const std::vector<Event> ev = s.StepsCollect(1);
    CHECK(s.Cur("q_rf_barrowmoss", 0) == 1);
    CHECK(CountNodes(s.Qw(), false) == 6);
    bool fly = false;
    for (const EvQuestUpdate* u : Of<EvQuestUpdate>(ev)) fly = fly || (u->hasFrom && u->itemKind == "moss");
    CHECK(fly);
    // a new visit restores every spot (gathered sets are per visit); progress is kept
    s.Qw().OnZoneExit();
    s.Qw().OnZoneEnter();
    CHECK(CountNodes(s.Qw(), false) == 7);
    CHECK(s.Cur("q_rf_barrowmoss", 0) == 1);
    for (size_t i = 1; i < 5; ++i) {
      s.Place(spots[i].Center());
      s.Steps(1);
    }
    CHECK(s.Status("q_rf_barrowmoss") == QuestStatus::Completed);
    CHECK(CountNodes(s.Qw(), false) == 0);  // Q5: synced on progress, the leftover nodes go
    // the guide now leads back to the giver, Brother Aldwin
    s.Steps(16);
    CHECK(s.Qw().Guide().reason == GuideReason::TurnIn);
    CHECK(s.Qw().Guide().pos == s.NpcPos("knell_chaplain"));
  }

  TEST_CASE("3.5 the Tag-Thief's clues: marks on the clue tiles, examined in any order within 2 tiles, note logged") {
    QwSim s;
    s.Qs().Load({Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {10})});
    REQUIRE(s.Qs().Accept("q_rf_tag_thief"));
    REQUIRE(CountNodes(s.Qw(), true) == 3);
    const QuestNode* cloth = NodeOf(s.Qw(), true, "q_rf_tag_thief", 1);
    REQUIRE(cloth != nullptr);
    const ZoneRuntime& zone = *s.ctx->sys.zone;
    TilePos expect;
    REQUIRE(NearestWalkableTile(TilePos(42, 34), [&zone](int32_t c, int32_t r) { return zone.Walkable(c, r); }, 8,
                                expect));
    CHECK(cloth->pos == expect.Center());
    Vec2 tile;
    CHECK(s.Qw().ClueTile("q_rf_tag_thief", 1, tile));
    s.Place(Vec2(cloth->pos.x + 1.5, cloth->pos.y));
    const std::vector<Event> ev = s.StepsCollect(1);
    CHECK(s.Cur("q_rf_tag_thief", 1) == 1);
    CHECK(s.Cur("q_rf_tag_thief", 0) == 0);
    CHECK(CountNodes(s.Qw(), true) == 2);
    CHECK(HasLog(ev, "zone.quest.clueFound"));
    CHECK(HasLog(ev, "data.questClue.clue_rf_tag_cloth"));
    CHECK_FALSE(s.Qw().ClueTile("q_rf_tag_thief", 1, tile));
  }

  // M3 (chapter1.md 5): explore Ashwheat (40,52) r8, then kill the Corpseherd at the Sheepfold (the zone's mini-boss).
  TEST_CASE("3.6 explore: within the radius at a 500 ms observer check; M3 then needs the Corpseherd") {
    QwSim s;
    s.Qs().Load({Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {10}),
                 Rec("q_rf_gnawed_field", QuestStatus::TurnedIn, {12, 1})});
    REQUIRE(s.Qs().Accept("q_rf_ashwheat"));
    s.Place(Vec2(48.5, 52));  // 8.5 > radius 8
    s.Steps(40);
    CHECK(s.Cur("q_rf_ashwheat", 0) == 0);
    s.Place(Vec2(47.9, 52));
    const std::vector<Event> ev = s.StepsCollect(31);
    CHECK(s.Cur("q_rf_ashwheat", 0) == 1);
    CHECK(s.Status("q_rf_ashwheat") == QuestStatus::Active);  // the second objective: the Corpseherd
    CHECK(HasLog(ev, "zone.quest.exploreFound"));
    const EntityId herd = s.FindAlive("miniboss_corpseherd");
    REQUIRE(herd != kNoEntity);
    REQUIRE(s.Kill(herd));
    CHECK(s.Cur("q_rf_ashwheat", 1) == 1);
    CHECK(s.Status("q_rf_ashwheat") == QuestStatus::Completed);
  }

  TEST_CASE("3.7 delivery: talking to Marl completes the breach-blood quest, and her shop opens too") {
    QwSim s;
    s.Qs().Load({Rec("q_rf_black_blood", QuestStatus::Active, {6, 0})});
    s.Qw().InteractNpc("plains_herbalist");
    CHECK(s.Status("q_rf_black_blood") == QuestStatus::Completed);
    CHECK(s.ctx->sys.shop->State().open);
    CHECK(s.ctx->sys.shop->State().npcId == "plains_herbalist");
  }

  TEST_CASE("4.1 turn-in: the card's cached gear is what is granted; exp, gold, achievements, autosave, toast") {
    QwSim s;
    s.Qs().Load({Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {10}),
                 Rec("q_rf_gnawed_field", QuestStatus::TurnedIn, {12, 1}),
                 Rec("q_rf_ashwheat", QuestStatus::TurnedIn, {1, 1}),
                 Rec("q_rf_marshal", QuestStatus::Completed, {1})});
    REQUIRE(s.Qw().OpenCard("quest_elder"));
    REQUIRE_FALSE(s.Qw().Card().entries.empty());
    const QuestCardEntry& e = s.Qw().Card().entries[0];
    CHECK(e.turnIn);
    CHECK(e.quest->id == "q_rf_marshal");
    REQUIRE(e.choices.size() == 2);
    const std::string pick = e.choices[1].uid;
    // reopening never rerolls
    s.Qw().CloseCard();
    REQUIRE(s.Qw().OpenCard("quest_elder"));
    CHECK(s.Qw().Card().entries[0].choices[1].uid == pick);
    const int64_t gold0 = s.H().Gold();
    s.sim->ClearEvents();
    REQUIRE(s.Qw().TurnIn("q_rf_marshal", 1));
    CHECK(s.H().Gold() == gold0 + 80);
    bool granted = false;
    for (const ItemInstance& it : s.ctx->sys.inventory->Items().Bag()) granted = granted || it.uid == pick;
    CHECK(granted);
    CHECK(s.Qw().RewardChoices("q_rf_marshal").empty());
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
    CHECK_FALSE(s.Qw().TurnIn("q_rf_marshal", 0));  // only from completed
  }

  TEST_CASE("turn-in overflow goes to the stash (I10); fixed items stack; the choice index clamps") {
    QwSim s;
    s.Qs().Load({Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {10}),
                 Rec("q_rf_black_blood", QuestStatus::Completed, {6, 1})});
    const size_t bag0 = s.ctx->sys.inventory->Items().Bag().size();
    REQUIRE(s.Qw().TurnIn("q_rf_black_blood", 7));  // no choices: the index is irrelevant
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
    s.Qs().Load({Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {10}),
                 Rec("q_rf_tag_thief", QuestStatus::Completed, {1, 1, 1, 1, 1})});
    REQUIRE(s.Qw().TurnIn("q_rf_tag_thief", 99));  // clamps to the last choice (S3: jewelry / weapon)
    CHECK(s.ctx->sys.inventory->Items().Stash().size() == stash0 + 1);
  }

  TEST_CASE("3.9 escort: spawn, wait, join, chip damage death -> failed and offered again; re-accept respawns") {
    QwSim s;
    s.Qs().Load({Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {10}),
                 Rec("q_rf_gnawed_field", QuestStatus::TurnedIn, {12, 1})});
    REQUIRE(s.Qs().Accept("q_rf_hallowed_ground"));
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
    MonsterDef brute = *s.ctx->data.Monsters().Find("gravegnawer");
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
    CHECK(s.Status("q_rf_hallowed_ground") == QuestStatus::Failed);
    CHECK(HasLog(ev, "zone.escort.npcDied"));
    bool died = false;
    for (const EvEntityDespawned* d : Of<EvEntityDespawned>(ev)) {
      died = died || (d->kind == EntityKind::Escort && d->reason == DespawnReason::Died);
    }
    CHECK(died);
    const std::vector<std::string> offers =
        OfferIds(s.Qs().Offers("knell_chaplain", s.H().Level(), OfferRule::QuestCard));
    CHECK(std::find(offers.begin(), offers.end(), "q_rf_hallowed_ground") != offers.end());
    REQUIRE(s.Qw().AcceptFromCard("q_rf_hallowed_ground"));
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
    s.Qs().Load({Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {10}),
                 Rec("q_rf_gnawed_field", QuestStatus::TurnedIn, {12, 1})});
    REQUIRE(s.Qs().Accept("q_rf_hallowed_ground"));
    s.Place(Vec2(31, 40));
    s.Steps(1);
    REQUIRE(s.Qw().Escort().joined);
    s.Place(Vec2(50, 90));  // > 14 tiles: the escort catches up within 2 rings, arriving with the hero
    const std::vector<Event> ev = s.StepsCollect(2);
    CHECK(s.Status("q_rf_hallowed_ground") == QuestStatus::Completed);
    CHECK(HasLog(ev, "zone.escort.complete"));
    CHECK_FALSE(s.Qw().Escort().active);
  }

  // DECISIONS Q9 (remake): the escort never walks onto a blocked tile and is never stranded where it has no path to the
  // hero. Seed 7013 (mage, the HEAD art manifest) froze Old Bern at (43.53,60.53): between tiles at (44.38,59.78), its
  // repath from (44,60) to the hero's (44,64) went round the generator's scatter wall (44,61) by (43,60), (43,61); the
  // web's path.slice(1, ...) dropped the first step, so it walked straight at (43,61), cut the wall's corner, and the
  // next repath started on the wall: findPath refuses an unwalkable start, the path stayed empty and nobody moved.
  TEST_CASE("3.9 escort (Q9): between two tiles it walks the path's first step, never across a blocked corner") {
    QwSim s;
    s.Qs().Load({Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {10}),
                 Rec("q_rf_gnawed_field", QuestStatus::TurnedIn, {12, 1})});
    REQUIRE(s.Qs().Accept("q_rf_hallowed_ground"));
    s.Place(Vec2(31, 40));
    s.Steps(1);
    REQUIRE(s.Qw().Escort().joined);
    const ZoneGrid& g = s.ctx->sys.zone->Grid();
    // the 7013 geometry: the scatter wall and the detour the path takes round it (golden map, W10)
    REQUIRE_FALSE(g.Walkable(44, 61));
    for (const TilePos t : {TilePos(44, 60), TilePos(43, 60), TilePos(43, 61), TilePos(43, 62), TilePos(44, 63)}) {
      REQUIRE(g.Walkable(t.col, t.row));
    }
    std::vector<EntityId> near;
    for (const MonsterInstance& m : s.Mon().All()) {
      if (m.IsAlive() && Dist(m.pos, Vec2(44, 62)) < 20) near.push_back(m.id);
    }
    for (EntityId id : near) s.Kill(id);
    const Vec2 hero(44.171, 64.009);
    s.Place(hero);
    EscortState& e = const_cast<EscortState&>(s.Qw().Escort());  // test-only: the escort's state one frame before
    e.pos = Vec2(44.3791, 59.7773);                                // the 7013 repath
    e.path.clear();
    e.repathAtMs = 0;
    int onBlocked = 0;
    for (int i = 0; i < 4 * 60; ++i) {
      s.Steps(1);
      if (!g.Walkable(JsRoundInt(e.pos.x), JsRoundInt(e.pos.y))) ++onBlocked;
    }
    CHECK(onBlocked == 0);
    CHECK(e.strandedSinceMs < 0);
    CHECK(Dist(e.pos, hero) <= 2.5);  // followed to a tile short of the hero
  }

  TEST_CASE("3.9 escort (Q9): an escort with no path to the hero catches up next to it after 2 s") {
    QwSim s;
    s.Qs().Load({Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {10}),
                 Rec("q_rf_gnawed_field", QuestStatus::TurnedIn, {12, 1})});
    REQUIRE(s.Qs().Accept("q_rf_hallowed_ground"));
    s.Place(Vec2(31, 40));
    s.Steps(1);
    REQUIRE(s.Qw().Escort().joined);
    const ZoneGrid& g = s.ctx->sys.zone->Grid();
    REQUIRE_FALSE(g.Walkable(44, 61));
    std::vector<EntityId> near;
    for (const MonsterInstance& m : s.Mon().All()) {
      if (m.IsAlive() && Dist(m.pos, Vec2(44, 62)) < 20) near.push_back(m.id);
    }
    for (EntityId id : near) s.Kill(id);
    // The 7013 freeze: the escort's rounded tile is the wall, the hero 4.97 tiles away (follow range, no catch-up).
    const Vec2 hero(45.9, 64.9);
    s.Place(hero);
    EscortState& e = const_cast<EscortState&>(s.Qw().Escort());
    e.pos = Vec2(43.53, 60.53);
    e.path.clear();
    e.repathAtMs = 0;
    std::vector<Event> ev = s.StepsCollect(60);  // 1 s: still waiting (a blocked tile for a moment is not stranded)
    CHECK(e.pos == Vec2(43.53, 60.53));
    CHECK(e.strandedSinceMs >= 0);
    ev = s.StepsCollect(75);  // past 2 s: catch-up next to the hero, in its component
    bool caughtUp = false;
    for (const EvEntityTeleported* t : Of<EvEntityTeleported>(ev)) {
      caughtUp = caughtUp || (t->id == e.entity && t->reason == TeleportReason::CatchUp);
    }
    CHECK(caughtUp);
    const TilePos at(JsRoundInt(e.pos.x), JsRoundInt(e.pos.y));
    CHECK(g.Walkable(at.col, at.row));
    CHECK(g.Component(at.col, at.row) == g.Component(JsRoundInt(hero.x), JsRoundInt(hero.y)));
    CHECK(Dist(e.pos, hero) <= 3.0);
    CHECK(e.strandedSinceMs < 0);
    // and it follows from there: the hero walks off 6 tiles, the escort keeps a path and comes along
    const Vec2 hero2(hero.x, hero.y + 6);
    REQUIRE(g.Walkable(JsRoundInt(hero2.x), JsRoundInt(hero2.y)));
    s.Place(hero2);
    s.Steps(5 * 60);
    CHECK(e.strandedSinceMs < 0);
    CHECK(Dist(e.pos, hero2) <= 2.5);
  }

  TEST_CASE("3.9 escort (Q9): the catch-up tile is in the hero's component, never a sealed pocket of the ring") {
    QwSim s;
    s.Qs().Load({Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {10}),
                 Rec("q_rf_gnawed_field", QuestStatus::TurnedIn, {12, 1})});
    REQUIRE(s.Qs().Accept("q_rf_hallowed_ground"));
    s.Place(Vec2(31, 40));
    s.Steps(1);
    REQUIRE(s.Qw().Escort().joined);
    // An open 7x7 field more than 14 tiles from the escort (catch-up range), no monster near it.
    ZoneGrid& g = const_cast<ZoneGrid&>(s.ctx->sys.zone->Grid());  // test-only: carve a pocket into the collision grid
    TilePos h(-1, -1);
    for (int32_t r = 50; r < 110 && h.col < 0; ++r) {
      for (int32_t c = 20; c < 100 && h.col < 0; ++c) {
        if (Dist(Vec2(c, r), Vec2(30, 40)) < 20) continue;
        bool open = true;
        for (int32_t dr = -3; dr <= 3 && open; ++dr) {
          for (int32_t dc = -3; dc <= 3 && open; ++dc) open = g.Walkable(c + dc, r + dr);
        }
        bool quiet = true;
        for (const MonsterInstance& m : s.Mon().All()) quiet = quiet && !(m.IsAlive() && Dist(m.pos, Vec2(c, r)) < 12);
        if (open && quiet) h = TilePos(c, r);
      }
    }
    REQUIRE(h.col >= 0);
    // Seal the ring's first tile in findWalkableNear order, (h - 1, h - 1): block its other neighbours and the two
    // tiles a diagonal step to the hero needs. It stays walkable, but in a component of its own.
    const TilePos pocket(h.col - 1, h.row - 1);
    for (const TilePos t : {TilePos(h.col - 2, h.row - 2), TilePos(h.col - 1, h.row - 2), TilePos(h.col, h.row - 2),
                            TilePos(h.col - 2, h.row - 1), TilePos(h.col, h.row - 1), TilePos(h.col - 2, h.row),
                            TilePos(h.col - 1, h.row)}) {
      g.SetWalkable(t.col, t.row, false);
    }
    REQUIRE(g.Walkable(pocket.col, pocket.row));
    REQUIRE(g.Component(pocket.col, pocket.row) != g.Component(h.col, h.row));
    TilePos first;
    REQUIRE(s.ctx->sys.zone->Paths().FindWalkableNear(h, 2, first));
    REQUIRE(first == pocket);  // where the web's catch-up put it
    s.Place(h.Center());
    std::vector<Event> ev = s.StepsCollect(1);  // > 14 tiles: catch-up
    const EscortState& e = s.Qw().Escort();
    const TilePos at(JsRoundInt(e.pos.x), JsRoundInt(e.pos.y));
    CHECK_FALSE(at == pocket);
    CHECK(g.Component(at.col, at.row) == g.Component(h.col, h.row));
    CHECK(Dist(e.pos, h.Center()) <= 3.0);
    // it has a path from there: walk 5 tiles off, it follows without being stranded
    s.Place(Vec2(h.col + 3, h.row + 3));
    s.Steps(4 * 60);
    CHECK(e.strandedSinceMs < 0);
    CHECK(Dist(e.pos, Vec2(h.col + 3, h.row + 3)) <= 2.5);
  }

  // Q9, the edges of the catch-up rule: (1) the hero's tile is walkable but no ring tile shares its component (a pocket
  // of its own): there is no catch-up tile, so the escort stays where it is - it used to fall back to findWalkableNear,
  // land in another component and be re-teleported to that same tile every 2 s; (2) the hero's tile is not walkable:
  // any walkable ring tile, once - the stranded re-check on the same tile emits no teleport whose from == to.
  TEST_CASE("3.9 escort (Q9): no catch-up into another component, and no teleport onto the tile it stands on") {
    QwSim s;
    s.Qs().Load({Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {10}),
                 Rec("q_rf_gnawed_field", QuestStatus::TurnedIn, {12, 1})});
    REQUIRE(s.Qs().Accept("q_rf_hallowed_ground"));
    s.Place(Vec2(31, 40));
    s.Steps(1);
    REQUIRE(s.Qw().Escort().joined);
    ZoneGrid& g = const_cast<ZoneGrid&>(s.ctx->sys.zone->Grid());  // test-only: carve pockets into the collision grid
    // Two open 7x7 fields more than 14 tiles from the escort (catch-up range) and from each other, no monster near.
    std::vector<TilePos> fields;
    for (int32_t r = 50; r < 110 && fields.size() < 2; ++r) {
      for (int32_t c = 20; c < 100 && fields.size() < 2; ++c) {
        if (Dist(Vec2(c, r), Vec2(30, 40)) < 20) continue;
        if (!fields.empty() && Dist(Vec2(c, r), fields[0].Center()) < 20) continue;
        bool open = true;
        for (int32_t dr = -3; dr <= 3 && open; ++dr) {
          for (int32_t dc = -3; dc <= 3 && open; ++dc) open = g.Walkable(c + dc, r + dr);
        }
        bool quiet = true;
        for (const MonsterInstance& m : s.Mon().All()) quiet = quiet && !(m.IsAlive() && Dist(m.pos, Vec2(c, r)) < 12);
        if (open && quiet) fields.push_back(TilePos(c, r));
      }
    }
    REQUIRE(fields.size() == 2);
    const EscortState& e = s.Qw().Escort();
    const auto teleports = [&e](const std::vector<Event>& ev) {
      int n = 0;
      for (const EvEntityTeleported* t : Of<EvEntityTeleported>(ev)) {
        if (t->id != e.entity) continue;
        ++n;
        CHECK_FALSE(t->from == t->to);
      }
      return n;
    };
    const auto blockRing1 = [&g](TilePos c) {
      for (int32_t dr = -1; dr <= 1; ++dr) {
        for (int32_t dc = -1; dc <= 1; ++dc) {
          if (dr != 0 || dc != 0) g.SetWalkable(c.col + dc, c.row + dr, false);
        }
      }
    };
    // (1) The hero alone in a one-tile pocket: walkable, ring 1 blocked, ring 2 walkable in another component.
    const TilePos a = fields[0];
    blockRing1(a);
    REQUIRE(g.Walkable(a.col, a.row));
    TilePos old;
    REQUIRE(s.ctx->sys.zone->Paths().FindWalkableNear(a, 2, old));
    REQUIRE(g.Component(old.col, old.row) != g.Component(a.col, a.row));  // where the old fallback put the escort
    const Vec2 before = e.pos;
    s.Place(a.Center());
    CHECK(teleports(s.StepsCollect(5 * 60)) == 0);
    CHECK(e.pos == before);
    // (2) The hero on a blocked tile with ring 1 blocked too: the first walkable ring-2 tile, 2.8 tiles off (follow
    // range); no path to an unwalkable hero, so the stranded re-check comes every 2 s - on the tile it already holds.
    const TilePos b = fields[1];
    blockRing1(b);
    g.SetWalkable(b.col, b.row, false);
    TilePos spot;
    REQUIRE(s.ctx->sys.zone->Paths().FindWalkableNear(b, 2, spot));
    REQUIRE(Dist(spot.Center(), b.Center()) > 2.0);
    s.Place(b.Center());
    std::vector<Event> ev = s.StepsCollect(1);
    CHECK(teleports(ev) == 1);
    CHECK(e.pos == spot.Center());
    s.Steps(1);  // StepsCollect starts with the events of the step before it
    ev = s.StepsCollect(6 * 60);
    CHECK(teleports(ev) == 0);
    CHECK(e.pos == spot.Center());
  }

  TEST_CASE("guide: M1 points at the nearest living footman; markers and the guide follow the quest state") {
    QwSim s;
    REQUIRE(s.Qs().Accept("q_rf_burial_detail"));
    s.Steps(1);
    const GuideTarget g = s.Qw().Guide();
    REQUIRE(g.Valid());
    CHECK(g.kind == GuideTargetKind::Monster);
    const EntityId near = s.FindAlive("risen_footman", s.H().Position());
    REQUIRE(near != kNoEntity);
    CHECK(g.pos == s.Mon().Find(near)->pos);
    CHECK(g.questId == "q_rf_burial_detail");
  }

  TEST_CASE("Q1: a dialogue choice reward is paid once per (npc, node, choice), and saved") {
    QwSim s;
    const int64_t gold0 = s.H().Gold();
    for (int round = 0; round < 2; ++round) {
      REQUIRE(s.Qw().OpenDialogueTree("quest_elder"));
      DialogueSystem& d = *s.ctx->sys.dialogue;
      d.Choose(3);  // reward_ask ("Is there pay?")
      REQUIRE(d.View().nodeId == "reward_ask");
      d.Choose(0);  // "Take it." -> help, +30 gold +50 exp (a dead man's pay)
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
    CHECK(lore.IsCollected("lore_rf_01"));
    CHECK(lore.Text().open);
    CHECK(lore.Text().loreId == "lore_rf_01");
    CHECK(Of<EvLoreCollected>(ev).size() == 1);
    CHECK(HasLog(ev, "zone.lore.discovered"));
    lore.CloseText();
    s.Steps(2);
    CHECK_FALSE(lore.Text().open);  // collected entries never respawn
    // hidden area (108,108) r6: only the centre side explored -> not yet
    s.Place(Vec2(100, 108));
    s.Steps(2);
    CHECK_FALSE(lore.IsDiscovered("hidden_rf_paymaster"));
    s.Place(Vec2(108, 108));
    ev = s.StepsCollect(2);
    REQUIRE(lore.IsDiscovered("hidden_rf_paymaster"));
    CHECK(Of<EvHiddenAreaDiscovered>(ev).size() == 1);
    REQUIRE(lore.HiddenRewards().size() == 2);
    const int64_t gold0 = s.H().Gold();
    REQUIRE(lore.ClaimHiddenReward("hidden_rf_paymaster", 1));  // gold pile 200
    CHECK(s.H().Gold() == gold0 + 200);
    // Q5: the unclaimed chest lies there again on the next visit; the claimed pile does not
    SaveData save;
    s.sim->BuildSave(save, 0);
    CHECK(save.loreCollected == std::vector<std::string>{"lore_rf_01"});
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
    CHECK(f.back()->decorId == "story_rf_belfry");
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
    CHECK(EscortNpcNameKey("q_rf_hallowed_ground") == "data.escortNpc.rf_hallowed_ground");
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
    CHECK(keyed == 5);  // + the Boneyard Gate of the M5 slot (1B part 1: defend at (10,32), DR4)
  }

  TEST_CASE("Q11: the escort logs and its label use the key, so English shows 'Old Bern'") {
    QwSim s;
    s.Qs().Load({Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {10}),
                 Rec("q_rf_gnawed_field", QuestStatus::TurnedIn, {12, 1})});
    s.sim->ClearEvents();
    REQUIRE(s.Qs().Accept("q_rf_hallowed_ground"));
    const std::vector<Event> ev(s.ctx->events.Items().begin(), s.ctx->events.Items().end());
    const EvLog* appeared = FindLog(ev, "zone.escort.npcAppeared");
    REQUIRE(appeared != nullptr);
    REQUIRE(appeared->text.args.size() == 1);
    CHECK(appeared->text.args[0].isKey);
    CHECK(appeared->text.args[0].value == "data.escortNpc.rf_hallowed_ground");
    const I18n en = EnglishStrings();
    CHECK(en.T(appeared->text) == "Old Bern appeared! Escort target marked.");
    // the label (snapshot EscortState): NameOr(nameKey, name)
    const EscortState& e = s.Qw().Escort();
    CHECK(e.nameKey == "data.escortNpc.rf_hallowed_ground");
    CHECK(e.name == s.ctx->data.FindQuest("q_rf_hallowed_ground")->escortNpc.name);
    CHECK(en.NameOr(e.nameKey, e.name) == "Old Bern");
    s.Place(Vec2(32, 40));
    const std::vector<Event> joined = s.StepsCollect(1);
    const EvLog* j = FindLog(joined, "zone.escort.joined");
    REQUIRE(j != nullptr);
    CHECK(en.T(j->text) == "Old Bern is following you.");
  }

  TEST_CASE("3.5: an examined clue floats '<targetName>\\n<note>'; progress carries targetId and amount (13)") {
    QwSim s;
    s.Qs().Load({Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {10})});
    REQUIRE(s.Qs().Accept("q_rf_tag_thief"));
    const QuestNode* cloth = NodeOf(s.Qw(), true, "q_rf_tag_thief", 1);
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
          zh.T("data.questTarget.clue_rf_tag_cloth") + "\n" + zh.T("data.questClue.clue_rf_tag_cloth"));
    CHECK(EnglishStrings().T(note->text) ==
          "The Caltrops\nA scrap of crusade tabard on the caltrops.");
    bool progress = false;
    for (const EvQuestUpdate* u : Of<EvQuestUpdate>(ev)) {
      if (u->kind != EvQuestUpdate::Kind::Progress || u->questId != "q_rf_tag_thief") continue;
      progress = u->objectiveIndex == 1 && u->targetId == "clue_rf_tag_cloth" && u->amount == 1;
    }
    CHECK(progress);
  }

  TEST_CASE("13: kill progress events name the target and the requested amount") {
    QwSim s;
    REQUIRE(s.Qs().Accept("q_rf_burial_detail"));
    s.sim->ClearEvents();
    REQUIRE(s.Kill(s.FindAlive("risen_footman")));
    const std::vector<Event> ev(s.ctx->events.Items().begin(), s.ctx->events.Items().end());
    const EvQuestUpdate* kill = nullptr;
    for (const EvQuestUpdate* u : Of<EvQuestUpdate>(ev)) {
      if (u->kind == EvQuestUpdate::Kind::Progress && u->questId == "q_rf_burial_detail") kill = u;
    }
    REQUIRE(kill != nullptr);
    CHECK(kill->targetId == "risen_footman");
    CHECK(kill->amount == 1);
    CHECK(kill->current == 1);
    CHECK_FALSE(kill->hasFrom);
    // an amount above what is left is reported as requested (the counter clamps)
    s.sim->ClearEvents();
    s.Qs().UpdateProgress(ObjectiveType::Kill, "risen_footman", 50);
    const std::vector<Event> ev2(s.ctx->events.Items().begin(), s.ctx->events.Items().end());
    bool clamped = false;
    for (const EvQuestUpdate* u : Of<EvQuestUpdate>(ev2)) {
      if (u->kind == EvQuestUpdate::Kind::Progress && u->questId == "q_rf_burial_detail") {
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
    REQUIRE(lore.IsDiscovered("hidden_rf_paymaster"));
    s.sim->ClearEvents();
    REQUIRE(lore.ClaimHiddenReward("hidden_rf_paymaster", 1));  // gold pile 200
    std::vector<Event> ev(s.ctx->events.Items().begin(), s.ctx->events.Items().end());
    const EvLog* gold = FindLog(ev, "zone.hiddenArea.gotGold");
    REQUIRE(gold != nullptr);
    CHECK(gold->type == LogType::Loot);
    CHECK(EnglishStrings().T(gold->text) == "Gained 200 gold");
    s.sim->ClearEvents();
    const size_t bag0 = s.ctx->sys.inventory->Items().Bag().size();
    REQUIRE(lore.ClaimHiddenReward("hidden_rf_paymaster", 0));  // rare chest
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
    CHECK(EnglishStrings().T(card->slide.subtitle) == "The Red Field");
    CHECK(EnglishStrings().T(card->slide.title) == "Woken from Ash");
  }

  TEST_CASE("Q7: a turn-in cutscene lost to a quit replays on the next entry of the quest's zone, with its pet") {
    QwSim s;
    s.Qs().Load({Rec("q_rf_burial_detail", QuestStatus::Completed, {10})});
    REQUIRE(s.Qw().TurnIn("q_rf_burial_detail", 0));
    REQUIRE(s.Story().IsBusy());
    SaveData save;  // the turn-in autosave: turned in, cs_rf_brand not seen yet
    s.sim->BuildSave(save, 0);
    REQUIRE(save.hasStorySeen);
    REQUIRE(std::find(save.storySeen.begin(), save.storySeen.end(), "cs_rf_brand") == save.storySeen.end());
    std::string err;
    REQUIRE(s.sim->LoadGame(save, &err) == SaveError::None);
    s.ctx = &s.sim->Context();
    CHECK(s.Story().IsBusy());
    CHECK_FALSE(s.Story().IsCinematic());  // the normal 650 ms turn-in delay, world live
    s.Steps(40);
    REQUIRE(s.Story().IsCinematic());
    CHECK(s.Story().Playback().beat.id == "cs_rf_brand");
    s.Story().FinishAllBeats();
    CHECK(s.Story().Progress().Has("cs_rf_brand"));
    // seen now: the next load plays nothing
    SaveData again;
    s.sim->BuildSave(again, 0);
    REQUIRE(s.sim->LoadGame(again, &err) == SaveError::None);
    s.ctx = &s.sim->Context();
    CHECK_FALSE(s.Story().IsBusy());

    // Only the quest's own zone replays it; a save without storySeen never does.
    SaveData elsewhere = SaveIn(s, "twilight_forest", 20, 20, {Rec("q_rf_burial_detail", QuestStatus::TurnedIn, {10})},
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
    CHECK_FALSE(s.Story().Progress().Has("cs_rf_brand"));

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
    CHECK(bosses == std::vector<std::string>{"demon_lord", "dungeon_abyss_lord", "mountain_troll", "phoenix", "valdras",
                                             "werewolf_alpha"});
    // chapter1.md 8: cs_boss_valdras, six steps; his Roar plays on the focus step (K1 `anim`).
    const BossIntroDef* valdras = s.BossIntroFor("valdras");
    REQUIRE(valdras != nullptr);
    CHECK(valdras->cutscene == "cs_boss_valdras");
    CHECK(valdras->name == "story.boss.valdras.name");
    CHECK(valdras->epithet == "story.boss.valdras.epithet");
    const Cutscene* intro = s.FindCutscene("cs_boss_valdras");
    REQUIRE(intro != nullptr);
    REQUIRE(intro->steps.size() == 6);
    CHECK(intro->steps[0].kind == StoryStepKind::Focus);
    CHECK(intro->steps[0].hasAnim);
    CHECK(intro->steps[0].anim == AnimAction::Roar);
    CHECK(intro->steps[1].kind == StoryStepKind::Title);
    CHECK(intro->steps[5].speaker.kind == StoryActorKind::Hero);
    CHECK(intro->steps[5].text == "story.cs_boss_valdras.6");
    // Chapter 1 triggers (chapter1.md 8 "Triggers"; cs_rf_pit waits for Wave 3): the quests' beats, the siege on
    // accept, the Corpseherd's and the Marshal's deaths.
    std::vector<std::string> ch1;
    for (const StoryTriggerDef& t : s.triggers) {
      const QuestDef* q = data.FindQuest(t.subjectId);
      const std::vector<std::string>& roster = data.Monsters().ZoneList("emerald_plains")->monsterIds;
      const MiniBossEntry* mini = data.Monsters().MiniBossFor("emerald_plains");
      const bool ch1Monster = t.on == StoryTriggerOn::MonsterKilled &&
                              (std::find(roster.begin(), roster.end(), t.subjectId) != roster.end() ||
                               (mini != nullptr && mini->monsterId == t.subjectId));
      if ((q != nullptr && q->zone == "emerald_plains") || ch1Monster) {
        ch1.push_back(std::string(EnumName(t.on)) + ":" + t.subjectId + ">" + t.cutscene);
      }
    }
    std::string got;
    for (const std::string& t : ch1) got += t + " ";
    INFO("Chapter 1 triggers: " << got);
    CHECK(ch1 == std::vector<std::string>{"quest_turned_in:q_rf_burial_detail>cs_rf_brand",
                                          "quest_turned_in:q_rf_ashwheat>cs_rf_whisper",
                                          "quest_accepted:q_rf_last_toll>cs_rf_siege",
                                          "quest_turned_in:q_rf_last_toll>cs_rf_finale",
                                          "monster_killed:miniboss_corpseherd>cs_rf_herd",
                                          "monster_killed:valdras>cs_rf_marshal_falls"});
    // Step counts (5-14, quests_test) and the beats' shapes: the finale's title card, the Marshal's last words.
    const std::pair<const char*, size_t> steps[] = {{"cs_rf_brand", 11},        {"cs_rf_whisper", 11},
                                                    {"cs_rf_herd", 5},          {"cs_rf_marshal_falls", 5},
                                                    {"cs_rf_siege", 5},         {"cs_rf_finale", 13}};
    for (const auto& [id, n] : steps) {
      CAPTURE(id);
      const Cutscene* c = s.FindCutscene(id);
      REQUIRE(c != nullptr);
      CHECK(c->steps.size() == n);
    }
    const Cutscene* finale = s.FindCutscene("cs_rf_finale");
    CHECK(finale->steps[5].kind == StoryStepKind::Title);
    CHECK(finale->steps[5].title == "story.cs_rf_finale.6.title");
    CHECK(finale->steps[11].speaker.kind == StoryActorKind::Npc);
    CHECK(finale->steps[11].speaker.id == "plains_herbalist");  // Marl leaves for the tower (herb garden)
    const Cutscene* falls = s.FindCutscene("cs_rf_marshal_falls");
    CHECK(falls->steps[1].speaker.kind == StoryActorKind::Monster);
    CHECK(falls->steps[1].speaker.id == "valdras");
    CHECK(falls->steps[4].kind == StoryStepKind::Wait);
    const Cutscene* herd = s.FindCutscene("cs_rf_herd");
    CHECK(herd->steps[0].target.kind == StoryActorKind::Tile);  // the Corpseherd is no map spawn: her tile (60,55)
    CHECK(herd->steps[0].target.tile == TilePos(60, 55));
    // The prologue: title + text on each slide, no heading line (chapter1.md 8); the chapter card's mood is night.
    for (const StorySlide& sl : s.prologue.slides) {
      CHECK(sl.heading.empty());
      CHECK_FALSE(sl.title.empty());
      CHECK_FALSE(sl.text.empty());
    }
    CHECK(EnglishStrings().T(s.prologue.slides[5].title) == "Woken in Ash");
  }
}  // TEST_SUITE

// =====================================================================================================================
// World: the generated emerald_plains map, its routes, camp NPCs, event data, hidden area (from world_test.cpp)
// Re-pinned by the one golden re-baseline (DECISIONS DR4, chapter1.md 17; package 1B part 1): the finale's two explore
// landmarks (65,30) (50,55) became the Boneyard Gate defend target (10,32), the sealed Pit entrance (17,26) and the
// echo lore spots (29,15) (38,47) (20,92) were added. The RNG checkpoints did not move (landmarks only clear tiles);
// tile and decor counts, two A* routes and the walkable count did. The golden fixture is
// CoreTests/golden/maps/emerald_plains.json (tilesSha 54ce0471e96c5d0e, decorSha 7a501f14066cbb08, 13169 walkable).
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
    REQUIRE(map->generatorAvoid.size() == 30);  // 28 - 2 explore landmarks + the defend target + 3 echo lore spots
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
    CHECK(counts == std::array<int32_t, 7>{11006, 1971, 0, 363, 818, 192, 50});
    CHECK(WorldWalkableCount(g) == 13169);
    CHECK(g.Decorations().size() == 1277);
    auto countDecor = [&g](const char* type) {
      return std::count_if(g.Decorations().begin(), g.Decorations().end(), [type](const Decoration& d) { return d.type == type; });
    };
    CHECK(countDecor("grass") == 545);
    CHECK(countDecor("flower") == 259);
    CHECK(countDecor("bush") == 208);
    CHECK(countDecor("rock") == 103);
    CHECK(countDecor("tree") == 64);
    CHECK(countDecor("tree_round") == 52);
    CHECK(countDecor("mushroom_red") == 44);
    CHECK(countDecor("boulder") == 2);
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
    // The re-baseline's landmarks stand on open ground: the sealed Pit entrance, the Boneyard Gate and the three echo
    // lore spots (chapter1.md 12: "provisional until the re-baseline confirms them walkable").
    for (const TilePos t : {TilePos{17, 26}, TilePos{10, 32}, TilePos{29, 15}, TilePos{38, 47}, TilePos{20, 92}}) {
      CAPTURE(t.col);
      CAPTURE(t.row);
      CHECK(g.Tile(t.col, t.row) == TileType::Grass);
    }
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
        {{15, 22}, {108, 108}, 98, 81, 131.534, {{16, 23}, {17, 24}, {18, 25}}},
        {{15, 15}, {95, 100}, 86, 79, 118.706, {{16, 16}, {17, 17}, {18, 18}}},
        {{15, 22}, {60, 55}, 45, 33, 58.662, {{16, 22}, {17, 22}, {18, 23}}},
        {{15, 22}, {18, 25}, 3, 3, 4.242, {{16, 23}, {17, 24}, {18, 25}}},
        // M5: from the burn pit to the Boneyard Gate (the siege).
        {{15, 22}, {10, 32}, 10, 5, 12.070, {{14, 23}, {13, 24}, {12, 25}, {12, 26}}},
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
    // The sealed Pit entrance (17,26) keeps its surroundings open: (17,25), a wall on the web map, is grass now, so the
    // route to (18,25) runs the direct diagonal.
    CHECK(g.Walkable(17, 25));
  }

  TEST_CASE("A* routes re-baselined on the full W5 grid (camp blockers + tall decor) stay valid and optimal-ish") {
    const DataStore& data = test::RealData();
    const MapDef& map = *data.FindMap("emerald_plains");
    const ZoneGrid raw = BuildRawZoneGrid(data, map);
    const ZoneGrid g = BuildZoneGrid(data, map);
    Pathfinder praw(raw), pf(g);
    const std::pair<TilePos, TilePos> routes[] = {{{15, 22}, {15, 15}},   {{15, 22}, {118, 60}}, {{15, 22}, {108, 108}},
                                                  {{15, 15}, {95, 100}},  {{15, 22}, {60, 55}},  {{15, 22}, {18, 25}},
                                                  {{15, 22}, {10, 32}}};
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
    CHECK(at("knell_chaplain") == Vec2(12.25, 14.25));  // chapter1.md 3: Brother Aldwin, the 4th camp-1 NPC
    CHECK(at("stash") == Vec2(17.25, 16.0));            // I7: the stash keeper is appended, the 5th (slot 4)
    // Idle facing (W11): every camp NPC looks at the fire (no tents).
    auto facing = [&w](const char* id) { return w.zone.FindNpc(id) != nullptr ? w.zone.FindNpc(id)->facing : Vec2(); };
    CHECK(facing("blacksmith") == camp::CampNpcFacing(0));
    CHECK(facing("merchant") == camp::CampNpcFacing(1));
    CHECK(facing("quest_elder") == camp::CampNpcFacing(2));
    CHECK(facing("knell_chaplain") == camp::CampNpcFacing(3));
    CHECK(facing("stash") == camp::CampNpcFacing(4));
    CHECK(facing("blacksmith").Dot((Vec2(15, 15) - at("blacksmith")).Normalized()) > 1.0 - 1e-12);
    CHECK(facing("merchant").Dot(Vec2(-1.0, 1.0).Normalized()) > 1.0 - 1e-12);  // slot 1 (1.375, -1.375) -> the fire
    CHECK(facing("stash").Dot((Vec2(15, 15) - at("stash")).Normalized()) > 1.0 - 1e-12);
    CHECK(facing("plains_herbalist") == Vec2(0, 1));  // field NPCs keep the default
    CHECK(at("plains_herbalist") == Vec2(50, 30));
    CHECK(at("plains_wanderer") == Vec2(70, 65));
    size_t merchants = 0;
    for (const NpcPlacement& n : w.zone.Npcs()) merchants += n.npcId == "merchant" ? 1 : 0;
    CHECK(merchants == 1);                                   // Crowmarket's own trader replaced camp 2's shared merchant
    CHECK(at("crow_trader") == Vec2(96.875, 102.375));        // camp 2 (95,100), slot 0: Mag Crowe
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

  TEST_CASE("Red Field random events (chapter1.md 13): the zone's own lines, zealots guard the Bound Pilgrim") {
    WorldRig w;
    REQUIRE(w.ok);
    MonsterSystem monsters(w.h.ctx);
    w.h.ctx.sys.monsters = &monsters;
    const TilePos spot = WorldClearRow(w.zone, 4);
    REQUIRE(spot.col > 0);
    w.Place(spot.Center());
    const ZoneEventDataDef* zd = w.h.ctx.data.World().randomEvents.ForZone("emerald_plains");
    REQUIRE(zd != nullptr);
    CHECK(zd->rescueMonsters == std::vector<std::string>{"oath_zealot"});
    CHECK(zd->ambushMonsters == std::vector<std::string>{"risen_footman", "gravegnawer"});
    // The rescue: the pilgrim, held by Red-Oath zealots, announced in the Red Field's line.
    w.events.TriggerEvent(RandomEventType::Rescue, spot.Center());
    REQUIRE(w.events.Active().size() == 1);
    const ActiveRandomEvent& rescue = w.events.Active().front();
    REQUIRE(rescue.monsters.size() >= 2);
    for (EntityId id : rescue.monsters) {
      REQUIRE(monsters.Find(id) != nullptr);
      CHECK(monsters.Find(id)->def.id == "oath_zealot");
    }
    CHECK(w.Logged("sys.event.msg.rescue.emerald_plains"));
    CHECK_FALSE(w.Logged("sys.event.msg.rescue"));
    // The ambush: hands from the mud (footmen and gnawers), after the toll line.
    w.events.TriggerEvent(RandomEventType::Ambush, spot.Center() + Vec2(0, 3));
    CHECK(w.Logged("sys.event.msg.ambush.emerald_plains"));
    for (const ActiveRandomEvent& e : w.events.Active()) {
      if (e.type != RandomEventType::Ambush) continue;
      for (EntityId id : e.monsters) {
        REQUIRE(monsters.Find(id) != nullptr);
        const std::string& def = monsters.Find(id)->def.id;
        CHECK((def == "risen_footman" || def == "gravegnawer"));
      }
    }
    // The pilgrim's line and the Relic Peddler's (chapter1.md 3) are the zone's completion and announce lines. The
    // completion line does not splice the pilgrim's name in (the core still passes {npcName}; final review: "You cut The
    // Bound Pilgrim loose" / 被缚的朝圣者的绳子), and the Peddler lays out relics, not white bones (no exposed bones in
    // Chapter 1, story-bible.md 12).
    const I18n en = abyss::test::quests_fx::EnglishStrings();
    const std::vector<I18nArg> args = {KeyArg("npcName", "sys.event.rescue.emerald_plains"), {"gold", "30"}, {"exp", "25"}};
    CHECK(en.T("zone.event.rescue.complete.emerald_plains", args) ==
          "You cut the pilgrim loose. \"They said I walked too slowly to keep up with the Flame.\" Gained 30 gold and "
          "25 EXP");
    const I18n& zh = w.h.ctx.data.Strings();
    CHECK(zh.T("zone.event.rescue.complete.emerald_plains", args).find(zh.T("sys.event.rescue.emerald_plains")) ==
          std::string::npos);
    CHECK(en.T("zone.event.merchant.announce.emerald_plains").find("At least one of them is.") != std::string::npos);
    CHECK(en.T("sys.event.msg.wandering_merchant.emerald_plains") == "By the road someone has laid out a tray of relics on an old cloth.");
  }

  TEST_CASE("exploration 10.3: standing at (108,108) sees the whole Paymaster's Wagon; (104,102) misses two corners") {
    WorldRig w;
    REQUIRE(w.ok);
    REQUIRE(w.zone.Map().hiddenAreas.size() == 1);
    const HiddenAreaDef& cache = w.zone.Map().hiddenAreas[0];
    CHECK(cache.id == "hidden_rf_paymaster");
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
  TEST_CASE("camp blockers: the raw emerald_plains grid has 13169 walkable tiles; the two camps' blockers take 2 x 8") {
    const DataStore& data = test::RealData();
    const MapDef& map = *data.FindMap("emerald_plains");
    REQUIRE(WorldWalkableCount(BuildRawZoneGrid(data, map)) == 13169);
    ZoneGrid full = BuildRawZoneGrid(data, map);
    ApplyCampBlockers(full, map);
    CHECK(WorldWalkableCount(full) == 13169 - 2 * 8);
  }

  // Split from "random events 13.1: exported config, definitions and zone data": the emerald_plains event data.
  TEST_CASE("random events 13.1: the emerald_plains event data (ambush pool and counts, merchant, puzzle, rescue)") {
    const RandomEventsDef& def = WD().World().randomEvents;
    const ZoneEventDataDef* ep = def.ForZone("emerald_plains");
    REQUIRE(ep != nullptr);
    CHECK(ep->ambushMonsters == std::vector<std::string>{"risen_footman", "gravegnawer"});  // Hands from the Mud
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
  // def (the fallback ambush / rescue monster) is the breachclot.
  TEST_CASE("random events 13.3: the emerald_plains zone list starts with breachclot (the fallback monster)") {
    const std::string first = WD().Monsters().ZoneList("emerald_plains")->monsterIds.front();
    CHECK(first == "breachclot");
  }

  // chapter1.md 15 (GameData/world.ts overrides): emerald_plains shows the `march` mood, the dawn mood waits for the
  // chapter, the Breach Rim region has its own; each with its fog, R18 light scale and weather. ember_tower keeps plains.
  TEST_CASE("zone moods (remake): march, march_dawn and the Breach Rim; fog, light scale, weather; the tower stays plains") {
    const ZoneMoodTables& m = WD().World().moods;
    MapTheme t{};
    REQUIRE(m.ThemeFor("emerald_plains", t));
    CHECK(t == MapTheme::March);
    const ZoneMoodDef& march = m.moods[EnumIndex(MapTheme::March)];
    CHECK(march.defined);
    CHECK(march.ambient == 0xA8AEBBu);
    CHECK(march.ambientAlpha == 0.30);
    CHECK(march.vignette == 0x08070Au);
    CHECK(march.vignetteAlpha == 0.58);
    CHECK(march.saturation == 0.68);
    CHECK(march.contrast == 1.12);
    CHECK(march.fogColor == 0x4E525Cu);
    CHECK(march.fogMaxOpacity == 0.40);
    CHECK(march.fogDensity == 1.15);
    CHECK(march.lightScale == 0.32);
    CHECK_FALSE(march.hasMoodWeather);
    const ZoneMoodDef& dawn = m.moods[EnumIndex(MapTheme::MarchDawn)];
    CHECK(dawn.defined);
    CHECK(dawn.ambient == 0xC8B8A0u);
    CHECK(dawn.fogColor == 0x6A6660u);
    CHECK(dawn.lightScale == 0.22);
    REQUIRE(dawn.hasMoodWeather);
    CHECK(dawn.moodWeather.ambience == "dust_motes");
    const ZoneMoodDef& rim = m.moods[EnumIndex(MapTheme::BreachRim)];
    CHECK(rim.defined);
    CHECK(rim.ambient == 0x6A4A8Au);
    CHECK(rim.fogColor == 0x3A2A48u);
    CHECK(rim.lightScale == 0.40);
    REQUIRE(rim.hasMoodWeather);
    CHECK(rim.moodWeather.ambience == "sparks");
    CHECK(rim.moodWeather.tint == 0xB02FD0u);
    const ZoneWeatherDef* w = m.WeatherFor("emerald_plains");
    REQUIRE(w != nullptr);
    CHECK(w->type == "none");
    CHECK(w->ambience == "ashfall");
    CHECK(w->tint == 0xCFCAC0u);
    // The homestead keeps the warm plains look and the pollen it used to borrow from emerald_plains.
    REQUIRE(m.ThemeFor("ember_tower", t));
    CHECK(t == MapTheme::Plains);
    const ZoneWeatherDef* tower = m.WeatherFor("ember_tower");
    REQUIRE(tower != nullptr);
    CHECK(tower->ambience == "pollen");
    CHECK(m.moods[EnumIndex(MapTheme::Plains)].lightScale == 0);  // R18's formula
  }

  // chapter1.md M5 / 17 (the one re-baseline, DR4): the finale slot defends the Boneyard Gate (10,32), 18 tiles from
  // camp 1, three authored waves on the half ring away from the camp (K1 waves / approachDeg).
  TEST_CASE("the Boneyard Gate: the M5 slot's defend target, its three waves and the approach away from camp 1") {
    const QuestDef* q = WD().FindQuest("q_rf_last_toll");
    REQUIRE(q != nullptr);
    REQUIRE(q->hasDefendTarget);
    const DefendTargetDef& dt = q->defendTarget;
    CHECK(dt.pos == TilePos{10, 32});
    CHECK(dt.totalWaves == 3);
    CHECK(dt.hasApproach);
    CHECK(dt.approachDeg == 106);
    CHECK(Dist(Vec2(10, 32), Vec2(15, 15)) > 17.7);  // 18 tiles from the camp centre: outside its safe radius (9)
    auto flat = [](const std::vector<DefendWaveEntry>& wave) {
      std::vector<std::string> out;
      for (const DefendWaveEntry& e : wave) {
        for (int32_t i = 0; i < e.count; ++i) out.push_back(e.monsterId);
      }
      return out;
    };
    REQUIRE(dt.waves.size() == 3);
    CHECK(flat(dt.waves[0]) == std::vector<std::string>(5, "risen_footman"));
    CHECK(flat(dt.waves[1]) == std::vector<std::string>{"gravegnawer", "gravegnawer", "gravegnawer", "gravegnawer",
                                                        "cinderbloat", "cinderbloat"});
    CHECK(flat(dt.waves[2]) == std::vector<std::string>{"risen_shieldbearer", "risen_shieldbearer", "dead_standard_bearer",
                                                        "oath_zealot", "oath_zealot"});
    CHECK(q->reacceptable);  // a fallen gate is offered again: the chapter stays completable
    REQUIRE(q->objectives.size() == 1);
    CHECK(q->objectives[0].type == ObjectiveType::DefendWave);
    CHECK(q->objectives[0].required == 3);
    CHECK(WD().Strings().Lookup(LocaleId::En, DefendTargetNameKey(q->id)) != nullptr);
  }
}  // TEST_SUITE

// =====================================================================================================================
// Pets / homestead: Chapter 1 embers (from pets_test.cpp)
// =====================================================================================================================

TEST_SUITE("pets") {
  using namespace abyss::test::pets_fx;

  TEST_CASE("embers: the Chapter 1 quests pay 23 in total (4.4.1; chapter1.md 7: 2 x 4 + 7 main, 1 x 5 + 3 side)") {
    int32_t total = 0;
    for (const QuestDef& q : PD().Quests().quests) {
      if (q.zone != "emerald_plains") continue;
      total += EmbersForQuest(HT(), q.category, q.rewards.hasEmbers, q.rewards.embers);
    }
    CHECK(total == 23);
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
    save.discoveredHiddenAreas = {"hidden_rf_paymaster"};
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
    CHECK(sim->Context().sys.lore->ClaimHiddenReward("hidden_rf_paymaster", 1));
    CHECK(sim->Context().sys.hero->Gold() == gold + 200);
    CHECK(lore.IsRewardClaimed("hidden_rf_paymaster", 1));
    CHECK_FALSE(sim->Context().sys.lore->ClaimHiddenReward("hidden_rf_paymaster", 1));
    CHECK(lore.HiddenRewards().size() == 1);

    SaveData after;
    sim->BuildSave(after, 0);
    CHECK(after.hiddenRewardsClaimed == std::vector<std::string>{"hidden_rf_paymaster#1"});
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
  TEST_CASE("monster tables: roster size, the gnawer, the Corpseherd (M5), the emerald_plains list, mini-boss and hunts") {
    const MonsterTables& m = test::RealData().Monsters();
    // 29 - the 4 web Chapter 1 defs + 12 roster defs + the Corpseherd + Valdras's roll-call standard-bearer (2F)
    CHECK(m.defs.size() == 39);
    const MonsterDef* gnawer = m.Find("gravegnawer");
    REQUIRE(gnawer);
    CHECK(gnawer->hp == 42);
    CHECK(gnawer->damage == 7);
    CHECK(gnawer->defense == 3);
    CHECK(gnawer->attackSpeedMs == 950);
    CHECK_FALSE(gnawer->isRanged);
    CHECK(gnawer->nameKey == "data.monster.gravegnawer");
    const MonsterDef* herd = m.Find("miniboss_corpseherd");
    REQUIRE(herd);
    CHECK(herd->attackRange == 3);  // M5's role, authored
    CHECK(herd->isRanged);
    const ZoneMonsterList* list = m.ZoneList("emerald_plains");
    REQUIRE(list);
    CHECK(list->monsterIds == std::vector<std::string>{"breachclot", "risen_footman", "gravegnawer", "cinderbloat",
                                                       "risen_arbalest", "oath_zealot", "deserter", "deserter_drummer",
                                                       "risen_shieldbearer", "dead_standard_bearer", "bonegorger",
                                                       "valdras", "rollcall_standard_bearer"});
    for (const char* gone : {"slime_green", "goblin", "goblin_chief", "miniboss_goblin_shaman"}) CHECK(m.Find(gone) == nullptr);
    const MiniBossEntry* mb = m.MiniBossFor("emerald_plains");
    REQUIRE(mb);
    CHECK(mb->spawn == TilePos{60, 55});
    CHECK(m.MiniBossDialogue("miniboss_corpseherd")->nodes.size() == 3);
    CHECK(m.hunts.size() == 18);  // + Hungerbell (M2); Dreck and Harke hold the S3 / S4 slots
    const HuntDef* dreck = m.FindHunt("hunt_dreck");
    REQUIRE(dreck);
    CHECK(dreck->monsterId == "deserter");
    CHECK(dreck->defNormal.hp == 240);
    CHECK(dreck->defNormal.damage == 14);
    CHECK(dreck->revealAfterPrevious);
    CHECK(dreck->spawn == TilePos{60, 48});
    const HuntDef* hungerbell = m.FindHunt("hunt_hungerbell");
    REQUIRE(hungerbell);
    CHECK(hungerbell->monsterId == "bonegorger");
    CHECK(hungerbell->revealAfterPrevious);
    CHECK(hungerbell->spawn == TilePos{45, 35});
    CHECK(hungerbell->minionMonsterId == "gravegnawer");
    CHECK(hungerbell->minionCount == 3);
    const HuntDef* harke = m.FindHunt("hunt_harke");
    REQUIRE(harke);
    CHECK_FALSE(harke->revealAfterPrevious);  // out on accept
    CHECK(harke->spawn == TilePos{82, 72});
    CHECK(harke->minionCount == 4);
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
    CHECK(ep->generatorAvoid.size() == 30);  // DR4
    REQUIRE(ep->camps.size() == 2);
    CHECK(ep->camps[0].pos == TilePos{15, 15});
    CHECK(ep->playerStart == TilePos{15, 22});
    CHECK(ep->levelMin == 1);
    CHECK(ep->safeZoneRadiusEffective == 9);
    CHECK(ep->orderIndex == 0);
    CHECK(w.randomEvents.ForZone("emerald_plains")->ambushCountMin == 3);
    // chapter1.md 3: Brother Aldwin joins Knellwatch; I7: the stash keeper stands in the camp (appended slot); Mag Crowe
    // trades in Crowmarket.
    CHECK(ep->camps[0].npcs == std::vector<std::string>{"blacksmith", "merchant", "quest_elder", "knell_chaplain", "stash"});
    CHECK(ep->camps[1].npcs == std::vector<std::string>{"crow_trader"});
    // Dark remake presentation (remake schema map.presentationTheme / map.regions): the march look, the Breach Rim.
    CHECK(ep->hasPresentationTheme);
    CHECK(ep->presentationTheme == MapTheme::March);
    REQUIRE(ep->regions.size() == 1);
    CHECK(ep->regions[0].id == "breach_rim");
    CHECK(ep->regions[0].center == TilePos{15, 95});
    CHECK(ep->regions[0].radius == 16);
    CHECK(ep->regions[0].mood == MapTheme::BreachRim);
    // The sealed Pit of the Faithful (Wave 3): a generator input of the re-baseline, a toast until K7.
    REQUIRE(ep->subDungeonEntrances.size() == 1);
    CHECK(ep->subDungeonEntrances[0].pos == TilePos{17, 26});
    CHECK(ep->subDungeonEntrances[0].targetSubDungeon == "sub_rf_pit");
    CHECK(ep->subDungeonEntrances[0].sealed);
    CHECK(ep->subDungeonEntrances[0].sealedTextKey == "data.subDungeonSealed.entrance_rf_pit");
    MapTheme mood{};
    REQUIRE(w.moods.ThemeFor("emerald_plains", mood));
    CHECK(mood == MapTheme::March);
    REQUIRE(w.moods.ThemeFor("ember_tower", mood));
    CHECK(mood == MapTheme::Plains);  // the homestead keeps the warm plains look
  }

  // Split from "quests, npcs, dialogue, story, lore": the table sizes and the Chapter 1 spot checks.
  TEST_CASE("quests, npcs, dialogue, story, lore: table sizes and the Chapter 1 quests, elder, beats and lore") {
    const DataStore& d = test::RealData();
    const QuestTables& q = d.Quests();
    CHECK(q.quests.size() == 58);
    const QuestDef* burial = q.Find("q_rf_burial_detail");
    REQUIRE(burial);
    CHECK(burial->category == QuestCategory::Main);
    REQUIRE(burial->objectives.size() == 1);
    CHECK(burial->objectives[0].type == ObjectiveType::Kill);
    CHECK(burial->objectives[0].targetId == "risen_footman");  // the burial detail, still digging
    CHECK(burial->objectives[0].required == 10);
    CHECK(burial->giverNpcId == "quest_elder");
    CHECK(burial->embersOnTurnIn == 2);
    const QuestDef* marshal = q.Find("q_rf_marshal");
    REQUIRE(marshal);
    CHECK(marshal->questArea.col == 15);  // Q4 data fix: centred on Valdras's spawn
    CHECK(marshal->questArea.row == 95);
    CHECK(marshal->rewards.choices.size() == 2);
    const QuestDef* moss = q.Find("q_rf_barrowmoss");
    REQUIRE(moss);
    CHECK(moss->objectives[0].sourceKind == ItemSourceKind::Gather);
    CHECK(moss->objectives[0].gatherCount == 7);
    CHECK(moss->objectives[0].itemKind == "moss");
    CHECK(q.achievements.size() == 12);
    CHECK(q.ItemKindFor("mat_herb") == "herb");             // the tuning's kind table (web) is unchanged
    CHECK(q.ItemKindFor("mat_breach_ichor") == "relic");     // not in it: S1's objective carries its own kind

    CHECK(d.Npcs().npcs.size() == 29);  // + knell_chaplain, crow_trader
    CHECK(d.Npcs().GiverOf("q_rf_burial_detail")->id == "quest_elder");
    CHECK(d.Npcs().GiverOf("q_rf_barrowmoss")->id == "knell_chaplain");
    CHECK(d.Dialogues().trees.size() == 10);
    const DialogueTree* elder = d.Dialogues().Find("quest_elder");
    REQUIRE(elder);
    CHECK(elder->FindNode(elder->startNodeId) != nullptr);

    const StoryScript& s = d.Story();
    CHECK(s.cutscenes.size() == 25);  // - cs_ep_mark / cs_ep_whisper / cs_ep_finale + the six cs_rf_* beats
    CHECK(s.triggers.size() == 19);
    CHECK(s.triggers[0].subjectId == "q_rf_burial_detail");
    CHECK(s.BossIntroFor("valdras")->cutscene == "cs_boss_valdras");
    const Cutscene* brand = s.FindCutscene("cs_rf_brand");
    REQUIRE(brand);
    CHECK(brand->steps[0].kind == StoryStepKind::Focus);
    CHECK(brand->steps[0].target.kind == StoryActorKind::Npc);
    CHECK(s.ChapterFor("emerald_plains") != nullptr);
    CHECK(d.Lore().entries.size() == 24);
    CHECK(d.Lore().ForZone("emerald_plains").size() == 7);  // lore_rf_01..04 + the three echoes (chapter1.md 12)
  }
}  // TEST_SUITE

// =====================================================================================================================
// The content layer itself: the TestContent.h handles, the synthetic fixtures' provenance, the isolation gate
// =====================================================================================================================

namespace {

using namespace abyss::test::src;

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
        "// \"valdras\" in a comment\n"
        "TEST_CASE(\"valdras hits\") {\n"
        "  const char q = '\"'; Spawn(\"monster_valdras\"); Find(\"valdras\");\n"
        "  Key(\"data.monster.breachclot\"); Raw(R\"x(\"cs_rf_brand\":1)x\"); /* \"lore_rf_01\" */\n"
        "}\n";
    const std::vector<SrcLiteral> lits = SrcStringLiterals(sample);
    REQUIRE(lits.size() == 5);
    CHECK(lits[0].title);
    CHECK(lits[1].text == "monster_valdras");
    CHECK(lits[4].text == "\"cs_rf_brand\":1");
    CHECK(SrcSpelledIds(sample) == std::vector<std::string>{"valdras", "breachclot", "cs_rf_brand"});

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

  // T0 provenance: the combat and AI vectors were cut on the web Chapter 1 roster (goblin, chief, slime, shaman). The
  // dark remake replaced that roster (package 1B part 1), so the check is re-pointed as planned ("the content switch
  // drops or re-points this check, never the fixtures"): the fixtures keep the T0 numbers, frozen below, their ids stay
  // out of the data, and their sprite keys still name shipped art (attack contact beats are read from the asset
  // manifest by sprite key; the shipped goblin, chief, shaman and slime assets stay in the repository, monsters.md 5).
  TEST_CASE("TestContent.h: the synthetic fixtures keep the T0 roster numbers and still map their shipped art") {
    struct Row {
      const MonsterDef* fx;
      const char* t0;  // the web def it was cut from (gone from the data since the remake roster)
      int32_t level;
      double hp, dmg, def, speed, aggro, range, atkSpeed, exp, g0, g1;
      const char* spriteKey;
      AnimRig rig;
      bool elite, miniBoss;
    };
    const Row rows[] = {
        {&test::fixture::Grunt(), "goblin", 3, 55, 8, 4, 55, 5, 1.5, 1200, 18, 3, 6, "monster_goblin", AnimRig::Humanoid,
         false, false},
        {&test::fixture::Brute(), "goblin_chief", 5, 160, 14, 8, 50, 6, 1.8, 1000, 55, 8, 15, "monster_goblin_chief",
         AnimRig::Humanoid, true, false},
        {&test::fixture::Ooze(), "slime_green", 1, 30, 5, 2, 40, 4, 1.2, 1500, 12, 2, 4, "monster_slime", AnimRig::Slime,
         false, false},
        {&test::fixture::Caster(), "miniboss_goblin_shaman", 6, 280, 16, 10, 45, 7, 3.0, 1300, 90, 15, 30,
         "monster_goblin_shaman", AnimRig::Humanoid, true, true},
    };
    const AssetManifest& assets = MD().Assets();
    for (const Row& r : rows) {
      CAPTURE(r.t0);
      const MonsterDef& f = *r.fx;
      CHECK(MD().FindMonster(r.t0) == nullptr);
      CHECK(MD().FindMonster(f.id) == nullptr);
      CHECK(f.nameKey == "data.monster." + f.id);
      CHECK(f.level == r.level);
      CHECK(f.hp == r.hp);
      CHECK(f.damage == r.dmg);
      CHECK(f.defense == r.def);
      CHECK(f.speed == r.speed);
      CHECK(f.aggroRange == r.aggro);
      CHECK(f.attackRange == r.range);
      CHECK(f.attackSpeedMs == r.atkSpeed);
      CHECK(f.expReward == r.exp);
      CHECK(f.goldMin == r.g0);
      CHECK(f.goldMax == r.g1);
      CHECK(f.spriteKey == r.spriteKey);
      CHECK(f.animCategory == r.rig);
      CHECK(f.elite == r.elite);
      CHECK(f.isMiniBoss == r.miniBoss);
      CHECK(f.isRanged == (r.range > 2.5));
      CHECK(f.projectileColor == 0xcc44ccu);
      CHECK(f.onHitStatus.empty());
      if (assets.loaded) CHECK(assets.FindByGameId(f.spriteKey) != nullptr);
    }
  }
}  // TEST_SUITE
