// Deterministic RNG (DECISIONS S3): xoshiro128** seeded by SplitMix64, one stream per domain, saveable state.
//
// * Never use <random>, rand() or std::random_device in the core (ue58-platform.md 3.3).
// * Helper semantics match the web (MathUtils.ts): Float01() in [0,1); RandomInt(a,b) = floor(Float01()*(b-a+1))+a
//   (inclusive); Chance(p) = Float01()*100 < p with p in PERCENT; Roll(p) = Float01() < p with p in 0..1.
// * Sequence parity with JS Math.random is not required; draw ORDER and CONDITIONS follow the specs.
// * Tests can script the next Float01() values (Script()) to force "no dodge, crit" etc. (combat-feel.md 24).
// * MapGen uses its own Park-Miller generator (world/MapGen.h) for bit-exact maps; it is not one of these streams.
#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string_view>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"

namespace abyss {

// SplitMix64 (Vigna). Used to expand seeds.
struct SplitMix64 {
  uint64_t state = 0;
  constexpr explicit SplitMix64(uint64_t seed) : state(seed) {}
  constexpr uint64_t Next() {
    uint64_t z = (state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }
};

struct RngState {
  std::array<uint32_t, 4> s{};
  bool operator==(const RngState& o) const = default;
};

// xoshiro128** 1.1 (Blackman & Vigna).
class ABYSS_API Rng {
 public:
  Rng();                          // seeded with 0 (deterministic)
  explicit Rng(uint64_t seed);
  void Seed(uint64_t seed);       // s[0..3] from SplitMix64(seed); never all-zero

  uint32_t NextU32();
  // [0,1) with 53 random bits (two NextU32 draws). Returns the next scripted value instead when one is queued.
  double Float01();
  // JS randomInt(a, b), inclusive. Requires a <= b.
  int32_t RandomInt(int32_t a, int32_t b);
  // floor(Float01() * n) for n > 0 (uniform index; the web's arr[floor(rand*len)]).
  size_t Index(size_t n);
  // JS chance(p): Float01()*100 < percent.
  bool Chance(double percent);
  // Float01() < p01.
  bool Roll(double p01);

  RngState GetState() const;
  void SetState(const RngState& st);

  // ---- test scripting ----
  // Queues values returned by the next Float01() calls (in order) before the generator resumes.
  void Script(std::initializer_list<double> values);
  void Script(std::span<const double> values);
  void ClearScript();
  size_t ScriptedRemaining() const { return script_.size() - scriptPos_; }

 private:
  uint32_t s_[4];
  std::vector<double> script_;
  size_t scriptPos_ = 0;
};

// Gameplay RNG domains (S3). A subsystem receives its stream by reference; never share one stream across domains.
enum class RngStream : uint8_t {
  Combat,  // damage rolls, procs, status chances, dodge, free cast
  Loot,    // drops, item creation, affixes, quest reward choices, shop/craft rolls
  Ai,      // monster patrol/jitter, elite affix rolls, respawn placement
  World,   // zone-entry placement (spawn jitter), rare pet spots, misc world rolls
  Events,  // random events (trigger chance, type, ambush/rescue placement)
  Quests,  // quest drops, garden yields
  Pets,    // pet companion decisions (stray swings), pet crits
};
ABYSS_ENUM_STRINGS(RngStream, "combat", "loot", "ai", "world", "events", "quests", "pets")
inline constexpr size_t kRngStreamCount = EnumCount<RngStream>();

class ABYSS_API RngSet {
 public:
  RngSet();
  // Seeds every stream from one master seed: SplitMix64(master) yields one 64-bit seed per stream in enum order.
  void SeedAll(uint64_t masterSeed);
  Rng& Get(RngStream s) { return streams_[static_cast<size_t>(s)]; }
  const Rng& Get(RngStream s) const { return streams_[static_cast<size_t>(s)]; }
  // Saved as {name: [s0,s1,s2,s3]} in the v4 save (`rng`).
  std::array<RngState, kRngStreamCount> GetStates() const;
  void SetStates(const std::array<RngState, kRngStreamCount>& states);

 private:
  std::array<Rng, kRngStreamCount> streams_;
};

}  // namespace abyss
