// Stub SDK. Real headers: Engine/Source/Runtime/Core/Public/Math/UnrealMathUtility.h and
// GenericPlatform/GenericPlatformMath.h (FMath derives from FPlatformMath). From UE 5.x source knowledge.
//
// The overload shapes are the engine's, because they decide what compiles:
//   * one-argument float functions (Sin, Cos, Sqrt, Exp, Floor*, ...) exist for float and double only: an int argument
//     is an ambiguous call;
//   * FloorToInt / CeilToInt / RoundToInt / TruncToInt return int32 for float and int64 for double (use the *32 forms);
//   * Min / Max / Clamp are single-type templates; mixed arguments compile only through the engine's mixing overloads
//     (a floating-point type with anything arithmetic, or two different SIGNED integer types). int32 with uint32 / uint8
//     / size_t does not compile.
//   * every value-returning function is [[nodiscard]], as in the engine (UE_NODISCARD since 5.1): UE builds with -Werror,
//     so `FMath::Max(A, B);` as a statement fails. That every one carries the attribute is from memory (the strict side).
#pragma once

#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/NumericLimits.h"
#include "Containers/ContainersFwd.h"
#include "Templates/UnrealTypeTraits.h"

#undef PI
#define UE_PI (3.1415926535897932f)
#define UE_SMALL_NUMBER (1.e-8f)
#define UE_KINDA_SMALL_NUMBER (1.e-4f)
#define UE_BIG_NUMBER (3.4e+38f)
#define UE_EULERS_NUMBER (2.71828182845904523536f)
#define UE_GOLDEN_RATIO (1.6180339887498948482045868343656381f)
#define UE_FLOAT_NON_FRACTIONAL (8388608.f)
#define UE_DOUBLE_PI (3.141592653589793238462643383279502884197169399)
#define UE_DOUBLE_SMALL_NUMBER (1.e-8)
#define UE_DOUBLE_KINDA_SMALL_NUMBER (1.e-4)
#define UE_DOUBLE_BIG_NUMBER (3.4e+38)
#define UE_DOUBLE_EULERS_NUMBER (2.7182818284590452353602874713526624977572)
#define UE_MAX_FLT (3.402823466e+38F)
#define UE_INV_PI (0.31830988618f)
#define UE_HALF_PI (1.57079632679f)
#define UE_TWO_PI (6.28318530717f)
#define UE_PI_SQUARED (9.86960440108f)
#define UE_DOUBLE_INV_PI (0.31830988618379067154)
#define UE_DOUBLE_HALF_PI (1.57079632679489661923)
#define UE_DOUBLE_TWO_PI (6.28318530717958647692)
#define UE_DOUBLE_PI_SQUARED (9.86960440108935861883)
#define UE_SQRT_2 (1.4142135623730950488016887242097f)
#define UE_SQRT_3 (1.7320508075688772935274463415059f)
#define UE_INV_SQRT_2 (0.70710678118654752440084436210485f)
#define UE_HALF_SQRT_2 (0.70710678118654752440084436210485f)
#define UE_DOUBLE_SQRT_2 (1.4142135623730950488016887242097)
#define UE_DELTA (0.00001f)
#define UE_DOUBLE_DELTA (0.00001)
#define UE_FLOAT_NORMAL_THRESH (0.0001f)
#define UE_THRESH_POINT_ON_PLANE (0.10f)
#define UE_THRESH_POINTS_ARE_SAME (0.00002f)
#define UE_THRESH_POINTS_ARE_NEAR (0.015f)
#define UE_THRESH_NORMALS_ARE_SAME (0.00002f)
#define UE_THRESH_VECTORS_ARE_NEAR (0.0004f)
#define UE_THRESH_VECTOR_NORMALIZED (0.01f)
#define UE_THRESH_QUAT_NORMALIZED (0.01f)
// Legacy names (UE_DEFINE_LEGACY_MATH_CONSTANT_MACRO_NAMES defaults to 1): any identifier spelled like these is replaced.
#define PI UE_PI
#define SMALL_NUMBER UE_SMALL_NUMBER
#define KINDA_SMALL_NUMBER UE_KINDA_SMALL_NUMBER
#define BIG_NUMBER UE_BIG_NUMBER
#define EULERS_NUMBER UE_EULERS_NUMBER
#define FLOAT_NON_FRACTIONAL UE_FLOAT_NON_FRACTIONAL
#define DOUBLE_PI UE_DOUBLE_PI
#define DOUBLE_SMALL_NUMBER UE_DOUBLE_SMALL_NUMBER
#define DOUBLE_KINDA_SMALL_NUMBER UE_DOUBLE_KINDA_SMALL_NUMBER
#define DOUBLE_BIG_NUMBER UE_DOUBLE_BIG_NUMBER
#define MAX_FLT UE_MAX_FLT
#define INV_PI UE_INV_PI
#define HALF_PI UE_HALF_PI
#define TWO_PI UE_TWO_PI
#define PI_SQUARED UE_PI_SQUARED
#define DELTA UE_DELTA
#define FLOAT_NORMAL_THRESH UE_FLOAT_NORMAL_THRESH
#define THRESH_POINT_ON_PLANE UE_THRESH_POINT_ON_PLANE
#define THRESH_POINTS_ARE_SAME UE_THRESH_POINTS_ARE_SAME
#define THRESH_POINTS_ARE_NEAR UE_THRESH_POINTS_ARE_NEAR
#define THRESH_NORMALS_ARE_SAME UE_THRESH_NORMALS_ARE_SAME
#define THRESH_VECTORS_ARE_NEAR UE_THRESH_VECTORS_ARE_NEAR
#define THRESH_VECTOR_NORMALIZED UE_THRESH_VECTOR_NORMALIZED
#define THRESH_QUAT_NORMALIZED UE_THRESH_QUAT_NORMALIZED

