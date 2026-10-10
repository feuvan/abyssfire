// Random world events: ambush, treasure cache, wandering merchant, rescue, environmental puzzle.
// Spec: world-map-nav.md 13.1 (data), 13.2 (trigger: cooldown, window budget, movement threshold, safe zone), 13.3
// (event creation and scene handling); monsters-ai.md 6.4 (event spawns); loot-items-inventory.md 5.5 (treasure cache);
// DECISIONS W6 (reset the movement counter after every roll; 3-8 events per 5 minutes), M4 / W9 (event monsters never
// respawn), D13 T20 (rescue completion poll on the sim clock). Web: src/systems/RandomEventSystem.ts and the scene glue
// ZoneScene.ts:3193-3718.
//
// Owner area: world. `RandomEventRules` is pure (trigger decision); `RandomEventSystem` is the runtime wrapper.
// RNG: RngStream::Events (trigger roll, type pick, event contents, ambush / rescue placement, cache gold); the cache's
// items come from GenerateLoot on RngStream::Loot.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Rng.h"
#include "abyss/base/Timers.h"
#include "abyss/base/Types.h"
#include "abyss/data/MapData.h"
#include "abyss/sim/SimTypes.h"

namespace abyss {

struct SimContext;
struct Snapshot;

// Shares TimerOwner::World with ZoneRuntime; kinds >= kRandomEventTimerKindBase are routed to RandomEventSystem.
inline constexpr uint16_t kRandomEventTimerKindBase = 100;
enum class RandomEventTimerKind : uint16_t {
  RescuePoll = kRandomEventTimerKindBase,  // 500 ms loop while a rescue is unresolved (T20)
  ChestFade = kRandomEventTimerKindBase + 1,  // the opened cache chest fades after 8 s (+ 1.2 s fade)
};

// The input of one trigger evaluation (13.2): the scene's checkRandomEvents arguments.
struct RandomEventTriggerInput {
  double nowMs = 0;
  double dtMs = 0;
  Vec2 heroPos;
  bool inCombat = false;        // the 1.5 s debounced combat flag
  bool eventPending = false;    // an unresolved event blocks further events
  bool inSafeZone = false;      // camp tile, or within safeZoneRadius of any camp (strict)
};

// Trigger state (13.2) for one zone visit (a fresh zone gets a fresh state: lastEventTime = -infinity).
class ABYSS_API RandomEventRules {
 public:
  void Reset();
  // update() (13.2): accumulates exploration time and Chebyshev movement on every changed position (exact compare;
  // the first call primes the last position), then: moved, not in combat, no unresolved event, not in a safe zone,
  // cooldown elapsed, movement >= threshold, fewer than maxEventsPerWindow in the window -> one roll
  // rand() > chance -> no event (W6: the movement counter resets after every roll when
  // def.resetMoveCounterAfterEveryRoll, else only on a trigger, W9 parity); a trigger draws the type (rand * total
  // weight, subtract in table order, <= 0) and records the time. Returns true with the picked type.
  bool Update(const RandomEventsDef& def, const RandomEventTriggerInput& in, Rng& rng, RandomEventType& outType);
  // calculateTriggerChance (13.2): base 0.07; + 0.05 * p when events in the window < min and p = min(exploration,
  // window) / window > 0.3; x 0.3 when events in the window >= max - 1; clamped to [0.02, 0.25].
  static double TriggerChance(const RandomEventsDef& def, int32_t eventsInWindow, double explorationMs);
  // The weighted type pick of a roll in [0, 1).
  static RandomEventType PickType(const RandomEventsDef& def, double roll01);

  int32_t EventsInWindow(const RandomEventsDef& def, double nowMs) const;
  double LastEventMs() const { return lastEventMs_; }
  void SetLastEventMs(double ms) { lastEventMs_ = ms; }
  double ExplorationMs() const { return explorationMs_; }
  double MovementAccum() const { return moved_; }
  const std::vector<double>& History() const { return history_; }

