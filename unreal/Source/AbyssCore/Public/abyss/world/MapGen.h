// Bit-exact port of the web MapGenerator (DECISIONS W10: same seed -> same grid, the specs' golden layouts hold).
// Spec: world-map-nav.md 0.1 (floating-point determinism), 3.2-3.3 (landmarks, golden result), 3.4 (camp blockers),
// 4.1 (SeededRandom Park-Miller, test vector), 4.2 (themes), 4.3 (pipeline order; every RNG draw listed), 4.4 (drunk
// walk), 4.5 (lakes, groveNoise), 4.6 (cellular automata), 4.7 (decoration scatter), 4.9 (golden tests), Appendix A/B.
//
// Owner area: world. Pure. Golden fixtures: Data/golden/maps/*.json (exported from the TS generator).
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/data/MapData.h"
#include "abyss/world/Grid.h"

namespace abyss {

class DataStore;

// SeededRandom (4.1): state = seed % 2147483647 (<= 0 -> += 2147483646); next = (state * 16807) % 2147483647,
// returns (state - 1) / 2147483646.
class ABYSS_API ParkMillerRng {
 public:
  explicit ParkMillerRng(int64_t seed);
  double Next();
  int32_t NextInt(int32_t lo, int32_t hi);  // floor(next * (hi - lo + 1)) + lo
  bool Chance(double p) { return Next() < p; }
  int64_t State() const { return state_; }

 private:
  int64_t state_;
};

struct MapGenInput {
  const MapDef* map = nullptr;
  std::span<const AvoidPointDef> avoid;  // external landmarks (3.2); margin default 4
};

// generate(map, avoid) (4.3): tiles, collisions and decorations, exact draw order.
ABYSS_API ZoneGrid GenerateMap(const MapGenDef& gen, const MapGenInput& in);

// Builds the zone grid for a map id: generated maps go through GenerateMap with the exporter's landmark list; hand-built
// maps (ember_tower) parse their digit rows. Then camp blockers (3.4) and W5 decoration footprints are applied.
ABYSS_API ZoneGrid BuildZoneGrid(const DataStore& data, const MapDef& map);

// Camp blockers (3.4): the 8 barrel/crate tiles per camp become unwalkable.
ABYSS_API void ApplyCampBlockers(ZoneGrid& grid, const MapDef& map);
// W5: blocking decoration footprints (asset manifest footprint/blocking, else map_gen tall list).
ABYSS_API void ApplyDecorationBlocking(ZoneGrid& grid, const DataStore& data);

}  // namespace abyss
