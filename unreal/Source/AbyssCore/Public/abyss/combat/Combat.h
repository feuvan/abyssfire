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
// * FIX Q7: at the release beat a range-checked skill whose (re)target is beyond range + slack takes the nearest
//   monster in reach, else fizzles (cost spent).
// * C4 ground effects ("the same total damage spread over their tick count"): the first tick that reaches a monster
//   rolls the web's one-shot hit for it ONCE (calculateDamage, combustion, crit-bonus consumption, Spirit 'hit', the
//   skill's status rules from the full damage); tick k of n then deals GroundTickShare(total, k, n) =
//   floor(total (k+1) / n) - floor(total k / n) of it (life / mana steal likewise), so the ticks of a target that
//   stands in the effect from the first tick sum to exactly the web's hit. A dodged roll misses for the whole effect.
//   A tick share of 0 deals nothing that tick. A Charge whose target already stands within melee range hits at once.
// * C4 chain lightning: link k lands at release + k x 55 ms; the AoE batch shake (6.5) follows the LAST link, counting
//   every link that landed.
// * C8 teleport on touch: the "locked target" fallback is the lock (hero.attackTarget, or the aim's tapped target),
//   never the preferred-target fallback (nearest monster on the map).
// * Audio: every hit emits a cue (SfxForCombatHit; hero damage -> the A6 player_hurt cue; the A7 monster_hurt vocal on
//   non-lethal, non-tick monster hits). A miss is audible only for a monster swing into the dodge-roll i-frames
//   (audio 3.2); a monster's stat dodge of a hero hit and the hero's stat dodge stay silent (web).
// * Hero death in a sub-dungeon (zone id = a world SubDungeonDef id) or a labyrinth floor (dungeon_floor_*) takes the
//   penalty permanently (SoulEchoSystem::OnHeroDied inDungeon, combat 15).
// * TickPassives computes the step's regen modifiers (Hero::stepRegen, pre-movement campfire) and applies Life Regen
//   (Hero::TickLifeRegen) before the Unyielding check; TickHeroUpdate applies mana / HP regen (Hero::TickRegen).
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
#include "abyss/combat/HitFeedback.h"
#include "abyss/combat/Projectiles.h"
#include "abyss/combat/SkillTargeting.h"
#include "abyss/combat/StatusEffects.h"
#include "abyss/data/AudioData.h"
#include "abyss/data/SkillData.h"
#include "abyss/hero/Spirit.h"
#include "abyss/sim/GameplayBus.h"

namespace abyss {

struct SimContext;
struct Snapshot;
struct MonsterInstance;

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
};

