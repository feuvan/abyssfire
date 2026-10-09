// CombatSystem (combat-feel.md 4-9, 11, 13; classes-stats-skills.md 4, 6.6, 9-13, 16; DECISIONS C1, C3-C5, C7, C8,
// C11, C12, S6, D13 T1-T7 / T11 / T12 / F1-F2; save-ui-input 5.1.1).
//
// Ordering notes (web parity):
// * A hero hit runs the steal step (Spirit 'hit', life / mana steal) BEFORE MonsterSystem::ApplyDamage: the web gains
//   'hit' before onMonsterKilled gains 'kill', and here the kill pipeline runs synchronously inside ApplyDamage.
// * Monster pointers are never held across ApplyDamage (the kill pipeline may spawn quest hunts).
#include "abyss/base/Platform.h"

#include "abyss/combat/Combat.h"

#include <algorithm>
#include <cmath>

#include "abyss/audio/Audio.h"
#include "abyss/base/Assert.h"
#include "abyss/base/I18n.h"
#include "abyss/base/Math.h"
#include "abyss/base/StrUtil.h"
#include "abyss/combat/SoulEcho.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/hero/Rewards.h"
#include "abyss/items/Inventory.h"
#include "abyss/monsters/EliteAffixes.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/pets/Homestead.h"
#include "abyss/pets/PetCompanion.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"
#include "abyss/world/Locomotion.h"
#include "abyss/world/Zone.h"

