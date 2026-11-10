// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Containers/Map.h (from UE 5.x source knowledge).
// TMap / TMultiMap: Add returns the value reference, Find returns a pointer (null when absent), FindRef a copy, Remove
// the number removed; iteration yields TPair<Key, Value> (.Key / .Value). The key needs GetTypeHash and operator==.
#pragma once

#include "CoreTypes.h"
#include "Containers/ContainersFwd.h"
#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Templates/Tuple.h"
#include "Templates/TypeHash.h"
#include "Templates/UnrealTemplate.h"
#include <initializer_list>

template <typename KeyType, typename ValueType, bool bInAllowDuplicateKeys>
struct TDefaultMapHashableKeyFuncs {};

template <typename InKeyType, typename InValueType, typename SetAllocator, typename KeyFuncs>
class TMapBase
{
public:
	using KeyType = InKeyType;
	using ValueType = InValueType;
	using ElementType = TPair<InKeyType, InValueType>;
	using KeyConstPointerType = typename TTypeTraits<InKeyType>::ConstPointerType;
	using KeyInitType = typename TTypeTraits<InKeyType>::ConstInitType;
	using ValueInitType = typename TTypeTraits<InValueType>::ConstInitType;

	class TIterator
	{
	public:
		TIterator& operator++() UE_STUB_BODY
		explicit operator bool() const UE_STUB_BODY
		bool operator==(const TIterator&) const UE_STUB_BODY
		bool operator!=(const TIterator&) const UE_STUB_BODY
		const KeyType& Key() const UE_STUB_BODY
		ValueType& Value() const UE_STUB_BODY
		ElementType& operator*() const UE_STUB_BODY
		ElementType* operator->() const UE_STUB_BODY
		void RemoveCurrent() UE_STUB_BODY
		FSetElementId GetId() const UE_STUB_BODY
	};
	class TConstIterator
	{
	public:
		TConstIterator& operator++() UE_STUB_BODY
		explicit operator bool() const UE_STUB_BODY
		bool operator==(const TConstIterator&) const UE_STUB_BODY
		bool operator!=(const TConstIterator&) const UE_STUB_BODY
		const KeyType& Key() const UE_STUB_BODY
		const ValueType& Value() const UE_STUB_BODY
		const ElementType& operator*() const UE_STUB_BODY
		const ElementType* operator->() const UE_STUB_BODY
		FSetElementId GetId() const UE_STUB_BODY
	};

	TMapBase() {}
	TMapBase(const TMapBase&) { static_assert(std::is_copy_constructible_v<InKeyType> && std::is_copy_constructible_v<InValueType>, "TMap: copying a map with a move-only key or value type"); }
	TMapBase(TMapBase&&) {}
	~TMapBase() { static_assert(sizeof(InKeyType) > 0 && sizeof(InValueType) > 0, "TMap: the key or value type is incomplete where the map is destroyed"); }
	TMapBase& operator=(const TMapBase&) { static_assert(std::is_copy_constructible_v<InKeyType> && std::is_copy_constructible_v<InValueType>, "TMap: copying a map with a move-only key or value type"); return *this; }
	TMapBase& operator=(TMapBase&&) { return *this; }

	bool OrderIndependentCompareEqual(const TMapBase& Other) const UE_STUB_BODY
	void Empty(int32 ExpectedNumElements = 0) UE_STUB_BODY
	void Reset() UE_STUB_BODY
	void Shrink() UE_STUB_BODY
	void Compact() UE_STUB_BODY
	void CompactStable() UE_STUB_BODY
	void Reserve(int32 Number) UE_STUB_BODY
	bool IsEmpty() const UE_STUB_BODY
	int32 Num() const UE_STUB_BODY
	template <typename Allocator> int32 GetKeys(TArray<KeyType, Allocator>& OutKeys) const UE_STUB_BODY
	template <typename Allocator> int32 GetKeys(TSet<KeyType, DefaultKeyFuncs<KeyType>, Allocator>& OutKeys) const UE_STUB_BODY
	SIZE_T GetAllocatedSize() const UE_STUB_BODY

