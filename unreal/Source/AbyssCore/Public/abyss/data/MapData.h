// Zones (authored map anchors + generator inputs), map-generation themes, world constants, random events, zone moods.
// Sources: maps.json, map_gen.json, world_constants.json, random_events.json, zone_moods.json.
// Spec: world-map-nav.md 2-4, 9-14, 19. The generated tile grids are produced at runtime by world/MapGen (W10);
// only the hand-built ember_tower ships its tiles/collisions/decorations in the data.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"

namespace abyss {

// Largest zone / sub-dungeon side accepted by DataStore::Finalize (the web's maps are <= 120; ZoneGrid, the exploration
// grid and the minimap texture are allocated from cols x rows).
inline constexpr int32_t kMaxMapDim = 512;

struct MapSpawnDef {
  TilePos pos;
  std::string monsterId;
  int32_t count = 1;
};

struct MapCampDef {
  TilePos pos;
  std::vector<std::string> npcs;  // slot order
};

struct MapExitDef {
  TilePos pos;
  std::string targetMap;
  TilePos target;
};

struct FieldNpcDef {
  TilePos pos;
  std::string npcId;
};

struct PetSpawnDef {
  TilePos pos;
  std::string petId;
  double chance = 0;
};

enum class HiddenRewardType : uint8_t { Chest, GoldPile, RareSpawn, Lore };
ABYSS_ENUM_STRINGS(HiddenRewardType, "chest", "gold_pile", "rare_spawn", "lore")

struct HiddenRewardDef {
  HiddenRewardType type = HiddenRewardType::Chest;
  std::string value;  // chest: quality ("rare"/"legendary"/else magic); gold_pile: amount text
  TilePos pos;
};

struct HiddenAreaDef {
  std::string id;
  std::string name;
  TilePos center;
  double radius = 0;
  bool hasBounds = false;  // explicit startCol/startRow/endCol/endRow
  TilePos boundsStart, boundsEnd;
  std::string discoveryText;  // zh-CN fallback (i18n data.hiddenArea.<id>.discovery)
  std::vector<HiddenRewardDef> rewards;
};

struct SubDungeonEntranceDef {
  std::string id;
  std::string name;
  TilePos pos;
  std::string targetSubDungeon;
};

struct StoryDecorationDef {
  std::string id;
  std::string name, description;
  TilePos pos;
  std::string spriteType;
};

struct DecorationDef {
  TilePos pos;
  std::string type;
};

struct AvoidPointDef {
  TilePos pos;
  bool hasMargin = false;
  int32_t margin = 4;
};

// One zone's MapData (world 2.2) + exporter fields.
struct MapDef {
  std::string id;
  std::string name;  // zh-CN fallback (nameKey data.zone.<id>)
  std::string nameKey;
  int32_t cols = 0, rows = 0;
  bool hasTheme = false;
  MapTheme theme = MapTheme::Plains;
  int32_t seed = 42;
  std::string bgColor;
  std::vector<MapSpawnDef> spawns;
  std::vector<MapCampDef> camps;
  TilePos playerStart;
  std::vector<MapExitDef> exits;
  int32_t levelMin = 1, levelMax = 1;
  bool hasSafeZoneRadius = false;
  double safeZoneRadius = 9;
  double safeZoneRadiusEffective = 9;
  std::vector<PetSpawnDef> petSpawns;
  std::vector<FieldNpcDef> fieldNpcs;
  std::vector<HiddenAreaDef> hiddenAreas;
  std::vector<SubDungeonEntranceDef> subDungeonEntrances;
  std::vector<StoryDecorationDef> storyDecorations;
  // generator input (W10); generated == true means tiles are produced by world/MapGen at load
  bool generated = false;
  MapTheme generatorTheme = MapTheme::Plains;
  int32_t generatorSeed = 42;
  std::vector<AvoidPointDef> generatorAvoid;
  int32_t avoidMarginDefault = 4;
  // hand-built maps only (ember_tower): rows of digit strings
  std::vector<std::string> tiles;
  std::vector<std::string> collisions;  // '1' = walkable
  std::vector<DecorationDef> decorations;
  int32_t orderIndex = -1;  // index in MapOrder (world map), -1 when not listed
};

struct SubDungeonDef {
  std::string id;
  std::string name;
  std::string parentZone;
  int32_t cols = 0, rows = 0;
  MapTheme theme = MapTheme::Plains;
  int32_t seed = 0;
  std::vector<MapSpawnDef> spawns;
  TilePos miniBossPos;
  std::string miniBossId;
  TilePos playerStart;
  TilePos exitPos;
  TilePos returnPos;
  int32_t levelMin = 1, levelMax = 1;
  std::string bgColor;
};

struct EmberTowerPlotDef {
  std::string id;
  TilePos pos;
  TilePos npcPos;
};

struct EmberTowerLayoutDef {
  int32_t size = 48;
  TilePos center, portal, meadow;
  double meadowRadius = 0;
  std::vector<EmberTowerPlotDef> plots;
};

// map_gen.json (world 4.2).
struct WeightedDecor {
  std::string type;
  double weight = 0;
};

struct MapThemeDef {
  int32_t primaryTile = 0;
  int32_t secondaryTile = 1;
  double wallDensity = 0;
  int32_t lakeCountMin = 0, lakeCountMax = 0;
  int32_t lakeSizeMin = 0, lakeSizeMax = 0;
  std::vector<std::string> decorTypes;
  double decorDensity = 0;
  std::vector<WeightedDecor> grove;
  std::vector<WeightedDecor> open;
  std::vector<std::string> tall;
};

struct MapGenDef {
  // tile ids: grass 0, dirt 1, stone 2, water 3, wall 4, camp 5, campWall 6
  std::vector<int32_t> blockingTiles;
  std::array<MapThemeDef, EnumCount<MapTheme>()> themes{};
  MapTheme defaultTheme = MapTheme::Plains;
  int32_t defaultSeed = 42;
  int32_t defaultAvoidMargin = 4;
  int32_t landmarkLoreMargin = 2;
  int32_t landmarkMiniBossMargin = 5;
  int32_t landmarkQuestObjectiveLocation = 3, landmarkQuestGatherArea = 3, landmarkQuestArea = 4,
          landmarkQuestDefendTarget = 6, landmarkQuestEscortStart = 3, landmarkQuestEscortDest = 3,
          landmarkQuestClue = 2;
  int64_t rngModulus = 2147483647;
  int64_t rngMultiplier = 16807;

