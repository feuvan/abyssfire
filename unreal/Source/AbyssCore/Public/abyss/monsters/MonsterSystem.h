// MonsterSystem: the zone's monster population (spawning, AI driver, damage, death, respawn, mini-boss, quest hunts,
// event spawns, elite behaviours) and the monster queries every other area uses.
// Spec: monsters-ai.md 3.8-3.10 (driver, tick order, external writes), 4 (swing scheduling lives in CombatSystem),
// 5 (takeDamage / heal / die), 6 (spawning, safe zones, event spawns), 7 (respawn), 8 (mini-bosses), 9 (hunts),
// 11 (kill hook contract), 12 (spatial grid), 14 (core API proposal); combat-feel.md 17.4 (elite behaviours);
// DECISIONS M1-M10, W9, D13 T9 (respawn on the sim clock).
//
// Owner area: monsters. Runtime system (SimContext). Timers: TimerOwner::Monsters (respawn T9). RNG: RngStream::Ai
// (patrol, zone-spawn / respawn / minion placement, elite affix rolls, elite teleports); SpawnAmbush (random-event
// ambush / rescue contents) draws from RngStream::Events (Rng.h stream rules).
// Kill credit: ApplyDamage publishes MonsterKilledMsg exactly once, on the alive -> dead transition, synchronously;
// GameSim's subscriptions run the kill pipeline (monsters-ai 11 order) and MonsterSystem::OnMonsterKilled is the last
// handler (log zone.monsterKill + respawn decision).
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Timers.h"
#include "abyss/base/Types.h"
#include "abyss/combat/CombatInput.h"
#include "abyss/data/QuestData.h"
#include "abyss/monsters/Corpses.h"
#include "abyss/monsters/Hunts.h"
#include "abyss/monsters/Monster.h"
#include "abyss/monsters/MonsterAI.h"
#include "abyss/monsters/Packs.h"
#include "abyss/monsters/SpatialGrid.h"
#include "abyss/sim/GameplayBus.h"
#include "abyss/sim/SimTypes.h"

namespace abyss {

struct SimContext;
struct Snapshot;
struct SaveData;
struct SubDungeonDef;

enum class MonsterTimerKind : uint16_t { Respawn = 1 };

struct MonsterSpawnParams {
  const MonsterDef* baseDef = nullptr;  // pre-difficulty definition (the system scales it)
  bool alreadyScaled = false;           // def is final (hunt / defend wave paths scale themselves)
  TilePos tile;
  MonsterRole role = MonsterRole::Regular;
  bool rollAffixes = false;             // monsters 1.3 "who rolls affixes"
  bool startChasing = false;            // ambush / rescue / defend spawns
  std::string huntId;
  double visualScale = 1.0;
  std::string affixZone;                // zone whose affix count applies ("" = the current zone; sub-dungeons: parent)
  bool hasHome = false;                 // MonsterInstance::homeTile = `home` (a map spawn's authored tile; else `tile`)
  TilePos home;
  uint32_t packId = 0;                  // K5: its pack (MonsterSystem::NewPackId; 0 = none)
  bool packLeader = false;              // K5: it leads that pack
};

// What one defend wave spawns (dark remake K1; QuestWorld builds it from the quest's DefendTargetDef). `composition`
// empty = the web rule (defendWaveBaseCount + w random picks from the zone list); `hasApproach` restricts the ring to
// the 180 deg arc centred on approachDeg (DefendTargetDef::approachDeg convention).
struct DefendWaveSpec {
  std::span<const DefendWaveEntry> composition;
  bool hasApproach = false;
  double approachDeg = 0;
};

struct DamageFlags {
  bool isCrit = false;
  bool isTick = false;
  bool hasFrom = false;
  Vec2 from;
  EntityId attacker = kNoEntity;
  KillSource source = KillSource::Other;
  bool provokes = true;  // M2: hero hits provoke
  DamageType element = DamageType::Physical;  // K3: a fire killing blow burns the body (MonsterKilledMsg::element)
};

class ABYSS_API MonsterSystem {
 public:
  explicit MonsterSystem(SimContext& ctx);

