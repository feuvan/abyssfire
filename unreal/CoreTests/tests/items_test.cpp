// Items area: item instances, loot generation, bag / equipment / stash, shops, crafting, compare, ground loot, saves.
// Ports: CraftingSystem.test.ts, GemSocketing.test.ts, ItemCompare.test.ts, smoke.test.ts (LootSystem /
// InventorySystem), NumericalBalance.test.ts (affix tiers, item power scaling, gold economy), EliteAffixSystem.test.ts
// (affix loot bonus), endgame-scrutiny-fixes.test.ts + story-scrutiny-zones-loot.test.ts (mini-boss floors),
// QuestEngine.test.ts (QuestRewards), ShopCloseMerchantDespawn.test.ts (shop close npcId), SaveMigration.test.ts and
// cross-area-integration.test.ts (item save fields, gem stat flow); plus loot-items-inventory.md 20 (worked examples),
// 5.7 (reference probabilities), 5.9 (Chapter 1 pools) and the DECISIONS I1-I11 port rules. Loot distributions use
// seeded Rng streams.
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

// The save section readers / writers are internal to the core (not exported): test them only when the core is linked
// statically (ABYSS_SHARED=1 builds the core as a hidden-visibility shared library), as hero_test.cpp does.
#if !(defined(ABYSS_CORE_DLL) && ABYSS_CORE_DLL)
#define ABYSS_ITEMS_TEST_SAVE_SECTIONS 1
#include "../../Source/AbyssCore/Private/save/SaveSections.h"
#else
#define ABYSS_ITEMS_TEST_SAVE_SECTIONS 0
#endif

using namespace abyss;

