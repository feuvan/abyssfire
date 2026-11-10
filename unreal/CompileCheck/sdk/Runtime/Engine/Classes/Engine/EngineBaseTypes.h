// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/EngineBaseTypes.h (from UE 5.x source knowledge;
// reflected enums / structs: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Templates/Casts.h"
#include "Templates/SubclassOf.h"
#include "Misc/CoreDelegates.h"

class UWorld;
class ULevel;
class AActor;

UENUM(BlueprintType)
enum ETickingGroup : int
{
	TG_PrePhysics,
	TG_StartPhysics,
	TG_DuringPhysics,
	TG_EndPhysics,
	TG_PostPhysics,
	TG_PostUpdateWork,
	TG_LastDemotable,
	TG_NewlySpawned,
	TG_MAX,
};
enum ELevelTick : int
{
	LEVELTICK_TimeOnly = 0,
	LEVELTICK_ViewportsOnly = 1,
	LEVELTICK_All = 2,
	LEVELTICK_PauseTick = 3,
};
enum ENetMode { NM_Standalone, NM_DedicatedServer, NM_ListenServer, NM_Client, NM_MAX };
UENUM()
enum EInputEvent : int
{
	IE_Pressed = 0,
	IE_Released = 1,
	IE_Repeat = 2,
	IE_DoubleClick = 3,
	IE_Axis = 4,
	IE_MAX = 5,
};
namespace EWorldType
{
	enum Type : int { None, Game, Editor, PIE, EditorPreview, GamePreview, GameRPC, Inactive };
}
const TCHAR* LexToString(const EWorldType::Type Value);
enum class ETeleportType : uint8 { None, TeleportPhysics, ResetPhysics };
enum class EUpdateTransformFlags : int32 { None = 0x0, SkipPhysicsUpdate = 0x1, PropagateFromParent = 0x2, OnlyUpdateIfUsingSocket = 0x4 };
ENUM_CLASS_FLAGS(EUpdateTransformFlags);

struct FTickFunction
{
	UE_STUB_STRUCT_BODY(FTickFunction)
	UPROPERTY(EditDefaultsOnly, Category = "Tick", AdvancedDisplay)
	TEnumAsByte<enum ETickingGroup> TickGroup;
	TEnumAsByte<enum ETickingGroup> EndTickGroup;
	UPROPERTY(EditDefaultsOnly, Category = "Tick", AdvancedDisplay)
	uint8 bTickEvenWhenPaused : 1;
	UPROPERTY()
	uint8 bCanEverTick : 1;
	UPROPERTY(EditDefaultsOnly, Category = "Tick")
	uint8 bStartWithTickEnabled : 1;
	UPROPERTY(EditDefaultsOnly, Category = "Tick", AdvancedDisplay)
	uint8 bAllowTickOnDedicatedServer : 1;
	uint8 bHighPriority : 1;
	uint8 bRunOnAnyThread : 1;
	UPROPERTY(EditDefaultsOnly, Category = "Tick", meta = (DisplayName = "Tick Interval (secs)"))
	float TickInterval;
	FTickFunction();
	virtual ~FTickFunction();
	void RegisterTickFunction(ULevel* Level);
	void UnRegisterTickFunction();
	bool IsTickFunctionRegistered() const;
	void SetTickFunctionEnable(bool bInEnabled);
	bool IsTickFunctionEnabled() const;
	void AddPrerequisite(UObject* TargetObject, struct FTickFunction& TargetTickFunction);
	void RemovePrerequisite(UObject* TargetObject, struct FTickFunction& TargetTickFunction);
	float GetLastTickGameTime() const;
};
struct FActorTickFunction : public FTickFunction
{
	UE_STUB_STRUCT_BODY(FActorTickFunction)
	AActor* Target;
};
struct FActorComponentTickFunction : public FTickFunction
{
	UE_STUB_STRUCT_BODY(FActorComponentTickFunction)
	class UActorComponent* Target;
};

UENUM(BlueprintType)
enum class EMouseCaptureMode : uint8
{
	NoCapture,
	CapturePermanently,
	CapturePermanently_IncludingInitialMouseDown,
	CaptureDuringMouseDown,
	CaptureDuringRightMouseDown,
};

UENUM(BlueprintType)
enum class EMouseLockMode : uint8
{
	DoNotLock,
	LockOnCapture,
	LockAlways,
	LockInFullscreen,
};
