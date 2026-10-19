// World-area test fixtures (moved out of world_test.cpp so content_ch1_test.cpp shares them): golden-map hashes, grid
// builders, path checks, the camp-layout geometry helpers, WorldEditedData and WorldRig (the world systems over the
// live data) and WorldClearRow.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <vector>

#include "SimHarness.h"
#include "abyss/audio/Audio.h"
#include "abyss/base/Json.h"
#include "abyss/base/Math.h"
#include "abyss/hero/Hero.h"
#include "abyss/hero/Rewards.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/combat/SoulEcho.h"
#include "abyss/save/SaveData.h"
#include "abyss/sim/SimContext.h"
#include "abyss/story/StoryDirector.h"
#include "abyss/sim/GameSim.h"
#include "abyss/sim/GameplayBus.h"
#include "abyss/world/CampLayout.h"
#include "abyss/world/Exploration.h"
#include "abyss/world/Grid.h"
#include "abyss/world/Locomotion.h"
#include "abyss/world/MapGen.h"
#include "abyss/world/Pathfinding.h"
#include "abyss/world/RandomEvents.h"
#include "abyss/world/Zone.h"
#include "doctest/doctest.h"

namespace abyss::test::world_fx {

// ---- SHA-256 (golden tilesSha / decorSha: first 16 hex digits) ------------------------------------------------------
inline std::string WorldSha256Prefix16(const std::string& msg) {
  static constexpr uint32_t k[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98,
      0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
      0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8,
      0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
      0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
      0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
      0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
      0xc67178f2};
  uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  std::vector<uint8_t> data(msg.begin(), msg.end());
  const uint64_t bitLen = static_cast<uint64_t>(data.size()) * 8u;
  data.push_back(0x80);
  while (data.size() % 64 != 56) data.push_back(0);
  for (int i = 7; i >= 0; --i) data.push_back(static_cast<uint8_t>(bitLen >> (i * 8)));
  auto rotr = [](uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); };
  for (size_t off = 0; off < data.size(); off += 64) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
      w[i] = (static_cast<uint32_t>(data[off + i * 4]) << 24) | (static_cast<uint32_t>(data[off + i * 4 + 1]) << 16) |
             (static_cast<uint32_t>(data[off + i * 4 + 2]) << 8) | static_cast<uint32_t>(data[off + i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
      const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; ++i) {
      const uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      const uint32_t ch = (e & f) ^ (~e & g);
      const uint32_t t1 = hh + S1 + ch + k[i] + w[i];
      const uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      const uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
      const uint32_t t2 = S0 + mj;
      hh = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += hh;
  }
  static const char* kHex = "0123456789abcdef";
  std::string out;
  for (int i = 0; i < 2; ++i) {
    for (int s = 28; s >= 0; s -= 4) out.push_back(kHex[(h[i] >> s) & 0xF]);
  }
  return out;
}

inline JsonValue WorldLoadGolden(const std::string& name) {
  JsonValue v;
  const std::string text = test::ReadFile(test::GoldenDir() + "/maps/" + name);
  REQUIRE_FALSE(text.empty());
  REQUIRE(ParseJson(text, v));
  return v;
}

inline std::string WorldTilesSha(const ZoneGrid& g) {
  std::string s;
  for (int32_t r = 0; r < g.Rows(); ++r) {
    if (r > 0) s.push_back(';');
    for (int32_t c = 0; c < g.Cols(); ++c) {
      if (c > 0) s.push_back(',');
      s += std::to_string(static_cast<int>(g.Tile(c, r)));
    }
  }
  return WorldSha256Prefix16(s);
}

inline std::string WorldDecorSha(const ZoneGrid& g) {
  std::string s;
  bool first = true;
  for (const Decoration& d : g.Decorations()) {
    if (!first) s.push_back(';');
    first = false;
    s += std::to_string(static_cast<int>(d.col)) + "," + std::to_string(static_cast<int>(d.row)) + "," + d.type;
  }
  return WorldSha256Prefix16(s);
}

inline int32_t WorldWalkableCount(const ZoneGrid& g) {
  int32_t n = 0;
  for (int32_t r = 0; r < g.Rows(); ++r) {
    for (int32_t c = 0; c < g.Cols(); ++c) n += g.Walkable(c, r) ? 1 : 0;
  }
  return n;
}

// makeGrid (PathfindingAndMaps.test.ts): all walkable except `blocked` (col,row).
inline ZoneGrid WorldOpenGrid(int32_t cols, int32_t rows, const std::vector<TilePos>& blocked = {}) {
  ZoneGrid g(cols, rows);
  for (const TilePos& b : blocked) g.SetTile(b.col, b.row, TileType::Wall);
  return g;
}

// makeGridWithBorders.
inline ZoneGrid WorldBorderGrid(int32_t cols, int32_t rows) {
  ZoneGrid g(cols, rows);
  for (int32_t r = 0; r < rows; ++r) {
    for (int32_t c = 0; c < cols; ++c) {
      if (!(r > 0 && r < rows - 1 && c > 0 && c < cols - 1)) g.SetTile(c, r, TileType::Wall);
    }
  }
  return g;
}

// make120x120Map (PathfindingAndMaps.test.ts).
inline MapDef WorldTest120Map(MapTheme theme = MapTheme::Plains, int32_t seed = 42) {
  MapDef m;
  m.id = "test_120";
  m.cols = 120;
  m.rows = 120;
  m.spawns = {{TilePos{30, 20}, "mob_a", 5}, {TilePos{90, 20}, "mob_b", 5}, {TilePos{30, 100}, "mob_c", 5},
              {TilePos{90, 100}, "mob_d", 5}};
  m.camps = {{TilePos{15, 15}, {"npc_a"}}, {TilePos{100, 100}, {"npc_b"}}};
  m.playerStart = TilePos{15, 20};
  m.exits = {{TilePos{119, 60}, "other", TilePos{1, 60}}};
  m.levelMin = 1;
  m.levelMax = 10;
  m.hasTheme = true;
  m.theme = theme;
  m.seed = seed;
  m.generated = true;
  return m;
}

inline double WorldPathCost(TilePos start, const std::vector<TilePos>& path) {
  double cost = 0;
  TilePos prev = start;
  for (const TilePos& p : path) {
    cost += (p.col != prev.col && p.row != prev.row) ? 1.414 : 1.0;
    prev = p;
  }
  return cost;
}

// Steps are 8-adjacent, walkable and never cut a corner.
inline bool WorldPathValid(const ZoneGrid& g, TilePos start, const std::vector<TilePos>& path) {
  TilePos prev = start;
  for (const TilePos& p : path) {
    const int32_t dc = p.col - prev.col, dr = p.row - prev.row;
    if (std::abs(dc) > 1 || std::abs(dr) > 1 || (dc == 0 && dr == 0)) return false;
    if (!g.Walkable(p.col, p.row)) return false;
    if (dc != 0 && dr != 0 && (!g.Walkable(prev.col + dc, prev.row) || !g.Walkable(prev.col, prev.row + dr))) return false;
    prev = p;
  }
  return true;
}

// 4-connected flood fill from a walkable tile (MapLiquids.test.ts reachable()).
inline std::vector<uint8_t> WorldReachable(const ZoneGrid& g, TilePos start) {
  std::vector<uint8_t> seen(static_cast<size_t>(g.Cols()) * static_cast<size_t>(g.Rows()), 0);
  auto idx = [&g](int32_t c, int32_t r) { return static_cast<size_t>(r) * static_cast<size_t>(g.Cols()) + static_cast<size_t>(c); };
  std::vector<TilePos> stack{start};
  seen[idx(start.col, start.row)] = 1;
  while (!stack.empty()) {
    const TilePos p = stack.back();
    stack.pop_back();
    static constexpr int32_t kD[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto& d : kD) {
      const int32_t nc = p.col + d[0], nr = p.row + d[1];
      if (g.Walkable(nc, nr) && seen[idx(nc, nr)] == 0) {
        seen[idx(nc, nr)] = 1;
        stack.emplace_back(nc, nr);
      }
    }
  }
  return seen;
}

inline const char* const kWorldZoneIds[] = {"emerald_plains", "twilight_forest", "anvil_mountains", "scorching_desert",
                                     "abyss_rift"};

// ---- Camp layout v2 geometry (DECISIONS W11, abyss/world/CampLayout.h) ----------------------------------------------
// Prop bodies in tiles (1 tile = 1 m) = Data/assets.json boundsCm / 100, in the mesh frame: x = mesh +X (across),
// y = mesh +Y = the prop's front (AAbyssZoneActor::BuildCamps yaws it to CampProp.front, or to the camera), z = height.
struct CampTestBody {
  const char* type;
  double minX, maxX, minY, maxY, height;
};
inline constexpr CampTestBody kCampTestBodies[] = {
    {"campfire", -0.53581, 0.52570, -0.49108, 0.49828, 0.28607},  // SM_Prop_Camp_Campfire
    {"well", -0.76051, 0.76051, -0.60697, 0.60697, 1.99629},       // SM_Prop_Camp_Well
    {"banner", -0.17086, 0.63360, -0.13705, 0.18202, 2.60200},     // SM_Prop_Camp_Banner (cloth toward mesh +X)
    {"torch", -0.18190, 0.18190, -0.18884, 0.18884, 1.72000},      // SM_Prop_Camp_Torch
    {"crate", -0.39901, 0.39901, -0.39901, 0.39901, 0.75451},      // SM_Prop_Camp_Crate
    {"barrel", -0.35761, 0.35761, -0.35761, 0.35761, 0.86549},     // SM_Prop_Camp_Barrel
};
// SM_Palisade_Plains_GatePost boundsCm x -14.611..14.570, y -14.585..46.051; AAbyssZoneActor::BuildWalls stands one at
// (camp.x - 1.5, camp.y - 5) and (camp.x + 0.5, camp.y - 5), yaw MeshYawOffsetDeg + 90 = 0 (mesh axes = world axes).
inline constexpr double kCampTestGatePostMinX = -0.14611, kCampTestGatePostMaxX = 0.14570;
inline constexpr double kCampTestGatePostMinY = -0.14585, kCampTestGatePostMaxY = 0.46051;
// SM_Palisade_Plains_Stake_A/B/C: radius <= 13.7 cm with a random yaw; the stakes run along the camp-wall tile centres.
inline constexpr double kCampTestStakeRadius = 0.15;
// NPC body: AAbyssZoneActor OccluderSlackCm 25 cm half width; SK_NPC_* stand 172-176 cm.
inline constexpr double kCampTestNpcRadius = 0.25, kCampTestNpcHeight = 1.75;
inline constexpr double kCampTestNpcClearance = 0.35;  // NPC body to any prop body
// Hero lanes that stay free (camp-relative): the 2-tile gate (x -1 / 0 on row -5) to the fire, and the open +row side's
// x = 0 column to the fire.
inline constexpr double kCampTestGateLane[4] = {-1.25, 0.25, -5.6, -0.5};  // minX, maxX, minY, maxY
inline constexpr double kCampTestSouthLane[4] = {-0.5, 0.5, 0.5, 6.0};

// An oriented rectangle in tile space: local x along V() = front rotated -90 deg (mesh +X), local y along u (front).
struct CampTestBox {
  Vec2 c, u{0.0, 1.0};
  double minX = 0, maxX = 0, minY = 0, maxY = 0, height = 0;
  Vec2 V() const { return {u.y, -u.x}; }
  Vec2 Local(Vec2 p) const { return {(p - c).Dot(V()), (p - c).Dot(u)}; }
  std::array<Vec2, 4> Corners() const {
    const Vec2 v = V();
    return {c + v * minX + u * minY, c + v * maxX + u * minY, c + v * maxX + u * maxY, c + v * minX + u * maxY};
  }
  double Dist(Vec2 p) const {
    const Vec2 l = Local(p);
    const double dx = (std::max)({minX - l.x, 0.0, l.x - maxX});
    const double dy = (std::max)({minY - l.y, 0.0, l.y - maxY});
    return std::sqrt(dx * dx + dy * dy);
  }
};

inline CampTestBox CampTestAxisBox(const double (&b)[4]) {
  CampTestBox r;
  r.minX = b[0];
  r.maxX = b[1];
  r.minY = b[2];
  r.maxY = b[3];
  return r;
}

inline double CampTestSegPointDist(Vec2 p, Vec2 a, Vec2 b) {
  const Vec2 ab = b - a;
  const double l2 = ab.LengthSq();
  const double t = l2 > 0 ? std::clamp((p - a).Dot(ab) / l2, 0.0, 1.0) : 0.0;
  return (p - (a + ab * t)).Length();
}

inline bool CampTestSegsCross(Vec2 a, Vec2 b, Vec2 c, Vec2 d) {
  auto orient = [](Vec2 p, Vec2 q, Vec2 r) { return (q - p).Cross(r - p); };
  return ((orient(a, b, c) > 0) != (orient(a, b, d) > 0)) && ((orient(c, d, a) > 0) != (orient(c, d, b) > 0));
}

inline bool CampTestInsideConvex(std::span<const Vec2> poly, Vec2 p) {
  if (poly.size() < 3) return false;
  bool pos = false, neg = false;
  for (size_t i = 0; i < poly.size(); ++i) {
    const double cr = (poly[(i + 1) % poly.size()] - poly[i]).Cross(p - poly[i]);
    pos = pos || cr > 0;
    neg = neg || cr < 0;
  }
  return !(pos && neg);
}

// Distance between two convex polygons (a 2-point polygon is a segment); 0 when they touch or overlap.
inline double CampTestPolyDist(std::span<const Vec2> p, std::span<const Vec2> q) {
  for (size_t i = 0; i < p.size(); ++i) {
    for (size_t j = 0; j < q.size(); ++j) {
      if (CampTestSegsCross(p[i], p[(i + 1) % p.size()], q[j], q[(j + 1) % q.size()])) return 0.0;
    }
  }
  if (CampTestInsideConvex(p, q[0]) || CampTestInsideConvex(q, p[0])) return 0.0;
  double best = 1e9;
  for (size_t i = 0; i < p.size(); ++i) {
    for (const Vec2& v : q) best = (std::min)(best, CampTestSegPointDist(v, p[i], p[(i + 1) % p.size()]));
  }
  for (size_t j = 0; j < q.size(); ++j) {
    for (const Vec2& v : p) best = (std::min)(best, CampTestSegPointDist(v, q[j], q[(j + 1) % q.size()]));
  }
  return best;
}

inline double CampTestBoxDist(const CampTestBox& a, const CampTestBox& b) {
  const std::array<Vec2, 4> ca = a.Corners(), cb = b.Corners();
  return CampTestPolyDist(ca, cb);
}

// A camp prop's body (its manifest bounds), camp-relative. Props without a front face the camera (front = -camera
// forward).
inline CampTestBox CampTestPropBox(const CampProp& p, Vec2 campCentre, Vec2 towardCamera) {
  CampTestBox b;
  b.c = p.centre - campCentre;
  b.u = p.front == Vec2() ? towardCamera : p.front;
  for (const CampTestBody& body : kCampTestBodies) {
    if (p.type != body.type) continue;
    b.minX = body.minX;
    b.maxX = body.maxX;
    b.minY = body.minY;
    b.maxY = body.maxY;
    b.height = body.height;
  }
  return b;
}


// ---- runtime: zone, locomotion, exploration, random events and the audio rules --------------------------------------

inline const DataStore& WD() { return test::RealData(); }

// The world systems around a SimHarness, stepped in the world 17 order (hero update -> combat-state slot (random
// events) -> exits / exploration).
// The real tables with one file replaced by `edit(parsed JSON)` (data switches and missing-data fallbacks).
template <class Edit>
inline std::unique_ptr<DataStore> WorldEditedData(std::string_view fileName, Edit edit) {
  JsonValue doc;
  REQUIRE(ParseJson(test::ReadFile(test::DataDir() + "/" + std::string(fileName)), doc));
  edit(doc);
  const std::string edited = WriteJson(doc);
  auto store = std::make_unique<DataStore>();
  DataLoadReport report;
  const bool ok = store->LoadAll(
      [&](std::string_view name, std::string& out) {
        out = name == fileName ? edited : test::ReadFile(test::DataDir() + "/" + std::string(name));
        return !out.empty();
      },
      report);
  INFO(report.Summary(5));
  REQUIRE(ok);
  return store;
}

struct WorldRig {
  explicit WorldRig(uint64_t seed = 5, bool milestone1 = true, std::string_view mapId = "", bool hasTarget = false,
                    Vec2 target = {}, const DataStore& data = test::RealData())
      : h(seed, data),
        hero(std::make_unique<Hero>(h.ctx.data, ClassId::Warrior)),
        zone(h.ctx),
        loco(h.ctx),
        explore(h.ctx),
        events(h.ctx),
        rewards(h.ctx),
        audio(h.ctx) {
    h.config.milestone1 = milestone1;
    SimSystems& s = h.ctx.sys;
    s.hero = hero.get();
    s.zone = &zone;
    s.locomotion = &loco;
    s.exploration = &explore;
    s.randomEvents = &events;
    s.rewards = &rewards;
    h.onTimer = [this](const Timer& t) {
      if (t.owner != TimerOwner::World) return;
      if (t.kind >= kRandomEventTimerKindBase) {
        events.OnTimer(t);
      } else {
        zone.OnTimer(t);
      }
    };
    // SimWiring's world hooks (W3: movement input cancels the portal channel when townPortal.cancelOnMove is on).
    h.bus.Subscribe<HeroMoveInputMsg>([this](const HeroMoveInputMsg&) { zone.OnHeroMoveInput(); });
    const std::string map = mapId.empty() ? WD().World().defaultMap : std::string(mapId);
    h.session.currentMap = map;
    ok = zone.EnterZone(map, hasTarget, target);
    hero->Skills().InitStarterLevels();
    hero->RecalcDerived(h.ctx.equip);
    hero->FillHpMana();
    loco.OnZoneEnter();
    explore.OnZoneEnter();
    events.OnZoneEnter();
    h.events.Clear();
  }

