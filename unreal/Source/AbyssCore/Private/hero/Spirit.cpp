// Spirit / Resonance (classes-stats-skills.md section 14). STUB: owner area hero+combat.
#include "abyss/base/Platform.h"

#include "abyss/hero/Spirit.h"

#include "abyss/base/Assert.h"

namespace abyss {

Spirit::Spirit(const SpiritTable& table, ClassId cls) : table_(&table), profile_(&table.For(cls)) {}

SpiritGainResult Spirit::GainFromCombat(SpiritSource source, double rawSpi, bool isCrit) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

SpiritGainResult Spirit::Gain(double amount) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

bool Spirit::Update(double dtMs) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

void Spirit::Reset() {
  value_ = 0;
  remainingMs_ = 0;
}

double Spirit::DamageMultiplier() const {
  ABYSS_UNIMPLEMENTED();
  return 1.0;
}

double Spirit::ManaCostMultiplier() const {
  ABYSS_UNIMPLEMENTED();
  return 1.0;
}

double Spirit::MoveSpeedMultiplier() const {
  ABYSS_UNIMPLEMENTED();
  return 1.0;
}

void Spirit::Restore(const SpiritSaveState& s) {
  ABYSS_UNIMPLEMENTED();
  (void)table_;
}

}  // namespace abyss
