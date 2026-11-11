// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/UObject/WeakObjectPtrTemplates.h (from UE 5.x source knowledge).
// TWeakObjectPtr<T>: the template is in Core, FWeakObjectPtr (its base) in CoreUObject's UObject/WeakObjectPtr.h; as
// in the engine, using Get() / IsValid() needs that header and the complete T (both come with UObject/Object.h).
#pragma once

#include "CoreTypes.h"
#include "Templates/UnrealTypeTraits.h"
#include "Templates/TypeHash.h"
#include "UObject/WeakObjectPtrTemplatesFwd.h"

class UObject;
template <typename T> struct TObjectPtr;

template <class T, class TWeakObjectPtrBase>
struct TWeakObjectPtr : private TWeakObjectPtrBase
{
public:
	using ElementType = T;
	TWeakObjectPtr() = default;
	TWeakObjectPtr(TYPE_OF_NULLPTR) {}
	template <typename U> requires std::is_convertible_v<U*, T*>
	TWeakObjectPtr(U* Object) { static_assert(std::is_base_of_v<UObject, U>, "TWeakObjectPtr can only point at UObject types"); }
	template <typename U> requires std::is_convertible_v<U*, T*>
	TWeakObjectPtr(const TObjectPtr<U>& Object) {}
	TWeakObjectPtr(const TWeakObjectPtr& Other) = default;
	template <typename OtherT> requires std::is_convertible_v<OtherT*, T*>
	TWeakObjectPtr(const TWeakObjectPtr<OtherT, TWeakObjectPtrBase>& Other) {}
	void Reset() UE_STUB_BODY
	TWeakObjectPtr& operator=(const TWeakObjectPtr& Other) = default;
	template <typename U> requires std::is_convertible_v<U*, T*>
	TWeakObjectPtr& operator=(U* Object) { return *this; }
	template <typename U> requires std::is_convertible_v<U*, T*>
	TWeakObjectPtr& operator=(const TObjectPtr<U>& Object) { return *this; }
	template <typename OtherT> requires std::is_convertible_v<OtherT*, T*>
	TWeakObjectPtr& operator=(const TWeakObjectPtr<OtherT, TWeakObjectPtrBase>& Other) { return *this; }
	// C-style casts, as in the engine: TWeakObjectPtr<T> works with a forward-declared T (the check that T is a UObject
	// is in the constructor from a pointer).
	T* Get(bool bEvenIfPendingKill) const { return (T*)TWeakObjectPtrBase::Get(bEvenIfPendingKill); }
	T* Get() const { return (T*)TWeakObjectPtrBase::Get(); }
	T* GetEvenIfUnreachable() const { return (T*)TWeakObjectPtrBase::GetEvenIfUnreachable(); }
	T& operator*() const { return *Get(); }
	T* operator->() const { return Get(); }
	// "This is explicitly not added to avoid resolving weak pointers too often - use Get() once in a function."
	// `if (Weak)`, `!Weak` and `Weak && X` do not compile in UE 5.x: IsValid() or Get().
	explicit operator bool() const = delete;
	bool IsValid(bool bEvenIfPendingKill, bool bThreadsafeTest = false) const { return TWeakObjectPtrBase::IsValid(bEvenIfPendingKill, bThreadsafeTest); }
	bool IsValid() const { return TWeakObjectPtrBase::IsValid(); }
	bool IsStale(bool bIncludingIfPendingKill = true, bool bThreadsafeTest = false) const { return TWeakObjectPtrBase::IsStale(bIncludingIfPendingKill, bThreadsafeTest); }
	bool IsExplicitlyNull() const { return TWeakObjectPtrBase::IsExplicitlyNull(); }
	bool HasSameIndexAndSerialNumber(const TWeakObjectPtr& Other) const UE_STUB_BODY
	uint32 GetWeakPtrTypeHash() const UE_STUB_BODY
	template <typename U, typename B> friend struct TWeakObjectPtr;
	template <typename RhsT> requires std::is_convertible_v<RhsT*, T*> || std::is_convertible_v<T*, RhsT*>
	bool operator==(const TWeakObjectPtr<RhsT, TWeakObjectPtrBase>& Rhs) const { return true; }
	template <typename RhsT> requires std::is_convertible_v<RhsT*, const T*> || std::is_convertible_v<const T*, RhsT*>
	bool operator==(const RhsT* Rhs) const { return Rhs == nullptr; }
	bool operator==(TYPE_OF_NULLPTR) const { return !IsValid(); }
};
template <class T, class B> uint32 GetTypeHash(const TWeakObjectPtr<T, B>& WeakObjectPtr) UE_STUB_BODY
template <class T> struct TIsWeakPointerType<TWeakObjectPtr<T>> { enum { Value = true }; };
template <typename T> TWeakObjectPtr<T> MakeWeakObjectPtr(T* Ptr) UE_STUB_BODY
template <typename T> TWeakObjectPtr<T> MakeWeakObjectPtr(TObjectPtr<T> Ptr) UE_STUB_BODY
