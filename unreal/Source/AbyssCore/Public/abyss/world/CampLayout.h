// Camp layout v2 (DECISIONS W11; replaces the web offsets of world-map-nav.md 3.4 and the NPC slots of
// quests-story-ch1.md 6.2): the one table of camp props and camp NPC slots, in camp-relative tile units (x = +col,
// y = +row, 1 tile = 100 uu; (0,0) = the camp centre = the campfire). MapGen CampProps / ApplyCampBlockers /
// CollectAnchors, ZoneRuntime::EnterZone (camp NPCs) and the UE camp dressing (AAbyssZoneActor::BuildCamps) all read
// it. The MapGen camp stamp (11 x 11, palisade at +-5, 2-tile gate at x -1 / 0 on row -5, open +row side) is unchanged.
//
// Tuned for the fixed W1 camera (yaw 45, pitch -50: the camera sits on the camp's (-x,-y) side). No tents (removed at the
// user's request; the camp is the fire, the well, banners, torches and the supply crates / barrels): the NPCs stand
// round the fire, so no NPC stands inside or behind a prop on screen (world_test "camp layout v2" checks it against the
// camera numbers in world_constants). Every slot's click-pick disc (world_constants interact.npcPickRadiusSq) keeps clear
// of the campfire and of the gate / open-side lanes, and the blockers leave no walkable tile the fire cannot reach
// (world_test). Positions are eighth / tenth tiles; front directions come from std::sqrt only.
//
// Owner area: world. Pure, header-only.
#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "abyss/base/Types.h"

namespace abyss::camp {

// One camp prop: type (art id camp_<type>), centre offset, whether it blocks walking, and whether its front (the mesh's
// face) turns to the camp centre (else it faces the camera).
struct CampPropSpec {
  const char* type;
  double x, y;
  bool blocking;
  bool facesCentre;
};

// MapGen camp stamp half size (world-map-nav.md 3.4: palisade rows / columns at +-5).
inline constexpr int32_t kCampHalfSize = 5;

// The body a blocking prop keeps the hero out of, in tiles: half across (mesh X) and half depth (mesh Y = the front) =
// Data/assets.json boundsCm of SM_Prop_Camp_Well / _Crate / _Barrel.
struct CampBlockBody {
  const char* type;
  double halfAcross, halfDepth;
};
inline constexpr std::array<CampBlockBody, 3> kCampBlockBodies{{
    {"well", 0.76051, 0.60697},
    {"crate", 0.39901, 0.39901},
    {"barrel", 0.35761, 0.35761},
}};
// A blocking prop blocks its anchor tile and every tile whose centre lies within its body grown by this pad (hero body
// half-width 0.25 + 0.05), so a hero standing on any walkable tile centre keeps >= 0.3 tile from every prop body.
// Click paths stop on tile centres; keyboard / stick steering only tests the rounded tile of the hero centre
// (Locomotion), so it can still bring the hero up to half a tile closer (DECISIONS W11).
inline constexpr double kCampBlockPad = 0.30;

// The block body of a prop type; nullptr when the type has none (it then blocks only its anchor tile).
inline const CampBlockBody* FindCampBlockBody(std::string_view type) {
  for (const CampBlockBody& b : kCampBlockBodies) {
    if (type == b.type) return &b;
  }
  return nullptr;
}

// Camp NPC slots: npcs[i] stands at kCampNpcSlots[i % 6] (eighth tiles: exact in binary). Slots 0-2 stand round the
// far half of the fire (where the layout-v2 tents had their doors), slot 3 beside crate 16, slots 4-5 are spares (only
// reached with 5-6 camp NPCs; the shipped camps have at most 4). Every slot is >= 1.94 tiles from the fire and >= 1.87
// from the gate / open-side lane centre lines, outside the 1.8-tile pick disc.
inline constexpr std::array<Vec2, 6> kCampNpcSlots{{
    {1.875, 2.375},    // 0: tile (2,2), beyond the fire (screen top)
    {1.375, -1.375},   // 1: tile (1,-1), screen left of the fire
    {-1.875, 2.375},   // 2: tile (-2,2), screen right of the fire
    {-2.75, -0.75},    // 3: tile (-3,-1), by crate 16 (lower right)
    {2.25, 1.0},       // 4: tile (2,1), toward the well (spare)
    {-2.375, -2.25},   // 5: tile (-2,-2) (spare)
}};

// Kept free for the hearthstone of a later milestone (DECISIONS Q3: not placed in milestone 1).
inline constexpr Vec2 kCampHearthReserve{-2.0, -3.25};

// Camp props in push order (render order and the per-camp hash order of CampProps).
// No tents (user request, DECISIONS W11): the three layout-v2 tents at (2.9, 3.75), (2.6, -2.6), (-2.9, 3.75) are gone;
// every other prop and every NPC slot stays where it was.
inline constexpr std::array<CampPropSpec, 18> kCampProps{{
    {"campfire", 0.0, 0.0, false, false},   // 0: the respawn point (not blocking)
    {"well", 4.0, -0.15, true, true},       // 1: anchor (4,0); its body also blocks (4,-1)
    {"banner", -2.1, -4.3, false, false},   // 2-3: flank the gate (+-1.6 about its centre x -0.5)
    {"banner", 1.1, -4.3, false, false},
    {"banner", -5.0, 4.2, false, false},    // 4-5: the open side's corners
    {"banner", 5.0, 4.2, false, false},
    {"torch", -4.4, -4.4, false, false},    // 6-7: the gate-side corners
    {"torch", 4.4, -4.4, false, false},
    {"torch", -4.55, -1.55, false, false},  // 8-11: 0.45 inside the side walls
    {"torch", 4.55, -1.55, false, false},
    {"torch", -4.55, 2.2, false, false},
    {"torch", 4.55, 2.2, false, false},
    {"torch", -5.0, 5.2, false, false},     // 12-13: the open side
    {"torch", 5.0, 5.2, false, false},
    {"crate", -3.9, -3.4, true, true},      // 14-15: supply corner (screen bottom)
    {"barrel", -3.4, -4.2, true, true},
    {"crate", -3.9, -0.1, true, true},      // 16: beside slot 3
    {"barrel", 3.6, -4.25, true, true},     // 17: the gate-side corner (screen left)
}};

// The idle facing of camp NPC slot `slot` (unit, tile space; std::sqrt only): every slot looks at the campfire (the
// camp's focus; with the tents gone there is no door to look out of). The hero within 3 tiles overrides it (QuestWorld
// heroNear).
inline Vec2 CampNpcFacing(size_t slot) {
  const Vec2 p = kCampNpcSlots[slot % kCampNpcSlots.size()];
  const double len = std::sqrt(p.x * p.x + p.y * p.y);
  return Vec2(-p.x / len, -p.y / len);
}

}  // namespace abyss::camp