	ValueType& Add(const KeyType& InKey, const ValueType& InValue) requires UE::Stub::CHashableKey<InKeyType> UE_STUB_BODY
	ValueType& Add(const KeyType& InKey, ValueType&& InValue) requires UE::Stub::CHashableKey<InKeyType> UE_STUB_BODY
	ValueType& Add(KeyType&& InKey, const ValueType& InValue) requires UE::Stub::CHashableKey<InKeyType> UE_STUB_BODY
	ValueType& Add(KeyType&& InKey, ValueType&& InValue) requires UE::Stub::CHashableKey<InKeyType> UE_STUB_BODY
	ValueType& Add(const KeyType& InKey) requires UE::Stub::CHashableKey<InKeyType> && std::is_default_constructible_v<InValueType> UE_STUB_BODY
	ValueType& Add(KeyType&& InKey) requires UE::Stub::CHashableKey<InKeyType> && std::is_default_constructible_v<InValueType> UE_STUB_BODY
	ValueType& Add(const TTuple<KeyType, ValueType>& InKeyValue) requires UE::Stub::CHashableKey<InKeyType> UE_STUB_BODY
	ValueType& Add(TTuple<KeyType, ValueType>&& InKeyValue) requires UE::Stub::CHashableKey<InKeyType> UE_STUB_BODY
	template <typename InitKeyType = KeyType, typename InitValueType = ValueType>
		requires UE::Stub::CHashableKey<InKeyType> && std::is_constructible_v<InKeyType, InitKeyType&&> && std::is_constructible_v<InValueType, InitValueType&&>
	ValueType& Emplace(InitKeyType&& InKey, InitValueType&& InValue) UE_STUB_BODY
	template <typename InitKeyType = KeyType>
		requires UE::Stub::CHashableKey<InKeyType> && std::is_constructible_v<InKeyType, InitKeyType&&> && std::is_default_constructible_v<InValueType>
	ValueType& Emplace(InitKeyType&& InKey) UE_STUB_BODY
	int32 Remove(KeyConstPointerType InKey) requires UE::Stub::CHashableKey<InKeyType> UE_STUB_BODY
	const KeyType* FindKey(ValueInitType Value) const requires requires(const ValueType& A, const ValueType& B) { { A == B } -> std::convertible_to<bool>; } UE_STUB_BODY
	template <typename Predicate> TMap<KeyType, ValueType> FilterByPredicate(Predicate Pred) const requires std::is_invocable_r_v<bool, Predicate&, const ElementType&> UE_STUB_BODY
	ValueType* Find(KeyConstPointerType Key) requires UE::Stub::CHashableKey<InKeyType> UE_STUB_BODY
	const ValueType* Find(KeyConstPointerType Key) const requires UE::Stub::CHashableKey<InKeyType> UE_STUB_BODY
	ValueType& FindOrAdd(const KeyType& Key) requires UE::Stub::CHashableKey<InKeyType> && std::is_default_constructible_v<InValueType> UE_STUB_BODY
	ValueType& FindOrAdd(KeyType&& Key) requires UE::Stub::CHashableKey<InKeyType> && std::is_default_constructible_v<InValueType> UE_STUB_BODY
	ValueType& FindOrAdd(const KeyType& Key, const ValueType& Value) requires UE::Stub::CHashableKey<InKeyType> UE_STUB_BODY
	ValueType& FindOrAdd(const KeyType& Key, ValueType&& Value) requires UE::Stub::CHashableKey<InKeyType> UE_STUB_BODY
	ValueType& FindOrAdd(KeyType&& Key, const ValueType& Value) requires UE::Stub::CHashableKey<InKeyType> UE_STUB_BODY
	ValueType& FindOrAdd(KeyType&& Key, ValueType&& Value) requires UE::Stub::CHashableKey<InKeyType> UE_STUB_BODY
	const ValueType& FindChecked(KeyConstPointerType Key) const requires UE::Stub::CHashableKey<InKeyType> UE_STUB_BODY
	ValueType& FindChecked(KeyConstPointerType Key) requires UE::Stub::CHashableKey<InKeyType> UE_STUB_BODY
	ValueType FindRef(KeyConstPointerType Key) const requires UE::Stub::CHashableKey<InKeyType> && std::is_default_constructible_v<InValueType> UE_STUB_BODY
	ValueType FindRef(KeyConstPointerType Key, ValueType DefaultValue) const requires UE::Stub::CHashableKey<InKeyType> UE_STUB_BODY
	bool Contains(KeyConstPointerType Key) const requires UE::Stub::CHashableKey<InKeyType> UE_STUB_BODY
	template <typename Allocator> void GenerateKeyArray(TArray<KeyType, Allocator>& OutArray) const UE_STUB_BODY
	template <typename Allocator> void GenerateValueArray(TArray<ValueType, Allocator>& OutArray) const UE_STUB_BODY
	TArray<ElementType> Array() const UE_STUB_BODY
	TIterator CreateIterator() UE_STUB_BODY
	TConstIterator CreateConstIterator() const UE_STUB_BODY
	struct FRangedForIterator { ElementType& operator*() const UE_STUB_BODY FRangedForIterator& operator++() UE_STUB_BODY bool operator!=(const FRangedForIterator&) const UE_STUB_BODY };
	struct FRangedForConstIterator { const ElementType& operator*() const UE_STUB_BODY FRangedForConstIterator& operator++() UE_STUB_BODY bool operator!=(const FRangedForConstIterator&) const UE_STUB_BODY };
	FRangedForIterator begin() UE_STUB_BODY
	FRangedForConstIterator begin() const UE_STUB_BODY
	FRangedForIterator end() UE_STUB_BODY
	FRangedForConstIterator end() const UE_STUB_BODY
	// FKeyComparisonClass / FValueComparisonClass wrap the predicate in TDereferenceWrapper (Templates/Sorting.h): raw
	// pointer keys / values reach it dereferenced, as in TArray::Sort.
	template <typename PREDICATE_CLASS> void KeySort(const PREDICATE_CLASS& Predicate) requires UE::Stub::CSortPredicate<PREDICATE_CLASS, InKeyType> UE_STUB_BODY
	template <typename PREDICATE_CLASS> void KeyStableSort(const PREDICATE_CLASS& Predicate) requires UE::Stub::CSortPredicate<PREDICATE_CLASS, InKeyType> UE_STUB_BODY
	template <typename PREDICATE_CLASS> void ValueSort(const PREDICATE_CLASS& Predicate) requires UE::Stub::CSortPredicate<PREDICATE_CLASS, InValueType> UE_STUB_BODY
	template <typename PREDICATE_CLASS> void ValueStableSort(const PREDICATE_CLASS& Predicate) requires UE::Stub::CSortPredicate<PREDICATE_CLASS, InValueType> UE_STUB_BODY
};

