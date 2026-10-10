// Pets area (quests+story+pets owner): ley-beast rules (PetSystem), the companion in the world (PetCompanion), the
// homestead / Ember Tower state (HomesteadTower, HomesteadSystem) and their save sections.
// Ports src/__tests__/PetSystem.test.ts and src/__tests__/Homestead.test.ts, plus the spec vectors of
// quests-story-ch1.md 18.14 and 4.4-4.7, and runtime checks of 18.5 (companion) against the real data.
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "SimHarness.h"
#include "abyss/base/Json.h"
#include "abyss/base/Math.h"
#include "abyss/base/StrUtil.h"
#include "abyss/combat/Combat.h"
#include "abyss/combat/HitFeedback.h"
#include "abyss/combat/Projectiles.h"
#include "abyss/combat/StatusEffects.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/hero/Rewards.h"
#include "abyss/items/Inventory.h"
#include "abyss/items/LootGen.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/pets/Homestead.h"
#include "abyss/pets/PetCompanion.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/save/SaveData.h"
#include "abyss/save/SaveIO.h"
#include "abyss/sim/GameSim.h"
#include "abyss/sim/Snapshot.h"
#include "abyss/world/Zone.h"
#include "doctest/doctest.h"
#include "../../Source/AbyssCore/Private/save/SaveSections.h"

using namespace abyss;

namespace {

const DataStore& PD() { return test::RealData(); }
const PetTables& PT() { return PD().Pets(); }
const HomesteadTables& HT() { return PD().Homestead(); }

const std::vector<std::string>& OriginalPetIds() {
  static const std::vector<std::string> ids = {"pet_sprite",  "pet_dragon",     "pet_owl",           "pet_cat",
                                               "pet_phoenix", "pet_storm_wolf", "pet_jade_tortoise", "pet_void_butterfly"};
  return ids;
}

const std::vector<std::string>& ChapterFinales() {
  static const std::vector<std::string> ids = {"q_secure_plains", "q_seal_dark_source", "q_kill_stone_guardian",
                                               "q_seal_fire_rift", "q_collect_demon_essence"};
  return ids;
}

// Log lines (keys) in emission order.
std::vector<std::string> PtLogKeys(const EventSink& s) {
  std::vector<std::string> out;
  for (const Event& e : s.Items()) {
    if (const EvLog* l = std::get_if<EvLog>(&e)) out.push_back(l->text.key);
  }
  return out;
}

std::vector<EvLog> PtLogs(const EventSink& s) {
  std::vector<EvLog> out;
  for (const Event& e : s.Items()) {
    if (const EvLog* l = std::get_if<EvLog>(&e)) out.push_back(*l);
  }
  return out;
}

template <class E>
std::vector<E> PtEvents(const EventSink& s) {
  std::vector<E> out;
  for (const Event& e : s.Items()) {
    if (const E* x = std::get_if<E>(&e)) out.push_back(*x);
  }
  return out;
}

// The web tests' levelTo: feeds exactly one level of exp at a time (silent).
void LevelTo(PetSystem& ps, std::string_view petId, int32_t level) {
  for (int guard = 0; guard < 100 && ps.Find(petId)->level < level; ++guard) {
    ps.AddExp(petId, static_cast<double>(PetExpToNext(PT().system, ps.Find(petId)->level)), true);
  }
}

std::vector<std::string> AbilityIds(const std::vector<const PetAbilityDef*>& list) {
  std::vector<std::string> out;
  for (const PetAbilityDef* a : list) out.push_back(a->id);
  return out;
}

const PetAbilityDef* PrimaryOf(std::string_view petId) { return PrimaryPetAbility(*PD().FindPet(petId)); }

// A fixed sequence repeated like the web tests' seq(...): scripts `count` values cycling through `values`.
void ScriptSeq(Rng& rng, std::initializer_list<double> values, size_t count) {
  std::vector<double> v;
  const std::vector<double> src(values);
  for (size_t i = 0; i < count; ++i) v.push_back(src[i % src.size()]);
  rng.Script(std::span<const double>(v));
}

JsonValue PtParse(std::string_view text) {
  JsonValue v;
  const bool ok = ParseJson(text, v);
  REQUIRE(ok);
  return v;
}

std::string PtJson(const std::function<void(JsonWriter&)>& write) {
  JsonWriter w;
  write(w);
  return w.Take();
}

std::string Render(const LocText& t) { return PD().Strings().T(t); }

// The web's choosePetAction test context (PetSystem.test.ts ctx()): owl abilities, target at 3, ready.
PetDecisionContext OwlCtx() {
  PetDecisionContext c;
  c.nowMs = 10000;
  c.abilities = UnlockedPetAbilities(*PD().FindPet("pet_owl"), 0);
  c.exhausted = false;
  c.peaceful = false;
  c.heroDist = 2;
  c.heroHpRatio = 1;
  c.heroAttackers = 0;
  c.hasTarget = true;
  c.targetDist = 3;
  c.targetMarked = false;
  c.enemiesNearTarget = 1;
  c.basicRange = 5;
  c.basicReadyAt = 0;
  return c;
}

// ---------------------------------------------------------------------------------------------------------------------
// Runtime rig: the systems the companion and the homestead talk to, around a SimHarness over the real data.
// ---------------------------------------------------------------------------------------------------------------------
struct PetRig {
  explicit PetRig(Vec2 heroPos = Vec2(40, 40), ClassId cls = ClassId::Warrior, uint64_t seed = 5)
      : h(seed),
        hero(std::make_unique<Hero>(h.ctx.data, cls)),
        status(h.ctx.data.Classes().statusRules),
        zone(h.ctx),
        monsters(h.ctx),
        projectiles(h.ctx),
        combat(h.ctx),
        rewards(h.ctx),
        inventory(h.ctx),
        quests(h.ctx.data, h.events, h.bus),
        homestead(h.ctx),
        pets(h.ctx.data, h.events, h.bus),
        companion(h.ctx) {
    SimSystems& s = h.ctx.sys;
    s.hero = hero.get();
    s.status = &status;
    s.zone = &zone;
    s.monsters = &monsters;
    s.projectiles = &projectiles;
    s.combat = &combat;
    s.rewards = &rewards;
    s.inventory = &inventory;
    s.quests = &quests;
    s.homestead = &homestead;
    s.pets = &pets;
    s.petCompanion = &companion;
    pets.SetBuildingLevelSource([this](std::string_view id) { return homestead.BuildingLevel(id); });
    pets.SetAwaySource([this](std::string_view id) { return homestead.IsPetAway(id); });
    h.onTimer = [this](const Timer& t) {
      switch (t.owner) {
        case TimerOwner::Combat: combat.OnTimer(t); break;
        case TimerOwner::Projectiles: projectiles.OnTimer(t); break;
        case TimerOwner::Monsters: monsters.OnTimer(t); break;
        case TimerOwner::Pets: companion.OnTimer(t); break;
        default: break;
      }
    };
    h.bus.Subscribe<MonsterKilledMsg>([this](const MonsterKilledMsg& m) { combat.OnMonsterKilled(m); });
    h.bus.Subscribe<MonsterKilledMsg>([this](const MonsterKilledMsg& m) { pets.OnKill(m.level); });
    h.bus.Subscribe<MonsterKilledMsg>([this](const MonsterKilledMsg& m) { homestead.OnMonsterKilled(m); });
    h.bus.Subscribe<MonsterKilledMsg>([this](const MonsterKilledMsg& m) { kills.push_back(m); });
    h.bus.Subscribe<PetChangedMsg>([this](const PetChangedMsg& m) {
      petChanges.push_back(m);
      companion.OnPetChanged(m);
    });
    h.bus.Subscribe<QuestTurnedInMsg>([this](const QuestTurnedInMsg& m) { homestead.OnQuestTurnedIn(m); });
    h.session.currentMap = "emerald_plains";
    ok = zone.EnterZone("emerald_plains", true, heroPos);
    hero->Skills().InitStarterLevels();
    hero->RecalcDerived(h.ctx.equip);
    hero->FillHpMana();
    combat.OnZoneEnter();
    companion.OnZoneEnter();
    h.events.Clear();
  }

  void Step(int n = 1) {
    for (int i = 0; i < n; ++i) {
      h.Step(1);
      hero->RecalcDerived(h.ctx.equip);
      companion.Tick(kSimStepMs);
    }
  }
  template <class Pred>
  bool StepUntil(Pred pred, int maxSteps = 600) {
    for (int i = 0; i < maxSteps; ++i) {
      if (pred()) return true;
      Step();
    }
    return pred();
  }

  EntityId Spawn(std::string_view defId, TilePos tile, MonsterState state = MonsterState::Chase) {
    MonsterSpawnParams p;
    p.baseDef = PD().FindMonster(defId);
    p.tile = tile;
    p.role = MonsterRole::Regular;
    const EntityId id = monsters.Spawn(p);
    if (MonsterInstance* m = monsters.Find(id)) m->state = state;
    return id;
  }

  // The beast at a fixed spot (tests that need exact distances).
  void PlacePet(Vec2 p) {
    companion.MutableStateForTesting().pos = p;
    companion.MutableStateForTesting().prevPos = p;
  }

  double Now() const { return h.clock.NowMs(); }
  Rng& PetRng() { return h.rng.Get(RngStream::Pets); }

  test::SimHarness h;
  std::unique_ptr<Hero> hero;
  StatusEffectSystem status;
  ZoneRuntime zone;
  MonsterSystem monsters;
  ProjectileSystem projectiles;
  CombatSystem combat;
  RewardService rewards;
  InventorySystem inventory;
  QuestSystem quests;
  HomesteadSystem homestead;
  PetSystem pets;
  PetCompanion companion;
  std::vector<MonsterKilledMsg> kills;
  std::vector<PetChangedMsg> petChanges;
  bool ok = false;
};

#define REQUIRE_MONSTER(id) REQUIRE((id) != kNoEntity)

}  // namespace

