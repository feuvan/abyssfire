// Spirit (lingli) and Resonance - the class combat resource (classes-stats-skills.md 14, combat-feel.md 16).
//
// Owner area: hero+combat. Pure state machine; the hero owns one. Gains use the hero's RAW (base + allocated) SPI.
#pragma once

#include <cstdint>

#include "abyss/base/Platform.h"
#include "abyss/data/ClassData.h"

namespace abyss {

enum class SpiritSource : uint8_t { Hit, Kill, Dodge };

struct SpiritSaveState {
  double value = 0;
  double resonanceRemainingMs = 0;
};

struct SpiritGainResult {
  double gained = 0;
  bool resonanceStarted = false;
};

class ABYSS_API Spirit {
 public:
  Spirit(const SpiritTable& table, ClassId cls);

  // gainFromCombat (14.2): (base[source] + (crit ? critBonusGain : 0)) * (1 + clamp(spi, 0, 200) * 0.015).
  SpiritGainResult GainFromCombat(SpiritSource source, double rawSpi, bool isCrit);
  // gain(amount): ignored when not finite, <= 0 or resonating; starts resonance on reaching max.
  SpiritGainResult Gain(double amount);
  // Drains while resonating (only called while the hero is alive). Returns true when resonance ended this call.
  bool Update(double dtMs);
  // Death: value 0, resonance off.
  void Reset();

  bool IsResonating() const { return remainingMs_ > 0 && value_ > 0; }
  double Value() const { return value_; }
  double MaxValue() const { return profile_->maxValue; }
  double ResonanceRemainingMs() const { return remainingMs_; }
  const SpiritProfileDef& Profile() const { return *profile_; }

  // Multipliers while resonating (1 otherwise).
  double DamageMultiplier() const;
  double ManaCostMultiplier() const;
  double MoveSpeedMultiplier() const;

  SpiritSaveState ToSave() const { return {value_, remainingMs_}; }
  // restore (14.4): non-finite -> 0; clamp; either <= 0 -> remaining 0.
  void Restore(const SpiritSaveState& s);

 private:
  const SpiritTable* table_;
  const SpiritProfileDef* profile_;
  double value_ = 0;
  double remainingMs_ = 0;
};

}  // namespace abyss
