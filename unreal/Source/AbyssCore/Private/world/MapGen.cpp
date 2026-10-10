// Map generator (world-map-nav.md section 4; W5, W10): a line-by-line port of src/systems/MapGenerator.ts. Every RNG
// draw happens in the web's order and every float expression is evaluated in the web's order (FP contraction is off,
// Platform.h + -ffp-contract=off; Math.hypot = JsHypot), so the output is bit-identical to the TS generator
// (CoreTests/golden/maps). Owner area: world.
#include "abyss/base/Platform.h"

#include "abyss/world/MapGen.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

#include "abyss/base/Math.h"
#include "abyss/data/DataStore.h"

namespace abyss {

ParkMillerRng::ParkMillerRng(int64_t seed) : state_(seed % 2147483647) {
  if (state_ <= 0) state_ += 2147483646;
}

double ParkMillerRng::Next() {
  ++draws_;
  state_ = (state_ * 16807) % 2147483647;
  return static_cast<double>(state_ - 1) / 2147483646.0;
}

int32_t ParkMillerRng::NextInt(int32_t lo, int32_t hi) {
  return static_cast<int32_t>(std::floor(Next() * static_cast<double>(hi - lo + 1))) + lo;
}

double GroveNoise(double col, double row, double seed, double scale) {
  // h(x, y): n = ToInt32(x*374761393 + y*668265263 + seed*2246822519) (double sum, left to right);
  // n = imul(n ^ (n >>> 13), 1274126177); uint32(n ^ (n >>> 16)) / 2^32.
  auto h = [seed](double x, double y) {
    const double sum = x * 374761393.0 + y * 668265263.0 + seed * 2246822519.0;
    const uint32_t n0 = static_cast<uint32_t>(ToInt32(sum));
    const int32_t n1 = Imul(static_cast<int32_t>(n0 ^ (n0 >> 13)), 1274126177);
    const uint32_t u = static_cast<uint32_t>(n1);
    return static_cast<double>(u ^ (u >> 16)) / 4294967296.0;
  };
  const double x = col / scale;
  const double y = row / scale;
  const double x0 = std::floor(x);
  const double y0 = std::floor(y);
  const double fx = x - x0;
  const double fy = y - y0;
  const double sx = fx * fx * (3.0 - 2.0 * fx);
  const double sy = fy * fy * (3.0 - 2.0 * fy);
  const double a = h(x0, y0) + (h(x0 + 1.0, y0) - h(x0, y0)) * sx;
  const double b = h(x0, y0 + 1.0) + (h(x0 + 1.0, y0 + 1.0) - h(x0, y0 + 1.0)) * sx;
  return a + (b - a) * sy;
}

const std::string& PickWeighted(std::span<const WeightedDecor> pool, double u) {
  static const std::string kEmpty;
  if (pool.empty()) return kEmpty;
  double total = 0;
  for (const WeightedDecor& e : pool) total = total + e.weight;
  double x = u * total;
  for (const WeightedDecor& e : pool) {
    x -= e.weight;
    if (x <= 0) return e.type;
  }
  return pool.back().type;
}

void ApplyDecorationJitter(Decoration& d, size_t index) {
  // placeDecorSprite (15.5) with seed = index + 1; h / h2 in [0, 1).
  const double seed = static_cast<double>(index + 1);
  const double h = static_cast<double>(ToUint32(seed * 2654435761.0)) / 4294967296.0;
  const double h2 = static_cast<double>(ToUint32(seed * 1597334677.0 + 12345.0)) / 4294967296.0;
  const double jx = (h - 0.5) * 18.0;
  const double jy = (h2 - 0.5) * 8.0;
  d.offsetCol = (jx / 32.0 + jy / 16.0) / 2.0;
  d.offsetRow = (jy / 16.0 - jx / 32.0) / 2.0;
  d.scale = 0.9 + std::fmod(h + h2, 1.0) * 0.2;
  d.yawDeg = h * 360.0;
}

namespace mapgen_impl {

constexpr int32_t kDirt = 1;
constexpr int32_t kWater = 3;
constexpr int32_t kWall = 4;
constexpr int32_t kCamp = 5;
constexpr int32_t kCampWall = 6;

constexpr bool IsCampTileId(int32_t t) { return t == kCamp || t == kCampWall; }

// Row-major int grid (tiles[row][col] in the web).
struct IntGrid {
  int32_t cols = 0, rows = 0;
  std::vector<int32_t> v;
  IntGrid(int32_t c, int32_t r, int32_t fill) : cols(c), rows(r), v(static_cast<size_t>(c) * static_cast<size_t>(r), fill) {}
  int32_t& At(int32_t c, int32_t r) { return v[static_cast<size_t>(r) * static_cast<size_t>(cols) + static_cast<size_t>(c)]; }
  int32_t At(int32_t c, int32_t r) const {
    return v[static_cast<size_t>(r) * static_cast<size_t>(cols) + static_cast<size_t>(c)];
  }
  bool In(int32_t c, int32_t r) const { return c >= 0 && r >= 0 && c < cols && r < rows; }
};

struct BoolGrid {
  int32_t cols = 0, rows = 0;
  std::vector<uint8_t> v;
  BoolGrid(int32_t c, int32_t r) : cols(c), rows(r), v(static_cast<size_t>(c) * static_cast<size_t>(r), 0) {}
  bool Get(int32_t c, int32_t r) const {
    return v[static_cast<size_t>(r) * static_cast<size_t>(cols) + static_cast<size_t>(c)] != 0;
  }
  void Set(int32_t c, int32_t r, bool b) {
    v[static_cast<size_t>(r) * static_cast<size_t>(cols) + static_cast<size_t>(c)] = b ? 1 : 0;
  }
  bool In(int32_t c, int32_t r) const { return c >= 0 && r >= 0 && c < cols && r < rows; }
};

// clearArea (4.3): Chebyshev square, interior only; skipCamp leaves camp ground / palisade untouched.
void ClearArea(IntGrid& t, int32_t cc, int32_t cr, int32_t radius, int32_t fill, bool skipCamp) {
  for (int32_t dr = -radius; dr <= radius; ++dr) {
    for (int32_t dc = -radius; dc <= radius; ++dc) {
      const int32_t r = cr + dr;
      const int32_t c = cc + dc;
      if (r > 0 && r < t.rows - 1 && c > 0 && c < t.cols - 1) {
        if (skipCamp && IsCampTileId(t.At(c, r))) continue;
        t.At(c, r) = fill;
      }
    }
  }
}

// drunkWalk (4.4).
void DrunkWalk(IntGrid& t, int32_t fromCol, int32_t fromRow, int32_t toCol, int32_t toRow, int32_t pathTile,
               ParkMillerRng& rng, BoolGrid& carved) {
  const int32_t cols = t.cols, rows = t.rows;
  int32_t col = fromCol;
  int32_t row = fromRow;
  const int32_t maxSteps = (cols + rows) * 3;
  int32_t steps = 0;
  while ((col != toCol || row != toRow) && steps < maxSteps) {
    ++steps;
    if (col > 0 && col < cols - 1 && row > 0 && row < rows - 1) {
      if (!IsCampTileId(t.At(col, row))) {
        t.At(col, row) = pathTile;
        carved.Set(col, row, true);
      }
      if (rng.Chance(0.5)) {
        const int32_t ac = col + rng.NextInt(-1, 1);
        const int32_t ar = row + rng.NextInt(-1, 1);
        if (ac > 0 && ac < cols - 1 && ar > 0 && ar < rows - 1) {
          if (!IsCampTileId(t.At(ac, ar))) {
            t.At(ac, ar) = pathTile;
            carved.Set(ac, ar, true);
          }
        }
      }
    }
    const int32_t dx = toCol - col;
    const int32_t dy = toRow - row;
    if (rng.Chance(0.7)) {
      if (std::abs(dx) > std::abs(dy)) {
        col += dx > 0 ? 1 : -1;
      } else {
        row += dy > 0 ? 1 : -1;
      }
    } else {
      const int32_t dir = rng.NextInt(0, 3);
      if (dir == 0 && col < cols - 2) {
        ++col;
      } else if (dir == 1 && col > 1) {
        --col;
      } else if (dir == 2 && row < rows - 2) {
        ++row;
      } else if (dir == 3 && row > 1) {
        --row;
      }
    }
  }
  if (toCol > 0 && toCol < cols - 1 && toRow > 0 && toRow < rows - 1) {
    if (!IsCampTileId(t.At(toCol, toRow))) t.At(toCol, toRow) = pathTile;
  }
}

bool TouchesCarved(const BoolGrid& carved, int32_t col, int32_t row, int32_t reach) {
  for (int32_t r = (std::max)(0, row - reach); r <= (std::min)(carved.rows - 1, row + reach); ++r) {
    for (int32_t c = (std::max)(0, col - reach); c <= (std::min)(carved.cols - 1, col + reach); ++c) {
      if (carved.Get(c, r)) return true;
    }
  }
  return false;
}

BoolGrid CellularAutomata(const BoolGrid& grid, int32_t iterations, int32_t birth, int32_t death) {
  BoolGrid current = grid;
  for (int32_t iter = 0; iter < iterations; ++iter) {
    BoolGrid next = current;
    for (int32_t r = 1; r < current.rows - 1; ++r) {
      for (int32_t c = 1; c < current.cols - 1; ++c) {
        int32_t n = 0;
        for (int32_t dr = -1; dr <= 1; ++dr) {
          for (int32_t dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0) continue;
            if (current.Get(c + dc, r + dr)) ++n;
          }
        }
        next.Set(c, r, current.Get(c, r) ? n >= death : n >= birth);
      }
    }
    current = std::move(next);
  }
  return current;
}

struct KeepClearPoint {
  double col = 0, row = 0;
  double margin = 0;
};

// Multi-source BFS over 8-neighbours: Chebyshev distance to the nearest source, capped at `cap`.
template <class Pred>
std::vector<int32_t> DistTo(int32_t cols, int32_t rows, Pred isSource, int32_t cap) {
  std::vector<int32_t> d(static_cast<size_t>(cols) * static_cast<size_t>(rows), cap);
  std::vector<std::pair<int32_t, int32_t>> frontier;
  for (int32_t r = 0; r < rows; ++r) {
    for (int32_t c = 0; c < cols; ++c) {
      if (isSource(r, c)) {
        d[static_cast<size_t>(r) * static_cast<size_t>(cols) + static_cast<size_t>(c)] = 0;
        frontier.emplace_back(r, c);
      }
    }
  }
  for (int32_t k = 1; k < cap && !frontier.empty(); ++k) {
    std::vector<std::pair<int32_t, int32_t>> next;
    for (const auto& [r, c] : frontier) {
      for (int32_t dr = -1; dr <= 1; ++dr) {
        for (int32_t dc = -1; dc <= 1; ++dc) {
          const int32_t rr = r + dr, cc = c + dc;
          if (rr < 0 || rr >= rows || cc < 0 || cc >= cols) continue;
          int32_t& cell = d[static_cast<size_t>(rr) * static_cast<size_t>(cols) + static_cast<size_t>(cc)];
          if (cell <= k) continue;
          cell = k;
          next.emplace_back(rr, cc);
        }
      }
    }
    frontier = std::move(next);
  }
  return d;
}

bool MatchesLowCover(const std::string& t) {
  static constexpr std::array<std::string_view, 6> kLow{"grass", "flower", "rock", "bones", "mushroom", "fern"};
  for (std::string_view k : kLow) {
    if (t.find(k) != std::string::npos) return t.rfind("boulder", 0) != 0;
  }
  return false;
}

bool InList(const std::vector<std::string>& list, const std::string& v) {
  return std::find(list.begin(), list.end(), v) != list.end();
}

// scatterDecorations (4.7).
std::vector<Decoration> ScatterDecorations(const MapDef& map, const MapThemeDef& config, bool forest,
                                           const IntGrid& tiles, const BoolGrid& collisions, ParkMillerRng& rng,
                                           double seed) {
  const int32_t cols = map.cols, rows = map.rows;
  const std::vector<WeightedDecor>& grove = config.grove;
  const std::vector<WeightedDecor>& open = config.open;
  const std::vector<std::string>& tall = config.tall;

  BoolGrid blocked(cols, rows);
  BoolGrid noTall(cols, rows);
  auto block = [cols, rows](BoolGrid& g, int32_t col, int32_t row, int32_t rad) {
    for (int32_t r = row - rad; r <= row + rad; ++r) {
      for (int32_t c = col - rad; c <= col + rad; ++c) {
        if (r >= 0 && r < rows && c >= 0 && c < cols) g.Set(c, r, true);
      }
    }
  };
  for (const MapExitDef& e : map.exits) {
    block(blocked, e.pos.col, e.pos.row, 3);
    block(noTall, e.pos.col, e.pos.row, 5);
  }
  for (const MapCampDef& c : map.camps) {
    block(blocked, c.pos.col, c.pos.row, 6);
    block(noTall, c.pos.col, c.pos.row, 9);
  }
  block(blocked, map.playerStart.col, map.playerStart.row, 2);
  block(noTall, map.playerStart.col, map.playerStart.row, 5);
  for (const FieldNpcDef& n : map.fieldNpcs) {
    block(blocked, n.pos.col, n.pos.row, 1);
    block(noTall, n.pos.col, n.pos.row, 2);
  }
  for (const SubDungeonEntranceDef& e : map.subDungeonEntrances) {
    block(blocked, e.pos.col, e.pos.row, 2);
    block(noTall, e.pos.col, e.pos.row, 3);
  }
  for (const StoryDecorationDef& d : map.storyDecorations) block(noTall, d.pos.col, d.pos.row, 2);
  for (const MapSpawnDef& s : map.spawns) block(noTall, s.pos.col, s.pos.row, 2);

  const bool dirtIsGround = config.primaryTile == kDirt;
  auto isPath = [&tiles](int32_t r, int32_t c) {
    if (tiles.At(c, r) != kDirt) return false;
    int32_t n = 0;
    for (int32_t dr = -1; dr <= 1; ++dr) {
      for (int32_t dc = -1; dc <= 1; ++dc) {
        if ((dr != 0 || dc != 0) && tiles.In(c + dc, r + dr) && tiles.At(c + dc, r + dr) == kDirt) ++n;
      }
    }
    return n >= 2;
  };
  const std::vector<int32_t> pathDist =
      dirtIsGround ? std::vector<int32_t>(static_cast<size_t>(cols) * static_cast<size_t>(rows), 9)
                   : DistTo(cols, rows, isPath, 3);
  const std::vector<int32_t> obstacleDist = DistTo(
      cols, rows, [&tiles](int32_t r, int32_t c) { return tiles.At(c, r) == kWall || tiles.At(c, r) == kWater; }, 3);
  const double openTallRate = forest ? 0.04 : 0.1;
  const double groveTallRate = forest ? 0.22 : 0.45;
  const double rimTallRate = forest ? 0.5 : 0.8;
  const double hugTallRate = forest ? 0.2 : 0.3;

  BoolGrid tallAt(cols, rows);
  std::vector<WeightedDecor> lowCover;
  for (const WeightedDecor& w : open) {
    if (InList(tall, w.type)) continue;
    if (MatchesLowCover(w.type)) lowCover.push_back(w);
  }

  std::vector<Decoration> out;
  for (int32_t r = 2; r < rows - 2; ++r) {
    for (int32_t c = 2; c < cols - 2; ++c) {
      const int32_t tile = tiles.At(c, r);
      if (!collisions.Get(c, r) || IsCampTileId(tile) || (tile == kDirt && !dirtIsGround) || blocked.Get(c, r)) {
        continue;
      }
      const double n = GroveNoise(c, r, seed, 9);
      const bool inGrove = n > 0.6;
      const double density = inGrove ? config.decorDensity * 3.2 : config.decorDensity * 1.3;
      if (!rng.Chance(density)) continue;
      std::string type = PickWeighted(inGrove ? grove : open, rng.Next());
      bool isTall = false;
      if (InList(tall, type)) {
        const bool rim = (std::min)((std::min)(c, r), (std::min)(cols - 1 - c, rows - 1 - r)) <= 6;
        const size_t idx = static_cast<size_t>(r) * static_cast<size_t>(cols) + static_cast<size_t>(c);
        const bool hugsObstacle = obstacleDist[idx] <= 1;
        const double rate = rim ? rimTallRate : hugsObstacle ? hugTallRate : inGrove ? groveTallRate : openTallRate;
        bool ok = !noTall.Get(c, r) && pathDist[idx] > 2 && rng.Chance(rate);
        for (int32_t dr = -2; dr <= 2 && ok; ++dr) {
          for (int32_t dc = -2; dc <= 2; ++dc) {
            const bool near = std::abs(dr) <= 1 && std::abs(dc) <= 1;
            const bool stacked = dr == dc || std::abs(dr - dc) == 1;
            if ((near || stacked) && tallAt.In(c + dc, r + dr) && tallAt.Get(c + dc, r + dr)) {
              ok = false;
              break;
            }
          }
        }
        if (ok) {
          tallAt.Set(c, r, true);
          isTall = true;
        } else {
          type = PickWeighted(!lowCover.empty() ? std::span<const WeightedDecor>(lowCover)
                                                : std::span<const WeightedDecor>(open),
                              rng.Next());
        }
      }
      Decoration d;
      d.type = std::move(type);
      d.col = c;
      d.row = r;
      d.blocking = isTall;
      out.push_back(std::move(d));
    }
  }
  return out;
}

}  // namespace mapgen_impl

