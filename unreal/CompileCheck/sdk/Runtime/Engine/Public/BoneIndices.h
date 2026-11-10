// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/BoneIndices.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"

typedef uint16 FBoneIndexType;

struct FBoneIndexBase
{
	FBoneIndexBase() : BoneIndex(INDEX_NONE) {}
	int32 GetInt() const { return BoneIndex; }
	bool IsRootBone() const { return BoneIndex == 0; }
	bool IsValid() const { return BoneIndex != INDEX_NONE; }
	explicit operator int32() const { return BoneIndex; }
	explicit operator bool() const { return IsValid(); }
protected:
	int32 BoneIndex;
};

struct FCompactPoseBoneIndex : public FBoneIndexBase
{
	explicit FCompactPoseBoneIndex(int32 InBoneIndex) { BoneIndex = InBoneIndex; }
	bool operator==(const FCompactPoseBoneIndex& Rhs) const { return BoneIndex == Rhs.BoneIndex; }
	bool operator!=(const FCompactPoseBoneIndex& Rhs) const { return BoneIndex != Rhs.BoneIndex; }
	bool operator<(const FCompactPoseBoneIndex& Rhs) const { return BoneIndex < Rhs.BoneIndex; }
	FCompactPoseBoneIndex& operator++() { ++BoneIndex; return *this; }
};

struct FMeshPoseBoneIndex : public FBoneIndexBase
{
	explicit FMeshPoseBoneIndex(int32 InBoneIndex) { BoneIndex = InBoneIndex; }
	bool operator==(const FMeshPoseBoneIndex& Rhs) const { return BoneIndex == Rhs.BoneIndex; }
};

struct FSkeletonPoseBoneIndex : public FBoneIndexBase
{
	explicit FSkeletonPoseBoneIndex(int32 InBoneIndex) { BoneIndex = InBoneIndex; }
	bool operator==(const FSkeletonPoseBoneIndex& Rhs) const { return BoneIndex == Rhs.BoneIndex; }
};
