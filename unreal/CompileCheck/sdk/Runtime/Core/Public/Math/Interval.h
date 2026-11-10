// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Math/Interval.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Math/NumericLimits.h"

template <typename ElementType>
struct TInterval
{
	ElementType Min;
	ElementType Max;
	TInterval() UE_STUB_BODY
	TInterval(ElementType InMin, ElementType InMax) UE_STUB_BODY
	ElementType Size() const UE_STUB_BODY
	bool IsValid() const UE_STUB_BODY
	bool Contains(const ElementType& Element) const UE_STUB_BODY
	void Expand(ElementType ExpandAmount) UE_STUB_BODY
	void Include(ElementType Element) UE_STUB_BODY
	ElementType Interpolate(float Alpha) const UE_STUB_BODY
	ElementType Clamp(ElementType Value) const UE_STUB_BODY
};
using FFloatInterval = TInterval<float>;
using FDoubleInterval = TInterval<double>;
using FInt32Interval = TInterval<int32>;
