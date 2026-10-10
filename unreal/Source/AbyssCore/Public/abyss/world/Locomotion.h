// Hero locomotion: path following, direct (keyboard / stick / joystick) movement and hold-to-move.
// Spec: world-map-nav.md 6.1-6.5 (state, path following, direct movement, speed model, facing), 7.2 (hold-to-move,
// 120 ms re-path, 0.6 tile stand-still radius, findWalkableNear 3), 17 (tick order); save-ui-input.md 5.2 (movement from
// input, ScreenDirToTile); DECISIONS S5 (uniform ground speed moveSpeed / 36 tiles/s for every input mode; keyboard /
// stick 90 ms ramp to full speed and 60 ms stop; click-move starts instantly), C5 (hero immobilize / slow affect
// movement), C7 (click-to-attack stops at range), W3 (movement input cancels the town portal: HeroMoveInputMsg),
// D13 F1 (hold-move cleared at freeze begin; cinematic also clears the path).
//
// Owner area: world. Runtime system (SimContext). Writes Hero::SetPosition / SetFacing. Also owns the C4 Charge dash
// (timed hero move) and the moving-target approach (C7 / auto-battle).
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/sim/SimTypes.h"

namespace abyss {

struct SimContext;
struct Snapshot;

// (sx + sy, -sx + sy): screen direction (x right, y down) to tile direction (save-ui-input 5.2), not normalised.
ABYSS_API Vec2 ScreenDirToTile(Vec2 screen);

// world 6.4: UniformTiles = S5 (default; moveSpeed / 36 tiles/s for every input mode, click-move instant, keyboard /
// stick 90 ms ramp and 60 ms stop, axis-separated wall sliding W3 FIX); IsoPixelParity = the web verbatim (6.2 / 6.3:
// iso-px path speed with first-order acceleration 8/s and decay 12/s, leftover step discarded at a node, keyboard
// moveSpeed * 0.015 tiles per second with no sliding) for parity tests.
enum class HeroSpeedModel : uint8_t { UniformTiles, IsoPixelParity };

struct HoldMoveState {
  bool active = false;
  int32_t pointerId = 0;
  TilePos tile;     // tile of the last re-path (hold.col / hold.row)
  TilePos pointer;  // latest picked tile from the input layer (re-projected every frame)
  double repathAtMs = 0;
};

// Following a moving target (C7 click-to-attack approach, auto-battle "path to it", pet-less melee chase): the goal is
// re-pathed to the target's current position on the hold-move cadence (holdMoveRepathMs, 120 ms).
struct ApproachState {
  bool active = false;
  EntityId target = kNoEntity;
  double stopRange = 0;
  double repathAtMs = 0;
};

// C4 Charge dash: a timed straight move (sim clock) that owns the hero position while it runs.
struct DashState {
  bool active = false;
  Vec2 from, to;
  double startMs = 0;
  double durationMs = 0;
  std::string skillId;
};

class ABYSS_API HeroLocomotion {
 public:
  explicit HeroLocomotion(SimContext& ctx);

