// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Materials/Material.h (UE 5.x source knowledge;
// UPROPERTY-marked members: checked against the 5.6 stub). Only the identity / domain surface of a master material.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialInterface.h"
#include "MaterialDomain.h"

class UMaterial : public UMaterialInterface
{
	UE_STUB_CLASS_BODY(UMaterial, UMaterialInterface)
public:
	/** The domain that the material's attributes will be evaluated in. */
	UPROPERTY(EditAnywhere, Category = Material, AssetRegistrySearchable)
	TEnumAsByte<EMaterialDomain> MaterialDomain;
	/** Determines how the material's color is blended with background colors. */
	UPROPERTY(EditAnywhere, Category = Material, AssetRegistrySearchable)
	TEnumAsByte<EBlendMode> BlendMode;
	/** Indicates that the material should be rendered without backface culling. */
	UPROPERTY(EditAnywhere, Category = Material)
	uint8 TwoSided : 1;
	virtual UMaterial* GetMaterial() override;
	virtual const UMaterial* GetMaterial() const override;
	virtual UPhysicalMaterial* GetPhysicalMaterial() const override;
	virtual FMaterialRenderProxy* GetRenderProxy() const override;
	virtual FMaterialResource* GetMaterialResource(ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type QualityLevel = EMaterialQualityLevel::Num) override;
	virtual const FMaterialResource* GetMaterialResource(ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type QualityLevel = EMaterialQualityLevel::Num) const override;
	virtual void GetUsedTextures(TArray<UTexture*>& OutTextures, EMaterialQualityLevel::Type QualityLevel, bool bAllQualityLevels, ERHIFeatureLevel::Type FeatureLevel, bool bAllFeatureLevels) const override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