 private:
  double lastEventMs_ = -1e300;
  std::vector<double> history_;  // trigger times inside the frequency window
  double explorationMs_ = 0;
  double moved_ = 0;
  bool hasLast_ = false;
  Vec2 lastPos_;
};

struct ActiveRandomEvent {
  RandomEventType type = RandomEventType::Ambush;
  Vec2 pos;                                // hero position at trigger time (the event tile for loot drops)
  double triggeredAtMs = 0;
  bool resolved = false;
  EntityId prop = kNoEntity;              // chest / merchant / puzzle prop / rescue NPC
  Vec2 propPos;
  std::string propArt;                    // decor_treasure_chest / npc_wandering_merchant / puzzle / rescue sprite key
  std::vector<EntityId> monsters;          // ambush / rescue spawns
  int32_t monsterCount = 0;                // createEvent count (ambush / rescue)
  int32_t puzzleIndex = -1;                // environmental puzzle: index into ZoneEventDataDef::puzzles
  int64_t rewardGold = 0, rewardExp = 0;   // rescue reward
  TimerId timer = kNoTimer;                // rescue poll / chest fade
};

// The environmental puzzle prompt (world 13.3): a core-owned modal (PanelId::Puzzle). Opened by interacting with an
// unresolved puzzle prop (EvPuzzlePrompt{open}); closed by CmdPuzzleAnswer (either choice), CmdClosePanel{Puzzle}
// (= leave), ExitZone and the hero's death. Text keys: sys.event.puzzle.<zoneId>.{prompt,solution,reward}.
struct PuzzlePromptState {
  bool open = false;
  EntityId prop = kNoEntity;
  std::string zoneId;
  int32_t puzzleIndex = -1;
};

class ABYSS_API RandomEventSystem {
 public:
  explicit RandomEventSystem(SimContext& ctx);

  void OnZoneEnter();
  void OnZoneExit();  // events dropped, puzzle prompt closed
  void Tick();        // trigger roll (hero alive) + the wandering merchant's despawn when its shop closed
  void OnTimer(const Timer& t);
  // Prop interaction (interact action / pointer; ZoneRuntime walks first): merchant opens the shop
  // (ShopSystem::OpenWanderingMerchant), puzzle prop opens the prompt (OpenPuzzle). False when nothing happened.
  bool InteractProp(EntityId prop);
  // Opens the prompt of an unresolved puzzle event (EvPuzzlePrompt{open, prop, zoneId, puzzleIndex}).
  bool OpenPuzzle(EntityId prop);
  // CmdPuzzleAnswer: kPuzzleChoiceSolve (0) -> RewardService::ChangeGold(rewardGold, RandomEvent) +
  // GrantExp(rewardExp, RandomEvent) (W11: the normal addExp path), event resolved (EvRandomEvent{resolved}), prompt
  // closed; kPuzzleChoiceLeave (1) -> prompt closed, the event stays unresolved. Other values / a prop that is not the
  // open prompt's: ignored (false).
  bool AnswerPuzzle(EntityId prop, int32_t choice);
  void ClosePuzzle();  // EvPuzzlePrompt{open = false} when it was open
  const PuzzlePromptState& Puzzle() const { return puzzle_; }

  // Creates and handles an event of `type` at `pos` now (the trigger path after a successful roll; tests force types).
  void TriggerEvent(RandomEventType type, Vec2 pos);
  // Unresolved puzzle / merchant props in interact range (world 7.4 order 7): the nearest unresolved puzzle prop.
  const ActiveRandomEvent* FindByProp(EntityId prop) const;
  bool HasUnresolved() const;

  const std::vector<ActiveRandomEvent>& Active() const { return events_; }
  const RandomEventRules& Rules() const { return rules_; }
  RandomEventRules& MutableRules() { return rules_; }
  void FillSnapshot(Snapshot& out) const;  // markers + Snapshot::randomEvents / puzzle

 private:
  void Resolve(ActiveRandomEvent& e);
  void DespawnEventProp(ActiveRandomEvent& e, DespawnReason reason);
  void SpawnProp(ActiveRandomEvent& e, Vec2 at, const std::string& art);
  void CompleteRescue(ActiveRandomEvent& e);
  void PruneFinished();
  bool HeroInSafeZone(Vec2 p) const;
  TilePos EventTile(Vec2 pos) const;  // findWalkableTile(round(pos)) else round(pos)

  SimContext& ctx_;
  RandomEventRules rules_;
  std::vector<ActiveRandomEvent> events_;
  PuzzlePromptState puzzle_;
};

}  // namespace abyss
