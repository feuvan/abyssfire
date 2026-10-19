// Monsters area: definitions pipeline, AI, spatial grid, spawning, respawn, hunts, mini-boss, elite affixes, queries.
// Spec vectors: monsters-ai.md section 19 (+ the tables of sections 2.1-2.4), combat-feel.md 14 / 17 / 24; ported web
// Vitest suites: EliteAffixSystem.test.ts, QuestHunts.test.ts, DifficultySystem.test.ts (scaleMonster),
// mini-bosses-lore.test.ts (mini-boss data), SpatialGrid.test.ts, PhaserOptimizationPass.test.ts (AI culling),
// MonsterFacing.test.ts (heading wiring). Port decisions: DECISIONS M1-M11, W9.
#include "MonstersFixtures.h"
#include "SimHarness.h"
#include "TestContent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "abyss/base/Math.h"
#include "abyss/combat/Combat.h"
#include "abyss/combat/HitFeedback.h"
#include "abyss/combat/Projectiles.h"
#include "abyss/combat/SoulEcho.h"
#include "abyss/combat/StatusEffects.h"
#include "abyss/hero/Hero.h"
#include "abyss/hero/Rewards.h"
#include "abyss/monsters/EliteAffixes.h"
#include "abyss/monsters/Hunts.h"
#include "abyss/monsters/MonsterAI.h"
#include "abyss/monsters/MonsterDefs.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/monsters/SpatialGrid.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/save/SaveData.h"
#include "abyss/sim/GameSim.h"
#include "abyss/sim/Snapshot.h"
#include "abyss/world/Locomotion.h"
#include "abyss/world/Zone.h"
#include "doctest/doctest.h"

using namespace abyss;
using namespace abyss::test::monsters_fx;
namespace fixture = abyss::test::fixture;

