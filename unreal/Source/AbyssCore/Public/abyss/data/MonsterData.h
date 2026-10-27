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
#include "abyss/sim/SimTypes.h"  // AnimAction (K4 phase enterAction)

namespace abyss {

enum class MonsterSource : uint8_t { Zone, Dungeon, MiniBoss, SubDungeonBoss, Hunt };
ABYSS_ENUM_STRINGS(MonsterSource, "zone", "dungeon", "miniBoss", "subDungeonBoss", "hunt")

// ---- K2 monster abilities (dark remake, implementation-plan.md 3 K2; monsters.md 1.3 fairness) ----------------------
// A monster's `abilities[]` (remake schema `monster.abilities`). MonsterAbilities.h holds the rules (picker, shapes,
// timing); CombatSystem runs them through the monster swing path. Every kind runs since K5 (raise and summon: K3, buff
// and call: K5; AbilityKindRuns).
enum class AbilityKind : uint8_t { Circle, Cone, Line, Ring, Bolt, Raise, Summon, Hazard, Buff, Call };
ABYSS_ENUM_STRINGS(AbilityKind, "circle", "cone", "line", "ring", "bolt", "raise", "summon", "hazard", "buff", "call")

// Where an ability aims (resolved once, at windup start):
//   hero          bolts: launched at contact, locked on the hero (the basic ranged swing's flight); cones / lines: from
//                 the caster toward the hero's position at windup start.
//   heroAtWindup  the hero's position at windup start (slams land where the hero stood); cones / lines as `hero`.
//   self          centred on the caster (cones / lines along its heading).
//   corpse        a corpse (raise, K3).
//   anchor        the caster's authored spawn tile (MonsterInstance::homeTile: Valdras's dais, not the M10-jittered
//                 spawn anchor); cones / lines point from it toward the hero.
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

// buff / call (runtime: K5 packs and morale, Combat.h "K5"): `stat` x `mul` for `ms` on the living monsters within
// `radius` of the caster, the caster included (of `family` when set). A call also provokes (M2) every other living
// monster within `radius` (of `family`): the social call; its `stat` / `ms` are optional (a call without them buffs
// nothing). Stats: damage (the damage formula's outgoing multiplier on its swings, ability hits and bolts), attackSpeed
// (swings per second: the interval / mul), moveSpeed (its top speed), fearImmune (it never flees: morale; `mul` unused).
// Buffs of one stat do not stack: the largest mul among its live buffs and auras applies.
enum class MonsterBuffStat : uint8_t { Damage, AttackSpeed, MoveSpeed, FearImmune };
ABYSS_ENUM_STRINGS(MonsterBuffStat, "damage", "attackSpeed", "moveSpeed", "fearImmune")

struct AbilityBuffDef {
  bool present = false;
  bool hasStat = false;  // always for a buff; a call may only provoke
  MonsterBuffStat stat = MonsterBuffStat::Damage;
  double mul = 1;
  double ms = 0;
  double radius = 0;
  std::string family;
};

// One ability (implementation-plan.md 3 K2 "Data"). Contact: `windupMs` when authored (>= 600 ms, >= 700 ms for a
// boss), else the clip's Contact notify (else its Release) from the asset manifest; the telegraph fills to full then.
// 2F review: a hazard or call ability may name no clip: the caster plays nothing and is not rooted while it winds up (the
// ground decal, the call's ripple are the tell: Valdras's Breach-seep pulses from the dais, Hungerbell's bell rings), and
// a damaging ability whose rangeMax passes the caster's swing reach is a ranged special that also starts between swings
// (MonsterAbilities.h AbilityIsRangedSpecial).
struct MonsterAbilityDef {
  std::string id;
  AbilityKind kind = AbilityKind::Circle;
  std::string clip;                 // the manifest clip it plays (Attack02, Cast01, Lunge, ...); "" = none (hazard / call)
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
  // 2F review (a charge, chapter1.md 11 Lunge): a line ability whose caster rushes along it at contact, to the farthest
  // point of the line it can reach in a straight walkable line inside its M1 leash and K4 tether and outside every camp
  // (resolved at windup start: MonsterAbilityCast::dashTo; the hit lands first, then the caster stands there).
  bool dash = false;
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

// ---- K4 boss phases and arenas (dark remake, implementation-plan.md 3 K4; monsters.md 1.3 "no invulnerable phases";
// runtime: monsters/Phases.h rules, MonsterSystem::TickPhases) ---------------------------------------------------------
// A monster's `phases[]` (remake schema `monster.phases`): rows in strictly decreasing `hpBelowPct`. Phase 0 is the
// monster as defined (no row); entering row i makes MonsterInstance::phase i + 1, once, in order, when its life drops
// below the row's threshold (a hit that crosses two thresholds enters both, in order). Each row describes the monster
// while that phase holds: its multipliers (x the base, composing with elite affixes: not with the previous row's), its
// tether, its Feed; a row's arena stays until a later row lights another one; abilitiesOn / abilitiesOff switch
// abilities in sequence (an ability whose first switch is an abilitiesOn starts switched off). Every centre defaults to
// the monster's authored spawn tile (MonsterInstance::homeTile, the dais; not the M10-jittered spawn anchor).

// The tether (`leash`): while the phase holds, the monster's chase / patrol goals are held inside `radius` of the centre
// (it walks to the edge nearest the hero and waits there; it walks back in when outside). Not the M1 leash: a tether
// never sends it home or resets the fight.
struct PhaseLeashDef {
  bool present = false;
  bool hasCentre = false;
  TilePos centre;
  double radius = 0;
};

// The soft enrage (`arena`, kind ringShrink): a ring around the centre whose radius goes from `fromR` to `toR` over
// `overMs` (linear; it stays at toR after) from the phase entry; every MonsterAiDef::arenaTickMs the hero outside it
// takes dpsPctMaxHp percent of its maximum life per second (x the difficulty's damage multiplier, less its resistance,
// capped by the monster's hit cap; a DoT-like tick that Death Save holds). Monsters are never hurt by it.
enum class ArenaKind : uint8_t { RingShrink };
ABYSS_ENUM_STRINGS(ArenaKind, "ringShrink")

struct PhaseArenaDef {
  bool present = false;
  ArenaKind kind = ArenaKind::RingShrink;
  bool hasCentre = false;
  TilePos centre;
  double fromR = 0;
  double toR = 0;
  double overMs = 0;
  double dpsPctMaxHp = 0;
  DamageType damageType = DamageType::Physical;
};

// Bodies the phase stands up at its entry (`summons[]`, after `delayMs`): `count` of `monsterId` as Raised with the
// monster as raiser (no rewards, no respawn, collapse with it), on a ring of `radius` around it (`at` self; default
// MonsterAiDef::summonRingRadius) or on its `anchors` (round robin). A body killed while its phase still holds stands up
// again in its slot (its anchor, or its ring slot around the monster then) `respawnMs` later, at most `respawns` times.
struct PhaseSummonDef {
  std::string monsterId;
  int32_t count = 1;
  AbilitySummonAt at = AbilitySummonAt::Self;  // self | anchors
  std::vector<TilePos> anchors;
  bool hasRadius = false;
  double radius = 0;
  double hpPct = 100;
  double delayMs = 0;
  double respawnMs = 0;
  int32_t respawns = 0;
};

// Feed: while the phase holds, a Raised monster killed within `radius` of the centre is drawn into the monster: its
// corpse is held for `delayMs` (fire can still burn it), then consumed, and the monster heals healPct percent of its
// maximum life.
struct PhaseFeedDef {
  bool present = false;
  bool hasCentre = false;
  TilePos centre;
  double radius = 0;
  double healPct = 0;
  double delayMs = 0;
};

struct MonsterPhaseDef {
  double hpBelowPct = 0;                       // entered when life < hpBelowPct percent
  bool hasEnterAction = false;
  AnimAction enterAction = AnimAction::Roar;   // played on entry (rooted, no swing, still takes damage)
  double speedMul = 1;
  double attackSpeedMul = 1;                   // swings per second x attackSpeedMul (the interval / attackSpeedMul)
  double damageTakenMul = 1;
  PhaseLeashDef leash;
  std::vector<std::string> abilitiesOn;
  std::vector<std::string> abilitiesOff;
  std::vector<PhaseSummonDef> summons;
  PhaseArenaDef arena;
  PhaseFeedDef feed;
  std::string stinger;                         // music stinger id for UE ("" = none)
};

// ---- K5 packs, morale, kite, block and auras (dark remake, implementation-plan.md 3 K5; monsters.md 2.2-2.5; runtime:
// monsters/Packs.h rules, MonsterSystem.h "K5", Combat.h "K5") ---------------------------------------------------------

// Kite (`kite`, a ranged monster's band): when the hero stands closer than `min` tiles, the monster steps away to a
// walkable point about `max` tiles from the hero along the away vector (the nearest it can reach in a straight line,
// inside its M1 leash and its K4 tether), then swings at least once before it kites again.
struct MonsterKiteDef {
  bool present = false;
  double min = 0;
  double max = 0;
};

// Block (`block`, the shield wall): a hit (not a tick) from within `arcDeg` (total) around the monster's heading, measured
// toward where the hit came from, is multiplied by `damageMul` (floored, at least 1) and emits EvBlock; a stunned or
// frozen monster does not block. `wall` > 0: blockers within `wall` tiles of each other (chained) share one heading, the
// mean of theirs (the line holds).
struct MonsterBlockDef {
  bool present = false;
  double arcDeg = 0;
  double damageMul = 1;
  double wall = 0;
};

// Morale (`morale`): when the leader of its pack dies, this monster flees (MonsterState::Flee) for fleeOnLeaderDeathMs,
// away from the leader's body, if it stands within `fleeRadius` of it (0 = anywhere: the whole pack) - unless it is
// fearImmune (a buff or aura) or a living sustain member (a monster of `sustainFamily`, any family when empty, with a
// buff / call ability that grants fearImmune) stands within `sustainRadius` of it; either also ends a flee under way.
struct MonsterMoraleDef {
  bool present = false;
  double fleeOnLeaderDeathMs = 0;
  double fleeRadius = 0;
  std::string sustainFamily;
  double sustainRadius = 0;
};

// Aura (`aura`, the standard-bearer's banner): while the monster lives, every living monster within `radius` of it (it
// included; of `family` when set) has `stat` x `mul` (MonsterBuffStat; no stacking with buffs of the same stat: the
// largest applies).
struct MonsterAuraDef {
  bool present = false;
  MonsterBuffStat stat = MonsterBuffStat::Damage;
  double mul = 1;
  double radius = 0;
  std::string family;
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
  // ---- dark remake K4 (remake schema monster.phases) ----
  std::vector<MonsterPhaseDef> phases;
  // ---- dark remake K5 (remake schema monster.packAggro / kite / block / morale / aura) ----
  // Pack aggro: when another living member of its pack within this many tiles is provoked (M2) or enters Chase, this
  // monster is provoked too, once per trigger (it does not pass the alarm on). 0 = it keeps to itself.
  double packAggro = 0;
  MonsterKiteDef kite;
  MonsterBlockDef block;
  MonsterMoraleDef morale;
  MonsterAuraDef aura;
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
  // Dark remake (remake schema quest.huntAbilities, package 2F): the hunt's own abilities (quest_hunts.json
  // `hunts[].abilities`), after its base's in its definition (MakeHuntDefinition; Hungerbell's supper bell).
  std::vector<MonsterAbilityDef> abilities;
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
  // Dark remake K4 tuning (implementation-plan.md 3 K4; port-only, not loaded): a phase's enter action roots the
  // monster for its clip's length (assets.json), else phaseActionMsDefault (chapter1.md 11: "Roar (1.2 s)"); an arena
  // hurts the hero outside it every arenaTickMs ("every 500 ms").
  double phaseActionMsDefault = 1200;
  double arenaTickMs = 500;
  // Dark remake K5 tuning (implementation-plan.md 3 K5; port-only, not loaded): a kite step gives up at the later of
  // kiteTimeoutMs and twice its straight walk at top speed after it starts; its target is sampled every kiteSampleTiles
  // along the away vector and must gain at least that much distance from the hero; a fleeing monster tests the point
  // fleeProbeTiles ahead in each direction, which must gain at least fleeMinGainTiles of distance from the body (so a
  // sidestep does not circle the body forever; MonsterAI.h "K5").
  double kiteTimeoutMs = 2000;
  double kiteSampleTiles = 0.5;
  double fleeProbeTiles = 1.0;
  double fleeMinGainTiles = 0.1;
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
