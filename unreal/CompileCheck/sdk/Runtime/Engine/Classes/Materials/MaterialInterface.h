// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Materials/MaterialInterface.h (UE 5.x source knowledge;
// UFUNCTION-marked members: checked against the 5.6 stub). Abstract in the engine (UCLASS(abstract)).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "MaterialTypes.h"
#include "SceneTypes.h"
#include "RHIDefinitions.h"

class UMaterial;
class UTexture;
class UPhysicalMaterial;
class FMaterialResource;
class FMaterialRenderProxy;

class UMaterialInterface : public UObject
{
	UE_STUB_CLASS_BODY(UMaterialInterface, UObject)
public:
	virtual UMaterial* GetMaterial() PURE_VIRTUAL(UMaterialInterface::GetMaterial, return nullptr;);
	virtual const UMaterial* GetMaterial() const PURE_VIRTUAL(UMaterialInterface::GetMaterial, return nullptr;);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	UMaterial* GetBaseMaterial();
	UFUNCTION(BlueprintCallable, Category = "Physics|Material")
	virtual UPhysicalMaterial* GetPhysicalMaterial() const PURE_VIRTUAL(UMaterialInterface::GetPhysicalMaterial, return nullptr;);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	virtual EBlendMode GetBlendMode() const;
	virtual bool IsTwoSided(bool bIsGameThread = IsInGameThread()) const;
	virtual bool IsMasked(bool bIsGameThread = IsInGameThread()) const;
	virtual FMaterialRenderProxy* GetRenderProxy() const PURE_VIRTUAL(UMaterialInterface::GetRenderProxy, return nullptr;);

	// The feature-level overloads are the long-standing API (UE 4.2x .. 5.x).
	virtual FMaterialResource* GetMaterialResource(ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type QualityLevel = EMaterialQualityLevel::Num);
	virtual const FMaterialResource* GetMaterialResource(ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type QualityLevel = EMaterialQualityLevel::Num) const;
#if defined(UE_STUB_UNVERIFIED_MATERIAL_RESOURCE_BY_SHADER_PLATFORM) && UE_STUB_UNVERIFIED_MATERIAL_RESOURCE_BY_SHADER_PLATFORM
	// UNVERIFIED (API_UNCERTAIN.md #1): newer engines are believed to take the shader platform here. Off by default so
	// the module's fallback branch is the one that is type-checked; define the macro to check the other branch.
	virtual FMaterialResource* GetMaterialResource(EShaderPlatform InShaderPlatform, EMaterialQualityLevel::Type QualityLevel = EMaterialQualityLevel::Num);
	virtual const FMaterialResource* GetMaterialResource(EShaderPlatform InShaderPlatform, EMaterialQualityLevel::Type QualityLevel = EMaterialQualityLevel::Num) const;
#endif

	void GetAllParameterInfoOfType(EMaterialParameterType Type, TArray<FMaterialParameterInfo>& OutParameterInfo, TArray<FGuid>& OutParameterIds) const;
	void GetAllScalarParameterInfo(TArray<FMaterialParameterInfo>& OutParameterInfo, TArray<FGuid>& OutParameterIds) const;
	void GetAllVectorParameterInfo(TArray<FMaterialParameterInfo>& OutParameterInfo, TArray<FGuid>& OutParameterIds) const;
	void GetAllTextureParameterInfo(TArray<FMaterialParameterInfo>& OutParameterInfo, TArray<FGuid>& OutParameterIds) const;
	virtual bool GetScalarParameterValue(const FHashedMaterialParameterInfo& ParameterInfo, float& OutValue, bool bOveriddenOnly = false) const;
	virtual bool GetVectorParameterValue(const FHashedMaterialParameterInfo& ParameterInfo, FLinearColor& OutValue, bool bOveriddenOnly = false) const;
	virtual bool GetTextureParameterValue(const FHashedMaterialParameterInfo& ParameterInfo, UTexture*& OutValue, bool bOveriddenOnly = false) const;
	virtual void GetUsedTextures(TArray<UTexture*>& OutTextures, EMaterialQualityLevel::Type QualityLevel, bool bAllQualityLevels, ERHIFeatureLevel::Type FeatureLevel, bool bAllFeatureLevels) const PURE_VIRTUAL(UMaterialInterface::GetUsedTextures, );
};
