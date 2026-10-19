// Headless, seeded CHAPTER 1 playthrough (ARCHITECTURE 3.3, milestone-1 exit criterion 1; dark remake,
// implementation-plan.md 6).
//
// For each class: new game -> prologue (skipped) -> the Knellwatch camp (Hald's forge, Tamsin's stash, the Captain's
// dialogue tree and quest card, Aldwin's card) -> every emerald_plains quest (main M1-M5 and the six side quests:
// kill, drop-collect + delivery, gather, investigate + hunt, bounty hunt, escort, the Lanternwisp's clues, the
// Boneyard Gate defend) plus the Corpseherd and Valdras -> the Chapter 1 story beats -> save -> load into a fresh
// GameSim (SaveGame() identical) -> continue to the sealed chapter-2 gate.
//
// The player is abyss::AutoPilot (abyss/tools/AutoPilot.h), the same bot the in-game autotest drives (UE
// UAbyssAutoTestSubsystem, -abyssautotest): it plays only through the public GameSim API - Commands in, Events /
// Snapshot out, no debug commands - one frame per Tick. Its script is the route `ch1` of autopilot_routes.json
// (unreal/GameData/autopilot/ch1.ts); the route stops at named checkpoints (they cost no frame) and this test checks
// the state there, so a route change re-pins this file (and only this file).
//
// Environment switches: ABYSS_PT_VERBOSE=1 prints the bot's notes while it plays; ABYSS_PT_SWEEP=<n> (and
// ABYSS_PT_SWEEP_BASE=<first seed>) plays n extra seeds per class; ABYSS_PT_DUMP=<dir> writes each run's final save
// (<class>_<seed>.json) so GCC and Clang builds can be diffed - they must agree bit for bit.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "TestContent.h"
#include "TestUtil.h"
#include "abyss/base/Math.h"
#include "abyss/base/SimClock.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/items/GroundLoot.h"
#include "abyss/items/Inventory.h"
#include "abyss/items/Shop.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/pets/Homestead.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/quests/Achievements.h"
#include "abyss/quests/QuestWorld.h"
#include "abyss/sim/GameSim.h"
#include "abyss/sim/SimContext.h"
#include "abyss/story/StoryDirector.h"
#include "abyss/tools/AutoPilot.h"
#include "abyss/world/Zone.h"
#include "doctest/doctest.h"

using namespace abyss;

namespace {

constexpr double kPtFrameMs = kSimStepMs;
constexpr int64_t kPtFps = 60;
constexpr int64_t kPtMaxFrames = 3 * 3600 * kPtFps;  // a run that has not finished after 3 h of game time is stuck

bool PtVerbose() {
  const char* v = std::getenv("ABYSS_PT_VERBOSE");
  return v != nullptr && *v != '\0' && *v != '0';
}

const char* PtClassName(ClassId c) {
  switch (c) {
    case ClassId::Warrior: return "warrior";
    case ClassId::Mage: return "mage";
    case ClassId::Rogue: return "rogue";
  }
  return "?";
}

bool PtContains(const std::vector<std::string>& v, const std::string& s) {
  return std::find(v.begin(), v.end(), s) != v.end();
}

const QuestProgress* PtProgress(const Snapshot& v, const std::string& questId) {
  return v.quests != nullptr ? v.quests->Progress(questId) : nullptr;
}
bool PtHasStatus(const Snapshot& v, const std::string& questId, QuestStatus s) {
  const QuestProgress* p = PtProgress(v, questId);
  return p != nullptr && p->status == s;
}
int32_t PtObjCurrent(const Snapshot& v, const std::string& questId, size_t obj) {
  const QuestProgress* p = PtProgress(v, questId);
  return p != nullptr && obj < p->objectives.size() ? p->objectives[obj] : 0;
}

AutoPilotConfig PtConfig(ClassId cls, AutoPilotScenario scenario) {
  AutoPilotConfig cfg;
  cfg.cls = cls;
  cfg.scenario = scenario;
  cfg.storyMode = AutoPilotStoryMode::PlayerLike;
  if (PtVerbose()) cfg.onNote = [](const std::string& line) { std::printf("%s\n", line.c_str()); };
  return cfg;
}

// One bot on one GameSim.
struct PtPlayer {
  PtPlayer(GameSim& s, ClassId cls, AutoPilotScenario scenario)
      : sim(s), pilot(test::RealData(), PtConfig(cls, scenario), [&s](const Command& c) { s.Submit(c); }) {}

  // The events NewGame / LoadGame left before the first frame (zone entry, prologue), as UE dispatches them.
  void ObserveSessionStart() {
    for (const Event& e : sim.Events()) pilot.ObserveEvent(e);
  }
  const Snapshot& V() const { return sim.View(); }
  bool HasStatus(const std::string& questId, QuestStatus s) const { return PtHasStatus(V(), questId, s); }
  const AutoPilotStats& Stats() const { return pilot.Stats(); }

  // Plays until the script reaches the checkpoint `id` (the next one it meets must be `id`). False when the bot failed,
  // the script ended first or the checkpoints are out of order.
  bool To(const std::string& id) {
    for (int64_t guard = 0; guard < kPtMaxFrames; ++guard) {
      const AutoPilotStatus st = pilot.Tick(sim, kPtFrameMs);
      if (st == AutoPilotStatus::Checkpoint) {
        if (PtVerbose()) {
          int64_t total = V().hero.exp;  // the exp granted so far (classes 5.1 curve)
          for (int32_t l = 1; l < V().hero.level; ++l) total += static_cast<int64_t>(std::floor(3.0 * l * l + 25.0 * l));
          std::printf("-- checkpoint %s: level %d, exp %lld, kills %d, deaths %d\n", pilot.CurrentCheckpoint().c_str(),
                      V().hero.level, static_cast<long long>(total), pilot.Stats().kills, pilot.Stats().deaths);
        }
        if (pilot.CurrentCheckpoint() == id) return true;
        MESSAGE("expected checkpoint " << id << ", the script reached " << pilot.CurrentCheckpoint());
        return false;
      }
      if (st == AutoPilotStatus::Finished) {
        if (pilot.Ok()) MESSAGE("the script ended before checkpoint " << id);
        return false;
      }
    }
    MESSAGE("no checkpoint " << id << " within " << kPtMaxFrames << " frames");
    return false;
  }
  // Plays the remaining steps. True when they all succeeded.
  bool ToEnd() {
    for (int64_t guard = 0; guard < kPtMaxFrames; ++guard) {
      if (pilot.Tick(sim, kPtFrameMs) == AutoPilotStatus::Finished) return pilot.Ok();
    }
    MESSAGE("the script did not finish within " << kPtMaxFrames << " frames");
    return false;
  }
  void Frame() { pilot.RunFrame(sim, kPtFrameMs); }

