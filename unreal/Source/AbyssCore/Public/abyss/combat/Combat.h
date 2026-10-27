// CombatSystem: the hero's combat loop and every hit application.
// Spec: combat-feel.md sections 4 (hero basic attack), 5 (monster swings and hit application), 6 (skills), 8 (dodge),
// 9 (targeting, auto-battle, combat state), 11 (feedback events), 13 (death); classes-stats-skills.md sections 4.2
// (Life Regen), 6.6 (passives), 9 (skill pipeline), 11 (buffs), 12.1-12.2 (steal, mana shield), 13 (statuses),
// 16 (per-frame order); DECISIONS C1 (all recommended fixes), C3, C4 (charge dash, persistent ground effects, chain
// stagger, multishot cone), C5, C7 (click-to-attack stops at range), C8, S6/S7, D13 (T1-T7, T11, T12, F1-F2),
// C12 + save-ui-input 5.1.1 (Dying gate).
//
// Owner area: hero+combat. Runtime system (SimContext). Uses: Hero, StatusEffectSystem, MonsterSystem (positions,
// ApplyDamage, Heal), ProjectileSystem, HeroLocomotion (path / hold-move state), ZoneRuntime (walkability),
// EventSink, GameplayBus, TimerQueue (TimerOwner::Combat), RngStream::Combat (+ Loot for kill gold).
//
// Kill credit: MonsterSystem::ApplyDamage publishes MonsterKilledMsg exactly once; CombatSystem::OnMonsterKilled is
// the combat part of the kill pipeline (subscribed first by GameSim): clear statuses, Spirit 'kill', exp and gold
// through RewardService (ExpSource::Kill, GoldReason::Kill), killHealPercent, floating texts, target cleanup
// (LastKillReward() keeps the exp / gold for MonsterSystem's zone.monsterKill line).
//
// Port decisions taken here (beyond the spec text):
// * C5: a frozen / stunned hero cannot start a basic attack, cast, dodge or teleport, and a strike contact / skill
//   release that comes due while immobilized fizzles (the monsters' "stun interrupts the swing" rule). Hero slow only
//   scales movement (HeroLocomotion), like a monster's.
// * D13 F2 is applied (a cinematic cancels pending monster contacts); S7's "not because of a cinematic" is read as "no
//   contact-time cinematic check", since the sim clock cannot run during one.
// * C14 cast target rule (replaces the web's preferredTarget and FIX Q7): the target is resolved ONCE, at the press
//   (ResolveCastTarget: explicit tap / lock, else the hovered monster, else the nearest alive monster within the cast
//   reach max(range + 2, 8) - the same auto-pick the ring previews without a lock and auto-battle acquires with); a
//   pick becomes the selection for every shape but Self / Blink / Nova unless the cast cannot reach it; a buffered
//   press keeps its target. Every shape aimed at a target needs it within its release reach (melee / dash range + 1,
//   else the cast reach): beyond, the press logs zone.combat.outOfRange and spends nothing. The release uses that target
//   while it lives (ranged shapes regardless of its distance by then; a Melee strike whiffs when it stepped out of
//   reach). A target that died before the release is re-resolved without the hover (Ground / Trap / Cone / Line aim at
//   its last position instead); a press without a target keeps its no-target aim. No target: Projectile / Chain / Cone
//   / Line fire along the cursor direction (else the facing; a projectile fizzles without its impact shake), Ground
//   lands at the cursor point (else ahead), Trap at the cursor point (else the hero's feet); Melee / Dash / Instant log
//   zone.combat.noTarget. The hero faces the target or the fallback point before the cast.
// * C4 ground effects ("the same total damage spread over their tick count"): the first tick that reaches a monster
//   rolls the web's one-shot hit for it ONCE (calculateDamage, combustion, crit-bonus consumption, Spirit 'hit', the
//   skill's status rules from the full damage); tick k of n then deals GroundTickShare(total, k, n) =
//   floor(total (k+1) / n) - floor(total k / n) of it (life / mana steal likewise), so the ticks of a target that
//   stands in the effect from the first tick sum to exactly the web's hit. A dodged roll misses for the whole effect.
//   A tick share of 0 deals nothing that tick. A Charge whose target already stands within melee range hits at once.
// * C4 chain lightning: link k lands at release + k x 55 ms; the AoE batch shake (6.5) follows the LAST link, counting
//   every link that landed.
// * C8 teleport on touch: the "locked target" fallback is the player's own lock (a click / tap / target cycle, or the
//   aim's tapped target), never a C14 hover / auto-pick or auto-battle's lock although they also hold
//   hero.attackTarget; teleport never resolves a target and keeps the lock (C14).
// * Audio: every hit emits a cue (SfxForCombatHit; hero damage keeps the web's hit / crit cue and adds the A6
//   player_hurt cue; the A7 monster_hurt vocal on non-lethal, non-tick monster hits). A miss is audible only for a
//   monster swing into the dodge-roll i-frames (audio 3.2); a monster's stat dodge of a hero hit and the hero's stat
//   dodge stay silent (web).
// * Camera shakes (11.5): hit / player-hit / AoE shakes on ShakeChannel::Hit, the skill-authored shakes of
//   hit_feedback skillShakes on ShakeChannel::Skill (at the skill's EvSkillVfx + delayMs, or at the projectile / fall
//   impact), one shared no-override rule (ShakeThrottle).
// * C7 / 9.1: a monster click always replaces the walk order with an approach that stops at attack range (at once when
//   the monster is already in range); auto-battle paths only when the target is beyond attack range (9.4).
// * Art ids: contact / release beats read the manifest asset UE plays (HeroArtGameId / MonsterArtGameId).
// * Hero death in a sub-dungeon (zone id = a world SubDungeonDef id) or a labyrinth floor (dungeon_floor_*) takes the
//   penalty permanently (SoulEchoSystem::OnHeroDied inDungeon, combat 15).
// * TickPassives computes the step's regen modifiers (Hero::stepRegen, pre-movement campfire) and applies Life Regen
//   (Hero::TickLifeRegen) before the Unyielding check; TickHeroUpdate applies mana / HP regen (Hero::TickRegen).
// * Dark remake K2 monster abilities (implementation-plan.md 3 K2; rules in monsters/MonsterAbilities.h): a monster
//   swing that comes due (Attack state, interval elapsed) starts the ability clock on the first swing opportunity of a
//   fight, then tries the monster's abilities unless a pet's taunt holds it (PetCompanion::TauntHolds: the taunt draws
//   the swing to the beast as a basic swing) and before the pet's stray-swing draw: the pick puts the ability on its
//   cooldown, sets lastAttackMs, resolves the aim once (MonsterInstance::abilityCast), roots the monster until contact
//   (MonsterSystem skips its AI tick) and emits EvPlayAnim (the ability's clip and beat; it is the plan's
//   EvMonsterAnim) then EvTelegraph for a ground shape. The ability is a PendingStrike (MonsterStrikeContact timer) and
//   resolves at contact like a swing: it aborts when the caster dies (EvMonsterAttackCancelled) or is stunned / frozen
//   at any moment of the windup (EvMonsterAttackCancelled at the next TickCombat, or at contact; S7), or the hero is
//   dead / the zone changes (silently, at contact); F2 cancels it with the swings.
//   At contact a ground shape hits the hero whose body touches it (ShapeContains, the shape placed at windup start)
//   and, with hitsMonsters, every other living monster in it (x monsterDmgMul, no provoke); a bolt launches a monster
//   bolt locked on the hero (ProjectileSpec::payload = ability index + 1) that hits on arrival; a hazard (kind hazard,
//   or what a strike leaves) joins the HazardField (EvHazard) and ticks from TickHazards. A monster's hits use its
//   damageType (an ability's own when set) through the formula's resistance step and are capped at
//   MonsterHitCap(hero max life, def.hitCapHeroMaxHpPct, the difficulty's damageMul); an ability hit rolls the
//   ability's status instead of the def's onHitStatus (elite on-hit effects apply to both). Hazard ticks deal their
//   fixed damage less the hero's resistance (no roll, no defense, no i-frames: like a DoT tick), capped the same way;
//   unlike a status DoT tick, a lethal hazard tick checks Death Save (it is monster damage).
// * Dark remake K3 on-death effects, raises and burning (implementation-plan.md 3 K3; rules in monsters/OnDeath.h,
//   bodies in monsters/Corpses.h through MonsterSystem). The kill pipeline's K3 step (SimWiring, after MonsterSystem::
//   OnMonsterDied) schedules the dead monster's onDeath, then each of its affixes' (elite_affixes.json), then the zone's
//   (SetZoneOnDeath: the Labyrinth's volatile curse, VolatileCurseOnDeath) at the death spot, each with its damage fixed
//   at the death (OnDeathBaseDamage) and an EvTelegraph{source OnDeath} for a fuse; TickOnDeath (GameSim, after
//   TickHazards) resolves the due ones in (time, schedule) order: a burst / ring hits the hero whose body touches it
//   (dodge-roll i-frames avoid it; else the fixed damage less the hero's resistance, capped by OnDeathHeroCap, then its
//   status; Death Save holds: monster damage) and, with hitsMonsters, every living monster in it (raw, x monsterDmgMul,
//   no provoke, KillSource::Environment); a puddle joins the HazardField (first pulse at once; both caps); a spawn stands
//   its bodies up as Raised without a raiser. A cinematic does not cancel a fuse (the clock waits); a zone change drops
//   them. Raise / summon abilities run now: the pick holds the bodies they would stand up (PlanSummon: count, the
//   caster's live Raised under capAlive, the bodies it stood up within windowMs under perWindowCap; a raise's free
//   corpses within its radius in query order; a summon's spots on its anchors or a ring around it) and draws a marker
//   per corpse / spot (EvTelegraph kind Raise / Summon); at contact the bodies still there stand up (capAlive checked
//   again). A raise / summon is a timed cast, not a swing: besides competing on a due swing, it starts between swings
//   from Chase or Attack (TryStartSupportCast: its own cooldown, rangeMin / rangeMax to the hero, bodies found; never
//   while a swing or ability of the caster is on its way to contact, nor under a pet's taunt; such a caster's ability
//   clock starts at its first Chase / Attack step in the swing scan); every K2 abort applies and lets its corpses go.
//   Fire burns bodies: a fire killing blow (MonsterSystem), a fire ground strike at its contact, each tick of a fire
//   hazard, a fire burst / ring, and a hero fire skill's area where it lands (ground / trap / nova circles, cones, lines;
//   a meteor at its fall; a fire wall / trap on each tick). Kills of Raised bodies pay nothing: no Spirit 'kill', exp,
//   gold (no Loot draw), kill heal or kill text.
// * Dark remake K4 boss phases (implementation-plan.md 3 K4; MonsterSystem.h "K4", monsters/Phases.h): a monster's swing
//   interval is MonsterInstance::AttackIntervalMs (its definition's attackSpeed / the phase's attackSpeedMul; the swing's
//   anim is timed on it) and it starts no swing or cast while a phase's enter action roots it (phaseActionUntilMs); every
//   hit on a monster (DamageMonster: hero, pet, DoT tick, environment) is PhaseScaledDamage by its phase's
//   damageTakenMul before anything announces it; the picker skips the abilities its phase table switched off
//   (AbilityPickInput::off). CancelMonsterStrike (an enter action's interrupt) and ArenaTickHero (the ring's ticks) are
//   MonsterSystem::TickPhases' entry points.
// * Dark remake K5 packs, morale, kite and block (implementation-plan.md 3 K5; MonsterSystem.h "K5", monsters/Packs.h):
//   buff / call abilities run as support casts like a raise / summon: on a due swing they compete by priority, and between
//   swings, from Chase or Attack, TryStartSupportCast starts them on their own cooldown and hero range (not bound by the
//   swing reach); one action at a time, a pet's taunt holds them, every K2 abort applies. They draw no telegraph (the
//   clip is the tell); at contact MonsterSystem::ResolveBuffCast buffs / provokes (EvMonsterBuffCast). A monster's damage
//   buff / aura is the damage formula's outgoing multiplier on its swings, ability contact hits (on the hero and, with
//   hitsMonsters, on monsters) and bolts (Packs.h MonsterDamageBuffMul, read at the hit); its hazards' and on-death
//   effects' fixed damage, and its hits on the beast, do not take it. Block: every hit on a monster with `block` (not a
//   tick; with a direction: MonsterHitRequest::hasFrom, `from` off the monster) from within its front arc
//   (Packs.h BlockArcContains on its heading) is BlockedDamage'd before the K4 damageTakenMul and emits EvBlock right
//   before the EvHit; a stunned or frozen monster does not block. A fleeing monster (MonsterState::Flee) neither swings
//   nor casts (the swing scan takes Chase / Attack only); its morale cancels the strike on its way
//   (CancelMonsterStrike).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Timers.h"
#include "abyss/base/Types.h"
#include "abyss/combat/CombatInput.h"
#include "abyss/combat/Damage.h"
#include "abyss/combat/HazardField.h"
#include "abyss/combat/HitFeedback.h"
#include "abyss/combat/Projectiles.h"
#include "abyss/combat/SkillTargeting.h"
#include "abyss/combat/StatusEffects.h"
#include "abyss/data/AudioData.h"
#include "abyss/data/SkillData.h"
#include "abyss/hero/Spirit.h"
#include "abyss/sim/GameplayBus.h"
#include "abyss/sim/SimTypes.h"

