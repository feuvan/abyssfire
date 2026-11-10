// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/StaticMesh.h (UE 5.x source knowledge;
// UFUNCTION / UPROPERTY-marked members: checked against the 5.6 stub). The real class also implements
// IInterface_CollisionDataProvider, IInterface_AssetUserData and IInterface_AsyncCompilation (not modelled).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/StreamableRenderAsset.h"
#include "Engine/EngineTypes.h"

class UMaterialInterface;
class UStaticMeshSocket;

/** A material slot of a static mesh. */
struct FStaticMaterial
{
	UE_STUB_STRUCT_BODY(FStaticMaterial)
	FStaticMaterial();
	FStaticMaterial(class UMaterialInterface* InMaterialInterface, FName InMaterialSlotName = NAME_None, FName InImportedMaterialSlotName = NAME_None);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = StaticMesh)
	TObjectPtr<class UMaterialInterface> MaterialInterface;
	/** This name should be use by the gameplay to avoid error if the skeletal mesh Materials array topology change. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = StaticMesh)
	FName MaterialSlotName;
	/** This name should be use when we re-import a skeletal mesh so we can order the Materials array like it should be. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = StaticMesh)
	FName ImportedMaterialSlotName;
};

class UStaticMesh : public UStreamableRenderAsset
{
	UE_STUB_CLASS_BODY(UStaticMesh, UStreamableRenderAsset)
public:
	/** Returns the bounds of the mesh. */
	UFUNCTION(BlueprintPure, Category = "StaticMesh")
	FBoxSphereBounds GetBounds() const;
	/** Returns the bounding box, in local space including bounds extension(s), of the StaticMesh asset. */
	UFUNCTION(BlueprintPure, Category = "StaticMesh")
	FBox GetBoundingBox() const;
	/** Returns number of LODs available in the mesh. */
	UFUNCTION(BlueprintPure, Category = "StaticMesh", meta = (ScriptName = "GetNumLods"))
	int32 GetNumLODs() const;
	/** Returns number of Sections that this StaticMesh has, in the supplied LOD (LOD 0 is the highest). */
	UFUNCTION(BlueprintPure, Category = "StaticMesh")
	int32 GetNumSections(int32 InLOD) const;
	/** Gets a Material given a Material Index. */
	UFUNCTION(BlueprintPure, Category = "StaticMesh")
	UMaterialInterface* GetMaterial(int32 MaterialIndex) const;
	/** Gets a Material index given a slot name. */
	UFUNCTION(BlueprintPure, Category = "StaticMesh")
	int32 GetMaterialIndex(FName MaterialSlotName) const;
	/** Find a socket object in this StaticMesh by name. */
	UFUNCTION(BlueprintPure, Category = "StaticMesh")
	UStaticMeshSocket* FindSocket(FName InSocketName) const;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
