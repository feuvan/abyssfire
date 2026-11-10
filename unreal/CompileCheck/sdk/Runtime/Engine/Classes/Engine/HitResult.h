// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/HitResult.h (UE 5.x source knowledge; reflected
// struct: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakObjectPtr.h"

class AActor;
class UPrimitiveComponent;
class UPhysicalMaterial;

struct FHitResult
{
	UE_STUB_STRUCT_BODY(FHitResult)
	UPROPERTY()
	int32 FaceIndex;
	UPROPERTY()
	float Time;
	UPROPERTY()
	float Distance;
	UPROPERTY()
	FVector Location;
	UPROPERTY()
	FVector ImpactPoint;
	UPROPERTY()
	FVector Normal;
	UPROPERTY()
	FVector ImpactNormal;
	UPROPERTY()
	FVector TraceStart;
	UPROPERTY()
	FVector TraceEnd;
	UPROPERTY()
	float PenetrationDepth;
	UPROPERTY()
	int32 Item;
	UPROPERTY()
	uint8 bBlockingHit : 1;
	UPROPERTY()
	uint8 bStartPenetrating : 1;
	UPROPERTY()
	TWeakObjectPtr<UPhysicalMaterial> PhysMaterial;
	UPROPERTY()
	TWeakObjectPtr<UPrimitiveComponent> Component;
	UPROPERTY()
	FName BoneName;
	FHitResult();
	explicit FHitResult(float InTime);
	explicit FHitResult(EForceInit InInit);
	AActor* GetActor() const;
	UPrimitiveComponent* GetComponent() const;
	bool IsValidBlockingHit() const;
	void Init();
	void Reset(float InTime = 1.f, bool bPreserveTraceData = true);
};
