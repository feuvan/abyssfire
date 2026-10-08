// JS-compatible math helpers (all specs 0 "Rounding", world-map-nav.md 0.1).
//
// * JsRound = Math.round (half toward +inf) = floor(x + 0.5). Never use std::round for a ported Math.round.
// * JsHypot = V8's Math.hypot (Kahan-compensated); MapGen must use it for bit-exact maps.
// * ToInt32 / ToUint32 / Imul / UShr = JS `x | 0`, `x >>> 0`, Math.imul, `>>>`.
// * Double -> int32 conversions are total and identical on every platform: SaturatingInt32 maps NaN to 0 and clamps
//   +-inf / out-of-range values (a raw static_cast is UB there; x64 cvttsd2si gives INT32_MIN, arm64 fcvtzs saturates).
// * Public headers write (std::max)(...) / (std::min)(...) so a <windows.h> min/max macro cannot break them.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>

#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"

namespace abyss {

inline constexpr double kPi = 3.14159265358979323846;
inline constexpr double kTwoPi = 2.0 * kPi;

// Total double -> int32: NaN -> 0, values <= INT32_MIN -> INT32_MIN, values >= INT32_MAX -> INT32_MAX, else truncation.
inline int32_t SaturatingInt32(double x) {
  if (!(x == x)) return 0;  // NaN (written without std::isnan so it survives a mis-configured fast-math TU)
  if (x <= -2147483648.0) return (std::numeric_limits<int32_t>::min)();
  if (x >= 2147483647.0) return (std::numeric_limits<int32_t>::max)();
  return static_cast<int32_t>(x);
}

// JS Math.round.
inline double JsRound(double x) { return std::floor(x + 0.5); }
// JS Math.round converted to an int (saturating, see SaturatingInt32).
inline int32_t JsRoundInt(double x) { return SaturatingInt32(std::floor(x + 0.5)); }
// Math.round(x * 10) / 10 (pet passive rounding, compare deltas).
inline double JsRound1(double x) { return JsRound(x * 10.0) / 10.0; }
inline int32_t FloorInt(double x) { return SaturatingInt32(std::floor(x)); }
inline int32_t CeilInt(double x) { return SaturatingInt32(std::ceil(x)); }

template <class T>
constexpr T Clamp(T v, T lo, T hi) {
  return v < lo ? lo : (hi < v ? hi : v);
}

inline double Lerp(double a, double b, double t) { return a + (b - a) * t; }

inline double DistSq(Vec2 a, Vec2 b) {
  const double dx = a.x - b.x;
  const double dy = a.y - b.y;
  return dx * dx + dy * dy;
}
inline double Dist(Vec2 a, Vec2 b) { return std::sqrt(DistSq(a, b)); }
// Chebyshev (max-norm) and Manhattan distance on integer tiles.
inline int32_t ChebyshevDist(TilePos a, TilePos b) { return (std::max)(std::abs(a.col - b.col), std::abs(a.row - b.row)); }
inline int32_t ManhattanDist(TilePos a, TilePos b) { return std::abs(a.col - b.col) + std::abs(a.row - b.row); }

// V8 Math.hypot(a, b) (world-map-nav.md 0.1 item 3). Use this wherever the web calls Math.hypot.
ABYSS_API double JsHypot(double a, double b);
inline double JsHypot(Vec2 d) { return JsHypot(d.x, d.y); }

// JS ToInt32 / ToUint32 of a finite double (`x | 0`, `x >>> 0`).
ABYSS_API int32_t ToInt32(double x);
ABYSS_API uint32_t ToUint32(double x);
// Math.imul.
constexpr int32_t Imul(int32_t a, int32_t b) {
  return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}
// `x >>> k` for a uint32 value.
constexpr uint32_t UShr(uint32_t x, unsigned k) { return x >> (k & 31u); }

// Frame-rate independent exponential approach factor: 1 - exp(-k * dtSec).
inline double ExpApproachAlpha(double k, double dtSec) { return 1.0 - std::exp(-k * dtSec); }

// Approximate equality for tests and float tolerances.
inline bool NearlyEqual(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }

// Angle helpers (radians).
inline double WrapAngle(double a) {
  a = std::fmod(a + kPi, kTwoPi);
  if (a < 0) a += kTwoPi;
  return a - kPi;
}

}  // namespace abyss
