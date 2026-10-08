// Save JSON top level (save-ui-input.md 3.2-3.7, 3.10, 11; U2, U3). FindNearestWalkablePosition and the string-list
// helpers are implemented; migration / parse / serialise are STUBS. Owner area: world.
#include "abyss/base/Platform.h"

#include "abyss/save/SaveIO.h"

#include <cmath>

#include "abyss/base/Assert.h"
#include "abyss/base/Math.h"
#include "SaveSections.h"

namespace abyss {

bool MigrateRaw(JsonValue& raw) {
  ABYSS_UNIMPLEMENTED();
  return raw.Type() == JsonType::Object;
}

SaveError ParseSave(std::string_view json, SaveData& out, std::string* errorMessage) {
  ABYSS_UNIMPLEMENTED();
  if (errorMessage != nullptr) *errorMessage = "ParseSave not implemented";
  return SaveError::Invalid;
}

std::string SerializeSave(const SaveData& s, bool pretty) {
  ABYSS_UNIMPLEMENTED();
  return "{}";
}

bool FindNearestWalkablePosition(double col, double row, const std::function<bool(int32_t, int32_t)>& walkable,
                                 int32_t cols, int32_t rows, std::span<const TilePos> camps, TilePos& out) {
  const int32_t rc = JsRoundInt(col);
  const int32_t rr = JsRoundInt(row);
  const bool inBounds = rc >= 0 && rr >= 0 && rc < cols && rr < rows;
  if (inBounds && walkable(rc, rr)) return false;
  if (camps.empty()) return false;
  size_t best = 0;
  double bestSq = 0;
  for (size_t i = 0; i < camps.size(); ++i) {
    const double dc = camps[i].col - col, dr = camps[i].row - row;
    const double dSq = dc * dc + dr * dr;
    if (i == 0 || dSq < bestSq) {
      best = i;
      bestSq = dSq;
    }
  }
  out = camps[best];
  return true;
}

SaveSlotInfo SummarizeSave(int32_t slot, const SaveData& s) {
  SaveSlotInfo info;
  info.slot = slot;
  info.exists = true;
  info.timestamp = s.timestamp;
  info.classId = s.classId;
  info.level = s.player.level;
  info.mapId = s.player.currentMap;
  info.playTimeMs = s.playTimeMs;
  info.difficulty = s.difficulty;
  info.completedDifficulties = DeriveCompletedDifficulties(s.difficulty, s.completedDifficulties);
  info.showDifficultySelector = ShouldShowDifficultySelector(info.difficulty, info.completedDifficulties);
  return info;
}

namespace savejson {

void WriteStringList(JsonWriter& w, const std::vector<std::string>& v) {
  w.StartArray();
  for (const std::string& s : v) w.String(s);
  w.EndArray();
}

void ReadStringList(const JsonValue& v, std::vector<std::string>& out) {
  out.clear();
  if (v.Type() != JsonType::Array) return;
  for (const JsonValue& e : v.Items()) {
    if (e.Type() == JsonType::String) out.emplace_back(e.AsString());
  }
}

}  // namespace savejson
}  // namespace abyss