#define UE_STUB_MIX_FLOATS_2_ARGS(Func) \
	template <typename Arg1, typename Arg2> \
		requires (std::is_floating_point_v<Arg1> || std::is_floating_point_v<Arg2>) && std::is_arithmetic_v<Arg1> && std::is_arithmetic_v<Arg2> && (!std::is_same_v<Arg1, Arg2>) \
	[[nodiscard]] static constexpr auto Func(Arg1 X, Arg2 Y) -> decltype(X * Y) { using ArgType = decltype(X * Y); return Func((ArgType)X, (ArgType)Y); }
#define UE_STUB_MIX_FLOATS_3_ARGS(Func) \
	template <typename Arg1, typename Arg2, typename Arg3> \
		requires (std::is_floating_point_v<Arg1> || std::is_floating_point_v<Arg2> || std::is_floating_point_v<Arg3>) \
			&& std::is_arithmetic_v<Arg1> && std::is_arithmetic_v<Arg2> && std::is_arithmetic_v<Arg3> \
			&& (!std::is_same_v<Arg1, Arg2> || !std::is_same_v<Arg2, Arg3> || !std::is_same_v<Arg1, Arg3>) \
	[[nodiscard]] static constexpr auto Func(Arg1 X, Arg2 Y, Arg3 Z) -> decltype(X * Y * Z) { using ArgType = decltype(X * Y * Z); return Func((ArgType)X, (ArgType)Y, (ArgType)Z); }
#define UE_STUB_MIX_SIGNED_INTS_2_ARGS(Func) \
	template <typename Arg1, typename Arg2> \
		requires std::is_signed_v<Arg1> && std::is_integral_v<Arg1> && std::is_signed_v<Arg2> && std::is_integral_v<Arg2> && (!std::is_same_v<Arg1, Arg2>) \
	[[nodiscard]] static constexpr auto Func(Arg1 X, Arg2 Y) -> decltype(X * Y) { using ArgType = decltype(X * Y); return Func((ArgType)X, (ArgType)Y); }
#define UE_STUB_RESOLVE_FLOAT_AMBIGUITY_2_ARGS(Func) \
	template <typename Arg1, typename Arg2> \
		requires std::is_arithmetic_v<Arg1> && std::is_arithmetic_v<Arg2> && (std::is_floating_point_v<Arg1> || std::is_floating_point_v<Arg2>) && (!std::is_same_v<Arg1, Arg2>) \
	[[nodiscard]] static auto Func(Arg1 X, Arg2 Y) -> decltype(X * Y);
#define UE_STUB_RESOLVE_FLOAT_AMBIGUITY_3_ARGS(Func) \
	template <typename Arg1, typename Arg2, typename Arg3> \
		requires std::is_arithmetic_v<Arg1> && std::is_arithmetic_v<Arg2> && std::is_arithmetic_v<Arg3> \
			&& (std::is_floating_point_v<Arg1> || std::is_floating_point_v<Arg2> || std::is_floating_point_v<Arg3>) \
			&& (!std::is_same_v<Arg1, Arg2> || !std::is_same_v<Arg2, Arg3> || !std::is_same_v<Arg1, Arg3>) \
	[[nodiscard]] static auto Func(Arg1 X, Arg2 Y, Arg3 Z) -> decltype(X * Y * Z);

template <typename T> struct TCustomLerp { enum { Value = false }; };
namespace UE::Stub
{
	/** What the body of FMath::Lerp(A, B, Alpha) needs: the arithmetic of the general Lerp, or a TCustomLerp type (FQuat,
	 *  FRotator). FMath::BiLerp is a Lerp of two Lerps and every FMath::Interp* ends in Lerp<T>(A, B, float). */
	template <typename T, typename AlphaType>
	concept CLerpableBy = bool(TCustomLerp<T>::Value) || requires(const T& A, const T& B, const AlphaType& Alpha) { (T)(A + Alpha * (B - A)); };
	template <typename T>
	concept CLerpable = CLerpableBy<T, float>;
}