namespace abyss {

struct SimContext;
struct Snapshot;
struct MonsterInstance;
struct MonsterAbilityCast;
struct EvSkillVfx;

enum class CombatTimerKind : uint16_t {
  HeroStrikeContact = 1,  // T3: other = target monster
  MonsterStrikeContact,   // T1: entity = monster (cancelled by F2 / death / stun)
  SkillRelease,           // T4: param = pending release slot (side table)
  SkillDelayedHit,        // T4: AoE batch / per-target arrow delay / chain stagger (param = pending hit slot)
  CombatStateOff,         // 9.6: 1500 ms falling-edge debounce (true debounce)
  HeroRespawn,            // T12: 1100 ms after death
  // C4: Charge = HeroLocomotion::StartDash(to attackRange short of the target, SkillPortDef::dashDurationMs) at the
  // release beat + this timer at startMs + duration (param = pending release slot); on fire the hit resolves where the
  // hero stands, if the target is within melee reach (attackRange + skill_rules rangeSlackTiles) - a dash interrupted by
  // stun / freeze (C5) may fall short. Sim clock: a modal freeze holds both (F3).
  ChargeDashEnd,
  DeathSaveRearm,         // T11 bookkeeping (the ready time itself lives in Hero::deathSaveReadyAtMs)
  SkillShake,             // 11.5: a delayed skill-authored shake (param = hit_feedback skillShakes index)
};

// How a skill is aimed by the input layer (pointer / stick / locked target). C14 resolves the cast target from it.
struct SkillAim {
  EntityId target = kNoEntity;       // explicit target (touch tap on a monster): becomes the lock, rule (a)
  bool hasPoint = false;             // desktop cursor ground point: teleport destination, no-target aim (C14 rule 3)
  Vec2 point;
  Vec2 stickDir;                     // touch joystick / pad stick direction at press time (C8 teleport)
  EntityId hoverTarget = kNoEntity;  // desktop: the monster under the cursor at the press, rule (b)
};

// Why a cast cannot go out now (CombatSystem::EvaluateCast). NoTarget / OutOfRange are logged at the press (C14).
enum class CastGate : uint8_t { Ok, Dead, Locked, Passive, Cooldown, Mana, Immobilized, NoTarget, OutOfRange };

enum class SkillRequestResult : uint8_t {
  Locked,     // unknown / level 0 (log zone.combat.skillLocked)
  Passive,    // C1 / FIX Q4: passives are not castable
  Blocked,    // hero dead / dying / frozen world
  Executed,   // committed now (cost paid)
  Buffered,   // stored in the 180 ms input buffer (EvSkillBuffered)
};

// A hit on a monster from any source (pets, status ticks, environment); hero hits use the internal paths, which end
// here too. Resolved feel (EvHit): attackerStopMs = profile.attackerStopMs for KillSource::HeroBasic, else 0; the
// impact burst + its profile shake only when `impactBurst`; elite killed by HeroBasic -> S6 slow motion.
struct MonsterHitRequest {
  EntityId monster = kNoEntity;
  double amount = 0;
  bool isCrit = false;
  bool isTick = false;
  bool hasFrom = false;
  Vec2 from;
  EntityId attacker = kNoEntity;
  KillSource source = KillSource::Other;
  DamageType element = DamageType::Physical;
  uint32_t impactColor = 0;
  std::string skillId;
  HitNumberSlot numberSlot = HitNumberSlot::Primary;  // proc extra hits (double strike / double shot)
  bool impactBurst = false;  // hero basic attacks and skills (not death_mark / slow_trap / ticks / pets)
  bool provokes = true;      // M2 (DamageFlags::provokes); false for DoT ticks
  bool scorch = false;       // EvHit ground scorch (6.4 #7: single-target skill hits with SkillDef::scorch)
  DamageType scorchType = DamageType::Fire;
};

// Exp and gold paid by the last kill (kill pipeline step 1), for the zone.monsterKill log written by the last handler.
struct KillRewardInfo {
  EntityId monster = kNoEntity;
  int64_t exp = 0;
  int64_t gold = 0;
};

struct HeroHitRequest {
  double amount = 0;
  bool isCrit = false;
  bool isTick = false;
  EntityId source = kNoEntity;
  DamageType element = DamageType::Physical;
  bool allowDeathSave = false;  // only monster hits check deathSave (5.3 QUIRK kept)
};

enum class HeroDeathCause : uint8_t { MonsterHit, StatusTick, Environment, Other };

class ABYSS_API CombatSystem {
 public:
  explicit CombatSystem(SimContext& ctx);

