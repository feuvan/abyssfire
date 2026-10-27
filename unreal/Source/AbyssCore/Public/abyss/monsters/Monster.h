// Runtime monster instance (monsters-ai.md 1.2-1.3, 3.1).
//
// Owner area: monsters. A MonsterInstance is plain state; behaviour lives in MonsterAI.h (pure AI tick) and
// MonsterSystem.h (spawning, damage, death, respawn, queries).
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Stats.h"
#include "abyss/base/Types.h"
#include "abyss/data/CombatData.h"
#include "abyss/data/MonsterData.h"
#include "abyss/hero/Buffs.h"

namespace abyss {

// AI states (monsters 3.1) + the port's leash `Returning` state (M1) + the dark remake's `Flee` (K5 morale, appended:
// its pack's leader died; it runs from the leader's body until MonsterInstance::fleeUntilMs, no swing or cast).
enum class MonsterState : uint8_t { Idle, Patrol, Chase, Attack, Returning, Dead, Flee };
ABYSS_ENUM_STRINGS(MonsterState, "idle", "patrol", "chase", "attack", "returning", "dead", "flee")

// Why a monster exists; drives respawn eligibility (M4/W9), affix rolls and loot floors (monsters 1.2).
// StoryBoss (M7): the zone spawn of MonsterAiDef::storyBossId (Ch1 goblin_chief); once per visit, no respawn.
// Raised (dark remake K3, appended): a body stood up from a corpse by a raise or the Unburied rise, or summoned (a summon
// ability, an on-death spawn): no exp, gold, loot, quest drop, embers or pet exp; never respawns; collapses (dies without
// a kill) when its raiser (MonsterInstance::raiserId) dies.
enum class MonsterRole : uint8_t {
  Regular,
  ZoneMiniBoss,
  SubDungeonMiniBoss,
  HuntLeader,
  HuntMinion,
  AmbushSpawn,
  RescueSpawn,
  DefendWave,
  LabyrinthFloor,
  SealKeeper,
  StoryBoss,
  Raised,
};
ABYSS_ENUM_STRINGS(MonsterRole, "regular", "miniBoss", "subDungeonMiniBoss", "hunt", "huntMinion", "ambush", "rescue",
                   "defendWave", "labyrinth", "sealKeeper", "storyBoss", "raised")

// The ability a monster is winding up (K2, implementation-plan.md 3): resolved once, at windup start, and resolved at
// contact through the pending-strike path (CombatSystem). `origin` / `dir` place the ground shape (the telegraph);
// `target` is the aim point (the hero's position at windup start, the caster, or its anchor).
struct MonsterAbilityCast {
  double startMs = 0;
  double contactMs = 0;  // absolute sim ms
  Vec2 origin;
  Vec2 dir{1, 0};        // unit
  Vec2 target;
  AbilityShapeDef shape;
  // 2F review: a dash (MonsterAbilityDef::dash) ends here at contact (the caster's position when it cannot move).
  bool hasDash = false;
  Vec2 dashTo;
};

// 2F review (SAVE-R5; MonsterSystem.h PinnedAffixesInto): a named boss's elite affix roll (the story boss, a zone
// mini-boss), kept from its first fight until it dies so that loading a save mid-fight never rolls it again
// (SaveData::bossAffixes).
struct BossAffixPin {
  std::string zone;
  std::string monsterId;
  std::vector<EliteAffixType> affixes;
  bool operator==(const BossAffixPin& o) const = default;
};
// Sorted by (zone, monsterId), one pin per pair (the first kept), no empty id (SaveIO's parse, MonsterSystem::ReadSave).
ABYSS_API void NormalizeBossAffixPins(std::vector<BossAffixPin>& pins);

// K4: the soft enrage a phase lit (MonsterPhaseDef::arena; Phases.h ArenaRadiusAt): resolved at the phase entry.
struct MonsterArena {
  bool active = false;
  ArenaKind kind = ArenaKind::RingShrink;
  int32_t phase = 0;      // the phase that lit it
  Vec2 centre;
  double fromR = 0, toR = 0;
  double startMs = 0, overMs = 0;
  double dpsPctMaxHp = 0;
  DamageType damageType = DamageType::Physical;
  double nextTickMs = 0;  // absolute sim ms of its next tick on the hero
};

// K5: a buff a buff / call ability gave (MonsterData.h AbilityBuffDef): `stat` x `mul` until untilMs (sim ms).
struct MonsterAbilityBuff {
  MonsterBuffStat stat = MonsterBuffStat::Damage;
  double mul = 1;
  double untilMs = 0;
  EntityId source = kNoEntity;
};

// One rolled elite affix with its behaviour timers (combat-feel.md 17).
struct MonsterAffix {
  EliteAffixType type = EliteAffixType::FireEnhanced;
  double lastTeleportMs = -1e300;  // first teleport check is immediate
  double lastCurseLogMs = -1e300;
};

struct ABYSS_API MonsterInstance {
  EntityId id = kNoEntity;
  MonsterDef def;           // current definition (difficulty / hunt / affix modified copy)
  MonsterDef originalDef;   // post-difficulty, pre-affix (respawn source; affixes never compound)
  MonsterRole role = MonsterRole::Regular;
  bool noRespawn = false;   // M4 / W9 and role rules
  std::string huntId;       // hunt leader: hunt id (== def.id)
  double visualScale = 1.0; // render-only (hunt leaders 1.25)
  // vitals
  double hp = 0, maxHp = 0;
  PrimaryStats stats;       // {str floor(dmg*0.8), dex floor(speed*0.1), vit floor(hp*0.1), 3, 3, 3}
  BuffList buffs;           // death_mark amplify, taunted; pruned by duration (FIX Q19)
  std::vector<MonsterAffix> affixes;
  // position / movement (tile space)
  Vec2 pos;
  Vec2 prevPos;             // position at the start of the last AI step (render interpolation)
  Vec2 heading{1, 0};       // unit vector; render yaw follows it (M11 turn rate is render-only)
  double moveSpeed = 0;     // current smoothed speed, tiles/s (velocity form of monsters 3.4)
  double groundSpeed = 0;   // actual displacement speed of the last AI step, tiles/s (locomotion blend)
  TilePos spawnAnchor;      // original zone-entry anchor (M3: respawn and leash use it, never drifts)
  TilePos homeTile;         // the authored spawn tile (a map spawn's tile before the M10 placement jitter; else the
                            // tile it spawned on): the dais. K2's `anchor` aim and every K4 default centre use it
  // AI
  MonsterState state = MonsterState::Idle;
  double patrolTimerMs = 0;
  bool hasPatrolTarget = false;
  TilePos patrolTarget;
  double patrolStartedMs = 0;       // M10 patrol timeout
  double provokedUntilMs = 0;       // M2
  std::vector<TilePos> path;        // M6 A* path when line-of-walk is blocked
  double repathAtMs = 0;
  // combat
  double lastAttackMs = 0;          // last swing start (0 = never; first swing immediate)
  double lastDamagedMs = 0;
  bool hasLastHitFrom = false;
  Vec2 lastHitFrom;                 // death throw direction (render)
  double lastEscortChipMs = -1e300; // quests 3.9
  double lastDefendChipMs = 0;      // quests 3.10
  // K2 abilities (MonsterAbilities.h). The ability clock starts at the monster's first swing opportunity of a fight
  // (each ability ready initialCooldownMs later) and stops when it leashes home (M1); abilityReadyAtMs is per
  // def.abilities index. One pending ability at a time; while it winds up the monster is rooted (no AI tick) and
  // starts no other swing.
  bool abilityClockStarted = false;
  std::vector<double> abilityReadyAtMs;
  int32_t pendingAbility = -1;      // def.abilities index, -1 = none
  MonsterAbilityCast abilityCast;   // the pending ability's resolved aim
  double abilityRootedUntilMs = 0;  // = the pending ability's contact
  // K4 boss phases (monsters/Phases.h; MonsterSystem.h "K4"). `phase` = the phase rows entered (0 = the monster as
  // defined; abilities' phases[] read it). The current row's multipliers (1 = none; on top of the affixed definition:
  // move speed in MonsterMoveToward, the swing interval in AttackIntervalMs, damage taken in CombatSystem::DamageMonster)
  // and its tether (MonsterAI: chase and patrol goals held inside the circle); abilityOff = abilities switched off by the
  // phase table (empty without phases); phaseActionUntilMs roots it (no AI step, no swing or cast) while its enter
  // action plays; `arena` = the soft enrage its phases lit.
  int32_t phase = 0;
  double phaseSpeedMul = 1, phaseAttackSpeedMul = 1, phaseDamageTakenMul = 1;
  bool hasTether = false;
  Vec2 tetherCentre;
  double tetherRadius = 0;
  std::vector<uint8_t> abilityOff;
  double phaseActionUntilMs = 0;
  MonsterArena arena;
  // K3 (Corpses.h; Combat.h / MonsterSystem.h "K3"). A Raised body (MonsterRole::Raised: it rose once, so its own
  // corpse is spent) stands for `raiserId` (kNoEntity for a self-risen or an on-death-spawned one: it collapses with
  // nobody). A raise / summon winding up keeps its corpses / spots (abilityCorpses / abilitySpots: the corpse positions
  // for a raise) until contact; summonedAtMs holds when this caster stood each body up (perWindowCap). A consume
  // channel holds consumeCorpse until consumeUntilMs (rooted; no swing). abilityRootedUntilMs also roots a risen body
  // while its Rise clip plays.
  EntityId raiserId = kNoEntity;
  std::vector<double> summonedAtMs;
  std::vector<uint32_t> abilityCorpses;
  std::vector<Vec2> abilitySpots;
  uint32_t consumeCorpse = 0;       // 0 = not eating
  double consumeUntilMs = 0;
  // K5 (monsters/Packs.h; MonsterSystem.h "K5"). Its pack (a map spawn entry, a hunt with its minions; 0 = none) and
  // whether it leads it. Flee (MonsterState::Flee): away from fleeFrom (the leader's body) until fleeUntilMs. Kite: a
  // step to kiteTarget under way (state Chase); after it ends (kiteEndedMs) it strikes before it kites again
  // (lastStrikeMs: the start of its last swing, at the hero or a beast, or of a damaging ability; a support cast between
  // swings, raise / summon / buff / call, sets lastAttackMs but is no strike). Buffs from
  // buff / call abilities (pruned when they run out), and the multipliers its buffs and the auras around it give this
  // step (MonsterSystem refreshes them at the start of every TickAI and when a buff lands; 1 / false = none): attack
  // speed in AttackIntervalMs, move speed in MonsterMoveToward, damage as the formula's outgoing multiplier (CombatSystem),
  // fearImmune in morale.
  uint32_t packId = 0;
  bool packLeader = false;
  // K6 (MonsterSystem.h "K6"): the map spawn entry it stands for (index into the zone's MapDef::spawns; -1 = not a map
  // spawn) and its slot among the entry's members; a respawn reads the entry's spawn variant again.
  int32_t spawnEntry = -1;
  int32_t spawnMember = -1;
  // K6: one more equipment item of this quality its kill drops after its regular loot (a champion pack's leader with
  // AmbushChampionDef::leaderLoot; Normal = none; MonsterKilledMsg::guaranteedLoot). Not carried over by a respawn.
  ItemQuality guaranteedLoot = ItemQuality::Normal;
  double fleeUntilMs = 0;
  Vec2 fleeFrom;
  Vec2 fleeDir;  // its last flee step's direction (zero when a flee starts): the next never doubles back on it
  bool kiting = false;
  Vec2 kiteTarget;
  double kiteUntilMs = 0;  // the step gives up then (MonsterAiDef::kiteTimeoutMs, or twice its walk)
  bool kiteNeedsSwing = false;
  double kiteEndedMs = 0;
  double lastStrikeMs = 0;
  std::vector<MonsterAbilityBuff> abilityBuffs;
  std::array<double, 4> auraMul{1, 1, 1, 1};  // per MonsterBuffStat: the largest aura reaching it this step
  uint8_t auraMask = 0;                       // bit per MonsterBuffStat: an aura of that stat reaches it
  double buffDamageMul = 1, buffAttackSpeedMul = 1, buffMoveSpeedMul = 1;
  bool fearImmune = false;
  // 2F review (SAVE-R5): a story boss / zone mini-boss whose affix roll is pinned (MonsterSystem::bossAffixes_): it
  // spawned from a pin, or entered its first fight (MonsterSystem.h PinnedAffixesInto).
  bool affixesPinned = false;
  // presentation flags
  bool storyNameShown = false;      // boss label renamed (story 8.3)
  bool active = true;               // in the AI activity set (30 tiles / aggro), monsters 3.8

  bool IsAlive() const { return state != MonsterState::Dead && hp > 0; }
  bool IsAggro() const { return state == MonsterState::Chase || state == MonsterState::Attack; }
  int32_t AffixCount() const { return static_cast<int32_t>(affixes.size()); }
  // ms between swing starts: the definition's attackSpeed, divided by the K4 phase's attackSpeedMul and the K5
  // attackSpeed buff / aura.
  double AttackIntervalMs() const { return def.attackSpeedMs / (phaseAttackSpeedMul * buffAttackSpeedMul); }
  double AffixLootBonus(const EliteAffixTable& table) const;
};

}  // namespace abyss
