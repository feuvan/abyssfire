// Stub SDK. Real header: Engine/Plugins/EnhancedInput/Source/EnhancedInput/Public/InputTriggers.h (UE 5.x source
// knowledge; reflected classes / enums: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "InputActionValue.h"

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ETriggerEvent : uint8
{
	None = (0x0) UMETA(Hidden),
	Triggered = (1 << 0),
	Started = (1 << 1),
	Ongoing = (1 << 2),
	Canceled = (1 << 3),
	Completed = (1 << 4),
};
ENUM_CLASS_FLAGS(ETriggerEvent)
UENUM(BlueprintType)
enum class ETriggerState : uint8
{
	None,
	Ongoing,
	Triggered,
};

class UInputTrigger : public UObject
{
	UE_STUB_CLASS_BODY(UInputTrigger, UObject)
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Settings")
	float ActuationThreshold;
	UPROPERTY(BlueprintReadOnly, Category = "Trigger Settings")
	bool bShouldAlwaysTick;
	UFUNCTION(BlueprintCallable, Category = "Trigger")
	bool IsActuated(const FInputActionValue& ForValue) const;
};
class UInputTriggerPressed final : public UInputTrigger
{
	UE_STUB_CLASS_BODY(UInputTriggerPressed, UInputTrigger)
};
class UInputTriggerReleased final : public UInputTrigger
{
	UE_STUB_CLASS_BODY(UInputTriggerReleased, UInputTrigger)
};
class UInputTriggerDown final : public UInputTrigger
{
	UE_STUB_CLASS_BODY(UInputTriggerDown, UInputTrigger)
};
