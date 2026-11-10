// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/MaterialTypes.h (UE 5.x source knowledge; FMaterialParameterInfo
// and EMaterialParameterAssociation are reflected: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

UENUM()
enum EMaterialParameterAssociation : int
{
	LayerParameter,
	BlendParameter,
	GlobalParameter,
};

enum class EMaterialParameterType : uint8
{
	Scalar = 0u,
	Vector,
	DoubleVector,
	Texture,
	TextureCollection,
	Font,
	RuntimeVirtualTexture,
	SparseVolumeTexture,
	StaticSwitch,

	NumRuntime, // Runtime parameter types must go above here, and editor-only ones below

	// TODO - Would be nice to make static parameter values editor-only, but will save that for a future-refactor
	StaticComponentMask = NumRuntime,

	Num,
	None = 0xff,
};

struct FMaterialParameterInfo
{
	UE_STUB_STRUCT_BODY(FMaterialParameterInfo)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ParameterInfo)
	FName Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ParameterInfo)
	TEnumAsByte<EMaterialParameterAssociation> Association;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ParameterInfo)
	int32 Index;

	FMaterialParameterInfo(const TCHAR* InName, EMaterialParameterAssociation InAssociation = EMaterialParameterAssociation::GlobalParameter, int32 InIndex = INDEX_NONE);
	FMaterialParameterInfo(FName InName = FName(), EMaterialParameterAssociation InAssociation = EMaterialParameterAssociation::GlobalParameter, int32 InIndex = INDEX_NONE);
	FString ToString() const;
};
bool operator==(const FMaterialParameterInfo& Lhs, const FMaterialParameterInfo& Rhs);
bool operator!=(const FMaterialParameterInfo& Lhs, const FMaterialParameterInfo& Rhs);
uint32 GetTypeHash(const FMaterialParameterInfo& Value);

struct FHashedMaterialParameterInfo
{
	FHashedMaterialParameterInfo(const TCHAR* InName, EMaterialParameterAssociation InAssociation = EMaterialParameterAssociation::GlobalParameter, int32 InIndex = INDEX_NONE);
	FHashedMaterialParameterInfo(const FName& InName, EMaterialParameterAssociation InAssociation = EMaterialParameterAssociation::GlobalParameter, int32 InIndex = INDEX_NONE);
	FHashedMaterialParameterInfo(const FMaterialParameterInfo& Rhs);
	FName GetName() const;
};

struct FMemoryImageMaterialParameterInfo
{
	FMemoryImageMaterialParameterInfo(const FName& InName, EMaterialParameterAssociation InAssociation = EMaterialParameterAssociation::GlobalParameter, int32 InIndex = INDEX_NONE);
	FMemoryImageMaterialParameterInfo(const FHashedMaterialParameterInfo& Rhs);
	FMemoryImageMaterialParameterInfo(const FMaterialParameterInfo& Rhs);
	FName GetName() const;
};
