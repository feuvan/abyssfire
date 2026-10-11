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
  walk_[i] = static_cast<uint8_t>(IsWalkableTile(t) ? 1 : 0);
  components_.clear();
}

bool ZoneGrid::Walkable(int32_t col, int32_t row) const {
  if (!InBounds(col, row)) return false;
  return walk_[static_cast<size_t>(row) * static_cast<size_t>(cols_) + static_cast<size_t>(col)] != 0;
}

void ZoneGrid::SetWalkable(int32_t col, int32_t row, bool walkable) {
  if (!InBounds(col, row)) return;
  walk_[static_cast<size_t>(row) * static_cast<size_t>(cols_) + static_cast<size_t>(col)] =
      static_cast<uint8_t>(walkable ? 1 : 0);
  components_.clear();
}

bool ZoneGrid::WalkableAt(Vec2 p) const { return Walkable(JsRoundInt(p.x), JsRoundInt(p.y)); }

void ZoneGrid::RebuildWalkability() {
  for (size_t i = 0; i < tiles_.size(); ++i) walk_[i] = static_cast<uint8_t>(IsWalkableTile(tiles_[i]) ? 1 : 0);
  components_.clear();
}

int32_t ZoneGrid::Component(int32_t col, int32_t row) const {
  if (!Walkable(col, row)) return -1;
  if (components_.empty()) {
    static constexpr int32_t kDirs[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
    components_.assign(walk_.size(), -1);
    std::vector<int32_t> stack;
    int32_t next = 0;
    for (int32_t r = 0; r < rows_; ++r) {
      for (int32_t c = 0; c < cols_; ++c) {
        const size_t i0 = static_cast<size_t>(r) * static_cast<size_t>(cols_) + static_cast<size_t>(c);
        if (walk_[i0] == 0 || components_[i0] >= 0) continue;
        components_[i0] = next;
        stack.assign(1, static_cast<int32_t>(i0));
        while (!stack.empty()) {
          const int32_t cur = stack.back();
          stack.pop_back();
          const int32_t cc = cur % cols_, cr = cur / cols_;
          for (const auto& d : kDirs) {
            const int32_t nc = cc + d[0], nr = cr + d[1];
            if (!Walkable(nc, nr)) continue;
            if (d[0] != 0 && d[1] != 0 && (!Walkable(cc + d[0], cr) || !Walkable(cc, cr + d[1]))) continue;
            const size_t ni = static_cast<size_t>(nr) * static_cast<size_t>(cols_) + static_cast<size_t>(nc);
            if (components_[ni] >= 0) continue;
            components_[ni] = next;
            stack.push_back(static_cast<int32_t>(ni));
          }
        }
        ++next;
      }
    }
  }
  return components_[static_cast<size_t>(row) * static_cast<size_t>(cols_) + static_cast<size_t>(col)];
}

}  // namespace abyss
