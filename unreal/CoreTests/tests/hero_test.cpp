// Hero area: hero state (derived stats, leveling, regen, resources, save mapping), skills (scaling, progression gates,
// C3 hotbar), Spirit / Resonance, buffs, RewardService, and the player / settings / soulEcho save sections.
// Spec: classes-stats-skills.md (sections 2-8, 11, 14, 18, 19; unit-test checklist 22 items 1-7 and 10),
// save-ui-input.md 3.2-3.5 / 5.1.1. Ported web suites: SkillProgressionSystem.test.ts, SpiritSystem.test.ts,
// SpiritSaveMigration.test.ts (spirit clamps), skill-combat-integration.test.ts (tiered scaling, synergy),
// GemSocketing.test.ts (stat flow into recalcDerived), NumericalBalance.test.ts (exp curve).
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "SimHarness.h"
#include "TestUtil.h"
#include "abyss/base/Json.h"
#include "abyss/base/SimClock.h"
#include "abyss/combat/Combat.h"
#include "abyss/hero/Buffs.h"
#include "abyss/hero/Hero.h"
#include "abyss/hero/Rewards.h"
#include "abyss/hero/Skills.h"
#include "abyss/hero/Spirit.h"
#include "abyss/items/Inventory.h"
#include "abyss/save/SaveData.h"
#include "abyss/save/SaveIO.h"
#include "abyss/sim/GameSim.h"
#include "abyss/story/StoryDirector.h"
#include "abyss/world/Zone.h"
#include "doctest/doctest.h"

// The JSON section readers / writers are internal to the core (not exported): test them only when the core is linked
// statically (ABYSS_SHARED=1 builds the core as a hidden-visibility shared library).
#if !(defined(ABYSS_CORE_DLL) && ABYSS_CORE_DLL)
#define ABYSS_HERO_TEST_SAVE_SECTIONS 1
#include "../../Source/AbyssCore/Private/save/SaveSections.h"
#else
#define ABYSS_HERO_TEST_SAVE_SECTIONS 0
#endif

using namespace abyss;

namespace {

const SkillRules& Rules() { return test::RealData().Classes().skillRules; }

const SkillDef& SkillById(const char* id) {
  const SkillDef* s = test::RealData().Classes().FindSkill(id);
  REQUIRE_MESSAGE(s != nullptr, id);
  return *s;
}

// The web's tieredScale loop verbatim (CombatSystem.ts:129-138), for exact comparisons.
double WebTieredScale(double per, int32_t level) {
  if (level <= 1) return 0;
  double total = 0;
  for (int32_t i = 2; i <= level; ++i) {
    if (i <= 8) {
      total += per;
    } else if (i <= 16) {
      total += per * 0.75;
    } else {
      total += per * 0.5;
    }
  }
  return total;
}

// Levels a hero up to `level` with exact exp grants (no gear).
void LevelTo(Hero& h, int32_t level) {
  while (h.Level() < level) h.AddExp(h.ExpToNext() - h.Exp(), EquipStats{});
}

std::vector<std::string> HotbarIds(const SkillBook& b) {
  std::vector<std::string> out;
  for (int32_t i = 0; i < SkillBook::kHotbarSlots; ++i) {
    const int32_t s = b.HotbarSkill(i);
    out.push_back(s >= 0 ? b.Skill(s).id : std::string("-"));
  }
  return out;
}

int32_t Idx(const Hero& h, const char* id) {
  const int32_t i = h.Skills().IndexOf(id);
  REQUIRE_MESSAGE(i >= 0, id);
  return i;
}

// A GameSim on a new game with the prologue / chapter card played out (they freeze the world, S2), one step in.
std::unique_ptr<GameSim> LiveHeroSim(ClassId cls, uint64_t seed) {
  auto sim = GameSim::Create(test::RealData(), SimConfig{});
  REQUIRE(sim != nullptr);
  REQUIRE(sim->NewGame(cls, Difficulty::Normal, seed));
  sim->Context().sys.story->FinishAllBeats();
  sim->Step();
  REQUIRE_FALSE(sim->WorldFrozen());
  return sim;
}

// classes-stats-skills.md 10.4, generated from the spec tables (no CDR, no Resonance). -1 = not shown for that skill.
struct SkillTableRow {
  const char* id;
  int32_t level;
  double mult;
  int32_t mana;
  double cooldownMs;
  double radius;
  double buffValue;
  double buffDurationMs;
};
const SkillTableRow kSkillTable[] = {
    {"slash", 1, 1.5, 8, 2000, -1, -1, -1},
    {"slash", 5, 2.22, 10, 2000, -1, -1, -1},
    {"slash", 10, 3.03, 12, 2000, -1, -1, -1},
    {"slash", 20, 4.2, 15, 2000, -1, -1, -1},
    {"whirlwind", 1, 1.2, 15, 4000, 2.5, -1, -1},
    {"whirlwind", 5, 1.76, 19, 3800, 2.82, -1, -1},
    {"whirlwind", 10, 2.39, 23, 3575, 3.18, -1, -1},
    {"whirlwind", 20, 3.3, 30, 3250, 3.7, -1, -1},
    {"war_stomp", 1, 1.8, 22, 8000, 2, -1, -1},
    {"war_stomp", 5, 2.6, 28, 7680, 2.2, -1, -1},
    {"war_stomp", 10, 3.5, 34, 7320, 2.425, -1, -1},
    {"war_stomp", 20, 4.8, 44, 6800, 2.75, -1, -1},
    {"shield_wall", 1, -1, 12, 10000, -1, 0.5, 5000},
    {"shield_wall", 5, -1, 14, 9600, -1, 0.56, 5600},
    {"shield_wall", 10, -1, 16, 9150, -1, 0.628, 6275},
    {"shield_wall", 20, -1, 19, 8500, -1, 0.725, 7250},
    {"taunt_roar", 1, -1, 14, 12000, 3, 0.3, 4000},
    {"taunt_roar", 5, -1, 16, 11520, 3.24, 0.38, 4800},
    {"taunt_roar", 10, -1, 18, 10980, 3.51, 0.47, 5700},
    {"taunt_roar", 20, -1, 21, 10200, 3.9, 0.6, 7000},
    {"vengeful_wrath", 1, -1, 20, 20000, -1, 0.25, 6000},
    {"vengeful_wrath", 5, -1, 24, 19200, -1, 0.33, 7000},
    {"vengeful_wrath", 10, -1, 28, 18300, -1, 0.42, 8125},
    {"vengeful_wrath", 20, -1, 35, 17000, -1, 0.55, 9750},
    {"charge", 1, 2, 14, 6000, -1, -1, -1},
    {"charge", 5, 2.72, 17, 5760, -1, -1, -1},
    {"charge", 10, 3.53, 20, 5490, -1, -1, -1},
    {"charge", 20, 4.7, 26, 5100, -1, -1, -1},
    {"lethal_strike", 1, 2.8, 22, 8000, -1, -1, -1},
    {"lethal_strike", 5, 3.68, 26, 7680, -1, -1, -1},
    {"lethal_strike", 10, 4.67, 32, 7320, -1, -1, -1},
    {"lethal_strike", 20, 6.1, 40, 6800, -1, -1, -1},
    {"dual_wield_mastery", 1, -1, 0, 500, -1, -1, -1},
    {"dual_wield_mastery", 5, -1, 0, 500, -1, -1, -1},
    {"dual_wield_mastery", 10, -1, 0, 500, -1, -1, -1},
    {"dual_wield_mastery", 20, -1, 0, 500, -1, -1, -1},
    {"iron_fortress", 1, -1, 16, 14000, -1, 0.4, 6000},
    {"iron_fortress", 5, -1, 19, 13520, -1, 0.472, 6800},
    {"iron_fortress", 10, -1, 22, 12980, -1, 0.553, 7700},
    {"iron_fortress", 20, -1, 28, 12200, -1, 0.67, 9000},
    {"unyielding", 1, -1, 0, 60000, -1, 0.35, 5000},
    {"unyielding", 5, -1, 0, 58000, -1, 0.41, 5600},
    {"unyielding", 10, -1, 0, 55750, -1, 0.478, 6275},
    {"unyielding", 20, -1, 0, 52500, -1, 0.575, 7250},
    {"life_regen", 1, -1, 0, 500, -1, -1, -1},
    {"life_regen", 5, -1, 0, 500, -1, -1, -1},
    {"life_regen", 10, -1, 0, 500, -1, -1, -1},
    {"life_regen", 20, -1, 0, 500, -1, -1, -1},
    {"frenzy", 1, -1, 18, 15000, -1, 0.2, 8000},
    {"frenzy", 5, -1, 22, 14400, -1, 0.272, 9000},
    {"frenzy", 10, -1, 26, 13725, -1, 0.353, 10125},
    {"frenzy", 20, -1, 33, 12750, -1, 0.47, 11750},
    {"bleed_strike", 1, 1.8, 16, 5000, -1, -1, -1},
    {"bleed_strike", 5, 2.44, 19, 4800, -1, -1, -1},
    {"bleed_strike", 10, 3.16, 22, 4575, -1, -1, -1},
    {"bleed_strike", 20, 4.2, 28, 4250, -1, -1, -1},
    {"rampage", 1, 2.2, 24, 10000, 2.5, -1, -1},
    {"rampage", 5, 3, 28, 9600, 2.74, -1, -1},
    {"rampage", 10, 3.9, 34, 9150, 3.01, -1, -1},
    {"rampage", 20, 5.2, 42, 8500, 3.4, -1, -1},
    {"fireball", 1, 1.8, 10, 2000, -1, -1, -1},
    {"fireball", 5, 2.6, 13, 2000, -1, -1, -1},
    {"fireball", 10, 3.5, 16, 2000, -1, -1, -1},
    {"fireball", 20, 4.8, 22, 2000, -1, -1, -1},
    {"meteor", 1, 2.5, 35, 8000, 2.5, -1, -1},
    {"meteor", 5, 3.62, 43, 7600, 2.74, -1, -1},
    {"meteor", 10, 4.88, 52, 7150, 3.01, -1, -1},
    {"meteor", 20, 6.7, 65, 6500, 3.4, -1, -1},
    {"blizzard", 1, 1, 20, 5000, 3, -1, -1},
    {"blizzard", 5, 1.64, 24, 4760, 3.32, -1, -1},
    {"blizzard", 10, 2.36, 30, 4490, 3.68, -1, -1},
    {"blizzard", 20, 3.4, 38, 4100, 4.2, -1, -1},
    {"ice_armor", 1, -1, 12, 15000, -1, 0.2, 10000},
    {"ice_armor", 5, -1, 14, 14400, -1, 0.248, 11200},
    {"ice_armor", 10, -1, 16, 13725, -1, 0.302, 12550},
    {"ice_armor", 20, -1, 19, 12750, -1, 0.38, 14500},
    {"chain_lightning", 1, 1.3, 18, 3500, 4, -1, -1},
    {"chain_lightning", 5, 1.94, 22, 3340, 4.4, -1, -1},
    {"chain_lightning", 10, 2.66, 26, 3160, 4.85, -1, -1},
    {"chain_lightning", 20, 3.7, 33, 2900, 5.5, -1, -1},
    {"mana_shield", 1, -1, 15, 12000, -1, 0.3, 8000},
    {"mana_shield", 5, -1, 18, 11520, -1, 0.36, 9000},
    {"mana_shield", 10, -1, 21, 10980, -1, 0.428, 10125},
    {"mana_shield", 20, -1, 27, 10200, -1, 0.525, 11750},
    {"fire_wall", 1, 1.2, 18, 6000, 2, -1, -1},
    {"fire_wall", 5, 1.76, 22, 5760, 2.24, -1, -1},
    {"fire_wall", 10, 2.39, 26, 5490, 2.51, -1, -1},
    {"fire_wall", 20, 3.3, 33, 5100, 2.9, -1, -1},
    {"combustion", 1, 2, 24, 7000, -1, -1, -1},
    {"combustion", 5, 2.88, 29, 6720, -1, -1, -1},
    {"combustion", 10, 3.87, 35, 6405, -1, -1, -1},
    {"combustion", 20, 5.3, 45, 5950, -1, -1, -1},
    {"ice_arrow", 1, 1.6, 10, 2500, -1, -1, -1},
    {"ice_arrow", 5, 2.24, 12, 2380, -1, -1, -1},
    {"ice_arrow", 10, 2.96, 15, 2245, -1, -1, -1},
    {"ice_arrow", 20, 4, 19, 2050, -1, -1, -1},
    {"freeze", 1, 0.8, 22, 10000, -1, -1, -1},
    {"freeze", 5, 1.2, 26, 9600, -1, -1, -1},
    {"freeze", 10, 1.65, 32, 9150, -1, -1, -1},
    {"freeze", 20, 2.3, 40, 8500, -1, -1, -1},
    {"teleport", 1, -1, 16, 8000, -1, -1, -1},
    {"teleport", 5, -1, 18, 7680, -1, -1, -1},
    {"teleport", 10, -1, 20, 7320, -1, -1, -1},
    {"teleport", 20, -1, 23, 6800, -1, -1, -1},
    {"arcane_torrent", 1, 1.8, 28, 8000, 2.5, -1, -1},
    {"arcane_torrent", 5, 2.6, 34, 7680, 2.74, -1, -1},
    {"arcane_torrent", 10, 3.5, 40, 7320, 3.01, -1, -1},
    {"arcane_torrent", 20, 4.8, 50, 6800, 3.4, -1, -1},
    {"backstab", 1, 2, 10, 2500, -1, -1, -1},
    {"backstab", 5, 2.88, 12, 2500, -1, -1, -1},
    {"backstab", 10, 3.87, 14, 2500, -1, -1, -1},
    {"backstab", 20, 5.3, 17, 2500, -1, -1, -1},
    {"poison_blade", 1, -1, 12, 8000, -1, 0.5, 6000},
    {"poison_blade", 5, -1, 14, 7680, -1, 0.62, 6800},
    {"poison_blade", 10, -1, 16, 7320, -1, 0.755, 7700},
    {"poison_blade", 20, -1, 19, 6800, -1, 0.95, 9000},
    {"vanish", 1, -1, 20, 15000, -1, 1, 3000},
    {"vanish", 5, -1, 23, 14200, -1, 1.2, 3400},
    {"vanish", 10, -1, 26, 13300, -1, 1.425, 3850},
    {"vanish", 20, -1, 32, 12000, -1, 1.75, 4500},
    {"multishot", 1, 0.8, 15, 3000, 3, -1, -1},
    {"multishot", 5, 1.28, 19, 2880, 3.32, -1, -1},
    {"multishot", 10, 1.82, 23, 2745, 3.68, -1, -1},
    {"multishot", 20, 2.6, 30, 2550, 4.2, -1, -1},
    {"arrow_rain", 1, 0.6, 25, 7000, 3.5, -1, -1},
    {"arrow_rain", 5, 1, 31, 6680, 3.9, -1, -1},
    {"arrow_rain", 10, 1.45, 37, 6320, 4.35, -1, -1},
    {"arrow_rain", 20, 2.1, 47, 5800, 5, -1, -1},
    {"shadow_step", 1, -1, 18, 12000, -1, 0.3, 4000},
    {"shadow_step", 5, -1, 21, 11520, -1, 0.38, 4400},
    {"shadow_step", 10, -1, 24, 10980, -1, 0.47, 4850},
    {"shadow_step", 20, -1, 30, 10200, -1, 0.6, 5500},
    {"death_mark", 1, 0.5, 16, 10000, -1, 0.25, 8000},
    {"death_mark", 5, 0.74, 19, 9600, -1, 0.33, 8800},
    {"death_mark", 10, 1.01, 22, 9150, -1, 0.42, 9700},
    {"death_mark", 20, 1.4, 28, 8500, -1, 0.55, 11000},
    {"piercing_arrow", 1, 1.8, 14, 4000, 1.5, -1, -1},
    {"piercing_arrow", 5, 2.44, 17, 3840, 1.5, -1, -1},
    {"piercing_arrow", 10, 3.16, 20, 3660, 1.5, -1, -1},
    {"piercing_arrow", 20, 4.2, 26, 3400, 1.5, -1, -1},
    {"poison_arrow", 1, 1.2, 12, 3500, -1, -1, -1},
    {"poison_arrow", 5, 1.76, 14, 3360, -1, -1, -1},
    {"poison_arrow", 10, 2.39, 17, 3202, -1, -1, -1},
    {"poison_arrow", 20, 3.3, 21, 2975, -1, -1, -1},
    {"explosive_trap", 1, 1.5, 14, 5000, 2, -1, -1},
    {"explosive_trap", 5, 2.14, 17, 4800, 2.24, -1, -1},
    {"explosive_trap", 10, 2.86, 20, 4575, 2.51, -1, -1},
    {"explosive_trap", 20, 3.9, 26, 4250, 2.9, -1, -1},
    {"poison_cloud", 1, 1, 18, 7000, 2.5, -1, -1},
    {"poison_cloud", 5, 1.48, 22, 6760, 2.74, -1, -1},
    {"poison_cloud", 10, 2.02, 26, 6490, 3.01, -1, -1},
    {"poison_cloud", 20, 2.8, 33, 6100, 3.4, -1, -1},
    {"slow_trap", 1, 0.6, 14, 8000, 2, 0.4, 5000},
    {"slow_trap", 5, 0.92, 16, 7680, 2.2, 0.48, 5600},
    {"slow_trap", 10, 1.28, 19, 7320, 2.425, 0.57, 6275},
    {"slow_trap", 20, 1.8, 23, 6800, 2.75, 0.7, 7250},
    {"chain_trap", 1, 1.4, 22, 9000, 3, -1, -1},
    {"chain_trap", 5, 1.96, 26, 8680, 3.24, -1, -1},
    {"chain_trap", 10, 2.59, 32, 8320, 3.51, -1, -1},
    {"chain_trap", 20, 3.5, 40, 7800, 3.9, -1, -1},
};

}  // namespace