  // Path to a tile (click-to-move). `stopRange` > 0 stops the walk once within that range of `goal` (C7). Cancels an
  // approach (a new explicit goal) but not a dash.
  bool MoveTo(Vec2 goal, double stopRange = 0);
  // Walks toward a moving monster: re-goals to its current position every holdMoveRepathMs (MoveTo(pos, stopRange)) and
  // ends when the hero is within stopRange of it (path cleared, the caller's attack takes over) or the target is gone /
  // dead. Cleared by MoveTo, direct input, hold-move, Stop, Teleport, zone entry and a cinematic freeze (F1, like the
  // path; a modal freeze keeps it, F3).
  bool Approach(EntityId target, double stopRange);
  void CancelApproach();
  bool IsApproaching() const { return approach_.active; }
  const ApproachState& Approaching() const { return approach_; }
  // C4 Charge dash: moves the hero in a straight line from its position to `to` over durationMs of sim time
  // (Lerp each step), suppressing path following, hold-move and direct input while it runs; path / approach are cleared
  // at the start. It stops where the hero stands when the hero becomes immobilized (stun / freeze, C5), and is kept
  // across a modal freeze (F3: the sim clock simply holds). Emits EvHeroDash{Started}, then {Arrived} or {Interrupted}.
  // CombatSystem resolves the charge hit on its ChargeDashEnd timer (scheduled for startMs + durationMs).
  // Returns false (nothing starts) for a non-positive duration or while a dash is already running.
  bool StartDash(Vec2 to, double durationMs, std::string_view skillId);
  bool IsDashing() const { return dash_.active; }
  const DashState& Dash() const { return dash_; }
  void SetPath(std::vector<TilePos> path);
  void Stop();  // clears path, hold-move, approach and direct input (a running dash finishes)
  // Direct input direction in tile space (keyboard / stick / joystick, already ScreenDirToTile-mapped); zero = none.
  void SetMoveInput(Vec2 tileDir);
  // Hold-to-move (7.2): started by the press chain's ground row (7.1 row 12, repathAt = now + 120); the input layer
  // re-sends the picked tile every frame while the pointer is down (UpdateHold), once with down = false.
  void BeginHold(int32_t pointerId, TilePos tile);
  void UpdateHold(int32_t pointerId, TilePos tile, bool down);
  bool IsHoldMoving() const { return hold_.active; }
  const HoldMoveState& Hold() const { return hold_; }
  void SetSpeedModel(HeroSpeedModel m) { model_ = m; }
  HeroSpeedModel SpeedModel() const { return model_; }
  // Bumped by every explicit movement order (MoveTo, direct input, hold re-path, Stop, Teleport, dash, approach start):
  // ZoneRuntime's walk-then-act interaction (W8) is abandoned when it changes.
  uint32_t MoveGeneration() const { return generation_; }

  // Per step (classes 16 step 7): a running dash first (owns the position), then direct input (clears hold + path +
  // approach), then hold re-path, then approach re-goal, then path following.
  void Tick(double dtMs);
  void OnFreezeBegin(bool cinematic);
  void OnZoneEnter();
  void Teleport(Vec2 to, TeleportReason reason);  // moveTo: snaps, clears path

  bool IsMoving() const { return moving_; }
  const std::vector<TilePos>& Path() const { return path_; }
  double CurrentSpeedTilesPerSec() const { return speed_; }
  Vec2 LastMoveDirection() const { return lastDir_; }
  void FillSnapshot(Snapshot& out) const;

 private:
  // Advances a running dash by one step; returns true while the dash owns the hero this step.
  bool TickDash();
  // Re-goals a running approach (or ends it: in range / target gone).
  void TickApproach();
  // Path request shared by MoveTo (which also cancels an approach) and the approach re-goal. An unwalkable goal tile
  // heads for findWalkableNear(goal, 3) (hold-to-move rule 7.2).
  bool PathTo(Vec2 goal, double stopRange);
  // Direct (keyboard / stick) movement for one step; returns true when it owned the hero this step.
  bool TickDirect(double dtMs, double speedMul);
  void TickHold();
  void TickPath(double dtMs, double speedMul);
  // Moves by `delta` with the W3 axis-separated retry (UniformTiles) or the destination-only rule (parity).
  bool TryDirectStep(Vec2 delta, bool slide);
  void FaceAlongPath();

  SimContext& ctx_;
  HeroSpeedModel model_ = HeroSpeedModel::UniformTiles;
  std::vector<TilePos> path_;
  bool moving_ = false;
  double speed_ = 0;        // actual ground speed of the last step (tiles/s; W2: drive the locomotion blend from it)
  double directSpeed_ = 0;  // UniformTiles keyboard / stick speed (ramps 90 ms up, 60 ms down)
  double pathSpeedPx_ = 0;  // IsoPixelParity currentSpeed (iso px/s)
  double directMoveMs_ = 0; // IsoPixelParity keyboard grace (120 ms)
  uint32_t generation_ = 0;
  Vec2 input_;           // current direct input (tile space)
  Vec2 lastDir_{1, -1};  // web initial (screen right); dodge falls back to facing (C8)
  HoldMoveState hold_;
  ApproachState approach_;
  DashState dash_;
  double stopRange_ = 0;
  Vec2 goal_;
};

}  // namespace abyss
