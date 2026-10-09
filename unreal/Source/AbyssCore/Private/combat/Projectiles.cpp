// Projectiles and persistent ground effects (combat-feel.md 5.2, 6.4-6.5; C4; S4; D13 T2/T4, F2).
#include "abyss/base/Platform.h"

#include "abyss/combat/Projectiles.h"

#include <algorithm>

#include "abyss/base/Math.h"
#include "abyss/base/Units.h"
#include "abyss/combat/Combat.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/pets/PetCompanion.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

double Projectile::Progress(double nowMs) const {
  if (spec.travelMs <= 0) return 1.0;
  return std::clamp((nowMs - launchMs) / spec.travelMs, 0.0, 1.0);
}

double MonsterBoltTravelMs(const ProjectileTimingTable& t, Vec2 from, Vec2 to) {
  const double px = TilesToProjectilePx(Dist(from, to));
  return Clamp(px * t.monsterProjectileMsPerPx, t.monsterProjectileMinMs, t.monsterProjectileMaxMs);
}

double SkillTravelMs(const SkillDef& s, Vec2 from, Vec2 to) {
  if (!s.hasProjectile) return 0;
  const double px = TilesToProjectilePx(Dist(from, to));
  return Clamp(px * s.projectile.msPerPx, s.projectile.minMs, s.projectile.maxMs);
}

double SkillArrowDelayMs(const SkillDef& s, Vec2 from, Vec2 to) {
  if (!s.hasArrowDelay) return 0;
  const double px = TilesToProjectilePx(Dist(from, to));
  return (std::min)(s.arrowDelay.maxMs, px * s.arrowDelay.msPerPx);
}

uint32_t MonsterBoltColor(std::string_view spriteKey) {
  if (spriteKey.find("fire") != std::string_view::npos || spriteKey.find("phoenix") != std::string_view::npos) {
    return 0xff6600;
  }
  if (spriteKey.find("ice") != std::string_view::npos) return 0x4488ff;
  return 0xcc44cc;
}

ProjectileSystem::ProjectileSystem(SimContext& ctx) : ctx_(ctx) {}

EntityId ProjectileSystem::Launch(const ProjectileSpec& spec) {
  Projectile p;
  p.id = ctx_.ids.Next();
  p.spec = spec;
  p.spec.travelMs = (std::max)(0.0, spec.travelMs);
  p.launchMs = ctx_.Now();
  p.timer = ctx_.timers.Schedule(p.launchMs + p.spec.travelMs, TimerOwner::Projectiles,
                                 static_cast<uint16_t>(ProjectileTimerKind::Arrive), p.id);
  ctx_.events.Emit(EvProjectileLaunched{p.id, spec.kind, spec.source, spec.target, spec.from, spec.to, p.launchMs,
                                        p.spec.travelMs, spec.vfxId, spec.color});
  projectiles_.push_back(std::move(p));
  return projectiles_.back().id;
}

void ProjectileSystem::Destroy(EntityId projectile) {
  for (size_t i = 0; i < projectiles_.size(); ++i) {
    if (projectiles_[i].id != projectile) continue;
    ctx_.timers.Cancel(projectiles_[i].timer);
    projectiles_.erase(projectiles_.begin() + static_cast<std::ptrdiff_t>(i));
    ctx_.events.Emit(EvProjectileEnded{projectile, false});
    return;
  }
}

EntityId ProjectileSystem::StartGroundEffect(const GroundEffectSpec& spec) {
  GroundEffect g;
  g.id = ctx_.ids.Next();
  g.spec = spec;
  g.spec.ticks = (std::max)(1, spec.ticks);
  g.startMs = ctx_.Now();
  const EntityId id = g.id;
  const double duration = (std::max)(0.0, g.spec.durationMs);
  g.endTimer = ctx_.timers.Schedule(g.startMs + duration, TimerOwner::Projectiles,
                                    static_cast<uint16_t>(ProjectileTimerKind::GroundEnd), id);
  ctx_.events.Emit(EvGroundEffectStarted{id, g.spec.skillId, g.spec.vfxId, g.spec.center, g.spec.radius, g.startMs,
                                         duration, g.spec.trigger, g.spec.ticks});
  const bool periodic = g.spec.trigger == GroundTrigger::Periodic;
  if (periodic) {
    g.triggered = true;
    g.triggerMs = g.startMs;
  }
  ground_.push_back(std::move(g));
  if (periodic) FireGroundTick(id, 0);  // C4: the cast still hits at once
  return id;
}

void ProjectileSystem::EndGroundEffect(EntityId effect) {
  for (size_t i = 0; i < ground_.size(); ++i) {
    if (ground_[i].id != effect) continue;
    const bool triggered = ground_[i].triggered;
    ctx_.timers.Cancel(ground_[i].timer);
    ctx_.timers.Cancel(ground_[i].endTimer);
    ground_.erase(ground_.begin() + static_cast<std::ptrdiff_t>(i));
    ctx_.events.Emit(EvGroundEffectEnded{effect, triggered});
    return;
  }
}

GroundEffect* ProjectileSystem::FindGround(EntityId id) {
  for (GroundEffect& g : ground_) {
    if (g.id == id) return &g;
  }
  return nullptr;
}

const GroundEffect* ProjectileSystem::FindGroundEffect(EntityId id) const {
  for (const GroundEffect& g : ground_) {
    if (g.id == id) return &g;
  }
  return nullptr;
}