namespace abyss {

namespace {

// classes 5.3 / combat 5.3: deathSave restores 30 % HP and re-arms after 60 s of sim time (T11, FIX Q24). The web
// hard-codes both (ZoneScene.ts:3055-3061); no data table carries them.
constexpr double kCombatDeathSaveHpFraction = 0.3;
constexpr double kCombatDeathSaveRearmMs = 60000;
// combat 6.5: a target within ~4 px (0.1 tile) of the blast centre is pushed from the hero instead.
constexpr double kCombatBlastCentreTiles = 0.1;
// Swing readiness tolerance: the sim clock is steps * 1000 / 60 while lastAttackMs is a stored stamp, so `now - last`
// can land an ulp under a whole-step interval (1200 ms = 72 steps) and slip the swing by one step.
constexpr double kCombatSwingEpsilonMs = 1e-6;

I18nArg CmbSkillNameArg(const SkillDef& s) { return KeyArg("skillName", "data.skill." + s.id + ".name"); }

I18nArg CmbStatusNameArg(StatusType t) {
  return KeyArg("effectName", "sys.statusEffect.name." + std::string(EnumName(t)));
}

DamageType CmbTickElement(StatusType t) {
  switch (t) {
    case StatusType::Burn: return DamageType::Fire;
    case StatusType::Poison: return DamageType::Poison;
    default: return DamageType::Physical;
  }
}

Vec2 CmbDirOr(Vec2 d, Vec2 fallback) {
  const double len = d.Length();
  return len > 1e-9 ? d / len : fallback;
}

}  // namespace

CombatSystem::CombatSystem(SimContext& ctx)
    : ctx_(ctx), buffer_(ctx.data.Combat().input.inputBufferMs), dodge_(ctx.data.Combat().input) {}

// =====================================================================================================================
// helpers
// =====================================================================================================================

DamageRules CombatSystem::Rules() const {
  const ClassTables& c = ctx_.data.Classes();
  return DamageRules{&c.formulas, &c.buffCaps, &c.skillRules};
}

bool CombatSystem::HeroAlive() const {
  const Hero& h = *ctx_.sys.hero;
  return h.Life() == HeroLife::Alive && h.Hp() > 0;
}

bool CombatSystem::HeroImmobilized() const {
  return ctx_.sys.status != nullptr && ctx_.sys.status->IsImmobilized(kHeroEntityId);
}

double CombatSystem::SkillReach(const SkillDef& s) const {
  return s.range + ctx_.data.Classes().skillRules.rangeSlackTiles;
}

std::vector<TargetCandidate> CombatSystem::AliveCandidates(Vec2 centre, double radius) const {
  std::vector<TargetCandidate> out;
  const MonsterSystem* ms = ctx_.sys.monsters;
  if (ms == nullptr) return out;
  std::vector<EntityId> ids;
  ms->QueryAlive(centre, radius, ids);
  for (EntityId id : ids) {
    const MonsterInstance* m = ms->Find(id);
    if (m != nullptr && m->IsAlive()) out.push_back({id, m->pos, true});
  }
  return out;
}

int32_t CombatSystem::HotbarSlotOf(int32_t skillIndex) const {
  const SkillBook& book = ctx_.sys.hero->Skills();
  for (int32_t i = 0; i < SkillBook::kHotbarSlots; ++i) {
    if (book.HotbarSkill(i) == skillIndex) return i;
  }
  return -1;
}

int32_t CombatSystem::CastManaCost(int32_t skillIndex) const {
  const Hero& hero = *ctx_.sys.hero;
  const SkillBook& book = hero.Skills();
  return SkillCastMana(ctx_.data.Classes().skillRules, book.Skill(skillIndex), book.Level(skillIndex),
                       hero.GetSpirit().ManaCostMultiplier());
}

void CombatSystem::EmitSfx(SfxId cue, Vec2 pos, EntityId source) {
  const AudioCueDef* def = ctx_.data.Audio().Find(cue);
  ctx_.events.Sfx(cue, def != nullptr && def->spatial3d, pos, source);
}

void CombatSystem::Shake(double durationMs, double intensity) {
  const ShakeRequest s{durationMs, intensity};
  if (shake_.Accept(s, ctx_.Now(), ctx_.data.Combat().hitFeedback.shakeThrottleMs)) {
    ctx_.events.Emit(EvCameraShake{durationMs, intensity});
  }
}

void CombatSystem::StartEliteSlowMotion() {
  const HitFeedbackTable& t = ctx_.data.Combat().hitFeedback;
  ctx_.clock.StartDilation(t.eliteKillSlowMoTimeScale, t.eliteKillSlowMoDurationMs);
  ctx_.events.Emit(EvSlowMotion{t.eliteKillSlowMoTimeScale, t.eliteKillSlowMoDurationMs});
}

void CombatSystem::GainSpirit(SpiritSource source, bool crit) {
  Hero& hero = *ctx_.sys.hero;
  Spirit& sp = hero.GetSpirit();
  const SpiritGainResult g = sp.GainFromCombat(source, hero.BaseStats().spi, crit);
  if (g.gained > 0) ctx_.events.Emit(EvSpiritChanged{sp.Value(), sp.MaxValue(), sp.IsResonating(), g.gained});
  if (g.resonanceStarted) {
    ctx_.events.Emit(EvResonance{true, sp.Profile().id, sp.Profile().resonanceDurationMs});
    EmitSfx(ctx_.data.Audio().rules.resonanceStarted, hero.Position(), kHeroEntityId);
  }
}

// applySteal (classes 12.1): Spirit 'hit' when the hit dealt damage, then life / mana steal (gains need a living hero).
void CombatSystem::ApplySteal(int32_t damage, bool crit, int32_t lifeStolen, int32_t manaStolen) {
  Hero& hero = *ctx_.sys.hero;
  if (damage > 0) GainSpirit(SpiritSource::Hit, crit);
  if (lifeStolen > 0 && hero.Hp() > 0) hero.Heal(lifeStolen);
  if (manaStolen > 0) hero.RestoreMana(manaStolen);
}

// FIX Q18: the shadow_step critBonus buff is read as crit points (value x 100) and consumed by the next landed hit.
void CombatSystem::ConsumeCritBonus() { ctx_.sys.hero->Buffs().RemoveStat(BuffStat::CritBonus); }

StatusApplyOutcome CombatSystem::ApplyStatus(EntityId target, StatusType type, double value, double durationMs,
                                             EntityId source) {
  if (ctx_.sys.status == nullptr) return StatusApplyOutcome::Rejected;
  const StatusApplyResult r = ctx_.sys.status->Apply(target, type, value, durationMs, source, ctx_.Now());
  if (r.outcome == StatusApplyOutcome::Applied) {
    ctx_.events.Emit(EvStatusApplied{target, type, value, r.effectiveDurationMs, false});
    ctx_.events.Log(MakeLoc("sys.statusEffect.applied", {CmbStatusNameArg(type)}), LogType::Combat);
  } else if (r.outcome == StatusApplyOutcome::Refreshed) {
    ctx_.events.Emit(EvStatusApplied{target, type, value, r.effectiveDurationMs, true});
    if (type == StatusType::Poison) {  // the web logs only the poison refresh
      ctx_.events.Log(MakeLoc("sys.statusEffect.refreshed", {CmbStatusNameArg(type)}), LogType::Combat);
    }
  }
  return r.outcome;
}

void CombatSystem::EmitMiss(EntityId target, Faction faction, EntityId source, const std::string& skillId,
                            bool iframe) {
  EvHit hit;
  hit.target = target;
  hit.source = source;
  hit.targetFaction = faction;
  hit.amount = 0;
  hit.weight = HitWeight::Tick;
  hit.profile = ctx_.data.Combat().hitFeedback.Profile(HitWeight::Tick);
  hit.dodged = true;
  hit.iframeAvoided = iframe;
  hit.skillId = skillId;
  Vec2 pos;
  if (faction == Faction::Hero) {
    pos = ctx_.sys.hero->Position();
    hit.targetHp = ctx_.sys.hero->Hp();
    hit.targetMaxHp = ctx_.sys.hero->MaxHp();
  } else if (const MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(target) : nullptr) {
    pos = m->pos;
    hit.targetHp = m->hp;
    hit.targetMaxHp = m->maxHp;
  }
  ctx_.events.Emit(hit);
  EvFloatingText ft;
  ft.kind = FloatingTextKind::Miss;
  ft.anchor = target;
  ft.pos = pos;
  ctx_.events.Emit(ft);
  // Audio 3.2: a hero basic attack the monster sidestepped and the dodge-roll avoid are `miss`; the hero's stat dodge
  // emitted no COMBAT_DAMAGE in the web (silent, kept).
  if (faction == Faction::Monster || iframe) {
    if (const std::optional<SfxId> cue = SfxForCombatHit(ctx_.data.Audio().rules, true, false, HitWeight::Tick)) {
      EmitSfx(*cue, pos, target);
    }
  }
}

// =====================================================================================================================
// shared hit API
// =====================================================================================================================

HitWeight CombatSystem::DamageMonster(const MonsterHitRequest& r) {
  MonsterSystem* ms = ctx_.sys.monsters;
  if (ms == nullptr) return HitWeight::Tick;
  const MonsterInstance* before = ms->Find(r.monster);
  if (before == nullptr || !before->IsAlive()) return HitWeight::Tick;
  const bool elite = before->def.elite;
  Vec2 pos = before->pos;
  DamageFlags flags;
  flags.isCrit = r.isCrit;
  flags.isTick = r.isTick;
  flags.hasFrom = r.hasFrom;
  flags.from = r.from;
  flags.attacker = r.attacker;
  flags.source = r.source;
  flags.provokes = r.provokes && !r.isTick;
  const HitWeight w = ms->ApplyDamage(r.monster, r.amount, flags);
  const MonsterInstance* after = ms->Find(r.monster);  // re-find: the kill pipeline may have grown the list
  const bool killed = after == nullptr || !after->IsAlive();
  if (after != nullptr) pos = after->pos;

  const HitFeedbackTable& t = ctx_.data.Combat().hitFeedback;
  EvHit hit;
  hit.target = r.monster;
  hit.source = r.attacker;
  hit.targetFaction = Faction::Monster;
  hit.amount = r.amount;
  hit.weight = w;
  hit.profile = t.Profile(w);
  hit.crit = r.isCrit;
  hit.tick = r.isTick;
  hit.killed = killed;
  hit.element = r.element;
  hit.impactColor = r.impactColor;
  hit.hasFrom = r.hasFrom;
  hit.from = r.from;
  hit.targetHp = after != nullptr ? after->hp : 0;
  hit.targetMaxHp = after != nullptr ? after->maxHp : 0;
  hit.skillId = r.skillId;
  hit.attackerStopMs = r.source == KillSource::HeroBasic ? t.Profile(w).attackerStopMs : 0;
  hit.impactBurst = r.impactBurst && !r.isTick;
  hit.melee = false;
  hit.numberSlot = r.numberSlot;
  ctx_.events.Emit(hit);

  EvFloatingText ft;
  ft.kind = FloatingTextKind::MonsterDamage;
  ft.anchor = r.monster;
  ft.pos = pos;
  ft.value = r.amount;
  ft.crit = r.isCrit;
  ft.element = r.element;
  ctx_.events.Emit(ft);

  // Audio (FIX combat 19: every hit is a CombatDamage): hit / crit / miss (A6 heavy), plus the A7 hurt vocal.
  const AudioRulesDef& rules = ctx_.data.Audio().rules;
  if (const std::optional<SfxId> cue = SfxForCombatHit(rules, false, r.isCrit, w)) EmitSfx(*cue, pos, r.monster);
  if (!killed && !r.isTick) EmitSfx(rules.monsterHurt, pos, r.monster);

  if (hit.impactBurst) Shake(t.Profile(w).shakeMs, t.Profile(w).shakeIntensity);
  if (r.source == KillSource::HeroBasic && w == HitWeight::Kill && elite) StartEliteSlowMotion();
  return w;
}

void CombatSystem::HurtHero(double amount, bool crit, bool tick, DamageType element, EntityId source, bool melee,
                            HitWeight weight, double attackerStopMs) {
  Hero& hero = *ctx_.sys.hero;
  hero.ApplyDamage(amount);
  const HitFeedbackTable& t = ctx_.data.Combat().hitFeedback;
  EvHit hit;
  hit.target = kHeroEntityId;
  hit.source = source;
  hit.targetFaction = Faction::Hero;
  hit.amount = amount;
  hit.weight = weight;
  hit.profile = t.Profile(weight);
  hit.crit = crit;
  hit.tick = tick;
  hit.element = element;
  hit.impactColor = 0;
  if (source != kNoEntity && ctx_.sys.monsters != nullptr) {
    if (const MonsterInstance* m = ctx_.sys.monsters->Find(source)) {
      hit.hasFrom = true;
      hit.from = m->pos;
    }
  }
  hit.targetHp = hero.Hp();
  hit.targetMaxHp = hero.MaxHp();
  hit.attackerStopMs = attackerStopMs;
  hit.melee = melee;
  ctx_.events.Emit(hit);
  EvFloatingText ft;
  ft.kind = FloatingTextKind::HeroDamage;
  ft.anchor = kHeroEntityId;
  ft.pos = hero.Position();
  ft.value = amount;
  ft.crit = crit;
  ft.element = element;
  ctx_.events.Emit(ft);
  EmitSfx(ctx_.data.Audio().rules.heroDamageTakenCue, hero.Position(), kHeroEntityId);  // A6
  const ShakeRequest s = HeroHitShake(t, amount, hero.MaxHp(), crit);
  Shake(s.durationMs, s.intensity);
  ctx_.bus.Publish(HeroDamagedMsg{amount, hero.Hp(), hero.MaxHp(), source, tick});
}

bool CombatSystem::TryDeathSave() {
  Hero& hero = *ctx_.sys.hero;
  const double now = ctx_.Now();
  if (!(ctx_.equip.Get(Stat::DeathSave) > 0) || now < hero.deathSaveReadyAtMs) return false;
  const double hp = std::floor(hero.MaxHp() * kCombatDeathSaveHpFraction);
  hero.SetHp(hp);
  hero.deathSaveReadyAtMs = now + kCombatDeathSaveRearmMs;
  ctx_.events.Log(MakeLoc("zone.combat.deathImmunity"), LogType::System);
  EvFloatingText ft;
  ft.kind = FloatingTextKind::Heal;
  ft.anchor = kHeroEntityId;
  ft.pos = hero.Position();
  ft.value = hp;
  ctx_.events.Emit(ft);
  return true;
}

void CombatSystem::DamageHero(const HeroHitRequest& r) {
  if (!HeroAlive() || !(r.amount > 0)) return;
  const HitWeight w =
      ClassifyHit(ctx_.data.Combat().hitFeedback, r.amount, ctx_.sys.hero->MaxHp(), r.isCrit, false, r.isTick);
  HurtHero(r.amount, r.isCrit, r.isTick, r.element, r.source, false, w, 0);
  if (ctx_.sys.hero->Hp() <= 0) {
    if (r.allowDeathSave && TryDeathSave()) return;
    KillHero(r.isTick ? HeroDeathCause::StatusTick : HeroDeathCause::Other);
  }
}

// =====================================================================================================================
// hero -> monster hits
// =====================================================================================================================

CombatSystem::HeroHitOutcome CombatSystem::HeroHitMonster(const HeroHitSpec& h) {
  HeroHitOutcome out;
  MonsterSystem* ms = ctx_.sys.monsters;
  if (ms == nullptr) return out;
  const MonsterInstance* m = ms->Find(h.target);
  if (m == nullptr || !m->IsAlive()) return out;
  out.attempted = true;
  Hero& hero = *ctx_.sys.hero;
  const Combatant attacker = HeroCombatant();
  const Combatant defender = MonsterCombatant(*m);
  SkillHitInput in;
  if (h.skill != nullptr) {
    in.skill = h.skill;
    in.level = h.level;
    in.synergyFactor = hero.Skills().SynergyFactor(*h.skill);
  }
  DamageResult r = CalculateDamage(Rules(), attacker, defender, in, h.forceCrit, ctx_.Rand(RngStream::Combat));
  const std::string skillId = h.skill != nullptr ? h.skill->id : std::string();
  if (r.isDodged) {  // C1 / FIX Q9: a dodged hit applies nothing
    EmitMiss(h.target, Faction::Monster, kHeroEntityId, skillId, false);
    out.dodged = true;
    return out;
  }
  if (h.damageShare < 1.0) {  // C4 ground tick: the same total damage spread over the ticks
    const double share = (std::max)(0.0, h.damageShare);
    r.damage = SaturatingInt32((std::max)(1.0, std::floor(r.damage * share)));
    r.lifeStolen = SaturatingInt32(std::floor(r.damage * ctx_.equip.Get(Stat::LifeSteal) / 100.0));
    r.manaStolen = SaturatingInt32(std::floor(r.damage * ctx_.equip.Get(Stat::ManaSteal) / 100.0));
    if (!(ctx_.equip.Get(Stat::LifeSteal) > 0)) r.lifeStolen = 0;
    if (!(ctx_.equip.Get(Stat::ManaSteal) > 0)) r.manaStolen = 0;
  }
  int32_t dealt = r.damage;
  if (h.skill != nullptr && h.skill->hasBonusVsStatus && ctx_.sys.status != nullptr &&
      ctx_.sys.status->Has(h.target, h.skill->bonusVsStatus)) {
    dealt = SaturatingInt32(std::floor(dealt * h.skill->bonusVsStatusMul));  // combustion x1.5 vs burning
  }
  ConsumeCritBonus();
  ApplySteal(r.damage, r.isCrit, r.lifeStolen, r.manaStolen);  // uses r, not the combustion value (classes 9.7)

  MonsterHitRequest req;
  req.monster = h.target;
  req.amount = dealt;
  req.isCrit = r.isCrit;
  req.hasFrom = true;
  req.from = h.hasFrom ? h.from : hero.Position();
  req.attacker = kHeroEntityId;
  req.source = h.skill != nullptr ? KillSource::HeroSkill : KillSource::HeroBasic;
  req.element = h.skill != nullptr ? h.skill->damageType : DamageType::Physical;
  req.impactColor = h.skill != nullptr ? h.skill->impactColor
                                       : ClassImpactColor(ctx_.data.Combat().hitFeedback, hero.Class());
  req.skillId = skillId;
  req.numberSlot = h.numberSlot;
  req.impactBurst = h.impactBurst;
  out.weight = DamageMonster(req);
  out.damage = dealt;
  out.crit = r.isCrit;
  const MonsterInstance* after = ms->Find(h.target);
  out.killed = after == nullptr || !after->IsAlive();
  return out;
}

// resolvePlayerStrike (combat 4.2) at the contact beat (T3).
void CombatSystem::ResolveHeroStrike(EntityId target) {
  Hero& hero = *ctx_.sys.hero;
  if (!HeroAlive() || ctx_.session.transitioning || HeroImmobilized()) return;  // C5: a stun interrupts the swing
  const MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(target) : nullptr;
  if (m == nullptr || !m->IsAlive()) return;
  const EquipStats& eq = ctx_.equip;

  // 1. dodgeCounter: consumed even if the target then dodges (QUIRK kept).
  const bool forceCrit = hero.dodgeCounterReady;
  if (forceCrit) {
    hero.dodgeCounterReady = false;
    ctx_.events.Log(MakeLoc("zone.combat.dodgeCounterCrit"), LogType::Combat);
  }
  HeroHitSpec spec;
  spec.target = target;
  spec.forceCrit = forceCrit;
  const HeroHitOutcome first = HeroHitMonster(spec);
  if (!first.attempted || first.dodged) return;  // MISS: no procs, stealth not consumed

  // 7. vanish: every stealthDamage buff ends with a landed basic attack.
  hero.Buffs().RemoveStat(BuffStat::StealthDamage);

  // 10. critDoubleStrike.
  Rng& rng = ctx_.Rand(RngStream::Combat);
  auto alive = [&]() {
    const MonsterInstance* x = ctx_.sys.monsters->Find(target);
    return x != nullptr && x->IsAlive();
  };
  if (first.crit && eq.Get(Stat::CritDoubleStrike) > 0 && alive() &&
      CheckCritDoubleStrike(eq.Get(Stat::CritDoubleStrike), true, rng)) {
    HeroHitSpec extra;
    extra.target = target;
    extra.numberSlot = HitNumberSlot::DoubleStrike;
    const HeroHitOutcome o = HeroHitMonster(extra);
    if (o.attempted && !o.dodged) ctx_.events.Log(MakeLoc("zone.combat.comboTrigger"), LogType::Combat);
  }
  // 11. doubleShot (needs attackRange > 2: never for the 1.5-tile heroes, kept for data parity).
  if (eq.Get(Stat::DoubleShot) > 0 && alive() &&
      CheckDoubleShot(eq.Get(Stat::DoubleShot), hero.Derived().attackRange, rng)) {
    HeroHitSpec extra;
    extra.target = target;
    extra.numberSlot = HitNumberSlot::DoubleShot;
    const HeroHitOutcome o = HeroHitMonster(extra);
    if (o.attempted && !o.dodged) ctx_.events.Log(MakeLoc("zone.combat.doubleArrow"), LogType::Combat);
  }
  // 12. the kill pipeline already ran inside ApplyDamage (target cleanup in OnMonsterKilled).
}

// =====================================================================================================================
// monster -> hero
// =====================================================================================================================

// resolveMonsterStrike (combat 5.2) at the contact beat (T1).
void CombatSystem::ResolveMonsterStrike(EntityId monster) {
  for (size_t i = 0; i < strikes_.size(); ++i) {
    if (strikes_[i].monster == monster) {
      strikes_.erase(strikes_.begin() + static_cast<std::ptrdiff_t>(i));
      break;
    }
  }
  MonsterSystem* ms = ctx_.sys.monsters;
  const MonsterInstance* m = ms != nullptr ? ms->Find(monster) : nullptr;
  if (m == nullptr || !m->IsAlive() || !HeroAlive() || ctx_.session.transitioning) return;
  if (ctx_.sys.status != nullptr && ctx_.sys.status->IsImmobilized(monster)) return;  // stunned mid-swing
  const Hero& hero = *ctx_.sys.hero;
  if (m->def.isRanged) {
    // 5.2: a bolt to the hero's position at contact; lands without a distance re-check.
    ProjectileSpec p;
    p.kind = ProjectileKind::MonsterBolt;
    p.source = monster;
    p.target = kHeroEntityId;
    p.from = m->pos;
    p.to = hero.Position();
    p.travelMs = MonsterBoltTravelMs(ctx_.data.Combat().projectiles, p.from, p.to);
    p.vfxId = "monster_bolt";
    p.color = m->def.projectileColor;
    if (ctx_.sys.projectiles != nullptr) ctx_.sys.projectiles->Launch(p);
    return;
  }
  const CombatInputDef& in = ctx_.data.Combat().input;
  const double reach = m->def.attackRange * in.monsterMeleeReachMul + in.monsterMeleeReachAdd;
  if (DistSq(hero.Position(), m->pos) > reach * reach) return;  // whiff: the hero stepped away during the wind-up
  ApplyMonsterHit(monster, false);
}

void CombatSystem::OnMonsterBoltArrived(const Projectile& p) {
  const MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(p.spec.source) : nullptr;
  if (m == nullptr || !m->IsAlive() || !HeroAlive() || ctx_.session.transitioning) return;
  ApplyMonsterHit(p.spec.source, true);
}

// applyMonsterHit (combat 5.3).
void CombatSystem::ApplyMonsterHit(EntityId monsterId, bool ranged) {
  MonsterSystem* ms = ctx_.sys.monsters;
  const MonsterInstance* m = ms->Find(monsterId);
  if (m == nullptr) return;
  Hero& hero = *ctx_.sys.hero;
  const double now = ctx_.Now();
  // The roll comes first (RNG consumed even when the i-frames avoid the hit).
  const DamageResult r = CalculateDamage(Rules(), MonsterCombatant(*m), HeroCombatant(), SkillHitInput{}, false,
                                         ctx_.Rand(RngStream::Combat));
  // 1. dodge-roll i-frames.
  if (dodge_.IsInvulnerable(now)) {
    if (dodge_.ClaimAvoidanceReward(now)) GainSpirit(SpiritSource::Dodge, false);
    EmitMiss(kHeroEntityId, Faction::Hero, monsterId, std::string(), true);
    return;
  }
  // 2. stat dodge.
  if (r.isDodged) {
    GainSpirit(SpiritSource::Dodge, false);
    EmitMiss(kHeroEntityId, Faction::Hero, monsterId, std::string(), false);
    if (ctx_.equip.Get(Stat::DodgeCounter) > 0) {
      hero.dodgeCounterReady = true;
      ctx_.events.Log(MakeLoc("zone.combat.dodgeCounterReady"), LogType::Combat);
    }
    return;
  }
  // 3. hit.
  const double dmg = r.damage;
  if (r.manaDamage > 0) hero.SpendMana(r.manaDamage);  // FIX Q16: the mana shield drains mana
  const HitFeedbackTable& t = ctx_.data.Combat().hitFeedback;
  const HitWeight w = ClassifyHit(t, dmg, hero.MaxHp(), r.isCrit, false, false);
  // hp loss with its feedback (recoil weight from this hit, monster hit-stop round(attackerStopMs x 0.6)), then
  // thornsHeal while still alive.
  HurtHero(dmg, r.isCrit, false, DamageType::Physical, monsterId, !ranged, w, MonsterAttackerStopMs(t, w));
  const double thorns = ctx_.equip.Get(Stat::ThornsHeal);
  if (thorns > 0 && hero.Hp() > 0) hero.Heal(PercentOfMaxHp(hero.MaxHp(), thorns));

  // Monster on-hit statuses (C6: data rules, evaluated in order).
  m = ms->Find(monsterId);
  if (m == nullptr) return;
  const double monsterDamage = m->def.damage;
  const std::vector<StatusRule> rules = m->def.onHitStatus;
  for (const StatusRule& rule : rules) {
    StatusRuleInput sin;
    sin.monsterDamage = monsterDamage;
    const StatusRuleRoll roll = RollStatusRule(rule, sin, ctx_.Rand(RngStream::Combat));
    if (roll.applies) ApplyStatus(kHeroEntityId, roll.status, roll.value, roll.durationMs, monsterId);
  }
  // Elite on-hit (17.4).
  m = ms->Find(monsterId);
  if (m != nullptr && !m->affixes.empty()) {
    const EliteAffixTable& et = ctx_.data.Combat().eliteAffixes;
    const EliteOnHit eo = EvaluateEliteOnHit(*m, et, dmg, ctx_.Rand(RngStream::Combat));
    if (eo.extraFireDamage > 0) {
      const HitWeight fw = ClassifyHit(t, eo.extraFireDamage, hero.MaxHp(), false, false, false);
      HurtHero(eo.extraFireDamage, false, false, DamageType::Fire, monsterId, false, fw, 0);
    }
    if (eo.lifestealHeal > 0) ms->Heal(monsterId, eo.lifestealHeal);  // FIX: through heal (bar refresh)
    if (eo.freezeSlow) {
      const StatusEffectRules& sr = ctx_.data.Classes().statusRules;
      ApplyStatus(kHeroEntityId, sr.eliteFrozenStatus, sr.eliteFrozenValue, sr.eliteFrozenDurationMs, monsterId);
      ctx_.events.Log(MakeLoc("zone.combat.freezeSlow"), LogType::Combat);
    }
  }
  // Death check (deathSave only on monster hits, QUIRK kept).
  if (hero.Hp() <= 0) {
    if (TryDeathSave()) return;
    KillHero(HeroDeathCause::MonsterHit);
  }
}

// =====================================================================================================================
// death
// =====================================================================================================================

void CombatSystem::KillHero(HeroDeathCause cause) {
  Hero& hero = *ctx_.sys.hero;
  if (hero.Life() == HeroLife::Dying) return;  // death is entered exactly once
  if (ctx_.sys.petCompanion != nullptr && ctx_.sys.petCompanion->TryReviveHero()) return;
  const Vec2 pos = hero.Position();
  hero.SetLife(HeroLife::Dying);
  if (hero.Hp() > 0) hero.SetHp(0);
  buffer_.Clear();
  if (ctx_.sys.locomotion != nullptr) ctx_.sys.locomotion->Stop();
  // Player.die(): spirit reset (events), death anim.
  Spirit& sp = hero.GetSpirit();
  const bool wasResonating = sp.IsResonating();
  sp.Reset();
  ctx_.events.Emit(EvSpiritChanged{0, sp.MaxValue(), false, 0});
  if (wasResonating) ctx_.events.Emit(EvResonance{false, sp.Profile().id, 0});
  EvPlayAnim anim;
  anim.entity = kHeroEntityId;
  anim.action = AnimAction::Death;
  anim.startMs = ctx_.Now();
  anim.durationMs = ctx_.data.Combat().anim.Preset(HeroRig(hero.Class())).deathDuration;
  ctx_.events.Emit(anim);
  ctx_.events.Emit(EvHeroDied{pos});
  EmitSfx(ctx_.data.Audio().rules.playerDied, pos, kHeroEntityId);
  ctx_.events.Log(MakeLoc("sys.player.death"), LogType::System);
  // handlePlayerDied: statuses cleared, death penalty / soul echo, flash + text, respawn timer.
  if (ctx_.sys.status != nullptr) ctx_.sys.status->ClearEntity(kHeroEntityId);
  if (ctx_.sys.soulEcho != nullptr) ctx_.sys.soulEcho->OnHeroDied(pos, false);
  ctx_.events.Emit(EvCameraFlash{0xffffff, 80, 0.6});
  ctx_.events.Emit(EvBanner{BannerKind::Death, MakeLoc("zone.death.text"), LocText{}});
  ctx_.bus.Publish(HeroDiedMsg{pos});
  ctx_.timers.Cancel(respawnTimer_);
  respawnTimer_ = ctx_.timers.Schedule(ctx_.Now() + ctx_.data.World().constants.deathRespawnMs, TimerOwner::Combat,
                                       static_cast<uint16_t>(CombatTimerKind::HeroRespawn));
  (void)cause;
}

// respawnAtCamp (world 9.5): full HP / MP after equipment (C1), camp 0, path and target cleared.
void CombatSystem::Respawn() {
  Hero& hero = *ctx_.sys.hero;
  respawnTimer_ = kNoTimer;
  hero.SetLife(HeroLife::Alive);
  hero.RecalcDerived(ctx_.equip);
  hero.FillHpMana();
  Vec2 camp = hero.Position();
  if (ctx_.sys.zone != nullptr && ctx_.sys.zone->HasZone()) camp = ctx_.sys.zone->CampPosition(0);
  if (ctx_.sys.locomotion != nullptr) {
    ctx_.sys.locomotion->Stop();
    ctx_.sys.locomotion->Teleport(camp, TeleportReason::Respawn);
  } else {
    hero.SetPosition(camp);
  }
  SetTargetInternal(kNoEntity);
  hero.lastAttackMs = 0;
  ctx_.events.Log(MakeLoc("sys.player.respawn"), LogType::System);
  ctx_.events.Emit(EvHeroRespawned{camp});
  ctx_.bus.Publish(HeroRespawnedMsg{camp});
}

void CombatSystem::ResolvePendingDeath() {
  if (ctx_.sys.hero->Life() != HeroLife::Dying) return;
  ctx_.timers.Cancel(respawnTimer_);
  Respawn();
}

// =====================================================================================================================
// kill pipeline (step 1)
// =====================================================================================================================

void CombatSystem::OnMonsterKilled(const MonsterKilledMsg& m) {
  Hero& hero = *ctx_.sys.hero;
  if (ctx_.sys.status != nullptr) ctx_.sys.status->ClearEntity(m.monster);
  GainSpirit(SpiritSource::Kill, false);
  // exp = floor(expReward * (1 + homeBonus.expBonus / 100 + eq.expBonus / 100)), homeBonus = homestead + pet bonuses.
  double homeExp = 0;
  if (ctx_.sys.homestead != nullptr) homeExp += ctx_.sys.homestead->TotalBonuses().Get(Stat::ExpBonus);
  if (ctx_.sys.pets != nullptr) homeExp += ctx_.sys.pets->Bonuses().Get(Stat::ExpBonus);
  const double expMul = 1 + homeExp / 100.0 + ctx_.equip.Get(Stat::ExpBonus) / 100.0;
  const int64_t exp = static_cast<int64_t>((std::max)(0.0, std::floor(m.expReward * expMul)));
  const int32_t gMin = SaturatingInt32(m.goldMin);
  const int32_t gMax = (std::max)(gMin, SaturatingInt32(m.goldMax));
  const int64_t gold = ctx_.Rand(RngStream::Loot).RandomInt(gMin, gMax);
  if (ctx_.sys.rewards != nullptr) {
    ctx_.sys.rewards->GrantExp(exp, ExpSource::Kill);
    if (gold > 0) ctx_.sys.rewards->ChangeGold(gold, GoldReason::Kill);
  }
  lastKill_ = KillRewardInfo{m.monster, exp, gold};
  // killHealPercent (C11).
  const double killHeal = ctx_.equip.Get(Stat::KillHealPercent);
  if (killHeal > 0 && hero.Hp() > 0 && hero.Life() == HeroLife::Alive) {
    const double healed = hero.Heal(PercentOfMaxHp(hero.MaxHp(), killHeal));
    if (healed > 0) {
      EvFloatingText ft;
      ft.kind = FloatingTextKind::Heal;
      ft.anchor = kHeroEntityId;
      ft.pos = hero.Position();
      ft.value = healed;
      ctx_.events.Emit(ft);
    }
  }
  EvFloatingText ftExp;
  ftExp.kind = FloatingTextKind::Exp;
  ftExp.pos = m.pos;
  ftExp.value = static_cast<double>(exp);
  ctx_.events.Emit(ftExp);
  EvFloatingText ftGold;
  ftGold.kind = FloatingTextKind::Gold;
  ftGold.pos = m.pos;
  ftGold.value = static_cast<double>(gold);
  ctx_.events.Emit(ftGold);
  // Target cleanup (the web cleared it on basic-attack kills; every kill path does here).
  if (hero.attackTarget == m.monster) SetTargetInternal(kNoEntity);
  if (indicator_ == m.monster) UpdateIndicator();
  // The dead monster's pending swing can never land.
  for (size_t i = 0; i < strikes_.size(); ++i) {
    if (strikes_[i].monster == m.monster) {
      ctx_.timers.Cancel(strikes_[i].timer);
      strikes_.erase(strikes_.begin() + static_cast<std::ptrdiff_t>(i));
      break;
    }
  }
}

// =====================================================================================================================
// targeting
// =====================================================================================================================

void CombatSystem::SetTargetInternal(EntityId target) {
  Hero& hero = *ctx_.sys.hero;
  if (hero.attackTarget == target && indicator_ == target) return;
  hero.attackTarget = target;
  if (indicator_ != target) {
    indicator_ = target;
    ctx_.events.Emit(EvTargetChanged{target});
  }
}

void CombatSystem::SetAttackTarget(EntityId monster, bool approach) {
  if (!HeroAlive()) return;
  const MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(monster) : nullptr;
  if (m == nullptr || !m->IsAlive()) return;
  const Vec2 mpos = m->pos;
  SetTargetInternal(monster);
  if (!approach || ctx_.sys.locomotion == nullptr) return;
  const double range = ctx_.sys.hero->Derived().attackRange;
  if (DistSq(ctx_.sys.hero->Position(), mpos) > range * range) {
    ctx_.sys.locomotion->Approach(monster, range);  // C7: stop at attack range
  }
}

void CombatSystem::ClearAttackTarget() {
  Hero& hero = *ctx_.sys.hero;
  if (hero.attackTarget == kNoEntity) return;
  hero.attackTarget = kNoEntity;
  UpdateIndicator();
}

void CombatSystem::CycleTarget() {
  if (!HeroAlive() || ctx_.sys.monsters == nullptr) return;
  std::vector<TargetCandidate> cands;
  ctx_.sys.monsters->Candidates(cands);
  const EntityId next = abyss::CycleTarget(cands, ctx_.sys.hero->Position(),
                                           ctx_.data.Combat().input.targetCycleRangeTiles, ctx_.sys.hero->attackTarget);
  Hero& hero = *ctx_.sys.hero;
  hero.attackTarget = next;
  if (indicator_ != next || next == kNoEntity) {
    indicator_ = next;
    ctx_.events.Emit(EvTargetChanged{next});
  }
}

void CombatSystem::SetAutoCombat(bool on) {
  Hero& hero = *ctx_.sys.hero;
  hero.autoCombat = on;
  ctx_.events.Log(MakeLoc("zone.combat.autoCombat",
                          {KeyArg("state", on ? "zone.combat.autoCombatOn" : "zone.combat.autoCombatOff")}),
                  LogType::Combat);
}

EntityId CombatSystem::AttackTarget() const { return ctx_.sys.hero->attackTarget; }

// findPreferredSkillTarget (9.2): the lock if alive, else the nearest alive monster anywhere on the map.
EntityId CombatSystem::PreferredTarget() const {
  const MonsterSystem* ms = ctx_.sys.monsters;
  if (ms == nullptr) return kNoEntity;
  const EntityId lock = ctx_.sys.hero->attackTarget;
  if (lock != kNoEntity) {
    const MonsterInstance* m = ms->Find(lock);
    if (m != nullptr && m->IsAlive()) return lock;
  }
  double radius = 1e6;
  if (ctx_.sys.zone != nullptr && ctx_.sys.zone->HasZone()) {
    radius = (std::max)(ctx_.sys.zone->Grid().Cols(), ctx_.sys.zone->Grid().Rows());
  }
  return ms->NearestAlive(ctx_.sys.hero->Position(), radius);
}

EntityId CombatSystem::IndicatorTarget() const { return indicator_; }

// updateTargetIndicator (9.5): the lock (a dead lock is cleared), else the nearest aggro monster; EvTargetChanged
// whenever the shown target changes.
void CombatSystem::UpdateIndicator() {
  Hero& hero = *ctx_.sys.hero;
  const MonsterSystem* ms = ctx_.sys.monsters;
  EntityId shown = kNoEntity;
  if (hero.attackTarget != kNoEntity) {
    const MonsterInstance* m = ms != nullptr ? ms->Find(hero.attackTarget) : nullptr;
    if (m != nullptr && m->IsAlive()) {
      shown = hero.attackTarget;
    } else {
      hero.attackTarget = kNoEntity;
    }
  }
  if (shown == kNoEntity && hero.attackTarget == kNoEntity && ms != nullptr) shown = ms->NearestAggro(hero.Position());
  if (shown != indicator_) {
    indicator_ = shown;
    ctx_.events.Emit(EvTargetChanged{shown});
  }
}

// =====================================================================================================================
// skills: request / gates
// =====================================================================================================================

SkillRequestResult CombatSystem::RequestSkillSlot(int32_t slot, const SkillAim& aim) {
  if (slot < 0 || slot >= SkillBook::kHotbarSlots) return SkillRequestResult::Locked;
  const int32_t idx = ctx_.sys.hero->Skills().HotbarSkill(slot);
  if (idx < 0) return SkillRequestResult::Locked;
  return RequestSkill(idx, aim);
}

SkillRequestResult CombatSystem::RequestSkill(int32_t skillIndex, const SkillAim& aim) {
  Hero& hero = *ctx_.sys.hero;
  // save-ui-input 5.1.1: a Dying hero's skill requests are rejected (no buffering); the world freeze is gated upstream.
  if (hero.Life() != HeroLife::Alive || ctx_.clock.IsFrozen()) return SkillRequestResult::Blocked;
  const SkillBook& book = hero.Skills();
  if (skillIndex < 0 || static_cast<size_t>(skillIndex) >= book.SkillCount() || book.Level(skillIndex) <= 0) {
    ctx_.events.Log(MakeLoc("zone.combat.skillLocked"), LogType::Combat);
    return SkillRequestResult::Locked;
  }
  if (book.Skill(skillIndex).passive) return SkillRequestResult::Passive;  // C1 / FIX Q4
  // An explicit target (touch tap) becomes the lock for this cast.
  if (aim.target != kNoEntity && aim.target != hero.attackTarget) {
    const MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(aim.target) : nullptr;
    if (m != nullptr && m->IsAlive()) SetTargetInternal(aim.target);
  }
  const double now = ctx_.Now();
  if (buffer_.Request(skillIndex, now, CanExecuteSkill(skillIndex)) == InputBuffer::RequestResult::ExecuteNow) {
    TryUseSkill(skillIndex, aim);
    return SkillRequestResult::Executed;
  }
  bufferedAim_ = aim;
  ctx_.events.Emit(EvSkillBuffered{book.Skill(skillIndex).id, buffer_.ExpiresAtMs()});
  return SkillRequestResult::Buffered;
}

// The shared gate of canExecuteSkill / tryUseSkill (classes 9.2-9.3 with C1: passives false, Q6 range + slack for
// targeted buffs (rangeCheck data), Q8 death_mark / shadow_step need a target; C5: an immobilized hero cannot cast).
bool CombatSystem::SkillUsableNow(int32_t skillIndex, bool logFailures, EntityId* outTarget) const {
  const Hero& hero = *ctx_.sys.hero;
  if (!HeroAlive()) return false;
  const SkillBook& book = hero.Skills();
  if (skillIndex < 0 || static_cast<size_t>(skillIndex) >= book.SkillCount()) return false;
  const SkillDef& s = book.Skill(skillIndex);
  if (book.Level(skillIndex) <= 0) {
    if (logFailures) ctx_.events.Log(MakeLoc("zone.combat.skillLocked"), LogType::Combat);
    return false;
  }
  if (s.passive) return false;
  if (!book.IsReady(skillIndex, ctx_.Now())) return false;
  if (hero.Mana() < CastManaCost(skillIndex)) {
    if (logFailures) ctx_.events.Log(MakeLoc("zone.combat.manaInsufficient"), LogType::Combat);
    return false;
  }
  if (HeroImmobilized()) {
    if (logFailures && s.execKind == SkillExecKind::Teleport) {
      ctx_.events.Log(MakeLoc("zone.teleport.blockedByCC"), LogType::Combat);
    }
    return false;
  }
  const EntityId target = PreferredTarget();
  if (outTarget != nullptr) *outTarget = target;
  if (target == kNoEntity) {
    if (s.requiresTarget) return false;
    return s.hasBuff || s.aoe || s.execKind == SkillExecKind::Teleport;
  }
  if (!s.rangeCheck) return true;
  const MonsterInstance* m = ctx_.sys.monsters->Find(target);
  if (m == nullptr) return false;
  const double reach = SkillReach(s);
  return DistSq(hero.Position(), m->pos) <= reach * reach;
}

bool CombatSystem::CanExecuteSkill(int32_t skillIndex) const { return SkillUsableNow(skillIndex, false, nullptr); }

void CombatSystem::ConsumeBufferedSkill() {
  if (!buffer_.HasPending()) return;
  const int32_t idx = buffer_.ConsumeReady(ctx_.Now(), [this](int32_t i) { return CanExecuteSkill(i); });
  if (idx >= 0) TryUseSkill(idx, bufferedAim_);
}

// tryUseSkill (classes 9.3): commit (cooldown, mana, SKILL_USED), freeCast, animation, release now or on the beat.
void CombatSystem::TryUseSkill(int32_t skillIndex, const SkillAim& aim) {
  EntityId target = kNoEntity;
  if (!SkillUsableNow(skillIndex, true, &target)) return;
  Hero& hero = *ctx_.sys.hero;
  SkillBook& book = hero.Skills();
  const SkillDef& s = book.Skill(skillIndex);
  const int32_t level = book.Level(skillIndex);
  const double now = ctx_.Now();
  const int32_t cost = CastManaCost(skillIndex);

  book.StartCooldown(skillIndex, now + SkillCooldownMs(ctx_.data.Classes().skillRules, s, level,
                                                       ctx_.equip.Get(Stat::CooldownReduction)));
  hero.SpendMana(cost);
  ctx_.events.Emit(EvSkillUsed{s.id, s.damageType, HotbarSlotOf(skillIndex)});
  EmitSfx(SfxForSkill(ctx_.data.Audio().rules, s.damageType), hero.Position(), kHeroEntityId);
  if (CheckFreeCast(ctx_.equip.Get(Stat::FreeCast), ctx_.Rand(RngStream::Combat))) {
    hero.RestoreMana(cost);
    ctx_.events.Log(MakeLoc("zone.combat.freeCast"), LogType::Combat);
  }

  // Animation (classes 9.3 step 8): cast for buff / aoe / range > 2 (exported animKind), else the basic-attack contact.
  const AnimTimingTable& at = ctx_.data.Combat().anim;
  const AssetManifest& manifest = ctx_.data.Assets();
  const std::string art(EnumName(hero.Class()));
  const AnimRig rig = HeroRig(hero.Class());
  const MonsterInstance* tm = target != kNoEntity ? ctx_.sys.monsters->Find(target) : nullptr;
  ActionTiming timing;
  AnimAction action = AnimAction::Cast;
  bool faceTarget = false;
  if (s.animKind == SkillAnimKind::Attack && tm != nullptr) {
    timing = ComputeAttackTiming(at, manifest, art, rig, hero.Derived().attackSpeedMs, "Attack01");
    action = AnimAction::Attack;
    faceTarget = true;
  } else {
    timing = ComputeCastTiming(at, manifest, art, rig, "Cast01");
    faceTarget = tm != nullptr && !s.hasBuff;
  }
  EvPlayAnim anim;
  anim.entity = kHeroEntityId;
  anim.action = action;
  anim.clip = s.id;
  anim.startMs = now;
  anim.contactMs = timing.contactMs;
  anim.durationMs = timing.durationMs;
  anim.playRate = timing.playRate;
  if (faceTarget && tm != nullptr) {
    anim.hasFaceTarget = true;
    anim.faceTarget = tm->pos;
    hero.SetFacing(CmbDirOr(tm->pos - hero.Position(), hero.Facing()));
  }
  ctx_.events.Emit(anim);

  if (s.instantRelease || !(timing.contactMs > 0)) {
    ReleaseSkill(skillIndex, level, target, aim, cost);
    return;
  }
  PendingRelease pr;
  pr.skillIndex = skillIndex;
  pr.level = level;
  pr.target = target;
  pr.aim = aim;
  pr.manaCost = cost;
  const int32_t slot = AllocRelease(pr);
  ctx_.timers.Schedule(now + timing.contactMs, TimerOwner::Combat, static_cast<uint16_t>(CombatTimerKind::SkillRelease),
                       kHeroEntityId, target, slot);
}

int32_t CombatSystem::AllocRelease(const PendingRelease& r) {
  for (size_t i = 0; i < releases_.size(); ++i) {
    if (!releases_[i].used) {
      releases_[i] = r;
      releases_[i].used = true;
      return static_cast<int32_t>(i);
    }
  }
  releases_.push_back(r);
  releases_.back().used = true;
  return static_cast<int32_t>(releases_.size() - 1);
}

int32_t CombatSystem::AllocHit(PendingHit h) {
  h.used = true;
  for (size_t i = 0; i < hits_.size(); ++i) {
    if (!hits_[i].used) {
      hits_[i] = std::move(h);
      return static_cast<int32_t>(i);
    }
  }
  hits_.push_back(std::move(h));
  return static_cast<int32_t>(hits_.size() - 1);
}

// =====================================================================================================================
// skills: release (classes 9.5)
// =====================================================================================================================

void CombatSystem::ReleaseSkill(int32_t skillIndex, int32_t level, EntityId target, const SkillAim& aim,
                                int32_t manaCost) {
  const SkillDef& s = ctx_.sys.hero->Skills().Skill(skillIndex);
  switch (s.execKind) {
    case SkillExecKind::Teleport:
      ReleaseTeleport(s, level, target, aim, manaCost);
      return;
    case SkillExecKind::ShadowStep:
      if (target != kNoEntity) ReleaseShadowStep(s, level, target);  // FIX Q8: no buff fallback without a target
      return;
    case SkillExecKind::DeathMark:
      if (target != kNoEntity) ReleaseDeathMark(s, level, target);
      return;
    case SkillExecKind::SlowTrap: {
      const SkillRules& rules = ctx_.data.Classes().skillRules;
      const double radius = SkillAoeRadius(rules, s, level);
      const Vec2 hero = ctx_.sys.hero->Position();
      if (s.port.persistentGround && ctx_.sys.projectiles != nullptr) {  // C4: an armed trap at the hero's feet
        GroundEffectSpec g;
        g.source = kHeroEntityId;
        g.skillIndex = skillIndex;
        g.skillLevel = level;
        g.skillId = s.id;
        g.vfxId = s.vfxId;
        g.center = hero;
        g.radius = radius;
        g.durationMs = s.port.groundDurationMs;
        g.trigger = s.port.groundTrigger;
        g.ticks = (std::max)(1, s.port.groundTicks);
        g.damageShare = 1.0 / g.ticks;
        ctx_.sys.projectiles->StartGroundEffect(g);
      } else {
        SlowTrapHits(s, level, hero, radius, 1.0);
      }
      return;
    }
    case SkillExecKind::Buff:
      if (s.hasBuff) ReleaseBuff(s, level);
      return;
    case SkillExecKind::Aoe:
      if (s.aoe && SkillAoeRadius(ctx_.data.Classes().skillRules, s, level) > 0) {
        ReleaseAoe(s, level, target);
      } else if (target != kNoEntity) {
        ReleaseSingle(s, level, target);
      }
      return;
    case SkillExecKind::Single:
      if (target != kNoEntity) ReleaseSingle(s, level, target);
      return;
  }
}

void CombatSystem::ReleaseTeleport(const SkillDef& s, int32_t level, EntityId target, const SkillAim& aim,
                                   int32_t manaCost) {
  Hero& hero = *ctx_.sys.hero;
  ZoneRuntime* zone = ctx_.sys.zone;
  if (zone == nullptr || !zone->HasZone()) return;
  TeleportAim ta;
  ta.hasPoint = aim.hasPoint;
  ta.point = aim.point;
  ta.stickDir = aim.stickDir;
  if (target != kNoEntity) {
    if (const MonsterInstance* m = ctx_.sys.monsters->Find(target)) {
      ta.hasTarget = true;
      ta.targetPos = m->pos;
    }
  }
  ta.facing = hero.Facing();
  TilePos dest;
  const WalkableFn walkable = [zone](int32_t c, int32_t r) { return zone->Walkable(c, r); };
  if (!ComputeTeleportDestination(s.port, hero.Position(), ta, zone->Grid().Cols(), zone->Grid().Rows(), walkable,
                                  dest)) {
    ctx_.events.Log(MakeLoc("zone.teleport.unreachable"), LogType::Combat);
    hero.RestoreMana(manaCost);  // cooldown stays spent
    return;
  }
  const Vec2 from = hero.Position();
  if (ctx_.sys.locomotion != nullptr) {
    ctx_.sys.locomotion->Teleport(dest.Center(), TeleportReason::Skill);
  } else {
    hero.SetPosition(dest.Center());
  }
  SetTargetInternal(kNoEntity);
  EvSkillVfx v;
  v.skillId = s.id;
  v.vfxId = s.vfxId;
  v.caster = kHeroEntityId;
  v.origin = from;
  v.point = dest.Center();
  ctx_.events.Emit(v);
  ctx_.events.Log(MakeLoc("zone.combat.skillActivated", {CmbSkillNameArg(s)}), LogType::Combat);
  (void)level;
}

void CombatSystem::ReleaseShadowStep(const SkillDef& s, int32_t level, EntityId target) {
  Hero& hero = *ctx_.sys.hero;
  ZoneRuntime* zone = ctx_.sys.zone;
  const MonsterInstance* m = ctx_.sys.monsters->Find(target);
  if (m == nullptr || !m->IsAlive() || zone == nullptr || !zone->HasZone()) return;
  const WalkableFn walkable = [zone](int32_t c, int32_t r) { return zone->Walkable(c, r); };
  const TilePos dest =
      ShadowStepDestination(hero.Position(), m->pos, zone->Grid().Cols(), zone->Grid().Rows(), walkable);
  const Vec2 from = hero.Position();
  const Vec2 mpos = m->pos;
  if (ctx_.sys.locomotion != nullptr) {
    ctx_.sys.locomotion->Teleport(dest.Center(), TeleportReason::Skill);
  } else {
    hero.SetPosition(dest.Center());
  }
  hero.SetFacing(CmbDirOr(mpos - dest.Center(), hero.Facing()));
  SetTargetInternal(target);
  if (s.hasBuff) {
    const SkillRules& rules = ctx_.data.Classes().skillRules;
    ActiveBuff b;
    b.stat = s.buff.stat;
    b.value = SkillBuffValue(rules, s, level);
    b.durationMs = SkillBuffDurationMs(rules, s, level);
    b.startMs = ctx_.Now();
    b.source = kHeroEntityId;
    hero.Buffs().Add(b);  // FIX Q18: read as crit points by the next hit
  }
  EvSkillVfx v;
  v.skillId = s.id;
  v.vfxId = s.vfxId;
  v.caster = kHeroEntityId;
  v.target = target;
  v.origin = from;
  v.point = dest.Center();
  ctx_.events.Emit(v);
  ctx_.events.Log(MakeLoc("zone.combat.skillActivated", {CmbSkillNameArg(s)}), LogType::Combat);
}

void CombatSystem::ReleaseDeathMark(const SkillDef& s, int32_t level, EntityId target) {
  MonsterSystem* ms = ctx_.sys.monsters;
  MonsterInstance* m = ms->Find(target);
  if (m == nullptr || !m->IsAlive()) return;
  const SkillRules& rules = ctx_.data.Classes().skillRules;
  ActiveBuff b;
  b.stat = BuffStat::DamageAmplify;
  b.value = SkillBuffValue(rules, s, level);
  b.durationMs = SkillBuffDurationMs(rules, s, level);
  b.startMs = ctx_.Now();
  b.source = kHeroEntityId;
  m->buffs.Add(b);  // on the monster (FIX Q19: pruned by duration)
  const std::string nameKey = m->def.nameKey;
  const Vec2 mpos = m->pos;
  if (s.damageMultiplier > 0) {
    HeroHitSpec h;
    h.target = target;
    h.skill = &s;
    h.level = level;
    h.applyStatusRules = false;
    h.impactBurst = false;
    HeroHitMonster(h);
  }
  EvSkillVfx v;
  v.skillId = s.id;
  v.vfxId = s.vfxId;
  v.caster = kHeroEntityId;
  v.target = target;
  v.origin = ctx_.sys.hero->Position();
  v.point = mpos;
  ctx_.events.Emit(v);
  ctx_.events.Log(MakeLoc("zone.combat.deathMarkApplied", {CmbSkillNameArg(s), KeyArg("targetName", nameKey)}),
                  LogType::Combat);
}

// Slow trap (classes 10.3): every alive monster within the radius takes the skill hit (no status rules, no impact
// burst); survivors get slow round(buffValue x 100) for buffDuration. Log slowTrapHit {count}.
void CombatSystem::SlowTrapHits(const SkillDef& s, int32_t level, Vec2 center, double radius, double share) {
  const SkillRules& rules = ctx_.data.Classes().skillRules;
  const std::vector<TargetCandidate> cands = AliveCandidates(center, radius);
  const std::vector<EntityId> targets = CandidatesInRadius(cands, center, radius);
  int32_t count = 0;
  for (EntityId id : targets) {
    const MonsterInstance* m = ctx_.sys.monsters->Find(id);
    if (m == nullptr || !m->IsAlive()) continue;
    ++count;
    HeroHitOutcome o;
    if (s.damageMultiplier > 0) {
      HeroHitSpec h;
      h.target = id;
      h.skill = &s;
      h.level = level;
      h.damageShare = share;
      h.applyStatusRules = false;
      h.impactBurst = false;
      h.hasFrom = true;
      h.from = center;
      o = HeroHitMonster(h);
      if (o.dodged || o.killed) continue;  // C1: a dodged hit applies nothing
    }
    for (const StatusRule& rule : s.statusRules) {
      StatusRuleInput in;
      in.dealtDamage = o.damage;
      in.buffValue = SkillBuffValue(rules, s, level);
      in.buffDurationMs = SkillBuffDurationMs(rules, s, level);
      const StatusRuleRoll roll = RollStatusRule(rule, in, ctx_.Rand(RngStream::Combat));
      if (roll.applies) ApplyStatus(id, roll.status, roll.value, roll.durationMs, kHeroEntityId);
    }
  }
  EvSkillVfx v;
  v.skillId = s.id;
  v.vfxId = s.vfxId;
  v.caster = kHeroEntityId;
  v.origin = center;
  v.point = center;
  v.radius = radius;
  ctx_.events.Emit(v);
  if (count > 0) {
    ctx_.events.Log(MakeLoc("zone.combat.slowTrapHit", {CmbSkillNameArg(s), {"count", ToStr(count)}}), LogType::Combat);
  }
}

// Generic self buff (classes 9.6) + taunt_roar's taunt.
void CombatSystem::ReleaseBuff(const SkillDef& s, int32_t level) {
  Hero& hero = *ctx_.sys.hero;
  const SkillRules& rules = ctx_.data.Classes().skillRules;
  const double now = ctx_.Now();
  ActiveBuff b;
  b.stat = s.buff.stat;
  b.value = SkillBuffValue(rules, s, level);
  b.durationMs = SkillBuffDurationMs(rules, s, level);
  b.startMs = now;
  b.source = kHeroEntityId;
  hero.Buffs().Add(b);
  ctx_.events.Log(MakeLoc("zone.combat.skillActivated", {CmbSkillNameArg(s)}), LogType::Combat);
  EvSkillVfx v;
  v.skillId = s.id;
  v.vfxId = s.vfxId;
  v.caster = kHeroEntityId;
  v.origin = hero.Position();
  v.point = hero.Position();
  if (s.tauntAoe && s.aoe) {
    const double radius = SkillAoeRadius(rules, s, level);
    v.radius = radius;
    const std::vector<TargetCandidate> cands = AliveCandidates(hero.Position(), radius);
    const std::vector<EntityId> targets = CandidatesInRadius(cands, hero.Position(), radius);
    for (EntityId id : targets) {
      MonsterInstance* m = ctx_.sys.monsters->Find(id);
      if (m == nullptr) continue;
      ActiveBuff taunt;
      taunt.stat = BuffStat::Taunted;
      taunt.value = 1;
      taunt.durationMs = b.durationMs;
      taunt.startMs = now;
      taunt.source = kHeroEntityId;
      m->buffs.Add(taunt);
      if (m->state == MonsterState::Idle || m->state == MonsterState::Patrol) ctx_.sys.monsters->ForceChase(id);
    }
    if (!targets.empty()) {
      ctx_.events.Log(MakeLoc("zone.combat.tauntRoar", {{"count", ToStr(targets.size())}}), LogType::Combat);
    }
  }
  ctx_.events.Emit(v);
}

void CombatSystem::ReleaseAoe(const SkillDef& s, int32_t level, EntityId target) {
  Hero& hero = *ctx_.sys.hero;
  MonsterSystem* ms = ctx_.sys.monsters;
  const SkillRules& rules = ctx_.data.Classes().skillRules;
  const double now = ctx_.Now();
  const double radius = SkillAoeRadius(rules, s, level);
  const Vec2 heroPos = hero.Position();
  const MonsterInstance* tm = target != kNoEntity ? ms->Find(target) : nullptr;
  const bool targetAlive = tm != nullptr && tm->IsAlive();
  const Vec2 targetPos = tm != nullptr ? tm->pos : heroPos;

  // Ground anchor (9.7): the target within range + 1, else the nearest alive monster within it, else the hero.
  EntityId anchor = kNoEntity;
  Vec2 center = heroPos;
  if (s.groundAnchored) {
    const double reach = s.range + rules.groundAnchorReachSlackTiles;
    const std::vector<TargetCandidate> near = AliveCandidates(heroPos, reach);
    anchor = FindGroundAoeAnchor(near, heroPos, target, targetPos, targetAlive, s.range);
    if (anchor != kNoEntity) {
      if (const MonsterInstance* am = ms->Find(anchor)) center = am->pos;
    }
  }
  const bool blastFrom = anchor != kNoEntity && !s.hasArrowDelay;

  // C4 persistent ground effects (fire_wall, arrow_rain periodic; explosive_trap, chain_trap armed at the hero's feet).
  if (s.port.persistentGround && ctx_.sys.projectiles != nullptr) {
    GroundEffectSpec g;
    g.source = kHeroEntityId;
    g.skillIndex = hero.Skills().IndexOf(s.id);
    g.skillLevel = level;
    g.skillId = s.id;
    g.vfxId = s.vfxId;
    g.center = s.port.groundTrigger == GroundTrigger::Armed ? heroPos : center;
    g.radius = radius;
    g.durationMs = s.port.groundDurationMs;
    g.trigger = s.port.groundTrigger;
    g.ticks = (std::max)(1, s.port.groundTicks);
    g.damageShare = 1.0 / g.ticks;
    ctx_.sys.projectiles->StartGroundEffect(g);
    return;
  }

  // Targets (fixed at release).
  std::vector<EntityId> targets;
  if (s.hasLineTarget && targetAlive) {
    const double halfWidth = radius * s.lineHalfWidthFactor;
    targets = CandidatesAlongLine(AliveCandidates(heroPos, s.range + halfWidth), heroPos, targetPos, target, s.range,
                                  halfWidth);
  } else if (s.port.coneDeg > 0) {
    // C4 multishot: a 50 degree fan from the hero toward the target (else along the facing).
    const Vec2 aim = targetAlive ? CmbDirOr(targetPos - heroPos, hero.Facing()) : hero.Facing();
    targets = CandidatesInCone(AliveCandidates(heroPos, radius), heroPos, aim, radius, s.port.coneDeg);
  } else {
    targets = CandidatesInRadius(AliveCandidates(center, radius), center, radius);
  }
  // C4 chain lightning: target-to-target order (nearest to the previous link), 55 ms per link.
  const int32_t stagger = s.port.chainStaggerMs;
  if (stagger > 0 && targets.size() > 1) {
    std::vector<EntityId> remaining = targets;
    std::vector<EntityId> chain;
    Vec2 prev = heroPos;
    while (!remaining.empty()) {
      size_t best = 0;
      double bestD = -1;
      for (size_t i = 0; i < remaining.size(); ++i) {
        const MonsterInstance* m = ms->Find(remaining[i]);
        const double d = m != nullptr ? DistSq(prev, m->pos) : 1e300;
        if (bestD < 0 || d < bestD) {
          best = i;
          bestD = d;
        }
      }
      chain.push_back(remaining[best]);
      if (const MonsterInstance* m = ms->Find(remaining[best])) prev = m->pos;
      remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(best));
    }
    targets.swap(chain);
  }

