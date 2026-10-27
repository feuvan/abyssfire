// AutoPilot: the Chapter-1 bot (ARCHITECTURE 3.3) shared by the headless playthrough test
// (CoreTests/tests/playthrough_test.cpp) and the in-game autotest (UE UAbyssAutoTestSubsystem, -abyssautotest).
//
// Its script is data: a route of autopilot_routes.json (exported from unreal/GameData/autopilot/*.ts,
// implementation-plan.md 6; DataStore::AutoPilotRoutes) whose steps it interprets one by one - checkpoints, notes and
// tasks (talk, dialogue tree, quest card, shop, gather, hunt by def ids, boss by role, clues, explore, escort, defend,
// walk, town portal, equip). No content id is spelled in the bot (the K1 grep gate, content_hooks_test.cpp).
//
// It plays only through the public contract: it reads the Snapshot and the Events and sends Commands - the commands
// the UE input layer and UI send (CmdMoveTo / CmdAttackTarget / CmdCastSkill / CmdInteract / CmdPickUp /
// CmdTownPortal, shop, quest card and dialogue buttons, potions, stat and skill points, story advance / skip). No debug
// command is used. The bot paths to its goals, fights the nearest threat (attack + hotbar skills), picks up loot it
// wants, equips upgrades, sells spare gear and buys potions, drinks a potion under 35 % HP and spends stat / skill
// points on level-up.
//
// Frame protocol - no blocking loops. Once per rendered frame the owner calls:
//   1. Advance(view)          runs the script until it needs the next frame (NeedFrame). It may instead stop at a
//                             checkpoint (Checkpoint: inspect the state, then call Advance again - a checkpoint costs
//                             no frame) or end (Finished: the scenario is done, or Ok() is false and Failure() says
//                             why).
//   2. PrepareFrame(view)     right before the sim frame: story beats, mini-boss dialogue, lore text, puzzles, stray
//                             panels; remembers the monsters for kill attribution.
//   3. the owner runs one GameSim::Frame
//   4. ObserveEvent(e)        every event of that frame, in emission order (counters, milestones); also the events
//                             NewGame / LoadGame emit before the first frame (zone entry, prologue)
//   5. EndFrame(view, steps)  hero upkeep: potions, stat and skill points.
// Tick(sim, dtMs) does all of it for an owner that drives the GameSim itself (tests, tools); RunFrame(sim, dtMs) is 2-5
// without script logic. `view` is the owner's current Snapshot (GameSim::View(), an object that lives as long as the
// GameSim): ObserveEvent's diary lines read the hero from the last one passed in, nothing else is kept across calls.
// The bot's timers count frames (60 per second of kSimStepMs): at a lower frame rate they stretch in sim time, a
// higher one shortens them (the in-game autotest caps t.MaxFPS at 60; -UseFixedTimeStep -FPS=60 gives the core test's
// exact pacing).
//
// Determinism: the bot uses no clock and no RNG of its own - the same data, seed and frame sequence give the same
// commands (the playthrough test compares final saves of two runs and of the GCC and Clang builds).
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/monsters/Monster.h"
#include "abyss/sim/Commands.h"
#include "abyss/sim/Events.h"
#include "abyss/sim/SimTypes.h"

