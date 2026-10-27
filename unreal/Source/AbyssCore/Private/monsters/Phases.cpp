// Boss phases and arenas: pure rules (dark remake K4, implementation-plan.md 3 K4; monsters.md 1.3).
#include "abyss/base/Platform.h"

#include "abyss/monsters/Phases.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/Math.h"

namespace abyss {

namespace {

// A product such as 100 x 1.15 lands a hair under its whole value (114.99999999999999): the floor keeps it.
constexpr double kPhaseFloorSlack = 1e-9;

int32_t PhaseAbilityIndex(const MonsterDef& def, const std::string& id) {
  for (size_t i = 0; i < def.abilities.size(); ++i) {
    if (def.abilities[i].id == id) return static_cast<int32_t>(i);
  }
  return -1;
}

}  // namespace

int32_t NextPhaseToEnter(std::span<const MonsterPhaseDef> phases, int32_t phase, double hpPct) {
  if (phase < 0 || static_cast<size_t>(phase) >= phases.size()) return -1;
  return hpPct < phases[static_cast<size_t>(phase)].hpBelowPct ? phase : -1;
}

void PhaseAbilitiesOff(const MonsterDef& def, int32_t phase, std::vector<uint8_t>& out) {
  out.clear();
  if (def.phases.empty()) return;
  out.assign(def.abilities.size(), 0);
  // An ability whose first switch (in row order) is abilitiesOn starts off; one first switched off starts on.
  std::vector<uint8_t> seen(def.abilities.size(), 0);
  for (const MonsterPhaseDef& row : def.phases) {
    for (const std::string& id : row.abilitiesOn) {
      const int32_t i = PhaseAbilityIndex(def, id);
      if (i < 0 || seen[static_cast<size_t>(i)] != 0) continue;
      seen[static_cast<size_t>(i)] = 1;
      out[static_cast<size_t>(i)] = 1;
    }
    for (const std::string& id : row.abilitiesOff) {
      const int32_t i = PhaseAbilityIndex(def, id);
      if (i >= 0) seen[static_cast<size_t>(i)] = 1;
    }
  }
  const size_t entered = static_cast<size_t>((std::max)(0, (std::min)(phase, static_cast<int32_t>(def.phases.size()))));
  for (size_t r = 0; r < entered; ++r) {
    const MonsterPhaseDef& row = def.phases[r];
    for (const std::string& id : row.abilitiesOn) {
      const int32_t i = PhaseAbilityIndex(def, id);
      if (i >= 0) out[static_cast<size_t>(i)] = 0;
    }
    for (const std::string& id : row.abilitiesOff) {
      const int32_t i = PhaseAbilityIndex(def, id);
      if (i >= 0) out[static_cast<size_t>(i)] = 1;
    }
  }
}

bool AbilityEnabledInPhase(const MonsterAbilityDef& a, size_t index, std::span<const uint8_t> off, int32_t phase) {
  if (index < off.size() && off[index] != 0) return false;
  return a.phases.empty() || std::find(a.phases.begin(), a.phases.end(), phase) != a.phases.end();
}

double ArenaRadiusAt(double fromR, double toR, double overMs, double elapsedMs) {
  if (!(overMs > 0)) return toR;
  const double t = Clamp(elapsedMs / overMs, 0.0, 1.0);
  return fromR + (toR - fromR) * t;
}

bool ArenaOutside(Vec2 centre, double radius, Vec2 pos) { return DistSq(centre, pos) > radius * radius; }

double ArenaTickDamage(double heroMaxHp, double dpsPctMaxHp, double tickMs, double difficultyDamageMul) {
  const double raw = heroMaxHp * dpsPctMaxHp / 100.0 * tickMs / 1000.0 * difficultyDamageMul;
  return (std::max)(1.0, std::floor(raw + kPhaseFloorSlack));
}

bool PhaseTableBindsFight(const MonsterDef& def) {
  for (const MonsterPhaseDef& row : def.phases) {
    if (row.arena.present || row.leash.present) return true;
  }
  return false;
}

double PhaseFightReach(const MonsterDef& def, TilePos home, double attackExitMul) {
  double reach = 0;
  const Vec2 h = home.Center();
  for (const MonsterPhaseDef& row : def.phases) {
    if (row.arena.present) {
      const double off = row.arena.hasCentre ? Dist(row.arena.centre.Center(), h) : 0.0;
      reach = (std::max)(reach, off + (std::max)(row.arena.fromR, row.arena.toR) + def.attackRange * attackExitMul);
    }
    if (row.leash.present) {
      const double off = row.leash.hasCentre ? Dist(row.leash.centre.Center(), h) : 0.0;
      reach = (std::max)(reach, off + row.leash.radius + kTetherSlackTiles);
    }
  }
  return reach;
}

Vec2 TetherGoal(Vec2 centre, double radius, Vec2 target) {
  const Vec2 d = target - centre;
  const double len2 = d.LengthSq();
  if (len2 <= radius * radius) return target;
  const double len = std::sqrt(len2);
  if (!(len > 0)) return centre;
  return centre + d * (radius / len);
}

double PhaseScaledDamage(double amount, double mul) {
  if (mul == 1.0) return amount;
  return (std::max)((std::min)(amount, 1.0), std::floor(amount * mul + kPhaseFloorSlack));
}

std::string_view PhaseActionClip(AnimAction action) {
  switch (action) {
    case AnimAction::Attack: return "Attack01";
    case AnimAction::Cast: return "Cast01";
    case AnimAction::Idle: return "Idle";
    case AnimAction::Walk: return "Walk";
    case AnimAction::Hurt: return "Hurt";
    case AnimAction::Dodge: return "Dodge";
    case AnimAction::Death: return "Death";
    case AnimAction::Signature: return "Signature";
    case AnimAction::Work: return "Work";
    case AnimAction::Talk: return "Talk";
    case AnimAction::Roar: return "Roar";
    case AnimAction::Taunt: return "Taunt";
    case AnimAction::Stun: return "Stun";
    case AnimAction::Attack02: return "Attack02";
    case AnimAction::Cast02: return "Cast02";
    case AnimAction::Rise: return "Rise";
  }
  return "Roar";
}

}  // namespace abyss
