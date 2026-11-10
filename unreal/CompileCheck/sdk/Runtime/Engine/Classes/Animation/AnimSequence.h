// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Animation/AnimSequence.h (UE 5.x source knowledge;
// UPROPERTY-marked members: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimTypes.h"

struct FCompactPose;

class UAnimSequence : public UAnimSequenceBase
{
	UE_STUB_CLASS_BODY(UAnimSequence, UAnimSequenceBase)
public:
	/** Additive animation type. */
	UPROPERTY(EditAnywhere, Category = AdditiveSettings, AssetRegistrySearchable)
	TEnumAsByte<enum EAdditiveAnimationType> AdditiveAnimType;
	/** Additive reference pose type. */
	UPROPERTY(EditAnywhere, Category = AdditiveSettings)
	TEnumAsByte<enum EAdditiveBasePoseType> RefPoseType;
	/** Additive reference animation if relevant. */
	UPROPERTY(EditAnywhere, Category = AdditiveSettings)
	TObjectPtr<class UAnimSequence> RefPoseSeq;
	/** If this is on, it will allow extracting of root motion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RootMotion)
	bool bEnableRootMotion;
	virtual void GetAnimationPose(FAnimationPoseData& OutAnimationPoseData, const FAnimExtractContext& ExtractionContext) const override;
	void GetBonePose(FAnimationPoseData& OutAnimationPoseData, const FAnimExtractContext& ExtractionContext, bool bForceUseRawData = false) const;
	virtual bool IsValidAdditive() const override;
	virtual bool HasRootMotion() const override;
	virtual EAdditiveAnimationType GetAdditiveAnimType() const override;
	virtual int32 GetNumberOfSampledKeys() const override;
	virtual FFrameRate GetSamplingFrameRate() const override;
	virtual void PostLoad() override;
};
