// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Animation/AnimInstance.h (UE 5.x source knowledge;
// UFUNCTION / UPROPERTY-marked members: checked against the 5.6 stub). UCLASS(transient, Blueprintable, BlueprintType,
// Within = SkeletalMeshComponent). The montage API is not modelled (the module plays sequences through its own proxy).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimEnums.h"

class AActor;
class APawn;
class USkeleton;
class USkeletalMeshComponent;
struct FAnimInstanceProxy;

enum class EUpdateAnimationFlag : uint8
{
	/** Enforces a parallel update, regardless of state. */
	ForceParallelUpdate,
	/** Use state to determine whether or not to immediately or update in parallel. */
	Default,
};

class UAnimInstance : public UObject
{
	UE_STUB_CLASS_BODY(UAnimInstance, UObject)
public:
	/** This is used to extract animation. If Mesh exists, this will be overwritten by Mesh->Skeleton. */
	TObjectPtr<USkeleton> CurrentSkeleton;
	UPROPERTY(Category = RootMotion, EditDefaultsOnly)
	TEnumAsByte<ERootMotionMode::Type> RootMotionMode;

	UFUNCTION(BlueprintCallable, Category = "Animation", meta = (BlueprintThreadSafe))
	virtual AActor* GetOwningActor() const;
	UFUNCTION(BlueprintCallable, Category = "Animation", meta = (BlueprintThreadSafe))
	USkeletalMeshComponent* GetOwningComponent() const;
	UFUNCTION(BlueprintCallable, Category = "Animation", meta = (BlueprintThreadSafe))
	virtual APawn* TryGetPawnOwner() const;
	USkeletalMeshComponent* GetSkelMeshComponent() const;
	UFUNCTION(BlueprintPure, Category = "Animation", meta = (BlueprintThreadSafe))
	float GetDeltaSeconds() const;
	UFUNCTION(BlueprintPure, Category = "Animation", meta = (BlueprintThreadSafe))
	float GetCurveValue(FName CurveName) const;
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void SetRootMotionMode(TEnumAsByte<ERootMotionMode::Type> Value);
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void SetMorphTarget(FName MorphTargetName, float Value);
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void ClearMorphTargets();

	/** Native initialization override point. */
	virtual void NativeInitializeAnimation();
	/** Native update override point (game thread). It is usually a good idea to simply gather data in this step. */
	virtual void NativeUpdateAnimation(float DeltaSeconds);
	/** Native thread safe update override point (worker thread, before the graph). */
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds);
	/** Native Post Evaluate override point. */
	virtual void NativePostEvaluateAnimation();
	/** Native Uninitialize override point. */
	virtual void NativeUninitializeAnimation();
	/** Executed when begin play is called on the owning component. */
	virtual void NativeBeginPlay();

	void InitializeAnimation(bool bInDeferRootNodeInitialization = false);
	void UpdateAnimation(float DeltaSeconds, bool bNeedsValidRootMotion, EUpdateAnimationFlag UpdateFlag = EUpdateAnimationFlag::Default);
	bool NeedsUpdate() const;

	virtual void PostInitProperties() override;
	virtual void BeginDestroy() override;

protected:
	/** Proxy object, nothing should access this from an externally-callable API as it is used as a scratch area on worker threads. */
	mutable FAnimInstanceProxy* AnimInstanceProxy;
	/** Access the proxy but block if a task is currently in progress as it wouldn't be safe to access it (game thread only). */
	template <typename T /*= FAnimInstanceProxy*/>
	T& GetProxyOnGameThread() UE_STUB_BODY
	template <typename T /*= FAnimInstanceProxy*/>
	const T& GetProxyOnGameThread() const UE_STUB_BODY
	/** Access the proxy but block if a task is currently in progress (and we are on the game thread) as it wouldn't be safe to access it. */
	template <typename T /*= FAnimInstanceProxy*/>
	T& GetProxyOnAnyThread() UE_STUB_BODY
	template <typename T /*= FAnimInstanceProxy*/>
	const T& GetProxyOnAnyThread() const UE_STUB_BODY
	/** Override point for derived classes to create their own proxy objects (allows custom allocation). */
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy();
	/** Override point for derived classes to destroy their own proxy objects (allows custom allocation). */
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy);

	friend struct FAnimInstanceProxy;
};
