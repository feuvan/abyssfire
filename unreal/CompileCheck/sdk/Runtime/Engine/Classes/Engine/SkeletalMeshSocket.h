// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/SkeletalMeshSocket.h (UE 5.x source knowledge;
// UPROPERTY-marked members: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

class USkeletalMeshComponent;
class USkinnedMeshComponent;

class USkeletalMeshSocket : public UObject
{
	UE_STUB_CLASS_BODY(USkeletalMeshSocket, UObject)
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Socket Parameters")
	FName SocketName;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Socket Parameters")
	FName BoneName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket Parameters")
	FVector RelativeLocation;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket Parameters")
	FRotator RelativeRotation;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket Parameters")
	FVector RelativeScale;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket Parameters")
	bool bForceAlwaysAnimated;
	FVector GetSocketLocation(const class USkeletalMeshComponent* SkelComp) const;
	FTransform GetSocketLocalTransform() const;
	FTransform GetSocketTransform(const class USkeletalMeshComponent* SkelComp) const;
};
