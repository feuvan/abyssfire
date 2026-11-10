// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Components/SceneComponent.h (UE 5.x source knowledge;
// UFUNCTION-marked members: checked against the 5.6 stub). The K2_ functions are the Blueprint entry points; C++ uses
// the unprefixed ones (not reflected).
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"

UENUM()
enum ERelativeTransformSpace : int
{
	RTS_World,
	RTS_Actor,
	RTS_Component,
	RTS_ParentBoneSpace,
};

class USceneComponent : public UActorComponent
{
	UE_STUB_CLASS_BODY(USceneComponent, UActorComponent)
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Mobility)
	TEnumAsByte<EComponentMobility::Type> Mobility;
	uint8 bHiddenInGame : 1;
	uint8 bVisualizeComponent : 1;
	uint8 bUseAttachParentBound : 1;
	static const FName GetDefaultSceneRootVariableName();
	FVector GetRelativeLocation() const;
	FRotator GetRelativeRotation() const;
	FVector GetRelativeScale3D() const;
	void SetRelativeLocation_Direct(const FVector NewRelativeLocation);
	void SetRelativeRotation_Direct(const FRotator NewRelativeRotation);
	void SetRelativeScale3D_Direct(const FVector NewRelativeScale3D);
	void SetRelativeLocation(FVector NewLocation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetRelativeRotation(FRotator NewRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetRelativeRotation(const FQuat& NewRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetRelativeTransform(const FTransform& NewTransform, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetRelativeLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetRelativeLocationAndRotation(FVector NewLocation, const FQuat& NewRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	UFUNCTION(BlueprintCallable, Category = "Transformation")
	void SetRelativeScale3D(FVector NewScale3D);
	UFUNCTION(BlueprintCallable, Category = "Transformation")
	void ResetRelativeTransform();
	FTransform GetRelativeTransform() const;
	void AddRelativeLocation(FVector DeltaLocation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void AddRelativeRotation(FRotator DeltaRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void AddLocalOffset(FVector DeltaLocation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void AddLocalRotation(FRotator DeltaRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void AddWorldOffset(FVector DeltaLocation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void AddWorldRotation(FRotator DeltaRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetWorldLocation(FVector NewLocation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetWorldRotation(FRotator NewRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetWorldRotation(const FQuat& NewRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	UFUNCTION(BlueprintCallable, Category = "Transformation")
	void SetWorldScale3D(FVector NewScale);
	void SetWorldTransform(const FTransform& NewTransform, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetWorldLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetWorldLocationAndRotation(FVector NewLocation, const FQuat& NewRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	UFUNCTION(BlueprintCallable, Category = "Transformation", meta = (DisplayName = "Get World Location", ScriptName = "GetWorldLocation", Keywords = "position"))
	FVector K2_GetComponentLocation() const;
	UFUNCTION(BlueprintCallable, Category = "Transformation", meta = (DisplayName = "Get World Rotation", ScriptName = "GetWorldRotation"))
	FRotator K2_GetComponentRotation() const;
	UFUNCTION(BlueprintCallable, Category = "Transformation", meta = (DisplayName = "Get World Scale", ScriptName = "GetWorldScale"))
	FVector K2_GetComponentScale() const;
	UFUNCTION(BlueprintCallable, Category = "Transformation", meta = (DisplayName = "Get World Transform", ScriptName = "GetWorldTransform"))
	FTransform K2_GetComponentToWorld() const;
	FVector GetComponentLocation() const;
	FRotator GetComponentRotation() const;
	FQuat GetComponentQuat() const;
	FVector GetComponentScale() const;
	const FTransform& GetComponentTransform() const;
	const FTransform& GetComponentToWorld() const;
	UFUNCTION(BlueprintCallable, Category = "Transformation")
	FVector GetForwardVector() const;
	UFUNCTION(BlueprintCallable, Category = "Transformation")
	FVector GetUpVector() const;
	UFUNCTION(BlueprintCallable, Category = "Transformation")
	FVector GetRightVector() const;
	UFUNCTION(BlueprintCallable, Category = "Transformation")
	virtual FVector GetComponentVelocity() const;
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	virtual bool IsVisible() const;
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetVisibility(bool bNewVisibility, bool bPropagateToChildren = false);
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void ToggleVisibility(bool bPropagateToChildren = false);
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetHiddenInGame(bool NewHidden, bool bPropagateToChildren = false);
	bool GetVisibleFlag() const;
	bool ShouldRender() const;
	UFUNCTION(BlueprintCallable, Category = "Components")
	USceneComponent* GetAttachParent() const;
	UFUNCTION(BlueprintCallable, Category = "Components")
	FName GetAttachSocketName() const;
	UFUNCTION(BlueprintCallable, Category = "Components")
	void GetChildrenComponents(bool bIncludeAllDescendants, TArray<USceneComponent*>& Children) const;
	UFUNCTION(BlueprintCallable, Category = "Components")
	USceneComponent* GetChildComponent(int32 ChildIndex) const;
	UFUNCTION(BlueprintCallable, Category = "Components")
	int32 GetNumChildrenComponents() const;
	const TArray<TObjectPtr<USceneComponent>>& GetAttachChildren() const;
	USceneComponent* GetAttachmentRoot() const;
	AActor* GetAttachmentRootActor() const;
	UFUNCTION(BlueprintCallable, Category = "Components")
	bool IsAnySimulatingPhysics() const;
	void SetupAttachment(USceneComponent* InParent, FName InSocketName = NAME_None);
	bool AttachToComponent(USceneComponent* InParent, const FAttachmentTransformRules& AttachmentRules, FName InSocketName = NAME_None);
	virtual void DetachFromComponent(const FDetachmentTransformRules& DetachmentRules);
	UFUNCTION(BlueprintCallable, Category = "Components")
	virtual bool DoesSocketExist(FName InSocketName) const;
	UFUNCTION(BlueprintCallable, Category = "Components")
	TArray<FName> GetAllSocketNames() const;
	virtual FTransform GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace = RTS_World) const;
	UFUNCTION(BlueprintCallable, Category = "Components")
	virtual FVector GetSocketLocation(FName InSocketName) const;
	UFUNCTION(BlueprintCallable, Category = "Components")
	virtual FRotator GetSocketRotation(FName InSocketName) const;
	UFUNCTION(BlueprintCallable, Category = "Components")
	virtual FQuat GetSocketQuaternion(FName InSocketName) const;
	virtual void SetMobility(EComponentMobility::Type NewMobility);
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const;
	virtual FBoxSphereBounds CalcLocalBounds() const;
	void UpdateBounds();
	FBoxSphereBounds Bounds;
	virtual ECollisionEnabled::Type GetCollisionEnabled() const;
	virtual const FCollisionResponseContainer& GetCollisionResponseToChannels() const;
	virtual ECollisionResponse GetCollisionResponseToChannel(ECollisionChannel Channel) const;
	virtual ECollisionChannel GetCollisionObjectType() const;
	bool IsCollisionEnabled() const;
	bool IsQueryCollisionEnabled() const;
	bool IsPhysicsCollisionEnabled() const;
	const FBoxSphereBounds& GetLocalBounds() const;
	void UpdateComponentToWorld(EUpdateTransformFlags UpdateTransformFlags = EUpdateTransformFlags::None, ETeleportType Teleport = ETeleportType::None);
	void SetAbsolute(bool bNewAbsoluteLocation = false, bool bNewAbsoluteRotation = false, bool bNewAbsoluteScale = false);
	void SetUsingAbsoluteLocation(bool bInAbsoluteLocation);
	void SetUsingAbsoluteRotation(bool bInAbsoluteRotation);
	void SetUsingAbsoluteScale(bool bInAbsoluteRotation);
	bool IsUsingAbsoluteLocation() const;
	bool IsUsingAbsoluteRotation() const;
	bool IsUsingAbsoluteScale() const;
protected:
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None);
	virtual void OnVisibilityChanged();
	virtual void OnHiddenInGameChanged();
	virtual void OnAttachmentChanged();
};
