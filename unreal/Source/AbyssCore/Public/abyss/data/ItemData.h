// Items, affixes, sets, legendaries, economy, loot rules, crafting, shops.
// Sources: item_bases.json, affixes.json, sets.json, legendaries.json, economy.json, loot_rules.json, crafting.json,
// shops.json. Spec: loot-items-inventory.md 2-5, 10-13.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Stats.h"
#include "abyss/base/Types.h"

namespace abyss {

enum class ConsumableEffect : uint8_t { None, Heal, Mana, Antidote, Teleport };
ABYSS_ENUM_STRINGS(ConsumableEffect, "none", "heal", "mana", "antidote", "teleport")

enum class PotionKind : uint8_t { Hp, Mp };
ABYSS_ENUM_STRINGS(PotionKind, "hp", "mp")

// ItemBase + WeaponBase/ArmorBase/AccessoryBase fields (loot 2.2). "Is equipment" = hasSlot.
struct ItemBaseDef {
  std::string id;
  std::string name, nameEn, description;  // zh-CN fallbacks (i18n data.item.<id>.name/.desc)
  ItemType type = ItemType::Material;
  bool hasSlot = false;
  EquipSlot slot = EquipSlot::Weapon;
  std::string icon;
  int32_t levelReq = 1;
  int32_t sellPrice = 0;
  bool stackable = false;
  int32_t maxStack = 1;
  // weapon (and shield): baseDamage/attackSpeed inert in milestone 1 (C10)
  bool hasBaseDamage = false;
  int32_t baseDamageMin = 0, baseDamageMax = 0;
  int32_t attackSpeedMs = 0;
  bool hasWeaponType = false;
  WeaponType weaponType = WeaponType::Sword;
  // armour
  bool hasBaseDefense = false;
  int32_t baseDefense = 0;
  int32_t sockets = 0;  // base socket count (0 for accessories)
  // consumables (InventorySystem.useConsumable) and ground potions
  ConsumableEffect consumableEffect = ConsumableEffect::None;
  double consumableValue = 0;
  bool isGroundPotion = false;
  PotionKind groundPotionKind = PotionKind::Hp;
  int32_t groundPotionAmount = 0;
  // gems (GEM_STAT_MAP)
  bool isGem = false;
  Stat gemStat = Stat::Str;
  double gemValue = 0;
  int32_t gemTier = 0;
  // group the base came from ("weapons", "armors", ...) and its index in the AllItemBases order
  std::string group;
  int32_t orderIndex = 0;

  bool IsEquipment() const { return hasSlot; }
};

enum class AffixKind : uint8_t { Prefix, Suffix };
ABYSS_ENUM_STRINGS(AffixKind, "prefix", "suffix")

struct AffixDef {
  std::string id;
  std::string name, nameEn;
  AffixKind kind = AffixKind::Prefix;
  int32_t tier = 1;
  Stat stat = Stat::Damage;
  int32_t minValue = 0, maxValue = 0;
  int32_t levelReq = 1;
  bool hasAllowedSlots = false;
  std::vector<EquipSlot> allowedSlots;
};

struct StatDisplayDef {
  Stat stat = Stat::Damage;
  std::string label;  // zh-CN fallback (i18n ui.stat.<key>)
  bool isPercent = false;
};

// {affixId, name, stat, value} - fixed affixes of sets/legendaries (also the ItemAffix save shape).
struct FixedAffixDef {
  std::string affixId;
  std::string name;
  Stat stat = Stat::Damage;
  double value = 0;
};

struct SetBonusDef {
  int32_t count = 0;
  std::string description;  // zh-CN fallback (i18n data.set.<id>.bonus.<count>)
  StatBag stats;
};

struct SetPieceAffixes {
  std::string pieceId;
  std::vector<FixedAffixDef> affixes;
};

struct ABYSS_API SetDef {
  std::string id;
  std::string name, nameEn;
  std::vector<std::string> pieces;  // piece ids
  std::vector<SetPieceAffixes> pieceAffixes;
  std::vector<SetBonusDef> bonuses;  // cumulative
  bool dungeonExclusive = false;

