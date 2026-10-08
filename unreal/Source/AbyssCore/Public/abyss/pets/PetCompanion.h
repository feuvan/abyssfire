// PetCompanion: the active ley-beast in the world (follow, target choice, basic attack, abilities, exhaustion,
// revive, bond rescue). Decision logic is core; UE only renders (APetActor).
// Spec: quests-story-ch1.md 18.5 (lifecycle and per-visit state, per-frame update, spawn, choosePetAction, abilities,
// basic attack, damage to monsters with full kill credit, interception hook), 18.4 (kill credit); combat-feel.md 5.1
// (pet interception hook, later); DECISIONS Q3, D13 (delays become sim timers; paused while cinematic / transitioning).
//
// Owner area: quests+story+pets. Runtime system (SimContext). Timers: TimerOwner::Pets. RNG: RngStream::Pets.
// Damage to monsters goes through CombatSystem::DamageMonster (KillSource::Pet); bolts through ProjectileSystem
// (ProjectileKind::PetBolt) which calls OnBoltArrived.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Timers.h"
#include "abyss/base/Types.h"
#include "abyss/data/PetData.h"
#include "abyss/sim/GameplayBus.h"
#include "abyss/sim/SimTypes.h"

namespace abyss {

struct SimContext;
struct Snapshot;
struct Projectile;

enum class PetTimerKind : uint16_t { AbilityHit = 1, BasicHit = 2 };

enum class PetActionKind : uint8_t { Rest, Follow, Approach, Attack, Ability };

// Inputs of choosePetAction (18.5, pure).
struct PetActionContext {
  double nowMs = 0;
  bool hasTarget = false;
  double targetDist = 0;
  double heroDist = 0;
  double heroHpRatio = 1;
  double petHpRatio = 1;
  bool peaceful = false;
  int32_t evolved = 0;
};

struct PetAction {
  PetActionKind kind = PetActionKind::Rest;
  int32_t abilityIndex = -1;  // into PetDef::abilities
};

// choosePetAction (18.5): abilities in array order (priority), unlock gate by evolution, readiness, then basic attack /
// approach / follow / rest.
ABYSS_API PetAction ChoosePetAction(const PetDef& def, const PetActionContext& ctx, const std::vector<double>& abilityReadyAt);

// Render art id of a beast at an evolution stage (web PetKit.ts:95): "beast_<petId>", "beast_<petId>_e<stage>".
ABYSS_API std::string PetArtId(std::string_view petId, int32_t stage);

struct PetCompanionState {
  bool present = false;
  EntityId entity = kNoEntity;
  std::string petId;
  int32_t stage = 0;          // evolution stage shown
  Vec2 pos;
  Vec2 facing{1, 0};
  double hp = 0, maxHp = 0;
  double exhaustedUntilMs = 0;
  double basicReadyAtMs = 0;
  double lockedUntilMs = 0;
  std::vector<double> abilityReadyAtMs;  // per ability
  std::vector<std::pair<EntityId, double>> taunts;  // monster -> until
  std::vector<std::pair<EntityId, double>> marks;   // monster -> until
  bool reviveUsed = false;    // once per zone visit
  double lastRescueMs = -1e300;
  EntityId target = kNoEntity;
};

class ABYSS_API PetCompanion {
 public:
  explicit PetCompanion(SimContext& ctx);

  void OnZoneEnter();  // per-visit state reset (18.5.1), then spawns the active beast beside the hero (18.5.3)
  void OnZoneExit();   // drops the entity (zone unload: no despawn event)
  // PetChangedMsg (GameSim wiring, after the equip-stat rebuild): makes the world match PetSystem::Active() at once -
  // no active beast -> despawn (EvEntityDespawned{Removed}); another beast or another evolution stage -> despawn +
  // spawn beside the hero at full HP with the stage's art id (EvEntitySpawned{Pet, petId, PetArtId}) and the per-visit
  // state reset (web QUIRK QP2: every evolution / refresh re-spawns the beast); same beast and stage -> nothing.
  // CmdSetActivePet, the q_pet_sprite_friend petReward and a story grantPet all arrive here. No-op without a zone.
  void OnPetChanged(const PetChangedMsg& m);
  void Tick(double dtMs);  // 18.5.2, after hero combat (classes 16 step 10)
  void OnTimer(const Timer& t);
  void OnBoltArrived(const Projectile& p);
  // killPlayer hook (18.5): a phoenix-type revive ability returns true and restores the hero instead of dying.
  bool TryReviveHero();
  // Later milestone (combat 5.1): a monster swing may be intercepted by a taunting pet.
  bool InterceptMonsterAttack(EntityId monster);
  // Pets panel "feed": consumes one ley fruit from the bag and calls PetSystem::Feed (quests 18.3).
  bool FeedFromBag(std::string_view petId);

  const PetCompanionState& State() const { return state_; }
  void FillSnapshot(Snapshot& out) const;

 private:
  // Spawns PetSystem::Active() at (hero - 1.2, hero + 1.2) (hero tile when that is not walkable), hp = max HP.
  void SpawnActive();
  void Despawn(DespawnReason reason);

  SimContext& ctx_;
  PetCompanionState state_;
};

}  // namespace abyss
