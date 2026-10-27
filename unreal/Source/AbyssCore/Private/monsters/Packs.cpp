// Packs, morale, kite, block and buffs: pure rules (dark remake K5, implementation-plan.md 3 K5; monsters.md 2.2-2.5).
#include "abyss/base/Platform.h"

#include "abyss/monsters/Packs.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/Math.h"

namespace abyss {

void ExpandSpawnMembers(const MapSpawnDef& spawn, std::vector<SpawnMember>& out) {
  out.clear();
  if (spawn.mix.empty()) {
    for (int32_t i = 0; i < spawn.count; ++i) out.push_back(SpawnMember{spawn.monsterId, false});
  } else {
    for (const MapSpawnMixDef& e : spawn.mix) {
      for (int32_t i = 0; i < e.count; ++i) out.push_back(SpawnMember{e.monsterId, false});
    }
  }
  if (!spawn.leader.present) return;
  for (SpawnMember& m : out) {
    if (m.monsterId == spawn.leader.monsterId) {
      m.leader = true;
      return;
    }
  }
}

bool BlockArcContains(Vec2 pos, Vec2 heading, double arcDeg, Vec2 from) {
  const Vec2 to = from - pos;
  const double toLen = to.Length();
  const double headLen = heading.Length();
  if (!(toLen > 1e-9) || !(headLen > 1e-9)) return false;
  if (arcDeg >= 360) return true;
  const double cosAngle = (to.x * heading.x + to.y * heading.y) / (toLen * headLen);
  return cosAngle + 1e-9 >= DetCos(arcDeg * kPi / 360.0);
}

double BlockedDamage(double amount, double damageMul) {
  return (std::max)((std::min)(amount, 1.0), std::floor(amount * damageMul));
}

Vec2 WallHeading(std::span<const Vec2> headings) {
  if (headings.empty()) return Vec2(1, 0);
  Vec2 sum;
  for (const Vec2& h : headings) sum += h;
  const double len = sum.Length();
  if (len > 1e-9) return sum / len;
  const double first = headings.front().Length();
  return first > 1e-9 ? headings.front() / first : Vec2(1, 0);
}

double BuffMultiplier(std::span<const MonsterAbilityBuff> buffs, MonsterBuffStat stat, double nowMs, double auraMul,
                      bool hasAura) {
  bool any = hasAura;
  double best = hasAura ? auraMul : 1.0;
  for (const MonsterAbilityBuff& b : buffs) {
    if (b.stat != stat || !(b.untilMs > nowMs)) continue;
    best = any ? (std::max)(best, b.mul) : b.mul;
    any = true;
  }
  return best;
}

bool HasLiveBuff(std::span<const MonsterAbilityBuff> buffs, MonsterBuffStat stat, double nowMs) {
  for (const MonsterAbilityBuff& b : buffs) {
    if (b.stat == stat && b.untilMs > nowMs) return true;
  }
  return false;
}

namespace {
size_t PacksStat(MonsterBuffStat s) { return static_cast<size_t>(s); }
bool PacksAura(const MonsterInstance& m, MonsterBuffStat s) { return (m.auraMask & (1u << PacksStat(s))) != 0; }
}  // namespace

double MonsterDamageBuffMul(const MonsterInstance& m, double nowMs) {
  return BuffMultiplier(m.abilityBuffs, MonsterBuffStat::Damage, nowMs, m.auraMul[PacksStat(MonsterBuffStat::Damage)],
                        PacksAura(m, MonsterBuffStat::Damage));
}

bool MonsterFearImmune(const MonsterInstance& m, double nowMs) {
  return PacksAura(m, MonsterBuffStat::FearImmune) || HasLiveBuff(m.abilityBuffs, MonsterBuffStat::FearImmune, nowMs);
}

void RefreshMonsterBuffs(MonsterInstance& m, double nowMs) {
  if (!m.abilityBuffs.empty()) {
    m.abilityBuffs.erase(std::remove_if(m.abilityBuffs.begin(), m.abilityBuffs.end(),
                                        [nowMs](const MonsterAbilityBuff& b) { return !(b.untilMs > nowMs); }),
                         m.abilityBuffs.end());
  }
  const auto mul = [&](MonsterBuffStat s) {
    return BuffMultiplier(m.abilityBuffs, s, nowMs, m.auraMul[PacksStat(s)], PacksAura(m, s));
  };
  m.buffDamageMul = mul(MonsterBuffStat::Damage);
  m.buffAttackSpeedMul = mul(MonsterBuffStat::AttackSpeed);
  m.buffMoveSpeedMul = mul(MonsterBuffStat::MoveSpeed);
  m.fearImmune = MonsterFearImmune(m, nowMs);
}

uint32_t MonsterBuffMask(const MonsterInstance& m) {
  uint32_t mask = 0;
  if (m.buffDamageMul != 1) mask |= 1u << PacksStat(MonsterBuffStat::Damage);
  if (m.buffAttackSpeedMul != 1) mask |= 1u << PacksStat(MonsterBuffStat::AttackSpeed);
  if (m.buffMoveSpeedMul != 1) mask |= 1u << PacksStat(MonsterBuffStat::MoveSpeed);
  if (m.fearImmune) mask |= 1u << PacksStat(MonsterBuffStat::FearImmune);
  return mask;
}

bool FamilyMatches(const std::string& filter, const MonsterDef& def) { return filter.empty() || filter == def.family; }

bool GrantsFearImmunity(const MonsterDef& def) {
  for (const MonsterAbilityDef& a : def.abilities) {
    if ((a.kind == AbilityKind::Buff || a.kind == AbilityKind::Call) && a.buff.present && a.buff.hasStat &&
        a.buff.stat == MonsterBuffStat::FearImmune) {
      return true;
    }
  }
  return false;
}

}  // namespace abyss
