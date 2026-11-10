// Stub SDK. Real header: Engine/Plugins/EnhancedInput/Source/EnhancedInput/Public/EnhancedInputSubsystemInterface.h
// (UE 5.x source knowledge; reflected interface: UFUNCTION-marked members checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InputAction.h"
#include "InputMappingContext.h"

class UEnhancedPlayerInput;

struct FModifyContextOptions
{
	UE_STUB_STRUCT_BODY(FModifyContextOptions)
	FModifyContextOptions();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	uint8 bIgnoreAllPressedKeysUntilRelease : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	uint8 bForceImmediately : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	uint8 bNotifyUserSettings : 1;
};

class UEnhancedInputSubsystemInterface : public UInterface
{
	UE_STUB_CLASS_BODY(UEnhancedInputSubsystemInterface, UInterface)
};
class IEnhancedInputSubsystemInterface
{
	UE_STUB_IINTERFACE_BODY(IEnhancedInputSubsystemInterface, UEnhancedInputSubsystemInterface)
public:
	virtual UEnhancedPlayerInput* GetPlayerInput() const = 0;
	UFUNCTION(BlueprintCallable, Category = "Input", meta = (AutoCreateRefTerm = "Modifiers,Triggers"))
	virtual void InjectInputForAction(const UInputAction* Action, FInputActionValue RawValue, const TArray<UInputModifier*>& Modifiers, const TArray<UInputTrigger*>& Triggers);
	UFUNCTION(BlueprintCallable, Category = "Input", meta = (AutoCreateRefTerm = "Modifiers,Triggers"))
	virtual void InjectInputVectorForAction(const UInputAction* Action, FVector Value, const TArray<UInputModifier*>& Modifiers, const TArray<UInputTrigger*>& Triggers);
	UFUNCTION(BlueprintCallable, Category = "Input|MappingContext", meta = (AutoCreateRefTerm = "Options"))
	virtual void ClearAllMappings();
	UFUNCTION(BlueprintCallable, Category = "Input|MappingContext", meta = (AutoCreateRefTerm = "Options"))
	virtual void AddMappingContext(const UInputMappingContext* MappingContext, int32 Priority, const FModifyContextOptions& Options = FModifyContextOptions());
	UFUNCTION(BlueprintCallable, Category = "Input|MappingContext", meta = (AutoCreateRefTerm = "Options"))
	virtual void RemoveMappingContext(const UInputMappingContext* MappingContext, const FModifyContextOptions& Options = FModifyContextOptions());
	virtual bool HasMappingContext(const UInputMappingContext* MappingContext) const;
	UFUNCTION(BlueprintCallable, Category = "Input|MappingContext")
	virtual bool HasMappingContext(const UInputMappingContext* MappingContext, int32& OutFoundPriority) const;
	UFUNCTION(BlueprintCallable, Category = "Input|Mapping Queries")
	virtual TArray<FKey> QueryKeysMappedToAction(const UInputAction* Action) const;
};
