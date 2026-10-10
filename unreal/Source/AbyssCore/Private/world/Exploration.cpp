// Exploration grid, fog core, hidden-area check points (world-map-nav.md 10; W2). Owner area: world.
#include "abyss/base/Platform.h"

#include "abyss/world/Exploration.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/Math.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"
#include "abyss/world/Zone.h"

namespace abyss {

void ExplorationGrid::Reset(int32_t cols, int32_t rows) {
  cols_ = std::max(0, cols);
  rows_ = std::max(0, rows);
  bits_.assign(static_cast<size_t>(cols_) * static_cast<size_t>(rows_), 0);
}

void ExplorationGrid::Reveal(double col, double row, double radius) {
  if (cols_ <= 0 || rows_ <= 0) return;
  // FloorInt / CeilInt saturate (a non-finite position reveals nothing instead of overflowing).
  const int32_t r0 = std::max(0, FloorInt(row - radius));
  const int32_t r1 = std::min(rows_ - 1, CeilInt(row + radius));
  const int32_t c0 = std::max(0, FloorInt(col - radius));
  const int32_t c1 = std::min(cols_ - 1, CeilInt(col + radius));
  const double r2 = radius * radius;
  for (int32_t r = r0; r <= r1; ++r) {
    for (int32_t c = c0; c <= c1; ++c) {
      const double dc = c - col, dr = r - row;
      if (dc * dc + dr * dr <= r2) {
        bits_[static_cast<size_t>(r) * static_cast<size_t>(cols_) + static_cast<size_t>(c)] = 1;
      }
    }
  }
}

bool ExplorationGrid::IsExplored(int32_t col, int32_t row) const {
  if (col < 0 || row < 0 || col >= cols_ || row >= rows_) return false;
  return bits_[static_cast<size_t>(row) * static_cast<size_t>(cols_) + static_cast<size_t>(col)] != 0;
}

void FogOfWarCore::Reset(int32_t cols, int32_t rows, double viewRadius, int32_t edgeBand) {
  cols_ = std::max(0, cols);
  rows_ = std::max(0, rows);
  viewRadius_ = viewRadius;
  edgeBand_ = edgeBand;
  explored_.assign(static_cast<size_t>(cols_) * static_cast<size_t>(rows_), 0);
  prevAlpha_.assign(explored_.size(), 0);
  dirty_.clear();
  lastCol_ = -1;
  lastRow_ = -1;
}

bool FogOfWarCore::Update(double col, double row) {
  constexpr uint8_t kUnexplored = 0, kExplored = 1, kVisible = 2;
  if (col == lastCol_ && row == lastRow_) return false;
  lastCol_ = col;
  lastRow_ = row;
  const double vr = viewRadius_;
  const double vrSq = vr * vr;
  const double band = static_cast<double>(edgeBand_);
  const int32_t minC = std::max(0, FloorInt(col - vr));
  const int32_t maxC = std::min(cols_ - 1, CeilInt(col + vr));
  const int32_t minR = std::max(0, FloorInt(row - vr));
  const int32_t maxR = std::min(rows_ - 1, CeilInt(row + vr));
  auto at = [this](int32_t c, int32_t r) {
    return static_cast<size_t>(r) * static_cast<size_t>(cols_) + static_cast<size_t>(c);
  };
  for (int32_t r = minR; r <= maxR; ++r) {
    const double dr = r - row;
    for (int32_t c = minC; c <= maxC; ++c) {
      const double dc = c - col;
      if (dc * dc + dr * dr <= vrSq) explored_[at(c, r)] = kVisible;
    }
  }
  dirty_.clear();
  const double innerEdge = vr - band;
  for (int32_t r = 0; r < rows_; ++r) {
    const double dr = r - row;
    for (int32_t c = 0; c < cols_; ++c) {
      const size_t idx = at(c, r);
      const double dc = c - col;
      const double dist = std::sqrt(dc * dc + dr * dr);
      double alpha;
      if (dist <= innerEdge) {
        alpha = 0;
        if (explored_[idx] == kUnexplored) explored_[idx] = kExplored;
      } else if (dist <= vr) {
        const double t = (dist - innerEdge) / band;
        alpha = t * 0.15;
        if (alpha < 0.01) alpha = 0;
        if (explored_[idx] == kUnexplored) explored_[idx] = kExplored;
      } else if (explored_[idx] >= kExplored) {
        alpha = dist < vr + band ? 0.18 + (dist - vr) / band * 0.15 : 0.35;
      } else {
        alpha = 0.85;
      }
      const uint8_t q = static_cast<uint8_t>(JsRoundInt(alpha * 255));
      if (q != prevAlpha_[idx]) {
        dirty_.push_back(static_cast<int32_t>(idx));
        prevAlpha_[idx] = q;
      }
    }
  }
  for (int32_t r = minR; r <= maxR; ++r) {
    for (int32_t c = minC; c <= maxC; ++c) {
      if (explored_[at(c, r)] == kVisible) explored_[at(c, r)] = kExplored;
    }
  }
  return true;
}

double FogOfWarCore::Alpha(int32_t col, int32_t row) const {
  if (col < 0 || row < 0 || col >= cols_ || row >= rows_) return 0.85;
  return prevAlpha_[static_cast<size_t>(row) * static_cast<size_t>(cols_) + static_cast<size_t>(col)] / 255.0;
}

bool FogOfWarCore::IsExplored(int32_t col, int32_t row) const {
  if (col < 0 || row < 0 || col >= cols_ || row >= rows_) return false;
  return explored_[static_cast<size_t>(row) * static_cast<size_t>(cols_) + static_cast<size_t>(col)] != 0;
}

std::vector<FogGradientTile> FogOfWarCore::GradientInfo(double col, double row) const {
  std::vector<FogGradientTile> out;
  const double vr = viewRadius_;
  const double band = static_cast<double>(edgeBand_);
  const double innerEdge = vr - band;
  const int32_t minC = std::max(0, FloorInt(col - vr));
  const int32_t maxC = std::min(cols_ - 1, CeilInt(col + vr));
  const int32_t minR = std::max(0, FloorInt(row - vr));
  const int32_t maxR = std::min(rows_ - 1, CeilInt(row + vr));
  for (int32_t r = minR; r <= maxR; ++r) {
    for (int32_t c = minC; c <= maxC; ++c) {
      const double dc = c - col, dr = r - row;
      const double dist = std::sqrt(dc * dc + dr * dr);
      if (dist > innerEdge && dist <= vr) {
        const double alpha = (dist - innerEdge) / band * 0.15;
        if (alpha >= 0.01) out.push_back(FogGradientTile{c, r, alpha});
      }
    }
  }
  return out;
}

std::vector<std::vector<bool>> FogOfWarCore::ExploredData() const {
  std::vector<std::vector<bool>> out(static_cast<size_t>(rows_), std::vector<bool>(static_cast<size_t>(cols_), false));
  for (int32_t r = 0; r < rows_; ++r) {
    for (int32_t c = 0; c < cols_; ++c) out[static_cast<size_t>(r)][static_cast<size_t>(c)] = IsExplored(c, r);
  }
  return out;
}

bool FogOfWarCore::LoadExploredData(const std::vector<std::vector<bool>>& data) {
  if (data.size() != static_cast<size_t>(rows_) || data.empty() || data[0].size() != static_cast<size_t>(cols_)) {
    return false;
  }
  for (int32_t r = 0; r < rows_; ++r) {
    const std::vector<bool>& row = data[static_cast<size_t>(r)];
    for (int32_t c = 0; c < cols_; ++c) {
      const bool v = static_cast<size_t>(c) < row.size() && row[static_cast<size_t>(c)];
      explored_[static_cast<size_t>(r) * static_cast<size_t>(cols_) + static_cast<size_t>(c)] = v ? 1 : 0;
    }
  }
  std::fill(prevAlpha_.begin(), prevAlpha_.end(), 0);
  lastCol_ = -1;
  lastRow_ = -1;
  return true;
}

void FogOfWarCore::Invalidate() {
  std::fill(prevAlpha_.begin(), prevAlpha_.end(), 0);
  lastCol_ = -1;
  lastRow_ = -1;
  dirty_.clear();
}

ExplorationSystem::ExplorationSystem(SimContext& ctx) : ctx_(ctx) {}

void ExplorationSystem::OnZoneEnter() {
  const ZoneRuntime* z = ctx_.sys.zone;
  if (z != nullptr && z->HasZone()) {
    grid_.Reset(z->Grid().Cols(), z->Grid().Rows());
  } else {
    grid_.Reset(0, 0);
  }
}

void ExplorationSystem::Tick() {
  const Vec2 p = ctx_.sys.hero->Position();
  grid_.Reveal(p.x, p.y, ctx_.data.World().constants.hiddenAreaExploreRadius);
}

bool ExplorationSystem::AreaFullyExplored(const HiddenAreaDef& area) const {
  // getHiddenAreaBounds: startCol ?? col - radius, ... (radius may be fractional).
  double c0 = area.center.col - area.radius, r0 = area.center.row - area.radius;
  double c1 = area.center.col + area.radius, r1 = area.center.row + area.radius;
  if (area.hasBounds) {
    c0 = area.boundsStart.col;
    r0 = area.boundsStart.row;
    c1 = area.boundsEnd.col;
    r1 = area.boundsEnd.row;
  }
  // The web indexes exploredTiles[r * cols + c]: a non-integral point reads undefined (unexplored).
  const auto explored = [this](double c, double r) {
    if (c != std::floor(c) || r != std::floor(r)) return false;
    return grid_.IsExplored(static_cast<int32_t>(c), static_cast<int32_t>(r));
  };
  return explored(c0, r0) && explored(c1, r0) && explored(c0, r1) && explored(c1, r1) &&
         explored(area.center.col, area.center.row);
}

void ExplorationSystem::FillSnapshot(Snapshot& out) const { out.exploration = &grid_; }

}  // namespace abyss
