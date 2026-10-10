// Internal per-section JSON readers / writers of SaveData, split by owning area so each area edits only its own file:
//   SaveHero.cpp    (hero+combat)        player, settings, soulEcho
//   SaveItems.cpp   (items)              inventory, equipment, stash, ItemInstance, itemUidCounter, potionSlots
//   SaveQuests.cpp  (quests+story+pets)  quests, dialogueState, dialogueOnce, achievements, storySeen, lore lists
//   SavePets.cpp    (quests+story+pets)  pets, homestead
//   SaveIO.cpp      (world)              top level: key order, migrations, version gate, rng, flow fields
// Readers are lenient (missing / wrong-typed fields keep defaults, save-ui-input 3.3) and never fail the whole load.
#pragma once

#include "abyss/base/Json.h"
#include "abyss/save/SaveData.h"

namespace abyss::savejson {

void WriteHero(JsonWriter& w, const SaveHero& h);
void ReadHero(const JsonValue& v, SaveHero& out);
void WriteSettings(JsonWriter& w, const SaveSettings& s);
void ReadSettings(const JsonValue& v, SaveSettings& out);
void WriteSoulEcho(JsonWriter& w, const SaveSoulEcho& e);
void ReadSoulEcho(const JsonValue& v, SaveSoulEcho& out);

void WriteEquipment(JsonWriter& w, const SaveData& s);
void ReadEquipment(const JsonValue& v, SaveData& out);
void WriteItemList(JsonWriter& w, const std::vector<ItemInstance>& items);
void ReadItemList(const JsonValue& v, std::vector<ItemInstance>& out);
void WritePotionSlots(JsonWriter& w, const std::array<std::string, 2>& slots);  // v4 potionSlots (I4)
void ReadPotionSlots(const JsonValue& v, std::array<std::string, 2>& out);
void WriteItemUidCounter(JsonWriter& w, uint64_t counter);  // v4 itemUidCounter
uint64_t ReadItemUidCounter(const JsonValue& v);            // missing / invalid -> 1

void WriteQuests(JsonWriter& w, const std::vector<QuestProgress>& quests);
void ReadQuests(const JsonValue& v, std::vector<QuestProgress>& out);
void WriteDialogueState(JsonWriter& w, const std::vector<DialogueNpcState>& states);
void ReadDialogueState(const JsonValue& v, std::vector<DialogueNpcState>& out);
void WriteAchievements(JsonWriter& w, const std::vector<std::pair<std::string, int64_t>>& entries);
void ReadAchievements(const JsonValue& v, std::vector<std::pair<std::string, int64_t>>& out);

void WritePets(JsonWriter& w, const SavePets& p);
void ReadPets(const JsonValue& v, SavePets& out);
void WriteHomestead(JsonWriter& w, const SaveHomestead& h);
void ReadHomestead(const JsonValue& v, SaveHomestead& out);

// Shared helpers.
void WriteStringList(JsonWriter& w, const std::vector<std::string>& v);
void ReadStringList(const JsonValue& v, std::vector<std::string>& out);

}  // namespace abyss::savejson