struct FGenericPlatformMath
{
	[[nodiscard]] static float LoadHalf(const uint16* Ptr);
	static void StoreHalf(uint16* Ptr, float Value);
	[[nodiscard]] static constexpr int32 TruncToInt32(float F) { return (int32)F; }
	[[nodiscard]] static constexpr int32 TruncToInt32(double F) { return (int32)F; }
	[[nodiscard]] static constexpr int64 TruncToInt64(double F) { return (int64)F; }
	[[nodiscard]] static constexpr int32 TruncToInt(float F) { return TruncToInt32(F); }
	[[nodiscard]] static constexpr int64 TruncToInt(double F) { return TruncToInt64(F); }
	[[nodiscard]] static float TruncToFloat(float F);
	[[nodiscard]] static double TruncToDouble(double F);
	[[nodiscard]] static double TruncToFloat(double F);
	[[nodiscard]] static int32 FloorToInt32(float F);
	[[nodiscard]] static int32 FloorToInt32(double F);
	[[nodiscard]] static int64 FloorToInt64(double F);
	[[nodiscard]] static int32 FloorToInt(float F);
	[[nodiscard]] static int64 FloorToInt(double F);
	[[nodiscard]] static float FloorToFloat(float F);
	[[nodiscard]] static double FloorToDouble(double F);
	[[nodiscard]] static double FloorToFloat(double F);
	[[nodiscard]] static int32 RoundToInt32(float F);
	[[nodiscard]] static int32 RoundToInt32(double F);
	[[nodiscard]] static int64 RoundToInt64(double F);
	[[nodiscard]] static int32 RoundToInt(float F);
	[[nodiscard]] static int64 RoundToInt(double F);
	[[nodiscard]] static float RoundToFloat(float F);
	[[nodiscard]] static double RoundToDouble(double F);
	[[nodiscard]] static double RoundToFloat(double F);
	[[nodiscard]] static int32 CeilToInt32(float F);
	[[nodiscard]] static int32 CeilToInt32(double F);
	[[nodiscard]] static int64 CeilToInt64(double F);
	[[nodiscard]] static int32 CeilToInt(float F);
	[[nodiscard]] static int64 CeilToInt(double F);
	[[nodiscard]] static float CeilToFloat(float F);
	[[nodiscard]] static double CeilToDouble(double F);
	[[nodiscard]] static double CeilToFloat(double F);
	[[nodiscard]] static int64 RoundToNearestTiesToEven(double F);
	[[nodiscard]] static float Fractional(float Value);
	[[nodiscard]] static double Fractional(double Value);
	[[nodiscard]] static float Frac(float Value);
	[[nodiscard]] static double Frac(double Value);
	[[nodiscard]] static float Modf(const float InValue, float* OutIntPart);
	[[nodiscard]] static double Modf(const double InValue, double* OutIntPart);
	[[nodiscard]] static float Exp(float Value);
	[[nodiscard]] static double Exp(double Value);
	[[nodiscard]] static float Exp2(float Value);
	[[nodiscard]] static double Exp2(double Value);
	[[nodiscard]] static float Loge(float Value);
	[[nodiscard]] static double Loge(double Value);
	[[nodiscard]] static float LogX(float Base, float Value);
	[[nodiscard]] static double LogX(double Base, double Value);
	UE_STUB_RESOLVE_FLOAT_AMBIGUITY_2_ARGS(LogX)
	[[nodiscard]] static float Log2(float Value);
	[[nodiscard]] static double Log2(double Value);
	[[nodiscard]] static float Fmod(float X, float Y);
	[[nodiscard]] static double Fmod(double X, double Y);
	UE_STUB_RESOLVE_FLOAT_AMBIGUITY_2_ARGS(Fmod)
	[[nodiscard]] static float Sin(float Value);
	[[nodiscard]] static double Sin(double Value);
	[[nodiscard]] static float Asin(float Value);
	[[nodiscard]] static double Asin(double Value);
	[[nodiscard]] static float Sinh(float Value);
	[[nodiscard]] static double Sinh(double Value);
	[[nodiscard]] static float Cos(float Value);
	[[nodiscard]] static double Cos(double Value);
	[[nodiscard]] static float Acos(float Value);
	[[nodiscard]] static double Acos(double Value);
	[[nodiscard]] static float Cosh(float Value);
	[[nodiscard]] static double Cosh(double Value);
	[[nodiscard]] static float Tan(float Value);
	[[nodiscard]] static double Tan(double Value);
	[[nodiscard]] static float Atan(float Value);
	[[nodiscard]] static double Atan(double Value);
	[[nodiscard]] static float Tanh(float Value);
	[[nodiscard]] static double Tanh(double Value);
	[[nodiscard]] static float Atan2(float Y, float X);
	[[nodiscard]] static double Atan2(double Y, double X);
	UE_STUB_RESOLVE_FLOAT_AMBIGUITY_2_ARGS(Atan2)
	[[nodiscard]] static float Sqrt(float Value);
	[[nodiscard]] static double Sqrt(double Value);
	[[nodiscard]] static float Pow(float A, float B);
	[[nodiscard]] static double Pow(double A, double B);
	UE_STUB_RESOLVE_FLOAT_AMBIGUITY_2_ARGS(Pow)
	[[nodiscard]] static float InvSqrt(float F);
	[[nodiscard]] static double InvSqrt(double F);
	[[nodiscard]] static float InvSqrtEst(float F);
	[[nodiscard]] static double InvSqrtEst(double F);
	[[nodiscard]] static bool IsNaN(float A);
	[[nodiscard]] static bool IsNaN(double A);
	[[nodiscard]] static bool IsFinite(float A);
	[[nodiscard]] static bool IsFinite(double A);
	[[nodiscard]] static bool IsNegativeOrNegativeZero(float A);
	[[nodiscard]] static bool IsNegativeOrNegativeZero(double A);
	[[nodiscard]] static int32 Rand();
	[[nodiscard]] static int32 Rand32();
	static void RandInit(int32 Seed);
	[[nodiscard]] static float FRand();
	static void SRandInit(int32 Seed);
	[[nodiscard]] static int32 GetRandSeed();
	[[nodiscard]] static float SRand();
	[[nodiscard]] static uint32 FloorLog2(uint32 Value);
	[[nodiscard]] static uint64 FloorLog2_64(uint64 Value);
	[[nodiscard]] static uint32 CountLeadingZeros(uint32 Value);
	[[nodiscard]] static uint64 CountLeadingZeros64(uint64 Value);
	[[nodiscard]] static uint32 CountTrailingZeros(uint32 Value);
	[[nodiscard]] static uint32 CeilLogTwo(uint32 Arg);
	[[nodiscard]] static uint32 RoundUpToPowerOfTwo(uint32 Arg);
	[[nodiscard]] static uint64 RoundUpToPowerOfTwo64(uint64 V);
	[[nodiscard]] static int32 CountBits(uint64 Bits);
	[[nodiscard]] static float FloatSelect(float Comparand, float ValueGEZero, float ValueLTZero);
	[[nodiscard]] static double FloatSelect(double Comparand, double ValueGEZero, double ValueLTZero);

