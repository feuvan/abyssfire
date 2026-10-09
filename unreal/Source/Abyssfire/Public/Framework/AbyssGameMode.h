// AAbyssGameMode: the only game mode (DefaultEngine.ini GlobalDefaultGameMode), used by the single persistent map L_Main
// (DECISIONS P9). There is no pawn: the hero is a core entity presented by an actor the world view spawns; the camera is
// the world agent's camera rig (the player controller's view target).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "AbyssGameMode.generated.h"

UCLASS()
class ABYSSFIRE_API AAbyssGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AAbyssGameMode();

	virtual void BeginPlay() override;
	/** No pawn is ever spawned for the player. */
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;
};