TEST_SUITE("hero") {
  // ===================================================================================================================
  // Construction, foundation behaviour
  // ===================================================================================================================
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
      // Player constructor: derived without gear and full HP / MP.
      CHECK(h.Hp() == h.MaxHp());
      CHECK(h.Mana() == h.MaxMana());
      CHECK(h.Life() == HeroLife::Alive);
      CHECK_FALSE(h.IsDead());
    }
  }

  TEST_CASE("gold spending never goes negative") {
    Hero h(test::RealData(), ClassId::Rogue);
    h.AddGold(50);
    CHECK_FALSE(h.SpendGold(60));
    CHECK(h.Gold() == 50);
    CHECK(h.SpendGold(20));
    CHECK(h.Gold() == 30);
    CHECK_FALSE(h.SpendGold(-1));
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
    // Spirit: unknown class -> warrior profile (SpiritSystem.ts:77-79), never an out-of-range read.
    CHECK(h.GetSpirit().Profile().id == "emberheart");
    h.Skills().InitStarterLevels();
    h.Skills().FillHotbarDefault();
    CHECK(h.Skills().HotbarSkill(0) == -1);
    h.TickRegen(1000, {});
    h.RecalcDerived(EquipStats{});
    CHECK(h.MaxHp() > 0);
  }

  // ===================================================================================================================
  // Leveling (5)
  // ===================================================================================================================
  TEST_CASE("exp curve: expToNext(L) = floor(3 L^2 + 25 L) and the cumulative table (5.1)") {
    const int64_t toNext[] = {28, 62, 102, 148, 200, 258, 322, 392, 468, 550,
                              638, 732, 832, 938, 1050, 1168, 1292, 1422, 1558, 1700};
    const int64_t cumulative[] = {0, 28, 90, 192, 340, 540, 798, 1120, 1512, 1980,
                                  2530, 3168, 3900, 4732, 5670, 6720, 7888, 9180, 10602, 12160};
    const HeroFormulas& f = test::RealData().Classes().formulas;
    int64_t total = 0;
    for (int32_t l = 1; l <= 20; ++l) {
      CHECK(f.ExpToNext(l) == toNext[l - 1]);
      CHECK(total == cumulative[l - 1]);
      total += f.ExpToNext(l);
      if (l > 1) CHECK(f.ExpToNext(l) > f.ExpToNext(l - 1));  // NumericalBalance: strictly increasing
    }
    CHECK(f.levelCap == 0);  // no level cap anywhere
  }

  TEST_CASE("addExp: one level per call, the overflow converts on the next call (checklist 6)") {
    Hero h(test::RealData(), ClassId::Warrior);
    LevelUpResult r = h.AddExp(500, EquipStats{});
    CHECK(r.leveledUp);
    CHECK(r.level == 2);
    CHECK(h.Level() == 2);
    CHECK(h.Exp() == 472);
    CHECK(h.FreeStatPoints() == 5);
    CHECK(h.FreeSkillPoints() == 1);
    r = h.AddExp(0, EquipStats{});
    CHECK(r.leveledUp);
    CHECK(h.Level() == 3);
    CHECK(h.Exp() == 410);
    CHECK(h.FreeStatPoints() == 10);
    CHECK(h.FreeSkillPoints() == 2);
    r = h.AddExp(0, EquipStats{});
    CHECK(r.leveledUp);
    CHECK(h.Level() == 4);
    CHECK(h.Exp() == 308);
    r = h.AddExp(0, EquipStats{});
    CHECK(r.leveledUp);
    CHECK(h.Level() == 5);
    CHECK(h.Exp() == 160);
    r = h.AddExp(0, EquipStats{});
    CHECK_FALSE(r.leveledUp);
    CHECK(h.Level() == 5);
    CHECK(h.Exp() == 160);
    // Exactly the requirement levels up with 0 left; negative grants add nothing.
    Hero g(test::RealData(), ClassId::Mage);
    CHECK(g.AddExp(27, EquipStats{}).leveledUp == false);
    CHECK(g.AddExp(-50, EquipStats{}).leveledUp == false);
    CHECK(g.Exp() == 27);
    CHECK(g.AddExp(1, EquipStats{}).leveledUp);
    CHECK(g.Exp() == 0);
    // statGrowth is never applied (Q1 / C2): primaries change only by allocation.
    CHECK(g.BaseStats() == test::RealData().Classes().Find(ClassId::Mage)->baseStats);
  }

  TEST_CASE("level-up refill uses gear-inclusive maxima (C1, FIX Q2); a Dying hero is not refilled (5.1.1)") {
    EquipStats eq;
    eq.Ref(Stat::MaxHp) = 40;
    eq.Ref(Stat::Vit) = 2;
    eq.Ref(Stat::MaxMana) = 15;
    Hero h(test::RealData(), ClassId::Warrior);
    h.RecalcDerived(eq);
    h.SetHp(10);
    h.SetMana(3);
    h.AddExp(28, eq);
    CHECK(h.Level() == 2);
    // L2 warrior: hp = 50 + 12*10 + 15 + 40 = 225; mp = 30 + 40 + 15 + 8 + 15 = 108.
    CHECK(h.MaxHp() == 225);
    CHECK(h.MaxMana() == 108);
    CHECK(h.Hp() == 225);
    CHECK(h.Mana() == 108);

    Hero d(test::RealData(), ClassId::Rogue);
    d.SetHp(0);
    d.SetLife(HeroLife::Dying);
    const LevelUpResult r = d.AddExp(30, EquipStats{});
    CHECK(r.leveledUp);
    CHECK(d.Level() == 2);
    CHECK(d.Exp() == 2);
    CHECK(d.FreeStatPoints() == 5);
    CHECK(d.Hp() == 0);  // the respawn refills, not the level-up
    CHECK(d.IsDead());
  }

  TEST_CASE("RemoveExp (death toll) stays inside the current level") {
    Hero h(test::RealData(), ClassId::Warrior);
    h.AddExp(40, EquipStats{});  // L2 with 12
    h.RemoveExp(5);
    CHECK(h.Exp() == 7);
    h.RemoveExp(100);
    CHECK(h.Exp() == 0);
    CHECK(h.Level() == 2);  // never de-levels
    h.RemoveExp(-5);
    CHECK(h.Exp() == 0);
  }

  // ===================================================================================================================
  // Derived stats (3), stat points (6), regen (4)
  // ===================================================================================================================
  TEST_CASE("derived values with base stats at L1 and L10 (2.4, checklist 7)") {
    struct Row {
      ClassId cls;
      int32_t level;
      double maxHp, maxMana, baseDamage, defense, dodge, crit, critMul, hpPerSec, mpPerSec, basic;
    };
    const Row rows[] = {
        {ClassId::Warrior, 1, 150, 85, 19.6, 9, 2.4, 4.1, 1.55, 1.0, 1.5, 25.6},
        {ClassId::Warrior, 10, 285, 157, 37.6, 18, 2.4, 4.1, 1.55, 1.0, 1.5, 43.6},
        {ClassId::Mage, 1, 110, 152, 13.2, 7, 1.8, 3.7, 1.55, 0.8, 2.0, 15.2},
        {ClassId::Mage, 10, 245, 224, 31.2, 16, 1.8, 3.7, 1.55, 0.8, 2.0, 33.2},
        {ClassId::Rogue, 1, 110, 85, 15.6, 7, 4.2, 6.8, 1.58, 0.8, 1.5, 19.1},
        {ClassId::Rogue, 10, 245, 157, 33.6, 16, 4.2, 6.8, 1.58, 0.8, 1.5, 37.1},
    };
    const HeroFormulas& f = test::RealData().Classes().formulas;
    for (const Row& row : rows) {
      CAPTURE(EnumName(row.cls));
      CAPTURE(row.level);
      Hero h(test::RealData(), row.cls);
      LevelTo(h, row.level);
      h.RecalcDerived(EquipStats{});
      const PrimaryStats& s = h.BaseStats();
      CHECK(h.MaxHp() == row.maxHp);
      CHECK(h.MaxMana() == row.maxMana);
      CHECK(h.Derived().baseDamage == doctest::Approx(row.baseDamage).epsilon(1e-12));
      CHECK(h.Derived().defense == row.defense);
      CHECK(h.Derived().moveSpeed == 120);
      CHECK(h.Derived().attackSpeedMs == 1000);
      CHECK(h.Derived().attackRange == 1.5);
      CHECK(s.dex * f.dodgePerDex == doctest::Approx(row.dodge));
      CHECK(s.dex * f.critPerDex + s.lck * f.critPerLck == doctest::Approx(row.crit));
      CHECK(f.critMultiplierBase + s.lck * f.critMultiplierPerLck == doctest::Approx(row.critMul));
      CHECK(h.Derived().baseDamage + s.str * f.statToDamage == doctest::Approx(row.basic).epsilon(1e-12));
      // Regen per second through TickRegen (no starter levels: no Life Regen passive).
      h.SetHp(10);
      h.SetMana(0);
      h.TickRegen(1000, {});
      CHECK(h.Hp() == doctest::Approx(10 + row.hpPerSec));
      CHECK(h.Mana() == doctest::Approx(row.mpPerSec));
    }
  }

  TEST_CASE("recalcDerived with gear: percent floors, move / attack speed, Resonance move speed, no hp clamp (3)") {
    Hero h(test::RealData(), ClassId::Warrior);
    EquipStats eq;
    eq.Ref(Stat::MaxHp) = 20;
    eq.Ref(Stat::MaxHpPercent) = 10;
    eq.Ref(Stat::MaxMana) = 5;
    eq.Ref(Stat::MaxManaPercent) = 7;
    eq.Ref(Stat::MoveSpeed) = 10;
    eq.Ref(Stat::AttackSpeed) = 25;
    eq.Ref(Stat::Defense) = 30;  // not part of derived defense (added inside the damage formula)
    h.RecalcDerived(eq);
    CHECK(h.MaxHp() == std::floor((150.0 + 20) * (1 + 10.0 / 100)));  // 187
    CHECK(h.MaxHp() == 187);
    CHECK(h.MaxMana() == std::floor((85.0 + 5) * (1 + 7.0 / 100)));  // floor(96.3) = 96
    CHECK(h.Derived().moveSpeed == 132);
    CHECK(h.Derived().attackSpeedMs == 750);
    CHECK(h.Derived().defense == 9);
    CHECK(&h.EquipStatsUsed() != &eq);
    CHECK(h.EquipStatsUsed() == eq);
    eq.Ref(Stat::AttackSpeed) = 90;
    h.RecalcDerived(eq);
    CHECK(h.Derived().attackSpeedMs == 200);  // min interval
    // Resonance: moveSpeed = floor(spd * 1.12) (warrior), ground speed = moveSpeed / 36 (S5).
    h.RecalcDerived(EquipStats{});
    CHECK(h.GroundSpeedTilesPerSec() == doctest::Approx(120.0 / 36.0));
    h.GetSpirit().Gain(100);
    REQUIRE(h.GetSpirit().IsResonating());
    h.RecalcDerived(EquipStats{});
    CHECK(h.Derived().moveSpeed == 134);  // floor(134.4)
    CHECK(h.GroundSpeedTilesPerSec() == doctest::Approx(134.0 / 36.0));
    // recalcDerived does not clamp: unequipping leaves hp above the new max until damage.
    Hero g(test::RealData(), ClassId::Mage);
    EquipStats big;
    big.Ref(Stat::MaxHp) = 100;
    g.RecalcDerived(big);
    g.FillHpMana();
    g.RecalcDerived(EquipStats{});
    CHECK(g.MaxHp() == 110);
    CHECK(g.Hp() == 210);
    g.TickLifeRegen(1000, {});
    g.TickRegen(1000, {});
    CHECK(g.Hp() == 210);  // regen only runs below the max: it never lowers it
    // ... but every web heal is `hp = Math.min(maxHp, hp + x)`, so the next heal snaps it down to the max.
    CHECK(g.Heal(10) == 0);
    CHECK(g.Hp() == 110);
  }

  TEST_CASE("gear primaries flow into recalcDerived (GemSocketing stat flow)") {
    Hero h(test::RealData(), ClassId::Rogue);
    LevelTo(h, 10);
    h.RecalcDerived(EquipStats{});
    const double dmg = h.Derived().baseDamage;
    const double mp = h.MaxMana();
    const double hp = h.MaxHp();
    const double def = h.Derived().defense;
    EquipStats eq;
    eq.Ref(Stat::Str) = 5;
    eq.Ref(Stat::Int) = 12;
    eq.Ref(Stat::Vit) = 3;
    eq.Ref(Stat::Spi) = 1;
    h.RecalcDerived(eq);
    CHECK(h.Derived().baseDamage - dmg == doctest::Approx(4.0));  // 5 * 0.8
    CHECK(h.MaxMana() - mp == 36 + 8);                             // 12 * 3 + 1 * 8
    CHECK(h.MaxHp() - hp == 30);                                    // 3 * 10
    CHECK(h.Derived().defense - def == doctest::Approx(1.5));      // 3 * 0.5
    // The raw primaries (combat stat bonus, regen, spirit) stay unchanged.
    CHECK(h.BaseStats() == test::RealData().Classes().Find(ClassId::Rogue)->baseStats);
    CHECK(h.AsCombatant().stats == h.BaseStats());
  }

  TEST_CASE("stat allocation: one point per call, max changes but current hp / mana do not (6)") {
    Hero h(test::RealData(), ClassId::Warrior);
    CHECK_FALSE(h.AllocateStat(PrimaryStat::Vit));  // no points at level 1
    h.AddExp(28, EquipStats{});
    REQUIRE(h.FreeStatPoints() == 5);
    const double hp = h.Hp();
    const double maxHp = h.MaxHp();
    CHECK(h.AllocateStat(PrimaryStat::Vit));
    CHECK(h.FreeStatPoints() == 4);
    CHECK(h.BaseStats().vit == 11);
    CHECK(h.MaxHp() == maxHp + 10);
    CHECK(h.Hp() == hp);
    CHECK(h.AllocateStat(PrimaryStat::Spi));
    CHECK(h.AllocateStat(PrimaryStat::Lck));
    CHECK(h.AllocateStat(PrimaryStat::Dex));
    CHECK(h.AllocateStat(PrimaryStat::Str));
    CHECK(h.FreeStatPoints() == 0);
    CHECK_FALSE(h.AllocateStat(PrimaryStat::Int));
    CHECK(h.BaseStats().int_ == 5);
    CHECK(h.BaseStats().str == 13);
  }

  TEST_CASE("regen: campfire x50, poison halves HP regen, Life Regen +2/s per level, skipped while dead (4.1, 4.2)") {
    const DataStore& data = test::RealData();
    // Warrior base: hp 1.0/s, mp 1.5/s.
    Hero h(data, ClassId::Warrior);
    h.SetHp(50);
    h.SetMana(10);
    h.TickRegen(500, RegenModifiers{50, 50});
    CHECK(h.Hp() == doctest::Approx(50 + 1.0 * 50 * 0.5));
    CHECK(h.Mana() == doctest::Approx(10 + 1.5 * 50 * 0.5));
    h.SetHp(50);
    h.TickRegen(1000, RegenModifiers{0.5, 1});
    CHECK(h.Hp() == doctest::Approx(50.5));
    // Mana stops at the max.
    h.SetMana(84.9);
    h.TickRegen(1000, {});
    CHECK(h.Mana() == 85);
    // Gear hpRegen / manaRegen add to the per-second rates (merged bag of the last recalc).
    EquipStats eq;
    eq.Ref(Stat::HpRegen) = 2;
    eq.Ref(Stat::ManaRegen) = 1;
    h.RecalcDerived(eq);
    h.SetHp(50);
    h.SetMana(0);
    h.TickRegen(1000, {});
    CHECK(h.Hp() == doctest::Approx(53));
    CHECK(h.Mana() == doctest::Approx(2.5));
    h.RecalcDerived(EquipStats{});
    // Life Regen L3: +6 HP/s, linear, same campfire / poison factor. It is its own step-6 call (TickLifeRegen, before
    // the Unyielding check); the step-7 TickRegen never applies it.
    h.Skills().InitStarterLevels();
    h.Skills().SetLevel(Idx(h, "life_regen"), 3);
    h.SetHp(50);
    h.TickRegen(1000, {});
    CHECK(h.Hp() == doctest::Approx(50 + 1));
    h.SetHp(50);
    h.SetMana(10);
    h.TickLifeRegen(1000, {});
    CHECK(h.Hp() == doctest::Approx(50 + 6));
    CHECK(h.Mana() == 10);  // HP only
    h.TickRegen(1000, {});
    CHECK(h.Hp() == doctest::Approx(50 + 6 + 1));
    h.SetHp(50);
    h.TickLifeRegen(1000, RegenModifiers{0.5, 1});
    h.TickRegen(1000, RegenModifiers{0.5, 1});
    CHECK(h.Hp() == doctest::Approx(50 + 3 + 0.5));
    h.SetHp(50);
    h.TickLifeRegen(100, RegenModifiers{50, 50});  // campfire: 6 x 50 x 0.1
    CHECK(h.Hp() == doctest::Approx(80));
    h.SetHp(149.99);
    h.TickLifeRegen(1000, {});
    CHECK(h.Hp() == 150);  // clamped at the max
    // Exact web expression order for one 60 Hz step: life regen then base regen, each clamped.
    h.SetHp(100);
    const double dt = kSimStepMs;
    double expect = (std::min)(150.0, 100 + 6.0 * dt / 1000 * 1.0);
    expect = (std::min)(150.0, expect + (0.5 + 10 * 0.05 + 0) * 1.0 * dt / 1000);
    h.TickLifeRegen(dt, {});
    h.TickRegen(dt, {});
    CHECK(h.Hp() == expect);
    // Dead or Dying: nothing regenerates.
    h.SetHp(0);
    h.SetMana(0);
    h.TickLifeRegen(1000, {});
    h.TickRegen(1000, {});
    CHECK(h.Hp() == 0);
    CHECK(h.Mana() == 0);
    h.SetHp(20);
    h.SetLife(HeroLife::Dying);
    h.TickLifeRegen(1000, {});
    h.TickRegen(1000, {});
    CHECK(h.Hp() == 20);
    CHECK(h.Mana() == 0);
  }

  TEST_CASE("resources: heals and mana gains are refused unless Alive (5.1.1), clamp at the max") {
    Hero h(test::RealData(), ClassId::Mage);
    h.SetHp(50);
    CHECK(h.Heal(30) == 30);
    CHECK(h.Hp() == 80);
    CHECK(h.Heal(1000) == 30);
    CHECK(h.Hp() == 110);
    CHECK(h.Heal(-5) == 0);
    CHECK(h.Heal(std::numeric_limits<double>::quiet_NaN()) == 0);
    h.SetMana(10);
    CHECK(h.RestoreMana(20) == 20);
    CHECK(h.RestoreMana(1000) == 122);
    CHECK(h.Mana() == 152);
    h.SpendMana(200);
    CHECK(h.Mana() == 0);
    CHECK(h.ApplyDamage(30) == 30);
    CHECK(h.Hp() == 80);
    CHECK(h.ApplyDamage(500) == 80);
    CHECK(h.Hp() == 0);
    CHECK(h.IsDead());
    h.SetLife(HeroLife::Dying);
    CHECK(h.Heal(50) == 0);
    CHECK(h.RestoreMana(50) == 0);
    CHECK(h.Hp() == 0);
    // Death save / pet revive heal BEFORE Dying is entered: an Alive hero at 0 hp can be healed.
    h.SetLife(HeroLife::Alive);
    CHECK(h.Heal(25) == 25);
    CHECK_FALSE(h.IsDead());
  }

  TEST_CASE("heals snap hp / mana above a lowered max down to it, like every web heal site (3, 4.3, 12.1)") {
    // Web: potions, life / mana steal, kill and thorns heals, merc / pet heals and the free-cast refund all assign
    // `Math.min(max, value + x)`; recalcDerived itself never clamps (unequipped +maxHp / +maxMana gear).
    const DataStore& data = test::RealData();
    Hero h(data, ClassId::Warrior);
    EquipStats gear;
    gear.Ref(Stat::MaxHp) = 60;
    gear.Ref(Stat::MaxMana) = 40;
    h.RecalcDerived(gear);
    h.FillHpMana();
    CHECK(h.Hp() == 210);
    CHECK(h.Mana() == 125);
    h.RecalcDerived(EquipStats{});  // unequip
    REQUIRE(h.MaxHp() == 150);
    REQUIRE(h.MaxMana() == 85);
    CHECK(h.Hp() == 210);  // 3: no clamp on recalc
    CHECK(h.Mana() == 125);
    // A positive heal snaps to the max and reports no gain.
    CHECK(h.Heal(50) == 0);
    CHECK(h.Hp() == 150);
    CHECK(h.RestoreMana(30) == 0);
    CHECK(h.Mana() == 85);
    // A zero heal (e.g. floor(maxHp * killHeal% / 100) == 0, a 0-cost free-cast refund) snaps too: min(max, v + 0).
    h.RecalcDerived(gear);
    h.FillHpMana();
    h.RecalcDerived(EquipStats{});
    CHECK(h.Heal(0) == 0);
    CHECK(h.Hp() == 150);
    CHECK(h.RestoreMana(0) == 0);
    CHECK(h.Mana() == 85);
    // Below the max a zero heal changes nothing; negative / non-finite amounts are refused (no web site passes one).
    h.SetHp(100);
    h.SetMana(20);
    CHECK(h.Heal(0) == 0);
    CHECK(h.Hp() == 100);
    CHECK(h.Heal(-10) == 0);
    CHECK(h.Hp() == 100);
    CHECK(h.RestoreMana(-10) == 0);
    CHECK(h.RestoreMana(std::numeric_limits<double>::infinity()) == 0);
    CHECK(h.Mana() == 20);
    // Not Alive: refused, even the snap (5.1.1).
    h.RecalcDerived(gear);
    h.FillHpMana();
    h.RecalcDerived(EquipStats{});
    h.SetLife(HeroLife::Dying);
    CHECK(h.Heal(10) == 0);
    CHECK(h.Hp() == 210);
    CHECK(h.RestoreMana(10) == 0);
    CHECK(h.Mana() == 125);
  }

  TEST_CASE("AsCombatant: raw stats, derived numbers, merged bag, spirit outgoing multiplier (12)") {
    Hero h(test::RealData(), ClassId::Warrior);
    EquipStats eq;
    eq.Ref(Stat::Str) = 4;
    eq.Ref(Stat::CritRate) = 3;
    h.RecalcDerived(eq);
    Combatant c = h.AsCombatant();
    CHECK(c.stats == h.BaseStats());
    CHECK(c.baseDamage == h.Derived().baseDamage);
    CHECK(c.defense == h.Derived().defense);
    CHECK(c.mana == h.Mana());
    CHECK(c.maxHp == h.MaxHp());
    CHECK(c.buffs == &h.Buffs());
    REQUIRE(c.eq != nullptr);
    CHECK(c.eq->Get(Stat::CritRate) == 3);
    CHECK(c.outgoingMultiplier == 1.0);
    CHECK(c.extraCritPercent == 0);
    h.GetSpirit().Gain(1000);
    c = h.AsCombatant();
    CHECK(c.outgoingMultiplier == doctest::Approx(1.3));
  }

  TEST_CASE("cooldowns are absolute SimClock times; a zone change resets them (Q23) but not deathSaveReadyAtMs (Q24)") {
    test::SimHarness sim;
    Hero h(sim.ctx.data, ClassId::Mage);
    h.Skills().InitStarterLevels();
    const int32_t fb = Idx(h, "fireball");
    sim.Step(30);
    const double now = sim.clock.NowMs();
    h.Skills().StartCooldown(fb, now + SkillCooldownMs(Rules(), h.Skills().Skill(fb), 1, 0));
    CHECK_FALSE(h.Skills().IsReady(fb, now));
    CHECK(h.Skills().ReadyAtMs(fb) == now + 2000);
    sim.Step(119);
    CHECK_FALSE(h.Skills().IsReady(fb, sim.clock.NowMs()));
    sim.Step(1);
    CHECK(h.Skills().IsReady(fb, sim.clock.NowMs()));
    h.Skills().StartCooldown(fb, sim.clock.NowMs() + 5000);
    h.deathSaveReadyAtMs = sim.clock.NowMs() + 60000;
    h.Skills().ResetCooldowns();
    CHECK(h.Skills().IsReady(fb, sim.clock.NowMs()));
    CHECK(h.deathSaveReadyAtMs == sim.clock.NowMs() + 60000);
    // A frozen world holds the clock: a buff with 5000 ms left still has 5000 ms after the freeze (D13 T5).
    h.Buffs().Add({BuffStat::DamageReduction, 0.2, 5000, sim.clock.NowMs(), BuffTag::None, kNoEntity});
    sim.clock.SetFrozen(FreezeReason::Cinematic, true);
    CHECK(sim.clock.IsFrozen());  // no Step() while frozen (GameSim never steps a frozen world)
    sim.clock.SetFrozen(FreezeReason::Cinematic, false);
    CHECK(h.Buffs().Items()[0].RemainingMs(sim.clock.NowMs()) == 5000);
  }

  // ===================================================================================================================
  // Skill scaling (8, 10.4)
  // ===================================================================================================================
  TEST_CASE("tieredScale(0.18, L) vectors and exact parity with the web loop (checklist 1)") {
    const int32_t levels[] = {1, 2, 8, 9, 16, 17, 20};
    const double expect[] = {0, 0.18, 1.26, 1.395, 2.34, 2.43, 2.7};
    for (size_t i = 0; i < 7; ++i) {
      CAPTURE(levels[i]);
      CHECK(TieredScale(Rules(), 0.18, levels[i]) == doctest::Approx(expect[i]).epsilon(1e-12));
    }
    CHECK(TieredScale(Rules(), 0.18, 0) == 0);
    CHECK(TieredScale(Rules(), 0.18, -3) == 0);
    // Sum of weights at L20 = 7 + 6 + 2 = 15.
    CHECK(TieredScale(Rules(), 1.0, 20) == 15.0);
    // Bit-identical to the web loop for every per-level value in the data, levels 0..25.
    for (const ClassDef& c : test::RealData().Classes().classes) {
      for (const SkillDef& s : c.skills) {
        const double pers[] = {s.scaling.damagePerLevel,    s.scaling.manaCostPerLevel,
                               s.scaling.cooldownReductionPerLevel, s.scaling.aoeRadiusPerLevel,
                               s.scaling.buffValuePerLevel, s.scaling.buffDurationPerLevel};
        for (double per : pers) {
          for (int32_t l = 0; l <= 25; ++l) CHECK(TieredScale(Rules(), per, l) == WebTieredScale(per, l));
        }
      }
    }
  }

  TEST_CASE("every skill x L1/5/10/20: multiplier, mana, cooldown, radius, buff (10.4, checklist 2)") {
    const ClassTables& t = test::RealData().Classes();
    size_t rows = 0;
    for (const SkillTableRow& row : kSkillTable) {
      CAPTURE(row.id);
      CAPTURE(row.level);
      const SkillDef& s = SkillById(row.id);
      ++rows;
      if (row.mult >= 0) CHECK(std::fabs(SkillDamageMultiplier(t.skillRules, s, row.level) - row.mult) < 6e-4);
      CHECK(SkillManaCost(t.skillRules, s, row.level) == row.mana);
      CHECK(SkillCooldownMs(t.skillRules, s, row.level, 0) == row.cooldownMs);
      if (row.radius >= 0) CHECK(std::fabs(SkillAoeRadius(t.skillRules, s, row.level) - row.radius) < 6e-4);
      if (row.buffValue >= 0) CHECK(std::fabs(SkillBuffValue(t.skillRules, s, row.level) - row.buffValue) < 6e-4);
      if (row.buffDurationMs >= 0) CHECK(SkillBuffDurationMs(t.skillRules, s, row.level) == row.buffDurationMs);
      // Exact web formulas (CombatSystem.ts:141-182).
      CHECK(SkillDamageMultiplier(t.skillRules, s, row.level) ==
            s.damageMultiplier + WebTieredScale(s.scaling.damagePerLevel, row.level));
      CHECK(SkillManaCost(t.skillRules, s, row.level) ==
            static_cast<int32_t>(std::floor(s.manaCost + WebTieredScale(s.scaling.manaCostPerLevel, row.level))));
    }
    // The table covers every skill of every class (40) at 4 levels.
    size_t skills = 0;
    for (const ClassDef& c : t.classes) skills += c.skills.size();
    CHECK(skills == 40);
    CHECK(rows == skills * 4);
    for (const ClassDef& c : t.classes) {
      for (const SkillDef& s : c.skills) {
        size_t found = 0;
        for (const SkillTableRow& row : kSkillTable) found += s.id == row.id ? 1 : 0;
        CHECK_MESSAGE(found == 4, s.id);
        CHECK(s.maxLevel == 20);
      }
    }
  }

  TEST_CASE("castMana in Resonance, cooldown CDR clamp and floor (checklist 3, 4)") {
    const SkillRules& r = Rules();
    // ceil(manaCost(L) * spirit mana-cost multiplier).
    CHECK(SkillCastMana(r, SkillById("slash"), 1, 0.85) == 7);
    CHECK(SkillCastMana(r, SkillById("meteor"), 1, 0.6) == 21);
    CHECK(SkillCastMana(r, SkillById("backstab"), 1, 0.75) == 8);
    CHECK(SkillCastMana(r, SkillById("slash"), 1, 1.0) == 8);
    // Through the Spirit profiles of each class.
    {
      Hero w(test::RealData(), ClassId::Warrior);
      w.GetSpirit().Gain(100);
      CHECK(SkillCastMana(r, SkillById("slash"), 1, w.GetSpirit().ManaCostMultiplier()) == 7);
      Hero m(test::RealData(), ClassId::Mage);
      m.GetSpirit().Gain(100);
      CHECK(SkillCastMana(r, SkillById("meteor"), 1, m.GetSpirit().ManaCostMultiplier()) == 21);
      Hero g(test::RealData(), ClassId::Rogue);
      g.GetSpirit().Gain(100);
      CHECK(SkillCastMana(r, SkillById("backstab"), 1, g.GetSpirit().ManaCostMultiplier()) == 8);
    }
    // CDR clamps to 50 %: floor(2000 * 0.5) = 1000.
    CHECK(SkillCooldownMs(r, SkillById("fireball"), 1, 60) == 1000);
    CHECK(SkillCooldownMs(r, SkillById("fireball"), 1, 50) == 1000);
    CHECK(SkillCooldownMs(r, SkillById("fireball"), 1, 25) == 1500);
    CHECK(SkillCooldownMs(r, SkillById("fireball"), 1, -10) == 2000);
    CHECK(SkillCooldownMs(r, SkillById("ice_arrow"), 10, 10) == std::floor(2245 * 0.9));
    // Passives (cooldown 0) get the 500 ms floor BEFORE CDR.
    CHECK(SkillCooldownMs(r, SkillById("life_regen"), 1, 0) == 500);
    CHECK(SkillCooldownMs(r, SkillById("life_regen"), 1, 50) == 250);
    // skill-combat-integration: tiered growth ratio, cooldown down / mana up with level.
    const SkillDef& charge = SkillById("charge");
    const double g18 = SkillDamageMultiplier(r, charge, 8) - SkillDamageMultiplier(r, charge, 1);
    const double g816 = SkillDamageMultiplier(r, charge, 16) - SkillDamageMultiplier(r, charge, 8);
    CHECK((g816 / 8) / (g18 / 7) == doctest::Approx(0.75));
    CHECK(SkillCooldownMs(r, charge, 10, 0) < SkillCooldownMs(r, charge, 1, 0));
    CHECK(SkillCooldownMs(r, charge, 20, 0) >= 500);
    CHECK(SkillManaCost(r, SkillById("fireball"), 20) > SkillManaCost(r, SkillById("fireball"), 10));
    CHECK(SkillBuffDurationMs(r, SkillById("iron_fortress"), 10) > SkillBuffDurationMs(r, SkillById("iron_fortress"), 1));
    // stunDuration is not level-scaled (Q5): it is plain data.
    CHECK(SkillById("war_stomp").stunDurationMs == 2000);
    CHECK(SkillById("chain_trap").stunDurationMs == 1000);
  }

  TEST_CASE("synergy factor uses the raw source levels (8; skill-combat-integration Synergy Bonuses)") {
    Hero h(test::RealData(), ClassId::Warrior);
    const SkillDef& charge = SkillById("charge");
    CHECK(h.Skills().SynergyFactor(charge) == 1.0);  // nothing learned
    h.Skills().SetLevel(Idx(h, "slash"), 5);
    h.Skills().SetLevel(Idx(h, "lethal_strike"), 3);
    CHECK(h.Skills().SynergyFactor(charge) == doctest::Approx(1.54));
    CHECK(h.Skills().SynergyFactor(charge) == 1 + (0.06 * 5 + 0.08 * 3));
    // 12.3: slash L5 with whirlwind L3 -> 1 + 0.08 * 3 = 1.24.
    h.Skills().SetLevel(Idx(h, "whirlwind"), 3);
    CHECK(h.Skills().SynergyFactor(SkillById("slash")) == doctest::Approx(1.24));
    // No synergies at all -> exactly 1.
    SkillDef bare;
    CHECK(h.Skills().SynergyFactor(bare) == 1.0);
    // A synergy on another class's skill reads 0 here.
    CHECK(h.Skills().SynergyFactor(SkillById("meteor")) == 1.0);
  }

  // ===================================================================================================================
  // Skill progression (7) - SkillProgressionSystem.test.ts + checklist 5
  // ===================================================================================================================
  TEST_CASE("starter levels: tier-one skills only (7.1, 2.3)") {
    Hero w(test::RealData(), ClassId::Warrior);
    w.Skills().InitStarterLevels();
    CHECK(w.Skills().Level("slash") == 1);
    CHECK(w.Skills().Level("shield_wall") == 1);
    CHECK(w.Skills().Level("frenzy") == 1);
    CHECK(w.Skills().Level("life_regen") == 1);
    CHECK(w.Skills().Level("whirlwind") == 0);
    CHECK(w.Skills().Level("war_stomp") == 0);
    CHECK(w.Skills().Level("unknown_skill") == 0);
    int32_t learned = 0;
    for (size_t i = 0; i < w.Skills().SkillCount(); ++i) learned += w.Skills().Level(static_cast<int32_t>(i)) > 0;
    CHECK(learned == 4);
    Hero m(test::RealData(), ClassId::Mage);
    m.Skills().InitStarterLevels();
    for (const char* id : {"fireball", "ice_armor", "mana_shield", "ice_arrow"}) CHECK(m.Skills().Level(id) == 1);
    Hero r(test::RealData(), ClassId::Rogue);
    r.Skills().InitStarterLevels();
    for (const char* id : {"backstab", "multishot", "explosive_trap"}) CHECK(r.Skills().Level(id) == 1);
    CHECK(r.Skills().Level("vanish") == 0);
  }

  TEST_CASE("tree points count every invested rank in a branch, starter levels included (7.2)") {
    Hero h(test::RealData(), ClassId::Warrior);
    h.Skills().SetLevel(Idx(h, "slash"), 4);
    h.Skills().SetLevel(Idx(h, "whirlwind"), 2);
    h.Skills().SetLevel(Idx(h, "shield_wall"), 10);
    CHECK(h.Skills().InvestedTreePoints("combat_master") == 6);
    CHECK(h.Skills().InvestedTreePoints("guardian") == 10);
    CHECK(h.Skills().InvestedTreePoints("berserker") == 0);
    CHECK(h.Skills().InvestedTreePoints("no_such_tree") == 0);
  }

  TEST_CASE("investment gates in the web order (7.3, checklist 5)") {
    Hero h(test::RealData(), ClassId::Warrior);
    SkillBook& b = h.Skills();
    b.InitStarterLevels();
    const int32_t slash = Idx(h, "slash");
    const int32_t whirlwind = Idx(h, "whirlwind");
    const int32_t warStomp = Idx(h, "war_stomp");
    // Ready: a learned tier-one skill with a point.
    InvestState st = b.GetInvestState(slash, 1, 1);
    CHECK(st.canInvest);
    CHECK(st.reason == InvestBlock::Ready);
    CHECK(st.requiredPlayerLevel == 1);
    CHECK(st.requiredTreePoints == 0);
    CHECK(st.investedTreePoints == 1);
    // No points / maxed (maxed is checked first).
    CHECK(b.GetInvestState(slash, 10, 0).reason == InvestBlock::NoPoints);
    b.SetLevel(slash, 20);
    CHECK(b.GetInvestState(slash, 99, 1).reason == InvestBlock::Maxed);
    CHECK(b.GetInvestState(slash, 99, 0).reason == InvestBlock::Maxed);
    b.SetLevel(slash, 1);
    // Tier 2: player level, then tree points.
    CHECK(b.GetInvestState(whirlwind, 5, 5).reason == InvestBlock::PlayerLevel);
    st = b.GetInvestState(whirlwind, 6, 5);
    CHECK(st.reason == InvestBlock::TreePoints);
    CHECK(st.requiredPlayerLevel == 6);
    CHECK(st.requiredTreePoints == 4);
    CHECK(st.investedTreePoints == 1);
    b.SetLevel(slash, 4);
    CHECK(b.GetInvestState(whirlwind, 6, 5).canInvest);
    // Tier 3: needs a learned tier-2 skill of the same tree.
    b.SetLevel(slash, 10);
    st = b.GetInvestState(warStomp, 12, 1);
    CHECK(st.reason == InvestBlock::PreviousTier);
    CHECK(st.requiredPlayerLevel == 12);
    CHECK(st.requiredTreePoints == 9);
    CHECK(b.GetInvestState(warStomp, 11, 1).reason == InvestBlock::PlayerLevel);
    b.SetLevel(whirlwind, 1);
    CHECK(b.GetInvestState(warStomp, 12, 1).canInvest);
    // A tier-2 skill of ANOTHER tree does not satisfy the previous-tier rule.
    b.SetLevel(whirlwind, 0);
    b.SetLevel(Idx(h, "taunt_roar"), 1);
    CHECK(b.GetInvestState(warStomp, 12, 1).reason == InvestBlock::PreviousTier);
    // Unknown index: nothing to invest in.
    CHECK_FALSE(b.GetInvestState(-1, 50, 5).canInvest);
    CHECK_FALSE(b.GetInvestState(999, 50, 5).canInvest);
  }

  TEST_CASE("invest: exactly one rank and one point; blocked leaves everything unchanged (7.3)") {
    Hero h(test::RealData(), ClassId::Warrior);
    SkillBook& b = h.Skills();
    b.InitStarterLevels();
    int32_t points = 1;
    CHECK(b.Invest(Idx(h, "slash"), 1, points));
    CHECK(b.Level("slash") == 2);
    CHECK(points == 0);
    CHECK_FALSE(b.Invest(Idx(h, "slash"), 1, points));
    CHECK(b.Level("slash") == 2);
    points = 2;
    const auto before = b.LevelsForSave();
    CHECK_FALSE(b.Invest(Idx(h, "war_stomp"), 1, points));
    CHECK(points == 2);
    CHECK(b.LevelsForSave() == before);
    // Through the hero's own skill points.
    LevelTo(h, 3);
    CHECK(h.FreeSkillPoints() == 2);
    CHECK(b.Invest(Idx(h, "frenzy"), h.Level(), h.MutableFreeSkillPoints()));
    CHECK(h.FreeSkillPoints() == 1);
    CHECK(b.Level("frenzy") == 2);
  }

  // ===================================================================================================================
  // Hotbar (C3, C1)
  // ===================================================================================================================
  TEST_CASE("starter hotbar per class: learned skills in definition order, passives excluded (C1, C3)") {
    Hero w(test::RealData(), ClassId::Warrior);
    w.Skills().InitStarterLevels();
    CHECK(HotbarIds(w.Skills()) == std::vector<std::string>{"slash", "shield_wall", "frenzy", "-", "-", "-"});
    Hero m(test::RealData(), ClassId::Mage);
    m.Skills().InitStarterLevels();
    CHECK(HotbarIds(m.Skills()) ==
          std::vector<std::string>{"fireball", "ice_armor", "mana_shield", "ice_arrow", "-", "-"});
    Hero r(test::RealData(), ClassId::Rogue);
    r.Skills().InitStarterLevels();
    CHECK(HotbarIds(r.Skills()) == std::vector<std::string>{"backstab", "multishot", "explosive_trap", "-", "-", "-"});
    CHECK(w.Skills().HotbarCapacity() == 6);
    CHECK(w.Skills().HotbarSkill(-1) == -1);
    CHECK(w.Skills().HotbarSkill(6) == -1);
    CHECK(w.Skills().HotbarSlotOf(Idx(w, "frenzy")) == 2);
    CHECK(w.Skills().HotbarSlotOf(Idx(w, "life_regen")) == -1);
  }

  TEST_CASE("learned loadout is bounded and in class order (getLearnedSkillLoadout, 7.5)") {
    Hero h(test::RealData(), ClassId::Warrior);
    h.Skills().InitStarterLevels();
    h.Skills().SetLevel(Idx(h, "whirlwind"), 1);
    const std::vector<int32_t> three = h.Skills().LearnedLoadout(3);
    REQUIRE(three.size() == 3);
    CHECK(h.Skills().Skill(three[0]).id == "slash");
    CHECK(h.Skills().Skill(three[1]).id == "whirlwind");
    CHECK(h.Skills().Skill(three[2]).id == "shield_wall");
    CHECK(h.Skills().LearnedLoadout(0).empty());
    CHECK(h.Skills().LearnedLoadout(-2).empty());
    CHECK(h.Skills().LearnedLoadout(10).size() == 4);  // life_regen is passive (C1)
  }

  TEST_CASE("a newly learned skill auto-fills the first empty slot; passives and full bars do not (C3)") {
    Hero h(test::RealData(), ClassId::Warrior);
    SkillBook& b = h.Skills();
    b.InitStarterLevels();
    // Free slot 0 so the first empty slot is not the last one.
    CHECK(b.SetHotbar(0, -1));
    int32_t points = 10;
    b.SetLevel(Idx(h, "slash"), 4);
    CHECK(b.Invest(Idx(h, "whirlwind"), 6, points));
    CHECK(HotbarIds(b) == std::vector<std::string>{"whirlwind", "shield_wall", "frenzy", "-", "-", "-"});
    // A second rank does not add it again.
    CHECK(b.Invest(Idx(h, "whirlwind"), 6, points));
    CHECK(HotbarIds(b) == std::vector<std::string>{"whirlwind", "shield_wall", "frenzy", "-", "-", "-"});
    // Passives never go on the bar (unyielding is a tier-2 guardian passive).
    b.SetLevel(Idx(h, "shield_wall"), 4);
    CHECK(b.Invest(Idx(h, "unyielding"), 6, points));
    CHECK(b.Level("unyielding") == 1);
    CHECK(b.HotbarSlotOf(Idx(h, "unyielding")) == -1);
    // Fill the bar, then learn one more: it stays off the bar.
    CHECK(b.Invest(Idx(h, "charge"), 6, points));
    CHECK(b.Invest(Idx(h, "taunt_roar"), 6, points));
    CHECK(b.Invest(Idx(h, "iron_fortress"), 6, points));
    CHECK(HotbarIds(b) ==
          std::vector<std::string>{"whirlwind", "shield_wall", "frenzy", "charge", "taunt_roar", "iron_fortress"});
    b.SetLevel(Idx(h, "frenzy"), 4);
    CHECK(b.Invest(Idx(h, "bleed_strike"), 6, points));
    CHECK(b.HotbarSlotOf(Idx(h, "bleed_strike")) == -1);
    // The slash binding was removed by the player and is not re-added by investing more ranks.
    CHECK(b.Invest(Idx(h, "slash"), 6, points));
    CHECK(b.HotbarSlotOf(Idx(h, "slash")) == -1);
  }

  TEST_CASE("SetHotbar: bind, swap, clear; passives, unlearned skills and bad slots are refused (C3)") {
    Hero h(test::RealData(), ClassId::Warrior);
    SkillBook& b = h.Skills();
    b.InitStarterLevels();
    const int32_t slash = Idx(h, "slash");
    const int32_t frenzy = Idx(h, "frenzy");
    // Move frenzy onto slash's slot: they swap.
    CHECK(b.SetHotbar(0, frenzy));
    CHECK(HotbarIds(b) == std::vector<std::string>{"frenzy", "shield_wall", "slash", "-", "-", "-"});
    // Move into an empty slot: the old slot empties.
    CHECK(b.SetHotbar(5, slash));
    CHECK(HotbarIds(b) == std::vector<std::string>{"frenzy", "shield_wall", "-", "-", "-", "slash"});
    // Same slot again is a no-op success.
    CHECK(b.SetHotbar(5, slash));
    CHECK(b.HotbarSkill(5) == slash);
    // Clear.
    CHECK(b.SetHotbar(1, -1));
    CHECK(b.HotbarSkill(1) == -1);
    // Refusals leave the bar unchanged.
    const auto before = b.Hotbar();
    CHECK_FALSE(b.SetHotbar(2, Idx(h, "life_regen")));  // passive, even though learned
    CHECK_FALSE(b.SetHotbar(2, Idx(h, "whirlwind")));   // not learned
    CHECK_FALSE(b.SetHotbar(6, slash));
    CHECK_FALSE(b.SetHotbar(-1, slash));
    CHECK_FALSE(b.SetHotbar(2, 999));
    CHECK_FALSE(b.SetHotbar(2, -7));
    CHECK(b.Hotbar() == before);
  }

  TEST_CASE("hotbar save round trip; LoadHotbar drops empty, unknown, passive, unlearned and repeated ids (C3)") {
    Hero h(test::RealData(), ClassId::Warrior);
    SkillBook& b = h.Skills();
    b.InitStarterLevels();
    CHECK(b.SetHotbar(4, Idx(h, "slash")));
    const std::array<std::string, SkillBook::kHotbarSlots> saved = b.HotbarForSave();
    CHECK(saved == std::array<std::string, 6>{"", "shield_wall", "frenzy", "", "slash", ""});
    Hero g(test::RealData(), ClassId::Warrior);
    g.Skills().InitStarterLevels();
    g.Skills().LoadHotbar(saved);
    CHECK(g.Skills().Hotbar() == b.Hotbar());
    g.Skills().LoadHotbar({"", "frenzy", "life_regen", "nope", "frenzy", "whirlwind"});
    CHECK(HotbarIds(g.Skills()) == std::vector<std::string>{"-", "frenzy", "-", "-", "-", "-"});
    g.Skills().FillHotbarDefault();
    CHECK(HotbarIds(g.Skills()) == std::vector<std::string>{"slash", "shield_wall", "frenzy", "-", "-", "-"});
  }

  TEST_CASE("skill levels save: definition order, missing = 0, unknown ignored, clamped to [0, maxLevel]") {
    Hero h(test::RealData(), ClassId::Rogue);
    h.Skills().InitStarterLevels();
    const auto saved = h.Skills().LevelsForSave();
    REQUIRE(saved.size() == 13);
    CHECK(saved[0] == std::pair<std::string, int32_t>{"backstab", 1});
    CHECK(saved[1] == std::pair<std::string, int32_t>{"poison_blade", 0});
    h.Skills().LoadLevels({{"vanish", 3}, {"power_strike", 5}, {"multishot", 99}, {"backstab", -4}, {"vanish", 4}});
    CHECK(h.Skills().Level("vanish") == 4);  // last value wins (JS object semantics)
    CHECK(h.Skills().Level("multishot") == 20);
    CHECK(h.Skills().Level("backstab") == 0);
    CHECK(h.Skills().Level("explosive_trap") == 0);
  }

  // ===================================================================================================================
  // Spirit (14) - SpiritSystem.test.ts, SpiritSaveMigration.test.ts, checklist 10
  // ===================================================================================================================
  TEST_CASE("spirit profiles give each class a distinct identity; unknown class falls back to warrior") {
    const SpiritTable& t = test::RealData().Classes().spirit;
    const SpiritProfileDef& w = t.For(ClassId::Warrior);
    const SpiritProfileDef& m = t.For(ClassId::Mage);
    const SpiritProfileDef& r = t.For(ClassId::Rogue);
    CHECK(w.id == "emberheart");
    CHECK(m.id == "astral_focus");
    CHECK(r.id == "shadow_rhythm");
    CHECK(w.visualColor == 0xffb45cu);
    CHECK(m.visualColor == 0xa98bffu);
    CHECK(r.visualColor == 0x66e58au);
    CHECK(w.resonanceDamageBonus > 0);
    CHECK(m.resonanceManaCostMultiplier < 1);
    CHECK(r.dodgeGain > r.hitGain);
    Spirit unknown(t, static_cast<ClassId>(9));
    CHECK(unknown.Profile().id == "emberheart");
  }

  TEST_CASE("spirit gains scale with raw SPI (clamped 0..200) and add the crit bonus (14.2)") {
    const SpiritTable& t = test::RealData().Classes().spirit;
    Spirit s(t, ClassId::Warrior);
    const SpiritGainResult base = s.GainFromCombat(SpiritSource::Hit, 0, false);
    const SpiritGainResult scaled = s.GainFromCombat(SpiritSource::Hit, 20, false);
    CHECK(base.gained > 0);
    CHECK(scaled.gained > base.gained);
    CHECK(s.Value() == base.gained + scaled.gained);
    Spirit normal(t, ClassId::Mage);
    Spirit critical(t, ClassId::Mage);
    normal.GainFromCombat(SpiritSource::Hit, 10, false);
    critical.GainFromCombat(SpiritSource::Hit, 10, true);
    CHECK(critical.Value() > normal.Value());
    // Per-class gains with base SPI (14.2).
    struct G {
      ClassId cls;
      int32_t spi;
      double hit, hitCrit, kill, dodge;
    };
    for (const G& g : {G{ClassId::Warrior, 5, 8.6, 12.9, 16.125, 12.9}, G{ClassId::Mage, 10, 8.05, 13.8, 13.8, 18.4},
                       G{ClassId::Rogue, 5, 6.45, 13.975, 13.975, 21.5}}) {
      CAPTURE(EnumName(g.cls));
      Spirit a(t, g.cls);
      CHECK(a.GainFromCombat(SpiritSource::Hit, g.spi, false).gained == doctest::Approx(g.hit));
      Spirit b(t, g.cls);
      CHECK(b.GainFromCombat(SpiritSource::Hit, g.spi, true).gained == doctest::Approx(g.hitCrit));
      Spirit c(t, g.cls);
      CHECK(c.GainFromCombat(SpiritSource::Kill, g.spi, false).gained == doctest::Approx(g.kill));
      Spirit d(t, g.cls);
      CHECK(d.GainFromCombat(SpiritSource::Dodge, g.spi, false).gained == doctest::Approx(g.dodge));
    }
    // SPI clamp: 500 counts as 200 (x4), negative as 0.
    Spirit hi(t, ClassId::Warrior);
    CHECK(hi.GainFromCombat(SpiritSource::Hit, 500, false).gained == doctest::Approx(8 * 4.0));
    Spirit lo(t, ClassId::Warrior);
    CHECK(lo.GainFromCombat(SpiritSource::Hit, -50, false).gained == doctest::Approx(8.0));
  }

  TEST_CASE("spirit clamps at max, starts Resonance, ignores zero / negative / non-finite / in-Resonance gains") {
    const SpiritTable& t = test::RealData().Classes().spirit;
    Spirit r(t, ClassId::Rogue);
    const SpiritGainResult res = r.Gain(999);
    CHECK(res.resonanceStarted);
    CHECK(r.IsResonating());
    CHECK(r.Value() == r.MaxValue());
    CHECK(res.gained == 100);
    CHECK(r.ResonanceRemainingMs() == 5500);
    Spirit w(t, ClassId::Warrior);
    CHECK(w.Gain(0).gained == 0);
    CHECK(w.Gain(-5).gained == 0);
    CHECK(w.Gain(std::numeric_limits<double>::quiet_NaN()).gained == 0);
    CHECK(w.Gain(std::numeric_limits<double>::infinity()).gained == 0);
    CHECK(w.Value() == 0);
    w.Gain(w.MaxValue());
    const SpiritGainResult blocked = w.Gain(20);
    CHECK(blocked.gained == 0);
    CHECK_FALSE(blocked.resonanceStarted);
  }

  TEST_CASE("Resonance drains linearly and ends; multipliers only while active (14.2)") {
    const SpiritTable& t = test::RealData().Classes().spirit;
    Spirit s(t, ClassId::Warrior);
    CHECK(s.DamageMultiplier() == 1);
    CHECK(s.ManaCostMultiplier() == 1);
    CHECK(s.MoveSpeedMultiplier() == 1);
    s.Gain(s.MaxValue());
    CHECK(s.DamageMultiplier() == 1 + 0.3);
    CHECK(s.ManaCostMultiplier() == 0.85);
    CHECK(s.MoveSpeedMultiplier() == 1 + 0.12);
    CHECK_FALSE(s.Update(3000));
    CHECK(s.Value() == doctest::Approx(50).epsilon(1e-9));
    CHECK(s.ResonanceRemainingMs() == 3000);
    CHECK_FALSE(s.Update(0));
    CHECK_FALSE(s.Update(-10));
    CHECK_FALSE(s.Update(std::numeric_limits<double>::quiet_NaN()));
    CHECK(s.Update(3000));
    CHECK_FALSE(s.IsResonating());
    CHECK(s.Value() == 0);
    CHECK(s.ResonanceRemainingMs() == 0);
    CHECK(s.DamageMultiplier() == 1);
    CHECK_FALSE(s.Update(16));  // not resonating: nothing to end
  }

  TEST_CASE("checklist 10: warrior 12 hits of 8.6 start Resonance; 6000 ms of 60 Hz steps drain it to 0") {
    const SpiritTable& t = test::RealData().Classes().spirit;
    Spirit s(t, ClassId::Warrior);
    for (int i = 1; i <= 11; ++i) {
      const SpiritGainResult g = s.GainFromCombat(SpiritSource::Hit, 5, false);
      CHECK(g.gained == doctest::Approx(8.6));
      CHECK_FALSE(g.resonanceStarted);
    }
    CHECK(s.Value() == doctest::Approx(94.6));
    const SpiritGainResult last = s.GainFromCombat(SpiritSource::Hit, 5, false);
    CHECK(last.resonanceStarted);
    CHECK(last.gained == doctest::Approx(5.4));  // 103.2 clamps to 100
    CHECK(s.Value() == 100);
    CHECK(s.GainFromCombat(SpiritSource::Kill, 5, false).gained == 0);  // ignored while resonating
    // Drive with sim steps (1000/60 ms): ends exactly when 6000 ms of sim time have elapsed.
    SimClock clock;
    double prev = clock.NowMs();
    int steps = 0;
    bool ended = false;
    while (!ended && steps < 1000) {
      clock.AdvanceStep();
      ended = s.Update(clock.NowMs() - prev);
      prev = clock.NowMs();
      ++steps;
    }
    CHECK(ended);
    CHECK(steps == 360);
    CHECK(clock.NowMs() == 6000);
    CHECK(s.Value() == 0);
    // restore({value: 50, resonanceRemainingMs: 0}) -> not resonating, value 50.
    s.Restore({50, 0});
    CHECK_FALSE(s.IsResonating());
    CHECK(s.Value() == 50);
  }

  TEST_CASE("spirit save state: round trip and malformed values (14.4; SpiritSaveMigration)") {
    const SpiritTable& t = test::RealData().Classes().spirit;
    Spirit a(t, ClassId::Warrior);
    a.Gain(37);
    Spirit b(t, ClassId::Warrior);
    b.Restore(a.ToSave());
    CHECK(b.ToSave().value == a.ToSave().value);
    CHECK(b.ToSave().resonanceRemainingMs == a.ToSave().resonanceRemainingMs);
    CHECK_FALSE(b.IsResonating());
    Spirit r(t, ClassId::Rogue);
    r.Restore({999, -100});
    CHECK(r.Value() == r.MaxValue());
    CHECK(r.ResonanceRemainingMs() == 0);
    CHECK_FALSE(r.IsResonating());
    r.Restore({std::numeric_limits<double>::quiet_NaN(), -10});
    CHECK(r.Value() == 0);
    CHECK(r.ResonanceRemainingMs() == 0);
    r.Restore({64, 2500});
    CHECK(r.Value() == 64);
    CHECK(r.ResonanceRemainingMs() == 2500);
    CHECK(r.IsResonating());
    r.Restore({100, 9000});  // clamped to the rogue profile duration
    CHECK(r.Value() == 100);
    CHECK(r.ResonanceRemainingMs() == 5500);
    r.Restore({0, 4000});  // value 0 -> remaining 0
    CHECK(r.ResonanceRemainingMs() == 0);
    r.Restore({30, std::numeric_limits<double>::infinity()});
    CHECK(r.ResonanceRemainingMs() == 0);
    r.Reset();
    CHECK(r.Value() == 0);
  }

  // ===================================================================================================================
  // Buffs (11)
  // ===================================================================================================================
  TEST_CASE("BuffList: tagged refresh, sums, caps, pruning by duration") {
    BuffList b;
    b.Add({BuffStat::DamageReduction, 0.2, 1000, 0, BuffTag::None, kNoEntity});
    b.Add({BuffStat::DamageReduction, 0.1, 3000, 0, BuffTag::None, kNoEntity});
    b.RefreshTagged({BuffStat::DamageAmplify, 0.15, 2000, 0, BuffTag::CurseAura, 7});
    b.RefreshTagged({BuffStat::DamageAmplify, 0.15, 2000, 500, BuffTag::CurseAura, 7});
    CHECK(b.Items().size() == 3);
    CHECK(b.RawSum(BuffStat::DamageReduction) == doctest::Approx(0.3));
    REQUIRE(b.FindTag(BuffTag::CurseAura) != nullptr);
    CHECK(b.FindTag(BuffTag::CurseAura)->startMs == 500);
    CHECK(b.Prune(999) == 0);
    CHECK(b.Prune(1000) == 1);  // now - start >= duration
    CHECK(b.RawSum(BuffStat::DamageReduction) == doctest::Approx(0.1));
    CHECK(b.RemoveTag(BuffTag::CurseAura) == 1);
    CHECK(b.Has(BuffStat::DamageReduction));
    // Caps (buff_caps.json): stacked shield walls cap at 0.9; damageAmplify is uncapped.
    const BuffCaps& caps = test::RealData().Classes().buffCaps;
    BuffList s;
    for (int i = 0; i < 3; ++i) s.Add({BuffStat::DamageReduction, 0.5, 5000, 0, BuffTag::None, kNoEntity});
    CHECK(s.Value(BuffStat::DamageReduction, caps) == 0.9);
    for (int i = 0; i < 30; ++i) s.Add({BuffStat::DamageAmplify, 0.25, 8000, 0, BuffTag::None, kNoEntity});
    CHECK(s.Value(BuffStat::DamageAmplify, caps) == doctest::Approx(7.5));
    s.Add({BuffStat::StealthDamage, 1.0, 3000, 0, BuffTag::None, kNoEntity});
    CHECK(s.RemoveStat(BuffStat::StealthDamage) == 1);
    CHECK_FALSE(s.Has(BuffStat::StealthDamage));
  }

  TEST_CASE("generic buff values come from the scaling functions (9.6): shield_wall L10 b0.628 / 6275 ms") {
    Hero h(test::RealData(), ClassId::Warrior);
    const SkillDef& sw = SkillById("shield_wall");
    h.Buffs().Add({sw.buff.stat, SkillBuffValue(Rules(), sw, 10), SkillBuffDurationMs(Rules(), sw, 10), 100,
                   BuffTag::None, kNoEntity});
    CHECK(h.Buffs().Value(BuffStat::DamageReduction, test::RealData().Classes().buffCaps) == doctest::Approx(0.6275));
    CHECK(h.Buffs().Items()[0].ActiveAt(6374));
    CHECK_FALSE(h.Buffs().Items()[0].ActiveAt(6375));
  }

  // ===================================================================================================================
  // Save mapping (18; save-ui-input 3.2 / 3.5)
  // ===================================================================================================================
  TEST_CASE("Hero::ToSave / FromSave round trip (no position restore, default hotbar, raw hp / mana)") {
    const DataStore& data = test::RealData();
    Hero a(data, ClassId::Mage);
    a.Skills().InitStarterLevels();
    LevelTo(a, 7);
    a.AddExp(33, EquipStats{});
    a.AddGold(1234);
    a.AllocateStat(PrimaryStat::Int);
    a.AllocateStat(PrimaryStat::Int);
    a.Skills().SetLevel(a.Skills().IndexOf("fireball"), 6);
    a.GetSpirit().Gain(37.5);
    a.SetPosition({15.5, 22.25});
    a.SetHp(77.25);
    a.SetMana(12.5);
    SaveHero s;
    a.ToSave(s);
    CHECK(s.level == 7);
    CHECK(s.exp == 33);
    CHECK(s.gold == 1234);
    CHECK(s.hp == 77.25);
    CHECK(s.mana == 12.5);
    CHECK(s.maxHp == a.MaxHp());
    CHECK(s.maxMana == a.MaxMana());
    CHECK(s.stats == a.BaseStats());
    CHECK(s.freeStatPoints == 28);
    CHECK(s.freeSkillPoints == 6);
    CHECK(s.skillLevels.size() == 12);
    CHECK(s.hasSpirit);
    CHECK(s.spirit.value == 37.5);
    CHECK(s.tileCol == 15.5);
    CHECK(s.tileRow == 22.25);

    Hero b(data, ClassId::Mage);
    b.Buffs().Add({BuffStat::ManaShield, 0.3, 8000, 0, BuffTag::None, kNoEntity});
    b.Skills().StartCooldown(0, 9999);
    b.SetLife(HeroLife::Dying);
    b.FromSave(s);
    CHECK(b.Level() == 7);
    CHECK(b.Exp() == 33);
    CHECK(b.Gold() == 1234);
    CHECK(b.BaseStats() == a.BaseStats());
    CHECK(b.FreeStatPoints() == 28);
    CHECK(b.FreeSkillPoints() == 6);
    CHECK(b.Skills().LevelsForSave() == a.Skills().LevelsForSave());
    CHECK(b.GetSpirit().Value() == 37.5);
    CHECK(b.Hp() == 77.25);
    CHECK(b.Mana() == 12.5);
    CHECK(b.MaxHp() == a.MaxHp());
    CHECK(b.Life() == HeroLife::Alive);
    CHECK(b.Buffs().Empty());  // buffs and cooldowns are not saved
    CHECK(b.Skills().ReadyAtMs(0) == 0);
    CHECK(b.Position() == Vec2{});  // GameSim places the hero
    CHECK(HotbarIds(b.Skills()) ==
          std::vector<std::string>{"fireball", "ice_armor", "mana_shield", "ice_arrow", "-", "-"});
  }

  TEST_CASE("Hero::FromSave normalises corrupt values and restores spirit with the class clamps") {
    const DataStore& data = test::RealData();
    SaveHero s;
    s.level = 0;
    s.exp = -40;
    s.gold = -5;
    s.freeStatPoints = -1;
    s.freeSkillPoints = -2;
    s.stats = data.Classes().Find(ClassId::Rogue)->baseStats;
    s.skillLevels = {{"backstab", 25}, {"slash", 3}, {"vanish", -1}};
    s.hasSpirit = true;
    s.spirit = {100, 9000};
    s.hp = std::numeric_limits<double>::quiet_NaN();
    s.mana = std::numeric_limits<double>::infinity();
    Hero h(data, ClassId::Rogue);
    h.FromSave(s);
    CHECK(h.Level() == 1);
    CHECK(h.Exp() == 0);
    CHECK(h.Gold() == 0);
    CHECK(h.FreeStatPoints() == 0);
    CHECK(h.FreeSkillPoints() == 0);
    CHECK(h.Skills().Level("backstab") == 20);
    CHECK(h.Skills().Level("vanish") == 0);
    CHECK(h.Skills().Level("multishot") == 0);  // missing key = 0 (no starter levels on load)
    CHECK(h.GetSpirit().ResonanceRemainingMs() == 5500);
    CHECK(h.GetSpirit().IsResonating());
    CHECK(h.Hp() == 0);              // non-finite -> 0: GameSim loads it as a completed respawn (FIX Q35)
    CHECK(h.Mana() == h.MaxMana());  // non-finite -> maxMana
    CHECK(HotbarIds(h.Skills()) == std::vector<std::string>{"backstab", "-", "-", "-", "-", "-"});
    // A save without spirit restores {0, 0}.
    s.hasSpirit = false;
    h.FromSave(s);
    CHECK(h.GetSpirit().Value() == 0);
    CHECK_FALSE(h.GetSpirit().IsResonating());
  }

  // ===================================================================================================================
  // RewardService (5.2 events and order, 5.1.1 Dying gates)
  // ===================================================================================================================
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

  TEST_CASE("RewardService level-up: EvLevelUp, the levelUp log, HeroLevelUpMsg, then EvExpGained (5.2)") {
    test::SimHarness h;
    Hero hero(h.ctx.data, ClassId::Warrior);
    InventorySystem inv(h.ctx);
    RewardService rewards(h.ctx);
    h.ctx.sys.hero = &hero;
    h.ctx.sys.inventory = &inv;
    h.ctx.sys.rewards = &rewards;
    std::vector<int32_t> levelMsgs;
    h.bus.Subscribe<HeroLevelUpMsg>([&](const HeroLevelUpMsg& m) { levelMsgs.push_back(m.level); });
    h.ctx.equip.Ref(Stat::MaxHp) = 30;  // the merged bag: the refill is gear-inclusive (C1)
    hero.RecalcDerived(h.ctx.equip);
    hero.SetHp(5);

    const LevelUpResult r = rewards.GrantExp(500, ExpSource::Quest);
    CHECK(r.leveledUp);
    CHECK(r.level == 2);
    REQUIRE(h.events.Size() == 3);
    const auto* up = std::get_if<EvLevelUp>(&h.events.Items()[0]);
    REQUIRE(up != nullptr);
    CHECK(up->level == 2);
    const auto* log = std::get_if<EvLog>(&h.events.Items()[1]);
    REQUIRE(log != nullptr);
    CHECK(log->text.key == "sys.player.levelUp");
    CHECK(log->type == LogType::System);
    const auto* gained = std::get_if<EvExpGained>(&h.events.Items()[2]);
    REQUIRE(gained != nullptr);
    CHECK(gained->amount == 500);
    CHECK(gained->exp == 472);
    CHECK(gained->expToNext == 62);
    CHECK(levelMsgs == std::vector<int32_t>{2});
    CHECK(hero.Hp() == hero.MaxHp());
    CHECK(hero.MaxHp() == 165 + 30);

    // The overflow converts on the next grant, even a zero one.
    h.events.Clear();
    rewards.GrantExp(0, ExpSource::Kill);
    CHECK(hero.Level() == 3);
    CHECK(levelMsgs == std::vector<int32_t>{2, 3});
    CHECK(test::CountEvents<EvLevelUp>(h.events) == 1);

    // Death toll: negative EvExpGained with the applied amount, never de-levels.
    h.events.Clear();
    rewards.RemoveExp(1000, ExpSource::DeathPenalty);
    CHECK(hero.Exp() == 0);
    CHECK(hero.Level() == 3);
    const auto* toll = std::get_if<EvExpGained>(&h.events.Items().back());
    REQUIRE(toll != nullptr);
    CHECK(toll->amount == -410);
    CHECK(toll->source == ExpSource::DeathPenalty);
  }

  // ===================================================================================================================
  // GameSim level: the hero inside the real step order and the real save path
  // ===================================================================================================================
  TEST_CASE("step order (16 step 6, 4.2): Life Regen heals BEFORE the Unyielding check reads the hp ratio") {
    auto sim = LiveHeroSim(ClassId::Warrior, 41);
    Hero& h = *sim->Context().sys.hero;
    const int32_t lifeRegen = Idx(h, "life_regen");
    const int32_t uny = Idx(h, "unyielding");
    h.Skills().SetLevel(lifeRegen, 5);
    h.Skills().SetLevel(uny, 1);
    const SkillDef& unySkill = h.Skills().Skill(uny);
    REQUIRE(unySkill.passiveRule.kind == PassiveRuleKind::LowHpProc);
    const double ratio = unySkill.passiveRule.hpRatioBelow;
    REQUIRE(ratio == 0.3);
    // This step's Life Regen (pre-movement campfire factor, the web's `recovery`).
    const HeroFormulas& f = test::RealData().Classes().formulas;
    const double mul = sim->Context().sys.zone->NearCampfire(h.Position()) ? f.campfireHpMultiplier : 1.0;
    const double perStep = 5 * SkillById("life_regen").passiveRule.hpPerSecondPerLevel * kSimStepMs / 1000 * mul;
    const double threshold = ratio * h.MaxHp();

    // Half a step's regen under 30 %: the heal lifts hp to >= 30 % first, so Unyielding does not proc.
    h.SetHp(threshold - perStep / 2);
    sim->Step();
    CHECK(h.Hp() > threshold);
    CHECK_FALSE(h.Buffs().Has(BuffStat::DamageReduction));
    CHECK(h.Skills().IsReady(uny, sim->NowMs()));
    CHECK(h.stepRegen.hpMul == mul);  // the step's modifiers, shared with step 7

    // Control: still under 30 % after this step's Life Regen -> the proc fires (buff + cooldown).
    h.SetHp(threshold - perStep * 1.5);
    sim->Step();
    CHECK(h.Buffs().Has(BuffStat::DamageReduction));
    CHECK_FALSE(h.Skills().IsReady(uny, sim->NowMs()));
  }

  TEST_CASE("kill pipeline: spirit 'kill' gain carries its source (14.3, 17); kill heal snaps hp above a lowered max") {
    auto sim = LiveHeroSim(ClassId::Warrior, 43);
    SimContext& ctx = sim->Context();
    Hero& h = *ctx.sys.hero;
    EntityId victim = kNoEntity;
    for (const MonsterView& m : sim->View().monsters) {
      if (m.alive) {
        victim = m.id;
        break;
      }
    }
    REQUIRE(victim != kNoEntity);
    // hp above a lowered max (unequipped +maxHp gear), killHealPercent 1 %: heal = floor(150 x 1 / 100) = 1.
    EquipStats gear;
    gear.Ref(Stat::MaxHp) = 60;
    h.RecalcDerived(gear);
    h.FillHpMana();
    h.RecalcDerived(EquipStats{});
    REQUIRE(h.Hp() == 210);
    REQUIRE(h.MaxHp() == 150);
    ctx.equip = EquipStats{};
    ctx.equip.Ref(Stat::KillHealPercent) = 1;
    sim->ClearEvents();
    MonsterHitRequest r;
    r.monster = victim;
    r.amount = 1e9;
    r.attacker = kHeroEntityId;
    r.source = KillSource::HeroBasic;
    ctx.sys.combat->DamageMonster(r);
    // Web ZoneScene.ts:3816: hp = Math.min(maxHp, hp + heal).
    CHECK(h.Hp() == 150);
    const EvSpiritChanged* gain = nullptr;
    for (const Event& e : sim->Events()) {
      if (const auto* s = std::get_if<EvSpiritChanged>(&e)) gain = s;
    }
    REQUIRE(gain != nullptr);
    CHECK(gain->hasSource);
    CHECK(gain->source == SpiritSource::Kill);
    CHECK(gain->gained == doctest::Approx(15 * 1.075));
    CHECK(gain->value == doctest::Approx(15 * 1.075));
    CHECK_FALSE(gain->resonating);

    // The Resonance end (14.3: {value 0, maxValue, resonating false}) carries no source.
    h.GetSpirit().Gain(h.GetSpirit().MaxValue());
    REQUIRE(h.GetSpirit().IsResonating());
    std::vector<EvSpiritChanged> changes;
    for (int i = 0; i < 400 && h.GetSpirit().IsResonating(); ++i) {
      sim->Step();
      for (const Event& e : sim->Events()) {
        if (const auto* s = std::get_if<EvSpiritChanged>(&e)) changes.push_back(*s);
      }
    }
    CHECK_FALSE(h.GetSpirit().IsResonating());
    REQUIRE_FALSE(changes.empty());
    CHECK_FALSE(changes.back().hasSource);
    CHECK(changes.back().value == 0);
    CHECK_FALSE(changes.back().resonating);
  }

  TEST_CASE("save round trip through GameSim: level, exp, gold, stats, points, skills, spirit, hotbar, settings") {
    // save-ui-input 3.2-3.5 (player / settings sections, v4 hotbar), U2, C3: SaveGame -> JSON -> ParseSave / LoadGame.
    auto sim = LiveHeroSim(ClassId::Rogue, 47);
    SimContext& ctx = sim->Context();
    Hero& h = *ctx.sys.hero;
    RewardService& rewards = *ctx.sys.rewards;
    CombatSystem& combat = *ctx.sys.combat;
    while (h.Level() < 4) rewards.GrantExp(h.ExpToNext() - h.Exp(), ExpSource::Debug);
    rewards.GrantExp(17, ExpSource::Debug);
    rewards.ChangeGold(321, GoldReason::Debug);
    REQUIRE(combat.AllocateStat(PrimaryStat::Dex, 3));
    REQUIRE(combat.AllocateStat(PrimaryStat::Vit, 1));
    // One point into the first learned active skill of the bar (a valid tier-1 investment).
    const int32_t first = h.Skills().HotbarSkill(0);
    REQUIRE(first >= 0);
    REQUIRE(combat.LearnSkill(first));
    // Swap the first two hotbar slots (C3: player-edited bar is saved as is).
    const int32_t second = h.Skills().HotbarSkill(1);
    REQUIRE(second >= 0);
    REQUIRE(combat.SetHotbar(0, second));
    REQUIRE(h.Skills().HotbarSkill(1) == first);
    h.GetSpirit().Gain(42.5);
    h.autoCombat = true;
    h.autoLoot = AutoLootMode::Rare;
    h.SetHp(h.MaxHp() - 7.5);
    h.SetMana(h.MaxMana() - 3.25);
    const Vec2 pos = h.Position();
    const int32_t level = h.Level();
    const int64_t exp = h.Exp();
    const int64_t gold = h.Gold();
    const PrimaryStats stats = h.BaseStats();
    const int32_t freeStat = h.FreeStatPoints();
    const int32_t freeSkill = h.FreeSkillPoints();
    const auto levels = h.Skills().LevelsForSave();
    const auto hotbar = h.Skills().HotbarForSave();
    const double spirit = h.GetSpirit().Value();
    const double hp = h.Hp();
    const double mana = h.Mana();
    REQUIRE(level == 4);
    REQUIRE(exp == 17);
    REQUIRE(freeStat == 3 * 5 - 4);
    REQUIRE(freeSkill == 3 - 1);

    const std::string json = sim->SaveGame(5555);
    SaveData parsed;
    std::string err;
    REQUIRE(ParseSave(json, parsed, &err, &test::RealData()) == SaveError::None);
    CHECK(parsed.classId == ClassId::Rogue);
    CHECK(parsed.player.level == level);
    CHECK(parsed.player.exp == exp);
    CHECK(parsed.player.gold == gold);
    CHECK(parsed.player.stats == stats);
    CHECK(parsed.player.freeStatPoints == freeStat);
    CHECK(parsed.player.freeSkillPoints == freeSkill);
    CHECK(parsed.player.skillLevels == levels);
    CHECK(parsed.player.hasSpirit);
    CHECK(parsed.player.spirit.value == spirit);
    CHECK(parsed.player.hp == hp);
    CHECK(parsed.player.mana == mana);
    CHECK(parsed.player.maxHp == h.MaxHp());
    CHECK(parsed.player.tileCol == pos.x);
    CHECK(parsed.player.tileRow == pos.y);
    CHECK(parsed.hasHotbar);
    CHECK(parsed.hotbar == hotbar);
    CHECK(parsed.settings.autoCombat);
    CHECK(parsed.settings.autoLootMode == AutoLootMode::Rare);

    auto other = GameSim::Create(test::RealData(), SimConfig{});
    REQUIRE(other != nullptr);
    REQUIRE(other->LoadGame(json, &err) == SaveError::None);
    const Hero& b = *other->Context().sys.hero;
    CHECK(b.Class() == ClassId::Rogue);
    CHECK(b.Level() == level);
    CHECK(b.Exp() == exp);
    CHECK(b.Gold() == gold);
    CHECK(b.BaseStats() == stats);
    CHECK(b.FreeStatPoints() == freeStat);
    CHECK(b.FreeSkillPoints() == freeSkill);
    CHECK(b.Skills().LevelsForSave() == levels);
    CHECK(b.Skills().HotbarForSave() == hotbar);
    CHECK(b.GetSpirit().Value() == spirit);
    CHECK(b.Hp() == hp);
    CHECK(b.Mana() == mana);
    CHECK(b.MaxHp() == h.MaxHp());
    CHECK(b.Position() == pos);
    CHECK(b.autoCombat);
    CHECK(b.autoLoot == AutoLootMode::Rare);
    // And the reloaded session saves the same player section again.
    SaveData again;
    other->BuildSave(again, 5555);
    CHECK(again.player.level == level);
    CHECK(again.player.skillLevels == levels);
    CHECK(again.hotbar == hotbar);
  }

