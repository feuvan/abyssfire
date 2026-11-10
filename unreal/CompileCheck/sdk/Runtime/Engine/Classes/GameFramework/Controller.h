// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/GameFramework/Controller.h (UE 5.x source knowledge;
// UFUNCTION-marked members: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

class APawn;
class APlayerState;

class AController : public AActor
{
	UE_STUB_CLASS_BODY(AController, AActor)
public:
	UPROPERTY(BlueprintReadOnly, Category = Controller)
	TObjectPtr<APlayerState> PlayerState;
	UFUNCTION(BlueprintCallable, Category = Pawn, meta = (DisplayName = "Get Controlled Pawn", ScriptName = "GetControlledPawn"))
	APawn* K2_GetPawn() const;
	APawn* GetPawn() const;
	template <class T> T* GetPawn() const { return Cast<T>(GetPawn()); }
	UFUNCTION(BlueprintCallable, Category = Pawn)
	virtual void SetControlRotation(const FRotator& NewRotation);
	UFUNCTION(BlueprintCallable, Category = Pawn)
	virtual FRotator GetControlRotation() const;
	UFUNCTION(BlueprintCallable, Category = Pawn, meta = (Keywords = "set controller"))
	virtual void Possess(APawn* InPawn);
	UFUNCTION(BlueprintCallable, Category = Pawn, meta = (Keywords = "set controller"))
	virtual void UnPossess();
	UFUNCTION(BlueprintCallable, Category = Pawn)
	bool IsLocalController() const;
	UFUNCTION(BlueprintCallable, Category = Pawn)
	bool IsLocalPlayerController() const;
	UFUNCTION(BlueprintCallable, Category = Pawn)
	bool IsPlayerController() const;
	UFUNCTION(BlueprintCallable, Category = Pawn)
	virtual AActor* GetViewTarget() const;
	UFUNCTION(BlueprintCallable, Category = Input)
	virtual void SetIgnoreMoveInput(bool bNewMoveInput);
	UFUNCTION(BlueprintCallable, Category = Input)
	virtual void SetIgnoreLookInput(bool bNewLookInput);
protected:
	virtual void OnPossess(APawn* InPawn);
	virtual void OnUnPossess();
};
