// Monsters, mini-bosses, quest hunts, monster AI constants.
// Sources: monsters.json, minibosses.json, quest_hunts.json, monster_ai.json. Spec: monsters-ai.md 1, 8, 9, 17.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/data/DialogueData.h"
#include "abyss/data/SkillData.h"

namespace abyss {

enum class MonsterSource : uint8_t { Zone, Dungeon, MiniBoss, SubDungeonBoss, Hunt };
ABYSS_ENUM_STRINGS(MonsterSource, "zone", "dungeon", "miniBoss", "subDungeonBoss", "hunt")

// MonsterDefinition verbatim + exporter `derived` block (monsters-ai.md 1.1). This is also the runtime "current
// definition" type: difficulty scaling, hunt transforms and elite affixes produce modified copies (1.3).
struct MonsterDef {
  std::string id;
  std::string name;  // zh-CN fallback; labels use nameKey (M8)
  int32_t level = 1;
  double hp = 0;
  double damage = 0;
  double defense = 0;
  double speed = 0;        // tiles/s = speed * 0.03
  double aggroRange = 0;   // tiles
  double attackRange = 0;  // tiles; ranged iff > 2.5 (derived.isRanged)
  double attackSpeedMs = 0;  // JSON "attackSpeed": ms between swing starts
  double expReward = 0;
  double goldMin = 0, goldMax = 0;  // JSON goldReward [min, max]
  std::string spriteKey;
  bool elite = false;
  bool isMiniBoss = false;
  bool isSubDungeonMiniBoss = false;
  AnimRig animCategory = AnimRig::Humanoid;
  // derived (C6, M5)
  bool isRanged = false;
  std::vector<StatusRule> onHitStatus;  // value kind MonsterDamage = fraction of the spawn-scaled damage
  uint32_t projectileColor = 0xcc44cc;
  std::string homeZone;
  std::string nameKey;  // i18n data.monster.<id>
  std::vector<MonsterSource> sources;
};

struct ZoneMonsterList {
  std::string zoneId;
  std::vector<std::string> monsterIds;
};

struct MiniBossEntry {
  std::string zoneId;
  std::string monsterId;
  bool hasSpawn = false;
  TilePos spawn;
};

// One quest hunt (monsters-ai.md 9.1) with its precomputed normal-difficulty definition.
struct HuntDef {
  std::string questId;
  std::string zone;
  std::string huntId;
  std::string monsterId;  // base monster
  std::string name;       // zh-CN fallback (i18n data.monster.<huntId>)
  TilePos spawn;
  bool hasHpMul = false;
  double hpMul = 4;
  bool hasDmgMul = false;
  double dmgMul = 1.5;
  bool revealAfterPrevious = false;
  bool hasMinions = false;
  std::string minionMonsterId;
  int32_t minionCount = 0;
  MonsterDef defNormal;  // makeHuntDefinition result before difficulty scaling / affixes
};

// monster_ai.json (monsters-ai.md 17 + port decisions).
enum class LeashMode : uint8_t { WebParity, Returning };
ABYSS_ENUM_STRINGS(LeashMode, "webParity", "returning")

struct MonsterAiDef {
  double leashRange = 8;
  double leashHealFractionPerTick = 0.01;
  double patrolIntervalMs = 3000;
  int32_t patrolRadius = 2;
  double arriveEpsilon = 0.1;
  double chaseDropMul = 1.5;
  double attackExitMul = 1.2;
  double moveSpeedScale = 0.03;
  double moveAccel = 6;
  double rangedThreshold = 2.5;
  double meleeReachMul = 1.35, meleeReachAdd = 0.5;
  double swingQueryRadius = 12;
  double aiCullRadius = 30;
  double activeRefreshMs = 250;
  int32_t spawnJitter = 3;
  double respawnDelayMs = 15000;
  int32_t respawnJitter = 2;
  double safeZoneRadiusDefault = 9;
  // monster combat stats (monsters 1.2)
  double strPerDamage = 0.8, dexPerSpeed = 0.1, vitPerHp = 0.1;
  int32_t statInt = 3, statSpi = 3, statLck = 3;
  // hunts
  double huntHpMul = 4, huntDmgMul = 1.5, huntDefMul = 1.2, huntExpMulMin = 3, huntGoldMul = 3, huntAggroMin = 7;
  int32_t huntMinionJitter = 3;
  int32_t huntSpotSearchRadius = 6;
  double huntVisualScale = 1.25;
  double huntRevealShakeMs = 260, huntRevealShakeIntensity = 0.004;
  // boss intro / bar
  double bossSightRange = 9, bossBarRange = 14;
  std::string finalBoss = "demon_lord";
  // escort / defend chip damage
  double escortChipRadiusSq = 16, escortChipIntervalMs = 2000, escortChipDamageMul = 0.3;
  double defendChipRadiusSq = 9, defendChipIntervalMs = 2000, defendChipDamageMul = 0.2;
  // port decisions
  LeashMode leashMode = LeashMode::Returning;      // M1
  double leashHealFractionPerSecond = 0.6;         // M1
  double provokeDurationMs = 5000;                 // M2
  std::vector<std::string> noRespawnRoles;         // M4 role names (MonsterRole spellings, incl. storyBoss)
  bool miniBossOncePerVisit = true;                // M7
  // M7 story boss (monster_ai.json port.storyBoss): a regular zone spawn of `storyBossId` gets MonsterRole::StoryBoss
  // in SpawnZonePopulation: it spawns at most once per zone visit (never respawns within the visit) and not at all once
  // `storyBossNotAfterQuestTurnIn` is turned in, unless the chapter is complete (`chapterCompleteQuest` turned in) and
  // storyBossFarmableAfterChapter, in which case it spawns once per visit again.
  std::string storyBossId;                         // "goblin_chief"
  bool storyBossOncePerVisit = true;
  std::string storyBossNotAfterQuestTurnIn;        // "q_find_goblin_chief"
  bool storyBossFarmableAfterChapter = true;
  std::string chapterCompleteQuest;                // "q_secure_plains" (its turn-in plays the chapter finale)
  double patrolTimeoutMs = 4000;                   // M10
  int32_t placementTries = 8;                      // M10
  double yawTurnRateDegPerSec = 720;               // M11 (render)
  // M6 steering constants. monster_ai.json only names the decision (port.pathfinding text), so these port-only tuning
  // values live here (not loaded): A* repath interval, the line-of-walk sampling step, the wall-slide rule and the
  // separation steering (neighbours closer than separationRadius push apart at up to separationMaxSpeed tiles/s;
  // attacking monsters are never pushed out of their attack range).
  double repathIntervalMs = 500;
  double lineOfWalkStep = 0.25;
  double separationRadius = 0.75;
  double separationMaxSpeed = 1.0;
  // Returning (M1) ends on arrival (< arriveEpsilon) or once within this many tiles of the anchor (monsters 3.5 FIX).
  double returnHomeRadius = 1;
  // Hero position while hidden by a safe zone (monsters 3.8: update(..., -999, -999)).
  double hiddenHeroCoord = -999;
};

struct MonsterOverride {
  std::string id;
  std::string field;
  double web = 0, port = 0;
  std::string decision;
};

struct DungeonMonsterTable {
  std::vector<std::string> exclusive;
  std::string boss;
  std::string midBoss;
  std::vector<std::string> pool;
};

struct ABYSS_API MonsterTables {
  std::vector<MonsterDef> defs;  // every definition (zone, dungeon, mini-boss, sub-dungeon boss), data order
  std::vector<std::string> lookupOrder;
  std::vector<ZoneMonsterList> byZone;
  DungeonMonsterTable dungeon;
  std::vector<MonsterOverride> overrides;
  std::vector<MiniBossEntry> miniBosses;
  std::vector<std::string> subDungeonBosses;
  std::vector<DialogueTree> miniBossDialogues;  // tree id = monster id
  std::vector<HuntDef> hunts;
  MonsterAiDef ai;
  IdIndex defIndex;

  // Any definition by id (zone, dungeon, mini-boss, sub-dungeon boss); nullptr when unknown.
  const MonsterDef* Find(std::string_view id) const;
  // Zone list lookup first, then any definition (ZoneScene: defs.find(id) ?? getMonsterDef(id)).
  const MonsterDef* FindForZone(std::string_view zoneId, std::string_view id) const;
  const ZoneMonsterList* ZoneList(std::string_view zoneId) const;
  const MiniBossEntry* MiniBossFor(std::string_view zoneId) const;
  const DialogueTree* MiniBossDialogue(std::string_view monsterId) const;
  const HuntDef* FindHunt(std::string_view huntId) const;
};

}  // namespace abyss
