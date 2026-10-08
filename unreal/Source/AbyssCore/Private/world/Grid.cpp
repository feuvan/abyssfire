// Zone grid (world-map-nav.md 1.1, 2.1). Implemented. Owner area: world.
#include "abyss/base/Platform.h"

#include "abyss/world/Grid.h"

#include "abyss/base/Math.h"

namespace abyss {

ZoneGrid::ZoneGrid(int32_t cols, int32_t rows) : cols_(cols), rows_(rows) {
  const size_t n = static_cast<size_t>(cols > 0 ? cols : 0) * static_cast<size_t>(rows > 0 ? rows : 0);
  tiles_.assign(n, TileType::Grass);
  walk_.assign(n, 1);
}

TileType ZoneGrid::Tile(int32_t col, int32_t row) const {
  if (!InBounds(col, row)) return TileType::Wall;
  return tiles_[static_cast<size_t>(row) * static_cast<size_t>(cols_) + static_cast<size_t>(col)];
}

void ZoneGrid::SetTile(int32_t col, int32_t row, TileType t) {
  if (!InBounds(col, row)) return;
  const size_t i = static_cast<size_t>(row) * static_cast<size_t>(cols_) + static_cast<size_t>(col);
  tiles_[i] = t;
  walk_[i] = IsWalkableTile(t) ? 1 : 0;
}

bool ZoneGrid::Walkable(int32_t col, int32_t row) const {
  if (!InBounds(col, row)) return false;
  return walk_[static_cast<size_t>(row) * static_cast<size_t>(cols_) + static_cast<size_t>(col)] != 0;
}

void ZoneGrid::SetWalkable(int32_t col, int32_t row, bool walkable) {
  if (!InBounds(col, row)) return;
  walk_[static_cast<size_t>(row) * static_cast<size_t>(cols_) + static_cast<size_t>(col)] = walkable ? 1 : 0;
}

bool ZoneGrid::WalkableAt(Vec2 p) const { return Walkable(JsRoundInt(p.x), JsRoundInt(p.y)); }

void ZoneGrid::RebuildWalkability() {
  for (size_t i = 0; i < tiles_.size(); ++i) walk_[i] = IsWalkableTile(tiles_[i]) ? 1 : 0;
}

}  // namespace abyss
