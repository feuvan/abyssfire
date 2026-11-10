// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Camera/PlayerCameraManager.h (UE 5.x source knowledge;
// UPROPERTY / UFUNCTION-marked members: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraTypes.h"

class APlayerController;

UENUM()
enum EViewTargetBlendFunction : int
{
	VTBlend_Linear,
	VTBlend_Cubic,
	VTBlend_EaseIn,
	VTBlend_EaseOut,
	VTBlend_EaseInOut,
	VTBlend_PreBlended,
	VTBlend_MAX,
};

struct FViewTargetTransitionParams
{
	UE_STUB_STRUCT_BODY(FViewTargetTransitionParams)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ViewTargetTransitionParams)
	float BlendTime;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ViewTargetTransitionParams)
	TEnumAsByte<enum EViewTargetBlendFunction> BlendFunction;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ViewTargetTransitionParams)
	float BlendExp;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ViewTargetTransitionParams)
	uint32 bLockOutgoing : 1;
	FViewTargetTransitionParams();
};

class APlayerCameraManager : public AActor
{
	UE_STUB_CLASS_BODY(APlayerCameraManager, AActor)
public:
	UPROPERTY(transient)
	TObjectPtr<APlayerController> PCOwner;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PlayerCameraManager)
	float DefaultFOV;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PlayerCameraManager)
	float ViewPitchMin;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PlayerCameraManager)
	float ViewPitchMax;
	FLinearColor FadeColor;
	float FadeAmount;
	uint32 bEnableFading : 1;
	UFUNCTION(BlueprintCallable, Category = "Camera", meta = (Keywords = "View Position"))
	FVector GetCameraLocation() const;
	UFUNCTION(BlueprintCallable, Category = "Camera", meta = (Keywords = "View Orientation"))
	FRotator GetCameraRotation() const;
	UFUNCTION(BlueprintCallable, Category = "Camera")
	virtual float GetFOVAngle() const;
	virtual void SetFOV(float NewFOV);
	virtual void UnlockFOV();
	UFUNCTION(BlueprintCallable, Category = "Camera")
	virtual APlayerController* GetOwningPlayerController() const;
	AActor* GetViewTarget() const;
	void GetCameraViewPoint(FVector& OutCamLoc, FRotator& OutCamRot) const;
	virtual void SetViewTarget(class AActor* NewViewTarget, FViewTargetTransitionParams TransitionParams = FViewTargetTransitionParams());
	UFUNCTION(BlueprintCallable, Category = "Camera Fades")
	virtual void StartCameraFade(float FromAlpha, float ToAlpha, float Duration, FLinearColor Color, bool bShouldFadeAudio = false, bool bHoldWhenFinished = false);
	UFUNCTION(BlueprintCallable, Category = "Camera Fades")
	virtual void StopCameraFade();
	UFUNCTION(BlueprintCallable, Category = "Camera Fades")
	virtual void SetManualCameraFade(float InFadeAmount, FLinearColor Color, bool bInFadeAudio);
	virtual void UpdateCamera(float DeltaTime);
	const FMinimalViewInfo& GetCameraCacheView() const;
	float GetCameraCacheTime() const;
};
