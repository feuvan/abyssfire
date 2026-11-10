// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/UObject/ObjectKey.h (from UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"

struct FObjectKey
{
public:
	FObjectKey() = default;
	FObjectKey(const UObject* Object);
	template <typename T> FObjectKey(TObjectPtr<T> Object) UE_STUB_BODY
	bool operator==(const FObjectKey& Other) const = default;
	bool operator<(const FObjectKey& Other) const;
	FWeakObjectPtr GetWeakObjectPtr() const;
	UObject* ResolveObjectPtr() const;
	UObject* ResolveObjectPtrEvenIfGarbage() const;
	friend uint32 GetTypeHash(const FObjectKey& Key) { return (uint32)Key.ObjectIndex; }
private:
	int32 ObjectIndex = 0;
	int32 ObjectSerialNumber = 0;
};

template <typename InElementType>
struct TObjectKey
{
public:
	using ElementType = InElementType;
	TObjectKey() = default;
	template <typename U> requires std::is_convertible_v<U, const InElementType*>
	TObjectKey(U Object) : ObjectKey(Object) {}
	template <typename U> requires std::is_convertible_v<U*, const InElementType*>
	TObjectKey(const TWeakObjectPtr<U>& WeakObjectPtr) {}
	bool operator==(const TObjectKey& Other) const = default;
	bool operator<(const TObjectKey& Other) const UE_STUB_BODY
	friend uint32 GetTypeHash(const TObjectKey& Key) { return GetTypeHash(Key.ObjectKey); }
	TWeakObjectPtr<InElementType> GetWeakObjectPtr() const UE_STUB_BODY
	InElementType* ResolveObjectPtr() const UE_STUB_BODY
	InElementType* ResolveObjectPtrEvenIfGarbage() const UE_STUB_BODY
private:
	FObjectKey ObjectKey;
};
