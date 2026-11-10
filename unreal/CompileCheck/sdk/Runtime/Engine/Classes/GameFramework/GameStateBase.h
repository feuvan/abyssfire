// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/GameFramework/GameStateBase.h (UE 5.x source knowledge;
// UPROPERTY / UFUNCTION-marked members: checked against the 5.6 stub). Only what makes AGameStateBase a complete class.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"

class APlayerState;

class AGameStateBase : public AInfo
{
	UE_STUB_CLASS_BODY(AGameStateBase, AInfo)
public:
	/** Array of all PlayerStates, maintained on both server and clients (PlayerStates are always relevant). */
	UPROPERTY(Transient, BlueprintReadOnly, Category = GameState)
	TArray<TObjectPtr<APlayerState>> PlayerArray;
	UFUNCTION(BlueprintCallable, Category = GameState)
	virtual bool HasBegunPlay() const;
	UFUNCTION(BlueprintCallable, Category = GameState)
	virtual bool HasMatchStarted() const;
	UFUNCTION(BlueprintCallable, Category = GameState)
	virtual bool HasMatchEnded() const;
	UFUNCTION(BlueprintCallable, Category = GameState)
	virtual double GetServerWorldTimeSeconds() const;
};