  // ---- zone lifecycle ----
  // Zone entry spawn order (monsters 6.1): zone spawns -> (NPCs etc. elsewhere) -> mini-boss -> hunts (announce=false).
  // M7 story boss (MonsterAiDef storyBoss*): the spawn entries of storyBossId get MonsterRole::StoryBoss (never
  // respawn in the visit) and are skipped when storyBossNotAfterQuestTurnIn is turned in, unless chapterCompleteQuest is
  // turned in too and storyBossFarmableAfterChapter.
  // Placement per monster (M10, FIX Q16): up to placementTries tiles spawn +/- spawnJitter (2 RngStream::Ai draws each,
  // col first, clamped to [1, size - 2]; walkable and outside every camp radius), else the anchor when walkable; elites
  // roll their affixes right after their placement (web draw order).
  void SpawnZonePopulation();
  // M7: once per visit at its fixed tile (bounds check only), always rolls affixes. In a sub-dungeon (the zone id is a
  // world SubDungeonDef id) it spawns that sub-dungeon's boss instead (8.5, later milestone).
  void SpawnMiniBoss();
  // A zone mini-boss that an open quest of this zone still needs (an unfinished Kill objective on its def id) comes back
  // when it is not alive, past once-per-visit: M7 keeps a zone from being farmed, never a quest from being finished (a
  // hero who killed it before taking the quest could otherwise not finish it before a new visit, and the Chapter 1
  // zone cannot be left in milestone 1). Like the hunts (SpawnDueHunts): trigger QuestAcceptedMsg. Its pre-fight
  // dialogue and its kill beat stay once per save. Not in sub-dungeons (their boss never respawns).
  void SpawnMiniBossForQuests();
  // The same rule for the M7 story boss (review fix of 1B part 3): when an open quest of this zone still needs it (an
  // unfinished Kill objective on storyBossId), none is alive and StoryBossBlocked() is false, its spawn entries spawn
  // again at their anchor tiles (count each, no placement draw; affixes roll as at zone entry; an unwalkable anchor is
  // skipped) with the StoryBoss role (Regular when monster_ai port.storyBoss.oncePerVisit is false). A hero who killed
  // the boss before taking its quest can still finish it in the same visit. A dead boss whose respawn timer is pending
  // (oncePerVisit false: the Regular role respawns) is left to that respawn, so there is never a second one.
  // Trigger: QuestAcceptedMsg.
  // Returns the spawned ids (empty when nothing was needed).
  std::vector<EntityId> SpawnStoryBossForQuests();
  // Sub-dungeon mini-boss (8.5): the sub-dungeon's fixed tile (bounds check against its size only), difficulty-scaled,
  // affixes rolled with the PARENT zone's count, MonsterRole::SubDungeonMiniBoss (never respawns, no dialogue tree).
  // Becomes MiniBoss(). Returns kNoEntity for an unknown boss or an out-of-bounds tile.
  EntityId SpawnSubDungeonMiniBoss(const SubDungeonDef& sd);
  void OnZoneExit();     // drops every monster (EvEntityDespawned ZoneUnload) and respawn timer (monsters are not saved)

