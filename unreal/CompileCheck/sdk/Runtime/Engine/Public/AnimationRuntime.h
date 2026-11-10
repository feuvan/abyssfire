// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/AnimationRuntime.h (UE 5.x source knowledge). The
// FAnimationPoseData overloads (UE 4.26+); the older FCompactPose / FBlendedCurve overloads are deprecated in the engine
// and not declared here.
#pragma once

#include "CoreMinimal.h"
#include "BonePose.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimationPoseData.h"

class FAnimationRuntime
{
public:
	/** Blends two poses together; Out = WeightOfPoseOne * One + (1 - WeightOfPoseOne) * Two. Curves and attributes too. */
	static void BlendTwoPosesTogether(const FAnimationPoseData& SourcePoseOneData, const FAnimationPoseData& SourcePoseTwoData, const float WeightOfPoseOne, /*out*/ FAnimationPoseData& OutAnimationPoseData);
	/** Accumulates an additive pose on a base pose with the given weight. */
	static void AccumulateAdditivePose(FAnimationPoseData& BaseAnimationPoseData, const FAnimationPoseData& AdditiveAnimationPoseData, float Weight, enum EAdditiveAnimationType AdditiveType);
	/** Accumulates a local-space additive pose. */
	static void AccumulateLocalSpaceAdditivePose(FAnimationPoseData& BaseAnimationPoseData, const FAnimationPoseData& AdditiveAnimationPoseData, float Weight);
	/** Accumulates a mesh-space rotation additive pose. */
	static void AccumulateMeshSpaceRotationAdditiveToLocalPose(FAnimationPoseData& BaseAnimationPoseData, const FAnimationPoseData& MeshSpaceRotationAdditiveAnimationPoseData, float Weight);
	/** Converts a pose to an additive pose against a base pose. */
	static void ConvertPoseToAdditive(FCompactPose& TargetPose, const FCompactPose& BasePose);
	static void ConvertPoseToMeshRotation(FCompactPose& LocalPose);
	static void ConvertMeshRotationPoseToLocalSpace(FCompactPose& Pose);
};
