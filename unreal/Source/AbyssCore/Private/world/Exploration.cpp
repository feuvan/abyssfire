// Exploration grid, fog core, hidden-area check points (world-map-nav.md 10; W2). ExplorationGrid is implemented;
// FogOfWarCore is a STUB. Owner area: world.
#include "abyss/base/Platform.h"

#include "abyss/world/Exploration.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/Assert.h"
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
  const int32_t r0 = std::max(0, static_cast<int32_t>(std::floor(row - radius)));
  const int32_t r1 = std::min(rows_ - 1, static_cast<int32_t>(std::ceil(row + radius)));
  const int32_t c0 = std::max(0, static_cast<int32_t>(std::floor(col - radius)));
  const int32_t c1 = std::min(cols_ - 1, static_cast<int32_t>(std::ceil(col + radius)));
  const double r2 = radius * radius;
  for (int32_t r = r0; r <= r1; ++r) {
    for (int32_t c = c0; c <= c1; ++c) {
      const double dc = c - col, dr = r - row;
      if (dc * dc + dr * dr <= r2) bits_[static_cast<size_t>(r) * static_cast<size_t>(cols_) + static_cast<size_t>(c)] = 1;
    }
  }
}

bool ExplorationGrid::IsExplored(int32_t col, int32_t row) const {
  if (col < 0 || row < 0 || col >= cols_ || row >= rows_) return false;
  return bits_[static_cast<size_t>(row) * static_cast<size_t>(cols_) + static_cast<size_t>(col)] != 0;
}

void FogOfWarCore::Reset(int32_t cols, int32_t rows, double viewRadius, int32_t edgeBand) {
  cols_ = cols;
  rows_ = rows;
  viewRadius_ = viewRadius;
  edgeBand_ = edgeBand;
  explored_.assign(static_cast<size_t>(std::max(0, cols)) * static_cast<size_t>(std::max(0, rows)), 0);
  prevAlpha_.assign(explored_.size(), 0);
  dirty_.clear();
  hasLast_ = false;
}

bool FogOfWarCore::Update(double col, double row) {
  ABYSS_UNIMPLEMENTED();
  (void)lastCol_;
  (void)lastRow_;
  (void)viewRadius_;
  (void)edgeBand_;
  return false;
}

double FogOfWarCore::Alpha(int32_t col, int32_t row) const {
  if (col < 0 || row < 0 || col >= cols_ || row >= rows_) return 0.85;
  return prevAlpha_[static_cast<size_t>(row) * static_cast<size_t>(cols_) + static_cast<size_t>(col)] / 255.0;
}

bool FogOfWarCore::IsExplored(int32_t col, int32_t row) const {
  if (col < 0 || row < 0 || col >= cols_ || row >= rows_) return false;
  return explored_[static_cast<size_t>(row) * static_cast<size_t>(cols_) + static_cast<size_t>(col)] != 0;
}

std::vector<std::vector<bool>> FogOfWarCore::ExploredData() const {
  ABYSS_UNIMPLEMENTED();
  return {};
}

bool FogOfWarCore::LoadExploredData(const std::vector<std::vector<bool>>& data) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

void FogOfWarCore::Invalidate() {
  hasLast_ = false;
  std::fill(prevAlpha_.begin(), prevAlpha_.end(), 0);
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

bool ExplorationSystem::AreaFullyExplored(int32_t c0, int32_t r0, int32_t c1, int32_t r1) const {
  return grid_.IsExplored(c0, r0) && grid_.IsExplored(c1, r0) && grid_.IsExplored(c0, r1) &&
         grid_.IsExplored(c1, r1) && grid_.IsExplored((c0 + c1) / 2, (r0 + r1) / 2);
}

void ExplorationSystem::FillSnapshot(Snapshot& out) const { out.exploration = &grid_; }

}  // namespace abyss
