// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/EngineTypes.h (UE 5.x source knowledge; reflected
// enums / structs: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Templates/SubclassOf.h"
#include "Engine/TimerHandle.h"
#include "Engine/HitResult.h"

class AActor;
class USceneComponent;

UENUM()
namespace EEndPlayReason
{
	enum Type : int
	{
		Destroyed,
		LevelTransition,
		EndPlayInEditor,
		RemovedFromWorld,
		Quit,
	};
}
UENUM(BlueprintType)
enum ECollisionChannel : int
{
	ECC_WorldStatic,
	ECC_WorldDynamic,
	ECC_Pawn,
	ECC_Visibility,
	ECC_Camera,
	ECC_PhysicsBody,
	ECC_Vehicle,
	ECC_Destructible,
	ECC_EngineTraceChannel1,
	ECC_EngineTraceChannel2,
	ECC_EngineTraceChannel3,
	ECC_EngineTraceChannel4,
	ECC_EngineTraceChannel5,
	ECC_EngineTraceChannel6,
	ECC_GameTraceChannel1,
	ECC_GameTraceChannel2,
	ECC_GameTraceChannel3,
	ECC_GameTraceChannel4,
	ECC_GameTraceChannel5,
	ECC_GameTraceChannel6,
	ECC_GameTraceChannel7,
	ECC_GameTraceChannel8,
	ECC_GameTraceChannel9,
	ECC_GameTraceChannel10,
	ECC_GameTraceChannel11,
	ECC_GameTraceChannel12,
	ECC_GameTraceChannel13,
	ECC_GameTraceChannel14,
	ECC_GameTraceChannel15,
	ECC_GameTraceChannel16,
	ECC_GameTraceChannel17,
	ECC_GameTraceChannel18,
	ECC_OverlapAll_Deprecated,
	ECC_MAX,
};
UENUM(BlueprintType)
enum ETraceTypeQuery : int
{
	TraceTypeQuery1, TraceTypeQuery2, TraceTypeQuery3, TraceTypeQuery4, TraceTypeQuery5, TraceTypeQuery6, TraceTypeQuery7, TraceTypeQuery8,
	TraceTypeQuery9, TraceTypeQuery10, TraceTypeQuery11, TraceTypeQuery12, TraceTypeQuery13, TraceTypeQuery14, TraceTypeQuery15, TraceTypeQuery16,
	TraceTypeQuery17, TraceTypeQuery18, TraceTypeQuery19, TraceTypeQuery20, TraceTypeQuery21, TraceTypeQuery22, TraceTypeQuery23, TraceTypeQuery24,
	TraceTypeQuery25, TraceTypeQuery26, TraceTypeQuery27, TraceTypeQuery28, TraceTypeQuery29, TraceTypeQuery30, TraceTypeQuery31, TraceTypeQuery32,
	TraceTypeQuery_MAX,
};
UENUM(BlueprintType)
enum EObjectTypeQuery : int
{
	ObjectTypeQuery1, ObjectTypeQuery2, ObjectTypeQuery3, ObjectTypeQuery4, ObjectTypeQuery5, ObjectTypeQuery6, ObjectTypeQuery7, ObjectTypeQuery8,
	ObjectTypeQuery9, ObjectTypeQuery10, ObjectTypeQuery11, ObjectTypeQuery12, ObjectTypeQuery13, ObjectTypeQuery14, ObjectTypeQuery15, ObjectTypeQuery16,
	ObjectTypeQuery17, ObjectTypeQuery18, ObjectTypeQuery19, ObjectTypeQuery20, ObjectTypeQuery21, ObjectTypeQuery22, ObjectTypeQuery23, ObjectTypeQuery24,
	ObjectTypeQuery25, ObjectTypeQuery26, ObjectTypeQuery27, ObjectTypeQuery28, ObjectTypeQuery29, ObjectTypeQuery30, ObjectTypeQuery31, ObjectTypeQuery32,
	ObjectTypeQuery_MAX,
};
UENUM(BlueprintType, meta = (ScriptName = "CollisionResponseType"))
enum ECollisionResponse : int
{
	ECR_Ignore,
	ECR_Overlap,
	ECR_Block,
	ECR_MAX,
};
UENUM(BlueprintType)
namespace ECollisionEnabled
{
	enum Type : int
	{
		NoCollision,
		QueryOnly,
		PhysicsOnly,
		QueryAndPhysics,
		ProbeOnly,
		QueryAndProbe,
	};
}
UENUM()
namespace EComponentMobility
{
	enum Type : int
	{
		Static,
		Stationary,
		Movable,
	};
}
UENUM()
enum class EAttachmentRule : uint8
{
	KeepRelative,
	KeepWorld,
	SnapToTarget,
};
UENUM()
enum class EDetachmentRule : uint8
{
	KeepRelative,
	KeepWorld,
};
UENUM(BlueprintType)
enum class ESpawnActorCollisionHandlingMethod : uint8
{
	Undefined,
	AlwaysSpawn,
	AdjustIfPossibleButAlwaysSpawn,
	AdjustIfPossibleButDontSpawnIfColliding,
	DontSpawnIfColliding,
};
UENUM()
enum EBlendMode : int
{
	BLEND_Opaque,
	BLEND_Masked,
	BLEND_Translucent,
	BLEND_Additive,
	BLEND_Modulate,
	BLEND_AlphaComposite,
	BLEND_AlphaHoldout,
	BLEND_TranslucentColoredTransmittance,
	BLEND_MAX,
};
UENUM()
enum ESceneDepthPriorityGroup : int
{
	SDPG_World,
	SDPG_Foreground,
	SDPG_MAX,
};

struct FAttachmentTransformRules
{
	static FAttachmentTransformRules KeepRelativeTransform;
	static FAttachmentTransformRules KeepWorldTransform;
	static FAttachmentTransformRules SnapToTargetNotIncludingScale;
	static FAttachmentTransformRules SnapToTargetIncludingScale;
	FAttachmentTransformRules(EAttachmentRule InRule, bool bInWeldSimulatedBodies);
	FAttachmentTransformRules(EAttachmentRule InLocationRule, EAttachmentRule InRotationRule, EAttachmentRule InScaleRule, bool bInWeldSimulatedBodies);
	EAttachmentRule LocationRule;
	EAttachmentRule RotationRule;
	EAttachmentRule ScaleRule;
	bool bWeldSimulatedBodies;
};
struct FDetachmentTransformRules
{
	static FDetachmentTransformRules KeepRelativeTransform;
	static FDetachmentTransformRules KeepWorldTransform;
	FDetachmentTransformRules(EDetachmentRule InRule, bool bInCallModify);
	FDetachmentTransformRules(EDetachmentRule InLocationRule, EDetachmentRule InRotationRule, EDetachmentRule InScaleRule, bool bInCallModify);
};
struct FCollisionResponseContainer
{
	UE_STUB_STRUCT_BODY(FCollisionResponseContainer)
	FCollisionResponseContainer();
	FCollisionResponseContainer(ECollisionResponse DefaultResponse);
	bool SetResponse(ECollisionChannel Channel, ECollisionResponse NewResponse);
	bool SetAllChannels(ECollisionResponse NewResponse);
	ECollisionResponse GetResponse(ECollisionChannel Channel) const;
};
struct FLightingChannels
{
	UE_STUB_STRUCT_BODY(FLightingChannels)
	FLightingChannels();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Channels)
	uint8 bChannel0 : 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Channels)
	uint8 bChannel1 : 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Channels)
	uint8 bChannel2 : 1;
};
