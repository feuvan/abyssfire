// Item creation and loot generation (pure).
// Spec: loot-items-inventory.md 4.1-4.5 (createItem, addRandomAffixes, makeLegendary, set items, naming and stats),
// 5.2-5.6 (generateLoot, rollQuality, base selection, other sources, quest pick-one gear), 5.8 (RNG draw order; parity
// with JS not required); DECISIONS I1 (gear magicFind and lck count toward loot luck, same x0.5 / x0.3 coefficients),
// I2 (always identified), I4 / I2 (removed TP / ID scrolls never drop), I11 (dungeon-exclusive items stay out of
// overworld drops).
//
// Owner area: items. Every function draws from the Rng passed in (RngStream::Loot) and assigns uids from the
// ItemUidGenerator passed in (saved in v4 saves).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
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
  // I11: dungeon-exclusive set pieces and legendaries only take part when true (the Abyss Labyrinth, later milestone).
  bool dungeon = false;
};

// createItem (4.1): nullopt for an unknown base. magic: 1..2+extra affixes; rare: 3..4+extra; legendary: fixed
// affixes (extra ignored) or a generic legendary (3..5 random affixes); set: the first set piece built on that base
// (piece affixes + 1..2 random, port 4.4), else 2..3 random affixes without a set; then name + stats. quantity 1,
// identified.
ABYSS_API std::optional<ItemInstance> CreateItem(const LootContext& ctx, std::string_view baseId, int32_t level,
                                                 ItemQuality quality, int32_t extraAffixes = 0);
// addRandomAffixes (4.2) / crafting rollAffixes: keeps existing affixes, rolls count in [min, max] new ones.
ABYSS_API void AddRandomAffixes(const LootContext& ctx, ItemInstance& item, int32_t level, int32_t minCount, int32_t maxCount);
// refreshItem (4.5): normal -> base name; buildItemName (magic / rare); computeStats.
ABYSS_API void FinalizeItem(const DataStore& data, ItemInstance& item);

// Affix tier band of an item level (4.2): ideal [minTier, maxTier]; candidates may be one tier outside (slack).
struct AffixTierRange {
  int32_t minTier = 1, maxTier = 2;  // ideal band (weight inBand)
  int32_t loTier = 1, hiTier = 3;    // allowed (weight outOfBand outside the band)
};
ABYSS_API AffixTierRange AffixTiersForLevel(const DataStore& data, int32_t level);
// The candidate pool of one addRandomAffixes pick (4.2), in table order: levelReq <= L + 5, not in `used`, allowed on
// the base's slot (no slot filter for an unknown base), tier inside the allowed range.
ABYSS_API std::vector<const AffixDef*> AffixPool(const DataStore& data, std::string_view baseId, int32_t level,
                                                 AffixKind kind, const std::vector<std::string>& used);

// Loot roll inputs (5.1): luck = raw hero lck + homestead / pet magicFind + (I1) gear lck + gear magicFind (+ labyrinth
// MF later), then the usual x0.5 (drop chances) / x0.3 (quality) coefficients inside GenerateLoot / RollQuality;
// affixLootBonus = sum of affix lootQualityBonus (+ labyrinth bonus later).
struct LootRollInput {
  int32_t monsterLevel = 1;
  bool elite = false;
  bool isMiniBoss = false;
  bool isSubDungeonMiniBoss = false;
  double luck = 0;
  double affixLootBonus = 0;
  Difficulty difficulty = Difficulty::Normal;
};
// generateLoot (5.2), exact step order and draws (5.8).
ABYSS_API std::vector<ItemInstance> GenerateLoot(const LootContext& ctx, const LootRollInput& in);
// rollQuality (5.3): one Float01 draw, sequential thresholds (QualityThreshold).
ABYSS_API ItemQuality RollQuality(const DataStore& data, int32_t level, double luck, bool isElite, double affixBonus, Rng& rng);
// The rollQuality threshold of one quality (legendary, set, rare or magic): r = rand*100 < threshold. -1 for normal.
ABYSS_API double QualityThreshold(const DataStore& data, ItemQuality q, int32_t level, double luck, bool isElite,
                                  double affixBonus);
// generateEquipment / generateEquipmentWide / generateConsumable / generateGem / generateSetPiece (5.4, 4.4).
ABYSS_API std::optional<ItemInstance> GenerateEquipment(const LootContext& ctx, int32_t level, ItemQuality q, int32_t extra = 0);
ABYSS_API std::optional<ItemInstance> GenerateEquipmentWide(const LootContext& ctx, int32_t level, ItemQuality q);
ABYSS_API std::optional<ItemInstance> GenerateConsumable(const LootContext& ctx, int32_t level);
ABYSS_API std::optional<ItemInstance> GenerateGem(const LootContext& ctx, int32_t level);
ABYSS_API std::optional<ItemInstance> GenerateSetPiece(const LootContext& ctx, int32_t level);
// The base pools those generators pick from (table order), for tests and tools.
ABYSS_API std::vector<const ItemBaseDef*> EquipmentDropPool(const DataStore& data, int32_t level);   // [L-10, L+3]
ABYSS_API std::vector<const ItemBaseDef*> ConsumableDropPool(const DataStore& data, int32_t level);  // levelReq <= L+5
ABYSS_API std::vector<const ItemBaseDef*> GemDropPool(const DataStore& data, int32_t level);         // levelReq <= L+5
struct SetPieceCandidate {
  const SetDef* set = nullptr;
  std::string pieceId;
  const ItemBaseDef* base = nullptr;
};
ABYSS_API std::vector<SetPieceCandidate> SetPieceCandidates(const DataStore& data, int32_t level, bool dungeon);

// Ground potion pickups (5.1 step 6): true for the 5 potion bases, with their kind/amount.
ABYSS_API bool IsGroundPotion(const DataStore& data, std::string_view baseId, PotionKind& kind, int32_t& amount);

// Quest pick-one gear (5.6): itemLevel = max(q.level, min(heroLevel, q.level + 5)); quality = choiceQuality ?? (main ?
// rare : magic); per choice slot: class pool -> usable (levelReq <= itemLevel + 2) -> top 3 by levelReq desc ->
// uniform pick (one Float01 draw). Generated once per quest per session (QuestWorld caches them).
ABYSS_API std::vector<ItemInstance> GenerateQuestRewardChoices(const LootContext& ctx, const QuestDef& quest, ClassId cls,
                                                               int32_t heroLevel);
ABYSS_API int32_t RewardItemLevel(const QuestDef& quest, int32_t heroLevel);
ABYSS_API ItemQuality RewardChoiceQuality(const DataStore& data, const QuestDef& quest);
// pickRewardBase: nullptr when the class pool is empty. Draws once from `rng` when a base is returned.
ABYSS_API const ItemBaseDef* PickRewardBase(const DataStore& data, RewardSlot choice, ClassId cls, int32_t level, Rng& rng);

// Treasure-cache fake definition (5.5): {level Lc = floor((min + max) / 2), elite, affix bonus floor(Lc / 10)}; the
// difficulty is the caller's choice (the web passes the default 'normal', loot Q20 "keep").
ABYSS_API LootRollInput TreasureCacheRoll(int32_t zoneLevelMin, int32_t zoneLevelMax, double luck, Difficulty difficulty);
// The cache's gold range [10 + 5 Lc, 20 + 10 Lc] (world 13.3).
ABYSS_API void TreasureCacheGold(int32_t zoneLevelMin, int32_t zoneLevelMax, int32_t& goldMin, int32_t& goldMax);

}  // namespace abyss