	template <class T> [[nodiscard]] static constexpr T Abs(const T A) { return (A < (T)0) ? -A : A; }
	template <class T> [[nodiscard]] static constexpr T Sign(const T A) { return (A > (T)0) ? (T)1 : ((A < (T)0) ? (T)-1 : (T)0); }
	template <class T> [[nodiscard]] static constexpr T Max(const T A, const T B) { return (B < A) ? A : B; }
	template <class T> [[nodiscard]] static constexpr T Min(const T A, const T B) { return (A < B) ? A : B; }
	UE_STUB_MIX_FLOATS_2_ARGS(Max)
	UE_STUB_MIX_FLOATS_2_ARGS(Min)
	UE_STUB_MIX_SIGNED_INTS_2_ARGS(Max)
	UE_STUB_MIX_SIGNED_INTS_2_ARGS(Min)
	template <class T> [[nodiscard]] static T Min(const TArray<T>& Values, int32* MinIndex = nullptr);
	template <class T> [[nodiscard]] static T Max(const TArray<T>& Values, int32* MaxIndex = nullptr) UE_STUB_BODY
};
using FPlatformMath = FGenericPlatformMath;

struct FMath : public FPlatformMath
{
	[[nodiscard]] static int32 RandHelper(int32 A);
	[[nodiscard]] static int64 RandHelper64(int64 A);
	[[nodiscard]] static int32 RandRange(int32 Min, int32 Max);
	[[nodiscard]] static int64 RandRange(int64 Min, int64 Max);
	[[nodiscard]] static float RandRange(float InMin, float InMax);
	[[nodiscard]] static double RandRange(double InMin, double InMax);
	UE_STUB_RESOLVE_FLOAT_AMBIGUITY_2_ARGS(RandRange)
	[[nodiscard]] static float FRandRange(float InMin, float InMax);
	[[nodiscard]] static double FRandRange(double InMin, double InMax);
	UE_STUB_RESOLVE_FLOAT_AMBIGUITY_2_ARGS(FRandRange)
	[[nodiscard]] static bool RandBool();
	[[nodiscard]] static FVector VRand();
	[[nodiscard]] static FVector VRandCone(FVector const& Dir, float ConeHalfAngleRad);
	[[nodiscard]] static FVector VRandCone(FVector const& Dir, float HorizontalConeHalfAngleRad, float VerticalConeHalfAngleRad);
	[[nodiscard]] static FVector2D RandPointInCircle(float CircleRadius);
	[[nodiscard]] static FVector RandPointInBox(const FBox& Box);
	[[nodiscard]] static FVector GetReflectionVector(const FVector& Direction, const FVector& SurfaceNormal);

	template <class T, class U> [[nodiscard]] static constexpr bool IsWithin(const T& TestValue, const U& MinValue, const U& MaxValue) { return ((TestValue >= MinValue) && (TestValue < MaxValue)); }
	template <class T, class U> [[nodiscard]] static constexpr bool IsWithinInclusive(const T& TestValue, const U& MinValue, const U& MaxValue) { return ((TestValue >= MinValue) && (TestValue <= MaxValue)); }
	[[nodiscard]] static bool IsNearlyEqual(float A, float B, float ErrorTolerance = UE_SMALL_NUMBER);
	[[nodiscard]] static bool IsNearlyEqual(double A, double B, double ErrorTolerance = UE_DOUBLE_SMALL_NUMBER);
	template <typename Arg1, typename Arg2>
		requires std::is_arithmetic_v<Arg1> && std::is_arithmetic_v<Arg2> && (std::is_floating_point_v<Arg1> || std::is_floating_point_v<Arg2>) && (!std::is_same_v<Arg1, Arg2>)
	[[nodiscard]] static bool IsNearlyEqual(Arg1 A, Arg2 B) UE_STUB_BODY
	template <typename Arg1, typename Arg2, typename Arg3>
		requires std::is_arithmetic_v<Arg1> && std::is_arithmetic_v<Arg2> && std::is_arithmetic_v<Arg3>
			&& (std::is_floating_point_v<Arg1> || std::is_floating_point_v<Arg2> || std::is_floating_point_v<Arg3>)
			&& (!std::is_same_v<Arg1, Arg2> || !std::is_same_v<Arg2, Arg3> || !std::is_same_v<Arg1, Arg3>)
	[[nodiscard]] static bool IsNearlyEqual(Arg1 A, Arg2 B, Arg3 ErrorTolerance) UE_STUB_BODY
	[[nodiscard]] static bool IsNearlyZero(float Value, float ErrorTolerance = UE_SMALL_NUMBER);
	[[nodiscard]] static bool IsNearlyZero(double Value, double ErrorTolerance = UE_DOUBLE_SMALL_NUMBER);
	template <typename Arg1, typename Arg2>
		requires std::is_arithmetic_v<Arg1> && std::is_arithmetic_v<Arg2> && (std::is_floating_point_v<Arg1> || std::is_floating_point_v<Arg2>) && (!std::is_same_v<Arg1, Arg2>)
	[[nodiscard]] static bool IsNearlyZero(Arg1 Value, Arg2 ErrorTolerance) UE_STUB_BODY
	[[nodiscard]] static bool IsNearlyEqualByULP(float A, float B, int32 MaxUlps = 4);
	[[nodiscard]] static bool IsNearlyEqualByULP(double A, double B, int32 MaxUlps = 4);
	template <typename T> [[nodiscard]] static constexpr bool IsPowerOfTwo(T Value) { return ((Value & (Value - 1)) == (T)0); }
	[[nodiscard]] static float Floor(float F);
	[[nodiscard]] static double Floor(double F);

