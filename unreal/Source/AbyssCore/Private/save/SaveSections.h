// Internal per-section JSON readers / writers of SaveData, split by owning area so each area edits only its own file:
//   SaveHero.cpp    (hero+combat)        player, settings, soulEcho
//   SaveItems.cpp   (items)              inventory, equipment, stash, ItemInstance, itemUidCounter, potionSlots
//   SaveQuests.cpp  (quests+story+pets)  quests, dialogueState, dialogueOnce, achievements, storySeen, lore lists
//   SavePets.cpp    (quests+story+pets)  pets, homestead
//   SaveIO.cpp      (world)              top level: key order, migrations, version gate, rng, flow fields
// Readers are lenient (missing / wrong-typed fields keep defaults, save-ui-input 3.3) and never fail the whole load.
// Exported (ABYSS_API) because the area tests call the section readers / writers directly.
#pragma once

#include "abyss/base/Json.h"
#include "abyss/base/Platform.h"
#include "abyss/save/SaveData.h"

namespace abyss::savejson {

ABYSS_API void WriteHero(JsonWriter& w, const SaveHero& h);
ABYSS_API void ReadHero(const JsonValue& v, SaveHero& out);
ABYSS_API void WriteSettings(JsonWriter& w, const SaveSettings& s);
ABYSS_API void ReadSettings(const JsonValue& v, SaveSettings& out);
ABYSS_API void WriteSoulEcho(JsonWriter& w, const SaveSoulEcho& e);
ABYSS_API void ReadSoulEcho(const JsonValue& v, SaveSoulEcho& out);

ABYSS_API void WriteEquipment(JsonWriter& w, const SaveData& s);
ABYSS_API void ReadEquipment(const JsonValue& v, SaveData& out);
ABYSS_API void WriteItemList(JsonWriter& w, const std::vector<ItemInstance>& items);
ABYSS_API void ReadItemList(const JsonValue& v, std::vector<ItemInstance>& out);
ABYSS_API void WritePotionSlots(JsonWriter& w, const std::array<std::string, 2>& slots);  // v4 potionSlots (I4)
ABYSS_API void ReadPotionSlots(const JsonValue& v, std::array<std::string, 2>& out);
ABYSS_API void WriteItemUidCounter(JsonWriter& w, uint64_t counter);  // v4 itemUidCounter
ABYSS_API uint64_t ReadItemUidCounter(const JsonValue& v);            // missing / invalid -> 1

ABYSS_API void WriteQuests(JsonWriter& w, const std::vector<QuestProgress>& quests);
ABYSS_API void ReadQuests(const JsonValue& v, std::vector<QuestProgress>& out);
ABYSS_API void WriteDialogueState(JsonWriter& w, const std::vector<DialogueNpcState>& states);
ABYSS_API void ReadDialogueState(const JsonValue& v, std::vector<DialogueNpcState>& out);
ABYSS_API void WriteAchievements(JsonWriter& w, const std::vector<std::pair<std::string, int64_t>>& entries);
ABYSS_API void ReadAchievements(const JsonValue& v, std::vector<std::pair<std::string, int64_t>>& out);

ABYSS_API void WritePets(JsonWriter& w, const SavePets& p);
ABYSS_API void ReadPets(const JsonValue& v, SavePets& out);
ABYSS_API void WriteHomestead(JsonWriter& w, const SaveHomestead& h);
ABYSS_API void ReadHomestead(const JsonValue& v, SaveHomestead& out);

// Shared helpers.
ABYSS_API void WriteStringList(JsonWriter& w, const std::vector<std::string>& v);
ABYSS_API void ReadStringList(const JsonValue& v, std::vector<std::string>& out);

}  // namespace abyss::savejson
