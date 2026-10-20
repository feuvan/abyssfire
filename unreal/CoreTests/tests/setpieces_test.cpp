// The UE set-piece layer's table and placement maths (dark remake, implementation-plan.md 4 U4; set_pieces.json,
// abyss/data/SetPieceData.h, abyss/world/SetPieces.h). Two halves:
//   - systems: the loader, Finalize's reference checks, anchors, rings, facings, the socket transform and the covered
//     tiles, on synthetic pieces;
//   - the shipped table, checked generically (no content id is spelled here, content_ch1_test.cpp's isolation rule):
//     every piece of every zone stands on its generated map (not on water or rock, no blocking decoration under it),
//     every game id resolves in the art manifest (or a fallback does) with a valid variant, and no piece that is not
//     walk-over covers a tile of the paths the bot's route of that zone walks or an interaction point (NPCs, lore, clues,
//     explore centres, story decorations, entrances, defend targets, hidden rewards, exits, the player start).
#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "TestContent.h"
#include "TestUtil.h"
#include "abyss/base/Math.h"
#include "abyss/base/StrUtil.h"
#include "abyss/data/DataStore.h"
#include "abyss/world/CampLayout.h"
#include "abyss/world/Grid.h"
#include "abyss/world/MapGen.h"
#include "abyss/world/Pathfinding.h"
#include "abyss/world/SetPieces.h"
#include "doctest/doctest.h"

using namespace abyss;
namespace ch1 = abyss::test::ch1;

