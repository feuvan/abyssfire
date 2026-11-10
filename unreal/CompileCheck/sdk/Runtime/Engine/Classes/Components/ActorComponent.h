// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Components/ActorComponent.h (UE 5.x source knowledge;
// UFUNCTION-marked members: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/EngineTypes.h"

class AActor;
class UWorld;

class UActorComponent : public UObject
{
	UE_STUB_CLASS_BODY(UActorComponent, UObject)
public:
	UPROPERTY(EditDefaultsOnly, Category = "ComponentTick")
	struct FActorComponentTickFunction PrimaryComponentTick;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Tags)
	TArray<FName> ComponentTags;
	uint8 bAutoActivate : 1;
	uint8 bWantsInitializeComponent : 1;
	uint8 bTickInEditor : 1;
	uint8 bNeverNeedsRenderUpdate : 1;
	bool IsRegistered() const;
	bool IsBeingDestroyed() const;
	bool HasBeenCreated() const;
	bool HasBeenInitialized() const;
	bool HasBegunPlay() const;
	UFUNCTION(BlueprintCallable, Category = "Components|Activation", meta = (UnsafeDuringActorConstruction = "true"))
	virtual void Activate(bool bReset = false);
	UFUNCTION(BlueprintCallable, Category = "Components|Activation", meta = (UnsafeDuringActorConstruction = "true"))
	virtual void Deactivate();
	UFUNCTION(BlueprintCallable, Category = "Components|Activation", meta = (UnsafeDuringActorConstruction = "true"))
	virtual void SetActive(bool bNewActive, bool bReset = false);
	UFUNCTION(BlueprintCallable, Category = "Components|Activation", meta = (UnsafeDuringActorConstruction = "true"))
	virtual void ToggleActive();
	UFUNCTION(BlueprintCallable, Category = "Components|Activation", meta = (UnsafeDuringActorConstruction = "true"))
	bool IsActive() const;
	UFUNCTION(BlueprintCallable, Category = "Components|Activation")
	virtual void SetAutoActivate(bool bNewAutoActivate);
	UFUNCTION(BlueprintCallable, Category = "Components|Tick")
	virtual void SetComponentTickEnabled(bool bEnabled);
	UFUNCTION(BlueprintCallable, Category = "Components|Tick")
	virtual bool IsComponentTickEnabled() const;
	UFUNCTION(BlueprintCallable, Category = "Components|Tick")
	void SetComponentTickInterval(float TickInterval);
	UFUNCTION(BlueprintCallable, Category = "Components|Tick")
	void SetTickGroup(ETickingGroup NewTickGroup);
	UFUNCTION(BlueprintCallable, Category = "Components")
	bool ComponentHasTag(FName Tag) const;
	UFUNCTION(BlueprintCallable, Category = "Components", meta = (Keywords = "Delete", HidePin = "Object", DefaultToSelf = "Object", DisplayName = "Destroy Component", ScriptName = "DestroyComponent"))
	void K2_DestroyComponent(UObject* Object);
	UFUNCTION(BlueprintCallable, Category = "Components")
	AActor* GetOwner() const;
	template <class T> T* GetOwner() const { return Cast<T>(GetOwner()); }
	virtual UWorld* GetWorld() const override final;
	virtual void OnRegister();
	virtual void OnUnregister();
	virtual void InitializeComponent();
	virtual void UninitializeComponent();
	virtual void BeginPlay();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction);
	virtual void OnComponentCreated();
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy);
	virtual void DestroyComponent(bool bPromoteChildren = false);
	void RegisterComponent();
	void UnregisterComponent();
	void RegisterComponentWithWorld(UWorld* InWorld);
	void ReregisterComponent();
	void MarkRenderStateDirty();
	void MarkRenderDynamicDataDirty();
	void MarkRenderTransformDirty();
#if !(defined(UE_STUB_UNVERIFIED_NO_MARK_RENDER_INSTANCES_DIRTY) && UE_STUB_UNVERIFIED_NO_MARK_RENDER_INSTANCES_DIRTY)
	// UNVERIFIED (API_UNCERTAIN.md #5): believed to be on UActorComponent since the 5.4 instance-data manager ("marks the
	// instances as dirty - changes to instance transforms / custom data are sent to the rendering thread at the end of
	// the frame"). Not reflected, so the 5.6 Python stub cannot confirm it. The module reaches it through a hedge
	// (AbyssWorldUtil::MarkInstancesDirty): define the macro to type-check the fallback of an engine without it.
	void MarkRenderInstancesDirty();
#endif
	/** Set value of bCanEverAffectNavigation flag and update navigation octree if needed. */
	void SetCanEverAffectNavigation(bool bRelevant);
	bool CanEverAffectNavigation() const;
	void RecreateRenderState_Concurrent();
	bool IsRenderStateCreated() const;
	virtual void SetIsReplicated(bool ShouldReplicate);
	void AddTickPrerequisiteActor(AActor* PrerequisiteActor);
	void AddTickPrerequisiteComponent(UActorComponent* PrerequisiteComponent);
	ENetMode GetNetMode() const;
	FString GetReadableName() const;
};
