#include "abyss/base/Platform.h"

#include "abyss/base/Math.h"

#include <cmath>

namespace abyss {

double JsHypot(double a, double b) {
  // V8 Math.hypot for two arguments (world-map-nav.md 0.1 item 3): Kahan-compensated sum of squares of the inputs
  // scaled by the maximum. Infinity wins over NaN as in the ECMAScript spec.
  const double x0 = std::fabs(a);
  const double x1 = std::fabs(b);
  if (std::isinf(x0) || std::isinf(x1)) return INFINITY;
  if (std::isnan(x0) || std::isnan(x1)) return NAN;
  const double mx = x0 > x1 ? x0 : x1;
  if (mx == 0.0) return 0.0;
  double sum = 0.0;
  double comp = 0.0;
  const double xs[2] = {x0, x1};
  for (double v : xs) {
    const double n = v / mx;
    const double s = n * n - comp;
    const double p = sum + s;
    comp = (p - sum) - s;
    sum = p;
  }
  return std::sqrt(sum) * mx;
}

uint32_t ToUint32(double x) {
  if (!std::isfinite(x)) return 0u;
  const double t = std::trunc(x);
  double m = std::fmod(t, 4294967296.0);
  if (m < 0) m += 4294967296.0;
  return static_cast<uint32_t>(m);
}

int32_t ToInt32(double x) { return static_cast<int32_t>(ToUint32(x)); }

}  // namespace abyss
