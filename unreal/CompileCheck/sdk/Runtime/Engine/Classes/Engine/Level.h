// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/Level.h (UE 5.x source knowledge; reflected class:
// checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/EngineBaseTypes.h"

class AActor;
class UWorld;
class AWorldSettings;

class ULevel : public UObject
{
	UE_STUB_CLASS_BODY(ULevel, UObject)
public:
	TArray<TObjectPtr<AActor>> Actors;
	UPROPERTY(Transient)
	TObjectPtr<UWorld> OwningWorld;
	uint8 bIsVisible : 1;
	AWorldSettings* GetWorldSettings(bool bChecked = true) const;
	bool IsPersistentLevel() const;
	bool IsCurrentLevel() const;
	virtual UWorld* GetWorld() const override final;
};
