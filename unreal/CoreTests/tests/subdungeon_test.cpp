// K7 sub-dungeon travel and the cleanse (dark remake, implementation-plan.md 3 "K7", work package 3A): the schema (a
// hand-authored sub-dungeon's map in maps.json `subDungeons`, its `cleansedFlag`, the quest objective `zone`, the
// `clear_area` objective, dormant spawns: loads, loader refusals, DataStore cross checks, the K6 validator's cleanse
// flags), and the runtime: going in through the parent's entrance and back out through the exit tile (the visit, the
// zone_entered beat, the snapshot), nothing respawning and no random event inside, a death returning to the parent's
// camp, the town portal ending on the exit, saves made inside written at the entrance, the clear_area completion (the
// flag, EvAreaCleansed, a cleansed sub-dungeon staying empty), the guide arrow across the two zones, objective zones
// (gather nodes, explore spots, drops), dormant sentinels (formation, wake range, the Rise on waking, the pack alarm),
// what a transition drops (telegraphs, hazards, corpses, the escort) and keeps (the pet), and a seeded replay whose
// event-log digest is pinned.
//
// Synthetic content only: the fx_k7_* sub-dungeon, monsters, NPC, quests, beat and strings are added to the real export
// in memory, and the Chapter 1 zone's first entrance is opened onto the synthetic sub-dungeon (its tile stays, so the
// generated map does not move). No content change moves these tests.
// Suite: world.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "QuestsFixtures.h"
#include "TestContent.h"
#include "TestUtil.h"
#include "abyss/base/Json.h"
#include "abyss/base/StrUtil.h"
#include "abyss/combat/Combat.h"
#include "abyss/combat/SoulEcho.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/pets/PetCompanion.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/quests/QuestGuide.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/quests/QuestWorld.h"
#include "abyss/save/SaveData.h"
#include "abyss/save/SaveIO.h"
#include "abyss/sim/GameSim.h"
#include "abyss/story/StoryDirector.h"
#include "abyss/world/Locomotion.h"
#include "abyss/world/MapGen.h"
#include "abyss/world/RandomEvents.h"
#include "abyss/world/WorldState.h"
#include "abyss/world/Zone.h"
#include "doctest/doctest.h"

using namespace abyss;

namespace {

// ---- synthetic ids --------------------------------------------------------------------------------------------------

constexpr char kSub[] = "sub_fx_k7_crypt";       // the hand-authored sub-dungeon (parent: the Chapter 1 zone)
constexpr char kSubName[] = "data.subDungeon.sub_fx_k7_crypt.name";
constexpr char kFlag[] = "fx_k7_cleansed";       // its cleansedFlag
constexpr char kGrunt[] = "fx_k7_grunt";         // its regulars
constexpr char kSentinel[] = "fx_k7_sentinel";   // its dormant pair (a pack: packAggro 6)
constexpr char kWarden[] = "fx_k7_warden";       // its guardian (sub-dungeon mini-boss)
constexpr char kGiver[] = "fx_k7_giver";         // a camp NPC of the parent with the K7 quests
constexpr char kQuestCleanse[] = "fx_k7_q_cleanse";  // kill the warden (inside), then clear_area
constexpr char kQuestInside[] = "fx_k7_q_inside";    // a gather and an explore objective inside
constexpr char kQuestOutside[] = "fx_k7_q_outside";  // a talk objective in the parent
constexpr char kCutscene[] = "fx_k7_cs";         // the zone_entered beat of the sub-dungeon
constexpr char kCutsceneLine[] = "fx.k7.cs.line";

// The sub-dungeon's grid: 20 x 16, a wall ring, a wall stub at col 10 rows 1-5, floor elsewhere.
constexpr int32_t kCols = 20, kRows = 16;
constexpr TilePos kStart{2, 3};
constexpr TilePos kExit{2, 1};
constexpr TilePos kBoss{16, 3};
constexpr TilePos kGruntAnchor{6, 10};     // 3 grunts, jittered
constexpr TilePos kSentinelAnchor{13, 12}; // 2 sentinels in a line along +col, facing -row
constexpr int32_t kGrunts = 3, kSentinels = 2;
constexpr int32_t kPopulation = kGrunts + kSentinels + 1;
constexpr double kWakeRange = 3;

// ---- data editing ---------------------------------------------------------------------------------------------------

JsonValue K7Parse(std::string_view text) {
  JsonValue v;
  JsonParseError err;
  const bool ok = ParseJson(text, v, &err);
  REQUIRE_MESSAGE(ok, "bad test JSON: " << text);
  return v;
}

JsonValue* K7At(JsonValue& root, std::initializer_list<std::string_view> keys) {
  JsonValue* v = &root;
  for (std::string_view k : keys) {
    v = v->FindMutable(k);
    REQUIRE(v != nullptr);
  }
  return v;
}

using K7Edits = std::vector<std::pair<std::string, std::function<void(JsonValue&)>>>;

// The real export with `edits` applied in memory (several edits of one file run in order on one document).
std::unique_ptr<DataStore> K7LoadEdited(const K7Edits& edits, DataLoadReport& report) {
  std::map<std::string, JsonValue> docs;
  for (const auto& [file, edit] : edits) {
    if (docs.count(file) == 0) docs.emplace(file, K7Parse(test::ReadFile(test::DataDir() + "/" + file)));
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

bool K7HasIssue(const std::vector<DataIssue>& issues, std::string_view pathPart, std::string_view text) {
  for (const DataIssue& e : issues) {
    if (e.path.find(pathPart) != std::string::npos && e.message.find(text) != std::string::npos) return true;
  }
  return false;
}

// The parent zone's first entrance (the Chapter 1 zone ships one, sealed) in the real export.
const SubDungeonEntranceDef& K7RealEntrance() {
  const MapDef* map = test::RealData().FindMap(test::ch1::kZone);
  REQUIRE(map != nullptr);
  REQUIRE_FALSE(map->subDungeonEntrances.empty());
  return map->subDungeonEntrances.front();
}

const ZoneGrid& K7ParentGrid() {
  static const ZoneGrid grid = BuildZoneGrid(test::RealData(), *test::RealData().FindMap(test::ch1::kZone));
  return grid;
}

// The nearest tile to the entrance (rings, row-major inside a ring) that is walkable / not walkable in the real parent
// grid and at least 2 tiles from the entrance: the fixture's return tile.
TilePos K7ReturnTile(bool walkable) {
  const ZoneGrid& g = K7ParentGrid();
  const TilePos e = K7RealEntrance().pos;
  for (int32_t ring = 2; ring < 40; ++ring) {
    for (int32_t dr = -ring; dr <= ring; ++dr) {
      for (int32_t dc = -ring; dc <= ring; ++dc) {
        if ((std::max)(std::abs(dc), std::abs(dr)) != ring) continue;
        const TilePos t{e.col + dc, e.row + dr};
        if (!g.InBounds(t.col, t.row)) continue;
        if (g.Walkable(t.col, t.row) == walkable) return t;
      }
    }
  }
  FAIL("no return tile");
  return e;
}

// What a fixture store is built with.
struct K7Opts {
  bool trigger = false;          // a zone_entered beat for the sub-dungeon
  bool returnOnWall = false;     // the return tile is not walkable in the parent
  bool dormantRise = true;       // the sentinels' `rise`
  bool wardenAbility = false;    // the warden slams (a K2 circle with a long windup)
  int32_t maxRemaining = 2;      // the clear_area threshold
  auto Key() const { return std::tie(trigger, returnOnWall, dormantRise, wardenAbility, maxRemaining); }
};

std::vector<std::string> K7TileRows() {
  std::vector<std::string> rows;
  for (int32_t r = 0; r < kRows; ++r) {
    std::string row;
    for (int32_t c = 0; c < kCols; ++c) {
      const bool ring = r == 0 || c == 0 || r == kRows - 1 || c == kCols - 1;
      const bool stub = c == 10 && r >= 1 && r <= 5;
      row.push_back(ring || stub ? '4' : '1');
    }
    rows.push_back(std::move(row));
  }
  return rows;
}

JsonValue K7Rows(const std::vector<std::string>& rows) {
  JsonValue a = JsonValue::Array();
  for (const std::string& r : rows) a.Append(JsonValue::String(r));
  return a;
}

std::vector<std::string> K7CollisionRows() {
  std::vector<std::string> rows = K7TileRows();
  for (std::string& r : rows) {
    for (char& ch : r) ch = ch == '4' ? '0' : '1';
  }
  return rows;
}

// The sub-dungeon record in the exporter's format (maps.json subDungeons.<id>): the web fields, then the K7 ones.
JsonValue K7SubDungeonJson(const K7Opts& o) {
  const TilePos back = K7ReturnTile(!o.returnOnWall);
  JsonValue sd = K7Parse(StrCat(
      R"({"id": ")", kSub, R"(", "name": "fx crypt", "parentZone": ")", test::ch1::kZone, R"(", "cols": )", kCols,
      R"(, "rows": )", kRows, R"(, "theme": "plains", "seed": 7, "spawns": [
        {"col": )", kGruntAnchor.col, R"(, "row": )", kGruntAnchor.row, R"(, "monsterId": ")", kGrunt, R"(", "count": )",
      kGrunts, R"(},
        {"col": )", kSentinelAnchor.col, R"(, "row": )", kSentinelAnchor.row, R"(, "monsterId": ")", kSentinel,
      R"(", "count": )", kSentinels, R"(, "dormant": {"wakeRange": )", kWakeRange, R"(, "rise": )",
      o.dormantRise ? "true" : "false", R"(, "line": {"dCol": 1, "dRow": 0}, "faceDeg": 270}}],
      "miniBoss": {"col": )", kBoss.col, R"(, "row": )", kBoss.row, R"(, "monsterId": ")", kWarden, R"("},
      "playerStart": {"col": )", kStart.col, R"(, "row": )", kStart.row, R"(},
      "exit": {"col": )", kExit.col, R"(, "row": )", kExit.row, R"(, "returnCol": )", back.col, R"(, "returnRow": )",
      back.row, R"(}, "levelRange": [3, 4], "bgColor": "#101014", "presentationTheme": "march", "cleansedFlag": ")",
      kFlag, R"("})"));
  sd.Set("tiles", K7Rows(K7TileRows()));
  sd.Set("collisions", K7Rows(K7CollisionRows()));
  sd.Set("decorations", JsonValue::Array());
  return sd;
}

// maps.json: the sub-dungeon, and the parent's first entrance opened onto it.
std::function<void(JsonValue&)> K7EditMaps(K7Opts o) {
  return [o](JsonValue& doc) {
    K7At(doc, {"subDungeons"})->Set(kSub, K7SubDungeonJson(o));
    JsonValue* entrances = K7At(doc, {"maps", test::ch1::kZone, "subDungeonEntrances"});
    REQUIRE(entrances->Size() > 0);
    JsonValue& e = entrances->MutableItems()[0];
    e.Set("targetSubDungeon", JsonValue::String(kSub));
    e.Remove("sealed");
    e.Remove("sealedTextKey");
    // The parent's first camp also hosts the K7 giver (camp NPC lists are not map-generator inputs).
    JsonValue* camps = K7At(doc, {"maps", test::ch1::kZone, "camps"});
    REQUIRE(camps->Size() > 0);
    camps->MutableItems()[0].FindMutable("npcs")->Append(JsonValue::String(kGiver));
  };
}

// A monsters.json definition with its exporter `derived` block; `extra` = more top-level members.
std::string K7DefJson(std::string_view id, int hp, int damage, double aggro, std::string_view extra = "",
                      std::string_view source = "zone") {
  return StrCat(R"({"id": ")", id, R"(", "name": ")", id, R"(", "level": 4, "hp": )", hp, R"(, "damage": )", damage,
                R"(, "defense": 2, "speed": 50, "aggroRange": )", aggro,
                R"(, "attackRange": 1.5, "attackSpeed": 1500, "expReward": 10, "goldReward": [1, 2],
    "spriteKey": "fx_k7_art", "animCategory": "humanoid", "family": "fx_k7_dead")",
                extra.empty() ? "" : ", ", extra,
                R"(, "derived": {"isRanged": false, "onHitStatus": [], "projectileColor": 13386956, "homeZone": null,
    "nameKey": "data.monster.)",
                id, R"(", "sources": [")", source, R"("]}})");
}

// A slow circle slam around the warden: a telegraph that outlasts a zone change.
constexpr char kWardenSlam[] = R"("abilities": [{"id": "fx_k7_slam", "kind": "circle", "windupMs": 3000,
    "cooldownMs": 4000, "initialCooldownMs": 0, "rangeMin": 0, "rangeMax": 30, "priority": 5, "aim": "self",
    "shape": {"radius": 4}, "dmgMul": 1, "clip": "Attack02"}])";

std::function<void(JsonValue&)> K7EditMonsters(K7Opts o) {
  return [o](JsonValue& doc) {
    JsonValue* defs = K7At(doc, {"defs"});
    defs->Set(kGrunt, K7Parse(K7DefJson(kGrunt, 60, 1, 3)));
    defs->Set(kSentinel, K7Parse(K7DefJson(kSentinel, 80, 1, 6, R"("packAggro": 6)")));
    defs->Set(kWarden, K7Parse(K7DefJson(
                           kWarden, 300, 2, 4,
                           StrCat(R"("elite": true, "isMiniBoss": true, "isSubDungeonMiniBoss": true)",
                                  o.wardenAbility ? ", " : "", o.wardenAbility ? kWardenSlam : ""),
                           "subDungeonBoss")));
  };
}

// A quests.json quest of the parent zone given by kGiver.
std::string K7QuestJson(std::string_view id, std::string_view objectives, std::string_view category = "side") {
  return StrCat(R"({"id": ")", id, R"(", "name": ")", id, R"(", "zone": ")", test::ch1::kZone,
                R"(", "type": "kill", "category": ")", category, R"(", "objectives": )", objectives,
                R"(, "rewards": {"exp": 0, "gold": 1}, "level": 1, "derived": {"giverNpcId": ")", kGiver,
                R"(", "objectiveItemKinds": [], "embersOnTurnIn": 0, "i18n": {"name": "data.quest.)", id,
                R"(.name", "desc": "data.quest.)", id, R"(.desc", "offer": "data.quest.)", id,
                R"(.offer", "complete": "data.quest.)", id, R"(.complete"}}})");
}

std::string K7CleanseObjectives(int32_t maxRemaining) {
  return StrCat(R"([{"type": "kill", "targetId": ")", kWarden, R"(", "targetName": "fx warden", "required": 1,
      "current": 0, "zone": ")", kSub, R"("},
     {"type": "clear_area", "targetId": ")", kSub, R"(", "targetName": "fx crypt", "required": 1, "current": 0,
      "maxRemaining": )", maxRemaining, R"(, "after": 0}])");
}

// A gather objective (3 nodes around (5,12), 2 needed) and an explore spot at (17,13), both inside the sub-dungeon.
std::string K7InsideObjectives() {
  return StrCat(R"([{"type": "collect", "targetId": "fx_k7_relic", "targetName": "fx relic", "required": 2,
      "current": 0, "source": {"kind": "gather", "area": {"col": 5, "row": 12, "radius": 2}, "count": 3},
      "zone": ")", kSub, R"("},
     {"type": "explore", "targetId": "fx_k7_spot", "targetName": "fx spot", "required": 1, "current": 0,
      "location": {"col": 17, "row": 13, "radius": 1.5}, "zone": ")", kSub, R"("}])");
}

std::string K7OutsideObjectives() {
  return StrCat(R"([{"type": "talk", "targetId": ")", kGiver, R"(", "targetName": "fx", "required": 1, "current": 0}])");
}

std::function<void(JsonValue&)> K7EditQuests(K7Opts o) {
  return [o](JsonValue& doc) {
    JsonValue* quests = K7At(doc, {"quests"});
    quests->Append(K7Parse(K7QuestJson(kQuestCleanse, K7CleanseObjectives(o.maxRemaining))));
    quests->Append(K7Parse(K7QuestJson(kQuestInside, K7InsideObjectives())));
    quests->Append(K7Parse(K7QuestJson(kQuestOutside, K7OutsideObjectives())));
  };
}

void K7EditNpcs(JsonValue& doc) {
  K7At(doc, {"npcs"})->Set(
      kGiver, K7Parse(StrCat(R"({"id": ")", kGiver, R"(", "name": "fx giver", "type": "quest", "dialogue": ["fx line"],
      "quests": [")", kQuestCleanse, R"(", ")", kQuestInside, R"(", ")", kQuestOutside,
                             R"("], "dialogueTreeId": null, "nameKey": "data.npc.)", kGiver, R"(.name"})")));
}

void K7EditStrings(JsonValue& doc) {
  JsonValue* s = K7At(doc, {"strings"});
  for (const char* k : {kSubName, kCutsceneLine}) s->Set(k, JsonValue::String(k));
}

// story.json: a one-line beat when the sub-dungeon is entered.
void K7EditStory(JsonValue& doc) {
  K7At(doc, {"cutscenes"})->Set(kCutscene, K7Parse(StrCat(R"({"id": ")", kCutscene, R"(", "steps": [{"kind": "narrate",
      "text": ")", kCutsceneLine, R"("}]})")));
  K7At(doc, {"triggers"})->Append(K7Parse(StrCat(R"({"on": "zone_entered", "zoneId": ")", kSub, R"(", "cutscene": ")",
                                                 kCutscene, R"("})")));
}

K7Edits K7BaseEdits(K7Opts o = K7Opts{}) {
  K7Edits e = {{"i18n_zh-CN.json", K7EditStrings}, {"i18n_en.json", K7EditStrings}, {"monsters.json", K7EditMonsters(o)},
               {"quests.json", K7EditQuests(o)},   {"npcs.json", K7EditNpcs},       {"maps.json", K7EditMaps(o)}};
  if (o.trigger) e.emplace_back("story.json", K7EditStory);
  return e;
}

const DataStore& K7Data(K7Opts o = K7Opts{}) {
  static std::map<std::tuple<bool, bool, bool, bool, int32_t>, std::unique_ptr<DataStore>> stores;
  std::unique_ptr<DataStore>& store = stores[o.Key()];
  if (store == nullptr) {
    DataLoadReport report;
    store = K7LoadEdited(K7BaseEdits(o), report);
    INFO(report.Summary());
    REQUIRE(report.Ok());
  }
  return *store;
}

// One more edit on top of the base: the load report.
DataLoadReport K7BadLoad(const std::string& file, std::function<void(JsonValue&)> edit) {
  DataLoadReport report;
  K7Edits e = K7BaseEdits();
  e.emplace_back(file, std::move(edit));
  K7LoadEdited(e, report);
  return report;
}

