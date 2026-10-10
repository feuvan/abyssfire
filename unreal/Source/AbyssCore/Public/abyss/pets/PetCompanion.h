// PetCompanion: the active ley-beast in the world (follow, target choice, basic attack, abilities, exhaustion,
// revive, bond rescue). Decision logic is core; UE only renders (APetActor).
// Spec: quests-story-ch1.md 18.5 (lifecycle and per-visit state, per-frame update, spawn, choosePetAction, abilities,
// basic attack, damage to monsters with full kill credit, interception hook), 18.4 (kill credit), 18.12 QP1 (FIX: the
// beast only follows while the hero is dead and never heals a dead hero); combat-feel.md 5.1 (pet interception hook);
// DECISIONS Q3, D13 (delays become sim timers; paused while cinematic / transitioning).
//
// Owner area: quests+story+pets. Runtime system (SimContext). Timers: TimerOwner::Pets. RNG: RngStream::Pets.
// Damage to monsters goes through CombatSystem::DamageMonster (KillSource::Pet, no provoke: pet hits never aggro, 18.5.4);
// bolts through ProjectileSystem (ProjectileKind::PetBolt) which calls OnBoltArrived.
//
// Presentation: EvEntitySpawned / EvEntityDespawned (EntityKind::Pet, art id PetArtId), EvEntityTeleported (CatchUp),
// EvPlayAnim (Attack / Cast with the release beat, Hurt), EvProjectileLaunched (via ProjectileSystem), EvHit (hits on
// monsters from CombatSystem; hits taken by the beast here), EvFloatingText (heal +N on the hero, damage on the beast,
// mana as Custom with value), EvSkillVfx (ability VFX: vfxId per kind, see PetAbilityVfxId), EvStatusApplied,
// EvPet{Exhausted / Recovered}, logs sys.pet.exhausted / rescue / revive. Position, HP, exhaustion come from the
// Snapshot (PetView).
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Timers.h"
#include "abyss/base/Types.h"
#include "abyss/data/PetData.h"
#include "abyss/hero/Buffs.h"
#include "abyss/sim/GameplayBus.h"
#include "abyss/sim/SimTypes.h"

namespace abyss {

struct SimContext;
struct Snapshot;
struct Projectile;
struct PetInstance;

enum class PetTimerKind : uint16_t {
  BasicRelease = 1,    // ranged basic attack: the cast releases -> bolt launched
  BasicContact = 2,    // melee basic attack: contact -> hit
  AbilityRelease = 3,  // heal / shield / buff / taunt / mark / bolt / cone / nova resolve
  StrikeHit = 4,       // one hit of a strike ability (castMs + i x 140)
  TakeHit = 5,         // a monster swing redirected at the beast reaches contact (entity = monster)
};

enum class PetActionKind : uint8_t { Rest, Follow, Approach, Attack, Ability };

// Inputs of choosePetAction (PetDecisionContext, PetSystem.ts:120-145).
struct ABYSS_API PetDecisionContext {
  double nowMs = 0;
  std::vector<const PetAbilityDef*> abilities;            // unlocked, priority order
  std::vector<std::pair<std::string, double>> readyAt;    // ability id -> ready again (missing = 0)
  bool exhausted = false;
  bool peaceful = false;
  double heroDist = 0;
  double heroHpRatio = 1;
  int32_t heroAttackers = 0;
  bool hasTarget = false;
  double targetDist = 0;
  bool targetMarked = false;
  int32_t enemiesNearTarget = 0;
  double basicRange = 0;
  double basicReadyAt = 0;
  double leash = 11;

  double ReadyAt(std::string_view abilityId) const;
};

struct PetAction {
  PetActionKind kind = PetActionKind::Rest;
  const PetAbilityDef* ability = nullptr;  // Ability only
};

// abilityUseful (18.5.5): ignores the cooldown.
ABYSS_API bool PetAbilityUseful(const PetAbilityDef& a, const PetDecisionContext& ctx);
// choosePetAction (18.5.5): exhausted -> rest; peaceful / beyond the leash -> follow; ready abilities (not revive) in
// array order, sustain first (heal / shield / taunt) when the hero is below 50 %; no target -> follow; in basic range
// -> attack when ready else rest; else approach.
ABYSS_API PetAction ChoosePetAction(const PetDecisionContext& ctx);
// shouldBondRescue (18.5.5): bond >= 5 and 0 < ratio < 0.3 and now - last >= 60000.
ABYSS_API bool ShouldBondRescue(const PetTables& t, int32_t bond, double heroHpRatio, double nowMs, double lastRescueAtMs);

// Render art id of a beast at an evolution stage (web PetKit.ts:95): "beast_<petId>", "beast_<petId>_e<stage>".
ABYSS_API std::string PetArtId(std::string_view petId, int32_t stage);
// VFX id of an ability's release (the web's skillEffects.play keys, PetCompanion.ts:582-745): heal life_regen,
// shield shield_wall, buff frenzy, taunt taunt_roar, mark death_mark, strike charge (leap) / backstab (crit) /
// bleed_strike (bleed) / slash, bolt combustion (splash), cone combustion, nova war_stomp (self) / combustion (fire) /
// arcane_torrent, revive combustion.
ABYSS_API std::string_view PetAbilityVfxId(const PetAbilityDef& a, DamageType element);

struct PetCompanionState {
  bool present = false;
  EntityId entity = kNoEntity;
  std::string petId;
  int32_t stage = 0;          // evolution stage shown
  Vec2 pos;
  Vec2 prevPos;               // position at the start of the last Tick (render interpolation)
  Vec2 facing{1, 0};
  double hp = 0, maxHp = 0;
  double exhaustedUntilMs = 0;
  double basicReadyAtMs = 0;
  double lockedUntilMs = 0;
  std::vector<std::pair<std::string, double>> readyAtMs;  // ability id -> ready again
  std::vector<std::pair<EntityId, double>> taunts;        // monster -> until
  std::vector<std::pair<EntityId, double>> marks;         // monster -> until
  bool reviveUsed = false;    // once per zone visit
  double lastRescueMs = -1e300;
  EntityId target = kNoEntity;
  PetActionKind lastAction = PetActionKind::Rest;
  bool moving = false;        // moved during the last Tick (walk / fly loop)
  // strike leap tween (Quad.easeOut over max(120, castMs))
  bool leaping = false;
  Vec2 leapFrom, leapTo;
  double leapStartMs = 0, leapDurationMs = 0;
};

class ABYSS_API PetCompanion {
 public:
  explicit PetCompanion(SimContext& ctx);

