// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/Subsystems/WorldSubsystem.h (from UE 5.x source knowledge;
// reflected classes: checked against the 5.6 stub). UWorldSubsystem::GetWorld() is final; DoesSupportWorldType is
// protected; UTickableWorldSubsystem::GetStatId() is pure.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/Subsystem.h"
#include "Tickable.h"
#include "Engine/EngineBaseTypes.h"

class UWorld;

class UWorldSubsystem : public USubsystem
{
	UE_STUB_CLASS_BODY(UWorldSubsystem, USubsystem)
public:
	virtual UWorld* GetWorld() const override final;
	UWorld& GetWorldRef() const;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void PostInitialize() {}
	virtual void OnWorldBeginPlay(UWorld& InWorld) {}
	virtual void OnWorldComponentsUpdated(UWorld& World) {}
	virtual void UpdateStreamingState() {}
protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const;
};

class UTickableWorldSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	UE_STUB_CLASS_BODY(UTickableWorldSubsystem, UWorldSubsystem)
public:
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsAllowedToTick() const override final;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override PURE_VIRTUAL(UTickableWorldSubsystem::GetStatId, return TStatId();)
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void BeginDestroy() override;
	bool IsInitialized() const;
};
