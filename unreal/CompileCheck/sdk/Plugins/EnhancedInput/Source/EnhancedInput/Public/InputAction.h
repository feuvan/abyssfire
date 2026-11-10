// Stub SDK. Real header: Engine/Plugins/EnhancedInput/Source/EnhancedInput/Public/InputAction.h (UE 5.x source knowledge;
// reflected classes / structs / enums: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InputActionValue.h"
#include "InputModifiers.h"
#include "InputTriggers.h"

UENUM()
enum class EInputActionAccumulationBehavior : uint8
{
	TakeHighestAbsoluteValue,
	Cumulative,
};

class UInputAction : public UDataAsset
{
	UE_STUB_CLASS_BODY(UInputAction, UDataAsset)
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Description")
	FText ActionDescription;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Action)
	bool bTriggerWhenPaused;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Action)
	bool bConsumeInput;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Action)
	bool bConsumesActionAndAxisMappings;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Action)
	EInputActionValueType ValueType;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Action)
	EInputActionAccumulationBehavior AccumulationBehavior;
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = Action)
	TArray<TObjectPtr<UInputTrigger>> Triggers;
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = Action)
	TArray<TObjectPtr<UInputModifier>> Modifiers;
};

struct FInputActionInstance
{
	UE_STUB_STRUCT_BODY(FInputActionInstance)
	FInputActionInstance() = default;
	FInputActionInstance(const UInputAction* InSourceAction);
	ETriggerEvent GetTriggerEvent() const;
	FInputActionValue GetValue() const;
	float GetElapsedTime() const;
	float GetTriggeredTime() const;
	float GetLastTriggeredWorldTime() const;
	const TArray<UInputTrigger*>& GetTriggers() const;
	const TArray<UInputModifier*>& GetModifiers() const;
	const UInputAction* GetSourceAction() const;
};
