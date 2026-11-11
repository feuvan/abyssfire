// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Containers/Array.h (from UE 5.x source knowledge).
// TArray: the member list and signatures of the engine's dynamic array (int32 indices, Num() not size(), Add returns the
// index, Emplace_GetRef / AddDefaulted_GetRef return the element, Remove* return counts, ...). Declarations only.
// The queries (Num, IsEmpty, Contains, Find*, IndexOf*, Last, Top, GetData, ...) are [[nodiscard]] as in the engine; the
// mutating calls whose result is routinely dropped (Add, Emplace, AddUnique, Remove*, Pop) are not.
#pragma once

#include "CoreTypes.h"
#include "Containers/ContainersFwd.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/ArrayView.h"
#include "Misc/AssertionMacros.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/UnrealTypeTraits.h"
#include "Templates/TypeHash.h"
#include "Templates/Less.h"
#include "Templates/Sorting.h"
#include "Containers/AllowShrinking.h"
#include <initializer_list>

/** Iterator over an array of pointers that yields the pointees (TIndirectArray's range-for; the engine's own body). */
template <typename ElementType, typename IteratorType>
struct TDereferencingIterator
{
	explicit TDereferencingIterator(IteratorType InIter) : Iter(InIter) {}
	ElementType& operator*() const { return *(ElementType*)*Iter; }
	TDereferencingIterator& operator++() { ++Iter; return *this; }
private:
	IteratorType Iter;
	friend bool operator!=(const TDereferencingIterator& Lhs, const TDereferencingIterator& Rhs) { return Lhs.Iter != Rhs.Iter; }
};

template <typename InElementType, typename InAllocatorType>
class TArray
{
	template <typename OtherInElementType, typename OtherAllocator> friend class TArray;
public:
	using SizeType = int32;
	using ElementType = InElementType;
	using AllocatorType = InAllocatorType;
	using RangedForIteratorType = ElementType*;
	using RangedForConstIteratorType = const ElementType*;
	using TIterator = TIndexedContainerIterator<TArray, ElementType, SizeType>;
	using TConstIterator = TIndexedContainerIterator<const TArray, const ElementType, SizeType>;

	constexpr TArray() {}
	TArray(const ElementType* Ptr, SizeType Count) UE_STUB_BODY
	template <typename OtherElementType, typename OtherSizeType> requires std::is_constructible_v<InElementType, OtherElementType&>
	explicit TArray(const TArrayView<OtherElementType, OtherSizeType>& Other) UE_STUB_BODY
	TArray(std::initializer_list<InElementType> InitList) requires std::is_copy_constructible_v<InElementType> UE_STUB_BODY
	// The element- / allocator-changing constructors are explicit in the engine whatever the element types: a
	// TArray<T, TInlineAllocator<N>> (or a TInlineComponentArray) is NOT passed where a TArray<T> is expected, and
	// `TArray<float> F = Ints;` does not compile. `TArray<T>(Other)` and the allocator-changing operator= below do.
	template <typename OtherElementType, typename OtherAllocator> requires std::is_constructible_v<InElementType, const OtherElementType&>
	explicit TArray(const TArray<OtherElementType, OtherAllocator>& Other) UE_STUB_BODY
	TArray(const TArray& Other) { static_assert(std::is_copy_constructible_v<InElementType>, "TArray: copying an array of a move-only element type"); }
	TArray(const TArray& Other, SizeType ExtraSlack) { static_assert(std::is_copy_constructible_v<InElementType>, "TArray: copying an array of a move-only element type"); }
	TArray(TArray&& Other) {}
	// MoveOrCopy: the allocation changes hands only between arrays of one type; any other source is copied element by
	// element (CopyToEmpty), so the elements must also be constructible from a const reference.
	template <typename OtherElementType, typename OtherAllocator> requires std::is_constructible_v<InElementType, OtherElementType&&> && std::is_constructible_v<InElementType, const OtherElementType&>
	explicit TArray(TArray<OtherElementType, OtherAllocator>&& Other) UE_STUB_BODY
	~TArray() { static_assert(sizeof(InElementType) > 0, "TArray: the element type is incomplete where the array is destroyed"); }
	TArray& operator=(std::initializer_list<InElementType> InitList) requires std::is_copy_constructible_v<InElementType> UE_STUB_BODY
	TArray& operator=(const TArray& Other) { static_assert(std::is_copy_constructible_v<InElementType>, "TArray: copying an array of a move-only element type"); return *this; }
	TArray& operator=(TArray&& Other) { return *this; }
	// Allocator-changing assignment: copy only (the engine has no allocator-changing move assignment; an rvalue binds here).
	template <typename OtherAllocator>
	TArray& operator=(const TArray<InElementType, OtherAllocator>& Other) requires std::is_copy_constructible_v<InElementType> UE_STUB_BODY
	template <typename OtherElementType, typename OtherSizeType> requires std::is_constructible_v<InElementType, OtherElementType&>
	TArray& operator=(const TArrayView<OtherElementType, OtherSizeType>& Other) UE_STUB_BODY

