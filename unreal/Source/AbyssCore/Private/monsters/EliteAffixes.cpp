// Elite affix rolling and behaviours (combat-feel.md 17.2, 17.4). STUB: owner area monsters.
#include "abyss/base/Platform.h"

#include "abyss/monsters/EliteAffixes.h"

#include "abyss/base/Assert.h"
#include "abyss/monsters/Monster.h"

namespace abyss {

std::vector<EliteAffixType> RollEliteAffixes(const EliteAffixTable& table, std::string_view zoneId, Rng& rng) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

EliteOnHit EvaluateEliteOnHit(const MonsterInstance& m, const EliteAffixTable& table, double finalDamage, Rng& rng) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

bool EliteTeleportTarget(MonsterInstance& m, const EliteAffixTable& table, Vec2 heroPos, double nowMs, int32_t cols,
                         int32_t rows, Rng& rng, TilePos& outTile) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

bool EliteCurseAuraInRange(MonsterInstance& m, const EliteAffixTable& table, Vec2 heroPos, double nowMs,
                           bool& logNow) {
  ABYSS_UNIMPLEMENTED();
  logNow = false;
  return false;
}

}  // namespace abyss