  EvSkillVfx v;
  v.skillId = s.id;
  v.vfxId = s.vfxId;
  v.caster = kHeroEntityId;
  v.target = target;
  v.origin = heroPos;
  v.point = s.hasLineTarget && targetAlive ? targetPos : center;
  v.radius = radius;
  for (EntityId id : targets) {
    if (const MonsterInstance* m = ms->Find(id)) v.points.push_back(m->pos);
  }
  v.staggerMs = stagger;
  ctx_.events.Emit(v);

  // Delays: the meteor batch, per-target arrows, the chain stagger; everything else lands now.
  if (s.aoeDelayMs > 0) {
    PendingHit ph;
    ph.skillIndex = hero.Skills().IndexOf(s.id);
    ph.level = level;
    ph.targets = targets;
    ph.center = center;
    ph.blastFrom = blastFrom;
    ph.batchShake = true;
    const int32_t slot = AllocHit(std::move(ph));
    ctx_.timers.Schedule(now + s.aoeDelayMs, TimerOwner::Combat,
                         static_cast<uint16_t>(CombatTimerKind::SkillDelayedHit), kHeroEntityId, kNoEntity, slot);
    return;
  }
  int32_t immediate = 0;
  for (size_t k = 0; k < targets.size(); ++k) {
    const EntityId id = targets[k];
    double delay = 0;
    if (s.hasArrowDelay) {
      if (const MonsterInstance* m = ms->Find(id)) delay = SkillArrowDelayMs(s, heroPos, m->pos);
    } else if (stagger > 0) {
      delay = static_cast<double>(stagger) * static_cast<double>(k);
    }
    if (delay > 0) {
      PendingHit ph;
      ph.skillIndex = hero.Skills().IndexOf(s.id);
      ph.level = level;
      ph.targets = {id};
      ph.center = center;
      ph.blastFrom = blastFrom;
      ph.batchShake = false;
      const int32_t slot = AllocHit(std::move(ph));
      ctx_.timers.Schedule(now + delay, TimerOwner::Combat, static_cast<uint16_t>(CombatTimerKind::SkillDelayedHit),
                           kHeroEntityId, id, slot);
    } else {
      const HeroHitOutcome o = ApplySkillHit(s, level, id, blastFrom, center, 1.0);
      if (o.attempted && !o.dodged) ++immediate;
    }
  }
  if (immediate > 0) {
    const ShakeRequest sh = AoeHitShake(ctx_.data.Combat().hitFeedback, immediate);
    Shake(sh.durationMs, sh.intensity);
  }
}