  // ---- spawning ----
  // One monster at a tile: difficulty scaling unless alreadyScaled, stats, spawn anchor = tile (home tile = `home` when
  // hasHome, else tile), role flags (noRespawn from monster_ai.json port.noRespawn + mini-bosses / seal keepers),
  // optional affixes for the zone, EvEntitySpawned; startChasing spawns publish MonsterAggroMsg. No walkability check
  // (callers place).
  EntityId Spawn(const MonsterSpawnParams& p);
  // Quest hunts (9.4): spawns every due hunt of the current zone not present. announce -> zone.quest.huntRevealed
  // log + camera shake + EvHuntRevealed. Triggers: zone entry (false), QuestAcceptedMsg (false), QuestProgressMsg (true).
  void SpawnDueHunts(bool announce);
  // The spawning half of SpawnDueHunts for an already computed due list (HuntsToSpawn order): leader (hunt def,
  // difficulty, affixes, visual scale), minions (one placement attempt each), announce. Unknown base monsters and
  // hunts without a walkable spot are skipped (retried on the next trigger).
  void SpawnHunts(std::span<const DueHunt> due, bool announce);
  // Event spawns (6.4): count monsters picked from ids around `centre` (ring placement + findWalkableTile), chasing,
  // no affixes, noRespawn (M4/W9). Returns the new ids in spawn order. Ambush: minDist 3, distRange 2 around the event
  // point; rescue: minDist 2, distRange 3 around the rescue NPC (the caller passes count = max(2, randomInt - 1)).
  // Draws (RngStream::Events) per monster: id index floor(rand * len) (an unknown id is skipped before the next two),
  // angle rand * 2 pi, distance minDist + rand * distRange; tile = round(centre + dir * dist) clamped to [1, size - 2],
  // else the first walkable tile of rings 1..5 (dr outer, dc inner).
  std::vector<EntityId> SpawnAmbush(std::span<const std::string> monsterIds, int32_t count, Vec2 centre,
                                    double minDist, double distRange, MonsterRole role);
  // Defend wave `waveIndex` (0-based; quests 3.10, monsters 6.4) around `target`: n = defendWaveBaseCount + w monsters
  // at angles 2 pi k / n on a circle of defendWaveRadius, JsRound, clamped to [edgeMargin, size - 1 - edgeMargin], NO
  // walkability check (web, quests Q8 later); each a uniformly random def of the zone's monster list
  // (RandomInt(0, len - 1), RngStream::Ai, bosses included), difficulty then ScaleDefendWave, chasing, no affixes,
  // MonsterRole::DefendWave (never respawns). Returns the ids in spawn order (empty without a zone list).
  // K1 (`spec`): a composition spawns exactly its entries in list order (each entry `count` times; n = the sum; no RNG
  // draw; an unknown id keeps its ring slot but is skipped) instead of the random picks - the zone list is then not
  // needed. With an approach the k-th of n monsters stands at approachDeg + 180 * ((k + 0.5) / n - 0.5) degrees (evenly
  // inside the half ring facing the approach) instead of 2 pi k / n. Scaling, role, chase and clamping are unchanged.
  std::vector<EntityId> SpawnDefendWave(Vec2 target, int32_t waveIndex, const DefendWaveSpec& spec = {});

  // ---- abstract chip damage for quest actors (monsters 4.4; quests 3.9 / 3.10): no animation, RNG or defense ----
  // `apply(monster, damage)` receives each hit in order and returns false to stop the loop (the actor died: the web
  // returns from the update there). The per-monster timer is stamped before `apply` is called.
  using ChipApply = std::function<bool(EntityId monster, double damage)>;
  // Escort (vector 20): every alive AGGRO monster in list order with distSq(monster, target) < escortChipRadiusSq and
  // now - lastEscortChipMs > escortChipIntervalMs (strictly; the first hit is immediate) deals
  // max(1, floor(def.damage * escortChipDamageMul)). Returns the total dealt.
  double ChipEscort(Vec2 target, const ChipApply& apply);
  // Defend target: each alive monster of `waveMonsters` (given order, aggro or not) with distSq < defendChipRadiusSq and
  // now - lastDefendChipMs > defendChipIntervalMs (lastDefendChipMs starts at 0) deals
  // max(1, floor(def.damage * defendChipDamageMul)). Returns the total dealt.
  double ChipDefendTarget(std::span<const EntityId> waveMonsters, Vec2 target, const ChipApply& apply);

  // ---- per step (GameSim, monsters 3.9 order) ----
  // 8.3: before the AI loop. Opening the mini-boss panel is a core-owned modal (PanelId::MiniBossDialogue): it blocks
  // hero gameplay input (U7) but does not freeze the sim (D13: it holds only the mini-boss).
  void CheckMiniBossDialogue();
  // 3.8: activity set (250 ms), safe-zone repel, immobilized skip, AI, grid update. Every Idle / Patrol / Returning ->
  // Chase transition publishes MonsterAggroMsg (A7 aggro vocalisation). The activity set is the monsters within
  // aiCullRadius of the hero (grid order) plus every aggro AND every Returning (M1) monster anywhere (list order), so a
  // leashed monster always finishes its walk home. K2: a monster winding up an ability (now < abilityRootedUntilMs) is
  // skipped like an immobilized one, and a leash (M1) stops its ability clock (MonsterInstance::abilityClockStarted).
  // K4: so is one playing a phase's enter action (now < phaseActionUntilMs); a monster with a phase table that arrives
  // home resets its phases (header "K4").
  void TickAI(double dtMs);
  void TickEliteBehaviours();    // 17.4: teleporting + curse aura
  // FIX Q19: prunes every monster's buffs (list order; no publish, so no re-entrancy).
  void PruneBuffs(double nowMs);
  void OnTimer(const Timer& t);
  void DismissMiniBossDialogue();  // 8.3 onDismiss: boss -> chase; EvMiniBossDialogue{opened = false}

