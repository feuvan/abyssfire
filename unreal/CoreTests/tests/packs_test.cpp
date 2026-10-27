// K5 packs, morale, kite and block (dark remake, implementation-plan.md 3 "K5", work package 2D): the schema (monster
// packAggro / kite / block / morale / aura, buff / call blocks, map spawn mix / leader: loads, loader refusals, DataStore
// cross checks), the pure rules (monsters/Packs.h, the kite / flee steps of MonsterAI.h) and the runtime through
// MonsterSystem / CombatSystem: a spawn entry's mix spawning as one pack with its leader (positions unchanged), hunts as
// packs, pack aggro within its radius once, morale (flee duration, direction, the blocked fallback, the leash), sustain
// (a drummer, a fearImmune buff), the kite band (leash, walls, then a swing), the block arc (front / side / back, ticks,
// a stunned blocker, before damageTakenMul), the shield wall's shared heading, buff / call abilities, auras, the GameSim
// wiring (a mixed pack through the zone population and the kill pipeline, the snapshot), and a seeded replay whose
// event-log digest is pinned (GCC and Clang).
//
// Synthetic content only: monster definitions are built here (TestContent.h fixtures); one synthetic asset
// (`fx_k5_art`: Attack01 / Cast01) and synthetic definitions (fx_k5_*) are added to the real export in memory, and the
// population cases edit a copy of the zone's spawn list in memory, so no content change moves these tests.
// Suite: monsters.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <optional>
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
#include "abyss/monsters/MonsterAbilities.h"
#include "abyss/monsters/Packs.h"
#include "abyss/monsters/Phases.h"
#include "abyss/quests/QuestGuide.h"
#include "abyss/quests/QuestWorld.h"
#include "abyss/sim/GameSim.h"
#include "abyss/story/StoryDirector.h"
#include "doctest/doctest.h"

using namespace abyss;

namespace {

using namespace abyss::test::monsters_fx;

constexpr char kArt[] = "fx_k5_art";  // the synthetic asset's game id

// Clips of the synthetic asset: Attack01 Contact 500 / 1000 ms, Cast01 Release 800 / 1200 ms (the drum).
constexpr char kK5AssetJson[] = R"({
  "kind": "SkeletalMesh", "category": "Monsters", "gameIds": ["fx_k5_art"], "skeleton": "SKEL_Human",
  "anims": [
    {"name": "Attack01", "lengthMs": 1000, "contactMs": 500, "notifies": [{"name": "Contact", "ms": 500}]},
    {"name": "Cast01", "lengthMs": 1200, "releaseMs": 800, "notifies": [{"name": "Release", "ms": 800}]}
  ]
})";

JsonValue K5Parse(std::string_view text) {
  JsonValue v;
  JsonParseError err;
  const bool ok = ParseJson(text, v, &err);
  REQUIRE_MESSAGE(ok, "bad test JSON");
  return v;
}

using K5Edits = std::vector<std::pair<std::string, std::function<void(JsonValue&)>>>;

// The real export with `edits` applied in memory (several edits of one file run in order on one document).
std::unique_ptr<DataStore> K5LoadEdited(const K5Edits& edits, DataLoadReport& report) {
  std::map<std::string, JsonValue> docs;
  for (const auto& [file, edit] : edits) {
    if (docs.count(file) == 0) docs.emplace(file, K5Parse(test::ReadFile(test::DataDir() + "/" + file)));
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

void K5AddAsset(JsonValue& assets) {
  JsonValue* list = assets.FindMutable("manifest");
  REQUIRE(list != nullptr);
  list = list->FindMutable("assets");
  REQUIRE(list != nullptr);
  list->Set("SK_Fx_K5", K5Parse(kK5AssetJson));
}

// A monsters.json definition (with its exporter `derived` block) and extra top-level members (`extra`, JSON object
// members without braces). Range 1.5 (melee) unless `range` says otherwise.
std::string K5DefJson(std::string_view id, std::string_view extra, double range = 1.5, int hp = 300) {
  return StrCat(R"({"id": ")", id, R"(", "name": ")", id, R"(", "level": 6, "hp": )", hp, R"(, "damage": 10,
    "defense": 4, "speed": 50, "aggroRange": 5, "attackRange": )",
                range, R"(, "attackSpeed": 1200, "expReward": 20, "goldReward": [2, 4],
    "spriteKey": "fx_k5_art", "animCategory": "humanoid")",
                extra.empty() ? "" : ", ", extra, R"(, "derived": {"isRanged": )", range > 2.5 ? "true" : "false",
                R"(, "onHitStatus": [], "projectileColor": 13386956, "homeZone": null, "nameKey": "data.monster.)", id,
                R"(", "sources": ["zone"]}})");
}

void K5AddDef(JsonValue& monsters, const std::string& json) {
  JsonValue def = K5Parse(json);
  const std::string id(def.Get("id").AsString());
  JsonValue* defs = monsters.FindMutable("defs");
  REQUIRE(defs != nullptr);
  defs->Set(id, std::move(def));
}