  GameSim& sim;
  AutoPilot pilot;
};

// Plays until the script reaches the checkpoint `id`, passing any other checkpoint on the way. False when the script
// finished first or no such checkpoint came within kPtMaxFrames.
bool PtPlayTo(PtPlayer& p, const std::string& id) {
  for (int64_t guard = 0; guard < kPtMaxFrames; ++guard) {
    const AutoPilotStatus st = p.pilot.Tick(p.sim, kPtFrameMs);
    if (st == AutoPilotStatus::Finished) return false;
    if (st == AutoPilotStatus::Checkpoint && p.pilot.CurrentCheckpoint() == id) return true;
  }
  return false;
}

// Test-only: kills every living monster of `ids` (as the hero would), so a forced situation leaves nothing behind.
void PtKillAll(SimContext& ctx, const std::vector<EntityId>& ids) {
  DamageFlags f;
  f.attacker = kHeroEntityId;
  f.source = KillSource::HeroBasic;
  for (EntityId id : ids) {
    const MonsterInstance* m = ctx.sys.monsters->Find(id);
    if (m != nullptr && m->IsAlive()) ctx.sys.monsters->ApplyDamage(id, 1e9, f);
  }
}

struct PtResult {
  bool ok = false;
  std::string failure;
  std::string recent;
  std::string finalSave;  // SaveGame(kPtSaveMs) after the finale
};

constexpr int64_t kPtSaveMs = 1760000000000;

// Chapter 1's quests (chapter1.md 5-7): main M1-M5, side S1-S6.
const std::vector<std::string> kPtAllCh1Quests = {
    test::ch1::kQuestM1, test::ch1::kQuestM2, test::ch1::kQuestM3, test::ch1::kQuestM4, test::ch1::kQuestM5,
    "q_rf_black_blood",  "q_rf_barrowmoss",   "q_rf_tag_thief",    "q_rf_oathless",     "q_rf_lanternwisp",
    "q_rf_hallowed_ground"};

// Plays chapter 1 to the finale (no save/load), checking the state at each checkpoint of the route. False when the bot
// failed or a checkpoint was not reached.
bool PtPlayChapter1(PtPlayer& p) {
  using test::ch1::kQuestM1;
  using test::ch1::kQuestM2;
  using test::ch1::kQuestM3;
  using test::ch1::kQuestM4;
  using test::ch1::kQuestM5;
  // ---- new game: prologue (skipped) + chapter card ----
  if (!p.To("start")) return false;
  CHECK(p.V().storyBusy);
  if (!p.To("intro")) return false;
  CHECK(PtContains(p.Stats().beatsEnded, "prologue"));
  CHECK(PtContains(p.Stats().beatsEnded, "chapter_emerald_plains"));
  CHECK(p.V().zone.mapId == test::ch1::kZone);

  // ---- Knellwatch: Hald, Tamsin, the Captain's tree (reward_ask once, S1 by its trigger), M1, Aldwin's S2 + S5 ----
  if (!p.To("blacksmith")) return false;
  CHECK(p.V().shop != nullptr);
  CHECK((p.V().shop != nullptr && p.V().shop->blacksmith));
  if (!p.To("stash")) return false;
  CHECK(p.V().stash != nullptr);
  CHECK((p.V().stash != nullptr && p.V().stash->open));
  if (!p.To("captain_tree")) return false;
  CHECK(p.HasStatus("q_rf_black_blood", QuestStatus::Active));  // accept_ichor's questTrigger
  CHECK(p.V().hero.gold == 30);                                  // reward_ask supplies (Q1, one-time)
  if (!p.To("first_quests")) return false;
  CHECK(p.HasStatus(kQuestM1, QuestStatus::Active));
  CHECK(p.HasStatus("q_rf_barrowmoss", QuestStatus::Active));
  CHECK(p.HasStatus("q_rf_lanternwisp", QuestStatus::Active));

  // ---- barrowmoss (S2), the Burial Trenches (M1), the Seep (S1's ichor) ----
  if (!p.To("barrowmoss")) return false;
  CHECK(p.HasStatus("q_rf_barrowmoss", QuestStatus::Completed));
  if (!p.To("trenches")) return false;
  CHECK(p.HasStatus(kQuestM1, QuestStatus::Completed));
  if (!p.To("seep")) return false;
  CHECK(PtObjCurrent(p.V(), "q_rf_black_blood", 0) == 6);

  // ---- home: M1 (cs_rf_brand), take M2; Aldwin: S2, take S3 ----
  if (!p.To("captain_m1_turned_in")) return false;
  CHECK(p.HasStatus(kQuestM1, QuestStatus::TurnedIn));
  CHECK(p.HasStatus(kQuestM2, QuestStatus::Active));
  if (!p.To("captain_m1")) return false;
  CHECK(PtContains(p.Stats().beatsEnded, "cs_rf_brand"));
  if (!p.To("chaplain_m1")) return false;
  CHECK(p.HasStatus("q_rf_barrowmoss", QuestStatus::TurnedIn));
  CHECK(p.HasStatus("q_rf_tag_thief", QuestStatus::Active));

  // ---- the tag thief: clues 1-2, Marl (S1's delivery), clue 3, Dreck ----
  if (!p.To("marl")) return false;
  CHECK(p.HasStatus("q_rf_black_blood", QuestStatus::Completed));
  if (!p.To("dreck")) return false;
  CHECK(p.HasStatus("q_rf_tag_thief", QuestStatus::Completed));

  // ---- the Gnawed Field (twelve gravegnawers, the Hungerbell), the lantern graves ----
  if (!p.To("gnawed_field")) return false;
  CHECK(p.HasStatus(kQuestM2, QuestStatus::Completed));
  if (!p.To("lanternwisp")) return false;
  CHECK(p.HasStatus("q_rf_lanternwisp", QuestStatus::Completed));

  // ---- home: S1 + M2, take M3; Aldwin: S3 + S5 (the Lanternwisp), take S6 ----
  if (!p.To("home_m2_turned_in")) return false;
  CHECK(p.HasStatus("q_rf_black_blood", QuestStatus::TurnedIn));
  CHECK(p.HasStatus(kQuestM2, QuestStatus::TurnedIn));
  CHECK(p.HasStatus("q_rf_tag_thief", QuestStatus::TurnedIn));
  CHECK(p.HasStatus("q_rf_lanternwisp", QuestStatus::TurnedIn));
  CHECK(p.HasStatus(kQuestM3, QuestStatus::Active));
  CHECK(p.HasStatus("q_rf_hallowed_ground", QuestStatus::Active));
  if (!p.To("home_m2")) return false;
  CHECK(p.Stats().petObtained);
  CHECK(p.V().pet.present);
  CHECK(p.V().pet.petId == "pet_sprite");  // the Lanternwisp

  // ---- M3: Ashwheat, then the Corpseherd at the Sheepfold (cs_rf_herd) ----
  if (!p.To("ashwheat")) return false;
  CHECK(PtObjCurrent(p.V(), kQuestM3, 0) == 1);
  if (!p.To("corpseherd")) return false;
  CHECK(p.HasStatus(kQuestM3, QuestStatus::Completed));
  bool miniBossAlive = false;
  for (const MonsterView& m : p.V().monsters) miniBossAlive = miniBossAlive || (m.miniBoss && m.alive);
  CHECK_FALSE(miniBossAlive);
  CHECK(p.Stats().miniBossDialogues >= 1);
  CHECK(PtContains(p.Stats().beatsBegan, "cs_rf_herd"));

  // ---- S6: Old Bern to the last hallowed ground ----
  if (!p.To("escort")) return false;
  CHECK(p.HasStatus("q_rf_hallowed_ground", QuestStatus::Completed));

  // ---- S4: Corran's bounty on Harke ----
  if (!p.To("corran")) return false;
  CHECK(p.HasStatus("q_rf_oathless", QuestStatus::Active));
  if (!p.To("harke")) return false;
  CHECK(p.HasStatus("q_rf_oathless", QuestStatus::Completed));
  if (!p.To("oathless")) return false;
  CHECK(p.HasStatus("q_rf_oathless", QuestStatus::TurnedIn));

  // ---- home: M3 (cs_rf_whisper), take M4; Aldwin: S6 ----
  if (!p.To("home_m3_turned_in")) return false;
  CHECK(p.HasStatus(kQuestM3, QuestStatus::TurnedIn));
  CHECK(p.HasStatus(kQuestM4, QuestStatus::Active));
  if (!p.To("home_m3")) return false;
  CHECK(PtContains(p.Stats().beatsEnded, "cs_rf_whisper"));
  CHECK(p.HasStatus("q_rf_hallowed_ground", QuestStatus::TurnedIn));

  // ---- M4: Lord-Marshal Valdras on the Breach Rim (boss intro, cs_rf_marshal_falls) ----
  if (!p.To("valdras")) return false;
  CHECK(p.HasStatus(kQuestM4, QuestStatus::Completed));
  CHECK(PtContains(p.Stats().beatsEnded, "boss_valdras"));
  CHECK(PtContains(p.Stats().beatsBegan, "cs_rf_marshal_falls"));

  // ---- home: M4 (weapon / armour choice), take M5 (cs_rf_siege) ----
  if (!p.To("home_m4_turned_in")) return false;
  CHECK(p.HasStatus(kQuestM4, QuestStatus::TurnedIn));
  CHECK(p.HasStatus(kQuestM5, QuestStatus::Active));
  if (!p.To("home_m4")) return false;
  CHECK(PtContains(p.Stats().beatsEnded, "cs_rf_siege"));

  // ---- M5: hold the Boneyard Gate till dawn; home (cs_rf_finale) ----
  if (!p.To("siege")) return false;
  CHECK(p.HasStatus(kQuestM5, QuestStatus::Completed));
  CHECK(PtObjCurrent(p.V(), kQuestM5, 0) == 3);  // three waves held
  if (!p.To("home_m5_turned_in")) return false;
  CHECK(p.HasStatus(kQuestM5, QuestStatus::TurnedIn));
  if (!p.To("finale")) return false;
  CHECK(PtContains(p.Stats().beatsEnded, "cs_rf_finale"));
  if (!p.ToEnd()) return false;
  CHECK(p.pilot.Reached(AutoPilotMilestone::ScenarioDone));
  return true;
}

void PtCheckChapterDone(PtPlayer& p) {
  const Snapshot& v = p.V();
  const AutoPilotStats& st = p.Stats();
  for (const std::string& q : kPtAllCh1Quests) {
    INFO("quest " << q);
    CHECK(p.HasStatus(q, QuestStatus::TurnedIn));
  }
  for (const char* beat : {"prologue", "chapter_emerald_plains", "cs_rf_brand", "cs_rf_herd", "cs_rf_whisper",
                           "boss_valdras", "cs_rf_marshal_falls", "cs_rf_siege", "cs_rf_finale"}) {
    INFO("beat " << beat);
    CHECK(PtContains(st.beatsEnded, beat));
    CHECK(v.story != nullptr);
  }
  // The kill beats play on the kill that counts (story trigger whileQuest, DECISIONS M7 note): cs_rf_herd after M3 was
  // accepted, cs_rf_marshal_falls after M4 was.
  const auto firstAt = [&p](AutoPilotMilestone kind, const std::string& detail) -> int64_t {
    const std::vector<AutoPilotMilestoneRecord>& ms = p.pilot.Milestones();
    for (size_t i = 0; i < ms.size(); ++i) {
      if (ms[i].kind == kind && ms[i].detail == detail) return static_cast<int64_t>(i);
    }
    return -1;
  };
  for (const auto& [beat, quest] : {std::pair<std::string, std::string>{"cs_rf_herd", test::ch1::kQuestM3},
                                    std::pair<std::string, std::string>{"cs_rf_marshal_falls", test::ch1::kQuestM4}}) {
    INFO("beat " << beat);
    const int64_t accepted = firstAt(AutoPilotMilestone::QuestAccepted, quest);
    const int64_t played = firstAt(AutoPilotMilestone::StoryBeat, beat);
    CHECK(accepted >= 0);
    CHECK(played > accepted);
  }
  // Exp ledger: the level + exp into it account for exactly the exp granted (classes 5.1 curve, 5.2 one level per
  // grant with the overflow kept), and quest rewards pay the chapter's 2740 (chapter1.md 5-7).
  int64_t granted = 0;
  for (int64_t x : st.expBySource) granted += x;
  int64_t cumulative = 0;
  for (int32_t l = 1; l < v.hero.level; ++l) cumulative += static_cast<int64_t>(std::floor(3.0 * l * l + 25.0 * l));
  CHECK(cumulative + v.hero.exp == granted);
  CHECK(st.expBySource[static_cast<size_t>(ExpSource::Quest)] == 2740);
  CHECK(st.expBySource[static_cast<size_t>(ExpSource::DialogueReward)] == 50);  // reward_ask, once (Q1)
  CHECK(st.expBySource[static_cast<size_t>(ExpSource::DeathPenalty)] == 0);     // Normal: no exp toll
  // Level band (implementation-plan.md 6; DECISIONS DR9): 12-14. The remade quests need more kills than the web's (the
  // M2 Hungerbell, the M5 waves, Valdras), so the Chapter 1 roster gives round(0.75 x monsters.md 2.0 Exp) and Valdras
  // 110 (DR9 *as built*): with the 2740 quest exp and what the bot fights on the way a full chapter lands on Lv 13-14
  // (Lv 12 starts at 3168 exp, Lv 15 at 5670).
  INFO("level " << v.hero.level);
  CHECK(v.hero.level >= 12);
  CHECK(v.hero.level <= 14);
  CHECK(v.hero.gold > 0);
  CHECK(v.hero.gold < 100000);
  CHECK(v.hero.freeStatPoints == 0);
  CHECK(st.itemsPicked > 0);
  CHECK(st.equipChanges > 0);
  CHECK(v.inventory != nullptr);
  int equipped = 0;
  for (const auto& slot : v.inventory->Equipment()) equipped += slot.has_value() ? 1 : 0;
  CHECK(equipped >= 3);  // drops vary; the pick-one rewards alone fill a weapon / armour and jewellery slot
  CHECK(v.inventory->Bag().size() < 100);
  CHECK(v.pet.present);
  REQUIRE(v.pets != nullptr);
  const PetInstance* wisp = v.pets->Find("pet_sprite");
  REQUIRE(wisp != nullptr);
  CHECK(v.pets->ActiveId() == "pet_sprite");
  CHECK(wisp->level > 1);  // beast exp from every kill since the turn-in (18.3 onKill)
  // the named kills the chapter requires: the story boss, the mini-boss and the three hunt leaders
  for (const char* boss : {test::ch1::kStoryBoss, "miniboss_corpseherd", "hunt_dreck", "hunt_hungerbell",
                           "hunt_harke"}) {
    INFO("killed " << boss);
    CHECK(PtContains(st.killedDefs, boss));
  }
  // embers (quests 4.4): 23 from the eleven turn-ins (chapter1.md 7: M5 7 and S5 3, the others their default 2 / 1) + 5 per
  // elite / mini-boss / affixed kill (>= the five above)
  REQUIRE(v.homestead != nullptr);
  CHECK(v.homestead->Embers() >= 40);
  CHECK((v.homestead->Embers() - 23) % 5 == 0);
  CHECK(v.homestead->IsBuildingUnlocked("herb_garden"));  // M5 sends Marl to the tower (4.6)
  // achievements a straight chapter-1 run earns (quests 10.5, chapter1.md 14)
  REQUIRE(v.achievements != nullptr);
  for (const char* ach : {"ach_first_kill", "ach_kill_valdras", "ach_quest_10", "ach_level_10"}) {
    INFO("achievement " << ach);
    CHECK(v.achievements->IsUnlocked(ach));
  }
  CHECK(st.deaths <= 2);
  CHECK(st.levelUps >= 11);
  CHECK(st.kills >= 40);
  // The retry paths (ApEscortWithRetries / ApDefendWithRetries, the defend's retreat): a gate or an escort lost on every
  // first attempt, or a bot that keeps running home, is a regression even when the retry saves the run.
  CHECK(st.escortFailures <= 1);
  CHECK(st.defendFailures <= 1);
  CHECK(st.defendRetreats <= 2);
  CHECK(st.saveRequests > 0);
  CHECK_FALSE(v.frozen);
  CHECK(v.modals.empty());
  CHECK(v.hero.life == HeroLife::Alive);
  // the milestones the in-game autotest reports
  for (AutoPilotMilestone m : {AutoPilotMilestone::ZoneEntered, AutoPilotMilestone::FirstKill,
                               AutoPilotMilestone::BossKill, AutoPilotMilestone::QuestAccepted,
                               AutoPilotMilestone::QuestTurnedIn, AutoPilotMilestone::StoryBeat,
                               AutoPilotMilestone::Cutscene, AutoPilotMilestone::LevelUp,
                               AutoPilotMilestone::PetObtained, AutoPilotMilestone::ScenarioDone,
                               AutoPilotMilestone::ChapterDone}) {
    INFO("milestone " << EnumName(m));
    CHECK(p.pilot.Reached(m));
  }
}

// Save -> load into a fresh GameSim (identical SaveGame) -> walk to the sealed chapter-2 gate.
void PtSaveLoadAndGate(PtPlayer& p, ClassId cls) {
  p.pilot.AddWaitIdle(120);
  REQUIRE(p.ToEnd());
  REQUIRE(p.sim.CanSave());
  const std::string save = p.sim.SaveGame(kPtSaveMs);
  auto fresh = GameSim::Create(test::RealData(), SimConfig{});
  REQUIRE(fresh != nullptr);
  std::string err;
  REQUIRE(fresh->LoadGame(save, &err) == SaveError::None);
  const std::string again = fresh->SaveGame(kPtSaveMs);
  CHECK(again == save);
  if (again != save) {
    size_t i = 0;
    while (i < again.size() && i < save.size() && again[i] == save[i]) ++i;
    const size_t from = i > 120 ? i - 120 : 0;
    MESSAGE("first difference at " << i << "\n  saved:  " << save.substr(from, 260) << "\n  loaded: "
                                   << again.substr(from, 260));
  }
  CHECK(fresh->View().zone.mapId == test::ch1::kZone);
  CHECK(fresh->View().hero.level == p.V().hero.level);

  PtPlayer cont(*fresh, cls, AutoPilotScenario::None);
  cont.ObserveSessionStart();
  cont.Frame();
  cont.pilot.AddWaitIdle(60);
  cont.pilot.AddCheckpoint("loaded");
  cont.pilot.AddGoTo(Vec2(118, 60), 0.4, 600);  // the exit's inner tile (the exit itself sits on the border wall)
  REQUIRE(cont.To("loaded"));
  CHECK(cont.V().quests->IsTurnedIn(test::ch1::kQuestM5));
  // the sealed gate to chapter 2 (W7): a coming-soon banner, no transition
  bool sealedMarker = false;
  for (const WorldMarkerView& m : cont.V().markers) {
    if (m.kind == MarkerKind::Exit && m.key == "twilight_forest") sealedMarker = m.sealed;
  }
  CHECK(sealedMarker);
  cont.ToEnd();
  for (int i = 0; i < 3 * kPtFps && cont.Stats().sealedBanners == 0; ++i) cont.Frame();
  CHECK(cont.pilot.Ok());
  if (!cont.pilot.Ok()) MESSAGE(cont.pilot.Failure() << "\n" << cont.pilot.RecentNotes());
  CHECK(cont.Stats().sealedBanners >= 1);
  cont.pilot.AddWait(2);
  cont.ToEnd();
  CHECK(cont.V().zone.mapId == test::ch1::kZone);
}

PtResult PtRun(ClassId cls, uint64_t seed, bool saveLoad) {
  PtResult r;
  auto sim = GameSim::Create(test::RealData(), SimConfig{});
  REQUIRE(sim != nullptr);
  REQUIRE(sim->NewGame(cls, Difficulty::Normal, seed));
  PtPlayer p(*sim, cls, AutoPilotScenario::Chapter1);
  p.ObserveSessionStart();
  const bool played = PtPlayChapter1(p);
  r.ok = played && p.pilot.Ok();
  r.failure = p.pilot.Failure();
  r.recent = p.pilot.RecentNotes();
  if (!r.ok) {
    MESSAGE(std::string(PtClassName(cls)) << " seed " << seed << " playthrough failed at step '"
                                          << p.pilot.CurrentStep() << "' after checkpoint '"
                                          << p.pilot.CurrentCheckpoint() << "': " << p.pilot.Failure() << "\n"
                                          << p.pilot.RecentNotes());
    CHECK(r.ok);
    return r;
  }
  PtCheckChapterDone(p);
  const AutoPilotStats& st = p.Stats();
  MESSAGE(std::string(PtClassName(cls))
          << ": level " << p.V().hero.level << ", gold " << p.V().hero.gold << ", kills " << st.kills << ", deaths "
          << st.deaths << ", potions " << st.potionsDrunk << ", picked " << st.itemsPicked << ", sold "
          << st.itemsSold << ", casts " << st.skillCasts << ", play time " << sim->View().playTimeMs / 60000.0
          << " min, exp kill " << st.expBySource[0] << " quest " << st.expBySource[1] << " dialogue "
          << st.expBySource[2] << " event " << st.expBySource[3] << ", pet feeds " << st.petFeeds);
  r.finalSave = sim->SaveGame(kPtSaveMs);
  // ABYSS_PT_DUMP=<dir>: keep the final saves (e.g. to diff GCC and Clang builds, which must agree bit for bit).
  if (const char* dir = std::getenv("ABYSS_PT_DUMP")) {
    std::ofstream out(std::string(dir) + "/" + PtClassName(cls) + "_" + std::to_string(seed) + ".json",
                      std::ios::binary);
    out << r.finalSave;
  }
  if (saveLoad) PtSaveLoadAndGate(p, cls);
  return r;
}

// The in-game autotest's smoke scenario (UE -abyssautotest=smoke): the route up to its smokeEnd step (the first
// main-quest turn-in and its cutscene), every milestone kind but the chapter's end, and nothing after the route's last
// checkpoint before smokeEnd.
// ueHolds: like UE, hold the bot (SetHold) for 2 s when the first story beat begins and for 1 s when the first quest
// card opens - the autotest photographs them meanwhile.
void PtRunSmoke(ClassId cls, uint64_t seed, AutoPilotStoryMode storyMode, double frameMs, bool ueHolds = false) {
  auto sim = GameSim::Create(test::RealData(), SimConfig{});
  REQUIRE(sim != nullptr);
  REQUIRE(sim->NewGame(cls, Difficulty::Normal, seed));
  AutoPilotConfig cfg = PtConfig(cls, AutoPilotScenario::Smoke);
  cfg.storyMode = storyMode;
  std::vector<std::string> checkpoints;
  int64_t holdFrames = 0;  // frames left to hold
  bool beatHeld = false, cardHeld = false;
  cfg.onMilestone = [&](const AutoPilotMilestoneRecord& m) {
    if (m.kind == AutoPilotMilestone::Checkpoint) checkpoints.push_back(m.detail);
    if (ueHolds && m.kind == AutoPilotMilestone::StoryBeat && !beatHeld) {
      beatHeld = true;
      holdFrames = 2 * kPtFps;
    }
  };
  AutoPilot pilot(test::RealData(), cfg, [&sim](const Command& c) { sim->Submit(c); });
  for (const Event& e : sim->Events()) pilot.ObserveEvent(e);  // NewGame's zone entry and prologue
  int64_t frames = 0;
  int64_t heldFrames = 0;
  while (frames < kPtMaxFrames) {
    if (ueHolds && !cardHeld && sim->View().questCard != nullptr && sim->View().questCard->open) {
      cardHeld = true;
      holdFrames = kPtFps;
    }
    pilot.SetHold(holdFrames > 0);
    if (holdFrames > 0) {
      --holdFrames;
      ++heldFrames;
    }
    if (pilot.Tick(*sim, frameMs) == AutoPilotStatus::Finished) break;
    ++frames;
  }
  if (ueHolds) {
    CHECK(beatHeld);
    CHECK(cardHeld);
    CHECK(heldFrames >= 3 * kPtFps);
  }
  INFO(PtClassName(cls) << " seed " << seed << " story " << EnumName(storyMode) << " frame " << frameMs << " ms");
  if (!pilot.Ok()) MESSAGE(pilot.Failure() << "\n" << pilot.RecentNotes());
  REQUIRE(pilot.Ok());
  CHECK(pilot.Finished());
  CHECK(!checkpoints.empty());
  CHECK(pilot.CurrentCheckpoint() == "chaplain_m1");  // the route's last checkpoint before its smokeEnd
  const Snapshot& v = sim->View();
  CHECK(PtHasStatus(v, test::ch1::kQuestM1, QuestStatus::TurnedIn));
  CHECK(PtHasStatus(v, "q_rf_barrowmoss", QuestStatus::TurnedIn));
  CHECK(PtHasStatus(v, test::ch1::kQuestM2, QuestStatus::Active));
  CHECK(PtHasStatus(v, "q_rf_tag_thief", QuestStatus::Active));
  CHECK_FALSE(PtHasStatus(v, test::ch1::kQuestM5, QuestStatus::Active));
  CHECK(PtContains(pilot.Stats().beatsEnded, "cs_rf_brand"));
  for (AutoPilotMilestone m : {AutoPilotMilestone::ZoneEntered, AutoPilotMilestone::FirstKill,
                               AutoPilotMilestone::QuestAccepted, AutoPilotMilestone::QuestTurnedIn,
                               AutoPilotMilestone::StoryBeat, AutoPilotMilestone::Cutscene,
                               AutoPilotMilestone::LevelUp, AutoPilotMilestone::ScenarioDone}) {
    INFO("milestone " << EnumName(m));
    CHECK(pilot.Reached(m));
  }
  CHECK_FALSE(pilot.Reached(AutoPilotMilestone::ChapterDone));
  CHECK(pilot.Stats().skillCasts > 0);
  CHECK(v.hero.life == HeroLife::Alive);
}

}  // namespace

