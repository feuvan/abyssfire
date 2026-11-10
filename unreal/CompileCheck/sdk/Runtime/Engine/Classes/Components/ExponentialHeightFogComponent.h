// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Components/ExponentialHeightFogComponent.h (UE 5.x source
// knowledge; UFUNCTION / UPROPERTY-marked members: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/SceneComponent.h"

class UTextureCube;

class UExponentialHeightFogComponent : public USceneComponent
{
	UE_STUB_CLASS_BODY(UExponentialHeightFogComponent, USceneComponent)
public:
	/** Global density factor. */
	UPROPERTY(BlueprintReadOnly, interp, Category = ExponentialHeightFogComponent, meta = (UIMin = "0", UIMax = ".05"))
	float FogDensity;
	/** Height density factor, controls how the density increases as height decreases. */
	UPROPERTY(BlueprintReadOnly, interp, Category = ExponentialHeightFogComponent, meta = (UIMin = "0.001", UIMax = "2"))
	float FogHeightFalloff;
	UPROPERTY(BlueprintReadOnly, interp, Category = ExponentialHeightFogComponent, meta = (DisplayName = "Fog Inscattering Color"))
	FLinearColor FogInscatteringLuminance;
	UPROPERTY(BlueprintReadOnly, interp, Category = DirectionalInscattering, meta = (DisplayName = "Directional Inscattering Color"))
	FLinearColor DirectionalInscatteringLuminance;
	/** Maximum opacity of the fog (1 = fully opaque, 0 = essentially invisible). */
	UPROPERTY(BlueprintReadOnly, interp, Category = ExponentialHeightFogComponent, meta = (UIMin = "0", UIMax = "1"))
	float FogMaxOpacity;
	/** Distance from the camera that the fog will start, in world units. */
	UPROPERTY(BlueprintReadOnly, interp, Category = ExponentialHeightFogComponent, meta = (UIMin = "0", UIMax = "5000"))
	float StartDistance;
	/** Scene elements past this distance will not have fog applied. */
	UPROPERTY(BlueprintReadOnly, interp, Category = ExponentialHeightFogComponent, meta = (UIMin = "100000", UIMax = "20000000"))
	float FogCutoffDistance;
	/** Whether to enable Volumetric fog. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = VolumetricFog, meta = (DisplayName = "Volumetric Fog"))
	bool bEnableVolumetricFog;

	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|ExponentialHeightFog")
	void SetFogDensity(float Value);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|ExponentialHeightFog")
	void SetSecondFogDensity(float Value);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|ExponentialHeightFog")
	void SetFogInscatteringColor(FLinearColor Value);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|ExponentialHeightFog")
	void SetInscatteringColorCubemap(UTextureCube* Value);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|ExponentialHeightFog")
	void SetDirectionalInscatteringExponent(float Value);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|ExponentialHeightFog")
	void SetDirectionalInscatteringStartDistance(float Value);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|ExponentialHeightFog")
	void SetDirectionalInscatteringColor(FLinearColor Value);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|ExponentialHeightFog")
	void SetSecondFogHeightOffset(float Value);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|ExponentialHeightFog")
	void SetFogHeightFalloff(float Value);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|ExponentialHeightFog")
	void SetSecondFogHeightFalloff(float Value);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|ExponentialHeightFog")
	void SetFogMaxOpacity(float Value);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|ExponentialHeightFog")
	void SetStartDistance(float Value);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|ExponentialHeightFog")
	void SetEndDistance(float Value);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|ExponentialHeightFog")
	void SetFogCutoffDistance(float Value);
	UFUNCTION(BlueprintCallable, Category = "Rendering|VolumetricFog")
	void SetVolumetricFog(bool bNewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|VolumetricFog")
	void SetVolumetricFogScatteringDistribution(float NewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|VolumetricFog")
	void SetVolumetricFogExtinctionScale(float NewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|VolumetricFog")
	void SetVolumetricFogAlbedo(FColor NewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|VolumetricFog")
	void SetVolumetricFogEmissive(FLinearColor NewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|VolumetricFog")
	void SetVolumetricFogDistance(float NewValue);
};
