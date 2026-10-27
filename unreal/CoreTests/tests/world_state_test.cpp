// K6 status, permanent rewards, world flags, NPC lines, spawn variants and event champion packs (dark remake,
// implementation-plan.md 3 "K6", work package 2E): the schema (status_effects.json `stacks`, quest `setsFlags` /
// `rewards.permanent`, map spawn `variants`, NPC `lines` / `visibleWhen`, random_events.json `ambushChampion`: loads,
// loader refusals, DataStore cross checks and the visibility validator), the pure rules (world/WorldState.h, the Ashnumb
// stack arithmetic of StatusEffectSystem), and the runtime: Ashnumb on the hero (heals, regeneration, a monster's hits,
// the antidote, the slow), a permanent reward once per (quest, difficulty) and its save round trip, world flags set on a
// turn-in (first listener, sorted set, snapshot, save round trip, an old save loading with none), spawn variants at the
// zone's spawn and at respawns (and the quest guide), NPC lines and visibility, the champion ambush (the extra draw only
// with the block, the pack and its leader, the shared affix), and a seeded replay whose event-log digest is pinned.
//
// Synthetic content only: fx_k6_* NPCs, quests, monsters and strings, the Ashnumb row, a spawn entry's variants and the
// zone's champion pack are added to the real export in memory, so no content change moves these tests.
// Suite: world.
#include <algorithm>
#include <cmath>
#include <cstdint>
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

#include "ItemsFixtures.h"
#include "MonstersFixtures.h"
#include "SimHarness.h"
#include "TestContent.h"
#include "TestUtil.h"
#include "abyss/base/Json.h"
#include "abyss/base/StrUtil.h"
#include "abyss/combat/Combat.h"
#include "abyss/combat/Projectiles.h"
#include "abyss/combat/SoulEcho.h"
#include "abyss/combat/StatusEffects.h"
#include "abyss/data/DataStore.h"
#include "abyss/monsters/EliteAffixes.h"
#include "abyss/monsters/MonsterDefs.h"
#include "abyss/quests/Dialogue.h"
#include "abyss/quests/QuestWorld.h"
#include "abyss/save/SaveData.h"
#include "abyss/save/SaveIO.h"
#include "abyss/sim/GameSim.h"
#include "abyss/story/StoryDirector.h"
#include "abyss/world/Locomotion.h"
#include "abyss/world/RandomEvents.h"
#include "abyss/world/WorldState.h"
#include "abyss/world/Zone.h"
#include "doctest/doctest.h"

using namespace abyss;

namespace {

using namespace abyss::test::monsters_fx;

// ---- synthetic ids --------------------------------------------------------------------------------------------------

constexpr char kGiver[] = "fx_k6_giver";      // a camp NPC with the two K6 quests and gated lines
constexpr char kGhost[] = "fx_k6_ghost";      // a camp NPC seen only while dawn is set and the bell is not
constexpr char kTalker[] = "fx_k6_talker";    // a camp NPC whose every line waits for the bell
constexpr char kQuestFlag[] = "fx_k6_q_flag";      // sets dawn then bell on its turn-in
constexpr char kQuestReward[] = "fx_k6_q_reward";  // a permanent reward
constexpr char kFlagDawn[] = "fx_k6_dawn";
constexpr char kFlagBell[] = "fx_k6_bell";
constexpr char kDead[] = "fx_k6_dead";    // family fx_k6_undead
constexpr char kGnaw[] = "fx_k6_gnaw";    // family fx_k6_beast: the dawn variant
constexpr char kBiter[] = "fx_k6_biter";  // its hits stack Ashnumb
constexpr char kLineFirst[] = "fx.k6.line.first";
constexpr char kLineSecond[] = "fx.k6.line.second";
constexpr char kLineAfter[] = "fx.k6.line.after";
constexpr char kLineBell[] = "fx.k6.line.bell";
constexpr char kAshnumbName[] = "sys.statusEffect.name.ashnumb";
constexpr char kTalkerLine[] = "fx talker web line";  // the talker's web dialogue[0] (no i18n key: logged as is)

// ---- data editing ---------------------------------------------------------------------------------------------------

JsonValue K6Parse(std::string_view text) {
  JsonValue v;
  JsonParseError err;
  const bool ok = ParseJson(text, v, &err);
  REQUIRE_MESSAGE(ok, "bad test JSON: " << text);
  return v;
}

JsonValue* K6At(JsonValue& root, std::initializer_list<std::string_view> keys) {
  JsonValue* v = &root;
  for (std::string_view k : keys) {
    v = v->FindMutable(k);
    REQUIRE(v != nullptr);
  }
  return v;
}

using K6Edits = std::vector<std::pair<std::string, std::function<void(JsonValue&)>>>;

// The real export with `edits` applied in memory (several edits of one file run in order on one document).
std::unique_ptr<DataStore> K6LoadEdited(const K6Edits& edits, DataLoadReport& report) {
  std::map<std::string, JsonValue> docs;
  for (const auto& [file, edit] : edits) {
    if (docs.count(file) == 0) docs.emplace(file, K6Parse(test::ReadFile(test::DataDir() + "/" + file)));
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

bool K6HasIssue(const std::vector<DataIssue>& issues, std::string_view file, std::string_view pathPart,
                std::string_view text) {
  for (const DataIssue& e : issues) {
    if (e.file == file && e.path.find(pathPart) != std::string::npos && e.message.find(text) != std::string::npos) {
      return true;
    }
  }
  return false;
}

// The strings the synthetic records name (both locales).
void K6AddStrings(JsonValue& doc) {
  JsonValue* s = K6At(doc, {"strings"});
  for (const char* k : {kLineFirst, kLineSecond, kLineAfter, kLineBell, kAshnumbName}) s->Set(k, JsonValue::String(k));
}

// The Ashnumb row (monsters.md 4): -10 % healing per stack, at most 5, slowed 20 % at 5.
constexpr char kAshnumbStacks[] = R"({"maxStacks": 5, "healingReceivedPerStack": 0.1, "slowAtMaxStacksPct": 20})";
void K6AddAshnumb(JsonValue& doc) {
  // The shipped export has the row since the dark remake's content (2F): written again here, listed once.
  bool listed = false;
  for (const JsonValue& t : K6At(doc, {"types"})->Items()) listed = listed || t.AsString() == "ashnumb";
  if (!listed) K6At(doc, {"types"})->Append(JsonValue::String("ashnumb"));
  K6At(doc, {"tickIntervalMs"})->Set("ashnumb", JsonValue::Integer(0));
  K6At(doc, {"stacking"})->Set("ashnumb", JsonValue::String("stackCount"));
  JsonValue stacks = JsonValue::Object();
  stacks.Set("ashnumb", K6Parse(kAshnumbStacks));
  doc.Set("stacks", std::move(stacks));
}

// A monsters.json definition (with its exporter `derived` block): level 4, `hp`, `damage`, melee 1.5, a swing every
// 1000 ms, of `family`; `onHit` = the derived onHitStatus array.
std::string K6DefJson(std::string_view id, std::string_view family, int hp = 200, int damage = 4,
                      std::string_view onHit = "[]") {
  return StrCat(R"({"id": ")", id, R"(", "name": ")", id, R"(", "level": 4, "hp": )", hp, R"(, "damage": )", damage,
                R"(, "defense": 2, "speed": 50, "aggroRange": 5, "attackRange": 1.5, "attackSpeed": 1000, "expReward": 10,
    "goldReward": [1, 2], "spriteKey": "fx_k6_art", "animCategory": "humanoid", "family": ")",
                family, R"(", "derived": {"isRanged": false, "onHitStatus": )", onHit,
                R"(, "projectileColor": 13386956, "homeZone": null, "nameKey": "data.monster.)", id,
                R"(", "sources": ["zone"]}})");
}

constexpr char kAshnumbOnHit[] =
    R"([{"status": "ashnumb", "chance": null, "value": {"kind": "fixed", "amount": 1}, "durationMs": 6000}])";

void K6AddMonsters(JsonValue& doc) {
  JsonValue* defs = K6At(doc, {"defs"});
  defs->Set(kDead, K6Parse(K6DefJson(kDead, "fx_k6_undead")));
  defs->Set(kGnaw, K6Parse(K6DefJson(kGnaw, "fx_k6_beast")));
  defs->Set(kBiter, K6Parse(K6DefJson(kBiter, "fx_k6_beast", 5000, 2, kAshnumbOnHit)));
}

// A quests.json quest of the zone given by kGiver (talk to it once), with `extra` top-level members and `rewards`.
std::string K6QuestJson(std::string_view id, std::string_view extra, std::string_view rewards = R"({"exp": 10, "gold": 1})",
                        std::string_view prereqs = "[]") {
  return StrCat(R"({"id": ")", id, R"(", "name": ")", id, R"(", "zone": ")", test::ch1::kZone,
                R"(", "type": "talk", "category": "side",
    "objectives": [{"type": "talk", "targetId": ")",
                kGiver, R"(", "targetName": "fx", "required": 1, "current": 0}], "rewards": )", rewards,
                R"(, "prereqQuests": )", prereqs, R"(, "level": 1)", extra.empty() ? "" : ", ", extra,
                R"(, "derived": {"giverNpcId": ")", kGiver, R"(", "objectiveItemKinds": [null], "embersOnTurnIn": 0,
    "i18n": {"name": "data.quest.)",
                id, R"(.name", "desc": "data.quest.)", id, R"(.desc", "offer": "data.quest.)", id,
                R"(.offer", "complete": "data.quest.)", id, R"(.complete"}}})");
}

constexpr char kPermanent[] = R"({"maxLife": 15, "statPoints": 3, "skillPoints": 1, "respecTokens": 1})";

void K6AddQuests(JsonValue& doc) {
  JsonValue* quests = K6At(doc, {"quests"});
  quests->Append(K6Parse(K6QuestJson(kQuestFlag, StrCat(R"("setsFlags": [")", kFlagDawn, R"(", ")", kFlagBell, R"("])"))));
  // No exp: a level-up would move the hero's max life under the permanent reward's checks.
  quests->Append(K6Parse(K6QuestJson(kQuestReward, "", StrCat(R"({"exp": 0, "gold": 1, "permanent": )", kPermanent, "}"))));
}

std::string K6NpcJson(std::string_view id, std::string_view quests, std::string_view extra) {
  return StrCat(R"({"id": ")", id, R"(", "name": ")", id, R"(", "type": "quest", "dialogue": [")", kTalkerLine,
                R"("], "quests": )", quests, R"(, "dialogueTreeId": null, "nameKey": "data.npc.)", id, R"(.name")",
                extra.empty() ? "" : ", ", extra, "}");
}

void K6AddNpcs(JsonValue& doc) {
  JsonValue* npcs = K6At(doc, {"npcs"});
  npcs->Set(kGiver, K6Parse(K6NpcJson(
                        kGiver, StrCat(R"([")", kQuestFlag, R"(", ")", kQuestReward, R"("])"),
                        StrCat(R"("lines": [{"key": ")", kLineAfter, R"(", "when": {"quest": ")", kQuestFlag,
                               R"(", "state": "turnedIn"}}, {"key": ")", kLineFirst, R"("}, {"key": ")", kLineSecond,
                               R"("}])"))));
  npcs->Set(kGhost, K6Parse(K6NpcJson(kGhost, "[]",
                                      StrCat(R"("visibleWhen": {"notFlag": ")", kFlagBell, R"(", "any": [{"flag": ")",
                                             kFlagDawn, R"("}]})"))));
  npcs->Set(kTalker, K6Parse(K6NpcJson(kTalker, "[]",
                                       StrCat(R"("lines": [{"key": ")", kLineBell, R"(", "when": {"flag": ")", kFlagBell,
                                              R"("}}, {"key": ")", kLineSecond, R"(", "when": {"flag": ")", kFlagBell,
                                              R"("}}])"))));
}

// The zone's first camp also hosts the giver and the talker, its second camp the ghost (on a slot of its own) (camp NPC
// lists are not map-generator inputs).
void K6AddCampNpcs(JsonValue& doc) {
  JsonValue* camps = K6At(doc, {"maps", test::ch1::kZone, "camps"});
  REQUIRE(camps->Size() >= 2);
  JsonValue* first = camps->MutableItems()[0].FindMutable("npcs");
  JsonValue* second = camps->MutableItems()[1].FindMutable("npcs");
  REQUIRE((first != nullptr && second != nullptr));
  first->Append(JsonValue::String(kGiver));
  first->Append(JsonValue::String(kTalker));
  second->Append(JsonValue::String(kGhost));
}

// A plain spawn entry of the zone (not elite, not the story boss, count >= minCount): its index.
size_t K6PlainSpawn(const DataStore& data, int32_t minCount) {
  const MapDef* map = data.World().FindMap(test::ch1::kZone);
  REQUIRE(map != nullptr);
  for (size_t i = 0; i < map->spawns.size(); ++i) {
    const MapSpawnDef& s = map->spawns[i];
    const MonsterDef* d = data.Monsters().FindForZone(map->id, s.monsterId);
    if (d == nullptr || d->elite || s.monsterId == data.Monsters().ai.storyBossId || s.count < minCount ||
        !s.mix.empty()) {
      continue;
    }
    return i;
  }
  FAIL("no plain spawn entry");
  return 0;
}

// Spawn entry `index` of the zone gets `variants` (JSON).
std::function<void(JsonValue&)> K6EditVariants(size_t index, std::string variants) {
  return [index, variants](JsonValue& doc) {
    JsonValue* spawns = K6At(doc, {"maps", test::ch1::kZone, "spawns"});
    REQUIRE(index < spawns->Size());
    spawns->MutableItems()[index].Set("variants", K6Parse(variants));
  };
}

// The dawn variant (2 gnawers) listed before the bell variant (1 dead).
std::string K6Variants() {
  return StrCat(R"([{"whenFlag": ")", kFlagDawn, R"(", "monsterId": ")", kGnaw, R"(", "count": 2}, {"whenFlag": ")",
                kFlagBell, R"(", "monsterId": ")", kDead, R"(", "count": 1}])");
}

// The zone's champion pack: one dead (the leader) and two gnawers sharing the affix roll.
std::string K6Champion(double chance, bool shared = true) {
  return StrCat(R"({"chance": )", chance, R"(, "mix": [{"monsterId": ")", kDead, R"(", "count": 1}, {"monsterId": ")",
                kGnaw, R"(", "count": 2}], "leader": ")", kDead, R"(", "sharedAffix": )", shared ? "true" : "false",
                "}");
}

std::function<void(JsonValue&)> K6EditChampion(std::string champion) {
  return [champion](JsonValue& doc) {
    K6At(doc, {"zones", test::ch1::kZone})->Set("ambushChampion", K6Parse(champion));
  };
}

// Elite affixes for the shared roll: the zone rolls two of {swift, frozen}; swift only for the undead family.
void K6EditAffixes(JsonValue& doc) {
  JsonValue order = JsonValue::Array();
  order.Append(JsonValue::String("swift"));
  order.Append(JsonValue::String("frozen"));
  doc.Set("order", std::move(order));
  K6At(doc, {"zoneAffixCounts"})->Set(test::ch1::kZone, K6Parse("[2, 2]"));
  K6At(doc, {"definitions", "swift"})->Set("families", K6Parse(R"(["fx_k6_undead"])"));
}

// Every K6 record but the champion pack (the base of the runtime cases).
// The engine cases start from no champion pack: the shipped one (the dark remake's Muster, 2F; content_ch1_test.cpp)
// is taken out, so "no block" is the block's absence and the cases' own blocks are the only ones.
void K6StripChampions(JsonValue& doc) {
  for (JsonMember& z : K6At(doc, {"zones"})->MutableMembers()) z.value.Remove("ambushChampion");
}

K6Edits K6BaseEdits() {
  const size_t plain = K6PlainSpawn(test::RealData(), 3);
  return {{"i18n_zh-CN.json", K6AddStrings}, {"i18n_en.json", K6AddStrings}, {"status_effects.json", K6AddAshnumb},
          {"monsters.json", K6AddMonsters},  {"quests.json", K6AddQuests},    {"npcs.json", K6AddNpcs},
          {"maps.json", K6AddCampNpcs},      {"maps.json", K6EditVariants(plain, K6Variants())},
          {"random_events.json", K6StripChampions}};
}

const DataStore& K6Data() {
  static std::unique_ptr<DataStore> store;
  if (store == nullptr) {
    DataLoadReport report;
    store = K6LoadEdited(K6BaseEdits(), report);
    INFO(report.Summary());
    REQUIRE(report.Ok());
  }
  return *store;
}

// K6Data plus the champion pack (chance 1, shared affix) and the affix edits.
const DataStore& K6ChampionData(double chance = 1, bool shared = true) {
  static std::map<std::pair<double, bool>, std::unique_ptr<DataStore>> stores;
  std::unique_ptr<DataStore>& store = stores[{chance, shared}];
  if (store == nullptr) {
    DataLoadReport report;
    K6Edits e = K6BaseEdits();
    e.emplace_back("random_events.json", K6EditChampion(K6Champion(chance, shared)));
    e.emplace_back("elite_affixes.json", K6EditAffixes);
    store = K6LoadEdited(e, report);
    INFO(report.Summary());
    REQUIRE(report.Ok());
  }
  return *store;
}

// Quest `id` of quests.json gets `edit`.
std::function<void(JsonValue&)> K6EditQuest(std::string id, std::function<void(JsonValue&)> edit) {
  return [id, edit](JsonValue& doc) {
    for (JsonValue& q : K6At(doc, {"quests"})->MutableItems()) {
      const JsonValue* qid = q.Find("id");
      if (qid != nullptr && qid->AsString() == id) {
        edit(q);
        return;
      }
    }
    FAIL("no quest " << id);
  };
}

// One bad edit on top of the K6 base: the load report.
DataLoadReport K6BadLoad(const std::string& file, std::function<void(JsonValue&)> edit) {
  DataLoadReport report;
  K6Edits e = K6BaseEdits();
  e.emplace_back(file, std::move(edit));
  K6LoadEdited(e, report);
  return report;
}

// ---- sims -----------------------------------------------------------------------------------------------------------

std::unique_ptr<GameSim> K6Start(const DataStore& data, uint64_t seed = 11) {
  auto sim = GameSim::Create(data, SimConfig{});
  REQUIRE(sim != nullptr);
  REQUIRE(sim->NewGame(ClassId::Warrior, Difficulty::Normal, seed));
  sim->Context().sys.story->FinishAllBeats();
  sim->Step();
  return sim;
}

QuestProgress K6Record(const DataStore& d, std::string_view id, QuestStatus st) {
  const QuestDef* q = d.FindQuest(id);
  REQUIRE(q != nullptr);
  std::vector<int32_t> cur;
  for (const QuestObjectiveDef& o : q->objectives) cur.push_back(st == QuestStatus::Active ? 0 : o.required);
  return QuestProgress{std::string(id), st, cur};
}

template <class E>
std::vector<E> K6Of(std::span<const Event> events) {
  std::vector<E> out;
  for (const Event& e : events) {
    if (const E* x = std::get_if<E>(&e)) out.push_back(*x);
  }
  return out;
}

template <class E>
std::vector<E> K6Of(const EventSink& sink) {
  return K6Of<E>(std::span<const Event>(sink.Items()));
}

// Index of the first event of type E (-1 = none).
template <class E>
int32_t K6IndexOf(std::span<const Event> events) {
  for (size_t i = 0; i < events.size(); ++i) {
    if (std::holds_alternative<E>(events[i])) return static_cast<int32_t>(i);
  }
  return -1;
}

std::vector<std::string> K6LogKeys(std::span<const Event> events) {
  std::vector<std::string> out;
  for (const EvLog& l : K6Of<EvLog>(events)) out.push_back(l.text.key);
  return out;
}

bool K6Has(const std::vector<std::string>& v, std::string_view s) { return std::find(v.begin(), v.end(), s) != v.end(); }

const NpcView* K6NpcView(const Snapshot& v, std::string_view id) {
  for (const NpcView& n : v.npcs) {
    if (n.npcId == id) return &n;
  }
  return nullptr;
}

const NpcPlacement* K6Placement(const ZoneRuntime& z, std::string_view id) {
  for (const NpcPlacement& n : z.Npcs()) {
    if (n.npcId == id) return &n;
  }
  return nullptr;
}

// The living instances of map spawn entry `entry`.
std::vector<const MonsterInstance*> K6EntryMonsters(const MonsterSystem& ms, size_t entry) {
  std::vector<const MonsterInstance*> out;
  for (const MonsterInstance& m : ms.All()) {
    if (m.IsAlive() && m.spawnEntry == static_cast<int32_t>(entry)) out.push_back(&m);
  }
  return out;
}

// The monsters a sink announced (EvEntitySpawned of kind Monster), in spawn order.
std::vector<EntityId> K6SpawnedMonsters(const EventSink& sink) {
  std::vector<EntityId> out;
  for (const EvEntitySpawned& e : K6Of<EvEntitySpawned>(sink)) {
    if (e.kind == EntityKind::Monster) out.push_back(e.id);
  }
  return out;
}

// Kills `id` with one huge hero hit (the kill pipeline runs).
void K6Kill(SimContext& ctx, EntityId id) {
  DamageFlags f;
  f.source = KillSource::HeroBasic;
  ctx.sys.monsters->ApplyDamage(id, 1e9, f);
}

// ---- the event-log digest (FNV-1a over the fields a K6 run produces) ------------------------------------------------

