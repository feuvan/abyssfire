// A* (world-map-nav.md section 5) and walkable-tile searches: a port of src/systems/PathfindingSystem.ts with the web's
// binary-heap tie-breaking (push bubbles up on strict <, pop sinks to the strictly smaller child with left winning ties,
// decreaseKey bubbles up), costs 1 / 1.414 and the octile heuristic with 0.414. Owner area: world.
#include "abyss/base/Platform.h"

#include "abyss/world/Pathfinding.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "abyss/base/Math.h"

namespace abyss {

namespace pathfinding_impl {

struct AStarNode {
  int32_t col = 0, row = 0;
  double g = 0, h = 0, f = 0;
  int32_t parent = -1;     // index into the node pool
  int32_t heapIndex = 0;
};

// MinHeap of node indices keyed on f (PathfindingSystem.ts MinHeap).
class AStarHeap {
 public:
  explicit AStarHeap(std::vector<AStarNode>& nodes) : nodes_(nodes) {}
  size_t Size() const { return data_.size(); }

  void Push(int32_t n) {
    nodes_[static_cast<size_t>(n)].heapIndex = static_cast<int32_t>(data_.size());
    data_.push_back(n);
    BubbleUp(static_cast<int32_t>(data_.size()) - 1);
  }

  int32_t Pop() {
    const int32_t top = data_[0];
    const int32_t last = data_.back();
    data_.pop_back();
    if (!data_.empty()) {
      nodes_[static_cast<size_t>(last)].heapIndex = 0;
      data_[0] = last;
      SinkDown(0);
    }
    return top;
  }

  void DecreaseKey(int32_t n) { BubbleUp(nodes_[static_cast<size_t>(n)].heapIndex); }

 private:
  double F(int32_t heapIdx) const { return nodes_[static_cast<size_t>(data_[static_cast<size_t>(heapIdx)])].f; }

  void BubbleUp(int32_t idx) {
    const int32_t node = data_[static_cast<size_t>(idx)];
    const double f = nodes_[static_cast<size_t>(node)].f;
    while (idx > 0) {
      const int32_t parentIdx = (idx - 1) >> 1;
      const int32_t parent = data_[static_cast<size_t>(parentIdx)];
      if (f >= nodes_[static_cast<size_t>(parent)].f) break;
      data_[static_cast<size_t>(idx)] = parent;
      nodes_[static_cast<size_t>(parent)].heapIndex = idx;
      idx = parentIdx;
    }
    data_[static_cast<size_t>(idx)] = node;
    nodes_[static_cast<size_t>(node)].heapIndex = idx;
  }

  void SinkDown(int32_t idx) {
    const int32_t length = static_cast<int32_t>(data_.size());
    const int32_t node = data_[static_cast<size_t>(idx)];
    while (true) {
      const int32_t left = 2 * idx + 1;
      const int32_t right = 2 * idx + 2;
      int32_t smallest = idx;
      if (left < length && F(left) < F(smallest)) smallest = left;
      if (right < length && F(right) < F(smallest)) smallest = right;
      if (smallest == idx) break;
      const int32_t swap = data_[static_cast<size_t>(smallest)];
      data_[static_cast<size_t>(smallest)] = node;
      nodes_[static_cast<size_t>(node)].heapIndex = smallest;
      data_[static_cast<size_t>(idx)] = swap;
      nodes_[static_cast<size_t>(swap)].heapIndex = idx;
      idx = smallest;
    }
  }