ZoneGrid GenerateMap(const MapGenDef& gen, const MapGenInput& in, MapGenTrace* trace) {
  using namespace mapgen_impl;
  if (in.map == nullptr || in.map->cols <= 0 || in.map->rows <= 0) return ZoneGrid();
  const MapDef& map = *in.map;
  const MapTheme theme = map.hasTheme ? map.theme : gen.defaultTheme;
  const double seed = static_cast<double>(map.seed);
  ParkMillerRng rng(map.seed);
  const int32_t cols = map.cols, rows = map.rows;
  const MapThemeDef& config = gen.Theme(theme);
  const int32_t primary = config.primaryTile;
  auto checkpoint = [&rng](int64_t& state, int64_t& count) {
    state = rng.State();
    count = rng.Draws();
  };

  IntGrid tiles(cols, rows, primary);
  auto border = [&]() {
    for (int32_t c = 0; c < cols; ++c) {
      tiles.At(c, 0) = kWall;
      tiles.At(c, rows - 1) = kWall;
    }
    for (int32_t r = 0; r < rows; ++r) {
      tiles.At(0, r) = kWall;
      tiles.At(cols - 1, r) = kWall;
    }
  };
  border();

  // (s) secondary scatter
  for (int32_t r = 1; r < rows - 1; ++r) {
    for (int32_t c = 1; c < cols - 1; ++c) {
      if (rng.Chance(0.08)) tiles.At(c, r) = config.secondaryTile;
    }
  }
  if (trace != nullptr) checkpoint(trace->afterSecondary, trace->drawsSecondary);
  // (w) wall scatter
  for (int32_t r = 2; r < rows - 2; ++r) {
    for (int32_t c = 2; c < cols - 2; ++c) {
      if (rng.Chance(config.wallDensity)) tiles.At(c, r) = kWall;
    }
  }
  // (m) mountain ridges
  if (theme == MapTheme::Mountain) {
    const int32_t ridgeCount = rng.NextInt(3, 6);
    for (int32_t i = 0; i < ridgeCount; ++i) {
      int32_t rc = rng.NextInt(5, cols - 6);
      int32_t rr = rng.NextInt(5, rows - 6);
      const int32_t length = rng.NextInt(6, 15);
      const int32_t dirCol = rng.Chance(0.5) ? 1 : 0;
      const int32_t dirRow = dirCol == 1 ? 0 : 1;
      for (int32_t s = 0; s < length; ++s) {
        if (rc > 1 && rc < cols - 2 && rr > 1 && rr < rows - 2) {
          tiles.At(rc, rr) = kWall;
          if (rng.Chance(0.4)) {
            const int32_t offR = rr + (dirCol == 1 ? rng.NextInt(-1, 1) : 0);
            const int32_t offC = rc + (dirRow == 1 ? rng.NextInt(-1, 1) : 0);
            if (offR > 1 && offR < rows - 2 && offC > 1 && offC < cols - 2) tiles.At(offC, offR) = kWall;
          }
        }
        rc += dirCol + (rng.Chance(0.3) ? rng.NextInt(-1, 1) : 0);
        rr += dirRow + (rng.Chance(0.3) ? rng.NextInt(-1, 1) : 0);
      }
    }
  }
  if (trace != nullptr) checkpoint(trace->afterWalls, trace->drawsWalls);

  // (c) camps
  for (const MapCampDef& camp : map.camps) {
    constexpr int32_t halfSize = 5;
    ClearArea(tiles, camp.pos.col, camp.pos.row, halfSize + 1, primary, false);
    for (int32_t dr = -halfSize; dr <= halfSize; ++dr) {
      for (int32_t dc = -halfSize; dc <= halfSize; ++dc) {
        const int32_t r = camp.pos.row + dr;
        const int32_t c = camp.pos.col + dc;
        if (r > 0 && r < rows - 1 && c > 0 && c < cols - 1) tiles.At(c, r) = kCamp;
      }
    }
    for (int32_t dc = -halfSize; dc <= halfSize; ++dc) {
      if (dc == -1 || dc == 0) continue;
      const int32_t r = camp.pos.row - halfSize;
      const int32_t c = camp.pos.col + dc;
      if (r > 0 && r < rows - 1 && c > 0 && c < cols - 1) tiles.At(c, r) = kCampWall;
    }
    for (int32_t dr = -halfSize; dr < halfSize - 1; ++dr) {
      const int32_t r = camp.pos.row + dr;
      const int32_t c = camp.pos.col - halfSize;
      if (r > 0 && r < rows - 1 && c > 0 && c < cols - 1) tiles.At(c, r) = kCampWall;
    }
    for (int32_t dr = -halfSize; dr < halfSize - 1; ++dr) {
      const int32_t r = camp.pos.row + dr;
      const int32_t c = camp.pos.col + halfSize;
      if (r > 0 && r < rows - 1 && c > 0 && c < cols - 1) tiles.At(c, r) = kCampWall;
    }
  }

  // (x) clearings (skip camp tiles)
  for (const MapSpawnDef& s : map.spawns) ClearArea(tiles, s.pos.col, s.pos.row, 2, primary, true);
  for (const MapExitDef& e : map.exits) ClearArea(tiles, e.pos.col, e.pos.row, 1, primary, true);
  ClearArea(tiles, map.playerStart.col, map.playerStart.row, 2, primary, true);
  for (const FieldNpcDef& n : map.fieldNpcs) ClearArea(tiles, n.pos.col, n.pos.row, 1, primary, true);
  for (const SubDungeonEntranceDef& e : map.subDungeonEntrances) ClearArea(tiles, e.pos.col, e.pos.row, 1, primary, true);
  for (const StoryDecorationDef& d : map.storyDecorations) ClearArea(tiles, d.pos.col, d.pos.row, 1, primary, true);
  for (const AvoidPointDef& p : in.avoid) ClearArea(tiles, p.pos.col, p.pos.row, 1, primary, true);

  // (d) paths
  BoolGrid carved(cols, rows);
  auto walk = [&](TilePos from, TilePos to) {
    DrunkWalk(tiles, from.col, from.row, to.col, to.row, kDirt, rng, carved);
  };
  if (!map.camps.empty()) walk(map.playerStart, map.camps[0].pos);
  for (size_t i = 0; i + 1 < map.camps.size(); ++i) walk(map.camps[i].pos, map.camps[i + 1].pos);
  if (!map.camps.empty()) {
    const TilePos last = map.camps.back().pos;
    for (const MapExitDef& e : map.exits) walk(last, e.pos);
  }
  for (const MapExitDef& e : map.exits) walk(map.playerStart, e.pos);
  for (const MapSpawnDef& s : map.spawns) {
    TilePos nearest = map.playerStart;
    int32_t best = std::abs(s.pos.col - nearest.col) + std::abs(s.pos.row - nearest.row);
    for (const MapCampDef& c : map.camps) {
      const int32_t d = std::abs(s.pos.col - c.pos.col) + std::abs(s.pos.row - c.pos.row);
      if (d < best) {
        best = d;
        nearest = c.pos;
      }
    }
    walk(s.pos, nearest);
  }
  if (trace != nullptr) checkpoint(trace->afterPaths, trace->drawsPaths);

  // (e) lakes
  const int32_t lakeCount = rng.NextInt(config.lakeCountMin, config.lakeCountMax);
  BoolGrid water(cols, rows);
  std::vector<KeepClearPoint> keepClear;
  auto add = [&keepClear](TilePos p, double margin) {
    keepClear.push_back(KeepClearPoint{static_cast<double>(p.col), static_cast<double>(p.row), margin});
  };
  add(map.playerStart, 5);
  for (const MapCampDef& c : map.camps) add(c.pos, 9);
  for (const MapSpawnDef& s : map.spawns) add(s.pos, 4);
  for (const MapExitDef& e : map.exits) add(e.pos, 4);
  for (const FieldNpcDef& n : map.fieldNpcs) add(n.pos, 3);
  for (const SubDungeonEntranceDef& e : map.subDungeonEntrances) add(e.pos, 3);
  for (const HiddenAreaDef& h : map.hiddenAreas) add(h.center, 4);
  for (const StoryDecorationDef& d : map.storyDecorations) add(d.pos, 2);
  for (const PetSpawnDef& p : map.petSpawns) add(p.pos, 2);
  for (const AvoidPointDef& p : in.avoid) add(p.pos, p.hasMargin ? p.margin : gen.defaultAvoidMargin);
  for (int32_t lake = 0; lake < lakeCount; ++lake) {
    const double radius = 2.2 + static_cast<double>(rng.NextInt(config.lakeSizeMin, config.lakeSizeMax)) * 0.28;
    for (int32_t attempt = 0; attempt < 24; ++attempt) {
      const int32_t lc = rng.NextInt(10, cols - 11);
      const int32_t lr = rng.NextInt(10, rows - 11);
      bool blockedLake = false;
      for (const KeepClearPoint& p : keepClear) {
        if (JsHypot(p.col - lc, p.row - lr) < radius * 1.3 + p.margin) {
          blockedLake = true;
          break;
        }
      }
      if (blockedLake) continue;
      const int32_t reach = static_cast<int32_t>(std::ceil(radius * 1.3));
      if (TouchesCarved(carved, lc, lr, reach + 2)) continue;
      const double noiseSeed = seed + static_cast<double>(lake) * 131.0;
      for (int32_t dr = -reach; dr <= reach; ++dr) {
        for (int32_t dc = -reach; dc <= reach; ++dc) {
          const int32_t c = lc + dc, r = lr + dr;
          if (c < 3 || c > cols - 4 || r < 3 || r > rows - 4) continue;
          const double edge = radius * (0.7 + 0.6 * GroveNoise(c, r, noiseSeed, 3.5));
          if (JsHypot(dc, dr) < edge) water.Set(c, r, true);
        }
      }
      break;
    }
  }
  if (trace != nullptr) checkpoint(trace->afterLakes, trace->drawsLakes);
  const BoolGrid refined = CellularAutomata(water, 2, 5, 4);
  for (int32_t r = 2; r < rows - 2; ++r) {
    for (int32_t c = 2; c < cols - 2; ++c) {
      if (refined.Get(c, r)) tiles.At(c, r) = kWater;
    }
  }

  // (b') border again, (g) exit inner pads
  border();
  for (const MapExitDef& e : map.exits) {
    const int32_t ec = e.pos.col, er = e.pos.row;
    if (ec == 0 || ec == cols - 1 || er == 0 || er == rows - 1) {
      const int32_t innerCol = ec == 0 ? 1 : ec == cols - 1 ? cols - 2 : ec;
      const int32_t innerRow = er == 0 ? 1 : er == rows - 1 ? rows - 2 : er;
      if (tiles.In(innerCol, innerRow)) tiles.At(innerCol, innerRow) = kDirt;
      ClearArea(tiles, innerCol, innerRow, 1, primary, false);
    }
  }

  // (j) collisions
  BoolGrid collisions(cols, rows);
  for (int32_t r = 0; r < rows; ++r) {
    for (int32_t c = 0; c < cols; ++c) {
      const int32_t t = tiles.At(c, r);
      collisions.Set(c, r, t != kWall && t != kWater && t != kCampWall);
    }
  }

  // (k) decorations
  std::vector<Decoration> decor =
      ScatterDecorations(map, config, theme == MapTheme::Forest, tiles, collisions, rng, seed);
  if (trace != nullptr) checkpoint(trace->afterDecorations, trace->drawsDecorations);

  ZoneGrid grid(cols, rows);
  for (int32_t r = 0; r < rows; ++r) {
    for (int32_t c = 0; c < cols; ++c) grid.SetTile(c, r, static_cast<TileType>(tiles.At(c, r)));
  }
  for (size_t i = 0; i < decor.size(); ++i) ApplyDecorationJitter(decor[i], i);
  grid.Decorations() = std::move(decor);
  return grid;
}