	template <class T> [[nodiscard]] static constexpr T Max3(const T A, const T B, const T C) { return Max(Max(A, B), C); }
	template <class T> [[nodiscard]] static constexpr T Min3(const T A, const T B, const T C) { return Min(Min(A, B), C); }
	UE_STUB_MIX_FLOATS_3_ARGS(Max3)
	UE_STUB_MIX_FLOATS_3_ARGS(Min3)
	template <class T> [[nodiscard]] static constexpr int32 Max3Index(const T A, const T B, const T C) { return (A > B) ? ((A > C) ? 0 : 2) : ((B > C) ? 1 : 2); }
	template <class T> [[nodiscard]] static constexpr int32 Min3Index(const T A, const T B, const T C) { return (A < B) ? ((A < C) ? 0 : 2) : ((B < C) ? 1 : 2); }
	template <class T> [[nodiscard]] static constexpr T Square(const T A) { return A * A; }
	template <class T> [[nodiscard]] static constexpr T Cube(const T A) { return A * A * A; }
	template <class T> [[nodiscard]] static constexpr T Clamp(const T X, const T MinValue, const T MaxValue) { return Max(Min(X, MaxValue), MinValue); }
	UE_STUB_MIX_FLOATS_3_ARGS(Clamp)
	[[nodiscard]] static constexpr float Clamp(const float X, const float InMin, const float InMax) { return X < InMin ? InMin : X < InMax ? X : InMax; }
	[[nodiscard]] static constexpr double Clamp(const double X, const double InMin, const double InMax) { return X < InMin ? InMin : X < InMax ? X : InMax; }
	[[nodiscard]] static constexpr int64 Clamp(const int64 X, const int32 InMin, const int32 InMax) { return X < InMin ? InMin : X < InMax ? X : InMax; }
	/** Wraps X to be between Min and Max, inclusive (the engine's body, without its unsigned-size refinement for integers). */
	template <typename T> [[nodiscard]] static constexpr T Wrap(const T X, const T Min, const T Max)
	{
		T Size = Max - Min;
		if (Size == 0)
		{
			return Max;
		}
		T EndVal = X;
		while (EndVal < Min) { EndVal += Size; }
		while (EndVal > Max) { EndVal -= Size; }
		return EndVal;
	}
	/** Snaps a value to the nearest grid multiple. */
	template <class T> [[nodiscard]] static constexpr T GridSnap(T Location, T Grid)
	{
		return (Grid == T{}) ? Location : (Floor((Location + (Grid / (T)2)) / Grid) * Grid);
	}
	UE_STUB_MIX_FLOATS_2_ARGS(GridSnap)
	template <class T> [[nodiscard]] static constexpr T DivideAndRoundUp(T Dividend, T Divisor) { return (Dividend + Divisor - 1) / Divisor; }
	template <class T> [[nodiscard]] static constexpr T DivideAndRoundDown(T Dividend, T Divisor) { return Dividend / Divisor; }
	template <class T> [[nodiscard]] static constexpr T DivideAndRoundNearest(T Dividend, T Divisor) { return (Dividend >= 0) ? (Dividend + Divisor / 2) / Divisor : (Dividend - Divisor / 2 + 1) / Divisor; }
	[[nodiscard]] static float Log2(float Value);
	[[nodiscard]] static double Log2(double Value);
	/** Computes the sine and cosine of a scalar value (the engine's minimax approximation; the constants only matter at run time). */
	template <typename T> static constexpr void SinCos(T* ScalarSin, T* ScalarCos, T Value) requires std::is_floating_point_v<T>
	{
		// Map Value to y in [-pi,pi], x = 2*pi*quotient + remainder.
		T quotient = (UE_INV_PI * 0.5f) * Value;
		if (Value >= 0.0f)
		{
			quotient = (T)((int64)(quotient + 0.5f));
		}
		else
		{
			quotient = (T)((int64)(quotient - 0.5f));
		}
		T y = Value - UE_TWO_PI * quotient;
		// Map y to [-pi/2,pi/2] with sin(y) = sin(Value).
		T sign;
		if (y > UE_HALF_PI)
		{
			y = UE_PI - y;
			sign = -1.0f;
		}
		else if (y < -UE_HALF_PI)
		{
			y = -UE_PI - y;
			sign = -1.0f;
		}
		else
		{
			sign = +1.0f;
		}
		T y2 = y * y;
		*ScalarSin = (((((-2.3889859e-08f * y2 + 2.7525562e-06f) * y2 - 0.00019840874f) * y2 + 0.0083333310f) * y2 - 0.16666667f) * y2 + 1.0f) * y;
		T p = ((((-2.6051615e-07f * y2 + 2.4760495e-05f) * y2 - 0.0013888378f) * y2 + 0.041666638f) * y2 - 0.5f) * y2 + 1.0f;
		*ScalarCos = sign * p;
	}
	[[nodiscard]] static float FastAsin(float Value);
	[[nodiscard]] static double FastAsin(double Value);

