// Hero locomotion (world-map-nav.md 6-7; S5, C4, C5, C7, W3, D13 F1). STUB: owner area world. Teleport, Stop, the
// screen-to-tile mapping, the C4 dash and the approach re-goal bookkeeping are implemented; path finding / following,
// hold-move and direct movement are stubs.
#include "abyss/base/Platform.h"

#include "abyss/world/Locomotion.h"

#include "abyss/base/Assert.h"
#include "abyss/base/Math.h"
#include "abyss/combat/StatusEffects.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

Vec2 ScreenDirToTile(Vec2 screen) { return {screen.x + screen.y, -screen.x + screen.y}; }

HeroLocomotion::HeroLocomotion(SimContext& ctx) : ctx_(ctx) {}

bool HeroLocomotion::MoveTo(Vec2 goal, double stopRange) {
  approach_ = ApproachState{};
  return PathTo(goal, stopRange);
}

bool HeroLocomotion::PathTo(Vec2 goal, double stopRange) {
  ABYSS_UNIMPLEMENTED();
  goal_ = goal;
  stopRange_ = stopRange;
  return false;
}

bool HeroLocomotion::Approach(EntityId target, double stopRange) {
  const MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(target) : nullptr;
  if (m == nullptr || !m->IsAlive()) return false;
  approach_.active = true;
  approach_.target = target;
  approach_.stopRange = stopRange;
  approach_.repathAtMs = ctx_.Now();  // first re-goal this step
  TickApproach();
  return approach_.active;
}

void HeroLocomotion::CancelApproach() { approach_ = ApproachState{}; }

void HeroLocomotion::TickApproach() {
  if (!approach_.active) return;
  const MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(approach_.target) : nullptr;
  const Vec2 hero = ctx_.sys.hero->Position();
  if (m == nullptr || !m->IsAlive()) {  // target gone / dead: stop where we are
    approach_ = ApproachState{};
    path_.clear();
    moving_ = false;
    return;
  }
  if (DistSq(hero, m->pos) <= approach_.stopRange * approach_.stopRange) {  // in range: the attack takes over
    approach_ = ApproachState{};
    path_.clear();
    moving_ = false;
    return;
  }
  const double now = ctx_.Now();
  if (now >= approach_.repathAtMs) {
    approach_.repathAtMs = now + ctx_.data.World().constants.holdMoveRepathMs;
    PathTo(m->pos, approach_.stopRange);
  }
}

bool HeroLocomotion::StartDash(Vec2 to, double durationMs, std::string_view skillId) {
  if (!(durationMs > 0) || dash_.active) return false;
  Hero& hero = *ctx_.sys.hero;
  path_.clear();
  moving_ = false;
  approach_ = ApproachState{};
  hold_ = HoldMoveState{};
  input_ = Vec2();
  dash_.active = true;
  dash_.from = hero.Position();
  dash_.to = to;
  dash_.startMs = ctx_.Now();
  dash_.durationMs = durationMs;
  dash_.skillId = std::string(skillId);
  const Vec2 dir = (to - dash_.from).Normalized();
  if (dir.LengthSq() > 0) {
    hero.SetFacing(dir);
    lastDir_ = dir;
  }
  ctx_.events.Emit(
      EvHeroDash{EvHeroDash::Phase::Started, dash_.from, dash_.to, dash_.startMs, dash_.durationMs, dash_.skillId});
  return true;
}

bool HeroLocomotion::TickDash() {
  if (!dash_.active) return false;
  Hero& hero = *ctx_.sys.hero;
  const bool immobilized = ctx_.sys.status != nullptr && ctx_.sys.status->IsImmobilized(kHeroEntityId);
  if (immobilized || hero.Life() != HeroLife::Alive) {  // C5: stun / freeze stop the dash where the hero stands
    const DashState d = dash_;
    dash_ = DashState{};
    ctx_.events.Emit(
        EvHeroDash{EvHeroDash::Phase::Interrupted, d.from, hero.Position(), d.startMs, d.durationMs, d.skillId});
    return true;
  }
  const double t = Clamp((ctx_.Now() - dash_.startMs) / dash_.durationMs, 0.0, 1.0);
  hero.SetPosition(Vec2(Lerp(dash_.from.x, dash_.to.x, t), Lerp(dash_.from.y, dash_.to.y, t)));
  speed_ = (dash_.to - dash_.from).Length() / (dash_.durationMs / 1000.0);
  if (t >= 1.0) {
    const DashState d = dash_;
    dash_ = DashState{};
    speed_ = 0;
    ctx_.events.Emit(EvHeroDash{EvHeroDash::Phase::Arrived, d.from, d.to, d.startMs, d.durationMs, d.skillId});
  }
  return true;
}

void HeroLocomotion::SetPath(std::vector<TilePos> path) {
  path_ = std::move(path);
  moving_ = !path_.empty();
}

void HeroLocomotion::Stop() {
  path_.clear();
  hold_ = HoldMoveState{};
  approach_ = ApproachState{};
  input_ = Vec2();
  moving_ = false;
}

void HeroLocomotion::SetMoveInput(Vec2 tileDir) {
  input_ = tileDir;
  if (tileDir.LengthSq() > 0) {
    lastDir_ = tileDir.Normalized();
    approach_ = ApproachState{};
    ctx_.bus.Publish(HeroMoveInputMsg{});
  }
}

void HeroLocomotion::BeginHold(int32_t pointerId, TilePos tile) {
  ABYSS_UNIMPLEMENTED();
}

void HeroLocomotion::UpdateHold(int32_t pointerId, TilePos tile, bool down) {
  if (!down && hold_.active && hold_.pointerId == pointerId) hold_ = HoldMoveState{};
  if (down) ABYSS_UNIMPLEMENTED();
}

void HeroLocomotion::Tick(double dtMs) {
  if (TickDash()) return;  // the dash owns the position (C4); input is suppressed meanwhile
  TickApproach();
  if (moving_ || input_.LengthSq() > 0 || hold_.active) ABYSS_UNIMPLEMENTED();
}

void HeroLocomotion::OnFreezeBegin(bool cinematic) {
  hold_ = HoldMoveState{};
  input_ = Vec2();
  if (cinematic) {
    path_.clear();
    approach_ = ApproachState{};
    moving_ = false;
  }
}

void HeroLocomotion::OnZoneEnter() {
  Stop();
  dash_ = DashState{};
  speed_ = 0;
}

void HeroLocomotion::Teleport(Vec2 to, TeleportReason reason) {
  const Vec2 from = ctx_.sys.hero->Position();
  path_.clear();
  approach_ = ApproachState{};
  dash_ = DashState{};
  moving_ = false;
  ctx_.sys.hero->SetPosition(to);
  ctx_.events.Emit(EvEntityTeleported{kHeroEntityId, from, to, reason});
}

void HeroLocomotion::FillSnapshot(Snapshot& out) const {
  out.hero.moving = moving_ || dash_.active;
  out.hero.speedTilesPerSec = speed_;
  out.hero.dashing = dash_.active;
}

}  // namespace abyss
