// Combat area (hero+combat owner): damage, statuses, hit feedback, input, projectiles, CombatSystem, soul echo.
// Spec vectors: combat-feel.md section 24, classes-stats-skills.md 12.3. Foundation smoke tests only.
#include "SimHarness.h"
#include "abyss/combat/Combat.h"
#include "abyss/combat/Damage.h"
#include "abyss/combat/HitFeedback.h"
#include "abyss/combat/Projectiles.h"
#include "abyss/combat/StatusEffects.h"
#include "abyss/hero/Hero.h"
#include "doctest/doctest.h"

using namespace abyss;

TEST_SUITE("combat") {
  TEST_CASE("status effect storage: masks, clear entity") {
    test::ScopedAssertCounter asserts;
    StatusEffectSystem s(test::RealData().Classes().statusRules);
    CHECK_FALSE(s.Has(kHeroEntityId, StatusType::Burn));
    CHECK(s.StatusMask(kHeroEntityId) == 0u);
    CHECK(s.EffectsOf(42).empty());
    s.ClearEntity(42);
    s.ClearAll();
    CHECK(asserts.Count() == 0);
  }

  TEST_CASE("StatusEffectSystem::Remove removes one type and reports it (I4 antidote)") {
    StatusEffectSystem s(test::RealData().Classes().statusRules);
    CHECK_FALSE(s.Remove(kHeroEntityId, StatusType::Poison));  // nothing tracked
  }

  TEST_CASE("SummarizeCombatant: the character panel's numbers from the damage formula terms (Q19)") {
    const DataStore& data = test::RealData();
    const DamageRules rules{&data.Classes().formulas, &data.Classes().buffCaps, &data.Classes().skillRules};
    Combatant c;
    c.stats = PrimaryStats{12, 10, 10, 5, 5, 6};
    c.baseDamage = 20;
    EquipStats eq;
    eq.Ref(Stat::Dex) = 5;
    eq.Ref(Stat::CritRate) = 2;
    eq.Ref(Stat::CritDamage) = 20;
    c.eq = &eq;
    const CombatSummary s = SummarizeCombatant(rules, c);
    CHECK(s.critChancePercent == doctest::Approx(15 * 0.2 + 6 * 0.5 + 2));  // 8
    CHECK(s.critMultiplier == doctest::Approx(1.5 + 6 * 0.01 + 0.2));
    CHECK(s.dodgeChancePercent == doctest::Approx(15 * 0.3));
    // basic attack, no defense: floor((20 + 12 * 0.5) * 1) = 26; crit: floor(26 * 1.76) = 45
    CHECK(s.damageMin == 26);
    CHECK(s.damageMax == 45);
    c.stats.dex = 200;  // caps: crit 75 %, dodge 30 %
    c.stats.lck = 200;
    const CombatSummary capped = SummarizeCombatant(rules, c);
    CHECK(capped.critChancePercent == 75);
    CHECK(capped.dodgeChancePercent == 30);
  }

  TEST_CASE("EvHit carries the resolved feel decisions (defaults: primary number, no stop / burst)") {
    const EvHit hit;
    CHECK(hit.attackerStopMs == 0);
    CHECK_FALSE(hit.impactBurst);
    CHECK_FALSE(hit.melee);
    CHECK(hit.numberSlot == HitNumberSlot::Primary);
  }

  TEST_CASE("hit profiles come from hit_feedback.json (combat 11.0b)") {
    const HitFeedbackTable& t = test::RealData().Combat().hitFeedback;
    const ShakeRequest normal = ProfileShake(t, HitWeight::Normal);
    CHECK(normal.durationMs == 60);
    CHECK(normal.intensity == doctest::Approx(0.0018));
    CHECK(ProfileShake(t, HitWeight::Tick).Empty());
    CHECK(HeroRig(ClassId::Mage) == AnimRig::Mage);
  }

  TEST_CASE("CombatSystem and ProjectileSystem construct on a harness") {
    test::SimHarness h;
    Hero hero(h.ctx.data, ClassId::Warrior);
    h.ctx.sys.hero = &hero;
    CombatSystem combat(h.ctx);
    ProjectileSystem projectiles(h.ctx);
    CHECK(combat.AttackTarget() == kNoEntity);
    CHECK_FALSE(combat.InCombat());
    CHECK(projectiles.Projectiles().empty());
    combat.SetAutoCombat(true);
    CHECK(hero.autoCombat);
  }
}
