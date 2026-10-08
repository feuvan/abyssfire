// Zone grid: tile types and the walkability (collision) grid.
// Spec: world-map-nav.md 1.1 (tile space), 2.1 (tile types, walkable = not water/wall/campWall), 3.4 (camp blockers
// mutate the collision grid once at zone load), 15 (3D dressing is layered on top); DECISIONS W5 (tall decoration
// footprints block walking), W10.
//
// Owner area: world. Pure value type, row-major.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Enums.h"
#include "abyss/base/Types.h"

namespace abyss {

enum class TileType : uint8_t { Grass = 0, Dirt = 1, Stone = 2, Water = 3, Wall = 4, Camp = 5, CampWall = 6 };
ABYSS_ENUM_STRINGS(TileType, "grass", "dirt", "stone", "water", "wall", "camp", "campWall")

constexpr bool IsWalkableTile(TileType t) {
  return t != TileType::Water && t != TileType::Wall && t != TileType::CampWall;
}

// A placed decoration (generator output; render data passed through to UE, plus W5 blocking).
struct Decoration {
  std::string type;
  double col = 0, row = 0;  // tile position (generator jitter included)
  double scale = 1;
  bool blocking = false;    // W5: tall decoration footprint baked into the collision grid
};

class ABYSS_API ZoneGrid {
 public:
  ZoneGrid() = default;
  ZoneGrid(int32_t cols, int32_t rows);

  int32_t Cols() const { return cols_; }
  int32_t Rows() const { return rows_; }
  bool InBounds(int32_t col, int32_t row) const { return col >= 0 && row >= 0 && col < cols_ && row < rows_; }

  TileType Tile(int32_t col, int32_t row) const;
  void SetTile(int32_t col, int32_t row, TileType t);  // also resets walkability from the tile type
  // collisions[row][col] (true = walkable); out of bounds = false.
  bool Walkable(int32_t col, int32_t row) const;
  void SetWalkable(int32_t col, int32_t row, bool walkable);
  // Walkability of the tile under a float position (JS round of both coordinates).
  bool WalkableAt(Vec2 p) const;
  // Rebuilds the collision grid from the tile types.
  void RebuildWalkability();

  const std::vector<TileType>& Tiles() const { return tiles_; }
  const std::vector<uint8_t>& Walk() const { return walk_; }
  std::vector<Decoration>& Decorations() { return decor_; }
  const std::vector<Decoration>& Decorations() const { return decor_; }

 private:
  int32_t cols_ = 0, rows_ = 0;
  std::vector<TileType> tiles_;
  std::vector<uint8_t> walk_;
  std::vector<Decoration> decor_;
};

}  // namespace abyss
