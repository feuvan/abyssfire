// In-flight projectiles (hero skill projectiles, monster bolts, pet bolts) and persistent ground effects.
// Spec: combat-feel.md 5.2 (monster bolt flight clamp(2 * px, 200, 500)), 6.4 (skill projectile flight, target-locked),
// 6.5 (AoE delays); classes-stats-skills.md 9.4-9.7; DECISIONS C4 (Fire Wall / Arrow Rain / traps are persistent ground
// effects with the same total damage spread over their ticks), S4 (36 px per tile), D13 T2/T4 + F2 (a cinematic
// destroys monster bolts; hero projectiles are held and resume).
//
// Owner area: hero+combat. Runtime system (SimContext). Projectiles are core entities (EntityKind::Projectile /
// GroundEffect): UE renders them from the Snapshot (progress = (now - launchMs) / travelMs) and the
// EvProjectileLaunched / EvProjectileEnded / EvGroundEffect* events. The arrival timer belongs to
// TimerOwner::Projectiles; on arrival the system calls the owner back:
//   HeroSkill  -> CombatSystem::OnSkillProjectileArrived
//   MonsterBolt-> CombatSystem::OnMonsterBoltArrived
//   PetBolt    -> PetCompanion::OnBoltArrived
// Ground-effect ticks call CombatSystem::OnGroundEffectTick.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Timers.h"
#include "abyss/base/Types.h"
#include "abyss/data/CombatData.h"
#include "abyss/data/SkillData.h"
#include "abyss/sim/SimTypes.h"

namespace abyss {

struct SimContext;
struct Snapshot;

enum class ProjectileTimerKind : uint16_t { Arrive = 1, GroundTick = 2, GroundEnd = 3 };

struct ProjectileSpec {
  ProjectileKind kind = ProjectileKind::HeroSkill;
  EntityId source = kNoEntity;
  EntityId target = kNoEntity;  // target-locked: lands on the target wherever it is
  Vec2 from, to;                // `to` = target position at launch (render only for locked projectiles)
  double travelMs = 0;
  std::string vfxId;
  uint32_t color = 0;
  // owner payload (opaque to this system)
  int32_t skillIndex = -1;
  int32_t skillLevel = 0;
  int32_t payload = 0;
};

struct ABYSS_API Projectile {
  EntityId id = kNoEntity;
  ProjectileSpec spec;
  double launchMs = 0;
  TimerId timer = kNoTimer;
  double Progress(double nowMs) const;  // clamp((now - launch) / travel, 0, 1)
};

struct GroundEffectSpec {
  EntityId source = kNoEntity;
  int32_t skillIndex = -1;
  int32_t skillLevel = 0;
  std::string skillId;
  std::string vfxId;
  Vec2 center;
  double radius = 0;
  double durationMs = 0;
  int32_t ticks = 1;          // C4: total damage spread over ticks (first tick at startMs + interval)
  double damageShare = 1.0;   // fraction of the skill hit each tick deals (1 / ticks)
};

struct GroundEffect {
  EntityId id = kNoEntity;
  GroundEffectSpec spec;
  double startMs = 0;
  int32_t ticksDone = 0;
  TimerId timer = kNoTimer;
  double TickIntervalMs() const { return spec.ticks > 0 ? spec.durationMs / spec.ticks : spec.durationMs; }
};

// Monster bolt flight (5.2): clamp(tileDist * 36 * msPerPx, min, max).
ABYSS_API double MonsterBoltTravelMs(const ProjectileTimingTable& t, Vec2 from, Vec2 to);
// Skill projectile flight (6.4): clamp(tileDist * 36 * msPerPx, min, max) from SkillDef::projectile; 0 when the skill
// has no projectile (instant hit). Meteor's fixed fall is min == max.
ABYSS_API double SkillTravelMs(const SkillDef& s, Vec2 from, Vec2 to);
// Per-target arrow delay (6.5): min(maxMs, tileDist * 36 * msPerPx) for multishot / piercing_arrow, else 0.
ABYSS_API double SkillArrowDelayMs(const SkillDef& s, Vec2 from, Vec2 to);
// Monster bolt tint (5.2): fire/phoenix -> 0xff6600, ice -> 0x4488ff, else 0xcc44cc (exported per monster as data).
ABYSS_API uint32_t MonsterBoltColor(std::string_view spriteKey);

class ABYSS_API ProjectileSystem {
 public:
  explicit ProjectileSystem(SimContext& ctx);

  // Spawns the projectile entity, emits EvProjectileLaunched, schedules the arrival timer at now + travelMs.
  EntityId Launch(const ProjectileSpec& spec);
  // Removes a projectile without arrival (fizzle): cancels its timer, emits EvProjectileEnded{hit=false}.
  void Destroy(EntityId projectile);
  EntityId StartGroundEffect(const GroundEffectSpec& spec);
  void EndGroundEffect(EntityId effect);

  void OnTimer(const Timer& t);
  // F2 (cinematic only): destroy every MonsterBolt (EvMonsterAttackCancelled{projectile=true} + EvProjectileEnded).
  void OnFreezeBegin(bool cinematic);
  // Zone unload: everything is discarded silently (timers cancelled).
  void ClearZone();

  const Projectile* Find(EntityId id) const;
  std::span<const Projectile> Projectiles() const { return projectiles_; }
  std::span<const GroundEffect> GroundEffects() const { return ground_; }
  void FillSnapshot(Snapshot& out) const;

 private:
  SimContext& ctx_;
  std::vector<Projectile> projectiles_;  // launch order
  std::vector<GroundEffect> ground_;
};

}  // namespace abyss
