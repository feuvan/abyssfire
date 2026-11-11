// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Containers/Set.h (from UE 5.x source knowledge).
// TSet: hashed set. Every hashing member requires GetTypeHash(Element) and operator== (DefaultKeyFuncs).
#pragma once

#include "CoreTypes.h"
#include "Containers/ContainersFwd.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Array.h"
#include "Templates/TypeHash.h"
#include "Templates/UnrealTemplate.h"
#include <initializer_list>

class FSetElementId
{
public:
	FSetElementId() = default;
	bool IsValidId() const;
	int32 AsInteger() const;
	static FSetElementId FromInteger(int32 Integer);
	bool operator==(const FSetElementId&) const = default;
};

template <typename ElementType, bool bInAllowDuplicateKeys>
struct DefaultKeyFuncs
{
	using KeyType = ElementType;
	using KeyInitType = typename TCallTraits<ElementType>::ParamType;
	using ElementInitType = typename TCallTraits<ElementType>::ParamType;
	enum { bAllowDuplicateKeys = bInAllowDuplicateKeys };
};

template <typename InElementType, typename KeyFuncs, typename Allocator>
class TSet
{
public:
	using ElementType = InElementType;
	using KeyFuncsType = KeyFuncs;
	using AllocatorType = Allocator;
	using KeyInitType = typename TCallTraits<InElementType>::ParamType;

	class TConstIterator
	{
	public:
		TConstIterator& operator++() UE_STUB_BODY
		explicit operator bool() const UE_STUB_BODY
		bool operator==(const TConstIterator&) const UE_STUB_BODY
		bool operator!=(const TConstIterator&) const UE_STUB_BODY
		const ElementType* operator->() const UE_STUB_BODY
		const ElementType& operator*() const UE_STUB_BODY
		FSetElementId GetId() const UE_STUB_BODY
	};
	class TIterator
	{
	public:
		TIterator& operator++() UE_STUB_BODY
		explicit operator bool() const UE_STUB_BODY
		bool operator==(const TIterator&) const UE_STUB_BODY
		bool operator!=(const TIterator&) const UE_STUB_BODY
		ElementType* operator->() const UE_STUB_BODY
		ElementType& operator*() const UE_STUB_BODY
		FSetElementId GetId() const UE_STUB_BODY
		void RemoveCurrent() UE_STUB_BODY
	};

	TSet() {}
	TSet(const TSet& Copy) { static_assert(std::is_copy_constructible_v<InElementType>, "TSet: copying a set of a move-only element type"); }
	TSet(TSet&& Other) {}
	explicit TSet(TArrayView<const ElementType> InArrayView) UE_STUB_BODY
	explicit TSet(TArray<ElementType>&& InArray) UE_STUB_BODY
	TSet(std::initializer_list<ElementType> InitList) requires UE::Stub::CHashableKey<InElementType> UE_STUB_BODY
	~TSet() { static_assert(sizeof(InElementType) > 0, "TSet: the element type is incomplete where the set is destroyed"); }
	TSet& operator=(const TSet& Copy) { static_assert(std::is_copy_constructible_v<InElementType>, "TSet: copying a set of a move-only element type"); return *this; }
	TSet& operator=(TSet&& Other) { return *this; }
	TSet& operator=(std::initializer_list<ElementType> InitList) UE_STUB_BODY

