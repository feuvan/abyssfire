// Skill targeting geometry (classes-stats-skills.md 9.2, 9.7, 10; combat-feel.md 6.4-6.5; C1, C4, C8).
#include "abyss/base/Platform.h"

#include "abyss/combat/SkillTargeting.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/Math.h"

namespace abyss {

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

EntityId FindGroundAoeAnchor(std::span<const TargetCandidate> candidates, Vec2 heroPos, EntityId target,
                             Vec2 targetPos, bool targetAlive, double skillRange) {
  const double reach = (skillRange + 1) * (skillRange + 1);
  if (target != kNoEntity && targetAlive && DistSq(heroPos, targetPos) <= reach) return target;
  EntityId best = kNoEntity;
  double bestD = 0;
  for (const TargetCandidate& c : candidates) {
    if (!c.alive) continue;
    const double d = DistSq(heroPos, c.pos);
    if (d <= reach && (best == kNoEntity || d < bestD)) {
      best = c.id;
      bestD = d;
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
  const double cosHalf = std::cos(coneDeg * 0.5 * kPi / 180.0);
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

bool ComputeTeleportDestination(const SkillPortDef& port, Vec2 heroPos, const TeleportAim& aim, int32_t cols,
                                int32_t rows, const WalkableFn& walkable, TilePos& outTile) {
  // Aim (C8): pointer tile (desktop) > joystick x touchJoystickTiles > locked target > touchJoystickTiles ahead.
  const double ahead = port.teleportTouchJoystickTiles > 0 ? port.teleportTouchJoystickTiles : 6.0;
  Vec2 point;
  if (aim.hasPoint) {
    point = aim.point;
  } else if (aim.stickDir.Length() > port.teleportTouchDeadzone && aim.stickDir.Length() > 0) {
    point = heroPos + aim.stickDir.Normalized() * ahead;
  } else if (aim.hasTarget) {
    point = aim.targetPos;
  } else {
    const Vec2 f = aim.facing.LengthSq() > 0 ? aim.facing.Normalized() : Vec2(1, 0);
    point = heroPos + f * ahead;
  }
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
