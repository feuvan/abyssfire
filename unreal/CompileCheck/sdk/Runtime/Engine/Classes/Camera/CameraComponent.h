// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Camera/CameraComponent.h (UE 5.x source knowledge;
// UPROPERTY / UFUNCTION-marked members: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Camera/CameraTypes.h"
#include "Engine/Scene.h"

class UCameraComponent : public USceneComponent
{
	UE_STUB_CLASS_BODY(UCameraComponent, USceneComponent)
public:
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = CameraSettings)
	float FieldOfView;
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = CameraSettings)
	float OrthoWidth;
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = CameraSettings)
	float OrthoNearClipPlane;
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = CameraSettings)
	float OrthoFarClipPlane;
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = CameraSettings)
	float AspectRatio;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CameraOptions)
	uint8 bConstrainAspectRatio : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CameraOptions)
	uint8 bUsePawnControlRotation : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CameraOptions)
	uint8 bLockToHmd : 1;
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = CameraSettings)
	TEnumAsByte<ECameraProjectionMode::Type> ProjectionMode;
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = PostProcess)
	float PostProcessBlendWeight;
	UPROPERTY(Interp, BlueprintReadWrite, Category = PostProcess)
	struct FPostProcessSettings PostProcessSettings;
	UFUNCTION(BlueprintCallable, Category = Camera)
	virtual void SetFieldOfView(float InFieldOfView);
	UFUNCTION(BlueprintCallable, Category = Camera)
	void SetOrthoWidth(float InOrthoWidth);
	UFUNCTION(BlueprintCallable, Category = Camera)
	void SetAspectRatio(float InAspectRatio);
	UFUNCTION(BlueprintCallable, Category = Camera)
	void SetConstraintAspectRatio(bool bInConstrainAspectRatio);
	UFUNCTION(BlueprintCallable, Category = Camera)
	void SetProjectionMode(ECameraProjectionMode::Type InProjectionMode);
	UFUNCTION(BlueprintCallable, Category = Camera)
	void SetPostProcessBlendWeight(float InPostProcessBlendWeight);
	UFUNCTION(BlueprintCallable, Category = Camera)
	virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView);
};
