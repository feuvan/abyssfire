// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Sound/SoundBase.h (UE 5.x source knowledge;
// UPROPERTY-marked members: checked against the 5.6 stub). Abstract in the engine (UCLASS(abstract)). The real class
// also implements IInterface_AssetUserData (not modelled). IsLooping is a C++-only virtual (not reflected; see the
// lower-confidence table of API_UNCERTAIN.md); GetDuration / GetMaxDistance exist too and are not declared (unused).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

class USoundClass;

/** The base class for a playable sound object. */
class USoundBase : public UObject
{
	UE_STUB_CLASS_BODY(USoundBase, UObject)
public:
	/** Duration of sound in seconds. */
	UPROPERTY(Category = Developer, AssetRegistrySearchable, VisibleAnywhere, BlueprintReadOnly)
	float Duration;
	/** The max distance of the asset, as determined by attenuation settings. */
	UPROPERTY(Category = Developer, AssetRegistrySearchable, VisibleAnywhere, BlueprintReadOnly)
	float MaxDistance;
	/** Total number of samples (in the thousands). */
	UPROPERTY(Category = Developer, AssetRegistrySearchable, VisibleAnywhere, BlueprintReadOnly)
	float TotalSamples;

	/** Returns whether the sound base is set up in a looping manner. */
	virtual bool IsLooping() const;
};