void CombatSystem::FireDelayedHit(int32_t slot) {
  if (slot < 0 || static_cast<size_t>(slot) >= hits_.size() || !hits_[static_cast<size_t>(slot)].used) return;
  const PendingHit ph = hits_[static_cast<size_t>(slot)];
  hits_[static_cast<size_t>(slot)].used = false;
  if (!HeroAlive() || ctx_.session.transitioning) return;  // stillCasting()
  const SkillBook& book = ctx_.sys.hero->Skills();
  if (ph.skillIndex < 0 || static_cast<size_t>(ph.skillIndex) >= book.SkillCount()) return;
  const SkillDef& s = book.Skill(ph.skillIndex);
  int32_t hits = 0;
  for (EntityId id : ph.targets) {
    const HeroHitOutcome o = ApplySkillHit(s, ph.level, id, ph.blastFrom, ph.center, 1.0);
    if (o.attempted && !o.dodged) ++hits;
  }
  if (ph.batchShake && hits > 0) {
    const ShakeRequest sh = AoeHitShake(ctx_.data.Combat().hitFeedback, hits);
    Shake(sh.durationMs, sh.intensity);
  }
}

// Single target (classes 9.8): projectile on arrival, C4 Charge after its dash, else now.
void CombatSystem::ReleaseSingle(const SkillDef& s, int32_t level, EntityId target) {
  Hero& hero = *ctx_.sys.hero;
  const MonsterInstance* m = ctx_.sys.monsters->Find(target);
  if (m == nullptr || !m->IsAlive()) return;
  const Vec2 heroPos = hero.Position();
  const Vec2 mpos = m->pos;
  const int32_t skillIndex = hero.Skills().IndexOf(s.id);

  if (s.port.hasDash && ctx_.sys.locomotion != nullptr) {
    const double stop = hero.Derived().attackRange;
    const Vec2 end = ChargeDashEnd(heroPos, mpos, stop);
    if (DistSq(heroPos, end) > 1e-12 &&
        ctx_.sys.locomotion->StartDash(end, static_cast<double>(s.port.dashDurationMs), s.id)) {
      PendingRelease pr;
      pr.skillIndex = skillIndex;
      pr.level = level;
      pr.target = target;
      const int32_t slot = AllocRelease(pr);
      ctx_.timers.Schedule(ctx_.Now() + s.port.dashDurationMs, TimerOwner::Combat,
                           static_cast<uint16_t>(CombatTimerKind::ChargeDashEnd), kHeroEntityId, target, slot);
      return;
    }
  }

  EvSkillVfx v;
  v.skillId = s.id;
  v.vfxId = s.vfxId;
  v.caster = kHeroEntityId;
  v.target = target;
  v.origin = heroPos;
  v.point = mpos;
  const double travel = SkillTravelMs(s, heroPos, mpos);
  if (travel > 0 && ctx_.sys.projectiles != nullptr) {
    ProjectileSpec p;
    p.kind = ProjectileKind::HeroSkill;
    p.source = kHeroEntityId;
    p.target = target;
    p.from = heroPos;
    p.to = mpos;
    p.travelMs = travel;
    p.vfxId = s.vfxId;
    p.color = s.impactColor;
    p.skillIndex = skillIndex;
    p.skillLevel = level;
    ctx_.sys.projectiles->Launch(p);
    return;
  }
  ctx_.events.Emit(v);
  ApplySkillHit(s, level, target, false, heroPos, 1.0);
}

