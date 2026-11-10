// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Math/IntVector.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/IntPoint.h"

namespace UE::Math
{
	template <typename IntType>
	struct TIntVector3
	{
		IntType X, Y, Z;
		static const TIntVector3 ZeroValue;
		static const TIntVector3 NoneValue;
		TIntVector3() = default;
		TIntVector3(IntType InX, IntType InY, IntType InZ) : X(InX), Y(InY), Z(InZ) {}
		explicit TIntVector3(IntType InValue) : X(InValue), Y(InValue), Z(InValue) {}
		explicit TIntVector3(EForceInit) : X(0), Y(0), Z(0) {}
		template <typename FloatType> requires std::is_floating_point_v<FloatType>
		explicit TIntVector3(TVector<FloatType> InVector) UE_STUB_BODY
		const IntType& operator[](int32 ComponentIndex) const UE_STUB_BODY
		IntType& operator[](int32 ComponentIndex) UE_STUB_BODY
		bool operator==(const TIntVector3& Other) const { return X == Other.X && Y == Other.Y && Z == Other.Z; }
		bool operator!=(const TIntVector3& Other) const { return !(*this == Other); }
		TIntVector3 operator*(IntType Scale) const UE_STUB_BODY
		TIntVector3 operator/(IntType Divisor) const UE_STUB_BODY
		TIntVector3 operator+(const TIntVector3& Other) const UE_STUB_BODY
		TIntVector3 operator-(const TIntVector3& Other) const UE_STUB_BODY
		TIntVector3& operator+=(const TIntVector3& Other) UE_STUB_BODY
		TIntVector3& operator-=(const TIntVector3& Other) UE_STUB_BODY
		IntType GetMax() const UE_STUB_BODY
		IntType GetMin() const UE_STUB_BODY
		IntType Size() const UE_STUB_BODY
		FString ToString() const UE_STUB_BODY
		bool IsZero() const UE_STUB_BODY
	};
	template <typename IntType> uint32 GetTypeHash(const TIntVector3<IntType>& Vector) UE_STUB_BODY
	template <typename IntType>
	struct TIntVector4
	{
		IntType X, Y, Z, W;
		TIntVector4() = default;
		TIntVector4(IntType InX, IntType InY, IntType InZ, IntType InW) : X(InX), Y(InY), Z(InZ), W(InW) {}
		explicit TIntVector4(IntType InValue) : X(InValue), Y(InValue), Z(InValue), W(InValue) {}
		bool operator==(const TIntVector4& Other) const = default;
	};
}
