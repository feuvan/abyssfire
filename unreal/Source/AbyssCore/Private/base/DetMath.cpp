// Deterministic transcendental functions (Math.h DetSin / DetCos / DetAtan / DetAtan2 / DetExp / DetLog / DetPow).
//
// The C runtimes the core ships with (MSVC CRT, Apple libm, Android bionic, glibc) are not correctly rounded and
// differ by up to 1 ulp for sin / cos / atan2 / exp / log / pow. Where such a value feeds gameplay state (a position
// rounded to a tile, a cone comparison, a JsRound'ed stat) a 1-ulp difference can flip the result, so the same seed and
// commands could diverge between Mac, Windows, Android and Linux. These implementations use only IEEE-754 basic
// operations (+ - * / sqrt, exact ldexp / frexp / modf / fmod) in a fixed order, so with FP contraction off
// (Platform.h) they return identical bits on every platform.
//
// Algorithms (ported from the Go standard library's pure-Go math package, BSD licence, Copyright 2009 The Go Authors):
//   sin / cos   - Cephes (Stephen L. Moshier) polynomials on [-pi/4, pi/4] after a 3-part Cody-Waite reduction
//                 (Go math/sin.go); arguments >= 2^29 are first reduced with the exact fmod(x, 2*pi as a double),
//                 which is deterministic though not the true reduction (no gameplay angle is that large).
//   atan/atan2  - Cephes rational approximation with the tan(3*pi/8) / 0.66 range split (Go math/atan.go, atan2.go).
//   exp / log   - FreeBSD msun e_exp.c / e_log.c (fdlibm, Sun Microsystems) as in Go math/exp.go, log.go.
//   pow         - Go math/pow.go: exp(yf * log(x)) for the fractional part, binary powering of frexp(x) for the
//                 integral part, ldexp at the end; IEEE 754-2008 special cases.
// Accuracy is within a few ulp of the correctly rounded result (CoreTests base_test compares them with the host libm).
#include "abyss/base/Platform.h"

#include "abyss/base/Math.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace abyss {