  const MapThemeDef& Theme(MapTheme t) const { return themes[static_cast<size_t>(t)]; }
};

// world_constants.json (world 19 + decisions S4/S5/W1/W3/W5/W7/W8).
struct WorldConstants {
  double tileWidthPx = 64, tileHeightPx = 32, portUnitsPerTile = 100;
  double cameraYawDeg = 45, cameraPitchDeg = -50, cameraFovDeg = 35;
  double cameraFramingTilesW = 16, cameraFramingTilesH = 12;
  double cameraZoomMin = 0.75, cameraZoomMax = 1.25;
  double cameraFollowLagSec = 0.12;
  double heroMoveSpeed = 120, heroPxPerTile = 36, heroRampMs = 90, heroStopMs = 60;
  bool heroClickStartsInstantly = true;
  double holdMoveRepathMs = 120;
  double exitRadiusSq = 2.25;
  double exitArmDistance = 2.449489742783178;  // sqrt(6) (W8)
  double exitArmDistanceSq = 6;                // W8 / W7: armed once distSq > 6 (strict)
  double campfireRadiusTiles = 5, campfireHpMul = 50, campfireMpMul = 50;
  double safeZoneRadiusDefault = 9;
  double hiddenAreaExploreRadius = 10;
  double escortCatchUpTiles = 14;
  double townPortalChannelMs = 1500;
  bool townPortalCancelOnMove = true;
  double townPortalCancelOnDamageFraction = 0.1;
  double deathRespawnMs = 1100;
  double zoneFadeInMs = 400;
  double lootRadiusSq = 4, hiddenChestRadiusSq = 4, puzzleRadiusSq = 4;
  double npcRange = 3, npcPickRadiusSq = 3.24, entranceRadiusSq = 9, touchTalkRange = 2.5;
  bool walkThenAct = true;
  std::string sealedGateExitTo = "twilight_forest";  // W7
  std::string sealedGateMessageKey;                  // W7 i18n key (zone.exit.sealedChapter2), checked by Finalize
};

// random_events.json (world 13).
enum class RandomEventType : uint8_t { Ambush, TreasureCache, WanderingMerchant, Rescue, EnvironmentalPuzzle };
ABYSS_ENUM_STRINGS(RandomEventType, "ambush", "treasure_cache", "wandering_merchant", "rescue",
                   "environmental_puzzle")

struct RandomEventTypeDef {
  RandomEventType type = RandomEventType::Ambush;
  double weight = 0;
  std::string name, message;  // zh-CN fallbacks (i18n sys.event.msg.<type>)
};

struct PuzzleDef {
  std::string prompt, solution, reward;  // zh-CN fallbacks (i18n sys.event.puzzle.<zone>.*)
  int64_t rewardGold = 0;
  int64_t rewardExp = 0;
};

struct ZoneEventDataDef {
  std::string zoneId;
  std::vector<std::string> ambushMonsters;
  int32_t ambushCountMin = 0, ambushCountMax = 0;
  std::vector<std::string> merchantItems;
  std::vector<PuzzleDef> puzzles;
  std::string puzzleSpriteKey;
  std::string rescueNpcName;
  std::string rescueNpcSpriteKey;
  int64_t rescueRewardGold = 0;
  int64_t rescueRewardExp = 0;
};

struct ABYSS_API RandomEventsDef {
  double cooldownMs = 30000;
  double safeZoneRadius = 9;
  int32_t minEventsPerWindow = 3, maxEventsPerWindow = 8;
  double frequencyWindowMs = 300000;
  double triggerMoveThresholdTiles = 3;
  std::vector<RandomEventTypeDef> types;  // weighted pick order
  std::vector<ZoneEventDataDef> zones;
  bool resetMoveCounterAfterEveryRoll = true;  // W6

