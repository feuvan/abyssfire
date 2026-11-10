// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Containers/IndirectArray.h (UE 5.x source knowledge).
// TIndirectArray<T>: an array of heap-allocated T that it owns, indexed as T&. Counting, indexing and the range-for
// iterators (TDereferencingIterator over the pointer array, yielding T& / const T&) are declared; mutation is added
// when the module needs it.
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"

template <typename T, typename Allocator = FDefaultAllocator>
class TIndirectArray
{
public:
	typedef T ElementType;
	typedef TArray<void*, Allocator> InternalArrayType;
	TIndirectArray() = default;
	TIndirectArray(TIndirectArray&& Other) = default;
	TIndirectArray(const TIndirectArray& Other) UE_STUB_BODY
	TIndirectArray& operator=(const TIndirectArray& Other) UE_STUB_BODY
	TIndirectArray& operator=(TIndirectArray&& Other) UE_STUB_BODY
	~TIndirectArray() UE_STUB_BODY
	/** Gets number of elements in array. */
	int32 Num() const UE_STUB_BODY
	/** Gets nth item in the array. */
	T& operator[](int32 Index) UE_STUB_BODY
	const T& operator[](int32 Index) const UE_STUB_BODY
	/** Tests if index is in range. */
	bool IsValidIndex(int32 Index) const UE_STUB_BODY
	/** STL-like iterators to enable range-based for loop support (the engine's own bodies). */
	TDereferencingIterator<ElementType, typename InternalArrayType::RangedForIteratorType> begin() { return TDereferencingIterator<ElementType, typename InternalArrayType::RangedForIteratorType>(Array.begin()); }
	TDereferencingIterator<const ElementType, typename InternalArrayType::RangedForConstIteratorType> begin() const { return TDereferencingIterator<const ElementType, typename InternalArrayType::RangedForConstIteratorType>(Array.begin()); }
	TDereferencingIterator<ElementType, typename InternalArrayType::RangedForIteratorType> end() { return TDereferencingIterator<ElementType, typename InternalArrayType::RangedForIteratorType>(Array.end()); }
	TDereferencingIterator<const ElementType, typename InternalArrayType::RangedForConstIteratorType> end() const { return TDereferencingIterator<const ElementType, typename InternalArrayType::RangedForConstIteratorType>(Array.end()); }
private:
	InternalArrayType Array;
};
