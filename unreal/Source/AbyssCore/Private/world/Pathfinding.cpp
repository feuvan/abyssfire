// A* (world-map-nav.md section 5) and walkable-tile searches. STUB: owner area world.
#include "abyss/base/Platform.h"

#include "abyss/world/Pathfinding.h"

#include "abyss/base/Assert.h"

namespace abyss {

bool Pathfinder::FindPath(double startCol, double startRow, double endCol, double endRow,
                          std::vector<TilePos>& out) const {
  ABYSS_UNIMPLEMENTED();
  out.clear();
  return false;
}

bool Pathfinder::FindWalkableNear(TilePos centre, int32_t radius, TilePos& out) const {
  ABYSS_UNIMPLEMENTED();
  return false;
}

bool Pathfinder::FindWalkableTile(TilePos preferred, int32_t radius, TilePos& out) const {
  ABYSS_UNIMPLEMENTED();
  if (grid_->Walkable(preferred.col, preferred.row)) {
    out = preferred;
    return true;
  }
  return false;
}

}  // namespace abyss
