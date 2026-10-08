// Unit conversions - the single constant table of DECISIONS S4 (never duplicate these numbers elsewhere).
//
// * 1 tile = 100 uu (1 m). Tile (col,row) -> world X = col*100, Y = row*100 (Z comes from the terrain, UE side).
// * Pixel-derived web constants convert with the specs' factors:
//     36 px per tile for projectile / arrow timing and hero speed (combat-feel.md 0, S4, S5),
//     45 px per tile for VFX sizes and heights of upright things (combat-feel.md 0).
// * The web's exact iso screen metric (64x32 tiles) is kept as IsoPx() for the few rules that must reproduce it.
// DataStore validation checks these constants against projectile_timing.json / world_constants.json.
#pragma once

#include <cmath>

#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"

namespace abyss {

inline constexpr double kTileSizeUU = 100.0;            // S4
inline constexpr double kIsoTileWidthPx = 64.0;         // web tile size (render-only metric)
inline constexpr double kIsoTileHeightPx = 32.0;
inline constexpr double kProjectilePxPerTile = 36.0;    // S4: projectile flight, arrow delays, monster bolts
inline constexpr double kVfxPxPerTile = 45.0;           // S4: VFX sizes (FxKit TILE_R)
inline constexpr double kHeroSpeedPxPerTile = 36.0;     // S5: hero ground speed = moveSpeed / 36 tiles/s
inline constexpr double kSimTickHz = 60.0;              // S1

// World position (uu) of a tile-space point, ground plane only.
struct WorldXY {
  double x = 0.0;
  double y = 0.0;
};
constexpr WorldXY TileToWorld(Vec2 t) { return {t.x * kTileSizeUU, t.y * kTileSizeUU}; }
constexpr Vec2 WorldToTile(WorldXY w) { return {w.x / kTileSizeUU, w.y / kTileSizeUU}; }

// The web's iso screen distance between two tile points (classes-stats-skills.md 0):
// sqrt(((dc - dr) * 32)^2 + ((dc + dr) * 16)^2).
inline double IsoPx(double dc, double dr) {
  const double a = (dc - dr) * (kIsoTileWidthPx * 0.5);
  const double b = (dc + dr) * (kIsoTileHeightPx * 0.5);
  return std::sqrt(a * a + b * b);
}

// S4 px-equivalent of a tile distance for projectile/arrow timing rules (tileDist x 36).
constexpr double TilesToProjectilePx(double tiles) { return tiles * kProjectilePxPerTile; }
// VFX px -> tiles (px / 45).
constexpr double VfxPxToTiles(double px) { return px / kVfxPxPerTile; }
// S5: uniform hero ground speed in tiles/s for a web moveSpeed (iso px/s units, 120 base -> 3.333 tiles/s).
constexpr double HeroTilesPerSecond(double moveSpeed) { return moveSpeed / kHeroSpeedPxPerTile; }

}  // namespace abyss