struct K6Fnv {
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

void K6Digest(K6Fnv& f, std::span<const Event> events) {
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
    } else if (const EvHit* h = std::get_if<EvHit>(&e)) {
      f.U64(h->target);
      f.U64(h->source);
      f.D(h->amount);
      f.D(h->targetHp);
    } else if (const EvStatusApplied* st = std::get_if<EvStatusApplied>(&e)) {
      f.U64(st->target);
      f.U64(static_cast<uint64_t>(st->type));
      f.D(st->value);
      f.D(st->durationMs);
    } else if (const EvWorldFlag* w = std::get_if<EvWorldFlag>(&e)) {
      f.S(w->flag);
      f.S(w->questId);
    } else if (const EvPermanentReward* p = std::get_if<EvPermanentReward>(&e)) {
      f.S(p->questId);
      f.U64(static_cast<uint64_t>(p->maxLife));
    } else if (const EvRandomEvent* r = std::get_if<EvRandomEvent>(&e)) {
      f.U64(static_cast<uint64_t>(r->type));
      f.U64((r->resolved ? 1u : 0u) | (r->champion ? 2u : 0u));
      f.V(r->pos);
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

  TEST_CASE("K6 data: the Ashnumb row, setsFlags, permanent rewards, spawn variants, NPC lines / visibleWhen load") {
    const DataStore& d = K6Data();
    // status_effects.json: the stackCount row.
    const StatusEffectRules& sr = d.Classes().statusRules;
    CHECK(sr.Stacking(StatusType::Ashnumb) == StatusStacking::StackCount);
    CHECK(sr.Lands(StatusType::Ashnumb));
    CHECK(sr.Stacks(StatusType::Ashnumb).maxStacks == 5);
    CHECK(sr.Stacks(StatusType::Ashnumb).healingReceivedPerStack == doctest::Approx(0.1));
    CHECK(sr.Stacks(StatusType::Ashnumb).slowAtMaxStacksPct == doctest::Approx(20));
    CHECK(sr.TickInterval(StatusType::Ashnumb) == 0);
    for (StatusType t : {StatusType::Burn, StatusType::Freeze, StatusType::Poison, StatusType::Bleed, StatusType::Slow,
                         StatusType::Stun}) {
      CHECK(sr.Lands(t));  // the web's six are untouched
      CHECK(sr.Stacks(t).maxStacks == 0);
    }
    // A monster's on-hit Ashnumb.
    const MonsterDef* biter = d.Monsters().Find(kBiter);
    REQUIRE(biter != nullptr);
    REQUIRE(biter->onHitStatus.size() == 1);
    CHECK(biter->onHitStatus[0].status == StatusType::Ashnumb);
    // quests.json.
    const QuestDef* flag = d.FindQuest(kQuestFlag);
    REQUIRE(flag != nullptr);
    CHECK(flag->setsFlags == std::vector<std::string>{kFlagDawn, kFlagBell});  // data order
    CHECK_FALSE(flag->rewards.permanent.present);
    const QuestDef* reward = d.FindQuest(kQuestReward);
    REQUIRE(reward != nullptr);
    CHECK(reward->setsFlags.empty());
    const QuestPermanentRewardDef& p = reward->rewards.permanent;
    CHECK((p.present && p.maxLife == 15 && p.statPoints == 3 && p.skillPoints == 1 && p.respecTokens == 1));
    // npcs.json.
    const NpcDef* giver = d.FindNpc(kGiver);
    REQUIRE(giver != nullptr);
    REQUIRE(giver->lines.size() == 3);
    CHECK(giver->lines[0].key == kLineAfter);
    CHECK(giver->lines[0].hasWhen);
    REQUIRE(giver->lines[0].when.all.size() == 1);
    CHECK(giver->lines[0].when.all[0].kind == WorldLeafKind::Quest);
    CHECK(giver->lines[0].when.all[0].id == kQuestFlag);
    CHECK(giver->lines[0].when.all[0].state == QuestGate::TurnedIn);
    CHECK_FALSE(giver->lines[1].hasWhen);
    CHECK_FALSE(giver->hasVisibleWhen);
    const NpcDef* ghost = d.FindNpc(kGhost);
    REQUIRE(ghost != nullptr);
    CHECK(ghost->lines.empty());
    REQUIRE(ghost->hasVisibleWhen);
    REQUIRE(ghost->visibleWhen.all.size() == 1);
    CHECK(ghost->visibleWhen.all[0].kind == WorldLeafKind::NotFlag);
    CHECK(ghost->visibleWhen.all[0].id == kFlagBell);
    REQUIRE(ghost->visibleWhen.any.size() == 1);
    CHECK(ghost->visibleWhen.any[0].kind == WorldLeafKind::Flag);
    CHECK(ghost->visibleWhen.any[0].id == kFlagDawn);
    // maps.json.
    const size_t plain = K6PlainSpawn(test::RealData(), 3);
    const MapSpawnDef& sp = d.FindMap(test::ch1::kZone)->spawns[plain];
    REQUIRE(sp.variants.size() == 2);
    CHECK(sp.variants[0].whenFlag == kFlagDawn);
    CHECK(sp.variants[0].monsterId == kGnaw);
    CHECK(sp.variants[0].count == 2);
    CHECK(sp.variants[1].whenFlag == kFlagBell);
    // No ambush champion without the block (K6BaseEdits takes the shipped one out). The real export's K6 records are
    // the dark remake's Chapter 1 content (2F), pinned in content_ch1_test.cpp; a status of stackCount type whose row
    // is missing never lands (the K6 rule; the Ashnumb case below).
    const ZoneEventDataDef* z = d.World().randomEvents.ForZone(test::ch1::kZone);
    REQUIRE(z != nullptr);
    CHECK_FALSE(z->ambushChampion.present);
    StatusEffectRules noRow = test::RealData().Classes().statusRules;
    noRow.stacks[EnumIndex(StatusType::Ashnumb)] = StatusStackRule{};
    CHECK_FALSE(noRow.Lands(StatusType::Ashnumb));  // no row: it never lands
    CHECK(noRow.Stacking(StatusType::Ashnumb) == StatusStacking::StackCount);
  }

  TEST_CASE("K6 data: the ambush champion block loads") {
    const DataStore& d = K6ChampionData();
    const ZoneEventDataDef* z = d.World().randomEvents.ForZone(test::ch1::kZone);
    REQUIRE(z != nullptr);
    const ZoneEventDataDef::AmbushChampionDef& c = z->ambushChampion;
    CHECK(c.present);
    CHECK(c.chance == 1);
    REQUIRE(c.mix.size() == 2);
    CHECK(c.mix[0].monsterId == kDead);
    CHECK(c.mix[0].count == 1);
    CHECK(c.mix[1].monsterId == kGnaw);
    CHECK(c.mix[1].count == 2);
    CHECK(c.leader == kDead);
    CHECK(c.sharedAffix);
    // sharedAffix defaults to false, leader to none.
    DataLoadReport report;
    K6Edits e = K6BaseEdits();
    e.emplace_back("random_events.json",
                   K6EditChampion(StrCat(R"({"chance": 0.1, "mix": [{"monsterId": ")", kDead, R"(", "count": 2}]})")));
    auto store = K6LoadEdited(e, report);
    INFO(report.Summary());
    REQUIRE(report.Ok());
    const ZoneEventDataDef::AmbushChampionDef& c2 = store->World().randomEvents.ForZone(test::ch1::kZone)->ambushChampion;
    CHECK((c2.present && c2.chance == doctest::Approx(0.1) && c2.leader.empty() && !c2.sharedAffix));
  }

  TEST_CASE("K6 data: the loader refuses broken status rows, quests, conditions, lines, variants and champions") {
    struct Bad {
      std::string file;
      std::function<void(JsonValue&)> edit;
      std::string path, text;
    };
    const size_t plain = K6PlainSpawn(test::RealData(), 3);
    const auto stacks = [](std::string json) {
      return [json](JsonValue& doc) { K6At(doc, {"stacks"})->Set("ashnumb", K6Parse(json)); };
    };
    const auto quest = [](std::string_view id, std::function<void(JsonValue&)> f) {
      return [id = std::string(id), f](JsonValue& doc) {
        for (JsonValue& q : K6At(doc, {"quests"})->MutableItems()) {
          if (q.Get("id").AsString() == id) f(q);
        }
      };
    };
    const auto setRaw = [](std::string key, std::string json) {
      return [key, json](JsonValue& o) { o.Set(key, K6Parse(json)); };
    };
    const auto permanent = [&](std::string json) {
      return quest(kQuestReward, [json](JsonValue& q) { K6At(q, {"rewards"})->Set("permanent", K6Parse(json)); });
    };
    const auto npc = [](std::string_view id, std::string key, std::string json) {
      return [id = std::string(id), key, json](JsonValue& doc) { K6At(doc, {"npcs", id})->Set(key, K6Parse(json)); };
    };
    const auto variants = [plain](std::string json) { return K6EditVariants(plain, json); };
    const auto champion = [](std::string json) { return K6EditChampion(json); };
    const std::vector<Bad> bad = {
        // status_effects.json stacks rows.
        {"status_effects.json", stacks(R"({"maxStacks": 0})"), "stacks.ashnumb.maxStacks", "expected 1..100"},
        {"status_effects.json", stacks(R"({"maxStacks": 101})"), "stacks.ashnumb.maxStacks", "expected 1..100"},
        {"status_effects.json", stacks(R"({"maxStacks": 5, "healingReceivedPerStack": 1.5})"),
         "healingReceivedPerStack", "fraction in [0, 1]"},
        {"status_effects.json", stacks(R"({"maxStacks": 5, "healingReceivedPerStack": -0.1})"),
         "healingReceivedPerStack", "fraction in [0, 1]"},
        {"status_effects.json", stacks(R"({"maxStacks": 5, "slowAtMaxStacksPct": 120})"), "slowAtMaxStacksPct",
         "percent in [0, 100]"},
        {"status_effects.json", stacks(R"({"maxStacks": 5, "slow": 20})"), "stacks.ashnumb.slow", "unknown stacks field"},
        {"status_effects.json", [](JsonValue& doc) { K6At(doc, {"stacks"})->Set("burn", K6Parse(R"({"maxStacks": 2})")); },
         "stacks.burn", "needs stacking 'stackCount'"},
        {"status_effects.json", [](JsonValue& doc) { K6At(doc, {"stacks"})->Set("ash", K6Parse(R"({"maxStacks": 2})")); },
         "stacks.ash", "unknown status type"},
        {"status_effects.json", [](JsonValue& doc) { doc.Set("stacks", JsonValue::Object()); }, "stacking.ashnumb",
         "needs its stacks row"},
        {"status_effects.json", [](JsonValue& doc) { K6At(doc, {"tickIntervalMs"})->Set("ashnumb", JsonValue::Integer(1000)); },
         "tickIntervalMs.ashnumb", "does not tick"},
        {"status_effects.json", [](JsonValue& doc) { K6At(doc, {"stacking"})->Set("poison", JsonValue::String("stackCount")); },
         "tickIntervalMs.poison", "does not tick"},
        // quests.json setsFlags / permanent.
        {"quests.json", quest(kQuestFlag, setRaw("setsFlags", "[]")), "setsFlags", "empty setsFlags"},
        {"quests.json", quest(kQuestFlag, setRaw("setsFlags", R"(["a", ""])")), "setsFlags", "empty flag id"},
        {"quests.json", quest(kQuestFlag, setRaw("setsFlags", R"(["a", "b", "a"])")), "setsFlags", "duplicate flag 'a'"},
        {"quests.json", permanent(R"({"maxLife": 15, "gold": 1})"), "permanent.gold", "unknown permanent reward field"},
        {"quests.json", permanent(R"({"maxLife": -1})"), "permanent.maxLife", "expected 0..1000"},
        {"quests.json", permanent(R"({"maxLife": 1001})"), "permanent.maxLife", "expected 0..1000"},
        {"quests.json", permanent(R"({"statPoints": 101})"), "permanent.statPoints", "expected 0..100"},
        {"quests.json", permanent(R"({"skillPoints": -2})"), "permanent.skillPoints", "expected 0..100"},
        {"quests.json", permanent(R"({"respecTokens": 500})"), "permanent.respecTokens", "expected 0..100"},
        {"quests.json", permanent(R"({"maxLife": 0})"), "permanent", "grants nothing"},
        // npcs.json conditions and lines.
        {"npcs.json", npc(kGhost, "visibleWhen", "{}"), "visibleWhen", "an empty condition"},
        {"npcs.json", npc(kGhost, "visibleWhen", R"({"flags": "x"})"), "visibleWhen.flags", "unknown condition field"},
        {"npcs.json", npc(kGhost, "visibleWhen", R"("x")"), "visibleWhen", "expected a condition object"},
        {"npcs.json", npc(kGhost, "visibleWhen", R"({"flag": ""})"), "visibleWhen.flag", "empty flag id"},
        {"npcs.json", npc(kGhost, "visibleWhen", R"({"state": "turnedIn"})"), "visibleWhen.state", "state without a quest"},
        {"npcs.json", npc(kGhost, "visibleWhen", StrCat(R"({"quest": ")", kQuestFlag, R"("})")), "visibleWhen",
         "needs its state"},
        {"npcs.json", npc(kGhost, "visibleWhen", StrCat(R"({"quest": ")", kQuestFlag, R"(", "state": "done"})")),
         "visibleWhen.state", "unknown value 'done'"},
        {"npcs.json", npc(kGhost, "visibleWhen", R"({"any": []})"), "visibleWhen.any", "an empty `any`"},
        {"npcs.json", npc(kGhost, "visibleWhen", R"({"any": [{"flag": "a", "notFlag": "b"}]})"), "visibleWhen.any[0]",
         "exactly one of flag / notFlag / quest"},
        {"npcs.json", npc(kGhost, "visibleWhen", R"({"any": [{"flag": "a", "any": []}]})"), "visibleWhen.any[0].any",
         "unknown condition leaf field"},
        {"npcs.json", npc(kGhost, "lines", "[]"), "lines", "an empty lines list"},
        {"npcs.json", npc(kGhost, "lines", R"([{"key": ""}])"), "lines[0].key", "empty line key"},
        {"npcs.json", npc(kGhost, "lines", R"([{"key": "fx.k6.line.first", "if": {"flag": "a"}}])"), "lines[0].if",
         "unknown npc line field"},
        {"npcs.json", npc(kGhost, "lines", R"([{"key": "fx.k6.line.first", "when": {}}])"), "lines[0].when",
         "an empty condition"},
        // maps.json variants.
        {"maps.json", variants("[]"), "variants", "an empty variants list"},
        {"maps.json", variants(StrCat(R"([{"whenFlag": "a", "monsterId": ")", kGnaw, R"(", "count": -1}])")),
         "variants[0].count", "expected 0..100"},
        {"maps.json", variants(StrCat(R"([{"whenFlag": "a", "monsterId": ")", kGnaw, R"(", "count": 101}])")),
         "variants[0].count", "expected 0..100"},
        {"maps.json", variants(StrCat(R"([{"whenFlag": "", "monsterId": ")", kGnaw, R"(", "count": 1}])")),
         "variants[0].whenFlag", "empty flag id"},
        {"maps.json", variants(R"([{"whenFlag": "a", "monsterId": "", "count": 1}])"), "variants[0].monsterId",
         "empty monster id"},
        {"maps.json", variants(StrCat(R"([{"whenFlag": "a", "monsterId": ")", kGnaw, R"(", "count": 1, "at": 2}])")),
         "variants[0].at", "unknown spawn variant field"},
        {"maps.json",
         variants(StrCat(R"([{"whenFlag": "a", "monsterId": ")", kGnaw, R"(", "count": 1}, {"whenFlag": "a", "monsterId": ")",
                         kDead, R"(", "count": 1}])")),
         "variants[1].whenFlag", "a second variant of flag 'a'"},
        // random_events.json champion packs.
        {"random_events.json", champion(StrCat(R"({"chance": 0, "mix": [{"monsterId": ")", kDead, R"(", "count": 1}]})")),
         "ambushChampion.chance", "expected 0 < chance <= 1"},
        {"random_events.json", champion(StrCat(R"({"chance": 1.5, "mix": [{"monsterId": ")", kDead, R"(", "count": 1}]})")),
         "ambushChampion.chance", "expected 0 < chance <= 1"},
        {"random_events.json", champion(R"({"chance": 0.1, "mix": []})"), "ambushChampion.mix", "needs members"},
        {"random_events.json", champion(StrCat(R"({"chance": 0.1, "mix": [{"monsterId": ")", kDead, R"(", "count": 0}]})")),
         "ambushChampion.mix[0].count", "expected >= 1"},
        {"random_events.json",
         champion(StrCat(R"({"chance": 0.1, "mix": [{"monsterId": ")", kDead, R"(", "count": 21}]})")),
         "ambushChampion.mix", "more than 20 members"},
        {"random_events.json",
         champion(StrCat(R"({"chance": 0.1, "mix": [{"monsterId": ")", kDead, R"(", "count": 1}], "leader": ")", kGnaw, "\"}")),
         "ambushChampion.leader", "is not a member"},
        {"random_events.json",
         champion(StrCat(R"({"chance": 0.1, "mix": [{"monsterId": ")", kDead, R"(", "count": 1}], "shared": true})")),
         "ambushChampion.shared", "unknown ambush champion field"},
        {"random_events.json",
         champion(StrCat(R"({"chance": 0.1, "mix": [{"monsterId": ")", kDead, R"(", "count": 1}], "leader": ")", kDead,
                         R"(", "leaderLoot": "legendary"})")),
         "ambushChampion.leaderLoot", "expected magic | rare"},
        {"random_events.json",
         champion(StrCat(R"({"chance": 0.1, "mix": [{"monsterId": ")", kDead, R"(", "count": 1}], "leaderLoot": "magic"})")),
         "ambushChampion.leaderLoot", "needs a leader"},
    };
    for (const Bad& b : bad) {
      INFO(b.file << " " << b.path << ": " << b.text);
      const DataLoadReport r = K6BadLoad(b.file, b.edit);
      CHECK_FALSE(r.Ok());
      CHECK(K6HasIssue(r.errors, b.file, b.path, b.text));
    }
    // A sub-dungeon spawn refuses variants until K7.
    {
      DataLoadReport r = K6BadLoad("maps.json", [](JsonValue& doc) {
        JsonValue* subs = K6At(doc, {"subDungeons"});
        REQUIRE(subs->Size() > 0);
        JsonValue& sd = subs->MutableMembers()[0].value;
        JsonValue* spawns = sd.FindMutable("spawns");
        REQUIRE(spawns != nullptr);
        REQUIRE(spawns->Size() > 0);
        spawns->MutableItems()[0].Set("variants",
                                      K6Parse(StrCat(R"([{"whenFlag": "a", "monsterId": ")", kGnaw, R"(", "count": 1}])")));
      });
      CHECK(K6HasIssue(r.errors, "maps.json", "variants", "a sub-dungeon spawn has no variants until K7"));
    }
  }

  TEST_CASE("K6 data: Finalize checks statuses that never land, keys, condition quests, the visibility validator, variants and champions") {
    // A monster's Ashnumb without the row never lands: refused (on-hit, ability, hazard, on-death, affix on-death).
    {
      K6Edits e = K6BaseEdits();
      e.emplace_back("status_effects.json", [](JsonValue& doc) {
        doc.Remove("stacks");
        K6At(doc, {"stacking"})->Remove("ashnumb");
      });
      e.emplace_back("monsters.json", [](JsonValue& doc) {
        K6At(doc, {"defs", kDead})->Set("abilities", K6Parse(R"([{"id": "ash", "kind": "circle", "clip": "Attack01",
          "windupMs": 800, "cooldownMs": 5000, "aim": "self", "shape": {"radius": 2},
          "status": {"type": "ashnumb", "ms": 6000, "value": 1},
          "hazard": {"durationMs": 2000, "tickMs": 500, "tickDmgMul": 0.1, "status": {"type": "ashnumb", "ms": 6000, "value": 1}}}])"));
        K6At(doc, {"defs", kDead})->Set("onDeath", K6Parse(R"({"kind": "ring", "shape": {"kind": "circle", "radius": 2},
          "status": {"type": "ashnumb", "ms": 6000, "value": 1}})"));
      });
      e.emplace_back("elite_affixes.json", [](JsonValue& doc) {
        K6At(doc, {"definitions", "frozen"})->Set("onDeath", K6Parse(R"({"kind": "ring", "shape": {"kind": "circle", "radius": 2},
          "status": {"type": "ashnumb", "ms": 6000, "value": 1}})"));
      });
      // A hunt's definition, a keyword rule and the elite frozen on-hit naming it never land either.
      std::string huntId;
      e.emplace_back("quest_hunts.json", [&huntId](JsonValue& doc) {
        JsonValue* defs = K6At(doc, {"defsNormal"});
        REQUIRE(defs->Size() > 0);
        huntId = std::string(defs->Members()[0].key);
        K6At(doc, {"defsNormal", huntId})->Set("onDeath", K6Parse(R"({"kind": "ring", "shape": {"kind": "circle", "radius": 2},
          "status": {"type": "ashnumb", "ms": 6000, "value": 1}})"));
      });
      e.emplace_back("status_effects.json", [](JsonValue& doc) {
        K6At(doc, {"monsterKeywordRules"})->Append(K6Parse(R"({"keywords": ["fx_k6_ash"], "status": "ashnumb", "chance": 0.3,
          "value": {"kind": "fixed", "amount": 1}, "durationMs": 3000})"));
        K6At(doc, {"eliteFrozenOnHit"})->Set("status", JsonValue::String("ashnumb"));
      });
      DataLoadReport r;
      K6LoadEdited(e, r);
      INFO(r.Summary());
      REQUIRE_FALSE(huntId.empty());
      CHECK(K6HasIssue(r.errors, "quest_hunts.json", StrCat("defsNormal.", huntId, ".onDeath.status"), "never lands"));
      CHECK(K6HasIssue(r.errors, "status_effects.json", "monsterKeywordRules", "never lands"));
      CHECK(K6HasIssue(r.errors, "status_effects.json", "eliteFrozenOnHit.status", "never lands"));
      CHECK(K6HasIssue(r.errors, "monsters.json", StrCat("defs.", kBiter, ".onHitStatus"), "never lands"));
      CHECK(K6HasIssue(r.errors, "monsters.json", StrCat("defs.", kDead, ".abilities.ash.status"), "never lands"));
      CHECK(K6HasIssue(r.errors, "monsters.json", StrCat("defs.", kDead, ".abilities.ash.hazard.status"), "never lands"));
      CHECK(K6HasIssue(r.errors, "monsters.json", StrCat("defs.", kDead, ".onDeath.status"), "never lands"));
      CHECK(K6HasIssue(r.errors, "elite_affixes.json", "definitions.frozen.onDeath.status", "never lands"));
    }
    // A landing stackCount status needs its name key.
    {
      DataLoadReport r = K6BadLoad("i18n_en.json", [](JsonValue& doc) { K6At(doc, {"strings"})->Remove(kAshnumbName); });
      CHECK(K6HasIssue(r.errors, "status_effects.json", "stacks.ashnumb", kAshnumbName));
    }
    // Line keys exist; condition quests exist; a flag no quest sets warns (lines, visibleWhen, variants).
    {
      DataLoadReport r = K6BadLoad("npcs.json", [](JsonValue& doc) {
        K6At(doc, {"npcs", kTalker})->Set("lines", K6Parse(R"([{"key": "fx.k6.missing", "when": {"quest": "fx_nope", "state": "active"}},
          {"key": "fx.k6.line.first", "when": {"any": [{"flag": "fx_k6_nobody"}]}}])"));
      });
      CHECK(K6HasIssue(r.errors, "npcs.json", StrCat("npcs.", kTalker, ".lines[0].key"), "fx.k6.missing"));
      CHECK(K6HasIssue(r.errors, "npcs.json", StrCat("npcs.", kTalker, ".lines[0].when"), "unknown quest 'fx_nope'"));
      CHECK(K6HasIssue(r.warnings, "npcs.json", StrCat("npcs.", kTalker, ".lines[1].when"), "no quest sets flag 'fx_k6_nobody'"));
      const size_t plain = K6PlainSpawn(test::RealData(), 3);
      DataLoadReport r2 = K6BadLoad("maps.json", K6EditVariants(plain, StrCat(R"([{"whenFlag": "fx_k6_nobody", "monsterId": ")",
                                                                         kGnaw, R"(", "count": 1}])")));
      CHECK(r2.Ok());
      CHECK(K6HasIssue(r2.warnings, "maps.json", "variants[0].whenFlag", "no quest sets flag 'fx_k6_nobody'"));
      // The base set warns about none of its flags (both are set by the flag quest).
      DataLoadReport ok;
      K6LoadEdited(K6BaseEdits(), ok);
      for (const DataIssue& w : ok.warnings) CHECK(w.message.find("no quest sets flag") == std::string::npos);
    }
    // The visibility validator (DataStore runs CheckNpcVisibility): an NPC that could hide while an open quest needs it
    // is an error; hiding it only while the quest is offered is a warning (the quest waits for it).
    {
      // The giver hides once its flag quest is turned in: fine for that quest (nothing needs the giver then), but the
      // giver also gives the reward quest, unrelated to the flag quest, so it could hide while that one is open.
      DataLoadReport mixed = K6BadLoad("npcs.json", [](JsonValue& doc) {
        K6At(doc, {"npcs", kGiver})->Set("visibleWhen", K6Parse(StrCat(R"({"quest": ")", kQuestFlag,
                                                                          R"(", "state": "notTurnedIn"})")));
      });
      INFO(mixed.Summary());
      CHECK(K6HasIssue(mixed.errors, "npcs.json", StrCat("npcs.", kGiver, ".visibleWhen"),
                       StrCat("while quest '", kQuestReward, "' is active and needs it (giver)")));
      CHECK_FALSE(K6HasIssue(mixed.errors, "npcs.json", StrCat("npcs.", kGiver, ".visibleWhen"),
                             StrCat("while quest '", kQuestFlag, "'")));
      // Shown only while its flag quest has not been accepted: the offer is fine, the open phases are not.
      DataLoadReport early = K6BadLoad("npcs.json", [](JsonValue& doc) {
        K6At(doc, {"npcs", kGiver})->Set("visibleWhen", K6Parse(StrCat(R"({"any": [{"quest": ")", kQuestFlag,
                                                                          R"(", "state": "notStarted"}]})")));
      });
      CHECK(K6HasIssue(early.errors, "npcs.json", StrCat("npcs.", kGiver, ".visibleWhen"),
                       StrCat("while quest '", kQuestFlag, "' is active")));
      // Shown only once its own quest is open (or turned in): hidden whenever the flag quest is offered, and nothing
      // else offers it, so it could never be accepted: an error. (The reward quest needs the flag quest here, so the
      // turned-in leaf shows the giver for all of it.)
      const std::string openOrDone = StrCat(R"({"any": [{"quest": ")", kQuestFlag, R"(", "state": "open"}, {"quest": ")",
                                            kQuestFlag, R"(", "state": "turnedIn"}]})");
      const auto needsFlagQuest = K6EditQuest(kQuestReward, [](JsonValue& q) {
        q.Set("prereqQuests", K6Parse(StrCat(R"([")", kQuestFlag, R"("])")));
      });
      {
        K6Edits e = K6BaseEdits();
        e.emplace_back("quests.json", needsFlagQuest);
        e.emplace_back("npcs.json", [&](JsonValue& doc) { K6At(doc, {"npcs", kGiver})->Set("visibleWhen", K6Parse(openOrDone)); });
        DataLoadReport never;
        K6LoadEdited(e, never);
        INFO(never.Summary());
        CHECK(never.errors.size() == 1);
        CHECK(K6HasIssue(never.errors, "npcs.json", StrCat("npcs.", kGiver, ".visibleWhen"),
                         StrCat("hides '", kGiver, "' whenever quest '", kQuestFlag,
                                "' is offered and nothing else offers it: the quest can never be accepted")));
        CHECK_FALSE(K6HasIssue(never.warnings, "npcs.json", StrCat("npcs.", kGiver, ".visibleWhen"), "is offered"));
        // Another NPC that lists the quest too (the talker, always visible) offers it meanwhile: only a warning.
        e.emplace_back("npcs.json", [](JsonValue& doc) {
          K6At(doc, {"npcs", kTalker})->Set("quests", K6Parse(StrCat(R"([")", kQuestFlag, R"("])")));
        });
        DataLoadReport elsewhere;
        K6LoadEdited(e, elsewhere);
        INFO(elsewhere.Summary());
        CHECK(elsewhere.Ok());
        CHECK(K6HasIssue(elsewhere.warnings, "npcs.json", StrCat("npcs.", kGiver, ".visibleWhen"),
                         StrCat("could hide '", kGiver, "' while quest '", kQuestFlag, "' is offered")));
      }
      // Shown while either quest is open: while one is only offered, the other's state is unknown (they are
      // unrelated): the giver could hide then, the quest waits: warnings only, the data loads.
      DataLoadReport offer = K6BadLoad("npcs.json", [](JsonValue& doc) {
        K6At(doc, {"npcs", kGiver})->Set("visibleWhen", K6Parse(StrCat(R"({"any": [{"quest": ")", kQuestFlag,
                                                                          R"(", "state": "open"}, {"quest": ")",
                                                                          kQuestReward, R"(", "state": "open"}]})")));
      });
      INFO(offer.Summary());
      CHECK(offer.Ok());
      CHECK(K6HasIssue(offer.warnings, "npcs.json", StrCat("npcs.", kGiver, ".visibleWhen"),
                       StrCat("while quest '", kQuestFlag, "' is offered")));
      CHECK(K6HasIssue(offer.warnings, "npcs.json", StrCat("npcs.", kGiver, ".visibleWhen"),
                       StrCat("while quest '", kQuestReward, "' is offered")));
      // A reacceptable quest is offered again after it failed: a giver shown before the quest is taken and while it
      // is open, but not once it failed, could never take it back: an error at that phase alone.
      {
        K6Edits e = K6BaseEdits();
        e.emplace_back("quests.json", needsFlagQuest);
        e.emplace_back("quests.json", K6EditQuest(kQuestFlag, [](JsonValue& q) { q.Set("reacceptable", JsonValue::Bool(true)); }));
        e.emplace_back("npcs.json", [](JsonValue& doc) {
          K6At(doc, {"npcs", kGiver})->Set("visibleWhen", K6Parse(StrCat(
              R"({"any": [{"quest": ")", kQuestFlag, R"(", "state": "notStarted"}, {"quest": ")", kQuestFlag,
              R"(", "state": "open"}, {"quest": ")", kQuestFlag, R"(", "state": "turnedIn"}]})")));
        });
        DataLoadReport failed;
        K6LoadEdited(e, failed);
        INFO(failed.Summary());
        CHECK(failed.errors.size() == 1);
        CHECK(K6HasIssue(failed.errors, "npcs.json", StrCat("npcs.", kGiver, ".visibleWhen"),
                         StrCat("whenever quest '", kQuestFlag, "' is offered again after failing")));
        // Not reacceptable: a failed quest is never offered again, nothing to check.
        e.emplace_back("quests.json", K6EditQuest(kQuestFlag, [](JsonValue& q) { q.Set("reacceptable", JsonValue::Bool(false)); }));
        DataLoadReport once;
        K6LoadEdited(e, once);
        INFO(once.Summary());
        CHECK(once.Ok());
        CHECK_FALSE(K6HasIssue(once.warnings, "npcs.json", StrCat("npcs.", kGiver, ".visibleWhen"), "is offered"));
      }
      // The giver seen only after dawn (the flag its own quest sets): hidden while that quest is open.
      DataLoadReport own = K6BadLoad("npcs.json", [](JsonValue& doc) {
        K6At(doc, {"npcs", kGiver})->Set("visibleWhen", K6Parse(StrCat(R"({"flag": ")", kFlagDawn, R"("})")));
      });
      CHECK(K6HasIssue(own.errors, "npcs.json", StrCat("npcs.", kGiver, ".visibleWhen"),
                       StrCat("while quest '", kQuestFlag, "' is active and needs it (giver)")));
    }
    // Variants name a monster of the zone, never the story boss, and the story boss's own entry takes none; champion
    // members are monsters of the zone and never the story boss.
    {
      const DataStore& real = test::RealData();
      const std::string& boss = real.Monsters().ai.storyBossId;
      const MapDef* map = real.FindMap(test::ch1::kZone);
      REQUIRE(map != nullptr);
      size_t bossEntry = map->spawns.size();
      for (size_t i = 0; i < map->spawns.size(); ++i) {
        if (map->spawns[i].monsterId == boss) bossEntry = i;
      }
      REQUIRE(bossEntry < map->spawns.size());
      const size_t plain = K6PlainSpawn(real, 3);
      const auto one = [](std::string_view monster) {
        return StrCat(R"([{"whenFlag": ")", kFlagDawn, R"(", "monsterId": ")", monster, R"(", "count": 1}])");
      };
      DataLoadReport unknown = K6BadLoad("maps.json", K6EditVariants(plain, one("fx_k6_nobody")));
      CHECK(K6HasIssue(unknown.errors, "maps.json", "variants[0].monsterId", "unknown monster 'fx_k6_nobody'"));
      DataLoadReport bossVariant = K6BadLoad("maps.json", K6EditVariants(plain, one(boss)));
      CHECK(K6HasIssue(bossVariant.errors, "maps.json", "variants[0].monsterId", "the story boss keeps a spawn entry"));
      DataLoadReport onBoss = K6BadLoad("maps.json", K6EditVariants(bossEntry, one(kGnaw)));
      CHECK(K6HasIssue(onBoss.errors, "maps.json", StrCat("spawns[", bossEntry, "].variants"),
                       "the story boss's entry has no variants"));
      // An entry whose mix holds the story boss counts as its entry too.
      K6Edits mixed = K6BaseEdits();
      mixed.emplace_back("maps.json", [plain, &boss, &real](JsonValue& doc) {
        const MapSpawnDef& sp = real.FindMap(test::ch1::kZone)->spawns[plain];
        K6At(doc, {"maps", test::ch1::kZone, "spawns"})->MutableItems()[plain].Set("mix", K6Parse(StrCat(
            R"([{"monsterId": ")", sp.monsterId, R"(", "count": )", sp.count - 1, R"(}, {"monsterId": ")", boss,
            R"(", "count": 1}])")));
      });
      mixed.emplace_back("maps.json", K6EditVariants(plain, one(kGnaw)));
      DataLoadReport mixBoss;
      K6LoadEdited(mixed, mixBoss);
      INFO(mixBoss.Summary());
      CHECK(K6HasIssue(mixBoss.errors, "maps.json", StrCat("spawns[", plain, "].variants"),
                       "the story boss's entry has no variants"));
      DataLoadReport champ = K6BadLoad("random_events.json", K6EditChampion(StrCat(
          R"({"chance": 0.1, "mix": [{"monsterId": "fx_k6_nobody", "count": 1}, {"monsterId": ")", boss,
          R"(", "count": 1}]})")));
      CHECK(K6HasIssue(champ.errors, "random_events.json", "ambushChampion.mix", "unknown monster 'fx_k6_nobody'"));
      CHECK(K6HasIssue(champ.errors, "random_events.json", "ambushChampion.mix", "the story boss is never an ambush"));
    }
  }

  // ===================================================================================================================
  // pure rules
  // ===================================================================================================================

  TEST_CASE("K6 rules: quest gates, flag leaves, all / any conditions over a world state") {
    using QS = QuestStatus;
    // QuestGateHolds over (no record, active, completed, turned in, failed).
    struct Row {
      QuestGate gate;
      bool none, active, completed, turnedIn, failed;
    };
    const Row rows[] = {
        {QuestGate::NotStarted, true, false, false, false, false},
        {QuestGate::Active, false, true, false, false, false},
        {QuestGate::Completed, false, false, true, false, false},
        {QuestGate::Open, false, true, true, false, false},
        {QuestGate::TurnedIn, false, false, false, true, false},
        {QuestGate::NotTurnedIn, true, true, true, false, true},
    };
    for (const Row& r : rows) {
      INFO(EnumName(r.gate));
      CHECK(QuestGateHolds(r.gate, false, QS::Active) == r.none);
      CHECK(QuestGateHolds(r.gate, true, QS::Active) == r.active);
      CHECK(QuestGateHolds(r.gate, true, QS::Completed) == r.completed);
      CHECK(QuestGateHolds(r.gate, true, QS::TurnedIn) == r.turnedIn);
      CHECK(QuestGateHolds(r.gate, true, QS::Failed) == r.failed);
    }
    // Conditions over flags and a quest system.
    test::SimHarness h(1, K6Data());
    QuestSystem qs(h.ctx.data, h.events, h.bus);
    qs.Load({K6Record(h.ctx.data, kQuestFlag, QS::TurnedIn), K6Record(h.ctx.data, kQuestReward, QS::Active)});
    std::vector<std::string> flags{"a", "c"};
    const WorldStateView w{flags, &qs};
    const auto leaf = [](WorldLeafKind k, std::string id, QuestGate g = QuestGate::TurnedIn) {
      return WorldConditionLeaf{k, std::move(id), g};
    };
    CHECK(WorldLeafHolds(leaf(WorldLeafKind::Flag, "a"), w));
    CHECK_FALSE(WorldLeafHolds(leaf(WorldLeafKind::Flag, "b"), w));
    CHECK(WorldLeafHolds(leaf(WorldLeafKind::NotFlag, "b"), w));
    CHECK_FALSE(WorldLeafHolds(leaf(WorldLeafKind::NotFlag, "c"), w));
    CHECK(WorldLeafHolds(leaf(WorldLeafKind::Quest, kQuestFlag, QuestGate::TurnedIn), w));
    CHECK(WorldLeafHolds(leaf(WorldLeafKind::Quest, kQuestReward, QuestGate::Open), w));
    CHECK_FALSE(WorldLeafHolds(leaf(WorldLeafKind::Quest, kQuestReward, QuestGate::Completed), w));
    CHECK(WorldLeafHolds(leaf(WorldLeafKind::Quest, "fx_unknown", QuestGate::NotStarted), w));
    CHECK(WorldLeafHolds(leaf(WorldLeafKind::Quest, "fx_unknown", QuestGate::NotTurnedIn), w));
    // No quest system: every quest has no record.
    const WorldStateView bare{flags, nullptr};
    CHECK(WorldLeafHolds(leaf(WorldLeafKind::Quest, kQuestFlag, QuestGate::NotStarted), bare));
    CHECK_FALSE(WorldLeafHolds(leaf(WorldLeafKind::Quest, kQuestFlag, QuestGate::TurnedIn), bare));
    // all: every leaf; any: one of them (when present); both together.
    WorldCondition c;
    CHECK(WorldConditionHolds(c, w));  // empty (the loader refuses one; a default holds)
    c.all = {leaf(WorldLeafKind::Flag, "a"), leaf(WorldLeafKind::NotFlag, "b")};
    CHECK(WorldConditionHolds(c, w));
    c.all.push_back(leaf(WorldLeafKind::Flag, "b"));
    CHECK_FALSE(WorldConditionHolds(c, w));
    c.all.pop_back();
    c.any = {leaf(WorldLeafKind::Flag, "b"), leaf(WorldLeafKind::Flag, "z")};
    CHECK_FALSE(WorldConditionHolds(c, w));
    c.any.push_back(leaf(WorldLeafKind::Quest, kQuestReward, QuestGate::Active));
    CHECK(WorldConditionHolds(c, w));
    c.all.push_back(leaf(WorldLeafKind::NotFlag, "a"));
    CHECK_FALSE(WorldConditionHolds(c, w));  // all fails even when any holds
    WorldCondition anyOnly;
    anyOnly.any = {leaf(WorldLeafKind::Flag, "z"), leaf(WorldLeafKind::Flag, "c")};
    CHECK(WorldConditionHolds(anyOnly, w));
  }

  TEST_CASE("K6 rules: NPC lines now, the panel's line and visibility") {
    std::vector<std::string> flags;
    const WorldStateView w{flags, nullptr};
    NpcDef npc;
    npc.id = "fx";
    CHECK(NpcLinesNow(npc, w).empty());  // no lines: the caller keeps the web's dialogue
    CHECK(NpcPanelLineNow(npc, w).empty());
    CHECK(NpcVisibleNow(npc, w));        // no visibleWhen: always
    NpcLineDef gated;
    gated.key = "k.gated";
    gated.hasWhen = true;
    gated.when.all = {WorldConditionLeaf{WorldLeafKind::Flag, "f", QuestGate::TurnedIn}};
    NpcLineDef always;
    always.key = "k.always";
    NpcLineDef second;
    second.key = "k.second";
    npc.lines = {gated, always, second};
    CHECK(NpcLinesNow(npc, w) == std::vector<std::string>{"k.always", "k.second"});
    CHECK(NpcPanelLineNow(npc, w) == "k.second");  // dialogue[1] ?? [0] over the holding lines
    flags = {"f"};
    const WorldStateView wf{flags, nullptr};
    CHECK(NpcLinesNow(npc, wf) == std::vector<std::string>{"k.gated", "k.always", "k.second"});
    CHECK(NpcPanelLineNow(npc, wf) == "k.always");
    npc.lines = {gated};
    CHECK(NpcPanelLineNow(npc, wf) == "k.gated");  // one line: it
    CHECK(NpcPanelLineNow(npc, w).empty());        // none holds
    npc.hasVisibleWhen = true;
    npc.visibleWhen.all = {WorldConditionLeaf{WorldLeafKind::NotFlag, "f", QuestGate::TurnedIn}};
    CHECK(NpcVisibleNow(npc, w));
    CHECK_FALSE(NpcVisibleNow(npc, wf));
  }

  TEST_CASE("K6 rules: the visibility validator reasons over prerequisites, dependents and flag setters") {
    // A chain A -> B -> C (B needs A, C needs B), D unrelated; the NPC lists B (its giver) and is C's talk target.
    QuestTables qt;
    const auto add = [&](std::string id, std::vector<std::string> prereqs, std::vector<std::string> sets) {
      QuestDef q;
      q.id = id;
      q.prereqQuests = std::move(prereqs);
      q.setsFlags = std::move(sets);
      qt.quests.push_back(std::move(q));
    };
    add("A", {}, {"fA"});
    add("B", {"A"}, {"fB"});
    add("C", {"B"}, {"fC"});
    add("D", {}, {"fD"});
    QuestObjectiveDef talk;
    talk.type = ObjectiveType::Talk;
    talk.targetId = "npc";
    qt.quests[2].objectives.push_back(talk);
    for (const QuestDef& q : qt.quests) CHECK(qt.questIndex.Add(q.id));
    NpcTables nt;
    NpcDef npc;
    npc.id = "npc";
    npc.quests = {"B"};
    npc.hasVisibleWhen = true;
    nt.npcs.push_back(npc);
    const auto issuesFor = [&](WorldCondition c) {
      nt.npcs[0].visibleWhen = std::move(c);
      return CheckNpcVisibility(nt, qt);
    };
    const auto cond = [](std::vector<WorldConditionLeaf> all, std::vector<WorldConditionLeaf> any = {}) {
      WorldCondition c;
      c.all = std::move(all);
      c.any = std::move(any);
      return c;
    };
    const auto flag = [](std::string f) { return WorldConditionLeaf{WorldLeafKind::Flag, std::move(f), QuestGate::TurnedIn}; };
    const auto notFlag = [](std::string f) { return WorldConditionLeaf{WorldLeafKind::NotFlag, std::move(f), QuestGate::TurnedIn}; };
    const auto quest = [](std::string q, QuestGate g) { return WorldConditionLeaf{WorldLeafKind::Quest, std::move(q), g}; };
    // fA is set by B's prerequisite: set while B is offered or open; C's talk too (A is C's prerequisite transitively).
    CHECK(issuesFor(cond({flag("fA")})).empty());
    // fB is set by B itself: unset while B is offered / open (an error: the giver hides), set before C (fine for C).
    {
      const std::vector<NpcVisibilityIssue> is = issuesFor(cond({flag("fB")}));
      REQUIRE(is.size() == 1);
      CHECK(is[0].questId == "B");
      CHECK(is[0].need == NpcNeed::Giver);
      CHECK_FALSE(is[0].offerOnly);
      CHECK(is[0].message.find("is active") != std::string::npos);
    }
    // notFlag fC: C is after B (unset while B runs); for C's own talk, C sets it on its turn-in, after the talk: fine.
    CHECK(issuesFor(cond({notFlag("fC")})).empty());
    // fD: set by an unrelated quest: unknown -> could hide: errors for B (giver) and C (talk).
    {
      const std::vector<NpcVisibilityIssue> is = issuesFor(cond({flag("fD")}));
      REQUIRE(is.size() == 2);
      CHECK((is[0].questId == "B" && is[0].need == NpcNeed::Giver));
      CHECK((is[1].questId == "C" && is[1].need == NpcNeed::Talk));
    }
    // A flag no quest sets is never set: a notFlag of it always holds, a flag of it never.
    CHECK(issuesFor(cond({notFlag("fNobody")})).empty());
    CHECK(issuesFor(cond({flag("fNobody")})).size() == 2);
    // Quest leaves: B's own state. Open holds while active and completed, but never while B is offered: hidden for sure
    // then, with nothing else offering B, B could never be accepted: an error at the offer.
    {
      const std::vector<NpcVisibilityIssue> is = issuesFor(cond({quest("B", QuestGate::Open)}));
      REQUIRE(is.size() == 2);
      CHECK((is[0].questId == "B" && is[0].need == NpcNeed::Giver && !is[0].offerOnly));
      CHECK(is[0].message.find("whenever quest 'B' is offered and nothing else offers it") != std::string::npos);
      CHECK((is[1].questId == "C" && !is[1].offerOnly));  // B is turned in while C runs: Open is false
    }
    // Unknown while offered (D is unrelated): the giver could hide then, the quest waits: a warning.
    {
      const std::vector<NpcVisibilityIssue> is =
          issuesFor(cond({}, {quest("B", QuestGate::Open), quest("D", QuestGate::TurnedIn), quest("C", QuestGate::Active)}));
      REQUIRE(is.size() == 1);  // C's talk: C active holds
      CHECK((is[0].questId == "B" && is[0].offerOnly));
      CHECK(is[0].message.find("could hide 'npc' while quest 'B' is offered and needs it (giver)") != std::string::npos);
    }
    // Other routes to B while it is offered turn the error into a warning: another NPC listing B, or another NPC's
    // dialogue tree with a choice whose questTrigger is B; only when that NPC is visible for sure then.
    {
      NpcDef other;
      other.id = "other";
      other.quests = {"B"};
      nt.npcs.push_back(other);
      std::vector<NpcVisibilityIssue> is = issuesFor(cond({quest("B", QuestGate::Open)}));
      REQUIRE(is.size() == 2);
      CHECK((is[0].questId == "B" && is[0].offerOnly && is[0].message.find("could hide") != std::string::npos));
      nt.npcs[1].hasVisibleWhen = true;  // the other NPC hides once A is turned in (always while B is offered)
      nt.npcs[1].visibleWhen = cond({quest("A", QuestGate::NotTurnedIn)});
      is = issuesFor(cond({quest("B", QuestGate::Open)}));
      REQUIRE(is.size() >= 1);
      CHECK((is[0].npcId == "npc" && is[0].questId == "B" && !is[0].offerOnly));
      nt.npcs[1].visibleWhen = cond({quest("A", QuestGate::TurnedIn)});  // shown for sure while B is offered
      is = issuesFor(cond({quest("B", QuestGate::Open)}));
      CHECK((is[0].npcId == "npc" && is[0].questId == "B" && is[0].offerOnly));
      // A dialogue tree: only through the trees passed in, only when its owner is another NPC.
      nt.npcs[1].hasVisibleWhen = false;
      nt.npcs[1].quests.clear();
      nt.npcs[1].dialogueTreeId = "tree";
      DialogueTables trees;
      DialogueTree tree;
      tree.id = "tree";
      DialogueNode node;
      node.id = "n";
      DialogueChoice plainChoice;
      DialogueChoice accept;
      accept.questTrigger = "B";
      node.choices = {plainChoice, accept};
      tree.nodes = {node};
      trees.trees.push_back(tree);
      nt.npcs[0].visibleWhen = cond({quest("B", QuestGate::Open)});
      is = CheckNpcVisibility(nt, qt);  // no trees given: no route
      CHECK((is[0].questId == "B" && !is[0].offerOnly));
      is = CheckNpcVisibility(nt, qt, &trees);
      CHECK((is[0].questId == "B" && is[0].offerOnly));
      trees.trees[0].nodes[0].choices[1].questTrigger = "C";  // a tree accepting another quest is no route to B
      is = CheckNpcVisibility(nt, qt, &trees);
      CHECK((is[0].questId == "B" && !is[0].offerOnly));
      trees.trees[0].nodes[0].choices[1].questTrigger = "B";
      nt.npcs[1].dialogueTreeId.clear();  // the hidden giver's own tree is no route either
      nt.npcs[0].dialogueTreeId = "tree";
      is = CheckNpcVisibility(nt, qt, &trees);
      CHECK((is[0].questId == "B" && !is[0].offerOnly));
      nt.npcs[0].dialogueTreeId.clear();
      nt.npcs.pop_back();
    }
    // A reacceptable quest is offered again after it failed (a Failed record): checked when the first offer is fine.
    {
      const WorldCondition notStartedOrOpen = cond({}, {quest("B", QuestGate::NotStarted), quest("B", QuestGate::Open)});
      CHECK(issuesFor(notStartedOrOpen).size() == 1);  // C's talk only (B turned in): B never comes back once failed
      qt.quests[1].reacceptable = true;
      std::vector<NpcVisibilityIssue> is = issuesFor(notStartedOrOpen);
      REQUIRE(is.size() == 2);
      CHECK((is[0].questId == "B" && !is[0].offerOnly));
      CHECK(is[0].message.find("whenever quest 'B' is offered again after failing") != std::string::npos);
      // NotTurnedIn holds for a failed record too: fine.
      CHECK(issuesFor(cond({quest("B", QuestGate::NotTurnedIn)})).size() == 1);  // C's talk only
      // Unknown once failed: a warning.
      is = issuesFor(cond({}, {quest("B", QuestGate::NotStarted), quest("B", QuestGate::Open), flag("fD")}));
      REQUIRE(is.size() == 2);
      CHECK((is[0].questId == "B" && is[0].offerOnly));
      CHECK(is[0].message.find("while quest 'B' is offered again after failing") != std::string::npos);
      // A failed first offer is reported alone (one issue per need).
      is = issuesFor(cond({quest("B", QuestGate::Open)}));
      CHECK((is[0].questId == "B" && is[0].message.find("is offered and") != std::string::npos));
      CHECK((is.size() == 2 && is[1].questId == "C"));
      qt.quests[1].reacceptable = false;
    }
    // NotTurnedIn of B: holds while B is offered / open; for C (B turned in) it does not: an error for C only.
    {
      const std::vector<NpcVisibilityIssue> is = issuesFor(cond({quest("B", QuestGate::NotTurnedIn)}));
      REQUIRE(is.size() == 1);
      CHECK(is[0].questId == "C");
    }
    // Active only: B completed would hide the giver (turn-in impossible): an error at the completed phase.
    {
      const std::vector<NpcVisibilityIssue> is = issuesFor(cond({quest("B", QuestGate::Active)}));
      REQUIRE(is.size() >= 1);
      CHECK((is[0].questId == "B" && is[0].message.find("is completed") != std::string::npos));
    }
    // A prerequisite's TurnedIn holds for both; a dependent's NotStarted holds while B runs.
    CHECK(issuesFor(cond({quest("A", QuestGate::TurnedIn)})).empty());
    {
      const std::vector<NpcVisibilityIssue> is = issuesFor(cond({quest("C", QuestGate::NotStarted)}));
      REQUIRE(is.size() == 1);  // fine for B; for C itself (active) it fails
      CHECK(is[0].questId == "C");
    }
    // An unrelated quest's state is unknown.
    CHECK(issuesFor(cond({quest("D", QuestGate::TurnedIn)})).size() == 2);
    // any: one leaf that holds for sure is enough; all + any combine.
    CHECK(issuesFor(cond({}, {flag("fD"), flag("fA")})).empty());
    CHECK(issuesFor(cond({flag("fA")}, {flag("fD"), flag("fNobody")})).size() == 2);
    // A craft NPC is needed while its quest is active: D sets fD on its turn-in, so fD is unset while D runs.
    qt.quests[3].craftPhases.present = true;
    qt.quests[3].craftPhases.craftNpc = "npc";
    {
      const std::vector<NpcVisibilityIssue> is = issuesFor(cond({notFlag("fD")}));
      REQUIRE(is.size() == 2);  // B and C (fD unknown for them); D's craft NPC is fine
      for (const NpcVisibilityIssue& i : is) CHECK(i.questId != "D");
    }
    {
      const std::vector<NpcVisibilityIssue> is = issuesFor(cond({flag("fD")}));
      REQUIRE(is.size() == 3);
      CHECK((is[2].questId == "D" && is[2].need == NpcNeed::Craft && !is[2].offerOnly));
    }
    // NPCs without visibleWhen are never checked.
    nt.npcs[0].hasVisibleWhen = false;
    CHECK(CheckNpcVisibility(nt, qt).empty());
    // QuestsSettingFlag: every setter in quests.json order.
    qt.quests[3].setsFlags.push_back("fA");
    const std::vector<const QuestDef*> setters = QuestsSettingFlag(qt, "fA");
    REQUIRE(setters.size() == 2);
    CHECK(setters[0]->id == "A");
    CHECK(setters[1]->id == "D");
    CHECK(QuestsSettingFlag(qt, "fNobody").empty());
  }

  TEST_CASE("K6 rules: spawn variants (the first set flag wins) and the members they expand to") {
    MapSpawnDef s;
    s.pos = TilePos{5, 5};
    s.monsterId = "base";
    s.count = 3;
    s.leader.present = true;
    s.leader.monsterId = "base";
    s.variants = {MapSpawnVariantDef{"dawn", "gnaw", 2}, MapSpawnVariantDef{"bell", "dead", 0}};
    std::vector<std::string> flags;
    CHECK(ActiveSpawnVariant(s, flags) == nullptr);
    std::vector<SpawnMember> out;
    ExpandSpawnMembersNow(s, flags, out);  // no flag: the entry as defined (count x monsterId, its leader)
    REQUIRE(out.size() == 3);
    CHECK((out[0].monsterId == "base" && out[0].leader && !out[1].leader && !out[2].leader));
    flags = {"bell"};
    REQUIRE(ActiveSpawnVariant(s, flags) == &s.variants[1]);
    ExpandSpawnMembersNow(s, flags, out);
    CHECK(out.empty());  // count 0: nothing stands there any more
    flags = {"bell", "dawn"};  // sorted; dawn's variant is listed first
    REQUIRE(ActiveSpawnVariant(s, flags) == &s.variants[0]);
    ExpandSpawnMembersNow(s, flags, out);
    REQUIRE(out.size() == 2);
    for (const SpawnMember& m : out) CHECK((m.monsterId == "gnaw" && !m.leader));  // a variant's pack has no leader
    // A mixed entry is replaced whole.
    s.mix = {MapSpawnMixDef{"base", 2}, MapSpawnMixDef{"other", 1}};
    flags = {"dawn"};
    ExpandSpawnMembersNow(s, flags, out);
    REQUIRE(out.size() == 2);
    flags.clear();
    ExpandSpawnMembersNow(s, flags, out);
    REQUIRE(out.size() == 3);
    CHECK(out[2].monsterId == "other");
  }

  TEST_CASE("K6 rules: a permanent reward is claimed once per (quest, difficulty); the session's flags are a sorted set") {
    PermanentRewardsState st;
    CHECK(st.Empty());
    QuestPermanentRewardDef r;
    CHECK_FALSE(ClaimPermanentReward(st, "q1", Difficulty::Normal, r));  // absent
    CHECK(st.Empty());
    r.present = true;
    r.maxLife = 15;
    r.statPoints = 3;
    CHECK(ClaimPermanentReward(st, "q1", Difficulty::Normal, r));
    CHECK_FALSE(ClaimPermanentReward(st, "q1", Difficulty::Normal, r));  // the same pair again: nothing
    CHECK(ClaimPermanentReward(st, "q1", Difficulty::Nightmare, r));
    CHECK_FALSE(ClaimPermanentReward(st, "", Difficulty::Hell, r));
    QuestPermanentRewardDef r2;
    r2.present = true;
    r2.skillPoints = 1;
    r2.respecTokens = 2;
    CHECK(ClaimPermanentReward(st, "q2", Difficulty::Normal, r2));
    CHECK(st.maxLife == 30);
    CHECK(st.statPoints == 6);
    CHECK(st.skillPoints == 1);
    CHECK(st.respecTokens == 2);
    REQUIRE(st.claimed.size() == 3);  // grant order
    CHECK(st.claimed[0] == PermanentRewardClaim{"q1", Difficulty::Normal});
    CHECK(st.claimed[1] == PermanentRewardClaim{"q1", Difficulty::Nightmare});
    CHECK(st.claimed[2] == PermanentRewardClaim{"q2", Difficulty::Normal});
    CHECK(st.Claimed("q1", Difficulty::Nightmare));
    CHECK_FALSE(st.Claimed("q2", Difficulty::Hell));
    CHECK_FALSE(st.Empty());
    // The totals saturate at int32 max (a parsed save clamps a hand-edited total there); a negative one counts as 0.
    {
      PermanentRewardsState big;
      big.maxLife = INT32_MAX;
      big.statPoints = INT32_MAX - 1;
      big.skillPoints = -5;
      big.respecTokens = INT32_MAX;
      QuestPermanentRewardDef all;
      all.present = true;
      all.maxLife = 1000;
      all.statPoints = 100;
      all.skillPoints = 2;
      all.respecTokens = 1;
      CHECK(ClaimPermanentReward(big, "q", Difficulty::Hell, all));
      CHECK(big.maxLife == INT32_MAX);
      CHECK(big.statPoints == INT32_MAX);
      CHECK(big.skillPoints == 2);
      CHECK(big.respecTokens == INT32_MAX);
    }
    // Normalize (ApplySave): claims without a quest id, of an unknown difficulty or repeated dropped; totals >= 0.
    {
      PermanentRewardsState n;
      n.claimed = {PermanentRewardClaim{"q", Difficulty::Hell}, PermanentRewardClaim{"", Difficulty::Normal},
                   PermanentRewardClaim{"q2", static_cast<Difficulty>(EnumCount<Difficulty>())},
                   PermanentRewardClaim{"q", Difficulty::Hell}, PermanentRewardClaim{"q", Difficulty::Normal}};
      n.maxLife = -4;
      n.statPoints = 7;
      n.Normalize();
      CHECK(n.claimed == std::vector<PermanentRewardClaim>{PermanentRewardClaim{"q", Difficulty::Hell},
                                                           PermanentRewardClaim{"q", Difficulty::Normal}});
      CHECK((n.maxLife == 0 && n.statPoints == 7));
    }
    // Session flags.
    SessionState s;
    CHECK_FALSE(s.HasFlag("b"));
    CHECK(s.SetFlag("b"));
    CHECK(s.SetFlag("a"));
    CHECK(s.SetFlag("c"));
    CHECK_FALSE(s.SetFlag("a"));  // already set
    CHECK_FALSE(s.SetFlag(""));
    CHECK(s.worldFlags == std::vector<std::string>{"a", "b", "c"});
    CHECK(s.HasFlag("c"));
    CHECK_FALSE(s.HasFlag("d"));
    std::vector<std::string> unsorted{"zz", "", "b", "zz", "a"};
    NormalizeWorldFlags(unsorted);
    CHECK(unsorted == std::vector<std::string>{"a", "b", "zz"});
  }

  TEST_CASE("K6 Ashnumb: stacks add up to the cap, every hit restarts the shared timer; healing and the slow") {
    const StatusEffectRules& rules = K6Data().Classes().statusRules;
    StatusEffectSystem fx(rules);
    const EntityId hero = kHeroEntityId;
    // The first application: one stack (an application adds max(1, floor(value)) stacks).
    StatusApplyResult r = fx.Apply(hero, StatusType::Ashnumb, 1, 6000, 9, 0);
    CHECK(r.outcome == StatusApplyOutcome::Applied);
    CHECK(r.value == 1);
    CHECK(r.effectiveDurationMs == 6000);
    CHECK(fx.Stacks(hero, StatusType::Ashnumb) == 1);
    CHECK(fx.HealingReceivedMul(hero) == doctest::Approx(0.9));
    CHECK(fx.SpeedMultiplier(hero) == 1.0);  // no slow below the cap
    CHECK(fx.EffectsOf(hero).size() == 1);
    CHECK((fx.StatusMask(hero) & (1u << static_cast<uint32_t>(StatusType::Ashnumb))) != 0);
    // Value 0.4 still adds one; value 2 adds two.
    r = fx.Apply(hero, StatusType::Ashnumb, 0.4, 6000, 9, 1000);
    CHECK((r.outcome == StatusApplyOutcome::Refreshed && r.value == 2));
    r = fx.Apply(hero, StatusType::Ashnumb, 2, 6000, 9, 2000);
    CHECK((r.outcome == StatusApplyOutcome::Refreshed && r.value == 4));
    CHECK(fx.EffectsOf(hero).size() == 1);  // one instance
    CHECK(fx.SpeedMultiplier(hero) == 1.0);
    // The fifth stack: -50 % healing and the 20 % slow.
    r = fx.Apply(hero, StatusType::Ashnumb, 1, 6000, 9, 3000);
    CHECK(r.value == 5);
    CHECK(fx.Stacks(hero, StatusType::Ashnumb) == 5);
    CHECK(fx.HealingReceivedMul(hero) == doctest::Approx(0.5));
    CHECK(fx.SpeedMultiplier(hero) == doctest::Approx(0.8));
    // Capped at 5.
    r = fx.Apply(hero, StatusType::Ashnumb, 3, 6000, 9, 4000);
    CHECK((r.outcome == StatusApplyOutcome::Refreshed && r.value == 5));
    // The timer restarted at 4000: still there at 9999 (6000 after the first hit), gone at 10000; never ticks.
    std::vector<StatusTick> ticks;
    std::vector<StatusExpiry> expired;
    fx.Tick(9999, ticks, expired);
    CHECK(ticks.empty());
    CHECK(expired.empty());
    CHECK(fx.Stacks(hero, StatusType::Ashnumb) == 5);
    fx.Tick(10000, ticks, expired);
    CHECK(ticks.empty());
    REQUIRE(expired.size() == 1);
    CHECK(expired[0].type == StatusType::Ashnumb);
    CHECK(fx.Stacks(hero, StatusType::Ashnumb) == 0);
    CHECK(fx.HealingReceivedMul(hero) == 1.0);
    CHECK(fx.SpeedMultiplier(hero) == 1.0);
    // A slow and the cap: the stronger applies; freeze still wins.
    for (int i = 0; i < 5; ++i) fx.Apply(hero, StatusType::Ashnumb, 1, 6000, 9, 20000);
    fx.Apply(hero, StatusType::Slow, 10, 3000, 9, 20000);
    CHECK(fx.SpeedMultiplier(hero) == doctest::Approx(0.8));
    fx.Apply(hero, StatusType::Slow, 30, 3000, 9, 20000);
    CHECK(fx.SpeedMultiplier(hero) == doctest::Approx(0.7));
    fx.Apply(hero, StatusType::Freeze, 1, 1000, 9, 20000);
    CHECK(fx.SpeedMultiplier(hero) == 0.0);
    // Remove (the antidote) takes every stack; other entities are separate.
    fx.Apply(77, StatusType::Ashnumb, 1, 6000, 9, 20000);
    CHECK(fx.Remove(hero, StatusType::Ashnumb));
    CHECK(fx.Stacks(hero, StatusType::Ashnumb) == 0);
    CHECK(fx.Stacks(77, StatusType::Ashnumb) == 1);
    CHECK_FALSE(fx.Remove(hero, StatusType::Ashnumb));
    // A first application above the cap starts at the cap (value 9: five stacks, not nine).
    r = fx.Apply(78, StatusType::Ashnumb, 9, 6000, 9, 20000);
    CHECK((r.outcome == StatusApplyOutcome::Applied && r.value == 5));
    CHECK(fx.Stacks(78, StatusType::Ashnumb) == 5);
    CHECK(fx.HealingReceivedMul(78) == doctest::Approx(0.5));
    // Rejected: value <= 0, duration <= 0; Stacks of a non-stackCount type reads 0.
    CHECK(fx.Apply(hero, StatusType::Ashnumb, 0, 6000, 9, 0).outcome == StatusApplyOutcome::Rejected);
    CHECK(fx.Apply(hero, StatusType::Ashnumb, 1, 0, 9, 0).outcome == StatusApplyOutcome::Rejected);
    CHECK(fx.Stacks(hero, StatusType::Slow) == 0);
    // Without its row Ashnumb never lands and changes nothing (the row taken out of a copy of the real rules, which carry
    // it since the dark remake's content, 2F).
    StatusEffectRules noRow = test::RealData().Classes().statusRules;
    noRow.stacks[EnumIndex(StatusType::Ashnumb)] = StatusStackRule{};
    StatusEffectSystem bare(noRow);
    const StatusApplyResult none = bare.Apply(hero, StatusType::Ashnumb, 1, 6000, 9, 0);
    CHECK(none.outcome == StatusApplyOutcome::Rejected);
    CHECK_FALSE(bare.Has(hero, StatusType::Ashnumb));
    CHECK(bare.HealingReceivedMul(hero) == 1.0);
    // The web's statuses are untouched by the K6 rules (a Slow alone, the poison refresh value).
    StatusEffectSystem web(rules);
    web.Apply(hero, StatusType::Slow, 25, 3000, 9, 0);
    CHECK(web.SpeedMultiplier(hero) == doctest::Approx(0.75));
    const StatusApplyResult p1 = web.Apply(hero, StatusType::Poison, 4, 3000, 9, 0);
    const StatusApplyResult p2 = web.Apply(hero, StatusType::Poison, 2, 3000, 9, 100);
    CHECK((p1.value == 4 && p2.outcome == StatusApplyOutcome::Refreshed && p2.value == 2));
    CHECK(web.HealingReceivedMul(hero) == 1.0);
  }

  TEST_CASE("K6 Ashnumb: a healing cut past 100 % floors at 0; no heal or regeneration takes life") {
    // healingReceivedPerStack 0.3 (the loader allows up to 1) x 5 stacks would be -50 %: the share floors at 0.
    K6Edits e = K6BaseEdits();
    e.emplace_back("status_effects.json", [](JsonValue& doc) {
      K6At(doc, {"stacks", "ashnumb"})->Set("healingReceivedPerStack", K6Parse("0.3"));
    });
    DataLoadReport report;
    const std::unique_ptr<DataStore> store = K6LoadEdited(e, report);
    INFO(report.Summary());
    REQUIRE(report.Ok());
    StatusEffectSystem fx(store->Classes().statusRules);
    fx.Apply(kHeroEntityId, StatusType::Ashnumb, 3, 6000, 9, 0);
    CHECK(fx.HealingReceivedMul(kHeroEntityId) == doctest::Approx(0.1));
    fx.Apply(kHeroEntityId, StatusType::Ashnumb, 1, 6000, 9, 0);
    CHECK(fx.HealingReceivedMul(kHeroEntityId) == 0.0);  // 1 - 1.2
    fx.Apply(kHeroEntityId, StatusType::Ashnumb, 1, 6000, 9, 0);
    CHECK(fx.HealingReceivedMul(kHeroEntityId) == 0.0);  // 1 - 1.5
    // On the hero: a heal gains nothing and takes nothing; regeneration stands still.
    std::unique_ptr<GameSim> sim = K6Start(*store, 41);
    SimContext& ctx = sim->Context();
    Hero& hero = *ctx.sys.hero;
    hero.SetHp(20);
    ctx.sys.status->Apply(kHeroEntityId, StatusType::Ashnumb, 5, 600000, 9, ctx.Now());
    CHECK(hero.HealingReceivedMul() == 0.0);
    CHECK(hero.Heal(30) == 0.0);
    CHECK(hero.Hp() == doctest::Approx(20));
    for (int i = 0; i < 300; ++i) sim->Step();
    CHECK(hero.Hp() == doctest::Approx(20));
    sim->Step();
    CHECK(sim->View().hero.healingReceivedMul == 0.0);
  }

  TEST_CASE("K6 Ashnumb on the hero: heals, potions and regeneration scaled, the antidote cleanses, the snapshot") {
    using test::items_fx::Stack;
    // Two sims on the same seed: one with five stacks, one clean.
    std::unique_ptr<GameSim> sim = K6Start(K6Data(), 41);
    std::unique_ptr<GameSim> clean = K6Start(K6Data(), 41);
    SimContext& ctx = sim->Context();
    Hero& hero = *ctx.sys.hero;
    StatusEffectSystem& status = *ctx.sys.status;
    const double now = ctx.Now();
    // Hero::Heal (potions, life steal, kill heal, thorns, pet heals): x (1 - 0.1 x stacks).
    hero.SetHp(10);
    status.Apply(kHeroEntityId, StatusType::Ashnumb, 2, 600000, 9, now);
    CHECK(hero.HealingReceivedMul() == doctest::Approx(0.8));
    CHECK(hero.Heal(20) == doctest::Approx(16));
    CHECK(hero.Hp() == doctest::Approx(26));
    // A potion: 50 -> 40 at two stacks; 25 at five.
    ItemInstance pot = Stack("c_hp_potion_s", 2);
    ctx.sys.inventory->Items().MutableBag().push_back(pot);
    hero.SetHp(10);
    REQUIRE(ctx.sys.inventory->UseItem(pot.uid) == InvResult::Ok);
    CHECK(hero.Hp() == doctest::Approx(50));
    status.Apply(kHeroEntityId, StatusType::Ashnumb, 3, 600000, 9, now);
    hero.SetHp(10);
    REQUIRE(ctx.sys.inventory->UseItem(pot.uid) == InvResult::Ok);
    CHECK(hero.Hp() == doctest::Approx(35));
    // A heal never goes below 0 or above the max; the snapshot shows the stacks and the share.
    sim->Step();
    CHECK(sim->View().hero.ashnumbStacks == 5);
    CHECK(sim->View().hero.healingReceivedMul == doctest::Approx(0.5));
    CHECK(clean->View().hero.ashnumbStacks == 0);
    CHECK(clean->View().hero.healingReceivedMul == 1.0);
    // Regeneration: the same steps regenerate half as much with five stacks.
    Hero& cleanHero = *clean->Context().sys.hero;
    hero.SetHp(20);
    cleanHero.SetHp(20);
    for (int i = 0; i < 300; ++i) {
      sim->Step();
      clean->Step();
    }
    const double withStacks = hero.Hp() - 20, without = cleanHero.Hp() - 20;
    REQUIRE(without > 0);
    CHECK(withStacks == doctest::Approx(without * 0.5));
    // The antidote cleanses every stack (and poison): EvStatusExpired for each.
    status.Apply(kHeroEntityId, StatusType::Poison, 3, 600000, 9, ctx.Now());
    ItemInstance anti = Stack("c_antidote", 1);
    ctx.sys.inventory->Items().MutableBag().push_back(anti);
    ctx.events.Clear();
    REQUIRE(ctx.sys.inventory->UseItem(anti.uid) == InvResult::Ok);
    CHECK_FALSE(status.Has(kHeroEntityId, StatusType::Ashnumb));
    CHECK_FALSE(status.Has(kHeroEntityId, StatusType::Poison));
    std::vector<StatusType> gone;
    for (const EvStatusExpired& e : K6Of<EvStatusExpired>(ctx.events)) gone.push_back(e.type);
    CHECK(gone == std::vector<StatusType>{StatusType::Poison, StatusType::Ashnumb});
    CHECK(hero.HealingReceivedMul() == 1.0);
    // A standalone hero (no status system bound) heals in full.
    Hero alone(K6Data(), ClassId::Warrior);
    alone.RecalcDerived(EquipStats{});
    alone.FillHpMana();
    alone.SetHp(1);
    CHECK(alone.HealingReceivedMul() == 1.0);
    CHECK(alone.Heal(5) == doctest::Approx(5));
  }

  TEST_CASE("K6 Ashnumb from a monster's hits: one stack per landed hit up to five, the shared timer, the slow") {
    // The K2 rig's systems on the K6 data: the hero bound to its status system, a biter swinging at it.
    test::SimHarness h(5, K6Data());
    auto hero = std::make_unique<Hero>(h.ctx.data, ClassId::Warrior);
    StatusEffectSystem status(h.ctx.data.Classes().statusRules);
    hero->BindStatusEffects(&status);
    ZoneRuntime zone(h.ctx);
    HeroLocomotion loco(h.ctx);
    MonsterSystem monsters(h.ctx);
    ProjectileSystem projectiles(h.ctx);
    CombatSystem combat(h.ctx);
    SoulEchoSystem soul(h.ctx);
    RewardService rewards(h.ctx);
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
    h.onTimer = [&](const Timer& t) {
      if (t.owner == TimerOwner::Combat) combat.OnTimer(t);
      if (t.owner == TimerOwner::Projectiles) projectiles.OnTimer(t);
      if (t.owner == TimerOwner::Monsters) monsters.OnTimer(t);
    };
    h.session.currentMap = h.ctx.data.World().defaultMap;
    REQUIRE(zone.EnterZone(h.ctx.data.World().defaultMap, true, Vec2(40, 40)));
    monsters.SetWorldForTesting(MOpenWorld(zone.Map().cols, zone.Map().rows));
    hero->Skills().InitStarterLevels();
    hero->RecalcDerived(h.ctx.equip);
    hero->FillHpMana();
    loco.OnZoneEnter();
    combat.OnZoneEnter();
    // The hero holds a lock on a far idle dummy (it never swings at the biter).
    MonsterSpawnParams dp;
    dp.baseDef = &test::fixture::Grunt();
    dp.tile = TilePos{70, 70};
    hero->attackTarget = monsters.Spawn(dp);
    MonsterSpawnParams bp;
    bp.baseDef = h.ctx.data.Monsters().Find(kBiter);
    REQUIRE(bp.baseDef != nullptr);
    bp.tile = TilePos{41, 40};
    const EntityId biter = monsters.Spawn(bp);
    MonsterInstance* b = monsters.Find(biter);
    REQUIRE(b != nullptr);
    b->stats.dex = 0;
    b->stats.lck = 0;
    b->state = MonsterState::Attack;
    h.events.Clear();
    std::vector<EvStatusApplied> applied;
    for (int i = 0; i < 60 * 12 && applied.size() < 6; ++i) {
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
      hero->SetHp(hero->MaxHp());  // keeps the hero standing
      applied.clear();
      for (const EvStatusApplied& a : K6Of<EvStatusApplied>(h.events)) {
        if (a.target == kHeroEntityId && a.type == StatusType::Ashnumb) applied.push_back(a);
      }
    }
    REQUIRE(applied.size() == 6);
    // EvStatusApplied carries the stacks after each hit (not the rule's value 1) and the refresh flag.
    for (size_t i = 0; i < applied.size(); ++i) {
      CHECK(applied[i].value == static_cast<double>((std::min)(i + 1, size_t{5})));
      CHECK(applied[i].refreshed == (i > 0));
      CHECK(applied[i].durationMs == 6000);
    }
    CHECK(status.Stacks(kHeroEntityId, StatusType::Ashnumb) == 5);
    CHECK(status.SpeedMultiplier(kHeroEntityId) == doctest::Approx(0.8));
    CHECK(hero->HealingReceivedMul() == doctest::Approx(0.5));
    // The applied log line names the status (sys.statusEffect.name.ashnumb), once (the first application).
    size_t logs = 0;
    for (const EvLog& l : K6Of<EvLog>(h.events)) {
      if (l.text.key != "sys.statusEffect.applied") continue;
      for (const I18nArg& a : l.text.args) logs += a.value == kAshnumbName ? 1 : 0;
    }
    CHECK(logs == 1);
  }

  // ===================================================================================================================
  // permanent rewards, world flags, saves
  // ===================================================================================================================

  TEST_CASE("K6 permanent rewards: once per (quest, difficulty) at the turn-in; max life, free points, tokens; the save") {
    const DataStore& d = K6Data();
    std::unique_ptr<GameSim> sim = K6Start(d, 43);
    SimContext& ctx = sim->Context();
    Hero& hero = *ctx.sys.hero;
    QuestWorld& qw = *ctx.sys.questWorld;
    const double maxHpBefore = hero.MaxHp();
    const int32_t statBefore = hero.FreeStatPoints(), skillBefore = hero.FreeSkillPoints();
    ctx.sys.quests->Load({K6Record(d, kQuestReward, QuestStatus::Completed)});
    ctx.events.Clear();
    REQUIRE(qw.TurnIn(kQuestReward, 0));
    const std::span<const Event> ev(ctx.events.Items());
    const std::vector<EvPermanentReward> grants = K6Of<EvPermanentReward>(ev);
    REQUIRE(grants.size() == 1);
    CHECK(grants[0].questId == kQuestReward);
    CHECK(grants[0].difficulty == Difficulty::Normal);
    CHECK((grants[0].maxLife == 15 && grants[0].statPoints == 3 && grants[0].skillPoints == 1 && grants[0].respecTokens == 1));
    // After the gold, before the turn-in's save request.
    CHECK(K6IndexOf<EvGoldChanged>(ev) < K6IndexOf<EvPermanentReward>(ev));
    CHECK(K6IndexOf<EvPermanentReward>(ev) < K6IndexOf<EvSaveRequested>(ev));
    const PermanentRewardsState& st = ctx.session.permanentRewards;
    CHECK((st.maxLife == 15 && st.statPoints == 3 && st.skillPoints == 1 && st.respecTokens == 1));
    REQUIRE(st.claimed.size() == 1);
    CHECK(st.claimed[0] == PermanentRewardClaim{kQuestReward, Difficulty::Normal});
    CHECK(hero.FreeStatPoints() == statBefore + 3);
    CHECK(hero.FreeSkillPoints() == skillBefore + 1);
    CHECK(ctx.equip.Get(Stat::MaxHp) == doctest::Approx(15));  // the equip-stat merge (EquipStatsDirtyMsg)
    sim->Step();
    CHECK(hero.MaxHp() == doctest::Approx(maxHpBefore + 15));
    REQUIRE(sim->View().permanentRewards != nullptr);
    CHECK(*sim->View().permanentRewards == st);
    // The same quest again on the same difficulty grants nothing.
    ctx.sys.quests->Load({K6Record(d, kQuestReward, QuestStatus::Completed)});
    ctx.events.Clear();
    REQUIRE(qw.TurnIn(kQuestReward, 0));
    CHECK(K6Of<EvPermanentReward>(ctx.events).empty());
    CHECK(st.maxLife == 15);
    CHECK(hero.FreeStatPoints() == statBefore + 3);
    // On another difficulty it is granted again (Nightmare after a completed Normal, as a load would derive it).
    ctx.session.difficulty = Difficulty::Nightmare;
    ctx.session.completedDifficulties.assign(1, Difficulty::Normal);  // (not `= {...}`: GCC 13's ASan build warns)
    ctx.sys.quests->Load({K6Record(d, kQuestReward, QuestStatus::Completed)});
    ctx.events.Clear();
    REQUIRE(qw.TurnIn(kQuestReward, 0));
    REQUIRE(K6Of<EvPermanentReward>(ctx.events).size() == 1);
    CHECK(K6Of<EvPermanentReward>(ctx.events)[0].difficulty == Difficulty::Nightmare);
    CHECK((st.maxLife == 30 && st.statPoints == 6 && st.skillPoints == 2 && st.respecTokens == 2));
    CHECK(st.claimed.size() == 2);
    sim->Step();
    CHECK(hero.MaxHp() == doctest::Approx(maxHpBefore + 30));
    // A quest without one grants nothing.
    ctx.sys.quests->Load({K6Record(d, kQuestFlag, QuestStatus::Completed)});
    ctx.events.Clear();
    REQUIRE(qw.TurnIn(kQuestFlag, 0));
    CHECK(K6Of<EvPermanentReward>(ctx.events).empty());
    // The save round trip: the claims and totals come back, max life with them, the points in the hero's free points.
    const std::string json = sim->SaveGame(7);
    CHECK(json.find("\"permanentRewards\"") != std::string::npos);
    std::unique_ptr<GameSim> loaded = K6Start(d, 1);
    std::string err;
    REQUIRE(loaded->LoadGame(json, &err) == SaveError::None);
    CHECK(loaded->Context().session.permanentRewards == st);
    CHECK(loaded->Context().sys.hero->FreeStatPoints() == hero.FreeStatPoints());
    CHECK(loaded->Context().sys.hero->MaxHp() == doctest::Approx(hero.MaxHp()));
    CHECK(loaded->SaveGame(7) == json);  // save -> load -> save is exact
    // A hand-edited save whose totals sit at int32 max (1e12 parses clamped there), and a hero whose free points do:
    // one more grant (Hell, unclaimed) saturates instead of wrapping, so the max life stays.
    JsonValue raw;
    REQUIRE(ParseJson(json, raw));
    for (const char* key : {"maxLife", "statPoints", "skillPoints", "respecTokens"}) {
      K6At(raw, {"permanentRewards"})->Set(key, K6Parse("1e12"));
    }
    std::unique_ptr<GameSim> edited = K6Start(d, 1);
    REQUIRE(edited->LoadGame(WriteJson(raw), &err) == SaveError::None);
    SimContext& ectx = edited->Context();
    CHECK(ectx.session.permanentRewards.maxLife == INT32_MAX);
    ectx.sys.hero->MutableFreeSkillPoints() = INT32_MAX;
    const double heroHpBefore = ectx.sys.hero->MaxHp();
    ectx.session.difficulty = Difficulty::Hell;
    ectx.session.completedDifficulties.assign(1, Difficulty::Normal);
    ectx.session.completedDifficulties.push_back(Difficulty::Nightmare);
    ectx.sys.quests->Load({K6Record(d, kQuestReward, QuestStatus::Completed)});
    ectx.events.Clear();
    REQUIRE(ectx.sys.questWorld->TurnIn(kQuestReward, 0));
    REQUIRE(K6Of<EvPermanentReward>(ectx.events).size() == 1);
    const PermanentRewardsState& sat = ectx.session.permanentRewards;
    CHECK((sat.maxLife == INT32_MAX && sat.statPoints == INT32_MAX && sat.skillPoints == INT32_MAX &&
           sat.respecTokens == INT32_MAX));
    CHECK(ectx.sys.hero->FreeSkillPoints() == INT32_MAX);
    CHECK(ectx.equip.Get(Stat::MaxHp) == doctest::Approx(static_cast<double>(INT32_MAX)));
    edited->Step();
    CHECK(ectx.sys.hero->MaxHp() >= heroHpBefore);
  }

  TEST_CASE("K6 world flags: set on the turn-in (first listener, data order), a sorted set, the snapshot, the save") {
    const DataStore& d = K6Data();
    std::unique_ptr<GameSim> sim = K6Start(d, 45);
    SimContext& ctx = sim->Context();
    // A save without world state writes neither field (byte-identical to the format before K6).
    const std::string before = sim->SaveGame(3);
    CHECK(before.find("worldFlags") == std::string::npos);
    CHECK(before.find("permanentRewards") == std::string::npos);
    ctx.sys.quests->Load({K6Record(d, kQuestFlag, QuestStatus::Completed)});
    ctx.events.Clear();
    REQUIRE(ctx.sys.questWorld->TurnIn(kQuestFlag, 0));
    const std::span<const Event> ev(ctx.events.Items());
    const std::vector<EvWorldFlag> set = K6Of<EvWorldFlag>(ev);
    REQUIRE(set.size() == 2);
    CHECK((set[0].flag == kFlagDawn && set[0].questId == kQuestFlag));  // the quest's data order
    CHECK((set[1].flag == kFlagBell && set[1].questId == kQuestFlag));
    // The first QuestTurnedInMsg listener: before the turn-in cue (audio) and every reward.
    const int32_t flagAt = K6IndexOf<EvWorldFlag>(ev);
    CHECK(flagAt < K6IndexOf<EvSfx>(ev));
    CHECK(flagAt < K6IndexOf<EvExpGained>(ev));
    CHECK(ctx.session.worldFlags == std::vector<std::string>{kFlagBell, kFlagDawn});  // sorted
    CHECK(ctx.session.HasFlag(kFlagDawn));
    sim->Step();
    REQUIRE(sim->View().worldFlags != nullptr);
    CHECK(*sim->View().worldFlags == ctx.session.worldFlags);
    // A flag already set is not announced again.
    ctx.sys.quests->Load({K6Record(d, kQuestFlag, QuestStatus::Completed)});
    ctx.events.Clear();
    REQUIRE(ctx.sys.questWorld->TurnIn(kQuestFlag, 0));
    CHECK(K6Of<EvWorldFlag>(ctx.events).empty());
    CHECK(ctx.session.worldFlags.size() == 2);
    // The save round trip.
    const std::string json = sim->SaveGame(3);
    CHECK(json.find(StrCat(R"("worldFlags":[")", kFlagBell, R"(",")", kFlagDawn, R"("])")) != std::string::npos);
    std::unique_ptr<GameSim> loaded = K6Start(d, 1);
    std::string err;
    REQUIRE(loaded->LoadGame(json, &err) == SaveError::None);
    CHECK(loaded->Context().session.worldFlags == ctx.session.worldFlags);
    CHECK(loaded->SaveGame(3) == json);
    // An old save (written before K6: no fields) loads with none; so does the one this session wrote before the turn-in.
    SaveData parsed;
    REQUIRE(ParseSave(json, parsed) == SaveError::None);
    JsonValue raw;
    REQUIRE(ParseJson(json, raw));
    raw.Remove("worldFlags");
    raw.Remove("permanentRewards");
    REQUIRE(loaded->LoadGame(WriteJson(raw), &err) == SaveError::None);
    CHECK(loaded->Context().session.worldFlags.empty());
    CHECK(loaded->Context().session.permanentRewards.Empty());
    REQUIRE(loaded->LoadGame(before, &err) == SaveError::None);
    CHECK(loaded->Context().session.worldFlags.empty());
    // A SaveData built in code (LoadGame(const SaveData&)) is normalised like a parsed one: the flags become a sorted
    // set (so dawn reads as set and the dawn-gated ghost stands), the claims and totals are cleaned.
    SaveData built;
    sim->BuildSave(built, 0);
    built.worldFlags = {"zzz_other", kFlagDawn, "", kFlagDawn};
    built.permanentRewards.claimed = {PermanentRewardClaim{"", Difficulty::Normal}, PermanentRewardClaim{"q", Difficulty::Hell},
                                      PermanentRewardClaim{"q", Difficulty::Hell}};
    built.permanentRewards.maxLife = -3;
    REQUIRE(loaded->LoadGame(built, &err) == SaveError::None);
    CHECK(loaded->Context().session.worldFlags == std::vector<std::string>{kFlagDawn, "zzz_other"});
    CHECK(loaded->Context().session.HasFlag(kFlagDawn));
    CHECK(loaded->Context().session.permanentRewards.claimed ==
          std::vector<PermanentRewardClaim>{PermanentRewardClaim{"q", Difficulty::Hell}});
    CHECK(loaded->Context().session.permanentRewards.maxLife == 0);
    const ZoneRuntime& lz = *loaded->Context().sys.zone;
    REQUIRE(lz.MapId() == test::ch1::kZone);
    CHECK_FALSE(lz.IsNpcHidden(kGhost));
    // A new game starts with none.
    std::unique_ptr<GameSim> fresh = K6Start(d, 45);
    CHECK(fresh->Context().session.worldFlags.empty());
  }

  TEST_CASE("K6 saves: worldFlags / permanentRewards serialise last and only when present; parsing normalises them") {
    SaveData s;
    s.hasStorySeen = true;  // (parsing defaults a missing storySeen to ["prologue"])
    const std::string empty = SerializeSave(s);
    CHECK(empty.find("worldFlags") == std::string::npos);
    CHECK(empty.find("permanentRewards") == std::string::npos);
    s.worldFlags = {"a", "b"};
    s.permanentRewards.claimed = {PermanentRewardClaim{"q1", Difficulty::Hell}};
    s.permanentRewards.maxLife = 15;
    s.permanentRewards.respecTokens = 2;
    const std::string json = SerializeSave(s);
    const size_t cv = json.find("\"contentVersion\"");
    const size_t wf = json.find("\"worldFlags\"");
    const size_t pr = json.find("\"permanentRewards\"");
    REQUIRE((cv != std::string::npos && wf != std::string::npos && pr != std::string::npos));
    CHECK((cv < wf && wf < pr));  // after the v4 fields, in this order
    CHECK(json.find(R"("permanentRewards":{"claimed":[{"questId":"q1","difficulty":"hell"}],"maxLife":15,"statPoints":0,)"
                    R"("skillPoints":0,"respecTokens":2})") != std::string::npos);
    SaveData back;
    REQUIRE(ParseSave(json, back) == SaveError::None);
    CHECK(back.worldFlags == s.worldFlags);
    CHECK(back.permanentRewards == s.permanentRewards);
    CHECK(SerializeSave(back) == json);
    // Only one of them present.
    SaveData onlyFlags;
    onlyFlags.worldFlags = {"x"};
    CHECK(SerializeSave(onlyFlags).find("permanentRewards") == std::string::npos);
    // Parsing normalises a hand-edited save: flags sorted, empty / duplicate / non-string ones dropped; claims without a
    // quest id or a known difficulty dropped, a repeated pair once; totals floored, clamped to >= 0, non-numbers 0.
    JsonValue raw;
    REQUIRE(ParseJson(empty, raw));
    raw.Set("worldFlags", K6Parse(R"(["c", "a", "", 7, "c", "b"])"));
    raw.Set("permanentRewards", K6Parse(R"({"claimed": [{"questId": "q", "difficulty": "nightmare"}, {"questId": "",
      "difficulty": "normal"}, {"questId": "q2", "difficulty": "legendary"}, {"difficulty": "normal"},
      {"questId": "q", "difficulty": "nightmare"}, 5], "maxLife": 12.7, "statPoints": -3, "skillPoints": "2",
      "respecTokens": 1e12})"));
    SaveData norm;
    REQUIRE(ParseSave(WriteJson(raw), norm) == SaveError::None);
    CHECK(norm.worldFlags == std::vector<std::string>{"a", "b", "c"});
    REQUIRE(norm.permanentRewards.claimed.size() == 1);
    CHECK(norm.permanentRewards.claimed[0] == PermanentRewardClaim{"q", Difficulty::Nightmare});
    CHECK(norm.permanentRewards.maxLife == 12);
    CHECK(norm.permanentRewards.statPoints == 0);
    CHECK(norm.permanentRewards.skillPoints == 0);
    CHECK(norm.permanentRewards.respecTokens == 2147483647);
    // Wrong types read as none.
    raw.Set("worldFlags", K6Parse(R"({"a": 1})"));
    raw.Set("permanentRewards", K6Parse("[1, 2]"));
    SaveData wrong;
    REQUIRE(ParseSave(WriteJson(raw), wrong) == SaveError::None);
    CHECK(wrong.worldFlags.empty());
    CHECK(wrong.permanentRewards.Empty());
    // The content-version upgrade keeps them (flags and claims are not data ids it drops).
    SaveData old = back;
    old.contentVersion = 1;
    ApplyContentVersion(test::RealData(), old);
    CHECK(old.worldFlags == s.worldFlags);
    CHECK(old.permanentRewards == s.permanentRewards);
  }

  // ===================================================================================================================
  // spawn variants, NPC lines and visibility
  // ===================================================================================================================

  TEST_CASE("K6 spawn variants: the zone's spawn and every respawn read the active variant; the guide follows") {
    const DataStore& d = K6Data();
    const size_t entry = K6PlainSpawn(test::RealData(), 3);
    const MapSpawnDef& sp = d.FindMap(test::ch1::kZone)->spawns[entry];
    std::unique_ptr<GameSim> sim = K6Start(d, 47);
    SimContext& ctx = sim->Context();
    MonsterSystem& ms = *ctx.sys.monsters;
    // No flag: the entry spawns as authored, every member with its entry and slot; placements are the real data's.
    std::vector<const MonsterInstance*> members = K6EntryMonsters(ms, entry);
    REQUIRE(static_cast<int32_t>(members.size()) == sp.count);
    for (size_t k = 0; k < members.size(); ++k) {
      CHECK(members[k]->def.id == sp.monsterId);
      CHECK(members[k]->spawnMember == static_cast<int32_t>(k));
    }
    {
      // The placements (the M10 draws) are the real data's: no flag, no change.
      std::unique_ptr<GameSim> real = K6Start(test::RealData(), 47);
      std::vector<std::pair<std::string, TilePos>> a, b;
      for (const MonsterInstance& m : ms.All()) a.emplace_back(m.def.id, m.spawnAnchor);
      for (const MonsterInstance& m : real->Context().sys.monsters->All()) b.emplace_back(m.def.id, m.spawnAnchor);
      CHECK(a == b);
    }
    // Dawn: the slot of member 0 comes back as the dawn variant's gnawer; member 2's slot (past the variant's 2) stays
    // down; respawns of other entries are untouched.
    ctx.session.SetFlag(kFlagDawn);
    const EntityId first = members[0]->id, third = members[2]->id;
    const uint32_t pack = members[0]->packId;
    const TilePos anchor = members[0]->spawnAnchor;
    K6Kill(ctx, first);
    K6Kill(ctx, third);
    const double delay = d.Monsters().ai.respawnDelayMs;
    const int steps = static_cast<int>(std::ceil(delay / kSimStepMs)) + 10;
    for (int i = 0; i < steps; ++i) sim->Step();
    std::vector<const MonsterInstance*> now = K6EntryMonsters(ms, entry);
    const MonsterInstance* back = nullptr;
    for (const MonsterInstance* m : now) {
      if (m->spawnMember == 0) back = m;
      CHECK(m->spawnMember != 2);  // never back
    }
    REQUIRE(back != nullptr);
    CHECK(back->def.id == kGnaw);
    CHECK(back->id != first);
    CHECK_FALSE(back->packLeader);
    CHECK(back->packId == pack);  // it keeps the entry's pack
    CHECK(back->spawnAnchor == anchor);
    const MonsterInstance* deadThird = ms.Find(third);
    REQUIRE(deadThird != nullptr);  // the dead entry stays listed (no respawn)
    CHECK_FALSE(deadThird->IsAlive());
    // The quest guide's spawn points read the variant: the gnawer stands at the entry, its old monster does not.
    const GuideWorld guide = ctx.sys.questWorld->MakeGuideWorld();
    std::vector<Vec2> pts;
    guide.spawns({std::string(kGnaw)}, pts);
    CHECK(std::find(pts.begin(), pts.end(), sp.pos.Center()) != pts.end());
    pts.clear();
    guide.spawns({sp.monsterId}, pts);
    CHECK(std::find(pts.begin(), pts.end(), sp.pos.Center()) == pts.end());
    // A load (zone entry with the flag): the entry spawns its variant whole.
    const std::string json = sim->SaveGame(9);
    std::unique_ptr<GameSim> loaded = K6Start(d, 1);
    std::string err;
    REQUIRE(loaded->LoadGame(json, &err) == SaveError::None);
    std::vector<const MonsterInstance*> fresh = K6EntryMonsters(*loaded->Context().sys.monsters, entry);
    REQUIRE(fresh.size() == 2);
    for (const MonsterInstance* m : fresh) {
      CHECK(m->def.id == kGnaw);
      CHECK_FALSE(m->packLeader);
      CHECK(m->packId == fresh[0]->packId);
    }
    // With the bell set too, dawn (listed first) still wins.
    loaded->Context().session.SetFlag(kFlagBell);
    const std::string json2 = loaded->SaveGame(9);
    REQUIRE(loaded->LoadGame(json2, &err) == SaveError::None);
    fresh = K6EntryMonsters(*loaded->Context().sys.monsters, entry);
    REQUIRE(fresh.size() == 2);
    CHECK(fresh[0]->def.id == kGnaw);
  }

  TEST_CASE("K6 spawn variants: a variant of count 0 empties the entry; the bell variant alone spawns its one monster") {
    const DataStore& d = K6Data();
    const size_t entry = K6PlainSpawn(test::RealData(), 3);
    std::unique_ptr<GameSim> sim = K6Start(d, 49);
    SimContext& ctx = sim->Context();
    ctx.session.SetFlag(kFlagBell);
    const std::string json = sim->SaveGame(9);
    std::string err;
    REQUIRE(sim->LoadGame(json, &err) == SaveError::None);
    std::vector<const MonsterInstance*> m = K6EntryMonsters(*sim->Context().sys.monsters, entry);
    REQUIRE(m.size() == 1);
    CHECK(m[0]->def.id == kDead);
    // Count 0.
    DataLoadReport report;
    K6Edits e = K6BaseEdits();
    e.emplace_back("maps.json", K6EditVariants(entry, StrCat(R"([{"whenFlag": ")", kFlagBell, R"(", "monsterId": ")", kGnaw,
                                                               R"(", "count": 0}])")));
    auto store = K6LoadEdited(e, report);
    INFO(report.Summary());
    REQUIRE(report.Ok());
    std::unique_ptr<GameSim> empty = K6Start(*store, 49);
    REQUIRE(static_cast<int32_t>(K6EntryMonsters(*empty->Context().sys.monsters, entry).size()) ==
            store->FindMap(test::ch1::kZone)->spawns[entry].count);
    // Every slot stays down once the flag is set.
    SimContext& ectx = empty->Context();
    ectx.session.SetFlag(kFlagBell);
    for (const MonsterInstance* x : K6EntryMonsters(*ectx.sys.monsters, entry)) K6Kill(ectx, x->id);
    const int steps = static_cast<int>(std::ceil(store->Monsters().ai.respawnDelayMs / kSimStepMs)) + 10;
    for (int i = 0; i < steps; ++i) empty->Step();
    CHECK(K6EntryMonsters(*ectx.sys.monsters, entry).empty());
    // The quest guide points nowhere for it: a count-0 variant stands nothing there.
    const Vec2 entryAt = store->FindMap(test::ch1::kZone)->spawns[entry].pos.Center();
    std::vector<Vec2> pts;
    ectx.sys.questWorld->MakeGuideWorld().spawns({std::string(kGnaw)}, pts);
    CHECK(std::find(pts.begin(), pts.end(), entryAt) == pts.end());
    pts.clear();
    ectx.sys.questWorld->MakeGuideWorld().spawns({store->FindMap(test::ch1::kZone)->spawns[entry].monsterId}, pts);
    CHECK(std::find(pts.begin(), pts.end(), entryAt) == pts.end());
  }

  TEST_CASE("K6 spawn variants: a led entry's leader slot comes back as the variant's monster, leading nothing") {
    const DataStore& base = K6Data();
    const size_t entry = K6PlainSpawn(test::RealData(), 3);
    const std::string own = base.FindMap(test::ch1::kZone)->spawns[entry].monsterId;
    DataLoadReport report;
    K6Edits e = K6BaseEdits();
    e.emplace_back("maps.json", [entry, own](JsonValue& doc) {
      JsonValue* spawns = K6At(doc, {"maps", test::ch1::kZone, "spawns"});
      spawns->MutableItems()[entry].Set("leader", K6Parse(StrCat(R"({"monsterId": ")", own, R"("})")));
    });
    auto store = K6LoadEdited(e, report);
    INFO(report.Summary());
    REQUIRE(report.Ok());
    std::unique_ptr<GameSim> sim = K6Start(*store, 57);
    SimContext& ctx = sim->Context();
    std::vector<const MonsterInstance*> members = K6EntryMonsters(*ctx.sys.monsters, entry);
    REQUIRE(members.size() >= 2);
    REQUIRE(members[0]->packLeader);  // the entry's own monster leads it
    const uint32_t pack = members[0]->packId;
    const EntityId leader = members[0]->id;
    ctx.session.SetFlag(kFlagDawn);
    K6Kill(ctx, leader);
    const int steps = static_cast<int>(std::ceil(store->Monsters().ai.respawnDelayMs / kSimStepMs)) + 10;
    for (int i = 0; i < steps; ++i) sim->Step();
    const MonsterInstance* back = nullptr;
    for (const MonsterInstance* m : K6EntryMonsters(*ctx.sys.monsters, entry)) {
      if (m->spawnMember == 0 && m->id != leader) back = m;
    }
    REQUIRE(back != nullptr);
    CHECK(back->def.id == kGnaw);
    CHECK_FALSE(back->packLeader);  // a variant's pack has no leader
    CHECK(back->packId == pack);
    // Without the flag the leader comes back as itself, leading again (K5).
    std::unique_ptr<GameSim> plain = K6Start(*store, 57);
    SimContext& pctx = plain->Context();
    const EntityId lead2 = K6EntryMonsters(*pctx.sys.monsters, entry)[0]->id;
    K6Kill(pctx, lead2);
    for (int i = 0; i < steps; ++i) plain->Step();
    const MonsterInstance* again = nullptr;
    for (const MonsterInstance* m : K6EntryMonsters(*pctx.sys.monsters, entry)) {
      if (m->spawnMember == 0 && m->id != lead2) again = m;
    }
    REQUIRE(again != nullptr);
    CHECK(again->def.id == own);
    CHECK(again->packLeader);
  }

  TEST_CASE("K6 NPC lines: the first holding line is logged, the panel shows the second; none holding: the web's line") {
    const DataStore& d = K6Data();
    std::unique_ptr<GameSim> sim = K6Start(d, 51);
    SimContext& ctx = sim->Context();
    QuestWorld& qw = *ctx.sys.questWorld;
    DialogueSystem& dlg = *ctx.sys.dialogue;
    // The talker's lines wait for the bell: its web line is logged, the panel keeps the web rule ("").
    ctx.events.Clear();
    qw.InteractNpc(kTalker);
    CHECK(K6Has(K6LogKeys(std::span<const Event>(ctx.events.Items())), kTalkerLine));
    REQUIRE(dlg.View().open);
    CHECK(dlg.View().linear);
    CHECK(dlg.View().lineKey.empty());
    dlg.Close();
    // The bell: both lines hold.
    ctx.session.SetFlag(kFlagBell);
    ctx.events.Clear();
    qw.InteractNpc(kTalker);
    const std::vector<std::string> logs = K6LogKeys(std::span<const Event>(ctx.events.Items()));
    CHECK(K6Has(logs, kLineBell));
    CHECK_FALSE(K6Has(logs, kTalkerLine));
    REQUIRE(dlg.View().open);
    CHECK(dlg.View().lineKey == kLineSecond);
    dlg.Close();
    // The giver (its quests turned in, so no card): its gated line first once the flag quest is turned in.
    ctx.sys.quests->Load({K6Record(d, kQuestFlag, QuestStatus::TurnedIn), K6Record(d, kQuestReward, QuestStatus::TurnedIn)});
    ctx.events.Clear();
    qw.InteractNpc(kGiver);
    CHECK(K6Has(K6LogKeys(std::span<const Event>(ctx.events.Items())), kLineAfter));
    REQUIRE(dlg.View().open);
    CHECK(dlg.View().lineKey == kLineFirst);
    dlg.Close();
    ctx.sys.quests->Load({K6Record(d, kQuestFlag, QuestStatus::Failed), K6Record(d, kQuestReward, QuestStatus::TurnedIn)});
    ctx.events.Clear();
    qw.InteractNpc(kGiver);  // a failed (not reacceptable) quest offers nothing: the linear panel
    CHECK(K6Has(K6LogKeys(std::span<const Event>(ctx.events.Items())), kLineFirst));
    REQUIRE(dlg.View().open);
    CHECK(dlg.View().lineKey == kLineSecond);
    dlg.Close();
    // A two-line NPC whose second line is always on (chapter1.md 3: the chaplain): gating the line the "after" line
    // replaces (notTurnedIn) keeps the second line on the panel before and after; the after line is the logged one.
    {
      K6Edits e = K6BaseEdits();
      e.emplace_back("npcs.json", [](JsonValue& doc) {
        K6At(doc, {"npcs", kGiver})->Set("lines", K6Parse(StrCat(
            R"([{"key": ")", kLineAfter, R"(", "when": {"quest": ")", kQuestFlag, R"(", "state": "turnedIn"}},
            {"key": ")", kLineFirst, R"(", "when": {"quest": ")", kQuestFlag, R"(", "state": "notTurnedIn"}},
            {"key": ")", kLineSecond, R"("}])")));
      });
      DataLoadReport report;
      const std::unique_ptr<DataStore> store = K6LoadEdited(e, report);
      INFO(report.Summary());
      REQUIRE(report.Ok());
      std::unique_ptr<GameSim> two = K6Start(*store, 51);
      SimContext& tctx = two->Context();
      DialogueSystem& tdlg = *tctx.sys.dialogue;
      for (const QuestStatus flagQuest : {QuestStatus::Failed, QuestStatus::TurnedIn}) {
        INFO(EnumName(flagQuest));
        tctx.sys.quests->Load({K6Record(*store, kQuestFlag, flagQuest), K6Record(*store, kQuestReward, QuestStatus::TurnedIn)});
        tctx.events.Clear();
        tctx.sys.questWorld->InteractNpc(kGiver);
        const std::vector<std::string> said = K6LogKeys(std::span<const Event>(tctx.events.Items()));
        const bool after = flagQuest == QuestStatus::TurnedIn;
        CHECK(K6Has(said, after ? kLineAfter : kLineFirst));
        CHECK_FALSE(K6Has(said, after ? kLineFirst : kLineAfter));
        REQUIRE(tdlg.View().open);
        CHECK(tdlg.View().lineKey == kLineSecond);
        tdlg.Close();
      }
    }
    // An NPC without lines keeps the web's rules (no key).
    for (const NpcPlacement& n : ctx.sys.zone->Npcs()) {
      const NpcDef* def = d.FindNpc(n.npcId);
      if (def == nullptr || !def->lines.empty() || def->type != NpcType::Quest || !def->dialogueTreeId.empty()) continue;
      if (!ctx.sys.quests->Offers(n.npcId, 1, OfferRule::QuestCard).empty()) continue;
      dlg.OpenLinear(n.npcId);
      CHECK(dlg.View().lineKey.empty());
      dlg.Close();
    }
  }

  TEST_CASE("K6 NPC visibility: hidden at the zone's entry, appears and leaves as its condition changes; ids kept") {
    const DataStore& d = K6Data();
    auto sim = GameSim::Create(d, SimConfig{});
    REQUIRE(sim != nullptr);
    REQUIRE(sim->NewGame(ClassId::Warrior, Difficulty::Normal, 53));
    // The zone entry announces the visible NPCs only.
    std::vector<std::string> announced;
    for (const EvEntitySpawned& s : K6Of<EvEntitySpawned>(sim->Events())) {
      if (s.kind == EntityKind::Npc) announced.push_back(s.defId);
    }
    CHECK(K6Has(announced, kGiver));
    CHECK(K6Has(announced, kTalker));
    CHECK_FALSE(K6Has(announced, kGhost));
    // ... and despawns none: the hidden one was never announced (the entry's refresh is silent).
    for (const EvEntityDespawned& g : K6Of<EvEntityDespawned>(sim->Events())) CHECK(g.kind != EntityKind::Npc);
    sim->Context().sys.story->FinishAllBeats();
    sim->Step();
    SimContext& ctx = sim->Context();
    ZoneRuntime& zone = *ctx.sys.zone;
    const NpcPlacement* ghost = K6Placement(zone, kGhost);
    REQUIRE(ghost != nullptr);  // placed (its id allocated) but hidden
    const EntityId ghostId = ghost->id;
    CHECK_FALSE(ghost->visible);
    CHECK(zone.FindNpc(kGhost) == nullptr);
    CHECK(zone.FindNpcEntity(ghostId) == nullptr);
    CHECK(zone.IsNpcHidden(kGhost));
    CHECK_FALSE(zone.IsNpcHidden(kGiver));
    CHECK_FALSE(zone.IsNpcHidden("fx_k6_nobody"));
    CHECK(K6NpcView(sim->View(), kGhost) == nullptr);
    CHECK(K6NpcView(sim->View(), kGiver) != nullptr);
    // Not interactable: no prompt next to it, the interaction does nothing, the card does not open.
    ctx.sys.locomotion->Teleport(ghost->pos + Vec2(0.5, 0), TeleportReason::Debug);
    sim->Step();
    const InteractTarget t = zone.FindInteractTarget();
    CHECK_FALSE((t.kind == InteractKind::Npc && t.id == ghostId));
    ctx.events.Clear();
    ctx.sys.questWorld->InteractNpc(kGhost);
    CHECK(ctx.events.Items().empty());
    CHECK_FALSE(ctx.sys.dialogue->View().open);
    // Dawn: it appears (same entity id) on the next step.
    const auto npcEvents = [](const auto& all) {
      std::decay_t<decltype(all)> out;
      for (const auto& e : all) {
        if (e.kind == EntityKind::Npc) out.push_back(e);
      }
      return out;
    };
    ctx.session.SetFlag(kFlagDawn);
    sim->Step();
    std::vector<EvEntitySpawned> spawned = npcEvents(K6Of<EvEntitySpawned>(sim->Events()));
    REQUIRE(spawned.size() == 1);
    CHECK((spawned[0].id == ghostId && spawned[0].kind == EntityKind::Npc && spawned[0].defId == kGhost));
    CHECK(zone.FindNpc(kGhost) != nullptr);
    CHECK(K6NpcView(sim->View(), kGhost) != nullptr);
    sim->Step();
    CHECK(npcEvents(K6Of<EvEntitySpawned>(sim->Events())).empty());  // once
    const InteractTarget t2 = zone.FindInteractTarget();
    CHECK((t2.kind == InteractKind::Npc && t2.id == ghostId));
    // The bell: it leaves.
    ctx.session.SetFlag(kFlagBell);
    sim->Step();
    std::vector<EvEntityDespawned> gone = npcEvents(K6Of<EvEntityDespawned>(sim->Events()));
    REQUIRE(gone.size() == 1);
    CHECK((gone[0].id == ghostId && gone[0].kind == EntityKind::Npc && gone[0].reason == DespawnReason::Removed));
    CHECK(zone.FindNpc(kGhost) == nullptr);
    CHECK(K6NpcView(sim->View(), kGhost) == nullptr);
    // A load with dawn set (and no bell) places it at the zone's entry.
    SaveData save;
    sim->BuildSave(save, 0);
    save.worldFlags = {kFlagDawn};
    std::string err;
    REQUIRE(sim->LoadGame(save, &err) == SaveError::None);
    CHECK(sim->Context().sys.zone->FindNpc(kGhost) != nullptr);
    CHECK(K6NpcView(sim->View(), kGhost) != nullptr);
  }

  TEST_CASE("K6 difficulty: variant spawns and respawns and a champion pack's members are scaled like any spawn (Nightmare)") {
    const DataStore& d = K6ChampionData(1, false);
    auto sim = GameSim::Create(d, SimConfig{});
    REQUIRE(sim != nullptr);
    REQUIRE(sim->NewGame(ClassId::Warrior, Difficulty::Nightmare, 71));
    sim->Context().sys.story->FinishAllBeats();
    sim->Step();
    SimContext& ctx = sim->Context();
    MonsterSystem& ms = *ctx.sys.monsters;
    const auto scaled = [&d](std::string_view id) {
      const MonsterDef* base = d.Monsters().Find(id);
      REQUIRE(base != nullptr);
      return ScaleMonsterForDifficulty(*base, d.Combat().difficulty, Difficulty::Nightmare);
    };
    const MonsterDef gnaw = scaled(kGnaw), dead = scaled(kDead);
    REQUIRE(gnaw.hp != d.Monsters().Find(kGnaw)->hp);  // Nightmare moves the numbers
    REQUIRE(gnaw.damage != d.Monsters().Find(kGnaw)->damage);
    const auto isScaled = [](const MonsterInstance& m, const MonsterDef& want) {
      CHECK(m.maxHp == doctest::Approx(want.hp));
      CHECK(m.def.hp == doctest::Approx(want.hp));
      CHECK(m.def.damage == doctest::Approx(want.damage));
    };
    // A variant respawn.
    const size_t entry = K6PlainSpawn(test::RealData(), 3);
    std::vector<const MonsterInstance*> members = K6EntryMonsters(ms, entry);
    REQUIRE(members.size() >= 1);
    ctx.session.SetFlag(kFlagDawn);
    K6Kill(ctx, members[0]->id);
    const MonsterInstance* back = nullptr;
    const int steps = static_cast<int>(std::ceil(3 * d.Monsters().ai.respawnDelayMs / kSimStepMs));
    for (int i = 0; i < steps && back == nullptr; ++i) {
      sim->Step();
      for (const MonsterInstance* m : K6EntryMonsters(ms, entry)) {
        if (m->spawnMember == 0) back = m;
      }
    }
    REQUIRE(back != nullptr);
    CHECK(back->def.id == kGnaw);
    isScaled(*back, gnaw);
    // The zone's spawn of the variant (a load with dawn set).
    const std::string json = sim->SaveGame(9);
    std::string err;
    REQUIRE(sim->LoadGame(json, &err) == SaveError::None);
    REQUIRE(ctx.session.difficulty == Difficulty::Nightmare);
    std::vector<const MonsterInstance*> fresh = K6EntryMonsters(*ctx.sys.monsters, entry);
    REQUIRE(fresh.size() == 2);
    for (const MonsterInstance* m : fresh) isScaled(*m, gnaw);
    // A champion pack's members.
    const Vec2 at = d.FindMap(test::ch1::kZone)->spawns[entry].pos.Center();
    ctx.events.Clear();
    ctx.sys.randomEvents->TriggerEvent(RandomEventType::Ambush, at);
    const std::vector<EntityId> pack = K6SpawnedMonsters(ctx.events);
    REQUIRE(pack.size() == 3);
    isScaled(*ctx.sys.monsters->Find(pack[0]), dead);
    isScaled(*ctx.sys.monsters->Find(pack[1]), gnaw);
  }

  TEST_CASE("K6 NPC visibility: a pointer press passes over a hidden NPC; one that appears is that step's prompt") {
    const DataStore& d = K6Data();
    // A press nearer the hidden ghost than a visible NPC within the pick radius picks the visible one. The ghost moves to
    // the first camp's sixth slot, before the giver (its own camp's slots stand too far apart for one press between two,
    // and a seventh camp NPC would share the first slot's tile).
    {
      K6Edits e = K6BaseEdits();
      e.emplace_back("maps.json", [](JsonValue& doc) {
        JsonValue* camps = K6At(doc, {"maps", test::ch1::kZone, "camps"});
        JsonValue second = JsonValue::Array(), first = JsonValue::Array();
        for (const JsonValue& n : camps->MutableItems()[1].Get("npcs").Items()) {
          if (n.AsString() != kGhost) second.Append(JsonValue::String(n.AsString()));
        }
        for (const JsonValue& n : camps->MutableItems()[0].Get("npcs").Items()) {
          if (n.AsString() == kGiver) first.Append(JsonValue::String(kGhost));
          first.Append(JsonValue::String(n.AsString()));
        }
        camps->MutableItems()[1].Set("npcs", std::move(second));
        camps->MutableItems()[0].Set("npcs", std::move(first));
      });
      DataLoadReport report;
      const std::unique_ptr<DataStore> store = K6LoadEdited(e, report);
      INFO(report.Summary());
      REQUIRE(report.Ok());
      std::unique_ptr<GameSim> sim = K6Start(*store, 73);
      SimContext& ctx = sim->Context();
      ZoneRuntime& zone = *ctx.sys.zone;
      const NpcPlacement* ghost = K6Placement(zone, kGhost);
      REQUIRE(ghost != nullptr);
      REQUIRE_FALSE(ghost->visible);
      const NpcPlacement* near = nullptr;
      for (const NpcPlacement& n : zone.Npcs()) {
        if (!n.visible) continue;
        if (near == nullptr || DistSq(n.pos, ghost->pos) < DistSq(near->pos, ghost->pos)) near = &n;
      }
      REQUIRE(near != nullptr);
      const double gap = Dist(near->pos, ghost->pos);
      INFO("ghost-to-nearest gap " << gap);
      REQUIRE((gap > 0.5 && gap < 3.2));  // apart, and closer than twice the pick radius (1.8)
      const Vec2 press = ghost->pos + (near->pos - ghost->pos) * 0.45;
      REQUIRE(DistSq(press, near->pos) < d.World().constants.npcPickRadiusSq);
      REQUIRE(DistSq(press, ghost->pos) < DistSq(press, near->pos));
      for (const NpcPlacement& n : zone.Npcs()) {
        if (n.visible && n.id != near->id) REQUIRE(DistSq(press, n.pos) > DistSq(press, near->pos));
      }
      // The hero stands away from the camp NPCs: the press walks to the visible one (walk-then-act).
      const Vec2 away = near->pos + Vec2(0, 8);
      ctx.sys.locomotion->Teleport(away, TeleportReason::Debug);
      REQUIRE(Dist(ctx.sys.hero->Position(), near->pos) > d.World().constants.npcRange + 0.5);
      zone.OnPointerPress(press, PointerButton::Primary);
      CHECK(zone.Pending().active);
      CHECK((zone.Pending().target.kind == InteractKind::Npc && zone.Pending().target.id == near->id));
      CHECK_FALSE(ctx.sys.locomotion->IsHoldMoving());  // not a ground press
    }
    // The step dawn is set on, the ghost appears and is the hero's prompt (the visibility refresh runs first).
    {
      std::unique_ptr<GameSim> sim = K6Start(d, 75);
      SimContext& ctx = sim->Context();
      ZoneRuntime& zone = *ctx.sys.zone;
      const NpcPlacement* ghost = K6Placement(zone, kGhost);
      REQUIRE(ghost != nullptr);
      const EntityId ghostId = ghost->id;
      ctx.sys.locomotion->Teleport(ghost->pos + Vec2(0.5, 0), TeleportReason::Debug);
      sim->Step();
      CHECK_FALSE((zone.Prompt().kind == InteractKind::Npc && zone.Prompt().id == ghostId));
      ctx.session.SetFlag(kFlagDawn);
      sim->Step();
      CHECK((zone.Prompt().kind == InteractKind::Npc && zone.Prompt().id == ghostId));
      bool prompted = false;
      for (const EvInteractPrompt& p : K6Of<EvInteractPrompt>(sim->Events())) {
        prompted = prompted || (p.kind == InteractKind::Npc && p.id == ghostId);
      }
      CHECK(prompted);
    }
  }

  // ===================================================================================================================
  // the champion ambush
  // ===================================================================================================================

  TEST_CASE("K6 champion ambush: no block draws exactly as before; a block draws once more after the count") {
    // A Vec2 where an ambush can stand (the plain entry's anchor).
    const Vec2 at = test::RealData().FindMap(test::ch1::kZone)->spawns[K6PlainSpawn(test::RealData(), 3)].pos.Center();
    const auto tiles = [](const MonsterSystem& ms, const std::vector<EntityId>& ids) {
      std::vector<Vec2> out;
      for (EntityId id : ids) out.push_back(ms.Find(id)->pos);
      return out;
    };
    // The event's draws by hand: the count (Float01), [the champion draw], then SpawnAmbush's draws.
    const auto byHand = [&](const DataStore& d, bool extraDraw, std::vector<Vec2>& placed) {
      std::unique_ptr<GameSim> sim = K6Start(d, 57);
      SimContext& ctx = sim->Context();
      Rng& rng = ctx.Rand(RngStream::Events);
      const ZoneEventDataDef* zd = d.World().randomEvents.ForZone(test::ch1::kZone);
      const int32_t count = static_cast<int32_t>(std::floor(rng.Float01() * (zd->ambushCountMax - zd->ambushCountMin + 1))) +
                            zd->ambushCountMin;
      if (extraDraw) rng.Float01();
      placed = tiles(*ctx.sys.monsters,
                     ctx.sys.monsters->SpawnAmbush(zd->ambushMonsters, count, at, 3, 2, MonsterRole::AmbushSpawn));
      return std::make_pair(rng.GetState(), ctx.Rand(RngStream::Ai).GetState());
    };
    const auto byEvent = [&](const DataStore& d, std::vector<Vec2>& placed, bool& champion) {
      std::unique_ptr<GameSim> sim = K6Start(d, 57);
      SimContext& ctx = sim->Context();
      ctx.events.Clear();
      ctx.sys.randomEvents->TriggerEvent(RandomEventType::Ambush, at);  // an ambush resolves (and is pruned) at once
      const std::vector<EvRandomEvent> re = K6Of<EvRandomEvent>(ctx.events);
      REQUIRE_FALSE(re.empty());
      champion = re.front().champion;
      placed = tiles(*ctx.sys.monsters, K6SpawnedMonsters(ctx.events));
      return std::make_pair(ctx.Rand(RngStream::Events).GetState(), ctx.Rand(RngStream::Ai).GetState());
    };
    // Without the block (K6Data has none): exactly the draws of today's ambush.
    {
      std::vector<Vec2> a, b;
      bool champion = true;
      const auto hand = byHand(K6Data(), false, a);
      const auto event = byEvent(K6Data(), b, champion);
      CHECK_FALSE(champion);
      CHECK(hand == event);
      CHECK(a == b);
      CHECK_FALSE(a.empty());
    }
    // With a block whose chance misses: one more Events draw right after the count, then today's ambush; no Ai draw.
    {
      std::vector<Vec2> a, b;
      bool champion = true;
      const DataStore& d = K6ChampionData(1e-9);
      const auto hand = byHand(d, true, a);
      const auto event = byEvent(d, b, champion);
      CHECK_FALSE(champion);
      CHECK(hand == event);
      CHECK(a == b);
    }
  }

  TEST_CASE("K6 champion ambush: the pack in mix order, one pack id and its leader, chasing, no respawn, the shared affix") {
    const DataStore& d = K6ChampionData();
    std::unique_ptr<GameSim> sim = K6Start(d, 59);
    SimContext& ctx = sim->Context();
    const Vec2 at = d.FindMap(test::ch1::kZone)->spawns[K6PlainSpawn(test::RealData(), 3)].pos.Center();
    // The shared roll is one RollEliteAffixes on the Ai stream (the leader's family): replay it on a copy.
    Rng aiCopy(1);
    aiCopy.SetState(ctx.Rand(RngStream::Ai).GetState());
    const std::vector<EliteAffixType> expected =
        RollEliteAffixes(d.Combat().eliteAffixes, test::ch1::kZone, aiCopy, "fx_k6_undead");
    REQUIRE(expected.size() == 2);  // the zone rolls both of {swift, frozen}
    ctx.events.Clear();
    ctx.sys.randomEvents->TriggerEvent(RandomEventType::Ambush, at);
    CHECK(ctx.Rand(RngStream::Ai).GetState() == aiCopy.GetState());  // exactly that roll
    const std::vector<EntityId> pack = K6SpawnedMonsters(ctx.events);
    REQUIRE(pack.size() == 3);
    std::vector<const MonsterInstance*> m;
    for (EntityId id : pack) m.push_back(ctx.sys.monsters->Find(id));
    CHECK(m[0]->def.id == kDead);
    CHECK(m[1]->def.id == kGnaw);
    CHECK(m[2]->def.id == kGnaw);
    CHECK(m[0]->packId != 0);
    for (const MonsterInstance* x : m) {
      CHECK(x->packId == m[0]->packId);
      CHECK(x->role == MonsterRole::AmbushSpawn);
      CHECK(x->noRespawn);
      CHECK(x->state == MonsterState::Chase);
      CHECK(x->spawnEntry == -1);
      CHECK(x->def.elite);  // each keeps at least one affix
    }
    CHECK(m[0]->packLeader);
    CHECK_FALSE(m[1]->packLeader);
    CHECK_FALSE(m[2]->packLeader);
    // The leader (undead) keeps both rolled affixes; the gnawers (beasts) only frozen (swift is the undead's alone).
    std::vector<EliteAffixType> leader, gnaw;
    for (const MonsterAffix& a : m[0]->affixes) leader.push_back(a.type);
    for (const MonsterAffix& a : m[1]->affixes) gnaw.push_back(a.type);
    CHECK(leader == expected);
    CHECK(gnaw == std::vector<EliteAffixType>{EliteAffixType::Frozen});
    // The affixes changed the stats as a roll would (swift's speed on the leader).
    const MonsterDef* base = d.Monsters().Find(kDead);
    CHECK(m[0]->def.speed > ScaleMonsterForDifficulty(*base, d.Combat().difficulty, Difficulty::Normal).speed);
    // The event announces the champion.
    std::vector<EvRandomEvent> re = K6Of<EvRandomEvent>(ctx.events);
    REQUIRE(re.size() == 2);  // triggered, then resolved
    CHECK((re[0].champion && !re[0].resolved && re[1].champion && re[1].resolved));
    // The snapshot shows the pack and its leader.
    sim->Step();
    size_t leaders = 0;
    for (const MonsterView& v : sim->View().monsters) {
      if (v.packId == m[0]->packId && v.packLeader) ++leaders;
    }
    CHECK(leaders == 1);
    // Killed members never come back.
    const EntityId gnawer = pack[1];
    K6Kill(ctx, gnawer);
    const int steps = static_cast<int>(std::ceil(d.Monsters().ai.respawnDelayMs / kSimStepMs)) + 10;
    for (int i = 0; i < steps; ++i) sim->Step();
    const MonsterInstance* dead = ctx.sys.monsters->Find(gnawer);
    CHECK((dead == nullptr || !dead->IsAlive()));
    // A leader id with several members: only the first of them leads; a pack without a leader has none.
    ZoneEventDataDef::AmbushChampionDef two;
    two.present = true;
    two.chance = 1;
    two.mix = {MapSpawnMixDef{std::string(kDead), 1}, MapSpawnMixDef{std::string(kGnaw), 2}};
    two.leader = kGnaw;
    const std::vector<EntityId> led = ctx.sys.monsters->SpawnChampionPack(two, at, 2, 1, MonsterRole::AmbushSpawn);
    REQUIRE(led.size() == 3);
    CHECK_FALSE(ctx.sys.monsters->Find(led[0])->packLeader);
    CHECK(ctx.sys.monsters->Find(led[1])->packLeader);
    CHECK_FALSE(ctx.sys.monsters->Find(led[2])->packLeader);
    CHECK(ctx.sys.monsters->Find(led[1])->packId != m[0]->packId);  // a new pack
    two.leader.clear();
    for (EntityId id : ctx.sys.monsters->SpawnChampionPack(two, at, 2, 1, MonsterRole::AmbushSpawn)) {
      CHECK_FALSE(ctx.sys.monsters->Find(id)->packLeader);
    }
  }

  TEST_CASE("K6 champion ambush: without sharedAffix the members roll nothing (no Ai draw) and stay plain") {
    const DataStore& d = K6ChampionData(1, false);
    std::unique_ptr<GameSim> sim = K6Start(d, 61);
    SimContext& ctx = sim->Context();
    const Vec2 at = d.FindMap(test::ch1::kZone)->spawns[K6PlainSpawn(test::RealData(), 3)].pos.Center();
    const RngState ai = ctx.Rand(RngStream::Ai).GetState();
    ctx.events.Clear();
    ctx.sys.randomEvents->TriggerEvent(RandomEventType::Ambush, at);
    CHECK(ctx.Rand(RngStream::Ai).GetState() == ai);
    REQUIRE_FALSE(K6Of<EvRandomEvent>(ctx.events).empty());
    CHECK(K6Of<EvRandomEvent>(ctx.events).front().champion);
    const std::vector<EntityId> pack = K6SpawnedMonsters(ctx.events);
    REQUIRE(pack.size() == 3);
    for (EntityId id : pack) {
      const MonsterInstance* x = ctx.sys.monsters->Find(id);
      CHECK(x->affixes.empty());
      CHECK_FALSE(x->def.elite);
    }
    CHECK(ctx.sys.monsters->Find(pack[0])->packLeader);
    // A pack without a leader: nobody leads.
    DataLoadReport report;
    K6Edits e = K6BaseEdits();
    e.emplace_back("random_events.json",
                   K6EditChampion(StrCat(R"({"chance": 1, "mix": [{"monsterId": ")", kGnaw, R"(", "count": 2}]})")));
    auto store = K6LoadEdited(e, report);
    INFO(report.Summary());
    REQUIRE(report.Ok());
    std::unique_ptr<GameSim> sim2 = K6Start(*store, 61);
    sim2->Context().events.Clear();
    sim2->Context().sys.randomEvents->TriggerEvent(RandomEventType::Ambush, at);
    const std::vector<EntityId> pack2 = K6SpawnedMonsters(sim2->Context().events);
    REQUIRE(pack2.size() == 2);
    for (EntityId id : pack2) {
      const MonsterInstance* x = sim2->Context().sys.monsters->Find(id);
      CHECK_FALSE(x->packLeader);
      CHECK(x->packId == sim2->Context().sys.monsters->Find(pack2[0])->packId);
    }
  }

  TEST_CASE("K6 champion ambush: leaderLoot drops one guaranteed item of its quality from the leader's kill, last") {
    // Two sims on one seed whose data differ only in leaderLoot: the same pack, the same members' loot; the leader's
    // kill drops exactly one item more, of the quality, after its regular loot.
    const auto load = [](bool loot) {
      DataLoadReport report;
      K6Edits e = K6BaseEdits();
      e.emplace_back("random_events.json",
                     K6EditChampion(StrCat(R"({"chance": 1, "mix": [{"monsterId": ")", kDead, R"(", "count": 1}, )",
                                           R"({"monsterId": ")", kGnaw, R"(", "count": 2}], "leader": ")", kDead, "\"",
                                           loot ? R"(, "leaderLoot": "magic")" : "", "}")));
      auto store = K6LoadEdited(e, report);
      INFO(report.Summary());
      REQUIRE(report.Ok());
      return store;
    };
    const std::unique_ptr<DataStore> withLoot = load(true), without = load(false);
    REQUIRE(withLoot->World().randomEvents.ForZone(test::ch1::kZone)->ambushChampion.leaderLoot == ItemQuality::Magic);
    CHECK(without->World().randomEvents.ForZone(test::ch1::kZone)->ambushChampion.leaderLoot == ItemQuality::Normal);
    std::unique_ptr<GameSim> a = K6Start(*withLoot, 67), b = K6Start(*without, 67);
    const Vec2 at = withLoot->FindMap(test::ch1::kZone)->spawns[K6PlainSpawn(test::RealData(), 3)].pos.Center();
    std::vector<EntityId> packs[2];
    GameSim* sims[2] = {a.get(), b.get()};
    for (int k = 0; k < 2; ++k) {
      SimContext& ctx = sims[k]->Context();
      ctx.events.Clear();
      ctx.sys.randomEvents->TriggerEvent(RandomEventType::Ambush, at);
      packs[k] = K6SpawnedMonsters(ctx.events);
      REQUIRE(packs[k].size() == 3);
    }
    const MonsterInstance* leaderA = a->Context().sys.monsters->Find(packs[0][0]);
    REQUIRE(leaderA->packLeader);
    CHECK(leaderA->guaranteedLoot == ItemQuality::Magic);
    CHECK(a->Context().sys.monsters->Find(packs[0][1])->guaranteedLoot == ItemQuality::Normal);
    CHECK(b->Context().sys.monsters->Find(packs[1][0])->guaranteedLoot == ItemQuality::Normal);
    const auto dropsOf = [](GameSim& sim, EntityId id) {
      SimContext& ctx = sim.Context();
      ctx.events.Clear();
      K6Kill(ctx, id);
      return K6Of<EvLootDropped>(ctx.events);
    };
    // A member's kill: the same drops in both.
    const std::vector<EvLootDropped> memberA = dropsOf(*a, packs[0][1]), memberB = dropsOf(*b, packs[1][1]);
    REQUIRE(memberA.size() == memberB.size());
    for (size_t i = 0; i < memberA.size(); ++i) CHECK(memberA[i].baseId == memberB[i].baseId);
    // The leader's kill: B's drops, then one equipment item of the quality.
    const int32_t level = a->Context().sys.monsters->Find(packs[0][0])->def.level;
    const std::vector<EvLootDropped> leadA = dropsOf(*a, packs[0][0]), leadB = dropsOf(*b, packs[1][0]);
    REQUIRE(leadA.size() == leadB.size() + 1);
    for (size_t i = 0; i < leadB.size(); ++i) {
      CHECK(leadA[i].baseId == leadB[i].baseId);
      CHECK(leadA[i].quality == leadB[i].quality);
    }
    const EvLootDropped& extra = leadA.back();
    CHECK(extra.quality == ItemQuality::Magic);
    const ItemBaseDef* base = withLoot->Items().FindBase(extra.baseId);
    REQUIRE(base != nullptr);
    bool inPool = false;
    for (const ItemBaseDef* p : EquipmentDropPool(*withLoot, level)) inPool = inPool || p->id == extra.baseId;
    CHECK(inPool);  // GenerateEquipment at the leader's level
    // The Loot stream moved further in A only by that item; the other streams agree.
    CHECK(a->Context().Rand(RngStream::Ai).GetState() == b->Context().Rand(RngStream::Ai).GetState());
    CHECK_FALSE(a->Context().Rand(RngStream::Loot).GetState() == b->Context().Rand(RngStream::Loot).GetState());
  }

  // ===================================================================================================================
  // replay
  // ===================================================================================================================

  TEST_CASE("K6 replay: flags, a permanent reward, NPC visibility, a variant respawn and a champion ambush replay") {
    const auto run = [] {
      const DataStore& d = K6ChampionData();
      std::unique_ptr<GameSim> sim = K6Start(d, 63);
      SimContext& ctx = sim->Context();
      K6Fnv f;
      const size_t entry = K6PlainSpawn(test::RealData(), 3);
      // Dawn first (the ghost appears), then the flag quest's turn-in sets the bell (the ghost leaves).
      ctx.session.SetFlag(kFlagDawn);
      sim->Step();
      K6Digest(f, sim->Events());
      int ghostIn = 0, ghostOut = 0;
      for (const EvEntitySpawned& e : K6Of<EvEntitySpawned>(sim->Events())) ghostIn += e.defId == kGhost ? 1 : 0;
      ctx.sys.quests->Load({K6Record(d, kQuestFlag, QuestStatus::Completed), K6Record(d, kQuestReward, QuestStatus::Completed)});
      ctx.events.Clear();
      REQUIRE(ctx.sys.questWorld->TurnIn(kQuestReward, 0));
      REQUIRE(ctx.sys.questWorld->TurnIn(kQuestFlag, 0));
      K6Digest(f, std::span<const Event>(ctx.events.Items()));
      std::vector<const MonsterInstance*> m = K6EntryMonsters(*ctx.sys.monsters, entry);
      REQUIRE_FALSE(m.empty());
      K6Kill(ctx, m[0]->id);
      ctx.sys.randomEvents->TriggerEvent(RandomEventType::Ambush, m[0]->pos);
      K6Digest(f, std::span<const Event>(ctx.events.Items()));
      bool champion = false;
      for (const EvRandomEvent& r : K6Of<EvRandomEvent>(ctx.events)) champion = champion || r.champion;
      CHECK(champion);
      const EntityId ghostId = K6Placement(*ctx.sys.zone, kGhost)->id;
      for (int i = 0; i < 60 * 20; ++i) {
        sim->Step();
        K6Digest(f, sim->Events());
        for (const EvEntityDespawned& e : K6Of<EvEntityDespawned>(sim->Events())) ghostOut += e.id == ghostId ? 1 : 0;
      }
      CHECK(ghostIn == 1);
      CHECK(ghostOut == 1);
      bool gnawed = false;
      for (const MonsterInstance* x : K6EntryMonsters(*ctx.sys.monsters, entry)) gnawed = gnawed || x->def.id == kGnaw;
      CHECK(gnawed);
      f.U64(ctx.session.worldFlags.size());
      for (const MonsterInstance& x : ctx.sys.monsters->All()) {
        f.S(x.def.id);
        f.V(x.pos);
        f.D(x.hp);
      }
      return f.h;
    };
    const uint64_t a = run();
    const uint64_t b = run();
    CHECK(a == b);
    char hex[17];
    std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(a));
    MESSAGE("K6 replay digest " << hex);
    CHECK(std::string(hex) == "2a75b9e104969bc9");  // pinned (GCC = Clang; 2F review: Valdras / Corpseherd numbers, affix rules)
  }
  // ===================================================================================================================
  // 2F review: ambush variants, the champion's notFlag, boss affix pins
  // ===================================================================================================================

  TEST_CASE("2F review: ambush variants and the champion's notFlag load; the loader and Finalize refuse broken ones") {
    const auto withBlocks = [](std::string champion, std::string variants) {
      return [champion, variants](JsonValue& doc) {
        JsonValue* z = K6At(doc, {"zones", test::ch1::kZone});
        if (!champion.empty()) z->Set("ambushChampion", K6Parse(champion));
        if (!variants.empty()) z->Set("ambushVariants", K6Parse(variants));
      };
    };
    const std::string champ = StrCat(R"({"chance": 1, "mix": [{"monsterId": ")", kDead, R"(", "count": 1}], "notFlag": ")",
                                     kFlagDawn, R"("})");
    const std::string vars = StrCat(R"([{"whenFlag": ")", kFlagBell, R"(", "monsters": [")", kGnaw, R"("]}, {"whenFlag": ")",
                                    kFlagDawn, R"(", "monsters": [")", kDead, R"(", ")", kGnaw, R"("]}])");
    {
      DataLoadReport r = K6BadLoad("random_events.json", withBlocks(champ, vars));
      INFO(r.Summary());
      REQUIRE(r.Ok());
    }
    K6Edits e = K6BaseEdits();
    e.emplace_back("random_events.json", withBlocks(champ, vars));
    DataLoadReport report;
    const auto store = K6LoadEdited(e, report);
    REQUIRE(report.Ok());
    const ZoneEventDataDef* z = store->World().randomEvents.ForZone(test::ch1::kZone);
    REQUIRE(z != nullptr);
    CHECK(z->ambushChampion.notFlag == kFlagDawn);
    REQUIRE(z->ambushVariants.size() == 2);
    CHECK(z->ambushVariants[0].whenFlag == kFlagBell);
    CHECK(z->ambushVariants[0].monsters == std::vector<std::string>{kGnaw});
    CHECK(z->ambushVariants[1].monsters == std::vector<std::string>{kDead, kGnaw});
    for (const DataIssue& w : report.warnings) CHECK(w.path.find("ambushVariants") == std::string::npos);  // flags set
    // Loader refusals.
    struct Bad {
      std::string champion, variants, path, text;
    };
    const Bad bad[] = {
        {"", R"([{"whenFlag": "", "monsters": ["x"]}])", "ambushVariants[0].whenFlag", "an empty flag id"},
        {"", StrCat(R"([{"whenFlag": ")", kFlagBell, R"(", "monsters": []}])"), "ambushVariants[0].monsters",
         "names its monsters"},
        {"", StrCat(R"([{"whenFlag": ")", kFlagBell, R"(", "monsters": [""]}])"), "ambushVariants[0].monsters",
         "an empty monster id"},
        {"", StrCat(R"([{"whenFlag": ")", kFlagBell, R"(", "monsters": ["a"], "count": 2}])"), "ambushVariants[0]",
         "unknown ambush variant field"},
        {StrCat(R"({"chance": 1, "mix": [{"monsterId": ")", kDead, R"(", "count": 1}], "notFlag": ""})"), "",
         "ambushChampion.notFlag", "an empty flag id"},
    };
    for (const Bad& b : bad) {
      INFO(b.path << ": " << b.text);
      const DataLoadReport r = K6BadLoad("random_events.json", withBlocks(b.champion, b.variants));
      CHECK_FALSE(r.Ok());
      CHECK(K6HasIssue(r.errors, "random_events.json", b.path, b.text));
    }
    // Finalize: known monsters of the zone, never the story boss; a flag no quest sets warns.
    const std::string boss = test::RealData().Monsters().ai.storyBossId;
    {
      const DataLoadReport r = K6BadLoad("random_events.json", withBlocks("", StrCat(R"([{"whenFlag": ")", kFlagBell,
          R"(", "monsters": ["fx_k6_nobody", ")", boss, R"("]}])")));
      CHECK(K6HasIssue(r.errors, "random_events.json", "ambushVariants[0].monsters", "unknown monster 'fx_k6_nobody'"));
      CHECK(K6HasIssue(r.errors, "random_events.json", "ambushVariants[0].monsters", "the story boss is never"));
    }
    {
      const DataLoadReport r = K6BadLoad(
          "random_events.json",
          withBlocks(StrCat(R"({"chance": 1, "mix": [{"monsterId": ")", kDead, R"(", "count": 1}], "notFlag": "fx_unset"})"),
                     StrCat(R"([{"whenFlag": "fx_unset2", "monsters": [")", kGnaw, R"("]}])")));
      CHECK(r.Ok());
      CHECK(K6HasIssue(r.warnings, "random_events.json", "ambushChampion.notFlag", "no quest sets flag 'fx_unset'"));
      CHECK(K6HasIssue(r.warnings, "random_events.json", "ambushVariants[0].whenFlag", "no quest sets flag 'fx_unset2'"));
    }
  }

  TEST_CASE("2F review: once its notFlag is set the champion draws nothing; the first set variant replaces the ambush") {
    K6Edits e = K6BaseEdits();
    e.emplace_back("random_events.json", [](JsonValue& doc) {
      JsonValue* z = K6At(doc, {"zones", test::ch1::kZone});
      z->Set("ambushChampion", K6Parse(StrCat(R"({"chance": 1, "mix": [{"monsterId": ")", kDead,
                                              R"(", "count": 1}], "notFlag": ")", kFlagDawn, R"("})")));
      z->Set("ambushVariants", K6Parse(StrCat(R"([{"whenFlag": ")", kFlagBell, R"(", "monsters": [")", kGnaw, R"("]}])")));
    });
    DataLoadReport report;
    const auto store = K6LoadEdited(e, report);
    REQUIRE(report.Ok());
    const DataStore& d = *store;
    const Vec2 at = d.FindMap(test::ch1::kZone)->spawns[K6PlainSpawn(test::RealData(), 3)].pos.Center();
    struct Out {
      bool champion = false;
      RngState events{}, ai{};
      std::vector<std::string> ids;
    };
    const auto run = [&](const DataStore& data, std::vector<std::string> flags) {
      std::unique_ptr<GameSim> sim = K6Start(data, 57);
      SimContext& ctx = sim->Context();
      for (const std::string& f : flags) ctx.session.SetFlag(f);
      ctx.events.Clear();
      ctx.sys.randomEvents->TriggerEvent(RandomEventType::Ambush, at);
      Out o;
      for (const EvRandomEvent& r : K6Of<EvRandomEvent>(ctx.events)) o.champion = o.champion || r.champion;
      for (const EntityId id : K6SpawnedMonsters(ctx.events)) o.ids.push_back(ctx.sys.monsters->Find(id)->def.id);
      o.events = ctx.Rand(RngStream::Events).GetState();
      o.ai = ctx.Rand(RngStream::Ai).GetState();
      return o;
    };
    // No flag: the champion (chance 1).
    CHECK(run(d, {}).champion);
    // Dawn set: no champion draw at all - the same draws and the same ambush as a zone without the block.
    const Out dawn = run(d, {kFlagDawn});
    const Out plain = run(K6Data(), {kFlagDawn});
    CHECK_FALSE(dawn.champion);
    CHECK(dawn.events == plain.events);
    CHECK(dawn.ai == plain.ai);
    CHECK(dawn.ids == plain.ids);
    CHECK_FALSE(dawn.ids.empty());
    // The bell set too: the ambush is the variant's (gnawers only), the same draws.
    const Out bell = run(d, {kFlagDawn, kFlagBell});
    CHECK_FALSE(bell.champion);
    CHECK(bell.ids.size() == plain.ids.size());
    for (const std::string& id : bell.ids) CHECK(id == kGnaw);
    CHECK(bell.events == plain.events);
  }

  TEST_CASE("2F review: a named boss's affix roll is pinned from its first fight until it dies; saves carry the pins") {
    const DataStore& real = test::RealData();
    // The plains' two named bosses, read from the data (never spelled here).
    const std::string boss = real.Monsters().ai.storyBossId;
    const MiniBossEntry* mb = real.Monsters().MiniBossFor(test::ch1::kZone);
    REQUIRE(mb != nullptr);
    const std::string herd = mb->monsterId;
    // Serialisation: written last, only when present; parsed back the same; normalised.
    {
      SaveData s;
      s.hasStorySeen = true;
      CHECK(SerializeSave(s).find("bossAffixes") == std::string::npos);
      s.bossAffixes = {BossAffixPin{"fx_zone", "fx_pin_b", {EliteAffixType::ExtraStrong, EliteAffixType::Swift}},
                       BossAffixPin{"fx_zone", "fx_pin_a", {}}};
      NormalizeBossAffixPins(s.bossAffixes);
      CHECK(s.bossAffixes[0].monsterId == "fx_pin_a");  // sorted by (zone, monster id)
      const std::string json = SerializeSave(s);
      CHECK(json.find(R"("bossAffixes":[{"zone":"fx_zone","monsterId":"fx_pin_a","affixes":[]},)"
                      R"({"zone":"fx_zone","monsterId":"fx_pin_b","affixes":["extra_strong","swift"]}])") !=
            std::string::npos);
      CHECK(json.find("\"permanentRewards\"") == std::string::npos);
      SaveData back;
      REQUIRE(ParseSave(json, back) == SaveError::None);
      CHECK(back.bossAffixes == s.bossAffixes);
      CHECK(SerializeSave(back) == json);
      JsonValue raw;
      REQUIRE(ParseJson(json, raw));
      raw.Set("bossAffixes", K6Parse(R"([{"zone": "z", "monsterId": "b", "affixes": ["swift", "holy", 3]},
        {"zone": "z", "monsterId": "b", "affixes": ["frozen"]}, {"zone": "", "monsterId": "c"}, {"monsterId": "d"},
        {"zone": "a", "monsterId": "e"}, 7])"));
      SaveData norm;
      REQUIRE(ParseSave(WriteJson(raw), norm) == SaveError::None);
      REQUIRE(norm.bossAffixes.size() == 2);
      CHECK(norm.bossAffixes[0] == BossAffixPin{"a", "e", {}});
      CHECK(norm.bossAffixes[1] == BossAffixPin{"z", "b", {EliteAffixType::Swift}});  // the first pin of a pair
      raw.Set("bossAffixes", K6Parse(R"({"a": 1})"));
      SaveData wrong;
      REQUIRE(ParseSave(WriteJson(raw), wrong) == SaveError::None);
      CHECK(wrong.bossAffixes.empty());
      // The content-version upgrade drops the pins of monsters the data no longer has.
      SaveData old;
      old.bossAffixes = {BossAffixPin{test::ch1::kZone, boss, {EliteAffixType::Swift}},
                         BossAffixPin{test::ch1::kZone, herd, {}}, BossAffixPin{test::ch1::kZone, "fx_gone", {}}};
      NormalizeBossAffixPins(old.bossAffixes);
      std::vector<BossAffixPin> kept = old.bossAffixes;
      std::erase_if(kept, [](const BossAffixPin& p) { return p.monsterId == "fx_gone"; });
      old.contentVersion = 1;
      ApplyContentVersion(real, old);
      CHECK(old.bossAffixes == kept);
    }
    // Runtime: the mini-boss of a new game rolls at the zone's entry; her first fight pins the roll; a load spawns her
    // with the pinned roll (even an edited one); a kill ends the pin. Before her fight nothing is pinned, and a load
    // leaves the save as it was.
    const auto bossAffixes = [](GameSim& sim, MonsterRole role) {
      std::vector<EliteAffixType> out;
      bool found = false;
      for (const MonsterInstance& m : sim.Context().sys.monsters->All()) {
        if (m.role != role || !m.IsAlive()) continue;
        found = true;
        for (const MonsterAffix& a : m.affixes) out.push_back(a.type);
      }
      REQUIRE(found);
      return out;
    };
    const auto pinOf = [&](const SaveData& s) -> const BossAffixPin* {
      for (const BossAffixPin& p : s.bossAffixes) {
        if (p.monsterId == herd) return &p;
      }
      return nullptr;
    };
    int elites = 0;
    for (uint64_t seed = 1; seed <= 6; ++seed) {
      CAPTURE(seed);
      std::unique_ptr<GameSim> sim = K6Start(real, seed);
      SimContext& ctx = sim->Context();
      const std::vector<EliteAffixType> rolled = bossAffixes(*sim, MonsterRole::ZoneMiniBoss);
      elites += rolled.empty() ? 0 : 1;
      REQUIRE(sim->CanSave());
      std::string err;
      {
        const std::string before = sim->SaveGame(1760000000000);
        SaveData parsed;
        REQUIRE(ParseSave(before, parsed) == SaveError::None);
        CHECK(pinOf(parsed) == nullptr);
        std::unique_ptr<GameSim> l0 = GameSim::Create(real, SimConfig{});
        REQUIRE(l0->LoadGame(before, &err) == SaveError::None);
        CHECK(l0->SaveGame(1760000000000) == before);
      }
      // Her fight: the hero 12 tiles off (in her activity range, past her aggro and her lines), she is set chasing.
      EntityId herdId = kNoEntity;
      for (const MonsterInstance& m : ctx.sys.monsters->All()) herdId = m.role == MonsterRole::ZoneMiniBoss ? m.id : herdId;
      REQUIRE(herdId != kNoEntity);
      ctx.sys.locomotion->Teleport(ctx.sys.monsters->Find(herdId)->pos + Vec2(-12, 0), TeleportReason::Debug);
      sim->Step();
      CHECK_FALSE(ctx.sys.monsters->Find(herdId)->affixesPinned);
      ctx.sys.monsters->ForceChase(herdId);
      for (int i = 0; i < 60 && !ctx.sys.monsters->Find(herdId)->affixesPinned; ++i) sim->Step();
      REQUIRE(ctx.sys.monsters->Find(herdId)->affixesPinned);
      REQUIRE(sim->CanSave());
      const std::string save = sim->SaveGame(1760000000000);
      SaveData parsed;
      REQUIRE(ParseSave(save, parsed) == SaveError::None);
      const BossAffixPin* pin = pinOf(parsed);
      REQUIRE(pin != nullptr);
      CHECK(pin->zone == test::ch1::kZone);
      CHECK(pin->affixes == rolled);
      // A load re-spawns the zone: the pinned roll, not a new one.
      std::unique_ptr<GameSim> loaded = GameSim::Create(real, SimConfig{});
      REQUIRE(loaded->LoadGame(save, &err) == SaveError::None);
      CHECK(bossAffixes(*loaded, MonsterRole::ZoneMiniBoss) == rolled);
      CHECK(loaded->SaveGame(1760000000000) == save);
      // The pin drives it: an edited pin is what the load spawns.
      SaveData edited = parsed;
      for (BossAffixPin& p : edited.bossAffixes) {
        if (p.monsterId == herd) p.affixes = {EliteAffixType::Swift};
      }
      std::unique_ptr<GameSim> e2 = GameSim::Create(real, SimConfig{});
      REQUIRE(e2->LoadGame(edited, &err) == SaveError::None);
      CHECK(bossAffixes(*e2, MonsterRole::ZoneMiniBoss) == std::vector<EliteAffixType>{EliteAffixType::Swift});
      // A kill ends the pin.
      K6Kill(ctx, herdId);
      sim->Step();
      SaveData after;
      REQUIRE(ParseSave(sim->SaveGame(1760000000000), after) == SaveError::None);
      CHECK(pinOf(after) == nullptr);
    }
    CHECK(elites > 0);  // some seeds rolled affixes for her
  }
}
