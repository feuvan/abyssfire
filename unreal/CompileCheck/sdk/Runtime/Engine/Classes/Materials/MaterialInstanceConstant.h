// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Materials/MaterialInstanceConstant.h (UE 5.x source
// knowledge; UFUNCTION-marked members: checked against the 5.6 stub). The *EditorOnly setters exist only WITH_EDITOR.
#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInstance.h"

class UMaterialInstanceConstant : public UMaterialInstance
{
	UE_STUB_CLASS_BODY(UMaterialInstanceConstant, UMaterialInstance)
public:
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetScalarParameterValue", ScriptName = "GetScalarParameterValue", Keywords = "GetFloatParameterValue"), Category = "Rendering|Material")
	float K2_GetScalarParameterValue(FName ParameterName);
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetTextureParameterValue", ScriptName = "GetTextureParameterValue"), Category = "Rendering|Material")
	UTexture* K2_GetTextureParameterValue(FName ParameterName);
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetVectorParameterValue", ScriptName = "GetVectorParameterValue", Keywords = "GetColorParameterValue"), Category = "Rendering|Material")
	FLinearColor K2_GetVectorParameterValue(FName ParameterName);
#if WITH_EDITOR
	void SetParentEditorOnly(UMaterialInterface* NewParent, bool RecacheShader = true);
	void SetVectorParameterValueEditorOnly(const FMaterialParameterInfo& ParameterInfo, FLinearColor Value);
	void SetScalarParameterValueEditorOnly(const FMaterialParameterInfo& ParameterInfo, float Value);
	void SetTextureParameterValueEditorOnly(const FMaterialParameterInfo& ParameterInfo, UTexture* Value);
	void ClearParameterValuesEditorOnly();
#endif
};
