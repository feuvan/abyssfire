// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/GameFramework/HUD.h (UE 5.x source knowledge;
// UPROPERTY / UFUNCTION-marked members: checked against the 5.6 stub). Only what makes AHUD a complete class.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

class APawn;
class APlayerController;

class AHUD : public AActor
{
	UE_STUB_CLASS_BODY(AHUD, AActor)
public:
	/** PlayerController which owns this HUD. */
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	TObjectPtr<APlayerController> PlayerOwner;
	UFUNCTION(BlueprintCallable, Category = HUD)
	APlayerController* GetOwningPlayerController() const;
	UFUNCTION(BlueprintCallable, Category = HUD)
	APawn* GetOwningPawn() const;
};