	template <class T> [[nodiscard]] static constexpr auto RadiansToDegrees(T const& RadVal) -> decltype(RadVal * (180.f / UE_PI)) { return RadVal * (180.f / UE_PI); }
	[[nodiscard]] static constexpr float RadiansToDegrees(float const& RadVal) { return RadVal * (180.f / UE_PI); }
	[[nodiscard]] static constexpr double RadiansToDegrees(double const& RadVal) { return RadVal * (180.0 / UE_DOUBLE_PI); }
	template <class T> [[nodiscard]] static constexpr auto DegreesToRadians(T const& DegVal) -> decltype(DegVal * (UE_PI / 180.f)) { return DegVal * (UE_PI / 180.f); }
	[[nodiscard]] static constexpr float DegreesToRadians(float const& DegVal) { return DegVal * (UE_PI / 180.f); }
	[[nodiscard]] static constexpr double DegreesToRadians(double const& DegVal) { return DegVal * (UE_DOUBLE_PI / 180.0); }
	template <typename T> [[nodiscard]] static T ClampAngle(T AngleDegrees, T MinAngleDegrees, T MaxAngleDegrees) requires std::is_floating_point_v<T> UE_STUB_BODY
	UE_STUB_RESOLVE_FLOAT_AMBIGUITY_3_ARGS(ClampAngle)
	/** Find the smallest angle between two headings (in degrees): the result is in [-180, 180]. The engine's body. */
	template <typename T, typename T2> requires std::is_floating_point_v<T> || std::is_floating_point_v<T2>
	[[nodiscard]] static constexpr auto FindDeltaAngleDegrees(T A1, T2 A2) -> decltype(A1 * A2)
	{
		auto Delta = A2 - A1;
		if (Delta > 180.0f)
		{
			Delta = Delta - 360.0f;
		}
		else if (Delta < -180.0f)
		{
			Delta = Delta + 360.0f;
		}
		return Delta;
	}
	/** Find the smallest angle between two headings (in radians). */
	template <typename T, typename T2> requires std::is_floating_point_v<T> || std::is_floating_point_v<T2>
	[[nodiscard]] static constexpr auto FindDeltaAngleRadians(T A1, T2 A2) -> decltype(A1 * A2)
	{
		auto Delta = A2 - A1;
		if (Delta > UE_PI)
		{
			Delta = Delta - UE_TWO_PI;
		}
		else if (Delta < -UE_PI)
		{
			Delta = Delta + UE_TWO_PI;
		}
		return Delta;
	}
	/** Given a heading which may be outside the +/- PI range, 'unwind' it back into that range. */
	template <typename T> [[nodiscard]] static constexpr T UnwindRadians(T A) requires std::is_floating_point_v<T>
	{
		while (A > UE_PI) { A -= UE_TWO_PI; }
		while (A < -UE_PI) { A += UE_TWO_PI; }
		return A;
	}
	/** Utility to ensure angle is between +/- 180 degrees by unwinding. */
	template <typename T> [[nodiscard]] static constexpr T UnwindDegrees(T A) requires std::is_floating_point_v<T>
	{
		while (A > 180.f) { A -= 360.f; }
		while (A < -180.f) { A += 360.f; }
		return A;
	}
	static void WindRelativeAnglesDegrees(float InAngle0, float& InOutAngle1);
	static void WindRelativeAnglesDegrees(double InAngle0, double& InOutAngle1);
	[[nodiscard]] static float FixedTurn(float InCurrent, float InDesired, float InDeltaRate);
	// Sqrt / Atan2 / Cos / Sin of T: float or double.
	template <typename T> requires std::is_floating_point_v<T> static void CartesianToPolar(const T X, const T Y, T& OutRad, T& OutAng) UE_STUB_BODY
	template <typename T> requires std::is_floating_point_v<T> static void PolarToCartesian(const T Rad, const T Ang, T& OutX, T& OutY) UE_STUB_BODY
	/** Calculates the percentage along a line from MinValue to MaxValue that Value is (the engine's body). */
	template <typename T, typename T2 = T> [[nodiscard]] static constexpr auto GetRangePct(T MinValue, T MaxValue, T2 Value) -> decltype(MinValue * Value)
	{
		using RetType = decltype(MinValue * Value);
		// Avoid Divide by Zero. But also if our range is a point, output whether Value is before or after.
		const T Divisor = MaxValue - MinValue;
		if (FMath::IsNearlyZero(Divisor))
		{
			return (Value >= MaxValue) ? (RetType)1 : (RetType)0;
		}
		return (RetType)((Value - MinValue) / Divisor);
	}
	template <typename T, typename T2> [[nodiscard]] static auto GetRangePct(UE::Math::TVector2<T> const& Range, T2 Value) -> decltype(Range.X * Value) UE_STUB_BODY
	template <typename T, typename T2> [[nodiscard]] static auto GetRangeValue(UE::Math::TVector2<T> const& Range, T2 Pct) -> decltype(Range.X * Pct) UE_STUB_BODY
	template <typename T, typename T2> [[nodiscard]] static auto GetMappedRangeValueClamped(const UE::Math::TVector2<T>& InputRange, const UE::Math::TVector2<T>& OutputRange, const T2 Value) -> decltype(InputRange.X * Value) UE_STUB_BODY
	template <typename T, typename T2> [[nodiscard]] static auto GetMappedRangeValueUnclamped(const UE::Math::TVector2<T>& InputRange, const UE::Math::TVector2<T>& OutputRange, const T2 Value) -> decltype(InputRange.X * Value) UE_STUB_BODY

