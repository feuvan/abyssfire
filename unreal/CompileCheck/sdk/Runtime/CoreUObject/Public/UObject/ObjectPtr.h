// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/UObject/ObjectPtr.h (from UE 5.x source knowledge).
// TObjectPtr<T>: the UPROPERTY pointer wrapper. Constructible from T* / nullptr / TObjectPtr<Derived>, converts to T*
// implicitly, Get() / -> / * / explicit bool. T may be incomplete where the member is declared (as in the engine);
// TArray<TObjectPtr<T>> does NOT convert to TArray<T*>.
#pragma once

#include "CoreMinimal.h"

class UObject;
class UClass;

namespace UE::Stub
{
	/** ObjectPtr_Private::CoerceToPointer<T> of the engine: through implicit conversion to const T*, or through Get(). */
	template <typename T> const T* ObjectPtrCoerce(const T* Other) { return Other; }
	template <typename T, typename U> auto ObjectPtrCoerce(const U& Other) -> decltype(Other.Get()) { return Other.Get(); }
}

template <typename T>
struct TObjectPtr
{
public:
	using ElementType = T;
	TObjectPtr() : ObjectPtr(nullptr) {}
	TObjectPtr(TObjectPtr<T>&& Other) = default;
	TObjectPtr(const TObjectPtr<T>& Other) = default;
	TObjectPtr(TYPE_OF_NULLPTR) : ObjectPtr(nullptr) {}
	explicit TObjectPtr(ENoInit) {}
	template <typename U> requires std::is_convertible_v<U*, T*>
	TObjectPtr(const TObjectPtr<U>& Other) : ObjectPtr(Other.Get()) {}
	template <typename U> requires std::is_convertible_v<U, T*> && (!std::is_null_pointer_v<std::decay_t<U>>)
	TObjectPtr(U&& Object) : ObjectPtr(Object) {}
	TObjectPtr(T& Object) : ObjectPtr(&Object) {}
	TObjectPtr<T>& operator=(TObjectPtr<T>&&) = default;
	TObjectPtr<T>& operator=(const TObjectPtr<T>&) = default;
	TObjectPtr<T>& operator=(TYPE_OF_NULLPTR) { ObjectPtr = nullptr; return *this; }
	template <typename U> requires std::is_convertible_v<U*, T*>
	TObjectPtr<T>& operator=(const TObjectPtr<U>& Other) { ObjectPtr = Other.Get(); return *this; }
	template <typename U> requires std::is_convertible_v<U, T*> && (!std::is_null_pointer_v<std::decay_t<U>>)
	TObjectPtr<T>& operator=(U&& Object) { ObjectPtr = Object; return *this; }

	// Equality / inequality comparisons against other (compatible) object pointers
	template <typename U> requires requires(T* A, U* B) { A == B; }
	bool operator==(const TObjectPtr<U>& Other) const { return ObjectPtr == Other.Get(); }
	// ... against nullptr
	bool operator==(TYPE_OF_NULLPTR) const { return ObjectPtr == nullptr; }
	// ... against another type that can be implicitly converted to the pointer type kept in a TObjectPtr (the engine's
	// `operator==(const U& Other)` over ObjectPtr_Private::CoerceToPointer<T>: anything that converts to const T*, or
	// has a Get()). Taking `const U&` matters for overload resolution: `Ptr == RawPointer` binds the raw pointer
	// exactly, so this member wins over the built-in pointer comparison reached through operator T*().
	template <typename U> requires requires(T* A, const U& Other) { A == UE::Stub::ObjectPtrCoerce<T>(Other); }
	bool operator==(const U& Other) const { return ObjectPtr == UE::Stub::ObjectPtrCoerce<T>(Other); }

	T* Get() const { return ObjectPtr; }
	UClass* GetClass() const UE_STUB_BODY
	operator T*() const { return Get(); }
	template <typename U> explicit operator U*() const { return (U*)Get(); }
	explicit operator UPTRINT() const { return (UPTRINT)Get(); }
	T* operator->() const { return Get(); }
	std::add_lvalue_reference_t<T> operator*() const { return *Get(); }
	bool IsNull() const { return ObjectPtr == nullptr; }
	bool IsNullNoResolve() const { return ObjectPtr == nullptr; }
	bool operator!() const { return IsNull(); }
	explicit operator bool() const { return !IsNull(); }
	bool IsResolved() const { return true; }
	FString GetPathName() const UE_STUB_BODY
	FName GetFName() const UE_STUB_BODY
	FString GetName() const UE_STUB_BODY
	FString GetFullName() const UE_STUB_BODY
	bool IsA(const UClass* SomeBase) const UE_STUB_BODY
	template <typename U> bool IsA() const { return IsA(U::StaticClass()); }
private:
	T* ObjectPtr;
};
template <typename T> uint32 GetTypeHash(const TObjectPtr<T>& InObjectPtr) UE_STUB_BODY
template <typename T> T* ToRawPtr(const TObjectPtr<T>& Ptr) { return Ptr.Get(); }
template <typename T> T* ToRawPtr(T* Ptr) { return Ptr; }
template <typename T> TObjectPtr<T> ToObjectPtr(T* Obj) { return TObjectPtr<T>{Obj}; }
template <typename T> const TArray<T*>& ObjectPtrDecay(const TArray<TObjectPtr<T>>& Array) { return reinterpret_cast<const TArray<T*>&>(Array); }
template <typename T> TArray<T*>& ObjectPtrDecay(TArray<TObjectPtr<T>>& Array) { return reinterpret_cast<TArray<T*>&>(Array); }
template <typename T> const TArray<TObjectPtr<T>>& ObjectPtrWrap(const TArray<T*>& Array) { return reinterpret_cast<const TArray<TObjectPtr<T>>&>(Array); }
template <typename T> TArray<TObjectPtr<T>>& ObjectPtrWrap(TArray<T*>& Array) { return reinterpret_cast<TArray<TObjectPtr<T>>&>(Array); }
template <typename T> struct TIsZeroConstructType<TObjectPtr<T>> { enum { Value = true }; };
namespace UE::Stub
{
	/** Sorting a container of TObjectPtr (TArray::Sort / StableSort / Heapify, TSet::Sort, TMap::KeySort / ValueSort) is
	 *  not declared: whether the engine specialises TDereferenceWrapper for TObjectPtr (the predicate would get T&, as for
	 *  T*) or passes the TObjectPtr itself could not be confirmed (API_UNCERTAIN.md). No Type: every such call is
	 *  rejected instead of being accepted with the wrong predicate. */
	template <typename T> struct TSortArgument<TObjectPtr<T>> {};
}