#if ABYSS_HERO_TEST_SAVE_SECTIONS
  // ===================================================================================================================
  // player / settings / soulEcho JSON sections (save-ui-input 3.2, 3.3 lenient reads, 3.9 example)
  // ===================================================================================================================
  TEST_CASE("player section: web key order and a parse round trip") {
    Hero hero(test::RealData(), ClassId::Warrior);
    hero.Skills().InitStarterLevels();
    hero.SetPosition({15, 22});
    SaveHero s;
    hero.ToSave(s);
    s.currentMap = "emerald_plains";
    JsonWriter w;
    savejson::WriteHero(w, s);
    const std::string text = w.Take();
    CHECK(text.rfind("{\"level\":1,\"exp\":0,\"gold\":0,\"hp\":150,\"maxHp\":150,\"mana\":85,\"maxMana\":85,"
                     "\"stats\":{\"str\":12,\"dex\":8,\"vit\":10,\"int\":5,\"spi\":5,\"lck\":5},"
                     "\"freeStatPoints\":0,\"freeSkillPoints\":0,\"skillLevels\":{\"slash\":1,\"whirlwind\":0,",
                     0) == 0);
    CHECK(text.find(",\"spirit\":{\"value\":0,\"resonanceRemainingMs\":0},\"tileCol\":15,\"tileRow\":22,"
                    "\"currentMap\":\"emerald_plains\"}") != std::string::npos);
    JsonValue v;
    REQUIRE(ParseJson(text, v));
    SaveHero back;
    savejson::ReadHero(v, back);
    CHECK(back.level == s.level);
    CHECK(back.exp == s.exp);
    CHECK(back.gold == s.gold);
    CHECK(back.hp == s.hp);
    CHECK(back.mana == s.mana);
    CHECK(back.maxHp == s.maxHp);
    CHECK(back.stats == s.stats);
    CHECK(back.skillLevels == s.skillLevels);
    CHECK(back.hasSpirit);
    CHECK(back.spirit.value == 0);
    CHECK(back.tileCol == 15);
    CHECK(back.tileRow == 22);
    CHECK(back.currentMap == "emerald_plains");
    // Fractional values survive (hp / mana / position are doubles; regen is fractional).
    s.hp = 123.456;
    s.tileCol = 15.75;
    s.spirit = {37.25, 1234.5};
    JsonWriter w2;
    savejson::WriteHero(w2, s);
    REQUIRE(ParseJson(w2.Take(), v));
    savejson::ReadHero(v, back);
    CHECK(back.hp == 123.456);
    CHECK(back.tileCol == 15.75);
    CHECK(back.spirit.value == 37.25);
    CHECK(back.spirit.resonanceRemainingMs == 1234.5);
  }

  TEST_CASE("player section reads leniently: legacy skillLevels entries, wrong types, missing fields") {
    JsonValue v;
    REQUIRE(ParseJson(R"({"level":4,"exp":"12","gold":250.0,"hp":"x","stats":{"str":13,"dex":"9"},
                          "skillLevels":[["slash",5],["whirlwind",3],["bad"],[1,2],["war_stomp",2.0]],
                          "spirit":7,"currentMap":42})",
                      v));
    SaveHero s;
    s.level = 99;
    savejson::ReadHero(v, s);
    CHECK(s.level == 4);
    CHECK(s.exp == 0);     // wrong type keeps the default
    CHECK(s.gold == 250);  // integral double
    CHECK(std::isnan(s.hp));  // missing / wrong-typed hp -> NaN -> a dead save loads as a respawn (FIX Q35)
    CHECK(std::isnan(s.mana));
    CHECK(std::isnan(s.tileCol));
    CHECK(std::isnan(s.tileRow));
    CHECK(s.stats.str == 13);
    CHECK(s.stats.dex == 0);
    CHECK(s.skillLevels ==
          std::vector<std::pair<std::string, int32_t>>{{"slash", 5}, {"whirlwind", 3}, {"war_stomp", 2}});
    CHECK_FALSE(s.hasSpirit);  // not an object -> restored as {0, 0}
    CHECK(s.currentMap.empty());
    // Spirit object with junk values: kept as NaN for Spirit::Restore (-> 0).
    REQUIRE(ParseJson(R"({"spirit":{"value":"a"}})", v));
    savejson::ReadHero(v, s);
    CHECK(s.hasSpirit);
    CHECK(std::isnan(s.spirit.value));
    Hero h(test::RealData(), ClassId::Warrior);
    h.FromSave(s);
    CHECK(h.GetSpirit().Value() == 0);
    // Not an object at all.
    savejson::ReadHero(JsonValue::Array(), s);
    CHECK(s.level == 1);
    CHECK(s.skillLevels.empty());
    CHECK(std::isnan(s.hp));
  }

  TEST_CASE("settings section: web shape, auto-loot mode by name, defaults for junk") {
    SaveSettings s;
    s.autoCombat = true;
    s.autoLootMode = AutoLootMode::Rare;
    JsonWriter w;
    savejson::WriteSettings(w, s);
    const std::string text = w.Take();
    CHECK(text == R"({"autoCombat":true,"musicVolume":0.5,"sfxVolume":0.7,"autoLootMode":"rare"})");
    JsonValue v;
    REQUIRE(ParseJson(text, v));
    SaveSettings back;
    savejson::ReadSettings(v, back);
    CHECK(back.autoCombat);
    CHECK(back.autoLootMode == AutoLootMode::Rare);
    CHECK(back.musicVolume == 0.5);
    CHECK(back.sfxVolume == 0.7);
    REQUIRE(ParseJson(R"({"autoCombat":1,"autoLootMode":"epic","musicVolume":0.25})", v));
    savejson::ReadSettings(v, back);
    CHECK_FALSE(back.autoCombat);
    CHECK(back.autoLootMode == AutoLootMode::Off);
    CHECK(back.musicVolume == 0.25);
    savejson::ReadSettings(JsonValue::Null(), back);
    CHECK(back.autoLootMode == AutoLootMode::Off);
    for (AutoLootMode m : {AutoLootMode::Off, AutoLootMode::All, AutoLootMode::Magic, AutoLootMode::Rare,
                           AutoLootMode::Legendary}) {
      s.autoLootMode = m;
      JsonWriter wm;
      savejson::WriteSettings(wm, s);
      REQUIRE(ParseJson(wm.Take(), v));
      savejson::ReadSettings(v, back);
      CHECK(back.autoLootMode == m);
    }
  }

  TEST_CASE("soulEcho section: null when absent, kept only with finite col / row (SoulEchoState.load)") {
    SaveSoulEcho e;
    {
      JsonWriter w;
      savejson::WriteSoulEcho(w, e);
      CHECK(w.Take() == "null");
    }
    e.present = true;
    e.mapId = "emerald_plains";
    e.col = 31.5;
    e.row = 40;
    e.gold = 120;
    e.exp = 7;
    JsonWriter w;
    savejson::WriteSoulEcho(w, e);
    const std::string text = w.Take();
    CHECK(text == R"({"mapId":"emerald_plains","col":31.5,"row":40,"gold":120,"exp":7})");
    JsonValue v;
    REQUIRE(ParseJson(text, v));
    SaveSoulEcho back;
    savejson::ReadSoulEcho(v, back);
    CHECK(back.present);
    CHECK(back.mapId == "emerald_plains");
    CHECK(back.col == 31.5);
    CHECK(back.row == 40);
    CHECK(back.gold == 120);
    CHECK(back.exp == 7);
    REQUIRE(ParseJson(R"({"mapId":"emerald_plains","col":"3","row":4,"gold":5,"exp":0})", v));
    savejson::ReadSoulEcho(v, back);
    CHECK_FALSE(back.present);
    savejson::ReadSoulEcho(JsonValue::Null(), back);
    CHECK_FALSE(back.present);
    REQUIRE(ParseJson(R"({"col":3,"row":4})", v));
    savejson::ReadSoulEcho(v, back);
    CHECK(back.present);
    CHECK(back.mapId.empty());
    CHECK(back.gold == 0);
  }
#endif
}