namespace {

const DataStore& D() { return test::RealData(); }

int itmUidSeq = 0;

ItemInstance Itm(std::string_view baseId, ItemQuality q = ItemQuality::Normal, int32_t level = 10) {
  ItemInstance it;
  it.uid = "t" + std::to_string(itmUidSeq++);
  it.baseId = std::string(baseId);
  const ItemBaseDef* b = D().Items().FindBase(baseId);
  it.name = b != nullptr ? b->name : std::string(baseId);
  it.quality = q;
  it.level = level;
  return it;
}

ItemInstance Stack(std::string_view baseId, int32_t qty) {
  ItemInstance it = Itm(baseId, ItemQuality::Normal, 1);
  it.quantity = qty;
  return it;
}

GemInstance GemOf(std::string_view gemId) {
  const ItemBaseDef* b = D().Items().FindBase(gemId);
  REQUIRE(b != nullptr);
  return GemInstance{b->id, b->name, b->gemStat, b->gemValue, b->gemTier};
}

ItemAffix FixedAffix(std::string_view affixId, double value) {
  const AffixDef* a = D().Items().FindAffix(affixId);
  REQUIRE(a != nullptr);
  return ItemAffix{a->id, a->name, a->stat, value};
}

// Test-only affix on a stat (ItemCompare.test.ts style: affixId = stat name).
ItemAffix StatAffix(Stat s, double v) { return ItemAffix{std::string(EnumName(s)), std::string(EnumName(s)), s, v}; }

std::vector<std::string> Ids(const std::vector<const AffixDef*>& v) {
  std::vector<std::string> out;
  for (const AffixDef* a : v) out.push_back(a->id);
  return out;
}

std::vector<std::string> BaseIds(const std::vector<const ItemBaseDef*>& v) {
  std::vector<std::string> out;
  for (const ItemBaseDef* b : v) out.push_back(b->id);
  return out;
}

bool HasEquip(const std::vector<ItemInstance>& items, ItemQuality floor) {
  for (const ItemInstance& it : items) {
    if (IsEquipmentItem(it, D()) && QualityMeetsFloor(it.quality, floor)) return true;
  }
  return false;
}

int CountEquip(const std::vector<ItemInstance>& items) {
  int n = 0;
  for (const ItemInstance& it : items) n += IsEquipmentItem(it, D()) ? 1 : 0;
  return n;
}

AffixKind KindOf(const ItemAffix& a) {
  const AffixDef* d = D().Items().FindAffix(a.affixId);
  REQUIRE(d != nullptr);
  return d->kind;
}

double StatSum(const ItemInstance& it) {
  double s = 0;
  for (const StatValue& sv : it.stats.Items()) s += sv.value;
  return s;
}

double AffixSum(const ItemInstance& it) {
  double s = 0;
  for (const ItemAffix& a : it.affixes) s += a.value;
  return s;
}

template <class E>
const E* LastEvent(const EventSink& sink) {
  const E* out = nullptr;
  for (const Event& e : sink.Items()) {
    if (const E* p = std::get_if<E>(&e)) out = p;
  }
  return out;
}

bool HasLogKey(const EventSink& sink, std::string_view key) {
  for (const Event& e : sink.Items()) {
    if (const EvLog* l = std::get_if<EvLog>(&e)) {
      if (l->text.key == key) return true;
    }
  }
  return false;
}

// A collator that reverses byte order (stands in for the culture collation UE installs, SetItemNameCollator).
int ItmTestReverseCollate(std::string_view a, std::string_view b, void* user) {
  if (user != nullptr) ++*static_cast<int*>(user);
  const int r = a.compare(b);
  return r < 0 ? 1 : (r > 0 ? -1 : 0);
}

const EvLog* LastLogWithKey(const EventSink& sink, std::string_view key) {
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

}  // namespace

TEST_SUITE("items") {
  // ===================================================================================================================
  // Item instance basics
  // ===================================================================================================================
  TEST_CASE("item stat cache sums affixes and socketed gems") {
    ItemInstance it;
    it.affixes.push_back({"p_sharp", "", Stat::Damage, 3});
    it.affixes.push_back({"s_bear", "", Stat::Str, 2});
    it.sockets.push_back({"g_ruby_1", "", Stat::Str, 4, 1});
    ComputeItemStats(it);
    CHECK(it.stats.Get(Stat::Damage) == 3);
    CHECK(it.stats.Get(Stat::Str) == 6);
    CHECK(it.stats.Size() == 2);
  }

  TEST_CASE("uid generator: 'i' + lower-case hex counter") {
    ItemUidGenerator g;
    CHECK(g.Next() == "i1");
    g.SetCounter(255);
    CHECK(g.Next() == "iff");
    CHECK(g.Counter() == 256);
    CHECK(ItemUidCounterValue("iff") == 255);
    CHECK(ItemUidCounterValue("i1") == 1);
    CHECK(ItemUidCounterValue("item_123_4") == 0);
    CHECK(ItemUidCounterValue("x12") == 0);
  }

  TEST_CASE("data: gems, materials, sockets (GemSocketing.test.ts, CraftingSystem.test.ts data)") {
    const ItemTables& t = D().Items();
    int ruby = 0, sapphire = 0, emerald = 0, topaz = 0, diamond = 0;
    for (const ItemBaseDef& b : t.bases) {
      if (b.group != "gems") continue;
      CHECK(b.stackable);
      CHECK(b.maxStack == 10);
      CHECK(b.type == ItemType::Gem);
      CHECK(b.isGem);
      if (b.id.rfind("g_ruby", 0) == 0) ++ruby;
      if (b.id.rfind("g_sapphire", 0) == 0) ++sapphire;
      if (b.id.rfind("g_emerald", 0) == 0) ++emerald;
      if (b.id.rfind("g_topaz", 0) == 0) ++topaz;
      if (b.id.rfind("g_diamond", 0) == 0) ++diamond;
    }
    CHECK(ruby == 3);
    CHECK(sapphire == 3);
    CHECK(emerald == 3);
    CHECK(topaz == 3);
    CHECK(diamond >= 1);
    struct Row {
      const char* id;
      Stat stat;
      double value;
      int tier;
    };
    const Row rows[] = {{"g_ruby_1", Stat::Str, 5, 1},        {"g_ruby_2", Stat::Str, 12, 2},
                        {"g_ruby_3", Stat::Str, 20, 3},       {"g_sapphire_1", Stat::Int, 5, 1},
                        {"g_sapphire_2", Stat::Int, 12, 2},   {"g_sapphire_3", Stat::Int, 20, 3},
                        {"g_emerald_1", Stat::Dex, 5, 1},     {"g_emerald_2", Stat::Dex, 12, 2},
                        {"g_emerald_3", Stat::Dex, 20, 3},    {"g_topaz_1", Stat::MagicFind, 5, 1},
                        {"g_topaz_2", Stat::MagicFind, 10, 2}, {"g_topaz_3", Stat::MagicFind, 18, 3},
                        {"g_diamond_1", Stat::AllStats, 3, 1}};
    for (const Row& r : rows) {
      const ItemBaseDef* b = t.FindBase(r.id);
      REQUIRE(b != nullptr);
      CHECK(b->gemStat == r.stat);
      CHECK(b->gemValue == r.value);
      CHECK(b->gemTier == r.tier);
    }
    // Materials: stackable material bases with their own icons and i18n names, in crafting order.
    REQUIRE(t.crafting.materials.size() == 3);
    CHECK(t.crafting.materials[0] == "m_scrap");
    CHECK(t.crafting.materials[1] == "m_dust");
    CHECK(t.crafting.materials[2] == "m_essence");
    for (const std::string& id : t.crafting.materials) {
      const ItemBaseDef* b = t.FindBase(id);
      REQUIRE(b != nullptr);
      CHECK(b->type == ItemType::Material);
      CHECK(b->stackable);
      CHECK(b->maxStack > 1);
      CHECK(b->icon == id);
      CHECK(D().Strings().Lookup(LocaleId::ZhCN, "data.item." + id + ".name") != nullptr);
      CHECK(D().Strings().Lookup(LocaleId::En, "data.item." + id + ".name") != nullptr);
    }
    // Base socket counts.
    CHECK(t.FindBase("w_short_sword")->sockets == 1);
    CHECK(t.FindBase("w_claymore")->sockets == 2);
    CHECK(t.FindBase("w_demon_blade")->sockets == 3);
    CHECK(t.FindBase("a_chain_mail")->sockets == 1);
    CHECK(t.FindBase("a_plate_armor")->sockets == 2);
    CHECK(t.FindBase("a_dragon_armor")->sockets == 3);
    // Removed items (I4 TP scroll, I2 ID scroll).
    CHECK(t.IsRemovedItem("c_tp_scroll"));
    CHECK(t.IsRemovedItem("c_id_scroll"));
    CHECK_FALSE(t.IsRemovedItem("c_hp_potion_s"));
  }

  TEST_CASE("every ui.forge key exists in both locales (CraftingSystem.test.ts)") {
    JsonValue zh, en;
    REQUIRE(ParseJson(test::ReadFile(test::DataDir() + "/i18n_zh-CN.json"), zh));
    REQUIRE(ParseJson(test::ReadFile(test::DataDir() + "/i18n_en.json"), en));
    std::set<std::string> zk, ek;
    for (const JsonMember& m : zh.Get("strings").Members()) {
      if (m.key.rfind("ui.forge.", 0) == 0) zk.insert(m.key);
    }
    for (const JsonMember& m : en.Get("strings").Members()) {
      if (m.key.rfind("ui.forge.", 0) == 0) ek.insert(m.key);
    }
    CHECK(zk.size() > 10);
    CHECK(zk == ek);
  }

  TEST_CASE("socket capacity (9.1) and equipment test") {
    ItemInstance rusty = Itm("w_rusty_sword");
    CHECK(ItemSocketCapacity(rusty, D()) == 0);
    rusty.bonusSockets = 1;
    CHECK(ItemSocketCapacity(rusty, D()) == 1);
    CHECK(ItemSocketCapacity(Itm("j_gold_ring"), D()) == 0);
    CHECK(ItemSocketCapacity(Itm("x_removed_item"), D()) == 0);
    CHECK(IsEquipmentItem(Itm("j_copper_ring"), D()));
    CHECK_FALSE(IsEquipmentItem(Itm("c_hp_potion_s"), D()));
    CHECK_FALSE(IsEquipmentItem(Itm("x_removed_item"), D()));
  }

  // ===================================================================================================================
  // Affixes and item creation (4.1-4.5)
  // ===================================================================================================================
  TEST_CASE("20.1 affix pools: item level 1 leather boots") {
    const std::vector<std::string> pre = Ids(AffixPool(D(), "a_leather_boots", 1, AffixKind::Prefix, {}));
    const std::vector<std::string> suf = Ids(AffixPool(D(), "a_leather_boots", 1, AffixKind::Suffix, {}));
    CHECK(pre == std::vector<std::string>{"pre_sturdy", "pre_strong", "pre_nimble", "pre_wise", "pre_hardy",
                                          "pre_spiritual"});
    CHECK(suf == std::vector<std::string>{"suf_life", "suf_mana", "suf_fire_res", "suf_ice_res", "suf_lightning_res",
                                          "suf_speed", "suf_luck"});
    // Used ids are excluded.
    CHECK(AffixPool(D(), "a_leather_boots", 1, AffixKind::Prefix, {"pre_sturdy"}).size() == 5);
    // 5.9: item level <= 4 reaches tier 1 only; from item level 5 the tier-2 rows join (17 prefixes, 23 suffixes before
    // the slot filter -> use an unknown base, which skips it).
    CHECK(AffixPool(D(), "x_none", 4, AffixKind::Prefix, {}).size() == 8);
    CHECK(AffixPool(D(), "x_none", 4, AffixKind::Suffix, {}).size() == 12);
    CHECK(AffixPool(D(), "x_none", 5, AffixKind::Prefix, {}).size() == 17);
    CHECK(AffixPool(D(), "x_none", 5, AffixKind::Suffix, {}).size() == 23);
    for (const AffixDef* a : AffixPool(D(), "x_none", 14, AffixKind::Suffix, {})) CHECK(a->tier <= 2);
  }

  TEST_CASE("20.2 affix tier bands") {
    struct Row {
      int level, minT, maxT, lo, hi;
    };
    const Row rows[] = {{1, 1, 2, 1, 3}, {7, 1, 2, 1, 3}, {8, 1, 3, 1, 4}, {17, 1, 3, 1, 4},
                        {18, 2, 4, 1, 5}, {28, 3, 5, 2, 5}, {38, 4, 5, 3, 5}, {60, 4, 5, 3, 5}};
    for (const Row& r : rows) {
      const AffixTierRange t = AffixTiersForLevel(D(), r.level);
      CHECK(t.minTier == r.minT);
      CHECK(t.maxTier == r.maxT);
      CHECK(t.loTier == r.lo);
      CHECK(t.hiTier == r.hi);
    }
  }

  TEST_CASE("4.2 affix alternation, unique ids, values in range (seeded)") {
    Rng rng(7);
    ItemUidGenerator uids;
    const LootContext lc{&D(), &rng, &uids};
    for (int i = 0; i < 300; ++i) {
      const ItemQuality q = (i % 2) == 0 ? ItemQuality::Magic : ItemQuality::Rare;
      std::optional<ItemInstance> it = CreateItem(lc, "w_broad_sword", 12, q);
      REQUIRE(it.has_value());
      const size_t n = it->affixes.size();
      if (q == ItemQuality::Magic) {
        CHECK(n >= 1);
        CHECK(n <= 2);
      } else {
        CHECK(n >= 3);
        CHECK(n <= 4);
      }
      std::set<std::string> ids;
      for (size_t k = 0; k < n; ++k) {
        // 1 -> P; 2 -> P,S; 3 -> P,S,P; 4 -> P,S,P,S
        CHECK(KindOf(it->affixes[k]) == ((k % 2) == 0 ? AffixKind::Prefix : AffixKind::Suffix));
        const AffixDef* d = D().Items().FindAffix(it->affixes[k].affixId);
        CHECK(it->affixes[k].value >= d->minValue);
        CHECK(it->affixes[k].value <= d->maxValue);
        ids.insert(it->affixes[k].affixId);
      }
      CHECK(ids.size() == n);
      CHECK(StatSum(*it) == AffixSum(*it));
      CHECK(it->identified);
      CHECK(it->quantity == 1);
    }
  }

  TEST_CASE("affix tiers match zone difficulty (NumericalBalance.test.ts)") {
    Rng rng(11);
    ItemUidGenerator uids;
    const LootContext lc{&D(), &rng, &uids};
    int maxLow = 0;
    int maxHigh = 0;
    for (int i = 0; i < 200; ++i) {
      if (std::optional<ItemInstance> it = GenerateEquipment(lc, 5, ItemQuality::Magic)) {
        for (const ItemAffix& a : it->affixes) maxLow = (std::max)(maxLow, D().Items().FindAffix(a.affixId)->tier);
      }
      if (std::optional<ItemInstance> it = GenerateEquipment(lc, 42, ItemQuality::Rare)) {
        for (const ItemAffix& a : it->affixes) maxHigh = (std::max)(maxHigh, D().Items().FindAffix(a.affixId)->tier);
      }
    }
    CHECK(maxLow >= 1);
    CHECK(maxLow <= 2);  // levelReq <= L + 5 keeps tier 3 out below item level 15
    CHECK(maxHigh >= 4);
  }

  TEST_CASE("20.3 legendary scaling round(v * clamp(L / 35, 0.6, 1.5))") {
    Rng rng(3);
    ItemUidGenerator uids;
    const LootContext lc{&D(), &rng, &uids};
    auto values = [](const ItemInstance& it) {
      std::vector<double> v;
      for (const ItemAffix& a : it.affixes) v.push_back(a.value);
      return v;
    };
    std::optional<ItemInstance> grief = CreateItem(lc, "w_broad_sword", 6, ItemQuality::Legendary);
    REQUIRE(grief.has_value());
    CHECK(grief->legendaryId == "leg_grief");
    CHECK(values(*grief) == std::vector<double>{18, 12, 3});
    CHECK(grief->name == D().Items().FindLegendary("leg_grief")->name);
    CHECK(grief->legendaryEffect == D().Items().FindLegendary("leg_grief")->specialEffectDescription);
    std::optional<ItemInstance> step = CreateItem(lc, "a_leather_boots", 1, ItemQuality::Legendary);
    CHECK(step->legendaryId == "leg_shadowstep");
    CHECK(values(*step) == std::vector<double>{15, 9});
    std::optional<ItemInstance> reaver = CreateItem(lc, "w_demon_blade", 52, ItemQuality::Legendary);
    CHECK(reaver->legendaryId == "leg_soulreaver");
    CHECK(values(*reaver) == std::vector<double>{67, 12, 30});
    reaver = CreateItem(lc, "w_demon_blade", 60, ItemQuality::Legendary);
    CHECK(values(*reaver)[0] == 68);  // 67.5 rounds up
    // Stats cache follows the scaled values.
    CHECK(grief->stats.Get(Stat::Damage) == 18);
  }

  TEST_CASE("generic legendaries and I11 (dungeon-exclusive items stay out of overworld drops)") {
    Rng rng(5);
    ItemUidGenerator uids;
    LootContext lc{&D(), &rng, &uids};
    std::optional<ItemInstance> gen = CreateItem(lc, "w_rusty_sword", 10, ItemQuality::Legendary);
    REQUIRE(gen.has_value());
    CHECK(gen->legendaryId.empty());
    CHECK(gen->affixes.size() >= 3);
    CHECK(gen->affixes.size() <= 5);
    CHECK(gen->legendaryEffect == *D().Strings().Lookup(LocaleId::ZhCN, "sys.loot.genericLegendaryEffect"));
    // Crown of the Abyss is dungeon-exclusive: an overworld roll on the dragon helm is a generic legendary.
    std::optional<ItemInstance> helm = CreateItem(lc, "a_dragon_helm", 40, ItemQuality::Legendary);
    CHECK(helm->legendaryId.empty());
    lc.dungeon = true;
    helm = CreateItem(lc, "a_dragon_helm", 40, ItemQuality::Legendary);
    CHECK(helm->legendaryId == "leg_abyss_crown");
    // First match wins: the demon blade is always Soulreaver (Voidedge unreachable, Q17).
    CHECK(CreateItem(lc, "w_demon_blade", 40, ItemQuality::Legendary)->legendaryId == "leg_soulreaver");
    // Set pieces: the abyss walker pieces only in a dungeon.
    for (const SetPieceCandidate& c : SetPieceCandidates(D(), 40, false)) CHECK_FALSE(c.set->dungeonExclusive);
    bool walker = false;
    for (const SetPieceCandidate& c : SetPieceCandidates(D(), 40, true)) walker |= c.set->id == "set_abyss_walker";
    CHECK(walker);
  }

  TEST_CASE("set pieces (4.4): generateSetPiece and createItem(set)") {
    Rng rng(9);
    ItemUidGenerator uids;
    const LootContext lc{&D(), &rng, &uids};
    // createItem(set) on a piece base: that piece (port of the dead makeSetItem path).
    std::optional<ItemInstance> helm = CreateItem(lc, "a_leather_helm", 5, ItemQuality::Set);
    REQUIRE(helm.has_value());
    const SetDef* hunter = D().Items().FindSet("set_hunter");
    CHECK(helm->setId == "set_hunter");
    CHECK(helm->setPieceId == "set_hunter_helm");
    CHECK(helm->name == hunter->name + " " + D().Items().FindBase("a_leather_helm")->name);
    REQUIRE(helm->affixes.size() >= 3);
    REQUIRE(helm->affixes.size() <= 4);
    CHECK(helm->affixes[0].affixId == "set_hu_1");
    CHECK(helm->affixes[0].value == 5);  // fixed piece affixes are not level-scaled
    CHECK(helm->affixes[1].affixId == "set_hu_2");
    // createItem(set) on a base without a piece: 2..3 random affixes, no set.
    std::optional<ItemInstance> plain = CreateItem(lc, "w_rusty_sword", 5, ItemQuality::Set);
    CHECK(plain->setId.empty());
    CHECK(plain->affixes.size() >= 2);
    CHECK(plain->affixes.size() <= 3);
    // generateSetPiece: always a candidate piece with its fixed affixes first.
    for (int i = 0; i < 100; ++i) {
      std::optional<ItemInstance> p = GenerateSetPiece(lc, 6);
      REQUIRE(p.has_value());
      CHECK(p->quality == ItemQuality::Set);
      const SetDef* s = D().Items().FindSet(p->setId);
      REQUIRE(s != nullptr);
      const std::vector<FixedAffixDef>* fixed = s->AffixesForPiece(p->setPieceId);
      REQUIRE(fixed != nullptr);
      REQUIRE(p->affixes.size() >= fixed->size() + 1);
      CHECK(p->affixes.size() <= fixed->size() + 2);
      for (size_t k = 0; k < fixed->size(); ++k) CHECK(p->affixes[k].affixId == (*fixed)[k].affixId);
      CHECK(p->level == 6);
    }
    // No candidates -> nothing (the drop is lost).
    CHECK(SetPieceCandidates(D(), 200, false).empty());
    CHECK_FALSE(GenerateSetPiece(lc, 200).has_value());
  }

  TEST_CASE("createItem basics and buildItemName (4.1, 4.5)") {
    Rng rng(1);
    ItemUidGenerator uids;
    const LootContext lc{&D(), &rng, &uids};
    CHECK_FALSE(CreateItem(lc, "x_unknown", 5, ItemQuality::Magic).has_value());
    std::optional<ItemInstance> n = CreateItem(lc, "w_short_sword", 4, ItemQuality::Normal);
    REQUIRE(n.has_value());
    CHECK(n->uid == "i1");
    CHECK(n->name == D().Items().FindBase("w_short_sword")->name);
    CHECK(n->affixes.empty());
    CHECK(n->level == 4);
    CHECK(n->identified);
    CHECK(uids.Counter() == 2);
    // buildItemName: first prefix + base + " (" + first suffix + ")".
    ItemInstance m = Itm("w_short_sword", ItemQuality::Magic, 4);
    m.affixes = {FixedAffix("suf_life", 7), FixedAffix("pre_sharp", 2), FixedAffix("pre_strong", 3)};
    FinalizeItem(D(), m);
    const std::string base = D().Items().FindBase("w_short_sword")->name;
    CHECK(m.name == D().Items().FindAffix("pre_sharp")->name + base + " (" + D().Items().FindAffix("suf_life")->name + ")");
    CHECK(m.stats.Get(Stat::MaxHp) == 7);
    // Normal quality resets to the base name (refreshItem); same stat twice sums.
    ItemInstance k = Itm("w_short_sword", ItemQuality::Normal, 4);
    k.name = "x";
    k.affixes = {FixedAffix("pre_sharp", 2), FixedAffix("pre_keen", 5)};
    FinalizeItem(D(), k);
    CHECK(k.name == base);
    CHECK(k.stats.Get(Stat::Damage) == 7);
  }

  TEST_CASE("display names: FIX Q4 (legendary / set), zh and en assembly (15.1)") {
    I18n en = D().Strings();
    en.SetLocale(LocaleId::En);
    const I18n& zh = D().Strings();
    ItemInstance m = Itm("w_short_sword", ItemQuality::Rare, 4);
    m.affixes = {FixedAffix("pre_sharp", 2), FixedAffix("pre_strong", 3), FixedAffix("suf_life", 7)};
    CHECK(ItemDisplayName(m, D(), en) == "Sharp Strong Short Sword of Life");
    const std::string zhExpected = *zh.Lookup(LocaleId::ZhCN, "data.affix.pre_sharp") +
                                   *zh.Lookup(LocaleId::ZhCN, "data.affix.pre_strong") +
                                   *zh.Lookup(LocaleId::ZhCN, "data.item.w_short_sword.name") + "\xC2\xB7" +
                                   *zh.Lookup(LocaleId::ZhCN, "data.affix.suf_life");
    CHECK(ItemDisplayName(m, D(), zh) == zhExpected);
    // Normal -> base name.
    CHECK(ItemDisplayName(Itm("w_short_sword"), D(), en) == "Short Sword");
    // Named legendary and set piece (FIX Q4).
    ItemInstance grief = Itm("w_broad_sword", ItemQuality::Legendary, 8);
    grief.legendaryId = "leg_grief";
    grief.affixes = {ItemAffix{"leg_gr1", "", Stat::Damage, 18}};
    CHECK(ItemDisplayName(grief, D(), en) == "Grief");
    ItemInstance helm = Itm("a_leather_helm", ItemQuality::Set, 5);
    helm.setId = "set_hunter";
    helm.affixes = {FixedAffix("pre_strong", 2)};
    CHECK(ItemDisplayName(helm, D(), en) == "Wilds Hunter Leather Helm");
    // Unknown base -> stored name.
    ItemInstance ghost = Itm("x_removed_item");
    ghost.name = "ghost";
    CHECK(ItemDisplayName(ghost, D(), en) == "ghost");
    // Log arguments: a key for normal items, rendered text (with the quality prefix) otherwise.
    const I18nArg a = ItemNameArg("name", Itm("w_short_sword"), D());
    CHECK(a.isKey);
    CHECK(a.value == "data.item.w_short_sword.name");
    const I18nArg b = ItemNameArg("name", m, D(), true);
    CHECK_FALSE(b.isKey);
    CHECK(b.value == *zh.Lookup(LocaleId::ZhCN, "sys.inventory.qualityPrefix.rare") + zhExpected);
  }

  // ===================================================================================================================
  // Loot generation (5)
  // ===================================================================================================================
  TEST_CASE("5.3 / 5.7 quality thresholds") {
    using doctest::Approx;
    // L3, luck 5, non-elite, bonus 0.
    CHECK(QualityThreshold(D(), ItemQuality::Legendary, 3, 5, false, 0) == Approx(0.65));
    CHECK(QualityThreshold(D(), ItemQuality::Set, 3, 5, false, 0) == Approx(2.3));
    CHECK(QualityThreshold(D(), ItemQuality::Rare, 3, 5, false, 0) == Approx(16.5));
    CHECK(QualityThreshold(D(), ItemQuality::Magic, 3, 5, false, 0) == Approx(46.5));
    // goblin_chief (L5, elite) main drop, luck 5, affix bonus 5.
    CHECK(QualityThreshold(D(), ItemQuality::Legendary, 5, 5, true, 5) == Approx(1.4));
    CHECK(QualityThreshold(D(), ItemQuality::Set, 5, 5, true, 5) == Approx(11.3));
    CHECK(QualityThreshold(D(), ItemQuality::Rare, 5, 5, true, 5) == Approx(36.5));
    CHECK(QualityThreshold(D(), ItemQuality::Magic, 5, 5, true, 5) == Approx(49.0));
    // L21, luck 0, non-elite.
    CHECK(QualityThreshold(D(), ItemQuality::Legendary, 21, 0, false, 0) == Approx(1.5));
    CHECK(QualityThreshold(D(), ItemQuality::Set, 21, 0, false, 0) == Approx(2));
    CHECK(QualityThreshold(D(), ItemQuality::Rare, 21, 0, false, 0) == Approx(15));
    CHECK(QualityThreshold(D(), ItemQuality::Magic, 21, 0, false, 0) == Approx(45));
    CHECK(QualityThreshold(D(), ItemQuality::Normal, 21, 0, false, 0) == -1);
    // Sequential thresholds on one draw (scripted).
    Rng rng(1);
    struct Row {
      double draw;
      ItemQuality q;
    };
    const Row rows[] = {{0.0064, ItemQuality::Legendary}, {0.0066, ItemQuality::Set},   {0.0229, ItemQuality::Set},
                        {0.0231, ItemQuality::Rare},      {0.1649, ItemQuality::Rare},  {0.1651, ItemQuality::Magic},
                        {0.4649, ItemQuality::Magic},     {0.4651, ItemQuality::Normal}, {0.999, ItemQuality::Normal}};
    for (const Row& r : rows) {
      rng.Script({r.draw});
      CHECK(RollQuality(D(), 3, 5, false, 0, rng) == r.q);
    }
  }

  TEST_CASE("5.3 quality distribution (seeded, 100k rolls)") {
    Rng rng(2024);
    int counts[5] = {};
    const int n = 100000;
    for (int i = 0; i < n; ++i) ++counts[EnumIndex(RollQuality(D(), 3, 5, false, 0, rng))];
    using doctest::Approx;
    CHECK(counts[EnumIndex(ItemQuality::Legendary)] / double(n) == Approx(0.0065).epsilon(0.25));
    CHECK(counts[EnumIndex(ItemQuality::Set)] / double(n) == Approx(0.0165).epsilon(0.15));
    CHECK(counts[EnumIndex(ItemQuality::Rare)] / double(n) == Approx(0.142).epsilon(0.05));
    CHECK(counts[EnumIndex(ItemQuality::Magic)] / double(n) == Approx(0.30).epsilon(0.03));
    CHECK(counts[EnumIndex(ItemQuality::Normal)] / double(n) == Approx(0.535).epsilon(0.02));
  }

  TEST_CASE("5.9 Chapter 1 pools: equipment window, consumables, gems, set pieces") {
    const int expected[][2] = {{1, 13}, {2, 15}, {3, 16}, {4, 16}, {5, 19}, {6, 19}, {7, 27}, {11, 27}, {12, 26},
                               {13, 26}};
    for (const auto& e : expected) {
      CAPTURE(e[0]);
      CHECK(static_cast<int>(EquipmentDropPool(D(), e[0]).size()) == e[1]);
    }
    std::vector<std::string> l1 = BaseIds(EquipmentDropPool(D(), 1));
    CHECK(l1.front() == "w_rusty_sword");
    CHECK(std::find(l1.begin(), l1.end(), "w_short_sword") != l1.end());
    // Consumables: TP / ID scrolls are removed (I4 / I2).
    CHECK(BaseIds(ConsumableDropPool(D(), 1)) ==
          std::vector<std::string>{"c_hp_potion_s", "c_mp_potion_s", "c_antidote", "c_ley_fruit"});
    CHECK(BaseIds(ConsumableDropPool(D(), 5)) ==
          std::vector<std::string>{"c_hp_potion_s", "c_hp_potion_m", "c_mp_potion_s", "c_mp_potion_m", "c_antidote",
                                   "c_ley_fruit"});
    CHECK(BaseIds(GemDropPool(D(), 4)) ==
          std::vector<std::string>{"g_ruby_1", "g_sapphire_1", "g_emerald_1", "g_topaz_1"});
    CHECK(GemDropPool(D(), 5).size() == 5);    // + g_diamond_1
    CHECK(GemDropPool(D(), 10).size() == 9);   // + the _2 ruby / sapphire / emerald / topaz
    CHECK(GemDropPool(D(), 13).size() == 10);  // + g_diamond_2
    auto pieces = [](int level) {
      std::vector<std::string> out;
      for (const SetPieceCandidate& c : SetPieceCandidates(D(), level, false)) out.push_back(c.pieceId);
      return out;
    };
    CHECK(pieces(1) == std::vector<std::string>{"set_hunter_helm", "set_hunter_armor", "set_hunter_boots"});
    CHECK(pieces(3).size() == 4);  // + set_archmage_staff
    CHECK(pieces(5).size() == 7);  // + shadow armor / gloves / boots
    CHECK(pieces(10).size() == 11);
  }

  TEST_CASE("generated consumables and gems (5.4)") {
    Rng rng(77);
    ItemUidGenerator uids;
    const LootContext lc{&D(), &rng, &uids};
    std::set<int> qty;
    for (int i = 0; i < 200; ++i) {
      std::optional<ItemInstance> c = GenerateConsumable(lc, 1);
      REQUIRE(c.has_value());
      CHECK(c->level == 1);
      CHECK(c->quality == ItemQuality::Normal);
      qty.insert(c->quantity);
      std::optional<ItemInstance> g = GenerateGem(lc, 1);
      REQUIRE(g.has_value());
      CHECK(g->quantity == 1);
      CHECK(D().Items().FindBase(g->baseId)->isGem);
    }
    CHECK(qty == std::set<int>{1, 2, 3});
    // Wide window fallback: any equipment base when the window is empty.
    CHECK(GenerateEquipmentWide(lc, 500, ItemQuality::Magic).has_value());
    CHECK_FALSE(GenerateEquipment(lc, 500, ItemQuality::Magic).has_value());
  }

  TEST_CASE("5.7 drop chances (seeded): equipment 42.5 %, consumable 30 %, gem 5.5 %, elites drop more") {
    Rng rng(99);
    ItemUidGenerator uids;
    const LootContext lc{&D(), &rng, &uids};
    LootRollInput in;
    in.monsterLevel = 1;
    in.luck = 5;
    const int n = 20000;
    int equip = 0, cons = 0, gems = 0;
    size_t normalTotal = 0;
    for (int i = 0; i < n; ++i) {
      const std::vector<ItemInstance> loot = GenerateLoot(lc, in);
      normalTotal += loot.size();
      bool e = false, c = false, g = false;
      for (const ItemInstance& it : loot) {
        const ItemBaseDef* b = D().Items().FindBase(it.baseId);
        e |= b->IsEquipment();
        c |= b->type == ItemType::Consumable;
        g |= b->type == ItemType::Gem;
      }
      equip += e;
      cons += c;
      gems += g;
    }
    using doctest::Approx;
    CHECK(equip / double(n) == Approx(0.425).epsilon(0.04));
    CHECK(cons / double(n) == Approx(0.30).epsilon(0.05));
    CHECK(gems / double(n) == Approx(0.055).epsilon(0.12));
    LootRollInput elite = in;
    elite.elite = true;
    size_t eliteTotal = 0;
    int second = 0;
    for (int i = 0; i < n; ++i) {
      const std::vector<ItemInstance> loot = GenerateLoot(lc, elite);
      eliteTotal += loot.size();
      second += CountEquip(loot) >= 2;
    }
    CHECK(eliteTotal > normalTotal);
    CHECK(second / double(n) == Approx(0.825 * 0.525).epsilon(0.05));
  }

  TEST_CASE("5.7 / 5.8 chance boundaries in draw order (scripted)") {
    Rng rng(6);
    ItemUidGenerator uids;
    const LootContext lc{&D(), &rng, &uids};
    LootRollInput in;
    in.monsterLevel = 1;
    in.luck = 5;
    // Main drop 42.5 %: [chance] [quality] [base] then consumable / gem chances.
    rng.Script({0.4249, 0.99, 0.0, 0.99, 0.99});
    std::vector<ItemInstance> loot = GenerateLoot(lc, in);
    REQUIRE(loot.size() == 1);
    CHECK(loot[0].baseId == "w_rusty_sword");
    CHECK(rng.ScriptedRemaining() == 0);
    rng.Script({0.4251, 0.99, 0.99});
    CHECK(GenerateLoot(lc, in).empty());
    CHECK(rng.ScriptedRemaining() == 0);
    // Consumable 30 % flat: [main no] [consumable yes] [pick] [quantity] [gem no].
    rng.Script({0.99, 0.2999, 0.0, 0.99, 0.99});
    loot = GenerateLoot(lc, in);
    REQUIRE(loot.size() == 1);
    CHECK(loot[0].baseId == "c_hp_potion_s");
    CHECK(loot[0].quantity == 3);
    // Gem 5.5 % (luck 5) and 7.0 % (+ one affix of bonus 5).
    rng.Script({0.99, 0.99, 0.0549, 0.0});
    loot = GenerateLoot(lc, in);
    REQUIRE(loot.size() == 1);
    CHECK(loot[0].baseId == "g_ruby_1");
    rng.Script({0.99, 0.99, 0.0551});
    CHECK(GenerateLoot(lc, in).empty());
    in.affixLootBonus = 5;
    rng.Script({0.99, 0.99, 0.0699, 0.0});
    CHECK(GenerateLoot(lc, in).size() == 1);
    rng.Script({0.99, 0.99, 0.0701});
    CHECK(GenerateLoot(lc, in).empty());
    // Elite second drop 52.5 % (rolled with isElite = true).
    in.affixLootBonus = 0;
    in.elite = true;
    rng.Script({0.99, 0.5249, 0.99, 0.0, 0.99, 0.99});
    loot = GenerateLoot(lc, in);
    REQUIRE(loot.size() == 1);
    CHECK(loot[0].quality == ItemQuality::Normal);
    rng.Script({0.99, 0.5251, 0.99, 0.99});
    CHECK(GenerateLoot(lc, in).empty());
    CHECK(rng.ScriptedRemaining() == 0);
    // Affix-elite third drop: gate affix bonus >= 10, chance 30 + luck * 0.5 + bonus = 42.5 %.
    in.elite = false;
    in.affixLootBonus = 10;
    rng.Script({0.99, 0.4249, 0.99, 0.0, 0.99, 0.99});
    CHECK(GenerateLoot(lc, in).size() == 1);
    rng.Script({0.99, 0.4251, 0.99, 0.99});
    CHECK(GenerateLoot(lc, in).empty());
    CHECK(rng.ScriptedRemaining() == 0);
  }

  TEST_CASE("generateLoot is deterministic per seed (5.8)") {
    for (uint64_t seed : {1ull, 42ull, 987654321ull}) {
      Rng a(seed), b(seed);
      ItemUidGenerator ua, ub;
      const LootContext la{&D(), &a, &ua};
      const LootContext lb{&D(), &b, &ub};
      LootRollInput in;
      in.monsterLevel = 6;
      in.elite = true;
      in.isMiniBoss = true;
      in.luck = 8;
      in.affixLootBonus = 12;
      for (int i = 0; i < 50; ++i) CHECK(GenerateLoot(la, in) == GenerateLoot(lb, in));
      CHECK(a.GetState() == b.GetState());
    }
  }

  TEST_CASE("20.12 mini-boss floor: appended only when no equipment meets it (scripted draws)") {
    Rng rng(5);
    ItemUidGenerator uids;
    const LootContext lc{&D(), &rng, &uids};
    LootRollInput in;
    in.monsterLevel = 6;
    in.isMiniBoss = true;  // non-elite here so the script covers every draw up to the floor
    // main chance yes, quality 0.99 -> normal, base index 0 (w_rusty_sword) -> floor appends a magic item.
    rng.Script({0.0, 0.99, 0.0});
    std::vector<ItemInstance> loot = GenerateLoot(lc, in);
    REQUIRE(loot.size() >= 2);
    CHECK(loot[0].baseId == "w_rusty_sword");
    CHECK(loot[0].quality == ItemQuality::Normal);
    CHECK(loot[1].quality == ItemQuality::Magic);
    CHECK(IsEquipmentItem(loot[1], D()));
    // main drop a rare ring -> nothing appended.
    const std::vector<std::string> pool = BaseIds(EquipmentDropPool(D(), 6));
    const size_t ring = static_cast<size_t>(std::find(pool.begin(), pool.end(), "j_copper_ring") - pool.begin());
    REQUIRE(ring < pool.size());
    rng.Script({0.0, 0.10, (ring + 0.5) / pool.size()});
    loot = GenerateLoot(lc, in);
    REQUIRE(!loot.empty());
    CHECK(loot[0].baseId == "j_copper_ring");
    CHECK(loot[0].quality == ItemQuality::Rare);
    CHECK(CountEquip(loot) == 1);
  }

  TEST_CASE("mini-boss floors (endgame / story scrutiny tests, seeded)") {
    Rng rng(31337);
    ItemUidGenerator uids;
    const LootContext lc{&D(), &rng, &uids};
    LootRollInput zone;
    zone.monsterLevel = 6;
    zone.elite = true;
    zone.isMiniBoss = true;
    zone.luck = 10;
    LootRollInput sub = zone;
    sub.isSubDungeonMiniBoss = true;
    LootRollInput plain;
    plain.monsterLevel = 6;
    plain.luck = 10;
    int plainWithout = 0;
    for (int i = 0; i < 500; ++i) {
      CHECK(HasEquip(GenerateLoot(lc, zone), ItemQuality::Magic));
      CHECK(HasEquip(GenerateLoot(lc, sub), ItemQuality::Rare));
      plainWithout += HasEquip(GenerateLoot(lc, plain), ItemQuality::Magic) ? 0 : 1;
    }
    CHECK(plainWithout > 0);
    CHECK(QualityMeetsFloor(ItemQuality::Set, ItemQuality::Legendary));
    CHECK_FALSE(QualityMeetsFloor(ItemQuality::Magic, ItemQuality::Rare));
  }

  TEST_CASE("gems drop with high luck at level 20 (GemSocketing.test.ts)") {
    Rng rng(8);
    ItemUidGenerator uids;
    const LootContext lc{&D(), &rng, &uids};
    LootRollInput in;
    in.monsterLevel = 20;
    in.luck = 50;
    bool found = false;
    for (int i = 0; i < 200 && !found; ++i) {
      for (const ItemInstance& it : GenerateLoot(lc, in)) {
        if (D().Items().FindBase(it.baseId)->type == ItemType::Gem) found = true;
      }
    }
    CHECK(found);
  }

  TEST_CASE("difficulty loot mods (5.2): nightmare +3 item level, +1 affix on the max") {
    Rng rng(4);
    ItemUidGenerator uids;
    const LootContext lc{&D(), &rng, &uids};
    LootRollInput in;
    in.monsterLevel = 10;
    in.elite = true;
    in.difficulty = Difficulty::Nightmare;
    bool sawThree = false;
    for (int i = 0; i < 400; ++i) {
      for (const ItemInstance& it : GenerateLoot(lc, in)) {
        if (!IsEquipmentItem(it, D())) continue;
        CHECK(it.level == 13);
        if (it.quality == ItemQuality::Magic) {
          CHECK(it.affixes.size() <= 3);
          sawThree |= it.affixes.size() == 3;
        }
      }
    }
    CHECK(sawThree);
  }

  TEST_CASE("ground potions (5.1 step 6)") {
    PotionKind k = PotionKind::Mp;
    int32_t amount = 0;
    CHECK(IsGroundPotion(D(), "c_hp_potion_s", k, amount));
    CHECK(k == PotionKind::Hp);
    CHECK(amount == 50);
    CHECK(IsGroundPotion(D(), "c_hp_potion_l", k, amount));
    CHECK(amount == 400);
    CHECK(IsGroundPotion(D(), "c_mp_potion_m", k, amount));
    CHECK(k == PotionKind::Mp);
    CHECK(amount == 80);
    CHECK_FALSE(IsGroundPotion(D(), "c_antidote", k, amount));
    CHECK_FALSE(IsGroundPotion(D(), "c_ley_fruit", k, amount));
  }

  TEST_CASE("treasure cache (5.5): Lc = floor((min + max) / 2), elite, bonus floor(Lc / 10), gold [10+5L, 20+10L]") {
    const LootRollInput in = TreasureCacheRoll(1, 7, 5, Difficulty::Normal);
    CHECK(in.monsterLevel == 4);
    CHECK(in.elite);
    CHECK(in.affixLootBonus == 0);
    CHECK(in.luck == 5);
    int32_t lo = 0, hi = 0;
    TreasureCacheGold(1, 7, lo, hi);
    CHECK(lo == 30);
    CHECK(hi == 60);
    CHECK(TreasureCacheRoll(20, 25, 0, Difficulty::Hell).affixLootBonus == 2);
    CHECK(TreasureCacheRoll(20, 25, 0, Difficulty::Hell).difficulty == Difficulty::Hell);
  }

  // ===================================================================================================================
  // Quest pick-one gear (5.6, QuestEngine.test.ts QuestRewards)
  // ===================================================================================================================
  TEST_CASE("quest rewards: class weapon families, off-hand rule, quality, item level") {
    Rng rng(12);
    for (ClassId cls : {ClassId::Warrior, ClassId::Mage, ClassId::Rogue}) {
      const std::vector<WeaponType>& types = D().Items().loot.classWeaponTypes[EnumIndex(cls)];
      for (int i = 0; i < 20; ++i) {
        const ItemBaseDef* b = PickRewardBase(D(), RewardSlot::Weapon, cls, 30, rng);
        REQUIRE(b != nullptr);
        CHECK(std::find(types.begin(), types.end(), b->weaponType) != types.end());
        CHECK(b->levelReq <= 32);
      }
    }
    CHECK(PickRewardBase(D(), RewardSlot::Offhand, ClassId::Mage, 20, rng)->type == ItemType::Accessory);
    CHECK(PickRewardBase(D(), RewardSlot::Offhand, ClassId::Warrior, 20, rng)->weaponType == WeaponType::Shield);
    CHECK(PickRewardBase(D(), RewardSlot::Boots, ClassId::Rogue, 20, rng)->slot == EquipSlot::Boots);
    // Top 3 by levelReq desc (stable): rogue weapons usable at 12 -> long bow (10), stiletto (6), dagger (1).
    rng.Script({0.0});
    CHECK(PickRewardBase(D(), RewardSlot::Weapon, ClassId::Rogue, 12, rng)->id == "w_long_bow");
    rng.Script({0.5});
    CHECK(PickRewardBase(D(), RewardSlot::Weapon, ClassId::Rogue, 12, rng)->id == "w_stiletto");
    rng.Script({0.99});
    CHECK(PickRewardBase(D(), RewardSlot::Weapon, ClassId::Rogue, 12, rng)->id == "w_dagger");

    QuestDef q;
    q.id = "q_test";
    q.level = 10;
    q.category = QuestCategory::Main;
    CHECK(RewardItemLevel(q, 8) == 10);
    CHECK(RewardItemLevel(q, 13) == 13);
    CHECK(RewardItemLevel(q, 40) == 15);
    CHECK(RewardChoiceQuality(D(), q) == ItemQuality::Rare);
    q.category = QuestCategory::Side;
    CHECK(RewardChoiceQuality(D(), q) == ItemQuality::Magic);
    q.rewards.hasChoiceQuality = true;
    q.rewards.choiceQuality = ItemQuality::Legendary;
    CHECK(RewardChoiceQuality(D(), q) == ItemQuality::Legendary);

    QuestDef main;
    main.id = "q_main";
    main.level = 7;
    main.category = QuestCategory::Main;
    main.rewards.choices = {RewardSlot::Weapon, RewardSlot::Armor};
    ItemUidGenerator uids;
    const LootContext lc{&D(), &rng, &uids};
    const std::vector<ItemInstance> items = GenerateQuestRewardChoices(lc, main, ClassId::Rogue, 12);
    REQUIRE(items.size() == 2);
    for (const ItemInstance& it : items) {
      CHECK(it.identified);
      CHECK(it.quality == ItemQuality::Rare);
      CHECK(it.level == 12);
    }
    CHECK(D().Items().FindBase(items[1].baseId)->slot == EquipSlot::Armor);
  }

  // ===================================================================================================================
  // The bag (7)
  // ===================================================================================================================
  TEST_CASE("bag basics (smoke.test.ts InventorySystem)") {
    Inventory inv(D());
    ItemInstance a = Itm("w_rusty_sword", ItemQuality::Normal, 1);
    CHECK(inv.AddItem(a).ok);
    CHECK(inv.Bag().size() == 1);
    CHECK(inv.RemoveItem(a.uid).has_value());
    CHECK(inv.Bag().empty());
    CHECK_FALSE(inv.RemoveItem("nope").has_value());
    for (int i = 0; i < 100; ++i) CHECK(inv.AddItem(Itm("w_rusty_sword")).ok);
    CHECK(inv.IsFull());
    CHECK_FALSE(inv.AddItem(Itm("w_rusty_sword")).ok);
    CHECK(inv.Bag().size() == 100);
    CHECK(inv.Capacity() == 100);
  }

  TEST_CASE("20.8 stacking: one partial stack topped up, remainder appended; a full bag keeps the top-up") {
    Inventory inv(D());
    inv.MutableBag().push_back(Stack("c_hp_potion_s", 19));
    const AddResult r = inv.AddItem(Stack("c_hp_potion_s", 3));
    CHECK(r.ok);
    CHECK(r.stackedQuantity == 1);
    CHECK(r.newEntry);
    REQUIRE(inv.Bag().size() == 2);
    CHECK(inv.Bag()[0].quantity == 20);
    CHECK(inv.Bag()[1].quantity == 2);
    // 20.8 second vector: bag at 100 entries with c_hp_potion_s x19; adding x3 -> existing 20, returns false, the item
    // keeps quantity 2 (Q21 kept: the web topped up first, then refused the new entry).
    Inventory full(D());
    full.MutableBag().push_back(Stack("c_hp_potion_s", 19));
    for (int i = 0; i < 99; ++i) full.MutableBag().push_back(Itm("w_rusty_sword"));
    CHECK(full.CanAdd(Stack("c_hp_potion_s", 1)));  // a full stack merge needs no slot
    ItemInstance three = Stack("c_hp_potion_s", 3);
    CHECK_FALSE(full.CanAdd(three));
    const AddResult partial = full.AddItem(three);
    CHECK_FALSE(partial.ok);
    CHECK(partial.stackedQuantity == 1);
    CHECK(partial.remaining == 2);
    CHECK_FALSE(partial.newEntry);
    CHECK(full.Bag()[0].quantity == 20);
    CHECK(full.Bag().size() == 100);
    // Nothing left to top up: refused untouched.
    const AddResult none = full.AddItem(Stack("c_hp_potion_s", 1));
    CHECK_FALSE(none.ok);
    CHECK(none.stackedQuantity == 0);
    CHECK(none.remaining == 1);
    CHECK(full.CountOf("c_hp_potion_s") == 20);
    Inventory fits(D());
    fits.MutableBag().push_back(Stack("c_hp_potion_s", 19));
    for (int i = 0; i < 99; ++i) fits.MutableBag().push_back(Itm("w_rusty_sword"));
    const AddResult one = fits.AddItem(Stack("c_hp_potion_s", 1));
    CHECK(one.ok);
    CHECK(one.remaining == 0);
    CHECK(fits.Bag()[0].quantity == 20);
    // Q21: only the first partial stack is topped up.
    Inventory two(D());
    two.MutableBag().push_back(Stack("c_mp_potion_s", 18));
    two.MutableBag().push_back(Stack("c_mp_potion_s", 18));
    CHECK(two.AddItem(Stack("c_mp_potion_s", 5)).ok);
    REQUIRE(two.Bag().size() == 3);
    CHECK(two.Bag()[0].quantity == 20);
    CHECK(two.Bag()[1].quantity == 18);
    CHECK(two.Bag()[2].quantity == 3);
    CHECK(two.CountOf("c_mp_potion_s") == 41);
    // Non-stackables never merge.
    Inventory gear(D());
    gear.AddItem(Itm("w_dagger"));
    gear.AddItem(Itm("w_dagger"));
    CHECK(gear.Bag().size() == 2);
  }

  TEST_CASE("removeItem split gets a new uid (port fix)") {
    Inventory inv(D());
    ItemInstance s = Stack("c_hp_potion_s", 5);
    s.uid = "keep";
    inv.MutableBag().push_back(s);
    ItemUidGenerator uids;
    std::optional<ItemInstance> part = inv.RemoveItem("keep", 2, &uids);
    REQUIRE(part.has_value());
    CHECK(part->quantity == 2);
    CHECK(part->uid == "i1");
    CHECK(inv.Bag()[0].quantity == 3);
    std::optional<ItemInstance> rest = inv.RemoveItem("keep", 3);
    CHECK(rest->uid == "keep");
    CHECK(inv.Bag().empty());
  }

  TEST_CASE("sort (7.3): quality, then type, then name; destroy normals") {
    Inventory inv(D());
    inv.MutableBag() = {Itm("c_hp_potion_s"), Itm("w_dagger", ItemQuality::Rare), Itm("m_scrap"),
                        Itm("j_copper_ring", ItemQuality::Legendary), Itm("a_leather_armor", ItemQuality::Magic),
                        Itm("w_rusty_sword"), Itm("a_cloth_cap"), Itm("g_ruby_1"), Itm("a_leather_helm", ItemQuality::Set)};
    inv.SortBag();
    std::vector<std::string> order;
    for (const ItemInstance& it : inv.Bag()) order.push_back(it.baseId);
    CHECK(order[0] == "j_copper_ring");    // legendary
    CHECK(order[1] == "a_leather_helm");   // set
    CHECK(order[2] == "w_dagger");         // rare
    CHECK(order[3] == "a_leather_armor");  // magic
    CHECK(order[4] == "w_rusty_sword");    // normal weapon
    CHECK(order[5] == "a_cloth_cap");      // normal armour
    CHECK(order[6] == "c_hp_potion_s");    // consumable
    CHECK(order[7] == "g_ruby_1");         // gem
    CHECK(order[8] == "m_scrap");          // material
    // Port rule (7.3): ties within quality and type sort by the localised display name, here English.
    {
      I18n en = D().Strings();
      en.SetLocale(LocaleId::En);
      Inventory names(D());
      ItemInstance sharp = Itm("w_short_sword", ItemQuality::Magic, 4);
      sharp.affixes = {FixedAffix("pre_sharp", 2)};  // "Sharp Short Sword"
      ItemInstance strong = Itm("w_dagger", ItemQuality::Magic, 4);
      strong.affixes = {FixedAffix("pre_strong", 2)};  // "Strong Dagger"
      ItemInstance life = Itm("w_rusty_sword", ItemQuality::Magic, 4);
      life.affixes = {FixedAffix("suf_life", 2)};  // "Rusty Sword of Life"
      names.MutableBag() = {Itm("w_short_sword"), Itm("w_dagger"), strong, Itm("w_rusty_sword"), life, sharp};
      names.SortBag(&en);
      std::vector<std::string> got;
      for (const ItemInstance& it : names.Bag()) got.push_back(ItemDisplayName(it, D(), en));
      CHECK(got == std::vector<std::string>{"Rusty Sword of Life", "Sharp Short Sword", "Strong Dagger", "Dagger",
                                            "Rusty Sword", "Short Sword"});
      names.MutableStash() = {Itm("w_short_sword"), Itm("w_dagger")};
      names.SortStash(&en);
      CHECK(names.Stash()[0].baseId == "w_dagger");
    }
    CHECK(inv.DestroyNormalItems() == 2);  // the normal sword and cap only
    CHECK(inv.Bag().size() == 7);
    CHECK(inv.CountOf("c_hp_potion_s") == 1);
    CHECK(inv.CountOf("m_scrap") == 1);
  }

  TEST_CASE("identify (7.6, vestigial)") {
    Inventory inv(D());
    ItemInstance u = Itm("w_dagger", ItemQuality::Magic);
    u.identified = false;
    inv.MutableBag().push_back(u);
    CHECK(inv.IdentifyItem(u.uid) == InvResult::NeedScroll);
    inv.MutableBag().push_back(Stack("c_id_scroll", 1));
    CHECK(inv.IdentifyItem(u.uid) == InvResult::Ok);
    CHECK(inv.FindInBag(u.uid)->identified);
    CHECK(inv.Bag().size() == 1);
    CHECK(inv.IdentifyItem(u.uid) == InvResult::NotUsable);
  }

  // ===================================================================================================================
  // Equipment (8)
  // ===================================================================================================================
  TEST_CASE("equip / unequip (8.1) with I3, FIX Q13 (swap in place, full bag) and FIX Q18 (ring slot)") {
    Inventory inv(D());
    ItemInstance sword = Itm("w_rusty_sword", ItemQuality::Normal, 1);
    inv.AddItem(sword);
    EquipSlot slot = EquipSlot::Helmet;
    CHECK(inv.Equip(sword.uid, 1, &slot) == InvResult::Ok);
    CHECK(slot == EquipSlot::Weapon);
    CHECK(inv.Bag().empty());
    REQUIRE(inv.Equipped(EquipSlot::Weapon) != nullptr);
    CHECK(inv.Unequip(EquipSlot::Weapon) == InvResult::Ok);
    CHECK(inv.Bag().size() == 1);
    CHECK(inv.Equipped(EquipSlot::Weapon) == nullptr);
    CHECK(inv.Unequip(EquipSlot::Weapon) == InvResult::UnknownItem);
    // I3: levelReq.
    ItemInstance broad = Itm("w_broad_sword");
    inv.AddItem(broad);
    CHECK(inv.Equip(broad.uid, 7) == InvResult::LevelTooLow);
    CHECK(inv.Equip(broad.uid, 8) == InvResult::Ok);
    CHECK(inv.Equip("c_none", 50) == InvResult::UnknownItem);
    ItemInstance pot = Stack("c_hp_potion_s", 1);
    inv.AddItem(pot);
    CHECK(inv.Equip(pot.uid, 50) == InvResult::NotEquipment);
    // FIX Q13: the worn item takes the new item's bag position, even with a full bag.
    while (!inv.IsFull()) inv.AddItem(Itm("a_cloth_cap"));
    const int32_t idx = inv.BagIndex(sword.uid);
    REQUIRE(idx >= 0);
    CHECK(inv.Equip(sword.uid, 1) == InvResult::Ok);
    CHECK(inv.Bag()[static_cast<size_t>(idx)].uid == broad.uid);
    CHECK(inv.Equipped(EquipSlot::Weapon)->uid == sword.uid);
    CHECK(inv.Unequip(EquipSlot::Weapon) == InvResult::BagFull);
    // FIX Q18: free ring1, then ring2, then the weaker ring.
    Inventory rings(D());
    ItemInstance strong = Itm("j_copper_ring", ItemQuality::Magic, 5);
    strong.affixes = {StatAffix(Stat::Str, 9)};
    ItemInstance weak = Itm("j_copper_ring", ItemQuality::Magic, 5);
    weak.affixes = {StatAffix(Stat::Str, 1)};
    ItemInstance third = Itm("j_copper_ring", ItemQuality::Normal, 5);
    rings.AddItem(strong);
    rings.AddItem(weak);
    rings.AddItem(third);
    CHECK(rings.Equip(strong.uid, 50, &slot) == InvResult::Ok);
    CHECK(slot == EquipSlot::Ring1);
    CHECK(rings.Equip(weak.uid, 50, &slot) == InvResult::Ok);
    CHECK(slot == EquipSlot::Ring2);
    CHECK(rings.Equip(third.uid, 50, &slot) == InvResult::Ok);
    CHECK(slot == EquipSlot::Ring2);  // replaces the weaker ring
    CHECK(rings.Equipped(EquipSlot::Ring1)->uid == strong.uid);
    CHECK(rings.Bag()[0].uid == weak.uid);
  }

  TEST_CASE("20.9 aggregation: base damage, base defense, affixes, diamond allStats") {
    Inventory inv(D());
    ItemInstance armor = Itm("a_leather_armor", ItemQuality::Magic, 5);
    armor.affixes = {FixedAffix("pre_sturdy", 2), FixedAffix("suf_life", 10)};
    ComputeItemStats(armor);
    ItemInstance sword = Itm("w_short_sword", ItemQuality::Normal, 5);
    sword.sockets = {GemOf("g_diamond_1")};
    ComputeItemStats(sword);
    inv.MutableEquipment()[EnumIndex(EquipSlot::Armor)] = armor;
    inv.MutableEquipment()[EnumIndex(EquipSlot::Weapon)] = sword;
    const StatBag s = inv.EquipmentStatBag();
    CHECK(s.Get(Stat::WeaponDamageMin) == 5);
    CHECK(s.Get(Stat::WeaponDamageMax) == 10);
    CHECK(s.Get(Stat::Defense) == 10);
    CHECK(s.Get(Stat::MaxHp) == 10);
    for (Stat k : {Stat::Str, Stat::Dex, Stat::Int, Stat::Vit, Stat::Spi, Stat::Lck}) CHECK(s.Get(k) == 3);
    CHECK_FALSE(s.Has(Stat::AllStats));
    const EquipStats e = inv.GearStats();
    CHECK(e.Get(Stat::Defense) == 10);
    CHECK(e.Get(Stat::Lck) == 3);
    CHECK(e.Get(Stat::WeaponDamageMin) == 0);  // not an EquipStats key (C10 inert)
    // Shields add 0 weapon damage and no defense.
    Inventory sh(D());
    sh.MutableEquipment()[EnumIndex(EquipSlot::Offhand)] = Itm("w_wooden_shield");
    CHECK(sh.EquipmentStatBag().Get(Stat::Defense) == 0);
    CHECK(sh.EquipmentStatBag().Get(Stat::WeaponDamageMax) == 0);
  }

  TEST_CASE("20.10 set bonuses (cumulative, by setId - Q14 duplicate ring counts twice)") {
    Inventory inv(D());
    auto piece = [&](std::string_view baseId, std::string_view pieceId, EquipSlot slot) {
      ItemInstance it = Itm(baseId, ItemQuality::Set, 10);
      it.setId = "set_hunter";
      it.setPieceId = std::string(pieceId);
      for (const FixedAffixDef& f : *D().Items().FindSet("set_hunter")->AffixesForPiece(pieceId)) {
        it.affixes.push_back(ItemAffix{f.affixId, f.name, f.stat, f.value});
      }
      ComputeItemStats(it);
      inv.MutableEquipment()[EnumIndex(slot)] = it;
    };
    piece("a_leather_helm", "set_hunter_helm", EquipSlot::Helmet);
    piece("a_leather_armor", "set_hunter_armor", EquipSlot::Armor);
    piece("a_leather_boots", "set_hunter_boots", EquipSlot::Boots);
    CHECK(inv.EquippedSetCount("set_hunter") == 3);
    const StatBag s = inv.EquipmentStatBag();
    CHECK(s.Get(Stat::CritRate) == 5);
    CHECK(s.Get(Stat::Dex) == 18);
    CHECK(s.Get(Stat::Defense) == 29);  // 15 + base 5 + 8 + 1
    CHECK(s.Get(Stat::HpRegen) == 4);
    CHECK(s.Get(Stat::MoveSpeed) == 40);
    CHECK(s.Get(Stat::AttackSpeed) == 15);
    CHECK(s.Get(Stat::MagicFind) == 30);
    CHECK(s.Get(Stat::KillHealPercent) == 3);
    CHECK(s.Get(Stat::DoubleShot) == 0);  // 4-piece bonus not reached
    // Q14: two archmage rings count as two pieces.
    Inventory r(D());
    ItemInstance ring = Itm("j_gold_ring", ItemQuality::Set, 20);
    ring.setId = "set_archmage";
    r.MutableEquipment()[EnumIndex(EquipSlot::Ring1)] = ring;
    ring.uid = "other";
    r.MutableEquipment()[EnumIndex(EquipSlot::Ring2)] = ring;
    CHECK(r.EquippedSetCount("set_archmage") == 2);
    CHECK(r.EquipmentStatBag().Get(Stat::MaxManaPercent) == 25);
  }

  TEST_CASE("C11: legendary special effects with a combat consumer are item stats") {
    Rng rng(1);
    ItemUidGenerator uids;
    const LootContext lc{&D(), &rng, &uids};
    Inventory inv(D());
    inv.MutableEquipment()[EnumIndex(EquipSlot::Weapon)] = *CreateItem(lc, "w_broad_sword", 10, ItemQuality::Legendary);
    inv.MutableEquipment()[EnumIndex(EquipSlot::Boots)] = *CreateItem(lc, "a_leather_boots", 10, ItemQuality::Legendary);
    inv.MutableEquipment()[EnumIndex(EquipSlot::Offhand)] = *CreateItem(lc, "w_iron_shield", 10, ItemQuality::Legendary);
    inv.MutableEquipment()[EnumIndex(EquipSlot::Necklace)] = *CreateItem(lc, "j_jade_amulet", 10, ItemQuality::Legendary);
    const EquipStats e = inv.GearStats();
    CHECK(e.Get(Stat::IgnoreDefense) == 20);  // Grief
    CHECK(e.Get(Stat::DodgeCounter) == 1);    // Shadowstep
    // Aegis deathDefiance / Mara's allStatsBonus have no combat consumer: nothing added (and no allStats expansion).
    const StatBag s = inv.EquipmentStatBag();
    CHECK(s.Get(Stat::Str) == JsRound(5 * 0.6));  // Mara's fixed str 5 only
  }

  // ===================================================================================================================
  // Gems and sockets (9, GemSocketing.test.ts)
  // ===================================================================================================================
  TEST_CASE("socketing: insert, stack decrement, full sockets, empty slot, missing / non-gem") {
    Inventory inv(D());
    ItemInstance sword = Itm("w_short_sword");
    inv.MutableEquipment()[EnumIndex(EquipSlot::Weapon)] = sword;
    ItemInstance gem = Stack("g_ruby_1", 1);
    inv.MutableBag().push_back(gem);
    CHECK(inv.SocketGem(EquipSlot::Weapon, gem.uid) == InvResult::Ok);
    const ItemInstance* w = inv.Equipped(EquipSlot::Weapon);
    REQUIRE(w->sockets.size() == 1);
    CHECK(w->sockets[0].gemId == "g_ruby_1");
    CHECK(w->sockets[0].stat == Stat::Str);
    CHECK(w->sockets[0].value == 5);
    CHECK(w->sockets[0].tier == 1);
    CHECK(inv.FindInBag(gem.uid) == nullptr);
    // Full sockets: the gem stays in the bag.
    ItemInstance sap = Stack("g_sapphire_1", 1);
    inv.MutableBag().push_back(sap);
    CHECK(inv.SocketGem(EquipSlot::Weapon, sap.uid) == InvResult::NoSockets);
    CHECK(inv.FindInBag(sap.uid) != nullptr);
    CHECK(inv.SocketGem(EquipSlot::Armor, sap.uid) == InvResult::UnknownItem);  // empty slot
    CHECK(inv.SocketGem(EquipSlot::Weapon, "nonexistent") == InvResult::NoSockets);
    // Stack decrement on a free socket.
    Inventory inv2(D());
    inv2.MutableEquipment()[EnumIndex(EquipSlot::Weapon)] = Itm("w_short_sword");
    ItemInstance three = Stack("g_ruby_1", 3);
    inv2.MutableBag().push_back(three);
    CHECK(inv2.SocketGem(EquipSlot::Weapon, three.uid) == InvResult::Ok);
    CHECK(inv2.FindInBag(three.uid)->quantity == 2);
    // Missing gem / non-gem.
    Inventory inv3(D());
    inv3.MutableEquipment()[EnumIndex(EquipSlot::Weapon)] = Itm("w_short_sword");
    CHECK(inv3.SocketGem(EquipSlot::Weapon, "nonexistent") == InvResult::UnknownItem);
    ItemInstance potion = Stack("c_hp_potion_s", 1);
    inv3.MutableBag().push_back(potion);
    CHECK(inv3.SocketGem(EquipSlot::Weapon, potion.uid) == InvResult::NotAGem);
    // 0-socket base.
    Inventory inv4(D());
    inv4.MutableEquipment()[EnumIndex(EquipSlot::Weapon)] = Itm("w_rusty_sword");
    ItemInstance r = Stack("g_ruby_1", 1);
    inv4.MutableBag().push_back(r);
    CHECK(inv4.SocketGem(EquipSlot::Weapon, r.uid) == InvResult::NoSockets);
  }

  TEST_CASE("socketing: multi-socket items, capacity per slot") {
    Inventory inv(D());
    inv.MutableEquipment()[EnumIndex(EquipSlot::Weapon)] = Itm("w_demon_blade");
    std::vector<ItemInstance> gems = {Stack("g_ruby_3", 1), Stack("g_sapphire_3", 1), Stack("g_emerald_3", 1)};
    for (const ItemInstance& g : gems) inv.MutableBag().push_back(g);
    for (const ItemInstance& g : gems) CHECK(inv.SocketGem(EquipSlot::Weapon, g.uid) == InvResult::Ok);
    CHECK(inv.Equipped(EquipSlot::Weapon)->sockets.size() == 3);
    ItemInstance extra = Stack("g_topaz_1", 1);
    inv.MutableBag().push_back(extra);
    CHECK(inv.SocketGem(EquipSlot::Weapon, extra.uid) == InvResult::NoSockets);
    // getMaxSockets.
    Inventory caps(D());
    CHECK(caps.SocketCapacity(EquipSlot::Weapon) == 0);
    for (const auto& [id, n] : std::vector<std::pair<std::string, int>>{
             {"w_rusty_sword", 0}, {"w_short_sword", 1}, {"w_claymore", 2}, {"w_demon_blade", 3}}) {
      caps.MutableEquipment()[EnumIndex(EquipSlot::Weapon)] = Itm(id);
      CHECK(caps.SocketCapacity(EquipSlot::Weapon) == n);
    }
    for (const auto& [id, n] : std::vector<std::pair<std::string, int>>{
             {"a_quilted_armor", 0}, {"a_chain_mail", 1}, {"a_plate_armor", 2}}) {
      caps.MutableEquipment()[EnumIndex(EquipSlot::Armor)] = Itm(id);
      CHECK(caps.SocketCapacity(EquipSlot::Armor) == n);
    }
    caps.MutableEquipment()[EnumIndex(EquipSlot::Ring1)] = Itm("j_copper_ring");
    CHECK(caps.SocketCapacity(EquipSlot::Ring1) == 0);
  }

  TEST_CASE("unsocketing: back to the bag, stacking, FIX Q19, invalid index, right gem") {
    ItemUidGenerator uids;
    Inventory inv(D());
    ItemInstance sword = Itm("w_short_sword");
    sword.sockets = {GemOf("g_ruby_1")};
    inv.MutableEquipment()[EnumIndex(EquipSlot::Weapon)] = sword;
    CHECK(inv.UnsocketGem(EquipSlot::Weapon, 0, uids) == InvResult::Ok);
    CHECK(inv.Equipped(EquipSlot::Weapon)->sockets.empty());
    REQUIRE(inv.Bag().size() == 1);
    CHECK(inv.Bag()[0].baseId == "g_ruby_1");
    CHECK(inv.Bag()[0].quantity == 1);
    CHECK(inv.Bag()[0].level == 1);
    CHECK(inv.Bag()[0].uid == "i1");
    // Stacks with an existing gem.
    Inventory st(D());
    st.MutableEquipment()[EnumIndex(EquipSlot::Weapon)] = sword;
    st.MutableBag().push_back(Stack("g_ruby_1", 3));
    CHECK(st.UnsocketGem(EquipSlot::Weapon, 0, uids) == InvResult::Ok);
    CHECK(st.Bag().size() == 1);
    CHECK(st.Bag()[0].quantity == 4);
    // Full bag without a stack: refused, socket kept.
    Inventory full(D());
    full.MutableEquipment()[EnumIndex(EquipSlot::Weapon)] = sword;
    for (int i = 0; i < 100; ++i) full.MutableBag().push_back(Itm("w_rusty_sword"));
    CHECK(full.UnsocketGem(EquipSlot::Weapon, 0, uids) == InvResult::BagFull);
    CHECK(full.Equipped(EquipSlot::Weapon)->sockets.size() == 1);
    // FIX Q19: full bag but the gem stacks -> allowed.
    full.MutableBag().back() = Stack("g_ruby_1", 2);
    CHECK(full.UnsocketGem(EquipSlot::Weapon, 0, uids) == InvResult::Ok);
    CHECK(full.Bag().back().quantity == 3);
    // Invalid index / empty slot.
    Inventory e(D());
    e.MutableEquipment()[EnumIndex(EquipSlot::Weapon)] = Itm("w_short_sword");
    CHECK(e.UnsocketGem(EquipSlot::Weapon, 0, uids) == InvResult::InvalidIndex);
    CHECK(e.UnsocketGem(EquipSlot::Weapon, -1, uids) == InvResult::InvalidIndex);
    CHECK(e.UnsocketGem(EquipSlot::Armor, 0, uids) == InvResult::UnknownItem);
    // The right gem leaves a multi-socket item.
    Inventory m(D());
    ItemInstance clay = Itm("w_claymore");
    clay.sockets = {GemOf("g_ruby_1"), GemOf("g_sapphire_2")};
    m.MutableEquipment()[EnumIndex(EquipSlot::Weapon)] = clay;
    CHECK(m.UnsocketGem(EquipSlot::Weapon, 0, uids) == InvResult::Ok);
    REQUIRE(m.Equipped(EquipSlot::Weapon)->sockets.size() == 1);
    CHECK(m.Equipped(EquipSlot::Weapon)->sockets[0].gemId == "g_sapphire_2");
  }

  TEST_CASE("gem stat flow: item stats -> equipment stats -> EquipStats") {
    ItemUidGenerator uids;
    Inventory inv(D());
    inv.MutableEquipment()[EnumIndex(EquipSlot::Weapon)] = Itm("w_short_sword");
    ItemInstance ruby2 = Stack("g_ruby_2", 1);
    inv.MutableBag().push_back(ruby2);
    inv.SocketGem(EquipSlot::Weapon, ruby2.uid);
    CHECK(inv.Equipped(EquipSlot::Weapon)->stats.Get(Stat::Str) == 12);
    inv.MutableEquipment()[EnumIndex(EquipSlot::Armor)] = Itm("a_chain_mail");
    ItemInstance sap = Stack("g_sapphire_1", 1);
    inv.MutableBag().push_back(sap);
    inv.SocketGem(EquipSlot::Armor, sap.uid);
    CHECK(inv.EquipmentStatBag().Get(Stat::Str) == 12);
    CHECK(inv.EquipmentStatBag().Get(Stat::Int) == 5);
    CHECK(inv.GearStats().Get(Stat::Str) == 12);
    // Removing the gem removes the stat.
    CHECK(inv.UnsocketGem(EquipSlot::Weapon, 0, uids) == InvResult::Ok);
    CHECK_FALSE(inv.EquipmentStatBag().Has(Stat::Str));
    // Topaz magicFind and diamond allStats.
    Inventory t(D());
    t.MutableEquipment()[EnumIndex(EquipSlot::Weapon)] = Itm("w_claymore");
    ItemInstance topaz = Stack("g_topaz_2", 1);
    ItemInstance diamond = Stack("g_diamond_1", 1);
    t.MutableBag() = {topaz, diamond};
    t.SocketGem(EquipSlot::Weapon, topaz.uid);
    t.SocketGem(EquipSlot::Weapon, diamond.uid);
    const EquipStats e = t.GearStats();
    CHECK(e.Get(Stat::MagicFind) == 10);
    for (Stat k : {Stat::Str, Stat::Dex, Stat::Int, Stat::Vit, Stat::Spi, Stat::Lck}) CHECK(e.Get(k) == 3);
    // Gem + affix on the same item.
    Inventory a(D());
    ItemInstance sw = Itm("w_short_sword");
    sw.affixes = {StatAffix(Stat::Str, 10)};
    ComputeItemStats(sw);
    a.MutableEquipment()[EnumIndex(EquipSlot::Weapon)] = sw;
    ItemInstance r1 = Stack("g_ruby_1", 1);
    a.MutableBag().push_back(r1);
    a.SocketGem(EquipSlot::Weapon, r1.uid);
    CHECK(a.Equipped(EquipSlot::Weapon)->stats.Get(Stat::Str) == 15);
    CHECK(a.EquipmentStatBag().Get(Stat::Str) == 15);
    // Punched sockets accept gems (CraftingSystem.test.ts).
    Inventory p(D());
    ItemInstance rusty = Itm("w_rusty_sword", ItemQuality::Normal, 5);
    rusty.bonusSockets = 1;
    p.MutableEquipment()[EnumIndex(EquipSlot::Weapon)] = rusty;
    ItemInstance r2 = Stack("g_ruby_1", 1);
    p.MutableBag().push_back(r2);
    CHECK(p.SocketCapacity(EquipSlot::Weapon) == 1);
    CHECK(p.SocketGem(EquipSlot::Weapon, r2.uid) == InvResult::Ok);
    CHECK(p.Equipped(EquipSlot::Weapon)->stats.Get(Stat::Str) == 5);
  }

  // ===================================================================================================================
  // Stash (10) and trade (12.4-12.5)
  // ===================================================================================================================
  TEST_CASE("stash: capacity, whole entries, no stacking, overflow, take stacks into the bag") {
    Inventory inv(D());
    ItemInstance a = Stack("c_hp_potion_s", 5);
    ItemInstance b = Stack("c_hp_potion_s", 4);
    inv.MutableBag() = {a, b};
    CHECK(inv.MoveToStash(a.uid, 1) == InvResult::Ok);
    CHECK(inv.MoveToStash(b.uid, 1) == InvResult::StashFull);
    CHECK(inv.MoveToStash(b.uid, 80) == InvResult::Ok);
    CHECK(inv.Stash().size() == 2);  // no stacking inside the stash
    CHECK(inv.Bag().empty());
    CHECK(inv.MoveToStash("nope", 80) == InvResult::UnknownItem);
    CHECK(inv.MoveFromStash(a.uid) == InvResult::Ok);
    CHECK(inv.MoveFromStash(b.uid) == InvResult::Ok);
    REQUIRE(inv.Bag().size() == 1);
    CHECK(inv.Bag()[0].quantity == 9);  // stacked into the bag
    CHECK(inv.MoveFromStash("nope") == InvResult::UnknownItem);
    // Overflow ignores capacity; take with a full bag fails.
    for (int i = 0; i < 99; ++i) inv.MutableBag().push_back(Itm("w_rusty_sword"));
    ItemInstance c = Itm("w_dagger");
    inv.PushStashOverflow(c);
    CHECK(inv.MoveFromStash(c.uid) == InvResult::BagFull);
    CHECK(inv.Stash().size() == 1);
  }

  TEST_CASE("20.7 prices: sell (I9 quality multipliers), buyback x5, FIFO 5") {
    Inventory inv(D());
    ItemInstance p = Stack("c_hp_potion_s", 1);
    inv.MutableBag().push_back(p);
    CHECK(ItemUnitSellPrice(p, D()) == 5);
    CHECK(inv.Sell(p.uid) == 5);
    REQUIRE(inv.BuybackList().size() == 1);
    CHECK(inv.BuybackList()[0].price == 25);
    ItemInstance s3 = Stack("c_hp_potion_s", 3);
    inv.MutableBag().push_back(s3);
    CHECK(inv.Sell(s3.uid) == 15);
    CHECK(inv.BuybackList()[1].price == 75);
    CHECK(ItemSellPrice(Itm("w_short_sword"), D()) == 15);
    CHECK(ItemSellPrice(Itm("a_leather_belt"), D()) == 2);
    CHECK(ItemSellPrice(Itm("w_broad_sword", ItemQuality::Legendary), D()) == 160);  // 40 x 4 (I9)
    CHECK(ItemSellPrice(Itm("w_short_sword", ItemQuality::Magic), D()) == 22);       // floor(15 x 1.5)
    CHECK(ItemSellPrice(Itm("w_short_sword", ItemQuality::Rare), D()) == 37);        // floor(15 x 2.5)
    CHECK(ItemSellPrice(Itm("w_short_sword", ItemQuality::Set), D()) == 60);
    CHECK(ItemSellPrice(Itm("x_removed_item"), D()) == 1);
    CHECK_FALSE(inv.Sell("nope").has_value());
    // FIFO keeps the 5 most recent.
    for (int i = 0; i < 5; ++i) {
      ItemInstance d = Itm("w_dagger");
      d.uid = "d" + std::to_string(i);
      inv.MutableBag().push_back(d);
      inv.Sell(d.uid);
    }
    REQUIRE(inv.BuybackList().size() == 5);
    CHECK(inv.BuybackList()[0].item.uid == "d0");
    // Buyback: appended without stacking, uid preserved.
    int64_t cost = 0;
    ItemInstance got;
    CHECK(inv.Buyback(9, cost, got) == InvResult::InvalidIndex);
    CHECK(inv.Buyback(0, cost, got) == InvResult::Ok);
    CHECK(cost == 40);
    CHECK(got.uid == "d0");
    CHECK(inv.Bag().back().uid == "d0");
    CHECK(inv.BuybackList().size() == 4);
    for (int i = 0; i < 100; ++i) inv.MutableBag().push_back(Itm("w_rusty_sword"));
    CHECK(inv.Buyback(0, cost, got) == InvResult::BagFull);
  }

  // ===================================================================================================================
  // Crafting (13, CraftingSystem.test.ts)
  // ===================================================================================================================
  TEST_CASE("crafting costs (13.1, 20.4)") {
    CHECK(CraftGoldUnit(1) == 36);
    CHECK(CraftGoldUnit(5) == 60);
    CHECK(CraftGoldUnit(40) == 270);
    CHECK(CraftGoldUnit(0) == 36);
    CHECK(CraftGoldUnit(D(), 0) == 36);
    CHECK(CraftGoldUnit(D(), 40) == 270);
    const CraftCost lo = ComputeCraftCost(D(), CraftAction::Reforge, Itm("w_short_sword", ItemQuality::Magic, 5));
    const CraftCost hi = ComputeCraftCost(D(), CraftAction::Reforge, Itm("w_short_sword", ItemQuality::Magic, 35));
    CHECK(hi.gold > lo.gold * 3);
    const CraftCost m = ComputeCraftCost(D(), CraftAction::Reforge, Itm("w_short_sword", ItemQuality::Magic, 20));
    const CraftCost r = ComputeCraftCost(D(), CraftAction::Reforge, Itm("w_short_sword", ItemQuality::Rare, 20));
    const CraftCost up = ComputeCraftCost(D(), CraftAction::Upgrade, Itm("w_short_sword", ItemQuality::Magic, 20));
    CHECK(r.gold > m.gold);
    CHECK(up.gold > r.gold);
    auto mat = [](const CraftCost& c, std::string_view id) {
      for (const CraftMaterialCost& x : c.materials) {
        if (x.itemId == id) return x.count;
      }
      return 0;
    };
    CHECK(mat(r, "m_essence") == 1);
    // Applies only to fitting qualities / types.
    CHECK_FALSE(ComputeCraftCost(D(), CraftAction::Reforge, Itm("w_short_sword")).applies);
    CHECK_FALSE(ComputeCraftCost(D(), CraftAction::Reforge, Itm("w_short_sword", ItemQuality::Legendary)).applies);
    CHECK_FALSE(ComputeCraftCost(D(), CraftAction::Upgrade, Itm("w_short_sword", ItemQuality::Rare)).applies);
    CHECK_FALSE(ComputeCraftCost(D(), CraftAction::Socket, Itm("j_copper_ring", ItemQuality::Magic)).applies);
    CHECK_FALSE(ComputeCraftCost(D(), CraftAction::Salvage, Itm("c_hp_potion_s")).applies);
    const CraftCost sal = ComputeCraftCost(D(), CraftAction::Salvage, Itm("j_copper_ring", ItemQuality::Rare));
    CHECK(sal.applies);
    CHECK(sal.gold == 0);
    CHECK(sal.materials.empty());
    CHECK(ComputeCraftCost(D(), CraftAction::Socket, Itm("w_short_sword", ItemQuality::Legendary)).applies);
    CHECK(CraftUpgradeTarget(ItemQuality::Normal) == ItemQuality::Magic);
    CHECK(CraftUpgradeTarget(ItemQuality::Magic) == ItemQuality::Rare);
    CHECK_FALSE(CraftUpgradeTarget(ItemQuality::Rare).has_value());
    // Balance guard: a magic reroll costs fewer than ~15 kills of gold at every zone.
    for (int L : {1, 10, 20, 30, 40}) {
      const CraftCost c = ComputeCraftCost(D(), CraftAction::Reforge, Itm("w_short_sword", ItemQuality::Magic, L));
      CHECK(c.gold / (std::max)(3.0, L * 1.9) < 15);
    }
    // 20.4 at L5.
    struct Row {
      CraftAction a;
      ItemQuality q;
      int64_t gold;
      int scrap, dust, essence;
    };
    const Row rows[] = {{CraftAction::Reforge, ItemQuality::Magic, 60, 2, 1, 0},
                        {CraftAction::Reforge, ItemQuality::Rare, 120, 0, 2, 1},
                        {CraftAction::Upgrade, ItemQuality::Normal, 120, 3, 1, 0},
                        {CraftAction::Upgrade, ItemQuality::Magic, 300, 0, 4, 1},
                        {CraftAction::Socket, ItemQuality::Normal, 180, 4, 0, 1}};
    for (const Row& row : rows) {
      const CraftCost c = ComputeCraftCost(D(), row.a, Itm("w_short_sword", row.q, 5));
      CHECK(c.gold == row.gold);
      CHECK(mat(c, "m_scrap") == row.scrap);
      CHECK(mat(c, "m_dust") == row.dust);
      CHECK(mat(c, "m_essence") == row.essence);
    }
  }

  TEST_CASE("salvage yields (13.2, 20.5)") {
    auto y = [](std::string_view base, ItemQuality q, int L) {
      std::vector<std::pair<std::string, int>> out;
      for (const MaterialYield& m : SalvageYield(D(), Itm(base, q, L))) out.push_back({m.baseId, m.quantity});
      return out;
    };
    using V = std::vector<std::pair<std::string, int>>;
    CHECK(y("w_dagger", ItemQuality::Normal, 1) == V{{"m_scrap", 2}});
    CHECK(y("w_dagger", ItemQuality::Magic, 5) == V{{"m_scrap", 1}, {"m_dust", 1}});
    CHECK(y("w_dagger", ItemQuality::Rare, 25) == V{{"m_scrap", 3}, {"m_dust", 2}, {"m_essence", 1}});
    CHECK(y("w_dagger", ItemQuality::Magic, 12) == V{{"m_scrap", 2}, {"m_dust", 1}});
    CHECK(y("w_dagger", ItemQuality::Legendary, 50) == V{{"m_scrap", 5}, {"m_dust", 4}, {"m_essence", 4}});
    CHECK(y("w_dagger", ItemQuality::Set, 40)[2].second >= 3);
    CHECK(y("w_dagger", ItemQuality::Normal, 0) == V{{"m_scrap", 2}});  // level 0 counts as 1
  }

  TEST_CASE("salvage: destroys the item, stacks materials, returns gems; bag-full and not-in-bag refusals") {
    Rng rng(1);
    ItemUidGenerator uids;
    const LootContext lc{&D(), &rng, &uids};
    Inventory inv(D());
    ItemInstance gear = Itm("w_short_sword", ItemQuality::Rare, 12);
    gear.sockets = {GemOf("g_ruby_1")};
    ItemInstance scrap = Stack("m_scrap", 3);
    inv.MutableBag() = {gear, scrap};
    int64_t gold = 0;
    const CraftResult r = PerformCraft(lc, CraftAction::Salvage, gear.uid, inv, gold);
    CHECK(r.ok);
    CHECK(inv.FindInBag(gear.uid) == nullptr);
    CHECK(inv.FindInBag(scrap.uid)->quantity == 5);  // stacked into the existing pile
    CHECK(inv.CountOf("m_dust") == 1);
    CHECK(inv.CountOf("m_essence") == 1);
    CHECK(inv.CountOf("g_ruby_1") == 1);
    CHECK(r.gemsReturned == std::vector<std::string>{"g_ruby_1"});
    CHECK(gold == 0);
    // Bag full: 100 entries, three new stacks needed.
    Inventory full(D());
    ItemInstance g2 = Itm("w_short_sword", ItemQuality::Rare, 12);
    full.MutableBag().push_back(g2);
    for (int i = 0; i < 99; ++i) full.MutableBag().push_back(Itm("w_dagger"));
    CHECK(CheckCraft(D(), CraftAction::Salvage, g2.uid, full, 0).reason == CraftFail::BagFull);
    full.MutableBag().resize(98);
    CHECK(CheckCraft(D(), CraftAction::Salvage, g2.uid, full, 0).ok);
    // Not in the bag (equipped) / not anywhere.
    Inventory eq(D());
    ItemInstance worn = Itm("w_short_sword", ItemQuality::Magic);
    eq.MutableEquipment()[EnumIndex(EquipSlot::Weapon)] = worn;
    CHECK(CheckCraft(D(), CraftAction::Salvage, worn.uid, eq, 0).reason == CraftFail::NotInBag);
    CHECK(CheckCraft(D(), CraftAction::Salvage, "nope", eq, 0).reason == CraftFail::NotInBag);
  }

  TEST_CASE("reforge / upgrade / socket (13.4)") {
    Rng rng(21);
    ItemUidGenerator uids;
    const LootContext lc{&D(), &rng, &uids};
    // Reforge rerolls affixes, keeps base + quality, spends gold + materials, identifies.
    Inventory inv(D());
    ItemInstance gear = Itm("w_broad_sword", ItemQuality::Rare, 22);
    gear.affixes = {ItemAffix{"old", "old", Stat::Dex, 9}};
    gear.identified = false;
    inv.MutableBag() = {gear, Stack("m_dust", 5), Stack("m_essence", 2)};
    const int64_t cost = ComputeCraftCost(D(), CraftAction::Reforge, gear).gold;
    int64_t gold = cost + 7;
    const CraftResult r = PerformCraft(lc, CraftAction::Reforge, gear.uid, inv, gold);
    CHECK(r.ok);
    CHECK(r.goldSpent == cost);
    CHECK(gold == 7);
    const ItemInstance* g = inv.FindInBag(gear.uid);
    REQUIRE(g != nullptr);
    CHECK(g->baseId == "w_broad_sword");
    CHECK(g->quality == ItemQuality::Rare);
    for (const ItemAffix& a : g->affixes) CHECK(a.affixId != "old");
    CHECK(g->affixes.size() >= 3);
    CHECK(g->affixes.size() <= 4);
    CHECK(g->identified);
    CHECK(inv.CountOf("m_dust") == 3);
    CHECK(inv.CountOf("m_essence") == 1);
    // Emptied material stacks are removed.
    Inventory e(D());
    ItemInstance m = Itm("w_short_sword", ItemQuality::Magic, 3);
    e.MutableBag() = {m, Stack("m_scrap", 2), Stack("m_dust", 1)};
    int64_t rich = 9999;
    CHECK(PerformCraft(lc, CraftAction::Reforge, m.uid, e, rich).ok);
    REQUIRE(e.Bag().size() == 1);
    CHECK(e.Bag()[0].uid == m.uid);
    // Blocks on gold, then materials; a failed perform changes nothing.
    Inventory b(D());
    ItemInstance bm = Itm("w_short_sword", ItemQuality::Magic, 10);
    b.MutableBag() = {bm, Stack("m_scrap", 2)};
    CHECK(CheckCraft(D(), CraftAction::Reforge, bm.uid, b, 0).reason == CraftFail::Gold);
    CHECK(CheckCraft(D(), CraftAction::Reforge, bm.uid, b, 99999).reason == CraftFail::Materials);
    int64_t five = 5;
    const CraftResult fail = PerformCraft(lc, CraftAction::Reforge, bm.uid, b, five);
    CHECK_FALSE(fail.ok);
    CHECK(fail.reason == CraftFail::Gold);
    CHECK(five == 5);
    CHECK(b.CountOf("m_scrap") == 2);
  }

  TEST_CASE("upgrade normal -> magic -> rare keeps existing affixes (20.6 target counts)") {
    Rng rng(22);
    ItemUidGenerator uids;
    const LootContext lc{&D(), &rng, &uids};
    Inventory inv(D());
    ItemInstance gear = Itm("a_chain_mail", ItemQuality::Normal, 15);
    inv.MutableBag() = {gear, Stack("m_scrap", 10), Stack("m_dust", 10), Stack("m_essence", 5)};
    int64_t gold = 100000;
    rng.Script({0.99});  // target = 1 + floor(0.99 * 2) = 2 -> need 2
    CHECK(PerformCraft(lc, CraftAction::Upgrade, gear.uid, inv, gold).ok);
    const ItemInstance* g = inv.FindInBag(gear.uid);
    CHECK(g->quality == ItemQuality::Magic);
    REQUIRE(g->affixes.size() == 2);
    const std::vector<ItemAffix> kept = g->affixes;
    CHECK(KindOf(kept[0]) == AffixKind::Prefix);
    rng.Script({0.99});  // target 4 -> need 2
    CHECK(PerformCraft(lc, CraftAction::Upgrade, gear.uid, inv, gold).ok);
    g = inv.FindInBag(gear.uid);
    CHECK(g->quality == ItemQuality::Rare);
    REQUIRE(g->affixes.size() == 4);
    CHECK(g->affixes[0] == kept[0]);
    CHECK(g->affixes[1] == kept[1]);
    CHECK(KindOf(g->affixes[2]) == AffixKind::Prefix);  // the new ones start with a prefix again
    CHECK(CheckCraft(D(), CraftAction::Upgrade, gear.uid, inv, gold).reason == CraftFail::Quality);
    // 20.6: normal with rng 0 -> need 1; magic with 2 affixes and rng 0 -> need 1.
    Inventory n(D());
    ItemInstance a = Itm("a_chain_mail", ItemQuality::Normal, 15);
    ItemInstance b = Itm("a_chain_mail", ItemQuality::Magic, 15);
    b.affixes = {FixedAffix("pre_sturdy", 2), FixedAffix("suf_life", 9)};
    n.MutableBag() = {a, b, Stack("m_scrap", 10), Stack("m_dust", 10), Stack("m_essence", 5)};
    rng.Script({0.0});
    CHECK(PerformCraft(lc, CraftAction::Upgrade, a.uid, n, gold).ok);
    CHECK(n.FindInBag(a.uid)->affixes.size() == 1);
    rng.Script({0.0});
    CHECK(PerformCraft(lc, CraftAction::Upgrade, b.uid, n, gold).ok);
    CHECK(n.FindInBag(b.uid)->affixes.size() == 3);
    // An upgraded normal item is named after its new affixes.
    Inventory nm(D());
    std::optional<ItemInstance> plain = CreateItem(lc, "a_chain_mail", 12, ItemQuality::Normal);
    nm.MutableBag() = {*plain, Stack("m_scrap", 3), Stack("m_dust", 1)};
    int64_t g2 = 1000000;
    CHECK(PerformCraft(lc, CraftAction::Upgrade, plain->uid, nm, g2).ok);
    CHECK(nm.FindInBag(plain->uid)->quality == ItemQuality::Magic);
    CHECK(nm.FindInBag(plain->uid)->affixes.size() >= 1);
    CHECK(nm.FindInBag(plain->uid)->name != D().Items().FindBase("a_chain_mail")->name);
  }

  TEST_CASE("socket punching: +1 to weapons / armour, capped at 3 total and 1 punched") {
    Rng rng(23);
    ItemUidGenerator uids;
    const LootContext lc{&D(), &rng, &uids};
    Inventory inv(D());
    ItemInstance rusty = Itm("w_rusty_sword", ItemQuality::Normal, 5);
    inv.MutableBag() = {rusty, Stack("m_scrap", 20), Stack("m_essence", 5)};
    int64_t gold = 100000;
    CHECK(ItemSocketCapacity(rusty, D()) == 0);
    CHECK(PerformCraft(lc, CraftAction::Socket, rusty.uid, inv, gold).ok);
    CHECK(ItemSocketCapacity(*inv.FindInBag(rusty.uid), D()) == 1);
    CHECK(CheckCraft(D(), CraftAction::Socket, rusty.uid, inv, gold).reason == CraftFail::MaxSockets);
    ItemInstance demon = Itm("w_demon_blade", ItemQuality::Magic, 35);
    inv.MutableBag().push_back(demon);
    CHECK(CheckCraft(D(), CraftAction::Socket, demon.uid, inv, gold).reason == CraftFail::MaxSockets);
    ItemInstance ring = Itm("j_gold_ring", ItemQuality::Magic, 20);
    inv.MutableBag().push_back(ring);
    const CraftCheck c = CheckCraft(D(), CraftAction::Socket, ring.uid, inv, gold);
    CHECK_FALSE(c.ok);
    CHECK(c.reason == CraftFail::NotEquipment);
  }

  TEST_CASE("unknown bases never crash and are refused") {
    Rng rng(24);
    ItemUidGenerator uids;
    const LootContext lc{&D(), &rng, &uids};
    Inventory inv(D());
    ItemInstance ghost = Itm("x_removed_item", ItemQuality::Rare, 10);
    inv.MutableBag().push_back(ghost);
    const CraftCheck c = CheckCraft(D(), CraftAction::Reforge, ghost.uid, inv, 1000000000);
    CHECK_FALSE(c.ok);
    CHECK(c.reason == CraftFail::UnknownBase);
    int64_t gold = 0;
    CHECK_FALSE(PerformCraft(lc, CraftAction::Salvage, ghost.uid, inv, gold).ok);
    CHECK(ItemSocketCapacity(ghost, D()) == 0);
    CHECK(SalvageYield(D(), ghost)[0].quantity > 0);
  }

  TEST_CASE("crafting with the real loot generator: rare reforge gives 3-4 unique affixes and matching stats") {
    Rng rng(25);
    ItemUidGenerator uids;
    const LootContext lc{&D(), &rng, &uids};
    for (int i = 0; i < 20; ++i) {
      Inventory inv(D());
      std::optional<ItemInstance> gear = CreateItem(lc, "w_claymore", 30, ItemQuality::Rare);
      inv.MutableBag() = {*gear, Stack("m_dust", 2), Stack("m_essence", 1)};
      int64_t gold = 1000000;
      CHECK(PerformCraft(lc, CraftAction::Reforge, gear->uid, inv, gold).ok);
      const ItemInstance* g = inv.FindInBag(gear->uid);
      CHECK(g->affixes.size() >= 3);
      CHECK(g->affixes.size() <= 4);
      std::set<std::string> ids;
      for (const ItemAffix& a : g->affixes) ids.insert(a.affixId);
      CHECK(ids.size() == g->affixes.size());
      CHECK(StatSum(*g) == AffixSum(*g));
    }
  }

  // ===================================================================================================================
  // Item comparison (14, ItemCompare.test.ts + 20.11)
  // ===================================================================================================================
  TEST_CASE("compare: target slot, deltas, rings, unidentified") {
    auto mk = [](std::string_view base, std::string_view uid, std::vector<ItemAffix> affixes) {
      ItemInstance it = Itm(base, affixes.empty() ? ItemQuality::Normal : ItemQuality::Magic, 5);
      it.uid = std::string(uid);
      it.affixes = std::move(affixes);
      return it;
    };
    CHECK(D().Items().FindBase("w_rusty_sword")->slot == EquipSlot::Weapon);
    CHECK(D().Items().FindBase("j_copper_ring")->slot == EquipSlot::Ring1);
    Inventory::EquipmentArray eq{};
    const ItemInstance worn = mk("w_rusty_sword", "a", {StatAffix(Stat::Str, 3)});
    const ItemInstance bag = mk("w_rusty_sword", "b", {StatAffix(Stat::Str, 5), StatAffix(Stat::CritRate, 4)});
    eq[EnumIndex(EquipSlot::Weapon)] = worn;
    std::optional<CompareTarget> t = FindCompareTarget(D(), bag, eq);
    REQUIRE(t.has_value());
    CHECK(t->slot == EquipSlot::Weapon);
    REQUIRE(t->equipped != nullptr);
    CHECK(t->equipped->uid == "a");
    CHECK_FALSE(FindCompareTarget(D(), worn, eq).has_value());
    std::vector<StatDelta> d = StatDeltas(D(), bag, &worn);
    REQUIRE(d.size() == 2);
    CHECK(d[0].key.stat == Stat::CritRate);
    CHECK(d[0].delta == 4);
    CHECK(d[1].key.stat == Stat::Str);
    CHECK(d[1].delta == 2);
    // Losses are negative, unchanged stats skipped.
    const ItemInstance w2 = mk("w_rusty_sword", "a", {StatAffix(Stat::Str, 5), StatAffix(Stat::Dex, 2)});
    const ItemInstance b2 = mk("w_rusty_sword", "b", {StatAffix(Stat::Str, 2), StatAffix(Stat::Dex, 2)});
    d = StatDeltas(D(), b2, &w2);
    REQUIRE(d.size() == 1);
    CHECK(d[0].key.stat == Stat::Str);
    CHECK(d[0].delta == -3);
    // Against an empty slot every stat is a gain, base damage first.
    d = StatDeltas(D(), mk("w_rusty_sword", "b", {StatAffix(Stat::Str, 1)}), nullptr);
    REQUIRE(!d.empty());
    CHECK(d[0].key.kind == CompareKey::Kind::AvgDamage);
    for (const StatDelta& x : d) CHECK(x.delta > 0);
    // Rings: a free ring slot, else the weaker ring.
    Inventory::EquipmentArray rings{};
    const ItemInstance strong = mk("j_copper_ring", "r1", {StatAffix(Stat::Str, 9)});
    const ItemInstance weak = mk("j_copper_ring", "r2", {StatAffix(Stat::Str, 1)});
    rings[EnumIndex(EquipSlot::Ring1)] = strong;
    t = FindCompareTarget(D(), mk("j_copper_ring", "n", {}), rings);
    CHECK(t->slot == EquipSlot::Ring2);
    CHECK(t->equipped == nullptr);
    rings[EnumIndex(EquipSlot::Ring2)] = weak;
    t = FindCompareTarget(D(), mk("j_copper_ring", "n", {}), rings);
    CHECK(t->slot == EquipSlot::Ring2);
    CHECK(t->equipped->uid == "r2");
    // Unidentified affixes are not counted.
    ItemInstance unid = mk("w_rusty_sword", "b", {StatAffix(Stat::Str, 9)});
    unid.identified = false;
    CHECK(StatDeltas(D(), unid, &worn).size() == 1);  // only the worn item's str 3 (as a loss)
    const ItemInstance plain = mk("w_rusty_sword", "a", {});
    CHECK(StatDeltas(D(), unid, &plain).empty());
  }

  TEST_CASE("20.11 compare worked examples") {
    const ItemInstance sword = Itm("w_short_sword");
    const ItemInstance rusty = Itm("w_rusty_sword");
    std::vector<StatDelta> d = StatDeltas(D(), sword, &rusty);
    REQUIRE(d.size() == 1);
    CHECK(d[0].key.kind == CompareKey::Kind::AvgDamage);
    CHECK(d[0].delta == 2.5);
    Inventory::EquipmentArray eq{};
    std::optional<CompareTarget> t = FindCompareTarget(D(), Itm("w_wooden_shield"), eq);
    CHECK(t->slot == EquipSlot::Offhand);
    CHECK(t->equipped == nullptr);
    CHECK(StatDeltas(D(), Itm("w_wooden_shield"), nullptr).empty());  // -> "empty slot" line
    ItemInstance r1 = Itm("j_copper_ring", ItemQuality::Magic, 1);
    r1.affixes = {StatAffix(Stat::Str, 5)};  // score 5.1
    ItemInstance r2 = Itm("j_copper_ring", ItemQuality::Magic, 1);
    r2.affixes = {StatAffix(Stat::Str, 3)};  // score 3.1
    eq[EnumIndex(EquipSlot::Ring1)] = r1;
    eq[EnumIndex(EquipSlot::Ring2)] = r2;
    CHECK(FindCompareTarget(D(), Itm("j_gold_ring"), eq)->slot == EquipSlot::Ring2);
    // Armour base defense and gems.
    ItemInstance cap = Itm("a_iron_helm");
    cap.sockets = {GemOf("g_ruby_1")};
    const std::vector<StatTotal> tot = ItemStatTotals(D(), cap);
    REQUIRE(tot.size() == 2);
    CHECK(tot[0].key.kind == CompareKey::Kind::BaseDefense);
    CHECK(tot[0].value == 10);
    CHECK(tot[1].value == 5);
  }

  // ===================================================================================================================
  // Save JSON (16; I8 field names)
  // ===================================================================================================================
  TEST_CASE("item JSON round trip keeps every field (I8 names)") {
    ItemInstance it = Itm("w_claymore", ItemQuality::Legendary, 33);
    it.uid = "i2a";
    it.affixes = {FixedAffix("pre_keen", 6), ItemAffix{"leg_x", "x", Stat::KillHealPercent, 2.5}};
    it.sockets = {GemOf("g_ruby_2"), GemOf("g_sapphire_1")};
    it.bonusSockets = 1;
    it.setId = "set_iron_guardian";
    it.setPieceId = "set_iron_shield";
    it.legendaryEffect = "fx";
    it.legendaryId = "leg_soulreaver";
    it.quantity = 1;
    ComputeItemStats(it);
    JsonWriter w;
    WriteItemJson(w, it);
    const std::string text = w.Take();
    CHECK(text.find("\"baseId\":\"w_claymore\"") != std::string::npos);
    CHECK(text.find("\"gemId\":\"g_ruby_2\"") != std::string::npos);
    CHECK(text.find("\"bonusSockets\":1") != std::string::npos);
    JsonValue v;
    REQUIRE(ParseJson(text, v));
    ItemInstance back;
    REQUIRE(ReadItemJson(v, back));
    CHECK(back == it);
    // Lenient reads: missing sockets -> [], unknown stats skipped, wrong types default, non-objects skipped.
    JsonValue partial;
    REQUIRE(ParseJson(R"({"uid":"u","baseId":"w_dagger","quality":"bogus","quantity":"x",
                          "affixes":[{"affixId":"a","stat":"nope","value":1},{"affixId":"b","stat":"str","value":2}],
                          "stats":{"nope":1,"str":2}})",
                      partial));
    ItemInstance p;
    REQUIRE(ReadItemJson(partial, p));
    CHECK(p.sockets.empty());
    CHECK(p.quality == ItemQuality::Normal);
    CHECK(p.quantity == 1);
    REQUIRE(p.affixes.size() == 1);
    CHECK(p.affixes[0].stat == Stat::Str);
    CHECK(p.stats.Size() == 1);
    CHECK(p.identified);
    CHECK_FALSE(ReadItemJson(JsonValue::Array(), p));
  }

  TEST_CASE("socketed gems survive the save round trip (GemSocketing.test.ts persistence)") {
    ItemInstance sword = Itm("w_claymore");
    sword.sockets = {GemOf("g_ruby_2"), GemOf("g_sapphire_1")};
    ComputeItemStats(sword);
    // Equipment is saved as {slot: ItemInstance} (savejson::WriteEquipment, private to the save module): the item
    // objects themselves round-trip through the public helpers.
    JsonWriter w;
    w.StartObject();
    w.Key("weapon");
    WriteItemJson(w, sword);
    w.EndObject();
    JsonValue v;
    REQUIRE(ParseJson(w.Take(), v));
    ItemInstance loaded;
    REQUIRE(ReadItemJson(v.Get("weapon"), loaded));
    REQUIRE(loaded.sockets.size() == 2);
    CHECK(loaded.sockets[0].gemId == "g_ruby_2");
    CHECK(loaded.sockets[0].value == 12);
    CHECK(loaded.sockets[0].tier == 2);
    CHECK(loaded.sockets[1].stat == Stat::Int);
    CHECK(loaded.stats.Get(Stat::Str) == 12);
    Inventory inv(D());
    inv.MutableEquipment()[EnumIndex(EquipSlot::Weapon)] = loaded;
    CHECK(inv.EquipmentStatBag().Get(Stat::Str) == 12);
    CHECK(inv.EquipmentStatBag().Get(Stat::Int) == 5);
    // Empty sockets survive as [].
    JsonWriter e;
    WriteItemJson(e, Itm("w_short_sword"));
    const std::string text = e.Take();
    CHECK(text.find("\"sockets\":[]") != std::string::npos);
  }

  // ===================================================================================================================
  // Runtime: InventorySystem
  // ===================================================================================================================
  TEST_CASE("stash session (I7): transfers need the stash keeper's session; grants follow the overflow policy") {
    ItmWorld w;
    InventorySystem& inv = w.inv;
    CHECK_FALSE(inv.Stash().open);
    CHECK(inv.StashPut("i1") == InvResult::StashClosed);
    CHECK(inv.StashTake("i1") == InvResult::StashClosed);
    inv.OpenStash("stash");
    CHECK(inv.Stash().open);
    CHECK(inv.Stash().npcId == "stash");
    CHECK(inv.StashCapacity() == kBaseStashSlots);
    ItemInstance d = Itm("w_dagger");
    inv.Items().MutableBag().push_back(d);
    CHECK(inv.StashPut(d.uid) == InvResult::Ok);
    CHECK(test::CountEvents<EvStashChanged>(w.h.events) == 1);
    CHECK(inv.StashTake(d.uid) == InvResult::Ok);
    CHECK(inv.Items().FindInBag(d.uid) != nullptr);
    inv.CloseStash();
    CHECK_FALSE(inv.Stash().open);

    // Grants: bag first (logs obtained), then the policy.
    w.h.events.Clear();
    ItemInstance item = Itm("w_short_sword");
    item.uid = "i5";
    CHECK(inv.Grant(item, OverflowPolicy::Refuse, ItemSource::Shop) == ItemGrantOutcome::Bag);
    CHECK(item.uid.empty());  // moved
    CHECK(HasLogKey(w.h.events, "sys.inventory.obtained"));
    w.FillBag(98);
    ItemInstance more = Itm("w_short_sword");
    more.uid = "i6";
    CHECK(inv.Grant(more, OverflowPolicy::Refuse, ItemSource::Shop) == ItemGrantOutcome::Refused);
    CHECK(more.uid == "i6");
    CHECK(inv.Grant(more, OverflowPolicy::Stash, ItemSource::QuestReward) == ItemGrantOutcome::Stash);
    REQUIRE(inv.Items().Stash().size() == 1);
    CHECK(inv.Items().Stash()[0].uid == "i6");
    w.h.events.Clear();
    ItemInstance lost = Itm("w_dagger");
    CHECK(inv.Grant(lost, OverflowPolicy::Lose, ItemSource::RandomEvent) == ItemGrantOutcome::Lost);
    CHECK(HasLogKey(w.h.events, "sys.inventory.bagFull"));
    // Stack merges log obtainedQty.
    ItemInstance pot = Stack("c_hp_potion_s", 1);
    w.inv.Items().MutableBag().back() = Stack("c_hp_potion_s", 3);
    w.h.events.Clear();
    CHECK(inv.Grant(pot, OverflowPolicy::Lose, ItemSource::QuestReward) == ItemGrantOutcome::Bag);
    CHECK(HasLogKey(w.h.events, "sys.inventory.obtainedQty"));
    CHECK(inv.Items().CountOf("c_hp_potion_s") == 4);
  }

  TEST_CASE("pickup grants publish ItemPickedMsg and EvItemPicked") {
    ItmWorld w;
    int picked = 0;
    w.h.bus.Subscribe<ItemPickedMsg>([&picked](const ItemPickedMsg& m) {
      ++picked;
      CHECK(m.quality == ItemQuality::Legendary);
    });
    ItemInstance it = Itm("w_broad_sword", ItemQuality::Legendary);
    CHECK(w.inv.Grant(it, OverflowPolicy::Refuse, ItemSource::Pickup) == ItemGrantOutcome::Bag);
    CHECK(picked == 1);
    CHECK(test::CountEvents<EvItemPicked>(w.h.events) == 1);
  }

  TEST_CASE("InventorySystem equip / unequip: events, EquipStatsDirtyMsg, logs, Dying gate (C12)") {
    ItmWorld w;
    int dirty = 0;
    w.h.bus.Subscribe<EquipStatsDirtyMsg>([&dirty](const EquipStatsDirtyMsg&) { ++dirty; });
    ItemInstance sword = Itm("w_rusty_sword", ItemQuality::Normal, 1);
    w.inv.Items().MutableBag().push_back(sword);
    CHECK(w.inv.Equip(sword.uid) == InvResult::Ok);
    CHECK(dirty == 1);
    const EvEquipmentChanged* ec = LastEvent<EvEquipmentChanged>(w.h.events);
    REQUIRE(ec != nullptr);
    CHECK(ec->slot == EquipSlot::Weapon);
    CHECK(HasLogKey(w.h.events, "sys.inventory.equipped"));
    // I3 at hero level 1.
    ItemInstance broad = Itm("w_broad_sword");
    w.inv.Items().MutableBag().push_back(broad);
    CHECK(w.inv.Equip(broad.uid) == InvResult::LevelTooLow);
    CHECK(dirty == 1);
    CHECK(w.inv.Unequip(EquipSlot::Weapon) == InvResult::Ok);
    CHECK(dirty == 2);
    // C12: Dying blocks item actions.
    w.hero.SetLife(HeroLife::Dying);
    CHECK(w.inv.Equip(sword.uid) == InvResult::Dying);
    CHECK(w.inv.Discard(sword.uid) == InvResult::Dying);
    CHECK(w.inv.UseItem(sword.uid) == InvResult::Dying);
    CHECK(w.inv.DestroyNormals() == 0);
    w.hero.SetLife(HeroLife::Alive);
    // Discard / destroy normals.
    w.h.events.Clear();
    CHECK(w.inv.Discard(sword.uid) == InvResult::Ok);
    CHECK(HasLogKey(w.h.events, "sys.inventory.discarded"));
    CHECK(w.inv.DestroyNormals() == 1);  // the broad sword
    CHECK(HasLogKey(w.h.events, "sys.inventory.bulkDestroy"));
    CHECK(w.inv.Discard("nope") == InvResult::UnknownItem);
  }

  TEST_CASE("consumables (7.4 + I4): potions restore the hero, antidote cleanses poison, removed / inert items") {
    ItmWorld w;
    InventorySystem& inv = w.inv;
    const double maxHp = w.hero.MaxHp();
    w.hero.SetHp(10);
    ItemInstance hp = Stack("c_hp_potion_s", 2);
    inv.Items().MutableBag().push_back(hp);
    CHECK(inv.UseItem(hp.uid) == InvResult::Ok);
    CHECK(w.hero.Hp() == (std::min)(maxHp, 60.0));
    CHECK(inv.Items().FindInBag(hp.uid)->quantity == 1);
    CHECK(inv.UseItem(hp.uid) == InvResult::Ok);
    CHECK(inv.Items().FindInBag(hp.uid) == nullptr);  // removed at 0
    w.hero.SetMana(0);
    ItemInstance mp = Stack("c_mp_potion_s", 1);
    inv.Items().MutableBag().push_back(mp);
    CHECK(inv.UseItem(mp.uid) == InvResult::Ok);
    CHECK(w.hero.Mana() == (std::min)(w.hero.MaxMana(), 30.0));
    // Antidote.
    w.status.Apply(kHeroEntityId, StatusType::Poison, 5, 5000, 77, 0);
    REQUIRE(w.status.Has(kHeroEntityId, StatusType::Poison));
    ItemInstance anti = Stack("c_antidote", 1);
    inv.Items().MutableBag().push_back(anti);
    w.h.events.Clear();
    CHECK(inv.UseItem(anti.uid) == InvResult::Ok);
    CHECK_FALSE(w.status.Has(kHeroEntityId, StatusType::Poison));
    const EvStatusExpired* ex = LastEvent<EvStatusExpired>(w.h.events);
    REQUIRE(ex != nullptr);
    CHECK(ex->type == StatusType::Poison);
    // TP scroll removed (I4), ley fruit / ID scroll inert: not consumed.
    for (const char* id : {"c_tp_scroll", "c_ley_fruit", "c_id_scroll"}) {
      ItemInstance x = Stack(id, 1);
      inv.Items().MutableBag().push_back(x);
      CHECK(inv.UseItem(x.uid) == InvResult::NotUsable);
      CHECK(inv.Items().FindInBag(x.uid) != nullptr);
    }
    ItemInstance gear = Itm("w_dagger");
    inv.Items().MutableBag().push_back(gear);
    CHECK(inv.UseItem(gear.uid) == InvResult::NotUsable);
    CHECK(inv.UseItem("nope") == InvResult::UnknownItem);
    // Dying: refused, quantity unchanged (save-ui-input 15 #19).
    ItemInstance last = Stack("c_hp_potion_s", 1);
    inv.Items().MutableBag().push_back(last);
    w.hero.SetLife(HeroLife::Dying);
    CHECK(inv.UseItem(last.uid) == InvResult::Dying);
    CHECK(inv.Items().FindInBag(last.uid)->quantity == 1);
  }

  TEST_CASE("potion quick slots (I4): bound base, else the strongest potion of the kind in the bag") {
    ItmWorld w;
    InventorySystem& inv = w.inv;
    CHECK(inv.ResolvePotionSlot(PotionSlot::Hp).empty());
    CHECK(inv.UsePotionSlot(PotionSlot::Hp) == InvResult::NotUsable);
    ItemInstance s = Stack("c_hp_potion_s", 2);
    s.uid = "i1";
    ItemInstance m = Stack("c_hp_potion_m", 1);
    m.uid = "i2";
    inv.Items().MutableBag() = {s, m};
    CHECK(inv.ResolvePotionSlot(PotionSlot::Hp) == "c_hp_potion_m");
    CHECK(inv.ResolvePotionSlot(PotionSlot::Mp).empty());
    w.hero.SetHp(1);
    CHECK(inv.UsePotionSlot(PotionSlot::Hp) == InvResult::Ok);
    CHECK(inv.Items().FindInBag("i2") == nullptr);
    CHECK(w.hero.Hp() == (std::min)(w.hero.MaxHp(), 151.0));
    inv.SetPotionSlot(PotionSlot::Hp, "c_hp_potion_s");
    const PotionSlotView v = inv.PotionSlotState(PotionSlot::Hp);
    CHECK(v.bound);
    CHECK(v.baseId == "c_hp_potion_s");
    CHECK(v.count == 2);
    inv.SetPotionSlot(PotionSlot::Hp, "c_hp_potion_l");  // bound but none in the bag: dimmed, nothing used
    CHECK(inv.PotionSlotState(PotionSlot::Hp).count == 0);
    CHECK(inv.UsePotionSlot(PotionSlot::Hp) == InvResult::NotUsable);
    CHECK(inv.Items().CountOf("c_hp_potion_s") == 2);
  }

  TEST_CASE("InventorySystem sockets, sort, snapshot") {
    ItmWorld w;
    w.inv.Items().MutableEquipment()[EnumIndex(EquipSlot::Weapon)] = Itm("w_short_sword");
    ItemInstance g = Stack("g_ruby_1", 1);
    w.inv.Items().MutableBag().push_back(g);
    CHECK(w.inv.SocketGem(EquipSlot::Weapon, g.uid) == InvResult::Ok);
    CHECK(HasLogKey(w.h.events, "sys.inventory.gem.socketed"));
    ItemInstance g2 = Stack("g_ruby_1", 1);
    w.inv.Items().MutableBag().push_back(g2);
    CHECK(w.inv.SocketGem(EquipSlot::Weapon, g2.uid) == InvResult::NoSockets);
    CHECK(HasLogKey(w.h.events, "sys.inventory.gem.noSlots"));
    CHECK(w.inv.UnsocketGem(EquipSlot::Weapon, 0) == InvResult::Ok);
    CHECK(HasLogKey(w.h.events, "sys.inventory.gem.removed"));
    CHECK(w.inv.Items().CountOf("g_ruby_1") == 2);
    w.inv.SortBag();
    Snapshot snap;
    w.inv.FillSnapshot(snap);
    CHECK(snap.inventory == &w.inv.Items());
    CHECK(snap.stashCapacity == 80);
  }

  TEST_CASE("InventorySystem save: write / read with load fix-ups") {
    ItmWorld w;
    ItemInstance a = Itm("w_dagger", ItemQuality::Magic);
    a.uid = "i10";
    a.identified = false;
    a.affixes = {FixedAffix("pre_sharp", 2)};
    ItemInstance dup = Itm("c_hp_potion_s");
    dup.uid = "i10";  // duplicate uid -> re-issued
    dup.quantity = 0;
    ItemInstance leg = Itm("w_broad_sword", ItemQuality::Legendary, 8);
    leg.uid = "";
    leg.name = D().Items().FindLegendary("leg_grief")->name;  // a save without legendaryId
    ItemInstance setp = Itm("a_leather_helm", ItemQuality::Set, 5);
    setp.setId = "set_hunter";
    SaveData s;
    s.inventory = {a, dup, leg};
    s.equipment[EnumIndex(EquipSlot::Helmet)] = setp;
    s.stash = {Itm("w_rusty_sword")};
    s.itemUidCounter = 3;
    w.inv.ReadSave(s);
    const Inventory& inv = w.inv.Items();
    REQUIRE(inv.Bag().size() == 3);
    CHECK(inv.Bag()[0].identified);
    CHECK(inv.Bag()[0].stats.Get(Stat::Damage) == 2);  // recomputed
    CHECK(inv.Bag()[1].uid != "i10");
    CHECK(inv.Bag()[1].quantity == 1);
    CHECK_FALSE(inv.Bag()[2].uid.empty());
    CHECK(inv.Bag()[2].legendaryId == "leg_grief");
    CHECK(inv.Equipped(EquipSlot::Helmet)->setPieceId == "set_hunter_helm");
    CHECK(inv.Stash().size() == 1);
    CHECK(w.inv.Uids().Counter() > 0x10);  // bumped past i10
    SaveData out;
    w.inv.WriteSave(out);
    CHECK(out.inventory.size() == 3);
    CHECK(out.equipment[EnumIndex(EquipSlot::Helmet)].has_value());
    CHECK(out.stash.size() == 1);
    CHECK(out.itemUidCounter == w.inv.Uids().Counter());
  }

  // ===================================================================================================================
  // Runtime: shops and the forge (12-13)
  // ===================================================================================================================
  TEST_CASE("shop: stock, buy (FIX Q6), sell (I9), buyback, closed shop") {
    ItmWorld w;
    w.shop.Open("merchant", false);
    REQUIRE(w.shop.State().open);
    CHECK(test::CountEvents<EvShopOpened>(w.h.events) == 1);
    const std::vector<ShopWare>& wares = w.shop.State().wares;
    REQUIRE(wares.size() == 9);
    for (const ShopWare& ware : wares) {
      CHECK_FALSE(D().Items().IsRemovedItem(ware.baseId));
      CHECK(ware.price == D().Items().FindBase(ware.baseId)->sellPrice * 3);
    }
    CHECK(wares[0].baseId == "c_hp_potion_s");
    CHECK(wares[0].price == 15);
    w.hero.SetGold(100);
    CHECK(w.shop.Buy(0) == InvResult::Ok);
    CHECK(w.hero.Gold() == 85);
    const EvGoldChanged* gc = LastEvent<EvGoldChanged>(w.h.events);
    REQUIRE(gc != nullptr);
    CHECK(gc->reason == GoldReason::ShopBuy);
    CHECK(gc->delta == -15);
    REQUIRE(w.inv.Items().Bag().size() == 1);
    CHECK(w.inv.Items().Bag()[0].level == w.hero.Level());
    CHECK(w.inv.Items().Bag()[0].quality == ItemQuality::Normal);
    CHECK(w.shop.Buy(0) == InvResult::Ok);
    CHECK(w.inv.Items().CountOf("c_hp_potion_s") == 2);
    CHECK(w.inv.Items().Bag().size() == 1);
    CHECK(w.shop.Buy(42) == InvResult::InvalidIndex);
    w.hero.SetGold(5);
    CHECK(w.shop.Buy(0) == InvResult::NotEnoughGold);
    // FIX Q6: a full bag refuses before paying (a partial stack still takes it).
    w.hero.SetGold(1000);
    w.FillBag(99);
    CHECK(w.shop.Buy(0) == InvResult::Ok);
    CHECK(w.shop.Buy(5) == InvResult::BagFull);  // g_ruby_1: needs a new entry
    CHECK(w.hero.Gold() == 1000 - 15);
    // Sell: I9 price, buyback x5.
    const ItemInstance& first = w.inv.Items().Bag()[1];
    const std::string sold = first.uid;
    CHECK(w.shop.Sell(sold) == InvResult::Ok);
    CHECK(w.hero.Gold() == 1000 - 15 + 5);
    CHECK(LastEvent<EvGoldChanged>(w.h.events)->reason == GoldReason::ShopSell);
    REQUIRE(w.inv.Items().BuybackList().size() == 1);
    CHECK(w.inv.Items().BuybackList()[0].price == 25);
    CHECK(w.shop.Sell("nope") == InvResult::UnknownItem);
    CHECK(w.shop.Buyback(0) == InvResult::Ok);
    CHECK(w.inv.Items().FindInBag(sold) != nullptr);
    CHECK(w.hero.Gold() == 1000 - 15 + 5 - 25);
    CHECK(HasLogKey(w.h.events, "ui.shop.buybackLog"));
    CHECK(w.shop.Buyback(0) == InvResult::InvalidIndex);
    // Dying (C12) and closed.
    w.hero.SetLife(HeroLife::Dying);
    CHECK(w.shop.Buy(0) == InvResult::Dying);
    w.hero.SetLife(HeroLife::Alive);
    w.shop.Close();
    CHECK(test::CountEvents<EvShopClosed>(w.h.events) == 1);
    CHECK(w.shop.Buy(0) == InvResult::ShopClosed);
    CHECK(w.shop.Sell(sold) == InvResult::ShopClosed);
    // Blacksmith stock; wandering merchant maps the web's ids (FIX Q12), skips removed bases, applies the multiplier.
    w.shop.Open("blacksmith", true);
    CHECK(w.shop.State().wares.size() == 12);
    CHECK(w.shop.State().blacksmith);
    w.shop.OpenWanderingMerchant({"iron_sword", "c_hp_potion_s", "c_tp_scroll", "no_such_item"}, 1.2);
    REQUIRE(w.shop.State().wares.size() == 2);
    CHECK(w.shop.State().wares[0].baseId == "w_short_sword");  // iron_sword (emerald_plains idMap)
    CHECK(w.shop.State().wares[0].price == 54);                // round(15 x 3 x 1.2)
    CHECK(w.shop.State().wares[1].price == 18);                // round(5 x 3 x 1.2)
    CHECK(w.shop.State().npcId == kWanderingMerchantShopId);
  }

  TEST_CASE("forge through the shop: blacksmith only, gold via RewardService, SFX and events") {
    ItmWorld w;
    ItemInstance m = Itm("w_short_sword", ItemQuality::Magic, 1);
    w.inv.Items().MutableBag() = {m, Stack("m_scrap", 2), Stack("m_dust", 1)};
    w.hero.SetGold(100);
    w.shop.Open("merchant", false);
    CHECK_FALSE(w.shop.Craft(CraftAction::Reforge, m.uid).ok);  // merchants have no forge
    CHECK(test::CountEvents<EvCraftPerformed>(w.h.events) == 0);
    w.shop.Open("blacksmith", true);
    const CraftResult r = w.shop.Craft(CraftAction::Reforge, m.uid);
    CHECK(r.ok);
    CHECK(w.hero.Gold() == 100 - 36);
    CHECK(LastEvent<EvGoldChanged>(w.h.events)->reason == GoldReason::Craft);
    const EvCraftPerformed* cp = LastEvent<EvCraftPerformed>(w.h.events);
    REQUIRE(cp != nullptr);
    CHECK(cp->ok);
    CHECK(cp->action == CraftAction::Reforge);
    CHECK(LastEvent<EvSfx>(w.h.events)->cue == SfxId::Anvil);
    CHECK(HasLogKey(w.h.events, "ui.forge.log.reforge"));
    // Refusal: error SFX + reason.
    const CraftResult f = w.shop.Craft(CraftAction::Reforge, m.uid);  // no materials left
    CHECK_FALSE(f.ok);
    CHECK(f.reason == CraftFail::Materials);
    CHECK(LastEvent<EvSfx>(w.h.events)->cue == SfxId::Error);
    CHECK(LastEvent<EvCraftPerformed>(w.h.events)->reason.key == "ui.forge.block.materials");
    // Salvage log lists the materials.
    CHECK(w.shop.Craft(CraftAction::Salvage, m.uid).ok);
    CHECK(HasLogKey(w.h.events, "ui.forge.log.salvage"));
    CHECK(w.inv.Items().CountOf("m_scrap") == 1);
    CHECK(w.inv.Items().CountOf("m_dust") == 1);
  }

  // ===================================================================================================================
  // Runtime: ground loot (6)
  // ===================================================================================================================
  TEST_CASE("ground items: drop event, 60 s despawn timer, potions 30 s") {
    ItmWorld w;
    const EntityId id = w.ground.Drop(Itm("w_dagger", ItemQuality::Legendary), Vec2(10, 10));
    REQUIRE(id != kNoEntity);
    const EvLootDropped* ld = LastEvent<EvLootDropped>(w.h.events);
    REQUIRE(ld != nullptr);
    CHECK(ld->drop == id);
    CHECK(ld->quality == ItemQuality::Legendary);
    CHECK(ld->expiresAtMs == doctest::Approx(60000));
    const GroundItem* g = w.ground.Find(id);
    REQUIRE(g != nullptr);
    CHECK(g->visualOffset.x >= 0);
    CHECK(g->visualOffset.x < 0.5);
    const EntityId cache = w.ground.Drop(Itm("w_dagger"), Vec2(11, 10), false);
    CHECK(LastEvent<EvLootDropped>(w.h.events)->expiresAtMs == 0);
    const EntityId pot = w.ground.DropPotion(PotionKind::Hp, 50, Vec2(30, 30));
    CHECK(LastEvent<EvPotionDropped>(w.h.events)->drop == pot);
    w.h.Step(static_cast<int>(30000 / kSimStepMs) + 2);
    CHECK(w.ground.Potions().empty());
    CHECK(w.ground.Find(id) != nullptr);
    w.h.Step(static_cast<int>(30000 / kSimStepMs) + 2);
    CHECK(w.ground.Find(id) == nullptr);
    CHECK(w.ground.Find(cache) != nullptr);  // treasure-cache drops never despawn
    const EvEntityDespawned* de = LastEvent<EvEntityDespawned>(w.h.events);
    REQUIRE(de != nullptr);
    CHECK(de->reason == DespawnReason::Expired);
    w.ground.OnZoneExit();
    CHECK(w.ground.Items().empty());
  }

  TEST_CASE("potion auto-collect within 2 tiles (6.2), even at full HP; paused while Dying") {
    ItmWorld w;
    w.hero.SetPosition(Vec2(10, 10));
    w.hero.SetHp(1);
    w.ground.DropPotion(PotionKind::Hp, 50, Vec2(12, 10));   // distSq 4: in range
    w.ground.DropPotion(PotionKind::Mp, 30, Vec2(12.01, 10));  // just out
    w.ground.Tick();
    REQUIRE(w.ground.Potions().size() == 1);
    CHECK(w.hero.Hp() == (std::min)(w.hero.MaxHp(), 51.0));
    CHECK(HasLogKey(w.h.events, "zone.combat.restoreHp"));
    CHECK(test::CountEvents<EvPotionPicked>(w.h.events) == 1);
    w.hero.SetLife(HeroLife::Dying);
    w.hero.SetPosition(Vec2(12, 10));
    w.ground.Tick();
    CHECK(w.ground.Potions().size() == 1);
    w.hero.SetLife(HeroLife::Alive);
    w.ground.Tick();
    CHECK(w.ground.Potions().empty());
    CHECK(HasLogKey(w.h.events, "zone.combat.restoreMana"));
  }

  TEST_CASE("click pickup (6.3 + FIX Q22) and findLootAt hit box") {
    ItmWorld w;
    w.hero.SetPosition(Vec2(10, 10));
    const EntityId near = w.ground.Drop(Itm("w_dagger"), Vec2(11, 11));
    const EntityId far = w.ground.Drop(Itm("w_dagger"), Vec2(20, 20));
    CHECK(w.ground.LootAt(Vec2(11.4, 12.4)) == near);
    CHECK(w.ground.LootAt(Vec2(12.5, 11)) == kNoEntity);  // |dcol| = 1.5 is outside
    CHECK(w.ground.InPickupRange(near));
    CHECK_FALSE(w.ground.InPickupRange(far));
    CHECK_FALSE(w.ground.TryPickUp(far));  // the caller walks first
    CHECK(w.ground.Find(far) != nullptr);
    CHECK(w.ground.TryPickUp(near));
    CHECK(w.ground.Find(near) == nullptr);
    CHECK(w.inv.Items().Bag().size() == 1);
    CHECK(test::CountEvents<EvItemPicked>(w.h.events) == 1);
    CHECK(LastEvent<EvEntityDespawned>(w.h.events)->reason == DespawnReason::PickedUp);
    // Arrival: in range now.
    w.hero.SetPosition(Vec2(19, 19));
    CHECK(w.ground.TryPickUp(far));
    // Full bag: stays on the ground, bag-full log.
    w.FillBag(98);
    const EntityId third = w.ground.Drop(Itm("w_dagger"), Vec2(19, 19));
    CHECK_FALSE(w.ground.TryPickUp(third));
    CHECK(w.ground.Find(third) != nullptr);
    CHECK(HasLogKey(w.h.events, "sys.inventory.bagFull"));
    // Dying: no pickup.
    w.inv.Items().MutableBag().pop_back();
    w.hero.SetLife(HeroLife::Dying);
    CHECK_FALSE(w.ground.TryPickUp(third));
  }

  TEST_CASE("auto-loot (6.4): every 300 ms, mode ranks, last-to-first, stops on a full bag") {
    ItmWorld w;
    w.hero.SetPosition(Vec2(10, 10));
    const EntityId normal = w.ground.Drop(Itm("w_dagger"), Vec2(10, 11));
    const EntityId magic = w.ground.Drop(Itm("w_dagger", ItemQuality::Magic), Vec2(10, 11));
    const EntityId set = w.ground.Drop(Itm("w_dagger", ItemQuality::Set), Vec2(11, 10));
    const EntityId away = w.ground.Drop(Itm("w_dagger", ItemQuality::Set), Vec2(20, 10));
    w.hero.autoLoot = AutoLootMode::Off;
    w.h.Step(30);
    w.ground.Tick();
    CHECK(w.ground.Items().size() == 4);
    w.hero.autoLoot = AutoLootMode::Legendary;  // set counts as legendary rank
    w.ground.Tick();
    CHECK(w.ground.Find(set) == nullptr);
    CHECK(w.ground.Find(away) != nullptr);
    CHECK(w.ground.Find(magic) != nullptr);
    w.hero.autoLoot = AutoLootMode::Magic;
    w.ground.Tick();  // within 300 ms of the last scan: nothing
    CHECK(w.ground.Find(magic) != nullptr);
    w.h.Step(19);
    w.ground.Tick();
    CHECK(w.ground.Find(magic) == nullptr);
    CHECK(w.ground.Find(normal) != nullptr);
    w.hero.autoLoot = AutoLootMode::All;
    w.FillBag(98);
    w.h.Step(19);
    w.h.events.Clear();
    w.ground.Tick();
    CHECK(w.ground.Find(normal) != nullptr);  // full bag: stop
    CHECK(HasLogKey(w.h.events, "sys.inventory.bagFull"));
    w.h.Step(19);
    w.h.events.Clear();
    w.ground.Tick();
    CHECK_FALSE(HasLogKey(w.h.events, "sys.inventory.bagFull"));  // edge-triggered (not every 300 ms)
  }

  TEST_CASE("kill pipeline (5.1): loot at the monster tile, potions as pickups, deterministic per seed") {
    auto run = [](uint64_t seed) {
      ItmWorld w;
      w.h.rng.SeedAll(seed);
      MonsterKilledMsg m;
      m.defId = "goblin_chief";
      m.level = 5;
      m.elite = true;
      m.affixLootBonus = 5;
      m.pos = Vec2(40.5, 41.25);
      std::vector<std::string> drops;
      int potions = 0;
      for (int i = 0; i < 30; ++i) {
        w.ground.OnMonsterKilled(m);
        for (const GroundItem& g : w.ground.Items()) {
          CHECK(g.pos == m.pos);
          drops.push_back(g.item.baseId + "/" + std::string(EnumName(g.item.quality)));
          PotionKind k = PotionKind::Hp;
          int32_t a = 0;
          CHECK_FALSE(IsGroundPotion(D(), g.item.baseId, k, a));
        }
        potions += static_cast<int>(w.ground.Potions().size());
        w.ground.OnZoneExit();
      }
      CHECK(!drops.empty());
      return std::make_pair(drops, potions);
    };
    CHECK(run(5) == run(5));
    CHECK(run(5) != run(6));
  }

  TEST_CASE("ley fruit drop rate: elite 12 %, other 1.5 % (seeded)") {
    ItmWorld w;
    w.h.rng.SeedAll(2468);
    MonsterKilledMsg m;
    m.level = 3;
    const int n = 20000;
    int normal = 0, elite = 0;
    for (int i = 0; i < 2 * n; ++i) {
      m.elite = i >= n;
      w.ground.OnMonsterKilled(m);
      for (const GroundItem& g : w.ground.Items()) {
        if (g.item.baseId == "c_ley_fruit" && g.item.level == w.hero.Level()) (m.elite ? elite : normal) += 1;
      }
      w.ground.OnZoneExit();
      w.h.events.Clear();
      w.h.timers.Clear();
    }
    // Ley fruit can also come out of the consumable roll (level 1 = hero level 1 here): subtract its expectation.
    using doctest::Approx;
    CHECK(normal / double(n) == Approx(0.015 + 0.30 * 0.25).epsilon(0.08));
    CHECK(elite / double(n) == Approx(0.12 + 0.30 * 0.25).epsilon(0.06));
  }

  TEST_CASE("loot luck (5.1 + I1): raw lck + gear lck + gear magicFind") {
    ItmWorld w;
    const double base = w.hero.BaseStats().lck;
    CHECK(w.ground.LootLuck() == base);
    ItemInstance sword = Itm("w_claymore");
    sword.sockets = {GemOf("g_topaz_2"), GemOf("g_diamond_1")};
    ComputeItemStats(sword);
    w.inv.Items().MutableEquipment()[EnumIndex(EquipSlot::Weapon)] = sword;
    CHECK(w.ground.LootLuck() == base + 10 + 3);
  }

  // ===================================================================================================================
  // Save sections owned by items (save-ui-input 3.2; SaveSections.h): equipment, item lists, potionSlots, uid counter
  // ===================================================================================================================
