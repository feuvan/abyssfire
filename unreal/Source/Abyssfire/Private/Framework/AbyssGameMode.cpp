#include "Framework/AbyssGameMode.h"

#include "Engine/World.h"

#include "Framework/AbyssGameInstance.h"
#include "Framework/AbyssPlayerController.h"

AAbyssGameMode::AAbyssGameMode()
{
	PlayerControllerClass = AAbyssPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	bStartPlayersAsSpectators = false;
}

void AAbyssGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (UAbyssGameInstance* GameInstance = UAbyssGameInstance::Get(this))
	{
		GameInstance->NotifyGameWorldReady(GetWorld());
	}
}

bool AAbyssGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	return false;
}