	template <typename T, typename U> requires (!bool(TCustomLerp<T>::Value)) && (std::is_floating_point_v<U> || std::is_same_v<T, U>) && (!std::is_same_v<T, bool>)
	[[nodiscard]] static constexpr T Lerp(const T& A, const T& B, const U& Alpha) { return (T)(A + Alpha * (B - A)); }
	template <typename T1, typename T2, typename T3> requires (!std::is_same_v<T1, T2>) && (!bool(TCustomLerp<T1>::Value)) && (!bool(TCustomLerp<T2>::Value))
	[[nodiscard]] static auto Lerp(const T1& A, const T2& B, const T3& Alpha) -> decltype(A * B) UE_STUB_BODY
	template <typename T, typename U> requires (bool(TCustomLerp<T>::Value))
	[[nodiscard]] static T Lerp(const T& A, const T& B, const U& Alpha) UE_STUB_BODY
	// The constraints below are the expressions of the engine's bodies: (T)((A * (1 - Alpha)) + (B * Alpha)); a Lerp of
	// two Lerps; Lerp<T>(A, B, float) at the end of every Interp*. CubicInterp is not declared (unused by the module).
	template <class T> requires requires(const T& A, const T& B, double Alpha) { (T)((A * (1.0 - Alpha)) + (B * Alpha)); }
	[[nodiscard]] static T LerpStable(const T& A, const T& B, double Alpha) UE_STUB_BODY
	template <class T> requires requires(const T& A, const T& B, float Alpha) { (T)((A * (1.0f - Alpha)) + (B * Alpha)); }
	[[nodiscard]] static T LerpStable(const T& A, const T& B, float Alpha) UE_STUB_BODY
	template <class T, class U> requires UE::Stub::CLerpableBy<T, U>
	[[nodiscard]] static T BiLerp(const T& P00, const T& P10, const T& P01, const T& P11, const U& FracX, const U& FracY) UE_STUB_BODY
	template <class T> requires UE::Stub::CLerpable<T> [[nodiscard]] static T InterpEaseIn(const T& A, const T& B, float Alpha, float Exp) UE_STUB_BODY
	template <class T> requires UE::Stub::CLerpable<T> [[nodiscard]] static T InterpEaseOut(const T& A, const T& B, float Alpha, float Exp) UE_STUB_BODY
	template <class T> requires UE::Stub::CLerpable<T> [[nodiscard]] static T InterpEaseInOut(const T& A, const T& B, float Alpha, float Exp) UE_STUB_BODY
	template <class T> requires UE::Stub::CLerpable<T> [[nodiscard]] static T InterpStep(const T& A, const T& B, float Alpha, int32 Steps) UE_STUB_BODY
	template <class T> requires UE::Stub::CLerpable<T> [[nodiscard]] static T InterpSinIn(const T& A, const T& B, float Alpha) UE_STUB_BODY
	template <class T> requires UE::Stub::CLerpable<T> [[nodiscard]] static T InterpSinOut(const T& A, const T& B, float Alpha) UE_STUB_BODY
	template <class T> requires UE::Stub::CLerpable<T> [[nodiscard]] static T InterpSinInOut(const T& A, const T& B, float Alpha) UE_STUB_BODY
	template <class T> requires UE::Stub::CLerpable<T> [[nodiscard]] static T InterpExpoIn(const T& A, const T& B, float Alpha) UE_STUB_BODY
	template <class T> requires UE::Stub::CLerpable<T> [[nodiscard]] static T InterpExpoOut(const T& A, const T& B, float Alpha) UE_STUB_BODY
	template <class T> requires UE::Stub::CLerpable<T> [[nodiscard]] static T InterpExpoInOut(const T& A, const T& B, float Alpha) UE_STUB_BODY
	template <class T> requires UE::Stub::CLerpable<T> [[nodiscard]] static T InterpCircularIn(const T& A, const T& B, float Alpha) UE_STUB_BODY
	template <class T> requires UE::Stub::CLerpable<T> [[nodiscard]] static T InterpCircularOut(const T& A, const T& B, float Alpha) UE_STUB_BODY
	template <class T> requires UE::Stub::CLerpable<T> [[nodiscard]] static T InterpCircularInOut(const T& A, const T& B, float Alpha) UE_STUB_BODY
	/** Returns a smooth Hermite interpolation between 0 and 1 for the value X (where X ranges between A and B). */
	template <class T> [[nodiscard]] static constexpr T SmoothStep(T A, T B, T X) requires std::is_floating_point_v<T>
	{
		if (X < A)
		{
			return 0;
		}
		else if (X >= B)
		{
			return 1;
		}
		const T InterpFraction = (X - A) / (B - A);
		return InterpFraction * InterpFraction * (3.0f - 2.0f * InterpFraction);
	}
	UE_STUB_RESOLVE_FLOAT_AMBIGUITY_3_ARGS(SmoothStep)

