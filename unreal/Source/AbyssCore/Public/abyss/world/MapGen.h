// Bit-exact port of the web MapGenerator (DECISIONS W10: same seed -> same grid, the specs' golden layouts hold).
// Spec: world-map-nav.md 0.1 (floating-point determinism), 3.2-3.3 (landmarks, golden result), 3.4 (camp blockers),
// 4.1 (SeededRandom Park-Miller, test vector), 4.2 (themes), 4.3 (pipeline order; every RNG draw listed), 4.4 (drunk
// walk), 4.5 (lakes, groveNoise), 4.6 (cellular automata), 4.7 (decoration scatter), 4.9 (golden tests), 15.5
// (decoration placement jitter), Appendix A/B.
//
// Owner area: world. Pure. Golden fixtures: CoreTests/golden/maps/*.json (exported from the TS generator).
#pragma once

#include <cstdint>
#include <span>
#include <string>
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
  int64_t Draws() const { return draws_; }  // next() calls so far (golden checkpoints, 3.3)

 private:
  int64_t state_;
  int64_t draws_ = 0;
};

// groveNoise(c, r, seed, scale) (4.5): smooth value noise in [0, 1], independent of the tile RNG.
ABYSS_API double GroveNoise(double col, double row, double seed, double scale);
// pickWeighted (4.7): total = sum of weights left to right; x = u * total; first type where (x -= w) <= 0; else last.
ABYSS_API const std::string& PickWeighted(std::span<const WeightedDecor> pool, double u);

struct MapGenInput {
  const MapDef* map = nullptr;
  std::span<const AvoidPointDef> avoid;  // external landmarks (3.2); a point without a margin uses gen.defaultAvoidMargin
};

// Park-Miller checkpoints after each pipeline phase (golden test 4.9 item 3 pinpoints the first diverging phase).
struct MapGenTrace {
  int64_t afterSecondary = 0, afterWalls = 0, afterPaths = 0, afterLakes = 0, afterDecorations = 0;
  int64_t drawsSecondary = 0, drawsWalls = 0, drawsPaths = 0, drawsLakes = 0, drawsDecorations = 0;
};

// generate(map, avoid) (4.3): tiles, collisions and decorations, exact draw order. Theme = map.theme (default
// gen.defaultTheme), seed = map.seed. Decorations carry the 15.5 render jitter and `blocking` = type in the theme's tall
// set (W5; BuildZoneGrid bakes it into the collision grid).
ABYSS_API ZoneGrid GenerateMap(const MapGenDef& gen, const MapGenInput& in, MapGenTrace* trace = nullptr);

// The 15.5 placement jitter of decoration `index` (generation order): fills offsetCol/offsetRow/scale/yawDeg.
ABYSS_API void ApplyDecorationJitter(Decoration& d, size_t index);

// Runtime camp props (ZoneScene.buildCampDecorations, 3.4), per camp in camp order, in the web's push order.
struct CampProp {
  std::string type;  // campfire | well | banner | tent | barrel | crate | torch
  TilePos pos;
  int32_t campIndex = 0;
  bool blocking = false;  // 3.4: barrel / crate; W5: well and tents (see CampProps)
};
// W5 camp props: barrels and crates block (3.4), the well and the tents block too (W5) except where a camp NPC stands
// (slots (-3,-2) and (+3,-2) share the tent tiles: the NPC stands in front of its tent and must stay reachable).
ABYSS_API std::vector<CampProp> CampProps(const MapDef& map);

// Builds the zone grid for a map: generated maps go through GenerateMap with the exporter's landmark list; hand-built
// maps (ember_tower) parse their digit rows (tiles + collisions) and decorations. Then camp blockers (3.4 + W5 camp
// props) and the W5 decoration footprints are applied.
ABYSS_API ZoneGrid BuildZoneGrid(const DataStore& data, const MapDef& map);
// The generator-only grid (no camp blockers, no W5 blocking): what the web's module load produced (golden fixtures).
ABYSS_API ZoneGrid BuildRawZoneGrid(const DataStore& data, const MapDef& map);

// Camp blockers: every blocking CampProp tile becomes unwalkable (bounds-checked; idempotent).
ABYSS_API void ApplyCampBlockers(ZoneGrid& grid, const MapDef& map);
// W5: blocking decoration footprints. A decoration blocks when the asset manifest entry for its type (game id `<type>`
// or `decor_<type>`) says so (footprint w x h tiles centred on the decoration tile), else when its generator flag is set
// (the theme's tall set). When `map` is given, anchor tiles stay walkable: the player start, camps (+ NPC slots),
// field NPCs, exits' inner tiles, spawn centres, hidden-area rewards, story decorations, sub-dungeon entrances and the
// generator's external landmarks (lore, mini-boss, quest spots).
ABYSS_API void ApplyDecorationBlocking(ZoneGrid& grid, const DataStore& data, const MapDef* map = nullptr);

}  // namespace abyss
