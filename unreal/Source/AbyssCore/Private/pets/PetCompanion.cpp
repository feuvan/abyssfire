// The active ley-beast in the world (quests-story-ch1.md 18.5; web src/systems/PetCompanion.ts). The web's
// scene.time.delayedCall closures become TimerOwner::Pets timers whose captured values live in `pending_`.
#include "abyss/base/Platform.h"

#include "abyss/pets/PetCompanion.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/I18n.h"
#include "abyss/base/Math.h"
#include "abyss/base/StrUtil.h"
#include "abyss/combat/Combat.h"
#include "abyss/combat/HitFeedback.h"
#include "abyss/combat/Projectiles.h"
#include "abyss/combat/StatusEffects.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/items/Inventory.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"
#include "abyss/story/StoryDirector.h"
#include "abyss/world/Zone.h"

namespace abyss {

namespace {

// Ability field defaults of the web code (`a.value ?? 0.08` ..., PetCompanion.ts:566-745).
constexpr double kPcHealValue = 0.08;
constexpr double kPcShieldValue = 0.2, kPcShieldMs = 5000;
constexpr double kPcBuffValue = 0.15, kPcBuffMs = 6000;
constexpr double kPcTauntRadius = 4, kPcTauntValue = 0.25, kPcTauntMs = 5000;
constexpr double kPcMarkValue = 0.15, kPcMarkMs = 6000;
constexpr double kPcStrikeDamage = 1.5, kPcBleedMs = 4000;
constexpr double kPcBoltDamage = 1.5, kPcBoltSplashShare = 0.5;
constexpr double kPcConeDamage = 1.3, kPcConeArc = kPi / 3.0, kPcConeRadius = 4, kPcConeOriginReach = 0.6;
constexpr double kPcNovaDamage = 1.0, kPcNovaRadius = 2.5;
constexpr double kPcDotMs = 3000;  // burn / slow default duration
constexpr double kPcReviveValue = 0.4;
// Ability VFX colours (PetCompanion.ts:579): fire, arcane, else the beast's colour.
constexpr uint32_t kPcFireColor = 0xff7a2a;
constexpr uint32_t kPcArcaneColor = 0xcc44cc;
// Revive flash (PetCompanion.ts:152-153).
constexpr uint32_t kPcReviveFlashColor = 0xffb040;
constexpr double kPcReviveFlashMs = 220, kPcReviveFlashAlpha = 0.35;
// Basic buff-useful range (abilityUseful 'buff': target within 8 tiles) and the minimum "in range" reach.
constexpr double kPcBuffTargetRange = 8;
constexpr double kPcMinAbilityReach = 0.5;
constexpr double kPcSustainBelowRatio = 0.5;
constexpr double kPcHealBelowRatio = 0.7;
constexpr double kPcShieldBelowRatio = 0.85;

uint32_t PcAbilityColor(const PetDef& def, DamageType element) {
  if (element == DamageType::Fire) return kPcFireColor;
  if (element == DamageType::Arcane) return kPcArcaneColor;
  return def.color;
}

Vec2 PcDir(Vec2 d, Vec2 fallback) {
  const double len = JsHypot(d);
  return len > 0 ? Vec2(d.x / len, d.y / len) : fallback;
}

}  // namespace

// =====================================================================================================================
// pure decision (18.5.5)
// =====================================================================================================================

double PetDecisionContext::ReadyAt(std::string_view abilityId) const {
  for (const auto& [id, at] : readyAt) {
    if (id == abilityId) return at;
  }
  return 0;
}

bool PetAbilityUseful(const PetAbilityDef& a, const PetDecisionContext& ctx) {
  const bool inRange = ctx.hasTarget && ctx.targetDist <= (std::max)(a.range, kPcMinAbilityReach);
  switch (a.kind) {
    case PetAbilityKind::Heal: return ctx.heroHpRatio < kPcHealBelowRatio;
    case PetAbilityKind::Shield: return ctx.heroAttackers > 0 && ctx.heroHpRatio < kPcShieldBelowRatio;
    case PetAbilityKind::Taunt: return ctx.heroAttackers > 0;
    case PetAbilityKind::Buff: return ctx.hasTarget && ctx.targetDist <= kPcBuffTargetRange;
    case PetAbilityKind::Mark: return inRange && !ctx.targetMarked;
    case PetAbilityKind::Strike:
    case PetAbilityKind::Bolt: return inRange;
    case PetAbilityKind::Cone:
    case PetAbilityKind::Nova: return inRange && ctx.enemiesNearTarget >= 1;
    case PetAbilityKind::Revive: return false;
  }
  return false;
}

PetAction ChoosePetAction(const PetDecisionContext& ctx) {
  if (ctx.exhausted) return {PetActionKind::Rest, nullptr};
  if (ctx.peaceful || ctx.heroDist > ctx.leash) return {PetActionKind::Follow, nullptr};
  std::vector<const PetAbilityDef*> ready;
  for (const PetAbilityDef* a : ctx.abilities) {
    if (a != nullptr && a->kind != PetAbilityKind::Revive && ctx.ReadyAt(a->id) <= ctx.nowMs) ready.push_back(a);
  }
  // Sustain first when the hero is in trouble.
  if (ctx.heroHpRatio < kPcSustainBelowRatio) {
    for (const PetAbilityDef* a : ready) {
      const bool sustain =
          a->kind == PetAbilityKind::Heal || a->kind == PetAbilityKind::Shield || a->kind == PetAbilityKind::Taunt;
      if (sustain && PetAbilityUseful(*a, ctx)) return {PetActionKind::Ability, a};
    }
  }
  for (const PetAbilityDef* a : ready) {
    if (PetAbilityUseful(*a, ctx)) return {PetActionKind::Ability, a};
  }
  if (!ctx.hasTarget) return {PetActionKind::Follow, nullptr};
  if (ctx.targetDist <= ctx.basicRange) {
    return {ctx.basicReadyAt <= ctx.nowMs ? PetActionKind::Attack : PetActionKind::Rest, nullptr};
  }
  return {PetActionKind::Approach, nullptr};
}

bool ShouldBondRescue(const PetTables& t, int32_t bond, double heroHpRatio, double nowMs, double lastRescueAtMs) {
  return bond >= t.maxBond && heroHpRatio > 0 && heroHpRatio < t.system.bondRescueHp &&
         nowMs - lastRescueAtMs >= t.system.bondRescueCooldownMs;
}

std::string PetArtId(std::string_view petId, int32_t stage) {
  return stage <= 0 ? StrCat("beast_", petId) : StrCat("beast_", petId, "_e", stage);
}

std::string_view PetAbilityVfxId(const PetAbilityDef& a, DamageType element) {
  switch (a.kind) {
    case PetAbilityKind::Heal: return "life_regen";
    case PetAbilityKind::Shield: return "shield_wall";
    case PetAbilityKind::Buff: return "frenzy";
    case PetAbilityKind::Taunt: return "taunt_roar";
    case PetAbilityKind::Mark: return "death_mark";
    case PetAbilityKind::Strike:
      if (a.crit) return "backstab";
      if (a.hasBleed && a.bleed > 0) return "bleed_strike";
      return "slash";
    case PetAbilityKind::Bolt: return "combustion";
    case PetAbilityKind::Cone: return "combustion";
    case PetAbilityKind::Nova:
      if (a.self) return "war_stomp";
      return element == DamageType::Fire ? "combustion" : "arcane_torrent";
    case PetAbilityKind::Revive: return "combustion";
  }
  return "";
}

// =====================================================================================================================
// lifecycle
// =====================================================================================================================

PetCompanion::PetCompanion(SimContext& ctx) : ctx_(ctx) {}

void PetCompanion::OnZoneEnter() {
  CancelPending();
  state_ = PetCompanionState{};
  SpawnActive();
}

void PetCompanion::OnZoneExit() {
  CancelPending();
  state_ = PetCompanionState{};
}

void PetCompanion::OnPetChanged(const PetChangedMsg& m) {
  (void)m;
  if (ctx_.sys.zone == nullptr || !ctx_.sys.zone->HasZone() || ctx_.sys.pets == nullptr) return;
  Reconcile();
}

bool PetCompanion::Reconcile() {
  const PetInstance* active = ctx_.sys.pets != nullptr ? ctx_.sys.pets->Active() : nullptr;
  if (active == nullptr || ctx_.data.FindPet(active->petId) == nullptr) {
    if (state_.present) {
      Despawn(DespawnReason::Removed);
      CancelPending();
    }
    return false;
  }
  if (state_.present && state_.petId == active->petId && state_.stage == active->evolved) return true;
  Despawn(DespawnReason::Removed);
  SpawnActive();
  return state_.present;
}

void PetCompanion::Despawn(DespawnReason reason) {
  if (!state_.present) return;
  ctx_.events.Emit(EvEntityDespawned{state_.entity, EntityKind::Pet, reason});
  state_.present = false;
  state_.entity = kNoEntity;
  state_.leaping = false;
  state_.moving = false;
  state_.target = kNoEntity;
}

void PetCompanion::CancelPending() {
  ctx_.timers.CancelIf([](const Timer& t) { return t.owner == TimerOwner::Pets; });
  pending_.clear();
}

void PetCompanion::SpawnActive() {
  if (ctx_.sys.pets == nullptr || ctx_.sys.hero == nullptr || ctx_.sys.zone == nullptr || !ctx_.sys.zone->HasZone()) {
    return;
  }
  const PetInstance* active = ctx_.sys.pets->Active();
  const PetDef* def = active != nullptr ? ctx_.data.FindPet(active->petId) : nullptr;
  if (def == nullptr) return;
  const PetCompanionConstants& k = ctx_.data.Pets().companion;
  const Hero& hero = *ctx_.sys.hero;
  const Vec2 heroPos = hero.Position();
  Vec2 pos(heroPos.x + k.spawnOffsetCol, heroPos.y + k.spawnOffsetRow);  // 18.5.3
  const TilePos t = RoundToTile(pos);
  if (!ctx_.sys.zone->Walkable(t.col, t.row)) pos = heroPos;  // checked for flyers too
  state_.present = true;
  state_.entity = ctx_.ids.Next();
  state_.petId = active->petId;
  state_.stage = active->evolved;
  state_.pos = pos;
  state_.prevPos = pos;
  state_.maxHp = (std::max)(1.0, JsRound(hero.MaxHp() * def->hpFraction));
  state_.hp = state_.maxHp;
  state_.leaping = false;
  state_.moving = false;
  ctx_.events.Emit(EvEntitySpawned{state_.entity, EntityKind::Pet, state_.petId, PetArtId(state_.petId, state_.stage),
                                   pos, state_.facing, 1.0});
}

// =====================================================================================================================
// helpers
// =====================================================================================================================

const PetDef* PetCompanion::CurrentDef() const {
  return state_.petId.empty() ? nullptr : ctx_.data.FindPet(state_.petId);
}

double PetCompanion::HeroDamage() const {
  if (ctx_.sys.hero == nullptr) return 0;
  return ctx_.sys.hero->Derived().baseDamage + ctx_.equip.Get(Stat::Damage);
}

bool PetCompanion::MonsterAlive(EntityId monster) const {
  if (ctx_.sys.monsters == nullptr || monster == kNoEntity) return false;
  const MonsterInstance* m = ctx_.sys.monsters->Find(monster);
  return m != nullptr && m->IsAlive();
}

Vec2 PetCompanion::MonsterPos(EntityId monster) const {
  const MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(monster) : nullptr;
  return m != nullptr ? m->pos : state_.pos;
}

double PetCompanion::MonsterDist(EntityId monster) const { return JsHypot(MonsterPos(monster) - state_.pos); }

double PetCompanion::ReadyAt(std::string_view abilityId) const {
  for (const auto& [id, at] : state_.readyAtMs) {
    if (id == abilityId) return at;
  }
  return 0;
}

void PetCompanion::SetReadyAt(std::string_view abilityId, double ms) {
  for (auto& [id, at] : state_.readyAtMs) {
    if (id == abilityId) {
      at = ms;
      return;
    }
  }
  state_.readyAtMs.emplace_back(std::string(abilityId), ms);
}

double PetCompanion::Until(const std::vector<std::pair<EntityId, double>>& list, EntityId id) const {
  for (const auto& [m, until] : list) {
    if (m == id) return until;
  }
  return 0;
}

void PetCompanion::SetUntil(std::vector<std::pair<EntityId, double>>& list, EntityId id, double until) {
  for (auto& [m, u] : list) {
    if (m == id) {
      u = until;
      return;
    }
  }
  list.emplace_back(id, until);
}

bool PetCompanion::IsExhausted() const { return ctx_.Now() < state_.exhaustedUntilMs; }

// 18.5.4: the hero's attack target within 10 tiles of the hero, else the nearest aggro monster within 7 of the hero.
EntityId PetCompanion::PickTarget() const {
  const MonsterSystem* ms = ctx_.sys.monsters;
  if (ms == nullptr || ctx_.sys.hero == nullptr) return kNoEntity;
  const PetCompanionConstants& k = ctx_.data.Pets().companion;
  const Hero& hero = *ctx_.sys.hero;
  const Vec2 hp = hero.Position();
  if (hero.attackTarget != kNoEntity) {
    const MonsterInstance* m = ms->Find(hero.attackTarget);
    if (m != nullptr && m->IsAlive() && JsHypot(m->pos - hp) <= k.targetKeepRange) return m->id;
  }
  std::vector<EntityId> near;
  ms->QueryAlive(hp, k.targetScanRange, near);
  EntityId best = kNoEntity;
  double bestD = 1e300;
  for (EntityId id : near) {
    const MonsterInstance* m = ms->Find(id);
    if (m == nullptr || !m->IsAlive() || !m->IsAggro()) continue;
    const double d = JsHypot(m->pos - hp);
    if (d < bestD) {
      bestD = d;
      best = id;
    }
  }
  return best;
}

int32_t PetCompanion::CountHeroAttackers() const {
  const MonsterSystem* ms = ctx_.sys.monsters;
  if (ms == nullptr || ctx_.sys.hero == nullptr) return 0;
  std::vector<EntityId> near;
  ms->QueryAlive(ctx_.sys.hero->Position(), ctx_.data.Pets().companion.attackersRange, near);
  int32_t n = 0;
  for (EntityId id : near) {
    const MonsterInstance* m = ms->Find(id);
    if (m != nullptr && m->IsAlive() && m->state == MonsterState::Attack) ++n;
  }
  return n;
}

int32_t PetCompanion::CountNear(Vec2 centre, double radius) const {
  const MonsterSystem* ms = ctx_.sys.monsters;
  if (ms == nullptr) return 0;
  std::vector<EntityId> near;
  ms->QueryAlive(centre, radius, near);
  int32_t n = 0;
  for (EntityId id : near) n += MonsterAlive(id) ? 1 : 0;
  return n;
}

void PetCompanion::ClearMark(EntityId monster) {
  if (ctx_.sys.monsters == nullptr) return;
  if (MonsterInstance* m = ctx_.sys.monsters->Find(monster)) m->buffs.RemoveTag(BuffTag::PetMark);
}

void PetCompanion::PruneMarks(double now) {
  for (size_t i = 0; i < state_.marks.size();) {
    const auto [m, until] = state_.marks[i];
    if (until <= now || !MonsterAlive(m)) {
      ClearMark(m);
      state_.marks.erase(state_.marks.begin() + static_cast<std::ptrdiff_t>(i));
    } else {
      ++i;
    }
  }
}

double PetCompanion::MarkValue(EntityId monster) const {
  const MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(monster) : nullptr;
  if (m == nullptr) return 0;
  double v = 0;
  for (const ActiveBuff& b : m->buffs.Items()) {
    if (b.tag == BuffTag::PetMark) v = (std::max)(v, b.value);
  }
  return v;
}

void PetCompanion::Face(Vec2 delta) {
  if (delta.x == 0 && delta.y == 0) return;
  state_.facing = PcDir(delta, state_.facing);
}

int32_t PetCompanion::Schedule(double dueMs, PetTimerKind kind, PendingAction pa) {
  pa.key = nextKey_++;
  const int32_t key = pa.key;
  const EntityId target = pa.target;
  pending_.push_back(std::move(pa));
  ctx_.timers.Schedule(dueMs, TimerOwner::Pets, static_cast<uint16_t>(kind), target, state_.entity, key);
  return key;
}

PetCompanion::PendingAction PetCompanion::TakePending(int32_t key) {
  for (size_t i = 0; i < pending_.size(); ++i) {
    if (pending_[i].key != key) continue;
    PendingAction out = std::move(pending_[i]);
    pending_.erase(pending_.begin() + static_cast<std::ptrdiff_t>(i));
    return out;
  }
  return PendingAction{};
}

// =====================================================================================================================
// per step (18.5.2)
// =====================================================================================================================

void PetCompanion::Tick(double dtMs) {
  if (ctx_.sys.zone == nullptr || !ctx_.sys.zone->HasZone() || ctx_.sys.hero == nullptr || ctx_.sys.pets == nullptr) {
    return;
  }
  PetSystem& pets = *ctx_.sys.pets;
  if (!Reconcile()) return;
  const PetInstance* inst = pets.Active();
  const PetDef* def = ctx_.data.FindPet(inst->petId);
  if (def == nullptr) return;
  const PetCompanionConstants& k = ctx_.data.Pets().companion;
  const Hero& hero = *ctx_.sys.hero;
  const double now = ctx_.Now();
  state_.prevPos = state_.pos;
  state_.moving = false;
  PruneMarks(now);
  state_.maxHp = (std::max)(1.0, JsRound(hero.MaxHp() * def->hpFraction));  // follows the hero's max HP every step
  if (state_.hp > state_.maxHp) state_.hp = state_.maxHp;
  const bool paused = ctx_.session.transitioning || (ctx_.sys.story != nullptr && ctx_.sys.story->IsCinematic());
  if (paused) return;
  UpdateLeap(now);

  const bool exhausted = now < state_.exhaustedUntilMs;
  if (!exhausted && state_.exhaustedUntilMs > 0) Recover();
  const bool peaceful = ctx_.sys.zone->InSafeZone(hero.Position());
  if (!peaceful) pets.TickActive(dtMs);  // QP7: also while exhausted / with a dead hero
  inst = pets.Active();  // TickActive may publish (bond up) - re-read
  if (inst == nullptr || !state_.present) return;
  if (!exhausted) {
    const double rate = peaceful ? k.regenPeacefulPerSec : k.regenFieldPerSec;
    state_.hp = (std::min)(state_.maxHp, state_.hp + state_.maxHp * rate * dtMs / 1000.0);
  }

  const EntityId target = peaceful ? kNoEntity : PickTarget();
  const double heroDist = JsHypot(hero.Position() - state_.pos);
  const double heroHpRatio = hero.MaxHp() > 0 ? hero.Hp() / hero.MaxHp() : 1.0;
  const bool heroDead = hero.IsDead();

  // Max bond: the beast answers once when the hero is in danger.
  if (!exhausted && !heroDead && ShouldBondRescue(ctx_.data.Pets(), inst->bond, heroHpRatio, now, state_.lastRescueMs)) {
    if (const PetAbilityDef* sig = PrimaryPetAbility(*def)) {
      state_.lastRescueMs = now;
      ctx_.events.Log(MakeLoc("sys.pet.rescue", {PetNameArg("name", ctx_.data, inst->petId, inst->evolved)}),
                      LogType::Combat);
      UseAbility(*sig, target, now, *def, true);
    }
  }

  PetAction action{PetActionKind::Follow, nullptr};
  if (now >= state_.lockedUntilMs) {
    if (heroDead) {
      action = PetAction{exhausted ? PetActionKind::Rest : PetActionKind::Follow, nullptr};  // FIX QP1
    } else {
      PetDecisionContext dc;
      dc.nowMs = now;
      dc.abilities = UnlockedPetAbilities(*def, inst->evolved);
      dc.readyAt = state_.readyAtMs;
      dc.exhausted = exhausted;
      dc.peaceful = peaceful;
      dc.heroDist = heroDist;
      dc.heroHpRatio = heroHpRatio;
      dc.heroAttackers = CountHeroAttackers();
      dc.hasTarget = target != kNoEntity;
      dc.targetDist = dc.hasTarget ? MonsterDist(target) : 0;
      dc.targetMarked = dc.hasTarget && Until(state_.marks, target) > now;
      dc.enemiesNearTarget = dc.hasTarget ? CountNear(MonsterPos(target), k.nearTargetRange) : 0;
      dc.basicRange = def->range;
      dc.basicReadyAt = state_.basicReadyAtMs;
      dc.leash = k.leash;
      action = ChoosePetAction(dc);
    }
  } else {
    action = PetAction{PetActionKind::Rest, nullptr};
  }
  state_.lastAction = action.kind;

  switch (action.kind) {
    case PetActionKind::Ability:
      if (action.ability != nullptr) UseAbility(*action.ability, target, now, *def, false);
      break;
    case PetActionKind::Attack:
      if (target != kNoEntity) BasicAttack(target, now, *def);
      break;
    case PetActionKind::Approach:
      if (target != kNoEntity) {
        const double stop = (std::max)(k.approachStopMin, def->range * k.approachStopRangeFactor);
        MoveToward(MonsterPos(target), stop, dtMs, *def, k.followSpeedTilesPerSec * k.approachSpeedMul);
      }
      break;
    case PetActionKind::Follow:
      Follow(dtMs, *def, exhausted);
      break;
    case PetActionKind::Rest:
      if (exhausted) Follow(dtMs, *def, true);
      break;
  }
  if (target != kNoEntity && state_.present && now >= state_.lockedUntilMs && action.kind != PetActionKind::Follow) {
    Face(MonsterPos(target) - state_.pos);
  }
  state_.target = target;
}

void PetCompanion::UpdateLeap(double now) {
  if (!state_.leaping) return;
  const double u = state_.leapDurationMs > 0 ? Clamp((now - state_.leapStartMs) / state_.leapDurationMs, 0.0, 1.0) : 1.0;
  const double e = 1.0 - (1.0 - u) * (1.0 - u);  // Quad.easeOut
  state_.pos = state_.leapFrom + (state_.leapTo - state_.leapFrom) * e;
  if (u >= 1.0) state_.leaping = false;
}

// 18.5.6
void PetCompanion::Follow(double dtMs, const PetDef& def, bool exhausted) {
  const PetCompanionConstants& k = ctx_.data.Pets().companion;
  const Vec2 heroPos = ctx_.sys.hero->Position();
  const Vec2 goal(heroPos.x + k.followOffsetCol, heroPos.y + k.followOffsetRow);
  const double d = JsHypot(goal - state_.pos);
  if (d > k.teleportDistanceTiles) {
    const Vec2 from = state_.pos;
    state_.pos = heroPos;
    state_.prevPos = heroPos;
    ctx_.events.Emit(EvEntityTeleported{state_.entity, from, heroPos, TeleportReason::CatchUp});
    return;
  }
  const double speed = d > k.dashOverTiles ? k.dashSpeedTilesPerSec : k.followSpeedTilesPerSec;
  MoveToward(goal, k.followStop, dtMs, def, exhausted ? speed * k.exhaustedSpeedMul : speed);
}

bool PetCompanion::MoveToward(Vec2 goal, double stopAt, double dtMs, const PetDef& def, double speed) {
  const PetCompanionConstants& k = ctx_.data.Pets().companion;
  const Vec2 delta = goal - state_.pos;
  const double d = JsHypot(delta);
  if (d <= stopAt) return true;
  const double step = (std::min)(d - stopAt, speed * dtMs / 1000.0);
  const Vec2 n(state_.pos.x + (delta.x / d) * step, state_.pos.y + (delta.y / d) * step);
  const ZoneRuntime& zone = *ctx_.sys.zone;
  auto walk = [&zone](double c, double r) { return zone.Walkable(JsRoundInt(c), JsRoundInt(r)); };
  if (def.flying || walk(n.x, n.y)) {
    state_.pos = n;
  } else if (walk(n.x, state_.pos.y)) {
    state_.pos.x = n.x;
  } else if (walk(state_.pos.x, n.y)) {
    state_.pos.y = n.y;
  } else if (d > k.stuckHopDistance) {
    // Stuck behind something: hop back to the hero.
    const Vec2 from = state_.pos;
    state_.pos = ctx_.sys.hero->Position();
    state_.prevPos = state_.pos;
    ctx_.events.Emit(EvEntityTeleported{state_.entity, from, state_.pos, TeleportReason::CatchUp});
  }
  Face(delta);
  state_.moving = true;
  return false;
}

// =====================================================================================================================
// damage (18.5.8)
// =====================================================================================================================

int32_t PetCompanion::Hit(EntityId target, double mult, bool forceCrit, DamageType element, uint32_t color) {
  if (!MonsterAlive(target) || ctx_.sys.pets == nullptr) return 0;
  const PetCompanionConstants& k = ctx_.data.Pets().companion;
  const int32_t base = ctx_.sys.pets->PetDamage(HeroDamage());
  if (base <= 0) return 0;  // nothing lands, no number, no VFX (QP3)
  const bool isCrit = forceCrit || ctx_.Rand(RngStream::Pets).Float01() < k.critChance;
  const double now = ctx_.Now();
  const double amp = Until(state_.marks, target) > now ? MarkValue(target) : 0.0;
  const double dmg =
      (std::max)(1.0, JsRound(static_cast<double>(base) * mult * (isCrit ? k.critMul : 1.0) * (1.0 + amp)));
  MonsterHitRequest r;
  r.monster = target;
  r.amount = dmg;
  r.isCrit = isCrit;
  r.hasFrom = state_.present;
  r.from = state_.pos;
  r.attacker = state_.entity;
  r.source = KillSource::Pet;
  r.element = element;
  r.impactColor = color;
  r.impactBurst = false;  // combat contract: pets get the profile through EvHit, no hero-style burst / shake
  r.provokes = false;     // 18.5.4: pet hits never aggro a monster
  if (ctx_.sys.combat != nullptr) {
    ctx_.sys.combat->DamageMonster(r);
  } else if (ctx_.sys.monsters != nullptr) {
    DamageFlags f;
    f.isCrit = isCrit;
    f.hasFrom = r.hasFrom;
    f.from = r.from;
    f.attacker = r.attacker;
    f.source = KillSource::Pet;
    f.provokes = false;
    ctx_.sys.monsters->ApplyDamage(target, dmg, f);
  }
  return static_cast<int32_t>(dmg);
}

// 18.5.7
void PetCompanion::BasicAttack(EntityId target, double now, const PetDef& def) {
  const PetCompanionConstants& k = ctx_.data.Pets().companion;
  const std::string art = PetArtId(state_.petId, state_.stage);
  state_.basicReadyAtMs = now + def.attackMs;
  const Vec2 tpos = MonsterPos(target);
  Face(tpos - state_.pos);
  EvPlayAnim anim;
  anim.entity = state_.entity;
  anim.startMs = now;
  anim.hasFaceTarget = true;
  anim.faceTarget = tpos;
  PendingAction pa;
  pa.petId = def.id;
  pa.ability = -1;
  pa.target = target;
  if (def.style == PetCombatStyle::Melee) {
    const ActionTiming t =
        ComputeAttackTiming(ctx_.data.Combat().anim, ctx_.data.Assets(), art, def.animCategory, def.attackMs);
    anim.action = AnimAction::Attack;
    anim.clip = "Attack01";
    anim.contactMs = t.contactMs;
    anim.windupMs = 0;
    anim.durationMs = t.durationMs;
    anim.playRate = t.playRate;
    ctx_.events.Emit(anim);
    state_.lockedUntilMs = now + t.contactMs + k.lockAfterMeleeMs;
    Schedule(now + t.contactMs, PetTimerKind::BasicContact, std::move(pa));
  } else {
    const ActionTiming t = ComputeCastTiming(ctx_.data.Combat().anim, ctx_.data.Assets(), art, def.animCategory);
    anim.action = AnimAction::Cast;
    anim.clip = "Cast01";
    anim.contactMs = t.contactMs;
    anim.durationMs = t.durationMs;
    anim.playRate = t.playRate;
    ctx_.events.Emit(anim);
    state_.lockedUntilMs = now + t.contactMs + k.lockAfterRangedMs;
    Schedule(now + t.contactMs, PetTimerKind::BasicRelease, std::move(pa));
  }
}

// 18.5.11
void PetCompanion::UseAbility(const PetAbilityDef& a, EntityId target, double now, const PetDef& def, bool forced) {
  const PetCompanionConstants& k = ctx_.data.Pets().companion;
  SetReadyAt(a.id, now + a.cooldownMs);  // at decision time
  const std::string art = PetArtId(state_.petId, state_.stage);
  const ActionTiming t = ComputeCastTiming(ctx_.data.Combat().anim, ctx_.data.Assets(), art, def.animCategory);
  const double castMs = t.contactMs;
  EvPlayAnim anim;
  anim.entity = state_.entity;
  anim.action = AnimAction::Cast;
  anim.clip = "Cast01";
  anim.startMs = now;
  anim.contactMs = castMs;
  anim.durationMs = t.durationMs;
  anim.playRate = t.playRate;
  if (target != kNoEntity) {
    anim.hasFaceTarget = true;
    anim.faceTarget = MonsterPos(target);
    Face(anim.faceTarget - state_.pos);
  }
  ctx_.events.Emit(anim);
  state_.lockedUntilMs = now + castMs + k.lockAfterAbilityMs;

  int32_t index = -1;
  for (size_t i = 0; i < def.abilities.size(); ++i) {
    if (&def.abilities[i] == &a) index = static_cast<int32_t>(i);
  }
  PendingAction pa;
  pa.petId = def.id;
  pa.ability = index;
  pa.target = target;

  switch (a.kind) {
    case PetAbilityKind::Heal:
    case PetAbilityKind::Shield:
    case PetAbilityKind::Buff:
    case PetAbilityKind::Taunt:
      pa.target = kNoEntity;
      Schedule(now + castMs, PetTimerKind::AbilityRelease, std::move(pa));
      break;
    case PetAbilityKind::Mark:
    case PetAbilityKind::Bolt:
    case PetAbilityKind::Cone:
      if (target == kNoEntity) break;
      Schedule(now + castMs, PetTimerKind::AbilityRelease, std::move(pa));
      break;
    case PetAbilityKind::Nova:
      if (a.self) pa.target = kNoEntity;  // centre: the pet when self or without a target
      Schedule(now + castMs, PetTimerKind::AbilityRelease, std::move(pa));
      break;
    case PetAbilityKind::Strike: {
      if (target == kNoEntity) break;
      if (a.leap) {
        // Leap beside the target before the blow lands (tween over max(120, castMs), Quad.easeOut).
        const Vec2 tpos = MonsterPos(target);
        double d = JsHypot(tpos - state_.pos);
        if (!(d > 0)) d = 1;
        const Vec2 dir((tpos.x - state_.pos.x) / d, (tpos.y - state_.pos.y) / d);
        state_.leaping = true;
        state_.leapFrom = state_.pos;
        state_.leapTo = Vec2(tpos.x - dir.x * k.leapStopShort, tpos.y - dir.y * k.leapStopShort);
        state_.leapStartMs = now;
        state_.leapDurationMs = (std::max)(k.leapMinMs, castMs);
        EvSkillVfx fx;
        fx.skillId = a.id;
        fx.vfxId = "charge";
        fx.caster = state_.entity;
        fx.target = target;
        fx.origin = state_.pos;
        fx.point = tpos;
        ctx_.events.Emit(std::move(fx));
      }
      const int32_t hits = (std::max)(1, a.hits);
      for (int32_t i = 0; i < hits; ++i) {
        PendingAction hp = pa;
        hp.hit = i;
        Schedule(now + castMs + static_cast<double>(i) * k.strikeHitSpacingMs, PetTimerKind::StrikeHit, std::move(hp));
      }
      state_.lockedUntilMs = now + castMs + static_cast<double>(hits) * k.strikeHitSpacingMs + k.lockAfterAbilityMs;
      break;
    }
    case PetAbilityKind::Revive:
      break;
  }
  if (forced) state_.lockedUntilMs = (std::max)(state_.lockedUntilMs, now + k.forcedLockMs);
}

void PetCompanion::HeroBuff(BuffStat stat, double value, double durationMs, BuffTag tag) {
  if (ctx_.sys.hero == nullptr) return;
  BuffList& buffs = ctx_.sys.hero->Buffs();
  buffs.RemoveTag(tag);
  ActiveBuff b;
  b.stat = stat;
  b.value = value;
  b.durationMs = durationMs;
  b.startMs = ctx_.Now();
  b.tag = tag;
  b.source = state_.entity;
  buffs.Add(b);
}

void PetCompanion::ApplyStatus(EntityId target, StatusType type, double value, double durationMs) {
  if (ctx_.sys.status == nullptr) return;
  const StatusApplyResult r = ctx_.sys.status->Apply(target, type, value, durationMs, state_.entity, ctx_.Now());
  const I18nArg name = KeyArg("effectName", "sys.statusEffect.name." + std::string(EnumName(type)));
  if (r.outcome == StatusApplyOutcome::Applied) {
    ctx_.events.Emit(EvStatusApplied{target, type, value, r.effectiveDurationMs, false});
    ctx_.events.Log(MakeLoc("sys.statusEffect.applied", {name}), LogType::Combat);
  } else if (r.outcome == StatusApplyOutcome::Refreshed) {
    ctx_.events.Emit(EvStatusApplied{target, type, value, r.effectiveDurationMs, true});
    if (type == StatusType::Poison) ctx_.events.Log(MakeLoc("sys.statusEffect.refreshed", {name}), LogType::Combat);
  }
}

void PetCompanion::EmitAbilityVfx(const PetAbilityDef& a, DamageType element, EntityId target, Vec2 point,
                                  double radius) {
  EvSkillVfx fx;
  fx.skillId = a.id;
  fx.vfxId = std::string(PetAbilityVfxId(a, element));
  fx.caster = state_.entity;
  fx.target = target;
  fx.origin = state_.pos;
  fx.point = point;
  fx.radius = radius;
  ctx_.events.Emit(std::move(fx));
}

// =====================================================================================================================
// timers / projectiles
// =====================================================================================================================

void PetCompanion::OnTimer(const Timer& t) {
  switch (static_cast<PetTimerKind>(t.kind)) {
    case PetTimerKind::TakeHit:
      TakeHit(t.entity);
      return;
    case PetTimerKind::BasicRelease: {
      const PendingAction pa = TakePending(t.param);
      if (pa.key == 0 || !state_.present || !MonsterAlive(pa.target)) return;
      const PetDef* def = ctx_.data.FindPet(pa.petId);
      if (def == nullptr) return;
      LaunchBolt(pa, pa.target, def->color);
      return;
    }
    case PetTimerKind::BasicContact: {
      const PendingAction pa = TakePending(t.param);
      if (pa.key == 0 || !MonsterAlive(pa.target)) return;
      const PetDef* def = ctx_.data.FindPet(pa.petId);
      if (def == nullptr) return;
      Hit(pa.target, 1.0, false, def->element, def->color);
      EvSkillVfx fx;
      fx.vfxId = "slash";
      fx.caster = state_.entity;
      fx.target = pa.target;
      fx.origin = state_.pos;
      fx.point = MonsterPos(pa.target);
      ctx_.events.Emit(std::move(fx));
      return;
    }
    case PetTimerKind::AbilityRelease: {
      const PendingAction pa = TakePending(t.param);
      if (pa.key == 0) return;
      ResolveAbility(pa);
      return;
    }
    case PetTimerKind::StrikeHit: {
      const PendingAction pa = TakePending(t.param);
      if (pa.key == 0) return;
      ResolveStrikeHit(pa);
      return;
    }
  }
}

void PetCompanion::LaunchBolt(const PendingAction& pa, EntityId target, uint32_t color) {
  if (ctx_.sys.projectiles == nullptr) return;
  PendingAction carry = pa;
  carry.key = nextKey_++;
  carry.target = target;
  const Vec2 to = MonsterPos(target);
  ProjectileSpec spec;
  spec.kind = ProjectileKind::PetBolt;
  spec.source = state_.entity;
  spec.target = target;
  spec.from = state_.pos;
  spec.to = to;
  spec.travelMs = MonsterBoltTravelMs(ctx_.data.Combat().projectiles, state_.pos, to);
  spec.vfxId = "pet_bolt";
  spec.color = color;
  spec.skillIndex = pa.ability;
  spec.payload = carry.key;
  pending_.push_back(std::move(carry));
  ctx_.sys.projectiles->Launch(spec);
}

void PetCompanion::OnBoltArrived(const Projectile& p) {
  const PendingAction pa = TakePending(p.spec.payload);
  if (pa.key == 0) return;
  const PetDef* def = ctx_.data.FindPet(pa.petId);
  if (def == nullptr) return;
  if (pa.ability < 0) {
    if (MonsterAlive(pa.target)) Hit(pa.target, 1.0, false, def->element, def->color);
    return;
  }
  ResolveBolt(pa, pa.target);
}

void PetCompanion::ResolveBolt(const PendingAction& pa, EntityId target) {
  const PetDef* def = ctx_.data.FindPet(pa.petId);
  if (def == nullptr || pa.ability < 0 || static_cast<size_t>(pa.ability) >= def->abilities.size()) return;
  const PetAbilityDef& a = def->abilities[static_cast<size_t>(pa.ability)];
  const DamageType element = a.hasElement ? a.element : def->element;
  const uint32_t color = PcAbilityColor(*def, element);
  const double damage = a.hasDamage ? a.damage : kPcBoltDamage;
  if (a.hasRadius && a.radius != 0 && ctx_.sys.monsters != nullptr) {
    const Vec2 centre = MonsterPos(target);
    std::vector<EntityId> near;
    ctx_.sys.monsters->QueryAlive(centre, a.radius, near);
    for (EntityId m : near) {
      if (m != target && MonsterAlive(m)) Hit(m, damage * kPcBoltSplashShare, false, element, color);
    }
    EmitAbilityVfx(a, element, target, centre, a.radius);
  }
  if (MonsterAlive(target)) Hit(target, damage, false, element, color);
  if (a.hasMana && a.mana != 0 && ctx_.sys.hero != nullptr) {
    Hero& hero = *ctx_.sys.hero;
    const double gain = (std::max)(1.0, std::floor(hero.MaxMana() * a.mana));
    hero.RestoreMana(gain);
    EvFloatingText ft;
    ft.kind = FloatingTextKind::Custom;  // mana +N (#6fb6ff): rendered from `value` with the arcane tint
    ft.anchor = kHeroEntityId;
    ft.pos = hero.Position();
    ft.value = gain;
    ft.element = DamageType::Arcane;
    ctx_.events.Emit(std::move(ft));
  }
}

void PetCompanion::ResolveStrikeHit(const PendingAction& pa) {
  const PetDef* def = ctx_.data.FindPet(pa.petId);
  if (def == nullptr || pa.ability < 0 || static_cast<size_t>(pa.ability) >= def->abilities.size()) return;
  if (!MonsterAlive(pa.target)) return;
  const PetAbilityDef& a = def->abilities[static_cast<size_t>(pa.ability)];
  const DamageType element = a.hasElement ? a.element : def->element;
  const uint32_t color = PcAbilityColor(*def, element);
  const int32_t dmg = Hit(pa.target, a.hasDamage ? a.damage : kPcStrikeDamage, a.crit, element, color);
  EmitAbilityVfx(a, element, pa.target, MonsterPos(pa.target), 0);
  if (a.hasBleed && a.bleed != 0 && MonsterAlive(pa.target) && dmg > 0) {
    ApplyStatus(pa.target, StatusType::Bleed, (std::max)(1.0, JsRound(static_cast<double>(dmg) * a.bleed)),
                a.hasDuration ? a.durationMs : kPcBleedMs);
  }
}

void PetCompanion::ResolveAbility(const PendingAction& pa) {
  const PetDef* def = ctx_.data.FindPet(pa.petId);
  if (def == nullptr || pa.ability < 0 || static_cast<size_t>(pa.ability) >= def->abilities.size()) return;
  const PetAbilityDef& a = def->abilities[static_cast<size_t>(pa.ability)];
  const DamageType element = a.hasElement ? a.element : def->element;
  const uint32_t color = PcAbilityColor(*def, element);
  const double now = ctx_.Now();
  Hero* hero = ctx_.sys.hero;
  if (hero == nullptr) return;
  switch (a.kind) {
    case PetAbilityKind::Heal: {
      if (hero->IsDead()) return;  // FIX QP1: never heal a dead hero
      const double amount = (std::max)(1.0, std::floor(hero->MaxHp() * (a.hasValue ? a.value : kPcHealValue)));
      hero->Heal(amount);
      EvFloatingText ft;
      ft.kind = FloatingTextKind::Heal;
      ft.anchor = kHeroEntityId;
      ft.pos = hero->Position();
      ft.value = amount;
      ctx_.events.Emit(std::move(ft));
      EmitAbilityVfx(a, element, kHeroEntityId, hero->Position(), 0);
      return;
    }
    case PetAbilityKind::Shield:
      HeroBuff(BuffStat::DamageReduction, a.hasValue ? a.value : kPcShieldValue,
               a.hasDuration ? a.durationMs : kPcShieldMs, BuffTag::PetShield);
      EmitAbilityVfx(a, element, kHeroEntityId, hero->Position(), 0);
      return;
    case PetAbilityKind::Buff:
      HeroBuff(BuffStat::DamageBonus, a.hasValue ? a.value : kPcBuffValue, a.hasDuration ? a.durationMs : kPcBuffMs,
               BuffTag::PetHowl);
      EmitAbilityVfx(a, element, kHeroEntityId, hero->Position(), 0);
      return;
    case PetAbilityKind::Taunt: {
      const double dur = a.hasDuration ? a.durationMs : kPcTauntMs;
      const double until = now + dur;
      if (ctx_.sys.monsters != nullptr && state_.present) {
        std::vector<EntityId> near;
        const TilePos c = RoundToTile(state_.pos);
        ctx_.sys.monsters->QueryAlive(c.Center(), a.hasRadius ? a.radius : kPcTauntRadius, near);
        for (EntityId m : near) {
          if (MonsterAlive(m)) SetUntil(state_.taunts, m, until);
        }
      }
      HeroBuff(BuffStat::DamageReduction, a.hasValue ? a.value : kPcTauntValue, dur, BuffTag::PetShield);
      EmitAbilityVfx(a, element, state_.entity, state_.pos, a.hasRadius ? a.radius : kPcTauntRadius);
      return;
    }
    case PetAbilityKind::Mark: {
      if (!MonsterAlive(pa.target) || ctx_.sys.monsters == nullptr) return;
      const double dur = a.hasDuration ? a.durationMs : kPcMarkMs;
      ClearMark(pa.target);
      if (MonsterInstance* m = ctx_.sys.monsters->Find(pa.target)) {
        ActiveBuff b;
        b.stat = BuffStat::DamageAmplify;
        b.value = a.hasValue ? a.value : kPcMarkValue;
        b.durationMs = dur;
        b.startMs = now;
        b.tag = BuffTag::PetMark;
        b.source = state_.entity;
        m->buffs.Add(b);
      }
      SetUntil(state_.marks, pa.target, now + dur);
      EmitAbilityVfx(a, element, pa.target, MonsterPos(pa.target), 0);
      return;
    }
    case PetAbilityKind::Bolt:
      if (!MonsterAlive(pa.target) || !state_.present) return;
      LaunchBolt(pa, pa.target, color);
      return;
    case PetAbilityKind::Cone: {
      if (ctx_.sys.monsters == nullptr) return;
      const Vec2 o = state_.pos;
      const Vec2 tpos = MonsterPos(pa.target);
      const double aim = std::atan2(tpos.y - o.y, tpos.x - o.x);
      const double half = (a.hasArc ? a.arc : kPcConeArc) / 2.0;
      const double len = a.hasRadius ? a.radius : kPcConeRadius;
      std::vector<EntityId> near;
      ctx_.sys.monsters->QueryAlive(RoundToTile(o).Center(), len + 1.0, near);
      for (EntityId id : near) {
        if (!MonsterAlive(id)) continue;
        const Vec2 d = MonsterPos(id) - o;
        const double dist = JsHypot(d);
        if (dist > len) continue;
        double da = std::fabs(std::atan2(d.y, d.x) - aim);
        if (da > kPi) da = kTwoPi - da;
        if (dist > kPcConeOriginReach && da > half) continue;
        const int32_t dmg = Hit(id, a.hasDamage ? a.damage : kPcConeDamage, false, element, color);
        if (a.hasBurn && a.burn != 0 && dmg > 0) {
          ApplyStatus(id, StatusType::Burn, (std::max)(1.0, JsRound(static_cast<double>(dmg) * a.burn)),
                      a.hasDuration ? a.durationMs : kPcDotMs);
        }
      }
      EmitAbilityVfx(a, element, pa.target, tpos, len);
      return;
    }
    case PetAbilityKind::Nova: {
      if (ctx_.sys.monsters == nullptr) return;
      const bool onTarget = pa.target != kNoEntity && ctx_.sys.monsters->Find(pa.target) != nullptr;
      const Vec2 c = onTarget ? MonsterPos(pa.target) : state_.pos;
      const double radius = a.hasRadius ? a.radius : kPcNovaRadius;
      EmitAbilityVfx(a, element, onTarget ? pa.target : state_.entity, c, radius);
      std::vector<EntityId> near;
      ctx_.sys.monsters->QueryAlive(RoundToTile(c).Center(), radius + 1.0, near);
      for (EntityId id : near) {
        if (!MonsterAlive(id) || JsHypot(MonsterPos(id) - c) > radius) continue;
        const int32_t dmg = Hit(id, a.hasDamage ? a.damage : kPcNovaDamage, false, element, color);
        if (!MonsterAlive(id)) continue;
        if (a.hasStun && a.stunMs != 0) ApplyStatus(id, StatusType::Stun, 1, a.stunMs);
        if (a.hasSlow && a.slow != 0) ApplyStatus(id, StatusType::Slow, a.slow, a.hasDuration ? a.durationMs : kPcDotMs);
        if (a.hasBurn && a.burn != 0 && dmg > 0) {
          ApplyStatus(id, StatusType::Burn, (std::max)(1.0, JsRound(static_cast<double>(dmg) * a.burn)),
                      a.hasDuration ? a.durationMs : kPcDotMs);
        }
      }
      return;
    }
    case PetAbilityKind::Strike:
    case PetAbilityKind::Revive:
      return;
  }
}

// =====================================================================================================================
// hits on the beast, exhaustion (18.5.9-18.5.10)
// =====================================================================================================================

bool PetCompanion::InterceptMonsterAttack(EntityId monster) {
  if (!state_.present || IsExhausted() || ctx_.sys.pets == nullptr || ctx_.sys.pets->Active() == nullptr ||
      ctx_.sys.monsters == nullptr || ctx_.sys.hero == nullptr) {
    return false;
  }
  MonsterInstance* m = ctx_.sys.monsters->Find(monster);
  if (m == nullptr || !m->IsAlive()) return false;
  const PetCompanionConstants& k = ctx_.data.Pets().companion;
  const double now = ctx_.Now();
  bool redirect = Until(state_.taunts, monster) > now;
  if (!redirect) {
    const double reach = m->def.attackRange + k.straySwingReachPad;
    const double dPet = JsHypot(m->pos - state_.pos);
    const double dHero = JsHypot(m->pos - ctx_.sys.hero->Position());
    // The draw happens only when both distance tests pass (JS short-circuit).
    redirect = dPet <= reach && dPet < dHero && ctx_.Rand(RngStream::Pets).Float01() < k.straySwingChance;
  }
  if (!redirect) return false;
  m->lastAttackMs = now;
  const ActionTiming t = ComputeAttackTiming(ctx_.data.Combat().anim, ctx_.data.Assets(), m->def.spriteKey,
                                             m->def.animCategory, m->def.attackSpeedMs);
  EvPlayAnim anim;
  anim.entity = monster;
  anim.action = AnimAction::Attack;
  anim.clip = "Attack01";
  anim.startMs = now;
  anim.contactMs = t.contactMs;
  anim.windupMs = t.windupMs;
  anim.durationMs = t.durationMs;
  anim.playRate = t.playRate;
  anim.hasFaceTarget = true;
  anim.faceTarget = state_.pos;
  ctx_.events.Emit(anim);
  ctx_.timers.Schedule(now + t.contactMs, TimerOwner::Pets, static_cast<uint16_t>(PetTimerKind::TakeHit), monster,
                       state_.entity);
  return true;
}

void PetCompanion::TakeHit(EntityId monster) {
  if (!state_.present || !MonsterAlive(monster) || IsExhausted()) return;
  const MonsterInstance* m = ctx_.sys.monsters->Find(monster);
  const PetCompanionConstants& k = ctx_.data.Pets().companion;
  const double raw = m->def.damage * (k.takeHitJitterMin + ctx_.Rand(RngStream::Pets).Float01() * k.takeHitJitterSpan);
  const double dmg = (std::max)(1.0, JsRound(raw * k.takeHitMul));
  state_.hp -= dmg;
  const HitFeedbackTable& hf = ctx_.data.Combat().hitFeedback;
  EvHit hit;
  hit.target = state_.entity;
  hit.source = monster;
  hit.targetFaction = Faction::Hero;
  hit.amount = dmg;
  hit.weight = HitWeight::Normal;
  hit.profile = hf.Profile(HitWeight::Normal);
  hit.hasFrom = true;
  hit.from = m->pos;
  hit.targetHp = (std::max)(0.0, state_.hp);
  hit.targetMaxHp = state_.maxHp;
  hit.melee = true;  // the web plays the claw VFX on the beast for every swing (no projectile)
  ctx_.events.Emit(hit);
  EvFloatingText ft;
  ft.kind = FloatingTextKind::HeroDamage;
  ft.anchor = state_.entity;
  ft.pos = state_.pos;
  ft.value = dmg;
  ctx_.events.Emit(std::move(ft));
  EvPlayAnim anim;
  anim.entity = state_.entity;
  anim.action = AnimAction::Hurt;
  anim.startMs = ctx_.Now();
  anim.hasFaceTarget = true;
  anim.faceTarget = m->pos;
  ctx_.events.Emit(anim);
  if (state_.hp <= 0) Exhaust();
}

void PetCompanion::Exhaust() {
  const double now = ctx_.Now();
  state_.hp = 0;
  state_.exhaustedUntilMs = now + ctx_.data.Pets().companion.exhaustMs;
  state_.lockedUntilMs = 0;
  for (auto& [m, until] : state_.taunts) until = 0;
  ctx_.events.Emit(EvPet{EvPet::Kind::Exhausted, state_.petId, false});
  if (ctx_.sys.pets != nullptr) {
    if (const PetInstance* inst = ctx_.sys.pets->Active()) {
      ctx_.events.Log(MakeLoc("sys.pet.exhausted", {PetNameArg("name", ctx_.data, inst->petId, inst->evolved)}),
                      LogType::Combat);
    }
  }
}

void PetCompanion::Recover() {
  state_.hp = state_.maxHp;
  state_.exhaustedUntilMs = 0;
  ctx_.events.Emit(EvPet{EvPet::Kind::Recovered, state_.petId, false});
}

// =====================================================================================================================
// revive, feeding
// =====================================================================================================================

bool PetCompanion::TryReviveHero() {
  if (ctx_.sys.pets == nullptr || ctx_.sys.hero == nullptr) return false;
  const PetInstance* inst = ctx_.sys.pets->Active();
  const PetDef* def = ctx_.sys.pets->ActiveDef();
  if (inst == nullptr || def == nullptr || state_.reviveUsed) return false;
  const PetAbilityDef* revive = nullptr;
  for (const PetAbilityDef* a : UnlockedPetAbilities(*def, inst->evolved)) {
    if (a->kind == PetAbilityKind::Revive) {
      revive = a;
      break;
    }
  }
  if (revive == nullptr) return false;
  state_.reviveUsed = true;
  Hero& hero = *ctx_.sys.hero;
  const double hp = (std::max)(1.0, std::floor(hero.MaxHp() * (revive->hasValue ? revive->value : kPcReviveValue)));
  hero.SetHp(hp);
  ctx_.events.Log(MakeLoc("sys.pet.revive", {PetNameArg("name", ctx_.data, inst->petId, inst->evolved)}),
                  LogType::System);
  ctx_.events.Emit(EvCameraFlash{kPcReviveFlashColor, kPcReviveFlashMs, kPcReviveFlashAlpha});
  EmitAbilityVfx(*revive, revive->hasElement ? revive->element : def->element, kHeroEntityId, hero.Position(), 0);
  EvFloatingText ft;
  ft.kind = FloatingTextKind::Heal;
  ft.anchor = kHeroEntityId;
  ft.pos = hero.Position();
  ft.value = hp;
  ctx_.events.Emit(std::move(ft));
  return true;
}

bool PetCompanion::FeedFromBag(std::string_view petId) {
  if (ctx_.sys.pets == nullptr || ctx_.sys.inventory == nullptr) return false;
  PetSystem& pets = *ctx_.sys.pets;
  InventorySystem& inv = *ctx_.sys.inventory;
  const std::string& fruitId = ctx_.data.Pets().leyFruitId;
  std::string uid;
  for (const ItemInstance& it : inv.Items().Bag()) {
    if (it.baseId == fruitId) {
      uid = it.uid;
      break;
    }
  }
  if (uid.empty()) {
    ctx_.events.Log(MakeLoc("sys.pet.noFruit"), LogType::System);
    return false;
  }
  if (!pets.CanFeed(petId)) {
    if (const PetInstance* inst = pets.Find(petId)) {
      ctx_.events.Log(MakeLoc("sys.pet.feedFull", {PetNameArg("name", ctx_.data, inst->petId, inst->evolved)}),
                      LogType::System);
    }
    return false;
  }
  inv.Items().RemoveItem(uid, 1, &inv.Uids());
  pets.Feed(petId);
  ctx_.events.Emit(EvInventoryChanged{});
  return true;
}

// =====================================================================================================================
// snapshot
// =====================================================================================================================

void PetCompanion::FillSnapshot(Snapshot& out) const {
  out.pet.present = state_.present;
  out.pet.id = state_.entity;
  out.pet.petId = state_.petId;
  out.pet.stage = state_.stage;
  out.pet.pos = state_.pos;
  out.pet.prevPos = state_.prevPos;
  out.pet.facing = state_.facing;
  out.pet.hp = state_.hp;
  out.pet.maxHp = state_.maxHp;
  out.pet.exhausted = state_.exhaustedUntilMs > ctx_.Now();
  out.pet.target = state_.target;
}

}  // namespace abyss
