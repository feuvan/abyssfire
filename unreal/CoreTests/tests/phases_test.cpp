// K4 boss phases and arenas (dark remake, implementation-plan.md 3 "K4", work package 2C): the schema (monster phases[]:
// loads, loader refusals, DataStore cross checks), the pure rules (monsters/Phases.h) and the runtime through
// MonsterSystem / CombatSystem: a single, in-order entry per threshold (a hit that crosses two enters both), the
// multipliers on top of elite affixes, abilities switched on / off, the enter action (rooted, interrupting, never
// invulnerable), phase summons and their one replacement, the shrinking-ring arena (radius over time, damage outside
// only), the tether, Feed, the reset on the M1 return home, the zealot's one-row frenzy table, the GameSim wiring (step
// hook, snapshot, boss bar), a zone exit, and a seeded replay whose event-log digest is pinned (GCC and Clang).
//
// Synthetic content only: monster definitions are built here (TestContent.h fixtures); one synthetic asset (`fx_k4_art`:
// Attack01 / Cast01 / Roar / Rise clips) and two synthetic definitions (fx_k4_risen, fx_k4_bearer) are added to the real
// export in memory; the wiring case reads the story boss id from the data (never spelled) and gives it a phase table in
// memory, so no content change moves these tests.
// Suite: monsters.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "MonstersFixtures.h"
#include "SimHarness.h"
#include "TestContent.h"
#include "TestUtil.h"
#include "abyss/base/Json.h"
#include "abyss/base/Math.h"
#include "abyss/base/StrUtil.h"
#include "abyss/data/DataStore.h"
#include "abyss/items/Inventory.h"
#include "abyss/monsters/Corpses.h"
#include "abyss/monsters/MonsterAbilities.h"
#include "abyss/monsters/Phases.h"
#include "abyss/pets/Homestead.h"
#include "abyss/pets/PetCompanion.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/sim/GameSim.h"
#include "abyss/story/StoryDirector.h"
#include "doctest/doctest.h"

using namespace abyss;

namespace {

using namespace abyss::test::monsters_fx;

constexpr char kArt[] = "fx_k4_art";  // the synthetic asset's game id
constexpr double kRoarMs = 1500;      // its Roar clip (MonsterAiDef::phaseActionMsDefault is 1200)

// Clips of the synthetic asset: Attack01 Contact 500 / 1000 ms, Cast01 Release 900 / 1500 ms, Roar 1500 ms, Rise 1200 ms.
constexpr char kK4AssetJson[] = R"({
  "kind": "SkeletalMesh", "category": "Monsters", "gameIds": ["fx_k4_art"], "skeleton": "SKEL_Human",
  "anims": [
    {"name": "Attack01", "lengthMs": 1000, "contactMs": 500, "notifies": [{"name": "Contact", "ms": 500}]},
    {"name": "Cast01", "lengthMs": 1500, "releaseMs": 900, "notifies": [{"name": "Release", "ms": 900}]},
    {"name": "Roar", "lengthMs": 1500, "notifies": [{"name": "FX_Roar", "ms": 450}]},
    {"name": "Rise", "lengthMs": 1200, "notifies": [{"name": "RiseUp", "ms": 840}]}
  ]
})";

JsonValue K4Parse(std::string_view text) {
  JsonValue v;
  JsonParseError err;
  const bool ok = ParseJson(text, v, &err);
  REQUIRE_MESSAGE(ok, "bad test JSON");
  return v;
}

using K4Edits = std::vector<std::pair<std::string, std::function<void(JsonValue&)>>>;

// The real export with `edits` applied in memory (several edits of one file run in order on one document).
std::unique_ptr<DataStore> K4LoadEdited(const K4Edits& edits, DataLoadReport& report) {
  std::map<std::string, JsonValue> docs;
  for (const auto& [file, edit] : edits) {
    if (docs.count(file) == 0) docs.emplace(file, K4Parse(test::ReadFile(test::DataDir() + "/" + file)));
    edit(docs.at(file));
  }
  std::map<std::string, std::string> texts;
  for (const auto& [file, doc] : docs) texts.emplace(file, WriteJson(doc));
  auto store = std::make_unique<DataStore>();
  store->LoadAll(
      [&](std::string_view name, std::string& out) {
        const auto it = texts.find(std::string(name));
        out = it != texts.end() ? it->second : test::ReadFile(test::DataDir() + "/" + std::string(name));
        return !out.empty();
      },
      report);
  return store;
}

void K4AddAsset(JsonValue& assets) {
  JsonValue* list = assets.FindMutable("manifest");
  REQUIRE(list != nullptr);
  list = list->FindMutable("assets");
  REQUIRE(list != nullptr);
  list->Set("SK_Fx_K4", K4Parse(kK4AssetJson));
}

// A monsters.json definition (with its exporter `derived` block) and extra top-level members (`extra`, JSON object
// members without braces).
std::string K4DefJson(std::string_view id, std::string_view extra, int level = 9, int hp = 1000) {
  return StrCat(R"({"id": ")", id, R"(", "name": ")", id, R"(", "level": )", level, R"(, "hp": )", hp,
                R"(, "damage": 20, "defense": 12,
    "speed": 48, "aggroRange": 8, "attackRange": 2, "attackSpeed": 1300, "expReward": 110, "goldReward": [30, 60],
    "spriteKey": "fx_k4_art", "animCategory": "humanoid")",
                extra.empty() ? "" : ", ", extra, R"(, "derived": {"isRanged": false, "onHitStatus": [],
    "projectileColor": 13386956, "homeZone": null, "nameKey": "data.monster.)",
                id, R"(", "sources": ["zone"]}})");
}

void K4AddDef(JsonValue& monsters, const std::string& json) {
  JsonValue def = K4Parse(json);
  const std::string id(def.Get("id").AsString());
  JsonValue* defs = monsters.FindMutable("defs");
  REQUIRE(defs != nullptr);
  defs->Set(id, std::move(def));
}

// The synthetic definitions a phase summons / a Feed eats (they must be data: the runtime looks them up).
void K4AddDefs(JsonValue& monsters) {
  K4AddDef(monsters, K4DefJson("fx_k4_risen", R"("family": "unburied", "corpse": {"leaves": "corpse"})", 2, 50));
  K4AddDef(monsters, K4DefJson("fx_k4_bearer", R"("family": "unburied", "corpse": {"leaves": "corpse"})", 7, 140));
}

// The real export plus the synthetic asset and definitions (loaded once).
const DataStore& K4Data() {
  static std::unique_ptr<DataStore> store;
  if (store == nullptr) {
    DataLoadReport report;
    store = K4LoadEdited({{"assets.json", K4AddAsset}, {"monsters.json", K4AddDefs}}, report);
    if (!report.Ok()) MESSAGE(report.Summary());
    REQUIRE(report.Ok());
  }
  return *store;
}

std::unique_ptr<DataStore> K4LoadDef(std::string defJson, DataLoadReport& report) {
  return K4LoadEdited({{"assets.json", K4AddAsset},
                       {"monsters.json", K4AddDefs},
                       {"monsters.json", [defJson](JsonValue& doc) { K4AddDef(doc, defJson); }}},
                      report);
}

bool K4HasIssue(const std::vector<DataIssue>& issues, std::string_view pathPart, std::string_view text) {
  for (const DataIssue& e : issues) {
    if (Contains(e.path, pathPart) && Contains(e.message, text)) return true;
  }
  return false;
}

// ---- synthetic definitions ------------------------------------------------------------------------------------------

MonsterPhaseDef K4Row(double below) {
  MonsterPhaseDef p;
  p.hpBelowPct = below;
  return p;
}

MonsterPhaseDef K4Roar(double below, AnimAction action = AnimAction::Roar) {
  MonsterPhaseDef p = K4Row(below);
  p.hasEnterAction = true;
  p.enterAction = action;
  return p;
}

// A boss on the synthetic art: level 9, 1000 HP, `damage` (1 keeps the hero standing), defense 12, speed 48, aggro 8,
// reach 2, a swing every 1300 ms.
MonsterDef K4Boss(std::vector<MonsterPhaseDef> rows, std::vector<MonsterAbilityDef> abilities = {}, double damage = 1,
                  const char* id = "fx_k4_boss") {
  MonsterDef d = test::fixture::MakeFixtureMonster(id, 9, 1000, damage, 12, 48, 8, 2.0, 1300, 110, 30, 60, kArt,
                                                   AnimRig::Humanoid, false, false);
  d.abilities = std::move(abilities);
  d.phases = std::move(rows);
  return d;
}

// A circle ability around the caster (aim self, r 3), contact 900 ms (its Cast01 Release).
MonsterAbilityDef K4Circle(std::string id, double cooldownMs, double initialCooldownMs = 0) {
  MonsterAbilityDef a;
  a.id = std::move(id);
  a.kind = AbilityKind::Circle;
  a.clip = "Cast01";
  a.aim = AbilityAim::Self;
  a.hasShape = true;
  a.shape.kind = AbilityShapeKind::Circle;
  a.shape.radius = 3;
  a.cooldownMs = cooldownMs;
  a.initialCooldownMs = initialCooldownMs;
  return a;
}

// The Red-Oath frenzy as one row (monsters.md 2.3): at <= 50 % life attack speed x1.35, move x1.2, damage taken x1.1,
// with its Roar. A zealot-like body: level 6, 90 HP, 11 damage, speed 52, reach 1.5, a swing every 1100 ms.
MonsterDef K4Zealot() {
  MonsterDef d = test::fixture::MakeFixtureMonster("fx_k4_zealot", 6, 90, 11, 6, 52, 6, 1.5, 1100, 23, 4, 8, kArt,
                                                   AnimRig::Humanoid, false, false);
  MonsterPhaseDef frenzy = K4Roar(50);
  frenzy.attackSpeedMul = 1.35;
  frenzy.speedMul = 1.2;
  frenzy.damageTakenMul = 1.1;
  d.phases.push_back(frenzy);
  return d;
}

// ---- runtime rig: the combat systems around a SimHarness on the synthetic data, stepped in the GameSim order (K4:
// TickPhases after TickCorpses), with the kill pipeline's K3 step wired as SimWiring does --------------------------------
struct K4Rig {
  explicit K4Rig(uint64_t seed = 7, Difficulty difficulty = Difficulty::Normal)
      : h(seed, K4Data()),
        hero(std::make_unique<Hero>(h.ctx.data, ClassId::Warrior)),
        status(h.ctx.data.Classes().statusRules),
        zone(h.ctx),
        loco(h.ctx),
        monsters(h.ctx),
        projectiles(h.ctx),
        combat(h.ctx),
        soul(h.ctx),
        rewards(h.ctx) {
    SimSystems& s = h.ctx.sys;
    s.hero = hero.get();
    s.status = &status;
    s.zone = &zone;
    s.locomotion = &loco;
    s.monsters = &monsters;
    s.projectiles = &projectiles;
    s.combat = &combat;
    s.soulEcho = &soul;
    s.rewards = &rewards;
    h.onTimer = [this](const Timer& t) {
      switch (t.owner) {
        case TimerOwner::Combat: combat.OnTimer(t); break;
        case TimerOwner::Projectiles: projectiles.OnTimer(t); break;
        case TimerOwner::Monsters: monsters.OnTimer(t); break;
        default: break;
      }
    };
    h.bus.Subscribe<MonsterKilledMsg>([this](const MonsterKilledMsg& m) {
      combat.OnMonsterKilled(m);
      monsters.OnMonsterDied(m);
      combat.OnMonsterDeathEffects(m);
      monsters.OnMonsterKilled(m);
    });
    h.session.difficulty = difficulty;
    h.session.currentMap = h.ctx.data.World().defaultMap;
    ok = zone.EnterZone(h.ctx.data.World().defaultMap, true, Vec2(40, 40));
    monsters.SetWorldForTesting(MOpenWorld(zone.Map().cols, zone.Map().rows));
    hero->Skills().InitStarterLevels();
    hero->RecalcDerived(h.ctx.equip);
    hero->FillHpMana();
    loco.OnZoneEnter();
    combat.OnZoneEnter();
    // The hero holds a lock on a far, idle dummy: it never swings, so the Combat stream only feeds the monsters' rolls.
    dummy = Spawn(test::fixture::Grunt(), Vec2(90, 90), MonsterState::Idle);
    hero->attackTarget = dummy;
    h.events.Clear();
  }

