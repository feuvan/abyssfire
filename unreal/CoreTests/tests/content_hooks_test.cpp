// K1 content hooks (dark remake, implementation-plan.md 3 "K1 Content hooks", work package 1A): defend wave composition
// and approach arc, the quest progress line, labelled reward choices, the monster anim actions (the mini-boss Taunt
// hold, the story focus-step anim), the sealed sub-dungeon entrance toast, the story boss configured by data (a
// synthetic id) and the grep gate that keeps Chapter 1 ids out of the core.
//
// The hooks are exercised on synthetic content: `HookData()` is the real export with a few records edited in memory
// (picked by role from the data, never by a Chapter 1 literal - content_ch1_test.cpp "content isolation"), so the
// tests keep working when the content switch (1B) changes the ids. The test suites are the existing ctest areas.
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "MonstersFixtures.h"
#include "SimHarness.h"
#include "SourceScan.h"
#include "TestContent.h"
#include "TestUtil.h"
#include "abyss/base/Json.h"
#include "abyss/base/Math.h"
#include "abyss/base/StrUtil.h"
#include "abyss/data/DataStore.h"
#include "abyss/items/LootGen.h"
#include "abyss/quests/QuestWorld.h"
#include "abyss/sim/GameSim.h"
#include "abyss/story/StoryDirector.h"
#include "doctest/doctest.h"

using namespace abyss;

