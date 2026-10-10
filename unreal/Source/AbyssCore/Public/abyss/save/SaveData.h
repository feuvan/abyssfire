// SaveData v4: plain structs mirroring the web v3 schema (field names identical) plus the port's v4 fields.
// Spec: save-ui-input.md 3.2 (schema and field order), 3.3 (migrations + load normalisations), 3.4 (build rules: only a
// living hero is serialised; labyrinth position override), 3.5 (restore order, FIX Q8 / Q35), 3.9 (example),
// 3.10 (port design); DECISIONS U2 (v4 adds hotbar, potionSlots, playTimeMs, rng, dialogueOnce), I8 (ItemInstance
// field names), C3 (hotbar), I4 (potion slots), Q1 (dialogueOnce), Q2 (visitedZones), Q5 (hidden rewards persist),
// S3 (rng states).
//
// Owner area: world (flow) owns the struct layout and SaveIO; each area fills / reads its own section through its
// system's WriteSave / ReadSave (GameSim::SaveGame / LoadGame call them in the save 3.5 restore order).
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Json.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Rng.h"
#include "abyss/base/Stats.h"
#include "abyss/hero/Skills.h"
#include "abyss/hero/Spirit.h"
#include "abyss/items/Item.h"
#include "abyss/pets/Homestead.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/quests/Dialogue.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/sim/Session.h"

namespace abyss {

inline constexpr int32_t kCurrentSaveVersion = 4;

// SaveData.player (3.2).
struct SaveHero {
  int32_t level = 1;
  int64_t exp = 0;
  int64_t gold = 0;
  double hp = 0, mana = 0;
  double maxHp = 0, maxMana = 0;  // written for readability, ignored on load (re-derived)
  PrimaryStats stats;
  int32_t freeStatPoints = 0;
  int32_t freeSkillPoints = 0;
  std::vector<std::pair<std::string, int32_t>> skillLevels;  // record (legacy entry arrays accepted on parse)
  bool hasSpirit = false;
  SpiritSaveState spirit;
  double tileCol = 0, tileRow = 0;
  std::string currentMap;
};

struct SaveSettings {
  bool autoCombat = false;
  double musicVolume = 0.5;  // constant in the web (Q5), kept for compatibility
  double sfxVolume = 0.7;
  AutoLootMode autoLootMode = AutoLootMode::Off;
};

struct SaveHomestead {
  std::vector<std::pair<std::string, int32_t>> buildings;
  int32_t embers = 0;
  GardenState garden;
  std::optional<ExpeditionState> expedition;
  std::optional<BlessingState> blessing;
  std::optional<TowerReturn> towerReturn;
  // legacy (read only by migratePetSave): homestead.pets / homestead.activePet
  bool hasLegacyPets = false;
  JsonValue legacyPets;
  bool hasLegacyActivePet = false;
  std::string legacyActivePet;
};

struct SavePets {
  bool present = false;
  bool hasOwned = false;  // `owned` was an array (migratePetSave falls back to homestead.pets when it is missing)
  std::vector<PetInstance> owned;
  std::string active;  // "" = null
};

struct SaveSoulEcho {
  bool present = false;
  std::string mapId;
  double col = 0, row = 0;
  int64_t gold = 0, exp = 0;
};

// v4: one state per RngStream, in RngStream order.
struct SaveRng {
  bool present = false;
  std::array<RngState, kRngStreamCount> streams{};
};

struct SaveData {
  // ---- v3 fields, in the web's write order ----
  std::string id = "autosave";
  int32_t version = kCurrentSaveVersion;
  int64_t timestamp = 0;  // Unix ms (UE supplies it)
  ClassId classId = ClassId::Warrior;
  SaveHero player;
  std::vector<ItemInstance> inventory;
  std::array<std::optional<ItemInstance>, EnumCount<EquipSlot>()> equipment{};
  std::vector<ItemInstance> stash;
  std::vector<QuestProgress> quests;
  JsonValue exploration = JsonValue::Object();  // always {} (dead data, Q6); kept verbatim
  SaveHomestead homestead;
  SavePets pets;
  std::vector<std::pair<std::string, int64_t>> achievements;
  SaveSettings settings;
  Difficulty difficulty = Difficulty::Normal;
  std::vector<Difficulty> completedDifficulties;
  bool hasMercenary = false;
  JsonValue mercenary;  // later milestone: kept verbatim
  std::vector<DialogueNpcState> dialogueState;
  std::vector<std::string> miniBossDialogueSeen;
  std::vector<std::string> loreCollected;
  std::vector<std::string> discoveredHiddenAreas;
  bool hasStorySeen = false;  // missing -> ['prologue'] (veterans skip the prologue)
  std::vector<std::string> storySeen;
  SaveSoulEcho soulEcho;
  AbyssRecord abyss;
  // ---- v4 (U2) ----
  int32_t slot = 0;
  bool hasHotbar = false;
  std::array<std::string, SkillBook::kHotbarSlots> hotbar{};  // skill ids, "" = empty (C3)
  std::array<std::string, 2> potionSlots{};                   // I4: bound base ids (HP, MP); "" = best available
  double playTimeMs = 0;
  SaveRng rng;
  std::vector<std::string> dialogueOnce;           // Q1 keys "<npcId>|<nodeId>|<choiceIndex>"
  std::vector<std::string> hiddenRewardsClaimed;   // Q5 "<areaId>#<rewardIndex>"
  uint64_t itemUidCounter = 1;
  // Q2: zones entered at least once, first-visit order. Restores SessionState::visitedZones (ZoneEnteredMsg.firstVisit)
  // and AchievementState's distinct-zone set (ach_explore_all) so a reload never counts a zone twice.
  std::vector<std::string> visitedZones;
};

}  // namespace abyss