  const std::vector<FixedAffixDef>* AffixesForPiece(std::string_view pieceId) const;
};

struct SetPieceBase {
  std::string pieceId;
  std::string baseId;
  bool dungeonExclusive = false;
};

struct LegendaryDef {
  std::string id;
  std::string baseId;
  std::string name, nameEn;
  std::vector<FixedAffixDef> fixedAffixes;
  std::string specialEffect;  // C11: applied as an item stat when it names an EquipStats key with a combat consumer
  bool hasSpecialEffectValue = false;
  double specialEffectValue = 0;
  std::string specialEffectDescription;
  bool dungeonExclusive = false;
};

// economy.json (loot 11-12, I9).
struct EconomyDef {
  double buyPriceMultiplier = 3;
  double buybackPriceMultiplier = 5;
  int32_t buybackSlots = 5;
  std::array<double, EnumCount<ItemQuality>()> sellQualityMultiplier{};
  int32_t bagCapacity = 100;
  int32_t stashBaseSlots = 80;
  std::vector<int32_t> stashSlotsPerWarehouseLevel;
  std::array<int32_t, EnumCount<ItemQuality>()> sortOrderQuality{};
  std::array<int32_t, EnumCount<ItemType>()> sortOrderType{};
};

// loot_rules.json (loot 5).
struct QualityThresholdDef {
  ItemQuality quality = ItemQuality::Normal;
  double base = 0, lm = 0, em = 0, affix = 0, levelOver20 = 0;
};

struct AffixTierBand {
  bool hasBelowLevel = false;
  int32_t belowLevel = 0;
  int32_t minTier = 1, maxTier = 1;
};

struct LootRulesDef {
  double equipmentChanceNormal = 40, equipmentChanceElite = 80, equipmentLuckFactor = 0.5;
  double eliteSecondChanceBase = 50, eliteSecondLuckFactor = 0.5;
  double affixThirdMinAffixBonus = 10, affixThirdBase = 30, affixThirdLuckFactor = 0.5, affixThirdAffixFactor = 1;
  ItemQuality miniBossFloorZone = ItemQuality::Magic;
  ItemQuality miniBossFloorSubDungeon = ItemQuality::Rare;
  double consumableChance = 30;
  double gemChanceBase = 5, gemChanceLuckFactor = 0.1, gemChanceAffixFactor = 0.3;
  double leyFruitChanceElite = 0.12, leyFruitChanceOther = 0.015;
  std::string leyFruitItemId = "c_ley_fruit";
  // quality roll
  double qualityLuckFactor = 0.3;
  double qualityEliteBonus = 15;
  std::vector<QualityThresholdDef> qualityThresholds;  // first match wins (legendary, set, rare, magic)
  // base windows [L + below, L + above]
  int32_t equipmentWindowBelow = -10, equipmentWindowAbove = 3;
  int32_t wideWindowBelow = -20, wideWindowAbove = 5;
  int32_t setPieceWindowBelow = -15, setPieceWindowAbove = 5;
  int32_t windowMinLevel = 1;
  int32_t levelGateConsumable = 5, levelGateGem = 5, levelGateAffix = 5;
  // affix counts [min, max]
  int32_t magicAffixMin = 1, magicAffixMax = 2;
  int32_t rareAffixMin = 3, rareAffixMax = 4;
  int32_t genericLegendaryAffixMin = 3, genericLegendaryAffixMax = 5;
  int32_t setPieceExtraMin = 1, setPieceExtraMax = 2;
  int32_t genericSetMin = 2, genericSetMax = 3;
  std::vector<AffixTierBand> affixTierBands;
  int32_t affixTierSlack = 1;
  int32_t affixWeightInBand = 3, affixWeightOutOfBand = 1;
  double legendaryScaleDivisor = 35, legendaryScaleMin = 0.6, legendaryScaleMax = 1.5;
  int32_t droppedConsumableLevel = 1, droppedConsumableQtyMin = 1, droppedConsumableQtyMax = 3;
  int32_t droppedGemLevel = 1, droppedGemQty = 1;
  double groundItemLifetimeMs = 60000;
  double potionPickupLifetimeMs = 30000;
  double pickupRadiusSq = 4;
  double clickHitBoxTiles = 1.5;
  double autoLootIntervalMs = 300;
  // quest reward choices (loot 5.6)
  std::array<std::vector<WeaponType>, EnumCount<ClassId>()> classWeaponTypes{};
  std::vector<WeaponType> unknownClassWeaponTypes;
  std::vector<ClassId> shieldClasses;
  int32_t rewardLevelHeadroom = 2;
  int32_t rewardTopCandidates = 3;
  ItemQuality rewardQualityMain = ItemQuality::Rare;
  ItemQuality rewardQualitySide = ItemQuality::Magic;
  double fallbackCollectChance = 0.25;
};

// crafting.json (loot 13).
enum class CraftAction : uint8_t { Salvage, Reforge, Upgrade, Socket };
ABYSS_ENUM_STRINGS(CraftAction, "salvage", "reforge", "upgrade", "socket")

struct CraftMaterialCost {
  std::string itemId;
  int32_t count = 0;
};

struct CraftCostDef {
  bool allowed = false;  // null in JSON = action does not apply
  int32_t goldUnits = 0;
  std::vector<CraftMaterialCost> materials;
};

struct SalvageYieldTerm {
  int32_t base = 0;
  bool hasPerLevelDiv = false;
  int32_t perLevelDiv = 0;
};

struct CraftingDef {
  std::vector<std::string> materials;  // m_scrap, m_dust, m_essence
  double goldUnitMul = 6, goldUnitAdd = 5;
  int32_t goldUnitLevelMin = 1;
  // costs[action][quality][0 = weaponOrArmor, 1 = accessory]
  std::array<std::array<std::array<CraftCostDef, 2>, EnumCount<ItemQuality>()>, EnumCount<CraftAction>()> costs{};
  SalvageYieldTerm scrap;
  int32_t scrapNormalBonus = 1;
  SalvageYieldTerm magicDust;
  SalvageYieldTerm rareDust, rareEssence;
  SalvageYieldTerm legendaryDust, legendaryEssence;
  int32_t rerollMagicMin = 1, rerollMagicMax = 2, rerollRareMin = 3, rerollRareMax = 4;
  int32_t maxItemSockets = 3;
  int32_t maxBonusSockets = 1;
  int32_t bagCapacity = 100;

