// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/SkeletalMesh.h (UE 5.x source knowledge;
// UFUNCTION-marked members: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/SkeletalMeshSocket.h"
#include "ReferenceSkeleton.h"

class UMorphTarget;

class USkeletalMesh : public USkinnedAsset
{
	UE_STUB_CLASS_BODY(USkeletalMesh, USkinnedAsset)
public:
	virtual FReferenceSkeleton& GetRefSkeleton() override;
	virtual const FReferenceSkeleton& GetRefSkeleton() const override;
	virtual FSkeletalMeshLODInfo* GetLODInfo(int32 Index) override;
	virtual const FSkeletalMeshLODInfo* GetLODInfo(int32 Index) const override;
	virtual const TArray<FSkeletalMeshLODInfo>& GetLODInfoArray() const override;
	TArray<FSkeletalMeshLODInfo>& GetLODInfoArray();
	virtual int32 GetLODNum() const override;
	virtual bool IsValidLODIndex(int32 Index) const override;
	virtual const TArray<FSkeletalMaterial>& GetMaterials() const override;
	TArray<FSkeletalMaterial>& GetMaterials();
	void SetMaterials(const TArray<FSkeletalMaterial>& InMaterials);
	virtual USkeleton* GetSkeleton() override;
	virtual const USkeleton* GetSkeleton() const override;
	void SetSkeleton(USkeleton* InSkeleton);
	virtual UPhysicsAsset* GetPhysicsAsset() const override;
	UFUNCTION(BlueprintCallable, Category = Mesh)
	virtual FBoxSphereBounds GetBounds() const override;
	UFUNCTION(BlueprintCallable, Category = Mesh)
	FBoxSphereBounds GetImportedBounds() const;
	virtual FSkeletalMeshRenderData* GetResourceForRendering() const override;
	virtual const TArray<FMatrix44f>& GetRefBasesInvMatrix() const override;
	UFUNCTION(BlueprintCallable, Category = "Animation")
	USkeletalMeshSocket* FindSocketAndIndex(FName InSocketName, int32& OutIndex) const;
	UFUNCTION(BlueprintCallable, Category = "Animation")
	USkeletalMeshSocket* FindSocketInfo(FName InSocketName, FTransform& OutTransform, int32& OutBoneIndex, int32& OutIndex) const;
	UFUNCTION(BlueprintCallable, Category = "Animation")
	USkeletalMeshSocket* GetSocketByIndex(int32 Index) const;
	UFUNCTION(BlueprintCallable, Category = "Animation")
	int32 NumSockets() const;
	TArray<TObjectPtr<USkeletalMeshSocket>>& GetMeshOnlySocketList();
	const TArray<USkeletalMeshSocket*>& GetMeshOnlySocketList() const;
	TArray<USkeletalMeshSocket*> GetActiveSocketList() const;
	void AddSocket(USkeletalMeshSocket* InSocket, bool bAddToSkeleton = false);
	const TArray<TObjectPtr<UMorphTarget>>& GetMorphTargets() const;
	void InitMorphTargets();
	virtual void PostLoad() override;
#if WITH_EDITOR
	void Build();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