  // ---- damage (5) ----
  // takeDamage: dead -> Tick weight, nothing else. Else lastDamaged = now; hp = max(0, hp - amount); weight =
  // ClassifyHit; lastHitFrom; hp <= 0 -> state Dead, statuses NOT cleared here (kill pipeline does it),
  // MonsterKilledMsg published. A surviving monster with flags.provokes runs ProvokeMonster (MonsterAI.h, the M2
  // contract: idle / patrol, or chase with the hero beyond aggroRange; Returning ignores it) and publishes
  // MonsterAggroMsg when that took it out of Idle / Patrol.
  HitWeight ApplyDamage(EntityId id, double amount, const DamageFlags& flags);
  // heal(): ignored if dead, amount <= 0 or full (FIX: also used by vampiric).
  void Heal(EntityId id, double amount);
  void Teleport(EntityId id, Vec2 to, TeleportReason reason);
  // Forced state changes from other areas (3.10): taunt (idle/patrol -> chase), ambush spawns, mini-boss dialogue.
  void ForceChase(EntityId id);
  // Boss intro rename (monsters 10 / story 8.3): the StoryDirector's 250 ms scan marks the nearest live instance of a
  // story boss within the bar range once; the snapshot then shows the intro name. True when newly marked (the caller
  // emits EvMonsterRenamed); false for unknown / dead / already renamed monsters. A respawn starts unmarked.
  bool MarkStoryNamed(EntityId id);

  // ---- kill pipeline (last handler) ----
  void OnMonsterKilled(const MonsterKilledMsg& m);

