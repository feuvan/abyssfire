// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Components/SkeletalMeshComponent.h (UE 5.x source
// knowledge; UFUNCTION / UPROPERTY-marked members and the enum: checked against the 5.6 stub). The real class also
// implements IInterface_CollisionDataProvider (not modelled). Physics, cloth and attribute API is not declared.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/SkinnedMeshComponent.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"

class UAnimInstance;
class UAnimationAsset;
class USkeletalMesh;

UENUM(BlueprintType)
namespace EAnimationMode
{
	enum Type : int
	{
		AnimationBlueprint UMETA(DisplayName = "Use Animation Blueprint"),
		AnimationSingleNode UMETA(DisplayName = "Use Animation Asset"),
		// This is custom type, engine leaves AnimInstance as it is
		AnimationCustomMode UMETA(DisplayName = "Use Custom Mode"),
	};
}

class USkeletalMeshComponent : public USkinnedMeshComponent
{
	UE_STUB_CLASS_BODY(USkeletalMeshComponent, USkinnedMeshComponent)
public:
	/** The AnimBlueprint class to use. Use 'SetAnimInstanceClass' to change at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Animation)
	TSubclassOf<UAnimInstance> AnimClass;
	/** The active animation graph program instance. */
	UPROPERTY(transient, NonTransactional)
	TObjectPtr<UAnimInstance> AnimScriptInstance;
	/** Used to scale speed of all animations on this skeletal mesh. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = Animation)
	float GlobalAnimRateScale;
	/** pauses this component's animations (doesn't tick them, but still refreshes bones). */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = Animation)
	uint8 bPauseAnims : 1;
	/** Skips Ticking and Bone Refresh. */
	UPROPERTY(Transient, BlueprintReadWrite, Category = SkeletalMesh)
	uint8 bNoSkeletonUpdate : 1;
	/** If true, AnimInstance will be updated even when the mesh is not rendered in the editor. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Animation)
	uint8 bEnableAnimation : 1;

	/** Set the anim instance class. Clears and re-initializes the anim instance with the new class and sets animation mode to 'AnimationBlueprint'. */
	UFUNCTION(BlueprintCallable, Category = "Components|SkeletalMesh", meta = (Keywords = "AnimBlueprint", DisplayName = "Set Anim Instance Class"))
	virtual void SetAnimClass(class UClass* NewClass);
	/** Get the anim instance class via getter callable by sequencer. */
	UFUNCTION(BlueprintInternalUseOnly)
	class UClass* GetAnimClass();
	/** Set the anim instance class. Clears and re-initializes the anim instance with the new class and sets animation mode to 'AnimationBlueprint'. */
	UFUNCTION(BlueprintCallable, Category = "Components|SkeletalMesh", meta = (Keywords = "AnimBlueprint"))
	void SetAnimInstanceClass(class UClass* NewClass);
	/** Returns the animation instance that is driving the class (if available). */
	UFUNCTION(BlueprintCallable, Category = "Components|SkeletalMesh", meta = (Keywords = "AnimBlueprint"))
	class UAnimInstance* GetAnimInstance() const;
	/** Returns the active post process instance if one is available. */
	UFUNCTION(BlueprintCallable, Category = "Components|SkeletalMesh", meta = (Keywords = "AnimBlueprint"))
	UAnimInstance* GetPostProcessInstance() const;
	/** Below are the interface to control animation when animation mode, not blueprint mode. */
	UFUNCTION(BlueprintCallable, Category = "Components|Animation", meta = (Keywords = "Animation"))
	void SetAnimationMode(EAnimationMode::Type InAnimationMode, bool bForceInitAnimScriptInstance = true);
	UFUNCTION(BlueprintCallable, Category = "Components|Animation", meta = (Keywords = "Animation"))
	EAnimationMode::Type GetAnimationMode() const;
	/* Animation play functions (single node mode). */
	UFUNCTION(BlueprintCallable, Category = "Components|Animation", meta = (Keywords = "Animation"))
	void PlayAnimation(class UAnimationAsset* NewAnimToPlay, bool bLooping);
	UFUNCTION(BlueprintCallable, Category = "Components|Animation", meta = (Keywords = "Animation"))
	void SetAnimation(class UAnimationAsset* NewAnimToPlay);
	UFUNCTION(BlueprintCallable, Category = "Components|Animation", meta = (Keywords = "Animation"))
	void Play(bool bLooping);
	UFUNCTION(BlueprintCallable, Category = "Components|Animation", meta = (Keywords = "Animation"))
	void Stop();
	UFUNCTION(BlueprintCallable, Category = "Components|Animation", meta = (Keywords = "Animation"))
	bool IsPlaying() const;
	UFUNCTION(BlueprintCallable, Category = "Components|Animation", meta = (Keywords = "Animation"))
	void SetPosition(float InPos, bool bFireNotifies = true);
	UFUNCTION(BlueprintCallable, Category = "Components|Animation", meta = (Keywords = "Animation"))
	float GetPosition() const;
	UFUNCTION(BlueprintCallable, Category = "Components|Animation", meta = (Keywords = "Animation"))
	void SetPlayRate(float Rate);
	UFUNCTION(BlueprintCallable, Category = "Components|Animation", meta = (Keywords = "Animation"))
	float GetPlayRate() const;
	/** Sets whether or not to force tick component in order to update animation and refresh transform for this component (editor). */
	UFUNCTION(BlueprintCallable, Category = "Components|SkeletalMesh")
	void SetUpdateAnimationInEditor(const bool NewUpdateState);
	/** Set the SkeletalMesh rendered for this mesh. */
	UFUNCTION(BlueprintCallable, Category = "Components|SkeletalMesh")
	void SetSkeletalMeshAsset(USkeletalMesh* NewMesh);
	/** Get the SkeletalMesh rendered for this mesh. */
	UFUNCTION(BlueprintPure, Category = "Components|SkeletalMesh")
	USkeletalMesh* GetSkeletalMeshAsset() const;
	virtual void SetSkinnedAssetAndUpdate(USkinnedAsset* NewMesh, bool bReinitPose = true) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
};