#if ABYSS_ITEMS_TEST_SAVE_SECTIONS
  TEST_CASE("save sections: equipment map, item lists, potion slots, uid counter (lenient readers)") {
    SaveData s;
    ItemInstance sword = Itm("w_short_sword", ItemQuality::Magic, 4);
    sword.affixes = {FixedAffix("pre_sharp", 3)};
    sword.sockets = {GemOf("g_ruby_1")};
    ComputeItemStats(sword);
    ItemInstance ring = Itm("j_copper_ring", ItemQuality::Rare, 6);
    ring.affixes = {FixedAffix("suf_life", 12)};
    ComputeItemStats(ring);
    s.equipment[EnumIndex(EquipSlot::Ring2)] = ring;
    s.equipment[EnumIndex(EquipSlot::Weapon)] = sword;
    JsonWriter w;
    savejson::WriteEquipment(w, s);
    const std::string text = w.Take();
    // Partial<Record<EquipSlot, ItemInstance>>: only worn slots, in EquipSlot order.
    CHECK(text.find("\"weapon\":{") != std::string::npos);
    CHECK(text.find("\"ring2\":{") != std::string::npos);
    CHECK(text.find("\"helmet\"") == std::string::npos);
    CHECK(text.find("\"weapon\"") < text.find("\"ring2\""));
    JsonValue v;
    REQUIRE(ParseJson(text, v));
    SaveData back;
    back.equipment[EnumIndex(EquipSlot::Helmet)] = Itm("a_leather_helm");  // stale content is cleared
    savejson::ReadEquipment(v, back);
    CHECK(back.equipment == s.equipment);
    // Unknown slot keys and non-object items are skipped; a non-object section reads as nothing worn.
    JsonValue odd;
    REQUIRE(ParseJson(R"({"tail":{"uid":"x","baseId":"w_dagger"},"boots":7,"belt":{"uid":"b","baseId":"a_leather_belt"}})",
                      odd));
    savejson::ReadEquipment(odd, back);
    CHECK_FALSE(back.equipment[EnumIndex(EquipSlot::Boots)].has_value());
    REQUIRE(back.equipment[EnumIndex(EquipSlot::Belt)].has_value());
    CHECK(back.equipment[EnumIndex(EquipSlot::Belt)]->baseId == "a_leather_belt");
    int worn = 0;
    for (const auto& e : back.equipment) worn += e.has_value() ? 1 : 0;
    CHECK(worn == 1);
    savejson::ReadEquipment(JsonValue::Array(), back);
    for (const auto& e : back.equipment) CHECK_FALSE(e.has_value());

    // inventory / stash lists keep order; non-objects are dropped, a non-array is an empty list.
    const std::vector<ItemInstance> bag = {sword, Stack("c_hp_potion_s", 7), ring};
    JsonWriter lw;
    savejson::WriteItemList(lw, bag);
    JsonValue lv;
    REQUIRE(ParseJson(lw.Take(), lv));
    std::vector<ItemInstance> list = {Itm("w_dagger")};
    savejson::ReadItemList(lv, list);
    CHECK(list == bag);
    JsonValue mixed;
    REQUIRE(ParseJson(R"([1,{"uid":"a","baseId":"w_dagger"},"x",null,{"uid":"b","baseId":"c_antidote","quantity":3}])",
                      mixed));
    savejson::ReadItemList(mixed, list);
    REQUIRE(list.size() == 2);
    CHECK(list[0].uid == "a");
    CHECK(list[1].quantity == 3);
    CHECK(list[0].sockets.empty());  // missing sockets -> [] (SaveMigration.test.ts)
    savejson::ReadItemList(JsonValue::Object(), list);
    CHECK(list.empty());

    // potionSlots: [hp, mp] strings; wrong types and missing entries read as "" (best available).
    JsonWriter pw;
    savejson::WritePotionSlots(pw, {std::string("c_hp_potion_m"), std::string()});
    const std::string ptext = pw.Take();
    CHECK(ptext == R"(["c_hp_potion_m",""])");
    JsonValue pv;
    REQUIRE(ParseJson(ptext, pv));
    std::array<std::string, 2> slots{"x", "y"};
    savejson::ReadPotionSlots(pv, slots);
    CHECK(slots[0] == "c_hp_potion_m");
    CHECK(slots[1].empty());
    REQUIRE(ParseJson(R"([5])", pv));
    savejson::ReadPotionSlots(pv, slots);
    CHECK(slots[0].empty());
    CHECK(slots[1].empty());
    REQUIRE(ParseJson(R"(["a","c_mp_potion_s","extra"])", pv));
    savejson::ReadPotionSlots(pv, slots);
    CHECK(slots[1] == "c_mp_potion_s");
    savejson::ReadPotionSlots(JsonValue::Null(), slots);
    CHECK(slots[0].empty());

    // itemUidCounter: an integer; missing / fractional / < 1 -> 1.
    JsonWriter cw;
    cw.StartObject();
    cw.Key("itemUidCounter");
    savejson::WriteItemUidCounter(cw, 0x2a);
    cw.EndObject();
    JsonValue cv;
    REQUIRE(ParseJson(cw.Take(), cv));
    CHECK(savejson::ReadItemUidCounter(cv.Get("itemUidCounter")) == 0x2a);
    CHECK(savejson::ReadItemUidCounter(cv.Get("missing")) == 1);
    CHECK(savejson::ReadItemUidCounter(JsonValue::Number(2.5)) == 1);
    CHECK(savejson::ReadItemUidCounter(JsonValue::Integer(-4)) == 1);
    CHECK(savejson::ReadItemUidCounter(JsonValue::String("9")) == 1);
  }
