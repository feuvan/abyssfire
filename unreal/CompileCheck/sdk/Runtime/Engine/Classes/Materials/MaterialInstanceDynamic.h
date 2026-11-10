// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Materials/MaterialInstanceDynamic.h (UE 5.x source
// knowledge; UFUNCTION-marked members: checked against the 5.6 stub). The FVector / FVector4 overloads of
// SetVectorParameterValue are the engine's inline C++ conveniences.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialInstance.h"

class UTexture;

class UMaterialInstanceDynamic : public UMaterialInstance
{
	UE_STUB_CLASS_BODY(UMaterialInstanceDynamic, UMaterialInstance)
public:
	/** Set a MID scalar (float) parameter value. */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "SetFloatParameterValue"), Category = "Rendering|Material")
	void SetScalarParameterValue(FName ParameterName, float Value);
	/** Set a MID scalar (float) parameter value using MPI (to allow access to layer parameters). */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "SetFloatParameterValue"), Category = "Rendering|Material")
	void SetScalarParameterValueByInfo(const FMaterialParameterInfo& ParameterInfo, float Value);
	// NOTE: These Index-related functions should be used VERY carefully, and only in cases where optimization is critical.
	UFUNCTION(BlueprintCallable, meta = (Keywords = "SetFloatParameterValue"), Category = "Rendering|Material")
	bool InitializeScalarParameterAndGetIndex(const FName& ParameterName, float Value, int32& OutParameterIndex);
	UFUNCTION(BlueprintCallable, meta = (Keywords = "SetFloatParameterValue"), Category = "Rendering|Material")
	bool SetScalarParameterByIndex(int32 ParameterIndex, float Value);
	/** Get the current scalar (float) parameter value from an MID. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get Scalar Parameter Value", ScriptName = "GetScalarParameterValue", Keywords = "GetFloatParameterValue"), Category = "Rendering|Material")
	float K2_GetScalarParameterValue(FName ParameterName);
	/** Set an MID texture parameter value. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetTextureParameterValue(FName ParameterName, class UTexture* Value);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetTextureParameterValueByInfo(const FMaterialParameterInfo& ParameterInfo, class UTexture* Value);
	/** Get the current MID texture parameter value. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get Texture Parameter Value", ScriptName = "GetTextureParameterValue"), Category = "Rendering|Material")
	class UTexture* K2_GetTextureParameterValue(FName ParameterName);
	/** Set an MID vector parameter value. */
	UFUNCTION(BlueprintCallable, meta = (Keywords = "SetColorParameterValue"), Category = "Rendering|Material")
	void SetVectorParameterValue(FName ParameterName, FLinearColor Value);
	void SetVectorParameterValue(FName ParameterName, const FVector& Value) { SetVectorParameterValue(ParameterName, FLinearColor(Value)); }
	void SetVectorParameterValue(FName ParameterName, const FVector4& Value) { SetVectorParameterValue(ParameterName, FLinearColor(Value)); }
	UFUNCTION(BlueprintCallable, meta = (Keywords = "SetColorParameterValue"), Category = "Rendering|Material")
	void SetVectorParameterValueByInfo(const FMaterialParameterInfo& ParameterInfo, FLinearColor Value);
	/** Get the current MID vector parameter value. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get Vector Parameter Value", ScriptName = "GetVectorParameterValue", Keywords = "GetColorParameterValue"), Category = "Rendering|Material")
	FLinearColor K2_GetVectorParameterValue(FName ParameterName);
	/** Copies over parameters given a material interface (copy each instance following the hierarchy). */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Copy Material Instance Parameters", ScriptName = "CopyMaterialInstanceParameters"), Category = "Rendering|Material")
	void K2_CopyMaterialInstanceParameters(UMaterialInterface* Source, bool bQuickParametersOnly = false);
	/** Copy parameter values from another material instance. This will copy only parameters explicitly overridden in that material instance. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void CopyParameterOverrides(UMaterialInstance* MaterialInstance);
	/** Create a material instance dynamic parented to the specified material. */
	static UMaterialInstanceDynamic* Create(class UMaterialInterface* ParentMaterial, class UObject* InOuter);
	/** Create a material instance dynamic parented to the specified material with the specified name. */
	static UMaterialInstanceDynamic* Create(class UMaterialInterface* ParentMaterial, class UObject* InOuter, FName Name);
};
