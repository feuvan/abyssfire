// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/Subsystems/Subsystem.h (from UE 5.x source knowledge;
// reflected class: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Templates/SubclassOf.h"

class FSubsystemCollectionBase;

class USubsystem : public UObject
{
	UE_STUB_CLASS_BODY(USubsystem, UObject)
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const { return true; }
	virtual void Initialize(FSubsystemCollectionBase& Collection) {}
	virtual void Deinitialize() {}
};
class UDynamicSubsystem : public USubsystem
{
	UE_STUB_CLASS_BODY(UDynamicSubsystem, USubsystem)
};
