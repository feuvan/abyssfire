// Stub SDK. Real header: Engine/Plugins/EnhancedInput/Source/EnhancedInput/Public/EnhancedActionKeyMapping.h (UE 5.x
// source knowledge; reflected struct: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "InputAction.h"

struct FEnhancedActionKeyMapping
{
	UE_STUB_STRUCT_BODY(FEnhancedActionKeyMapping)
	FEnhancedActionKeyMapping(const UInputAction* InAction = nullptr, const FKey InKey = EKeys::Invalid);
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Input")
	TArray<TObjectPtr<UInputTrigger>> Triggers;
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Input")
	TArray<TObjectPtr<UInputModifier>> Modifiers;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<const UInputAction> Action;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	FKey Key;
	bool operator==(const FEnhancedActionKeyMapping& Other) const;
};