std::vector<CampProp> CampProps(const MapDef& map) {
  struct Off {
    const char* type;
    int32_t dc, dr;
    bool blocking;
  };
  // ZoneScene.buildCampDecorations push order (3.4); blocking per 3.4 (barrel / crate) and W5 (well, tents).
  static constexpr Off kProps[] = {
      {"campfire", 0, 0, false},  {"well", 1, -1, true},     {"banner", -1, -4, false}, {"banner", 2, -4, false},
      {"tent", -3, -2, true},     {"tent", 3, -2, true},     {"tent", -2, 2, true},     {"tent", 2, 2, true},
      {"barrel", -2, 0, true},    {"crate", 2, 0, true},     {"crate", -3, -3, true},   {"barrel", 3, -3, true},
      {"banner", -5, 4, false},   {"banner", 5, 4, false},   {"torch", -5, 5, false},   {"torch", 5, 5, false},
      {"torch", -5, -2, false},   {"torch", -5, 1, false},   {"torch", 5, -2, false},   {"torch", 5, 1, false},
      {"torch", -2, -5, false},   {"torch", 3, -5, false},
  };
  // Camp NPC slots (quests-story-ch1.md 6.2 / world 3.4): npcs[i] at slots[i % 6].
  static constexpr std::array<std::pair<int32_t, int32_t>, 6> kSlots{{{-3, -2}, {3, -2}, {-3, 2}, {3, 2}, {0, -3}, {0, 3}}};
  std::vector<CampProp> out;
  for (size_t ci = 0; ci < map.camps.size(); ++ci) {
    const MapCampDef& camp = map.camps[ci];
    for (const Off& o : kProps) {
      CampProp p;
      p.type = o.type;
      p.pos = TilePos{camp.pos.col + o.dc, camp.pos.row + o.dr};
      p.campIndex = static_cast<int32_t>(ci);
      p.blocking = o.blocking;
      if (p.blocking && p.type == "tent") {
        for (size_t i = 0; i < camp.npcs.size(); ++i) {
          const auto& s = kSlots[i % kSlots.size()];
          if (s.first == o.dc && s.second == o.dr) p.blocking = false;  // the NPC stands in front of its tent
        }
      }
      out.push_back(std::move(p));
    }
  }
  return out;
}

