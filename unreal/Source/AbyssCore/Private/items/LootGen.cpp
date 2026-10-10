// Item creation and loot generation (loot-items-inventory.md 4-5; I1, I2, I4, I11). Owner area: items.
#include "abyss/base/Platform.h"

#include "abyss/items/LootGen.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/Assert.h"
#include "abyss/base/Math.h"
#include "abyss/data/DataStore.h"

namespace abyss {

namespace {

constexpr int32_t kLgMaxAffixTier = 5;            // tiers are 1..5 (affixes.json)
constexpr int32_t kLgQualityLevelThreshold = 20;  // rollQuality: "(level > 20 ? levelOver20 : 0)"
constexpr int32_t kLgRewardLevelCapAbove = 5;     // rewardItemLevel: min(playerLevel, quest.level + 5)

ItemInstance LgNewInstance(const LootContext& ctx, const ItemBaseDef& base, int32_t level, ItemQuality quality) {
  ItemInstance it;
  ABYSS_ASSERT(ctx.uids != nullptr, "LootContext without a uid generator");
  if (ctx.uids != nullptr) it.uid = ctx.uids->Next();
  it.baseId = base.id;
  it.name = base.name;
  it.quality = quality;
  it.level = level;
  it.identified = true;
  it.quantity = 1;
  return it;
}

ItemAffix LgFromFixed(const FixedAffixDef& f) { return ItemAffix{f.affixId, f.name, f.stat, f.value}; }

// The zh text of an i18n key (stored strings are zh like the web's), else the key itself.
std::string LgZhText(const DataStore& data, std::string_view key) {
  const std::string* s = data.Strings().Lookup(LocaleId::ZhCN, key);
  return s != nullptr ? *s : std::string(key);
}

// makeLegendary (4.3): first legendary of that base (overworld first; dungeon-exclusive ones only in a dungeon, I11).
void LgMakeLegendary(const LootContext& ctx, ItemInstance& item) {
  const ItemTables& t = ctx.data->Items();
  const LootRulesDef& l = t.loot;
  const LegendaryDef* def = nullptr;
  for (const LegendaryDef& d : t.legendaries) {
    if (d.baseId != item.baseId) continue;
    if (d.dungeonExclusive && !ctx.dungeon) continue;
    def = &d;
    break;
  }
  if (def != nullptr) {
    item.name = def->name;
    const double scale =
        (std::max)(l.legendaryScaleMin, (std::min)(l.legendaryScaleMax, item.level / l.legendaryScaleDivisor));
    item.affixes.clear();
    for (const FixedAffixDef& f : def->fixedAffixes) {
      ItemAffix a = LgFromFixed(f);
      a.value = JsRound(f.value * scale);
      item.affixes.push_back(std::move(a));
    }
    item.legendaryEffect = def->specialEffectDescription;
    item.legendaryId = def->id;
    item.identified = true;
  } else {
    // Generic legendary: legendaryId stays empty (that is its marker, Item.h). The stored text is zh like every stored
    // string (15.3); ItemLegendaryEffectText shows sys.loot.genericLegendaryEffect in the player's locale.
    AddRandomAffixes(ctx, item, item.level, l.genericLegendaryAffixMin, l.genericLegendaryAffixMax);
    item.legendaryEffect = LgZhText(*ctx.data, "sys.loot.genericLegendaryEffect");
  }
}

// The set piece built on `baseId` (sets in table order; dungeon-exclusive ones only in a dungeon).
bool LgFindPieceForBase(const LootContext& ctx, std::string_view baseId, const SetDef*& outSet, std::string& outPiece) {
  const ItemTables& t = ctx.data->Items();
  for (const SetDef& s : t.sets) {
    if (s.dungeonExclusive && !ctx.dungeon) continue;
    for (const std::string& pieceId : s.pieces) {
      const SetPieceBase* pb = t.FindSetPiece(pieceId);
      if (pb == nullptr || pb->baseId != baseId) continue;
      if (pb->dungeonExclusive && !ctx.dungeon) continue;
      outSet = &s;
      outPiece = pieceId;
      return true;
    }
  }
  return false;
}

// Turns `item` into set piece `pieceId` of `set` (generateSetPiece body, 4.4): zh name "<set> <base>", the piece's
// fixed affixes (not level-scaled), then setPieceExtra random affixes; no buildItemName.
void LgApplySetPiece(const LootContext& ctx, ItemInstance& item, const SetDef& set, const std::string& pieceId,
                     const ItemBaseDef& base) {
  const LootRulesDef& l = ctx.data->Items().loot;
  item.name = set.name + " " + base.name;
  item.quality = ItemQuality::Set;
  item.setId = set.id;
  item.setPieceId = pieceId;
  item.affixes.clear();
  if (const std::vector<FixedAffixDef>* fixed = set.AffixesForPiece(pieceId)) {
    for (const FixedAffixDef& f : *fixed) item.affixes.push_back(LgFromFixed(f));
  }
  AddRandomAffixes(ctx, item, item.level, l.setPieceExtraMin, l.setPieceExtraMax);
}

// createItem(set) (port of the dead makeSetItem path, 4.4): a piece on this base, else 2..3 random affixes, no set.
void LgMakeSetItem(const LootContext& ctx, ItemInstance& item, const ItemBaseDef& base) {
  const SetDef* set = nullptr;
  std::string pieceId;
  if (LgFindPieceForBase(ctx, base.id, set, pieceId)) {
    LgApplySetPiece(ctx, item, *set, pieceId, base);
    return;
  }
  const LootRulesDef& l = ctx.data->Items().loot;
  AddRandomAffixes(ctx, item, item.level, l.genericSetMin, l.genericSetMax);
}

bool LgSlotAllowed(const AffixDef& a, const ItemBaseDef* base) {
  if (!a.hasAllowedSlots || base == nullptr || !base->hasSlot) return true;
  for (EquipSlot s : a.allowedSlots) {
    if (s == base->slot) return true;
  }
  return false;
}

bool LgUsed(const std::vector<std::string>& used, const std::string& id) {
  for (const std::string& u : used) {
    if (u == id) return true;
  }
  return false;
}

// Equipment bases in AllItemBases group order ([...Weapons, ...Armors, ...Accessories]) within [lo, hi].
std::vector<const ItemBaseDef*> LgEquipmentWindow(const ItemTables& t, int32_t lo, int32_t hi) {
  std::vector<const ItemBaseDef*> out;
  for (const std::string& g : t.equipmentPoolOrder) {
    for (const ItemBaseDef& b : t.bases) {
      if (b.group != g || !b.IsEquipment()) continue;
      if (b.levelReq <= hi && b.levelReq >= lo) out.push_back(&b);
    }
  }
  return out;
}

std::vector<const ItemBaseDef*> LgAllEquipment(const ItemTables& t) {
  std::vector<const ItemBaseDef*> out;
  for (const std::string& g : t.equipmentPoolOrder) {
    for (const ItemBaseDef& b : t.bases) {
      if (b.group == g && b.IsEquipment()) out.push_back(&b);
    }
  }
  return out;
}

const ItemBaseDef* LgPick(const std::vector<const ItemBaseDef*>& pool, Rng& rng) {
  if (pool.empty()) return nullptr;
  return pool[static_cast<size_t>(rng.RandomInt(0, static_cast<int32_t>(pool.size()) - 1))];
}

// enforceMiniBossQualityFloor (5.2 step 4).
void LgEnforceFloor(const LootContext& ctx, std::vector<ItemInstance>& out, int32_t level, ItemQuality floor) {
  for (const ItemInstance& it : out) {
    if (IsEquipmentItem(it, *ctx.data) && QualityMeetsFloor(it.quality, floor)) return;
  }
  std::optional<ItemInstance> item = GenerateEquipment(ctx, level, floor);
  if (!item.has_value()) item = GenerateEquipmentWide(ctx, level, floor);
  if (item.has_value()) out.push_back(std::move(*item));
}

}  // namespace

// ---- item creation ----

std::optional<ItemInstance> CreateItem(const LootContext& ctx, std::string_view baseId, int32_t level,
                                       ItemQuality quality, int32_t extraAffixes) {
  const DataStore& d = *ctx.data;
  const ItemBaseDef* base = d.Items().FindBase(baseId);
  if (base == nullptr) return std::nullopt;
  const LootRulesDef& l = d.Items().loot;
  ItemInstance item = LgNewInstance(ctx, *base, level, quality);
  switch (quality) {
    case ItemQuality::Magic:
      AddRandomAffixes(ctx, item, level, l.magicAffixMin, l.magicAffixMax + extraAffixes);
      break;
    case ItemQuality::Rare:
      AddRandomAffixes(ctx, item, level, l.rareAffixMin, l.rareAffixMax + extraAffixes);
      break;
    case ItemQuality::Legendary:
      LgMakeLegendary(ctx, item);
      break;
    case ItemQuality::Set:
      LgMakeSetItem(ctx, item, *base);
      break;
    case ItemQuality::Normal:
      break;
  }
  FinalizeItem(d, item);
  return item;
}

AffixTierRange AffixTiersForLevel(const DataStore& data, int32_t level) {
  const LootRulesDef& l = data.Items().loot;
  AffixTierRange r;
  for (const AffixTierBand& b : l.affixTierBands) {
    if (!b.hasBelowLevel || level < b.belowLevel) {
      r.minTier = b.minTier;
      r.maxTier = b.maxTier;
      break;
    }
  }
  r.loTier = (std::max)(1, r.minTier - l.affixTierSlack);
  r.hiTier = (std::min)(kLgMaxAffixTier, r.maxTier + l.affixTierSlack);
  return r;
}

std::vector<const AffixDef*> AffixPool(const DataStore& data, std::string_view baseId, int32_t level, AffixKind kind,
                                       const std::vector<std::string>& used) {
  const ItemTables& t = data.Items();
  const ItemBaseDef* base = t.FindBase(baseId);
  const AffixTierRange tiers = AffixTiersForLevel(data, level);
  std::vector<const AffixDef*> pool;
  const std::vector<AffixDef>& table = kind == AffixKind::Prefix ? t.prefixes : t.suffixes;
  for (const AffixDef& a : table) {
    if (a.levelReq > level + t.loot.levelGateAffix) continue;
    if (LgUsed(used, a.id)) continue;
    if (!LgSlotAllowed(a, base)) continue;
    if (a.tier < tiers.loTier || a.tier > tiers.hiTier) continue;
    pool.push_back(&a);
  }
  return pool;
}

void AddRandomAffixes(const LootContext& ctx, ItemInstance& item, int32_t level, int32_t minCount, int32_t maxCount) {
  const DataStore& d = *ctx.data;
  const LootRulesDef& l = d.Items().loot;
  Rng& rng = *ctx.rng;
  const int32_t count = rng.RandomInt(minCount, (std::max)(minCount, maxCount));
  std::vector<std::string> used;
  for (const ItemAffix& a : item.affixes) used.push_back(a.affixId);
  int32_t nPrefix = 0;  // counts only the affixes added by this call
  int32_t nSuffix = 0;
  const AffixTierRange tiers = AffixTiersForLevel(d, level);
  for (int32_t i = 0; i < count; ++i) {
    const bool wantPrefix = nPrefix <= nSuffix;
    const std::vector<const AffixDef*> pool =
        AffixPool(d, item.baseId, level, wantPrefix ? AffixKind::Prefix : AffixKind::Suffix, used);
    if (pool.empty()) continue;  // the next iteration asks for the same side again
    // Weighted list in table order: in-band tiers x inBand, others x outOfBand.
    int32_t total = 0;
    for (const AffixDef* a : pool) {
      total += (a->tier >= tiers.minTier && a->tier <= tiers.maxTier) ? l.affixWeightInBand : l.affixWeightOutOfBand;
    }
    if (total <= 0) continue;
    int32_t k = rng.RandomInt(0, total - 1);
    const AffixDef* pick = pool.back();
    for (const AffixDef* a : pool) {
      const int32_t w =
          (a->tier >= tiers.minTier && a->tier <= tiers.maxTier) ? l.affixWeightInBand : l.affixWeightOutOfBand;
      if (k < w) {
        pick = a;
        break;
      }
      k -= w;
    }
    const int32_t value = rng.RandomInt(pick->minValue, (std::max)(pick->minValue, pick->maxValue));
    item.affixes.push_back(ItemAffix{pick->id, pick->name, pick->stat, static_cast<double>(value)});
    used.push_back(pick->id);
    if (pick->kind == AffixKind::Prefix) {
      ++nPrefix;
    } else {
      ++nSuffix;
    }
  }
}

void FinalizeItem(const DataStore& data, ItemInstance& item) {
  const ItemTables& t = data.Items();
  const ItemBaseDef* base = t.FindBase(item.baseId);
  if (item.quality == ItemQuality::Normal && base != nullptr) item.name = base->name;
  // buildItemName: magic / rare only; first prefix (by the prefix table) + base + " (" first suffix ")".
  if ((item.quality == ItemQuality::Magic || item.quality == ItemQuality::Rare) && base != nullptr) {
    const ItemAffix* prefix = nullptr;
    const ItemAffix* suffix = nullptr;
    for (const ItemAffix& a : item.affixes) {
      const AffixDef* def = t.FindAffix(a.affixId);
      if (def == nullptr) continue;
      if (def->kind == AffixKind::Prefix && prefix == nullptr) prefix = &a;
      if (def->kind == AffixKind::Suffix && suffix == nullptr) suffix = &a;
    }
    std::string name = base->name;
    if (prefix != nullptr) name = prefix->name + name;
    if (suffix != nullptr) name = name + " (" + suffix->name + ")";
    item.name = std::move(name);
  }
  ComputeItemStats(item);
}

// ---- loot generation ----

double QualityThreshold(const DataStore& data, ItemQuality q, int32_t level, double luck, bool isElite,
                        double affixBonus) {
  const LootRulesDef& l = data.Items().loot;
  const double lm = luck * l.qualityLuckFactor;
  const double em = isElite ? l.qualityEliteBonus : 0.0;
  for (const QualityThresholdDef& t : l.qualityThresholds) {
    if (t.quality != q) continue;
    // Same addition order as the web's literals (base + luck + level term + elite + affix).
    return t.base + lm * t.lm + (level > kLgQualityLevelThreshold ? t.levelOver20 : 0.0) + em * t.em +
           affixBonus * t.affix;
  }
  return -1.0;
}

ItemQuality RollQuality(const DataStore& data, int32_t level, double luck, bool isElite, double affixBonus, Rng& rng) {
  const double r = rng.Float01() * 100.0;
  for (const QualityThresholdDef& t : data.Items().loot.qualityThresholds) {
    if (r < QualityThreshold(data, t.quality, level, luck, isElite, affixBonus)) return t.quality;
  }
  return ItemQuality::Normal;
}

std::vector<const ItemBaseDef*> EquipmentDropPool(const DataStore& data, int32_t level) {
  const LootRulesDef& l = data.Items().loot;
  return LgEquipmentWindow(data.Items(), (std::max)(l.windowMinLevel, level + l.equipmentWindowBelow),
                           level + l.equipmentWindowAbove);
}

std::optional<ItemInstance> GenerateEquipment(const LootContext& ctx, int32_t level, ItemQuality q, int32_t extra) {
  if (q == ItemQuality::Set) return GenerateSetPiece(ctx, level);
  const ItemBaseDef* base = LgPick(EquipmentDropPool(*ctx.data, level), *ctx.rng);
  if (base == nullptr) return std::nullopt;
  return CreateItem(ctx, base->id, level, q, extra);
}

std::optional<ItemInstance> GenerateEquipmentWide(const LootContext& ctx, int32_t level, ItemQuality q) {
  const ItemTables& t = ctx.data->Items();
  const LootRulesDef& l = t.loot;
  std::vector<const ItemBaseDef*> pool =
      LgEquipmentWindow(t, (std::max)(l.windowMinLevel, level + l.wideWindowBelow), level + l.wideWindowAbove);
  if (pool.empty()) pool = LgAllEquipment(t);  // last resort: any equipment base
  const ItemBaseDef* base = LgPick(pool, *ctx.rng);
  if (base == nullptr) return std::nullopt;
  return CreateItem(ctx, base->id, level, q);
}

std::vector<const ItemBaseDef*> ConsumableDropPool(const DataStore& data, int32_t level) {
  const ItemTables& t = data.Items();
  std::vector<const ItemBaseDef*> out;
  for (const ItemBaseDef& b : t.bases) {
    if (b.group != "consumables" || b.levelReq > level + t.loot.levelGateConsumable) continue;
    if (t.IsRemovedItem(b.id)) continue;  // I4 / I2: TP and ID scrolls are gone
    out.push_back(&b);
  }
  return out;
}

std::vector<const ItemBaseDef*> GemDropPool(const DataStore& data, int32_t level) {
  const ItemTables& t = data.Items();
  std::vector<const ItemBaseDef*> out;
  for (const ItemBaseDef& b : t.bases) {
    if (b.group != "gems" || b.levelReq > level + t.loot.levelGateGem) continue;
    if (t.IsRemovedItem(b.id)) continue;
    out.push_back(&b);
  }
  return out;
}

std::optional<ItemInstance> GenerateConsumable(const LootContext& ctx, int32_t level) {
  const LootRulesDef& l = ctx.data->Items().loot;
  const ItemBaseDef* base = LgPick(ConsumableDropPool(*ctx.data, level), *ctx.rng);
  if (base == nullptr) return std::nullopt;
  ItemInstance it = LgNewInstance(ctx, *base, l.droppedConsumableLevel, ItemQuality::Normal);
  it.quantity = ctx.rng->RandomInt(l.droppedConsumableQtyMin, (std::max)(l.droppedConsumableQtyMin, l.droppedConsumableQtyMax));
  ComputeItemStats(it);
  return it;
}

std::optional<ItemInstance> GenerateGem(const LootContext& ctx, int32_t level) {
  const LootRulesDef& l = ctx.data->Items().loot;
  const ItemBaseDef* base = LgPick(GemDropPool(*ctx.data, level), *ctx.rng);
  if (base == nullptr) return std::nullopt;
  ItemInstance it = LgNewInstance(ctx, *base, l.droppedGemLevel, ItemQuality::Normal);
  it.quantity = l.droppedGemQty;
  ComputeItemStats(it);
  return it;
}

std::vector<SetPieceCandidate> SetPieceCandidates(const DataStore& data, int32_t level, bool dungeon) {
  const ItemTables& t = data.Items();
  const LootRulesDef& l = t.loot;
  const int32_t lo = (std::max)(l.windowMinLevel, level + l.setPieceWindowBelow);
  const int32_t hi = level + l.setPieceWindowAbove;
  std::vector<SetPieceCandidate> out;
  for (const SetDef& s : t.sets) {
    if (s.dungeonExclusive && !dungeon) continue;  // I11
    for (const std::string& pieceId : s.pieces) {
      const SetPieceBase* pb = t.FindSetPiece(pieceId);
      if (pb == nullptr || (pb->dungeonExclusive && !dungeon)) continue;
      const ItemBaseDef* base = t.FindBase(pb->baseId);
      if (base == nullptr) continue;
      if (base->levelReq <= hi && base->levelReq >= lo) out.push_back(SetPieceCandidate{&s, pieceId, base});
    }
  }
  return out;
}

std::optional<ItemInstance> GenerateSetPiece(const LootContext& ctx, int32_t level) {
  const std::vector<SetPieceCandidate> c = SetPieceCandidates(*ctx.data, level, ctx.dungeon);
  if (c.empty()) return std::nullopt;  // the drop is simply lost (web)
  const SetPieceCandidate& pick = c[static_cast<size_t>(ctx.rng->RandomInt(0, static_cast<int32_t>(c.size()) - 1))];
  ItemInstance item = LgNewInstance(ctx, *pick.base, level, ItemQuality::Set);
  LgApplySetPiece(ctx, item, *pick.set, pick.pieceId, *pick.base);
  ComputeItemStats(item);
  return item;
}

std::vector<ItemInstance> GenerateLoot(const LootContext& ctx, const LootRollInput& in) {
  const DataStore& d = *ctx.data;
  const LootRulesDef& l = d.Items().loot;
  const DifficultyDef& mods = d.Combat().difficulty.defs[EnumIndex(in.difficulty)];
  Rng& rng = *ctx.rng;
  const int32_t level = in.monsterLevel + mods.lootLevelBonus;
  const double lb = in.luck * l.equipmentLuckFactor;
  const double qb = in.affixLootBonus + mods.lootQualityBonus;
  std::vector<ItemInstance> out;
  auto equipmentDrop = [&](bool elite) {
    const ItemQuality q = RollQuality(d, level, in.luck, elite, qb, rng);
    std::optional<ItemInstance> it = GenerateEquipment(ctx, level, q, mods.lootExtraAffixes);
    if (it.has_value()) out.push_back(std::move(*it));
  };
  // 1) main equipment drop (a non-elite monster rolls quality with isElite = false)
  if (rng.Chance((in.elite ? l.equipmentChanceElite : l.equipmentChanceNormal) + lb)) equipmentDrop(in.elite);
  // 2) elite second drop
  if (in.elite && rng.Chance(l.eliteSecondChanceBase + in.luck * l.eliteSecondLuckFactor)) equipmentDrop(true);
  // 3) affix-elite third drop (the gate uses the affix bonus WITHOUT the difficulty bonus)
  if (in.affixLootBonus >= l.affixThirdMinAffixBonus &&
      rng.Chance(l.affixThirdBase + in.luck * l.affixThirdLuckFactor + in.affixLootBonus * l.affixThirdAffixFactor)) {
    equipmentDrop(true);
  }
  // 4) mini-boss quality floor
  if (in.isMiniBoss) {
    LgEnforceFloor(ctx, out, level, in.isSubDungeonMiniBoss ? l.miniBossFloorSubDungeon : l.miniBossFloorZone);
  }
  // 5) consumable
  if (rng.Chance(l.consumableChance)) {
    std::optional<ItemInstance> it = GenerateConsumable(ctx, level);
    if (it.has_value()) out.push_back(std::move(*it));
  }
  // 6) gem: 5 + (luck * 0.5) * 0.2 + affix * 0.3 (gemChance.luckFactor 0.1 is per point of luck; dividing it by the
  //    0.5 drop-luck factor is exact, so this is the web's luckBonus * 0.2).
  const double gemLuck = l.equipmentLuckFactor != 0 ? lb * (l.gemChanceLuckFactor / l.equipmentLuckFactor)
                                                    : in.luck * l.gemChanceLuckFactor;
  if (rng.Chance(l.gemChanceBase + gemLuck + in.affixLootBonus * l.gemChanceAffixFactor)) {
    std::optional<ItemInstance> it = GenerateGem(ctx, level);
    if (it.has_value()) out.push_back(std::move(*it));
  }
  return out;
}

bool IsGroundPotion(const DataStore& data, std::string_view baseId, PotionKind& kind, int32_t& amount) {
  const ItemBaseDef* b = data.Items().FindBase(baseId);
  if (b == nullptr || !b->isGroundPotion) return false;
  kind = b->groundPotionKind;
  amount = b->groundPotionAmount;
  return true;
}

// ---- quest pick-one gear (5.6) ----

int32_t RewardItemLevel(const QuestDef& quest, int32_t heroLevel) {
  return (std::max)(quest.level, (std::min)(heroLevel, quest.level + kLgRewardLevelCapAbove));
}

ItemQuality RewardChoiceQuality(const DataStore& data, const QuestDef& quest) {
  if (quest.rewards.hasChoiceQuality) return quest.rewards.choiceQuality;
  const LootRulesDef& l = data.Items().loot;
  return quest.category == QuestCategory::Main ? l.rewardQualityMain : l.rewardQualitySide;
}

const ItemBaseDef* PickRewardBase(const DataStore& data, RewardSlot choice, ClassId cls, int32_t level, Rng& rng,
                                  int32_t levelReqCap) {
  const ItemTables& t = data.Items();
  const LootRulesDef& l = t.loot;
  std::vector<const ItemBaseDef*> pool;
  auto group = [&](std::string_view g, auto&& pred) {
    for (const ItemBaseDef& b : t.bases) {
      if (b.group == g && pred(b)) pool.push_back(&b);
    }
  };
  switch (choice) {
    case RewardSlot::Weapon: {
      const std::vector<WeaponType>& types = l.classWeaponTypes[EnumIndex(cls)].empty()
                                                 ? l.unknownClassWeaponTypes
                                                 : l.classWeaponTypes[EnumIndex(cls)];
      group("weapons", [&](const ItemBaseDef& b) {
        if (!b.hasSlot || b.slot != EquipSlot::Weapon || !b.hasWeaponType) return false;
        return std::find(types.begin(), types.end(), b.weaponType) != types.end();
      });
      break;
    }
    case RewardSlot::Offhand:
      if (std::find(l.shieldClasses.begin(), l.shieldClasses.end(), cls) != l.shieldClasses.end()) {
        group("weapons", [](const ItemBaseDef& b) { return b.hasSlot && b.slot == EquipSlot::Offhand; });
      } else {
        group("accessories", [](const ItemBaseDef&) { return true; });
      }
      break;
    case RewardSlot::Jewelry:
      group("accessories", [](const ItemBaseDef&) { return true; });
      break;
    case RewardSlot::Armor:
    case RewardSlot::Helmet:
    case RewardSlot::Gloves:
    case RewardSlot::Boots:
    case RewardSlot::Belt: {
      EquipSlot slot = EquipSlot::Armor;
      if (choice == RewardSlot::Helmet) slot = EquipSlot::Helmet;
      if (choice == RewardSlot::Gloves) slot = EquipSlot::Gloves;
      if (choice == RewardSlot::Boots) slot = EquipSlot::Boots;
      if (choice == RewardSlot::Belt) slot = EquipSlot::Belt;
      group("armors", [slot](const ItemBaseDef& b) { return b.hasSlot && b.slot == slot; });
      break;
    }
  }
  if (pool.empty()) return nullptr;
  std::vector<const ItemBaseDef*> ranked;
  const int32_t usableMax = (std::min)(level + l.rewardLevelHeadroom, levelReqCap);
  for (const ItemBaseDef* b : pool) {
    if (b->levelReq <= usableMax) ranked.push_back(b);
  }
  if (ranked.empty()) {
    // The pool's lowest-levelReq base (first of equals, stable sort ascending).
    const ItemBaseDef* low = pool.front();
    for (const ItemBaseDef* b : pool) {
      if (b->levelReq < low->levelReq) low = b;
    }
    ranked.push_back(low);
  }
  std::stable_sort(ranked.begin(), ranked.end(),
                   [](const ItemBaseDef* a, const ItemBaseDef* b) { return a->levelReq > b->levelReq; });
  if (ranked.size() > static_cast<size_t>((std::max)(1, l.rewardTopCandidates))) {
    ranked.resize(static_cast<size_t>((std::max)(1, l.rewardTopCandidates)));
  }
  return ranked[rng.Index(ranked.size())];
}

std::vector<ItemInstance> GenerateQuestRewardChoices(const LootContext& ctx, const QuestDef& quest, ClassId cls,
                                                     int32_t heroLevel) {
  std::vector<ItemInstance> out;
  const int32_t level = RewardItemLevel(quest, heroLevel);
  const ItemQuality quality = RewardChoiceQuality(*ctx.data, quest);
  for (RewardSlot choice : quest.rewards.choices) {
    // I3 enforces levelReq on equip: never offer a piece the hero cannot wear at turn-in (the hero level only rises
    // after the choices were generated).
    const int32_t cap = ctx.data->Items().loot.rewardCapUsableAtHeroLevel ? (std::max)(1, heroLevel) : INT32_MAX;
    const ItemBaseDef* base = PickRewardBase(*ctx.data, choice, cls, level, *ctx.rng, cap);
    if (base == nullptr) continue;
    std::optional<ItemInstance> item = CreateItem(ctx, base->id, level, quality);
    if (!item.has_value()) continue;
    item->identified = true;
    out.push_back(std::move(*item));
  }
  return out;
}

// ---- random-event treasure cache (5.5) ----

LootRollInput TreasureCacheRoll(int32_t zoneLevelMin, int32_t zoneLevelMax, double luck, Difficulty difficulty) {
  const int32_t lc = FloorInt((zoneLevelMin + zoneLevelMax) / 2.0);
  LootRollInput in;
  in.monsterLevel = lc;
  in.elite = true;
  in.luck = luck;
  in.affixLootBonus = std::floor(lc / 10.0);
  in.difficulty = difficulty;
  return in;
}

void TreasureCacheGold(int32_t zoneLevelMin, int32_t zoneLevelMax, int32_t& goldMin, int32_t& goldMax) {
  const int32_t lc = FloorInt((zoneLevelMin + zoneLevelMax) / 2.0);
  goldMin = 10 + 5 * lc;
  goldMax = 20 + 10 * lc;
}

}  // namespace abyss
