// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Math/Box2D.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/Vector2D.h"
#include "Containers/Array.h"

namespace UE::Math
{
	template <typename T>
	struct TBox2
	{
		using FReal = T;
		TVector2<T> Min;
		TVector2<T> Max;
		bool bIsValid;
		TBox2() = default;
		explicit TBox2(EForceInit) UE_STUB_BODY
		template <typename FArg> requires std::is_arithmetic_v<FArg>
		TBox2(const TVector2<FArg>& InMin, const TVector2<FArg>& InMax) UE_STUB_BODY
		TBox2(const TVector2<T>* Points, const int32 Count) UE_STUB_BODY
		explicit TBox2(const TArray<TVector2<T>>& Points) UE_STUB_BODY
		bool operator==(const TBox2& Other) const UE_STUB_BODY
		bool operator!=(const TBox2& Other) const UE_STUB_BODY
		bool Equals(const TBox2& Other, T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		TBox2& operator+=(const TVector2<T>& Other) UE_STUB_BODY
		TBox2 operator+(const TVector2<T>& Other) const UE_STUB_BODY
		TBox2& operator+=(const TBox2& Other) UE_STUB_BODY
		TBox2 operator+(const TBox2& Other) const UE_STUB_BODY
		TVector2<T>& operator[](int32 Index) UE_STUB_BODY
		T ComputeSquaredDistanceToPoint(const TVector2<T>& Point) const UE_STUB_BODY
		TBox2 ExpandBy(const T W) const UE_STUB_BODY
		TBox2 ExpandBy(const TVector2<T>& V) const UE_STUB_BODY
		T GetArea() const UE_STUB_BODY
		TVector2<T> GetCenter() const UE_STUB_BODY
		void GetCenterAndExtents(TVector2<T>& center, TVector2<T>& Extents) const UE_STUB_BODY
		TVector2<T> GetClosestPointTo(const TVector2<T>& Point) const UE_STUB_BODY
		TVector2<T> GetExtent() const UE_STUB_BODY
		TVector2<T> GetSize() const UE_STUB_BODY
		void Init() UE_STUB_BODY
		TBox2 Overlap(const TBox2& Other) const UE_STUB_BODY
		bool Intersect(const TBox2& other) const UE_STUB_BODY
		bool IsInside(const TVector2<T>& TestPoint) const UE_STUB_BODY
		bool IsInsideOrOn(const TVector2<T>& TestPoint) const UE_STUB_BODY
		bool IsInside(const TBox2& Other) const UE_STUB_BODY
		TBox2 ShiftBy(const TVector2<T>& Offset) const UE_STUB_BODY
		FString ToString() const UE_STUB_BODY
	};
}