  // ---- commands (rejected while frozen or Dying; save-ui-input 5.1.1) ----
  // Hotbar slot 0..5 (C3) -> skill index; passives never (C1).
  SkillRequestResult RequestSkillSlot(int32_t slot, const SkillAim& aim);
  // C14: resolves the cast target once (ResolveCastTarget; the pick becomes the selection for selecting shapes), then
  // executes now or buffers the press WITH that target (180 ms). A press that fails only for its target logs
  // zone.combat.noTarget / zone.combat.outOfRange.
  SkillRequestResult RequestSkill(int32_t skillIndex, const SkillAim& aim);
  // EvaluateCast == Ok for the target a press would resolve now (PreviewCastTarget): passives false (C1), Melee / Dash /
  // Instant need a target within their reach, the other aimed shapes one within the cast reach or none at all.
  bool CanExecuteSkill(int32_t skillIndex) const;
  // performDodge (8.1 + C8). False when blocked (dead, cooldown, immobilized, no landing tile).
  bool RequestDodge(Vec2 requestedDir);
  // Click / tap on a monster (9.1): sets the lock, emits EvTargetChanged, paths toward it stopping at attack range (C7).
  void SetAttackTarget(EntityId monster, bool approach);
  void ClearAttackTarget();
  void CycleTarget();  // 9.3
  void SetAutoCombat(bool on);
  // Hero progression commands (classes 6-7, C3): skill point investment (EvSkillLevelChanged, hotbar auto-fill),
  // stat allocation (EquipStatsDirty not needed: derived stats are recomputed every step), hotbar binding.
  bool LearnSkill(int32_t skillIndex);
  bool AllocateStat(PrimaryStat stat, int32_t points);
  bool SetHotbar(int32_t slot, int32_t skillIndex);

