// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Components/LightComponentBase.h (UE 5.x source knowledge;
// UFUNCTION / UPROPERTY-marked members: checked against the 5.6 stub). Abstract in the engine.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/SceneComponent.h"

class ULightComponentBase : public USceneComponent
{
	UE_STUB_CLASS_BODY(ULightComponentBase, USceneComponent)
public:
	/** Total energy that the light emits. */
	UPROPERTY(BlueprintReadOnly, interp, Category = Light, meta = (DisplayName = "Intensity", UIMin = "0.0", UIMax = "20.0"))
	float Intensity;
	/** Filter color of the light (the bytes are sRGB; the renderer decodes them). */
	UPROPERTY(BlueprintReadOnly, interp, Category = Light, meta = (HideAlphaChannel))
	FColor LightColor;
	/** Whether the light can affect the world, or whether it is disabled. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Light)
	uint32 bAffectsWorld : 1;
	/** Whether the light should cast any shadows. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Light)
	uint32 CastShadows : 1;
	/** Whether the light should cast shadows from static objects. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Light, AdvancedDisplay)
	uint32 CastStaticShadows : 1;
	/** Whether the light should cast shadows from dynamic objects. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Light, AdvancedDisplay)
	uint32 CastDynamicShadows : 1;
	/** Whether the light affects translucency or not. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Light, AdvancedDisplay)
	uint32 bAffectTranslucentLighting : 1;
	/** Scales the indirect lighting contribution from this light. */
	UPROPERTY(BlueprintReadOnly, interp, Category = Light, meta = (UIMin = "0.0", UIMax = "6.0"))
	float IndirectLightingIntensity;
	/** Intensity of the volumetric scattering from this light. */
	UPROPERTY(BlueprintReadOnly, interp, Category = Light, meta = (UIMin = "0.25", UIMax = "4.0"))
	float VolumetricScatteringIntensity;
	/** Sets whether this light casts shadows. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetCastShadows(bool bNewValue);
	/** Gets the light color as a linear color. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	FLinearColor GetLightColor() const;
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetCastVolumetricShadow(bool bNewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetAffectReflection(bool bNewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetAffectGlobalIllumination(bool bNewValue);
	virtual void OnRegister() override;
};
