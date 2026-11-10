// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/Animation/AnimationPoseData.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"

struct FCompactPose;
struct FBlendedCurve;
struct FPoseContext;
struct FSlotEvaluationPose;
namespace UE { namespace Anim { struct FStackAttributeContainer; } }

/** A pose, its curves and its attributes, referenced as one (does not own them). */
struct FAnimationPoseData
{
	FAnimationPoseData(FPoseContext& InPoseContext);
	FAnimationPoseData(FSlotEvaluationPose& InSlotPoseContext);
	FAnimationPoseData(FCompactPose& InPose, FBlendedCurve& InCurve, UE::Anim::FStackAttributeContainer& InAttributes);

	FAnimationPoseData() = delete;
	FAnimationPoseData& operator=(FAnimationPoseData&& Other) = delete;

	const FCompactPose& GetPose() const;
	FCompactPose& GetPose();
	const FBlendedCurve& GetCurve() const;
	FBlendedCurve& GetCurve();
	const UE::Anim::FStackAttributeContainer& GetAttributes() const;
	UE::Anim::FStackAttributeContainer& GetAttributes();
};
