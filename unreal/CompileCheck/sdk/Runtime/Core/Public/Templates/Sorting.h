// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Templates/Sorting.h (from UE 5.x source knowledge).
// TDereferenceWrapper is why TArray<T*>::Sort / StableSort / Heapify, TSet<T*>::Sort, TMap::KeySort / ValueSort and the
// Sort(T**, ...) overloads hand the predicate references to the pointees (T&) and not the pointers; smart pointers and
// every other element type reach the predicate as they are. Every constraint below is that rule.
#pragma once

#include "CoreTypes.h"
#include "Templates/Less.h"

/** Helper class for dereferencing pointer types in Sort function. */
template <typename T, class PREDICATE_CLASS>
struct TDereferenceWrapper
{
	const PREDICATE_CLASS& Predicate;
	TDereferenceWrapper(const PREDICATE_CLASS& InPredicate) : Predicate(InPredicate) {}
	/** Pass through for non-pointer types. */
	bool operator()(T& A, T& B) { return Predicate(A, B); }
	bool operator()(const T& A, const T& B) const { return Predicate(A, B); }
};
/** Partially specialized version of the above class. */
template <typename T, class PREDICATE_CLASS>
struct TDereferenceWrapper<T*, PREDICATE_CLASS>
{
	const PREDICATE_CLASS& Predicate;
	TDereferenceWrapper(const PREDICATE_CLASS& InPredicate) : Predicate(InPredicate) {}
	/** Dereference pointers. */
	bool operator()(T* A, T* B) const { return Predicate(*A, *B); }
};

namespace UE::Stub
{
	/** What TDereferenceWrapper<ElementType, Predicate> hands the predicate: the pointee of a raw pointer, else the element
	 *  (the const overload: a predicate must not need mutable elements). */
	template <typename ElementType> struct TSortArgument { using Type = const ElementType&; };
	template <typename PointeeType> struct TSortArgument<PointeeType*> { using Type = PointeeType&; };
	/** A sort / heap predicate of a container of ElementType: bool(Argument, Argument) on a const predicate. */
	template <typename PredicateType, typename ElementType>
	concept CSortPredicate = std::is_invocable_r_v<bool, const PredicateType&, typename TSortArgument<ElementType>::Type, typename TSortArgument<ElementType>::Type>;
	/** Sort() / StableSort() without a predicate: operator< of what the wrapper passes (of the pointee for raw pointers). */
	template <typename ElementType>
	concept CSortable = requires(typename TSortArgument<ElementType>::Type A, typename TSortArgument<ElementType>::Type B) { { A < B } -> std::convertible_to<bool>; };
	/** The heap functions without a predicate use TLess<ElementType> behind the wrapper: operator< of a non-pointer element
	 *  (a heap of raw pointers needs an explicit predicate). */
	template <typename ElementType>
	concept CHeapOrdered = (!std::is_pointer_v<ElementType>) && requires(const ElementType& A, const ElementType& B) { { A < B } -> std::convertible_to<bool>; };
}

template <class T, class PREDICATE_CLASS> requires UE::Stub::CSortPredicate<PREDICATE_CLASS, T>
void Sort(T* First, const int32 Num, const PREDICATE_CLASS& Predicate) UE_STUB_BODY
template <class T, class PREDICATE_CLASS> requires UE::Stub::CSortPredicate<PREDICATE_CLASS, T*>
void Sort(T** First, const int32 Num, const PREDICATE_CLASS& Predicate) UE_STUB_BODY
template <class T> requires UE::Stub::CSortable<T>
void Sort(T* First, const int32 Num) UE_STUB_BODY
template <class T> requires UE::Stub::CSortable<T*>
void Sort(T** First, const int32 Num) UE_STUB_BODY
template <class T, class PREDICATE_CLASS> requires UE::Stub::CSortPredicate<PREDICATE_CLASS, T>
void StableSort(T* First, const int32 Num, const PREDICATE_CLASS& Predicate) UE_STUB_BODY
template <class T, class PREDICATE_CLASS> requires UE::Stub::CSortPredicate<PREDICATE_CLASS, T*>
void StableSort(T** First, const int32 Num, const PREDICATE_CLASS& Predicate) UE_STUB_BODY
template <class T> requires UE::Stub::CSortable<T>
void StableSort(T* First, const int32 Num) UE_STUB_BODY
template <class T> requires UE::Stub::CSortable<T*>
void StableSort(T** First, const int32 Num) UE_STUB_BODY
