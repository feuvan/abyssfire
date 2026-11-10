// Stub SDK. Real header: Engine/Plugins/EnhancedInput/Source/EnhancedInput/Public/EnhancedInputSubsystems.h (UE 5.x
// source knowledge; reflected classes: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputSubsystemInterface.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Subsystems/WorldSubsystem.h"

class UEnhancedInputLocalPlayerSubsystem : public ULocalPlayerSubsystem, public IEnhancedInputSubsystemInterface
{
	UE_STUB_CLASS_BODY(UEnhancedInputLocalPlayerSubsystem, ULocalPlayerSubsystem)
public:
	virtual UEnhancedPlayerInput* GetPlayerInput() const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;
};
class UEnhancedInputWorldSubsystem : public UWorldSubsystem, public IEnhancedInputSubsystemInterface
{
	UE_STUB_CLASS_BODY(UEnhancedInputWorldSubsystem, UWorldSubsystem)
public:
	virtual UEnhancedPlayerInput* GetPlayerInput() const override;
};