  void OnZoneEnter();  // per-visit state reset (18.5.1), then spawns the active beast beside the hero (18.5.3)
  void OnZoneExit();   // drops the entity (zone unload: no despawn event) and every pending pet timer
  // PetChangedMsg (GameSim wiring, after the equip-stat rebuild): makes the world match PetSystem::Active() at once -
  // no active beast -> despawn (EvEntityDespawned{Removed}); another beast or another evolution stage -> despawn +
  // spawn beside the hero at full HP with the stage's art id (EvEntitySpawned{Pet, petId, PetArtId}); cooldowns,
  // exhaustion, taunts / marks and the per-visit revive carry over (one companion per zone visit, web QUIRK QP2: every
  // evolution / switch re-spawns the beast); same beast and stage -> nothing. No-op without a zone.
  void OnPetChanged(const PetChangedMsg& m);
  void Tick(double dtMs);  // 18.5.2, after hero combat (classes 16 step 10)
  void OnTimer(const Timer& t);
  void OnBoltArrived(const Projectile& p);
  // killPlayer hook (18.5): a phoenix-type revive ability returns true and restores the hero instead of dying.
  bool TryReviveHero();
  // 18.5.9 (combat 5.1): called for a ready monster swing before it is aimed at the hero. True = the swing goes to the
  // beast (taunted, or a 25 % stray swing when the beast is within reach and closer than the hero); the monster's swing
  // clock restarts and the hit lands on the beast at its contact beat.
  bool InterceptMonsterAttack(EntityId monster);
  // Pets panel "feed" (18.8): no fruit -> sys.pet.noFruit; !CanFeed -> sys.pet.feedFull; else one c_ley_fruit leaves
  // the first stack in bag order, PetSystem::Feed, EvInventoryChanged.
  bool FeedFromBag(std::string_view petId);

  bool IsExhausted() const;
  const PetCompanionState& State() const { return state_; }
  PetCompanionState& MutableStateForTesting() { return state_; }  // tests only (hp, position, cooldowns)
  void FillSnapshot(Snapshot& out) const;

 private:
  struct PendingAction {
    int32_t key = 0;
    std::string petId;
    int32_t ability = -1;  // index into PetDef::abilities; -1 = the basic attack
    int32_t hit = 0;       // strike hit index
    EntityId target = kNoEntity;
  };

  // Spawns PetSystem::Active() at (hero - 1.2, hero + 1.2) (hero tile when that is not walkable), hp = max HP.
  void SpawnActive();
  void Despawn(DespawnReason reason);
  void CancelPending();
  bool Reconcile();  // the world matches PetSystem::Active(); false when no beast is out

  const PetDef* CurrentDef() const;
  double HeroDamage() const;
  double MonsterDist(EntityId monster) const;
  bool MonsterAlive(EntityId monster) const;
  Vec2 MonsterPos(EntityId monster) const;
  double ReadyAt(std::string_view abilityId) const;
  void SetReadyAt(std::string_view abilityId, double ms);
  double Until(const std::vector<std::pair<EntityId, double>>& list, EntityId id) const;
  void SetUntil(std::vector<std::pair<EntityId, double>>& list, EntityId id, double until);

  EntityId PickTarget() const;
  int32_t CountHeroAttackers() const;
  int32_t CountNear(Vec2 centre, double radius) const;
  void PruneMarks(double now);
  void ClearMark(EntityId monster);
  double MarkValue(EntityId monster) const;

  void Face(Vec2 delta);
  void Follow(double dtMs, const PetDef& def, bool exhausted);
  bool MoveToward(Vec2 goal, double stopAt, double dtMs, const PetDef& def, double speed);
  void UpdateLeap(double now);

  int32_t Hit(EntityId target, double mult, bool forceCrit, DamageType element, uint32_t color);
  void BasicAttack(EntityId target, double now, const PetDef& def);
  void UseAbility(const PetAbilityDef& a, EntityId target, double now, const PetDef& def, bool forced);
  void ResolveAbility(const PendingAction& pa);
  void ResolveStrikeHit(const PendingAction& pa);
  void ResolveBolt(const PendingAction& pa, EntityId target);
  void LaunchBolt(const PendingAction& pa, EntityId target, uint32_t color);
  void TakeHit(EntityId monster);
  void Exhaust();
  void Recover();
  void HeroBuff(BuffStat stat, double value, double durationMs, BuffTag tag);
  void ApplyStatus(EntityId target, StatusType type, double value, double durationMs);
  void EmitAbilityVfx(const PetAbilityDef& a, DamageType element, EntityId target, Vec2 point, double radius);

  int32_t Schedule(double dueMs, PetTimerKind kind, PendingAction pa);
  PendingAction TakePending(int32_t key);

  SimContext& ctx_;
  PetCompanionState state_;
  std::vector<PendingAction> pending_;  // closures of the web's delayed calls (timer param / projectile payload = key)
  int32_t nextKey_ = 1;
};

}  // namespace abyss