  // ---- per-step hooks, called by GameSim in classes 16 order ----
  void TickPassives(double dtMs);   // step 6: Life Regen, Unyielding proc (60 s), Dual Wield buff
  // step 7 minus movement (classes 4.1, 14.2): spirit drain (EvResonance end), HP/MP regen with the campfire x50 and
  // poisoned x0.5 modifiers; skipped while dead.
  void TickHeroUpdate(double dtMs);
  void ConsumeBufferedSkill();      // step 3
  void TickCombat();                // step 9: monster buffs (FIX Q19, even dying), hero buffs, swings, basic attack
  void TickStatusEffects();         // step 11: ticks then expiry; DoT kills go through MonsterSystem::ApplyDamage
  // K2: the HazardField's due ticks in (time, id) order (hero inside: damage + status; monsters inside when the ability
  // hits monsters), then expired hazards are dropped. GameSim calls it right after TickStatusEffects.
  void TickHazards();
  void TickAutoCombat();            // step 12 (C1: first skill that CanExecute, only while a living lock is held; C14
                                    // acquires that lock with the press's auto-pick)
  void TickCombatState();           // 9.6 rising edge immediate, falling edge debounced 1500 ms
  // K3: resolves the due on-death effects (header "K3"). GameSim calls it right after TickHazards.
  void TickOnDeath();
  void OnTimer(const Timer& t);
  // F1: clear the input buffer (cinematic: also the attack target). F2 (cinematic): cancel every pending
  // MonsterStrikeContact (EvMonsterAttackCancelled); the monster keeps lastAttackMs.
  void OnFreezeBegin(bool cinematic);
  void OnZoneEnter();               // cooldowns reset (Q23), indicator reset
  void OnZoneExit();                // pending combat timers dropped (hero respawn timer survives as a resolved death)

