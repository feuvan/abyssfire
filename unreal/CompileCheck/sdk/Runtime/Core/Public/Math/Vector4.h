// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Math/Vector4.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Math/Color.h"

namespace UE::Math
{
	template <typename T>
	struct TVector4
	{
		using FReal = T;
		T X, Y, Z, W;
		TVector4(const TVector<T>& InVector) UE_STUB_BODY
		TVector4(const TVector<T>& InVector, T InW) UE_STUB_BODY
		TVector4(const FLinearColor& InColor) UE_STUB_BODY
		TVector4(const FLinearColor& InColor, T InOverrideW) UE_STUB_BODY
		explicit TVector4(T InX = 0.0f, T InY = 0.0f, T InZ = 0.0f, T InW = 1.0f) UE_STUB_BODY
		explicit TVector4(TVector2<T> InXY, TVector2<T> InZW) UE_STUB_BODY
		explicit TVector4(EForceInit) UE_STUB_BODY
		explicit TVector4(ENoInit) UE_STUB_BODY
		template <typename FArg> requires (!std::is_same_v<T, FArg>)
		explicit TVector4(const TVector4<FArg>& From) UE_STUB_BODY
		[[nodiscard]] static TVector4 Zero() UE_STUB_BODY
		[[nodiscard]] static TVector4 One() UE_STUB_BODY
		T& operator[](int32 ComponentIndex) UE_STUB_BODY
		T operator[](int32 ComponentIndex) const UE_STUB_BODY
		[[nodiscard]] TVector4 operator-() const UE_STUB_BODY
		[[nodiscard]] TVector4 operator+(const TVector4& V) const UE_STUB_BODY
		TVector4& operator+=(const TVector4& V) UE_STUB_BODY
		[[nodiscard]] TVector4 operator-(const TVector4& V) const UE_STUB_BODY
		TVector4& operator-=(const TVector4& V) UE_STUB_BODY
		template <typename FArg> requires std::is_arithmetic_v<FArg> [[nodiscard]] TVector4 operator*(FArg Scale) const UE_STUB_BODY
		template <typename FArg> requires std::is_arithmetic_v<FArg> [[nodiscard]] TVector4 operator/(FArg Scale) const UE_STUB_BODY
		[[nodiscard]] TVector4 operator*(const TVector4& V) const UE_STUB_BODY
		[[nodiscard]] TVector4 operator/(const TVector4& V) const UE_STUB_BODY
		[[nodiscard]] bool operator==(const TVector4& V) const UE_STUB_BODY
		[[nodiscard]] bool operator!=(const TVector4& V) const UE_STUB_BODY
		[[nodiscard]] bool Equals(const TVector4& V, T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		[[nodiscard]] T Size3() const UE_STUB_BODY
		[[nodiscard]] T SizeSquared3() const UE_STUB_BODY
		[[nodiscard]] T Size() const UE_STUB_BODY
		[[nodiscard]] T SizeSquared() const UE_STUB_BODY
		[[nodiscard]] TVector4 GetSafeNormal(T Tolerance = UE_SMALL_NUMBER) const UE_STUB_BODY
		void Set(T InX, T InY, T InZ, T InW) UE_STUB_BODY
		[[nodiscard]] FString ToString() const UE_STUB_BODY
	};
	template <typename T, typename T2> requires std::is_arithmetic_v<T2>
	TVector4<T> operator*(T2 Scale, const TVector4<T>& V) UE_STUB_BODY
}