	void Empty(int32 ExpectedNumElements = 0) UE_STUB_BODY
	void Reset() UE_STUB_BODY
	void Shrink() UE_STUB_BODY
	void Compact() UE_STUB_BODY
	void Reserve(int32 Number) UE_STUB_BODY
	[[nodiscard]] bool IsEmpty() const UE_STUB_BODY
	[[nodiscard]] int32 Num() const UE_STUB_BODY
	[[nodiscard]] int32 GetMaxIndex() const UE_STUB_BODY
	[[nodiscard]] bool IsValidId(FSetElementId Id) const UE_STUB_BODY
	ElementType& operator[](FSetElementId Id) UE_STUB_BODY
	const ElementType& operator[](FSetElementId Id) const UE_STUB_BODY
	FSetElementId Add(const InElementType& InElement, bool* bIsAlreadyInSetPtr = nullptr) requires UE::Stub::CHashableKey<InElementType> UE_STUB_BODY
	FSetElementId Add(InElementType&& InElement, bool* bIsAlreadyInSetPtr = nullptr) requires UE::Stub::CHashableKey<InElementType> UE_STUB_BODY
	ElementType& FindOrAdd(const InElementType& InElement, bool* bIsAlreadyInSetPtr = nullptr) requires UE::Stub::CHashableKey<InElementType> UE_STUB_BODY
	ElementType& FindOrAdd(InElementType&& InElement, bool* bIsAlreadyInSetPtr = nullptr) requires UE::Stub::CHashableKey<InElementType> UE_STUB_BODY
	template <typename ArgsType> requires UE::Stub::CHashableKey<InElementType> && std::is_constructible_v<InElementType, ArgsType&&>
	FSetElementId Emplace(ArgsType&& Args, bool* bIsAlreadyInSetPtr = nullptr) UE_STUB_BODY
	template <typename ArrayAllocator> void Append(const TArray<ElementType, ArrayAllocator>& InElements) requires UE::Stub::CHashableKey<InElementType> UE_STUB_BODY
	template <typename ArrayAllocator> void Append(TArray<ElementType, ArrayAllocator>&& InElements) requires UE::Stub::CHashableKey<InElementType> UE_STUB_BODY
	void Append(TArrayView<const ElementType> InElements) requires UE::Stub::CHashableKey<InElementType> UE_STUB_BODY
	template <typename OtherKeyFuncs, typename OtherAllocator> void Append(const TSet<ElementType, OtherKeyFuncs, OtherAllocator>& OtherSet) UE_STUB_BODY
	void Append(std::initializer_list<ElementType> InitList) requires UE::Stub::CHashableKey<InElementType> UE_STUB_BODY
	void Remove(FSetElementId ElementId) UE_STUB_BODY
	[[nodiscard]] FSetElementId FindId(KeyInitType Key) const requires UE::Stub::CHashableKey<InElementType> UE_STUB_BODY
	[[nodiscard]] ElementType* Find(KeyInitType Key) requires UE::Stub::CHashableKey<InElementType> UE_STUB_BODY
	[[nodiscard]] const ElementType* Find(KeyInitType Key) const requires UE::Stub::CHashableKey<InElementType> UE_STUB_BODY
	int32 Remove(KeyInitType Key) requires UE::Stub::CHashableKey<InElementType> UE_STUB_BODY
	[[nodiscard]] bool Contains(KeyInitType Key) const requires UE::Stub::CHashableKey<InElementType> UE_STUB_BODY
	// FElementCompareClass wraps the predicate in TDereferenceWrapper (Templates/Sorting.h): as TArray::Sort.
	void Sort() requires UE::Stub::CSortable<InElementType> UE_STUB_BODY
	template <typename PREDICATE_CLASS> void Sort(const PREDICATE_CLASS& Predicate) requires UE::Stub::CSortPredicate<PREDICATE_CLASS, InElementType> UE_STUB_BODY
	template <typename PREDICATE_CLASS> void StableSort(const PREDICATE_CLASS& Predicate) requires UE::Stub::CSortPredicate<PREDICATE_CLASS, InElementType> UE_STUB_BODY
	[[nodiscard]] TSet Intersect(const TSet& OtherSet) const UE_STUB_BODY
	[[nodiscard]] TSet Union(const TSet& OtherSet) const UE_STUB_BODY
	[[nodiscard]] TSet Difference(const TSet& OtherSet) const UE_STUB_BODY
	[[nodiscard]] bool Includes(const TSet& OtherSet) const UE_STUB_BODY
	[[nodiscard]] TArray<ElementType> Array() const UE_STUB_BODY
	[[nodiscard]] TIterator CreateIterator() UE_STUB_BODY
	[[nodiscard]] TConstIterator CreateConstIterator() const UE_STUB_BODY
	struct FRangedForIterator { ElementType& operator*() const UE_STUB_BODY FRangedForIterator& operator++() UE_STUB_BODY bool operator!=(const FRangedForIterator&) const UE_STUB_BODY };
	struct FRangedForConstIterator { const ElementType& operator*() const UE_STUB_BODY FRangedForConstIterator& operator++() UE_STUB_BODY bool operator!=(const FRangedForConstIterator&) const UE_STUB_BODY };
	FRangedForIterator begin() UE_STUB_BODY
	FRangedForConstIterator begin() const UE_STUB_BODY
	FRangedForIterator end() UE_STUB_BODY
	FRangedForConstIterator end() const UE_STUB_BODY
};
