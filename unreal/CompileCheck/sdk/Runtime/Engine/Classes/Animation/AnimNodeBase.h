// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Animation/AnimNodeBase.h (UE 5.x source knowledge). The
// evaluation contexts; the node-graph types (FPoseLink, FAnimNode_Base hooks) are not modelled.
#pragma once

#include "CoreMinimal.h"
#include "BonePose.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AttributesRuntime.h"
#include "Animation/AnimTypes.h"

class UAnimInstance;
struct FAnimInstanceProxy;
struct FAnimNode_Base;

/** Base class for update / evaluate contexts. */
struct FAnimationBaseContext
{
public:
	FAnimInstanceProxy* AnimInstanceProxy;
	FAnimationBaseContext();
	FAnimationBaseContext(FAnimInstanceProxy* InAnimInstanceProxy);
	FAnimationBaseContext(const FAnimationBaseContext& InContext) = default;
	FAnimationBaseContext& operator=(const FAnimationBaseContext& InContext) = default;
	UObject* GetAnimInstanceObject() const;
	bool IsNodeRelevant() const;
};

struct FAnimationInitializeContext : public FAnimationBaseContext
{
	FAnimationInitializeContext(FAnimInstanceProxy* InAnimInstanceProxy);
};

struct FAnimationCacheBonesContext : public FAnimationBaseContext
{
	FAnimationCacheBonesContext(FAnimInstanceProxy* InAnimInstanceProxy);
};

struct FAnimationUpdateContext : public FAnimationBaseContext
{
	FAnimationUpdateContext(FAnimInstanceProxy* InAnimInstanceProxy = nullptr);
	FAnimationUpdateContext(FAnimInstanceProxy* InAnimInstanceProxy, float InDeltaTime);
	float GetFinalBlendWeight() const;
	float GetDeltaTime() const;
};

/** Evaluation context passed around during animation tree evaluation. */
struct FPoseContext : public FAnimationBaseContext
{
public:
	FCompactPose Pose;
	FBlendedCurve Curve;
	UE::Anim::FStackAttributeContainer CustomAttributes;

	// This constructor allocates a new uninitialized pose for the specified anim instance
	FPoseContext(FAnimInstanceProxy* InAnimInstanceProxy, bool bInExpectsAdditivePose = false);
	// This constructor allocates a new uninitialized pose, copying non-pose state from the source context
	FPoseContext(const FPoseContext& SourceContext, bool bInOverrideExpectsAdditivePose = false);
	void ResetToRefPose();
	void ResetToAdditiveIdentity();
	bool ContainsNaN() const;
	bool IsNormalized() const;
	FPoseContext& operator=(const FPoseContext& Other);
	bool ExpectsAdditivePose() const;
};
