// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/UObject/SoftObjectPtr.h (from UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "Templates/Casts.h"

struct FSoftObjectPtr
{
	FSoftObjectPtr() = default;
	explicit FSoftObjectPtr(const FSoftObjectPath& ObjectPath);
	explicit FSoftObjectPtr(const UObject* Object);
	UObject* LoadSynchronous() const;
	const FSoftObjectPath& ToSoftObjectPath() const;
	FString ToString() const;
	FString GetLongPackageName() const;
	FString GetAssetName() const;
	UObject* Get() const;
	bool IsValid() const;
	bool IsPending() const;
	bool IsNull() const;
	void Reset();
};

template <class T = UObject>
struct TSoftObjectPtr
{
public:
	using ElementType = T;
	TSoftObjectPtr() = default;
	TSoftObjectPtr(TYPE_OF_NULLPTR) {}
	template <class U> requires std::is_convertible_v<U*, T*>
	TSoftObjectPtr(const TSoftObjectPtr<U>& Other) UE_STUB_BODY
	template <typename U> requires std::is_convertible_v<U*, T*>
	TSoftObjectPtr(const U* Object) UE_STUB_BODY
	TSoftObjectPtr(TObjectPtr<T> Object) UE_STUB_BODY
	explicit TSoftObjectPtr(FSoftObjectPath ObjectPath) UE_STUB_BODY
	void Reset() UE_STUB_BODY
	template <typename U> requires std::is_convertible_v<U*, T*>
	TSoftObjectPtr& operator=(const U* Object) UE_STUB_BODY
	TSoftObjectPtr& operator=(TObjectPtr<T> Object) UE_STUB_BODY
	TSoftObjectPtr& operator=(FSoftObjectPath ObjectPath) UE_STUB_BODY
	TSoftObjectPtr& operator=(TYPE_OF_NULLPTR) UE_STUB_BODY
	T* Get() const UE_STUB_BODY
	T& operator*() const UE_STUB_BODY
	T* operator->() const UE_STUB_BODY
	T* LoadSynchronous() const UE_STUB_BODY
	bool IsValid() const UE_STUB_BODY
	bool IsPending() const UE_STUB_BODY
	bool IsNull() const UE_STUB_BODY
	const FSoftObjectPath& ToSoftObjectPath() const UE_STUB_BODY
	FString ToString() const UE_STUB_BODY
	FString GetLongPackageName() const UE_STUB_BODY
	FString GetAssetName() const UE_STUB_BODY
	explicit operator bool() const UE_STUB_BODY
	bool operator==(const TSoftObjectPtr& Rhs) const UE_STUB_BODY
	bool operator==(TYPE_OF_NULLPTR) const UE_STUB_BODY
};
template <class T> uint32 GetTypeHash(const TSoftObjectPtr<T>& Ptr) UE_STUB_BODY
template <class TClass = UObject>
class TSoftClassPtr
{
public:
	TSoftClassPtr() = default;
	TSoftClassPtr(const UClass* From) UE_STUB_BODY
	explicit TSoftClassPtr(const FSoftObjectPath& ObjectPath) UE_STUB_BODY
	UClass* Get() const UE_STUB_BODY
	UClass* LoadSynchronous() const UE_STUB_BODY
	bool IsValid() const UE_STUB_BODY
	bool IsNull() const UE_STUB_BODY
	const FSoftObjectPath& ToSoftObjectPath() const UE_STUB_BODY
	FString ToString() const UE_STUB_BODY
};