namespace detmath_impl {

constexpr double kDmPi = 3.14159265358979323846264338327950288;
constexpr double kDmPiOver2 = 1.57079632679489661923132169163975144;
constexpr double kDmPiOver4 = 0.785398163397448309615660845819875721;
constexpr double kDm3PiOver4 = 2.35619449019234492884698253745962716;
constexpr double kDmFourOverPi = 1.27323954473516268615107010698011489;
constexpr double kDmSqrt2Over2 = 0.707106781186547524400844362104849039;
constexpr double kDmTwoPi = 6.28318530717958647692528676655900576;

// Cephes sin / cos coefficients (Go math/sin.go).
constexpr double kDmSin[6] = {
    1.58962301576546568060e-10,   // 0x3de5d8fd1fd19ccd
    -2.50507477628578072866e-8,   // 0xbe5ae5e5a9291f5d
    2.75573136213857245213e-6,    // 0x3ec71de3567d48a1
    -1.98412698295895385996e-4,   // 0xbf2a01a019bfdf03
    8.33333333332211858878e-3,    // 0x3f8111111110f7d0
    -1.66666666666666307295e-1,   // 0xbfc5555555555548
};
constexpr double kDmCos[6] = {
    -1.13585365213876817300e-11,  // 0xbda8fa49a0861a9b
    2.08757008419747316778e-9,    // 0x3e21ee9d7b4e3f05
    -2.75573141792967388112e-7,   // 0xbe927e4f7eac4bc6
    2.48015872888517045348e-5,    // 0x3efa01a019c844f5
    -1.38888888888730564116e-3,   // 0xbf56c16c16c14f91
    4.16666666666665929218e-2,    // 0x3fa555555555554b
};
// Pi/4 split into three parts (Cody-Waite).
constexpr double kDmPi4A = 7.85398125648498535156e-1;   // 0x3fe921fb40000000
constexpr double kDmPi4B = 3.77489470793079817668e-8;   // 0x3e64442d00000000
constexpr double kDmPi4C = 2.69515142907905952645e-15;  // 0x3ce8469898cc5170
constexpr double kDmReduceThreshold = 536870912.0;      // 2^29

bool DmIsNaN(double x) { return !(x == x); }
bool DmIsInf(double x) {
  return x == std::numeric_limits<double>::infinity() || x == -std::numeric_limits<double>::infinity();
}
double DmNaN() { return std::numeric_limits<double>::quiet_NaN(); }
double DmInf() { return std::numeric_limits<double>::infinity(); }
bool DmSignbit(double x) {
  uint64_t b = 0;
  std::memcpy(&b, &x, sizeof b);
  return (b >> 63) != 0;
}
double DmCopysign(double mag, double sign) {
  uint64_t m = 0, s = 0;
  std::memcpy(&m, &mag, sizeof m);
  std::memcpy(&s, &sign, sizeof s);
  m = (m & ~(uint64_t{1} << 63)) | (s & (uint64_t{1} << 63));
  double out = 0;
  std::memcpy(&out, &m, sizeof out);
  return out;
}

// x >= 0, finite. Octant j (0..7, even) and the reduced argument z in [-pi/4, pi/4] (Go sin.go "map zeros to origin").
void DmReduce(double x, uint64_t& j, double& z) {
  if (x >= kDmReduceThreshold) x = std::fmod(x, kDmTwoPi);  // exact; see the file comment
  j = static_cast<uint64_t>(x * kDmFourOverPi);  // x < 2^29: fits
  double y = static_cast<double>(j);
  if ((j & 1u) == 1u) {
    ++j;
    y += 1.0;
  }
  j &= 7u;
  z = ((x - y * kDmPi4A) - y * kDmPi4B) - y * kDmPi4C;
}

double DmSinPoly(double z, double zz) {
  return z + z * zz * ((((((kDmSin[0] * zz) + kDmSin[1]) * zz + kDmSin[2]) * zz + kDmSin[3]) * zz + kDmSin[4]) * zz +
                       kDmSin[5]);
}
double DmCosPoly(double zz) {
  return 1.0 - 0.5 * zz +
         zz * zz * ((((((kDmCos[0] * zz) + kDmCos[1]) * zz + kDmCos[2]) * zz + kDmCos[3]) * zz + kDmCos[4]) * zz +
                    kDmCos[5]);
}

// Cephes atan (Go math/atan.go).
double DmXatan(double x) {
  constexpr double P0 = -8.750608600031904122785e-01;
  constexpr double P1 = -1.615753718733365076637e+01;
  constexpr double P2 = -7.500855792314704667340e+01;
  constexpr double P3 = -1.228866684490136173410e+02;
  constexpr double P4 = -6.485021904942025371773e+01;
  constexpr double Q0 = +2.485846490142306297962e+01;
  constexpr double Q1 = +1.650270098316988542046e+02;
  constexpr double Q2 = +4.328810604912902668951e+02;
  constexpr double Q3 = +4.853903996359136964868e+02;
  constexpr double Q4 = +1.945506571482613964425e+02;
  double z = x * x;
  z = z * ((((P0 * z + P1) * z + P2) * z + P3) * z + P4) / (((((z + Q0) * z + Q1) * z + Q2) * z + Q3) * z + Q4);
  return x * z + x;
}

// x >= 0.
double DmSatan(double x) {
  constexpr double kMorebits = 6.123233995736765886130e-17;  // pi/2 = PIO2 + Morebits
  constexpr double kTan3Pio8 = 2.41421356237309504880;       // tan(3*pi/8)
  if (x <= 0.66) return DmXatan(x);
  if (x > kTan3Pio8) return kDmPiOver2 - DmXatan(1 / x) + kMorebits;
  return kDmPiOver4 + DmXatan((x - 1) / (x + 1)) + 0.5 * kMorebits;
}

// exp(hi - lo) * 2^k (Go math/exp.go expmulti; fdlibm e_exp.c).
double DmExpMulti(double hi, double lo, int k) {
  constexpr double P1 = 1.66666666666666657415e-01;   // 0x3FC55555; 0x55555555
  constexpr double P2 = -2.77777777770155933842e-03;  // 0xBF66C16C; 0x16BEBD93
  constexpr double P3 = 6.61375632143793436117e-05;   // 0x3F11566A; 0xAF25DE2C
  constexpr double P4 = -1.65339022054652515390e-06;  // 0xBEBBBD41; 0xC5D26BF1
  constexpr double P5 = 4.13813679705723846039e-08;   // 0x3E663769; 0x72BEA4D0
  const double r = hi - lo;
  const double t = r * r;
  const double c = r - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
  const double y = 1 - ((lo - (r * c) / (2 - c)) - hi);
  return std::ldexp(y, k);
}

bool DmIsOddInt(double x) {
  if (std::fabs(x) >= 9007199254740992.0) return false;  // 2^53: every such double is even
  double xi = 0;
  const double xf = std::modf(x, &xi);
  return xf == 0 && (static_cast<int64_t>(xi) & 1) == 1;
}

}  // namespace detmath_impl

