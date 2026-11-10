// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/ReferenceSkeleton.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"

struct FMeshBoneInfo
{
	FName Name;
	int32 ParentIndex;
};

struct FReferenceSkeleton
{
	FReferenceSkeleton(bool bInOnlyOneRootAllowed = true);
	int32 GetRawBoneNum() const;
	int32 GetNum() const;
	const TArray<FMeshBoneInfo>& GetRefBoneInfo() const;
	const TArray<FTransform>& GetRefBonePose() const;
	const TArray<FMeshBoneInfo>& GetRawRefBoneInfo() const;
	const TArray<FTransform>& GetRawRefBonePose() const;
	bool IsValidIndex(int32 Index) const;
	bool IsValidRawIndex(int32 Index) const;
	int32 FindBoneIndex(const FName& BoneName) const;
	int32 FindRawBoneIndex(const FName& BoneName) const;
	FName GetBoneName(const int32 BoneIndex) const;
	int32 GetParentIndex(const int32 BoneIndex) const;
	int32 GetRawParentIndex(const int32 BoneIndex) const;
	int32 GetDepthBetweenBones(const int32 BoneIndex, const int32 ParentBoneIndex) const;
	bool BoneIsChildOf(const int32 ChildBoneIndex, const int32 ParentBoneIndex) const;
};