  void Step(int n = 1) {
    for (int i = 0; i < n; ++i) {
      h.Step(1);
      loco.Tick(kSimStepMs);
      events.Tick();
      zone.Tick();
      explore.Tick();
    }
  }
  template <class Pred>
  bool StepUntil(Pred pred, int maxSteps = 2000) {
    for (int i = 0; i < maxSteps; ++i) {
      if (pred()) return true;
      Step();
    }
    return pred();
  }
  void Place(Vec2 p) { loco.Teleport(p, TeleportReason::Debug); }
  template <class E>
  size_t Count() const {
    return test::CountEvents<E>(h.events);
  }
  template <class E>
  const E* Last() const {
    const E* out = nullptr;
    for (const Event& e : h.events.Items()) {
      if (const E* p = std::get_if<E>(&e)) out = p;
    }
    return out;
  }
  size_t SfxCount(SfxId cue) const {
    size_t n = 0;
    for (const Event& e : h.events.Items()) {
      if (const EvSfx* p = std::get_if<EvSfx>(&e); p != nullptr && p->cue == cue) ++n;
    }
    return n;
  }
  bool Logged(std::string_view key) const {
    for (const Event& e : h.events.Items()) {
      if (const EvLog* p = std::get_if<EvLog>(&e); p != nullptr && p->text.key == key) return true;
    }
    return false;
  }
  // The first log line with that key (nullptr when none) and its position in the event list (`index`).
  const EvLog* FindLog(std::string_view key, size_t* index = nullptr) const {
    const std::span<const Event> items = h.events.Items();
    for (size_t i = 0; i < items.size(); ++i) {
      if (const EvLog* p = std::get_if<EvLog>(&items[i]); p != nullptr && p->text.key == key) {
        if (index != nullptr) *index = i;
        return p;
      }
    }
    return nullptr;
  }

