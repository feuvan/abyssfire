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
    auto& items = cells_[static_cast<size_t>(cell)];
    entries_.insert(LowerBound(id), Entry{id, cell, static_cast<int32_t>(items.size())});
    items.push_back(Item{id, pos});
  }

  void Remove(EntityId id) {
    const auto it = LowerBound(id);
    if (it == entries_.end() || it->id != id) return;
    const int32_t cell = it->cell, slot = it->slot;
    entries_.erase(it);
    EraseFromCell(cell, slot);
  }

  // Moves the entity between cells only when its cell changed (insertion order kept otherwise); inserts if unknown.
  void Update(EntityId id, Vec2 pos) {
    Entry* e = FindEntry(id);
    if (e == nullptr) {
      Insert(id, pos);
      return;
    }
    const int32_t cell = CellIndex(pos.x, pos.y);
    if (cell == e->cell) {
      cells_[static_cast<size_t>(cell)][static_cast<size_t>(e->slot)].pos = pos;
      return;
    }
    const int32_t oldCell = e->cell, oldSlot = e->slot;
    auto& items = cells_[static_cast<size_t>(cell)];
    e->cell = cell;
    e->slot = static_cast<int32_t>(items.size());
    items.push_back(Item{id, pos});
    EraseFromCell(oldCell, oldSlot);  // re-finds the shifted entries (e is not used after this)
  }

  void Clear() {
    for (auto& c : cells_) c.clear();
    entries_.clear();
  }

  // queryRadius (12): ids with dx^2 + dy^2 <= r^2 (inclusive) over the clamped cell rectangle.
  void QueryRadius(double col, double row, double r, std::vector<EntityId>& out) const {
    out.clear();
    ForEachInRect(col, row, r, [&](const Item& e) {
      const double dx = e.pos.x - col, dy = e.pos.y - row;
      if (dx * dx + dy * dy <= r * r) out.push_back(e.id);
    });
  }

  // findNearest (12): dSq <= maxR^2 and strictly smaller than the best (first found wins ties).
  template <class Filter>
  EntityId FindNearestIf(double col, double row, double maxR, const Filter& filter) const {
    EntityId best = kNoEntity;
    double bestSq = maxR * maxR;
    bool found = false;
    ForEachInRect(col, row, maxR, [&](const Item& e) {
      const double dx = e.pos.x - col, dy = e.pos.y - row;
      const double dSq = dx * dx + dy * dy;
      if (dSq > maxR * maxR) return;
      if (found && !(dSq < bestSq)) return;  // cheap distance test first; the filter has no side effects
      if (!filter(e.id)) return;
      best = e.id;
      bestSq = dSq;
      found = true;
    });
    return best;
  }
  EntityId FindNearest(double col, double row, double maxR, const std::function<bool(EntityId)>& filter = {}) const {
    return FindNearestIf(col, row, maxR, [&filter](EntityId id) { return !filter || filter(id); });
  }

  size_t Size() const { return entries_.size(); }
  bool Contains(EntityId id) const { return FindEntry(id) != nullptr; }

 private:
  struct Item {
    EntityId id = kNoEntity;
    Vec2 pos;
  };
  struct Entry {
    EntityId id = kNoEntity;
    int32_t cell = 0;
    int32_t slot = 0;  // index in cells_[cell]
  };

  void EraseFromCell(int32_t cell, int32_t slot) {
    auto& items = cells_[static_cast<size_t>(cell)];
    items.erase(items.begin() + slot);
    for (size_t i = static_cast<size_t>(slot); i < items.size(); ++i) {
      if (Entry* moved = FindEntry(items[i].id)) moved->slot = static_cast<int32_t>(i);
    }
  }

  std::vector<Entry>::iterator LowerBound(EntityId id) {
    return std::lower_bound(entries_.begin(), entries_.end(), id, [](const Entry& e, EntityId k) { return e.id < k; });
  }
  std::vector<Entry>::const_iterator LowerBound(EntityId id) const {
    return std::lower_bound(entries_.begin(), entries_.end(), id, [](const Entry& e, EntityId k) { return e.id < k; });
  }
  Entry* FindEntry(EntityId id) {
    const auto it = LowerBound(id);
    return it != entries_.end() && it->id == id ? &*it : nullptr;
  }
  const Entry* FindEntry(EntityId id) const {
    const auto it = LowerBound(id);
    return it != entries_.end() && it->id == id ? &*it : nullptr;
  }

  template <class Fn>
  void ForEachInRect(double col, double row, double r, Fn&& fn) const {
    const int32_t x0 = Clamp(FloorInt((col - r) / cellSize_), 0, gridCols_ - 1);
    const int32_t x1 = Clamp(FloorInt((col + r) / cellSize_), 0, gridCols_ - 1);
    const int32_t y0 = Clamp(FloorInt((row - r) / cellSize_), 0, gridRows_ - 1);
    const int32_t y1 = Clamp(FloorInt((row + r) / cellSize_), 0, gridRows_ - 1);
    for (int32_t cy = y0; cy <= y1; ++cy) {
      for (int32_t cx = x0; cx <= x1; ++cx) {
        for (const Item& e : cells_[static_cast<size_t>(cy * gridCols_ + cx)]) fn(e);
      }
    }
  }

  int32_t cellSize_ = kDefaultCellSize;
  int32_t gridCols_ = 1;
  int32_t gridRows_ = 1;
  // One cell from the start, matching gridCols_ * gridRows_, so a default-constructed grid (MonsterSystem before the
  // first zone Reset) can be queried and inserted into without indexing past the end.
  // Cells hold id + position (query order = cell row-major, then per-cell insertion order); entries_ is the id index
  // (sorted by id) for Update / Remove.
  std::vector<std::vector<Item>> cells_ = std::vector<std::vector<Item>>(1);
  std::vector<Entry> entries_;
};

}  // namespace abyss
