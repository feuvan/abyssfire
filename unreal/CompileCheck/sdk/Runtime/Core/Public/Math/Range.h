// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Math/Range.h (from UE 5.x source knowledge). TRange<T>: a
// contiguous set of values between two bounds. FInt32Range / FFloatRange / ... are wrapper structs deriving from the
// template (DEFINE_RANGE_WRAPPER_STRUCT): their value constructors are explicit, the one from TRange<T> is not, so
// "FInt32Range R = FInt32Range::Inclusive(A, B);" works and "FInt32Range R = 5;" does not.
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Math/RangeBound.h"
#include "Misc/DateTime.h"
#include "Templates/TypeHash.h"

template <typename ElementType>
class TRange
{
public:
	typedef TRangeBound<ElementType> BoundsType;
	typedef typename TCallTraits<ElementType>::ParamType ElementValueOrConstRef;
	/** Default constructor: an empty range. */
	TRange() UE_STUB_BODY
	/** A range with a single element. */
	explicit TRange(ElementValueOrConstRef A) UE_STUB_BODY
	/** The range [A, B). */
	explicit TRange(ElementValueOrConstRef A, ElementValueOrConstRef B) UE_STUB_BODY
	explicit TRange(const BoundsType& InLowerBound, const BoundsType& InUpperBound) UE_STUB_BODY
	bool operator==(const TRange& Other) const UE_STUB_BODY
	bool operator!=(const TRange& Other) const UE_STUB_BODY
	bool Adjoins(const TRange& Other) const UE_STUB_BODY
	bool Conjoins(const TRange& X, const TRange& Y) const UE_STUB_BODY
	bool Contains(ElementValueOrConstRef Element) const UE_STUB_BODY
	bool Contains(const TRange& Other) const UE_STUB_BODY
	bool Contiguous(const TRange& Other) const UE_STUB_BODY
	BoundsType GetLowerBound() const UE_STUB_BODY
	void SetLowerBound(const BoundsType& NewLowerBound) UE_STUB_BODY
	void SetLowerBoundValue(ElementValueOrConstRef NewLowerBoundValue) UE_STUB_BODY
	ElementValueOrConstRef GetLowerBoundValue() const UE_STUB_BODY
	BoundsType GetUpperBound() const UE_STUB_BODY
	void SetUpperBound(const BoundsType& NewUpperBound) UE_STUB_BODY
	void SetUpperBoundValue(ElementValueOrConstRef NewUpperBoundValue) UE_STUB_BODY
	ElementValueOrConstRef GetUpperBoundValue() const UE_STUB_BODY
	bool HasLowerBound() const UE_STUB_BODY
	bool HasUpperBound() const UE_STUB_BODY
	bool IsDegenerate() const UE_STUB_BODY
	bool IsEmpty() const UE_STUB_BODY
	bool Overlaps(const TRange& Other) const UE_STUB_BODY
	template <typename DifferenceType> DifferenceType Size() const UE_STUB_BODY
	TArray<TRange> Split(ElementValueOrConstRef Element) const UE_STUB_BODY
	static TArray<TRange> Difference(const TRange& X, const TRange& Y) UE_STUB_BODY
	static TRange Hull(const TRange& X, const TRange& Y) UE_STUB_BODY
	static TRange Hull(const TArray<TRange>& Ranges) UE_STUB_BODY
	static TRange Intersection(const TRange& X, const TRange& Y) UE_STUB_BODY
	static TRange Intersection(const TArray<TRange>& Ranges) UE_STUB_BODY
	static TArray<TRange> Union(const TRange& X, const TRange& Y) UE_STUB_BODY
	/** (-inf, +inf). */
	static TRange All() UE_STUB_BODY
	/** [Value, +inf). */
	static TRange AtLeast(ElementValueOrConstRef Value) UE_STUB_BODY
	/** (-inf, Value]. */
	static TRange AtMost(ElementValueOrConstRef Value) UE_STUB_BODY
	static TRange Empty() UE_STUB_BODY
	/** (Min, Max). */
	static TRange Exclusive(ElementValueOrConstRef Min, ElementValueOrConstRef Max) UE_STUB_BODY
	/** (Value, +inf). */
	static TRange GreaterThan(ElementValueOrConstRef Value) UE_STUB_BODY
	/** [Min, Max]. */
	static TRange Inclusive(ElementValueOrConstRef Min, ElementValueOrConstRef Max) UE_STUB_BODY
	/** (-inf, Value). */
	static TRange LessThan(ElementValueOrConstRef Value) UE_STUB_BODY
};
template <typename ElementType> uint32 GetTypeHash(const TRange<ElementType>& Range) UE_STUB_BODY

#define DEFINE_RANGE_WRAPPER_STRUCT(Name, ElementType) \
	struct Name : TRange<ElementType> \
	{ \
	private: \
		typedef TRange<ElementType> Super; \
	public: \
		Name() : Super() {} \
		Name(const Super& Other) : Super(Other) {} \
		explicit Name(TRange<ElementType>::ElementValueOrConstRef A) : Super(A) {} \
		explicit Name(TRange<ElementType>::ElementValueOrConstRef A, TRange<ElementType>::ElementValueOrConstRef B) : Super(A, B) {} \
		explicit Name(const TRangeBound<ElementType>& InLowerBound, const TRangeBound<ElementType>& InUpperBound) : Super(InLowerBound, InUpperBound) {} \
		TRange<ElementType>& operator=(const Super& Other) { Super::operator=(Other); return *this; } \
	};

DEFINE_RANGE_WRAPPER_STRUCT(FDateRange, FDateTime)
DEFINE_RANGE_WRAPPER_STRUCT(FDoubleRange, double)
DEFINE_RANGE_WRAPPER_STRUCT(FFloatRange, float)
DEFINE_RANGE_WRAPPER_STRUCT(FInt8Range, int8)
DEFINE_RANGE_WRAPPER_STRUCT(FInt16Range, int16)
DEFINE_RANGE_WRAPPER_STRUCT(FInt32Range, int32)
DEFINE_RANGE_WRAPPER_STRUCT(FInt64Range, int64)
