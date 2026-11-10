// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/BonePose.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "BoneIndices.h"
#include "BoneContainer.h"
#include "Misc/MemStack.h"

typedef TMemStackAllocator<> FAnimStackAllocator;

template <class BoneIndexType, typename InAllocator>
struct FBasePose
{
public:
	int32 GetNumBones() const UE_STUB_BODY
	FTransform& operator[](const BoneIndexType& BoneIndex) UE_STUB_BODY
	const FTransform& operator[](const BoneIndexType& BoneIndex) const UE_STUB_BODY
	TArrayView<FTransform> GetMutableBones() UE_STUB_BODY
	bool IsValidIndex(const BoneIndexType& BoneIndex) const UE_STUB_BODY
};

template <typename InAllocator>
struct FBaseCompactPose : FBasePose<FCompactPoseBoneIndex, InAllocator>
{
public:
	typedef FCompactPoseBoneIndex BoneIndexType;
	FBaseCompactPose() UE_STUB_BODY
	void SetBoneContainer(const FBoneContainer* InBoneContainer) UE_STUB_BODY
	void CopyAndAssignBoneContainer(FBoneContainer& NewBoneContainer) UE_STUB_BODY
	template <typename OtherAllocator>
	void CopyBonesFrom(const FBaseCompactPose<OtherAllocator>& SrcPose) UE_STUB_BODY
	void Empty() UE_STUB_BODY
	const FBoneContainer& GetBoneContainer() const UE_STUB_BODY
	FBoneContainer& GetBoneContainer() UE_STUB_BODY
	void SetBoneContainer(FBoneContainer* InBoneContainer) UE_STUB_BODY
	bool IsValid() const UE_STUB_BODY
	FCompactPoseBoneIndex GetParentBoneIndex(const FCompactPoseBoneIndex& BoneIndex) const UE_STUB_BODY
	/** Sets this pose to its ref pose. */
	void ResetToRefPose() UE_STUB_BODY
	void ResetToRefPose(const FBoneContainer& RequiredBones) UE_STUB_BODY
	/** Sets every bone transform to Identity. */
	void ResetToAdditiveIdentity() UE_STUB_BODY
	/** Returns true if all bone rotations are normalized. */
	bool IsNormalized() const UE_STUB_BODY
	/** Returns true if any bone rotation contains NaN or Inf. */
	bool ContainsNaN() const UE_STUB_BODY
	/** Normalizes all rotations in this pose. */
	void NormalizeRotations() UE_STUB_BODY
	const FTransform& GetRefPose(const FCompactPoseBoneIndex& BoneIndex) const UE_STUB_BODY
};

struct FCompactPose : public FBaseCompactPose<FAnimStackAllocator>
{
};

struct FCompactHeapPose : public FBaseCompactPose<FDefaultAllocator>
{
};
