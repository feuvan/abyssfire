// Item comparison (loot-items-inventory.md section 14). Owner area: items.
#include "abyss/base/Platform.h"

#include "abyss/items/ItemCompare.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/Math.h"
#include "abyss/data/DataStore.h"

namespace abyss {

namespace {

void CmpAdd(std::vector<StatTotal>& out, CompareKey key, double v) {
  for (StatTotal& t : out) {
    if (t.key == key) {
      t.value += v;
      return;
    }
  }
  out.push_back(StatTotal{key, v});
}

CompareKey CmpStatKey(Stat s) {
  CompareKey k;
  k.kind = CompareKey::Kind::StatKey;
  k.stat = s;
  return k;
}

double CmpItemScore(const DataStore& data, const ItemInstance& item) {
  double s = 0;
  for (const StatTotal& t : ItemStatTotals(data, item)) s += std::fabs(t.value);
  return s + item.level * 0.1;
}

int CmpRank(const CompareKey& k) {
  switch (k.kind) {
    case CompareKey::Kind::AvgDamage: return 0;
    case CompareKey::Kind::BaseDefense: return 1;
    case CompareKey::Kind::StatKey: return 2;
  }
  return 2;
}

}  // namespace

std::vector<StatTotal> ItemStatTotals(const DataStore& data, const ItemInstance& item) {
  std::vector<StatTotal> out;
  const ItemBaseDef* base = data.Items().FindBase(item.baseId);
  if (base != nullptr && base->hasBaseDamage) {
    CompareKey k;
    k.kind = CompareKey::Kind::AvgDamage;
    CmpAdd(out, k, (base->baseDamageMin + base->baseDamageMax) / 2.0);  // shields contribute 0
  }
  if (base != nullptr && base->hasBaseDefense && base->baseDefense != 0) {
    CompareKey k;
    k.kind = CompareKey::Kind::BaseDefense;
    CmpAdd(out, k, base->baseDefense);
  }
  if (item.identified || item.quality == ItemQuality::Normal) {
    for (const ItemAffix& a : item.affixes) CmpAdd(out, CmpStatKey(a.stat), a.value);
    // C11: a named legendary's special effect is a real gear stat (Inventory::EquipmentStatBag adds it), so the
    // compare deltas and the ring score (FIX Q18 slot choice) count it too.
    Stat effect{};
    double effectValue = 0;
    if (ItemSpecialEffectStat(item, data, effect, effectValue)) CmpAdd(out, CmpStatKey(effect), effectValue);
  }
  for (const GemInstance& g : item.sockets) CmpAdd(out, CmpStatKey(g.stat), g.value);
  return out;
}

std::optional<CompareTarget> FindCompareTarget(
    const DataStore& data, const ItemInstance& item,
    const std::array<std::optional<ItemInstance>, EnumCount<EquipSlot>()>& equipment) {
  const ItemBaseDef* base = data.Items().FindBase(item.baseId);
  if (base == nullptr || !base->hasSlot) return std::nullopt;
  for (const auto& e : equipment) {
    if (e.has_value() && e->uid == item.uid) return std::nullopt;  // the item itself is worn
  }
  const EquipSlot slot = base->slot;
  if (slot == EquipSlot::Ring1 || slot == EquipSlot::Ring2) {
    const auto& r1 = equipment[EnumIndex(EquipSlot::Ring1)];
    const auto& r2 = equipment[EnumIndex(EquipSlot::Ring2)];
    if (!r1.has_value()) return CompareTarget{EquipSlot::Ring1, nullptr};
    if (!r2.has_value()) return CompareTarget{EquipSlot::Ring2, nullptr};
    // The weaker ring (tie -> ring1).
    if (CmpItemScore(data, *r1) <= CmpItemScore(data, *r2)) return CompareTarget{EquipSlot::Ring1, &*r1};
    return CompareTarget{EquipSlot::Ring2, &*r2};
  }
  const auto& worn = equipment[EnumIndex(slot)];
  return CompareTarget{slot, worn.has_value() ? &*worn : nullptr};
}

std::vector<StatDelta> StatDeltas(const DataStore& data, const ItemInstance& candidate, const ItemInstance* equipped) {
  const std::vector<StatTotal> a = ItemStatTotals(data, candidate);
  const std::vector<StatTotal> b = equipped != nullptr ? ItemStatTotals(data, *equipped) : std::vector<StatTotal>{};
  // Keys in insertion order: the candidate's, then the worn item's new ones.
  std::vector<CompareKey> keys;
  for (const StatTotal& t : a) keys.push_back(t.key);
  for (const StatTotal& t : b) {
    if (std::find(keys.begin(), keys.end(), t.key) == keys.end()) keys.push_back(t.key);
  }
  auto valueOf = [](const std::vector<StatTotal>& list, const CompareKey& k) {
    for (const StatTotal& t : list) {
      if (t.key == k) return t.value;
    }
    return 0.0;
  };
  std::vector<StatDelta> out;
  for (const CompareKey& k : keys) {
    const double d = JsRound((valueOf(a, k) - valueOf(b, k)) * 10.0) / 10.0;
    if (d != 0) out.push_back(StatDelta{k, d});
  }
  std::stable_sort(out.begin(), out.end(), [](const StatDelta& x, const StatDelta& y) {
    const int rx = CmpRank(x.key);
    const int ry = CmpRank(y.key);
    if (rx != ry) return rx < ry;
    return std::fabs(x.delta) > std::fabs(y.delta);
  });
  return out;
}

}  // namespace abyss
