// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/Subsystems/SubsystemCollection.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/Subsystem.h"
#include "UObject/GCObject.h"

class FSubsystemCollectionBase : public FGCObject
{
public:
	USubsystem* InitializeDependency(TSubclassOf<USubsystem> SubsystemClass);
	template <typename TSubsystemClass>
	TSubsystemClass* InitializeDependency() { return Cast<TSubsystemClass>(InitializeDependency(TSubsystemClass::StaticClass())); }
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
};
