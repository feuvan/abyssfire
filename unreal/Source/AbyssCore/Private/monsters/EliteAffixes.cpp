// Elite affix rolling and behaviours (combat-feel.md 17.2, 17.4). Owner area: monsters.
#include "abyss/base/Platform.h"

#include "abyss/monsters/EliteAffixes.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/Math.h"
#include "abyss/monsters/Monster.h"
#include "abyss/monsters/MonsterDefs.h"

namespace abyss {

std::vector<EliteAffixType> RollEliteAffixes(const EliteAffixTable& table, std::string_view zoneId, Rng& rng) {
  int32_t lo = 1, hi = 1;
  table.CountFor(zoneId, lo, hi);
  if (hi < lo) hi = lo;
  const int32_t count = rng.RandomInt(lo, hi);
  // selectAffixes: without replacement over the table order (idx = randomInt(0, len - 1), splice).
  std::vector<EliteAffixType> available = table.order;
  std::vector<EliteAffixType> selected;
  const int32_t actual = (std::min)(count, static_cast<int32_t>(available.size()));
  for (int32_t i = 0; i < actual; ++i) {
    const int32_t idx = rng.RandomInt(0, static_cast<int32_t>(available.size()) - 1);
    selected.push_back(available[static_cast<size_t>(idx)]);
    available.erase(available.begin() + idx);
  }
  return selected;
}

EliteOnHit EvaluateEliteOnHit(const MonsterInstance& m, const EliteAffixTable& table, double finalDamage, Rng& rng) {
  EliteOnHit out;
  if (m.affixes.empty()) return out;
  std::vector<EliteAffixType> types;
  for (const MonsterAffix& a : m.affixes) types.push_back(a.type);
  const CombinedAffixStats c = CombineAffixes(types, table);
  if (c.extraFireDamage > 0) out.extraFireDamage = std::floor(finalDamage * c.extraFireDamage);
  if (c.lifestealFraction > 0) out.lifestealHeal = std::floor(finalDamage * c.lifestealFraction);
  if (c.freezeChance > 0) out.freezeSlow = rng.Float01() < c.freezeChance;  // the only draw (web order)
  return out;
}

namespace {
MonsterAffix* EliteFindAffix(MonsterInstance& m, EliteAffixType t) {
  for (MonsterAffix& a : m.affixes) {
    if (a.type == t) return &a;
  }
  return nullptr;
}
}  // namespace

bool EliteTeleportTarget(MonsterInstance& m, const EliteAffixTable& table, Vec2 heroPos, double nowMs, int32_t cols,
                         int32_t rows, Rng& rng, TilePos& outTile) {
  MonsterAffix* a = EliteFindAffix(m, EliteAffixType::Teleporting);
  if (a == nullptr) return false;
  if (nowMs - a->lastTeleportMs < table.Def(EliteAffixType::Teleporting).teleportCooldownMs) return false;
  a->lastTeleportMs = nowMs;
  const double dSq = DistSq(m.pos, heroPos);
  if (!(dSq > table.teleportWindowDistSqMin && dSq < table.teleportWindowDistSqMax)) return false;
  // hero +/- (1 + rand) per axis: sign draw then magnitude draw, col first (JS left-to-right evaluation).
  const double span = table.teleportOffsetMax - table.teleportOffsetMin;
  const double signC = rng.Float01() < 0.5 ? -1.0 : 1.0;
  const double offC = signC * (table.teleportOffsetMin + rng.Float01() * span);
  const double signR = rng.Float01() < 0.5 ? -1.0 : 1.0;
  const double offR = signR * (table.teleportOffsetMin + rng.Float01() * span);
  const double hiC = static_cast<double>(cols - 2), hiR = static_cast<double>(rows - 2);
  outTile.col = JsRoundInt((std::max)(1.0, (std::min)(hiC, heroPos.x + offC)));
  outTile.row = JsRoundInt((std::max)(1.0, (std::min)(hiR, heroPos.y + offR)));
  return true;
}

bool EliteCurseAuraInRange(MonsterInstance& m, const EliteAffixTable& table, Vec2 heroPos, double nowMs,
                           bool& logNow) {
  logNow = false;
  MonsterAffix* a = EliteFindAffix(m, EliteAffixType::CurseAura);
  if (a == nullptr) return false;
  const double r = table.Def(EliteAffixType::CurseAura).curseAuraRadius;
  if (DistSq(m.pos, heroPos) > r * r) return false;
  if (nowMs - a->lastCurseLogMs > table.curseLogIntervalMs) {  // strictly more than the interval (web)
    a->lastCurseLogMs = nowMs;
    logNow = true;
  }
  return true;
}

}  // namespace abyss
