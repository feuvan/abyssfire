// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/GameFramework/Actor.h (UE 5.x source knowledge;
// UFUNCTION-marked members: checked against the 5.6 stub). The unprefixed transform setters / getters are the C++ API
// (not reflected); their K2_ twins are the Blueprint entry points.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Templates/Casts.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/EngineTypes.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/HitResult.h"

class UWorld;
class ULevel;
class UGameInstance;
class APawn;
class AController;
class UInputComponent;
class APlayerController;
class UPrimitiveComponent;

namespace UE::Stub
{
	/** TPointedToType of the engine, for what AActor::GetComponents accepts: T* and TObjectPtr<T>. No Type otherwise. */
	template <typename PointerType> struct TComponentPointee {};
	template <typename T> struct TComponentPointee<T*> { using Type = T; };
	template <typename T> struct TComponentPointee<TObjectPtr<T>> { using Type = T; };
}

/** TArray with inline storage for a typical component count; the constructor from an actor fills it (GetComponents). */
template <class T, uint32 NumElements = 24>
class TInlineComponentArray : public TArray<T, TInlineAllocator<NumElements>>
{
	using Super = TArray<T, TInlineAllocator<NumElements>>;
public:
	TInlineComponentArray() : Super() {}
	TInlineComponentArray(const class AActor* Actor, bool bIncludeFromChildActors = false);
};

class AActor : public UObject
{
	UE_STUB_CLASS_BODY(AActor, UObject)
public:
	UPROPERTY(EditDefaultsOnly, Category = Tick)
	struct FActorTickFunction PrimaryActorTick;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = Actor)
	TArray<FName> Tags;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Actor)
	float InitialLifeSpan;
	UPROPERTY(BlueprintReadWrite, AdvancedDisplay, Category = Actor)
	float CustomTimeDilation;
	UPROPERTY(DuplicateTransient)
	TObjectPtr<UInputComponent> InputComponent;
	uint8 bAllowTickBeforeBeginPlay : 1;
	uint8 bFindCameraComponentWhenViewTarget : 1;
	uint8 bIsEditorOnlyActor : 1;
protected:
	UPROPERTY(BlueprintGetter = K2_GetRootComponent, Category = "Transformation")
	TObjectPtr<USceneComponent> RootComponent;