  // ---- dark remake K3: corpses, raises, the Unburied rise, consume, collapse (implementation-plan.md 3 K3) ----
  // The zone's corpse registry (Corpses.h) is cleared at zone entry / exit; SpawnZonePopulation seeds the map's
  // corpseSeeds after the population (EvCorpseAdded{seeded}).
  //
  // Kill pipeline K3 step (SimWiring, right after the quest drops; before CombatSystem::OnMonsterDeathEffects): the dead
  // monster's corpse holds are let go, its body joins the registry when its definition leaves one (corpse.leaves; a fire
  // killing blow registers it burned: EvCorpseAdded then EvCorpseRemoved{burned}; a Raised body's is spent), its
  // Unburied rise is scheduled (the first of its affixes with riseAgain, else the definition's trait; RiseAgainApplies;
  // chance < 1 draws once on RngStream::Ai; never for a Raised or a boss role - mini-boss, sub-dungeon mini-boss, story
  // boss, seal keeper, hunt leader -; the pending rise holds the corpse), then every Raised it stood up collapses.
  void OnMonsterDied(const MonsterKilledMsg& m);
  // Per step (GameSim, right after CombatSystem::TickOnDeath): expired bodies go (EvCorpseRemoved{expired}); consume
  // channels end (aborted when the eater is stunned / frozen or its corpse is gone: EvMonsterAttackCancelled; finished:
  // it heals floor(maxHp x consumeHealFraction) and the corpse goes, EvCorpseRemoved{consumed, by}); due rises stand up
  // when their corpse is still there (else nothing: fire, a raise or an eater got it first).
  void TickCorpses();
  // A raise's contact: `base` (pre-difficulty) stands up as Raised on corpse `corpseId` at hpPct of its life for `raiser`
  // (EvCorpseRemoved{raised, raiser}, EvEntitySpawned, EvMonsterRaised, EvPlayAnim{Rise}; rooted for the Rise clip;
  // chasing). kNoEntity (nothing) when the corpse is gone.
  EntityId RaiseCorpse(uint32_t corpseId, const MonsterDef& base, EntityId raiser, double hpPct, std::string_view abilityId);
  // A summon's / an on-death spawn's body at `pos` (EvMonsterRaised with corpseId 0), else as RaiseCorpse.
  EntityId SummonAt(const MonsterDef& base, Vec2 pos, EntityId raiser, double hpPct, std::string_view abilityId);
  // Every body (corpse or remains, seeds too) whose 0.35-tile body touches the shape burns: EvCorpseRemoved{burned}.
  // Returns how many burned.
  int32_t BurnCorpses(const AbilityShapeDef& shape, Vec2 origin, Vec2 dir);
  // Raised bodies standing for `raiser` (alive).
  int32_t RaisedAliveOf(EntityId raiser) const;
  // A Raised body falls without a kill because its raiser died: EvPlayAnim{Death}, EvMonsterCollapsed, statuses and its
  // pending swing dropped (CombatSystem::OnMonsterCollapsed), its corpse (spent) when it leaves one, then its own Raised.
  // No kill credit, no on-death effect, no rise, no respawn.
  void Collapse(EntityId id);
  // ---- dark remake K4: boss phases and arenas (implementation-plan.md 3 K4; rules in monsters/Phases.h) ----
  // Per step (GameSim, right after TickCorpses), no RNG:
  //  1. every living monster with a phase table enters the rows its life crossed (NextPhaseToEnter, one at a time, in
  //     order: a hit that crosses two thresholds enters both). Entering row i: phase = i + 1; the row's multipliers and
  //     tether replace the previous row's (x the affixed definition); abilitiesOn / abilitiesOff switch (an ability the
  //     entry makes usable is ready initialCooldownMs after it, when the fight's ability clock runs); with an
  //     enterAction, its swing or ability on its way to contact and a consume channel are cancelled
  //     (EvMonsterAttackCancelled); EvMonsterPhase; the action plays (EvPlayAnim{enterAction, its clip}) and roots it -
  //     no AI step, swing or cast - for the clip's length (assets.json), else MonsterAiDef::phaseActionMsDefault, while
  //     every hit still lands (no invulnerable phase, monsters.md 1.3); its arena lights (EvArena, replacing an older
  //     one); its summons are queued (delayMs).
  //  2. arenas tick (list order): every arenaTickMs from the entry, the hero outside the ring (Phases.h ArenaOutside at
  //     the tick's radius) takes CombatSystem::ArenaTickHero while the monster is in its fight (chase / attack) and the
  //     hero is not in a camp's safe zone.
  //  3. queued phase summons stand up (SummonAt: Raised, raiser = the monster, abilityId "phase.<n>"); a body of a
  //     summon with respawns, killed while its phase still holds, is queued again respawnMs later (same slot).
  //  4. Feeds come due: a corpse still there is consumed (EvCorpseRemoved{consumed, by the monster}) and the monster
  //     heals floor(maxHp x healPct / 100); otherwise nothing.
  // Feed starts in the kill pipeline (OnMonsterDied): a Raised kill whose body leaves an unburned corpse within the
  // current row's feed radius of its centre is held for the first such monster (list order; EvMonsterFeed).
  // The M1 reset (TickAI: a monster with a phase table arrives home): full life, phase 0 (EvMonsterPhase{reset} when it
  // had entered a row), multipliers, tether and switches back to phase 0, its arena out (EvArena{cleared}), its queued
  // summons and Feeds dropped, every Raised it stood up collapses. Its death or collapse puts its arena out and drops its
  // queued work too; a zone change drops everything silently.
  void TickPhases();
  // How long a phase's enter action roots `def`: its art's clip for the action (Phases.h PhaseActionClip) when the
  // manifest has it, else MonsterAiDef::phaseActionMsDefault.
  double PhaseActionMs(const MonsterDef& def, AnimAction action) const;

