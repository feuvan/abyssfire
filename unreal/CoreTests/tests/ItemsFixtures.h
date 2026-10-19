// Items-area test fixtures (moved out of items_test.cpp so content_ch1_test.cpp shares them): item builders, id
// lists, event / log helpers and ItmWorld (hero, status effects, inventory, rewards, shop and ground loot over the real
// data).
#pragma once

#include <algorithm>
#include <array>
#include <cstdlib>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "SimHarness.h"
#include "abyss/base/Json.h"
#include "abyss/base/Math.h"
#include "abyss/combat/StatusEffects.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/hero/Rewards.h"
#include "abyss/items/Crafting.h"
#include "abyss/items/GroundLoot.h"
#include "abyss/items/Inventory.h"
#include "abyss/items/Item.h"
#include "abyss/items/ItemCompare.h"
#include "abyss/items/LootGen.h"
#include "abyss/items/Shop.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/save/SaveData.h"
#include "abyss/save/SaveIO.h"
#include "abyss/sim/Snapshot.h"
#include "doctest/doctest.h"

namespace abyss::test::items_fx {

inline const DataStore& D() { return test::RealData(); }

// One counter for every TU that includes this header (content_ch1_test.cpp builds no items with it).
inline int itmUidSeq = 0;

inline ItemInstance Itm(std::string_view baseId, ItemQuality q = ItemQuality::Normal, int32_t level = 10) {
  ItemInstance it;
  it.uid = "t" + std::to_string(itmUidSeq++);
  it.baseId = std::string(baseId);
  const ItemBaseDef* b = D().Items().FindBase(baseId);
  it.name = b != nullptr ? b->name : std::string(baseId);
  it.quality = q;
  it.level = level;
  return it;
}

inline ItemInstance Stack(std::string_view baseId, int32_t qty) {
  ItemInstance it = Itm(baseId, ItemQuality::Normal, 1);
  it.quantity = qty;
  return it;
}

inline GemInstance GemOf(std::string_view gemId) {
  const ItemBaseDef* b = D().Items().FindBase(gemId);
  REQUIRE(b != nullptr);
  return GemInstance{b->id, b->name, b->gemStat, b->gemValue, b->gemTier};
}

inline ItemAffix FixedAffix(std::string_view affixId, double value) {
  const AffixDef* a = D().Items().FindAffix(affixId);
  REQUIRE(a != nullptr);
  return ItemAffix{a->id, a->name, a->stat, value};
}

// Test-only affix on a stat (ItemCompare.test.ts style: affixId = stat name).
inline ItemAffix StatAffix(Stat s, double v) { return ItemAffix{std::string(EnumName(s)), std::string(EnumName(s)), s, v}; }

inline std::vector<std::string> Ids(const std::vector<const AffixDef*>& v) {
  std::vector<std::string> out;
  for (const AffixDef* a : v) out.push_back(a->id);
  return out;
}

inline std::vector<std::string> BaseIds(const std::vector<const ItemBaseDef*>& v) {
  std::vector<std::string> out;
  for (const ItemBaseDef* b : v) out.push_back(b->id);
  return out;
}

inline bool HasEquip(const std::vector<ItemInstance>& items, ItemQuality floor) {
  for (const ItemInstance& it : items) {
    if (IsEquipmentItem(it, D()) && QualityMeetsFloor(it.quality, floor)) return true;
  }
  return false;
}

inline int CountEquip(const std::vector<ItemInstance>& items) {
  int n = 0;
  for (const ItemInstance& it : items) n += IsEquipmentItem(it, D()) ? 1 : 0;
  return n;
}

inline AffixKind KindOf(const ItemAffix& a) {
  const AffixDef* d = D().Items().FindAffix(a.affixId);
  REQUIRE(d != nullptr);
  return d->kind;
}

inline double StatSum(const ItemInstance& it) {
  double s = 0;
  for (const StatValue& sv : it.stats.Items()) s += sv.value;
  return s;
}

inline double AffixSum(const ItemInstance& it) {
  double s = 0;
  for (const ItemAffix& a : it.affixes) s += a.value;
  return s;
}

template <class E>
inline const E* LastEvent(const EventSink& sink) {
  const E* out = nullptr;
  for (const Event& e : sink.Items()) {
    if (const E* p = std::get_if<E>(&e)) out = p;
  }
  return out;
}

inline bool HasLogKey(const EventSink& sink, std::string_view key) {
  for (const Event& e : sink.Items()) {
    if (const EvLog* l = std::get_if<EvLog>(&e)) {
      if (l->text.key == key) return true;
    }
  }
  return false;
}

// A collator that reverses byte order (stands in for the culture collation UE installs, SetItemNameCollator).
inline int ItmTestReverseCollate(std::string_view a, std::string_view b, void* user) {
  if (user != nullptr) ++*static_cast<int*>(user);
  const int r = a.compare(b);
  return r < 0 ? 1 : (r > 0 ? -1 : 0);
}

inline const EvLog* LastLogWithKey(const EventSink& sink, std::string_view key) {
  const EvLog* out = nullptr;
  for (const Event& e : sink.Items()) {
    if (const EvLog* l = std::get_if<EvLog>(&e)) {
      if (l->text.key == key) out = l;
    }
  }
  return out;
}

// A full runtime slice: hero, status effects, inventory, rewards, shop and ground loot over the real data.
struct ItmWorld {
  ItmWorld()
      : hero(h.ctx.data, ClassId::Warrior),
        status(h.ctx.data.Classes().statusRules),
        inv(h.ctx),
        rewards(h.ctx),
        shop(h.ctx),
        ground(h.ctx) {
    h.ctx.sys.hero = &hero;
    h.ctx.sys.status = &status;
    h.ctx.sys.inventory = &inv;
    h.ctx.sys.rewards = &rewards;
    h.ctx.sys.shop = &shop;
    h.ctx.sys.groundLoot = &ground;
    h.onTimer = [this](const Timer& t) {
      if (t.owner == TimerOwner::Items) ground.OnTimer(t);
    };
  }
  void FillBag(int n, std::string_view baseId = "w_rusty_sword") {
    for (int i = 0; i < n; ++i) inv.Items().MutableBag().push_back(Itm(baseId, ItemQuality::Normal, 1));
  }

  test::SimHarness h;
  Hero hero;
  StatusEffectSystem status;
  InventorySystem inv;
  RewardService rewards;
  ShopSystem shop;
  GroundLootSystem ground;
};

}  // namespace abyss::test::items_fx