// An edit of the fixture sub-dungeon's record.
std::function<void(JsonValue&)> K7EditSub(std::function<void(JsonValue&)> edit) {
  return [edit](JsonValue& doc) { edit(*K7At(doc, {"subDungeons", kSub})); };
}

// An edit of objective `index` of quest `id`.
std::function<void(JsonValue&)> K7EditObjective(std::string id, size_t index, std::function<void(JsonValue&)> edit) {
  return [id, index, edit](JsonValue& doc) {
    for (JsonValue& q : K7At(doc, {"quests"})->MutableItems()) {
      const JsonValue* qid = q.Find("id");
      if (qid == nullptr || qid->AsString() != id) continue;
      JsonValue* objectives = q.FindMutable("objectives");
      REQUIRE((objectives != nullptr && index < objectives->Size()));
      edit(objectives->MutableItems()[index]);
      return;
    }
    FAIL("no quest " << id);
  };
}

// ---- sims -----------------------------------------------------------------------------------------------------------

template <class E>
std::vector<E> K7Of(std::span<const Event> events) {
  std::vector<E> out;
  for (const Event& e : events) {
    if (const E* x = std::get_if<E>(&e)) out.push_back(*x);
  }
  return out;
}

template <class E>
int32_t K7IndexOf(std::span<const Event> events, const std::function<bool(const E&)>& pred = {}) {
  for (size_t i = 0; i < events.size(); ++i) {
    const E* x = std::get_if<E>(&events[i]);
    if (x != nullptr && (!pred || pred(*x))) return static_cast<int32_t>(i);
  }
  return -1;
}

bool K7HasLog(std::span<const Event> events, std::string_view key) {
  for (const EvLog& l : K7Of<EvLog>(events)) {
    if (l.text.key == key) return true;
  }
  return false;
}

// A running game on a K7 store. Step() keeps every event of the steps since the last Clear() in `log`.
struct K7Sim {
  explicit K7Sim(const DataStore& d, uint64_t seed = 11, ClassId cls = ClassId::Warrior) : data(d) {
    sim = GameSim::Create(d, SimConfig{});
    REQUIRE(sim != nullptr);
    REQUIRE(sim->NewGame(cls, Difficulty::Normal, seed));
    Bind();
    ctx->sys.story->FinishAllBeats();
    Step();
    Clear();
  }
  void Bind() { ctx = &sim->Context(); }

  // Step() clears the sim's buffer at its start, so what a direct system call (Interact(), Kill(), ...) emitted between
  // two steps is taken first.
  void Flush() {
    const std::span<const Event> ev = sim->Events();
    for (size_t i = taken; i < ev.size(); ++i) log.push_back(ev[i]);
    taken = ev.size();
  }
  void Step(int n = 1) {
    for (int i = 0; i < n; ++i) {
      Flush();
      sim->Step();
      taken = 0;
      Flush();
    }
  }
  void StepMs(double ms) { Step(static_cast<int>(std::ceil(ms / kSimStepMs))); }
  void Clear() {
    log.clear();
    sim->ClearEvents();
    taken = 0;
  }
  std::span<const Event> Log() {
    Flush();
    return log;
  }

  const Snapshot& View() const { return sim->View(); }
  const std::string& Zone() const { return ctx->session.currentMap; }
  Hero& TheHero() { return *ctx->sys.hero; }
  MonsterSystem& Monsters() { return *ctx->sys.monsters; }
  QuestSystem& Quests() { return *ctx->sys.quests; }
  ZoneRuntime& TheZone() { return *ctx->sys.zone; }

  // Puts the hero on a tile (no path, no zone change).
  void Place(Vec2 pos) { ctx->sys.locomotion->Teleport(pos, TeleportReason::Debug); }

  // Steps until the live zone is `id` (the fade, then the exit + entry sequences); fails after 3 s.
  void AwaitZone(std::string_view id) {
    for (int i = 0; i < 180 && Zone() != id; ++i) Step();
    REQUIRE(Zone() == id);
    REQUIRE_FALSE(ctx->session.transitioning);
  }

  // The parent's entrance onto the sub-dungeon.
  const SubDungeonEntranceDef& Entrance() const {
    const MapDef* map = data.FindMap(test::ch1::kZone);
    REQUIRE(map != nullptr);
    return map->subDungeonEntrances.front();
  }

  // Stands the hero beside the entrance and uses the interact action; steps into the sub-dungeon.
  void GoIn() {
    REQUIRE(Zone() == test::ch1::kZone);
    Place(Entrance().pos.Center() + Vec2(1, 0));
    Step();
    REQUIRE(TheZone().Interact());
    AwaitZone(kSub);
  }

  // Walks away from the exit (arming it), then stands on it; steps back into the parent.
  void GoOut() {
    REQUIRE(Zone() == kSub);
    Place(Vec2(6, 6));
    Step();
    Place(kExit.Center());
    AwaitZone(test::ch1::kZone);
  }

  std::vector<const MonsterInstance*> Alive(std::string_view defId = {}) {
    std::vector<const MonsterInstance*> out;
    for (const MonsterInstance& m : Monsters().All()) {
      if (m.IsAlive() && (defId.empty() || m.def.id == defId)) out.push_back(&m);
    }
    return out;
  }

  // Kills a monster with one huge hero hit (the kill pipeline runs).
  void Kill(EntityId id) {
    DamageFlags f;
    f.source = KillSource::HeroBasic;
    Monsters().ApplyDamage(id, 1e9, f);
  }
  void KillAll(std::string_view defId, size_t keep = 0) {
    std::vector<EntityId> ids;
    for (const MonsterInstance* m : Alive(defId)) ids.push_back(m->id);
    for (size_t i = keep; i < ids.size(); ++i) Kill(ids[i]);
  }

  const DataStore& data;
  std::unique_ptr<GameSim> sim;
  SimContext* ctx = nullptr;
  std::vector<Event> log;
  size_t taken = 0;  // how many of sim->Events() are in `log` already
};

// Whether a respawn timer is pending for `id`.
bool K7RespawnQueued(const K7Sim& s, EntityId id) {
  for (const Timer& t : s.ctx->timers.Pending()) {
    if (t.owner == TimerOwner::Monsters && t.kind == static_cast<uint16_t>(MonsterTimerKind::Respawn) && t.entity == id) {
      return true;
    }
  }
  return false;
}

int32_t K7Current(const QuestSystem& qs, std::string_view quest, size_t objective) {
  const QuestProgress* p = qs.Progress(quest);
  return p != nullptr && objective < p->objectives.size() ? p->objectives[objective] : -1;
}

const WorldMarkerView* K7Marker(const Snapshot& v, MarkerKind kind) {
  for (const WorldMarkerView& m : v.markers) {
    if (m.kind == kind) return &m;
  }
  return nullptr;
}

const MonsterView* K7View(const Snapshot& v, EntityId id) {
  for (const MonsterView& m : v.monsters) {
    if (m.id == id) return &m;
  }
  return nullptr;
}

// ---- the event-log digest (FNV-1a over the fields a K7 run produces) ------------------------------------------------

struct K7Fnv {
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
    for (int i = 0; i < 8; ++i) b[i] = static_cast<unsigned char>(v >> (8 * i));
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

void K7Digest(K7Fnv& f, std::span<const Event> events) {
  for (const Event& e : events) {
    f.U64(e.index());
    if (const EvEntitySpawned* s = std::get_if<EvEntitySpawned>(&e)) {
      f.U64(s->id);
      f.U64(static_cast<uint64_t>(s->kind));
      f.S(s->defId);
      f.V(s->pos);
    } else if (const EvEntityDespawned* d = std::get_if<EvEntityDespawned>(&e)) {
      f.U64(d->id);
      f.U64(static_cast<uint64_t>(d->reason));
    } else if (const EvZone* z = std::get_if<EvZone>(&e)) {
      f.U64(static_cast<uint64_t>(z->phase));
      f.S(z->mapId);
      f.U64(static_cast<uint64_t>(static_cast<int64_t>(z->target.col)));
      f.U64(static_cast<uint64_t>(static_cast<int64_t>(z->target.row)));
    } else if (const EvWorldFlag* w = std::get_if<EvWorldFlag>(&e)) {
      f.S(w->flag);
      f.S(w->questId);
    } else if (const EvAreaCleansed* c = std::get_if<EvAreaCleansed>(&e)) {
      f.S(c->zoneId);
      f.S(c->questId);
      f.U64(static_cast<uint64_t>(c->remaining));
      f.V(c->pos);
    } else if (const EvQuestUpdate* q = std::get_if<EvQuestUpdate>(&e)) {
      f.U64(static_cast<uint64_t>(q->kind));
      f.S(q->questId);
      f.U64(static_cast<uint64_t>(static_cast<int64_t>(q->objectiveIndex)));
    } else if (const EvPlayAnim* a = std::get_if<EvPlayAnim>(&e)) {
      f.U64(a->entity);
      f.U64(static_cast<uint64_t>(a->action));
      f.D(a->startMs);
    } else if (const EvLog* l = std::get_if<EvLog>(&e)) {
      f.S(l->text.key);
    }
  }
}

}  // namespace

