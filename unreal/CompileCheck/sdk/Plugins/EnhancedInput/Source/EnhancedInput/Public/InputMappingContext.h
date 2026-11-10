// Stub SDK. Real header: Engine/Plugins/EnhancedInput/Source/EnhancedInput/Public/InputMappingContext.h (UE 5.x source
// knowledge; reflected class: checked against the 5.6 stub). MapKey returns a reference into the mapping array.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EnhancedActionKeyMapping.h"
#include "InputAction.h"

class UInputMappingContext : public UDataAsset
{
	UE_STUB_CLASS_BODY(UInputMappingContext, UDataAsset)
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Description", meta = (DisplayName = "Description"))
	FText ContextDescription;
	const TArray<FEnhancedActionKeyMapping>& GetMappings() const;
	FEnhancedActionKeyMapping& GetMapping(TArray<FEnhancedActionKeyMapping>::SizeType Index);
	UFUNCTION(BlueprintCallable, Category = "Mapping")
	FEnhancedActionKeyMapping& MapKey(const UInputAction* Action, FKey ToKey);
	UFUNCTION(BlueprintCallable, Category = "Mapping")
	void UnmapKey(const UInputAction* Action, FKey Key);
	UFUNCTION(BlueprintCallable, Category = "Mapping")
	void UnmapAllKeysFromAction(const UInputAction* Action);
	UFUNCTION(BlueprintCallable, Category = "Mapping")
	void UnmapAll();
};
