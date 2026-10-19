// World area (+ audio): grid, map generator, pathfinding, exploration, zone runtime, locomotion, random events,
// music / SFX rules. Spec vectors: world-map-nav.md section 21 (golden maps in CoreTests/golden/maps), audio.md 9.8.
// Web suites ported: PathfindingAndMaps.test.ts, MapLiquids.test.ts, FogOfWarOptimization.test.ts,
// RandomEventSystem.test.ts, random-events-scrutiny-fix.test.ts (core parts; the probabilistic web cases become
// scripted-RNG cases). The web has no hold-move / audio-director suites: world 21 items 4-10 and audio 9.8's list are
// covered from the spec instead.
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
#include "TestContent.h"
#include "WorldFixtures.h"
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

using namespace abyss;
using namespace abyss::test::world_fx;
namespace ch1 = abyss::test::ch1;
namespace fixture = abyss::test::fixture;

namespace {

// The key RandomEventSystem logs for a shared event line in `zone`: the zone's own variant when it has event data and
// the strings carry `<base>.<zone>` (RandomEvents.cpp RevZoneKey; the remade Red Field has its own lines).
std::string WtEventKey(const DataStore& d, const std::string& base, const std::string& zone) {
  if (d.World().randomEvents.ForZone(zone) == nullptr) return base;
  return d.Strings().Has(base + "." + zone) ? base + "." + zone : base;
}
// Live-map anchors through the TestContent.h handles (the remake keeps them, chapter1.md 17): a point `d` tiles off the
// Chapter 1 exit / the first camp's fire.
Vec2 AtExit(double dx = 0, double dy = 0) { return ch1::kExit.Center() + Vec2(dx, dy); }
Vec2 AtCamp(double dx = 0, double dy = 0) { return ch1::kCamp.Center() + Vec2(dx, dy); }
TilePos ExitTile(int32_t dc, int32_t dr = 0) { return TilePos{ch1::kExit.col + dc, ch1::kExit.row + dr}; }
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

  TEST_CASE("camp blockers (W11): every blocking prop blocks its anchor + its body grown by 0.3 tile, no pocket") {
    const DataStore& data = test::RealData();
    const MapDef& map = *data.FindMap("emerald_plains");
    // The generated grid's walkable count is a Chapter 1 pin (content_ch1_test.cpp); the blockers take 2 x 8 of it.
    const int32_t rawWalkable = WorldWalkableCount(BuildRawZoneGrid(data, map));
    REQUIRE(rawWalkable > 0);
    // The exact tiles of every blocking prop (camp-relative, row-major as CampPropBlockedTiles appends them): the well,
    // crate and barrel bounds (camp::kCampBlockBodies) grown by kCampBlockPad, plus the anchor. No tents (user request).
    // Smallest margin of a walkable tile centre to a padded body edge: 0.082 tile (the well's (3,-1)).
    const std::vector<std::pair<int32_t, int32_t>> kExpected[] = {
        {{4, -1}, {4, 0}},    // well 1
        {{-4, -4}, {-4, -3}}, // crate 14
        {{-4, -4}, {-3, -4}}, // barrel 15
        {{-4, 0}},            // crate 16
        {{3, -4}, {4, -4}},   // barrel 17
    };
    std::set<std::pair<int32_t, int32_t>> expected;
    for (const auto& tiles : kExpected) expected.insert(tiles.begin(), tiles.end());
    REQUIRE(expected.size() == 8);
    const ZoneGrid raw = BuildRawZoneGrid(data, map);
    ZoneGrid full = BuildRawZoneGrid(data, map);
    ApplyCampBlockers(full, map);
    const int32_t once = WorldWalkableCount(full);
    ApplyCampBlockers(full, map);
    CHECK(WorldWalkableCount(full) == once);  // idempotent
    CHECK(once == rawWalkable - 2 * 8);
    for (size_t ci = 0; ci < map.camps.size(); ++ci) {
      CAPTURE(ci);
      const TilePos camp = map.camps[ci].pos;
      const Vec2 campCentre = camp.Center();
      std::set<std::pair<int32_t, int32_t>> blocked;
      std::vector<TilePos> tiles;
      size_t blockingIndex = 0;
      std::vector<CampTestBox> bodies;
      for (const CampProp& p : CampProps(map)) {
        if (p.campIndex != static_cast<int32_t>(ci)) continue;
        tiles.clear();
        CampPropBlockedTiles(p, tiles);
        if (!p.blocking) {
          CHECK(tiles.empty());
          continue;
        }
        CAPTURE(p.type);
        CHECK(p.pos == RoundToTile(p.centre));
        REQUIRE(blockingIndex < std::size(kExpected));
        std::vector<std::pair<int32_t, int32_t>> rel;
        for (const TilePos& t : tiles) rel.emplace_back(t.col - camp.col, t.row - camp.row);
        CHECK(rel == kExpected[blockingIndex]);  // exact tiles, row-major
        CHECK(std::find(tiles.begin(), tiles.end(), p.pos) != tiles.end());  // the anchor always blocks
        ++blockingIndex;
        blocked.insert(rel.begin(), rel.end());
        bodies.push_back(CampTestPropBox(p, campCentre, p.front));  // every blocking prop has a front
      }
      CHECK(blockingIndex == std::size(kExpected));
      CHECK(blocked == expected);
      for (int32_t dr = -6; dr <= 6; ++dr) {
        for (int32_t dc = -6; dc <= 6; ++dc) {
          const int32_t c = camp.col + dc, r = camp.row + dr;
          CAPTURE(dc);
          CAPTURE(dr);
          CHECK(full.Walkable(c, r) == (raw.Walkable(c, r) && expected.count({dc, dr}) == 0));
          if (expected.count({dc, dr}) != 0) CHECK(raw.Tile(c, r) == TileType::Camp);  // never on the palisade
          // A hero / pet / follower standing on a walkable tile centre keeps >= kCampBlockPad from every prop body.
          if (full.Walkable(c, r)) {
            for (const CampTestBox& b : bodies) CHECK(b.Dist(Vec2(dc, dr)) > camp::kCampBlockPad);
          }
        }
      }
      CHECK(full.Walkable(camp.col, camp.row));          // campfire: the hero respawns on it
      CHECK(full.Walkable(camp.col - 1, camp.row - 5));  // the 2-tile gate
      CHECK(full.Walkable(camp.col, camp.row - 5));
      CHECK(full.Walkable(camp.col + 4, camp.row + 1));  // beside the well (4, -0.15): clear of its body + pad
      CHECK_FALSE(full.Walkable(camp.col + 4, camp.row - 1));  // under the well body (grown by the pad)
      // The camp has no tents (user request): the layout-v2 tent tiles are open ground again.
      for (const auto& [dc, dr] : {std::pair{3, 4}, std::pair{3, -3}, std::pair{-3, 4}, std::pair{4, -3}}) {
        CHECK(full.Walkable(camp.col + dc, camp.row + dr));
      }
    }
    // A blocker outside the grid is skipped (bounds-checked): a camp in the corner keeps only the well's (5,0) / (5,1);
    // the crates' and barrels' tiles (negative col or row) are dropped.
    MapDef corner = map;
    corner.camps = {{TilePos{1, 1}, {}}};
    ZoneGrid small(8, 8);
    REQUIRE(WorldWalkableCount(small) == 64);
    ApplyCampBlockers(small, corner);
    CHECK_FALSE(small.Walkable(5, 0));  // well (4, -0.15) -> (4,-1) / (4,0) relative
    CHECK_FALSE(small.Walkable(5, 1));
    CHECK(WorldWalkableCount(small) == 62);
    CHECK(small.Walkable(1, 1));
  }

  TEST_CASE("camp layout v2 geometry (W11): NPC slots clear of every prop, props clear of each other and the palisade") {
    const DataStore& data = test::RealData();
    const WorldConstants& wc = data.World().constants;
    const MapDef& map = *data.FindMap("emerald_plains");
    const ZoneGrid raw = BuildRawZoneGrid(data, map);
    const TilePos camp = map.camps[0].pos;
    const Vec2 campCentre = camp.Center();
    constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
    const double yaw = wc.cameraYawDeg * kDegToRad;
    const Vec2 camForward(std::cos(yaw), std::sin(yaw));     // ground forward: the camera sits on the -forward side
    const Vec2 camRight(-std::sin(yaw), std::cos(yaw));      // screen right on the ground
    const double tanPitch = std::tan(std::fabs(wc.cameraPitchDeg) * kDegToRad);
    const Vec2 towardCamera = -camForward;

    std::vector<CampProp> props;
    for (const CampProp& p : CampProps(map)) {
      if (p.campIndex == 0) props.push_back(p);
    }
    REQUIRE(props.size() == camp::kCampProps.size());
    REQUIRE(CampProps(map).size() == camp::kCampProps.size() * map.camps.size());
    for (size_t i = 0; i < props.size(); ++i) {
      CAPTURE(i);
      const camp::CampPropSpec& spec = camp::kCampProps[i];
      CHECK(props[i].type == spec.type);
      CHECK(props[i].centre == campCentre + Vec2(spec.x, spec.y));
      CHECK(props[i].pos == RoundToTile(props[i].centre));
      CHECK(props[i].blocking == spec.blocking);
      CHECK(props[i].type != "tent");  // no tents in the camp (user request, W11)
      if (spec.facesCentre) {
        // Unit front toward the camp centre.
        const Vec2 to = (campCentre - props[i].centre).Normalized();
        CHECK(std::fabs(props[i].front.Length() - 1.0) < 1e-12);
        CHECK(props[i].front.Dot(to) > 1.0 - 1e-12);
      } else {
        CHECK(props[i].front == Vec2());
      }
      // A blocking prop's anchor is camp ground (a side torch at x +-4.55 rounds onto the wall tile: its anchor only
      // seeds the art hash; its body clears the stakes, checked below).
      if (props[i].blocking) CHECK(raw.Tile(props[i].pos.col, props[i].pos.row) == TileType::Camp);
    }
    std::vector<CampTestBox> bodies;
    for (const CampProp& p : props) {
      bodies.push_back(CampTestPropBox(p, campCentre, towardCamera));
      CHECK(bodies.back().height > 0);  // every prop type has a body above
    }

    // (a) NPC slots: distinct tiles, apart, and the NPC body >= 0.35 m from every prop body.
    std::set<std::pair<int32_t, int32_t>> slotTiles;
    for (size_t si = 0; si < camp::kCampNpcSlots.size(); ++si) {
      CAPTURE(si);
      const Vec2 s = camp::kCampNpcSlots[si];
      const TilePos t = RoundToTile(s);
      slotTiles.emplace(t.col, t.row);
      for (size_t sj = si + 1; sj < camp::kCampNpcSlots.size(); ++sj) {
        CHECK((s - camp::kCampNpcSlots[sj]).Length() >= 2 * kCampTestNpcRadius + 0.5);
      }
      for (size_t i = 0; i < props.size(); ++i) {
        CAPTURE(i);
        CHECK(bodies[i].Dist(s) >= kCampTestNpcRadius + kCampTestNpcClearance);
      }
      // Nothing hides the NPC from the W1 camera (conservative screen rule): against every prop body and every other
      // NPC, the screen-x spans are disjoint, or the NPC is entirely nearer, or it stands beyond the occluder's far
      // edge by more than the occluder's height / tan(pitch).
      auto visible = [&](const std::vector<Vec2>& occ, double height) {
        double x0 = 1e9, x1 = -1e9, d0 = 1e9, d1 = -1e9;
        for (const Vec2& c : occ) {
          x0 = (std::min)(x0, c.Dot(camRight));
          x1 = (std::max)(x1, c.Dot(camRight));
          d0 = (std::min)(d0, c.Dot(camForward));
          d1 = (std::max)(d1, c.Dot(camForward));
        }
        const double sx = s.Dot(camRight), sd = s.Dot(camForward), r = kCampTestNpcRadius;
        return sx + r < x0 || sx - r > x1 || sd + r < d0 || sd - r > d1 + height / tanPitch;
      };
      for (size_t i = 0; i < props.size(); ++i) {
        CAPTURE(i);
        const std::array<Vec2, 4> cs = bodies[i].Corners();
        CHECK(visible(std::vector<Vec2>(cs.begin(), cs.end()), bodies[i].height));
      }
      for (size_t sj = 0; sj < camp::kCampNpcSlots.size(); ++sj) {
        if (sj == si) continue;
        CAPTURE(sj);
        const Vec2 o = camp::kCampNpcSlots[sj];
        const double r = kCampTestNpcRadius;
        CHECK(visible({o + camRight * r, o - camRight * r, o + camForward * r, o - camForward * r}, kCampTestNpcHeight));
      }
    }
    CHECK(slotTiles.size() == camp::kCampNpcSlots.size());
    // Click picking (ZoneRuntime::OnPointerPress row 5: the nearest NPC with distSq < interact.npcPickRadiusSq wins over
    // the ground): no slot's pick disc reaches the campfire (respawn / town-portal point) or the gate / open-side lane
    // centre lines, so clicking the fire or walking in along a lane never opens a shop.
    const double pickRadius = std::sqrt(wc.npcPickRadiusSq);
    for (size_t si = 0; si < camp::kCampNpcSlots.size(); ++si) {
      CAPTURE(si);
      const Vec2 s = camp::kCampNpcSlots[si];
      CHECK(s.Length() >= pickRadius + 0.1);
      CHECK(CampTestSegPointDist(s, Vec2(-0.5, -5.5), Vec2(-0.5, -0.5)) >= pickRadius + 0.05);
      CHECK(CampTestSegPointDist(s, Vec2(0.0, 0.5), Vec2(0.0, 6.0)) >= pickRadius + 0.05);
      // Idle facing: every slot looks straight at the fire (unit, std::sqrt only; no tents, no doors to look out of).
      const Vec2 f = camp::CampNpcFacing(si);
      CHECK(std::fabs(f.Length() - 1.0) < 1e-12);
      CHECK(f.Dot((-s).Normalized()) > 1.0 - 1e-12);
    }
    // The hearth reserve stays clear of every prop body (a later milestone puts the hearthstone there, Q3).
    for (const CampTestBox& b : bodies) CHECK(b.Dist(camp::kCampHearthReserve) >= 0.5);

    // (d) Props never overlap each other.
    for (size_t i = 0; i < props.size(); ++i) {
      for (size_t j = i + 1; j < props.size(); ++j) {
        CAPTURE(i);
        CAPTURE(j);
        CHECK(CampTestBoxDist(bodies[i], bodies[j]) >= 0.05);
      }
    }
    // ... nor the palisade: stakes along the camp-wall tile centres (radius kCampTestStakeRadius) and the gate posts.
    std::vector<std::array<Vec2, 2>> stakeRuns;
    for (int32_t dr = -7; dr <= 7; ++dr) {
      for (int32_t dc = -7; dc <= 7; ++dc) {
        if (raw.Tile(camp.col + dc, camp.row + dr) != TileType::CampWall) continue;
        const Vec2 a(dc, dr);
        stakeRuns.push_back({a, a});
        if (raw.Tile(camp.col + dc + 1, camp.row + dr) == TileType::CampWall) stakeRuns.push_back({a, Vec2(dc + 1, dr)});
        if (raw.Tile(camp.col + dc, camp.row + dr + 1) == TileType::CampWall) stakeRuns.push_back({a, Vec2(dc, dr + 1)});
      }
    }
    CHECK(stakeRuns.size() == 25 + 23);  // 25 wall tiles; 23 adjacent pairs (8 + 8 sides, 3 + 4 beside the gate)
    std::vector<CampTestBox> gatePosts;
    for (const double gx : {-1.5, 0.5}) {
      CampTestBox g;
      g.c = Vec2(gx, -camp::kCampHalfSize);
      g.minX = kCampTestGatePostMinX;
      g.maxX = kCampTestGatePostMaxX;
      g.minY = kCampTestGatePostMinY;
      g.maxY = kCampTestGatePostMaxY;
      gatePosts.push_back(g);
    }
    const CampTestBox gateLane = CampTestAxisBox(kCampTestGateLane);
    const CampTestBox southLane = CampTestAxisBox(kCampTestSouthLane);
    for (size_t i = 0; i < props.size(); ++i) {
      CAPTURE(i);
      const CampTestBox& box = bodies[i];
      const std::array<Vec2, 4> cs = box.Corners();
      double stakeClear = 1e9;
      for (const auto& run : stakeRuns) stakeClear = (std::min)(stakeClear, CampTestPolyDist(cs, run) - kCampTestStakeRadius);
      CHECK(stakeClear > 0.0);
      for (const CampTestBox& g : gatePosts) CHECK(CampTestBoxDist(box, g) > 0.0);
      if (props[i].type == "campfire") continue;  // the lanes end at the fire
      CHECK(CampTestBoxDist(box, gateLane) > 0.0);
      CHECK(CampTestBoxDist(box, southLane) > 0.0);
    }
    for (const Vec2& s : camp::kCampNpcSlots) {
      CHECK(gateLane.Dist(s) > kCampTestNpcRadius);
      CHECK(southLane.Dist(s) > kCampTestNpcRadius);
    }

    // (c) Every tile whose centre is under a blocking prop's body is unwalkable: the well's (4,0), crate 14's (-4,-3),
    // crate 16's (-4,0) (the barrels' bodies cover no tile centre; their anchors block anyway).
    const ZoneGrid g = BuildZoneGrid(data, map);
    int32_t underBody = 0;
    for (size_t i = 0; i < props.size(); ++i) {
      if (!props[i].blocking) continue;
      for (int32_t dr = -6; dr <= 6; ++dr) {
        for (int32_t dc = -6; dc <= 6; ++dc) {
          if (bodies[i].Dist(Vec2(dc, dr)) > 0.0) continue;
          ++underBody;
          CHECK_FALSE(g.Walkable(camp.col + dc, camp.row + dr));
        }
      }
    }
    CHECK(underBody == 3);
  }