  // ---- dark remake K5: packs, morale, kite, block, buffs and auras (implementation-plan.md 3 K5; rules in
  // monsters/Packs.h, the kite and flee steps in MonsterAI.h "K5") ----
  // Packs: SpawnZonePopulation spawns each map spawn entry as one pack (its members in mix order, Packs.h
  // ExpandSpawnMembers, each placed by the same M10 draws as before; the leader is MonsterInstance::packLeader and, when
  // the entry's leader says elite, rolls the zone's affixes on the Ai stream right after its placement, as an elite
  // definition does, so every later placement of the zone moves); SpawnHunts a hunt's leader with its minions (the
  // leader leads). A respawn keeps its pack and leadership. Event spawns, Raised and summons have no pack.
  // Pack aggro (MonsterDef::packAggro): when a pack member is provoked by a hit (M2, ApplyDamage), enters Chase from
  // Idle / Patrol / Returning (TickAI) or is forced to (ForceChase), every other living member of its pack with
  // packAggro, standing within its packAggro of that member, is provoked (ProvokeMonster; MonsterAggroMsg when it leaves
  // Idle / Patrol), once: a member provoked so does not pass the alarm on, and neither does a call.
  // Morale (kill pipeline K5 step, OnMonsterDied, after the dead monster's buffs ended): a pack leader's death breaks
  // its pack: each living member with `morale` (in list order; not one walking home, not one already fleeing: its flee
  // runs on unchanged), within fleeRadius of the body (0 = any), neither fearImmune (MonsterFearImmune) nor steadied by
  // a sustain (Sustained), flees: its swing / ability on the way and a consume channel are cancelled
  // (EvMonsterAttackCancelled), state Flee until now + fleeOnLeaderDeathMs away from the body (EvMonsterFlee). TickAI
  // ends a flee that a sustain or a fearImmune buff now steadies (Chase, EvMonsterFlee{ended, sustained}); the AI ends
  // the rest (expired, blocked; interrupted by the leash or a camp's safe zone, which also drops a fleeing monster to
  // Idle; the flee step itself never enters a safe zone). A fleeing monster stays in the activity set, never swings,
  // casts or eats, and is not aggro.
  // Buffs and auras: TickAI first gives every living monster the auras around it this step (each living aura owner, list
  // order, reaches the living monsters within its radius, itself included, of its family; the largest of a stat holds),
  // then refreshes every monster's multipliers (Packs.h RefreshMonsterBuffs: live buffs and auras; the largest of a stat
  // applies), which hold for the step: attack speed (the swing interval), move speed; damage and fearImmune are also
  // read live at their use (Packs.h). A buff ends with its caster: the kill pipeline's K5 step drops every buff a dead
  // monster gave (monsters.md 2.3 "while he drums": kill the drummer, then the leader) before the morale step.
  // Shield wall: after the AI loop, living blockers with block.wall > 0 that are fighting (Chase / Attack) and stand
  // within the smaller of their walls of each other (chained, list order) share one heading (Packs.h WallHeading); an
  // idle, patrolling, returning or fleeing blocker keeps its own and joins no wall.
  // A new pack id for the zone (1, 2, ... from the zone's entry).
  uint32_t NewPackId() { return nextPackId_++; }
  // A sustain member steadies `m` (its morale's sustainRadius > 0): a living monster other than `m`, of its
  // sustainFamily (any when empty), with a buff / call granting fearImmune (Packs.h GrantsFearImmunity), within reach.
  bool Sustained(const MonsterInstance& m) const;
  // The contact of a buff / call ability of `caster` (CombatSystem::ResolveAbility): within a.buff.radius of the caster
  // (list order; radius 0 = the caster alone) a call provokes every other living monster of its family (M2; no pack
  // alarm), and a buff with a stat gives each living monster of its family (the caster included) stat x mul until now +
  // ms (one buff per caster and stat: a recast refreshes it; the target's multipliers refresh at once); EvMonsterBuffCast.
  void ResolveBuffCast(EntityId caster, const MonsterAbilityDef& a);

  const Corpses& CorpseRegistry() const { return corpses_; }
  void QueryCorpses(Vec2 centre, double radius, CorpseFilter filter, std::vector<uint32_t>& out) const {
    corpses_.Query(centre, radius, filter, out);
  }
  bool HoldCorpse(uint32_t id, EntityId holder) { return corpses_.Hold(id, holder); }
  void ReleaseCorpse(uint32_t id, EntityId holder) { corpses_.Release(id, holder); }
  // Where body k of n of a summon around `centre` stands: the tile at angle 2 pi k / n on a ring of `radius` (rounded;
  // radius 0 = the centre's tile), else the nearest walkable tile within 3 rings, else `centre`. No RNG.
  Vec2 SummonSpot(Vec2 centre, double radius, int32_t k, int32_t n) const;
  // How long a risen body of `def` is rooted: its art's Rise clip length (assets.json), else MonsterAiDef::riseMsDefault.
  double RiseMsFor(const MonsterDef& def) const;