  const CraftCostDef& Cost(CraftAction a, ItemQuality q, bool accessory) const {
    return costs[static_cast<size_t>(a)][static_cast<size_t>(q)][accessory ? 1 : 0];
  }
};

struct ShopDef {
  std::string npcId;
  std::vector<std::string> items;  // base ids, display order
};

struct WanderingMerchantDef {
  std::string zoneId;
  std::vector<std::string> items;  // NOT item base ids (web quirk W10 data); resolved by the events area
};

struct ABYSS_API ItemTables {
  std::vector<ItemBaseDef> bases;  // AllItemBases order (weapons, armors, accessories, consumables, gems, materials)
  std::vector<std::string> equipmentPoolOrder;  // group names
  std::vector<Stat> allStatsExpandsTo;
  std::vector<std::string> materialIds;
  std::vector<AffixDef> prefixes;  // pool order
  std::vector<AffixDef> suffixes;  // pool order
  std::vector<StatDisplayDef> statDisplay;
  std::vector<SetDef> sets;  // overworld then dungeon
  std::vector<SetPieceBase> setPieceBases;
  std::vector<LegendaryDef> legendaries;  // overworld first
  std::vector<std::string> appliedSpecialEffects;  // C11
  EconomyDef economy;
  LootRulesDef loot;
  CraftingDef crafting;
  std::vector<ShopDef> shops;
  std::vector<WanderingMerchantDef> wanderingMerchant;

  const ItemBaseDef* FindBase(std::string_view id) const;
  int32_t BaseIndex(std::string_view id) const;
  const AffixDef* FindAffix(std::string_view id) const;  // prefixes then suffixes
  const SetDef* FindSet(std::string_view id) const;
  const SetPieceBase* FindSetPiece(std::string_view pieceId) const;
  const LegendaryDef* FindLegendary(std::string_view id) const;
  const LegendaryDef* FindLegendaryForBase(std::string_view baseId) const;  // first match (overworld first)
  const ShopDef* FindShop(std::string_view npcId) const;

  IdIndex baseIndex;  // built at load
};

}  // namespace abyss
