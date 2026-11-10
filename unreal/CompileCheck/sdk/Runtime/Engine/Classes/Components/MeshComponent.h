// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Components/MeshComponent.h (UE 5.x source knowledge;
// UFUNCTION-marked members: checked against the 5.6 stub). Abstract in the engine (UCLASS(abstract)).
#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"

class UMaterialInterface;

class UMeshComponent : public UPrimitiveComponent
{
	UE_STUB_CLASS_BODY(UMeshComponent, UPrimitiveComponent)
public:
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Rendering)
	TArray<TObjectPtr<UMaterialInterface>> OverrideMaterials;
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	virtual TArray<UMaterialInterface*> GetMaterials() const;
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	UMaterialInterface* GetOverlayMaterial() const;
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetOverlayMaterial(UMaterialInterface* NewOverlayMaterial);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetOverlayMaterialMaxDrawDistance(float InMaxDrawDistance);
	virtual int32 GetMaterialIndex(FName MaterialSlotName) const;
	virtual TArray<FName> GetMaterialSlotNames() const;
	virtual bool IsMaterialSlotNameValid(FName MaterialSlotName) const;
	virtual int32 GetNumOverrideMaterials() const;
	void EmptyOverrideMaterials();
	bool HasOverrideMaterials();
	virtual int32 GetNumMaterials() const override;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* Material) override;
	virtual void SetMaterialByName(FName MaterialSlotName, UMaterialInterface* Material) override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	virtual void PrestreamTextures(float Seconds, bool bPrioritizeCharacterTextures, int32 CinematicTextureGroups = 0);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetScalarParameterValueOnMaterials(const FName ParameterName, const float ParameterValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetVectorParameterValueOnMaterials(const FName ParameterName, const FVector ParameterValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetColorParameterValueOnMaterials(const FName ParameterName, const FLinearColor ParameterValue);
};
