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

// ---- K2 monster abilities (dark remake, implementation-plan.md 3 K2; monsters.md 1.3 fairness) ----------------------
// A monster's `abilities[]` (remake schema `monster.abilities`). MonsterAbilities.h holds the rules (picker, shapes,
// timing); CombatSystem runs them through the monster swing path. Kinds whose runtime a later package lands (raise and
// summon: K3, buff and call: K5) are parsed and validated here but never picked until then (AbilityKindRuns).
enum class AbilityKind : uint8_t { Circle, Cone, Line, Ring, Bolt, Raise, Summon, Hazard, Buff, Call };
ABYSS_ENUM_STRINGS(AbilityKind, "circle", "cone", "line", "ring", "bolt", "raise", "summon", "hazard", "buff", "call")

// Where an ability aims (resolved once, at windup start):
//   hero          bolts: launched at contact, locked on the hero (the basic ranged swing's flight); cones / lines: from
//                 the caster toward the hero's position at windup start.
//   heroAtWindup  the hero's position at windup start (slams land where the hero stood); cones / lines as `hero`.
//   self          centred on the caster (cones / lines along its heading).
//   corpse        a corpse (raise, K3).
//   anchor        the caster's spawn anchor tile (Valdras's dais); cones / lines point from it toward the hero.
// A ground shape that hurts on contact (circle, ring, hazard) never takes `hero`: its decal is placed at windup start
// and fills to contact, so the loader refuses a target that would follow the hero (monsters.md 1.3).
enum class AbilityAim : uint8_t { Hero, HeroAtWindup, Self, Corpse, Anchor };
ABYSS_ENUM_STRINGS(AbilityAim, "hero", "heroAtWindup", "self", "corpse", "anchor")

// Geometry of a ground telegraph / hazard (tiles, degrees). Circle: `radius` around the origin. Ring: between
// `innerRadius` and `radius`. Cone: `radius` reach, `arcDeg` total opening, centred on the direction. Line: a
// `length` x `width` rectangle starting at the origin along the direction (width centred on it).
enum class AbilityShapeKind : uint8_t { Circle, Cone, Line, Ring };
ABYSS_ENUM_STRINGS(AbilityShapeKind, "circle", "cone", "line", "ring")

struct AbilityShapeDef {
  AbilityShapeKind kind = AbilityShapeKind::Circle;
  double radius = 0;
  double arcDeg = 0;
  double length = 0;
  double width = 0;
  double innerRadius = 0;
};

// {type, chance, ms, value}: rolled on a landed hit (chance 0..1; 1 = no draw); `value` (> 0) is the status value (slow
// / freeze percent, DoT damage per tick, any positive value for stun), `ms` its duration.
struct AbilityStatusDef {
  bool present = false;
  StatusType type = StatusType::Slow;
  double chance = 1;
  double ms = 0;
  double value = 0;
};

// A hazard the ability leaves on the ground (or, for kind `hazard`, the ability itself), ticking on the sim clock
// (combat/HazardField.h): per tick floor(damage x tickDmgMul) (at least 1 when tickDmgMul > 0), the status rolled per
// tick. `shape` defaults to the ability's shape.
struct AbilityHazardDef {
  bool present = false;
  double durationMs = 0;
  double tickMs = 0;
  double tickDmgMul = 0;
  AbilityStatusDef status;
  bool hasShape = false;
  AbilityShapeDef shape;
};

// raise / summon (runtime: K3 corpses and raises, Combat.h "K3"). `at`: around the caster, at its `anchors`, or on a
// corpse (a raise). Every body stands as MonsterRole::Raised with the caster as its raiser.
enum class AbilitySummonAt : uint8_t { Self, Anchors, Corpse };
ABYSS_ENUM_STRINGS(AbilitySummonAt, "self", "anchors", "corpse")

struct AbilitySummonDef {
  bool present = false;
  std::string monsterId;
  int32_t count = 1;
  AbilitySummonAt at = AbilitySummonAt::Self;
  std::vector<TilePos> anchors;
  int32_t capAlive = 0;       // live Raised per caster (0 = no cap)
  int32_t perWindowCap = 0;   // bodies the caster raised / summoned within the last `windowMs` (0 = no cap)
  double windowMs = 0;
  double hpPct = 100;         // summoned at this percent of their life
  // K3: a raise takes corpses within `radius` tiles of the caster (default MonsterAiDef::raiseRadiusDefault, monsters.md
  // "within 6 tiles"); a summon at `self` stands its bodies on a ring of `radius` around the caster (default
  // MonsterAiDef::summonRingRadius). Unused by `anchors`.
  bool hasRadius = false;
  double radius = 0;
};

// buff / call (runtime: K5 packs and morale): `stat` x `mul` for `ms` on allies within `radius` (of `family` when set).
enum class MonsterBuffStat : uint8_t { Damage, AttackSpeed, MoveSpeed, FearImmune };
ABYSS_ENUM_STRINGS(MonsterBuffStat, "damage", "attackSpeed", "moveSpeed", "fearImmune")

