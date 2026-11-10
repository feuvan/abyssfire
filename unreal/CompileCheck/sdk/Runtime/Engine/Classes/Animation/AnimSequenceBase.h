// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Animation/AnimSequenceBase.h (UE 5.x source knowledge;
// UPROPERTY-marked members: checked against the 5.6 stub). Abstract in the engine (UCLASS(abstract)).
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimTypes.h"
#include "Misc/FrameRate.h"

struct FAnimationPoseData;

class UAnimSequenceBase : public UAnimationAsset
{
	UE_STUB_CLASS_BODY(UAnimSequenceBase, UAnimationAsset)
public:
	/** Number for tweaking playback rate of this animation globally. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Animation)
	float RateScale;
	/** The default looping behavior of this animation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Animation)
	bool bLoop;
	virtual float GetPlayLength() const override;
	/** Extracts the pose (bones, curves, attributes) at the context's time. */
	virtual void GetAnimationPose(FAnimationPoseData& OutPoseData, const FAnimExtractContext& ExtractionContext) const PURE_VIRTUAL(UAnimSequenceBase::GetAnimationPose, );
	virtual bool HasRootMotion() const;
	virtual EAdditiveAnimationType GetAdditiveAnimType() const;
	virtual int32 GetNumberOfSampledKeys() const;
	virtual FFrameRate GetSamplingFrameRate() const;
	virtual void PostLoad() override;
};
