// Hero area (hero+combat owner): hero state, skills, spirit, buffs. Spec checklist: classes-stats-skills.md section 22.
// Foundation smoke tests only; the owner adds the spec vectors here.
#include "SimHarness.h"
#include "TestUtil.h"
#include "abyss/hero/Buffs.h"
#include "abyss/hero/Hero.h"
#include "abyss/hero/Rewards.h"
#include "abyss/items/Inventory.h"
#include "doctest/doctest.h"

using namespace abyss;

TEST_SUITE("hero") {
  TEST_CASE("hero constructs for every class with its base stats and skill book") {
    const DataStore& data = test::RealData();
    for (ClassId cls : {ClassId::Warrior, ClassId::Mage, ClassId::Rogue}) {
      Hero h(data, cls);
      const ClassDef* def = data.Classes().Find(cls);
      REQUIRE(def != nullptr);
      CHECK(h.Class() == cls);
      CHECK(h.BaseStats() == def->baseStats);
      CHECK(h.Level() == 1);
      CHECK(h.Skills().SkillCount() == def->skills.size());
      CHECK(h.Skills().HotbarSkill(0) == -1);
      // expToNext(L) = floor(3 L^2 + 25 L) (classes 5.1): level 1 -> 28.
      CHECK(h.ExpToNext() == 28);
    }
  }

  TEST_CASE("gold spending never goes negative") {
    Hero h(test::RealData(), ClassId::Rogue);
    h.AddGold(50);
    CHECK_FALSE(h.SpendGold(60));
    CHECK(h.Gold() == 50);
    CHECK(h.SpendGold(20));
    CHECK(h.Gold() == 30);
  }

  TEST_CASE("RewardService: the one exp / gold / item path (events, reasons, Dying gates)") {
    test::SimHarness h;
    Hero hero(h.ctx.data, ClassId::Warrior);
    InventorySystem inv(h.ctx);
    RewardService rewards(h.ctx);
    h.ctx.sys.hero = &hero;
    h.ctx.sys.inventory = &inv;
    h.ctx.sys.rewards = &rewards;

    // Exp: EvExpGained carries the source on every call (PLAYER_EXP_CHANGED).
    rewards.GrantExp(5, ExpSource::Quest);
    REQUIRE(test::CountEvents<EvExpGained>(h.events) == 1);
    const auto* exp = std::get_if<EvExpGained>(&h.events.Items().back());
    REQUIRE(exp != nullptr);
    CHECK(exp->source == ExpSource::Quest);
    CHECK(exp->amount == 5);
    CHECK(exp->expToNext == hero.ExpToNext());
    rewards.GrantExp(-10, ExpSource::Kill);  // negative grants are 0 (RemoveExp is the toll path)
    CHECK(std::get_if<EvExpGained>(&h.events.Items().back())->amount == 0);

    // Gold: applied delta + reason; never below 0; saturating; no event for a no-op.
    h.events.Clear();
    CHECK(rewards.ChangeGold(100, GoldReason::Kill) == 100);
    CHECK(rewards.ChangeGold(-150, GoldReason::DeathPenalty) == -100);
    CHECK(hero.Gold() == 0);
    CHECK(rewards.ChangeGold(-5, GoldReason::DeathPenalty) == 0);
    REQUIRE(test::CountEvents<EvGoldChanged>(h.events) == 2);
    const auto* gold = std::get_if<EvGoldChanged>(&h.events.Items()[1]);
    CHECK(gold->gold == 0);
    CHECK(gold->delta == -100);
    CHECK(gold->reason == GoldReason::DeathPenalty);
    CHECK(rewards.ChangeGold(40, GoldReason::HiddenReward) == 40);
    CHECK_FALSE(rewards.SpendGold(41, GoldReason::ShopBuy));
    CHECK(rewards.SpendGold(15, GoldReason::ShopBuy));
    CHECK(hero.Gold() == 25);
    CHECK_FALSE(rewards.SpendGold(-1, GoldReason::ShopBuy));
    hero.SetGold(9223372036854775800);
    CHECK(rewards.ChangeGold(100, GoldReason::Debug) == 7);  // saturates at INT64_MAX
    hero.SetGold(25);

    // Dying (save-ui-input 5.1.1): player transactions and pickups are refused, passive credits still apply.
    hero.SetLife(HeroLife::Dying);
    CHECK(rewards.ChangeGold(10, GoldReason::ShopSell) == 0);
    CHECK_FALSE(rewards.SpendGold(5, GoldReason::Craft));
    CHECK(rewards.ChangeGold(10, GoldReason::Kill) == 10);
    ItemInstance drop;
    drop.uid = "i1";
    drop.baseId = "c_hp_potion_s";
    CHECK(rewards.GrantItem(drop, OverflowPolicy::Refuse, ItemSource::Pickup) == ItemGrantOutcome::Refused);
    CHECK(drop.uid == "i1");  // the caller keeps a refused item
    hero.SetLife(HeroLife::Alive);
  }

  TEST_CASE("Hero / SkillBook fall back safely for an unknown class (no null deref under a returning assert)") {
    test::ScopedAssertCounter asserts;
    Hero h(test::RealData(), static_cast<ClassId>(7));
    CHECK(asserts.Count() >= 1);
    CHECK(h.ClassData().skills.empty());
    CHECK(h.Skills().SkillCount() == 0);
    CHECK(h.Skills().IndexOf("slash") == -1);
    CHECK(h.Skills().Skill(0).id.empty());
    CHECK(h.Skills().Class().skills.empty());
    CHECK(h.Skills().LevelsForSave().empty());
    CHECK(h.Skills().HotbarForSave()[0].empty());
  }

  TEST_CASE("BuffList: tagged refresh, sums, pruning by duration") {
    BuffList b;
    b.Add({BuffStat::DamageReduction, 0.2, 1000, 0, BuffTag::None, kNoEntity});
    b.Add({BuffStat::DamageReduction, 0.1, 3000, 0, BuffTag::None, kNoEntity});
    b.RefreshTagged({BuffStat::DamageAmplify, 0.15, 2000, 0, BuffTag::CurseAura, 7});
    b.RefreshTagged({BuffStat::DamageAmplify, 0.15, 2000, 500, BuffTag::CurseAura, 7});
    CHECK(b.Items().size() == 3);
    CHECK(b.RawSum(BuffStat::DamageReduction) == doctest::Approx(0.3));
    REQUIRE(b.FindTag(BuffTag::CurseAura) != nullptr);
    CHECK(b.FindTag(BuffTag::CurseAura)->startMs == 500);
    CHECK(b.Prune(1000) == 1);  // now - start >= duration
    CHECK(b.RawSum(BuffStat::DamageReduction) == doctest::Approx(0.1));
    CHECK(b.RemoveTag(BuffTag::CurseAura) == 1);
    CHECK(b.Has(BuffStat::DamageReduction));
  }
}
