// Pets-area test fixtures (moved out of pets_test.cpp so content_ch1_test.cpp shares them): data accessors, log /
// event helpers, the web tests' levelTo / seq / ctx helpers and PetRig (the companion and homestead systems over the
// live Chapter 1 zone).
#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "SimHarness.h"
#include "TestContent.h"
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
#include "../../Source/AbyssCore/Private/save/SaveSections.h"
#include "doctest/doctest.h"

namespace abyss::test::pets_fx {


inline const DataStore& PD() { return test::RealData(); }
inline const PetTables& PT() { return PD().Pets(); }
inline const HomesteadTables& HT() { return PD().Homestead(); }

// The ley sprite's cast release: the shipped clip's Release notify (assets.json SK_Pet_Sprite Cast01 releaseMs) when
// the art exists (the clip is authoritative, like every hero / monster contact beat); else the flying rig's 161 ms
// (quests-story-ch1.md 18.5.7).
inline double SpriteCastReleaseMs() {
  const AssetEntryDef* a = PD().Assets().FindByGameId("beast_pet_sprite");
  const AnimClipDef* c = a != nullptr ? a->FindClip("Cast01") : nullptr;
  return c != nullptr && c->hasReleaseMs ? c->releaseMs : 161.0;
}

inline const std::vector<std::string>& OriginalPetIds() {
  static const std::vector<std::string> ids = {"pet_sprite",  "pet_dragon",     "pet_owl",           "pet_cat",
                                               "pet_phoenix", "pet_storm_wolf", "pet_jade_tortoise", "pet_void_butterfly"};
  return ids;
}

inline const std::vector<std::string>& ChapterFinales() {
  static const std::vector<std::string> ids = {ch1::kQuestM5, "q_seal_dark_source", "q_kill_stone_guardian",
                                               "q_seal_fire_rift", "q_collect_demon_essence"};
  return ids;
}

// Log lines (keys) in emission order.
inline std::vector<std::string> PtLogKeys(const EventSink& s) {
  std::vector<std::string> out;
  for (const Event& e : s.Items()) {
    if (const EvLog* l = std::get_if<EvLog>(&e)) out.push_back(l->text.key);
  }
  return out;
}

inline std::vector<EvLog> PtLogs(const EventSink& s) {
  std::vector<EvLog> out;
  for (const Event& e : s.Items()) {
    if (const EvLog* l = std::get_if<EvLog>(&e)) out.push_back(*l);
  }
  return out;
}

template <class E>
inline std::vector<E> PtEvents(const EventSink& s) {
  std::vector<E> out;
  for (const Event& e : s.Items()) {
    if (const E* x = std::get_if<E>(&e)) out.push_back(*x);
  }
  return out;
}

// The web tests' levelTo: feeds exactly one level of exp at a time (silent).
inline void LevelTo(PetSystem& ps, std::string_view petId, int32_t level) {
  for (int guard = 0; guard < 100 && ps.Find(petId)->level < level; ++guard) {
    ps.AddExp(petId, static_cast<double>(PetExpToNext(PT().system, ps.Find(petId)->level)), true);
  }
}

inline std::vector<std::string> AbilityIds(const std::vector<const PetAbilityDef*>& list) {
  std::vector<std::string> out;
  for (const PetAbilityDef* a : list) out.push_back(a->id);
  return out;
}

inline const PetAbilityDef* PrimaryOf(std::string_view petId) { return PrimaryPetAbility(*PD().FindPet(petId)); }

// A fixed sequence repeated like the web tests' seq(...): scripts `count` values cycling through `values`.
inline void ScriptSeq(Rng& rng, std::initializer_list<double> values, size_t count) {
  std::vector<double> v;
  const std::vector<double> src(values);
  for (size_t i = 0; i < count; ++i) v.push_back(src[i % src.size()]);
  rng.Script(std::span<const double>(v));
}

inline JsonValue PtParse(std::string_view text) {
  JsonValue v;
  const bool ok = ParseJson(text, v);
  REQUIRE(ok);
  return v;
}

inline std::string PtJson(const std::function<void(JsonWriter&)>& write) {
  JsonWriter w;
  write(w);
  return w.Take();
}

inline std::string Render(const LocText& t) { return PD().Strings().T(t); }

// The web's choosePetAction test context (PetSystem.test.ts ctx()): owl abilities, target at 3, ready.
inline PetDecisionContext OwlCtx() {
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
    h.session.currentMap = ch1::kZone;
    ok = zone.EnterZone(ch1::kZone, true, heroPos);
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
    return SpawnDef(PD().FindMonster(defId), tile, state);
  }
  // A synthetic fixture (TestContent.h).
  EntityId Spawn(const MonsterDef& def, TilePos tile, MonsterState state = MonsterState::Chase) {
    return SpawnDef(&def, tile, state);
  }
  EntityId SpawnDef(const MonsterDef* def, TilePos tile, MonsterState state) {
    MonsterSpawnParams p;
    p.baseDef = def;
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

}  // namespace abyss::test::pets_fx
