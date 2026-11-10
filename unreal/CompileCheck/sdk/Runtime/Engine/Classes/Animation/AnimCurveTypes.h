// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Animation/AnimCurveTypes.h (UE 5.x source knowledge). Only
// the blended curve container that travels with a pose.
#pragma once

#include "CoreMinimal.h"
#include "BoneContainer.h"

struct FBlendedCurve
{
	void InitFrom(const FBoneContainer& RequiredBones);
	void Empty();
	bool IsValid() const;
};

struct FBlendedHeapCurve
{
	void InitFrom(const FBoneContainer& RequiredBones);
	void Empty();
};