TEST_SUITE("pets") {
  // ===================================================================================================================
  // PetSystem.test.ts - ley-beast data (src/data/pets.ts)
  // ===================================================================================================================
  TEST_CASE("data: keeps the eight original pet ids (save compatibility)") {
    std::vector<std::string> ids;
    for (const PetDef& p : PT().pets) ids.push_back(p.id);
    std::vector<std::string> want = OriginalPetIds();
    std::sort(ids.begin(), ids.end());
    std::sort(want.begin(), want.end());
    CHECK(ids == want);
    CHECK(PT().maxLevel == 20);
    CHECK(PT().evolutionLevels == std::vector<int32_t>{10, 20});
    CHECK(PT().evolutionMult == std::vector<double>{1, 1.5, 2});
    CHECK(PT().maxBond == 5);
    CHECK(PT().leyFruitId == "c_ley_fruit");
    CHECK(PT().chapter1Slice == std::vector<std::string>{"pet_sprite"});
  }

  TEST_CASE("data: every beast has a base ability and one unlocked by awakening") {
    for (const PetDef& def : PT().pets) {
      CAPTURE(def.id);
      bool base = false, awakened = false;
      for (const PetAbilityDef& a : def.abilities) {
        base = base || (a.unlock == 0 && a.kind != PetAbilityKind::Revive);
        awakened = awakened || a.unlock == 1;
        CHECK(a.cooldownMs >= 0);
        if (a.kind == PetAbilityKind::Strike || a.kind == PetAbilityKind::Bolt || a.kind == PetAbilityKind::Cone) {
          CHECK(a.hasDamage);
          CHECK(a.damage > 0);
        }
      }
      CHECK(base);
      CHECK(awakened);
      REQUIRE(PrimaryPetAbility(def) != nullptr);
      CHECK(PrimaryPetAbility(def)->unlock == 0);
      CHECK(PrimaryPetAbility(def)->id == def.primaryAbilityId);
    }
  }

  TEST_CASE("data: ability ids are unique") {
    std::vector<std::string> ids;
    for (const PetDef& p : PT().pets) {
      for (const PetAbilityDef& a : p.abilities) ids.push_back(a.id);
    }
    std::vector<std::string> sorted = ids;
    std::sort(sorted.begin(), sorted.end());
    CHECK(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end());
  }

  TEST_CASE("data: design table - roles, passives and signature abilities") {
    auto expectRole = [](std::string_view id, PetRole role, Stat stat, PetAbilityKind kind) {
      const PetDef* d = PD().FindPet(id);
      REQUIRE(d != nullptr);
      CHECK(d->role == role);
      CHECK(d->passiveStat == stat);
      CHECK(PrimaryPetAbility(*d)->kind == kind);
    };
    expectRole("pet_sprite", PetRole::Support, Stat::ExpBonus, PetAbilityKind::Heal);
    expectRole("pet_owl", PetRole::Scout, Stat::MagicFind, PetAbilityKind::Mark);
    expectRole("pet_storm_wolf", PetRole::Melee, Stat::AttackSpeed, PetAbilityKind::Strike);
    expectRole("pet_cat", PetRole::Assassin, Stat::CritRate, PetAbilityKind::Strike);
    expectRole("pet_jade_tortoise", PetRole::Tank, Stat::Defense, PetAbilityKind::Taunt);
    expectRole("pet_dragon", PetRole::Ranged, Stat::DamagePercent, PetAbilityKind::Cone);
    expectRole("pet_phoenix", PetRole::Support, Stat::HpRegen, PetAbilityKind::Heal);
    expectRole("pet_void_butterfly", PetRole::Caster, Stat::ManaRegen, PetAbilityKind::Bolt);
    CHECK(PD().FindPet("pet_storm_wolf")->abilities[0].bleed > 0);
    CHECK(PD().FindPet("pet_cat")->abilities[0].crit);
    bool revive = false;
    for (const PetAbilityDef& a : PD().FindPet("pet_phoenix")->abilities) revive = revive || a.kind == PetAbilityKind::Revive;
    CHECK(revive);
    CHECK(PD().FindPet("pet_void_butterfly")->abilities[0].mana > 0);
    // 18.1 table: pet_sprite basic attack and HP.
    const PetDef* s = PD().FindPet("pet_sprite");
    CHECK(s->flying);
    CHECK(s->style == PetCombatStyle::Ranged);
    CHECK(s->range == doctest::Approx(4.5));
    CHECK(s->attackMs == doctest::Approx(1800));
    CHECK(s->color == 0x8ff0c0u);
    CHECK(s->element == DamageType::Arcane);
    CHECK(s->hpFraction == doctest::Approx(0.45));
    CHECK(PD().FindPet("pet_dragon")->abilities[0].arc == doctest::Approx(kPi / 3));
  }

  TEST_CASE("data: every name / description / ability / role / stat key exists in zh-CN and en") {
    std::vector<std::string> keys;
    for (const PetDef& def : PT().pets) {
      keys.push_back(def.nameKey);
      keys.push_back(def.descKey);
      keys.push_back(def.originKey);
      keys.push_back(StrCat("data.pet.role.", EnumName(def.role)));
      keys.push_back(StrCat("data.pet.stat.", EnumName(def.passiveStat)));
      for (const PetAbilityDef& a : def.abilities) {
        keys.push_back(StrCat("data.pet.ability.", a.id, ".name"));
        keys.push_back(StrCat("data.pet.ability.", a.id, ".desc"));
      }
    }
    for (const char* k : {"data.item.c_ley_fruit.name", "ui.pet.title", "sys.pet.evoName.1", "sys.pet.evoName.2",
                          "zone.pet.rareLabel", "sys.pet.obtained", "sys.pet.levelUp", "sys.pet.evolved",
                          "sys.pet.bondUp", "sys.pet.fed", "sys.pet.noFruit", "sys.pet.feedFull", "sys.pet.exhausted",
                          "sys.pet.revive", "sys.pet.rescue", "sys.pet.duplicate", "zone.pet.exhaustedTag",
                          "homestead.float.embers", "homestead.log.questEmbers", "homestead.log.wingUnlocked",
                          "homestead.log.towerUnlocked", "homestead.log.expeditionBack"}) {
      keys.push_back(k);
    }
    for (const std::string& k : keys) {
      CAPTURE(k);
      CHECK(PD().Strings().Lookup(LocaleId::ZhCN, k) != nullptr);
      CHECK(PD().Strings().Lookup(LocaleId::En, k) != nullptr);
    }
  }

  TEST_CASE("data: the ley fruit is a stackable consumable") {
    const ItemBaseDef* base = PD().FindItemBase(PT().leyFruitId);
    REQUIRE(base != nullptr);
    CHECK(base->type == ItemType::Consumable);
    CHECK(base->stackable);
  }

  // ===================================================================================================================
  // PetSystem ownership
  // ===================================================================================================================
  TEST_CASE("ownership: starts empty") {
    test::SimHarness h;
    PetSystem ps(h.ctx.data, h.events, h.bus);
    CHECK(ps.Owned().empty());
    CHECK(ps.Active() == nullptr);
    CHECK(ps.ActiveId().empty());
    CHECK(ps.Bonuses().Empty());
    CHECK(ps.PetDamage(100) == 0);
  }

  TEST_CASE("ownership: addPet grants a beast, auto-activates the first and emits PET_OBTAINED") {
    test::SimHarness h;
    PetSystem ps(h.ctx.data, h.events, h.bus);
    std::vector<PetChangedMsg> changes;
    h.bus.Subscribe<PetChangedMsg>([&](const PetChangedMsg& m) { changes.push_back(m); });
    CHECK(ps.AddPet("pet_owl"));
    CHECK(ps.AddPet("pet_cat", true));
    CHECK(ps.ActiveId() == "pet_owl");
    const PetInstance* cat = ps.Find("pet_cat");
    REQUIRE(cat != nullptr);
    CHECK(*cat == PetInstance{"pet_cat", 1, 0, 0, 0, 0});
    std::vector<std::pair<std::string, bool>> obtained;
    for (const EvPet& e : PtEvents<EvPet>(h.events)) {
      if (e.kind == EvPet::Kind::Obtained) obtained.emplace_back(e.petId, e.silent);
    }
    CHECK(obtained == std::vector<std::pair<std::string, bool>>{{"pet_owl", false}, {"pet_cat", true}});
    // One web PET_CHANGED per grant; only the grant that made a beast active moves the companion.
    size_t changed = 0;
    for (const EvPet& e : PtEvents<EvPet>(h.events)) changed += e.kind == EvPet::Kind::Changed ? 1 : 0;
    CHECK(changed == 2);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].petId == "pet_owl");
    CHECK(changes[0].reason == PetChangeReason::Obtained);
    // Logged unless silent.
    CHECK(PtLogKeys(h.events) == std::vector<std::string>{"sys.pet.obtained"});
  }

  TEST_CASE("ownership: rejects duplicates and unknown ids") {
    test::SimHarness h;
    PetSystem ps(h.ctx.data, h.events, h.bus);
    ps.AddPet("pet_owl");
    h.events.Clear();
    CHECK_FALSE(ps.AddPet("pet_owl"));
    CHECK(PtLogKeys(h.events) == std::vector<std::string>{"sys.pet.duplicate"});
    CHECK_FALSE(ps.AddPet("pet_unicorn"));
    CHECK(ps.Owned().size() == 1);
  }

  TEST_CASE("ownership: setActivePet only accepts owned beasts that are home") {
    test::SimHarness h;
    PetSystem ps(h.ctx.data, h.events, h.bus);
    std::vector<PetChangedMsg> changes;
    h.bus.Subscribe<PetChangedMsg>([&](const PetChangedMsg& m) { changes.push_back(m); });
    ps.AddPet("pet_owl");
    ps.AddPet("pet_cat");
    ps.SetActivePet("pet_dragon");
    CHECK(ps.ActiveId() == "pet_owl");
    ps.SetAwaySource([](std::string_view id) { return id == "pet_cat"; });
    ps.SetActivePet("pet_cat");
    CHECK(ps.ActiveId() == "pet_owl");
    ps.SetActivePet("pet_owl");  // same -> no event
    ps.SetActivePet("");
    CHECK(ps.ActiveId().empty());
    REQUIRE(changes.size() == 2);
    CHECK(changes[1].petId.empty());
    CHECK(changes[1].reason == PetChangeReason::Rested);
  }

  TEST_CASE("ownership: display name carries the evolution suffix") {
    test::SimHarness h;
    PetSystem ps(h.ctx.data, h.events, h.bus);
    ps.AddPet("pet_storm_wolf");
    PetInstance* inst = ps.FindMutableForTesting("pet_storm_wolf");
    const std::string base = Render(ps.DisplayName(*inst));
    inst->evolved = 1;
    const std::string evolved = Render(ps.DisplayName(*inst));
    CHECK(evolved != base);
    CHECK(evolved.find(base) != std::string::npos);
    inst->evolved = 2;
    CHECK(Render(ps.DisplayName(*inst)) != evolved);
    // Names fall back to the id when the key is missing (getPetDisplayName).
    CHECK(PetNameArg("name", PD(), "pet_sprite", 0).isKey);
    const std::string spriteName = PD().Strings().T("data.pet.pet_sprite.name");
    const std::string awakened =
        PD().Strings().T("sys.pet.evoName.1", std::vector<I18nArg>{{"name", spriteName, false}});
    CHECK(awakened.find(spriteName) == 0);
    CHECK(awakened.size() > spriteName.size());
    CHECK(Render(MakeLoc("sys.pet.obtained", {PetNameArg("name", PD(), "pet_sprite", 1)})) ==
          PD().Strings().T("sys.pet.obtained", std::vector<I18nArg>{{"name", awakened, false}}));
    CHECK(Render(MakeLoc("sys.pet.obtained", {PetNameArg("name", PD(), "pet_unknown", 0)})) ==
          PD().Strings().T("sys.pet.obtained", std::vector<I18nArg>{{"name", "pet_unknown", false}}));
  }

  // ===================================================================================================================
  // PetSystem growth: exp, level, evolution
  // ===================================================================================================================
  TEST_CASE("growth: exp curve and kill exp") {
    CHECK(PetExpToNext(PT().system, 1) == 100);
    CHECK(PetExpToNext(PT().system, 10) > PetExpToNext(PT().system, 9));
    CHECK(PetKillExp(PT().system, 20) > PetKillExp(PT().system, 1));
  }

  TEST_CASE("growth: levels up and carries overflow exp") {
    test::SimHarness h;
    PetSystem ps(h.ctx.data, h.events, h.bus);
    ps.AddPet("pet_sprite", true);
    ps.AddExp("pet_sprite", static_cast<double>(PetExpToNext(PT().system, 1) + 5));
    CHECK(ps.Find("pet_sprite")->level == 2);
    CHECK(ps.Find("pet_sprite")->exp == 5);
  }

  TEST_CASE("growth: evolves at 10 and 20, unlocking the second ability") {
    test::SimHarness h;
    PetSystem ps(h.ctx.data, h.events, h.bus);
    ps.AddPet("pet_sprite", true);
    CHECK(PetEvolutionForLevel(PT(), 9) == 0);
    CHECK(PetEvolutionForLevel(PT(), 10) == 1);
    CHECK(PetEvolutionForLevel(PT(), 20) == 2);
    LevelTo(ps, "pet_sprite", 10);
    CHECK(ps.Find("pet_sprite")->evolved == 1);
    const std::vector<std::string> ids = AbilityIds(ps.UnlockedAbilities("pet_sprite"));
    CHECK(std::find(ids.begin(), ids.end(), "sprite_ley_ward") != ids.end());
    LevelTo(ps, "pet_sprite", 20);
    CHECK(ps.Find("pet_sprite")->level == PT().maxLevel);
    CHECK(ps.Find("pet_sprite")->evolved == 2);
    CHECK(ps.AddExp("pet_sprite", 99999) == 0);
    CHECK(ps.Find("pet_sprite")->level == PT().maxLevel);
  }

  TEST_CASE("growth: base abilities only before evolution") {
    test::SimHarness h;
    PetSystem ps(h.ctx.data, h.events, h.bus);
    ps.AddPet("pet_sprite", true);
    CHECK(AbilityIds(ps.UnlockedAbilities("pet_sprite")) == std::vector<std::string>{"sprite_heal_pulse"});
    std::vector<PetAbilityKind> kinds;
    for (const PetAbilityDef* a : UnlockedPetAbilities(*PD().FindPet("pet_phoenix"), 0)) kinds.push_back(a->kind);
    CHECK(kinds == std::vector<PetAbilityKind>{PetAbilityKind::Heal, PetAbilityKind::Revive});
    CHECK(ps.UnlockedAbilities("pet_owl").empty());  // not owned
  }

  TEST_CASE("growth: a big exp grant can level several times and evolve at once") {
    test::SimHarness h;
    PetSystem ps(h.ctx.data, h.events, h.bus);
    ps.AddPet("pet_sprite", true);
    int64_t total = 0;
    for (int32_t l = 1; l < 12; ++l) total += PetExpToNext(PT().system, l);
    CHECK(ps.AddExp("pet_sprite", static_cast<double>(total), true) == 11);
    CHECK(ps.Find("pet_sprite")->evolved == 1);
  }

  TEST_CASE("growth: kills feed the active beast; resting beasts learn only with the moon well") {
    test::SimHarness h;
    PetSystem ps(h.ctx.data, h.events, h.bus);
    ps.AddPet("pet_sprite", true);
    ps.AddPet("pet_owl", true);
    int32_t well = 0;
    ps.SetBuildingLevelSource([&](std::string_view id) { return id == "pet_house" ? well : 0; });
    ps.OnKill(10);
    CHECK(ps.Find("pet_sprite")->exp == PetKillExp(PT().system, 10));
    CHECK(ps.Find("pet_owl")->exp == 0);
    well = 2;
    ps.OnKill(10);
    CHECK(ps.Find("pet_owl")->exp > 0);
    CHECK(ps.Find("pet_owl")->exp == 6);  // floor(20 x (0.2 + 0.1 x 1))
    // The moon well also speeds up the active beast (+10 % per level).
    CHECK(ps.ExpMultiplier() == doctest::Approx(1.2));
  }

  TEST_CASE("growth: grantRestingExp skips the active beast") {
    test::SimHarness h;
    PetSystem ps(h.ctx.data, h.events, h.bus);
    ps.AddPet("pet_sprite", true);
    ps.AddPet("pet_owl", true);
    ps.GrantRestingExp(40);
    CHECK(ps.Find("pet_sprite")->exp == 0);
    CHECK(ps.Find("pet_owl")->exp == 40);
  }

  TEST_CASE("growth: feeding gives exp and bond") {
    test::SimHarness h;
    PetSystem ps(h.ctx.data, h.events, h.bus);
    ps.AddPet("pet_sprite", true);
    CHECK(ps.Feed("pet_sprite"));
    const PetInstance* inst = ps.Find("pet_sprite");
    CHECK((inst->level > 1 || inst->exp >= static_cast<int64_t>(PT().system.feedExp) - PetExpToNext(PT().system, 1)));
    CHECK(inst->bondProgress > 0);
    CHECK_FALSE(ps.Feed("pet_owl"));
  }

  // ===================================================================================================================
  // PetSystem bond
  // ===================================================================================================================
  TEST_CASE("bond: capped at 3 without the moon well, 5 with it") {
    test::SimHarness h;
    PetSystem ps(h.ctx.data, h.events, h.bus);
    ps.AddPet("pet_cat", true);
    ps.AddBond("pet_cat", PT().system.bondProgressPerLevel * 10);
    CHECK(ps.Find("pet_cat")->bond == 3);
    ps.SetBuildingLevelSource([](std::string_view id) { return id == "pet_house" ? 2 : 0; });
    CHECK(ps.BondCap() == PT().maxBond);
    ps.AddBond("pet_cat", PT().system.bondProgressPerLevel * 10);
    CHECK(ps.Find("pet_cat")->bond == PT().maxBond);
    CHECK(ps.Find("pet_cat")->bondProgress == 0);
  }

  TEST_CASE("bond: active time builds bond") {
    test::SimHarness h;
    PetSystem ps(h.ctx.data, h.events, h.bus);
    ps.AddPet("pet_cat", true);
    ps.TickActive(60000.0 * 50);
    CHECK(ps.Find("pet_cat")->bond == 1);
    // Resting beasts build nothing.
    ps.SetActivePet("");
    ps.TickActive(60000.0 * 50);
    CHECK(ps.Find("pet_cat")->bond == 1);
  }

  TEST_CASE("bond: a fully fed max-level, max-bond beast refuses more fruit") {
    test::SimHarness h;
    PetSystem ps(h.ctx.data, h.events, h.bus);
    ps.AddPet("pet_cat", true);
    ps.SetBuildingLevelSource([](std::string_view) { return 5; });
    LevelTo(ps, "pet_cat", 20);
    ps.AddBond("pet_cat", 1000);
    CHECK_FALSE(ps.CanFeed("pet_cat"));
    CHECK_FALSE(ps.Feed("pet_cat"));
  }

  TEST_CASE("bond: multiplier +10 % per bond level") {
    CHECK(PetBondMultiplier(PT(), 0) == 1.0);
    CHECK(PetBondMultiplier(PT(), 5) == doctest::Approx(1.5));
    CHECK(PetBondMultiplier(PT(), 9) == doctest::Approx(1.5));  // clamped to 5
    CHECK(PetBondMultiplier(PT(), -2) == 1.0);
  }

  // ===================================================================================================================
  // PetSystem passive bonuses and damage
  // ===================================================================================================================
  TEST_CASE("passive: getBonuses reports only the active beast, scaled by level / evolution / bond") {
    test::SimHarness h;
    PetSystem ps(h.ctx.data, h.events, h.bus);
    ps.AddPet("pet_cat", true);
    ps.AddPet("pet_owl", true);
    const PetDef& cat = *PD().FindPet("pet_cat");
    StatBag want;
    want.Set(Stat::CritRate, cat.passiveBase);
    CHECK(ps.Bonuses() == want);
    PetInstance* inst = ps.FindMutableForTesting("pet_cat");
    const double lv1 = PetPassiveValue(PT(), cat, *inst);
    inst->level = 10;
    inst->evolved = 1;
    const double evo = PetPassiveValue(PT(), cat, *inst);
    CHECK(evo > lv1 * 1.5);
    inst->bond = 5;
    CHECK(PetPassiveValue(PT(), cat, *inst) == doctest::Approx(JsRound(evo * 1.5 * 10) / 10).epsilon(0.01));
    ps.SetActivePet("pet_owl");
    REQUIRE(ps.Bonuses().Size() == 1);
    CHECK(ps.Bonuses().Items()[0].stat == Stat::MagicFind);
    ps.SetActivePet("");
    CHECK(ps.Bonuses().Empty());
  }

  TEST_CASE("passive: pet attack is 5 % of hero damage at Lv1, capped at 15 % (x evolution)") {
    auto dmg = [](double d, int32_t level, int32_t evolved) {
      return PetAttackDamage(PT(), d, PetInstance{"pet_sprite", level, 0, evolved, 0, 0});
    };
    CHECK(dmg(100, 1, 0) == 5);
    CHECK(dmg(100, 10, 0) == 10);
    CHECK(dmg(100, 20, 0) == static_cast<int32_t>(100 * PT().system.damageMaxFraction));
    CHECK(dmg(100, 20, 2) == 30);
    CHECK(dmg(0, 5, 0) == 0);
    CHECK(dmg(3, 1, 0) == 0);
    CHECK(dmg(20, 1, 0) == 1);
  }

  TEST_CASE("passive: mergeBonuses adds overlapping stats") {
    StatBag a, b;
    a.Set(Stat::Str, 1);
    a.Set(Stat::Dex, 2);
    b.Set(Stat::Dex, 3);
    b.Set(Stat::Vit, 4);
    const StatBag m = MergeBonuses(a, b);
    REQUIRE(m.Size() == 3);
    CHECK(m.Items()[0] == StatValue{Stat::Str, 1});
    CHECK(m.Items()[1] == StatValue{Stat::Dex, 5});
    CHECK(m.Items()[2] == StatValue{Stat::Vit, 4});
    CHECK(LeyFruitDropChance(PT().system, true) == doctest::Approx(0.12));
    CHECK(LeyFruitDropChance(PT().system, false) == doctest::Approx(0.015));
  }

  // ===================================================================================================================
  // PetSystem save + migration (save-ui-input 3.3)
  // ===================================================================================================================
  TEST_CASE("save: round-trips through toSave / loadSave (JSON included)") {
    test::SimHarness h;
    PetSystem ps(h.ctx.data, h.events, h.bus);
    ps.AddPet("pet_dragon", true);
    ps.AddPet("pet_owl", true);
    LevelTo(ps, "pet_dragon", 12);
    ps.AddBond("pet_dragon", 150);
    ps.SetActivePet("pet_owl");
    SaveData saved;
    ps.WriteSave(saved);
    const std::string json = PtJson([&](JsonWriter& w) { savejson::WritePets(w, saved.pets); });
    SaveData back;
    savejson::ReadPets(PtParse(json), back.pets);
    PetSystem ps2(h.ctx.data, h.events, h.bus);
    std::vector<PetChangedMsg> changes;
    h.bus.Subscribe<PetChangedMsg>([&](const PetChangedMsg& m) { changes.push_back(m); });
    ps2.ReadSave(back);
    CHECK(std::equal(ps2.Owned().begin(), ps2.Owned().end(), ps.Owned().begin(), ps.Owned().end()));
    CHECK(ps2.ActiveId() == "pet_owl");
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].reason == PetChangeReason::Loaded);
    CHECK(changes[0].petId == "pet_owl");
  }

  TEST_CASE("save: migrates the old homestead pet list (pre ley-beast saves)") {
    SaveData old;
    savejson::ReadHomestead(PtParse(R"({"buildings": {"pet_house": 2}, "pets": [
        {"petId": "pet_sprite", "level": 5, "exp": 30},
        {"petId": "pet_dragon", "level": 12, "exp": 50, "evolved": 0},
        {"petId": "wolf", "level": 3, "exp": 15},
        {"petId": "pet_sprite", "level": 9, "exp": 1}], "activePet": "pet_dragon"})"),
                            old.homestead);
    test::SimHarness h;
    PetSystem ps(h.ctx.data, h.events, h.bus);
    ps.ReadSave(old);
    REQUIRE(ps.Owned().size() == 2);
    CHECK(ps.Owned()[0].petId == "pet_sprite");
    CHECK(ps.Owned()[1].petId == "pet_dragon");
    CHECK(*ps.Find("pet_sprite") == PetInstance{"pet_sprite", 5, 30, 0, 0, 0});
    CHECK(ps.Find("pet_dragon")->evolved == 1);  // evolution re-derived from the level
    CHECK(ps.ActiveId() == "pet_dragon");
  }

  TEST_CASE("save: drops an active pet that is not owned, and clamps junk values") {
    SaveData s;
    savejson::ReadHomestead(
        PtParse(R"({"buildings": {}, "pets": [{"petId": "pet_cat", "level": 99, "exp": -5}], "activePet": "pet_owl"})"),
        s.homestead);
    const PetSaveNormalized n = MigratePetSave(PD(), s);
    CHECK(n.active.empty());
    REQUIRE(n.owned.size() == 1);
    CHECK(n.owned[0].level == PT().maxLevel);
    CHECK(n.owned[0].exp == 0);
    CHECK(n.owned[0].evolved == 2);
    // Every clamp of 3.3: exp below expToNext, evolved >= the level's stage, bond 0..5, progress 0..99, junk -> default.
    SaveData j;
    savejson::ReadPets(PtParse(R"({"owned": [{"petId": "pet_owl", "level": 4.7, "exp": 9999, "evolved": 7, "bond": 12,
        "bondProgress": 250}, {"petId": "pet_cat", "level": "x", "exp": null, "bond": -3}, {"level": 3}, 7],
        "active": 5})"),
                       j.pets);
    const PetSaveNormalized m = MigratePetSave(PD(), j);
    REQUIRE(m.owned.size() == 2);
    CHECK(m.owned[0] == PetInstance{"pet_owl", 4, PetExpToNext(PT().system, 4) - 1, 2, 5, 99});
    CHECK(m.owned[1] == PetInstance{"pet_cat", 1, 0, 0, 0, 0});
    CHECK(m.active.empty());
  }

  TEST_CASE("save: prefers the new `pets` field over the legacy block") {
    SaveData s;
    savejson::ReadHomestead(
        PtParse(R"({"buildings": {}, "pets": [{"petId": "pet_cat", "level": 3, "exp": 0}], "activePet": "pet_cat"})"),
        s.homestead);
    savejson::ReadPets(PtParse(R"({"owned": [{"petId": "pet_owl", "level": 4, "exp": 10, "evolved": 0, "bond": 2,
        "bondProgress": 40}], "active": "pet_owl"})"),
                       s.pets);
    const PetSaveNormalized n = MigratePetSave(PD(), s);
    REQUIRE(n.owned.size() == 1);
    CHECK(n.owned[0].petId == "pet_owl");
    CHECK(n.owned[0].bond == 2);
    CHECK(n.active == "pet_owl");
    // `pets` without an `owned` array falls back to the legacy list, but its own (missing) active wins.
    SaveData f = s;
    savejson::ReadPets(PtParse(R"({"active": "pet_cat"})"), f.pets);
    const PetSaveNormalized fb = MigratePetSave(PD(), f);
    REQUIRE(fb.owned.size() == 1);
    CHECK(fb.owned[0].petId == "pet_cat");
    CHECK(fb.active == "pet_cat");
    savejson::ReadPets(PtParse(R"({})"), f.pets);
    CHECK(MigratePetSave(PD(), f).active.empty());
  }

  TEST_CASE("save: handles missing data") {
    SaveData empty;
    const PetSaveNormalized n = MigratePetSave(PD(), empty);
    CHECK(n.owned.empty());
    CHECK(n.active.empty());
    SaveData h;
    savejson::ReadHomestead(PtParse(R"({"buildings": {}})"), h.homestead);
    CHECK(MigratePetSave(PD(), h).owned.empty());
  }

  // ===================================================================================================================
  // pet combat decisions (choosePetAction)
  // ===================================================================================================================
  TEST_CASE("decide: rests while exhausted and follows in safe zones / when leashed") {
    PetDecisionContext c = OwlCtx();
    c.exhausted = true;
    CHECK(ChoosePetAction(c).kind == PetActionKind::Rest);
    c = OwlCtx();
    c.peaceful = true;
    CHECK(ChoosePetAction(c).kind == PetActionKind::Follow);
    c = OwlCtx();
    c.heroDist = 20;
    CHECK(ChoosePetAction(c).kind == PetActionKind::Follow);
    c = OwlCtx();
    c.hasTarget = false;
    CHECK(ChoosePetAction(c).kind == PetActionKind::Follow);
  }

  TEST_CASE("decide: uses a ready ability when useful, then falls back to the basic attack") {
    PetDecisionContext c = OwlCtx();
    const PetAction a = ChoosePetAction(c);
    REQUIRE(a.kind == PetActionKind::Ability);
    CHECK(a.ability->id == "owl_moon_mark");
    c.targetMarked = true;  // already marked -> basic attack
    CHECK(ChoosePetAction(c).kind == PetActionKind::Attack);
    c = OwlCtx();
    c.readyAt = {{"owl_moon_mark", 20000}};  // on cooldown -> basic attack
    CHECK(ChoosePetAction(c).kind == PetActionKind::Attack);
    c = OwlCtx();
    c.targetMarked = true;
    c.basicReadyAt = 20000;  // basic on cooldown -> wait
    CHECK(ChoosePetAction(c).kind == PetActionKind::Rest);
  }

  TEST_CASE("decide: closes in when the target is out of reach") {
    PetDecisionContext c = OwlCtx();
    c.targetDist = 12;
    c.targetMarked = true;
    CHECK(ChoosePetAction(c).kind == PetActionKind::Approach);
    c = OwlCtx();
    c.targetDist = 7;  // mark range 8: marked from range
    CHECK(ChoosePetAction(c).kind == PetActionKind::Ability);
  }

  TEST_CASE("decide: heals only when the hero needs it") {
    PetDecisionContext base = OwlCtx();
    base.abilities = UnlockedPetAbilities(*PD().FindPet("pet_sprite"), 1);
    base.basicRange = 4.5;
    CHECK(ChoosePetAction(base).kind == PetActionKind::Attack);
    PetDecisionContext low = base;
    low.heroHpRatio = 0.4;
    PetAction a = ChoosePetAction(low);
    REQUIRE(a.kind == PetActionKind::Ability);
    CHECK(a.ability->kind == PetAbilityKind::Heal);
    PetDecisionContext mob = base;  // shield when surrounded
    mob.heroHpRatio = 0.8;
    mob.heroAttackers = 2;
    a = ChoosePetAction(mob);
    REQUIRE(a.kind == PetActionKind::Ability);
    CHECK(a.ability->kind == PetAbilityKind::Shield);
    PetDecisionContext cd = base;  // heal on cooldown while low -> shield takes over
    cd.heroHpRatio = 0.4;
    cd.heroAttackers = 1;
    cd.readyAt = {{"sprite_heal_pulse", 99999}};
    a = ChoosePetAction(cd);
    REQUIRE(a.kind == PetActionKind::Ability);
    CHECK(a.ability->kind == PetAbilityKind::Shield);
  }

  TEST_CASE("decide: the tortoise taunts only when something is hitting the hero") {
    PetDecisionContext base = OwlCtx();
    base.abilities = UnlockedPetAbilities(*PD().FindPet("pet_jade_tortoise"), 0);
    base.basicRange = 1.4;
    base.targetDist = 1;
    CHECK(ChoosePetAction(base).kind == PetActionKind::Attack);
    base.heroAttackers = 1;
    const PetAction a = ChoosePetAction(base);
    REQUIRE(a.kind == PetActionKind::Ability);
    CHECK(a.ability->kind == PetAbilityKind::Taunt);
  }

  TEST_CASE("decide: breath needs the target in range") {
    const PetAbilityDef& breath = *UnlockedPetAbilities(*PD().FindPet("pet_dragon"), 0)[0];
    PetDecisionContext c = OwlCtx();
    c.targetDist = 3;
    CHECK(PetAbilityUseful(breath, c));
    c.targetDist = 6;
    CHECK_FALSE(PetAbilityUseful(breath, c));
    c.targetDist = 3;
    c.enemiesNearTarget = 0;
    CHECK_FALSE(PetAbilityUseful(breath, c));
  }

  TEST_CASE("decide: never picks the passive revive") {
    PetDecisionContext c = OwlCtx();
    c.abilities = UnlockedPetAbilities(*PD().FindPet("pet_phoenix"), 0);
    c.heroHpRatio = 0.1;
    c.readyAt = {{"phoenix_ember_mend", 99999}};
    const PetAction a = ChoosePetAction(c);
    CHECK((a.kind != PetActionKind::Ability || a.ability->kind != PetAbilityKind::Revive));
  }

  TEST_CASE("decide: max bond rescue below 30 % HP, once per minute") {
    const double cd = PT().system.bondRescueCooldownMs;
    CHECK(ShouldBondRescue(PT(), 5, 0.2, 100000, -1e300));
    CHECK_FALSE(ShouldBondRescue(PT(), 4, 0.2, 100000, -1e300));
    CHECK_FALSE(ShouldBondRescue(PT(), 5, 0.5, 100000, -1e300));
    CHECK_FALSE(ShouldBondRescue(PT(), 5, 0, 100000, -1e300));
    CHECK_FALSE(ShouldBondRescue(PT(), 5, 0.2, 100000, 100000 - cd + 1));
    CHECK(ShouldBondRescue(PT(), 5, 0.2, 100000, 100000 - cd));
  }

  // ===================================================================================================================
  // Spec vectors (quests-story-ch1.md 18.14)
  // ===================================================================================================================
  TEST_CASE("18.14 #1: exp curve, cumulative table, kill exp") {
    const PetSystemConstants& c = PT().system;
    CHECK(PetExpToNext(c, 1) == 100);
    CHECK(PetExpToNext(c, 9) == 420);
    CHECK(PetExpToNext(c, 19) == 820);
    const int64_t cumulative[] = {0, 100, 240, 420, 640, 900, 1200, 1540, 1920, 2340,
                                  2800, 3300, 3840, 4420, 5040, 5700, 6400, 7140, 7920, 8740};
    int64_t sum = 0;
    for (int32_t l = 1; l <= 20; ++l) {
      CHECK(sum == cumulative[l - 1]);
      sum += PetExpToNext(c, l);
    }
    CHECK(PetKillExp(c, 1) == 11);
    CHECK(PetKillExp(c, 3) == 13);
    CHECK(PetKillExp(c, 5) == 15);
    CHECK(PetKillExp(c, 6) == 16);
    CHECK(PetKillExp(c, -2) == 10);
  }

  TEST_CASE("18.14 #2: addExp across the awakening logs levelUp then evolved, one PET_CHANGED") {
    test::SimHarness h;
    PetSystem ps(h.ctx.data, h.events, h.bus);
    std::vector<PetChangedMsg> changes;
    h.bus.Subscribe<PetChangedMsg>([&](const PetChangedMsg& m) { changes.push_back(m); });
    ps.AddPet("pet_sprite", true);
    PetInstance* p = ps.FindMutableForTesting("pet_sprite");
    p->level = 9;
    p->exp = 400;
    h.events.Clear();
    changes.clear();
    CHECK(ps.AddExp("pet_sprite", 30) == 1);
    CHECK(p->level == 10);
    CHECK(p->exp == 10);
    CHECK(p->evolved == 1);
    const std::vector<EvLog> logs = PtLogs(h.events);
    REQUIRE(logs.size() == 2);
    CHECK(logs[0].text.key == "sys.pet.levelUp");
    CHECK(Render(logs[0].text) == PD().Strings().T("sys.pet.levelUp", std::vector<I18nArg>{
                                                                      {"name", PD().Strings().T("data.pet.pet_sprite.name"), false},
                                                                      {"level", "10", false}}));
    CHECK(logs[1].text.key == "sys.pet.evolved");
    const std::string sprite = PD().Strings().T("data.pet.pet_sprite.name");
    const std::string awakened = PD().Strings().T("sys.pet.evoName.1", std::vector<I18nArg>{{"name", sprite, false}});
    CHECK(Render(logs[1].text) == PD().Strings().T("sys.pet.evolved", std::vector<I18nArg>{{"name", sprite, false},
                                                                                           {"evolvedName", awakened, false}}));
    size_t changed = 0;
    for (const EvPet& e : PtEvents<EvPet>(h.events)) changed += e.kind == EvPet::Kind::Changed ? 1 : 0;
    CHECK(changed == 1);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].reason == PetChangeReason::Evolved);
    // L19 + 5000 -> L20 with exp 0; L20 takes nothing more.
    p->level = 19;
    p->exp = 0;
    ps.AddExp("pet_sprite", 5000, true);
    CHECK(p->level == 20);
    CHECK(p->exp == 0);
    CHECK(ps.AddExp("pet_sprite", 50) == 0);
    CHECK(p->level == 20);
  }

  TEST_CASE("18.14 #3: passive value of pet_sprite") {
    const PetDef& s = *PD().FindPet("pet_sprite");
    auto v = [&](int32_t level, int32_t evolved, int32_t bond) {
      return PetPassiveValue(PT(), s, PetInstance{"pet_sprite", level, 0, evolved, bond, 0});
    };
    CHECK(v(1, 0, 0) == doctest::Approx(3.0));
    CHECK(v(5, 0, 3) == doctest::Approx(6.0));
    CHECK(v(9, 0, 3) == doctest::Approx(8.1));
    CHECK(v(10, 1, 3) == doctest::Approx(12.9));
    CHECK(v(20, 2, 5) == doctest::Approx(31.8));
  }

  TEST_CASE("18.14 #4: petAttackDamage(D, L, evo)") {
    auto d = [](double dmg, int32_t level, int32_t evolved) {
      return PetAttackDamage(PT(), dmg, PetInstance{"pet_sprite", level, 0, evolved, 0, 0});
    };
    CHECK(d(18, 1, 0) == 0);
    CHECK(d(20, 1, 0) == 1);
    CHECK(d(40, 5, 0) == 3);
    CHECK(d(60, 10, 1) == 9);
    CHECK(d(100, 20, 2) == 30);
    CHECK(d(23.6, 1, 0) == 1);  // QP3: a Lv3 warrior's sprite
  }

  TEST_CASE("18.14 #5: bond with cap 3") {
    test::SimHarness h;
    PetSystem ps(h.ctx.data, h.events, h.bus);
    ps.AddPet("pet_sprite", true);
    PetInstance* p = ps.FindMutableForTesting("pet_sprite");
    p->bond = 2;
    p->bondProgress = 95;
    CHECK(ps.AddBond("pet_sprite", 10) == 1);
    CHECK(p->bond == 3);
    CHECK(p->bondProgress == 0);
    CHECK(ps.AddBond("pet_sprite", 5) == 0);
    CHECK(p->bondProgress == 0);
    p->level = 20;
    CHECK_FALSE(ps.CanFeed("pet_sprite"));
  }

  TEST_CASE("18.14 #6: feeding at Lv1 -> Lv2, exp 20, bondProgress 20; logs fed then levelUp") {
    test::SimHarness h;
    PetSystem ps(h.ctx.data, h.events, h.bus);
    ps.AddPet("pet_sprite", true);
    h.events.Clear();
    CHECK(ps.Feed("pet_sprite"));
    const PetInstance* p = ps.Find("pet_sprite");
    CHECK(p->level == 2);
    CHECK(p->exp == 20);
    CHECK(p->bondProgress == 20);
    CHECK(PtLogKeys(h.events) == std::vector<std::string>{"sys.pet.fed", "sys.pet.levelUp"});
  }

  TEST_CASE("18.14 #7: kill exp with an active pet_sprite Lv1 (expBonus at the kill)") {
    test::SimHarness h;
    PetSystem ps(h.ctx.data, h.events, h.bus);
    ps.AddPet("pet_sprite", true);
    auto killExp = [&](double expReward) {
      return std::floor(expReward * (1 + ps.Bonuses().Get(Stat::ExpBonus) / 100.0));
    };
    CHECK(killExp(12) == 12);
    CHECK(killExp(18) == 18);
    CHECK(killExp(55) == 56);
    CHECK(killExp(90) == 92);
    ps.SetActivePet("");
    CHECK(killExp(55) == 55);
    CHECK(killExp(90) == 90);
  }

  TEST_CASE("18.14 #8: choosePetAction for pet_sprite Lv1 (abilities [heal]), now 0") {
    PetDecisionContext c;
    c.nowMs = 0;
    c.abilities = UnlockedPetAbilities(*PD().FindPet("pet_sprite"), 0);
    c.heroHpRatio = 0.65;
    c.heroDist = 2;
    c.basicRange = 4.5;
    PetAction a = ChoosePetAction(c);
    REQUIRE(a.kind == PetActionKind::Ability);
    CHECK(a.ability->id == "sprite_heal_pulse");
    c.readyAt = {{"sprite_heal_pulse", 12000}};
    c.hasTarget = true;
    c.targetDist = 4.0;
    CHECK(ChoosePetAction(c).kind == PetActionKind::Attack);
    c.basicReadyAt = 1800;
    CHECK(ChoosePetAction(c).kind == PetActionKind::Rest);
    c.targetDist = 5.0;
    CHECK(ChoosePetAction(c).kind == PetActionKind::Approach);
    c.hasTarget = false;
    CHECK(ChoosePetAction(c).kind == PetActionKind::Follow);
    c.hasTarget = true;
    c.targetDist = 4.0;
    c.peaceful = true;
    CHECK(ChoosePetAction(c).kind == PetActionKind::Follow);
    c.peaceful = false;
    c.heroDist = 11.5;
    CHECK(ChoosePetAction(c).kind == PetActionKind::Follow);
  }

  TEST_CASE("18.5.7: flying beasts release their cast at 161 ms; art ids") {
    const ActionTiming t =
        ComputeCastTiming(PD().Combat().anim, PD().Assets(), PetArtId("pet_sprite", 0), AnimRig::Flying);
    CHECK(t.contactMs == doctest::Approx(161));
    CHECK(PetArtId("pet_sprite", 0) == "beast_pet_sprite");
    CHECK(PetArtId("pet_sprite", 1) == "beast_pet_sprite_e1");
    CHECK(PetArtId("pet_sprite", 2) == "beast_pet_sprite_e2");
    CHECK(PetAbilityVfxId(*PrimaryOf("pet_sprite"), DamageType::Arcane) == "life_regen");
    CHECK(PetAbilityVfxId(PD().FindPet("pet_jade_tortoise")->abilities[1], DamageType::Physical) == "war_stomp");
  }

  // ===================================================================================================================
  // Homestead.test.ts - Ember Tower unlocks
  // ===================================================================================================================
  TEST_CASE("tower: ties every story wing to an existing main quest and an ally NPC") {
    std::vector<std::string> unlocks;
    for (const BuildingDef& b : HT().buildings) {
      if (b.unlockQuest.empty()) continue;
      CAPTURE(b.id);
      const QuestDef* q = PD().FindQuest(b.unlockQuest);
      REQUIRE(q != nullptr);
      CHECK(q->category == QuestCategory::Main);
      CHECK(PD().FindNpc(b.allyNpc) != nullptr);
      unlocks.push_back(b.unlockQuest);
    }
    std::vector<std::string> finales = ChapterFinales();
    std::sort(unlocks.begin(), unlocks.end());
    std::sort(finales.begin(), finales.end());
    CHECK(unlocks == finales);
    CHECK(HT().FindBuilding("altar")->unlockQuest == "q_collect_demon_essence");
    CHECK(HT().FindBuilding("warehouse")->unlockQuest.empty());
    CHECK(HT().towerUnlockQuest == "q_explore_goblin_camp");
    CHECK(HT().towerZoneId == "ember_tower");
  }

  TEST_CASE("tower: opens after the camp quest and each wing after its chapter finale") {
    HomesteadTower t(HT());
    CHECK_FALSE(t.TowerUnlocked());
    CHECK(t.IsBuildingUnlocked("warehouse"));
    CHECK_FALSE(t.IsBuildingUnlocked("herb_garden"));
    CHECK_FALSE(t.IsBuildingUnlocked("nope"));
    const std::vector<std::string> first = {"q_kill_slimes", "q_kill_goblins", HT().towerUnlockQuest};
    CHECK(t.SyncUnlocks(first).empty());
    CHECK(t.TowerUnlocked());
    const std::vector<std::string> second = {HT().towerUnlockQuest, "q_secure_plains"};
    CHECK(t.SyncUnlocks(second) == std::vector<std::string>{"herb_garden"});
    CHECK(t.IsBuildingUnlocked("herb_garden"));
    CHECK(t.BuildingLevel("herb_garden") == 1);  // the ally restores the wing: Lv1 for free
    CHECK_FALSE(t.IsBuildingUnlocked("pet_house"));
    std::vector<std::string> all = {HT().towerUnlockQuest};
    for (const std::string& q : ChapterFinales()) all.push_back(q);
    std::vector<std::string> fresh = t.SyncUnlocks(all);
    std::sort(fresh.begin(), fresh.end());
    CHECK(fresh == std::vector<std::string>{"altar", "gem_workshop", "pet_house", "training_ground"});
    CHECK(t.SyncUnlocks(all).empty());  // syncing again reports nothing new
    CHECK(t.BuildingLevel("warehouse") == 0);  // not a story wing
  }

  TEST_CASE("tower: keeps an old save's building levels and derives its unlocks from completed quests") {
    HomesteadTower t(HT());
    SaveHomestead old;
    old.buildings = {{"herb_garden", 4}, {"gem_workshop", 2}, {"altar", 0}};
    t.Load(old);
    const std::vector<std::string> done = {HT().towerUnlockQuest, "q_secure_plains", "q_seal_dark_source",
                                           "q_kill_stone_guardian"};
    t.SyncUnlocks(done);
    CHECK(t.BuildingLevel("herb_garden") == 4);
    CHECK(t.BuildingLevel("gem_workshop") == 2);
    CHECK(t.BuildingLevel("pet_house") == 1);
    CHECK_FALSE(t.IsBuildingUnlocked("altar"));
    CHECK(t.BuildingLevel("altar") == 0);
  }

  TEST_CASE("tower: draws each wing as ruin, restored or thriving") {
    const BuildingDef& garden = *HT().FindBuilding("herb_garden");
    const BuildingDef& altar = *HT().FindBuilding("altar");
    CHECK(BuildingStage(garden, 3, false) == 0);
    CHECK(BuildingStage(garden, 0, true) == 0);
    CHECK(BuildingStage(garden, 1, true) == 1);
    CHECK(BuildingStage(garden, 2, true) == 1);
    CHECK(BuildingStage(garden, 3, true) == 2);
    CHECK(BuildingStage(altar, 1, true) == 1);
    CHECK(BuildingStage(altar, 2, true) == 2);
  }

  // ===================================================================================================================
  // upgrade costs (gold + embers)
  // ===================================================================================================================
  TEST_CASE("upgrade: charges embers and returns the gold cost") {
    HomesteadTower t(HT());
    const std::vector<std::string> done = {HT().towerUnlockQuest, "q_secure_plains"};
    t.SyncUnlocks(done);
    BuildingLevelCost c;
    REQUIRE(t.UpgradeCost("herb_garden", c));
    CHECK(c.gold == 250);
    CHECK(c.embers == 5);
    CHECK_FALSE(t.CanUpgrade("herb_garden", 1000));  // no embers yet
    t.AddEmbers(7);
    CHECK_FALSE(t.CanUpgrade("herb_garden", 100));  // not enough gold
    CHECK(t.CanUpgrade("herb_garden", 250));
    CHECK(t.Upgrade("herb_garden") == 250);
    CHECK(t.BuildingLevel("herb_garden") == 2);
    CHECK(t.Embers() == 2);
  }

  TEST_CASE("upgrade: refuses locked wings and maxed wings") {
    HomesteadTower t(HT());
    t.AddEmbers(999);
    CHECK_FALSE(t.CanUpgrade("altar", 99999));
    CHECK(t.Upgrade("altar") == 0);
    t.SetBuildingLevel("warehouse", 5);
    BuildingLevelCost c;
    CHECK_FALSE(t.UpgradeCost("warehouse", c));
    CHECK(t.Upgrade("warehouse") == 0);
    t.SetBuildingLevel("warehouse", 0);  // always open; the first levels are gold only
    REQUIRE(t.UpgradeCost("warehouse", c));
    CHECK(c.gold == 100);
    CHECK(c.embers == 0);
  }

  TEST_CASE("upgrade: keeps the old building bonuses") {
    HomesteadTower t(HT());
    t.SetBuildings({{"herb_garden", 2}, {"training_ground", 3}, {"gem_workshop", 1}, {"warehouse", 2}, {"altar", 1}});
    const StatBag b = t.BuildingBonuses();
    CHECK(b.Get(Stat::PotionDiscount) == 10);
    CHECK(b.Get(Stat::MercExpBonus) == 15);
    CHECK(b.Get(Stat::GemBonus) == 2);
    CHECK(b.Get(Stat::StashSlots) == 20);
    CHECK(b.Get(Stat::AltarBonus) == 3);
    CHECK(t.TrainingGroundBonus() == 15);
  }

  // ===================================================================================================================
  // embers (4.4)
  // ===================================================================================================================
  TEST_CASE("embers: come from bosses, mini-bosses, affixed elites and quests") {
    CHECK(EmbersForKill(HT(), true, false, 0) == 5);
    CHECK(EmbersForKill(HT(), true, true, 1) == 5);  // elite first (Q16)
    CHECK(EmbersForKill(HT(), false, true, 0) == 3);
    CHECK(EmbersForKill(HT(), false, false, 2) == 1);
    CHECK(EmbersForKill(HT(), false, false, 0) == 0);
    CHECK(EmbersForQuest(HT(), QuestCategory::Main, false, 0) == 2);
    CHECK(EmbersForQuest(HT(), QuestCategory::Side, false, 0) == 1);
    CHECK(EmbersForQuest(HT(), QuestCategory::Main, true, 15) == 15);
    for (const std::string& id : ChapterFinales()) {
      CAPTURE(id);
      const QuestDef* q = PD().FindQuest(id);
      REQUIRE(q != nullptr);
      CHECK(q->rewards.hasEmbers);
      CHECK(q->rewards.embers > 0);
    }
    HomesteadTower t(HT());
    CHECK(t.AddEmbers(-3) == 0);
    CHECK(t.AddEmbers(5) == 5);
    CHECK(t.Embers() == 5);
  }

  TEST_CASE("embers: the Chapter 1 quests pay 20 in total (4.4.1)") {
    int32_t total = 0;
    for (const QuestDef& q : PD().Quests().quests) {
      if (q.zone != "emerald_plains") continue;
      total += EmbersForQuest(HT(), q.category, q.rewards.hasEmbers, q.rewards.embers);
    }
    CHECK(total == 20);
  }

  // ===================================================================================================================
  // herb garden (4.7.1)
  // ===================================================================================================================
  TEST_CASE("garden: one item every few kills, faster with level, up to its capacity") {
    CHECK(GardenInterval(HT(), 1) == 14);
    CHECK(GardenInterval(HT(), 5) == 6);
    CHECK(GardenInterval(HT(), 9) == 6);
    CHECK(GardenCapacity(HT(), 0) == 0);
    CHECK(GardenCapacity(HT(), 1) == 8);
    CHECK(GardenCapacity(HT(), 2) == 12);
    for (int32_t lv = 0; lv < static_cast<int32_t>(HT().gardenIntervalByLevel.size()); ++lv) {
      CHECK(GardenInterval(HT(), lv) == HT().gardenIntervalByLevel[static_cast<size_t>(lv)]);
      CHECK(GardenCapacity(HT(), lv) == HT().gardenCapacityByLevel[static_cast<size_t>(lv)]);
    }
    HomesteadTower t(HT());
    Rng rng(1);
    for (int i = 0; i < 30; ++i) CHECK(t.OnKillGarden(rng).empty());  // locked: nothing grows
    CHECK(t.Garden().progress == 0);
    const std::vector<std::string> done = {"q_secure_plains"};
    t.SyncUnlocks(done);
    ScriptSeq(rng, {0.9, 0.1}, 64);  // potion, hp
    std::vector<std::string> grown;
    for (int i = 0; i < 14; ++i) {
      const std::string g = t.OnKillGarden(rng);
      if (!g.empty()) grown.push_back(g);
    }
    CHECK(grown == std::vector<std::string>{"c_hp_potion_s"});
    CHECK(t.GardenStockCount() == 1);
    for (int i = 0; i < 14 * 20; ++i) t.OnKillGarden(rng);
    CHECK(t.GardenStockCount() == GardenCapacity(HT(), 1));
    const int32_t progress = t.Garden().progress;
    t.OnKillGarden(rng);
    CHECK(t.Garden().progress == progress);  // full: progress does not advance
  }

  TEST_CASE("garden: rolls ley fruit and better potions as it grows") {
    Rng rng(1);
    rng.Script({0.05});
    CHECK(RollGardenYield(HT(), 1, rng) == PT().leyFruitId);
    rng.Script({0.9, 0.1});
    CHECK(RollGardenYield(HT(), 1, rng) == "c_hp_potion_s");
    rng.Script({0.9, 0.1});
    CHECK(RollGardenYield(HT(), 3, rng) == "c_hp_potion_m");
    rng.Script({0.9, 0.1});
    CHECK(RollGardenYield(HT(), 5, rng) == "c_hp_potion_l");
    rng.Script({0.9, 0.9});
    CHECK(RollGardenYield(HT(), 5, rng) == "c_mp_potion_m");
    rng.Script({0.9, 0.9});
    CHECK(RollGardenYield(HT(), 1, rng) == "c_mp_potion_s");
    // P(ley fruit) = 0.12 + 0.03 lv: the boundary at lv 1 is 0.15.
    rng.Script({0.1499, 0.15, 0.1});
    CHECK(RollGardenYield(HT(), 1, rng) == PT().leyFruitId);
    CHECK(RollGardenYield(HT(), 1, rng) == "c_hp_potion_s");
    for (const char* id : {"c_hp_potion_s", "c_hp_potion_m", "c_hp_potion_l", "c_mp_potion_s", "c_mp_potion_m"}) {
      CHECK(PD().FindItemBase(id) != nullptr);
    }
  }

  TEST_CASE("garden: harvests everything and takes back what did not fit") {
    HomesteadTower t(HT());
    t.Garden().stock = {{"c_hp_potion_s", 3}, {PT().leyFruitId, 1}};
    const std::vector<std::pair<std::string, int32_t>> h = t.Harvest();
    CHECK(h == std::vector<std::pair<std::string, int32_t>>{{"c_hp_potion_s", 3}, {PT().leyFruitId, 1}});
    CHECK(t.GardenStockCount() == 0);
    t.ReturnToGarden("c_hp_potion_s", 2);
    t.ReturnToGarden("c_mp_potion_s", 0);
    CHECK(t.Garden().stock == std::vector<std::pair<std::string, int32_t>>{{"c_hp_potion_s", 2}});
  }

  // ===================================================================================================================
  // gem workshop
  // ===================================================================================================================
  TEST_CASE("gems: three combine into the next tier, capped by the workshop level and hero level") {
    CHECK(NextGemId(PD(), "g_ruby_1") == "g_ruby_2");
    CHECK(NextGemId(PD(), "g_ruby_3").empty());
    CHECK(NextGemId(PD(), "g_diamond_4") == "g_diamond_5");
    CHECK(NextGemId(PD(), "m_scrap").empty());
    CHECK(NextGemId(PD(), "g_ruby_12").empty());
    CHECK(MaxCombineTier(0) == 0);
    CHECK(MaxCombineTier(1) == 2);
    CHECK(MaxCombineTier(5) == 5);
    CHECK(GemCombineBlockFor(PD(), "g_ruby_1", 3, 0, 20, 9999) == GemCombineBlock::Workshop);
    CHECK(GemCombineBlockFor(PD(), "g_ruby_1", 3, 1, 10, 9999) == GemCombineBlock::Level);  // g_ruby_2 needs Lv15
    CHECK(GemCombineBlockFor(PD(), "g_ruby_1", 2, 1, 20, 9999) == GemCombineBlock::Count);
    CHECK(GemCombineBlockFor(PD(), "g_ruby_1", 3, 1, 20, 10) == GemCombineBlock::Gold);
    CHECK(GemCombineBlockFor(PD(), "g_ruby_1", 3, 1, 20, GemCombineGold(2)) == GemCombineBlock::None);
    CHECK(GemCombineBlockFor(PD(), "g_ruby_2", 3, 1, 40, 9999) == GemCombineBlock::Workshop);  // tier 3 needs Lv2
    CHECK(GemCombineBlockFor(PD(), "g_ruby_2", 3, 2, 40, 9999) == GemCombineBlock::None);
    CHECK(GemCombineBlockFor(PD(), "g_ruby_3", 9, 5, 50, 9999) == GemCombineBlock::NoNext);
    CHECK(GemCombineGold(2) == 160);
  }

  // ===================================================================================================================
  // caravan expeditions
  // ===================================================================================================================
  TEST_CASE("expedition: sends one idle pet at a time") {
    HomesteadTower locked(HT());
    const std::vector<std::string> owl = {"pet_owl"};
    CHECK(locked.ExpeditionBlockFor("pet_owl", owl, "", "short") == ExpeditionBlock::Locked);
    HomesteadTower t(HT());
    const std::vector<std::string> done = {"q_seal_fire_rift"};
    t.SyncUnlocks(done);
    t.SetBuildingLevel("training_ground", 1);
    const std::vector<std::string> none;
    CHECK(t.ExpeditionBlockFor("pet_owl", none, "", "short") == ExpeditionBlock::NoPet);
    CHECK(t.ExpeditionBlockFor("pet_owl", owl, "pet_owl", "short") == ExpeditionBlock::Active);
    CHECK(t.ExpeditionBlockFor("pet_owl", owl, "", "nowhere") == ExpeditionBlock::Option);
    const std::vector<std::string> both = {"pet_owl", "pet_cat"};
    CHECK(t.SendExpedition("pet_owl", "short", both, "pet_cat"));
    CHECK(t.IsPetAway("pet_owl"));
    CHECK(t.ExpeditionBlockFor("pet_cat", both, "", "short") == ExpeditionBlock::Busy);
  }

  TEST_CASE("expedition: comes back after enough kills") {
    HomesteadTower t(HT());
    const std::vector<std::string> done = {"q_seal_fire_rift"};
    t.SyncUnlocks(done);
    t.SetBuildingLevel("training_ground", 2);
    const std::vector<std::string> owl = {"pet_owl"};
    REQUIRE(t.SendExpedition("pet_owl", "short", owl, ""));
    int32_t need = 0;
    for (const ExpeditionOptionDef& o : HT().expeditionOptions) {
      if (o.id == "short") need = o.killsRequired;
    }
    CHECK(need == 25);
    Rng rng(3);
    CHECK_FALSE(t.ClaimExpedition(20, rng).has_value());
    bool back = false;
    for (int32_t i = 0; i < need; ++i) back = t.OnKillExpedition() || back;
    CHECK(back);
    CHECK_FALSE(t.OnKillExpedition());  // already home
    rng.Script({0.5, 0.5});
    const std::optional<ClaimedExpedition> r = t.ClaimExpedition(20, rng);
    REQUIRE(r.has_value());
    CHECK(r->petId == "pet_owl");
    CHECK(r->reward.embers == 8);  // 4 + 2 x post level 2
    CHECK(r->reward.gold > 0);
    CHECK_FALSE(r->reward.items.empty());
    CHECK(t.Embers() == 8);
    CHECK_FALSE(t.Expedition().has_value());
    CHECK_FALSE(t.IsPetAway("pet_owl"));
  }

  TEST_CASE("expedition: comes back when its time runs out") {
    HomesteadTower t(HT());
    const std::vector<std::string> done = {"q_seal_fire_rift"};
    t.SyncUnlocks(done);
    t.SetBuildingLevel("training_ground", 1);
    const std::vector<std::string> cat = {"pet_cat"};
    REQUIRE(t.SendExpedition("pet_cat", "long", cat, ""));
    double duration = 0;
    for (const ExpeditionOptionDef& o : HT().expeditionOptions) {
      if (o.id == "long") duration = o.durationMs;
    }
    CHECK_FALSE(t.Tick(duration - 1).expeditionReturned);
    CHECK(t.Tick(1).expeditionReturned);
    CHECK(t.ExpeditionDone());
  }

  TEST_CASE("expedition: a long journey brings a gem and ley fruit back") {
    Rng rng(1);
    ScriptSeq(rng, {0.1}, 4);
    const ExpeditionReward r = RollExpeditionReward(HT(), "long", 3, 32, rng);
    CHECK(r.embers == 22);
    CHECK(r.gold == static_cast<int64_t>(JsRound(60 * 3 + 32 * 12 * (0.8 + 0.1 * 0.4))));
    REQUIRE(r.items.size() == 2);
    CHECK(r.items[0] == std::pair<std::string, int32_t>{"g_ruby_3", 1});
    CHECK(r.items[1] == std::pair<std::string, int32_t>{PT().leyFruitId, 2});
    rng.ClearScript();
    rng.Script({0.5, 0.9});
    const ExpeditionReward s = RollExpeditionReward(HT(), "short", 0, 30, rng);
    CHECK(s.embers == 6);  // post level floored at 1
    REQUIRE(s.items.size() == 1);
    CHECK(s.items[0].first == "c_hp_potion_l");
  }

  // ===================================================================================================================
  // altar blessings
  // ===================================================================================================================
  TEST_CASE("blessing: costs embers and scales with the altar level") {
    CHECK(BlessingCost(1) == 10);
    CHECK(BlessingCost(3) == 20);
    StatBag edge1;
    edge1.Set(Stat::DamagePercent, 10);
    edge1.Set(Stat::CritRate, 3);
    CHECK(BlessingStatsFor(HT(), "ember_edge", 1) == edge1);
    StatBag edge3;
    edge3.Set(Stat::DamagePercent, 20);
    edge3.Set(Stat::CritRate, 6);
    CHECK(BlessingStatsFor(HT(), "ember_edge", 3) == edge3);
    CHECK(BlessingStatsFor(HT(), "nope", 1).Empty());
    HomesteadTower fresh(HT());
    CHECK(fresh.BlessingBlockFor("ember_edge") == BlessingBlock::Locked);
    HomesteadTower t(HT());
    const std::vector<std::string> done = {"q_collect_demon_essence"};
    t.SyncUnlocks(done);
    t.SetBuildingLevel("altar", 2);
    CHECK(t.BlessingBlockFor("ember_edge") == BlessingBlock::Embers);
    t.AddEmbers(20);
    CHECK(t.BlessingBlockFor("nope") == BlessingBlock::Unknown);
    CHECK(t.BuyBlessing("hearth_ward"));
    CHECK(t.Embers() == 5);
    CHECK(t.BlessingStats() == BlessingStatsFor(HT(), "hearth_ward", 2));
  }

  TEST_CASE("blessing: lasts until the next return to the tower or until its time runs out") {
    HomesteadTower t(HT());
    const std::vector<std::string> done = {"q_collect_demon_essence"};
    t.SyncUnlocks(done);
    t.AddEmbers(30);
    REQUIRE(t.BuyBlessing("ley_fortune"));
    CHECK_FALSE(t.Tick(HT().blessingDurationMs - 1).blessingEnded);
    CHECK(t.Tick(1).blessingEnded);
    CHECK(t.BlessingStats().Empty());
    REQUIRE(t.BuyBlessing("swift_flame"));
    CHECK(t.OnEnterTower());
    CHECK_FALSE(t.Blessing().has_value());
    CHECK_FALSE(t.OnEnterTower());
  }

  // ===================================================================================================================
  // save and migration (save-ui-input 3.2-3.3)
  // ===================================================================================================================
  TEST_CASE("homestead save: round-trips the tower state through JSON") {
    HomesteadTower t(HT());
    const std::vector<std::string> done = {HT().towerUnlockQuest, "q_secure_plains", "q_seal_fire_rift",
                                           "q_collect_demon_essence"};
    t.SyncUnlocks(done);
    t.AddEmbers(42);
    t.Garden() = GardenState{3, {{"c_hp_potion_m", 2}}};
    const std::vector<std::string> owl = {"pet_owl"};
    REQUIRE(t.SendExpedition("pet_owl", "long", owl, ""));
    REQUIRE(t.BuyBlessing("ember_edge"));
    t.Return() = TowerReturn{"twilight_forest", 18, 55};
    SaveHomestead out;
    t.WriteSave(out);
    const std::string json = PtJson([&](JsonWriter& w) { savejson::WriteHomestead(w, out); });
    SaveHomestead back;
    savejson::ReadHomestead(PtParse(json), back);
    HomesteadTower t2(HT());
    t2.Load(back);
    SaveHomestead again;
    t2.WriteSave(again);
    CHECK(again.buildings == out.buildings);
    CHECK(again.embers == out.embers);
    CHECK(again.garden == out.garden);
    CHECK(again.expedition == out.expedition);
    CHECK(again.blessing == out.blessing);
    CHECK(again.towerReturn == out.towerReturn);
    CHECK(t2.Embers() == 32);
    CHECK(PtJson([&](JsonWriter& w) { savejson::WriteHomestead(w, again); }) == json);
  }

  TEST_CASE("homestead save: key order and null sections of a new game") {
    HomesteadTower t(HT());
    SaveHomestead out;
    t.WriteSave(out);
    CHECK(PtJson([&](JsonWriter& w) { savejson::WriteHomestead(w, out); }) ==
          R"({"buildings":{},"embers":0,"garden":{"progress":0,"stock":{}},"expedition":null,"blessing":null,)"
          R"("towerReturn":null})");
    SavePets p;
    CHECK(PtJson([&](JsonWriter& w) { savejson::WritePets(w, p); }) == R"({"owned":[],"active":null})");
    p.owned.push_back(PetInstance{"pet_sprite", 1, 0, 0, 0, 0});
    p.active = "pet_sprite";
    CHECK(PtJson([&](JsonWriter& w) { savejson::WritePets(w, p); }) ==
          R"({"owned":[{"petId":"pet_sprite","level":1,"exp":0,"evolved":0,"bond":0,"bondProgress":0}],)"
          R"("active":"pet_sprite"})");
  }

  TEST_CASE("homestead save: saves from before the tower get safe defaults") {
    SaveHomestead old;
    savejson::ReadHomestead(PtParse(R"({"buildings": {"warehouse": 2}, "pets": []})"), old);
    CHECK(old.hasLegacyPets);
    HomesteadTower t(HT());
    t.Load(old);
    CHECK(t.Embers() == 0);
    CHECK(t.Garden() == GardenState{});
    CHECK_FALSE(t.Expedition().has_value());
    CHECK_FALSE(t.Blessing().has_value());
    CHECK_FALSE(t.Return().has_value());
    CHECK(t.BuildingLevel("warehouse") == 2);
    SaveHomestead none;
    savejson::ReadHomestead(JsonValue::Null(), none);
    CHECK(none.buildings.empty());
    CHECK_FALSE(none.hasLegacyPets);
  }

  TEST_CASE("homestead save: drops malformed fields") {
    SaveHomestead s;
    savejson::ReadHomestead(PtParse(R"({"embers": "x",
        "garden": {"progress": -4, "stock": {"c_hp_potion_s": -1, "c_mp_potion_s": 2}},
        "expedition": {"petId": "pet_owl", "optionId": "moon", "kills": 1, "killsRequired": 3, "remainingMs": 5},
        "blessing": {"id": "unknown", "level": 1, "remainingMs": 1000},
        "towerReturn": {"mapId": "emerald_plains", "col": 5, "row": 6}})"),
                            s);
    HomesteadTower t(HT());
    t.Load(s);
    CHECK(t.Embers() == 0);
    CHECK(t.Garden() == GardenState{0, {{"c_mp_potion_s", 2}}});
    CHECK_FALSE(t.Expedition().has_value());
    CHECK_FALSE(t.Blessing().has_value());
    REQUIRE(t.Return().has_value());
    CHECK(*t.Return() == TowerReturn{"emerald_plains", 5, 6});
    // Valid sections keep the num() guards: negatives -> 0, killsRequired >= 1, an expired blessing is dropped.
    SaveHomestead v;
    savejson::ReadHomestead(PtParse(R"({"embers": 7.9,
        "expedition": {"petId": "pet_owl", "optionId": "short", "kills": -2, "killsRequired": 0, "remainingMs": -1},
        "blessing": {"id": "ember_edge", "level": 0, "remainingMs": 0},
        "towerReturn": {"mapId": 3}})"),
                            v);
    HomesteadTower u(HT());
    u.Load(v);
    CHECK(u.Embers() == 7);
    REQUIRE(u.Expedition().has_value());
    CHECK(*u.Expedition() == ExpeditionState{"pet_owl", "short", 0, 1, 0});
    CHECK(u.ExpeditionDone());
    CHECK_FALSE(u.Blessing().has_value());
    CHECK_FALSE(u.Return().has_value());
  }

  TEST_CASE("homestead save: resets with a new game") {
    HomesteadTower t(HT());
    t.SetBuildingLevel("warehouse", 3);
    t.AddEmbers(5);
    const std::vector<std::string> done = {HT().towerUnlockQuest};
    t.SyncUnlocks(done);
    t.Reset();
    CHECK(t.Buildings().empty());
    CHECK(t.Embers() == 0);
    CHECK_FALSE(t.TowerUnlocked());
  }

  TEST_CASE("pet JSON: readers are lenient, writers keep the web field order") {
    PetInstance p;
    CHECK_FALSE(ReadPetJson(JsonValue::Array(), p));
    CHECK_FALSE(ReadPetJson(PtParse(R"({"petId": 3})"), p));
    REQUIRE(ReadPetJson(PtParse(R"({"petId": "pet_cat", "level": 2.9, "exp": 12.5, "bond": "x"})"), p));
    CHECK(p == PetInstance{"pet_cat", 2, 12, 0, 0, 0});
    SavePets s;
    savejson::ReadPets(PtParse(R"({"owned": 4, "active": "pet_cat"})"), s);
    CHECK(s.present);
    CHECK_FALSE(s.hasOwned);
    CHECK(s.active == "pet_cat");
    savejson::ReadPets(PtParse("[]"), s);
    CHECK_FALSE(s.present);
  }

  // ===================================================================================================================
  // ember_tower map
  // ===================================================================================================================
  TEST_CASE("ember_tower map: a small, monster-free zone outside the progression") {
    const MapDef* map = PD().FindMap(HT().towerZoneId);
    REQUIRE(map != nullptr);
    CHECK(map->cols == 48);
    CHECK(map->rows == 48);
    CHECK(map->spawns.empty());
    CHECK(map->exits.empty());
    CHECK(map->orderIndex == -1);
  }

  TEST_CASE("ember_tower map: every plot, ally, the portal and the meadow are reachable from the start") {
    const MapDef* map = PD().FindMap(HT().towerZoneId);
    REQUIRE(map != nullptr);
    REQUIRE(static_cast<int32_t>(map->collisions.size()) == map->rows);
    auto walk = [&](int32_t c, int32_t r) {
      if (r < 0 || r >= map->rows || c < 0 || c >= static_cast<int32_t>(map->collisions[static_cast<size_t>(r)].size())) {
        return false;
      }
      return map->collisions[static_cast<size_t>(r)][static_cast<size_t>(c)] == '1';
    };
    std::vector<std::vector<bool>> seen(static_cast<size_t>(map->rows), std::vector<bool>(static_cast<size_t>(map->cols)));
    std::vector<TilePos> stack = {map->playerStart};
    seen[static_cast<size_t>(map->playerStart.row)][static_cast<size_t>(map->playerStart.col)] = true;
    while (!stack.empty()) {
      const TilePos p = stack.back();
      stack.pop_back();
      for (const TilePos d : {TilePos{1, 0}, TilePos{-1, 0}, TilePos{0, 1}, TilePos{0, -1}}) {
        const int32_t nc = p.col + d.col, nr = p.row + d.row;
        if (walk(nc, nr) && !seen[static_cast<size_t>(nr)][static_cast<size_t>(nc)]) {
          seen[static_cast<size_t>(nr)][static_cast<size_t>(nc)] = true;
          stack.push_back(TilePos{nc, nr});
        }
      }
    }
    auto isSeen = [&](int32_t c, int32_t r) {
      return r >= 0 && r < map->rows && c >= 0 && c < map->cols && seen[static_cast<size_t>(r)][static_cast<size_t>(c)];
    };
    auto reach = [&](int32_t c, int32_t r) {
      for (const TilePos d : {TilePos{0, 0}, TilePos{1, 0}, TilePos{-1, 0}, TilePos{0, 1}, TilePos{0, -1}, TilePos{1, 1}}) {
        if (isSeen(c + d.col, r + d.row)) return true;
      }
      return false;
    };
    const EmberTowerLayoutDef& layout = PD().World().emberTower;
    CHECK(layout.plots.size() == HT().buildings.size());
    for (const EmberTowerPlotDef& plot : layout.plots) {
      CAPTURE(plot.id);
      CHECK(isSeen(plot.npcPos.col, plot.npcPos.row));
      CHECK((reach(plot.pos.col, plot.pos.row + 1) || reach(plot.pos.col + 2, plot.pos.row + 1)));
    }
    CHECK(isSeen(layout.portal.col, layout.portal.row));
    CHECK(isSeen(layout.meadow.col, layout.meadow.row));
    for (const FieldNpcDef& n : map->fieldNpcs) {
      CAPTURE(n.npcId);
      CHECK(PD().FindNpc(n.npcId) != nullptr);
    }
  }

  // ===================================================================================================================
  // PetCompanion runtime (18.5) on the real data
  // ===================================================================================================================
  TEST_CASE("companion: spawns beside the hero, re-spawns on evolution (QP2) and leaves when resting") {
    PetRig r;
    REQUIRE(r.ok);
    REQUIRE(r.pets.AddPet("pet_sprite"));
    const PetCompanionState& s = r.companion.State();
    REQUIRE(s.present);
    const Vec2 hp = r.hero->Position();
    const Vec2 want = r.zone.Walkable(JsRoundInt(hp.x - 1.2), JsRoundInt(hp.y + 1.2)) ? Vec2(hp.x - 1.2, hp.y + 1.2) : hp;
    CHECK(s.pos.x == doctest::Approx(want.x));
    CHECK(s.pos.y == doctest::Approx(want.y));
    CHECK(s.maxHp == JsRound(r.hero->MaxHp() * 0.45));
    CHECK(s.hp == s.maxHp);
    std::vector<EvEntitySpawned> spawned = PtEvents<EvEntitySpawned>(r.h.events);
    REQUIRE(spawned.size() == 1);
    CHECK(spawned[0].kind == EntityKind::Pet);
    CHECK(spawned[0].defId == "pet_sprite");
    CHECK(spawned[0].artId == "beast_pet_sprite");
    const EntityId first = s.entity;
    // A cooldown carries over the re-spawn (one companion per visit).
    r.companion.MutableStateForTesting().readyAtMs = {{"sprite_heal_pulse", 99999}};
    r.PlacePet(Vec2(hp.x + 5, hp.y));
    r.companion.MutableStateForTesting().hp = 3;
    r.h.events.Clear();
    r.pets.AddExp("pet_sprite", 2340, true);  // -> Lv10, awakened
    REQUIRE(r.pets.Active()->evolved == 1);
    CHECK(s.present);
    CHECK(s.stage == 1);
    CHECK(s.entity != first);
    CHECK(s.hp == s.maxHp);
    CHECK(s.pos.x == doctest::Approx(want.x));
    CHECK(s.readyAtMs == std::vector<std::pair<std::string, double>>{{"sprite_heal_pulse", 99999}});
    REQUIRE(PtEvents<EvEntityDespawned>(r.h.events).size() == 1);
    spawned = PtEvents<EvEntitySpawned>(r.h.events);
    REQUIRE(spawned.size() == 1);
    CHECK(spawned[0].artId == "beast_pet_sprite_e1");
    r.h.events.Clear();
    r.pets.SetActivePet("");
    CHECK_FALSE(s.present);
    const std::vector<EvEntityDespawned> gone = PtEvents<EvEntityDespawned>(r.h.events);
    REQUIRE(gone.size() == 1);
    CHECK(gone[0].reason == DespawnReason::Removed);
    // Snapshot view.
    r.pets.SetActivePet("pet_sprite");
    Snapshot snap;
    r.companion.FillSnapshot(snap);
    CHECK(snap.pet.present);
    CHECK(snap.pet.petId == "pet_sprite");
    CHECK(snap.pet.stage == 1);
    CHECK(snap.pet.maxHp == s.maxHp);
  }

  TEST_CASE("companion: ranged basic attack - cast release 161 ms, bolt flight, petAttackDamage on arrival") {
    PetRig r;
    REQUIRE(r.ok);
    r.h.ctx.equip.Ref(Stat::Damage) = 100;
    REQUIRE(r.pets.AddPet("pet_sprite"));
    const EntityId g = r.Spawn("goblin", TilePos{42, 40});
    REQUIRE_MONSTER(g);
    r.PlacePet(Vec2(39, 41));
    const double hp0 = r.monsters.Find(g)->hp;
    r.h.events.Clear();
    r.PetRng().Script({0.99});  // no crit
    r.Step();
    const double t0 = r.Now();
    const std::vector<EvPlayAnim> anims = PtEvents<EvPlayAnim>(r.h.events);
    REQUIRE(anims.size() == 1);
    CHECK(anims[0].entity == r.companion.State().entity);
    CHECK(anims[0].action == AnimAction::Cast);
    CHECK(anims[0].contactMs == doctest::Approx(161));
    CHECK(r.companion.State().basicReadyAtMs == doctest::Approx(t0 + 1800));
    CHECK(r.companion.State().target == g);
    REQUIRE(r.StepUntil([&] { return !PtEvents<EvProjectileLaunched>(r.h.events).empty(); }, 30));
    const EvProjectileLaunched launch = PtEvents<EvProjectileLaunched>(r.h.events)[0];
    CHECK(launch.kind == ProjectileKind::PetBolt);
    CHECK(launch.launchMs >= t0 + 161);
    CHECK(launch.launchMs < t0 + 161 + kSimStepMs);
    CHECK(launch.travelMs == doctest::Approx(MonsterBoltTravelMs(PD().Combat().projectiles, Vec2(39, 41), Vec2(42, 40))));
    CHECK(launch.color == 0x8ff0c0u);
    REQUIRE(r.StepUntil([&] { return !PtEvents<EvProjectileEnded>(r.h.events).empty(); }, 60));
    const int32_t want = PetAttackDamage(PT(), r.hero->Derived().baseDamage + 100, *r.pets.Active());
    REQUIRE(want > 0);
    CHECK(r.monsters.Find(g)->hp == doctest::Approx(hp0 - want));
    const std::vector<EvHit> hits = PtEvents<EvHit>(r.h.events);
    REQUIRE(hits.size() == 1);
    CHECK(hits[0].source == r.companion.State().entity);
    CHECK(hits[0].element == DamageType::Arcane);
    CHECK(hits[0].attackerStopMs == 0);
    CHECK_FALSE(hits[0].crit);
    // Pet hits never provoke / aggro an idle monster (18.5.4) - the goblin was already chasing; state untouched.
    CHECK(r.monsters.Find(g)->state == MonsterState::Chase);
  }

  TEST_CASE("companion: no damage while heroDamage x fraction < 1 (QP3)") {
    PetRig r;
    r.h.ctx.equip.Ref(Stat::Damage) = -1000;
    REQUIRE(r.pets.AddPet("pet_sprite"));
    const EntityId g = r.Spawn("goblin", TilePos{42, 40});
    REQUIRE_MONSTER(g);
    r.PlacePet(Vec2(39, 41));
    const double hp0 = r.monsters.Find(g)->hp;
    r.Step(80);
    CHECK_FALSE(PtEvents<EvProjectileLaunched>(r.h.events).empty());  // the bolt still flies
    CHECK(PtEvents<EvHit>(r.h.events).empty());
    CHECK(r.monsters.Find(g)->hp == hp0);
  }

  TEST_CASE("companion: heal pulse below 70 % HP outside the camps, +floor(8 % max HP), 12 s cooldown") {
    PetRig r;
    REQUIRE(r.pets.AddPet("pet_sprite"));
    const double maxHp = r.hero->MaxHp();
    r.hero->SetHp(maxHp * 0.5);
    r.h.events.Clear();
    r.Step();
    const double t0 = r.Now();
    CHECK(r.companion.State().readyAtMs == std::vector<std::pair<std::string, double>>{{"sprite_heal_pulse", t0 + 12000}});
    REQUIRE(r.StepUntil([&] { return r.hero->Hp() > maxHp * 0.5; }, 20));
    CHECK(r.Now() >= t0 + 161);
    const double amount = std::floor(maxHp * 0.08);
    CHECK(r.hero->Hp() == doctest::Approx(maxHp * 0.5 + amount));
    const std::vector<EvFloatingText> ft = PtEvents<EvFloatingText>(r.h.events);
    REQUIRE(ft.size() == 1);
    CHECK(ft[0].kind == FloatingTextKind::Heal);
    CHECK(ft[0].value == amount);
    bool vfx = false;
    for (const EvSkillVfx& v : PtEvents<EvSkillVfx>(r.h.events)) vfx = vfx || v.vfxId == "life_regen";
    CHECK(vfx);
    // Still below 70 %: no second heal before the cooldown ends.
    const double hp1 = r.hero->Hp();
    r.Step(600);
    CHECK(r.hero->Hp() == hp1);
    r.StepUntil([&] { return r.hero->Hp() > hp1; }, 200);
    CHECK(r.hero->Hp() > hp1);
    CHECK(r.Now() >= t0 + 12000);
  }

  TEST_CASE("companion: never heals inside a camp's safe zone, nor a dead hero (QP1 fix)") {
    {
      const MapDef* map = PD().FindMap("emerald_plains");
      REQUIRE(!map->camps.empty());
      PetRig r(map->camps[0].pos.Center());
      REQUIRE(r.pets.AddPet("pet_sprite"));
      r.hero->SetHp(r.hero->MaxHp() * 0.5);
      const double hp = r.hero->Hp();
      r.Step(60);
      CHECK(r.hero->Hp() == hp);
      CHECK(r.companion.State().lastAction == PetActionKind::Follow);
      // Peaceful regeneration: 8 % of the beast's max HP per second.
      r.companion.MutableStateForTesting().hp = 1;
      r.Step(60);
      CHECK(r.companion.State().hp == doctest::Approx(1 + r.companion.State().maxHp * 0.08).epsilon(0.02));
    }
    {
      PetRig r;
      REQUIRE(r.pets.AddPet("pet_sprite"));
      r.hero->SetHp(0);
      r.Step(60);
      CHECK(r.hero->Hp() == 0);
      CHECK(r.companion.State().lastAction == PetActionKind::Follow);
      CHECK(PtEvents<EvPlayAnim>(r.h.events).empty());
    }
  }

  TEST_CASE("companion: stray swings - a 25 % roll when the beast is in reach and closer; the hit lands at contact") {
    PetRig r;
    r.h.ctx.equip.Ref(Stat::Damage) = -1000;  // the beast deals nothing: no crit rolls on the Pets stream
    REQUIRE(r.pets.AddPet("pet_sprite"));
    r.PlacePet(Vec2(38, 42));
    const EntityId g = r.Spawn("goblin", TilePos{37, 42}, MonsterState::Attack);
    REQUIRE_MONSTER(g);
    MonsterInstance* m = r.monsters.Find(g);
    REQUIRE(JsHypot(m->pos - Vec2(38, 42)) <= m->def.attackRange + 0.5);
    r.PetRng().Script({0.3});
    CHECK_FALSE(r.companion.InterceptMonsterAttack(g));
    CHECK(m->lastAttackMs == 0);
    r.PetRng().Script({0.1});
    REQUIRE(r.companion.InterceptMonsterAttack(g));
    CHECK(m->lastAttackMs == r.Now());
    const std::vector<EvPlayAnim> anims = PtEvents<EvPlayAnim>(r.h.events);
    REQUIRE(anims.size() == 1);
    CHECK(anims[0].entity == g);
    CHECK(anims[0].faceTarget == Vec2(38, 42));
    r.PetRng().Script({0.5});  // takeHit jitter -> x1.0
    r.h.events.Clear();
    REQUIRE(r.StepUntil([&] { return !PtEvents<EvHit>(r.h.events).empty(); }, 120));
    const EvHit hit = PtEvents<EvHit>(r.h.events)[0];
    CHECK(hit.target == r.companion.State().entity);
    CHECK(hit.source == g);
    CHECK(hit.amount == (std::max)(1.0, JsRound(m->def.damage * 0.8)));
    // Beyond reach / farther than the hero: no roll at all.
    r.PlacePet(Vec2(30, 30));
    r.PetRng().Script({0.0});
    CHECK_FALSE(r.companion.InterceptMonsterAttack(g));
    CHECK(r.PetRng().ScriptedRemaining() == 1);
    r.PetRng().ClearScript();
  }

  TEST_CASE("companion: exhaustion at 0 HP - 5 s, swings ignored, recovers at full HP") {
    PetRig r;
    r.h.ctx.equip.Ref(Stat::Damage) = -1000;
    REQUIRE(r.pets.AddPet("pet_sprite"));
    r.PlacePet(Vec2(38, 42));
    const EntityId g = r.Spawn("goblin", TilePos{37, 42}, MonsterState::Attack);
    REQUIRE_MONSTER(g);
    r.companion.MutableStateForTesting().hp = 1;
    r.PetRng().Script({0.1, 0.5});
    REQUIRE(r.companion.InterceptMonsterAttack(g));
    r.h.events.Clear();
    REQUIRE(r.StepUntil([&] { return r.companion.IsExhausted(); }, 120));
    const double t0 = r.Now();
    CHECK(r.companion.State().hp == 0);
    CHECK(r.companion.State().exhaustedUntilMs == doctest::Approx(t0 + 5000));
    bool exhaustedEv = false;
    for (const EvPet& e : PtEvents<EvPet>(r.h.events)) exhaustedEv = exhaustedEv || e.kind == EvPet::Kind::Exhausted;
    CHECK(exhaustedEv);
    const std::vector<std::string> logs = PtLogKeys(r.h.events);
    CHECK(std::find(logs.begin(), logs.end(), "sys.pet.exhausted") != logs.end());
    r.PetRng().Script({0.0});
    CHECK_FALSE(r.companion.InterceptMonsterAttack(g));  // incoming swings ignored
    CHECK(r.PetRng().ScriptedRemaining() == 1);
    r.PetRng().ClearScript();
    Snapshot snap;
    r.companion.FillSnapshot(snap);
    CHECK(snap.pet.exhausted);
    r.Step(30);
    CHECK(r.companion.State().hp == 0);  // no regen while exhausted
    REQUIRE(r.StepUntil([&] { return !r.companion.IsExhausted() && r.companion.State().hp > 0; }, 400));
    CHECK(r.Now() >= t0 + 5000);
    CHECK(r.companion.State().hp == r.companion.State().maxHp);
    CHECK(r.companion.State().exhaustedUntilMs == 0);
  }

  TEST_CASE("companion: follow - dash beyond 4 tiles, walk within, teleport beyond 16") {
    PetRig r;
    REQUIRE(r.pets.AddPet("pet_sprite"));
    const Vec2 hero = r.hero->Position();
    const Vec2 goal(hero.x - 1.3, hero.y + 1.3);
    r.PlacePet(Vec2(goal.x + 6, goal.y));
    r.Step();
    CHECK(r.companion.State().pos.x == doctest::Approx(goal.x + 6 - 8.5 * kSimStepMs / 1000.0));
    CHECK(r.companion.State().moving);
    r.PlacePet(Vec2(goal.x + 2, goal.y));
    r.Step();
    CHECK(r.companion.State().pos.x == doctest::Approx(goal.x + 2 - 4.2 * kSimStepMs / 1000.0));
    r.PlacePet(Vec2(goal.x + 0.2, goal.y));
    r.Step();
    CHECK(r.companion.State().pos.x == doctest::Approx(goal.x + 0.2));  // within followStop 0.35
    r.h.events.Clear();
    r.hero->SetPosition(Vec2(hero.x + 30, hero.y));
    r.Step();
    CHECK(r.companion.State().pos == r.hero->Position());
    const std::vector<EvEntityTeleported> tp = PtEvents<EvEntityTeleported>(r.h.events);
    REQUIRE(tp.size() == 1);
    CHECK(tp[0].reason == TeleportReason::CatchUp);
    // An exhausted beast follows at 0.7x.
    r.companion.MutableStateForTesting().exhaustedUntilMs = r.Now() + 5000;
    const Vec2 g2(r.hero->Position().x - 1.3, r.hero->Position().y + 1.3);
    r.PlacePet(Vec2(g2.x + 2, g2.y));
    r.Step();
    CHECK(r.companion.State().pos.x == doctest::Approx(g2.x + 2 - 4.2 * 0.7 * kSimStepMs / 1000.0));
  }

  TEST_CASE("companion: kill credit - a monster killed by the beast runs the whole kill pipeline") {
    PetRig r;
    r.h.ctx.equip.Ref(Stat::Damage) = 100;
    REQUIRE(r.pets.AddPet("pet_sprite"));
    const EntityId g = r.Spawn("goblin", TilePos{42, 40});
    REQUIRE_MONSTER(g);
    r.PlacePet(Vec2(39, 41));
    r.monsters.Find(g)->hp = 1;
    r.PetRng().Script({0.99});
    REQUIRE(r.StepUntil([&] { return !r.kills.empty(); }, 120));
    CHECK(r.kills[0].monster == g);
    CHECK(r.kills[0].source == KillSource::Pet);
    CHECK(r.kills[0].killer == r.companion.State().entity);
    CHECK(r.pets.Active()->exp == PetKillExp(PT().system, r.kills[0].level));
    CHECK(r.pets.Active()->bondProgress == 1);
    CHECK(r.hero->Exp() > 0);  // hero exp / gold paid by the combat handler
  }

  TEST_CASE("companion: the awakened sprite shields the hero when monsters swing and HP < 85 %") {
    PetRig r;
    REQUIRE(r.pets.AddPet("pet_sprite"));
    r.pets.AddExp("pet_sprite", 2340, true);
    REQUIRE(r.pets.Active()->evolved == 1);
    r.hero->SetHp(r.hero->MaxHp() * 0.8);
    const EntityId g = r.Spawn("goblin", TilePos{41, 40}, MonsterState::Attack);
    REQUIRE_MONSTER(g);
    r.Step();
    REQUIRE(r.StepUntil([&] { return r.hero->Buffs().FindTag(BuffTag::PetShield) != nullptr; }, 20));
    const ActiveBuff* b = r.hero->Buffs().FindTag(BuffTag::PetShield);
    CHECK(b->stat == BuffStat::DamageReduction);
    CHECK(b->value == doctest::Approx(0.2));
    CHECK(b->durationMs == doctest::Approx(5000));
  }

  TEST_CASE("companion: owl moon mark amplifies the beast's own hits") {
    PetRig r;
    r.h.ctx.equip.Ref(Stat::Damage) = 300;
    REQUIRE(r.pets.AddPet("pet_owl"));
    const EntityId g = r.Spawn("goblin_chief", TilePos{42, 40});
    REQUIRE_MONSTER(g);
    r.PlacePet(Vec2(39, 41));
    r.Step();
    REQUIRE(r.StepUntil([&] { return r.monsters.Find(g)->buffs.FindTag(BuffTag::PetMark) != nullptr; }, 20));
    const ActiveBuff* mark = r.monsters.Find(g)->buffs.FindTag(BuffTag::PetMark);
    CHECK(mark->stat == BuffStat::DamageAmplify);
    CHECK(mark->value == doctest::Approx(0.15));
    REQUIRE(r.companion.State().marks.size() == 1);
    const double hp0 = r.monsters.Find(g)->hp;
    r.h.events.Clear();
    r.PetRng().Script({0.99});
    REQUIRE(r.StepUntil([&] { return !PtEvents<EvHit>(r.h.events).empty(); }, 200));
    const int32_t base = PetAttackDamage(PT(), r.hero->Derived().baseDamage + 300, *r.pets.Active());
    CHECK(r.monsters.Find(g)->hp == doctest::Approx(hp0 - (std::max)(1.0, JsRound(base * 1.15))));
  }

  TEST_CASE("companion: storm wolf pounce leaps beside the target, strikes x1.5 and bleeds") {
    PetRig r;
    r.h.ctx.equip.Ref(Stat::Damage) = 300;
    REQUIRE(r.pets.AddPet("pet_storm_wolf"));
    const EntityId g = r.Spawn("goblin_chief", TilePos{42, 40});
    REQUIRE_MONSTER(g);
    r.PlacePet(Vec2(39, 41));
    const double hp0 = r.monsters.Find(g)->hp;
    r.PetRng().Script({0.99});
    r.Step();
    CHECK(r.companion.State().leaping);
    REQUIRE(r.StepUntil([&] { return !PtEvents<EvStatusApplied>(r.h.events).empty(); }, 30));
    const Vec2 tpos(42, 40);
    const Vec2 dir = (tpos - Vec2(39, 41)).Normalized();
    CHECK(r.companion.State().pos.x == doctest::Approx(tpos.x - dir.x * 0.9).epsilon(0.001));
    const int32_t base = PetAttackDamage(PT(), r.hero->Derived().baseDamage + 300, *r.pets.Active());
    const double dmg = (std::max)(1.0, JsRound(base * 1.5));
    CHECK(r.monsters.Find(g)->hp == doctest::Approx(hp0 - dmg));
    const EvStatusApplied st = PtEvents<EvStatusApplied>(r.h.events)[0];
    CHECK(st.type == StatusType::Bleed);
    CHECK(st.value == (std::max)(1.0, JsRound(dmg * 0.25)));
    CHECK(st.durationMs == doctest::Approx(4000));
  }

  TEST_CASE("companion: dragon breath burns the monsters inside the cone only") {
    PetRig r;
    r.h.ctx.equip.Ref(Stat::Damage) = 300;
    REQUIRE(r.pets.AddPet("pet_dragon"));
    r.PlacePet(Vec2(40, 42));
    const EntityId front = r.Spawn("goblin_chief", TilePos{42, 42});
    const EntityId behind = r.Spawn("goblin_chief", TilePos{37, 42}, MonsterState::Idle);
    REQUIRE_MONSTER(front);
    REQUIRE_MONSTER(behind);
    const double f0 = r.monsters.Find(front)->hp, b0 = r.monsters.Find(behind)->hp;
    r.PetRng().Script({0.99});
    r.Step();
    REQUIRE(r.StepUntil([&] { return !PtEvents<EvStatusApplied>(r.h.events).empty(); }, 30));
    CHECK(r.monsters.Find(front)->hp < f0);
    CHECK(r.monsters.Find(behind)->hp == b0);
    const EvStatusApplied st = PtEvents<EvStatusApplied>(r.h.events)[0];
    CHECK(st.target == front);
    CHECK(st.type == StatusType::Burn);
    CHECK(PtEvents<EvHit>(r.h.events)[0].element == DamageType::Fire);
  }

  TEST_CASE("companion: tortoise taunt redirects swings without a roll") {
    PetRig r;
    REQUIRE(r.pets.AddPet("pet_jade_tortoise"));
    const EntityId g = r.Spawn("goblin", TilePos{41, 40}, MonsterState::Attack);
    REQUIRE_MONSTER(g);
    r.PlacePet(Vec2(39, 41));
    r.Step();
    REQUIRE(r.StepUntil([&] { return !r.companion.State().taunts.empty(); }, 20));
    CHECK(r.companion.State().taunts[0].first == g);
    const ActiveBuff* dr = r.hero->Buffs().FindTag(BuffTag::PetShield);
    REQUIRE(dr != nullptr);
    CHECK(dr->value == doctest::Approx(0.25));
    r.PetRng().Script({0.99});
    CHECK(r.companion.InterceptMonsterAttack(g));
    CHECK(r.PetRng().ScriptedRemaining() == 1);
    r.PetRng().ClearScript();
  }

  TEST_CASE("companion: void butterfly bolt hits and restores mana") {
    PetRig r;
    r.h.ctx.equip.Ref(Stat::Damage) = 300;
    REQUIRE(r.pets.AddPet("pet_void_butterfly"));
    const EntityId g = r.Spawn("goblin_chief", TilePos{43, 40});
    REQUIRE_MONSTER(g);
    r.PlacePet(Vec2(39, 41));
    r.hero->SetMana(0);
    const double hp0 = r.monsters.Find(g)->hp;
    r.PetRng().Script({0.99});
    r.Step();
    REQUIRE(r.StepUntil([&] { return r.hero->Mana() > 0; }, 80));
    CHECK(r.hero->Mana() == (std::max)(1.0, std::floor(r.hero->MaxMana() * 0.05)));
    const int32_t base = PetAttackDamage(PT(), r.hero->Derived().baseDamage + 300, *r.pets.Active());
    CHECK(r.monsters.Find(g)->hp == doctest::Approx(hp0 - (std::max)(1.0, JsRound(base * 1.5))));
  }

  TEST_CASE("companion: the phoenix rekindles the hero once per zone visit") {
    PetRig r;
    REQUIRE(r.pets.AddPet("pet_phoenix"));
    r.hero->SetHp(0);
    r.combat.KillHero(HeroDeathCause::Other);
    CHECK(r.hero->Life() == HeroLife::Alive);
    CHECK(r.hero->Hp() == std::floor(r.hero->MaxHp() * 0.4));
    const std::vector<std::string> logs = PtLogKeys(r.h.events);
    CHECK(std::find(logs.begin(), logs.end(), "sys.pet.revive") != logs.end());
    CHECK_FALSE(r.companion.TryReviveHero());
    r.companion.OnZoneEnter();  // a new visit
    CHECK(r.companion.TryReviveHero());
    // Beasts without a revive never save the hero.
    PetRig s;
    REQUIRE(s.pets.AddPet("pet_sprite"));
    CHECK_FALSE(s.companion.TryReviveHero());
  }

  TEST_CASE("companion: max bond rescue fires the signature ability once a minute") {
    PetRig r;
    r.homestead.MutableTower().SetBuildingLevel("pet_house", 2);
    REQUIRE(r.pets.AddPet("pet_sprite"));
    r.pets.FindMutableForTesting("pet_sprite")->bond = 5;
    r.companion.MutableStateForTesting().readyAtMs = {{"sprite_heal_pulse", 1e12}};
    r.hero->SetHp(r.hero->MaxHp() * 0.2);
    const double hp0 = r.hero->Hp();
    r.h.events.Clear();
    r.Step();
    const double t0 = r.Now();
    const std::vector<std::string> logs = PtLogKeys(r.h.events);
    CHECK(std::find(logs.begin(), logs.end(), "sys.pet.rescue") != logs.end());
    CHECK(r.companion.State().lastRescueMs == t0);
    CHECK(r.companion.State().lockedUntilMs >= t0 + 400);
    REQUIRE(r.StepUntil([&] { return r.hero->Hp() > hp0; }, 20));
    r.hero->SetHp(r.hero->MaxHp() * 0.2);
    r.h.events.Clear();
    r.Step(120);
    const std::vector<std::string> again = PtLogKeys(r.h.events);
    CHECK(std::find(again.begin(), again.end(), "sys.pet.rescue") == again.end());
  }

  TEST_CASE("companion: feeding from the bag uses one ley fruit") {
    PetRig r;
    REQUIRE(r.pets.AddPet("pet_sprite"));
    r.h.events.Clear();
    CHECK_FALSE(r.companion.FeedFromBag("pet_sprite"));
    CHECK(PtLogKeys(r.h.events) == std::vector<std::string>{"sys.pet.noFruit"});
    const LootContext lc{&PD(), &r.h.rng.Get(RngStream::Loot), &r.inventory.Uids()};
    std::optional<ItemInstance> fruit = CreateItem(lc, PT().leyFruitId, 1, ItemQuality::Normal);
    REQUIRE(fruit.has_value());
    fruit->quantity = 2;
    REQUIRE(r.inventory.Items().AddItem(*fruit).ok);
    r.h.events.Clear();
    CHECK(r.companion.FeedFromBag("pet_sprite"));
    CHECK(r.inventory.Items().CountOf(PT().leyFruitId) == 1);
    CHECK(r.pets.Active()->level == 2);
    CHECK(r.pets.Active()->exp == 20);
    CHECK(PtEvents<EvInventoryChanged>(r.h.events).size() == 1);
    r.pets.FindMutableForTesting("pet_sprite")->level = 20;
    r.pets.FindMutableForTesting("pet_sprite")->bond = 3;
    r.h.events.Clear();
    CHECK_FALSE(r.companion.FeedFromBag("pet_sprite"));
    CHECK(PtLogKeys(r.h.events) == std::vector<std::string>{"sys.pet.feedFull"});
    CHECK(r.inventory.Items().CountOf(PT().leyFruitId) == 1);
  }

  TEST_CASE("companion: bond grows per active minute outside the camps (QP7: also while exhausted)") {
    PetRig r;
    REQUIRE(r.pets.AddPet("pet_sprite"));
    r.companion.MutableStateForTesting().exhaustedUntilMs = 1e12;
    r.Step(3601);  // a minute of sim time
    CHECK(r.pets.Active()->bondProgress == 2);
  }

  // ===================================================================================================================
  // HomesteadSystem runtime (4.4-4.7)
  // ===================================================================================================================
  TEST_CASE("homestead: kill embers float, garden growth and nothing for plain kills") {
    PetRig r;
    MonsterKilledMsg m;
    m.elite = true;
    m.isMiniBoss = false;
    m.eliteAffixCount = 1;
    m.pos = Vec2(10, 12);
    r.homestead.OnMonsterKilled(m);
    CHECK(r.homestead.Tower().Embers() == 5);
    const std::vector<EvEmbersGained> ev = PtEvents<EvEmbersGained>(r.h.events);
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].amount == 5);
    CHECK(ev[0].pos == Vec2(10, 12));
    CHECK(ev[0].fromKill);
    r.h.events.Clear();
    MonsterKilledMsg plain;
    r.homestead.OnMonsterKilled(plain);
    CHECK(r.homestead.Tower().Embers() == 5);
    CHECK(r.h.events.Items().empty());
    CHECK(r.homestead.Tower().Garden().progress == 0);  // garden locked until q_secure_plains
    r.quests.Load({QuestProgress{"q_secure_plains", QuestStatus::TurnedIn, {}}});
    r.homestead.OnQuestTurnedIn(QuestTurnedInMsg{"q_secure_plains"});
    CHECK(r.homestead.BuildingLevel("herb_garden") == 1);
    for (int i = 0; i < 13; ++i) r.homestead.OnMonsterKilled(plain);
    CHECK(r.homestead.Tower().Garden().progress == 13);
    r.homestead.OnMonsterKilled(plain);
    CHECK(r.homestead.Tower().GardenStockCount() == 1);
    CHECK(r.homestead.Tower().Garden().progress == 0);
  }

  TEST_CASE("homestead: quest embers + unlock records; milestone 1 hides the unlock lines (OQ8)") {
    PetRig r;
    r.quests.Load({QuestProgress{"q_explore_goblin_camp", QuestStatus::TurnedIn, {}}});
    r.homestead.OnQuestTurnedIn(QuestTurnedInMsg{"q_explore_goblin_camp"});
    const QuestDef* q = PD().FindQuest("q_explore_goblin_camp");
    const int32_t n = EmbersForQuest(HT(), q->category, q->rewards.hasEmbers, q->rewards.embers);
    CHECK(r.homestead.Tower().Embers() == n);
    CHECK(r.homestead.Tower().TowerUnlocked());
    CHECK(PtLogKeys(r.h.events) == std::vector<std::string>{"homestead.log.questEmbers"});
    const EvLog log = PtLogs(r.h.events)[0];
    CHECK(log.type == LogType::Loot);
    CHECK(log.text.args == std::vector<I18nArg>{{"n", ToStr(n), false}});
    const std::vector<EvEmbersGained> ev = PtEvents<EvEmbersGained>(r.h.events);
    REQUIRE(ev.size() == 1);
    CHECK_FALSE(ev[0].fromKill);
    // The later-milestone build logs the tower and wing lines.
    PetRig t;
    t.h.config.milestone1 = false;
    t.quests.Load({QuestProgress{"q_explore_goblin_camp", QuestStatus::TurnedIn, {}},
                   QuestProgress{"q_secure_plains", QuestStatus::TurnedIn, {}}});
    t.homestead.OnQuestTurnedIn(QuestTurnedInMsg{"q_explore_goblin_camp"});
    CHECK(PtLogKeys(t.h.events) == std::vector<std::string>{"homestead.log.questEmbers", "homestead.log.wingUnlocked",
                                                            "homestead.log.towerUnlocked"});
  }

  TEST_CASE("homestead: TotalBonuses = building bonuses + blessing; BuildingBonuses = the web's homeBonus") {
    PetRig r;
    HomesteadTower& t = r.homestead.MutableTower();
    const std::vector<std::string> done = {"q_collect_demon_essence"};
    t.SyncUnlocks(done);
    t.SetBuildingLevel("warehouse", 2);
    t.AddEmbers(10);
    REQUIRE(t.BuyBlessing("ley_fortune"));
    CHECK(r.homestead.BuildingBonuses().Get(Stat::StashSlots) == 20);
    CHECK(r.homestead.BuildingBonuses().Get(Stat::MagicFind) == 0);
    CHECK(r.homestead.TotalBonuses().Get(Stat::MagicFind) == 30);
    CHECK(r.homestead.BlessingStats().Get(Stat::ExpBonus) == 10);
    // Blessing timer: fades with a log and an equip-stat rebuild request.
    size_t dirty = 0;
    r.h.bus.Subscribe<EquipStatsDirtyMsg>([&](const EquipStatsDirtyMsg&) { ++dirty; });
    r.homestead.Tick(HT().blessingDurationMs);
    CHECK(r.homestead.BlessingStats().Empty());
    CHECK(dirty == 1);
    CHECK(PtLogKeys(r.h.events) == std::vector<std::string>{"homestead.log.blessingFaded"});
  }

  TEST_CASE("homestead: save keeps the state and re-syncs unlocks from the loaded quests (no log)") {
    PetRig r;
    r.homestead.MutableTower().AddEmbers(23);
    r.homestead.MutableTower().SetBuildingLevel("herb_garden", 1);
    r.homestead.MutableTower().Garden().progress = 9;
    SaveData out;
    r.homestead.WriteSave(out);
    CHECK(out.homestead.embers == 23);
    PetRig s;
    s.quests.Load({QuestProgress{"q_explore_goblin_camp", QuestStatus::TurnedIn, {}},
                   QuestProgress{"q_secure_plains", QuestStatus::TurnedIn, {}}});
    s.h.events.Clear();
    s.homestead.ReadSave(out);
    CHECK(s.homestead.Tower().Embers() == 23);
    CHECK(s.homestead.Tower().TowerUnlocked());
    CHECK(s.homestead.Tower().IsBuildingUnlocked("herb_garden"));
    CHECK(s.homestead.Tower().Garden().progress == 9);
    CHECK(s.h.events.Items().empty());
    Snapshot snap;
    s.homestead.FillSnapshot(snap);
    CHECK(snap.homestead == &s.homestead.Tower());
  }

  TEST_CASE("homestead: tower actions work only inside the tower") {
    PetRig r;
    HomesteadTower& t = r.homestead.MutableTower();
    std::vector<QuestProgress> done;
    for (const char* q : {"q_explore_goblin_camp", "q_secure_plains", "q_kill_stone_guardian", "q_seal_fire_rift",
                          "q_collect_demon_essence"}) {
      done.push_back(QuestProgress{q, QuestStatus::TurnedIn, {}});
    }
    r.quests.Load(done);
    r.homestead.OnEnterTower();  // the tower syncs its unlocks from the quest log (EmberTower constructor)
    CHECK(t.TowerUnlocked());
    t.Garden().stock = {{"c_hp_potion_s", 2}};
    t.AddEmbers(40);
    r.hero->AddGold(5000);
    CHECK(r.homestead.HarvestGarden() == 0);
    CHECK_FALSE(r.homestead.BuyBlessing("ember_edge"));
    // Upgrades are panel actions (not tower-gated).
    CHECK(r.homestead.UpgradeBuilding("herb_garden"));
    CHECK(r.homestead.BuildingLevel("herb_garden") == 2);
    CHECK(r.hero->Gold() == 5000 - 250);
    CHECK(t.Embers() == 35);
    r.h.session.currentMap = HT().towerZoneId;
    CHECK(r.homestead.HarvestGarden() == 2);
    CHECK(r.inventory.Items().CountOf("c_hp_potion_s") == 2);
    CHECK(t.GardenStockCount() == 0);
    CHECK(r.homestead.BuyBlessing("ember_edge"));
    CHECK(t.Embers() == 25);
    r.homestead.OnEnterTower();
    CHECK_FALSE(t.Blessing().has_value());
    // Gem combine: three rubies -> one g_ruby_2 for 160 gold (hero level gate 15).
    for (int i = 0; i < 20; ++i) r.rewards.GrantExp(1000000, ExpSource::Debug);
    REQUIRE(r.hero->Level() >= 15);
    const LootContext lc{&PD(), &r.h.rng.Get(RngStream::Loot), &r.inventory.Uids()};
    std::optional<ItemInstance> gem = CreateItem(lc, "g_ruby_1", 1, ItemQuality::Normal);
    REQUIRE(gem.has_value());
    gem->quantity = 3;
    REQUIRE(r.inventory.Items().AddItem(*gem).ok);
    const int64_t gold = r.hero->Gold();
    CHECK(r.homestead.CombineGem("g_ruby_1"));
    CHECK(r.inventory.Items().CountOf("g_ruby_1") == 0);
    CHECK(r.inventory.Items().CountOf("g_ruby_2") == 1);
    CHECK(r.hero->Gold() == gold - 160);
    // Expedition: an idle owned beast leaves and comes back.
    REQUIRE(r.pets.AddPet("pet_sprite"));
    REQUIRE(r.pets.AddPet("pet_owl"));
    CHECK_FALSE(r.homestead.SendExpedition("pet_sprite", "short"));  // the active beast
    CHECK(r.homestead.SendExpedition("pet_owl", "short"));
    CHECK(r.pets.IsAway("pet_owl"));
    r.pets.SetActivePet("pet_owl");
    CHECK(r.pets.ActiveId() == "pet_sprite");
    r.homestead.Tick(10 * 60000.0);  // expeditions travel while the hero rests at home
    CHECK(r.homestead.Tower().ExpeditionDone());
    const int32_t embers = t.Embers();
    CHECK(r.homestead.ClaimExpedition());
    CHECK(t.Embers() > embers);
    CHECK_FALSE(r.pets.IsAway("pet_owl"));
  }

  // ===================================================================================================================
  // GameSim integration (Q3 slice)
  // ===================================================================================================================
  TEST_CASE("sim: the active beast follows into the world, feeds the equip stats, saves and loads") {
    auto sim = GameSim::Create(PD(), SimConfig{});
    REQUIRE(sim->NewGame(ClassId::Warrior, Difficulty::Normal, 99));
    SimContext& ctx = sim->Context();
    REQUIRE(ctx.sys.pets != nullptr);
    sim->Step();
    CHECK_FALSE(sim->View().pet.present);
    const double crit0 = ctx.equip.Get(Stat::CritRate);
    REQUIRE(ctx.sys.pets->AddPet("pet_cat"));
    REQUIRE(ctx.sys.pets->AddPet("pet_sprite"));
    sim->Step();
    CHECK(sim->View().pet.present);
    CHECK(sim->View().pet.petId == "pet_cat");
    // Passive: critRate enters EquipStats (expBonus / magicFind would not).
    CHECK(ctx.equip.Get(Stat::CritRate) == doctest::Approx(crit0 + PD().FindPet("pet_cat")->passiveBase));
    sim->Submit(CmdSetActivePet{"pet_sprite"});
    sim->Step();
    CHECK(sim->View().pet.petId == "pet_sprite");
    CHECK(ctx.equip.Get(Stat::CritRate) == doctest::Approx(crit0));
    CHECK(ctx.equip.Get(Stat::ExpBonus) == 0);  // expBonus applies at the kill, never through EquipStats
    for (int i = 0; i < 30; ++i) sim->Step();
    SaveData save;
    sim->BuildSave(save, 1234);
    CHECK(save.pets.present);
    REQUIRE(save.pets.owned.size() == 2);
    CHECK(save.pets.active == "pet_sprite");
    auto sim2 = GameSim::Create(PD(), SimConfig{});
    std::string err;
    REQUIRE(sim2->LoadGame(save, &err) == SaveError::None);
    sim2->Step();
    CHECK(sim2->View().pet.present);
    CHECK(sim2->View().pet.petId == "pet_sprite");
    REQUIRE(sim2->View().pets != nullptr);
    CHECK(sim2->View().pets->Owned().size() == 2);
  }
}
