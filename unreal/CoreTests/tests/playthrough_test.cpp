// Headless, seeded, scripted CHAPTER 1 playthrough (ARCHITECTURE 3.3, milestone-1 exit criterion 1).
//
// For each class: new game -> prologue (skipped) -> the camp NPCs (blacksmith, merchant, stash keeper, the elder's
// dialogue tree and quest card) -> every emerald_plains quest (main M1-M5 and the six side quests: kill, drop-collect,
// gather, talk delivery, investigate + hunt, bounty hunt, escort, the pet_sprite clues) plus the zone mini-boss and the
// goblin chief -> the chapter-1 story beats -> save -> load into a fresh GameSim (SaveGame() identical) -> continue to
// the sealed chapter-2 gate.
//
// The player is abyss::AutoPilot (abyss/tools/AutoPilot.h), the same bot the in-game autotest drives (UE
// UAbyssAutoTestSubsystem, -abyssautotest): it plays only through the public GameSim API - Commands in, Events /
// Snapshot out, no debug commands - one frame per Tick. Its Chapter1 script stops at named checkpoints (they cost no
// frame); this test checks the state there.
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
#include <vector>

#include "TestUtil.h"
#include "abyss/base/Math.h"
#include "abyss/base/SimClock.h"
#include "abyss/data/DataStore.h"
#include "abyss/items/Inventory.h"
#include "abyss/items/Shop.h"
#include "abyss/pets/Homestead.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/quests/Achievements.h"
#include "abyss/quests/QuestWorld.h"
#include "abyss/sim/GameSim.h"
#include "abyss/story/StoryDirector.h"
#include "abyss/tools/AutoPilot.h"
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

struct PtResult {
  bool ok = false;
  std::string failure;
  std::string recent;
  std::string finalSave;  // SaveGame(kPtSaveMs) after the finale
};

constexpr int64_t kPtSaveMs = 1760000000000;

const std::vector<std::string> kPtAllCh1Quests = {
    "q_kill_slimes",       "q_kill_goblins",   "q_explore_goblin_camp",    "q_find_goblin_chief",
    "q_secure_plains",     "q_collect_slime_gel", "q_herb_gathering",      "q_lost_pendant",
    "q_bandit_trouble",    "q_pet_sprite_friend", "q_escort_merchant_plains"};

