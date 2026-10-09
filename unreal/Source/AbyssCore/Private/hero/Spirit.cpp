// Spirit / Resonance (classes-stats-skills.md section 14; web src/systems/SpiritSystem.ts). Owner area: hero.
#include "abyss/base/Platform.h"

#include "abyss/hero/Spirit.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/Math.h"

namespace abyss {

namespace {
// Unknown class -> the table's fallback profile (14.1: warrior), never an out-of-range read.
const SpiritProfileDef& SpiritProfileFor(const SpiritTable& table, ClassId cls) {
  const size_t i = static_cast<size_t>(cls);
  if (i < table.profiles.size()) return table.profiles[i];
  const size_t fb = static_cast<size_t>(table.fallbackClass);
  return table.profiles[fb < table.profiles.size() ? fb : 0];
}
}  // namespace

Spirit::Spirit(const SpiritTable& table, ClassId cls) : table_(&table), profile_(&SpiritProfileFor(table, cls)) {}

SpiritGainResult Spirit::GainFromCombat(SpiritSource source, double rawSpi, bool isCrit) {
  double base = profile_->hitGain;
  if (source == SpiritSource::Kill) {
    base = profile_->killGain;
  } else if (source == SpiritSource::Dodge) {
    base = profile_->dodgeGain;
  }
  const double critGain = isCrit ? profile_->critBonusGain : 0.0;
  // clamp(spi, 0, 200) as MathUtils.clamp: max(min, min(max, v)).
  const double spi = (std::max)(table_->spiClampMin, (std::min)(table_->spiClampMax, rawSpi));
  const double statMultiplier = 1.0 + spi * table_->spiGainFactor;
  return Gain((base + critGain) * statMultiplier);
}

SpiritGainResult Spirit::Gain(double amount) {
  if (!std::isfinite(amount) || amount <= 0 || IsResonating()) return {};
  const double max = profile_->maxValue;
  const double previous = value_;
  value_ = (std::max)(0.0, (std::min)(max, previous + amount));
  SpiritGainResult r;
  r.gained = value_ - previous;
  r.resonanceStarted = previous < max && value_ >= max;
  if (r.resonanceStarted) remainingMs_ = profile_->resonanceDurationMs;
  return r;
}

bool Spirit::Update(double dtMs) {
  if (!IsResonating() || !std::isfinite(dtMs) || dtMs <= 0) return false;
  const double elapsed = (std::min)(dtMs, remainingMs_);
  remainingMs_ = (std::max)(0.0, remainingMs_ - elapsed);
  value_ = (std::max)(0.0, value_ - (profile_->maxValue / profile_->resonanceDurationMs) * elapsed);
  const bool ended = remainingMs_ <= 0 || value_ <= 0;
  if (ended) {
    value_ = 0;
    remainingMs_ = 0;
  }
  return ended;
}

void Spirit::Reset() {
  value_ = 0;
  remainingMs_ = 0;
}

double Spirit::DamageMultiplier() const { return IsResonating() ? 1.0 + profile_->resonanceDamageBonus : 1.0; }

double Spirit::ManaCostMultiplier() const { return IsResonating() ? profile_->resonanceManaCostMultiplier : 1.0; }

double Spirit::MoveSpeedMultiplier() const { return IsResonating() ? 1.0 + profile_->resonanceMoveSpeedBonus : 1.0; }

void Spirit::Restore(const SpiritSaveState& s) {
  const double value = std::isfinite(s.value) ? s.value : 0.0;
  const double remaining = std::isfinite(s.resonanceRemainingMs) ? s.resonanceRemainingMs : 0.0;
  value_ = Clamp(value, 0.0, profile_->maxValue);
  remainingMs_ = Clamp(remaining, 0.0, profile_->resonanceDurationMs);
  if (value_ <= 0 || remainingMs_ <= 0) remainingMs_ = 0;
}

}  // namespace abyss
