// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Containers/IndirectArray.h (UE 5.x source knowledge).
// TIndirectArray<T>: an array of heap-allocated T that it owns, indexed as T&. Only counting and indexing are declared
// (what the module does with FSkeletalMeshRenderData::LODRenderData); iteration and mutation are added when needed.
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
private:
	InternalArrayType Array;
};
