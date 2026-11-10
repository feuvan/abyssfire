// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Animation/AnimationAsset.h (UE 5.x source knowledge;
// UFUNCTION-marked members: checked against the 5.6 stub). Abstract in the engine (UCLASS(abstract)).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Animation/AnimTypes.h"

class USkeleton;

/** Previous / delta time of an asset player, for root motion and notify extraction. */
struct FDeltaTimeRecord
{
public:
	void Set(float InPrevious, float InDelta) { Previous = InPrevious; Delta = InDelta; bPreviousIsValid = true; }
	void SetPrevious(float InPrevious) { Previous = InPrevious; bPreviousIsValid = true; }
	float GetPrevious() const { return Previous; }
	bool IsPreviousValid() const { return bPreviousIsValid; }

	float Delta = 0.f;

	FDeltaTimeRecord() = default;
	explicit FDeltaTimeRecord(float InDeltaTime) : Delta(InDeltaTime), Previous(0.f), bPreviousIsValid(false) {}
private:
	float Previous = 0.f;
	bool bPreviousIsValid = false;
};

/** Parameters of one pose extraction (UAnimSequenceBase::GetAnimationPose). Time is a double since UE 5.1. */
struct FAnimExtractContext
{
	/** Position in animation to extract pose from. */
	double CurrentTime;
	/** Is root motion being extracted? */
	bool bExtractRootMotion;
	/** Delta time range required for root motion extraction. */
	FDeltaTimeRecord DeltaTimeRecord;
	/** Is the current animation asset marked as looping? */
	bool bLooping;
	/** Bones required for the pose (empty = all). */
	TArray<bool> BonesRequired;

	FAnimExtractContext(double InCurrentTime = 0.0, bool InbExtractRootMotion = false, FDeltaTimeRecord InDeltaTimeRecord = {}, bool InbLooping = false);
	bool IsBoneRequired(int32 BoneIndex) const;
};

class UAnimationAsset : public UObject
{
	UE_STUB_CLASS_BODY(UAnimationAsset, UObject)
public:
	void SetSkeleton(USkeleton* NewSkeleton);
	UFUNCTION(BlueprintPure, Category = "Animation")
	USkeleton* GetSkeleton() const;
	UFUNCTION(BlueprintPure, Category = "Animation")
	virtual float GetPlayLength() const;
	virtual bool IsValidAdditive() const;
	virtual void PostLoad() override;
};
