// RewardService: the one runtime path for exp, gold and item rewards (finding: eight areas change exp or gold).
// Spec: classes-stats-skills.md 5.2 (addExp: one level per call, events, log), 5.3 (exp sources); save-ui-input.md
// 5.1.1 (Dying gate: exp is credited while Dying but a level-up does not refill HP/MP; player transactions and pickups
// are refused); loot-items-inventory.md 7.2 / DECISIONS I10 (full-bag handling: OverflowPolicy), Q5 (chest overflow to
// the stash), FIX Q6 (a purchase into a full bag is refused before paying); world-map-nav.md 13.3 / W11 (event rewards
// use the normal addExp path).
//
// Every area calls this service instead of Hero::AddExp / AddGold / SpendGold (those stay pure state changes):
//   combat (kill exp / gold), quests (turn-in), dialogue (Q1 choice rewards), random events (puzzle / rescue), soul echo
//   (claim refund and death toll), lore (hidden gold pile / chest), shop (buy / sell / buyback / forge), inventory,
//   homestead, debug commands.
// It emits the presentation events (EvExpGained, EvLevelUp, EvGoldChanged with its reason, the sys.player.levelUp log)
// and publishes HeroLevelUpMsg (GameSim wiring: achievements level check, level-up autosave, level-up SFX).
//
// Owner area: hero+combat (exp, gold, levels); GrantItem delegates to the items area (InventorySystem::Grant).
// Runtime system (SimContext): registered in SimSystems::rewards.
#pragma once

#include <cstdint>

#include "abyss/base/Platform.h"
#include "abyss/hero/Hero.h"
#include "abyss/items/Inventory.h"
#include "abyss/items/Item.h"
#include "abyss/sim/SimTypes.h"

namespace abyss {

struct SimContext;

class ABYSS_API RewardService {
 public:
  explicit RewardService(SimContext& ctx);

  // addExp (classes 5.2): Hero::AddExp(max(0, amount)) - ONE level per call, the overflow converts on the next call
  // (GrantExp(0) included). While Dying the exp and level still count but HP/MP are not refilled (5.1.1). Emits, in
  // the web's order: on a level-up EvLevelUp, log sys.player.levelUp {level}, HeroLevelUpMsg; then always EvExpGained.
  LevelUpResult GrantExp(int64_t amount, ExpSource source);
  // Death toll (soul echo): removes exp within the current level only; EvExpGained with the negative amount applied.
  void RemoveExp(int64_t amount, ExpSource source);

  // Gold: delta > 0 adds (saturating), delta < 0 removes (clamped at 0 gold). Player transactions
  // (IsPlayerGoldTransaction) are refused while the hero is Dying. Emits EvGoldChanged{gold, applied delta, reason}
  // when gold changed. Returns the applied delta.
  int64_t ChangeGold(int64_t delta, GoldReason reason);
  // Pays exactly `cost` (>= 0) or nothing: false (no event) when gold is short, cost < 0, or a player transaction while
  // Dying. EvGoldChanged{gold, -cost, reason} on success with cost > 0.
  bool SpendGold(int64_t cost, GoldReason reason);
  bool CanAfford(int64_t cost) const;

  // Item grant through the bag (InventorySystem::Grant): bag first, then the OverflowPolicy (Stash ignores the stash
  // capacity; Refuse leaves `item` untouched with the caller; Lose drops it with the bag-full log). A Pickup while the
  // hero is Dying is Refused (drops stay on the ground, 5.1.1). Pickups also publish ItemPickedMsg + EvItemPicked.
  ItemGrantOutcome GrantItem(ItemInstance& item, OverflowPolicy policy, ItemSource source);

 private:
  SimContext& ctx_;
};

}  // namespace abyss