  // ---- callbacks from other systems ----
  void OnSkillProjectileArrived(const Projectile& p);
  void OnMonsterBoltArrived(const Projectile& p);
  void OnGroundEffectTick(const GroundEffect& g, int32_t tickIndex);
  void OnMonsterKilled(const MonsterKilledMsg& m);
  // K3 kill pipeline step (after MonsterSystem::OnMonsterDied): schedules the dead monster's on-death effects.
  void OnMonsterDeathEffects(const MonsterKilledMsg& m);
  // K3: a Raised body collapsed (MonsterSystem::Collapse): its statuses, pending swing / ability (EvMonsterAttackCancelled
  // for an ability) and the hero's lock on it go, as for a kill.
  void OnMonsterCollapsed(EntityId monster);
  // K4 (MonsterSystem.h "K4"): a phase's enter action cancels the monster's swing or ability on its way to contact,
  // and a swing it aimed at the beast (PetCompanion::CancelMonsterSwing) (EvMonsterAttackCancelled for each; an ability
  // lets its own held corpses go; the cooldown stays spent). False when nothing was pending.
  bool CancelMonsterStrike(EntityId monster);
  // K4: one arena tick on the hero outside the ring: Phases.h ArenaTickDamage (x the difficulty's damage multiplier)
  // less the hero's resistance, capped by `hitCapPct` (the monster's hitCapHeroMaxHpPct, MonsterHitCap); a DoT-like tick
  // (no roll, defense or i-frames) that Death Save holds. Nothing while the hero is dead or a zone change is under way.
  void ArenaTickHero(EntityId source, double dpsPctMaxHp, DamageType type, double tickMs, double hitCapPct);
  // K3: an on-death effect every monster killed in this zone leaves (`label` names it in the events, e.g.
  // "curse.volatile"); `present` false clears it. The Labyrinth runtime sets its floor's curse here
  // (VolatileCurseOnDeath); a zone exit clears it.
  void SetZoneOnDeath(const MonsterOnDeathDef& d, std::string label);

  // ---- shared hit API ----
  // takeDamage on a monster with feedback events (EvHit, floating text, impact); kill credit via MonsterSystem.
  // Every EvHit carries the resolved combat-feel 11.1 decisions (Events.h EvHit: attackerStopMs, impactBurst, melee,
  // numberSlot, scorch); CombatSystem fills them, UE only plays them. A non-lethal, non-tick hit also emits the A7
  // monster_hurt cue. The hit's events (EvHit, number, cues; a basic attack's impact too) precede ApplyDamage, so they
  // come before the death anim and the kill pipeline's events (4.2 / 13.1); a skill's impact shake follows the kill.
  HitWeight DamageMonster(const MonsterHitRequest& r);
  // Camera shake through the core throttles (11.5): per-channel throttle stamp, shared no-override. Emits EvCameraShake
  // when accepted. Other areas use ShakeChannel::Direct for their cameras.main.shake calls (quest-hunt reveal).
  bool CameraShake(const ShakeRequest& s, ShakeChannel channel);
  // Damage to the hero outside the monster-swing path (DoT ticks, elite fire, hazards): hp clamp, feedback, W3 portal
  // cancel check (>= 10 % max HP), death (KillHero). Ignored while Dying.
  void DamageHero(const HeroHitRequest& r);
  // killPlayer (13.3 + 5.1.1): pet revive hook first, then Dying once (repeat calls no-op): statuses cleared, Spirit
  // reset, soul echo / penalty (RewardService: GoldReason::DeathPenalty, ExpSource::DeathPenalty), EvHeroDied,
  // HeroDiedMsg (GameSim wiring closes every core-owned modal and cancels the town portal), respawn timer (T12, 1100 ms).
  void KillHero(HeroDeathCause cause);
  // save-ui-input 3.4 rule 3: finish a Dying hero's respawn now (menu / quit / background), no second penalty.
  void ResolvePendingDeath();

  // ---- queries ----
  EntityId AttackTarget() const;
  // The lock when the player chose it (click / tap / target cycle), else kNoEntity - never a C14 hover / auto-pick or
  // auto-battle's lock: the C8 teleport fallback.
  EntityId PlayerLock() const;
  // C14: the target a press of this skill would resolve now without a hover and without selecting it (kNoEntity for
  // teleport and when nothing is in reach): the lock, else the nearest monster within the cast reach.
  EntityId PreviewCastTarget(int32_t skillIndex) const;
  // 9.5 with C14: the lock, else the auto-pick a press would take (nearest alive within castAcquireMinTiles).
  EntityId IndicatorTarget() const;
  bool InCombat() const { return inCombat_; }
  // Exp / gold of the last kill credited by OnMonsterKilled (for MonsterSystem's zone.monsterKill log line).
  const KillRewardInfo& LastKillReward() const { return lastKill_; }
  // A monster swing is in its wind-up telegraph (0.62 x contact after the swing start, combat 10.3; a K2 ability: its
  // whole windup).
  bool IsWindingUp(EntityId monster) const;
  // A monster swing or K2 ability waiting for its contact beat (the whole strike, past the windup telegraph): one action
  // at a time (K3: no raise / summon cast and no consume channel starts while one is on its way).
  bool HasPendingStrike(EntityId monster) const;
  // K2 ground hazards of the zone (cleared on zone exit).
  const HazardField& Hazards() const { return hazards_; }
  // K3 on-death effects waiting for their delay (schedule order; cleared on zone exit).
  struct PendingOnDeath {
    uint32_t seq = 0;
    EntityId source = kNoEntity;  // the dead monster
    std::string label;            // "onDeath", "affix.<type>", the zone label
    MonsterOnDeathDef def;
    Vec2 origin;
    Vec2 dir{1, 0};
    double startMs = 0;
    double dueMs = 0;
    double damage = 0;            // OnDeathBaseDamage at the death
    DamageType type = DamageType::Physical;
    double monsterCapPct = 0;     // the dead monster's hitCapHeroMaxHpPct
    TelegraphStyle style = TelegraphStyle::Normal;
  };
  const std::vector<PendingOnDeath>& PendingOnDeathEffects() const { return onDeath_; }
  const DodgeController& Dodge() const { return dodge_; }
  const InputBuffer& Buffer() const { return buffer_; }
  // Combatant views for the damage formula (combat-feel 1.2).
  Combatant HeroCombatant() const;
  static Combatant MonsterCombatant(const MonsterInstance& m);

