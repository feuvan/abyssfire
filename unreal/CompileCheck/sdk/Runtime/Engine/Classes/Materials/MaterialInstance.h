// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Materials/MaterialInstance.h (UE 5.x source knowledge;
// UPROPERTY-marked members: checked against the 5.6 stub). Abstract in the engine (UCLASS(abstract)).
#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInterface.h"

class UMaterialInstance : public UMaterialInterface
{
	UE_STUB_CLASS_BODY(UMaterialInstance, UMaterialInterface)
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = MaterialInstance, AssetRegistrySearchable)
	TObjectPtr<UMaterialInterface> Parent;
	virtual UMaterial* GetMaterial() override;
	virtual const UMaterial* GetMaterial() const override;
	virtual UPhysicalMaterial* GetPhysicalMaterial() const override;
	virtual FMaterialRenderProxy* GetRenderProxy() const override;
	virtual FMaterialResource* GetMaterialResource(ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type QualityLevel = EMaterialQualityLevel::Num) override;
	virtual const FMaterialResource* GetMaterialResource(ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type QualityLevel = EMaterialQualityLevel::Num) const override;
	virtual bool GetScalarParameterValue(const FHashedMaterialParameterInfo& ParameterInfo, float& OutValue, bool bOveriddenOnly = false) const override;
	virtual bool GetVectorParameterValue(const FHashedMaterialParameterInfo& ParameterInfo, FLinearColor& OutValue, bool bOveriddenOnly = false) const override;
	virtual bool GetTextureParameterValue(const FHashedMaterialParameterInfo& ParameterInfo, UTexture*& OutValue, bool bOveriddenOnly = false) const override;
	virtual void GetUsedTextures(TArray<UTexture*>& OutTextures, EMaterialQualityLevel::Type QualityLevel, bool bAllQualityLevels, ERHIFeatureLevel::Type FeatureLevel, bool bAllFeatureLevels) const override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