template <typename InKeyType, typename InValueType, typename SetAllocator, typename KeyFuncs>
class TMap : public TMapBase<InKeyType, InValueType, SetAllocator, KeyFuncs>
{
	using Super = TMapBase<InKeyType, InValueType, SetAllocator, KeyFuncs>;
public:
	using typename Super::KeyType;
	using typename Super::ValueType;
	using typename Super::KeyConstPointerType;
	TMap() = default;
	TMap(TMap&&) = default;
	TMap(const TMap&) = default;
	TMap& operator=(TMap&&) = default;
	TMap& operator=(const TMap&) = default;
	TMap(std::initializer_list<TPair<InKeyType, InValueType>> InitList) requires UE::Stub::CHashableKey<InKeyType> UE_STUB_BODY
	TMap& operator=(std::initializer_list<TPair<InKeyType, InValueType>> InitList) UE_STUB_BODY
	bool RemoveAndCopyValue(typename Super::KeyInitType Key, ValueType& OutRemovedValue) UE_STUB_BODY
	ValueType FindAndRemoveChecked(KeyConstPointerType Key) UE_STUB_BODY
	template <typename OtherSetAllocator> void Append(TMap<InKeyType, InValueType, OtherSetAllocator, KeyFuncs>&& OtherMap) UE_STUB_BODY
	template <typename OtherSetAllocator> void Append(const TMap<InKeyType, InValueType, OtherSetAllocator, KeyFuncs>& OtherMap) UE_STUB_BODY
	ValueType& operator[](KeyConstPointerType Key) requires UE::Stub::CHashableKey<InKeyType> UE_STUB_BODY
	const ValueType& operator[](KeyConstPointerType Key) const requires UE::Stub::CHashableKey<InKeyType> UE_STUB_BODY
};

template <typename InKeyType, typename InValueType, typename SetAllocator, typename KeyFuncs>
class TMultiMap : public TMapBase<InKeyType, InValueType, SetAllocator, KeyFuncs>
{
	using Super = TMapBase<InKeyType, InValueType, SetAllocator, KeyFuncs>;
public:
	using typename Super::KeyType;
	using typename Super::ValueType;
	TMultiMap() = default;
	template <typename Allocator> void MultiFind(typename Super::KeyInitType Key, TArray<ValueType, Allocator>& OutValues, bool bMaintainOrder = false) const UE_STUB_BODY
	template <typename Allocator> void MultiFindPointer(typename Super::KeyInitType Key, TArray<const ValueType*, Allocator>& OutValues, bool bMaintainOrder = false) const UE_STUB_BODY
	ValueType& AddUnique(const KeyType& InKey, const ValueType& InValue) UE_STUB_BODY
	int32 Remove(typename Super::KeyConstPointerType InKey) UE_STUB_BODY
	int32 Remove(typename Super::KeyInitType InKey, typename Super::ValueInitType InValue) UE_STUB_BODY
	int32 RemoveSingle(typename Super::KeyInitType InKey, typename Super::ValueInitType InValue) UE_STUB_BODY
	ValueType* FindPair(typename Super::KeyInitType Key, typename Super::ValueInitType Value) UE_STUB_BODY
	int32 Num(typename Super::KeyInitType Key) const UE_STUB_BODY
	int32 Num() const UE_STUB_BODY
};
