// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Camera/CameraTypes.h (UE 5.x source knowledge; reflected
// struct / enum: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/Scene.h"

UENUM()
namespace ECameraProjectionMode
{
	enum Type : int
	{
		Perspective,
		Orthographic,
	};
}

struct FMinimalViewInfo
{
	UE_STUB_STRUCT_BODY(FMinimalViewInfo)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	FVector Location;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	FRotator Rotation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	float FOV;
	UPROPERTY(Transient)
	float DesiredFOV;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	float OrthoWidth;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	float OrthoNearClipPlane;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	float OrthoFarClipPlane;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	float AspectRatio;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	uint32 bConstrainAspectRatio : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	TEnumAsByte<ECameraProjectionMode::Type> ProjectionMode;
	UPROPERTY(BlueprintReadWrite, Category = Camera)
	float PostProcessBlendWeight;
	UPROPERTY(BlueprintReadWrite, Category = Camera)
	struct FPostProcessSettings PostProcessSettings;
	FMinimalViewInfo();
	bool Equals(const FMinimalViewInfo& OtherInfo) const;
	void BlendViewInfo(FMinimalViewInfo& OtherInfo, float OtherWeight);
	FMatrix CalculateProjectionMatrix() const;
};