double DetSin(double x) {
  using namespace detmath_impl;
  if (x == 0 || DmIsNaN(x)) return x;  // +-0, NaN
  if (DmIsInf(x)) return DmNaN();
  bool sign = false;
  if (x < 0) {
    x = -x;
    sign = true;
  }
  uint64_t j = 0;
  double z = 0;
  DmReduce(x, j, z);
  if (j > 3) {  // reflect in x axis
    sign = !sign;
    j -= 4;
  }
  const double zz = z * z;
  double y = (j == 1 || j == 2) ? DmCosPoly(zz) : DmSinPoly(z, zz);
  if (sign) y = -y;
  return y;
}

double DetCos(double x) {
  using namespace detmath_impl;
  if (DmIsNaN(x) || DmIsInf(x)) return DmNaN();
  bool sign = false;
  x = std::fabs(x);
  uint64_t j = 0;
  double z = 0;
  DmReduce(x, j, z);
  if (j > 3) {
    j -= 4;
    sign = !sign;
  }
  if (j > 1) sign = !sign;
  const double zz = z * z;
  double y = (j == 1 || j == 2) ? DmSinPoly(z, zz) : DmCosPoly(zz);
  if (sign) y = -y;
  return y;
}

double DetAtan(double x) {
  using namespace detmath_impl;
  if (x == 0 || DmIsNaN(x)) return x;
  return x > 0 ? DmSatan(x) : -DmSatan(-x);
}

double DetAtan2(double y, double x) {
  using namespace detmath_impl;
  if (DmIsNaN(y) || DmIsNaN(x)) return DmNaN();
  if (y == 0) {
    if (x >= 0 && !DmSignbit(x)) return DmCopysign(0, y);
    return DmCopysign(kDmPi, y);
  }
  if (x == 0) return DmCopysign(kDmPiOver2, y);
  if (DmIsInf(x)) {
    if (x > 0) return DmIsInf(y) ? DmCopysign(kDmPiOver4, y) : DmCopysign(0, y);
    return DmIsInf(y) ? DmCopysign(kDm3PiOver4, y) : DmCopysign(kDmPi, y);
  }
  if (DmIsInf(y)) return DmCopysign(kDmPiOver2, y);
  const double q = DetAtan(y / x);
  if (x < 0) return q <= 0 ? q + kDmPi : q - kDmPi;
  return q;
}

double DetExp(double x) {
  using namespace detmath_impl;
  constexpr double kLn2Hi = 6.93147180369123816490e-01;
  constexpr double kLn2Lo = 1.90821492927058770002e-10;
  constexpr double kLog2e = 1.44269504088896338700e+00;
  constexpr double kOverflow = 7.09782712893383973096e+02;
  constexpr double kUnderflow = -7.45133219101941108420e+02;
  constexpr double kNearZero = 1.0 / 268435456.0;  // 2^-28
  if (DmIsNaN(x) || x == DmInf()) return x;
  if (x == -DmInf()) return 0;
  if (x > kOverflow) return DmInf();
  if (x < kUnderflow) return 0;
  if (-kNearZero < x && x < kNearZero) return 1 + x;
  int k = 0;
  if (x < 0) {
    k = static_cast<int>(kLog2e * x - 0.5);  // |x| <= 745: fits
  } else if (x > 0) {
    k = static_cast<int>(kLog2e * x + 0.5);
  }
  const double hi = x - static_cast<double>(k) * kLn2Hi;
  const double lo = static_cast<double>(k) * kLn2Lo;
  return DmExpMulti(hi, lo, k);
}

