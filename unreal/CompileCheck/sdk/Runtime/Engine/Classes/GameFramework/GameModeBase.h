// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/GameFramework/GameModeBase.h (UE 5.x source knowledge;
// UPROPERTY / UFUNCTION-marked members: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "Templates/SubclassOf.h"

class APawn;
class APlayerController;
class AHUD;
class AGameStateBase;
class APlayerState;
class ASpectatorPawn;
class AGameSession;
class AController;

class AGameModeBase : public AInfo
{
	UE_STUB_CLASS_BODY(AGameModeBase, AInfo)
public:
	UPROPERTY(EditAnywhere, NoClear, BlueprintReadOnly, Category = Classes)
	TSubclassOf<AGameSession> GameSessionClass;
	UPROPERTY(EditAnywhere, NoClear, BlueprintReadOnly, Category = Classes)
	TSubclassOf<AGameStateBase> GameStateClass;
	UPROPERTY(EditAnywhere, NoClear, BlueprintReadOnly, Category = Classes)
	TSubclassOf<APlayerController> PlayerControllerClass;
	UPROPERTY(EditAnywhere, NoClear, BlueprintReadOnly, Category = Classes)
	TSubclassOf<APlayerState> PlayerStateClass;
	UPROPERTY(EditAnywhere, NoClear, BlueprintReadOnly, Category = Classes, meta = (DisplayName = "HUD Class"))
	TSubclassOf<AHUD> HUDClass;
	UPROPERTY(Transient)
	TObjectPtr<AGameStateBase> GameState;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Classes)
	TSubclassOf<APawn> DefaultPawnClass;
	UPROPERTY(EditAnywhere, NoClear, BlueprintReadOnly, Category = Classes)
	TSubclassOf<ASpectatorPawn> SpectatorClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GameMode)
	uint32 bUseSeamlessTravel : 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GameMode)
	uint32 bStartPlayersAsSpectators : 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GameMode)
	uint32 bPauseable : 1;
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage);
	virtual void InitGameState();
	virtual void StartPlay();
	UFUNCTION(BlueprintCallable, Category = Game)
	virtual bool HasMatchStarted() const;
	UFUNCTION(BlueprintCallable, Category = Game)
	virtual bool HasMatchEnded() const;
	virtual void PostLogin(APlayerController* NewPlayer);
	virtual void Logout(AController* Exiting);
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController);
	virtual void RestartPlayer(AController* NewPlayer);
	/** BlueprintNativeEvent bool PlayerCanRestart(APlayerController*): true when RestartPlayer may be called for Player. */
	UFUNCTION(BlueprintNativeEvent, Category = Game)
	bool PlayerCanRestart(APlayerController* Player);
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player);
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer);
	template <class T> T* GetGameState() const { return Cast<T>(GameState); }
};
