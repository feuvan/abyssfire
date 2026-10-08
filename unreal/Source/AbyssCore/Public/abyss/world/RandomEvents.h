// Random world events: ambush, treasure cache, wandering merchant, rescue, environmental puzzle.
// Spec: world-map-nav.md 13.1 (data), 13.2 (trigger: cooldown, window budget, movement threshold, safe zone), 13.3
// (event creation and scene handling); monsters-ai.md 6.4 (event spawns); loot-items-inventory.md 5.5 (treasure cache);
// DECISIONS W6 (reset the movement counter after every roll; 3-8 events per 5 minutes), M4 / W9 (event monsters never
// respawn), D13 T20 (rescue completion poll on the sim clock).
//
// Owner area: world. `RandomEventRules` is pure (trigger decision); `RandomEventSystem` is the runtime wrapper.
// RNG: RngStream::Events.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Rng.h"
#include "abyss/base/Timers.h"
#include "abyss/base/Types.h"
#include "abyss/data/MapData.h"

namespace abyss {

struct SimContext;
struct Snapshot;

// Shares TimerOwner::World with ZoneRuntime; kinds >= kRandomEventTimerKindBase are routed to RandomEventSystem.
inline constexpr uint16_t kRandomEventTimerKindBase = 100;
enum class RandomEventTimerKind : uint16_t { RescuePoll = kRandomEventTimerKindBase };

// Trigger state (13.2) for one zone visit.
class ABYSS_API RandomEventRules {
 public:
  void Reset(const RandomEventsDef& def, double nowMs);
  // Called every step while the hero is alive: accumulates movement; when the threshold is passed and the cooldown /
  // window budget allow it, rolls once (W6: the counter resets after every roll). Returns true with the picked type.
  bool Update(const RandomEventsDef& def, double nowMs, Vec2 heroPos, bool heroInSafeZone, Rng& rng,
              RandomEventType& outType);

 private:
  double lastEventMs_ = -1e300;
  double windowStartMs_ = 0;
  int32_t eventsInWindow_ = 0;
  double moved_ = 0;
  bool hasLast_ = false;
  Vec2 lastPos_;
};

struct ActiveRandomEvent {
  RandomEventType type = RandomEventType::Ambush;
  Vec2 pos;
  bool resolved = false;
  EntityId prop = kNoEntity;              // chest / merchant / puzzle prop / rescue NPC
  std::vector<EntityId> monsters;          // ambush / rescue spawns
  int32_t puzzleIndex = -1;                // environmental puzzle: index into ZoneEventDataDef::puzzles
};

// The environmental puzzle prompt (world 13.3): a core-owned modal (PanelId::Puzzle). Opened by interacting with an
// unresolved puzzle prop (EvPuzzlePrompt{open}); closed by CmdPuzzleAnswer (either choice), CmdClosePanel{Puzzle}
// (= leave), ExitZone and the hero's death. Text keys: sys.event.puzzle.<zoneId>.<puzzleIndex>.{prompt,solution,reward}.
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
  void Tick();        // trigger roll + per-event updates (ambush auto-resolve, rescue completion)
  void OnTimer(const Timer& t);
  // Prop interaction (interact action / pointer; ZoneRuntime walks first): treasure cache opens, merchant opens the
  // shop (ShopSystem::OpenWanderingMerchant), puzzle prop opens the prompt (OpenPuzzle). False when nothing happened.
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

  const std::vector<ActiveRandomEvent>& Active() const { return events_; }
  void FillSnapshot(Snapshot& out) const;  // markers + Snapshot::randomEvents / puzzle

 private:
  SimContext& ctx_;
  RandomEventRules rules_;
  std::vector<ActiveRandomEvent> events_;
  PuzzlePromptState puzzle_;
};

}  // namespace abyss
