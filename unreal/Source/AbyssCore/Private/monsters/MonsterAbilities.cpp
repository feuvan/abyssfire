// Monster abilities and telegraphs: pure rules (dark remake K2, implementation-plan.md 3 K2; monsters.md 1.3).
#include "abyss/base/Platform.h"

#include "abyss/monsters/MonsterAbilities.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/Math.h"

namespace abyss {

namespace {

// Cooldowns compare on the sim clock with the swing scheduler's tolerance (Combat.cpp kCombatSwingEpsilonMs).
constexpr double kAbilityEpsilonMs = 1e-6;

Vec2 AbilityUnitOr(Vec2 v, Vec2 fallback) {
  const double l2 = v.LengthSq();
  if (l2 > 0) return v / std::sqrt(l2);
  const double f2 = fallback.LengthSq();
  if (f2 > 0) return fallback / std::sqrt(f2);
  return Vec2(1, 0);
}

// Squared distance from `p` to the segment a-b.
double AbilitySegmentDistSq(Vec2 p, Vec2 a, Vec2 b) {
  const Vec2 ab = b - a;
  const double len2 = ab.LengthSq();
  double t = 0;
  if (len2 > 0) t = Clamp(((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / len2, 0.0, 1.0);
  return DistSq(p, a + ab * t);
}

bool AbilityIsDirectional(AbilityShapeKind k) { return k == AbilityShapeKind::Cone || k == AbilityShapeKind::Line; }

}  // namespace

bool AbilityKindRuns(AbilityKind kind) {
  switch (kind) {
    case AbilityKind::Circle:
    case AbilityKind::Cone:
    case AbilityKind::Line:
    case AbilityKind::Ring:
    case AbilityKind::Bolt:
    case AbilityKind::Hazard:
    case AbilityKind::Raise:   // K3 (corpses)
    case AbilityKind::Summon:  // K3 (raise caps)
      return true;
    case AbilityKind::Buff:    // K5 (packs, morale)
    case AbilityKind::Call:    // K5 (social call)
      return false;
  }
  return false;
}

bool AbilityKindSummons(AbilityKind kind) { return kind == AbilityKind::Raise || kind == AbilityKind::Summon; }

bool AbilityHasGroundShape(AbilityKind kind) {
  return kind == AbilityKind::Circle || kind == AbilityKind::Cone || kind == AbilityKind::Line ||
         kind == AbilityKind::Ring || kind == AbilityKind::Hazard;
}

bool AbilityUsable(const MonsterAbilityDef& a, size_t index, const AbilityPickInput& in) {
  if (!AbilityKindRuns(a.kind)) return false;
  if (in.summonsOnly && !AbilityKindSummons(a.kind)) return false;
  if (index >= in.readyAtMs.size() || in.nowMs + kAbilityEpsilonMs < in.readyAtMs[index]) return false;
  if (in.heroDist < a.rangeMin) return false;
  if (a.hasRangeMax && in.heroDist > a.rangeMax) return false;
  if (a.hasHpBelowPct && !(in.hpPct < a.hpBelowPct)) return false;
  if (!a.phases.empty() && std::find(a.phases.begin(), a.phases.end(), in.phase) == a.phases.end()) return false;
  if (AbilityKindSummons(a.kind) && (index >= in.available.size() || in.available[index] == 0)) return false;
  return true;
}

int32_t PickMonsterAbility(std::span<const MonsterAbilityDef> abilities, const AbilityPickInput& in) {
  int32_t best = -1;
  for (size_t i = 0; i < abilities.size(); ++i) {
    if (!AbilityUsable(abilities[i], i, in)) continue;
    if (best < 0 || abilities[i].priority > abilities[static_cast<size_t>(best)].priority) best = static_cast<int32_t>(i);
  }
  return best;
}

bool ShapeContains(const AbilityShapeDef& s, Vec2 origin, Vec2 dir, Vec2 point, double bodyRadius) {
  const Vec2 v = point - origin;
  const double d2 = v.LengthSq();
  const double r = (std::max)(0.0, bodyRadius);
  switch (s.kind) {
    case AbilityShapeKind::Circle: {
      const double reach = s.radius + r;
      return d2 <= reach * reach;
    }
    case AbilityShapeKind::Ring: {
      const double outer = s.radius + r;
      if (d2 > outer * outer) return false;
      const double inner = s.innerRadius - r;
      return inner <= 0 || d2 >= inner * inner;
    }
    case AbilityShapeKind::Cone: {
      const double reach = s.radius + r;
      if (d2 > reach * reach) return false;
      if (s.arcDeg >= 360 || d2 <= r * r) return true;  // a full circle, or the body covers the apex
      const Vec2 u = AbilityUnitOr(dir, Vec2(1, 0));
      const double halfRad = s.arcDeg * kPi / 360.0;
      const double cosHalf = DetCos(halfRad);
      const double dist = std::sqrt(d2);
      if (v.x * u.x + v.y * u.y >= cosHalf * dist) return true;  // inside the opening: the radial test above holds
      // Outside the opening: the body must touch one of the two edges.
      const double sinHalf = DetSin(halfRad);
      const Vec2 e1(u.x * cosHalf - u.y * sinHalf, u.x * sinHalf + u.y * cosHalf);
      const Vec2 e2(u.x * cosHalf + u.y * sinHalf, -u.x * sinHalf + u.y * cosHalf);
      return AbilitySegmentDistSq(point, origin, origin + e1 * s.radius) <= r * r ||
             AbilitySegmentDistSq(point, origin, origin + e2 * s.radius) <= r * r;
    }
    case AbilityShapeKind::Line: {
      const Vec2 u = AbilityUnitOr(dir, Vec2(1, 0));
      const double along = v.x * u.x + v.y * u.y;
      const double across = v.y * u.x - v.x * u.y;  // lateral offset (perpendicular (-u.y, u.x))
      const double half = s.width * 0.5;
      const double ca = Clamp(along, 0.0, s.length);
      const double cl = Clamp(across, -half, half);
      const double dx = along - ca;
      const double dy = across - cl;
      return dx * dx + dy * dy <= r * r;
    }
  }
  return false;
}

double ShapeReach(const AbilityShapeDef& s) {
  switch (s.kind) {
    case AbilityShapeKind::Circle:
    case AbilityShapeKind::Ring:
    case AbilityShapeKind::Cone:
      return s.radius;
    case AbilityShapeKind::Line:
      return std::sqrt(s.length * s.length + s.width * s.width * 0.25);
  }
  return 0;
}

AbilityTiming ComputeAbilityTiming(const MonsterAbilityDef& a, const AnimClipDef* clip, double minWindupMs,
                                   const AnimTimingTable& t, AnimRig rig) {
  double beat = 0;
  if (clip != nullptr) {
    if (clip->hasContactMs && clip->contactMs > 0) {
      beat = clip->contactMs;
    } else if (clip->hasReleaseMs && clip->releaseMs > 0) {
      beat = clip->releaseMs;
    }
  }
  double contact = a.hasWindupMs ? a.windupMs : (beat > 0 ? beat : minWindupMs);
  contact = (std::max)(contact, minWindupMs);
  AbilityTiming out;
  out.contactMs = contact;
  out.fromClip = beat > 0;
  out.playRate = beat > 0 ? beat / contact : 1.0;
  if (clip != nullptr && clip->lengthMs > 0) {
    out.durationMs = (std::max)(contact, clip->lengthMs / out.playRate);
  } else {
    const double frame = t.Contact(rig).frameContactMs;
    const double swing = t.Preset(rig).attackDuration;
    out.durationMs = frame > 0 && swing > 0 ? (std::max)(contact, contact * swing / frame) : contact;
  }
  return out;
}

AnimAction AbilityAnimAction(std::string_view clip) {
  if (clip == "Attack02") return AnimAction::Attack02;
  if (clip == "Cast02") return AnimAction::Cast02;
  if (clip == "Roar") return AnimAction::Roar;
  if (clip.substr(0, 4) == "Cast") return AnimAction::Cast;
  return AnimAction::Attack;
}

MonsterAbilityCast ResolveAbilityCast(const MonsterAbilityDef& a, const AbilityAimInput& in, double startMs,
                                      double contactMs) {
  MonsterAbilityCast c;
  c.startMs = startMs;
  c.contactMs = contactMs;
  c.shape = a.shape;
  const Vec2 anchor = in.anchor.Center();
  switch (a.aim) {
    case AbilityAim::Hero:
    case AbilityAim::HeroAtWindup: c.target = in.heroPos; break;
    case AbilityAim::Anchor: c.target = anchor; break;
    case AbilityAim::Self:
    case AbilityAim::Corpse: c.target = in.casterPos; break;  // corpse: K3 places it on the picked corpse
  }
  if (a.hasShape && AbilityIsDirectional(a.shape.kind)) {
    c.origin = a.aim == AbilityAim::Anchor ? anchor : in.casterPos;
    c.dir = a.aim == AbilityAim::Self ? AbilityUnitOr(in.casterHeading, Vec2(1, 0))
                                      : AbilityUnitOr(in.heroPos - c.origin, in.casterHeading);
  } else {
    c.origin = a.kind == AbilityKind::Bolt ? in.casterPos : c.target;
    c.dir = AbilityUnitOr(in.heroPos - in.casterPos, in.casterHeading);
  }
  return c;
}

TelegraphStyle TelegraphStyleOf(const MonsterInstance& m) {
  switch (m.role) {
    case MonsterRole::StoryBoss:
    case MonsterRole::ZoneMiniBoss:
    case MonsterRole::SubDungeonMiniBoss:
    case MonsterRole::SealKeeper:
      return TelegraphStyle::Boss;
    default:
      break;
  }
  if (m.def.elite || !m.affixes.empty() || m.role == MonsterRole::HuntLeader) return TelegraphStyle::Elite;
  return TelegraphStyle::Normal;
}

double AbilityMinWindupMs(TelegraphStyle style) {
  return style == TelegraphStyle::Boss ? kAbilityBossMinWindupMs : kAbilityMinWindupMs;
}

double MonsterHitCap(double heroMaxHp, double capPct, double difficultyDamageMul) {
  if (!(capPct > 0) || !(heroMaxHp > 0)) return 0;
  const double mul = difficultyDamageMul > 0 ? difficultyDamageMul : 1.0;
  return (std::max)(1.0, std::floor(heroMaxHp * capPct * mul / 100.0));
}

double ApplyMonsterHitCap(double damage, double heroMaxHp, double capPct, double difficultyDamageMul) {
  const double cap = MonsterHitCap(heroMaxHp, capPct, difficultyDamageMul);
  return cap > 0 ? (std::min)(damage, cap) : damage;
}

StatusRule AbilityStatusRule(const AbilityStatusDef& s) {
  StatusRule r;
  r.status = s.type;
  r.hasChance = s.chance < 1;
  r.chance = s.chance;
  r.valueKind = StatusValueKind::Fixed;
  r.amount = s.value;
  r.hasDuration = true;
  r.durationMs = SaturatingInt32(s.ms);
  return r;
}

double HazardTickDamage(double monsterDamage, double tickDmgMul) {
  if (!(tickDmgMul > 0) || !(monsterDamage > 0)) return 0;
  return (std::max)(1.0, std::floor(monsterDamage * tickDmgMul));
}

}  // namespace abyss