	[[nodiscard]] ElementType* GetData() UE_LIFETIMEBOUND UE_STUB_BODY
	[[nodiscard]] const ElementType* GetData() const UE_LIFETIMEBOUND UE_STUB_BODY
	[[nodiscard]] static constexpr uint32 GetTypeSize() { return sizeof(ElementType); }
	[[nodiscard]] SIZE_T GetAllocatedSize() const UE_STUB_BODY
	[[nodiscard]] SizeType GetSlack() const UE_STUB_BODY
	[[nodiscard]] bool IsValidIndex(SizeType Index) const UE_STUB_BODY
	[[nodiscard]] bool IsEmpty() const UE_STUB_BODY
	[[nodiscard]] SizeType Num() const UE_STUB_BODY
	[[nodiscard]] SIZE_T NumBytes() const UE_STUB_BODY
	[[nodiscard]] SizeType Max() const UE_STUB_BODY
	ElementType& operator[](SizeType Index) UE_LIFETIMEBOUND UE_STUB_BODY
	const ElementType& operator[](SizeType Index) const UE_LIFETIMEBOUND UE_STUB_BODY
	ElementType Pop(EAllowShrinking AllowShrinking = EAllowShrinking::Yes) UE_STUB_BODY
	void Push(ElementType&& Item) UE_STUB_BODY
	void Push(const ElementType& Item) UE_STUB_BODY
	[[nodiscard]] ElementType& Top() UE_LIFETIMEBOUND UE_STUB_BODY
	[[nodiscard]] const ElementType& Top() const UE_LIFETIMEBOUND UE_STUB_BODY
	[[nodiscard]] ElementType& Last(SizeType IndexFromTheEnd = 0) UE_LIFETIMEBOUND UE_STUB_BODY
	[[nodiscard]] const ElementType& Last(SizeType IndexFromTheEnd = 0) const UE_LIFETIMEBOUND UE_STUB_BODY
	void Shrink() UE_STUB_BODY
	bool Find(const ElementType& Item, SizeType& Index) const requires requires(const ElementType& A, const ElementType& B) { { A == B } -> std::convertible_to<bool>; } UE_STUB_BODY
	[[nodiscard]] SizeType Find(const ElementType& Item) const requires requires(const ElementType& A, const ElementType& B) { { A == B } -> std::convertible_to<bool>; } UE_STUB_BODY
	bool FindLast(const ElementType& Item, SizeType& Index) const requires requires(const ElementType& A, const ElementType& B) { { A == B } -> std::convertible_to<bool>; } UE_STUB_BODY
	[[nodiscard]] SizeType FindLast(const ElementType& Item) const requires requires(const ElementType& A, const ElementType& B) { { A == B } -> std::convertible_to<bool>; } UE_STUB_BODY
	template <typename Predicate> [[nodiscard]] SizeType FindLastByPredicate(Predicate Pred, SizeType Count) const requires std::is_invocable_r_v<bool, Predicate&, const ElementType&> UE_STUB_BODY
	template <typename Predicate> [[nodiscard]] SizeType FindLastByPredicate(Predicate Pred) const requires std::is_invocable_r_v<bool, Predicate&, const ElementType&> UE_STUB_BODY
	template <typename KeyType> [[nodiscard]] SizeType IndexOfByKey(const KeyType& Key) const requires requires(const ElementType& E, const KeyType& K) { { E == K } -> std::convertible_to<bool>; } UE_STUB_BODY
	template <typename Predicate> [[nodiscard]] SizeType IndexOfByPredicate(Predicate Pred) const requires std::is_invocable_r_v<bool, Predicate&, const ElementType&> UE_STUB_BODY
	template <typename KeyType> [[nodiscard]] const ElementType* FindByKey(const KeyType& Key) const requires requires(const ElementType& E, const KeyType& K) { { E == K } -> std::convertible_to<bool>; } UE_STUB_BODY
	template <typename KeyType> [[nodiscard]] ElementType* FindByKey(const KeyType& Key) requires requires(const ElementType& E, const KeyType& K) { { E == K } -> std::convertible_to<bool>; } UE_STUB_BODY
	template <typename Predicate> [[nodiscard]] const ElementType* FindByPredicate(Predicate Pred) const requires std::is_invocable_r_v<bool, Predicate&, const ElementType&> UE_STUB_BODY
	template <typename Predicate> [[nodiscard]] ElementType* FindByPredicate(Predicate Pred) requires std::is_invocable_r_v<bool, Predicate&, ElementType&> UE_STUB_BODY
	template <typename Predicate> [[nodiscard]] TArray<ElementType> FilterByPredicate(Predicate Pred) const requires std::is_invocable_r_v<bool, Predicate&, const ElementType&> UE_STUB_BODY
	template <typename ComparisonType> [[nodiscard]] bool Contains(const ComparisonType& Item) const requires requires(const ElementType& E, const ComparisonType& K) { { E == K } -> std::convertible_to<bool>; } UE_STUB_BODY
	template <typename Predicate> [[nodiscard]] bool ContainsByPredicate(Predicate Pred) const requires std::is_invocable_r_v<bool, Predicate&, const ElementType&> UE_STUB_BODY
	bool operator==(const TArray& OtherArray) const requires requires(const ElementType& A, const ElementType& B) { { A == B } -> std::convertible_to<bool>; } UE_STUB_BODY
	bool operator!=(const TArray& OtherArray) const requires requires(const ElementType& A, const ElementType& B) { { A == B } -> std::convertible_to<bool>; } UE_STUB_BODY

