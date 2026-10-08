// Item creation and loot generation (pure).
// Spec: loot-items-inventory.md 4.1-4.5 (createItem, addRandomAffixes, makeLegendary, set items, naming and stats),
// 5.2-5.6 (generateLoot, rollQuality, base selection, other sources, quest pick-one gear), 5.8 (RNG draw order; parity
// with JS not required); DECISIONS I1 (gear magicFind and lck count toward loot luck with x0.5 / x0.3), I2 (always
// identified), I11 (dungeon-exclusive items stay out of overworld drops).
//
// Owner area: items. Every function draws from the Rng passed in (RngStream::Loot) and assigns uids from the
// ItemUidGenerator passed in (saved in v4 saves).
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Rng.h"
#include "abyss/data/ItemData.h"
#include "abyss/data/MonsterData.h"
#include "abyss/data/QuestData.h"
#include "abyss/items/Item.h"

namespace abyss {

class DataStore;

// Everything an item generator needs.
struct LootContext {
  const DataStore* data = nullptr;
  Rng* rng = nullptr;
  ItemUidGenerator* uids = nullptr;
};

// createItem (4.1): nullopt for an unknown base. magic: 1..2+extra affixes; rare: 3..4+extra; legendary: fixed
// affixes (extra ignored); set: set piece affixes; then name + stats. quantity 1, identified.
ABYSS_API std::optional<ItemInstance> CreateItem(const LootContext& ctx, std::string_view baseId, int32_t level,
                                                 ItemQuality quality, int32_t extraAffixes = 0);
// addRandomAffixes (4.2) / crafting rollAffixes: keeps existing affixes, rolls count in [min, max] new ones.
ABYSS_API void AddRandomAffixes(const LootContext& ctx, ItemInstance& item, int32_t level, int32_t minCount, int32_t maxCount);
// buildItemName + computeStats (4.5).
ABYSS_API void FinalizeItem(const DataStore& data, ItemInstance& item);

// Loot roll inputs (5.1): luck = raw hero lck + homestead/pet magicFind (+ I1: gear magicFind * 0.5 + gear lck * 0.3)
// (+ labyrinth MF later); affixLootBonus = sum of affix lootQualityBonus (+ labyrinth bonus later).
struct LootRollInput {
  int32_t monsterLevel = 1;
  bool elite = false;
  bool isMiniBoss = false;
  bool isSubDungeonMiniBoss = false;
  double luck = 0;
  double affixLootBonus = 0;
  Difficulty difficulty = Difficulty::Normal;
};
// generateLoot (5.2), exact step order and draws.
ABYSS_API std::vector<ItemInstance> GenerateLoot(const LootContext& ctx, const LootRollInput& in);
// rollQuality (5.3): one Float01 draw, sequential thresholds.
ABYSS_API ItemQuality RollQuality(const DataStore& data, int32_t level, double luck, bool isElite, double affixBonus, Rng& rng);
// generateEquipment / generateEquipmentWide / generateConsumable / generateGem (5.4).
ABYSS_API std::optional<ItemInstance> GenerateEquipment(const LootContext& ctx, int32_t level, ItemQuality q, int32_t extra = 0);
ABYSS_API std::optional<ItemInstance> GenerateEquipmentWide(const LootContext& ctx, int32_t level, ItemQuality q);
ABYSS_API std::optional<ItemInstance> GenerateConsumable(const LootContext& ctx, int32_t level);
ABYSS_API std::optional<ItemInstance> GenerateGem(const LootContext& ctx, int32_t level);
ABYSS_API std::optional<ItemInstance> GenerateSetPiece(const LootContext& ctx, int32_t level);

// Ground potion pickups (5.1 step 6): true for the 5 potion bases, with their kind/amount.
ABYSS_API bool IsGroundPotion(const DataStore& data, std::string_view baseId, PotionKind& kind, int32_t& amount);

// Quest pick-one gear (5.6): itemLevel = max(q.level, min(heroLevel, q.level + 5)); quality = choiceQuality ?? (main ?
// rare : magic); per choice slot: class pool -> usable (levelReq <= itemLevel + 2) -> top 3 by levelReq desc ->
// uniform pick. Generated once per quest per session (QuestWorld caches them).
ABYSS_API std::vector<ItemInstance> GenerateQuestRewardChoices(const LootContext& ctx, const QuestDef& quest, ClassId cls,
                                                               int32_t heroLevel);

// Treasure-cache fake definition (5.5): {level Lc, elite, gold [10 + 5Lc, 20 + 10Lc]}; uses the current difficulty
// (FIX Q20 decision recorded in loot spec 19).
ABYSS_API LootRollInput TreasureCacheRoll(int32_t zoneLevelMin, int32_t zoneLevelMax, double luck, Difficulty difficulty);

}  // namespace abyss