  void FillSnapshot(Snapshot& out) const;

 private:
  // C14: a cast's resolved target and the position it is aimed at (its position when resolved; only that last known
  // position when the target died before the release of a Ground / Trap / Cone / Line cast).
  struct CastTarget {
    EntityId id = kNoEntity;
    bool hasPos = false;
    Vec2 pos;
  };
  // A skill waiting for its release beat (T4), or a Charge waiting for its dash end (C4). Slot index = timer param.
  struct PendingRelease {
    int32_t skillIndex = -1;
    int32_t level = 0;
    CastTarget target;
    SkillAim aim;
    int32_t manaCost = 0;
    bool used = false;
  };
  // Per-batch cache of the (c) auto-pick (nearest within a radius) while nothing changes between the calls (hotbar).
  struct AcquireCache {
    bool valid = false;
    double radius = 0;
    EntityId id = kNoEntity;
  };
  // A delayed skill hit (T4): the meteor batch (all targets, then the AoE shake), a per-target arrow delay, or the rest
  // of a C4 chain lightning (links nextLink.. of `targets`, one per timer at baseMs + link x staggerMs; the AoE shake
  // after the last link counts `chainHits`, which includes the immediate link 0). Slot index = timer param.
  struct PendingHit {
    int32_t skillIndex = -1;
    int32_t level = 0;
    std::vector<EntityId> targets;
    Vec2 center;
    bool blastFrom = false;  // knock-back from `center` (ground skills), else from the hero
    bool batchShake = false;
    bool chain = false;
    size_t nextLink = 0;
    int32_t chainHits = 0;
    double baseMs = 0;
    double staggerMs = 0;
    bool used = false;
  };
  // C4 ground effect tick being resolved (nullptr = an ordinary one-shot hit).
  struct GroundTick {
    EntityId effect = kNoEntity;
    int32_t index = 0;  // 0-based tick index
    int32_t ticks = 1;
  };
  // One target's hit for a whole C4 ground effect (rolled on its first tick, shared out by GroundTickShare).
  struct GroundRoll {
    EntityId effect = kNoEntity;
    EntityId target = kNoEntity;
    bool dodged = false;
    bool crit = false;
    int32_t total = 0;       // the web's dealt damage (after combustion)
    int32_t lifeStolen = 0;  // the web's steal from that hit
    int32_t manaStolen = 0;
    // The target's first landed share carried the web's one-shot impact (burst, hurt vocal, provoke, AoE batch shake);
    // later shares land as ticks (combat 6.5 / 11.1, DECISIONS C4: the feel of one hit, spread damage).
    bool impactDone = false;
  };
  // A monster swing waiting for its contact beat (T1): telegraph state for the snapshot and the F2 cancel event. A K2
  // ability rides the same path (`ability`; its index and aim live on MonsterInstance::pendingAbility / abilityCast).
  struct PendingStrike {
    EntityId monster = kNoEntity;
    double startMs = 0;
    double windupMs = 0;
    TimerId timer = kNoTimer;
    bool ability = false;
  };
  // What a K2 ability changes on a monster hit (ApplyMonsterHit): x dmgMul, its damage type, its status instead of the
  // def's onHitStatus. A copy, so a hit that grows the monster list never reads a moved definition.
  struct MonsterHitMods {
    double dmgMul = 1;
    DamageType type = DamageType::Physical;
    bool hasStatus = false;
    StatusRule status;
  };
  // One hero hit on a monster (basic attack, proc extra hit, skill hit, ground-effect tick).
  struct HeroHitSpec {
    EntityId target = kNoEntity;
    const SkillDef* skill = nullptr;  // nullptr = basic attack
    int32_t level = 1;
    bool forceCrit = false;
    const GroundTick* ground = nullptr;  // C4 ground-effect tick (ticks > 1: shared roll)
    bool applyStatusRules = true;
    bool impactBurst = true;
    bool hasFrom = false;
    Vec2 from;
    HitNumberSlot numberSlot = HitNumberSlot::Primary;
    bool scorch = false;  // single-target skill hit: EvHit carries the skill's ground scorch (6.4 #7)
  };
  struct HeroHitOutcome {
    bool attempted = false;  // the target was alive
    bool dodged = false;
    bool landed = false;     // damage was applied (DamageMonster ran): counts for the AoE batch shake
    bool killed = false;
    int32_t damage = 0;
    bool crit = false;
    HitWeight weight = HitWeight::Tick;
    // Whether this hit evaluates the skill's status rules, and the dealt damage they read (the full one-shot hit for a
    // C4 ground tick: only on the target's first tick).
    bool rollStatuses = true;
    int32_t statusDamage = 0;
    // The hit landed with the full impact (every one-shot hit; a C4 ground effect's first landed share per target):
    // counts for the AoE batch shake. Later shares are tick-weight hits without burst or shake.
    bool impact = false;
  };