#endif

  TEST_CASE("potion slot binding (I4): only a potion of the slot's kind binds; saved and validated on load") {
    ItmWorld w;
    InventorySystem& inv = w.inv;
    CHECK(inv.SetPotionSlot(PotionSlot::Hp, "c_hp_potion_l"));
    CHECK(inv.SetPotionSlot(PotionSlot::Mp, "c_mp_potion_m"));
    // Wrong kind, non-potions, unknown and removed ids are refused; the binding stays.
    CHECK_FALSE(inv.SetPotionSlot(PotionSlot::Hp, "c_mp_potion_s"));
    CHECK_FALSE(inv.SetPotionSlot(PotionSlot::Mp, "c_hp_potion_s"));
    CHECK_FALSE(inv.SetPotionSlot(PotionSlot::Hp, "c_antidote"));
    CHECK_FALSE(inv.SetPotionSlot(PotionSlot::Hp, "w_dagger"));
    CHECK_FALSE(inv.SetPotionSlot(PotionSlot::Hp, "nope"));
    CHECK_FALSE(inv.SetPotionSlot(PotionSlot::Mp, "c_tp_scroll"));
    CHECK(inv.PotionSlots()[0] == "c_hp_potion_l");
    CHECK(inv.PotionSlots()[1] == "c_mp_potion_m");
    SaveData out;
    inv.WriteSave(out);
    CHECK(out.potionSlots[0] == "c_hp_potion_l");
    CHECK(out.potionSlots[1] == "c_mp_potion_m");
    // Unbinding.
    CHECK(inv.SetPotionSlot(PotionSlot::Hp, ""));
    CHECK(inv.PotionSlots()[0].empty());
    // Load: valid ids restored, an invalid one falls back to "" (best available).
    SaveData in;
    in.potionSlots = {std::string("c_hp_potion_s"), std::string("w_dagger")};
    inv.ReadSave(in);
    CHECK(inv.PotionSlots()[0] == "c_hp_potion_s");
    CHECK(inv.PotionSlots()[1].empty());
    in.potionSlots = {};
    inv.ReadSave(in);
    CHECK(inv.PotionSlots()[0].empty());
  }

  // ===================================================================================================================
  // Data balance guards (NumericalBalance.test.ts: item power scaling, gold economy)
  // ===================================================================================================================
  TEST_CASE("item power scaling: tiers exist, weapon damage and armour defense rise with levelReq") {
    const ItemTables& t = D().Items();
    std::vector<const ItemBaseDef*> weapons;
    std::vector<const ItemBaseDef*> armors;
    for (const ItemBaseDef& b : t.bases) {
      if (b.group == "weapons") weapons.push_back(&b);
      if (b.group == "armors") armors.push_back(&b);
    }
    REQUIRE_FALSE(weapons.empty());
    REQUIRE_FALSE(armors.empty());
    auto near = [](const std::vector<const ItemBaseDef*>& list, int tier) {
      int n = 0;
      for (const ItemBaseDef* b : list) n += std::abs(b->levelReq - tier) <= 3 ? 1 : 0;
      return n;
    };
    for (int tier : {1, 10, 15, 20, 28, 35}) CHECK_MESSAGE(near(weapons, tier) > 0, "weapon tier " << tier);
    for (int tier : {1, 10, 15, 20, 30, 35}) CHECK_MESSAGE(near(armors, tier) > 0, "armour tier " << tier);
    auto byLevel = [](std::vector<const ItemBaseDef*> v) {
      std::stable_sort(v.begin(), v.end(),
                       [](const ItemBaseDef* a, const ItemBaseDef* b) { return a->levelReq < b->levelReq; });
      return v;
    };
    for (WeaponType wt : {WeaponType::Sword, WeaponType::Dagger, WeaponType::Bow, WeaponType::Staff}) {
      std::vector<const ItemBaseDef*> v;
      for (const ItemBaseDef* b : weapons) {
        if (b->hasWeaponType && b->weaponType == wt && b->baseDamageMax > 0) v.push_back(b);
      }
      v = byLevel(v);
      CHECK(v.size() >= 2);
      for (size_t i = 0; i + 1 < v.size(); ++i) {
        if (v[i + 1]->levelReq <= v[i]->levelReq) continue;
        const double a = (v[i]->baseDamageMin + v[i]->baseDamageMax) / 2.0;
        const double b = (v[i + 1]->baseDamageMin + v[i + 1]->baseDamageMax) / 2.0;
        CHECK_MESSAGE(b >= a, v[i]->id << " -> " << v[i + 1]->id);
      }
    }
    for (EquipSlot slot : {EquipSlot::Helmet, EquipSlot::Armor, EquipSlot::Gloves, EquipSlot::Boots, EquipSlot::Belt}) {
      std::vector<const ItemBaseDef*> v;
      for (const ItemBaseDef* b : armors) {
        if (b->hasSlot && b->slot == slot) v.push_back(b);
      }
      v = byLevel(v);
      CHECK(v.size() >= 2);
      for (size_t i = 0; i + 1 < v.size(); ++i) {
        if (v[i + 1]->levelReq > v[i]->levelReq) CHECK_MESSAGE(v[i + 1]->baseDefense >= v[i]->baseDefense, v[i + 1]->id);
      }
    }
  }

  TEST_CASE("gold economy: kill gold rises by zone; a weapon near the hero level costs <= 200 average kills") {
    const MonsterTables& m = D().Monsters();
    const std::vector<std::string> zones = {"emerald_plains", "twilight_forest", "anvil_mountains", "scorching_desert",
                                            "abyss_rift"};
    auto avgNormalGold = [&](const std::string& zone) {
      const ZoneMonsterList* list = m.ZoneList(zone);
      REQUIRE(list != nullptr);
      double sum = 0;
      int n = 0;
      for (const std::string& id : list->monsterIds) {
        const MonsterDef* def = m.FindForZone(zone, id);
        REQUIRE(def != nullptr);
        if (def->elite) continue;
        sum += (def->goldMin + def->goldMax) / 2.0;
        ++n;
      }
      REQUIRE(n > 0);
      return sum / n;
    };
    for (size_t i = 0; i + 1 < zones.size(); ++i) {
      CHECK_MESSAGE(avgNormalGold(zones[i + 1]) > avgNormalGold(zones[i]), zones[i + 1]);
    }
    const ItemTables& t = D().Items();
    const int levels[] = {5, 15, 25, 35, 45};
    for (size_t z = 0; z < zones.size(); ++z) {
      const int lvl = levels[z];
      std::vector<const ItemBaseDef*> near;
      for (const ItemBaseDef& b : t.bases) {
        if (b.group == "weapons" && b.baseDamageMax > 0 && b.levelReq <= lvl && b.levelReq >= lvl - 10) near.push_back(&b);
      }
      if (near.empty()) continue;
      std::stable_sort(near.begin(), near.end(),
                       [](const ItemBaseDef* a, const ItemBaseDef* b) { return a->levelReq > b->levelReq; });
      const double buyPrice = near.front()->sellPrice * t.economy.buyPriceMultiplier;
      CHECK(t.economy.buyPriceMultiplier == 3);
      CHECK_MESSAGE(buyPrice <= avgNormalGold(zones[z]) * 200, "level " << lvl << " " << near.front()->id);
    }
  }

  TEST_CASE("elite affix loot bonus yields more loot (EliteAffixSystem.test.ts, seeded)") {
    Rng rng(31);
    ItemUidGenerator uids;
    const LootContext lc{&D(), &rng, &uids};
    LootRollInput in;
    in.monsterLevel = 20;
    in.elite = true;
    in.luck = 10;
    in.affixLootBonus = 15;
    for (int i = 0; i < 50; ++i) {
      for (const ItemInstance& it : GenerateLoot(lc, in)) CHECK(D().Items().FindBase(it.baseId) != nullptr);
    }
    in.monsterLevel = 30;
    size_t with = 0, without = 0;
    for (int i = 0; i < 500; ++i) {
      in.affixLootBonus = 20;
      with += GenerateLoot(lc, in).size();
      in.affixLootBonus = 0;
      without += GenerateLoot(lc, in).size();
    }
    CHECK(with > without);
  }

  TEST_CASE("gem stats reach the hero's derived stats (GemSocketing.test.ts recalcDerived)") {
    const HeroFormulas& f = D().Classes().formulas;
    Hero hero(D(), ClassId::Warrior);
    hero.RecalcDerived(EquipStats{});
    const HeroDerived base = hero.Derived();
    auto withGem = [&](std::string_view itemBase, EquipSlot slot, std::string_view gemId) {
      Inventory inv(D());
      ItemInstance it = Itm(itemBase);
      inv.MutableEquipment()[EnumIndex(slot)] = it;
      ItemInstance gem = Stack(gemId, 1);
      inv.MutableBag().push_back(gem);
      REQUIRE(inv.SocketGem(slot, gem.uid) == InvResult::Ok);
      Hero h(D(), ClassId::Warrior);
      h.RecalcDerived(inv.GearStats());
      return h.Derived();
    };
    using doctest::Approx;
    const GemInstance ruby = GemOf("g_ruby_1");
    REQUIRE(ruby.stat == Stat::Str);
    const HeroDerived str = withGem("w_short_sword", EquipSlot::Weapon, "g_ruby_1");
    CHECK(str.baseDamage - base.baseDamage == Approx(ruby.value * f.baseDamagePerStr));
    CHECK(str.baseDamage - base.baseDamage == Approx(4.0));  // 5 str x 0.8
    CHECK(str.maxHp == base.maxHp);
    const GemInstance sapphire = GemOf("g_sapphire_2");
    REQUIRE(sapphire.stat == Stat::Int);
    const HeroDerived intl = withGem("a_chain_mail", EquipSlot::Armor, "g_sapphire_2");
    CHECK(intl.maxMana - base.maxMana == sapphire.value * f.maxManaPerInt);
    CHECK(intl.maxMana - base.maxMana == 36);  // 12 int x 3
    const GemInstance diamond = GemOf("g_diamond_1");
    REQUIRE(diamond.stat == Stat::AllStats);
    const HeroDerived vit = withGem("w_claymore", EquipSlot::Weapon, "g_diamond_1");
    CHECK(vit.maxHp - base.maxHp == diamond.value * f.maxHpPerVit);
    CHECK(vit.maxHp - base.maxHp == 30);  // allStats 3 -> vit 3 x 10
  }

  TEST_CASE("shop close carries the shop's npcId, once (ShopCloseMerchantDespawn.test.ts)") {
    ItmWorld w;
    w.shop.Close();  // nothing open: no event
    CHECK(test::CountEvents<EvShopClosed>(w.h.events) == 0);
    w.shop.OpenWanderingMerchant({"c_hp_potion_s"}, 1.2);
    w.shop.Close();
    w.shop.Close();  // a second close of the same panel does nothing
    REQUIRE(test::CountEvents<EvShopClosed>(w.h.events) == 1);
    CHECK(LastEvent<EvShopClosed>(w.h.events)->npcId == kWanderingMerchantShopId);
    w.shop.Open("merchant", false);
    w.shop.Close();
    REQUIRE(test::CountEvents<EvShopClosed>(w.h.events) == 2);
    CHECK(LastEvent<EvShopClosed>(w.h.events)->npcId == "merchant");
    CHECK(LastEvent<EvShopOpened>(w.h.events)->npcId == "merchant");
  }

  // ===================================================================================================================
  // Audit fixes (round 2): spec vectors that the first port missed
  // ===================================================================================================================
  TEST_CASE("20.8 through the runtime: grants, ground pickups, auto-loot and stash take keep the top-up") {
    ItmWorld w;
    auto fullWithPartial = [&w]() {
      w.inv.Items().MutableBag().clear();
      w.inv.Items().MutableBag().push_back(Stack("c_hp_potion_s", 19));
      w.FillBag(99);
    };
    // Grant: Refuse keeps the remainder with the caller; Stash pushes only the remainder; Lose drops only the remainder.
    fullWithPartial();
    ItemInstance three = Stack("c_hp_potion_s", 3);
    w.h.events.Clear();
    CHECK(w.inv.Grant(three, OverflowPolicy::Refuse, ItemSource::Pickup) == ItemGrantOutcome::Refused);
    CHECK(three.quantity == 2);
    CHECK(w.inv.Items().CountOf("c_hp_potion_s") == 20);
    CHECK(test::CountEvents<EvInventoryChanged>(w.h.events) == 1);
    CHECK(test::CountEvents<EvItemPicked>(w.h.events) == 0);  // the web emitted ITEM_PICKED only on success
    fullWithPartial();
    ItemInstance toStash = Stack("c_hp_potion_s", 3);
    CHECK(w.inv.Grant(toStash, OverflowPolicy::Stash, ItemSource::QuestReward) == ItemGrantOutcome::Stash);
    REQUIRE(w.inv.Items().Stash().size() == 1);
    CHECK(w.inv.Items().Stash()[0].quantity == 2);  // web: stash.push(item) after addItem reduced it
    CHECK(w.inv.Items().CountOf("c_hp_potion_s") == 20);
    w.inv.Items().MutableStash().clear();
    fullWithPartial();
    ItemInstance toLose = Stack("c_hp_potion_s", 3);
    CHECK(w.inv.Grant(toLose, OverflowPolicy::Lose, ItemSource::RandomEvent) == ItemGrantOutcome::Lost);
    CHECK(w.inv.Items().CountOf("c_hp_potion_s") == 20);

    // Click pickup: the bag takes one, two stay on the ground (the web left the mutated item there).
    fullWithPartial();
    w.hero.SetPosition(Vec2(10, 10));
    const EntityId drop = w.ground.Drop(Stack("c_hp_potion_s", 3), Vec2(10, 11));
    w.h.events.Clear();
    CHECK_FALSE(w.ground.TryPickUp(drop));
    REQUIRE(w.ground.Find(drop) != nullptr);
    CHECK(w.ground.Find(drop)->item.quantity == 2);
    CHECK(w.inv.Items().CountOf("c_hp_potion_s") == 20);
    CHECK(HasLogKey(w.h.events, "sys.inventory.bagFull"));
    Snapshot snap;
    w.ground.FillSnapshot(snap);
    REQUIRE(snap.groundItems.size() == 1);
    CHECK(snap.groundItems[0].quantity == 2);
    // Room again: the rest is picked up.
    w.inv.Items().MutableBag().pop_back();
    CHECK(w.ground.TryPickUp(drop));
    CHECK(w.inv.Items().CountOf("c_hp_potion_s") == 22);

    // Auto-loot: same top-up, then the scan stops.
    fullWithPartial();
    w.ground.OnZoneExit();
    const EntityId autoDrop = w.ground.Drop(Stack("c_hp_potion_s", 4), Vec2(10, 11));
    w.hero.autoLoot = AutoLootMode::All;
    w.h.Step(static_cast<int>(400 / kSimStepMs));
    w.ground.Tick();
    REQUIRE(w.ground.Find(autoDrop) != nullptr);
    CHECK(w.ground.Find(autoDrop)->item.quantity == 3);
    CHECK(w.inv.Items().CountOf("c_hp_potion_s") == 20);

    // Stash take: the stash entry keeps the remainder.
    fullWithPartial();
    ItemInstance stashed = Stack("c_hp_potion_s", 3);
    stashed.uid = "stashed";
    w.inv.Items().MutableStash() = {stashed};
    w.inv.OpenStash("stash");
    w.h.events.Clear();
    CHECK(w.inv.StashTake("stashed") == InvResult::BagFull);
    REQUIRE(w.inv.Items().FindInStash("stashed") != nullptr);
    CHECK(w.inv.Items().FindInStash("stashed")->quantity == 2);
    CHECK(w.inv.Items().CountOf("c_hp_potion_s") == 20);
    CHECK(test::CountEvents<EvStashChanged>(w.h.events) == 1);
    CHECK(HasLogKey(w.h.events, "ui.stash.bagFull"));
  }

  TEST_CASE("ground drops carry the item: nameplate inputs in EvLootDropped and the snapshot (17 / 18, FIX Q16)") {
    ItmWorld w;
    Rng rng(3);
    const LootContext lc{&D(), &rng, &w.inv.Uids()};
    std::optional<ItemInstance> grief = CreateItem(lc, "w_broad_sword", 6, ItemQuality::Legendary);
    REQUIRE(grief.has_value());
    REQUIRE(grief->legendaryId == "leg_grief");
    std::optional<ItemInstance> rare = CreateItem(lc, "w_short_sword", 6, ItemQuality::Rare);
    REQUIRE(rare.has_value());
    REQUIRE(rare->affixes.size() >= 3);
    const EntityId gid = w.ground.Drop(*grief, Vec2(5, 5));
    const EvLootDropped* ld = LastEvent<EvLootDropped>(w.h.events);
    REQUIRE(ld != nullptr);
    CHECK(ld->item == *grief);
    CHECK_FALSE(ld->cacheDrop);
    CHECK(ld->fallInMs == 0);
    CHECK(ld->visualOffset == w.ground.Find(gid)->visualOffset);
    I18n en = D().Strings();
    en.SetLocale(LocaleId::En);
    CHECK(ItemDisplayName(ld->item, D(), en) == *en.Lookup(LocaleId::En, "data.legendary.leg_grief.name"));
    const EntityId rid = w.ground.Drop(*rare, Vec2(6, 5));
    Snapshot snap;
    w.ground.FillSnapshot(snap);
    REQUIRE(snap.groundItems.size() == 2);
    REQUIRE(snap.groundItems[1].item != nullptr);
    CHECK(snap.groundItems[1].item == &w.ground.Find(rid)->item);
    CHECK(ItemDisplayName(*snap.groundItems[1].item, D(), en) == ItemDisplayName(*rare, D(), en));
    CHECK(ItemDisplayName(*snap.groundItems[1].item, D(), en) != *en.Lookup(LocaleId::En, "data.item.w_short_sword.name"));
    CHECK(snap.groundItems[0].item->legendaryId == "leg_grief");
    // The pickup event carries the item too (HUD notice name).
    w.hero.SetPosition(Vec2(5, 5));
    CHECK(w.ground.TryPickUp(gid));
    const EvItemPicked* ip = LastEvent<EvItemPicked>(w.h.events);
    REQUIRE(ip != nullptr);
    CHECK(ip->item.legendaryId == "leg_grief");
    CHECK(ip->item.uid == grief->uid);
  }

  TEST_CASE("treasure-cache drops (6.1 / 6.5): flagged, fall in over 400 ms, own jitter, no despawn") {
    ItmWorld w;
    const LootRulesDef& l = D().Items().loot;
    CHECK(l.cacheDropFallInMs == 400);
    CHECK(l.cacheDropJitterXPx == 20);
    CHECK(l.cacheDropJitterYPx == 10);
    // Inverse iso of the largest screen jitter (10 px, 5 px): |dc|, |dr| <= 10 / 64 + 5 / 32.
    const double bound = 10.0 / 64.0 + 5.0 / 32.0 + 1e-9;
    for (int i = 0; i < 200; ++i) {
      const EntityId id = w.ground.Drop(Itm("w_dagger", ItemQuality::Set), Vec2(8, 8), /*despawns=*/false);
      const EvLootDropped* ld = LastEvent<EvLootDropped>(w.h.events);
      REQUIRE(ld != nullptr);
      CHECK(ld->drop == id);
      CHECK(ld->cacheDrop);
      CHECK(ld->fallInMs == 400);
      CHECK(ld->expiresAtMs == 0);
      const GroundItem* g = w.ground.Find(id);
      REQUIRE(g != nullptr);
      CHECK(g->cacheDrop);
      CHECK(std::fabs(g->visualOffset.x) <= bound);
      CHECK(std::fabs(g->visualOffset.y) <= bound);
      // Back to screen px: within +-10 / +-5.
      const double sx = (g->visualOffset.x - g->visualOffset.y) * 32.0;
      const double sy = (g->visualOffset.x + g->visualOffset.y) * 16.0;
      CHECK(std::fabs(sx) <= 10.0 + 1e-9);
      CHECK(std::fabs(sy) <= 5.0 + 1e-9);
    }
    Snapshot snap;
    w.ground.FillSnapshot(snap);
    REQUIRE_FALSE(snap.groundItems.empty());
    CHECK(snap.groundItems[0].cacheDrop);
  }

  TEST_CASE("treasure-cache luck (5.5 + I1): raw lck + gear, without homestead / pet magicFind") {
    ItmWorld w;
    PetSystem pets(w.h.ctx.data, w.h.events, w.h.bus);
    w.h.ctx.sys.pets = &pets;
    REQUIRE(pets.AddPet("pet_owl", true));
    const double petMf = pets.Bonuses().Get(Stat::MagicFind);
    REQUIRE(petMf > 0);
    const double base = w.hero.BaseStats().lck;
    CHECK(w.ground.TreasureCacheLuck() == base);
    CHECK(w.ground.LootLuck() == base + petMf);
    ItemInstance ring = Itm("j_copper_ring", ItemQuality::Magic);
    ring.affixes = {StatAffix(Stat::Lck, 4), StatAffix(Stat::MagicFind, 6)};
    ComputeItemStats(ring);
    w.inv.Items().MutableEquipment()[EnumIndex(EquipSlot::Ring1)] = ring;
    CHECK(w.ground.TreasureCacheLuck() == base + 10);
    CHECK(w.ground.LootLuck() == base + 10 + petMf);
  }

  TEST_CASE("wandering merchant (12.7, FIX Q12): every event id maps to a base of the zone's tier") {
    ItmWorld w;
    for (const ZoneEventDataDef& z : D().World().randomEvents.zones) {
      const MapDef* map = D().FindMap(z.zoneId);
      REQUIRE(map != nullptr);
      REQUIRE_FALSE(z.merchantItems.empty());
      for (const std::string& id : z.merchantItems) {
        const std::string base = ShopSystem::ResolveWanderingMerchantId(D(), z.zoneId, id);
        INFO(z.zoneId << " " << id);
        REQUIRE_FALSE(base.empty());
        const ItemBaseDef* b = D().Items().FindBase(base);
        REQUIRE(b != nullptr);
        CHECK_FALSE(D().Items().IsRemovedItem(base));
        CHECK(b->levelReq <= map->levelMax);  // buyable gear of the zone's tier
      }
      w.h.session.currentMap = z.zoneId;
      w.shop.OpenWanderingMerchant(z.merchantItems, 1.0);
      CHECK(w.shop.State().wares.size() == z.merchantItems.size());  // never an empty panel
    }
    w.h.session.currentMap = "emerald_plains";
    w.shop.OpenWanderingMerchant(D().World().randomEvents.ForZone("emerald_plains")->merchantItems, 1.0);
    std::vector<std::string> got;
    for (const ShopWare& ware : w.shop.State().wares) got.push_back(ware.baseId);
    CHECK(got == std::vector<std::string>{"w_short_sword", "a_leather_armor", "c_hp_potion_s", "c_mp_potion_s"});
    // The current zone's map wins for ids every zone sells.
    CHECK(ShopSystem::ResolveWanderingMerchantId(D(), "twilight_forest", "hp_potion") == "c_hp_potion_m");
    CHECK(ShopSystem::ResolveWanderingMerchantId(D(), "", "hp_potion") == "c_hp_potion_s");
    CHECK(ShopSystem::ResolveWanderingMerchantId(D(), "emerald_plains", "c_antidote") == "c_antidote");
    CHECK(ShopSystem::ResolveWanderingMerchantId(D(), "emerald_plains", "nope").empty());
    // Buying from it works like any shop.
    w.hero.SetGold(1000);
    CHECK(w.shop.Buy(2) == InvResult::Ok);
    CHECK(w.inv.Items().CountOf("c_hp_potion_s") == 1);
  }

  TEST_CASE("C11 effects in the compare totals: deltas show them and the weaker ring counts them (14, FIX Q18)") {
    Rng rng(2);
    ItemUidGenerator uids;
    const LootContext lc{&D(), &rng, &uids};
    std::optional<ItemInstance> tyrael = CreateItem(lc, "a_plate_armor", 35, ItemQuality::Legendary);
    REQUIRE(tyrael.has_value());
    REQUIRE(tyrael->legendaryId == "leg_tyrael");
    const LegendaryDef* tyraelDef = D().Items().FindLegendary("leg_tyrael");
    double dr = 0;
    for (const StatTotal& t : ItemStatTotals(D(), *tyrael)) {
      if (t.key.kind == CompareKey::Kind::StatKey && t.key.stat == Stat::DamageReduction) dr = t.value;
    }
    CHECK(dr == tyraelDef->specialEffectValue);
    CHECK(dr == 10);
    ItemInstance plain = Itm("a_plate_armor", ItemQuality::Normal, 35);
    bool shown = false;
    for (const StatDelta& d : StatDeltas(D(), *tyrael, &plain)) {
      if (d.key.kind == CompareKey::Kind::StatKey && d.key.stat == Stat::DamageReduction) shown = d.delta == 10;
    }
    CHECK(shown);
    // The same value as the gear stat (one source of truth).
    Inventory inv(D());
    inv.MutableEquipment()[EnumIndex(EquipSlot::Armor)] = *tyrael;
    CHECK(inv.GearStats().Get(Stat::DamageReduction) == dr);
    // Grief / Mara's: an effect without a combat consumer adds nothing.
    std::optional<ItemInstance> maras = CreateItem(lc, "j_jade_amulet", 35, ItemQuality::Legendary);
    REQUIRE(maras->legendaryId == "leg_maras");
    Stat st{};
    double v = 0;
    CHECK_FALSE(ItemSpecialEffectStat(*maras, D(), st, v));
    CHECK_FALSE(ItemSpecialEffectStat(Itm("w_dagger", ItemQuality::Legendary), D(), st, v));  // generic: no effect

    // Rings: SoJ at L35 scores 25 + 20 + 12 + 3.5 = 60.5 without its cooldownReduction 10 and 70.5 with it; a 65.5 ring
    // is the weaker one, so the new ring replaces it, not the SoJ.
    std::optional<ItemInstance> soj = CreateItem(lc, "j_gold_ring", 35, ItemQuality::Legendary);
    REQUIRE(soj->legendaryId == "leg_soj");
    ItemInstance other = Itm("j_silver_ring", ItemQuality::Magic, 35);
    other.affixes = {StatAffix(Stat::Str, 62)};
    ComputeItemStats(other);
    Inventory::EquipmentArray eq{};
    eq[EnumIndex(EquipSlot::Ring1)] = *soj;
    eq[EnumIndex(EquipSlot::Ring2)] = other;
    const ItemInstance candidate = Itm("j_copper_ring", ItemQuality::Normal, 1);
    const std::optional<CompareTarget> ct = FindCompareTarget(D(), candidate, eq);
    REQUIRE(ct.has_value());
    CHECK(ct->slot == EquipSlot::Ring2);
    Inventory rings(D());
    rings.MutableEquipment() = eq;
    rings.MutableBag().push_back(candidate);
    EquipSlot used = EquipSlot::Weapon;
    CHECK(rings.Equip(candidate.uid, 40, &used) == InvResult::Ok);
    CHECK(used == EquipSlot::Ring2);
    CHECK(rings.Equipped(EquipSlot::Ring1)->legendaryId == "leg_soj");
  }

  TEST_CASE("sort uses the installed culture collator (7.3 port rule); default is byte order") {
    Inventory inv(D());
    I18n en = D().Strings();
    en.SetLocale(LocaleId::En);
    inv.MutableBag() = {Itm("w_dagger"), Itm("w_short_sword"), Itm("w_rusty_sword")};
    inv.SortBag(&en);
    CHECK(ItemDisplayName(inv.Bag()[0], D(), en) == "Dagger");
    CHECK(ItemDisplayName(inv.Bag()[2], D(), en) == "Short Sword");
    CHECK(CompareItemNames("a", "b") < 0);
    CHECK(CompareItemNames("b", "a") > 0);
    CHECK(CompareItemNames("a", "a") == 0);
    int calls = 0;
    const ItemNameCollatorBinding previous = SetItemNameCollator(&ItmTestReverseCollate, &calls);
    inv.SortBag(&en);
    inv.MutableStash() = {Itm("w_dagger"), Itm("w_short_sword")};
    inv.SortStash(&en);
    SetItemNameCollator(previous.fn, previous.user);
    CHECK(calls > 0);
    CHECK(ItemDisplayName(inv.Bag()[0], D(), en) == "Short Sword");
    CHECK(ItemDisplayName(inv.Bag()[1], D(), en) == "Rusty Sword");
    CHECK(ItemDisplayName(inv.Bag()[2], D(), en) == "Dagger");
    CHECK(inv.Stash()[0].baseId == "w_short_sword");
    // Quality and type still come first.
    inv.MutableBag().push_back(Itm("c_hp_potion_s"));
    inv.MutableBag().push_back(Itm("w_dagger", ItemQuality::Rare));
    SetItemNameCollator(&ItmTestReverseCollate, nullptr);
    inv.SortBag(&en);
    SetItemNameCollator(previous.fn, previous.user);
    CHECK(inv.Bag().front().quality == ItemQuality::Rare);
    CHECK(inv.Bag().back().baseId == "c_hp_potion_s");
    CHECK(CompareItemNames("a", "b") < 0);  // restored
  }

  TEST_CASE("legendary effect text is localised (4.3 / 15.3, Q5): named and generic legendaries") {
    Rng rng(4);
    ItemUidGenerator uids;
    const LootContext lc{&D(), &rng, &uids};
    I18n zh = D().Strings();
    zh.SetLocale(LocaleId::ZhCN);
    I18n en = D().Strings();
    en.SetLocale(LocaleId::En);
    std::optional<ItemInstance> gen = CreateItem(lc, "w_rusty_sword", 10, ItemQuality::Legendary);
    REQUIRE(gen.has_value());
    REQUIRE(gen->legendaryId.empty());
    CHECK(gen->legendaryEffect == *D().Strings().Lookup(LocaleId::ZhCN, "sys.loot.genericLegendaryEffect"));  // stored zh
    CHECK(ItemLegendaryEffectText(*gen, D(), en) == *en.Lookup(LocaleId::En, "sys.loot.genericLegendaryEffect"));
    CHECK(ItemLegendaryEffectText(*gen, D(), en) == "Contains an unknown power");
    CHECK(ItemLegendaryEffectText(*gen, D(), zh) == gen->legendaryEffect);
    std::optional<ItemInstance> grief = CreateItem(lc, "w_broad_sword", 10, ItemQuality::Legendary);
    REQUIRE(grief->legendaryId == "leg_grief");
    CHECK(ItemLegendaryEffectText(*grief, D(), en) == *en.Lookup(LocaleId::En, "data.legendary.leg_grief.effect"));
    CHECK(ItemLegendaryEffectText(*grief, D(), zh) == *zh.Lookup(LocaleId::ZhCN, "data.legendary.leg_grief.effect"));
    CHECK(ItemLegendaryEffectText(Itm("w_dagger", ItemQuality::Rare), D(), en).empty());
  }

  TEST_CASE("I3 refusal logs sys.inventory.levelTooLow {level}; discard emits ItemDiscarded {item} (17)") {
    ItmWorld w;
    for (LocaleId loc : {LocaleId::ZhCN, LocaleId::En}) {
      REQUIRE(D().Strings().Lookup(loc, "sys.inventory.levelTooLow") != nullptr);
      CHECK(D().Strings().Lookup(loc, "sys.inventory.levelTooLow")->find("{level}") != std::string::npos);
    }
    ItemInstance plate = Itm("a_plate_armor");
    w.inv.Items().MutableBag().push_back(plate);
    CHECK(w.inv.Equip(plate.uid) == InvResult::LevelTooLow);
    const EvLog* log = LastLogWithKey(w.h.events, "sys.inventory.levelTooLow");
    REQUIRE(log != nullptr);
    REQUIRE(log->text.args.size() == 1);
    CHECK(log->text.args[0].name == "level");
    CHECK(log->text.args[0].value == "20");
    CHECK_FALSE(HasLogKey(w.h.events, "homestead.workshop.block.level"));
    CHECK(LastEvent<EvSfx>(w.h.events)->cue == SfxId::Error);
    // Discard.
    ItemInstance rare = Itm("w_dagger", ItemQuality::Rare);
    rare.affixes = {FixedAffix("pre_sharp", 2)};
    w.inv.Items().MutableBag().push_back(rare);
    w.h.events.Clear();
    CHECK(w.inv.Discard(rare.uid) == InvResult::Ok);
    REQUIRE(test::CountEvents<EvItemDiscarded>(w.h.events) == 1);
    CHECK(LastEvent<EvItemDiscarded>(w.h.events)->item == rare);
    CHECK(test::CountEvents<EvInventoryChanged>(w.h.events) == 1);
    CHECK(HasLogKey(w.h.events, "sys.inventory.discarded"));
    CHECK(w.inv.Discard(rare.uid) == InvResult::UnknownItem);
    CHECK(test::CountEvents<EvItemDiscarded>(w.h.events) == 1);
  }

  TEST_CASE("quest pick-one gear is always equippable at turn-in (5.6 with I3)") {
    CHECK(D().Items().loot.rewardCapUsableAtHeroLevel);
    // Without the cap (the web pool) a level-13 jewelry pick can be the jade amulet (levelReq 15)...
    Rng rng(1);
    rng.Script({0.0});
    CHECK(PickRewardBase(D(), RewardSlot::Jewelry, ClassId::Rogue, 13, rng)->id == "j_jade_amulet");
    // ... with the hero-level cap the top-3 window is silver ring (10), copper ring (1), bone amulet (1).
    rng.Script({0.0});
    CHECK(PickRewardBase(D(), RewardSlot::Jewelry, ClassId::Rogue, 13, rng, 13)->id == "j_silver_ring");
    rng.Script({0.99});
    CHECK(PickRewardBase(D(), RewardSlot::Jewelry, ClassId::Rogue, 13, rng, 13)->id == "j_bone_amulet");
    // Nothing under the cap: the pool's lowest base (web fallback).
    rng.Script({0.5});
    CHECK(PickRewardBase(D(), RewardSlot::Gloves, ClassId::Warrior, 13, rng, 0)->levelReq == 1);

    const QuestDef* secure = D().FindQuest("q_secure_plains");  // L8 main: jewelry + boots + gloves
    const QuestDef* chief = D().FindQuest("q_find_goblin_chief");  // L7 main: weapon + armor
    REQUIRE(secure != nullptr);
    REQUIRE(chief != nullptr);
    for (ClassId cls : {ClassId::Warrior, ClassId::Mage, ClassId::Rogue}) {
      for (int heroLevel : {1, 5, 9, 13, 20}) {
        for (uint64_t seed = 1; seed <= 40; ++seed) {
          Rng r(seed);
          ItemUidGenerator uids;
          const LootContext lc{&D(), &r, &uids};
          for (const QuestDef* q : {secure, chief}) {
            const std::vector<ItemInstance> items = GenerateQuestRewardChoices(lc, *q, cls, heroLevel);
            CHECK(items.size() == q->rewards.choices.size());
            for (const ItemInstance& it : items) {
              const ItemBaseDef* b = D().Items().FindBase(it.baseId);
              REQUIRE(b != nullptr);
              INFO(q->id << " hero " << heroLevel << " " << it.baseId);
              CHECK(b->levelReq <= heroLevel);
              Inventory inv(D());
              inv.MutableBag().push_back(it);
              CHECK(inv.Equip(it.uid, heroLevel) == InvResult::Ok);
            }
          }
        }
      }
    }
  }
}
