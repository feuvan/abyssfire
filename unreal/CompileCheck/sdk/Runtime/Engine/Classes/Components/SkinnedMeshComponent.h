// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Components/SkinnedMeshComponent.h (UE 5.x source
// knowledge; UFUNCTION / UPROPERTY-marked members and the enums: checked against the 5.6 stub). Abstract in the engine.
// The leader / follower names are the 5.1+ ones (MasterPoseComponent before).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/MeshComponent.h"
#include "BoneContainer.h"
#include "ReferenceSkeleton.h"

class USkinnedAsset;
class USkeletalMesh;

/** Skinned Mesh Animation Tick option based on rendered or not. This dictates "TickPose and RefreshBoneTransforms". */
UENUM(BlueprintType)
enum class EVisibilityBasedAnimTickOption : uint8
{
	/** Tick animation and refresh bone transforms whether rendered or not. */
	AlwaysTickPoseAndRefreshBones,
	/** Tick animation whether rendered or not, refresh bone transforms only when rendered. */
	AlwaysTickPose,
	/** When rendered, tick pose and refresh bone transforms; otherwise just update montages and refresh bones when playing montages. */
	OnlyTickMontagesAndRefreshBonesWhenPlayingMontages,
	/** When rendered, tick pose and refresh bone transforms; otherwise just update montages and skip everything else. */
	OnlyTickMontagesWhenNotRendered,
	/** Tick only when rendered, and it will only RefreshBoneTransforms when rendered. */
	OnlyTickPoseWhenRendered,
};

/** Values for specifying bone space. */
UENUM()
namespace EBoneSpaces
{
	enum Type : int
	{
		/** Set absolute position of bone in world space. */
		WorldSpace,
		/** Set position of bone in SkeletalMeshComponent's reference frame. */
		ComponentSpace,
	};
}

/** PhysicsBody options when bone is hidden. */
UENUM()
enum EPhysBodyOp : int
{
	/** Don't do anything. */
	PBO_None,
	/** Terminate - if you terminate, you won't be able to re-init when unhidden. */
	PBO_Term,
	PBO_MAX,
};

class USkinnedMeshComponent : public UMeshComponent
{
	UE_STUB_CLASS_BODY(USkinnedMeshComponent, UMeshComponent)
public:
	/** If set, this SkeletalMeshComponent will not use its Animation or SpaceBases but the SpaceBases of the LeaderPoseComponent. */
	UPROPERTY(BlueprintReadOnly, Category = "Mesh")
	TWeakObjectPtr<USkinnedMeshComponent> LeaderPoseComponent;
	/** This is update frequency flag even when our Owner has not been rendered recently. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = Optimization)
	EVisibilityBasedAnimTickOption VisibilityBasedAnimTickOption;
	/** When true, we will just use the bounds from our LeaderPoseComponent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mesh)
	uint8 bUseBoundsFromLeaderPoseComponent : 1;
	/** If true, when updating bounds from a PhysicsAsset, consider all BodySetups, not just those flagged with bConsiderForBounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Optimization)
	uint8 bConsiderAllBodiesForBounds : 1;
	/** If true, animation updates rate is tuned by distance to the camera (URO). */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = Optimization)
	uint8 bEnableUpdateRateOptimizations : 1;
	/** If true, use a fixed bounds for the skeletal mesh instead of recomputing them from the pose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Optimization)
	uint8 bComponentUseFixedSkelBounds : 1;

	/** Change the SkinnedAsset that is rendered for this Component. Will re-initialize the animation tree etc. */
	UFUNCTION(BlueprintCallable, Category = "Components|SkinnedMesh")
	virtual void SetSkinnedAssetAndUpdate(class USkinnedAsset* NewMesh, bool bReinitPose = true);
	/** Get the SkinnedAsset rendered for this mesh. */
	UFUNCTION(BlueprintPure, Category = "Components|SkinnedMesh")
	USkinnedAsset* GetSkinnedAsset() const;
	/** Set LeaderPoseComponent for this component. */
	UFUNCTION(BlueprintCallable, Category = "Components|SkinnedMesh")
	void SetLeaderPoseComponent(USkinnedMeshComponent* NewLeaderBoneComponent, bool bForceUpdate = false, bool bInFollowerShouldTickPose = false);
	/** Find the index of bone by name. Looks in the current SkinnedAsset being used by this SkinnedMeshComponent. */
	UFUNCTION(BlueprintCallable, Category = "Components|SkinnedMesh")
	int32 GetBoneIndex(FName BoneName) const;
	/** Get Bone Name from index. */
	UFUNCTION(BlueprintCallable, Category = "Components|SkinnedMesh")
	FName GetBoneName(int32 BoneIndex) const;
	/** Returns bone name linked to a given named socket on the skeletal mesh component. */
	UFUNCTION(BlueprintCallable, Category = "Components|SkinnedMesh")
	FName GetSocketBoneName(FName InSocketName) const;
	/** Returns the number of bones in the skeleton. */
	UFUNCTION(BlueprintCallable, Category = "Components|SkinnedMesh")
	int32 GetNumBones() const;
	/** Get Parent Bone of the input bone. */
	UFUNCTION(BlueprintCallable, Category = "Components|SkinnedMesh")
	FName GetParentBone(FName BoneName) const;
	/** Set MinLodModel of the mesh component (0 = auto, >0 forces LOD - 1). */
	UFUNCTION(BlueprintCallable, Category = "Components|SkinnedMesh")
	void SetForcedLOD(int32 InNewForcedLOD);
	UFUNCTION(BlueprintCallable, Category = "Components|SkinnedMesh")
	int32 GetForcedLOD() const;
	/** Allows hiding of a particular material (by ID) on this instance of a SkeletalMesh. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void ShowMaterialSection(int32 MaterialID, int32 SectionIndex, bool bShow, int32 LODIndex);
	/** Hides the specified bone with name. Currently this just enforces a scale of 0 for the hidden bones. */
	UFUNCTION(BlueprintCallable, Category = "Components|SkinnedMesh")
	void HideBoneByName(FName BoneName, EPhysBodyOp PhysBodyOption);
	UFUNCTION(BlueprintCallable, Category = "Components|SkinnedMesh")
	void UnHideBoneByName(FName BoneName);
	/** Get Bone Transform by name, in the given space. */
	UFUNCTION(BlueprintCallable, Category = "Components|SkinnedMesh")
	FTransform GetBoneTransform(FName InBoneName, ERelativeTransformSpace TransformSpace = RTS_World) const;
	virtual bool DoesSocketExist(FName InSocketName) const override;
	virtual FTransform GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace = RTS_World) const override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual int32 GetNumMaterials() const override;
	virtual UMaterialInterface* GetMaterial(int32 MaterialIndex) const override;
	virtual int32 GetMaterialIndex(FName MaterialSlotName) const override;
	virtual TArray<FName> GetMaterialSlotNames() const override;
	virtual bool IsMaterialSlotNameValid(FName MaterialSlotName) const override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