  void Step(int n = 1) {
    for (int i = 0; i < n; ++i) {
      if (keepHeroUp && hero->Life() == HeroLife::Alive) hero->SetHp(hero->MaxHp());
      h.Step(1);
      combat.ConsumeBufferedSkill();
      hero->RecalcDerived(h.ctx.equip);
      combat.TickPassives(kSimStepMs);
      loco.Tick(kSimStepMs);
      combat.TickHeroUpdate(kSimStepMs);
      monsters.TickAI(kSimStepMs);
      combat.TickCombat();
      combat.TickStatusEffects();
      combat.TickHazards();
      combat.TickOnDeath();
      monsters.TickCorpses();
      monsters.TickPhases();
      combat.TickCombatState();
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
  void StepUntilMs(double ms) {
    while (h.clock.NowMs() + 1e-9 < ms) Step();
  }
  double Now() const { return h.clock.NowMs(); }

  // A normal-difficulty-scaled monster at an exact position, no stat dodge / crit (deterministic rolls), in `state`.
  EntityId Spawn(const MonsterDef& def, Vec2 pos, MonsterState state = MonsterState::Attack,
                 MonsterRole role = MonsterRole::Regular) {
    MonsterSpawnParams p;
    p.baseDef = &def;
    p.tile = RoundToTile(pos);
    p.role = role;
    const EntityId id = monsters.Spawn(p);
    MonsterInstance* m = monsters.Find(id);
    REQUIRE(m != nullptr);
    m->pos = pos;
    m->prevPos = pos;
    m->stats.dex = 0;
    m->stats.lck = 0;
    m->state = state;
    return id;
  }
  MonsterInstance& M(EntityId id) {
    MonsterInstance* m = monsters.Find(id);
    REQUIRE(m != nullptr);
    return *m;
  }
  // A hit of `amount` on a monster from the hero (no provoke), through CombatSystem::DamageMonster.
  HitWeight Hit(EntityId id, double amount, DamageType element = DamageType::Physical, bool tick = false) {
    MonsterHitRequest r;
    r.monster = id;
    r.amount = amount;
    r.isTick = tick;
    r.element = element;
    r.attacker = kHeroEntityId;
    r.source = tick ? KillSource::StatusTick : KillSource::HeroSkill;
    r.provokes = false;
    return combat.DamageMonster(r);
  }
  // Hits a monster down to `pct` percent of its life (rounded down to a whole HP).
  void HitTo(EntityId id, double pct) {
    const MonsterInstance& m = M(id);
    const double target = std::floor(m.maxHp * pct / 100.0);
    REQUIRE(m.hp > target);
    Hit(id, m.hp - target);
  }
  void Kill(EntityId id, DamageType element = DamageType::Physical) { Hit(id, M(id).hp, element); }
  void PlaceHero(Vec2 p) { loco.Teleport(p, TeleportReason::Debug); }
  // A Raised body (no raiser) of a synthetic definition, at `pos`.
  EntityId Raised(std::string_view defId, Vec2 pos) {
    const MonsterDef* d = h.ctx.data.FindMonster(defId);
    REQUIRE(d != nullptr);
    const EntityId id = monsters.SummonAt(*d, pos, kNoEntity, 100, "test");
    REQUIRE(id != kNoEntity);
    M(id).abilityRootedUntilMs = 0;  // no Rise root
    M(id).state = MonsterState::Idle;
    return id;
  }
  Snapshot View() const {
    Snapshot s;
    monsters.FillSnapshot(s);
    return s;
  }
  std::vector<EntityId> RaisedOf(EntityId raiser) const {
    std::vector<EntityId> out;
    for (const MonsterInstance& m : monsters.All()) {
      if (m.IsAlive() && m.role == MonsterRole::Raised && m.raiserId == raiser) out.push_back(m.id);
    }
    return out;
  }

  test::SimHarness h;
  std::unique_ptr<Hero> hero;
  StatusEffectSystem status;
  ZoneRuntime zone;
  HeroLocomotion loco;
  MonsterSystem monsters;
  ProjectileSystem projectiles;
  CombatSystem combat;
  SoulEchoSystem soul;
  RewardService rewards;
  bool ok = false;
  bool keepHeroUp = true;  // refill the hero before each step (the fights here measure the monster, not the hero)
  EntityId dummy = kNoEntity;
};

// K4Rig plus a ley-beast out (the systems PetCompanion talks to), `petId` active. Step leaves the beast idle (its timers
// still fire); StepPet also runs its Tick (it picks a target and attacks).
struct K4PetRig : K4Rig {
  explicit K4PetRig(const std::string& petId)
      : inventory(h.ctx),
        quests(h.ctx.data, h.events, h.bus),
        homestead(h.ctx),
        pets(h.ctx.data, h.events, h.bus),
        companion(h.ctx) {
    SimSystems& s = h.ctx.sys;
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
    h.bus.Subscribe<PetChangedMsg>([this](const PetChangedMsg& m) { companion.OnPetChanged(m); });
    companion.OnZoneEnter();
    pets.AddPet(petId, true);
    pets.SetActivePet(petId);
    h.events.Clear();
  }
  void StepPet(int n = 1) {
    for (int i = 0; i < n; ++i) {
      Step();
      companion.Tick(kSimStepMs);
    }
  }
  void PlacePet(Vec2 p) {
    companion.MutableStateForTesting().pos = p;
    companion.MutableStateForTesting().prevPos = p;
  }

  InventorySystem inventory;
  QuestSystem quests;
  HomesteadSystem homestead;
  PetSystem pets;
  PetCompanion companion;
};

// The first beast of the data that fights in melee (its basic attack lands at a contact timer, no projectile).
std::string K4MeleePet() {
  for (const PetDef& p : K4Data().Pets().pets) {
    if (p.style == PetCombatStyle::Melee) return p.id;
  }
  return {};
}

template <class E>
std::vector<E> K4Events(const EventSink& sink) {
  std::vector<E> out;
  for (const Event& e : sink.Items()) {
    if (const E* p = std::get_if<E>(&e)) out.push_back(*p);
  }
  return out;
}

template <class E>
std::vector<E> K4Events(std::span<const Event> events) {
  std::vector<E> out;
  for (const Event& e : events) {
    if (const E* p = std::get_if<E>(&e)) out.push_back(*p);
  }
  return out;
}

// Arena ticks on the hero (tick hits from `source`).
std::vector<EvHit> ArenaTicks(const EventSink& sink, EntityId source) {
  std::vector<EvHit> out;
  for (const EvHit& e : K4Events<EvHit>(sink)) {
    if (e.target == kHeroEntityId && e.tick && e.source == source) out.push_back(e);
  }
  return out;
}

// Swing starts (EvPlayAnim Attack01) of `entity`.
std::vector<double> SwingStarts(const EventSink& sink, EntityId entity) {
  std::vector<double> out;
  for (const EvPlayAnim& a : K4Events<EvPlayAnim>(sink)) {
    if (a.entity == entity && a.action == AnimAction::Attack && a.clip == "Attack01") out.push_back(a.startMs);
  }
  return out;
}

// The order of the K4 events of `entity` in the log: 'P' phase, 'A' arena, 'C' cancel, 'R' enter-action anim.
std::string K4Sequence(const EventSink& sink, EntityId entity) {
  std::string out;
  for (const Event& e : sink.Items()) {
    if (const EvMonsterPhase* p = std::get_if<EvMonsterPhase>(&e)) {
      if (p->entity == entity) out += 'P';
    } else if (const EvArena* a = std::get_if<EvArena>(&e)) {
      if (a->entity == entity) out += 'A';
    } else if (const EvMonsterAttackCancelled* c = std::get_if<EvMonsterAttackCancelled>(&e)) {
      if (c->monster == entity) out += 'C';
    } else if (const EvPlayAnim* r = std::get_if<EvPlayAnim>(&e)) {
      if (r->entity == entity && r->action == AnimAction::Roar) out += 'R';
    }
  }
  return out;
}

// ---- replay digest --------------------------------------------------------------------------------------------------

struct K4Fnv {
  uint64_t h = 0xcbf29ce484222325ull;
  void Bytes(const void* p, size_t n) {
    const auto* b = static_cast<const unsigned char*>(p);
    for (size_t i = 0; i < n; ++i) {
      h ^= b[i];
      h *= 0x100000001b3ull;
    }
  }
  void U64(uint64_t v) {
    unsigned char b[8];
    for (int i = 0; i < 8; ++i) b[i] = static_cast<unsigned char>(v >> (8 * i));  // little-endian on every host
    Bytes(b, 8);
  }
  void D(double d) {
    uint64_t bits = 0;
    std::memcpy(&bits, &d, sizeof bits);
    U64(bits);
  }
  void V(Vec2 v) {
    D(v.x);
    D(v.y);
  }
  void S(std::string_view s) {
    U64(s.size());
    Bytes(s.data(), s.size());
  }
};

// Every event's variant index, and the fields of the ones a K4 fight produces.
void K4DigestEvents(K4Fnv& f, const EventSink& sink) {
  for (const Event& e : sink.Items()) {
    f.U64(e.index());
    if (const EvPlayAnim* a = std::get_if<EvPlayAnim>(&e)) {
      f.U64(a->entity);
      f.U64(static_cast<uint64_t>(a->action));
      f.S(a->clip);
      f.D(a->startMs);
      f.D(a->durationMs);
    } else if (const EvHit* hh = std::get_if<EvHit>(&e)) {
      f.U64(hh->target);
      f.U64(hh->source);
      f.D(hh->amount);
      f.U64((hh->crit ? 1u : 0u) | (hh->dodged ? 2u : 0u) | (hh->tick ? 4u : 0u) | (hh->killed ? 8u : 0u));
    } else if (const EvMonsterPhase* p = std::get_if<EvMonsterPhase>(&e)) {
      f.U64(p->entity);
      f.U64(static_cast<uint64_t>(p->phase));
      f.U64(p->reset ? 1u : 0u);
    } else if (const EvArena* ar = std::get_if<EvArena>(&e)) {
      f.U64(ar->entity);
      f.V(ar->centre);
      f.D(ar->radius);
      f.D(ar->startMs);
      f.U64(ar->cleared ? 1u : 0u);
    } else if (const EvMonsterFeed* fd = std::get_if<EvMonsterFeed>(&e)) {
      f.U64(fd->entity);
      f.U64(fd->fed);
      f.U64(fd->corpseId);
      f.D(fd->dueMs);
    } else if (const EvMonsterRaised* r = std::get_if<EvMonsterRaised>(&e)) {
      f.U64(r->entity);
      f.U64(r->raiser);
      f.S(r->abilityId);
      f.V(r->pos);
    } else if (const EvCorpseRemoved* c = std::get_if<EvCorpseRemoved>(&e)) {
      f.U64(c->corpseId);
      f.U64(static_cast<uint64_t>(c->reason));
      f.U64(c->by);
    } else if (const EvTelegraph* t = std::get_if<EvTelegraph>(&e)) {
      f.U64(t->entity);
      f.S(t->abilityId);
      f.V(t->origin);
      f.D(t->contactMs);
    } else if (const EvMonsterCollapsed* k = std::get_if<EvMonsterCollapsed>(&e)) {
      f.U64(k->entity);
      f.U64(k->raiser);
    }
  }
}

}  // namespace

TEST_SUITE("monsters") {
  // =================================================================================================================
  // Data: the phases schema (LoadMonsters.cpp) and DataStore::Finalize's cross checks
  // =================================================================================================================
  TEST_CASE("K4 data: a phase table loads with every field; defaults; abilities named by the switches") {
    const std::string extra = R"("abilities": [
        {"id": "sweep", "kind": "cone", "clip": "Attack01", "windupMs": 700, "cooldownMs": 5000, "aim": "hero",
         "shape": {"radius": 2.6, "arcDeg": 120}},
        {"id": "lunge", "kind": "line", "clip": "Attack01", "windupMs": 800, "cooldownMs": 7000, "aim": "hero",
         "shape": {"length": 5, "width": 1.2}}],
      "phases": [
        {"hpBelowPct": 65, "enterAction": "roar", "speedMul": 0.6, "leash": {"radius": 3},
         "summons": [{"monsterId": "fx_k4_bearer", "at": "anchors", "anchors": [{"col": 11, "row": 91}], "delayMs": 500,
                      "respawnMs": 20000, "respawns": 1, "hpPct": 100},
                     {"monsterId": "fx_k4_risen", "count": 2, "radius": 2.5, "hpPct": 40}],
         "feed": {"radius": 4, "healPct": 3, "delayMs": 1500}, "stinger": "fx_stinger_roll_call"},
        {"hpBelowPct": 30, "enterAction": "cast02", "attackSpeedMul": 1.25, "speedMul": 1.15, "damageTakenMul": 1.1,
         "abilitiesOn": ["lunge"], "abilitiesOff": ["sweep"],
         "leash": {"centre": {"col": 15, "row": 95}, "radius": 9},
         "arena": {"kind": "ringShrink", "centre": {"col": 15, "row": 95}, "fromR": 9, "toR": 5, "overMs": 40000,
                   "dpsPctMaxHp": 4, "damageType": "fire"},
         "feed": {"centre": {"col": 15, "row": 95}, "radius": 4, "healPct": 3}}])";
    DataLoadReport report;
    auto store = K4LoadDef(K4DefJson("fx_k4_full", extra), report);
    INFO(report.Summary());
    REQUIRE(report.Ok());
    const MonsterDef* d = store->FindMonster("fx_k4_full");
    REQUIRE(d != nullptr);
    REQUIRE(d->phases.size() == 2);
    const MonsterPhaseDef& p1 = d->phases[0];
    CHECK(p1.hpBelowPct == 65);
    CHECK((p1.hasEnterAction && p1.enterAction == AnimAction::Roar));
    CHECK((p1.speedMul == 0.6 && p1.attackSpeedMul == 1 && p1.damageTakenMul == 1));
    CHECK((p1.leash.present && !p1.leash.hasCentre && p1.leash.radius == 3));
    REQUIRE(p1.summons.size() == 2);
    const PhaseSummonDef& s0 = p1.summons[0];
    CHECK((s0.monsterId == "fx_k4_bearer" && s0.count == 1 && s0.at == AbilitySummonAt::Anchors));
    CHECK((s0.anchors.size() == 1 && s0.anchors[0] == TilePos{11, 91}));
    CHECK((s0.delayMs == 500 && s0.respawnMs == 20000 && s0.respawns == 1 && s0.hpPct == 100 && !s0.hasRadius));
    const PhaseSummonDef& s1 = p1.summons[1];
    CHECK((s1.count == 2 && s1.at == AbilitySummonAt::Self && s1.hasRadius && s1.radius == 2.5 && s1.hpPct == 40));
    CHECK((s1.delayMs == 0 && s1.respawns == 0));
    CHECK((p1.feed.present && !p1.feed.hasCentre && p1.feed.radius == 4 && p1.feed.healPct == 3 &&
           p1.feed.delayMs == 1500));
    CHECK(!p1.arena.present);
    CHECK(p1.stinger == "fx_stinger_roll_call");
    CHECK((p1.abilitiesOn.empty() && p1.abilitiesOff.empty()));
    const MonsterPhaseDef& p2 = d->phases[1];
    CHECK((p2.enterAction == AnimAction::Cast02 && p2.attackSpeedMul == 1.25 && p2.speedMul == 1.15 &&
           p2.damageTakenMul == 1.1));
    CHECK(p2.abilitiesOn == std::vector<std::string>{"lunge"});
    CHECK(p2.abilitiesOff == std::vector<std::string>{"sweep"});
    CHECK((p2.leash.hasCentre && p2.leash.centre == TilePos{15, 95} && p2.leash.radius == 9));
    CHECK((p2.arena.present && p2.arena.kind == ArenaKind::RingShrink && p2.arena.hasCentre &&
           p2.arena.centre == TilePos{15, 95}));
    CHECK((p2.arena.fromR == 9 && p2.arena.toR == 5 && p2.arena.overMs == 40000 && p2.arena.dpsPctMaxHp == 4 &&
           p2.arena.damageType == DamageType::Fire));
    CHECK((p2.feed.hasCentre && p2.feed.delayMs == 0));
    CHECK(p2.stinger.empty());
    // A definition without phases has none; the arena's damage type defaults to physical.
    const MonsterDef* plain = store->FindMonster("fx_k4_risen");
    REQUIRE(plain != nullptr);
    CHECK(plain->phases.empty());
    DataLoadReport r2;
    auto s2 = K4LoadDef(K4DefJson("fx_k4_arena", R"("phases": [{"hpBelowPct": 50, "arena": {"kind": "ringShrink",
        "fromR": 6, "toR": 6, "overMs": 1000, "dpsPctMaxHp": 1}}])"),
                        r2);
    REQUIRE(r2.Ok());
    CHECK(s2->FindMonster("fx_k4_arena")->phases[0].arena.damageType == DamageType::Physical);
    CHECK(!s2->FindMonster("fx_k4_arena")->phases[0].hasEnterAction);
  }

  TEST_CASE("K4 data: the loader refuses broken phase tables") {
    struct Bad {
      std::string phases;
      const char* error;
    };
    const char* abilities = R"("abilities": [{"id": "a", "kind": "circle", "clip": "Cast01", "cooldownMs": 0,
        "aim": "self", "shape": {"radius": 2}}], )";
    const std::vector<Bad> bad = {
        {R"([{"hpBelowPct": 50, "frenzy": true}])", "unknown phase field 'frenzy'"},
        {R"([{"speedMul": 1.2}])", "hpBelowPct"},
        {R"([{"hpBelowPct": 0}])", "0 < hpBelowPct <= 100"},
        {R"([{"hpBelowPct": 120}])", "0 < hpBelowPct <= 100"},
        {R"([{"hpBelowPct": 50}, {"hpBelowPct": 50}])", "strictly decrease"},
        {R"([{"hpBelowPct": 30}, {"hpBelowPct": 60}])", "strictly decrease"},
        {R"([{"hpBelowPct": 50, "enterAction": "idle"}])", "not idle / walk / death"},
        {R"([{"hpBelowPct": 50, "enterAction": "death"}])", "not idle / walk / death"},
        {R"([{"hpBelowPct": 50, "enterAction": "howl"}])", "unknown value 'howl'"},
        {R"([{"hpBelowPct": 50, "speedMul": 0}])", "0 < speedMul <= 10"},
        {R"([{"hpBelowPct": 50, "attackSpeedMul": 11}])", "0 < attackSpeedMul <= 10"},
        {R"([{"hpBelowPct": 50, "damageTakenMul": -1}])", "0 < damageTakenMul <= 10"},
        {R"([{"hpBelowPct": 50, "leash": {"radius": 0}}])", "0 < radius <= 30"},
        {R"([{"hpBelowPct": 50, "leash": {"radius": 31}}])", "0 < radius <= 30"},
        {R"([{"hpBelowPct": 50, "leash": {"radius": 3, "home": true}}])", "unknown leash field 'home'"},
        {R"([{"hpBelowPct": 50, "leash": {"radius": 3, "centre": {"col": 1, "row": 2, "z": 0}}}])",
         "unknown centre field 'z'"},
        {R"([{"hpBelowPct": 50, "abilitiesOn": ["nope"]}])", "'nope' names no ability"},
        {R"([{"hpBelowPct": 50, "abilitiesOff": ["nope"]}])", "'nope' names no ability"},
        {R"([{"hpBelowPct": 50, "abilitiesOn": ["a"], "abilitiesOff": ["a"]}])", "switched both on and off"},
        {R"([{"hpBelowPct": 50, "summons": [{"monsterId": ""}]}])", "empty monster id"},
        {R"([{"hpBelowPct": 50, "summons": [{"monsterId": "fx_k4_risen", "count": 0}]}])", "1 <= count <= 20"},
        {R"([{"hpBelowPct": 50, "summons": [{"monsterId": "fx_k4_risen", "count": 21}]}])", "1 <= count <= 20"},
        {R"([{"hpBelowPct": 50, "summons": [{"monsterId": "fx_k4_risen", "at": "corpse"}]}])", "'self' or 'anchors'"},
        {R"([{"hpBelowPct": 50, "summons": [{"monsterId": "fx_k4_risen", "at": "anchors"}]}])", "needs anchors"},
        {R"([{"hpBelowPct": 50, "summons": [{"monsterId": "fx_k4_risen", "anchors": [{"col": 1, "row": 1}]}]}])",
         "anchors need at 'anchors'"},
        {R"([{"hpBelowPct": 50, "summons": [{"monsterId": "fx_k4_risen", "at": "anchors", "anchors": [{"col": 1,
            "row": 1}], "radius": 2}]}])",
         "no radius"},
        {R"([{"hpBelowPct": 50, "summons": [{"monsterId": "fx_k4_risen", "radius": 0}]}])", "0 < radius <= 30"},
        {R"([{"hpBelowPct": 50, "summons": [{"monsterId": "fx_k4_risen", "hpPct": 0}]}])", "0 < hpPct <= 100"},
        {R"([{"hpBelowPct": 50, "summons": [{"monsterId": "fx_k4_risen", "delayMs": 70000}]}])", "0 <= delayMs <= 60000"},
        {R"([{"hpBelowPct": 50, "summons": [{"monsterId": "fx_k4_risen", "respawnMs": 700000}]}])",
         "0 <= respawnMs <= 600000"},
        {R"([{"hpBelowPct": 50, "summons": [{"monsterId": "fx_k4_risen", "respawns": 11}]}])", "0 <= respawns <= 10"},
        {R"([{"hpBelowPct": 50, "summons": [{"monsterId": "fx_k4_risen", "every": 10}]}])",
         "unknown phase summon field 'every'"},
        {R"([{"hpBelowPct": 50, "summons": [{"monsterId": "fx_k4_risen", "at": "anchors", "anchors": [{"col": 1,
            "row": 1, "z": 2}]}]}])",
         "unknown anchor field 'z'"},
        {R"([{"hpBelowPct": 50, "arena": {"kind": "ringGrow", "fromR": 9, "toR": 5, "overMs": 1, "dpsPctMaxHp": 4}}])",
         "unknown value 'ringGrow'"},
        {R"([{"hpBelowPct": 50, "arena": {"fromR": 9, "toR": 5, "overMs": 1, "dpsPctMaxHp": 4}}])",
         "expected an enum string"},
        {R"([{"hpBelowPct": 50, "arena": {"kind": "ringShrink", "fromR": 0, "toR": 0, "overMs": 1, "dpsPctMaxHp": 4}}])",
         "0 < fromR <= 60"},
        {R"([{"hpBelowPct": 50, "arena": {"kind": "ringShrink", "fromR": 61, "toR": 5, "overMs": 1, "dpsPctMaxHp": 4}}])",
         "0 < fromR <= 60"},
        {R"([{"hpBelowPct": 50, "arena": {"kind": "ringShrink", "fromR": 5, "toR": 9, "overMs": 1, "dpsPctMaxHp": 4}}])",
         "a ring shrinks"},
        {R"([{"hpBelowPct": 50, "arena": {"kind": "ringShrink", "fromR": 5, "toR": 0, "overMs": 1, "dpsPctMaxHp": 4}}])",
         "a ring shrinks"},
        {R"([{"hpBelowPct": 50, "arena": {"kind": "ringShrink", "fromR": 9, "toR": 5, "overMs": 0, "dpsPctMaxHp": 4}}])",
         "0 < overMs <= 600000"},
        {R"([{"hpBelowPct": 50, "arena": {"kind": "ringShrink", "fromR": 9, "toR": 5, "overMs": 700000,
            "dpsPctMaxHp": 4}}])",
         "0 < overMs <= 600000"},
        {R"([{"hpBelowPct": 50, "arena": {"kind": "ringShrink", "fromR": 9, "toR": 5, "overMs": 1, "dpsPctMaxHp": 0}}])",
         "0 < dpsPctMaxHp <= 100"},
        {R"([{"hpBelowPct": 50, "arena": {"kind": "ringShrink", "fromR": 9, "toR": 5, "overMs": 1, "dpsPctMaxHp": 101}}])",
         "0 < dpsPctMaxHp <= 100"},
        {R"([{"hpBelowPct": 50, "arena": {"kind": "ringShrink", "fromR": 9, "toR": 5, "overMs": 1, "dpsPctMaxHp": 4,
            "damageType": "holy"}}])",
         "unknown value 'holy'"},
        {R"([{"hpBelowPct": 50, "arena": {"kind": "ringShrink", "fromR": 9, "toR": 5, "overMs": 1, "dpsPctMaxHp": 4,
            "tickMs": 250}}])",
         "unknown arena field 'tickMs'"},
        {R"([{"hpBelowPct": 50, "feed": {"radius": 0, "healPct": 3}}])", "0 < radius <= 30"},
        {R"([{"hpBelowPct": 50, "feed": {"radius": 4, "healPct": 0}}])", "0 < healPct <= 100"},
        {R"([{"hpBelowPct": 50, "feed": {"radius": 4, "healPct": 3, "delayMs": 20000}}])", "0 <= delayMs <= 10000"},
        {R"([{"hpBelowPct": 50, "feed": {"radius": 4, "healPct": 3, "family": "unburied"}}])",
         "unknown feed field 'family'"},
        {R"([{"hpBelowPct": 50, "stinger": ""}])", "an empty stinger id"},
    };
    for (const Bad& b : bad) {
      INFO(b.phases);
      DataLoadReport report;
      K4LoadDef(K4DefJson("fx_k4_bad", StrCat(abilities, R"("phases": )", b.phases)), report);
      CHECK_FALSE(report.Ok());
      bool found = false;
      for (const DataIssue& e : report.errors) found = found || Contains(e.message, b.error);
      if (!found) MESSAGE(report.Summary());
      CHECK(found);
    }
  }

  TEST_CASE("K4 data: Finalize checks phase summons, abilities of unreachable phases and the enter action's clip") {
    DataLoadReport report;
    K4LoadDef(K4DefJson("fx_k4_fin", R"("abilities": [
        {"id": "late", "kind": "circle", "clip": "Cast01", "cooldownMs": 0, "aim": "self", "shape": {"radius": 2},
         "phases": [2, 3]},
        {"id": "ok", "kind": "circle", "clip": "Cast01", "cooldownMs": 0, "aim": "self", "shape": {"radius": 2},
         "phases": [1, 5]}],
      "phases": [{"hpBelowPct": 50, "enterAction": "taunt", "summons": [{"monsterId": "fx_no_such_monster"}]}])"),
              report);
    CHECK(K4HasIssue(report.errors, "fx_k4_fin.phases[0].summons", "unknown monster 'fx_no_such_monster'"));
    CHECK(K4HasIssue(report.warnings, "fx_k4_fin.abilities.late.phases", "never usable"));
    CHECK_FALSE(K4HasIssue(report.warnings, "fx_k4_fin.abilities.ok.phases", "never usable"));
    CHECK(K4HasIssue(report.warnings, "fx_k4_fin.phases[0].enterAction", "no clip 'Taunt'"));
    // The art has a Roar clip: no warning; a monster without phases that lists phase 1 is warned.
    DataLoadReport ok;
    K4LoadDef(K4DefJson("fx_k4_roar", R"("abilities": [{"id": "p1", "kind": "circle", "clip": "Cast01",
        "cooldownMs": 0, "aim": "self", "shape": {"radius": 2}, "phases": [1]}])"),
              ok);
    REQUIRE(ok.Ok());
    CHECK(K4HasIssue(ok.warnings, "fx_k4_roar.abilities.p1.phases", "reaches phases 0..0"));
    DataLoadReport roar;
    K4LoadDef(K4DefJson("fx_k4_roar2", R"("phases": [{"hpBelowPct": 50, "enterAction": "roar"}])"), roar);
    REQUIRE(roar.Ok());
    CHECK_FALSE(K4HasIssue(roar.warnings, "fx_k4_roar2", "enterAction"));
  }

  TEST_CASE("K4 data: Finalize warns on a tether reaching past the M1 leash and on a hunt's unreachable phases") {
    const DataStore& real = test::RealData();
    const MonsterAiDef& ai = real.Monsters().ai;
    const std::string boss = ai.storyBossId;  // read, never spelled
    REQUIRE(!boss.empty());
    TilePos home{-1, -1};
    for (const MapDef& m : real.World().maps) {
      for (const MapSpawnDef& s : m.spawns) {
        if (s.monsterId == boss && home.col < 0) home = s.pos;
      }
    }
    REQUIRE(home.col >= 0);
    REQUIRE(!real.Monsters().hunts.empty());
    const std::string hunt = real.Monsters().hunts.front().huntId;
    // The story boss stands on a map spawn: its anchor may lie spawnJitter tiles per axis off the authored tile, so a
    // default-centred tether of r reaches r + jitter x sqrt 2 + the slack from it. The rows below straddle leashRange.
    const double jitter = ai.spawnJitter * std::sqrt(2.0);
    const double fits = std::floor(ai.leashRange - jitter - kTetherSlackTiles);
    REQUIRE(fits >= 1);
    REQUIRE(fits + 1 + jitter + kTetherSlackTiles > ai.leashRange);
    const double offset = std::ceil(ai.leashRange - jitter - kTetherSlackTiles - 1 + 0.01);  // with r 1
    DataLoadReport report;
    K4LoadEdited({{"assets.json", K4AddAsset},
                  {"monsters.json", K4AddDefs},
                  {"monsters.json",
                   [&](JsonValue& doc) {
                     // No map spawn: the anchor is the tile, so only r + slack counts.
                     K4AddDef(doc, K4DefJson("fx_k4_t_in", StrCat(R"("phases": [{"hpBelowPct": 50, "leash": {"radius": )",
                                                                  ai.leashRange - kTetherSlackTiles, "}}]")));
                     K4AddDef(doc, K4DefJson("fx_k4_t_out", StrCat(R"("phases": [{"hpBelowPct": 50, "leash": {"radius": )",
                                                                   ai.leashRange - kTetherSlackTiles + 0.05, "}}]")));
                     JsonValue* def = doc.FindMutable("defs")->FindMutable(boss);
                     REQUIRE(def != nullptr);
                     def->Set("phases",
                              K4Parse(StrCat(R"([{"hpBelowPct": 70, "leash": {"radius": )", fits, R"(}},
                                {"hpBelowPct": 60, "leash": {"radius": )", fits + 1, R"(}},
                                {"hpBelowPct": 50, "leash": {"centre": {"col": )", home.col, R"(, "row": )", home.row,
                                             R"(}, "radius": )", fits, R"(}},
                                {"hpBelowPct": 40, "leash": {"centre": {"col": )", home.col + offset, R"(, "row": )",
                                             home.row, R"(}, "radius": 1}}])")));
                   }},
                  {"quest_hunts.json",
                   [&](JsonValue& doc) {
                     JsonValue* def = doc.FindMutable("defsNormal")->FindMutable(hunt);
                     REQUIRE(def != nullptr);
                     def->Set("abilities", K4Parse(R"([{"id": "late", "kind": "circle", "clip": "Cast01", "cooldownMs": 0,
                        "aim": "self", "shape": {"radius": 2}, "phases": [3]}])"));
                     def->Set("phases", K4Parse(StrCat(R"([{"hpBelowPct": 50, "leash": {"radius": )", ai.leashRange,
                                                       "}}]")));
                   }}},
                 report);
    INFO(report.Summary());
    CHECK(report.Ok());
    CHECK_FALSE(K4HasIssue(report.warnings, "fx_k4_t_in.phases[0].leash", "past the M1 leash"));
    CHECK(K4HasIssue(report.warnings, "fx_k4_t_out.phases[0].leash", "past the M1 leash"));
    CHECK_FALSE(K4HasIssue(report.warnings, boss + ".phases[0].leash", "past the M1 leash"));
    CHECK(K4HasIssue(report.warnings, boss + ".phases[1].leash", "past the M1 leash"));
    CHECK_FALSE(K4HasIssue(report.warnings, boss + ".phases[2].leash", "past the M1 leash"));  // centred on the tile
    CHECK(K4HasIssue(report.warnings, boss + ".phases[3].leash", "past the M1 leash"));        // centred off it
    // A hunt's normal definition is checked too (its spot is its anchor).
    CHECK(K4HasIssue(report.warnings, "defsNormal." + hunt + ".abilities.late.phases", "never usable"));
    CHECK(K4HasIssue(report.warnings, "defsNormal." + hunt + ".phases[0].leash", "past the M1 leash"));
  }

  // =================================================================================================================
  // Pure rules (monsters/Phases.h) and the picker's phase switches (MonsterAbilities.h)
  // =================================================================================================================
  TEST_CASE("K4 rules: entry order, switches, the arena's radius / edge / tick, the tether goal, scaled damage, clips") {
    // NextPhaseToEnter: strictly below the next row's threshold, one row at a time.
    const std::vector<MonsterPhaseDef> rows = {K4Row(75), K4Row(50), K4Row(25)};
    CHECK(NextPhaseToEnter(rows, 0, 100) == -1);
    CHECK(NextPhaseToEnter(rows, 0, 75) == -1);  // the threshold itself is not below it
    CHECK(NextPhaseToEnter(rows, 0, 74.99) == 0);
    CHECK(NextPhaseToEnter(rows, 0, 10) == 0);   // a deep hit still enters row 0 first
    CHECK(NextPhaseToEnter(rows, 1, 10) == 1);
    CHECK(NextPhaseToEnter(rows, 2, 10) == 2);
    CHECK(NextPhaseToEnter(rows, 3, 1) == -1);   // every row entered
    CHECK(NextPhaseToEnter(rows, 1, 60) == -1);  // healed above: nothing more (and nothing back)
    CHECK(NextPhaseToEnter({}, 0, 1) == -1);
    // PhaseAbilitiesOff: an ability first switched on (in row order) starts off, one first switched off starts on; rows
    // switch in sequence (a: off in row 0, on again in row 2; b: on, off, on; c: on in row 3; d: never switched).
    MonsterDef d = K4Boss({K4Row(80), K4Row(60), K4Row(40), K4Row(20)},
                          {K4Circle("a", 0), K4Circle("b", 0), K4Circle("c", 0), K4Circle("d", 0)});
    d.phases[0].abilitiesOn = {"b"};
    d.phases[0].abilitiesOff = {"a"};
    d.phases[1].abilitiesOff = {"b"};
    d.phases[2].abilitiesOn = {"b", "a"};
    d.phases[3].abilitiesOn = {"c"};
    std::vector<uint8_t> off;
    PhaseAbilitiesOff(d, 0, off);
    CHECK(off == std::vector<uint8_t>{0, 1, 1, 0});
    PhaseAbilitiesOff(d, 1, off);
    CHECK(off == std::vector<uint8_t>{1, 0, 1, 0});
    PhaseAbilitiesOff(d, 2, off);
    CHECK(off == std::vector<uint8_t>{1, 1, 1, 0});
    PhaseAbilitiesOff(d, 3, off);
    CHECK(off == std::vector<uint8_t>{0, 0, 1, 0});
    PhaseAbilitiesOff(d, 4, off);
    CHECK(off == std::vector<uint8_t>{0, 0, 0, 0});
    PhaseAbilitiesOff(d, 99, off);  // clamped to the table
    CHECK(off == std::vector<uint8_t>{0, 0, 0, 0});
    PhaseAbilitiesOff(K4Boss({}, {K4Circle("a", 0)}), 0, off);
    CHECK(off.empty());  // no phase table: nothing is switched
    // AbilityEnabledInPhase: the switch and the ability's own phases[].
    MonsterAbilityDef listed = K4Circle("l", 0);
    listed.phases = {1, 3};
    const std::vector<uint8_t> none;
    const std::vector<uint8_t> offAt0 = {1};
    CHECK_FALSE(AbilityEnabledInPhase(listed, 0, none, 0));
    CHECK(AbilityEnabledInPhase(listed, 0, none, 1));
    CHECK_FALSE(AbilityEnabledInPhase(listed, 0, none, 2));
    CHECK_FALSE(AbilityEnabledInPhase(listed, 0, offAt0, 1));
    CHECK(AbilityEnabledInPhase(K4Circle("x", 0), 0, none, 7));
    // The picker skips a switched-off ability (the next one by priority takes the swing).
    std::vector<MonsterAbilityDef> picks = {K4Circle("hi", 0), K4Circle("lo", 0)};
    picks[0].priority = 2;
    const std::vector<double> ready = {0, 0};
    AbilityPickInput in;
    in.nowMs = 1000;
    in.heroDist = 1;
    in.readyAtMs = ready;
    CHECK(PickMonsterAbility(picks, in) == 0);
    const std::vector<uint8_t> hiOff = {1, 0};
    in.off = hiOff;
    CHECK(PickMonsterAbility(picks, in) == 1);
    const std::vector<uint8_t> allOff = {1, 1};
    in.off = allOff;
    CHECK(PickMonsterAbility(picks, in) == -1);
    // ArenaRadiusAt: linear from fromR to toR over overMs, then stays.
    CHECK(ArenaRadiusAt(9, 5, 40000, 0) == 9);
    CHECK(ArenaRadiusAt(9, 5, 40000, 10000) == 8);
    CHECK(ArenaRadiusAt(9, 5, 40000, 20000) == 7);
    CHECK(ArenaRadiusAt(9, 5, 40000, 40000) == 5);
    CHECK(ArenaRadiusAt(9, 5, 40000, 90000) == 5);
    CHECK(ArenaRadiusAt(9, 5, 40000, -500) == 9);
    CHECK(ArenaRadiusAt(6, 6, 1000, 500) == 6);
    // ArenaOutside: strictly beyond the radius (the edge is inside).
    CHECK_FALSE(ArenaOutside(Vec2(10, 10), 5, Vec2(10, 10)));
    CHECK_FALSE(ArenaOutside(Vec2(10, 10), 5, Vec2(15, 10)));
    CHECK_FALSE(ArenaOutside(Vec2(10, 10), 5, Vec2(13, 14)));
    CHECK(ArenaOutside(Vec2(10, 10), 5, Vec2(15.01, 10)));
    CHECK(ArenaOutside(Vec2(10, 10), 5, Vec2(10, 4.9)));
    // ArenaTickDamage: floor(maxHp x pct / 100 x tick / 1000 x mul), at least 1.
    CHECK(ArenaTickDamage(300, 4, 500, 1) == 6);
    CHECK(ArenaTickDamage(310, 4, 500, 1) == 6);    // 6.2
    CHECK(ArenaTickDamage(300, 4, 500, 2) == 12);   // Hell's damage multiplier
    CHECK(ArenaTickDamage(300, 4, 500, 1.5) == 9);
    CHECK(ArenaTickDamage(20, 4, 500, 1) == 1);     // 0.4: at least 1
    CHECK(ArenaTickDamage(250, 4, 500, 1) == 5);    // 5 exactly
    // TetherGoal: inside stays; outside -> the circle's nearest point; on the centre -> the centre.
    CHECK(TetherGoal(Vec2(10, 10), 3, Vec2(11, 11)) == Vec2(11, 11));
    CHECK(TetherGoal(Vec2(10, 10), 3, Vec2(13, 10)) == Vec2(13, 10));
    CHECK(TetherGoal(Vec2(10, 10), 3, Vec2(20, 10)) == Vec2(13, 10));
    const Vec2 g = TetherGoal(Vec2(10, 10), 3, Vec2(14, 14));
    CHECK(Dist(g, Vec2(10, 10)) == doctest::Approx(3).epsilon(1e-12));
    CHECK(g.x == doctest::Approx(g.y));
    CHECK(TetherGoal(Vec2(10, 10), 0, Vec2(10, 10)) == Vec2(10, 10));
    // PhaseScaledDamage: x1 untouched; floor(amount x mul), never below min(amount, 1).
    CHECK(PhaseScaledDamage(37, 1) == 37);
    CHECK(PhaseScaledDamage(0.5, 1) == 0.5);
    CHECK(PhaseScaledDamage(100, 1.1) == 110);
    CHECK(PhaseScaledDamage(15, 1.1) == 16);   // 16.5
    CHECK(PhaseScaledDamage(100, 1.15) == 115);  // 114.99999999999999 before the slack
    CHECK(PhaseScaledDamage(10, 0.5) == 5);
    CHECK(PhaseScaledDamage(1, 0.5) == 1);     // a landed hit stays a hit
    CHECK(PhaseScaledDamage(0.5, 0.5) == 0.5);
    CHECK(PhaseScaledDamage(3, 1.35) == 4);
    // PhaseActionClip: the clip of the action.
    CHECK(PhaseActionClip(AnimAction::Roar) == "Roar");
    CHECK(PhaseActionClip(AnimAction::Taunt) == "Taunt");
    CHECK(PhaseActionClip(AnimAction::Cast02) == "Cast02");
    CHECK(PhaseActionClip(AnimAction::Attack02) == "Attack02");
    CHECK(PhaseActionClip(AnimAction::Attack) == "Attack01");
    CHECK(PhaseActionClip(AnimAction::Cast) == "Cast01");
    CHECK(PhaseActionClip(AnimAction::Stun) == "Stun");
  }

  // =================================================================================================================
  // Runtime (MonsterSystem::TickPhases and the systems it drives)
  // =================================================================================================================
  TEST_CASE("K4 runtime: each threshold is entered once, in order; a hit that crosses two enters both in that step") {
    K4Rig rig;
    std::vector<MonsterPhaseDef> rows = {K4Row(75), K4Row(50), K4Row(25)};
    rows[0].stinger = "fx_s1";
    rows[1].stinger = "fx_s2";
    rows[2].stinger = "fx_s3";
    const MonsterDef def = K4Boss(rows);
    rig.PlaceHero(Vec2(70, 40));
    const EntityId b = rig.Spawn(def, Vec2(40, 40), MonsterState::Idle);
    CHECK(rig.M(b).phase == 0);
    rig.Hit(b, 250);  // exactly 75 %: not below the threshold
    rig.Step();
    CHECK(K4Events<EvMonsterPhase>(rig.h.events).empty());
    CHECK(rig.M(b).phase == 0);
    rig.Hit(b, 1);
    rig.h.events.Clear();
    rig.Step();
    std::vector<EvMonsterPhase> ev = K4Events<EvMonsterPhase>(rig.h.events);
    REQUIRE(ev.size() == 1);
    CHECK((ev[0].entity == b && ev[0].phase == 1 && ev[0].phaseCount == 3 && ev[0].hpBelowPct == 75 &&
           ev[0].stinger == "fx_s1" && !ev[0].reset));
    CHECK(rig.M(b).phase == 1);
    // More damage inside the phase and a heal back above every threshold enter nothing, and leave nothing.
    rig.Hit(b, 50);
    rig.monsters.Heal(b, 10000);
    REQUIRE(rig.M(b).hp == rig.M(b).maxHp);
    rig.h.events.Clear();
    rig.Step(60);
    CHECK(K4Events<EvMonsterPhase>(rig.h.events).empty());
    CHECK(rig.M(b).phase == 1);
    // One hit from full life to 10 %: rows 1 and 2 (50, 25) are both entered, in order, in this step.
    rig.Hit(b, 900);
    rig.h.events.Clear();
    rig.Step();
    ev = K4Events<EvMonsterPhase>(rig.h.events);
    REQUIRE(ev.size() == 2);
    CHECK((ev[0].phase == 2 && ev[0].hpBelowPct == 50 && ev[0].stinger == "fx_s2"));
    CHECK((ev[1].phase == 3 && ev[1].hpBelowPct == 25 && ev[1].stinger == "fx_s3"));
    CHECK(rig.M(b).phase == 3);
    rig.Hit(b, 50);
    rig.h.events.Clear();
    rig.Step(30);
    CHECK(K4Events<EvMonsterPhase>(rig.h.events).empty());
    // A fresh instance crosses all three at once: three entries in order.
    const EntityId c = rig.Spawn(def, Vec2(45, 40), MonsterState::Idle);
    rig.Hit(c, 999);
    rig.h.events.Clear();
    rig.Step();
    ev = K4Events<EvMonsterPhase>(rig.h.events);
    REQUIRE(ev.size() == 3);
    CHECK((ev[0].phase == 1 && ev[1].phase == 2 && ev[2].phase == 3));
    // A kill is no entry: a monster killed through every threshold enters none.
    const EntityId k = rig.Spawn(def, Vec2(50, 40), MonsterState::Idle);
    rig.Kill(k);
    rig.h.events.Clear();
    rig.Step();
    CHECK(K4Events<EvMonsterPhase>(rig.h.events).empty());
  }

  TEST_CASE("K4 runtime: multipliers compose with elite affixes: move speed, the swing interval, damage taken") {
    K4Rig rig;
    MonsterPhaseDef row = K4Row(50);
    row.speedMul = 1.2;
    row.attackSpeedMul = 1.35;
    row.damageTakenMul = 1.1;
    const MonsterDef def = K4Boss({row});
    rig.PlaceHero(Vec2(41.5, 40));
    const EntityId b = rig.Spawn(def, Vec2(40, 40), MonsterState::Attack);
    const std::array<EliteAffixType, 1> swift = {EliteAffixType::Swift};
    ApplyEliteAffixes(rig.M(b), swift, rig.h.ctx.data.Combat().eliteAffixes, rig.h.ctx.data.Monsters().ai);
    const double affixedSpeed = rig.M(b).def.speed;
    CHECK(affixedSpeed == std::floor(48 * rig.h.ctx.data.Combat().eliteAffixes.Def(EliteAffixType::Swift).speedMult));
    const MonsterAiDef& ai = rig.h.ctx.data.Monsters().ai;
    const auto firstStep = [&](double phaseMul) {  // one moveToward step from rest
      MonsterInstance probe = rig.M(b);
      probe.moveSpeed = 0;
      MonsterMoveToward(probe, ai, probe.pos + Vec2(5, 0), kSimStepMs, 1.0, MOpenWorld());
      const double top = affixedSpeed * ai.moveSpeedScale * 1.0 * phaseMul;
      CHECK(probe.moveSpeed == doctest::Approx(top * ai.moveAccel * (kSimStepMs / 1000.0)).epsilon(1e-12));
    };
    // Phase 0: the affixed definition as it is.
    firstStep(1.0);
    CHECK(rig.M(b).AttackIntervalMs() == 1300);
    const double hp0 = rig.M(b).hp;
    rig.Hit(b, 100);
    CHECK(rig.M(b).hp == hp0 - 100);
    // Swings every 1300 ms before the frenzy.
    rig.h.events.Clear();
    rig.Step(240);
    std::vector<double> starts = SwingStarts(rig.h.events, b);
    REQUIRE(starts.size() >= 3);
    for (size_t i = 1; i < starts.size(); ++i) {
      CHECK(starts[i] - starts[i - 1] >= 1300 - 1e-6);
      CHECK(starts[i] - starts[i - 1] < 1300 + kSimStepMs);
    }
    // Below half life: x1.2 on top of Swift, 1300 / 1.35 between swings, x1.1 damage taken (hits, ticks, numbers).
    rig.HitTo(b, 49);
    rig.Step();
    REQUIRE(rig.M(b).phase == 1);
    CHECK(rig.M(b).phaseSpeedMul == 1.2);
    firstStep(1.2);
    CHECK(rig.M(b).AttackIntervalMs() == doctest::Approx(1300 / 1.35));
    CHECK(rig.M(b).def.speed == affixedSpeed);  // the definition is untouched: the phase multiplies at use
    CHECK(rig.M(b).def.attackSpeedMs == 1300);
    rig.h.events.Clear();
    const double hp1 = rig.M(b).hp;
    rig.Hit(b, 100);
    CHECK(rig.M(b).hp == hp1 - 110);
    std::vector<EvHit> hits = K4Events<EvHit>(rig.h.events);
    REQUIRE(hits.size() == 1);
    CHECK(hits[0].amount == 110);
    std::vector<EvFloatingText> nums = K4Events<EvFloatingText>(rig.h.events);
    REQUIRE(!nums.empty());
    CHECK(nums[0].value == 110);
    rig.Hit(b, 10, DamageType::Fire, /*tick=*/true);
    CHECK(rig.M(b).hp == hp1 - 110 - 11);
    rig.h.events.Clear();
    rig.Step(240);
    starts = SwingStarts(rig.h.events, b);
    REQUIRE(starts.size() >= 4);
    for (size_t i = 1; i < starts.size(); ++i) {
      CHECK(starts[i] - starts[i - 1] >= 1300 / 1.35 - 1e-6);
      CHECK(starts[i] - starts[i - 1] < 1300 / 1.35 + kSimStepMs);
    }
    // The swing's anim is timed on the faster interval.
    for (const EvPlayAnim& a : K4Events<EvPlayAnim>(rig.h.events)) {
      if (a.entity != b || a.clip != "Attack01") continue;
      const ActionTiming t = ComputeAttackTiming(rig.h.ctx.data.Combat().anim, rig.h.ctx.data.Assets(), kArt,
                                                 AnimRig::Humanoid, 1300 / 1.35);
      CHECK(a.contactMs == doctest::Approx(t.contactMs));
      CHECK(a.durationMs == doctest::Approx(t.durationMs));
    }
    // The hit's weight is classified on the damage taken: an amount whose class changes under a x3 row is announced
    // (EvHit.weight) with the class of its tripled value (a pet's hits: the case "a pet's hits ..." below).
    {
      const HitFeedbackTable& table = rig.h.ctx.data.Combat().hitFeedback;
      MonsterPhaseDef soft = K4Row(90);
      soft.damageTakenMul = 3;
      const EntityId t = rig.Spawn(K4Boss({soft}, {}, 1, "fx_k4_soft"), Vec2(60, 60), MonsterState::Idle);
      rig.HitTo(t, 80);
      rig.Step();
      REQUIRE(rig.M(t).phase == 1);
      const double maxHp = rig.M(t).maxHp;
      double amount = 0;
      for (double a = 1; a < 200 && amount == 0; ++a) {
        if (ClassifyHit(table, PhaseScaledDamage(a, 3), maxHp, false, false, false) !=
            ClassifyHit(table, a, maxHp, false, false, false)) {
          amount = a;
        }
      }
      REQUIRE(amount > 0);
      REQUIRE(PhaseScaledDamage(amount, 3) < rig.M(t).hp);
      rig.h.events.Clear();
      CHECK(rig.Hit(t, amount) == ClassifyHit(table, PhaseScaledDamage(amount, 3), maxHp, false, false, false));
      std::vector<EvHit> soft3 = K4Events<EvHit>(rig.h.events);
      REQUIRE(soft3.size() == 1);
      CHECK(soft3[0].amount == PhaseScaledDamage(amount, 3));
      CHECK(soft3[0].weight == ClassifyHit(table, PhaseScaledDamage(amount, 3), maxHp, false, false, false));
    }
    // Each row's multipliers are x the base, not x the previous row's (P2 move x0.6, then P3 x1.15 - not x0.69).
    MonsterPhaseDef slow = K4Row(65);
    slow.speedMul = 0.6;
    slow.attackSpeedMul = 0.5;
    MonsterPhaseDef fast = K4Row(30);
    fast.speedMul = 1.15;
    fast.attackSpeedMul = 1.25;
    const EntityId c = rig.Spawn(K4Boss({slow, fast}), Vec2(50, 50), MonsterState::Idle);
    rig.HitTo(c, 60);
    rig.Step();
    CHECK((rig.M(c).phaseSpeedMul == 0.6 && rig.M(c).AttackIntervalMs() == 2600));
    rig.HitTo(c, 20);
    rig.Step();
    REQUIRE(rig.M(c).phase == 2);
    CHECK((rig.M(c).phaseSpeedMul == 1.15 && rig.M(c).AttackIntervalMs() == 1300 / 1.25 &&
           rig.M(c).phaseDamageTakenMul == 1));
  }

  TEST_CASE("K4 runtime: abilities switch with the phases; one switched on is ready initialCooldownMs after the entry") {
    K4Rig rig;
    MonsterPhaseDef p1 = K4Row(60);
    p1.abilitiesOn = {"b"};
    p1.abilitiesOff = {"a"};
    MonsterPhaseDef p2 = K4Row(30);
    MonsterAbilityDef a = K4Circle("a", 2500);
    MonsterAbilityDef bb = K4Circle("b", 2500, 2000);
    bb.priority = 1;
    MonsterAbilityDef c = K4Circle("c", 2500);
    c.phases = {2};
    c.priority = 2;
    const MonsterDef def = K4Boss({p1, p2}, {a, bb, c});
    rig.PlaceHero(Vec2(41.5, 40));
    const EntityId boss = rig.Spawn(def, Vec2(40, 40), MonsterState::Attack);
    CHECK(rig.M(boss).abilityOff == std::vector<uint8_t>{0, 1, 0});
    const auto casts = [&](const EventSink& sink) {
      std::vector<std::pair<std::string, double>> out;
      for (const EvTelegraph& t : K4Events<EvTelegraph>(sink)) {
        if (t.entity == boss) out.emplace_back(t.abilityId, t.startMs);
      }
      return out;
    };
    rig.Step(360);  // 6 s of phase 0: only `a`
    std::vector<std::pair<std::string, double>> seen = casts(rig.h.events);
    REQUIRE(!seen.empty());
    for (const auto& [id, at] : seen) CHECK(id == "a");
    // Phase 1: `a` off, `b` on, ready 2000 ms after the entry (its fight's clock was already running). The entry step's
    // own swing scan ran before the entry, so the window starts after it.
    rig.HitTo(boss, 59);
    rig.Step();
    const double entry = rig.Now();
    rig.h.events.Clear();
    REQUIRE(rig.M(boss).phase == 1);
    CHECK(rig.M(boss).abilityOff == std::vector<uint8_t>{1, 0, 0});
    rig.Step(480);
    seen = casts(rig.h.events);
    REQUIRE(!seen.empty());
    CHECK(seen[0].first == "b");
    CHECK(seen[0].second >= entry + 2000 - 1e-6);
    for (const auto& [id, at] : seen) CHECK(id == "b");
    // Phase 2: `c` (its own phases[] lists 2) takes the swings by priority; `a` stays off.
    rig.HitTo(boss, 29);
    rig.Step();
    REQUIRE(rig.M(boss).phase == 2);
    rig.h.events.Clear();
    rig.Step(480);
    seen = casts(rig.h.events);
    REQUIRE(!seen.empty());
    bool sawC = false;
    for (const auto& [id, at] : seen) {
      CHECK(id != "a");
      sawC = sawC || id == "c";
    }
    CHECK(sawC);
  }

  TEST_CASE("K4 runtime: a raise / summon switched on by a row casts between swings only from that row's entry") {
    K4Rig rig;
    MonsterAbilityDef call;
    call.id = "call";
    call.kind = AbilityKind::Summon;
    call.clip = "Cast01";
    call.aim = AbilityAim::Self;
    call.cooldownMs = 100000;
    call.summon.present = true;
    call.summon.monsterId = "fx_k4_risen";
    call.summon.count = 2;
    call.summon.capAlive = 4;
    MonsterPhaseDef p1 = K4Row(70);
    p1.abilitiesOn = {"call"};
    const MonsterDef def = K4Boss({p1}, {call});
    rig.PlaceHero(Vec2(44, 40));
    const EntityId b = rig.Spawn(def, Vec2(40, 40), MonsterState::Chase);
    CHECK(rig.M(b).abilityOff == std::vector<uint8_t>{1});
    const auto summonMarks = [&](const EventSink& sink) {
      int n = 0;
      for (const EvTelegraph& t : K4Events<EvTelegraph>(sink)) n += t.entity == b && t.abilityId == "call" ? 1 : 0;
      return n;
    };
    // Phase 0: the fight's clock runs (Chase / Attack near the hero), the cast is switched off.
    rig.Step(240);
    CHECK(rig.M(b).abilityClockStarted);
    CHECK(summonMarks(rig.h.events) == 0);
    CHECK(rig.RaisedOf(b).empty());
    // The row switches it on: its markers and then its two bodies follow the entry (ready initialCooldownMs = 0 after).
    rig.HitTo(b, 69);
    rig.Step();
    REQUIRE(rig.M(b).phase == 1);
    CHECK(rig.M(b).abilityOff == std::vector<uint8_t>{0});
    rig.h.events.Clear();
    REQUIRE(rig.StepUntil([&] { return rig.M(b).pendingAbility == 0; }, 120));
    CHECK(summonMarks(rig.h.events) == 2);
    REQUIRE(rig.StepUntil([&] { return rig.RaisedOf(b).size() == 2; }, 180));
    for (const EvMonsterRaised& r : K4Events<EvMonsterRaised>(rig.h.events)) CHECK(r.abilityId == "call");
  }

  TEST_CASE("K4 runtime: the enter action roots the monster and cancels its windup; every hit lands during it") {
    for (const bool withAction : {true, false}) {
      INFO("enter action " << withAction);
      K4Rig rig;
      MonsterAbilityDef slam = K4Circle("slam", 10000);
      slam.hasWindupMs = true;
      slam.windupMs = 1500;
      MonsterPhaseDef row = withAction ? K4Roar(80) : K4Row(80);
      row.damageTakenMul = 1.0;
      const MonsterDef def = K4Boss({row}, {slam}, 5);
      rig.PlaceHero(Vec2(41.5, 40));
      const EntityId b = rig.Spawn(def, Vec2(40, 40), MonsterState::Attack);
      REQUIRE(rig.StepUntil([&] { return rig.M(b).pendingAbility >= 0; }));
      rig.Step(10);  // mid-windup
      rig.HitTo(b, 79);
      rig.h.events.Clear();
      rig.Step();
      REQUIRE(rig.M(b).phase == 1);
      if (!withAction) {
        // No enter action: the slam winds on and lands.
        CHECK(K4Sequence(rig.h.events, b) == "P");
        CHECK(rig.M(b).pendingAbility >= 0);
        int heroHits = 0;
        for (int i = 0; i < 120; ++i) {
          rig.Step();
          for (const EvHit& h : K4Events<EvHit>(rig.h.events)) heroHits += h.target == kHeroEntityId && h.source == b;
          rig.h.events.Clear();
        }
        CHECK(heroHits >= 1);
        continue;
      }
      // Cancel (the slam's telegraph ends), then the phase, then its Roar for the clip's 1500 ms.
      CHECK(K4Sequence(rig.h.events, b) == "CPR");
      CHECK(rig.M(b).pendingAbility == -1);
      std::vector<EvPlayAnim> roar;
      for (const EvPlayAnim& a : K4Events<EvPlayAnim>(rig.h.events)) {
        if (a.entity == b && a.action == AnimAction::Roar) roar.push_back(a);
      }
      REQUIRE(roar.size() == 1);
      CHECK((roar[0].clip == "Roar" && roar[0].durationMs == kRoarMs && roar[0].startMs == rig.Now()));
      CHECK((roar[0].hasFaceTarget && roar[0].faceTarget == rig.hero->Position()));
      CHECK(rig.M(b).phaseActionUntilMs == rig.Now() + kRoarMs);
      // While it roars: rooted (the hero walks off; it does not follow), no swing, no cast, no hit on the hero - and
      // every hit on it lands (never invulnerable).
      const double until = rig.M(b).phaseActionUntilMs;
      const Vec2 pos = rig.M(b).pos;
      rig.PlaceHero(Vec2(44, 40));
      rig.h.events.Clear();
      int landed = 0;
      while (rig.Now() + kSimStepMs < until) {
        const double hp = rig.M(b).hp;
        rig.Hit(b, 3);
        landed += rig.M(b).hp == hp - 3 ? 1 : 0;
        rig.Step();
      }
      CHECK(landed > 50);
      CHECK(rig.M(b).pos == pos);
      CHECK(SwingStarts(rig.h.events, b).empty());
      CHECK(K4Events<EvTelegraph>(rig.h.events).empty());
      for (const EvHit& h : K4Events<EvHit>(rig.h.events)) CHECK(h.target != kHeroEntityId);
      // After it: it moves and fights again.
      rig.Step(90);
      CHECK(rig.M(b).pos != pos);
    }
    // An action the art has no clip for roots for MonsterAiDef::phaseActionMsDefault.
    K4Rig rig;
    const EntityId t = rig.Spawn(K4Boss({K4Roar(80, AnimAction::Taunt)}), Vec2(40, 40), MonsterState::Idle);
    rig.HitTo(t, 70);
    rig.h.events.Clear();
    rig.Step();
    bool found = false;
    for (const EvPlayAnim& a : K4Events<EvPlayAnim>(rig.h.events)) {
      if (a.entity != t || a.action != AnimAction::Taunt) continue;
      found = true;
      CHECK(a.clip == "Taunt");
      CHECK(a.durationMs == rig.h.ctx.data.Monsters().ai.phaseActionMsDefault);
    }
    CHECK(found);
    CHECK(rig.monsters.PhaseActionMs(rig.M(t).def, AnimAction::Roar) == kRoarMs);
    CHECK(rig.monsters.PhaseActionMs(rig.M(t).def, AnimAction::Taunt) == 1200);
    // A corpse eater with a phase table: the enter action ends its consume channel (cancelled, no heal).
    MonsterDef eater = K4Boss({K4Roar(60)}, {}, 1, "fx_k4_eater");
    eater.corpse.consumes = true;
    rig.PlaceHero(Vec2(80, 60));
    const EntityId e = rig.Spawn(eater, Vec2(60, 60), MonsterState::Idle);
    const EntityId dead = rig.Raised("fx_k4_risen", Vec2(60.5, 60));
    rig.Kill(dead);
    REQUIRE(rig.StepUntil([&] { return rig.M(e).consumeCorpse != 0; }, 30));
    const uint32_t meal = rig.M(e).consumeCorpse;
    rig.HitTo(e, 50);
    rig.h.events.Clear();
    rig.Step();
    REQUIRE(rig.M(e).phase == 1);
    CHECK(rig.M(e).consumeCorpse == 0);
    CHECK(K4Sequence(rig.h.events, e) == "CPR");
    REQUIRE(rig.monsters.CorpseRegistry().Find(meal) != nullptr);
    CHECK(rig.monsters.CorpseRegistry().Find(meal)->holder == kNoEntity);
  }

  TEST_CASE("K4 runtime: a hit through the next threshold during an enter action enters that row at once") {
    K4Rig rig;
    // Row 1 roars (the art's 1500 ms), row 2 taunts (no clip: the 1200 ms default), row 3 roars again.
    const MonsterDef def = K4Boss({K4Roar(80), K4Roar(60, AnimAction::Taunt), K4Roar(40)});
    rig.PlaceHero(Vec2(41.5, 40));
    const EntityId b = rig.Spawn(def, Vec2(40, 40), MonsterState::Attack);
    rig.HitTo(b, 79);
    rig.Step();
    REQUIRE(rig.M(b).phase == 1);
    const double until1 = rig.M(b).phaseActionUntilMs;
    CHECK(until1 == rig.Now() + kRoarMs);
    // 100 ms into the Roar: the next row enters in that step (nothing in flight to cancel), its Taunt plays, and the
    // root keeps the later end (the Roar's: 1500 > 100 + 1200).
    rig.Step(6);
    rig.HitTo(b, 59);
    rig.h.events.Clear();
    rig.Step();
    REQUIRE(rig.M(b).phase == 2);
    CHECK(K4Sequence(rig.h.events, b) == "P");  // no cancel; a Taunt is no Roar ('R')
    bool taunt = false;
    for (const EvPlayAnim& a : K4Events<EvPlayAnim>(rig.h.events)) {
      if (a.entity != b || a.action != AnimAction::Taunt) continue;
      taunt = true;
      CHECK((a.startMs == rig.Now() && a.durationMs == 1200));
    }
    CHECK(taunt);
    CHECK(rig.M(b).phaseActionUntilMs == until1);
    // Late in the Taunt's window a third row roars: the root runs to its later end.
    rig.StepUntilMs(until1 - 200);
    rig.HitTo(b, 39);
    rig.h.events.Clear();
    rig.Step();
    REQUIRE(rig.M(b).phase == 3);
    CHECK(K4Sequence(rig.h.events, b) == "PR");
    CHECK(rig.M(b).phaseActionUntilMs == rig.Now() + kRoarMs);
    // Rooted throughout: no swing until the last action ends.
    const double until3 = rig.M(b).phaseActionUntilMs;
    rig.h.events.Clear();
    rig.StepUntilMs(until3 - kSimStepMs);
    CHECK(SwingStarts(rig.h.events, b).empty());
    rig.Step(30);
    CHECK(!SwingStarts(rig.h.events, b).empty());
  }

  TEST_CASE("K4 runtime: phase summons stand up at the entry (anchors, a ring), as the monster's Raised; one replacement") {
    K4Rig rig;
    MonsterPhaseDef p1 = K4Row(70);
    PhaseSummonDef circles;
    circles.monsterId = "fx_k4_risen";
    circles.count = 2;
    circles.at = AbilitySummonAt::Anchors;
    circles.anchors = {TilePos{45, 40}, TilePos{35, 40}};
    circles.hpPct = 40;
    PhaseSummonDef bearer;
    bearer.monsterId = "fx_k4_bearer";
    bearer.hasRadius = true;
    bearer.radius = 2;
    bearer.delayMs = 500;
    bearer.respawnMs = 20000;
    bearer.respawns = 1;
    p1.summons = {circles, bearer};
    const MonsterDef def = K4Boss({p1, K4Row(20)});
    rig.PlaceHero(Vec2(80, 40));
    const EntityId b = rig.Spawn(def, Vec2(40, 40), MonsterState::Idle);
    rig.HitTo(b, 69);
    rig.h.events.Clear();
    rig.Step();
    std::vector<EvMonsterRaised> raised = K4Events<EvMonsterRaised>(rig.h.events);
    REQUIRE(raised.size() == 2);
    CHECK((raised[0].pos == Vec2(45, 40) && raised[1].pos == Vec2(35, 40)));
    for (const EvMonsterRaised& r : raised) {
      CHECK((r.raiser == b && r.corpseId == 0 && r.abilityId == "phase.1" && r.defId == "fx_k4_risen"));
      const MonsterInstance& m = rig.M(r.entity);
      CHECK((m.role == MonsterRole::Raised && m.raiserId == b && m.noRespawn));
      CHECK(m.hp == std::floor(m.maxHp * 0.4));
    }
    // The bearer stands 500 ms later on the ring of 2 around the boss (slot 0 of 1: +col).
    const double entry = rig.Now();
    rig.h.events.Clear();
    rig.StepUntilMs(entry + 500 - kSimStepMs);
    CHECK(K4Events<EvMonsterRaised>(rig.h.events).empty());
    rig.StepUntilMs(entry + 500 + kSimStepMs);
    raised = K4Events<EvMonsterRaised>(rig.h.events);
    REQUIRE(raised.size() == 1);
    CHECK((raised[0].defId == "fx_k4_bearer" && raised[0].pos == Vec2(42, 40) && raised[0].raiser == b));
    CHECK(rig.RaisedOf(b).size() == 3);
    // Killed while the phase holds: one replacement 20 s later on its spot; killed again: none.
    rig.Kill(raised[0].entity);
    const double killed = rig.Now();
    rig.h.events.Clear();
    rig.StepUntilMs(killed + 20000 - kSimStepMs);
    CHECK(K4Events<EvMonsterRaised>(rig.h.events).empty());
    rig.StepUntilMs(killed + 20000 + kSimStepMs);
    raised = K4Events<EvMonsterRaised>(rig.h.events);
    REQUIRE(raised.size() == 1);
    CHECK((raised[0].defId == "fx_k4_bearer" && raised[0].abilityId == "phase.1"));
    rig.Kill(raised[0].entity);
    rig.h.events.Clear();
    rig.Step(60 * 25);
    CHECK(K4Events<EvMonsterRaised>(rig.h.events).empty());
    // A replacement waits for its phase: killed in phase 1, the boss enters phase 2 before it is due -> none.
    K4Rig rig2;
    rig2.PlaceHero(Vec2(80, 40));
    const EntityId b2 = rig2.Spawn(def, Vec2(40, 40), MonsterState::Idle);
    rig2.HitTo(b2, 69);
    rig2.Step(40);
    std::vector<EntityId> bearers;
    for (EntityId id : rig2.RaisedOf(b2)) {
      if (rig2.M(id).def.id == "fx_k4_bearer") bearers.push_back(id);
    }
    REQUIRE(bearers.size() == 1);
    rig2.Kill(bearers[0]);
    rig2.HitTo(b2, 19);
    rig2.Step();
    REQUIRE(rig2.M(b2).phase == 2);
    rig2.h.events.Clear();
    rig2.Step(60 * 25);
    CHECK(K4Events<EvMonsterRaised>(rig2.h.events).empty());
    // The boss's death: every body it stood up collapses; what was queued never stands.
    K4Rig rig3;
    rig3.PlaceHero(Vec2(80, 40));
    const EntityId b3 = rig3.Spawn(def, Vec2(40, 40), MonsterState::Idle);
    rig3.HitTo(b3, 69);
    rig3.Step();
    REQUIRE(rig3.RaisedOf(b3).size() == 2);
    rig3.h.events.Clear();
    rig3.Kill(b3);
    CHECK(K4Events<EvMonsterCollapsed>(rig3.h.events).size() == 2);
    rig3.Step(60);
    CHECK(K4Events<EvMonsterRaised>(rig3.h.events).empty());
    CHECK(rig3.RaisedOf(b3).empty());
    // One hit through the summoning row and the next one: both rows are entered in that step and the first row's bodies
    // (delay 0) still stand up, though the boss is already past their phase.
    K4Rig rig4;
    rig4.PlaceHero(Vec2(80, 40));
    const EntityId b4 = rig4.Spawn(def, Vec2(40, 40), MonsterState::Idle);
    rig4.HitTo(b4, 19);
    rig4.h.events.Clear();
    rig4.Step();
    REQUIRE(rig4.M(b4).phase == 2);
    raised = K4Events<EvMonsterRaised>(rig4.h.events);
    REQUIRE(raised.size() == 2);
    CHECK((raised[0].pos == Vec2(45, 40) && raised[1].pos == Vec2(35, 40) && raised[0].abilityId == "phase.1"));
    CHECK(rig4.RaisedOf(b4).size() == 2);
    rig4.StepUntilMs(rig4.Now() + 500 + kSimStepMs);  // and the delayed bearer of that row too
    CHECK(rig4.RaisedOf(b4).size() == 3);
  }

  TEST_CASE("K4 runtime: the shrinking-ring arena: radius over time, damage outside only, every 500 ms; out at death") {
    K4Rig rig;
    MonsterPhaseDef p = K4Row(70);
    p.arena.present = true;
    p.arena.fromR = 9;
    p.arena.toR = 5;
    p.arena.overMs = 40000;
    p.arena.dpsPctMaxHp = 4;
    p.arena.damageType = DamageType::Fire;
    MonsterDef def = K4Boss({p});
    def.aggroRange = 30;  // it stays in its fight wherever the hero stands, on its spot (no step toward the leash)
    def.attackRange = 0.5;
    def.speed = 0;
    rig.PlaceHero(Vec2(43, 40));
    const EntityId b = rig.Spawn(def, Vec2(40, 40), MonsterState::Chase);
    rig.h.ctx.equip.Ref(Stat::FireResist) = 25;
    rig.HitTo(b, 69);
    rig.h.events.Clear();
    rig.Step();
    const double lit = rig.Now();
    std::vector<EvArena> arenas = K4Events<EvArena>(rig.h.events);
    REQUIRE(arenas.size() == 1);
    CHECK((arenas[0].entity == b && arenas[0].kind == ArenaKind::RingShrink && arenas[0].centre == Vec2(40, 40)));
    CHECK((arenas[0].radius == 9 && arenas[0].targetRadius == 5 && arenas[0].startMs == lit &&
           arenas[0].endMs == lit + 40000 && arenas[0].tickMs == 500 && arenas[0].dpsPctMaxHp == 4 &&
           arenas[0].damageType == DamageType::Fire && !arenas[0].cleared));
    Snapshot view = rig.View();
    REQUIRE(view.arenas.size() == 1);
    CHECK((view.arenas[0].entity == b && view.arenas[0].radius == 9 && view.arenas[0].fromRadius == 9 &&
           view.arenas[0].targetRadius == 5 && view.arenas[0].endMs == lit + 40000));
    view.ClearDynamic();  // a snapshot reused step after step (GameSim's) shows the ring once
    rig.monsters.FillSnapshot(view);
    CHECK(view.arenas.size() == 1);
    // Inside (3 tiles from the centre): no tick.
    rig.Step(180);
    CHECK(ArenaTicks(rig.h.events, b).empty());
    CHECK(rig.M(b).IsAggro());
    // Outside (12 tiles): a tick every 500 ms of floor(maxHp x 4 % x 0.5 s) less the hero's fire resistance.
    rig.PlaceHero(Vec2(52, 40));
    const double next = rig.M(b).arena.nextTickMs;
    CHECK(std::fmod(next - lit, 500) == doctest::Approx(0).epsilon(1e-6));
    rig.StepUntilMs(next + 1);  // that tick fires; count the next four
    rig.h.events.Clear();
    rig.StepUntilMs(next + 2001);
    std::vector<EvHit> ticks = ArenaTicks(rig.h.events, b);
    REQUIRE(ticks.size() == 4);
    const double maxHp = rig.hero->MaxHp();
    const double resist = ResistancePercent(rig.h.ctx.data.Classes().formulas, &rig.h.ctx.equip, DamageType::Fire);
    CHECK(resist == 25);
    const double expect = std::max(1.0, std::floor(ArenaTickDamage(maxHp, 4, 500, 1) * (1 - resist / 100.0)));
    for (const EvHit& t : ticks) CHECK((t.amount == expect && t.element == DamageType::Fire && t.tick));
    // The radius 20 s in: 7. Ticks read the radius at their own time: the hero 6.9 tiles out is inside around then,
    // 7.2 out it is outside.
    rig.StepUntilMs(lit + 20000 - 600);
    rig.PlaceHero(Vec2(46.9, 40));
    rig.h.events.Clear();
    rig.StepUntilMs(lit + 20000 + 10);
    CHECK(rig.View().arenas[0].radius == doctest::Approx(7).epsilon(1e-3));
    CHECK(ArenaTicks(rig.h.events, b).empty());
    rig.PlaceHero(Vec2(47.2, 40));
    rig.h.events.Clear();
    rig.StepUntilMs(rig.Now() + 501);
    CHECK(ArenaTicks(rig.h.events, b).size() == 1);
    // After overMs the ring stays at toR.
    rig.StepUntilMs(lit + 60000);
    CHECK(rig.View().arenas[0].radius == 5);
    // No tick while the boss is out of its fight (idle, the hero beyond its aggro range).
    rig.M(b).def.aggroRange = 4;
    rig.M(b).state = MonsterState::Idle;
    rig.PlaceHero(Vec2(52, 40));
    rig.h.events.Clear();
    rig.Step(120);
    CHECK(!rig.M(b).IsAggro());
    CHECK(ArenaTicks(rig.h.events, b).empty());
    // Nor while the hero stands in a camp's safe zone (the boss still chasing it from afar).
    const MapCampDef& camp = rig.zone.Map().camps.at(0);
    rig.M(b).def.aggroRange = 300;
    rig.M(b).state = MonsterState::Chase;
    rig.PlaceHero(camp.pos.Center());
    rig.h.events.Clear();
    rig.Step(90);
    CHECK(rig.M(b).IsAggro());
    CHECK(ArenaTicks(rig.h.events, b).empty());
    // Its death puts it out (EvArena{cleared}); no tick after.
    rig.PlaceHero(Vec2(52, 40));
    rig.h.events.Clear();
    rig.Kill(b);
    arenas = K4Events<EvArena>(rig.h.events);
    REQUIRE(arenas.size() == 1);
    CHECK((arenas[0].cleared && arenas[0].entity == b && arenas[0].radius == 5));
    CHECK(rig.View().arenas.empty());
    rig.Step(120);
    CHECK(ArenaTicks(rig.h.events, b).empty());
  }

  TEST_CASE("K4 runtime: arena ticks follow the difficulty and the hit cap; Death Save holds against a lethal tick") {
    MonsterPhaseDef p = K4Row(70);
    p.arena.present = true;
    p.arena.fromR = 5;
    p.arena.toR = 5;
    p.arena.overMs = 1000;
    p.arena.dpsPctMaxHp = 4;
    p.arena.damageType = DamageType::Physical;
    const auto firstTick = [&](Difficulty diff, double capPct, double dps) {
      K4Rig rig(7, diff);
      MonsterPhaseDef row = p;
      row.arena.dpsPctMaxHp = dps;
      MonsterDef def = K4Boss({row});
      def.aggroRange = 30;
      def.attackRange = 0.5;
      def.speed = 0;
      def.hitCapHeroMaxHpPct = capPct;
      rig.PlaceHero(Vec2(50, 40));
      const EntityId b = rig.Spawn(def, Vec2(40, 40), MonsterState::Chase);
      rig.HitTo(b, 60);
      rig.Step();
      rig.h.events.Clear();
      rig.Step(31);
      const std::vector<EvHit> ticks = ArenaTicks(rig.h.events, b);
      REQUIRE(ticks.size() == 1);
      return std::make_pair(ticks[0].amount, rig.hero->MaxHp());
    };
    const DifficultyTable& d = K4Data().Combat().difficulty;
    for (Difficulty diff : {Difficulty::Normal, Difficulty::Nightmare, Difficulty::Hell}) {
      INFO("difficulty " << static_cast<int>(diff));
      const auto [amount, maxHp] = firstTick(diff, 0, 4);
      CHECK(amount == std::max(1.0, std::floor(maxHp * 4 / 100.0 * 500 / 1000.0 * d.Def(diff).damageMul)));
    }
    // The monster's hit cap binds a big tick (1 % of max life on Normal).
    const auto [capped, maxHp] = firstTick(Difficulty::Normal, 1, 100);
    CHECK(capped == std::floor(maxHp * 1 / 100.0));
    // A tick the hero's resistance would round to nothing still hurts 1; none lands during a zone transition.
    {
      K4Rig rig;
      MonsterPhaseDef row = p;
      row.arena.dpsPctMaxHp = 0.01;
      row.arena.damageType = DamageType::Fire;
      MonsterDef def = K4Boss({row});
      def.aggroRange = 30;
      def.attackRange = 0.5;
      def.speed = 0;
      rig.h.ctx.equip.Ref(Stat::FireResist) = 1000;
      REQUIRE(ResistancePercent(rig.h.ctx.data.Classes().formulas, &rig.h.ctx.equip, DamageType::Fire) >= 50);
      rig.PlaceHero(Vec2(50, 40));
      const EntityId b = rig.Spawn(def, Vec2(40, 40), MonsterState::Chase);
      rig.HitTo(b, 60);
      rig.Step();
      rig.h.events.Clear();
      rig.Step(31);
      std::vector<EvHit> ticks = ArenaTicks(rig.h.events, b);
      REQUIRE(ticks.size() == 1);
      CHECK(ticks[0].amount == 1);
      rig.h.session.transitioning = true;
      rig.h.events.Clear();
      rig.Step(90);
      CHECK(rig.M(b).IsAggro());
      CHECK(ArenaTicks(rig.h.events, b).empty());
      rig.h.session.transitioning = false;
      rig.Step(31);
      CHECK(ArenaTicks(rig.h.events, b).size() == 1);
    }
    // Death Save: a lethal tick leaves the hero alive with it, dying without it.
    for (bool save : {true, false}) {
      INFO("death save " << save);
      K4Rig rig;
      rig.keepHeroUp = false;
      if (save) rig.h.ctx.equip.Ref(Stat::DeathSave) = 1;
      MonsterDef def = K4Boss({p});
      def.aggroRange = 30;
      def.attackRange = 0.5;
      def.speed = 0;
      rig.PlaceHero(Vec2(50, 40));
      const EntityId b = rig.Spawn(def, Vec2(40, 40), MonsterState::Chase);
      rig.HitTo(b, 60);
      rig.Step();
      rig.hero->SetHp(1);
      rig.Step(31);
      CHECK(rig.hero->Life() == (save ? HeroLife::Alive : HeroLife::Dying));
    }
  }

  TEST_CASE("K4 runtime: a later row's arena replaces the earlier one, a row without one keeps it; a collapse puts it out") {
    K4Rig rig;
    MonsterPhaseDef a1 = K4Row(80);
    a1.arena.present = true;
    a1.arena.fromR = 9;
    a1.arena.toR = 5;
    a1.arena.overMs = 40000;
    a1.arena.dpsPctMaxHp = 1;
    MonsterPhaseDef a2 = K4Row(60);
    a2.arena.present = true;
    a2.arena.hasCentre = true;
    a2.arena.centre = TilePos{50, 40};
    a2.arena.fromR = 6;
    a2.arena.toR = 6;
    a2.arena.overMs = 1000;
    a2.arena.dpsPctMaxHp = 2;
    a2.arena.damageType = DamageType::Fire;
    MonsterDef def = K4Boss({a1, a2, K4Row(40)});
    def.aggroRange = 30;
    def.attackRange = 0.5;
    def.speed = 0;
    rig.PlaceHero(Vec2(42, 40));  // inside the first ring (2 of 9), outside the second (8 of 6)
    const EntityId b = rig.Spawn(def, Vec2(40, 40), MonsterState::Chase);
    rig.HitTo(b, 79);
    rig.h.events.Clear();
    rig.Step();
    std::vector<EvArena> ev = K4Events<EvArena>(rig.h.events);
    REQUIRE(ev.size() == 1);
    CHECK((!ev[0].cleared && ev[0].centre == Vec2(40, 40) && ev[0].radius == 9));
    rig.Step(90);
    CHECK(ArenaTicks(rig.h.events, b).empty());
    // Row 2 lights its own ring: one new EvArena for the same monster (no `cleared` in between), the view shows it alone,
    // and its ticks (centre (50, 40), r 6, 2 %/s fire) reach the hero the first ring spared.
    rig.HitTo(b, 59);
    rig.h.events.Clear();
    rig.Step();
    const double lit2 = rig.Now();
    ev = K4Events<EvArena>(rig.h.events);
    REQUIRE(ev.size() == 1);
    CHECK((!ev[0].cleared && ev[0].entity == b && ev[0].centre == Vec2(50, 40) && ev[0].radius == 6 &&
           ev[0].targetRadius == 6 && ev[0].dpsPctMaxHp == 2 && ev[0].damageType == DamageType::Fire &&
           ev[0].startMs == lit2));
    Snapshot view = rig.View();
    REQUIRE(view.arenas.size() == 1);
    CHECK((view.arenas[0].centre == Vec2(50, 40) && view.arenas[0].radius == 6));
    CHECK(rig.M(b).arena.phase == 2);
    rig.h.events.Clear();
    rig.StepUntilMs(lit2 + 1001);
    std::vector<EvHit> ticks = ArenaTicks(rig.h.events, b);
    REQUIRE(ticks.size() == 2);
    for (const EvHit& t : ticks) {
      CHECK((t.element == DamageType::Fire && t.amount == ArenaTickDamage(rig.hero->MaxHp(), 2, 500, 1)));
    }
    // Row 3 has no arena: the second ring stays (no EvArena).
    rig.HitTo(b, 39);
    rig.h.events.Clear();
    rig.Step();
    REQUIRE(rig.M(b).phase == 3);
    CHECK(K4Events<EvArena>(rig.h.events).empty());
    view = rig.View();
    REQUIRE(view.arenas.size() == 1);
    CHECK(view.arenas[0].centre == Vec2(50, 40));
    // A Raised monster with a phase table: its raiser's death collapses it, and the collapse puts its arena out.
    const EntityId raiser = rig.Spawn(test::fixture::Grunt(), Vec2(70, 70), MonsterState::Idle);
    const EntityId risen = rig.monsters.SummonAt(def, Vec2(30, 40), raiser, 100, "test");
    REQUIRE(risen != kNoEntity);
    rig.HitTo(risen, 79);
    rig.Step();
    REQUIRE(rig.M(risen).phase == 1);
    REQUIRE(rig.View().arenas.size() == 2);
    rig.h.events.Clear();
    rig.Kill(raiser);
    REQUIRE(K4Events<EvMonsterCollapsed>(rig.h.events).size() == 1);
    ev = K4Events<EvArena>(rig.h.events);
    REQUIRE(ev.size() == 1);
    CHECK((ev[0].cleared && ev[0].entity == risen));
    view = rig.View();
    REQUIRE(view.arenas.size() == 1);
    CHECK(view.arenas[0].entity == b);
  }

  TEST_CASE("K4 runtime: the monster walks home (M1): full life, phase 0, its arena out, its fight's bodies collapse") {
    K4Rig rig;
    MonsterPhaseDef p = K4Roar(90);
    p.speedMul = 1.5;
    p.attackSpeedMul = 2;
    p.damageTakenMul = 1.5;
    p.abilitiesOn = {"late"};
    p.arena.present = true;
    p.arena.fromR = 9;
    p.arena.toR = 5;
    p.arena.overMs = 40000;
    p.arena.dpsPctMaxHp = 1;
    PhaseSummonDef now;
    now.monsterId = "fx_k4_risen";
    PhaseSummonDef later = now;
    later.delayMs = 15000;
    p.summons = {now, later};
    p.feed.present = true;
    p.feed.radius = 4;
    p.feed.healPct = 3;
    p.feed.delayMs = 10000;
    const MonsterDef def = K4Boss({p}, {K4Circle("late", 60000)});
    rig.PlaceHero(Vec2(42, 40));
    const EntityId b = rig.Spawn(def, Vec2(40, 40), MonsterState::Chase);
    CHECK(rig.M(b).abilityOff == std::vector<uint8_t>{1});
    rig.HitTo(b, 85);
    rig.Step();
    REQUIRE(rig.M(b).phase == 1);
    CHECK(rig.M(b).abilityOff == std::vector<uint8_t>{0});
    CHECK(rig.RaisedOf(b).size() == 1);
    // A Raised killed by the dais starts a Feed (held corpse).
    const EntityId fed = rig.Raised("fx_k4_risen", Vec2(41, 41));
    rig.Kill(fed);
    REQUIRE(K4Events<EvMonsterFeed>(rig.h.events).size() == 1);
    const uint32_t corpse = K4Events<EvMonsterFeed>(rig.h.events)[0].corpseId;
    REQUIRE(rig.monsters.CorpseRegistry().Find(corpse) != nullptr);
    CHECK(rig.monsters.CorpseRegistry().Find(corpse)->holder == b);
    // Drag it past its leash (8 tiles from its anchor): it walks home ignoring the hero.
    rig.Step(100);  // let the roar end
    rig.M(b).pos = Vec2(49.5, 40);
    rig.M(b).state = MonsterState::Chase;
    rig.PlaceHero(Vec2(50.5, 40));
    REQUIRE(rig.StepUntil([&] { return rig.M(b).state == MonsterState::Returning; }, 60));
    CHECK(rig.M(b).phase == 1);  // walking home is no reset yet
    REQUIRE(rig.StepUntil([&] { return Dist(rig.M(b).pos, Vec2(40, 40)) < 1.4; }, 600));
    // M1: it walks home at its normal speed, not its row's x1.5 (the smoothed speed has settled on the base top speed).
    CHECK(rig.M(b).phaseSpeedMul == 1.5);
    CHECK(rig.M(b).moveSpeed ==
          doctest::Approx(rig.M(b).def.speed * rig.h.ctx.data.Monsters().ai.moveSpeedScale).epsilon(0.01));
    rig.M(b).hp = std::floor(rig.M(b).maxHp * 0.2);  // so the full life comes from the reset, not the walk's heal
    rig.h.events.Clear();
    REQUIRE(rig.StepUntil([&] { return rig.M(b).state != MonsterState::Returning; }, 120));
    CHECK(rig.M(b).hp == rig.M(b).maxHp);
    CHECK(rig.M(b).phase == 0);
    CHECK((rig.M(b).phaseSpeedMul == 1 && rig.M(b).phaseAttackSpeedMul == 1 && rig.M(b).phaseDamageTakenMul == 1));
    CHECK(rig.M(b).AttackIntervalMs() == 1300);
    CHECK(rig.M(b).abilityOff == std::vector<uint8_t>{1});
    CHECK(rig.M(b).phaseActionUntilMs == 0);
    std::vector<EvMonsterPhase> ph = K4Events<EvMonsterPhase>(rig.h.events);
    REQUIRE(ph.size() == 1);
    CHECK((ph[0].entity == b && ph[0].phase == 0 && ph[0].reset && ph[0].phaseCount == 1 && ph[0].stinger.empty()));
    std::vector<EvArena> ar = K4Events<EvArena>(rig.h.events);
    REQUIRE(ar.size() == 1);
    CHECK(ar[0].cleared);
    CHECK(rig.View().arenas.empty());
    CHECK(K4Events<EvMonsterCollapsed>(rig.h.events).size() == 1);
    CHECK(rig.RaisedOf(b).empty());
    REQUIRE(rig.monsters.CorpseRegistry().Find(corpse) != nullptr);
    CHECK(rig.monsters.CorpseRegistry().Find(corpse)->holder == kNoEntity);  // the Feed let go
    // The queued summon and the Feed never come due.
    REQUIRE(rig.Now() < 9000);
    const double hpAfter = rig.M(b).hp;
    rig.PlaceHero(Vec2(80, 40));
    rig.h.events.Clear();
    rig.Step(60 * 16);
    CHECK(K4Events<EvMonsterRaised>(rig.h.events).empty());
    for (const EvCorpseRemoved& r : K4Events<EvCorpseRemoved>(rig.h.events)) CHECK(r.reason != CorpseRemoveReason::Consumed);
    CHECK(rig.M(b).hp == hpAfter);
    // A new fight enters the phase again.
    rig.HitTo(b, 85);
    rig.h.events.Clear();
    rig.Step();
    REQUIRE(K4Events<EvMonsterPhase>(rig.h.events).size() == 1);
    CHECK(K4Events<EvMonsterPhase>(rig.h.events)[0].phase == 1);
    // A monster without a phase table walks home as before (M1's heal over time only).
    const EntityId plain = rig.Spawn(K4Boss({}), Vec2(60, 60), MonsterState::Chase);
    rig.M(plain).pos = Vec2(69.5, 60);
    rig.PlaceHero(Vec2(70.5, 60));
    REQUIRE(rig.StepUntil([&] { return rig.M(plain).state == MonsterState::Returning; }, 60));
    REQUIRE(rig.StepUntil([&] { return Dist(rig.M(plain).pos, Vec2(60, 60)) < 1.4; }, 600));
    rig.M(plain).hp = std::floor(rig.M(plain).maxHp * 0.2);
    rig.h.events.Clear();
    REQUIRE(rig.StepUntil([&] { return rig.M(plain).state != MonsterState::Returning; }, 120));
    CHECK(rig.M(plain).hp < rig.M(plain).maxHp * 0.5);
    for (const EvMonsterPhase& e : K4Events<EvMonsterPhase>(rig.h.events)) CHECK(e.entity != plain);
  }

  TEST_CASE("K4 runtime: a phase table's monster home before its first row heals to full and announces nothing") {
    K4Rig rig;
    const EntityId b = rig.Spawn(K4Boss({K4Row(50)}), Vec2(40, 40), MonsterState::Chase);
    rig.HitTo(b, 70);  // above the only threshold
    rig.Step();
    REQUIRE(rig.M(b).phase == 0);
    rig.M(b).pos = Vec2(49.5, 40);
    rig.PlaceHero(Vec2(50.5, 40));
    REQUIRE(rig.StepUntil([&] { return rig.M(b).state == MonsterState::Returning; }, 60));
    REQUIRE(rig.StepUntil([&] { return Dist(rig.M(b).pos, Vec2(40, 40)) < 1.4; }, 600));
    rig.M(b).hp = std::floor(rig.M(b).maxHp * 0.6);
    rig.h.events.Clear();
    REQUIRE(rig.StepUntil([&] { return rig.M(b).state != MonsterState::Returning; }, 120));
    CHECK(rig.M(b).hp == rig.M(b).maxHp);
    CHECK(rig.M(b).phase == 0);
    CHECK(K4Events<EvMonsterPhase>(rig.h.events).empty());
    CHECK(K4Events<EvArena>(rig.h.events).empty());
    CHECK(K4Events<EvMonsterCollapsed>(rig.h.events).empty());
  }

  TEST_CASE("K4 runtime: the tether holds the chase inside its circle, walks it back in, never resets; a later row frees it") {
    K4Rig rig;
    MonsterPhaseDef hold = K4Row(90);
    hold.leash.present = true;
    hold.leash.radius = 3;
    const MonsterDef def = K4Boss({hold, K4Row(50)});
    rig.PlaceHero(Vec2(47, 40));
    const EntityId b = rig.Spawn(def, Vec2(40, 40), MonsterState::Chase);
    rig.HitTo(b, 89);
    rig.Step();
    REQUIRE(rig.M(b).phase == 1);
    CHECK((rig.M(b).hasTether && rig.M(b).tetherCentre == Vec2(40, 40) && rig.M(b).tetherRadius == 3));
    double far = 0;
    for (int i = 0; i < 240; ++i) {
      rig.Step();
      far = std::max(far, Dist(rig.M(b).pos, Vec2(40, 40)));
      CHECK(rig.M(b).state != MonsterState::Returning);
    }
    CHECK(far <= 3.25);
    CHECK(Dist(rig.M(b).pos, Vec2(43, 40)) < 0.3);  // waiting at the edge nearest the hero
    CHECK(rig.M(b).state == MonsterState::Chase);
    // The hero walks around it: it follows along the circle.
    rig.PlaceHero(Vec2(40, 47));
    rig.Step(240);
    CHECK(Dist(rig.M(b).pos, Vec2(40, 43)) < 0.3);
    CHECK(rig.M(b).phase == 1);
    // A hero inside the circle is fought as usual.
    rig.PlaceHero(Vec2(41, 41));
    REQUIRE(rig.StepUntil([&] { return rig.M(b).state == MonsterState::Attack; }, 240));
    // Standing outside its circle when the row starts (it chased the hero off in phase 0), it walks back in before it
    // attacks, although the hero stands within its reach.
    K4Rig rig2;
    rig2.PlaceHero(Vec2(47, 40));
    const EntityId c = rig2.Spawn(def, Vec2(40, 40), MonsterState::Attack);
    rig2.M(c).pos = Vec2(45.5, 40);
    rig2.HitTo(c, 89);
    rig2.h.events.Clear();
    rig2.Step(180);
    CHECK(Dist(rig2.M(c).pos, Vec2(40, 40)) <= 3.25);
    // A separation push of under 0.25 tiles past the edge (the slack) does not stop its attack.
    K4Rig rig3;
    rig3.PlaceHero(Vec2(44.5, 40));
    const EntityId e = rig3.Spawn(def, Vec2(40, 40), MonsterState::Idle);
    rig3.HitTo(e, 89);
    rig3.Step(30);  // in its fight and in the AI's activity set
    REQUIRE(rig3.M(e).hasTether);
    rig3.M(e).pos = rig3.M(e).prevPos = Vec2(43.2, 40);
    rig3.M(e).state = MonsterState::Chase;
    rig3.Step();
    CHECK(rig3.M(e).state == MonsterState::Attack);
    rig3.M(e).pos = rig3.M(e).prevPos = Vec2(43.3, 40);  // past it: back in first
    rig3.M(e).state = MonsterState::Chase;
    rig3.Step();
    CHECK(rig3.M(e).state == MonsterState::Chase);
    // A row that changes the tether drops the path the monster was following (it re-plans toward the new goal).
    K4Rig rig4;
    rig4.PlaceHero(Vec2(80, 40));
    const EntityId f = rig4.Spawn(def, Vec2(40, 40), MonsterState::Idle);
    rig4.M(f).path = {TilePos{45, 40}, TilePos{46, 40}};
    rig4.HitTo(f, 89);
    rig4.Step();
    REQUIRE(rig4.M(f).phase == 1);
    CHECK(rig4.M(f).path.empty());
    // The next row has no leash: the tether ends and it chases freely.
    rig.PlaceHero(Vec2(47, 40));
    rig.HitTo(b, 49);
    rig.Step();
    REQUIRE(rig.M(b).phase == 2);
    CHECK(!rig.M(b).hasTether);
    REQUIRE(rig.StepUntil([&] { return Dist(rig.M(b).pos, Vec2(40, 40)) > 4; }, 300));
  }

  TEST_CASE("K4 runtime: the tether holds a patrol goal inside its circle too (out of the fight)") {
    K4Rig rig;
    MonsterPhaseDef hold = K4Row(90);
    hold.leash.present = true;
    hold.leash.hasCentre = true;
    hold.leash.centre = TilePos{40, 40};
    hold.leash.radius = 2;
    rig.PlaceHero(Vec2(54, 40));  // in the AI's activity radius (30), beyond its aggro range (8)
    const EntityId b = rig.Spawn(K4Boss({hold}), Vec2(40, 40), MonsterState::Idle);
    rig.HitTo(b, 80);  // no provoke: it stays out of the fight (the hero is far beyond its aggro range)
    rig.Step();
    REQUIRE(rig.M(b).phase == 1);
    REQUIRE(rig.M(b).state == MonsterState::Idle);
    MonsterInstance& m = rig.M(b);
    m.state = MonsterState::Patrol;
    m.hasPatrolTarget = true;
    m.patrolTarget = TilePos{40, 46};
    m.patrolStartedMs = rig.Now();
    double far = 0;
    bool arrived = false;
    for (int i = 0; i < 200 && !arrived; ++i) {
      rig.Step();
      far = std::max(far, Dist(rig.M(b).pos, Vec2(40, 40)));
      arrived = rig.M(b).state == MonsterState::Idle;
    }
    CHECK(arrived);  // it reached the tethered goal (the circle's point nearest the target), not a timeout
    CHECK(rig.Now() < rig.M(b).patrolStartedMs + rig.h.ctx.data.Monsters().ai.patrolTimeoutMs);
    CHECK(far <= 2 + 1e-9);
    CHECK(Dist(rig.M(b).pos, Vec2(40, 42)) < rig.h.ctx.data.Monsters().ai.arriveEpsilon + 1e-9);
  }

  TEST_CASE("K4 runtime: Feed draws a Raised killed near the centre in after its delay; fire, range or phase 0 stop it") {
    MonsterPhaseDef p = K4Row(90);
    p.feed.present = true;
    p.feed.radius = 4;
    p.feed.healPct = 3;
    p.feed.delayMs = 1000;
    const MonsterDef def = K4Boss({p});
    const auto setup = [&](K4Rig& rig) {
      rig.PlaceHero(Vec2(80, 40));
      const EntityId b = rig.Spawn(def, Vec2(40, 40), MonsterState::Idle);
      rig.HitTo(b, 80);
      rig.Step();
      REQUIRE(rig.M(b).phase == 1);
      return b;
    };
    {
      K4Rig rig;
      const EntityId b = setup(rig);
      const double hp = rig.M(b).hp;
      const EntityId r = rig.Raised("fx_k4_risen", Vec2(42, 40));
      rig.h.events.Clear();
      rig.Kill(r);
      std::vector<EvMonsterFeed> feeds = K4Events<EvMonsterFeed>(rig.h.events);
      REQUIRE(feeds.size() == 1);
      CHECK((feeds[0].entity == b && feeds[0].fed == r && feeds[0].pos == Vec2(42, 40) && feeds[0].startMs == rig.Now() &&
             feeds[0].dueMs == rig.Now() + 1000));
      const uint32_t corpse = feeds[0].corpseId;
      REQUIRE(rig.monsters.CorpseRegistry().Find(corpse) != nullptr);
      CHECK(rig.monsters.CorpseRegistry().Find(corpse)->holder == b);
      const double due = feeds[0].dueMs;
      rig.h.events.Clear();
      rig.StepUntilMs(due - kSimStepMs);
      CHECK(K4Events<EvCorpseRemoved>(rig.h.events).empty());
      CHECK(rig.M(b).hp == hp);
      rig.StepUntilMs(due + kSimStepMs);
      std::vector<EvCorpseRemoved> gone = K4Events<EvCorpseRemoved>(rig.h.events);
      REQUIRE(gone.size() == 1);
      CHECK((gone[0].corpseId == corpse && gone[0].reason == CorpseRemoveReason::Consumed && gone[0].by == b));
      CHECK(rig.M(b).hp == hp + std::floor(rig.M(b).maxHp * 0.03));
      // Past the radius (5 tiles), a fire killing blow, or a regular (not Raised) body: no Feed.
      const EntityId farAway = rig.Raised("fx_k4_risen", Vec2(45, 40));
      rig.h.events.Clear();
      rig.Kill(farAway);
      CHECK(K4Events<EvMonsterFeed>(rig.h.events).empty());
      const EntityId burnt = rig.Raised("fx_k4_risen", Vec2(41, 40));
      rig.Kill(burnt, DamageType::Fire);
      CHECK(K4Events<EvMonsterFeed>(rig.h.events).empty());
      const MonsterDef* risen = K4Data().FindMonster("fx_k4_risen");
      REQUIRE(risen != nullptr);
      const EntityId regular = rig.Spawn(*risen, Vec2(41, 41), MonsterState::Idle, MonsterRole::DefendWave);
      rig.Kill(regular);
      CHECK(K4Events<EvMonsterFeed>(rig.h.events).empty());
    }
    {
      // Fire over the held corpse during the delay: no heal.
      K4Rig rig;
      const EntityId b = setup(rig);
      const double hp = rig.M(b).hp;
      const EntityId r = rig.Raised("fx_k4_risen", Vec2(42, 40));
      rig.Kill(r);
      REQUIRE(K4Events<EvMonsterFeed>(rig.h.events).size() == 1);
      AbilityShapeDef fire;
      fire.kind = AbilityShapeKind::Circle;
      fire.radius = 1;
      CHECK(rig.monsters.BurnCorpses(fire, Vec2(42, 40), Vec2(1, 0)) == 1);
      rig.h.events.Clear();
      rig.Step(90);
      for (const EvCorpseRemoved& e : K4Events<EvCorpseRemoved>(rig.h.events)) CHECK(e.reason != CorpseRemoveReason::Consumed);
      CHECK(rig.M(b).hp == hp);
    }
    {
      // The feeder dies during the delay: the corpse is let go, nothing is consumed.
      K4Rig rig;
      const EntityId b = setup(rig);
      const EntityId r = rig.Raised("fx_k4_risen", Vec2(42, 40));
      rig.Kill(r);
      const uint32_t corpse = K4Events<EvMonsterFeed>(rig.h.events).at(0).corpseId;
      rig.Kill(b);
      REQUIRE(rig.monsters.CorpseRegistry().Find(corpse) != nullptr);
      CHECK(rig.monsters.CorpseRegistry().Find(corpse)->holder == kNoEntity);
      rig.h.events.Clear();
      rig.Step(90);
      for (const EvCorpseRemoved& e : K4Events<EvCorpseRemoved>(rig.h.events)) CHECK(e.reason != CorpseRemoveReason::Consumed);
    }
    {
      // Phase 0 has no Feed; an authored centre replaces the spawn anchor.
      K4Rig rig;
      rig.PlaceHero(Vec2(80, 40));
      const EntityId b = rig.Spawn(def, Vec2(40, 40), MonsterState::Idle);
      const EntityId r = rig.Raised("fx_k4_risen", Vec2(41, 40));
      rig.Kill(r);
      CHECK(K4Events<EvMonsterFeed>(rig.h.events).empty());
      MonsterPhaseDef there = p;
      there.feed.hasCentre = true;
      there.feed.centre = TilePos{60, 60};
      const EntityId c = rig.Spawn(K4Boss({there}, {}, 1, "fx_k4_far"), Vec2(30, 30), MonsterState::Idle);
      rig.HitTo(c, 80);
      rig.Step();
      const EntityId near = rig.Raised("fx_k4_risen", Vec2(61, 60));
      rig.h.events.Clear();
      rig.Kill(near);
      std::vector<EvMonsterFeed> feeds = K4Events<EvMonsterFeed>(rig.h.events);
      REQUIRE(feeds.size() == 1);
      CHECK(feeds[0].entity == c);
      (void)b;
    }
  }

  TEST_CASE("K4 runtime: the Red-Oath frenzy as a one-row table (attack speed x1.35, move x1.2, damage taken x1.1, Roar)") {
    K4Rig rig;
    rig.PlaceHero(Vec2(41.2, 40));
    const EntityId z = rig.Spawn(K4Zealot(), Vec2(40, 40), MonsterState::Attack);
    CHECK(rig.M(z).AttackIntervalMs() == 1100);
    rig.HitTo(z, 50);  // 45 of 90: not below half its life
    rig.h.events.Clear();
    rig.Step();
    CHECK(rig.M(z).phase == 0);
    CHECK(K4Events<EvMonsterPhase>(rig.h.events).empty());
    rig.Hit(z, 1);
    rig.h.events.Clear();
    rig.Step();
    REQUIRE(rig.M(z).phase == 1);
    std::vector<EvMonsterPhase> ev = K4Events<EvMonsterPhase>(rig.h.events);
    REQUIRE(ev.size() == 1);
    CHECK((ev[0].phase == 1 && ev[0].phaseCount == 1 && ev[0].hpBelowPct == 50));
    bool roared = false;
    for (const EvPlayAnim& a : K4Events<EvPlayAnim>(rig.h.events)) roared = roared || (a.entity == z && a.action == AnimAction::Roar);
    CHECK(roared);
    CHECK(rig.M(z).AttackIntervalMs() == doctest::Approx(1100 / 1.35));
    CHECK(rig.M(z).phaseSpeedMul == 1.2);
    const double hp = rig.M(z).hp;
    rig.Hit(z, 10);
    CHECK(rig.M(z).hp == hp - 11);
  }

  TEST_CASE("K4 runtime: a zone exit drops the arenas, the queued summons and the Feeds silently") {
    K4Rig rig;
    MonsterPhaseDef p = K4Row(90);
    p.arena.present = true;
    p.arena.fromR = 9;
    p.arena.toR = 5;
    p.arena.overMs = 40000;
    p.arena.dpsPctMaxHp = 4;
    PhaseSummonDef later;
    later.monsterId = "fx_k4_risen";
    later.delayMs = 2000;
    p.summons = {later};
    p.feed.present = true;
    p.feed.radius = 4;
    p.feed.healPct = 3;
    p.feed.delayMs = 2000;
    rig.PlaceHero(Vec2(80, 40));
    const EntityId b = rig.Spawn(K4Boss({p}), Vec2(40, 40), MonsterState::Idle);
    rig.HitTo(b, 80);
    rig.Step();
    rig.Kill(rig.Raised("fx_k4_risen", Vec2(41, 40)));
    REQUIRE(K4Events<EvMonsterFeed>(rig.h.events).size() == 1);
    REQUIRE(rig.View().arenas.size() == 1);
    rig.h.events.Clear();
    rig.combat.OnZoneExit();
    rig.monsters.OnZoneExit();
    CHECK(K4Events<EvArena>(rig.h.events).empty());
    CHECK(rig.View().arenas.empty());
    rig.h.events.Clear();
    rig.Step(60 * 3);
    CHECK(K4Events<EvMonsterRaised>(rig.h.events).empty());
    CHECK(K4Events<EvCorpseRemoved>(rig.h.events).empty());
  }

  // =================================================================================================================
  // Review follow-ups (2C): the dais, Feed's hold, swings drawn to the beast, a pet's hits, cooldowns across switches
  // =================================================================================================================
  TEST_CASE("K4 runtime: the dais is the authored spawn tile: default centres and the anchor aim; a respawn keeps it") {
    K4Rig rig;
    MonsterAbilityDef seep = K4Circle("seep", 60000);
    seep.aim = AbilityAim::Anchor;
    MonsterPhaseDef p = K4Row(90);
    p.leash.present = true;
    p.leash.radius = 3;
    p.arena.present = true;
    p.arena.fromR = 9;
    p.arena.toR = 5;
    p.arena.overMs = 40000;
    p.arena.dpsPctMaxHp = 1;
    p.feed.present = true;
    p.feed.radius = 1;
    p.feed.healPct = 3;
    p.feed.delayMs = 500;
    const MonsterDef def = K4Boss({p}, {seep});
    MonsterSpawnParams sp;
    sp.baseDef = &def;
    sp.tile = TilePos{42, 41};  // where M10's placement jitter put it
    sp.hasHome = true;
    sp.home = TilePos{40, 40};  // the map's tile: the dais
    const EntityId b = rig.monsters.Spawn(sp);
    REQUIRE(b != kNoEntity);
    CHECK((rig.M(b).spawnAnchor == TilePos{42, 41} && rig.M(b).homeTile == TilePos{40, 40}));
    rig.M(b).stats.dex = 0;
    rig.M(b).stats.lck = 0;
    rig.M(b).state = MonsterState::Attack;
    rig.PlaceHero(Vec2(43.5, 41));
    // K2's anchor aim: the circle is centred on the dais, not on the jittered tile.
    const auto seeps = [&]() {
      std::vector<EvTelegraph> out;
      for (const EvTelegraph& t : K4Events<EvTelegraph>(rig.h.events)) {
        if (t.entity == b && t.abilityId == "seep") out.push_back(t);
      }
      return out;
    };
    REQUIRE(rig.StepUntil([&] { return !seeps().empty(); }, 120));
    CHECK(seeps()[0].origin == Vec2(40, 40));
    // The row's tether, arena and Feed centres default to it too.
    rig.HitTo(b, 80);
    rig.h.events.Clear();
    rig.Step();
    REQUIRE(rig.M(b).phase == 1);
    CHECK((rig.M(b).hasTether && rig.M(b).tetherCentre == Vec2(40, 40)));
    std::vector<EvArena> arenas = K4Events<EvArena>(rig.h.events);
    REQUIRE(arenas.size() == 1);
    CHECK(arenas[0].centre == Vec2(40, 40));
    const EntityId offDais = rig.Raised("fx_k4_risen", Vec2(42, 41));  // on the spawn tile, 2.2 tiles off the dais
    rig.h.events.Clear();
    rig.Kill(offDais);
    CHECK(K4Events<EvMonsterFeed>(rig.h.events).empty());
    const EntityId onDais = rig.Raised("fx_k4_risen", Vec2(40.5, 40.5));
    rig.Kill(onDais);
    CHECK(K4Events<EvMonsterFeed>(rig.h.events).size() == 1);
    // A respawn keeps both (M3: around the same anchor; the same dais).
    const MonsterDef plainDef = K4Boss({}, {}, 1, "fx_k4_home");
    MonsterSpawnParams pp;
    pp.baseDef = &plainDef;
    pp.tile = TilePos{60, 61};
    pp.hasHome = true;
    pp.home = TilePos{60, 60};
    const EntityId g = rig.monsters.Spawn(pp);
    REQUIRE(g != kNoEntity);
    rig.Kill(g);
    rig.StepUntilMs(rig.Now() + rig.h.ctx.data.Monsters().ai.respawnDelayMs + 100);
    const MonsterInstance* back = nullptr;
    for (const MonsterInstance& m : rig.monsters.All()) {
      if (m.def.id == "fx_k4_home" && m.IsAlive()) back = &m;
    }
    REQUIRE(back != nullptr);
    CHECK(back->id != g);
    CHECK((back->spawnAnchor == TilePos{60, 61} && back->homeTile == TilePos{60, 60}));
  }

  TEST_CASE("K4 runtime: a Feed keeps its corpse through the feeder's ability contacts and a later row's enter action") {
    K4Rig rig;
    MonsterPhaseDef p = K4Row(90);
    p.feed.present = true;
    p.feed.radius = 4;
    p.feed.healPct = 3;
    p.feed.delayMs = 8000;
    const MonsterDef def = K4Boss({p, K4Roar(50)}, {K4Circle("slam", 500)});
    rig.PlaceHero(Vec2(41.5, 40));
    const EntityId b = rig.Spawn(def, Vec2(40, 40), MonsterState::Attack);
    rig.HitTo(b, 80);
    rig.Step();
    REQUIRE(rig.M(b).phase == 1);
    const EntityId r = rig.Raised("fx_k4_risen", Vec2(42, 41));
    rig.h.events.Clear();
    rig.Kill(r);
    const std::vector<EvMonsterFeed> feeds = K4Events<EvMonsterFeed>(rig.h.events);
    REQUIRE(feeds.size() == 1);
    const uint32_t corpse = feeds[0].corpseId;
    const double due = feeds[0].dueMs;
    const auto holder = [&]() {
      const CorpseEntry* c = rig.monsters.CorpseRegistry().Find(corpse);
      REQUIRE(c != nullptr);
      return c->holder;
    };
    CHECK(holder() == b);
    // Its own ability winds up and lands (contact, which lets the ability's own holds go) inside the delay.
    REQUIRE(rig.StepUntil([&] { return rig.M(b).pendingAbility >= 0; }, 120));
    REQUIRE(rig.StepUntil([&] { return rig.M(b).pendingAbility < 0; }, 120));
    REQUIRE(rig.Now() < due);
    CHECK(holder() == b);
    // An eater beside the corpse, out of its fight, finds it held.
    MonsterDef eaterDef = K4Boss({}, {}, 1, "fx_k4_eater");
    eaterDef.corpse.consumes = true;
    eaterDef.aggroRange = 0.1;
    const EntityId e = rig.Spawn(eaterDef, Vec2(42.5, 41), MonsterState::Idle);
    rig.Step(30);
    CHECK(rig.M(e).consumeCorpse == 0);
    // A windup in flight when the Roar row starts: the enter action cancels it; the hold stays.
    REQUIRE(rig.StepUntil([&] { return rig.M(b).pendingAbility >= 0; }, 120));
    rig.h.events.Clear();
    rig.HitTo(b, 40);
    rig.Step();
    REQUIRE(rig.M(b).phase == 2);
    CHECK(rig.M(b).pendingAbility < 0);
    CHECK(K4Sequence(rig.h.events, b) == "CPR");
    CHECK(holder() == b);
    // At its due time the boss eats it.
    REQUIRE(rig.Now() < due);
    const double hp = rig.M(b).hp;
    rig.h.events.Clear();
    rig.StepUntilMs(due + kSimStepMs);
    std::vector<EvCorpseRemoved> gone = K4Events<EvCorpseRemoved>(rig.h.events);
    REQUIRE(gone.size() == 1);
    CHECK((gone[0].corpseId == corpse && gone[0].reason == CorpseRemoveReason::Consumed && gone[0].by == b));
    CHECK(rig.M(b).hp == hp + std::floor(rig.M(b).maxHp * 0.03));
  }

  TEST_CASE("K4 runtime: a swing drawn to the beast is timed on the row's interval; an enter action cancels it") {
    const std::string petId = K4MeleePet();
    REQUIRE(!petId.empty());
    K4PetRig rig(petId);
    MonsterPhaseDef fast = K4Row(80);
    fast.attackSpeedMul = 1.35;
    const MonsterDef def = K4Boss({fast, K4Roar(50)});
    rig.PlaceHero(Vec2(54, 40));  // beyond its aggro range: it swings at nobody by itself
    const EntityId b = rig.Spawn(def, Vec2(40, 40), MonsterState::Idle);
    rig.HitTo(b, 70);
    rig.Step();
    REQUIRE(rig.M(b).phase == 1);
    REQUIRE(rig.companion.State().present);
    const EntityId pet = rig.companion.State().entity;
    rig.PlacePet(Vec2(41, 40));
    // The beast's taunt holds it: its swing goes to the beast, timed on 1300 / 1.35 like a swing at the hero.
    rig.companion.MutableStateForTesting().taunts = {{b, rig.Now() + 60000}};
    rig.h.events.Clear();
    REQUIRE(rig.companion.InterceptMonsterAttack(b));
    const AnimTimingTable& anim = rig.h.ctx.data.Combat().anim;
    const ActionTiming t = ComputeAttackTiming(anim, rig.h.ctx.data.Assets(), kArt, AnimRig::Humanoid, 1300 / 1.35);
    REQUIRE(t.contactMs != ComputeAttackTiming(anim, rig.h.ctx.data.Assets(), kArt, AnimRig::Humanoid, 1300).contactMs);
    std::vector<EvPlayAnim> swing;
    for (const EvPlayAnim& a : K4Events<EvPlayAnim>(rig.h.events)) {
      if (a.entity == b && a.clip == "Attack01") swing.push_back(a);
    }
    REQUIRE(swing.size() == 1);
    CHECK(swing[0].contactMs == doctest::Approx(t.contactMs));
    CHECK(swing[0].durationMs == doctest::Approx(t.durationMs));
    const double contact = swing[0].startMs + swing[0].contactMs;
    // Before its contact a hit crosses the Roar row: the swing is cancelled first (EvMonsterAttackCancelled, then the
    // phase, then the Roar) and never lands on the beast.
    rig.companion.MutableStateForTesting().taunts.clear();
    rig.HitTo(b, 40);
    rig.Step();
    REQUIRE(rig.M(b).phase == 2);
    REQUIRE(rig.Now() < contact);
    CHECK(K4Sequence(rig.h.events, b) == "CPR");
    rig.StepUntilMs(contact + 200);
    for (const EvHit& e : K4Events<EvHit>(rig.h.events)) CHECK_FALSE((e.target == pet && e.source == b));
    // No such swing pending: nothing to cancel.
    CHECK_FALSE(rig.companion.CancelMonsterSwing(b));
  }

  TEST_CASE("K4 runtime: a pet's hits take the row's damageTakenMul, through CombatSystem and through its fallback") {
    const std::string petId = K4MeleePet();
    REQUIRE(!petId.empty());
    K4PetRig rig(petId);
    MonsterPhaseDef soft = K4Row(90);
    soft.damageTakenMul = 2;
    MonsterDef def = K4Boss({soft});
    def.hp = 100000;  // the beast's hits cross nothing else and kill nothing
    def.speed = 0;
    rig.PlaceHero(Vec2(41.5, 40));
    const EntityId b = rig.Spawn(def, Vec2(40, 40), MonsterState::Attack);  // in its fight: the beast's pick
    rig.HitTo(b, 85);
    rig.Step();
    REQUIRE(rig.M(b).phase == 1);
    REQUIRE(rig.companion.State().present);
    const EntityId pet = rig.companion.State().entity;
    rig.PlacePet(Vec2(39, 40));
    // Basic attacks only (every ability far off), no crit and no stray swing (every Pets draw 0.99).
    const PetDef* pd = rig.h.ctx.data.FindPet(petId);
    REQUIRE(pd != nullptr);
    std::vector<std::pair<std::string, double>>& ready = rig.companion.MutableStateForTesting().readyAtMs;
    ready.clear();
    for (const PetAbilityDef& a : pd->abilities) ready.emplace_back(a.id, 1e12);
    const std::vector<double> high(4000, 0.99);
    rig.h.rng.Get(RngStream::Pets).Script(std::span<const double>(high));
    // Through CombatSystem::DamageMonster: the hit as announced (EvHit) is floor(D x 2).
    rig.h.events.Clear();
    double hp = rig.M(b).hp;
    EvHit viaCombat;
    bool got = false;
    for (int i = 0; i < 600 && !got; ++i) {
      rig.StepPet();
      for (const EvHit& e : K4Events<EvHit>(rig.h.events)) {
        if (e.source == pet && e.target == b && !got) {
          viaCombat = e;
          got = true;
        }
      }
    }
    REQUIRE(got);
    CHECK(viaCombat.amount >= 2);
    CHECK(std::fmod(viaCombat.amount, 2) == 0);
    CHECK(rig.M(b).hp == hp - viaCombat.amount);
    // Without a CombatSystem (PetCompanion::Hit's fallback straight to MonsterSystem::ApplyDamage): the same hit.
    rig.h.ctx.sys.combat = nullptr;
    hp = rig.M(b).hp;
    for (int i = 0; i < 600 && rig.M(b).hp == hp; ++i) rig.StepPet();
    rig.h.ctx.sys.combat = &rig.combat;
    REQUIRE(rig.M(b).hp < hp);
    CHECK(hp - rig.M(b).hp == viaCombat.amount);
    rig.h.rng.Get(RngStream::Pets).ClearScript();
  }

  TEST_CASE("K4 runtime: an ability switched off and on again inside its cooldown still waits for that cooldown") {
    K4Rig rig;
    MonsterPhaseDef off = K4Row(80);
    off.abilitiesOff = {"slam"};
    MonsterPhaseDef on = K4Row(60);
    on.abilitiesOn = {"slam"};
    const MonsterDef def = K4Boss({off, on}, {K4Circle("slam", 20000)});
    rig.PlaceHero(Vec2(41.5, 40));
    const EntityId b = rig.Spawn(def, Vec2(40, 40), MonsterState::Attack);
    CHECK(rig.M(b).abilityOff == std::vector<uint8_t>{0});  // first switched off: it starts on
    const auto slams = [&]() {
      std::vector<double> out;
      for (const EvTelegraph& t : K4Events<EvTelegraph>(rig.h.events)) {
        if (t.entity == b && t.abilityId == "slam") out.push_back(t.startMs);
      }
      return out;
    };
    REQUIRE(rig.StepUntil([&] { return !slams().empty(); }, 120));
    const double first = slams()[0];
    REQUIRE(rig.StepUntil([&] { return rig.M(b).pendingAbility < 0; }, 120));
    rig.HitTo(b, 79);
    rig.Step();
    REQUIRE(rig.M(b).phase == 1);
    CHECK(rig.M(b).abilityOff == std::vector<uint8_t>{1});
    rig.HitTo(b, 59);
    rig.Step();
    REQUIRE(rig.M(b).phase == 2);
    CHECK(rig.M(b).abilityOff == std::vector<uint8_t>{0});
    CHECK(rig.M(b).abilityReadyAtMs.at(0) >= first + 20000 - 1e-6);  // the cooldown it spent, not initialCooldownMs
    rig.h.events.Clear();
    rig.StepUntilMs(first + 15000);
    CHECK(slams().empty());
  }

  // =================================================================================================================
  // GameSim wiring: the step hook, the snapshot, the boss bar's ticks
  // =================================================================================================================
  TEST_CASE("K4 wiring: GameSim steps TickPhases; the snapshot shows the phase, the arena and the boss bar's ticks") {
    const std::string boss = test::RealData().Monsters().ai.storyBossId;  // read, never spelled
    REQUIRE(!boss.empty());
    static std::unique_ptr<DataStore> store;
    if (store == nullptr) {
      DataLoadReport report;
      store = K4LoadEdited({{"monsters.json",
                             [boss](JsonValue& doc) {
                               JsonValue* def = doc.FindMutable("defs")->FindMutable(boss);
                               REQUIRE(def != nullptr);
                               def->Set("phases", K4Parse(R"([
                                 {"hpBelowPct": 60, "stinger": "fx_k4_stinger",
                                  "arena": {"kind": "ringShrink", "fromR": 9, "toR": 5, "overMs": 40000, "dpsPctMaxHp": 4}},
                                 {"hpBelowPct": 30}])"));
                             }}},
                           report);
      INFO(report.Summary());
      REQUIRE(report.Ok());
    }
    auto sim = GameSim::Create(*store, SimConfig{});
    REQUIRE(sim != nullptr);
    REQUIRE(sim->NewGame(ClassId::Warrior, Difficulty::Normal, 79));
    sim->Context().sys.story->FinishAllBeats();
    sim->Step();
    SimContext& ctx = sim->Context();
    ZoneRuntime& zone = *ctx.sys.zone;
    const MapDef& map = zone.Map();
    TilePos spot{-1, -1};
    for (int32_t r = 3; r < map.rows - 3 && spot.col < 0; ++r) {
      for (int32_t c = 3; c < map.cols - 15 && spot.col < 0; ++c) {
        bool ok = true;
        for (int32_t dr = -2; dr <= 2 && ok; ++dr) {
          for (int32_t dc = -2; dc <= 13 && ok; ++dc) ok = zone.Walkable(c + dc, r + dr);
        }
        for (const MapCampDef& camp : map.camps) ok = ok && Dist(Vec2(c, r), camp.pos.Center()) > map.safeZoneRadius + 14;
        if (ok) spot = TilePos{c, r};
      }
    }
    REQUIRE(spot.col >= 0);
    ctx.sys.locomotion->Teleport(spot.Center(), TeleportReason::Debug);
    const MonsterDef* def = ctx.data.FindMonster(boss);
    REQUIRE(def != nullptr);
    REQUIRE(def->phases.size() == 2);
    MonsterSpawnParams p;
    p.baseDef = def;
    p.tile = TilePos{spot.col + 11, spot.row};  // inside the bar range, outside its sight (no intro beat) and aggro
    const EntityId b = ctx.sys.monsters->Spawn(p);
    REQUIRE(b != kNoEntity);
    for (int i = 0; i < 30; ++i) sim->Step();
    const auto monsterView = [&]() -> const MonsterView* {
      for (const MonsterView& v : sim->View().monsters) {
        if (v.id == b) return &v;
      }
      return nullptr;
    };
    REQUIRE(monsterView() != nullptr);
    CHECK(monsterView()->phase == 0);
    REQUIRE(sim->View().bossBar.show);
    REQUIRE(sim->View().bossBar.monster == b);
    CHECK(sim->View().bossBar.phase == 0);
    CHECK(sim->View().bossBar.phaseThresholdsPct == std::vector<double>{60, 30});
    MonsterHitRequest r;
    r.monster = b;
    r.amount = ctx.sys.monsters->Find(b)->hp - std::floor(ctx.sys.monsters->Find(b)->maxHp * 0.5);
    r.attacker = kHeroEntityId;
    r.source = KillSource::HeroSkill;
    r.provokes = false;
    ctx.sys.combat->DamageMonster(r);
    sim->Step();
    std::vector<EvMonsterPhase> ev = K4Events<EvMonsterPhase>(sim->Events());
    REQUIRE(ev.size() == 1);
    CHECK((ev[0].entity == b && ev[0].phase == 1 && ev[0].stinger == "fx_k4_stinger"));
    REQUIRE(K4Events<EvArena>(sim->Events()).size() == 1);
    REQUIRE(monsterView() != nullptr);
    CHECK(monsterView()->phase == 1);
    CHECK(sim->View().bossBar.phase == 1);
    REQUIRE(sim->View().arenas.size() == 1);
    CHECK((sim->View().arenas[0].entity == b && sim->View().arenas[0].centre == Vec2(spot.col + 11, spot.row) &&
           sim->View().arenas[0].radius == doctest::Approx(9).epsilon(1e-3)));
  }

  TEST_CASE("K4 wiring: a zone spawn's default centres are its authored tile, whatever tile M10's jitter placed it on") {
    const std::string boss = test::RealData().Monsters().ai.storyBossId;  // read, never spelled
    REQUIRE(!boss.empty());
    static std::unique_ptr<DataStore> store;
    if (store == nullptr) {
      DataLoadReport report;
      store = K4LoadEdited({{"monsters.json",
                             [boss](JsonValue& doc) {
                               JsonValue* def = doc.FindMutable("defs")->FindMutable(boss);
                               REQUIRE(def != nullptr);
                               def->Set("phases", K4Parse(R"([{"hpBelowPct": 60, "leash": {"radius": 3},
                                 "arena": {"kind": "ringShrink", "fromR": 9, "toR": 5, "overMs": 40000, "dpsPctMaxHp": 4}}])"));
                             }}},
                           report);
      INFO(report.Summary());
      REQUIRE(report.Ok());
    }
    int jittered = 0;
    for (uint64_t seed : {79ull, 1000ull, 1001ull, 1002ull}) {
      INFO("seed " << seed);
      auto sim = GameSim::Create(*store, SimConfig{});
      REQUIRE(sim != nullptr);
      REQUIRE(sim->NewGame(ClassId::Warrior, Difficulty::Normal, seed));
      sim->Context().sys.story->FinishAllBeats();
      sim->Step();
      SimContext& ctx = sim->Context();
      TilePos authored{-1, -1};
      for (const MapSpawnDef& s : ctx.sys.zone->Map().spawns) {
        if (s.monsterId == boss) authored = s.pos;
      }
      REQUIRE(authored.col >= 0);
      EntityId b = kNoEntity;
      for (const MonsterInstance& m : ctx.sys.monsters->All()) {
        if (m.def.id == boss && m.IsAlive()) b = m.id;
      }
      REQUIRE(b != kNoEntity);
      const MonsterInstance* m = ctx.sys.monsters->Find(b);
      CHECK(m->homeTile == authored);
      if (!(m->spawnAnchor == authored)) ++jittered;
      MonsterHitRequest r;
      r.monster = b;
      r.amount = m->hp - std::floor(m->maxHp * 0.5);
      r.attacker = kHeroEntityId;
      r.source = KillSource::HeroSkill;
      r.provokes = false;
      ctx.sys.combat->DamageMonster(r);
      sim->Step();
      REQUIRE(ctx.sys.monsters->Find(b)->phase == 1);
      CHECK(ctx.sys.monsters->Find(b)->tetherCentre == authored.Center());
      const std::vector<EvArena> arenas = K4Events<EvArena>(sim->Events());
      REQUIRE(arenas.size() == 1);
      CHECK(arenas[0].centre == authored.Center());
    }
    CHECK(jittered >= 1);  // the case bites: some seed placed the boss off its authored tile
  }

  // =================================================================================================================
  // Replay: a seeded boss fight through every K4 mechanism replays bit for bit (GCC = Clang)
  // =================================================================================================================
  // A boss with three rows (a Roar with two summons, Feed and a stinger; then the frenzy multipliers, a switched-on lunge
  // and the shrinking ring; then a tether), a sweep from the start, Raised adds the hero kills by the dais, the hero
  // circling on a fixed loop and its basic attacks plus periodic skill-sized hits (the Combat stream feeds both). The
  // digest of the event log is pinned: GCC and Clang (and every platform) must replay it bit for bit.
  TEST_CASE("K4 replay: a seeded boss fight with phases, an arena, summons and Feed replays to the pinned digest") {
    struct Replay {
      uint64_t digest = 0;
      int phases = 0, arenas = 0, raised = 0, feeds = 0, consumed = 0, arenaTicks = 0, roars = 0;
    };
    const auto run = [](uint64_t seed) {
      Replay out;
      K4Rig rig(seed);
      MonsterAbilityDef sweep;
      sweep.id = "sweep";
      sweep.kind = AbilityKind::Cone;
      sweep.clip = "Attack01";
      sweep.aim = AbilityAim::Hero;
      sweep.hasShape = true;
      sweep.shape.kind = AbilityShapeKind::Cone;
      sweep.shape.radius = 2.6;
      sweep.shape.arcDeg = 120;
      sweep.hasWindupMs = true;
      sweep.windupMs = 700;
      sweep.cooldownMs = 5000;
      MonsterAbilityDef lunge;
      lunge.id = "lunge";
      lunge.kind = AbilityKind::Line;
      lunge.clip = "Attack01";
      lunge.aim = AbilityAim::Hero;
      lunge.hasShape = true;
      lunge.shape.kind = AbilityShapeKind::Line;
      lunge.shape.length = 5;
      lunge.shape.width = 1.2;
      lunge.hasWindupMs = true;
      lunge.windupMs = 800;
      lunge.cooldownMs = 7000;
      lunge.dmgMul = 1.5;
      lunge.priority = 1;
      MonsterPhaseDef p1 = K4Roar(70);
      PhaseSummonDef adds;
      adds.monsterId = "fx_k4_risen";
      adds.count = 2;
      adds.hasRadius = true;
      adds.radius = 2;
      adds.hpPct = 40;
      adds.respawnMs = 4000;
      adds.respawns = 2;
      p1.summons = {adds};
      p1.feed.present = true;
      p1.feed.radius = 4;
      p1.feed.healPct = 3;
      p1.feed.delayMs = 800;
      p1.stinger = "fx_roll_call";
      MonsterPhaseDef p2 = K4Roar(40);
      p2.attackSpeedMul = 1.25;
      p2.speedMul = 1.15;
      p2.damageTakenMul = 1.1;
      p2.abilitiesOn = {"lunge"};
      p2.arena.present = true;
      p2.arena.fromR = 9;
      p2.arena.toR = 5;
      p2.arena.overMs = 20000;
      p2.arena.dpsPctMaxHp = 4;
      p2.arena.damageType = DamageType::Fire;
      MonsterPhaseDef p3 = K4Row(15);
      p3.leash.present = true;
      p3.leash.radius = 3;
      MonsterDef def = K4Boss({p1, p2, p3}, {sweep, lunge}, 6);
      def.hp = 600;
      def.hitCapHeroMaxHpPct = 35;
      rig.PlaceHero(Vec2(41.5, 40));
      const EntityId boss = rig.Spawn(def, Vec2(40, 40), MonsterState::Chase);
      rig.hero->attackTarget = boss;
      K4Fnv f;
      const Vec2 loop[] = {Vec2(41.5, 40), Vec2(42, 41.5), Vec2(40.5, 42), Vec2(38.5, 40.5), Vec2(41, 38.8),
                           Vec2(47, 40)};
      for (int s = 0; s < 40 * 60; ++s) {
        if (s % 75 == 0 && rig.hero->Life() == HeroLife::Alive) rig.PlaceHero(loop[(s / 75) % 6]);
        if (s % 40 == 20 && rig.monsters.Find(boss) != nullptr && rig.M(boss).IsAlive()) {
          // A skill-sized hit on whatever stands nearest the hero (the adds by the dais feed the boss).
          const EntityId t = rig.monsters.NearestAlive(rig.hero->Position(), 3);
          if (t != kNoEntity && t != rig.dummy) rig.Hit(t, 9 + static_cast<double>(rig.h.rng.Get(RngStream::Combat).RandomInt(0, 8)));
        }
        rig.Step();
        K4DigestEvents(f, rig.h.events);
        out.phases += static_cast<int>(K4Events<EvMonsterPhase>(rig.h.events).size());
        for (const EvArena& a : K4Events<EvArena>(rig.h.events)) out.arenas += a.cleared ? 0 : 1;
        out.raised += static_cast<int>(K4Events<EvMonsterRaised>(rig.h.events).size());
        out.feeds += static_cast<int>(K4Events<EvMonsterFeed>(rig.h.events).size());
        for (const EvCorpseRemoved& c : K4Events<EvCorpseRemoved>(rig.h.events)) {
          out.consumed += c.reason == CorpseRemoveReason::Consumed ? 1 : 0;
        }
        out.arenaTicks += static_cast<int>(ArenaTicks(rig.h.events, boss).size());
        for (const EvPlayAnim& a : K4Events<EvPlayAnim>(rig.h.events)) out.roars += a.action == AnimAction::Roar ? 1 : 0;
        rig.h.events.Clear();
      }
      f.D(rig.hero->Hp());
      for (const MonsterInstance& m : rig.monsters.All()) {
        f.U64(m.id);
        f.D(m.hp);
        f.V(m.pos);
        f.U64(static_cast<uint64_t>(m.phase));
      }
      out.digest = f.h;
      return out;
    };
    const Replay r = run(5150);
    // The fight used every mechanism.
    CHECK(r.phases >= 3);
    CHECK(r.arenas >= 1);
    CHECK(r.raised >= 2);
    CHECK(r.feeds >= 1);
    CHECK(r.consumed >= 1);
    CHECK(r.arenaTicks >= 1);
    CHECK(r.roars >= 2);
    CHECK(r.digest == run(5150).digest);  // same seed, same log
    CHECK(r.digest != run(5151).digest);  // the seed feeds the rolls
    char hex[17];
    std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(r.digest));
    INFO("digest " << hex);
    CHECK(std::string(hex) == "4567bd8648a82662");
  }
}