namespace {

constexpr double kSpEps = 1e-9;
// A hero standing on a tile centre overlaps a mesh when the centre lies within its bounds grown by the body half-width.
constexpr double kSpHeroPad = 0.25;

// The real tables with set_pieces.json replaced by `json` (Finalize runs over the real maps, quests and lore).
DataLoadReport SpLoadWith(const std::string& json, DataStore& store) {
  DataLoadReport report;
  store.LoadAll(
      [&json](std::string_view name, std::string& out) {
        if (name == "set_pieces.json") {
          out = json;
          return true;
        }
        out = test::ReadFile(test::DataDir() + "/" + std::string(name));
        return !out.empty();
      },
      report);
  return report;
}

bool SpHasIssue(const std::vector<DataIssue>& issues, std::string_view needle) {
  for (const DataIssue& e : issues) {
    if (e.file == "set_pieces.json" && (e.message.find(needle) != std::string::npos || e.path.find(needle) != std::string::npos)) {
      return true;
    }
  }
  return false;
}
// Shape problems fail the load; references that do not resolve are warnings (presentation only: the piece is skipped).
bool SpHasError(const DataLoadReport& r, std::string_view needle) { return SpHasIssue(r.errors, needle); }
bool SpHasWarning(const DataLoadReport& r, std::string_view needle) {
  return r.Ok() && SpHasIssue(r.warnings, needle);
}

SetPieceDef SpPiece(SetPieceFacingKind facing) {
  SetPieceDef d;
  d.id = "probe";
  d.gameId = "probe_mesh";
  d.anchor = SetPieceAnchorKind::Tile;
  d.tile = Vec2(10, 20);
  d.facing = facing;
  return d;
}

double SpAngleDiff(double a, double b) {
  double d = std::fmod(a - b, 360.0);
  if (d < -180.0) d += 360.0;
  if (d > 180.0) d -= 360.0;
  return std::fabs(d);
}

// ---- the bot's route as waypoints (the shipped-table check) ----

// Where an NPC of the map stands: a camp slot (camp layout v2) or its field position.
bool SpNpcPos(const MapDef& map, const std::string& npc, Vec2& out) {
  for (const MapCampDef& c : map.camps) {
    for (size_t i = 0; i < c.npcs.size(); ++i) {
      if (c.npcs[i] == npc) {
        out = c.pos.Center() + camp::kCampNpcSlots[i % camp::kCampNpcSlots.size()];
        return true;
      }
    }
  }
  for (const FieldNpcDef& n : map.fieldNpcs) {
    if (n.npcId == npc) {
      out = n.pos.Center();
      return true;
    }
  }
  return false;
}

// Legs (from, to) the route walks, in order. A town portal breaks the chain (the hero reappears at the nearest camp).
std::vector<std::pair<Vec2, Vec2>> SpRouteLegs(const DataStore& data, const MapDef& map, const AutoPilotRoute& route) {
  std::vector<std::pair<Vec2, Vec2>> legs;
  Vec2 at = map.playerStart.Center();
  const auto walk = [&](Vec2 to) {
    legs.emplace_back(at, to);
    at = to;
  };
  for (const AutoPilotStep& s : route.steps) {
    const QuestDef* q = s.quest.empty() ? nullptr : data.FindQuest(s.quest);
    Vec2 p;
    switch (s.kind) {
      case AutoPilotStepKind::Talk:
      case AutoPilotStepKind::Tree:
      case AutoPilotStepKind::Visit:
      case AutoPilotStepKind::Shop:
      case AutoPilotStepKind::TownTrip:
        if (SpNpcPos(map, s.npc, p)) walk(p);
        break;
      case AutoPilotStepKind::Gather:
        if (q != nullptr && s.obj >= 0 && static_cast<size_t>(s.obj) < q->objectives.size()) {
          walk(q->objectives[static_cast<size_t>(s.obj)].gatherArea.Center());
        }
        break;
      case AutoPilotStepKind::Hunt:
      case AutoPilotStepKind::Boss:
      case AutoPilotStepKind::Explore:
      case AutoPilotStepKind::GoTo: walk(s.tile.Center()); break;
      case AutoPilotStepKind::Clues:
        for (const TilePos& t : s.tiles) walk(t.Center());
        break;
      case AutoPilotStepKind::Escort:
        if (q != nullptr && q->hasEscortNpc) {
          walk(q->escortNpc.start.Center());
          walk(q->escortNpc.dest.Center());
        }
        break;
      case AutoPilotStepKind::Defend:
        if (q != nullptr && q->hasDefendTarget) {
          const double a = q->defendTarget.approachDeg * kPi / 180.0;
          walk(q->defendTarget.pos.Center() + Vec2(DetCos(a), DetSin(a)) * s.standOff);
        }
        break;
      case AutoPilotStepKind::PortalHome: {
        double best = 1e18;
        for (const MapCampDef& c : map.camps) {
          const double d = DistSq(at, c.pos.Center());
          if (d < best) {
            best = d;
            p = c.pos.Center();
          }
        }
        at = p;
        break;
      }
      default: break;
    }
  }
  return legs;
}

// Tiles of the A* paths of every leg (ends moved onto a walkable tile first, as a click would).
std::set<std::pair<int32_t, int32_t>> SpRouteTiles(const ZoneGrid& grid, const std::vector<std::pair<Vec2, Vec2>>& legs,
                                                   size_t& routed) {
  std::set<std::pair<int32_t, int32_t>> tiles;
  const Pathfinder pf(grid);
  const auto walkable = [&](Vec2 v) {
    TilePos t = RoundToTile(v);
    if (grid.Walkable(t.col, t.row)) return t;
    TilePos near;
    return pf.FindWalkableNear(t, 4, near) ? near : t;
  };
  routed = 0;
  for (const auto& [from, to] : legs) {
    const TilePos a = walkable(from);
    const TilePos b = walkable(to);
    std::vector<TilePos> path;
    if (pf.FindPath(a.col, a.row, b.col, b.row, path)) ++routed;
    tiles.emplace(a.col, a.row);
    for (const TilePos& t : path) tiles.emplace(t.col, t.row);
  }
  return tiles;
}

// Points the hero interacts with (a set piece must not stand on them).
std::vector<std::pair<std::string, TilePos>> SpInteractionPoints(const DataStore& data, const MapDef& map) {
  std::vector<std::pair<std::string, TilePos>> out;
  const auto add = [&out](std::string what, Vec2 v) { out.emplace_back(std::move(what), RoundToTile(v)); };
  add("player start", map.playerStart.Center());
  for (const MapCampDef& c : map.camps) {
    for (size_t i = 0; i < c.npcs.size(); ++i) {
      add("camp npc " + c.npcs[i], c.pos.Center() + camp::kCampNpcSlots[i % camp::kCampNpcSlots.size()]);
    }
  }
  for (const FieldNpcDef& n : map.fieldNpcs) add("npc " + n.npcId, n.pos.Center());
  for (const MapExitDef& e : map.exits) add("exit to " + e.targetMap, e.pos.Center());
  for (const SubDungeonEntranceDef& e : map.subDungeonEntrances) add("entrance " + e.id, e.pos.Center());
  for (const StoryDecorationDef& d : map.storyDecorations) add("story decoration " + d.id, d.pos.Center());
  for (const HiddenAreaDef& h : map.hiddenAreas) {
    for (const HiddenRewardDef& r : h.rewards) add("hidden reward of " + h.id, r.pos.Center());
  }
  for (const LoreEntryDef* l : data.Lore().ForZone(map.id)) add("lore " + l->id, l->pos.Center());
  for (const QuestDef& q : data.Quests().quests) {
    if (q.zone != map.id) continue;
    for (const QuestClueDef& c : q.clues) add("clue " + c.id, c.pos.Center());
    for (const QuestObjectiveDef& o : q.objectives) {
      if (o.type == ObjectiveType::Explore && o.hasLocation) add("explore centre of " + q.id, o.location.Center());
    }
    if (q.hasEscortNpc) {
      add("escort start of " + q.id, q.escortNpc.start.Center());
      add("escort end of " + q.id, q.escortNpc.dest.Center());
    }
    if (q.hasDefendTarget) add("defend target of " + q.id, q.defendTarget.pos.Center());
  }
  return out;
}

// A point that a piece is anchored on (its own entrance / defend target) is the piece's to cover.
bool SpOwnAnchor(const SetPieceDef& def, const std::string& what) {
  if (def.anchor == SetPieceAnchorKind::Entrance) return what == "entrance " + def.ref;
  if (def.anchor == SetPieceAnchorKind::DefendTarget) return what == "defend target of " + def.ref;
  return false;
}

}  // namespace

