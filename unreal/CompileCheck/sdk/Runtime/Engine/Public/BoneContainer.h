// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/BoneContainer.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "BoneIndices.h"
#include "ReferenceSkeleton.h"

class USkeleton;

struct FBoneContainer
{
	FBoneContainer();
	bool IsValid() const;
	UObject* GetAsset() const;
	USkeleton* GetSkeletonAsset(bool bEvenIfUnreachable = false) const;
	const FReferenceSkeleton& GetReferenceSkeleton() const;
	int32 GetNumBones() const;
	int32 GetCompactPoseNumBones() const;
	const TArray<FTransform>& GetRefPoseArray() const;
	const FTransform& GetRefPoseTransform(const FCompactPoseBoneIndex& BoneIndex) const;
	int32 GetPoseBoneIndexForBoneName(const FName& BoneName) const;
	FCompactPoseBoneIndex GetCompactPoseIndexFromSkeletonIndex(const int32 SkeletonIndex) const;
	FCompactPoseBoneIndex GetParentBoneIndex(const FCompactPoseBoneIndex& BoneIndex) const;
	FCompactPoseBoneIndex MakeCompactPoseIndex(const FMeshPoseBoneIndex& BoneIndex) const;
	FMeshPoseBoneIndex MakeMeshPoseIndex(const FCompactPoseBoneIndex& BoneIndex) const;
};
