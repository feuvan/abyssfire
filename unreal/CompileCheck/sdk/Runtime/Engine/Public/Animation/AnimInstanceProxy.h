// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/Animation/AnimInstanceProxy.h (UE 5.x source knowledge). The
// engine struct is a USTRUCT (not exposed to Python); the virtual hooks are protected and UAnimInstance is a friend.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimInstance.h"
#include "BoneContainer.h"

class USkeleton;
class USkeletalMeshComponent;
struct FPoseContext;
struct FAnimNode_Base;
struct FAnimationUpdateContext;
struct FAnimationInitializeContext;

/** Proxy object passed around during animation tree update in lieu of a UAnimInstance. */
struct FAnimInstanceProxy
{
	UE_STUB_STRUCT_BODY(FAnimInstanceProxy)
public:
	FAnimInstanceProxy();
	FAnimInstanceProxy(UAnimInstance* Instance);
	FAnimInstanceProxy(const FAnimInstanceProxy&);
	FAnimInstanceProxy& operator=(FAnimInstanceProxy&&);
	FAnimInstanceProxy& operator=(const FAnimInstanceProxy&);
	virtual ~FAnimInstanceProxy();

	float GetDeltaSeconds() const;
	float GetTimeDilation() const;
	UObject* GetAnimInstanceObject();
	const UObject* GetAnimInstanceObject() const;
	USkeleton* GetSkeleton();
	USkeletalMeshComponent* GetSkelMeshComponent() const;
	const FBoneContainer& GetRequiredBones() const;
	FBoneContainer& GetRequiredBones();
	const FTransform& GetComponentTransform() const;
	const FTransform& GetActorTransform() const;
	int32 GetLODLevel() const;

protected:
	/** Called when our anim instance is being initialized. */
	virtual void Initialize(UAnimInstance* InAnimInstance);
	/** Called when our anim instance is being uninitialized. */
	virtual void Uninitialize(UAnimInstance* InAnimInstance);
	/** Called before update so we can copy any data we need (game thread). */
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds);
	/** Update override point (worker thread). */
	virtual void Update(float DeltaSeconds) {}
	/** Updates the anim graph. */
	virtual void UpdateAnimationNode(const FAnimationUpdateContext& InContext);
	/** Called on the game thread post-update. */
	virtual void PostUpdate(UAnimInstance* InAnimInstance) const;
	/** Called on the game thread post-evaluation. */
	virtual void PostEvaluate(UAnimInstance* InAnimInstance);
	/** Calls Initialize on the root node. */
	virtual void InitializeRootNode(bool bInDeferRootNodeInitialization = false);
	/** Evaluate override point; return true if this function is implemented, false otherwise (the graph runs). */
	virtual bool Evaluate(FPoseContext& Output) { return false; }
	virtual bool Evaluate_WithRoot(FPoseContext& Output, FAnimNode_Base* InRootNode);
	/** Called when bones are recached. */
	virtual void CacheBones();

	friend class UAnimInstance;
	friend class USkeletalMeshComponent;
};
