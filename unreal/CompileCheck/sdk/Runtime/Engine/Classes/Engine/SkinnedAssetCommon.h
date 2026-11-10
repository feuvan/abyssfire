// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/SkinnedAssetCommon.h (UE 5.1+; UE 5.x source
// knowledge; reflected structs: checked against the 5.6 stub). FSkeletalMeshLODInfo, FSkeletalMaterial.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "PerPlatformProperties.h"

class UMaterialInterface;

struct FSkeletalMeshLODInfo
{
	UE_STUB_STRUCT_BODY(FSkeletalMeshLODInfo)
	UPROPERTY(EditAnywhere, Category = SkeletalMeshLODInfo)
	FPerPlatformFloat ScreenSize;
	UPROPERTY(EditAnywhere, Category = SkeletalMeshLODInfo, meta = (DisplayName = "LOD Hysteresis"))
	float LODHysteresis;
	UPROPERTY()
	TArray<int32> LODMaterialMap;
	UPROPERTY(EditAnywhere, Category = SkeletalMeshLODInfo)
	float MorphTargetPositionErrorTolerance;
	UPROPERTY()
	uint8 bHasBeenSimplified : 1;
	UPROPERTY(EditAnywhere, Category = SkeletalMeshLODInfo)
	uint8 bAllowCPUAccess : 1;
	FSkeletalMeshLODInfo();
};
struct FSkeletalMaterial
{
	UE_STUB_STRUCT_BODY(FSkeletalMaterial)
	FSkeletalMaterial();
	FSkeletalMaterial(class UMaterialInterface* InMaterialInterface, FName InMaterialSlotName, FName InImportedMaterialSlotName = NAME_None);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SkeletalMesh)
	TObjectPtr<UMaterialInterface> MaterialInterface;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SkeletalMesh)
	FName MaterialSlotName;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = SkeletalMesh)
	FName ImportedMaterialSlotName;
};