void CombatSystem::ResolveChargeDash(int32_t slot) {
  if (slot < 0 || static_cast<size_t>(slot) >= releases_.size() || !releases_[static_cast<size_t>(slot)].used) return;
  const PendingRelease pr = releases_[static_cast<size_t>(slot)];
  releases_[static_cast<size_t>(slot)].used = false;
  if (!HeroAlive() || ctx_.session.transitioning) return;
  const SkillBook& book = ctx_.sys.hero->Skills();
  if (pr.skillIndex < 0 || static_cast<size_t>(pr.skillIndex) >= book.SkillCount()) return;
  const SkillDef& s = book.Skill(pr.skillIndex);
  const MonsterInstance* m = ctx_.sys.monsters->Find(pr.target);
  if (m == nullptr || !m->IsAlive()) return;
  // The hit lands where the hero stands: it needs the target within melee reach (attack range + range slack), which an
  // interrupted dash (C5 stun / freeze) may not reach.
  const double reach = ctx_.sys.hero->Derived().attackRange + ctx_.data.Classes().skillRules.rangeSlackTiles;
  if (DistSq(ctx_.sys.hero->Position(), m->pos) > reach * reach) return;
  EvSkillVfx v;
  v.skillId = s.id;
  v.vfxId = s.vfxId;
  v.caster = kHeroEntityId;
  v.target = pr.target;
  v.origin = ctx_.sys.hero->Position();
  v.point = m->pos;
  ctx_.events.Emit(v);
  ApplySkillHit(s, pr.level, pr.target, false, ctx_.sys.hero->Position(), 1.0);
}

