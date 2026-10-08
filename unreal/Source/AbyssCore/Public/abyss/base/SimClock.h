// The one gameplay clock (DECISIONS S1, S2, S6; classes-stats-skills.md 19.1 "Clock domains - D13").
//
// * NowMs() counts sim milliseconds as a double, starts at 0 for the session, is never reset on a zone change and
//   never runs backwards. It advances only inside a sim step, by the fixed step kSimStepMs = 1000/60. It is computed
//   as steps * 1000 / 60 (exact at every 3rd step) rather than by repeated addition.
// * While the world is frozen (any freeze reason set) no step runs: timers, AI, regen and statuses hold.
// * Real (presentation) time is fed by the host each frame; the accumulator turns it into 0..maxStepsPerFrame steps
//   (spiral-of-death clamp, S1). Elite-kill slow motion (S6) is a time dilation applied to the accumulated real time,
//   so it scales the sim and the visuals together; its duration is measured in real ms.
#pragma once

#include <cstdint>

#include "abyss/base/Platform.h"

namespace abyss {

inline constexpr double kSimStepMs = 1000.0 / 60.0;

// Why the world is frozen (bit flags). Freeze predicate = any bit set (D13 + S2 + U7).
enum class FreezeReason : uint32_t {
  None = 0,
  Cinematic = 1u << 0,    // story beat playing (StoryDirector::IsCinematic)
  Modal = 1u << 1,        // modal pickers (labyrinth boon/tier pickers; later milestone)
  PauseMenu = 1u << 2,    // system menu (U4)
  QuestDialog = 1u << 3,  // dialogue with a quest card / NPC modal panels (S2, U7)
  TouchPanel = 1u << 4,   // HUD-side panels on touch devices (U7)
  Background = 1u << 5,   // app in background (host)
  Debug = 1u << 6,        // tests / tools
};

constexpr uint32_t FreezeBit(FreezeReason r) { return static_cast<uint32_t>(r); }

class ABYSS_API SimClock {
 public:
  SimClock() = default;

  // ---- sim time ----
  double NowMs() const { return static_cast<double>(steps_) * 1000.0 / 60.0; }
  uint64_t Steps() const { return steps_; }
  // Advances by exactly one step. Only GameSim calls this, and only when not frozen.
  void AdvanceStep() { ++steps_; }
  // Restores the step counter (session restore / tests). Never moves backwards in normal play.
  void SetSteps(uint64_t steps) { steps_ = steps; }

  // ---- freeze ----
  void SetFrozen(FreezeReason reason, bool on);
  bool IsFrozen() const { return freezeMask_ != 0; }
  bool IsFrozenBy(FreezeReason reason) const { return (freezeMask_ & FreezeBit(reason)) != 0; }
  uint32_t FreezeMask() const { return freezeMask_; }

  // ---- real time -> steps (S1) ----
  // Adds one rendered frame's undilated real time. Clamps a single frame to maxFrameMs (resume from background) and
  // applies the current dilation. While frozen the accumulator is discarded (no catch-up after a freeze).
  void AccumulateRealTime(double realDtMs);
  // True if one more step may run this frame (accumulator >= kSimStepMs, budget left, not frozen). Consumes it.
  bool ConsumeStep();
  // Ends the frame: resets the per-frame step budget; if the budget was exhausted the leftover accumulator is dropped
  // to less than one step (spiral-of-death clamp).
  void EndFrame();
  double Accumulator() const { return accMs_; }
  void SetMaxStepsPerFrame(int n) { maxStepsPerFrame_ = n; }
  void SetMaxFrameMs(double ms) { maxFrameMs_ = ms; }

  // ---- time dilation (S6) ----
  // Starts a dilation of `scale` (e.g. 0.4) for `realDurationMs` of real time; a new call replaces the current one.
  void StartDilation(double scale, double realDurationMs);
  void CancelDilation();
  double Dilation() const { return dilationRemainingMs_ > 0.0 ? dilationScale_ : 1.0; }

  // ---- presentation clock ----
  // Real ms fed so far (excludes frames while the app is backgrounded); story/UI timers use this.
  double RealNowMs() const { return realNowMs_; }
  void AdvanceRealClock(double realDtMs) { realNowMs_ += realDtMs; }

 private:
  uint64_t steps_ = 0;
  uint32_t freezeMask_ = 0;
  double accMs_ = 0.0;
  int stepsThisFrame_ = 0;
  int maxStepsPerFrame_ = 4;
  double maxFrameMs_ = 250.0;
  double dilationScale_ = 1.0;
  double dilationRemainingMs_ = 0.0;
  double realNowMs_ = 0.0;
};

}  // namespace abyss
