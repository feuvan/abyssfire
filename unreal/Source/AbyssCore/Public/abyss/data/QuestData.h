// Quests, quest tuning constants, achievements.
// Sources: quests.json (all chapters, array order), quest_tuning.json, achievements.json.
// Spec: quests-story-ch1.md 1.1-1.5, 1.10, 2-4, 9, 14.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Stats.h"
#include "abyss/base/Types.h"

namespace abyss {

enum class QuestType : uint8_t { Kill, Collect, Explore, Talk, Escort, Defend, Investigate, Craft };
ABYSS_ENUM_STRINGS(QuestType, "kill", "collect", "explore", "talk", "escort", "defend", "investigate", "craft")

enum class QuestCategory : uint8_t { Main, Side };
ABYSS_ENUM_STRINGS(QuestCategory, "main", "side")

enum class ObjectiveType : uint8_t {
  Kill,
  Collect,
  Explore,
  Talk,
  Escort,
  DefendWave,
  InvestigateClue,
  CraftCollect,
  CraftCraft,
  CraftDeliver,
};
ABYSS_ENUM_STRINGS(ObjectiveType, "kill", "collect", "explore", "talk", "escort", "defend_wave", "investigate_clue",
                   "craft_collect", "craft_craft", "craft_deliver")

enum class ItemSourceKind : uint8_t { None, Drop, Gather };

// Reward choice slots (quests 1.4).
enum class RewardSlot : uint8_t { Weapon, Armor, Helmet, Gloves, Boots, Belt, Jewelry, Offhand };
ABYSS_ENUM_STRINGS(RewardSlot, "weapon", "armor", "helmet", "gloves", "boots", "belt", "jewelry", "offhand")

struct QuestObjectiveDef {
  ObjectiveType type = ObjectiveType::Kill;
  std::string targetId;
  std::string targetName;  // zh-CN fallback (i18n data.questTarget.<targetId>)
  int32_t required = 1;
  bool hasLocation = false;
  TileCircle location;
  ItemSourceKind sourceKind = ItemSourceKind::None;
  std::vector<std::string> dropMonsters;  // drop source
  double dropChance = 0;                  // drop source
  TileCircle gatherArea;                  // gather source
  int32_t gatherCount = 0;                // gather source
  std::string itemKind;                   // icon / node look (derived default by target, else "relic")
  std::string labelKey;                   // overrides the label when set
};

struct QuestRewardDef {
  int64_t exp = 0;
  int64_t gold = 0;
  std::vector<std::string> items;  // item base ids (duplicates = 2 items)
  std::string petReward;           // pet id or empty
  bool hasEmbers = false;
  int32_t embers = 0;
  std::vector<RewardSlot> choices;
  bool hasChoiceQuality = false;
  ItemQuality choiceQuality = ItemQuality::Rare;
};

struct EscortNpcDef {
  std::string name;  // zh-CN fallback (i18n data.escortNpc.<questId>)
  std::string spriteKey;
  TilePos start;
  TilePos dest;
};

struct DefendTargetDef {
  std::string name;
  std::string spriteKey;
  TilePos pos;
  int32_t totalWaves = 0;
};

struct QuestClueDef {
  std::string id;
  std::string name;
  TilePos pos;
};

struct CraftMaterialReq {
  std::string itemId;
  std::string name;
  int32_t required = 0;
};

struct CraftPhasesDef {
  bool present = false;
  std::vector<CraftMaterialReq> materials;  // unused at runtime
  std::string craftNpc;
  std::string deliverNpc;
};

struct QuestHuntRef {
  std::string huntId;  // full data in MonsterTables::hunts (quest_hunts.json)
};

struct QuestDef {
  std::string id;
  std::string name, description;  // zh-CN fallbacks
  std::string zone;
  QuestType type = QuestType::Kill;
  QuestCategory category = QuestCategory::Side;
  std::vector<QuestObjectiveDef> objectives;
  QuestRewardDef rewards;
  std::vector<std::string> prereqQuests;
  int32_t level = 1;
  bool hasQuestArea = false;
  TileCircle questArea;
  bool hasEscortNpc = false;
  EscortNpcDef escortNpc;
  bool hasDefendTarget = false;
  DefendTargetDef defendTarget;
  std::vector<QuestClueDef> clues;  // map-generation landmarks only
  CraftPhasesDef craftPhases;
  bool reacceptable = false;
  std::vector<QuestHuntRef> hunts;
  // derived
  std::string giverNpcId;
  int32_t embersOnTurnIn = 0;
  std::string nameKey, descKey, offerKey, completeKey;
  int32_t orderIndex = 0;  // AllQuests order
};

struct QuestTuningDef {
  int32_t levelGateAbove = 5;
  double gatherRange = 1.3;
  double clueRange = 2;
  double guideNear = 2.5;
  double fallbackCollectChance = 0.25;
  std::vector<std::pair<std::string, std::string>> itemKindByTarget;
  std::string defaultItemKind = "relic";
  // escort
  double escortHpBase = 100, escortHpPerLevel = 20, escortSpeedDivisor = 38, escortSpeedFactor = 0.9;
  double escortCatchUpTiles = 14, escortArriveEscortSq = 25, escortArriveHeroSq = 36, escortThreatRangeSq = 16;
  double escortHitIntervalMs = 2000, escortDmgMul = 0.3;
  // defend
  double defendHpBase = 200, defendHpPerLevel = 30, defendStartRangeSq = 225, defendWaveDelayMs = 5000;
  double defendHitRangeSq = 9, defendHitIntervalMs = 2000, defendDmgMul = 0.2;
  // embers
  int32_t embersQuestMain = 2, embersQuestSide = 1;
};

enum class AchievementType : uint8_t { Kill, Collect, Explore, Level, Quest };
ABYSS_ENUM_STRINGS(AchievementType, "kill", "collect", "explore", "level", "quest")

struct AchievementDef {
  std::string id;
  std::string name, description, title;  // zh-CN fallbacks (i18n data.achievement.<id>.*)
  AchievementType type = AchievementType::Kill;
  std::string targetId;  // empty = any
  int32_t required = 1;
  bool hasReward = false;
  Stat rewardStat = Stat::Damage;
  double rewardValue = 0;
};

struct ABYSS_API QuestTables {
  std::vector<QuestDef> quests;  // AllQuests order
  QuestTuningDef tuning;
  std::vector<AchievementDef> achievements;
  bool achievementsCountKillOnce = true;    // Q2
  bool achievementsExploreDistinct = true;  // Q2
  IdIndex questIndex;

  const QuestDef* Find(std::string_view id) const;
  int32_t Index(std::string_view id) const { return questIndex.Find(id); }
  const AchievementDef* FindAchievement(std::string_view id) const;
  std::string_view ItemKindFor(std::string_view targetId) const;
};

}  // namespace abyss
