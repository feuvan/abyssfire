// Ley-beasts (pets) and homestead tables.
// Sources: pets.json, homestead.json. Spec: quests-story-ch1.md 4.4-4.7, 18.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Stats.h"

namespace abyss {

enum class PetRole : uint8_t { Support, Scout, Melee, Assassin, Tank, Ranged, Caster };
ABYSS_ENUM_STRINGS(PetRole, "support", "scout", "melee", "assassin", "tank", "ranged", "caster")

enum class PetRarity : uint8_t { Common, Rare, Epic };
ABYSS_ENUM_STRINGS(PetRarity, "common", "rare", "epic")

enum class PetAbilityKind : uint8_t { Heal, Shield, Mark, Strike, Bolt, Cone, Nova, Taunt, Buff, Revive };
ABYSS_ENUM_STRINGS(PetAbilityKind, "heal", "shield", "mark", "strike", "bolt", "cone", "nova", "taunt", "buff",
                   "revive")

enum class PetCombatStyle : uint8_t { Melee, Ranged };
ABYSS_ENUM_STRINGS(PetCombatStyle, "melee", "ranged")

struct PetAbilityDef {
  std::string id;
  PetAbilityKind kind = PetAbilityKind::Heal;
  int32_t unlock = 0;  // 0 = from start, 1 = awakened (evolution 1)
  double cooldownMs = 0;
  double range = 0;  // tiles pet -> target; 0 = no target
  bool hasDamage = false;
  double damage = 0;  // x pet attack
  bool hasValue = false;
  double value = 0;  // heal fraction / DR / amplify / revive HP
  bool hasDuration = false;
  double durationMs = 0;
  bool hasRadius = false;
  double radius = 0;
  bool hasArc = false;
  double arc = 0;  // radians
  bool hasElement = false;
  DamageType element = DamageType::Physical;
  bool crit = false;
  bool leap = false;
  bool hasBleed = false;
  double bleed = 0;
  bool hasBurn = false;
  double burn = 0;
  bool hasSlow = false;
  double slow = 0;  // percent
  bool hasStun = false;
  double stunMs = 0;
  int32_t hits = 1;
  bool hasMana = false;
  double mana = 0;
  bool self = false;
};

struct PetDef {
  std::string id;
  int32_t chapter = 1;
  PetRole role = PetRole::Support;
  PetRarity rarity = PetRarity::Common;
  bool flying = false;
  AnimRig animCategory = AnimRig::Flying;
  PetCombatStyle style = PetCombatStyle::Ranged;
  double range = 0;
  double attackMs = 0;
  uint32_t color = 0;
  DamageType element = DamageType::Physical;
  Stat passiveStat = Stat::ExpBonus;
  double passiveBase = 0, passivePerLevel = 0;
  double hpFraction = 0;
  std::vector<PetAbilityDef> abilities;  // array order = AI priority
  std::string primaryAbilityId;
  std::string nameKey, descKey, originKey;
};

struct PetSystemConstants {
  double damageBaseFraction = 0.05, damagePerLevelFraction = 0.005, damageMaxFraction = 0.15;
  int32_t bondProgressPerLevel = 100;
  int32_t bondPerKill = 1, bondPerActiveMinute = 2, bondPerFeed = 20;
  double feedExp = 120;
  double bondRescueHp = 0.3;
  double bondRescueCooldownMs = 60000;
  int32_t baseBondCap = 3;
  double expToNextBase = 60, expToNextPerLevel = 40;
  double killExpBase = 10, killExpPerMonsterLevel = 1;
  double bondMultiplierPerBond = 0.1;
  double leyFruitDropElite = 0.12, leyFruitDropOther = 0.015;
};

struct PetCompanionConstants {
  double exhaustMs = 5000;
  double straySwingChance = 0.25;
  double followSpeedTilesPerSec = 4.2;
  double dashSpeedTilesPerSec = 8.5;
  double teleportDistanceTiles = 16;
};

struct ABYSS_API PetTables {
  std::vector<PetDef> pets;
  int32_t maxLevel = 20;
  std::vector<int32_t> evolutionLevels;  // [10, 20]
  std::vector<double> evolutionMult;     // [1, 1.5, 2]
  int32_t maxBond = 5;
  std::string leyFruitId = "c_ley_fruit";
  PetSystemConstants system;
  PetCompanionConstants companion;
  std::vector<std::string> chapter1Slice;  // Q3: pets shipped in milestone 1

  const PetDef* Find(std::string_view id) const;
};

// homestead.json
struct BuildingLevelCost {
  int64_t gold = 0;
  int32_t embers = 0;
};

struct BuildingDef {
  std::string id;
  std::string name, description;
  int32_t maxLevel = 0;
  std::vector<BuildingLevelCost> costPerLevel;
  std::vector<StatValue> bonusPerLevel;
  std::string unlockQuest;  // empty = always open (warehouse)
  std::string allyNpc;
};

struct ExpeditionOptionDef {
  std::string id;
  int32_t killsRequired = 0;
  double durationMs = 0;
};

struct BlessingDef {
  std::string id;
  StatBag stats;
  std::string glyph;
};

struct ABYSS_API HomesteadTables {
  std::string towerZoneId = "ember_tower";
  std::string towerUnlockQuest;
  std::string leyFruitId = "c_ley_fruit";
  std::vector<BuildingDef> buildings;
  int32_t gemCombineCount = 3;
  std::vector<ExpeditionOptionDef> expeditionOptions;
  std::vector<BlessingDef> blessings;
  double blessingDurationMs = 1800000;
  int32_t embersKillElite = 5, embersKillMiniBoss = 3, embersKillAffixedPerAffix = 1, embersKillPlain = 0;
  int32_t embersQuestMain = 2, embersQuestSide = 1;
  std::vector<int32_t> gardenIntervalByLevel;
  std::vector<int32_t> gardenCapacityByLevel;
  double gardenLeyFruitBase = 0.12, gardenLeyFruitPerLevel = 0.03, gardenHpShare = 0.6;
  bool towerHiddenInMilestone1 = true;  // Q3

  const BuildingDef* FindBuilding(std::string_view id) const;
};

}  // namespace abyss
