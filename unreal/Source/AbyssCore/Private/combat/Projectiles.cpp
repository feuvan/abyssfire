// Projectiles and persistent ground effects (combat-feel.md 5.2, 6.4-6.5; C4; D13 F2). STUB: owner area hero+combat.
#include "abyss/base/Platform.h"

#include "abyss/combat/Projectiles.h"

#include <algorithm>

#include "abyss/base/Assert.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

double Projectile::Progress(double nowMs) const {
  if (spec.travelMs <= 0) return 1.0;
  return std::clamp((nowMs - launchMs) / spec.travelMs, 0.0, 1.0);
}

double MonsterBoltTravelMs(const ProjectileTimingTable& t, Vec2 from, Vec2 to) {
  ABYSS_UNIMPLEMENTED();
  return t.monsterProjectileMinMs;
}

double SkillTravelMs(const SkillDef& s, Vec2 from, Vec2 to) {
  ABYSS_UNIMPLEMENTED();
  return 0;
}

double SkillArrowDelayMs(const SkillDef& s, Vec2 from, Vec2 to) {
  ABYSS_UNIMPLEMENTED();
  return 0;
}

uint32_t MonsterBoltColor(std::string_view spriteKey) {
  ABYSS_UNIMPLEMENTED();
  return 0xcc44cc;
}

ProjectileSystem::ProjectileSystem(SimContext& ctx) : ctx_(ctx) {}

EntityId ProjectileSystem::Launch(const ProjectileSpec& spec) {
  ABYSS_UNIMPLEMENTED();
  return kNoEntity;
}

void ProjectileSystem::Destroy(EntityId projectile) { ABYSS_UNIMPLEMENTED(); }

EntityId ProjectileSystem::StartGroundEffect(const GroundEffectSpec& spec) {
  ABYSS_UNIMPLEMENTED();
  return kNoEntity;
}

void ProjectileSystem::EndGroundEffect(EntityId effect) { ABYSS_UNIMPLEMENTED(); }

void ProjectileSystem::OnTimer(const Timer& t) { ABYSS_UNIMPLEMENTED(); }

void ProjectileSystem::OnFreezeBegin(bool cinematic) {
  if (!projectiles_.empty()) ABYSS_UNIMPLEMENTED();
}

void ProjectileSystem::ClearZone() {
  for (const Projectile& p : projectiles_) ctx_.timers.Cancel(p.timer);
  for (const GroundEffect& g : ground_) ctx_.timers.Cancel(g.timer);
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
