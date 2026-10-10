// Hero locomotion (world-map-nav.md 6-7; S5, C4, C5, C7, W3, D13 F1): path following, direct (keyboard / stick /
// joystick) movement with the S5 ramp, hold-to-move (7.2), the C7 approach re-goal and the C4 dash. Owner area: world.
#include "abyss/base/Platform.h"

#include "abyss/world/Locomotion.h"

#include <cmath>

#include "abyss/base/Math.h"
#include "abyss/base/Units.h"
#include "abyss/combat/StatusEffects.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"
#include "abyss/world/Zone.h"

namespace abyss {

namespace locomotion_impl {

// cartToIso (world 1.2): tile -> world px of the 2:1 projection (IsoPixelParity metric).
Vec2 LocoCartToIso(Vec2 t) { return Vec2((t.x - t.y) * 32.0, (t.x + t.y) * 16.0); }

constexpr double kLocoFacePx = 1.5;  // 6.2: face the next node when that vector is > 1.5 px
constexpr double kLocoParityAccel = 8.0, kLocoParityDecel = 12.0, kLocoParityKeyboardFactor = 0.015;
constexpr double kLocoParityDirectGraceMs = 120.0;
constexpr double kLocoHoldStandStill = 0.6;  // 7.2: stand still within 0.6 tiles of the pointer
constexpr int32_t kLocoHoldNearRings = 3;    // 7.2: findWalkableNear(tile, 3)

}  // namespace locomotion_impl

Vec2 ScreenDirToTile(Vec2 screen) { return {screen.x + screen.y, -screen.x + screen.y}; }

HeroLocomotion::HeroLocomotion(SimContext& ctx) : ctx_(ctx) {}

bool HeroLocomotion::MoveTo(Vec2 goal, double stopRange) {
  approach_ = ApproachState{};
  ++generation_;
  const bool ok = PathTo(goal, stopRange);
  if (ok) ctx_.bus.Publish(HeroMoveInputMsg{});  // W3: a click-move is movement input
  return ok;
}

bool HeroLocomotion::PathTo(Vec2 goal, double stopRange) {
  goal_ = goal;
  stopRange_ = stopRange;
  const ZoneRuntime* zone = ctx_.sys.zone;
  if (zone == nullptr || !zone->HasZone()) return false;
  const Vec2 hero = ctx_.sys.hero->Position();
  TilePos g = RoundToTile(goal);
  if (!zone->Walkable(g.col, g.row)) {
    TilePos near;
    if (!zone->Paths().FindWalkableNear(g, locomotion_impl::kLocoHoldNearRings, near)) return false;
    g = near;
  }
  std::vector<TilePos> p;
  if (!zone->Paths().FindPath(JsRound(hero.x), JsRound(hero.y), g.col, g.row, p)) return false;
  SetPath(std::move(p));
  return true;
}

bool HeroLocomotion::Approach(EntityId target, double stopRange) {
  const MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(target) : nullptr;
  if (m == nullptr || !m->IsAlive()) return false;
  ++generation_;
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
  ++generation_;
  path_.clear();
  moving_ = false;
  approach_ = ApproachState{};
  hold_ = HoldMoveState{};
  input_ = Vec2();
  directSpeed_ = 0;
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
    speed_ = 0;
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
  if (moving_) directSpeed_ = 0;  // a click path ends a keyboard coast
}

void HeroLocomotion::Stop() {
  ++generation_;
  path_.clear();
  hold_ = HoldMoveState{};
  approach_ = ApproachState{};
  input_ = Vec2();
  directSpeed_ = 0;
  directMoveMs_ = 0;
  moving_ = false;
}

void HeroLocomotion::SetMoveInput(Vec2 tileDir) {
  input_ = tileDir;
  if (tileDir.LengthSq() > 0) {
    ++generation_;
    lastDir_ = tileDir.Normalized();
    approach_ = ApproachState{};
    ctx_.bus.Publish(HeroMoveInputMsg{});
  }
}

void HeroLocomotion::BeginHold(int32_t pointerId, TilePos tile) {
  hold_.active = true;
  hold_.pointerId = pointerId;
  hold_.tile = tile;
  hold_.pointer = tile;
  hold_.repathAtMs = ctx_.Now() + ctx_.data.World().constants.holdMoveRepathMs;
}

void HeroLocomotion::UpdateHold(int32_t pointerId, TilePos tile, bool down) {
  if (!hold_.active || hold_.pointerId != pointerId) return;  // holds only begin on a world press (7.1 row 12)
  if (!down) {
    hold_ = HoldMoveState{};  // released: the hero finishes the current path
    return;
  }
  hold_.pointer = tile;
}

bool HeroLocomotion::TryDirectStep(Vec2 delta, bool slide) {
  const ZoneRuntime* zone = ctx_.sys.zone;
  Hero& hero = *ctx_.sys.hero;
  const Vec2 pos = hero.Position();
  auto walkable = [zone](Vec2 p) {
    if (zone == nullptr || !zone->HasZone()) return true;
    return zone->Grid().WalkableAt(p);
  };
  const Vec2 n = pos + delta;
  if (walkable(n)) {
    hero.SetPosition(n);
    return true;
  }
  if (!slide) return false;  // web parity: only the destination is checked, no sliding (W3)
  // W3 FIX: axis-separated retry, col component first, then row.
  const Vec2 nc(pos.x + delta.x, pos.y);
  if (delta.x != 0 && walkable(nc)) {
    hero.SetPosition(nc);
    return true;
  }
  const Vec2 nr(pos.x, pos.y + delta.y);
  if (delta.y != 0 && walkable(nr)) {
    hero.SetPosition(nr);
    return true;
  }
  return false;
}

bool HeroLocomotion::TickDirect(double dtMs, double speedMul) {
  using namespace locomotion_impl;
  Hero& hero = *ctx_.sys.hero;
  const WorldConstants& wc = ctx_.data.World().constants;
  const double dtSec = dtMs / 1000.0;
  const bool hasInput = input_.LengthSq() > 0;
  if (model_ == HeroSpeedModel::IsoPixelParity) {
    if (!hasInput) return false;
    hold_ = HoldMoveState{};
    path_.clear();
    const Vec2 dir = input_.Normalized();
    lastDir_ = dir;
    const double step = hero.Derived().moveSpeed * speedMul * dtSec * kLocoParityKeyboardFactor;
    if (TryDirectStep(dir * step, false)) {
      hero.SetFacing(dir);
      moving_ = true;
      directMoveMs_ = kLocoParityDirectGraceMs;
    }
    return true;
  }
  const double full = hero.GroundSpeedTilesPerSec() * speedMul;
  if (hasInput) {
    hold_ = HoldMoveState{};
    path_.clear();
    const Vec2 dir = input_.Normalized();
    lastDir_ = dir;
    directSpeed_ = wc.heroRampMs > 0 ? (std::min)(full, directSpeed_ + full * dtMs / wc.heroRampMs) : full;
    if (directSpeed_ > full) directSpeed_ = full;  // a slow applied mid-run caps the speed at once
  } else if (directSpeed_ > 0 && path_.empty()) {
    // 60 ms stop: coast along the last direction while decelerating (S5).
    const double base = hero.GroundSpeedTilesPerSec();
    directSpeed_ = wc.heroStopMs > 0 ? (std::max)(0.0, directSpeed_ - base * dtMs / wc.heroStopMs) : 0.0;
    directSpeed_ = (std::min)(directSpeed_, full);
    if (directSpeed_ <= 0) return false;
  } else {
    directSpeed_ = 0;
    return false;
  }
  const Vec2 dir = hasInput ? input_.Normalized() : lastDir_;
  if (directSpeed_ > 0 && TryDirectStep(dir * (directSpeed_ * dtSec), true)) {
    hero.SetFacing(dir);
    moving_ = true;
  } else if (!hasInput) {
    directSpeed_ = 0;  // a wall ends the coast
  }
  return true;
}

void HeroLocomotion::TickHold() {
  using namespace locomotion_impl;
  if (!hold_.active) return;
  const ZoneRuntime* zone = ctx_.sys.zone;
  if (zone == nullptr || !zone->HasZone()) return;
  const Vec2 hero = ctx_.sys.hero->Position();
  const int32_t col = (std::max)(0, (std::min)(zone->Grid().Cols() - 1, hold_.pointer.col));
  const int32_t row = (std::max)(0, (std::min)(zone->Grid().Rows() - 1, hold_.pointer.row));
  if (JsHypot(col - hero.x, row - hero.y) < kLocoHoldStandStill) {  // close enough: stand still under the cursor
    path_.clear();
    return;
  }
  const bool moved = col != hold_.tile.col || row != hold_.tile.row;
  const double now = ctx_.Now();
  if (!moved && now < hold_.repathAtMs && !path_.empty()) return;
  hold_.tile = TilePos(col, row);
  hold_.repathAtMs = now + ctx_.data.World().constants.holdMoveRepathMs;
  TilePos goal(col, row);
  if (!zone->Walkable(col, row) && !zone->Paths().FindWalkableNear(TilePos(col, row), kLocoHoldNearRings, goal)) return;
  std::vector<TilePos> p;
  if (zone->Paths().FindPath(JsRound(hero.x), JsRound(hero.y), goal.col, goal.row, p)) {
    ++generation_;
    SetPath(std::move(p));
    ctx_.bus.Publish(HeroMoveInputMsg{});
  }
}

void HeroLocomotion::FaceAlongPath() {
  if (path_.empty()) return;
  Hero& hero = *ctx_.sys.hero;
  const Vec2 pos = hero.Position();
  const TilePos look = path_.size() > 1 ? path_[1] : path_[0];
  const Vec2 v = look.Center() - pos;
  // 6.2: face when the screen vector is > 1.5 px (iso metric), i.e. never flip on a sub-pixel remainder.
  const Vec2 iso = locomotion_impl::LocoCartToIso(v);
  if (iso.LengthSq() > locomotion_impl::kLocoFacePx * locomotion_impl::kLocoFacePx) {
    const Vec2 dir = v.Normalized();
    hero.SetFacing(dir);
    lastDir_ = dir;
  }
}

void HeroLocomotion::TickPath(double dtMs, double speedMul) {
  using namespace locomotion_impl;
  Hero& hero = *ctx_.sys.hero;
  const double dtSec = dtMs / 1000.0;
  if (model_ == HeroSpeedModel::IsoPixelParity) {
    if (path_.empty()) {
      if (directMoveMs_ > 0) {
        directMoveMs_ -= dtMs;
        return;
      }
      pathSpeedPx_ *= 1.0 - kLocoParityDecel * dtSec;
      if (pathSpeedPx_ < 0.5) {
        pathSpeedPx_ = 0;
        moving_ = false;
      }
      return;
    }
    const double target = hero.Derived().moveSpeed * speedMul;
    pathSpeedPx_ += (target - pathSpeedPx_) * kLocoParityAccel * dtSec;
    FaceAlongPath();
    const TilePos node = path_.front();
    const Vec2 pos = hero.Position();
    const Vec2 t = LocoCartToIso(node.Center());
    const Vec2 p = LocoCartToIso(pos);
    const double dist = (t - p).Length();
    const double step = pathSpeedPx_ * dtSec;
    if (dist <= step) {
      hero.SetPosition(node.Center());
      path_.erase(path_.begin());
      if (path_.empty()) moving_ = false;
    } else {
      hero.SetPosition(Vec2(pos.x + (node.col - pos.x) * step / dist, pos.y + (node.row - pos.y) * step / dist));
    }
    return;
  }
  if (path_.empty()) return;
  // S5 UniformTiles: full ground speed at once (click-move starts instantly); the remaining step carries over a node so
  // the ground speed is exactly moveSpeed / 36 (R9: stride matches speed). Stops exactly on the last node.
  double remaining = hero.GroundSpeedTilesPerSec() * speedMul * dtSec;
  if (!(remaining > 0)) return;
  FaceAlongPath();
  while (remaining > 0 && !path_.empty()) {
    if (stopRange_ > 0 && DistSq(hero.Position(), goal_) <= stopRange_ * stopRange_) {
      path_.clear();  // C7 / walk-then-act: within range of the goal
      break;
    }
    const TilePos node = path_.front();
    const Vec2 pos = hero.Position();
    const Vec2 d = node.Center() - pos;
    const double dist = d.Length();
    if (dist <= remaining) {
      hero.SetPosition(node.Center());
      remaining -= dist;
      path_.erase(path_.begin());
      if (!path_.empty()) FaceAlongPath();
    } else {
      hero.SetPosition(pos + d * (remaining / dist));
      remaining = 0;
    }
  }
  if (path_.empty()) moving_ = false;
}

void HeroLocomotion::Tick(double dtMs) {
  Hero& hero = *ctx_.sys.hero;
  const Vec2 before = hero.Position();
  if (TickDash()) return;  // the dash owns the position (C4); input is suppressed meanwhile
  if (hero.Life() != HeroLife::Alive || hero.Hp() <= 0) {  // 6.2: a dead hero has no path and does not move
    path_.clear();
    hold_ = HoldMoveState{};
    approach_ = ApproachState{};
    directSpeed_ = 0;
    pathSpeedPx_ = 0;
    moving_ = false;
    speed_ = 0;
    return;
  }
  // C5: freeze / stun stop movement (the path is kept and resumes); slow scales the speed.
  const double speedMul = ctx_.sys.status != nullptr ? ctx_.sys.status->SpeedMultiplier(kHeroEntityId) : 1.0;
  // moving_ = a walk order is running: a path, or direct movement this step (set by TickDirect / the parity grace).
  moving_ = !path_.empty() || (model_ == HeroSpeedModel::IsoPixelParity && directMoveMs_ > 0);
  const bool direct = TickDirect(dtMs, speedMul);  // 6.3: keyboard / stick clears hold + path
  if (!direct) {
    TickHold();
    TickApproach();
    TickPath(dtMs, speedMul);
  }
  const double dtSec = dtMs / 1000.0;
  speed_ = dtSec > 0 ? (hero.Position() - before).Length() / dtSec : 0.0;
}

void HeroLocomotion::OnFreezeBegin(bool cinematic) {
  hold_ = HoldMoveState{};
  input_ = Vec2();
  directSpeed_ = 0;
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
  pathSpeedPx_ = 0;
}

void HeroLocomotion::Teleport(Vec2 to, TeleportReason reason) {
  const Vec2 from = ctx_.sys.hero->Position();
  ++generation_;
  path_.clear();
  approach_ = ApproachState{};
  dash_ = DashState{};
  directSpeed_ = 0;
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
