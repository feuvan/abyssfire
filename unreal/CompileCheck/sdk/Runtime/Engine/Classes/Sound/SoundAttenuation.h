// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Sound/SoundAttenuation.h (UE 5.x source knowledge;
// UPROPERTY-marked members: checked against the 5.6 stub). Only the enable flags and the plain float fields of
// FSoundAttenuationSettings are declared (the engine has about seventy more: curves, enums, plugin settings).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "Engine/Attenuation.h"

/** The settings for attenuating: distance attenuation, spatialization, air absorption, focus, occlusion, reverb send. */
struct FSoundAttenuationSettings : public FBaseAttenuationSettings
{
	UE_STUB_STRUCT_BODY(FSoundAttenuationSettings)

	/** Allows distance-based volume attenuation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationDistance, meta = (DisplayName = "Enable Volume Attenuation"))
	uint8 bAttenuate : 1;
	/** Allows the source to be 3D spatialized. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationSpatialization, meta = (DisplayName = "Enable Spatialization"))
	uint8 bSpatialize : 1;
	/** Allows simulation of air absorption by applying a filter with a cutoff frequency as a function of distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationAirAbsorption, meta = (DisplayName = "Enable Air Absorption"))
	uint8 bAttenuateWithLPF : 1;
	/** Enable listener focus-based adjustments. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationListenerFocus)
	uint8 bEnableListenerFocus : 1;
	/** Enables focus interpolation to smooth transition in and and of focus. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationListenerFocus)
	uint8 bEnableFocusInterpolation : 1;
	/** Enables realtime occlusion tracing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationOcclusion)
	uint8 bEnableOcclusion : 1;
	/** Enables tracing against complex collision when doing occlusion traces. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationOcclusion)
	uint8 bUseComplexCollisionForOcclusion : 1;
	/** Enables adjusting reverb sends based on distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationReverbSend, meta = (DisplayName = "Enable Reverb Send"))
	uint8 bEnableReverbSend : 1;
	/** Enables attenuation of sound priority based off distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationPriority, meta = (DisplayName = "Enable Priority Attenuation"))
	uint8 bEnablePriorityAttenuation : 1;
	/** Enables applying a -6 dB attenuation to stereo assets which are 3d spatialized. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationSpatialization, meta = (DisplayName = "Normalize 3D Stereo Sounds"))
	uint8 bApplyNormalizationToStereoSounds : 1;

	/** The amount of stereo spread to use when rendering a stereo or multichannel asset as 3d. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationSpatialization, meta = (ClampMin = "0", EditCondition = "bSpatialize", DisplayName = "3D Stereo Spread"))
	float StereoSpread;
	/** The interpolation speed (in seconds) of the occlusion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationOcclusion, meta = (ClampMin = "0", UIMin = "0.0", EditCondition = "bEnableOcclusion"))
	float OcclusionInterpolationTime;
};

/** Defines how a sound changes volume with distance to the listener. */
class USoundAttenuation : public UObject
{
	UE_STUB_CLASS_BODY(USoundAttenuation, UObject)
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (CustomizeProperty))
	FSoundAttenuationSettings Attenuation;
};
