// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Components/PoseableMeshComponent.h (UE 5.x source
// knowledge; UFUNCTION-marked members: checked against the 5.6 stub). USkeletalMeshComponent is only forward-declared
// here, as in the engine.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/SkinnedMeshComponent.h"

class USkeletalMeshComponent;

/** UPoseableMeshComponent that allows bone transforms to be driven by blueprint. */
class UPoseableMeshComponent : public USkinnedMeshComponent
{
	UE_STUB_CLASS_BODY(UPoseableMeshComponent, USkinnedMeshComponent)
public:
	UFUNCTION(BlueprintCallable, Category = "Components|PoseableMesh")
	void SetBoneTransformByName(FName BoneName, const FTransform& InTransform, EBoneSpaces::Type BoneSpace);
	UFUNCTION(BlueprintCallable, Category = "Components|PoseableMesh")
	void SetBoneLocationByName(FName BoneName, FVector InLocation, EBoneSpaces::Type BoneSpace);
	UFUNCTION(BlueprintCallable, Category = "Components|PoseableMesh")
	void SetBoneRotationByName(FName BoneName, FRotator InRotation, EBoneSpaces::Type BoneSpace);
	UFUNCTION(BlueprintCallable, Category = "Components|PoseableMesh")
	void SetBoneScaleByName(FName BoneName, FVector InScale3D, EBoneSpaces::Type BoneSpace);
	UFUNCTION(BlueprintCallable, Category = "Components|PoseableMesh")
	FTransform GetBoneTransformByName(FName BoneName, EBoneSpaces::Type BoneSpace);
	UFUNCTION(BlueprintCallable, Category = "Components|PoseableMesh")
	FVector GetBoneLocationByName(FName BoneName, EBoneSpaces::Type BoneSpace);
	UFUNCTION(BlueprintCallable, Category = "Components|PoseableMesh")
	FRotator GetBoneRotationByName(FName BoneName, EBoneSpaces::Type BoneSpace);
	UFUNCTION(BlueprintCallable, Category = "Components|PoseableMesh")
	FVector GetBoneScaleByName(FName BoneName, EBoneSpaces::Type BoneSpace);
	UFUNCTION(BlueprintCallable, Category = "Components|PoseableMesh")
	void ResetBoneTransformByName(FName BoneName);
	UFUNCTION(BlueprintCallable, Category = "Components|PoseableMesh")
	void CopyPoseFromSkeletalComponent(USkeletalMeshComponent* InComponentToCopy);
};
