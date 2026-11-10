// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/UObject/StrongObjectPtr.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

template <typename ObjectType>
class TStrongObjectPtr
{
public:
	using ElementType = ObjectType;
	TStrongObjectPtr(TStrongObjectPtr&& InOther) UE_STUB_BODY
	TStrongObjectPtr(const TStrongObjectPtr& InOther) UE_STUB_BODY
	TStrongObjectPtr(TYPE_OF_NULLPTR = nullptr) UE_STUB_BODY
	explicit TStrongObjectPtr(ObjectType* InObject) UE_STUB_BODY
	~TStrongObjectPtr() UE_STUB_BODY
	TStrongObjectPtr& operator=(TStrongObjectPtr&& InOther) UE_STUB_BODY
	TStrongObjectPtr& operator=(const TStrongObjectPtr& InOther) UE_STUB_BODY
	ObjectType& operator*() const UE_STUB_BODY
	ObjectType* operator->() const UE_STUB_BODY
	bool IsValid() const UE_STUB_BODY
	explicit operator bool() const UE_STUB_BODY
	ObjectType* Get() const UE_STUB_BODY
	void Reset() UE_STUB_BODY
	void Reset(ObjectType* InNewObject) UE_STUB_BODY
};