double DetLog(double x) {
  using namespace detmath_impl;
  constexpr double kLn2Hi = 6.93147180369123816490e-01;  // 3fe62e42 fee00000
  constexpr double kLn2Lo = 1.90821492927058770002e-10;  // 3dea39ef 35793c76
  constexpr double L1 = 6.666666666666735130e-01;        // 3FE55555 55555593
  constexpr double L2 = 3.999999999940941908e-01;        // 3FD99999 9997FA04
  constexpr double L3 = 2.857142874366239149e-01;        // 3FD24924 94229359
  constexpr double L4 = 2.222219843214978396e-01;        // 3FCC71C5 1D8E78AF
  constexpr double L5 = 1.818357216161805012e-01;        // 3FC74664 96CB03DE
  constexpr double L6 = 1.531383769920937332e-01;        // 3FC39A09 D078C69F
  constexpr double L7 = 1.479819860511658591e-01;        // 3FC2F112 DF3E5244
  if (DmIsNaN(x) || x == DmInf()) return x;
  if (x < 0) return DmNaN();
  if (x == 0) return -DmInf();
  int ki = 0;
  double f1 = std::frexp(x, &ki);
  if (f1 < kDmSqrt2Over2) {
    f1 *= 2;
    --ki;
  }
  const double f = f1 - 1;
  const double k = static_cast<double>(ki);
  const double s = f / (2 + f);
  const double s2 = s * s;
  const double s4 = s2 * s2;
  const double t1 = s2 * (L1 + s4 * (L3 + s4 * (L5 + s4 * L7)));
  const double t2 = s4 * (L2 + s4 * (L4 + s4 * L6));
  const double R = t1 + t2;
  const double hfsq = 0.5 * f * f;
  return k * kLn2Hi - ((hfsq - (s * (hfsq + R) + k * kLn2Lo)) - f);
}

double DetPow(double x, double y) {
  using namespace detmath_impl;
  if (y == 0 || x == 1) return 1;
  if (y == 1) return x;
  if (DmIsNaN(x) || DmIsNaN(y)) return DmNaN();
  if (x == 0) {
    if (y < 0) return DmSignbit(x) && DmIsOddInt(y) ? -DmInf() : DmInf();
    if (y > 0) return DmSignbit(x) && DmIsOddInt(y) ? x : 0.0;
  }
  if (DmIsInf(y)) {
    if (x == -1) return 1;
    if ((std::fabs(x) < 1) == (y > 0)) return 0;
    return DmInf();
  }
  if (DmIsInf(x)) {
    if (x < 0) return DetPow(1 / x, -y);  // Pow(-0, -y)
    if (y < 0) return 0;
    if (y > 0) return DmInf();
  }
  if (y == 0.5) return std::sqrt(x);
  if (y == -0.5) return 1 / std::sqrt(x);
  double yi = 0;
  double yf = std::modf(std::fabs(y), &yi);
  if (yf != 0 && x < 0) return DmNaN();
  if (yi >= 9223372036854775808.0) {  // 2^63
    if (x == -1) return 1;
    if ((std::fabs(x) < 1) == (y > 0)) return 0;
    return DmInf();
  }
  // ans = a1 * 2^ae (= 1 for now).
  double a1 = 1.0;
  int64_t ae = 0;
  // ans *= x^yf
  if (yf != 0) {
    if (yf > 0.5) {
      yf -= 1;
      yi += 1;
    }
    a1 = DetExp(yf * DetLog(x));
  }
  // ans *= x^yi by repeated squaring (frexp avoids overflow).
  int xeInt = 0;
  double x1 = std::frexp(x, &xeInt);
  int64_t xe = xeInt;
  for (int64_t i = static_cast<int64_t>(yi); i != 0; i >>= 1) {
    if (xe < -(int64_t{1} << 12) || (int64_t{1} << 12) < xe) {
      // catastrophic overflow: the ldexp below saturates to 0 or +-Inf (Go: ae += xe; break)
      ae += xe;
      break;
    }
    if ((i & 1) == 1) {
      a1 *= x1;
      ae += xe;
    }
    x1 *= x1;
    xe <<= 1;
    if (x1 < 0.5) {
      x1 += x1;
      --xe;
    }
  }
  // ans = a1 * 2^ae; if y < 0 { ans = 1 / ans } but careful with overflow.
  if (y < 0) {
    a1 = 1 / a1;
    ae = -ae;
  }
  // ldexp takes an int: clamp (anything beyond +-2^20 saturates the same way).
  const int64_t kAeLim = int64_t{1} << 20;
  const int aeInt = static_cast<int>(ae < -kAeLim ? -kAeLim : (ae > kAeLim ? kAeLim : ae));
  return std::ldexp(a1, aeInt);
}

}  // namespace abyss
