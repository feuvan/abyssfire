// Random world events (world-map-nav.md section 13; W6, M4/W9). STUB: owner area world.
#include "abyss/base/Platform.h"

#include "abyss/world/RandomEvents.h"

#include "abyss/base/Assert.h"
#include "abyss/data/DataStore.h"
#include "abyss/sim/Commands.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

void RandomEventRules::Reset(const RandomEventsDef& def, double nowMs) {
  lastEventMs_ = -1e300;
  windowStartMs_ = nowMs;
  eventsInWindow_ = 0;
  moved_ = 0;
  hasLast_ = false;
  lastPos_ = Vec2();
}

bool RandomEventRules::Update(const RandomEventsDef& def, double nowMs, Vec2 heroPos, bool heroInSafeZone, Rng& rng,
                              RandomEventType& outType) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

RandomEventSystem::RandomEventSystem(SimContext& ctx) : ctx_(ctx) {}

void RandomEventSystem::OnZoneEnter() {
  rules_.Reset(ctx_.data.World().randomEvents, ctx_.Now());
  events_.clear();
  puzzle_ = PuzzlePromptState{};
}

void RandomEventSystem::OnZoneExit() {
  ClosePuzzle();
  events_.clear();
}

void RandomEventSystem::Tick() { ABYSS_UNIMPLEMENTED(); }

void RandomEventSystem::OnTimer(const Timer& t) { ABYSS_UNIMPLEMENTED(); }

bool RandomEventSystem::InteractProp(EntityId prop) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

bool RandomEventSystem::OpenPuzzle(EntityId prop) {
  for (const ActiveRandomEvent& e : events_) {
    if (e.prop != prop || e.type != RandomEventType::EnvironmentalPuzzle || e.resolved) continue;
    puzzle_.open = true;
    puzzle_.prop = prop;
    puzzle_.zoneId = ctx_.session.currentMap;
    puzzle_.puzzleIndex = e.puzzleIndex;
    ctx_.events.Emit(EvPuzzlePrompt{true, prop, puzzle_.zoneId, puzzle_.puzzleIndex});
    return true;
  }
  return false;
}

void RandomEventSystem::ClosePuzzle() {
  if (!puzzle_.open) return;
  const PuzzlePromptState was = puzzle_;
  puzzle_ = PuzzlePromptState{};
  ctx_.events.Emit(EvPuzzlePrompt{false, was.prop, was.zoneId, was.puzzleIndex});
}

bool RandomEventSystem::AnswerPuzzle(EntityId prop, int32_t choice) {
  if (!puzzle_.open || puzzle_.prop != prop) return false;
  if (choice == kPuzzleChoiceLeave) {
    ClosePuzzle();  // the event stays unresolved (world 13.3)
    return true;
  }
  if (choice != kPuzzleChoiceSolve) return false;
  ABYSS_UNIMPLEMENTED();  // reward via RewardService (W11), resolve the event, EvRandomEvent{resolved}
  ClosePuzzle();
  return true;
}

void RandomEventSystem::FillSnapshot(Snapshot& out) const {
  out.randomEvents = &events_;
  out.puzzle = &puzzle_;
  for (const ActiveRandomEvent& e : events_) {
    if (e.prop == kNoEntity) continue;
    WorldMarkerView v;
    v.id = e.prop;
    v.kind = MarkerKind::EventProp;
    v.pos = e.pos;
    v.key = std::string(EnumName(e.type));
    out.markers.push_back(std::move(v));
  }
}

}  // namespace abyss