  test::SimHarness h;
  std::unique_ptr<Hero> hero;
  ZoneRuntime zone;
  HeroLocomotion loco;
  ExplorationSystem explore;
  RandomEventSystem events;
  RewardService rewards;
  AudioDirector audio;
  bool ok = false;
};

// A walkable row segment (c .. c + len - 1, r) with walkable rows above and below (rounding slack), out of every
// camp's safe zone and at least 6 tiles from any NPC, scanning from (fromCol, fromRow): the runtime tests run on the
// real map, so they look the segment up instead of hardcoding it.
inline TilePos WorldClearRow(const ZoneRuntime& z, int32_t len, int32_t fromCol = 30, int32_t fromRow = 30) {
  for (int32_t r = fromRow; r < z.Grid().Rows() - 2; ++r) {
    for (int32_t c = fromCol; c + len < z.Grid().Cols() - 1; ++c) {
      bool clear = true;
      for (int32_t i = 0; i < len && clear; ++i) {
        for (int32_t dr = -1; dr <= 1 && clear; ++dr) clear = z.Walkable(c + i, r + dr);
        const Vec2 p(c + i, r);
        clear = clear && !z.InSafeZone(p);
        for (const NpcPlacement& n : z.Npcs()) clear = clear && DistSq(p, n.pos) >= 36.0;
      }
      if (clear) return TilePos{c, r};
    }
  }
  return TilePos{-1, -1};
}

}  // namespace abyss::test::world_fx
