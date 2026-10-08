// Map generator (world-map-nav.md section 4; W5, W10). ParkMillerRng is implemented (4.1 test vector in
// world_test.cpp); GenerateMap / blockers are STUBS: owner area world. Until the port lands, BuildZoneGrid returns an
// open placeholder grid (walkable interior, wall border) so the rest of the sim can run.
#include "abyss/base/Platform.h"

#include "abyss/world/MapGen.h"

#include <cmath>

#include "abyss/base/Assert.h"
#include "abyss/data/DataStore.h"

namespace abyss {

ParkMillerRng::ParkMillerRng(int64_t seed) : state_(seed % 2147483647) {
  if (state_ <= 0) state_ += 2147483646;
}

double ParkMillerRng::Next() {
  state_ = (state_ * 16807) % 2147483647;
  return static_cast<double>(state_ - 1) / 2147483646.0;
}

int32_t ParkMillerRng::NextInt(int32_t lo, int32_t hi) {
  return static_cast<int32_t>(std::floor(Next() * static_cast<double>(hi - lo + 1))) + lo;
}

ZoneGrid GenerateMap(const MapGenDef& gen, const MapGenInput& in) {
  ABYSS_UNIMPLEMENTED();
  return ZoneGrid(in.map != nullptr ? in.map->cols : 0, in.map != nullptr ? in.map->rows : 0);
}

ZoneGrid BuildZoneGrid(const DataStore& data, const MapDef& map) {
  ABYSS_UNIMPLEMENTED();
  ZoneGrid g(map.cols, map.rows);
  for (int32_t c = 0; c < map.cols; ++c) {
    g.SetTile(c, 0, TileType::Wall);
    g.SetTile(c, map.rows - 1, TileType::Wall);
  }
  for (int32_t r = 0; r < map.rows; ++r) {
    g.SetTile(0, r, TileType::Wall);
    g.SetTile(map.cols - 1, r, TileType::Wall);
  }
  return g;
}

void ApplyCampBlockers(ZoneGrid& grid, const MapDef& map) { ABYSS_UNIMPLEMENTED(); }

void ApplyDecorationBlocking(ZoneGrid& grid, const DataStore& data) { ABYSS_UNIMPLEMENTED(); }

}  // namespace abyss