void ApplyCampBlockers(ZoneGrid& grid, const MapDef& map) {
  for (const CampProp& p : CampProps(map)) {
    if (p.blocking && grid.InBounds(p.pos.col, p.pos.row)) grid.SetWalkable(p.pos.col, p.pos.row, false);
  }
}

namespace mapgen_impl {

void CollectAnchors(const MapDef& map, const DataStore& data, std::vector<TilePos>& out) {
  out.push_back(map.playerStart);
  static constexpr std::array<std::pair<int32_t, int32_t>, 6> kSlots{{{-3, -2}, {3, -2}, {-3, 2}, {3, 2}, {0, -3}, {0, 3}}};
  for (const MapCampDef& c : map.camps) {
    out.push_back(c.pos);
    for (size_t i = 0; i < c.npcs.size(); ++i) {
      const auto& s = kSlots[i % kSlots.size()];
      out.push_back(TilePos{c.pos.col + s.first, c.pos.row + s.second});
    }
  }
  for (const FieldNpcDef& n : map.fieldNpcs) out.push_back(n.pos);
  for (const MapSpawnDef& s : map.spawns) out.push_back(s.pos);
  for (const MapExitDef& e : map.exits) {
    const int32_t ic = e.pos.col == 0 ? 1 : e.pos.col == map.cols - 1 ? map.cols - 2 : e.pos.col;
    const int32_t ir = e.pos.row == 0 ? 1 : e.pos.row == map.rows - 1 ? map.rows - 2 : e.pos.row;
    out.push_back(TilePos{ic, ir});
  }
  for (const HiddenAreaDef& h : map.hiddenAreas) {
    for (const HiddenRewardDef& r : h.rewards) out.push_back(r.pos);
  }
  for (const StoryDecorationDef& d : map.storyDecorations) out.push_back(d.pos);
  for (const SubDungeonEntranceDef& e : map.subDungeonEntrances) out.push_back(e.pos);
  for (const AvoidPointDef& a : map.generatorAvoid) out.push_back(a.pos);
  (void)data;
}

}  // namespace mapgen_impl

