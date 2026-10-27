// K2 monster abilities and telegraphs (dark remake, implementation-plan.md 3 "K2", work package 2A): the
// `abilities[]` schema (loader shape rules, DataStore cross checks), the pure rules of MonsterAbilities.h (picker, shapes,
// contact timing, hit cap, telegraph style), the HazardField schedule, and the runtime through CombatSystem /
// MonsterSystem (the pending-strike path, aim snapshots, aborts, hazards, damage types, the per-difficulty hit cap,
// rooting, the snapshot views) plus a seeded replay whose event-log digest is pinned (GCC and Clang must both match it).
//
// Synthetic content only: monster definitions are built here (TestContent.h fixtures), the art beats come from one
// synthetic asset (`fx_k2_art`) added to the real assets.json in memory, so no shipped content id is spelled and no
// content change moves these tests. Suite: monsters.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "MonstersFixtures.h"
#include "SimHarness.h"
#include "TestContent.h"
#include "TestUtil.h"
#include "abyss/base/Json.h"
#include "abyss/base/Math.h"
#include "abyss/base/StrUtil.h"
#include "abyss/combat/HazardField.h"
#include "abyss/data/DataStore.h"
#include "abyss/items/Inventory.h"
#include "abyss/monsters/MonsterAbilities.h"
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

constexpr char kArt[] = "fx_k2_art";  // the synthetic asset's game id (its clips carry the beats below)

// Clips of the synthetic asset: Attack01 Contact 500 / 1000 ms, Attack02 Contact 750 / 1300 ms, Cast01 Release 820 /
// 1500 ms, Cast02 Contact 400 / 900 ms (under the 600 ms floor), Roar without a beat.
constexpr char kK2AssetJson[] = R"({
  "kind": "SkeletalMesh", "category": "Monsters", "gameIds": ["fx_k2_art"], "skeleton": "SKEL_Human",
  "anims": [
    {"name": "Attack01", "lengthMs": 1000, "contactMs": 500, "notifies": [{"name": "Contact", "ms": 500}]},
    {"name": "Attack02", "lengthMs": 1300, "contactMs": 750, "notifies": [{"name": "Contact", "ms": 750}]},
    {"name": "Cast01", "lengthMs": 1500, "releaseMs": 820, "notifies": [{"name": "Release", "ms": 820}]},
    {"name": "Cast02", "lengthMs": 900, "contactMs": 400, "notifies": [{"name": "Contact", "ms": 400}]},
    {"name": "Roar", "lengthMs": 1200}
  ]
})";

JsonValue K2Parse(std::string_view text) {
  JsonValue v;
  JsonParseError err;
  const bool ok = ParseJson(text, v, &err);
  REQUIRE_MESSAGE(ok, "bad test JSON");
  return v;
}

using K2Edits = std::vector<std::pair<std::string, std::function<void(JsonValue&)>>>;

// The real export with `edits` applied in memory (several edits of one file run in order on one document).
std::unique_ptr<DataStore> K2LoadEdited(const K2Edits& edits, DataLoadReport& report) {
  std::map<std::string, JsonValue> docs;
  for (const auto& [file, edit] : edits) {
    if (docs.count(file) == 0) docs.emplace(file, K2Parse(test::ReadFile(test::DataDir() + "/" + file)));
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

void K2AddAsset(JsonValue& assets) {
  JsonValue* list = assets.FindMutable("manifest");
  REQUIRE(list != nullptr);
  list = list->FindMutable("assets");
  REQUIRE(list != nullptr);
  list->Set("SK_Fx_K2", K2Parse(kK2AssetJson));
}

// The real export plus the synthetic asset (loaded once).
const DataStore& K2Data() {
  static std::unique_ptr<DataStore> store;
  if (store == nullptr) {
    DataLoadReport report;
    store = K2LoadEdited({{"assets.json", K2AddAsset}}, report);
    if (!report.Ok()) MESSAGE(report.Summary());
  }
  return *store;
}

// ---- synthetic definitions ------------------------------------------------------------------------------------------

AbilityShapeDef K2Shape(AbilityShapeKind kind, double radius, double arcDeg = 0, double length = 0, double width = 0,
                        double inner = 0) {
  AbilityShapeDef s;
  s.kind = kind;
  s.radius = radius;
  s.arcDeg = arcDeg;
  s.length = length;
  s.width = width;
  s.innerRadius = inner;
  return s;
}

MonsterAbilityDef K2Ability(std::string id, AbilityKind kind, std::string clip, AbilityAim aim, double windupMs = 0) {
  MonsterAbilityDef a;
  a.id = std::move(id);
  a.kind = kind;
  a.clip = std::move(clip);
  a.aim = aim;
  if (windupMs > 0) {
    a.hasWindupMs = true;
    a.windupMs = windupMs;
  }
  return a;
}

MonsterAbilityDef K2Circle(std::string id, double radius, AbilityAim aim = AbilityAim::HeroAtWindup,
                           double windupMs = 0, std::string clip = "Attack02") {
  MonsterAbilityDef a = K2Ability(std::move(id), AbilityKind::Circle, std::move(clip), aim, windupMs);
  a.hasShape = true;
  a.shape = K2Shape(AbilityShapeKind::Circle, radius);
  return a;
}

// A sturdy melee caster on the synthetic art: level 5, 400 HP, `damage`, reach 2, a swing every 1500 ms.
MonsterDef K2Caster(std::vector<MonsterAbilityDef> abilities, double damage = 20, const char* id = "fx_k2_caster") {
  MonsterDef d = test::fixture::MakeFixtureMonster(id, 5, 400, damage, 5, 40, 6, 2.0, 1500, 10, 1, 2, kArt,
                                                   AnimRig::Humanoid, false, false);
  d.abilities = std::move(abilities);
  return d;
}

// ---- runtime rig: the combat systems around a SimHarness on the synthetic data, stepped in the GameSim order ---------
struct K2Rig {
  explicit K2Rig(uint64_t seed = 5, Difficulty difficulty = Difficulty::Normal)
      : h(seed, K2Data()),
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
    dummy = Spawn(test::fixture::Grunt(), Vec2(70, 70), MonsterState::Idle);
    hero->attackTarget = dummy;
    h.events.Clear();
  }

  void Step(int n = 1) {
    for (int i = 0; i < n; ++i) {
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
  void PlaceHero(Vec2 p) { loco.Teleport(p, TeleportReason::Debug); }
  // A long stun: the monster swings no more (its hazards keep ticking).
  void Silence(EntityId id) { status.Apply(id, StatusType::Stun, 1, 600000, kHeroEntityId, Now()); }
  // "no dodge, no crit" for the next `hits` monster rolls (two draws each).
  void ScriptNoDodge(int hits = 1) {
    std::vector<double> v(static_cast<size_t>(2 * hits), 0.99);
    h.rng.Get(RngStream::Combat).Script(v);
  }
  // What a monster's next roll on the hero deals (the CalculateDamage the swing path runs, "no dodge, no crit").
  double ExpectedHit(EntityId monster, double dmgMul, DamageType type) {
    Rng rng(1);
    rng.Script({0.99, 0.99});
    SkillHitInput in;
    in.basicType = type;
    in.basicMultiplier = dmgMul;
    const ClassTables& c = h.ctx.data.Classes();
    return CalculateDamage(DamageRules{&c.formulas, &c.buffCaps, &c.skillRules}, CombatSystem::MonsterCombatant(M(monster)),
                           combat.HeroCombatant(), in, false, rng)
        .damage;
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
  EntityId dummy = kNoEntity;
};

// K2Rig plus a ley-beast at the hero's side (the systems PetCompanion talks to; the beast never acts on its own: its
// Tick is not stepped, so only InterceptMonsterAttack and its hit timer run). The beast is the first pet of the data.
struct K2PetRig : K2Rig {
  K2PetRig()
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
    REQUIRE_FALSE(h.ctx.data.Pets().pets.empty());
    petAdded = pets.AddPet(h.ctx.data.Pets().pets.front().id, true);
    h.events.Clear();
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
  bool petAdded = false;
};

template <class E>
std::vector<E> K2Events(const EventSink& sink) {
  std::vector<E> out;
  for (const Event& e : sink.Items()) {
    if (const E* p = std::get_if<E>(&e)) out.push_back(*p);
  }
  return out;
}

std::vector<EvHit> HeroHits(const EventSink& sink, bool ticks) {
  std::vector<EvHit> out;
  for (const EvHit& e : K2Events<EvHit>(sink)) {
    if (e.target == kHeroEntityId && !e.dodged && e.tick == ticks) out.push_back(e);
  }
  return out;
}

size_t CancelsOf(const EventSink& sink, EntityId monster) {
  size_t n = 0;
  for (const EvMonsterAttackCancelled& e : K2Events<EvMonsterAttackCancelled>(sink)) n += e.monster == monster ? 1 : 0;
  return n;
}

// ---- loader fixtures ------------------------------------------------------------------------------------------------

// A monsters.json definition with `abilities` (a JSON array text) and extra top-level members (`extra`, JSON object
// members without braces).
std::string K2DefJson(std::string_view id, std::string_view abilities, std::string_view extra = "",
                      bool miniBoss = false) {
  return StrCat(R"({"id": ")", id, R"(", "name": ")", id, R"(", "level": 5, "hp": 200, "damage": 20, "defense": 5,
    "speed": 40, "aggroRange": 6, "attackRange": 2, "attackSpeed": 1500, "expReward": 10, "goldReward": [1, 2],
    "spriteKey": "fx_k2_art", "animCategory": "humanoid", "isMiniBoss": )",
                miniBoss ? "true" : "false", extra.empty() ? "" : ", ", extra, R"(, "abilities": )", abilities,
                R"(, "derived": {"isRanged": false, "onHitStatus": [], "projectileColor": 13386956, "homeZone": null,
    "nameKey": "data.monster.)",
                id, R"(", "sources": ["zone"]}})");
}

std::unique_ptr<DataStore> K2LoadDef(std::string defJson, DataLoadReport& report) {
  return K2LoadEdited({{"assets.json", K2AddAsset},
                       {"monsters.json",
                        [defJson](JsonValue& doc) {
                          JsonValue def = K2Parse(defJson);
                          const std::string id(def.Get("id").AsString());
                          JsonValue* defs = doc.FindMutable("defs");
                          REQUIRE(defs != nullptr);
                          defs->Set(id, std::move(def));
                        }}},
                      report);
}

bool K2HasIssue(const std::vector<DataIssue>& issues, std::string_view pathPart, std::string_view text) {
  for (const DataIssue& e : issues) {
    if (Contains(e.path, pathPart) && Contains(e.message, text)) return true;
  }
  return false;
}

// ---- replay digest --------------------------------------------------------------------------------------------------

struct K2Fnv {
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
  void Shape(const AbilityShapeDef& s) {
    U64(static_cast<uint64_t>(s.kind));
    D(s.radius);
    D(s.arcDeg);
    D(s.length);
    D(s.width);
    D(s.innerRadius);
  }
};

// Every event's variant index, and the fields of the ones a K2 fight produces.
void K2DigestEvents(K2Fnv& f, const EventSink& sink) {
  for (const Event& e : sink.Items()) {
    f.U64(e.index());
    if (const EvPlayAnim* a = std::get_if<EvPlayAnim>(&e)) {
      f.U64(a->entity);
      f.U64(static_cast<uint64_t>(a->action));
      f.S(a->clip);
      f.D(a->startMs);
      f.D(a->contactMs);
      f.D(a->windupMs);
      f.D(a->durationMs);
      f.D(a->playRate);
      f.V(a->faceTarget);
    } else if (const EvHit* h = std::get_if<EvHit>(&e)) {
      f.U64(h->target);
      f.U64(h->source);
      f.D(h->amount);
      f.U64(static_cast<uint64_t>(h->element));
      f.U64((h->crit ? 1u : 0u) | (h->dodged ? 2u : 0u) | (h->tick ? 4u : 0u) | (h->killed ? 8u : 0u));
      f.D(h->targetHp);
    } else if (const EvTelegraph* t = std::get_if<EvTelegraph>(&e)) {
      f.U64(t->entity);
      f.S(t->abilityId);
      f.Shape(t->shape);
      f.V(t->origin);
      f.V(t->dir);
      f.D(t->startMs);
      f.D(t->contactMs);
      f.U64(static_cast<uint64_t>(t->style));
    } else if (const EvHazard* z = std::get_if<EvHazard>(&e)) {
      f.U64(z->id);
      f.U64(z->source);
      f.S(z->abilityId);
      f.Shape(z->shape);
      f.V(z->origin);
      f.V(z->dir);
      f.D(z->startMs);
      f.D(z->untilMs);
    } else if (const EvStatusApplied* s = std::get_if<EvStatusApplied>(&e)) {
      f.U64(s->target);
      f.U64(static_cast<uint64_t>(s->type));
      f.D(s->value);
      f.D(s->durationMs);
    } else if (const EvProjectileLaunched* p = std::get_if<EvProjectileLaunched>(&e)) {
      f.U64(p->source);
      f.V(p->from);
      f.V(p->to);
      f.D(p->launchMs);
      f.D(p->travelMs);
    } else if (const EvMonsterAttackCancelled* c = std::get_if<EvMonsterAttackCancelled>(&e)) {
      f.U64(c->monster);
    }
  }
}

}  // namespace