  // ---- skills ----
  int32_t CastManaCost(int32_t skillIndex) const;
  // C14 resolver (no side effects; teleport resolves nothing): (a) the explicit tap, else the lock, if alive; (b) the
  // hovered monster when `useHover`; (c) AutoPickTarget(radius). `cache` memoises (c).
  EntityId ResolveCastTarget(const SkillDef& s, const SkillAim& aim, bool useHover, double radius,
                             AcquireCache* cache = nullptr) const;
  // C14 (c), also the 9.5 ring without a lock and auto-battle's acquisition: the nearest alive monster within `radius`
  // (MonsterSystem spatial-grid order keeps ties deterministic).
  EntityId AutoPickTarget(double radius) const;
  // The pick becomes the selection (lock + ring, EvTargetChanged) when the skill's cast shape selects; it is not the
  // player's own lock (PlayerLock).
  void SelectCastTarget(const SkillDef& s, EntityId target);
  // The gates of canExecute / tryUseSkill for `target` (C1 passives false, C5 an immobilized hero cannot cast, C14 the
  // target rule per cast shape). `logFailures`: the locked / mana / teleport-CC lines (tryUseSkill).
  CastGate EvaluateCast(int32_t skillIndex, EntityId target, bool logFailures) const;
  // The target a buffered press fires with: its press-time target while alive, else re-resolved without the hover.
  EntityId BufferedCastTarget(int32_t skillIndex) const;
  // Auto-battle's extra gate (9.4 with C1, C14): never teleport; a buff only with a living lock `lock` (taunt_roar: in
  // its blast), a nova only when its blast reaches it (the aimed shapes are reach-checked by EvaluateCast).
  bool AutoSkillFits(int32_t skillIndex, EntityId lock) const;
  void TryUseSkill(int32_t skillIndex, const SkillAim& aim, EntityId target);
  CastAim MakeCastAim(const CastTarget& target, const SkillAim& aim) const;
  void ReleaseSkill(int32_t skillIndex, int32_t level, const CastTarget& target, const SkillAim& aim,
                    int32_t manaCost);
  void ReleaseTeleport(const SkillDef& s, int32_t level, const SkillAim& aim, int32_t manaCost);
  void ReleaseShadowStep(const SkillDef& s, int32_t level, EntityId target);
  void ReleaseDeathMark(const SkillDef& s, int32_t level, EntityId target);
  void ReleaseBuff(const SkillDef& s, int32_t level);
  void ReleaseAoe(const SkillDef& s, int32_t level, const CastTarget& target, const SkillAim& aim);
  void ReleaseSingle(const SkillDef& s, int32_t level, const CastTarget& target, const SkillAim& aim);
  void SlowTrapHits(const SkillDef& s, int32_t level, Vec2 center, double radius, const GroundTick* ground);
  // Shared single-target / AoE hit (classes 9.7 "apply hit"); `ground` = the C4 ground tick being resolved;
  // `singleTarget` = the 6.4 #7 path (ground scorch).
  HeroHitOutcome ApplySkillHit(const SkillDef& s, int32_t level, EntityId target, bool blastFrom, Vec2 center,
                               const GroundTick* ground, bool singleTarget = false);
  // Applies link `ph.nextLink` of a pending chain lightning and schedules the next one, or the batch shake after the
  // last (C4).
  void FireChainLink(int32_t slot);
  void ResolveGroundTick(const GroundEffect& g, const GroundTick& tick);
  GroundRoll* FindGroundRoll(EntityId effect, EntityId target);
  void DropGroundRolls(EntityId effect);  // kNoEntity = every effect
  // The live zone is a sub-dungeon or a labyrinth floor (combat 15: the death penalty is permanent there).
  bool InDungeonZone() const;
  // Where a death there respawns (world 9.5): the parent zone and its camps[0] (fallback playerStart, then (3, 3)).
  bool DungeonRespawnTarget(std::string& mapId, Vec2& pos) const;
  void ApplySkillStatuses(const SkillDef& s, int32_t level, EntityId target, double dealt);
  int32_t AllocRelease(const PendingRelease& r);
  int32_t AllocHit(PendingHit h);
  void FireDelayedHit(int32_t slot);
  void ResolveChargeDash(int32_t slot);

