// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Delegates/DelegateBase.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Delegates/IDelegateInstance.h"
#include "UObject/NameTypes.h"

struct FDefaultDelegateUserPolicy {};
struct FDefaultTSDelegateUserPolicy {};
struct FNotThreadSafeNotCheckedDelegateUserPolicy {};

template <typename UserPolicy>
class TDelegateBase
{
public:
	void Unbind() UE_STUB_BODY
	FName TryGetBoundFunctionName() const UE_STUB_BODY
	class UObject* GetUObject() const;
	bool IsBoundToObject(const void* InUserObject) const UE_STUB_BODY
	const void* GetObjectForTimerManager() const UE_STUB_BODY
	bool IsBound() const UE_STUB_BODY
	FDelegateHandle GetHandle() const UE_STUB_BODY
};
using FDelegateBase = TDelegateBase<FDefaultDelegateUserPolicy>;
