// RewardService: exp, gold and item rewards through one path (classes-stats-skills.md 5.2-5.3; save-ui-input.md 5.1.1;
// loot-items-inventory.md 7.2; DECISIONS I10, Q5, W11). Implemented (owner area hero+combat).
#include "abyss/base/Platform.h"

#include "abyss/hero/Rewards.h"

#include <algorithm>
#include <limits>

#include "abyss/base/I18n.h"
#include "abyss/base/StrUtil.h"
#include "abyss/sim/SimContext.h"

namespace abyss {

RewardService::RewardService(SimContext& ctx) : ctx_(ctx) {}

LevelUpResult RewardService::GrantExp(int64_t amount, ExpSource source) {
  Hero& hero = *ctx_.sys.hero;
  const int64_t add = (std::max)(int64_t{0}, amount);
  // Hero::AddExp applies the Dying rule itself (exp and level count, no HP / MP refill).
  const LevelUpResult r = hero.AddExp(add, ctx_.equip);
  if (r.leveledUp) {
    ctx_.events.Emit(EvLevelUp{r.level});
    ctx_.events.Log(MakeLoc("sys.player.levelUp", {{"level", ToStr(r.level)}}), LogType::System);
    ctx_.bus.Publish(HeroLevelUpMsg{r.level});
  }
  ctx_.events.Emit(EvExpGained{add, hero.Exp(), hero.ExpToNext(), source});
  return r;
}

void RewardService::RemoveExp(int64_t amount, ExpSource source) {
  Hero& hero = *ctx_.sys.hero;
  const int64_t before = hero.Exp();
  hero.RemoveExp(amount);
  ctx_.events.Emit(EvExpGained{hero.Exp() - before, hero.Exp(), hero.ExpToNext(), source});
}

int64_t RewardService::ChangeGold(int64_t delta, GoldReason reason) {
  Hero& hero = *ctx_.sys.hero;
  if (delta == 0) return 0;
  if (hero.Life() == HeroLife::Dying && IsPlayerGoldTransaction(reason)) return 0;  // 5.1.1
  const int64_t gold = (std::max)(int64_t{0}, hero.Gold());
  int64_t applied = delta;
  if (delta < 0) {
    applied = delta < -gold ? -gold : delta;  // never below 0 gold
  } else if (delta > (std::numeric_limits<int64_t>::max)() - gold) {
    applied = (std::numeric_limits<int64_t>::max)() - gold;  // saturate
  }
  if (applied == 0) return 0;
  hero.SetGold(gold + applied);
  ctx_.events.Emit(EvGoldChanged{hero.Gold(), applied, reason});
  return applied;
}

bool RewardService::SpendGold(int64_t cost, GoldReason reason) {
  Hero& hero = *ctx_.sys.hero;
  if (cost < 0) return false;
  if (hero.Life() == HeroLife::Dying && IsPlayerGoldTransaction(reason)) return false;  // 5.1.1
  if (!hero.SpendGold(cost)) return false;
  if (cost > 0) ctx_.events.Emit(EvGoldChanged{hero.Gold(), -cost, reason});
  return true;
}

bool RewardService::CanAfford(int64_t cost) const { return cost >= 0 && ctx_.sys.hero->Gold() >= cost; }

ItemGrantOutcome RewardService::GrantItem(ItemInstance& item, OverflowPolicy policy, ItemSource source) {
  if (source == ItemSource::Pickup && ctx_.sys.hero->Life() != HeroLife::Alive) {
    return ItemGrantOutcome::Refused;  // 5.1.1: drops stay on the ground while Dying
  }
  return ctx_.sys.inventory->Grant(item, policy, source);
}

}  // namespace abyss