  // ---- hits ----
  HeroHitOutcome HeroHitMonster(const HeroHitSpec& h);
  void ResolveHeroStrike(EntityId target);
  void ResolveMonsterStrike(EntityId monster);
  // `ranged`: not a melee swing (bolts, K2 ground strikes: EvHit::melee false). `mods`: a K2 ability's hit.
  void ApplyMonsterHit(EntityId monster, bool ranged, const MonsterHitMods* mods = nullptr);
  // 2F review: a status-only ability strike (dmgMul 0) on the hero: i-frames avoid it, else its status rolls.
  void ApplyMonsterStatusOnly(EntityId monster, const MonsterHitMods& mods);
  // ---- K2 monster abilities ----
  // The ability clock of `m` (no-op once started for this fight).
  static void StartAbilityClock(MonsterInstance& m, double now);
  // K3: what a raise / summon would stand up now (header "K3"); empty = it cannot fire.
  struct SummonPlan {
    std::vector<uint32_t> corpses;  // a raise's corpses
    std::vector<Vec2> spots;        // where each body stands (a raise: its corpse's position)
  };
  SummonPlan PlanSummon(const MonsterInstance& m, const MonsterAbilityDef& a, double now) const;
  // Ends `m`'s pending ability (index, root, spots, and the holds on its own corpses only: a K4 Feed keeps its hold).
  void ClearPendingAbility(MonsterInstance& m);
  // K3: burns the bodies a fire area touches (no-op for another type).
  void BurnCorpsesIfFire(DamageType type, const AbilityShapeDef& shape, Vec2 origin, Vec2 dir);
  void ResolveOnDeath(const PendingOnDeath& e);
  // A due swing of `m` becomes an ability (true) or stays a basic swing (false). `castsOnly` (K3 / K5): the cast between
  // swings, where only a raise / summon / buff / call or (2F review) a ranged special may start.
  bool TryStartAbility(MonsterInstance& m, double now, bool castsOnly = false);
  // K3 / K5: a support cast between swings (Chase, or Attack before the swing is due): a raise / summon (header "K3")
  // or a buff / call (header "K5").
  bool TryStartSupportCast(MonsterInstance& m, double now);
  // S7: aborts every pending ability whose caster is stunned or frozen (TickCombat, before the dead-hero gate).
  void AbortImmobilizedAbilities();
  // Contact of the pending ability (PendingStrike::ability).
  void ResolveAbility(EntityId monster);
  static MonsterHitMods AbilityHitMods(const MonsterInstance& m, const MonsterAbilityDef& a);
  // hitsMonsters: every living monster but the caster whose body touches the cast shape.
  void HitMonstersInShape(EntityId caster, const MonsterAbilityDef& a, const MonsterAbilityCast& cast);
  void PlaceHazard(const MonsterInstance& m, const MonsterAbilityDef& a, const MonsterAbilityCast& cast);
  void ResolveHazardTick(const HazardEntry& h);
  double DifficultyDamageMul() const;
  // Raw HP loss with feedback (EvHit, number, SFX, shake, HeroDamagedMsg); no death check.
  void HurtHero(double amount, bool crit, bool tick, DamageType element, EntityId source, bool melee, HitWeight weight,
                double attackerStopMs);
  bool TryDeathSave();
  void EmitMiss(EntityId target, Faction faction, EntityId source, const std::string& skillId, bool iframe);
  void ApplySteal(int32_t damage, bool crit, int32_t lifeStolen, int32_t manaStolen);
  void ConsumeCritBonus();
  void GainSpirit(SpiritSource source, bool crit);
  // StatusEffectSystem::Apply + EvStatusApplied (K6: a stackCount status reports its stacks) + the applied log line.
  StatusApplyOutcome ApplyStatus(EntityId target, StatusType type, double value, double durationMs,
                                     EntityId source);
  void Shake(double durationMs, double intensity);  // ShakeChannel::Hit (VFXManager.cameraShake)
  // The skill's authored shake (hit_feedback skillShakes) if it is anchored at `at`: now, or delayMs later (timer).
  void SkillShake(const SkillDef& s, SkillShakeAnchor at);
  // EvSkillVfx for a hero skill + its VFX-anchored authored shake.
  void EmitSkillVfx(const SkillDef& s, const EvSkillVfx& v);
  void EmitSfx(SfxId cue, Vec2 pos, EntityId source);
  void StartEliteSlowMotion();

  // ---- state helpers ----
  // Sets the lock and the ring; `playerChosen` = a click / tap / target cycle (PlayerLock), false for C14 picks.
  void SetTargetInternal(EntityId target, bool playerChosen);
  void UpdateIndicator();
  void ClearIndicator();  // indicator -> none, EvTargetChanged{kNoEntity} when it showed one (zone change)
  void Respawn(bool parentZoneRestart);
  void CancelPendingStrikes(bool emitCancelled);
  bool HeroImmobilized() const;
  bool HeroAlive() const;
  DamageRules Rules() const;
  std::vector<TargetCandidate> AliveCandidates(Vec2 centre, double radius) const;
  int32_t HotbarSlotOf(int32_t skillIndex) const;

  SimContext& ctx_;
  InputBuffer buffer_;
  DodgeController dodge_;
  ShakeThrottle shake_;
  SkillAim bufferedAim_;
  EntityId bufferedTarget_ = kNoEntity;  // C14: the press-time target of the buffered request
  std::vector<PendingRelease> releases_;
  std::vector<PendingHit> hits_;
  std::vector<GroundRoll> groundRolls_;  // C4: per (effect, target), dropped after the effect's last tick
  std::vector<PendingStrike> strikes_;
  std::vector<EntityId> scratchNearby_;  // TickCombat swing scan (capacity kept)
  HazardField hazards_;                   // K2 ground hazards of the zone
  std::vector<HazardTick> scratchHazardTicks_;
  std::vector<PendingOnDeath> onDeath_;   // K3 on-death effects waiting for their delay (schedule order)
  uint32_t nextOnDeathSeq_ = 1;
  // 2F review (monsters.md 1.3): on-death damage the hero took, {sim ms, amount}, within the last kOnDeathWindowMs
  // (OnDeath.h): bursts and rings landing together share one cap.
  std::vector<std::pair<double, double>> onDeathTaken_;
  // 2F review (a held lock that steps away): the lock the hero last had in reach and the locomotion's MoveGeneration
  // then; while neither changed the hero walks to it again when it steps out of reach (TickCombat).
  EntityId reachLock_ = kNoEntity;
  uint32_t reachLockGeneration_ = 0;
  MonsterOnDeathDef zoneOnDeath_;         // K3: the zone's on-death effect (SetZoneOnDeath)
  std::string zoneOnDeathLabel_;
  bool inCombat_ = false;
  TimerId combatOffTimer_ = kNoTimer;
  TimerId respawnTimer_ = kNoTimer;
  EntityId indicator_ = kNoEntity;
  EntityId playerLock_ = kNoEntity;  // the lock the player chose (PlayerLock: only while it is still the lock)
  KillRewardInfo lastKill_;
};

}  // namespace abyss
