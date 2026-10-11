// A* on the zone collision grid with the web's exact tie-breaking.
// Spec: world-map-nav.md section 5 (rounded inputs, end checked first, 8 directions in the listed order, no corner
// cutting, costs 1 / 1.414, octile heuristic with 0.414, binary heap push/pop/decreaseKey tie rules), 3.5 (golden
// routes, re-baselined for W5), 7.2 (findWalkableNear for hold-to-move).
//
// Owner area: world. Pure; reads the grid by reference (later collision mutations are seen). Not thread-safe: FindPath
// reuses per-instance scratch buffers (the core is single-threaded, ue58-platform.md 3.3).
#pragma once

#include <cstdint>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/world/Grid.h"

namespace abyss {

class ABYSS_API Pathfinder {
 public:
  explicit Pathfinder(const ZoneGrid& grid) : grid_(&grid) {}

  // findPath (5): start excluded, end included; false (out empty) when unwalkable / same tile / unreachable.
  bool FindPath(double startCol, double startRow, double endCol, double endRow, std::vector<TilePos>& out) const;

  // findWalkableNear (7.2 / monsters 9.4): Chebyshev rings 1..radius, dr outer, dc inner; centre never returned.
  bool FindWalkableNear(TilePos centre, int32_t radius, TilePos& out) const;
  // RandomEventSystem.findWalkableTile (world 13.3 / monsters 6.4): the preferred tile clamped to [1, size - 2] when
  // walkable, else ring edges 1..radius (dr outer, dc inner) inside [1, size - 2].
  bool FindWalkableTile(TilePos preferred, int32_t radius, TilePos& out) const;

  const ZoneGrid& Grid() const { return *grid_; }

  // A* search node (PathfindingSystem.ts PathNode): public only so the implementation's heap can name it.
  struct Node {
    int32_t col = 0, row = 0;
    double g = 0, h = 0, f = 0;
    int32_t parent = -1;  // index into the node pool
    int32_t heapIndex = 0;
  };

 private:
  const ZoneGrid* grid_;
  // Reused A* scratch: per-cell state is valid only where its stamp equals the call's generation, so a call costs
  // O(cells visited) instead of allocating and filling three grid-sized arrays. Search order and tie-breaking are
  // unchanged (same node pool indices, same heap).
  struct Scratch {
    std::vector<double> gCost;
    std::vector<int32_t> nodeMap;
    std::vector<uint32_t> seen;    // == gen: gCost / nodeMap set this call
    std::vector<uint32_t> closed;  // == gen: closed this call
    uint32_t gen = 0;
    std::vector<int32_t> heap;     // open set (node indices)
    std::vector<Node> nodes;       // node pool
  };
  mutable Scratch scratch_;
};

}  // namespace abyss
