// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Math/Vector2D.h (from UE 5.x source knowledge).
// TVector2<T>: FVector2D = TVector2<double>, FVector2f = TVector2<float>. Converting between the two is EXPLICIT;
// FIntPoint converts implicitly; a single scalar and FVector are explicit.
#pragma once

#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/UnrealMathUtility.h"
#include "Math/IntPoint.h"
#include "Containers/UnrealString.h"
#include "Templates/TypeHash.h"

namespace UE::Math
{
	template <typename T>
	struct TVector2
	{
		static_assert(std::is_floating_point_v<T>, "T must be floating point");
		using FReal = T;
		T X;
		T Y;
		static const TVector2<T> ZeroVector;
		static const TVector2<T> UnitVector;
		static const TVector2<T> Unit45Deg;
		static TVector2<T> Zero() { return TVector2<T>((T)0, (T)0); }
		static TVector2<T> One() { return TVector2<T>((T)1, (T)1); }
		static TVector2<T> UnitX() { return TVector2<T>((T)1, (T)0); }
		static TVector2<T> UnitY() { return TVector2<T>((T)0, (T)1); }

		TVector2() = default;
		constexpr TVector2(T InX, T InY) : X(InX), Y(InY) {}
		explicit constexpr TVector2(T InF) : X(InF), Y(InF) {}
		template <typename IntType> constexpr TVector2(TIntPoint<IntType> InPos) : X((T)InPos.X), Y((T)InPos.Y) {}
		explicit constexpr TVector2(EForceInit) : X(0), Y(0) {}
		explicit TVector2(ENoInit) {}
		explicit TVector2(const TVector<T>& V) UE_STUB_BODY
		explicit TVector2(const TVector4<T>& V) UE_STUB_BODY
		template <typename FArg> requires (!std::is_same_v<T, FArg>)
		explicit constexpr TVector2(const TVector2<FArg>& From) : X((T)From.X), Y((T)From.Y) {}

		constexpr TVector2 operator+(const TVector2& V) const { return TVector2(X + V.X, Y + V.Y); }
		constexpr TVector2 operator-(const TVector2& V) const { return TVector2(X - V.X, Y - V.Y); }
		constexpr TVector2 operator*(T Scale) const { return TVector2(X * Scale, Y * Scale); }
		template <typename FArg> requires std::is_arithmetic_v<FArg> && (!std::is_same_v<T, FArg>)
		constexpr TVector2 operator*(FArg Scale) const { return TVector2(X * (T)Scale, Y * (T)Scale); }
		constexpr TVector2 operator/(T Scale) const { return TVector2(X / Scale, Y / Scale); }
		template <typename FArg> requires std::is_arithmetic_v<FArg> && (!std::is_same_v<T, FArg>)
		constexpr TVector2 operator/(FArg Scale) const { return TVector2(X / (T)Scale, Y / (T)Scale); }
		constexpr TVector2 operator+(T A) const { return TVector2(X + A, Y + A); }
		constexpr TVector2 operator-(T A) const { return TVector2(X - A, Y - A); }
		constexpr TVector2 operator*(const TVector2& V) const { return TVector2(X * V.X, Y * V.Y); }
		constexpr TVector2 operator/(const TVector2& V) const { return TVector2(X / V.X, Y / V.Y); }
		constexpr T operator|(const TVector2& V) const { return X * V.X + Y * V.Y; }
		constexpr T operator^(const TVector2& V) const { return X * V.Y - Y * V.X; }
		constexpr bool operator==(const TVector2& V) const { return X == V.X && Y == V.Y; }
		constexpr bool operator!=(const TVector2& V) const { return X != V.X || Y != V.Y; }
		constexpr bool ComponentwiseAllLessThan(const TVector2& Other) const { return X < Other.X && Y < Other.Y; }
		constexpr bool ComponentwiseAllGreaterThan(const TVector2& Other) const { return X > Other.X && Y > Other.Y; }
		constexpr bool ComponentwiseAllLessOrEqual(const TVector2& Other) const { return X <= Other.X && Y <= Other.Y; }
		constexpr bool ComponentwiseAllGreaterOrEqual(const TVector2& Other) const { return X >= Other.X && Y >= Other.Y; }
		constexpr TVector2 operator-() const { return TVector2(-X, -Y); }
		TVector2& operator+=(const TVector2& V) { X += V.X; Y += V.Y; return *this; }
		TVector2& operator-=(const TVector2& V) { X -= V.X; Y -= V.Y; return *this; }
		TVector2& operator*=(T Scale) { X *= Scale; Y *= Scale; return *this; }
		TVector2& operator/=(T V) { X /= V; Y /= V; return *this; }
		TVector2& operator*=(const TVector2& V) { X *= V.X; Y *= V.Y; return *this; }
		TVector2& operator/=(const TVector2& V) { X /= V.X; Y /= V.Y; return *this; }
		T& operator[](int32 Index) UE_STUB_BODY
		T operator[](int32 Index) const UE_STUB_BODY
		T& Component(int32 Index) UE_STUB_BODY
		T Component(int32 Index) const UE_STUB_BODY

