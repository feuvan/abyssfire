// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/Attenuation.h (UE 5.x source knowledge;
// the enums and UPROPERTY-marked members: checked against the 5.6 stub). Only the distance model, the shape and the
// plain float / vector fields are declared (the custom curve FRuntimeFloatCurve is not modelled).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

UENUM(BlueprintType)
enum class EAttenuationDistanceModel : uint8
{
	Linear,
	Logarithmic,
	Inverse,
	LogReverse,
	NaturalSound,
	Custom,
};

UENUM(BlueprintType)
namespace EAttenuationShape
{
	enum Type : int
	{
		Sphere,
		Capsule,
		Box,
		Cone,
	};
}

/** Base class for attenuation settings. */
struct FBaseAttenuationSettings
{
	UE_STUB_STRUCT_BODY(FBaseAttenuationSettings)

	virtual ~FBaseAttenuationSettings() { }

	/** The type of attenuation as a function of distance to use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationDistance, meta = (DisplayName = "Attenuation Function"))
	EAttenuationDistanceModel DistanceAlgorithm;
	/** The shape of the non-custom attenuation method. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationDistance)
	TEnumAsByte<enum EAttenuationShape::Type> AttenuationShape;
	/** The attenuation volume at the falloff distance in decibels (only for the 'Natural Sound' distance algorithm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationDistance, meta = (DisplayName = "Attenuation At Max (dB)", ClampMin = "-60", ClampMax = "0"))
	float dBAttenuationAtMax;
	/** The dimensions to use for the attenuation shape. Interpretation of the values differ per shape. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationDistance)
	FVector AttenuationShapeExtents;
	/** The distance back from the sound's origin to begin the cone when using the cone attenuation shape. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationDistance, meta = (ClampMin = "0"))
	float ConeOffset;
	/** The distance over which volume attenuation occurs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationDistance, meta = (ClampMin = "0"))
	float FalloffDistance;
};
