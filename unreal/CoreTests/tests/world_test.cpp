// World area (+ audio): grid, map generator, pathfinding, exploration, zone runtime, locomotion, random events,
// music / SFX rules. Spec vectors: world-map-nav.md section 21 (golden maps in CoreTests/golden/maps), audio.md 9.8.
// Foundation smoke tests only.
#include "SimHarness.h"
#include "abyss/audio/Audio.h"
#include "abyss/world/Exploration.h"
#include "abyss/world/Grid.h"
#include "abyss/hero/Hero.h"
#include "abyss/sim/GameSim.h"
#include "abyss/world/Locomotion.h"
#include "abyss/world/MapGen.h"
#include "doctest/doctest.h"

using namespace abyss;

TEST_SUITE("world") {
  TEST_CASE("Park-Miller SeededRandom reference vector (world 4.1, seed 12345)") {
    ParkMillerRng r(12345);
    const int64_t states[] = {207482415, 1790989824, 2035175616, 77048696, 24794531};
    const double values[] = {0.096616528086938450, 0.83399462730995810, 0.94770249766083670, 0.035878594532495915,
                             0.011545852768743274};
    for (int i = 0; i < 5; ++i) {
      const double v = r.Next();
      CHECK(r.State() == states[i]);
      CHECK(v == values[i]);
    }
    ParkMillerRng z(0);  // state <= 0 -> += 2147483646
    CHECK(z.State() == 2147483646);
  }

  TEST_CASE("ZoneGrid walkability follows tile types (world 2.1)") {
    ZoneGrid g(4, 3);
    CHECK(g.Walkable(1, 1));
    g.SetTile(1, 1, TileType::Water);
    CHECK_FALSE(g.Walkable(1, 1));
    g.SetTile(2, 1, TileType::Camp);
    CHECK(g.Walkable(2, 1));
    CHECK_FALSE(g.Walkable(-1, 0));
    CHECK_FALSE(g.Walkable(4, 0));
    CHECK(g.WalkableAt({1.6, 1.4}) == true);  // rounds to (2,1) camp
    g.SetWalkable(2, 1, false);
    CHECK_FALSE(g.Walkable(2, 1));
  }

  TEST_CASE("exploration reveals the radius-10 disc (world 10.1)") {
    ExplorationGrid e;
    e.Reset(40, 40);
    e.Reveal(20, 20, 10);
    CHECK(e.IsExplored(20, 10));
    CHECK(e.IsExplored(30, 20));
    CHECK_FALSE(e.IsExplored(28, 28));  // 8^2 + 8^2 = 128 > 100
    CHECK(e.IsExplored(27, 27));        // 98 <= 100
  }

  TEST_CASE("screen direction to tile direction (save-ui-input 5.2)") {
    CHECK(ScreenDirToTile({1, 0}) == Vec2(1, -1));
    CHECK(ScreenDirToTile({0, 1}) == Vec2(1, 1));
  }
}

TEST_SUITE("audio") {
  TEST_CASE("skill and pickup cues come from the audio rules table") {
    const AudioRulesDef& r = test::RealData().Audio().rules;
    CHECK(SfxForSkill(r, DamageType::Fire) == SfxId::SkillFire);
    CHECK(SfxForPickup(r, ItemQuality::Set) == SfxId::LootLegendary);
  }

  TEST_CASE("C4 charge dash: timed straight move on the sim clock, Started -> Arrived / Interrupted") {
    auto sim = GameSim::Create(test::RealData(), SimConfig{});
    REQUIRE(sim != nullptr);
    REQUIRE(sim->NewGame(ClassId::Warrior, Difficulty::Normal, 3));
    sim->Step();
    HeroLocomotion& loco = *sim->Context().sys.locomotion;
    const Vec2 from = sim->View().hero.pos;
    const Vec2 to(from.x + 3, from.y);
    CHECK_FALSE(loco.StartDash(to, 0, "charge"));  // non-positive duration: nothing starts
    REQUIRE(loco.StartDash(to, 300, "charge"));
    CHECK_FALSE(loco.StartDash(to, 300, "charge"));  // one at a time
    CHECK(test::CountEvents<EvHeroDash>(sim->Context().events) == 1);
    bool arrived = false;
    int steps = 0;
    while (!arrived && steps < 40) {
      sim->Step();
      ++steps;
      CHECK(sim->View().hero.pos.y == from.y);
      for (const Event& e : sim->Events()) {
        if (const EvHeroDash* d = std::get_if<EvHeroDash>(&e)) {
          if (d->phase == EvHeroDash::Phase::Arrived) arrived = true;
        }
      }
      if (!arrived) CHECK(sim->View().hero.dashing);
    }
    CHECK(arrived);
    CHECK(steps >= 17);
    CHECK(steps <= 19);  // 300 ms at 60 Hz
    CHECK(sim->View().hero.pos == to);
    CHECK_FALSE(sim->View().hero.dashing);

    // A hero that leaves Alive mid-dash stops where it stands.
    REQUIRE(loco.StartDash(from, 300, "charge"));
    sim->Step();
    const double midX = sim->View().hero.pos.x;
    CHECK(midX < to.x);
    sim->Context().sys.hero->SetLife(HeroLife::Dying);
    sim->Step();
    bool interrupted = false;
    for (const Event& e : sim->Events()) {
      if (const EvHeroDash* d = std::get_if<EvHeroDash>(&e)) interrupted |= d->phase == EvHeroDash::Phase::Interrupted;
    }
    CHECK(interrupted);
    CHECK_FALSE(loco.IsDashing());
    CHECK(sim->View().hero.pos.x == midX);
    sim->Context().sys.hero->SetLife(HeroLife::Alive);
  }
}