void ApplyDecorationBlocking(ZoneGrid& grid, const DataStore& data, const MapDef* map) {
  std::vector<TilePos> anchors;
  if (map != nullptr) mapgen_impl::CollectAnchors(*map, data, anchors);
  auto isAnchor = [&anchors](int32_t c, int32_t r) {
    for (const TilePos& a : anchors) {
      if (a.col == c && a.row == r) return true;
    }
    return false;
  };
  const AssetManifest& manifest = data.Assets();
  for (Decoration& d : grid.Decorations()) {
    bool blocking = d.blocking;
    int32_t fw = 1, fh = 1;
    if (manifest.loaded) {
      const AssetEntryDef* a = manifest.FindByGameId(d.type);
      if (a == nullptr) a = manifest.FindByGameId("decor_" + d.type);
      if (a != nullptr) {
        if (a->hasBlocking) blocking = a->blocking;
        if (a->hasFootprint) {
          fw = (std::max)(1, a->footprintW);
          fh = (std::max)(1, a->footprintH);
        }
      }
    }
    d.blocking = blocking;
    if (!blocking) continue;
    const int32_t c0 = JsRoundInt(d.col) - (fw - 1) / 2;
    const int32_t r0 = JsRoundInt(d.row) - (fh - 1) / 2;
    for (int32_t r = r0; r < r0 + fh; ++r) {
      for (int32_t c = c0; c < c0 + fw; ++c) {
        if (!grid.InBounds(c, r) || isAnchor(c, r)) continue;
        grid.SetWalkable(c, r, false);
      }
    }
  }
}

