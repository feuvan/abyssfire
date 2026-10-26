// K3 on-death effects and the corpse registry (dark remake, implementation-plan.md 3 "K3", work package 2B): the schema
// (monster onDeath / corpse / riseAgain / family, the summon radius, elite affix onDeath / riseAgain / families and the
// `unburied` type, map corpseSeeds: loads, loader refusals, DataStore cross checks), the pure rules (monsters/OnDeath.h),
// the Corpses container (TTL, cap and eviction order, seeds, query order, holds) and the runtime through MonsterSystem /
// CombatSystem: bodies from the kill pipeline and fire killing blows, raises and summons (markers, holds, caps, Raised),
// rewards, respawn and collapse, burning (fire strikes, hazard ticks, hero fire skills), consume, the Unburied rise
// (trait and affix, the family-restricted roll), bursts / rings / puddles / spawns (a monster's, an affix's, the volatile
// curse's), the GameSim wiring, and a seeded replay whose event-log digest is pinned (GCC and Clang must both match it).
// The 2B review added: raise / summon casts between swings, every release of a corpse hold, consume against swings, the
// Raised reward skips through GameSim, the on-death damage inputs, rise / collapse rules, hero area and beast fire burns.
//
// Synthetic content only: monster definitions are built here (TestContent.h fixtures); one synthetic asset (`fx_k3_art`,
// its Cast01 / Rise / Consume clips), two synthetic definitions (fx_k3_risen, fx_k3_clot) and the affix rows below are
// added to the real export in memory, so no shipped content id is spelled and no content change moves these tests.
// Suite: monsters.
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
#include "abyss/hero/Skills.h"
#include "abyss/items/Inventory.h"
#include "abyss/monsters/Corpses.h"
#include "abyss/monsters/MonsterAbilities.h"
#include "abyss/monsters/OnDeath.h"
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

constexpr char kArt[] = "fx_k3_art";  // the synthetic asset's game id

// Clips of the synthetic asset: Attack01 Contact 500 / 1000 ms, Cast01 Release 900 / 1500 ms, Rise 1200 ms, Consume a
// 1000 ms loop.
constexpr char kK3AssetJson[] = R"({
  "kind": "SkeletalMesh", "category": "Monsters", "gameIds": ["fx_k3_art"], "skeleton": "SKEL_Human",
  "anims": [
    {"name": "Attack01", "lengthMs": 1000, "contactMs": 500, "notifies": [{"name": "Contact", "ms": 500}]},
    {"name": "Cast01", "lengthMs": 1500, "releaseMs": 900, "notifies": [{"name": "Release", "ms": 900}]},
    {"name": "Rise", "lengthMs": 1200, "notifies": [{"name": "RiseUp", "ms": 840}]},
    {"name": "Consume", "lengthMs": 1000, "loop": true}
  ]
})";

JsonValue K3Parse(std::string_view text) {
  JsonValue v;
  JsonParseError err;
  const bool ok = ParseJson(text, v, &err);
  REQUIRE_MESSAGE(ok, "bad test JSON");
  return v;
}

using K3Edits = std::vector<std::pair<std::string, std::function<void(JsonValue&)>>>;

// The real export with `edits` applied in memory (several edits of one file run in order on one document).
std::unique_ptr<DataStore> K3LoadEdited(const K3Edits& edits, DataLoadReport& report) {
  std::map<std::string, JsonValue> docs;
  for (const auto& [file, edit] : edits) {
    if (docs.count(file) == 0) docs.emplace(file, K3Parse(test::ReadFile(test::DataDir() + "/" + file)));
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

void K3AddAsset(JsonValue& assets) {
  JsonValue* list = assets.FindMutable("manifest");
  REQUIRE(list != nullptr);
  list = list->FindMutable("assets");
  REQUIRE(list != nullptr);
  list->Set("SK_Fx_K3", K3Parse(kK3AssetJson));
}

// A monsters.json definition (with its exporter `derived` block) and extra top-level members (`extra`, JSON object
// members without braces).
std::string K3DefJson(std::string_view id, std::string_view extra, int level = 2, int hp = 50) {
  return StrCat(R"({"id": ")", id, R"(", "name": ")", id, R"(", "level": )", level, R"(, "hp": )", hp,
                R"(, "damage": 7, "defense": 5,
    "speed": 32, "aggroRange": 4, "attackRange": 1.5, "attackSpeed": 1600, "expReward": 11, "goldReward": [2, 5],
    "spriteKey": "fx_k3_art", "animCategory": "humanoid")",
                extra.empty() ? "" : ", ", extra, R"(, "derived": {"isRanged": false, "onHitStatus": [],
    "projectileColor": 13386956, "homeZone": null, "nameKey": "data.monster.)",
                id, R"(", "sources": ["zone"]}})");
}

void K3AddDef(JsonValue& monsters, const std::string& json) {
  JsonValue def = K3Parse(json);
  const std::string id(def.Get("id").AsString());
  JsonValue* defs = monsters.FindMutable("defs");
  REQUIRE(defs != nullptr);
  defs->Set(id, std::move(def));
}

// The synthetic definitions a raise / summon / on-death spawn stands up (they must be data: the runtime looks them up).
void K3AddDefs(JsonValue& monsters) {
  K3AddDef(monsters, K3DefJson("fx_k3_risen", R"("family": "unburied", "corpse": {"leaves": "corpse"})"));
  K3AddDef(monsters, K3DefJson("fx_k3_clot", R"("family": "breach")", 1, 20));
}

// elite_affixes.json: Scorched's burst (1 s, r2, 10 % of the elite's life, fire, capped at 20 %), Gravecold's frost ring
// (600 ms, r2.5, slow 30 for 2.5 s) and the Unburied affix (rises at 50 % after 3 s; Unburied family only).
void K3EditAffixes(JsonValue& doc) {
  JsonValue* defs = doc.FindMutable("definitions");
  REQUIRE(defs != nullptr);
  JsonValue* fire = defs->FindMutable("fire_enhanced");
  REQUIRE(fire != nullptr);
  fire->Set("onDeath", K3Parse(R"({"kind": "burst", "delayMs": 1000, "shape": {"radius": 2}, "hpPct": 10,
                                   "damageType": "fire", "capHeroMaxHpPct": 20})"));
  JsonValue* frozen = defs->FindMutable("frozen");
  REQUIRE(frozen != nullptr);
  frozen->Set("onDeath", K3Parse(R"({"kind": "ring", "delayMs": 600, "shape": {"kind": "circle", "radius": 2.5},
                                     "status": {"type": "slow", "ms": 2500, "value": 30}})"));
  JsonValue unburied = *fire;
  unburied.Remove("onDeath");
  unburied.Set("type", JsonValue::String("unburied"));
  unburied.Set("riseAgain", K3Parse(R"({"delayMs": 3000, "hpPct": 50})"));
  unburied.Set("families", K3Parse(R"(["unburied"])"));
  defs->Set("unburied", std::move(unburied));
  JsonValue* order = doc.FindMutable("order");
  REQUIRE(order != nullptr);
  order->Append(JsonValue::String("unburied"));
}

// The real export plus the synthetic asset, definitions and affix rows (loaded once).
const DataStore& K3Data() {
  static std::unique_ptr<DataStore> store;
  if (store == nullptr) {
    DataLoadReport report;
    store = K3LoadEdited({{"assets.json", K3AddAsset}, {"monsters.json", K3AddDefs}, {"elite_affixes.json", K3EditAffixes}},
                         report);
    if (!report.Ok()) MESSAGE(report.Summary());
    REQUIRE(report.Ok());
  }
  return *store;
}

std::unique_ptr<DataStore> K3LoadDef(std::string defJson, DataLoadReport& report) {
  return K3LoadEdited({{"assets.json", K3AddAsset},
                       {"monsters.json", K3AddDefs},
                       {"monsters.json", [defJson](JsonValue& doc) { K3AddDef(doc, defJson); }}},
                      report);
}

bool K3HasIssue(const std::vector<DataIssue>& issues, std::string_view pathPart, std::string_view text) {
  for (const DataIssue& e : issues) {
    if (Contains(e.path, pathPart) && Contains(e.message, text)) return true;
  }
  return false;
}

// ---- synthetic definitions ------------------------------------------------------------------------------------------

MonsterAbilityDef K3Ability(std::string id, AbilityKind kind, AbilityAim aim, std::string clip = "Cast01") {
  MonsterAbilityDef a;
  a.id = std::move(id);
  a.kind = kind;
  a.clip = std::move(clip);
  a.aim = aim;
  return a;
}

// A raise of `count` fx_k3_risen at 40 % (cap / window optional), Cast01 (contact 900 from its Release beat).
MonsterAbilityDef K3Raise(std::string id, int32_t count, int32_t capAlive = 0, double cooldownMs = 8000) {
  MonsterAbilityDef a = K3Ability(std::move(id), AbilityKind::Raise, AbilityAim::Corpse);
  a.cooldownMs = cooldownMs;
  a.summon.present = true;
  a.summon.monsterId = "fx_k3_risen";
  a.summon.count = count;
  a.summon.at = AbilitySummonAt::Corpse;
  a.summon.capAlive = capAlive;
  a.summon.hpPct = 40;
  return a;
}

// A body that leaves a corpse: level 3, 60 HP, 8 damage, reach 1.5, a swing every 1500 ms, family unburied.
MonsterDef K3Footman(const char* id = "fx_k3_footman") {
  MonsterDef d = test::fixture::MakeFixtureMonster(id, 3, 60, 8, 2, 40, 6, 1.5, 1500, 20, 2, 4, kArt, AnimRig::Humanoid,
                                                   false, false);
  d.family = "unburied";
  d.corpse.leaves = CorpseLeaves::Corpse;
  return d;
}

// A sturdy melee caster on the synthetic art: level 6, 400 HP, 10 damage, reach 2, a swing every 1500 ms.
MonsterDef K3Caster(std::vector<MonsterAbilityDef> abilities, const char* id = "fx_k3_caster") {
  MonsterDef d = test::fixture::MakeFixtureMonster(id, 6, 400, 10, 5, 40, 6, 2.0, 1500, 30, 3, 6, kArt, AnimRig::Humanoid,
                                                   false, false);
  d.abilities = std::move(abilities);
  return d;
}

// A corpse eater (the Gravegnawer): level 3, 42 HP, leaves remains.
MonsterDef K3Gnawer() {
  MonsterDef d = test::fixture::MakeFixtureMonster("fx_k3_gnawer", 3, 42, 7, 3, 66, 6, 1.3, 950, 17, 1, 4, kArt,
                                                   AnimRig::Humanoid, false, false);
  d.family = "gravegnawers";
  d.corpse.leaves = CorpseLeaves::Remains;
  d.corpse.consumes = true;
  return d;
}

// An exploder: 40 HP, a fire burst of `dmg` after 800 ms, r2.2, monsters x2, capped at 20 % of the hero's life.
MonsterDef K3Bloat(double dmg = 18) {
  MonsterDef d = test::fixture::MakeFixtureMonster("fx_k3_bloat", 4, 40, 4, 2, 30, 5, 1.2, 1800, 18, 1, 3, kArt,
                                                   AnimRig::Slime, false, false);
  d.family = "breach";
  d.onDeath.present = true;
  d.onDeath.kind = OnDeathKind::Burst;
  d.onDeath.delayMs = 800;
  d.onDeath.shape.kind = AbilityShapeKind::Circle;
  d.onDeath.shape.radius = 2.2;
  d.onDeath.hasDmg = true;
  d.onDeath.dmg = dmg;
  d.onDeath.hasDamageType = true;
  d.onDeath.damageType = DamageType::Fire;
  d.onDeath.hitsMonsters = true;
  d.onDeath.monsterDmgMul = 2;
  d.onDeath.capHeroMaxHpPct = 20;
  return d;
}

// ---- runtime rig: the combat systems around a SimHarness on the synthetic data, stepped in the GameSim order, with the
// kill pipeline's K3 step wired as SimWiring does (combat, then the K3 step, then the respawn decision) ----------------
struct K3Rig {
  explicit K3Rig(uint64_t seed = 5, ClassId cls = ClassId::Warrior, Difficulty difficulty = Difficulty::Normal,
                 const DataStore* data = nullptr)
      : h(seed, data != nullptr ? *data : K3Data()),
        hero(std::make_unique<Hero>(h.ctx.data, cls)),
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
      ++kills;
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
  // The killing blow: the monster's whole life as one hit of `element` (a crit when `crit`), from the hero.
  void Kill(EntityId id, DamageType element = DamageType::Physical, bool crit = false) {
    MonsterHitRequest r;
    r.monster = id;
    r.amount = M(id).hp;
    r.isCrit = crit;
    r.element = element;
    r.attacker = kHeroEntityId;
    r.source = KillSource::HeroSkill;
    r.provokes = false;
    combat.DamageMonster(r);
  }
  // A monster that dies where it stands and leaves its body (a role that never respawns: no new eater walks in).
  uint32_t CorpseAt(const MonsterDef& def, Vec2 pos) {
    const EntityId id = Spawn(def, pos, MonsterState::Idle, MonsterRole::DefendWave);
    Kill(id);
    const CorpseEntry* c = Newest();
    REQUIRE(c != nullptr);
    return c->id;
  }
  const CorpseEntry* Newest() const {
    const auto e = monsters.CorpseRegistry().Entries();
    return e.empty() ? nullptr : &e.back();
  }
  const CorpseEntry* Corpse(uint32_t id) const { return monsters.CorpseRegistry().Find(id); }
  void PlaceHero(Vec2 p) { loco.Teleport(p, TeleportReason::Debug); }
  void Silence(EntityId id) { status.Apply(id, StatusType::Stun, 1, 600000, kHeroEntityId, Now()); }
  void ScriptNoDodge(int hits = 1) {
    std::vector<double> v(static_cast<size_t>(2 * hits), 0.99);
    h.rng.Get(RngStream::Combat).Script(v);
  }
  // The Raised bodies standing for `raiser` (alive), list order.
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
  EntityId dummy = kNoEntity;
  int kills = 0;  // MonsterKilledMsg count
};

template <class E>
std::vector<E> K3Events(const EventSink& sink) {
  std::vector<E> out;
  for (const Event& e : sink.Items()) {
    if (const E* p = std::get_if<E>(&e)) out.push_back(*p);
  }
  return out;
}

template <class E>
std::vector<E> K3Events(std::span<const Event> events) {
  std::vector<E> out;
  for (const Event& e : events) {
    if (const E* p = std::get_if<E>(&e)) out.push_back(*p);
  }
  return out;
}

std::vector<EvCorpseRemoved> Removals(const EventSink& sink, CorpseRemoveReason reason) {
  std::vector<EvCorpseRemoved> out;
  for (const EvCorpseRemoved& r : K3Events<EvCorpseRemoved>(sink)) {
    if (r.reason == reason) out.push_back(r);
  }
  return out;
}

// Landed hits on the hero (from `source` when set).
std::vector<EvHit> HeroHits(const EventSink& sink, EntityId source = kNoEntity) {
  std::vector<EvHit> out;
  for (const EvHit& e : K3Events<EvHit>(sink)) {
    if (e.target == kHeroEntityId && !e.dodged && (source == kNoEntity || e.source == source)) out.push_back(e);
  }
  return out;
}

// What a fixed on-death / hazard amount of `type` leaves after the hero's resistance (OnDeath.h header rule).
double AfterResist(const K3Rig& rig, double amount, DamageType type) {
  if (type == DamageType::Physical) return amount;
  const double r = ResistancePercent(rig.h.ctx.data.Classes().formulas, &rig.h.ctx.equip, type);
  return (std::max)(1.0, std::floor(amount * (1 - r / 100.0)));
}

// ---- review fixes (2B review) ---------------------------------------------------------------------------------------

// A walkable 6 x 5 patch of the live zone, well away from every camp (the GameSim wiring cases fight there).
TilePos K3QuietSpot(const ZoneRuntime& zone) {
  const MapDef& map = zone.Map();
  TilePos spot{-1, -1};
  for (int32_t r = 3; r < map.rows - 3 && spot.col < 0; ++r) {
    for (int32_t c = 3; c < map.cols - 3 && spot.col < 0; ++c) {
      bool ok = true;
      for (int32_t dr = -2; dr <= 2 && ok; ++dr) {
        for (int32_t dc = -2; dc <= 3 && ok; ++dc) ok = zone.Walkable(c + dc, r + dr);
      }
      for (const MapCampDef& camp : map.camps) ok = ok && Dist(Vec2(c, r), camp.pos.Center()) > map.safeZoneRadius + 4;
      if (ok) spot = TilePos{c, r};
    }
  }
  return spot;
}

