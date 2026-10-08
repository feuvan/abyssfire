// Save area (world owner; per-section readers belong to each area): migrations, parse, serialise, position fallback.
// Spec vectors: save-ui-input.md section 15 and 3.7. Foundation tests only.
#include <functional>
#include <vector>

#include "TestUtil.h"
#include "abyss/save/SaveIO.h"
#include "doctest/doctest.h"

using namespace abyss;

namespace {
// Border walls, everything else walkable; `blocked` adds unwalkable tiles.
std::function<bool(int32_t, int32_t)> BorderMap(int32_t cols, int32_t rows, std::vector<TilePos> blocked = {}) {
  return [=](int32_t c, int32_t r) {
    if (c <= 0 || r <= 0 || c >= cols - 1 || r >= rows - 1) return false;
    for (const TilePos& b : blocked) {
      if (b.col == c && b.row == r) return false;
    }
    return true;
  };
}
}  // namespace

TEST_SUITE("save") {
  TEST_CASE("findNearestWalkablePosition vectors (save-ui-input 3.7)") {
    TilePos out;
    const std::vector<TilePos> camp1{{5, 5}};
    CHECK(FindNearestWalkablePosition(0, 0, BorderMap(20, 20), 20, 20, camp1, out));
    CHECK(out == TilePos{5, 5});
    CHECK_FALSE(FindNearestWalkablePosition(3, 3, BorderMap(20, 20), 20, 20, camp1, out));
    const std::vector<TilePos> camps2{{5, 5}, {15, 15}};
    CHECK(FindNearestWalkablePosition(14, 14, BorderMap(20, 20, {{14, 14}}), 20, 20, camps2, out));
    CHECK(out == TilePos{15, 15});
    CHECK(FindNearestWalkablePosition(200, 200, BorderMap(20, 20), 20, 20, camp1, out));
    CHECK(out == TilePos{5, 5});
    const std::vector<TilePos> camp15{{15, 15}};
    CHECK(FindNearestWalkablePosition(79, 79, BorderMap(120, 120, {{79, 79}}), 120, 120, camp15, out));
    CHECK(out == TilePos{15, 15});
    CHECK_FALSE(FindNearestWalkablePosition(1, 1, BorderMap(20, 20), 20, 20, camp1, out));
    const std::vector<TilePos> camps3{{5, 5}, {25, 25}};
    CHECK(FindNearestWalkablePosition(23, 23, BorderMap(30, 30, {{23, 23}}), 30, 30, camps3, out));
    CHECK(out == TilePos{25, 25});
    CHECK_FALSE(FindNearestWalkablePosition(0, 0, BorderMap(20, 20), 20, 20, {}, out));  // no camps
  }

  TEST_CASE("SaveData defaults are the v4 new-game shape") {
    SaveData s;
    CHECK(s.version == kCurrentSaveVersion);
    CHECK(s.version == 4);
    CHECK(s.exploration.IsObject());
    CHECK(s.abyss.unlockedTier == 1);
    CHECK(s.abyss.bestTier == 0);
    CHECK_FALSE(s.soulEcho.present);
    const SaveSlotInfo info = SummarizeSave(2, s);
    CHECK(info.slot == 2);
    CHECK(info.exists);
  }
}
