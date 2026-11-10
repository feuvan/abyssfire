// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Components/PrimitiveComponent.h (UE 5.x source knowledge;
// UFUNCTION / UPROPERTY-marked members: checked against the 5.6 stub). The real class also implements
// INavRelevantInterface, IInterface_AsyncCompilation and IPhysicsComponent (not modelled). FBodyInstance (PhysicsCore /
// Engine physics) is not modelled: use the SetCollision* functions.
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Engine/EngineTypes.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class FPrimitiveSceneProxy;
struct FCollisionQueryParams;

class UPrimitiveComponent : public USceneComponent
{
	UE_STUB_CLASS_BODY(UPrimitiveComponent, USceneComponent)
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = LOD)
	float MinDrawDistance;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = LOD)
	float LDMaxDrawDistance;
	UPROPERTY(Category = LOD, AdvancedDisplay, VisibleAnywhere, BlueprintReadOnly)
	float CachedMaxDrawDistance;
	TEnumAsByte<ESceneDepthPriorityGroup> DepthPriorityGroup;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Rendering)
	uint8 bVisibleInReflectionCaptures : 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Rendering)
	uint8 bRenderInMainPass : 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Rendering)
	uint8 bRenderInDepthPass : 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Rendering)
	uint8 bReceivesDecals : 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Rendering)
	uint8 bOwnerNoSee : 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Rendering)
	uint8 bOnlyOwnerSee : 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Rendering)
	uint8 bTreatAsBackgroundForOcclusion : 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Rendering)
	uint8 bUseAsOccluder : 1;
	uint8 bSelectable : 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Lighting)
	uint8 CastShadow : 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Lighting)
	uint8 bAffectDynamicIndirectLighting : 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Lighting)
	uint8 bAffectDistanceFieldLighting : 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Lighting)
	uint8 bCastDynamicShadow : 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Lighting)
	uint8 bCastStaticShadow : 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Lighting)
	uint8 bCastContactShadow : 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Lighting)
	uint8 bCastHiddenShadow : 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Rendering)
	uint8 bRenderCustomDepth : 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Mobile)
	uint8 bReceiveMobileCSMShadows : 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Rendering)
	int32 CustomDepthStencilValue;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Rendering)
	int32 TranslucencySortPriority;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Rendering)
	float TranslucencySortDistanceOffset;
	float BoundsScale;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Lighting)
	FLightingChannels LightingChannels;
	FPrimitiveSceneProxy* SceneProxy;

	bool GetGenerateOverlapEvents() const;
	UFUNCTION(BlueprintSetter)
	void SetGenerateOverlapEvents(bool bInGenerateOverlapEvents);

	UFUNCTION(BlueprintCallable, Category = "Collision")
	virtual void SetCollisionEnabled(ECollisionEnabled::Type NewType);
	UFUNCTION(BlueprintCallable, Category = "Collision")
	virtual void SetCollisionProfileName(FName InCollisionProfileName, bool bUpdateOverlaps = true);
	UFUNCTION(BlueprintPure, Category = "Collision")
	FName GetCollisionProfileName() const;
	UFUNCTION(BlueprintCallable, Category = "Collision")
	virtual void SetCollisionObjectType(ECollisionChannel Channel);
	UFUNCTION(BlueprintCallable, Category = "Collision")
	virtual void SetCollisionResponseToChannel(ECollisionChannel Channel, ECollisionResponse NewResponse);
	UFUNCTION(BlueprintCallable, Category = "Collision")
	virtual void SetCollisionResponseToAllChannels(ECollisionResponse NewResponse);
	virtual void SetCollisionResponseToChannels(const FCollisionResponseContainer& NewReponses);
	virtual ECollisionEnabled::Type GetCollisionEnabled() const override;
	virtual ECollisionResponse GetCollisionResponseToChannel(ECollisionChannel Channel) const override;
	virtual ECollisionChannel GetCollisionObjectType() const override;
	virtual const FCollisionResponseContainer& GetCollisionResponseToChannels() const override;
	virtual bool LineTraceComponent(FHitResult& OutHit, const FVector Start, const FVector End, const FCollisionQueryParams& Params);

	UFUNCTION(BlueprintPure, Category = "Rendering|Material")
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const PURE_VIRTUAL(UPrimitiveComponent::GetMaterial, return nullptr;);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* Material) PURE_VIRTUAL(UPrimitiveComponent::SetMaterial, );
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	virtual void SetMaterialByName(FName MaterialSlotName, UMaterialInterface* Material) PURE_VIRTUAL(UPrimitiveComponent::SetMaterialByName, );
	UFUNCTION(BlueprintPure, Category = "Rendering|Material")
	virtual int32 GetNumMaterials() const PURE_VIRTUAL(UPrimitiveComponent::GetNumMaterials, return 0;);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	virtual UMaterialInstanceDynamic* CreateAndSetMaterialInstanceDynamic(int32 ElementIndex);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	virtual UMaterialInstanceDynamic* CreateAndSetMaterialInstanceDynamicFromMaterial(int32 ElementIndex, UMaterialInterface* Parent);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	virtual UMaterialInstanceDynamic* CreateDynamicMaterialInstance(int32 ElementIndex, UMaterialInterface* SourceMaterial = nullptr, FName OptionalName = NAME_None);
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const;

	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetCustomPrimitiveDataFloat(int32 DataIndex, float Value);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetCustomPrimitiveDataVector2(int32 DataIndex, FVector2D Value);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetCustomPrimitiveDataVector3(int32 DataIndex, FVector Value);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetCustomPrimitiveDataVector4(int32 DataIndex, FVector4 Value);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetDefaultCustomPrimitiveDataFloat(int32 DataIndex, float Value);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetDefaultCustomPrimitiveDataVector4(int32 DataIndex, FVector4 Value);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetScalarParameterForCustomPrimitiveData(FName ParameterName, float Value);
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetVectorParameterForCustomPrimitiveData(FName ParameterName, FVector4 Value);

	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetCastShadow(bool NewCastShadow);
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetCastHiddenShadow(bool NewCastHiddenShadow);
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetCastContactShadow(bool bInCastContactShadow);
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetRenderCustomDepth(bool bValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetCustomDepthStencilValue(int32 Value);
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetRenderInMainPass(bool bValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetRenderInDepthPass(bool bValue);
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetReceivesDecals(bool bNewReceivesDecals);
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetTranslucentSortPriority(int32 NewTranslucentSortPriority);
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetTranslucencySortDistanceOffset(float NewTranslucencySortDistanceOffset);
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetBoundsScale(float NewBoundsScale = 1.f);
	UFUNCTION(BlueprintCallable, Category = "LOD")
	void SetCullDistance(float NewCullDistance);
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetOwnerNoSee(bool bNewOwnerNoSee);
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetOnlyOwnerSee(bool bNewOnlyOwnerSee);
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetVisibleInRayTracing(bool bNewVisibleInRayTracing);
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetLightingChannels(bool bChannel0, bool bChannel1, bool bChannel2);
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	bool WasRecentlyRendered(float Tolerance = 0.2f) const;
	float GetLastRenderTime() const;
	float GetLastRenderTimeOnScreen() const;
};
