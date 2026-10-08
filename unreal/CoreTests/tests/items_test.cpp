// Items area: item instances, loot generation, bag / equipment / stash, shops, crafting, compare, ground loot.
// Spec vectors: loot-items-inventory.md section 20. Foundation smoke tests only.
#include "SimHarness.h"
#include "abyss/items/Crafting.h"
#include "abyss/items/Inventory.h"
#include "abyss/items/Item.h"
#include "doctest/doctest.h"

using namespace abyss;

TEST_SUITE("items") {
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
  }

  TEST_CASE("craft gold unit (loot 13): 6 * (max(1, L) + 5)") {
    CHECK(CraftGoldUnit(1) == 36);
    CHECK(CraftGoldUnit(5) == 60);
    CHECK(CraftGoldUnit(40) == 270);
    CHECK(CraftGoldUnit(0) == 36);
  }

  TEST_CASE("inventory container lookups") {
    Inventory inv(test::RealData());
    ItemInstance a;
    a.uid = "i1";
    a.baseId = "c_hp_potion_s";
    a.quantity = 3;
    inv.MutableBag().push_back(a);
    CHECK(inv.CountOf("c_hp_potion_s") == 3);
    CHECK(inv.FindInBag("i1") != nullptr);
    CHECK(inv.TakeEntry("i1").has_value());
    CHECK(inv.Bag().empty());
    CHECK(inv.Equipped(EquipSlot::Weapon) == nullptr);
    // Read-only equipment for tooltip compare (FindCompareTarget) through a const Inventory (Snapshot::inventory).
    ItemInstance ring;
    ring.uid = "i9";
    ring.baseId = "j_copper_ring";
    inv.MutableEquipment()[EnumIndex(EquipSlot::Ring1)] = ring;
    const Inventory& view = inv;
    const Inventory::EquipmentArray& eq = view.Equipment();
    REQUIRE(eq[EnumIndex(EquipSlot::Ring1)].has_value());
    CHECK(eq[EnumIndex(EquipSlot::Ring1)]->uid == "i9");
    CHECK_FALSE(eq[EnumIndex(EquipSlot::Ring2)].has_value());
  }

  TEST_CASE("stash session (I7): transfers need the stash keeper's session; grants follow the overflow policy") {
    test::SimHarness h;
    InventorySystem inv(h.ctx);
    CHECK_FALSE(inv.Stash().open);
    CHECK(inv.StashPut("i1") == InvResult::StashClosed);
    CHECK(inv.StashTake("i1") == InvResult::StashClosed);
    inv.OpenStash("stash");
    CHECK(inv.Stash().open);
    CHECK(inv.Stash().npcId == "stash");
    inv.CloseStash();
    CHECK_FALSE(inv.Stash().open);
    CHECK(inv.StashCapacity() == kBaseStashSlots);

    // Overflow policies (the bag add itself is still a stub, so every grant reaches the policy).
    ItemInstance item;
    item.uid = "i5";
    item.baseId = "w_short_sword";
    CHECK(inv.Grant(item, OverflowPolicy::Refuse, ItemSource::Shop) == ItemGrantOutcome::Refused);
    CHECK(item.uid == "i5");
    CHECK(inv.Grant(item, OverflowPolicy::Stash, ItemSource::QuestReward) == ItemGrantOutcome::Stash);
    REQUIRE(inv.Items().Stash().size() == 1);
    CHECK(inv.Items().Stash()[0].uid == "i5");
    CHECK(test::CountEvents<EvStashChanged>(h.events) == 1);
    ItemInstance lost;
    lost.uid = "i6";
    CHECK(inv.Grant(lost, OverflowPolicy::Lose, ItemSource::RandomEvent) == ItemGrantOutcome::Lost);
    CHECK(test::CountEvents<EvLog>(h.events) == 1);  // sys.inventory.bagFull
  }

  TEST_CASE("potion quick slots (I4): bound base, else the strongest potion of the kind in the bag") {
    test::SimHarness h;
    InventorySystem inv(h.ctx);
    CHECK(inv.ResolvePotionSlot(PotionSlot::Hp).empty());
    ItemInstance s;
    s.uid = "i1";
    s.baseId = "c_hp_potion_s";
    s.quantity = 2;
    ItemInstance m;
    m.uid = "i2";
    m.baseId = "c_hp_potion_m";
    m.quantity = 1;
    inv.Items().MutableBag() = {s, m};
    CHECK(inv.ResolvePotionSlot(PotionSlot::Hp) == "c_hp_potion_m");
    CHECK(inv.ResolvePotionSlot(PotionSlot::Mp).empty());
    inv.SetPotionSlot(PotionSlot::Hp, "c_hp_potion_s");
    const PotionSlotView v = inv.PotionSlotState(PotionSlot::Hp);
    CHECK(v.bound);
    CHECK(v.baseId == "c_hp_potion_s");
    CHECK(v.count == 2);
  }
}
