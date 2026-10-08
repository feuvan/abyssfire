// NPC definitions. Source: npcs.json (object key order = quest-giver tie-break). Spec: quests-story-ch1.md 1.6, 6.
// Placement is map data (MapDef camps / fieldNpcs), not NPC data.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"

namespace abyss {

enum class NpcType : uint8_t { Blacksmith, Merchant, Quest, Stash };
ABYSS_ENUM_STRINGS(NpcType, "blacksmith", "merchant", "quest", "stash")

struct NpcDef {
  std::string id;
  std::string name;  // zh-CN fallback (i18n nameKey = data.npc.<id>.name)
  NpcType type = NpcType::Quest;
  std::vector<std::string> dialogue;   // linear lines; [0] logged on every interaction (data.npc.<id>.dialogue.<n>)
  std::vector<std::string> shopItems;  // shop wares (merchant / blacksmith)
  std::vector<std::string> quests;     // quests this NPC gives and receives (quest-card order)
  std::string spriteId;                // borrowed look (tower allies); empty = own
  std::string dialogueTreeId;          // empty = no tree
  std::string nameKey;
};

struct ABYSS_API NpcTables {
  std::vector<NpcDef> npcs;  // key order
  const NpcDef* Find(std::string_view id) const;
  // The NPC that lists `questId` (first in key order), or nullptr.
  const NpcDef* GiverOf(std::string_view questId) const;
};

}  // namespace abyss