  std::vector<AStarNode>& nodes_;
  std::vector<int32_t> data_;
};

double Octile(int32_t c1, int32_t r1, int32_t c2, int32_t r2) {
  const double dx = std::abs(c1 - c2);
  const double dy = std::abs(r1 - r2);
  return (std::max)(dx, dy) + 0.414 * (std::min)(dx, dy);
}

}  // namespace pathfinding_impl

bool Pathfinder::FindPath(double startCol, double startRow, double endCol, double endRow,
                          std::vector<TilePos>& out) const {
  using namespace pathfinding_impl;
  out.clear();
  const ZoneGrid& grid = *grid_;
  const int32_t sc = JsRoundInt(startCol);
  const int32_t sr = JsRoundInt(startRow);
  const int32_t ec = JsRoundInt(endCol);
  const int32_t er = JsRoundInt(endRow);
  if (!grid.Walkable(ec, er)) return false;
  if (!grid.Walkable(sc, sr)) return false;
  if (sc == ec && sr == er) return false;

  const int32_t cols = grid.Cols();
  const size_t cells = static_cast<size_t>(cols) * static_cast<size_t>(grid.Rows());
  std::vector<double> gCost(cells, std::numeric_limits<double>::infinity());
  std::vector<uint8_t> closed(cells, 0);
  std::vector<int32_t> nodeMap(cells, -1);
  std::vector<AStarNode> nodes;
  nodes.reserve(256);
  AStarHeap open(nodes);
  auto flat = [cols](int32_t c, int32_t r) { return static_cast<size_t>(r) * static_cast<size_t>(cols) + static_cast<size_t>(c); };

  AStarNode start;
  start.col = sc;
  start.row = sr;
  start.g = 0;
  start.h = Octile(sc, sr, ec, er);
  start.f = start.g + start.h;
  nodes.push_back(start);
  gCost[flat(sc, sr)] = 0;
  nodeMap[flat(sc, sr)] = 0;
  open.Push(0);

  static constexpr int32_t kDirs[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
  while (open.Size() > 0) {
    const int32_t cur = open.Pop();
    const AStarNode current = nodes[static_cast<size_t>(cur)];
    if (current.col == ec && current.row == er) {
      for (int32_t n = cur; n >= 0; n = nodes[static_cast<size_t>(n)].parent) {
        out.emplace_back(nodes[static_cast<size_t>(n)].col, nodes[static_cast<size_t>(n)].row);
      }
      std::reverse(out.begin(), out.end());
      out.erase(out.begin());  // start excluded
      return !out.empty();
    }
    closed[flat(current.col, current.row)] = 1;
    for (const auto& d : kDirs) {
      const int32_t dc = d[0], dr = d[1];
      const int32_t nc = current.col + dc;
      const int32_t nr = current.row + dr;
      if (!grid.Walkable(nc, nr)) continue;
      const size_t nIdx = flat(nc, nr);
      if (closed[nIdx] != 0) continue;
      if (dc != 0 && dr != 0) {
        if (!grid.Walkable(current.col + dc, current.row) || !grid.Walkable(current.col, current.row + dr)) continue;
      }
      const double moveCost = dc != 0 && dr != 0 ? 1.414 : 1.0;
      const double g = current.g + moveCost;
      if (g < gCost[nIdx]) {
        gCost[nIdx] = g;
        const int32_t existing = nodeMap[nIdx];
        if (existing >= 0) {
          AStarNode& e = nodes[static_cast<size_t>(existing)];
          e.g = g;
          e.f = g + e.h;
          e.parent = cur;
          open.DecreaseKey(existing);
        } else {
          AStarNode n;
          n.col = nc;
          n.row = nr;
          n.g = g;
          n.h = Octile(nc, nr, ec, er);
          n.f = g + n.h;
          n.parent = cur;
          nodes.push_back(n);
          const int32_t id = static_cast<int32_t>(nodes.size()) - 1;
          nodeMap[nIdx] = id;
          open.Push(id);
        }
      }
    }
  }
  return false;
}

bool Pathfinder::FindWalkableNear(TilePos centre, int32_t radius, TilePos& out) const {
  for (int32_t r = 1; r <= radius; ++r) {
    for (int32_t dr = -r; dr <= r; ++dr) {
      for (int32_t dc = -r; dc <= r; ++dc) {
        if ((std::max)(std::abs(dc), std::abs(dr)) != r) continue;
        if (grid_->Walkable(centre.col + dc, centre.row + dr)) {
          out = TilePos(centre.col + dc, centre.row + dr);
          return true;
        }
      }
    }
  }
  return false;
}

bool Pathfinder::FindWalkableTile(TilePos preferred, int32_t radius, TilePos& out) const {
  const int32_t cols = grid_->Cols(), rows = grid_->Rows();
  const int32_t pc = (std::max)(1, (std::min)(cols - 2, preferred.col));
  const int32_t pr = (std::max)(1, (std::min)(rows - 2, preferred.row));
  if (grid_->Walkable(pc, pr)) {
    out = TilePos(pc, pr);
    return true;
  }
  for (int32_t rad = 1; rad <= radius; ++rad) {
    for (int32_t dr = -rad; dr <= rad; ++dr) {
      for (int32_t dc = -rad; dc <= rad; ++dc) {
        if (std::abs(dr) != rad && std::abs(dc) != rad) continue;
        const int32_t nr = pr + dr, nc = pc + dc;
        if (nr >= 1 && nr < rows - 1 && nc >= 1 && nc < cols - 1 && grid_->Walkable(nc, nr)) {
          out = TilePos(nc, nr);
          return true;
        }
      }
    }
  }
  return false;
}

}  // namespace abyss