void CombatSystem::OnSkillProjectileArrived(const Projectile& p) {
  if (!HeroAlive() || ctx_.session.transitioning) return;
  const SkillBook& book = ctx_.sys.hero->Skills();
  if (p.spec.skillIndex < 0 || static_cast<size_t>(p.spec.skillIndex) >= book.SkillCount()) return;
  const MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(p.spec.target) : nullptr;
  if (m == nullptr || !m->IsAlive()) return;  // target-locked: lands wherever the target is, unless it died
  ApplySkillHit(book.Skill(p.spec.skillIndex), p.spec.skillLevel, p.spec.target, false, p.spec.from, 1.0);
}

void CombatSystem::OnGroundEffectTick(const GroundEffect& g, int32_t tickIndex) {
  if (!HeroAlive() || ctx_.session.transitioning) return;
  const SkillBook& book = ctx_.sys.hero->Skills();
  if (g.spec.skillIndex < 0 || static_cast<size_t>(g.spec.skillIndex) >= book.SkillCount()) return;
  const SkillDef& s = book.Skill(g.spec.skillIndex);
  if (s.execKind == SkillExecKind::SlowTrap) {
    SlowTrapHits(s, g.spec.skillLevel, g.spec.center, g.spec.radius, g.spec.damageShare);
    return;
  }
  const std::vector<EntityId> targets =
      CandidatesInRadius(AliveCandidates(g.spec.center, g.spec.radius), g.spec.center, g.spec.radius);
  if (tickIndex == 0 || g.spec.trigger == GroundTrigger::Armed) {
    EvSkillVfx v;
    v.skillId = s.id;
    v.vfxId = s.vfxId;
    v.caster = kHeroEntityId;
    v.origin = g.spec.center;
    v.point = g.spec.center;
    v.radius = g.spec.radius;
    ctx_.events.Emit(v);
  }
  int32_t hits = 0;
  for (EntityId id : targets) {
    const HeroHitOutcome o = ApplySkillHit(s, g.spec.skillLevel, id, true, g.spec.center, g.spec.damageShare);
    if (o.attempted && !o.dodged) ++hits;
  }
  if (hits > 0) {
    const ShakeRequest sh = AoeHitShake(ctx_.data.Combat().hitFeedback, hits);
    Shake(sh.durationMs, sh.intensity);
  }
}

// Shared skill hit (classes 9.7 applyHit / 9.8): calculateDamage with the skill + synergy, combustion bonus,
// takeDamage, steal, status rules (alive targets), kill credit, impact burst (skill colour).
CombatSystem::HeroHitOutcome CombatSystem::ApplySkillHit(const SkillDef& s, int32_t level, EntityId target,
                                                         bool blastFrom, Vec2 center, double share) {
  const MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(target) : nullptr;
  if (m == nullptr || !m->IsAlive()) return {};
  HeroHitSpec h;
  h.target = target;
  h.skill = &s;
  h.level = level;
  h.damageShare = share;
  h.applyStatusRules = true;
  h.impactBurst = s.execKind != SkillExecKind::DeathMark && s.execKind != SkillExecKind::SlowTrap;
  h.hasFrom = true;
  h.from = blastFrom && Dist(m->pos, center) > kCombatBlastCentreTiles ? center : ctx_.sys.hero->Position();
  const HeroHitOutcome o = HeroHitMonster(h);
  if (o.attempted && !o.dodged && !o.killed) ApplySkillStatuses(s, level, target, o.damage);
  return o;
}

