// Blacksmith crafting (loot-items-inventory.md section 13; crafting.json). Owner area: items.
#include "abyss/base/Platform.h"

#include "abyss/items/Crafting.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/Assert.h"
#include "abyss/base/Math.h"
#include "abyss/data/DataStore.h"
#include "abyss/items/Inventory.h"

namespace abyss {

namespace {

int32_t CrfMaxStack(const DataStore& data, std::string_view baseId) {
  const ItemBaseDef* b = data.Items().FindBase(baseId);
  return b != nullptr ? (std::max)(1, b->maxStack) : 1;
}

int32_t CrfYieldTerm(const SalvageYieldTerm& t, int32_t level) {
  return t.base + (t.hasPerLevelDiv && t.perLevelDiv > 0 ? level / t.perLevelDiv : 0);
}

// New bag slots needed to add `adds` (stacking into existing partial stacks first; 13.3 step 8).
int32_t CrfSlotsNeeded(const DataStore& data, std::span<const ItemInstance> bag, const std::vector<MaterialYield>& adds) {
  int32_t slots = 0;
  for (const MaterialYield& a : adds) {
    const int32_t cap = CrfMaxStack(data, a.baseId);
    int64_t room = 0;
    for (const ItemInstance& it : bag) {
      if (it.baseId == a.baseId) room += (std::max)(0, cap - it.quantity);
    }
    const int64_t overflow = (std::max)(int64_t{0}, a.quantity - room);
    slots += static_cast<int32_t>((overflow + cap - 1) / cap);
  }
  return slots;
}

// Adds a stackable quantity, topping up existing stacks in bag order first, then new stacks of <= maxStack.
void CrfAddStacked(const LootContext& ctx, std::vector<ItemInstance>& bag, const std::string& baseId,
                   const std::string& name, int32_t quantity) {
  const int32_t cap = CrfMaxStack(*ctx.data, baseId);
  int32_t left = quantity;
  for (ItemInstance& it : bag) {
    if (left <= 0) break;
    if (it.baseId != baseId || it.quantity >= cap) continue;
    const int32_t add = (std::min)(cap - it.quantity, left);
    it.quantity += add;
    left -= add;
  }
  while (left > 0) {
    const int32_t q = (std::min)(cap, left);
    ItemInstance it;
    if (ctx.uids != nullptr) it.uid = ctx.uids->Next();
    it.baseId = baseId;
    it.name = name;
    it.quality = ItemQuality::Normal;
    it.level = 1;
    it.identified = true;
    it.quantity = q;
    bag.push_back(std::move(it));
    left -= q;
  }
}

// spendMaterials (13.4): per material in crafting.json order, drain the smallest stacks first, then drop every emptied
// entry.
void CrfSpendMaterials(const DataStore& data, std::vector<ItemInstance>& bag, const std::vector<CraftMaterialCost>& need) {
  for (const std::string& id : data.Items().crafting.materials) {
    int32_t left = 0;
    for (const CraftMaterialCost& m : need) {
      if (m.itemId == id) left += m.count;
    }
    if (left <= 0) continue;
    std::vector<size_t> stacks;
    for (size_t i = 0; i < bag.size(); ++i) {
      if (bag[i].baseId == id) stacks.push_back(i);
    }
    std::stable_sort(stacks.begin(), stacks.end(),
                     [&bag](size_t a, size_t b) { return bag[a].quantity < bag[b].quantity; });
    for (size_t i : stacks) {
      if (left <= 0) break;
      const int32_t take = (std::min)(left, bag[i].quantity);
      bag[i].quantity -= take;
      left -= take;
    }
  }
  bag.erase(std::remove_if(bag.begin(), bag.end(), [](const ItemInstance& it) { return it.quantity <= 0; }),
            bag.end());
}

const ItemInstance* CrfFindAnywhere(const Inventory& inv, std::string_view uid, bool& inBag) {
  inBag = false;
  if (const ItemInstance* it = inv.FindInBag(uid)) {
    inBag = true;
    return it;
  }
  for (const auto& e : inv.Equipment()) {
    if (e.has_value() && e->uid == uid) return &*e;
  }
  return inv.FindInStash(uid);
}

void CrfAffixRange(const CraftingDef& c, ItemQuality q, int32_t& lo, int32_t& hi) {
  if (q == ItemQuality::Rare) {
    lo = c.rerollRareMin;
    hi = c.rerollRareMax;
  } else {
    lo = c.rerollMagicMin;
    hi = c.rerollMagicMax;
  }
}

}  // namespace

int64_t CraftGoldUnit(int32_t level) { return 6 * (static_cast<int64_t>((std::max)(1, level)) + 5); }

int64_t CraftGoldUnit(const DataStore& data, int32_t level) {
  const CraftingDef& c = data.Items().crafting;
  const double l = static_cast<double>((std::max)(c.goldUnitLevelMin, level));
  return static_cast<int64_t>(c.goldUnitMul * (l + c.goldUnitAdd));
}

std::optional<ItemQuality> CraftUpgradeTarget(ItemQuality q) {
  if (q == ItemQuality::Normal) return ItemQuality::Magic;
  if (q == ItemQuality::Magic) return ItemQuality::Rare;
  return std::nullopt;
}

CraftCost ComputeCraftCost(const DataStore& data, CraftAction action, const ItemInstance& item) {
  CraftCost cost;
  const ItemBaseDef* base = data.Items().FindBase(item.baseId);
  if (base == nullptr || !base->IsEquipment()) return cost;
  const bool accessory = base->type != ItemType::Weapon && base->type != ItemType::Armor;
  const CraftCostDef& def = data.Items().crafting.Cost(action, item.quality, accessory);
  if (!def.allowed) return cost;
  cost.applies = true;
  cost.gold = def.goldUnits * CraftGoldUnit(data, item.level);
  cost.materials = def.materials;
  return cost;
}

std::vector<MaterialYield> SalvageYield(const DataStore& data, const ItemInstance& item) {
  const CraftingDef& c = data.Items().crafting;
  const int32_t level = (std::max)(1, item.level);
  int32_t scrap = CrfYieldTerm(c.scrap, level) + (item.quality == ItemQuality::Normal ? c.scrapNormalBonus : 0);
  int32_t dust = 0;
  int32_t essence = 0;
  switch (item.quality) {
    case ItemQuality::Magic:
      dust = CrfYieldTerm(c.magicDust, level);
      break;
    case ItemQuality::Rare:
      dust = CrfYieldTerm(c.rareDust, level);
      essence = CrfYieldTerm(c.rareEssence, level);
      break;
    case ItemQuality::Legendary:
    case ItemQuality::Set:
      dust = CrfYieldTerm(c.legendaryDust, level);
      essence = CrfYieldTerm(c.legendaryEssence, level);
      break;
    case ItemQuality::Normal:
      break;
  }
  const int32_t amounts[3] = {scrap, dust, essence};
  std::vector<MaterialYield> out;
  for (size_t i = 0; i < c.materials.size() && i < 3; ++i) {
    if (amounts[i] > 0) out.push_back(MaterialYield{c.materials[i], amounts[i]});
  }
  return out;
}

CraftCheck CheckCraft(const DataStore& data, CraftAction action, std::string_view itemUid, const Inventory& inv,
                      int64_t gold) {
  CraftCheck c;
  bool inBag = false;
  const ItemInstance* item = CrfFindAnywhere(inv, itemUid, inBag);
  if (item == nullptr) {
    c.reason = CraftFail::NotInBag;
    return c;
  }
  const ItemBaseDef* base = data.Items().FindBase(item->baseId);
  if (base == nullptr) {
    c.reason = CraftFail::UnknownBase;
    return c;
  }
  if (!base->IsEquipment()) {
    c.reason = CraftFail::NotEquipment;
    return c;
  }
  const CraftCost cost = ComputeCraftCost(data, action, *item);
  if (!cost.applies) {
    c.reason = action == CraftAction::Socket ? CraftFail::NotEquipment : CraftFail::Quality;
    return c;
  }
  c.cost = cost;
  if (!inBag) {
    c.reason = CraftFail::NotInBag;
    return c;
  }
  const CraftingDef& cd = data.Items().crafting;
  if (action == CraftAction::Socket &&
      (item->bonusSockets >= cd.maxBonusSockets || ItemSocketCapacity(*item, data) >= cd.maxItemSockets)) {
    c.reason = CraftFail::MaxSockets;
    return c;
  }
  if (gold < cost.gold) {
    c.reason = CraftFail::Gold;
    return c;
  }
  for (const CraftMaterialCost& m : cost.materials) {
    if (inv.CountOf(m.itemId) < m.count) {
      c.reason = CraftFail::Materials;
      return c;
    }
  }
  if (action == CraftAction::Salvage) {
    std::vector<MaterialYield> adds = SalvageYield(data, *item);
    for (const GemInstance& g : item->sockets) adds.push_back(MaterialYield{g.gemId, 1});
    // The salvaged item frees its own slot.
    if (static_cast<int32_t>(inv.Bag().size()) - 1 + CrfSlotsNeeded(data, inv.Bag(), adds) > cd.bagCapacity) {
      c.reason = CraftFail::BagFull;
      return c;
    }
  }
  c.ok = true;
  return c;
}

CraftResult PerformCraft(const LootContext& ctx, CraftAction action, std::string_view itemUid, Inventory& inv,
                         int64_t& gold) {
  const DataStore& data = *ctx.data;
  CraftResult r;
  r.itemUid = std::string(itemUid);
  const CraftCheck c = CheckCraft(data, action, itemUid, inv, gold);
  if (!c.ok) {
    r.reason = c.reason;
    return r;
  }
  gold -= c.cost.gold;
  r.goldSpent = c.cost.gold;
  std::vector<ItemInstance>& bag = inv.MutableBag();
  CrfSpendMaterials(data, bag, c.cost.materials);
  const CraftingDef& cd = data.Items().crafting;
  switch (action) {
    case CraftAction::Salvage: {
      std::optional<ItemInstance> item = inv.TakeEntry(itemUid);
      ABYSS_ASSERT(item.has_value(), "salvaged item vanished");
      if (!item.has_value()) break;
      r.yields = SalvageYield(data, *item);
      for (const MaterialYield& y : r.yields) {
        const ItemBaseDef* mb = data.Items().FindBase(y.baseId);
        CrfAddStacked(ctx, inv.MutableBag(), y.baseId, mb != nullptr ? mb->name : y.baseId, y.quantity);
      }
      for (const GemInstance& g : item->sockets) {
        CrfAddStacked(ctx, inv.MutableBag(), g.gemId, g.name, 1);
        r.gemsReturned.push_back(g.gemId);
      }
      break;
    }
    case CraftAction::Reforge: {
      ItemInstance* item = inv.FindInBagMutable(itemUid);
      if (item == nullptr) break;
      int32_t lo = 0;
      int32_t hi = 0;
      CrfAffixRange(cd, item->quality, lo, hi);
      item->affixes.clear();
      AddRandomAffixes(ctx, *item, item->level, lo, hi);
      item->identified = true;
      FinalizeItem(data, *item);
      break;
    }
    case CraftAction::Upgrade: {
      ItemInstance* item = inv.FindInBagMutable(itemUid);
      if (item == nullptr) break;
      const std::optional<ItemQuality> next = CraftUpgradeTarget(item->quality);
      if (!next.has_value()) break;
      int32_t lo = 0;
      int32_t hi = 0;
      CrfAffixRange(cd, *next, lo, hi);
      item->quality = *next;
      const int32_t target = lo + static_cast<int32_t>(std::floor(ctx.rng->Float01() * (hi - lo + 1)));
      const int32_t need = (std::max)(1, target - static_cast<int32_t>(item->affixes.size()));
      AddRandomAffixes(ctx, *item, item->level, need, need);  // existing affixes kept; the new ones start a prefix
      item->identified = true;
      FinalizeItem(data, *item);
      break;
    }
    case CraftAction::Socket: {
      ItemInstance* item = inv.FindInBagMutable(itemUid);
      if (item != nullptr) item->bonusSockets += 1;
      break;
    }
  }
  r.ok = true;
  return r;
}

}  // namespace abyss