ZoneGrid BuildRawZoneGrid(const DataStore& data, const MapDef& map) {
  if (map.generated) {
    MapGenInput in;
    in.map = &map;
    in.avoid = map.generatorAvoid;
    return GenerateMap(data.World().mapGen, in);
  }
  ZoneGrid g(map.cols, map.rows);
  for (int32_t r = 0; r < map.rows; ++r) {
    const std::string* row = static_cast<size_t>(r) < map.tiles.size() ? &map.tiles[static_cast<size_t>(r)] : nullptr;
    for (int32_t c = 0; c < map.cols; ++c) {
      int32_t id = 0;
      if (row != nullptr && static_cast<size_t>(c) < row->size()) id = (*row)[static_cast<size_t>(c)] - '0';
      if (id < 0 || id > 6) id = static_cast<int32_t>(TileType::Wall);
      g.SetTile(c, r, static_cast<TileType>(id));
    }
  }
  if (!map.collisions.empty()) {
    for (int32_t r = 0; r < map.rows; ++r) {
      const std::string* row =
          static_cast<size_t>(r) < map.collisions.size() ? &map.collisions[static_cast<size_t>(r)] : nullptr;
      for (int32_t c = 0; c < map.cols; ++c) {
        const bool walk = row != nullptr && static_cast<size_t>(c) < row->size() && (*row)[static_cast<size_t>(c)] == '1';
        g.SetWalkable(c, r, walk);
      }
    }
  }
  const MapThemeDef& theme = data.World().mapGen.Theme(map.hasTheme ? map.theme : data.World().mapGen.defaultTheme);
  std::vector<Decoration>& decor = g.Decorations();
  decor.clear();
  for (size_t i = 0; i < map.decorations.size(); ++i) {
    const DecorationDef& def = map.decorations[i];
    Decoration d;
    d.type = def.type;
    d.col = def.pos.col;
    d.row = def.pos.row;
    d.blocking = mapgen_impl::InList(theme.tall, def.type);
    ApplyDecorationJitter(d, i);
    decor.push_back(std::move(d));
  }
  return g;
}

ZoneGrid BuildZoneGrid(const DataStore& data, const MapDef& map) {
  ZoneGrid g = BuildRawZoneGrid(data, map);
  ApplyCampBlockers(g, map);
  ApplyDecorationBlocking(g, data, &map);
  return g;
}

}  // namespace abyss
