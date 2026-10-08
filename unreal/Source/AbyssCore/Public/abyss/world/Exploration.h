// Exploration (minimap / world-map fog) and hidden-area discovery inputs.
// Spec: world-map-nav.md 10.1 (per-visit exploration grid, radius 10, not saved), 10.2 (FogOfWarCore alpha rules,
// optional), 10.3 (hidden-area discovery: all five check points explored during one visit), 11 (minimap data);
// DECISIONS W2 (no fog in the 3D view; the minimap and world map show explored fog), U8.
//
// Owner area: world. `ExplorationGrid` and `FogOfWarCore` are pure; `ExplorationSystem` is the runtime wrapper.
#pragma once

#include <cstdint>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"

namespace abyss {

struct SimContext;
struct Snapshot;

class ABYSS_API ExplorationGrid {
 public:
  void Reset(int32_t cols, int32_t rows);
  // updateExploredTiles (10.1): tiles with (c - hc)^2 + (r - hr)^2 <= radius^2.
  void Reveal(double col, double row, double radius = 10);
  bool IsExplored(int32_t col, int32_t row) const;
  int32_t Cols() const { return cols_; }
  int32_t Rows() const { return rows_; }
  const std::vector<uint8_t>& Bits() const { return bits_; }

 private:
  int32_t cols_ = 0, rows_ = 0;
  std::vector<uint8_t> bits_;
};

// FogOfWarCore (10.2), kept for a later milestone; quantised alpha per tile with a dirty list.
class ABYSS_API FogOfWarCore {
 public:
  void Reset(int32_t cols, int32_t rows, double viewRadius = 10, int32_t edgeBand = 3);
  bool Update(double col, double row);  // false when the position equals the last call exactly
  double Alpha(int32_t col, int32_t row) const;
  bool IsExplored(int32_t col, int32_t row) const;
  const std::vector<int32_t>& Dirty() const { return dirty_; }
  void ClearDirty() { dirty_.clear(); }
  std::vector<std::vector<bool>> ExploredData() const;
  bool LoadExploredData(const std::vector<std::vector<bool>>& data);
  void Invalidate();

 private:
  int32_t cols_ = 0, rows_ = 0;
  double viewRadius_ = 10;
  int32_t edgeBand_ = 3;
  std::vector<uint8_t> explored_;
  std::vector<uint8_t> prevAlpha_;
  std::vector<int32_t> dirty_;
  bool hasLast_ = false;
  double lastCol_ = 0, lastRow_ = 0;
};

class ABYSS_API ExplorationSystem {
 public:
  explicit ExplorationSystem(SimContext& ctx);

  void OnZoneEnter();  // reset to the zone's size (per visit)
  void Tick();         // reveal around the hero
  const ExplorationGrid& Grid() const { return grid_; }
  // Hidden-area discovery check points (10.3).
  bool AreaFullyExplored(int32_t c0, int32_t r0, int32_t c1, int32_t r1) const;
  void FillSnapshot(Snapshot& out) const;

 private:
  SimContext& ctx_;
  ExplorationGrid grid_;
};

}  // namespace abyss
