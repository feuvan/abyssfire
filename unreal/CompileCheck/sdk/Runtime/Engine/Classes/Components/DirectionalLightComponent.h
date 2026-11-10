// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Components/DirectionalLightComponent.h (UE 5.x source
// knowledge; UFUNCTION / UPROPERTY-marked members: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/LightComponent.h"

class UDirectionalLightComponent : public ULightComponent
{
	UE_STUB_CLASS_BODY(UDirectionalLightComponent, ULightComponent)
public:
	/** How far Cascaded Shadow Map dynamic shadows will cover for a movable light, measured from the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CascadedShadowMaps, meta = (UIMin = "0", UIMax = "20000", DisplayName = "Dynamic Shadow Distance MovableLight"))
	float DynamicShadowDistanceMovableLight;
	/** How far Cascaded Shadow Map dynamic shadows will cover for a stationary light, measured from the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CascadedShadowMaps, meta = (UIMin = "0", UIMax = "20000", DisplayName = "Dynamic Shadow Distance StationaryLight"))
	float DynamicShadowDistanceStationaryLight;
	/** Number of cascades to split the view frustum into for the whole scene dynamic shadow. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CascadedShadowMaps, meta = (UIMin = "0", UIMax = "4", DisplayName = "Num Dynamic Shadow Cascades"))
	int32 DynamicShadowCascades;
	/** Controls whether the cascades are distributed closer to the camera (larger exponent) or further from the camera (smaller exponent). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CascadedShadowMaps, meta = (UIMin = "1", UIMax = "4", DisplayName = "Distribution Exponent"))
	float CascadeDistributionExponent;
	/** Proportion of the fade region between cascades. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CascadedShadowMaps, meta = (UIMin = "0", UIMax = "0.3", DisplayName = "Transition Fraction"))
	float CascadeTransitionFraction;
	/** Controls the size of the fade out region at the far extent of the dynamic shadow's influence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CascadedShadowMaps, meta = (UIMin = "0", UIMax = "1.0", DisplayName = "Distance Fadeout Fraction"))
	float ShadowDistanceFadeoutFraction;
	/** Angle subtended by light source in degrees (also known as angular diameter). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Light, meta = (UIMin = "0", UIMax = "5"), DisplayName = "Source Angle")
	float LightSourceAngle;

	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetDynamicShadowDistanceMovableLight(float NewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetDynamicShadowDistanceStationaryLight(float NewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetDynamicShadowCascades(int32 NewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetCascadeDistributionExponent(float NewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetCascadeTransitionFraction(float NewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetShadowDistanceFadeoutFraction(float NewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetShadowCascadeBiasDistribution(float NewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetEnableLightShaftOcclusion(bool bNewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetOcclusionMaskDarkness(float NewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetLightShaftOverrideDirection(FVector NewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetLightSourceAngle(float NewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetAtmosphereSunLight(bool bNewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetForwardShadingPriority(int32 NewValue);
};
