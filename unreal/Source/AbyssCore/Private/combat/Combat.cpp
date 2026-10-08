// CombatSystem (combat-feel.md 4-9, 11, 13; classes-stats-skills.md 4.2, 6.6, 9, 11-13, 16). STUB: owner area
// hero+combat. The constructor and simple queries are real; everything else is a stub that keeps the skeleton
// GameSim running.
#include "abyss/base/Platform.h"

#include "abyss/combat/Combat.h"

#include "abyss/base/Assert.h"
#include "abyss/combat/StatusEffects.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

CombatSystem::CombatSystem(SimContext& ctx)
    : ctx_(ctx), buffer_(ctx.data.Combat().input.inputBufferMs), dodge_(ctx.data.Combat().input) {}

SkillRequestResult CombatSystem::RequestSkillSlot(int32_t slot, const SkillAim& aim) {
  ABYSS_UNIMPLEMENTED();
  return SkillRequestResult::Blocked;
}

SkillRequestResult CombatSystem::RequestSkill(int32_t skillIndex, const SkillAim& aim) {
  ABYSS_UNIMPLEMENTED();
  return SkillRequestResult::Blocked;
}

bool CombatSystem::CanExecuteSkill(int32_t skillIndex) const {
  ABYSS_UNIMPLEMENTED();
  return false;
}

bool CombatSystem::RequestDodge(Vec2 requestedDir) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

void CombatSystem::SetAttackTarget(EntityId monster, bool approach) {
  ABYSS_UNIMPLEMENTED();
  ctx_.sys.hero->attackTarget = monster;
}

void CombatSystem::ClearAttackTarget() { ctx_.sys.hero->attackTarget = kNoEntity; }

void CombatSystem::CycleTarget() { ABYSS_UNIMPLEMENTED(); }

bool CombatSystem::LearnSkill(int32_t skillIndex) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

bool CombatSystem::AllocateStat(PrimaryStat stat, int32_t points) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

bool CombatSystem::SetHotbar(int32_t slot, int32_t skillIndex) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

void CombatSystem::SetAutoCombat(bool on) { ctx_.sys.hero->autoCombat = on; }

void CombatSystem::TickPassives(double dtMs) { ABYSS_UNIMPLEMENTED(); }

void CombatSystem::TickHeroUpdate(double dtMs) { ABYSS_UNIMPLEMENTED(); }

void CombatSystem::ConsumeBufferedSkill() {
  if (buffer_.HasPending()) ABYSS_UNIMPLEMENTED();
}

void CombatSystem::TickCombat() {
  ABYSS_UNIMPLEMENTED();
  (void)releases_;
  (void)hits_;
  (void)shake_;
  (void)critBonusPending_;
}

void CombatSystem::TickStatusEffects() { ABYSS_UNIMPLEMENTED(); }

void CombatSystem::TickAutoCombat() {
  if (ctx_.sys.hero->autoCombat) ABYSS_UNIMPLEMENTED();
}

void CombatSystem::TickCombatState() {
  ABYSS_UNIMPLEMENTED();
  (void)fighting_;
  (void)combatOffTimer_;
}

void CombatSystem::OnTimer(const Timer& t) { ABYSS_UNIMPLEMENTED(); }

void CombatSystem::OnFreezeBegin(bool cinematic) {
  buffer_.Clear();
  if (cinematic) ABYSS_UNIMPLEMENTED();
}

void CombatSystem::OnZoneEnter() {
  ctx_.sys.hero->Skills().ResetCooldowns();
  indicator_ = kNoEntity;
}

void CombatSystem::OnZoneExit() { ABYSS_UNIMPLEMENTED(); }

void CombatSystem::OnSkillProjectileArrived(const Projectile& p) { ABYSS_UNIMPLEMENTED(); }

void CombatSystem::OnMonsterBoltArrived(const Projectile& p) { ABYSS_UNIMPLEMENTED(); }

void CombatSystem::OnGroundEffectTick(const GroundEffect& g, int32_t tickIndex) { ABYSS_UNIMPLEMENTED(); }

void CombatSystem::OnMonsterKilled(const MonsterKilledMsg& m) { ABYSS_UNIMPLEMENTED(); }

HitWeight CombatSystem::DamageMonster(const MonsterHitRequest& r) {
  ABYSS_UNIMPLEMENTED();
  return HitWeight::Tick;
}

void CombatSystem::DamageHero(const HeroHitRequest& r) { ABYSS_UNIMPLEMENTED(); }

void CombatSystem::KillHero(HeroDeathCause cause) {
  ABYSS_UNIMPLEMENTED();
  (void)respawnTimer_;
}

void CombatSystem::ResolvePendingDeath() {
  if (ctx_.sys.hero->Life() == HeroLife::Dying) ABYSS_UNIMPLEMENTED();
}

EntityId CombatSystem::AttackTarget() const { return ctx_.sys.hero->attackTarget; }

EntityId CombatSystem::PreferredTarget() const {
  ABYSS_UNIMPLEMENTED();
  return kNoEntity;
}

EntityId CombatSystem::IndicatorTarget() const { return indicator_; }

Combatant CombatSystem::HeroCombatant() const { return ctx_.sys.hero->AsCombatant(); }

Combatant CombatSystem::MonsterCombatant(const MonsterInstance& m) {
  Combatant c;
  c.stats = m.stats;
  c.baseDamage = m.def.damage;
  c.defense = m.def.defense;
  c.maxHp = m.maxHp;
  c.buffs = &m.buffs;
  return c;
}

void CombatSystem::FillSnapshot(Snapshot& out) const {
  const double now = ctx_.Now();
  out.hero.dodgeCooldownRemainingMs = dodge_.CooldownRemainingMs(now);
  out.hero.dodgeCooldownMs = dodge_.CooldownMs();
  out.hero.invulnerable = dodge_.IsInvulnerable(now);
  out.hero.target = AttackTarget();
  out.hero.indicator = indicator_;
  out.hero.inCombat = inCombat_;
}

}  // namespace abyss
