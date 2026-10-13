// Camp layout v2 (DECISIONS W11; replaces the web offsets of world-map-nav.md 3.4 and the NPC slots of
// quests-story-ch1.md 6.2): the one table of camp props and camp NPC slots, in camp-relative tile units (x = +col,
// y = +row, 1 tile = 100 uu; (0,0) = the camp centre = the campfire). MapGen CampProps / ApplyCampBlockers /
// CollectAnchors, ZoneRuntime::EnterZone (camp NPCs) and the UE camp dressing (AAbyssZoneActor::BuildCamps) all read
// it. The MapGen camp stamp (11 x 11, palisade at +-5, 2-tile gate at x -1 / 0 on row -5, open +row side) is unchanged.
//
// Tuned for the fixed W1 camera (yaw 45, pitch -50: the camera sits on the camp's (-x,-y) side): three tents set back in
// the far / side corners with their doors to the fire, NPC slots 0-2 on the door axes 0.73-0.76 m in front of the door
// plane, so no NPC stands inside or behind a prop on screen (world_test "camp layout v2" checks it against the camera
// numbers in world_constants). Every slot's click-pick disc (world_constants interact.npcPickRadiusSq) keeps clear of the
// campfire and of the gate / open-side lanes, and the blockers leave no walkable tile the fire cannot reach (world_test).
// Positions are eighth / tenth tiles; front directions come from std::sqrt only.
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

// One camp prop: type (art id camp_<type>), centre offset, whether it blocks walking, whether its front (the mesh's
// door / face) turns to the camp centre (else it faces the camera), and the NPC slot that stands at its door (-1).
struct CampPropSpec {
  const char* type;
  double x, y;
  bool blocking;
  bool facesCentre;
  int32_t ownerSlot;
};

// MapGen camp stamp half size (world-map-nav.md 3.4: palisade rows / columns at +-5).
inline constexpr int32_t kCampHalfSize = 5;

// Tent canvas body half sizes in tiles (Art/blender/environment/camp.py build_tent W = 1.25 across, D = 0.98 deep; the
// manifest boundsCm +-170.6 x +-139.6 cm add the guy ropes and pegs). Front = the door side.
inline constexpr double kTentCanvasHalfAcross = 1.25;
inline constexpr double kTentCanvasHalfDepth = 0.98;

// The body a blocking prop keeps the hero out of, in tiles: half across (mesh X) and half depth (mesh Y = the front).
// Tent = the canvas above; the rest = Data/assets.json boundsCm of SM_Prop_Camp_Well / _Crate / _Barrel.
struct CampBlockBody {
  const char* type;
  double halfAcross, halfDepth;
};
inline constexpr std::array<CampBlockBody, 4> kCampBlockBodies{{
    {"tent", kTentCanvasHalfAcross, kTentCanvasHalfDepth},
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

// Camp NPC slots: npcs[i] stands at kCampNpcSlots[i % 6] (eighth tiles: exact in binary). Slots 0-2 stand at the
// doors of tents 1-3, slot 3 beside crate 19, slots 4-5 are spares (only reached with 5-6 camp NPCs; the shipped camps
// have at most 4). Every slot is >= 1.94 tiles from the fire and >= 1.87 from the gate / open-side lane centre lines,
// outside the 1.8-tile pick disc.
inline constexpr std::array<Vec2, 6> kCampNpcSlots{{
    {1.875, 2.375},    // 0: tile (2,2), in front of tent 1 (screen top)
    {1.375, -1.375},   // 1: tile (1,-1), in front of tent 2 (screen left)
    {-1.875, 2.375},   // 2: tile (-2,2), in front of tent 3 (screen right)
    {-2.75, -0.75},    // 3: tile (-3,-1), by crate 19 (lower right)
    {2.25, 1.0},       // 4: tile (2,1), toward the well (spare)
    {-2.375, -2.25},   // 5: tile (-2,-2) (spare)
}};

// Kept free for the hearthstone of a later milestone (DECISIONS Q3: not placed in milestone 1).
inline constexpr Vec2 kCampHearthReserve{-2.0, -3.25};

// Camp props in push order (render order and the per-camp hash order of CampProps).
inline constexpr std::array<CampPropSpec, 21> kCampProps{{
    {"campfire", 0.0, 0.0, false, false, -1},   // the respawn point (not blocking)
    {"tent", 2.9, 3.75, true, true, 0},         // 1: screen top, anchor (3,4)
    {"tent", 2.6, -2.6, true, true, 1},         // 2: screen left, anchor (3,-3); its back seals the corner
    {"tent", -2.9, 3.75, true, true, 2},        // 3: screen right, anchor (-3,4)
    {"well", 4.0, -0.15, true, true, -1},       // 4: anchor (4,0); blocks (4,-1) too, so no pocket by tent 2
    {"banner", -2.1, -4.3, false, false, -1},   // 5-6: flank the gate (+-1.6 about its centre x -0.5)
    {"banner", 1.1, -4.3, false, false, -1},
    {"banner", -5.0, 4.2, false, false, -1},    // 7-8: the open side's corners
    {"banner", 5.0, 4.2, false, false, -1},
    {"torch", -4.4, -4.4, false, false, -1},    // 9-10: the gate-side corners
    {"torch", 4.4, -4.4, false, false, -1},
    {"torch", -4.55, -1.55, false, false, -1},  // 11-14: 0.45 inside the side walls
    {"torch", 4.55, -1.55, false, false, -1},
    {"torch", -4.55, 2.2, false, false, -1},
    {"torch", 4.55, 2.2, false, false, -1},
    {"torch", -5.0, 5.2, false, false, -1},     // 15-16: the open side
    {"torch", 5.0, 5.2, false, false, -1},
    {"crate", -3.9, -3.4, true, true, -1},      // 17-18: supply corner (screen bottom)
    {"barrel", -3.4, -4.2, true, true, -1},
    {"crate", -3.9, -0.1, true, true, -1},      // 19: beside slot 3
    {"barrel", 3.6, -4.25, true, true, -1},     // 20: tucked behind tent 2
}};

// The idle facing of camp NPC slot `slot` (unit, tile space; std::sqrt only): a tent owner looks out of its door (the
// tent's front), every other slot looks at the campfire. The hero within 3 tiles overrides it (QuestWorld heroNear).
inline Vec2 CampNpcFacing(size_t slot) {
  for (const CampPropSpec& s : kCampProps) {
    if (s.ownerSlot >= 0 && static_cast<size_t>(s.ownerSlot) == slot) {
      const double len = std::sqrt(s.x * s.x + s.y * s.y);
      return Vec2(-s.x / len, -s.y / len);
    }
  }
  const Vec2 p = kCampNpcSlots[slot % kCampNpcSlots.size()];
  const double len = std::sqrt(p.x * p.x + p.y * p.y);
  return Vec2(-p.x / len, -p.y / len);
}

}  // namespace abyss::camp
