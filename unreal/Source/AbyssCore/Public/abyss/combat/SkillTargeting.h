// Pure targeting geometry for skills and auto-battle.
// Spec: classes-stats-skills.md 9.2 (cast target rule), 9.7 (release geometry per cast shape), section 10 (teleport,
// shadow step); combat-feel.md 6.4-6.5, 9.2, 9.4; DECISIONS C14 (cast target rule: resolver, cast reach, release
// geometry, no-target fallbacks), C1 (FIX Q17: teleport clamped to its range along the aim ray), C4 (multishot 50 degree
// cone from the hero, chain lightning target to target), C8 (touch teleport: joystick x 6 tiles, else locked target,
// else 6 tiles ahead).
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

// How a skill is aimed and released (DECISIONS C14), derived from the exported skill data:
//   Self        buffs (execKind buff, taunt_roar included): no target; no facing.
//   Blink       teleport: the cursor / stick / lock destination (C8); never resolves or changes the selection.
//   Melee       single target, attack animation (slash, lethal_strike, bleed_strike, backstab): needs a target within
//               range + rangeSlackTiles, at the press and again at the contact beat.
//   Dash        charge (C4 dash): needs a target within range + rangeSlackTiles at the press.
//   Projectile  fireball, ice_arrow, poison_arrow: homing at the target; with no target a shot along the aim
//               direction that flies the cast reach and fizzles.
//   Instant     other single-target skills (combustion, freeze, shadow_step, death_mark): need a target within the
//               cast reach.
//   Ground      ground-anchored AoEs (meteor, blizzard, fire_wall, arcane_torrent, arrow_rain, poison_cloud).
//   Trap        armed traps (explosive_trap, chain_trap, slow_trap).
//   Chain       chain_lightning: link 0 is the target, then target to target.
//   Cone        multishot.   Line  piercing_arrow.
//   Nova        hero-centred blasts (whirlwind, war_stomp, rampage).
enum class CastShape : uint8_t { Self, Blink, Melee, Dash, Projectile, Instant, Ground, Trap, Chain, Cone, Line, Nova };
ABYSS_ENUM_STRINGS(CastShape, "self", "blink", "melee", "dash", "projectile", "instant", "ground", "trap", "chain",
                   "cone", "line", "nova")

ABYSS_API CastShape SkillCastShape(const SkillDef& s);
// Melee, Dash and Instant casts need a resolved target (no fallback without one).
ABYSS_API bool CastShapeNeedsTarget(CastShape shape);
// The press-time pick of these shapes becomes the selection (hero lock + target ring); Self, Blink and Nova only turn
// the hero toward it.
ABYSS_API bool CastShapeSelects(CastShape shape);
// Cast reach = acquisition radius: max(range + castAcquirePadTiles, castAcquireMinTiles) (8 tiles for every shipped
// skill). The auto-pick searches it, and every ranged shape releases at a target within it.
ABYSS_API double SkillCastReach(const SkillRules& rules, const SkillDef& s);
// Where the target must stand for the cast to go out: range + rangeSlackTiles for Melee and Dash, else the cast reach.
ABYSS_API double SkillReleaseReach(const SkillRules& rules, const SkillDef& s);

// Aim inputs of one release (pure): the resolved target's position (or its last known position), the desktop cursor
// ground point, the hero's facing.
struct CastAim {
  bool hasTarget = false;
  Vec2 targetPos;
  bool hasPoint = false;
  Vec2 point;
  Vec2 facing{1, 0};
};
// Unit cast direction (cones, lines, projectiles, chains, facing): toward the target, else toward the cursor point,
// else the facing (else +x). A point on the hero (< 1e-6 tile) does not count.
ABYSS_API Vec2 CastDirection(Vec2 heroPos, const CastAim& aim);
// Centre of a ground AoE or trap: the target, else the cursor point, else `noTargetAheadTiles` along the facing (0 = the
// hero's feet); clamped to `reach` along the hero -> point ray.
ABYSS_API Vec2 GroundCastPoint(Vec2 heroPos, const CastAim& aim, double reach, double noTargetAheadTiles);
// C4 chain: link 0 = `first` (taken as given), then repeatedly the nearest alive candidate not linked yet within
// hopRadius of the previous link (strictly smaller wins, first found keeps ties), up to maxLinks links (0 = no cap).
ABYSS_API std::vector<EntityId> ChainLinks(std::span<const TargetCandidate> candidates, EntityId first, Vec2 firstPos,
                                           double hopRadius, int32_t maxLinks);

// Nearest alive candidate within maxRange (dist^2 <= maxRange^2; strictly smaller wins, first found keeps ties).
ABYSS_API EntityId NearestAlive(std::span<const TargetCandidate> candidates, Vec2 from, double maxRange);

// Alive candidates with dist(centre, m) <= radius, in candidate order.
ABYSS_API std::vector<EntityId> CandidatesInRadius(std::span<const TargetCandidate> candidates, Vec2 centre, double radius);

// monstersAlongLine (9.7): 0 <= along <= range and |perp| <= halfWidth from the hero toward targetPos.
ABYSS_API std::vector<EntityId> CandidatesAlongLine(std::span<const TargetCandidate> candidates, Vec2 heroPos, Vec2 targetPos,
                                                    EntityId target, double range, double halfWidth);

// C4 multishot: alive candidates within radius whose bearing from the hero is within coneDeg/2 of the aim direction.
ABYSS_API std::vector<EntityId> CandidatesInCone(std::span<const TargetCandidate> candidates, Vec2 heroPos, Vec2 aimDir,
                                                 double radius, double coneDeg);

using WalkableFn = std::function<bool(int32_t col, int32_t row)>;

// Teleport destination (section 10 + FIX Q17 + C8). Aim = pointer tile (desktop), else the touch rule (joystick beyond
// the deadzone x touchJoystickTiles, else the locked target, else touchJoystickTiles ahead along `facing`); the point is
// clamped to maxRange along the hero->aim ray, rounded, clamped to [1, size - 2], then if not walkable the square
// rings r = 1..rings are searched row-major (dr = -r..r, dc = -r..r) for the first walkable tile. False = unreachable
// (caller refunds mana, keeps the cooldown, logs zone.teleport.unreachable).
struct TeleportAim {
  bool hasPoint = false;   // pointer ground tile (desktop / mouse)
  Vec2 point;
  Vec2 stickDir;           // touch joystick direction (length > deadzone wins)
  bool hasTarget = false;  // locked target position (touch fallback)
  Vec2 targetPos;
  Vec2 facing{1, 0};       // last fallback: touchJoystickTiles (6) ahead (C8)
};
// The aimed point before the range clamp, the map clamp and the ring search (the cast faces it, C14).
ABYSS_API Vec2 TeleportAimPoint(const SkillPortDef& port, Vec2 heroPos, const TeleportAim& aim);
ABYSS_API bool ComputeTeleportDestination(const SkillPortDef& port, Vec2 heroPos, const TeleportAim& aim, int32_t cols,
                                          int32_t rows, const WalkableFn& walkable, TilePos& outTile);

// Shadow step (section 10): round(target - unit(hero - target)) clamped to the map; not walkable -> the target tile.
ABYSS_API TilePos ShadowStepDestination(Vec2 heroPos, Vec2 targetPos, int32_t cols, int32_t rows, const WalkableFn& walkable);

// Charge dash end point (C4): along hero->target, stopping `stopRange` (the hero's melee attackRange) short of the
// target; the hero position itself when already within stopRange.
ABYSS_API Vec2 ChargeDashEnd(Vec2 heroPos, Vec2 targetPos, double stopRange);

}  // namespace abyss