void ProjectileSystem::FireGroundTick(EntityId id, int32_t index) {
  GroundEffect* g = FindGround(id);
  if (g == nullptr) return;
  g->timer = kNoTimer;
  g->ticksDone = index + 1;
  const GroundEffect copy = *g;  // the callback may start / end effects (vector growth)
  if (ctx_.sys.combat != nullptr) ctx_.sys.combat->OnGroundEffectTick(copy, index);
  g = FindGround(id);
  if (g == nullptr) return;
  if (index + 1 < g->spec.ticks) {
    const double due = g->triggerMs + (index + 1) * g->TickIntervalMs();
    g->timer = ctx_.timers.Schedule(due, TimerOwner::Projectiles,
                                    static_cast<uint16_t>(ProjectileTimerKind::GroundTick), id, kNoEntity, index + 1);
  } else if (g->spec.trigger == GroundTrigger::Armed) {
    EndGroundEffect(id);  // a sprung trap ends after its last hit
  }
}

void ProjectileSystem::TickArmedTraps() {
  if (ctx_.sys.monsters == nullptr) return;
  std::vector<EntityId> fire;
  std::vector<EntityId> inside;
  for (const GroundEffect& g : ground_) {
    if (g.spec.trigger != GroundTrigger::Armed || g.triggered) continue;
    inside.clear();
    ctx_.sys.monsters->QueryAlive(g.spec.center, g.spec.radius, inside);
    if (!inside.empty()) fire.push_back(g.id);
  }
  for (EntityId id : fire) {
    GroundEffect* g = FindGround(id);
    if (g == nullptr || g->triggered) continue;
    g->triggered = true;
    g->triggerMs = ctx_.Now();
    ctx_.timers.Cancel(g->endTimer);
    g->endTimer = kNoTimer;
    ctx_.events.Emit(EvGroundEffectTriggered{id, g->triggerMs});
    FireGroundTick(id, 0);
  }
}

void ProjectileSystem::OnTimer(const Timer& t) {
  switch (static_cast<ProjectileTimerKind>(t.kind)) {
    case ProjectileTimerKind::Arrive: {
      for (size_t i = 0; i < projectiles_.size(); ++i) {
        if (projectiles_[i].id != t.entity) continue;
        const Projectile p = projectiles_[i];
        projectiles_.erase(projectiles_.begin() + static_cast<std::ptrdiff_t>(i));
        ctx_.events.Emit(EvProjectileEnded{p.id, true});
        switch (p.spec.kind) {
          case ProjectileKind::HeroSkill:
            if (ctx_.sys.combat != nullptr) ctx_.sys.combat->OnSkillProjectileArrived(p);
            break;
          case ProjectileKind::MonsterBolt:
            if (ctx_.sys.combat != nullptr) ctx_.sys.combat->OnMonsterBoltArrived(p);
            break;
          case ProjectileKind::PetBolt:
            if (ctx_.sys.petCompanion != nullptr) ctx_.sys.petCompanion->OnBoltArrived(p);
            break;
        }
        return;
      }
      return;
    }
    case ProjectileTimerKind::GroundTick:
      FireGroundTick(t.entity, t.param);
      return;
    case ProjectileTimerKind::GroundEnd: {
      GroundEffect* g = FindGround(t.entity);
      if (g == nullptr) return;
      g->endTimer = kNoTimer;
      EndGroundEffect(t.entity);
      return;
    }
  }
}

void ProjectileSystem::OnFreezeBegin(bool cinematic) {
  if (!cinematic) return;  // F3: a modal freeze keeps everything
  // F2: every in-flight monster bolt is destroyed (the web landed them during the cutscene - FIX).
  for (size_t i = 0; i < projectiles_.size();) {
    const Projectile& p = projectiles_[i];
    if (p.spec.kind != ProjectileKind::MonsterBolt) {
      ++i;
      continue;
    }
    ctx_.timers.Cancel(p.timer);
    const EntityId id = p.id;
    const EntityId monster = p.spec.source;
    projectiles_.erase(projectiles_.begin() + static_cast<std::ptrdiff_t>(i));
    ctx_.events.Emit(EvMonsterAttackCancelled{monster, true});
    ctx_.events.Emit(EvProjectileEnded{id, false});
  }
}

void ProjectileSystem::ClearZone() {
  for (const Projectile& p : projectiles_) ctx_.timers.Cancel(p.timer);
  for (const GroundEffect& g : ground_) {
    ctx_.timers.Cancel(g.timer);
    ctx_.timers.Cancel(g.endTimer);
  }
  projectiles_.clear();
  ground_.clear();
}

const Projectile* ProjectileSystem::Find(EntityId id) const {
  for (const Projectile& p : projectiles_) {
    if (p.id == id) return &p;
  }
  return nullptr;
}

void ProjectileSystem::FillSnapshot(Snapshot& out) const {
  const double now = ctx_.Now();
  for (const Projectile& p : projectiles_) {
    ProjectileView v;
    v.id = p.id;
    v.kind = p.spec.kind;
    v.target = p.spec.target;
    v.from = p.spec.from;
    v.to = p.spec.to;
    v.progress = p.Progress(now);
    v.vfxId = p.spec.vfxId;
    v.color = p.spec.color;
    out.projectiles.push_back(std::move(v));
  }
  for (const GroundEffect& g : ground_) {
    GroundEffectView v;
    v.id = g.id;
    v.skillId = g.spec.skillId;
    v.vfxId = g.spec.vfxId;
    v.center = g.spec.center;
    v.radius = g.spec.radius;
    v.progress = g.spec.durationMs > 0 ? std::clamp((now - g.startMs) / g.spec.durationMs, 0.0, 1.0) : 1.0;
    out.groundEffects.push_back(std::move(v));
  }
}

}  // namespace abyss
