#include "abyss/base/Platform.h"

#include "abyss/base/SimClock.h"

#include <algorithm>
#include <cmath>

namespace abyss {

void SimClock::SetFrozen(FreezeReason reason, bool on) {
  if (on) {
    freezeMask_ |= FreezeBit(reason);
  } else {
    freezeMask_ &= ~FreezeBit(reason);
  }
  if (freezeMask_ != 0) accMs_ = 0.0;
}

void SimClock::AccumulateRealTime(double realDtMs) {
  if (!(realDtMs > 0.0)) return;
  const double clamped = std::min(realDtMs, maxFrameMs_);
  double scaled = clamped;
  if (dilationRemainingMs_ > 0.0) {
    // The part of this frame inside the dilation window is scaled, the rest runs at full speed.
    const double inside = std::min(clamped, dilationRemainingMs_);
    scaled = inside * dilationScale_ + (clamped - inside);
    dilationRemainingMs_ -= inside;
    if (dilationRemainingMs_ <= 0.0) {
      dilationRemainingMs_ = 0.0;
      dilationScale_ = 1.0;
    }
  }
  if (freezeMask_ != 0) {
    accMs_ = 0.0;
    return;
  }
  accMs_ += scaled;
}

bool SimClock::ConsumeStep() {
  if (freezeMask_ != 0) {
    accMs_ = 0.0;
    return false;
  }
  if (stepsThisFrame_ >= maxStepsPerFrame_) return false;
  if (accMs_ < kSimStepMs) return false;
  accMs_ -= kSimStepMs;
  ++stepsThisFrame_;
  return true;
}

void SimClock::EndFrame() {
  if (stepsThisFrame_ >= maxStepsPerFrame_ && accMs_ >= kSimStepMs) {
    // Spiral-of-death clamp: drop whole steps we could not run this frame.
    accMs_ = std::fmod(accMs_, kSimStepMs);
  }
  stepsThisFrame_ = 0;
}

void SimClock::StartDilation(double scale, double realDurationMs) {
  if (!(realDurationMs > 0.0)) return;
  dilationScale_ = std::clamp(scale, 0.0, 1.0);
  dilationRemainingMs_ = realDurationMs;
}

void SimClock::CancelDilation() {
  dilationScale_ = 1.0;
  dilationRemainingMs_ = 0.0;
}

}  // namespace abyss
