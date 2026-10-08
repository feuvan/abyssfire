// Monsters area: definitions pipeline, AI, spatial grid, spawning, hunts, mini-boss. Spec vectors: monsters-ai.md
// section 19. Foundation smoke tests (the spatial grid is implemented in the header and fully tested here).
#include "SimHarness.h"
#include <algorithm>

#include "abyss/monsters/Hunts.h"
#include "abyss/monsters/MonsterAI.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/monsters/SpatialGrid.h"
#include "doctest/doctest.h"

using namespace abyss;

TEST_SUITE("monsters") {
  TEST_CASE("SpatialGrid: cell index, inclusive radius, row-major order, nearest ties keep the first") {
    SpatialGrid g(120, 120);
    CHECK(g.CellIndex(0, 0) == 0);
    CHECK(g.CellIndex(17, 0) == 1);
    CHECK(g.CellIndex(0, 16) == 8);
    CHECK(g.CellIndex(500, 500) == 63);  // clamped
    g.Insert(1, {20, 20});
    g.Insert(2, {10, 10});
    g.Insert(3, {12, 10});
    std::vector<EntityId> out;
    g.QueryRadius(10, 10, 2, out);  // dx^2 + dy^2 <= r^2 inclusive
    CHECK(out == std::vector<EntityId>{2, 3});
    g.QueryRadius(15, 15, 10, out);
    CHECK(out == std::vector<EntityId>{2, 3, 1});  // cell (0,0) before cell (1,1)
    g.Insert(4, {8, 10});                          // same distance from (10,10) as id 3
    CHECK(g.FindNearest(10, 10.5, 5, [](EntityId id) { return id != 2; }) == 3);
    g.Update(2, {100, 100});
    g.QueryRadius(10, 10, 2, out);
    CHECK(out == std::vector<EntityId>{3, 4});
    g.Remove(3);
    CHECK(g.Size() == 3);
    CHECK_FALSE(g.Contains(3));
  }

  TEST_CASE("hunt objective index on the real Chapter 1 data") {
    const DataStore& data = test::RealData();
    const QuestDef* q = data.FindQuest("q_lost_pendant");
    REQUIRE(q != nullptr);
    CHECK(HuntObjectiveIndex(*q, "hunt_pendant_thief") >= 0);
    CHECK(HuntObjectiveIndex(*q, "no_such_hunt") == -1);
  }

  TEST_CASE("SpatialGrid: a default-constructed grid is safe before Reset (MonsterSystem before the first zone)") {
    SpatialGrid g;
    std::vector<EntityId> out{99};
    g.QueryRadius(5, 5, 100, out);
    CHECK(out.empty());
    CHECK(g.FindNearest(0, 0, 1e9) == kNoEntity);
    g.Insert(7, {3, 4});
    g.QueryRadius(0, 0, 10, out);
    CHECK(out == std::vector<EntityId>{7});
    g.Update(7, {1e9, -1e9});  // far outside: clamped into the single cell
    CHECK(g.FindNearest(0, 0, 1e300) == 7);
    g.Remove(7);
    CHECK(g.Size() == 0);
  }

  TEST_CASE("M2 provoke contract: idle / patrol, or chase beyond aggro; Returning ignores it") {
    const MonsterAiDef& ai = test::RealData().Monsters().ai;
    MonsterInstance m;
    m.def.aggroRange = 4;
    m.hp = m.maxHp = 10;
    for (MonsterState s : {MonsterState::Idle, MonsterState::Patrol}) {
      m.state = s;
      m.provokedUntilMs = 0;
      CHECK(ProvokeMonster(m, ai, 1000, 2.0));
      CHECK(m.state == MonsterState::Chase);
      CHECK(m.provokedUntilMs == 1000 + ai.provokeDurationMs);
    }
    // A chasing slime hit from beyond its aggro range (range-6 skill): provoked, so the 1.5x drop does not apply.
    m.state = MonsterState::Chase;
    CHECK(ProvokeMonster(m, ai, 2000, 6.0));
    CHECK(m.provokedUntilMs == 2000 + ai.provokeDurationMs);
    // Chasing within aggro range: nothing to provoke, the timer is not refreshed.
    CHECK_FALSE(ProvokeMonster(m, ai, 3000, 3.0));
    CHECK(m.provokedUntilMs == 2000 + ai.provokeDurationMs);
    // M1: Returning ignores the hero (and every provoke) until home; Attack and Dead are unchanged.
    for (MonsterState s : {MonsterState::Returning, MonsterState::Attack, MonsterState::Dead}) {
      m.state = s;
      m.provokedUntilMs = 0;
      CHECK_FALSE(ProvokeMonster(m, ai, 4000, 10.0));
      CHECK(m.state == s);
      CHECK(m.provokedUntilMs == 0);
    }
    CHECK(ai.provokeDurationMs == 5000);
  }

  TEST_CASE("M7 story boss is data: id, quests and the storyBoss role") {
    const DataStore& data = test::RealData();
    const MonsterAiDef& ai = data.Monsters().ai;
    CHECK(ai.storyBossId == "goblin_chief");
    CHECK(ai.storyBossNotAfterQuestTurnIn == "q_find_goblin_chief");
    CHECK(ai.chapterCompleteQuest == "q_secure_plains");
    CHECK(ai.storyBossOncePerVisit);
    CHECK(ai.storyBossFarmableAfterChapter);
    CHECK(std::find(ai.noRespawnRoles.begin(), ai.noRespawnRoles.end(), "storyBoss") != ai.noRespawnRoles.end());
    MonsterRole r{};
    CHECK(ParseEnum("storyBoss", r));
    CHECK(r == MonsterRole::StoryBoss);
    // Q4 data fix uses the same data: the chief's quest area is centred on its spawn.
    const QuestDef* q = data.FindQuest(ai.storyBossNotAfterQuestTurnIn);
    REQUIRE(q != nullptr);
    CHECK(q->questArea.col == 15);
    CHECK(q->questArea.row == 95);
  }

  TEST_CASE("MonsterSystem constructs and is empty before a zone") {
    test::SimHarness h;
    MonsterSystem m(h.ctx);
    CHECK(m.All().empty());
    CHECK(m.MiniBoss() == kNoEntity);
    CHECK_FALSE(m.AnyAttacking());
    std::vector<EntityId> none;
    m.QueryAlive({10, 10}, 5, none);  // the default grid is queryable before the first zone Reset
    CHECK(none.empty());
    CHECK(m.NearestAlive({10, 10}, 50) == kNoEntity);
    CHECK(m.NearestAliveOfDef("goblin", {0, 0}) == kNoEntity);
    MonsterInstance inst;
    inst.affixes.push_back({EliteAffixType::Swift, 0, 0});
    CHECK(inst.AffixLootBonus(h.ctx.data.Combat().eliteAffixes) ==
          h.ctx.data.Combat().eliteAffixes.Def(EliteAffixType::Swift).lootQualityBonus);
  }
}
