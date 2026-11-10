// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/SkinnedAsset.h (UE 5.1+; UE 5.x source knowledge;
// UFUNCTION-marked members: checked against the 5.6 stub). The abstract base of USkeletalMesh.
#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableRenderAsset.h"
#include "Engine/SkinnedAssetCommon.h"
#include "ReferenceSkeleton.h"

class USkeleton;
class USkeletalMeshSocket;
class UPhysicsAsset;
class FSkeletalMeshRenderData;

class USkinnedAsset : public UStreamableRenderAsset
{
	UE_STUB_CLASS_BODY(USkinnedAsset, UStreamableRenderAsset)
public:
	virtual FReferenceSkeleton& GetRefSkeleton() PURE_VIRTUAL(USkinnedAsset::GetRefSkeleton, static FReferenceSkeleton Dummy; return Dummy;);
	virtual const FReferenceSkeleton& GetRefSkeleton() const PURE_VIRTUAL(USkinnedAsset::GetRefSkeleton, static FReferenceSkeleton Dummy; return Dummy;);
	virtual FSkeletalMeshLODInfo* GetLODInfo(int32 Index) PURE_VIRTUAL(USkinnedAsset::GetLODInfo, return nullptr;);
	virtual const FSkeletalMeshLODInfo* GetLODInfo(int32 Index) const PURE_VIRTUAL(USkinnedAsset::GetLODInfo, return nullptr;);
	virtual const TArray<FSkeletalMeshLODInfo>& GetLODInfoArray() const PURE_VIRTUAL(USkinnedAsset::GetLODInfoArray, static const TArray<FSkeletalMeshLODInfo> Dummy; return Dummy;);
	virtual int32 GetLODNum() const PURE_VIRTUAL(USkinnedAsset::GetLODNum, return 0;);
	virtual bool IsValidLODIndex(int32 Index) const PURE_VIRTUAL(USkinnedAsset::IsValidLODIndex, return false;);
	virtual const TArray<FSkeletalMaterial>& GetMaterials() const PURE_VIRTUAL(USkinnedAsset::GetMaterials, static const TArray<FSkeletalMaterial> Dummy; return Dummy;);
	virtual USkeleton* GetSkeleton() PURE_VIRTUAL(USkinnedAsset::GetSkeleton, return nullptr;);
	virtual const USkeleton* GetSkeleton() const PURE_VIRTUAL(USkinnedAsset::GetSkeleton, return nullptr;);
	virtual UPhysicsAsset* GetPhysicsAsset() const PURE_VIRTUAL(USkinnedAsset::GetPhysicsAsset, return nullptr;);
	virtual FBoxSphereBounds GetBounds() const PURE_VIRTUAL(USkinnedAsset::GetBounds, return FBoxSphereBounds(););
	virtual FSkeletalMeshRenderData* GetResourceForRendering() const PURE_VIRTUAL(USkinnedAsset::GetResourceForRendering, return nullptr;);
	virtual const TArray<FMatrix44f>& GetRefBasesInvMatrix() const PURE_VIRTUAL(USkinnedAsset::GetRefBasesInvMatrix, static const TArray<FMatrix44f> Dummy; return Dummy;);
	UFUNCTION(BlueprintCallable, Category = "Animation")
	virtual USkeletalMeshSocket* FindSocket(FName InSocketName) const;
	virtual bool IsCompiling() const;
};