TEST_SUITE("playthrough") {
  TEST_CASE("chapter 1 playthrough: warrior") { PtRun(ClassId::Warrior, 101, true); }
  TEST_CASE("chapter 1 playthrough: mage") { PtRun(ClassId::Mage, 202, true); }
  TEST_CASE("chapter 1 playthrough: rogue") { PtRun(ClassId::Rogue, 303, true); }
  // Stress sweep (off by default): ABYSS_PT_SWEEP=<n> plays n more seeds per class, ABYSS_PT_SWEEP_BASE the first seed.
  TEST_CASE("chapter 1 playthrough: seed sweep (ABYSS_PT_SWEEP)") {
    const char* n = std::getenv("ABYSS_PT_SWEEP");
    const char* base = std::getenv("ABYSS_PT_SWEEP_BASE");
    const int count = n != nullptr ? std::atoi(n) : 0;
    const uint64_t first = base != nullptr ? static_cast<uint64_t>(std::atoll(base)) : 1000;
    for (int i = 0; i < count; ++i) {
      for (ClassId cls : {ClassId::Warrior, ClassId::Mage, ClassId::Rogue}) {
        INFO(std::string(PtClassName(cls)) << " seed " << first + static_cast<uint64_t>(i));
        PtRun(cls, first + static_cast<uint64_t>(i), true);
      }
    }
  }
  // The defend retry path (ApDefendWithRetries): the gate falls once - the test holds the bot when the first wave is
  // out, stands the hero on the gate and drops the gate to 1 life - then the bot re-takes M5 from the Captain and holds
  // the gate through every wave.
  TEST_CASE("autopilot defend: a fallen gate is re-taken from its giver and held") {
    auto sim = GameSim::Create(test::RealData(), SimConfig{});
    REQUIRE(sim != nullptr);
    REQUIRE(sim->NewGame(ClassId::Warrior, Difficulty::Normal, 101));
    PtPlayer p(*sim, ClassId::Warrior, AutoPilotScenario::Chapter1);
    p.ObserveSessionStart();
    REQUIRE(PtPlayTo(p, "home_m4"));  // M5 taken
    REQUIRE(p.HasStatus(test::ch1::kQuestM5, QuestStatus::Active));
    SimContext& ctx = sim->Context();
    const QuestWorld& qw = *ctx.sys.questWorld;
    for (int64_t guard = 0; guard < 600 * kPtFps && !qw.Defend().waveActive; ++guard) {
      REQUIRE(p.pilot.Tick(*sim, kPtFrameMs) == AutoPilotStatus::NeedFrame);
    }
    REQUIRE(qw.Defend().waveActive);
    p.pilot.SetHold(true);
    // Test-only: the gate is down to 1 life and one wave monster stands on it, so the next chip brings it down.
    const_cast<DefendState&>(qw.Defend()).hp = 1;
    MonsterInstance* raider = nullptr;
    for (EntityId id : qw.Defend().waveMonsters) {
      MonsterInstance* m = ctx.sys.monsters->Find(id);
      if (m != nullptr && m->IsAlive() && raider == nullptr) raider = m;
    }
    REQUIRE(raider != nullptr);
    raider->pos = qw.Defend().pos;
    for (int64_t guard = 0; guard < 10 * kPtFps && !p.HasStatus(test::ch1::kQuestM5, QuestStatus::Failed); ++guard) {
      p.Frame();
    }
    REQUIRE(p.HasStatus(test::ch1::kQuestM5, QuestStatus::Failed));
    p.pilot.SetHold(false);
    REQUIRE(PtPlayTo(p, "siege"));
    INFO(p.pilot.RecentNotes());
    CHECK(p.pilot.Ok());
    CHECK(p.Stats().defendFailures == 1);
    CHECK(p.HasStatus(test::ch1::kQuestM5, QuestStatus::Completed));
    CHECK(PtObjCurrent(p.V(), test::ch1::kQuestM5, 0) == 3);
  }

  // A route step that defends a quest it never accepted fails at once with the reason, not after 900 s of standing.
  TEST_CASE("autopilot defend: a defend step for a quest that is not open fails at its start") {
    auto sim = GameSim::Create(test::RealData(), SimConfig{});
    REQUIRE(sim != nullptr);
    REQUIRE(sim->NewGame(ClassId::Warrior, Difficulty::Normal, 7));
    PtPlayer p(*sim, ClassId::Warrior, AutoPilotScenario::None);
    p.ObserveSessionStart();
    p.pilot.AddWaitIdle(120);
    p.pilot.AddDefend(test::ch1::kQuestM5, 1, 3.0);
    for (int64_t guard = 0; guard < 10 * kPtFps && !p.pilot.Finished(); ++guard) p.pilot.Tick(*sim, kPtFrameMs);
    CHECK(p.pilot.Finished());
    CHECK_FALSE(p.pilot.Ok());
    CHECK(p.pilot.Failure().find("not active") != std::string::npos);
  }

  // A gate that fell BEFORE the defend step is a lost attempt, not a bot failure (ApDefend::Start): M5 is Failed - the
  // re-acceptable state - when the step starts, so ApDefendWithRetries takes it again from the Captain and holds it.
  // Here the hero, still in Knellwatch after taking M5, is put at the gate (test-only) until the first wave is out, the
  // gate drops to 1 life with a raider on it, then the hero is put back and the wave cleared.
  TEST_CASE("autopilot defend: a gate that fell before its step is re-taken from its giver and held") {
    auto sim = GameSim::Create(test::RealData(), SimConfig{});
    REQUIRE(sim != nullptr);
    REQUIRE(sim->NewGame(ClassId::Rogue, Difficulty::Normal, 303));
    PtPlayer p(*sim, ClassId::Rogue, AutoPilotScenario::Chapter1);
    p.ObserveSessionStart();
    REQUIRE(PtPlayTo(p, "home_m4"));  // M5 taken; the town trip and the defend step come next
    REQUIRE(p.HasStatus(test::ch1::kQuestM5, QuestStatus::Active));
    p.pilot.SetHold(true);
    SimContext& ctx = sim->Context();
    const QuestWorld& qw = *ctx.sys.questWorld;
    REQUIRE(qw.Defend().active);
    const Vec2 home = ctx.sys.hero->Position();
    ctx.sys.hero->SetPosition(qw.Defend().pos + Vec2(1, 0));
    for (int64_t guard = 0; guard < 30 * kPtFps && !qw.Defend().waveActive; ++guard) p.Frame();
    REQUIRE(qw.Defend().waveActive);
    const std::vector<EntityId> wave = qw.Defend().waveMonsters;
    const_cast<DefendState&>(qw.Defend()).hp = 1;
    MonsterInstance* raider = nullptr;
    for (EntityId id : wave) {
      MonsterInstance* m = ctx.sys.monsters->Find(id);
      if (m != nullptr && m->IsAlive() && raider == nullptr) raider = m;
    }
    REQUIRE(raider != nullptr);
    raider->pos = qw.Defend().pos;
    for (int64_t guard = 0; guard < 10 * kPtFps && !p.HasStatus(test::ch1::kQuestM5, QuestStatus::Failed); ++guard) {
      p.Frame();
    }
    REQUIRE(p.HasStatus(test::ch1::kQuestM5, QuestStatus::Failed));
    PtKillAll(ctx, wave);
    ctx.sys.hero->SetPosition(home);
    p.Frame();
    p.pilot.SetHold(false);
    REQUIRE(PtPlayTo(p, "siege"));
    INFO(p.pilot.RecentNotes());
    CHECK(p.pilot.Ok());
    CHECK(p.Stats().defendFailures == 1);
    CHECK(p.pilot.RecentNotes().find("defend failed before its step") != std::string::npos);
    CHECK(p.HasStatus(test::ch1::kQuestM5, QuestStatus::Completed));
    CHECK(PtObjCurrent(p.V(), test::ch1::kQuestM5, 0) == 3);
  }

  // The same for S6 (ApEscort::Start): Old Bern joined the hero on an earlier walk and died in a fight there, so the
  // escort step finds the quest Failed. Here, at the checkpoint before the step, Bern stands by the hero at 1 life
  // (test-only) and a chaser chips him; the bot then takes S6 again from the Chaplain and brings Bern home.
  TEST_CASE("autopilot escort: an escort that died before its step is re-taken from its giver and completed") {
    const std::string s6 = "q_rf_hallowed_ground";
    auto sim = GameSim::Create(test::RealData(), SimConfig{});
    REQUIRE(sim != nullptr);
    REQUIRE(sim->NewGame(ClassId::Warrior, Difficulty::Normal, 101));
    PtPlayer p(*sim, ClassId::Warrior, AutoPilotScenario::Chapter1);
    p.ObserveSessionStart();
    REQUIRE(PtPlayTo(p, "corpseherd"));  // S6 open, its escort step next
    REQUIRE(p.HasStatus(s6, QuestStatus::Active));
    p.pilot.SetHold(true);
    SimContext& ctx = sim->Context();
    ctx.sys.story->FinishAllBeats();  // cs_rf_herd (the held bot does not page through it; the world is frozen meanwhile)
    p.Frame();
    REQUIRE_FALSE(p.V().storyBusy);
    const QuestWorld& qw = *ctx.sys.questWorld;
    REQUIRE(qw.Escort().active);
    REQUIRE(qw.Escort().questId == s6);
    EscortState& bern = const_cast<EscortState&>(qw.Escort());
    bern.joined = true;
    bern.hp = 1;
    bern.pos = ctx.sys.hero->Position();
    bern.path.clear();
    MonsterDef chaser = *ctx.data.Monsters().Find("gravegnawer");
    chaser.damage = 1;
    MonsterSpawnParams sp;
    sp.baseDef = &chaser;
    sp.alreadyScaled = true;
    sp.tile = TilePos(JsRoundInt(bern.pos.x), JsRoundInt(bern.pos.y));
    sp.startChasing = true;
    const EntityId m = ctx.sys.monsters->Spawn(sp);
    REQUIRE(m != kNoEntity);
    for (int64_t guard = 0; guard < 10 * kPtFps && !p.HasStatus(s6, QuestStatus::Failed); ++guard) p.Frame();
    REQUIRE(p.HasStatus(s6, QuestStatus::Failed));
    CHECK_FALSE(qw.Escort().active);
    PtKillAll(ctx, {m});
    p.Frame();
    p.pilot.SetHold(false);
    REQUIRE(PtPlayTo(p, "escort"));
    INFO(p.pilot.RecentNotes());
    CHECK(p.pilot.Ok());
    CHECK(p.Stats().escortFailures == 1);
    CHECK(p.pilot.RecentNotes().find("escort failed before its step") != std::string::npos);
    CHECK(p.HasStatus(s6, QuestStatus::Completed));
  }

  // A route step that escorts a quest it never accepted fails at once with the reason (ApEscort::Start keeps the hard
  // fail for a quest that is not open and has not failed).
  TEST_CASE("autopilot escort: an escort step for a quest that was never accepted fails at its start") {
    auto sim = GameSim::Create(test::RealData(), SimConfig{});
    REQUIRE(sim != nullptr);
    REQUIRE(sim->NewGame(ClassId::Warrior, Difficulty::Normal, 7));
    PtPlayer p(*sim, ClassId::Warrior, AutoPilotScenario::None);
    p.ObserveSessionStart();
    p.pilot.AddWaitIdle(120);
    p.pilot.AddEscort("q_rf_hallowed_ground", 3);
    for (int64_t guard = 0; guard < 10 * kPtFps && !p.pilot.Finished(); ++guard) p.pilot.Tick(*sim, kPtFrameMs);
    CHECK(p.pilot.Finished());
    CHECK_FALSE(p.pilot.Ok());
    CHECK(p.pilot.Failure().find("not active") != std::string::npos);
    CHECK(p.Stats().escortFailures == 0);
  }

  // ApLootNearby's potion walk is an optional detour: when its 15 s run out (fights count - here a chaser the hero
  // cannot kill holds it away from the potion), the bot skips the potion and goes on. It used to fail the whole script:
  // mage 8018 (art-lane manifest) gave up on the Gnawed Field hunt at 137.1 s, though the hunt was already done.
  TEST_CASE("autopilot loot: a potion walk that runs out of time is skipped, not a script failure") {
    auto sim = GameSim::Create(test::RealData(), SimConfig{});
    REQUIRE(sim != nullptr);
    REQUIRE(sim->NewGame(ClassId::Warrior, Difficulty::Normal, 7));
    SimContext& ctx = sim->Context();
    ctx.sys.story->FinishAllBeats();
    sim->Step();
    // An open field outside the camps with no monster within 30 tiles; the hero stands in it, a potion 5 tiles east.
    const ZoneRuntime& zone = *ctx.sys.zone;
    const ZoneGrid& g = zone.Grid();
    TilePos field(-1, -1);
    for (int32_t r = 30; r < 100 && field.col < 0; ++r) {
      for (int32_t c = 30; c < 100 && field.col < 0; ++c) {
        bool open = !zone.InSafeZone(Vec2(c, r)) && !zone.InSafeZone(Vec2(c + 5, r));
        for (int32_t dr = -3; dr <= 3 && open; ++dr) {
          for (int32_t dc = -3; dc <= 8 && open; ++dc) open = g.Walkable(c + dc, r + dr);
        }
        if (open) field = TilePos(c, r);
      }
    }
    REQUIRE(field.col >= 0);
    std::vector<EntityId> near;
    for (const MonsterInstance& m : ctx.sys.monsters->All()) {
      if (m.IsAlive() && Dist(m.pos, field.Center()) < 30) near.push_back(m.id);
    }
    for (EntityId id : near) {
      DamageFlags f;
      f.attacker = kHeroEntityId;
      f.source = KillSource::HeroBasic;
      ctx.sys.monsters->ApplyDamage(id, 1e9, f);
    }
    ctx.sys.hero->SetPosition(field.Center());
    const Vec2 potionAt(field.col + 5, field.row);
    REQUIRE(ctx.sys.groundLoot->DropPotion(PotionKind::Hp, 50, potionAt) != kNoEntity);
    sim->Step();
    PtPlayer p(*sim, ClassId::Warrior, AutoPilotScenario::None);
    p.ObserveSessionStart();
    p.pilot.AddLootNearby(7.0);
    p.pilot.AddCheckpoint("looted");
    // The bot sets off for the potion ...
    for (int64_t guard = 0; guard < 5 * kPtFps && !p.V().hero.moving; ++guard) {
      REQUIRE(p.pilot.Tick(*sim, kPtFrameMs) == AutoPilotStatus::NeedFrame);
    }
    REQUIRE(p.V().hero.moving);
    // ... and a chaser it cannot kill comes at it from the other side (test-only def: huge hp, 1 damage).
    MonsterDef chaser = *ctx.data.Monsters().Find("risen_footman");
    chaser.hp = 1e12;
    chaser.damage = 1;
    MonsterSpawnParams sp;
    sp.baseDef = &chaser;
    sp.alreadyScaled = true;
    sp.tile = TilePos(field.col - 2, field.row);
    sp.startChasing = true;
    REQUIRE(ctx.sys.monsters->Spawn(sp) != kNoEntity);
    bool looted = false;
    for (int64_t guard = 0; guard < 120 * kPtFps && !looted; ++guard) {
      const AutoPilotStatus st = p.pilot.Tick(*sim, kPtFrameMs);
      if (st == AutoPilotStatus::Finished) break;
      looted = st == AutoPilotStatus::Checkpoint && p.pilot.CurrentCheckpoint() == "looted";
    }
    INFO(p.pilot.RecentNotes());
    CHECK(p.pilot.Ok());
    CHECK(looted);
    CHECK(p.pilot.RecentNotes().find("gave up: timeout walking to") != std::string::npos);
  }

  // Seeds the wide sweeps (2026-10) found stopping the bot, each in one manifest's clip timings only (the art lane's
  // manifest moves hit timing, so which seed meets a given geometry drifts whenever the manifest changes): mage 7013 froze
  // Old Bern on a wall corner with the committed manifest and mage 1023 with the art lane's of 2026-10-19 (QuestWorld,
  // DECISIONS Q9), mage 8018 timed out a potion detour with the art lane's (ApLootNearby). All three play with whichever
  // data is checked out; they stay as smoke, but the seed pins are not the guard - the three "3.9 escort (Q9)" tests of
  // content_ch1_test.cpp (and the catch-up edge test) pin the escort rule itself, and the potion-walk test here ApLootNearby.
  TEST_CASE("chapter 1 playthrough: sweep regression seeds (mage 7013 + 1023 escort, mage 8018 potion detour)") {
    CHECK(PtRun(ClassId::Mage, 7013, false).ok);
    CHECK(PtRun(ClassId::Mage, 1023, false).ok);
    CHECK(PtRun(ClassId::Mage, 8018, false).ok);
  }

  TEST_CASE("chapter 1 playthrough is deterministic: same seed twice -> identical save JSON") {
    const PtResult a = PtRun(ClassId::Rogue, 404, false);
    const PtResult b = PtRun(ClassId::Rogue, 404, false);
    REQUIRE(a.ok);
    REQUIRE(b.ok);
    CHECK(a.finalSave == b.finalSave);
  }
  // The in-game autotest's scenario and story mode, at 60 fps and at the uneven frame times a software renderer gives
  // (the sim accumulates them; the bot counts frames).
  TEST_CASE("autopilot smoke scenario: every beat skipped, 60 fps") {
    PtRunSmoke(ClassId::Warrior, 505, AutoPilotStoryMode::SkipAll, kPtFrameMs);
  }
  TEST_CASE("autopilot smoke scenario: every beat skipped, 20 fps") {
    PtRunSmoke(ClassId::Mage, 606, AutoPilotStoryMode::SkipAll, 50.0);
  }
  TEST_CASE("autopilot smoke scenario: player-like story, 30 fps") {
    PtRunSmoke(ClassId::Rogue, 707, AutoPilotStoryMode::PlayerLike, 2 * kPtFrameMs);
  }
  TEST_CASE("autopilot smoke scenario: held like the in-game autotest (story beat, quest card)") {
    PtRunSmoke(ClassId::Warrior, 808, AutoPilotStoryMode::SkipAll, kPtFrameMs, true);
  }
}
