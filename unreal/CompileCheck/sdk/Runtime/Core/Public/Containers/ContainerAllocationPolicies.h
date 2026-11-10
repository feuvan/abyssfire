// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Containers/ContainerAllocationPolicies.h
// (from UE 5.x source knowledge). Allocator tags only: the SDK containers do not allocate.
#pragma once

#include "CoreTypes.h"
#include "Containers/ContainersFwd.h"

template <int IndexSize> class TSizedDefaultAllocator { public: using SizeType = std::conditional_t<IndexSize == 64, int64, int32>; };
template <uint32 NumInlineElements, typename SecondaryAllocator = FDefaultAllocator>
class TInlineAllocator { public: using SizeType = int32; };
template <uint32 NumInlineElements> class TFixedAllocator { public: using SizeType = int32; };
template <uint32 NumInlineElements> class TNonRelocatableInlineAllocator { public: using SizeType = int32; };
template <uint32 Alignment = 16> class TAlignedHeapAllocator { public: using SizeType = int32; };
class FDefaultSetAllocator {};
class FDefaultBitArrayAllocator {};
class FDefaultSparseArrayAllocator {};
template <uint32 NumInlineElements, typename SecondaryAllocator = FDefaultSetAllocator> class TInlineSetAllocator {};
template <uint32 NumInlineElements> class TFixedSetAllocator {};
