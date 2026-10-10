// World area (+ audio): grid, map generator, pathfinding, exploration, zone runtime, locomotion, random events,
// music / SFX rules. Spec vectors: world-map-nav.md section 21 (golden maps in CoreTests/golden/maps), audio.md 9.8.
// Web suites ported: PathfindingAndMaps.test.ts, MapLiquids.test.ts, FogOfWarOptimization.test.ts,
// RandomEventSystem.test.ts, random-events-scrutiny-fix.test.ts (core parts).
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "SimHarness.h"
#include "abyss/audio/Audio.h"
#include "abyss/base/Json.h"
#include "abyss/base/Math.h"
#include "abyss/hero/Hero.h"
#include "abyss/sim/GameSim.h"
#include "abyss/world/Exploration.h"
#include "abyss/world/Grid.h"
#include "abyss/world/Locomotion.h"
#include "abyss/world/MapGen.h"
#include "abyss/world/Pathfinding.h"
#include "doctest/doctest.h"

using namespace abyss;

namespace {

// ---- SHA-256 (golden tilesSha / decorSha: first 16 hex digits) -------------------------------------------------------
std::string WorldSha256Prefix16(const std::string& msg) {
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

JsonValue WorldLoadGolden(const std::string& name) {
  JsonValue v;
  const std::string text = test::ReadFile(test::GoldenDir() + "/maps/" + name);
  REQUIRE_FALSE(text.empty());
  REQUIRE(ParseJson(text, v));
  return v;
}

std::string WorldTilesSha(const ZoneGrid& g) {
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

std::string WorldDecorSha(const ZoneGrid& g) {
  std::string s;
  bool first = true;
  for (const Decoration& d : g.Decorations()) {
    if (!first) s.push_back(';');
    first = false;
    s += std::to_string(static_cast<int>(d.col)) + "," + std::to_string(static_cast<int>(d.row)) + "," + d.type;
  }
  return WorldSha256Prefix16(s);
}

int32_t WorldWalkableCount(const ZoneGrid& g) {
  int32_t n = 0;
  for (int32_t r = 0; r < g.Rows(); ++r) {
    for (int32_t c = 0; c < g.Cols(); ++c) n += g.Walkable(c, r) ? 1 : 0;
  }
  return n;
}

// makeGrid (PathfindingAndMaps.test.ts): all walkable except `blocked` (col,row).
ZoneGrid WorldOpenGrid(int32_t cols, int32_t rows, const std::vector<TilePos>& blocked = {}) {
  ZoneGrid g(cols, rows);
  for (const TilePos& b : blocked) g.SetTile(b.col, b.row, TileType::Wall);
  return g;
}

// makeGridWithBorders.
ZoneGrid WorldBorderGrid(int32_t cols, int32_t rows) {
  ZoneGrid g(cols, rows);
  for (int32_t r = 0; r < rows; ++r) {
    for (int32_t c = 0; c < cols; ++c) {
      if (!(r > 0 && r < rows - 1 && c > 0 && c < cols - 1)) g.SetTile(c, r, TileType::Wall);
    }
  }
  return g;
}

// make120x120Map (PathfindingAndMaps.test.ts).
MapDef WorldTest120Map(MapTheme theme = MapTheme::Plains, int32_t seed = 42) {
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

double WorldPathCost(TilePos start, const std::vector<TilePos>& path) {
  double cost = 0;
  TilePos prev = start;
  for (const TilePos& p : path) {
    cost += (p.col != prev.col && p.row != prev.row) ? 1.414 : 1.0;
    prev = p;
  }
  return cost;
}

// Steps are 8-adjacent, walkable and never cut a corner.
bool WorldPathValid(const ZoneGrid& g, TilePos start, const std::vector<TilePos>& path) {
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
std::vector<uint8_t> WorldReachable(const ZoneGrid& g, TilePos start) {
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

const char* const kWorldZoneIds[] = {"emerald_plains", "twilight_forest", "anvil_mountains", "scorching_desert",
                                     "abyss_rift"};

}  // namespace

TEST_SUITE("world") {
  // ---- 4.1 SeededRandom ------------------------------------------------------------------------------------------------
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
    CHECK(r.Draws() == 5);
    ParkMillerRng z(0);  // state <= 0 -> += 2147483646
    CHECK(z.State() == 2147483646);
  }

  TEST_CASE("SeededRandom / nextInt golden vectors (_vectors.json)") {
    const JsonValue v = WorldLoadGolden("_vectors.json");
    const JsonValue& list = v.Get("seededRandom");
    REQUIRE(list.Size() == 6);
    for (const JsonValue& e : list.Items()) {
      ParkMillerRng r(e.Get("seed").AsInt64());
      CAPTURE(e.Get("seed").AsInt64());
      for (size_t i = 0; i < 8; ++i) {
        const double x = r.Next();
        CHECK(r.State() == e.Get("states").At(i).AsInt64());
        CHECK(x == e.Get("values").At(i).AsDouble());
      }
      for (size_t i = 0; i < 8; ++i) CHECK(r.NextInt(-3, 7) == e.Get("nextIntMinus3To7").At(i).AsInt());
    }
  }

  TEST_CASE("groveNoise golden vectors (world 4.5 + _vectors.json)") {
    CHECK(GroveNoise(0, 0, 12345, 9) == 0.29843593621626496);
    CHECK(GroveNoise(2, 2, 12345, 9) == 0.32999298519119385);
    CHECK(GroveNoise(10, 7, 12345, 9) == 0.32898032832131274);
    CHECK(GroveNoise(50, 50, 12345, 9) == 0.34534253995646536);
    CHECK(GroveNoise(119, 3, 12345, 9) == 0.53900245003280522);
    CHECK(GroveNoise(10, 10, 12476, 3.5) == 0.28213746321251321);
    CHECK(GroveNoise(40, 100, 12476, 3.5) == 0.58164606030258292);
    const JsonValue v = WorldLoadGolden("_vectors.json");
    const JsonValue& rows = v.Get("groveNoise").Get("rows");
    REQUIRE(rows.Size() > 0);
    for (const JsonValue& r : rows.Items()) {
      CHECK(GroveNoise(r.At(0).AsDouble(), r.At(1).AsDouble(), r.At(2).AsDouble(), r.At(3).AsDouble()) ==
            r.At(4).AsDouble());
    }
  }

  TEST_CASE("pickWeighted golden picks (world 4.7)") {
    const JsonValue v = WorldLoadGolden("_vectors.json");
    std::vector<WeightedDecor> pool;
    for (const JsonValue& e : v.Get("pickWeighted").Get("pool").Items()) {
      pool.push_back(WeightedDecor{std::string(e.At(0).AsString()), e.At(1).AsDouble()});
    }
    for (const JsonValue& p : v.Get("pickWeighted").Get("picks").Items()) {
      CHECK(PickWeighted(pool, p.At(0).AsDouble()) == p.At(1).AsString());
    }
  }

  // ---- 4.9 golden maps -------------------------------------------------------------------------------------------------
  TEST_CASE("MapGen reproduces every golden map exactly (tiles, decorations, hashes, walkable counts)") {
    const DataStore& data = test::RealData();
    for (const char* id : {"emerald_plains", "twilight_forest", "anvil_mountains", "scorching_desert", "abyss_rift",
                           "ember_tower"}) {
      CAPTURE(id);
      const JsonValue golden = WorldLoadGolden(std::string(id) + ".json");
      const MapDef* map = data.FindMap(id);
      REQUIRE(map != nullptr);
      const ZoneGrid g = BuildRawZoneGrid(data, *map);
      REQUIRE(g.Cols() == golden.Get("cols").AsInt());
      REQUIRE(g.Rows() == golden.Get("rows").AsInt());
      // tiles row by row (first mismatching row reported)
      const JsonValue& rows = golden.Get("tiles");
      REQUIRE(rows.Size() == static_cast<size_t>(g.Rows()));
      int32_t badRows = 0;
      for (int32_t r = 0; r < g.Rows(); ++r) {
        std::string row;
        for (int32_t c = 0; c < g.Cols(); ++c) row.push_back(static_cast<char>('0' + static_cast<int>(g.Tile(c, r))));
        if (row != rows.At(static_cast<size_t>(r)).AsString()) {
          if (badRows == 0) {
            CAPTURE(r);
            CHECK(row == rows.At(static_cast<size_t>(r)).AsString());
          }
          ++badRows;
        }
      }
      CHECK(badRows == 0);
      const JsonValue& decor = golden.Get("decorations");
      CHECK(g.Decorations().size() == decor.Size());
      size_t badDecor = 0;
      for (size_t i = 0; i < (std::min)(decor.Size(), g.Decorations().size()); ++i) {
        const Decoration& d = g.Decorations()[i];
        const JsonValue& e = decor.At(i);
        if (d.col != e.At(0).AsDouble() || d.row != e.At(1).AsDouble() || d.type != e.At(2).AsString()) ++badDecor;
      }
      CHECK(badDecor == 0);
      const JsonValue& gold = golden.Get("golden");
      CHECK(WorldTilesSha(g) == gold.Get("tilesSha").AsString());
      CHECK(WorldDecorSha(g) == gold.Get("decorSha").AsString());
      CHECK(WorldWalkableCount(g) == gold.Get("walkable").AsInt());
    }
  }

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

  TEST_CASE("camp stamp layout (world 3.4 / 21 item 2)") {
    const DataStore& data = test::RealData();
    const ZoneGrid g = BuildRawZoneGrid(data, *data.FindMap("emerald_plains"));
    // Camp 1 (15,15): ground 10..20; gate (14,10),(15,10); palisade rows 10..18 at cols 10 / 20; rows 19-20 open.
    CHECK(g.Tile(14, 10) == TileType::Camp);
    CHECK(g.Tile(15, 10) == TileType::Camp);
    CHECK(g.Tile(13, 10) == TileType::CampWall);
    CHECK(g.Tile(16, 10) == TileType::CampWall);
    for (int32_t r = 10; r <= 18; ++r) {
      CHECK(g.Tile(10, r) == TileType::CampWall);
      CHECK(g.Tile(20, r) == TileType::CampWall);
    }
    for (int32_t r = 19; r <= 20; ++r) {
      CHECK(g.Tile(10, r) == TileType::Camp);
      CHECK(g.Tile(20, r) == TileType::Camp);
    }
    for (int32_t c = 10; c <= 20; ++c) CHECK(g.Tile(c, 20) == TileType::Camp);
    int32_t palisade = 0;
    for (TileType t : g.Tiles()) palisade += t == TileType::CampWall ? 1 : 0;
    CHECK(palisade == 50);  // 25 per camp
    // Camp 2 (95,100) gate.
    CHECK(g.Tile(94, 95) == TileType::Camp);
    CHECK(g.Tile(95, 95) == TileType::Camp);
  }

  TEST_CASE("camp blockers make exactly the 8 barrel / crate tiles unwalkable (world 3.4)") {
    const DataStore& data = test::RealData();
    const MapDef& map = *data.FindMap("emerald_plains");
    ZoneGrid g = BuildRawZoneGrid(data, map);
    REQUIRE(WorldWalkableCount(g) == 13167);
    // 3.4 parity: only barrels and crates.
    int32_t barrelsAndCrates = 0;
    for (const CampProp& p : CampProps(map)) {
      if (p.type == "barrel" || p.type == "crate") {
        CHECK(p.blocking);
        g.SetWalkable(p.pos.col, p.pos.row, false);
        ++barrelsAndCrates;
      }
    }
    CHECK(barrelsAndCrates == 8);
    CHECK(WorldWalkableCount(g) == 13159);
    for (const TilePos t : {TilePos{13, 15}, TilePos{17, 15}, TilePos{12, 12}, TilePos{18, 12}, TilePos{93, 100},
                            TilePos{97, 100}, TilePos{92, 97}, TilePos{98, 97}}) {
      CHECK_FALSE(g.Walkable(t.col, t.row));
    }
    // W5 camp props: the well and the tents without an NPC in front block too; ApplyCampBlockers is idempotent.
    ZoneGrid full = BuildRawZoneGrid(data, map);
    ApplyCampBlockers(full, map);
    const int32_t once = WorldWalkableCount(full);
    ApplyCampBlockers(full, map);
    CHECK(WorldWalkableCount(full) == once);
    CHECK_FALSE(full.Walkable(16, 14));  // well (+1,-1)
    CHECK(full.Walkable(12, 13));        // tent (-3,-2): blacksmith stands there
    CHECK(full.Walkable(18, 13));        // tent (+3,-2): merchant
    CHECK_FALSE(full.Walkable(13, 17));  // tent (-2,+2)
    CHECK_FALSE(full.Walkable(17, 17));  // tent (+2,+2)
    CHECK(full.Walkable(15, 15));        // campfire: the hero respawns on it
    // Camp 2 has one NPC (slot 0): its (+3,-2) tent blocks.
    CHECK(full.Walkable(92, 98));
    CHECK_FALSE(full.Walkable(98, 98));
  }

  TEST_CASE("W5: tall decorations block walking, anchors stay walkable, every POI stays reachable") {
    const DataStore& data = test::RealData();
    for (const char* id : kWorldZoneIds) {
      CAPTURE(id);
      const MapDef& map = *data.FindMap(id);
      const ZoneGrid raw = BuildRawZoneGrid(data, map);
      const ZoneGrid g = BuildZoneGrid(data, map);
      int32_t tall = 0;
      for (const Decoration& d : g.Decorations()) {
        if (!d.blocking) {
          CHECK(g.Walkable(static_cast<int32_t>(d.col), static_cast<int32_t>(d.row)) ==
                raw.Walkable(static_cast<int32_t>(d.col), static_cast<int32_t>(d.row)));
          continue;
        }
        ++tall;
      }
      CHECK(tall > 0);
      CHECK(WorldWalkableCount(g) < WorldWalkableCount(raw));
      CHECK(g.Walkable(map.playerStart.col, map.playerStart.row));
      const std::vector<uint8_t> seen = WorldReachable(g, map.playerStart);
      auto reach = [&](TilePos p) {
        return seen[static_cast<size_t>(p.row) * static_cast<size_t>(g.Cols()) + static_cast<size_t>(p.col)] != 0;
      };
      for (const MapCampDef& c : map.camps) CHECK(reach(c.pos));
      for (const MapSpawnDef& s : map.spawns) CHECK(reach(s.pos));
      for (const FieldNpcDef& n : map.fieldNpcs) CHECK(reach(n.pos));
      for (const StoryDecorationDef& d : map.storyDecorations) CHECK(reach(d.pos));
      for (const SubDungeonEntranceDef& e : map.subDungeonEntrances) CHECK(reach(e.pos));
      for (const AvoidPointDef& a : map.generatorAvoid) CHECK(reach(a.pos));
      for (const HiddenAreaDef& h : map.hiddenAreas) {
        for (const HiddenRewardDef& r : h.rewards) CHECK(reach(r.pos));
      }
      for (const MapExitDef& e : map.exits) {
        const int32_t ic = e.pos.col == 0 ? 1 : e.pos.col == map.cols - 1 ? map.cols - 2 : e.pos.col;
        const int32_t ir = e.pos.row == 0 ? 1 : e.pos.row == map.rows - 1 ? map.rows - 2 : e.pos.row;
        CHECK(reach(TilePos{ic, ir}));
      }
    }
  }

  // ---- MapLiquids.test.ts ----------------------------------------------------------------------------------------------
  TEST_CASE("generated water / lava pools block movement; every POI reachable on the raw map (MapLiquids)") {
    const DataStore& data = test::RealData();
    for (const char* id : kWorldZoneIds) {
      CAPTURE(id);
      const MapDef& map = *data.FindMap(id);
      const ZoneGrid g = BuildRawZoneGrid(data, map);
      int32_t liquid = 0;
      for (int32_t r = 0; r < g.Rows(); ++r) {
        for (int32_t c = 0; c < g.Cols(); ++c) {
          if (g.Tile(c, r) == TileType::Water) {
            ++liquid;
            CHECK_FALSE(g.Walkable(c, r));
          }
        }
      }
      CHECK(liquid > 40);
      const std::vector<uint8_t> seen = WorldReachable(g, map.playerStart);
      auto reach = [&](TilePos p) {
        return seen[static_cast<size_t>(p.row) * static_cast<size_t>(g.Cols()) + static_cast<size_t>(p.col)] != 0;
      };
      for (const MapCampDef& c : map.camps) CHECK(reach(c.pos));
      for (const MapSpawnDef& s : map.spawns) CHECK(reach(s.pos));
      for (const FieldNpcDef& n : map.fieldNpcs) CHECK(reach(n.pos));
      for (const SubDungeonEntranceDef& e : map.subDungeonEntrances) CHECK(reach(e.pos));
      for (const StoryDecorationDef& d : map.storyDecorations) CHECK(reach(d.pos));
      for (const AvoidPointDef& a : map.generatorAvoid) CHECK(reach(a.pos));
    }
  }

  // ---- PathfindingAndMaps.test.ts: zone map validation -----------------------------------------------------------------
  TEST_CASE("zone maps: 120x120, anchors in bounds, exits on the border, spawns cover all quadrants") {
    const DataStore& data = test::RealData();
    for (const char* id : kWorldZoneIds) {
      CAPTURE(id);
      const MapDef& map = *data.FindMap(id);
      CHECK(map.cols == 120);
      CHECK(map.rows == 120);
      const ZoneGrid g = BuildRawZoneGrid(data, map);
      CHECK(map.playerStart.col >= 1);
      CHECK(map.playerStart.col < 120);
      CHECK(map.playerStart.row >= 1);
      CHECK(map.playerStart.row < 120);
      CHECK(g.Walkable(map.playerStart.col, map.playerStart.row));
      for (const MapSpawnDef& s : map.spawns) {
        CHECK(s.pos.col >= 1);
        CHECK(s.pos.col < 119);
        CHECK(s.pos.row >= 1);
        CHECK(s.pos.row < 119);
        CHECK(g.Walkable(s.pos.col, s.pos.row));
      }
      for (const MapCampDef& c : map.camps) {
        CHECK(c.pos.col >= 6);
        CHECK(c.pos.col <= 113);
        CHECK(c.pos.row >= 6);
        CHECK(c.pos.row <= 113);
      }
      bool q[4] = {false, false, false, false};
      for (const MapSpawnDef& s : map.spawns) q[(s.pos.col >= 60 ? 1 : 0) + (s.pos.row >= 60 ? 2 : 0)] = true;
      CHECK((q[0] && q[1] && q[2] && q[3]));
      Pathfinder pf(g);
      for (const MapExitDef& e : map.exits) {
        CHECK((e.pos.col == 0 || e.pos.col == 119 || e.pos.row == 0 || e.pos.row == 119));
        const MapDef* target = data.FindMap(e.targetMap);
        REQUIRE(target != nullptr);
        CHECK(e.target.col >= 0);
        CHECK(e.target.col < target->cols);
        CHECK(e.target.row >= 0);
        CHECK(e.target.row < target->rows);
        const int32_t ic = e.pos.col == 0 ? 1 : e.pos.col == map.cols - 1 ? map.cols - 2 : e.pos.col;
        const int32_t ir = e.pos.row == 0 ? 1 : e.pos.row == map.rows - 1 ? map.rows - 2 : e.pos.row;
        std::vector<TilePos> path;
        CHECK(pf.FindPath(map.playerStart.col, map.playerStart.row, ic, ir, path));
        // ... and still after the camp blockers and W5.
        const ZoneGrid full = BuildZoneGrid(data, map);
        Pathfinder pf2(full);
        CHECK(pf2.FindPath(map.playerStart.col, map.playerStart.row, ic, ir, path));
      }
    }
  }

  TEST_CASE("MapGenerator 120x120 generation validity (make120x120Map)") {
    const DataStore& data = test::RealData();
    const MapDef m = WorldTest120Map();
    MapGenInput in;
    in.map = &m;
    const ZoneGrid g = GenerateMap(data.World().mapGen, in);
    REQUIRE(g.Cols() == 120);
    REQUIRE(g.Rows() == 120);
    for (int32_t c = 0; c < 120; ++c) {
      CHECK(g.Tile(c, 0) == TileType::Wall);
      CHECK(g.Tile(c, 119) == TileType::Wall);
      CHECK_FALSE(g.Walkable(c, 0));
      CHECK_FALSE(g.Walkable(c, 119));
    }
    for (int32_t r = 0; r < 120; ++r) {
      CHECK(g.Tile(0, r) == TileType::Wall);
      CHECK(g.Tile(119, r) == TileType::Wall);
    }
    CHECK(g.Walkable(15, 20));
    Pathfinder pf(g);
    std::vector<TilePos> path;
    CHECK(pf.FindPath(15, 20, 118, 60, path));
    bool q[4] = {false, false, false, false};
    for (const Decoration& d : g.Decorations()) q[(d.col >= 60 ? 1 : 0) + (d.row >= 60 ? 2 : 0)] = true;
    CHECK((q[0] && q[1] && q[2] && q[3]));
    const ZoneGrid again = GenerateMap(data.World().mapGen, in);
    CHECK(again.Tiles() == g.Tiles());
    CHECK(again.Walk() == g.Walk());
    for (size_t i = 0; i + 1 < m.camps.size(); ++i) {
      CHECK(pf.FindPath(m.camps[i].pos.col, m.camps[i].pos.row, m.camps[i + 1].pos.col, m.camps[i + 1].pos.row, path));
    }
    for (const MapSpawnDef& s : m.spawns) CHECK(pf.FindPath(15, 20, s.pos.col, s.pos.row, path));
    for (MapTheme theme : {MapTheme::Plains, MapTheme::Forest, MapTheme::Mountain, MapTheme::Desert, MapTheme::Abyss}) {
      const MapDef tm = WorldTest120Map(theme, 99999 + static_cast<int32_t>(EnumName(theme).size()));
      MapGenInput ti;
      ti.map = &tm;
      const ZoneGrid tg = GenerateMap(data.World().mapGen, ti);
      CHECK(tg.Rows() == 120);
      CHECK(tg.Walkable(15, 20));
    }
  }

  // ---- 5 A* ------------------------------------------------------------------------------------------------------------
  TEST_CASE("A* correctness (PathfindingAndMaps.test.ts)") {
    std::vector<TilePos> path;
    {
      const ZoneGrid g = WorldOpenGrid(20, 20);
      Pathfinder pf(g);
      REQUIRE(pf.FindPath(0, 0, 5, 0, path));
      CHECK(path.back() == TilePos{5, 0});
      REQUIRE(pf.FindPath(0, 0, 5, 5, path));
      CHECK(path.back() == TilePos{5, 5});
      CHECK(path.size() == 5);
      REQUIRE(pf.FindPath(0, 0, 15, 12, path));
      CHECK(WorldPathValid(g, {0, 0}, path));
      REQUIRE(pf.FindPath(0, 0, 10, 5, path));
      CHECK(path.size() == 10);
    }
    {
      std::vector<TilePos> blocked;
      for (int32_t r = 0; r <= 7; ++r) blocked.emplace_back(5, r);
      for (int32_t c = 5; c <= 9; ++c) blocked.emplace_back(c, 7);
      const ZoneGrid g = WorldOpenGrid(15, 15, blocked);
      Pathfinder pf(g);
      REQUIRE(pf.FindPath(0, 0, 10, 0, path));
      CHECK(path.back() == TilePos{10, 0});
      CHECK(WorldPathValid(g, {0, 0}, path));
    }
  }

  TEST_CASE("A* edge cases (PathfindingAndMaps.test.ts)") {
    std::vector<TilePos> path{{9, 9}};
    const ZoneGrid open = WorldOpenGrid(10, 10);
    Pathfinder pf(open);
    CHECK_FALSE(pf.FindPath(3, 3, 3, 3, path));
    CHECK(path.empty());
    CHECK_FALSE(pf.FindPath(0, 0, 15, 15, path));
    CHECK_FALSE(pf.FindPath(-1, -1, 5, 5, path));
    {
      const ZoneGrid g = WorldOpenGrid(10, 10, {{5, 5}});
      Pathfinder p(g);
      CHECK_FALSE(p.FindPath(0, 0, 5, 5, path));
    }
    {
      std::vector<TilePos> ring;
      for (int32_t dc = -1; dc <= 1; ++dc) {
        for (int32_t dr = -1; dr <= 1; ++dr) {
          if (dc != 0 || dr != 0) ring.emplace_back(5 + dc, 5 + dr);
        }
      }
      const ZoneGrid g = WorldOpenGrid(10, 10, ring);
      Pathfinder p(g);
      CHECK_FALSE(p.FindPath(0, 0, 5, 5, path));
    }
    {
      const ZoneGrid g = WorldOpenGrid(10, 10, {{3, 2}, {2, 3}});
      Pathfinder p(g);
      if (p.FindPath(2, 2, 3, 3, path)) CHECK(WorldPathValid(g, {2, 2}, path));
      // the direct diagonal is never taken
      CHECK((path.empty() || path.front() != TilePos{3, 3}));
    }
    REQUIRE(pf.FindPath(0.4, 0.6, 3.7, 2.3, path));
    CHECK(path.back() == TilePos{4, 2});
    {
      std::vector<TilePos> blocked;
      for (int32_t r = 1; r < 5; ++r) {
        for (int32_t c = 0; c < 10; ++c) blocked.emplace_back(c, r);
      }
      const ZoneGrid g = WorldOpenGrid(10, 5, blocked);
      Pathfinder p(g);
      REQUIRE(p.FindPath(0, 0, 9, 0, path));
      CHECK(path.size() == 9);
      CHECK(path.back() == TilePos{9, 0});
    }
    {
      const ZoneGrid g = WorldOpenGrid(10, 10, {{0, 0}});
      Pathfinder p(g);
      CHECK_FALSE(p.FindPath(0, 0, 5, 5, path));
    }
    // JS Math.round on negative halves: -0.5 -> 0 (in bounds).
    REQUIRE(pf.FindPath(-0.5, -0.5, 2, 0, path));
    CHECK(path.back() == TilePos{2, 0});
  }

  TEST_CASE("A* performance, maze and determinism (PathfindingAndMaps.test.ts)") {
    ZoneGrid g = WorldBorderGrid(120, 120);
    double s = 54321;
    auto random = [&s]() {
      s = std::fmod(s * 16807.0, 2147483647.0);
      return (s - 1) / 2147483646.0;
    };
    for (int32_t r = 2; r < 118; ++r) {
      for (int32_t c = 2; c < 118; ++c) {
        if (random() < 0.10) g.SetWalkable(c, r, false);
      }
    }
    Pathfinder pf(g);
    int found = 0;
    std::vector<TilePos> path;
    for (int i = 0; i < 100; ++i) {
      int32_t sc, sr, ec, er;
      do {
        sc = static_cast<int32_t>(std::floor(random() * 118)) + 1;
        sr = static_cast<int32_t>(std::floor(random() * 118)) + 1;
      } while (!g.Walkable(sc, sr));
      do {
        ec = static_cast<int32_t>(std::floor(random() * 118)) + 1;
        er = static_cast<int32_t>(std::floor(random() * 118)) + 1;
      } while (!g.Walkable(ec, er));
      if (pf.FindPath(sc, sr, ec, er, path)) {
        ++found;
        CHECK(WorldPathValid(g, {sc, sr}, path));
        CHECK(path.back() == TilePos{ec, er});
      }
    }
    CHECK(found > 50);
    const ZoneGrid open = WorldBorderGrid(120, 120);
    Pathfinder po(open);
    REQUIRE(po.FindPath(1, 1, 118, 118, path));
    CHECK(path.back() == TilePos{118, 118});
    CHECK(path.size() == 117);

    std::vector<TilePos> blocked;
    for (int32_t r = 0; r <= 15; ++r) blocked.emplace_back(5, r);
    for (int32_t r = 4; r <= 19; ++r) blocked.emplace_back(10, r);
    for (int32_t r = 0; r <= 15; ++r) blocked.emplace_back(15, r);
    const ZoneGrid maze = WorldOpenGrid(20, 20, blocked);
    Pathfinder pm(maze);
    REQUIRE(pm.FindPath(0, 0, 19, 0, path));
    CHECK(path.back() == TilePos{19, 0});
    CHECK(WorldPathValid(maze, {0, 0}, path));

    const ZoneGrid g50 = WorldBorderGrid(50, 50);
    std::vector<TilePos> p1, p2;
    Pathfinder a(g50), b(g50);
    REQUIRE(a.FindPath(1, 1, 48, 48, p1));
    REQUIRE(b.FindPath(1, 1, 48, 48, p2));
    CHECK(p1 == p2);
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

  TEST_CASE("findWalkableNear / findWalkableTile ring order (world 7.2, 13.3)") {
    ZoneGrid g = WorldOpenGrid(20, 20);
    Pathfinder pf(g);
    TilePos out;
    // centre walkable is never returned by findWalkableNear: ring 1 first cell is (c-1, r-1)
    REQUIRE(pf.FindWalkableNear({10, 10}, 3, out));
    CHECK(out == TilePos{9, 9});
    for (int32_t dr = -1; dr <= 1; ++dr) {
      for (int32_t dc = -1; dc <= 1; ++dc) g.SetWalkable(10 + dc, 10 + dr, false);
    }
    g.SetWalkable(11, 8, false);
    REQUIRE(pf.FindWalkableNear({10, 10}, 3, out));
    CHECK(out == TilePos{8, 8});
    CHECK_FALSE(pf.FindWalkableNear({10, 10}, 1, out));
    // findWalkableTile: preferred first, clamped to [1, size-2]
    REQUIRE(pf.FindWalkableTile({5, 5}, 5, out));
    CHECK(out == TilePos{5, 5});
    REQUIRE(pf.FindWalkableTile({0, 0}, 5, out));
    CHECK(out == TilePos{1, 1});
    REQUIRE(pf.FindWalkableTile({10, 10}, 3, out));
    CHECK(out == TilePos{8, 8});  // ring 1 blocked; ring 2 starts at (c-2, r-2)
    // random-events-scrutiny-fix: fully blocked map -> none
    ZoneGrid blocked = WorldOpenGrid(20, 20);
    for (int32_t r = 0; r < 20; ++r) {
      for (int32_t c = 0; c < 20; ++c) blocked.SetWalkable(c, r, false);
    }
    Pathfinder pb(blocked);
    CHECK_FALSE(pb.FindWalkableTile({10, 10}, 5, out));
    blocked.SetWalkable(12, 10, true);
    REQUIRE(pb.FindWalkableTile({10, 10}, 3, out));
    CHECK(out == TilePos{12, 10});
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
}
