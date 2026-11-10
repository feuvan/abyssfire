// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Math/IntRect.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/IntPoint.h"

namespace UE::Math
{
	template <typename InIntType>
	struct TIntRect
	{
		using IntType = InIntType;
		using IntPointType = TIntPoint<IntType>;
		IntPointType Min;
		IntPointType Max;
		TIntRect() UE_STUB_BODY
		TIntRect(IntType X0, IntType Y0, IntType X1, IntType Y1) UE_STUB_BODY
		TIntRect(IntPointType InMin, IntPointType InMax) UE_STUB_BODY
		bool operator==(const TIntRect& Other) const UE_STUB_BODY
		bool operator!=(const TIntRect& Other) const UE_STUB_BODY
		TIntRect operator+(const IntPointType& Point) const UE_STUB_BODY
		TIntRect operator-(const IntPointType& Point) const UE_STUB_BODY
		TIntRect operator*(IntType Scale) const UE_STUB_BODY
		TIntRect operator/(IntType Div) const UE_STUB_BODY
		IntType Area() const UE_STUB_BODY
		void Clip(const TIntRect& Other) UE_STUB_BODY
		bool Contains(IntPointType Point) const UE_STUB_BODY
		bool Intersect(const TIntRect& Other) const UE_STUB_BODY
		IntPointType Size() const UE_STUB_BODY
		IntType Width() const UE_STUB_BODY
		IntType Height() const UE_STUB_BODY
		bool IsEmpty() const UE_STUB_BODY
		void InflateRect(IntType Amount) UE_STUB_BODY
		void Include(IntPointType Point) UE_STUB_BODY
		FString ToString() const UE_STUB_BODY
	};
}
