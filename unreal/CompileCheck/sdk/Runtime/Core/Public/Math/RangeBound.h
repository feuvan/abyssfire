// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Math/RangeBound.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Misc/DateTime.h"
#include "Misc/Timespan.h"
#include "Templates/TypeHash.h"

namespace ERangeBoundTypes
{
	enum Type
	{
		/** The range excludes the bound. */
		Exclusive,
		/** The range includes the bound. */
		Inclusive,
		/** The bound is open. */
		Open,
	};
}

/** One bound of a TRange: open, or a value that is included or excluded. */
template <typename ElementType>
class TRangeBound
{
public:
	typedef typename TCallTraits<ElementType>::ParamType ElementValueOrConstRef;
	/** Default constructor: an open bound. */
	TRangeBound() UE_STUB_BODY
	/** Creates a closed (inclusive) bound from a value (implicit, as in the engine). */
	TRangeBound(ElementValueOrConstRef InValue) UE_STUB_BODY
	bool operator==(const TRangeBound& Other) const UE_STUB_BODY
	bool operator!=(const TRangeBound& Other) const UE_STUB_BODY
	ElementValueOrConstRef GetValue() const UE_STUB_BODY
	void SetValue(ElementValueOrConstRef NewValue) UE_STUB_BODY
	bool IsClosed() const UE_STUB_BODY
	bool IsExclusive() const UE_STUB_BODY
	bool IsInclusive() const UE_STUB_BODY
	bool IsOpen() const UE_STUB_BODY
	static TRangeBound Exclusive(ElementValueOrConstRef Value) UE_STUB_BODY
	static TRangeBound Inclusive(ElementValueOrConstRef Value) UE_STUB_BODY
	static TRangeBound Open() UE_STUB_BODY
	static TRangeBound FlipInclusion(const TRangeBound& Bound) UE_STUB_BODY
	static const TRangeBound& MaxLower(const TRangeBound& A, const TRangeBound& B) UE_STUB_BODY
	static const TRangeBound& MaxUpper(const TRangeBound& A, const TRangeBound& B) UE_STUB_BODY
	static const TRangeBound& MinLower(const TRangeBound& A, const TRangeBound& B) UE_STUB_BODY
	static const TRangeBound& MinUpper(const TRangeBound& A, const TRangeBound& B) UE_STUB_BODY
};
template <typename ElementType> uint32 GetTypeHash(const TRangeBound<ElementType>& Bound) UE_STUB_BODY

struct FFloatRangeBound : TRangeBound<float> { using TRangeBound<float>::TRangeBound; FFloatRangeBound(const TRangeBound<float>& Other); };
struct FDoubleRangeBound : TRangeBound<double> { using TRangeBound<double>::TRangeBound; FDoubleRangeBound(const TRangeBound<double>& Other); };
struct FInt32RangeBound : TRangeBound<int32> { using TRangeBound<int32>::TRangeBound; FInt32RangeBound(const TRangeBound<int32>& Other); };
