// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/TimerHandle.h (from UE 5.x source knowledge;
// reflected struct: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

struct FTimerHandle
{
	UE_STUB_STRUCT_BODY(FTimerHandle)
	FTimerHandle() : Handle(0) {}
	bool IsValid() const { return Handle != 0; }
	void Invalidate() { Handle = 0; }
	bool operator==(const FTimerHandle& Other) const { return Handle == Other.Handle; }
	bool operator!=(const FTimerHandle& Other) const { return Handle != Other.Handle; }
	FString ToString() const;
private:
	uint64 Handle;
	friend uint32 GetTypeHash(const FTimerHandle& InHandle) { return (uint32)InHandle.Handle; }
};
