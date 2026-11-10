// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Components/InstancedStaticMeshComponent.h (UE 5.x source
// knowledge; UFUNCTION / UPROPERTY-marked members: checked against the 5.6 stub). The real class also implements
// ISMInstanceManager (not modelled). SetCustomData (the whole float row of one instance) is not reflected.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/StaticMeshComponent.h"

struct FInstancedStaticMeshInstanceData
{
	UE_STUB_STRUCT_BODY(FInstancedStaticMeshInstanceData)
	UPROPERTY(EditAnywhere, Category = Instances)
	FMatrix Transform;
	FInstancedStaticMeshInstanceData();
};

class UInstancedStaticMeshComponent : public UStaticMeshComponent
{
	UE_STUB_CLASS_BODY(UInstancedStaticMeshComponent, UStaticMeshComponent)
public:
	/** Array of instances, bulk serialized. */
	UPROPERTY(EditAnywhere, SkipSerialization, DisplayName = "Instances", Category = Instances, meta = (MakeEditWidget = true, EditFixedOrder))
	TArray<FInstancedStaticMeshInstanceData> PerInstanceSMData;
	/** Defines the number of floats that will be available per instance for custom data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Instances, AdvancedDisplay)
	int32 NumCustomDataFloats;
	/** Array of custom data for instances: NumCustomDataFloats floats per instance. */
	UPROPERTY(EditAnywhere, EditFixedSize, SkipSerialization, DisplayName = "Custom data", Category = Instances, AdvancedDisplay, meta = (EditFixedOrder))
	TArray<float> PerInstanceSMCustomData;
	/** Distance from camera at which each instance begins to fade out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Culling)
	int32 InstanceStartCullDistance;
	/** Distance from camera at which each instance completely fades out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Culling)
	int32 InstanceEndCullDistance;

	/** Add an instance to this component. Transform is given in local space of this component unless bWorldSpace is set. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	virtual int32 AddInstance(const FTransform& InstanceTransform, bool bWorldSpace = false);
	/** Add multiple instances to this component. Transform is given in local space of this component unless bWorldSpace is set. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	virtual TArray<int32> AddInstances(const TArray<FTransform>& InstanceTransforms, bool bShouldReturnIndices, bool bWorldSpace = false, bool bUpdateNavigation = true);
	/** Update custom data for specific instance. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	virtual bool SetCustomDataValue(int32 InstanceIndex, int32 CustomDataIndex, float CustomDataValue, bool bMarkRenderStateDirty = false);
	/** Per Instance Custom Data: all the custom floats of one instance. */
	virtual bool SetCustomData(int32 InstanceIndex, TArrayView<const float> CustomDataFloats, bool bMarkRenderStateDirty = false);
	/** Update number of custom data entries per instance. This applies to all instances and will reallocate the full custom data buffer and reset all values to 0. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	virtual void SetNumCustomDataFloats(int32 InNumCustomDataFloats);
	/** Get the transform for the instance specified. Instance is returned in local space of this component unless bWorldSpace is set. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	bool GetInstanceTransform(int32 InstanceIndex, FTransform& OutInstanceTransform, bool bWorldSpace = false) const;
	/** Update the transform for the instance specified. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	virtual bool UpdateInstanceTransform(int32 InstanceIndex, const FTransform& NewInstanceTransform, bool bWorldSpace = false, bool bMarkRenderStateDirty = false, bool bTeleport = false);
	/** Update the transform for an array of instances. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	virtual bool BatchUpdateInstancesTransforms(int32 StartInstanceIndex, const TArray<FTransform>& NewInstancesTransforms, bool bWorldSpace = false, bool bMarkRenderStateDirty = false, bool bTeleport = false);
	/** Update the transform for a number of instances. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	virtual bool BatchUpdateInstancesTransform(int32 StartInstanceIndex, int32 NumInstances, const FTransform& NewInstancesTransform, bool bWorldSpace = false, bool bMarkRenderStateDirty = false, bool bTeleport = false);
	/** Remove the instance specified. Returns True on success. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	virtual bool RemoveInstance(int32 InstanceIndex);
	/** Remove the instances specified. Returns True on success. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	virtual bool RemoveInstances(const TArray<int32>& InstancesToRemove);
	/** Clear all instances being rendered by this component. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	virtual void ClearInstances();
	/** Get the number of instances in this component. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	int32 GetInstanceCount() const;
	/** Does the given index map to a valid instance in this component? */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	bool IsValidInstance(int32 InstanceIndex) const;
	/** Sets the fading start and culling end distances for this component. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	void SetCullDistances(int32 StartCullDistance, int32 EndCullDistance);
	virtual bool SetStaticMesh(UStaticMesh* NewMesh) override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& BoundTransform) const override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
};