// The synthetic definitions the population cases spawn by id: a member that keeps its pack's morale and joins its
// alarm, and a plain leader.
void K5AddDefs(JsonValue& monsters) {
  K5AddDef(monsters, K5DefJson("fx_k5_member", R"("family": "fx_living", "packAggro": 6,
    "morale": {"fleeOnLeaderDeathMs": 5000})"));
  K5AddDef(monsters, K5DefJson("fx_k5_chief", R"("family": "fx_living")"));
}

// The real export plus the synthetic asset and definitions (loaded once).
const DataStore& K5Data() {
  static std::unique_ptr<DataStore> store;
  if (store == nullptr) {
    DataLoadReport report;
    store = K5LoadEdited({{"assets.json", K5AddAsset}, {"monsters.json", K5AddDefs}}, report);
    if (!report.Ok()) MESSAGE(report.Summary());
    REQUIRE(report.Ok());
  }
  return *store;
}

std::unique_ptr<DataStore> K5LoadDef(std::string defJson, DataLoadReport& report) {
  return K5LoadEdited({{"assets.json", K5AddAsset},
                       {"monsters.json", K5AddDefs},
                       {"monsters.json", [defJson](JsonValue& doc) { K5AddDef(doc, defJson); }}},
                      report);
}

bool K5HasIssue(const std::vector<DataIssue>& issues, std::string_view pathPart, std::string_view text) {
  for (const DataIssue& e : issues) {
    if (Contains(e.path, pathPart) && Contains(e.message, text)) return true;
  }
  return false;
}

// The index of the default map's first spawn entry whose monster is a plain regular (not elite, not the story boss) and
// whose count is at least `minCount` (the population cases rewrite it).
size_t K5PlainSpawn(const DataStore& data, int32_t minCount) {
  const MapDef* map = data.World().FindMap(data.World().defaultMap);
  REQUIRE(map != nullptr);
  for (size_t i = 0; i < map->spawns.size(); ++i) {
    const MapSpawnDef& s = map->spawns[i];
    const MonsterDef* d = data.Monsters().FindForZone(map->id, s.monsterId);
    if (d == nullptr || d->elite || s.monsterId == data.Monsters().ai.storyBossId || s.count < minCount) continue;
    return i;
  }
  FAIL("no plain spawn entry");
  return 0;
}

// Rewrites spawn `index` of the default map in maps.json: `mix` / `leader` JSON (empty = leave out) and the entry's
// `monsterId` (empty = keep it).
std::function<void(JsonValue&)> K5EditSpawn(const DataStore& data, size_t index, std::string mix, std::string leader,
                                            std::string monsterId = "") {
  const std::string mapId = data.World().defaultMap;
  return [mapId, index, mix, leader, monsterId](JsonValue& doc) {
    JsonValue* spawns = doc.FindMutable("maps")->FindMutable(mapId)->FindMutable("spawns");
    REQUIRE(spawns != nullptr);
    REQUIRE(index < spawns->Size());
    JsonValue* entry = &spawns->MutableItems()[index];
    if (!mix.empty()) entry->Set("mix", K5Parse(mix));
    if (!leader.empty()) entry->Set("leader", K5Parse(leader));
    if (!monsterId.empty()) entry->Set("monsterId", JsonValue::String(monsterId));
  };
}

// ---- synthetic definitions ------------------------------------------------------------------------------------------

// A melee body of the living family: level 6, 400 HP, `damage` (1 keeps the hero standing), defense 0, speed 50, aggro
// 5, reach 1.5, a swing every 1200 ms.
MonsterDef K5Melee(const char* id = "fx_k5_melee", double damage = 1) {
  MonsterDef d = test::fixture::MakeFixtureMonster(id, 6, 400, damage, 0, 50, 5, 1.5, 1200, 20, 2, 4, kArt,
                                                   AnimRig::Humanoid, false, false);
  d.family = "fx_living";
  return d;
}

// A pack member that joins the alarm within `aggro` tiles and flees for `fleeMs` when its leader dies.
MonsterDef K5Member(double packAggro = 6, double fleeMs = 5000, const char* id = "fx_k5_member") {
  MonsterDef d = K5Melee(id);
  d.packAggro = packAggro;
  if (fleeMs > 0) {
    d.morale.present = true;
    d.morale.fleeOnLeaderDeathMs = fleeMs;
  }
  return d;
}

// A ranged body (range 6, a swing every 2200 ms, speed 34) that kites 3-6 tiles.
MonsterDef K5Archer(double kiteMin = 3, double kiteMax = 6) {
  MonsterDef d = test::fixture::MakeFixtureMonster("fx_k5_archer", 5, 300, 1, 0, 34, 7, 6.0, 2200, 24, 3, 6, kArt,
                                                   AnimRig::Humanoid, false, false);
  d.isRanged = true;
  d.family = "fx_unburied";
  d.kite.present = true;
  d.kite.min = kiteMin;
  d.kite.max = kiteMax;
  return d;
}

// A shieldbearer: a 100-degree front arc halves hits; `wall` tiles of shield wall.
MonsterDef K5Shield(double wall = 2, const char* id = "fx_k5_shield") {
  MonsterDef d = test::fixture::MakeFixtureMonster(id, 7, 500, 1, 0, 30, 5, 1.6, 1700, 34, 3, 7, kArt,
                                                   AnimRig::Humanoid, false, false);
  d.family = "fx_unburied";
  d.block.present = true;
  d.block.arcDeg = 100;
  d.block.damageMul = 0.5;
  d.block.wall = wall;
  return d;
}

// A buff / call ability on Cast01 (contact 800 ms, its Release), aim self.
MonsterAbilityDef K5Cast(std::string id, AbilityKind kind, double cooldownMs, double radius, std::string family = "") {
  MonsterAbilityDef a;
  a.id = std::move(id);
  a.kind = kind;
  a.clip = "Cast01";
  a.aim = AbilityAim::Self;
  a.cooldownMs = cooldownMs;
  a.buff.present = true;
  a.buff.radius = radius;
  a.buff.family = std::move(family);
  return a;
}

MonsterAbilityDef K5Buff(MonsterAbilityDef a, MonsterBuffStat stat, double mul, double ms) {
  a.buff.hasStat = true;
  a.buff.stat = stat;
  a.buff.mul = mul;
  a.buff.ms = ms;
  return a;
}

// The drummer: a call every 8 s that steadies its family within 8 tiles (fearImmune 3 s) and calls them to the fight.
MonsterDef K5Drummer() {
  MonsterDef d = K5Melee("fx_k5_drummer");
  d.abilities = {K5Buff(K5Cast("drum", AbilityKind::Call, 8000, 8, "fx_living"), MonsterBuffStat::FearImmune, 1, 3000)};
  return d;
}

// ---- runtime rig: the combat systems around a SimHarness on the synthetic data, stepped in the GameSim order, with the
// kill pipeline's K3 / K5 step wired as SimWiring does --------------------------------------------------------------------
struct K5Rig {
  explicit K5Rig(uint64_t seed = 7, const DataStore& data = K5Data())
      : h(seed, data),
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
    h.bus.Subscribe<MonsterAggroMsg>([this](const MonsterAggroMsg& m) { aggro.push_back(m.monster); });
    h.session.difficulty = Difficulty::Normal;
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
  double Now() const { return h.clock.NowMs(); }

  // A normal-difficulty-scaled monster at an exact position, no stat dodge / crit (deterministic rolls), in `state`, in
  // `pack` (0 = none), leading it when `leader`.
  EntityId Spawn(const MonsterDef& def, Vec2 pos, MonsterState state = MonsterState::Idle, uint32_t pack = 0,
                 bool leader = false) {
    MonsterSpawnParams p;
    p.baseDef = &def;
    p.tile = RoundToTile(pos);
    p.packId = pack;
    p.packLeader = leader;
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
  // A hit of `amount` on a monster from `from` (the hero by default: KillSource::HeroSkill, no provoke unless asked).
  HitWeight Hit(EntityId id, double amount, std::optional<Vec2> from = std::nullopt, bool provokes = false,
                bool tick = false) {
    MonsterHitRequest r;
    r.monster = id;
    r.amount = amount;
    r.isTick = tick;
    r.hasFrom = true;
    r.from = from ? *from : hero->Position();
    r.attacker = kHeroEntityId;
    r.source = tick ? KillSource::StatusTick : KillSource::HeroSkill;
    r.provokes = provokes;
    return combat.DamageMonster(r);
  }
  // A killing hit with no direction (so no block can stop it).
  void Kill(EntityId id) {
    MonsterHitRequest r;
    r.monster = id;
    r.amount = M(id).hp;
    r.attacker = kHeroEntityId;
    r.source = KillSource::HeroSkill;
    r.provokes = false;
    combat.DamageMonster(r);
  }
  void PlaceHero(Vec2 p) { loco.Teleport(p, TeleportReason::Debug); }
  Snapshot View() const {
    Snapshot s;
    monsters.FillSnapshot(s);
    return s;
  }
  const MonsterView* ViewOf(const Snapshot& s, EntityId id) const {
    for (const MonsterView& v : s.monsters) {
      if (v.id == id) return &v;
    }
    return nullptr;
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
  bool keepHeroUp = true;  // refill the hero before each step (the fights here measure the monsters, not the hero)
  EntityId dummy = kNoEntity;
  std::vector<EntityId> aggro;  // MonsterAggroMsg publications, in order
};

template <class E>
std::vector<E> K5Events(const EventSink& sink) {
  std::vector<E> out;
  for (const Event& e : sink.Items()) {
    if (const E* p = std::get_if<E>(&e)) out.push_back(*p);
  }
  return out;
}

template <class E>
std::vector<E> K5Events(std::span<const Event> events) {
  std::vector<E> out;
  for (const Event& e : events) {
    if (const E* p = std::get_if<E>(&e)) out.push_back(*p);
  }
  return out;
}

// Swing starts (EvPlayAnim Attack01) of `entity`.
int K5Swings(const EventSink& sink, EntityId entity) {
  int n = 0;
  for (const EvPlayAnim& a : K5Events<EvPlayAnim>(sink)) n += a.entity == entity && a.clip == "Attack01" ? 1 : 0;
  return n;
}

// ---- replay digest --------------------------------------------------------------------------------------------------

struct K5Fnv {
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

// Every event's variant index, and the fields of the ones a K5 fight produces.
void K5DigestEvents(K5Fnv& f, const EventSink& sink) {
  for (const Event& e : sink.Items()) {
    f.U64(e.index());
    if (const EvPlayAnim* a = std::get_if<EvPlayAnim>(&e)) {
      f.U64(a->entity);
      f.U64(static_cast<uint64_t>(a->action));
      f.S(a->clip);
      f.D(a->startMs);
    } else if (const EvHit* hh = std::get_if<EvHit>(&e)) {
      f.U64(hh->target);
      f.U64(hh->source);
      f.D(hh->amount);
      f.U64((hh->crit ? 1u : 0u) | (hh->dodged ? 2u : 0u) | (hh->tick ? 4u : 0u) | (hh->killed ? 8u : 0u));
    } else if (const EvBlock* b = std::get_if<EvBlock>(&e)) {
      f.U64(b->entity);
      f.V(b->from);
      f.D(b->amount);
      f.D(b->blocked);
    } else if (const EvMonsterFlee* fl = std::get_if<EvMonsterFlee>(&e)) {
      f.U64(fl->entity);
      f.U64(fl->leader);
      f.V(fl->from);
      f.D(fl->untilMs);
      f.U64((fl->ended ? 1u : 0u) | (static_cast<uint64_t>(fl->reason) << 1));
    } else if (const EvMonsterBuffCast* c = std::get_if<EvMonsterBuffCast>(&e)) {
      f.U64(c->entity);
      f.S(c->abilityId);
      f.U64(static_cast<uint64_t>(c->buffed));
      f.U64(static_cast<uint64_t>(c->called));
      f.D(c->untilMs);
    }
  }
}

}  // namespace

TEST_SUITE("monsters") {
  // =================================================================================================================
  // Data: the K5 schema (LoadMonsters.cpp ReadMonsterK5, the buff / call block, LoadWorld.cpp spawn mix / leader) and
  // DataStore::Finalize's cross checks
  // =================================================================================================================
  TEST_CASE("K5 data: pack aggro, kite, block, morale, aura and buff / call blocks load; defaults") {
    const std::string extra = R"("family": "fx_living", "packAggro": 6,
      "kite": {"min": 3, "max": 6}, "block": {"arcDeg": 100, "damageMul": 0.5, "wall": 2},
      "morale": {"fleeOnLeaderDeathMs": 5000, "fleeRadius": 8, "sustainFamily": "fx_living", "sustainRadius": 8},
      "aura": {"stat": "damage", "mul": 1.15, "radius": 5, "family": "fx_unburied"},
      "abilities": [
        {"id": "drum", "kind": "call", "clip": "Cast01", "cooldownMs": 8000, "aim": "self",
         "buff": {"stat": "fearImmune", "ms": 2000, "radius": 8, "family": "fx_living"}},
        {"id": "bell", "kind": "call", "clip": "Cast01", "cooldownMs": 10000, "aim": "self", "buff": {"radius": 10}},
        {"id": "march", "kind": "buff", "clip": "Cast01", "cooldownMs": 8000, "aim": "self",
         "buff": {"stat": "attackSpeed", "mul": 1.1, "ms": 2000}}])";
    DataLoadReport report;
    auto store = K5LoadDef(K5DefJson("fx_k5_full", extra, 6.0), report);
    INFO(report.Summary());
    REQUIRE(report.Ok());
    const MonsterDef* d = store->FindMonster("fx_k5_full");
    REQUIRE(d != nullptr);
    CHECK(d->packAggro == 6);
    CHECK((d->kite.present && d->kite.min == 3 && d->kite.max == 6));
    CHECK((d->block.present && d->block.arcDeg == 100 && d->block.damageMul == 0.5 && d->block.wall == 2));
    CHECK((d->morale.present && d->morale.fleeOnLeaderDeathMs == 5000 && d->morale.fleeRadius == 8 &&
           d->morale.sustainFamily == "fx_living" && d->morale.sustainRadius == 8));
    CHECK((d->aura.present && d->aura.stat == MonsterBuffStat::Damage && d->aura.mul == 1.15 && d->aura.radius == 5 &&
           d->aura.family == "fx_unburied"));
    REQUIRE(d->abilities.size() == 3);
    const AbilityBuffDef& drum = d->abilities[0].buff;
    CHECK((drum.present && drum.hasStat && drum.stat == MonsterBuffStat::FearImmune && drum.ms == 2000 &&
           drum.radius == 8 && drum.family == "fx_living" && drum.mul == 1));
    const AbilityBuffDef& bell = d->abilities[1].buff;  // a call that only provokes
    CHECK((bell.present && !bell.hasStat && bell.radius == 10 && bell.ms == 0 && bell.family.empty()));
    const AbilityBuffDef& march = d->abilities[2].buff;
    CHECK((march.present && march.hasStat && march.stat == MonsterBuffStat::AttackSpeed && march.mul == 1.1 &&
           march.radius == 0));
    // A buff / call is a cast between swings: no swing-reach warning even past the reach (the drum's rangeMax below).
    CHECK_FALSE(K5HasIssue(report.warnings, "fx_k5_full.abilities", "swing"));
    // Defaults: nothing of K5 on a plain definition.
    const MonsterDef* plain = store->FindMonster("fx_k5_chief");
    REQUIRE(plain != nullptr);
    CHECK(plain->packAggro == 0);
    CHECK_FALSE(plain->kite.present);
    CHECK_FALSE(plain->block.present);
    CHECK_FALSE(plain->morale.present);
    CHECK_FALSE(plain->aura.present);
    // Optional fields: no wall, no flee radius (the whole pack), no sustain; an aura's mul defaults to 1.
    DataLoadReport r2;
    auto s2 = K5LoadDef(K5DefJson("fx_k5_min", R"("block": {"arcDeg": 90, "damageMul": 0},
      "morale": {"fleeOnLeaderDeathMs": 3000}, "aura": {"stat": "fearImmune", "radius": 4})"), r2);
    INFO(r2.Summary());
    REQUIRE(r2.Ok());
    const MonsterDef* m = s2->FindMonster("fx_k5_min");
    REQUIRE(m != nullptr);
    CHECK((m->block.wall == 0 && m->block.damageMul == 0));
    CHECK((m->morale.fleeRadius == 0 && m->morale.sustainRadius == 0 && m->morale.sustainFamily.empty()));
    CHECK((m->aura.mul == 1 && m->aura.family.empty()));
    // A hunt built on a K5 definition keeps its blocks (MakeHuntDefinition copies the base).
    const HuntDef& hunt = store->Monsters().hunts.front();
    MonsterDef base = *d;
    const MonsterDef hd = MakeHuntDefinition(base, hunt, store->Monsters().ai, hunt.name);
    CHECK((hd.packAggro == 6 && hd.kite.present && hd.block.present && hd.morale.present && hd.aura.present));
  }

  TEST_CASE("K5 data: the loader refuses broken K5 blocks") {
    const std::vector<std::pair<std::string, std::string>> bad = {
        {R"("packAggro": 0)", "expected 0 < packAggro <= 30"},
        {R"("packAggro": 31)", "expected 0 < packAggro <= 30"},
        {R"("kite": {"min": 3})", "expected a number"},
        {R"("kite": {"min": 4, "max": 3})", "expected max > min"},
        {R"("kite": {"min": 0, "max": 3})", "expected 0 < min <= 30"},
        {R"("kite": {"min": 3, "max": 6, "band": 1})", "unknown kite field 'band'"},
        {R"("block": {"arcDeg": 0, "damageMul": 0.5})", "expected 0 < arcDeg <= 360"},
        {R"("block": {"arcDeg": 400, "damageMul": 0.5})", "expected 0 < arcDeg <= 360"},
        {R"("block": {"arcDeg": 100, "damageMul": 1})", "expected 0 <= damageMul < 1"},
        {R"("block": {"arcDeg": 100, "damageMul": -0.1})", "expected 0 <= damageMul < 1"},
        {R"("block": {"arcDeg": 100, "damageMul": 0.5, "wall": 0})", "expected 0 < wall <= 30"},
        {R"("block": {"arcDeg": 100})", "expected a number"},
        {R"("block": {"arcDeg": 100, "damageMul": 0.5, "front": true})", "unknown block field 'front'"},
        {R"("morale": {})", "expected a number"},
        {R"("morale": {"fleeOnLeaderDeathMs": 0})", "expected 0 < fleeOnLeaderDeathMs <= 60000"},
        {R"("morale": {"fleeOnLeaderDeathMs": 70000})", "expected 0 < fleeOnLeaderDeathMs <= 60000"},
        {R"("morale": {"fleeOnLeaderDeathMs": 5000, "sustainFamily": "x"})", "a sustain needs its sustainRadius"},
        {R"("morale": {"fleeOnLeaderDeathMs": 5000, "sustainFamily": "", "sustainRadius": 8})", "an empty family id"},
        {R"("morale": {"fleeOnLeaderDeathMs": 5000, "fleeRadius": -1})", "expected 0 < fleeRadius <= 30"},
        {R"("morale": {"fleeOnLeaderDeathMs": 5000, "sustainRadius": 31})", "expected 0 < sustainRadius <= 30"},
        {R"("morale": {"fleeOnLeaderDeathMs": 5000, "flee": 1})", "unknown morale field 'flee'"},
        {R"("aura": {"stat": "damage", "radius": 0})", "expected 0 < radius <= 30"},
        {R"("aura": {"stat": "damage", "mul": 0, "radius": 5})", "expected 0 < mul <= 10"},
        {R"("aura": {"stat": "damage", "mul": 11, "radius": 5})", "expected 0 < mul <= 10"},
        {R"("aura": {"stat": "damage", "radius": 5, "family": ""})", "an empty family id"},
        {R"("aura": {"stat": "damage", "radius": 5, "halo": 1})", "unknown aura field 'halo'"},
        {R"("abilities": [{"id": "c", "kind": "call", "clip": "Cast01", "cooldownMs": 0, "aim": "self"}])",
         "a call ability needs its 'buff' block"},
        {R"("abilities": [{"id": "c", "kind": "call", "clip": "Cast01", "cooldownMs": 0, "aim": "self",
            "buff": {"radius": 0}}])", "a call reaches radius > 0 tiles"},
        {R"("abilities": [{"id": "c", "kind": "call", "clip": "Cast01", "cooldownMs": 0, "aim": "self",
            "buff": {"radius": 5, "ms": 2000}}])", "a call buffs for `ms` only with a `stat`"},
        {R"("abilities": [{"id": "c", "kind": "call", "clip": "Cast01", "cooldownMs": 0, "aim": "self",
            "buff": {"radius": 5, "mul": 1.2}}])", "a call buffs by `mul` only with a `stat`"},
        {R"("abilities": [{"id": "b", "kind": "buff", "clip": "Cast01", "cooldownMs": 0, "aim": "self",
            "buff": {"stat": "damage", "mul": 11, "ms": 1000}}])", "expected 0 < mul <= 10"},
        {R"("abilities": [{"id": "b", "kind": "buff", "clip": "Cast01", "cooldownMs": 0, "aim": "self",
            "buff": {"stat": "damage", "ms": 700000}}])", "expected <= 600000"},
        {R"("abilities": [{"id": "b", "kind": "buff", "clip": "Cast01", "cooldownMs": 0, "aim": "self",
            "buff": {"stat": "damage", "ms": 0}}])", "expected > 0"},
        {R"("abilities": [{"id": "b", "kind": "buff", "clip": "Cast01", "cooldownMs": 0, "aim": "self",
            "buff": {"stat": "damage", "ms": 1000, "radius": 31}}])", "expected 0 <= radius <= 30"},
        {R"("abilities": [{"id": "b", "kind": "buff", "clip": "Cast01", "cooldownMs": 0, "aim": "self",
            "buff": {"stat": "damage", "ms": 1000, "family": ""}}])", "an empty family id"},
    };
    for (const auto& [extra, message] : bad) {
      INFO(extra);
      DataLoadReport report;
      auto store = K5LoadDef(K5DefJson("fx_k5_bad", extra), report);
      CHECK_FALSE(report.Ok());
      CHECK(K5HasIssue(report.errors, "fx_k5_bad", message));
    }
    // An unknown stat (aura or buff) and a buff without one fail on the stat.
    for (const std::string& extra : {std::string(R"("aura": {"stat": "luck", "radius": 5})"),
                                    std::string(R"("aura": {"radius": 5})"),
                                    std::string(R"("abilities": [{"id": "b", "kind": "buff", "clip": "Cast01",
                                      "cooldownMs": 0, "aim": "self", "buff": {"ms": 1000}}])")}) {
      INFO(extra);
      DataLoadReport report;
      auto store = K5LoadDef(K5DefJson("fx_k5_bad", extra), report);
      CHECK_FALSE(report.Ok());
      CHECK(K5HasIssue(report.errors, "stat", ""));
    }
  }

  TEST_CASE("K5 data: spawn mix and leader load; the loader refuses broken ones; Finalize checks sums, ids and leaders") {
    const DataStore& base = K5Data();
    const size_t i = K5PlainSpawn(base, 3);
    const MapDef* map = base.World().FindMap(base.World().defaultMap);
    const MapSpawnDef plain = map->spawns[i];
    const std::string id = plain.monsterId;
    const int32_t n = plain.count;
    const std::string mix = StrCat(R"([{"monsterId": ")", id, R"(", "count": )", n - 1,
                                   R"(}, {"monsterId": "fx_k5_chief", "count": 1}])");
    {
      DataLoadReport report;
      auto store = K5LoadEdited({{"assets.json", K5AddAsset},
                                 {"monsters.json", K5AddDefs},
                                 {"maps.json", K5EditSpawn(base, i, mix, R"({"monsterId": "fx_k5_chief", "elite": true})")}},
                                report);
      INFO(report.Summary());
      REQUIRE(report.Ok());
      const MapSpawnDef& s = store->World().FindMap(base.World().defaultMap)->spawns[i];
      REQUIRE(s.mix.size() == 2);
      CHECK((s.mix[0].monsterId == id && s.mix[0].count == n - 1));
      CHECK((s.mix[1].monsterId == "fx_k5_chief" && s.mix[1].count == 1));
      CHECK((s.leader.present && s.leader.monsterId == "fx_k5_chief" && s.leader.elite));
      CHECK((s.pos == plain.pos && s.count == n && s.monsterId == id));  // the entry itself is unchanged
      // Without mix / leader (every shipped entry): empty, absent.
      CHECK(map->spawns[i].mix.empty());
      CHECK_FALSE(map->spawns[i].leader.present);
      // A leader without a mix names the entry's own monster.
      DataLoadReport r2;
      auto s2 = K5LoadEdited({{"maps.json", K5EditSpawn(base, i, "", StrCat(R"({"monsterId": ")", id, R"("})"))}}, r2);
      INFO(r2.Summary());
      CHECK(r2.Ok());
    }
    const std::string story = base.Monsters().ai.storyBossId;  // read, never spelled
    REQUIRE(!story.empty());
    size_t storyEntry = map->spawns.size();
    for (size_t k = 0; k < map->spawns.size(); ++k) {
      if (map->spawns[k].monsterId == story) storyEntry = k;
    }
    REQUIRE(storyEntry < map->spawns.size());
    struct Bad {
      size_t entry;
      std::string mix, leader, path, message;
    };
    const std::vector<Bad> bad = {
        {i, StrCat(R"([{"monsterId": ")", id, R"(", "count": )", n, R"(}, {"monsterId": "fx_k5_chief", "count": 1}])"), "",
         ".mix", StrCat("the mix sums to ", n + 1, ", the entry's count is ", n)},
        {i, StrCat(R"([{"monsterId": "fx_k5_nobody", "count": )", n, "}]"), "", ".mix[0].monsterId",
         "unknown monster 'fx_k5_nobody'"},
        {i, StrCat(R"([{"monsterId": ")", id, R"(", "count": )", n - 1, R"(}, {"monsterId": ")", story,
                   R"(", "count": 1}])"),
         "", ".mix[1].monsterId", "the story boss keeps a spawn entry of its own"},
        {i, "", R"({"monsterId": "fx_k5_chief"})", ".leader", "'fx_k5_chief' is not a member of the pack"},
        {i, mix, StrCat(R"({"monsterId": ")", "fx_k5_member", R"("})"), ".leader", "is not a member of the pack"},
        {storyEntry, "", StrCat(R"({"monsterId": ")", story, R"("})"), ".leader",
         "the story boss keeps a spawn entry of its own"},
        {i, StrCat(R"([{"monsterId": ")", id, R"(", "count": 0}])"), "", ".mix", "expected >= 1"},
        {i, "[]", "", ".mix", "an empty mix"},
        {i, StrCat(R"([{"monsterId": ")", id, R"(", "count": )", n, R"(, "weight": 1}])"), "", ".mix",
         "unknown spawn mix field 'weight'"},
        {i, "", R"({"monsterId": "", "elite": true})", ".leader", "empty monster id"},
        {i, "", StrCat(R"({"monsterId": ")", id, R"(", "champion": true})"), ".leader", "unknown spawn leader field"},
    };
    for (const Bad& b : bad) {
      INFO(b.mix << " / " << b.leader);
      DataLoadReport report;
      auto store = K5LoadEdited({{"assets.json", K5AddAsset},
                                 {"monsters.json", K5AddDefs},
                                 {"maps.json", K5EditSpawn(base, b.entry, b.mix, b.leader)}},
                                report);
      CHECK_FALSE(report.Ok());
      CHECK(K5HasIssue(report.errors, StrCat("spawns[", b.entry, "]", b.path), b.message));
    }
    // A mixed entry's own monsterId names one of its members (the story-boss spawn and the quest guide read it).
    {
      const std::string memberMix = StrCat(R"([{"monsterId": "fx_k5_member", "count": )", n - 1,
                                           R"(}, {"monsterId": "fx_k5_chief", "count": 1}])");
      DataLoadReport stale;
      auto s1 = K5LoadEdited({{"assets.json", K5AddAsset},
                              {"monsters.json", K5AddDefs},
                              {"maps.json", K5EditSpawn(base, i, memberMix, "")}},
                             stale);
      CHECK_FALSE(stale.Ok());
      CHECK(K5HasIssue(stale.errors, StrCat("spawns[", i, "].monsterId"),
                       StrCat("'", id, "' is not in the entry's mix")));
      DataLoadReport named;
      auto s2 = K5LoadEdited({{"assets.json", K5AddAsset},
                              {"monsters.json", K5AddDefs},
                              {"maps.json", K5EditSpawn(base, i, memberMix, "", "fx_k5_chief")}},
                             named);
      INFO(named.Summary());
      CHECK(named.Ok());
    }
    // A sub-dungeon's spawns have no pack until K7: the loader refuses a mix or a leader there.
    for (const char* key : {"mix", "leader"}) {
      INFO(key);
      DataLoadReport report;
      auto store = K5LoadEdited({{"maps.json",
                                  [key](JsonValue& doc) {
                                    JsonValue* subs = doc.FindMutable("subDungeons");
                                    REQUIRE(subs != nullptr);
                                    REQUIRE(subs->Size() > 0);
                                    JsonValue* sd = &subs->MutableMembers()[0].value;
                                    JsonValue* spawns = sd->FindMutable("spawns");
                                    REQUIRE((spawns != nullptr && spawns->Size() > 0));
                                    JsonValue* entry = &spawns->MutableItems()[0];
                                    const std::string own(entry->Get("monsterId").AsString());
                                    const int32_t count = entry->Get("count").AsInt();
                                    entry->Set(key, std::string(key) == "mix"
                                                        ? K5Parse(StrCat(R"([{"monsterId": ")", own, R"(", "count": )",
                                                                         count, "}]"))
                                                        : K5Parse(StrCat(R"({"monsterId": ")", own, R"("})")));
                                  }}},
                                report);
      CHECK_FALSE(report.Ok());
      CHECK(K5HasIssue(report.errors, "subDungeons", "a sub-dungeon spawn has no pack (mix / leader) until K7"));
    }
  }

  TEST_CASE("K5 data: Finalize warns on a kite on a melee monster and on a kite band past the attack range") {
    DataLoadReport report;
    auto store = K5LoadEdited({{"assets.json", K5AddAsset},
                               {"monsters.json",
                                [](JsonValue& doc) {
                                  K5AddDefs(doc);
                                  K5AddDef(doc, K5DefJson("fx_k5_melee_kite", R"("kite": {"min": 1, "max": 1.5})"));
                                  K5AddDef(doc, K5DefJson("fx_k5_far_kite", R"("kite": {"min": 3, "max": 7.5})", 6.0));
                                  // Within the swing reach (6 x 1.2 = 7.2) but past the attack range: a kite runs in
                                  // Chase and ends in Attack only within attackRange, so this one walks back in.
                                  K5AddDef(doc, K5DefJson("fx_k5_reach_kite", R"("kite": {"min": 3, "max": 7.2})", 6.0));
                                  K5AddDef(doc, K5DefJson("fx_k5_hair_kite", R"("kite": {"min": 3, "max": 6.01})", 6.0));
                                  K5AddDef(doc, K5DefJson("fx_k5_edge_kite", R"("kite": {"min": 3, "max": 6})", 6.0));
                                  // A buff / call is a cast between swings: no reach warning past the swing reach.
                                  K5AddDef(doc, K5DefJson("fx_k5_far_drum", R"("abilities": [
                                    {"id": "drum", "kind": "call", "clip": "Cast01", "cooldownMs": 8000, "aim": "self",
                                     "rangeMin": 3, "rangeMax": 9, "buff": {"radius": 8}},
                                    {"id": "slam", "kind": "circle", "clip": "Attack01", "cooldownMs": 8000,
                                     "aim": "self", "rangeMin": 3, "shape": {"radius": 2}}])"));
                                }}},
                              report);
    INFO(report.Summary());
    REQUIRE(report.Ok());  // warnings only: the data loads
    CHECK(K5HasIssue(report.warnings, "defs.fx_k5_melee_kite.kite", "a kite is a ranged monster's rule"));
    CHECK(K5HasIssue(report.warnings, "defs.fx_k5_far_kite.kite.max", "past the attack range"));
    CHECK(K5HasIssue(report.warnings, "defs.fx_k5_reach_kite.kite.max", "walks back in before it shoots"));
    CHECK(K5HasIssue(report.warnings, "defs.fx_k5_hair_kite.kite.max", "past the attack range"));
    CHECK_FALSE(K5HasIssue(report.warnings, "defs.fx_k5_far_kite.kite", "ranged monster"));
    CHECK_FALSE(K5HasIssue(report.warnings, "defs.fx_k5_edge_kite", "kite"));  // max = attackRange: it ends in reach
    CHECK_FALSE(K5HasIssue(report.warnings, "defs.fx_k5_far_drum.abilities.drum", "swing"));
    CHECK(K5HasIssue(report.warnings, "defs.fx_k5_far_drum.abilities.slam.rangeMin", "never picked"));
  }

  // =================================================================================================================
  // Rules: monsters/Packs.h and the K5 steps of MonsterAI.h
  // =================================================================================================================
  TEST_CASE("K5 rules: spawn members and the leader; the block arc; blocked damage; the wall heading; buff multipliers") {
    // Members in mix order, the leader the first of its id; no mix: count x monsterId.
    MapSpawnDef s;
    s.monsterId = "a";
    s.count = 3;
    std::vector<SpawnMember> out;
    ExpandSpawnMembers(s, out);
    REQUIRE(out.size() == 3);
    CHECK((out[0].monsterId == "a" && !out[0].leader && !out[2].leader));
    s.mix = {{"b", 2}, {"c", 1}, {"b", 1}};
    s.count = 4;
    s.leader.present = true;
    s.leader.monsterId = "b";
    ExpandSpawnMembers(s, out);
    REQUIRE(out.size() == 4);
    CHECK((out[0].monsterId == "b" && out[1].monsterId == "b" && out[2].monsterId == "c" && out[3].monsterId == "b"));
    CHECK((out[0].leader && !out[1].leader && !out[2].leader && !out[3].leader));
    s.leader.monsterId = "c";
    ExpandSpawnMembers(s, out);
    CHECK((!out[0].leader && out[2].leader));
    s.leader.monsterId = "zzz";  // not a member: nobody leads
    ExpandSpawnMembers(s, out);
    for (const SpawnMember& m : out) CHECK_FALSE(m.leader);

    // Block arc: 100 degrees around the heading (+col): front, the edge, side, back; no direction; 360 = all round.
    const Vec2 pos(10, 10), head(1, 0);
    const auto at = [&](double deg) {
      const double r = deg * kPi / 180.0;
      return pos + Vec2(std::cos(r), std::sin(r)) * 2.0;
    };
    CHECK(BlockArcContains(pos, head, 100, at(0)));
    CHECK(BlockArcContains(pos, head, 100, at(49.5)));
    CHECK(BlockArcContains(pos, head, 100, at(-49.5)));
    CHECK_FALSE(BlockArcContains(pos, head, 100, at(50.5)));
    CHECK_FALSE(BlockArcContains(pos, head, 100, at(90)));
    CHECK_FALSE(BlockArcContains(pos, head, 100, at(180)));
    CHECK_FALSE(BlockArcContains(pos, head, 100, pos));            // no direction: not blocked
    CHECK_FALSE(BlockArcContains(pos, Vec2(), 100, at(0)));        // no heading: not blocked
    CHECK(BlockArcContains(pos, head, 360, at(180)));
    CHECK(BlockArcContains(pos, Vec2(0, 2), 100, pos + Vec2(0.5, 3)));  // an unnormalised heading
    // Blocked damage: floored, at least min(amount, 1).
    CHECK(BlockedDamage(100, 0.5) == 50);
    CHECK(BlockedDamage(15, 0.5) == 7);
    CHECK(BlockedDamage(1, 0.5) == 1);
    CHECK(BlockedDamage(40, 0) == 1);
    CHECK(BlockedDamage(0, 0.5) == 0);
    // Wall heading: the mean; opposite headings fall back to the first; none -> +col.
    const std::array<Vec2, 3> hs{Vec2(1, 0), Vec2(0, 1), Vec2(-1, 0)};
    CHECK(WallHeading(hs) == Vec2(0, 1));
    const std::array<Vec2, 2> opposed{Vec2(0, -1), Vec2(0, 1)};
    CHECK(WallHeading(opposed) == Vec2(0, -1));
    const std::array<Vec2, 2> two{Vec2(1, 0), Vec2(0, 1)};
    const Vec2 w = WallHeading(two);
    CHECK((w.x == doctest::Approx(std::sqrt(0.5)) && w.y == doctest::Approx(std::sqrt(0.5))));
    CHECK(WallHeading(std::span<const Vec2>()) == Vec2(1, 0));

    // Buffs: the largest live one of a stat (and the aura) applies; expired ones do not; none -> 1.
    std::vector<MonsterAbilityBuff> buffs = {{MonsterBuffStat::Damage, 1.2, 1000, 1},
                                             {MonsterBuffStat::Damage, 1.1, 3000, 2},
                                             {MonsterBuffStat::AttackSpeed, 1.25, 3000, 1},
                                             {MonsterBuffStat::FearImmune, 1, 2000, 1}};
    CHECK(BuffMultiplier(buffs, MonsterBuffStat::Damage, 500, 1, false) == 1.2);
    CHECK(BuffMultiplier(buffs, MonsterBuffStat::Damage, 1000, 1, false) == 1.1);  // the first ran out (untilMs <= now)
    CHECK(BuffMultiplier(buffs, MonsterBuffStat::Damage, 500, 1.15, true) == 1.2);
    CHECK(BuffMultiplier(buffs, MonsterBuffStat::Damage, 1500, 1.15, true) == 1.15);
    CHECK(BuffMultiplier(buffs, MonsterBuffStat::Damage, 5000, 1, false) == 1);
    CHECK(BuffMultiplier(buffs, MonsterBuffStat::MoveSpeed, 0, 1, false) == 1);
    CHECK(BuffMultiplier(buffs, MonsterBuffStat::MoveSpeed, 0, 0.8, true) == 0.8);  // a lone aura applies as it is
    CHECK(HasLiveBuff(buffs, MonsterBuffStat::FearImmune, 1999));
    CHECK_FALSE(HasLiveBuff(buffs, MonsterBuffStat::FearImmune, 2000));
    // On an instance: the step's multipliers, the mask, pruning.
    MonsterInstance m = MMake(K5Melee(), Vec2(10, 10));
    m.abilityBuffs = buffs;
    m.auraMul[static_cast<size_t>(MonsterBuffStat::MoveSpeed)] = 1.3;
    m.auraMask = 1u << static_cast<uint32_t>(MonsterBuffStat::MoveSpeed);
    RefreshMonsterBuffs(m, 1500);
    CHECK(m.abilityBuffs.size() == 3);  // the damage x1.2 ran out at 1000
    CHECK((m.buffDamageMul == 1.1 && m.buffAttackSpeedMul == 1.25 && m.buffMoveSpeedMul == 1.3 && m.fearImmune));
    CHECK(m.AttackIntervalMs() == doctest::Approx(1200 / 1.25));
    CHECK(MonsterBuffMask(m) == 0xFu);
    CHECK(MonsterDamageBuffMul(m, 1500) == 1.1);
    CHECK(MonsterFearImmune(m, 1500));
    CHECK_FALSE(MonsterFearImmune(m, 2500));
    m.auraMask = 0;
    RefreshMonsterBuffs(m, 4000);
    CHECK((m.abilityBuffs.empty() && m.buffDamageMul == 1 && m.buffAttackSpeedMul == 1 && m.buffMoveSpeedMul == 1 &&
           !m.fearImmune && MonsterBuffMask(m) == 0));
    CHECK(m.AttackIntervalMs() == 1200);
    // Families and sustain casters.
    MonsterDef living = K5Melee();
    CHECK(FamilyMatches("", living));
    CHECK(FamilyMatches("fx_living", living));
    CHECK_FALSE(FamilyMatches("fx_unburied", living));
    CHECK(GrantsFearImmunity(K5Drummer()));
    CHECK_FALSE(GrantsFearImmunity(living));
    MonsterDef caller = K5Melee();
    caller.abilities = {K5Cast("bell", AbilityKind::Call, 8000, 10)};  // a call without a stat sustains nobody
    CHECK_FALSE(GrantsFearImmunity(caller));
    caller.abilities = {K5Buff(K5Cast("march", AbilityKind::Buff, 8000, 8), MonsterBuffStat::AttackSpeed, 1.1, 2000)};
    CHECK_FALSE(GrantsFearImmunity(caller));
    caller.abilities = {K5Buff(K5Cast("ward", AbilityKind::Buff, 8000, 8), MonsterBuffStat::FearImmune, 1, 2000)};
    CHECK(GrantsFearImmunity(caller));
    // The kinds: every one runs; buff / call cast between swings like raise / summon, and stand nothing up.
    for (AbilityKind k : {AbilityKind::Raise, AbilityKind::Summon, AbilityKind::Buff, AbilityKind::Call}) {
      CHECK(AbilityKindRuns(k));
      CHECK(AbilityKindCastsBetweenSwings(k));
    }
    for (AbilityKind k : {AbilityKind::Circle, AbilityKind::Cone, AbilityKind::Line, AbilityKind::Ring, AbilityKind::Bolt,
                          AbilityKind::Hazard}) {
      CHECK_FALSE(AbilityKindCastsBetweenSwings(k));
    }
    // The cast between swings picks only those kinds; a due swing picks a buff / call like any ability.
    std::vector<MonsterAbilityDef> abilities = {
        K5Buff(K5Cast("drum", AbilityKind::Call, 8000, 8), MonsterBuffStat::FearImmune, 1, 3000)};
    abilities.insert(abilities.begin(), MonsterAbilityDef{});
    abilities[0].id = "slam";
    abilities[0].kind = AbilityKind::Circle;
    abilities[0].hasShape = true;
    abilities[0].shape.radius = 2;
    abilities[0].priority = 1;
    std::vector<double> ready(2, 0.0);
    AbilityPickInput in;
    in.readyAtMs = ready;
    CHECK(PickMonsterAbility(abilities, in) == 0);
    in.castsOnly = true;
    CHECK(PickMonsterAbility(abilities, in) == 1);
  }

  TEST_CASE("K5 rules: the kite target and the flee step (away, turned, bounded by the leash, the tether and walls)") {
    const MonsterAiDef& ai = K5Data().Monsters().ai;
    MonsterInstance m = MMake(K5Archer(), Vec2(40, 40));
    Vec2 out;
    // Open ground: max tiles from the hero along the away vector.
    REQUIRE(MonsterKiteTarget(m, ai, Vec2(41.5, 40), 1.5, MOpenWorld(), out));
    CHECK(out == Vec2(35.5, 40));
    // A corridor along row 40 (cols 37..44): straight back only, shortened by its end wall.
    const auto corridor = [](int32_t firstCol) {
      auto g = std::make_shared<MTestGrid>(120, 120);
      for (int32_t c = 0; c < 120; ++c) {
        for (int32_t r = 0; r < 120; ++r) {
          if (!(r == 40 && c >= firstCol && c <= 44)) g->Block(c, r);
        }
      }
      return g;
    };
    REQUIRE(MonsterKiteTarget(m, ai, Vec2(41.5, 40), 1.5, MGridWorld(corridor(37), false), out));
    CHECK(out == Vec2(36.5, 40));  // 35.5 / 36 lie on tile 36 (a wall); 36.5 rounds onto 37
    // The leash bounds it (the anchor 4 tiles toward the hero: 35.5 would be 8.5 from it).
    m.spawnAnchor = TilePos{44, 40};
    REQUIRE(MonsterKiteTarget(m, ai, Vec2(41.5, 40), 1.5, MGridWorld(corridor(30), false), out));
    CHECK(out == Vec2(36, 40));
    // In the open the leash turns it instead (60 degrees about the hero, still max from it, inside the leash).
    REQUIRE(MonsterKiteTarget(m, ai, Vec2(41.5, 40), 1.5, MOpenWorld(), out));
    CHECK(Dist(out, Vec2(41.5, 40)) == doctest::Approx(6));
    CHECK(DistSq(out, Vec2(44, 40)) <= 64);
    CHECK(out.x == doctest::Approx(38.5));
    m.spawnAnchor = TilePos{40, 40};
    // The tether (K4) bounds it too.
    m.hasTether = true;
    m.tetherCentre = Vec2(40, 40);
    m.tetherRadius = 3;
    REQUIRE(MonsterKiteTarget(m, ai, Vec2(41.5, 40), 1.5, MGridWorld(corridor(30), false), out));
    CHECK(out == Vec2(37, 40));
    m.hasTether = false;
    // Straight back blocked right behind it: the turn about the hero that clears the block, at max.
    auto block = std::make_shared<MTestGrid>(120, 120);
    for (int32_t c = 38; c <= 39; ++c) {
      for (int32_t r = 39; r <= 41; ++r) block->Block(c, r);
    }
    REQUIRE(MonsterKiteTarget(m, ai, Vec2(41.5, 40), 1.5, MGridWorld(block, false), out));
    CHECK(Dist(out, Vec2(41.5, 40)) == doctest::Approx(6));
    CHECK(out.x == doctest::Approx(38.5));
    // Its back to the end of a dead end (the corridor from col 40): no point gains distance (it fights).
    MonsterInstance cornered = MMake(K5Archer(), Vec2(39.6, 40));
    CHECK_FALSE(MonsterKiteTarget(cornered, ai, Vec2(41.5, 40), 1.9, MGridWorld(corridor(40), false), out));
    // No gain possible: the hero already stands near max.
    CHECK_FALSE(MonsterKiteTarget(m, ai, Vec2(45.8, 40), 5.8, MOpenWorld(), out));

    // Flee: away from the body; turned when the way is blocked; none -> false.
    MonsterInstance f = MMake(K5Member(), Vec2(40, 40));
    f.fleeFrom = Vec2(38, 40);
    Vec2 dir;
    REQUIRE(MonsterFleeStep(f, ai, Vec2(60, 60), true, MOpenWorld(), dir));
    CHECK(dir == Vec2(1, 0));
    auto ahead = std::make_shared<MTestGrid>(120, 120);
    for (int32_t r = 30; r <= 50; ++r) ahead->Block(41, r);
    REQUIRE(MonsterFleeStep(f, ai, Vec2(60, 60), true, MGridWorld(ahead, false), dir));
    CHECK((dir.x == 0 && dir.y == 1));  // the 90-degree turn (left first): the 45s run into the column
    auto cell = std::make_shared<MTestGrid>(120, 120);
    for (const TilePos t : {TilePos{41, 39}, TilePos{41, 40}, TilePos{41, 41}, TilePos{40, 39}, TilePos{40, 41}}) {
      cell->Block(t.col, t.row);
    }
    CHECK_FALSE(MonsterFleeStep(f, ai, Vec2(60, 60), true, MGridWorld(cell, false), dir));
    // A probe longer than a one-tile wall lands on open ground past it: the straight walk is still closed (the
    // 45-degree turn passes the wall's corner).
    MonsterAiDef longProbe = ai;
    longProbe.fleeProbeTiles = 3;
    auto post = std::make_shared<MTestGrid>(120, 120);
    post->Block(41, 40);
    REQUIRE(MonsterFleeStep(f, longProbe, Vec2(60, 60), true, MGridWorld(post, false), dir));
    CHECK((dir.x == doctest::Approx(std::sqrt(0.5)) && dir.y == doctest::Approx(std::sqrt(0.5))));
    REQUIRE(MonsterFleeStep(f, longProbe, Vec2(60, 60), true, MOpenWorld(), dir));
    CHECK(dir == Vec2(1, 0));
    // On the body itself: away from the hero.
    f.fleeFrom = f.pos;
    REQUIRE(MonsterFleeStep(f, ai, Vec2(40, 45), true, MOpenWorld(), dir));
    CHECK(dir == Vec2(0, -1));
    // The leash: one tile out to its edge is allowed; past it the step turns along it; on the edge with nowhere along
    // it that gains distance, none (it fights).
    f.fleeFrom = Vec2(38, 40);
    f.spawnAnchor = TilePos{33, 40};  // 7 tiles behind: one more tile out is 8 (the edge) -> allowed
    REQUIRE(MonsterFleeStep(f, ai, Vec2(60, 60), true, MOpenWorld(), dir));
    CHECK(dir == Vec2(1, 0));
    f.spawnAnchor = TilePos{33, 41};  // straight out is 8.06 from it: the 45-degree turn (7.71) is taken
    REQUIRE(MonsterFleeStep(f, ai, Vec2(60, 60), true, MOpenWorld(), dir));
    CHECK((dir.x == doctest::Approx(std::sqrt(0.5)) && dir.y == doctest::Approx(std::sqrt(0.5))));
    f.spawnAnchor = TilePos{32, 40};  // on the edge, the body between it and the anchor
    CHECK_FALSE(MonsterFleeStep(f, ai, Vec2(60, 60), true, MOpenWorld(), dir));
    f.spawnAnchor = TilePos{40, 40};

    // A camp's safe zone (MonsterWorld::safeZone) is closed to both: the kite turns about the hero until its point and
    // its line stay outside; the flee turns until its probe does (a strict circle, as ZoneRuntime::InSafeZone).
    const auto camp = [](Vec2 c, double r) {
      MonsterWorld w = MOpenWorld();
      w.safeZone = [c, r](Vec2 p) { return DistSq(p, c) < r * r; };
      return w;
    };
    const MonsterWorld west = camp(Vec2(30, 40), 9);  // (35.5, 40) and the 30-degree turns lie inside it
    REQUIRE(MonsterKiteTarget(m, ai, Vec2(41.5, 40), 1.5, west, out));
    CHECK(Dist(out, Vec2(41.5, 40)) == doctest::Approx(6));
    CHECK(out.x == doctest::Approx(38.5));  // the 60-degree turn
    CHECK_FALSE(west.safeZone(out));
    // A camp right behind it and around: no point outside it gains distance (it fights).
    CHECK_FALSE(MonsterKiteTarget(m, ai, Vec2(41.5, 40), 1.5, camp(Vec2(36, 40), 7.5), out));
    // A point outside the camp whose straight line crosses it is refused too (straight back: (35.5, 40) beyond a small
    // camp at (37.5, 40)); the turns pass it.
    const MonsterWorld between = camp(Vec2(37.5, 40), 1);
    REQUIRE(MonsterKiteTarget(m, ai, Vec2(41.5, 40), 1.5, between, out));
    CHECK(out.y != 40);
    CHECK_FALSE(between.safeZone(out));
    f.fleeFrom = Vec2(38, 40);
    const MonsterWorld east = camp(Vec2(50, 40), 9.5);  // (41, 40) and the 45-degree probes lie inside it
    REQUIRE(MonsterFleeStep(f, ai, Vec2(60, 60), true, east, dir));
    CHECK((dir.x == 0 && dir.y == 1));  // the 90-degree turn
    CHECK_FALSE(MonsterFleeStep(f, ai, Vec2(60, 60), true, camp(Vec2(41, 40), 1.6), dir));
    // It never doubles back on its last flee step (turned more than 120 degrees from it): at a curved edge the two
    // sidesteps would take turns being open and cancel out. Its last step went -row: the +row sidestep is skipped.
    f.fleeDir = Vec2(0, -1);
    REQUIRE(MonsterFleeStep(f, ai, Vec2(60, 60), true, east, dir));
    CHECK((dir.x == 0 && dir.y == -1));
    f.fleeDir = Vec2(-std::sqrt(0.5), -std::sqrt(0.5));  // 135 degrees from +row, 45 from -row
    REQUIRE(MonsterFleeStep(f, ai, Vec2(60, 60), true, east, dir));
    CHECK((dir.x == 0 && dir.y == -1));
    f.fleeDir = Vec2(-0.5, std::sqrt(0.75));  // 120 degrees from straight on (the edge counts: allowed)
    REQUIRE(MonsterFleeStep(f, ai, Vec2(60, 60), true, MOpenWorld(), dir));
    CHECK(dir == Vec2(1, 0));
    f.fleeDir = Vec2();
  }

  // =================================================================================================================
  // Runtime: packs through the zone population and the hunts
  // =================================================================================================================
  TEST_CASE("K5 runtime: a map entry spawns its mix as one pack with its leader; the placements stay those of the entry") {
    const DataStore& base = K5Data();
    const size_t i = K5PlainSpawn(base, 3);
    const MapSpawnDef plain = base.World().FindMap(base.World().defaultMap)->spawns[i];
    const std::string mix = StrCat(R"([{"monsterId": "fx_k5_chief", "count": 1}, {"monsterId": ")", plain.monsterId,
                                   R"(", "count": )", plain.count - 1, "}]");
    DataLoadReport r1, r2;
    auto mixed = K5LoadEdited({{"assets.json", K5AddAsset},
                               {"monsters.json", K5AddDefs},
                               {"maps.json", K5EditSpawn(base, i, mix, R"({"monsterId": "fx_k5_chief"})")}},
                              r1);
    auto elite = K5LoadEdited({{"assets.json", K5AddAsset},
                               {"monsters.json", K5AddDefs},
                               {"maps.json", K5EditSpawn(base, i, mix, R"({"monsterId": "fx_k5_chief", "elite": true})")}},
                              r2);
    REQUIRE((r1.Ok() && r2.Ok()));
    K5Rig a(4242, base), b(4242, *mixed), c(4242, *elite);
    for (K5Rig* rig : {&a, &b, &c}) rig->monsters.SpawnZonePopulation();
    const auto all = [](const K5Rig& rig) {
      std::vector<const MonsterInstance*> out;
      for (const MonsterInstance& m : rig.monsters.All()) out.push_back(&m);
      return out;
    };
    const auto ma = all(a), mb = all(b);
    REQUIRE(ma.size() == mb.size());
    // Golden-neutral: every monster stands where the plain entry put it (the same M10 draws, in the same order).
    size_t entryStart = ma.size();
    for (size_t k = 0; k < ma.size(); ++k) {
      CHECK(ma[k]->pos == mb[k]->pos);
      CHECK(ma[k]->spawnAnchor == mb[k]->spawnAnchor);
      CHECK(ma[k]->homeTile == mb[k]->homeTile);
      if (ma[k]->def.id != mb[k]->def.id && entryStart == ma.size()) entryStart = k;
    }
    REQUIRE(entryStart < ma.size());
    // The mix in list order; the leader is its first chief; one pack id per entry.
    CHECK(mb[entryStart]->def.id == "fx_k5_chief");
    CHECK(mb[entryStart]->packLeader);
    CHECK_FALSE(mb[entryStart]->def.elite);
    for (int32_t k = 1; k < plain.count; ++k) {
      CHECK(mb[entryStart + static_cast<size_t>(k)]->def.id == plain.monsterId);
      CHECK_FALSE(mb[entryStart + static_cast<size_t>(k)]->packLeader);
      CHECK(mb[entryStart + static_cast<size_t>(k)]->packId == mb[entryStart]->packId);
    }
    // The edit adds exactly one leader (the shipped data's own leaders, the dark remake's W2 packs, lead in both runs).
    int leaders = 0, plainLeaders = 0;
    std::vector<uint32_t> packs;
    for (const MonsterInstance* m : mb) {
      leaders += m->packLeader ? 1 : 0;
      CHECK(m->packId != 0);  // every map spawn is a pack
      if (std::find(packs.begin(), packs.end(), m->packId) == packs.end()) packs.push_back(m->packId);
    }
    for (const MonsterInstance* m : ma) plainLeaders += m->packLeader ? 1 : 0;
    CHECK(leaders == plainLeaders + 1);
    // One pack per entry that spawned (entries spawn contiguously).
    int entries = 0;
    for (size_t k = 0; k < mb.size(); ++k) entries += k == 0 || mb[k]->packId != mb[k - 1]->packId ? 1 : 0;
    CHECK(entries == static_cast<int>(packs.size()));
    // Nothing else changed: the plain run has packs too, and the plain entry no leader.
    for (int32_t k = 0; k < plain.count; ++k) CHECK_FALSE(ma[entryStart + static_cast<size_t>(k)]->packLeader);
    // An elite leader rolls the zone's affixes right after its placement (an elite definition's rule): on the Ai stream,
    // so the placements up to and including its own stay, and the zone's later ones move (as they would for an elite
    // definition; a mix alone moves nothing).
    const auto mc = all(c);
    REQUIRE(mc.size() == ma.size());
    for (size_t k = 0; k <= entryStart; ++k) CHECK(mc[k]->pos == ma[k]->pos);  // up to and including its placement
    REQUIRE(entryStart + static_cast<size_t>(plain.count) < mc.size());       // entries spawn after it
    int moved = 0;
    for (size_t k = entryStart + 1; k < mc.size(); ++k) moved += mc[k]->pos != ma[k]->pos ? 1 : 0;
    CHECK(moved > 0);
    CHECK(mc[entryStart]->def.id == "fx_k5_chief");
    CHECK((mc[entryStart]->def.elite && mc[entryStart]->packLeader));
    CHECK(mc[entryStart]->AffixCount() >= 1);
    CHECK(mc[entryStart]->originalDef.elite);  // a respawn re-rolls them
  }

  TEST_CASE("K5 runtime: a hunt's leader leads its minions; a respawned monster keeps its pack and leadership") {
    K5Rig rig;
    const HuntDef* hunt = nullptr;
    for (const HuntDef& h : rig.h.ctx.data.Monsters().hunts) {
      if (h.hasMinions && h.minionCount > 0 && hunt == nullptr) hunt = &h;
    }
    REQUIRE(hunt != nullptr);
    const DueHunt due{nullptr, hunt};
    rig.monsters.SpawnHunts(std::span<const DueHunt>(&due, 1), false);
    EntityId leader = kNoEntity;
    std::vector<EntityId> minions;
    for (const MonsterInstance& m : rig.monsters.All()) {
      if (m.role == MonsterRole::HuntLeader) leader = m.id;
      if (m.role == MonsterRole::HuntMinion) minions.push_back(m.id);
    }
    REQUIRE(leader != kNoEntity);
    REQUIRE(!minions.empty());
    CHECK(rig.M(leader).packLeader);
    CHECK(rig.M(leader).packId != 0);
    for (EntityId id : minions) {
      CHECK(rig.M(id).packId == rig.M(leader).packId);
      CHECK_FALSE(rig.M(id).packLeader);
    }
    // A regular pack leader killed and respawned (M3) leads its pack again.
    const uint32_t pack = rig.monsters.NewPackId();
    const EntityId chief = rig.Spawn(K5Melee("fx_k5_chief"), Vec2(30, 30), MonsterState::Idle, pack, true);
    rig.Kill(chief);
    rig.Step(static_cast<int>(rig.h.ctx.data.Monsters().ai.respawnDelayMs / kSimStepMs) + 2);
    EntityId again = kNoEntity;
    for (const MonsterInstance& m : rig.monsters.All()) {
      if (m.def.id == "fx_k5_chief" && m.IsAlive()) again = m.id;
    }
    REQUIRE(again != kNoEntity);
    CHECK(again != chief);
    CHECK(rig.M(again).packId == pack);
    CHECK(rig.M(again).packLeader);
  }

  TEST_CASE("K5 runtime: a hunt's minions with morale break when their leader falls") {
    const DataStore& base = K5Data();
    const HuntDef* hunt = nullptr;
    for (const HuntDef& h : base.Monsters().hunts) {
      if (h.hasMinions && h.minionCount > 0 && hunt == nullptr) hunt = &h;
    }
    REQUIRE(hunt != nullptr);
    const std::string minion = hunt->minionMonsterId;  // read, never spelled
    DataLoadReport report;
    auto store = K5LoadEdited({{"assets.json", K5AddAsset},
                               {"monsters.json",
                                [minion](JsonValue& doc) {
                                  K5AddDefs(doc);
                                  JsonValue* def = doc.FindMutable("defs")->FindMutable(minion);
                                  REQUIRE(def != nullptr);
                                  def->Set("morale", K5Parse(R"({"fleeOnLeaderDeathMs": 4000})"));
                                }}},
                              report);
    INFO(report.Summary());
    REQUIRE(report.Ok());
    K5Rig rig(7, *store);
    const HuntDef* h2 = store->Monsters().FindHunt(hunt->huntId);
    REQUIRE(h2 != nullptr);
    const DueHunt due{nullptr, h2};
    rig.monsters.SpawnHunts(std::span<const DueHunt>(&due, 1), false);
    EntityId leader = kNoEntity;
    int minions = 0;
    for (const MonsterInstance& m : rig.monsters.All()) {
      if (m.role == MonsterRole::HuntLeader) leader = m.id;
      minions += m.role == MonsterRole::HuntMinion ? 1 : 0;
    }
    REQUIRE(leader != kNoEntity);
    REQUIRE(minions > 0);
    rig.h.events.Clear();
    rig.Kill(leader);
    const std::vector<EvMonsterFlee> fled = K5Events<EvMonsterFlee>(rig.h.events);
    CHECK(static_cast<int>(fled.size()) == minions);
    for (const EvMonsterFlee& f : fled) {
      CHECK((f.leader == leader && !f.ended && f.untilMs == rig.Now() + 4000));
      CHECK(rig.M(f.entity).state == MonsterState::Flee);
    }
  }

  // =================================================================================================================
  // Runtime: pack aggro
  // =================================================================================================================
  TEST_CASE("K5 runtime: pack aggro: a provoked member wakes its pack within each member's packAggro, once (no chain)") {
    K5Rig rig;
    rig.PlaceHero(Vec2(60, 40));
    const uint32_t pack = rig.monsters.NewPackId(), other = rig.monsters.NewPackId();
    const EntityId a = rig.Spawn(K5Member(), Vec2(40, 40), MonsterState::Idle, pack);
    const EntityId b = rig.Spawn(K5Member(), Vec2(44, 40), MonsterState::Idle, pack);    // 4 from a
    const EntityId c = rig.Spawn(K5Member(), Vec2(48, 40), MonsterState::Idle, pack);    // 8 from a, 4 from b
    const EntityId d = rig.Spawn(K5Melee(), Vec2(42, 40), MonsterState::Idle, pack);     // no packAggro
    const EntityId e = rig.Spawn(K5Member(), Vec2(41, 40), MonsterState::Idle, other);   // another pack
    const EntityId f = rig.Spawn(K5Member(4), Vec2(40, 45), MonsterState::Idle, pack);   // 5 from a, its reach 4
    const EntityId g = rig.Spawn(K5Member(), Vec2(40, 34.5), MonsterState::Idle, pack);  // 5.5 from a
    rig.Hit(a, 5, std::nullopt, /*provokes=*/true);
    CHECK(rig.M(a).state == MonsterState::Chase);
    CHECK(rig.M(b).state == MonsterState::Chase);
    CHECK(rig.M(b).provokedUntilMs == doctest::Approx(rig.Now() + rig.h.ctx.data.Monsters().ai.provokeDurationMs));
    CHECK(rig.M(g).state == MonsterState::Chase);
    CHECK(rig.M(c).state == MonsterState::Idle);  // only within reach of the provoked member: b passes nothing on
    CHECK(rig.M(d).state == MonsterState::Idle);  // keeps to itself
    CHECK(rig.M(e).state == MonsterState::Idle);  // another pack
    CHECK(rig.M(f).state == MonsterState::Idle);  // beyond its own packAggro (4)
    CHECK(rig.aggro == std::vector<EntityId>{a, b, g});  // MonsterAggroMsg in order: the member, then its pack
    // A provoke that keeps a member in Chase (a hit from beyond its aggro range) alarms again: b / g refreshed, no
    // MonsterAggroMsg (they were already chasing), c still idle.
    rig.aggro.clear();
    rig.Step(30);
    const double before = rig.M(b).provokedUntilMs;
    rig.Hit(a, 5, std::nullopt, true);
    CHECK(rig.M(b).provokedUntilMs > before);
    CHECK(rig.M(c).state == MonsterState::Idle);
    CHECK(rig.aggro.empty());
    // Nobody with packAggro: nothing (no RNG, no state change).
    const uint32_t quiet = rig.monsters.NewPackId();
    const EntityId q1 = rig.Spawn(K5Melee(), Vec2(70, 70), MonsterState::Idle, quiet);
    const EntityId q2 = rig.Spawn(K5Melee(), Vec2(71, 70), MonsterState::Idle, quiet);
    const EntityId q3 = rig.Spawn(K5Melee(), Vec2(70, 70), MonsterState::Idle, quiet);  // on q1's spot
    rig.Hit(q1, 5, std::nullopt, true);
    CHECK(rig.M(q2).state == MonsterState::Idle);
    CHECK(rig.M(q3).state == MonsterState::Idle);
    // A monster without a pack alarms nobody.
    const EntityId lone = rig.Spawn(K5Member(), Vec2(75, 75), MonsterState::Idle);
    const EntityId near = rig.Spawn(K5Member(), Vec2(76, 75), MonsterState::Idle);
    rig.Hit(lone, 5, std::nullopt, true);
    CHECK(rig.M(near).state == MonsterState::Idle);
  }

  TEST_CASE("K5 runtime: pack aggro on entering Chase (the AI's aggro scan) and on a forced chase") {
    K5Rig rig;
    rig.PlaceHero(Vec2(60, 60));
    const uint32_t pack = rig.monsters.NewPackId();
    const EntityId f = rig.Spawn(K5Member(), Vec2(40, 50), MonsterState::Idle, pack);
    const EntityId g = rig.Spawn(K5Member(), Vec2(45, 51), MonsterState::Idle, pack);  // 5.1 from f, 7.07 from the hero
    rig.Step();
    CHECK(rig.M(f).state == MonsterState::Idle);
    rig.PlaceHero(Vec2(40, 46));  // within f's aggro range (5), outside g's
    rig.Step();
    CHECK(rig.M(f).state == MonsterState::Chase);
    CHECK(rig.M(g).state == MonsterState::Chase);
    CHECK(rig.M(g).provokedUntilMs > rig.Now());  // provoked (M2), not by its own scan
    // ForceChase (a taunt, the mini-boss dialogue's dismissal) alarms too.
    const uint32_t p2 = rig.monsters.NewPackId();
    const EntityId h1 = rig.Spawn(K5Member(), Vec2(80, 20), MonsterState::Idle, p2);
    const EntityId h2 = rig.Spawn(K5Member(), Vec2(83, 20), MonsterState::Idle, p2);
    rig.monsters.ForceChase(h1);
    CHECK(rig.M(h1).state == MonsterState::Chase);
    CHECK(rig.M(h2).state == MonsterState::Chase);
    // A returning (M1) member ignores the alarm.
    const uint32_t p3 = rig.monsters.NewPackId();
    const EntityId r1 = rig.Spawn(K5Member(), Vec2(20, 80), MonsterState::Idle, p3);
    const EntityId r2 = rig.Spawn(K5Member(), Vec2(22, 80), MonsterState::Returning, p3);
    rig.Hit(r1, 5, std::nullopt, true);
    CHECK(rig.M(r2).state == MonsterState::Returning);
  }

  // =================================================================================================================
  // Runtime: morale
  // =================================================================================================================
  TEST_CASE("K5 runtime: the leader's death breaks the pack: flee for its time, away from the body, no swing; then fight") {
    K5Rig rig;
    rig.PlaceHero(Vec2(43, 41));
    const uint32_t pack = rig.monsters.NewPackId();
    MonsterDef memberDef = K5Member(0, 3000);
    memberDef.morale.fleeRadius = 8;
    const EntityId leader = rig.Spawn(K5Melee("fx_k5_chief"), Vec2(40, 40), MonsterState::Idle, pack, true);
    const EntityId m1 = rig.Spawn(memberDef, Vec2(42.2, 40.2), MonsterState::Attack, pack);  // next to the hero
    const EntityId m2 = rig.Spawn(memberDef, Vec2(40, 44), MonsterState::Idle, pack);
    const EntityId m3 = rig.Spawn(memberDef, Vec2(52, 40), MonsterState::Idle, pack);       // 12 from the body
    const EntityId m4 = rig.Spawn(K5Melee(), Vec2(39, 40), MonsterState::Idle, pack);       // no morale
    const EntityId m5 = rig.Spawn(memberDef, Vec2(36, 45), MonsterState::Idle, pack);       // walking home (M1)
    // m1 starts a swing; the leader dies before its contact: the swing is cancelled, m1 flees.
    REQUIRE(rig.StepUntil([&]() { return K5Swings(rig.h.events, m1) > 0; }, 120));
    REQUIRE(rig.combat.HasPendingStrike(m1));
    rig.M(m5).spawnAnchor = TilePos{20, 42};
    rig.M(m5).state = MonsterState::Returning;
    rig.h.events.Clear();
    const double t0 = rig.Now();
    const Vec2 body = rig.M(leader).pos;
    rig.Kill(leader);
    CHECK_FALSE(rig.combat.HasPendingStrike(m1));
    const std::vector<EvMonsterFlee> fled = K5Events<EvMonsterFlee>(rig.h.events);
    REQUIRE(fled.size() == 2);  // list order
    CHECK((fled[0].entity == m1 && fled[1].entity == m2));
    for (const EvMonsterFlee& f : fled) {
      CHECK((f.leader == leader && f.from == body && f.startMs == t0 && f.untilMs == t0 + 3000 && !f.ended));
    }
    bool cancelled = false;
    for (const EvMonsterAttackCancelled& c : K5Events<EvMonsterAttackCancelled>(rig.h.events)) cancelled |= c.monster == m1;
    CHECK(cancelled);
    CHECK(rig.M(m1).state == MonsterState::Flee);
    CHECK(rig.M(m2).state == MonsterState::Flee);
    CHECK(rig.M(m3).state == MonsterState::Idle);  // beyond its fleeRadius
    CHECK(rig.M(m4).state != MonsterState::Flee);  // no morale: it fights on
    CHECK(rig.M(m5).state == MonsterState::Returning);  // one walking home keeps walking (M1)
    Snapshot snap = rig.View();
    REQUIRE(rig.ViewOf(snap, m1) != nullptr);
    CHECK((rig.ViewOf(snap, m1)->state == MonsterState::Flee && rig.ViewOf(snap, m1)->fleeUntilMs == t0 + 3000));
    // It runs away from the body (m1 along +col, m2 along +row), and never swings, though the hero stands by.
    const Vec2 p1 = rig.M(m1).pos, p2 = rig.M(m2).pos;
    rig.h.events.Clear();
    rig.Step(60);
    CHECK(Dist(rig.M(m1).pos, body) > Dist(p1, body) + 0.5);
    CHECK(rig.M(m1).pos.x > p1.x + 0.5);
    CHECK(rig.M(m2).pos.y > p2.y + 0.5);
    CHECK(std::fabs(rig.M(m2).pos.x - 40) < 0.2);
    CHECK(K5Swings(rig.h.events, m1) == 0);
    CHECK(rig.M(m1).state == MonsterState::Flee);
    // Its time runs out: it fights again (Chase), EvMonsterFlee{ended, expired}.
    rig.h.events.Clear();
    REQUIRE(rig.StepUntil([&]() { return rig.M(m1).state != MonsterState::Flee; }, 400));
    CHECK(rig.Now() >= t0 + 3000);
    CHECK(rig.Now() < t0 + 3000 + 2 * kSimStepMs);
    CHECK(rig.M(m1).state == MonsterState::Chase);
    bool ended = false;
    for (const EvMonsterFlee& f : K5Events<EvMonsterFlee>(rig.h.events)) {
      if (f.entity == m1) ended = f.ended && f.reason == FleeEnd::Expired;
    }
    CHECK(ended);
    // A non-leader's death breaks nothing.
    rig.h.events.Clear();
    rig.Kill(m4);
    CHECK(K5Events<EvMonsterFlee>(rig.h.events).empty());
  }

  TEST_CASE("K5 runtime: a break cancels an ability winding up and a consume channel; hits do not stop a flee") {
    K5Rig rig;
    rig.PlaceHero(Vec2(41.5, 40));
    const uint32_t pack = rig.monsters.NewPackId();
    MonsterDef caster = K5Member(0, 3000, "fx_k5_caster");
    MonsterAbilityDef slam;
    slam.id = "slam";
    slam.kind = AbilityKind::Circle;
    slam.clip = "Cast01";
    slam.aim = AbilityAim::Self;
    slam.hasShape = true;
    slam.shape.radius = 2;
    slam.cooldownMs = 8000;
    caster.abilities = {slam};
    MonsterDef eater = K5Member(0, 3000, "fx_k5_eater");
    eater.corpse.consumes = true;
    MonsterDef body = K5Melee("fx_k5_body");
    body.corpse.leaves = CorpseLeaves::Corpse;
    const EntityId leader = rig.Spawn(K5Melee("fx_k5_chief"), Vec2(36, 44), MonsterState::Idle, pack, true);
    const EntityId c = rig.Spawn(caster, Vec2(40.5, 40), MonsterState::Attack, pack);
    const EntityId e = rig.Spawn(eater, Vec2(30, 40), MonsterState::Idle, pack);
    const EntityId food = rig.Spawn(body, Vec2(30.5, 40), MonsterState::Idle);
    rig.Kill(food);  // its corpse lies within a tile of the eater
    rig.Step();      // the caster's due swing becomes its slam; the eater starts eating
    REQUIRE(rig.M(c).pendingAbility == 0);
    REQUIRE(rig.M(e).consumeCorpse != 0);
    rig.M(e).hp = rig.M(e).maxHp * 0.2;  // wounded: a fighting eater would eat (K3), a fleeing one does not
    rig.h.events.Clear();
    rig.Kill(leader);
    CHECK(rig.M(c).state == MonsterState::Flee);
    CHECK(rig.M(c).pendingAbility == -1);
    CHECK(rig.M(c).abilityRootedUntilMs == 0);
    CHECK_FALSE(rig.combat.HasPendingStrike(c));
    CHECK(rig.M(e).state == MonsterState::Flee);
    CHECK(rig.M(e).consumeCorpse == 0);
    int cancelled = 0;
    for (const EvMonsterAttackCancelled& x : K5Events<EvMonsterAttackCancelled>(rig.h.events)) {
      cancelled += x.monster == c || x.monster == e ? 1 : 0;
    }
    CHECK(cancelled == 2);
    // The slam never lands, the corpse is not eaten (the eater does not stop to eat while it flees).
    rig.h.events.Clear();
    rig.Step();
    CHECK(rig.M(e).state == MonsterState::Flee);
    CHECK(rig.M(e).consumeCorpse == 0);
    rig.Step(89);
    CHECK(K5Events<EvTelegraph>(rig.h.events).empty());
    for (const EvCorpseRemoved& r : K5Events<EvCorpseRemoved>(rig.h.events)) CHECK(r.reason != CorpseRemoveReason::Consumed);
    // A hit on a fleeing monster (one that provokes) does not stop it: it keeps running.
    rig.Hit(c, 5, std::nullopt, true);
    rig.Step();
    CHECK(rig.M(c).state == MonsterState::Flee);
  }

  TEST_CASE("K5 runtime: a fleeing monster with no step away fights instead; the leash bounds a flee; a camp stops it") {
    K5Rig rig;
    rig.PlaceHero(Vec2(60, 60));
    auto grid = std::make_shared<MTestGrid>(rig.zone.Map().cols, rig.zone.Map().rows);
    for (const TilePos t : {TilePos{41, 39}, TilePos{41, 40}, TilePos{41, 41}, TilePos{40, 39}, TilePos{40, 41}}) {
      grid->Block(t.col, t.row);
    }
    rig.monsters.SetWorldForTesting(MGridWorld(grid, true));
    const uint32_t pack = rig.monsters.NewPackId();
    const EntityId leader = rig.Spawn(K5Melee("fx_k5_chief"), Vec2(38, 40), MonsterState::Idle, pack, true);
    const EntityId m = rig.Spawn(K5Member(0), Vec2(40, 40), MonsterState::Idle, pack);
    rig.Kill(leader);
    REQUIRE(rig.M(m).state == MonsterState::Flee);
    rig.h.events.Clear();
    rig.Step();
    CHECK(rig.M(m).state == MonsterState::Chase);  // cornered: it fights
    CHECK(rig.M(m).pos == Vec2(40, 40));
    const std::vector<EvMonsterFlee> ev = K5Events<EvMonsterFlee>(rig.h.events);
    REQUIRE(ev.size() == 1);
    CHECK((ev[0].entity == m && ev[0].ended && ev[0].reason == FleeEnd::Blocked));

    // The leash: a flee never carries it past its leash range (no Returning, no heal), it ends against it.
    K5Rig rig2;
    rig2.PlaceHero(Vec2(80, 80));
    const uint32_t p2 = rig2.monsters.NewPackId();
    const EntityId l2 = rig2.Spawn(K5Melee("fx_k5_chief"), Vec2(38, 40), MonsterState::Idle, p2, true);
    const EntityId f2 = rig2.Spawn(K5Member(0, 20000), Vec2(40, 40), MonsterState::Idle, p2);
    rig2.M(f2).spawnAnchor = TilePos{42, 40};  // it runs toward its leash edge (8 from here)
    rig2.M(f2).hp = rig2.M(f2).maxHp / 2;
    rig2.Kill(l2);
    bool returning = false, blocked = false;
    double farthest = 0;
    for (int s = 0; s < 600 && rig2.M(f2).state == MonsterState::Flee; ++s) {
      rig2.Step();
      returning |= rig2.M(f2).state == MonsterState::Returning;
      farthest = std::max(farthest, Dist(rig2.M(f2).pos, Vec2(42, 40)));
      for (const EvMonsterFlee& e : K5Events<EvMonsterFlee>(rig2.h.events)) blocked |= e.ended && e.reason == FleeEnd::Blocked;
      rig2.h.events.Clear();
    }
    CHECK_FALSE(returning);
    CHECK(farthest <= rig2.h.ctx.data.Monsters().ai.leashRange + 1e-6);
    CHECK(farthest > 6.5);  // it did run
    CHECK(blocked);       // out of room inside the leash: it turned to fight
    CHECK(rig2.M(f2).hp == rig2.M(f2).maxHp / 2);  // never healed by a walk home

    // A camp's safe zone ends a flee (interrupted): the monster drops to Idle there.
    K5Rig rig3;
    const MapCampDef& camp = rig3.zone.Map().camps.front();
    rig3.PlaceHero(Vec2(80, 80));
    const uint32_t p3 = rig3.monsters.NewPackId();
    const Vec2 inside = camp.pos.Center() + Vec2(1, 0);
    const EntityId l3 = rig3.Spawn(K5Melee("fx_k5_chief"), inside + Vec2(-2, 0), MonsterState::Idle, p3, true);
    const EntityId f3 = rig3.Spawn(K5Member(0), inside, MonsterState::Idle, p3);
    rig3.Kill(l3);
    REQUIRE(rig3.M(f3).state == MonsterState::Flee);
    rig3.h.events.Clear();
    rig3.Step();
    CHECK(rig3.M(f3).state == MonsterState::Idle);
    const std::vector<EvMonsterFlee> ev3 = K5Events<EvMonsterFlee>(rig3.h.events);
    REQUIRE(ev3.size() == 1);
    CHECK((ev3[0].ended && ev3[0].reason == FleeEnd::Interrupted));
  }

  TEST_CASE("K5 runtime: sustain: a living drummer within reach holds the pack and steadies a flee; so does fearImmune") {
    K5Rig rig;
    rig.PlaceHero(Vec2(80, 80));
    MonsterDef memberDef = K5Member(0, 5000);
    memberDef.morale.sustainFamily = "fx_living";
    memberDef.morale.sustainRadius = 8;
    const uint32_t pack = rig.monsters.NewPackId();
    const EntityId leader = rig.Spawn(K5Melee("fx_k5_chief"), Vec2(40, 40), MonsterState::Idle, pack, true);
    const EntityId drummer = rig.Spawn(K5Drummer(), Vec2(40, 47), MonsterState::Idle);  // any pack: a sustain is a sustain
    const EntityId held = rig.Spawn(memberDef, Vec2(42, 42), MonsterState::Idle, pack);    // 5.4 from the drummer
    const EntityId far = rig.Spawn(memberDef, Vec2(52, 40), MonsterState::Idle, pack);     // 13.9 from it
    rig.Kill(leader);
    CHECK(rig.M(held).state == MonsterState::Idle);  // held by the drum
    CHECK(rig.M(far).state == MonsterState::Flee);
    CHECK(rig.monsters.Sustained(rig.M(held)));
    CHECK_FALSE(rig.monsters.Sustained(rig.M(far)));
    // The drummer comes within reach of the fleeing one: its flee ends (sustained) and it fights.
    rig.monsters.Teleport(drummer, rig.M(far).pos + Vec2(0, 3), TeleportReason::Debug);
    rig.h.events.Clear();
    rig.Step();
    CHECK(rig.M(far).state != MonsterState::Flee);  // back to the fight rules (the hero is far: it idles)
    const std::vector<EvMonsterFlee> ev = K5Events<EvMonsterFlee>(rig.h.events);
    REQUIRE(ev.size() == 1);
    CHECK((ev[0].entity == far && ev[0].ended && ev[0].reason == FleeEnd::Sustained));

    // Kill order: the drummer first, then the leader - they break.
    K5Rig rig2;
    rig2.PlaceHero(Vec2(80, 80));
    const uint32_t p2 = rig2.monsters.NewPackId();
    const EntityId l2 = rig2.Spawn(K5Melee("fx_k5_chief"), Vec2(40, 40), MonsterState::Idle, p2, true);
    const EntityId d2 = rig2.Spawn(K5Drummer(), Vec2(40, 47), MonsterState::Idle);
    const EntityId h2 = rig2.Spawn(memberDef, Vec2(42, 42), MonsterState::Idle, p2);
    rig2.Kill(d2);
    rig2.Kill(l2);
    CHECK(rig2.M(h2).state == MonsterState::Flee);

    // Another family's drummer sustains nobody; a member with no sustainRadius never counts drummers.
    K5Rig rig3;
    rig3.PlaceHero(Vec2(80, 80));
    const uint32_t p3 = rig3.monsters.NewPackId();
    MonsterDef foreign = K5Drummer();
    foreign.family = "fx_unburied";
    const EntityId l3 = rig3.Spawn(K5Melee("fx_k5_chief"), Vec2(40, 40), MonsterState::Idle, p3, true);
    rig3.Spawn(foreign, Vec2(41, 43), MonsterState::Idle);
    const EntityId h3 = rig3.Spawn(memberDef, Vec2(42, 42), MonsterState::Idle, p3);       // only the foreign one near
    const EntityId plain3 = rig3.Spawn(K5Member(0, 5000), Vec2(30, 42), MonsterState::Idle, p3);
    rig3.Spawn(K5Drummer(), Vec2(29, 44), MonsterState::Idle);  // 2.2 from plain3, 13.2 from h3
    rig3.Kill(l3);
    CHECK(rig3.M(h3).state == MonsterState::Flee);      // another family's drummer holds nobody
    CHECK(rig3.M(plain3).state == MonsterState::Flee);  // no sustainRadius: drummers do not hold it

    // A fearImmune buff (the drum's) holds a member, and steadies a flee under way.
    K5Rig rig4;
    rig4.PlaceHero(Vec2(80, 80));
    const uint32_t p4 = rig4.monsters.NewPackId();
    const EntityId l4 = rig4.Spawn(K5Melee("fx_k5_chief"), Vec2(40, 40), MonsterState::Idle, p4, true);
    const EntityId brave = rig4.Spawn(K5Member(0, 5000), Vec2(42, 40), MonsterState::Idle, p4);
    const EntityId timid = rig4.Spawn(K5Member(0, 5000), Vec2(38, 40), MonsterState::Idle, p4);
    rig4.M(brave).abilityBuffs.push_back(MonsterAbilityBuff{MonsterBuffStat::FearImmune, 1, rig4.Now() + 2000, 99});
    rig4.Kill(l4);
    CHECK(rig4.M(brave).state == MonsterState::Idle);
    CHECK(rig4.M(timid).state == MonsterState::Flee);
    rig4.Step(10);
    rig4.M(timid).abilityBuffs.push_back(MonsterAbilityBuff{MonsterBuffStat::FearImmune, 1, rig4.Now() + 2000, 99});
    rig4.h.events.Clear();
    rig4.Step();
    CHECK(rig4.M(timid).state != MonsterState::Flee);
    const std::vector<EvMonsterFlee> ev4 = K5Events<EvMonsterFlee>(rig4.h.events);
    REQUIRE(ev4.size() == 1);
    CHECK(ev4[0].reason == FleeEnd::Sustained);
    // A fearImmune aura holds them too.
    K5Rig rig5;
    rig5.PlaceHero(Vec2(80, 80));
    const uint32_t p5 = rig5.monsters.NewPackId();
    MonsterDef banner = K5Melee("fx_k5_banner");
    banner.aura.present = true;
    banner.aura.stat = MonsterBuffStat::FearImmune;
    banner.aura.radius = 5;
    const EntityId l5 = rig5.Spawn(K5Melee("fx_k5_chief"), Vec2(40, 40), MonsterState::Idle, p5, true);
    rig5.Spawn(banner, Vec2(44, 44), MonsterState::Idle);
    const EntityId near5 = rig5.Spawn(K5Member(0, 5000), Vec2(42, 42), MonsterState::Idle, p5);
    const EntityId far5 = rig5.Spawn(K5Member(0, 5000), Vec2(36, 36), MonsterState::Idle, p5);
    rig5.Step();  // the step's auras
    rig5.Kill(l5);
    CHECK(rig5.M(near5).state == MonsterState::Idle);
    CHECK(rig5.M(far5).state == MonsterState::Flee);
  }

  // =================================================================================================================
  // Runtime: kite
  // =================================================================================================================
  TEST_CASE("K5 runtime: kite: closer than min it backs off to max along the away vector, swings, then may kite again") {
    K5Rig rig;
    const MonsterAiDef& ai = rig.h.ctx.data.Monsters().ai;
    rig.PlaceHero(Vec2(41.5, 40));
    const EntityId a = rig.Spawn(K5Archer(), Vec2(40, 40), MonsterState::Attack);
    rig.Step();  // the kite starts (no movement on that tick, no swing: it is in Chase now)
    CHECK(rig.M(a).kiting);
    CHECK(rig.M(a).state == MonsterState::Chase);
    CHECK(rig.M(a).kiteTarget == Vec2(35.5, 40));
    CHECK(rig.M(a).pos == Vec2(40, 40));
    CHECK(K5Swings(rig.h.events, a) == 0);
    {
      const Snapshot snap = rig.View();
      REQUIRE(rig.ViewOf(snap, a) != nullptr);
      CHECK(rig.ViewOf(snap, a)->kiting);
    }
    // It walks back; just before it arrives its swing is put off (it swung "now"): it arrives with none due.
    REQUIRE(rig.StepUntil([&]() { return Dist(rig.M(a).pos, rig.M(a).kiteTarget) < 0.5; }, 600));
    CHECK(rig.M(a).kiting);
    rig.M(a).lastAttackMs = rig.Now();
    REQUIRE(rig.StepUntil([&]() { return !rig.M(a).kiting; }, 120));
    const Vec2 spot = rig.M(a).pos;
    CHECK(Dist(spot, Vec2(35.5, 40)) < ai.arriveEpsilon + 1e-9);
    CHECK(rig.M(a).state == MonsterState::Attack);  // the hero 6 tiles off: within reach
    // The hero closes in again: no kite before it has swung, and none while its swing is on the way.
    rig.PlaceHero(spot + Vec2(1.5, 0));
    rig.h.events.Clear();
    double swingAt = -1, contactAt = -1, kiteAt = -1;
    for (int s = 0; s < 400 && kiteAt < 0; ++s) {
      rig.Step();
      if (swingAt < 0 && K5Swings(rig.h.events, a) > 0) swingAt = rig.Now();
      if (swingAt > 0 && contactAt < 0 && !rig.combat.HasPendingStrike(a)) contactAt = rig.Now();
      if (rig.M(a).kiting) kiteAt = rig.Now();
      rig.h.events.Clear();
    }
    REQUIRE(swingAt > 0);
    REQUIRE(kiteAt > 0);
    CHECK(swingAt == doctest::Approx(rig.M(a).lastAttackMs));
    CHECK(kiteAt >= contactAt);
    CHECK(contactAt > swingAt);
    CHECK(Dist(rig.M(a).kiteTarget, rig.hero->Position()) == doctest::Approx(6));

    // Its back to the end of a dead end: no kite target - it stands and shoots.
    K5Rig pen;
    auto grid = std::make_shared<MTestGrid>(pen.zone.Map().cols, pen.zone.Map().rows);
    for (int32_t c = 0; c < grid->cols; ++c) {
      for (int32_t r = 0; r < grid->rows; ++r) {
        if (!(r == 40 && c >= 40 && c <= 44)) grid->Block(c, r);
      }
    }
    pen.monsters.SetWorldForTesting(MGridWorld(grid, true));
    pen.PlaceHero(Vec2(41.5, 40));
    const EntityId b = pen.Spawn(K5Archer(), Vec2(39.6, 40), MonsterState::Attack);
    pen.Step(10);
    CHECK_FALSE(pen.M(b).kiting);
    CHECK(pen.M(b).state == MonsterState::Attack);
    CHECK(K5Swings(pen.h.events, b) == 1);

    // The leash bounds a kite in play: with its anchor toward the hero, it never leashes home.
    K5Rig rig3;
    rig3.PlaceHero(Vec2(41.5, 40));
    const EntityId c3 = rig3.Spawn(K5Archer(), Vec2(40, 40), MonsterState::Attack);
    rig3.M(c3).spawnAnchor = TilePos{44, 40};
    bool returning = false;
    for (int s = 0; s < 400; ++s) {
      rig3.Step();
      returning |= rig3.M(c3).state == MonsterState::Returning;
    }
    CHECK_FALSE(returning);
    CHECK(DistSq(rig3.M(c3).kiteTarget, Vec2(44, 40)) <= 64 + 1e-9);
  }

  // =================================================================================================================
  // Runtime: block and the shield wall
  // =================================================================================================================
  TEST_CASE("K5 runtime: block: the front arc halves a hit (EvBlock before the EvHit); side, back, ticks, a stunned one") {
    K5Rig rig;
    rig.PlaceHero(Vec2(60, 60));
    MonsterDef tough = K5Shield(0);
    tough.hp = 5000;
    const EntityId s = rig.Spawn(tough, Vec2(40, 40), MonsterState::Idle);
    rig.M(s).heading = Vec2(1, 0);
    const auto taken = [&](std::optional<Vec2> from, bool tick = false) {
      const double before = rig.M(s).hp;
      rig.Hit(s, 100, from, false, tick);
      return before - rig.M(s).hp;
    };
    rig.h.events.Clear();
    CHECK(taken(Vec2(42, 40)) == 50);  // front
    const std::vector<EvBlock> blocks = K5Events<EvBlock>(rig.h.events);
    REQUIRE(blocks.size() == 1);
    CHECK((blocks[0].entity == s && blocks[0].source == kHeroEntityId && blocks[0].from == Vec2(42, 40) &&
           blocks[0].amount == 50 && blocks[0].blocked == 50));
    // EvBlock comes right before its EvHit, which carries the reduced amount.
    size_t blockAt = 0, hitAt = 0;
    for (size_t k = 0; k < rig.h.events.Items().size(); ++k) {
      if (std::holds_alternative<EvBlock>(rig.h.events.Items()[k])) blockAt = k;
      if (const EvHit* hh = std::get_if<EvHit>(&rig.h.events.Items()[k])) {
        if (hh->target == s) {
          hitAt = k;
          CHECK(hh->amount == 50);
        }
      }
    }
    CHECK(blockAt + 1 == hitAt);
    rig.h.events.Clear();
    CHECK(taken(Vec2(41.4, 41.4)) == 50);   // 45 degrees off: inside the 100-degree arc
    CHECK(taken(Vec2(40, 42)) == 100);      // side
    CHECK(taken(Vec2(41, 42)) == 100);      // 63 degrees off
    CHECK(taken(Vec2(38, 40)) == 100);      // back
    CHECK(taken(Vec2(42, 40), true) == 100);  // a tick is never blocked
    CHECK(K5Events<EvBlock>(rig.h.events).size() == 1);
    // A hit with no direction (from the blocker's own spot, or none at all) is not blocked.
    CHECK(taken(Vec2(40, 40)) == 100);
    {
      MonsterHitRequest r;
      r.monster = s;
      r.amount = 100;
      r.attacker = kHeroEntityId;
      r.source = KillSource::HeroSkill;
      r.provokes = false;
      const double before = rig.M(s).hp;
      rig.combat.DamageMonster(r);
      CHECK(before - rig.M(s).hp == 100);
    }
    // A beast (or anything) behind it gets through; in front it is blocked like the hero.
    {
      MonsterHitRequest r;
      r.monster = s;
      r.amount = 40;
      r.hasFrom = true;
      r.from = Vec2(37, 40);
      r.attacker = 777;
      r.source = KillSource::Pet;
      const double before = rig.M(s).hp;
      rig.combat.DamageMonster(r);
      CHECK(before - rig.M(s).hp == 40);
      r.from = Vec2(43, 40);
      const double before2 = rig.M(s).hp;
      rig.combat.DamageMonster(r);
      CHECK(before2 - rig.M(s).hp == 20);
    }
    // The block comes before the K4 damageTakenMul: 99 -> floor(49.5) = 49 -> floor(49 x 1.1) = 53 (the other way round
    // would be floor(floor(108.9) x 0.5) = 54). EvBlock reports both in the units of the damage taken: unblocked the hit
    // would have dealt floor(99 x 1.1) = 108, so the shield stopped 108 - 53 = 55.
    rig.M(s).phaseDamageTakenMul = 1.1;
    rig.h.events.Clear();
    {
      const double before = rig.M(s).hp;
      rig.Hit(s, 99, Vec2(42, 40));
      CHECK(before - rig.M(s).hp == 53);
    }
    REQUIRE(K5Events<EvBlock>(rig.h.events).size() == 1);
    CHECK((K5Events<EvBlock>(rig.h.events)[0].amount == 53 && K5Events<EvBlock>(rig.h.events)[0].blocked == 55));
    rig.M(s).phaseDamageTakenMul = 1;
    // A small hit stays a hit; a stunned or frozen blocker cannot block.
    {
      const double before = rig.M(s).hp;
      rig.Hit(s, 1, Vec2(42, 40));
      CHECK(before - rig.M(s).hp == 1);
    }
    rig.status.Apply(s, StatusType::Stun, 1, 1000, kHeroEntityId, rig.Now());
    CHECK(taken(Vec2(42, 40)) == 100);
    // In the fight it squares up to the hero (Attack): the hero's blows land in front.
    K5Rig rig2;
    rig2.PlaceHero(Vec2(41, 41));
    const EntityId t = rig2.Spawn(K5Shield(0), Vec2(40, 40), MonsterState::Attack);
    rig2.M(t).heading = Vec2(-1, 0);
    rig2.Step();
    const double before = rig2.M(t).hp;
    rig2.Hit(t, 100);  // from the hero
    CHECK(before - rig2.M(t).hp == 50);
  }

  TEST_CASE("K5 runtime: the shield wall: fighting blockers within wall tiles of each other (chained) share their mean heading") {
    K5Rig rig;
    rig.PlaceHero(Vec2(80, 80));
    const EntityId w1 = rig.Spawn(K5Shield(2), Vec2(40, 40));
    const EntityId w2 = rig.Spawn(K5Shield(2), Vec2(41.5, 40));
    const EntityId w3 = rig.Spawn(K5Shield(2), Vec2(43, 40));     // 3 from w1: in the wall through w2
    const EntityId lone = rig.Spawn(K5Shield(2), Vec2(50, 40));
    const EntityId narrow = rig.Spawn(K5Shield(1), Vec2(44, 41));  // 1.41 from w3: past its own wall (1)
    const EntityId none = rig.Spawn(K5Shield(0), Vec2(40, 41));    // no wall: keeps its own
    // Fighting (Attack) and held this step as by a windup (rooted: the AI does not turn them), so the wall's mean is
    // that of the headings set here.
    const auto fight = [&](EntityId id, Vec2 heading) {
      MonsterInstance& m = rig.M(id);
      m.state = MonsterState::Attack;
      m.abilityRootedUntilMs = rig.Now() + 1000;
      m.heading = heading;
    };
    fight(w1, Vec2(1, 0));
    fight(w2, Vec2(0, 1));
    fight(w3, Vec2(-1, 0));
    fight(lone, Vec2(0, -1));
    fight(narrow, Vec2(1, 0));
    fight(none, Vec2(-1, 0));
    rig.Step();
    for (EntityId id : {w1, w2, w3}) CHECK(rig.M(id).heading == Vec2(0, 1));
    CHECK(rig.M(lone).heading == Vec2(0, -1));
    CHECK(rig.M(narrow).heading == Vec2(1, 0));
    CHECK(rig.M(none).heading == Vec2(-1, 0));
    // The block reads the shared heading: w1 now guards +row, not its old +col.
    const double hp = rig.M(w1).hp;
    rig.Hit(w1, 100, Vec2(40, 42));
    CHECK(hp - rig.M(w1).hp == 50);
    rig.Hit(w1, 100, Vec2(42, 40));
    CHECK(hp - rig.M(w1).hp == 150);
    // The snapshot shows the guard: its arc and the heading.
    const Snapshot snap = rig.View();
    REQUIRE(rig.ViewOf(snap, w1) != nullptr);
    CHECK((rig.ViewOf(snap, w1)->blockArcDeg == 100 && rig.ViewOf(snap, w1)->heading == Vec2(0, 1)));
    // A fallen link splits the wall: w1 and w3 (3 apart) keep their own again.
    rig.Kill(w2);
    fight(w1, Vec2(1, 0));
    fight(w3, Vec2(-1, 0));
    rig.Step();
    CHECK(rig.M(w1).heading == Vec2(1, 0));
    CHECK(rig.M(w3).heading == Vec2(-1, 0));
    // Only fighters (Chase / Attack) join: the same three idle keep their own headings.
    for (EntityId id : {w1, w3}) {
      rig.M(id).state = MonsterState::Idle;
      rig.M(id).abilityRootedUntilMs = 0;
    }
    const EntityId w4 = rig.Spawn(K5Shield(2), Vec2(41.5, 40));
    rig.M(w1).heading = Vec2(1, 0);
    rig.M(w4).heading = Vec2(0, 1);
    rig.M(w3).heading = Vec2(-1, 0);
    rig.Step();
    CHECK(rig.M(w1).heading == Vec2(1, 0));
    CHECK(rig.M(w4).heading == Vec2(0, 1));
    CHECK(rig.M(w3).heading == Vec2(-1, 0));

    // A neighbour walking home (M1) or standing idle within the wall does not turn a fighter's guard away from the hero
    // it fights: the fighter squares up to the hero alone, and the hero's blows land on its shield.
    K5Rig rig2;
    rig2.PlaceHero(Vec2(38.8, 40));
    const EntityId fighter = rig2.Spawn(K5Shield(2), Vec2(40, 40), MonsterState::Attack);
    const EntityId home = rig2.Spawn(K5Shield(2), Vec2(41.5, 40), MonsterState::Returning);
    rig2.M(home).spawnAnchor = TilePos{46, 40};
    MonsterDef dozing = K5Shield(2, "fx_k5_dozing");
    dozing.aggroRange = 0.5;  // the hero 1.9 off does not wake it
    const EntityId idle = rig2.Spawn(dozing, Vec2(40, 41.5), MonsterState::Idle);
    rig2.M(idle).heading = Vec2(1, 0);
    rig2.Step();
    REQUIRE(rig2.M(fighter).state == MonsterState::Attack);
    REQUIRE(rig2.M(home).state == MonsterState::Returning);
    REQUIRE(rig2.M(idle).state == MonsterState::Idle);
    CHECK(rig2.M(fighter).heading == Vec2(-1, 0));
    CHECK(rig2.M(home).heading.x > 0.99);  // its own: toward home
    CHECK(rig2.M(idle).heading == Vec2(1, 0));
    const double before = rig2.M(fighter).hp;
    rig2.Hit(fighter, 100);  // from the hero
    CHECK(before - rig2.M(fighter).hp == 50);
  }

  // =================================================================================================================
  // Runtime: buff / call abilities and auras
  // =================================================================================================================
  TEST_CASE("K5 runtime: buff / call: the drum is a cast between swings; it calls its family in reach and steadies them") {
    K5Rig rig;
    rig.PlaceHero(Vec2(44, 40));
    MonsterDef drummer = K5Drummer();
    drummer.speed = 0;  // it stays 4 tiles from the hero, past its swing reach: it never swings
    const EntityId d = rig.Spawn(drummer, Vec2(40, 40), MonsterState::Chase);
    const EntityId f1 = rig.Spawn(K5Member(0, 0), Vec2(40, 46), MonsterState::Idle);  // fx_living, 6 away
    const EntityId f2 = rig.Spawn(K5Member(0, 0), Vec2(40, 50), MonsterState::Idle);  // 10 away
    const EntityId u1 = rig.Spawn(K5Shield(0), Vec2(37, 44), MonsterState::Idle);     // another family, 5 away
    // f1's pack-mate, 5.5 from f1 (within its packAggro) but past the call: a call passes no alarm on.
    const uint32_t pack = rig.monsters.NewPackId();
    rig.M(f1).packId = pack;
    const EntityId f3 = rig.Spawn(K5Member(6, 0), Vec2(40, 51.5), MonsterState::Idle, pack);
    rig.Step();
    std::vector<EvPlayAnim> anims;
    for (const EvPlayAnim& a : K5Events<EvPlayAnim>(rig.h.events)) {
      if (a.entity == d) anims.push_back(a);
    }
    REQUIRE(anims.size() == 1);
    CHECK((anims[0].action == AnimAction::Cast && anims[0].clip == "Cast01" && anims[0].contactMs == 800));
    CHECK(K5Events<EvTelegraph>(rig.h.events).empty());  // the clip is the tell: no ground decal
    const double start = anims[0].startMs;
    CHECK(rig.M(d).pendingAbility == 0);
    rig.aggro.clear();
    std::vector<EvMonsterBuffCast> casts;
    REQUIRE(rig.StepUntil([&]() {
      casts = K5Events<EvMonsterBuffCast>(rig.h.events);
      rig.h.events.Clear();
      return !casts.empty();
    }));
    REQUIRE(casts.size() == 1);
    const EvMonsterBuffCast& c = casts[0];
    CHECK(rig.Now() == doctest::Approx(start + 800));
    CHECK((c.entity == d && c.abilityId == "drum" && c.kind == AbilityKind::Call && c.radius == 8 &&
           c.family == "fx_living" && c.hasBuff && c.stat == MonsterBuffStat::FearImmune));
    CHECK(c.untilMs == doctest::Approx(start + 800 + 3000));
    CHECK(c.buffed == 2);  // the drummer and f1
    CHECK(c.called == 1);  // f1
    CHECK(rig.M(f1).state == MonsterState::Chase);
    CHECK(rig.M(f1).provokedUntilMs > rig.Now());
    CHECK(rig.aggro == std::vector<EntityId>{f1});
    CHECK(rig.M(f2).state == MonsterState::Idle);
    CHECK(rig.M(u1).state == MonsterState::Idle);
    CHECK(rig.M(f3).state == MonsterState::Idle);
    CHECK((rig.M(f1).fearImmune && rig.M(d).fearImmune && !rig.M(f2).fearImmune && !rig.M(u1).fearImmune));
    {
      const Snapshot snap = rig.View();
      CHECK(rig.ViewOf(snap, f1)->buffMask == (1u << static_cast<uint32_t>(MonsterBuffStat::FearImmune)));
      CHECK(rig.ViewOf(snap, f2)->buffMask == 0);
    }
    // It runs out after its ms.
    REQUIRE(rig.StepUntil([&]() { return rig.Now() >= c.untilMs; }));
    rig.Step();
    CHECK_FALSE(rig.M(f1).fearImmune);
    CHECK(rig.M(f1).abilityBuffs.empty());
    // The next drum comes on its cooldown (8 s from the first windup).
    double second = -1;
    for (int s = 0; s < 600 && second < 0; ++s) {
      rig.Step();
      for (const EvPlayAnim& a : K5Events<EvPlayAnim>(rig.h.events)) {
        if (a.entity == d && a.clip == "Cast01") second = a.startMs;
      }
      rig.h.events.Clear();
    }
    CHECK(second == doctest::Approx(start + 8000).epsilon(1e-9));
  }

  TEST_CASE("K5 runtime: a buff raises its stat for its time (a recast refreshes it); a stun aborts the cast") {
    K5Rig rig;
    rig.PlaceHero(Vec2(44, 40));
    MonsterDef captain = K5Melee("fx_k5_captain");
    captain.abilities = {K5Buff(K5Cast("march", AbilityKind::Buff, 2000, 6), MonsterBuffStat::AttackSpeed, 1.25, 5000),
                         K5Buff(K5Cast("press", AbilityKind::Buff, 2000, 0), MonsterBuffStat::MoveSpeed, 1.5, 5000)};
    captain.abilities[1].priority = -1;  // after the march
    const EntityId cap = rig.Spawn(captain, Vec2(40, 40), MonsterState::Chase);
    const EntityId ally = rig.Spawn(K5Melee("fx_k5_ally"), Vec2(40, 44), MonsterState::Idle);
    const EntityId far = rig.Spawn(K5Melee("fx_k5_far"), Vec2(40, 47), MonsterState::Idle);
    REQUIRE(rig.StepUntil([&]() { return !K5Events<EvMonsterBuffCast>(rig.h.events).empty(); }));
    const EvMonsterBuffCast c = K5Events<EvMonsterBuffCast>(rig.h.events).front();
    CHECK((c.abilityId == "march" && c.kind == AbilityKind::Buff && c.buffed == 2 && c.called == 0));
    CHECK(rig.M(ally).AttackIntervalMs() == doctest::Approx(1200 / 1.25));
    CHECK(rig.M(cap).AttackIntervalMs() == doctest::Approx(1200 / 1.25));
    CHECK(rig.M(far).AttackIntervalMs() == 1200);
    CHECK(rig.M(ally).state == MonsterState::Idle);  // a buff calls nobody
    // The press (radius 0) buffs the captain alone; a second march refreshes the one buff it gave.
    rig.h.events.Clear();
    REQUIRE(rig.StepUntil([&]() { return !K5Events<EvMonsterBuffCast>(rig.h.events).empty(); }, 900));
    const EvMonsterBuffCast p = K5Events<EvMonsterBuffCast>(rig.h.events).front();
    CHECK((p.abilityId == "press" && p.buffed == 1));
    CHECK(rig.M(cap).buffMoveSpeedMul == 1.5);
    CHECK(rig.M(ally).buffMoveSpeedMul == 1);
    rig.h.events.Clear();
    REQUIRE(rig.StepUntil([&]() { return !K5Events<EvMonsterBuffCast>(rig.h.events).empty(); }, 900));
    CHECK(K5Events<EvMonsterBuffCast>(rig.h.events).front().abilityId == "march");
    int marches = 0;
    for (const MonsterAbilityBuff& b : rig.M(ally).abilityBuffs) marches += b.stat == MonsterBuffStat::AttackSpeed ? 1 : 0;
    CHECK(marches == 1);
    // A stun mid-windup aborts the cast (K2's rule): no buff, EvMonsterAttackCancelled.
    rig.h.events.Clear();
    REQUIRE(rig.StepUntil([&]() { return rig.M(cap).pendingAbility >= 0; }, 900));
    rig.status.Apply(cap, StatusType::Stun, 1, 300, kHeroEntityId, rig.Now());
    rig.h.events.Clear();
    rig.Step(60);
    CHECK(K5Events<EvMonsterBuffCast>(rig.h.events).empty());
    bool cancelled = false;
    for (const EvMonsterAttackCancelled& e : K5Events<EvMonsterAttackCancelled>(rig.h.events)) cancelled |= e.monster == cap;
    CHECK(cancelled);
    // A contact refreshes its targets' multipliers at once (not at the next step's refresh).
    REQUIRE(rig.M(cap).buffDamageMul == 1);
    const MonsterAbilityDef rally =
        K5Buff(K5Cast("rally", AbilityKind::Buff, 2000, 0), MonsterBuffStat::Damage, 1.3, 5000);
    rig.monsters.ResolveBuffCast(cap, rally);
    CHECK(rig.M(cap).buffDamageMul == 1.3);
    const Snapshot snap = rig.View();
    REQUIRE(rig.ViewOf(snap, cap) != nullptr);
    CHECK((rig.ViewOf(snap, cap)->buffMask & (1u << static_cast<uint32_t>(MonsterBuffStat::Damage))) != 0);
  }

  TEST_CASE("K5 runtime: auras: a banner raises its family's stat within its radius (itself included); the largest holds") {
    K5Rig rig;
    rig.PlaceHero(Vec2(80, 80));
    MonsterDef banner = K5Shield(0, "fx_k5_banner");
    banner.block.present = false;
    banner.aura.present = true;
    banner.aura.stat = MonsterBuffStat::Damage;
    banner.aura.mul = 1.15;
    banner.aura.radius = 5;
    banner.aura.family = "fx_unburied";
    const EntityId b = rig.Spawn(banner, Vec2(40, 40));
    const EntityId f = rig.Spawn(K5Shield(0), Vec2(43, 40));     // fx_unburied, 3 away
    const EntityId g = rig.Spawn(K5Shield(0), Vec2(46, 40));     // 6 away
    const EntityId x = rig.Spawn(K5Melee(), Vec2(41, 40));       // fx_living
    rig.Step();
    CHECK(rig.M(b).buffDamageMul == 1.15);
    CHECK(rig.M(f).buffDamageMul == 1.15);
    CHECK(rig.M(g).buffDamageMul == 1);
    CHECK(rig.M(x).buffDamageMul == 1);
    CHECK(MonsterDamageBuffMul(rig.M(f), rig.Now()) == 1.15);
    {
      const Snapshot snap = rig.View();
      CHECK((rig.ViewOf(snap, b)->auraRadius == 5 && rig.ViewOf(snap, b)->auraStat == MonsterBuffStat::Damage));
      CHECK(rig.ViewOf(snap, f)->auraRadius == 0);
      CHECK(rig.ViewOf(snap, f)->buffMask == 1u);
    }
    // A stronger banner: the largest applies (no stacking).
    MonsterDef big = banner;
    big.id = "fx_k5_banner2";
    big.aura.mul = 1.3;
    const EntityId b2 = rig.Spawn(big, Vec2(45, 40));
    rig.Step();
    CHECK(rig.M(f).buffDamageMul == 1.3);
    CHECK(rig.M(g).buffDamageMul == 1.3);
    CHECK(rig.M(b).buffDamageMul == 1.3);
    rig.Kill(b2);
    rig.Step();
    CHECK(rig.M(f).buffDamageMul == 1.15);
    CHECK(rig.M(g).buffDamageMul == 1);
    rig.Kill(b);
    rig.Step();
    CHECK(rig.M(f).buffDamageMul == 1);
    {
      const Snapshot snap = rig.View();
      CHECK(rig.ViewOf(snap, b)->auraRadius == 0);  // a dead banner draws no halo
    }
    // Attack speed and move speed auras.
    MonsterDef drum = banner;
    drum.id = "fx_k5_banner3";
    drum.aura.stat = MonsterBuffStat::AttackSpeed;
    drum.aura.mul = 1.1;
    drum.aura.family.clear();
    const EntityId b3 = rig.Spawn(drum, Vec2(44, 41));
    MonsterDef wind = drum;
    wind.id = "fx_k5_banner4";
    wind.aura.stat = MonsterBuffStat::MoveSpeed;
    wind.aura.mul = 2;
    rig.Spawn(wind, Vec2(44, 39));
    rig.Step();
    CHECK(rig.M(f).AttackIntervalMs() == doctest::Approx(1700 / 1.1));
    CHECK(rig.M(f).buffMoveSpeedMul == 2);
    CHECK(rig.M(b3).AttackIntervalMs() == doctest::Approx(1700 / 1.1));
    // The move speed aura moves it faster: the first moveToward step from rest (v = top x 6 x dt).
    {
      MonsterInstance& fm = rig.M(f);
      fm.moveSpeed = 0;
      const Vec2 from = fm.pos;
      MonsterMoveToward(fm, rig.h.ctx.data.Monsters().ai, from + Vec2(5, 0), kSimStepMs, 1.0, rig.monsters.World());
      const double top = MonsterTopSpeed(fm.def, rig.h.ctx.data.Monsters().ai) * 2;
      CHECK(fm.pos.x - from.x == doctest::Approx(top * 6 * kSimStepMs / 1000.0 * kSimStepMs / 1000.0));
    }

    // The damage aura reaches the monster's blows: the same seeded fight, with and without the banner by it.
    const auto firstHit = [](bool withBanner) {
      K5Rig r(99);
      r.PlaceHero(Vec2(44, 40));
      MonsterDef bearer = K5Melee("fx_k5_bearer");
      bearer.aggroRange = 0.1;  // it stays idle (never swings: the Combat stream feeds the footman alone)
      bearer.family = "fx_unburied";
      bearer.aura.present = true;
      bearer.aura.stat = MonsterBuffStat::Damage;
      bearer.aura.mul = 1.5;
      bearer.aura.radius = 5;
      MonsterDef foot = K5Melee("fx_k5_foot", 15);
      foot.family = "fx_unburied";
      if (withBanner) r.Spawn(bearer, Vec2(40, 40));
      const EntityId ff = r.Spawn(foot, Vec2(43, 40), MonsterState::Attack);
      for (int s = 0; s < 300; ++s) {
        r.Step();
        for (const EvHit& h : K5Events<EvHit>(r.h.events)) {
          if (h.target == kHeroEntityId && h.source == ff) return h.amount;
        }
        r.h.events.Clear();
      }
      return -1.0;
    };
    const double plainHit = firstHit(false), auraHit = firstHit(true);
    REQUIRE(plainHit > 0);
    CHECK(auraHit > plainHit);
  }

  // =================================================================================================================
  // Runtime: camps, a dead caster's buffs, the tether and the leash on a flee, a second break, the kite's strike gate
  // =================================================================================================================
  TEST_CASE("K5 runtime: a flee or a kite never walks into a camp's safe zone: with the hero just outside it they fight") {
    // The hero stands just outside a camp, within the monsters' aggro range, and their way away from him leads into the
    // camp. Inside it the safe-zone repel would drop them to Idle and the aggro scan put them back in Chase on a
    // no-movement tick, every step: frozen at the camp's edge, never swinging.
    K5Rig rig;
    const MapCampDef& camp = rig.zone.Map().camps.front();
    const Vec2 c(camp.pos.col, camp.pos.row);
    const double r = rig.zone.SafeZoneRadius();
    REQUIRE(r > 2);
    rig.PlaceHero(c + Vec2(r + 3.5, 0));
    REQUIRE_FALSE(rig.zone.InSafeZone(rig.hero->Position()));
    const uint32_t pack = rig.monsters.NewPackId();
    const EntityId leader = rig.Spawn(K5Melee("fx_k5_chief"), c + Vec2(r + 3, 1), MonsterState::Idle, pack, true);
    // A 2 s flee: when it ends the member stands within its aggro range of the hero (it fights him).
    const EntityId m = rig.Spawn(K5Member(0, 2000), c + Vec2(r + 1, 0), MonsterState::Chase, pack);
    rig.Kill(leader);
    REQUIRE(rig.M(m).state == MonsterState::Flee);
    bool entered = false, interrupted = false;
    int swings = 0, frozen = 0;
    for (int s = 0; s < 600; ++s) {
      const Vec2 before = rig.M(m).pos;
      rig.Step();
      entered |= rig.zone.InSafeZone(rig.M(m).pos);
      swings += K5Swings(rig.h.events, m);
      frozen += rig.M(m).pos == before && rig.M(m).state == MonsterState::Chase ? 1 : 0;
      for (const EvMonsterFlee& e : K5Events<EvMonsterFlee>(rig.h.events)) {
        interrupted |= e.ended && e.reason == FleeEnd::Interrupted;
      }
      rig.h.events.Clear();
    }
    CHECK_FALSE(entered);
    CHECK_FALSE(interrupted);
    CHECK(swings >= 2);    // it fought the hero (after its flee, or at once where the camp corners it)
    CHECK(frozen < 60);    // not stuck in Chase on the spot

    // The kite: its way straight back (and the 30-degree turns) lies in the camp; it turns 60 degrees about the hero.
    K5Rig rig2;
    rig2.PlaceHero(c + Vec2(r + 3.5, 0));
    const EntityId a = rig2.Spawn(K5Archer(), c + Vec2(r + 1.5, 0), MonsterState::Attack);
    rig2.Step();
    REQUIRE(rig2.M(a).kiting);
    CHECK_FALSE(rig2.zone.InSafeZone(rig2.M(a).kiteTarget));
    CHECK(Dist(rig2.M(a).kiteTarget, rig2.hero->Position()) <= rig2.M(a).def.kite.max + 1e-9);
    bool archerIn = false;
    int shots = 0;
    for (int s = 0; s < 600; ++s) {
      rig2.Step();
      archerIn |= rig2.zone.InSafeZone(rig2.M(a).pos);
      shots += K5Swings(rig2.h.events, a);
      rig2.h.events.Clear();
    }
    CHECK_FALSE(archerIn);
    CHECK(shots >= 2);

    // A kiting monster pushed into a camp all the same stops kiting there (the repel ends the step: Idle).
    K5Rig rig3;
    rig3.PlaceHero(Vec2(41.5, 40));
    const EntityId k = rig3.Spawn(K5Archer(), Vec2(40, 40), MonsterState::Attack);
    rig3.Step();
    REQUIRE(rig3.M(k).kiting);
    rig3.monsters.Teleport(k, c + Vec2(1, 0), TeleportReason::Debug);
    rig3.PlaceHero(c + Vec2(r + 3, 0));
    rig3.Step();
    CHECK_FALSE(rig3.M(k).kiting);
    const Snapshot snap = rig3.View();
    REQUIRE(rig3.ViewOf(snap, k) != nullptr);
    CHECK_FALSE(rig3.ViewOf(snap, k)->kiting);
  }

  TEST_CASE("K5 runtime: a buff ends with its caster: the drum lands, the drummer dies, the leader 0.5 s later: they break") {
    K5Rig rig;
    rig.PlaceHero(Vec2(80, 80));
    MonsterDef deserter = K5Member(0, 5000);
    deserter.morale.sustainFamily = "fx_living";
    deserter.morale.sustainRadius = 8;
    const MonsterAbilityDef drum =
        K5Buff(K5Cast("drum", AbilityKind::Call, 8000, 8, "fx_living"), MonsterBuffStat::FearImmune, 1, 3000);
    const MonsterAbilityDef march =
        K5Buff(K5Cast("march", AbilityKind::Buff, 8000, 8, "fx_living"), MonsterBuffStat::AttackSpeed, 1.1, 3000);
    const uint32_t pack = rig.monsters.NewPackId();
    const EntityId leader = rig.Spawn(K5Melee("fx_k5_chief"), Vec2(40, 40), MonsterState::Idle, pack, true);
    const EntityId drummer = rig.Spawn(K5Drummer(), Vec2(40, 44), MonsterState::Idle);
    const EntityId m1 = rig.Spawn(deserter, Vec2(42, 42), MonsterState::Idle, pack);
    // Another drummer far off (10.2 from m1: its drum does not reach it) keeps its own drum on m2.
    const EntityId drummer2 = rig.Spawn(K5Drummer(), Vec2(32, 40), MonsterState::Idle);
    const EntityId m2 = rig.Spawn(K5Member(0, 5000), Vec2(36, 40), MonsterState::Idle, pack);
    rig.monsters.ResolveBuffCast(drummer, drum);
    rig.monsters.ResolveBuffCast(drummer, march);
    rig.monsters.ResolveBuffCast(drummer2, drum);
    REQUIRE((rig.M(m1).fearImmune && rig.M(m1).buffAttackSpeedMul == 1.1 && rig.M(m2).fearImmune));
    REQUIRE(rig.M(m1).abilityBuffs.size() == 2);
    // The drummer dies: the buffs it gave end at once (its targets' multipliers too); drummer2's stay.
    rig.Kill(drummer);
    CHECK(rig.M(m1).abilityBuffs.empty());
    CHECK_FALSE(rig.M(m1).fearImmune);
    CHECK(rig.M(m1).buffAttackSpeedMul == 1);
    CHECK(rig.M(m1).AttackIntervalMs() == rig.M(m1).def.attackSpeedMs);
    CHECK(rig.M(m2).fearImmune);
    CHECK_FALSE(rig.monsters.Sustained(rig.M(m1)));  // drummer2 stands 10.2 off, past its sustainRadius
    rig.Step(30);
    // The leader falls half a second later, well inside the drum's 3 s: m1 breaks; m2 still holds drummer2's drum.
    rig.h.events.Clear();
    rig.Kill(leader);
    CHECK(rig.M(m1).state == MonsterState::Flee);
    CHECK(rig.M(m2).state != MonsterState::Flee);
    const std::vector<EvMonsterFlee> fled = K5Events<EvMonsterFlee>(rig.h.events);
    REQUIRE(fled.size() == 1);
    CHECK(fled[0].entity == m1);
    // A drummer with morale does not hold itself (a sustain is another monster): alone, it breaks with its pack.
    K5Rig rig2;
    rig2.PlaceHero(Vec2(80, 80));
    MonsterDef drumMember = K5Drummer();
    drumMember.morale = deserter.morale;
    const uint32_t p2 = rig2.monsters.NewPackId();
    const EntityId l2 = rig2.Spawn(K5Melee("fx_k5_chief"), Vec2(40, 40), MonsterState::Idle, p2, true);
    const EntityId d2 = rig2.Spawn(drumMember, Vec2(42, 40), MonsterState::Idle, p2);
    CHECK_FALSE(rig2.monsters.Sustained(rig2.M(d2)));
    rig2.Kill(l2);
    CHECK(rig2.M(d2).state == MonsterState::Flee);
  }

  TEST_CASE("K5 runtime: a flee keeps inside a K4 tether; the leash interrupts a flee; a second break leaves a flee alone") {
    // The tether bounds a flee as the leash does.
    K5Rig rig;
    rig.PlaceHero(Vec2(80, 80));
    const uint32_t pack = rig.monsters.NewPackId();
    const EntityId leader = rig.Spawn(K5Melee("fx_k5_chief"), Vec2(38, 40), MonsterState::Idle, pack, true);
    const EntityId m = rig.Spawn(K5Member(0, 20000), Vec2(40, 40), MonsterState::Idle, pack);
    rig.M(m).hasTether = true;
    rig.M(m).tetherCentre = Vec2(40.5, 40);
    rig.M(m).tetherRadius = 2;
    rig.Kill(leader);
    REQUIRE(rig.M(m).state == MonsterState::Flee);
    double farthest = 0;
    bool blocked = false;
    for (int s = 0; s < 600 && rig.M(m).state == MonsterState::Flee; ++s) {
      rig.Step();
      farthest = std::max(farthest, Dist(rig.M(m).pos, Vec2(40.5, 40)));
      for (const EvMonsterFlee& e : K5Events<EvMonsterFlee>(rig.h.events)) blocked |= e.ended && e.reason == FleeEnd::Blocked;
      rig.h.events.Clear();
    }
    CHECK(farthest <= 2 + 1e-6);
    CHECK(farthest > 1);  // it did run, as far as a probe 1 tile ahead stays inside the circle
    CHECK(blocked);       // out of room inside the tether: it turned to fight

    // A fleeing monster beyond its leash (pushed, carried) is interrupted by it: EvMonsterFlee{ended, interrupted},
    // then it walks home (M1).
    K5Rig rig2;
    rig2.PlaceHero(Vec2(80, 80));
    const uint32_t p2 = rig2.monsters.NewPackId();
    const EntityId l2 = rig2.Spawn(K5Melee("fx_k5_chief"), Vec2(38, 40), MonsterState::Idle, p2, true);
    const EntityId f2 = rig2.Spawn(K5Member(0, 20000), Vec2(40, 40), MonsterState::Idle, p2);
    rig2.Kill(l2);
    REQUIRE(rig2.M(f2).state == MonsterState::Flee);
    rig2.Step(5);
    REQUIRE(rig2.M(f2).state == MonsterState::Flee);
    rig2.M(f2).spawnAnchor = TilePos{20, 40};  // 20 tiles off: past the leash range
    rig2.h.events.Clear();
    rig2.Step();
    CHECK(rig2.M(f2).state == MonsterState::Returning);
    const std::vector<EvMonsterFlee> ev = K5Events<EvMonsterFlee>(rig2.h.events);
    REQUIRE(ev.size() == 1);
    CHECK((ev[0].entity == f2 && ev[0].ended && ev[0].reason == FleeEnd::Interrupted));

    // A second leader of the pack falls while a member flees (a respawned one, or a second leader by hand): its flee
    // runs on unchanged (no new event, the same time and body).
    K5Rig rig3;
    rig3.PlaceHero(Vec2(80, 80));
    const uint32_t p3 = rig3.monsters.NewPackId();
    const EntityId l3 = rig3.Spawn(K5Melee("fx_k5_chief"), Vec2(38, 40), MonsterState::Idle, p3, true);
    const EntityId l4 = rig3.Spawn(K5Melee("fx_k5_chief"), Vec2(44, 44), MonsterState::Idle, p3, true);
    const EntityId f3 = rig3.Spawn(K5Member(0, 5000), Vec2(40, 40), MonsterState::Idle, p3);
    rig3.Kill(l3);
    REQUIRE(rig3.M(f3).state == MonsterState::Flee);
    const double until = rig3.M(f3).fleeUntilMs;
    const Vec2 from = rig3.M(f3).fleeFrom;
    rig3.Step(30);
    rig3.h.events.Clear();
    rig3.Kill(l4);
    CHECK(K5Events<EvMonsterFlee>(rig3.h.events).empty());
    CHECK(rig3.M(f3).state == MonsterState::Flee);
    CHECK(rig3.M(f3).fleeUntilMs == until);
    CHECK(rig3.M(f3).fleeFrom == from);
    // Once that flee is over, a later break starts afresh: its last step's direction (+col) is forgotten, so with a new
    // leader falling on its +col side it runs straight back along -col (not a sidestep).
    REQUIRE(rig3.StepUntil([&]() { return rig3.M(f3).state != MonsterState::Flee; }, 600));
    REQUIRE(rig3.M(f3).fleeDir.x > 0.5);
    const EntityId l5 = rig3.Spawn(K5Melee("fx_k5_chief"), rig3.M(f3).pos + Vec2(2, 0), MonsterState::Idle, p3, true);
    rig3.Kill(l5);
    REQUIRE(rig3.M(f3).state == MonsterState::Flee);
    CHECK(rig3.M(f3).fleeDir == Vec2());
    rig3.Step();
    CHECK(rig3.M(f3).fleeDir == Vec2(-1, 0));
  }

  TEST_CASE("K5 runtime: a kiting archer that breaks stops kiting; the kite ends in Attack only inside its tether") {
    K5Rig rig;
    rig.PlaceHero(Vec2(41.5, 40));
    MonsterDef archer = K5Archer();
    archer.morale.present = true;
    archer.morale.fleeOnLeaderDeathMs = 1000;
    const uint32_t pack = rig.monsters.NewPackId();
    const EntityId leader = rig.Spawn(K5Melee("fx_k5_chief"), Vec2(30, 30), MonsterState::Idle, pack, true);
    const EntityId a = rig.Spawn(archer, Vec2(40, 40), MonsterState::Attack, pack);
    rig.Step();
    REQUIRE(rig.M(a).kiting);
    rig.Kill(leader);
    CHECK(rig.M(a).state == MonsterState::Flee);
    CHECK_FALSE(rig.M(a).kiting);  // the break ends the kite: no old kite step resumes after the flee
    bool kitingWhileFleeing = false;
    for (int s = 0; s < 120 && rig.M(a).state == MonsterState::Flee; ++s) {
      rig.Step();
      kitingWhileFleeing |= rig.M(a).kiting;
    }
    CHECK_FALSE(kitingWhileFleeing);
    {
      const Snapshot snap = rig.View();
      REQUIRE(rig.ViewOf(snap, a) != nullptr);
      CHECK_FALSE(rig.ViewOf(snap, a)->kiting);
    }

    // The kite ends in Attack with the hero in reach only when the monster stands inside its tether (a tether that
    // came in while it walked); outside, Chase walks it back first.
    const auto kiteEnd = [](Vec2 tetherCentre) {
      K5Rig r;
      r.PlaceHero(Vec2(41.5, 40));
      const EntityId k = r.Spawn(K5Archer(), Vec2(40, 40), MonsterState::Attack);
      r.Step();
      REQUIRE(r.M(k).kiting);
      REQUIRE(r.M(k).kiteTarget == Vec2(35.5, 40));
      r.M(k).hasTether = true;
      r.M(k).tetherCentre = tetherCentre;
      r.M(k).tetherRadius = 1;
      REQUIRE(r.StepUntil([&]() { return !r.M(k).kiting; }, 600));
      return r.M(k).state;
    };
    CHECK(kiteEnd(Vec2(35.5, 40)) == MonsterState::Attack);  // inside: the hero 6 off, in reach
    CHECK(kiteEnd(Vec2(45, 40)) == MonsterState::Chase);      // outside: it walks back in first
  }

  TEST_CASE("K5 runtime: a support cast is no strike: a kiter that casts after its kite does not kite again until it shoots") {
    K5Rig rig;
    rig.PlaceHero(Vec2(41.5, 40));
    MonsterDef archer = K5Archer();
    archer.abilities = {K5Buff(K5Cast("focus", AbilityKind::Buff, 20000, 0), MonsterBuffStat::AttackSpeed, 1.1, 500)};
    archer.abilities[0].initialCooldownMs = 600000;  // not during the first kite
    const EntityId a = rig.Spawn(archer, Vec2(40, 40), MonsterState::Attack);
    rig.Step();
    REQUIRE(rig.M(a).kiting);
    REQUIRE(rig.StepUntil([&]() { return Dist(rig.M(a).pos, rig.M(a).kiteTarget) < 0.5; }, 600));
    rig.M(a).lastAttackMs = rig.Now();  // it arrives with no swing due
    REQUIRE(rig.StepUntil([&]() { return !rig.M(a).kiting; }, 120));
    REQUIRE(rig.M(a).state == MonsterState::Attack);
    // The focus is ready now and the hero closes in: the cast comes first (between swings), then the shot.
    rig.M(a).abilityReadyAtMs[0] = rig.Now();
    rig.PlaceHero(rig.M(a).pos + Vec2(1.5, 0));
    rig.h.events.Clear();
    double castAt = -1, swingAt = -1, kiteAt = -1;
    for (int s = 0; s < 400 && kiteAt < 0; ++s) {
      rig.Step();
      if (castAt < 0 && !K5Events<EvMonsterBuffCast>(rig.h.events).empty()) castAt = rig.Now();
      if (swingAt < 0 && K5Swings(rig.h.events, a) > 0) swingAt = rig.Now();
      if (rig.M(a).kiting) kiteAt = rig.Now();
      rig.h.events.Clear();
    }
    REQUIRE(castAt > 0);
    REQUIRE(swingAt > 0);
    CHECK(castAt < swingAt);
    CHECK(rig.M(a).lastStrikeMs == doctest::Approx(swingAt));
    REQUIRE(kiteAt > 0);
    CHECK(kiteAt > swingAt);  // not after the cast alone
  }

  TEST_CASE("K5 runtime: a damage buff reaches its owner's ability hits on monsters (hitsMonsters)") {
    const auto firstHit = [](bool buffed) {
      K5Rig r(99);
      r.PlaceHero(Vec2(41, 40));
      MonsterDef brute = K5Melee("fx_k5_brute", 6);
      MonsterAbilityDef slam;
      slam.id = "slam";
      slam.kind = AbilityKind::Circle;
      slam.clip = "Attack01";
      slam.aim = AbilityAim::Self;
      slam.hasShape = true;
      slam.shape.kind = AbilityShapeKind::Circle;
      slam.shape.radius = 3;
      slam.cooldownMs = 8000;
      slam.hitsMonsters = true;
      brute.abilities = {slam};
      if (buffed) {  // its own damage aura (radius 0.5: itself alone)
        brute.aura.present = true;
        brute.aura.stat = MonsterBuffStat::Damage;
        brute.aura.mul = 1.5;
        brute.aura.radius = 0.5;
      }
      MonsterDef victim = K5Melee("fx_k5_victim");
      victim.aggroRange = 0.1;
      const EntityId b = r.Spawn(brute, Vec2(40, 40), MonsterState::Attack);
      const EntityId v = r.Spawn(victim, Vec2(40, 41.5), MonsterState::Idle);
      for (int s = 0; s < 300; ++s) {
        r.Step();
        for (const EvHit& h : K5Events<EvHit>(r.h.events)) {
          if (h.target == v && h.source == b) return h.amount;
        }
        r.h.events.Clear();
      }
      return -1.0;
    };
    const double plainHit = firstHit(false), buffedHit = firstHit(true);
    REQUIRE(plainHit > 0);
    CHECK(buffedHit > plainHit);
  }

  // =================================================================================================================
  // Wiring and replay
  // =================================================================================================================
  TEST_CASE("K5 wiring: GameSim spawns a mixed pack through the population; the kill pipeline breaks it; the snapshot") {
    const DataStore& base = K5Data();
    const size_t i = K5PlainSpawn(base, 3);
    const MapSpawnDef plain = base.World().FindMap(base.World().defaultMap)->spawns[i];
    static std::unique_ptr<DataStore> store;
    if (store == nullptr) {
      DataLoadReport report;
      const std::string mix = StrCat(R"([{"monsterId": "fx_k5_member", "count": )", plain.count - 1,
                                     R"(}, {"monsterId": "fx_k5_chief", "count": 1}])");
      store = K5LoadEdited({{"assets.json", K5AddAsset},
                            {"monsters.json", K5AddDefs},
                            {"maps.json", K5EditSpawn(base, i, mix, R"({"monsterId": "fx_k5_chief", "elite": true})",
                                                      "fx_k5_member")}},
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
    EntityId chief = kNoEntity;
    std::vector<EntityId> members;
    for (const MonsterInstance& m : ctx.sys.monsters->All()) {
      if (!m.IsAlive()) continue;
      if (m.def.id == "fx_k5_chief") chief = m.id;
      if (m.def.id == "fx_k5_member") members.push_back(m.id);
    }
    REQUIRE(chief != kNoEntity);
    REQUIRE(static_cast<int32_t>(members.size()) == plain.count - 1);
    const MonsterInstance* c = ctx.sys.monsters->Find(chief);
    CHECK((c->packLeader && c->packId != 0 && c->def.elite && c->AffixCount() >= 1));
    CHECK(c->homeTile == plain.pos);
    for (EntityId id : members) CHECK(ctx.sys.monsters->Find(id)->packId == c->packId);
    const auto view = [&](EntityId id) -> const MonsterView* {
      for (const MonsterView& v : sim->View().monsters) {
        if (v.id == id) return &v;
      }
      return nullptr;
    };
    REQUIRE(view(chief) != nullptr);
    CHECK((view(chief)->packLeader && view(chief)->packId == c->packId && view(chief)->elite));
    REQUIRE(view(members.front()) != nullptr);
    CHECK((!view(members.front())->packLeader && view(members.front())->packId == c->packId));
    // The quest guide's spawn points read a mixed entry's members: the chief (only in the mix) points at the entry too.
    REQUIRE(ctx.sys.questWorld != nullptr);
    const GuideWorld guide = ctx.sys.questWorld->MakeGuideWorld();
    for (const char* who : {"fx_k5_chief", "fx_k5_member"}) {
      INFO(who);
      std::vector<Vec2> pts;
      guide.spawns({std::string(who)}, pts);
      CHECK(std::find(pts.begin(), pts.end(), plain.pos.Center()) != pts.end());
    }
    {
      std::vector<Vec2> pts;
      guide.spawns({plain.monsterId}, pts);  // the entry's old monster no longer stands there
      CHECK(std::find(pts.begin(), pts.end(), plain.pos.Center()) == pts.end());
    }
    // The chief falls (through CombatSystem and SimWiring's kill pipeline): its pack breaks.
    MonsterHitRequest r;
    r.monster = chief;
    r.amount = c->hp;
    r.attacker = kHeroEntityId;
    r.source = KillSource::HeroSkill;
    r.provokes = false;
    ctx.sys.combat->DamageMonster(r);
    const std::vector<EvMonsterFlee> fled = K5Events<EvMonsterFlee>(sim->Events());
    REQUIRE(fled.size() == members.size());
    for (size_t k = 0; k < fled.size(); ++k) {
      CHECK((fled[k].entity == members[k] && fled[k].leader == chief && !fled[k].ended));
      CHECK(ctx.sys.monsters->Find(members[k])->state == MonsterState::Flee);
    }
    sim->Step();
    for (EntityId id : members) {
      const MonsterView* v = view(id);
      REQUIRE(v != nullptr);
      // Fleeing (or, where the ground closes in at once, fighting again: blocked).
      CHECK((v->state == MonsterState::Flee || v->state == MonsterState::Chase));
      if (v->state == MonsterState::Flee) CHECK(v->fleeUntilMs == doctest::Approx(fled.front().untilMs));
    }
    // An alarm in the pack: a member provoked by the hero wakes the others within 6 (fx_k5_member: packAggro 6).
    auto sim2 = GameSim::Create(*store, SimConfig{});
    REQUIRE(sim2 != nullptr);
    REQUIRE(sim2->NewGame(ClassId::Warrior, Difficulty::Normal, 79));
    sim2->Context().sys.story->FinishAllBeats();
    sim2->Step();
    SimContext& c2 = sim2->Context();
    std::vector<EntityId> pack;
    for (const MonsterInstance& m : c2.sys.monsters->All()) {
      if (m.IsAlive() && (m.def.id == "fx_k5_member" || m.def.id == "fx_k5_chief")) pack.push_back(m.id);
    }
    REQUIRE(pack.size() == static_cast<size_t>(plain.count));
    MonsterHitRequest poke;
    poke.monster = pack.front();
    poke.amount = 1;
    poke.attacker = kHeroEntityId;
    poke.source = KillSource::HeroSkill;
    c2.sys.combat->DamageMonster(poke);  // provokes (M2)
    const Vec2 at = c2.sys.monsters->Find(pack.front())->pos;
    for (EntityId id : pack) {
      const MonsterInstance* m = c2.sys.monsters->Find(id);
      const bool joins = id == pack.front() || (m->def.packAggro > 0 && Dist(m->pos, at) <= m->def.packAggro);
      INFO("member " << id);
      CHECK((m->state == MonsterState::Chase) == joins);
    }
  }

  TEST_CASE("K5 replay: a seeded pack fight (alarm, morale, kite, block, wall, drum, aura) replays to the pinned digest") {
    struct Replay {
      uint64_t digest = 0;
      int alarms = 0, flees = 0, fleeEnds = 0, kites = 0, blocks = 0, casts = 0, hits = 0;
    };
    const auto run = [](uint64_t seed) {
      Replay out;
      K5Rig rig(seed);
      rig.PlaceHero(Vec2(46, 40));
      const uint32_t pack = rig.monsters.NewPackId();
      MonsterDef member = K5Member(6, 3000, "fx_k5_member");
      member.damage = 4;
      member.morale.sustainFamily = "fx_living";
      member.morale.sustainRadius = 6;
      MonsterDef chief = K5Melee("fx_k5_chief", 5);
      chief.hp = 260;
      MonsterDef drummer = K5Drummer();
      drummer.hp = 120;
      drummer.packAggro = 6;  // it answers the pack's alarm (then drums between its swings)
      MonsterDef archer = K5Archer();
      MonsterDef shield = K5Shield(2);
      shield.damage = 3;
      MonsterDef banner = K5Shield(0, "fx_k5_banner");
      banner.block.present = false;
      banner.aura.present = true;
      banner.aura.stat = MonsterBuffStat::Damage;
      banner.aura.mul = 1.15;
      banner.aura.radius = 5;
      banner.aura.family = "fx_unburied";
      std::vector<EntityId> ids;
      ids.push_back(rig.Spawn(chief, Vec2(40, 40), MonsterState::Idle, pack, true));
      for (const Vec2 p : {Vec2(41, 42), Vec2(39, 38), Vec2(38, 41)}) ids.push_back(rig.Spawn(member, p, MonsterState::Idle, pack));
      ids.push_back(rig.Spawn(drummer, Vec2(36, 44), MonsterState::Idle, pack));
      ids.push_back(rig.Spawn(archer, Vec2(48, 46), MonsterState::Idle));
      ids.push_back(rig.Spawn(shield, Vec2(47, 34), MonsterState::Idle));
      ids.push_back(rig.Spawn(shield, Vec2(48.5, 34), MonsterState::Idle));
      ids.push_back(rig.Spawn(banner, Vec2(50, 35), MonsterState::Idle));
      K5Fnv f;
      const Vec2 loop[] = {Vec2(46, 40), Vec2(43, 40), Vec2(41, 41), Vec2(47, 44), Vec2(48, 37), Vec2(44, 36)};
      for (int s = 0; s < 40 * 60; ++s) {
        if (s % 90 == 0 && rig.hero->Life() == HeroLife::Alive) rig.PlaceHero(loop[(s / 90) % 6]);
        if (s % 45 == 30) {
          // A blow on whatever stands nearest the hero (from the hero: blocks read its side).
          const EntityId t = rig.monsters.NearestAlive(rig.hero->Position(), 3);
          if (t != kNoEntity && t != rig.dummy) {
            rig.Hit(t, 14 + static_cast<double>(rig.h.rng.Get(RngStream::Combat).RandomInt(0, 10)), std::nullopt, true);
          }
        }
        if (s == 900 && rig.monsters.Find(ids[4]) != nullptr && rig.M(ids[4]).IsAlive()) rig.Kill(ids[4]);  // the drum
        if (s == 960 && rig.monsters.Find(ids[0]) != nullptr && rig.M(ids[0]).IsAlive()) rig.Kill(ids[0]);  // the chief
        const size_t aggroBefore = rig.aggro.size();
        rig.Step();
        out.alarms += static_cast<int>(rig.aggro.size() - aggroBefore);
        K5DigestEvents(f, rig.h.events);
        for (const EvMonsterFlee& e : K5Events<EvMonsterFlee>(rig.h.events)) (e.ended ? out.fleeEnds : out.flees) += 1;
        out.blocks += static_cast<int>(K5Events<EvBlock>(rig.h.events).size());
        out.casts += static_cast<int>(K5Events<EvMonsterBuffCast>(rig.h.events).size());
        for (const EvHit& h : K5Events<EvHit>(rig.h.events)) out.hits += h.target == kHeroEntityId ? 1 : 0;
        for (const EntityId id : ids) {
          const MonsterInstance* m = rig.monsters.Find(id);
          if (m != nullptr && m->kiting && m->kiteUntilMs > rig.Now() - kSimStepMs && m->state == MonsterState::Chase &&
              m->pos == m->prevPos) {
            ++out.kites;  // the tick a kite starts (no movement on it)
          }
        }
        rig.h.events.Clear();
      }
      f.D(rig.hero->Hp());
      for (const MonsterInstance& m : rig.monsters.All()) {
        f.U64(m.id);
        f.D(m.hp);
        f.V(m.pos);
        f.V(m.heading);
        f.U64(static_cast<uint64_t>(m.state));
      }
      out.digest = f.h;
      return out;
    };
    const Replay r = run(6160);
    // The fight used every mechanism.
    CHECK(r.alarms >= 3);
    CHECK(r.flees == 3);  // the drummer fell first (its drum ended with it), then the chief: all three members broke
    CHECK(r.fleeEnds >= 1);
    CHECK(r.kites >= 1);
    CHECK(r.blocks >= 1);
    CHECK(r.casts >= 1);
    CHECK(r.hits >= 10);
    CHECK(r.digest == run(6160).digest);  // same seed, same log
    CHECK(r.digest != run(6161).digest);  // the seed feeds the rolls
    char hex[17];
    std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(r.digest));
    INFO("digest " << hex);
    CHECK(std::string(hex) == "3126917caf246261");
  }
}