// K3Rig plus a ley-beast at the hero's side (the systems PetCompanion talks to), `petId` active; its Tick is stepped by
// the case.
struct K3PetRig : K3Rig {
  explicit K3PetRig(const std::string& petId)
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

// The first beast of the data whose every hit at its first stage is fire (its element, and its stage-0 abilities').
std::string K3FirePet(const DataStore& data) {
  for (const PetDef& p : data.Pets().pets) {
    bool fire = p.element == DamageType::Fire;
    for (const PetAbilityDef& a : p.abilities) {
      if (a.unlock == 0 && a.hasElement) fire = fire && a.element == DamageType::Fire;
    }
    if (fire) return p.id;
  }
  return {};
}

// One area skill per cast shape whose area lands at release (no delay, no persistent ground): nova, ground circle,
// cone, line; the class that owns it. Picked from the real data, so no skill id is spelled here.
struct K3AreaSkill {
  CastShape shape = CastShape::Nova;
  ClassId cls = ClassId::Warrior;
  std::string id;
};
std::vector<K3AreaSkill> K3AreaSkills(const DataStore& data) {
  std::vector<K3AreaSkill> out;
  for (CastShape shape : {CastShape::Nova, CastShape::Ground, CastShape::Cone, CastShape::Line}) {
    for (ClassId cls : {ClassId::Warrior, ClassId::Mage, ClassId::Rogue}) {
      const Hero hero(data, cls);
      const SkillBook& book = hero.Skills();
      for (size_t i = 0; i < book.SkillCount(); ++i) {
        const SkillDef& sk = book.Skill(static_cast<int32_t>(i));
        if (sk.passive || !sk.aoe || sk.aoeDelayMs > 0 || sk.port.persistentGround || SkillCastShape(sk) != shape) continue;
        if (out.empty() || out.back().shape != shape) out.push_back(K3AreaSkill{shape, cls, sk.id});
      }
    }
  }
  return out;
}

// ---- replay digest --------------------------------------------------------------------------------------------------

struct K3Fnv {
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

// Every event's variant index, and the fields of the ones a K3 fight produces.
void K3DigestEvents(K3Fnv& f, const EventSink& sink) {
  for (const Event& e : sink.Items()) {
    f.U64(e.index());
    if (const EvPlayAnim* a = std::get_if<EvPlayAnim>(&e)) {
      f.U64(a->entity);
      f.U64(static_cast<uint64_t>(a->action));
      f.S(a->clip);
      f.D(a->startMs);
      f.D(a->durationMs);
    } else if (const EvHit* h = std::get_if<EvHit>(&e)) {
      f.U64(h->target);
      f.U64(h->source);
      f.D(h->amount);
      f.U64((h->crit ? 1u : 0u) | (h->dodged ? 2u : 0u) | (h->tick ? 4u : 0u) | (h->killed ? 8u : 0u));
    } else if (const EvTelegraph* t = std::get_if<EvTelegraph>(&e)) {
      f.U64(t->entity);
      f.S(t->abilityId);
      f.U64(static_cast<uint64_t>(t->kind));
      f.U64(static_cast<uint64_t>(t->source));
      f.V(t->origin);
      f.D(t->startMs);
      f.D(t->contactMs);
    } else if (const EvCorpseAdded* c = std::get_if<EvCorpseAdded>(&e)) {
      f.U64(c->corpseId);
      f.U64(c->monster);
      f.S(c->defId);
      f.V(c->pos);
      f.D(c->expiresMs);
      f.U64(c->spent ? 1u : 0u);
    } else if (const EvCorpseRemoved* r = std::get_if<EvCorpseRemoved>(&e)) {
      f.U64(r->corpseId);
      f.U64(static_cast<uint64_t>(r->reason));
      f.U64(r->by);
    } else if (const EvMonsterRaised* m = std::get_if<EvMonsterRaised>(&e)) {
      f.U64(m->entity);
      f.U64(m->raiser);
      f.U64(m->corpseId);
      f.V(m->pos);
      f.U64(m->selfRise ? 1u : 0u);
    } else if (const EvMonsterCollapsed* k = std::get_if<EvMonsterCollapsed>(&e)) {
      f.U64(k->entity);
      f.U64(k->raiser);
    } else if (const EvStatusApplied* s = std::get_if<EvStatusApplied>(&e)) {
      f.U64(s->target);
      f.U64(static_cast<uint64_t>(s->type));
    }
  }
}

}  // namespace

TEST_SUITE("monsters") {
  // =================================================================================================================
  // Data: the K3 schema (LoadMonsters.cpp, LoadCombat.cpp, LoadWorld.cpp) and DataStore::Finalize's cross checks
  // =================================================================================================================
  TEST_CASE("K3 data: onDeath, corpse, riseAgain, family and the summon radius load on a monster") {
    DataLoadReport report;
    const auto store = K3LoadDef(
        K3DefJson("fx_k3_full", R"("family": "unburied",
          "onDeath": {"kind": "burst", "delayMs": 800, "shape": {"radius": 2.2}, "dmg": 18, "damageType": "fire",
                      "status": {"type": "burn", "chance": 0.5, "ms": 2000, "value": 3}, "hitsMonsters": true,
                      "monsterDmgMul": 2, "capHeroMaxHpPct": 20},
          "corpse": {"leaves": "corpse", "consumes": true},
          "riseAgain": {"chance": 0.25, "delayMs": 3000, "hpPct": 40, "notOnCrit": true},
          "attackRange": 2,
          "abilities": [{"id": "flock", "kind": "raise", "clip": "Cast01", "cooldownMs": 8000, "aim": "corpse",
                         "summon": {"monsterId": "fx_k3_risen", "count": 2, "capAlive": 3, "hpPct": 40, "radius": 5}}])"),
        report);
    if (!report.Ok()) MESSAGE(report.Summary());
    REQUIRE(report.Ok());
    const MonsterDef* d = store->FindMonster("fx_k3_full");
    REQUIRE(d != nullptr);
    CHECK(d->family == "unburied");
    const MonsterOnDeathDef& o = d->onDeath;
    CHECK((o.present && o.kind == OnDeathKind::Burst && o.delayMs == 800 && o.shape.kind == AbilityShapeKind::Circle));
    CHECK((o.shape.radius == 2.2 && o.hasDmg && o.dmg == 18 && !o.hasHpPct));
    CHECK((o.hasDamageType && o.damageType == DamageType::Fire));
    CHECK((o.status.present && o.status.type == StatusType::Burn && o.status.chance == 0.5 && o.status.value == 3));
    CHECK((o.hitsMonsters && o.monsterDmgMul == 2 && o.capHeroMaxHpPct == 20));
    CHECK((d->corpse.leaves == CorpseLeaves::Corpse && d->corpse.consumes));
    const MonsterRiseAgainDef& r = d->riseAgain;
    CHECK((r.present && r.chance == 0.25 && r.delayMs == 3000 && r.hpPct == 40 && r.notOnCrit));
    REQUIRE(d->abilities.size() == 1);
    CHECK((d->abilities[0].summon.hasRadius && d->abilities[0].summon.radius == 5));
    // The other kinds and their blocks.
    DataLoadReport r2;
    const auto s2 = K3LoadDef(K3DefJson("fx_k3_kinds", R"("onDeath": {"kind": "puddle", "shape": {"kind": "ring", "radius": 2,
        "innerRadius": 1}, "hpPct": 5, "puddle": {"durationMs": 2000, "tickMs": 500}}, "corpse": {"leaves": "remains"})"),
                              r2);
    REQUIRE(r2.Ok());
    const MonsterDef* p = s2->FindMonster("fx_k3_kinds");
    REQUIRE(p != nullptr);
    CHECK((p->onDeath.kind == OnDeathKind::Puddle && p->onDeath.shape.kind == AbilityShapeKind::Ring));
    CHECK((p->onDeath.hasHpPct && p->onDeath.hpPct == 5 && p->onDeath.puddle.durationMs == 2000 && p->onDeath.puddle.tickMs == 500));
    CHECK((p->corpse.leaves == CorpseLeaves::Remains && !p->corpse.consumes && !p->riseAgain.present));
    DataLoadReport r3;
    const auto s3 = K3LoadDef(K3DefJson("fx_k3_split", R"("onDeath": {"kind": "spawn", "delayMs": 500,
        "spawn": {"monsterId": "fx_k3_clot", "count": 2}})"),
                              r3);
    REQUIRE(r3.Ok());
    const MonsterDef* sp = s3->FindMonster("fx_k3_split");
    REQUIRE(sp != nullptr);
    CHECK((sp->onDeath.kind == OnDeathKind::Spawn && sp->onDeath.spawn.monsterId == "fx_k3_clot" && sp->onDeath.spawn.count == 2));
    // A ring defaults to a ring shape, a puddle to a circle.
    DataLoadReport r4;
    const auto s4 = K3LoadDef(K3DefJson("fx_k3_ring", R"("onDeath": {"kind": "ring", "shape": {"radius": 3, "innerRadius": 1},
        "status": {"type": "slow", "ms": 1000, "value": 20}})"),
                              r4);
    REQUIRE(r4.Ok());
    CHECK(s4->FindMonster("fx_k3_ring")->onDeath.shape.kind == AbilityShapeKind::Ring);
    // Without the K3 fields nothing changes (the shipped definitions).
    const MonsterDef* plain = store->FindMonster("fx_k3_clot");
    REQUIRE(plain != nullptr);
    CHECK((!plain->onDeath.present && plain->corpse.leaves == CorpseLeaves::None && !plain->riseAgain.present));
  }

  TEST_CASE("K3 data: elite affix onDeath / riseAgain / families, the unburied type, and map corpse seeds load") {
    const EliteAffixTable& t = K3Data().Combat().eliteAffixes;
    CHECK(t.Def(EliteAffixType::FireEnhanced).onDeath.present);
    CHECK(t.Def(EliteAffixType::FireEnhanced).onDeath.kind == OnDeathKind::Burst);
    CHECK(t.Def(EliteAffixType::FireEnhanced).onDeath.hpPct == 10);
    CHECK(t.Def(EliteAffixType::Frozen).onDeath.kind == OnDeathKind::Ring);
    CHECK(t.Def(EliteAffixType::Frozen).onDeath.shape.kind == AbilityShapeKind::Circle);
    const EliteAffixDef& u = t.Def(EliteAffixType::Unburied);
    CHECK(u.type == EliteAffixType::Unburied);
    CHECK((u.riseAgain.present && u.riseAgain.chance == 1 && u.riseAgain.delayMs == 3000 && u.riseAgain.hpPct == 50));
    CHECK(u.families == std::vector<std::string>{"unburied"});
    CHECK(t.order.back() == EliteAffixType::Unburied);
    // The shipped table: no K3 field yet, and the unburied type undefined (never in the pool).
    const EliteAffixTable& real = test::RealData().Combat().eliteAffixes;
    for (EliteAffixType a : real.order) {
      CHECK_FALSE(real.Def(a).onDeath.present);
      CHECK_FALSE(real.Def(a).riseAgain.present);
      CHECK(real.Def(a).families.empty());
      CHECK(a != EliteAffixType::Unburied);
    }
    CHECK(real.Def(EliteAffixType::Unburied).type == EliteAffixType::Unburied);
    // Corpse seeds on a map.
    DataLoadReport report;
    const auto store = K3LoadEdited(
        {{"assets.json", K3AddAsset},
         {"monsters.json", K3AddDefs},
         {"maps.json",
          [](JsonValue& doc) {
            JsonValue* m = doc.FindMutable("maps")->FindMutable("emerald_plains");
            REQUIRE(m != nullptr);
            m->Set("corpseSeeds", K3Parse(R"([{"col": 60, "row": 54, "monsterId": "fx_k3_risen"},
                                              {"col": 62, "row": 55, "monsterId": "fx_k3_risen"}])"));
          }}},
        report);
    if (!report.Ok()) MESSAGE(report.Summary());
    REQUIRE(report.Ok());
    const MapDef* map = store->World().FindMap("emerald_plains");
    REQUIRE(map != nullptr);
    REQUIRE(map->corpseSeeds.size() == 2);
    CHECK(map->corpseSeeds[1].pos == TilePos{62, 55});
    CHECK(map->corpseSeeds[1].monsterId == "fx_k3_risen");
    CHECK(test::RealData().World().FindMap("emerald_plains")->corpseSeeds.empty());
  }

  TEST_CASE("K3 data: the loader refuses broken on-death / corpse / rise / seed / affix blocks") {
    struct Bad {
      std::string extra;
      const char* error;
    };
    const std::vector<Bad> bad = {
        {R"("onDeath": {"kind": "burst", "delayMs": 700, "shape": {"radius": 2}, "dmg": 5, "capHeroMaxHpPct": 20})",
         "fuse lasts >= 800 ms"},
        {R"("onDeath": {"kind": "burst", "delayMs": 800, "shape": {"radius": 2}, "dmg": 5})", "capHeroMaxHpPct"},
        {R"("onDeath": {"kind": "burst", "delayMs": 800, "shape": {"radius": 2}, "capHeroMaxHpPct": 20})",
         "a burst deals damage"},
        {R"("onDeath": {"kind": "burst", "delayMs": 800, "dmg": 5, "capHeroMaxHpPct": 20})", "needs a shape"},
        {R"("onDeath": {"kind": "burst", "delayMs": 800, "shape": {"kind": "ring", "radius": 2}, "dmg": 5, "capHeroMaxHpPct": 20})",
         "differs from the ability's kind"},
        {R"("onDeath": {"kind": "burst", "delayMs": 800, "shape": {"radius": 2}, "dmg": 5, "hpPct": 5, "capHeroMaxHpPct": 20})",
         "not both"},
        {R"("onDeath": {"kind": "ring", "shape": {"radius": 2}})", "needs damage or a status"},
        {R"("onDeath": {"kind": "ring", "shape": {"kind": "cone", "radius": 2, "arcDeg": 90}, "dmg": 3})",
         "a circle or a ring"},
        {R"("onDeath": {"kind": "puddle", "shape": {"radius": 2}, "dmg": 3})", "needs its 'puddle' block"},
        {R"("onDeath": {"kind": "puddle", "shape": {"radius": 2}, "dmg": 3, "puddle": {"durationMs": 1000, "tickMs": 50}})",
         "100 <= tickMs"},
        {R"("onDeath": {"kind": "ring", "shape": {"radius": 2}, "dmg": 3, "puddle": {"durationMs": 1000, "tickMs": 500}})",
         "only a puddle"},
        {R"("onDeath": {"kind": "spawn"})", "needs its 'spawn' block"},
        {R"("onDeath": {"kind": "spawn", "spawn": {"monsterId": "fx_k3_clot", "count": 21}})", "1 <= count <= 20"},
        {R"("onDeath": {"kind": "spawn", "dmg": 3, "spawn": {"monsterId": "fx_k3_clot"}})", "a spawn deals no damage"},
        {R"("onDeath": {"kind": "spawn", "shape": {"radius": 1}, "spawn": {"monsterId": "fx_k3_clot"}})", "a spawn has no shape"},
        {R"("onDeath": {"kind": "ring", "delayMs": 20000, "shape": {"radius": 2}, "dmg": 3})", "0 <= delayMs <= 10000"},
        {R"("onDeath": {"kind": "ring", "shape": {"radius": 2}, "dmg": 3, "fuse": 900})", "unknown onDeath field 'fuse'"},
        {R"("onDeath": {"kind": "blast", "shape": {"radius": 2}, "dmg": 3})", "unknown value 'blast'"},
        {R"("onDeath": {"kind": "ring", "shape": {"radius": 2}, "dmg": 3, "capHeroMaxHpPct": 120})", "capHeroMaxHpPct <= 100"},
        {R"("corpse": {"leaves": "husk"})", "unknown value 'husk'"},
        {R"("corpse": {"leaves": "corpse", "eats": true})", "unknown corpse field 'eats'"},
        {R"("riseAgain": {"chance": 0.5})", "corpse.leaves must be 'corpse'"},
        {R"("corpse": {"leaves": "remains"}, "riseAgain": {"chance": 0.5})", "corpse.leaves must be 'corpse'"},
        {R"("corpse": {"leaves": "corpse"}, "riseAgain": {"chance": 0})", "0 < chance <= 1"},
        {R"("corpse": {"leaves": "corpse"}, "riseAgain": {"hpPct": 0})", "0 < hpPct <= 100"},
        {R"("corpse": {"leaves": "corpse"}, "riseAgain": {"delayMs": 70000})", "0 <= delayMs <= 60000"},
        {R"("corpse": {"leaves": "corpse"}, "riseAgain": {"once": true})", "unknown riseAgain field 'once'"},
        {R"("family": "")", "an empty family id"},
        {R"("attackRange": 2, "abilities": [{"id": "a", "kind": "raise", "clip": "Cast01", "cooldownMs": 0, "aim": "corpse",
            "summon": {"monsterId": "fx_k3_risen", "radius": 0}}])",
         "0 < radius <= 30"},
        {R"("attackRange": 2, "abilities": [{"id": "a", "kind": "summon", "clip": "Cast01", "cooldownMs": 0, "aim": "self",
            "summon": {"monsterId": "fx_k3_risen", "at": "anchors", "anchors": [{"col": 1, "row": 1}], "radius": 2}}])",
         "no radius"},
        {R"("attackRange": 2, "abilities": [{"id": "a", "kind": "summon", "clip": "Cast01", "cooldownMs": 0, "aim": "self",
            "summon": {"monsterId": "fx_k3_risen", "perWindowCap": 1, "windowMs": 700000}}])",
         "0 <= windowMs <= 600000"},
    };
    for (const Bad& b : bad) {
      INFO(b.extra);
      DataLoadReport report;
      K3LoadDef(K3DefJson("fx_k3_bad", b.extra), report);
      CHECK_FALSE(report.Ok());
      bool found = false;
      for (const DataIssue& e : report.errors) found = found || Contains(e.message, b.error);
      if (!found) MESSAGE(report.Summary());
      CHECK(found);
    }
    // Affix rows: an unknown field, a type that differs from its key, an order entry without a definition, a broken K3
    // block.
    const std::vector<std::pair<std::function<void(JsonValue&)>, const char*>> affixBad = {
        {[](JsonValue& d) { d.FindMutable("definitions")->FindMutable("swift")->Set("onDeth", JsonValue::Object()); },
         "unknown elite affix field 'onDeth'"},
        {[](JsonValue& d) { d.FindMutable("definitions")->FindMutable("swift")->Set("type", JsonValue::String("frozen")); },
         "differs from its key"},
        {[](JsonValue& d) { d.FindMutable("order")->Append(JsonValue::String("unburied")); }, "has no definition"},
        {[](JsonValue& d) {
           d.FindMutable("definitions")->FindMutable("swift")->Set("riseAgain", K3Parse(R"({"chance": 2})"));
         },
         "0 < chance <= 1"},
        {[](JsonValue& d) {
           d.FindMutable("definitions")->FindMutable("swift")->Set("onDeath", K3Parse(R"({"kind": "burst", "delayMs": 900,
             "shape": {"radius": 2}, "dmg": 4})"));
         },
         "capHeroMaxHpPct"},
        {[](JsonValue& d) { d.FindMutable("definitions")->FindMutable("swift")->Set("families", K3Parse(R"([""])")); },
         "an empty family id"},
    };
    for (const auto& [edit, error] : affixBad) {
      INFO(error);
      DataLoadReport report;
      K3LoadEdited({{"elite_affixes.json", edit}}, report);
      CHECK_FALSE(report.Ok());
      bool found = false;
      for (const DataIssue& e : report.errors) found = found || Contains(e.message, error);
      if (!found) MESSAGE(report.Summary());
      CHECK(found);
    }
    // A corpse seed with an unknown field.
    DataLoadReport seedReport;
    K3LoadEdited({{"monsters.json", K3AddDefs},
                  {"maps.json",
                   [](JsonValue& doc) {
                     doc.FindMutable("maps")->FindMutable("emerald_plains")->Set(
                         "corpseSeeds", K3Parse(R"([{"col": 60, "row": 54, "monsterId": "fx_k3_risen", "facing": 90}])"));
                   }}},
                 seedReport);
    CHECK(K3HasIssue(seedReport.errors, "corpseSeeds", "unknown corpse seed field 'facing'"));
  }

  TEST_CASE("K3 data: Finalize checks on-death spawn ids and corpse seeds (monster, body, grid)") {
    DataLoadReport report;
    K3LoadDef(K3DefJson("fx_k3_split", R"("onDeath": {"kind": "spawn", "spawn": {"monsterId": "fx_no_such_monster"}})"),
              report);
    CHECK(K3HasIssue(report.errors, "fx_k3_split.onDeath.spawn.monsterId", "unknown monster 'fx_no_such_monster'"));
    DataLoadReport affix;
    K3LoadEdited({{"elite_affixes.json",
                   [](JsonValue& d) {
                     d.FindMutable("definitions")->FindMutable("swift")->Set(
                         "onDeath", K3Parse(R"({"kind": "spawn", "spawn": {"monsterId": "fx_no_such_monster"}})"));
                   }}},
                 affix);
    CHECK(K3HasIssue(affix.errors, "definitions.swift.onDeath.spawn.monsterId", "unknown monster"));
    const auto seeds = [](std::string_view json) {
      DataLoadReport r;
      const std::string text(json);
      K3LoadEdited({{"monsters.json", K3AddDefs},
                    {"maps.json",
                     [text](JsonValue& doc) {
                       doc.FindMutable("maps")->FindMutable("emerald_plains")->Set("corpseSeeds", K3Parse(text));
                     }}},
                   r);
      return r;
    };
    CHECK(K3HasIssue(seeds(R"([{"col": 60, "row": 54, "monsterId": "fx_no_such_monster"}])").errors, "corpseSeeds[0]",
                     "unknown monster 'fx_no_such_monster'"));
    CHECK(K3HasIssue(seeds(R"([{"col": 60, "row": 54, "monsterId": "fx_k3_clot"}])").errors, "corpseSeeds[0]",
                     "leaves no corpse"));
    CHECK(K3HasIssue(seeds(R"([{"col": 6000, "row": 54, "monsterId": "fx_k3_risen"}])").errors, "corpseSeeds[0]",
                     "outside the"));
    CHECK(seeds(R"([{"col": 60, "row": 54, "monsterId": "fx_k3_risen"}])").Ok());
  }

  // =================================================================================================================
  // Pure rules (monsters/OnDeath.h) and the Corpses container (monsters/Corpses.h)
  // =================================================================================================================
  TEST_CASE("K3 rules: on-death damage, the caps, monster damage, the volatile curse and the rise conditions") {
    MonsterOnDeathDef d;
    d.present = true;
    d.kind = OnDeathKind::Burst;
    d.hasDmg = true;
    d.dmg = 18;
    CHECK(OnDeathBaseDamage(d, 999, 1) == 18);
    CHECK(OnDeathBaseDamage(d, 999, 1.5) == 27);  // x the difficulty's monster damage
    d.hasDmg = false;
    d.hasHpPct = true;
    d.hpPct = 12;
    CHECK(OnDeathBaseDamage(d, 125, 2) == 15);  // 12 % of the dying monster's life (rounded half up), not scaled
    CHECK(OnDeathBaseDamage(d, 4, 1) == 1);     // at least 1 (0.48 rounds to 0)
    CHECK(OnDeathBaseDamage(d, 50, 1) == 6);
    d.hasHpPct = false;
    CHECK(OnDeathBaseDamage(d, 125, 1) == 0);  // a status-only ring / puddle
    d.kind = OnDeathKind::Spawn;
    d.hasDmg = true;
    CHECK(OnDeathBaseDamage(d, 125, 1) == 0);  // a spawn deals none
    d.present = false;
    d.kind = OnDeathKind::Burst;
    CHECK(OnDeathBaseDamage(d, 125, 1) == 0);
    // Caps: the smaller positive one binds; difficulty-scaled like a hit cap.
    CHECK(OnDeathHeroCap(500, 20, 35, 1) == 100);
    CHECK(OnDeathHeroCap(500, 0, 35, 1) == 175);
    CHECK(OnDeathHeroCap(500, 20, 0, 1.5) == 150);
    CHECK(OnDeathHeroCap(500, 40, 35, 1) == 175);
    CHECK(OnDeathHeroCap(500, 0, 0, 1) == 0);
    CHECK(OnDeathMonsterDamage(18, 2) == 36);
    CHECK(OnDeathMonsterDamage(3, 0.5) == 1);
    CHECK(OnDeathMonsterDamage(1, 0.1) == 1);
    CHECK(OnDeathMonsterDamage(5, 0) == 0);
    CHECK(OnDeathMonsterDamage(0, 2) == 0);
    AbilityShapeDef ring;
    ring.kind = AbilityShapeKind::Ring;
    CHECK(OnDeathTelegraphKind(ring) == AbilityKind::Ring);
    ring.kind = AbilityShapeKind::Circle;
    CHECK(OnDeathTelegraphKind(ring) == AbilityKind::Circle);
    // The volatile curse (abyss_run.json): a 12 % fire burst of radius 2 on the 800 ms fuse, capped at 20 %.
    const CurseDef* vol = nullptr;
    for (const CurseDef& c : test::RealData().AbyssRun().curses) {
      if (c.deathBurst > 0) vol = &c;
    }
    REQUIRE(vol != nullptr);
    const MonsterOnDeathDef v = VolatileCurseOnDeath(*vol);
    CHECK((v.present && v.kind == OnDeathKind::Burst && v.delayMs == kOnDeathBurstMinFuseMs));
    CHECK((v.shape.kind == AbilityShapeKind::Circle && v.shape.radius == 2));
    CHECK((v.hasHpPct && v.hpPct == 12 && !v.hasDmg));
    CHECK((v.hasDamageType && v.damageType == DamageType::Fire));
    CHECK((v.capHeroMaxHpPct == 20 && !v.hitsMonsters));
    CHECK(OnDeathBaseDamage(v, 250, 1) == 30);  // round(250 x 0.12)
    CurseDef calm;
    CHECK_FALSE(VolatileCurseOnDeath(calm).present);
    // Rise conditions.
    MonsterRiseAgainDef r;
    CHECK_FALSE(RiseAgainApplies(r, DamageType::Physical, false));
    r.present = true;
    CHECK(RiseAgainApplies(r, DamageType::Physical, false));
    CHECK(RiseAgainApplies(r, DamageType::Ice, true));
    CHECK_FALSE(RiseAgainApplies(r, DamageType::Fire, false));  // a fire killing blow burns the body
    r.notOnCrit = true;
    CHECK_FALSE(RiseAgainApplies(r, DamageType::Physical, true));
    CHECK(RiseAgainApplies(r, DamageType::Physical, false));
  }

  TEST_CASE("K3 picker: raise / summon run when the runtime found bodies for them (available), never without") {
    MonsterAbilityDef summon = K3Ability("muster", AbilityKind::Summon, AbilityAim::Self);
    summon.summon.present = true;
    summon.summon.monsterId = "fx_k3_risen";
    const std::vector<MonsterAbilityDef> a = {K3Raise("flock", 1), summon};
    std::vector<double> ready(2, 0.0);
    AbilityPickInput in;
    in.readyAtMs = ready;
    CHECK(PickMonsterAbility(a, in) == -1);  // no plan: nothing to stand up
    std::vector<uint8_t> avail = {0, 1};
    in.available = avail;
    CHECK(PickMonsterAbility(a, in) == 1);
    avail = {1, 1};
    CHECK(PickMonsterAbility(a, in) == 0);  // same priority: data order
    ready[0] = 5000;
    in.nowMs = 1000;
    CHECK(PickMonsterAbility(a, in) == 1);  // the cooldown still gates it
    for (AbilityKind k : {AbilityKind::Raise, AbilityKind::Summon}) {
      CHECK(AbilityKindRuns(k));
      CHECK(AbilityKindSummons(k));
    }
    for (AbilityKind k : {AbilityKind::Buff, AbilityKind::Call}) CHECK_FALSE(AbilityKindRuns(k));  // K5
    CHECK_FALSE(AbilityKindSummons(AbilityKind::Circle));
  }

  TEST_CASE("K3 Corpses: the cap evicts the oldest kill bodies; seeds stay; TTL; query order; holds") {
    Corpses c;
    std::vector<uint32_t> ev;
    const auto body = [](Vec2 pos, double deathMs, bool seeded = false) {
      CorpseEntry e;
      e.defId = "x";
      e.pos = pos;
      e.deathMs = deathMs;
      e.expiresMs = seeded ? 0 : deathMs + 20000;
      e.seeded = seeded;
      return e;
    };
    const uint32_t seed = c.Add(body(Vec2(0, 0.5), 0, true), 3, ev);
    const uint32_t a = c.Add(body(Vec2(0, 0), 100), 3, ev);
    const uint32_t b = c.Add(body(Vec2(1, 0), 200), 3, ev);
    const uint32_t d = c.Add(body(Vec2(1, 0), 150), 3, ev);  // same distance as b, died earlier
    CHECK(ev.empty());
    CHECK(c.KillBodies() == 3);
    CHECK(seed < a);
    CHECK(a < b);
    CHECK(b < d);  // ids count up in add order
    std::vector<uint32_t> q;
    c.Query(Vec2(0, 0), 5, CorpseFilter::Any, q);
    CHECK(q == std::vector<uint32_t>{a, seed, d, b});  // distance, then death time, then id
    c.Query(Vec2(0, 0), 0.5, CorpseFilter::Any, q);
    CHECK(q == std::vector<uint32_t>{a, seed});  // distance <= radius (the seed at exactly 0.5)
    // The cap (3 kill bodies): a 4th pushes the oldest kill body out (death time, then id); the seed never counts.
    const uint32_t e = c.Add(body(Vec2(2, 0), 300), 3, ev);
    CHECK(ev == std::vector<uint32_t>{a});
    CHECK(c.Find(a) == nullptr);
    CHECK(c.Find(seed) != nullptr);
    const uint32_t f = c.Add(body(Vec2(2, 0), 140), 3, ev);  // a late registration of an earlier death goes first
    CHECK(ev == std::vector<uint32_t>{a, f});
    CHECK(c.KillBodies() == 3);
    // TTL: kill bodies only, add order.
    c.CollectExpired(20149, q);
    CHECK(q.empty());
    c.CollectExpired(20200, q);
    CHECK(q == std::vector<uint32_t>{b, d});
    c.CollectExpired(1e12, q);
    CHECK(q == std::vector<uint32_t>{b, d, e});  // never the seed
    // Holds: a held body is neither raisable nor consumable; spent bodies are eaten but never raised; remains neither.
    CHECK(c.Hold(b, 7));
    CHECK_FALSE(c.Hold(b, 8));
    CHECK_FALSE(c.Find(b)->Raisable());
    CHECK_FALSE(c.Find(b)->Consumable());
    c.Release(b, 8);  // not its holder
    CHECK(c.Find(b)->holder == 7);
    c.Release(b, 7);
    CHECK(c.Find(b)->Raisable());
    CHECK(c.Hold(b, 7));
    CHECK(c.Hold(d, 7));
    c.ReleaseAll(7);
    CHECK((c.Find(b)->holder == kNoEntity && c.Find(d)->holder == kNoEntity));
    CHECK_FALSE(c.Hold(99999, 7));
    CorpseEntry spent = body(Vec2(5, 5), 400);
    spent.spent = true;
    const uint32_t s = c.Add(spent, 0, ev);  // cap 0: no cap
    CorpseEntry remains = body(Vec2(5, 5), 400);
    remains.kind = CorpseKind::Remains;
    const uint32_t r = c.Add(remains, 0, ev);
    CHECK((!c.Find(s)->Raisable() && c.Find(s)->Consumable()));
    CHECK((!c.Find(r)->Raisable() && !c.Find(r)->Consumable()));
    c.Query(Vec2(5, 5), 1, CorpseFilter::Raisable, q);
    CHECK(q.empty());
    c.Query(Vec2(5, 5), 1, CorpseFilter::Consumable, q);
    CHECK(q == std::vector<uint32_t>{s});
    CHECK(c.Remove(s));
    CHECK_FALSE(c.Remove(s));
    c.Clear();
    CHECK(c.Entries().empty());
    // A Sheepfold-like zone: six seeds plus more kill bodies than the cap (24). The cap pushes out the six oldest kill
    // bodies, never a seed, and the TTL never takes a seed.
    Corpses fold;
    std::vector<uint32_t> out;
    std::vector<uint32_t> seeds;
    for (int32_t i = 0; i < 6; ++i) seeds.push_back(fold.Add(body(Vec2(10 + i, 10), 0, true), 24, out));
    std::vector<uint32_t> kills;
    for (int32_t i = 0; i < 30; ++i) kills.push_back(fold.Add(body(Vec2(20, 20), 1000 + i), 24, out));
    CHECK(out == std::vector<uint32_t>(kills.begin(), kills.begin() + 6));
    CHECK(fold.KillBodies() == 24);
    CHECK(fold.Entries().size() == 30);
    for (uint32_t id : seeds) CHECK(fold.Find(id) != nullptr);
    fold.CollectExpired(1e12, q);
    CHECK(q == std::vector<uint32_t>(kills.begin() + 6, kills.end()));
    fold.Query(Vec2(12, 10), 0.5, CorpseFilter::Raisable, q);
    CHECK(q == std::vector<uint32_t>{seeds[2]});
  }

  // =================================================================================================================
  // Runtime: bodies, raises, rewards, collapse, burning, consume, the Unburied rise, on-death effects
  // =================================================================================================================
  TEST_CASE("K3 runtime: a kill registers its body (corpse / remains / none); fire burns it at once; TTL and the cap") {
    K3Rig rig;
    REQUIRE(rig.ok);
    const MonsterDef foot = K3Footman();
    const MonsterDef gnawer = K3Gnawer();
    MonsterDef clot = K3Footman("fx_k3_nobody");
    clot.corpse.leaves = CorpseLeaves::None;
    const EntityId f = rig.Spawn(foot, Vec2(50, 50), MonsterState::Idle);
    const double deathAt = rig.Now();
    rig.Kill(f);
    std::vector<EvCorpseAdded> added = K3Events<EvCorpseAdded>(rig.h.events);
    REQUIRE(added.size() == 1);
    CHECK(added[0].monster == f);
    CHECK(added[0].defId == "fx_k3_footman");
    CHECK(added[0].artId == kArt);
    CHECK(added[0].family == "unburied");
    CHECK(added[0].kind == CorpseKind::Corpse);
    CHECK(added[0].pos == Vec2(50, 50));
    CHECK(added[0].deathMs == deathAt);
    CHECK(added[0].expiresMs == deathAt + 20000);
    CHECK_FALSE(added[0].seeded);
    CHECK_FALSE(added[0].spent);
    REQUIRE(rig.monsters.CorpseRegistry().Entries().size() == 1);
    const uint32_t first = added[0].corpseId;
    rig.h.events.Clear();
    // (A role that never respawns: a gnawer coming back 15 s later would eat the footman's body, rightly.)
    const EntityId g = rig.Spawn(gnawer, Vec2(52, 50), MonsterState::Idle, MonsterRole::DefendWave);
    rig.Kill(g);
    added = K3Events<EvCorpseAdded>(rig.h.events);
    REQUIRE(added.size() == 1);
    CHECK(added[0].kind == CorpseKind::Remains);
    rig.h.events.Clear();
    rig.Kill(rig.Spawn(clot, Vec2(54, 50), MonsterState::Idle));
    CHECK(K3Events<EvCorpseAdded>(rig.h.events).empty());  // leaves none (Breach-blood's puddle)
    // A fire killing blow: the body is added burned (added, then removed at once).
    rig.h.events.Clear();
    rig.Kill(rig.Spawn(foot, Vec2(56, 50), MonsterState::Idle), DamageType::Fire);
    added = K3Events<EvCorpseAdded>(rig.h.events);
    REQUIRE(added.size() == 1);
    const std::vector<EvCorpseRemoved> burned = Removals(rig.h.events, CorpseRemoveReason::Burned);
    REQUIRE(burned.size() == 1);
    CHECK(burned[0].corpseId == added[0].corpseId);
    CHECK(burned[0].by == kNoEntity);
    CHECK(rig.monsters.CorpseRegistry().Entries().size() == 2);
    // TTL: the first body fades 20 s after its death.
    rig.h.events.Clear();
    rig.StepUntilMs(deathAt + 20000 - kSimStepMs);
    CHECK(Removals(rig.h.events, CorpseRemoveReason::Expired).empty());
    rig.StepUntilMs(deathAt + 20000 + kSimStepMs);
    const std::vector<EvCorpseRemoved> expired = Removals(rig.h.events, CorpseRemoveReason::Expired);
    REQUIRE(expired.size() == 2);
    CHECK(expired[0].corpseId == first);
    CHECK(rig.monsters.CorpseRegistry().Entries().empty());
    // The cap: 24 kill bodies per zone; the 25th pushes the oldest out (reason expired).
    rig.h.events.Clear();
    std::vector<uint32_t> ids;
    for (int i = 0; i < 25; ++i) {
      rig.Kill(rig.Spawn(foot, Vec2(40 + i % 10, 60 + i / 10), MonsterState::Idle));
      ids.push_back(rig.Newest()->id);
      if (i % 5 == 4) rig.Step();
    }
    CHECK(rig.monsters.CorpseRegistry().Entries().size() == 24);
    const std::vector<EvCorpseRemoved> pushed = Removals(rig.h.events, CorpseRemoveReason::Expired);
    REQUIRE(pushed.size() == 1);
    CHECK(pushed[0].corpseId == ids[0]);
    CHECK(rig.Corpse(ids[0]) == nullptr);
    CHECK(rig.Corpse(ids[1]) != nullptr);
  }

  TEST_CASE("K3 runtime: a raise holds the nearest corpses, marks them, and stands them up as Raised at contact") {
    K3Rig rig;
    const MonsterDef raiser = K3Caster({K3Raise("flock", 2, 3)});
    rig.PlaceHero(Vec2(41.5, 40));
    const uint32_t c1 = rig.CorpseAt(K3Footman(), Vec2(41, 41));  // 1.41 from the caster
    const uint32_t c2 = rig.CorpseAt(K3Footman(), Vec2(42, 42));  // 2.83
    const uint32_t c3 = rig.CorpseAt(K3Footman(), Vec2(43, 43));  // 4.24
    const uint32_t far = rig.CorpseAt(K3Footman(), Vec2(47, 40)); // 7: beyond the default radius 6
    MonsterDef gn = K3Gnawer();
    const uint32_t remains = rig.CorpseAt(gn, Vec2(40, 41));      // 1: remains are never raised
    const EntityId c = rig.Spawn(raiser, Vec2(40, 40));
    rig.h.events.Clear();
    rig.ScriptNoDodge();
    rig.Step();
    REQUIRE(rig.M(c).pendingAbility == 0);
    const double start = rig.Now();
    const std::vector<EvPlayAnim> anims = K3Events<EvPlayAnim>(rig.h.events);
    REQUIRE(!anims.empty());
    CHECK(anims[0].action == AnimAction::Cast);
    CHECK(anims[0].clip == "Cast01");
    CHECK(anims[0].contactMs == 900);  // the clip's Release beat
    const std::vector<EvTelegraph> marks = K3Events<EvTelegraph>(rig.h.events);
    REQUIRE(marks.size() == 2);
    for (const EvTelegraph& t : marks) {
      CHECK(t.kind == AbilityKind::Raise);
      CHECK(t.source == TelegraphSource::Ability);
      CHECK(t.abilityId == "flock");
      CHECK(t.shape.radius == rig.h.ctx.data.Monsters().ai.raiseMarkerRadius);
      CHECK(t.contactMs == start + 900);
    }
    CHECK(marks[0].origin == Vec2(41, 41));
    CHECK(marks[1].origin == Vec2(42, 42));
    CHECK(rig.Corpse(c1)->holder == c);
    CHECK(rig.Corpse(c2)->holder == c);
    CHECK(rig.Corpse(c3)->holder == kNoEntity);
    CHECK(rig.Corpse(remains)->holder == kNoEntity);
    // The snapshot: one marker view per corpse.
    Snapshot snap;
    rig.combat.FillSnapshot(snap);
    REQUIRE(snap.telegraphs.size() == 2);
    CHECK((snap.telegraphs[0].kind == AbilityKind::Raise && snap.telegraphs[0].origin == Vec2(41, 41)));
    // Contact: both stand up as Raised fx_k3_risen at 40 % of their life, rooted for their Rise clip.
    rig.h.events.Clear();
    rig.StepUntilMs(start + 900);
    const std::vector<EvCorpseRemoved> raised = Removals(rig.h.events, CorpseRemoveReason::Raised);
    REQUIRE(raised.size() == 2);
    CHECK((raised[0].corpseId == c1 && raised[0].by == c));
    CHECK((raised[1].corpseId == c2 && raised[1].by == c));
    const std::vector<EvMonsterRaised> risen = K3Events<EvMonsterRaised>(rig.h.events);
    REQUIRE(risen.size() == 2);
    for (const EvMonsterRaised& r : risen) {
      CHECK(r.raiser == c);
      CHECK(r.defId == "fx_k3_risen");
      CHECK(r.abilityId == "flock");
      CHECK(r.riseMs == 1200);  // the Rise clip of fx_k3_art
      CHECK_FALSE(r.selfRise);
      const MonsterInstance& m = rig.M(r.entity);
      CHECK(m.role == MonsterRole::Raised);
      CHECK(m.raiserId == c);
      CHECK(m.noRespawn);
      CHECK(m.hp == std::floor(m.maxHp * 0.4));
      CHECK(m.def.expReward == 0);
      CHECK(m.abilityRootedUntilMs == r.startMs + 1200);
      CHECK(m.state == MonsterState::Chase);
    }
    CHECK(risen[0].pos == Vec2(41, 41));
    CHECK(risen[0].corpseId == c1);
    size_t riseAnims = 0;
    for (const EvPlayAnim& a : K3Events<EvPlayAnim>(rig.h.events)) riseAnims += a.action == AnimAction::Rise ? 1 : 0;
    CHECK(riseAnims == 2);
    CHECK(rig.Corpse(c1) == nullptr);
    CHECK(rig.Corpse(c3) != nullptr);
    CHECK(rig.Corpse(far) != nullptr);
    CHECK(rig.RaisedOf(c).size() == 2);
    // Rooted while the clip plays (no AI step: it stays in Chase next to the hero), then it fights.
    const EntityId first = risen[0].entity;
    const Vec2 at = rig.M(first).pos;
    rig.StepUntilMs(start + 900 + 1100);
    CHECK(rig.M(first).pos == at);
    CHECK(rig.M(first).state == MonsterState::Chase);
    rig.StepUntilMs(start + 900 + 1300);
    CHECK(rig.M(first).state == MonsterState::Attack);
    CHECK(rig.combat.PendingOnDeathEffects().empty());
  }

  TEST_CASE("K3 runtime: raise caps: live Raised per caster, bodies per window; the opening raise of two") {
    // Live cap 3, two at a time, no cooldown: 2, then 1 (room), then the basic swing.
    {
      K3Rig rig;
      const MonsterDef raiser = K3Caster({K3Raise("flock", 2, 3, 0)});
      rig.PlaceHero(Vec2(41.5, 40));
      for (int i = 0; i < 5; ++i) rig.CorpseAt(K3Footman(), Vec2(39 + i * 0.5, 41.5));
      const EntityId c = rig.Spawn(raiser, Vec2(40, 40));
      rig.h.events.Clear();
      std::vector<size_t> perCast;
      int basics = 0;
      for (int s = 0; s < 6 * 60 && perCast.size() + static_cast<size_t>(basics) < 4; ++s) {
        rig.ScriptNoDodge();
        rig.Step();
        // In log order: since the 2B review a cast may start in the step its predecessor stood its bodies up (R1).
        for (const Event& e : rig.h.events.Items()) {
          if (const EvPlayAnim* a = std::get_if<EvPlayAnim>(&e); a != nullptr && a->entity == c) {
            if (a->clip == "Cast01") perCast.push_back(0);
            if (a->clip == "Attack01") ++basics;
          } else if (const EvMonsterRaised* r = std::get_if<EvMonsterRaised>(&e); r != nullptr && r->raiser == c) {
            if (!perCast.empty()) ++perCast.back();
          }
        }
        // The risen keep away (stunned) so the hero's fight stays the caster's.
        for (EntityId id : rig.RaisedOf(c)) rig.Silence(id);
        rig.h.events.Clear();
      }
      REQUIRE(perCast.size() >= 2);
      CHECK(perCast[0] == 2);
      CHECK(perCast[1] == 1);
      CHECK(rig.RaisedOf(c).size() == 3);
      CHECK(basics >= 1);  // the cap holds: the next due swing is a basic one
    }
    // One body per 8 s window (count 1, no cooldown): a raise, a basic swing 1.5 s later, a raise again after 8 s.
    {
      K3Rig rig;
      MonsterAbilityDef a = K3Raise("flock", 1, 0, 0);
      a.summon.perWindowCap = 1;
      a.summon.windowMs = 8000;
      const MonsterDef raiser = K3Caster({a});
      rig.PlaceHero(Vec2(41.5, 40));
      for (int i = 0; i < 4; ++i) rig.CorpseAt(K3Footman(), Vec2(39 + i, 41.5));
      const EntityId c = rig.Spawn(raiser, Vec2(40, 40));
      rig.h.events.Clear();
      std::vector<double> raises;
      bool basicBetween = false;
      for (int s = 0; s < 11 * 60; ++s) {
        rig.ScriptNoDodge();
        rig.Step();
        for (const EvPlayAnim& e : K3Events<EvPlayAnim>(rig.h.events)) {
          if (e.entity != c) continue;
          if (e.clip == "Cast01") raises.push_back(e.startMs);
          if (e.clip == "Attack01" && raises.size() == 1) basicBetween = true;
        }
        for (EntityId id : rig.RaisedOf(c)) rig.Silence(id);
        rig.h.events.Clear();
      }
      REQUIRE(raises.size() >= 2);
      CHECK(basicBetween);
      CHECK(raises[1] - raises[0] >= 8000);
      CHECK(raises[1] - raises[0] < 8000 + 1600);  // at the first due swing once the window allows it
    }
    // The opening raise (two at once, once per fight) before the regular one (higher priority).
    {
      K3Rig rig;
      MonsterAbilityDef open = K3Raise("pull", 2, 3, 600000);
      open.priority = 5;
      const MonsterDef raiser = K3Caster({K3Raise("flock", 1, 3, 8000), open});
      rig.PlaceHero(Vec2(41.5, 40));
      for (int i = 0; i < 3; ++i) rig.CorpseAt(K3Footman(), Vec2(39 + i, 41.5));
      const EntityId c = rig.Spawn(raiser, Vec2(40, 40));
      rig.h.events.Clear();
      rig.ScriptNoDodge();
      rig.Step();
      CHECK(rig.M(c).pendingAbility == 1);
      CHECK(rig.M(c).abilityCorpses.size() == 2);
    }
  }

  TEST_CASE("K3 runtime: a Raised body pays no exp, gold or kill text and never respawns; its corpse is spent") {
    K3Rig rig;
    const MonsterDef* risen = rig.h.ctx.data.FindMonster("fx_k3_risen");
    REQUIRE(risen != nullptr);
    const EntityId body = rig.monsters.SummonAt(*risen, Vec2(50, 50), kNoEntity, 100, "test");
    REQUIRE(body != kNoEntity);
    CHECK(rig.M(body).role == MonsterRole::Raised);
    const int64_t exp = rig.hero->Exp();
    const int64_t gold = rig.hero->Gold();
    rig.h.events.Clear();
    rig.Kill(body);
    CHECK(rig.kills == 1);  // still a kill (quests, achievements)
    CHECK(rig.hero->Exp() == exp);
    CHECK(rig.hero->Gold() == gold);
    CHECK(rig.combat.LastKillReward().exp == 0);
    CHECK(rig.combat.LastKillReward().gold == 0);
    for (const EvFloatingText& t : K3Events<EvFloatingText>(rig.h.events)) {
      CHECK(t.kind != FloatingTextKind::Exp);
      CHECK(t.kind != FloatingTextKind::Gold);
    }
    CHECK(K3Events<EvGoldChanged>(rig.h.events).empty());
    CHECK(K3Events<EvExpGained>(rig.h.events).empty());
    for (const Timer& t : rig.h.timers.Pending()) CHECK_FALSE((t.owner == TimerOwner::Monsters && t.entity == body));
    const std::vector<EvCorpseAdded> added = K3Events<EvCorpseAdded>(rig.h.events);
    REQUIRE(added.size() == 1);
    CHECK(added[0].spent);
    CHECK_FALSE(rig.Corpse(added[0].corpseId)->Raisable());
    CHECK(rig.Corpse(added[0].corpseId)->Consumable());
    // A regular kill of the same definition pays and schedules its respawn (the control).
    const EntityId regular = rig.Spawn(*risen, Vec2(52, 50), MonsterState::Idle);
    rig.Kill(regular);
    CHECK(rig.combat.LastKillReward().exp > 0);
    bool respawn = false;
    for (const Timer& t : rig.h.timers.Pending()) respawn = respawn || (t.owner == TimerOwner::Monsters && t.entity == regular);
    CHECK(respawn);
    // 15 s later nothing came back for the Raised body.
    rig.StepUntilMs(rig.Now() + 15100);
    CHECK_FALSE(rig.M(body).IsAlive());
  }

  TEST_CASE("K3 runtime: every Raised collapses when its raiser dies (no kill, a spent corpse, chained)") {
    K3Rig rig;
    const MonsterDef* risen = rig.h.ctx.data.FindMonster("fx_k3_risen");
    REQUIRE(risen != nullptr);
    MonsterDef raiserDef = K3Footman("fx_k3_raiser");
    const EntityId raiser = rig.Spawn(raiserDef, Vec2(50, 50), MonsterState::Idle);
    const EntityId a = rig.monsters.SummonAt(*risen, Vec2(51, 50), raiser, 40, "test");
    const EntityId b = rig.monsters.SummonAt(*risen, Vec2(52, 50), raiser, 40, "test");
    const EntityId chained = rig.monsters.SummonAt(*risen, Vec2(53, 50), a, 40, "test");  // a's own Raised
    const EntityId stranger = rig.monsters.SummonAt(*risen, Vec2(54, 50), kNoEntity, 40, "test");
    CHECK(rig.monsters.RaisedAliveOf(raiser) == 2);
    CHECK(rig.monsters.RaisedAliveOf(a) == 1);
    rig.h.events.Clear();
    rig.Kill(raiser);
    CHECK(rig.kills == 1);  // the raiser only: collapses are not kills
    const std::vector<EvMonsterCollapsed> fell = K3Events<EvMonsterCollapsed>(rig.h.events);
    REQUIRE(fell.size() == 3);
    CHECK((fell[0].entity == a && fell[0].raiser == raiser));
    CHECK((fell[1].entity == chained && fell[1].raiser == a));  // a's Raised fall right after a
    CHECK((fell[2].entity == b && fell[2].raiser == raiser));
    for (EntityId id : {a, b, chained}) {
      CHECK_FALSE(rig.M(id).IsAlive());
      CHECK(rig.M(id).hp == 0);
      CHECK(rig.M(id).state == MonsterState::Dead);
    }
    CHECK(rig.M(stranger).IsAlive());
    // Bodies: the raiser's (raisable), each Raised's (spent).
    const std::vector<EvCorpseAdded> added = K3Events<EvCorpseAdded>(rig.h.events);
    REQUIRE(added.size() == 4);
    CHECK((added[0].monster == raiser && !added[0].spent));
    CHECK((added[1].monster == a && added[1].spent));
    // A death anim for each fall, no on-death effect and no rise for them.
    size_t deaths = 0;
    for (const EvPlayAnim& e : K3Events<EvPlayAnim>(rig.h.events)) deaths += e.action == AnimAction::Death ? 1 : 0;
    CHECK(deaths == 4);
    CHECK(rig.combat.PendingOnDeathEffects().empty());
    CHECK(rig.monsters.RaisedAliveOf(raiser) == 0);
  }

  TEST_CASE("K3 runtime: fire burns bodies: fire strikes, fire hazard ticks; a burned corpse is not raised") {
    // A fire circle strike burns the bodies under it at contact; a physical one does not.
    for (DamageType type : {DamageType::Fire, DamageType::Physical}) {
      INFO(EnumName(type));
      K3Rig rig;
      MonsterAbilityDef slam = K3Ability("slam", AbilityKind::Circle, AbilityAim::Self, "Cast01");
      slam.hasShape = true;
      slam.shape.kind = AbilityShapeKind::Circle;
      slam.shape.radius = 2;
      slam.cooldownMs = 100000;
      slam.hasDamageType = true;
      slam.damageType = type;
      rig.PlaceHero(Vec2(41.5, 40));
      const uint32_t under = rig.CorpseAt(K3Footman(), Vec2(41, 41));
      const uint32_t outside = rig.CorpseAt(K3Footman(), Vec2(44, 40));
      const EntityId c = rig.Spawn(K3Caster({slam}), Vec2(40, 40));
      rig.ScriptNoDodge();
      rig.Step();
      REQUIRE(rig.M(c).pendingAbility == 0);
      rig.h.events.Clear();
      rig.StepUntil([&] { return rig.M(c).pendingAbility < 0; }, 120);
      CHECK((rig.Corpse(under) == nullptr) == (type == DamageType::Fire));
      CHECK(rig.Corpse(outside) != nullptr);
    }
    // A fire hazard burns what lies in it on each tick (a body that dies in it later burns on the next tick).
    {
      K3Rig rig;
      MonsterAbilityDef seep = K3Ability("seep", AbilityKind::Hazard, AbilityAim::Self, "Cast01");
      seep.hasShape = true;
      seep.shape.kind = AbilityShapeKind::Circle;
      seep.shape.radius = 2;
      seep.cooldownMs = 100000;
      seep.hasDamageType = true;
      seep.damageType = DamageType::Fire;
      seep.hazard.present = true;
      seep.hazard.durationMs = 3000;
      seep.hazard.tickMs = 500;
      seep.hazard.tickDmgMul = 0.1;
      rig.PlaceHero(Vec2(41.5, 40));
      const EntityId c = rig.Spawn(K3Caster({seep}), Vec2(40, 40));
      rig.ScriptNoDodge();
      rig.Step();
      REQUIRE(rig.StepUntil([&] { return !rig.combat.Hazards().Entries().empty(); }, 120));
      rig.Silence(c);
      const uint32_t late = rig.CorpseAt(K3Footman(), Vec2(39, 40));  // dies (physically) inside the burning ground
      REQUIRE(rig.Corpse(late) != nullptr);
      rig.h.events.Clear();
      REQUIRE(rig.StepUntil([&] { return rig.Corpse(late) == nullptr; }, 40));
      CHECK(Removals(rig.h.events, CorpseRemoveReason::Burned).size() == 1);
    }
    // Burn blocks raise: a corpse burned during the windup that held it does not stand up.
    {
      K3Rig rig;
      const MonsterDef raiser = K3Caster({K3Raise("flock", 2, 0)});
      rig.PlaceHero(Vec2(41.5, 40));
      const uint32_t c1 = rig.CorpseAt(K3Footman(), Vec2(41, 41));
      const uint32_t c2 = rig.CorpseAt(K3Footman(), Vec2(39, 41));
      const EntityId c = rig.Spawn(raiser, Vec2(40, 40));
      rig.ScriptNoDodge();
      rig.Step();
      REQUIRE(rig.M(c).abilityCorpses.size() == 2);
      AbilityShapeDef fire;
      fire.kind = AbilityShapeKind::Circle;
      fire.radius = 0.5;
      CHECK(rig.monsters.BurnCorpses(fire, Vec2(41, 41), Vec2(1, 0)) == 1);
      rig.h.events.Clear();
      rig.StepUntil([&] { return rig.M(c).pendingAbility < 0; }, 120);
      const std::vector<EvMonsterRaised> risen = K3Events<EvMonsterRaised>(rig.h.events);
      REQUIRE(risen.size() == 1);
      CHECK(risen[0].corpseId == c2);
      CHECK(rig.Corpse(c1) == nullptr);
    }
  }

  TEST_CASE("K3 runtime: a hero's fire skill burns the bodies its area lands on (fire wall ticks, the meteor's fall)") {
    for (const char* skill : {"fire_wall", "meteor"}) {
      INFO(skill);
      K3Rig rig(5, ClassId::Mage);
      Hero& hero = *rig.hero;
      const int32_t idx = hero.Skills().IndexOf(skill);
      REQUIRE(idx >= 0);
      hero.Skills().SetLevel(idx, 1);
      hero.RecalcDerived(rig.h.ctx.equip);
      hero.FillHpMana();
      const Vec2 p = hero.Position();
      // A sturdy target standing on the bodies (the cast lands on it), bodies around it and one outside the area.
      const uint32_t on = rig.CorpseAt(K3Footman(), p + Vec2(3, 0.5));
      const uint32_t off = rig.CorpseAt(K3Footman(), p + Vec2(3, 4));
      const EntityId target = rig.Spawn(test::fixture::Brute(), p + Vec2(3, 0), MonsterState::Idle);
      rig.combat.SetAttackTarget(target, false);
      hero.lastAttackMs = 1e18;
      const double t0 = rig.Now();
      REQUIRE(rig.combat.RequestSkill(idx, SkillAim{}) == SkillRequestResult::Executed);
      rig.StepUntilMs(t0 + 1500);
      CHECK(rig.Corpse(on) == nullptr);
      CHECK(rig.Corpse(off) != nullptr);
      CHECK(Removals(rig.h.events, CorpseRemoveReason::Burned).size() == 1);
    }
  }

  TEST_CASE("K3 runtime: consume: out of combat or under half life, a corpse within 1 tile, 2 s, heals 30 %") {
    // Out of combat (idle): it eats the nearest corpse within a tile, rooted, and heals at the end.
    {
      K3Rig rig;
      const uint32_t corpse = rig.CorpseAt(K3Footman(), Vec2(50.8, 50));
      rig.CorpseAt(K3Footman(), Vec2(51.5, 50));  // 1.5 away: out of reach
      const EntityId g = rig.Spawn(K3Gnawer(), Vec2(50, 50), MonsterState::Idle);
      rig.M(g).hp = 10;
      rig.h.events.Clear();
      rig.Step();
      CHECK(rig.M(g).consumeCorpse == corpse);
      CHECK(rig.Corpse(corpse)->holder == g);
      const std::vector<EvPlayAnim> anims = K3Events<EvPlayAnim>(rig.h.events);
      REQUIRE(anims.size() == 1);
      CHECK(anims[0].clip == "Consume");
      CHECK(anims[0].action == AnimAction::Attack);
      CHECK(anims[0].durationMs == 2000);
      CHECK(anims[0].faceTarget == Vec2(50.8, 50));
      Snapshot snap;
      rig.monsters.FillSnapshot(snap);
      bool viewed = false;
      for (const MonsterView& v : snap.monsters) viewed = viewed || (v.id == g && v.consumingCorpse == corpse);
      CHECK(viewed);
      const double start = rig.Now();
      const Vec2 pos = rig.M(g).pos;
      rig.h.events.Clear();
      rig.StepUntilMs(start + 2000 - kSimStepMs);
      CHECK(rig.M(g).pos == pos);  // rooted
      CHECK(rig.M(g).hp == 10);
      CHECK(rig.Corpse(corpse) != nullptr);
      rig.StepUntilMs(start + 2000);
      CHECK(rig.M(g).hp == 10 + std::floor(42 * 0.3));
      const std::vector<EvCorpseRemoved> eaten = Removals(rig.h.events, CorpseRemoveReason::Consumed);
      REQUIRE(eaten.size() == 1);
      CHECK((eaten[0].corpseId == corpse && eaten[0].by == g));
      CHECK(rig.M(g).consumeCorpse == 0);
    }
    // In combat above half life it fights; below half life it eats; remains are never eaten; a stun aborts it.
    {
      K3Rig rig;
      rig.PlaceHero(Vec2(51, 50));
      rig.CorpseAt(K3Gnawer(), Vec2(50.5, 50.5));  // a carcass (remains): ignored
      const EntityId g = rig.Spawn(K3Gnawer(), Vec2(50, 50));
      rig.ScriptNoDodge();
      rig.Step();
      CHECK(rig.M(g).consumeCorpse == 0);
      const uint32_t corpse = rig.CorpseAt(K3Footman(), Vec2(49.5, 50));
      rig.Step(60);
      CHECK(rig.M(g).consumeCorpse == 0);  // full life, fighting
      rig.M(g).hp = 20;                     // under half (42 / 2)
      rig.StepUntil([&] { return rig.M(g).consumeCorpse != 0; }, 120);
      CHECK(rig.M(g).consumeCorpse == corpse);
      rig.h.events.Clear();
      rig.Silence(g);
      rig.Step();
      CHECK(rig.M(g).consumeCorpse == 0);
      size_t cancels = 0;
      for (const EvMonsterAttackCancelled& e : K3Events<EvMonsterAttackCancelled>(rig.h.events)) cancels += e.monster == g ? 1 : 0;
      CHECK(cancels == 1);
      CHECK(rig.Corpse(corpse) != nullptr);
      CHECK(rig.Corpse(corpse)->holder == kNoEntity);
      CHECK(rig.M(g).hp == 20);
    }
    // A spent corpse (a Raised's) is eaten; a corpse burned mid-channel ends it without a heal.
    {
      K3Rig rig;
      const MonsterDef* risen = rig.h.ctx.data.FindMonster("fx_k3_risen");
      const EntityId body = rig.monsters.SummonAt(*risen, Vec2(50.5, 50), kNoEntity, 100, "test");
      rig.Kill(body);
      const uint32_t spent = rig.Newest()->id;
      REQUIRE(rig.Corpse(spent)->spent);
      const EntityId g = rig.Spawn(K3Gnawer(), Vec2(50, 50), MonsterState::Idle);
      rig.M(g).hp = 10;
      rig.Step();
      CHECK(rig.M(g).consumeCorpse == spent);
      AbilityShapeDef fire;
      fire.kind = AbilityShapeKind::Circle;
      fire.radius = 0.5;
      rig.monsters.BurnCorpses(fire, Vec2(50.5, 50), Vec2(1, 0));
      rig.h.events.Clear();
      rig.Step();
      CHECK(rig.M(g).consumeCorpse == 0);
      CHECK(rig.M(g).hp == 10);
      CHECK(K3Events<EvMonsterAttackCancelled>(rig.h.events).size() == 1);
    }
  }

  TEST_CASE("K3 runtime: the Unburied trait rises once after its delay; fire, a crit (notOnCrit), a burned corpse stop it") {
    MonsterDef foot = K3Footman("fx_k3_unburied");
    foot.riseAgain.present = true;
    foot.riseAgain.chance = 1;
    foot.riseAgain.delayMs = 3000;
    foot.riseAgain.hpPct = 40;
    foot.riseAgain.notOnCrit = true;
    {
      K3Rig rig;
      const EntityId f = rig.Spawn(foot, Vec2(50, 50), MonsterState::Idle);
      const double death = rig.Now();
      rig.Kill(f);
      const uint32_t corpse = rig.Newest()->id;
      CHECK(rig.Corpse(corpse)->holder == f);  // the pending rise holds it (no raiser or eater takes it)
      rig.h.events.Clear();
      rig.StepUntilMs(death + 3000 - kSimStepMs);
      CHECK(K3Events<EvMonsterRaised>(rig.h.events).empty());
      rig.StepUntilMs(death + 3000);
      const std::vector<EvMonsterRaised> risen = K3Events<EvMonsterRaised>(rig.h.events);
      REQUIRE(risen.size() == 1);
      CHECK(risen[0].selfRise);
      CHECK(risen[0].raiser == kNoEntity);
      CHECK(risen[0].corpseId == corpse);
      CHECK(risen[0].abilityId == "riseAgain");
      CHECK(risen[0].defId == "fx_k3_unburied");
      const std::vector<EvCorpseRemoved> gone = Removals(rig.h.events, CorpseRemoveReason::Raised);
      REQUIRE(gone.size() == 1);
      CHECK(gone[0].by == kNoEntity);
      MonsterInstance& again = rig.M(risen[0].entity);
      CHECK(again.role == MonsterRole::Raised);
      CHECK(again.raiserId == kNoEntity);
      CHECK(again.hp == std::floor(again.maxHp * 0.4));
      CHECK(again.pos == Vec2(50, 50));
      // Once: its second death leaves a spent corpse and no rise.
      rig.h.events.Clear();
      rig.Kill(risen[0].entity);
      REQUIRE(rig.Newest() != nullptr);
      CHECK(rig.Newest()->spent);
      CHECK(rig.Newest()->holder == kNoEntity);
      rig.StepUntilMs(rig.Now() + 4000);
      CHECK(K3Events<EvMonsterRaised>(rig.h.events).empty());
    }
    // Stoppers: a fire killing blow, a crit (notOnCrit), a burned corpse, a boss role.
    for (int stopper = 0; stopper < 4; ++stopper) {
      INFO("stopper " << stopper);
      K3Rig rig;
      const EntityId f = rig.Spawn(foot, Vec2(50, 50), MonsterState::Idle,
                                   stopper == 3 ? MonsterRole::HuntLeader : MonsterRole::Regular);
      rig.Kill(f, stopper == 0 ? DamageType::Fire : DamageType::Physical, stopper == 1);
      if (stopper == 2) {
        AbilityShapeDef fire;
        fire.kind = AbilityShapeKind::Circle;
        fire.radius = 1;
        CHECK(rig.monsters.BurnCorpses(fire, Vec2(50, 50), Vec2(1, 0)) == 1);
      }
      rig.h.events.Clear();
      rig.StepUntilMs(rig.Now() + 3500);
      CHECK(K3Events<EvMonsterRaised>(rig.h.events).empty());
    }
    // A crit without notOnCrit rises; the chance draws once on the Ai stream (0.25: 0.2 rises, 0.3 does not).
    for (double roll : {0.2, 0.3}) {
      INFO(roll);
      MonsterDef maybe = foot;
      maybe.riseAgain.chance = 0.25;
      maybe.riseAgain.notOnCrit = false;
      K3Rig rig;
      const EntityId f = rig.Spawn(maybe, Vec2(50, 50), MonsterState::Idle);
      rig.h.rng.Get(RngStream::Ai).Script({roll});
      rig.Kill(f, DamageType::Physical, true);
      rig.h.events.Clear();
      rig.StepUntilMs(rig.Now() + 3100);
      CHECK(K3Events<EvMonsterRaised>(rig.h.events).size() == (roll < 0.25 ? 1u : 0u));
    }
  }

  TEST_CASE("K3 runtime: the Unburied affix rises (crits do not stop it) as itself; affixes with families roll by family") {
    K3Rig rig;
    const EliteAffixTable& table = rig.h.ctx.data.Combat().eliteAffixes;
    MonsterDef foot = K3Footman("fx_k3_elite");
    foot.elite = true;
    const EntityId f = rig.Spawn(foot, Vec2(50, 50), MonsterState::Idle);
    ApplyEliteAffixes(rig.M(f), std::vector<EliteAffixType>{EliteAffixType::Unburied}, table, rig.h.ctx.data.Monsters().ai);
    const double maxHp = rig.M(f).maxHp;
    rig.Kill(f, DamageType::Physical, true);
    rig.h.events.Clear();
    rig.StepUntilMs(rig.Now() + 3000);
    const std::vector<EvMonsterRaised> risen = K3Events<EvMonsterRaised>(rig.h.events);
    REQUIRE(risen.size() == 1);
    const MonsterInstance& again = rig.M(risen[0].entity);
    CHECK(again.maxHp == maxHp);
    CHECK(again.hp == std::floor(maxHp * 0.5));
    REQUIRE(again.affixes.size() == 1);
    CHECK(again.affixes[0].type == EliteAffixType::Unburied);
    CHECK(again.def.elite);
    // The roll: an affix with families is in the pool only for those families.
    EliteAffixTable only = table;
    only.order.assign(1, EliteAffixType::Unburied);  // (GCC 13 + ASan: = {x} trips a false -Warray-bounds)
    Rng rng(9);
    CHECK(RollEliteAffixes(only, "emerald_plains", rng, "unburied") ==
          std::vector<EliteAffixType>(1, EliteAffixType::Unburied));
    CHECK(RollEliteAffixes(only, "emerald_plains", rng, "living").empty());
    CHECK(RollEliteAffixes(only, "emerald_plains", rng).empty());
    CHECK(table.RollableFor(EliteAffixType::Unburied, "unburied"));
    CHECK_FALSE(table.RollableFor(EliteAffixType::Unburied, "gravegnawers"));
    CHECK(table.RollableFor(EliteAffixType::Swift, ""));
    // Over the whole K3 pool (eight affixes, one for the Unburied only), every family draws the same count.
    for (const char* fam : {"unburied", "living"}) {
      Rng r2(11);
      for (int i = 0; i < 40; ++i) {
        const std::vector<EliteAffixType> got = RollEliteAffixes(table, "abyss_rift", r2, fam);
        for (EliteAffixType t : got) CHECK((std::string(fam) == "unburied" || t != EliteAffixType::Unburied));
      }
    }
  }

  TEST_CASE("K3 runtime: a burst after its fuse hits the hero within its radius, capped, and monsters x2; burns bodies") {
    for (double dmg : {18.0, 1000.0}) {
      INFO(dmg);
      K3Rig rig;
      const MonsterDef bloat = K3Bloat(dmg);
      rig.PlaceHero(Vec2(51.5, 50));
      const EntityId b = rig.Spawn(bloat, Vec2(50, 50), MonsterState::Idle);
      const EntityId near = rig.Spawn(K3Caster({}), Vec2(49, 50), MonsterState::Idle);
      const EntityId far = rig.Spawn(K3Caster({}), Vec2(53, 50), MonsterState::Idle);  // 3 tiles: outside 2.2
      const uint32_t body = rig.CorpseAt(K3Footman(), Vec2(50, 51));
      rig.Silence(near);  // bystanders: they never swing at the hero
      rig.Silence(far);
      const double nearHp = rig.M(near).hp;
      const double farHp = rig.M(far).hp;
      rig.h.events.Clear();
      const double death = rig.Now();
      rig.Kill(b);
      const std::vector<EvTelegraph> fuse = K3Events<EvTelegraph>(rig.h.events);
      REQUIRE(fuse.size() == 1);
      CHECK(fuse[0].source == TelegraphSource::OnDeath);
      CHECK(fuse[0].entity == b);
      CHECK(fuse[0].abilityId == "onDeath");
      CHECK(fuse[0].kind == AbilityKind::Circle);
      CHECK(fuse[0].shape.radius == 2.2);
      CHECK(fuse[0].origin == Vec2(50, 50));
      CHECK(fuse[0].contactMs == death + 800);
      REQUIRE(rig.combat.PendingOnDeathEffects().size() == 1);
      Snapshot snap;
      rig.combat.FillSnapshot(snap);
      REQUIRE(snap.telegraphs.size() == 1);
      CHECK(snap.telegraphs[0].source == TelegraphSource::OnDeath);
      rig.StepUntilMs(death + 800 - kSimStepMs);
      CHECK(HeroHits(rig.h.events, b).empty());
      rig.StepUntilMs(death + 800);
      const std::vector<EvHit> hits = HeroHits(rig.h.events, b);
      REQUIRE(hits.size() == 1);
      const double cap = std::floor(rig.hero->MaxHp() * 20 / 100.0);
      const double expect = (std::min)(AfterResist(rig, dmg, DamageType::Fire), cap);
      CHECK(hits[0].amount == expect);
      CHECK(hits[0].element == DamageType::Fire);
      CHECK(hits[0].source == b);
      if (dmg > 100) CHECK(hits[0].amount == cap);
      CHECK(rig.M(near).hp == (std::max)(0.0, nearHp - std::floor(dmg * 2)));  // x monsterDmgMul 2, raw
      CHECK(rig.M(far).hp == farHp);
      CHECK(rig.Corpse(body) == nullptr);  // a fire burst burns the bodies in it
      CHECK(rig.combat.PendingOnDeathEffects().empty());
    }
    // A hero outside the radius takes nothing; a dodge roll through the blast avoids it.
    for (int c = 0; c < 2; ++c) {
      K3Rig rig;
      rig.PlaceHero(c == 0 ? Vec2(53, 50) : Vec2(51.5, 50));
      const EntityId b = rig.Spawn(K3Bloat(), Vec2(50, 50), MonsterState::Idle);
      const double death = rig.Now();
      rig.Kill(b);
      if (c == 1) {
        rig.StepUntilMs(death + 800 - 100);
        REQUIRE(rig.combat.RequestDodge(Vec2(-1, 0)));  // through the blast
      }
      rig.h.events.Clear();
      rig.StepUntilMs(death + 800);
      CHECK(HeroHits(rig.h.events).empty());
      if (c == 1) {
        bool iframe = false;
        for (const EvHit& h : K3Events<EvHit>(rig.h.events)) iframe = iframe || (h.target == kHeroEntityId && h.iframeAvoided);
        CHECK(iframe);
      }
    }
    // Death Save holds against a lethal burst (monster damage).
    for (bool save : {true, false}) {
      INFO("death save " << save);
      K3Rig rig;
      if (save) rig.h.ctx.equip.Ref(Stat::DeathSave) = 1;
      rig.hero->RecalcDerived(rig.h.ctx.equip);
      rig.PlaceHero(Vec2(51, 50));
      const EntityId b = rig.Spawn(K3Bloat(), Vec2(50, 50), MonsterState::Idle);
      rig.Kill(b);
      rig.hero->SetHp(1);
      rig.StepUntilMs(rig.Now() + 800);
      CHECK(rig.hero->Life() == (save ? HeroLife::Alive : HeroLife::Dying));
    }
  }

  TEST_CASE("K3 runtime: affix on-death (Scorched burst, Gravecold ring), a puddle's ticks, a spawn on death") {
    // Scorched: a burst of 10 % of the elite's life after 1 s within 2 tiles, capped at 20 %.
    {
      K3Rig rig;
      const EliteAffixTable& table = rig.h.ctx.data.Combat().eliteAffixes;
      MonsterDef foot = K3Footman("fx_k3_scorched");
      foot.elite = true;
      foot.hp = 300;
      rig.PlaceHero(Vec2(51.5, 50));
      const EntityId f = rig.Spawn(foot, Vec2(50, 50), MonsterState::Idle);
      ApplyEliteAffixes(rig.M(f), std::vector<EliteAffixType>{EliteAffixType::FireEnhanced}, table,
                        rig.h.ctx.data.Monsters().ai);
      const double maxHp = rig.M(f).maxHp;
      rig.h.events.Clear();
      const double death = rig.Now();
      rig.Kill(f, DamageType::Ice);
      const std::vector<EvTelegraph> fuse = K3Events<EvTelegraph>(rig.h.events);
      REQUIRE(fuse.size() == 1);
      CHECK(fuse[0].abilityId == "affix.fire_enhanced");
      CHECK(fuse[0].style == TelegraphStyle::Elite);
      CHECK(fuse[0].contactMs == death + 1000);
      rig.StepUntilMs(death + 1000);
      const std::vector<EvHit> hits = HeroHits(rig.h.events, f);
      REQUIRE(hits.size() == 1);
      const double base = (std::max)(1.0, std::floor(maxHp * 10 / 100.0 + 0.5));
      CHECK(hits[0].amount == (std::min)(AfterResist(rig, base, DamageType::Fire), std::floor(rig.hero->MaxHp() * 0.2)));
    }
    // Gravecold: a frost ring after 600 ms, slow 30 for 2.5 s, no damage.
    {
      K3Rig rig;
      const EliteAffixTable& table = rig.h.ctx.data.Combat().eliteAffixes;
      MonsterDef foot = K3Footman("fx_k3_gravecold");
      foot.elite = true;
      rig.PlaceHero(Vec2(52, 50));
      const EntityId f = rig.Spawn(foot, Vec2(50, 50), MonsterState::Idle);
      ApplyEliteAffixes(rig.M(f), std::vector<EliteAffixType>{EliteAffixType::Frozen}, table, rig.h.ctx.data.Monsters().ai);
      const double hp = rig.hero->Hp();
      const double death = rig.Now();
      rig.Kill(f);
      rig.h.events.Clear();
      rig.StepUntilMs(death + 600);
      CHECK(HeroHits(rig.h.events).empty());
      CHECK(rig.hero->Hp() == hp);
      const std::vector<EvStatusApplied> st = K3Events<EvStatusApplied>(rig.h.events);
      REQUIRE(st.size() == 1);
      CHECK(st[0].target == kHeroEntityId);
      CHECK(st[0].type == StatusType::Slow);
      CHECK(st[0].value == 30);
      CHECK(st[0].durationMs == 2500);
    }
    // A puddle: a ground hazard with its first pulse at placement, ticking the damage every 500 ms for 2 s.
    {
      K3Rig rig;
      MonsterDef ooze = K3Footman("fx_k3_ooze");
      ooze.onDeath.present = true;
      ooze.onDeath.kind = OnDeathKind::Puddle;
      ooze.onDeath.delayMs = 300;
      ooze.onDeath.shape.kind = AbilityShapeKind::Circle;
      ooze.onDeath.shape.radius = 1.5;
      ooze.onDeath.hasDmg = true;
      ooze.onDeath.dmg = 3;
      ooze.onDeath.hasDamageType = true;
      ooze.onDeath.damageType = DamageType::Poison;
      ooze.onDeath.puddle.durationMs = 2000;
      ooze.onDeath.puddle.tickMs = 500;
      rig.PlaceHero(Vec2(51, 50));
      const EntityId o = rig.Spawn(ooze, Vec2(50, 50), MonsterState::Idle);
      const double death = rig.Now();
      rig.Kill(o);
      rig.h.events.Clear();
      rig.StepUntilMs(death + 300 + 2100);
      const std::vector<EvHazard> hz = K3Events<EvHazard>(rig.h.events);
      REQUIRE(hz.size() == 1);
      CHECK(hz[0].source == o);
      CHECK(hz[0].abilityId == "onDeath");
      CHECK(hz[0].startMs == doctest::Approx(death + 300).epsilon(1e-9));
      CHECK(hz[0].untilMs == doctest::Approx(death + 2300).epsilon(1e-9));
      const std::vector<EvHit> ticks = HeroHits(rig.h.events, o);
      CHECK(ticks.size() == 4);  // at 0, 500, 1000, 1500 ms of the puddle
      for (const EvHit& t : ticks) {
        CHECK(t.tick);
        CHECK(t.amount == AfterResist(rig, 3, DamageType::Poison));
      }
      CHECK(rig.combat.Hazards().Entries().empty());
    }
    // A spawn: two bodies (Raised without a raiser) around the spot after the delay.
    {
      K3Rig rig;
      MonsterDef split = K3Footman("fx_k3_split");
      split.onDeath.present = true;
      split.onDeath.kind = OnDeathKind::Spawn;
      split.onDeath.delayMs = 500;
      split.onDeath.spawn.monsterId = "fx_k3_clot";
      split.onDeath.spawn.count = 2;
      const EntityId s = rig.Spawn(split, Vec2(50, 50), MonsterState::Idle);
      const double death = rig.Now();
      rig.Kill(s);
      CHECK(K3Events<EvTelegraph>(rig.h.events).empty());  // a spawn has no fuse decal
      rig.h.events.Clear();
      rig.StepUntilMs(death + 500);
      const std::vector<EvMonsterRaised> risen = K3Events<EvMonsterRaised>(rig.h.events);
      REQUIRE(risen.size() == 2);
      for (const EvMonsterRaised& r : risen) {
        CHECK(r.defId == "fx_k3_clot");
        CHECK(r.raiser == kNoEntity);
        CHECK(r.corpseId == 0);
        CHECK(r.abilityId == "onDeath");
        CHECK(rig.M(r.entity).role == MonsterRole::Raised);
        CHECK(rig.M(r.entity).hp == rig.M(r.entity).maxHp);
      }
      const double r = rig.h.ctx.data.Monsters().ai.summonRingRadius;
      CHECK(risen[0].pos == Vec2(JsRound(50 + r), 50));  // k = 0 at angle 0, rounded to its tile
      CHECK(risen[1].pos != risen[0].pos);
    }
  }

  TEST_CASE("K3 runtime: the volatile curse bursts every kill of the zone (12 %, 800 ms, r2, capped); a zone exit ends it") {
    K3Rig rig;
    const CurseDef* vol = nullptr;
    for (const CurseDef& c : rig.h.ctx.data.AbyssRun().curses) {
      if (c.deathBurst > 0) vol = &c;
    }
    REQUIRE(vol != nullptr);
    rig.combat.SetZoneOnDeath(VolatileCurseOnDeath(*vol), "curse." + vol->id);
    rig.PlaceHero(Vec2(51.9, 50));
    const EntityId f = rig.Spawn(K3Caster({}), Vec2(50, 50), MonsterState::Idle);
    const EntityId bystander = rig.Spawn(K3Caster({}), Vec2(51, 50), MonsterState::Idle);
    rig.Silence(bystander);
    const double maxHp = rig.M(f).maxHp;
    const double other = rig.M(bystander).hp;
    rig.h.events.Clear();
    const double death = rig.Now();
    rig.Kill(f);
    const std::vector<EvTelegraph> fuse = K3Events<EvTelegraph>(rig.h.events);
    REQUIRE(fuse.size() == 1);
    CHECK(fuse[0].abilityId == "curse." + vol->id);
    CHECK(fuse[0].contactMs == death + 800);
    rig.StepUntilMs(death + 800);
    const std::vector<EvHit> hits = HeroHits(rig.h.events, f);
    REQUIRE(hits.size() == 1);
    const double base = std::floor(maxHp * 0.12 + 0.5);
    CHECK(hits[0].amount == (std::min)(AfterResist(rig, base, DamageType::Fire), std::floor(rig.hero->MaxHp() * 0.2)));
    CHECK(rig.M(bystander).hp == other);  // the curse spares the monsters
    // A zone exit drops the zone's on-death effect (and every pending fuse).
    const EntityId g = rig.Spawn(K3Caster({}), Vec2(50, 52), MonsterState::Idle);
    rig.Kill(g);
    REQUIRE(rig.combat.PendingOnDeathEffects().size() == 1);
    rig.combat.OnZoneExit();
    CHECK(rig.combat.PendingOnDeathEffects().empty());
    rig.combat.OnZoneEnter();
    rig.h.events.Clear();
    rig.Kill(rig.Spawn(K3Caster({}), Vec2(50, 54), MonsterState::Idle));
    CHECK(K3Events<EvTelegraph>(rig.h.events).empty());
    CHECK(rig.combat.PendingOnDeathEffects().empty());
  }

  TEST_CASE("K3 runtime: a summon stands its bodies on its anchors or on a ring around the caster") {
    for (bool anchors : {true, false}) {
      INFO("anchors " << anchors);
      K3Rig rig;
      MonsterAbilityDef a = K3Ability("muster", AbilityKind::Summon, anchors ? AbilityAim::Anchor : AbilityAim::Self);
      a.cooldownMs = 100000;
      a.summon.present = true;
      a.summon.monsterId = "fx_k3_risen";
      a.summon.count = anchors ? 3 : 2;
      a.summon.at = anchors ? AbilitySummonAt::Anchors : AbilitySummonAt::Self;
      a.summon.capAlive = 4;
      if (anchors) a.summon.anchors = {TilePos{45, 40}, TilePos{45, 44}};
      const MonsterDef caster = K3Caster({a});
      rig.PlaceHero(Vec2(41.5, 40));
      const EntityId c = rig.Spawn(caster, Vec2(40, 40));
      rig.h.events.Clear();
      rig.ScriptNoDodge();
      rig.Step();
      REQUIRE(rig.M(c).pendingAbility == 0);
      const std::vector<EvTelegraph> marks = K3Events<EvTelegraph>(rig.h.events);
      REQUIRE(marks.size() == (anchors ? 3u : 2u));
      for (const EvTelegraph& t : marks) CHECK(t.kind == AbilityKind::Summon);
      rig.h.events.Clear();
      rig.StepUntil([&] { return rig.M(c).pendingAbility < 0; }, 120);
      const std::vector<EvMonsterRaised> risen = K3Events<EvMonsterRaised>(rig.h.events);
      REQUIRE(risen.size() == (anchors ? 3u : 2u));
      for (const EvMonsterRaised& r : risen) {
        CHECK(r.raiser == c);
        CHECK(r.corpseId == 0);
        CHECK(rig.M(r.entity).role == MonsterRole::Raised);
      }
      if (anchors) {
        CHECK(risen[0].pos == Vec2(45, 40));
        CHECK(risen[1].pos == Vec2(45, 44));
        CHECK(risen[2].pos == Vec2(45, 40));  // round robin over the anchors
      } else {
        const double r = rig.h.ctx.data.Monsters().ai.summonRingRadius;
        CHECK(risen[0].pos == Vec2(JsRound(40 + r), 40));
        CHECK(risen[1].pos == Vec2(JsRound(40 - r), 40));
      }
      CHECK(rig.monsters.RaisedAliveOf(c) == static_cast<int32_t>(risen.size()));
    }
  }

  TEST_CASE("K3 runtime: a zone exit drops the bodies, the pending rises and the fuses silently") {
    K3Rig rig;
    MonsterDef foot = K3Footman("fx_k3_unburied");
    foot.riseAgain.present = true;
    foot.riseAgain.delayMs = 3000;
    rig.Kill(rig.Spawn(foot, Vec2(50, 50), MonsterState::Idle));
    rig.Kill(rig.Spawn(K3Bloat(), Vec2(52, 50), MonsterState::Idle));
    REQUIRE(rig.monsters.CorpseRegistry().Entries().size() == 1);
    REQUIRE(rig.combat.PendingOnDeathEffects().size() == 1);
    rig.h.events.Clear();
    rig.combat.OnZoneExit();
    rig.monsters.OnZoneExit();
    CHECK(rig.monsters.CorpseRegistry().Entries().empty());
    CHECK(rig.combat.PendingOnDeathEffects().empty());
    CHECK(K3Events<EvCorpseRemoved>(rig.h.events).empty());
    rig.monsters.SetWorldForTesting(MOpenWorld(rig.zone.Map().cols, rig.zone.Map().rows));
    rig.StepUntilMs(rig.Now() + 3500);
    CHECK(K3Events<EvMonsterRaised>(rig.h.events).empty());
  }

  // =================================================================================================================
  // GameSim wiring: map seeds at zone entry, the kill pipeline's K3 step, TickOnDeath / TickCorpses, the snapshot views,
  // and a Raised body's kill through the real kill pipeline (no exp, gold, loot, quest drop, embers)
  // =================================================================================================================
  TEST_CASE("K3 wiring: corpse seeds, the kill pipeline's K3 step, the step hooks, the snapshot, Raised rewards") {
    static std::unique_ptr<DataStore> store;
    if (store == nullptr) {
      DataLoadReport report;
      store = K3LoadEdited({{"assets.json", K3AddAsset},
                            {"monsters.json", K3AddDefs},
                            {"maps.json",
                             [](JsonValue& doc) {
                               doc.FindMutable("maps")->FindMutable("emerald_plains")->Set(
                                   "corpseSeeds", K3Parse(R"([{"col": 60, "row": 54, "monsterId": "fx_k3_risen"},
                                                              {"col": 62, "row": 55, "monsterId": "fx_k3_risen"}])"));
                             }}},
                           report);
      REQUIRE(report.Ok());
    }
    auto sim = GameSim::Create(*store, SimConfig{});
    REQUIRE(sim != nullptr);
    REQUIRE(sim->NewGame(ClassId::Warrior, Difficulty::Normal, 77));
    // Zone entry seeded the Sheepfold-like bodies (no TTL).
    std::vector<EvCorpseAdded> seeded = K3Events<EvCorpseAdded>(sim->Events());
    REQUIRE(seeded.size() == 2);
    CHECK((seeded[0].seeded && seeded[0].expiresMs == 0 && seeded[0].pos == Vec2(60, 54)));
    sim->Context().sys.story->FinishAllBeats();
    sim->Step();
    REQUIRE(sim->View().corpses.size() == 2);
    CHECK(sim->View().corpses[1].pos == Vec2(62, 55));
    CHECK(sim->View().corpses[1].seeded);
    SimContext& ctx = sim->Context();
    ZoneRuntime& zone = *ctx.sys.zone;
    const MapDef& map = zone.Map();
    TilePos spot{-1, -1};
    for (int32_t r = 3; r < map.rows - 3 && spot.col < 0; ++r) {
      for (int32_t c = 3; c < map.cols - 3 && spot.col < 0; ++c) {
        bool ok = true;
        for (int32_t dr = -2; dr <= 2 && ok; ++dr) {
          for (int32_t dc = -2; dc <= 3 && ok; ++dc) ok = zone.Walkable(c + dc, r + dr);
        }
        for (const MapCampDef& camp : map.camps) ok = ok && Dist(Vec2(c, r), camp.pos.Center()) > map.safeZoneRadius + 4;
        if (ok) spot = TilePos{c, r};
      }
    }
    REQUIRE(spot.col >= 0);
    ctx.sys.locomotion->Teleport(spot.Center(), TeleportReason::Debug);
    // A bloat dies by the hero: the kill pipeline registers nothing for it (no corpse) and schedules its fuse; the step
    // resolves it 800 ms later; a footman's body joins the registry and shows in the snapshot.
    const MonsterDef bloat = K3Bloat(5);
    const MonsterDef foot = K3Footman();
    MonsterSpawnParams p;
    p.baseDef = &bloat;
    p.tile = TilePos{spot.col + 1, spot.row};
    const EntityId b = ctx.sys.monsters->Spawn(p);
    p.baseDef = &foot;
    p.tile = TilePos{spot.col, spot.row + 1};
    const EntityId f = ctx.sys.monsters->Spawn(p);
    REQUIRE((b != kNoEntity && f != kNoEntity));
    const auto kill = [&](EntityId id) {
      MonsterHitRequest r;
      r.monster = id;
      r.amount = ctx.sys.monsters->Find(id)->hp;
      r.attacker = kHeroEntityId;
      r.source = KillSource::HeroSkill;
      ctx.sys.combat->DamageMonster(r);
    };
    sim->ClearEvents();
    kill(f);
    kill(b);
    std::vector<EvCorpseAdded> added = K3Events<EvCorpseAdded>(sim->Events());
    REQUIRE(added.size() == 1);
    CHECK(added[0].monster == f);
    REQUIRE(K3Events<EvTelegraph>(sim->Events()).size() == 1);
    REQUIRE(ctx.sys.combat->PendingOnDeathEffects().size() == 1);
    sim->Step();
    CHECK(sim->View().corpses.size() == 3);
    bool fuseView = false;
    for (const TelegraphView& v : sim->View().telegraphs) fuseView = fuseView || v.source == TelegraphSource::OnDeath;
    CHECK(fuseView);
    int heroHits = 0;
    for (int i = 0; i < 60; ++i) {
      sim->Step();
      for (const EvHit& h : K3Events<EvHit>(sim->Events())) heroHits += h.target == kHeroEntityId && h.source == b ? 1 : 0;
    }
    CHECK(heroHits == 1);
    CHECK(ctx.sys.combat->PendingOnDeathEffects().empty());
    // A Raised body's kill through the real pipeline: no exp, gold, loot, quest drop or embers.
    const MonsterDef* risen = ctx.data.FindMonster("fx_k3_risen");
    REQUIRE(risen != nullptr);
    const EntityId body = ctx.sys.monsters->SummonAt(*risen, Vec2(spot.col + 2, spot.row), kNoEntity, 100, "test");
    REQUIRE(body != kNoEntity);
    const int64_t exp = ctx.sys.hero->Exp();
    const int64_t gold = ctx.sys.hero->Gold();
    const auto totalKills = ctx.session.totalKills;
    sim->ClearEvents();
    kill(body);
    CHECK(ctx.sys.hero->Exp() == exp);
    CHECK(ctx.sys.hero->Gold() == gold);
    CHECK(ctx.session.totalKills == totalKills + 1);  // still a kill
    CHECK(K3Events<EvLootDropped>(sim->Events()).empty());
    CHECK(K3Events<EvPotionDropped>(sim->Events()).empty());
    CHECK(K3Events<EvEmbersGained>(sim->Events()).empty());
    CHECK(K3Events<EvExpGained>(sim->Events()).empty());
    added = K3Events<EvCorpseAdded>(sim->Events());
    REQUIRE(added.size() == 1);
    CHECK(added[0].spent);
  }

  // =================================================================================================================
  // Review fixes (2B review): a raise / summon is a cast between swings (R1); every end of a hold lets the corpses go
  // (R2); consume and swings (R3); a Raised kill pays nothing through the real pipeline (R4, R5); what the on-death
  // runtime feeds its rules (R6); rise and collapse rules (R7); hero fire areas and a beast's fire kill burn bodies (R8)
  // =================================================================================================================
  TEST_CASE("K3 runtime: a raise / summon is a cast between swings: from Chase, past the swing reach, on its cooldown") {
    // From Chase with the hero 5 tiles away (the swing reach is 2 x 1.2): the opening raise of two on the fight's first
    // step, rooted where it stands, still chasing; the bodies stand up at contact.
    {
      K3Rig rig;
      MonsterAbilityDef open = K3Raise("pull", 2, 4, 600000);
      open.priority = 5;
      open.hasRangeMax = true;
      open.rangeMax = 8;
      const MonsterDef raiser = K3Caster({K3Raise("flock", 1, 4, 2400), open});
      const uint32_t c1 = rig.CorpseAt(K3Footman(), Vec2(39, 41));
      const uint32_t c2 = rig.CorpseAt(K3Footman(), Vec2(39, 39));
      rig.PlaceHero(Vec2(45, 40));
      const EntityId c = rig.Spawn(raiser, Vec2(40, 40), MonsterState::Chase);
      rig.h.events.Clear();
      rig.Step();
      REQUIRE(rig.M(c).pendingAbility == 1);
      CHECK(rig.M(c).state == MonsterState::Chase);
      CHECK(Dist(rig.M(c).pos, rig.hero->Position()) > 2 * rig.h.ctx.data.Monsters().ai.attackExitMul);
      CHECK((rig.Corpse(c1)->holder == c && rig.Corpse(c2)->holder == c));
      CHECK(K3Events<EvTelegraph>(rig.h.events).size() == 2);
      const Vec2 at = rig.M(c).pos;
      const double contact = rig.M(c).abilityRootedUntilMs;
      rig.StepUntilMs(contact - kSimStepMs);
      CHECK(rig.M(c).pos == at);  // rooted for the cast
      rig.h.events.Clear();
      rig.StepUntilMs(contact);
      CHECK(K3Events<EvMonsterRaised>(rig.h.events).size() == 2);
    }
    // In the Attack state: one body per cast on a 2400 ms cooldown, swings every 1500 ms. The second raise starts the
    // moment its cooldown ends (between swings), not at the next due swing (3000 ms).
    {
      K3Rig rig;
      const MonsterDef raiser = K3Caster({K3Raise("flock", 1, 5, 2400)});
      rig.PlaceHero(Vec2(41.5, 40));
      for (int i = 0; i < 5; ++i) rig.CorpseAt(K3Footman(), Vec2(38.5 + i * 0.5, 41.5));
      const EntityId c = rig.Spawn(raiser, Vec2(40, 40));
      rig.h.events.Clear();
      std::vector<double> raises, basics;
      for (int s = 0; s < 4 * 60 && raises.size() < 2; ++s) {
        rig.ScriptNoDodge();
        rig.Step();
        for (const EvPlayAnim& a : K3Events<EvPlayAnim>(rig.h.events)) {
          if (a.entity == c && a.clip == "Cast01") raises.push_back(a.startMs);
          if (a.entity == c && a.clip == "Attack01") basics.push_back(a.startMs);
        }
        for (EntityId id : rig.RaisedOf(c)) rig.Silence(id);
        rig.h.events.Clear();
      }
      REQUIRE(raises.size() == 2);
      REQUIRE(basics.size() == 1);
      CHECK(basics[0] - raises[0] == doctest::Approx(1500).epsilon(1e-9));
      CHECK(raises[1] - raises[0] >= 2400 - 1e-6);
      CHECK(raises[1] - raises[0] < 2400 + kSimStepMs);
    }
    // One action at a time: on a 1700 ms cooldown the raise comes due while the basic swing (started at 1500 ms) is on
    // its way to contact; the raise starts in the step the swing lands.
    {
      K3Rig rig;
      const MonsterDef raiser = K3Caster({K3Raise("flock", 1, 5, 1700)});
      rig.PlaceHero(Vec2(41.5, 40));
      for (int i = 0; i < 5; ++i) rig.CorpseAt(K3Footman(), Vec2(38.5 + i * 0.5, 41.5));
      const EntityId c = rig.Spawn(raiser, Vec2(40, 40));
      rig.h.events.Clear();
      std::vector<double> raises;
      double basicContact = 0, landed = 0;
      for (int s = 0; s < 4 * 60 && raises.size() < 2; ++s) {
        rig.ScriptNoDodge();
        rig.Step();
        for (const EvPlayAnim& a : K3Events<EvPlayAnim>(rig.h.events)) {
          if (a.entity == c && a.clip == "Cast01") raises.push_back(a.startMs);
          if (a.entity == c && a.clip == "Attack01") basicContact = a.startMs + a.contactMs;
        }
        if (landed == 0 && !HeroHits(rig.h.events, c).empty()) landed = rig.Now();
        for (EntityId id : rig.RaisedOf(c)) rig.Silence(id);
        rig.h.events.Clear();
      }
      REQUIRE(raises.size() == 2);
      REQUIRE(basicContact > raises[0] + 1700);  // the swing is still on its way when the cooldown ends
      CHECK(landed >= basicContact - 1e-6);
      CHECK(raises[1] == landed);
    }
    // The hero range gates it: rangeMax 6, the hero 7.5 tiles away: no cast; 5.5 tiles: the next step casts.
    {
      K3Rig rig;
      MonsterAbilityDef flock = K3Raise("flock", 1, 4, 8000);
      flock.hasRangeMax = true;
      flock.rangeMax = 6;
      rig.CorpseAt(K3Footman(), Vec2(39, 41));
      rig.PlaceHero(Vec2(47.5, 40));
      const EntityId c = rig.Spawn(K3Caster({flock}), Vec2(40, 40), MonsterState::Chase);
      rig.Step();
      CHECK(rig.M(c).pendingAbility == -1);
      rig.PlaceHero(rig.M(c).pos + Vec2(5.5, 0));
      rig.Step();
      CHECK(rig.M(c).pendingAbility == 0);
    }
    // Other kinds stay on the swing: a circle ability never starts from Chase; a summon does.
    {
      K3Rig rig;
      MonsterAbilityDef slam = K3Ability("slam", AbilityKind::Circle, AbilityAim::Self, "Cast01");
      slam.hasShape = true;
      slam.shape.kind = AbilityShapeKind::Circle;
      slam.shape.radius = 2;
      MonsterAbilityDef call = K3Ability("call", AbilityKind::Summon, AbilityAim::Self);
      call.cooldownMs = 100000;
      call.summon.present = true;
      call.summon.monsterId = "fx_k3_risen";
      call.summon.count = 1;
      call.summon.at = AbilitySummonAt::Self;
      rig.PlaceHero(Vec2(45, 40));
      const EntityId a = rig.Spawn(K3Caster({slam}), Vec2(40, 40), MonsterState::Chase);
      const EntityId b = rig.Spawn(K3Caster({slam, call}), Vec2(40, 44), MonsterState::Chase);
      rig.Step();
      CHECK(rig.M(a).pendingAbility == -1);
      CHECK(rig.M(b).pendingAbility == 1);
    }
    // Data: a raise / summon's range is not bound by the swing reach (no "never picked" / "no effect" warning); a
    // circle's still is.
    {
      DataLoadReport report;
      K3LoadDef(K3DefJson("fx_k3_far", R"("attackRange": 2, "abilities": [
          {"id": "flock", "kind": "raise", "clip": "Cast01", "cooldownMs": 8000, "aim": "corpse", "rangeMin": 3,
           "rangeMax": 9, "summon": {"monsterId": "fx_k3_risen", "count": 1}},
          {"id": "call", "kind": "summon", "clip": "Cast01", "cooldownMs": 8000, "aim": "self", "rangeMax": 9,
           "summon": {"monsterId": "fx_k3_risen", "count": 1}},
          {"id": "slam", "kind": "circle", "clip": "Cast01", "windupMs": 900, "cooldownMs": 0, "aim": "self",
           "shape": {"radius": 1}, "rangeMin": 3}])"),
                report);
      if (!report.Ok()) MESSAGE(report.Summary());
      CHECK(report.Ok());
      CHECK_FALSE(K3HasIssue(report.warnings, "fx_k3_far.abilities.flock", "never picked"));
      CHECK_FALSE(K3HasIssue(report.warnings, "fx_k3_far.abilities.flock", "no effect past"));
      CHECK_FALSE(K3HasIssue(report.warnings, "fx_k3_far.abilities.call", "no effect past"));
      CHECK(K3HasIssue(report.warnings, "fx_k3_far.abilities.slam.rangeMin", "never picked"));
    }
  }

  TEST_CASE("K3 runtime: every end of a hold lets its corpses go: a stun, F2, a dead hero at contact, a death") {
    // A raise winding up holds two corpses; a stun, F2 or a dead hero at contact frees both, and another raiser takes
    // them; the raiser's own death frees them too.
    for (int abort = 0; abort < 4; ++abort) {
      INFO("abort " << abort);
      K3Rig rig;
      rig.PlaceHero(Vec2(41.5, 40));
      const uint32_t c1 = rig.CorpseAt(K3Footman(), Vec2(41, 41));
      const uint32_t c2 = rig.CorpseAt(K3Footman(), Vec2(42, 42));
      const EntityId c = rig.Spawn(K3Caster({K3Raise("flock", 2, 0, 100000)}), Vec2(40, 40));
      rig.ScriptNoDodge();
      rig.Step();
      REQUIRE(rig.M(c).pendingAbility == 0);
      REQUIRE((rig.Corpse(c1)->holder == c && rig.Corpse(c2)->holder == c));
      const double contact = rig.M(c).abilityRootedUntilMs;
      rig.h.events.Clear();
      if (abort == 0) {
        rig.Silence(c);
        rig.Step();
      } else if (abort == 1) {
        rig.combat.OnFreezeBegin(true);
      } else if (abort == 2) {
        rig.hero->SetHp(0);
        rig.combat.KillHero(HeroDeathCause::Other);
        REQUIRE(rig.hero->Life() == HeroLife::Dying);
        rig.StepUntilMs(contact);
        CHECK(rig.hero->Life() == HeroLife::Dying);
      } else {
        rig.Kill(c);
      }
      CHECK(rig.M(c).pendingAbility == -1);
      CHECK(rig.Corpse(c1)->holder == kNoEntity);
      CHECK(rig.Corpse(c2)->holder == kNoEntity);
      CHECK(K3Events<EvMonsterRaised>(rig.h.events).empty());
      if (abort == 2) continue;  // the hero is down: nobody fights
      size_t cancels = 0;
      for (const EvMonsterAttackCancelled& e : K3Events<EvMonsterAttackCancelled>(rig.h.events)) {
        cancels += e.monster == c ? 1 : 0;
      }
      CHECK(cancels == 1);
      const EntityId other = rig.Spawn(K3Caster({K3Raise("flock", 2, 0, 100000)}), Vec2(43, 41), MonsterState::Chase);
      rig.ScriptNoDodge();
      rig.Step();
      CHECK(rig.M(other).pendingAbility == 0);
      CHECK(rig.Corpse(c1)->holder == other);
      CHECK(rig.Corpse(c2)->holder == other);
    }
    // An eater killed mid-channel lets its corpse go; another eater takes it.
    {
      K3Rig rig;
      const uint32_t corpse = rig.CorpseAt(K3Footman(), Vec2(50.8, 50));
      const EntityId g = rig.Spawn(K3Gnawer(), Vec2(50, 50), MonsterState::Idle);
      rig.M(g).hp = 10;
      rig.Step();
      REQUIRE(rig.M(g).consumeCorpse == corpse);
      REQUIRE(rig.Corpse(corpse)->holder == g);
      rig.Kill(g);
      REQUIRE(rig.Corpse(corpse) != nullptr);
      CHECK(rig.Corpse(corpse)->holder == kNoEntity);
      const EntityId g2 = rig.Spawn(K3Gnawer(), Vec2(51.5, 50), MonsterState::Idle);
      rig.M(g2).hp = 10;
      rig.StepUntil([&] { return rig.M(g2).consumeCorpse != 0; }, 120);  // once the AI's activity set takes it in
      CHECK(rig.M(g2).consumeCorpse == corpse);
      CHECK(rig.Corpse(corpse)->holder == g2);
    }
  }

  TEST_CASE("K3 runtime: consume waits for a swing on its way, never bites while eating, never starts while returning") {
    // A gnawer under half its life swings (no corpse yet); a corpse falls at its feet while the swing is on its way: the
    // bite lands first, then the channel starts; while it eats it neither swings nor bites.
    {
      K3Rig rig;
      rig.PlaceHero(Vec2(51, 50));
      const EntityId g = rig.Spawn(K3Gnawer(), Vec2(50, 50));
      rig.M(g).hp = 20;  // under half (42 / 2)
      rig.ScriptNoDodge(8);
      rig.Step();
      REQUIRE(rig.combat.HasPendingStrike(g));
      const uint32_t corpse = rig.CorpseAt(K3Footman(), Vec2(49.5, 50));
      rig.h.events.Clear();
      size_t bitesBefore = 0;
      bool pendingAtStart = true;
      for (int s = 0; s < 120 && rig.M(g).consumeCorpse == 0; ++s) {
        rig.Step();
        bitesBefore += HeroHits(rig.h.events, g).size();
        if (rig.M(g).consumeCorpse != 0) pendingAtStart = rig.combat.HasPendingStrike(g);
        rig.h.events.Clear();
      }
      REQUIRE(rig.M(g).consumeCorpse == corpse);
      CHECK(bitesBefore == 1);  // the swing on its way landed before the channel
      CHECK_FALSE(pendingAtStart);
      size_t bitesDuring = 0, swingsDuring = 0;
      while (rig.M(g).consumeCorpse != 0) {
        rig.ScriptNoDodge();
        rig.Step();
        bitesDuring += HeroHits(rig.h.events, g).size();
        for (const EvPlayAnim& a : K3Events<EvPlayAnim>(rig.h.events)) swingsDuring += a.entity == g && a.clip == "Attack01";
        if (rig.M(g).consumeCorpse != 0) CHECK_FALSE(rig.combat.HasPendingStrike(g));
        rig.h.events.Clear();
      }
      CHECK(bitesDuring == 0);
      CHECK(swingsDuring == 0);
      CHECK(rig.M(g).hp == 20 + std::floor(42 * 0.3));  // it finished and healed
    }
    // A returning eater under half its life walks past a corpse at its feet.
    {
      K3Rig rig;
      const uint32_t corpse = rig.CorpseAt(K3Footman(), Vec2(50.5, 50));
      const EntityId g = rig.Spawn(K3Gnawer(), Vec2(50, 50), MonsterState::Returning);
      rig.M(g).spawnAnchor = TilePos{62, 50};
      rig.M(g).hp = 10;
      for (int s = 0; s < 3; ++s) {
        REQUIRE(rig.M(g).state == MonsterState::Returning);
        rig.Step();
        CHECK(rig.M(g).consumeCorpse == 0);
      }
      CHECK(rig.Corpse(corpse)->holder == kNoEntity);
    }
  }

  // Through the real kill pipeline (GameSim): an elite fx_k3_risen whose kills pay embers, with an active beast, a quest
  // whose drop comes from it (always), life on kill and a hero below full life. As a Raised body its kill pays none of
  // it and draws nothing from the Loot or Quests streams; the same body as a regular monster pays each.
  TEST_CASE("K3 wiring: a Raised kill pays nothing: no Spirit, kill heal, pet exp, embers, quest drop or Loot draw") {
    static std::unique_ptr<DataStore> store;
    static std::string questId;
    if (store == nullptr) {
      const std::string startZone = test::RealData().World().defaultMap;
      DataLoadReport report;
      store = K3LoadEdited({{"assets.json", K3AddAsset},
                            {"monsters.json", K3AddDefs},
                            {"quests.json",
                             [startZone](JsonValue& doc) {
                               // The start zone's first drop objective drops from fx_k3_risen, always.
                               for (JsonValue& q : doc.FindMutable("quests")->MutableItems()) {
                                 const JsonValue* zone = q.Find("zone");
                                 if (!questId.empty() || zone == nullptr || zone->AsString() != startZone) continue;
                                 for (JsonValue& o : q.FindMutable("objectives")->MutableItems()) {
                                   JsonValue* src = o.FindMutable("source");
                                   if (src == nullptr || src->Get("kind").AsString() != "drop") continue;
                                   src->Set("monsters", K3Parse(R"(["fx_k3_risen"])"));
                                   src->Set("chance", JsonValue::Number(1));
                                   questId = std::string(q.Get("id").AsString());
                                   break;
                                 }
                               }
                             }}},
                           report);
      if (!report.Ok()) MESSAGE(report.Summary());
      REQUIRE(report.Ok());
    }
    REQUIRE_FALSE(questId.empty());
    auto sim = GameSim::Create(*store, SimConfig{});
    REQUIRE(sim != nullptr);
    REQUIRE(sim->NewGame(ClassId::Warrior, Difficulty::Normal, 78));
    sim->Context().sys.story->FinishAllBeats();
    sim->Step();
    SimContext& ctx = sim->Context();
    const TilePos spot = K3QuietSpot(*ctx.sys.zone);
    REQUIRE(spot.col >= 0);
    ctx.sys.locomotion->Teleport(spot.Center(), TeleportReason::Debug);
    REQUIRE(ctx.sys.quests->Accept(questId));
    REQUIRE_FALSE(ctx.data.Pets().pets.empty());
    const std::string petId = ctx.data.Pets().pets.front().id;
    ctx.sys.pets->AddPet(petId, true);
    ctx.sys.pets->SetActivePet(petId);
    REQUIRE(ctx.sys.pets->ActiveId() == petId);
    MonsterDef elite = *ctx.data.FindMonster("fx_k3_risen");
    elite.elite = true;
    REQUIRE(EmbersForKill(ctx.data.Homestead(), true, false, 0) > 0);
    struct Ledger {
      int64_t exp = 0, gold = 0, petExp = 0;
      int32_t embers = 0;
      double hp = 0;
      std::vector<int32_t> quest;
      RngState loot, quests;
    };
    const auto ledger = [&] {
      Ledger l;
      l.exp = ctx.sys.hero->Exp();
      l.gold = ctx.sys.hero->Gold();
      l.petExp = ctx.sys.pets->Find(petId)->exp;
      l.embers = ctx.sys.homestead->Tower().Embers();
      l.hp = ctx.sys.hero->Hp();
      l.quest = ctx.sys.quests->Progress(questId)->objectives;
      l.loot = ctx.Rand(RngStream::Loot).GetState();
      l.quests = ctx.Rand(RngStream::Quests).GetState();
      return l;
    };
    const auto kill = [&](EntityId id) {
      ctx.equip.Ref(Stat::KillHealPercent) = 10;  // an equip rebuild (an achievement) may reset it
      ctx.sys.hero->SetHp(std::floor(ctx.sys.hero->MaxHp() * 0.5));
      const Ledger before = ledger();
      sim->ClearEvents();
      MonsterHitRequest r;
      r.monster = id;
      r.amount = ctx.sys.monsters->Find(id)->hp;
      r.attacker = kHeroEntityId;
      r.source = KillSource::HeroSkill;
      r.provokes = false;
      ctx.sys.combat->DamageMonster(r);
      return before;
    };
    const auto killSpirit = [&] {
      size_t n = 0;
      for (const EvSpiritChanged& e : K3Events<EvSpiritChanged>(sim->Events())) n += e.hasSource && e.source == SpiritSource::Kill;
      return n;
    };
    // The Raised body.
    const EntityId body = ctx.sys.monsters->SummonAt(elite, Vec2(spot.col + 2, spot.row), kNoEntity, 100, "test");
    REQUIRE(body != kNoEntity);
    const auto totalKills = ctx.session.totalKills;
    const Ledger before = kill(body);
    const Ledger after = ledger();
    CHECK(ctx.session.totalKills == totalKills + 1);  // still a kill
    CHECK(after.exp == before.exp);
    CHECK(after.gold == before.gold);
    CHECK(after.petExp == before.petExp);
    CHECK(after.embers == before.embers);
    CHECK(after.hp == before.hp);
    CHECK(after.quest == before.quest);
    CHECK(after.loot == before.loot);
    CHECK(after.quests == before.quests);
    CHECK(killSpirit() == 0);
    CHECK(K3Events<EvEmbersGained>(sim->Events()).empty());
    // The control: the same elite as a regular monster pays each of them.
    MonsterSpawnParams p;
    p.baseDef = &elite;
    p.tile = TilePos{spot.col + 2, spot.row + 1};
    const EntityId regular = ctx.sys.monsters->Spawn(p);
    REQUIRE(regular != kNoEntity);
    const Ledger before2 = kill(regular);
    const Ledger paid = ledger();
    CHECK(paid.exp > before2.exp);
    CHECK(paid.petExp > before2.petExp);
    CHECK(paid.embers == before2.embers + EmbersForKill(ctx.data.Homestead(), true, false, 0));
    CHECK(paid.hp > before2.hp);
    CHECK(paid.quest != before2.quest);
    CHECK_FALSE(paid.loot == before2.loot);
    CHECK_FALSE(paid.quests == before2.quests);
    CHECK(killSpirit() == 1);
  }

  TEST_CASE("K3 runtime: on-death damage follows the difficulty, the resistances, both caps and the (time, schedule) order") {
    // A burst on each difficulty: the damage x the difficulty's monster damage multiplier, and the 20 % cap x it too.
    for (Difficulty diff : {Difficulty::Normal, Difficulty::Nightmare, Difficulty::Hell}) {
      for (double dmg : {9.0, 1000.0}) {
        INFO(EnumName(diff) << " dmg " << dmg);
        K3Rig rig(5, ClassId::Warrior, diff);
        const double mul = rig.h.ctx.data.Combat().difficulty.Def(diff).damageMul;
        rig.PlaceHero(Vec2(51.5, 50));
        const EntityId b = rig.Spawn(K3Bloat(dmg), Vec2(50, 50), MonsterState::Idle);
        const double death = rig.Now();
        rig.Kill(b);
        rig.h.events.Clear();
        rig.StepUntilMs(death + 800);
        const std::vector<EvHit> hits = HeroHits(rig.h.events, b);
        REQUIRE(hits.size() == 1);
        const double cap = std::floor(rig.hero->MaxHp() * 20 * mul / 100.0);
        const double base = std::floor(dmg * mul);
        CHECK(hits[0].amount == (std::min)(AfterResist(rig, base, DamageType::Fire), cap));
        if (dmg > 100) CHECK(hits[0].amount == cap);
        if (dmg < 100) CHECK(hits[0].amount < cap);
      }
    }
    // The hero's fire resistance takes its share of a burst (and of a fire puddle's ticks).
    {
      K3Rig rig;
      rig.h.ctx.equip.Ref(Stat::FireResist) = 40;
      REQUIRE(AfterResist(rig, 18, DamageType::Fire) < 18);
      rig.PlaceHero(Vec2(51.5, 50));
      const EntityId b = rig.Spawn(K3Bloat(18), Vec2(50, 50), MonsterState::Idle);
      const double death = rig.Now();
      rig.Kill(b);
      rig.h.events.Clear();
      rig.StepUntilMs(death + 800);
      const std::vector<EvHit> hits = HeroHits(rig.h.events, b);
      REQUIRE(hits.size() == 1);
      CHECK(hits[0].amount == AfterResist(rig, 18, DamageType::Fire));
    }
    // The monster's own hit cap wins when it is the smaller (10 % under the burst's 20 %).
    {
      K3Rig rig;
      MonsterDef bloat = K3Bloat(1000);
      bloat.hitCapHeroMaxHpPct = 10;
      rig.PlaceHero(Vec2(51.5, 50));
      const EntityId b = rig.Spawn(bloat, Vec2(50, 50), MonsterState::Idle);
      const double death = rig.Now();
      rig.Kill(b);
      rig.h.events.Clear();
      rig.StepUntilMs(death + 800);
      const std::vector<EvHit> hits = HeroHits(rig.h.events, b);
      REQUIRE(hits.size() == 1);
      CHECK(hits[0].amount == std::floor(rig.hero->MaxHp() * 10 / 100.0));
    }
    // A puddle's own cap (capHeroMaxHpPct 15) holds every tick of a huge puddle.
    {
      K3Rig rig;
      MonsterDef ooze = K3Footman("fx_k3_ooze");
      ooze.onDeath.present = true;
      ooze.onDeath.kind = OnDeathKind::Puddle;
      ooze.onDeath.delayMs = 300;
      ooze.onDeath.shape.kind = AbilityShapeKind::Circle;
      ooze.onDeath.shape.radius = 1.5;
      ooze.onDeath.hasDmg = true;
      ooze.onDeath.dmg = 1000;
      ooze.onDeath.hasDamageType = true;
      ooze.onDeath.damageType = DamageType::Poison;
      ooze.onDeath.capHeroMaxHpPct = 15;
      ooze.onDeath.puddle.durationMs = 2000;
      ooze.onDeath.puddle.tickMs = 500;
      rig.PlaceHero(Vec2(51, 50));
      const EntityId o = rig.Spawn(ooze, Vec2(50, 50), MonsterState::Idle);
      const double death = rig.Now();
      rig.Kill(o);
      rig.h.events.Clear();
      rig.StepUntilMs(death + 300 + 2100);
      const std::vector<EvHit> ticks = HeroHits(rig.h.events, o);
      REQUIRE(ticks.size() == 4);
      for (const EvHit& t : ticks) CHECK(t.amount == std::floor(rig.hero->MaxHp() * 15 / 100.0));
      CHECK(rig.hero->Life() == HeroLife::Alive);
    }
    // (time, schedule) order: a burst scheduled first (due at +800) and a ring scheduled 47 steps later with no fuse
    // (due at once) resolve in the same step, the ring first.
    {
      K3Rig rig;
      MonsterDef ring = K3Footman("fx_k3_ringer");
      ring.onDeath.present = true;
      ring.onDeath.kind = OnDeathKind::Ring;
      ring.onDeath.delayMs = 0;
      ring.onDeath.shape.kind = AbilityShapeKind::Circle;
      ring.onDeath.shape.radius = 3;
      ring.onDeath.hasDmg = true;
      ring.onDeath.dmg = 3;
      rig.PlaceHero(Vec2(51, 50));
      const EntityId b = rig.Spawn(K3Bloat(5), Vec2(50, 50), MonsterState::Idle);
      const EntityId r = rig.Spawn(ring, Vec2(50, 51), MonsterState::Idle);
      rig.Silence(r);
      const double death = rig.Now();
      rig.Kill(b);
      rig.StepUntilMs(death + 800 - kSimStepMs);
      rig.Kill(r);
      REQUIRE(rig.combat.PendingOnDeathEffects().size() == 2);
      CHECK(rig.combat.PendingOnDeathEffects()[0].source == b);  // schedule order
      rig.h.events.Clear();
      rig.Step();
      const std::vector<EvHit> hits = HeroHits(rig.h.events);
      REQUIRE(hits.size() == 2);
      CHECK(hits[0].source == r);  // due first
      CHECK(hits[1].source == b);
    }
  }

  TEST_CASE("K3 runtime: rises and collapses: no boss role rises, the affix's rise wins, a sure rise draws nothing") {
    MonsterDef foot = K3Footman("fx_k3_unburied");
    foot.riseAgain.present = true;
    foot.riseAgain.chance = 1;
    foot.riseAgain.delayMs = 3000;
    foot.riseAgain.hpPct = 25;
    // Every boss role stays down (a hunt leader is in the trait case above).
    for (MonsterRole role : {MonsterRole::ZoneMiniBoss, MonsterRole::SubDungeonMiniBoss, MonsterRole::StoryBoss,
                             MonsterRole::SealKeeper, MonsterRole::HuntLeader}) {
      INFO(EnumName(role));
      K3Rig rig;
      const EntityId f = rig.Spawn(foot, Vec2(50, 50), MonsterState::Idle, role);
      rig.Kill(f);
      REQUIRE(rig.Newest() != nullptr);
      CHECK(rig.Newest()->holder == kNoEntity);  // no pending rise holds it
      rig.h.events.Clear();
      rig.StepUntilMs(rig.Now() + 3500);
      CHECK(K3Events<EvMonsterRaised>(rig.h.events).empty());
    }
    // The affix's rise wins over the definition's trait (50 % of its life, not 25 %).
    {
      K3Rig rig;
      MonsterDef both = foot;
      both.elite = true;
      const EntityId f = rig.Spawn(both, Vec2(50, 50), MonsterState::Idle);
      ApplyEliteAffixes(rig.M(f), std::vector<EliteAffixType>{EliteAffixType::Unburied},
                        rig.h.ctx.data.Combat().eliteAffixes, rig.h.ctx.data.Monsters().ai);
      const double maxHp = rig.M(f).maxHp;
      rig.Kill(f);
      rig.h.events.Clear();
      rig.StepUntilMs(rig.Now() + 3000);
      const std::vector<EvMonsterRaised> risen = K3Events<EvMonsterRaised>(rig.h.events);
      REQUIRE(risen.size() == 1);
      CHECK(rig.M(risen[0].entity).hp == std::floor(maxHp * 0.5));
    }
    // A sure rise (chance 1) draws nothing from the Ai stream; a chance below 1 draws once.
    for (double chance : {1.0, 0.5}) {
      INFO(chance);
      K3Rig rig;
      MonsterDef d = foot;
      d.riseAgain.chance = chance;
      const EntityId f = rig.Spawn(d, Vec2(50, 50), MonsterState::Idle);
      Rng copy = rig.h.rng.Get(RngStream::Ai);
      rig.Kill(f);
      if (chance >= 1) {
        CHECK(rig.h.rng.Get(RngStream::Ai).GetState() == copy.GetState());
      } else {
        copy.Float01();
        CHECK(rig.h.rng.Get(RngStream::Ai).GetState() == copy.GetState());
      }
    }
  }

  TEST_CASE("K3 runtime: a collapse clears the fight: its windup, its statuses, the hero's lock") {
    K3Rig rig;
    MonsterAbilityDef slam = K3Ability("slam", AbilityKind::Circle, AbilityAim::Self, "Cast01");
    slam.hasShape = true;
    slam.shape.kind = AbilityShapeKind::Circle;
    slam.shape.radius = 2;
    slam.cooldownMs = 100000;
    const MonsterDef risenCaster = K3Caster({slam}, "fx_k3_risen_caster");
    rig.PlaceHero(Vec2(51.5, 50));
    const EntityId raiser = rig.Spawn(K3Footman("fx_k3_raiser"), Vec2(60, 60), MonsterState::Idle);
    const EntityId raised = rig.monsters.SummonAt(risenCaster, Vec2(50, 50), raiser, 100, "test");
    REQUIRE(raised != kNoEntity);
    rig.ScriptNoDodge(4);
    REQUIRE(rig.StepUntil([&] { return rig.M(raised).pendingAbility == 0; }, 240));
    rig.status.Apply(raised, StatusType::Slow, 30, 600000, kHeroEntityId, rig.Now());
    REQUIRE(rig.status.Has(raised, StatusType::Slow));
    rig.hero->attackTarget = raised;
    rig.h.events.Clear();
    rig.Kill(raiser);
    const std::vector<EvMonsterCollapsed> fell = K3Events<EvMonsterCollapsed>(rig.h.events);
    REQUIRE(fell.size() == 1);
    CHECK(fell[0].entity == raised);
    size_t cancels = 0;
    for (const EvMonsterAttackCancelled& e : K3Events<EvMonsterAttackCancelled>(rig.h.events)) cancels += e.monster == raised;
    CHECK(cancels == 1);
    CHECK(rig.M(raised).pendingAbility == -1);
    CHECK_FALSE(rig.combat.HasPendingStrike(raised));
    CHECK_FALSE(rig.status.Has(raised, StatusType::Slow));
    CHECK(rig.hero->attackTarget == kNoEntity);
    rig.h.events.Clear();
    rig.Step(60);
    CHECK(HeroHits(rig.h.events, raised).empty());  // the slam never lands
  }

  TEST_CASE("K3 runtime: a hero's fire nova / ground circle / cone / line burns the bodies at release; a beast's fire kill") {
    // One area skill per shape from the real data, its damage type turned to fire in memory.
    static std::unique_ptr<DataStore> store;
    static std::vector<K3AreaSkill> skills;
    if (store == nullptr) {
      skills = K3AreaSkills(test::RealData());
      REQUIRE(skills.size() == 4);
      DataLoadReport report;
      store = K3LoadEdited({{"assets.json", K3AddAsset},
                            {"monsters.json", K3AddDefs},
                            {"classes.json",
                             [](JsonValue& doc) {
                               for (JsonValue& cls : doc.FindMutable("classes")->MutableItems()) {
                                 for (JsonValue& sk : cls.FindMutable("skills")->MutableItems()) {
                                   for (const K3AreaSkill& a : skills) {
                                     if (sk.Get("id").AsString() == a.id) sk.Set("damageType", JsonValue::String("fire"));
                                   }
                                 }
                               }
                             }}},
                           report);
      if (!report.Ok()) MESSAGE(report.Summary());
      REQUIRE(report.Ok());
    }
    for (const K3AreaSkill& a : skills) {
      INFO(a.id);
      K3Rig rig(5, a.cls, Difficulty::Normal, store.get());
      Hero& hero = *rig.hero;
      const int32_t idx = hero.Skills().IndexOf(a.id);
      REQUIRE(idx >= 0);
      const SkillDef& s = hero.Skills().Skill(idx);
      REQUIRE(s.damageType == DamageType::Fire);
      hero.Skills().SetLevel(idx, 1);
      hero.RecalcDerived(rig.h.ctx.equip);
      hero.FillHpMana();
      rig.PlaceHero(Vec2(40, 40));
      const Vec2 p = hero.Position();
      const double r = SkillAoeRadius(rig.h.ctx.data.Classes().skillRules, s, 1);
      const Vec2 aim = p + Vec2(3, 0);  // the lock: ground circles land on it, cones and lines point at it
      Vec2 in, out;
      switch (a.shape) {
        case CastShape::Nova: in = p + Vec2(0, r * 0.5); out = p + Vec2(0, r + 1.5); break;
        case CastShape::Ground: in = aim + Vec2(0, r * 0.5); out = aim + Vec2(0, r + 1.5); break;
        case CastShape::Cone: in = p + Vec2(r * 0.6, 0); out = p + Vec2(0, -r * 0.6); break;
        default: in = p + Vec2(s.range * 0.6, 0); out = p + Vec2(s.range * 0.6, r + 2); break;  // Line
      }
      const uint32_t on = rig.CorpseAt(K3Footman(), in);
      const uint32_t off = rig.CorpseAt(K3Footman(), out);
      const EntityId target = rig.Spawn(test::fixture::Brute(), aim, MonsterState::Idle);
      rig.combat.SetAttackTarget(target, false);
      hero.lastAttackMs = 1e18;
      rig.h.events.Clear();
      const double t0 = rig.Now();
      REQUIRE(rig.combat.RequestSkill(idx, SkillAim{}) == SkillRequestResult::Executed);
      rig.StepUntilMs(t0 + 1500);
      CHECK(rig.Corpse(on) == nullptr);
      CHECK(rig.Corpse(off) != nullptr);
      const std::vector<EvCorpseRemoved> burned = Removals(rig.h.events, CorpseRemoveReason::Burned);
      REQUIRE(burned.size() == 1);
      CHECK(burned[0].corpseId == on);
    }
    // A beast whose hits are fire kills a footman: its body registers and burns at once, through the combat path and
    // through PetCompanion::Hit's fallback straight into MonsterSystem (no CombatSystem).
    const std::string firePet = K3FirePet(K3Data());
    REQUIRE_FALSE(firePet.empty());
    for (bool viaCombat : {true, false}) {
      INFO("via combat " << viaCombat);
      K3PetRig rig(firePet);
      REQUIRE(rig.pets.ActiveId() == firePet);
      rig.PlaceHero(Vec2(50, 50));
      const EntityId f = rig.Spawn(K3Footman(), Vec2(52, 50), MonsterState::Idle);
      rig.M(f).hp = 1;
      rig.hero->attackTarget = f;
      rig.hero->lastAttackMs = 1e18;  // the hero never swings: the beast's hit kills
      rig.PlacePet(Vec2(51, 50.5));
      if (!viaCombat) rig.h.ctx.sys.combat = nullptr;
      rig.h.events.Clear();
      for (int s = 0; s < 600 && rig.M(f).IsAlive(); ++s) {
        if (viaCombat) {
          rig.Step();
        } else {
          rig.h.Step(1);
        }
        rig.companion.Tick(kSimStepMs);
      }
      rig.h.ctx.sys.combat = &rig.combat;
      REQUIRE_FALSE(rig.M(f).IsAlive());
      const std::vector<EvCorpseAdded> added = K3Events<EvCorpseAdded>(rig.h.events);
      REQUIRE(added.size() == 1);
      CHECK(added[0].monster == f);
      const std::vector<EvCorpseRemoved> burned = Removals(rig.h.events, CorpseRemoveReason::Burned);
      REQUIRE(burned.size() == 1);
      CHECK(burned[0].corpseId == added[0].corpseId);
    }
  }

  // A seeded fight: a raiser (the opening raise of two, then one every 3 s, cap 4) next to bodies, Unburied footmen
  // (three in four rise), a corpse eater under half life, two bloats (a fire burst that burns bodies, a physical one),
  // the hero going for the raiser first (its Raised collapse) on a fixed loop. The digest of the event log is pinned:
  // GCC and Clang (and every platform) must replay it bit for bit (implementation-plan.md 3 K3 "deterministic replay").
  TEST_CASE("K3 replay: a seeded fight with raises, rises, consumes and bursts replays to the pinned digest (GCC = Clang)") {
    struct Replay {
      uint64_t digest = 0;
      int corpses = 0, raised = 0, selfRises = 0, consumed = 0, burned = 0, expired = 0, bursts = 0, collapses = 0;
    };
    const auto run = [](uint64_t seed) {
      Replay out;
      K3Rig rig(seed);
      MonsterAbilityDef open = K3Raise("pull", 2, 3, 600000);
      open.priority = 5;
      MonsterAbilityDef flock = K3Raise("flock", 1, 4, 3000);
      MonsterDef herd = K3Caster({open, flock}, "fx_k3_herd");
      herd.hp = 150;
      MonsterDef foot = K3Footman("fx_k3_unburied");
      foot.riseAgain.present = true;
      foot.riseAgain.chance = 0.75;
      foot.riseAgain.delayMs = 3000;
      foot.riseAgain.hpPct = 40;
      foot.riseAgain.notOnCrit = true;
      MonsterDef gnawer = K3Gnawer();
      MonsterDef bloat = K3Bloat(9);
      MonsterDef dull = K3Bloat(6);  // a physical burst: its kills leave their bodies
      dull.onDeath.damageType = DamageType::Physical;
      rig.hero->SetHp(rig.hero->MaxHp());
      rig.PlaceHero(Vec2(41.5, 40));
      for (int i = 0; i < 3; ++i) rig.CorpseAt(K3Footman(), Vec2(38.5 + i, 38));  // the dead the herd finds
      rig.CorpseAt(K3Footman(), Vec2(39.5, 42.5));                                 // and one at the eater's feet
      const EntityId raiser = rig.Spawn(herd, Vec2(40, 40), MonsterState::Chase);
      for (int i = 0; i < 4; ++i) rig.Spawn(foot, Vec2(42 + (i % 2), 41 + i / 2), MonsterState::Chase);
      const EntityId eater = rig.Spawn(gnawer, Vec2(39, 42), MonsterState::Chase);
      rig.M(eater).hp = 15;  // under half its life: it eats whatever falls next to it
      rig.Spawn(bloat, Vec2(43, 39), MonsterState::Chase);
      rig.Spawn(dull, Vec2(38, 39), MonsterState::Chase);
      rig.hero->attackTarget = raiser;  // the hero goes for the raiser first (then the nearest aggro monster)
      K3Fnv f;
      const Vec2 loop[] = {Vec2(41.5, 40), Vec2(42, 41.5), Vec2(40.5, 42), Vec2(39, 40.5), Vec2(41, 38.8)};
      for (int s = 0; s < 30 * 60; ++s) {
        if (s % 75 == 0 && rig.hero->Life() == HeroLife::Alive) rig.PlaceHero(loop[(s / 75) % 5]);
        if (rig.hero->Hp() < rig.hero->MaxHp() * 0.5) rig.hero->SetHp(rig.hero->MaxHp());  // keep the fight going
        rig.Step();
        K3DigestEvents(f, rig.h.events);
        out.corpses += static_cast<int>(K3Events<EvCorpseAdded>(rig.h.events).size());
        for (const EvMonsterRaised& r : K3Events<EvMonsterRaised>(rig.h.events)) (r.selfRise ? out.selfRises : out.raised) += 1;
        out.consumed += static_cast<int>(Removals(rig.h.events, CorpseRemoveReason::Consumed).size());
        out.burned += static_cast<int>(Removals(rig.h.events, CorpseRemoveReason::Burned).size());
        out.expired += static_cast<int>(Removals(rig.h.events, CorpseRemoveReason::Expired).size());
        for (const EvTelegraph& t : K3Events<EvTelegraph>(rig.h.events)) out.bursts += t.source == TelegraphSource::OnDeath ? 1 : 0;
        out.collapses += static_cast<int>(K3Events<EvMonsterCollapsed>(rig.h.events).size());
        rig.h.events.Clear();
      }
      f.D(rig.hero->Hp());
      for (const MonsterInstance& m : rig.monsters.All()) {
        f.U64(m.id);
        f.D(m.hp);
        f.V(m.pos);
      }
      for (const CorpseEntry& c : rig.monsters.CorpseRegistry().Entries()) {
        f.U64(c.id);
        f.V(c.pos);
      }
      out.digest = f.h;
      return out;
    };
    const Replay r = run(4242);
    // The fight used every mechanism.
    CHECK(r.corpses > 0);
    CHECK(r.raised > 0);
    CHECK(r.selfRises > 0);
    CHECK(r.consumed > 0);
    CHECK(r.burned > 0);
    CHECK(r.bursts > 0);
    CHECK(r.collapses > 0);
    CHECK(r.digest == run(4242).digest);  // same seed, same log
    CHECK(r.digest != run(4243).digest);  // the seed feeds the rolls
    char hex[17];
    std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(r.digest));
    INFO("digest " << hex);
    CHECK(std::string(hex) == "022a6e97d326a70c");
  }
}
