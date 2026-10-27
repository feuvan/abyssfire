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
#include <string_view>

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
#include "abyss/monsters/MonsterAbilities.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/monsters/OnDeath.h"
#include "abyss/monsters/Packs.h"
#include "abyss/monsters/Phases.h"
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
// Labyrinth floor zone ids (DungeonSystem floors; the same convention as the music theme rule, audio 2).
constexpr std::string_view kCombatLabyrinthFloorPrefix = "dungeon_floor_";
// A death on a labyrinth floor restarts the rift that holds the portal: the web hard-codes it (ZoneScene.ts:920,
// `this.isInDungeon ? 'abyss_rift' : ...`); no data table carries it.
constexpr std::string_view kCombatLabyrinthParentZone = "abyss_rift";

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

// C14: a face / aim point closer than 1e-6 tile to the hero gives no direction.
constexpr double kCombatMinAimTilesSq = 1e-12;

// C14 no-target ground fallback: min(range, noTargetGroundAheadTiles) ahead; a trap drops at the hero's feet.
double CmbNoTargetAhead(const SkillRules& rules, const SkillDef& s, CastShape shape) {
  if (shape == CastShape::Trap) return 0;
  return (std::min)(s.range, rules.noTargetGroundAheadTiles);
}

// Teleport aim (section 10 + C8): the cursor point, the stick, the player's own lock (`playerLock`: a click / tap /
// target cycle), else the tapped target, the facing. Never a C14 hover / auto-pick or auto-battle's lock, although they
// hold hero.attackTarget too: an escape blink must not land next to the monster the hero just shot at.
TeleportAim CmbTeleportAim(const SimContext& ctx, const SkillAim& aim, EntityId playerLock) {
  TeleportAim ta;
  ta.hasPoint = aim.hasPoint;
  ta.point = aim.point;
  ta.stickDir = aim.stickDir;
  for (const EntityId lock : {playerLock, aim.target}) {
    if (lock == kNoEntity) continue;
    const MonsterInstance* m = ctx.sys.monsters != nullptr ? ctx.sys.monsters->Find(lock) : nullptr;
    if (m != nullptr && m->IsAlive()) {
      ta.hasTarget = true;
      ta.targetPos = m->pos;
      break;
    }
  }
  ta.facing = ctx.sys.hero->Facing();
  return ta;
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
  CameraShake(ShakeRequest{durationMs, intensity}, ShakeChannel::Hit);
}

bool CombatSystem::CameraShake(const ShakeRequest& s, ShakeChannel channel) {
  const HitFeedbackTable& t = ctx_.data.Combat().hitFeedback;
  const double throttle = channel == ShakeChannel::Skill ? t.skillShakeThrottleMs : t.shakeThrottleMs;
  if (!shake_.Accept(s, ctx_.Now(), channel, throttle)) return false;
  ctx_.events.Emit(EvCameraShake{s.durationMs, s.intensity});
  return true;
}

// Skill-authored shake (combat 11.5, hit_feedback skillShakes): at the anchor moment, or delayMs later (SkillShake
// timer, sim clock; dropped with the other combat timers at zone exit like the web's FxEngine tasks at scene restart).
void CombatSystem::SkillShake(const SkillDef& s, SkillShakeAnchor at) {
  const HitFeedbackTable& t = ctx_.data.Combat().hitFeedback;
  const int32_t idx = t.SkillShakeIndex(s.id);
  if (idx < 0) return;
  const SkillShakeDef& sh = t.skillShakes[static_cast<size_t>(idx)];
  if (sh.at != at) return;
  if (sh.delayMs > 0) {
    ctx_.timers.Schedule(ctx_.Now() + sh.delayMs, TimerOwner::Combat, static_cast<uint16_t>(CombatTimerKind::SkillShake),
                         kHeroEntityId, kNoEntity, idx);
    return;
  }
  CameraShake(ShakeRequest{sh.durationMs, sh.intensity}, ShakeChannel::Skill);
}

