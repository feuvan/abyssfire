// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/Subsystems/LocalPlayerSubsystem.h (UE 5.x source knowledge;
// reflected class: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/Subsystem.h"

class ULocalPlayer;
class APlayerController;

class ULocalPlayerSubsystem : public USubsystem
{
	UE_STUB_CLASS_BODY_DEFAULT_CTOR(ULocalPlayerSubsystem, USubsystem)
public:
	template <typename LocalPlayerType = ULocalPlayer> LocalPlayerType* GetLocalPlayer() const;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController);
};