  // ---- queries (alive filtering is explicit) ----
  MonsterInstance* Find(EntityId id);
  const MonsterInstance* Find(EntityId id) const;
  std::span<const MonsterInstance> All() const { return monsters_; }  // includes dead entries until respawn/unload
  // Alive monsters within radius of a point, grid order (12).
  void QueryAlive(Vec2 centre, double radius, std::vector<EntityId>& out) const;
  EntityId NearestAlive(Vec2 from, double maxRange) const;
  EntityId NearestAggro(Vec2 from) const;          // state chase/attack, whole map
  EntityId MonsterAtTile(Vec2 tile) const;         // findMonsterAt: |dcol| < 1.5 && |drow| < 1.5, first alive
  EntityId NearestAliveOfDef(std::string_view defId, Vec2 from) const;  // quest guide / boss scan
  bool AnyAttacking() const;                        // combat state (9.6)
  // TargetCandidate list (combat targeting helpers) of every monster in list order.
  void Candidates(std::vector<TargetCandidate>& out) const;
  EntityId MiniBoss() const { return miniBoss_; }
  bool MiniBossDialogueActive() const { return miniBossDialogueActive_; }
  bool IsHuntPresent(std::string_view huntId) const;

  const SpatialGrid& Grid() const { return grid_; }
  // The walkability / A* service the AI and every placement use (built from ZoneRuntime at zone entry).
  const MonsterWorld& World() const { return world_; }
  // Test hook: replaces the zone's walkability / A* service (bounds included) until the next zone entry.
  void SetWorldForTesting(MonsterWorld world);
  void FillSnapshot(Snapshot& out) const;
  // miniBossDialogueSeen (save 3.2).
  void WriteSave(SaveData& out) const;
  void ReadSave(const SaveData& in);

 private:
  void Respawn(EntityId deadId);
  bool RespawnPending(EntityId deadId) const;  // a MonsterTimerKind::Respawn timer for this dead instance is queued
  // K3 helpers (header "K3").
  struct PendingRise {
    uint32_t corpse = 0;
    EntityId dead = kNoEntity;
    double dueMs = 0;
    double hpPct = 100;
    MonsterDef def;          // the dead instance's current definition (affixes applied)
    MonsterDef originalDef;
    std::vector<MonsterAffix> affixes;
    double maxHp = 0;
    double visualScale = 1;
  };
  void SeedCorpses();
  // Registers the body of `m` at `pos`; returns its corpse id (0 when the definition leaves none). Evictions emit
  // EvCorpseRemoved{expired} after the EvCorpseAdded.
  uint32_t AddBody(const MonsterInstance& m, Vec2 pos, bool spent);
  void RemoveCorpse(uint32_t id, CorpseRemoveReason reason, EntityId by);
  // Lists a Raised body (role, raiser, life, root for the Rise clip, chasing) and emits its rise events.
  EntityId StandUp(MonsterInstance&& m, Vec2 pos, uint32_t corpseId, EntityId raiser, std::string_view abilityId,
                   bool selfRise);
  // A monster that consumes may start eating (TickAI): true when it did (it skips its AI tick). Never while returning,
  // casting or with a swing on its way to contact (CombatSystem::HasPendingStrike: the swing lands first).
  bool TryStartConsume(MonsterInstance& m, double now);
  void EndConsume(MonsterInstance& m, bool cancelled);
  void CollapseRaisedOf(EntityId raiser);
  // K4 helpers (header "K4"). A queued or standing phase summon (one body: slot k of its summon's count).
  struct PhaseSummonEntry {
    uint32_t seq = 0;            // queue identity (a stand-up may publish; the entry is found again by it)
    EntityId owner = kNoEntity;
    int32_t phase = 0;           // the phase that queued it (1-based: row phase - 1)
    uint32_t summon = 0;         // index into the row's summons
    int32_t slot = 0;
    EntityId body = kNoEntity;   // the standing body (kNoEntity while queued)
    bool queued = true;
    bool respawn = false;        // queued again after a kill: only while its phase still holds
    double dueMs = 0;
    int32_t respawnsLeft = 0;
  };
  struct PendingFeed {
    EntityId owner = kNoEntity;
    uint32_t corpse = 0;
    double dueMs = 0;
    double healPct = 0;
  };
  void EnterPhase(MonsterInstance& m, int32_t row);
  void ResetPhases(MonsterInstance& m);
  // The multipliers and tether of `m`'s current phase (1 / none at phase 0).
  void ApplyPhaseRow(MonsterInstance& m);
  void ClearArena(MonsterInstance& m, bool emit);
  // Drops the queued summons, the standing-summon bookkeeping and the Feeds (their holds let go) of `owner`.
  void DropPhaseWork(EntityId owner);
  // Kill pipeline K4 step (OnMonsterDied): the dead monster's arena and queued work, its summon bookkeeping, Feed.
  void OnPhaseDeath(const MonsterKilledMsg& msg, uint32_t corpse);
  void StandPhaseSummon(PhaseSummonEntry& e);
  // K5 helpers (header "K5").
  void PackAlarm(EntityId from);
  void OnPackDeath(const MonsterKilledMsg& msg);
  void DropBuffsOf(EntityId source, double now);
  void StartFlee(EntityId id, EntityId leader, Vec2 from, double now);
  void EndFlee(MonsterInstance& m, FleeEnd reason);
  void RefreshBuffsAndAuras(double now);
  void AlignShieldWalls();
  bool InSafeZone(Vec2 p) const;
  bool Walkable(int32_t col, int32_t row) const;
  void PrepareZone();      // zone entry: clears the population, sizes the grid, builds the world service
  void EnsureZoneGrid();   // lazily sizes the grid / world for the live zone (spawns before SpawnZonePopulation)
  void BuildWorld();
  bool RoleNeverRespawns(MonsterRole role) const;
  bool StoryBossBlocked() const;  // M7: the story boss stays away after its quest (until the chapter is done)
  // An Active quest of the current zone has an unfinished Kill objective on `defId`.
  bool OpenQuestNeedsKill(std::string_view defId) const;
  // A fresh instance from a final (scaled) definition at a tile (stats, hp, anchor, role flags); not yet listed.
  MonsterInstance MakeInstance(const MonsterDef& def, TilePos tile, MonsterRole role);
  void RollAffixesInto(MonsterInstance& m, std::string_view zoneId);
  EntityId AddInstance(MonsterInstance&& m, bool startChasing);
  void PublishAggro(const MonsterInstance& m, MonsterState previous);
  void EmitSpawned(const MonsterInstance& m);
  std::string CurrentZoneId() const;

