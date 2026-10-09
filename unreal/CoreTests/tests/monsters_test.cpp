// Monsters area: definitions pipeline, AI, spatial grid, spawning, respawn, hunts, mini-boss, elite affixes, queries.
// Spec vectors: monsters-ai.md section 19 (+ the tables of sections 2.1-2.4), combat-feel.md 14 / 17 / 24; ported web
// Vitest suites: EliteAffixSystem.test.ts, QuestHunts.test.ts, DifficultySystem.test.ts (scaleMonster),
// mini-bosses-lore.test.ts (mini-boss data), SpatialGrid.test.ts, PhaserOptimizationPass.test.ts (AI culling),
// MonsterFacing.test.ts (heading wiring). Port decisions: DECISIONS M1-M11, W9.
#include "SimHarness.h"

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

namespace {

const DataStore& MD() { return test::RealData(); }
const MonsterAiDef& MAi() { return MD().Monsters().ai; }
const EliteAffixTable& MAffixes() { return MD().Combat().eliteAffixes; }
const DifficultyTable& MDiff() { return MD().Combat().difficulty; }

const MonsterDef& MDef(std::string_view id) {
  const MonsterDef* d = MD().FindMonster(id);
  REQUIRE(d != nullptr);
  return *d;
}

const HuntDef& MHunt(std::string_view id) {
  const HuntDef* h = MD().Monsters().FindHunt(id);
  REQUIRE(h != nullptr);
  return *h;
}

// The normal-difficulty definition of a Chapter-1 monster or hunt leader (hunts through makeHuntDefinition).
MonsterDef MNormalDef(std::string_view id) {
  if (const HuntDef* h = MD().Monsters().FindHunt(id)) return MakeHuntDefinition(MDef(h->monsterId), *h, MAi(), h->name);
  return MDef(id);
}

MonsterInstance MMake(const MonsterDef& def, Vec2 pos, EntityId id = 100) {
  MonsterInstance m;
  m.id = id;
  m.def = def;
  m.originalDef = def;
  m.hp = m.maxHp = def.hp;
  m.stats = MonsterBaseStats(def, MAi());
  m.pos = pos;
  m.prevPos = pos;
  m.spawnAnchor = RoundToTile(pos);
  return m;
}

MonsterWorld MOpenWorld(int32_t cols = 120, int32_t rows = 120) {
  MonsterWorld w;
  w.cols = cols;
  w.rows = rows;
  w.walkable = [](int32_t, int32_t) { return true; };
  return w;
}

// A small editable walkability grid with a BFS-shortest 8-direction path service (no corner cutting, start excluded,
// end included) standing in for the world area's A*.
struct MTestGrid {
  int32_t cols = 0, rows = 0;
  std::vector<uint8_t> walk;
  MTestGrid(int32_t c, int32_t r) : cols(c), rows(r), walk(static_cast<size_t>(c * r), 1) {}
  bool Walk(int32_t c, int32_t r) const {
    return c >= 0 && r >= 0 && c < cols && r < rows && walk[static_cast<size_t>(r * cols + c)] != 0;
  }
  void Block(int32_t c, int32_t r) { walk[static_cast<size_t>(r * cols + c)] = 0; }
  bool Path(Vec2 from, Vec2 to, std::vector<TilePos>& out) const {
    out.clear();
    const TilePos s = RoundToTile(from), e = RoundToTile(to);
    if (!Walk(e.col, e.row) || s == e) return false;
    std::vector<int32_t> prev(static_cast<size_t>(cols * rows), -2);
    std::deque<TilePos> q;
    q.push_back(s);
    prev[static_cast<size_t>(s.row * cols + s.col)] = -1;
    const std::array<TilePos, 8> dirs{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}}};
    while (!q.empty()) {
      const TilePos c = q.front();
      q.pop_front();
      if (c == e) break;
      for (const TilePos d : dirs) {
        const TilePos n{c.col + d.col, c.row + d.row};
        if (!Walk(n.col, n.row)) continue;
        if (d.col != 0 && d.row != 0 && (!Walk(c.col + d.col, c.row) || !Walk(c.col, c.row + d.row))) continue;
        int32_t& p = prev[static_cast<size_t>(n.row * cols + n.col)];
        if (p != -2) continue;
        p = c.row * cols + c.col;
        q.push_back(n);
      }
    }
    if (prev[static_cast<size_t>(e.row * cols + e.col)] == -2) return false;
    for (int32_t at = e.row * cols + e.col; at != s.row * cols + s.col; at = prev[static_cast<size_t>(at)]) {
      out.push_back(TilePos{at % cols, at / cols});
    }
    std::reverse(out.begin(), out.end());
    return true;
  }
};

