// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/Tickable.h (from UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"

class UWorld;
enum class ETickableTickType : uint8 { Conditional, Always, Never, NewObject };

class FTickableObjectBase
{
public:
	virtual ~FTickableObjectBase() {}
	virtual void Tick(float DeltaTime) = 0;
	virtual ETickableTickType GetTickableTickType() const { return ETickableTickType::Conditional; }
	virtual bool IsTickable() const { return true; }
	virtual bool IsAllowedToTick() const { return true; }
	virtual void SetTickableTickType(ETickableTickType NewTickType);
	virtual TStatId GetStatId() const = 0;
};
class FTickableGameObject : public FTickableObjectBase
{
public:
	FTickableGameObject();
	virtual ~FTickableGameObject();
	virtual bool IsTickableWhenPaused() const { return false; }
	virtual bool IsTickableInEditor() const { return false; }
	virtual UWorld* GetTickableGameObjectWorld() const { return nullptr; }
	static void TickObjects(UWorld* World, int32 TickType, bool bIsPaused, float DeltaSeconds);
};
