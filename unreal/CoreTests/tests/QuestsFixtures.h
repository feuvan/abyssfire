// Quests + story test fixtures (moved out of quests_test.cpp so content_ch1_test.cpp shares them).
#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <vector>

#include "SimHarness.h"
#include "TestContent.h"
#include "abyss/audio/Audio.h"
#include "abyss/base/I18n.h"
#include "abyss/base/Math.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/items/Inventory.h"
#include "abyss/items/LootGen.h"
#include "abyss/items/Shop.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/quests/Achievements.h"
#include "abyss/quests/Dialogue.h"
#include "abyss/quests/Lore.h"
#include "abyss/quests/QuestGuide.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/quests/QuestWorld.h"
#include "abyss/save/SaveData.h"
#include "abyss/sim/GameSim.h"
#include "abyss/story/StoryDirector.h"
#include "abyss/world/MapGen.h"
#include "abyss/world/Zone.h"
#include "doctest/doctest.h"

namespace abyss::test::quests_fx {

// ---- pure QuestSystem rig ------------------------------------------------------------------------------------------
struct QsRig {
  test::SimHarness h;
  QuestSystem qs{h.ctx.data, h.events, h.bus};
  std::vector<std::string> log;  // "accepted:<id>", "progress:<id>:<i>:<cur>:<done>", "completed:<id>", ...
  QsRig() {
    h.bus.Subscribe<QuestAcceptedMsg>([this](const QuestAcceptedMsg& m) { log.push_back("accepted:" + m.questId); });
    h.bus.Subscribe<QuestProgressMsg>([this](const QuestProgressMsg& m) {
      log.push_back("progress:" + m.questId + ":" + std::to_string(m.objectiveIndex) + ":" + std::to_string(m.current) +
                    (m.completesQuest ? ":done" : ""));
    });
    h.bus.Subscribe<QuestCompletedMsg>([this](const QuestCompletedMsg& m) { log.push_back("completed:" + m.questId); });
    h.bus.Subscribe<QuestTurnedInMsg>([this](const QuestTurnedInMsg& m) { log.push_back("turnedIn:" + m.questId); });
    h.bus.Subscribe<QuestFailedMsg>([this](const QuestFailedMsg& m) { log.push_back("failed:" + m.questId); });
  }
  int32_t Cur(const std::string& id, size_t i) const {
    const QuestProgress* p = qs.Progress(id);
    return p != nullptr && i < p->objectives.size() ? p->objectives[i] : -1;
  }
  QuestStatus Status(const std::string& id) const {
    bool has = false;
    const QuestStatus s = qs.StatusOf(id, has);
    REQUIRE(has);
    return s;
  }
};

inline QuestProgress Rec(std::string id, QuestStatus st, std::vector<int32_t> cur) {
  return QuestProgress{std::move(id), st, std::move(cur)};
}

inline std::vector<std::string> OfferIds(const std::vector<QuestOffer>& offers) {
  std::vector<std::string> out;
  for (const QuestOffer& o : offers) out.push_back((o.turnIn ? "T:" : "") + o.quest->id);
  return out;
}

// ---- fake guide world (QuestEngine.test.ts `world()`) ---------------------------------------------------------------
inline QuestDef GuideQuest() {
  QuestDef q;
  q.id = "q_test";
  q.zone = "z";
  q.type = QuestType::Kill;
  q.category = QuestCategory::Main;
  q.level = 10;
  QuestObjectiveDef o;
  o.type = ObjectiveType::Kill;
  o.targetId = fixture::Grunt().id;
  o.required = 3;
  q.objectives.push_back(o);
  return q;
}

inline GuideWorld FakeWorld() {
  GuideWorld w;
  w.player = Vec2(0, 0);
  w.npcTile = [](std::string_view id, Vec2& out) {
    if (id != "giver") return false;
    out = Vec2(5, 5);
    return true;
  };
  w.monsters = [](const std::vector<std::string>&, std::vector<Vec2>& out) {
    out.push_back(Vec2(20, 0));
    out.push_back(Vec2(3, 4));
  };
  w.spawns = [](const std::vector<std::string>&, std::vector<Vec2>& out) { out.push_back(Vec2(50, 50)); };
  w.gatherSpots = [](std::string_view, int32_t, std::vector<Vec2>&) {};
  w.giverOf = [](std::string_view, std::string& out) {
    out = "giver";
    return true;
  };
  return w;
}

inline QuestProgress ActiveP(std::vector<int32_t> cur) { return Rec("q_test", QuestStatus::Active, std::move(cur)); }

// Number of Unicode code points of a UTF-8 string, '\n' excluded (StoryScript.test.ts `zh()`).
inline size_t CodePoints(std::string_view s) {
  size_t n = 0;
  for (unsigned char c : s) {
    if ((c & 0xC0) != 0x80 && c != '\n') ++n;
  }
  return n;
}

// NPC ids standing in a zone (camps + field NPCs).
inline std::set<std::string> NpcsIn(const DataStore& data, std::string_view zone) {
  std::set<std::string> out;
  const MapDef* m = data.FindMap(zone);
  if (m == nullptr) return out;
  for (const MapCampDef& c : m->camps) out.insert(c.npcs.begin(), c.npcs.end());
  for (const FieldNpcDef& f : m->fieldNpcs) out.insert(f.npcId);
  return out;
}

inline std::set<std::string> MonstersIn(const DataStore& data, std::string_view zone) {
  std::set<std::string> out;
  const MapDef* m = data.FindMap(zone);
  if (m == nullptr) return out;
  for (const MapSpawnDef& s : m->spawns) out.insert(s.monsterId);
  if (const MiniBossEntry* b = data.Monsters().MiniBossFor(zone)) out.insert(b->monsterId);
  return out;
}

// ---- dialogue navigation helpers (quest-dialogue-navigation.test.ts) and DlgRig -------------------------------------

// quest-dialogue-navigation.test.ts helpers (the UIScene filter rules, as data checks).
inline std::vector<const DialogueChoice*> DlgVisible(const DialogueTree& t, const DialogueNode& n,
                                                     const std::set<std::string>& active,
                                                     const std::set<std::string>& done) {
  std::vector<const DialogueChoice*> out;
  for (const DialogueChoice& c : n.choices) {
    bool ok = true;
    for (const std::string& p : c.prereqQuests) ok = ok && done.count(p) == 1;
    if (!ok) continue;
    if (!c.questTrigger.empty() && (active.count(c.questTrigger) == 1 || done.count(c.questTrigger) == 1)) {
      const DialogueNode* target = t.FindNode(c.nextNodeId);
      if (target != nullptr && target->isEnd) continue;
    }
    out.push_back(&c);
  }
  return out;
}

inline bool DlgCanReach(const DialogueTree& t, const std::string& quest, const std::set<std::string>& active) {
  std::vector<std::string> visited;
  std::function<bool(const std::string&)> dfs = [&](const std::string& id) -> bool {
    if (std::find(visited.begin(), visited.end(), id) != visited.end()) return false;
    visited.push_back(id);
    const DialogueNode* n = t.FindNode(id);
    if (n == nullptr) return false;
    const auto vis = DlgVisible(t, *n, active, {});
    for (const DialogueChoice* c : vis) {
      if (c->questTrigger == quest) return true;
      if (dfs(c->nextNodeId)) return true;
    }
    if (!n->nextNodeId.empty() && !n->isEnd && vis.empty() && dfs(n->nextNodeId)) return true;
    return false;
  };
  return dfs(t.startNodeId);
}

inline std::vector<std::string> DlgTriggers(const DialogueTree& t) {
  std::vector<std::string> out;
  for (const DialogueNode& n : t.nodes) {
    for (const DialogueChoice& c : n.choices) {
      if (!c.questTrigger.empty() && std::find(out.begin(), out.end(), c.questTrigger) == out.end()) {
        out.push_back(c.questTrigger);
      }
    }
  }
  return out;
}

// DialogueSystem + QuestSystem on the harness (no rewards service: choice rewards are skipped).
struct DlgRig {
  test::SimHarness h;
  QuestSystem qs{h.ctx.data, h.events, h.bus};
  DialogueSystem dlg{h.ctx};
  DlgRig() {
    h.ctx.sys.quests = &qs;
    h.ctx.sys.dialogue = &dlg;
  }
  void Open(const char* npc) {
    const DialogueTree* t = h.ctx.data.Dialogues().Find(npc);
    REQUIRE(t != nullptr);
    dlg.OpenTree(npc, *t);
  }
  // Index of the button presenting choice `choiceIndex` (-1 when hidden).
  int32_t ChoiceButton(int32_t choiceIndex) const {
    const auto& b = dlg.View().buttons;
    for (size_t i = 0; i < b.size(); ++i) {
      if (b[i].kind == DialogueButtonKind::Choice && b[i].choiceIndex == choiceIndex) return static_cast<int32_t>(i);
    }
    return -1;
  }
  std::vector<DialogueButtonKind> Kinds() const {
    std::vector<DialogueButtonKind> out;
    for (const DialogueButton& b : dlg.View().buttons) out.push_back(b.kind);
    return out;
  }
};

// ---- QwSim: GameSim with the real data and wiring (quest world, story director, lore, achievements, dialogue rewards)

struct QwSim {
  std::unique_ptr<GameSim> sim;
  SimContext* ctx = nullptr;
  explicit QwSim(uint64_t seed = 7, bool skipStory = true) {
    sim = GameSim::Create(test::RealData(), SimConfig{});
    REQUIRE(sim != nullptr);
    REQUIRE(sim->NewGame(ClassId::Warrior, Difficulty::Normal, seed));
    ctx = &sim->Context();
    if (skipStory) ctx->sys.story->FinishAllBeats();
    sim->Step();
  }
  QuestSystem& Qs() { return *ctx->sys.quests; }
  QuestWorld& Qw() { return *ctx->sys.questWorld; }
  StoryDirector& Story() { return *ctx->sys.story; }
  Hero& H() { return *ctx->sys.hero; }
  MonsterSystem& Mon() { return *ctx->sys.monsters; }
  void Steps(int n) {
    for (int i = 0; i < n; ++i) sim->Step();
  }
  // Steps while collecting every event (GameSim clears the list at each step).
  std::vector<Event> StepsCollect(int n) {
    std::vector<Event> out(sim->Events().begin(), sim->Events().end());
    for (int i = 0; i < n; ++i) {
      sim->Step();
      out.insert(out.end(), sim->Events().begin(), sim->Events().end());
    }
    return out;
  }
  void Place(Vec2 p) { H().SetPosition(p); }
  Vec2 NpcPos(const char* id) {
    const NpcPlacement* p = ctx->sys.zone->FindNpc(id);
    REQUIRE(p != nullptr);
    return p->pos;
  }
  EntityId FindAlive(std::string_view defId, Vec2 near = Vec2(-1, -1)) {
    if (near.x >= 0) return Mon().NearestAliveOfDef(defId, near);
    for (const MonsterInstance& m : Mon().All()) {
      if (m.IsAlive() && m.def.id == defId) return m.id;
    }
    return kNoEntity;
  }
  bool Kill(EntityId id) {
    if (id == kNoEntity) return false;
    DamageFlags f;
    f.attacker = kHeroEntityId;
    f.source = KillSource::HeroBasic;
    return Mon().ApplyDamage(id, 1e9, f) == HitWeight::Kill;
  }
  int32_t Cur(const char* q, size_t i) {
    const QuestProgress* p = Qs().Progress(q);
    return p != nullptr && i < p->objectives.size() ? p->objectives[i] : -1;
  }
  QuestStatus Status(const char* q) {
    bool has = false;
    const QuestStatus st = Qs().StatusOf(q, has);
    REQUIRE(has);
    return st;
  }
  // Plays the current beat to its end on real time, answering every input wait with StoryAdvance.
  void PlayBeat() {
    const std::string id = Story().Playback().beat.id;
    for (int i = 0; i < 2000 && Story().Playback().playing && Story().Playback().beat.id == id; ++i) {
      if (Story().Playback().waitingForInput) {
        Story().Advance();
      } else {
        sim->AdvanceRealTime(50);
      }
    }
  }
};

template <class E>
inline std::vector<const E*> Of(std::span<const Event> events) {
  std::vector<const E*> out;
  for (const Event& e : events) {
    if (const E* x = std::get_if<E>(&e)) out.push_back(x);
  }
  return out;
}

inline bool HasLog(std::span<const Event> events, std::string_view key) {
  for (const EvLog* l : Of<EvLog>(events)) {
    if (l->text.key == key) return true;
  }
  return false;
}

inline const QuestNode* NodeOf(QuestWorld& qw, bool clue, std::string_view questId, int32_t obj = -1) {
  for (const QuestNode& n : qw.Nodes()) {
    if (n.clue == clue && n.questId == questId && (obj < 0 || n.objectiveIndex == obj)) return &n;
  }
  return nullptr;
}

inline size_t CountNodes(QuestWorld& qw, bool clue) {
  size_t n = 0;
  for (const QuestNode& q : qw.Nodes()) n += q.clue == clue ? 1 : 0;
  return n;
}

// ---- audit-fix helpers: English strings, log lookup, saves placed in another zone -----------------------------------

inline I18n EnglishStrings() {
  I18n en;
  REQUIRE(en.LoadTable(test::ReadFile(test::DataDir() + "/i18n_en.json")));
  en.SetLocale(LocaleId::En);
  return en;
}

inline const EvLog* FindLog(std::span<const Event> events, std::string_view key) {
  for (const EvLog* l : Of<EvLog>(events)) {
    if (l->text.key == key) return l;
  }
  return nullptr;
}

// Builds a save of the running sim placed in another zone, with the given quests and seen beats.
inline SaveData SaveIn(QwSim& s, const char* map, int32_t col, int32_t row, std::vector<QuestProgress> quests,
                       std::vector<std::string> seen) {
  SaveData save;
  s.sim->BuildSave(save, 0);
  save.player.currentMap = map;
  save.player.tileCol = col;
  save.player.tileRow = row;
  save.quests = std::move(quests);
  save.hasStorySeen = true;
  save.storySeen = std::move(seen);
  return save;
}

}  // namespace abyss::test::quests_fx