// Plays chapter 1 to the finale (no save/load), checking the state at each checkpoint of the script. False when the
// bot failed or a checkpoint was not reached.
bool PtPlayChapter1(PtPlayer& p) {
  // ---- new game: prologue (skipped) + chapter card ----
  if (!p.To("start")) return false;
  CHECK(p.V().storyBusy);
  if (!p.To("intro")) return false;
  CHECK(PtContains(p.Stats().beatsEnded, "prologue"));
  CHECK(PtContains(p.Stats().beatsEnded, "chapter_emerald_plains"));
  CHECK(p.V().zone.mapId == "emerald_plains");

  // ---- the camp ----
  if (!p.To("blacksmith")) return false;
  CHECK(p.V().shop != nullptr);
  CHECK((p.V().shop != nullptr && p.V().shop->blacksmith));
  if (!p.To("stash")) return false;
  CHECK(p.V().stash != nullptr);
  CHECK((p.V().stash != nullptr && p.V().stash->open));
  if (!p.To("elder_tree")) return false;
  CHECK(p.HasStatus("q_collect_slime_gel", QuestStatus::Active));
  CHECK(p.V().hero.gold == 30);  // reward_ask supplies (Q1, one-time)
  if (!p.To("first_quests")) return false;
  CHECK(p.HasStatus("q_kill_slimes", QuestStatus::Active));
  CHECK(p.HasStatus("q_herb_gathering", QuestStatus::Active));
  CHECK(p.HasStatus("q_pet_sprite_friend", QuestStatus::Active));

  // ---- herbs, slimes, gel ----
  if (!p.To("herbs")) return false;
  CHECK(p.HasStatus("q_herb_gathering", QuestStatus::Completed));
  if (!p.To("slimes")) return false;
  CHECK(p.HasStatus("q_kill_slimes", QuestStatus::Completed));
  CHECK(PtObjCurrent(p.V(), "q_collect_slime_gel", 0) == 6);

  // ---- turn in M1 + herbs (cs_ep_mark), take M2 + the pendant ----
  if (!p.To("elder_m1_turned_in")) return false;
  CHECK(p.HasStatus("q_kill_slimes", QuestStatus::TurnedIn));
  CHECK(p.HasStatus("q_herb_gathering", QuestStatus::TurnedIn));
  if (!p.To("elder_m1")) return false;
  CHECK(PtContains(p.Stats().beatsEnded, "cs_ep_mark"));
  CHECK(p.HasStatus("q_kill_goblins", QuestStatus::Active));
  CHECK(p.HasStatus("q_lost_pendant", QuestStatus::Active));

  // ---- pendant clues 1-2, the herbalist (gel delivery), clue 3, the thief ----
  if (!p.To("herbalist")) return false;
  CHECK(p.HasStatus("q_collect_slime_gel", QuestStatus::Completed));
  if (!p.To("pendant")) return false;
  CHECK(p.HasStatus("q_lost_pendant", QuestStatus::Completed));

  // ---- goblins, the sprite's hide-and-seek ----
  if (!p.To("goblins")) return false;
  CHECK(p.HasStatus("q_kill_goblins", QuestStatus::Completed));
  if (!p.To("sprite")) return false;
  CHECK(p.HasStatus("q_pet_sprite_friend", QuestStatus::Completed));

  // ---- home: gel, M2, pendant, sprite (pet) ----
  if (!p.To("elder_m2_turned_in")) return false;
  CHECK(p.HasStatus("q_collect_slime_gel", QuestStatus::TurnedIn));
  CHECK(p.HasStatus("q_kill_goblins", QuestStatus::TurnedIn));
  CHECK(p.HasStatus("q_lost_pendant", QuestStatus::TurnedIn));
  CHECK(p.HasStatus("q_pet_sprite_friend", QuestStatus::TurnedIn));
  CHECK(p.HasStatus("q_explore_goblin_camp", QuestStatus::Active));
  CHECK(p.HasStatus("q_escort_merchant_plains", QuestStatus::Active));
  if (!p.To("elder_m2")) return false;
  CHECK(p.Stats().petObtained);
  CHECK(p.V().pet.present);
  CHECK(p.V().pet.petId == "pet_sprite");

  // ---- M3: the goblin camp; escort the merchant south ----
  if (!p.To("goblin_camp")) return false;
  CHECK(p.HasStatus("q_explore_goblin_camp", QuestStatus::Completed));
  if (!p.To("escort")) return false;
  CHECK(p.HasStatus("q_escort_merchant_plains", QuestStatus::Completed));

  // ---- the wanderer's bounty ----
  if (!p.To("bounty_accepted")) return false;
  CHECK(p.HasStatus("q_bandit_trouble", QuestStatus::Active));
  if (!p.To("bounty_done")) return false;
  CHECK(p.HasStatus("q_bandit_trouble", QuestStatus::Completed));
  if (!p.To("bounty")) return false;
  CHECK(p.HasStatus("q_bandit_trouble", QuestStatus::TurnedIn));

  // ---- the zone mini-boss: goblin shaman ----
  if (!p.To("miniboss")) return false;
  bool miniBossAlive = false;
  for (const MonsterView& m : p.V().monsters) miniBossAlive = miniBossAlive || (m.miniBoss && m.alive);
  CHECK_FALSE(miniBossAlive);
  CHECK(p.Stats().miniBossDialogues >= 1);

  // ---- home: M3 (cs_ep_whisper), escort; take M4 ----
  if (!p.To("elder_m3_turned_in")) return false;
  CHECK(p.HasStatus("q_explore_goblin_camp", QuestStatus::TurnedIn));
  CHECK(p.HasStatus("q_escort_merchant_plains", QuestStatus::TurnedIn));
  CHECK(p.HasStatus("q_find_goblin_chief", QuestStatus::Active));
  if (!p.To("elder_m3")) return false;
  CHECK(PtContains(p.Stats().beatsEnded, "cs_ep_whisper"));

  // ---- M4: the goblin chief (boss intro) ----
  if (!p.To("chief")) return false;
  CHECK(p.HasStatus("q_find_goblin_chief", QuestStatus::Completed));
  CHECK(PtContains(p.Stats().beatsEnded, "boss_goblin_chief"));

  // ---- home: M4 (weapon / armour choice); take M5 ----
  if (!p.To("elder_m4")) return false;
  CHECK(p.HasStatus("q_find_goblin_chief", QuestStatus::TurnedIn));
  CHECK(p.HasStatus("q_secure_plains", QuestStatus::Active));

  // ---- M5: the seal of veins ----
  if (!p.To("seal")) return false;
  CHECK(p.HasStatus("q_secure_plains", QuestStatus::Completed));
  if (!p.To("elder_m5_turned_in")) return false;
  CHECK(p.HasStatus("q_secure_plains", QuestStatus::TurnedIn));
  if (!p.To("finale")) return false;
  CHECK(PtContains(p.Stats().beatsEnded, "cs_ep_finale"));
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
  for (const char* beat : {"prologue", "chapter_emerald_plains", "cs_ep_mark", "cs_ep_whisper", "boss_goblin_chief",
                           "cs_ep_finale"}) {
    INFO("beat " << beat);
    CHECK(PtContains(st.beatsEnded, beat));
    CHECK(v.story != nullptr);
  }
  // Exp ledger: the level + exp into it account for exactly the exp granted (classes 5.1 curve, 5.2 one level per
  // grant with the overflow kept), and quest rewards pay the chapter's 2660 (quests 10.1 totals).
  int64_t granted = 0;
  for (int64_t x : st.expBySource) granted += x;
  int64_t cumulative = 0;
  for (int32_t l = 1; l < v.hero.level; ++l) cumulative += static_cast<int64_t>(std::floor(3.0 * l * l + 25.0 * l));
  CHECK(cumulative + v.hero.exp == granted);
  CHECK(st.expBySource[static_cast<size_t>(ExpSource::Quest)] == 2660);
  CHECK(st.expBySource[static_cast<size_t>(ExpSource::DialogueReward)] == 50);  // reward_ask, once (Q1)
  CHECK(st.expBySource[static_cast<size_t>(ExpSource::DeathPenalty)] == 0);     // Normal: no exp toll
  // Level band. The chapter's quest rewards alone are 2660 exp (Lv 11 starts at 2530) and the kills every quest needs
  // (10 slimes, 15 goblins, the pendant thief, Gruk, the chief) add >= 589 more, so a complete chapter 1 ends at Lv 12
  // or above with the web's numbers (1-10 is the zone's monster band, not where the chapter ends); a bot that also
  // fights what aggroes on the way lands on 12-14.
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
  const PetInstance* sprite = v.pets->Find("pet_sprite");
  REQUIRE(sprite != nullptr);
  CHECK(v.pets->ActiveId() == "pet_sprite");
  CHECK(sprite->level > 1);  // beast exp from every kill since the turn-in (18.3 onKill)
  // the elite kills the chapter requires (the chief and both hunt leaders) and the zone mini-boss
  for (const char* boss : {"goblin_chief", "miniboss_goblin_shaman", "hunt_pendant_thief", "hunt_redcap_gruk"}) {
    INFO("killed " << boss);
    CHECK(PtContains(st.killedDefs, boss));
  }
  // embers (quests 4.4): 20 from the eleven turn-ins + 5 per elite / mini-boss / affixed kill (>= the four above)
  REQUIRE(v.homestead != nullptr);
  CHECK(v.homestead->Embers() >= 40);
  CHECK((v.homestead->Embers() - 20) % 5 == 0);
  CHECK(v.homestead->IsBuildingUnlocked("herb_garden"));  // q_secure_plains (4.6)
  // achievements a straight chapter-1 run earns (quests 10.5)
  REQUIRE(v.achievements != nullptr);
  for (const char* ach : {"ach_first_kill", "ach_kill_goblin_chief", "ach_quest_10", "ach_level_10"}) {
    INFO("achievement " << ach);
    CHECK(v.achievements->IsUnlocked(ach));
  }
  CHECK(st.deaths <= 2);
  CHECK(st.levelUps >= 11);
  CHECK(st.kills >= 40);
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
  CHECK(fresh->View().zone.mapId == "emerald_plains");
  CHECK(fresh->View().hero.level == p.V().hero.level);

  PtPlayer cont(*fresh, cls, AutoPilotScenario::None);
  cont.ObserveSessionStart();
  cont.Frame();
  cont.pilot.AddWaitIdle(60);
  cont.pilot.AddCheckpoint("loaded");
  cont.pilot.AddGoTo(Vec2(118, 60), 0.4, 600);  // the exit's inner tile (the exit itself sits on the border wall)
  REQUIRE(cont.To("loaded"));
  CHECK(cont.V().quests->IsTurnedIn("q_secure_plains"));
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
  CHECK(cont.V().zone.mapId == "emerald_plains");
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

// The in-game autotest's smoke scenario (UE -abyssautotest=smoke): the first main-quest turn-in, every milestone kind
// but the chapter's end, and nothing after the smoke script's last checkpoint.
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
  CHECK(pilot.CurrentCheckpoint() == "elder_m1");
  const Snapshot& v = sim->View();
  CHECK(PtHasStatus(v, "q_kill_slimes", QuestStatus::TurnedIn));
  CHECK(PtHasStatus(v, "q_herb_gathering", QuestStatus::TurnedIn));
  CHECK(PtHasStatus(v, "q_kill_goblins", QuestStatus::Active));
  CHECK_FALSE(PtHasStatus(v, "q_secure_plains", QuestStatus::Active));
  CHECK(PtContains(pilot.Stats().beatsEnded, "cs_ep_mark"));
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
