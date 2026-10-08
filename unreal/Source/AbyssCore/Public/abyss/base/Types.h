// Basic value types shared by every core module: entity ids, tile-space vectors, id indexes.
//
// Space (all specs 0): the core works in TILE units on the ground plane. Vec2.x = col (float), Vec2.y = row (float);
// integer values are tile centres. Distances are Euclidean in tiles unless a rule says otherwise. UE converts with
// Units.h (1 tile = 100 uu, DECISIONS S4).
#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "abyss/base/Platform.h"

namespace abyss {

// ---- Entities -----------------------------------------------------------------------------------------------------
// Stable runtime handle for anything that exists in the world (hero, monsters, NPCs, pets, ground items, projectiles,
// ground effects, props). Allocated monotonically per session by the GameSim (never reused within a session, so never
// within a zone visit; ARCHITECTURE 3.1, monsters-ai.md 1.2). 0 = none.
using EntityId = uint32_t;
inline constexpr EntityId kNoEntity = 0;
// The hero always has this id.
inline constexpr EntityId kHeroEntityId = 1;

enum class EntityKind : uint8_t {
  None,
  Hero,
  Monster,
  Npc,
  Pet,
  GroundItem,    // item lying on the ground (items/GroundLoot.h)
  PotionDrop,    // auto-pickup potion (loot spec 6.2)
  Projectile,    // in-flight hero/monster/pet projectile (combat/Projectiles.h)
  GroundEffect,  // persistent ground AoE (C4)
  Prop,          // interactable world prop (lore, hidden reward, event prop, gather node, clue mark, soul echo)
  Escort,        // escort quest NPC (quests spec 3.9)
  DefendTarget,  // defend quest target (quests spec 3.10)
};

// Hands out EntityIds monotonically for one session (kHeroEntityId is reserved). Owned by the GameSim and passed by
// reference to every subsystem that creates world entities.
class EntityIdAllocator {
 public:
  EntityId Next() { return next_++; }
  EntityId Peek() const { return next_; }
  void Reset(EntityId next = kHeroEntityId + 1) { next_ = next; }

 private:
  EntityId next_ = kHeroEntityId + 1;
};

// ---- Tile-space vectors -------------------------------------------------------------------------------------------
struct Vec2 {
  double x = 0.0;  // col
  double y = 0.0;  // row

  constexpr Vec2() = default;
  constexpr Vec2(double px, double py) : x(px), y(py) {}

  constexpr Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
  constexpr Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
  constexpr Vec2 operator*(double s) const { return {x * s, y * s}; }
  constexpr Vec2 operator/(double s) const { return {x / s, y / s}; }
  constexpr Vec2 operator-() const { return {-x, -y}; }
  Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
  Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
  Vec2& operator*=(double s) { x *= s; y *= s; return *this; }
  constexpr bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
  constexpr bool operator!=(const Vec2& o) const { return !(*this == o); }

  constexpr double Dot(const Vec2& o) const { return x * o.x + y * o.y; }
  // 2D cross product z component (x1*y2 - y1*x2).
  constexpr double Cross(const Vec2& o) const { return x * o.y - y * o.x; }
  constexpr double LengthSq() const { return x * x + y * y; }
  double Length() const { return std::sqrt(LengthSq()); }
  // Unit vector; returns (0,0) for a zero-length vector.
  Vec2 Normalized() const {
    const double len = Length();
    return len > 0.0 ? Vec2{x / len, y / len} : Vec2{};
  }
};

// Integer tile coordinate (col, row).
struct TilePos {
  int32_t col = 0;
  int32_t row = 0;

  constexpr TilePos() = default;
  constexpr TilePos(int32_t c, int32_t r) : col(c), row(r) {}
  constexpr bool operator==(const TilePos& o) const { return col == o.col && row == o.row; }
  constexpr bool operator!=(const TilePos& o) const { return !(*this == o); }
  constexpr Vec2 Center() const { return {static_cast<double>(col), static_cast<double>(row)}; }
};

// {col, row, radius}: quest areas, objective locations, hidden areas (data shape used by many tables).
struct TileCircle {
  int32_t col = 0;
  int32_t row = 0;
  double radius = 0.0;
  constexpr Vec2 Center() const { return {static_cast<double>(col), static_cast<double>(row)}; }
};

// JS Math.round on both components (floor(x + 0.5)), the web's rule for "tile under an entity".
ABYSS_API TilePos RoundToTile(Vec2 p);

// ---- Id index -----------------------------------------------------------------------------------------------------
// Maps string ids to dense indices. Built once from data (insertion order = data order), lookups by binary search on a
// sorted copy. Never iterated in hash order (ue58-platform.md 3.3 "Hashing / ordering").
class ABYSS_API IdIndex {
 public:
  IdIndex() = default;
  // Adds an id with the next index (Size()). Returns false (and does not add) for a duplicate id.
  bool Add(std::string_view id);
  // Index of the id, or -1.
  int32_t Find(std::string_view id) const;
  bool Contains(std::string_view id) const { return Find(id) >= 0; }
  size_t Size() const { return ids_.size(); }
  // Id with the given index (data order).
  const std::string& IdAt(size_t index) const { return ids_[index]; }
  void Clear();

 private:
  std::vector<std::string> ids_;                       // data order
  std::vector<std::pair<std::string, int32_t>> sorted_;  // sorted by id
};

}  // namespace abyss
