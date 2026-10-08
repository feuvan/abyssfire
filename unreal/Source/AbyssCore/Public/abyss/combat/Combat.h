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
// through RewardService (ExpSource::Kill, GoldReason::Kill), killHealPercent, floating texts, target cleanup.
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
  // C4: Charge = HeroLocomotion::StartDash(to the target's melee reach, SkillPortDef::dashDurationMs) + this timer at
  // startMs + duration; on fire the hit resolves where the hero stands (a dash interrupted by stun / freeze, C5, hits
  // only if the target is still within melee reach). Sim clock: a modal freeze holds both (F3).
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

// A hit on a monster from any source (pets, status ticks, environment); hero hits use the internal paths.
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
  const DodgeController& Dodge() const { return dodge_; }
  const InputBuffer& Buffer() const { return buffer_; }
  // Combatant views for the damage formula (combat-feel 1.2).
  Combatant HeroCombatant() const;
  static Combatant MonsterCombatant(const MonsterInstance& m);

  void FillSnapshot(Snapshot& out) const;

 private:
  struct PendingRelease {
    int32_t skillIndex = -1;
    int32_t level = 0;
    EntityId target = kNoEntity;
    SkillAim aim;
    bool used = false;
  };
  struct PendingHit {
    int32_t skillIndex = -1;
    int32_t level = 0;
    std::vector<EntityId> targets;
    Vec2 center;
    bool hasAnchor = false;
    bool used = false;
  };

  SimContext& ctx_;
  InputBuffer buffer_;
  DodgeController dodge_;
  ShakeThrottle shake_;
  std::vector<PendingRelease> releases_;
  std::vector<PendingHit> hits_;
  bool fighting_ = false;
  bool inCombat_ = false;
  TimerId combatOffTimer_ = kNoTimer;
  TimerId respawnTimer_ = kNoTimer;
  EntityId indicator_ = kNoEntity;
  bool critBonusPending_ = false;  // FIX Q18: shadow_step crit buff consumed by the next hero hit
};

}  // namespace abyss
