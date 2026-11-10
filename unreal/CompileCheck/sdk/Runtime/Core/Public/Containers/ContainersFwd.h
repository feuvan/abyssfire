// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Containers/ContainersFwd.h (from UE 5.x source knowledge).
#pragma once

#include "HAL/Platform.h"
#include "Containers/StringFwd.h"

template <int IndexSize> class TSizedDefaultAllocator;
using FDefaultAllocator = TSizedDefaultAllocator<32>;
using FDefaultAllocator64 = TSizedDefaultAllocator<64>;
class FDefaultSetAllocator;

template <typename T, typename Allocator = FDefaultAllocator> class TArray;
template <typename T> using TArray64 = TArray<T, FDefaultAllocator64>;
template <typename T, typename SizeType = int32> class TArrayView;
template <typename T, typename SizeType = int32> using TConstArrayView = TArrayView<const T, SizeType>;
template <typename T> class TTransArray;
template <typename KeyType, typename ValueType, bool bInAllowDuplicateKeys> struct TDefaultMapHashableKeyFuncs;
template <typename KeyType, typename ValueType, typename SetAllocator = FDefaultSetAllocator,
	typename KeyFuncs = TDefaultMapHashableKeyFuncs<KeyType, ValueType, false>> class TMap;
template <typename KeyType, typename ValueType, typename SetAllocator = FDefaultSetAllocator,
	typename KeyFuncs = TDefaultMapHashableKeyFuncs<KeyType, ValueType, true>> class TMultiMap;
template <typename KeyType, typename ValueType, typename ArrayAllocator = FDefaultAllocator, typename SortPredicate = void>
class TSortedMap;
template <typename ElementType, bool bInAllowDuplicateKeys = false> struct DefaultKeyFuncs;
template <typename InElementType, typename KeyFuncs = DefaultKeyFuncs<InElementType>,
	typename Allocator = FDefaultSetAllocator> class TSet;
template <typename InElementType, typename InSizeType = int32> class TStridedView;