TEST_SUITE("world") {
  // ===================================================================================================================
  // data
  // ===================================================================================================================

  TEST_CASE("K7 data: a hand-authored sub-dungeon loads as a zone; objective zones, clear_area and dormant spawns load") {
    const DataStore& d = K7Data();
    const WorldTables& w = d.World();
    // The sub-dungeon is a zone: FindMap finds its map, which is not one of the maps proper.
    const SubDungeonDef* sd = w.FindSubDungeon(kSub);
    REQUIRE(sd != nullptr);
    CHECK(sd->hasMap);
    CHECK(w.FindEnterableSubDungeon(kSub) == sd);
    CHECK(d.FindMap(kSub) == &sd->map);
    CHECK(w.FindMapProper(kSub) == nullptr);
    CHECK(w.maps.size() == test::RealData().World().maps.size());
    CHECK(sd->nameKey == kSubName);
    CHECK(sd->cleansedFlag == kFlag);
    CHECK(sd->parentZone == test::ch1::kZone);
    const MapDef& m = sd->map;
    CHECK(m.id == kSub);
    CHECK(m.nameKey == kSubName);
    CHECK(m.cols == kCols);
    CHECK(m.rows == kRows);
    CHECK_FALSE(m.generated);
    CHECK(m.camps.empty());
    CHECK(m.orderIndex == -1);
    CHECK(m.playerStart == kStart);
    CHECK(m.levelMin == 3);
    CHECK(m.levelMax == 4);
    CHECK(m.hasPresentationTheme);
    CHECK(m.presentationTheme == MapTheme::March);
    REQUIRE(m.exits.size() == 1);
    CHECK(m.exits[0].pos == kExit);
    CHECK(m.exits[0].targetMap == test::ch1::kZone);
    CHECK(m.exits[0].target == K7ReturnTile(true));
    CHECK(m.exits[0].target == sd->returnPos);
    REQUIRE(m.tiles.size() == static_cast<size_t>(kRows));
    REQUIRE(m.collisions.size() == static_cast<size_t>(kRows));
    // Its grid is the authored one (no generator): the ring and the stub are walls, the rest is floor.
    const ZoneGrid g = BuildZoneGrid(d, m);
    CHECK(g.Cols() == kCols);
    CHECK(g.Rows() == kRows);
    CHECK_FALSE(g.Walkable(0, 0));
    CHECK_FALSE(g.Walkable(10, 3));
    CHECK(g.Tile(10, 3) == TileType::Wall);
    CHECK(g.Walkable(10, 6));
    CHECK(g.Tile(5, 5) == TileType::Dirt);
    CHECK(g.Walkable(kStart.col, kStart.row));
    CHECK(g.Walkable(kExit.col, kExit.row));
    // Its spawns carry the zone spawn fields: the dormant block with its line and facing.
    REQUIRE(m.spawns.size() == 2);
    CHECK_FALSE(m.spawns[0].dormant.present);
    const MapSpawnDormantDef& dm = m.spawns[1].dormant;
    CHECK(dm.present);
    CHECK(dm.wakeRange == kWakeRange);
    CHECK(dm.rise);
    CHECK(dm.hasLine);
    CHECK(dm.lineStep == TilePos{1, 0});
    CHECK(dm.hasFace);
    CHECK(dm.faceDeg == 270);
    // The web's generated sub-dungeons are no zones.
    int32_t generated = 0;
    for (const SubDungeonDef& s : w.subDungeons) {
      if (s.id == kSub) continue;
      ++generated;
      CAPTURE(s.id);
      CHECK_FALSE(s.hasMap);
      CHECK(d.FindMap(s.id) == nullptr);
      CHECK(w.FindEnterableSubDungeon(s.id) == nullptr);
      CHECK(s.nameKey == "data.subDungeon." + s.id + ".name");
    }
    CHECK(generated == static_cast<int32_t>(test::RealData().World().subDungeons.size()));
    // The parent's entrance leads in.
    const MapDef* parent = d.FindMap(test::ch1::kZone);
    REQUIRE(parent != nullptr);
    CHECK_FALSE(parent->subDungeonEntrances.front().sealed);
    CHECK(parent->subDungeonEntrances.front().targetSubDungeon == kSub);
    // The mood themes of the Pit parse (MapTheme, appended).
    MapTheme t{};
    CHECK(ParseEnum("pit", t));
    CHECK(t == MapTheme::Pit);
    CHECK(ParseEnum("pit_cleansed", t));
    CHECK(t == MapTheme::PitCleansed);
    CHECK(EnumName(MapTheme::BreachRim) == "breach_rim");
    // Quests: the objective's zone, the clear_area's fields and its derived flag.
    const QuestDef* q = d.FindQuest(kQuestCleanse);
    REQUIRE(q != nullptr);
    REQUIRE(q->objectives.size() == 2);
    CHECK(q->objectives[0].type == ObjectiveType::Kill);
    CHECK(q->objectives[0].zone == kSub);
    CHECK(q->objectives[0].cleansedFlag.empty());
    const QuestObjectiveDef& clear = q->objectives[1];
    CHECK(clear.type == ObjectiveType::ClearArea);
    CHECK(EnumName(ObjectiveType::ClearArea) == "clear_area");
    CHECK(clear.targetId == kSub);
    CHECK(clear.zone == kSub);  // the loader's: a clear_area's zone is its target
    CHECK(clear.maxRemaining == 2);
    CHECK(clear.after == 0);
    CHECK(clear.required == 1);
    CHECK(clear.cleansedFlag == kFlag);
    CHECK(ObjectiveZone(*q, q->objectives[0]) == kSub);
    const QuestDef* outside = d.FindQuest(kQuestOutside);
    REQUIRE(outside != nullptr);
    CHECK(outside->objectives[0].zone.empty());
    CHECK(outside->objectives[0].after == -1);
    CHECK(ObjectiveZone(*outside, outside->objectives[0]) == test::ch1::kZone);
    // The cleanser sets the flag (the K6 flag checks see it).
    const std::vector<const QuestDef*> setters = QuestsSettingFlag(d.Quests(), kFlag);
    REQUIRE(setters.size() == 1);
    CHECK(setters[0]->id == kQuestCleanse);
    // The shipped export has no hand-authored sub-dungeon yet and no objective with a zone (3B authors the Pit).
    CHECK(ObjectiveZone(*q, clear) == kSub);
  }

  TEST_CASE("K7 data: the loader and Finalize refuse broken sub-dungeons, entrances and dormant spawns") {
    struct Bad {
      const char* name;
      std::function<void(JsonValue&)> edit;
      const char* path;
      const char* text;
    };
    const auto setTile = [](JsonValue& sd, const char* key, TilePos p) {
      JsonValue* t = sd.FindMutable(key);
      REQUIRE(t != nullptr);
      t->Set("col", JsonValue::Integer(p.col));
      t->Set("row", JsonValue::Integer(p.row));
    };
    const auto dormant = [](std::string json) {
      return K7EditSub([json](JsonValue& sd) {
        sd.FindMutable("spawns")->MutableItems()[1].Set("dormant", K7Parse(json));
      });
    };
    const std::string otherZone = test::RealData().World().subDungeons.front().parentZone;
    const std::vector<Bad> cases = {
        {"tile rows short", K7EditSub([](JsonValue& sd) {
           std::vector<std::string> rows = K7TileRows();
           rows.pop_back();
           sd.Set("tiles", K7Rows(rows));
         }),
         "subDungeons.sub_fx_k7_crypt.tiles", "rows differ"},
        {"tile row narrow", K7EditSub([](JsonValue& sd) {
           std::vector<std::string> rows = K7TileRows();
           rows[3].pop_back();
           sd.Set("tiles", K7Rows(rows));
         }),
         "subDungeons.sub_fx_k7_crypt.tiles", "row width differs"},
        {"collision row narrow", K7EditSub([](JsonValue& sd) {
           std::vector<std::string> rows = K7CollisionRows();
           rows[3].pop_back();
           sd.Set("collisions", K7Rows(rows));
         }),
         "subDungeons.sub_fx_k7_crypt.collisions", "row width differs"},
        {"empty tiles", K7EditSub([](JsonValue& sd) { sd.Set("tiles", JsonValue::Array()); }),
         "subDungeons.sub_fx_k7_crypt.tiles", "needs its tile rows"},
        {"start on a wall", K7EditSub([setTile](JsonValue& sd) { setTile(sd, "playerStart", TilePos{10, 3}); }),
         "subDungeons.sub_fx_k7_crypt.playerStart", "is not walkable"},
        {"boss on a wall", K7EditSub([setTile](JsonValue& sd) { setTile(sd, "miniBoss", TilePos{0, 0}); }),
         "subDungeons.sub_fx_k7_crypt.miniBoss", "is not walkable"},
        {"exit on a wall", K7EditSub([setTile](JsonValue& sd) { setTile(sd, "exit", TilePos{10, 2}); }),
         "subDungeons.sub_fx_k7_crypt.exit", "is not walkable"},
        {"exit on the start", K7EditSub([setTile](JsonValue& sd) { setTile(sd, "exit", kStart); }),
         "subDungeons.sub_fx_k7_crypt.exit", "stands on the player start"},
        {"spawn on a wall", K7EditSub([](JsonValue& sd) {
           sd.FindMutable("spawns")->MutableItems()[0].Set("col", JsonValue::Integer(10));
           sd.FindMutable("spawns")->MutableItems()[0].Set("row", JsonValue::Integer(4));
         }),
         "subDungeons.sub_fx_k7_crypt.spawns[0]", "is not walkable"},
        {"return tile off the parent", K7EditSub([](JsonValue& sd) {
           sd.FindMutable("exit")->Set("returnCol", JsonValue::Integer(9999));
         }),
         "subDungeons.sub_fx_k7_crypt.exits[0]", "outside"},
        {"parent is a sub-dungeon", K7EditSub([](JsonValue& sd) {
           sd.Set("parentZone", JsonValue::String(test::RealData().World().subDungeons.front().id));
         }),
         "subDungeons.sub_fx_k7_crypt", "unknown parent zone"},
        {"another zone's sub-dungeon", K7EditSub([otherZone](JsonValue& sd) {
           sd.Set("parentZone", JsonValue::String(otherZone));
         }),
         "subDungeonEntrances", "a sub-dungeon of '"},
        {"empty flag", K7EditSub([](JsonValue& sd) { sd.Set("cleansedFlag", JsonValue::String("")); }),
         "subDungeons.sub_fx_k7_crypt.cleansedFlag", "empty flag id"},
        {"unknown mood", K7EditSub([](JsonValue& sd) { sd.Set("presentationTheme", JsonValue::String("pit")); }),
         "subDungeons.sub_fx_k7_crypt.presentationTheme", "no zone_moods.json mood 'pit'"},
        {"wake range 0", dormant(R"({"wakeRange": 0})"), "dormant.wakeRange", "expected > 0"},
        {"wake range too far", dormant(R"({"wakeRange": 31})"), "dormant.wakeRange", "expected > 0"},
        {"unknown dormant field", dormant(R"({"wakeRange": 3, "sleep": true})"), "dormant.sleep",
         "unknown spawn dormant field"},
        {"line without a step", dormant(R"({"wakeRange": 3, "line": {"dCol": 0, "dRow": 0}})"), "dormant.line",
         "needs a step"},
        {"line step too long", dormant(R"({"wakeRange": 3, "line": {"dCol": 9, "dRow": 0}})"), "dormant.line.dCol",
         "expected -8..8"},
        {"line off the grid", dormant(R"({"wakeRange": 3, "line": {"dCol": 8, "dRow": 0}})"), "dormant.line",
         "stands outside the grid"},
        {"facing out of range", dormant(R"({"wakeRange": 3, "faceDeg": 400})"), "dormant.faceDeg", "expected -360..360"},
        // 3A review R5: every member of a line stands on a walkable tile of the authored grid (the second sentinel on
        // the bottom wall), counted over the most members the entry ever spawns (a variant's seventh on the east wall,
        // its eighth off the grid).
        {"line member on a wall", dormant(R"({"wakeRange": 3, "line": {"dCol": 0, "dRow": 3}})"),
         "spawns[1].dormant.line", "member 1 stands on (13,15), which is not walkable"},
        {"variant member on a wall", K7EditSub([](JsonValue& sd) {
           sd.FindMutable("spawns")->MutableItems()[1].Set(
               "variants", K7Parse(StrCat(R"([{"whenFlag": ")", kFlag, R"(", "monsterId": ")", kSentinel,
                                          R"(", "count": 7}])")));
         }),
         "spawns[1].dormant.line", "member 6 stands on (19,12), which is not walkable"},
        {"variant member off the grid", K7EditSub([](JsonValue& sd) {
           sd.FindMutable("spawns")->MutableItems()[1].Set(
               "variants", K7Parse(StrCat(R"([{"whenFlag": ")", kFlag, R"(", "monsterId": ")", kSentinel,
                                          R"(", "count": 8}])")));
         }),
         "spawns[1].dormant.line", "(20,12) stands outside the grid"},
    };
    for (const Bad& b : cases) {
      CAPTURE(b.name);
      const DataLoadReport r = K7BadLoad("maps.json", b.edit);
      INFO(r.Summary());
      CHECK_FALSE(r.Ok());
      CHECK(K7HasIssue(r.errors, b.path, b.text));
    }
    // 3A review R5: a sub-dungeon is no zone proper - it is not the default map, not on the world map's order, not an
    // exit's target (it is entered through an entrance of its parent) and has no random-event row (none rolls inside).
    {
      const std::vector<Bad> proper = {
          {"default map", [](JsonValue& doc) { doc.Set("defaultMap", JsonValue::String(kSub)); }, "defaultMap",
           "'sub_fx_k7_crypt' is a sub-dungeon, not a map"},
          {"world map order", [](JsonValue& doc) { K7At(doc, {"order"})->Append(JsonValue::String(kSub)); }, "order",
           "'sub_fx_k7_crypt' is a sub-dungeon, not a map"},
          {"exit target", [](JsonValue& doc) {
             JsonValue& exit = K7At(doc, {"maps", test::ch1::kZone, "exits"})->MutableItems()[0];
             exit.Set("targetMap", JsonValue::String(kSub));
             exit.Set("targetCol", JsonValue::Integer(kStart.col));
             exit.Set("targetRow", JsonValue::Integer(kStart.row));
           },
           "exits", "target 'sub_fx_k7_crypt' is a sub-dungeon"},
      };
      for (const Bad& b : proper) {
        CAPTURE(b.name);
        const DataLoadReport r = K7BadLoad("maps.json", b.edit);
        INFO(r.Summary());
        CHECK_FALSE(r.Ok());
        CHECK(K7HasIssue(r.errors, b.path, b.text));
      }
      const DataLoadReport events = K7BadLoad("random_events.json", [](JsonValue& doc) {
        JsonValue row = K7At(doc, {"zones"})->Get(test::ch1::kZone);
        K7At(doc, {"zones"})->Set(kSub, std::move(row));
      });
      INFO(events.Summary());
      CHECK_FALSE(events.Ok());
      CHECK(K7HasIssue(events.errors, "zones.sub_fx_k7_crypt", "a sub-dungeon rolls no random event"));
      // Unknown ids keep their own messages.
      const DataLoadReport unknown = K7BadLoad("maps.json", [](JsonValue& doc) {
        K7At(doc, {"order"})->Append(JsonValue::String("fx_k7_nowhere"));
      });
      CHECK(K7HasIssue(unknown.errors, "order", "unknown map 'fx_k7_nowhere'"));
    }
    // A name key missing in a locale: the zone banner would have nothing to show.
    {
      DataLoadReport r;
      K7Edits e = K7BaseEdits();
      e.emplace_back("i18n_en.json", [](JsonValue& doc) { K7At(doc, {"strings"})->Remove(kSubName); });
      K7LoadEdited(e, r);
      CHECK(K7HasIssue(r.errors, "subDungeons.sub_fx_k7_crypt.name", "i18n key missing"));
    }
    // A cleansedFlag is the sub-dungeon's own: no quest's setsFlags names it.
    {
      const DataLoadReport r = K7BadLoad("quests.json", [](JsonValue& doc) {
        for (JsonValue& q : K7At(doc, {"quests"})->MutableItems()) {
          if (q.Get("id").AsString() == kQuestOutside) q.Set("setsFlags", K7Parse(StrCat(R"([")", kFlag, R"("])")));
        }
      });
      CHECK(K7HasIssue(r.errors, "subDungeons.sub_fx_k7_crypt.cleansedFlag", "is also set by quest"));
    }
    // A map with the sub-dungeon's id.
    {
      const DataLoadReport r = K7BadLoad("maps.json", [](JsonValue& doc) {
        JsonValue copy = K7At(doc, {"subDungeons"})->Get(kSub);
        JsonValue* subs = K7At(doc, {"subDungeons"});
        const std::string mapId = test::ch1::kZone;
        copy.Set("id", JsonValue::String(mapId));
        copy.Remove("cleansedFlag");
        subs->Set(mapId, std::move(copy));
      });
      CHECK(K7HasIssue(r.errors, StrCat("subDungeons.", test::ch1::kZone), "a map has the same id"));
    }
    // A generated sub-dungeon (no tiles) carries none of the hand-authored fields.
    for (const char* key : {"collisions", "decorations", "presentationTheme", "regions", "corpseSeeds", "cleansedFlag"}) {
      CAPTURE(key);
      const DataLoadReport r = K7BadLoad("maps.json", [key](JsonValue& doc) {
        JsonValue& sd = K7At(doc, {"subDungeons"})->MutableMembers()[0].value;
        REQUIRE(sd.Find("tiles") == nullptr);
        sd.Set(key, std::string(key) == "presentationTheme" || std::string(key) == "cleansedFlag"
                        ? JsonValue::String("march")
                        : JsonValue::Array());
      });
      CHECK(K7HasIssue(r.errors, key, "on a generated sub-dungeon"));
    }
    {
      const DataLoadReport r = K7BadLoad("maps.json", [](JsonValue& doc) {
        JsonValue& sd = K7At(doc, {"subDungeons"})->MutableMembers()[0].value;
        sd.FindMutable("spawns")->MutableItems()[0].Set("dormant", K7Parse(R"({"wakeRange": 3})"));
      });
      CHECK(K7HasIssue(r.errors, "dormant", "a generated sub-dungeon's spawn has no 'dormant'"));
    }
    // An unsealed entrance of a generated sub-dungeon still loads (it stays inert), and a sub-dungeon no entrance
    // leads to only warns.
    {
      DataLoadReport r;
      K7Edits e = K7BaseEdits();
      e.emplace_back("maps.json", [](JsonValue& doc) {
        JsonValue& entrance = K7At(doc, {"maps", test::ch1::kZone, "subDungeonEntrances"})->MutableItems()[0];
        entrance.Set("targetSubDungeon", JsonValue::String(test::RealData().World().subDungeons.front().id));
      });
      // (The cleanse quest keeps naming the fixture sub-dungeon, which is still in the data.)
      K7LoadEdited(e, r);
      INFO(r.Summary());
      CHECK(r.Ok());
      CHECK(K7HasIssue(r.warnings, "subDungeons.sub_fx_k7_crypt", "no entrance of"));
    }
  }

  // 3A review R5: the walkability checks read the authored collisions, but the zone's grid also takes its blocking
  // decorations' footprints (MapGen ApplyDecorationBlocking), which spare the zone's anchors only. The guardian's tile
  // (the record's miniBoss, not a spawn entry of the map) and the tiles of a dormant line's members are anchors too.
  TEST_CASE("K7 data: a blocking decoration never stands on the guardian's tile or on a dormant line's members") {
    const TilePos plainTile{8, 7};                                       // floor that is nobody's anchor
    const TilePos member{kSentinelAnchor.col + 1, kSentinelAnchor.row};  // the second sentinel (line step +col)
    DataLoadReport r;
    K7Edits e = K7BaseEdits();
    e.emplace_back("maps.json", K7EditSub([member, plainTile](JsonValue& sd) {
      JsonValue decor = JsonValue::Array();
      for (const TilePos t : {kBoss, member, kSentinelAnchor, kStart, kExit, kGruntAnchor, plainTile}) {
        decor.Append(K7Parse(StrCat(R"({"col": )", t.col, R"(, "row": )", t.row, R"(, "type": "tree"})")));
      }
      sd.Set("decorations", std::move(decor));
    }));
    const std::unique_ptr<DataStore> store = K7LoadEdited(e, r);
    INFO(r.Summary());
    REQUIRE(r.Ok());
    const SubDungeonDef* sd = store->World().FindSubDungeon(kSub);
    REQUIRE(sd != nullptr);
    const ZoneGrid grid = BuildZoneGrid(*store, sd->map);
    REQUIRE(grid.Decorations().size() == 7);
    CHECK_FALSE(grid.Walkable(plainTile.col, plainTile.row));  // the tree blocks where it may
    CHECK(grid.Walkable(kBoss.col, kBoss.row));
    CHECK(grid.Walkable(member.col, member.row));
    for (const TilePos t : {kSentinelAnchor, kStart, kExit, kGruntAnchor}) CHECK(grid.Walkable(t.col, t.row));
    // In play: the guardian and both sentinels stand on walkable tiles of their own.
    K7Sim s(*store);
    s.GoIn();
    REQUIRE(s.Alive(kWarden).size() == 1);
    CHECK(s.Alive(kWarden).front()->pos == kBoss.Center());
    CHECK(s.TheZone().Walkable(kBoss.col, kBoss.row));
    const auto sentinels = s.Alive(kSentinel);
    REQUIRE(sentinels.size() == 2);
    CHECK(sentinels[0]->pos == kSentinelAnchor.Center());
    CHECK(sentinels[1]->pos == member.Center());
  }

  TEST_CASE("K7 data: Finalize refuses broken objective zones and clear_area objectives") {
    struct Bad {
      const char* name;
      const char* quest;
      size_t objective;
      std::function<void(JsonValue&)> edit;
      const char* path;
      const char* text;
    };
    const std::string generated = test::RealData().World().subDungeons.front().id;
    const std::vector<Bad> cases = {
        {"unknown zone", kQuestCleanse, 0, [](JsonValue& o) { o.Set("zone", JsonValue::String("fx_k7_nowhere")); },
         "objectives[0].zone", "unknown sub-dungeon"},
        {"empty zone", kQuestCleanse, 0, [](JsonValue& o) { o.Set("zone", JsonValue::String("")); },
         "objectives[0].zone", "empty zone id"},
        {"the quest's own zone", kQuestCleanse, 0,
         [](JsonValue& o) { o.Set("zone", JsonValue::String(test::ch1::kZone)); }, "objectives[0].zone",
         "the quest's own zone"},
        {"a generated sub-dungeon", kQuestCleanse, 0,
         [generated](JsonValue& o) { o.Set("zone", JsonValue::String(generated)); }, "objectives[0].zone",
         "is a generated sub-dungeon"},
        {"clear_area zone differs", kQuestCleanse, 1,
         [](JsonValue& o) { o.Set("zone", JsonValue::String("fx_k7_other")); }, "objectives[1].zone",
         "a clear_area's zone is its targetId"},
        {"clear_area required 2", kQuestCleanse, 1, [](JsonValue& o) { o.Set("required", JsonValue::Integer(2)); },
         "objectives[1].required", "has required 1"},
        {"clear_area negative threshold", kQuestCleanse, 1,
         [](JsonValue& o) { o.Set("maxRemaining", JsonValue::Integer(-1)); }, "objectives[1].maxRemaining",
         "expected 0..1000"},
        {"clear_area after itself", kQuestCleanse, 1, [](JsonValue& o) { o.Set("after", JsonValue::Integer(1)); },
         "objectives[1].after", "is not an earlier objective"},
        {"clear_area after a negative index", kQuestCleanse, 1,
         [](JsonValue& o) { o.Set("after", JsonValue::Integer(-2)); }, "objectives[1].after",
         "expected an objective index"},
        {"maxRemaining on a kill", kQuestCleanse, 0, [](JsonValue& o) { o.Set("maxRemaining", JsonValue::Integer(1)); },
         "objectives[0].maxRemaining", "not clear_area"},
        {"after on a kill", kQuestCleanse, 0, [](JsonValue& o) { o.Set("after", JsonValue::Integer(0)); },
         "objectives[0].after", "not clear_area"},
    };
    for (const Bad& b : cases) {
      CAPTURE(b.name);
      const DataLoadReport r = K7BadLoad("quests.json", K7EditObjective(b.quest, b.objective, b.edit));
      INFO(r.Summary());
      CHECK_FALSE(r.Ok());
      CHECK(K7HasIssue(r.errors, b.path, b.text));
    }
    // A sub-dungeon of another zone than the quest's.
    {
      const DataLoadReport r = K7BadLoad("quests.json", [](JsonValue& doc) {
        for (JsonValue& q : K7At(doc, {"quests"})->MutableItems()) {
          if (q.Get("id").AsString() == kQuestInside) {
            q.Set("zone", JsonValue::String(test::RealData().World().subDungeons.front().parentZone));
          }
        }
      });
      CHECK(K7HasIssue(r.errors, StrCat("quests.", kQuestInside, ".objectives[0].zone"), "not of the quest's zone"));
    }
    // A quest's own zone is a zone proper.
    {
      const DataLoadReport r = K7BadLoad("quests.json", [](JsonValue& doc) {
        for (JsonValue& q : K7At(doc, {"quests"})->MutableItems()) {
          if (q.Get("id").AsString() == kQuestOutside) q.Set("zone", JsonValue::String(kSub));
        }
      });
      CHECK(K7HasIssue(r.errors, StrCat("quests.", kQuestOutside, ".zone"), "is a sub-dungeon"));
    }
    // A clear_area waits for no other clear_area, and a sub-dungeon is cleansed by one objective only.
    {
      const DataLoadReport r = K7BadLoad("quests.json", [](JsonValue& doc) {
        K7At(doc, {"quests"})->Append(K7Parse(K7QuestJson(
            "fx_k7_q_second", StrCat(R"([{"type": "clear_area", "targetId": ")", kSub,
                                     R"(", "targetName": "fx", "required": 1, "current": 0}])"))));
      });
      CHECK(K7HasIssue(r.errors, "quests.fx_k7_q_second.objectives", "a second clear_area"));
    }
    {
      const DataLoadReport r = K7BadLoad("quests.json", [](JsonValue& doc) {
        for (JsonValue& q : K7At(doc, {"quests"})->MutableItems()) {
          if (q.Get("id").AsString() != kQuestCleanse) continue;
          q.Set("objectives", K7Parse(StrCat(R"([{"type": "clear_area", "targetId": ")", kSub,
                                             R"(", "targetName": "fx", "required": 1, "current": 0},
              {"type": "clear_area", "targetId": ")", kSub,
                                             R"(", "targetName": "fx", "required": 1, "current": 0, "after": 0}])")));
        }
      });
      CHECK(K7HasIssue(r.errors, "objectives[1].after", "cannot wait for another clear_area"));
    }
    // A sub-dungeon without a cleansedFlag cannot be cleansed.
    {
      const DataLoadReport r = K7BadLoad("maps.json", K7EditSub([](JsonValue& sd) { sd.Remove("cleansedFlag"); }));
      CHECK(K7HasIssue(r.errors, "objectives[1].zone", "has no cleansedFlag"));
    }
    // An escort objective is done in the quest's zone.
    {
      const DataLoadReport r = K7BadLoad("quests.json", K7EditObjective(kQuestOutside, 0, [](JsonValue& o) {
        o.Set("type", JsonValue::String("escort"));
        o.Set("zone", JsonValue::String(kSub));
      }));
      CHECK(K7HasIssue(r.errors, "objectives[0].zone", "an escort / defend_wave objective"));
    }
  }

  TEST_CASE("K7 data: Finalize refuses what a cleansed, empty sub-dungeon would soft-lock") {
    const auto killInside = [](JsonValue& o) {
      o.Set("type", JsonValue::String("kill"));
      o.Set("targetId", JsonValue::String(kGrunt));
      o.Set("zone", JsonValue::String(kSub));
    };
    const auto dropInside = [](JsonValue& o) {
      o.Set("type", JsonValue::String("collect"));
      o.Set("targetId", JsonValue::String("fx_k7_scrap"));
      o.Set("zone", JsonValue::String(kSub));
      o.Set("source", K7Parse(StrCat(R"({"kind": "drop", "monsters": [")", kGrunt, R"("], "chance": 1})")));
    };
    const auto cleanseNeeds = [](std::string_view quest) {
      return [quest = std::string(quest)](JsonValue& doc) {
        for (JsonValue& q : K7At(doc, {"quests"})->MutableItems()) {
          if (q.Find("id")->AsString() == kQuestCleanse) q.Set("prereqQuests", K7Parse(StrCat(R"([")", quest, R"("])")));
        }
      };
    };
    // The clear_area does not wait for the kill inside: taken after the guardian fell, the quest would cleanse the
    // crypt with its kill still open - and nothing spawns there again.
    {
      const DataLoadReport r =
          K7BadLoad("quests.json", K7EditObjective(kQuestCleanse, 1, [](JsonValue& o) { o.Remove("after"); }));
      INFO(r.Summary());
      CHECK(K7HasIssue(r.errors, "quests.fx_k7_q_cleanse.objectives[0].zone",
                       "its clear_area has to wait for this objective"));
    }
    // A kill or a drop inside for another quest: refused unless that quest is a prerequisite of the cleansing one (so
    // it is over before the cleanse can begin).
    for (const bool drop : {false, true}) {
      CAPTURE(drop);
      const std::function<void(JsonValue&)> edit = drop ? std::function<void(JsonValue&)>(dropInside) : killInside;
      {
        const DataLoadReport r = K7BadLoad("quests.json", K7EditObjective(kQuestOutside, 0, edit));
        INFO(r.Summary());
        CHECK(K7HasIssue(r.errors, "quests.fx_k7_q_outside.objectives[0].zone",
                         "has to be a prerequisite of that quest"));
      }
      {
        DataLoadReport r;
        K7Edits e = K7BaseEdits();
        e.emplace_back("quests.json", K7EditObjective(kQuestOutside, 0, edit));
        e.emplace_back("quests.json", cleanseNeeds(kQuestOutside));
        K7LoadEdited(e, r);
        INFO(r.Summary());
        CHECK(r.Ok());
      }
    }
    // A gather or an explore spot inside needs no monster: the fixture's inside quest is no prerequisite and loads
    // (K7Data() requires a clean report).
    CHECK(K7Data().FindQuest(kQuestInside) != nullptr);
    // A quest that can fail (an escort) and be taken again cannot cleanse: its second try would find the crypt empty.
    {
      const DataLoadReport r = K7BadLoad("quests.json", [](JsonValue& doc) {
        for (JsonValue& q : K7At(doc, {"quests"})->MutableItems()) {
          if (q.Find("id")->AsString() != kQuestCleanse) continue;
          q.Set("escortNpc", K7Parse(R"({"name": "fx", "spriteKey": "fx", "startCol": 20, "startRow": 20,
                                         "destCol": 24, "destRow": 24})"));
        }
      });
      INFO(r.Summary());
      CHECK(K7HasIssue(r.errors, "quests.fx_k7_q_cleanse.objectives", "a quest that can fail"));
    }
  }

  TEST_CASE("K7 validator: a flag set by a cleanse is set once its quest completes, unknown while it is active") {
    // Three NPCs with a visibleWhen on the cleanse flag: one that gives the cleanse quest and hides once the area is
    // clean (it could hide while the quest is active: an error), one that gives it but shows only once it is clean (the
    // quest could never be accepted: an error), and one that gives a follow-up of the cleanse quest and shows once it is
    // clean (fine: the cleanser is a prerequisite, so the flag is set for sure).
    const auto npc = [](std::string_view id, std::string_view quests, std::string_view when) {
      return StrCat(R"({"id": ")", id, R"(", "name": ")", id, R"(", "type": "quest", "dialogue": ["fx"], "quests": )",
                    quests, R"(, "dialogueTreeId": null, "nameKey": "data.npc.)", id, R"(.name", "visibleWhen": )",
                    when, "}");
    };
    const std::string flag = StrCat(R"({"flag": ")", kFlag, R"("})");
    const std::string notFlag = StrCat(R"({"notFlag": ")", kFlag, R"("})");
    const auto load = [&](std::string_view id, std::string_view quests, std::string_view when, bool follow) {
      DataLoadReport r;
      K7Edits e = K7BaseEdits();
      const std::string json = npc(id, quests, when);
      e.emplace_back("npcs.json", [id = std::string(id), json](JsonValue& doc) {
        K7At(doc, {"npcs"})->Set(id, K7Parse(json));
        // The cleanse quest moves from the fixture's giver to the new NPC when that one lists it.
      });
      e.emplace_back("maps.json", [id = std::string(id)](JsonValue& doc) {
        K7At(doc, {"maps", test::ch1::kZone, "camps"})->MutableItems()[0].FindMutable("npcs")->Append(
            JsonValue::String(id));
      });
      if (follow) {
        e.emplace_back("quests.json", [id = std::string(id)](JsonValue& doc) {
          JsonValue q = K7Parse(K7QuestJson("fx_k7_q_follow", K7OutsideObjectives()));
          q.Set("prereqQuests", K7Parse(StrCat(R"([")", kQuestCleanse, R"("])")));
          K7At(q, {"derived"})->Set("giverNpcId", JsonValue::String(id));
          K7At(doc, {"quests"})->Append(std::move(q));
        });
      } else {
        e.emplace_back("npcs.json", [](JsonValue& doc) {  // the new NPC is the cleanse quest's only giver
          K7At(doc, {"npcs", kGiver})->Set("quests", K7Parse(StrCat(R"([")", kQuestInside, R"(", ")", kQuestOutside,
                                                                    R"("])")));
        });
        e.emplace_back("quests.json", [id = std::string(id)](JsonValue& doc) {
          for (JsonValue& q : K7At(doc, {"quests"})->MutableItems()) {
            if (q.Find("id")->AsString() == kQuestCleanse) K7At(q, {"derived"})->Set("giverNpcId", JsonValue::String(id));
          }
        });
      }
      K7LoadEdited(e, r);
      return r;
    };
    const std::string cleanseOnly = StrCat(R"([")", kQuestCleanse, R"("])");
    {
      const DataLoadReport r = load("fx_k7_hider", cleanseOnly, notFlag, false);
      INFO(r.Summary());
      CHECK(K7HasIssue(r.errors, "npcs.fx_k7_hider.visibleWhen", "could hide 'fx_k7_hider' while quest"));
      CHECK(K7HasIssue(r.errors, "npcs.fx_k7_hider.visibleWhen", "is active"));
    }
    {
      const DataLoadReport r = load("fx_k7_late", cleanseOnly, flag, false);
      INFO(r.Summary());
      CHECK(K7HasIssue(r.errors, "npcs.fx_k7_late.visibleWhen", "could hide 'fx_k7_late' while quest"));
    }
    {
      const DataLoadReport r = load("fx_k7_after", R"(["fx_k7_q_follow"])", flag, true);
      INFO(r.Summary());
      CHECK(r.Ok());
      CHECK_FALSE(K7HasIssue(r.warnings, "npcs.fx_k7_after", "no quest sets flag"));
      CHECK_FALSE(K7HasIssue(r.warnings, "npcs.fx_k7_after.visibleWhen", "could hide"));
    }
    // A spawn variant on the cleanse flag is not "a flag no quest sets".
    {
      DataLoadReport r;
      K7Edits e = K7BaseEdits();
      e.emplace_back("maps.json", K7EditSub([](JsonValue& sd) {
        sd.FindMutable("spawns")->MutableItems()[0].Set(
            "variants", K7Parse(StrCat(R"([{"whenFlag": ")", kFlag, R"(", "monsterId": ")", kGrunt, R"(", "count": 0},
                {"whenFlag": "fx_k7_never", "monsterId": ")", kGrunt, R"(", "count": 1}])")));
      }));
      const std::unique_ptr<DataStore> store = K7LoadEdited(e, r);
      INFO(r.Summary());
      CHECK(r.Ok());
      CHECK_FALSE(K7HasIssue(r.warnings, "variants[0].whenFlag", "no quest sets flag"));
      CHECK(K7HasIssue(r.warnings, "subDungeons.sub_fx_k7_crypt.spawns[0].variants[1].whenFlag", "no quest sets flag"));
      const MapDef* m = store->FindMap(kSub);
      REQUIRE(m != nullptr);
      CHECK(m->spawns[0].variants.size() == 2);
    }
  }

  // ===================================================================================================================
  // travel
  // ===================================================================================================================

  TEST_CASE("K7 enter: the interact action at the open entrance goes in; the visit, the events and the snapshot") {
    K7Sim s(K7Data());
    const SubDungeonEntranceDef& entrance = s.Entrance();
    const SubDungeonDef& sd = *s.data.World().FindSubDungeon(kSub);
    // Out of reach (more than 3 tiles): no target, no prompt. The entrance is a static marker of the parent zone.
    s.Place(entrance.pos.Center() + Vec2(4, 0));
    s.Step();
    CHECK(s.TheZone().FindInteractTarget().kind != InteractKind::SubDungeon);
    const bool wentInFromAfar = s.TheZone().Interact() && s.ctx->session.transitioning;
    CHECK_FALSE(wentInFromAfar);
    const WorldMarkerView* mark = K7Marker(s.View(), MarkerKind::SubDungeonEntrance);
    REQUIRE(mark != nullptr);
    CHECK(mark->id == kNoEntity);
    CHECK(mark->key == entrance.id);
    CHECK(mark->target == kSub);
    CHECK(mark->enterable);
    CHECK_FALSE(mark->sealed);
    CHECK_FALSE(mark->cleansed);
    CHECK(Dist(mark->pos, entrance.pos.Center()) < 1e-9);
    CHECK_FALSE(s.View().zone.subDungeon.inside);
    // In reach: the prompt, then the way in.
    s.Place(entrance.pos.Center() + Vec2(1, 0));
    s.Clear();
    s.Step();
    const InteractTarget t = s.TheZone().FindInteractTarget();
    CHECK(t.kind == InteractKind::SubDungeon);
    CHECK(t.key == entrance.id);
    CHECK(s.View().prompt.kind == InteractKind::SubDungeon);
    s.Clear();
    REQUIRE(s.TheZone().Interact());
    CHECK(s.ctx->session.transitioning);
    CHECK_FALSE(s.sim->CanSave());
    // A second press during the fade does nothing.
    CHECK_FALSE(s.TheZone().Interact());
    s.AwaitZone(kSub);
    // The events: the enter line, the autosave of the parent, the fade, the parent's exit, the entry, its autosave.
    const auto zones = K7Of<EvZone>(s.Log());
    REQUIRE(zones.size() == 3);
    CHECK(zones[0].phase == EvZone::Phase::TransitionBegan);
    CHECK(zones[0].mapId == kSub);
    CHECK(zones[0].target == kStart);
    CHECK(zones[1].phase == EvZone::Phase::Exited);
    CHECK(zones[1].mapId == test::ch1::kZone);
    CHECK(zones[2].phase == EvZone::Phase::Entered);
    CHECK(zones[2].mapId == kSub);
    CHECK(zones[2].target == kStart);
    const int32_t enterLine = K7IndexOf<EvLog>(s.Log(), [](const EvLog& l) { return l.text.key == "zone.subDungeon.enter"; });
    REQUIRE(enterLine >= 0);
    CHECK(std::get<EvLog>(s.Log()[static_cast<size_t>(enterLine)]).text.args.size() == 1);
    CHECK(enterLine < K7IndexOf<EvZone>(s.Log()));
    CHECK(K7HasLog(s.Log(), "zone.enterZone"));
    bool banner = false;
    for (const EvBanner& b : K7Of<EvBanner>(s.Log())) banner = banner || (b.kind == BannerKind::Zone && b.title.key == kSubName);
    CHECK(banner);
    std::vector<std::string> saves;
    for (const EvSaveRequested& r : K7Of<EvSaveRequested>(s.Log())) saves.push_back(r.reason);
    CHECK(std::count(saves.begin(), saves.end(), std::string(EnumName(SaveReason::ZoneChange))) >= 1);
    CHECK(std::count(saves.begin(), saves.end(), std::string(EnumName(SaveReason::ZoneEntered))) == 1);
    // The hero stands on the player start; the zone is the authored grid.
    CHECK(Dist(s.TheHero().Position(), kStart.Center()) < 1e-9);
    CHECK(s.TheZone().MapId() == kSub);
    CHECK(s.TheZone().Grid().Cols() == kCols);
    CHECK_FALSE(s.TheZone().Walkable(10, 3));
    CHECK(s.TheZone().Npcs().empty());
    CHECK_FALSE(s.TheZone().InSafeZone(kStart.Center()));
    // The visit.
    const SubDungeonVisit& visit = s.ctx->session.subDungeon;
    CHECK(visit.active);
    CHECK(visit.subDungeonId == kSub);
    CHECK(visit.parentZone == test::ch1::kZone);
    CHECK(visit.entranceId == entrance.id);
    CHECK(visit.entrancePos == entrance.pos);
    CHECK(visit.returnPos == sd.returnPos);
    CHECK(s.ctx->session.HasVisited(kSub));
    // The snapshot.
    const Snapshot& v = s.View();
    CHECK(v.zone.mapId == kSub);
    CHECK(v.zone.nameKey == kSubName);
    CHECK(v.zone.cols == kCols);
    CHECK(v.zone.camps.empty());
    const SubDungeonView& sub = v.zone.subDungeon;
    CHECK(sub.inside);
    CHECK(sub.id == kSub);
    CHECK(sub.parentZone == test::ch1::kZone);
    CHECK(sub.parentNameKey == s.data.FindMap(test::ch1::kZone)->nameKey);
    CHECK(sub.entranceId == entrance.id);
    CHECK(Dist(sub.entrancePos, entrance.pos.Center()) < 1e-9);
    CHECK(Dist(sub.returnPos, sd.returnPos.Center()) < 1e-9);
    CHECK(Dist(sub.exitPos, kExit.Center()) < 1e-9);
    CHECK(sub.miniBossId == kWarden);
    CHECK(sub.guardianAlive);
    CHECK(sub.hostiles == kPopulation);
    CHECK(sub.cleansedFlag == kFlag);
    CHECK_FALSE(sub.cleansed);
    const WorldMarkerView* exit = K7Marker(v, MarkerKind::Exit);
    REQUIRE(exit != nullptr);
    CHECK(exit->key == test::ch1::kZone);
    CHECK_FALSE(exit->sealed);
    CHECK(Dist(exit->pos, kExit.Center()) < 1e-9);
    CHECK(K7Marker(v, MarkerKind::SubDungeonEntrance) == nullptr);
    CHECK(v.npcs.empty());
    // The population: the spawn list, the boss on its tile with the sub-dungeon role, nothing that respawns.
    CHECK(s.Alive(kGrunt).size() == static_cast<size_t>(kGrunts));
    CHECK(s.Alive(kSentinel).size() == static_cast<size_t>(kSentinels));
    const auto wardens = s.Alive(kWarden);
    REQUIRE(wardens.size() == 1);
    CHECK(wardens[0]->role == MonsterRole::SubDungeonMiniBoss);
    CHECK(wardens[0]->id == s.Monsters().MiniBoss());
    CHECK(wardens[0]->homeTile == kBoss);
    for (const MonsterInstance& m : s.Monsters().All()) CHECK(m.noRespawn);
    CHECK(s.Monsters().ZoneSubDungeon() == &sd);
    CHECK_FALSE(s.Monsters().ZoneCleansed());
  }

  TEST_CASE("K7 enter: a press on the entrance walks there first; a generated sub-dungeon's entrance stays inert") {
    {
      K7Sim s(K7Data());
      const SubDungeonEntranceDef& entrance = s.Entrance();
      s.Place(entrance.pos.Center() + Vec2(0, 6));
      s.Step();
      s.TheZone().OnPointerPress(entrance.pos.Center(), PointerButton::Primary);
      CHECK(s.TheZone().Pending().active);
      CHECK(s.TheZone().Pending().target.kind == InteractKind::SubDungeon);
      CHECK_FALSE(s.ctx->session.transitioning);
      for (int i = 0; i < 1200 && s.Zone() != kSub; ++i) s.Step();
      CHECK(s.Zone() == kSub);
      CHECK(s.ctx->session.subDungeon.entranceId == entrance.id);
    }
    // The parent's entrance pointed at one of the web's generated sub-dungeons: no target, no prompt, no press action.
    {
      DataLoadReport r;
      K7Edits e = K7BaseEdits();
      e.emplace_back("maps.json", [](JsonValue& doc) {
        JsonValue& entrance = K7At(doc, {"maps", test::ch1::kZone, "subDungeonEntrances"})->MutableItems()[0];
        entrance.Set("targetSubDungeon", JsonValue::String(test::RealData().World().subDungeons.front().id));
      });
      const std::unique_ptr<DataStore> store = K7LoadEdited(e, r);
      REQUIRE(r.Ok());
      K7Sim s(*store);
      const SubDungeonEntranceDef& entrance = s.Entrance();
      s.Place(entrance.pos.Center() + Vec2(1, 0));
      s.Step();
      CHECK(s.TheZone().FindInteractTarget().kind != InteractKind::SubDungeon);
      CHECK(s.View().prompt.kind != InteractKind::SubDungeon);
      s.TheZone().Interact();
      CHECK_FALSE(s.ctx->session.transitioning);
      const WorldMarkerView* mark = K7Marker(s.View(), MarkerKind::SubDungeonEntrance);
      REQUIRE(mark != nullptr);
      CHECK_FALSE(mark->enterable);
    }
  }

  TEST_CASE("K7 enter: zone_entered fires for the sub-dungeon, once per save") {
    K7Opts o;
    o.trigger = true;
    K7Sim s(K7Data(o));
    s.GoIn();
    StoryDirector& story = *s.ctx->sys.story;
    CHECK(story.IsBusy());  // the T15 delay of the zone_entered beat is running
    CHECK_FALSE(story.Progress().Has(kCutscene));
    s.StepMs(s.data.Story().timing.beatDelayZoneEnteredMs + 50);
    CHECK(story.IsCinematic());
    story.FinishAllBeats();
    s.Step();
    CHECK(story.Progress().Has(kCutscene));
    CHECK_FALSE(story.IsBusy());
    // A second visit plays nothing.
    s.GoOut();
    s.GoIn();
    CHECK_FALSE(story.IsBusy());
  }

  TEST_CASE("K7 exit: the exit tile arms once the hero has walked away and returns to the parent's return tile") {
    K7Sim s(K7Data());
    const SubDungeonDef& sd = *s.data.World().FindSubDungeon(kSub);
    s.GoIn();
    // The start lies inside the exit's arming distance: standing on the exit right away does nothing (W8).
    REQUIRE(s.TheZone().Exits().size() == 1);
    CHECK_FALSE(s.TheZone().Exits()[0].armed);
    s.Place(kExit.Center());
    s.Step(5);
    CHECK(s.Zone() == kSub);
    CHECK_FALSE(s.ctx->session.transitioning);
    // Away (more than sqrt(6) tiles), then back onto it.
    s.Place(Vec2(6, 6));
    s.Step();
    CHECK(s.TheZone().Exits()[0].armed);
    const size_t insideKilled = s.Alive().size();
    s.Kill(s.Alive(kGrunt).front()->id);
    CHECK(s.Alive().size() == insideKilled - 1);
    s.Place(kExit.Center());
    s.Clear();
    s.Step();
    CHECK(s.ctx->session.transitioning);
    s.AwaitZone(test::ch1::kZone);
    const auto zones = K7Of<EvZone>(s.Log());
    REQUIRE(zones.size() == 3);
    CHECK(zones[0].phase == EvZone::Phase::TransitionBegan);
    CHECK(zones[0].mapId == test::ch1::kZone);
    CHECK(zones[0].target == sd.returnPos);
    CHECK(zones[1].phase == EvZone::Phase::Exited);
    CHECK(zones[1].mapId == kSub);
    CHECK(zones[2].phase == EvZone::Phase::Entered);
    CHECK(K7HasLog(s.Log(), "zone.subDungeon.exit"));
    CHECK(K7IndexOf<EvLog>(s.Log(), [](const EvLog& l) { return l.text.key == "zone.subDungeon.exit"; }) <
          K7IndexOf<EvZone>(s.Log()));
    // Back at the entrance: the return tile, the visit over, the parent zone rebuilt.
    CHECK(Dist(s.TheHero().Position(), sd.returnPos.Center()) < 1e-9);
    CHECK_FALSE(s.ctx->session.subDungeon.active);
    CHECK(s.ctx->session.subDungeon.subDungeonId.empty());
    CHECK_FALSE(s.ctx->session.arrivingFromSubDungeon);
    CHECK_FALSE(s.View().zone.subDungeon.inside);
    CHECK(s.View().zone.mapId == test::ch1::kZone);
    CHECK(K7Marker(s.View(), MarkerKind::SubDungeonEntrance) != nullptr);
    CHECK(s.Alive(kGrunt).empty());
    CHECK_FALSE(s.Alive().empty());
    // The entrance is no proximity trigger: standing by it changes nothing.
    s.Step(30);
    CHECK(s.Zone() == test::ch1::kZone);
    CHECK_FALSE(s.ctx->session.transitioning);
    // A second visit is a fresh one (not cleansed): everything stands again.
    s.GoIn();
    CHECK(s.Alive().size() == static_cast<size_t>(kPopulation));
    CHECK(s.View().zone.subDungeon.guardianAlive);
  }

  TEST_CASE("K7 exit: a return tile that is not walkable puts the hero on the nearest walkable tile") {
    K7Opts o;
    o.returnOnWall = true;
    K7Sim s(K7Data(o));
    const SubDungeonDef& sd = *s.data.World().FindSubDungeon(kSub);
    REQUIRE_FALSE(K7ParentGrid().Walkable(sd.returnPos.col, sd.returnPos.row));
    s.GoIn();
    s.GoOut();
    const TilePos at = RoundToTile(s.TheHero().Position());
    CHECK(s.TheZone().Walkable(at.col, at.row));
    CHECK(Dist(s.TheHero().Position(), sd.returnPos.Center()) <= 8 * std::sqrt(2.0));
    CHECK(at != sd.returnPos);
  }

  TEST_CASE("K7 inside: nothing respawns and no random event rolls; the parent zone still does both") {
    K7Sim s(K7Data());
    const double respawnMs = s.data.Monsters().ai.respawnDelayMs;
    // The parent: a regular monster's respawn is queued.
    EntityId outside = kNoEntity;
    for (const MonsterInstance* m : s.Alive()) {
      if (m->role == MonsterRole::Regular && !m->noRespawn) {
        outside = m->id;
        break;
      }
    }
    REQUIRE(outside != kNoEntity);
    s.Kill(outside);
    CHECK(K7RespawnQueued(s, outside));
    s.GoIn();
    // Inside: a kill queues nothing, and the zone stays one short for good.
    const EntityId grunt = s.Alive(kGrunt).front()->id;
    s.Kill(grunt);
    CHECK_FALSE(K7RespawnQueued(s, grunt));
    for (const Timer& t : s.ctx->timers.Pending()) {
      CHECK_FALSE((t.owner == TimerOwner::Monsters && t.kind == static_cast<uint16_t>(MonsterTimerKind::Respawn)));
    }
    // The hero walks about for longer than the respawn delay: no respawn, no event, no draw on the Events stream.
    const RngState events = s.ctx->rng.Get(RngStream::Events).GetState();
    s.Clear();
    const int steps = static_cast<int>(respawnMs / kSimStepMs) + 120;
    for (int i = 0; i < steps; ++i) {
      if (i % 30 == 0) s.Place(Vec2(3 + (i / 30) % 5, 3 + (i / 150) % 2));  // far from every monster's notice
      s.Step();
    }
    CHECK(s.Alive(kGrunt).size() == static_cast<size_t>(kGrunts - 1));
    CHECK(s.Alive().size() == static_cast<size_t>(kPopulation - 1));
    CHECK(K7Of<EvRandomEvent>(s.Log()).empty());
    CHECK(s.ctx->rng.Get(RngStream::Events).GetState() == events);
    CHECK(s.ctx->sys.randomEvents->Active().empty());
  }

  TEST_CASE("K7 death: a death inside restarts the parent zone at its first camp; nothing is left behind") {
    K7Sim s(K7Data());
    const MapDef& parent = *s.data.FindMap(test::ch1::kZone);
    REQUIRE_FALSE(parent.camps.empty());
    s.GoIn();
    Hero& hero = s.TheHero();
    hero.SetGold(1000);
    while (hero.Level() < 8) hero.AddExp(hero.ExpToNext() - hero.Exp(), s.ctx->equip);
    hero.RecalcDerived(s.ctx->equip);
    hero.FillHpMana();
    s.Place(Vec2(6, 6));
    s.Step();
    hero.SetHp(0);
    s.Clear();
    s.ctx->sys.combat->KillHero(HeroDeathCause::Other);
    CHECK(hero.Life() == HeroLife::Dying);
    // A save while it lies there is written where the respawn will put it: the parent's first camp, full life.
    SaveData dying;
    s.sim->BuildSave(dying, 1);
    CHECK(dying.player.currentMap == test::ch1::kZone);
    CHECK(dying.player.tileCol == parent.camps[0].pos.col);
    CHECK(dying.player.tileRow == parent.camps[0].pos.row);
    CHECK(dying.player.hp == hero.MaxHp());
    s.StepMs(s.data.World().constants.deathRespawnMs + 2 * kSimStepMs);
    CHECK(hero.Life() == HeroLife::Alive);
    CHECK(s.ctx->session.transitioning);
    s.AwaitZone(test::ch1::kZone);
    const auto zones = K7Of<EvZone>(s.Log());
    REQUIRE(zones.size() == 3);
    CHECK(zones[0].mapId == test::ch1::kZone);
    CHECK(zones[0].target == parent.camps[0].pos);
    CHECK(K7HasLog(s.Log(), "zone.death.logMessage"));
    CHECK_FALSE(K7HasLog(s.Log(), "zone.subDungeon.exit"));
    CHECK(Dist(hero.Position(), parent.camps[0].pos.Center()) < 1e-9);
    CHECK(hero.Hp() == hero.MaxHp());
    CHECK_FALSE(s.ctx->session.subDungeon.active);
    // combat 15: the penalty is permanent in a sub-dungeon - gold lost, no echo anywhere.
    CHECK(hero.Gold() < 1000);
    CHECK(K7HasLog(s.Log(), "zone.soulEcho.lostInDungeon"));
    CHECK_FALSE(s.ctx->sys.soulEcho->State().Has());
    CHECK(K7Marker(s.View(), MarkerKind::SoulEcho) == nullptr);
  }

  // 3A review R1 (save-ui-input 3.4 rule 3, scenario 17): the app going to the background (or the menu / quit path)
  // resolves a pending death with the timer's effects. Before K7 that path stood the hero up inside the sub-dungeon:
  // alt-tabbing in the death window was a free revive at the start with the guardian's damage kept.
  TEST_CASE("K7 death: a pending death resolved by the background / quit path restarts the parent zone as the timer does") {
    for (const bool background : {true, false}) {
      CAPTURE(background);
      K7Sim s(K7Data());
      const MapDef& parent = *s.data.FindMap(test::ch1::kZone);
      REQUIRE_FALSE(parent.camps.empty());
      const Vec2 camp = parent.camps[0].pos.Center();
      s.GoIn();
      Hero& hero = s.TheHero();
      hero.SetGold(1000);
      while (hero.Level() < 8) hero.AddExp(hero.ExpToNext() - hero.Exp(), s.ctx->equip);
      hero.RecalcDerived(s.ctx->equip);
      hero.FillHpMana();
      // The fight so far: the guardian hurt, a grunt dead.
      const EntityId warden = s.Alive(kWarden).front()->id;
      DamageFlags hurt;
      hurt.source = KillSource::HeroBasic;
      s.Monsters().ApplyDamage(warden, 55, hurt);
      REQUIRE(s.Monsters().Find(warden)->hp < s.Monsters().Find(warden)->maxHp);
      s.Kill(s.Alive(kGrunt).front()->id);
      s.Place(Vec2(6, 6));
      s.Step();
      hero.SetHp(0);
      s.ctx->sys.combat->KillHero(HeroDeathCause::Other);
      s.StepMs(300);  // inside the death window
      REQUIRE(hero.Life() == HeroLife::Dying);
      const int64_t goldAfterDeath = hero.Gold();
      CHECK(goldAfterDeath < 1000);
      s.Clear();
      if (background) {
        s.sim->Submit(CmdAppBackground{true});
      } else {
        s.sim->Submit(CmdResolvePendingDeath{});
      }
      s.Step();
      // The respawn completed now: alive, full, on its way to the parent's first camp - not standing inside.
      CHECK(hero.Life() == HeroLife::Alive);
      CHECK(hero.Hp() == hero.MaxHp());
      CHECK(hero.Mana() == hero.MaxMana());
      CHECK(hero.Gold() == goldAfterDeath);  // no second penalty
      CHECK(s.ctx->session.transitioning);
      CHECK(K7Of<EvHeroRespawned>(s.Log()).size() == 1);
      CHECK(K7HasLog(s.Log(), "zone.death.logMessage"));
      CHECK_FALSE(K7HasLog(s.Log(), "sys.player.respawn"));
      const int32_t began = K7IndexOf<EvZone>(s.Log(), [](const EvZone& z) {
        return z.phase == EvZone::Phase::TransitionBegan;
      });
      REQUIRE(began >= 0);
      CHECK(std::get<EvZone>(s.Log()[static_cast<size_t>(began)]).mapId == test::ch1::kZone);
      CHECK(std::get<EvZone>(s.Log()[static_cast<size_t>(began)]).target == parent.camps[0].pos);
      // The save UE writes in that very step (the OS may suspend the app right after): the parent zone at that camp,
      // full life - the same place the timer's restart ends, never the pit mouth and never inside.
      bool asked = false;
      for (const EvSaveRequested& r : K7Of<EvSaveRequested>(s.Log())) {
        asked = asked || r.reason == EnumName(background ? SaveReason::AppBackground : SaveReason::Respawn);
      }
      CHECK(asked);
      SaveData save;
      s.sim->BuildSave(save, 9);
      CHECK(save.player.currentMap == test::ch1::kZone);
      CHECK(save.player.tileCol == camp.x);
      CHECK(save.player.tileRow == camp.y);
      CHECK(save.player.hp == hero.MaxHp());
      CHECK(save.player.gold == goldAfterDeath);
      auto loaded = GameSim::Create(s.data, SimConfig{});
      REQUIRE(loaded->LoadGame(save, nullptr) == SaveError::None);
      CHECK(loaded->Context().session.currentMap == test::ch1::kZone);
      CHECK(Dist(loaded->Context().sys.hero->Position(), camp) < 1e-9);
      if (background) {
        // In the background the sim is frozen: the fade waits for the foreground.
        s.Step(90);
        CHECK(s.Zone() == kSub);
        s.sim->Submit(CmdAppBackground{false});
      }
      s.AwaitZone(test::ch1::kZone);
      CHECK(Dist(hero.Position(), camp) < 1e-9);
      CHECK(hero.Life() == HeroLife::Alive);
      CHECK_FALSE(s.ctx->session.subDungeon.active);
      // The death's own timer is gone: no second respawn after the window.
      s.StepMs(s.data.World().constants.deathRespawnMs + 200);
      CHECK(K7Of<EvHeroRespawned>(s.Log()).size() == 1);
      CHECK(s.Zone() == test::ch1::kZone);
      CHECK(Dist(hero.Position(), camp) < 1e-9);
      // Going back in finds the sub-dungeon rebuilt: the grunt is back, the guardian whole.
      s.GoIn();
      CHECK(s.Alive().size() == static_cast<size_t>(kPopulation));
      REQUIRE(s.Alive(kWarden).size() == 1);
      CHECK(s.Alive(kWarden).front()->hp == s.Alive(kWarden).front()->maxHp);
    }
  }

  // The zone-exit path is unchanged: a hero that dies while the exit's fade runs stands up in the zone being left and
  // arrives with the zone change that was already under way (no second request, no camp restart).
  TEST_CASE("K7 death: a death during the exit's fade arrives at the return tile with the zone change under way") {
    K7Sim s(K7Data());
    const SubDungeonDef& sd = *s.data.World().FindSubDungeon(kSub);
    s.GoIn();
    Hero& hero = s.TheHero();
    s.Place(Vec2(6, 6));
    s.Step();
    s.Place(kExit.Center());
    s.Step(2);
    REQUIRE(s.ctx->session.transitioning);
    hero.SetHp(0);
    s.Clear();
    s.ctx->sys.combat->KillHero(HeroDeathCause::Other);
    s.sim->Submit(CmdResolvePendingDeath{});  // the menu path inside the fade: no second zone request either
    s.Step();
    CHECK(hero.Life() == HeroLife::Alive);
    s.AwaitZone(test::ch1::kZone);
    int32_t began = 0;
    for (const EvZone& z : K7Of<EvZone>(s.Log())) began += z.phase == EvZone::Phase::TransitionBegan ? 1 : 0;
    CHECK(began == 0);  // the one request predates the death
    CHECK(Dist(hero.Position(), sd.returnPos.Center()) < 1e-9);
    CHECK(hero.Hp() == hero.MaxHp());
  }

  TEST_CASE("K7 portal: inside, the town portal ends on the exit tile and so leaves; near the exit it is refused") {
    K7Sim s(K7Data());
    s.GoIn();
    // Within 3 tiles of the exit (the start is): refused with the exit's line.
    s.Clear();
    CHECK(s.TheZone().CanUseTownPortal() == PortalRefusal::AlreadyAtCamp);
    CHECK_FALSE(s.TheZone().UseTownPortal());
    CHECK(K7HasLog(s.Log(), "zone.teleport.alreadyAtExit"));
    CHECK_FALSE(K7HasLog(s.Log(), "zone.teleport.alreadyAtCamp"));
    // From the far side: the channel, then the exit tile, then the zone change.
    s.Place(Vec2(4, 12));
    s.Step();
    CHECK(s.TheZone().CanUseTownPortal() == PortalRefusal::None);
    s.Clear();
    REQUIRE(s.TheZone().UseTownPortal());
    CHECK(Dist(s.TheZone().PortalDestination(), kExit.Center()) < 1e-9);
    s.StepMs(s.data.World().constants.townPortalChannelMs + 2 * kSimStepMs);
    CHECK(K7HasLog(s.Log(), "zone.teleport.toSubDungeonEntrance"));
    CHECK_FALSE(K7HasLog(s.Log(), "zone.teleport.toCamp"));
    s.AwaitZone(test::ch1::kZone);
    CHECK(K7HasLog(s.Log(), "zone.subDungeon.exit"));
  }

  // ===================================================================================================================
  // saves
  // ===================================================================================================================

  TEST_CASE("K7 save: a save made inside is written at the entrance; it loads in the parent zone") {
    K7Sim s(K7Data());
    const SubDungeonDef& sd = *s.data.World().FindSubDungeon(kSub);
    s.GoIn();
    s.Place(Vec2(7, 12));
    s.Step();
    CHECK(s.sim->CanSave());
    SaveData save;
    s.sim->BuildSave(save, 5);
    CHECK(save.player.currentMap == test::ch1::kZone);
    CHECK(save.player.tileCol == sd.returnPos.col);
    CHECK(save.player.tileRow == sd.returnPos.row);
    CHECK(std::find(save.visitedZones.begin(), save.visitedZones.end(), std::string(kSub)) != save.visitedZones.end());
    // The text form names no sub-dungeon as the map either.
    const std::string text = s.sim->SaveGame(5);
    SaveData parsed;
    REQUIRE(ParseSave(text, parsed, nullptr) == SaveError::None);
    CHECK(parsed.player.currentMap == test::ch1::kZone);
    // Loading it: the parent zone, the return tile, no visit.
    auto loaded = GameSim::Create(s.data, SimConfig{});
    REQUIRE(loaded->LoadGame(text, nullptr) == SaveError::None);
    const SimContext& lc = loaded->Context();
    CHECK(lc.session.currentMap == test::ch1::kZone);
    CHECK(Dist(lc.sys.hero->Position(), sd.returnPos.Center()) < 1e-9);
    CHECK_FALSE(lc.session.subDungeon.active);
    CHECK_FALSE(loaded->View().zone.subDungeon.inside);
    // A save that names the sub-dungeon (hand-edited) loads the same way.
    SaveData edited = save;
    edited.player.currentMap = kSub;
    edited.player.tileCol = 7;
    edited.player.tileRow = 12;
    auto fixed = GameSim::Create(s.data, SimConfig{});
    REQUIRE(fixed->LoadGame(edited, nullptr) == SaveError::None);
    CHECK(fixed->Context().session.currentMap == test::ch1::kZone);
    CHECK(Dist(fixed->Context().sys.hero->Position(), sd.returnPos.Center()) < 1e-9);
    // So does one that names a generated sub-dungeon: its parent zone, never the default map by accident.
    const SubDungeonDef& web = test::RealData().World().subDungeons.front();
    SaveData webSave = save;
    webSave.player.currentMap = web.id;
    auto webSim = GameSim::Create(s.data, SimConfig{});
    REQUIRE(webSim->LoadGame(webSave, nullptr) == SaveError::None);
    CHECK(webSim->Context().session.currentMap == web.parentZone);
  }

  // 3A review R2: the return tile is authored against the parent's generated grid. When a prop or a wall stands on it,
  // a save made inside loads where the exit would have put the hero (the nearest walkable tile within 8), not at a
  // camp a zone away from the entrance.
  TEST_CASE("K7 save: with a return tile that is not walkable a save made inside still loads beside the entrance") {
    K7Opts o;
    o.returnOnWall = true;
    K7Sim s(K7Data(o));
    const SubDungeonDef& sd = *s.data.World().FindSubDungeon(kSub);
    const MapDef& parent = *s.data.FindMap(test::ch1::kZone);
    REQUIRE_FALSE(K7ParentGrid().Walkable(sd.returnPos.col, sd.returnPos.row));
    s.GoIn();
    s.Place(Vec2(7, 12));
    s.Step();
    SaveData save;
    s.sim->BuildSave(save, 5);
    REQUIRE(save.player.currentMap == test::ch1::kZone);
    REQUIRE(save.player.tileCol == sd.returnPos.col);
    REQUIRE(save.player.tileRow == sd.returnPos.row);
    // Where the exit itself puts the hero in this data.
    s.GoOut();
    const Vec2 viaExit = s.TheHero().Position();
    REQUIRE(s.TheZone().Walkable(RoundToTile(viaExit).col, RoundToTile(viaExit).row));
    REQUIRE(Dist(viaExit, sd.returnPos.Center()) <= 8 * std::sqrt(2.0));
    // The load: the same tile, no camp reset.
    const auto loadAt = [&](const SaveData& in, bool& reset) {
      auto sim = GameSim::Create(s.data, SimConfig{});
      REQUIRE(sim->LoadGame(in, nullptr) == SaveError::None);
      REQUIRE(sim->Context().session.currentMap == test::ch1::kZone);
      reset = K7HasLog(sim->Events(), "zone.save.positionReset");
      CHECK_FALSE(sim->Context().session.arrivingFromSubDungeon);
      return sim->Context().sys.hero->Position();
    };
    bool reset = false;
    const Vec2 loaded = loadAt(save, reset);
    CHECK(Dist(loaded, viaExit) < 1e-9);
    CHECK_FALSE(reset);
    CHECK(Dist(loaded, parent.camps[0].pos.Center()) > 2);  // not the 3.7 camp fallback
    // A save that names the sub-dungeon itself loads the same way.
    SaveData named = save;
    named.player.currentMap = kSub;
    named.player.tileCol = 7;
    named.player.tileRow = 12;
    CHECK(Dist(loadAt(named, reset), viaExit) < 1e-9);
    CHECK_FALSE(reset);
    // Any other unwalkable saved tile of the parent keeps the 3.7 rule: the nearest camp, with its log line.
    SaveData other = save;
    const TilePos wall = K7ReturnTile(false);
    TilePos elsewhere = wall;
    for (int32_t r = 0; r < K7ParentGrid().Rows() && elsewhere == wall; ++r) {
      for (int32_t c = 0; c < K7ParentGrid().Cols() && elsewhere == wall; ++c) {
        if (!K7ParentGrid().Walkable(c, r) && !(TilePos{c, r} == wall)) elsewhere = TilePos{c, r};
      }
    }
    REQUIRE_FALSE(elsewhere == wall);
    other.player.tileCol = elsewhere.col;
    other.player.tileRow = elsewhere.row;
    const Vec2 fallback = loadAt(other, reset);
    CHECK(reset);
    bool atCamp = false;
    for (const MapCampDef& c : parent.camps) atCamp = atCamp || Dist(fallback, c.pos.Center()) < 1e-9;
    CHECK(atCamp);
  }

  // ===================================================================================================================
  // clear_area
  // ===================================================================================================================

  TEST_CASE("K7 clear_area: the guardian dead and few enough left cleanses - the flag, the event, the objective") {
    K7Sim s(K7Data());
    QuestSystem& qs = s.Quests();
    REQUIRE(qs.Accept(kQuestCleanse));
    s.GoIn();
    // Few enough left but the guardian stands: nothing.
    s.KillAll(kGrunt);
    s.KillAll(kSentinel, 1);  // one sentinel and the warden stand
    s.Step(10);
    CHECK(s.Monsters().AliveCount() == 2);
    CHECK(K7Current(qs, kQuestCleanse, 1) == 0);
    CHECK_FALSE(s.ctx->session.HasFlag(kFlag));
    CHECK(K7Of<EvAreaCleansed>(s.Log()).empty());
    // The guardian falls: its kill objective, then the cleanse, in the same step.
    s.Place(Vec2(4, 8));
    s.Step();
    s.Clear();
    s.Kill(s.Alive(kWarden).front()->id);
    CHECK(K7Current(qs, kQuestCleanse, 0) == 1);
    s.Step();
    CHECK(K7Current(qs, kQuestCleanse, 1) == 1);
    CHECK(qs.Progress(kQuestCleanse)->status == QuestStatus::Completed);
    CHECK(s.ctx->session.HasFlag(kFlag));
    const auto cleansed = K7Of<EvAreaCleansed>(s.Log());
    REQUIRE(cleansed.size() == 1);
    CHECK(cleansed[0].zoneId == kSub);
    CHECK(cleansed[0].questId == kQuestCleanse);
    CHECK(cleansed[0].objectiveIndex == 1);
    CHECK(cleansed[0].flag == kFlag);
    CHECK(cleansed[0].remaining == 1);
    CHECK(Dist(cleansed[0].pos, Vec2(4, 8)) < 1e-9);
    // Order: the flag, the cleanse, the objective's progress, the quest's completion.
    const int32_t flagAt = K7IndexOf<EvWorldFlag>(s.Log(), [](const EvWorldFlag& w) { return w.flag == kFlag; });
    const int32_t cleanseAt = K7IndexOf<EvAreaCleansed>(s.Log());
    const int32_t progressAt = K7IndexOf<EvQuestUpdate>(s.Log(), [](const EvQuestUpdate& q) {
      return q.kind == EvQuestUpdate::Kind::Progress && q.objectiveIndex == 1;
    });
    const int32_t completedAt = K7IndexOf<EvQuestUpdate>(
        s.Log(), [](const EvQuestUpdate& q) { return q.kind == EvQuestUpdate::Kind::Completed; });
    REQUIRE(flagAt >= 0);
    CHECK(std::get<EvWorldFlag>(s.Log()[static_cast<size_t>(flagAt)]).questId == kQuestCleanse);
    CHECK(flagAt < cleanseAt);
    CHECK(cleanseAt < progressAt);
    CHECK(progressAt < completedAt);
    // The snapshot; the one left stays (and is not respawned or removed).
    CHECK(s.View().zone.subDungeon.cleansed);
    CHECK_FALSE(s.View().zone.subDungeon.guardianAlive);
    CHECK(s.View().zone.subDungeon.hostiles == 1);
    REQUIRE(s.View().worldFlags != nullptr);
    CHECK(std::find(s.View().worldFlags->begin(), s.View().worldFlags->end(), std::string(kFlag)) !=
          s.View().worldFlags->end());
    // Once: nothing more on later steps.
    s.Clear();
    s.Step(120);
    CHECK(K7Of<EvAreaCleansed>(s.Log()).empty());
    CHECK(K7Of<EvWorldFlag>(s.Log()).empty());
    CHECK(s.Alive(kSentinel).size() == 1);
  }

  TEST_CASE("K7 clear_area: the threshold, the guardian, the objective it waits for, the zone and a living hero all count") {
    // maxRemaining 0: every monster has to fall.
    {
      K7Opts o;
      o.maxRemaining = 0;
      K7Sim s(K7Data(o));
      REQUIRE(s.Quests().Accept(kQuestCleanse));
      s.GoIn();
      s.Kill(s.Alive(kWarden).front()->id);
      s.KillAll(kGrunt);
      s.KillAll(kSentinel, 1);
      s.Step(5);
      CHECK(K7Current(s.Quests(), kQuestCleanse, 1) == 0);
      s.KillAll(kSentinel);
      s.Step();
      CHECK(K7Current(s.Quests(), kQuestCleanse, 1) == 1);
      CHECK(K7Of<EvAreaCleansed>(s.Log()).back().remaining == 0);
    }
    // The warden killed before the quest was accepted: its kill objective is open, so nothing is cleansed; the quest is
    // then finished on a later visit (the sub-dungeon is rebuilt: the guardian stands again and has to fall).
    {
      K7Sim s(K7Data());
      s.GoIn();
      s.Kill(s.Alive(kWarden).front()->id);
      s.KillAll(kGrunt);
      s.Step(5);
      REQUIRE(s.Quests().Accept(kQuestCleanse));
      s.Step(5);
      CHECK(s.Monsters().AliveCount() <= 2);
      CHECK_FALSE(s.Monsters().GuardianAlive());
      CHECK(K7Current(s.Quests(), kQuestCleanse, 0) == 0);
      CHECK(K7Current(s.Quests(), kQuestCleanse, 1) == 0);
      CHECK_FALSE(s.ctx->session.HasFlag(kFlag));
      s.GoOut();
      s.GoIn();
      CHECK(s.Monsters().GuardianAlive());
      s.Kill(s.Alive(kWarden).front()->id);
      s.Step(2);
      CHECK(K7Current(s.Quests(), kQuestCleanse, 0) == 1);
      CHECK(K7Current(s.Quests(), kQuestCleanse, 1) == 0);  // six stood, five are left: more than two
      s.KillAll(kGrunt);
      s.Step();
      CHECK(K7Current(s.Quests(), kQuestCleanse, 1) == 1);
    }
    // The kill objective done, the hero gone before enough fell: outside nothing counts; a new visit needs the guardian
    // (standing again) dead as well.
    {
      K7Sim s(K7Data());
      REQUIRE(s.Quests().Accept(kQuestCleanse));
      s.GoIn();
      s.Kill(s.Alive(kWarden).front()->id);
      s.Step(2);
      CHECK(K7Current(s.Quests(), kQuestCleanse, 0) == 1);
      s.GoOut();
      s.Step(30);
      CHECK(K7Current(s.Quests(), kQuestCleanse, 1) == 0);
      s.GoIn();
      s.KillAll(kGrunt);
      s.KillAll(kSentinel);
      s.Step(5);
      CHECK(s.Monsters().AliveCount() == 1);
      CHECK(K7Current(s.Quests(), kQuestCleanse, 1) == 0);  // the guardian of this visit stands
      s.Kill(s.Alive(kWarden).front()->id);
      s.Step();
      CHECK(K7Current(s.Quests(), kQuestCleanse, 1) == 1);
    }
    // A hero that fell cleanses nothing.
    {
      K7Sim s(K7Data());
      REQUIRE(s.Quests().Accept(kQuestCleanse));
      s.GoIn();
      s.KillAll(kGrunt);
      s.TheHero().SetHp(0);
      s.ctx->sys.combat->KillHero(HeroDeathCause::Other);
      s.Kill(s.Alive(kWarden).front()->id);
      s.Step(3);
      CHECK(K7Current(s.Quests(), kQuestCleanse, 0) == 1);
      CHECK(K7Current(s.Quests(), kQuestCleanse, 1) == 0);
      CHECK_FALSE(s.ctx->session.HasFlag(kFlag));
    }
  }

  TEST_CASE("K7 cleansed: a cleansed sub-dungeon spawns nothing again, across exits, saves and loads") {
    K7Sim s(K7Data());
    REQUIRE(s.Quests().Accept(kQuestCleanse));
    s.GoIn();
    s.KillAll(kGrunt);
    s.Kill(s.Alive(kWarden).front()->id);
    s.Step();
    REQUIRE(s.ctx->session.HasFlag(kFlag));
    CHECK(s.Monsters().ZoneCleansed());
    CHECK(s.Alive().size() == static_cast<size_t>(kSentinels));  // the ones left stay for this visit
    s.GoOut();
    const WorldMarkerView* mark = K7Marker(s.View(), MarkerKind::SubDungeonEntrance);
    REQUIRE(mark != nullptr);
    CHECK(mark->cleansed);
    CHECK(mark->enterable);
    s.GoIn();
    CHECK(s.Monsters().All().empty());
    CHECK(s.Monsters().MiniBoss() == kNoEntity);
    CHECK(s.View().zone.subDungeon.cleansed);
    CHECK(s.View().zone.subDungeon.hostiles == 0);
    CHECK_FALSE(s.View().zone.subDungeon.guardianAlive);
    CHECK(s.View().monsters.empty());
    // The quest is still to be turned in; the save carries the flag, and a load keeps the crypt empty.
    SaveData save;
    s.sim->BuildSave(save, 9);
    CHECK(std::find(save.worldFlags.begin(), save.worldFlags.end(), std::string(kFlag)) != save.worldFlags.end());
    K7Sim t(K7Data(), 23);
    REQUIRE(t.sim->LoadGame(save, nullptr) == SaveError::None);
    t.Bind();
    CHECK(t.ctx->session.HasFlag(kFlag));
    CHECK(t.Zone() == test::ch1::kZone);
    t.Step();
    t.GoIn();
    CHECK(t.Monsters().All().empty());
    CHECK(t.View().zone.subDungeon.cleansed);
    // An emptied crypt cleanses nothing a second time.
    t.Clear();
    t.Step(60);
    CHECK(K7Of<EvAreaCleansed>(t.Log()).empty());
  }

  // 3A review R3: "cleansed = empty" reads the flag alone. A save whose flag has lost its cleanse (the cleansing
  // quest's record dropped by a content-version upgrade, reset because its objective list changed, or edited by hand)
  // would leave that quest unfinishable: nothing spawns, so the kill its clear_area waits for can never be earned.
  TEST_CASE("K7 cleansed: a load drops a cleansed flag whose cleanse is not in the quest log; the quest can be done again") {
    K7Sim s(K7Data());
    REQUIRE(s.Quests().Accept(kQuestCleanse));
    s.GoIn();
    s.KillAll(kGrunt);
    s.Kill(s.Alive(kWarden).front()->id);
    s.Step();
    REQUIRE(s.ctx->session.HasFlag(kFlag));
    s.GoOut();
    SaveData save;
    s.sim->BuildSave(save, 9);
    save.worldFlags.push_back("fx_k7_other_flag");  // a flag no sub-dungeon owns: never touched
    const auto record = [&](SaveData& in) -> QuestProgress* {
      for (QuestProgress& p : in.quests) {
        if (p.questId == kQuestCleanse) return &p;
      }
      return nullptr;
    };
    REQUIRE(record(save) != nullptr);
    REQUIRE(record(save)->status == QuestStatus::Completed);
    const auto load = [&](const SaveData& in) {
      auto t = std::make_unique<K7Sim>(K7Data(), 23);
      REQUIRE(t->sim->LoadGame(in, nullptr) == SaveError::None);
      t->Bind();
      t->Step();
      t->Clear();
      CHECK(t->ctx->session.HasFlag("fx_k7_other_flag"));
      return t;
    };
    // The cleanse is in the log: completed, turned in, or the objective itself done in a quest that is still active.
    {
      CHECK(load(save)->ctx->session.HasFlag(kFlag));
      SaveData turnedIn = save;
      record(turnedIn)->status = QuestStatus::TurnedIn;
      CHECK(load(turnedIn)->ctx->session.HasFlag(kFlag));
      SaveData active = save;
      record(active)->status = QuestStatus::Active;
      REQUIRE(record(active)->objectives.size() == 2);
      REQUIRE(record(active)->objectives[1] >= 1);
      auto t = load(active);
      CHECK(t->ctx->session.HasFlag(kFlag));
      t->GoIn();
      CHECK(t->Monsters().All().empty());
    }
    // It is not: no record (the quest's id is new to the save), a record without the clear_area done, a failed one, and
    // a record QuestSystem::Load resets because its objective count no longer matches the data.
    SaveData noRecord = save;
    noRecord.quests.erase(noRecord.quests.begin() + (record(noRecord) - noRecord.quests.data()));
    SaveData notDone = save;
    record(notDone)->status = QuestStatus::Active;
    record(notDone)->objectives = {1, 0};
    SaveData failed = save;
    record(failed)->status = QuestStatus::Failed;
    SaveData reshaped = save;
    record(reshaped)->objectives.push_back(1);
    for (const SaveData* in : {&noRecord, &notDone, &failed, &reshaped}) {
      CAPTURE(in == &noRecord);
      CAPTURE(in == &notDone);
      CAPTURE(in == &failed);
      auto t = load(*in);
      CHECK_FALSE(t->ctx->session.HasFlag(kFlag));
      const WorldMarkerView* mark = K7Marker(t->View(), MarkerKind::SubDungeonEntrance);
      REQUIRE(mark != nullptr);
      CHECK_FALSE(mark->cleansed);
      // The save it writes no longer carries the flag.
      SaveData again;
      t->sim->BuildSave(again, 10);
      CHECK(std::find(again.worldFlags.begin(), again.worldFlags.end(), std::string(kFlag)) == again.worldFlags.end());
      if (in == &failed) continue;  // (a failed quest that is not reacceptable stays failed)
      // The crypt is populated again and the quest - taken (again) where it has no live record - cleanses it.
      QuestSystem& qs = t->Quests();
      if (qs.Progress(kQuestCleanse) == nullptr) REQUIRE(qs.Accept(kQuestCleanse));
      REQUIRE(qs.Progress(kQuestCleanse)->status == QuestStatus::Active);
      t->GoIn();
      CHECK_FALSE(t->View().zone.subDungeon.cleansed);
      CHECK(t->Alive().size() == static_cast<size_t>(kPopulation));
      REQUIRE(t->Alive(kWarden).size() == 1);
      t->KillAll(kGrunt);
      t->Kill(t->Alive(kWarden).front()->id);
      t->Step();
      CHECK(t->ctx->session.HasFlag(kFlag));
      CHECK(K7Of<EvAreaCleansed>(t->Log()).size() == 1);
      CHECK(qs.Progress(kQuestCleanse)->status == QuestStatus::Completed);
    }
  }

  // ===================================================================================================================
  // the guide and objective zones
  // ===================================================================================================================

  TEST_CASE("K7 guide: the arrow points at the entrance, inside at the objective, then at the exit and the giver") {
    K7Sim s(K7Data());
    QuestSystem& qs = s.Quests();
    const SubDungeonEntranceDef& entrance = s.Entrance();
    REQUIRE(qs.Accept(kQuestCleanse));
    qs.SetTracked(kQuestCleanse);
    s.Step(2);
    // In the parent: the objective lies inside, so the arrow leads to the entrance.
    GuideTarget g = s.View().guide;
    CHECK(g.kind == GuideTargetKind::Entrance);
    CHECK(g.questId == kQuestCleanse);
    CHECK(g.objectiveIndex == 0);
    CHECK(g.reason == GuideReason::Objective);
    CHECK(Dist(g.pos, entrance.pos.Center()) < 1e-9);
    // Inside: the guardian (the kill objective), wherever he stands.
    s.GoIn();
    s.Step(2);
    g = s.View().guide;
    CHECK(g.kind == GuideTargetKind::Monster);
    CHECK(g.objectiveIndex == 0);
    CHECK(Dist(g.pos, s.Alive(kWarden).front()->pos) < 1e-9);
    // The guardian dead, too many left: the nearest monster still standing (the clear_area objective).
    s.Place(Vec2(4, 13));
    s.Kill(s.Alive(kWarden).front()->id);
    s.Step(20);
    g = s.View().guide;
    CHECK(g.kind == GuideTargetKind::Monster);
    CHECK(g.objectiveIndex == 1);
    double nearest = 1e9;
    for (const MonsterInstance* m : s.Alive()) nearest = (std::min)(nearest, Dist(m->pos, s.TheHero().Position()));
    CHECK(Dist(g.pos, s.TheHero().Position()) == doctest::Approx(nearest).epsilon(0.05));
    // Cleansed: the quest is complete, its giver waits outside - the exit.
    s.KillAll(kGrunt);
    s.Step(20);
    REQUIRE(qs.Progress(kQuestCleanse)->status == QuestStatus::Completed);
    g = s.View().guide;
    CHECK(g.kind == GuideTargetKind::Exit);
    CHECK(g.reason == GuideReason::TurnIn);
    CHECK(g.objectiveIndex == -1);
    CHECK(Dist(g.pos, kExit.Center()) < 1e-9);
    // Outside: the giver.
    s.GoOut();
    s.Step(20);
    g = s.View().guide;
    CHECK(g.kind == GuideTargetKind::Npc);
    CHECK(g.reason == GuideReason::TurnIn);
    const NpcPlacement* giver = s.TheZone().FindNpc(kGiver);
    REQUIRE(giver != nullptr);
    CHECK(Dist(g.pos, giver->pos) < 1e-9);
  }

  TEST_CASE("K7 guide: inside, a quest with an objective here leads; a quest of the parent points at the exit") {
    K7Sim s(K7Data());
    QuestSystem& qs = s.Quests();
    // Only a quest whose objective is outside: inside, the arrow shows the way out.
    REQUIRE(qs.Accept(kQuestOutside));
    qs.SetTracked("");
    s.GoIn();
    s.Step(20);
    const QuestDef* guided = qs.GuidedQuest(kSub);
    REQUIRE(guided != nullptr);
    GuideTarget g = s.View().guide;
    if (guided->id == kQuestOutside) {
      CHECK(g.kind == GuideTargetKind::Exit);
      CHECK(g.reason == GuideReason::Objective);
      CHECK(Dist(g.pos, kExit.Center()) < 1e-9);
    }
    // A quest with an unfinished objective in here leads when nothing is pinned, whatever the list order.
    REQUIRE(qs.Accept(kQuestCleanse));
    qs.SetTracked("");
    CHECK(qs.GuidedQuest(kSub)->id == kQuestCleanse);
    // In the parent the old rule holds: the pinned one, else the first main quest, else the first open one.
    const QuestDef* parentGuided = qs.GuidedQuest(test::ch1::kZone);
    REQUIRE(parentGuided != nullptr);
    CHECK(parentGuided->id == kQuestOutside);
    qs.SetTracked(kQuestOutside);
    CHECK(qs.GuidedQuest(kSub)->id == kQuestOutside);  // the pin wins inside too
    // A zone that is neither: nothing of this zone's.
    CHECK(qs.GuidedQuest(test::RealData().World().subDungeons.front().id) == nullptr);
  }

  TEST_CASE("K7 guide: ComputeGuideTarget keeps its rules in a zone-less world and follows wayTo in a zoned one") {
    const DataStore& d = K7Data();
    const QuestDef& q = *d.FindQuest(kQuestCleanse);
    QuestProgress p{kQuestCleanse, QuestStatus::Active, {0, 0}};
    // No zone id (the pure tests before K7): the kill objective is local - its monsters, else nothing.
    GuideWorld w;
    w.player = Vec2(1, 1);
    w.monsters = [](const std::vector<std::string>& ids, std::vector<Vec2>& out) {
      if (std::find(ids.begin(), ids.end(), std::string(kWarden)) != ids.end()) out.push_back(Vec2(9, 9));
    };
    GuideTarget g = ComputeGuideTarget(q, p, w);
    CHECK(g.kind == GuideTargetKind::Monster);
    // In the parent: the way in; with no way, no target for that objective (and none for the next, in the same zone).
    w.zoneId = test::ch1::kZone;
    w.wayTo = [](std::string_view zone, GuideTargetKind& kind, Vec2& out) {
      if (zone != kSub) return false;
      kind = GuideTargetKind::Entrance;
      out = Vec2(17, 26);
      return true;
    };
    g = ComputeGuideTarget(q, p, w);
    CHECK(g.kind == GuideTargetKind::Entrance);
    CHECK(g.objectiveIndex == 0);
    CHECK(g.pos == Vec2(17, 26));
    w.wayTo = nullptr;
    CHECK_FALSE(ComputeGuideTarget(q, p, w).Valid());
    // Inside: local targets again; the clear_area points at the nearest living monster.
    w.zoneId = kSub;
    p.objectives = {1, 0};
    w.hostiles = [](std::vector<Vec2>& out) {
      out.push_back(Vec2(8, 8));
      out.push_back(Vec2(2, 2));
    };
    g = ComputeGuideTarget(q, p, w);
    CHECK(g.kind == GuideTargetKind::Monster);
    CHECK(g.objectiveIndex == 1);
    CHECK(g.pos == Vec2(2, 2));
    w.hostiles = nullptr;
    CHECK_FALSE(ComputeGuideTarget(q, p, w).Valid());
    // Completed inside with the giver elsewhere: the way to the quest's zone.
    p.status = QuestStatus::Completed;
    w.wayTo = [](std::string_view zone, GuideTargetKind& kind, Vec2& out) {
      if (zone != test::ch1::kZone) return false;
      kind = GuideTargetKind::Exit;
      out = Vec2(2, 1);
      return true;
    };
    g = ComputeGuideTarget(q, p, w);
    CHECK(g.kind == GuideTargetKind::Exit);
    CHECK(g.reason == GuideReason::TurnIn);
    CHECK(EnumName(GuideTargetKind::Entrance) == "entrance");
    CHECK(EnumName(GuideTargetKind::Exit) == "exit");
  }

  // 3A review R4: the guide's spawn points in a sub-dungeon (QuestWorld::MakeGuideWorld). A live monster is always
  // found first in a normal visit, so the two K7 rules are asked directly: the guardian stands on the record's own
  // tile (he is no spawn entry), and a cleansed sub-dungeon has no spawn point at all (it spawns nothing).
  TEST_CASE("K7 guide: a sub-dungeon's spawn points are its entries and its guardian's tile; cleansed, it has none") {
    K7Sim s(K7Data());
    const QuestWorld& qw = *s.ctx->sys.questWorld;
    const auto points = [&](std::vector<std::string> ids) {
      std::vector<Vec2> out;
      const GuideWorld w = qw.MakeGuideWorld();
      REQUIRE(w.spawns);
      w.spawns(ids, out);
      return out;
    };
    // The parent: its own entries, and no point for the sub-dungeon's roster.
    const MapDef& parent = *s.data.FindMap(test::ch1::kZone);
    REQUIRE_FALSE(parent.spawns.empty());
    CHECK_FALSE(points({parent.spawns.front().monsterId}).empty());
    CHECK(points({kGrunt, kSentinel, kWarden}).empty());
    s.GoIn();
    REQUIRE(points({kGrunt}).size() == 1);
    CHECK(points({kGrunt})[0] == kGruntAnchor.Center());
    REQUIRE(points({kWarden}).size() == 1);
    CHECK(points({kWarden})[0] == kBoss.Center());
    CHECK(points({kGrunt, kSentinel, kWarden}).size() == 3);
    // The live arrow uses it when the guardian fell before his kill was asked for (the quest taken after): his tile.
    QuestSystem& qs = s.Quests();
    s.Place(Vec2(4, 13));
    s.Kill(s.Alive(kWarden).front()->id);
    REQUIRE(qs.Accept(kQuestCleanse));
    qs.SetTracked(kQuestCleanse);
    s.Step(20);
    REQUIRE(K7Current(qs, kQuestCleanse, 0) == 0);
    GuideTarget g = s.View().guide;
    CHECK(g.kind == GuideTargetKind::Spawn);
    CHECK(g.objectiveIndex == 0);
    CHECK(Dist(g.pos, kBoss.Center()) < 1e-9);
    // Out and in again he stands there; his kill and the cleanse follow, and the cleansed crypt has no spawn point.
    s.GoOut();
    s.GoIn();
    s.KillAll(kGrunt);
    s.Kill(s.Alive(kWarden).front()->id);
    s.Step();
    REQUIRE(s.ctx->session.HasFlag(kFlag));
    CHECK(points({kGrunt, kSentinel, kWarden}).empty());
    s.GoOut();
    s.GoIn();
    CHECK(points({kGrunt, kSentinel, kWarden}).empty());
  }

  TEST_CASE("K7 quest content: a target stands in its objective's zone - the guardian in the sub-dungeon only") {
    const DataStore& d = K7Data();
    // The content rule every quest is held to (quests_test runs it on the shipped data): the fixture's quests pass it.
    test::quests_fx::CheckQuestTargetsStandInTheirZones(d);
    // What it reads: the sub-dungeon's spawn list and its guardian; the parent zone has none of them.
    const std::set<std::string> inside = test::quests_fx::MonstersIn(d, kSub);
    CHECK(inside == std::set<std::string>{kGrunt, kSentinel, kWarden});
    const std::set<std::string> outside = test::quests_fx::MonstersIn(d, test::ch1::kZone);
    CHECK(outside.count(kWarden) == 0);
    CHECK(outside.count(kGrunt) == 0);
    CHECK(test::quests_fx::NpcsIn(d, kSub).empty());
    // The objective's zone, not the quest's, is where the rule looks.
    const QuestDef* q = d.FindQuest(kQuestCleanse);
    REQUIRE(q != nullptr);
    CHECK(ObjectiveZone(*q, q->objectives[0]) == kSub);
    CHECK(ObjectiveZone(*q, q->objectives[1]) == kSub);
    CHECK(q->zone == test::ch1::kZone);
    const QuestDef* talk = d.FindQuest(kQuestOutside);
    REQUIRE(talk != nullptr);
    CHECK(ObjectiveZone(*talk, talk->objectives[0]) == test::ch1::kZone);
  }

  TEST_CASE("K7 objective zones: gather nodes and explore spots live in the objective's zone") {
    K7Sim s(K7Data());
    QuestSystem& qs = s.Quests();
    REQUIRE(qs.Accept(kQuestInside));
    s.Step(40);
    // In the parent: no node of the inside objective, and standing on its tiles there explores nothing.
    CHECK(s.ctx->sys.questWorld->Nodes().empty());
    const Vec2 parentSpot(17, 13);
    s.Place(parentSpot);
    s.StepMs(1200);
    CHECK(K7Current(qs, kQuestInside, 1) == 0);
    GuideTarget g = s.View().guide;
    if (g.questId == kQuestInside) CHECK(g.kind == GuideTargetKind::Entrance);
    // Inside: three nodes around (5,12); gathering two completes the objective, the third goes with it.
    s.GoIn();
    s.Step(2);
    const std::vector<QuestNode> nodes(s.ctx->sys.questWorld->Nodes().begin(), s.ctx->sys.questWorld->Nodes().end());
    REQUIRE(nodes.size() == 3);
    for (const QuestNode& n : nodes) {
      CHECK(n.questId == kQuestInside);
      CHECK(JsHypot(n.pos.x - 5, n.pos.y - 12) <= 2.0 + 0.7072);  // a point in the circle, rounded to its tile
      CHECK(s.TheZone().Walkable(RoundToTile(n.pos).col, RoundToTile(n.pos).row));
    }
    s.Place(nodes[0].pos);
    s.Step(2);
    s.Place(nodes[1].pos);
    s.Step(2);
    CHECK(K7Current(qs, kQuestInside, 0) == 2);
    CHECK(s.ctx->sys.questWorld->Nodes().empty());
    // The explore spot inside.
    s.Place(Vec2(17, 13));
    s.StepMs(1200);
    CHECK(K7Current(qs, kQuestInside, 1) == 1);
    CHECK(qs.Progress(kQuestInside)->status == QuestStatus::Completed);
  }

  TEST_CASE("K7 objective zones: a drop objective of the parent's quest falls from kills inside the sub-dungeon") {
    // The talk quest becomes "2 scraps from the crypt's grunts" (always dropped) and a prerequisite of the cleansing
    // quest, as Finalize asks of a drop inside a sub-dungeon that can be cleansed.
    DataLoadReport r;
    K7Edits e = K7BaseEdits();
    e.emplace_back("quests.json", K7EditObjective(kQuestOutside, 0, [](JsonValue& o) {
      o.Set("type", JsonValue::String("collect"));
      o.Set("targetId", JsonValue::String("fx_k7_scrap"));
      o.Set("required", JsonValue::Integer(2));
      o.Set("zone", JsonValue::String(kSub));
      o.Set("source", K7Parse(StrCat(R"({"kind": "drop", "monsters": [")", kGrunt, R"("], "chance": 1})")));
    }));
    e.emplace_back("quests.json", [](JsonValue& doc) {
      for (JsonValue& q : K7At(doc, {"quests"})->MutableItems()) {
        if (q.Find("id")->AsString() == kQuestCleanse) {
          q.Set("prereqQuests", K7Parse(StrCat(R"([")", kQuestOutside, R"("])")));
        }
      }
    });
    const std::unique_ptr<DataStore> store = K7LoadEdited(e, r);
    INFO(r.Summary());
    REQUIRE(r.Ok());
    K7Sim s(*store);
    QuestSystem& qs = s.Quests();
    REQUIRE(qs.Accept(kQuestOutside));
    s.Step(2);
    // The arrow leads to the entrance; inside, to a grunt.
    CHECK(s.View().guide.kind == GuideTargetKind::Entrance);
    s.GoIn();
    s.Step(20);
    CHECK(s.View().guide.kind == GuideTargetKind::Monster);
    // Each grunt killed inside drops one; the hero standing on it picks it up.
    for (int i = 0; i < 2; ++i) {
      const MonsterInstance* g = s.Alive(kGrunt).front();
      const Vec2 at = g->pos;
      s.Place(at + Vec2(0.4, 0));
      s.Step();
      s.Kill(g->id);
      s.Step(2);
      s.Place(at);
      s.StepMs(1500);
    }
    CHECK(K7Current(qs, kQuestOutside, 0) == 2);
    CHECK(qs.Progress(kQuestOutside)->status == QuestStatus::Completed);
    // Done: the arrow now leads out, to the giver in the parent zone.
    s.Step(20);
    CHECK(s.View().guide.kind == GuideTargetKind::Exit);
  }

  // ===================================================================================================================
  // dormant
  // ===================================================================================================================

  TEST_CASE("K7 dormant: sentinels stand in formation, ignore the hero outside their wake range and never patrol") {
    K7Sim s(K7Data());
    s.GoIn();
    const auto sentinels = s.Alive(kSentinel);
    REQUIRE(sentinels.size() == 2);
    const EntityId a = sentinels[0]->id, b = sentinels[1]->id;
    // The line: member k on the anchor + k x (1, 0), facing -row (270 degrees), no jitter.
    CHECK(Dist(sentinels[0]->pos, kSentinelAnchor.Center()) < 1e-9);
    CHECK(Dist(sentinels[1]->pos, kSentinelAnchor.Center() + Vec2(1, 0)) < 1e-9);
    for (const MonsterInstance* m : sentinels) {
      CHECK(m->dormant);
      CHECK(m->dormantWakeRange == kWakeRange);
      CHECK(m->heading.x == doctest::Approx(0).epsilon(1e-9));
      CHECK(m->heading.y == doctest::Approx(-1));
      CHECK(m->packId != 0);
      REQUIRE(K7View(s.View(), m->id) != nullptr);
      CHECK(K7View(s.View(), m->id)->dormant);
    }
    CHECK(sentinels[0]->packId == sentinels[1]->packId);
    for (const MonsterInstance* m : s.Alive(kGrunt)) CHECK_FALSE(m->dormant);
    // Inside their aggro range (6) but outside the wake range (3): they stay as they are, for several patrol periods.
    s.Place(kSentinelAnchor.Center() + Vec2(0, -4.5));
    s.Clear();
    s.StepMs(4 * s.data.Monsters().ai.patrolIntervalMs);
    for (EntityId id : {a, b}) {
      const MonsterInstance* m = s.Monsters().Find(id);
      REQUIRE(m != nullptr);
      CHECK(m->dormant);
      CHECK(m->state == MonsterState::Idle);
    }
    CHECK(Dist(s.Monsters().Find(a)->pos, kSentinelAnchor.Center()) < 1e-9);
    CHECK(Dist(s.Monsters().Find(b)->pos, kSentinelAnchor.Center() + Vec2(1, 0)) < 1e-9);
    CHECK(K7IndexOf<EvPlayAnim>(s.Log(), [a, b](const EvPlayAnim& e) { return e.entity == a || e.entity == b; }) < 0);
  }

  TEST_CASE("K7 dormant: the hero inside the wake range wakes one - its Rise roots it - and its pack wakes with it") {
    K7Sim s(K7Data());
    s.GoIn();
    const EntityId a = s.Alive(kSentinel)[0]->id, b = s.Alive(kSentinel)[1]->id;
    // 2.5 tiles from the first, 2.5+ from the second's column: inside the first one's wake range only.
    s.Place(kSentinelAnchor.Center() + Vec2(-2.4, -0.5));
    s.Clear();
    s.Step(2);
    const MonsterInstance* ma = s.Monsters().Find(a);
    const MonsterInstance* mb = s.Monsters().Find(b);
    CHECK_FALSE(ma->dormant);
    CHECK(ma->state == MonsterState::Chase);
    const auto rises = K7Of<EvPlayAnim>(s.Log());
    int32_t riseA = 0, riseB = 0;
    double riseMs = 0;
    for (const EvPlayAnim& e : rises) {
      if (e.action != AnimAction::Rise) continue;
      riseA += e.entity == a ? 1 : 0;
      riseB += e.entity == b ? 1 : 0;
      riseMs = e.durationMs;
      CHECK(e.clip == "Rise");
    }
    CHECK(riseA == 1);
    CHECK(riseMs == doctest::Approx(s.Monsters().RiseMsFor(ma->def)));
    CHECK(riseMs > 0);
    // Its pack heard it (packAggro 6): the second wakes too, with its own Rise, within a step.
    s.Step(2);
    for (const EvPlayAnim& e : K7Of<EvPlayAnim>(s.Log())) riseB += e.entity == b && e.action == AnimAction::Rise ? 1 : 0;
    CHECK_FALSE(mb->dormant);
    CHECK(riseB >= 1);
    CHECK_FALSE(K7View(s.View(), a)->dormant);
    // Rooted while the clip plays, then it comes.
    const Vec2 at = ma->pos;
    s.StepMs(riseMs - 8 * kSimStepMs);
    CHECK(Dist(s.Monsters().Find(a)->pos, at) < 1e-9);
    s.StepMs(1500);
    CHECK(Dist(s.Monsters().Find(a)->pos, at) > 0.05);
    // One Rise each, ever.
    int32_t total = 0;
    for (const EvPlayAnim& e : K7Of<EvPlayAnim>(s.Log())) total += e.action == AnimAction::Rise ? 1 : 0;
    CHECK(total == 2);
  }

  // 3A review R6: the Helm Wall's footmen have no packAggro (on the surface a footman keeps to himself), so the K5
  // alarm alone would wake the wall man by man and a hero with reach could pick the line off from outside every wake
  // range. A dormant entry wakes as one: the alarm of the first to wake reaches every member still dormant.
  TEST_CASE("K7 dormant: a formation without packAggro wakes as one, from a step inside one wake range or a far hit") {
    DataLoadReport r;
    K7Edits e = K7BaseEdits();
    e.emplace_back("monsters.json", [](JsonValue& doc) { K7At(doc, {"defs", kSentinel})->Remove("packAggro"); });
    e.emplace_back("maps.json", K7EditSub([](JsonValue& sd) {
      sd.FindMutable("spawns")->MutableItems()[1].Set("count", JsonValue::Integer(4));  // (13..16, 12)
    }));
    const std::unique_ptr<DataStore> store = K7LoadEdited(e, r);
    INFO(r.Summary());
    REQUIRE(r.Ok());
    REQUIRE(store->Monsters().Find(kSentinel)->packAggro == 0);
    REQUIRE(store->Monsters().Find(kGrunt)->packAggro == 0);
    const auto rises = [](K7Sim& s, EntityId id) {
      int32_t n = 0;
      for (const EvPlayAnim& a : K7Of<EvPlayAnim>(s.Log())) n += a.entity == id && a.action == AnimAction::Rise ? 1 : 0;
      return n;
    };
    for (const bool byHit : {false, true}) {
      CAPTURE(byHit);
      K7Sim s(*store);
      s.GoIn();
      std::vector<EntityId> wall;
      for (const MonsterInstance* m : s.Alive(kSentinel)) wall.push_back(m->id);
      REQUIRE(wall.size() == 4);
      for (size_t k = 0; k < wall.size(); ++k) {
        const MonsterInstance* m = s.Monsters().Find(wall[k]);
        REQUIRE(m->dormant);
        CHECK(m->pos == kSentinelAnchor.Center() + Vec2(static_cast<double>(k), 0));
      }
      s.Clear();
      if (byHit) {
        // From the far side of the crypt (9+ tiles from the line): the last man is shot.
        s.Place(Vec2(4, 13));
        s.Step();
        for (EntityId id : wall) REQUIRE(s.Monsters().Find(id)->dormant);
        DamageFlags f;
        f.source = KillSource::HeroBasic;
        s.Monsters().ApplyDamage(wall[3], 1, f);
      } else {
        // Inside the first man's wake range only: 2.5 tiles from him, 3.4+ from the second.
        s.Place(kSentinelAnchor.Center() + Vec2(-2.4, -0.5));
        REQUIRE(Dist(s.TheHero().Position(), s.Monsters().Find(wall[1])->pos) > kWakeRange);
      }
      s.Step(3);
      for (EntityId id : wall) {
        const MonsterInstance* m = s.Monsters().Find(id);
        CAPTURE(id);
        CHECK_FALSE(m->dormant);
        CHECK(m->IsAggro());
        CHECK(rises(s, id) == 1);
        CHECK(s.ctx->Now() < m->abilityRootedUntilMs);  // each rooted for its own Rise
      }
      // They come: after the clip every one of them has left its tile toward the hero.
      const double riseMs = s.Monsters().RiseMsFor(s.Monsters().Find(wall[0])->def);
      s.StepMs(riseMs + 1500);
      for (size_t k = 0; k < wall.size(); ++k) {
        const MonsterInstance* m = s.Monsters().Find(wall[k]);
        CHECK(Dist(m->pos, kSentinelAnchor.Center() + Vec2(static_cast<double>(k), 0)) > 0.05);
        CHECK(rises(s, wall[k]) == 1);  // one Rise each, ever
      }
    }
    // The rule is the dormant entry's: an awake pack without packAggro still keeps to itself (K5).
    K7Sim s(*store);
    s.GoIn();
    s.Place(Vec2(3, 3));
    s.Step();
    const auto grunts = s.Alive(kGrunt);
    REQUIRE(grunts.size() == static_cast<size_t>(kGrunts));
    DamageFlags f;
    f.source = KillSource::HeroBasic;
    s.Monsters().ApplyDamage(grunts[0]->id, 1, f);
    s.Step(2);
    CHECK(s.Monsters().Find(grunts[0]->id)->IsAggro());
    for (size_t i = 1; i < grunts.size(); ++i) CHECK_FALSE(s.Monsters().Find(grunts[i]->id)->IsAggro());
    for (const MonsterInstance* m : s.Alive(kSentinel)) CHECK(m->dormant);  // another pack: the wall sleeps on
  }

  TEST_CASE("K7 spawns: a sub-dungeon spawn takes a flag variant and a pack leader, as a map spawn does") {
    DataLoadReport r;
    K7Edits e = K7BaseEdits();
    e.emplace_back("maps.json", K7EditSub([](JsonValue& sd) {
      JsonValue& grunts = sd.FindMutable("spawns")->MutableItems()[0];
      grunts.Set("variants", K7Parse(StrCat(R"([{"whenFlag": "fx_k7_thinned", "monsterId": ")", kGrunt,
                                            R"(", "count": 1}])")));
      grunts.Set("leader", K7Parse(StrCat(R"({"monsterId": ")", kGrunt, R"("})")));
    }));
    const std::unique_ptr<DataStore> store = K7LoadEdited(e, r);
    INFO(r.Summary());
    REQUIRE(r.Ok());
    K7Sim s(*store);
    s.GoIn();
    // No flag: the base entry - the three grunts, one of them their leader, one pack.
    CHECK(s.Alive(kGrunt).size() == static_cast<size_t>(kGrunts));
    CHECK(s.Alive(kSentinel).size() == static_cast<size_t>(kSentinels));
    uint32_t pack = 0;
    int32_t leaders = 0;
    for (const MonsterInstance* m : s.Alive()) {
      if (!m->packLeader) continue;
      ++leaders;
      pack = m->packId;
      CHECK(m->def.id == kGrunt);
      CHECK_FALSE(m->dormant);
      CHECK(m->noRespawn);
    }
    CHECK(leaders == 1);
    REQUIRE(pack != 0);
    for (const MonsterInstance* m : s.Alive(kGrunt)) CHECK(m->packId == pack);
    // The flag set (outside, as a quest's setsFlags would): the next visit spawns the variant - one grunt.
    s.GoOut();
    s.ctx->session.SetFlag("fx_k7_thinned");
    s.GoIn();
    CHECK(s.Alive(kGrunt).size() == 1);
    CHECK(s.Alive(kSentinel).size() == static_cast<size_t>(kSentinels));
  }

  // 3A review R4: separation steering (M6) pushes crowded monsters apart; a dormant one holds its formation tile.
  TEST_CASE("K7 dormant: a monster crowding a dormant one is pushed away; the dormant one does not move") {
    K7Sim s(K7Data());
    s.GoIn();
    const double radius = s.data.Monsters().ai.separationRadius;
    REQUIRE(radius > 0.4);
    REQUIRE(s.data.Monsters().ai.separationMaxSpeed > 0);
    const auto sentinels = s.Alive(kSentinel);
    REQUIRE(sentinels.size() == 2);
    const EntityId a = sentinels[0]->id;
    const Vec2 home = sentinels[0]->pos;
    const Vec2 heading = sentinels[0]->heading;
    const EntityId grunt = s.Alive(kGrunt).front()->id;
    // The hero: inside the AI's cull radius, outside the wake range and every grunt's notice.
    s.Place(home + Vec2(0, -5));
    s.Step();
    const Vec2 crowd = home + Vec2(0.3, 0);
    int pushed = 0;
    for (int i = 0; i < 40; ++i) {
      s.Monsters().Teleport(grunt, crowd, TeleportReason::Debug);
      s.Step();
      const MonsterInstance* g = s.Monsters().Find(grunt);
      const MonsterInstance* m = s.Monsters().Find(a);
      REQUIRE((g != nullptr && m != nullptr));
      pushed += Dist(g->pos, home) > Dist(crowd, home) ? 1 : 0;  // the awake one gives way...
      CHECK(m->pos == home);                                      // ...the dormant one stands, bit for bit
      CHECK(m->heading == heading);
      CHECK(m->dormant);
      CHECK(m->state == MonsterState::Idle);
    }
    CHECK(pushed == 40);
  }

  // 3A review R4: an elite of a sub-dungeon's population rolls the PARENT zone's affix count (as its guardian always
  // did), not the default a zone without a row gets. The table is edited so the two differ.
  TEST_CASE("K7 spawns: a sub-dungeon's elites roll the parent zone's affix count") {
    DataLoadReport r;
    K7Edits e = K7BaseEdits();
    e.emplace_back("monsters.json",
                   [](JsonValue& doc) { K7At(doc, {"defs", kGrunt})->Set("elite", JsonValue::Bool(true)); });
    e.emplace_back("elite_affixes.json", [](JsonValue& doc) {
      K7At(doc, {"zoneAffixCounts"})->Set(test::ch1::kZone, K7Parse("[2, 2]"));
      doc.Set("defaultAffixCount", K7Parse("[1, 1]"));
    });
    const std::unique_ptr<DataStore> store = K7LoadEdited(e, r);
    INFO(r.Summary());
    REQUIRE(r.Ok());
    const EliteAffixTable& table = store->Combat().eliteAffixes;
    int32_t lo = 0, hi = 0;
    table.CountFor(kSub, lo, hi);  // no row of its own: the default
    REQUIRE((lo == 1 && hi == 1));
    table.CountFor(test::ch1::kZone, lo, hi);
    REQUIRE((lo == 2 && hi == 2));
    for (const uint64_t seed : {11ull, 12ull, 13ull}) {
      K7Sim s(*store, seed);
      s.GoIn();
      const auto grunts = s.Alive(kGrunt);
      REQUIRE(grunts.size() == static_cast<size_t>(kGrunts));
      for (const MonsterInstance* m : grunts) CHECK(m->AffixCount() == 2);
      REQUIRE(s.Alive(kWarden).size() == 1);
      CHECK(s.Alive(kWarden).front()->AffixCount() == 2);
      for (const MonsterInstance* m : s.Alive(kSentinel)) CHECK(m->AffixCount() == 0);  // not elite: no roll
    }
  }

  TEST_CASE("K7 dormant: a hit wakes it; without `rise` it wakes at once") {
    {
      K7Sim s(K7Data());
      s.GoIn();
      const EntityId a = s.Alive(kSentinel)[0]->id;
      s.Place(kSentinelAnchor.Center() + Vec2(0, -5.5));  // outside the wake range
      s.Step(2);
      REQUIRE(s.Monsters().Find(a)->dormant);
      s.Clear();
      DamageFlags f;
      f.source = KillSource::HeroBasic;
      s.Monsters().ApplyDamage(a, 1, f);
      s.Step(2);
      CHECK_FALSE(s.Monsters().Find(a)->dormant);
      CHECK(K7IndexOf<EvPlayAnim>(s.Log(), [a](const EvPlayAnim& e) {
              return e.entity == a && e.action == AnimAction::Rise;
            }) >= 0);
    }
    {
      K7Opts o;
      o.dormantRise = false;
      K7Sim s(K7Data(o));
      s.GoIn();
      const EntityId a = s.Alive(kSentinel)[0]->id;
      CHECK_FALSE(s.Monsters().Find(a)->dormantRise);
      s.Place(kSentinelAnchor.Center() + Vec2(-2.4, -0.5));
      s.Clear();
      s.Step(2);
      CHECK_FALSE(s.Monsters().Find(a)->dormant);
      const Vec2 at = s.Monsters().Find(a)->pos;
      CHECK(K7IndexOf<EvPlayAnim>(s.Log(), [](const EvPlayAnim& e) { return e.action == AnimAction::Rise; }) < 0);
      s.StepMs(600);
      CHECK(Dist(s.Monsters().Find(a)->pos, at) > 0.05);  // no root: it is already coming
    }
  }

  // ===================================================================================================================
  // what a transition drops and keeps
  // ===================================================================================================================

  TEST_CASE("K7 transition: telegraphs, hazards, corpses and timers stay behind; the pet comes along") {
    K7Opts o;
    o.wardenAbility = true;
    K7Sim s(K7Data(o));
    // A beast walks in with the hero.
    REQUIRE_FALSE(s.data.Pets().pets.empty());
    const std::string petId = s.data.Pets().pets.front().id;
    REQUIRE(s.ctx->sys.pets->AddPet(petId, true));
    s.Step(2);
    REQUIRE(s.View().pet.present);
    const EntityId petOutside = s.View().pet.id;
    // A body in the parent's registry.
    const size_t seeded = s.View().corpses.size();
    s.GoIn();
    CHECK(s.View().pet.present);
    CHECK(s.View().pet.petId == petId);
    CHECK(s.View().pet.id != petOutside);
    CHECK(Dist(s.View().pet.pos, s.TheHero().Position()) < 4);
    CHECK(s.View().corpses.empty());
    // The warden winds its slam up beside the hero...
    const EntityId warden = s.Alive(kWarden).front()->id;
    s.Place(kBoss.Center() + Vec2(-1, 0));
    s.Clear();
    for (int i = 0; i < 600 && K7Of<EvTelegraph>(s.Log()).empty(); ++i) s.Step();
    REQUIRE_FALSE(K7Of<EvTelegraph>(s.Log()).empty());
    CHECK(K7Of<EvTelegraph>(s.Log())[0].entity == warden);
    s.Step();
    REQUIRE_FALSE(s.View().telegraphs.empty());
    // ...and a body falls; then the hero leaves through the exit while the telegraph is still up.
    s.Kill(s.Alive(kGrunt).front()->id);
    s.Step();
    s.Place(Vec2(6, 6));
    s.Step();
    s.Place(kExit.Center());
    s.Step();
    REQUIRE(s.ctx->session.transitioning);
    s.Clear();
    s.AwaitZone(test::ch1::kZone);
    // Nothing of the crypt is left: no telegraph, hazard, arena; the corpses are the parent's seeded ones again; no
    // timer of the old zone is pending; the strike never lands.
    CHECK(s.View().telegraphs.empty());
    CHECK(s.View().hazards.empty());
    CHECK(s.View().arenas.empty());
    CHECK(s.View().corpses.size() == seeded);
    for (const Timer& t : s.ctx->timers.Pending()) CHECK(t.dueMs >= s.sim->NowMs());
    const double hp = s.TheHero().Hp();
    s.StepMs(4000);
    CHECK(K7IndexOf<EvHit>(s.Log(), [warden](const EvHit& h) { return h.source == warden; }) < 0);
    CHECK(s.TheHero().Hp() >= hp - 1e-9 - 50);  // (the parent's own monsters may have found the hero)
    CHECK(s.View().pet.present);
    CHECK(s.Monsters().Find(warden) == nullptr);
  }

  TEST_CASE("K7 transition: an escort does not follow inside; its quest waits and it stands at its start again outside") {
    K7Sim s(K7Data());
    // The Chapter 1 zone's escort quest, with its prerequisites turned in.
    const QuestDef* escort = nullptr;
    for (const QuestDef& q : s.data.Quests().quests) {
      if (q.zone == test::ch1::kZone && q.hasEscortNpc) {
        escort = &q;
        break;
      }
    }
    REQUIRE(escort != nullptr);
    std::vector<QuestProgress> records;
    std::function<void(const QuestDef&)> turnIn = [&](const QuestDef& q) {
      for (const std::string& pre : q.prereqQuests) {
        const QuestDef* p = s.data.FindQuest(pre);
        REQUIRE(p != nullptr);
        turnIn(*p);
        std::vector<int32_t> done;
        for (const QuestObjectiveDef& ob : p->objectives) done.push_back(ob.required);
        records.push_back(QuestProgress{p->id, QuestStatus::TurnedIn, done});
      }
    };
    turnIn(*escort);
    s.Quests().Load(records);
    REQUIRE(s.Quests().Accept(escort->id));
    s.Step(2);
    REQUIRE(s.View().escort != nullptr);
    CHECK(s.View().escort->active);
    const Vec2 start = escort->escortNpc.start.Center();
    s.GoIn();
    CHECK_FALSE(s.View().escort->active);
    CHECK(K7Marker(s.View(), MarkerKind::EscortNpc) == nullptr);
    CHECK(s.Quests().Progress(escort->id)->status == QuestStatus::Active);
    s.GoOut();
    s.Step(2);
    CHECK(s.View().escort->active);
    CHECK_FALSE(s.View().escort->joined);
    CHECK(Dist(s.View().escort->pos, start) < 1.5);
  }

  // ===================================================================================================================
  // replay
  // ===================================================================================================================

  TEST_CASE("K7 replay: a seeded visit - in, a fight, the cleanse, out, in again - replays the same event log") {
    const auto run = [](uint64_t seed) {
      K7Sim s(K7Data(), seed);
      K7Fnv f;
      REQUIRE(s.Quests().Accept(kQuestCleanse));
      s.Quests().SetTracked(kQuestCleanse);
      s.GoIn();
      // Wake the sentinels and let them come; fight the grunts where they stand.
      s.Place(kSentinelAnchor.Center() + Vec2(-2.4, -0.5));
      s.Step(240);
      s.Place(kGruntAnchor.Center());
      s.Step(240);
      s.KillAll(kGrunt);
      s.Kill(s.Alive(kWarden).front()->id);
      s.Step(30);
      s.GoOut();
      s.Step(30);
      s.GoIn();
      s.Step(30);
      K7Digest(f, s.Log());
      f.U64(static_cast<uint64_t>(s.ctx->session.HasFlag(kFlag) ? 1 : 0));
      f.D(s.TheHero().Hp());
      for (const RngState& st : s.ctx->rng.GetStates()) {
        for (uint32_t w : st.s) f.U64(w);
      }
      return f.h;
    };
    const uint64_t a = run(31), b = run(31), c = run(32);
    CHECK(a == b);
    CHECK(a != c);
    MESSAGE("K7 replay digest (seed 31): " << a);
    // Pinned on GCC and Clang (determinism across compilers; re-pin with the value the message prints when the event
    // order of a visit changes on purpose).
    CHECK(a == 17502125020523143409ull);
  }
}
