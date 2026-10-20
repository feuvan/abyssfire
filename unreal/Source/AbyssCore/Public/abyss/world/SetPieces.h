// Placement maths of the UE set-piece layer (dark remake, implementation-plan.md 4 U4; table: abyss/data/SetPieceData.h).
// One deterministic function the presentation (AAbyssZoneActor::BuildSetPieces) and the tests share: anchors resolved
// on the map, offsets, rings and facings expanded into placements (tile position + actor yaw), and the transforms the
// presentation needs on top (a socket in the mesh frame -> tiles, the tiles a piece's bounds cover).
//
// Conventions: tile space (x = col / UE X, y = row / UE Y; 1 tile = 100 cm); angles in degrees, 0 = +col, 90 = +row,
// which is also UE's yaw (FRotator yaw turns +X toward +Y). An actor yaw is the mesh's own frame: a mesh whose front is
// Blender -Y (= UE +Y, the art kit's convention) faces direction `yaw - meshYawOffsetDeg`. Det* trigonometry (presentation
// only, but bit-identical on every platform, so a test pin holds everywhere). Pure; owner area: world.
#pragma once

#include <cstdint>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/data/SetPieceData.h"

namespace abyss {

class DataStore;
struct MapDef;

// The two presentation constants a placement depends on: the art manifest's mesh yaw offset (units.characterForward:
// meshes face +Y, the actor turns them by -90) and the W1 camera yaw (WorldConstants::cameraYawDeg).
struct SetPieceFrame {
  double meshYawOffsetDeg = -90.0;
  double cameraYawDeg = 45.0;
};

struct SetPiecePlacement {
  const SetPieceDef* def = nullptr;
  int32_t member = 0;    // ring member index (0 without a ring)
  Vec2 pos;              // tile position (sub-tile)
  double yawDeg = 0.0;   // actor yaw (UE degrees, mesh frame), in [0, 360)
  int32_t variant = -1;  // asset index among the game id's variants, -1 = the presentation's tile hash
};

// The anchor point of a piece on a map (before its offset); false when the anchor does not resolve there (unknown camp,
// entrance, region, quest without a defend target).
ABYSS_API bool ResolveSetPieceAnchor(const SetPieceDef& def, const MapDef& map, const DataStore& data, Vec2& out);

// Every placement of a zone's pieces (data order; a ring's members in member order, skipped members left out). A piece
// whose anchor does not resolve is skipped (DataStore::Finalize reports it).
ABYSS_API void ExpandSetPieces(const SetPieceZoneDef& zone, const MapDef& map, const DataStore& data,
                               const SetPieceFrame& frame, std::vector<SetPiecePlacement>& out);

// The front direction (unit, tile space) of a placement.
ABYSS_API Vec2 SetPieceFront(const SetPiecePlacement& p, const SetPieceFrame& frame);

// A point of the mesh frame (UE cm: a manifest socket's locCm x / y, scaled by the piece's scale) in tile coordinates.
ABYSS_API Vec2 SetPieceLocalToTile(const SetPiecePlacement& p, double localXCm, double localYCm);

// The tiles (row-major, appended) whose centre lies inside the placement's rectangle [minX, maxX] x [minY, maxY] (UE cm,
// mesh frame: the asset's boundsCm, scaled by the piece's scale) grown by padTiles: the tiles a hero standing on would
// overlap the mesh (pad 0.25 = the hero's body half-width).
ABYSS_API void SetPieceCoveredTiles(const SetPiecePlacement& p, double minXCm, double minYCm, double maxXCm, double maxYCm,
                                    double padTiles, std::vector<TilePos>& out);

}  // namespace abyss
