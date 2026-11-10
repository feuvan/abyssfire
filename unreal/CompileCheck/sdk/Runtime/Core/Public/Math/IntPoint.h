// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Math/IntPoint.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/UnrealString.h"
#include "Templates/TypeHash.h"

namespace UE::Math
{
	template <typename IntType>
	struct TIntPoint
	{
		static_assert(std::is_integral_v<IntType>, "An integer type is required.");
		using FReal = IntType;
		IntType X, Y;
		static const TIntPoint ZeroValue;
		static const TIntPoint NoneValue;
		TIntPoint() = default;
		TIntPoint(IntType InX, IntType InY) : X(InX), Y(InY) {}
		explicit TIntPoint(IntType InXY) : X(InXY), Y(InXY) {}
		explicit TIntPoint(EForceInit) : X(0), Y(0) {}
		template <typename OtherIntType> requires (!std::is_same_v<IntType, OtherIntType>)
		explicit TIntPoint(TIntPoint<OtherIntType> Other) : X((IntType)Other.X), Y((IntType)Other.Y) {}
		const IntType& operator()(int32 PointIndex) const UE_STUB_BODY
		IntType& operator()(int32 PointIndex) UE_STUB_BODY
		bool operator==(const TIntPoint& Other) const { return X == Other.X && Y == Other.Y; }
		bool operator!=(const TIntPoint& Other) const { return !(*this == Other); }
		TIntPoint& operator*=(IntType Scale) UE_STUB_BODY
		TIntPoint& operator/=(IntType Divisor) UE_STUB_BODY
		TIntPoint& operator+=(const TIntPoint& Other) UE_STUB_BODY
		TIntPoint& operator*=(const TIntPoint& Other) UE_STUB_BODY
		TIntPoint& operator-=(const TIntPoint& Other) UE_STUB_BODY
		TIntPoint& operator/=(const TIntPoint& Other) UE_STUB_BODY
		TIntPoint operator*(IntType Scale) const UE_STUB_BODY
		TIntPoint operator/(IntType Divisor) const UE_STUB_BODY
		TIntPoint operator+(const TIntPoint& Other) const UE_STUB_BODY
		TIntPoint operator-(const TIntPoint& Other) const UE_STUB_BODY
		TIntPoint operator*(const TIntPoint& Other) const UE_STUB_BODY
		TIntPoint operator/(const TIntPoint& Other) const UE_STUB_BODY
		IntType& operator[](IntType Index) UE_STUB_BODY
		IntType operator[](IntType Index) const UE_STUB_BODY
		TIntPoint ComponentMin(const TIntPoint& Other) const UE_STUB_BODY
		TIntPoint ComponentMax(const TIntPoint& Other) const UE_STUB_BODY
		IntType GetMax() const UE_STUB_BODY
		IntType GetMin() const UE_STUB_BODY
		IntType Size() const UE_STUB_BODY
		IntType SizeSquared() const UE_STUB_BODY
		FString ToString() const UE_STUB_BODY
		static TIntPoint DivideAndRoundUp(TIntPoint lhs, IntType Divisor) UE_STUB_BODY
		static TIntPoint DivideAndRoundUp(TIntPoint lhs, TIntPoint Divisor) UE_STUB_BODY
		static TIntPoint DivideAndRoundDown(TIntPoint lhs, IntType Divisor) UE_STUB_BODY
		static TIntPoint DivideAndRoundDown(TIntPoint lhs, TIntPoint Divisor) UE_STUB_BODY
		static int32 Num() UE_STUB_BODY
	};
	template <typename IntType> uint32 GetTypeHash(const TIntPoint<IntType>& InPoint) UE_STUB_BODY
}
