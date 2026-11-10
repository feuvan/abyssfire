// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Components/LightComponent.h (UE 5.x source knowledge;
// UFUNCTION / UPROPERTY-marked members: checked against the 5.6 stub). Abstract in the engine. SetLightFColor (the
// sRGB bytes as they are stored) is the C++-only twin of SetLightColor.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/LightComponentBase.h"
#include "Engine/EngineTypes.h"

class UMaterialInterface;

class ULightComponent : public ULightComponentBase
{
	UE_STUB_CLASS_BODY(ULightComponent, ULightComponentBase)
public:
	/** Color temperature in Kelvin of the blackbody illuminant (White (D65) is 6500K). */
	UPROPERTY(BlueprintReadOnly, interp, Category = Light, meta = (UIMin = "1700.0", UIMax = "12000.0"))
	float Temperature;
	/** false: use white (D65) as illuminant. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Light, meta = (DisplayName = "Use Temperature"))
	uint32 bUseTemperature : 1;
	/** Controls how accurate self shadowing of whole scene shadows from this light are. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Light, AdvancedDisplay, meta = (UIMin = "0", UIMax = "1"))
	float ShadowBias;
	/** Controls how accurate self shadowing of whole scene shadows from this light are across faces with different slopes. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Light, AdvancedDisplay, meta = (UIMin = "0", UIMax = "1"))
	float ShadowSlopeBias;
	/** Amount to sharpen shadow filtering. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Light, AdvancedDisplay, meta = (UIMin = "0.0", UIMax = "1.0"))
	float ShadowSharpen;
	/** Length of screen space ray trace for sharp contact shadows. Zero is disabled. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Light, AdvancedDisplay, meta = (UIMin = "0.0", UIMax = "0.1"))
	float ContactShadowLength;
	/** Multiplier on specular highlights. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Light, AdvancedDisplay, meta = (UIMin = "0", UIMax = "1"))
	float SpecularScale;
	/** Channels that this light should affect. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = Light)
	FLightingChannels LightingChannels;

	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetIntensity(float NewIntensity);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetIndirectLightingIntensity(float NewIntensity);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetVolumetricScatteringIntensity(float NewIntensity);
	/** Set color of the light (bSRGB: NewLightColor is in sRGB, otherwise linear). */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetLightColor(FLinearColor NewLightColor, bool bSRGB = true);
	/** Set color of the light. */
	void SetLightFColor(FColor NewLightColor);
	/** Set color temperature of the light. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetTemperature(float NewTemperature);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetUseTemperature(bool bNewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetLightFunctionMaterial(UMaterialInterface* NewLightFunctionMaterial);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetAffectTranslucentLighting(bool bNewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetTransmission(bool bNewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetShadowBias(float NewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetShadowSlopeBias(float NewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetSpecularScale(float NewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetLightingChannels(bool bChannel0, bool bChannel1, bool bChannel2);
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
};
