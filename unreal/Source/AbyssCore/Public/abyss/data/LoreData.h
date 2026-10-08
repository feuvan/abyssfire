// Lore collectibles. Source: lore.json (byZone, pickupRangeSq). Spec: quests-story-ch1.md 1.9, world-map-nav.md 12.1.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"

namespace abyss {

struct LoreEntryDef {
  std::string id;
  std::string zone;
  std::string name, text;  // zh-CN fallbacks (i18n data.lore.<id>.name/.text)
  TilePos pos;
  std::string spriteType;  // ancient_tablet | old_scroll | crystal_shard | carved_stone | torn_journal | rune_pillar
  bool hidden = false;     // metadata only
};

struct ABYSS_API LoreTables {
  std::vector<LoreEntryDef> entries;  // zone order then entry order
  double pickupRangeSq = 4;
  const LoreEntryDef* Find(std::string_view id) const;
  std::vector<const LoreEntryDef*> ForZone(std::string_view zoneId) const;
};

}  // namespace abyss
