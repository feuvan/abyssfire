// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Containers/ArrayView.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Containers/ContainersFwd.h"
#include "Templates/UnrealTemplate.h"
#include <initializer_list>

namespace UE::Stub
{
	/** TArrayView binds to anything GetData() / GetNum() work on whose element pointer converts (TIsCompatibleRangeType). */
	template <typename RangeType, typename ElementType>
	concept CArrayViewCompatibleRange = requires(RangeType&& Range)
	{
		{ GetData(Range) } -> std::convertible_to<ElementType*>;
		GetNum(Range);
	} && (sizeof(std::remove_pointer_t<decltype(GetData(std::declval<RangeType&>()))>) == sizeof(ElementType));
}

template <typename InElementType, typename InSizeType>
class TArrayView
{
public:
	using ElementType = InElementType;
	using SizeType = InSizeType;
	TArrayView() UE_STUB_BODY
	template <typename OtherRangeType>
		requires UE::Stub::CArrayViewCompatibleRange<OtherRangeType, InElementType>
			&& (!std::is_same_v<std::decay_t<OtherRangeType>, TArrayView>)
	TArrayView(OtherRangeType&& Other UE_LIFETIMEBOUND) UE_STUB_BODY
	template <typename OtherElementType> requires std::is_convertible_v<OtherElementType*, InElementType*>
	TArrayView(OtherElementType* InData UE_LIFETIMEBOUND, SizeType InCount) UE_STUB_BODY
	TArrayView(std::initializer_list<std::remove_const_t<InElementType>> List UE_LIFETIMEBOUND) requires std::is_const_v<InElementType> UE_STUB_BODY
	ElementType* GetData() const UE_STUB_BODY
	static constexpr SIZE_T GetTypeSize() { return sizeof(ElementType); }
	bool IsValidIndex(SizeType Index) const UE_STUB_BODY
	bool IsEmpty() const UE_STUB_BODY
	SizeType Num() const UE_STUB_BODY
	SIZE_T NumBytes() const UE_STUB_BODY
	ElementType& operator[](SizeType Index) const UE_STUB_BODY
	ElementType& Last(SizeType IndexFromTheEnd = 0) const UE_STUB_BODY
	TArrayView Slice(SizeType Index, SizeType InNum) const UE_STUB_BODY
	TArrayView Left(SizeType Count) const UE_STUB_BODY
	TArrayView LeftChop(SizeType Count) const UE_STUB_BODY
	TArrayView Right(SizeType Count) const UE_STUB_BODY
	TArrayView RightChop(SizeType Count) const UE_STUB_BODY
	TArrayView Mid(SizeType Index, SizeType Count = TNumericLimitsStub()) const UE_STUB_BODY
	bool Find(const ElementType& Item, SizeType& Index) const requires requires(const ElementType& A, const ElementType& B) { { A == B } -> std::convertible_to<bool>; } UE_STUB_BODY
	SizeType Find(const ElementType& Item) const requires requires(const ElementType& A, const ElementType& B) { { A == B } -> std::convertible_to<bool>; } UE_STUB_BODY
	// As on TArray: a key needs Element == Key, a predicate is bool(Element). The view is const but its elements are
	// ElementType (FindByPredicate / ContainsByPredicate pass ElementType&, IndexOfByPredicate const ElementType&).
	template <typename KeyType> SizeType IndexOfByKey(const KeyType& Key) const requires requires(const ElementType& E, const KeyType& K) { { E == K } -> std::convertible_to<bool>; } UE_STUB_BODY
	template <typename Predicate> SizeType IndexOfByPredicate(Predicate Pred) const requires std::is_invocable_r_v<bool, Predicate&, const ElementType&> UE_STUB_BODY
	template <typename KeyType> ElementType* FindByKey(const KeyType& Key) const requires requires(const ElementType& E, const KeyType& K) { { E == K } -> std::convertible_to<bool>; } UE_STUB_BODY
	template <typename Predicate> ElementType* FindByPredicate(Predicate Pred) const requires std::is_invocable_r_v<bool, Predicate&, ElementType&> UE_STUB_BODY
	template <typename ComparisonType> bool Contains(const ComparisonType& Item) const requires requires(const ElementType& E, const ComparisonType& K) { { E == K } -> std::convertible_to<bool>; } UE_STUB_BODY
	template <typename Predicate> bool ContainsByPredicate(Predicate Pred) const requires std::is_invocable_r_v<bool, Predicate&, ElementType&> UE_STUB_BODY
	ElementType* begin() const UE_STUB_BODY
	ElementType* end() const UE_STUB_BODY
private:
	static constexpr SizeType TNumericLimitsStub() { return static_cast<SizeType>(0x7fffffff); }
};
template <typename T, typename SizeType> struct TIsContiguousContainer<TArrayView<T, SizeType>> { enum { Value = true }; };

template <typename OtherRangeType>
auto MakeArrayView(OtherRangeType&& Other) -> TArrayView<std::remove_pointer_t<decltype(GetData(Other))>> UE_STUB_BODY
template <typename ElementType>
TArrayView<ElementType> MakeArrayView(ElementType* Pointer UE_LIFETIMEBOUND, int32 Size) UE_STUB_BODY
template <typename T>
TArrayView<const T> MakeArrayView(std::initializer_list<T> List) UE_STUB_BODY
template <typename OtherRangeType>
auto MakeConstArrayView(OtherRangeType&& Other) -> TArrayView<const std::remove_pointer_t<decltype(GetData(Other))>> UE_STUB_BODY
template <typename ElementType>
TArrayView<const ElementType> MakeConstArrayView(const ElementType* Pointer UE_LIFETIMEBOUND, int32 Size) UE_STUB_BODY
