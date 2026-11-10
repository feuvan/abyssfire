// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Components/StaticMeshComponent.h (UE 5.x source knowledge;
// UFUNCTION / UPROPERTY-marked members: checked against the 5.6 stub). GetStaticMesh() returns the TObjectPtr (5.x).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/MeshComponent.h"

class UStaticMesh;

class UStaticMeshComponent : public UMeshComponent
{
	UE_STUB_CLASS_BODY(UStaticMeshComponent, UMeshComponent)
public:
	/** If 0, auto-select LOD level. If >0, force to (ForcedLodModel-1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = LOD)
	int32 ForcedLodModel;
	/** Specifies the smallest LOD that will be used for this component (only when bOverrideMinLOD). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = LOD, meta = (editcondition = "bOverrideMinLOD"))
	int32 MinLOD;
	/** Whether to override the MinLOD setting of the static mesh asset with the MinLOD of this component. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = LOD)
	uint8 bOverrideMinLOD : 1;
	/** Change the StaticMesh used by this instance. */
	UFUNCTION(BlueprintCallable, Category = "Components|StaticMesh")
	virtual bool SetStaticMesh(class UStaticMesh* NewMesh);
	/** Get the StaticMesh used by this instance. */
	TObjectPtr<UStaticMesh> GetStaticMesh() const;
	UFUNCTION(BlueprintCallable, Category = "Rendering|LOD")
	void SetForcedLodModel(int32 NewForcedLodModel);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetDistanceFieldSelfShadowBias(float NewValue);
	UFUNCTION(BlueprintCallable, Category = Rendering)
	void SetEvaluateWorldPositionOffset(bool NewValue);
	UFUNCTION(BlueprintCallable, Category = Rendering)
	void SetWorldPositionOffsetDisableDistance(int32 NewValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetReverseCulling(bool ReverseCulling);
	/** Get Local bounds. */
	UFUNCTION(BlueprintCallable, Category = "Components|StaticMesh")
	void GetLocalBounds(FVector& Min, FVector& Max) const;
	virtual int32 GetNumMaterials() const override;
	virtual UMaterialInterface* GetMaterial(int32 MaterialIndex) const override;
	virtual int32 GetMaterialIndex(FName MaterialSlotName) const override;
	virtual TArray<FName> GetMaterialSlotNames() const override;
	virtual bool IsMaterialSlotNameValid(FName MaterialSlotName) const override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual bool DoesSocketExist(FName InSocketName) const override;
	virtual FTransform GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace = RTS_World) const override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void SetCollisionProfileName(FName InCollisionProfileName, bool bUpdateOverlaps = true) override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
};
