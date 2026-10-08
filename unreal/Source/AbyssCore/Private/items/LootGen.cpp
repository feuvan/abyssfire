// Item creation and loot generation (loot-items-inventory.md 4-5; I1, I2, I11). STUB: owner area items.
#include "abyss/base/Platform.h"

#include "abyss/items/LootGen.h"

#include "abyss/base/Assert.h"
#include "abyss/data/DataStore.h"

namespace abyss {

std::optional<ItemInstance> CreateItem(const LootContext& ctx, std::string_view baseId, int32_t level,
                                       ItemQuality quality, int32_t extraAffixes) {
  ABYSS_UNIMPLEMENTED();
  return std::nullopt;
}

void AddRandomAffixes(const LootContext& ctx, ItemInstance& item, int32_t level, int32_t minCount, int32_t maxCount) {
  ABYSS_UNIMPLEMENTED();
}

void FinalizeItem(const DataStore& data, ItemInstance& item) {
  ABYSS_UNIMPLEMENTED();
  ComputeItemStats(item);
}

std::vector<ItemInstance> GenerateLoot(const LootContext& ctx, const LootRollInput& in) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

ItemQuality RollQuality(const DataStore& data, int32_t level, double luck, bool isElite, double affixBonus,
                        Rng& rng) {
  ABYSS_UNIMPLEMENTED();
  return ItemQuality::Normal;
}

std::optional<ItemInstance> GenerateEquipment(const LootContext& ctx, int32_t level, ItemQuality q, int32_t extra) {
  ABYSS_UNIMPLEMENTED();
  return std::nullopt;
}

std::optional<ItemInstance> GenerateEquipmentWide(const LootContext& ctx, int32_t level, ItemQuality q) {
  ABYSS_UNIMPLEMENTED();
  return std::nullopt;
}

std::optional<ItemInstance> GenerateConsumable(const LootContext& ctx, int32_t level) {
  ABYSS_UNIMPLEMENTED();
  return std::nullopt;
}

std::optional<ItemInstance> GenerateGem(const LootContext& ctx, int32_t level) {
  ABYSS_UNIMPLEMENTED();
  return std::nullopt;
}

std::optional<ItemInstance> GenerateSetPiece(const LootContext& ctx, int32_t level) {
  ABYSS_UNIMPLEMENTED();
  return std::nullopt;
}

bool IsGroundPotion(const DataStore& data, std::string_view baseId, PotionKind& kind, int32_t& amount) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

std::vector<ItemInstance> GenerateQuestRewardChoices(const LootContext& ctx, const QuestDef& quest, ClassId cls,
                                                     int32_t heroLevel) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

LootRollInput TreasureCacheRoll(int32_t zoneLevelMin, int32_t zoneLevelMax, double luck, Difficulty difficulty) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

}  // namespace abyss