TEST_SUITE("monsters") {
  // =================================================================================================================
  // Data: the abilities schema (LoadMonsters.cpp) and DataStore::Finalize's cross checks
  // =================================================================================================================
  TEST_CASE("K2 data: an ability loads with every field, and damageType / hitCap / bossSkills on the monster") {
    const std::string abilities = R"([
      {"id": "slam", "kind": "circle", "clip": "Attack02", "windupMs": 900, "cooldownMs": 6000, "initialCooldownMs": 1500,
       "rangeMin": 0.5, "rangeMax": 4, "hpBelowPct": 80, "phases": [0, 2], "priority": 3, "aim": "heroAtWindup",
       "shape": {"radius": 1.8}, "dmgMul": 1.6, "damageType": "fire", "status": {"type": "slow", "chance": 0.5, "ms": 2000, "value": 30},
       "hitsMonsters": true, "monsterDmgMul": 2,
       "hazard": {"durationMs": 4000, "tickMs": 500, "tickDmgMul": 0.15, "status": {"type": "burn", "ms": 1000, "value": 2},
                  "shape": {"kind": "ring", "radius": 2, "innerRadius": 1}}},
      {"id": "sweep", "kind": "cone", "clip": "Attack02", "cooldownMs": 5000, "aim": "hero",
       "shape": {"radius": 2.6, "arcDeg": 120}},
      {"id": "seep", "kind": "hazard", "clip": "Cast01", "cooldownMs": 6000, "aim": "anchor",
       "shape": {"kind": "line", "length": 8, "width": 1}, "hazard": {"durationMs": 1000, "tickMs": 500, "tickDmgMul": 0.5}},
      {"id": "bolt", "kind": "bolt", "clip": "Cast01", "cooldownMs": 3000, "aim": "hero"},
      {"id": "flock", "kind": "raise", "clip": "Cast01", "cooldownMs": 8000, "aim": "corpse",
       "summon": {"monsterId": "fx_k2_minion", "count": 2, "capAlive": 3, "perWindowCap": 1, "windowMs": 8000, "hpPct": 40}},
      {"id": "muster", "kind": "summon", "clip": "Cast01", "cooldownMs": 20000, "aim": "anchor",
       "summon": {"monsterId": "fx_k2_minion", "at": "anchors", "anchors": [{"col": 11, "row": 91}, {"col": 19, "row": 99}]}},
      {"id": "drum", "kind": "call", "clip": "Cast01", "cooldownMs": 8000, "aim": "self",
       "buff": {"stat": "fearImmune", "ms": 2000, "radius": 8, "family": "living"}}
    ])";
    DataLoadReport report;
    auto store = K2LoadEdited(
        {{"assets.json", K2AddAsset},
         {"monsters.json",
          [&](JsonValue& doc) {
            JsonValue* defs = doc.FindMutable("defs");
            REQUIRE(defs != nullptr);
            defs->Set("fx_k2", K2Parse(K2DefJson("fx_k2", abilities,
                                                 R"("damageType": "ice", "hitCapHeroMaxHpPct": 35, "bossSkills": ["slam", "seep"])")));
            defs->Set("fx_k2_minion", K2Parse(K2DefJson("fx_k2_minion", "[]")));
          }}},
        report);
    INFO(report.Summary());
    REQUIRE(report.Ok());
    const MonsterDef* d = store->FindMonster("fx_k2");
    REQUIRE(d != nullptr);
    CHECK(d->damageType == DamageType::Ice);
    CHECK(d->hitCapHeroMaxHpPct == 35);
    CHECK(d->bossSkills == std::vector<std::string>{"slam", "seep"});
    REQUIRE(d->abilities.size() == 7);
    const MonsterAbilityDef& s = d->abilities[0];
    CHECK(s.id == "slam");
    CHECK(s.kind == AbilityKind::Circle);
    CHECK(s.clip == "Attack02");
    CHECK((s.hasWindupMs && s.windupMs == 900));
    CHECK(s.cooldownMs == 6000);
    CHECK(s.initialCooldownMs == 1500);
    CHECK(s.rangeMin == 0.5);
    CHECK((s.hasRangeMax && s.rangeMax == 4));
    CHECK((s.hasHpBelowPct && s.hpBelowPct == 80));
    CHECK(s.phases == std::vector<int32_t>{0, 2});
    CHECK(s.priority == 3);
    CHECK(s.aim == AbilityAim::HeroAtWindup);
    CHECK((s.hasShape && s.shape.kind == AbilityShapeKind::Circle && s.shape.radius == 1.8));
    CHECK(s.dmgMul == 1.6);
    CHECK((s.hasDamageType && s.damageType == DamageType::Fire));
    CHECK((s.status.present && s.status.type == StatusType::Slow && s.status.chance == 0.5 && s.status.ms == 2000 &&
           s.status.value == 30));
    CHECK((s.hitsMonsters && s.monsterDmgMul == 2));
    CHECK((s.hazard.present && s.hazard.durationMs == 4000 && s.hazard.tickMs == 500 && s.hazard.tickDmgMul == 0.15));
    CHECK((s.hazard.status.present && s.hazard.status.type == StatusType::Burn && s.hazard.status.chance == 1));
    CHECK((s.hazard.hasShape && s.hazard.shape.kind == AbilityShapeKind::Ring && s.hazard.shape.innerRadius == 1));
    const MonsterAbilityDef& cone = d->abilities[1];
    CHECK((cone.kind == AbilityKind::Cone && cone.shape.kind == AbilityShapeKind::Cone && cone.shape.arcDeg == 120));
    CHECK((!cone.hasWindupMs && cone.initialCooldownMs == 0 && cone.priority == 0 && !cone.hasRangeMax));
    CHECK((cone.dmgMul == 1 && !cone.hasDamageType && !cone.hazard.present && cone.phases.empty()));
    const MonsterAbilityDef& seep = d->abilities[2];
    CHECK((seep.kind == AbilityKind::Hazard && seep.shape.kind == AbilityShapeKind::Line && seep.shape.length == 8));
    CHECK(seep.aim == AbilityAim::Anchor);
    CHECK((d->abilities[3].kind == AbilityKind::Bolt && !d->abilities[3].hasShape));
    const AbilitySummonDef& flock = d->abilities[4].summon;
    CHECK((flock.present && flock.monsterId == "fx_k2_minion" && flock.count == 2 && flock.at == AbilitySummonAt::Corpse));
    CHECK((flock.capAlive == 3 && flock.perWindowCap == 1 && flock.windowMs == 8000 && flock.hpPct == 40));
    const AbilitySummonDef& muster = d->abilities[5].summon;
    CHECK((muster.at == AbilitySummonAt::Anchors && muster.anchors.size() == 2 && muster.anchors[1] == TilePos{19, 99}));
    CHECK((muster.count == 1 && muster.hpPct == 100));
    const AbilityBuffDef& drum = d->abilities[6].buff;
    CHECK((drum.present && drum.stat == MonsterBuffStat::FearImmune && drum.mul == 1 && drum.radius == 8 &&
           drum.family == "living"));
    // A monster without the fields keeps today's: physical, uncapped, no abilities.
    const MonsterDef* plain = store->FindMonster("fx_k2_minion");
    REQUIRE(plain != nullptr);
    CHECK((plain->damageType == DamageType::Physical && plain->hitCapHeroMaxHpPct == 0 && plain->abilities.empty()));
  }

  TEST_CASE("K2 data: the loader refuses broken abilities (shape, aim, blocks, fairness floor, unknown fields)") {
    struct Case {
      const char* ability;  // one ability object
      const char* text;     // expected message part
    };
    const Case cases[] = {
        {R"({"id": "a", "kind": "circle", "clip": "Attack02", "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}, "windupMS": 900})",
         "unknown ability field 'windupMS'"},
        {R"({"id": "a", "kind": "circle", "clip": "Attack02", "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}, "windupMs": 599})",
         ">= 600 ms"},
        {R"({"id": "a", "kind": "circle", "clip": "Attack02", "cooldownMs": 0, "aim": "self", "shape": {"radius": 0}})",
         "radius > 0"},
        {R"({"id": "a", "kind": "circle", "clip": "Attack02", "cooldownMs": 0, "aim": "self"})", "needs a shape"},
        {R"({"id": "a", "kind": "circle", "clip": "Attack02", "cooldownMs": 0, "aim": "hero", "shape": {"radius": 1}})",
         "does not fit a circle"},
        {R"({"id": "a", "kind": "ring", "clip": "Attack02", "cooldownMs": 0, "aim": "self", "shape": {"radius": 1, "innerRadius": 1}})",
         "radius > innerRadius"},
        {R"({"id": "a", "kind": "cone", "clip": "Attack02", "cooldownMs": 0, "aim": "hero", "shape": {"radius": 2, "arcDeg": 0}})",
         "0 < arcDeg <= 360"},
        {R"({"id": "a", "kind": "line", "clip": "Attack02", "cooldownMs": 0, "aim": "hero", "shape": {"length": 4, "width": 0}})",
         "width > 0"},
        {R"({"id": "a", "kind": "line", "clip": "Attack02", "cooldownMs": 0, "aim": "hero", "shape": {"kind": "cone", "length": 4, "width": 1}})",
         "differs from the ability's kind"},
        {R"({"id": "a", "kind": "bolt", "clip": "Cast01", "cooldownMs": 0, "aim": "self"})", "does not fit a bolt"},
        {R"({"id": "a", "kind": "bolt", "clip": "Cast01", "cooldownMs": 0, "aim": "hero", "shape": {"radius": 1}})",
         "have a shape"},
        {R"({"id": "a", "kind": "hazard", "clip": "Cast01", "cooldownMs": 0, "aim": "self", "shape": {"kind": "circle", "radius": 1}})",
         "needs its 'hazard' block"},
        {R"({"id": "a", "kind": "hazard", "clip": "Cast01", "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}, "hazard": {"durationMs": 1000, "tickMs": 500, "tickDmgMul": 1}})",
         "needs its 'kind'"},
        {R"({"id": "a", "kind": "circle", "clip": "Attack02", "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}, "hazard": {"durationMs": 1000, "tickMs": 50, "tickDmgMul": 1}})",
         "100 <= tickMs <= durationMs"},
        {R"({"id": "a", "kind": "hazard", "clip": "Cast01", "cooldownMs": 0, "aim": "self", "shape": {"kind": "circle", "radius": 1}, "dmgMul": 2, "hazard": {"durationMs": 1000, "tickMs": 500, "tickDmgMul": 1}})",
         "hits through its hazard"},
        {R"({"id": "a", "kind": "bolt", "clip": "Cast01", "cooldownMs": 0, "aim": "hero", "hazard": {"durationMs": 1000, "tickMs": 500, "tickDmgMul": 1}})",
         "only ground abilities leave a hazard"},
        {R"({"id": "a", "kind": "raise", "clip": "Cast01", "cooldownMs": 0, "aim": "corpse"})", "needs its 'summon' block"},
        {R"({"id": "a", "kind": "circle", "clip": "Attack02", "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}, "summon": {"monsterId": "x"}})",
         "only raise / summon abilities summon"},
        {R"({"id": "a", "kind": "raise", "clip": "Cast01", "cooldownMs": 0, "aim": "corpse", "summon": {"monsterId": "x", "at": "self"}})",
         "a raise stands a corpse up"},
        {R"({"id": "a", "kind": "summon", "clip": "Cast01", "cooldownMs": 0, "aim": "self", "summon": {"monsterId": "x", "perWindowCap": 1}})",
         "per-window cap needs windowMs"},
        {R"({"id": "a", "kind": "buff", "clip": "Cast01", "cooldownMs": 0, "aim": "self"})", "needs its 'buff' block"},
        {R"({"id": "a", "kind": "call", "clip": "Cast01", "cooldownMs": 0, "aim": "hero"})", "does not fit a call"},
        {R"({"id": "a", "kind": "circle", "clip": "Attack02", "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}, "status": {"type": "slow", "ms": 1000, "value": 0}})",
         "value > 0"},
        {R"({"id": "a", "kind": "circle", "clip": "", "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}})", "empty clip"},
        {R"({"id": "a", "kind": "circle", "clip": "Attack02", "cooldownMs": -1, "aim": "self", "shape": {"radius": 1}})",
         "expected >= 0"},
        {R"({"id": "a", "kind": "circle", "clip": "Attack02", "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}, "rangeMin": 3, "rangeMax": 2})",
         "rangeMax > rangeMin"},
        {R"({"id": "a", "kind": "circle", "clip": "Attack02", "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}, "hpBelowPct": 0})",
         "0 < hpBelowPct <= 100"},
        {R"({"id": "a", "kind": "nova", "clip": "Attack02", "cooldownMs": 0, "aim": "self"})", "unknown value 'nova'"},
    };
    for (const Case& c : cases) {
      INFO(c.ability);
      DataLoadReport report;
      K2LoadDef(K2DefJson("fx_k2", std::string("[") + c.ability + "]"), report);
      CHECK_FALSE(report.Ok());
      CHECK(K2HasIssue(report.errors, "fx_k2", c.text));
    }
    // A duplicate id, and the monster fields.
    {
      DataLoadReport report;
      const char* dup = R"({"id": "a", "kind": "circle", "clip": "Attack02", "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}})";
      K2LoadDef(K2DefJson("fx_k2", StrCat("[", dup, ", ", dup, "]")), report);
      CHECK(K2HasIssue(report.errors, "fx_k2", "duplicate ability id 'a'"));
    }
    for (const char* cap : {R"("hitCapHeroMaxHpPct": 0)", R"("hitCapHeroMaxHpPct": 150)"}) {
      DataLoadReport report;
      K2LoadDef(K2DefJson("fx_k2", "[]", cap), report);
      CHECK(K2HasIssue(report.errors, "fx_k2", "hitCapHeroMaxHpPct"));
    }
    {
      DataLoadReport report;
      K2LoadDef(K2DefJson("fx_k2", "[]", R"("damageType": "holy")"), report);
      CHECK(K2HasIssue(report.errors, "fx_k2", "unknown value 'holy'"));
    }
  }

  TEST_CASE("K2 data: Finalize checks the boss floor, summoned ids, bossSkills and the clip beat") {
    const char* slam650 =
        R"([{"id": "slam", "kind": "circle", "clip": "Attack02", "windupMs": 650, "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}}])";
    {  // 650 ms is fair for a regular, not for a mini-boss (monsters.md 1.3: bosses >= 700 ms).
      DataLoadReport report;
      K2LoadDef(K2DefJson("fx_k2", slam650), report);
      CHECK(report.Ok());
      DataLoadReport bossReport;
      K2LoadDef(K2DefJson("fx_k2", slam650, "", true), bossReport);
      CHECK(K2HasIssue(bossReport.errors, "fx_k2.abilities.slam.windupMs", ">= 700 ms"));
    }
    {
      DataLoadReport report;
      K2LoadDef(K2DefJson("fx_k2", R"([{"id": "flock", "kind": "raise", "clip": "Cast01", "cooldownMs": 0, "aim": "corpse",
                                       "summon": {"monsterId": "fx_no_such_monster"}}])"),
                report);
      CHECK(K2HasIssue(report.errors, "fx_k2.abilities.flock.summon.monsterId", "unknown monster 'fx_no_such_monster'"));
    }
    {  // bossSkills name abilities once the monster has any; without abilities they stay inert (a warning, M9).
      DataLoadReport report;
      K2LoadDef(K2DefJson("fx_k2", slam650, R"("bossSkills": ["slam", "nova"])"), report);
      CHECK(K2HasIssue(report.errors, "fx_k2.bossSkills", "'nova' names no ability"));
      CHECK_FALSE(K2HasIssue(report.errors, "fx_k2.bossSkills", "'slam'"));
      DataLoadReport inert;
      K2LoadDef(K2DefJson("fx_k2", "[]", R"("bossSkills": ["nova"])"), inert);
      CHECK(inert.Ok());
      CHECK(K2HasIssue(inert.warnings, "fx_k2.bossSkills", "inert"));
    }
    {  // An ability timed by its clip warns when the manifest has no beat for it (the floor times it then).
      DataLoadReport report;
      K2LoadDef(K2DefJson("fx_k2", R"([{"id": "roar", "kind": "circle", "clip": "Roar", "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}},
                                      {"id": "cast", "kind": "circle", "clip": "Cast01", "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}}])"),
                report);
      CHECK(report.Ok());
      CHECK(K2HasIssue(report.warnings, "fx_k2.abilities.roar.clip", "no Contact / Release"));
      CHECK_FALSE(K2HasIssue(report.warnings, "fx_k2.abilities.cast.clip", "no Contact / Release"));  // Release 820
    }
    // The shipped data: a web bossSkills list without abilities stays inert (warned once per monster, never an error).
    DataLoadReport real;
    DataStore store;
    store.LoadAll(
        [](std::string_view name, std::string& out) {
          out = test::ReadFile(test::DataDir() + "/" + std::string(name));
          return !out.empty();
        },
        real);
    CHECK(real.Ok());
    for (const MonsterDef& m : store.Monsters().defs) {
      if (!m.bossSkills.empty() && m.abilities.empty()) CHECK(K2HasIssue(real.warnings, m.id + ".bossSkills", "inert"));
    }
  }

  TEST_CASE("K2 data: timing fields have upper bounds; a hazard's tick count saturates") {
    struct Case {
      const char* ability;
      const char* path;
      const char* text;
    };
    const Case cases[] = {
        {R"({"id": "a", "kind": "circle", "clip": "Attack02", "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}, "windupMs": 5001})",
         "windupMs", "windupMs <= 5000"},
        {R"({"id": "a", "kind": "circle", "clip": "Attack02", "cooldownMs": 600001, "aim": "self", "shape": {"radius": 1}})",
         "cooldownMs", "expected <= 600000"},
        {R"({"id": "a", "kind": "circle", "clip": "Attack02", "cooldownMs": 0, "initialCooldownMs": 1e12, "aim": "self", "shape": {"radius": 1}})",
         "initialCooldownMs", "expected <= 600000"},
        {R"({"id": "a", "kind": "circle", "clip": "Attack02", "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}, "hazard": {"durationMs": 3e11, "tickMs": 100, "tickDmgMul": 1}})",
         "hazard.durationMs", "expected <= 60000"},
    };
    for (const Case& c : cases) {
      INFO(c.ability);
      DataLoadReport report;
      K2LoadDef(K2DefJson("fx_k2", std::string("[") + c.ability + "]"), report);
      CHECK_FALSE(report.Ok());
      CHECK(K2HasIssue(report.errors, c.path, c.text));
    }
    {  // The bounds themselves load.
      DataLoadReport report;
      K2LoadDef(K2DefJson("fx_k2", R"([{"id": "a", "kind": "circle", "clip": "Attack02", "windupMs": 5000, "cooldownMs": 600000,
                                       "initialCooldownMs": 600000, "aim": "self", "shape": {"radius": 1},
                                       "hazard": {"durationMs": 60000, "tickMs": 100, "tickDmgMul": 1}}])"),
                report);
      CHECK(report.Ok());
    }
    // 60 s at the 100 ms floor is the most the loader lets through; TickCount stops at kHazardMaxTicks whatever it is
    // given (a hazard of 3e11 ms used to overflow its int32 counter).
    CHECK(HazardField::TickCount(60000, 100, true) == 600);
    CHECK(HazardField::TickCount(60000, 100, false) == 600);
    CHECK(kHazardMaxTicks >= 600);
    CHECK(HazardField::TickCount(3e11, 100, true) == kHazardMaxTicks);
    CHECK(HazardField::TickCount(3e11, 100, false) == kHazardMaxTicks);
    CHECK(HazardField::TickCount(1e300, 1, true) == kHazardMaxTicks);
  }

  TEST_CASE("K2 data: Finalize warns on an ability whose rangeMin lies past the swing reach (never picked)") {
    // The fixture's attackRange is 2; the Attack state holds within 2 x attackExitMul.
    const double reach = 2 * K2Data().Monsters().ai.attackExitMul;
    REQUIRE(reach == doctest::Approx(2.4));
    DataLoadReport report;
    K2LoadDef(K2DefJson("fx_k2", R"([{"id": "far", "kind": "circle", "clip": "Attack02", "windupMs": 900, "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}, "rangeMin": 3},
                                    {"id": "long", "kind": "circle", "clip": "Attack02", "windupMs": 900, "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}, "rangeMax": 5},
                                    {"id": "edge", "kind": "circle", "clip": "Attack02", "windupMs": 900, "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}, "rangeMin": 2.4, "rangeMax": 2.4001},
                                    {"id": "near", "kind": "circle", "clip": "Attack02", "windupMs": 900, "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}, "rangeMin": 1, "rangeMax": 2.4}])"),
              report);
    CHECK(report.Ok());  // warnings only: the data loads
    CHECK(K2HasIssue(report.warnings, "fx_k2.abilities.far.rangeMin", "never picked"));
    // 2F review: a rangeMax past the reach makes a ranged special (it also starts between swings, MonsterAbilities.h
    // AbilityIsRangedSpecial), so neither "long" nor "edge" (rangeMax a hair past 2.4) warns any more.
    CHECK_FALSE(K2HasIssue(report.warnings, "fx_k2.abilities.long", ""));
    CHECK_FALSE(K2HasIssue(report.warnings, "fx_k2.abilities.edge", ""));
    CHECK_FALSE(K2HasIssue(report.warnings, "fx_k2.abilities.near", ""));
    // The rule the warning states: with the hero at the edge of the Attack state (2.39 of 2.4 tiles) the swings go on
    // and an ability whose rangeMin lies past the reach never starts.
    MonsterAbilityDef far = K2Circle("far", 1.0, AbilityAim::Self, 900);
    far.rangeMin = 2.5;
    K2Rig rig;
    const MonsterDef def = K2Caster({far}, 5);
    rig.PlaceHero(Vec2(42.39, 40));
    const EntityId c = rig.Spawn(def, Vec2(40, 40));
    rig.h.rng.Get(RngStream::Combat).Script(std::vector<double>(40, 0.99));
    size_t swings = 0;
    for (int i = 0; i < 8 * 60; ++i) {
      rig.Step();
      for (const EvPlayAnim& a : K2Events<EvPlayAnim>(rig.h.events)) {
        if (a.entity != c) continue;
        CHECK(a.clip == "Attack01");
        ++swings;
      }
      rig.h.events.Clear();
    }
    CHECK(rig.M(c).state == MonsterState::Attack);
    CHECK(swings == 6);  // 0, 1.5, 3, 4.5, 6, 7.5 s
    CHECK(rig.M(c).abilityClockStarted);
  }

  // =================================================================================================================
  // Pure rules (MonsterAbilities.h, HazardField.h)
  // =================================================================================================================
  TEST_CASE("K2 shapes: inside, on the edge and outside, for a body of radius 0.35") {
    const double r = kAbilityBodyRadiusTiles;
    CHECK(r == 0.35);
    const Vec2 o(10, 10);
    const Vec2 east(1, 0);
    // Circle r 2: the body touches it up to 2 + 0.35 from the centre.
    const AbilityShapeDef circle = K2Shape(AbilityShapeKind::Circle, 2);
    CHECK(ShapeContains(circle, o, east, o));
    CHECK(ShapeContains(circle, o, east, Vec2(12, 10)));
    CHECK(ShapeContains(circle, o, east, Vec2(12.35, 10)));  // edge
    CHECK_FALSE(ShapeContains(circle, o, east, Vec2(12.36, 10)));
    CHECK(ShapeContains(circle, o, east, Vec2(10, 7.65)));
    CHECK_FALSE(ShapeContains(circle, o, east, Vec2(10, 7.64)));
    CHECK_FALSE(ShapeContains(circle, o, east, Vec2(12.35, 10), 0.0));  // a point body
    // Ring 1..3: the safe hole at its centre.
    const AbilityShapeDef ring = K2Shape(AbilityShapeKind::Ring, 3, 0, 0, 0, 1);
    CHECK_FALSE(ShapeContains(ring, o, east, o));
    CHECK_FALSE(ShapeContains(ring, o, east, Vec2(10.64, 10)));
    CHECK(ShapeContains(ring, o, east, Vec2(10.65, 10)));  // inner edge: 1 - 0.35
    CHECK(ShapeContains(ring, o, east, Vec2(12, 10)));
    CHECK(ShapeContains(ring, o, east, Vec2(13.35, 10)));  // outer edge
    CHECK_FALSE(ShapeContains(ring, o, east, Vec2(13.36, 10)));
    // Cone r 3, 90 degrees facing east: the opening, the edge rays, behind the apex.
    const AbilityShapeDef cone = K2Shape(AbilityShapeKind::Cone, 3, 90);
    CHECK(ShapeContains(cone, o, east, Vec2(12, 10)));
    CHECK(ShapeContains(cone, o, east, Vec2(13.35, 10)));
    CHECK_FALSE(ShapeContains(cone, o, east, Vec2(13.36, 10)));
    CHECK(ShapeContains(cone, o, east, Vec2(11.5, 11.4)));   // inside the 45 degree half-angle
    CHECK_FALSE(ShapeContains(cone, o, east, Vec2(10, 12)));  // 90 degrees off, 2 tiles out: 1.41 from the edge ray
    // On the 45 degree edge ray at distance 2, pushed out perpendicular by exactly the body radius: touches.
    const double s2 = std::sqrt(0.5);
    const Vec2 onRay = o + Vec2(s2, s2) * 2.0;
    const Vec2 outward(-s2, s2);
    CHECK(ShapeContains(cone, o, east, onRay + outward * 0.349));
    CHECK_FALSE(ShapeContains(cone, o, east, onRay + outward * 0.351));
    CHECK(ShapeContains(cone, o, east, Vec2(9.8, 10)));       // behind the apex, but the body covers it
    CHECK_FALSE(ShapeContains(cone, o, east, Vec2(9.5, 10)));
    CHECK(ShapeContains(K2Shape(AbilityShapeKind::Cone, 3, 360), o, east, Vec2(8, 10)));  // a full circle
    CHECK(ShapeContains(cone, o, Vec2(0, -2), Vec2(10, 8)));  // a non-unit direction is normalised
    // Line 4 x 1 from the origin, east: the sides at 0.5 + 0.35, the far end at 4 + 0.35, the start behind the origin.
    const AbilityShapeDef line = K2Shape(AbilityShapeKind::Line, 0, 0, 4, 1);
    CHECK(ShapeContains(line, o, east, Vec2(12, 10)));
    CHECK(ShapeContains(line, o, east, Vec2(12, 10.85)));
    CHECK_FALSE(ShapeContains(line, o, east, Vec2(12, 10.86)));
    CHECK(ShapeContains(line, o, east, Vec2(14.35, 10)));
    CHECK_FALSE(ShapeContains(line, o, east, Vec2(14.36, 10)));
    CHECK(ShapeContains(line, o, east, Vec2(9.65, 10)));
    CHECK_FALSE(ShapeContains(line, o, east, Vec2(9.64, 10)));
    CHECK_FALSE(ShapeContains(line, o, Vec2(0, 1), Vec2(12, 10)));  // the same point, the line turned south
    // Reach covers every shape.
    CHECK(ShapeReach(circle) == 2);
    CHECK(ShapeReach(line) == doctest::Approx(std::sqrt(16.25)));
  }

  TEST_CASE("K2 picker: priority, then data order; cooldown, range, life and phase conditions; kinds not run yet") {
    std::vector<MonsterAbilityDef> a = {K2Circle("low", 1), K2Circle("high", 1), K2Circle("high_too", 1)};
    a[0].priority = 1;
    a[1].priority = 5;
    a[2].priority = 5;
    std::vector<double> ready(3, 0.0);
    AbilityPickInput in;
    in.nowMs = 1000;
    in.heroDist = 1.5;
    in.readyAtMs = ready;
    CHECK(PickMonsterAbility(a, in) == 1);  // the highest priority, the first of a tie
    ready[1] = 1000.0000005;                // ready within the clock tolerance
    CHECK(PickMonsterAbility(a, in) == 1);
    ready[1] = 1001;                        // cooling down
    CHECK(PickMonsterAbility(a, in) == 2);
    ready[2] = 5000;
    CHECK(PickMonsterAbility(a, in) == 0);
    ready[0] = 5000;
    CHECK(PickMonsterAbility(a, in) == -1);  // the basic swing
    ready.assign(3, 0.0);
    // Range: [rangeMin, rangeMax].
    a[1].rangeMin = 2;
    CHECK(PickMonsterAbility(a, in) == 2);
    a[2].hasRangeMax = true;
    a[2].rangeMax = 1;
    CHECK(PickMonsterAbility(a, in) == 0);
    in.heroDist = 2;
    CHECK(PickMonsterAbility(a, in) == 1);
    // Life: strictly below hpBelowPct.
    a[1].hasHpBelowPct = true;
    a[1].hpBelowPct = 50;
    in.hpPct = 50;
    CHECK(PickMonsterAbility(a, in) == 0);
    in.hpPct = 49.9;
    CHECK(PickMonsterAbility(a, in) == 1);
    // Phase.
    a[1].phases = {1, 2};
    CHECK(PickMonsterAbility(a, in) == 0);
    in.phase = 2;
    CHECK(PickMonsterAbility(a, in) == 1);
    // A K3 raise / summon is picked only with bodies to stand up (AbilityPickInput::available, none here;
    // corpses_test.cpp "K3 picker"). K5 (2D): buff / call run since their package landed (they used to be "kinds not run
    // yet", never picked), so the buff is picked (data order); packs_test.cpp "K5 buff / call" covers their runtime.
    std::vector<MonsterAbilityDef> later = {K2Ability("raise", AbilityKind::Raise, "Cast01", AbilityAim::Corpse),
                                            K2Ability("summon", AbilityKind::Summon, "Cast01", AbilityAim::Self),
                                            K2Ability("buff", AbilityKind::Buff, "Cast01", AbilityAim::Self),
                                            K2Ability("call", AbilityKind::Call, "Cast01", AbilityAim::Self)};
    std::vector<double> ready4(4, 0.0);
    AbilityPickInput in2;
    in2.readyAtMs = ready4;
    CHECK(PickMonsterAbility(std::span<const MonsterAbilityDef>(later).first(2), in2) == -1);
    CHECK(PickMonsterAbility(later, in2) == 2);
    for (AbilityKind k : {AbilityKind::Circle, AbilityKind::Cone, AbilityKind::Line, AbilityKind::Ring, AbilityKind::Bolt,
                          AbilityKind::Hazard}) {
      CHECK(AbilityKindRuns(k));
    }
    // Missing ready slots (a definition that grew) never pick.
    AbilityPickInput none;
    CHECK(PickMonsterAbility(a, none) == -1);
  }

  TEST_CASE("K2 timing: the clip's beat is the contact; an authored windup re-times the clip; the fairness floor") {
    const AnimTimingTable& t = K2Data().Combat().anim;
    const AssetEntryDef* art = K2Data().Assets().FindByGameId(kArt);
    REQUIRE(art != nullptr);
    const AnimClipDef* attack02 = art->FindClip("Attack02");
    const AnimClipDef* cast01 = art->FindClip("Cast01");
    const AnimClipDef* cast02 = art->FindClip("Cast02");
    REQUIRE((attack02 != nullptr && cast01 != nullptr && cast02 != nullptr));
    // No windupMs: the Contact notify, at play rate 1, the clip's length.
    AbilityTiming a = ComputeAbilityTiming(K2Circle("x", 1), attack02, kAbilityMinWindupMs, t, AnimRig::Humanoid);
    CHECK(a.contactMs == 750);
    CHECK(a.playRate == 1);
    CHECK(a.durationMs == 1300);
    CHECK(a.fromClip);
    // A cast clip: its Release.
    a = ComputeAbilityTiming(K2Circle("x", 1, AbilityAim::Self, 0, "Cast01"), cast01, kAbilityMinWindupMs, t,
                             AnimRig::Humanoid);
    CHECK(a.contactMs == 820);
    // An authored windup: the clip plays slower so its beat lands on it.
    a = ComputeAbilityTiming(K2Circle("x", 1, AbilityAim::Self, 900), attack02, kAbilityMinWindupMs, t, AnimRig::Humanoid);
    CHECK(a.contactMs == 900);
    CHECK(a.playRate == doctest::Approx(750.0 / 900.0));
    CHECK(a.durationMs == doctest::Approx(1560));
    // A beat under the floor is held to it (600 ms; 700 ms for a boss), the clip slowed to match.
    a = ComputeAbilityTiming(K2Circle("x", 1, AbilityAim::Self, 0, "Cast02"), cast02, kAbilityMinWindupMs, t,
                             AnimRig::Humanoid);
    CHECK(a.contactMs == 600);
    CHECK(a.playRate == doctest::Approx(400.0 / 600.0));
    a = ComputeAbilityTiming(K2Circle("x", 1, AbilityAim::Self, 0, "Cast02"), cast02, kAbilityBossMinWindupMs, t,
                             AnimRig::Humanoid);
    CHECK(a.contactMs == 700);
    a = ComputeAbilityTiming(K2Circle("x", 1, AbilityAim::Self, 650), attack02, kAbilityBossMinWindupMs, t,
                             AnimRig::Humanoid);
    CHECK(a.contactMs == 700);
    // No clip (art not shipped): the floor, the rig preset's swing share for the length.
    a = ComputeAbilityTiming(K2Circle("x", 1), nullptr, kAbilityMinWindupMs, t, AnimRig::Humanoid);
    CHECK(a.contactMs == 600);
    CHECK_FALSE(a.fromClip);
    CHECK(a.playRate == 1);
    CHECK(a.durationMs >= 600);
    // The action an ability's clip plays.
    CHECK(AbilityAnimAction("Attack02") == AnimAction::Attack02);
    CHECK(AbilityAnimAction("Cast02") == AnimAction::Cast02);
    CHECK(AbilityAnimAction("Cast01") == AnimAction::Cast);
    CHECK(AbilityAnimAction("Roar") == AnimAction::Roar);
    CHECK(AbilityAnimAction("Lunge") == AnimAction::Attack);
    CHECK(AbilityAnimAction("Attack01") == AnimAction::Attack);
  }

  TEST_CASE("K2 hit cap: a percent of the hero's max life, scaled by the difficulty's damage multiplier") {
    const DifficultyTable& d = K2Data().Combat().difficulty;
    CHECK(MonsterHitCap(200, 35, d.Def(Difficulty::Normal).damageMul) == 70);
    CHECK(MonsterHitCap(200, 35, d.Def(Difficulty::Nightmare).damageMul) == 105);
    CHECK(MonsterHitCap(200, 35, d.Def(Difficulty::Hell).damageMul) == 140);
    CHECK(MonsterHitCap(213, 35, 1) == 74);  // floor(74.55)
    CHECK(MonsterHitCap(1, 35, 1) == 1);     // never 0
    CHECK(MonsterHitCap(200, 0, 1) == 0);    // no cap
    CHECK(ApplyMonsterHitCap(90, 200, 35, 1) == 70);
    CHECK(ApplyMonsterHitCap(50, 200, 35, 1) == 50);
    CHECK(ApplyMonsterHitCap(900, 200, 0, 2) == 900);
    CHECK(HazardTickDamage(20, 0.15) == 3);
    CHECK(HazardTickDamage(3, 0.1) == 1);  // at least 1
    CHECK(HazardTickDamage(20, 0) == 0);   // a status-only hazard
  }

  TEST_CASE("K2 HazardField: tick count, schedule, order and expiry") {
    CHECK(HazardField::TickCount(4000, 500, false) == 8);  // left behind: +500 .. +4000
    CHECK(HazardField::TickCount(4000, 500, true) == 8);   // pulse: +0 .. +3500
    CHECK(HazardField::TickCount(1000, 500, true) == 2);
    CHECK(HazardField::TickCount(1200, 500, true) == 3);
    CHECK(HazardField::TickCount(1200, 500, false) == 2);
    CHECK(HazardField::TickCount(400, 500, false) == 0);
    CHECK(HazardField::TickCount(0, 500, true) == 0);
    HazardField f;
    HazardEntry a;
    a.startMs = 1000;
    a.tickMs = 500;
    const uint32_t ida = f.Add(a, 1000);  // left behind: ticks at 1500, 2000
    HazardEntry b;
    b.startMs = 1500;
    b.tickMs = 500;
    b.pulseAtStart = true;
    const uint32_t idb = f.Add(b, 1000);  // pulse: ticks at 1500, 2000
    CHECK(idb == ida + 1);
    REQUIRE(f.Find(ida) != nullptr);
    CHECK(f.Find(ida)->untilMs == 2000);
    CHECK(f.Find(ida)->ticksTotal == 2);
    std::vector<HazardTick> due;
    f.CollectDue(1499, due);
    CHECK(due.empty());
    f.CollectDue(2000, due);  // two steps' worth at once: time order, then id
    REQUIRE(due.size() == 4);
    CHECK((due[0].hazard == ida && due[0].atMs == 1500 && due[0].index == 0));
    CHECK((due[1].hazard == idb && due[1].atMs == 1500));
    CHECK((due[2].hazard == ida && due[2].atMs == 2000 && due[2].index == 1));
    CHECK((due[3].hazard == idb && due[3].atMs == 2000));
    CHECK(f.Entries().size() == 2);  // read while applying
    f.DropExpired(1999);
    CHECK(f.Entries().size() == 2);
    f.DropExpired(2000);  // a: done at until; b: until 2500
    REQUIRE(f.Entries().size() == 1);
    CHECK(f.Entries()[0].id == idb);
    f.CollectDue(5000, due);
    CHECK(due.empty());
    f.DropExpired(2500);
    CHECK(f.Entries().empty());
    f.Clear();
    CHECK(f.Add(a, 1000) == idb + 1);  // ids are never reused
  }

  TEST_CASE("K2 aim: snapshot at windup start; cones and lines from the caster toward the hero; anchor; self") {
    AbilityAimInput in;
    in.casterPos = Vec2(10, 10);
    in.casterHeading = Vec2(0, 1);
    in.anchor = TilePos{5, 5};
    in.heroPos = Vec2(13, 14);
    MonsterAbilityCast c = ResolveAbilityCast(K2Circle("x", 1), in, 100, 900);
    CHECK((c.origin == Vec2(13, 14) && c.target == Vec2(13, 14) && c.startMs == 100 && c.contactMs == 900));
    c = ResolveAbilityCast(K2Circle("x", 1, AbilityAim::Self), in, 0, 600);
    CHECK(c.origin == Vec2(10, 10));
    c = ResolveAbilityCast(K2Circle("x", 1, AbilityAim::Anchor), in, 0, 600);
    CHECK(c.origin == Vec2(5, 5));
    MonsterAbilityDef cone = K2Ability("c", AbilityKind::Cone, "Attack02", AbilityAim::Hero);
    cone.hasShape = true;
    cone.shape = K2Shape(AbilityShapeKind::Cone, 3, 90);
    c = ResolveAbilityCast(cone, in, 0, 600);
    CHECK(c.origin == Vec2(10, 10));
    CHECK(c.dir.x == doctest::Approx(0.6));
    CHECK(c.dir.y == doctest::Approx(0.8));
    cone.aim = AbilityAim::Self;
    c = ResolveAbilityCast(cone, in, 0, 600);
    CHECK(c.dir == Vec2(0, 1));  // the caster's heading
    cone.aim = AbilityAim::Anchor;
    c = ResolveAbilityCast(cone, in, 0, 600);
    CHECK(c.origin == Vec2(5, 5));
    CHECK(c.dir.x == doctest::Approx(8 / std::sqrt(145.0)));
    in.heroPos = in.casterPos;  // the hero on the caster: the heading
    cone.aim = AbilityAim::Hero;
    c = ResolveAbilityCast(cone, in, 0, 600);
    CHECK(c.dir == Vec2(0, 1));
  }

  // =================================================================================================================
  // Runtime (CombatSystem / MonsterSystem)
  // =================================================================================================================
  TEST_CASE("K2 runtime: a due swing becomes the ability; contact time equals the clip's; telegraph, anim, hit") {
    K2Rig rig;
    REQUIRE(rig.ok);
    const MonsterDef def = K2Caster({K2Circle("slam", 1.5)}, 20);
    rig.PlaceHero(Vec2(41.5, 40));
    const EntityId c = rig.Spawn(def, Vec2(40, 40));
    rig.ScriptNoDodge();
    const double hp0 = rig.hero->Hp();
    rig.Step();
    const std::vector<EvPlayAnim> anims = K2Events<EvPlayAnim>(rig.h.events);
    REQUIRE(anims.size() == 1);
    const EvPlayAnim& a = anims[0];
    CHECK(a.entity == c);
    CHECK(a.action == AnimAction::Attack02);
    CHECK(a.clip == "Attack02");
    CHECK(a.contactMs == 750);   // the synthetic clip's Contact notify
    CHECK(a.windupMs == 750);    // the whole windup is the telegraph
    CHECK(a.durationMs == 1300);
    CHECK(a.playRate == 1);
    CHECK((a.hasFaceTarget && a.faceTarget == Vec2(41.5, 40)));
    const std::vector<EvTelegraph> tgs = K2Events<EvTelegraph>(rig.h.events);
    REQUIRE(tgs.size() == 1);
    CHECK(tgs[0].entity == c);
    CHECK(tgs[0].abilityId == "slam");
    CHECK(tgs[0].kind == AbilityKind::Circle);
    CHECK((tgs[0].shape.kind == AbilityShapeKind::Circle && tgs[0].shape.radius == 1.5));
    CHECK(tgs[0].origin == Vec2(41.5, 40));
    CHECK(tgs[0].startMs == a.startMs);
    CHECK(tgs[0].contactMs - tgs[0].startMs == 750);
    CHECK(tgs[0].style == TelegraphStyle::Normal);
    // The animation precedes its telegraph.
    size_t animAt = 0, tgAt = 0;
    for (size_t i = 0; i < rig.h.events.Size(); ++i) {
      if (std::holds_alternative<EvPlayAnim>(rig.h.events.Items()[i])) animAt = i;
      if (std::holds_alternative<EvTelegraph>(rig.h.events.Items()[i])) tgAt = i;
    }
    CHECK(animAt < tgAt);
    // Rooted, winding up, on cooldown, the swing clock stamped.
    MonsterInstance& m = rig.M(c);
    CHECK(m.pendingAbility == 0);
    CHECK(m.abilityRootedUntilMs == tgs[0].contactMs);
    CHECK(m.lastAttackMs == a.startMs);
    CHECK(rig.combat.IsWindingUp(c));
    // The snapshot shows the telegraph filling.
    rig.StepUntilMs(a.startMs + 375);
    Snapshot snap;
    rig.combat.FillSnapshot(snap);
    REQUIRE(snap.telegraphs.size() == 1);
    CHECK(snap.telegraphs[0].abilityId == "slam");
    CHECK(snap.telegraphs[0].progress == doctest::Approx(0.5).epsilon(0.03));
    // The hit lands at the contact beat and not one step before.
    const double expected = rig.ExpectedHit(c, 1.0, DamageType::Physical);
    rig.StepUntil([&] { return rig.hero->Hp() < hp0; }, 100);
    CHECK(rig.Now() >= tgs[0].contactMs - 1e-6);
    CHECK(rig.Now() < tgs[0].contactMs + kSimStepMs);
    const std::vector<EvHit> hits = HeroHits(rig.h.events, false);
    REQUIRE(hits.size() == 1);
    CHECK(hits[0].amount == expected);
    CHECK(hits[0].source == c);
    CHECK_FALSE(hits[0].melee);  // a ground strike, not a claw rake
    CHECK(rig.M(c).pendingAbility == -1);
    CHECK_FALSE(rig.combat.IsWindingUp(c));
    Snapshot after;
    rig.combat.FillSnapshot(after);
    CHECK(after.telegraphs.empty());
  }

  TEST_CASE("K2 runtime: priority and cooldown order; the basic swing in between; the initial cooldown") {
    MonsterAbilityDef big = K2Circle("big", 2, AbilityAim::Self, 600, "Attack02");
    big.priority = 2;
    big.cooldownMs = 5000;
    MonsterAbilityDef small = K2Circle("small", 2, AbilityAim::Self, 600, "Cast01");
    small.priority = 1;
    small.cooldownMs = 3000;
    K2Rig rig;
    const MonsterDef def = K2Caster({small, big}, 5);  // data order: small first; priority picks big
    rig.PlaceHero(Vec2(41.5, 40));
    const EntityId c = rig.Spawn(def, Vec2(40, 40));
    rig.h.rng.Get(RngStream::Combat).Script(std::vector<double>(40, 0.99));
    std::vector<std::string> clips;
    for (int i = 0; i < 6 * 60 + 30; ++i) {  // 6.5 s: swings at 0, 1.5, 3, 4.5, 6 s
      rig.Step();
      for (const EvPlayAnim& a : K2Events<EvPlayAnim>(rig.h.events)) {
        if (a.entity == c) clips.push_back(a.clip);
      }
      rig.h.events.Clear();
    }
    // 0 s big (cd to 5 s), 1.5 s small (cd to 4.5 s), 3 s basic, 4.5 s small, 6 s big.
    CHECK(clips == std::vector<std::string>{"Attack02", "Cast01", "Attack01", "Cast01", "Attack02"});
    // initialCooldownMs: the clock starts at the first swing of the fight.
    MonsterAbilityDef late = K2Circle("late", 2, AbilityAim::Self, 600);
    late.initialCooldownMs = 3000;
    late.cooldownMs = 100000;
    K2Rig rig2;
    const MonsterDef def2 = K2Caster({late}, 5);
    rig2.PlaceHero(Vec2(41.5, 40));
    const EntityId c2 = rig2.Spawn(def2, Vec2(40, 40));
    rig2.h.rng.Get(RngStream::Combat).Script(std::vector<double>(40, 0.99));
    std::vector<std::string> clips2;
    for (int i = 0; i < 5 * 60; ++i) {  // swings at 0, 1.5, 3, 4.5 s
      rig2.Step();
      for (const EvPlayAnim& a : K2Events<EvPlayAnim>(rig2.h.events)) {
        if (a.entity == c2) clips2.push_back(a.clip);
      }
      rig2.h.events.Clear();
    }
    CHECK(clips2 == std::vector<std::string>{"Attack01", "Attack01", "Attack02", "Attack01"});
    CHECK(rig2.M(c2).abilityClockStarted);
  }

  TEST_CASE("K2 runtime: hpBelowPct and range gate the pick; a leash restarts the ability clock") {
    MonsterAbilityDef low = K2Circle("low", 2, AbilityAim::Self, 600);
    low.hasHpBelowPct = true;
    low.hpBelowPct = 50;
    MonsterAbilityDef far = K2Circle("far", 2, AbilityAim::Self, 600, "Cast01");
    far.rangeMin = 1.8;
    K2Rig rig;
    const MonsterDef def = K2Caster({low, far}, 5);
    rig.PlaceHero(Vec2(41, 40));  // 1 tile: `far` out of its range, full life: no `low`
    const EntityId c = rig.Spawn(def, Vec2(40, 40));
    rig.h.rng.Get(RngStream::Combat).Script(std::vector<double>(20, 0.99));
    rig.Step();
    CHECK(K2Events<EvPlayAnim>(rig.h.events).at(0).clip == "Attack01");
    rig.M(c).hp = rig.M(c).maxHp * 0.4;
    rig.StepUntil([&] { return !K2Events<EvTelegraph>(rig.h.events).empty(); }, 200);
    REQUIRE_FALSE(K2Events<EvTelegraph>(rig.h.events).empty());
    CHECK(K2Events<EvTelegraph>(rig.h.events)[0].abilityId == "low");
    // A leash (M1) ends the fight: the clock restarts at the next fight's first swing.
    rig.StepUntil([&] { return rig.M(c).pendingAbility < 0; }, 100);
    MonsterInstance& m = rig.M(c);
    CHECK(m.abilityClockStarted);
    m.state = MonsterState::Chase;
    m.pos = Vec2(50, 40);  // 10 tiles from its anchor: past the 8-tile leash
    rig.PlaceHero(Vec2(60, 40));
    rig.Step();
    CHECK(rig.M(c).state == MonsterState::Returning);
    CHECK_FALSE(rig.M(c).abilityClockStarted);
  }

  TEST_CASE("K2 runtime: heroAtWindup snapshots the target; cones and lines keep the facing of windup start") {
    // Circle at the hero's windup position: stepping out dodges it, staying in takes it.
    for (bool stepOut : {false, true}) {
      K2Rig rig;
      const MonsterDef def = K2Caster({K2Circle("slam", 1.0, AbilityAim::HeroAtWindup, 900)});
      rig.PlaceHero(Vec2(41.5, 40));
      rig.Spawn(def, Vec2(40, 40));
      rig.ScriptNoDodge();
      rig.Step();
      REQUIRE(K2Events<EvTelegraph>(rig.h.events).size() == 1);
      CHECK(K2Events<EvTelegraph>(rig.h.events)[0].origin == Vec2(41.5, 40));
      if (stepOut) rig.PlaceHero(Vec2(42.9, 40));  // 1.4 tiles from the decal centre: clear of 1.0 + 0.35
      rig.Step(60);
      INFO("stepped out: " << stepOut);
      CHECK(HeroHits(rig.h.events, false).size() == (stepOut ? 0u : 1u));
    }
    // A cone toward the hero at windup start: the hero who walks round the caster leaves it, though a cone that
    // followed would now catch him; inside the original opening he is still hit.
    struct Move {
      Vec2 to;
      bool hit;
    };
    for (const Move mv : {Move{Vec2(40, 41.6), false}, Move{Vec2(42.2, 40.6), true}, Move{Vec2(38.6, 40), false}}) {
      K2Rig rig;
      MonsterAbilityDef cone = K2Ability("sweep", AbilityKind::Cone, "Attack02", AbilityAim::Hero, 700);
      cone.hasShape = true;
      cone.shape = K2Shape(AbilityShapeKind::Cone, 2.6, 120);
      const MonsterDef def = K2Caster({cone});
      rig.PlaceHero(Vec2(41.5, 40));
      rig.Spawn(def, Vec2(40, 40));
      rig.ScriptNoDodge();
      rig.Step();
      REQUIRE(K2Events<EvTelegraph>(rig.h.events).size() == 1);
      CHECK(K2Events<EvTelegraph>(rig.h.events)[0].dir == Vec2(1, 0));
      rig.PlaceHero(mv.to);
      rig.Step(50);
      INFO("moved to " << mv.to.x << "," << mv.to.y);
      CHECK(HeroHits(rig.h.events, false).size() == (mv.hit ? 1u : 0u));
    }
    // A line (heroAtWindup) from the caster: it keeps its windup direction.
    for (const Move mv : {Move{Vec2(43.5, 40.2), true}, Move{Vec2(41.5, 41.2), false}}) {
      K2Rig rig;
      MonsterAbilityDef line = K2Ability("lunge", AbilityKind::Line, "Attack02", AbilityAim::HeroAtWindup, 800);
      line.hasShape = true;
      line.shape = K2Shape(AbilityShapeKind::Line, 0, 0, 5, 1.2);
      const MonsterDef def = K2Caster({line});
      rig.PlaceHero(Vec2(41.5, 40));
      rig.Spawn(def, Vec2(40, 40));
      rig.ScriptNoDodge();
      rig.Step();
      rig.PlaceHero(mv.to);
      rig.Step(55);
      INFO("moved to " << mv.to.x << "," << mv.to.y);
      CHECK(HeroHits(rig.h.events, false).size() == (mv.hit ? 1u : 0u));
    }
  }

  TEST_CASE("K2 runtime: the caster is rooted through its windup, then chases") {
    K2Rig rig;
    const MonsterDef def = K2Caster({K2Circle("slam", 1.0, AbilityAim::HeroAtWindup, 900)});
    rig.PlaceHero(Vec2(41.5, 40));
    const EntityId c = rig.Spawn(def, Vec2(40, 40));
    rig.ScriptNoDodge();
    rig.Step();
    REQUIRE(rig.M(c).pendingAbility == 0);
    const double contact = rig.M(c).abilityRootedUntilMs;
    rig.PlaceHero(Vec2(45, 40));  // out of reach: a free monster would chase at once
    while (rig.Now() + kSimStepMs < contact - 1e-6) {
      rig.Step();
      CHECK(rig.M(c).pos == Vec2(40, 40));
      CHECK(rig.M(c).state == MonsterState::Attack);
    }
    rig.Step(3);
    CHECK(rig.M(c).pendingAbility == -1);
    CHECK(rig.M(c).state == MonsterState::Chase);
    CHECK(rig.M(c).pos.x > 40);
  }

  TEST_CASE("K2 runtime: an ability aborts when its caster dies or is stunned (S7), and on a cinematic (F2)") {
    MonsterAbilityDef slam = K2Circle("slam", 2.0, AbilityAim::Self, 900);
    slam.cooldownMs = 5000;
    slam.hazard.present = true;
    slam.hazard.durationMs = 2000;
    slam.hazard.tickMs = 500;
    slam.hazard.tickDmgMul = 0.5;
    const MonsterDef def = K2Caster({slam});
    {  // death
      K2Rig rig;
      rig.PlaceHero(Vec2(41, 40));
      const EntityId c = rig.Spawn(def, Vec2(40, 40));
      rig.Step();
      REQUIRE(rig.M(c).pendingAbility == 0);
      rig.Step(10);
      MonsterHitRequest kill;
      kill.monster = c;
      kill.amount = 100000;
      rig.combat.DamageMonster(kill);
      CHECK(CancelsOf(rig.h.events, c) == 1);
      rig.Step(80);
      CHECK(HeroHits(rig.h.events, false).empty());
      CHECK(HeroHits(rig.h.events, true).empty());
      CHECK(K2Events<EvHazard>(rig.h.events).empty());
      CHECK(rig.combat.Hazards().Entries().empty());
    }
    {  // stun
      K2Rig rig;
      rig.PlaceHero(Vec2(41, 40));
      const EntityId c = rig.Spawn(def, Vec2(40, 40));
      rig.Step();
      REQUIRE(rig.M(c).pendingAbility == 0);
      rig.Step(10);
      rig.status.Apply(c, StatusType::Stun, 1, 2000, kHeroEntityId, rig.Now());
      rig.Step(60);
      CHECK(CancelsOf(rig.h.events, c) == 1);
      CHECK(HeroHits(rig.h.events, false).empty());
      CHECK(K2Events<EvHazard>(rig.h.events).empty());
      CHECK(rig.M(c).pendingAbility == -1);
      CHECK(rig.M(c).abilityRootedUntilMs == 0);
    }
    {  // F2: a cinematic cancels the windup with the swings; the cooldown stays spent
      K2Rig rig;
      rig.PlaceHero(Vec2(41, 40));
      const EntityId c = rig.Spawn(def, Vec2(40, 40));
      rig.Step();
      REQUIRE(rig.M(c).pendingAbility == 0);
      rig.combat.OnFreezeBegin(true);
      CHECK(CancelsOf(rig.h.events, c) == 1);
      CHECK(rig.M(c).pendingAbility == -1);
      CHECK(rig.M(c).abilityRootedUntilMs == 0);
      CHECK(rig.M(c).abilityReadyAtMs.at(0) > rig.Now());
      rig.Step(80);
      CHECK(HeroHits(rig.h.events, false).empty());
    }
  }

  TEST_CASE("K2 runtime: hazards tick on the sim clock, the first pulse at contact for a hazard ability, then expire") {
    // A slam that leaves a burning brand for 4 s (8 ticks of 0.15 x damage, the first 500 ms after the slam).
    MonsterAbilityDef brand = K2Circle("brand", 1.8, AbilityAim::HeroAtWindup, 900, "Cast01");
    brand.dmgMul = 1.8;
    brand.cooldownMs = 100000;
    brand.hasDamageType = true;
    brand.damageType = DamageType::Fire;
    brand.hazard.present = true;
    brand.hazard.durationMs = 4000;
    brand.hazard.tickMs = 500;
    brand.hazard.tickDmgMul = 0.15;
    brand.hazard.status.present = true;
    brand.hazard.status.type = StatusType::Slow;
    brand.hazard.status.ms = 1000;
    brand.hazard.status.value = 30;
    K2Rig rig;
    const MonsterDef def = K2Caster({brand}, 20);
    rig.PlaceHero(Vec2(41.5, 40));
    const EntityId c = rig.Spawn(def, Vec2(40, 40));
    rig.ScriptNoDodge();
    rig.Step();
    const EvTelegraph tg = K2Events<EvTelegraph>(rig.h.events).at(0);
    rig.StepUntilMs(tg.contactMs);
    REQUIRE(K2Events<EvHazard>(rig.h.events).size() == 1);
    const EvHazard hz = K2Events<EvHazard>(rig.h.events)[0];
    CHECK(hz.source == c);
    CHECK(hz.abilityId == "brand");
    CHECK(hz.damageType == DamageType::Fire);
    CHECK(hz.origin == tg.origin);
    CHECK(hz.untilMs == hz.startMs + 4000);
    CHECK(hz.tickMs == 500);
    rig.Silence(c);  // no more swings; the brand keeps burning
    rig.StepUntilMs(hz.untilMs + 100);
    const std::vector<EvHit> ticks = HeroHits(rig.h.events, true);
    CHECK(ticks.size() == 8);
    for (const EvHit& t : ticks) {
      CHECK(t.amount == 3);  // floor(20 x 0.15)
      CHECK(t.element == DamageType::Fire);
      CHECK(t.source == c);
    }
    size_t slows = 0;
    for (const EvStatusApplied& s : K2Events<EvStatusApplied>(rig.h.events)) slows += s.type == StatusType::Slow ? 1 : 0;
    CHECK(slows == 8);
    CHECK(rig.combat.Hazards().Entries().empty());
    Snapshot snap;
    rig.combat.FillSnapshot(snap);
    CHECK(snap.hazards.empty());

    // A hazard ability (the ground is the attack): its first pulse lands with the contact, 2 pulses in 1 s, nothing
    // outside it.
    for (bool inside : {true, false}) {
      MonsterAbilityDef seep = K2Ability("seep", AbilityKind::Hazard, "Cast01", AbilityAim::Self, 700);
      seep.hasShape = true;
      seep.shape = K2Shape(AbilityShapeKind::Line, 0, 0, 8, 1);
      seep.cooldownMs = 100000;
      seep.hazard.present = true;
      seep.hazard.durationMs = 1000;
      seep.hazard.tickMs = 500;
      seep.hazard.tickDmgMul = 0.5;
      K2Rig r2;
      const MonsterDef d2 = K2Caster({seep}, 20);
      r2.PlaceHero(Vec2(41.5, 40));
      const EntityId c2 = r2.Spawn(d2, Vec2(40, 40));
      r2.Step();
      const EvTelegraph t2 = K2Events<EvTelegraph>(r2.h.events).at(0);
      if (!inside) r2.PlaceHero(Vec2(41.5, 41.5));
      r2.StepUntil([&] { return !K2Events<EvHazard>(r2.h.events).empty(); }, 100);
      REQUIRE(K2Events<EvHazard>(r2.h.events).size() == 1);
      const double placedAt = r2.Now();
      CHECK(placedAt >= t2.contactMs - 1e-6);
      CHECK(HeroHits(r2.h.events, false).empty());  // no contact hit: the pulse is the hit
      CHECK(HeroHits(r2.h.events, true).size() == (inside ? 1u : 0u));  // pulse 1, same step
      r2.Silence(c2);
      r2.StepUntilMs(placedAt + 1100);
      CHECK(HeroHits(r2.h.events, true).size() == (inside ? 2u : 0u));
      for (const EvHit& t : HeroHits(r2.h.events, true)) CHECK(t.amount == 10);
      CHECK(r2.combat.Hazards().Entries().empty());
    }
  }

  TEST_CASE("K2 runtime: a zone change drops the hazards") {
    MonsterAbilityDef slam = K2Circle("slam", 2.0, AbilityAim::Self, 600);
    slam.hazard.present = true;
    slam.hazard.durationMs = 5000;
    slam.hazard.tickMs = 500;
    slam.hazard.tickDmgMul = 0.1;
    K2Rig rig;
    const MonsterDef def = K2Caster({slam});
    rig.PlaceHero(Vec2(41, 40));
    rig.Spawn(def, Vec2(40, 40));
    rig.ScriptNoDodge();
    rig.Step(40);
    REQUIRE(rig.combat.Hazards().Entries().size() == 1);
    Snapshot snap;
    rig.combat.FillSnapshot(snap);
    REQUIRE(snap.hazards.size() == 1);
    CHECK(snap.hazards[0].abilityId == "slam");
    CHECK(snap.hazards[0].untilMs == snap.hazards[0].startMs + 5000);
    rig.combat.OnZoneExit();
    CHECK(rig.combat.Hazards().Entries().empty());
  }

  TEST_CASE("K2 runtime: damage types reach the hero's resistances (swings, abilities, hazard ticks)") {
    // A fire ability against 0 % and 50 % fire resistance.
    MonsterAbilityDef fire = K2Circle("fire", 2.0, AbilityAim::Self, 600);
    fire.hasDamageType = true;
    fire.damageType = DamageType::Fire;
    fire.dmgMul = 2;
    double dealt[2] = {0, 0};
    for (int resist = 0; resist < 2; ++resist) {
      K2Rig rig;
      if (resist == 1) rig.h.ctx.equip.Ref(Stat::FireResist) = 50;
      rig.hero->RecalcDerived(rig.h.ctx.equip);
      const MonsterDef def = K2Caster({fire}, 30);
      rig.PlaceHero(Vec2(41, 40));
      const EntityId c = rig.Spawn(def, Vec2(40, 40));
      rig.ScriptNoDodge();
      rig.Step();
      const double expected = rig.ExpectedHit(c, 2, DamageType::Fire);
      rig.Step(40);
      const std::vector<EvHit> hits = HeroHits(rig.h.events, false);
      REQUIRE(hits.size() == 1);
      CHECK(hits[0].element == DamageType::Fire);
      CHECK(hits[0].amount == expected);
      dealt[resist] = expected;
    }
    CHECK(dealt[1] < dealt[0]);
    // A basic swing of an ice monster: the def's damageType, resisted.
    for (int resist = 0; resist < 2; ++resist) {
      K2Rig rig;
      if (resist == 1) rig.h.ctx.equip.Ref(Stat::IceResist) = 40;
      rig.hero->RecalcDerived(rig.h.ctx.equip);
      MonsterDef def = K2Caster({}, 30);
      def.damageType = DamageType::Ice;
      rig.PlaceHero(Vec2(41, 40));
      const EntityId c = rig.Spawn(def, Vec2(40, 40));
      rig.ScriptNoDodge();
      const double expected = rig.ExpectedHit(c, 1, DamageType::Ice);
      const double physical = rig.ExpectedHit(c, 1, DamageType::Physical);
      if (resist == 1) CHECK(expected < physical);
      if (resist == 0) CHECK(expected == physical);
      rig.Step(40);
      const std::vector<EvHit> hits = HeroHits(rig.h.events, false);
      REQUIRE(hits.size() == 1);
      CHECK(hits[0].element == DamageType::Ice);
      CHECK(hits[0].melee);
      CHECK(hits[0].amount == expected);
    }
    // Hazard ticks: the fixed tick less the resistance, at least 1.
    MonsterAbilityDef burn = K2Circle("burn", 2.0, AbilityAim::Self, 600);
    burn.hasDamageType = true;
    burn.damageType = DamageType::Fire;
    burn.dmgMul = 0;
    burn.hazard.present = true;
    burn.hazard.durationMs = 1000;
    burn.hazard.tickMs = 500;
    burn.hazard.tickDmgMul = 1;
    for (const auto& [resist, tick] : std::vector<std::pair<double, double>>{{0, 30}, {50, 15}, {75, 7}}) {
      K2Rig rig;
      rig.h.ctx.equip.Ref(Stat::FireResist) = resist;
      rig.hero->RecalcDerived(rig.h.ctx.equip);
      const MonsterDef def = K2Caster({burn}, 30);
      rig.PlaceHero(Vec2(41, 40));
      const EntityId c = rig.Spawn(def, Vec2(40, 40));
      rig.ScriptNoDodge();
      REQUIRE(rig.StepUntil([&] { return !rig.combat.Hazards().Entries().empty(); }, 100));
      rig.Silence(c);
      rig.Step(80);
      const std::vector<EvHit> ticks = HeroHits(rig.h.events, true);
      INFO("fire resist " << resist);
      REQUIRE(ticks.size() == 2);
      CHECK(ticks[0].amount == tick);
    }
  }

  TEST_CASE("K2 runtime: the hit cap per difficulty (swings, abilities and hazard ticks)") {
    for (Difficulty diff : {Difficulty::Normal, Difficulty::Nightmare, Difficulty::Hell}) {
      for (bool capped : {true, false}) {
        K2Rig rig(5, diff);
        MonsterDef def = K2Caster({}, 400);  // a swing far above any cap
        if (capped) def.hitCapHeroMaxHpPct = 35;
        rig.PlaceHero(Vec2(41, 40));
        rig.Spawn(def, Vec2(40, 40));
        rig.ScriptNoDodge();
        const double maxHp = rig.hero->MaxHp();
        rig.Step(40);
        const std::vector<EvHit> hits = HeroHits(rig.h.events, false);
        REQUIRE(hits.size() == 1);
        const double cap = std::floor(maxHp * 35 * K2Data().Combat().difficulty.Def(diff).damageMul / 100.0);
        INFO(EnumName(diff) << " capped " << capped);
        if (capped) {
          CHECK(hits[0].amount == cap);
          CHECK(hits[0].targetHp == maxHp - cap);  // the hero survives it
        } else {
          CHECK(hits[0].amount > cap);
        }
      }
    }
    // An ability and its hazard ticks under the same cap.
    MonsterAbilityDef slam = K2Circle("slam", 2.0, AbilityAim::Self, 600);
    slam.dmgMul = 3;
    slam.hazard.present = true;
    slam.hazard.durationMs = 1000;
    slam.hazard.tickMs = 500;
    slam.hazard.tickDmgMul = 5;
    K2Rig rig;
    MonsterDef def = K2Caster({slam}, 400);
    def.hitCapHeroMaxHpPct = 20;
    rig.PlaceHero(Vec2(41, 40));
    const EntityId c = rig.Spawn(def, Vec2(40, 40));
    rig.ScriptNoDodge();
    REQUIRE(rig.StepUntil([&] { return !rig.combat.Hazards().Entries().empty(); }, 100));
    rig.Silence(c);
    rig.Step(80);
    const double cap = std::floor(rig.hero->MaxHp() * 20 / 100.0);
    const std::vector<EvHit> hits = HeroHits(rig.h.events, false);
    REQUIRE(hits.size() == 1);
    CHECK(hits[0].amount == cap);
    const std::vector<EvHit> ticks = HeroHits(rig.h.events, true);
    REQUIRE(ticks.size() == 2);
    for (const EvHit& t : ticks) CHECK(t.amount == cap);
  }

  TEST_CASE("K2 runtime: a bolt flies at contact, locked on the hero, and hits with its ability's multiplier") {
    MonsterAbilityDef bolt = K2Ability("toll", AbilityKind::Bolt, "Cast01", AbilityAim::Hero);
    bolt.dmgMul = 2;
    bolt.status.present = true;
    bolt.status.type = StatusType::Slow;
    bolt.status.ms = 2000;
    bolt.status.value = 20;
    K2Rig rig;
    const MonsterDef def = K2Caster({bolt}, 20);
    rig.PlaceHero(Vec2(41.5, 40));
    const EntityId c = rig.Spawn(def, Vec2(40, 40));
    rig.ScriptNoDodge();
    rig.Step();
    CHECK(K2Events<EvTelegraph>(rig.h.events).empty());  // a bolt draws no ground decal
    const EvPlayAnim a = K2Events<EvPlayAnim>(rig.h.events).at(0);
    CHECK(a.action == AnimAction::Cast);
    CHECK(a.contactMs == 820);  // the synthetic Cast01 Release
    rig.StepUntil([&] { return !K2Events<EvProjectileLaunched>(rig.h.events).empty(); }, 100);
    REQUIRE(rig.projectiles.Projectiles().size() == 1);
    CHECK(rig.projectiles.Projectiles()[0].spec.payload == 1);
    CHECK(rig.projectiles.Projectiles()[0].spec.target == kHeroEntityId);
    const double expected = rig.ExpectedHit(c, 2, DamageType::Physical);
    rig.PlaceHero(Vec2(42.5, 40.5));  // locked on: it follows
    rig.StepUntil([&] { return !HeroHits(rig.h.events, false).empty(); }, 60);
    REQUIRE(HeroHits(rig.h.events, false).size() == 1);
    CHECK(HeroHits(rig.h.events, false)[0].amount == expected);
    bool slowed = false;
    for (const EvStatusApplied& s : K2Events<EvStatusApplied>(rig.h.events)) slowed = slowed || s.type == StatusType::Slow;
    CHECK(slowed);
    CHECK_FALSE(K2Events<EvHit>(rig.h.events).back().melee);
  }

  TEST_CASE("K2 runtime: hitsMonsters hurts the other monsters in the shape (x monsterDmgMul), never the caster") {
    MonsterAbilityDef nova = K2Circle("nova", 2.0, AbilityAim::Self, 600);
    nova.hitsMonsters = true;
    nova.monsterDmgMul = 2;
    K2Rig rig;
    const MonsterDef def = K2Caster({nova}, 20);
    const MonsterDef bystander = K2Caster({}, 5, "fx_k2_bystander");
    rig.PlaceHero(Vec2(41, 40));
    const EntityId c = rig.Spawn(def, Vec2(40, 40));
    const EntityId near = rig.Spawn(bystander, Vec2(40, 41.5), MonsterState::Idle);
    const EntityId far = rig.Spawn(bystander, Vec2(40, 43), MonsterState::Idle);
    rig.h.rng.Get(RngStream::Combat).Script(std::vector<double>(10, 0.99));
    rig.Step(40);
    const double casterHp = rig.M(c).hp;
    CHECK(casterHp == rig.M(c).maxHp);
    CHECK(rig.M(near).hp < rig.M(near).maxHp);
    CHECK(rig.M(far).hp == rig.M(far).maxHp);
    bool sawHit = false;
    for (const EvHit& e : K2Events<EvHit>(rig.h.events)) {
      if (e.target == near) {
        sawHit = true;
        CHECK(e.source == c);
      }
    }
    CHECK(sawHit);
  }

  TEST_CASE("K2 runtime: telegraph style and windup floor follow the caster's role (bosses >= 700 ms)") {
    MonsterAbilityDef quick = K2Circle("quick", 1.5, AbilityAim::Self, 0, "Cast02");  // a 400 ms beat
    for (const auto& [role, elite, style, contact] :
         std::vector<std::tuple<MonsterRole, bool, TelegraphStyle, double>>{
             {MonsterRole::Regular, false, TelegraphStyle::Normal, 600},
             {MonsterRole::Regular, true, TelegraphStyle::Elite, 600},
             {MonsterRole::HuntLeader, false, TelegraphStyle::Elite, 600},
             {MonsterRole::ZoneMiniBoss, false, TelegraphStyle::Boss, 700},
             {MonsterRole::StoryBoss, false, TelegraphStyle::Boss, 700}}) {
      K2Rig rig;
      MonsterDef def = K2Caster({quick});
      def.elite = elite;
      rig.PlaceHero(Vec2(41, 40));
      rig.Spawn(def, Vec2(40, 40), MonsterState::Attack, role);
      rig.Step();
      INFO(EnumName(role) << " elite " << elite);
      const std::vector<EvTelegraph> tg = K2Events<EvTelegraph>(rig.h.events);
      REQUIRE(tg.size() == 1);
      CHECK(tg[0].style == style);
      CHECK(tg[0].contactMs - tg[0].startMs == contact);
      CHECK(K2Events<EvPlayAnim>(rig.h.events).at(0).playRate == doctest::Approx(400.0 / contact));
    }
  }

  // Since K3 a raise runs, but only with a corpse in reach (none here), so the due swing stays a basic one.
  TEST_CASE("K2 runtime: abilities of kinds that land later never start; the monster swings") {
    K2Rig rig;
    MonsterAbilityDef raise = K2Ability("raise", AbilityKind::Raise, "Cast01", AbilityAim::Corpse);
    raise.summon.present = true;
    raise.summon.monsterId = "fx_grunt";
    const MonsterDef def = K2Caster({raise});
    rig.PlaceHero(Vec2(41, 40));
    const EntityId c = rig.Spawn(def, Vec2(40, 40));
    rig.ScriptNoDodge();
    rig.Step();
    CHECK(K2Events<EvPlayAnim>(rig.h.events).at(0).clip == "Attack01");
    CHECK(rig.M(c).pendingAbility == -1);
  }

  // =================================================================================================================
  // Review fixes (2A review): one action at a time, per-difficulty caps on abilities and ticks, damage-type fallbacks,
  // shape-bound monster hits, a dead hero at contact, affix telegraphs, stuns mid-windup, pet taunts, Death Save.
  // =================================================================================================================
  TEST_CASE("K2 runtime: one action at a time: a windup longer than the attack interval starts nothing else") {
    MonsterAbilityDef slow = K2Circle("slow", 1.5, AbilityAim::Self, 2400);  // the fixture swings every 1500 ms
    slow.cooldownMs = 100000;
    K2Rig rig;
    const MonsterDef def = K2Caster({slow}, 5);
    rig.PlaceHero(Vec2(41, 40));
    const EntityId c = rig.Spawn(def, Vec2(40, 40));
    rig.h.rng.Get(RngStream::Combat).Script(std::vector<double>(20, 0.99));
    rig.Step();
    REQUIRE(rig.M(c).pendingAbility == 0);
    const double start = rig.Now();
    const double contact = rig.M(c).abilityRootedUntilMs;
    CHECK(contact - K2Events<EvPlayAnim>(rig.h.events).at(0).startMs == 2400);
    std::vector<EvPlayAnim> anims = K2Events<EvPlayAnim>(rig.h.events);
    size_t heroHitsBefore = 0;
    rig.h.events.Clear();
    while (rig.Now() + kSimStepMs < contact - 1e-6) {
      rig.Step();
      for (const EvPlayAnim& a : K2Events<EvPlayAnim>(rig.h.events)) {
        if (a.entity == c) anims.push_back(a);
      }
      heroHitsBefore += HeroHits(rig.h.events, false).size();
      rig.h.events.Clear();
      CHECK(rig.M(c).pendingAbility == 0);
      CHECK(rig.combat.IsWindingUp(c));
    }
    CHECK(rig.Now() - start > 1500 + kSimStepMs);  // the attack interval ran out mid-windup
    REQUIRE(anims.size() == 1);  // the ability's own clip, nothing started on top of it
    CHECK(anims[0].clip == "Attack02");
    CHECK(heroHitsBefore == 0);
    // Contact: the ability lands, then the overdue basic swing starts (the interval counts from the windup start).
    rig.Step(2);
    CHECK(HeroHits(rig.h.events, false).size() == 1);
    CHECK(rig.M(c).pendingAbility == -1);
    const std::vector<EvPlayAnim> next = K2Events<EvPlayAnim>(rig.h.events);
    REQUIRE(next.size() == 1);
    CHECK(next[0].clip == "Attack01");
    CHECK(next[0].startMs >= contact - 1e-6);
  }

  TEST_CASE("K2 runtime: the hit cap on Nightmare and Hell for ability hits and hazard ticks") {
    MonsterAbilityDef slam = K2Circle("slam", 2.0, AbilityAim::Self, 600);
    slam.dmgMul = 3;
    slam.hazard.present = true;
    slam.hazard.durationMs = 1000;
    slam.hazard.tickMs = 500;
    slam.hazard.tickDmgMul = 5;
    MonsterAbilityDef seep = K2Ability("seep", AbilityKind::Hazard, "Cast01", AbilityAim::Self, 700);
    seep.hasShape = true;
    seep.shape = K2Shape(AbilityShapeKind::Circle, 2.0);
    seep.hazard.present = true;
    seep.hazard.durationMs = 500;
    seep.hazard.tickMs = 500;
    seep.hazard.tickDmgMul = 5;
    for (Difficulty diff : {Difficulty::Normal, Difficulty::Nightmare, Difficulty::Hell}) {
      const double mul = K2Data().Combat().difficulty.Def(diff).damageMul;
      INFO(EnumName(diff));
      {  // a strike and the two ticks of the hazard it leaves
        K2Rig rig(5, diff);
        MonsterDef def = K2Caster({slam}, 400);
        def.hitCapHeroMaxHpPct = 10;
        rig.PlaceHero(Vec2(41, 40));
        const EntityId c = rig.Spawn(def, Vec2(40, 40));
        rig.ScriptNoDodge();
        REQUIRE(rig.StepUntil([&] { return !rig.combat.Hazards().Entries().empty(); }, 100));
        rig.Silence(c);
        rig.Step(80);
        const double cap = std::floor(rig.hero->MaxHp() * 10 * mul / 100.0);
        if (diff != Difficulty::Normal) CHECK(cap > std::floor(rig.hero->MaxHp() * 10 / 100.0));  // the scale matters
        const std::vector<EvHit> hits = HeroHits(rig.h.events, false);
        REQUIRE(hits.size() == 1);
        CHECK(hits[0].amount == cap);
        const std::vector<EvHit> ticks = HeroHits(rig.h.events, true);
        REQUIRE(ticks.size() == 2);
        for (const EvHit& t : ticks) CHECK(t.amount == cap);
      }
      {  // a hazard ability's pulse
        K2Rig rig(5, diff);
        MonsterDef def = K2Caster({seep}, 400);
        def.hitCapHeroMaxHpPct = 10;
        rig.PlaceHero(Vec2(41, 40));
        rig.Spawn(def, Vec2(40, 40));
        REQUIRE(rig.StepUntil([&] { return !HeroHits(rig.h.events, true).empty(); }, 100));
        CHECK(HeroHits(rig.h.events, true)[0].amount == std::floor(rig.hero->MaxHp() * 10 * mul / 100.0));
      }
    }
  }

  TEST_CASE("K2 runtime: an ability and its hazard without their own damageType take the monster's") {
    MonsterAbilityDef frost = K2Circle("frost", 2.0, AbilityAim::Self, 600);
    frost.dmgMul = 2;
    frost.hitsMonsters = true;
    frost.hazard.present = true;
    frost.hazard.durationMs = 1000;
    frost.hazard.tickMs = 500;
    frost.hazard.tickDmgMul = 1;
    MonsterAbilityDef shard = K2Ability("shard", AbilityKind::Bolt, "Cast01", AbilityAim::Hero);
    shard.dmgMul = 1.5;
    for (double resist : {0.0, 40.0}) {
      INFO("ice resist " << resist);
      {
        K2Rig rig;
        rig.h.ctx.equip.Ref(Stat::IceResist) = resist;
        rig.hero->RecalcDerived(rig.h.ctx.equip);
        MonsterDef def = K2Caster({frost}, 30);
        def.damageType = DamageType::Ice;
        const MonsterDef bystander = K2Caster({}, 5, "fx_k2_bystander");
        rig.PlaceHero(Vec2(41, 40));
        const EntityId c = rig.Spawn(def, Vec2(40, 40));
        const EntityId by = rig.Spawn(bystander, Vec2(39, 40), MonsterState::Idle);
        rig.Silence(by);
        rig.ScriptNoDodge(2);
        const double expected = rig.ExpectedHit(c, 2, DamageType::Ice);
        if (resist > 0) CHECK(expected < rig.ExpectedHit(c, 2, DamageType::Physical));
        REQUIRE(rig.StepUntil([&] { return !rig.combat.Hazards().Entries().empty(); }, 100));
        rig.Silence(c);
        rig.Step(80);
        const std::vector<EvHit> hits = HeroHits(rig.h.events, false);
        REQUIRE(hits.size() == 1);
        CHECK(hits[0].element == DamageType::Ice);
        CHECK(hits[0].amount == expected);
        bool sawBystander = false;
        for (const EvHit& e : K2Events<EvHit>(rig.h.events)) {
          if (e.target != by || e.dodged) continue;
          sawBystander = true;
          CHECK(e.element == DamageType::Ice);
        }
        CHECK(sawBystander);
        const std::vector<EvHazard> hz = K2Events<EvHazard>(rig.h.events);
        REQUIRE(hz.size() == 1);
        CHECK(hz[0].damageType == DamageType::Ice);
        const std::vector<EvHit> ticks = HeroHits(rig.h.events, true);
        REQUIRE(ticks.size() == 2);
        for (const EvHit& t : ticks) {
          CHECK(t.element == DamageType::Ice);
          CHECK(t.amount == (resist > 0 ? 18 : 30));  // floor(30 x 1) less 40 %
        }
      }
      {  // a bolt
        K2Rig rig;
        rig.h.ctx.equip.Ref(Stat::IceResist) = resist;
        rig.hero->RecalcDerived(rig.h.ctx.equip);
        MonsterDef def = K2Caster({shard}, 30);
        def.damageType = DamageType::Ice;
        rig.PlaceHero(Vec2(41.5, 40));
        const EntityId c = rig.Spawn(def, Vec2(40, 40));
        rig.ScriptNoDodge();
        const double expected = rig.ExpectedHit(c, 1.5, DamageType::Ice);
        REQUIRE(rig.StepUntil([&] { return !HeroHits(rig.h.events, false).empty(); }, 150));
        const EvHit hit = HeroHits(rig.h.events, false)[0];
        CHECK(hit.element == DamageType::Ice);
        CHECK(hit.amount == expected);
      }
    }
  }

  TEST_CASE("K2 runtime: hitsMonsters follows the shape (cone strike, hazard ticks) and deals dmgMul x monsterDmgMul") {
    const MonsterDef bystander = K2Caster({}, 5, "fx_k2_bystander");
    {  // A cone toward the hero: a monster in front is hit for exactly the formula's damage; one behind the caster,
       // within the cone's reach but outside its opening, is untouched.
      MonsterAbilityDef sweep = K2Ability("sweep", AbilityKind::Cone, "Attack02", AbilityAim::HeroAtWindup, 700);
      sweep.hasShape = true;
      sweep.shape = K2Shape(AbilityShapeKind::Cone, 2.6, 90);
      sweep.dmgMul = 1.5;
      sweep.hitsMonsters = true;
      sweep.monsterDmgMul = 3;
      K2Rig rig;
      const MonsterDef def = K2Caster({sweep}, 20);
      rig.PlaceHero(Vec2(41.5, 40));
      const EntityId c = rig.Spawn(def, Vec2(40, 40));
      const EntityId front = rig.Spawn(bystander, Vec2(42.2, 39.6), MonsterState::Idle);
      const EntityId behind = rig.Spawn(bystander, Vec2(38.5, 40), MonsterState::Idle);
      rig.Silence(front);
      rig.Silence(behind);
      CHECK(Dist(rig.M(behind).pos, Vec2(40, 40)) < ShapeReach(sweep.shape) + kAbilityBodyRadiusTiles);
      rig.ScriptNoDodge(3);
      rig.Step();
      REQUIRE(rig.M(c).pendingAbility == 0);
      // What the strike deals the monster in front: dmgMul x monsterDmgMul through the formula, "no dodge, no crit".
      Rng rng(1);
      rng.Script({0.99, 0.99});
      SkillHitInput in;
      in.basicType = DamageType::Physical;
      in.basicMultiplier = 1.5 * 3;
      const ClassTables& ct = rig.h.ctx.data.Classes();
      const double expected = CalculateDamage(DamageRules{&ct.formulas, &ct.buffCaps, &ct.skillRules},
                                              CombatSystem::MonsterCombatant(rig.M(c)),
                                              CombatSystem::MonsterCombatant(rig.M(front)), in, false, rng)
                                  .damage;
      rig.Step(50);
      CHECK(rig.M(front).maxHp - rig.M(front).hp == expected);
      CHECK(rig.M(behind).hp == rig.M(behind).maxHp);
      CHECK(rig.M(c).hp == rig.M(c).maxHp);
    }
    {  // A hazard ability (a line from the caster toward the hero) ticks on the monsters inside the line only.
      MonsterAbilityDef seep = K2Ability("seep", AbilityKind::Hazard, "Cast01", AbilityAim::Self, 700);
      seep.hasShape = true;
      seep.shape = K2Shape(AbilityShapeKind::Line, 0, 0, 6, 1);
      seep.hitsMonsters = true;
      seep.monsterDmgMul = 2;
      seep.cooldownMs = 100000;
      seep.hazard.present = true;
      seep.hazard.durationMs = 1000;
      seep.hazard.tickMs = 500;
      seep.hazard.tickDmgMul = 0.5;
      K2Rig rig;
      const MonsterDef def = K2Caster({seep}, 20);
      rig.PlaceHero(Vec2(41.5, 40));
      const EntityId c = rig.Spawn(def, Vec2(40, 40));
      const EntityId inside = rig.Spawn(bystander, Vec2(44, 40.2), MonsterState::Idle);
      const EntityId beside = rig.Spawn(bystander, Vec2(41, 41.6), MonsterState::Idle);
      rig.Silence(inside);
      rig.Silence(beside);
      CHECK(Dist(rig.M(beside).pos, Vec2(40, 40)) < ShapeReach(seep.shape));
      REQUIRE(rig.StepUntil([&] { return !rig.combat.Hazards().Entries().empty(); }, 100));
      rig.Silence(c);
      rig.Step(80);
      CHECK(rig.combat.Hazards().Entries().empty());
      CHECK(rig.M(inside).maxHp - rig.M(inside).hp == 2 * HazardTickDamage(20, 0.5 * 2));  // two ticks of 20
      CHECK(rig.M(beside).hp == rig.M(beside).maxHp);
      CHECK(rig.M(c).hp == rig.M(c).maxHp);
    }
  }

  TEST_CASE("K2 runtime: a hero who dies during the windup takes nothing at contact and dies once") {
    MonsterAbilityDef slam = K2Circle("slam", 2.0, AbilityAim::Self, 900);
    slam.hazard.present = true;
    slam.hazard.durationMs = 1000;
    slam.hazard.tickMs = 500;
    slam.hazard.tickDmgMul = 1;
    K2Rig rig;
    const MonsterDef def = K2Caster({slam}, 20);
    rig.PlaceHero(Vec2(41, 40));
    const EntityId c = rig.Spawn(def, Vec2(40, 40));
    rig.h.rng.Get(RngStream::Combat).Script(std::vector<double>(10, 0.99));
    rig.Step();
    REQUIRE(rig.M(c).pendingAbility == 0);
    const double contact = rig.M(c).abilityRootedUntilMs;
    rig.Step(10);
    rig.hero->SetHp(0);
    rig.combat.KillHero(HeroDeathCause::Other);
    REQUIRE(rig.hero->Life() == HeroLife::Dying);
    REQUIRE(K2Events<EvHeroDied>(rig.h.events).size() == 1);
    rig.h.events.Clear();
    rig.StepUntilMs(contact + 50);  // past the contact, before the respawn
    CHECK(rig.hero->Life() == HeroLife::Dying);
    CHECK(rig.M(c).pendingAbility == -1);
    CHECK(K2Events<EvHit>(rig.h.events).empty());
    CHECK(K2Events<EvHeroDied>(rig.h.events).empty());
    CHECK(K2Events<EvHazard>(rig.h.events).empty());
  }

  TEST_CASE("K2 runtime: an affix makes a regular monster's telegraph elite") {
    MonsterInstance m;
    m.role = MonsterRole::Regular;
    CHECK(TelegraphStyleOf(m) == TelegraphStyle::Normal);
    m.affixes.push_back(MonsterAffix{});
    CHECK(TelegraphStyleOf(m) == TelegraphStyle::Elite);
    m.role = MonsterRole::ZoneMiniBoss;
    CHECK(TelegraphStyleOf(m) == TelegraphStyle::Boss);  // the boss style wins over affixes
    K2Rig rig;
    const MonsterDef def = K2Caster({K2Circle("quick", 1.5, AbilityAim::Self, 0, "Cast02")});
    REQUIRE_FALSE(def.elite);
    rig.PlaceHero(Vec2(41, 40));
    const EntityId c = rig.Spawn(def, Vec2(40, 40));
    rig.M(c).affixes.push_back(MonsterAffix{});
    rig.Step();
    const std::vector<EvTelegraph> tg = K2Events<EvTelegraph>(rig.h.events);
    REQUIRE(tg.size() == 1);
    CHECK(tg[0].style == TelegraphStyle::Elite);
    CHECK(tg[0].contactMs - tg[0].startMs == 600);  // the elite floor is the regular one
    Snapshot snap;
    rig.combat.FillSnapshot(snap);
    REQUIRE(snap.telegraphs.size() == 1);
    CHECK(snap.telegraphs[0].style == TelegraphStyle::Elite);
  }

  TEST_CASE("K2 runtime: a stun or freeze mid-windup aborts the ability at once, even when it ends before contact") {
    for (StatusType st : {StatusType::Stun, StatusType::Freeze}) {
      INFO(EnumName(st));
      MonsterAbilityDef slam = K2Circle("slam", 2.0, AbilityAim::Self, 900);
      slam.cooldownMs = 5000;
      slam.hazard.present = true;
      slam.hazard.durationMs = 1000;
      slam.hazard.tickMs = 500;
      slam.hazard.tickDmgMul = 1;
      K2Rig rig;
      const MonsterDef def = K2Caster({slam}, 20);
      rig.PlaceHero(Vec2(41, 40));
      const EntityId c = rig.Spawn(def, Vec2(40, 40));
      rig.h.rng.Get(RngStream::Combat).Script(std::vector<double>(10, 0.99));
      rig.Step(2);
      REQUIRE(rig.M(c).pendingAbility == 0);
      const double contact = rig.M(c).abilityRootedUntilMs;
      rig.status.Apply(c, st, st == StatusType::Freeze ? 100 : 1, 300, kHeroEntityId, rig.Now());  // ends at +300 ms
      rig.Step();
      CHECK(CancelsOf(rig.h.events, c) == 1);
      CHECK(rig.M(c).pendingAbility == -1);
      CHECK(rig.M(c).abilityRootedUntilMs == 0);
      CHECK_FALSE(rig.combat.IsWindingUp(c));
      Snapshot snap;
      rig.combat.FillSnapshot(snap);
      CHECK(snap.telegraphs.empty());
      CHECK(rig.M(c).abilityReadyAtMs.at(0) > rig.Now());  // the cooldown stays spent
      rig.StepUntilMs(contact + 200);  // the stun is long over at contact, and the next swing is not due yet
      CHECK_FALSE(rig.status.IsImmobilized(c));
      CHECK(CancelsOf(rig.h.events, c) == 1);
      CHECK(HeroHits(rig.h.events, false).empty());
      CHECK(HeroHits(rig.h.events, true).empty());
      CHECK(K2Events<EvHazard>(rig.h.events).empty());
    }
  }

  TEST_CASE("K2 runtime: a pet's taunt draws the due swing to the beast; the taunted monster starts no ability") {
    K2PetRig rig;
    REQUIRE(rig.petAdded);
    REQUIRE(rig.companion.State().present);
    MonsterAbilityDef slam = K2Circle("slam", 1.5, AbilityAim::Self, 600);
    slam.cooldownMs = 1000;
    const MonsterDef def = K2Caster({slam}, 5);
    rig.PlaceHero(Vec2(41, 40));
    rig.PlacePet(Vec2(39, 41));
    const EntityId c = rig.Spawn(def, Vec2(40, 40));
    const double until = rig.Now() + 2500;
    rig.companion.MutableStateForTesting().taunts.push_back({c, until});
    CHECK(rig.companion.TauntHolds(c));
    std::vector<EvPlayAnim> anims;
    while (rig.Now() + kSimStepMs < until) {
      rig.Step();
      for (const EvPlayAnim& a : K2Events<EvPlayAnim>(rig.h.events)) {
        if (a.entity == c) anims.push_back(a);
      }
      CHECK(K2Events<EvTelegraph>(rig.h.events).empty());
      rig.h.events.Clear();
    }
    REQUIRE(anims.size() == 2);  // 0 and 1.5 s, both at the beast
    for (const EvPlayAnim& a : anims) {
      CHECK(a.clip == "Attack01");
      CHECK(a.faceTarget == Vec2(39, 41));
    }
    CHECK(rig.M(c).abilityClockStarted);  // the ability clock ran through the taunt
    CHECK(rig.M(c).pendingAbility == -1);
    // The taunt over, the next due swing (3 s) is the ability, at the hero.
    REQUIRE(rig.StepUntil([&] { return !K2Events<EvTelegraph>(rig.h.events).empty(); }, 120));
    CHECK_FALSE(rig.companion.TauntHolds(c));
    CHECK(K2Events<EvTelegraph>(rig.h.events)[0].abilityId == "slam");
    CHECK(K2Events<EvPlayAnim>(rig.h.events).back().faceTarget == Vec2(41, 40));
    // An exhausted beast holds no taunt.
    rig.companion.MutableStateForTesting().taunts.assign(1, {c, rig.Now() + 5000});
    CHECK(rig.companion.TauntHolds(c));
    rig.companion.MutableStateForTesting().exhaustedUntilMs = rig.Now() + 5000;
    CHECK_FALSE(rig.companion.TauntHolds(c));
  }

  TEST_CASE("K2 runtime: Death Save holds against a lethal hazard tick (a hazard ability's pulse, a left hazard)") {
    for (bool pulse : {true, false}) {
      INFO("pulse " << pulse);
      MonsterAbilityDef a = pulse ? K2Ability("seep", AbilityKind::Hazard, "Cast01", AbilityAim::Self, 700)
                                  : K2Circle("brand", 2.0, AbilityAim::Self, 700);
      if (pulse) {
        a.hasShape = true;
        a.shape = K2Shape(AbilityShapeKind::Circle, 2.0);
      } else {
        a.dmgMul = 0.1;
      }
      a.cooldownMs = 100000;
      a.hazard.present = true;
      a.hazard.durationMs = 1000;
      a.hazard.tickMs = 500;
      a.hazard.tickDmgMul = 1;
      for (bool save : {true, false}) {
        INFO("death save " << save);
        K2Rig rig;
        if (save) rig.h.ctx.equip.Ref(Stat::DeathSave) = 1;
        rig.hero->RecalcDerived(rig.h.ctx.equip);
        const MonsterDef def = K2Caster({a}, 30);
        rig.PlaceHero(Vec2(41, 40));
        const EntityId c = rig.Spawn(def, Vec2(40, 40));
        rig.ScriptNoDodge();
        rig.Step();
        REQUIRE(rig.M(c).pendingAbility == 0);
        if (pulse) rig.hero->SetHp(5);  // the first pulse lands at contact
        REQUIRE(rig.StepUntil([&] { return !rig.combat.Hazards().Entries().empty(); }, 100));
        rig.Silence(c);
        if (!pulse) rig.hero->SetHp(5);  // after the strike's own hit: the first tick 500 ms later
        REQUIRE(rig.StepUntil([&] { return !HeroHits(rig.h.events, true).empty(); }, 60));
        CHECK(HeroHits(rig.h.events, true).size() == 1);
        bool immunity = false;
        for (const Event& e : rig.h.events.Items()) {
          if (const EvLog* l = std::get_if<EvLog>(&e)) immunity = immunity || l->text.key == "zone.combat.deathImmunity";
        }
        if (save) {
          CHECK(immunity);
          CHECK(rig.hero->Life() == HeroLife::Alive);
          CHECK(rig.hero->Hp() == std::floor(rig.hero->MaxHp() * 0.3));
          CHECK(K2Events<EvHeroDied>(rig.h.events).empty());
        } else {
          CHECK_FALSE(immunity);
          CHECK(rig.hero->Life() == HeroLife::Dying);
          CHECK(K2Events<EvHeroDied>(rig.h.events).size() == 1);
        }
      }
    }
  }

  // The GameSim wiring: the contact timer reaches CombatSystem, TickHazards runs in the step, the snapshot carries the
  // telegraph and the hazard (a live game on the synthetic data, a caster dropped next to the hero away from the camps).
  TEST_CASE("K2 wiring: a GameSim step runs the ability, its telegraph, its hazard ticks and the snapshot views") {
    auto sim = GameSim::Create(K2Data(), SimConfig{});
    REQUIRE(sim != nullptr);
    REQUIRE(sim->NewGame(ClassId::Warrior, Difficulty::Normal, 77));
    sim->Context().sys.story->FinishAllBeats();
    sim->Step();
    SimContext& ctx = sim->Context();
    ZoneRuntime& zone = *ctx.sys.zone;
    const MapDef& map = zone.Map();
    // A walkable 3 x 3 patch far from every camp (no safe zone) for the hero and the caster.
    TilePos spot{-1, -1};
    for (int32_t r = 3; r < map.rows - 3 && spot.col < 0; ++r) {
      for (int32_t c = 3; c < map.cols - 3 && spot.col < 0; ++c) {
        bool ok = true;
        for (int32_t dr = -1; dr <= 1 && ok; ++dr) {
          for (int32_t dc = -1; dc <= 2 && ok; ++dc) ok = zone.Walkable(c + dc, r + dr);
        }
        for (const MapCampDef& camp : map.camps) ok = ok && Dist(Vec2(c, r), camp.pos.Center()) > map.safeZoneRadius + 4;
        if (ok) spot = TilePos{c, r};
      }
    }
    REQUIRE(spot.col >= 0);
    ctx.sys.locomotion->Teleport(spot.Center(), TeleportReason::Debug);
    MonsterAbilityDef brand = K2Circle("brand", 1.5, AbilityAim::HeroAtWindup, 900, "Cast01");
    brand.cooldownMs = 100000;
    brand.hazard.present = true;
    brand.hazard.durationMs = 1500;
    brand.hazard.tickMs = 500;
    brand.hazard.tickDmgMul = 0.1;
    const MonsterDef def = K2Caster({brand}, 10);
    MonsterSpawnParams p;
    p.baseDef = &def;
    p.tile = TilePos{spot.col + 1, spot.row};
    const EntityId c = ctx.sys.monsters->Spawn(p);
    REQUIRE(c != kNoEntity);
    ctx.sys.monsters->Find(c)->state = MonsterState::Attack;
    std::vector<EvTelegraph> telegraphs;
    std::vector<EvHazard> hazards;
    int ticks = 0;
    bool sawTelegraphView = false, sawHazardView = false;
    for (int i = 0; i < 4 * 60; ++i) {
      sim->Step();
      for (const Event& e : sim->Events()) {
        if (const EvTelegraph* t = std::get_if<EvTelegraph>(&e)) {
          if (t->entity == c) telegraphs.push_back(*t);
        }
        if (const EvHazard* h = std::get_if<EvHazard>(&e)) {
          if (h->source == c) hazards.push_back(*h);
        }
        if (const EvHit* h = std::get_if<EvHit>(&e)) ticks += h->target == kHeroEntityId && h->tick && h->source == c ? 1 : 0;
      }
      for (const TelegraphView& v : sim->View().telegraphs) sawTelegraphView = sawTelegraphView || v.entity == c;
      for (const HazardView& v : sim->View().hazards) sawHazardView = sawHazardView || v.source == c;
      if (!hazards.empty() && ctx.sys.monsters->Find(c) != nullptr) {
        ctx.sys.status->Apply(c, StatusType::Stun, 1, 600000, kHeroEntityId, sim->NowMs());  // no more swings
      }
    }
    REQUIRE(telegraphs.size() == 1);
    CHECK(telegraphs[0].abilityId == "brand");
    CHECK(telegraphs[0].contactMs - telegraphs[0].startMs == 900);
    REQUIRE(hazards.size() == 1);
    CHECK(hazards[0].startMs == doctest::Approx(telegraphs[0].contactMs).epsilon(1e-9));
    CHECK(sawTelegraphView);
    CHECK(sawHazardView);
    CHECK(ticks == 3);  // 1500 / 500, left behind by the strike
    CHECK(sim->View().hazards.empty());
  }

  // A seeded fight: two casters with every kind that runs (circle + brand hazard, cone, line, ring, bolt, a hazard
  // ability), the hero walking a fixed loop and swinging back, statuses, monster-on-monster hits. The digest of the
  // event log is pinned: GCC and Clang (and every platform) must replay it bit for bit (implementation-plan.md 3 K2).
  TEST_CASE("K2 replay: a seeded fight replays to the pinned event-log digest (GCC = Clang)") {
    struct Replay {
      uint64_t digest = 0;
      std::map<std::string, int> telegraphs;  // per ability id
      int hazards = 0, heroTicks = 0, heroHits = 0, monsterHits = 0, statuses = 0, bolts = 0, cancels = 0;
    };
    const auto run = [](uint64_t seed) {
      Replay out;
      K2Rig rig(seed);
      MonsterAbilityDef slam = K2Circle("slam", 1.8, AbilityAim::HeroAtWindup, 900, "Cast01");
      slam.cooldownMs = 4000;
      slam.priority = 3;
      slam.dmgMul = 1.6;
      slam.hasDamageType = true;
      slam.damageType = DamageType::Fire;
      slam.hazard.present = true;
      slam.hazard.durationMs = 3000;
      slam.hazard.tickMs = 500;
      slam.hazard.tickDmgMul = 0.15;
      slam.hazard.status.present = true;
      slam.hazard.status.type = StatusType::Burn;
      slam.hazard.status.chance = 0.5;
      slam.hazard.status.ms = 1500;
      slam.hazard.status.value = 2;
      slam.hitsMonsters = true;
      slam.monsterDmgMul = 2;
      MonsterAbilityDef sweep = K2Ability("sweep", AbilityKind::Cone, "Attack02", AbilityAim::Hero, 700);
      sweep.hasShape = true;
      sweep.shape = K2Shape(AbilityShapeKind::Cone, 2.6, 120);
      sweep.cooldownMs = 2500;
      sweep.priority = 2;
      sweep.dmgMul = 1.3;
      sweep.status.present = true;
      sweep.status.type = StatusType::Slow;
      sweep.status.chance = 0.6;
      sweep.status.ms = 1200;
      sweep.status.value = 25;
      MonsterAbilityDef lunge = K2Ability("lunge", AbilityKind::Line, "Lunge", AbilityAim::HeroAtWindup, 800);
      lunge.hasShape = true;
      lunge.shape = K2Shape(AbilityShapeKind::Line, 0, 0, 5, 1.2);
      lunge.cooldownMs = 5000;
      lunge.priority = 1;
      lunge.rangeMin = 1.2;
      MonsterAbilityDef ring = K2Ability("keen", AbilityKind::Ring, "Cast02", AbilityAim::Self);
      ring.hasShape = true;
      ring.shape = K2Shape(AbilityShapeKind::Ring, 4, 0, 0, 0, 1);
      ring.cooldownMs = 6000;
      ring.hasHpBelowPct = true;
      ring.hpBelowPct = 90;
      MonsterAbilityDef bolt = K2Ability("toll", AbilityKind::Bolt, "Cast01", AbilityAim::Hero);
      bolt.cooldownMs = 3000;
      bolt.dmgMul = 1.2;
      MonsterAbilityDef seep = K2Ability("seep", AbilityKind::Hazard, "Cast01", AbilityAim::Anchor, 700);
      seep.hasShape = true;
      seep.shape = K2Shape(AbilityShapeKind::Line, 0, 0, 8, 1);
      seep.cooldownMs = 6000;
      seep.priority = 4;
      seep.hazard.present = true;
      seep.hazard.durationMs = 1000;
      seep.hazard.tickMs = 500;
      seep.hazard.tickDmgMul = 0.5;
      MonsterDef boss = K2Caster({slam, sweep, lunge, ring}, 18, "fx_k2_boss");
      boss.hp = 900;
      boss.hitCapHeroMaxHpPct = 35;
      MonsterDef caster = K2Caster({bolt, seep}, 12, "fx_k2_shooter");
      caster.attackRange = 4;
      caster.isRanged = true;
      rig.hero->SetHp(rig.hero->MaxHp());
      rig.hero->attackTarget = kNoEntity;  // the hero swings back at the nearest aggro monster
      rig.PlaceHero(Vec2(41.5, 40));
      const EntityId a = rig.Spawn(boss, Vec2(40, 40), MonsterState::Chase, MonsterRole::StoryBoss);
      const EntityId b = rig.Spawn(caster, Vec2(43, 42), MonsterState::Chase);
      K2Fnv f;
      const Vec2 loop[] = {Vec2(41.5, 40), Vec2(42, 41.5), Vec2(40.5, 42), Vec2(39, 40.5), Vec2(41, 38.8)};
      for (int s = 0; s < 20 * 60; ++s) {
        if (s % 75 == 0 && rig.hero->Life() == HeroLife::Alive) rig.PlaceHero(loop[(s / 75) % 5]);
        if (rig.hero->Hp() < rig.hero->MaxHp() * 0.5) rig.hero->SetHp(rig.hero->MaxHp());  // keep the fight going
        rig.Step();
        K2DigestEvents(f, rig.h.events);
        for (const EvTelegraph& t : K2Events<EvTelegraph>(rig.h.events)) ++out.telegraphs[t.abilityId];
        out.hazards += static_cast<int>(K2Events<EvHazard>(rig.h.events).size());
        for (const EvHit& e : K2Events<EvHit>(rig.h.events)) {
          if (e.dodged) continue;
          if (e.target == kHeroEntityId) {
            (e.tick ? out.heroTicks : out.heroHits) += 1;
          } else if (e.source == a || e.source == b) {
            ++out.monsterHits;
          }
        }
        out.statuses += static_cast<int>(K2Events<EvStatusApplied>(rig.h.events).size());
        for (const EvProjectileLaunched& p : K2Events<EvProjectileLaunched>(rig.h.events)) out.bolts += p.source == b ? 1 : 0;
        out.cancels += static_cast<int>(K2Events<EvMonsterAttackCancelled>(rig.h.events).size());
        rig.h.events.Clear();
      }
      f.D(rig.hero->Hp());
      for (const MonsterInstance& m : rig.monsters.All()) {
        f.U64(m.id);
        f.D(m.hp);
        f.V(m.pos);
      }
      out.digest = f.h;
      return out;
    };
    const Replay r = run(4242);
    // The fight used every kind that runs.
    for (const char* id : {"slam", "sweep", "lunge", "keen", "seep"}) {
      INFO(id);
      CHECK(r.telegraphs.count(id) == 1);
    }
    CHECK(r.hazards > 0);
    CHECK(r.heroTicks > 0);
    CHECK(r.heroHits > 0);
    CHECK(r.monsterHits > 0);  // the slam's hitsMonsters
    CHECK(r.statuses > 0);
    CHECK(r.bolts > 0);
    CHECK(r.digest == run(4242).digest);  // same seed, same log
    CHECK(r.digest != run(4243).digest);  // the seed feeds the rolls
    char hex[17];
    std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(r.digest));
    INFO("digest " << hex);
    CHECK(std::string(hex) == "aeebb747324d47b9");
  }
  // =================================================================================================================
  // 2F review: ranged specials, clipless abilities, status-only strikes, dashes
  // =================================================================================================================
  TEST_CASE("2F review: a ranged special (rangeMax past the swing reach) starts between swings, from Chase") {
    // Pure: the kind and the range decide.
    MonsterAbilityDef far = K2Circle("far", 1.5, AbilityAim::HeroAtWindup, 900);
    far.hasRangeMax = true;
    far.rangeMax = 10;
    CHECK(AbilityIsRangedSpecial(far, 2.4));
    CHECK(AbilityCastsBetweenSwings(far, 2.4));
    CHECK_FALSE(AbilityIsRangedSpecial(far, 10));  // the reach itself: bound by it
    MonsterAbilityDef edge = far;
    edge.rangeMax = 2.4;
    CHECK_FALSE(AbilityIsRangedSpecial(edge, 1.7999999999999998 + 0.6));  // within the 1e-9 slack
    MonsterAbilityDef open = K2Circle("open", 1.5);
    CHECK_FALSE(AbilityIsRangedSpecial(open, 2.4));  // no rangeMax: bound by the reach
    MonsterAbilityDef raise = K2Ability("raise", AbilityKind::Raise, "Cast01", AbilityAim::Corpse);
    raise.hasRangeMax = true;
    raise.rangeMax = 10;
    CHECK_FALSE(AbilityIsRangedSpecial(raise, 2.4));  // a support kind already casts between swings
    CHECK(AbilityCastsBetweenSwings(raise, 2.4));
    // The picker: castsOnly takes the ranged special at 6 tiles, not the reach-bound ability; with castsOnly off both.
    {
      const MonsterAbilityDef list[] = {open, far};
      const double ready[] = {0, 0};
      AbilityPickInput in;
      in.readyAtMs = ready;
      in.heroDist = 6;
      in.swingReach = 2.4;
      in.castsOnly = true;
      CHECK(PickMonsterAbility(list, in) == 1);
      in.heroDist = 11;  // past its rangeMax
      CHECK(PickMonsterAbility(list, in) == -1);
      in.heroDist = 2;
      in.castsOnly = false;  // a due swing: the first by priority, then data order
      CHECK(PickMonsterAbility(list, in) == 0);
      in.swingReach = 0;  // an input without the reach: every ability with a rangeMax counts as one
      in.castsOnly = true;
      CHECK(PickMonsterAbility(list, in) == 1);
    }
    // Runtime: a caster in Chase, the hero 6 tiles off (no swing could reach him): the special starts at once from where
    // the caster stands, rooted, and lands at the clip's beat.
    {
      far.cooldownMs = 3000;
      K2Rig rig;
      const MonsterDef def = K2Caster({far}, 20);
      rig.PlaceHero(Vec2(46, 40));
      const EntityId c = rig.Spawn(def, Vec2(40, 40), MonsterState::Chase);
      rig.ScriptNoDodge(4);
      const double hp0 = rig.hero->Hp();
      rig.Step();
      const std::vector<EvTelegraph> tgs = K2Events<EvTelegraph>(rig.h.events);
      REQUIRE(tgs.size() == 1);
      CHECK(tgs[0].abilityId == "far");
      CHECK(tgs[0].origin == Vec2(46, 40));
      CHECK(rig.M(c).pendingAbility == 0);
      CHECK(Dist(rig.M(c).pos, rig.hero->Position()) > 2.4);
      const double contact = tgs[0].contactMs;
      const Vec2 at = rig.M(c).pos;  // where it stood when the windup started (the AI stepped first)
      CHECK(Dist(at, Vec2(40, 40)) < 0.1);
      while (rig.Now() + kSimStepMs < contact - 1e-6) {
        rig.Step();
        CHECK(rig.M(c).pos == at);  // rooted through its windup
      }
      rig.StepUntil([&] { return rig.hero->Hp() < hp0; }, 20);
      CHECK(HeroHits(rig.h.events, false).size() == 1);
      rig.Step(5);
      CHECK(rig.M(c).pos.x > at.x);  // then it chases
    }
    // Held on a dais (a tether of radius 1): the special keeps coming on its cooldown at a hero far off it.
    {
      far.cooldownMs = 3000;
      K2Rig rig;
      const MonsterDef def = K2Caster({far}, 5);
      rig.PlaceHero(Vec2(46, 40));
      const EntityId c = rig.Spawn(def, Vec2(40, 40), MonsterState::Chase);
      rig.M(c).hasTether = true;
      rig.M(c).tetherCentre = Vec2(40, 40);
      rig.M(c).tetherRadius = 1;
      rig.h.rng.Get(RngStream::Combat).Script(std::vector<double>(40, 0.99));
      size_t casts = 0;
      for (int i = 0; i < 10 * 60; ++i) {
        rig.Step();
        for (const EvTelegraph& t : K2Events<EvTelegraph>(rig.h.events)) casts += t.entity == c ? 1 : 0;
        rig.h.events.Clear();
        CHECK(Dist(rig.M(c).pos, Vec2(40, 40)) <= 1 + 1e-6);
      }
      CHECK(casts == 4);  // 0, 3, 6, 9 s
    }
    // The same ability without its rangeMax is bound by the reach: nothing starts at 6 tiles; the caster walks in.
    {
      K2Rig rig;
      const MonsterDef def = K2Caster({open}, 20);
      rig.PlaceHero(Vec2(46, 40));
      const EntityId c = rig.Spawn(def, Vec2(40, 40), MonsterState::Chase);
      rig.Step(30);
      CHECK(K2Events<EvTelegraph>(rig.h.events).empty());
      CHECK(rig.M(c).pos.x > 40);
    }
  }

  TEST_CASE("2F review: a hazard without a clip plays nothing and does not root its caster; the loader's rules") {
    MonsterAbilityDef pool = K2Ability("pool", AbilityKind::Hazard, "", AbilityAim::HeroAtWindup, 900);
    pool.hasShape = true;
    pool.shape = K2Shape(AbilityShapeKind::Circle, 1.2);
    pool.cooldownMs = 100000;
    pool.hazard.present = true;
    pool.hazard.durationMs = 1000;
    pool.hazard.tickMs = 500;
    pool.hazard.tickDmgMul = 0.5;
    K2Rig rig;
    const MonsterDef def = K2Caster({pool}, 20);
    rig.PlaceHero(Vec2(41.5, 40));
    const EntityId c = rig.Spawn(def, Vec2(40, 40));
    rig.Step();
    REQUIRE(rig.M(c).pendingAbility == 0);
    CHECK(K2Events<EvTelegraph>(rig.h.events).size() == 1);
    for (const EvPlayAnim& a : K2Events<EvPlayAnim>(rig.h.events)) CHECK(a.entity != c);  // no clip, no anim
    CHECK(rig.M(c).abilityRootedUntilMs == 0);
    CHECK(rig.combat.IsWindingUp(c));
    const double contact = rig.M(c).abilityCast.contactMs;
    CHECK(contact - rig.M(c).abilityCast.startMs == 900);  // its authored windup
    rig.PlaceHero(Vec2(46, 40));  // out of reach: the caster follows while the ground fills
    rig.StepUntilMs(contact - 100);
    CHECK(rig.M(c).pos.x > 40);
    CHECK(rig.M(c).pendingAbility == 0);
    rig.StepUntil([&] { return !K2Events<EvHazard>(rig.h.events).empty(); }, 30);
    REQUIRE(K2Events<EvHazard>(rig.h.events).size() == 1);
    CHECK(K2Events<EvHazard>(rig.h.events)[0].origin == Vec2(41.5, 40));  // where the hero stood at windup start
    // Loader: only a hazard or a call may leave its clip out, and then it authors its windup.
    struct Case {
      const char* ability;
      const char* text;  // nullptr = loads
    };
    const Case cases[] = {
        {R"({"id": "a", "kind": "circle", "windupMs": 900, "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}})",
         "names its clip"},
        {R"({"id": "a", "kind": "hazard", "cooldownMs": 0, "aim": "self", "shape": {"kind": "circle", "radius": 1}, "hazard": {"durationMs": 1000, "tickMs": 500, "tickDmgMul": 1}})",
         "authors its windupMs"},
        {R"({"id": "a", "kind": "hazard", "clip": "", "windupMs": 900, "cooldownMs": 0, "aim": "self", "shape": {"kind": "circle", "radius": 1}, "hazard": {"durationMs": 1000, "tickMs": 500, "tickDmgMul": 1}})",
         "empty clip"},
        {R"({"id": "a", "kind": "hazard", "windupMs": 900, "cooldownMs": 0, "aim": "self", "shape": {"kind": "circle", "radius": 1}, "hazard": {"durationMs": 1000, "tickMs": 500, "tickDmgMul": 1}})",
         nullptr},
    };
    for (const Case& k : cases) {
      INFO(k.ability);
      DataLoadReport report;
      K2LoadDef(K2DefJson("fx_k2", std::string("[") + k.ability + "]"), report);
      if (k.text == nullptr) {
        CHECK(report.Ok());
        CHECK_FALSE(K2HasIssue(report.warnings, "fx_k2.abilities.a", ""));  // no clip beat to check
      } else {
        CHECK_FALSE(report.Ok());
        CHECK(K2HasIssue(report.errors, "fx_k2", k.text));
      }
    }
  }

  TEST_CASE("2F review: a status-only strike (dmgMul 0) rolls no damage; the dodge roll's i-frames avoid it") {
    MonsterAbilityDef keen = K2Circle("keen", 4, AbilityAim::Self, 900, "Cast02");
    keen.cooldownMs = 100000;
    keen.dmgMul = 0;
    keen.status.present = true;
    keen.status.type = StatusType::Slow;
    keen.status.chance = 1;
    keen.status.ms = 2000;
    keen.status.value = 30;
    keen.hitsMonsters = false;
    for (const bool dodge : {false, true}) {
      INFO("dodge " << dodge);
      K2Rig rig;
      const MonsterDef def = K2Caster({keen}, 20);
      rig.PlaceHero(Vec2(41, 40));
      const EntityId c = rig.Spawn(def, Vec2(40, 40));
      const EntityId other = rig.Spawn(def, Vec2(39, 40), MonsterState::Idle);  // inside the ring: never hurt
      rig.Silence(other);
      const double hp0 = rig.hero->Hp();
      const double otherHp = rig.M(other).hp;
      rig.Step();
      REQUIRE(rig.M(c).pendingAbility == 0);
      const double contact = rig.M(c).abilityCast.contactMs;
      if (dodge) {
        rig.StepUntilMs(contact - 100);
        REQUIRE(rig.combat.RequestDodge(Vec2(0, 1)));  // 1.8 tiles: still inside the ring, invulnerable at contact
      }
      rig.h.events.Clear();
      rig.StepUntilMs(contact + 2 * kSimStepMs);
      CHECK(rig.M(c).pendingAbility == -1);
      CHECK(rig.hero->Hp() == hp0);
      CHECK(rig.M(other).hp == otherHp);
      size_t slows = 0, misses = 0;
      for (const EvStatusApplied& s : K2Events<EvStatusApplied>(rig.h.events)) {
        slows += s.target == kHeroEntityId && s.type == StatusType::Slow ? 1 : 0;
      }
      for (const EvHit& h : K2Events<EvHit>(rig.h.events)) {
        if (h.target != kHeroEntityId) continue;
        CHECK(h.amount == 0);
        misses += h.dodged && h.iframeAvoided ? 1 : 0;
      }
      CHECK(slows == (dodge ? 0u : 1u));
      CHECK(misses == (dodge ? 1u : 0u));
      CHECK(rig.status.Has(kHeroEntityId, StatusType::Slow) == !dodge);
    }
    // Loader: a status-only strike needs its status and hits no monster.
    struct Case {
      const char* ability;
      const char* text;
    };
    const Case cases[] = {
        {R"({"id": "a", "kind": "circle", "clip": "Cast02", "windupMs": 900, "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}, "dmgMul": 0})",
         "needs its 'status'"},
        {R"({"id": "a", "kind": "circle", "clip": "Cast02", "windupMs": 900, "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}, "dmgMul": 0, "status": {"type": "slow", "ms": 1000, "value": 30}, "hitsMonsters": true})",
         "hits no monster"},
    };
    for (const Case& k : cases) {
      INFO(k.ability);
      DataLoadReport report;
      K2LoadDef(K2DefJson("fx_k2", std::string("[") + k.ability + "]"), report);
      CHECK_FALSE(report.Ok());
      CHECK(K2HasIssue(report.errors, "fx_k2", k.text));
    }
    DataLoadReport ok;
    K2LoadDef(K2DefJson("fx_k2", R"([{"id": "a", "kind": "circle", "clip": "Cast02", "windupMs": 900, "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}, "dmgMul": 0, "status": {"type": "slow", "ms": 1000, "value": 30}}])"),
              ok);
    CHECK(ok.Ok());
  }

  TEST_CASE("2F review: a dash ends where its windup fixed it, after the hit; walls, camps and bounds stop it short") {
    // Pure: the samples every kDashSampleTiles, the exact end, the first closed sample.
    const auto all = [](Vec2) { return true; };
    CHECK(AbilityDashEnd(Vec2(40, 40), Vec2(2, 0), 4, all) == Vec2(44, 40));
    const Vec2 odd = AbilityDashEnd(Vec2(40, 40), Vec2(1, 0), 3.1, all);
    CHECK(odd == Vec2(43.1, 40));
    CHECK(AbilityDashEnd(Vec2(40, 40), Vec2(1, 0), 4, [](Vec2 p) { return p.x < 42.6; }) == Vec2(42.5, 40));
    CHECK(AbilityDashEnd(Vec2(40, 40), Vec2(1, 0), 4, [](Vec2 p) { return p.x < 40.1; }) == Vec2(40, 40));
    CHECK(AbilityDashEnd(Vec2(40, 40), Vec2(1, 0), 3.1, [](Vec2 p) { return p.x < 43.05; }) == Vec2(43, 40));
    CHECK(AbilityDashEnd(Vec2(40, 40), Vec2(1, 0), 0, all) == Vec2(40, 40));
    // Runtime.
    MonsterAbilityDef lunge = K2Ability("lunge", AbilityKind::Line, "Attack02", AbilityAim::HeroAtWindup, 800);
    lunge.hasShape = true;
    lunge.shape = K2Shape(AbilityShapeKind::Line, 0, 0, 4, 1.2);
    lunge.cooldownMs = 100000;
    lunge.dash = true;
    const MonsterDef def = K2Caster({lunge}, 20);
    {
      K2Rig rig;
      rig.PlaceHero(Vec2(42, 40));
      const EntityId c = rig.Spawn(def, Vec2(40, 40));
      rig.ScriptNoDodge();
      rig.Step();
      const std::vector<EvTelegraph> tgs = K2Events<EvTelegraph>(rig.h.events);
      REQUIRE(tgs.size() == 1);
      CHECK(tgs[0].hasDash);
      CHECK(tgs[0].dashTo == Vec2(44, 40));
      Snapshot snap;
      rig.combat.FillSnapshot(snap);
      REQUIRE(snap.telegraphs.size() == 1);
      CHECK(snap.telegraphs[0].hasDash);
      CHECK(snap.telegraphs[0].dashTo == Vec2(44, 40));
      const double contact = tgs[0].contactMs;
      rig.h.events.Clear();
      rig.StepUntil([&] { return !K2Events<EvEntityTeleported>(rig.h.events).empty(); }, 80);
      CHECK(rig.Now() >= contact - 1e-6);
      CHECK(rig.Now() < contact + kSimStepMs);
      const std::vector<EvEntityTeleported> tp = K2Events<EvEntityTeleported>(rig.h.events);
      REQUIRE(tp.size() == 1);
      CHECK(tp[0].id == c);
      CHECK(tp[0].from == Vec2(40, 40));
      CHECK(tp[0].to == Vec2(44, 40));
      CHECK(tp[0].reason == TeleportReason::Dash);
      CHECK(rig.M(c).pos == Vec2(44, 40));
      CHECK(HeroHits(rig.h.events, false).size() == 1);  // the hit came first, along the line it then ran
      size_t hitAt = 0, tpAt = 0;
      for (size_t i = 0; i < rig.h.events.Size(); ++i) {
        if (const EvHit* h = std::get_if<EvHit>(&rig.h.events.Items()[i]); h != nullptr && h->target == kHeroEntityId) hitAt = i;
        if (std::holds_alternative<EvEntityTeleported>(rig.h.events.Items()[i])) tpAt = i;
      }
      CHECK(hitAt < tpAt);
    }
    {  // the leash range stops it: a spawn anchor 7.5 tiles behind, leash 8 -> the dash ends 0.5 tiles on
      K2Rig rig;
      rig.PlaceHero(Vec2(49, 40));
      const EntityId c = rig.Spawn(def, Vec2(40, 40));
      rig.M(c).pos = Vec2(47.5, 40);
      rig.M(c).prevPos = rig.M(c).pos;
      const double leash = rig.h.ctx.data.Monsters().ai.leashRange;
      rig.ScriptNoDodge();
      rig.Step();
      const std::vector<EvTelegraph> tgs = K2Events<EvTelegraph>(rig.h.events);
      REQUIRE(tgs.size() == 1);
      CHECK(tgs[0].hasDash);
      CHECK(tgs[0].dashTo.x <= 40 + leash + 1e-9);
      CHECK(tgs[0].dashTo.x > 40 + leash - kDashSampleTiles - 1e-9);
    }
    {  // a wall across the line: the dash ends on the last sample before it (its tile, JS-rounded, is closed)
      K2Rig rig;
      MonsterWorld w = MOpenWorld(rig.zone.Map().cols, rig.zone.Map().rows);
      w.walkable = [](int32_t col, int32_t) { return col != 43; };
      rig.monsters.SetWorldForTesting(std::move(w));
      rig.PlaceHero(Vec2(42, 40));
      const EntityId c = rig.Spawn(def, Vec2(40, 40));
      rig.ScriptNoDodge();
      rig.Step();
      const std::vector<EvTelegraph> tgs = K2Events<EvTelegraph>(rig.h.events);
      REQUIRE(tgs.size() == 1);
      CHECK(tgs[0].hasDash);
      CHECK(tgs[0].dashTo == Vec2(42.25, 40));  // 42.5 rounds to column 43
      rig.h.events.Clear();
      REQUIRE(rig.StepUntil([&] { return !K2Events<EvEntityTeleported>(rig.h.events).empty(); }, 80));
      CHECK(K2Events<EvEntityTeleported>(rig.h.events)[0].id == c);
      CHECK(rig.M(c).pos == Vec2(42.25, 40));
    }
    {  // a camp's safe zone across the line: the dash stops outside it
      K2Rig rig;
      REQUIRE_FALSE(rig.zone.Map().camps.empty());
      const Vec2 camp = rig.zone.Map().camps[0].pos.Center();
      REQUIRE(rig.zone.InSafeZone(camp));
      Vec2 edge = camp;  // the first point west of the camp's centre outside its safe zone
      while (rig.zone.InSafeZone(edge)) edge.x -= 0.05;
      const Vec2 from = edge - Vec2(2, 0);
      REQUIRE_FALSE(rig.zone.InSafeZone(from));
      rig.PlaceHero(from + Vec2(1.5, 0));
      rig.Spawn(def, from);
      rig.ScriptNoDodge();
      rig.Step();
      const std::vector<EvTelegraph> tgs = K2Events<EvTelegraph>(rig.h.events);
      REQUIRE(tgs.size() == 1);
      CHECK(tgs[0].hasDash);
      CHECK_FALSE(rig.zone.InSafeZone(tgs[0].dashTo));
      CHECK(tgs[0].dashTo.x > from.x + 1.5);  // it ran up to the camp's edge
      CHECK(rig.zone.InSafeZone(tgs[0].dashTo + Vec2(kDashSampleTiles, 0)));
    }
    {  // a K4 tether (radius 2 round its spot): the dash ends on the circle
      K2Rig rig;
      rig.PlaceHero(Vec2(42, 40));
      const EntityId c = rig.Spawn(def, Vec2(40, 40));
      rig.M(c).hasTether = true;
      rig.M(c).tetherCentre = Vec2(40, 40);
      rig.M(c).tetherRadius = 2;
      rig.ScriptNoDodge();
      rig.Step();
      const std::vector<EvTelegraph> tgs = K2Events<EvTelegraph>(rig.h.events);
      REQUIRE(tgs.size() == 1);
      CHECK(tgs[0].dashTo == Vec2(42, 40));
    }
    {  // a stun mid-windup: no hit, no dash
      K2Rig rig;
      rig.PlaceHero(Vec2(42, 40));
      const EntityId c = rig.Spawn(def, Vec2(40, 40));
      rig.Step();
      REQUIRE(rig.M(c).pendingAbility == 0);
      rig.Step(10);
      rig.status.Apply(c, StatusType::Stun, 1, 2000, kHeroEntityId, rig.Now());
      rig.Step(80);
      CHECK(CancelsOf(rig.h.events, c) == 1);
      for (const EvEntityTeleported& t : K2Events<EvEntityTeleported>(rig.h.events)) CHECK(t.id != c);
      CHECK(rig.M(c).pos == Vec2(40, 40));
      CHECK(HeroHits(rig.h.events, false).empty());
    }
    // Loader: a dash runs along a line from the caster.
    struct Case {
      const char* ability;
      const char* text;
    };
    const Case cases[] = {
        {R"({"id": "a", "kind": "circle", "clip": "Attack02", "cooldownMs": 0, "aim": "self", "shape": {"radius": 1}, "dash": true})",
         "only a line ability dashes"},
        {R"({"id": "a", "kind": "line", "clip": "Attack02", "cooldownMs": 0, "aim": "anchor", "shape": {"length": 4, "width": 1}, "dash": true})",
         "not aim 'anchor'"},
    };
    for (const Case& k : cases) {
      INFO(k.ability);
      DataLoadReport report;
      K2LoadDef(K2DefJson("fx_k2", std::string("[") + k.ability + "]"), report);
      CHECK_FALSE(report.Ok());
      CHECK(K2HasIssue(report.errors, "fx_k2", k.text));
    }
    DataLoadReport ok;
    K2LoadDef(K2DefJson("fx_k2", R"([{"id": "a", "kind": "line", "clip": "Attack02", "cooldownMs": 0, "aim": "hero", "shape": {"length": 4, "width": 1}, "dash": true}])"),
              ok);
    CHECK(ok.Ok());
  }
}
