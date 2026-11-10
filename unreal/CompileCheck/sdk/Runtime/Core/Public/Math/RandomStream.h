// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Math/RandomStream.h (from UE 5.x source knowledge).
// Not part of CoreMinimal.h: include it where FRandomStream is used.
#pragma once

#include "CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Math/Box.h"
#include "UObject/NameTypes.h"

struct FRandomStream
{
	FRandomStream();
	FRandomStream(int32 InSeed);
	FRandomStream(FName InName);
	void Initialize(int32 InSeed);
	void Initialize(FName InName);
	void Reset() const;
	int32 GetInitialSeed() const;
	void GenerateNewSeed();
	float GetFraction() const;
	uint32 GetUnsignedInt() const;
	FVector GetUnitVector() const;
	int32 GetCurrentSeed() const;
	float FRand() const;
	int32 RandHelper(int32 A) const;
	int32 RandRange(int32 Min, int32 Max) const;
	float FRandRange(float InMin, float InMax) const;
	FVector VRand() const;
	FVector RandPointInBox(const FBox& Box) const;
	FVector VRandCone(FVector const& Dir, float ConeHalfAngleRad) const;
	FVector VRandCone(FVector const& Dir, float HorizontalConeHalfAngleRad, float VerticalConeHalfAngleRad) const;
	FString ToString() const;
};
