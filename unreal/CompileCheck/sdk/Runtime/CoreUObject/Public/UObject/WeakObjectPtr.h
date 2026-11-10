// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/UObject/WeakObjectPtr.h (from UE 5.x source knowledge).
// FWeakObjectPtr: the untyped base of TWeakObjectPtr<T> (template in Core's UObject/WeakObjectPtrTemplates.h).
#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UObject;

struct FWeakObjectPtr
{
public:
	FWeakObjectPtr() = default;
	FWeakObjectPtr(TYPE_OF_NULLPTR);
	FWeakObjectPtr(const UObject* Object);
	FWeakObjectPtr(const FWeakObjectPtr& Other) = default;
	void Reset();
	void operator=(const UObject* Object);
	FWeakObjectPtr& operator=(const FWeakObjectPtr& Other) = default;
	bool operator==(const FWeakObjectPtr& Other) const;
	bool operator!=(const FWeakObjectPtr& Other) const;
	bool HasSameIndexAndSerialNumber(const FWeakObjectPtr& Other) const;
	UObject* Get(bool bEvenIfPendingKill) const;
	UObject* Get() const;
	UObject* GetEvenIfUnreachable() const;
	bool IsValid(bool bEvenIfPendingKill, bool bThreadsafeTest = false) const;
	bool IsValid() const;
	bool IsStale(bool bIncludingIfPendingKill = true, bool bThreadsafeTest = false) const;
	bool IsExplicitlyNull() const;
	uint32 GetTypeHash() const;
private:
	int32 ObjectIndex = 0;
	int32 ObjectSerialNumber = 0;
};
uint32 GetTypeHash(const FWeakObjectPtr& WeakObjectPtr);