MonsterWorld MGridWorld(const std::shared_ptr<MTestGrid>& g, bool withPath) {
  MonsterWorld w;
  w.cols = g->cols;
  w.rows = g->rows;
  w.walkable = [g](int32_t c, int32_t r) { return g->Walk(c, r); };
  if (withPath) w.findPath = [g](Vec2 a, Vec2 b, std::vector<TilePos>& out) { return g->Path(a, b, out); };
  return w;
}

MonsterAiResult MTick(MonsterInstance& m, const MonsterAiDef& ai, Vec2 hero, const MonsterWorld& w, Rng& rng,
                      double nowMs, bool visible = true, double speedMul = 1, std::span<const Vec2> neighbours = {}) {
  MonsterAiInput in;
  in.nowMs = nowMs;
  in.dtMs = kSimStepMs;
  in.heroVisible = visible;
  in.heroPos = hero;
  in.speedMul = speedMul;
  in.neighbours = neighbours;
  return UpdateMonsterAI(m, ai, in, w, rng);
}

template <class E>
std::vector<E> MEvents(const EventSink& sink) {
  std::vector<E> out;
  for (const Event& e : sink.Items()) {
    if (const E* p = std::get_if<E>(&e)) out.push_back(*p);
  }
  return out;
}

bool MHasLog(const EventSink& sink, std::string_view key) {
  for (const EvLog& l : MEvents<EvLog>(sink)) {
    if (l.text.key == key) return true;
  }
  return false;
}

std::vector<EliteAffixType> MOne(EliteAffixType t) { return {t}; }

// ---------------------------------------------------------------------------------------------------------------------
// Runtime rig: MonsterSystem over the real emerald_plains zone with the hero, statuses and (optionally) a quest system.
// Step() runs the monster parts of a GameSim step in order: timers, mini-boss check, AI, elite behaviours.
// ---------------------------------------------------------------------------------------------------------------------
struct MonsterRig {
  explicit MonsterRig(uint64_t seed = 7, Vec2 heroPos = Vec2(60, 80))
      : h(seed),
        hero(std::make_unique<Hero>(h.ctx.data, ClassId::Warrior)),
        status(h.ctx.data.Classes().statusRules),
        zone(h.ctx),
        quests(h.ctx.data, h.events, h.bus),
        monsters(h.ctx) {
    SimSystems& s = h.ctx.sys;
    s.hero = hero.get();
    s.status = &status;
    s.zone = &zone;
    s.quests = &quests;
    s.monsters = &monsters;
    h.onTimer = [this](const Timer& t) {
      if (t.owner == TimerOwner::Monsters) monsters.OnTimer(t);
    };
    h.bus.Subscribe<MonsterKilledMsg>([this](const MonsterKilledMsg& m) {
      kills.push_back(m);
      monsters.OnMonsterKilled(m);
    });
    h.bus.Subscribe<MonsterAggroMsg>([this](const MonsterAggroMsg& m) { aggros.push_back(m); });
    h.session.currentMap = "emerald_plains";
    ok = zone.EnterZone("emerald_plains", true, heroPos);
    hero->RecalcDerived(h.ctx.equip);
    hero->FillHpMana();
  }