TEST_SUITE("world") {
  TEST_CASE("set pieces: the loader reads every anchor, facing, ring and socket field") {
    DataStore store;
    const std::string zone(ch1::kZone);
    const std::string json = R"({"schemaVersion": 1, "zones": {")" + zone + R"(": {"pieces": [
      {"id": "a", "area": "x", "gameId": "g", "fallbackIds": ["f1", "f2"], "variant": 1,
       "anchor": {"kind": "tile", "col": 12.5, "row": 7}, "offset": [1, -2], "ring": null,
       "facing": {"kind": "yaw", "deg": 90}, "turnDeg": 5, "scale": 1.25, "partners": ["p"],
       "effects": [{"socket": "s", "recipe": "r"}], "emitters": [{"socket": "k", "emitter": "e"}],
       "cues": [{"cues": ["c1", "c2"], "socket": "b", "recipe": "r2"}], "echoes": [], "echoTurnDeg": 30, "occluder": true,
       "walkOver": "why", "yieldsToDefendTarget": true, "note": "n"},
      {"id": "b", "gameId": "g", "anchor": {"kind": "camp", "camp": 1, "prop": "campfire"},
       "ring": {"count": 4, "radius": 2, "startDeg": 10, "skip": [1], "variants": [0, 1]},
       "facing": {"kind": "ringCentre"}}
    ]}}})";
    const DataLoadReport report = SpLoadWith(json, store);
    REQUIRE_MESSAGE(report.Ok(), report.Summary());
    const SetPieceZoneDef* z = store.SetPieces().ForZone(zone);
    REQUIRE(z != nullptr);
    REQUIRE(z->pieces.size() == 2);
    const SetPieceDef& a = z->pieces[0];
    CHECK(a.fallbackIds == std::vector<std::string>{"f1", "f2"});
    CHECK(a.variant == 1);
    CHECK(a.anchor == SetPieceAnchorKind::Tile);
    CHECK(a.tile.x == doctest::Approx(12.5));
    CHECK(a.offset.y == doctest::Approx(-2));
    CHECK_FALSE(a.hasRing);
    CHECK(a.facing == SetPieceFacingKind::Yaw);
    CHECK(a.facingDeg == doctest::Approx(90));
    CHECK(a.turnDeg == doctest::Approx(5));
    CHECK(a.scale == doctest::Approx(1.25));
    CHECK(a.echoTurnDeg == doctest::Approx(30));
    REQUIRE(a.effects.size() == 1);
    CHECK(a.effects[0].recipe == "r");
    REQUIRE(a.emitters.size() == 1);
    CHECK(a.emitters[0].emitter == "e");
    REQUIRE(a.cues.size() == 1);
    CHECK(a.cues[0].cues.size() == 2);
    CHECK(a.occluder);
    CHECK(a.walkOver == "why");
    CHECK(a.yieldsToDefendTarget);
    const SetPieceDef& b = z->pieces[1];
    CHECK(b.anchor == SetPieceAnchorKind::Camp);
    CHECK(b.camp == 1);
    CHECK(b.ref == "campfire");
    REQUIRE(b.hasRing);
    CHECK(b.ring.stepDeg == doctest::Approx(90));  // default 360 / count
    CHECK(b.variant == -1);
    CHECK(b.walkOver.empty());
    CHECK(store.SetPieces().ForZone("no_such_zone") == nullptr);
  }

  TEST_CASE("set pieces: shape errors fail the load, unresolved references warn") {
    const std::string zone(ch1::kZone);
    const auto load = [&zone](const std::string& pieces) {
      DataStore store;
      return SpLoadWith(R"({"schemaVersion": 1, "zones": {")" + zone + R"(": {"pieces": [)" + pieces + "]}}}", store);
    };
    CHECK(SpHasError(load(R"({"id": "a", "gameId": "g", "anchor": {"kind": "tile", "col": 1, "row": 1}, "facing": {"kind": "ringCentre"}})"),
                     "ringCentre facing needs a ring"));
    CHECK(SpHasError(load(R"({"id": "a", "gameId": "g", "anchor": {"kind": "tile", "col": 1, "row": 1}, "facing": {"kind": "camera"},
                              "ring": {"count": 0, "radius": 1}})"),
                     "ring count"));
    CHECK(SpHasError(load(R"({"id": "a", "gameId": "g", "anchor": {"kind": "tile", "col": 1, "row": 1}, "facing": {"kind": "camera"}},
                             {"id": "a", "gameId": "g", "anchor": {"kind": "tile", "col": 2, "row": 1}, "facing": {"kind": "camera"}})"),
                     "duplicate set-piece id"));
    CHECK(SpHasWarning(load(R"({"id": "a", "gameId": "g", "anchor": {"kind": "camp", "camp": 99}, "facing": {"kind": "camera"}})"),
                     "has no camp 99"));
    CHECK(SpHasWarning(load(R"({"id": "a", "gameId": "g", "anchor": {"kind": "camp", "camp": 1, "prop": "tent"}, "facing": {"kind": "camera"}})"),
                     "no camp prop 'tent'"));
    CHECK(SpHasWarning(load(R"({"id": "a", "gameId": "g", "anchor": {"kind": "entrance", "id": "nope"}, "facing": {"kind": "camera"}})"),
                     "no entrance 'nope'"));
    CHECK(SpHasWarning(load(R"({"id": "a", "gameId": "g", "anchor": {"kind": "region", "id": "nope"}, "facing": {"kind": "camera"}})"),
                     "no region 'nope'"));
    CHECK(SpHasWarning(load(R"({"id": "a", "gameId": "g", "anchor": {"kind": "defendTarget", "quest": "nope"}, "facing": {"kind": "camera"}})"),
                     "is no defend quest"));
    CHECK(SpHasWarning(load(R"({"id": "a", "gameId": "g", "anchor": {"kind": "tile", "col": 1, "row": 1}, "facing": {"kind": "camera"},
                              "echoes": ["nope"]})"),
                     "is no echo lore"));
    CHECK(SpHasError(load(R"({"id": "a", "gameId": "g", "anchor": {"kind": "sky"}, "facing": {"kind": "camera"}})"), "unknown value 'sky'"));
    DataStore store;
    const DataLoadReport unknownZone = SpLoadWith(R"({"schemaVersion": 1, "zones": {"atlantis": {"pieces": []}}})", store);
    CHECK(SpHasWarning(unknownZone, "unknown zone 'atlantis'"));
  }

  TEST_CASE("set pieces: facings give the actor yaw in the mesh frame") {
    const DataStore& data = test::RealData();
    const MapDef* map = data.FindMap(ch1::kZone);
    REQUIRE(map != nullptr);
    const SetPieceFrame frame;  // mesh yaw offset -90, camera yaw 45
    const auto place = [&](const SetPieceDef& d) {
      SetPieceZoneDef z;
      z.pieces.push_back(d);
      std::vector<SetPiecePlacement> out;
      ExpandSetPieces(z, *map, data, frame, out);
      REQUIRE(out.size() == 1);
      SetPiecePlacement p = out[0];
      p.def = nullptr;  // z dies with this lambda
      return p;
    };
    // yaw: literal (+ turn), normalised to [0, 360)
    SetPieceDef yaw = SpPiece(SetPieceFacingKind::Yaw);
    yaw.facingDeg = -90;
    yaw.turnDeg = 10;
    CHECK(place(yaw).yawDeg == doctest::Approx(280));
    // camera: the front toward the W1 camera = a camp prop without a front (BuildCamps: cameraYaw + 180 + mesh offset)
    CHECK(place(SpPiece(SetPieceFacingKind::Camera)).yawDeg == doctest::Approx(135));
    // front 0 (east) -> yaw -90 = 270
    SetPieceDef front = SpPiece(SetPieceFacingKind::Front);
    front.facingDeg = 0;
    CHECK(place(front).yawDeg == doctest::Approx(270));
    // tile: toward (10, 30) from (10, 20) = +row (90) -> yaw 0
    SetPieceDef tile = SpPiece(SetPieceFacingKind::Tile);
    tile.toward = Vec2(10, 30);
    const SetPiecePlacement tp = place(tile);
    CHECK(SpAngleDiff(tp.yawDeg, 0) < 1e-9);
    SetPiecePlacement withDef = tp;
    withDef.def = &tile;
    const Vec2 f = SetPieceFront(withDef, frame);
    CHECK(f.x == doctest::Approx(0).epsilon(1e-12));
    CHECK(f.y == doctest::Approx(1));
    // anchor: an offset piece turns toward its anchor point (a camp's hearth): from (+3, +4) the front is (-3, -4) / 5
    SetPieceDef anchor = SpPiece(SetPieceFacingKind::Anchor);
    anchor.offset = Vec2(3, 4);
    const double expect = std::atan2(-4.0, -3.0) * 180.0 / kPi - 90.0;
    CHECK(SpAngleDiff(place(anchor).yawDeg, expect) < 1e-9);
    // anchor without an offset falls back to the camera
    CHECK(place(SpPiece(SetPieceFacingKind::Anchor)).yawDeg == doctest::Approx(135));
  }

  TEST_CASE("set pieces: anchors resolve on the map, rings expand with their gaps, variants and facing") {
    const DataStore& data = test::RealData();
    const MapDef* map = data.FindMap(ch1::kZone);
    REQUIRE(map != nullptr);
    REQUIRE_FALSE(map->camps.empty());
    SetPieceDef campFire = SpPiece(SetPieceFacingKind::Camera);
    campFire.anchor = SetPieceAnchorKind::Camp;
    campFire.camp = 1;
    campFire.ref = "campfire";
    Vec2 at;
    REQUIRE(ResolveSetPieceAnchor(campFire, *map, data, at));
    CHECK(at.x == doctest::Approx(ch1::kCamp.col));
    CHECK(at.y == doctest::Approx(ch1::kCamp.row));
    campFire.ref = "well";
    REQUIRE(ResolveSetPieceAnchor(campFire, *map, data, at));
    CHECK(at.x == doctest::Approx(ch1::kCamp.col + camp::kCampProps[1].x));
    campFire.camp = static_cast<int32_t>(map->camps.size()) + 1;
    CHECK_FALSE(ResolveSetPieceAnchor(campFire, *map, data, at));
    for (const SubDungeonEntranceDef& e : map->subDungeonEntrances) {
      SetPieceDef d = SpPiece(SetPieceFacingKind::Camera);
      d.anchor = SetPieceAnchorKind::Entrance;
      d.ref = e.id;
      REQUIRE(ResolveSetPieceAnchor(d, *map, data, at));
      CHECK(RoundToTile(at) == e.pos);
    }
    for (const MapRegionDef& r : map->regions) {
      SetPieceDef d = SpPiece(SetPieceFacingKind::Camera);
      d.anchor = SetPieceAnchorKind::Region;
      d.ref = r.id;
      REQUIRE(ResolveSetPieceAnchor(d, *map, data, at));
      CHECK(RoundToTile(at) == r.center);
    }

    // A ring of 12 at r 9, every member facing the centre, the third left out, variants cycling.
    SetPieceDef ring = SpPiece(SetPieceFacingKind::RingCentre);
    ring.offset = Vec2(5, 0);  // the ring's centre is anchor + offset
    ring.hasRing = true;
    ring.ring.count = 12;
    ring.ring.radius = 9;
    ring.ring.startDeg = -11.4592;
    ring.ring.stepDeg = -30;
    ring.ring.skip = {2};
    ring.ring.variants = {1, 0, 0};
    SetPieceZoneDef z;
    z.pieces.push_back(ring);
    std::vector<SetPiecePlacement> out;
    ExpandSetPieces(z, *map, data, SetPieceFrame{}, out);
    REQUIRE(out.size() == 11);
    const Vec2 centre(15, 20);
    for (const SetPiecePlacement& p : out) {
      CAPTURE(p.member);
      CHECK(p.member != 2);
      CHECK(Dist(p.pos, centre) == doctest::Approx(9.0));
      const double expectAngle = ring.ring.startDeg + ring.ring.stepDeg * p.member;
      CHECK(SpAngleDiff(std::atan2(p.pos.y - centre.y, p.pos.x - centre.x) * 180.0 / kPi, expectAngle) < 1e-6);
      const Vec2 f = SetPieceFront(p, SetPieceFrame{});
      const Vec2 toCentre = (centre - p.pos) / 9.0;
      CHECK(f.x == doctest::Approx(toCentre.x).epsilon(1e-9));
      CHECK(f.y == doctest::Approx(toCentre.y).epsilon(1e-9));
      CHECK(p.variant == ring.ring.variants[static_cast<size_t>(p.member) % 3]);
    }
    // Deterministic: the same placements twice (Det* trigonometry).
    std::vector<SetPiecePlacement> again;
    ExpandSetPieces(z, *map, data, SetPieceFrame{}, again);
    REQUIRE(again.size() == out.size());
    for (size_t i = 0; i < out.size(); ++i) {
      CHECK(again[i].pos.x == out[i].pos.x);
      CHECK(again[i].yawDeg == out[i].yawDeg);
    }
    // An unresolved anchor is skipped.
    SetPieceZoneDef bad;
    SetPieceDef lost = SpPiece(SetPieceFacingKind::Camera);
    lost.anchor = SetPieceAnchorKind::Region;
    lost.ref = "nowhere";
    bad.pieces.push_back(lost);
    out.clear();
    ExpandSetPieces(bad, *map, data, SetPieceFrame{}, out);
    CHECK(out.empty());
  }

  TEST_CASE("set pieces: sockets and bounds turn with the placement") {
    SetPieceDef d = SpPiece(SetPieceFacingKind::Yaw);
    d.scale = 2.0;
    SetPiecePlacement p;
    p.def = &d;
    p.pos = Vec2(38, 48);
    // yaw 0: the mesh frame is tile space (x cm -> col, y cm -> row), times the scale
    p.yawDeg = 0;
    Vec2 t = SetPieceLocalToTile(p, -190, 0);
    CHECK(t.x == doctest::Approx(38 - 3.8));
    CHECK(t.y == doctest::Approx(48));
    // yaw 90: +X turns to +row, so mesh -X lies north (-row)
    d.scale = 1.0;
    p.yawDeg = 90;
    t = SetPieceLocalToTile(p, -190, 0);
    CHECK(t.x == doctest::Approx(38).epsilon(kSpEps));
    CHECK(t.y == doctest::Approx(46.1));
    t = SetPieceLocalToTile(p, 0, 100);
    CHECK(t.x == doctest::Approx(37));
    CHECK(t.y == doctest::Approx(48).epsilon(kSpEps));
    // yaw -45 (= 315): (100, 0) -> (0.7071, -0.7071)
    p.yawDeg = 315;
    t = SetPieceLocalToTile(p, 100, 0);
    CHECK(t.x == doctest::Approx(38 + std::sqrt(0.5)));
    CHECK(t.y == doctest::Approx(48 - std::sqrt(0.5)));

    // Covered tiles: a 3 x 1 tile wall (x -150..150, y -50..50) at (10, 10), axis-aligned, no pad: three tiles in a row.
    p.pos = Vec2(10, 10);
    p.yawDeg = 0;
    std::vector<TilePos> tiles;
    SetPieceCoveredTiles(p, -150, -50, 150, 50, 0.0, tiles);
    CHECK(tiles == std::vector<TilePos>{{9, 10}, {10, 10}, {11, 10}});
    // turned 90: a column
    p.yawDeg = 90;
    tiles.clear();
    SetPieceCoveredTiles(p, -150, -50, 150, 50, 0.0, tiles);
    CHECK(tiles == std::vector<TilePos>{{10, 9}, {10, 10}, {10, 11}});
    // the hero pad reaches the next centres of a 1 x 1 at a fractional spot
    p.pos = Vec2(10.4, 10);
    p.yawDeg = 0;
    tiles.clear();
    SetPieceCoveredTiles(p, -50, -50, 50, 50, kSpHeroPad, tiles);
    CHECK(tiles == std::vector<TilePos>{{10, 10}, {11, 10}});
    // a degenerate rectangle covers nothing
    tiles.clear();
    SetPieceCoveredTiles(p, 50, 50, -50, -50, 0.0, tiles);
    CHECK(tiles.empty());
  }

  TEST_CASE("set pieces (shipped): every piece stands on its map, its art resolves and no route or interaction is covered") {
    const DataStore& data = test::RealData();
    REQUIRE(data.IsFinalized());
    REQUIRE(data.SetPieces().loaded);
    REQUIRE(data.Assets().loaded);
    {
      // Finalize resolves every reference of the shipped table (a stale one would only warn and skip the piece).
      DataStore fresh;
      const DataLoadReport report = SpLoadWith(test::ReadFile(test::DataDir() + "/set_pieces.json"), fresh);
      REQUIRE(report.Ok());
      for (const DataIssue& w : report.warnings) CHECK_MESSAGE(w.file != "set_pieces.json", w.path << ": " << w.message);
    }
    const SetPieceFrame frame{-90.0, data.World().constants.cameraYawDeg};
    size_t pieces = 0;
    size_t checkedRoutes = 0;
    for (const SetPieceZoneDef& z : data.SetPieces().zones) {
      CAPTURE(z.zone);
      const MapDef* map = data.FindMap(z.zone);
      REQUIRE(map != nullptr);
      const ZoneGrid grid = BuildZoneGrid(data, *map);
      std::vector<SetPiecePlacement> placements;
      ExpandSetPieces(z, *map, data, frame, placements);
      // every piece (and every ring member not skipped) is placed
      size_t expected = 0;
      for (const SetPieceDef& d : z.pieces) expected += d.hasRing ? static_cast<size_t>(d.ring.count) - d.ring.skip.size() : 1;
      CHECK(placements.size() == expected);
      pieces += placements.size();

      // the bot's route through this zone (if it has one)
      const AutoPilotRoute* route = data.AutoPilotRoutes().ForZone(z.zone);
      std::set<std::pair<int32_t, int32_t>> routeTiles;
      if (route != nullptr) {
        size_t routed = 0;
        const std::vector<std::pair<Vec2, Vec2>> legs = SpRouteLegs(data, *map, *route);
        routeTiles = SpRouteTiles(grid, legs, routed);
        CHECK(legs.size() > 10);
        CHECK(routed * 10 >= legs.size() * 9);  // the legs are real walks (not unreachable)
        ++checkedRoutes;
      }
      const std::vector<std::pair<std::string, TilePos>> points = SpInteractionPoints(data, *map);
      // generated decorations by tile
      std::set<std::pair<int32_t, int32_t>> blockingDecor;
      for (const Decoration& d : grid.Decorations()) {
        if (d.blocking) blockingDecor.emplace(JsRoundInt(d.col), JsRoundInt(d.row));
      }

      for (const SetPieceDef& d : z.pieces) {
        for (const std::string& id : d.echoes) {
          const LoreEntryDef* lore = data.Lore().Find(id);
          REQUIRE_MESSAGE(lore != nullptr, d.id << " poses unknown echo " << id);
          CHECK(lore->zone == z.zone);
          CHECK(lore->spriteType == "echo");
        }
      }
      for (const SetPiecePlacement& p : placements) {
        const SetPieceDef& d = *p.def;
        CAPTURE(d.id);
        CAPTURE(p.member);
        CHECK(grid.InBounds(JsRoundInt(p.pos.x), JsRoundInt(p.pos.y)));
        // art: the id (its theme variant first in a themed zone) or a fallback; partners too; the variant exists. The
        // meshes checked: the placement's variant, or every variant when the presentation picks one by tile hash.
        std::vector<std::string> ids{d.gameId};
        ids.insert(ids.end(), d.fallbackIds.begin(), d.fallbackIds.end());
        std::vector<const AssetEntryDef*> variants;
        for (const std::string& id : ids) {
          if (!variants.empty()) break;
          if (map->hasPresentationTheme) {
            variants = data.Assets().FindAllByGameId(id + "@" + std::string(EnumName(map->presentationTheme)));
          }
          if (variants.empty()) variants = data.Assets().FindAllByGameId(id);
        }
        REQUIRE_MESSAGE(!variants.empty(), "no asset for " << d.gameId);
        CHECK(p.variant < static_cast<int32_t>(variants.size()));
        std::vector<const AssetEntryDef*> meshes;
        if (p.variant >= 0 && static_cast<size_t>(p.variant) < variants.size()) {
          meshes.push_back(variants[static_cast<size_t>(p.variant)]);
        } else {
          meshes = variants;
        }
        for (const std::string& partner : d.partners) {
          std::vector<const AssetEntryDef*> pa;
          if (map->hasPresentationTheme) {
            pa = data.Assets().FindAllByGameId(partner + "@" + std::string(EnumName(map->presentationTheme)));
          }
          if (pa.empty()) pa = data.Assets().FindAllByGameId(partner);
          CHECK_MESSAGE(!pa.empty(), "no asset for partner " << partner);
          meshes.insert(meshes.end(), pa.begin(), pa.end());
        }
        for (const AssetEntryDef* mesh : meshes) {
          CAPTURE(mesh->name);
          REQUIRE(mesh->hasBounds);
          // on the map: the tile under its origin and every tile its bounds cover, none off the map, on water or rock,
          // nor a blocking decoration (W5: an invisible wall inside the mesh)
          std::vector<TilePos> under{RoundToTile(p.pos)};
          SetPieceCoveredTiles(p, mesh->boundsMinCm[0], mesh->boundsMinCm[1], mesh->boundsMaxCm[0], mesh->boundsMaxCm[1], 0.0, under);
          for (const TilePos& t : under) {
            CAPTURE(t.col);
            CAPTURE(t.row);
            REQUIRE(grid.InBounds(t.col, t.row));
            CHECK(grid.Tile(t.col, t.row) != TileType::Water);
            CHECK(grid.Tile(t.col, t.row) != TileType::Wall);
            CHECK_MESSAGE(blockingDecor.count({t.col, t.row}) == 0, "a blocking decoration stands inside the piece");
          }
          if (!d.walkOver.empty()) continue;
          // not walk-over: no route tile, no interaction point under it (with the hero's body pad)
          std::vector<TilePos> covered{RoundToTile(p.pos)};
          SetPieceCoveredTiles(p, mesh->boundsMinCm[0], mesh->boundsMinCm[1], mesh->boundsMaxCm[0], mesh->boundsMaxCm[1],
                               kSpHeroPad, covered);
          for (const TilePos& t : covered) {
            CAPTURE(t.col);
            CAPTURE(t.row);
            CHECK_MESSAGE(routeTiles.count({t.col, t.row}) == 0, "a route leg walks through the piece");
            for (const auto& [what, at] : points) {
              if (at == t && !SpOwnAnchor(d, what)) CHECK_MESSAGE(false, "the piece covers " << what);
            }
          }
        }
      }
    }
    CHECK(pieces > 0);
    CHECK(checkedRoutes > 0);
  }
}