  SimContext& ctx_;
  std::vector<MonsterInstance> monsters_;  // spawn order; a respawn replaces the dead entry in place (7)
  // id -> index into monsters_, sorted by id (Find is O(log n); monsters_ order stays the spawn order).
  std::vector<std::pair<EntityId, uint32_t>> index_;
  void IndexAdd(EntityId id, uint32_t idx);
  void IndexRemove(EntityId id);
  int32_t IndexOf(EntityId id) const;
  SpatialGrid grid_;
  std::vector<EntityId> active_;           // activity set (3.8)
  // Per-step scratch (capacity kept across steps; no heap traffic in TickAI / TickEliteBehaviours).
  std::vector<EntityId> scratchIds_, scratchNear_, scratchEliteIds_;
  std::vector<Vec2> scratchNeighbours_;
  double nextActiveRefreshMs_ = 0;
  bool activeDue_ = true;                  // the first activity refresh of a zone is immediately due
  MonsterWorld world_;
  bool worldOverride_ = false;
  std::string preparedMapId_;
  int32_t preparedCols_ = 0, preparedRows_ = 0;
  EntityId miniBoss_ = kNoEntity;
  EntityId miniBossDialogueMonster_ = kNoEntity;
  bool miniBossSpawnedThisVisit_ = false;
  bool miniBossDialogueActive_ = false;
  std::vector<std::string> miniBossDialogueSeen_;
  std::vector<std::pair<std::string, EntityId>> huntLeaders_;  // huntId -> leader
  // K3: the zone's corpse registry and the pending Unburied rises (schedule order).
  Corpses corpses_;
  std::vector<PendingRise> rises_;
  std::vector<uint32_t> scratchCorpses_;
  // K4: phase summons (queue order) and pending Feeds (schedule order).
  std::vector<PhaseSummonEntry> phaseSummons_;
  std::vector<PendingFeed> feeds_;
  uint32_t nextPhaseSummonSeq_ = 1;
  // K5: the zone's next pack id; per-step scratch (capacity kept).
  uint32_t nextPackId_ = 1;
  std::vector<EntityId> scratchPack_;
  std::vector<size_t> scratchWall_;
  std::vector<size_t> scratchWallRoot_;
  std::vector<Vec2> scratchWallHeadings_;
};

}  // namespace abyss
