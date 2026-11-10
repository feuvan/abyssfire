// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Templates/UniquePtr.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Templates/UnrealTemplate.h"

template <typename T>
struct TDefaultDelete
{
	TDefaultDelete() = default;
	template <typename U> requires std::is_convertible_v<U*, T*>
	TDefaultDelete(const TDefaultDelete<U>&) {}
	void operator()(T* Ptr) const { static_assert(sizeof(T) > 0, "TUniquePtr: cannot delete an incomplete type"); delete Ptr; }
};
template <typename T>
struct TDefaultDelete<T[]>
{
	TDefaultDelete() = default;
	template <typename U> void operator()(U* Ptr) const { delete[] Ptr; }
};

template <typename T, typename Deleter = TDefaultDelete<T>>
class TUniquePtr : private Deleter
{
	template <typename OtherT, typename OtherDeleter> friend class TUniquePtr;
public:
	using ElementType = T;
	TUniquePtr(const TUniquePtr&) = delete;
	TUniquePtr& operator=(const TUniquePtr&) = delete;
	constexpr TUniquePtr() : Ptr(nullptr) {}
	constexpr TUniquePtr(TYPE_OF_NULLPTR) : Ptr(nullptr) {}
	template <typename U> requires std::is_convertible_v<U*, T*>
	explicit TUniquePtr(U* InPtr) : Ptr(InPtr) {}
	TUniquePtr(TUniquePtr&& Other) : Deleter(static_cast<Deleter&&>(Other.GetDeleter())), Ptr(Other.Ptr) { Other.Ptr = nullptr; }
	template <typename OtherT, typename OtherDeleter>
		requires (!std::is_array_v<OtherT>) && std::is_convertible_v<OtherT*, T*> && std::is_convertible_v<OtherDeleter&&, Deleter>
	TUniquePtr(TUniquePtr<OtherT, OtherDeleter>&& Other) : Deleter(static_cast<OtherDeleter&&>(Other.GetDeleter())), Ptr(Other.Ptr) { Other.Ptr = nullptr; }
	TUniquePtr& operator=(TUniquePtr&& Other) { if (this != &Other) { T* Old = Ptr; Ptr = Other.Ptr; Other.Ptr = nullptr; GetDeleter()(Old); } return *this; }
	template <typename OtherT, typename OtherDeleter>
		requires (!std::is_array_v<OtherT>) && std::is_convertible_v<OtherT*, T*> && std::is_assignable_v<Deleter&, OtherDeleter&&>
	TUniquePtr& operator=(TUniquePtr<OtherT, OtherDeleter>&& Other) { T* Old = Ptr; Ptr = Other.Ptr; Other.Ptr = nullptr; GetDeleter()(Old); return *this; }
	TUniquePtr& operator=(TYPE_OF_NULLPTR) { T* Old = Ptr; Ptr = nullptr; GetDeleter()(Old); return *this; }
	~TUniquePtr() { GetDeleter()(Ptr); }
	bool IsValid() const { return Ptr != nullptr; }
	explicit operator bool() const { return IsValid(); }
	bool operator!() const { return !IsValid(); }
	T* operator->() const { return Ptr; }
	T& operator*() const { return *Ptr; }
	T* Get() const { return Ptr; }
	T* Release() { T* Result = Ptr; Ptr = nullptr; return Result; }
	void Reset(T* InPtr = nullptr) { if (Ptr != InPtr) { T* Old = Ptr; Ptr = InPtr; GetDeleter()(Old); } }
	Deleter& GetDeleter() { return static_cast<Deleter&>(*this); }
	const Deleter& GetDeleter() const { return static_cast<const Deleter&>(*this); }
	template <typename RhsT> bool operator==(const TUniquePtr<RhsT>& Rhs) const { return Get() == Rhs.Get(); }
	bool operator==(TYPE_OF_NULLPTR) const { return !IsValid(); }
private:
	T* Ptr;
};
template <typename T, typename Deleter>
class TUniquePtr<T[], Deleter> : private Deleter
{
public:
	TUniquePtr(const TUniquePtr&) = delete;
	TUniquePtr& operator=(const TUniquePtr&) = delete;
	constexpr TUniquePtr() : Ptr(nullptr) {}
	constexpr TUniquePtr(TYPE_OF_NULLPTR) : Ptr(nullptr) {}
	explicit TUniquePtr(T* InPtr) : Ptr(InPtr) {}
	TUniquePtr(TUniquePtr&& Other) : Ptr(Other.Ptr) { Other.Ptr = nullptr; }
	TUniquePtr& operator=(TUniquePtr&& Other) UE_STUB_BODY
	TUniquePtr& operator=(TYPE_OF_NULLPTR) UE_STUB_BODY
	~TUniquePtr() UE_STUB_BODY
	bool IsValid() const { return Ptr != nullptr; }
	explicit operator bool() const { return IsValid(); }
	bool operator!() const { return !IsValid(); }
	T& operator[](SIZE_T Index) const { return Ptr[Index]; }
	T* Get() const { return Ptr; }
	T* Release() UE_STUB_BODY
	void Reset(T* InPtr = nullptr) UE_STUB_BODY
private:
	T* Ptr;
};
template <typename T, typename... TArgs> requires (!std::is_array_v<T>) && std::is_constructible_v<T, TArgs&&...>
TUniquePtr<T> MakeUnique(TArgs&&... Args) { return TUniquePtr<T>(new T(static_cast<TArgs&&>(Args)...)); }
template <typename T> requires std::is_unbounded_array_v<T>
TUniquePtr<T> MakeUnique(SIZE_T Size) { return TUniquePtr<T>(new std::remove_extent_t<T>[Size]()); }
template <typename T> requires (!std::is_array_v<T>)
TUniquePtr<T> MakeUniqueForOverwrite() { return TUniquePtr<T>(new T); }
template <typename T> bool operator==(TYPE_OF_NULLPTR, const TUniquePtr<T>& Rhs) { return !Rhs.IsValid(); }
template <typename T> bool operator!=(TYPE_OF_NULLPTR, const TUniquePtr<T>& Rhs) { return Rhs.IsValid(); }
template <typename T> uint32 GetTypeHash(const TUniquePtr<T>& InPtr) UE_STUB_BODY