	SizeType AddUninitialized() UE_STUB_BODY
	SizeType AddUninitialized(SizeType Count) UE_STUB_BODY
	void InsertUninitialized(SizeType Index) UE_STUB_BODY
	void InsertUninitialized(SizeType Index, SizeType Count) UE_STUB_BODY
	void InsertZeroed(SizeType Index) UE_STUB_BODY
	void InsertZeroed(SizeType Index, SizeType Count) UE_STUB_BODY
	ElementType& InsertZeroed_GetRef(SizeType Index) UE_LIFETIMEBOUND UE_STUB_BODY
	void InsertDefaulted(SizeType Index) UE_STUB_BODY
	void InsertDefaulted(SizeType Index, SizeType Count) UE_STUB_BODY
	ElementType& InsertDefaulted_GetRef(SizeType Index) UE_LIFETIMEBOUND UE_STUB_BODY
	SizeType Insert(std::initializer_list<ElementType> InitList, const SizeType InIndex) UE_STUB_BODY
	template <typename OtherAllocator> SizeType Insert(const TArray<ElementType, OtherAllocator>& Items, const SizeType InIndex) UE_STUB_BODY
	template <typename OtherAllocator> SizeType Insert(TArray<ElementType, OtherAllocator>&& Items, const SizeType InIndex) UE_STUB_BODY
	SizeType Insert(const ElementType* Ptr, SizeType Count, SizeType Index) UE_STUB_BODY
	SizeType Insert(ElementType&& Item, SizeType Index) UE_STUB_BODY
	SizeType Insert(const ElementType& Item, SizeType Index) requires std::is_copy_constructible_v<InElementType> UE_STUB_BODY
	ElementType& Insert_GetRef(ElementType&& Item, SizeType Index) UE_LIFETIMEBOUND UE_STUB_BODY
	ElementType& Insert_GetRef(const ElementType& Item, SizeType Index) UE_LIFETIMEBOUND UE_STUB_BODY
	void RemoveAt(SizeType Index, EAllowShrinking AllowShrinking = EAllowShrinking::Yes) UE_STUB_BODY
	template <typename CountType> requires std::is_integral_v<CountType> && (!std::is_same_v<CountType, bool>)
	void RemoveAt(SizeType Index, CountType Count, EAllowShrinking AllowShrinking = EAllowShrinking::Yes) UE_STUB_BODY
	void RemoveAtSwap(SizeType Index, EAllowShrinking AllowShrinking = EAllowShrinking::Yes) UE_STUB_BODY
	template <typename CountType> requires std::is_integral_v<CountType> && (!std::is_same_v<CountType, bool>)
	void RemoveAtSwap(SizeType Index, CountType Count, EAllowShrinking AllowShrinking = EAllowShrinking::Yes) UE_STUB_BODY
	void Reset(SizeType NewSize = 0) UE_STUB_BODY
	void Empty(SizeType Slack = 0) UE_STUB_BODY
	void SetNum(SizeType NewNum, EAllowShrinking AllowShrinking = EAllowShrinking::Yes) requires std::is_default_constructible_v<InElementType> UE_STUB_BODY
	void SetNumZeroed(SizeType NewNum, EAllowShrinking AllowShrinking = EAllowShrinking::Yes) UE_STUB_BODY
	void SetNumUninitialized(SizeType NewNum, EAllowShrinking AllowShrinking = EAllowShrinking::Yes) UE_STUB_BODY
	void SetNumUnsafeInternal(SizeType NewNum) UE_STUB_BODY
	template <typename OtherElementType, typename OtherAllocatorType> requires std::is_constructible_v<InElementType, const OtherElementType&>
	void Append(const TArray<OtherElementType, OtherAllocatorType>& Source) UE_STUB_BODY
	template <typename OtherElementType, typename OtherAllocator> requires std::is_constructible_v<InElementType, OtherElementType&&>
	void Append(TArray<OtherElementType, OtherAllocator>&& Source) UE_STUB_BODY
	template <typename RangeType>
		requires (!bool(TIsTArray<std::remove_reference_t<RangeType>>::Value)) && requires(RangeType&& R) { { UE::Stub::RangeData(R) } -> std::convertible_to<const InElementType*>; UE::Stub::RangeNum(R); }
	void Append(RangeType&& Source) UE_STUB_BODY
	void Append(const ElementType* Ptr, SizeType Count) UE_STUB_BODY
	void Append(std::initializer_list<ElementType> InitList) UE_STUB_BODY
	TArray& operator+=(TArray&& Other) UE_STUB_BODY
	TArray& operator+=(const TArray& Other) UE_STUB_BODY
	TArray& operator+=(std::initializer_list<ElementType> InitList) UE_STUB_BODY
	template <typename... ArgsType> requires std::is_constructible_v<InElementType, ArgsType&&...>
	SizeType Emplace(ArgsType&&... Args) UE_STUB_BODY
	template <typename... ArgsType> requires std::is_constructible_v<InElementType, ArgsType&&...>
	ElementType& Emplace_GetRef(ArgsType&&... Args) UE_LIFETIMEBOUND UE_STUB_BODY
	template <typename... ArgsType> requires std::is_constructible_v<InElementType, ArgsType&&...>
	void EmplaceAt(SizeType Index, ArgsType&&... Args) UE_STUB_BODY
	template <typename... ArgsType> requires std::is_constructible_v<InElementType, ArgsType&&...>
	ElementType& EmplaceAt_GetRef(SizeType Index, ArgsType&&... Args) UE_LIFETIMEBOUND UE_STUB_BODY
	SizeType Add(ElementType&& Item) UE_STUB_BODY
	SizeType Add(const ElementType& Item) requires std::is_copy_constructible_v<InElementType> UE_STUB_BODY
	ElementType& Add_GetRef(ElementType&& Item) UE_LIFETIMEBOUND UE_STUB_BODY
	ElementType& Add_GetRef(const ElementType& Item) UE_LIFETIMEBOUND requires std::is_copy_constructible_v<InElementType> UE_STUB_BODY
	SizeType AddZeroed() UE_STUB_BODY
	SizeType AddZeroed(SizeType Count) UE_STUB_BODY
	ElementType& AddZeroed_GetRef() UE_LIFETIMEBOUND UE_STUB_BODY
	SizeType AddDefaulted() requires std::is_default_constructible_v<InElementType> UE_STUB_BODY
	SizeType AddDefaulted(SizeType Count) requires std::is_default_constructible_v<InElementType> UE_STUB_BODY
	ElementType& AddDefaulted_GetRef() UE_LIFETIMEBOUND requires std::is_default_constructible_v<InElementType> UE_STUB_BODY
	SizeType AddUnique(ElementType&& Item) requires requires(const ElementType& A, const ElementType& B) { { A == B } -> std::convertible_to<bool>; } UE_STUB_BODY
	SizeType AddUnique(const ElementType& Item) requires requires(const ElementType& A, const ElementType& B) { { A == B } -> std::convertible_to<bool>; } UE_STUB_BODY
	void Reserve(SizeType Number) UE_STUB_BODY
	void Init(const ElementType& Element, SizeType Number) UE_STUB_BODY
	SizeType RemoveSingle(const ElementType& Item) requires requires(const ElementType& A, const ElementType& B) { { A == B } -> std::convertible_to<bool>; } UE_STUB_BODY
	SizeType Remove(const ElementType& Item) requires requires(const ElementType& A, const ElementType& B) { { A == B } -> std::convertible_to<bool>; } UE_STUB_BODY
	template <class PREDICATE_CLASS> SizeType RemoveAll(const PREDICATE_CLASS& Predicate) requires std::is_invocable_r_v<bool, const PREDICATE_CLASS&, ElementType&> UE_STUB_BODY
	template <class PREDICATE_CLASS> SizeType RemoveAllSwap(const PREDICATE_CLASS& Predicate, EAllowShrinking AllowShrinking = EAllowShrinking::Yes) requires std::is_invocable_r_v<bool, const PREDICATE_CLASS&, ElementType&> UE_STUB_BODY
	SizeType RemoveSingleSwap(const ElementType& Item, EAllowShrinking AllowShrinking = EAllowShrinking::Yes) requires requires(const ElementType& A, const ElementType& B) { { A == B } -> std::convertible_to<bool>; } UE_STUB_BODY
	SizeType RemoveSwap(const ElementType& Item, EAllowShrinking AllowShrinking = EAllowShrinking::Yes) requires requires(const ElementType& A, const ElementType& B) { { A == B } -> std::convertible_to<bool>; } UE_STUB_BODY
	void SwapMemory(SizeType FirstIndexToSwap, SizeType SecondIndexToSwap) UE_STUB_BODY
	void Swap(SizeType FirstIndexToSwap, SizeType SecondIndexToSwap) UE_STUB_BODY
	template <typename SearchType> bool FindItemByClass(SearchType** Item = nullptr, SizeType* ItemIndex = nullptr, SizeType StartIndex = 0) const UE_STUB_BODY