		static T DotProduct(const TVector2& A, const TVector2& B) UE_STUB_BODY
		static T DistSquared(const TVector2& V1, const TVector2& V2) UE_STUB_BODY
		static T Distance(const TVector2& V1, const TVector2& V2) UE_STUB_BODY
		static T CrossProduct(const TVector2& A, const TVector2& B) UE_STUB_BODY
		static TVector2 Max(const TVector2& A, const TVector2& B) UE_STUB_BODY
		static TVector2 Min(const TVector2& A, const TVector2& B) UE_STUB_BODY
		static TVector2 Clamp(const TVector2& V, const TVector2& MinValue, const TVector2& MaxValue) UE_STUB_BODY
		bool Equals(const TVector2& V, T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		void Set(T InX, T InY) UE_STUB_BODY
		T GetMax() const UE_STUB_BODY
		T GetAbsMax() const UE_STUB_BODY
		T GetMin() const UE_STUB_BODY
		T Size() const UE_STUB_BODY
		T Length() const UE_STUB_BODY
		T SizeSquared() const UE_STUB_BODY
		T SquaredLength() const UE_STUB_BODY
		T Dot(const TVector2& V2) const UE_STUB_BODY
		TVector2 GetRotated(T AngleDeg) const UE_STUB_BODY
		TVector2 GetSafeNormal(T Tolerance = UE_SMALL_NUMBER) const UE_STUB_BODY
		bool Normalize(T Tolerance = UE_SMALL_NUMBER) UE_STUB_BODY
		bool IsNearlyZero(T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		void ToDirectionAndLength(TVector2& OutDir, double& OutLength) const UE_STUB_BODY
		void ToDirectionAndLength(TVector2& OutDir, float& OutLength) const UE_STUB_BODY
		bool IsZero() const UE_STUB_BODY
		FIntPoint IntPoint() const UE_STUB_BODY
		TVector2 RoundToVector() const UE_STUB_BODY
		TVector2 ClampAxes(T MinAxisVal, T MaxAxisVal) const UE_STUB_BODY
		TVector2 GetSignVector() const UE_STUB_BODY
		TVector2 GetAbs() const UE_STUB_BODY
		FString ToString() const UE_STUB_BODY
		bool InitFromString(const FString& InSourceString) UE_STUB_BODY
		bool ContainsNaN() const UE_STUB_BODY
		TVector<T> SphericalToUnitCartesian() const UE_STUB_BODY
	};
	template <typename T, typename T2> requires std::is_arithmetic_v<T2>
	constexpr TVector2<T> operator*(T2 Scale, const TVector2<T>& V) { return V * (T)Scale; }
	template <typename T> uint32 GetTypeHash(const TVector2<T>& Vector) UE_STUB_BODY
}
template <> struct TIsPODType<FVector2f> { enum { Value = true }; };
template <> struct TIsPODType<FVector2d> { enum { Value = true }; };
