#include "abyss/base/Platform.h"

#include "abyss/base/Rng.h"

#include <cmath>

namespace abyss {
namespace {

constexpr uint32_t Rotl(uint32_t x, int k) { return (x << k) | (x >> (32 - k)); }

}  // namespace

Rng::Rng() { Seed(0); }

Rng::Rng(uint64_t seed) { Seed(seed); }

void Rng::Seed(uint64_t seed) {
  SplitMix64 sm(seed);
  const uint64_t a = sm.Next();
  const uint64_t b = sm.Next();
  s_[0] = static_cast<uint32_t>(a);
  s_[1] = static_cast<uint32_t>(a >> 32);
  s_[2] = static_cast<uint32_t>(b);
  s_[3] = static_cast<uint32_t>(b >> 32);
  if ((s_[0] | s_[1] | s_[2] | s_[3]) == 0u) s_[0] = 1u;  // the all-zero state is a fixed point
  script_.clear();
  scriptPos_ = 0;
}

uint32_t Rng::NextU32() {
  const uint32_t result = Rotl(s_[1] * 5u, 7) * 9u;
  const uint32_t t = s_[1] << 9;
  s_[2] ^= s_[0];
  s_[3] ^= s_[1];
  s_[1] ^= s_[2];
  s_[0] ^= s_[3];
  s_[2] ^= t;
  s_[3] = Rotl(s_[3], 11);
  return result;
}

double Rng::Float01() {
  if (scriptPos_ < script_.size()) {
    const double v = script_[scriptPos_++];
    if (scriptPos_ == script_.size()) {
      script_.clear();
      scriptPos_ = 0;
    }
    return v;
  }
  // 27 high bits of the first draw and 26 of the second: (a * 2^26 + b) / 2^53, exactly representable.
  const uint32_t a = NextU32() >> 5;
  const uint32_t b = NextU32() >> 6;
  return (static_cast<double>(a) * 67108864.0 + static_cast<double>(b)) * (1.0 / 9007199254740992.0);
}

int32_t Rng::RandomInt(int32_t a, int32_t b) {
  const double span = static_cast<double>(b) - static_cast<double>(a) + 1.0;
  return static_cast<int32_t>(std::floor(Float01() * span) + static_cast<double>(a));
}

size_t Rng::Index(size_t n) {
  if (n == 0) return 0;
  size_t i = static_cast<size_t>(std::floor(Float01() * static_cast<double>(n)));
  return i < n ? i : n - 1;
}

bool Rng::Chance(double percent) { return Float01() * 100.0 < percent; }

bool Rng::Roll(double p01) { return Float01() < p01; }

RngState Rng::GetState() const { return RngState{{s_[0], s_[1], s_[2], s_[3]}}; }

void Rng::SetState(const RngState& st) {
  for (int i = 0; i < 4; ++i) s_[i] = st.s[static_cast<size_t>(i)];
  if ((s_[0] | s_[1] | s_[2] | s_[3]) == 0u) s_[0] = 1u;
}

void Rng::Script(std::initializer_list<double> values) {
  Script(std::span<const double>(values.begin(), values.size()));
}

void Rng::Script(std::span<const double> values) {
  if (scriptPos_ > 0) {
    script_.erase(script_.begin(), script_.begin() + static_cast<std::ptrdiff_t>(scriptPos_));
    scriptPos_ = 0;
  }
  script_.insert(script_.end(), values.begin(), values.end());
}

void Rng::ClearScript() {
  script_.clear();
  scriptPos_ = 0;
}

RngSet::RngSet() { SeedAll(0); }

void RngSet::SeedAll(uint64_t masterSeed) {
  SplitMix64 sm(masterSeed);
  for (Rng& r : streams_) r.Seed(sm.Next());
}

std::array<RngState, kRngStreamCount> RngSet::GetStates() const {
  std::array<RngState, kRngStreamCount> out{};
  for (size_t i = 0; i < kRngStreamCount; ++i) out[i] = streams_[i].GetState();
  return out;
}

void RngSet::SetStates(const std::array<RngState, kRngStreamCount>& states) {
  for (size_t i = 0; i < kRngStreamCount; ++i) streams_[i].SetState(states[i]);
}

}  // namespace abyss
