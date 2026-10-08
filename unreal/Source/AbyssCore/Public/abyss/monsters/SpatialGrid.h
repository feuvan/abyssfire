// Uniform spatial grid (monsters-ai.md section 12, SpatialGrid<T>): cell size 16 tiles, entities keyed by id, results in
// cell row-major order then per-cell insertion order (that order feeds RNG-draw order in tests).
//
// Owner area: monsters (used by combat, pets and quests through MonsterSystem queries). Header-only, implemented.
#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <vector>

#include "abyss/base/Math.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"

namespace abyss {

class SpatialGrid {
 public:
  static constexpr int32_t kDefaultCellSize = 16;

  SpatialGrid() = default;
  SpatialGrid(int32_t cols, int32_t rows, int32_t cellSize = kDefaultCellSize) { Reset(cols, rows, cellSize); }

  void Reset(int32_t cols, int32_t rows, int32_t cellSize = kDefaultCellSize) {
    cellSize_ = cellSize > 0 ? cellSize : kDefaultCellSize;
    gridCols_ = (std::max)(1, (cols + cellSize_ - 1) / cellSize_);
    gridRows_ = (std::max)(1, (rows + cellSize_ - 1) / cellSize_);
    cells_.assign(static_cast<size_t>(gridCols_) * static_cast<size_t>(gridRows_), {});
    entries_.clear();
  }

  // cellIndex (12): clamp(floor(row / 16)) * gridCols + clamp(floor(col / 16)).
  int32_t CellIndex(double col, double row) const {
    const int32_t cx = Clamp(FloorInt(col / cellSize_), 0, gridCols_ - 1);
    const int32_t cy = Clamp(FloorInt(row / cellSize_), 0, gridRows_ - 1);
    return cy * gridCols_ + cx;
  }

  void Insert(EntityId id, Vec2 pos) {
    if (FindEntry(id) != nullptr) {
      Update(id, pos);
      return;
    }
    const int32_t cell = CellIndex(pos.x, pos.y);
    entries_.push_back({id, pos, cell});
    cells_[static_cast<size_t>(cell)].push_back(id);
  }

  void Remove(EntityId id) {
    for (size_t i = 0; i < entries_.size(); ++i) {
      if (entries_[i].id != id) continue;
      auto& cell = cells_[static_cast<size_t>(entries_[i].cell)];
      cell.erase(std::find(cell.begin(), cell.end(), id));
      entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(i));
      return;
    }
  }

  // Moves the entity between cells only when its cell changed (insertion order kept otherwise); inserts if unknown.
  void Update(EntityId id, Vec2 pos) {
    Entry* e = FindEntry(id);
    if (e == nullptr) {
      Insert(id, pos);
      return;
    }
    e->pos = pos;
    const int32_t cell = CellIndex(pos.x, pos.y);
    if (cell == e->cell) return;
    auto& old = cells_[static_cast<size_t>(e->cell)];
    old.erase(std::find(old.begin(), old.end(), id));
    cells_[static_cast<size_t>(cell)].push_back(id);
    e->cell = cell;
  }

  void Clear() {
    for (auto& c : cells_) c.clear();
    entries_.clear();
  }

  // queryRadius (12): ids with dx^2 + dy^2 <= r^2 (inclusive) over the clamped cell rectangle.
  void QueryRadius(double col, double row, double r, std::vector<EntityId>& out) const {
    out.clear();
    ForEachInRect(col, row, r, [&](const Entry& e) {
      const double dx = e.pos.x - col, dy = e.pos.y - row;
      if (dx * dx + dy * dy <= r * r) out.push_back(e.id);
    });
  }

  // findNearest (12): dSq <= maxR^2 and strictly smaller than the best (first found wins ties).
  EntityId FindNearest(double col, double row, double maxR, const std::function<bool(EntityId)>& filter = {}) const {
    EntityId best = kNoEntity;
    double bestSq = maxR * maxR;
    bool found = false;
    ForEachInRect(col, row, maxR, [&](const Entry& e) {
      if (filter && !filter(e.id)) return;
      const double dx = e.pos.x - col, dy = e.pos.y - row;
      const double dSq = dx * dx + dy * dy;
      if (dSq > maxR * maxR) return;
      if (!found || dSq < bestSq) {
        best = e.id;
        bestSq = dSq;
        found = true;
      }
    });
    return best;
  }

  size_t Size() const { return entries_.size(); }
  bool Contains(EntityId id) const { return FindEntry(id) != nullptr; }

 private:
  struct Entry {
    EntityId id = kNoEntity;
    Vec2 pos;
    int32_t cell = 0;
  };

  Entry* FindEntry(EntityId id) {
    for (Entry& e : entries_)
      if (e.id == id) return &e;
    return nullptr;
  }
  const Entry* FindEntry(EntityId id) const {
    for (const Entry& e : entries_)
      if (e.id == id) return &e;
    return nullptr;
  }

  template <class Fn>
  void ForEachInRect(double col, double row, double r, Fn&& fn) const {
    const int32_t x0 = Clamp(FloorInt((col - r) / cellSize_), 0, gridCols_ - 1);
    const int32_t x1 = Clamp(FloorInt((col + r) / cellSize_), 0, gridCols_ - 1);
    const int32_t y0 = Clamp(FloorInt((row - r) / cellSize_), 0, gridRows_ - 1);
    const int32_t y1 = Clamp(FloorInt((row + r) / cellSize_), 0, gridRows_ - 1);
    for (int32_t cy = y0; cy <= y1; ++cy) {
      for (int32_t cx = x0; cx <= x1; ++cx) {
        for (EntityId id : cells_[static_cast<size_t>(cy * gridCols_ + cx)]) {
          const Entry* e = FindEntry(id);
          if (e != nullptr) fn(*e);
        }
      }
    }
  }

  int32_t cellSize_ = kDefaultCellSize;
  int32_t gridCols_ = 1;
  int32_t gridRows_ = 1;
  // One cell from the start, matching gridCols_ * gridRows_, so a default-constructed grid (MonsterSystem before the
  // first zone Reset) can be queried and inserted into without indexing past the end.
  std::vector<std::vector<EntityId>> cells_ = std::vector<std::vector<EntityId>>(1);
  std::vector<Entry> entries_;
};

}  // namespace abyss