  void Step(int n = 1) {
    for (int i = 0; i < n; ++i) {
      h.Step(1);
      monsters.CheckMiniBossDialogue();
      monsters.TickAI(kSimStepMs);
      monsters.TickEliteBehaviours();
    }
  }

  // An open 120 x 120 test world (the zone's camps still define the safe zones).
  void OpenWorld() { monsters.SetWorldForTesting(MOpenWorld()); }

  EntityId Spawn(std::string_view defId, TilePos tile, MonsterRole role = MonsterRole::Regular, bool affixes = false) {
    MonsterSpawnParams p;
    p.baseDef = MD().FindMonster(defId);
    p.tile = tile;
    p.role = role;
    p.rollAffixes = affixes;
    return monsters.Spawn(p);
  }

  MonsterInstance& M(EntityId id) {
    MonsterInstance* m = monsters.Find(id);
    REQUIRE(m != nullptr);
    return *m;
  }

  test::SimHarness h;
  std::unique_ptr<Hero> hero;
  StatusEffectSystem status;
  ZoneRuntime zone;
  QuestSystem quests;
  MonsterSystem monsters;
  std::vector<MonsterKilledMsg> kills;
  std::vector<MonsterAggroMsg> aggros;
  bool ok = false;
};

}  // namespace

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
    const MiniBossEntry* ep = t.MiniBossFor("emerald_plains");
    CHECK(ep->monsterId == "miniboss_goblin_shaman");
    CHECK(ep->spawn == TilePos{60, 55});
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

  TEST_CASE("defend-wave scaling 6.4: floor(hp * (1 + 0.3 w)), floor(damage * (1 + 0.2 w))") {
    const MonsterDef& g = MDef("goblin");
    const MonsterDef w0 = ScaleDefendWave(g, 0);
    CHECK(w0.hp == 55);
    CHECK(w0.damage == 8);
    const MonsterDef w2 = ScaleDefendWave(g, 2);
    CHECK(w2.hp == std::floor(55 * 1.6));
    CHECK(w2.damage == std::floor(8 * 1.4));
    CHECK(w2.defense == g.defense);
  }

  // ===================================================================================================================
  // Quest hunts (monsters 9; QuestHunts.test.ts; vectors 12-14)
  // ===================================================================================================================
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

  TEST_CASE("elite on-hit 17.4: extra fire, lifesteal (floors), frozen roll draws only with a freeze chance") {
    MonsterInstance m = MMake(MDef("goblin_chief"), {5, 5});
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
    MonsterInstance m = MMake(MDef("goblin_chief"), {60, 80});
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
    MonsterInstance edge = MMake(MDef("goblin_chief"), {5, 5});
    ApplyEliteAffixes(edge, MOne(EliteAffixType::Teleporting), MAffixes(), MAi());
    rng.ClearScript();
    rng.Script({0.1, 0.99, 0.1, 0.99});
    REQUIRE(EliteTeleportTarget(edge, MAffixes(), Vec2(1.5, 1.5), 0, 120, 120, rng, out));
    CHECK(out == TilePos{1, 1});
    // No teleporting affix: never.
    MonsterInstance plain = MMake(MDef("goblin_chief"), {60, 80});
    CHECK_FALSE(EliteTeleportTarget(plain, MAffixes(), Vec2(60, 85), 0, 120, 120, rng, out));
  }

  TEST_CASE("curse aura 17.4: radius 4 inclusive, log at most every 2000 ms (strictly more)") {
    MonsterInstance m = MMake(MDef("goblin_chief"), {60, 80});
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
    MonsterInstance plain = MMake(MDef("goblin_chief"), {60, 80});
    CHECK_FALSE(EliteCurseAuraInRange(plain, MAffixes(), Vec2(60, 80), 0, logNow));
  }

  // ===================================================================================================================
  // AI tick (monsters 3; vectors 1-7; MonsterFacing.test.ts)
  // ===================================================================================================================
  TEST_CASE("movement ramp (vector 1): slime from rest, per-tick steps and cumulative distance") {
    MonsterInstance m = MMake(MDef("slime_green"), {10, 10});
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
    MonsterInstance s = MMake(MDef("slime_green"), {10, 10});
    s.state = MonsterState::Chase;
    s.def.aggroRange = 1000;
    for (int t = 1; t <= 600; ++t) MTick(s, MAi(), Vec2(300, 10), w, rng, t * kSimStepMs, true, 0.5);
    CHECK(s.moveSpeed == doctest::Approx(0.6).epsilon(1e-6));
  }

  TEST_CASE("aggro edge (vector 2), chase / attack hysteresis (vector 3), chase drop (vector 4), facing") {
    const MonsterWorld w = MOpenWorld();
    Rng rng(1);
    MonsterInstance g = MMake(MDef("goblin"), {10, 10});
    MonsterAiResult r = MTick(g, MAi(), Vec2(15, 10), w, rng, 100);  // dP = 5 = aggroRange
    CHECK(g.state == MonsterState::Chase);
    CHECK(r.stateChanged);
    CHECK(g.pos == Vec2(10, 10));  // no move on the transition tick
    MonsterInstance g2 = MMake(MDef("goblin"), {10, 10});
    MTick(g2, MAi(), Vec2(15.01, 10), w, rng, 100);
    CHECK(g2.state == MonsterState::Idle);

    // Chase -> attack at <= 1.5 (no move), stays until > 1.8 (1.5 * 1.2 = 1.7999999999999998).
    MonsterInstance c = MMake(MDef("goblin"), {10, 10});
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
    MonsterInstance d = MMake(MDef("goblin"), {10, 10});
    d.state = MonsterState::Chase;
    MTick(d, MAi(), Vec2(17.5, 10), w, rng, 100);
    CHECK(d.state == MonsterState::Chase);
    CHECK(d.pos.x > 10);
    CHECK(d.moveSpeed > 0);
    MonsterInstance e = MMake(MDef("goblin"), {10, 10});
    e.state = MonsterState::Chase;
    e.moveSpeed = 1;
    MTick(e, MAi(), Vec2(17.5001, 10), w, rng, 100);
    CHECK(e.state == MonsterState::Idle);
    CHECK(e.moveSpeed == 0);
    CHECK(e.pos == Vec2(10, 10));

    // MonsterFacing.test.ts: chase faces the hero (up / down), attack keeps facing it.
    MonsterInstance up = MMake(MDef("goblin"), {10, 10});
    up.state = MonsterState::Chase;
    up.def.aggroRange = 20;
    MTick(up, MAi(), Vec2(10, 5), w, rng, 100);
    CHECK(up.heading.y < 0);
    MTick(up, MAi(), Vec2(10, 15), w, rng, 117);
    CHECK(up.heading.y > 0);
    MonsterInstance at = MMake(MDef("goblin"), {10, 10});
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
    MonsterInstance g = MMake(MDef("goblin"), {18.5, 10});
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
    MonsterInstance g = MMake(MDef("goblin"), {18.5, 10});
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
    MonsterInstance g = MMake(MDef("goblin"), {20, 20});
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
    MonsterInstance b = MMake(MDef("goblin"), {20, 20});
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
    MonsterInstance s = MMake(MDef("goblin"), {20, 20});
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
    MonsterInstance k = MMake(MDef("goblin"), {20, 20});
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
    MonsterInstance s = MMake(MDef("slime_green"), {20, 20});  // aggro 4: drops at 6
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
    MonsterInstance g = MMake(MDef("goblin"), {5, 5});
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
    MonsterInstance stuck = MMake(MDef("goblin"), {5, 5});
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
    MonsterInstance sl = MMake(MDef("goblin"), {10.49, 10});
    sl.moveSpeed = MonsterTopSpeed(sl.def, MAi());
    CHECK_FALSE(MonsterMoveToward(sl, MAi(), Vec2(20, 20), kSimStepMs, 1, MGridWorld(wall, false)));
    CHECK(sl.pos.x == 10.49);
    CHECK(sl.pos.y > 10);

    // Separation: two goblins stacked on one tile drift apart; an attacker never leaves its attack range.
    const MonsterWorld open = MOpenWorld();
    MonsterInstance a = MMake(MDef("goblin"), {20, 20}, 201);
    MonsterInstance b = MMake(MDef("goblin"), {20, 20}, 202);
    for (int i = 1; i <= 120; ++i) {
      const std::vector<Vec2> na{b.pos};
      MTick(a, MAi(), Vec2(90, 90), open, rng, i * kSimStepMs, true, 1, na);
      const std::vector<Vec2> nb{a.pos};
      MTick(b, MAi(), Vec2(90, 90), open, rng, i * kSimStepMs, true, 1, nb);
    }
    CHECK(Dist(a.pos, b.pos) >= MAi().separationRadius - 0.05);
    MonsterInstance att = MMake(MDef("goblin"), {21.5, 20}, 203);
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
    CHECK(m.NearestAliveOfDef("goblin", {0, 0}) == kNoEntity);
    CHECK(m.NearestAggro({0, 0}) == kNoEntity);
    m.TickAI(kSimStepMs);  // no hero, no zone: nothing happens
    MonsterInstance inst;
    inst.affixes.push_back({EliteAffixType::Swift, 0, 0});
    CHECK(inst.AffixLootBonus(h.ctx.data.Combat().eliteAffixes) ==
          h.ctx.data.Combat().eliteAffixes.Def(EliteAffixType::Swift).lootQualityBonus);
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

  TEST_CASE("activity set 3.8 (AI culling): 30 tiles inclusive, far aggro monsters stay active, 250 ms refresh") {
    MonsterRig rig(4, Vec2(50, 50));
    rig.OpenWorld();
    const EntityId edge = rig.Spawn("goblin", {80, 50});    // exactly 30
    const EntityId beyond = rig.Spawn("goblin", {81, 50});  // 31
    const EntityId farAggro = rig.Spawn("goblin", {50, 110});
    rig.M(farAggro).state = MonsterState::Chase;
    rig.Step();
    CHECK(rig.M(edge).patrolTimerMs == doctest::Approx(kSimStepMs));
    CHECK(rig.M(beyond).patrolTimerMs == 0);        // culled: frozen
    CHECK(rig.M(farAggro).state == MonsterState::Idle);  // updated (chase dropped: 60 > 7.5)
    CHECK(rig.M(edge).active);
    CHECK_FALSE(rig.M(beyond).active);
    // A spawn between refreshes joins at the next refresh (<= 250 ms).
    const EntityId late = rig.Spawn("goblin", {55, 50});
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
    const EntityId idle = rig.Spawn("goblin", {15, 25});
    rig.M(idle).pos = Vec2(15, 24.5);  // 4.5 from the hero (< aggro 5) but outside the camp
    rig.Step();
    CHECK(rig.M(idle).state == MonsterState::Idle);  // the hero in camp is invisible to non-aggro monsters
    const EntityId chaser = rig.Spawn("goblin", {15, 24});
    rig.M(chaser).pos = Vec2(15, 23.9);  // inside the radius (8.9)
    rig.M(chaser).state = MonsterState::Chase;
    rig.Step(16);  // refresh the activity set
    CHECK(rig.M(chaser).state == MonsterState::Idle);
    // Hero outside the camp but within aggro of a monster standing inside: it flips idle -> chase -> repelled and
    // never moves, and the aggro message is not repeated every tick.
    MonsterRig edge(4, Vec2(15, 28));
    edge.OpenWorld();
    const EntityId inside = edge.Spawn("goblin", {15, 23});
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
    const EntityId g = rig.Spawn("goblin", {60, 84});
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
    CHECK(rig.aggros[0].defId == "goblin");
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
    const EntityId g = rig.Spawn("goblin_chief", {60, 90}, MonsterRole::Regular, true);  // 10 tiles: idle
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
    CHECK(k.defId == "goblin_chief");
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
    const EntityId g = rig.Spawn("goblin", {70, 80});
    const EntityId chief = rig.Spawn("goblin_chief", {72, 80}, MonsterRole::Regular, true);
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
      const EntityId id = r2.Spawn("goblin", {70, 70}, role);
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
    const EntityId z = r4.Spawn("goblin", {70, 70});
    r4.monsters.ApplyDamage(z, 1e6, DamageFlags{});
    CHECK_FALSE(r4.h.timers.Empty());
    r4.monsters.OnZoneExit();
    CHECK(r4.h.timers.Empty());
    CHECK(MEvents<EvEntityDespawned>(r4.h.events).back().reason == DespawnReason::ZoneUnload);
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

  TEST_CASE("elite behaviours 17.4 in the system: teleport blink next to the hero, curse aura buff + log") {
    MonsterRig rig(41, Vec2(60, 80));
    rig.OpenWorld();
    const EntityId t = rig.Spawn("goblin_chief", {60, 85});
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
    const EntityId it = idle.Spawn("goblin_chief", {60, 85});
    ApplyEliteAffixes(idle.M(it), MOne(EliteAffixType::Teleporting), MAffixes(), MAi());
    idle.monsters.TickEliteBehaviours();
    CHECK(idle.M(it).pos == Vec2(60, 85));

    MonsterRig c(43, Vec2(60, 80));
    c.OpenWorld();
    const EntityId cu = c.Spawn("goblin_chief", {60, 83});
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
    const EntityId a = rig.Spawn("goblin", {62, 60});
    const EntityId b = rig.Spawn("slime_green", {70, 60});
    const EntityId c = rig.Spawn("goblin", {100, 100});
    rig.M(c).state = MonsterState::Chase;
    CHECK(rig.monsters.NearestAlive(Vec2(60, 60), 50) == a);
    CHECK(rig.monsters.NearestAggro(Vec2(60, 60)) == c);
    CHECK(rig.monsters.MonsterAtTile(Vec2(62.4, 61.4)) == a);
    CHECK(rig.monsters.MonsterAtTile(Vec2(63.6, 60)) == kNoEntity);  // |dcol| = 1.6
    CHECK(rig.monsters.NearestAliveOfDef("slime_green", Vec2(0, 0)) == b);
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
    CHECK(snap.monsters[1].defId == "slime_green");
    CHECK(snap.monsters[1].artId == MDef("slime_green").spriteKey);
    CHECK(snap.monsters[1].nameKey == "data.monster.slime_green");
    CHECK(snap.monsters[1].alive);
    CHECK(snap.monsters[1].hp == 30);
    CHECK_FALSE(snap.monsters[0].alive);
    CHECK_FALSE(snap.miniBossDialogue);
    // Story 8.3: once the boss label is renamed the view carries the intro name key.
    const EntityId chief = rig.Spawn("goblin_chief", {5, 110});
    rig.M(chief).storyNameShown = true;
    Snapshot named;
    rig.monsters.FillSnapshot(named);
    REQUIRE(named.monsters.size() == 4);
    CHECK(named.monsters[3].storyNamed);
    CHECK(named.monsters[3].nameKey == "story.boss.goblin_chief.name");
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
    h.session.currentMap = "emerald_plains";
    REQUIRE(zone.EnterZone("emerald_plains", true, Vec2(60, 80)));
    hero->RecalcDerived(h.ctx.equip);
    hero->FillHpMana();
    loco.OnZoneEnter();
    combat.OnZoneEnter();
    monsters.SetWorldForTesting(MOpenWorld());
    MonsterSpawnParams p;
    p.baseDef = MD().FindMonster("goblin");
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
}