namespace abyss {

class DataStore;
class GameSim;
struct Snapshot;
struct AutoPilotImpl;

// What the bot plays after the new game.
enum class AutoPilotScenario : uint8_t {
  None,      // nothing: only the steps queued with AddGoTo / AddWait / AddWaitIdle / AddCheckpoint
  Smoke,     // the starting zone's route up to its `smokeEnd` step (Chapter 1: prologue, camp NPCs - shop, stash, the
             // Captain's dialogue tree and quest cards -, gathering, the first hunts, M1 and the first side quest
             // turned in with M1's cutscene)
  Chapter1,  // the whole route of the starting zone: every quest of the zone, its mini-boss, the story boss, the
             // defend siege and the finale
};
ABYSS_ENUM_STRINGS(AutoPilotScenario, "none", "smoke", "chapter")

// How the bot treats story beats.
enum class AutoPilotStoryMode : uint8_t {
  PlayerLike,  // skips the prologue, advances every other beat like a player tapping (the playthrough test)
  SkipAll,     // skips every beat (the in-game autotest)
};
ABYSS_ENUM_STRINGS(AutoPilotStoryMode, "playerLike", "skipAll")

enum class AutoPilotStatus : uint8_t {
  NeedFrame,   // run one frame (PrepareFrame, GameSim::Frame, ObserveEvent..., EndFrame), then Advance again
  Checkpoint,  // the script reached CurrentCheckpoint(); no frame is needed before the next Advance
  Finished,    // the script ended (Ok() true) or failed (Ok() false)
};
ABYSS_ENUM_STRINGS(AutoPilotStatus, "needFrame", "checkpoint", "finished")

// Progress markers recorded while the bot plays (reports, screenshots, test checks).
enum class AutoPilotMilestone : uint8_t {
  ZoneEntered,     // detail: map id
  FirstKill,       // detail: monster def id
  BossKill,        // a hunt leader, mini-boss or story boss killed (by MonsterRole); detail: monster def id
  QuestAccepted,   // detail: quest id
  QuestCompleted,  // detail: quest id
  QuestTurnedIn,   // detail: quest id
  StoryBeat,       // a beat began (prologue, chapter card, cutscene, boss intro); detail: beat id
  Cutscene,        // a cutscene began (cs_*, also counted as StoryBeat); detail: beat id
  LevelUp,         // detail: the new level
  PetObtained,     // detail: pet id
  HeroDied,
  Checkpoint,      // detail: checkpoint id
  ScenarioDone,    // the scenario's script ended; detail: scenario name
  ChapterDone,     // the Chapter1 scenario ended (finale cutscene seen); detail: the route's zone
};
ABYSS_ENUM_STRINGS(AutoPilotMilestone, "zoneEntered", "firstKill", "bossKill", "questAccepted", "questCompleted",
                   "questTurnedIn", "storyBeat", "cutscene", "levelUp", "petObtained", "heroDied", "checkpoint",
                   "scenarioDone", "chapterDone")

struct AutoPilotMilestoneRecord {
  AutoPilotMilestone kind = AutoPilotMilestone::Checkpoint;
  std::string detail;
  int64_t frame = 0;  // bot frame (Frames()) when it happened
  double simMs = 0;   // sim clock
};

// A named fight (dark remake 2F: the sweep reports how long the bosses last): a hunt leader, the zone mini-boss, a
// sub-dungeon mini-boss or the story boss, from the first hit it and the hero trade (either way) to its kill.
struct AutoPilotFight {
  std::string defId;
  MonsterRole role = MonsterRole::Regular;
  EntityId entity = kNoEntity;
  double startMs = 0;  // sim ms of the first hit
  double endMs = -1;   // sim ms of the kill (< 0: not killed)
  int heroDeaths = 0;  // the hero's deaths while it was open
  std::string affixes; // its elite affixes when it began ("+"-joined data spellings; "" for none)
  std::vector<double> phaseMs;  // sim ms of each K4 phase row it entered (a reset clears them)
  int feeds = 0, resets = 0;    // K4 Feeds it started, walks home that reset it
  // 2F review: its abilities, in first-use order: casts started (a strike's or a raise's telegraph, a buff / call
  // resolving) and the started ones cancelled before contact (EvMonsterAttackCancelled: a stun, a freeze, a phase's
  // enter action, F2).
  struct AbilityUse {
    std::string id;
    int started = 0, cancelled = 0;
  };
  std::vector<AbilityUse> abilities;
};

// Counters of what the bot did and saw (all from events and its own commands).
struct AutoPilotStats {
  int deaths = 0, levelUps = 0, kills = 0, potionsDrunk = 0, itemsPicked = 0, equipChanges = 0, saveRequests = 0;
  int sealedBanners = 0, miniBossDialogues = 0, itemsSold = 0, itemsBought = 0, skillCasts = 0;
  int petFeeds = 0, escortFailures = 0, defendFailures = 0, defendRetreats = 0;
  bool petObtained = false;
  std::array<int64_t, EnumCount<ExpSource>()> expBySource{};  // exp granted, indexed by ExpSource
  std::vector<std::string> killedDefs;        // distinct monster defs killed, first-kill order
  std::vector<std::string> beatsBegan, beatsEnded;
  // Dark remake 2F (the W2 fairness rules, monsters.md 1.3): the named fights in the order they began; the largest
  // single hit the hero took (not a DoT tick), as a percent of its maximum life then, and the monster definition that
  // dealt it ("" for a source that is no monster); deaths to one hit (a blow that took the hero from at least half its
  // life to 0).
  std::vector<AutoPilotFight> fights;
  double maxHitTakenPct = 0;
  std::string maxHitTakenBy;
  int oneHitDeaths = 0;
  // 2F review: the most damage the hero took inside any 1000 ms (hits and ticks, before a heal), as a percent of its
  // maximum life (the burst a reaction-limited player cannot dodge out of).
  double maxBurstTakenPct = 0;
  // Dark remake 2F (the W2 bot rules, implementation-plan.md 6): telegraphs the bot stepped out of, and priority targets
  // (a support caster, an aura owner or a pack leader) it switched to.
  int telegraphDodges = 0, priorityTargets = 0;
  // Dark remake 2F: how often the Wave 2 monster systems showed up in the run (evidence that the content runs): bodies
  // stood up (raises, summons, the Unburied rise), blocked hits, flees, support casts (drum, call, muster), corpses
  // eaten, on-death fuses, Ashnumb applications (and the most stacks at once), champion ambushes, world flags set,
  // permanent rewards granted, phase rows entered.
  int raised = 0, selfRises = 0, blocks = 0, flees = 0, supportCasts = 0, corpsesEaten = 0, onDeathFuses = 0;
  int ashnumbHits = 0, ashnumbMaxStacks = 0, championAmbushes = 0, worldFlags = 0, permanentRewards = 0, phases = 0;
};

using AutoPilotSubmit = std::function<void(const Command&)>;

struct AutoPilotConfig {
  ClassId cls = ClassId::Warrior;  // the hero's class (stat allocation, skill priority, gear weights)
  AutoPilotScenario scenario = AutoPilotScenario::Chapter1;
  AutoPilotStoryMode storyMode = AutoPilotStoryMode::PlayerLike;
  // The bot's diary, one line per note ("[class  t s Lv hp (x,y)] ..."); may be empty. The last 80 lines are also kept
  // for RecentNotes().
  std::function<void(const std::string&)> onNote;
  // Every milestone as it is recorded; may be empty.
  std::function<void(const AutoPilotMilestoneRecord&)> onMilestone;
  // 2F review (a player's reflexes, implementation-plan.md 6): a telegraph, hazard or on-death fuse is a danger for the
  // W2 rules only `reactionMs` after it appeared (0: at once, the frame-perfect bot); `dodgeRoll` false never uses the
  // dodge roll's i-frames against a contact the walk cannot outrun. The DR9 sweep runs both the default and the
  // reaction-limited bot (playthrough_test ABYSS_PT_REACTION_MS / ABYSS_PT_NO_DODGE).
  double reactionMs = 0;
  bool dodgeRoll = true;
};

class ABYSS_API AutoPilot {
 public:
  // `data` must outlive the bot. `submit` receives every command (GameSim::Submit in tests, the player controller's
  // SubmitCommand in UE).
  AutoPilot(const DataStore& data, AutoPilotConfig cfg, AutoPilotSubmit submit);
  ~AutoPilot();
  AutoPilot(const AutoPilot&) = delete;
  AutoPilot& operator=(const AutoPilot&) = delete;

