// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/Player.h (UE 5.x source knowledge; reflected class:
// checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

class APlayerController;

class UPlayer : public UObject
{
	UE_STUB_CLASS_BODY(UPlayer, UObject)
public:
	UPROPERTY(Transient)
	TObjectPtr<APlayerController> PlayerController;
	APlayerController* GetPlayerController(const UWorld* const InWorld) const;
};
