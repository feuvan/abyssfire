// Monsters-area test fixtures (moved out of monsters_test.cpp so content_ch1_test.cpp shares them): data accessors,
// instance builders, test worlds, the AI tick driver, event helpers and MonsterRig (MonsterSystem over the live
// Chapter 1 zone). MFix wraps a synthetic fixture of TestContent.h.
#pragma once

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
#include "SimHarness.h"
#include "TestContent.h"
#include "doctest/doctest.h"

namespace abyss::test::monsters_fx {

inline const DataStore& MD() { return test::RealData(); }
inline const MonsterAiDef& MAi() { return MD().Monsters().ai; }
inline const EliteAffixTable& MAffixes() { return MD().Combat().eliteAffixes; }
inline const DifficultyTable& MDiff() { return MD().Combat().difficulty; }

inline const MonsterDef& MDef(std::string_view id) {
  const MonsterDef* d = MD().FindMonster(id);
  REQUIRE(d != nullptr);
  return *d;
}

inline const HuntDef& MHunt(std::string_view id) {
  const HuntDef* h = MD().Monsters().FindHunt(id);
  REQUIRE(h != nullptr);
  return *h;
}

// The normal-difficulty definition of a Chapter-1 monster or hunt leader (hunts through makeHuntDefinition).
inline MonsterDef MNormalDef(std::string_view id) {
  if (const HuntDef* h = MD().Monsters().FindHunt(id)) return MakeHuntDefinition(MDef(h->monsterId), *h, MAi(), h->name);
  return MDef(id);
}

inline MonsterInstance MMake(const MonsterDef& def, Vec2 pos, EntityId id = 100) {
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

inline MonsterWorld MOpenWorld(int32_t cols = 120, int32_t rows = 120) {
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

inline MonsterWorld MGridWorld(const std::shared_ptr<MTestGrid>& g, bool withPath) {
  MonsterWorld w;
  w.cols = g->cols;
  w.rows = g->rows;
  w.walkable = [g](int32_t c, int32_t r) { return g->Walk(c, r); };
  if (withPath) w.findPath = [g](Vec2 a, Vec2 b, std::vector<TilePos>& out) { return g->Path(a, b, out); };
  return w;
}

inline MonsterAiResult MTick(MonsterInstance& m, const MonsterAiDef& ai, Vec2 hero, const MonsterWorld& w, Rng& rng,
                             double nowMs, bool visible = true, double speedMul = 1,
                             std::span<const Vec2> neighbours = {}) {
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
inline std::vector<E> MEvents(const EventSink& sink) {
  std::vector<E> out;
  for (const Event& e : sink.Items()) {
    if (const E* p = std::get_if<E>(&e)) out.push_back(*p);
  }
  return out;
}

inline bool MHasLog(const EventSink& sink, std::string_view key) {
  for (const EvLog& l : MEvents<EvLog>(sink)) {
    if (l.text.key == key) return true;
  }
  return false;
}

inline std::vector<EliteAffixType> MOne(EliteAffixType t) { return {t}; }

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
    h.session.currentMap = ch1::kZone;
    ok = zone.EnterZone(ch1::kZone, true, heroPos);
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
    return SpawnDef(MD().FindMonster(defId), tile, role, affixes);
  }
  // A synthetic fixture (TestContent.h).
  EntityId Spawn(const MonsterDef& def, TilePos tile, MonsterRole role = MonsterRole::Regular, bool affixes = false) {
    return SpawnDef(&def, tile, role, affixes);
  }
  EntityId SpawnDef(const MonsterDef* def, TilePos tile, MonsterRole role, bool affixes) {
    MonsterSpawnParams p;
    p.baseDef = def;
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


// A synthetic monster fixture (TestContent.h). The REQUIRE stands where MDef checked the data lookup: a definition with
// stats.
inline const MonsterDef& MFix(const MonsterDef& d) {
  REQUIRE(d.hp > 0);
  return d;
}

}  // namespace abyss::test::monsters_fx
