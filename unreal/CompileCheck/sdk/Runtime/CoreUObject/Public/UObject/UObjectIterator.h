// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectIterator.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Class.h"

template <class T>
class TObjectIterator
{
public:
	explicit TObjectIterator(EObjectFlags AdditionalExclusionFlags = RF_ClassDefaultObject, bool bIncludeDerivedClasses = true, EInternalObjectFlags InInternalExclusionFlags = EInternalObjectFlags::None) UE_STUB_BODY
	void operator++() UE_STUB_BODY
	explicit operator bool() const UE_STUB_BODY
	bool operator!() const UE_STUB_BODY
	T* operator*() const UE_STUB_BODY
	T* operator->() const UE_STUB_BODY
};
template <class T>
struct TObjectRange
{
	TObjectRange(EObjectFlags AdditionalExclusionFlags = RF_ClassDefaultObject, bool bIncludeDerivedClasses = true, EInternalObjectFlags InInternalExclusionFlags = EInternalObjectFlags::None) UE_STUB_BODY
	struct FIt { T* operator*() const UE_STUB_BODY FIt& operator++() UE_STUB_BODY bool operator!=(const FIt&) const UE_STUB_BODY };
	FIt begin() const UE_STUB_BODY
	FIt end() const UE_STUB_BODY
};