public:
	USceneComponent* GetRootComponent() const { return RootComponent; }
	bool SetRootComponent(USceneComponent* NewRootComponent);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get Actor Location", ScriptName = "GetActorLocation", Keywords = "position"), Category = "Transformation")
	FVector K2_GetActorLocation() const;
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get Actor Rotation", ScriptName = "GetActorRotation"), Category = "Transformation")
	FRotator K2_GetActorRotation() const;
	FVector GetActorLocation() const;
	FRotator GetActorRotation() const;
	FQuat GetActorQuat() const;
	UFUNCTION(BlueprintCallable, Category = "Transformation")
	FVector GetActorScale3D() const;
	const FTransform& GetActorTransform() const;
	const FTransform& ActorToWorld() const;
	UFUNCTION(BlueprintCallable, Category = "Transformation")
	FVector GetActorForwardVector() const;
	UFUNCTION(BlueprintCallable, Category = "Transformation")
	FVector GetActorUpVector() const;
	UFUNCTION(BlueprintCallable, Category = "Transformation")
	FVector GetActorRightVector() const;
	bool SetActorLocation(const FVector& NewLocation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	bool SetActorRotation(FRotator NewRotation, ETeleportType Teleport = ETeleportType::None);
	bool SetActorRotation(const FQuat& NewRotation, ETeleportType Teleport = ETeleportType::None);
	bool SetActorLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	bool SetActorLocationAndRotation(FVector NewLocation, const FQuat& NewRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	UFUNCTION(BlueprintCallable, Category = "Transformation")
	void SetActorScale3D(FVector NewScale3D);
	bool SetActorTransform(const FTransform& NewTransform, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void AddActorWorldOffset(FVector DeltaLocation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void AddActorWorldRotation(FRotator DeltaRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void AddActorLocalOffset(FVector DeltaLocation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void AddActorLocalRotation(FRotator DeltaRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetActorRelativeLocation(FVector NewRelativeLocation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetActorRelativeRotation(FRotator NewRelativeRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	UFUNCTION(BlueprintCallable, Category = "Transformation")
	void SetActorRelativeScale3D(FVector NewRelativeScale);
	UFUNCTION(BlueprintCallable, Category = "Rendering", meta = (DisplayName = "Set Actor Hidden In Game", Keywords = "Visible Hidden Show Hide"))
	virtual void SetActorHiddenInGame(bool bNewHidden);
	UFUNCTION(BlueprintCallable, Category = "Collision")
	void SetActorEnableCollision(bool bNewActorEnableCollision);
	UFUNCTION(BlueprintPure, Category = "Collision")
	bool GetActorEnableCollision() const;
	bool IsHidden() const;
	void SetHidden(const bool bInHidden);
	UFUNCTION(BlueprintCallable, Category = "Actor", meta = (Keywords = "Delete", DisplayName = "Destroy Actor", ScriptName = "DestroyActor"))
	virtual void K2_DestroyActor();
	bool Destroy(bool bNetForce = false, bool bShouldModifyLevel = true);
	UFUNCTION(BlueprintCallable, Category = "Actor")
	virtual void SetLifeSpan(float InLifespan);
	UFUNCTION(BlueprintCallable, Category = "Actor")
	virtual float GetLifeSpan() const;
	UFUNCTION(BlueprintCallable, Category = "Actor")
	virtual void SetOwner(AActor* NewOwner);
	UFUNCTION(BlueprintCallable, Category = "Actor")
	AActor* GetOwner() const;
	template <class T> T* GetOwner() const { return Cast<T>(GetOwner()); }
	UFUNCTION(BlueprintCallable, Category = "Actor")
	APawn* GetInstigator() const;
	UFUNCTION(BlueprintCallable, Category = "Actor")
	AController* GetInstigatorController() const;
	UFUNCTION(BlueprintCallable, Category = "Actor")
	bool ActorHasTag(FName Tag) const;
	UFUNCTION(BlueprintCallable, Category = "Actor|Tick")
	void SetActorTickEnabled(bool bEnabled);
	UFUNCTION(BlueprintCallable, Category = "Actor|Tick")
	bool IsActorTickEnabled() const;
	UFUNCTION(BlueprintCallable, Category = "Actor|Tick")
	void SetActorTickInterval(float TickInterval);
	UFUNCTION(BlueprintCallable, Category = "Actor|Tick")
	float GetActorTickInterval() const;
	UFUNCTION(BlueprintCallable, Category = "Actor")
	float GetActorTimeDilation() const;
	UFUNCTION(BlueprintCallable, Category = "Collision", meta = (DisplayName = "Get Actor Bounds"))
	virtual void GetActorBounds(bool bOnlyCollidingComponents, FVector& Origin, FVector& BoxExtent, bool bIncludeFromChildActors = false) const;
	virtual FBox GetComponentsBoundingBox(bool bNonColliding = false, bool bIncludeFromChildActors = false) const;
	UFUNCTION(BlueprintCallable, Category = "Actor")
	float GetDistanceTo(const AActor* OtherActor) const;
	UFUNCTION(BlueprintCallable, Category = "Actor")
	float GetSquaredDistanceTo(const AActor* OtherActor) const;
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void EnableInput(class APlayerController* PlayerController);
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DisableInput(class APlayerController* PlayerController);
	UFUNCTION(BlueprintCallable, Category = "Actor")
	bool IsActorBeingDestroyed() const;
	bool HasActorBegunPlay() const;
	bool IsActorInitialized() const;
	bool AttachToActor(AActor* ParentActor, const FAttachmentTransformRules& AttachmentRules, FName SocketName = NAME_None);
	bool AttachToComponent(USceneComponent* Parent, const FAttachmentTransformRules& AttachmentRules, FName SocketName = NAME_None);
	void DetachFromActor(const FDetachmentTransformRules& DetachmentRules);
	UFUNCTION(BlueprintCallable, Category = "Actor")
	AActor* GetAttachParentActor() const;
	UFUNCTION(BlueprintCallable, Category = "Actor")
	void GetAttachedActors(TArray<AActor*>& OutActors, bool bResetArray = true, bool bRecursivelyIncludeAttachedActors = false) const;

	UFUNCTION(BlueprintCallable, Category = "Components", meta = (ComponentClass = "/Script/Engine.ActorComponent"), meta = (DeterminesOutputType = "ComponentClass"))
	UActorComponent* GetComponentByClass(TSubclassOf<UActorComponent> ComponentClass) const;
	virtual UActorComponent* FindComponentByClass(const TSubclassOf<UActorComponent> ComponentClass) const;
	template <class T> T* FindComponentByClass() const
	{
		static_assert(std::is_base_of_v<UActorComponent, T>, "'T' template parameter to FindComponentByClass must be derived from UActorComponent");
		return (T*)FindComponentByClass(T::StaticClass());
	}
	template <class T> T* GetComponentByClass() const { return FindComponentByClass<T>(); }
	/** The engine's body is `typedef TPointedToType<ComponentType> T; ... ForEachComponent_Internal<T>(T::StaticClass(), ...)`,
	 *  and ForEachComponent_Internal static_asserts that T derives from UActorComponent: the element is a T* or a
	 *  TObjectPtr<T> (nothing else has a pointed-to type), T is complete, a component class, and has StaticClass(). */
	template <class ComponentType, class AllocatorType>
	void GetComponents(TArray<ComponentType, AllocatorType>& OutComponents, bool bIncludeFromChildActors = false) const
	{
		typedef typename UE::Stub::TComponentPointee<ComponentType>::Type T;
		static_assert(sizeof(T) > 0, "GetComponents: the component class is incomplete here (include its header)");
		static_assert(std::is_convertible_v<T*, const UActorComponent*>, "'ComponentType' template parameter to ForEachComponent must be derived from UActorComponent");
		(void)T::StaticClass();
		OutComponents.Reset();
	}
	void GetComponents(TSubclassOf<UActorComponent> ComponentClass, TArray<UActorComponent*>& OutComponents, bool bIncludeFromChildActors = false) const;
	const TSet<UActorComponent*>& GetComponents() const;
	/** InFunc is called with each ComponentType* (the engine static_asserts that ComponentType derives from UActorComponent). */
	template <class ComponentType, typename Func> requires std::is_convertible_v<ComponentType*, const UActorComponent*> && std::is_invocable_v<Func&, ComponentType*>
	void ForEachComponent(bool bIncludeFromChildActors, Func InFunc) const UE_STUB_BODY
	template <typename Func> requires std::is_invocable_v<Func&, UActorComponent*>
	void ForEachComponent(bool bIncludeFromChildActors, Func InFunc) const UE_STUB_BODY
	void AddInstanceComponent(UActorComponent* Component);
	void RemoveInstanceComponent(UActorComponent* Component);
	void AddOwnedComponent(UActorComponent* Component);
	UActorComponent* AddComponentByClass(TSubclassOf<UActorComponent> Class, bool bManualAttachment, const FTransform& RelativeTransform, bool bDeferredFinish);
	void FinishAddComponent(UActorComponent* Component, bool bManualAttachment, const FTransform& RelativeTransform);

	virtual UWorld* GetWorld() const override final;
	ULevel* GetLevel() const;
	UGameInstance* GetGameInstance() const;
	template <class T> T* GetGameInstance() const { return Cast<T>(GetGameInstance()); }
	ENetMode GetNetMode() const;
	bool HasAuthority() const;
	void SetReplicates(bool bInReplicates);
	void SetCanBeDamaged(bool bInCanBeDamaged);
	void ForceNetUpdate();
#if WITH_EDITOR
	void SetActorLabel(const FString& NewActorLabel, bool bMarkDirty = true);
	const FString& GetActorLabel(bool bCreateIfNone = true) const;
	void SetFolderPath(const FName& NewFolderPath);
#endif
	FString GetActorNameOrLabel() const;
	FString GetHumanReadableName() const;

	virtual void Tick(float DeltaSeconds);
	virtual void TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction);
	virtual void PreInitializeComponents();
	virtual void PostInitializeComponents();
	virtual void OnConstruction(const FTransform& Transform) {}
	virtual void PostActorCreated();
	virtual void PostRegisterAllComponents();
	virtual void PreRegisterAllComponents();
	virtual void PostUnregisterAllComponents();
	virtual void Destroyed();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);
	virtual void LifeSpanExpired();
	virtual void Reset();
	virtual void TornOff();
	virtual void BecomeViewTarget(APlayerController* PC);
	virtual void EndViewTarget(APlayerController* PC);
	virtual void CalcCamera(float DeltaTime, struct FMinimalViewInfo& OutResult);
	virtual bool HasActiveCameraComponent(bool bForceFindCamera = false) const;
	virtual bool ShouldTickIfViewportsOnly() const;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor);
	virtual void NotifyActorEndOverlap(AActor* OtherActor);
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser);
	virtual void GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
	virtual bool IsLevelBoundsRelevant() const;
protected:
	virtual void BeginPlay();
	virtual void RegisterActorTickFunctions(bool bRegister);
};

template <class T, uint32 NumElements>
TInlineComponentArray<T, NumElements>::TInlineComponentArray(const AActor* Actor, bool bIncludeFromChildActors) : Super()
{
	if (Actor)
	{
		Actor->GetComponents(*this, bIncludeFromChildActors);
	}
}
