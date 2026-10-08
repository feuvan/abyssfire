// Pure targeting geometry for skills and auto-battle.
// Spec: classes-stats-skills.md 9.2 (preferred target), 9.7 (ground AoE anchor, line targets), section 10 (teleport,
// shadow step); combat-feel.md 6.4-6.5, 9.2, 9.4; DECISIONS C1 (FIX Q17: teleport clamped to its range along the aim
// ray), C4 (multishot 50 degree cone from the hero), C8 (touch teleport: joystick x 6 tiles, else locked target, else
// 6 tiles ahead).
//
// Owner area: hero+combat. Functions take spans of TargetCandidate (id, pos, alive) built by the caller from
// MonsterSystem queries, in MonsterSystem order (spatial-grid order where the web used the grid).
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/combat/CombatInput.h"
#include "abyss/data/SkillData.h"

namespace abyss {

// Nearest alive candidate within maxRange (dist^2 <= maxRange^2; strictly smaller wins, first found keeps ties).
ABYSS_API EntityId NearestAlive(std::span<const TargetCandidate> candidates, Vec2 from, double maxRange);

// findGroundAoeAnchor (9.7): `target` if alive and dist^2 <= (range + 1)^2, else the nearest alive candidate within
// that reach, else kNoEntity.
ABYSS_API EntityId FindGroundAoeAnchor(std::span<const TargetCandidate> candidates, Vec2 heroPos, EntityId target,
                                       Vec2 targetPos, bool targetAlive, double skillRange);

// Alive candidates with dist(centre, m) <= radius, in candidate order.
ABYSS_API std::vector<EntityId> CandidatesInRadius(std::span<const TargetCandidate> candidates, Vec2 centre, double radius);

// monstersAlongLine (9.7): 0 <= along <= range and |perp| <= halfWidth from the hero toward targetPos.
ABYSS_API std::vector<EntityId> CandidatesAlongLine(std::span<const TargetCandidate> candidates, Vec2 heroPos, Vec2 targetPos,
                                                    EntityId target, double range, double halfWidth);

// C4 multishot: alive candidates within radius whose bearing from the hero is within coneDeg/2 of the aim direction.
ABYSS_API std::vector<EntityId> CandidatesInCone(std::span<const TargetCandidate> candidates, Vec2 heroPos, Vec2 aimDir,
                                                 double radius, double coneDeg);

using WalkableFn = std::function<bool(int32_t col, int32_t row)>;

// Teleport destination (section 10 + FIX Q17 + C8). Aim = pointer tile (desktop) or the touch rule; the point is
// clamped to maxRange along the hero->aim ray, rounded, clamped to [1, size - 2], then if not walkable the square
// rings r = 1..rings are searched row-major (dr = -r..r, dc = -r..r) for the first walkable tile. False = unreachable
// (caller refunds mana, keeps the cooldown, logs zone.teleport.unreachable).
struct TeleportAim {
  bool hasPoint = false;   // pointer ground tile (desktop / mouse)
  Vec2 point;
  Vec2 stickDir;           // touch joystick direction (length > deadzone wins)
  bool hasTarget = false;  // locked target position (touch fallback)
  Vec2 targetPos;
  Vec2 facing{1, 0};       // last fallback: maxRange ahead
};
ABYSS_API bool ComputeTeleportDestination(const SkillPortDef& port, Vec2 heroPos, const TeleportAim& aim, int32_t cols,
                                          int32_t rows, const WalkableFn& walkable, TilePos& outTile);

// Shadow step (section 10): round(target - unit(hero - target)) clamped to the map; not walkable -> the target tile.
ABYSS_API TilePos ShadowStepDestination(Vec2 heroPos, Vec2 targetPos, int32_t cols, int32_t rows, const WalkableFn& walkable);

// Charge dash end point (C4): along hero->target, stopping at melee range (attackRange) of the target.
ABYSS_API Vec2 ChargeDashEnd(Vec2 heroPos, Vec2 targetPos, double stopRange);

}  // namespace abyss