  TEST_CASE("camp NPC slots are walkable and reachable from the camp centre, the gate and the start (W11, every map)") {
    const DataStore& data = test::RealData();
    int32_t checkedCamps = 0;
    for (const char* id : {"emerald_plains", "twilight_forest", "anvil_mountains", "scorching_desert", "abyss_rift",
                           "ember_tower"}) {
      CAPTURE(id);
      const MapDef& map = *data.FindMap(id);
      REQUIRE_FALSE(map.camps.empty());
      const ZoneGrid g = BuildZoneGrid(data, map);
      Pathfinder pf(g);
      std::vector<TilePos> path;
      for (size_t ci = 0; ci < map.camps.size(); ++ci) {
        CAPTURE(ci);
        const MapCampDef& c = map.camps[ci];
        ++checkedCamps;
        REQUIRE(g.Walkable(c.pos.col, c.pos.row));
        std::vector<TilePos> from{c.pos};
        for (const TilePos gate : {TilePos{c.pos.col - 1, c.pos.row - 6}, TilePos{c.pos.col, c.pos.row + 6}}) {
          if (g.Walkable(gate.col, gate.row)) from.push_back(gate);
        }
        if (map.generated) CHECK(from.size() == 3);  // generated camps: the gate and the open side both lead in
        for (size_t i = 0; i < c.npcs.size(); ++i) {
          CAPTURE(i);
          const TilePos slot = RoundToTile(c.pos.Center() + camp::kCampNpcSlots[i % camp::kCampNpcSlots.size()]);
          REQUIRE(g.Walkable(slot.col, slot.row));
          for (const TilePos& f : from) {
            CAPTURE(f.col);
            CAPTURE(f.row);
            CHECK(pf.FindPath(f.col, f.row, slot.col, slot.row, path));
            CHECK(WorldPathValid(g, f, path));
          }
        }
        // Every slot tile (spares too) stays walkable.
        for (const Vec2& s : camp::kCampNpcSlots) {
          const TilePos t = RoundToTile(c.pos.Center() + s);
          CHECK(g.Walkable(t.col, t.row));
        }
        // No pocket: every walkable tile of the camp stamp is in the campfire's A* component (a walkable tile sealed off
        // by props and the palisade is a dead click target and traps a hero that corner-cuts or teleports into it), and
        // a click on any prop-blocked tile (findWalkableNear, 3 rings) lands on a tile the fire reaches.
        const int32_t fire = g.Component(c.pos.col, c.pos.row);
        for (int32_t dr = -camp::kCampHalfSize; dr <= camp::kCampHalfSize; ++dr) {
          for (int32_t dc = -camp::kCampHalfSize; dc <= camp::kCampHalfSize; ++dc) {
            if (!g.Walkable(c.pos.col + dc, c.pos.row + dr)) continue;
            CAPTURE(dc);
            CAPTURE(dr);
            CHECK(g.Component(c.pos.col + dc, c.pos.row + dr) == fire);
          }
        }
        std::vector<TilePos> blockedTiles;
        for (const CampProp& p : CampProps(map)) {
          if (p.campIndex == static_cast<int32_t>(ci)) CampPropBlockedTiles(p, blockedTiles);
        }
        CHECK(blockedTiles.size() == 9);  // 8 distinct: (-4,-4) belongs to crate 14 and barrel 15
        for (const TilePos& t : blockedTiles) {
          CAPTURE(t.col);
          CAPTURE(t.row);
          TilePos nearTile;
          REQUIRE(pf.FindWalkableNear(t, 3, nearTile));
          CHECK(g.Component(nearTile.col, nearTile.row) == fire);
        }
      }
      CHECK(pf.FindPath(map.playerStart.col, map.playerStart.row, map.camps[0].pos.col, map.camps[0].pos.row, path));
    }
    CHECK(checkedCamps == 12);
    // W11: decorations on a camp prop's tile are dropped (ember_tower's grass under banner 4); its bush (27,42) stood
    // under tent 1 and stays now the camp has no tents.
    const MapDef& tower = *data.FindMap("ember_tower");
    const ZoneGrid raw = BuildRawZoneGrid(data, tower);
    const ZoneGrid built = BuildZoneGrid(data, tower);
    auto has = [](const ZoneGrid& z, int32_t c, int32_t r, const char* type) {
      return std::any_of(z.Decorations().begin(), z.Decorations().end(),
                         [&](const Decoration& d) { return d.col == c && d.row == r && d.type == type; });
    };
    CHECK(has(raw, 27, 42, "bush"));
    CHECK(has(raw, 19, 42, "grass"));
    CHECK(has(built, 27, 42, "bush"));
    CHECK_FALSE(has(built, 19, 42, "grass"));
    CHECK(built.Decorations().size() == raw.Decorations().size() - 1);
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
      for (const MapCampDef& c : map.camps) {
        CHECK(reach(c.pos));
        for (size_t i = 0; i < c.npcs.size(); ++i) {
          CHECK(reach(RoundToTile(c.pos.Center() + camp::kCampNpcSlots[i % camp::kCampNpcSlots.size()])));  // W11
        }
      }
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

  TEST_CASE("A* connectivity labels: unreachable goals fail fast, labels follow every walkability change") {
    // ZoneGrid::Component flood-fills with exactly A*'s moves (8 directions, no corner cutting). FindPath rejects a goal
    // in another component before searching; the result must equal a plain reachability search's verdict.
    std::vector<TilePos> walls;
    for (int32_t c = 4; c <= 8; ++c) {
      walls.emplace_back(c, 4);
      walls.emplace_back(c, 8);
    }
    for (int32_t r = 5; r <= 7; ++r) {
      walls.emplace_back(4, r);
      walls.emplace_back(8, r);
    }
    ZoneGrid g = WorldOpenGrid(20, 20, walls);
    Pathfinder pf(g);
    std::vector<TilePos> path;
    CHECK(g.Component(6, 6) >= 0);
    CHECK(g.Component(6, 6) != g.Component(1, 1));  // the walled pocket
    CHECK(g.Component(4, 4) == -1);                 // a wall
    CHECK_FALSE(pf.FindPath(1, 1, 6, 6, path));
    CHECK(path.empty());
    CHECK_FALSE(pf.FindPath(6, 6, 1, 1, path));
    CHECK(pf.FindPath(5, 5, 7, 7, path));  // inside the pocket
    g.SetWalkable(8, 6, true);             // open a door: labels rebuild
    CHECK(g.Component(6, 6) == g.Component(1, 1));
    REQUIRE(pf.FindPath(1, 1, 6, 6, path));
    CHECK(WorldPathValid(g, {1, 1}, path));
    g.SetTile(8, 6, TileType::Wall);  // and close it again
    CHECK(g.Component(6, 6) != g.Component(1, 1));
    CHECK_FALSE(pf.FindPath(1, 1, 6, 6, path));

    // Property check on a random grid: FindPath succeeds iff a reachability flood (A* moves) connects the tiles, and a
    // reused Pathfinder (persistent scratch) returns exactly what a fresh one does.
    uint32_t seed = 12345u;
    auto rnd = [&seed]() {
      seed = seed * 1103515245u + 12345u;
      return (seed >> 16) & 0x7FFFu;
    };
    std::vector<TilePos> scatter;
    for (int32_t r = 0; r < 30; ++r) {
      for (int32_t c = 0; c < 30; ++c) {
        if (rnd() % 100 < 38) scatter.emplace_back(c, r);
      }
    }
    const ZoneGrid rg = WorldOpenGrid(30, 30, scatter);
    auto reachable = [&rg](TilePos a, TilePos b) {
      std::vector<uint8_t> seen(30 * 30, 0);
      std::vector<TilePos> stack{a};
      seen[static_cast<size_t>(a.row * 30 + a.col)] = 1;
      const int32_t dirs[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
      while (!stack.empty()) {
        const TilePos t = stack.back();
        stack.pop_back();
        if (t == b) return true;
        for (const auto& d : dirs) {
          const int32_t nc = t.col + d[0], nr = t.row + d[1];
          if (!rg.Walkable(nc, nr) || seen[static_cast<size_t>(nr * 30 + nc)] != 0) continue;
          if (d[0] != 0 && d[1] != 0 && (!rg.Walkable(t.col + d[0], t.row) || !rg.Walkable(t.col, t.row + d[1]))) continue;
          seen[static_cast<size_t>(nr * 30 + nc)] = 1;
          stack.emplace_back(nc, nr);
        }
      }
      return false;
    };
    Pathfinder reused(rg);
    int checked = 0, found = 0;
    for (int i = 0; i < 400; ++i) {
      const TilePos a(static_cast<int32_t>(rnd() % 30), static_cast<int32_t>(rnd() % 30));
      const TilePos b(static_cast<int32_t>(rnd() % 30), static_cast<int32_t>(rnd() % 30));
      if (!rg.Walkable(a.col, a.row) || !rg.Walkable(b.col, b.row) || a == b) continue;
      ++checked;
      std::vector<TilePos> p1, p2;
      const bool ok1 = reused.FindPath(a.col, a.row, b.col, b.row, p1);
      Pathfinder fresh(rg);
      const bool ok2 = fresh.FindPath(a.col, a.row, b.col, b.row, p2);
      CHECK(ok1 == reachable(a, b));
      CHECK(ok1 == ok2);
      CHECK(p1 == p2);
      CHECK((rg.Component(a.col, a.row) == rg.Component(b.col, b.row)) == ok1);
      found += ok1 ? 1 : 0;
    }
    CHECK(checked > 100);
    CHECK(found > 10);
    CHECK(found < checked);
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

// =====================================================================================================================
// Runtime: zone, locomotion, exploration, random events (world 6-7, 9-10, 13; W3, W5-W8, S5) and the audio rules
// =====================================================================================================================

TEST_SUITE("world") {
  // ===================================================================================================================
  // FogOfWarCore (FogOfWarOptimization.test.ts, core logic)
  // ===================================================================================================================
  TEST_CASE("FogOfWarCore: updates only on a tile change; dirty list per update") {
    FogOfWarCore core;
    core.Reset(30, 30, 5);
    CHECK(core.Update(10, 10));
    CHECK(core.IsExplored(10, 10));
    CHECK(core.IsExplored(11, 10));
    CHECK(core.IsExplored(10, 11));
    CHECK_FALSE(core.Update(10, 10));
    core.ClearDirty();
    for (int i = 0; i < 3; ++i) {
      CHECK_FALSE(core.Update(10, 10));
      CHECK(core.Dirty().empty());  // same position: nothing recomputed
    }
    CHECK(core.Update(12, 12));
    CHECK_FALSE(core.Dirty().empty());
    // Dirty indices are row-major, in bounds and unique (the web's Set).
    std::set<int32_t> uniq(core.Dirty().begin(), core.Dirty().end());
    CHECK(uniq.size() == core.Dirty().size());
    CHECK(std::is_sorted(core.Dirty().begin(), core.Dirty().end()));
    for (int32_t idx : core.Dirty()) {
      CHECK(idx >= 0);
      CHECK(idx < 30 * 30);
    }
  }

  TEST_CASE("FogOfWarCore: explored data export / import, wrong dimensions rejected, out of bounds") {
    FogOfWarCore core;
    core.Reset(30, 30, 5);
    core.Update(5, 5);
    const std::vector<std::vector<bool>> data = core.ExploredData();
    REQUIRE(data.size() == 30);
    REQUIRE(data[0].size() == 30);
    CHECK(data[5][5]);
    CHECK_FALSE(data[29][29]);
    FogOfWarCore other;
    other.Reset(30, 30, 5);
    REQUIRE(other.LoadExploredData(data));
    for (int32_t r = 0; r < 30; ++r) {
      for (int32_t c = 0; c < 30; ++c) CHECK(other.IsExplored(c, r) == core.IsExplored(c, r));
    }
    std::vector<std::vector<bool>> one(30, std::vector<bool>(30, false));
    one[15][15] = true;
    REQUIRE(other.LoadExploredData(one));
    CHECK(other.IsExplored(15, 15));
    CHECK_FALSE(other.IsExplored(0, 0));
    CHECK_FALSE(core.LoadExploredData({{true, false}}));  // wrong dimensions: unchanged
    CHECK(core.IsExplored(5, 5));
    CHECK_FALSE(core.IsExplored(-1, 0));
    CHECK_FALSE(core.IsExplored(0, -1));
    CHECK_FALSE(core.IsExplored(30, 0));
    CHECK_FALSE(core.IsExplored(0, 30));
    // After a load the next update recomputes even at the same position.
    core.Update(10, 10);
    REQUIRE(core.LoadExploredData(core.ExploredData()));
    CHECK(core.Update(10, 10));
  }

  TEST_CASE("FogOfWarCore: alpha bands and the gradient edge (vr 5, edge band 3)") {
    FogOfWarCore core;
    core.Reset(30, 30, 5);
    core.Update(15, 15);
    CHECK(core.Alpha(15, 15) == 0);
    CHECK(core.Alpha(16, 15) == 0);  // inside innerEdge 2
    const std::vector<FogGradientTile> g = core.GradientInfo(15, 15);
    REQUIRE_FALSE(g.empty());
    bool found3 = false;
    for (const FogGradientTile& t : g) {
      const double dist = std::sqrt((t.col - 15.0) * (t.col - 15.0) + (t.row - 15.0) * (t.row - 15.0));
      CHECK(dist > 2.0);
      CHECK(dist <= 5.0);
      CHECK(t.alpha >= 0.01);
      if (t.col == 18 && t.row == 15) {
        found3 = true;
        CHECK(t.alpha > 0);
        CHECK(t.alpha < 0.85);
        CHECK(t.alpha == doctest::Approx(0.05));  // (3 - 2) / 3 * 0.15
      }
    }
    CHECK(found3);
    // Farther gradient tiles never get less fog.
    std::vector<FogGradientTile> sorted = g;
    std::stable_sort(sorted.begin(), sorted.end(), [](const FogGradientTile& a, const FogGradientTile& b) {
      return (a.col - 15) * (a.col - 15) + (a.row - 15) * (a.row - 15) <
             (b.col - 15) * (b.col - 15) + (b.row - 15) * (b.row - 15);
    });
    CHECK(sorted.back().alpha >= sorted.front().alpha);
    // Quantised alpha: unexplored 0.85 (round(0.85 * 255) / 255), out of bounds 0.85.
    CHECK(core.Alpha(29, 29) == doctest::Approx(217.0 / 255.0));
    CHECK(core.Alpha(-1, 3) == 0.85);
    // Explored but out of view: 0.18 + (dist - vr) / 3 * 0.15 within the band, 0.35 beyond.
    core.Update(25, 15);
    CHECK(core.Alpha(15, 15) == doctest::Approx(JsRound(0.35 * 255) / 255.0));
    CHECK(core.Alpha(19, 15) == doctest::Approx(JsRound((0.18 + 1.0 / 3.0 * 0.15) * 255) / 255.0));
  }

  TEST_CASE("FogOfWarCore: incremental updates, corners, 1x1 grid, invalidate") {
    FogOfWarCore big;
    big.Reset(120, 120, 10);
    big.Update(60, 60);
    const size_t first = big.Dirty().size();
    big.Update(61, 60);
    const size_t moved = big.Dirty().size();
    CHECK(moved > 0);
    CHECK(moved < first);
    CHECK(moved < 120 * 120 * 0.2);
    // load -> update is a full pass again
    REQUIRE(big.LoadExploredData(big.ExploredData()));
    big.Update(61, 60);
    CHECK(static_cast<double>(big.Dirty().size()) >= first * 0.8);
    FogOfWarCore core;
    core.Reset(30, 30, 5);
    core.Update(0, 0);
    CHECK(core.IsExplored(0, 0));
    core.Update(29, 29);
    CHECK(core.IsExplored(29, 29));
    CHECK(core.IsExplored(0, 0));  // explored tiles accumulate
    core.Invalidate();
    CHECK(core.Update(29, 29));
    CHECK_FALSE(core.Dirty().empty());
    FogOfWarCore tiny;
    tiny.Reset(1, 1, 5);
    tiny.Update(0, 0);
    CHECK(tiny.ExploredData() == std::vector<std::vector<bool>>{{true}});
    FogOfWarCore fresh;
    fresh.Reset(30, 30, 5);
    bool anyExplored = false;
    for (int32_t r = 0; r < 30; ++r) {
      for (int32_t c = 0; c < 30; ++c) anyExplored = anyExplored || fresh.IsExplored(c, r);
    }
    CHECK_FALSE(anyExplored);
  }

  // ===================================================================================================================
  // Random event rules (world 13.1-13.2; RandomEventSystem.test.ts with a scripted RNG; W6)
  // ===================================================================================================================
  TEST_CASE("random events 13.1: exported config, definitions and zone data") {
    const RandomEventsDef& def = WD().World().randomEvents;
    CHECK(def.cooldownMs == 30000);
    CHECK(def.safeZoneRadius == 9);
    CHECK(def.minEventsPerWindow == 3);
    CHECK(def.maxEventsPerWindow == 8);
    CHECK(def.frequencyWindowMs == 300000);
    CHECK(def.triggerMoveThresholdTiles == 3);
    CHECK(def.resetMoveCounterAfterEveryRoll);  // W6
    REQUIRE(def.types.size() == 5);
    const RandomEventType order[] = {RandomEventType::Ambush, RandomEventType::TreasureCache,
                                     RandomEventType::WanderingMerchant, RandomEventType::Rescue,
                                     RandomEventType::EnvironmentalPuzzle};
    const double weights[] = {30, 25, 15, 20, 10};
    double total = 0;
    for (size_t i = 0; i < 5; ++i) {
      CHECK(def.types[i].type == order[i]);
      CHECK(def.types[i].weight == weights[i]);
      total += def.types[i].weight;
    }
    CHECK(total == 100);
    for (const char* z : kWorldZoneIds) CHECK(def.ForZone(z) != nullptr);
    CHECK(def.ForZone("nonexistent") == nullptr);
    // The emerald_plains event data: content_ch1_test.cpp.
    // Zone scaling: ambush counts and puzzle rewards never shrink along the map order.
    int32_t prevMax = 0;
    int64_t prevGold = 0;
    for (const char* z : kWorldZoneIds) {
      const ZoneEventDataDef* d = def.ForZone(z);
      CHECK(d->ambushCountMax >= prevMax);
      CHECK(d->puzzles.front().rewardGold >= prevGold);
      prevMax = d->ambushCountMax;
      prevGold = d->puzzles.front().rewardGold;
    }
  }

  TEST_CASE("random events 13.2: chance formula breakpoints and the weighted pick boundaries") {
    const RandomEventsDef& def = WD().World().randomEvents;
    CHECK(RandomEventRules::TriggerChance(def, 0, 0) == doctest::Approx(0.07));
    CHECK(RandomEventRules::TriggerChance(def, 0, 90000) == doctest::Approx(0.07));  // p = 0.3: strict >
    CHECK(RandomEventRules::TriggerChance(def, 0, 90001) == doctest::Approx(0.07 + 0.05 * 90001.0 / 300000.0));
    CHECK(RandomEventRules::TriggerChance(def, 2, 600000) == doctest::Approx(0.12));  // p capped at 1
    CHECK(RandomEventRules::TriggerChance(def, 3, 600000) == doctest::Approx(0.07));  // at the minimum: no ramp
    CHECK(RandomEventRules::TriggerChance(def, 7, 600000) == doctest::Approx(0.021));  // >= max - 1: x 0.3
    CHECK(RandomEventRules::PickType(def, 0.0) == RandomEventType::Ambush);
    CHECK(RandomEventRules::PickType(def, 0.30) == RandomEventType::Ambush);  // 30 - 30 = 0 <= 0
    CHECK(RandomEventRules::PickType(def, 0.300001) == RandomEventType::TreasureCache);
    CHECK(RandomEventRules::PickType(def, 0.5499999) == RandomEventType::TreasureCache);
    CHECK(RandomEventRules::PickType(def, 0.55) == RandomEventType::WanderingMerchant);  // 0.55 * 100 = 55.000...01
    CHECK(RandomEventRules::PickType(def, 0.70) == RandomEventType::WanderingMerchant);
    CHECK(RandomEventRules::PickType(def, 0.90) == RandomEventType::Rescue);
    CHECK(RandomEventRules::PickType(def, 0.95) == RandomEventType::EnvironmentalPuzzle);
    CHECK(RandomEventRules::PickType(def, 0.9999999) == RandomEventType::EnvironmentalPuzzle);
  }

  TEST_CASE("random events 13.2: gates (moving, combat, pending, safe zone, cooldown, threshold, window cap)") {
    const RandomEventsDef& def = WD().World().randomEvents;
    Rng rng(9);
    RandomEventType type = RandomEventType::Ambush;
    auto in = [](double now, Vec2 pos) {
      RandomEventTriggerInput i;
      i.nowMs = now;
      i.dtMs = 500;
      i.heroPos = pos;
      return i;
    };
    RandomEventRules r;
    CHECK_FALSE(r.Update(def, in(0, {60, 50}), rng, type));  // primes the position (no jump)
    CHECK(r.MovementAccum() == 0);
    CHECK(r.ExplorationMs() == 500);  // the first call counts as a move (lastCol = -1 in the web)
    CHECK_FALSE(r.Update(def, in(100, {61, 50}), rng, type));
    CHECK_FALSE(r.Update(def, in(200, {62, 50}), rng, type));
    CHECK(r.MovementAccum() == 2);
    // Standing still: no exploration time, no roll.
    const double explored = r.ExplorationMs();
    rng.Script({0.0});
    CHECK_FALSE(r.Update(def, in(300, {62, 50}), rng, type));
    CHECK(r.ExplorationMs() == explored);
    CHECK(rng.ScriptedRemaining() == 1);
    // In combat / pending / safe zone: no roll even with the threshold met.
    RandomEventTriggerInput c = in(400, {63, 50});
    c.inCombat = true;
    CHECK_FALSE(r.Update(def, c, rng, type));
    RandomEventTriggerInput p = in(500, {64, 50});
    p.eventPending = true;
    CHECK_FALSE(r.Update(def, p, rng, type));
    RandomEventTriggerInput s = in(600, {65, 50});
    s.inSafeZone = true;
    CHECK_FALSE(r.Update(def, s, rng, type));
    CHECK(rng.ScriptedRemaining() == 1);
    CHECK(r.MovementAccum() == 5);  // movement still accumulates
    // Threshold met, out of camp: the scripted 0.0 roll triggers; the second draw (0.0) picks ambush.
    rng.ClearScript();
    rng.Script({0.0, 0.0});
    CHECK(r.Update(def, in(700, {66, 50}), rng, type));
    CHECK(type == RandomEventType::Ambush);
    CHECK(r.LastEventMs() == 700);
    CHECK(r.History() == std::vector<double>{700});
    CHECK(r.MovementAccum() == 0);
    // Cooldown 30 s: no roll before 30700 even after 10 tiles.
    for (int i = 1; i <= 10; ++i) CHECK_FALSE(r.Update(def, in(700 + i * 1000, {66.0 + i, 50}), rng, type));
    CHECK(rng.ScriptedRemaining() == 0);
    // W6: a failed roll resets the movement counter, so the next roll needs 3 more tiles.
    RandomEventRules w;
    w.Update(def, in(0, {60, 50}), rng, type);
    w.Update(def, in(100, {62, 50}), rng, type);
    CHECK(w.MovementAccum() == 2);
    rng.Script({0.99});
    CHECK_FALSE(w.Update(def, in(200, {64, 50}), rng, type));  // 4 tiles, roll 0.99 > chance
    CHECK(rng.ScriptedRemaining() == 0);
    CHECK(w.MovementAccum() == 0);
    rng.Script({0.0, 0.5});
    CHECK_FALSE(w.Update(def, in(300, {65, 50}), rng, type));  // 1 tile: below the threshold, no draw
    CHECK_FALSE(w.Update(def, in(400, {66, 50}), rng, type));  // 2 tiles
    CHECK(rng.ScriptedRemaining() == 2);
    CHECK(w.Update(def, in(500, {67, 50}), rng, type));  // 3 tiles again: rolls 0.0, picks 0.5 -> treasure
    CHECK(type == RandomEventType::TreasureCache);
    rng.ClearScript();
  }

  TEST_CASE("random events 13.2: at most maxEventsPerWindow per 300 s; old entries leave the window") {
    const RandomEventsDef& def = WD().World().randomEvents;
    Rng rng(4);
    RandomEventType type = RandomEventType::Ambush;
    RandomEventRules r;
    double now = 0;
    double col = 60;
    auto move = [&](double ms) {
      now += ms;
      col += 1;
      RandomEventTriggerInput i;
      i.nowMs = now;
      i.dtMs = ms;
      i.heroPos = {col, 50};
      return r.Update(def, i, rng, type);
    };
    move(0);
    int triggered = 0;
    for (int i = 0; i < 1000 && now < 290000; ++i) {
      rng.Script({0.0, 0.0});
      if (move(500)) ++triggered;
      rng.ClearScript();
    }
    CHECK(triggered == def.maxEventsPerWindow);  // one per 30 s cooldown, capped at 8 inside the window
    CHECK(r.EventsInWindow(def, now) == 8);
    // Rolling the window forward frees the oldest entries.
    for (int i = 0; i < 200 && triggered < 9; ++i) {
      rng.Script({0.0, 0.0});
      if (move(500)) ++triggered;
      rng.ClearScript();
    }
    CHECK(triggered == 9);
    CHECK(now > 300000);
    CHECK(r.EventsInWindow(def, now) <= def.maxEventsPerWindow);
  }

  // ===================================================================================================================
  // Random events runtime (13.3; W10, W11)
  // ===================================================================================================================
  TEST_CASE("random events 13.3: environmental puzzle prop, interact range, leave keeps it pending, solve rewards") {
    WorldRig w;
    REQUIRE(w.ok);
    const TilePos spot = WorldClearRow(w.zone, 4);
    REQUIRE(spot.col > 0);
    w.Place(spot.Center());
    w.events.TriggerEvent(RandomEventType::EnvironmentalPuzzle, spot.Center());
    REQUIRE(w.events.Active().size() == 1);
    const ActiveRandomEvent& ev = w.events.Active().front();
    CHECK_FALSE(ev.resolved);
    REQUIRE(ev.prop != kNoEntity);
    CHECK(ev.propPos == spot.Center());  // walkable: findWalkableTile keeps it
    CHECK(ev.puzzleIndex == 0);
    CHECK(w.Logged(WtEventKey(w.h.ctx.data, "sys.event.msg.environmental_puzzle", w.h.session.currentMap)));
    CHECK(w.Logged("zone.event.puzzle.prompt"));
    CHECK(w.Count<EvRandomEvent>() == 1);
    CHECK(w.events.HasUnresolved());
    // Interact range distSq <= 4 (7.4 order 7).
    w.Place(Vec2(spot.col + 2.0, spot.row));
    InteractTarget t = w.zone.FindInteractTarget();
    CHECK(t.kind == InteractKind::EventPuzzle);
    CHECK(t.id == ev.prop);
    w.Place(Vec2(spot.col + 2.01, spot.row));
    CHECK(w.zone.FindInteractTarget().kind == InteractKind::None);
    w.Place(Vec2(spot.col + 1.0, spot.row));
    REQUIRE(w.zone.Interact());
    CHECK(w.events.Puzzle().open);
    CHECK(w.events.Puzzle().prop == ev.prop);
    // Leave: closed, unresolved, blocks further events.
    CHECK(w.events.AnswerPuzzle(ev.prop, kPuzzleChoiceLeave));
    CHECK_FALSE(w.events.Puzzle().open);
    CHECK(w.events.HasUnresolved());
    CHECK(w.Logged("zone.event.puzzle.left"));
    CHECK_FALSE(w.events.AnswerPuzzle(ev.prop, kPuzzleChoiceSolve));  // not open
    // Solve: gold + exp through the reward service (W11 addExp path), resolved, prop removed.
    REQUIRE(w.events.OpenPuzzle(ev.prop));
    const int64_t gold0 = w.hero->Gold();
    const int64_t exp0 = w.hero->Exp();
    const int64_t toNext = w.hero->ExpToNext();
    const int32_t level0 = w.hero->Level();
    w.h.events.Clear();
    CHECK(w.events.AnswerPuzzle(ev.prop, kPuzzleChoiceSolve));
    // ZoneScene.ts:3655: "<solution> - <reward>" first, then the gold / exp line.
    size_t solvedAt = 0, rewardAt = 0;
    const EvLog* solved = w.FindLog("zone.event.puzzle.solved", &solvedAt);
    REQUIRE(solved != nullptr);
    REQUIRE(w.FindLog("zone.event.puzzle.rewardGoldExp", &rewardAt) != nullptr);
    CHECK(solvedAt < rewardAt);
    REQUIRE(solved->text.args.size() == 2);
    CHECK(solved->text.args[0] == KeyArg("solution", "sys.event.puzzle.emerald_plains.solution"));
    CHECK(solved->text.args[1] == KeyArg("reward", "sys.event.puzzle.emerald_plains.reward"));
    CHECK(WD().Strings().Lookup(LocaleId::En, "zone.event.puzzle.solved") != nullptr);
    CHECK(WD().Strings().Lookup(LocaleId::ZhCN, "zone.event.puzzle.solved") != nullptr);
    CHECK(w.hero->Gold() == gold0 + 50);
    if (exp0 + 30 >= toNext) {  // the normal addExp path levels up (W11)
      CHECK(w.hero->Level() == level0 + 1);
      CHECK(w.hero->Exp() == exp0 + 30 - toNext);
      CHECK(w.Count<EvLevelUp>() == 1);
    } else {
      CHECK(w.hero->Exp() == exp0 + 30);
    }
    CHECK_FALSE(w.events.HasUnresolved());
    CHECK(w.Count<EvEntityDespawned>() == 1);
    REQUIRE(w.Last<EvRandomEvent>() != nullptr);
    CHECK(w.Last<EvRandomEvent>()->resolved);
    CHECK(w.events.Active().empty());  // pruned
    CHECK(w.zone.FindInteractTarget().kind == InteractKind::None);
  }

  TEST_CASE("random events 13.3: rescue completes once the tracked monsters are gone (M4 / W11), 500 ms poll") {
    WorldRig w;
    REQUIRE(w.ok);
    const TilePos spot = WorldClearRow(w.zone, 4);
    w.Place(spot.Center());
    w.events.TriggerEvent(RandomEventType::Rescue, spot.Center());
    REQUIRE(w.events.Active().size() == 1);
    CHECK_FALSE(w.events.Active().front().resolved);
    CHECK(w.events.Active().front().monsterCount >= 2);  // max(2, ...): emerald 2-4
    CHECK(w.events.Active().front().monsterCount <= 4);
    const int64_t gold0 = w.hero->Gold();
    w.Step(29);  // 483 ms: the first poll is due at 500 ms
    CHECK(w.events.HasUnresolved());
    w.Step(1);
    // No MonsterSystem in this rig: no tracked monster is alive at the first poll.
    CHECK_FALSE(w.events.HasUnresolved());
    CHECK(w.hero->Gold() == gold0 + 30);
    CHECK(w.Logged(WtEventKey(w.h.ctx.data, "zone.event.rescue.complete", w.h.session.currentMap)));
  }

  TEST_CASE("random events 13.3: a zone without event data gets createEvent's fallbacks (puzzle, ambush, rescue)") {
    // RandomEventSystem.ts:383-423 / ZoneScene.ts:3264-3280: no zone data -> the fallback puzzle (50 gold, 30 exp,
    // sys.event.puzzle.fallback.*), ambush / rescue monsters = the zone's first monster def, rescue reward 50 / 40.
    const std::unique_ptr<DataStore> data = WorldEditedData("random_events.json", [](JsonValue& d) {
      REQUIRE(d.FindMutable("zones")->Remove("emerald_plains"));
    });
    REQUIRE(data->World().randomEvents.ForZone("emerald_plains") == nullptr);
    WorldRig w(5, true, "", false, {}, *data);
    REQUIRE(w.ok);
    MonsterSystem monsters(w.h.ctx);
    w.h.ctx.sys.monsters = &monsters;
    const TilePos spot = WorldClearRow(w.zone, 4);
    REQUIRE(spot.col > 0);
    w.Place(spot.Center());

    // Environmental puzzle: always a prop (the web's "no puzzle -> resolved" branch is unreachable).
    w.events.TriggerEvent(RandomEventType::EnvironmentalPuzzle, spot.Center());
    REQUIRE(w.events.Active().size() == 1);
    const EntityId prop = w.events.Active().front().prop;
    REQUIRE(prop != kNoEntity);
    CHECK_FALSE(w.events.Active().front().resolved);
    CHECK(w.events.Active().front().puzzleIndex == -1);
    CHECK(w.events.Active().front().propArt == "decor_puzzle_stone");
    const EvLog* prompt = w.FindLog("zone.event.puzzle.prompt");
    REQUIRE(prompt != nullptr);
    REQUIRE(prompt->text.args.size() == 1);
    CHECK(prompt->text.args[0] == KeyArg("prompt", "sys.event.puzzle.fallback.prompt"));
    REQUIRE(w.events.OpenPuzzle(prop));
    CHECK(w.events.Puzzle().puzzleIndex == -1);
    const int64_t gold0 = w.hero->Gold();
    w.h.events.Clear();
    REQUIRE(w.events.AnswerPuzzle(prop, kPuzzleChoiceSolve));
    CHECK(w.hero->Gold() == gold0 + 50);
    const EvLog* solved = w.FindLog("zone.event.puzzle.solved");
    REQUIRE(solved != nullptr);
    REQUIRE(solved->text.args.size() == 2);
    CHECK(solved->text.args[0] == KeyArg("solution", "sys.event.puzzle.fallback.solution"));
    CHECK(solved->text.args[1] == KeyArg("reward", "sys.event.puzzle.fallback.reward"));
    const EvLog* gained = w.FindLog("zone.event.puzzle.rewardGoldExp");
    REQUIRE(gained != nullptr);
    REQUIRE(gained->text.args.size() == 2);
    CHECK(gained->text.args[0].value == "50");
    CHECK(gained->text.args[1].value == "30");
    CHECK(w.events.Active().empty());

    // Ambush: 3-5 monsters (fallback count), every one the zone's first monster def.
    const std::string first = WD().Monsters().ZoneList("emerald_plains")->monsterIds.front();
    CHECK_FALSE(first.empty());  // which def is first is a Chapter 1 pin (content_ch1_test.cpp)
    REQUIRE(monsters.All().empty());
    w.events.TriggerEvent(RandomEventType::Ambush, spot.Center());
    CHECK(w.events.Active().empty());  // the ambush resolves once spawned (and is pruned)
    size_t ambush = 0;
    for (const MonsterInstance& m : monsters.All()) {
      CHECK(m.def.id == first);
      CHECK(m.role == MonsterRole::AmbushSpawn);
      ++ambush;
    }
    CHECK(ambush >= 3);
    CHECK(ambush <= 5);

    // Rescue: >= 2 monsters of the first def around the fallback NPC; once they are gone the 50 / 40 fallback reward
    // and the fallback NPC name.
    w.events.TriggerEvent(RandomEventType::Rescue, spot.Center());
    const ActiveRandomEvent* rescue = nullptr;
    for (const ActiveRandomEvent& e : w.events.Active()) {
      if (e.type == RandomEventType::Rescue) rescue = &e;
    }
    REQUIRE(rescue != nullptr);
    CHECK(rescue->propArt == "npc_rescue");
    CHECK(rescue->monsters.size() >= 2);
    CHECK(rescue->rewardGold == 50);
    CHECK(rescue->rewardExp == 40);
    for (EntityId id : rescue->monsters) {
      REQUIRE(monsters.Find(id) != nullptr);
      CHECK(monsters.Find(id)->def.id == first);
    }
    w.h.ctx.sys.monsters = nullptr;  // nothing tracked is alive any more at the next poll
    const int64_t gold1 = w.hero->Gold();
    w.h.events.Clear();
    w.Step(30);
    CHECK(w.hero->Gold() == gold1 + 50);
    const EvLog* complete = w.FindLog("zone.event.rescue.complete");
    REQUIRE(complete != nullptr);
    REQUIRE_FALSE(complete->text.args.empty());
    CHECK(complete->text.args[0] == KeyArg("npcName", "sys.event.rescue.fallback"));
  }

  TEST_CASE("random events 13.3: treasure cache gold (30-60 in emerald_plains) and the chest prop fades after 9.2 s") {
    WorldRig w;
    REQUIRE(w.ok);
    const TilePos spot = WorldClearRow(w.zone, 4);
    w.Place(spot.Center());
    const int64_t gold0 = w.hero->Gold();
    w.events.TriggerEvent(RandomEventType::TreasureCache, spot.Center());
    const int64_t got = w.hero->Gold() - gold0;
    CHECK(got >= 30);
    CHECK(got <= 60);
    REQUIRE(w.events.Active().size() == 1);
    CHECK(w.events.Active().front().resolved);  // resolved at once
    CHECK(w.events.Active().front().prop != kNoEntity);
    CHECK_FALSE(w.events.HasUnresolved());
    w.Step(551);  // 9183 ms
    CHECK(w.events.Active().size() == 1);
    w.Step(1);
    CHECK(w.events.Active().empty());
  }

  TEST_CASE("random events: the runtime trigger respects the camp safe zone and fires out of camp") {
    WorldRig w;
    REQUIRE(w.ok);
    Rng& rng = w.h.rng.Get(RngStream::Events);
    // Walk inside the camp radius (camp 1 at (15,15), radius 9): never rolls.
    for (int i = 0; i < 6; ++i) {
      rng.Script({0.0, 0.99});
      w.Place(Vec2(15.0 + i * 0.6, 21));
      w.Step();
    }
    CHECK(w.events.Active().empty());
    rng.ClearScript();
    // Out of camp, after 3 tiles of movement: the forced roll triggers (0.99 -> puzzle).
    const TilePos spot = WorldClearRow(w.zone, 6);
    w.Place(spot.Center());
    w.Step();
    for (int i = 1; i <= 4 && w.events.Active().empty(); ++i) {
      rng.Script({0.0, 0.99});
      w.Place(Vec2(spot.col + i, spot.row));
      w.Step();
      rng.ClearScript();
    }
    REQUIRE(w.events.Active().size() == 1);
    CHECK(w.events.Active().front().type == RandomEventType::EnvironmentalPuzzle);
  }

  // ===================================================================================================================
  // Zone runtime (world 3.4, 7.1, 7.4, 9.2, 9.4; W3, W5-W8)
  // ===================================================================================================================

  TEST_CASE("exits 9.2: strict 1.5-tile trigger, W8 arming, W7 sealed gate message, transition after the fade") {
    WorldRig w(5, /*milestone1=*/false);
    REQUIRE(w.ok);
    REQUIRE_FALSE(w.zone.Exits()[0].sealed);
    w.Step();  // the hero starts far away: armed
    CHECK(w.zone.Exits()[0].armed);
    w.Place(AtExit(-1.5));  // distSq 2.25: not < 2.25
    w.Step();
    CHECK_FALSE(w.h.session.transitioning);
    w.Place(AtExit(-1.4966));  // distSq 2.2398
    w.h.events.Clear();
    w.Step();
    CHECK(w.h.session.transitioning);
    REQUIRE(w.Last<EvZone>() != nullptr);
    CHECK(w.Last<EvZone>()->phase == EvZone::Phase::TransitionBegan);
    CHECK(w.Last<EvZone>()->mapId == "twilight_forest");
    std::string map;
    Vec2 target;
    CHECK_FALSE(w.zone.TakePendingTransition(map, target));
    w.zone.RequestZoneChange("abyss_rift", {3, 3});  // guarded while transitioning
    w.Step(23);  // 400 ms fade: due on step 24
    CHECK_FALSE(w.zone.TakePendingTransition(map, target));
    w.Step(1);
    REQUIRE(w.zone.TakePendingTransition(map, target));
    CHECK(map == "twilight_forest");
    CHECK(target == Vec2(2, 58));
    CHECK_FALSE(w.zone.TakePendingTransition(map, target));
  }

  TEST_CASE("exits W8: arriving inside an exit trigger never bounces; it fires after walking > sqrt(6) away") {
    WorldRig w(5, /*milestone1=*/false, ch1::kZone, true, AtExit(-1));
    REQUIRE(w.ok);
    w.Step(30);
    CHECK_FALSE(w.zone.Exits()[0].armed);
    CHECK_FALSE(w.h.session.transitioning);
    w.Place(AtExit(1.4));  // distSq 1.96... still inside 6 (and inside the trigger)
    w.Step();
    CHECK_FALSE(w.h.session.transitioning);
    w.Place(AtExit(-2.5));  // distSq 6.25 > 6: armed
    w.Step();
    CHECK(w.zone.Exits()[0].armed);
    CHECK_FALSE(w.h.session.transitioning);
    w.Place(AtExit(-1));
    w.Step();
    CHECK(w.h.session.transitioning);
  }

  TEST_CASE("exits W8: arming is strict at distSq exactly 6 (the data's squared constant, not sqrt(6)^2)") {
    const WorldConstants& wc = WD().World().constants;
    CHECK(wc.exitArmDistanceSq == 6);
    CHECK(wc.exitArmDistance * wc.exitArmDistance < 6);  // 5.999999999999999: would arm at exactly 6
    // A hero position whose DistSq to the exit rounds to exactly 6.0 (found for the exit tile (119,60); the REQUIRE
    // fails loudly if the exit ever moves).
    const Vec2 exact6(116.55052350684032, 60.008056640625);
    REQUIRE(DistSq(exact6, AtExit()) == 6.0);
    WorldRig w(5, /*milestone1=*/false, ch1::kZone, true, AtExit(-1));
    REQUIRE(w.ok);
    w.Step();
    REQUIRE_FALSE(w.zone.Exits()[0].armed);
    w.Place(exact6);
    w.Step(3);
    CHECK_FALSE(w.zone.Exits()[0].armed);  // distSq 6 is not > 6
    w.Place(AtExit(-2.5));                 // 6.25
    w.Step();
    CHECK(w.zone.Exits()[0].armed);
  }

  TEST_CASE("tick order (world 17 step 10): exit proximity runs before exploration, lore and the soul echo claim") {
    SimConfig cfg;
    cfg.milestone1 = false;
    auto sim = GameSim::Create(test::RealData(), cfg);
    REQUIRE(sim != nullptr);
    REQUIRE(sim->NewGame(ClassId::Warrior, Difficulty::Normal, 9, 1));
    SimContext& ctx = sim->Context();
    ctx.sys.story->FinishAllBeats();
    sim->Step();
    REQUIRE(ctx.sys.zone->Exits()[0].armed);
    // A soul echo lying in the exit trigger: the same step fires the exit and reclaims the echo.
    SaveData echo;
    echo.soulEcho.present = true;
    echo.soulEcho.mapId = ch1::kZone;
    echo.soulEcho.col = AtExit(-1.5).x;
    echo.soulEcho.row = AtExit(-1.5).y;
    echo.soulEcho.gold = 12;
    ctx.sys.soulEcho->ReadSave(echo);
    ctx.sys.locomotion->Teleport(AtExit(-1.4), TeleportReason::Debug);
    sim->Step();
    size_t exitAt = 0, claimAt = 0;
    bool exitSeen = false, claimSeen = false;
    const std::span<const Event> events = sim->Events();
    for (size_t i = 0; i < events.size(); ++i) {
      if (const EvZone* z = std::get_if<EvZone>(&events[i]);
          z != nullptr && z->phase == EvZone::Phase::TransitionBegan && !exitSeen) {
        exitSeen = true;
        exitAt = i;
      }
      if (const EvLog* l = std::get_if<EvLog>(&events[i]);
          l != nullptr && l->text.key == "zone.soulEcho.claimed" && !claimSeen) {
        claimSeen = true;
        claimAt = i;
      }
    }
    REQUIRE(exitSeen);
    REQUIRE(claimSeen);
    CHECK(exitAt < claimAt);  // web: checkExitProximity (step 10) before the soul echo (step 11)
  }

  TEST_CASE("exits W7: the sealed chapter-2 gate shows the coming-soon line once per approach") {
    WorldRig w;
    REQUIRE(w.ok);
    w.Step();
    w.Place({118, 60});
    w.Step(5);
    CHECK_FALSE(w.h.session.transitioning);
    CHECK(w.Count<EvBanner>() == 1);
    CHECK(w.Last<EvBanner>()->kind == BannerKind::ComingSoon);
    CHECK(w.Last<EvBanner>()->title.key == WD().World().constants.sealedGateMessageKey);
    CHECK(w.Logged("zone.exit.sealedChapter2"));
    w.Place({114, 60});
    w.Step();
    w.Place({118, 60});
    w.Step();
    CHECK(w.Count<EvBanner>() == 2);  // re-armed after leaving
  }

  TEST_CASE("town portal 9.4 / W3: refusals, 1500 ms channel, nearest camp, cancelled by movement input") {
    WorldRig w;
    REQUIRE(w.ok);
    CHECK(w.zone.CanUseTownPortal() == PortalRefusal::AlreadyAtCamp);  // playerStart is 7 tiles from camp 1
    CHECK_FALSE(w.zone.UseTownPortal());
    CHECK(w.Logged("zone.teleport.alreadyAtCamp"));
    w.Place({15, 15 + std::sqrt(80.99)});
    CHECK(w.zone.CanUseTownPortal() == PortalRefusal::AlreadyAtCamp);
    w.Place({15, 24});  // distSq 81
    CHECK(w.zone.CanUseTownPortal() == PortalRefusal::None);
    // W3: the nearest camp, not camps[0].
    w.Place({90, 110});
    w.h.events.Clear();
    REQUIRE(w.zone.UseTownPortal());
    CHECK(w.zone.IsPortaling());
    CHECK(w.zone.PortalDestination() == Vec2(95, 100));
    CHECK(w.zone.CanUseTownPortal() == PortalRefusal::Busy);
    CHECK(w.Logged("zone.teleport.opening"));
    REQUIRE(w.Last<EvTownPortal>() != nullptr);
    CHECK(w.Last<EvTownPortal>()->phase == EvTownPortal::Phase::Started);
    w.Step(89);  // 1483 ms
    CHECK(w.hero->Position() == Vec2(90, 110));
    w.Step(1);   // 1500 ms
    CHECK(w.hero->Position() == Vec2(95, 100));
    CHECK_FALSE(w.zone.IsPortaling());
    CHECK(w.Last<EvTownPortal>()->phase == EvTownPortal::Phase::Completed);
    CHECK(w.SfxCount(SfxId::ZoneTransition) == 1);
    CHECK(w.Count<EvCameraFlash>() == 1);
    CHECK(w.Logged("zone.teleport.toCamp"));
    // Movement input (keyboard / stick / click) cancels the channel.
    w.Place({60, 40});
    REQUIRE(w.zone.UseTownPortal());
    w.Step(10);
    w.loco.SetMoveInput({1, 0});
    CHECK_FALSE(w.zone.IsPortaling());
    CHECK(w.Last<EvTownPortal>()->phase == EvTownPortal::Phase::Cancelled);
    w.loco.SetMoveInput({});
    w.Step(120);
    CHECK(w.hero->Position() == Vec2(60, 40));  // never teleported
    // Right click (7.1 row 2) uses the portal; a Dying hero is refused.
    w.Place({60, 40});
    w.zone.OnPointerPress({30, 30}, PointerButton::Secondary);
    CHECK(w.zone.IsPortaling());
    w.zone.CancelTownPortal();
    w.hero->SetLife(HeroLife::Dying);
    CHECK(w.zone.CanUseTownPortal() == PortalRefusal::Dead);
    CHECK_FALSE(w.zone.UseTownPortal());
  }

  TEST_CASE("town portal W3 / U10: world_constants townPortal.cancelOnMove switches the movement cancel") {
    CHECK(WD().World().constants.townPortalCancelOnMove);  // shipped: movement input cancels (W3)
    const std::unique_ptr<DataStore> data = WorldEditedData("world_constants.json", [](JsonValue& d) {
      d.FindMutable("townPortal")->Set("cancelOnMove", JsonValue::Bool(false));
    });
    REQUIRE_FALSE(data->World().constants.townPortalCancelOnMove);
    WorldRig w(5, true, "", false, {}, *data);
    REQUIRE(w.ok);
    w.Place({60, 40});
    REQUIRE(w.zone.UseTownPortal());
    w.Step(10);
    w.loco.SetMoveInput({1, 0});  // HeroMoveInputMsg -> OnHeroMoveInput: the switch is off, the channel goes on
    CHECK(w.zone.IsPortaling());
    w.Step(5);
    w.loco.SetMoveInput({});
    CHECK(w.zone.IsPortaling());
    w.Step(80);  // 1583 ms since the channel started
    CHECK_FALSE(w.zone.IsPortaling());
    CHECK(w.hero->Position() == w.zone.PortalDestination());
    REQUIRE(w.Last<EvTownPortal>() != nullptr);
    CHECK(w.Last<EvTownPortal>()->phase == EvTownPortal::Phase::Completed);
    // The damage / death cancels (CancelTownPortal from the wiring) do not depend on the switch.
    w.Place({60, 40});
    REQUIRE(w.zone.UseTownPortal());
    w.zone.CancelTownPortal();
    CHECK_FALSE(w.zone.IsPortaling());
    CHECK(w.Last<EvTownPortal>()->phase == EvTownPortal::Phase::Cancelled);
  }

  TEST_CASE("interact 7.4: NPC range 3.0 / 3.01, prompt events, nothing in range is a silent no-op") {
    WorldRig w;
    REQUIRE(w.ok);
    const NpcPlacement* herb = w.zone.FindNpc(ch1::kHerbalist);
    REQUIRE(herb != nullptr);
    w.Place({53, 30});
    InteractTarget t = w.zone.FindInteractTarget();
    CHECK(t.kind == InteractKind::Npc);
    CHECK(t.id == herb->id);
    CHECK(t.key == ch1::kHerbalist);
    w.Place({53.01, 30});
    CHECK(w.zone.FindInteractTarget().kind == InteractKind::None);
    w.h.events.Clear();
    const size_t logs = w.Count<EvLog>();
    CHECK_FALSE(w.zone.Interact());
    CHECK(w.Count<EvLog>() == logs);
    // InteractPrompt follows FindInteractTarget every tick (EvInteractPrompt on change only).
    w.Place({52, 30});
    w.Step();
    REQUIRE(w.Last<EvInteractPrompt>() != nullptr);
    CHECK(w.Last<EvInteractPrompt>()->kind == InteractKind::Npc);
    CHECK(w.zone.Prompt().id == herb->id);
    w.h.events.Clear();
    w.Step(3);
    CHECK(w.Count<EvInteractPrompt>() == 0);
    w.Place({58, 30});
    w.Step();
    REQUIRE(w.Last<EvInteractPrompt>() != nullptr);
    CHECK(w.Last<EvInteractPrompt>()->kind == InteractKind::None);
    // Nearest wins between two NPCs in range (camp 1, W11 slots: blacksmith (16.875,17.375), quest_elder
    // (13.125,17.375); the merchant and the stash keeper are > 3 tiles away from both spots).
    w.Place({15.1, 17.375});
    CHECK(w.zone.FindInteractTarget().key == "blacksmith");
    w.Place({14.9, 17.375});
    CHECK(w.zone.FindInteractTarget().key == ch1::kElder);
  }

  TEST_CASE("pointer 7.1 / W8: a far NPC is walked to (walk-then-act), another order abandons it") {
    WorldRig w;
    REQUIRE(w.ok);
    const NpcPlacement* herb = w.zone.FindNpc(ch1::kHerbalist);
    REQUIRE(herb != nullptr);
    w.zone.OnPointerPress(herb->pos, PointerButton::Primary);
    CHECK(w.zone.Pending().active);
    CHECK(w.zone.Pending().target.kind == InteractKind::Npc);
    CHECK_FALSE(w.loco.Path().empty());
    REQUIRE(w.StepUntil([&w] { return !w.zone.Pending().active; }, 1200));
    CHECK(DistSq(w.hero->Position(), herb->pos) <= 9.0 + 1e-9);
    w.Step();
    CHECK(w.loco.Path().empty());  // stopped at the 3-tile range
    // A new click on the ground replaces the pending interaction.
    w.Place({30, 30});
    w.zone.OnPointerPress(herb->pos, PointerButton::Primary);
    REQUIRE(w.zone.Pending().active);
    const TilePos ground = WorldClearRow(w.zone, 3, 20, 40);
    w.zone.OnPointerPress(ground.Center(), PointerButton::Primary);
    CHECK_FALSE(w.zone.Pending().active);
    CHECK(w.loco.IsHoldMoving());  // row 12 always arms hold-to-move
  }

  TEST_CASE("pointer 7.1 rows 3 / 5 / 12: dead hero ignored, NPC pick radius distSq < 3.24 (strict), ground else") {
    WorldRig w;
    REQUIRE(w.ok);
    const NpcPlacement* herb = w.zone.FindNpc(ch1::kHerbalist);
    REQUIRE(herb != nullptr);
    // A far NPC is picked when distSq(npc, tile) < 3.24 (the press walks there: pending interaction).
    w.Place({30, 30});
    w.zone.OnPointerPress(Vec2(herb->pos.x + 1.79, herb->pos.y), PointerButton::Primary);
    CHECK(w.zone.Pending().active);
    CHECK(w.zone.Pending().target.id == herb->id);
    CHECK_FALSE(w.loco.IsHoldMoving());  // the NPC row consumed the press
    w.loco.Stop();
    w.zone.OnPointerPress(Vec2(herb->pos.x + 1.81, herb->pos.y), PointerButton::Primary);  // distSq > 3.24: ground
    CHECK_FALSE(w.zone.Pending().active);
    CHECK(w.loco.IsHoldMoving());
    // Row 3: a Dying hero's press does nothing (row 2, the portal, is evaluated before it).
    w.loco.Stop();
    w.hero->SetLife(HeroLife::Dying);
    const uint32_t gen = w.loco.MoveGeneration();
    w.zone.OnPointerPress(Vec2(40, 40), PointerButton::Primary);
    CHECK(w.loco.MoveGeneration() == gen);
    CHECK(w.loco.Path().empty());
    CHECK_FALSE(w.loco.IsHoldMoving());
  }

  TEST_CASE("pointer W11: the campfire and the camp lanes are ground clicks, outside every camp NPC's pick disc") {
    WorldRig w;
    REQUIRE(w.ok);
    w.Place(AtCamp(0, 4));  // camp 1, on the open side's lane
    // The fire (respawn / town-portal point), the gate lane and the open-side lane 2 tiles from the fire (inside the
    // blacksmith / elder discs with the first v2 slots).
    for (const Vec2 click : {AtCamp(), AtCamp(-0.5, -3), AtCamp(0, 2)}) {
      CAPTURE(click.x);
      CAPTURE(click.y);
      w.loco.Stop();
      w.zone.OnPointerPress(click, PointerButton::Primary);
      CHECK_FALSE(w.zone.Pending().active);
      CHECK(w.loco.IsHoldMoving());
      CHECK_FALSE(w.loco.Path().empty());
    }
  }

  TEST_CASE("pointer W6 / W8: clicking an exit walks to its inner tile; the proximity trigger fires on arrival") {
    WorldRig w;
    REQUIRE(w.ok);
    TilePos near;
    REQUIRE(w.zone.Paths().FindWalkableNear(ExitTile(-11), 3, near));
    w.Place(near.Center());
    w.Step();
    w.h.events.Clear();
    w.zone.OnPointerPress(AtExit(), PointerButton::Primary);
    CHECK(w.Count<EvBanner>() == 0);  // no instant zone change (W6)
    REQUIRE_FALSE(w.loco.Path().empty());
    CHECK(w.loco.Path().back() == ExitTile(-1));
    REQUIRE(w.StepUntil([&w] { return w.Count<EvBanner>() > 0; }, 600));
    CHECK(DistSq(w.hero->Position(), AtExit()) < 2.25);
  }

  // ===================================================================================================================
  // Locomotion (world 6, 7.2; S5, W3, W4)
  // ===================================================================================================================
  TEST_CASE("locomotion S5: uniform ground speed moveSpeed / 36 tiles/s for click paths, instant start") {
    WorldRig w;
    REQUIRE(w.ok);
    const TilePos a = WorldClearRow(w.zone, 6);
    REQUIRE(a.col > 0);
    w.Place(a.Center());
    CHECK(w.hero->GroundSpeedTilesPerSec() == doctest::Approx(120.0 / 36.0));
    w.loco.SetPath({TilePos{a.col + 1, a.row}, TilePos{a.col + 2, a.row}});
    w.Step(9);  // 9 * 16.67 ms * 3.333 tiles/s = 0.5 tiles
    CHECK(w.hero->Position().x == doctest::Approx(a.col + 0.5));
    CHECK(w.loco.CurrentSpeedTilesPerSec() == doctest::Approx(120.0 / 36.0));
    w.Step(9);
    CHECK(w.hero->Position().x == doctest::Approx(a.col + 1.0));  // the remainder carries over the node
    CHECK(w.loco.IsMoving());
    REQUIRE(w.StepUntil([&w] { return !w.loco.IsMoving(); }, 40));
    CHECK(w.hero->Position() == Vec2(a.col + 2, a.row));  // stops exactly on the last node
  }

  TEST_CASE("locomotion S5: keyboard / stick ramp 90 ms up and 60 ms stop; dead hero does not move") {
    WorldRig w;
    REQUIRE(w.ok);
    const TilePos a = WorldClearRow(w.zone, 6);
    REQUIRE(a.col > 0);
    w.Place(a.Center());
    w.loco.SetMoveInput({1, 0});
    const double full = 120.0 / 36.0;
    const double dt = kSimStepMs / 1000.0;
    double expected = 0;
    double speed = 0;
    for (int i = 0; i < 60; ++i) {
      speed = (std::min)(full, speed + full * kSimStepMs / 90.0);
      expected += speed * dt;
    }
    w.Step(60);
    CHECK(w.hero->Position().x == doctest::Approx(a.col + expected).epsilon(1e-9));
    CHECK(expected == doctest::Approx(full * dt * (55 + (1.0 + 2 + 3 + 4 + 5) * kSimStepMs / 90.0)));
    const double before = w.hero->Position().x;
    w.loco.SetMoveInput({});
    w.Step(4);  // 60 ms stop: coasts for at most 4 steps
    const double coast = w.hero->Position().x - before;
    CHECK(coast > 0);
    CHECK(coast < full * 0.06);
    const double stopped = w.hero->Position().x;
    w.Step(5);
    CHECK(w.hero->Position().x == stopped);
    w.hero->SetHp(0);
    w.loco.SetMoveInput({1, 0});
    w.Step(10);
    CHECK(w.hero->Position().x == stopped);
  }

  TEST_CASE("locomotion W3: pushing diagonally into a wall slides (UniformTiles) and stops in parity mode") {
    for (const HeroSpeedModel model : {HeroSpeedModel::UniformTiles, HeroSpeedModel::IsoPixelParity}) {
      WorldRig w;
      REQUIRE(w.ok);
      w.loco.SetSpeedModel(model);
      int32_t row = -1;
      for (int32_t r = 40; r < 110 && row < 0; ++r) {
        if (w.zone.Walkable(1, r) && w.zone.Walkable(1, r + 1) && w.zone.Walkable(1, r + 2)) row = r;
      }
      REQUIRE(row > 0);
      w.Place({0.505, static_cast<double>(row)});
      w.loco.SetMoveInput({-1, 1});
      w.Step(3);
      const Vec2 p = w.hero->Position();
      if (model == HeroSpeedModel::UniformTiles) {
        CHECK(p.x == 0.505);  // the col component would round into the border wall
        CHECK(p.y > row);    // slides along the row axis
      } else {
        CHECK(p == Vec2(0.505, row));  // web: only the destination is checked, no sliding
      }
    }
  }

  TEST_CASE("locomotion IsoPixelParity (world 21 item 4): arrival on the tick the iso-px sum reaches 35.777") {
    WorldRig w;
    REQUIRE(w.ok);
    w.loco.SetSpeedModel(HeroSpeedModel::IsoPixelParity);
    const TilePos a = WorldClearRow(w.zone, 3);
    w.Place(a.Center());
    w.loco.SetPath({TilePos{a.col + 1, a.row}});
    const double dt = kSimStepMs / 1000.0;
    const double nodePx = std::sqrt(32.0 * 32.0 + 16.0 * 16.0);  // cartToIso(1, 0)
    double speed = 0, sum = 0;
    int expectedSteps = 0;
    while (sum < nodePx) {
      speed += (120.0 - speed) * 8.0 * dt;
      sum += speed * dt;
      ++expectedSteps;
    }
    int steps = 0;
    while (w.loco.IsMoving() && steps < 200) {
      w.Step();
      ++steps;
    }
    CHECK(steps == expectedSteps);
    CHECK(w.hero->Position() == Vec2(a.col + 1, a.row));
  }

  TEST_CASE("hold-to-move 7.2: 120 ms re-path cadence, immediate on a tile change, stand still within 0.6") {
    WorldRig w;
    REQUIRE(w.ok);
    const TilePos a = WorldClearRow(w.zone, 8);
    REQUIRE(a.col > 0);
    w.Place(a.Center());
    const TilePos goal{a.col + 6, a.row};
    w.zone.OnPointerPress(goal.Center(), PointerButton::Primary, 7);
    REQUIRE(w.loco.IsHoldMoving());
    CHECK(w.loco.Hold().pointerId == 7);
    const uint32_t g0 = w.loco.MoveGeneration();
    w.loco.UpdateHold(7, goal, true);
    w.Step(6);  // 100 ms: same tile, path non-empty -> no re-path
    CHECK(w.loco.MoveGeneration() == g0);
    w.Step(2);  // past repathAt (120 ms): re-paths even on the same tile
    CHECK(w.loco.MoveGeneration() == g0 + 1);
    w.loco.UpdateHold(7, TilePos{a.col + 5, a.row}, true);  // a new tile: immediate
    w.Step();
    CHECK(w.loco.MoveGeneration() == g0 + 2);
    CHECK(w.loco.Path().back() == TilePos{a.col + 5, a.row});
    w.loco.UpdateHold(3, TilePos{a.col, a.row}, true);  // another pointer: ignored
    w.Step();
    CHECK(w.loco.Hold().pointer == TilePos{a.col + 5, a.row});
    // Pointer on the hero's own tile: stand still.
    w.loco.UpdateHold(7, RoundToTile(w.hero->Position()), true);
    w.Step();
    CHECK(w.loco.Path().empty());
    // Release: the hold ends; the hero finishes its current path.
    w.loco.UpdateHold(7, TilePos{a.col + 6, a.row}, true);
    w.Step();
    REQUIRE_FALSE(w.loco.Path().empty());
    w.loco.UpdateHold(7, TilePos{a.col + 6, a.row}, false);
    CHECK_FALSE(w.loco.IsHoldMoving());
    REQUIRE(w.StepUntil([&w] { return !w.loco.IsMoving(); }, 200));
    CHECK(w.hero->Position() == Vec2(a.col + 6, a.row));
    // Keyboard input clears the hold.
    w.zone.OnPointerPress(Vec2(a.col + 1, a.row), PointerButton::Primary, 7);
    REQUIRE(w.loco.IsHoldMoving());
    w.loco.SetMoveInput({0, 1});
    w.Step();
    CHECK_FALSE(w.loco.IsHoldMoving());
  }

  // ===================================================================================================================
  // Exploration + hidden areas (10.1, 10.3; world 21 item 9)
  // ===================================================================================================================

  TEST_CASE("exploration 10.3: the fifth check point is the area centre (area.col, area.row), not the bounds midpoint") {
    WorldRig w;
    REQUIRE(w.ok);
    // Explicit bounds whose midpoint (44,44) is not the centre (60,60) (isHiddenAreaExplored, ZoneScene.ts:4841).
    HiddenAreaDef area;
    area.id = "test_bounds";
    area.center = TilePos{60, 60};
    area.radius = 3;
    area.hasBounds = true;
    area.boundsStart = TilePos{40, 40};
    area.boundsEnd = TilePos{48, 48};
    w.Place({44, 44});  // all four corners and the midpoint within the radius-10 disc; the centre is 22.6 tiles away
    w.Step();
    REQUIRE(w.explore.Grid().IsExplored(44, 44));
    REQUIRE_FALSE(w.explore.Grid().IsExplored(60, 60));
    CHECK_FALSE(w.explore.AreaFullyExplored(area));
    w.Place({60, 60});
    w.Step();
    CHECK(w.explore.AreaFullyExplored(area));  // the grid keeps what was seen during this visit
    // Without explicit bounds a fractional radius puts the corners between tiles: never explored (web: undefined).
    HiddenAreaDef frac;
    frac.center = TilePos{60, 60};
    frac.radius = 2.5;
    CHECK(w.explore.Grid().IsExplored(58, 58));
    CHECK_FALSE(w.explore.AreaFullyExplored(frac));
  }
}

// =====================================================================================================================
// Audio rules (audio.md 3, 5.4, 9.8, 10.4; A2, A4-A6)
// =====================================================================================================================
TEST_SUITE("audio") {
  TEST_CASE("3.1 / 3.2 cue rules: combat damage, skills, pickups, quest progress") {
    const AudioRulesDef& r = WD().Audio().rules;
    CHECK(SfxForCombatHit(r, true, true, HitWeight::Crit) == SfxId::Miss);  // dodged first
    CHECK(SfxForCombatHit(r, false, true, HitWeight::Crit) == SfxId::Crit);
    CHECK(SfxForCombatHit(r, false, false, HitWeight::Normal) == SfxId::Hit);
    CHECK(SfxForCombatHit(r, false, false, HitWeight::Light) == SfxId::Hit);
    CHECK(SfxForCombatHit(r, false, false, HitWeight::Tick) == SfxId::Hit);
    CHECK(SfxForCombatHit(r, false, false, HitWeight::Heavy) == SfxId::HitHeavy);  // A6
    CHECK(SfxForCombatHit(r, false, false, HitWeight::Kill) == SfxId::HitHeavy);
    CHECK(SfxForSkill(r, DamageType::Physical) == SfxId::SkillMelee);
    CHECK(SfxForSkill(r, DamageType::Fire) == SfxId::SkillFire);
    CHECK(SfxForSkill(r, DamageType::Ice) == SfxId::SkillIce);
    CHECK(SfxForSkill(r, DamageType::Lightning) == SfxId::SkillLightning);
    CHECK(SfxForSkill(r, DamageType::Poison) == SfxId::SkillBuff);
    CHECK(SfxForSkill(r, DamageType::Arcane) == SfxId::SkillBuff);
    CHECK(SfxForPickup(r, ItemQuality::Normal) == SfxId::LootCommon);
    CHECK(SfxForPickup(r, ItemQuality::Magic) == SfxId::LootMagic);
    CHECK(SfxForPickup(r, ItemQuality::Rare) == SfxId::LootRare);
    CHECK(SfxForPickup(r, ItemQuality::Legendary) == SfxId::LootLegendary);
    CHECK(SfxForPickup(r, ItemQuality::Set) == SfxId::LootLegendary);
    const std::string kill = fixture::Grunt().id;  // a kill target: no progress prefix
    CHECK_FALSE(SfxForQuestProgress(r, 5, 5, kill, true).has_value());  // the fanfare covers it
    CHECK(SfxForQuestProgress(r, 5, 5, kill, false) == SfxId::QuestObjective);
    CHECK(SfxForQuestProgress(r, 2, 5, "mat_scrap", false) == SfxId::QuestProgress);
    CHECK(SfxForQuestProgress(r, 1, 3, "clue_bones", false) == SfxId::QuestProgress);
    CHECK_FALSE(SfxForQuestProgress(r, 1, 5, kill, false).has_value());  // single kills are silent
  }

  TEST_CASE("theme resolution: theme table, dungeon floors -> abyss_rift, ember tower -> plains, else silence") {
    const MusicDirectorDef& def = WD().Audio().music;
    CHECK(ResolveMusicTheme(def, "emerald_plains") == "emerald_plains");
    CHECK(ResolveMusicTheme(def, "menu") == "menu");
    CHECK(ResolveMusicTheme(def, "abyss_rift") == "abyss_rift");
    CHECK(ResolveMusicTheme(def, "dungeon_floor_3") == "abyss_rift");
    CHECK(ResolveMusicTheme(def, "ember_tower") == "emerald_plains");
    CHECK(ResolveMusicTheme(def, "nowhere").empty());
    CHECK(def.zoneFadeSec == 2);
    CHECK(def.stateFadeSec == 1.5);
    CHECK(def.fadeInSec == 1);
    CHECK(def.victoryHoldMs == 3000);
    CHECK(def.bossVictoryHoldMs == 8000);  // A5
    CHECK(def.bossCh1Score == "boss_ch1");
  }

  TEST_CASE("5.4 transition table: zone 2.0 / 1.0, state 1.5 / 1.0, A2 explore resumes, unchanged -> nothing") {
    MusicDirector m(WD().Audio().music);
    m.OnZoneEntered("emerald_plains");
    std::optional<MusicCommand> c = m.TakeCommand();
    REQUIRE(c.has_value());
    CHECK(c->trackKey == "emerald_plains_explore");
    CHECK(c->fadeOutSec == 2.0);
    CHECK(c->fadeInSec == 1.0);
    CHECK(c->loop);
    CHECK(c->restart);
    CHECK_FALSE(m.TakeCommand().has_value());
    m.OnCombatStateChanged(false);  // unchanged state: setState returns
    CHECK_FALSE(m.TakeCommand().has_value());
    m.OnCombatStateChanged(true);
    c = m.TakeCommand();
    REQUIRE(c.has_value());
    CHECK(c->trackKey == "emerald_plains_combat");
    CHECK(c->fadeOutSec == 1.5);
    CHECK(c->fadeInSec == 1.0);
    CHECK(c->restart);
    m.OnCombatStateChanged(false);
    c = m.TakeCommand();
    REQUIRE(c.has_value());
    CHECK(c->trackKey == "emerald_plains_explore");
    CHECK(c->fadeOutSec == 1.5);
    CHECK_FALSE(c->restart);  // A2: the explore track resumes where it left off
    // Re-entering the same zone: no zone fade, no command (already exploring).
    m.OnZoneEntered("emerald_plains");
    CHECK_FALSE(m.TakeCommand().has_value());
    // playTrack is forced: restarts even when unchanged.
    m.PlayTrack("emerald_plains", MusicState::Explore);
    c = m.TakeCommand();
    REQUIRE(c.has_value());
    CHECK(c->trackKey == "emerald_plains_explore");
    CHECK(c->fadeOutSec == 2.0);
    CHECK(c->restart);
    // Unknown zone: silence.
    m.OnZoneEntered("nowhere");
    c = m.TakeCommand();
    REQUIRE(c.has_value());
    CHECK(c->trackKey.empty());
    // A zone change while fighting starts the new zone in explore.
    m.OnZoneEntered("emerald_plains");
    m.OnCombatStateChanged(true);
    m.TakeCommand();
    m.OnZoneEntered("abyss_rift");
    c = m.TakeCommand();
    REQUIRE(c.has_value());
    CHECK(c->trackKey == "abyss_rift_explore");
    CHECK(m.State() == MusicState::Explore);
  }

  TEST_CASE("10.4 boss precedence and the boss-victory hold (A5 8000 ms); 3000 ms for forced victory tracks") {
    MusicDirector m(WD().Audio().music);
    m.Tick(0);
    m.OnZoneEntered("emerald_plains");
    m.TakeCommand();
    m.OnCombatStateChanged(true);
    m.TakeCommand();
    m.OnBossEngaged(ch1::kStoryBoss);
    std::optional<MusicCommand> c = m.TakeCommand();
    REQUIRE(c.has_value());
    CHECK(c->trackKey == "boss_ch1");
    CHECK(c->fadeOutSec == 1.5);
    CHECK(m.State() == MusicState::Boss);
    m.OnCombatStateChanged(false);  // boss outranks combat / explore
    CHECK_FALSE(m.TakeCommand().has_value());
    m.OnCombatStateChanged(true);
    m.OnBossDefeated(ch1::kStoryBoss);
    c = m.TakeCommand();
    REQUIRE(c.has_value());
    CHECK(c->trackKey == "emerald_plains_victory");
    CHECK_FALSE(c->loop);  // one-shot
    m.OnCombatStateChanged(false);  // ignored during the hold
    CHECK_FALSE(m.TakeCommand().has_value());
    m.Tick(7999);
    CHECK_FALSE(m.TakeCommand().has_value());
    m.Tick(8000);
    c = m.TakeCommand();
    REQUIRE(c.has_value());
    CHECK(c->trackKey == "emerald_plains_explore");  // the queued combat flag (off) applies at the end
    // Disengaged without a kill: back to the combat flag's state.
    m.OnCombatStateChanged(true);
    m.OnBossEngaged(ch1::kStoryBoss);
    m.TakeCommand();
    m.OnBossDisengaged();
    c = m.TakeCommand();
    REQUIRE(c.has_value());
    CHECK(c->trackKey == "emerald_plains_combat");
    // A forced victory (epilogue) holds 3000 ms.
    m.Tick(10000);
    m.PlayTrack("abyss_rift", MusicState::Victory);
    m.TakeCommand();
    m.OnCombatStateChanged(false);
    m.Tick(12999);
    CHECK_FALSE(m.TakeCommand().has_value());
    m.Tick(13000);
    c = m.TakeCommand();
    REQUIRE(c.has_value());
    CHECK(c->trackKey == "abyss_rift_explore");
  }

  TEST_CASE("10.4 story lock: sequence music owns the bed; combat changes are queued and applied on release") {
    MusicDirector m(WD().Audio().music);
    m.OnZoneEntered("emerald_plains");
    m.TakeCommand();
    m.SetStoryLock(true);
    m.PlayTrack("abyss_rift", MusicState::Explore);
    std::optional<MusicCommand> c = m.TakeCommand();
    REQUIRE(c.has_value());
    CHECK(c->trackKey == "abyss_rift_explore");
    m.OnCombatStateChanged(true);
    m.OnBossEngaged(ch1::kStoryBoss);
    CHECK_FALSE(m.TakeCommand().has_value());
    m.SetStoryLock(false);
    c = m.TakeCommand();
    REQUIRE(c.has_value());
    CHECK(c->trackKey == "boss_ch1");  // boss > combat once the lock is released
    // A zone entered under the lock starts its track when the lock is released (zone fade).
    MusicDirector z(WD().Audio().music);
    z.OnZoneEntered("emerald_plains");
    z.TakeCommand();
    z.SetStoryLock(true);
    z.OnZoneEntered("abyss_rift");
    CHECK_FALSE(z.TakeCommand().has_value());
    z.SetStoryLock(false);
    c = z.TakeCommand();
    REQUIRE(c.has_value());
    CHECK(c->trackKey == "abyss_rift_explore");
    CHECK(c->fadeOutSec == 2.0);
    CHECK(c->restart);
    // A forced victory under the lock (epilogue) still returns to the sequence zone's explore after 3000 ms, and
    // combat stays queued meanwhile.
    MusicDirector e(WD().Audio().music);
    e.Tick(0);
    e.OnZoneEntered("emerald_plains");
    e.SetStoryLock(true);
    e.PlayTrack("abyss_rift", MusicState::Victory);
    e.TakeCommand();
    e.OnCombatStateChanged(true);
    e.Tick(3000);
    c = e.TakeCommand();
    REQUIRE(c.has_value());
    CHECK(c->trackKey == "abyss_rift_explore");
  }

  TEST_CASE("AudioDirector: bus listeners -> EvSfx / EvMusic (prologue lock, boss bar, kills, quests, NPCs)") {
    WorldRig w;
    REQUIRE(w.ok);
    AudioDirector& a = w.audio;
    a.OnZoneEntered(ZoneEnteredMsg{"emerald_plains", true});
    a.OnStoryState(StoryStateMsg{true, "prologue", "abyss_rift"});
    a.AdvanceRealTime(16);
    REQUIRE(w.Last<EvMusic>() != nullptr);
    CHECK(w.Count<EvMusic>() == 1);  // one command per frame: the sequence's track wins
    CHECK(w.Last<EvMusic>()->trackKey == "abyss_rift_explore");
    a.OnStoryState(StoryStateMsg{false, "", ""});
    a.AdvanceRealTime(16);
    CHECK(w.Last<EvMusic>()->trackKey == "emerald_plains_explore");
    CHECK(w.Last<EvMusic>()->restart);
    // Boss bar + combat -> boss music; bar cleared by the kill -> victory, 8000 ms -> explore.
    a.OnBossBar(BossBarMsg{true, 77, ch1::kStoryBoss, false});
    a.AdvanceRealTime(16);
    CHECK(w.Last<EvMusic>()->trackKey == "emerald_plains_explore");  // not engaged until the combat flag
    a.OnCombatStateChanged(CombatStateChangedMsg{true});
    a.AdvanceRealTime(16);
    CHECK(w.Last<EvMusic>()->trackKey == "boss_ch1");
    a.OnBossBar(BossBarMsg{false, 77, ch1::kStoryBoss, true});
    a.AdvanceRealTime(16);
    CHECK(w.Last<EvMusic>()->trackKey == "emerald_plains_victory");
    a.OnCombatStateChanged(CombatStateChangedMsg{false});
    w.h.events.Clear();
    a.AdvanceRealTime(7000);
    CHECK(w.Count<EvMusic>() == 0);
    a.AdvanceRealTime(1000);
    REQUIRE(w.Last<EvMusic>() != nullptr);
    CHECK(w.Last<EvMusic>()->trackKey == "emerald_plains_explore");
    // SFX listeners.
    w.h.events.Clear();
    MonsterKilledMsg k;
    k.monster = 12;
    k.pos = {40, 41};
    a.OnMonsterKilled(k);
    REQUIRE(w.Last<EvSfx>() != nullptr);
    CHECK(w.Last<EvSfx>()->cue == SfxId::MonsterDeath);
    CHECK(w.Last<EvSfx>()->spatial);  // A4: world SFX are 3D
    CHECK(w.Last<EvSfx>()->source == 12);
    a.OnLevelUp(HeroLevelUpMsg{2});
    CHECK(w.Last<EvSfx>()->cue == SfxId::LevelUp);
    CHECK_FALSE(w.Last<EvSfx>()->spatial);
    a.OnItemPicked(ItemPickedMsg{"u", "b", ItemQuality::Set});
    CHECK(w.Last<EvSfx>()->cue == SfxId::LootLegendary);
    a.OnQuestAccepted(QuestAcceptedMsg{"q"});
    CHECK(w.Last<EvSfx>()->cue == SfxId::NpcInteract);
    QuestProgressMsg qp;
    qp.current = 1;
    qp.required = 5;
    qp.targetId = fixture::Grunt().id;
    const size_t before = w.Count<EvSfx>();
    a.OnQuestProgress(qp);
    CHECK(w.Count<EvSfx>() == before);  // silent kill step
    qp.targetId = "mat_scrap";
    a.OnQuestProgress(qp);
    CHECK(w.Last<EvSfx>()->cue == SfxId::QuestProgress);
    a.OnQuestCompleted(QuestCompletedMsg{"q"});
    CHECK(w.Last<EvSfx>()->cue == SfxId::QuestComplete);
    a.OnQuestTurnedIn(QuestTurnedInMsg{"q"});
    CHECK(w.Last<EvSfx>()->cue == SfxId::QuestComplete);
    a.OnNpcInteracted(NpcInteractedMsg{ch1::kElder, 3});
    CHECK(w.Last<EvSfx>()->cue == SfxId::NpcInteract);
    a.OnNpcInteracted(NpcInteractedMsg{"blacksmith", 4});
    CHECK(w.Last<EvSfx>()->cue == SfxId::PanelOpen);  // SHOP_OPEN
    const size_t n = w.Count<EvSfx>();
    a.OnNpcInteracted(NpcInteractedMsg{"stash", 5});
    CHECK(w.Count<EvSfx>() == n);  // the stash panel's click is UE's
  }
  TEST_CASE("A6: the hit_heavy weights are audio_cues.json rules.port.heavyHitCue.weights, not a code table") {
    const AudioRulesDef& r = WD().Audio().rules;
    CHECK(r.heavyHitWeights == std::vector<HitWeight>{HitWeight::Heavy, HitWeight::Crit, HitWeight::Kill});
    CHECK(r.heavyHitCue == SfxId::HitHeavy);
    AudioRulesDef only = r;
    // (count, value) rather than {Kill}: GCC 13 -O2 + sanitizers reports a false -Warray-bounds on the 1-byte
    // initializer_list memmove (as in sim_test.cpp).
    only.heavyHitWeights = std::vector<HitWeight>(1, HitWeight::Kill);
    CHECK(SfxForCombatHit(only, false, false, HitWeight::Heavy) == SfxId::Hit);
    CHECK(SfxForCombatHit(only, false, false, HitWeight::Kill) == SfxId::HitHeavy);
    CHECK(SfxForCombatHit(only, false, true, HitWeight::Kill) == SfxId::Crit);  // crit still first
    only.heavyHitWeights.clear();
    CHECK(SfxForCombatHit(only, false, false, HitWeight::Kill) == SfxId::Hit);
  }

  TEST_CASE("10.4 rule 4: bossMusic maps a boss to its score; a boss without one keeps the combat track") {
    // Which boss is scored with which track is content (content_ch1_test.cpp); here: the lookup and the switch.
    const MusicDirectorDef& def = WD().Audio().music;
    REQUIRE(def.bossMusic.size() == 1);
    const std::string boss = def.bossMusic[0].first;
    const std::string score = def.bossMusic[0].second;
    CHECK_FALSE(boss.empty());
    CHECK_FALSE(score.empty());
    REQUIRE(def.BossScore(boss) != nullptr);
    CHECK(*def.BossScore(boss) == score);
    CHECK(def.BossScore("werewolf_alpha") == nullptr);
    MusicDirector m(def);
    m.Tick(0);
    m.OnZoneEntered("twilight_forest");
    m.TakeCommand();
    m.OnCombatStateChanged(true);
    std::optional<MusicCommand> c = m.TakeCommand();
    REQUIRE(c.has_value());
    CHECK(c->trackKey == "twilight_forest_combat");
    m.OnBossEngaged("werewolf_alpha");  // a later-chapter story boss: no boss score
    CHECK_FALSE(m.TakeCommand().has_value());
    CHECK(m.State() == MusicState::Combat);
    m.OnBossDisengaged();
    CHECK_FALSE(m.TakeCommand().has_value());
    m.OnBossEngaged(boss);
    c = m.TakeCommand();
    REQUIRE(c.has_value());
    CHECK(c->trackKey == score);
    // Released story lock with an unscored boss and the combat flag on: the queued combat state applies.
    MusicDirector q(def);
    q.OnZoneEntered("twilight_forest");
    q.TakeCommand();
    q.SetStoryLock(true);
    q.OnCombatStateChanged(true);
    q.OnBossEngaged("werewolf_alpha");
    CHECK_FALSE(q.TakeCommand().has_value());
    q.SetStoryLock(false);
    c = q.TakeCommand();
    REQUIRE(c.has_value());
    CHECK(c->trackKey == "twilight_forest_combat");
  }

  TEST_CASE("10.4 rule 2: StoryStateMsg carries the sequence's music state (epilogue / credits victory)") {
    WorldRig w;
    REQUIRE(w.ok);
    AudioDirector& a = w.audio;
    a.OnZoneEntered(ZoneEnteredMsg{"emerald_plains", false});
    a.AdvanceRealTime(16);
    a.OnStoryState(StoryStateMsg{true, "epilogue", "abyss_rift", "victory"});  // sequence(EPILOGUE, 'abyss_rift', 'victory')
    a.AdvanceRealTime(16);
    REQUIRE(w.Last<EvMusic>() != nullptr);
    CHECK(w.Last<EvMusic>()->trackKey == "abyss_rift_victory");
    CHECK_FALSE(w.Last<EvMusic>()->loop);
    a.AdvanceRealTime(3000);  // the forced victory holds 3000 ms, then the sequence zone's explore under the lock
    CHECK(w.Last<EvMusic>()->trackKey == "abyss_rift_explore");
    a.OnStoryState(StoryStateMsg{false, "epilogue", ""});  // the sequence's finally: playTrack(<map>, 'explore')
    a.AdvanceRealTime(16);
    CHECK(w.Last<EvMusic>()->trackKey == "emerald_plains_explore");
    CHECK(w.Last<EvMusic>()->restart);
    a.OnStoryState(StoryStateMsg{true, "credits", "abyss_rift", "victory"});
    a.AdvanceRealTime(16);
    CHECK(w.Last<EvMusic>()->trackKey == "abyss_rift_victory");
    a.OnStoryState(StoryStateMsg{false, "credits", ""});
    // Only explore / victory are sequence states; anything else plays explore.
    a.OnStoryState(StoryStateMsg{true, "prologue", "abyss_rift", "boss"});
    a.AdvanceRealTime(16);
    CHECK(w.Last<EvMusic>()->trackKey == "abyss_rift_explore");
  }

  TEST_CASE("10.4 rule 2 (GameSim): the prologue plays abyss_rift explore; the chapter card plays over the zone track") {
    auto sim = GameSim::Create(test::RealData(), SimConfig{});
    REQUIRE(sim != nullptr);
    REQUIRE(sim->NewGame(ClassId::Mage, Difficulty::Normal, 4, 1));
    StoryDirector& story = *sim->Context().sys.story;
    REQUIRE(story.Playback().playing);
    REQUIRE(story.Playback().beat.id == "prologue");
    std::vector<std::string> tracks, beats;
    auto frame = [&]() {
      sim->Frame(16);
      for (const Event& e : sim->Events()) {
        if (const EvMusic* m = std::get_if<EvMusic>(&e)) {
          tracks.push_back(m->trackKey);
          beats.push_back(story.Playback().playing ? story.Playback().beat.id : std::string());
        }
      }
    };
    frame();
    REQUIRE(tracks.size() == 1);
    CHECK(tracks.back() == "abyss_rift_explore");  // StoryDirector.ts:83 sequence(PROLOGUE, 'abyss_rift')
    for (int i = 0; i < 4000 && story.Playback().beat.id == "prologue"; ++i) {
      story.Skip();
      frame();
    }
    REQUIRE(story.Playback().playing);
    REQUIRE(story.Playback().beat.id == "chapter_emerald_plains");
    CHECK(story.IsCinematic());
    REQUIRE(tracks.size() == 2);
    CHECK(tracks.back() == "emerald_plains_explore");  // the prologue's end, not the end of the whole queue
    CHECK(beats.back() == "chapter_emerald_plains");
    story.FinishAllBeats();
    frame();
    CHECK_FALSE(story.IsCinematic());
    CHECK(tracks.size() == 2);  // the card's end and the idle director change nothing
  }
}
