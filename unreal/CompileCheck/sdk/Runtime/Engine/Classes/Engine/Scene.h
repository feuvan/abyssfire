// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/Scene.h (UE 5.x source knowledge; reflected struct:
// checked against the 5.6 stub). FPostProcessSettings: each setting has a bOverride_<Name> bit next to the value; only
// the settings a stylised top-down game plausibly drives are declared (add the rest as needed, names as in the engine).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptInterface.h"

class UTexture;
class UTextureCube;

UENUM()
enum EAutoExposureMethod : int
{
	AEM_Histogram,
	AEM_Basic,
	AEM_Manual,
	AEM_MAX,
};
UENUM()
enum EBloomMethod : int
{
	BM_SOG,
	BM_FFT,
	BM_MAX,
};

struct FWeightedBlendable
{
	UE_STUB_STRUCT_BODY(FWeightedBlendable)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FWeightedBlendable)
	float Weight;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FWeightedBlendable)
	TObjectPtr<UObject> Object;
	FWeightedBlendable();
	FWeightedBlendable(float InWeight, UObject* InObject);
};
struct FWeightedBlendables
{
	UE_STUB_STRUCT_BODY(FWeightedBlendables)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Postprocess Materials")
	TArray<FWeightedBlendable> Array;
};

struct FPostProcessSettings
{
	UE_STUB_STRUCT_BODY(FPostProcessSettings)
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_WhiteTemp : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_WhiteTint : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_ColorSaturation : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_ColorContrast : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_ColorGamma : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_ColorGain : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_ColorOffset : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_FilmSlope : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_FilmToe : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_FilmShoulder : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_FilmBlackClip : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_FilmWhiteClip : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_SceneColorTint : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_BloomMethod : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_BloomIntensity : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_BloomThreshold : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_AutoExposureMethod : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_AutoExposureBias : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_AutoExposureMinBrightness : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_AutoExposureMaxBrightness : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_VignetteIntensity : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_FilmGrainIntensity : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_AmbientOcclusionIntensity : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_AmbientOcclusionRadius : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_MotionBlurAmount : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_DepthOfFieldFocalDistance : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_DepthOfFieldFstop : 1;
	UPROPERTY(BlueprintReadWrite, Category = Overrides)
	uint8 bOverride_SceneFringeIntensity : 1;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Temperature")
	float WhiteTemp;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Temperature")
	float WhiteTint;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Global")
	FVector4 ColorSaturation;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Global")
	FVector4 ColorContrast;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Global")
	FVector4 ColorGamma;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Global")
	FVector4 ColorGain;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Global")
	FVector4 ColorOffset;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Film")
	float FilmSlope;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Film")
	float FilmToe;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Film")
	float FilmShoulder;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Film")
	float FilmBlackClip;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Film")
	float FilmWhiteClip;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Misc")
	FLinearColor SceneColorTint;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens|Bloom")
	TEnumAsByte<enum EBloomMethod> BloomMethod;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Lens|Bloom")
	float BloomIntensity;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Lens|Bloom")
	float BloomThreshold;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens|Exposure")
	TEnumAsByte<enum EAutoExposureMethod> AutoExposureMethod;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Lens|Exposure")
	float AutoExposureBias;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Lens|Exposure")
	float AutoExposureMinBrightness;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Lens|Exposure")
	float AutoExposureMaxBrightness;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Lens|Image Effects")
	float VignetteIntensity;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Film Grain")
	float FilmGrainIntensity;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Rendering Features|Ambient Occlusion")
	float AmbientOcclusionIntensity;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Rendering Features|Ambient Occlusion")
	float AmbientOcclusionRadius;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Rendering Features|Motion Blur")
	float MotionBlurAmount;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Lens|Depth of Field")
	float DepthOfFieldFocalDistance;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Lens|Depth of Field")
	float DepthOfFieldFstop;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Lens|Chromatic Aberration")
	float SceneFringeIntensity;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering Features")
	FWeightedBlendables WeightedBlendables;
	FPostProcessSettings();
	void AddBlendable(TScriptInterface<class IBlendableInterface> InBlendableObject, float InWeight);
	void RemoveBlendable(TScriptInterface<class IBlendableInterface> InBlendableObject);
};