	[[nodiscard]] TIterator CreateIterator() UE_STUB_BODY
	[[nodiscard]] TConstIterator CreateConstIterator() const UE_STUB_BODY
	RangedForIteratorType begin() UE_STUB_BODY
	RangedForConstIteratorType begin() const UE_STUB_BODY
	RangedForIteratorType end() UE_STUB_BODY
	RangedForConstIteratorType end() const UE_STUB_BODY

	// Sorting and the heap go through TDereferenceWrapper (Templates/Sorting.h): an array of raw pointers hands the
	// predicate the pointees (T&), and Sort() / StableSort() compare the pointees with operator<.
	void Sort() requires UE::Stub::CSortable<InElementType> UE_STUB_BODY
	template <class PREDICATE_CLASS> void Sort(const PREDICATE_CLASS& Predicate) requires UE::Stub::CSortPredicate<PREDICATE_CLASS, InElementType> UE_STUB_BODY
	void StableSort() requires UE::Stub::CSortable<InElementType> UE_STUB_BODY
	template <class PREDICATE_CLASS> void StableSort(const PREDICATE_CLASS& Predicate) requires UE::Stub::CSortPredicate<PREDICATE_CLASS, InElementType> UE_STUB_BODY
	template <class PREDICATE_CLASS> void Heapify(const PREDICATE_CLASS& Predicate) requires UE::Stub::CSortPredicate<PREDICATE_CLASS, InElementType> UE_STUB_BODY
	void Heapify() requires UE::Stub::CHeapOrdered<InElementType> UE_STUB_BODY
	template <class PREDICATE_CLASS> SizeType HeapPush(ElementType&& InItem, const PREDICATE_CLASS& Predicate) requires UE::Stub::CSortPredicate<PREDICATE_CLASS, InElementType> UE_STUB_BODY
	template <class PREDICATE_CLASS> SizeType HeapPush(const ElementType& InItem, const PREDICATE_CLASS& Predicate) requires UE::Stub::CSortPredicate<PREDICATE_CLASS, InElementType> UE_STUB_BODY
	SizeType HeapPush(ElementType&& InItem) requires UE::Stub::CHeapOrdered<InElementType> UE_STUB_BODY
	SizeType HeapPush(const ElementType& InItem) requires UE::Stub::CHeapOrdered<InElementType> UE_STUB_BODY
	template <class PREDICATE_CLASS> void HeapPop(ElementType& OutItem, const PREDICATE_CLASS& Predicate, EAllowShrinking AllowShrinking = EAllowShrinking::Yes) requires UE::Stub::CSortPredicate<PREDICATE_CLASS, InElementType> UE_STUB_BODY
	void HeapPop(ElementType& OutItem, EAllowShrinking AllowShrinking = EAllowShrinking::Yes) requires UE::Stub::CHeapOrdered<InElementType> UE_STUB_BODY
	[[nodiscard]] const ElementType& HeapTop() const UE_LIFETIMEBOUND UE_STUB_BODY
	[[nodiscard]] ElementType& HeapTop() UE_LIFETIMEBOUND UE_STUB_BODY
};
template <typename T, typename AllocatorType> struct TIsContiguousContainer<TArray<T, AllocatorType>> { enum { Value = true }; };
template <typename T, typename AllocatorType> struct TIsZeroConstructType<TArray<T, AllocatorType>> { enum { Value = true }; };
template <typename InElementType, typename AllocatorType> uint32 GetTypeHash(const TArray<InElementType, AllocatorType>& A) UE_STUB_BODY

/** TInlineComponentArray lives in Engine (GameFramework/Actor.h); TArray<T, TInlineAllocator<N>> is the Core spelling. */