TEST_SUITE("monsters") {
  // ===================================================================================================================
  // Spatial grid (monsters 12; SpatialGrid.test.ts)
  // ===================================================================================================================
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

  TEST_CASE("SpatialGrid vector 18: zero radius is inclusive, nearest tie keeps the first in scan order") {
    SpatialGrid g(120, 120, 16);
    g.Insert(1, {16, 16});
    g.Insert(2, {16, 16.0001});
    std::vector<EntityId> out;
    g.QueryRadius(16, 16, 0, out);
    CHECK(out == std::vector<EntityId>{1});
    // Ties: (14,16) and (18,16) are both 2 from (16,16); (14,16) lives in an earlier cell (row-major) -> it wins even
    // though it was inserted later.
    SpatialGrid t(120, 120, 16);
    t.Insert(10, {18, 16});
    t.Insert(11, {14, 16});
    CHECK(t.FindNearest(16, 16, 5) == 11);
    // Same cell: insertion order decides.
    SpatialGrid s(120, 120, 16);
    s.Insert(20, {20, 20});
    s.Insert(21, {24, 20});
    CHECK(s.FindNearest(22, 20, 5) == 20);
  }

  TEST_CASE("SpatialGrid: queries match brute force; respawn = remove + insert; cell sizes") {
    SpatialGrid g(120, 120, 8);
    std::vector<std::pair<EntityId, Vec2>> ents;
    Rng rng(42);
    for (EntityId id = 1; id <= 300; ++id) {
      const Vec2 p(rng.Float01() * 120, rng.Float01() * 120);
      ents.emplace_back(id, p);
      g.Insert(id, p);
    }
    for (int q = 0; q < 50; ++q) {
      const Vec2 c(rng.Float01() * 120, rng.Float01() * 120);
      const double r = 1 + rng.Float01() * 30;
      std::vector<EntityId> got;
      g.QueryRadius(c.x, c.y, r, got);
      std::set<EntityId> want;
      for (const auto& [id, p] : ents) {
        if (DistSq(p, c) <= r * r) want.insert(id);
      }
      CHECK(std::set<EntityId>(got.begin(), got.end()) == want);
      CHECK(got.size() == want.size());
    }
    // Respawn: the dead entry leaves, the new instance enters at its new tile.
    g.Remove(5);
    g.Insert(1000, {60, 60});
    std::vector<EntityId> at;
    g.QueryRadius(60, 60, 0.01, at);
    CHECK(std::find(at.begin(), at.end(), static_cast<EntityId>(1000)) != at.end());
    CHECK_FALSE(g.Contains(5));
    CHECK(g.Size() == 300);
  }

  // ===================================================================================================================
  // Data (monsters 1.1, 2.1, 8.1, 17; mini-bosses-lore.test.ts)
  // ===================================================================================================================

  TEST_CASE("derived data 1.1: onHitStatus only on fire_elemental and phoenix; projectile colours") {
    for (const MonsterDef& d : MD().Monsters().defs) {
      CAPTURE(d.id);
      const bool fiery = d.spriteKey.find("fire") != std::string::npos || d.spriteKey.find("phoenix") != std::string::npos;
      if (d.id == "fire_elemental" || d.id == "phoenix") {
        REQUIRE(d.onHitStatus.size() == 1);
        CHECK(d.onHitStatus[0].status == StatusType::Burn);
        CHECK(d.onHitStatus[0].chance == doctest::Approx(0.3));
        CHECK(d.onHitStatus[0].durationMs == 3000);
      }
      if (fiery) {
        CHECK(d.projectileColor == 0xff6600u);
      } else if (d.spriteKey.find("ice") != std::string::npos) {
        CHECK(d.projectileColor == 0x4488ffu);
      } else {
        CHECK(d.projectileColor == 0xcc44ccu);
      }
    }
  }

  TEST_CASE("mini-boss data (mini-bosses-lore.test.ts): one elite per zone, spawns in bounds, dialogue >= 3 lines") {
    const MonsterTables& t = MD().Monsters();
    const char* zones[] = {"emerald_plains", "twilight_forest", "anvil_mountains", "scorching_desert", "abyss_rift"};
    std::set<std::string> ids;
    int32_t prevLevel = 0;
    double prevHp = 0;
    for (const char* z : zones) {
      CAPTURE(z);
      const MiniBossEntry* e = t.MiniBossFor(z);
      REQUIRE(e != nullptr);
      const MonsterDef& d = MDef(e->monsterId);
      CHECK(d.elite);
      CHECK(d.isMiniBoss);
      CHECK(d.id.rfind("miniboss_", 0) == 0);
      CHECK(d.hp > 0);
      CHECK(d.damage > 0);
      CHECK(d.speed > 0);
      CHECK(d.goldMax >= d.goldMin);
      CHECK(d.level > prevLevel);
      CHECK(d.hp > prevHp);
      prevLevel = d.level;
      prevHp = d.hp;
      ids.insert(d.id);
      REQUIRE(e->hasSpawn);
      const MapDef* map = MD().FindMap(z);
      REQUIRE(map != nullptr);
      CHECK(e->spawn.col >= 0);
      CHECK(e->spawn.col < map->cols);
      CHECK(e->spawn.row >= 0);
      CHECK(e->spawn.row < map->rows);
      const DialogueTree* tree = t.MiniBossDialogue(d.id);
      REQUIRE(tree != nullptr);
      // Walk start -> nextNodeId until isEnd: >= 3 lines, every reference valid, ends properly.
      int32_t lines = 0;
      bool reachedEnd = false;
      const DialogueNode* n = tree->FindNode(tree->startNodeId);
      for (int guard = 0; n != nullptr && guard < 100; ++guard) {
        ++lines;
        CHECK(n->text.size() >= 10);
        if (n->isEnd) {
          reachedEnd = true;
          break;
        }
        n = tree->FindNode(n->nextNodeId);
      }
      CHECK(lines >= 3);
      CHECK(reachedEnd);
    }
    CHECK(ids.size() == 5);
  }

  // ===================================================================================================================
  // Difficulty scaling (combat 14; DifficultySystem.test.ts scaleMonster)
  // ===================================================================================================================
  TEST_CASE("scaleMonster: normal unchanged, nightmare / hell multipliers with JS rounding, gold uses the exp mul") {
    MonsterDef base;
    base.id = "test_monster";
    base.hp = 100;
    base.damage = 20;
    base.defense = 10;
    base.expReward = 50;
    base.goldMin = 5;
    base.goldMax = 15;
    base.aggroRange = 6;
    base.speed = 60;
    const MonsterDef n = ScaleMonsterForDifficulty(base, MDiff(), Difficulty::Normal);
    CHECK(n.hp == 100);
    CHECK(n.damage == 20);
    CHECK(n.defense == 10);
    CHECK(n.expReward == 50);
    CHECK(n.goldMin == 5);
    CHECK(n.goldMax == 15);
    const MonsterDef nm = ScaleMonsterForDifficulty(base, MDiff(), Difficulty::Nightmare);
    CHECK(nm.hp == 150);
    CHECK(nm.damage == 30);
    CHECK(nm.defense == 13);
    CHECK(nm.expReward == 100);
    CHECK(nm.goldMin == 10);
    CHECK(nm.goldMax == 30);
    const MonsterDef hl = ScaleMonsterForDifficulty(base, MDiff(), Difficulty::Hell);
    CHECK(hl.hp == 200);
    CHECK(hl.damage == 40);
    CHECK(hl.defense == 16);
    CHECK(hl.expReward == 150);
    CHECK(hl.goldMin == 15);
    CHECK(hl.goldMax == 45);
    CHECK(hl.aggroRange == 6);  // untouched fields
    CHECK(hl.speed == 60);
    CHECK(base.hp == 100);  // the original is not mutated
    MonsterDef odd = base;
    odd.hp = 77;
    odd.damage = 33;
    odd.defense = 7;
    odd.expReward = 23;
    odd.goldMin = 3;
    odd.goldMax = 11;
    const MonsterDef o = ScaleMonsterForDifficulty(odd, MDiff(), Difficulty::Nightmare);
    CHECK(o.hp == 116);
    CHECK(o.damage == 50);
    CHECK(o.defense == 9);
    CHECK(o.expReward == 46);
    CHECK(o.goldMin == 6);
    CHECK(o.goldMax == 22);
  }

  TEST_CASE("defend-wave scaling 6.4: floor(hp * (1 + 0.3 w)), floor(damage * (1 + 0.2 w))") {
    const MonsterDef& g = MFix(fixture::Grunt());
    const MonsterDef w0 = ScaleDefendWave(g, 0, MAi());
    CHECK(w0.hp == 55);
    CHECK(w0.damage == 8);
    const MonsterDef w2 = ScaleDefendWave(g, 2, MAi());
    CHECK(w2.hp == std::floor(55 * 1.6));
    CHECK(w2.damage == std::floor(8 * 1.4));
    CHECK(w2.defense == g.defense);
  }

  // ===================================================================================================================
  // Quest hunts (monsters 9; QuestHunts.test.ts; vectors 12-14)
  // ===================================================================================================================

  TEST_CASE("findWalkableNear (vector 14): rings 1..radius, dr outer then dc, centre never returned") {
    std::set<std::pair<int32_t, int32_t>> open{{4, 4}, {5, 4}, {5, 5}};
    auto walk = [&open](int32_t c, int32_t r) { return open.count({c, r}) != 0; };
    TilePos out;
    REQUIRE(FindWalkableNear({5, 5}, 6, walk, out));
    CHECK(out == TilePos{4, 4});  // (dr=-1, dc=-1) first; the walkable centre itself is skipped
    open = {{7, 5}};
    REQUIRE(FindWalkableNear({5, 5}, 6, walk, out));
    CHECK(out == TilePos{7, 5});  // ring 2
    open = {{12, 5}};
    CHECK_FALSE(FindWalkableNear({5, 5}, 6, walk, out));  // ring 7 is beyond the radius
  }

  // ===================================================================================================================
  // Elite affixes (combat 17; EliteAffixSystem.test.ts; vector 16; table 2.4)
  // ===================================================================================================================
  TEST_CASE("elite affix definitions 17.1 are exported verbatim") {
    const EliteAffixTable& t = MAffixes();
    REQUIRE(t.order.size() == 7);
    CHECK(t.order[0] == EliteAffixType::FireEnhanced);
    CHECK(t.order[6] == EliteAffixType::Frozen);
    for (EliteAffixType ty : t.order) {
      const EliteAffixDef& d = t.Def(ty);
      CAPTURE(EnumName(ty));
      CHECK(d.type == ty);
      CHECK_FALSE(d.name.empty());
      CHECK_FALSE(d.nameEn.empty());
      CHECK(d.vfxColor > 0u);
      CHECK(d.lootQualityBonus >= 0);
      const std::string key = "sys.eliteAffix.name." + std::string(EnumName(ty));
      CHECK(MD().Strings().Lookup(LocaleId::ZhCN, key) != nullptr);
      CHECK(MD().Strings().Lookup(LocaleId::En, key) != nullptr);
    }
    CHECK(t.Def(EliteAffixType::FireEnhanced).extraFireDamage == doctest::Approx(0.3));
    CHECK(t.Def(EliteAffixType::FireEnhanced).hpMult == doctest::Approx(1.2));
    CHECK(t.Def(EliteAffixType::FireEnhanced).vfxColor == 0xff4400u);
    CHECK(t.Def(EliteAffixType::Swift).speedMult == doctest::Approx(1.6));
    CHECK(t.Def(EliteAffixType::Swift).lootQualityBonus == 3);
    CHECK(t.Def(EliteAffixType::Teleporting).teleportCooldownMs == 5000);
    CHECK(t.Def(EliteAffixType::ExtraStrong).damageMult >= 1.3);
    CHECK(t.Def(EliteAffixType::ExtraStrong).defenseMult == doctest::Approx(1.15));
    CHECK(t.Def(EliteAffixType::CurseAura).curseAuraRadius == 4);
    CHECK(t.Def(EliteAffixType::CurseAura).curseAuraReduction == doctest::Approx(0.15));
    CHECK(t.Def(EliteAffixType::Vampiric).lifestealFraction == doctest::Approx(0.2));
    CHECK(t.Def(EliteAffixType::Frozen).freezeChance == doctest::Approx(0.25));
    CHECK(t.freezeChanceCap == doctest::Approx(0.5));
  }

  TEST_CASE("rollAffixes 17.2: zone counts, unique picks, without-replacement draw order") {
    const EliteAffixTable& t = MAffixes();
    Rng rng(9);
    for (const char* z : {"emerald_plains", "twilight_forest", "anvil_mountains", "nonexistent_zone"}) {
      for (int i = 0; i < 10; ++i) CHECK(RollEliteAffixes(t, z, rng).size() == 1);
    }
    std::set<size_t> desert, rift;
    for (int i = 0; i < 100; ++i) {
      const std::vector<EliteAffixType> d = RollEliteAffixes(t, "scorching_desert", rng);
      CHECK(d.size() >= 1);
      CHECK(d.size() <= 2);
      desert.insert(d.size());
      const std::vector<EliteAffixType> r = RollEliteAffixes(t, "abyss_rift", rng);
      CHECK(r.size() >= 2);
      CHECK(r.size() <= 3);
      rift.insert(r.size());
      CHECK(std::set<EliteAffixType>(r.begin(), r.end()).size() == r.size());
    }
    CHECK(desert.size() == 2);
    CHECK(rift.size() == 2);
    // Draw order: count (randomInt(1,1) still draws), then idx = randomInt(0, 6) over the table order.
    Rng s(1);
    s.Script({0.0, 0.5});  // count draw, then floor(0.5 * 7) = 3 -> extra_strong
    const std::vector<EliteAffixType> one = RollEliteAffixes(t, "emerald_plains", s);
    REQUIRE(one.size() == 1);
    CHECK(one[0] == EliteAffixType::ExtraStrong);
    CHECK(s.ScriptedRemaining() == 0);
    // Splice: the second pick indexes the remaining list.
    Rng s2(1);
    s2.Script({0.99, 0.0, 0.0, 0.99});  // abyss_rift count = 3; picks idx 0, 0, 4 of the shrinking list
    const std::vector<EliteAffixType> three = RollEliteAffixes(t, "abyss_rift", s2);
    REQUIRE(three.size() == 3);
    CHECK(three[0] == EliteAffixType::FireEnhanced);
    CHECK(three[1] == EliteAffixType::Swift);
    CHECK(three[2] == EliteAffixType::Frozen);
    // A count above the pool size is capped at the pool (selectAffixes).
    EliteAffixTable big = t;
    big.zoneCounts.push_back(ZoneAffixCount{"test_zone", 12, 12});
    std::vector<EliteAffixType> all = RollEliteAffixes(big, "test_zone", rng);
    CHECK(all.size() == 7);
    CHECK(std::set<EliteAffixType>(all.begin(), all.end()).size() == 7);
    EliteAffixTable none = t;
    none.zoneCounts.push_back(ZoneAffixCount{"zero_zone", 0, 0});
    CHECK(RollEliteAffixes(none, "zero_zone", rng).empty());
  }

  TEST_CASE("getCombinedStats 17.3: multipliers multiply, extras add, freeze chance capped") {
    const EliteAffixTable& t = MAffixes();
    const CombinedAffixStats empty = CombineAffixes({}, t);
    CHECK(empty.damageMult == 1);
    CHECK(empty.speedMult == 1);
    CHECK(empty.hpMult == 1);
    CHECK(empty.defenseMult == 1);
    CHECK(empty.extraFireDamage == 0);
    CHECK(empty.lootQualityBonus == 0);
    const std::vector<EliteAffixType> fire{EliteAffixType::FireEnhanced};
    const CombinedAffixStats f = CombineAffixes(fire, t);
    CHECK(f.hpMult == doctest::Approx(1.2));
    CHECK(f.extraFireDamage == doctest::Approx(0.3));
    CHECK(f.lootQualityBonus == 5);
    const std::vector<EliteAffixType> es{EliteAffixType::ExtraStrong, EliteAffixType::Swift};
    const CombinedAffixStats a = CombineAffixes(es, t);
    CHECK(a.damageMult == doctest::Approx(1.35));
    CHECK(a.speedMult == doctest::Approx(1.6));
    CHECK(a.hpMult == doctest::Approx(1.3));
    const std::vector<EliteAffixType> fe{EliteAffixType::FireEnhanced, EliteAffixType::ExtraStrong};
    CHECK(CombineAffixes(fe, t).hpMult == doctest::Approx(1.56));
    const std::vector<EliteAffixType> fet{EliteAffixType::FireEnhanced, EliteAffixType::ExtraStrong,
                                          EliteAffixType::Teleporting};
    CHECK(CombineAffixes(fet, t).lootQualityBonus == 18);
    CHECK(CombineAffixes(fet, t).teleporting);
    const std::vector<EliteAffixType> fec{EliteAffixType::FireEnhanced, EliteAffixType::ExtraStrong,
                                          EliteAffixType::CurseAura};
    CHECK(CombineAffixes(fec, t).lootQualityBonus == 19);
    CHECK(CombineAffixes(fec, t).curseAura);
    // Two frozen (and the extra_strong + vampiric vector of combat 24).
    const std::vector<EliteAffixType> ff{EliteAffixType::Frozen, EliteAffixType::Frozen, EliteAffixType::Frozen};
    CHECK(CombineAffixes(ff, t).freezeChance == doctest::Approx(0.5));
    const std::vector<EliteAffixType> ev{EliteAffixType::ExtraStrong, EliteAffixType::Vampiric};
    const CombinedAffixStats v = CombineAffixes(ev, t);
    CHECK(v.damageMult == doctest::Approx(1.485));
    CHECK(v.hpMult == doctest::Approx(1.625));
    CHECK(v.lifestealFraction == doctest::Approx(0.2));
    CHECK(v.lootQualityBonus == 13);
    // All seven stacked.
    const CombinedAffixStats all = CombineAffixes(t.order, t);
    CHECK(all.damageMult > 1);
    CHECK(all.speedMult > 1);
    CHECK(all.hpMult > 1);
    CHECK(all.extraFireDamage > 0);
    CHECK(all.lifestealFraction > 0);
    CHECK(all.freezeChance > 0);
    // Stat application on a 100/20/50/10 monster (EliteAffixSystem.test.ts "Stat Application").
    MonsterDef d;
    d.hp = 100;
    d.damage = 20;
    d.speed = 50;
    d.defense = 10;
    MonsterInstance m = MMake(d, {5, 5});
    const std::vector<EliteAffixType> fes{EliteAffixType::FireEnhanced, EliteAffixType::ExtraStrong,
                                          EliteAffixType::Swift};
    ApplyEliteAffixes(m, fes, t, MAi());
    CHECK(m.maxHp == 156);
    CHECK(m.hp == 156);
    CHECK(m.def.damage == 27);
    CHECK(m.def.speed == 80);
    CHECK(m.def.defense == 11);
    CHECK(m.def.hp == 100);  // the definition hp is untouched (web: only maxHp / hp)
    CHECK(m.originalDef.damage == 20);
    CHECK(m.AffixCount() == 3);
    CHECK(m.AffixLootBonus(t) == 16);
  }

  TEST_CASE("elite on-hit 17.4: extra fire, lifesteal (floors), frozen roll draws only with a freeze chance") {
    MonsterInstance m = MMake(MFix(fixture::Brute()), {5, 5});
    Rng rng(3);
    rng.Script({0.1});
    EliteOnHit none = EvaluateEliteOnHit(m, MAffixes(), 100, rng);
    CHECK(none.extraFireDamage == 0);
    CHECK(none.lifestealHeal == 0);
    CHECK_FALSE(none.freezeSlow);
    CHECK(rng.ScriptedRemaining() == 1);  // no affix: no draw
    rng.ClearScript();
    const std::vector<EliteAffixType> fv{EliteAffixType::FireEnhanced, EliteAffixType::Vampiric};
    ApplyEliteAffixes(m, fv, MAffixes(), MAi());
    rng.Script({0.1});
    EliteOnHit fvh = EvaluateEliteOnHit(m, MAffixes(), 50, rng);
    CHECK(fvh.extraFireDamage == 15);  // 30 % of 50
    CHECK(fvh.lifestealHeal == 10);    // 20 % of 50
    CHECK_FALSE(fvh.freezeSlow);
    CHECK(rng.ScriptedRemaining() == 1);
    CHECK(EvaluateEliteOnHit(m, MAffixes(), 0, rng).extraFireDamage == 0);
    CHECK(EvaluateEliteOnHit(m, MAffixes(), 80, rng).lifestealHeal == 16);
    rng.ClearScript();
    ApplyEliteAffixes(m, MOne(EliteAffixType::Frozen), MAffixes(), MAi());
    rng.Script({0.24, 0.25});
    CHECK(EvaluateEliteOnHit(m, MAffixes(), 10, rng).freezeSlow);        // 0.24 < 0.25
    CHECK_FALSE(EvaluateEliteOnHit(m, MAffixes(), 10, rng).freezeSlow);  // 0.25 is not < 0.25
  }

  TEST_CASE("teleporting 17.4: cooldown (first check immediate, boundary inclusive), window, offsets, clamp") {
    MonsterInstance m = MMake(MFix(fixture::Brute()), {60, 80});
    ApplyEliteAffixes(m, MOne(EliteAffixType::Teleporting), MAffixes(), MAi());
    m.state = MonsterState::Chase;
    Rng rng(5);
    TilePos out;
    // dist^2 = 25 (inside 4 < d^2 < 225): sign -, magnitude 1 + 0.5; sign +, magnitude 1 + 0.
    rng.Script({0.2, 0.5, 0.7, 0.0});
    REQUIRE(EliteTeleportTarget(m, MAffixes(), Vec2(60, 85), 0, 120, 120, rng, out));
    CHECK(out == TilePos{JsRoundInt(60 - 1.5), JsRoundInt(85 + 1.0)});
    // Cooldown 5000: 4999 later no, exactly 5000 yes.
    CHECK_FALSE(EliteTeleportTarget(m, MAffixes(), Vec2(60, 85), 4999, 120, 120, rng, out));
    rng.Script({0.9, 0.0, 0.9, 0.0});
    CHECK(EliteTeleportTarget(m, MAffixes(), Vec2(60, 85), 5000, 120, 120, rng, out));
    CHECK(out == TilePos{61, 86});
    // Outside the window (too close: d^2 = 4 is not > 4; too far: 225) the timer still restarts, no draws.
    rng.ClearScript();
    rng.Script({0.5});
    CHECK_FALSE(EliteTeleportTarget(m, MAffixes(), Vec2(62, 80), 10000, 120, 120, rng, out));
    CHECK_FALSE(EliteTeleportTarget(m, MAffixes(), Vec2(75, 80), 15000, 120, 120, rng, out));
    CHECK(rng.ScriptedRemaining() == 1);
    CHECK_FALSE(EliteTeleportTarget(m, MAffixes(), Vec2(75, 80), 19999, 120, 120, rng, out));
    // Clamp to [1, size - 2].
    MonsterInstance edge = MMake(MFix(fixture::Brute()), {5, 5});
    ApplyEliteAffixes(edge, MOne(EliteAffixType::Teleporting), MAffixes(), MAi());
    rng.ClearScript();
    rng.Script({0.1, 0.99, 0.1, 0.99});
    REQUIRE(EliteTeleportTarget(edge, MAffixes(), Vec2(1.5, 1.5), 0, 120, 120, rng, out));
    CHECK(out == TilePos{1, 1});
    // No teleporting affix: never.
    MonsterInstance plain = MMake(MFix(fixture::Brute()), {60, 80});
    CHECK_FALSE(EliteTeleportTarget(plain, MAffixes(), Vec2(60, 85), 0, 120, 120, rng, out));
  }

  TEST_CASE("curse aura 17.4: radius 4 inclusive, log at most every 2000 ms (strictly more)") {
    MonsterInstance m = MMake(MFix(fixture::Brute()), {60, 80});
    ApplyEliteAffixes(m, MOne(EliteAffixType::CurseAura), MAffixes(), MAi());
    bool logNow = false;
    CHECK(EliteCurseAuraInRange(m, MAffixes(), Vec2(64, 80), 1000, logNow));
    CHECK(logNow);
    CHECK(EliteCurseAuraInRange(m, MAffixes(), Vec2(63, 80), 3000, logNow));
    CHECK_FALSE(logNow);  // exactly 2000 later: not more than 2000
    CHECK(EliteCurseAuraInRange(m, MAffixes(), Vec2(63, 80), 3001, logNow));
    CHECK(logNow);
    CHECK_FALSE(EliteCurseAuraInRange(m, MAffixes(), Vec2(64.01, 80), 9000, logNow));
    CHECK_FALSE(logNow);
    MonsterInstance plain = MMake(MFix(fixture::Brute()), {60, 80});
    CHECK_FALSE(EliteCurseAuraInRange(plain, MAffixes(), Vec2(60, 80), 0, logNow));
  }

  // ===================================================================================================================
  // AI tick (monsters 3; vectors 1-7; MonsterFacing.test.ts)
  // ===================================================================================================================
  TEST_CASE("movement ramp (vector 1): slime from rest, per-tick steps and cumulative distance") {
    MonsterInstance m = MMake(MFix(fixture::Ooze()), {10, 10});
    m.state = MonsterState::Chase;
    m.def.aggroRange = 1000;
    const MonsterWorld w = MOpenWorld(400, 400);
    Rng rng(1);
    double prev = m.pos.x;
    double cum = 0;
    for (int t = 1; t <= 60; ++t) {
      MTick(m, MAi(), Vec2(300, 10), w, rng, t * kSimStepMs);
      const double step = m.pos.x - prev;
      prev = m.pos.x;
      cum += step;
      if (t == 1) CHECK(step == doctest::Approx(0.002).epsilon(1e-6));
      if (t == 2) {
        CHECK(step == doctest::Approx(0.0038).epsilon(1e-6));
        CHECK(cum == doctest::Approx(0.0058).epsilon(1e-6));
      }
      if (t == 10) {
        CHECK(step == doctest::Approx(0.013026).epsilon(1e-5));
        CHECK(cum == doctest::Approx(0.082762).epsilon(1e-5));
      }
      if (t == 30) {
        CHECK(step == doctest::Approx(0.019152).epsilon(1e-5));
        CHECK(cum == doctest::Approx(0.427630).epsilon(1e-5));
      }
      if (t == 60) {
        CHECK(step == doctest::Approx(0.019964).epsilon(1e-5));
        CHECK(cum == doctest::Approx(1.020323).epsilon(1e-5));
      }
    }
    CHECK(m.pos.y == 10);
    CHECK(m.heading == Vec2(1, 0));
    CHECK(m.moveSpeed < MonsterTopSpeed(m.def, MAi()));
    // A slow halves the steady-state speed (movement only).
    MonsterInstance s = MMake(MFix(fixture::Ooze()), {10, 10});
    s.state = MonsterState::Chase;
    s.def.aggroRange = 1000;
    for (int t = 1; t <= 600; ++t) MTick(s, MAi(), Vec2(300, 10), w, rng, t * kSimStepMs, true, 0.5);
    CHECK(s.moveSpeed == doctest::Approx(0.6).epsilon(1e-6));
  }

  TEST_CASE("aggro edge (vector 2), chase / attack hysteresis (vector 3), chase drop (vector 4), facing") {
    const MonsterWorld w = MOpenWorld();
    Rng rng(1);
    MonsterInstance g = MMake(MFix(fixture::Grunt()), {10, 10});
    MonsterAiResult r = MTick(g, MAi(), Vec2(15, 10), w, rng, 100);  // dP = 5 = aggroRange
    CHECK(g.state == MonsterState::Chase);
    CHECK(r.stateChanged);
    CHECK(g.pos == Vec2(10, 10));  // no move on the transition tick
    MonsterInstance g2 = MMake(MFix(fixture::Grunt()), {10, 10});
    MTick(g2, MAi(), Vec2(15.01, 10), w, rng, 100);
    CHECK(g2.state == MonsterState::Idle);

    // Chase -> attack at <= 1.5 (no move), stays until > 1.8 (1.5 * 1.2 = 1.7999999999999998).
    MonsterInstance c = MMake(MFix(fixture::Grunt()), {10, 10});
    c.state = MonsterState::Chase;
    MTick(c, MAi(), Vec2(11.5, 10), w, rng, 100);
    CHECK(c.state == MonsterState::Attack);
    CHECK(c.pos == Vec2(10, 10));
    MTick(c, MAi(), Vec2(11.79, 10), w, rng, 117);
    CHECK(c.state == MonsterState::Attack);
    MTick(c, MAi(), Vec2(10, 8.19), w, rng, 133);  // 1.81 away, up-screen
    CHECK(c.state == MonsterState::Chase);
    CHECK(c.pos == Vec2(10, 10));               // no move on the exit tick
    CHECK(c.heading.y == doctest::Approx(-1));  // still squared up to the hero on the exit tick

    // Chase drop at 1.5 x aggro: 7.5 keeps chasing (and moves), 7.5001 -> idle with speed 0.
    MonsterInstance d = MMake(MFix(fixture::Grunt()), {10, 10});
    d.state = MonsterState::Chase;
    MTick(d, MAi(), Vec2(17.5, 10), w, rng, 100);
    CHECK(d.state == MonsterState::Chase);
    CHECK(d.pos.x > 10);
    CHECK(d.moveSpeed > 0);
    MonsterInstance e = MMake(MFix(fixture::Grunt()), {10, 10});
    e.state = MonsterState::Chase;
    e.moveSpeed = 1;
    MTick(e, MAi(), Vec2(17.5001, 10), w, rng, 100);
    CHECK(e.state == MonsterState::Idle);
    CHECK(e.moveSpeed == 0);
    CHECK(e.pos == Vec2(10, 10));

    // MonsterFacing.test.ts: chase faces the hero (up / down), attack keeps facing it.
    MonsterInstance up = MMake(MFix(fixture::Grunt()), {10, 10});
    up.state = MonsterState::Chase;
    up.def.aggroRange = 20;
    MTick(up, MAi(), Vec2(10, 5), w, rng, 100);
    CHECK(up.heading.y < 0);
    MTick(up, MAi(), Vec2(10, 15), w, rng, 117);
    CHECK(up.heading.y > 0);
    MonsterInstance at = MMake(MFix(fixture::Grunt()), {10, 10});
    at.state = MonsterState::Attack;
    MTick(at, MAi(), Vec2(9, 10), w, rng, 100);
    CHECK(at.state == MonsterState::Attack);
    CHECK(at.heading == Vec2(-1, 0));
  }

  TEST_CASE("leash, web parity mode (vector 5): one-tick leash pins the monster, heals 1 % per leash tick") {
    MonsterAiDef ai = MAi();
    ai.leashMode = LeashMode::WebParity;
    const MonsterWorld w = MOpenWorld();
    Rng rng(1);
    MonsterInstance g = MMake(MFix(fixture::Grunt()), {18.5, 10});
    g.spawnAnchor = TilePos{10, 10};
    g.state = MonsterState::Chase;
    g.hp = 40;
    MonsterAiResult r = MTick(g, ai, Vec2(20, 10), w, rng, 100);
    CHECK(r.leashed);
    CHECK(g.state == MonsterState::Idle);
    CHECK(g.pos.x == doctest::Approx(18.49725).epsilon(1e-9));
    CHECK(g.hp == doctest::Approx(40.55));
    MTick(g, ai, Vec2(20, 10), w, rng, 117);
    CHECK(g.state == MonsterState::Chase);
    CHECK(g.pos.x == doctest::Approx(18.49725).epsilon(1e-9));  // no move
    MTick(g, ai, Vec2(20, 10), w, rng, 133);
    CHECK(g.state == MonsterState::Idle);
    CHECK(g.hp == doctest::Approx(41.10));
    bool attacked = false;
    for (int t = 0; t < 600; ++t) {
      MTick(g, ai, Vec2(20, 10), w, rng, 150 + t * kSimStepMs);
      attacked = attacked || g.state == MonsterState::Attack;
    }
    CHECK_FALSE(attacked);
    // Pinned near the ring: it creeps home by one 10 % step per leash tick (every other tick), ~5 % speed.
    CHECK(g.pos.x > 17.5);
    CHECK(g.pos.x < 18.5);
  }

  TEST_CASE("leash M1 (vector 6): Returning walks home with the ramp, heals 0.6 maxHp / s, ignores the hero") {
    const MonsterAiDef& ai = MAi();
    REQUIRE(ai.leashMode == LeashMode::Returning);
    const MonsterWorld w = MOpenWorld();
    Rng rng(1);
    MonsterInstance g = MMake(MFix(fixture::Grunt()), {18.5, 10});
    g.spawnAnchor = TilePos{10, 10};
    g.state = MonsterState::Chase;
    g.hp = 20;
    g.provokedUntilMs = 99999;
    MonsterAiResult r = MTick(g, ai, Vec2(20, 10), w, rng, 100);
    CHECK(r.leashed);
    CHECK(g.state == MonsterState::Returning);
    CHECK_FALSE(g.IsAggro());
    CHECK(g.provokedUntilMs == 0);
    CHECK(g.pos.x == doctest::Approx(18.5 - 0.00275).epsilon(1e-9));  // first ramp step, same tick
    CHECK(g.hp == doctest::Approx(20 + 33.0 / 60.0));
    // Provokes are ignored while returning (M1 + M2 contract).
    CHECK_FALSE(ProvokeMonster(g, ai, 200, 1.0));
    int ticks = 1;
    bool everAggro = false;
    while (g.state == MonsterState::Returning && ticks < 2000) {
      MTick(g, ai, Vec2(g.pos.x + 1, 10), w, rng, 100 + ticks * kSimStepMs);  // hero right next to it
      everAggro = everAggro || g.IsAggro();
      ++ticks;
    }
    CHECK_FALSE(everAggro);
    CHECK(g.state == MonsterState::Idle);
    CHECK(g.moveSpeed == 0);
    CHECK(Dist(g.pos, Vec2(10, 10)) <= ai.returnHomeRadius + 1e-9);
    CHECK(g.hp == 55);  // healed (capped) on the way
    // ~6.5 tiles at 1.65 tiles/s after the ramp: about 4 s.
    CHECK(ticks > 200);
    CHECK(ticks < 300);
    // Within aggro range of the hero again at home: the next tick aggroes normally.
    MTick(g, ai, Vec2(13, 10), w, rng, 9000);
    CHECK(g.state == MonsterState::Chase);
  }

  TEST_CASE("patrol (vector 7): tick 181 with draws (-2, +1); blocked target stays idle; M10 timeout; arrival") {
    const MonsterWorld w = MOpenWorld();
    Rng rng(1);
    MonsterInstance g = MMake(MFix(fixture::Grunt()), {20, 20});
    rng.Script({0.0, 0.7});  // randomInt(-2, 2): floor(0 * 5) - 2 = -2; floor(0.7 * 5) - 2 = 1
    for (int t = 1; t <= 180; ++t) MTick(g, MAi(), Vec2(90, 90), w, rng, t * kSimStepMs);
    CHECK(g.state == MonsterState::Idle);
    CHECK(g.patrolTimerMs == doctest::Approx(3000));
    CHECK(g.patrolTimerMs <= 3000);  // 2999.999999999995 after 180 additions
    MTick(g, MAi(), Vec2(90, 90), w, rng, 181 * kSimStepMs);
    CHECK(g.state == MonsterState::Patrol);
    REQUIRE(g.hasPatrolTarget);
    CHECK(g.patrolTarget == TilePos{18, 21});
    CHECK(g.patrolTimerMs == 0);
    CHECK(rng.ScriptedRemaining() == 0);
    // It walks there and goes idle on arrival (< 0.1), speed 0.
    int t = 182;
    while (g.state == MonsterState::Patrol && t < 600) MTick(g, MAi(), Vec2(90, 90), w, rng, (t++) * kSimStepMs);
    CHECK(g.state == MonsterState::Idle);
    CHECK(g.moveSpeed == 0);
    CHECK(Dist(g.pos, Vec2(18, 21)) < MAi().arriveEpsilon);

    // Blocked patrol tile: stays idle, timer reset to 0 (next try in 3000 ms).
    auto grid = std::make_shared<MTestGrid>(40, 40);
    grid->Block(18, 21);
    const MonsterWorld bw = MGridWorld(grid, false);
    MonsterInstance b = MMake(MFix(fixture::Grunt()), {20, 20});
    b.patrolTimerMs = 3000;
    rng.Script({0.0, 0.7});
    MTick(b, MAi(), Vec2(90, 90), bw, rng, 1000);
    CHECK(b.state == MonsterState::Idle);
    CHECK(b.patrolTimerMs == 0);

    // M10: a target that can never be reached (walled in) gives up after 4000 ms.
    auto box = std::make_shared<MTestGrid>(40, 40);
    for (int32_t c = 0; c < 40; ++c) {
      for (int32_t r = 0; r < 40; ++r) {
        if (!((c == 20 && r == 20) || (c == 18 && r == 21))) box->Block(c, r);
      }
    }
    const MonsterWorld boxw = MGridWorld(box, false);
    MonsterInstance s = MMake(MFix(fixture::Grunt()), {20, 20});
    s.patrolTimerMs = 3000;
    rng.Script({0.0, 0.7});
    MTick(s, MAi(), Vec2(90, 90), boxw, rng, 1000);
    REQUIRE(s.state == MonsterState::Patrol);
    int n = 0;
    while (s.state == MonsterState::Patrol && n < 1000) {
      ++n;
      MTick(s, MAi(), Vec2(90, 90), boxw, rng, 1000 + n * kSimStepMs);
    }
    CHECK(s.state == MonsterState::Idle);
    CHECK(RoundToTile(s.pos) == TilePos{20, 20});  // it only shuffled inside its own tile
    CHECK(n * kSimStepMs >= MAi().patrolTimeoutMs - 1e-6);
    CHECK(n * kSimStepMs < MAi().patrolTimeoutMs + 2 * kSimStepMs);
    // The patrol timer keeps its value across a chase: a monster that drops aggro after > 3 s retries at once.
    MonsterInstance k = MMake(MFix(fixture::Grunt()), {20, 20});
    k.patrolTimerMs = 3500;
    k.state = MonsterState::Chase;
    rng.Script({0.5, 0.5});
    MTick(k, MAi(), Vec2(90, 90), w, rng, 1000);  // chase drop -> idle
    CHECK(k.state == MonsterState::Idle);
    MTick(k, MAi(), Vec2(90, 90), w, rng, 1017);
    CHECK(k.state == MonsterState::Patrol);
    CHECK(k.patrolTarget == TilePos{20, 20});
  }

  TEST_CASE("M2 provoke contract: idle / patrol, or chase beyond aggro; Returning ignores it") {
    const MonsterAiDef& ai = MAi();
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
    m.state = MonsterState::Chase;
    CHECK(ProvokeMonster(m, ai, 2000, 6.0));
    CHECK(m.provokedUntilMs == 2000 + ai.provokeDurationMs);
    CHECK_FALSE(ProvokeMonster(m, ai, 3000, 3.0));
    CHECK(m.provokedUntilMs == 2000 + ai.provokeDurationMs);
    for (MonsterState s : {MonsterState::Returning, MonsterState::Attack, MonsterState::Dead}) {
      m.state = s;
      m.provokedUntilMs = 0;
      CHECK_FALSE(ProvokeMonster(m, ai, 4000, 10.0));
      CHECK(m.state == s);
      CHECK(m.provokedUntilMs == 0);
    }
    CHECK(ai.provokeDurationMs == 5000);
  }

  TEST_CASE("M2 provoked chase ignores the 1.5x aggro drop for 5 s (leash still applies)") {
    const MonsterWorld w = MOpenWorld();
    Rng rng(1);
    MonsterInstance s = MMake(MFix(fixture::Ooze()), {20, 20});  // aggro 4: drops at 6
    REQUIRE(ProvokeMonster(s, MAi(), 0, 7.0));
    MTick(s, MAi(), Vec2(27, 20), w, rng, 100);
    CHECK(s.state == MonsterState::Chase);
    CHECK(s.pos.x > 20);
    MTick(s, MAi(), Vec2(27, 20), w, rng, 4999);
    CHECK(s.state == MonsterState::Chase);
    MTick(s, MAi(), Vec2(27, 20), w, rng, 5000);  // provoke over
    CHECK(s.state == MonsterState::Idle);
  }

  TEST_CASE("M6: A* waypoints around a wall, wall sliding, separation keeps attackers in range") {
    // A wall at col 10 (rows 0..15) between a goblin at (5,5) and the hero at (15,5).
    auto grid = std::make_shared<MTestGrid>(30, 30);
    for (int32_t r = 0; r <= 15; ++r) grid->Block(10, r);
    Rng rng(1);
    MonsterInstance g = MMake(MFix(fixture::Grunt()), {5, 5});
    g.state = MonsterState::Chase;
    g.def.aggroRange = 30;
    g.spawnAnchor = TilePos{10, 10};  // the whole detour stays inside the 8-tile leash ring
    const MonsterWorld withPath = MGridWorld(grid, true);
    int t = 0;
    while (g.state != MonsterState::Attack && t < 3000) {
      ++t;
      MTick(g, MAi(), Vec2(15, 5), withPath, rng, t * kSimStepMs);
      CHECK(grid->Walk(JsRoundInt(g.pos.x), JsRoundInt(g.pos.y)));
    }
    CHECK(g.state == MonsterState::Attack);
    CHECK(Dist(g.pos, Vec2(15, 5)) <= g.def.attackRange);
    CHECK(g.path.empty());
    // Without the path service the straight chase is blocked by the wall (sliding has no free component here).
    MonsterInstance stuck = MMake(MFix(fixture::Grunt()), {5, 5});
    stuck.state = MonsterState::Chase;
    stuck.def.aggroRange = 30;
    stuck.spawnAnchor = TilePos{10, 10};
    const MonsterWorld noPath = MGridWorld(grid, false);
    for (int i = 1; i <= 600; ++i) MTick(stuck, MAi(), Vec2(15, 5), noPath, rng, i * kSimStepMs);
    CHECK(stuck.state == MonsterState::Chase);
    CHECK(stuck.pos.x < 10);
    CHECK(stuck.heading == Vec2(1, 0));  // walks in place toward the hero

    // Wall sliding: a diagonal step into a wall column keeps its row component.
    auto wall = std::make_shared<MTestGrid>(30, 30);
    for (int32_t r = 0; r < 30; ++r) wall->Block(11, r);
    MonsterInstance sl = MMake(MFix(fixture::Grunt()), {10.49, 10});
    sl.moveSpeed = MonsterTopSpeed(sl.def, MAi());
    CHECK_FALSE(MonsterMoveToward(sl, MAi(), Vec2(20, 20), kSimStepMs, 1, MGridWorld(wall, false)));
    CHECK(sl.pos.x == 10.49);
    CHECK(sl.pos.y > 10);

    // Separation: two goblins stacked on one tile drift apart; an attacker never leaves its attack range.
    const MonsterWorld open = MOpenWorld();
    MonsterInstance a = MMake(MFix(fixture::Grunt()), {20, 20}, 201);
    MonsterInstance b = MMake(MFix(fixture::Grunt()), {20, 20}, 202);
    for (int i = 1; i <= 120; ++i) {
      const std::vector<Vec2> na{b.pos};
      MTick(a, MAi(), Vec2(90, 90), open, rng, i * kSimStepMs, true, 1, na);
      const std::vector<Vec2> nb{a.pos};
      MTick(b, MAi(), Vec2(90, 90), open, rng, i * kSimStepMs, true, 1, nb);
    }
    CHECK(Dist(a.pos, b.pos) >= MAi().separationRadius - 0.05);
    MonsterInstance att = MMake(MFix(fixture::Grunt()), {21.5, 20}, 203);
    att.state = MonsterState::Attack;
    const std::vector<Vec2> pushFromHeroSide{Vec2(21.2, 20)};
    for (int i = 1; i <= 60; ++i) {
      MTick(att, MAi(), Vec2(20, 20), open, rng, i * kSimStepMs, true, 1, pushFromHeroSide);
      CHECK(Dist(att.pos, Vec2(20, 20)) <= att.def.attackRange + 1e-9);
    }
    CHECK(att.state == MonsterState::Attack);
  }

  // ===================================================================================================================
  // MonsterSystem runtime (monsters 3.8, 5-9, 11)
  // ===================================================================================================================
  TEST_CASE("MonsterSystem constructs and is empty before a zone") {
    test::SimHarness h;
    MonsterSystem m(h.ctx);
    CHECK(m.All().empty());
    CHECK(m.MiniBoss() == kNoEntity);
    CHECK_FALSE(m.AnyAttacking());
    std::vector<EntityId> none;
    m.QueryAlive({10, 10}, 5, none);
    CHECK(none.empty());
    CHECK(m.NearestAlive({10, 10}, 50) == kNoEntity);
    CHECK(m.NearestAliveOfDef(fixture::Grunt().id, {0, 0}) == kNoEntity);
    CHECK(m.NearestAggro({0, 0}) == kNoEntity);
    m.TickAI(kSimStepMs);  // no hero, no zone: nothing happens
    MonsterInstance inst;
    inst.affixes.push_back({EliteAffixType::Swift, 0, 0});
    CHECK(inst.AffixLootBonus(h.ctx.data.Combat().eliteAffixes) ==
          h.ctx.data.Combat().eliteAffixes.Def(EliteAffixType::Swift).lootQualityBonus);
  }

  TEST_CASE("activity set 3.8 (AI culling): 30 tiles inclusive, far aggro monsters stay active, 250 ms refresh") {
    MonsterRig rig(4, Vec2(50, 50));
    rig.OpenWorld();
    const EntityId edge = rig.Spawn(fixture::Grunt(), {80, 50});    // exactly 30
    const EntityId beyond = rig.Spawn(fixture::Grunt(), {81, 50});  // 31
    const EntityId farAggro = rig.Spawn(fixture::Grunt(), {50, 110});
    rig.M(farAggro).state = MonsterState::Chase;
    rig.Step();
    CHECK(rig.M(edge).patrolTimerMs == doctest::Approx(kSimStepMs));
    CHECK(rig.M(beyond).patrolTimerMs == 0);        // culled: frozen
    CHECK(rig.M(farAggro).state == MonsterState::Idle);  // updated (chase dropped: 60 > 7.5)
    CHECK(rig.M(edge).active);
    CHECK_FALSE(rig.M(beyond).active);
    // A spawn between refreshes joins at the next refresh (<= 250 ms).
    const EntityId late = rig.Spawn(fixture::Grunt(), {55, 50});
    rig.Step();
    CHECK(rig.M(late).patrolTimerMs == 0);
    rig.Step(15);  // 17 steps in total: past the 250 ms refresh
    CHECK(rig.M(late).patrolTimerMs > 0);
    // The hero walks closer: the culled monster wakes at the next refresh.
    rig.hero->SetPosition(Vec2(60, 50));
    rig.Step(16);
    CHECK(rig.M(beyond).patrolTimerMs > 0);
  }

  TEST_CASE("safe zones (vector 8): hidden hero, forced idle inside a camp radius, no aggro message spam") {
    MonsterRig rig(4, Vec2(15, 20));  // camp (15,15), radius 9: the hero is inside
    rig.OpenWorld();
    const EntityId idle = rig.Spawn(fixture::Grunt(), {15, 25});
    rig.M(idle).pos = Vec2(15, 24.5);  // 4.5 from the hero (< aggro 5) but outside the camp
    rig.Step();
    CHECK(rig.M(idle).state == MonsterState::Idle);  // the hero in camp is invisible to non-aggro monsters
    const EntityId chaser = rig.Spawn(fixture::Grunt(), {15, 24});
    rig.M(chaser).pos = Vec2(15, 23.9);  // inside the radius (8.9)
    rig.M(chaser).state = MonsterState::Chase;
    rig.Step(16);  // refresh the activity set
    CHECK(rig.M(chaser).state == MonsterState::Idle);
    // Hero outside the camp but within aggro of a monster standing inside: it flips idle -> chase -> repelled and
    // never moves, and the aggro message is not repeated every tick.
    MonsterRig edge(4, Vec2(15, 28));
    edge.OpenWorld();
    const EntityId inside = edge.Spawn(fixture::Grunt(), {15, 23});
    edge.Step();
    CHECK(edge.M(inside).state == MonsterState::Chase);
    const Vec2 p = edge.M(inside).pos;
    edge.Step(30);
    CHECK(edge.M(inside).pos == p);
    CHECK(edge.aggros.size() == 1);
  }

  TEST_CASE("immobilized monsters skip the AI; slow scales movement; idle -> chase publishes MonsterAggroMsg") {
    MonsterRig rig(4, Vec2(60, 80));
    rig.OpenWorld();
    const EntityId g = rig.Spawn(fixture::Grunt(), {60, 84});
    rig.status.Apply(g, StatusType::Freeze, 1, 2000, kNoEntity, rig.h.clock.NowMs());
    REQUIRE(rig.status.IsImmobilized(g));
    rig.Step(30);
    CHECK(rig.M(g).state == MonsterState::Idle);
    CHECK(rig.aggros.empty());
    rig.status.ClearEntity(g);
    rig.Step();
    CHECK(rig.M(g).state == MonsterState::Chase);
    REQUIRE(rig.aggros.size() == 1);
    CHECK(rig.aggros[0].monster == g);
    CHECK(rig.aggros[0].defId == fixture::Grunt().id);
    CHECK(rig.aggros[0].previous == MonsterState::Idle);
    rig.status.Apply(g, StatusType::Slow, 50, 60000, kNoEntity, rig.h.clock.NowMs());
    rig.hero->SetPosition(Vec2(60, 90));  // keep it chasing
    rig.Step(120);
    CHECK(rig.M(g).moveSpeed < MonsterTopSpeed(rig.M(g).def, MAi()) * 0.5 + 1e-6);
    CHECK(rig.M(g).groundSpeed > 0);
  }

  TEST_CASE("damage 5: weight, single kill message with every field, provoke M2, heal clamps, dead ignores hits") {
    MonsterRig rig(8, Vec2(60, 80));
    rig.OpenWorld();
    const EntityId g = rig.Spawn(fixture::Brute(), {60, 90}, MonsterRole::Regular, true);  // 10 tiles: idle
    const double maxHp = rig.M(g).maxHp;
    DamageFlags f;
    f.hasFrom = true;
    f.from = Vec2(60, 80);
    f.attacker = kHeroEntityId;
    f.source = KillSource::HeroSkill;
    const HitWeight w = rig.monsters.ApplyDamage(g, 1, f);
    CHECK(w == ClassifyHit(MD().Combat().hitFeedback, 1, maxHp, false, false, false));
    CHECK(rig.M(g).hp == maxHp - 1);
    CHECK(rig.M(g).lastDamagedMs == rig.h.clock.NowMs());
    CHECK(rig.M(g).hasLastHitFrom);
    // M2: provoked from beyond aggro range -> chase + 5 s + aggro message.
    CHECK(rig.M(g).state == MonsterState::Chase);
    CHECK(rig.M(g).provokedUntilMs == rig.h.clock.NowMs() + MAi().provokeDurationMs);
    REQUIRE(rig.aggros.size() == 1);
    rig.monsters.Heal(g, 1000);
    CHECK(rig.M(g).hp == maxHp);
    rig.monsters.Heal(g, -5);
    CHECK(rig.M(g).hp == maxHp);
    DamageFlags tick;
    tick.isTick = true;
    tick.provokes = false;
    CHECK(rig.monsters.ApplyDamage(g, 2, tick) == HitWeight::Tick);
    // Kill: exactly one message, every field filled; later hits return Tick and publish nothing.
    f.source = KillSource::HeroBasic;
    f.isCrit = true;
    CHECK(rig.monsters.ApplyDamage(g, 1e6, f) == HitWeight::Kill);
    REQUIRE(rig.kills.size() == 1);
    const MonsterKilledMsg& k = rig.kills[0];
    CHECK(k.monster == g);
    CHECK(k.defId == fixture::Brute().id);
    CHECK(k.level == 5);
    CHECK(k.elite);
    CHECK_FALSE(k.isMiniBoss);
    CHECK(k.eliteAffixCount == 1);
    CHECK(k.affixLootBonus > 0);
    CHECK(k.expReward == 55);
    CHECK(k.goldMin == 8);
    CHECK(k.goldMax == 15);
    CHECK(k.pos == Vec2(60, 90));
    CHECK(k.role == MonsterRole::Regular);
    CHECK(k.killer == kHeroEntityId);
    CHECK(k.source == KillSource::HeroBasic);
    CHECK(k.killedByEliteBasic);
    CHECK(rig.M(g).state == MonsterState::Dead);
    CHECK(rig.M(g).hp == 0);
    CHECK_FALSE(rig.M(g).IsAlive());
    CHECK(rig.monsters.ApplyDamage(g, 5, f) == HitWeight::Tick);
    CHECK(rig.kills.size() == 1);
    rig.monsters.Heal(g, 10);
    CHECK(rig.M(g).hp == 0);
    bool deathAnim = false;
    for (const EvPlayAnim& a : MEvents<EvPlayAnim>(rig.h.events)) {
      if (a.entity == g && a.action == AnimAction::Death) {
        deathAnim = true;
        CHECK(a.durationMs == MD().Combat().anim.Preset(AnimRig::Humanoid).deathDuration);
      }
    }
    CHECK(deathAnim);
    CHECK(MHasLog(rig.h.events, "zone.monsterKill"));
    // Damage on unknown ids is ignored.
    CHECK(rig.monsters.ApplyDamage(999999, 5, f) == HitWeight::Tick);
  }

  TEST_CASE("respawn 7 (vector 15): 15 s, original anchor (M3), fresh instance, elites re-roll; no-respawn roles") {
    MonsterRig rig(12, Vec2(60, 60));
    rig.OpenWorld();
    const EntityId g = rig.Spawn(fixture::Grunt(), {70, 80});
    const EntityId chief = rig.Spawn(fixture::Brute(), {72, 80}, MonsterRole::Regular, true);
    rig.M(g).pos = Vec2(71.3, 80.4);  // wandered off: the anchor stays (70,80)
    const size_t gIdx = 0;
    REQUIRE(rig.monsters.All()[gIdx].id == g);
    rig.monsters.ApplyDamage(g, 1e6, DamageFlags{});
    rig.monsters.ApplyDamage(chief, 1e6, DamageFlags{});
    REQUIRE(rig.kills.size() == 2);
    const double killedAt = rig.h.clock.NowMs();
    // Not before 15 000 ms.
    while (rig.h.clock.NowMs() + kSimStepMs < killedAt + MAi().respawnDelayMs - 1e-6) rig.h.Step(1);
    CHECK(rig.monsters.All()[gIdx].id == g);
    rig.h.events.Clear();
    rig.h.Step(2);
    const MonsterInstance& fresh = rig.monsters.All()[gIdx];  // replaced in place
    CHECK(fresh.id != g);
    CHECK(fresh.IsAlive());
    CHECK(fresh.state == MonsterState::Idle);
    CHECK(fresh.hp == 55);
    CHECK(fresh.lastAttackMs == 0);
    CHECK(fresh.spawnAnchor == TilePos{70, 80});
    CHECK(std::abs(JsRoundInt(fresh.pos.x) - 70) <= 2);
    CHECK(std::abs(JsRoundInt(fresh.pos.y) - 80) <= 2);
    const MonsterInstance& chief2 = rig.monsters.All()[1];
    CHECK(chief2.id != chief);
    CHECK(chief2.AffixCount() == 1);              // re-rolled
    CHECK(chief2.originalDef.hp == 160);           // affixes never compound
    CHECK(chief2.maxHp <= 160 * 1.3 + 1e-9);
    size_t despawned = 0;
    for (const EvEntityDespawned& d : MEvents<EvEntityDespawned>(rig.h.events)) {
      CHECK(d.reason == DespawnReason::Died);
      ++despawned;
    }
    CHECK(despawned == 2);
    CHECK(MEvents<EvEntitySpawned>(rig.h.events).size() == 2);
    // Kill the respawn: the anchor still does not drift.
    const EntityId again = fresh.id;
    rig.monsters.ApplyDamage(again, 1e6, DamageFlags{});
    rig.h.Step(static_cast<int>(MAi().respawnDelayMs / kSimStepMs) + 2);
    CHECK(rig.monsters.All()[gIdx].spawnAnchor == TilePos{70, 80});
    CHECK(rig.monsters.All()[gIdx].IsAlive());

    // Roles that never respawn (M4 / M7 / W9): no timer.
    for (MonsterRole role : {MonsterRole::ZoneMiniBoss, MonsterRole::HuntLeader, MonsterRole::HuntMinion,
                             MonsterRole::AmbushSpawn, MonsterRole::RescueSpawn, MonsterRole::DefendWave,
                             MonsterRole::StoryBoss, MonsterRole::LabyrinthFloor, MonsterRole::SealKeeper}) {
      CAPTURE(EnumName(role));
      MonsterRig r2(13, Vec2(60, 60));
      r2.OpenWorld();
      const EntityId id = r2.Spawn(fixture::Grunt(), {70, 70}, role);
      CHECK(r2.M(id).noRespawn);
      r2.monsters.ApplyDamage(id, 1e6, DamageFlags{});
      CHECK(r2.h.timers.Empty());
      r2.h.Step(static_cast<int>(MAi().respawnDelayMs / kSimStepMs) + 5);
      CHECK_FALSE(r2.monsters.Find(id)->IsAlive());
      CHECK(r2.monsters.All().size() == 1);
    }
    // The zone mini-boss reference is cleared on its kill.
    MonsterRig r3(14, Vec2(60, 40));
    r3.OpenWorld();
    r3.monsters.SpawnMiniBoss();
    const EntityId mb = r3.monsters.MiniBoss();
    REQUIRE(mb != kNoEntity);
    r3.monsters.ApplyDamage(mb, 1e6, DamageFlags{});
    CHECK(r3.monsters.MiniBoss() == kNoEntity);
    CHECK(r3.h.timers.Empty());
    // Zone exit drops pending respawns.
    MonsterRig r4(15, Vec2(60, 60));
    r4.OpenWorld();
    const EntityId z = r4.Spawn(fixture::Grunt(), {70, 70});
    r4.monsters.ApplyDamage(z, 1e6, DamageFlags{});
    CHECK_FALSE(r4.h.timers.Empty());
    r4.monsters.OnZoneExit();
    CHECK(r4.h.timers.Empty());
    CHECK(MEvents<EvEntityDespawned>(r4.h.events).back().reason == DespawnReason::ZoneUnload);
  }

  TEST_CASE("elite behaviours 17.4 in the system: teleport blink next to the hero, curse aura buff + log") {
    MonsterRig rig(41, Vec2(60, 80));
    rig.OpenWorld();
    const EntityId t = rig.Spawn(fixture::Brute(), {60, 85});
    ApplyEliteAffixes(rig.M(t), MOne(EliteAffixType::Teleporting), MAffixes(), MAi());
    rig.M(t).state = MonsterState::Chase;
    rig.h.rng.Get(RngStream::Ai).Script({0.2, 0.5, 0.7, 0.0});
    rig.monsters.TickEliteBehaviours();
    CHECK(rig.M(t).pos == Vec2(59, 81));
    const std::vector<EvEntityTeleported> tp = MEvents<EvEntityTeleported>(rig.h.events);
    REQUIRE(tp.size() == 1);
    CHECK(tp[0].reason == TeleportReason::EliteBlink);
    CHECK(tp[0].from == Vec2(60, 85));
    CHECK(rig.monsters.MonsterAtTile(Vec2(59, 81)) == t);
    // Not aggro: no blink.
    MonsterRig idle(42, Vec2(60, 80));
    idle.OpenWorld();
    const EntityId it = idle.Spawn(fixture::Brute(), {60, 85});
    ApplyEliteAffixes(idle.M(it), MOne(EliteAffixType::Teleporting), MAffixes(), MAi());
    idle.monsters.TickEliteBehaviours();
    CHECK(idle.M(it).pos == Vec2(60, 85));

    MonsterRig c(43, Vec2(60, 80));
    c.OpenWorld();
    const EntityId cu = c.Spawn(fixture::Brute(), {60, 83});
    ApplyEliteAffixes(c.M(cu), MOne(EliteAffixType::CurseAura), MAffixes(), MAi());
    c.h.Step(1);
    c.monsters.TickEliteBehaviours();
    const ActiveBuff* b = c.hero->Buffs().FindTag(BuffTag::CurseAura);
    REQUIRE(b != nullptr);
    CHECK(b->stat == BuffStat::DamageAmplify);
    CHECK(b->value == doctest::Approx(0.15));
    CHECK(b->durationMs == 2000);
    CHECK(MHasLog(c.h.events, "zone.combat.curseAura"));
    c.h.events.Clear();
    c.h.Step(1);
    c.monsters.TickEliteBehaviours();
    CHECK(c.hero->Buffs().Items().size() == 1);  // refreshed, not stacked
    CHECK(c.hero->Buffs().FindTag(BuffTag::CurseAura)->startMs == c.h.clock.NowMs());
    CHECK_FALSE(MHasLog(c.h.events, "zone.combat.curseAura"));
  }

  TEST_CASE("queries: nearest alive / aggro, monster at tile, candidates, attacking, snapshot views") {
    MonsterRig rig(51, Vec2(60, 60));
    rig.OpenWorld();
    const EntityId a = rig.Spawn(fixture::Grunt(), {62, 60});
    const EntityId b = rig.Spawn(fixture::Ooze(), {70, 60});
    const EntityId c = rig.Spawn(fixture::Grunt(), {100, 100});
    rig.M(c).state = MonsterState::Chase;
    CHECK(rig.monsters.NearestAlive(Vec2(60, 60), 50) == a);
    CHECK(rig.monsters.NearestAggro(Vec2(60, 60)) == c);
    CHECK(rig.monsters.MonsterAtTile(Vec2(62.4, 61.4)) == a);
    CHECK(rig.monsters.MonsterAtTile(Vec2(63.6, 60)) == kNoEntity);  // |dcol| = 1.6
    CHECK(rig.monsters.NearestAliveOfDef(fixture::Ooze().id, Vec2(0, 0)) == b);
    std::vector<EntityId> alive;
    rig.monsters.QueryAlive(Vec2(60, 60), 10, alive);
    CHECK(alive == std::vector<EntityId>{a, b});
    CHECK_FALSE(rig.monsters.AnyAttacking());
    rig.M(a).state = MonsterState::Attack;
    CHECK(rig.monsters.AnyAttacking());
    rig.monsters.ApplyDamage(a, 1e6, DamageFlags{});
    CHECK(rig.monsters.NearestAlive(Vec2(60, 60), 50) == b);
    CHECK(rig.monsters.MonsterAtTile(Vec2(62, 60)) == kNoEntity);
    rig.monsters.QueryAlive(Vec2(60, 60), 10, alive);
    CHECK(alive == std::vector<EntityId>{b});
    std::vector<TargetCandidate> cands;
    rig.monsters.Candidates(cands);
    REQUIRE(cands.size() == 3);
    CHECK_FALSE(cands[0].alive);
    CHECK(cands[1].alive);
    // Snapshot.
    Snapshot snap;
    rig.monsters.FillSnapshot(snap);
    REQUIRE(snap.monsters.size() == 3);
    CHECK(snap.monsters[1].id == b);
    CHECK(snap.monsters[1].defId == fixture::Ooze().id);
    CHECK(snap.monsters[1].artId == MFix(fixture::Ooze()).spriteKey);
    CHECK(snap.monsters[1].nameKey == "data.monster." + fixture::Ooze().id);
    CHECK(snap.monsters[1].alive);
    CHECK(snap.monsters[1].hp == 30);
    CHECK_FALSE(snap.monsters[0].alive);
    CHECK_FALSE(snap.miniBossDialogue);
    // Story 8.3: a renamed label shows storyNamed; a monster without a boss intro keeps its own name key (the story
    // boss's intro name key: content_ch1_test.cpp).
    const EntityId chief = rig.Spawn(fixture::Brute(), {5, 110});
    rig.M(chief).storyNameShown = true;
    Snapshot named;
    rig.monsters.FillSnapshot(named);
    REQUIRE(named.monsters.size() == 4);
    CHECK(named.monsters[3].storyNamed);
    CHECK(named.monsters[3].nameKey == fixture::Brute().nameKey);  // no boss intro: its own key (intro names: content)
    // Teleport keeps the grid in sync and emits the event.
    rig.monsters.Teleport(b, Vec2(30, 30), TeleportReason::Debug);
    CHECK(rig.monsters.MonsterAtTile(Vec2(30, 30)) == b);
    CHECK(rig.monsters.NearestAlive(Vec2(60, 60), 20) == kNoEntity);
    // ForceChase (taunt / ambush / dismiss): only from idle / patrol / returning.
    rig.monsters.ForceChase(b);
    CHECK(rig.M(b).state == MonsterState::Chase);
    rig.M(b).state = MonsterState::Attack;
    rig.monsters.ForceChase(b);
    CHECK(rig.M(b).state == MonsterState::Attack);
  }

  TEST_CASE("monster swing integration (vector 9): the first swing starts on the first tick in attack state") {
    // The real combat systems around the monster AI, stepped in the GameSim order (AI before handleCombat).
    test::SimHarness h(61);
    auto hero = std::make_unique<Hero>(h.ctx.data, ClassId::Warrior);
    StatusEffectSystem status(h.ctx.data.Classes().statusRules);
    ZoneRuntime zone(h.ctx);
    HeroLocomotion loco(h.ctx);
    MonsterSystem monsters(h.ctx);
    ProjectileSystem projectiles(h.ctx);
    CombatSystem combat(h.ctx);
    SoulEchoSystem soul(h.ctx);
    RewardService rewards(h.ctx);
    SimSystems& s = h.ctx.sys;
    s.hero = hero.get();
    s.status = &status;
    s.zone = &zone;
    s.locomotion = &loco;
    s.monsters = &monsters;
    s.projectiles = &projectiles;
    s.combat = &combat;
    s.soulEcho = &soul;
    s.rewards = &rewards;
    h.onTimer = [&](const Timer& t) {
      if (t.owner == TimerOwner::Combat) combat.OnTimer(t);
      if (t.owner == TimerOwner::Projectiles) projectiles.OnTimer(t);
      if (t.owner == TimerOwner::Monsters) monsters.OnTimer(t);
    };
    h.bus.Subscribe<MonsterKilledMsg>([&](const MonsterKilledMsg& m) { combat.OnMonsterKilled(m); });
    h.session.currentMap = test::ch1::kZone;
    REQUIRE(zone.EnterZone(test::ch1::kZone, true, Vec2(60, 80)));
    hero->RecalcDerived(h.ctx.equip);
    hero->FillHpMana();
    loco.OnZoneEnter();
    combat.OnZoneEnter();
    monsters.SetWorldForTesting(MOpenWorld());
    MonsterSpawnParams p;
    p.baseDef = &fixture::Grunt();
    p.tile = TilePos{60, 83};
    const EntityId g = monsters.Spawn(p);
    MonsterInstance* gm = monsters.Find(g);
    REQUIRE(gm != nullptr);
    gm->hp = gm->maxHp = 1e6;  // survives the hero's auto-attack
    // Contact 250 ms / telegraph 155 ms from the frame rule (manifest clips override it when the art provides them).
    const ActionTiming expect =
        ComputeAttackTiming(MD().Combat().anim, MD().Assets(), "monster_goblin", AnimRig::Humanoid, 1200);
    if (!MD().Assets().FindByGameId("monster_goblin")) {
      CHECK(expect.contactMs == 250);
      CHECK(expect.windupMs == doctest::Approx(155));
    }
    double enteredAttackAt = -1;
    std::vector<double> swings;
    for (int i = 0; i < 400; ++i) {
      h.Step(1);
      hero->RecalcDerived(h.ctx.equip);
      loco.Tick(kSimStepMs);
      monsters.CheckMiniBossDialogue();
      monsters.TickAI(kSimStepMs);
      const MonsterInstance* m = monsters.Find(g);
      if (enteredAttackAt < 0 && m != nullptr && m->state == MonsterState::Attack) enteredAttackAt = h.clock.NowMs();
      h.events.Clear();
      combat.TickCombat();
      for (const Event& e : h.events.Items()) {
        if (const EvPlayAnim* a = std::get_if<EvPlayAnim>(&e)) {
          if (a->entity == g && a->action == AnimAction::Attack) {
            swings.push_back(a->startMs);
            CHECK(a->contactMs == expect.contactMs);
            CHECK(a->windupMs == doctest::Approx(expect.windupMs));
          }
        }
      }
      monsters.TickEliteBehaviours();
      combat.TickStatusEffects();
      if (hero->Hp() <= 0) break;
    }
    REQUIRE(enteredAttackAt > 0);
    REQUIRE(swings.size() >= 2);
    CHECK(swings[0] == enteredAttackAt);
    CHECK(swings[1] - swings[0] >= 1200 - 1e-6);
    CHECK(swings[1] - swings[0] < 1200 + kSimStepMs);
  }

  // ===================================================================================================================
  // Data invariants: NumericalBalance.test.ts "Monster stat curves", story-scrutiny-zones-loot / endgame-scrutiny-fixes
  // (loot-floor flags), zone-content-runtime-wiring (sub-dungeon bosses and spawns), getMonsterDef lookup order (1.1).
  // ===================================================================================================================
  TEST_CASE("monster stat curves (NumericalBalance): hp / damage / defense / exp / gold rise smoothly by zone") {
    const char* zones[] = {"emerald_plains", "twilight_forest", "anvil_mountains", "scorching_desert", "abyss_rift"};
    struct Avg {
      double hp = 0, dmg = 0, def = 0, exp = 0, gold = 0;
    };
    auto normals = [](const char* zone) {
      const ZoneMonsterList* z = MD().Monsters().ZoneList(zone);
      REQUIRE(z != nullptr);
      Avg a;
      int32_t n = 0;
      for (const std::string& id : z->monsterIds) {
        const MonsterDef& d = MDef(id);
        if (d.elite) continue;
        a.hp += d.hp;
        a.dmg += d.damage;
        a.def += d.defense;
        a.exp += d.expReward;
        a.gold += (d.goldMin + d.goldMax) / 2;
        ++n;
      }
      REQUIRE(n > 0);
      a.hp /= n;
      a.dmg /= n;
      a.def /= n;
      a.exp /= n;
      a.gold /= n;
      return a;
    };
    for (size_t i = 0; i + 1 < std::size(zones); ++i) {
      CAPTURE(zones[i]);
      const Avg a = normals(zones[i]);
      const Avg b = normals(zones[i + 1]);
      CHECK(b.hp / a.hp > 1);
      CHECK(b.hp / a.hp < 10);
      CHECK(b.dmg / a.dmg > 1);
      CHECK(b.dmg / a.dmg < 10);
      CHECK(b.def / a.def >= 1);
      CHECK(b.def / a.def < 10);
      CHECK(b.exp > a.exp);
      CHECK(b.gold > a.gold);
    }
    // Elites are significantly stronger than the normals of their zone (avg hp > 2x).
    for (const char* zone : zones) {
      CAPTURE(zone);
      double nHp = 0, eHp = 0;
      int32_t nn = 0, ne = 0;
      for (const std::string& id : MD().Monsters().ZoneList(zone)->monsterIds) {
        const MonsterDef& d = MDef(id);
        if (d.elite) {
          eHp += d.hp;
          ++ne;
        } else {
          nHp += d.hp;
          ++nn;
        }
      }
      if (nn == 0 || ne == 0) continue;
      CHECK(eHp / ne > (nHp / nn) * 2);
    }
  }

  TEST_CASE("loot-floor flags (story / endgame scrutiny): mini-bosses magic+, sub-dungeon and final dungeon boss rare+") {
    const MonsterTables& t = MD().Monsters();
    for (const MiniBossEntry& e : t.miniBosses) {
      CAPTURE(e.monsterId);
      const MonsterDef& d = MDef(e.monsterId);
      CHECK(d.isMiniBoss);
      CHECK_FALSE(d.isSubDungeonMiniBoss);
      CHECK(d.elite);
      CHECK(d.homeZone == e.zoneId);
    }
    REQUIRE(t.subDungeonBosses.size() == 2);
    for (const std::string& id : t.subDungeonBosses) {
      CAPTURE(id);
      const MonsterDef& d = MDef(id);
      CHECK(d.isMiniBoss);
      CHECK(d.isSubDungeonMiniBoss);
      CHECK(d.elite);
      CHECK(d.hp > 0);
      CHECK(d.damage > 0);
      CHECK(std::find(d.sources.begin(), d.sources.end(), MonsterSource::SubDungeonBoss) != d.sources.end());
    }
    const MonsterDef& lord = MDef(t.dungeon.boss);
    CHECK(lord.isMiniBoss);
    CHECK(lord.isSubDungeonMiniBoss);  // the labyrinth final boss guarantees rare+
    const MonsterDef& mid = MDef(t.dungeon.midBoss);
    CHECK(mid.isMiniBoss);
    CHECK_FALSE(mid.isSubDungeonMiniBoss);  // magic floor only
  }

  TEST_CASE("sub-dungeons (zone-content-runtime-wiring): bosses defined, spawns valid, parent zones exist") {
    const WorldTables& w = MD().World();
    REQUIRE_FALSE(w.subDungeons.empty());
    for (const SubDungeonDef& sd : w.subDungeons) {
      CAPTURE(sd.id);
      CHECK(MD().FindMap(sd.parentZone) != nullptr);
      CHECK(sd.cols > 0);
      CHECK(sd.rows > 0);
      CHECK(sd.levelMin <= sd.levelMax);
      const MonsterDef* boss = MD().FindMonster(sd.miniBossId);
      REQUIRE(boss != nullptr);
      CHECK(boss->elite);
      CHECK(boss->isSubDungeonMiniBoss);
      CHECK(std::find(MD().Monsters().subDungeonBosses.begin(), MD().Monsters().subDungeonBosses.end(), sd.miniBossId) !=
            MD().Monsters().subDungeonBosses.end());
      CHECK(sd.miniBossPos.col >= 0);
      CHECK(sd.miniBossPos.col < sd.cols);
      CHECK(sd.miniBossPos.row >= 0);
      CHECK(sd.miniBossPos.row < sd.rows);
      REQUIRE_FALSE(sd.spawns.empty());
      for (const MapSpawnDef& sp : sd.spawns) {
        CAPTURE(sp.monsterId);
        CHECK(MD().Monsters().FindForZone(sd.parentZone, sp.monsterId) != nullptr);
        CHECK(sp.count > 0);
        CHECK(sp.pos.col >= 0);
        CHECK(sp.pos.col < sd.cols);
        CHECK(sp.pos.row >= 0);
        CHECK(sp.pos.row < sd.rows);
      }
    }
    // Every story zone's spawn entries resolve through the zone lookup (defs.find(id) ?? getMonsterDef(id)).
    for (const MapDef& m : w.maps) {
      for (const MapSpawnDef& sp : m.spawns) {
        CAPTURE(m.id);
        CAPTURE(sp.monsterId);
        CHECK(MD().Monsters().FindForZone(m.id, sp.monsterId) != nullptr);
      }
    }
  }

  TEST_CASE("sub-dungeon mini-boss 8.5: fixed tile, parent-zone affix count, rare+ floor flag, never respawns") {
    for (const SubDungeonDef& sd : MD().World().subDungeons) {
      CAPTURE(sd.id);
      MonsterRig rig(62, Vec2(5, 5));
      rig.OpenWorld();
      const EntityId boss = rig.monsters.SpawnSubDungeonMiniBoss(sd);
      REQUIRE(boss != kNoEntity);
      CHECK(rig.monsters.MiniBoss() == boss);
      const MonsterInstance& b = rig.M(boss);
      CHECK(b.def.id == sd.miniBossId);
      CHECK(b.pos == sd.miniBossPos.Center());
      CHECK(b.role == MonsterRole::SubDungeonMiniBoss);
      CHECK(b.noRespawn);
      CHECK(b.def.isSubDungeonMiniBoss);
      int32_t lo = 0, hi = 0;
      MAffixes().CountFor(sd.parentZone, lo, hi);
      CHECK(b.AffixCount() >= lo);
      CHECK(b.AffixCount() <= hi);
      rig.monsters.ApplyDamage(boss, 1e9, DamageFlags{});
      CHECK(rig.monsters.MiniBoss() == kNoEntity);
      CHECK(rig.h.timers.Empty());
      REQUIRE(rig.kills.size() == 1);
      CHECK(rig.kills[0].isSubDungeonMiniBoss);
      CHECK(rig.kills[0].role == MonsterRole::SubDungeonMiniBoss);
    }
    // Out of the sub-dungeon's bounds: nothing spawns.
    SubDungeonDef bad = MD().World().subDungeons.front();
    bad.miniBossPos = TilePos{bad.cols, 0};
    MonsterRig rig(63, Vec2(5, 5));
    rig.OpenWorld();
    CHECK(rig.monsters.SpawnSubDungeonMiniBoss(bad) == kNoEntity);
    bad.miniBossPos = TilePos{1, 1};
    bad.miniBossId = "no_such_monster";
    CHECK(rig.monsters.SpawnSubDungeonMiniBoss(bad) == kNoEntity);
    CHECK(rig.monsters.All().empty());
  }

  TEST_CASE("labyrinth scaling 13.2 (AbyssRun / DifficultySystem tests): raiseToLevel, floor scaling, gatekeepers") {
    const MonsterDef& skel = MDef("skeleton");  // L8 100 / 14 / 7, exp 35, gold 6-12
    const MonsterDef r = RaiseToLevel(skel, 42, MAi());
    CHECK(r.level == 42);
    CHECK(r.hp == 620);  // round(100 x 5.25^1.1)
    CHECK(r.damage == 68);
    CHECK(r.defense == 31);
    CHECK(r.expReward == 217);
    CHECK(r.goldMin == 32);
    CHECK(r.goldMax == 63);
    CHECK(r.hp > skel.hp * 4);  // "raises monsters from earlier lands to the labyrinth level"
    CHECK(r.speed == skel.speed);
    CHECK(r.attackRange == skel.attackRange);
    // Within 6 levels, or no target: unchanged.
    CHECK(RaiseToLevel(MDef("imp"), 42, MAi()).hp == MDef("imp").hp);  // 38 >= 36
    CHECK(RaiseToLevel(MDef("imp"), 42, MAi()).level == 38);
    CHECK(RaiseToLevel(skel, 0, MAi()).level == 8);
    CHECK(RaiseToLevel(skel, 14, MAi()).hp == 100);  // 8 >= 14 - 6
    CHECK(RaiseToLevel(skel, 15, MAi()).level == 15);
    // Floor scaling: depth, floor multipliers, difficulty, curse speed.
    LabyrinthFloorScale f;
    f.levelTarget = 42;
    f.floorNumber = 3;
    f.hpMul = 1.2;
    f.curseSpeedMul = 1.25;
    const MonsterDef s3 = ScaleLabyrinthMonster(skel, f, MDiff(), Difficulty::Nightmare, MAi());
    CHECK(s3.level == 42);
    CHECK(s3.hp == 1116);         // round(620 x 1.2 x 1.5)
    CHECK(s3.damage == 102);      // round(68 x 1 x 1.5)
    CHECK(s3.defense == 40);      // round(31 x 1.3) = round(40.3)
    CHECK(s3.expReward == 564);   // round(217 x 1.3 x 2)
    CHECK(s3.goldMin == 38);      // round(32 x 1.2); gold has no difficulty factor
    CHECK(s3.goldMax == 76);      // round(75.6)
    CHECK(s3.speed == 88);        // round(70 x 1.25)
    CHECK(s3.attackSpeedMs == 720);
    // Floor 1, unit multipliers, no level target: identical to the overworld difficulty scaling.
    MonsterDef mock;
    mock.id = "mock";
    mock.level = 10;
    mock.hp = 200;
    mock.damage = 50;
    mock.defense = 20;
    mock.expReward = 100;
    const LabyrinthFloorScale one;
    const MonsterDef dn = ScaleLabyrinthMonster(mock, one, MDiff(), Difficulty::Nightmare, MAi());
    const MonsterDef ow = ScaleMonsterForDifficulty(mock, MDiff(), Difficulty::Nightmare);
    CHECK(dn.hp == ow.hp);
    CHECK(dn.damage == ow.damage);
    CHECK(dn.defense == ow.defense);
    LabyrinthFloorScale deep;
    deep.floorNumber = 5;
    deep.hpMul = 1.4;
    CHECK(ScaleLabyrinthMonster(mock, deep, MDiff(), Difficulty::Nightmare, MAi()).hp >= ow.hp);
    // Gatekeepers: elite / mini-boss bases x1.6 hp, others x4; damage x1.3, exp x4, gold x3, aggro >= 8.
    LabyrinthFloorScale g1;
    g1.levelTarget = 42;
    for (const char* id : {"werewolf_alpha", "mountain_troll", "sandworm", "dungeon_fiend"}) {
      CAPTURE(id);
      const MonsterDef& base = MDef(id);
      const MonsterDef scaled = ScaleLabyrinthMonster(base, g1, MDiff(), Difficulty::Normal, MAi());
      const MonsterDef gk = MakeGatekeeper(base, g1, MDiff(), Difficulty::Normal, MAi(), "dungeon.gatekeeper.crypt");
      CHECK(gk.id == "dungeon_gatekeeper");
      CHECK(gk.nameKey == "dungeon.gatekeeper.crypt");
      CHECK(gk.elite);
      CHECK(gk.isMiniBoss);
      CHECK(gk.hp > base.hp);
      CHECK(gk.hp == JsRound(scaled.hp * ((base.elite || base.isMiniBoss) ? 1.6 : 4.0)));
      CHECK(gk.damage == JsRound(scaled.damage * 1.3));
      CHECK(gk.expReward == JsRound(scaled.expReward * 4));
      CHECK(gk.goldMin == scaled.goldMin * 3);
      CHECK(gk.goldMax == scaled.goldMax * 3);
      CHECK(gk.aggroRange == (std::max)(scaled.aggroRange, 8.0));
    }
  }

  // ===================================================================================================================
  // Labels and mini-boss lines as i18n keys (EliteAffixSystem.test.ts buildAffixName; FIX Q10 / Q11, M8)
  // ===================================================================================================================
  TEST_CASE("buildAffixName: base name without affixes, [a] base, [a<middle dot>b] base, both locales") {
    I18n zh, en;
    REQUIRE(zh.LoadTable(test::ReadFile(test::DataDir() + "/i18n_zh-CN.json")));
    REQUIRE(en.LoadTable(test::ReadFile(test::DataDir() + "/i18n_en.json")));
    en.SetLocale(LocaleId::En);
    REQUIRE(en.Current() == LocaleId::En);
    for (EliteAffixType t : MAffixes().order) {
      CAPTURE(EnumName(t));
      const std::string key = EliteAffixNameKey(t);
      CHECK(key == "sys.eliteAffix.name." + std::string(EnumName(t)));
      CHECK(zh.Has(key));
      CHECK(en.Has(key));
      CHECK(zh.T(key) == MAffixes().Def(t).name);    // the zh-CN names of the affix table
      CHECK(en.T(key) == MAffixes().Def(t).nameEn);  // and the English ones
    }
    // An empty base name keeps the bracket (the named labels of the Chapter 1 chief and hunt: content_ch1_test.cpp).
    const std::vector<EliteAffixType> one{EliteAffixType::FireEnhanced};
    CHECK(MonsterLabelText(en, "", one) == "[" + en.T("sys.eliteAffix.name.fire_enhanced") + "] ");
  }

  TEST_CASE("mini-boss lines are i18n keys (Q10 / M8): the linear tree in order, every key in zh-CN and en") {
    I18n zh, en;
    REQUIRE(zh.LoadTable(test::ReadFile(test::DataDir() + "/i18n_zh-CN.json")));
    REQUIRE(en.LoadTable(test::ReadFile(test::DataDir() + "/i18n_en.json")));
    en.SetLocale(LocaleId::En);
    for (const MiniBossEntry& e : MD().Monsters().miniBosses) {
      CAPTURE(e.monsterId);
      const DialogueTree* tree = MD().Monsters().MiniBossDialogue(e.monsterId);
      REQUIRE(tree != nullptr);
      const std::vector<std::string> keys = MiniBossDialogueLineKeys(*tree, e.monsterId);
      CHECK(keys.size() >= 3);
      const DialogueNode* n = tree->FindNode(tree->startNodeId);
      for (const std::string& k : keys) {
        CAPTURE(k);
        REQUIRE(n != nullptr);
        CHECK(k == "data.miniBossDialogue." + e.monsterId + "." + n->id);
        CHECK(zh.Has(k));
        CHECK(en.Has(k));
        CHECK(zh.T(k) == n->text);  // zh-CN is the tree's own text
        CHECK(en.T(k) != n->text);  // translated
        n = n->isEnd ? nullptr : tree->FindNode(n->nextNodeId);
      }
      CHECK(n == nullptr);  // stopped at isEnd
      CHECK(zh.Has(MDef(e.monsterId).nameKey));
      CHECK(en.Has(MDef(e.monsterId).nameKey));
    }
    // A broken tree (cycle / dangling next) terminates.
    DialogueTree loop;
    loop.id = "x";
    loop.startNodeId = "a";
    loop.nodes = {DialogueNode{"a", "A", {}, "b", false}, DialogueNode{"b", "B", {}, "a", false}};
    CHECK(MiniBossDialogueLineKeys(loop, "x") == std::vector<std::string>{"data.miniBossDialogue.x.a",
                                                                          "data.miniBossDialogue.x.b"});
    DialogueTree dangling;
    dangling.startNodeId = "a";
    dangling.nodes = {DialogueNode{"a", "A", {}, "missing", false}};
    CHECK(MiniBossDialogueLineKeys(dangling, "y").size() == 1);
    DialogueTree empty;
    empty.startNodeId = "nope";
    CHECK(MiniBossDialogueLineKeys(empty, "z").empty());
  }

  // ===================================================================================================================
  // Difficulty scaling edge cases (DifficultySystem.test.ts)
  // ===================================================================================================================
  TEST_CASE("scaleMonster edge cases: zero stats, very high stats, untouched fields and derived data") {
    MonsterDef zero;
    zero.id = "zero";
    const MonsterDef z = ScaleMonsterForDifficulty(zero, MDiff(), Difficulty::Hell);
    CHECK(z.hp == 0);
    CHECK(z.damage == 0);
    CHECK(z.defense == 0);
    CHECK(z.expReward == 0);
    CHECK(z.goldMin == 0);
    CHECK(z.goldMax == 0);
    MonsterDef big = MDef("demon_lord");
    big.hp = 1e6;
    big.damage = 5e4;
    const MonsterDef b = ScaleMonsterForDifficulty(big, MDiff(), Difficulty::Hell);
    CHECK(b.hp == 2e6);
    CHECK(b.damage == 1e5);
    const MonsterDef& fe = MDef("fire_elemental");
    const MonsterDef f = ScaleMonsterForDifficulty(fe, MDiff(), Difficulty::Nightmare);
    CHECK(f.id == fe.id);
    CHECK(f.level == fe.level);
    CHECK(f.speed == fe.speed);
    CHECK(f.aggroRange == fe.aggroRange);
    CHECK(f.attackRange == fe.attackRange);
    CHECK(f.attackSpeedMs == fe.attackSpeedMs);
    CHECK(f.isRanged == fe.isRanged);
    CHECK(f.projectileColor == fe.projectileColor);
    CHECK(f.onHitStatus.size() == fe.onHitStatus.size());
    CHECK(f.spriteKey == fe.spriteKey);
    CHECK(f.animCategory == fe.animCategory);
    CHECK(f.elite == fe.elite);
    CHECK(f.nameKey == fe.nameKey);
  }

  // ===================================================================================================================
  // Quest actors: escort chip (vector 20, quests 3.9), defend waves and their chip damage (quests 3.10, monsters 6.4)
  // ===================================================================================================================
  TEST_CASE("escort chip (vector 20): 2 aggro goblins within 4 tiles, hits at 0 / 2016.7 / 4033.3 ms -> 12 damage") {
    MonsterRig rig(51, Vec2(100, 100));
    rig.OpenWorld();
    const Vec2 escort(60, 60);
    const EntityId a = rig.Spawn(fixture::Grunt(), {62, 60});
    const EntityId b = rig.Spawn(fixture::Grunt(), {60, 63});
    const EntityId idle = rig.Spawn(fixture::Grunt(), {61, 61});       // not aggro: never chips
    const EntityId edge = rig.Spawn(fixture::Ooze(), {64, 60});  // distSq 16 is not < 16
    rig.M(a).state = MonsterState::Chase;
    rig.M(b).state = MonsterState::Attack;
    rig.M(edge).state = MonsterState::Chase;
    double hp = 200;
    std::vector<double> hitTimes;
    std::vector<EntityId> hitters;
    const MonsterSystem::ChipApply apply = [&](EntityId m, double dmg) {
      hp -= dmg;
      hitters.push_back(m);
      if (hitTimes.empty() || hitTimes.back() != rig.h.clock.NowMs()) hitTimes.push_back(rig.h.clock.NowMs());
      return hp > 0;
    };
    REQUIRE(rig.h.clock.NowMs() == 0);
    while (rig.h.clock.NowMs() <= 4100) {
      rig.monsters.ChipEscort(escort, apply);
      rig.h.Step(1);  // the clock only: the monsters keep their states
    }
    REQUIRE(hitTimes.size() == 3);
    CHECK(hitTimes[0] == 0);
    CHECK(hitTimes[1] == doctest::Approx(2016.6667).epsilon(1e-6));
    CHECK(hitTimes[2] == doctest::Approx(4033.3333).epsilon(1e-6));
    CHECK(hp == 188);  // 3 hits x max(1, floor(8 x 0.3)) = 2 dmg x 2 goblins
    CHECK(std::count(hitters.begin(), hitters.end(), idle) == 0);
    CHECK(std::count(hitters.begin(), hitters.end(), edge) == 0);
    CHECK(hitters == std::vector<EntityId>{a, b, a, b, a, b});  // list order
    // The escort dies mid-loop: the loop stops (web `return`), the next monster keeps its timer.
    MonsterRig r2(52, Vec2(100, 100));
    r2.OpenWorld();
    const EntityId c = r2.Spawn(fixture::Brute(), {60, 61});
    const EntityId d = r2.Spawn(fixture::Grunt(), {61, 60});
    r2.M(c).state = MonsterState::Chase;
    r2.M(d).state = MonsterState::Chase;
    double escortHp = 3;
    std::vector<EntityId> who;
    const double dealt = r2.monsters.ChipEscort(escort, [&](EntityId m, double dmg) {
      escortHp -= dmg;
      who.push_back(m);
      return escortHp > 0;
    });
    CHECK(dealt == 4);  // max(1, floor(14 x 0.3)) = 4
    CHECK(who == std::vector<EntityId>{c});
    CHECK(r2.M(c).lastEscortChipMs == 0);
    CHECK(r2.M(d).lastEscortChipMs < -1e299);  // untouched
    // Dead monsters never chip.
    r2.monsters.ApplyDamage(d, 1e6, DamageFlags{});
    r2.h.Step(200);
    CHECK(r2.monsters.ChipEscort(escort, nullptr) == 4);  // only the chief, null apply allowed
  }

  TEST_CASE("defend chip (quests 3.10): wave monsters within 3 tiles, first hit after 2000 ms, floor(dmg x 0.2)") {
    MonsterRig rig(55, Vec2(100, 100));
    rig.OpenWorld();
    const Vec2 target(60, 60);
    const EntityId near = rig.Spawn(fixture::Brute(), {62, 60}, MonsterRole::DefendWave);
    const EntityId far = rig.Spawn(fixture::Grunt(), {63, 60}, MonsterRole::DefendWave);  // distSq 9 is not < 9
    const EntityId other = rig.Spawn(fixture::Grunt(), {60, 61});                         // not a wave monster
    const std::vector<EntityId> wave{near, far};
    double total = 0;
    while (rig.h.clock.NowMs() <= 4100) {
      total += rig.monsters.ChipDefendTarget(wave, target, nullptr);
      rig.h.Step(1);
    }
    CHECK(total == 2 * 2);  // max(1, floor(14 x 0.2)) = 2 at 2016.7 and 4033.3 (lastDefendAttack starts at 0)
    CHECK(rig.M(other).lastDefendChipMs == 0);
    CHECK(rig.M(far).lastDefendChipMs == 0);
    std::vector<EntityId> hits;
    rig.M(far).pos = Vec2(61, 61);
    rig.h.Step(130);
    rig.monsters.ChipDefendTarget(wave, target, [&](EntityId m, double) {
      hits.push_back(m);
      return false;  // destroyed by the first hit
    });
    CHECK(hits == std::vector<EntityId>{near});
  }

  // ===================================================================================================================
  // Boss rename hook (monsters 10), Returning monsters in the activity set (M1), wind-up snapshot (combat 10.3)
  // ===================================================================================================================

  TEST_CASE("M1: a Returning monster stays in the activity set beyond 30 tiles and walks home") {
    MonsterRig rig(57, Vec2(10, 10));
    rig.OpenWorld();
    const EntityId g = rig.Spawn(fixture::Grunt(), {90, 90});
    rig.M(g).pos = Vec2(99.5, 90);  // 9.5 from home, 89.6 from the hero
    rig.M(g).state = MonsterState::Chase;
    rig.M(g).hp = 20;
    rig.Step();  // chase -> leash -> Returning (same tick)
    REQUIRE(rig.M(g).state == MonsterState::Returning);
    rig.Step(20);  // past an activity refresh: Returning is no longer aggro but stays active
    CHECK(rig.M(g).active);
    CHECK(rig.M(g).pos.x < 99.5);
    for (int i = 0; i < 600 && rig.M(g).state == MonsterState::Returning; ++i) rig.Step();
    CHECK(rig.M(g).state == MonsterState::Idle);
    CHECK(Dist(rig.M(g).pos, Vec2(90, 90)) <= MAi().returnHomeRadius + 1e-9);
    CHECK(rig.M(g).hp == 55);  // healed 0.6 maxHp / s on the way
    rig.Step(20);              // idle again far from the hero: culled at the next refresh
    CHECK_FALSE(rig.M(g).active);
  }

  // The real combat systems around the monster AI (GameSim step order), hero at a fixed position.
  struct MCombatRig {
    explicit MCombatRig(uint64_t seed, Vec2 heroPos)
        : h(seed),
          hero(std::make_unique<Hero>(h.ctx.data, ClassId::Warrior)),
          status(h.ctx.data.Classes().statusRules),
          zone(h.ctx),
          loco(h.ctx),
          monsters(h.ctx),
          projectiles(h.ctx),
          combat(h.ctx),
          soul(h.ctx),
          rewards(h.ctx) {
      SimSystems& s = h.ctx.sys;
      s.hero = hero.get();
      s.status = &status;
      s.zone = &zone;
      s.locomotion = &loco;
      s.monsters = &monsters;
      s.projectiles = &projectiles;
      s.combat = &combat;
      s.soulEcho = &soul;
      s.rewards = &rewards;
      h.onTimer = [this](const Timer& t) {
        if (t.owner == TimerOwner::Combat) combat.OnTimer(t);
        if (t.owner == TimerOwner::Projectiles) projectiles.OnTimer(t);
        if (t.owner == TimerOwner::Monsters) monsters.OnTimer(t);
      };
      h.bus.Subscribe<MonsterKilledMsg>([this](const MonsterKilledMsg& m) { combat.OnMonsterKilled(m); });
      h.bus.Subscribe<HeroDamagedMsg>([this](const HeroDamagedMsg&) { ++heroHits; });
      h.session.currentMap = test::ch1::kZone;
      ok = zone.EnterZone(test::ch1::kZone, true, heroPos);
      hero->SetPosition(heroPos);
      hero->RecalcDerived(h.ctx.equip);
      hero->FillHpMana();
      loco.OnZoneEnter();
      combat.OnZoneEnter();
      monsters.SetWorldForTesting(MOpenWorld());
    }
    EntityId Spawn(const MonsterDef& def, TilePos tile) {  // a synthetic fixture (TestContent.h)
      MonsterSpawnParams p;
      p.baseDef = &def;
      p.tile = tile;
      const EntityId e = monsters.Spawn(p);
      MonsterInstance* m = monsters.Find(e);
      REQUIRE(m != nullptr);
      m->hp = m->maxHp = 1e6;  // survives anything the hero does
      return e;
    }
    test::SimHarness h;
    std::unique_ptr<Hero> hero;
    StatusEffectSystem status;
    ZoneRuntime zone;
    HeroLocomotion loco;
    MonsterSystem monsters;
    ProjectileSystem projectiles;
    CombatSystem combat;
    SoulEchoSystem soul;
    RewardService rewards;
    int32_t heroHits = 0;
    bool ok = false;
  };

  TEST_CASE("melee whiff (vector 10): reach attackRange x 1.35 + 0.5 measured at contact; 2.52 hits, 2.53 whiffs") {
    for (const double at : {2.52, 2.53}) {
      CAPTURE(at);
      MCombatRig rig(58, Vec2(60, 60));
      REQUIRE(rig.ok);
      const EntityId g = rig.Spawn(fixture::Grunt(), {60, 61});  // 1 tile: in attack range when the swing starts
      rig.monsters.Find(g)->state = MonsterState::Attack;
      rig.h.rng.Get(RngStream::Combat).Script({0.99, 0.99, 0.99, 0.99, 0.99, 0.99});  // no dodge, no crit
      rig.combat.TickCombat();                                                          // swing starts now
      REQUIRE(rig.monsters.Find(g)->lastAttackMs == rig.h.clock.NowMs());
      rig.hero->SetPosition(Vec2(60, 61 - at));  // the hero steps away during the wind-up
      rig.h.Step(16);                             // 266.7 ms > 250 ms contact
      CHECK(rig.heroHits == (at < 2.525 ? 1 : 0));
    }
  }

  TEST_CASE("ranged rule (vector 11, M5): attackRange > 2.5 launches a bolt at contact, 2.5 resolves as melee") {
    MCombatRig rig(59, Vec2(60, 60));
    REQUIRE(rig.ok);
    const EntityId shaman = rig.Spawn(fixture::Caster(), {60, 63});  // range 3.0 -> ranged
    rig.monsters.Find(shaman)->state = MonsterState::Attack;
    rig.monsters.Find(shaman)->affixes.clear();
    rig.combat.TickCombat();
    rig.h.events.Clear();
    rig.h.Step(16);
    const std::vector<EvProjectileLaunched> bolts = MEvents<EvProjectileLaunched>(rig.h.events);
    REQUIRE(bolts.size() == 1);
    CHECK(bolts[0].source == shaman);
    CHECK(bolts[0].kind == ProjectileKind::MonsterBolt);
    CHECK(bolts[0].to == Vec2(60, 60));
    CHECK(bolts[0].color == MFix(fixture::Caster()).projectileColor);
    // A 2.5-range definition is melee (no bolt).
    MCombatRig m2(60, Vec2(60, 60));
    MonsterDef melee = MFix(fixture::Grunt());
    melee.attackRange = 2.5;
    melee.isRanged = melee.attackRange > MAi().rangedThreshold;
    CHECK_FALSE(melee.isRanged);
    MonsterSpawnParams p;
    p.baseDef = &melee;
    p.tile = TilePos{60, 62};
    const EntityId g = m2.monsters.Spawn(p);
    m2.monsters.Find(g)->state = MonsterState::Attack;
    m2.combat.TickCombat();
    m2.h.events.Clear();
    m2.h.Step(16);
    CHECK(MEvents<EvProjectileLaunched>(m2.h.events).empty());
  }

  TEST_CASE("snapshot wind-up (combat 10.3): windingUp for 0.62 x contact after a swing start") {
    MCombatRig rig(61, Vec2(60, 60));
    REQUIRE(rig.ok);
    const EntityId g = rig.Spawn(fixture::Grunt(), {60, 61});
    auto winding = [&]() {
      Snapshot s;
      rig.monsters.FillSnapshot(s);
      rig.combat.FillSnapshot(s);  // the telegraph's owner (pending strike), filled after the monster views as in GameSim
      for (const MonsterView& v : s.monsters) {
        if (v.id == g) return v.windingUp;
      }
      return false;
    };
    CHECK_FALSE(winding());  // never swung
    rig.h.Step(1);           // lastAttackMs 0 means "never swung": start the swing after t = 0
    rig.monsters.Find(g)->state = MonsterState::Attack;
    rig.combat.TickCombat();
    REQUIRE(rig.monsters.Find(g)->lastAttackMs == rig.h.clock.NowMs());
    const ActionTiming t = ComputeAttackTiming(MD().Combat().anim, MD().Assets(), "monster_goblin", AnimRig::Humanoid, 1200);
    CHECK(winding());
    const int windSteps = static_cast<int>(std::floor(t.windupMs / kSimStepMs));
    rig.h.Step(windSteps);
    CHECK(winding() == (windSteps * kSimStepMs < t.windupMs));
    rig.h.Step(2);
    CHECK_FALSE(winding());
  }
}