struct AbilityBuffDef {
  bool present = false;
  MonsterBuffStat stat = MonsterBuffStat::Damage;
  double mul = 1;
  double ms = 0;
  double radius = 0;
  std::string family;
};

// One ability (implementation-plan.md 3 K2 "Data"). Contact: `windupMs` when authored (>= 600 ms, >= 700 ms for a
// boss), else the clip's Contact notify (else its Release) from the asset manifest; the telegraph fills to full then.
struct MonsterAbilityDef {
  std::string id;
  AbilityKind kind = AbilityKind::Circle;
  std::string clip;                 // the manifest clip it plays (Attack02, Cast01, Lunge, ...)
  bool hasWindupMs = false;
  double windupMs = 0;
  double cooldownMs = 0;            // from windup start to the next use
  double initialCooldownMs = 0;     // from the monster's first swing of a fight
  double rangeMin = 0;              // hero distance (tiles) at the pick
  bool hasRangeMax = false;
  double rangeMax = 0;
  bool hasHpBelowPct = false;
  double hpBelowPct = 100;          // usable while life < hpBelowPct percent
  std::vector<int32_t> phases;      // usable in these phases (K4); empty = every phase
  int32_t priority = 0;             // higher first; ties in data order
  AbilityAim aim = AbilityAim::HeroAtWindup;
  bool hasShape = false;
  AbilityShapeDef shape;
  double dmgMul = 1;                // x the monster's damage roll (contact hit)
  bool hasDamageType = false;
  DamageType damageType = DamageType::Physical;  // default: the monster's damageType
  AbilityStatusDef status;
  bool hitsMonsters = false;
  double monsterDmgMul = 1;
  AbilityHazardDef hazard;
  AbilitySummonDef summon;
  AbilityBuffDef buff;
};

// ---- K3 on-death effects, corpses and the Unburied rise (dark remake, implementation-plan.md 3 K3; monsters.md 1.3 /
// 1.5; runtime: Combat.h "K3" and Corpses.h) ----------------------------------------------------------------------------
// What happens where a monster died, `delayMs` after its death (on the sim clock):
//   burst   a circle hit (the exploder: fuse >= 800 ms under a telegraph, damage capped, monsters x monsterDmgMul);
//   ring    a circle or ring hit (the Gravecold frost ring: a status, damage optional);
//   puddle  a ground hazard (HazardField) that ticks the damage every puddle.tickMs for puddle.durationMs;
//   spawn   spawn.count of spawn.monsterId around the spot (MonsterRole::Raised without a raiser: no rewards, no respawn).
// Damage: `dmg` (x the difficulty's monster damage multiplier) or `hpPct` percent of the dying monster's maximum life;
// the hero's resistance applies to a non-physical type, then the cap (capHeroMaxHpPct and the monster's
// hitCapHeroMaxHpPct, each a percent of the hero's max life x the difficulty's damage multiplier; the smaller binds).
enum class OnDeathKind : uint8_t { Burst, Ring, Puddle, Spawn };
ABYSS_ENUM_STRINGS(OnDeathKind, "burst", "ring", "puddle", "spawn")

struct OnDeathPuddleDef {
  double durationMs = 0;
  double tickMs = 0;
};

struct OnDeathSpawnDef {
  std::string monsterId;
  int32_t count = 1;
};

struct MonsterOnDeathDef {
  bool present = false;
  OnDeathKind kind = OnDeathKind::Burst;
  double delayMs = 0;          // after the death (bursts >= 800: the fuse)
  AbilityShapeDef shape;       // burst: a circle; ring: a ring or a circle; puddle: either (circle when omitted)
  bool hasDmg = false;
  double dmg = 0;              // fixed (x the difficulty's damageMul); a puddle: per tick
  bool hasHpPct = false;
  double hpPct = 0;            // percent of the dying monster's maximum life (the Labyrinth's volatile curse)
  bool hasDamageType = false;
  DamageType damageType = DamageType::Physical;  // default: the monster's damageType
  AbilityStatusDef status;     // rolled on a landed hit (a puddle: per tick)
  bool hitsMonsters = false;
  double monsterDmgMul = 1;    // monsters inside take floor(damage x monsterDmgMul) (raw, no cap)
  double capHeroMaxHpPct = 0;  // 0 = only the monster's own hit cap (a burst must name one: monsters.md 1.3)
  OnDeathPuddleDef puddle;
  OnDeathSpawnDef spawn;
};

// What the body leaves (monsters.md 1.5): `corpse` an entry other monsters raise (Unburied, Living) and eat; `remains`
// an entry nothing raises or eats (the Gravegnawers' carcass, a boss's body); `none` no entry (Breach-blood's tar
// puddle). Both kinds burn. `consumes`: this monster eats corpses (the Gravegnawer).
enum class CorpseLeaves : uint8_t { None, Corpse, Remains };
ABYSS_ENUM_STRINGS(CorpseLeaves, "none", "corpse", "remains")

struct MonsterCorpseDef {
  CorpseLeaves leaves = CorpseLeaves::None;
  bool consumes = false;
};