namespace {

using namespace abyss::test::monsters_fx;

// ---- data editing ---------------------------------------------------------------------------------------------------

JsonValue ReadDataJson(std::string_view file) {
  JsonValue doc;
  REQUIRE(ParseJson(test::ReadFile(test::DataDir() + "/" + std::string(file)), doc));
  return doc;
}

JsonValue* At(JsonValue& root, std::initializer_list<std::string_view> keys) {
  JsonValue* v = &root;
  for (std::string_view k : keys) {
    v = v->FindMutable(k);
    REQUIRE(v != nullptr);
  }
  return v;
}

// The element of a JSON array whose "id" (or `key`) is `id`.
JsonValue* ById(JsonValue& arr, std::string_view id, std::string_view key = "id") {
  for (JsonValue& e : arr.MutableItems()) {
    if (e.Get(key).AsString() == id) return &e;
  }
  FAIL("no element " << id);
  return nullptr;
}

JsonValue Str(std::string_view s) { return JsonValue::String(s); }

JsonValue StrArray(const std::vector<std::string>& v) {
  JsonValue a = JsonValue::Array();
  for (const std::string& s : v) a.Append(Str(s));
  return a;
}

JsonValue WaveJson(const std::vector<DefendWaveEntry>& wave) {
  JsonValue a = JsonValue::Array();
  for (const DefendWaveEntry& e : wave) {
    JsonValue o = JsonValue::Object();
    o.Set("monsterId", Str(e.monsterId));
    o.Set("count", JsonValue::Integer(e.count));
    a.Append(std::move(o));
  }
  return a;
}

using DataEdits = std::vector<std::pair<std::string, std::function<void(JsonValue&)>>>;

// The real export with `edits` applied (several edits of one file run in order on one document).
std::unique_ptr<DataStore> LoadEdited(const DataEdits& edits, DataLoadReport& report) {
  std::map<std::string, JsonValue> docs;
  for (const auto& [file, edit] : edits) {
    if (docs.count(file) == 0) docs.emplace(file, ReadDataJson(file));
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

bool HasIssue(const DataLoadReport& r, std::string_view file, std::string_view pathPart, std::string_view text) {
  for (const DataIssue& e : r.errors) {
    if (e.file == file && Contains(e.path, pathPart) && Contains(e.message, text)) return true;
  }
  return false;
}

// ---- the records the hooks are put on (picked by role from the real export) -----------------------------------------

constexpr char kSynthBoss[] = "fx_marshal";         // the synthetic story boss (a copy of the data's one)
constexpr char kSealedEntrance[] = "fx_sealed_pit";  // a sealed entrance near the hero's start
constexpr char kOpenEntrance[] = "fx_open_door";     // an unsealed entrance (inert until sub-dungeon travel, K7)
constexpr char kSealedToast[] = "zone.dungeon.entranceBlocked";
constexpr double kApproachDeg = 90;  // the synthetic defend quest's approach (+row)

struct Picks {
  std::string zone;                  // the Chapter 1 zone (MapData defaultMap)
  std::string storyBoss;             // monster_ai storyBoss of the export
  std::string bossQuest;             // the quest that kills it (storyBossNotAfterQuestTurnIn)
  std::string bossKillCutscene;      // a cutscene the synthetic boss's monster_killed trigger plays (whileQuest bossQuest)
  std::string bossCutscene;          // its boss intro cutscene
  int32_t bossFocusStep = -1;        // the first focus step on it
  std::string progressQuest;         // the elder's first quest: gets a progressKey
  std::string progressKey;           // an existing key (the quest's name)
  std::string labelQuest;            // the elder's first quest with >= 2 choice slots: gets choiceLabels
  std::vector<std::string> labels;   // slot keys in reverse slot order (a mapping by slot, not by position)
  std::string defendQuest;           // the first defend quest with a target: gets waves + approachDeg
  std::string defendZone;
  std::vector<std::vector<DefendWaveEntry>> waves;
  TilePos sealedTile, openTile;
  std::string openTarget;            // an existing sub-dungeon
};

const Picks& P() {
  static const Picks p = [] {
    const DataStore& d = test::RealData();
    Picks r;
    r.zone = d.World().defaultMap;
    r.storyBoss = d.Monsters().ai.storyBossId;
    REQUIRE_FALSE(r.storyBoss.empty());
    r.bossQuest = d.Monsters().ai.storyBossNotAfterQuestTurnIn;
    REQUIRE_FALSE(r.bossQuest.empty());
    const BossIntroDef* intro = d.Story().BossIntroFor(r.storyBoss);
    REQUIRE(intro != nullptr);
    r.bossCutscene = intro->cutscene;
    for (const Cutscene& c : d.Story().cutscenes) {  // any cutscene that is not the intro (the synthetic kill beat)
      if (c.id != r.bossCutscene && r.bossKillCutscene.empty()) r.bossKillCutscene = c.id;
    }
    REQUIRE_FALSE(r.bossKillCutscene.empty());
    const Cutscene* cs = d.Story().FindCutscene(r.bossCutscene);
    REQUIRE(cs != nullptr);
    for (size_t i = 0; i < cs->steps.size() && r.bossFocusStep < 0; ++i) {
      const CutsceneStep& s = cs->steps[i];
      if (s.kind == StoryStepKind::Focus && s.target.kind == StoryActorKind::Monster && s.target.id == r.storyBoss) {
        r.bossFocusStep = static_cast<int32_t>(i);
      }
    }
    REQUIRE(r.bossFocusStep >= 0);
    const NpcDef* elder = d.FindNpc(test::ch1::kElder);
    REQUIRE(elder != nullptr);
    REQUIRE_FALSE(elder->quests.empty());
    r.progressQuest = elder->quests.front();
    r.progressKey = d.FindQuest(r.progressQuest)->nameKey;
    for (const std::string& id : elder->quests) {
      const QuestDef* q = d.FindQuest(id);
      if (q->rewards.choices.size() < 2) continue;
      r.labelQuest = id;
      for (auto it = q->rewards.choices.rbegin(); it != q->rewards.choices.rend(); ++it) {
        r.labels.push_back("sys.questCard.choice." + std::string(EnumName(*it)));
      }
      break;
    }
    REQUIRE_FALSE(r.labelQuest.empty());
    for (const QuestDef& q : d.Quests().quests) {
      if (q.type == QuestType::Defend && q.hasDefendTarget && q.defendTarget.totalWaves >= 2) {
        r.defendQuest = q.id;
        r.defendZone = q.zone;
        break;
      }
    }
    REQUIRE_FALSE(r.defendQuest.empty());
    // Regular monsters of the defend zone (no boss intro, not the mini-boss): wave w = 2 x a, 1 x b.
    std::vector<std::string> regular;
    const MiniBossEntry* mini = d.Monsters().MiniBossFor(r.defendZone);
    for (const std::string& id : d.Monsters().ZoneList(r.defendZone)->monsterIds) {
      if (d.Story().BossIntroFor(id) != nullptr || (mini != nullptr && mini->monsterId == id)) continue;
      regular.push_back(id);
    }
    REQUIRE(regular.size() >= 2);
    for (int32_t w = 0; w < d.FindQuest(r.defendQuest)->defendTarget.totalWaves; ++w) {
      const size_t a = static_cast<size_t>(w) % regular.size(), b = (a + 1) % regular.size();
      r.waves.push_back({{regular[a], 2}, {regular[b], 1 + w % 2}});
    }
    const MapDef* map = d.FindMap(r.zone);
    r.sealedTile = TilePos{map->playerStart.col + 2, map->playerStart.row + 4};
    r.openTile = TilePos{map->playerStart.col + 6, map->playerStart.row + 8};
    REQUIRE_FALSE(d.World().subDungeons.empty());
    r.openTarget = d.World().subDungeons.front().id;
    return r;
  }();
  return p;
}

// Every hook on one store: the real export plus the K1 fields on the picked records and the synthetic story boss.
DataEdits HookEdits() {
  const Picks& p = P();
  DataEdits e;
  e.emplace_back("quests.json", [&p](JsonValue& d) {
    JsonValue& quests = *At(d, {"quests"});
    ById(quests, p.progressQuest)->Set("progressKey", Str(p.progressKey));
    At(*ById(quests, p.labelQuest), {"rewards"})->Set("choiceLabels", StrArray(p.labels));
    JsonValue& target = *At(*ById(quests, p.defendQuest), {"defendTarget"});
    JsonValue waves = JsonValue::Array();
    for (const auto& w : p.waves) waves.Append(WaveJson(w));
    target.Set("waves", std::move(waves));
    target.Set("approachDeg", JsonValue::Number(kApproachDeg));
    // The story boss's quest kills the synthetic boss.
    for (JsonValue& o : At(*ById(quests, p.bossQuest), {"objectives"})->MutableItems()) {
      if (o.Get("targetId").AsString() == p.storyBoss) o.Set("targetId", Str(kSynthBoss));
    }
  });
  e.emplace_back("maps.json", [&p](JsonValue& d) {
    JsonValue list = JsonValue::Array();
    JsonValue sealed = JsonValue::Object();
    sealed.Set("id", Str(kSealedEntrance));
    sealed.Set("name", Str(kSealedEntrance));
    sealed.Set("col", JsonValue::Integer(p.sealedTile.col));
    sealed.Set("row", JsonValue::Integer(p.sealedTile.row));
    sealed.Set("targetSubDungeon", Str("fx_pit_later"));  // not in the data yet: allowed while sealed
    sealed.Set("sealed", JsonValue::Bool(true));
    sealed.Set("sealedTextKey", Str(kSealedToast));
    list.Append(std::move(sealed));
    JsonValue open = JsonValue::Object();
    open.Set("id", Str(kOpenEntrance));
    open.Set("name", Str(kOpenEntrance));
    open.Set("col", JsonValue::Integer(p.openTile.col));
    open.Set("row", JsonValue::Integer(p.openTile.row));
    open.Set("targetSubDungeon", Str(p.openTarget));
    list.Append(std::move(open));
    At(d, {"maps", p.zone})->Set("subDungeonEntrances", std::move(list));
    // The story boss spawn now spawns the synthetic boss.
    for (JsonValue& s : At(d, {"maps", p.zone, "spawns"})->MutableItems()) {
      if (s.Get("monsterId").AsString() == p.storyBoss) s.Set("monsterId", Str(kSynthBoss));
    }
  });
  e.emplace_back("monsters.json", [&p](JsonValue& d) {
    JsonValue copy = *At(d, {"defs", p.storyBoss});
    copy.Set("id", Str(kSynthBoss));
    At(d, {"defs"})->Set(kSynthBoss, std::move(copy));
  });
  e.emplace_back("monster_ai.json",
                 [](JsonValue& d) { At(d, {"port", "storyBoss"})->Set("monsterId", Str(kSynthBoss)); });
  e.emplace_back("story.json", [&p](JsonValue& d) {
    ById(*At(d, {"bossIntros"}), p.storyBoss, "monsterId")->Set("monsterId", Str(kSynthBoss));
    JsonValue& steps = *At(d, {"cutscenes", p.bossCutscene, "steps"});
    for (JsonValue& s : steps.MutableItems()) {
      for (const char* actor : {"target", "speaker"}) {
        JsonValue* a = s.FindMutable(actor);
        if (a != nullptr && a->IsObject() && a->Get("monster").AsString() == p.storyBoss) {
          a->Set("monster", Str(kSynthBoss));
        }
      }
    }
    steps.MutableItems()[static_cast<size_t>(p.bossFocusStep)].Set("anim", Str("roar"));
    // The story boss's kill beats key off the synthetic boss; one more plays only while its quest is open (whileQuest).
    JsonValue& triggers = *At(d, {"triggers"});
    for (JsonValue& t : triggers.MutableItems()) {
      if (t.Get("on").AsString() == "monster_killed" && t.Get("monsterId").AsString() == p.storyBoss) {
        t.Set("monsterId", Str(kSynthBoss));
      }
    }
    JsonValue guarded = JsonValue::Object();
    guarded.Set("on", Str("monster_killed"));
    guarded.Set("monsterId", Str(kSynthBoss));
    guarded.Set("cutscene", Str(p.bossKillCutscene));
    guarded.Set("whileQuest", Str(p.bossQuest));
    triggers.Append(std::move(guarded));
  });
  return e;
}

const DataStore& HookData() {
  static const std::unique_ptr<DataStore> store = [] {
    DataLoadReport report;
    std::unique_ptr<DataStore> s = LoadEdited(HookEdits(), report);
    INFO(report.Summary(20));
    REQUIRE(report.Ok());
    return s;
  }();
  return *store;
}

// HookData with monster_ai port.storyBoss.oncePerVisit off: the story boss spawns with the Regular role and respawns
// like any regular after respawnDelayMs.
const DataStore& RespawningBossData() {
  static const std::unique_ptr<DataStore> store = [] {
    DataEdits e = HookEdits();
    e.emplace_back("monster_ai.json",
                   [](JsonValue& d) { At(d, {"port", "storyBoss"})->Set("oncePerVisit", JsonValue::Bool(false)); });
    DataLoadReport report;
    std::unique_ptr<DataStore> s = LoadEdited(e, report);
    INFO(report.Summary(20));
    REQUIRE(report.Ok());
    return s;
  }();
  return *store;
}

// The real export with every quest's progressKey removed: the giver's fallback when no line is authored (the remade
// Chapter 1 authors one on every quest, so the fallback is tested on an edited store, not on the live content).
const DataStore& NoProgressKeyData() {
  static const std::unique_ptr<DataStore> store = [] {
    DataEdits e;
    e.emplace_back("quests.json", [](JsonValue& d) {
      for (JsonValue& q : At(d, {"quests"})->MutableItems()) q.Remove("progressKey");
    });
    DataLoadReport report;
    std::unique_ptr<DataStore> s = LoadEdited(e, report);
    INFO(report.Summary(20));
    REQUIRE(report.Ok());
    return s;
  }();
  return *store;
}

// ---- rigs -----------------------------------------------------------------------------------------------------------

// MonsterSystem over `zoneId` of `data` with a hero, statuses and a quest system (MonstersFixtures' MonsterRig on any
// data store).
struct HookMonsterRig {
  HookMonsterRig(const DataStore& data, std::string_view zoneId, uint64_t seed, Vec2 heroPos)
      : h(seed, data),
        hero(std::make_unique<Hero>(data, ClassId::Warrior)),
        status(data.Classes().statusRules),
        zone(h.ctx),
        quests(data, h.events, h.bus),
        monsters(h.ctx) {
    SimSystems& s = h.ctx.sys;
    s.hero = hero.get();
    s.status = &status;
    s.zone = &zone;
    s.quests = &quests;
    s.monsters = &monsters;
    h.onTimer = [this](const Timer& t) {
      if (t.owner == TimerOwner::Monsters) monsters.OnTimer(t);
    };
    h.bus.Subscribe<MonsterKilledMsg>([this](const MonsterKilledMsg& m) { monsters.OnMonsterKilled(m); });
    h.session.currentMap = std::string(zoneId);
    ok = zone.EnterZone(zoneId, true, heroPos);
    hero->RecalcDerived(h.ctx.equip);
    hero->FillHpMana();
  }
  std::vector<const MonsterInstance*> AliveOf(std::string_view defId) const {
    std::vector<const MonsterInstance*> out;
    for (const MonsterInstance& m : monsters.All()) {
      if (m.IsAlive() && m.def.id == defId) out.push_back(&m);
    }
    return out;
  }
  test::SimHarness h;
  std::unique_ptr<Hero> hero;
  StatusEffectSystem status;
  ZoneRuntime zone;
  QuestSystem quests;
  MonsterSystem monsters;
  bool ok = false;
};

std::unique_ptr<GameSim> StartOn(const DataStore& data, uint64_t seed = 11) {
  auto sim = GameSim::Create(data, SimConfig{});
  REQUIRE(sim != nullptr);
  REQUIRE(sim->NewGame(ClassId::Warrior, Difficulty::Normal, seed));
  sim->Context().sys.story->FinishAllBeats();
  sim->Step();
  return sim;
}

std::vector<Event> StepCollect(GameSim& sim, int steps) {
  std::vector<Event> out;
  for (int i = 0; i < steps; ++i) {
    sim.Step();
    out.insert(out.end(), sim.Events().begin(), sim.Events().end());
  }
  return out;
}

template <class E>
std::vector<E> OfType(std::span<const Event> events) {
  std::vector<E> out;
  for (const Event& e : events) {
    if (const E* x = std::get_if<E>(&e)) out.push_back(*x);
  }
  return out;
}

QuestProgress Record(const DataStore& d, const std::string& id, QuestStatus st) {
  const QuestDef* q = d.FindQuest(id);
  REQUIRE(q != nullptr);
  std::vector<int32_t> cur;
  for (const QuestObjectiveDef& o : q->objectives) cur.push_back(st == QuestStatus::Active ? 0 : o.required);
  return QuestProgress{id, st, cur};
}

// The angle of `p` around `c` in degrees, relative to `approachDeg`, in (-180, 180].
double ArcOffsetDeg(Vec2 p, Vec2 c, double approachDeg) {
  double a = std::atan2(p.y - c.y, p.x - c.x) * 180.0 / kPi - approachDeg;
  while (a > 180) a -= 360;
  while (a <= -180) a += 360;
  return a;
}

std::vector<std::string> DefIds(MonsterSystem& ms, const std::vector<EntityId>& ids) {
  std::vector<std::string> out;
  for (EntityId id : ids) {
    const MonsterInstance* m = ms.Find(id);
    REQUIRE(m != nullptr);
    out.push_back(m->def.id);
  }
  return out;
}

std::vector<std::string> Flatten(const std::vector<DefendWaveEntry>& wave) {
  std::vector<std::string> out;
  for (const DefendWaveEntry& e : wave) {
    for (int32_t i = 0; i < e.count; ++i) out.push_back(e.monsterId);
  }
  return out;
}

}  // namespace

// =====================================================================================================================
// Loaders: every K1 field round-trips; malformed hooks fail with file + path
// =====================================================================================================================

TEST_SUITE("data") {
  TEST_CASE("K1 loaders: only the remade chapter carries hooks; elsewhere the absent fields keep their defaults") {
    // The web-exported chapters (2-5) author none of the K1 fields: their records keep the loader defaults. The remade
    // Chapter 1 (TestContent.h ch1::kZone) uses them; content_ch1_test.cpp pins its values.
    const DataStore& d = test::RealData();
    for (const QuestDef& q : d.Quests().quests) {
      if (q.zone == test::ch1::kZone) continue;
      CAPTURE(q.id);
      CHECK(q.progressKey.empty());
      CHECK(q.rewards.choiceLabels.empty());
      CHECK(q.defendTarget.waves.empty());
      CHECK_FALSE(q.defendTarget.hasApproach);
    }
    for (const MapDef& m : d.World().maps) {
      if (m.id == test::ch1::kZone) continue;
      for (const SubDungeonEntranceDef& e : m.subDungeonEntrances) {
        CHECK_FALSE(e.sealed);
        CHECK(e.sealedTextKey.empty());
      }
    }
    // A focus `anim` only ever plays on a monster of the remade chapter (the web script has none).
    for (const Cutscene& c : d.Story().cutscenes) {
      for (const CutsceneStep& s : c.steps) {
        if (!s.hasAnim) continue;
        CAPTURE(c.id);
        CHECK(s.target.kind == StoryActorKind::Monster);
        CHECK(d.Monsters().FindForZone(test::ch1::kZone, s.target.id) != nullptr);
      }
    }
  }

  TEST_CASE("K1 loaders: progressKey, choiceLabels, defend waves + approach, sealed entrances, focus anim round-trip") {
    const Picks& p = P();
    const DataStore& d = HookData();
    CHECK(d.FindQuest(p.progressQuest)->progressKey == p.progressKey);
    CHECK(d.FindQuest(p.labelQuest)->rewards.choiceLabels == p.labels);
    const DefendTargetDef& dt = d.FindQuest(p.defendQuest)->defendTarget;
    REQUIRE(dt.waves.size() == p.waves.size());
    REQUIRE(dt.waves.size() == static_cast<size_t>(dt.totalWaves));
    for (size_t w = 0; w < dt.waves.size(); ++w) {
      REQUIRE(dt.waves[w].size() == p.waves[w].size());
      for (size_t i = 0; i < dt.waves[w].size(); ++i) {
        CHECK(dt.waves[w][i].monsterId == p.waves[w][i].monsterId);
        CHECK(dt.waves[w][i].count == p.waves[w][i].count);
      }
    }
    CHECK(dt.hasApproach);
    CHECK(dt.approachDeg == kApproachDeg);
    const MapDef* map = d.FindMap(p.zone);
    REQUIRE(map->subDungeonEntrances.size() == 2);
    const SubDungeonEntranceDef& sealed = map->subDungeonEntrances[0];
    CHECK(sealed.id == kSealedEntrance);
    CHECK(sealed.sealed);
    CHECK(sealed.sealedTextKey == kSealedToast);
    CHECK(sealed.targetSubDungeon == "fx_pit_later");  // a sealed entrance may name a later sub-dungeon
    CHECK_FALSE(map->subDungeonEntrances[1].sealed);
    const Cutscene* cs = d.Story().FindCutscene(p.bossCutscene);
    REQUIRE(cs != nullptr);
    const CutsceneStep& focus = cs->steps[static_cast<size_t>(p.bossFocusStep)];
    CHECK(focus.hasAnim);
    CHECK(focus.anim == AnimAction::Roar);
    CHECK(focus.target.id == kSynthBoss);
    CHECK(d.Monsters().ai.storyBossId == kSynthBoss);
  }

  TEST_CASE("K1 loaders: malformed hooks are refused with their file and path") {
    const Picks& p = P();
    const JsonValue story = ReadDataJson("story.json");
    // Steps the bad anims go on: a title step, a focus step on the player, two focus steps on monsters.
    std::string titleCs, playerCs;
    size_t titleAt = 0, playerAt = 0;
    std::vector<std::pair<std::string, size_t>> monsterFocus;
    for (const JsonMember& c : story.Get("cutscenes").Members()) {
      const std::span<const JsonValue> steps = c.value.Get("steps").Items();
      for (size_t i = 0; i < steps.size(); ++i) {
        const std::string_view kind = steps[i].Get("kind").AsString();
        if (kind == "title" && titleCs.empty()) {
          titleCs = c.key;
          titleAt = i;
        }
        if (kind == "focus" && steps[i].Get("target").AsString() == "player" && playerCs.empty()) {
          playerCs = c.key;
          playerAt = i;
        }
        if (kind == "focus" && steps[i].Get("target").IsObject() && steps[i].Get("target").Has("monster")) {
          monsterFocus.emplace_back(c.key, i);
        }
      }
    }
    REQUIRE_FALSE(titleCs.empty());
    REQUIRE_FALSE(playerCs.empty());
    REQUIRE(monsterFocus.size() >= 2);
    DataEdits bad;
    bad.emplace_back("quests.json", [&p](JsonValue& d) {
      JsonValue& quests = *At(d, {"quests"});
      ById(quests, p.progressQuest)->Set("progressKey", Str(""));
      At(*ById(quests, p.labelQuest), {"rewards"})->Set("choiceLabels", StrArray({p.labels.front()}));  // one short
      JsonValue waves = JsonValue::Array();
      waves.Append(WaveJson({{p.waves[0][0].monsterId, 0}}));  // one wave for totalWaves >= 2, and a count of 0
      At(*ById(quests, p.defendQuest), {"defendTarget"})->Set("waves", std::move(waves));
    });
    bad.emplace_back("story.json", [&](JsonValue& d) {
      At(d, {"cutscenes", titleCs, "steps"})->MutableItems()[titleAt].Set("anim", Str("roar"));
      At(d, {"cutscenes", playerCs, "steps"})->MutableItems()[playerAt].Set("anim", Str("roar"));
      const auto stepOf = [&d](const std::pair<std::string, size_t>& at) -> JsonValue& {
        return At(d, {"cutscenes", at.first, "steps"})->MutableItems()[at.second];
      };
      stepOf(monsterFocus[0]).Set("anim", Str("walk"));
      stepOf(monsterFocus[1]).Set("anim", Str("dance"));
    });
    bad.emplace_back("maps.json", [&p](JsonValue& d) {
      JsonValue list = JsonValue::Array();
      JsonValue noKey = JsonValue::Object();
      noKey.Set("id", Str("fx_a"));
      noKey.Set("name", Str("fx_a"));
      noKey.Set("col", JsonValue::Integer(p.sealedTile.col));
      noKey.Set("row", JsonValue::Integer(p.sealedTile.row));
      noKey.Set("targetSubDungeon", Str(p.openTarget));
      noKey.Set("sealed", JsonValue::Bool(true));
      JsonValue keyOnly = noKey;
      keyOnly.Set("id", Str("fx_b"));
      keyOnly.Set("sealed", JsonValue::Bool(false));
      keyOnly.Set("sealedTextKey", Str(kSealedToast));
      list.Append(std::move(noKey));
      list.Append(std::move(keyOnly));
      At(d, {"maps", p.zone})->Set("subDungeonEntrances", std::move(list));
    });
    DataLoadReport r;
    LoadEdited(bad, r);
    INFO(r.Summary(30));
    CHECK_FALSE(r.Ok());
    CHECK(HasIssue(r, "quests.json", "progressKey", "empty progress key"));
    CHECK(HasIssue(r, "quests.json", "rewards.choiceLabels", "one key per choices slot"));
    CHECK(HasIssue(r, "quests.json", "defendTarget.waves", "one composition per wave"));
    CHECK(HasIssue(r, "quests.json", "defendTarget.waves", "count >= 1"));
    CHECK(HasIssue(r, "story.json", "anim", "only valid on a focus step"));
    CHECK(HasIssue(r, "story.json", "anim", "needs a {monster} focus target"));
    CHECK(HasIssue(r, "story.json", "anim", "not idle / walk / death"));
    CHECK(HasIssue(r, "story.json", "anim", "unknown value 'dance'"));
    CHECK(HasIssue(r, "maps.json", "sealedTextKey", "needs its sealedTextKey"));
    CHECK(HasIssue(r, "maps.json", "sealedTextKey", "not sealed"));
  }

  TEST_CASE("K1 Finalize: hook keys exist in zh-CN and en, wave monsters exist, sealed entrances may point ahead") {
    const Picks& p = P();
    DataEdits bad;
    bad.emplace_back("quests.json", [&p](JsonValue& d) {
      JsonValue& quests = *At(d, {"quests"});
      ById(quests, p.progressQuest)->Set("progressKey", Str("fx.k1.missing.progress"));
      std::vector<std::string> labels = p.labels;
      labels.back() = "fx.k1.missing.label";
      At(*ById(quests, p.labelQuest), {"rewards"})->Set("choiceLabels", StrArray(labels));
      std::vector<std::vector<DefendWaveEntry>> waves = p.waves;
      waves.back().push_back({"fx_nobody", 1});
      JsonValue arr = JsonValue::Array();
      for (const auto& w : waves) arr.Append(WaveJson(w));
      At(*ById(quests, p.defendQuest), {"defendTarget"})->Set("waves", std::move(arr));
    });
    bad.emplace_back("maps.json", [&p](JsonValue& d) {
      JsonValue list = JsonValue::Array();
      JsonValue sealed = JsonValue::Object();
      sealed.Set("id", Str("fx_a"));
      sealed.Set("name", Str("fx_a"));
      sealed.Set("col", JsonValue::Integer(p.sealedTile.col));
      sealed.Set("row", JsonValue::Integer(p.sealedTile.row));
      sealed.Set("targetSubDungeon", Str("fx_later"));
      sealed.Set("sealed", JsonValue::Bool(true));
      sealed.Set("sealedTextKey", Str("fx.k1.missing.toast"));
      JsonValue open = JsonValue::Object();
      open.Set("id", Str("fx_b"));
      open.Set("name", Str("fx_b"));
      open.Set("col", JsonValue::Integer(p.openTile.col));
      open.Set("row", JsonValue::Integer(p.openTile.row));
      open.Set("targetSubDungeon", Str("fx_nowhere"));
      list.Append(std::move(sealed));
      list.Append(std::move(open));
      At(d, {"maps", p.zone})->Set("subDungeonEntrances", std::move(list));
    });
    DataLoadReport r;
    LoadEdited(bad, r);
    INFO(r.Summary(30));
    CHECK_FALSE(r.Ok());
    CHECK(HasIssue(r, "quests.json", "progressKey", "fx.k1.missing.progress"));
    CHECK(HasIssue(r, "quests.json", "rewards.choiceLabels", "fx.k1.missing.label"));
    CHECK(HasIssue(r, "quests.json", "defendTarget.waves[", "unknown monster 'fx_nobody'"));
    CHECK(HasIssue(r, "maps.json", "sealedTextKey", "fx.k1.missing.toast"));
    size_t unknownSub = 0;
    for (const DataIssue& e : r.errors) unknownSub += e.message == "unknown sub-dungeon" ? 1 : 0;
    CHECK(unknownSub == 1);  // the unsealed entrance only; the sealed one may point at a later sub-dungeon
  }
}

// =====================================================================================================================
// Defend waves: composition per wave, the approach arc, the M5 geometry, determinism, the quest wiring
// =====================================================================================================================

TEST_SUITE("monsters") {
  TEST_CASE("K1 SpawnDefendWave: a composition spawns exactly its list, in order, scaled per wave, without RNG") {
    MonsterRig rig(61, Vec2(100, 100));
    rig.OpenWorld();
    const std::vector<std::string>& zoneIds = MD().Monsters().ZoneList(test::ch1::kZone)->monsterIds;
    REQUIRE(zoneIds.size() >= 2);
    const std::vector<DefendWaveEntry> wave{{zoneIds[0], 2}, {zoneIds[1], 1}};
    const RngState before = rig.h.rng.Get(RngStream::Ai).GetState();
    DefendWaveSpec spec;
    spec.composition = wave;
    const std::vector<EntityId> ids = rig.monsters.SpawnDefendWave(Vec2(60, 60), 1, spec);
    CHECK(rig.h.rng.Get(RngStream::Ai).GetState() == before);  // no draw: the list decides
    REQUIRE(ids.size() == 3);
    CHECK(DefIds(rig.monsters, ids) == std::vector<std::string>{zoneIds[0], zoneIds[0], zoneIds[1]});
    for (size_t i = 0; i < ids.size(); ++i) {
      const MonsterInstance& m = rig.M(ids[i]);
      const MonsterDef expect = ScaleDefendWave(
          ScaleMonsterForDifficulty(MDef(m.def.id), MDiff(), Difficulty::Normal), 1, MAi());
      CHECK(m.maxHp == expect.hp);
      CHECK(m.def.damage == expect.damage);
      CHECK(m.role == MonsterRole::DefendWave);
      CHECK(m.state == MonsterState::Chase);
      CHECK(m.noRespawn);
      CHECK(m.affixes.empty());
      CHECK(Dist(m.pos, Vec2(60, 60)) == doctest::Approx(MAi().defendWaveRadius).epsilon(0.1));
    }
    // Without an approach the ring is the web's: 2 pi k / n.
    CHECK(rig.M(ids[0]).pos == Vec2(68, 60));
    // The web rule still applies without a composition (n = base + w random picks: the Ai stream advances).
    const std::vector<EntityId> web = rig.monsters.SpawnDefendWave(Vec2(60, 60), 1);
    CHECK(web.size() == static_cast<size_t>(MAi().defendWaveBaseCount + 1));
    CHECK_FALSE(rig.h.rng.Get(RngStream::Ai).GetState() == before);
  }

  TEST_CASE("K1 SpawnDefendWave: an approach puts every monster on the half ring facing it, evenly") {
    MonsterRig rig(62, Vec2(100, 100));
    rig.OpenWorld();
    const std::string& id = MD().Monsters().ZoneList(test::ch1::kZone)->monsterIds.front();
    const std::vector<DefendWaveEntry> wave{{id, 4}};
    DefendWaveSpec spec;
    spec.composition = wave;
    spec.hasApproach = true;
    spec.approachDeg = 90;  // +row
    const std::vector<EntityId> ids = rig.monsters.SpawnDefendWave(Vec2(60, 60), 0, spec);
    REQUIRE(ids.size() == 4);
    // 90 + 180 ((k + .5) / 4 - .5) = 22.5, 67.5, 112.5, 157.5 deg at r 8, rounded.
    const std::vector<Vec2> expect{Vec2(67, 63), Vec2(63, 67), Vec2(57, 67), Vec2(53, 63)};
    for (size_t i = 0; i < ids.size(); ++i) {
      CAPTURE(i);
      CHECK(rig.M(ids[i]).pos == expect[i]);
      CHECK(std::fabs(ArcOffsetDeg(rig.M(ids[i]).pos, Vec2(60, 60), 90)) < 90);
    }
    // The random picks keep the arc too, for any count and approach.
    for (const double approach : {0.0, 45.0, 200.0, -30.0}) {
      for (int32_t w = 0; w < 4; ++w) {
        DefendWaveSpec arc;
        arc.hasApproach = true;
        arc.approachDeg = approach;
        for (EntityId e : rig.monsters.SpawnDefendWave(Vec2(60, 60), w, arc)) {
          CAPTURE(approach);
          CAPTURE(w);
          CHECK(std::fabs(ArcOffsetDeg(rig.M(e).pos, Vec2(60, 60), approach)) < 90);
        }
      }
    }
  }

  TEST_CASE("K1 SpawnDefendWave: the M5 geometry keeps every wave out of the camp's safe radius") {
    // chapter1.md M5: the Boneyard Gate (10,32), 18 tiles from camp 1, waves on the half ring pointing away from the
    // camp (r 8), wave sizes 5 / 6 / 5. Whatever the remake's monsters, no spawn may stand in the camp's safe zone.
    MonsterRig rig(63, Vec2(60, 60));
    REQUIRE(rig.ok);
    const Vec2 gate(10, 32);
    const Vec2 camp = test::ch1::kCamp.Center();
    const double approach = std::atan2(gate.y - camp.y, gate.x - camp.x) * 180.0 / kPi;
    const std::vector<std::string>& zoneIds = MD().Monsters().ZoneList(test::ch1::kZone)->monsterIds;
    const std::vector<std::vector<DefendWaveEntry>> waves{
        {{zoneIds[0], 5}}, {{zoneIds[0], 4}, {zoneIds[1], 2}}, {{zoneIds[1], 2}, {zoneIds[0], 3}}};
    for (size_t w = 0; w < waves.size(); ++w) {
      DefendWaveSpec spec;
      spec.composition = waves[w];
      spec.hasApproach = true;
      spec.approachDeg = approach;
      const std::vector<EntityId> ids = rig.monsters.SpawnDefendWave(gate, static_cast<int32_t>(w), spec);
      CHECK(ids.size() == Flatten(waves[w]).size());
      for (EntityId id : ids) {
        const Vec2 pos = rig.M(id).pos;
        CAPTURE(w);
        CAPTURE(pos.x);
        CAPTURE(pos.y);
        CHECK_FALSE(rig.zone.InSafeZone(pos));
        CHECK(Dist(pos, camp) > rig.zone.SafeZoneRadius());
        CHECK(Dist(pos, camp) > Dist(gate, camp) - 1);  // the far side of the gate
      }
    }
  }

  TEST_CASE("K1 SpawnDefendWave: the same spec and seed give the same wave") {
    const std::vector<std::string>& zoneIds = MD().Monsters().ZoneList(test::ch1::kZone)->monsterIds;
    const std::vector<DefendWaveEntry> wave{{zoneIds[1], 1}, {zoneIds[0], 3}};
    const auto run = [&](uint64_t seed) {
      MonsterRig rig(seed, Vec2(100, 100));
      rig.OpenWorld();
      std::vector<std::pair<std::string, Vec2>> out;
      for (int32_t w = 0; w < 3; ++w) {
        DefendWaveSpec spec;
        spec.composition = wave;
        spec.hasApproach = w > 0;
        spec.approachDeg = 33.0 * w;
        for (EntityId id : rig.monsters.SpawnDefendWave(Vec2(40, 70), w, spec)) {
          out.emplace_back(rig.M(id).def.id, rig.M(id).pos);
        }
      }
      return out;
    };
    const auto a = run(64), b = run(64), c = run(65);
    CHECK(a.size() == 12);
    CHECK(a == b);
    CHECK(a == c);  // a composed wave draws nothing, so even the seed does not move it
  }

  TEST_CASE("K1 defend quest wiring: each wave spawns the quest's composition on its approach arc") {
    const Picks& p = P();
    const DataStore& d = HookData();
    const QuestDef* q = d.FindQuest(p.defendQuest);
    std::unique_ptr<GameSim> sim = StartOn(d, 21);
    SaveData save;
    sim->BuildSave(save, 0);
    const Vec2 target = q->defendTarget.pos.Center();
    save.player.currentMap = p.defendZone;
    save.player.tileCol = q->defendTarget.pos.col + 1;
    save.player.tileRow = q->defendTarget.pos.row + 1;
    save.quests = {Record(d, p.defendQuest, QuestStatus::Active)};
    save.hasStorySeen = true;
    save.storySeen = {"prologue", "chapter_" + p.zone, "chapter_" + p.defendZone};
    std::string err;
    REQUIRE(sim->LoadGame(save, &err) == SaveError::None);
    SimContext& ctx = sim->Context();
    ctx.sys.story->FinishAllBeats();
    REQUIRE(ctx.sys.questWorld->Defend().active);
    for (size_t w = 0; w < p.waves.size(); ++w) {
      CAPTURE(w);
      int guard = 0;
      while (!ctx.sys.questWorld->Defend().waveActive && ++guard < 400) sim->Step();
      REQUIRE(ctx.sys.questWorld->Defend().waveActive);
      const std::vector<EntityId> ids = ctx.sys.questWorld->Defend().waveMonsters;
      CHECK(DefIds(*ctx.sys.monsters, ids) == Flatten(p.waves[w]));
      for (EntityId id : ids) {
        const MonsterInstance* m = ctx.sys.monsters->Find(id);
        CHECK(std::fabs(ArcOffsetDeg(m->pos, target, kApproachDeg)) < 90);
        CHECK(m->role == MonsterRole::DefendWave);
      }
      for (EntityId id : ids) {
        DamageFlags f;
        f.attacker = kHeroEntityId;
        f.source = KillSource::HeroBasic;
        ctx.sys.monsters->ApplyDamage(id, 1e9, f);
      }
      ctx.sys.story->FinishAllBeats();
      sim->Step();
      const QuestProgress* pr = ctx.sys.quests->Progress(p.defendQuest);
      REQUIRE(pr != nullptr);
      CHECK(pr->objectives[0] == static_cast<int32_t>(w + 1));
    }
    bool has = false;
    CHECK(ctx.sys.quests->StatusOf(p.defendQuest, has) == QuestStatus::Completed);
  }
}

// =====================================================================================================================
// Monster anim actions: the enum, the mini-boss Taunt hold
// =====================================================================================================================

TEST_SUITE("monsters") {
  TEST_CASE("K1 AnimAction: the monster actions are appended with their data spellings") {
    CHECK(static_cast<int>(AnimAction::Talk) == 9);  // earlier values keep their numbers
    const std::vector<std::pair<AnimAction, std::string_view>> added{
        {AnimAction::Roar, "roar"},         {AnimAction::Taunt, "taunt"},   {AnimAction::Stun, "stun"},
        {AnimAction::Attack02, "attack02"}, {AnimAction::Cast02, "cast02"}, {AnimAction::Rise, "rise"}};
    for (const auto& [a, name] : added) {
      CHECK(EnumName(a) == name);
      AnimAction back = AnimAction::Idle;
      CHECK(ParseEnum(name, back));
      CHECK(back == a);
    }
    CHECK(EnumCount<AnimAction>() == 16);
  }

  TEST_CASE("K1 Taunt: the mini-boss taunts while its pre-fight dialogue holds it, and stops when dismissed") {
    MonsterRig rig(66, Vec2(110, 110));
    rig.OpenWorld();
    rig.monsters.SpawnMiniBoss();
    const EntityId boss = rig.monsters.MiniBoss();
    REQUIRE(boss != kNoEntity);
    REQUIRE(MD().Monsters().MiniBossDialogue(rig.M(boss).def.id) != nullptr);
    const auto holdOf = [&rig](EntityId id) {
      Snapshot snap;
      rig.monsters.FillSnapshot(snap);
      for (const MonsterView& v : snap.monsters) {
        if (v.id == id) return v.holdAction;
      }
      FAIL("no view");
      return AnimAction::Idle;
    };
    CHECK(holdOf(boss) == AnimAction::Idle);
    rig.h.events.Clear();
    const Vec2 heroAt = rig.M(boss).pos + Vec2(0, 1);
    rig.hero->SetPosition(heroAt);
    rig.Step();
    REQUIRE(rig.monsters.MiniBossDialogueActive());
    std::vector<EvPlayAnim> taunts;
    for (const EvPlayAnim& a : MEvents<EvPlayAnim>(rig.h.events)) {
      if (a.action == AnimAction::Taunt) taunts.push_back(a);
    }
    REQUIRE(taunts.size() == 1);
    CHECK(taunts[0].entity == boss);
    CHECK(taunts[0].hasFaceTarget);
    CHECK(taunts[0].faceTarget == heroAt);
    CHECK(holdOf(boss) == AnimAction::Taunt);
    rig.h.events.Clear();
    rig.Step(30);
    CHECK(holdOf(boss) == AnimAction::Taunt);  // held: still taunting, no new event
    CHECK(MEvents<EvPlayAnim>(rig.h.events).empty());
    for (const MonsterInstance& m : rig.monsters.All()) {
      if (m.id != boss) CHECK(holdOf(m.id) == AnimAction::Idle);
    }
    rig.monsters.DismissMiniBossDialogue();
    CHECK(holdOf(boss) == AnimAction::Idle);
  }
}

// =====================================================================================================================
// The story boss comes from data (M7 with a synthetic id); its intro's focus step plays Roar on it
// =====================================================================================================================

TEST_SUITE("monsters") {
  TEST_CASE("K1 story boss config: M7 runs on a synthetic boss id (once per visit, not after its quest, farmable)") {
    const Picks& p = P();
    const DataStore& d = HookData();
    const MonsterAiDef& ai = d.Monsters().ai;
    REQUIRE(ai.storyBossId == kSynthBoss);
    HookMonsterRig rig(d, p.zone, 67, Vec2(60, 60));
    REQUIRE(rig.ok);
    rig.monsters.SpawnZonePopulation();
    std::vector<const MonsterInstance*> bosses = rig.AliveOf(kSynthBoss);
    REQUIRE(bosses.size() == 1);
    CHECK(bosses[0]->role == MonsterRole::StoryBoss);
    CHECK(bosses[0]->noRespawn);
    CHECK(rig.AliveOf(p.storyBoss).empty());  // the spawn entry was re-pointed: nothing keys off the old id
    for (const MonsterInstance& m : rig.monsters.All()) {
      if (m.def.id != kSynthBoss) CHECK(m.role != MonsterRole::StoryBoss);
    }
    // Killed: it never respawns within the visit.
    DamageFlags f;
    f.attacker = kHeroEntityId;
    f.source = KillSource::HeroBasic;
    rig.monsters.ApplyDamage(bosses[0]->id, 1e9, f);
    CHECK(rig.h.timers.Empty());  // no respawn timer
    rig.h.Step(static_cast<int>(ai.respawnDelayMs / kSimStepMs) + 600);
    CHECK(rig.AliveOf(kSynthBoss).empty());
    // Its quest turned in: the next visit has no boss.
    REQUIRE_FALSE(ai.storyBossNotAfterQuestTurnIn.empty());
    rig.quests.Load({Record(d, ai.storyBossNotAfterQuestTurnIn, QuestStatus::TurnedIn)});
    rig.monsters.SpawnZonePopulation();
    CHECK(rig.AliveOf(kSynthBoss).empty());
    // The chapter complete too: farmable again (once per visit), when the data says so.
    REQUIRE_FALSE(ai.chapterCompleteQuest.empty());
    rig.quests.Load({Record(d, ai.storyBossNotAfterQuestTurnIn, QuestStatus::TurnedIn),
                     Record(d, ai.chapterCompleteQuest, QuestStatus::TurnedIn)});
    rig.monsters.SpawnZonePopulation();
    CHECK(rig.AliveOf(kSynthBoss).size() == (ai.storyBossFarmableAfterChapter ? 1u : 0u));
  }
}

TEST_SUITE("monsters") {
  TEST_CASE("M7 amendment: a story boss killed before its quest comes back when the quest needs it (synthetic id)") {
    const Picks& p = P();
    const DataStore& d = HookData();
    const MonsterAiDef& ai = d.Monsters().ai;
    REQUIRE(ai.storyBossId == kSynthBoss);
    const MapDef* map = d.FindMap(p.zone);
    REQUIRE(map != nullptr);
    std::vector<TilePos> anchors;
    for (const MapSpawnDef& sp : map->spawns) {
      if (sp.monsterId == kSynthBoss) anchors.push_back(sp.pos);
    }
    REQUIRE(anchors.size() == 1);
    HookMonsterRig rig(d, p.zone, 71, Vec2(60, 60));
    REQUIRE(rig.ok);
    rig.monsters.SpawnZonePopulation();
    REQUIRE(rig.AliveOf(kSynthBoss).size() == 1);
    CHECK(rig.monsters.SpawnStoryBossForQuests().empty());  // alive: nothing to do
    DamageFlags f;
    f.attacker = kHeroEntityId;
    f.source = KillSource::HeroBasic;
    const auto killBoss = [&] {
      for (const MonsterInstance* m : rig.AliveOf(kSynthBoss)) rig.monsters.ApplyDamage(m->id, 1e9, f);
      REQUIRE(rig.AliveOf(kSynthBoss).empty());
    };
    killBoss();
    CHECK(rig.monsters.SpawnStoryBossForQuests().empty());  // dead, but no quest needs it: once per visit holds
    // Its quest is accepted (the kill objective open): it comes back once, at its anchor, as the story boss.
    rig.quests.Load({Record(d, p.bossQuest, QuestStatus::Active)});
    const std::vector<EntityId> back = rig.monsters.SpawnStoryBossForQuests();
    REQUIRE(back.size() == 1);
    const MonsterInstance* boss = rig.monsters.Find(back[0]);
    REQUIRE(boss != nullptr);
    CHECK(boss->def.id == kSynthBoss);
    CHECK(boss->role == MonsterRole::StoryBoss);
    CHECK(boss->noRespawn);
    CHECK(Dist(boss->pos, anchors[0].Center()) < 1.0);
    CHECK(rig.monsters.SpawnStoryBossForQuests().empty());  // alive: no second one
    CHECK(rig.AliveOf(kSynthBoss).size() == 1);
    // Killed for the quest (objective done): nothing more.
    killBoss();
    rig.quests.Load({Record(d, p.bossQuest, QuestStatus::Completed)});
    CHECK(rig.monsters.SpawnStoryBossForQuests().empty());
    // Turned in: the boss stays away (StoryBossBlocked), whatever else is open.
    rig.quests.Load({Record(d, p.bossQuest, QuestStatus::TurnedIn)});
    CHECK(rig.monsters.SpawnStoryBossForQuests().empty());
  }

  TEST_CASE("M7 amendment: accepting the story boss's quest through the sim brings the dead boss back (wiring)") {
    const Picks& p = P();
    const DataStore& d = HookData();
    std::unique_ptr<GameSim> sim = StartOn(d, 29);
    SimContext& ctx = sim->Context();
    EntityId boss = ctx.sys.monsters->NearestAliveOfDef(kSynthBoss, ctx.sys.hero->Position());
    REQUIRE(boss != kNoEntity);
    DamageFlags f;
    f.attacker = kHeroEntityId;
    f.source = KillSource::HeroBasic;
    ctx.sys.monsters->ApplyDamage(boss, 1e9, f);
    sim->Step();
    ctx.sys.story->FinishAllBeats();
    CHECK(ctx.sys.monsters->NearestAliveOfDef(kSynthBoss, ctx.sys.hero->Position()) == kNoEntity);
    // The guarded kill beat did not play: its quest was not open.
    CHECK_FALSE(ctx.sys.story->Progress().Has(p.bossKillCutscene));
    // Every prerequisite turned in, then the quest accepted: QuestAcceptedMsg -> SpawnStoryBossForQuests.
    std::vector<QuestProgress> prereqs;
    for (const std::string& pre : d.FindQuest(p.bossQuest)->prereqQuests) {
      prereqs.push_back(Record(d, pre, QuestStatus::TurnedIn));
    }
    ctx.sys.quests->Load(prereqs);
    REQUIRE(ctx.sys.quests->Accept(p.bossQuest));
    sim->Step();
    boss = ctx.sys.monsters->NearestAliveOfDef(kSynthBoss, ctx.sys.hero->Position());
    REQUIRE(boss != kNoEntity);
    CHECK(ctx.sys.monsters->Find(boss)->role == MonsterRole::StoryBoss);
    // The kill that counts plays the guarded beat and completes the objective.
    ctx.sys.monsters->ApplyDamage(boss, 1e9, f);
    sim->Step();
    ctx.sys.story->FinishAllBeats();
    CHECK(ctx.sys.story->Progress().Has(p.bossKillCutscene));
    const QuestProgress* qp = ctx.sys.quests->Progress(p.bossQuest);
    REQUIRE(qp != nullptr);
    CHECK(qp->status == QuestStatus::Completed);
  }
}

TEST_SUITE("monsters") {
  // R2 (final review): with oncePerVisit off the story boss is a Regular whose death queues a respawn; accepting his
  // quest in between must not spawn a second one (SpawnStoryBossForQuests leaves a pending respawn to itself).
  TEST_CASE("M7 amendment, oncePerVisit off: kill, accept his quest, past respawnDelayMs -> exactly one boss alive") {
    const Picks& p = P();
    const DataStore& d = RespawningBossData();
    const MonsterAiDef& ai = d.Monsters().ai;
    REQUIRE(ai.storyBossId == kSynthBoss);
    REQUIRE_FALSE(ai.storyBossOncePerVisit);
    std::unique_ptr<GameSim> sim = StartOn(d, 29);
    SimContext& ctx = sim->Context();
    const auto aliveBosses = [&ctx] {
      size_t n = 0;
      for (const MonsterInstance& m : ctx.sys.monsters->All()) n += m.IsAlive() && m.def.id == kSynthBoss ? 1 : 0;
      return n;
    };
    REQUIRE(aliveBosses() == 1);
    const EntityId boss = ctx.sys.monsters->NearestAliveOfDef(kSynthBoss, ctx.sys.hero->Position());
    REQUIRE(boss != kNoEntity);
    CHECK(ctx.sys.monsters->Find(boss)->role == MonsterRole::Regular);
    CHECK_FALSE(ctx.sys.monsters->Find(boss)->noRespawn);
    DamageFlags f;
    f.attacker = kHeroEntityId;
    f.source = KillSource::HeroBasic;
    ctx.sys.monsters->ApplyDamage(boss, 1e9, f);
    sim->Step();
    ctx.sys.story->FinishAllBeats();
    REQUIRE(aliveBosses() == 0);
    // Every prerequisite turned in, then the quest accepted (QuestAcceptedMsg -> SpawnStoryBossForQuests): the
    // pending respawn brings him back, the accept adds none.
    std::vector<QuestProgress> prereqs;
    for (const std::string& pre : d.FindQuest(p.bossQuest)->prereqQuests) {
      prereqs.push_back(Record(d, pre, QuestStatus::TurnedIn));
    }
    ctx.sys.quests->Load(prereqs);
    REQUIRE(ctx.sys.quests->Accept(p.bossQuest));
    sim->Step();
    CHECK(aliveBosses() == 0);
    CHECK(ctx.sys.monsters->SpawnStoryBossForQuests().empty());
    const int steps = static_cast<int>(ai.respawnDelayMs / kSimStepMs) + 120;
    for (int i = 0; i < steps; ++i) {
      sim->Step();
      ctx.sys.story->FinishAllBeats();
    }
    CHECK(aliveBosses() == 1);
    CHECK(ctx.sys.monsters->SpawnStoryBossForQuests().empty());  // alive: still one
    CHECK(aliveBosses() == 1);
  }
}

TEST_SUITE("data") {
  TEST_CASE("story trigger whileQuest: loaded on a monster_killed trigger, refused elsewhere or unknown") {
    const Picks& p = P();
    {
      const DataStore& d = HookData();
      bool found = false;
      for (const StoryTriggerDef& t : d.Story().triggers) {
        if (t.on == StoryTriggerOn::MonsterKilled && t.subjectId == kSynthBoss && t.cutscene == p.bossKillCutscene) {
          found = true;
          CHECK(t.whileQuest == p.bossQuest);
        }
        if (t.on != StoryTriggerOn::MonsterKilled) CHECK(t.whileQuest.empty());
      }
      CHECK(found);
    }
    const auto refused = [](std::function<void(JsonValue&)> edit, const std::string& what) {
      DataEdits e;
      e.emplace_back("story.json", std::move(edit));
      DataLoadReport report;
      LoadEdited(e, report);
      INFO(report.Summary(20));
      CHECK_FALSE(report.Ok());
      CHECK(report.Summary(50).find(what) != std::string::npos);
    };
    refused([&p](JsonValue& d) {
      for (JsonValue& t : At(d, {"triggers"})->MutableItems()) {
        if (t.Get("on").AsString() == "quest_turned_in") {
          t.Set("whileQuest", Str(p.progressQuest));
          break;
        }
      }
    }, "whileQuest");
    refused([](JsonValue& d) {
      for (JsonValue& t : At(d, {"triggers"})->MutableItems()) {
        if (t.Get("on").AsString() == "monster_killed") {
          t.Set("whileQuest", Str("q_fx_no_such_quest"));
          break;
        }
      }
    }, "q_fx_no_such_quest");
  }
}

TEST_SUITE("story") {
  TEST_CASE("K1 story focus anim: the boss intro's focus step plays Roar on the focused (synthetic) boss") {
    const DataStore& d = HookData();
    std::unique_ptr<GameSim> sim = StartOn(d, 23);
    SimContext& ctx = sim->Context();
    const EntityId boss = ctx.sys.monsters->NearestAliveOfDef(kSynthBoss, ctx.sys.hero->Position());
    REQUIRE(boss != kNoEntity);
    const Vec2 bossAt = ctx.sys.monsters->Find(boss)->pos;
    ctx.sys.hero->SetPosition(bossAt + Vec2(0, -6));  // inside the boss sight (9 tiles)
    int guard = 0;
    while (!ctx.sys.story->Playback().playing && ++guard < 600) sim->Step();
    REQUIRE(ctx.sys.story->Playback().playing);
    CHECK(ctx.sys.story->Playback().beat.kind == StoryBeatKind::BossIntro);
    std::vector<Event> events;
    sim->ClearEvents();
    for (int i = 0; i < 400 && ctx.sys.story->Playback().playing; ++i) {
      if (ctx.sys.story->Playback().waitingForInput) ctx.sys.story->Advance();
      sim->AdvanceRealTime(50);
      events.insert(events.end(), sim->Events().begin(), sim->Events().end());
      sim->ClearEvents();
    }
    const std::vector<EvPlayAnim> roars = [&] {
      std::vector<EvPlayAnim> out;
      for (const EvPlayAnim& a : OfType<EvPlayAnim>(events)) {
        if (a.action == AnimAction::Roar) out.push_back(a);
      }
      return out;
    }();
    REQUIRE(roars.size() == 1);
    CHECK(roars[0].entity == boss);
    // It follows the focus step's EvStoryStep (the camera has its target first).
    size_t stepAt = events.size(), roarAt = events.size();
    for (size_t i = 0; i < events.size(); ++i) {
      const EvStoryStep* s = std::get_if<EvStoryStep>(&events[i]);
      if (s != nullptr && s->index == P().bossFocusStep && stepAt == events.size()) {
        stepAt = i;
        CHECK(s->hasFocus);
        CHECK(s->focusEntity == boss);
        CHECK(s->step.hasAnim);
      }
      if (const EvPlayAnim* a = std::get_if<EvPlayAnim>(&events[i]); a && a->action == AnimAction::Roar) roarAt = i;
    }
    CHECK(stepAt < roarAt);
    CHECK(roarAt == stepAt + 1);
  }
}

// =====================================================================================================================
// Quest progress line and labelled reward choices on the giver's card
// =====================================================================================================================

TEST_SUITE("quests") {
  TEST_CASE("K1 progress line: the giver's card carries an active quest's progressKey, and opens for it alone") {
    const Picks& p = P();
    const DataStore& d = HookData();
    std::unique_ptr<GameSim> sim = StartOn(d, 31);
    QuestWorld& qw = *sim->Context().sys.questWorld;
    QuestSystem& qs = *sim->Context().sys.quests;
    // Offered, not active: no line.
    REQUIRE(qw.OpenCard(test::ch1::kElder));
    CHECK(qw.Card().progress.empty());
    // Accepted: the next card shows its line beside the other offers.
    REQUIRE(qw.AcceptFromCard(p.progressQuest));
    REQUIRE(qw.OpenCard(test::ch1::kElder));
    REQUIRE(qw.Card().progress.size() == 1);
    CHECK(qw.Card().progress[0].quest == d.FindQuest(p.progressQuest));
    CHECK(qw.Card().progress[0].progressKey == p.progressKey);
    for (const QuestCardEntry& e : qw.Card().entries) CHECK(e.quest->id != p.progressQuest);
    qw.CloseCard();
    // Every other quest of the giver turned in: the card opens for the line alone (talking to the giver).
    std::vector<QuestProgress> records;
    for (const std::string& id : d.FindNpc(test::ch1::kElder)->quests) {
      records.push_back(Record(d, id, id == p.progressQuest ? QuestStatus::Active : QuestStatus::TurnedIn));
    }
    qs.Load(records);
    qw.InteractNpc(test::ch1::kElder);
    CHECK(qw.Card().open);
    CHECK(qw.Card().entries.empty());
    REQUIRE(qw.Card().progress.size() == 1);
    CHECK(qw.Card().progress[0].progressKey == p.progressKey);
    CHECK_FALSE(sim->Context().sys.dialogue->View().open);
    qw.CloseCard();
    // Completed: a turn-in entry instead of the line.
    for (QuestProgress& r : records) {
      if (r.questId == p.progressQuest) r = Record(d, p.progressQuest, QuestStatus::Completed);
    }
    qs.Load(records);
    REQUIRE(qw.OpenCard(test::ch1::kElder));
    CHECK(qw.Card().progress.empty());
    REQUIRE(qw.Card().entries.size() == 1);
    CHECK(qw.Card().entries[0].turnIn);
  }

  TEST_CASE("K1 progress line: without a progressKey the giver keeps today's fallback (no card for an active quest)") {
    const Picks& p = P();
    const DataStore& d = NoProgressKeyData();
    REQUIRE(d.FindQuest(p.progressQuest)->progressKey.empty());
    std::unique_ptr<GameSim> sim = StartOn(d, 31);
    QuestWorld& qw = *sim->Context().sys.questWorld;
    std::vector<QuestProgress> records;
    for (const std::string& id : d.FindNpc(test::ch1::kElder)->quests) {
      records.push_back(Record(d, id, id == p.progressQuest ? QuestStatus::Active : QuestStatus::TurnedIn));
    }
    sim->Context().sys.quests->Load(records);
    CHECK_FALSE(qw.OpenCard(test::ch1::kElder));
    qw.InteractNpc(test::ch1::kElder);
    CHECK_FALSE(qw.Card().open);
  }

  TEST_CASE("K1 labelled choices: the turn-in card labels each generated item by its slot") {
    const Picks& p = P();
    const DataStore& d = HookData();
    std::unique_ptr<GameSim> sim = StartOn(d, 33);
    QuestWorld& qw = *sim->Context().sys.questWorld;
    sim->Context().sys.quests->Load({Record(d, p.labelQuest, QuestStatus::Completed)});
    REQUIRE(qw.OpenCard(test::ch1::kElder));
    const QuestCardEntry* entry = nullptr;
    for (const QuestCardEntry& e : qw.Card().entries) {
      if (e.turnIn && e.quest->id == p.labelQuest) entry = &e;
    }
    REQUIRE(entry != nullptr);
    REQUIRE(entry->choices.size() == p.labels.size());  // every slot yields an item for the hero
    CHECK(entry->choiceLabels == p.labels);              // label i = choiceLabels[slot of item i]
    for (const QuestCardEntry& e : qw.Card().entries) {
      if (&e != entry) CHECK(e.choiceLabels.empty());
    }
    // The cached choices keep their labels on a refresh.
    REQUIRE(qw.OpenCard(test::ch1::kElder));
    for (const QuestCardEntry& e : qw.Card().entries) {
      if (e.turnIn && e.quest->id == p.labelQuest) CHECK(e.choiceLabels == p.labels);
    }
  }

  TEST_CASE("K1 labelled choices: a slot that yields no item drops its label with it") {
    QuestRewardDef r;
    r.choices = {RewardSlot::Weapon, RewardSlot::Belt, RewardSlot::Jewelry};
    r.choiceLabels = {"fx.label.keep", "fx.label.forge", "fx.label.burn"};
    const std::vector<int32_t> slots{0, 2};  // the belt slot produced nothing
    CHECK(QuestRewardChoiceLabels(r, slots) == std::vector<std::string>{"fx.label.keep", "fx.label.burn"});
    CHECK(QuestRewardChoiceLabels(r, std::vector<int32_t>{5}) == std::vector<std::string>{""});
    r.choiceLabels.clear();
    CHECK(QuestRewardChoiceLabels(r, slots).empty());  // unlabelled quest
    // GenerateQuestRewardChoices reports the slot of every item it returns.
    const DataStore& d = test::RealData();
    const QuestDef* q = d.FindQuest(P().labelQuest);
    Rng rng(5);
    ItemUidGenerator uids;
    const LootContext lc{&d, &rng, &uids};
    std::vector<int32_t> got;
    const std::vector<ItemInstance> items = GenerateQuestRewardChoices(lc, *q, ClassId::Mage, 3, &got);
    REQUIRE(got.size() == items.size());
    for (size_t i = 0; i < got.size(); ++i) CHECK(got[i] == static_cast<int32_t>(i));
  }
}

// =====================================================================================================================
// Sealed sub-dungeon entrance: a toast, nothing else
// =====================================================================================================================

TEST_SUITE("world") {
  TEST_CASE("K1 sealed entrance: interact logs its sealedTextKey as a toast; no zone change; unsealed ones are inert") {
    const Picks& p = P();
    const DataStore& d = HookData();
    std::unique_ptr<GameSim> sim = StartOn(d, 41);
    SimContext& ctx = sim->Context();
    ZoneRuntime& zone = *ctx.sys.zone;
    const Vec2 pit = p.sealedTile.Center();
    ctx.sys.hero->SetPosition(pit + Vec2(0, -2));
    std::vector<Event> ev = StepCollect(*sim, 1);
    const InteractTarget t = zone.FindInteractTarget();
    CHECK(t.kind == InteractKind::SubDungeon);
    CHECK(t.key == kSealedEntrance);
    CHECK(t.pos == pit);
    CHECK(sim->View().prompt.kind == InteractKind::SubDungeon);
    bool prompted = false;
    for (const EvInteractPrompt& e : OfType<EvInteractPrompt>(ev)) {
      prompted = prompted || e.kind == InteractKind::SubDungeon;
    }
    CHECK(prompted);
    // Interact: one toast + one log line with the key; nothing else changes.
    const std::string mapBefore = zone.MapId();
    const Vec2 heroBefore = ctx.sys.hero->Position();
    sim->Submit(CmdInteract{});
    ev = StepCollect(*sim, 2);
    const std::vector<EvBanner> banners = OfType<EvBanner>(ev);
    REQUIRE(banners.size() == 1);
    CHECK(banners[0].kind == BannerKind::Toast);
    CHECK(banners[0].title.key == kSealedToast);
    size_t logs = 0;
    for (const EvLog& l : OfType<EvLog>(ev)) logs += l.text.key == kSealedToast ? 1 : 0;
    CHECK(logs == 1);
    CHECK(OfType<EvZone>(ev).empty());
    CHECK_FALSE(ctx.session.transitioning);
    CHECK(zone.MapId() == mapBefore);
    CHECK(ctx.sys.hero->Position() == heroBefore);
    // Out of reach (distSq > 9): not a target.
    ctx.sys.hero->SetPosition(pit + Vec2(0, -3.2));
    CHECK(zone.FindInteractTarget().kind != InteractKind::SubDungeon);
    // A press on it from afar walks there, then shows the toast (W8 walk-then-act).
    ctx.sys.hero->SetPosition(pit + Vec2(0, -6));
    sim->ClearEvents();
    sim->Submit(CmdPointerPress{pit, PointerButton::Primary, 0});
    ev = StepCollect(*sim, 1);
    CHECK(zone.Pending().active);
    ev = StepCollect(*sim, 300);
    size_t toasts = 0;
    for (const EvBanner& b : OfType<EvBanner>(ev)) toasts += b.title.key == kSealedToast ? 1 : 0;
    CHECK(toasts == 1);
    CHECK(DistSq(ctx.sys.hero->Position(), pit) <= 9.0 + 1e-9);
    CHECK(zone.MapId() == mapBefore);
    // An unsealed entrance stays inert until sub-dungeon travel (K7): no target, no toast, no transition.
    const Vec2 door = p.openTile.Center();
    ctx.sys.hero->SetPosition(door + Vec2(0, -1));
    CHECK(zone.FindInteractTarget().kind != InteractKind::SubDungeon);
    sim->Submit(CmdInteract{});
    ev = StepCollect(*sim, 2);
    CHECK(OfType<EvBanner>(ev).empty());
    CHECK(OfType<EvZone>(ev).empty());
    CHECK(zone.MapId() == mapBefore);
  }
}

// =====================================================================================================================
// Grep gate: no Chapter 1 content id is hard-coded in the core
// =====================================================================================================================

namespace {

// Every content id of the Chapter 1 zone in `d` (quests, their objectives' targets, hunts, the zone's monsters, the
// mini-boss and story boss, the cutscenes they trigger, lore, hidden areas, story decorations, the achievements that
// target those monsters). NPC ids and zone ids are role keys the remake keeps (DECISIONS DR3), so they are not listed.
std::set<std::string> Chapter1ContentIds(const DataStore& d) {
  const std::string zone = d.World().defaultMap;
  std::set<std::string> ids;
  std::set<std::string> quests, monsters;
  for (const QuestDef& q : d.Quests().quests) {
    if (q.zone != zone) continue;
    quests.insert(q.id);
    for (const QuestHuntRef& h : q.hunts) ids.insert(h.huntId);
    for (const QuestObjectiveDef& o : q.objectives) {
      ids.insert(o.targetId);
      ids.insert(o.dropMonsters.begin(), o.dropMonsters.end());
    }
    for (const QuestClueDef& c : q.clues) ids.insert(c.id);
  }
  if (const ZoneMonsterList* z = d.Monsters().ZoneList(zone)) {
    monsters.insert(z->monsterIds.begin(), z->monsterIds.end());
  }
  if (const MiniBossEntry* m = d.Monsters().MiniBossFor(zone)) monsters.insert(m->monsterId);
  if (!d.Monsters().ai.storyBossId.empty()) monsters.insert(d.Monsters().ai.storyBossId);
  for (const HuntDef& h : d.Monsters().hunts) {
    if (quests.count(h.questId) == 0) continue;
    ids.insert(h.huntId);
    monsters.insert(h.monsterId);
  }
  for (const StoryTriggerDef& t : d.Story().triggers) {
    const bool inChapter = quests.count(t.subjectId) != 0 || monsters.count(t.subjectId) != 0 || t.subjectId == zone;
    if (inChapter) ids.insert(t.cutscene);
  }
  for (const BossIntroDef& b : d.Story().bossIntros) {
    if (monsters.count(b.monsterId) != 0) ids.insert(b.cutscene);
  }
  for (const LoreEntryDef& l : d.Lore().entries) {
    if (l.zone == zone) ids.insert(l.id);
  }
  // Quest objective targets already bring in the collect items (mat_*), the escort and defend targets and the explore
  // zones; the map adds its entrances, the sub-dungeons they lead to and its mood regions.
  std::set<std::string> npcs;
  if (const MapDef* m = d.FindMap(zone)) {
    for (const HiddenAreaDef& h : m->hiddenAreas) ids.insert(h.id);
    for (const StoryDecorationDef& s : m->storyDecorations) ids.insert(s.id);
    for (const SubDungeonEntranceDef& e : m->subDungeonEntrances) {
      ids.insert(e.id);
      ids.insert(e.targetSubDungeon);
    }
    // A region id that is also a mood theme name (MapTheme, e.g. the Breach Rim's) is a presentation key the core and UE
    // share, like a zone id: not gated (making mood themes data, not enum values, needs the UE mood lookup; DECISIONS
    // DR8 note).
    for (const MapRegionDef& r : m->regions) {
      MapTheme asTheme{};
      if (!ParseEnum(r.id, asTheme)) ids.insert(r.id);
    }
    for (const MapCampDef& c : m->camps) npcs.insert(c.npcs.begin(), c.npcs.end());
    for (const FieldNpcDef& f : m->fieldNpcs) npcs.insert(f.npcId);
  }
  // Dialogue-tree node ids of the zone's NPCs (the old bot's hard-coded path {reward_ask, help, other_quests,
  // accept_gel} was this kind). Compound ids only: one-word nodes (root, help, who, bell, farewell) are ordinary words
  // the core spells for other reasons, and a token gate on them would be noise.
  for (const std::string& npcId : npcs) {
    const NpcDef* npc = d.FindNpc(npcId);
    const DialogueTree* tree = npc != nullptr && !npc->dialogueTreeId.empty() ? d.Dialogues().Find(npc->dialogueTreeId)
                                                                            : nullptr;
    if (tree == nullptr) continue;
    for (const DialogueNode& n : tree->nodes) {
      if (n.id.find('_') != std::string::npos) ids.insert(n.id);
    }
  }
  for (const AchievementDef& a : d.Quests().achievements) {
    if (monsters.count(a.targetId) != 0) ids.insert(a.id);
  }
  ids.insert(quests.begin(), quests.end());
  ids.insert(monsters.begin(), monsters.end());
  for (auto it = ids.begin(); it != ids.end();) {
    if (it->empty() || d.FindNpc(*it) != nullptr || d.FindMap(*it) != nullptr) {
      it = ids.erase(it);
    } else {
      ++it;
    }
  }
  return ids;
}

// Core sources with a known hard-coded Chapter 1 script, and who removes it. A listed file must still need its entry.
// Empty since the content switch's quest rename (1B part 2): Private/tools/AutoPilot.cpp, exempt until then, still
// hard-codes the web route, but every id it spells is gone from the data, so it hides no hit; its route interpreter over
// Data/autopilot_routes.json (1B part 3, implementation-plan.md 6) must keep the gate clean without an entry.
struct GateExemption {
  const char* path;  // relative to Source/AbyssCore
  const char* why;
};
constexpr std::array<GateExemption, 0> kGateExemptions{};

}  // namespace

TEST_SUITE("world") {
  TEST_CASE("K1 grep gate: no Chapter 1 content id is spelled in the core's string literals") {
    // The scanner (SourceScan.h), on a sample: literals count, comments do not, ids are tokens.
    const std::string sample =
        "// \"fx_marshal\" in a comment\n"
        "Find(\"fx_marshal\"); Key(\"data.monster.fx_marshal_ii\"); Raw(R\"x(fx_marshal:1)x\");\n";
    std::vector<std::string> hits;
    for (const test::src::SrcLiteral& lit : test::src::SrcStringLiterals(sample)) {
      if (test::src::SrcHasToken(lit.text, kSynthBoss)) hits.push_back(lit.text);
    }
    CHECK(hits == std::vector<std::string>{"fx_marshal", "fx_marshal:1"});

    const DataStore& d = test::RealData();
    const std::set<std::string> ids = Chapter1ContentIds(d);
    CHECK(ids.size() >= 20);  // the gate looks for something
    CHECK(ids.count(d.Monsters().ai.storyBossId) == 1);
    // Every id family the gate covers is represented in the live chapter (an empty family would gate nothing).
    const MapDef* zoneMap = d.FindMap(d.World().defaultMap);
    REQUIRE(zoneMap != nullptr);
    for (const SubDungeonEntranceDef& e : zoneMap->subDungeonEntrances) CHECK(ids.count(e.id) == 1);
    bool collectItem = false, escortOrDefend = false;
    for (const QuestDef& q : d.Quests().quests) {
      if (q.zone != d.World().defaultMap) continue;
      for (const QuestObjectiveDef& o : q.objectives) {
        if (o.type == ObjectiveType::Collect && ids.count(o.targetId) == 1) collectItem = true;
        if ((o.type == ObjectiveType::Escort || o.type == ObjectiveType::DefendWave) && ids.count(o.targetId) == 1) {
          escortOrDefend = true;
        }
      }
    }
    CHECK(collectItem);
    CHECK(escortOrDefend);
    size_t treeNodes = 0;
    for (const std::string& id : ids) treeNodes += (id.rfind("accept_", 0) == 0) ? 1 : 0;
    CHECK(treeNodes >= 1);  // the Captain's questTrigger nodes
    CHECK(ids.count(test::ch1::kElder) == 0);  // role keys are not content ids

    namespace fs = std::filesystem;
    const fs::path root = fs::path(test::UnrealDir()) / "Source" / "AbyssCore";
    std::error_code ec;
    std::vector<fs::path> files;
    for (const char* sub : {"Public", "Private"}) {
      for (fs::recursive_directory_iterator it(root / sub, ec), end; !ec && it != end; it.increment(ec)) {
        const fs::path f = it->path();
        if (f.extension() == ".h" || f.extension() == ".cpp") files.push_back(f);
      }
    }
    REQUIRE_FALSE(ec);
    std::sort(files.begin(), files.end());
    std::string spelled;
    std::set<std::string> exemptHit;
    size_t scanned = 0;
    for (const fs::path& f : files) {
      const std::string rel = f.lexically_relative(root).generic_string();
      const std::string text = test::ReadFile(f.string());
      REQUIRE_FALSE(text.empty());
      ++scanned;
      const bool exempt = std::any_of(std::begin(kGateExemptions), std::end(kGateExemptions),
                                      [&rel](const GateExemption& e) { return rel == e.path; });
      for (const test::src::SrcLiteral& lit : test::src::SrcStringLiterals(text)) {
        for (const std::string& id : ids) {
          if (!test::src::SrcHasToken(lit.text, id)) continue;
          if (exempt) {
            exemptHit.insert(rel);
          } else {
            spelled += rel + ": \"" + lit.text + "\" spells " + id + "\n";
          }
        }
      }
    }
    CHECK(scanned > 100);
    INFO("Chapter 1 content ids hard-coded in the core (read them from data: GameData/config.ts, monster_ai.json "
         "port.storyBoss, the quest / story tables):\n"
         << spelled);
    CHECK(spelled.empty());
    for (const GateExemption& e : kGateExemptions) {
      INFO("stale exemption " << e.path << " (" << e.why << "): it spells no Chapter 1 id any more, drop the entry");
      CHECK(exemptHit.count(e.path) == 1);
    }
  }
}