  // ---- extra steps, run after the scenario's (tests / tools) ----
  void AddCheckpoint(const std::string& id);
  void AddWaitIdle(double timeoutSec);  // until no story beat and no core modal (closes stray modals); fails on timeout
  void AddWait(double seconds);
  void AddGoTo(Vec2 goal, double arrive, double timeoutSec);  // walks there, fighting on the way; fails when stuck
  // The hunt's loot pass (ApLootNearby): picks up wanted items and walks over potions within `radius` tiles; a potion
  // walk that times out is skipped, not a failure.
  void AddLootNearby(double radius);
  // The route's `escort` step: lead the quest's escort NPC home (ApEscort), re-taken from its giver up to `tries` times
  // (an escort that died before the step is one lost attempt); a quest that was never accepted fails the step at once.
  void AddEscort(const std::string& questId, int tries);
  // The route's `defend` step: hold the quest's defend target (ApDefend), re-taken from its giver up to `tries` times
  // (a target that fell before the step is one lost attempt); a quest that was never accepted fails the step at once.
  void AddDefend(const std::string& questId, int tries, double standOff);

  // ---- frame protocol (see the file comment) ----
  AutoPilotStatus Advance(const Snapshot& view);
  void PrepareFrame(const Snapshot& view);
  void ObserveEvent(const Event& e);
  void EndFrame(const Snapshot& view, int32_t steps);
  AutoPilotStatus Tick(GameSim& sim, double dtMs);
  void RunFrame(GameSim& sim, double dtMs);
  // While held the bot sends nothing (no script, story, panel or hero commands) but keeps counting frames and events -
  // e.g. while UE photographs a cutscene or tours the HUD panels.
  void SetHold(bool hold);
  bool Held() const;

  // ---- state ----
  bool Ok() const;
  const std::string& Failure() const;          // "" while Ok()
  bool Finished() const;                       // the script ended or failed
  const std::string& CurrentCheckpoint() const;  // the last checkpoint reached ("" before the first)
  const std::string& CurrentStep() const;        // label of the script step being played
  std::string RecentNotes() const;               // the last 80 notes, one per line
  const AutoPilotStats& Stats() const;
  const std::vector<AutoPilotMilestoneRecord>& Milestones() const;
  bool Reached(AutoPilotMilestone kind) const;
  int64_t Frames() const;
  const AutoPilotConfig& Config() const;

 private:
  std::unique_ptr<AutoPilotImpl> impl_;
};

}  // namespace abyss