	[[nodiscard]] static FVector VInterpNormalRotationTo(const FVector& Current, const FVector& Target, float DeltaTime, float RotationSpeedDegrees);
	[[nodiscard]] static FVector VInterpConstantTo(const FVector& Current, const FVector& Target, float DeltaTime, float InterpSpeed);
	[[nodiscard]] static FVector VInterpTo(const FVector& Current, const FVector& Target, float DeltaTime, float InterpSpeed);
	[[nodiscard]] static FVector2D Vector2DInterpConstantTo(const FVector2D& Current, const FVector2D& Target, float DeltaTime, float InterpSpeed);
	[[nodiscard]] static FVector2D Vector2DInterpTo(const FVector2D& Current, const FVector2D& Target, float DeltaTime, float InterpSpeed);
	[[nodiscard]] static FRotator RInterpConstantTo(const FRotator& Current, const FRotator& Target, float DeltaTime, float InterpSpeed);
	[[nodiscard]] static FRotator RInterpTo(const FRotator& Current, const FRotator& Target, float DeltaTime, float InterpSpeed);
	template <typename T1, typename T2 = T1, typename T3 = T2, typename T4 = T3>
		requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2> && std::is_arithmetic_v<T3> && std::is_arithmetic_v<T4>
	[[nodiscard]] static auto FInterpConstantTo(T1 Current, T2 Target, T3 DeltaTime, T4 InterpSpeed) -> decltype(Current * Target * DeltaTime * InterpSpeed) UE_STUB_BODY
	template <typename T1, typename T2 = T1, typename T3 = T2, typename T4 = T3>
		requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2> && std::is_arithmetic_v<T3> && std::is_arithmetic_v<T4>
	[[nodiscard]] static constexpr auto FInterpTo(T1 Current, T2 Target, T3 DeltaTime, T4 InterpSpeed) -> decltype(Current * Target * DeltaTime * InterpSpeed)
	{
		using RetType = decltype(Current * Target * DeltaTime * InterpSpeed);
		// If no interp speed, jump to target value
		if (InterpSpeed <= 0.f)
		{
			return static_cast<RetType>(Target);
		}
		// Distance to reach
		const RetType Dist = Target - Current;
		// If distance is too small, just set the desired location
		if (Dist * Dist < UE_SMALL_NUMBER)
		{
			return static_cast<RetType>(Target);
		}
		// Delta Move, Clamp so we do not over shoot.
		const RetType Alpha = static_cast<RetType>(DeltaTime * InterpSpeed);
		const RetType DeltaMove = Dist * (Alpha < 0 ? static_cast<RetType>(0) : (Alpha > 1 ? static_cast<RetType>(1) : Alpha));
		return Current + DeltaMove;
	}
	[[nodiscard]] static FLinearColor CInterpTo(const FLinearColor& Current, const FLinearColor& Target, float DeltaTime, float InterpSpeed);
	[[nodiscard]] static float MakePulsatingValue(const double InCurrentTime, const float InPulsesPerSecond, const float InPhase = 0.0f);
	[[nodiscard]] static float PerlinNoise1D(float Value);
	[[nodiscard]] static float PerlinNoise2D(const FVector2D& Location);
	[[nodiscard]] static float PerlinNoise3D(const FVector& Location);
	[[nodiscard]] static FVector LinePlaneIntersection(const FVector& Point1, const FVector& Point2, const FVector& PlaneOrigin, const FVector& PlaneNormal);
	[[nodiscard]] static FVector LinePlaneIntersection(const FVector& Point1, const FVector& Point2, const FPlane& Plane);
	[[nodiscard]] static FVector RayPlaneIntersection(const FVector& RayOrigin, const FVector& RayDirection, const FPlane& Plane);
	[[nodiscard]] static bool SegmentPlaneIntersection(const FVector& StartPoint, const FVector& EndPoint, const FPlane& Plane, FVector& out_IntersectionPoint);
	[[nodiscard]] static bool SegmentIntersection2D(const FVector& SegmentStartA, const FVector& SegmentEndA, const FVector& SegmentStartB, const FVector& SegmentEndB, FVector& out_IntersectionPoint);
	[[nodiscard]] static FVector ClosestPointOnSegment(const FVector& Point, const FVector& StartPoint, const FVector& EndPoint);
	[[nodiscard]] static FVector2D ClosestPointOnSegment2D(const FVector2D& Point, const FVector2D& StartPoint, const FVector2D& EndPoint);
	[[nodiscard]] static FVector ClosestPointOnLine(const FVector& LineStart, const FVector& LineEnd, const FVector& Point);
	[[nodiscard]] static float PointDistToSegment(const FVector& Point, const FVector& StartPoint, const FVector& EndPoint);
	[[nodiscard]] static float PointDistToSegmentSquared(const FVector& Point, const FVector& StartPoint, const FVector& EndPoint);
	[[nodiscard]] static bool LineBoxIntersection(const FBox& Box, const FVector& Start, const FVector& End, const FVector& Direction);
	[[nodiscard]] static bool SphereAABBIntersection(const FVector& SphereCenter, const double RadiusSquared, const FBox& AABB);
	static void ApplyScaleToFloat(float& Dst, const FVector& DeltaScale, float Magnitude = 1.0f);
};
