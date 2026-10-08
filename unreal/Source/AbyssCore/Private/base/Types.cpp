#include "abyss/base/Platform.h"

#include "abyss/base/Types.h"

#include <algorithm>

#include "abyss/base/Math.h"

namespace abyss {

TilePos RoundToTile(Vec2 p) { return {JsRoundInt(p.x), JsRoundInt(p.y)}; }

bool IdIndex::Add(std::string_view id) {
  auto it = std::lower_bound(sorted_.begin(), sorted_.end(), id,
                             [](const std::pair<std::string, int32_t>& e, std::string_view k) { return e.first < k; });
  if (it != sorted_.end() && it->first == id) return false;
  const int32_t index = static_cast<int32_t>(ids_.size());
  ids_.emplace_back(id);
  sorted_.insert(it, {std::string(id), index});
  return true;
}

int32_t IdIndex::Find(std::string_view id) const {
  auto it = std::lower_bound(sorted_.begin(), sorted_.end(), id,
                             [](const std::pair<std::string, int32_t>& e, std::string_view k) { return e.first < k; });
  if (it != sorted_.end() && it->first == id) return it->second;
  return -1;
}

void IdIndex::Clear() {
  ids_.clear();
  sorted_.clear();
}

}  // namespace abyss