void CombatSystem::ApplySkillStatuses(const SkillDef& s, int32_t level, EntityId target, double dealt) {
  const SkillRules& rules = ctx_.data.Classes().skillRules;
  for (const StatusRule& rule : s.statusRules) {
    StatusRuleInput in;
    in.dealtDamage = dealt;
    in.buffValue = SkillBuffValue(rules, s, level);
    in.buffDurationMs = SkillBuffDurationMs(rules, s, level);
    const StatusRuleRoll roll = RollStatusRule(rule, in, ctx_.Rand(RngStream::Combat));
    if (!roll.applies) continue;
    const MonsterInstance* m = ctx_.sys.monsters->Find(target);
    if (m == nullptr || !m->IsAlive()) return;
    ApplyStatus(target, roll.status, roll.value, roll.durationMs, kHeroEntityId);
  }
}

// =====================================================================================================================
// dodge
// =====================================================================================================================

bool CombatSystem::RequestDodge(Vec2 requestedDir) {
  Hero& hero = *ctx_.sys.hero;
  const double now = ctx_.Now();
  if (!HeroAlive() || !dodge_.CanStart(now) || HeroImmobilized()) return false;
  ZoneRuntime* zone = ctx_.sys.zone;
  if (zone == nullptr || !zone->HasZone()) return false;
  if (ctx_.sys.locomotion != nullptr && ctx_.sys.locomotion->IsDashing()) return false;
  const CombatInputDef& def = ctx_.data.Combat().input;
  Vec2 dest;
  const auto walkable = [zone](int32_t c, int32_t r) { return zone->Walkable(c, r); };
  if (!ComputeDodgeDestination(def, hero.Class(), hero.Position(), requestedDir, hero.Facing(), walkable, dest)) {
    return false;  // no landing tile: no dodge, cooldown not spent
  }
  dodge_.Start(now);
  const Vec2 from = hero.Position();
  if (ctx_.sys.locomotion != nullptr) {
    ctx_.sys.locomotion->Teleport(dest, TeleportReason::Dodge);  // clears the path; the attack target is kept
  } else {
    hero.SetPosition(dest);
  }
  hero.SetFacing(CmbDirOr(dest - from, hero.Facing()));
  EvPlayAnim anim;
  anim.entity = kHeroEntityId;
  anim.action = AnimAction::Dodge;
  anim.startMs = now;
  anim.durationMs = ctx_.data.Combat().anim.Preset(HeroRig(hero.Class())).dodgeDuration;
  ctx_.events.Emit(anim);
  ctx_.events.Emit(EvDodgeStarted{dodge_.CooldownMs(), dodge_.InvulnerabilityMs(), from, dest});
  EmitSfx(ctx_.data.Audio().rules.dodgeStarted, from, kHeroEntityId);
  return true;
}

// =====================================================================================================================
// progression commands
// =====================================================================================================================

bool CombatSystem::LearnSkill(int32_t skillIndex) {
  Hero& hero = *ctx_.sys.hero;
  if (hero.Life() != HeroLife::Alive) return false;
  SkillBook& book = hero.Skills();
  if (skillIndex < 0 || static_cast<size_t>(skillIndex) >= book.SkillCount()) return false;
  const auto hotbarBefore = book.Hotbar();
  if (!book.Invest(skillIndex, hero.Level(), hero.MutableFreeSkillPoints())) return false;
  ctx_.events.Emit(EvSkillLevelChanged{book.Skill(skillIndex).id, book.Level(skillIndex)});
  if (book.Hotbar() != hotbarBefore) ctx_.events.Emit(EvHotbarChanged{});
  return true;
}

bool CombatSystem::AllocateStat(PrimaryStat stat, int32_t points) {
  Hero& hero = *ctx_.sys.hero;
  if (hero.Life() != HeroLife::Alive) return false;
  int32_t spent = 0;
  for (int32_t i = 0; i < points; ++i) {
    if (!hero.AllocateStat(stat)) break;
    ++spent;
  }
  if (spent > 0) hero.RecalcDerived(ctx_.equip);
  return spent > 0;
}

bool CombatSystem::SetHotbar(int32_t slot, int32_t skillIndex) {
  Hero& hero = *ctx_.sys.hero;
  if (!hero.Skills().SetHotbar(slot, skillIndex)) return false;
  ctx_.events.Emit(EvHotbarChanged{});
  return true;
}

// =====================================================================================================================
// per-step hooks
// =====================================================================================================================

// Step 6 (classes 6.6 / 10.1): Unyielding proc and the Dual Wield Mastery buff. The Life Regen passive heals inside
// Hero::TickRegen (step 7).
void CombatSystem::TickPassives(double dtMs) {
  (void)dtMs;
  Hero& hero = *ctx_.sys.hero;
  if (!HeroAlive()) return;
  SkillBook& book = hero.Skills();
  const SkillRules& rules = ctx_.data.Classes().skillRules;
  const double now = ctx_.Now();
  for (size_t i = 0; i < book.SkillCount(); ++i) {
    const int32_t idx = static_cast<int32_t>(i);
    const SkillDef& s = book.Skill(idx);
    const int32_t level = book.Level(idx);
    if (!s.passive || level <= 0) continue;
    switch (s.passiveRule.kind) {
      case PassiveRuleKind::LowHpProc: {
        if (hero.MaxHp() <= 0 || !(hero.Hp() / hero.MaxHp() < s.passiveRule.hpRatioBelow)) break;
        if (!book.IsReady(idx, now) || !s.hasBuff) break;
        ActiveBuff b;
        b.stat = s.buff.stat;
        b.value = SkillBuffValue(rules, s, level);
        b.durationMs = SkillBuffDurationMs(rules, s, level);
        b.startMs = now;
        b.source = kHeroEntityId;
        hero.Buffs().Add(b);
        book.StartCooldown(idx, now + SkillCooldownMs(rules, s, level, ctx_.equip.Get(Stat::CooldownReduction)));
        EvSkillVfx v;
        v.skillId = s.id;
        v.vfxId = s.vfxId;
        v.caster = kHeroEntityId;
        v.origin = hero.Position();
        v.point = hero.Position();
        ctx_.events.Emit(v);
        ctx_.events.Log(MakeLoc("zone.combat.unyieldingProc"), LogType::Combat);
        break;
      }
      case PassiveRuleKind::DualWield: {
        const InventorySystem* inv = ctx_.sys.inventory;
        if (inv == nullptr) break;
        const Inventory& items = inv->Items();
        if (items.Equipped(EquipSlot::Weapon) == nullptr || items.Equipped(EquipSlot::Offhand) == nullptr) break;
        if (hero.Buffs().FindTag(BuffTag::DualWieldMastery) != nullptr) break;
        ActiveBuff b;
        b.stat = BuffStat::DamageBonus;
        b.value = s.passiveRule.damageBonusPerLevel * level;
        b.durationMs = s.passiveRule.buffDurationMs;
        b.startMs = now;
        b.tag = BuffTag::DualWieldMastery;
        b.source = kHeroEntityId;
        hero.Buffs().Add(b);
        break;
      }
      case PassiveRuleKind::Regen:
      case PassiveRuleKind::None:
        break;
    }
  }
}

// Step 7 minus movement: spirit drain, then MP / HP regen with the campfire and poison modifiers (classes 4.1).
void CombatSystem::TickHeroUpdate(double dtMs) {
  Hero& hero = *ctx_.sys.hero;
  if (!HeroAlive()) return;
  Spirit& sp = hero.GetSpirit();
  if (sp.Update(dtMs)) {
    ctx_.events.Emit(EvSpiritChanged{sp.Value(), sp.MaxValue(), false, 0});
    ctx_.events.Emit(EvResonance{false, sp.Profile().id, 0});
  }
  RegenModifiers mods;
  if (ctx_.sys.zone != nullptr && ctx_.sys.zone->HasZone() && ctx_.sys.zone->NearCampfire(hero.Position())) {
    mods.hpMul = ctx_.data.Classes().formulas.campfireHpMultiplier;
    mods.mpMul = ctx_.data.Classes().formulas.campfireManaMultiplier;
  }
  if (ctx_.sys.status != nullptr && ctx_.sys.status->Has(kHeroEntityId, StatusType::Poison)) {
    mods.hpMul *= ctx_.data.Classes().statusRules.poisonedHpRegenMultiplier;
  }
  hero.TickRegen(dtMs, mods);
}

// Step 9 handleCombat: prune buffs (hero + FIX Q19 monsters), monster swings, the hero's basic attack; armed traps.
void CombatSystem::TickCombat() {
  Hero& hero = *ctx_.sys.hero;
  if (!HeroAlive()) return;
  const double now = ctx_.Now();
  hero.Buffs().Prune(now);
  MonsterSystem* ms = ctx_.sys.monsters;
  if (ms == nullptr) return;
  {
    std::vector<EntityId> ids;
    for (const MonsterInstance& m : ms->All()) ids.push_back(m.id);
    for (EntityId id : ids) {
      if (MonsterInstance* m = ms->Find(id)) m->buffs.Prune(now);
    }
  }

  // Monster swings (5.1).
  const CombatInputDef& in = ctx_.data.Combat().input;
  const AnimTimingTable& at = ctx_.data.Combat().anim;
  std::vector<EntityId> nearby;
  ms->QueryAlive(hero.Position(), in.monsterSwingScanRadiusTiles, nearby);
  for (EntityId id : nearby) {
    MonsterInstance* m = ms->Find(id);
    if (m == nullptr || !m->IsAlive() || m->state != MonsterState::Attack) continue;
    if (ctx_.sys.status != nullptr && ctx_.sys.status->IsImmobilized(id)) continue;
    if (m->lastAttackMs != 0 && now - m->lastAttackMs + kCombatSwingEpsilonMs < m->def.attackSpeedMs) continue;
    if (ctx_.sys.petCompanion != nullptr && ctx_.sys.petCompanion->InterceptMonsterAttack(id)) continue;
    m = ms->Find(id);
    if (m == nullptr) continue;
    m->lastAttackMs = now;
    const ActionTiming timing =
        ComputeAttackTiming(at, ctx_.data.Assets(), m->def.spriteKey, m->def.animCategory, m->def.attackSpeedMs);
    EvPlayAnim anim;
    anim.entity = id;
    anim.action = AnimAction::Attack;
    anim.clip = "Attack01";
    anim.startMs = now;
    anim.contactMs = timing.contactMs;
    anim.windupMs = timing.windupMs;
    anim.durationMs = timing.durationMs;
    anim.playRate = timing.playRate;
    anim.hasFaceTarget = true;
    anim.faceTarget = hero.Position();
    ctx_.events.Emit(anim);
    PendingStrike st;
    st.monster = id;
    st.startMs = now;
    st.windupMs = timing.windupMs;
    st.timer = ctx_.timers.Schedule(now + timing.contactMs, TimerOwner::Combat,
                                    static_cast<uint16_t>(CombatTimerKind::MonsterStrikeContact), id);
    strikes_.push_back(st);
  }

  // Hero basic attack (4.1): no auto-attack while hold-moving, dashing (C4) or immobilized (C5).
  HeroLocomotion* loco = ctx_.sys.locomotion;
  const bool holding = loco != nullptr && loco->IsHoldMoving();
  const bool dashing = loco != nullptr && loco->IsDashing();
  if (!holding && !dashing && !HeroImmobilized()) {
    EntityId target = kNoEntity;
    if (hero.attackTarget != kNoEntity) {
      const MonsterInstance* m = ms->Find(hero.attackTarget);
      if (m != nullptr && m->IsAlive()) target = hero.attackTarget;
    } else {
      target = ms->NearestAggro(hero.Position());
    }
    const MonsterInstance* tm = target != kNoEntity ? ms->Find(target) : nullptr;
    const HeroDerived& d = hero.Derived();
    if (tm != nullptr && tm->IsAlive() && DistSq(hero.Position(), tm->pos) <= d.attackRange * d.attackRange &&
        (hero.lastAttackMs == 0 || now - hero.lastAttackMs + kCombatSwingEpsilonMs >= d.attackSpeedMs)) {
      hero.lastAttackMs = now;
      const std::string art(EnumName(hero.Class()));
      const ActionTiming timing =
          ComputeAttackTiming(at, ctx_.data.Assets(), art, HeroRig(hero.Class()), d.attackSpeedMs, "Attack01");
      hero.SetFacing(CmbDirOr(tm->pos - hero.Position(), hero.Facing()));
      EvPlayAnim anim;
      anim.entity = kHeroEntityId;
      anim.action = AnimAction::Attack;
      anim.clip = "Attack01";
      anim.startMs = now;
      anim.contactMs = timing.contactMs;
      anim.windupMs = 0;
      anim.durationMs = timing.durationMs;
      anim.playRate = timing.playRate;
      anim.hasFaceTarget = true;
      anim.faceTarget = tm->pos;
      ctx_.events.Emit(anim);
      ctx_.timers.Schedule(now + timing.contactMs, TimerOwner::Combat,
                           static_cast<uint16_t>(CombatTimerKind::HeroStrikeContact), kHeroEntityId, target);
    }
  }

  if (ctx_.sys.projectiles != nullptr) ctx_.sys.projectiles->TickArmedTraps();
}