  const ZoneEventDataDef* ForZone(std::string_view zoneId) const;
};

// zone_moods.json (render-only atmosphere; the core passes the ids through to UE).
struct ZoneMoodDef {
  MapTheme theme = MapTheme::Plains;
  uint32_t ambient = 0;
  double ambientAlpha = 0;
  uint32_t vignette = 0;
  double vignetteAlpha = 0;
  uint32_t haze = 0;
  double hazeAlpha = 0;
  double saturation = 1, contrast = 1;
  std::array<double, 3> lift{}, gain{};
};

struct ZoneWeatherDef {
  std::string zoneId;
  std::string type;      // none | snow | sand | ember_storm
  std::string ambience;  // pollen | wisps | dust_motes | sparks | ""
  bool hasTint = false;
  uint32_t tint = 0;
};

struct ABYSS_API ZoneMoodTables {
  std::vector<std::pair<std::string, MapTheme>> themeByZone;
  std::array<ZoneMoodDef, EnumCount<MapTheme>()> moods{};
  std::vector<ZoneWeatherDef> weather;

  bool ThemeFor(std::string_view zoneId, MapTheme& out) const;
  const ZoneWeatherDef* WeatherFor(std::string_view zoneId) const;
};

struct ABYSS_API WorldTables {
  std::vector<std::string> mapOrder;  // world-map order (5 story zones)
  std::string defaultMap = "emerald_plains";
  std::vector<MapDef> maps;
  std::vector<SubDungeonDef> subDungeons;
  EmberTowerLayoutDef emberTower;
  MapGenDef mapGen;
  WorldConstants constants;
  RandomEventsDef randomEvents;
  ZoneMoodTables moods;

  const MapDef* FindMap(std::string_view id) const;
  const SubDungeonDef* FindSubDungeon(std::string_view id) const;
};

}  // namespace abyss
