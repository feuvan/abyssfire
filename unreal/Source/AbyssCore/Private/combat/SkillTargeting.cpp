// Skill targeting geometry (classes-stats-skills.md 9.2, 9.7, 10; combat-feel.md 6.4-6.5; C1, C4, C8, C14).
#include "abyss/base/Platform.h"

#include "abyss/combat/SkillTargeting.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/Math.h"

namespace abyss {

namespace {
// A point closer than this to the hero gives no direction (cursor on the hero, target on the hero's tile centre).
constexpr double kTargetingMinAimTiles = 1e-6;
}  // namespace

CastShape SkillCastShape(const SkillDef& s) {
  switch (s.execKind) {
    case SkillExecKind::Teleport: return CastShape::Blink;
    case SkillExecKind::Buff: return CastShape::Self;
    case SkillExecKind::SlowTrap: return CastShape::Trap;
    case SkillExecKind::ShadowStep:
    case SkillExecKind::DeathMark: return CastShape::Instant;
    case SkillExecKind::Aoe:
      if (s.port.persistentGround && s.port.groundTrigger == GroundTrigger::Armed) return CastShape::Trap;
      if (s.groundAnchored) return CastShape::Ground;
      if (s.port.chainStaggerMs > 0) return CastShape::Chain;
      if (s.port.coneDeg > 0) return CastShape::Cone;
      if (s.hasLineTarget) return CastShape::Line;
      return CastShape::Nova;
    case SkillExecKind::Single:
      if (s.port.hasDash) return CastShape::Dash;
      if (s.animKind == SkillAnimKind::Attack) return CastShape::Melee;
      if (s.hasProjectile) return CastShape::Projectile;
      return CastShape::Instant;
  }
  return CastShape::Instant;
}

bool CastShapeNeedsTarget(CastShape shape) {
  return shape == CastShape::Melee || shape == CastShape::Dash || shape == CastShape::Instant;
}

bool CastShapeSelects(CastShape shape) {
  return shape != CastShape::Self && shape != CastShape::Blink && shape != CastShape::Nova;
}

double SkillCastReach(const SkillRules& rules, const SkillDef& s) {
  return (std::max)(s.range + rules.castAcquirePadTiles, rules.castAcquireMinTiles);
}

double SkillReleaseReach(const SkillRules& rules, const SkillDef& s) {
  const CastShape shape = SkillCastShape(s);
  if (shape == CastShape::Melee || shape == CastShape::Dash) return s.range + rules.rangeSlackTiles;
  return SkillCastReach(rules, s);
}

Vec2 CastDirection(Vec2 heroPos, const CastAim& aim) {
  if (aim.hasTarget) {
    const Vec2 d = aim.targetPos - heroPos;
    const double len = d.Length();
    if (len > kTargetingMinAimTiles) return d / len;
  }
  if (aim.hasPoint) {
    const Vec2 d = aim.point - heroPos;
    const double len = d.Length();
    if (len > kTargetingMinAimTiles) return d / len;
  }
  const double f = aim.facing.Length();
  return f > kTargetingMinAimTiles ? aim.facing / f : Vec2(1, 0);
}

Vec2 GroundCastPoint(Vec2 heroPos, const CastAim& aim, double reach, double noTargetAheadTiles) {
  Vec2 p;
  if (aim.hasTarget) {
    p = aim.targetPos;
  } else if (aim.hasPoint) {
    p = aim.point;
  } else {
    const double f = aim.facing.Length();
    p = heroPos + (f > kTargetingMinAimTiles ? aim.facing / f : Vec2(1, 0)) * noTargetAheadTiles;
  }
  // FIX Q17's rule for teleport, applied to every ground cast: clamp along the hero -> point ray.
  const Vec2 rel = p - heroPos;
  const double len = rel.Length();
  if (reach >= 0 && len > reach) p = heroPos + rel * (reach / len);
  return p;
}

std::vector<EntityId> ChainLinks(std::span<const TargetCandidate> candidates, EntityId first, Vec2 firstPos,
                                 double hopRadius, int32_t maxLinks) {
  std::vector<EntityId> links;
  if (first == kNoEntity) return links;
  links.push_back(first);
  const double hopSq = hopRadius * hopRadius;
  Vec2 prev = firstPos;
  while (maxLinks <= 0 || static_cast<int32_t>(links.size()) < maxLinks) {
    const TargetCandidate* best = nullptr;
    double bestSq = 0;
    for (const TargetCandidate& c : candidates) {
      if (!c.alive || std::find(links.begin(), links.end(), c.id) != links.end()) continue;
      const double d = DistSq(prev, c.pos);
      if (d > hopSq) continue;
      if (best == nullptr || d < bestSq) {
        best = &c;
        bestSq = d;
      }
    }
    if (best == nullptr) break;
    links.push_back(best->id);
    prev = best->pos;
  }
  return links;
}

EntityId NearestAlive(std::span<const TargetCandidate> candidates, Vec2 from, double maxRange) {
  EntityId best = kNoEntity;
  double bestSq = 0;
  const double maxSq = maxRange * maxRange;
  for (const TargetCandidate& c : candidates) {
    if (!c.alive) continue;
    const double d = DistSq(from, c.pos);
    if (d > maxSq) continue;
    if (best == kNoEntity || d < bestSq) {
      best = c.id;
      bestSq = d;
    }
  }
  return best;
}

std::vector<EntityId> CandidatesInRadius(std::span<const TargetCandidate> candidates, Vec2 centre, double radius) {
  std::vector<EntityId> out;
  const double rSq = radius * radius;
  for (const TargetCandidate& c : candidates) {
    if (c.alive && DistSq(centre, c.pos) <= rSq) out.push_back(c.id);
  }
  return out;
}

std::vector<EntityId> CandidatesAlongLine(std::span<const TargetCandidate> candidates, Vec2 heroPos, Vec2 targetPos,
                                          EntityId target, double range, double halfWidth) {
  std::vector<EntityId> out;
  Vec2 d = targetPos - heroPos;
  const double len = JsHypot(d.x, d.y);
  if (len < 0.001) {
    bool alive = true;
    for (const TargetCandidate& c : candidates) {
      if (c.id == target) alive = c.alive;
    }
    if (target != kNoEntity && alive) out.push_back(target);
    return out;
  }
  d = d / len;
  for (const TargetCandidate& c : candidates) {
    if (!c.alive) continue;
    const double rx = c.pos.x - heroPos.x;
    const double ry = c.pos.y - heroPos.y;
    const double along = rx * d.x + ry * d.y;
    const double perp = std::fabs(rx * d.y - ry * d.x);
    if (along >= 0 && along <= range && perp <= halfWidth) out.push_back(c.id);
  }
  return out;
}

std::vector<EntityId> CandidatesInCone(std::span<const TargetCandidate> candidates, Vec2 heroPos, Vec2 aimDir,
                                       double radius, double coneDeg) {
  std::vector<EntityId> out;
  const Vec2 aim = aimDir.Normalized();
  const double rSq = radius * radius;
  const double cosHalf = DetCos(coneDeg * 0.5 * kPi / 180.0);
  for (const TargetCandidate& c : candidates) {
    if (!c.alive) continue;
    const Vec2 rel = c.pos - heroPos;
    const double dSq = rel.LengthSq();
    if (dSq > rSq) continue;
    if (dSq < 1e-12 || !(aim.LengthSq() > 0)) {  // standing on the hero / no aim: inside the fan
      out.push_back(c.id);
      continue;
    }
    const double cosAngle = rel.Dot(aim) / std::sqrt(dSq);
    if (cosAngle >= cosHalf - 1e-12) out.push_back(c.id);
  }
  return out;
}

namespace {
TilePos TargetingClampTile(TilePos t, int32_t cols, int32_t rows) {
  return TilePos((std::max)(1, (std::min)(cols - 2, t.col)), (std::max)(1, (std::min)(rows - 2, t.row)));
}
}  // namespace

Vec2 TeleportAimPoint(const SkillPortDef& port, Vec2 heroPos, const TeleportAim& aim) {
  // Aim (C8): pointer tile (desktop) > joystick x touchJoystickTiles > locked target > touchJoystickTiles ahead.
  const double ahead = port.teleportTouchJoystickTiles > 0 ? port.teleportTouchJoystickTiles : 6.0;
  if (aim.hasPoint) return aim.point;
  if (aim.stickDir.Length() > port.teleportTouchDeadzone && aim.stickDir.Length() > 0) {
    return heroPos + aim.stickDir.Normalized() * ahead;
  }
  if (aim.hasTarget) return aim.targetPos;
  const Vec2 f = aim.facing.LengthSq() > 0 ? aim.facing.Normalized() : Vec2(1, 0);
  return heroPos + f * ahead;
}

bool ComputeTeleportDestination(const SkillPortDef& port, Vec2 heroPos, const TeleportAim& aim, int32_t cols,
                                int32_t rows, const WalkableFn& walkable, TilePos& outTile) {
  Vec2 point = TeleportAimPoint(port, heroPos, aim);
  // FIX Q17: clamp to the skill's max range along the hero -> aim ray.
  if (port.teleportMaxRangeTiles > 0) {
    const Vec2 rel = point - heroPos;
    const double len = rel.Length();
    if (len > port.teleportMaxRangeTiles) point = heroPos + rel * (port.teleportMaxRangeTiles / len);
  }
  TilePos dest = TargetingClampTile(RoundToTile(point), cols, rows);
  if (walkable && walkable(dest.col, dest.row)) {
    outTile = dest;
    return true;
  }
  const int32_t rings = port.teleportWalkableSearchRings > 0 ? port.teleportWalkableSearchRings : 3;
  for (int32_t r = 1; r <= rings; ++r) {
    for (int32_t dr = -r; dr <= r; ++dr) {
      for (int32_t dc = -r; dc <= r; ++dc) {
        const int32_t nr = dest.row + dr;
        const int32_t nc = dest.col + dc;
        if (nr >= 1 && nr < rows - 1 && nc >= 1 && nc < cols - 1 && walkable && walkable(nc, nr)) {
          outTile = TilePos(nc, nr);
          return true;
        }
      }
    }
  }
  return false;
}

TilePos ShadowStepDestination(Vec2 heroPos, Vec2 targetPos, int32_t cols, int32_t rows, const WalkableFn& walkable) {
  const Vec2 d = heroPos - targetPos;
  double len = std::sqrt(d.x * d.x + d.y * d.y);
  if (!(len > 0)) len = 1;  // web: `|| 1`
  const TilePos behind = TargetingClampTile(RoundToTile(targetPos - d / len), cols, rows);
  if (walkable && walkable(behind.col, behind.row)) return behind;
  return RoundToTile(targetPos);
}

Vec2 ChargeDashEnd(Vec2 heroPos, Vec2 targetPos, double stopRange) {
  const Vec2 rel = targetPos - heroPos;
  const double dist = rel.Length();
  if (dist <= stopRange || !(dist > 0)) return heroPos;
  return heroPos + rel * ((dist - stopRange) / dist);
}

}  // namespace abyss