// Step 11: DoT ticks then expiries (classes 13.3; FIX Q21 inside StatusEffectSystem).
void CombatSystem::TickStatusEffects() {
  StatusEffectSystem* st = ctx_.sys.status;
  if (st == nullptr) return;
  std::vector<StatusTick> ticks;
  std::vector<StatusExpiry> expired;
  st->Tick(ctx_.Now(), ticks, expired);
  for (const StatusTick& t : ticks) {
    for (int32_t i = 0; i < t.ticks; ++i) {
      if (t.target == kHeroEntityId) {
        if (!HeroAlive()) break;
        HeroHitRequest r;
        r.amount = t.damagePerTick;
        r.isTick = true;
        r.source = t.source;
        r.element = CmbTickElement(t.type);
        r.allowDeathSave = false;  // QUIRK kept: DoT deaths bypass deathSave
        DamageHero(r);
      } else {
        const MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(t.target) : nullptr;
        if (m == nullptr || !m->IsAlive()) break;
        MonsterHitRequest r;
        r.monster = t.target;
        r.amount = t.damagePerTick;  // raw: no defense / resist / DR / amplify
        r.isTick = true;
        r.attacker = t.source;
        r.source = KillSource::StatusTick;
        r.element = CmbTickElement(t.type);
        r.provokes = false;
        DamageMonster(r);
      }
    }
  }
  for (const StatusExpiry& e : expired) {
    if (e.target == kHeroEntityId) {
      if (ctx_.sys.hero->Life() != HeroLife::Alive) continue;  // death cleared the hero's statuses
    } else {
      const MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(e.target) : nullptr;
      if (m == nullptr || !m->IsAlive()) continue;  // killed by a tick of this batch: cleared by the kill pipeline
    }
    ctx_.events.Emit(EvStatusExpired{e.target, e.type});
    ctx_.events.Log(MakeLoc("zone.statusEffect.expired", {CmbStatusNameArg(e.type)}), LogType::Combat);
  }
}

// Step 12 handleAutoCombat (9.4 with C1: the first skill that can execute; C7 approach stops at attack range).
void CombatSystem::TickAutoCombat() {
  Hero& hero = *ctx_.sys.hero;
  if (!hero.autoCombat || !HeroAlive()) return;
  MonsterSystem* ms = ctx_.sys.monsters;
  if (ms == nullptr) return;
  if (hero.attackTarget == kNoEntity) {
    if (ctx_.sys.locomotion != nullptr && ctx_.sys.locomotion->IsMoving()) return;  // never override a click path
    double radius = 1e6;
    if (ctx_.sys.zone != nullptr && ctx_.sys.zone->HasZone()) {
      radius = (std::max)(ctx_.sys.zone->Grid().Cols(), ctx_.sys.zone->Grid().Rows());
    }
    const EntityId nearest = ms->NearestAlive(hero.Position(), radius);
    if (const MonsterInstance* m = nearest != kNoEntity ? ms->Find(nearest) : nullptr) {
      if (DistSq(hero.Position(), m->pos) <= m->def.aggroRange * m->def.aggroRange) SetAttackTarget(nearest, true);
    }
  }
  const SkillBook& book = hero.Skills();
  for (size_t i = 0; i < book.SkillCount(); ++i) {  // autoSkillPriority = class definition order
    const int32_t idx = static_cast<int32_t>(i);
    if (book.Skill(idx).passive || !CanExecuteSkill(idx)) continue;
    RequestSkill(idx, SkillAim{});
    break;
  }
}

// 9.6 combat state (A2 true debounce) + 9.5 indicator.
void CombatSystem::TickCombatState() {
  Hero& hero = *ctx_.sys.hero;
  const MonsterSystem* ms = ctx_.sys.monsters;
  bool targetAlive = false;
  if (hero.attackTarget != kNoEntity && ms != nullptr) {
    const MonsterInstance* m = ms->Find(hero.attackTarget);
    targetAlive = m != nullptr && m->IsAlive();
  }
  const bool fighting = (ms != nullptr && ms->AnyAttacking()) || targetAlive;
  if (fighting) {
    if (combatOffTimer_ != kNoTimer) {  // fighting resumed inside the debounce: cancel it
      ctx_.timers.Cancel(combatOffTimer_);
      combatOffTimer_ = kNoTimer;
    }
    if (!inCombat_) {
      inCombat_ = true;
      ctx_.events.Emit(EvCombatStateChanged{true});
      ctx_.bus.Publish(CombatStateChangedMsg{true});
    }
  } else if (inCombat_ && combatOffTimer_ == kNoTimer) {
    combatOffTimer_ = ctx_.timers.Schedule(ctx_.Now() + ctx_.data.Combat().input.combatStateOffDebounceMs,
                                           TimerOwner::Combat, static_cast<uint16_t>(CombatTimerKind::CombatStateOff));
  }
  UpdateIndicator();
}

void CombatSystem::OnTimer(const Timer& t) {
  switch (static_cast<CombatTimerKind>(t.kind)) {
    case CombatTimerKind::HeroStrikeContact:
      ResolveHeroStrike(t.other);
      return;
    case CombatTimerKind::MonsterStrikeContact:
      ResolveMonsterStrike(t.entity);
      return;
    case CombatTimerKind::SkillRelease: {
      const int32_t slot = t.param;
      if (slot < 0 || static_cast<size_t>(slot) >= releases_.size() || !releases_[static_cast<size_t>(slot)].used)
        return;
      const PendingRelease pr = releases_[static_cast<size_t>(slot)];
      releases_[static_cast<size_t>(slot)].used = false;
      // Abort if the hero died, a zone transition started or the hero is stunned / frozen (C5); cost stays spent.
      if (!HeroAlive() || ctx_.session.transitioning || HeroImmobilized()) return;
      const SkillBook& book = ctx_.sys.hero->Skills();
      if (pr.skillIndex < 0 || static_cast<size_t>(pr.skillIndex) >= book.SkillCount()) return;
      const SkillDef& s = book.Skill(pr.skillIndex);
      // Retarget (9.3 step 9) with FIX Q7: the original target if alive, else the preferred target; a range-checked
      // skill whose target is now beyond range + slack takes the nearest monster in reach instead, else fizzles.
      EntityId target = pr.target;
      const MonsterInstance* m = target != kNoEntity ? ctx_.sys.monsters->Find(target) : nullptr;
      if (m == nullptr || !m->IsAlive()) target = PreferredTarget();
      if (target != kNoEntity && s.rangeCheck) {
        const MonsterInstance* tm = ctx_.sys.monsters->Find(target);
        const double reach = SkillReach(s);
        if (tm == nullptr || DistSq(ctx_.sys.hero->Position(), tm->pos) > reach * reach) {
          target = ctx_.sys.monsters->NearestAlive(ctx_.sys.hero->Position(), reach);
        }
      }
      ReleaseSkill(pr.skillIndex, pr.level, target, pr.aim, pr.manaCost);
      return;
    }
    case CombatTimerKind::SkillDelayedHit:
      FireDelayedHit(t.param);
      return;
    case CombatTimerKind::CombatStateOff:
      combatOffTimer_ = kNoTimer;
      if (inCombat_) {
        inCombat_ = false;
        ctx_.events.Emit(EvCombatStateChanged{false});
        ctx_.bus.Publish(CombatStateChangedMsg{false});
      }
      return;
    case CombatTimerKind::HeroRespawn:
      if (ctx_.sys.hero->Life() == HeroLife::Dying) Respawn();
      return;
    case CombatTimerKind::ChargeDashEnd:
      ResolveChargeDash(t.param);
      return;
    case CombatTimerKind::DeathSaveRearm:
      return;  // T11 lives in Hero::deathSaveReadyAtMs
  }
}

void CombatSystem::CancelPendingStrikes(bool emitCancelled) {
  for (const PendingStrike& s : strikes_) {
    if (ctx_.timers.Cancel(s.timer) && emitCancelled) ctx_.events.Emit(EvMonsterAttackCancelled{s.monster, false});
  }
  strikes_.clear();
}

// F1 (every freeze): the input buffer. F2 (cinematic): every pending monster contact (lastAttackMs kept).
void CombatSystem::OnFreezeBegin(bool cinematic) {
  buffer_.Clear();
  if (cinematic) CancelPendingStrikes(true);
}

void CombatSystem::OnZoneEnter() {
  Hero& hero = *ctx_.sys.hero;
  hero.Skills().ResetCooldowns();  // Q23: the web rebuilt the hero
  hero.lastAttackMs = 0;
  hero.attackTarget = kNoEntity;
  indicator_ = kNoEntity;
  buffer_.Clear();
  dodge_.Reset();
  shake_.Reset();
}

// Zone unload: every pending combat timer is dropped. A Dying hero is respawned first (at this zone's camp), so the
// death never outlives its timer.
void CombatSystem::OnZoneExit() {
  ResolvePendingDeath();
  ctx_.timers.CancelIf([](const Timer& t) { return t.owner == TimerOwner::Combat; });
  respawnTimer_ = kNoTimer;
  combatOffTimer_ = kNoTimer;
  strikes_.clear();
  releases_.clear();
  hits_.clear();
  buffer_.Clear();
  if (inCombat_) {
    inCombat_ = false;
    ctx_.events.Emit(EvCombatStateChanged{false});
    ctx_.bus.Publish(CombatStateChangedMsg{false});
  }
  Hero& hero = *ctx_.sys.hero;
  hero.attackTarget = kNoEntity;
  hero.lastAttackMs = 0;
  indicator_ = kNoEntity;
}

// =====================================================================================================================
// views
// =====================================================================================================================

Combatant CombatSystem::HeroCombatant() const {
  const Hero& hero = *ctx_.sys.hero;
  Combatant c = hero.AsCombatant();
  if (c.eq == nullptr) c.eq = &ctx_.equip;
  if (c.buffs == nullptr) c.buffs = &hero.Buffs();
  c.extraCritPercent = hero.Buffs().RawSum(BuffStat::CritBonus) * 100.0;  // FIX Q18
  return c;
}

Combatant CombatSystem::MonsterCombatant(const MonsterInstance& m) {
  Combatant c;
  c.stats = m.stats;
  c.baseDamage = m.def.damage;
  c.defense = m.def.defense;
  c.maxHp = m.maxHp;
  c.buffs = &m.buffs;
  return c;
}

bool CombatSystem::IsWindingUp(EntityId monster) const {
  const double now = ctx_.Now();
  for (const PendingStrike& s : strikes_) {
    if (s.monster == monster) return now < s.startMs + s.windupMs;
  }
  return false;
}

void CombatSystem::FillSnapshot(Snapshot& out) const {
  const double now = ctx_.Now();
  out.hero.dodgeCooldownRemainingMs = dodge_.CooldownRemainingMs(now);
  out.hero.dodgeCooldownMs = dodge_.CooldownMs();
  out.hero.invulnerable = dodge_.IsInvulnerable(now);
  out.hero.target = AttackTarget();
  out.hero.indicator = indicator_;
  out.hero.inCombat = inCombat_;
  // Hotbar extras (mana cost, cooldown length, affordability, CanExecute).
  const Hero& hero = *ctx_.sys.hero;
  const SkillBook& book = hero.Skills();
  const SkillRules& rules = ctx_.data.Classes().skillRules;
  for (SkillSlotView& v : out.hero.hotbar) {
    if (v.skillIndex < 0 || static_cast<size_t>(v.skillIndex) >= book.SkillCount()) continue;
    const SkillDef& s = book.Skill(v.skillIndex);
    const int32_t level = book.Level(v.skillIndex);
    if (level <= 0) continue;
    v.manaCost = CastManaCost(v.skillIndex);
    v.cooldownTotalMs = SkillCooldownMs(rules, s, level, ctx_.equip.Get(Stat::CooldownReduction));
    v.affordable = hero.Mana() >= v.manaCost;
    v.usable = CanExecuteSkill(v.skillIndex);
  }
  for (MonsterView& m : out.monsters) m.windingUp = IsWindingUp(m.id);
}

}  // namespace abyss