// The Unburied rise (monsters.md 2.2 trait, 3 affix): with `chance` (RngStream::Ai; 1 = no draw) the monster's own
// corpse stands up once, `delayMs` after the death, at `hpPct` of its life; a fire killing blow or a burned (or raised,
// or eaten) corpse prevents it, and `notOnCrit` a critical killing blow too.
struct MonsterRiseAgainDef {
  bool present = false;
  double chance = 1;
  double delayMs = 0;
  double hpPct = 100;
  bool notOnCrit = false;
};

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
  // ---- dark remake K2 (remake schema monster.damageType / monster.hitCap / monster.abilities) ----
  // Damage type of its swings and bolts (resistances apply to every type but physical; default physical).
  DamageType damageType = DamageType::Physical;
  // No single hit of this monster takes more than this percent of the hero's maximum life on Normal; Nightmare / Hell
  // scale it by the difficulty's damage multiplier (MonsterHitCap, monsters.md 1.3). 0 = no cap.
  double hitCapHeroMaxHpPct = 0;
  std::vector<MonsterAbilityDef> abilities;
  // The web's bossSkills ids (M9: inert until K2). Every id must name one of `abilities` once the monster has any;
  // a monster without abilities keeps them as inert data (DataStore warns).
  std::vector<std::string> bossSkills;
  // ---- dark remake K3 (remake schema monster.family / monster.onDeath / monster.corpse / monster.riseAgain) ----
  // Family id (monsters.md 1.1: unburied, living, gravegnawers, breach; "" = none): copied onto its corpse entries;
  // elite affixes with `families` roll only for these (the Unburied affix).
  std::string family;
  MonsterOnDeathDef onDeath;
  MonsterCorpseDef corpse;
  MonsterRiseAgainDef riseAgain;
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
  // Defend waves (quests 3.10, monsters 6.4; ZoneScene.spawnDefendWave constants, not exported, so not loaded):
  // n = defendWaveBaseCount + w monsters on a circle of defendWaveRadius tiles around the target, rounded and clamped
  // to [defendWaveEdgeMargin, size - 1 - defendWaveEdgeMargin]; hp floor(hp * (1 + defendWaveHpPerWave * w)),
  // damage floor(damage * (1 + defendWaveDmgPerWave * w)) after difficulty.
  int32_t defendWaveBaseCount = 3;
  double defendWaveRadius = 8;
  int32_t defendWaveEdgeMargin = 2;
  double defendWaveHpPerWave = 0.3, defendWaveDmgPerWave = 0.2;
  // Labyrinth scaling (monsters 13.2, later milestone; DungeonSystem constants, not exported, so not loaded):
  // raiseToLevel keeps monsters within raiseLevelWindow levels; exponents of m = L / max(1, level); depth bonuses per
  // floor; gatekeeper multipliers and id.
  int32_t raiseLevelWindow = 6;
  double raiseHpExp = 1.1, raiseDamageExp = 0.95, raiseDefenseExp = 0.9, raiseExpExp = 1.1;
  double labyrinthExpPerFloor = 0.15, labyrinthGoldPerFloor = 0.1;
  double gatekeeperHpMulElite = 1.6, gatekeeperHpMul = 4, gatekeeperDamageMul = 1.3, gatekeeperExpMul = 4;
  double gatekeeperGoldMul = 3, gatekeeperAggroMin = 8;
  std::string gatekeeperId = "dungeon_gatekeeper";
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
  std::string storyBossId;                         // the chapter's story boss (GameData/config.ts storyBoss)
  bool storyBossOncePerVisit = true;
  std::string storyBossNotAfterQuestTurnIn;        // the quest that kills it
  bool storyBossFarmableAfterChapter = true;
  std::string chapterCompleteQuest;                // its turn-in plays the chapter finale (GameData/config.ts)
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
  // Dark remake K3 tuning (implementation-plan.md 3 K3, monsters.md 1.5 / 2.4; port-only, not loaded): a corpse entry
  // lasts corpseTtlMs, at most corpseCap killed bodies per zone (the oldest goes first; map seeds do not count and never
  // expire); a monster that `consumes` eats a corpse within consumeRadius tiles while out of combat (idle / patrol) or
  // below consumeBelowHpFraction of its life: a consumeChannelMs channel (the Consume clip), then it heals
  // consumeHealFraction of its maximum life. A raise takes corpses within raiseRadiusDefault tiles of the caster unless
  // the ability names its radius; a summon at `self` stands its bodies on a ring of summonRingRadius. A risen body is
  // rooted while its Rise clip plays (the manifest clip's length, else riseMsDefault); raise / summon markers are drawn
  // with a radius of raiseMarkerRadius.
  double corpseTtlMs = 20000;
  int32_t corpseCap = 24;
  double consumeRadius = 1;
  double consumeChannelMs = 2000;
  double consumeHealFraction = 0.3;
  double consumeBelowHpFraction = 0.5;
  double raiseRadiusDefault = 6;
  double summonRingRadius = 1.5;
  double riseMsDefault = 1200;
  double raiseMarkerRadius = 0.75;
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