// How a skill is aimed by the input layer (pointer / stick / locked target).
struct SkillAim {
  EntityId target = kNoEntity;  // explicit target (touch tap on a monster); kNoEntity = preferred target
  bool hasPoint = false;        // pointer ground point (teleport, desktop)
  Vec2 point;
  Vec2 stickDir;                // touch joystick direction at press time (C8)
};

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
  SkillRequestResult RequestSkill(int32_t skillIndex, const SkillAim& aim);
  // canExecuteSkill (classes 9.2) with C1 fixes (passives false, Q6 range + 1 for targeted buffs, Q8 death_mark /
  // shadow_step need a target).
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
  void TickCombat();                // step 9: prune hero + monster buffs (FIX Q19), monster swings, basic attack
  void TickStatusEffects();         // step 11: ticks then expiry; DoT kills go through MonsterSystem::ApplyDamage
  void TickAutoCombat();            // step 12 (C1: first skill that CanExecute)
  void TickCombatState();           // 9.6 rising edge immediate, falling edge debounced 1500 ms
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

  // ---- shared hit API ----
  // takeDamage on a monster with feedback events (EvHit, floating text, impact); kill credit via MonsterSystem.
  // Every EvHit carries the resolved combat-feel 11.1 decisions (Events.h EvHit: attackerStopMs, impactBurst, melee,
  // numberSlot); CombatSystem fills them, UE only plays them. A non-lethal, non-tick hit also emits the A7
  // monster_hurt cue.
  HitWeight DamageMonster(const MonsterHitRequest& r);
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
  EntityId PreferredTarget() const;   // 9.2
  EntityId IndicatorTarget() const;   // 9.5
  bool InCombat() const { return inCombat_; }
  // Exp / gold of the last kill credited by OnMonsterKilled (for MonsterSystem's zone.monsterKill log line).
  const KillRewardInfo& LastKillReward() const { return lastKill_; }
  // A monster swing is in its wind-up telegraph (0.62 x contact after the swing start, combat 10.3).
  bool IsWindingUp(EntityId monster) const;
  const DodgeController& Dodge() const { return dodge_; }
  const InputBuffer& Buffer() const { return buffer_; }
  // Combatant views for the damage formula (combat-feel 1.2).
  Combatant HeroCombatant() const;
  static Combatant MonsterCombatant(const MonsterInstance& m);

  void FillSnapshot(Snapshot& out) const;

 private:
  // A skill waiting for its release beat (T4), or a Charge waiting for its dash end (C4). Slot index = timer param.
  struct PendingRelease {
    int32_t skillIndex = -1;
    int32_t level = 0;
    EntityId target = kNoEntity;
    SkillAim aim;
    int32_t manaCost = 0;
    bool used = false;
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
  };
  // A monster swing waiting for its contact beat (T1): telegraph state for the snapshot and the F2 cancel event.
  struct PendingStrike {
    EntityId monster = kNoEntity;
    double startMs = 0;
    double windupMs = 0;
    TimerId timer = kNoTimer;
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
  };

  // ---- skills ----
  int32_t CastManaCost(int32_t skillIndex) const;
  bool SkillUsableNow(int32_t skillIndex, bool logFailures, EntityId* outTarget) const;
  void TryUseSkill(int32_t skillIndex, const SkillAim& aim);
  void ReleaseSkill(int32_t skillIndex, int32_t level, EntityId target, const SkillAim& aim, int32_t manaCost);
  void ReleaseTeleport(const SkillDef& s, int32_t level, EntityId target, const SkillAim& aim, int32_t manaCost);
  void ReleaseShadowStep(const SkillDef& s, int32_t level, EntityId target);
  void ReleaseDeathMark(const SkillDef& s, int32_t level, EntityId target);
  void ReleaseBuff(const SkillDef& s, int32_t level);
  void ReleaseAoe(const SkillDef& s, int32_t level, EntityId target);
  void ReleaseSingle(const SkillDef& s, int32_t level, EntityId target);
  void SlowTrapHits(const SkillDef& s, int32_t level, Vec2 center, double radius, const GroundTick* ground);
  // Shared single-target / AoE hit (classes 9.7 "apply hit"); `ground` = the C4 ground tick being resolved.
  HeroHitOutcome ApplySkillHit(const SkillDef& s, int32_t level, EntityId target, bool blastFrom, Vec2 center,
                               const GroundTick* ground);
  // Applies link `ph.nextLink` of a pending chain lightning and schedules the next one, or the batch shake after the
  // last (C4).
  void FireChainLink(int32_t slot);
  void ResolveGroundTick(const GroundEffect& g, const GroundTick& tick);
  GroundRoll* FindGroundRoll(EntityId effect, EntityId target);
  void DropGroundRolls(EntityId effect);  // kNoEntity = every effect
  // The live zone is a sub-dungeon or a labyrinth floor (combat 15: the death penalty is permanent there).
  bool InDungeonZone() const;
  void ApplySkillStatuses(const SkillDef& s, int32_t level, EntityId target, double dealt);
  int32_t AllocRelease(const PendingRelease& r);
  int32_t AllocHit(PendingHit h);
  void FireDelayedHit(int32_t slot);
  void ResolveChargeDash(int32_t slot);

  // ---- hits ----
  HeroHitOutcome HeroHitMonster(const HeroHitSpec& h);
  void ResolveHeroStrike(EntityId target);
  void ResolveMonsterStrike(EntityId monster);
  void ApplyMonsterHit(EntityId monster, bool ranged);
  // Raw HP loss with feedback (EvHit, number, SFX, shake, HeroDamagedMsg); no death check.
  void HurtHero(double amount, bool crit, bool tick, DamageType element, EntityId source, bool melee, HitWeight weight,
                double attackerStopMs);
  bool TryDeathSave();
  void EmitMiss(EntityId target, Faction faction, EntityId source, const std::string& skillId, bool iframe);
  void ApplySteal(int32_t damage, bool crit, int32_t lifeStolen, int32_t manaStolen);
  void ConsumeCritBonus();
  void GainSpirit(SpiritSource source, bool crit);
  StatusApplyOutcome ApplyStatus(EntityId target, StatusType type, double value, double durationMs,
                                     EntityId source);
  void Shake(double durationMs, double intensity);
  void EmitSfx(SfxId cue, Vec2 pos, EntityId source);
  void StartEliteSlowMotion();

  // ---- state helpers ----
  void SetTargetInternal(EntityId target);
  void UpdateIndicator();
  void Respawn();
  void CancelPendingStrikes(bool emitCancelled);
  bool HeroImmobilized() const;
  bool HeroAlive() const;
  double SkillReach(const SkillDef& s) const;  // range + rangeSlackTiles
  DamageRules Rules() const;
  std::vector<TargetCandidate> AliveCandidates(Vec2 centre, double radius) const;
  int32_t HotbarSlotOf(int32_t skillIndex) const;

  SimContext& ctx_;
  InputBuffer buffer_;
  DodgeController dodge_;
  ShakeThrottle shake_;
  SkillAim bufferedAim_;
  std::vector<PendingRelease> releases_;
  std::vector<PendingHit> hits_;
  std::vector<GroundRoll> groundRolls_;  // C4: per (effect, target), dropped after the effect's last tick
  std::vector<PendingStrike> strikes_;
  bool inCombat_ = false;
  TimerId combatOffTimer_ = kNoTimer;
  TimerId respawnTimer_ = kNoTimer;
  EntityId indicator_ = kNoEntity;
  KillRewardInfo lastKill_;
};

}  // namespace abyss
