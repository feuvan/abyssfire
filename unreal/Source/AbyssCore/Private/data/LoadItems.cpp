#include "abyss/base/Platform.h"

#include "JsonReader.h"

namespace abyss::dataload {
namespace {

FixedAffixDef ReadFixedAffix(const JNode& n) {
  FixedAffixDef a;
  a.affixId = n.Str("affixId");
  a.name = n.Str("name", "");
  a.stat = n.Enum<Stat>("stat");
  a.value = n.Num("value");
  return a;
}

AffixDef ReadAffix(const JNode& n) {
  AffixDef a;
  a.id = n.Str("id");
  a.name = n.Str("name");
  a.nameEn = n.Str("nameEn", "");
  a.kind = n.Enum<AffixKind>("type");
  a.tier = n.Int("tier");
  a.stat = n.Enum<Stat>("stat");
  a.minValue = n.Int("minValue");
  a.maxValue = n.Int("maxValue");
  a.levelReq = n.Int("levelReq");
  if (n.Has("allowedSlots")) {
    a.hasAllowedSlots = true;
    a.allowedSlots = n.EnumList<EquipSlot>("allowedSlots");
  }
  return a;
}

void ReadPair(const JNode& n, std::string_view key, int32_t& a, int32_t& b) {
  const std::vector<int32_t> v = n.IntList(key);
  if (v.size() != 2) {
    n.Child(key).Error("expected [a, b]");
    return;
  }
  a = v[0];
  b = v[1];
}

CraftCostDef ReadCraftCost(const JNode& n) {
  CraftCostDef c;
  if (!n.Exists()) return c;  // null = not allowed
  c.allowed = true;
  c.goldUnits = n.Int("goldUnits");
  for (const auto& [k, m] : n.Members("materials")) c.materials.push_back(CraftMaterialCost{k, m.AsInt()});
  return c;
}

SalvageYieldTerm ReadYield(const JNode& n) {
  SalvageYieldTerm t;
  t.base = n.Int("base");
  if (n.Has("perLevelDiv")) {
    t.hasPerLevelDiv = true;
    t.perLevelDiv = n.Int("perLevelDiv");
  }
  return t;
}

}  // namespace

void LoadItemBasesFile(const JNode& r, ItemTables& out) {
  out.bases.clear();
  out.baseIndex.Clear();
  out.equipmentPoolOrder = r.StrList("equipmentPoolOrder");
  static constexpr std::string_view kGroups[] = {"weapons", "armors", "accessories", "consumables", "gems",
                                                 "materials"};
  const JNode groups = r.Child("groups");
  for (std::string_view g : kGroups) {
    for (const JNode& n : groups.Items(g)) {
      ItemBaseDef b;
      b.group = std::string(g);
      b.id = n.Str("id");
      b.name = n.Str("name");
      b.nameEn = n.Str("nameEn", "");
      b.description = n.Str("description", "");
      b.type = n.Enum<ItemType>("type");
      if (n.Has("slot")) {
        b.hasSlot = true;
        b.slot = n.Enum<EquipSlot>("slot");
      }
      b.icon = n.Str("icon", "");
      b.levelReq = n.Int("levelReq");
      b.sellPrice = n.Int("sellPrice");
      b.stackable = n.Bool("stackable");
      b.maxStack = n.Int("maxStack");
      if (n.Has("baseDamage")) {
        b.hasBaseDamage = true;
        ReadPair(n, "baseDamage", b.baseDamageMin, b.baseDamageMax);
      }
      b.attackSpeedMs = n.Int("attackSpeed", 0);
      if (n.Has("weaponType")) {
        b.hasWeaponType = true;
        b.weaponType = n.Enum<WeaponType>("weaponType");
      }
      if (n.Has("baseDefense")) {
        b.hasBaseDefense = true;
        b.baseDefense = n.Int("baseDefense");
      }
      b.sockets = n.Int("sockets", 0);
      b.orderIndex = static_cast<int32_t>(out.bases.size());
      if (!out.baseIndex.Add(b.id)) n.Child("id").Error("duplicate item base id '" + b.id + "'");
      out.bases.push_back(std::move(b));
    }
  }
  // AllItemBases order must match the exported `order` list.
  const std::vector<std::string> order = r.StrList("order");
  if (order.size() != out.bases.size()) {
    r.Child("order").Error("order length differs from the groups");
  } else {
    for (size_t i = 0; i < order.size(); ++i) {
      if (order[i] != out.bases[i].id) {
        r.Child("order").Error("order differs from the group concatenation at " + std::to_string(i));
        break;
      }
    }
  }
  for (const auto& [k, n] : r.Members("gemStats")) {
    const int32_t i = out.baseIndex.Find(k);
    if (i < 0) {
      n.Error("gem stat for unknown base");
      continue;
    }
    ItemBaseDef& b = out.bases[static_cast<size_t>(i)];
    b.isGem = true;
    b.gemStat = n.Enum<Stat>("stat");
    b.gemValue = n.Num("value");
    b.gemTier = n.Int("tier");
  }
  out.allStatsExpandsTo = r.EnumList<Stat>("allStatsExpandsTo");
  for (const auto& [k, n] : r.Members("consumableEffects")) {
    const int32_t i = out.baseIndex.Find(k);
    if (i < 0) {
      n.Error("consumable effect for unknown base");
      continue;
    }
    if (!n.Exists()) continue;  // null = not usable from the bag
    ItemBaseDef& b = out.bases[static_cast<size_t>(i)];
    b.consumableEffect = n.Enum<ConsumableEffect>("effect");
    b.consumableValue = n.Num("value");
  }
  for (const auto& [k, n] : r.Members("groundPotions")) {
    const int32_t i = out.baseIndex.Find(k);
    if (i < 0) {
      n.Error("ground potion for unknown base");
      continue;
    }
    ItemBaseDef& b = out.bases[static_cast<size_t>(i)];
    b.isGroundPotion = true;
    b.groundPotionKind = n.Enum<PotionKind>("type");
    b.groundPotionAmount = n.Int("amount");
  }
  out.materialIds = r.StrList("materialIds");
}

void LoadAffixesFile(const JNode& r, ItemTables& out) {
  out.prefixes.clear();
  out.suffixes.clear();
  out.statDisplay.clear();
  for (const JNode& n : r.Items("prefixes")) out.prefixes.push_back(ReadAffix(n));
  for (const JNode& n : r.Items("suffixes")) out.suffixes.push_back(ReadAffix(n));
  for (const auto& [k, n] : r.Members("statDisplay")) {
    StatDisplayDef d;
    if (!ParseEnum(k, d.stat)) {
      n.Error("unknown stat key '" + k + "'");
      continue;
    }
    d.label = n.Str("label");
    d.isPercent = n.Bool("isPercent");
    out.statDisplay.push_back(std::move(d));
  }
}

void LoadSetsFile(const JNode& r, ItemTables& out) {
  out.sets.clear();
  out.setPieceBases.clear();
  for (const JNode& n : r.Items("sets")) {
    SetDef s;
    s.id = n.Str("id");
    s.name = n.Str("name");
    s.nameEn = n.Str("nameEn", "");
    s.pieces = n.StrList("pieces");
    for (const auto& [pieceId, list] : n.Members("pieceAffixes", true)) {
      SetPieceAffixes pa;
      pa.pieceId = pieceId;
      for (const JNode& a : list.SelfItems()) pa.affixes.push_back(ReadFixedAffix(a));
      s.pieceAffixes.push_back(std::move(pa));
    }
    for (const JNode& b : n.Items("bonuses")) {
      SetBonusDef bonus;
      bonus.count = b.Int("count");
      bonus.description = b.Str("description", "");
      bonus.stats = b.Stats("stats");
      s.bonuses.push_back(std::move(bonus));
    }
    s.dungeonExclusive = n.Bool("dungeonExclusive", false);
    out.sets.push_back(std::move(s));
  }
  for (const auto& [k, n] : r.Members("pieceBases")) {
    out.setPieceBases.push_back(SetPieceBase{k, n.Str("baseId"), n.Bool("dungeonExclusive", false)});
  }
}

void LoadLegendariesFile(const JNode& r, ItemTables& out) {
  out.legendaries.clear();
  for (const JNode& n : r.Items("legendaries")) {
    LegendaryDef l;
    l.id = n.Str("id");
    l.baseId = n.Str("baseId");
    l.name = n.Str("name");
    l.nameEn = n.Str("nameEn", "");
    for (const JNode& a : n.Items("fixedAffixes")) l.fixedAffixes.push_back(ReadFixedAffix(a));
    l.specialEffect = n.Str("specialEffect", "");
    if (n.Has("specialEffectValue")) {
      l.hasSpecialEffectValue = true;
      l.specialEffectValue = n.Num("specialEffectValue");
    }
    l.specialEffectDescription = n.Str("specialEffectDescription", "");
    l.dungeonExclusive = n.Bool("dungeonExclusive", false);
    out.legendaries.push_back(std::move(l));
  }
  out.appliedSpecialEffects = r.StrList("appliedSpecialEffects", true);
}

void LoadEconomyFile(const JNode& r, ItemTables& out) {
  EconomyDef& e = out.economy;
  e.buyPriceMultiplier = r.Num("buyPriceMultiplier");
  e.buybackPriceMultiplier = r.Num("buybackPriceMultiplier");
  e.buybackSlots = r.Int("buybackSlots");
  for (size_t i = 0; i < EnumCount<ItemQuality>(); ++i) {
    e.sellQualityMultiplier[i] = r.Child("sellQualityMultiplier").Num(EnumName(static_cast<ItemQuality>(i)));
    e.sortOrderQuality[i] = r.Child("sortOrder").Child("quality").Int(EnumName(static_cast<ItemQuality>(i)));
  }
  for (size_t i = 0; i < EnumCount<ItemType>(); ++i) {
    e.sortOrderType[i] = r.Child("sortOrder").Child("type").Int(EnumName(static_cast<ItemType>(i)));
  }
  e.bagCapacity = r.Int("bagCapacity");
  e.stashBaseSlots = r.Int("stashBaseSlots");
  e.stashSlotsPerWarehouseLevel = r.IntList("stashSlotsPerWarehouseLevel");
}

void LoadLootRulesFile(const JNode& r, ItemTables& out) {
  LootRulesDef& l = out.loot;
  const JNode d = r.Child("drops");
  if (!d.Bool("chancesArePercent", true)) d.Error("loot chances must be percent (chancesArePercent)");
  l.equipmentChanceNormal = d.Child("equipmentChance").Num("normal");
  l.equipmentChanceElite = d.Child("equipmentChance").Num("elite");
  l.equipmentLuckFactor = d.Child("equipmentChance").Num("luckFactor");
  l.eliteSecondChanceBase = d.Child("eliteSecondChance").Num("base");
  l.eliteSecondLuckFactor = d.Child("eliteSecondChance").Num("luckFactor");
  l.affixThirdMinAffixBonus = d.Child("affixThirdDrop").Num("minAffixBonus");
  l.affixThirdBase = d.Child("affixThirdDrop").Num("base");
  l.affixThirdLuckFactor = d.Child("affixThirdDrop").Num("luckFactor");
  l.affixThirdAffixFactor = d.Child("affixThirdDrop").Num("affixFactor");
  l.miniBossFloorZone = d.Child("miniBossFloor").Enum<ItemQuality>("zone");
  l.miniBossFloorSubDungeon = d.Child("miniBossFloor").Enum<ItemQuality>("subDungeon");
  l.consumableChance = d.Num("consumableChance");
  l.gemChanceBase = d.Child("gemChance").Num("base");
  l.gemChanceLuckFactor = d.Child("gemChance").Num("luckFactor");
  l.gemChanceAffixFactor = d.Child("gemChance").Num("affixFactor");
  l.leyFruitChanceElite = d.Child("leyFruitChance").Num("elite");
  l.leyFruitChanceOther = d.Child("leyFruitChance").Num("other");
  l.leyFruitItemId = d.Child("leyFruitChance").Str("itemId");
  const JNode q = r.Child("qualityRoll");
  l.qualityLuckFactor = q.Num("luckFactor");
  l.qualityEliteBonus = q.Num("eliteBonus");
  l.qualityThresholds.clear();
  for (const JNode& t : q.Items("thresholds")) {
    QualityThresholdDef th;
    th.quality = t.Enum<ItemQuality>("quality");
    th.base = t.Num("base");
    th.lm = t.Num("lm");
    th.em = t.Num("em");
    th.affix = t.Num("affix");
    th.levelOver20 = t.Num("levelOver20");
    l.qualityThresholds.push_back(th);
  }
  const JNode w = r.Child("baseWindows");
  ReadPair(w, "equipment", l.equipmentWindowBelow, l.equipmentWindowAbove);
  ReadPair(w, "wide", l.wideWindowBelow, l.wideWindowAbove);
  ReadPair(w, "setPiece", l.setPieceWindowBelow, l.setPieceWindowAbove);
  l.windowMinLevel = w.Int("minLevel");
  l.levelGateConsumable = r.Child("levelGate").Int("consumable");
  l.levelGateGem = r.Child("levelGate").Int("gem");
  l.levelGateAffix = r.Child("levelGate").Int("affix");
  const JNode ac = r.Child("affixCounts");
  ReadPair(ac, "magic", l.magicAffixMin, l.magicAffixMax);
  ReadPair(ac, "rare", l.rareAffixMin, l.rareAffixMax);
  ReadPair(ac, "genericLegendary", l.genericLegendaryAffixMin, l.genericLegendaryAffixMax);
  ReadPair(ac, "setPieceExtra", l.setPieceExtraMin, l.setPieceExtraMax);
  ReadPair(ac, "genericSet", l.genericSetMin, l.genericSetMax);
  l.affixTierBands.clear();
  for (const JNode& b : r.Items("affixTierBands")) {
    AffixTierBand band;
    if (b.Has("belowLevel")) {
      band.hasBelowLevel = true;
      band.belowLevel = b.Int("belowLevel");
    }
    ReadPair(b, "tiers", band.minTier, band.maxTier);
    l.affixTierBands.push_back(band);
  }
  l.affixTierSlack = r.Int("affixTierSlack");
  l.affixWeightInBand = r.Child("affixWeight").Int("inBand");
  l.affixWeightOutOfBand = r.Child("affixWeight").Int("outOfBand");
  l.legendaryScaleDivisor = r.Child("legendaryLevelScale").Num("divisor");
  l.legendaryScaleMin = r.Child("legendaryLevelScale").Num("min");
  l.legendaryScaleMax = r.Child("legendaryLevelScale").Num("max");
  l.droppedConsumableLevel = r.Child("droppedConsumable").Int("level");
  ReadPair(r.Child("droppedConsumable"), "quantity", l.droppedConsumableQtyMin, l.droppedConsumableQtyMax);
  l.droppedGemLevel = r.Child("droppedGem").Int("level");
  l.droppedGemQty = r.Child("droppedGem").Int("quantity");
  l.groundItemLifetimeMs = r.Num("groundItemLifetimeMs");
  l.potionPickupLifetimeMs = r.Num("potionPickupLifetimeMs");
  l.pickupRadiusSq = r.Num("pickupRadiusSq");
  l.clickHitBoxTiles = r.Num("clickHitBoxTiles");
  l.autoLootIntervalMs = r.Num("autoLootIntervalMs");
  const JNode qr = r.Child("questRewards");
  for (const auto& [k, n] : qr.Members("classWeaponTypes")) {
    ClassId c{};
    if (!ParseEnum(k, c)) {
      n.Error("unknown class");
      continue;
    }
    l.classWeaponTypes[EnumIndex(c)] = qr.Child("classWeaponTypes").EnumList<WeaponType>(k);
  }
  l.unknownClassWeaponTypes = qr.EnumList<WeaponType>("unknownClassWeaponTypes");
  l.shieldClasses = qr.EnumList<ClassId>("shieldClasses");
  l.rewardLevelHeadroom = qr.Int("levelHeadroom");
  l.rewardTopCandidates = qr.Int("topCandidates");
  l.rewardQualityMain = qr.Child("quality").Enum<ItemQuality>("main");
  l.rewardQualitySide = qr.Child("quality").Enum<ItemQuality>("side");
  l.fallbackCollectChance = qr.Num("fallbackCollectChance");
}

void LoadCraftingFile(const JNode& r, ItemTables& out) {
  CraftingDef& c = out.crafting;
  c.materials = r.StrList("materials");
  c.goldUnitMul = r.Child("goldUnit").Num("mul");
  c.goldUnitAdd = r.Child("goldUnit").Num("add");
  c.goldUnitLevelMin = r.Child("goldUnit").Int("levelMin");
  for (const auto& [ak, an] : r.Members("costs")) {
    CraftAction a{};
    if (!ParseEnum(ak, a)) {
      an.Error("unknown craft action");
      continue;
    }
    for (const auto& [qk, qn] : an.SelfMembers()) {
      ItemQuality q{};
      if (!ParseEnum(qk, q)) {
        qn.Error("unknown quality");
        continue;
      }
      c.costs[EnumIndex(a)][EnumIndex(q)][0] = ReadCraftCost(qn.Child("weaponOrArmor"));
      c.costs[EnumIndex(a)][EnumIndex(q)][1] = ReadCraftCost(qn.Child("accessory"));
    }
  }
  const JNode y = r.Child("salvageYield");
  c.scrap = ReadYield(y.Child("scrap"));
  c.scrapNormalBonus = y.Child("scrap").Int("normalBonus");
  c.magicDust = ReadYield(y.Child("magic").Child("dust"));
  c.rareDust = ReadYield(y.Child("rare").Child("dust"));
  c.rareEssence = ReadYield(y.Child("rare").Child("essence"));
  c.legendaryDust = ReadYield(y.Child("legendaryOrSet").Child("dust"));
  c.legendaryEssence = ReadYield(y.Child("legendaryOrSet").Child("essence"));
  ReadPair(r.Child("rerollAffixCounts"), "magic", c.rerollMagicMin, c.rerollMagicMax);
  ReadPair(r.Child("rerollAffixCounts"), "rare", c.rerollRareMin, c.rerollRareMax);
  c.maxItemSockets = r.Int("maxItemSockets");
  c.maxBonusSockets = r.Int("maxBonusSockets");
  c.bagCapacity = r.Int("bagCapacity");
}

void LoadShopsFile(const JNode& r, ItemTables& out) {
  out.shops.clear();
  out.wanderingMerchant.clear();
  for (const auto& [k, n] : r.Members("shops")) {
    out.shops.push_back(ShopDef{k, r.Child("shops").StrList(k)});
  }
  for (const auto& [k, n] : r.Members("wanderingMerchant")) {
    out.wanderingMerchant.push_back(WanderingMerchantDef{k, n.StrList("items")});
  }
}

}  // namespace abyss::dataload