// Every hero skill VFX goes out here: the event, then its VFX-anchored authored shake (SkillEffectSystem.play runs the
// effect - and a synchronous e.shake - before the release applies its hits).
void CombatSystem::EmitSkillVfx(const SkillDef& s, const EvSkillVfx& v) {
  EvSkillVfx stamped = v;
  stamped.atMs = ctx_.Now();  // C14: one host frame dispatches several steps' events; presentation times from this
  ctx_.events.Emit(std::move(stamped));
  SkillShake(s, SkillShakeAnchor::Vfx);
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
  if (g.gained > 0) {
    ctx_.events.Emit(EvSpiritChanged{sp.Value(), sp.MaxValue(), sp.IsResonating(), g.gained, true, source});
  }
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
  // K6: a StackCount status (Ashnumb) reports its stacks after the application; every other type the value passed.
  const double shown = ctx_.data.Classes().statusRules.Stacking(type) == StatusStacking::StackCount ? r.value : value;
  if (r.outcome == StatusApplyOutcome::Applied) {
    ctx_.events.Emit(EvStatusApplied{target, type, shown, r.effectiveDurationMs, false});
    ctx_.events.Log(MakeLoc("sys.statusEffect.applied", {CmbStatusNameArg(type)}), LogType::Combat);
  } else if (r.outcome == StatusApplyOutcome::Refreshed) {
    ctx_.events.Emit(EvStatusApplied{target, type, shown, r.effectiveDurationMs, true});
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
  // Audio 3.2: only a monster swing into the dodge-roll i-frames emits COMBAT_DAMAGE{isDodged} (`miss`). A hero hit the
  // monster stat-dodged returns before COMBAT_DAMAGE (combat 4.2 step 3) and the hero's stat dodge emits none: silent.
  if (iframe) {
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
  const Vec2 pos = before->pos;
  const double maxHp = before->maxHp;
  // K5: a hit (not a tick) from within a blocker's front arc is reduced first (a stunned or frozen one cannot block).
  double dealt = r.amount;
  const MonsterBlockDef& block = before->def.block;
  if (block.present && !r.isTick && r.hasFrom && BlockArcContains(pos, before->heading, block.arcDeg, r.from) &&
      !(ctx_.sys.status != nullptr && ctx_.sys.status->IsImmobilized(r.monster))) {
    dealt = BlockedDamage(r.amount, block.damageMul);
    EvBlock blocked;
    blocked.entity = r.monster;
    blocked.source = r.attacker;
    blocked.from = r.from;
    blocked.amount = PhaseScaledDamage(dealt, before->phaseDamageTakenMul);
    // In the units of `amount` (both after the K4 damageTakenMul): what the hit would have dealt unblocked, less it.
    blocked.blocked = PhaseScaledDamage(r.amount, before->phaseDamageTakenMul) - blocked.amount;
    ctx_.events.Emit(blocked);
  }
  // K4: a phase's damageTakenMul scales every hit (hero, pet, tick, environment) before anything announces it.
  const double amount = PhaseScaledDamage(dealt, before->phaseDamageTakenMul);
  // takeDamage's outcome is known up front (MonsterSystem::ApplyDamage: hp = max(0, hp - amount), killed at 0, weight =
  // classifyHit), so the hit's events go out BEFORE ApplyDamage runs the kill pipeline (death anim, exp / gold texts,
  // loot, quest progress): UE gets the hit before the death it causes (combat 4.2 / 13.1 order).
  const double hpAfter = (std::max)(0.0, before->hp - amount);
  const bool killed = hpAfter <= 0;
  const HitFeedbackTable& t = ctx_.data.Combat().hitFeedback;
  const HitWeight w = ClassifyHit(t, amount, maxHp, r.isCrit, killed, r.isTick);

  EvHit hit;
  hit.target = r.monster;
  hit.source = r.attacker;
  hit.targetFaction = Faction::Monster;
  hit.amount = amount;
  hit.weight = w;
  hit.profile = t.Profile(w);
  hit.crit = r.isCrit;
  hit.tick = r.isTick;
  hit.killed = killed;
  hit.element = r.element;
  hit.impactColor = r.impactColor;
  hit.hasFrom = r.hasFrom;
  hit.from = r.from;
  hit.targetHp = hpAfter;
  hit.targetMaxHp = maxHp;
  hit.skillId = r.skillId;
  hit.attackerStopMs = r.source == KillSource::HeroBasic ? t.Profile(w).attackerStopMs : 0;
  hit.impactBurst = r.impactBurst && !r.isTick;
  hit.melee = false;
  hit.numberSlot = r.numberSlot;
  hit.scorch = r.scorch;
  hit.scorchType = r.scorchType;
  ctx_.events.Emit(hit);

  EvFloatingText ft;
  ft.kind = FloatingTextKind::MonsterDamage;
  ft.anchor = r.monster;
  ft.pos = pos;
  ft.value = amount;
  ft.crit = r.isCrit;
  ft.element = r.element;
  ctx_.events.Emit(ft);

  // Audio (FIX combat 19: every hit is a CombatDamage): hit / crit / miss (A6 heavy), plus the A7 hurt vocal.
  const AudioRulesDef& rules = ctx_.data.Audio().rules;
  if (const std::optional<SfxId> cue = SfxForCombatHit(rules, false, r.isCrit, w)) EmitSfx(*cue, pos, r.monster);
  if (!killed && !r.isTick) EmitSfx(rules.monsterHurt, pos, r.monster);

  // Impact (profile shake; an elite killed by a basic attack -> S6 slow motion): a basic attack's playHitImpact runs
  // before onMonsterKilled (4.2 steps 8 / 12), a skill's impactBurst after its kill check (6.4 #7, 6.5 applyHit).
  const auto impact = [&]() {
    if (hit.impactBurst) Shake(t.Profile(w).shakeMs, t.Profile(w).shakeIntensity);
    if (r.source == KillSource::HeroBasic && w == HitWeight::Kill && elite) StartEliteSlowMotion();
  };
  const bool impactFirst = r.source == KillSource::HeroBasic;
  if (impactFirst) impact();

  DamageFlags flags;
  flags.isCrit = r.isCrit;
  flags.isTick = r.isTick;
  flags.hasFrom = r.hasFrom;
  flags.from = r.from;
  flags.attacker = r.attacker;
  flags.source = r.source;
  flags.provokes = r.provokes && !r.isTick;
  flags.element = r.element;
  const HitWeight applied = ms->ApplyDamage(r.monster, amount, flags);
  ABYSS_ASSERT(applied == w, "DamageMonster: ApplyDamage outcome differs from the announced hit");

  if (!impactFirst) impact();
  return applied;
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
  // Audio: the web's COMBAT_DAMAGE cue for a hit on the hero (hit / crit, A6 hit_heavy by weight; DoT ticks `hit`,
  // audio 3.2), then A6's player_hurt layered on top - A6 adds the hurt cue, it does not take the crit cue away.
  const AudioRulesDef& rules = ctx_.data.Audio().rules;
  if (const std::optional<SfxId> cue = SfxForCombatHit(rules, false, crit, weight)) {
    EmitSfx(*cue, hero.Position(), kHeroEntityId);
  }
  EmitSfx(rules.heroDamageTakenCue, hero.Position(), kHeroEntityId);  // A6
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
  const std::string skillId = h.skill != nullptr ? h.skill->id : std::string();
  // C4: a multi-tick ground effect rolls the target's one-shot hit on the first tick that reaches it, then shares it.
  const bool shared = h.ground != nullptr && h.ground->ticks > 1;
  GroundRoll* roll = shared ? FindGroundRoll(h.ground->effect, h.target) : nullptr;
  GroundRoll hit;  // this hit's whole damage and steal (a fresh roll, or the effect's stored one)
  int32_t spiritDamage = 0;  // Spirit 'hit' (classes 12.1): once per rolled hit, from the formula damage
  if (roll == nullptr) {
    const Combatant attacker = HeroCombatant();
    const Combatant defender = MonsterCombatant(*m);
    SkillHitInput in;
    if (h.skill != nullptr) {
      in.skill = h.skill;
      in.level = h.level;
      in.synergyFactor = hero.Skills().SynergyFactor(*h.skill);
    }
    const DamageResult r =
        CalculateDamage(Rules(), attacker, defender, in, h.forceCrit, ctx_.Rand(RngStream::Combat));
    hit.effect = shared ? h.ground->effect : kNoEntity;
    hit.target = h.target;
    if (r.isDodged) {  // C1 / FIX Q9: a dodged hit applies nothing (a ground effect: for its whole duration)
      hit.dodged = true;
      if (shared) groundRolls_.push_back(hit);
      EmitMiss(h.target, Faction::Monster, kHeroEntityId, skillId, false);
      out.dodged = true;
      return out;
    }
    hit.crit = r.isCrit;
    hit.total = SaturatingInt32(r.damage);
    if (h.skill != nullptr && h.skill->hasBonusVsStatus && ctx_.sys.status != nullptr &&
        ctx_.sys.status->Has(h.target, h.skill->bonusVsStatus)) {
      hit.total = SaturatingInt32(std::floor(hit.total * h.skill->bonusVsStatusMul));  // combustion x1.5 vs burning
    }
    hit.lifeStolen = r.lifeStolen;  // from r, not the combustion value (classes 9.7)
    hit.manaStolen = r.manaStolen;
    spiritDamage = SaturatingInt32(r.damage);
    // The first share that lands carries the one-shot impact (a share rounding to 0 passes it on to the next).
    if (shared) hit.impactDone = GroundTickShare(hit.total, h.ground->index, h.ground->ticks) > 0;
    if (shared) groundRolls_.push_back(hit);
    ConsumeCritBonus();
    out.statusDamage = hit.total;
  } else {
    if (roll->dodged) {  // missed on its first tick (MISS shown then)
      out.dodged = true;
      out.rollStatuses = false;
      return out;
    }
    hit = *roll;
    hit.impactDone = false;  // whether THIS share carries the impact (set below)
    out.rollStatuses = false;  // the statuses were rolled with the first tick
  }
  int32_t amount = hit.total;
  int32_t life = hit.lifeStolen;
  int32_t mana = hit.manaStolen;
  if (shared) {
    amount = GroundTickShare(hit.total, h.ground->index, h.ground->ticks);
    life = GroundTickShare(hit.lifeStolen, h.ground->index, h.ground->ticks);
    mana = GroundTickShare(hit.manaStolen, h.ground->index, h.ground->ticks);
  }
  ApplySteal(spiritDamage, hit.crit, life, mana);
  out.crit = hit.crit;
  out.damage = amount;
  if (amount <= 0) return out;  // a ground tick whose share rounds to 0: nothing lands this tick
  // C4 feel (combat 6.5 / 11.1): the web's fire wall / arrow rain was ONE hit per target - one impact burst, hurt
  // vocal, provoke and one AoE batch shake. A target's first landed share is that hit; its later shares land as ticks
  // (tick weight: no recoil, burst, vocal, provoke or shake), except the share that kills, which keeps the kill weight
  // (still without burst or shake) so the death reads as the web's killing blow.
  bool impact = true;
  if (shared) {
    if (roll == nullptr) {
      impact = hit.impactDone;
    } else {
      impact = !roll->impactDone;
      roll->impactDone = true;
    }
  }
  const bool lateShare = !impact;
  const MonsterInstance* victim = ms->Find(h.target);
  const bool lateShareKills = lateShare && victim != nullptr && !(victim->hp - amount > 0);

  MonsterHitRequest req;
  req.monster = h.target;
  req.amount = amount;
  req.isCrit = hit.crit;
  req.hasFrom = true;
  req.from = h.hasFrom ? h.from : hero.Position();
  req.attacker = kHeroEntityId;
  req.source = h.skill != nullptr ? KillSource::HeroSkill : KillSource::HeroBasic;
  req.element = h.skill != nullptr ? h.skill->damageType : DamageType::Physical;
  req.impactColor = h.skill != nullptr ? h.skill->impactColor
                                       : ClassImpactColor(ctx_.data.Combat().hitFeedback, hero.Class());
  req.skillId = skillId;
  req.numberSlot = h.numberSlot;
  req.impactBurst = h.impactBurst && !lateShare;
  req.isTick = lateShare && !lateShareKills;
  if (h.scorch && h.skill != nullptr && h.skill->hasScorch) {  // 6.4 #7 / C6: single-target skill hits only
    req.scorch = true;
    req.scorchType = h.skill->scorch;
  }
  out.weight = DamageMonster(req);
  out.landed = true;
  out.impact = impact;
  const MonsterInstance* after = ms->Find(h.target);
  out.killed = after == nullptr || !after->IsAlive();
  return out;
}

CombatSystem::GroundRoll* CombatSystem::FindGroundRoll(EntityId effect, EntityId target) {
  for (GroundRoll& g : groundRolls_) {
    if (g.effect == effect && g.target == target) return &g;
  }
  return nullptr;
}

void CombatSystem::DropGroundRolls(EntityId effect) {
  if (effect == kNoEntity) {
    groundRolls_.clear();
    return;
  }
  groundRolls_.erase(std::remove_if(groundRolls_.begin(), groundRolls_.end(),
                                    [effect](const GroundRoll& g) { return g.effect == effect; }),
                     groundRolls_.end());
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
    if (o.landed) ctx_.events.Log(MakeLoc("zone.combat.comboTrigger"), LogType::Combat);
  }
  // 11. doubleShot (needs attackRange > 2: never for the 1.5-tile heroes, kept for data parity). The roll is drawn
  // whenever doubleShot gear is worn and the target lives, also for a 1.5-tile hero (the web's default argument).
  if (eq.Get(Stat::DoubleShot) > 0 && alive() &&
      CheckDoubleShot(eq.Get(Stat::DoubleShot), hero.Derived().attackRange, rng)) {
    HeroHitSpec extra;
    extra.target = target;
    extra.numberSlot = HitNumberSlot::DoubleShot;
    const HeroHitOutcome o = HeroHitMonster(extra);
    if (o.landed) ctx_.events.Log(MakeLoc("zone.combat.doubleArrow"), LogType::Combat);
  }
  // 12. the kill pipeline already ran inside ApplyDamage (target cleanup in OnMonsterKilled).
}

// =====================================================================================================================
// monster -> hero
// =====================================================================================================================

// resolveMonsterStrike (combat 5.2) at the contact beat (T1).
void CombatSystem::ResolveMonsterStrike(EntityId monster) {
  bool ability = false;
  for (size_t i = 0; i < strikes_.size(); ++i) {
    if (strikes_[i].monster == monster) {
      ability = strikes_[i].ability;
      strikes_.erase(strikes_.begin() + static_cast<std::ptrdiff_t>(i));
      break;
    }
  }
  if (ability) {
    ResolveAbility(monster);
    return;
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
  // K2: an ability bolt carries its ability (payload = index + 1).
  if (p.spec.payload > 0 && static_cast<size_t>(p.spec.payload) <= m->def.abilities.size()) {
    const MonsterHitMods mods = AbilityHitMods(*m, m->def.abilities[static_cast<size_t>(p.spec.payload - 1)]);
    ApplyMonsterHit(p.spec.source, true, &mods);
    return;
  }
  ApplyMonsterHit(p.spec.source, true);
}

// applyMonsterHit (combat 5.3). K2: the hit is the monster's damageType (an ability's own type and x dmgMul through
// `mods`), and the HP it takes is capped at the monster's hitCapHeroMaxHpPct of the hero's max life (difficulty-scaled).
void CombatSystem::ApplyMonsterHit(EntityId monsterId, bool ranged, const MonsterHitMods* mods) {
  MonsterSystem* ms = ctx_.sys.monsters;
  const MonsterInstance* m = ms->Find(monsterId);
  if (m == nullptr) return;
  Hero& hero = *ctx_.sys.hero;
  const double now = ctx_.Now();
  // The roll comes first (RNG consumed even when the i-frames avoid the hit).
  SkillHitInput hitIn;
  hitIn.basicType = mods != nullptr ? mods->type : m->def.damageType;
  hitIn.basicMultiplier = mods != nullptr ? mods->dmgMul : 1.0;
  Combatant attacker = MonsterCombatant(*m);
  attacker.outgoingMultiplier = MonsterDamageBuffMul(*m, now);  // K5: its damage buff / aura (1 = none)
  const DamageResult r = CalculateDamage(Rules(), attacker, HeroCombatant(), hitIn, false, ctx_.Rand(RngStream::Combat));
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
  // 3. hit (K2 hit cap: no single hit above the monster's share of the hero's max life).
  const double dmg = ApplyMonsterHitCap(r.damage, hero.MaxHp(), m->def.hitCapHeroMaxHpPct, DifficultyDamageMul());
  if (r.manaDamage > 0) hero.SpendMana(r.manaDamage);  // FIX Q16: the mana shield drains mana
  const HitFeedbackTable& t = ctx_.data.Combat().hitFeedback;
  const HitWeight w = ClassifyHit(t, dmg, hero.MaxHp(), r.isCrit, false, false);
  // hp loss with its feedback (recoil weight from this hit, monster hit-stop round(attackerStopMs x 0.6)), then
  // thornsHeal while still alive.
  HurtHero(dmg, r.isCrit, false, r.type, monsterId, !ranged, w, MonsterAttackerStopMs(t, w));
  const double thorns = ctx_.equip.Get(Stat::ThornsHeal);
  if (thorns > 0 && hero.Hp() > 0) hero.Heal(PercentOfMaxHp(hero.MaxHp(), thorns));

  // Monster on-hit statuses (C6: data rules, evaluated in order); a K2 ability hit rolls the ability's status instead.
  m = ms->Find(monsterId);
  if (m == nullptr) return;
  const double monsterDamage = m->def.damage;
  if (mods != nullptr) {
    if (mods->hasStatus) {
      StatusRuleInput sin;
      sin.monsterDamage = monsterDamage;
      const StatusRuleRoll roll = RollStatusRule(mods->status, sin, ctx_.Rand(RngStream::Combat));
      if (roll.applies) ApplyStatus(kHeroEntityId, roll.status, roll.value, roll.durationMs, monsterId);
    }
  } else {
    const std::vector<StatusRule> rules = m->def.onHitStatus;
    for (const StatusRule& rule : rules) {
      StatusRuleInput sin;
      sin.monsterDamage = monsterDamage;
      const StatusRuleRoll roll = RollStatusRule(rule, sin, ctx_.Rand(RngStream::Combat));
      if (roll.applies) ApplyStatus(kHeroEntityId, roll.status, roll.value, roll.durationMs, monsterId);
    }
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
// K2 monster abilities (implementation-plan.md 3 K2; rules: monsters/MonsterAbilities.h)
// =====================================================================================================================

double CombatSystem::DifficultyDamageMul() const {
  return ctx_.data.Combat().difficulty.Def(ctx_.session.difficulty).damageMul;
}

CombatSystem::MonsterHitMods CombatSystem::AbilityHitMods(const MonsterInstance& m, const MonsterAbilityDef& a) {
  MonsterHitMods mods;
  mods.dmgMul = a.dmgMul;
  mods.type = a.hasDamageType ? a.damageType : m.def.damageType;
  if (a.status.present) {
    mods.hasStatus = true;
    mods.status = AbilityStatusRule(a.status);
  }
  return mods;
}

// The ability clock starts on the first swing opportunity of a fight (each ability ready initialCooldownMs later).
void CombatSystem::StartAbilityClock(MonsterInstance& m, double now) {
  const size_t n = m.def.abilities.size();
  if (m.abilityClockStarted && m.abilityReadyAtMs.size() == n) return;
  m.abilityReadyAtMs.assign(n, 0.0);
  for (size_t i = 0; i < n; ++i) m.abilityReadyAtMs[i] = now + m.def.abilities[i].initialCooldownMs;
  m.abilityClockStarted = true;
}

// The pick (Combat.h K2 notes): the first ready ability by priority replaces the swing (`castsOnly`: the K3 / K5 cast
// between swings, raise / summon / buff / call kinds and, 2F review, the ranged specials only).
bool CombatSystem::TryStartAbility(MonsterInstance& m, double now, bool castsOnly) {
  const Hero& hero = *ctx_.sys.hero;
  StartAbilityClock(m, now);
  AbilityPickInput in;
  in.castsOnly = castsOnly;
  in.swingReach = m.def.attackRange * ctx_.data.Monsters().ai.attackExitMul;
  in.nowMs = now;
  in.heroDist = Dist(m.pos, hero.Position());
  in.hpPct = m.maxHp > 0 ? m.hp / m.maxHp * 100.0 : 100.0;
  in.phase = m.phase;
  in.readyAtMs = m.abilityReadyAtMs;
  in.off = m.abilityOff;  // K4: the phase table's switches
  // K3: a raise / summon is usable only with bodies to stand up (planned for the ready ones only).
  std::vector<uint8_t> available;
  std::vector<SummonPlan> plans;
  for (size_t i = 0; i < m.def.abilities.size(); ++i) {
    const MonsterAbilityDef& cand = m.def.abilities[i];
    if (!AbilityKindSummons(cand.kind) || i >= m.abilityReadyAtMs.size()) continue;
    if (now + 1e-6 < m.abilityReadyAtMs[i]) continue;
    if (available.empty()) {
      available.assign(m.def.abilities.size(), 0);
      plans.resize(m.def.abilities.size());
    }
    plans[i] = PlanSummon(m, cand, now);
    available[i] = plans[i].spots.empty() ? 0 : 1;
  }
  in.available = available;
  const int32_t pick = PickMonsterAbility(m.def.abilities, in);
  if (pick < 0) return false;
  const MonsterAbilityDef& a = m.def.abilities[static_cast<size_t>(pick)];
  if (AbilityKindSummons(a.kind)) {  // K3: the plan's corpses are held until contact
    SummonPlan& plan = plans[static_cast<size_t>(pick)];
    MonsterSystem* ms = ctx_.sys.monsters;
    m.abilityCorpses.clear();
    m.abilitySpots.clear();
    for (size_t k = 0; k < plan.spots.size(); ++k) {
      if (a.kind == AbilityKind::Raise) {
        if (ms == nullptr || !ms->HoldCorpse(plan.corpses[k], m.id)) continue;
        m.abilityCorpses.push_back(plan.corpses[k]);
      }
      m.abilitySpots.push_back(plan.spots[k]);
    }
  }
  const TelegraphStyle style = TelegraphStyleOf(m);
  const AssetManifest& manifest = ctx_.data.Assets();
  const AssetEntryDef* art =
      manifest.loaded ? manifest.FindByGameId(MonsterArtGameId(manifest, m.def.spriteKey, m.def.id)) : nullptr;
  const AnimClipDef* clip = art != nullptr ? art->FindClip(a.clip) : nullptr;
  const AbilityTiming timing =
      ComputeAbilityTiming(a, clip, AbilityMinWindupMs(style), ctx_.data.Combat().anim, m.def.animCategory);
  AbilityAimInput aim;
  aim.casterPos = m.pos;
  aim.casterHeading = m.heading;
  aim.anchor = m.homeTile;  // the authored spawn tile (the dais), not the jittered spawn anchor
  aim.heroPos = hero.Position();
  MonsterAbilityCast cast = ResolveAbilityCast(a, aim, now, now + timing.contactMs);
  if (a.dash && ctx_.sys.monsters != nullptr) {  // 2F review: a charge's end, fixed at windup start
    cast.hasDash = true;
    cast.dashTo = ctx_.sys.monsters->DashEnd(m, cast.dir, a.shape.length);
  }
  m.lastAttackMs = now;
  if (!AbilityKindCastsBetweenSwings(a.kind)) m.lastStrikeMs = now;  // K5: a support cast is no strike (the kite gate)
  m.abilityReadyAtMs[static_cast<size_t>(pick)] = now + a.cooldownMs;
  m.pendingAbility = pick;
  m.abilityCast = cast;
  // 2F review: an ability without a clip (a hazard or a call) plays nothing and does not root its caster.
  const bool animated = !a.clip.empty();
  m.abilityRootedUntilMs = animated ? cast.contactMs : 0.0;
  const bool directional =
      a.hasShape && (a.shape.kind == AbilityShapeKind::Cone || a.shape.kind == AbilityShapeKind::Line);
  if (animated) {
    EvPlayAnim anim;  // the plan's EvMonsterAnim {entity, action}: the action, its clip and its beat
    anim.entity = m.id;
    anim.action = AbilityAnimAction(a.clip);
    anim.clip = a.clip;
    anim.startMs = now;
    anim.contactMs = timing.contactMs;
    anim.windupMs = timing.contactMs;
    anim.durationMs = timing.durationMs;
    anim.playRate = timing.playRate;
    anim.hasFaceTarget = true;
    anim.faceTarget = a.aim == AbilityAim::Self && directional ? m.pos + cast.dir : hero.Position();
    ctx_.events.Emit(anim);
  }
  if (AbilityHasGroundShape(a.kind)) {
    EvTelegraph tg;
    tg.entity = m.id;
    tg.abilityId = a.id;
    tg.kind = a.kind;
    tg.shape = cast.shape;
    tg.origin = cast.origin;
    tg.dir = cast.dir;
    tg.startMs = cast.startMs;
    tg.contactMs = cast.contactMs;
    tg.style = style;
    tg.hasDash = cast.hasDash;
    tg.dashTo = cast.dashTo;
    ctx_.events.Emit(std::move(tg));
  } else if (AbilityKindSummons(a.kind)) {  // K3: a breach-violet marker under each corpse / spot
    AbilityShapeDef marker;
    marker.kind = AbilityShapeKind::Circle;
    marker.radius = ctx_.data.Monsters().ai.raiseMarkerRadius;
    for (const Vec2 spot : m.abilitySpots) {
      EvTelegraph tg;
      tg.entity = m.id;
      tg.abilityId = a.id;
      tg.kind = a.kind;
      tg.shape = marker;
      tg.origin = spot;
      tg.dir = cast.dir;
      tg.startMs = cast.startMs;
      tg.contactMs = cast.contactMs;
      tg.style = style;
      ctx_.events.Emit(std::move(tg));
    }
  }
  PendingStrike st;
  st.monster = m.id;
  st.startMs = now;
  st.windupMs = timing.contactMs;
  st.timer = ctx_.timers.Schedule(cast.contactMs, TimerOwner::Combat,
                                  static_cast<uint16_t>(CombatTimerKind::MonsterStrikeContact), m.id);
  st.ability = true;
  strikes_.push_back(st);
  return true;
}

// K3 (Combat.h "K3"): a raise / summon is a timed cast (monsters.md 2.2 Muster, chapter1.md Call the Flock), not a swing:
// between swings, from Chase or Attack, it starts on its own cooldown when its hero range holds and the runtime found
// bodies for it. K5: so does a buff / call (the drummer's drum "2 s every 8 s", monsters.md 2.3); 2F review: and a ranged
// special (a damaging ability whose rangeMax passes the swing reach: Breach-seep, Lunge). Such a monster's ability
// clock starts at its first moment in a fight (its first Chase / Attack step in the swing scan). One action at a time:
// never while a swing or ability is on its way to contact; a pet's taunt holds it like any ability (the clock runs on).
// TickCombat has already skipped an immobilized, casting or eating monster.
bool CombatSystem::TryStartSupportCast(MonsterInstance& m, double now) {
  bool casts = false;
  const double reach = m.def.attackRange * ctx_.data.Monsters().ai.attackExitMul;
  for (const MonsterAbilityDef& a : m.def.abilities) casts = casts || AbilityCastsBetweenSwings(a, reach);
  if (!casts) return false;
  StartAbilityClock(m, now);
  if (HasPendingStrike(m.id)) return false;
  if (ctx_.sys.petCompanion != nullptr && ctx_.sys.petCompanion->TauntHolds(m.id)) return false;
  return TryStartAbility(m, now, /*castsOnly=*/true);
}

// S7 for abilities: a caster stunned or frozen at any moment of its windup aborts the ability at once, visibly
// (EvMonsterAttackCancelled ends the telegraph; the root ends, the cooldown stays spent). Strikes in add order. A basic
// swing keeps the web rule (checked at contact, ResolveMonsterStrike).
void CombatSystem::AbortImmobilizedAbilities() {
  if (ctx_.sys.status == nullptr || strikes_.empty()) return;
  for (size_t i = 0; i < strikes_.size();) {
    const PendingStrike s = strikes_[i];
    if (!s.ability || !ctx_.sys.status->IsImmobilized(s.monster)) {
      ++i;
      continue;
    }
    ctx_.timers.Cancel(s.timer);
    strikes_.erase(strikes_.begin() + static_cast<std::ptrdiff_t>(i));
    if (MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(s.monster) : nullptr) {
      ClearPendingAbility(*m);
    }
    ctx_.events.Emit(EvMonsterAttackCancelled{s.monster, false});
  }
}

// Contact of the pending ability (T1 timer). S7: a caster immobilized since the last TickCombat aborts it visibly here
// (AbortImmobilizedAbilities catches the rest of the windup); a dead caster never gets here (OnMonsterKilled cancelled
// the timer).
void CombatSystem::ResolveAbility(EntityId monster) {
  MonsterSystem* ms = ctx_.sys.monsters;
  MonsterInstance* m = ms != nullptr ? ms->Find(monster) : nullptr;
  if (m == nullptr) return;
  const int32_t idx = m->pendingAbility;
  const std::vector<uint32_t> corpses = m->abilityCorpses;  // K3: a raise's held corpses, a summon's spots
  const std::vector<Vec2> spots = m->abilitySpots;
  ClearPendingAbility(*m);
  if (!m->IsAlive() || idx < 0 || static_cast<size_t>(idx) >= m->def.abilities.size()) return;
  if (ctx_.sys.status != nullptr && ctx_.sys.status->IsImmobilized(monster)) {
    ctx_.events.Emit(EvMonsterAttackCancelled{monster, false});
    return;
  }
  if (!HeroAlive() || ctx_.session.transitioning) return;
  // Copies: a hit may grow the monster list (kill pipeline) and move the instance.
  const MonsterAbilityDef a = m->def.abilities[static_cast<size_t>(idx)];
  const MonsterAbilityCast cast = m->abilityCast;
  const Hero& hero = *ctx_.sys.hero;
  if (AbilityKindSummons(a.kind)) {  // K3: the bodies still there stand up, within the caster's live cap
    const MonsterDef* base =
        ctx_.sys.zone != nullptr && ctx_.sys.zone->HasZone()
            ? ctx_.data.Monsters().FindForZone(ctx_.sys.zone->MapId(), a.summon.monsterId)
            : ctx_.data.Monsters().Find(a.summon.monsterId);
    if (base == nullptr) return;
    const MonsterDef summoned = *base;  // a copy: spawning grows the monster list
    int32_t room = a.summon.capAlive > 0 ? a.summon.capAlive - ms->RaisedAliveOf(monster)
                                         : static_cast<int32_t>(spots.size());
    if (a.kind == AbilityKind::Raise) {
      for (uint32_t corpse : corpses) {
        if (room <= 0) break;
        if (ms->RaiseCorpse(corpse, summoned, monster, a.summon.hpPct, a.id) != kNoEntity) --room;
      }
    } else {
      for (const Vec2 spot : spots) {
        if (room <= 0) break;
        if (ms->SummonAt(summoned, spot, monster, a.summon.hpPct, a.id) != kNoEntity) --room;
      }
    }
    return;
  }
  if (a.kind == AbilityKind::Bolt) {
    ProjectileSpec p;
    p.kind = ProjectileKind::MonsterBolt;
    p.source = monster;
    p.target = kHeroEntityId;
    p.from = m->pos;
    p.to = hero.Position();
    p.travelMs = MonsterBoltTravelMs(ctx_.data.Combat().projectiles, p.from, p.to);
    p.vfxId = "monster_bolt";
    p.color = m->def.projectileColor;
    p.payload = idx + 1;
    if (ctx_.sys.projectiles != nullptr) ctx_.sys.projectiles->Launch(p);
    return;
  }
  if (a.kind == AbilityKind::Hazard) {  // the ground is the attack: its first pulse lands now (TickHazards)
    PlaceHazard(*m, a, cast);
    return;
  }
  if (a.kind == AbilityKind::Buff || a.kind == AbilityKind::Call) {  // K5: the drum, the call (MonsterSystem.h "K5")
    ms->ResolveBuffCast(monster, a);
    return;
  }
  const MonsterHitMods mods = AbilityHitMods(*m, a);
  // 2F review: dmgMul 0 is a status-only strike (the Corpseherd's Keen): no damage roll, the status on a body inside.
  const bool damaging = a.dmgMul > 0;
  if (ShapeContains(cast.shape, cast.origin, cast.dir, hero.Position())) {
    if (damaging) {
      ApplyMonsterHit(monster, true, &mods);
    } else {
      ApplyMonsterStatusOnly(monster, mods);
    }
  }
  if (a.hitsMonsters && damaging) HitMonstersInShape(monster, a, cast);
  BurnCorpsesIfFire(mods.type, cast.shape, cast.origin, cast.dir);  // K3: a fire strike burns the bodies under it
  if (a.hazard.present) {
    if (const MonsterInstance* caster = ms->Find(monster)) PlaceHazard(*caster, a, cast);
  }
  // 2F review: a charge ends where its windup fixed it, after the hit (MonsterAbilityDef::dash).
  if (cast.hasDash) {
    if (MonsterInstance* caster = ms->Find(monster); caster != nullptr && caster->IsAlive()) {
      if (caster->pos != cast.dashTo) {
        caster->heading = cast.dir;
        ms->Teleport(monster, cast.dashTo, TeleportReason::Dash);
      }
    }
  }
}

// 2F review: a status-only strike on the hero (an ability with dmgMul 0): no roll, no damage; the dodge roll's i-frames
// avoid it, else its status rolls (Combat stream) and lands.
void CombatSystem::ApplyMonsterStatusOnly(EntityId monsterId, const MonsterHitMods& mods) {
  const MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(monsterId) : nullptr;
  if (m == nullptr || !HeroAlive()) return;
  const double now = ctx_.Now();
  if (dodge_.IsInvulnerable(now)) {
    if (dodge_.ClaimAvoidanceReward(now)) GainSpirit(SpiritSource::Dodge, false);
    EmitMiss(kHeroEntityId, Faction::Hero, monsterId, std::string(), true);
    return;
  }
  if (!mods.hasStatus) return;
  StatusRuleInput sin;
  sin.monsterDamage = m->def.damage;
  const StatusRuleRoll roll = RollStatusRule(mods.status, sin, ctx_.Rand(RngStream::Combat));
  if (roll.applies) ApplyStatus(kHeroEntityId, roll.status, roll.value, roll.durationMs, monsterId);
}

// K3 (Combat.h "K3"): count, then the live cap, then the window cap; a raise takes its free corpses in query order.
CombatSystem::SummonPlan CombatSystem::PlanSummon(const MonsterInstance& m, const MonsterAbilityDef& a,
                                                  double now) const {
  SummonPlan plan;
  const MonsterSystem* ms = ctx_.sys.monsters;
  if (ms == nullptr || !a.summon.present) return plan;
  const MonsterAiDef& ai = ctx_.data.Monsters().ai;
  int32_t n = a.summon.count;
  if (a.summon.capAlive > 0) n = (std::min)(n, a.summon.capAlive - ms->RaisedAliveOf(m.id));
  if (a.summon.perWindowCap > 0) {
    int32_t recent = 0;
    for (double t : m.summonedAtMs) recent += now - t < a.summon.windowMs ? 1 : 0;
    n = (std::min)(n, a.summon.perWindowCap - recent);
  }
  if (n <= 0) return plan;
  if (a.kind == AbilityKind::Raise) {
    const double radius = a.summon.hasRadius ? a.summon.radius : ai.raiseRadiusDefault;
    std::vector<uint32_t> ids;
    ms->QueryCorpses(m.pos, radius, CorpseFilter::Raisable, ids);
    for (uint32_t id : ids) {
      if (static_cast<int32_t>(plan.corpses.size()) >= n) break;
      const CorpseEntry* c = ms->CorpseRegistry().Find(id);
      if (c == nullptr) continue;
      plan.corpses.push_back(id);
      plan.spots.push_back(c->pos);
    }
    return plan;
  }
  for (int32_t k = 0; k < n; ++k) {
    if (a.summon.at == AbilitySummonAt::Anchors && !a.summon.anchors.empty()) {
      const TilePos anchor = a.summon.anchors[static_cast<size_t>(k) % a.summon.anchors.size()];
      plan.spots.push_back(ms->SummonSpot(anchor.Center(), 0, 0, 1));
    } else {
      const double radius = a.summon.hasRadius ? a.summon.radius : ai.summonRingRadius;
      plan.spots.push_back(ms->SummonSpot(m.pos, radius, k, n));
    }
  }
  return plan;
}

void CombatSystem::ClearPendingAbility(MonsterInstance& m) {
  // Only the ability's own holds go: a Feed (K4) holds its corpse under the same monster until its due time.
  if (ctx_.sys.monsters != nullptr) {
    for (uint32_t corpse : m.abilityCorpses) ctx_.sys.monsters->ReleaseCorpse(corpse, m.id);
  }
  m.pendingAbility = -1;
  m.abilityRootedUntilMs = 0;
  m.abilityCorpses.clear();
  m.abilitySpots.clear();
}

void CombatSystem::BurnCorpsesIfFire(DamageType type, const AbilityShapeDef& shape, Vec2 origin, Vec2 dir) {
  if (type != DamageType::Fire || ctx_.sys.monsters == nullptr) return;
  ctx_.sys.monsters->BurnCorpses(shape, origin, dir);
}

void CombatSystem::HitMonstersInShape(EntityId caster, const MonsterAbilityDef& a, const MonsterAbilityCast& cast) {
  MonsterSystem* ms = ctx_.sys.monsters;
  std::vector<EntityId> ids;
  ms->QueryAlive(cast.origin, ShapeReach(cast.shape) + kAbilityBodyRadiusTiles, ids);
  for (EntityId id : ids) {
    if (id == caster) continue;
    const MonsterInstance* target = ms->Find(id);
    const MonsterInstance* source = ms->Find(caster);
    if (source == nullptr) break;
    if (target == nullptr || !target->IsAlive()) continue;
    if (!ShapeContains(cast.shape, cast.origin, cast.dir, target->pos)) continue;
    SkillHitInput in;
    in.basicType = a.hasDamageType ? a.damageType : source->def.damageType;
    in.basicMultiplier = a.dmgMul * a.monsterDmgMul;
    Combatant attacker = MonsterCombatant(*source);
    attacker.outgoingMultiplier = MonsterDamageBuffMul(*source, ctx_.Now());  // K5
    const DamageResult r =
        CalculateDamage(Rules(), attacker, MonsterCombatant(*target), in, false, ctx_.Rand(RngStream::Combat));
    if (r.isDodged) {
      EmitMiss(id, Faction::Monster, caster, std::string(), false);
      continue;
    }
    MonsterHitRequest req;
    req.monster = id;
    req.amount = r.damage;
    req.isCrit = r.isCrit;
    req.hasFrom = true;
    req.from = cast.origin;
    req.attacker = caster;
    req.source = KillSource::Other;
    req.element = r.type;
    req.provokes = false;
    DamageMonster(req);
  }
}

void CombatSystem::PlaceHazard(const MonsterInstance& m, const MonsterAbilityDef& a, const MonsterAbilityCast& cast) {
  HazardEntry e;
  e.source = m.id;
  e.abilityId = a.id;
  e.shape = a.kind != AbilityKind::Hazard && a.hazard.hasShape ? a.hazard.shape : cast.shape;
  e.origin = cast.origin;
  e.dir = cast.dir;
  e.startMs = ctx_.Now();
  e.tickMs = a.hazard.tickMs;
  e.pulseAtStart = a.kind == AbilityKind::Hazard;
  e.heroTickDamage = HazardTickDamage(m.def.damage, a.hazard.tickDmgMul);
  e.hitsMonsters = a.hitsMonsters;
  e.monsterTickDamage = a.hitsMonsters ? HazardTickDamage(m.def.damage, a.hazard.tickDmgMul * a.monsterDmgMul) : 0.0;
  e.damageType = a.hasDamageType ? a.damageType : m.def.damageType;
  if (a.hazard.status.present) {
    e.hasStatus = true;
    e.status = AbilityStatusRule(a.hazard.status);
  }
  e.hitCapPct = m.def.hitCapHeroMaxHpPct;
  const uint32_t id = hazards_.Add(std::move(e), a.hazard.durationMs);
  const HazardEntry* h = hazards_.Find(id);
  if (h == nullptr) return;
  EvHazard ev;
  ev.id = h->id;
  ev.source = h->source;
  ev.abilityId = h->abilityId;
  ev.shape = h->shape;
  ev.origin = h->origin;
  ev.dir = h->dir;
  ev.damageType = h->damageType;
  ev.startMs = h->startMs;
  ev.untilMs = h->untilMs;
  ev.tickMs = h->tickMs;
  ctx_.events.Emit(std::move(ev));
}

void CombatSystem::TickHazards() {
  if (hazards_.Entries().empty()) return;
  const double now = ctx_.Now();
  std::vector<HazardTick>& due = scratchHazardTicks_;  // only TickHazards uses it (ticks never run TickHazards)
  hazards_.CollectDue(now, due);
  for (size_t i = 0; i < due.size(); ++i) {
    const HazardEntry* found = hazards_.Find(due[i].hazard);
    if (found == nullptr) continue;
    const HazardEntry h = *found;  // a copy: a tick's hit may add hazards
    ResolveHazardTick(h);
  }
  hazards_.DropExpired(now);
}

// One hazard tick: the hero inside takes the fixed tick less its resistance, capped (a DoT-like tick: no roll, no
// defense, no i-frames, but Death Save applies as to a monster hit), then the status roll; with hitsMonsters every
// monster inside takes the monster tick.
void CombatSystem::ResolveHazardTick(const HazardEntry& h) {
  Hero& hero = *ctx_.sys.hero;
  if (HeroAlive() && !ctx_.session.transitioning && ShapeContains(h.shape, h.origin, h.dir, hero.Position())) {
    if (h.heroTickDamage > 0) {
      double amount = h.heroTickDamage;
      if (h.damageType != DamageType::Physical) {
        const double resist = ResistancePercent(ctx_.data.Classes().formulas, &ctx_.equip, h.damageType);
        amount = (std::max)(1.0, std::floor(amount * (1 - resist / 100.0)));
      }
      // The caster's hit cap (K3: and an on-death puddle's own; the smaller binds).
      const double cap = OnDeathHeroCap(hero.MaxHp(), h.extraCapPct, h.hitCapPct, DifficultyDamageMul());
      if (cap > 0) amount = (std::min)(amount, cap);
      HeroHitRequest req;
      req.amount = amount;
      req.isTick = true;
      req.source = h.source;
      req.element = h.damageType;
      req.allowDeathSave = true;  // monster damage, not a status DoT (whose 5.3 QUIRK stays): Death Save holds
      DamageHero(req);
    }
    if (h.hasStatus && HeroAlive()) {
      StatusRuleInput sin;
      const StatusRuleRoll roll = RollStatusRule(h.status, sin, ctx_.Rand(RngStream::Combat));
      if (roll.applies) ApplyStatus(kHeroEntityId, roll.status, roll.value, roll.durationMs, h.source);
    }
  }
  BurnCorpsesIfFire(h.damageType, h.shape, h.origin, h.dir);  // K3: every tick of a fire hazard burns the bodies in it
  if (!h.hitsMonsters || !(h.monsterTickDamage > 0) || ctx_.sys.monsters == nullptr) return;
  MonsterSystem* ms = ctx_.sys.monsters;
  std::vector<EntityId> ids;
  ms->QueryAlive(h.origin, ShapeReach(h.shape) + kAbilityBodyRadiusTiles, ids);
  for (EntityId id : ids) {
    if (id == h.source) continue;
    const MonsterInstance* target = ms->Find(id);
    if (target == nullptr || !target->IsAlive() || !ShapeContains(h.shape, h.origin, h.dir, target->pos)) continue;
    MonsterHitRequest req;
    req.monster = id;
    req.amount = h.monsterTickDamage;
    req.isTick = true;
    req.hasFrom = true;
    req.from = h.origin;
    req.attacker = h.source;
    req.source = KillSource::Other;
    req.element = h.damageType;
    req.provokes = false;
    DamageMonster(req);
  }
}

// =====================================================================================================================
// K3 on-death effects (implementation-plan.md 3 K3; rules: monsters/OnDeath.h)
// =====================================================================================================================

void CombatSystem::SetZoneOnDeath(const MonsterOnDeathDef& d, std::string label) {
  zoneOnDeath_ = d;
  zoneOnDeathLabel_ = d.present ? std::move(label) : std::string();
}

// Kill pipeline K3 step: the def's effect, then its affixes' (in affix order), then the zone's, each fixed at the death.
void CombatSystem::OnMonsterDeathEffects(const MonsterKilledMsg& msg) {
  const MonsterSystem* ms = ctx_.sys.monsters;
  const MonsterInstance* m = ms != nullptr ? ms->Find(msg.monster) : nullptr;
  if (m == nullptr) return;
  const double now = ctx_.Now();
  const double mul = DifficultyDamageMul();
  const TelegraphStyle style = TelegraphStyleOf(*m);
  const auto schedule = [&](const MonsterOnDeathDef& d, std::string label) {
    if (!d.present) return;
    PendingOnDeath e;
    e.seq = nextOnDeathSeq_++;
    e.source = msg.monster;
    e.label = std::move(label);
    e.def = d;
    e.origin = msg.pos;
    e.dir = m->heading;
    e.startMs = now;
    e.dueMs = now + d.delayMs;
    e.damage = OnDeathBaseDamage(d, m->maxHp, mul);
    e.type = d.hasDamageType ? d.damageType : m->def.damageType;
    e.monsterCapPct = m->def.hitCapHeroMaxHpPct;
    e.style = style;
    if (d.kind != OnDeathKind::Spawn && d.delayMs > 0) {  // the fuse
      EvTelegraph tg;
      tg.entity = e.source;
      tg.abilityId = e.label;
      tg.kind = OnDeathTelegraphKind(d.shape);
      tg.shape = d.shape;
      tg.origin = e.origin;
      tg.dir = e.dir;
      tg.startMs = e.startMs;
      tg.contactMs = e.dueMs;
      tg.style = style;
      tg.source = TelegraphSource::OnDeath;
      ctx_.events.Emit(std::move(tg));
    }
    onDeath_.push_back(std::move(e));
  };
  schedule(m->def.onDeath, "onDeath");
  const EliteAffixTable& table = ctx_.data.Combat().eliteAffixes;
  for (const MonsterAffix& a : m->affixes) {
    schedule(table.Def(a.type).onDeath, "affix." + std::string(EnumName(a.type)));
  }
  schedule(zoneOnDeath_, zoneOnDeathLabel_);
}

void CombatSystem::TickOnDeath() {
  if (onDeath_.empty()) return;
  const double now = ctx_.Now();
  std::vector<PendingOnDeath> due;
  for (size_t i = 0; i < onDeath_.size();) {
    if (onDeath_[i].dueMs <= now + 1e-6) {
      due.push_back(std::move(onDeath_[i]));
      onDeath_.erase(onDeath_.begin() + static_cast<std::ptrdiff_t>(i));
    } else {
      ++i;
    }
  }
  std::stable_sort(due.begin(), due.end(), [](const PendingOnDeath& a, const PendingOnDeath& b) {
    if (a.dueMs != b.dueMs) return a.dueMs < b.dueMs;
    return a.seq < b.seq;
  });
  for (const PendingOnDeath& e : due) ResolveOnDeath(e);  // a death they cause schedules for a later step
}

void CombatSystem::ResolveOnDeath(const PendingOnDeath& e) {
  MonsterSystem* ms = ctx_.sys.monsters;
  if (ms == nullptr || ctx_.session.transitioning) return;
  const MonsterOnDeathDef& d = e.def;
  const double now = ctx_.Now();
  if (d.kind == OnDeathKind::Spawn) {  // bodies around the spot: Raised without a raiser
    const MonsterDef* base = ctx_.sys.zone != nullptr && ctx_.sys.zone->HasZone()
                                 ? ctx_.data.Monsters().FindForZone(ctx_.sys.zone->MapId(), d.spawn.monsterId)
                                 : ctx_.data.Monsters().Find(d.spawn.monsterId);
    if (base == nullptr) return;
    const MonsterDef spawned = *base;
    const double radius = ctx_.data.Monsters().ai.summonRingRadius;
    for (int32_t k = 0; k < d.spawn.count; ++k) {
      ms->SummonAt(spawned, ms->SummonSpot(e.origin, radius, k, d.spawn.count), kNoEntity, 100, e.label);
    }
    return;
  }
  if (d.kind == OnDeathKind::Puddle) {  // a ground hazard; its first pulse lands at once (TickHazards)
    HazardEntry h;
    h.source = e.source;
    h.abilityId = e.label;
    h.shape = d.shape;
    h.origin = e.origin;
    h.dir = e.dir;
    h.startMs = now;
    h.tickMs = d.puddle.tickMs;
    h.pulseAtStart = true;
    h.heroTickDamage = e.damage;
    h.hitsMonsters = d.hitsMonsters;
    h.monsterTickDamage = d.hitsMonsters ? OnDeathMonsterDamage(e.damage, d.monsterDmgMul) : 0.0;
    h.damageType = e.type;
    if (d.status.present) {
      h.hasStatus = true;
      h.status = AbilityStatusRule(d.status);
    }
    h.hitCapPct = e.monsterCapPct;
    h.extraCapPct = d.capHeroMaxHpPct;
    const uint32_t id = hazards_.Add(std::move(h), d.puddle.durationMs);
    if (const HazardEntry* p = hazards_.Find(id)) {
      EvHazard ev;
      ev.id = p->id;
      ev.source = p->source;
      ev.abilityId = p->abilityId;
      ev.shape = p->shape;
      ev.origin = p->origin;
      ev.dir = p->dir;
      ev.damageType = p->damageType;
      ev.startMs = p->startMs;
      ev.untilMs = p->untilMs;
      ev.tickMs = p->tickMs;
      ctx_.events.Emit(std::move(ev));
    }
    return;
  }
  // Burst / ring: the hero whose body touches the shape, then the monsters in it, then the bodies a fire one burns.
  Hero& hero = *ctx_.sys.hero;
  if (HeroAlive() && ShapeContains(d.shape, e.origin, e.dir, hero.Position())) {
    if (dodge_.IsInvulnerable(now)) {  // a dodge roll through the blast
      if (dodge_.ClaimAvoidanceReward(now)) GainSpirit(SpiritSource::Dodge, false);
      EmitMiss(kHeroEntityId, Faction::Hero, e.source, std::string(), true);
    } else {
      if (e.damage > 0) {
        double amount = e.damage;
        if (e.type != DamageType::Physical) {
          const double resist = ResistancePercent(ctx_.data.Classes().formulas, &ctx_.equip, e.type);
          amount = (std::max)(1.0, std::floor(amount * (1 - resist / 100.0)));
        }
        const double cap = OnDeathHeroCap(hero.MaxHp(), d.capHeroMaxHpPct, e.monsterCapPct, DifficultyDamageMul());
        if (cap > 0) amount = (std::min)(amount, cap);
        // 2F review: the bursts and rings of the last second share one cap (OnDeath.h kOnDeathWindowCapPct).
        const double windowCap = MonsterHitCap(hero.MaxHp(), kOnDeathWindowCapPct, DifficultyDamageMul());
        std::erase_if(onDeathTaken_, [now](const std::pair<double, double>& t) { return t.first <= now - kOnDeathWindowMs; });
        amount = (std::min)(amount, OnDeathWindowRoom(windowCap, now, onDeathTaken_));
        if (amount > 0) {
          onDeathTaken_.emplace_back(now, amount);
          HeroHitRequest req;
          req.amount = amount;
          req.source = e.source;
          req.element = e.type;
          req.allowDeathSave = true;  // monster damage
          DamageHero(req);
        }
      }
      if (d.status.present && HeroAlive()) {
        StatusRuleInput sin;
        const StatusRuleRoll roll = RollStatusRule(AbilityStatusRule(d.status), sin, ctx_.Rand(RngStream::Combat));
        if (roll.applies) ApplyStatus(kHeroEntityId, roll.status, roll.value, roll.durationMs, e.source);
      }
    }
  }
  const double monsterDamage = d.hitsMonsters ? OnDeathMonsterDamage(e.damage, d.monsterDmgMul) : 0.0;
  if (monsterDamage > 0) {
    std::vector<EntityId> ids;
    ms->QueryAlive(e.origin, ShapeReach(d.shape) + kAbilityBodyRadiusTiles, ids);
    for (EntityId id : ids) {
      const MonsterInstance* target = ms->Find(id);
      if (target == nullptr || !target->IsAlive() || !ShapeContains(d.shape, e.origin, e.dir, target->pos)) continue;
      MonsterHitRequest req;
      req.monster = id;
      req.amount = monsterDamage;
      req.hasFrom = true;
      req.from = e.origin;
      req.attacker = e.source;
      req.source = KillSource::Environment;
      req.element = e.type;
      req.provokes = false;
      DamageMonster(req);
    }
  }
  BurnCorpsesIfFire(e.type, d.shape, e.origin, e.dir);
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
  if (ctx_.sys.soulEcho != nullptr) ctx_.sys.soulEcho->OnHeroDied(pos, InDungeonZone());
  ctx_.events.Emit(EvCameraFlash{0xffffff, 80, 0.6});
  ctx_.events.Emit(EvBanner{BannerKind::Death, MakeLoc("zone.death.text"), LocText{}});
  ctx_.bus.Publish(HeroDiedMsg{pos});
  ctx_.timers.Cancel(respawnTimer_);
  respawnTimer_ = ctx_.timers.Schedule(ctx_.Now() + ctx_.data.World().constants.deathRespawnMs, TimerOwner::Combat,
                                       static_cast<uint16_t>(CombatTimerKind::HeroRespawn));
  (void)cause;
}

// respawnAtCamp (world 9.5): full HP / MP after equipment (C1), camp 0, path and target cleared. The T12 respawn in a
// sub-dungeon or a labyrinth floor (`parentZoneRestart`) restarts the parent zone instead (13.3 step 3).
void CombatSystem::Respawn(bool parentZoneRestart) {
  Hero& hero = *ctx_.sys.hero;
  respawnTimer_ = kNoTimer;
  hero.SetLife(HeroLife::Alive);
  hero.RecalcDerived(ctx_.equip);
  hero.FillHpMana();
  std::string parentMap;
  Vec2 parentCamp;
  ZoneRuntime* zone = ctx_.sys.zone;
  if (parentZoneRestart && zone != nullptr && zone->HasZone() && !ctx_.session.transitioning &&
      DungeonRespawnTarget(parentMap, parentCamp)) {
    // scene.restart into the parent zone at its camps[0] with full HP / MP (world 9.5): the hero waits where it fell
    // until the zone change places it; the parent zone's entry autosave stands in for the web's create() autosave.
    // (A labyrinth run would end 'fallen' here; the core has no labyrinth run yet - later milestone.)
    if (ctx_.sys.locomotion != nullptr) ctx_.sys.locomotion->Stop();
    SetTargetInternal(kNoEntity, false);
    hero.lastAttackMs = 0;
    ctx_.events.Log(MakeLoc("zone.death.logMessage"), LogType::System);
    zone->RequestZoneChange(parentMap, parentCamp);
    ctx_.events.Emit(EvHeroRespawned{hero.Position()});
    ctx_.bus.Publish(HeroRespawnedMsg{hero.Position()});
    return;
  }
  Vec2 camp = hero.Position();
  if (ctx_.sys.zone != nullptr && ctx_.sys.zone->HasZone()) camp = ctx_.sys.zone->CampPosition(0);
  if (ctx_.sys.locomotion != nullptr) {
    ctx_.sys.locomotion->Stop();
    ctx_.sys.locomotion->Teleport(camp, TeleportReason::Respawn);
  } else {
    hero.SetPosition(camp);
  }
  SetTargetInternal(kNoEntity, false);
  hero.lastAttackMs = 0;
  ctx_.events.Log(MakeLoc("sys.player.respawn"), LogType::System);
  ctx_.events.Emit(EvHeroRespawned{camp});
  ctx_.bus.Publish(HeroRespawnedMsg{camp});
}

// combat 15: a sub-dungeon (its zone id is a world SubDungeonDef id) or a labyrinth floor is rebuilt on every visit, so
// a death there leaves no echo.
bool CombatSystem::InDungeonZone() const {
  const std::string& zoneId = ctx_.session.currentMap;
  if (zoneId.empty()) return false;
  if (ctx_.data.World().FindSubDungeon(zoneId) != nullptr) return true;
  return zoneId.rfind(kCombatLabyrinthFloorPrefix, 0) == 0;
}

// world 9.5 / combat 13.3 step 3: the parent of a sub-dungeon (SubDungeonDef::parentZone) or of a labyrinth floor (the
// rift), and its camps[0] (fallback playerStart, then (3, 3); the web's `parentCamp ?? playerStart ?? 3`).
bool CombatSystem::DungeonRespawnTarget(std::string& mapId, Vec2& pos) const {
  const std::string& zoneId = ctx_.session.currentMap;
  if (zoneId.empty()) return false;
  if (const SubDungeonDef* sd = ctx_.data.World().FindSubDungeon(zoneId)) {
    mapId = sd->parentZone;
  } else if (zoneId.rfind(kCombatLabyrinthFloorPrefix, 0) == 0) {
    mapId = std::string(kCombatLabyrinthParentZone);
  } else {
    return false;
  }
  pos = Vec2(3, 3);
  if (const MapDef* parent = ctx_.data.FindMap(mapId)) {
    const TilePos t = !parent->camps.empty() ? parent->camps[0].pos : parent->playerStart;
    pos = Vec2(t.col, t.row);
  }
  return true;
}

// The menu / quit / background / zone-exit path respawns in the current zone (the save that follows maps the position,
// SimSave rule 1); only the T12 timer restarts a dungeon's parent zone.
void CombatSystem::ResolvePendingDeath() {
  if (ctx_.sys.hero->Life() != HeroLife::Dying) return;
  ctx_.timers.Cancel(respawnTimer_);
  Respawn(false);
}

// =====================================================================================================================
// kill pipeline (step 1)
// =====================================================================================================================

void CombatSystem::OnMonsterKilled(const MonsterKilledMsg& m) {
  Hero& hero = *ctx_.sys.hero;
  if (ctx_.sys.status != nullptr) ctx_.sys.status->ClearEntity(m.monster);
  // K3: a Raised body pays nothing (no Spirit 'kill', exp, gold - no draw -, kill heal or texts); the rest of the kill
  // step applies (statuses, target cleanup, its pending swing).
  const bool rewards = m.role != MonsterRole::Raised;
  if (rewards) GainSpirit(SpiritSource::Kill, false);
  // exp = floor(expReward * (1 + homeBonus.expBonus / 100 + eq.expBonus / 100)), homeBonus = homestead + pet bonuses.
  double homeExp = 0;
  if (ctx_.sys.homestead != nullptr) homeExp += ctx_.sys.homestead->TotalBonuses().Get(Stat::ExpBonus);
  if (ctx_.sys.pets != nullptr) homeExp += ctx_.sys.pets->Bonuses().Get(Stat::ExpBonus);
  const double expMul = 1 + homeExp / 100.0 + ctx_.equip.Get(Stat::ExpBonus) / 100.0;
  // Saturating to [0, 2^53] (SaveQuests / PetSystem rule): equip expBonus is summed from save-supplied affix values, so
  // a hand-edited item must not reach a raw out-of-range double -> int64 cast (UB, platform-dependent).
  const double expRaw = rewards ? std::floor(m.expReward * expMul) : 0.0;
  const int64_t exp = expRaw > 0 ? static_cast<int64_t>((std::min)(expRaw, 9007199254740992.0)) : 0;
  const int32_t gMin = SaturatingInt32(m.goldMin);
  const int32_t gMax = (std::max)(gMin, SaturatingInt32(m.goldMax));
  const int64_t gold = rewards ? ctx_.Rand(RngStream::Loot).RandomInt(gMin, gMax) : 0;
  if (ctx_.sys.rewards != nullptr && rewards) {
    ctx_.sys.rewards->GrantExp(exp, ExpSource::Kill);
    if (gold > 0) ctx_.sys.rewards->ChangeGold(gold, GoldReason::Kill);
  }
  lastKill_ = KillRewardInfo{m.monster, exp, gold};
  // killHealPercent (C11; K3: not for a Raised body).
  const double killHeal = rewards ? ctx_.equip.Get(Stat::KillHealPercent) : 0.0;
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
  if (rewards) {
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
  }
  // Target cleanup (the web cleared it on basic-attack kills; every kill path does here).
  if (hero.attackTarget == m.monster) SetTargetInternal(kNoEntity, false);
  if (indicator_ == m.monster) UpdateIndicator();
  // The dead monster's pending swing can never land. A K2 ability aborts visibly (S7): its telegraph goes.
  for (size_t i = 0; i < strikes_.size(); ++i) {
    if (strikes_[i].monster == m.monster) {
      const bool ability = strikes_[i].ability;
      ctx_.timers.Cancel(strikes_[i].timer);
      strikes_.erase(strikes_.begin() + static_cast<std::ptrdiff_t>(i));
      if (ability) {
        if (MonsterInstance* dead = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(m.monster) : nullptr) {
          ClearPendingAbility(*dead);
        }
        ctx_.events.Emit(EvMonsterAttackCancelled{m.monster, false});
      }
      break;
    }
  }
}

// K3: a collapse is not a kill (no credit), but its body leaves the fight like a dead one.
void CombatSystem::OnMonsterCollapsed(EntityId monster) {
  Hero& hero = *ctx_.sys.hero;
  if (ctx_.sys.status != nullptr) ctx_.sys.status->ClearEntity(monster);
  if (hero.attackTarget == monster) SetTargetInternal(kNoEntity, false);
  if (indicator_ == monster) UpdateIndicator();
  for (size_t i = 0; i < strikes_.size(); ++i) {
    if (strikes_[i].monster != monster) continue;
    const bool ability = strikes_[i].ability;
    ctx_.timers.Cancel(strikes_[i].timer);
    strikes_.erase(strikes_.begin() + static_cast<std::ptrdiff_t>(i));
    if (ability) {  // a basic swing just never lands (as for a kill); an ability's telegraph ends visibly
      if (MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(monster) : nullptr) {
        ClearPendingAbility(*m);
      }
      ctx_.events.Emit(EvMonsterAttackCancelled{monster, false});
    }
    break;
  }
}

// K4: a phase's enter action interrupts what the monster was doing: its swing or ability on its way to contact goes,
// visibly (EvMonsterAttackCancelled: UE drops the wind-up tint and the telegraph); the cooldown stays spent. So does a
// swing the beast drew (its taunt or a stray swing: a pet timer, not one of strikes_).
bool CombatSystem::CancelMonsterStrike(EntityId monster) {
  bool cancelled = false;
  for (size_t i = 0; i < strikes_.size(); ++i) {
    if (strikes_[i].monster != monster) continue;
    const bool ability = strikes_[i].ability;
    ctx_.timers.Cancel(strikes_[i].timer);
    strikes_.erase(strikes_.begin() + static_cast<std::ptrdiff_t>(i));
    if (ability) {
      if (MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(monster) : nullptr) {
        ClearPendingAbility(*m);
      }
    }
    ctx_.events.Emit(EvMonsterAttackCancelled{monster, false});
    cancelled = true;
    break;
  }
  if (ctx_.sys.petCompanion != nullptr && ctx_.sys.petCompanion->CancelMonsterSwing(monster)) cancelled = true;
  return cancelled;
}

// K4: one arena tick on the hero standing outside the ring (MonsterSystem::TickPhases): the tick less the hero's
// resistance, capped by the monster's hit cap; like a hazard tick, no roll, defense or i-frames, and Death Save holds
// (monster damage, not a status DoT).
void CombatSystem::ArenaTickHero(EntityId source, double dpsPctMaxHp, DamageType type, double tickMs, double hitCapPct) {
  if (!HeroAlive() || ctx_.session.transitioning) return;
  const Hero& hero = *ctx_.sys.hero;
  double amount = ArenaTickDamage(hero.MaxHp(), dpsPctMaxHp, tickMs, DifficultyDamageMul());
  if (type != DamageType::Physical) {
    const double resist = ResistancePercent(ctx_.data.Classes().formulas, &ctx_.equip, type);
    amount = (std::max)(1.0, std::floor(amount * (1 - resist / 100.0)));
  }
  const double cap = MonsterHitCap(hero.MaxHp(), hitCapPct, DifficultyDamageMul());
  if (cap > 0) amount = (std::min)(amount, cap);
  HeroHitRequest req;
  req.amount = amount;
  req.isTick = true;
  req.source = source;
  req.element = type;
  req.allowDeathSave = true;
  DamageHero(req);
}

// =====================================================================================================================
// targeting
// =====================================================================================================================

void CombatSystem::SetTargetInternal(EntityId target, bool playerChosen) {
  Hero& hero = *ctx_.sys.hero;
  playerLock_ = playerChosen ? target : kNoEntity;
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
  SetTargetInternal(monster, true);  // a click / tap: the player's own lock (C8 teleport may blink to it)
  if (!approach || ctx_.sys.locomotion == nullptr) return;
  // 9.1: a monster click always replaces the walk order (the web paths to the monster's tile); C7 only makes that walk
  // stop at attack range. Approach ends at once - path cleared, hero stopped - when the target is already in range, so a
  // click-move path or the approach toward a previous lock never keeps running.
  ctx_.sys.locomotion->Approach(monster, ctx_.sys.hero->Derived().attackRange);
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
  playerLock_ = next;  // a deliberate selection, like a click
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

EntityId CombatSystem::PlayerLock() const {
  const EntityId lock = ctx_.sys.hero->attackTarget;
  return lock != kNoEntity && lock == playerLock_ ? lock : kNoEntity;
}

// C14 rule (c), shared by the press, the target ring (9.5) and auto-battle (9.4): the nearest alive monster within
// `radius` (MonsterSystem spatial-grid order keeps ties deterministic).
EntityId CombatSystem::AutoPickTarget(double radius) const {
  const MonsterSystem* ms = ctx_.sys.monsters;
  return ms != nullptr ? ms->NearestAlive(ctx_.sys.hero->Position(), radius) : kNoEntity;
}

// C14 cast target rule (classes 9.2): (a) tap / lock, (b) hover, (c) the nearest within the radius (the skill's cast
// reach at the press).
EntityId CombatSystem::ResolveCastTarget(const SkillDef& s, const SkillAim& aim, bool useHover, double radius,
                                         AcquireCache* cache) const {
  const MonsterSystem* ms = ctx_.sys.monsters;
  if (ms == nullptr || SkillCastShape(s) == CastShape::Blink) return kNoEntity;
  const auto aliveMonster = [ms](EntityId id) -> const MonsterInstance* {
    if (id == kNoEntity) return nullptr;
    const MonsterInstance* m = ms->Find(id);
    return m != nullptr && m->IsAlive() ? m : nullptr;
  };
  const Hero& hero = *ctx_.sys.hero;
  if (aliveMonster(aim.target) != nullptr) return aim.target;                   // (a) a touch tap
  if (aliveMonster(hero.attackTarget) != nullptr) return hero.attackTarget;     // (a) the lock
  if (useHover && aliveMonster(aim.hoverTarget) != nullptr) return aim.hoverTarget;  // (b) under the cursor
  if (cache != nullptr && cache->valid && cache->radius == radius) return cache->id;
  const EntityId pick = AutoPickTarget(radius);  // (c); with no lock the ring previews this same pick (9.5)
  if (cache != nullptr) {
    cache->valid = true;
    cache->radius = radius;
    cache->id = pick;
  }
  return pick;
}

void CombatSystem::SelectCastTarget(const SkillDef& s, EntityId target) {
  if (target == kNoEntity || target == ctx_.sys.hero->attackTarget || !CastShapeSelects(SkillCastShape(s))) return;
  SetTargetInternal(target, false);  // a hover / auto-pick: selected, but not the player's own (C8) lock
}

EntityId CombatSystem::PreviewCastTarget(int32_t skillIndex) const {
  const SkillBook& book = ctx_.sys.hero->Skills();
  if (skillIndex < 0 || static_cast<size_t>(skillIndex) >= book.SkillCount()) return kNoEntity;
  const SkillDef& s = book.Skill(skillIndex);
  return ResolveCastTarget(s, SkillAim{}, false, SkillCastReach(ctx_.data.Classes().skillRules, s));
}

EntityId CombatSystem::IndicatorTarget() const { return indicator_; }

// updateTargetIndicator (9.5 with C14): the lock (a dead lock is cleared), else the C14 (c) auto-pick a press would
// take - the nearest alive monster within castAcquireMinTiles (the cast reach of every shipped skill) - so the ring and
// the cast never disagree; EvTargetChanged whenever the shown target changes.
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
  if (shown == kNoEntity && hero.attackTarget == kNoEntity) {
    shown = AutoPickTarget(ctx_.data.Classes().skillRules.castAcquireMinTiles);
  }
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
  const SkillDef& s = book.Skill(skillIndex);
  if (s.passive) return SkillRequestResult::Passive;  // C1 / FIX Q4
  // An explicit target (touch tap) becomes the player's lock, whatever the skill (C8 teleport reads it as its lock) -
  // also when it already was the lock as a C14 pick.
  if (aim.target != kNoEntity && aim.target != PlayerLock()) {
    const MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(aim.target) : nullptr;
    if (m != nullptr && m->IsAlive()) SetTargetInternal(aim.target, true);
  }
  // C14: resolve once, at the press; the pick is highlighted (selected) at once, also when the press is buffered for
  // cooldown / mana / a stun - but never a pick the cast cannot reach (the lock would then pin every later press and the
  // basic attack to it).
  const EntityId target = ResolveCastTarget(s, aim, true, SkillCastReach(ctx_.data.Classes().skillRules, s));
  const CastGate gate = EvaluateCast(skillIndex, target, false);
  if (gate != CastGate::OutOfRange) SelectCastTarget(s, target);
  const double now = ctx_.Now();
  if (buffer_.Request(skillIndex, now, gate == CastGate::Ok) == InputBuffer::RequestResult::ExecuteNow) {
    TryUseSkill(skillIndex, aim, target);
    return SkillRequestResult::Executed;
  }
  bufferedAim_ = aim;
  bufferedTarget_ = target;
  // Never a silent failure for the target rule: the press says why (cooldown / mana show on the hotbar).
  if (gate == CastGate::NoTarget) ctx_.events.Log(MakeLoc("zone.combat.noTarget"), LogType::Combat);
  if (gate == CastGate::OutOfRange) ctx_.events.Log(MakeLoc("zone.combat.outOfRange"), LogType::Combat);
  ctx_.events.Emit(EvSkillBuffered{s.id, buffer_.ExpiresAtMs()});
  return SkillRequestResult::Buffered;
}

// The shared gate of canExecuteSkill / tryUseSkill (classes 9.2-9.3 with C1: passives false; C5: an immobilized hero
// cannot cast; C14: the target rule per cast shape - FIX Q8 death_mark / shadow_step need a target).
CastGate CombatSystem::EvaluateCast(int32_t skillIndex, EntityId target, bool logFailures) const {
  const Hero& hero = *ctx_.sys.hero;
  if (!HeroAlive()) return CastGate::Dead;
  const SkillBook& book = hero.Skills();
  if (skillIndex < 0 || static_cast<size_t>(skillIndex) >= book.SkillCount()) return CastGate::Locked;
  const SkillDef& s = book.Skill(skillIndex);
  if (book.Level(skillIndex) <= 0) {
    if (logFailures) ctx_.events.Log(MakeLoc("zone.combat.skillLocked"), LogType::Combat);
    return CastGate::Locked;
  }
  if (s.passive) return CastGate::Passive;
  if (!book.IsReady(skillIndex, ctx_.Now())) return CastGate::Cooldown;
  if (hero.Mana() < CastManaCost(skillIndex)) {
    if (logFailures) ctx_.events.Log(MakeLoc("zone.combat.manaInsufficient"), LogType::Combat);
    return CastGate::Mana;
  }
  if (HeroImmobilized()) {
    if (logFailures && s.execKind == SkillExecKind::Teleport) {
      ctx_.events.Log(MakeLoc("zone.teleport.blockedByCC"), LogType::Combat);
    }
    return CastGate::Immobilized;
  }
  const CastShape shape = SkillCastShape(s);
  // Buffs, teleport and hero-centred novas are never aimed at a target.
  if (shape == CastShape::Self || shape == CastShape::Blink || shape == CastShape::Nova) return CastGate::Ok;
  const MonsterInstance* m =
      target != kNoEntity && ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(target) : nullptr;
  if (m == nullptr || !m->IsAlive()) {
    // Projectile / Chain / Ground / Trap / Cone / Line without a target fire along the aim (rule 3); the others need one.
    return CastShapeNeedsTarget(shape) ? CastGate::NoTarget : CastGate::Ok;
  }
  // Every shape aimed at a target needs it within its release reach: a ground AoE beyond it would land short of it
  // (clamped) and a cone / line would miss it, so they refuse like a projectile instead of spending the cost.
  const double reach = SkillReleaseReach(ctx_.data.Classes().skillRules, s);
  return DistSq(hero.Position(), m->pos) <= reach * reach ? CastGate::Ok : CastGate::OutOfRange;
}

bool CombatSystem::CanExecuteSkill(int32_t skillIndex) const {
  return EvaluateCast(skillIndex, PreviewCastTarget(skillIndex), false) == CastGate::Ok;
}

EntityId CombatSystem::BufferedCastTarget(int32_t skillIndex) const {
  if (bufferedTarget_ != kNoEntity && ctx_.sys.monsters != nullptr) {
    const MonsterInstance* m = ctx_.sys.monsters->Find(bufferedTarget_);
    if (m != nullptr && m->IsAlive()) return bufferedTarget_;  // the press-time target, kept
  }
  const SkillBook& book = ctx_.sys.hero->Skills();
  if (skillIndex < 0 || static_cast<size_t>(skillIndex) >= book.SkillCount()) return kNoEntity;
  const SkillDef& s = book.Skill(skillIndex);
  // It died (or there was none): resolve again from now, without the stale hover.
  return ResolveCastTarget(s, bufferedAim_, false, SkillCastReach(ctx_.data.Classes().skillRules, s));
}

void CombatSystem::ConsumeBufferedSkill() {
  if (!buffer_.HasPending()) return;
  const EntityId target = BufferedCastTarget(buffer_.PendingSkill());
  const int32_t idx = buffer_.ConsumeReady(
      ctx_.Now(), [this, target](int32_t i) { return EvaluateCast(i, target, false) == CastGate::Ok; });
  if (idx < 0) return;
  SelectCastTarget(ctx_.sys.hero->Skills().Skill(idx), target);
  TryUseSkill(idx, bufferedAim_, target);
}

CastAim CombatSystem::MakeCastAim(const CastTarget& target, const SkillAim& aim) const {
  CastAim a;
  a.hasTarget = target.hasPos;
  a.targetPos = target.pos;
  a.hasPoint = aim.hasPoint;
  a.point = aim.point;
  a.facing = ctx_.sys.hero->Facing();
  return a;
}

// tryUseSkill (classes 9.3): commit (cooldown, mana, SKILL_USED), freeCast, animation, release now or on the beat.
void CombatSystem::TryUseSkill(int32_t skillIndex, const SkillAim& aim, EntityId target) {
  if (EvaluateCast(skillIndex, target, true) != CastGate::Ok) return;
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
  const std::string art = HeroArtGameId(manifest, hero.Class());  // the asset UE plays (player_<class>)
  const AnimRig rig = HeroRig(hero.Class());
  const MonsterInstance* tm = target != kNoEntity ? ctx_.sys.monsters->Find(target) : nullptr;
  if (tm != nullptr && !tm->IsAlive()) tm = nullptr;
  CastTarget ct;
  ct.id = tm != nullptr ? target : kNoEntity;
  ct.hasPos = tm != nullptr;
  ct.pos = tm != nullptr ? tm->pos : Vec2();
  ActionTiming timing;
  AnimAction action = AnimAction::Cast;
  if (s.animKind == SkillAnimKind::Attack && tm != nullptr) {
    timing = ComputeAttackTiming(at, manifest, art, rig, hero.Derived().attackSpeedMs, "Attack01");
    action = AnimAction::Attack;
  } else {
    timing = ComputeCastTiming(at, manifest, art, rig, "Cast01");
  }
  // C14 rule 4: the hero turns toward what the cast is released at before the animation - the target, else the
  // no-target aim (a ground cast's point, the cursor / facing direction), a teleport's aimed destination. Buffs never.
  const CastShape shape = SkillCastShape(s);
  const SkillRules& rules = ctx_.data.Classes().skillRules;
  const Vec2 heroPos = hero.Position();
  bool face = false;
  Vec2 facePoint;
  if (shape == CastShape::Blink) {
    face = true;
    facePoint = TeleportAimPoint(s.port, heroPos, CmbTeleportAim(ctx_, aim, PlayerLock()));
  } else if (shape != CastShape::Self) {
    face = true;
    const CastAim ca = MakeCastAim(ct, aim);
    if (tm != nullptr) {
      facePoint = tm->pos;
    } else if (shape == CastShape::Ground || shape == CastShape::Trap) {
      facePoint = GroundCastPoint(heroPos, ca, SkillCastReach(rules, s), CmbNoTargetAhead(rules, s, shape));
    } else {
      facePoint = heroPos + CastDirection(heroPos, ca);
    }
  }
  EvPlayAnim anim;
  anim.entity = kHeroEntityId;
  anim.action = action;
  anim.clip = s.id;
  anim.startMs = now;
  anim.contactMs = timing.contactMs;
  anim.durationMs = timing.durationMs;
  anim.playRate = timing.playRate;
  if (face && DistSq(facePoint, heroPos) > kCombatMinAimTilesSq) {
    anim.hasFaceTarget = true;
    anim.faceTarget = facePoint;
    hero.SetFacing(CmbDirOr(facePoint - heroPos, hero.Facing()));
  }
  ctx_.events.Emit(anim);

  if (s.instantRelease || !(timing.contactMs > 0)) {
    ReleaseSkill(skillIndex, level, ct, aim, cost);
    return;
  }
  PendingRelease pr;
  pr.skillIndex = skillIndex;
  pr.level = level;
  pr.target = ct;
  pr.aim = aim;
  pr.manaCost = cost;
  const int32_t slot = AllocRelease(pr);
  ctx_.timers.Schedule(now + timing.contactMs, TimerOwner::Combat, static_cast<uint16_t>(CombatTimerKind::SkillRelease),
                       kHeroEntityId, ct.id, slot);
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

void CombatSystem::ReleaseSkill(int32_t skillIndex, int32_t level, const CastTarget& target, const SkillAim& aim,
                                int32_t manaCost) {
  const SkillDef& s = ctx_.sys.hero->Skills().Skill(skillIndex);
  const MonsterInstance* tm = target.id != kNoEntity ? ctx_.sys.monsters->Find(target.id) : nullptr;
  const EntityId alive = tm != nullptr && tm->IsAlive() ? target.id : kNoEntity;
  switch (s.execKind) {
    case SkillExecKind::Teleport:
      ReleaseTeleport(s, level, aim, manaCost);
      return;
    case SkillExecKind::ShadowStep:
      if (alive != kNoEntity) ReleaseShadowStep(s, level, alive);  // FIX Q8: no buff fallback without a target
      return;
    case SkillExecKind::DeathMark:
      if (alive != kNoEntity) ReleaseDeathMark(s, level, alive);
      return;
    case SkillExecKind::SlowTrap: {
      // C14: on the target's tile (clamped to the cast reach), else the cursor point, else the hero's feet.
      const SkillRules& rules = ctx_.data.Classes().skillRules;
      const double radius = SkillAoeRadius(rules, s, level);
      const Vec2 center = GroundCastPoint(ctx_.sys.hero->Position(), MakeCastAim(target, aim), SkillCastReach(rules, s),
                                          CmbNoTargetAhead(rules, s, CastShape::Trap));
      if (s.port.persistentGround && ctx_.sys.projectiles != nullptr) {  // C4: an armed trap
        GroundEffectSpec g;
        g.source = kHeroEntityId;
        g.skillIndex = skillIndex;
        g.skillLevel = level;
        g.skillId = s.id;
        g.vfxId = s.vfxId;
        g.center = center;
        g.radius = radius;
        g.durationMs = s.port.groundDurationMs;
        g.trigger = s.port.groundTrigger;
        g.ticks = (std::max)(1, s.port.groundTicks);
        ctx_.sys.projectiles->StartGroundEffect(g);
      } else {
        SlowTrapHits(s, level, center, radius, nullptr);
      }
      return;
    }
    case SkillExecKind::Buff:
      if (s.hasBuff) ReleaseBuff(s, level);
      return;
    case SkillExecKind::Aoe:
      if (s.aoe && SkillAoeRadius(ctx_.data.Classes().skillRules, s, level) > 0) {
        ReleaseAoe(s, level, target, aim);
      } else if (alive != kNoEntity) {
        ReleaseSingle(s, level, target, aim);
      }
      return;
    case SkillExecKind::Single:
      ReleaseSingle(s, level, target, aim);
      return;
  }
}

void CombatSystem::ReleaseTeleport(const SkillDef& s, int32_t level, const SkillAim& aim, int32_t manaCost) {
  Hero& hero = *ctx_.sys.hero;
  ZoneRuntime* zone = ctx_.sys.zone;
  if (zone == nullptr || !zone->HasZone()) return;
  const TeleportAim ta = CmbTeleportAim(ctx_, aim, PlayerLock());
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
  // C14: the selection survives the blink (the web cleared it): the mage blinks away and keeps casting at it.
  EvSkillVfx v;
  v.skillId = s.id;
  v.vfxId = s.vfxId;
  v.caster = kHeroEntityId;
  v.origin = from;
  v.point = dest.Center();
  v.dir = CmbDirOr(dest.Center() - from, Vec2());
  EmitSkillVfx(s, v);
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
  SetTargetInternal(target, target == PlayerLock());  // the lock it struck stays the player's own if it was
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
  EmitSkillVfx(s, v);
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
  EmitSkillVfx(s, v);
  ctx_.events.Log(MakeLoc("zone.combat.deathMarkApplied", {CmbSkillNameArg(s), KeyArg("targetName", nameKey)}),
                  LogType::Combat);
}

// Slow trap (classes 10.3): every alive monster within the radius takes the skill hit (no status rules, no impact
// burst); survivors get slow round(buffValue x 100) for buffDuration. Log slowTrapHit {count}. On a multi-tick C4
// effect the slow is rolled once per target (its first tick), like the damage.
void CombatSystem::SlowTrapHits(const SkillDef& s, int32_t level, Vec2 center, double radius,
                                const GroundTick* ground) {
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
      h.ground = ground;
      h.applyStatusRules = false;
      h.impactBurst = false;
      h.hasFrom = true;
      h.from = center;
      o = HeroHitMonster(h);
      if (o.dodged || o.killed) continue;  // C1: a dodged hit applies nothing
    } else if (ground != nullptr && ground->ticks > 1) {
      if (FindGroundRoll(ground->effect, id) != nullptr) continue;  // slowed on its first tick
      GroundRoll seen;
      seen.effect = ground->effect;
      seen.target = id;
      groundRolls_.push_back(seen);
    }
    if (!o.rollStatuses) continue;
    for (const StatusRule& rule : s.statusRules) {
      StatusRuleInput in;
      in.dealtDamage = o.statusDamage;
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
  EmitSkillVfx(s, v);
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
  EmitSkillVfx(s, v);
}

void CombatSystem::ReleaseAoe(const SkillDef& s, int32_t level, const CastTarget& target, const SkillAim& aim) {
  Hero& hero = *ctx_.sys.hero;
  MonsterSystem* ms = ctx_.sys.monsters;
  const SkillRules& rules = ctx_.data.Classes().skillRules;
  const double now = ctx_.Now();
  const double radius = SkillAoeRadius(rules, s, level);
  const Vec2 heroPos = hero.Position();
  const CastShape shape = SkillCastShape(s);
  const double reach = SkillCastReach(rules, s);
  const MonsterInstance* tm = target.id != kNoEntity ? ms->Find(target.id) : nullptr;
  const bool targetAlive = tm != nullptr && tm->IsAlive();
  const CastAim ca = MakeCastAim(target, aim);
  // C14: one aim for the whole cast - toward the target (or its last position), else the cursor, else the facing.
  const Vec2 dir = CastDirection(heroPos, ca);

  // Ground AoEs and traps land on the target's tile (its last position if it died), else the cursor point, else ahead
  // (traps: the hero's feet), clamped to the cast reach; novas, chains, cones and lines start at the hero.
  const bool grounded = shape == CastShape::Ground || shape == CastShape::Trap;
  const Vec2 center = grounded ? GroundCastPoint(heroPos, ca, reach, CmbNoTargetAhead(rules, s, shape)) : heroPos;
  const bool blastFrom = grounded && !s.hasArrowDelay;

  // C4 persistent ground effects (fire_wall, arrow_rain periodic; explosive_trap, chain_trap armed).
  if (s.port.persistentGround && ctx_.sys.projectiles != nullptr) {
    GroundEffectSpec g;
    g.source = kHeroEntityId;
    g.skillIndex = hero.Skills().IndexOf(s.id);
    g.skillLevel = level;
    g.skillId = s.id;
    g.vfxId = s.vfxId;
    g.center = center;
    g.radius = radius;
    g.durationMs = s.port.groundDurationMs;
    g.trigger = s.port.groundTrigger;
    g.ticks = (std::max)(1, s.port.groundTicks);
    ctx_.sys.projectiles->StartGroundEffect(g);
    return;
  }

  // Targets (fixed at release). A cone or a line keeps its own size, and the resolved target within the cast reach is
  // always among its hits (the arrow aimed at it reaches it).
  std::vector<EntityId> targets;
  Vec2 vfxPoint = center;
  const bool targetInReach = targetAlive && DistSq(heroPos, tm->pos) <= reach * reach;
  if (shape == CastShape::Line) {
    const double halfWidth = radius * s.lineHalfWidthFactor;
    const Vec2 end = heroPos + dir * s.range;
    targets = CandidatesAlongLine(AliveCandidates(heroPos, s.range + halfWidth), heroPos, end, kNoEntity, s.range,
                                  halfWidth);
    if (targetInReach && std::find(targets.begin(), targets.end(), target.id) == targets.end()) {
      targets.push_back(target.id);
    }
    vfxPoint = targetInReach && Dist(heroPos, tm->pos) > s.range ? tm->pos : end;
  } else if (shape == CastShape::Cone) {
    // C4 multishot: a 50 degree fan from the hero along the aim.
    targets = CandidatesInCone(AliveCandidates(heroPos, radius), heroPos, dir, radius, s.port.coneDeg);
    if (targetInReach && std::find(targets.begin(), targets.end(), target.id) == targets.end()) {
      targets.push_back(target.id);
    }
    vfxPoint = targetInReach ? tm->pos : heroPos + dir * radius;
  } else if (shape == CastShape::Chain) {
    // C4 chain lightning: link 0 is the target, then the nearest monster not hit yet within aoeRadius of the previous
    // link (at most chainMaxLinks). No target: the bolt flies along the aim and grounds (no hit).
    if (targetAlive) {
      const int32_t cap = rules.chainMaxLinks;
      // Every link lies within (links - 1) hops of the first; without a cap, the whole zone.
      double span = radius * static_cast<double>(cap);
      if (cap <= 0) {
        span = ctx_.sys.zone != nullptr && ctx_.sys.zone->HasZone()
                   ? static_cast<double>((std::max)(ctx_.sys.zone->Grid().Cols(), ctx_.sys.zone->Grid().Rows()))
                   : 1e4;
      }
      targets = ChainLinks(AliveCandidates(tm->pos, span), target.id, tm->pos, radius, cap);
      vfxPoint = tm->pos;
    } else {
      vfxPoint = heroPos + dir * s.range;
    }
  } else {
    targets = CandidatesInRadius(AliveCandidates(center, radius), center, radius);
  }
  const int32_t stagger = s.port.chainStaggerMs;

  EvSkillVfx v;
  v.skillId = s.id;
  v.vfxId = s.vfxId;
  v.caster = kHeroEntityId;
  v.target = targetAlive ? target.id : kNoEntity;
  v.origin = heroPos;
  v.point = vfxPoint;
  v.radius = radius;
  if (shape == CastShape::Chain || shape == CastShape::Cone || shape == CastShape::Line) v.dir = dir;
  for (EntityId id : targets) {
    if (const MonsterInstance* m = ms->Find(id)) {
      v.points.push_back(m->pos);
      v.pointTargets.push_back(id);
    }
  }
  if (shape == CastShape::Chain && targets.empty()) {  // the grounded bolt's end
    v.points.push_back(vfxPoint);
    v.pointTargets.push_back(kNoEntity);
  }
  v.staggerMs = stagger;
  EmitSkillVfx(s, v);
  // K3: a fire area burns the bodies it lands on now (a delayed fall burns at its fall, FireDelayedHit).
  if (s.damageType == DamageType::Fire && !(s.aoeDelayMs > 0) && shape != CastShape::Chain) {
    AbilityShapeDef area;
    Vec2 areaOrigin = center;
    if (shape == CastShape::Line) {
      area.kind = AbilityShapeKind::Line;
      area.length = s.range;
      area.width = 2 * radius * s.lineHalfWidthFactor;
      areaOrigin = heroPos;
    } else if (shape == CastShape::Cone) {
      area.kind = AbilityShapeKind::Cone;
      area.radius = radius;
      area.arcDeg = s.port.coneDeg;
      areaOrigin = heroPos;
    } else {
      area.kind = AbilityShapeKind::Circle;
      area.radius = radius;
    }
    BurnCorpsesIfFire(s.damageType, area, areaOrigin, dir);
  }

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
  // C4 chain lightning: link 0 lands now, link k at release + k x stagger (FireChainLink); one batch shake at the end.
  if (stagger > 0 && targets.size() > 1) {
    PendingHit ph;
    ph.skillIndex = hero.Skills().IndexOf(s.id);
    ph.level = level;
    ph.targets = targets;
    ph.center = center;
    ph.blastFrom = blastFrom;
    ph.chain = true;
    ph.nextLink = 1;
    ph.baseMs = now;
    ph.staggerMs = static_cast<double>(stagger);
    const HeroHitOutcome first = ApplySkillHit(s, level, targets[0], blastFrom, center, nullptr);
    ph.chainHits = first.landed ? 1 : 0;
    const EntityId next = ph.targets[1];
    const int32_t slot = AllocHit(std::move(ph));
    ctx_.timers.Schedule(now + static_cast<double>(stagger), TimerOwner::Combat,
                         static_cast<uint16_t>(CombatTimerKind::SkillDelayedHit), kHeroEntityId, next, slot);
    return;
  }
  int32_t immediate = 0;
  for (size_t k = 0; k < targets.size(); ++k) {
    const EntityId id = targets[k];
    double delay = 0;
    if (s.hasArrowDelay) {
      if (const MonsterInstance* m = ms->Find(id)) delay = SkillArrowDelayMs(s, heroPos, m->pos);
    }
    if (delay > 0) {
      PendingHit ph;
      ph.skillIndex = hero.Skills().IndexOf(s.id);
      ph.level = level;
      ph.targets.assign(1, id);
      ph.center = center;
      ph.blastFrom = blastFrom;
      ph.batchShake = false;
      const int32_t slot = AllocHit(std::move(ph));
      ctx_.timers.Schedule(now + delay, TimerOwner::Combat, static_cast<uint16_t>(CombatTimerKind::SkillDelayedHit),
                           kHeroEntityId, id, slot);
    } else {
      const HeroHitOutcome o = ApplySkillHit(s, level, id, blastFrom, center, nullptr);
      if (o.landed) ++immediate;
    }
  }
  if (immediate > 0) {
    const ShakeRequest sh = AoeHitShake(ctx_.data.Combat().hitFeedback, immediate);
    Shake(sh.durationMs, sh.intensity);
  }
}

void CombatSystem::FireDelayedHit(int32_t slot) {
  if (slot < 0 || static_cast<size_t>(slot) >= hits_.size() || !hits_[static_cast<size_t>(slot)].used) return;
  if (hits_[static_cast<size_t>(slot)].chain) {
    FireChainLink(slot);
    return;
  }
  const PendingHit ph = hits_[static_cast<size_t>(slot)];
  hits_[static_cast<size_t>(slot)].used = false;
  if (ctx_.session.transitioning) return;
  const SkillBook& book = ctx_.sys.hero->Skills();
  if (ph.skillIndex < 0 || static_cast<size_t>(ph.skillIndex) >= book.SkillCount()) return;
  const SkillDef& s = book.Skill(ph.skillIndex);
  if (HeroAlive()) {  // stillCasting()
    int32_t hits = 0;
    for (EntityId id : ph.targets) {
      const HeroHitOutcome o = ApplySkillHit(s, ph.level, id, ph.blastFrom, ph.center, nullptr);
      if (o.landed) ++hits;
    }
    if (ph.batchShake && s.damageType == DamageType::Fire) {  // K3: the fall burns the bodies under it
      AbilityShapeDef area;
      area.kind = AbilityShapeKind::Circle;
      area.radius = SkillAoeRadius(ctx_.data.Classes().skillRules, s, ph.level);
      BurnCorpsesIfFire(s.damageType, area, ph.center, Vec2(1, 0));
    }
    if (ph.batchShake && hits > 0) {
      const ShakeRequest sh = AoeHitShake(ctx_.data.Combat().hitFeedback, hits);
      Shake(sh.durationMs, sh.intensity);
    }
  }
  // The fall's end (the meteor explosion): its authored shake comes after the batch's hits and shake (the web's damage
  // timer runs in the scene update, the FxEngine task in post-update).
  if (ph.batchShake) SkillShake(s, SkillShakeAnchor::Impact);
}

// C4 chain lightning: one link per timer (release + link x stagger); the hero's death or a zone transition ends the
// chain (stillCasting). After the last link: the 6.5 AoE shake for every link that landed.
void CombatSystem::FireChainLink(int32_t slot) {
  PendingHit ph = hits_[static_cast<size_t>(slot)];
  hits_[static_cast<size_t>(slot)].used = false;
  hits_[static_cast<size_t>(slot)].targets.clear();
  if (!HeroAlive() || ctx_.session.transitioning) return;
  const SkillBook& book = ctx_.sys.hero->Skills();
  if (ph.skillIndex < 0 || static_cast<size_t>(ph.skillIndex) >= book.SkillCount()) return;
  if (ph.nextLink >= ph.targets.size()) return;
  const SkillDef& s = book.Skill(ph.skillIndex);
  const HeroHitOutcome o = ApplySkillHit(s, ph.level, ph.targets[ph.nextLink], ph.blastFrom, ph.center, nullptr);
  if (o.landed) ++ph.chainHits;
  ++ph.nextLink;
  if (ph.nextLink < ph.targets.size()) {
    const double due = ph.baseMs + ph.staggerMs * static_cast<double>(ph.nextLink);
    const EntityId next = ph.targets[ph.nextLink];
    const int32_t again = AllocHit(std::move(ph));
    ctx_.timers.Schedule(due, TimerOwner::Combat, static_cast<uint16_t>(CombatTimerKind::SkillDelayedHit),
                         kHeroEntityId, next, again);
    return;
  }
  if (ph.chainHits > 0) {
    const ShakeRequest sh = AoeHitShake(ctx_.data.Combat().hitFeedback, ph.chainHits);
    Shake(sh.durationMs, sh.intensity);
  }
}

// Single target (classes 9.8): projectile on arrival, C4 Charge after its dash, else now. C14: a projectile without a
// living target flies along the aim (cursor, else facing) for the cast reach and fizzles.
void CombatSystem::ReleaseSingle(const SkillDef& s, int32_t level, const CastTarget& target, const SkillAim& aim) {
  Hero& hero = *ctx_.sys.hero;
  const MonsterInstance* m = target.id != kNoEntity ? ctx_.sys.monsters->Find(target.id) : nullptr;
  const bool alive = m != nullptr && m->IsAlive();
  const Vec2 heroPos = hero.Position();
  const int32_t skillIndex = hero.Skills().IndexOf(s.id);
  if (!alive) {
    if (!s.hasProjectile || ctx_.sys.projectiles == nullptr) return;
    const Vec2 dir = CastDirection(heroPos, MakeCastAim(target, aim));
    const Vec2 end = heroPos + dir * SkillCastReach(ctx_.data.Classes().skillRules, s);
    EvSkillVfx v;  // the cast glint (the flight is the projectile)
    v.skillId = s.id;
    v.vfxId = s.vfxId;
    v.caster = kHeroEntityId;
    v.origin = heroPos;
    v.point = end;
    v.dir = dir;
    EmitSkillVfx(s, v);
    ProjectileSpec p;
    p.kind = ProjectileKind::HeroSkill;
    p.source = kHeroEntityId;
    p.target = kNoEntity;  // not target-locked: ends at `to` without a hit (EvProjectileEnded hit = false)
    p.from = heroPos;
    p.to = end;
    p.travelMs = SkillTravelMs(s, heroPos, end);
    p.vfxId = s.vfxId;
    p.color = s.impactColor;
    p.skillIndex = skillIndex;
    p.skillLevel = level;
    ctx_.sys.projectiles->Launch(p);
    return;
  }
  const EntityId id = target.id;
  const Vec2 mpos = m->pos;

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
                           static_cast<uint16_t>(CombatTimerKind::ChargeDashEnd), kHeroEntityId, id, slot);
      return;
    }
  }

  EvSkillVfx v;
  v.skillId = s.id;
  v.vfxId = s.vfxId;
  v.caster = kHeroEntityId;
  v.target = id;
  v.origin = heroPos;
  v.point = mpos;
  v.dir = CmbDirOr(mpos - heroPos, Vec2());
  const double travel = SkillTravelMs(s, heroPos, mpos);
  if (travel > 0 && ctx_.sys.projectiles != nullptr) {
    EmitSkillVfx(s, v);  // the cast glint (the flight is the projectile)
    ProjectileSpec p;
    p.kind = ProjectileKind::HeroSkill;
    p.source = kHeroEntityId;
    p.target = id;
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
  EmitSkillVfx(s, v);
  ApplySkillHit(s, level, id, false, heroPos, nullptr, true);
}

void CombatSystem::ResolveChargeDash(int32_t slot) {
  if (slot < 0 || static_cast<size_t>(slot) >= releases_.size() || !releases_[static_cast<size_t>(slot)].used) return;
  const PendingRelease pr = releases_[static_cast<size_t>(slot)];
  releases_[static_cast<size_t>(slot)].used = false;
  if (!HeroAlive() || ctx_.session.transitioning) return;
  const SkillBook& book = ctx_.sys.hero->Skills();
  if (pr.skillIndex < 0 || static_cast<size_t>(pr.skillIndex) >= book.SkillCount()) return;
  const SkillDef& s = book.Skill(pr.skillIndex);
  const MonsterInstance* m = ctx_.sys.monsters->Find(pr.target.id);
  if (m == nullptr || !m->IsAlive()) return;
  // The hit lands where the hero stands: it needs the target within melee reach (attack range + range slack), which an
  // interrupted dash (C5 stun / freeze) may not reach.
  const double reach = ctx_.sys.hero->Derived().attackRange + ctx_.data.Classes().skillRules.rangeSlackTiles;
  if (DistSq(ctx_.sys.hero->Position(), m->pos) > reach * reach) return;
  EvSkillVfx v;
  v.skillId = s.id;
  v.vfxId = s.vfxId;
  v.caster = kHeroEntityId;
  v.target = pr.target.id;
  v.origin = ctx_.sys.hero->Position();
  v.point = m->pos;
  v.dir = CmbDirOr(m->pos - v.origin, Vec2());
  EmitSkillVfx(s, v);
  ApplySkillHit(s, pr.level, pr.target.id, false, ctx_.sys.hero->Position(), nullptr, true);
}

void CombatSystem::OnSkillProjectileArrived(const Projectile& p) {
  if (ctx_.session.transitioning) return;
  const SkillBook& book = ctx_.sys.hero->Skills();
  if (p.spec.skillIndex < 0 || static_cast<size_t>(p.spec.skillIndex) >= book.SkillCount()) return;
  const SkillDef& s = book.Skill(p.spec.skillIndex);
  const MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(p.spec.target) : nullptr;
  // Target-locked: lands wherever the target is, unless it (or the hero) died in flight.
  if (HeroAlive() && m != nullptr && m->IsAlive()) {
    ApplySkillHit(s, p.spec.skillLevel, p.spec.target, false, p.spec.from, nullptr, true);
  }
  // The flight's end (the fireball explosion and its authored shake) plays whatever became of the target or the hero,
  // after the hit's own impact shake (the web's damage timer runs in the scene update, the FxEngine task in post-update).
  // A C14 no-target shot only fizzles (EvProjectileEnded.hit = false): no explosion, no shake.
  if (p.spec.target != kNoEntity) SkillShake(s, SkillShakeAnchor::Impact);
}

void CombatSystem::OnGroundEffectTick(const GroundEffect& g, int32_t tickIndex) {
  GroundTick tick;
  tick.effect = g.id;
  tick.index = tickIndex;
  tick.ticks = (std::max)(1, g.spec.ticks);
  // Rolls of effects that ended before their last tick (zone change) are dropped here; this effect's after its last.
  if (ctx_.sys.projectiles != nullptr) {
    const ProjectileSystem& ps = *ctx_.sys.projectiles;
    groundRolls_.erase(std::remove_if(groundRolls_.begin(), groundRolls_.end(),
                                      [&ps, &g](const GroundRoll& r) {
                                        return r.effect != g.id && ps.FindGroundEffect(r.effect) == nullptr;
                                      }),
                       groundRolls_.end());
  }
  const bool lastTick = tickIndex + 1 >= tick.ticks;
  ResolveGroundTick(g, tick);
  if (lastTick) DropGroundRolls(g.id);
}

void CombatSystem::ResolveGroundTick(const GroundEffect& g, const GroundTick& tick) {
  if (!HeroAlive() || ctx_.session.transitioning) return;
  const SkillBook& book = ctx_.sys.hero->Skills();
  if (g.spec.skillIndex < 0 || static_cast<size_t>(g.spec.skillIndex) >= book.SkillCount()) return;
  const SkillDef& s = book.Skill(g.spec.skillIndex);
  if (s.execKind == SkillExecKind::SlowTrap) {
    SlowTrapHits(s, g.spec.skillLevel, g.spec.center, g.spec.radius, &tick);
    return;
  }
  const std::vector<EntityId> targets =
      CandidatesInRadius(AliveCandidates(g.spec.center, g.spec.radius), g.spec.center, g.spec.radius);
  if (tick.index == 0 || g.spec.trigger == GroundTrigger::Armed) {
    EvSkillVfx v;
    v.skillId = s.id;
    v.vfxId = s.vfxId;
    v.caster = kHeroEntityId;
    v.origin = g.spec.center;
    v.point = g.spec.center;
    v.radius = g.spec.radius;
    EmitSkillVfx(s, v);
  }
  // One AoE batch shake (6.5) for the targets whose web hit lands now (their first landed share), none for later ticks.
  int32_t hits = 0;
  for (EntityId id : targets) {
    const HeroHitOutcome o = ApplySkillHit(s, g.spec.skillLevel, id, true, g.spec.center, &tick);
    if (o.landed && o.impact) ++hits;
  }
  if (s.damageType == DamageType::Fire) {  // K3: each tick of a fire wall / trap burns the bodies in it
    AbilityShapeDef area;
    area.kind = AbilityShapeKind::Circle;
    area.radius = g.spec.radius;
    BurnCorpsesIfFire(s.damageType, area, g.spec.center, Vec2(1, 0));
  }
  if (hits > 0) {
    const ShakeRequest sh = AoeHitShake(ctx_.data.Combat().hitFeedback, hits);
    Shake(sh.durationMs, sh.intensity);
  }
}

// Shared skill hit (classes 9.7 applyHit / 9.8): calculateDamage with the skill + synergy, combustion bonus,
// takeDamage, steal, status rules (alive targets), kill credit, impact burst (skill colour).
CombatSystem::HeroHitOutcome CombatSystem::ApplySkillHit(const SkillDef& s, int32_t level, EntityId target,
                                                         bool blastFrom, Vec2 center, const GroundTick* ground,
                                                         bool singleTarget) {
  const MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(target) : nullptr;
  if (m == nullptr || !m->IsAlive()) return {};
  HeroHitSpec h;
  h.target = target;
  h.skill = &s;
  h.level = level;
  h.ground = ground;
  h.applyStatusRules = true;
  h.impactBurst = s.execKind != SkillExecKind::DeathMark && s.execKind != SkillExecKind::SlowTrap;
  h.hasFrom = true;
  h.from = blastFrom && Dist(m->pos, center) > kCombatBlastCentreTiles ? center : ctx_.sys.hero->Position();
  h.scorch = singleTarget;
  const HeroHitOutcome o = HeroHitMonster(h);
  if (o.attempted && !o.dodged && !o.killed && o.rollStatuses) ApplySkillStatuses(s, level, target, o.statusDamage);
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

// Step 6 (classes 4.2, 6.6 / 10.1, 16): the Life Regen heal, then the Unyielding proc (so it reads the hp ratio AFTER
// this step's Life Regen, web ZoneScene.ts:1372-1392), then the Dual Wield Mastery buff.
void CombatSystem::TickPassives(double dtMs) {
  Hero& hero = *ctx_.sys.hero;
  if (!HeroAlive()) return;
  // The step's recovery modifiers (web `recovery`, 16 steps 2 / 5): campfire from the position BEFORE this step's
  // movement, poison halves HP regen. Shared with TickHeroUpdate's mana / HP regen.
  RegenModifiers mods;
  if (ctx_.sys.zone != nullptr && ctx_.sys.zone->HasZone() && ctx_.sys.zone->NearCampfire(hero.Position())) {
    mods.hpMul = ctx_.data.Classes().formulas.campfireHpMultiplier;
    mods.mpMul = ctx_.data.Classes().formulas.campfireManaMultiplier;
  }
  if (ctx_.sys.status != nullptr && ctx_.sys.status->Has(kHeroEntityId, StatusType::Poison)) {
    mods.hpMul *= ctx_.data.Classes().statusRules.poisonedHpRegenMultiplier;
  }
  // K6: Ashnumb takes its share off the step's HP regeneration as off every heal (Hero::Heal).
  if (ctx_.sys.status != nullptr) {
    const double healMul = ctx_.sys.status->HealingReceivedMul(kHeroEntityId);
    if (healMul < 1.0) mods.hpMul *= healMul;
  }
  hero.stepRegen = mods;
  hero.TickLifeRegen(dtMs, mods);
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
        EmitSkillVfx(s, v);
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

// Step 7 minus movement: spirit drain, then MP / HP regen with the step's campfire and poison modifiers (classes 4.1),
// computed by TickPassives (step 6) before the movement.
void CombatSystem::TickHeroUpdate(double dtMs) {
  Hero& hero = *ctx_.sys.hero;
  if (!HeroAlive()) return;
  Spirit& sp = hero.GetSpirit();
  if (sp.Update(dtMs)) {
    ctx_.events.Emit(EvSpiritChanged{sp.Value(), sp.MaxValue(), false, 0});
    ctx_.events.Emit(EvResonance{false, sp.Profile().id, 0});
  }
  hero.TickRegen(dtMs, hero.stepRegen);
}

// Step 9 handleCombat: prune buffs (FIX Q19 monsters, then the hero), monster swings, the hero's basic attack; armed
// traps. FIX Q19 prunes monster buffs every frame, so it runs before the web's dead-hero gate: a death mark or taunt
// still runs out during the 1100 ms Dying window. The rest stays behind the gate (an armed trap does not spring for a
// dying hero; it stays armed until its duration ends).
void CombatSystem::TickCombat() {
  Hero& hero = *ctx_.sys.hero;
  const double now = ctx_.Now();
  MonsterSystem* ms = ctx_.sys.monsters;
  if (ms != nullptr) ms->PruneBuffs(now);
  AbortImmobilizedAbilities();  // K2 (S7): before the dead-hero gate, like the buff prune
  if (!HeroAlive()) return;
  hero.Buffs().Prune(now);
  if (ms == nullptr) return;

  // Monster swings (5.1).
  const CombatInputDef& in = ctx_.data.Combat().input;
  const AnimTimingTable& at = ctx_.data.Combat().anim;
  std::vector<EntityId>& nearby = scratchNearby_;  // only TickCombat uses it (strike handlers run from timers)
  ms->QueryAlive(hero.Position(), in.monsterSwingScanRadiusTiles, nearby);
  for (EntityId id : nearby) {
    MonsterInstance* m = ms->Find(id);
    if (m == nullptr || !m->IsAlive()) continue;
    // K3: Chase as well as Attack, for the raise / summon / buff / call cast between swings (TryStartSupportCast).
    const bool attacking = m->state == MonsterState::Attack;
    if (!attacking && m->state != MonsterState::Chase) continue;
    if (ctx_.sys.status != nullptr && ctx_.sys.status->IsImmobilized(id)) continue;
    if (m->pendingAbility >= 0) continue;  // K2: one action at a time (a windup may outlast the attack interval)
    if (m->consumeCorpse != 0) continue;   // K3: eating (the consume channel roots it)
    if (now < m->phaseActionUntilMs) continue;  // K4: rooted while a phase's enter action plays (no swing or cast)
    if (!attacking || (m->lastAttackMs != 0 && now - m->lastAttackMs + kCombatSwingEpsilonMs < m->AttackIntervalMs())) {
      if (!m->def.abilities.empty()) TryStartSupportCast(*m, now);  // no swing due: a support cast may start
      continue;
    }
    // K2: a due swing becomes the first ready ability, unless a pet's taunt holds the monster (the taunt draws the
    // swing to the beast below; the ability clock runs on). The pick comes before the stray-swing draw, so a stray
    // roll never takes an ability from the hero.
    if (!m->def.abilities.empty()) {
      StartAbilityClock(*m, now);
      const bool taunted = ctx_.sys.petCompanion != nullptr && ctx_.sys.petCompanion->TauntHolds(id);
      if (!taunted && TryStartAbility(*m, now)) continue;
    }
    if (ctx_.sys.petCompanion != nullptr && ctx_.sys.petCompanion->InterceptMonsterAttack(id)) continue;
    m = ms->Find(id);
    if (m == nullptr) continue;
    m->lastAttackMs = now;
    m->lastStrikeMs = now;  // K5
    const ActionTiming timing =
        ComputeAttackTiming(at, ctx_.data.Assets(), MonsterArtGameId(ctx_.data.Assets(), m->def.spriteKey, m->def.id),
                            m->def.animCategory, m->AttackIntervalMs());
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
    // 2F review (a held lock that steps away: a kite, a tether, a blink): the hero walks to its lock again (the C7
    // approach) when the lock left its reach by itself - the hero reached it and has had no movement order since (the
    // locomotion's MoveGeneration is unchanged: a click-move, keyboard walk, dodge or hold-move ends this) - and the
    // hero stands idle; never past targetCycleRangeTiles. The web pathed once to the monster's tile per click.
    if (tm != nullptr && tm->IsAlive() && hero.attackTarget == target && loco != nullptr) {
      const double d2 = DistSq(hero.Position(), tm->pos);
      if (d2 <= d.attackRange * d.attackRange) {
        reachLock_ = target;
        reachLockGeneration_ = loco->MoveGeneration();
      } else if (reachLock_ == target && reachLockGeneration_ == loco->MoveGeneration() && !loco->IsMoving() &&
                 !loco->IsApproaching()) {
        const double far = in.targetCycleRangeTiles;
        if (d2 <= far * far && loco->Approach(target, d.attackRange)) reachLockGeneration_ = loco->MoveGeneration();
      }
    }
    if (tm != nullptr && tm->IsAlive() && DistSq(hero.Position(), tm->pos) <= d.attackRange * d.attackRange &&
        (hero.lastAttackMs == 0 || now - hero.lastAttackMs + kCombatSwingEpsilonMs >= d.attackSpeedMs)) {
      hero.lastAttackMs = now;
      const std::string art = HeroArtGameId(ctx_.data.Assets(), hero.Class());
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

// Auto-battle's gate on top of EvaluateCast (9.4 with C1, C14). Each skill needs its own reason to fire at the lock:
// teleport is a movement skill (never automatic); a Self buff needs only the living lock (taunt_roar: its taunt blast
// must reach it); a nova must reach the lock with its blast; every shape aimed at the lock (Melee / Dash / Projectile /
// Instant / Ground / Trap / Chain / Cone / Line) is reach-checked by EvaluateCast, and within that reach its release
// lands on the lock (a ground AoE or trap on its tile; a cone or line always includes it; a chain starts on it).
bool CombatSystem::AutoSkillFits(int32_t skillIndex, EntityId lock) const {
  const SkillBook& book = ctx_.sys.hero->Skills();
  if (skillIndex < 0 || static_cast<size_t>(skillIndex) >= book.SkillCount()) return false;
  const SkillDef& s = book.Skill(skillIndex);
  const CastShape shape = SkillCastShape(s);
  if (shape == CastShape::Blink) return false;
  const MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(lock) : nullptr;
  if (m == nullptr || !m->IsAlive()) return false;
  const SkillRules& rules = ctx_.data.Classes().skillRules;
  const Vec2 heroPos = ctx_.sys.hero->Position();
  const double radius = SkillAoeRadius(rules, s, book.Level(skillIndex));
  switch (shape) {
    case CastShape::Self:
      return !s.aoe || DistSq(heroPos, m->pos) <= radius * radius;
    case CastShape::Nova:
      return DistSq(heroPos, m->pos) <= radius * radius;
    case CastShape::Blink:
      return false;
    case CastShape::Melee:
    case CastShape::Dash:
    case CastShape::Projectile:
    case CastShape::Instant:
    case CastShape::Ground:
    case CastShape::Trap:
    case CastShape::Chain:
    case CastShape::Cone:
    case CastShape::Line:
      return true;
  }
  return false;
}

// Step 12 handleAutoCombat (9.4 with C1: the first skill that can execute; C7 approach stops at attack range). Skills
// are cast only while a living lock is held (the web's targeted first skill never fired without one) and must fit it
// (AutoSkillFits): no buffs, empty-air AoEs, traps or teleports while idling with auto-battle on. C14: without a lock it
// acquires with the press's own (c) auto-pick - the nearest alive monster within castAcquireMinTiles, which the ring
// already shows - and the cast goes through RequestSkill with an empty aim, so the resolver releases it at that lock
// like a manual press.
void CombatSystem::TickAutoCombat() {
  Hero& hero = *ctx_.sys.hero;
  if (!hero.autoCombat || !HeroAlive()) return;
  MonsterSystem* ms = ctx_.sys.monsters;
  if (ms == nullptr) return;
  if (hero.attackTarget == kNoEntity) {
    if (ctx_.sys.locomotion != nullptr && ctx_.sys.locomotion->IsMoving()) return;  // never override a click path
    const EntityId pick = AutoPickTarget(ctx_.data.Classes().skillRules.castAcquireMinTiles);
    if (const MonsterInstance* m = pick != kNoEntity ? ms->Find(pick) : nullptr) {
      const double range = hero.Derived().attackRange;
      // 9.4 step 1: target it (an auto-pick: never the C8 teleport lock), and path to it only when beyond attack range
      // (C7: stopping there).
      SetTargetInternal(pick, false);
      if (DistSq(hero.Position(), m->pos) > range * range && ctx_.sys.locomotion != nullptr) {
        ctx_.sys.locomotion->Approach(pick, range);
      }
    }
  }
  const EntityId lock = hero.attackTarget;  // the held lock, or the one taken this step
  const MonsterInstance* lm = lock != kNoEntity ? ms->Find(lock) : nullptr;
  if (lm == nullptr || !lm->IsAlive()) return;
  const SkillBook& book = hero.Skills();
  for (size_t i = 0; i < book.SkillCount(); ++i) {  // autoSkillPriority = class definition order
    const int32_t idx = static_cast<int32_t>(i);
    if (book.Skill(idx).passive || EvaluateCast(idx, lock, false) != CastGate::Ok || !AutoSkillFits(idx, lock)) {
      continue;
    }
    RequestSkill(idx, SkillAim{});  // C14 rule (a): resolves to this lock
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
      // C14 (replaces 9.3 step 9 / FIX Q7): the press-time target while it lives, wherever it went (a ranged cast
      // follows it; a Melee strike whiffs when it stepped out of reach - never another monster). A target that died
      // before the beat is replaced through the resolver from the hero (no hover; only a monster within the skill's
      // release reach), except that Ground / Trap / Cone / Line casts keep aiming at its last position. With no
      // replacement a Projectile / Chain flies toward that last position, the others fizzle (cost spent). A press
      // that had no target keeps its no-target aim (cursor point / direction, facing) - the hero already turned to it,
      // so a monster stepping into reach during the beat does not capture it.
      CastTarget target = pr.target;
      const MonsterInstance* m = target.id != kNoEntity ? ctx_.sys.monsters->Find(target.id) : nullptr;
      const CastShape shape = SkillCastShape(s);
      const SkillRules& rules = ctx_.data.Classes().skillRules;
      const Vec2 heroPos = ctx_.sys.hero->Position();
      const double reach = SkillReleaseReach(rules, s);
      if (m != nullptr && m->IsAlive()) {
        target.pos = m->pos;
        if (shape == CastShape::Melee && DistSq(heroPos, m->pos) > reach * reach) return;  // whiff (cost spent)
      } else if (target.hasPos) {
        target.id = kNoEntity;
        const bool aimsAtSpot = shape == CastShape::Ground || shape == CastShape::Trap || shape == CastShape::Cone ||
                                shape == CastShape::Line;
        if (!aimsAtSpot) {
          const EntityId again = ResolveCastTarget(s, SkillAim{}, false, reach);
          const MonsterInstance* am = again != kNoEntity ? ctx_.sys.monsters->Find(again) : nullptr;
          if (am != nullptr && DistSq(heroPos, am->pos) <= reach * reach) {
            target = CastTarget{again, true, am->pos};
            SelectCastTarget(s, again);
          }
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
      if (ctx_.sys.hero->Life() == HeroLife::Dying) Respawn(true);
      return;
    case CombatTimerKind::ChargeDashEnd:
      ResolveChargeDash(t.param);
      return;
    case CombatTimerKind::DeathSaveRearm:
      return;  // T11 lives in Hero::deathSaveReadyAtMs
    case CombatTimerKind::SkillShake: {
      const std::vector<SkillShakeDef>& rows = ctx_.data.Combat().hitFeedback.skillShakes;
      if (t.param < 0 || static_cast<size_t>(t.param) >= rows.size()) return;
      const SkillShakeDef& sh = rows[static_cast<size_t>(t.param)];
      CameraShake(ShakeRequest{sh.durationMs, sh.intensity}, ShakeChannel::Skill);
      return;
    }
  }
}

void CombatSystem::CancelPendingStrikes(bool emitCancelled) {
  for (const PendingStrike& s : strikes_) {
    if (s.ability && ctx_.sys.monsters != nullptr) {
      if (MonsterInstance* m = ctx_.sys.monsters->Find(s.monster)) ClearPendingAbility(*m);
    }
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
  ClearIndicator();
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
  hazards_.Clear();  // K2: UE drops the zone's hazards with the zone
  onDeath_.clear();  // K3: pending on-death effects and the zone's own go with the zone
  onDeathTaken_.clear();
  reachLock_ = kNoEntity;  // 2F review: the held lock's re-approach ends with the zone (its target is gone)
  reachLockGeneration_ = 0;
  zoneOnDeath_ = MonsterOnDeathDef{};
  zoneOnDeathLabel_.clear();
  DropGroundRolls(kNoEntity);
  buffer_.Clear();
  if (inCombat_) {
    inCombat_ = false;
    ctx_.events.Emit(EvCombatStateChanged{false});
    ctx_.bus.Publish(CombatStateChangedMsg{false});
  }
  Hero& hero = *ctx_.sys.hero;
  hero.attackTarget = kNoEntity;
  hero.lastAttackMs = 0;
  ClearIndicator();
}

// 9.5: TARGET_CHANGED whenever the shown target changes - also when a zone change drops it, so the HUD target frame
// never keeps an id from the previous zone.
void CombatSystem::ClearIndicator() {
  if (indicator_ == kNoEntity) return;
  indicator_ = kNoEntity;
  ctx_.events.Emit(EvTargetChanged{kNoEntity});
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

bool CombatSystem::HasPendingStrike(EntityId monster) const {
  for (const PendingStrike& s : strikes_) {
    if (s.monster == monster) return true;
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
  AcquireCache acquire;  // nothing changes between the slots: one auto-pick per cast reach for the whole hotbar
  for (SkillSlotView& v : out.hero.hotbar) {
    if (v.skillIndex < 0 || static_cast<size_t>(v.skillIndex) >= book.SkillCount()) continue;
    const SkillDef& s = book.Skill(v.skillIndex);
    const int32_t level = book.Level(v.skillIndex);
    if (level <= 0) continue;
    v.manaCost = CastManaCost(v.skillIndex);
    v.cooldownTotalMs = SkillCooldownMs(rules, s, level, ctx_.equip.Get(Stat::CooldownReduction));
    v.affordable = hero.Mana() >= v.manaCost;
    v.usable = EvaluateCast(v.skillIndex, ResolveCastTarget(s, SkillAim{}, false, SkillCastReach(rules, s), &acquire),
                            false) == CastGate::Ok;
  }
  for (MonsterView& m : out.monsters) m.windingUp = IsWindingUp(m.id);
  // K2: abilities winding up with a ground shape, then the live hazards.
  if (const MonsterSystem* ms = ctx_.sys.monsters) {
    for (const PendingStrike& s : strikes_) {
      if (!s.ability) continue;
      const MonsterInstance* m = ms->Find(s.monster);
      if (m == nullptr || m->pendingAbility < 0) continue;
      if (static_cast<size_t>(m->pendingAbility) >= m->def.abilities.size()) continue;
      const MonsterAbilityDef& a = m->def.abilities[static_cast<size_t>(m->pendingAbility)];
      const MonsterAbilityCast& c = m->abilityCast;
      const double progress =
          c.contactMs > c.startMs ? Clamp((now - c.startMs) / (c.contactMs - c.startMs), 0.0, 1.0) : 1.0;
      if (AbilityHasGroundShape(a.kind)) {
        TelegraphView v;
        v.entity = m->id;
        v.abilityId = a.id;
        v.shape = c.shape;
        v.origin = c.origin;
        v.dir = c.dir;
        v.startMs = c.startMs;
        v.contactMs = c.contactMs;
        v.progress = progress;
        v.style = TelegraphStyleOf(*m);
        v.kind = a.kind;
        v.hasDash = c.hasDash;  // 2F review: a charge's end (EvTelegraph::dashTo)
        v.dashTo = c.dashTo;
        out.telegraphs.push_back(std::move(v));
      } else if (AbilityKindSummons(a.kind)) {  // K3: one marker per corpse / spot
        for (const Vec2 spot : m->abilitySpots) {
          TelegraphView v;
          v.entity = m->id;
          v.abilityId = a.id;
          v.shape.kind = AbilityShapeKind::Circle;
          v.shape.radius = ctx_.data.Monsters().ai.raiseMarkerRadius;
          v.origin = spot;
          v.dir = c.dir;
          v.startMs = c.startMs;
          v.contactMs = c.contactMs;
          v.progress = progress;
          v.style = TelegraphStyleOf(*m);
          v.kind = a.kind;
          out.telegraphs.push_back(std::move(v));
        }
      }
    }
  }
  // K3: the on-death fuses (schedule order).
  for (const PendingOnDeath& e : onDeath_) {
    if (e.def.kind == OnDeathKind::Spawn || !(e.dueMs > e.startMs)) continue;
    TelegraphView v;
    v.entity = e.source;
    v.abilityId = e.label;
    v.shape = e.def.shape;
    v.origin = e.origin;
    v.dir = e.dir;
    v.startMs = e.startMs;
    v.contactMs = e.dueMs;
    v.progress = Clamp((now - e.startMs) / (e.dueMs - e.startMs), 0.0, 1.0);
    v.style = e.style;
    v.kind = OnDeathTelegraphKind(e.def.shape);
    v.source = TelegraphSource::OnDeath;
    out.telegraphs.push_back(std::move(v));
  }
  for (const HazardEntry& h : hazards_.Entries()) {
    HazardView v;
    v.id = h.id;
    v.source = h.source;
    v.abilityId = h.abilityId;
    v.shape = h.shape;
    v.origin = h.origin;
    v.dir = h.dir;
    v.damageType = h.damageType;
    v.startMs = h.startMs;
    v.untilMs = h.untilMs;
    out.hazards.push_back(std::move(v));
  }
}

}  // namespace abyss
