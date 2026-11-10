// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/GameInstance.h (UE 5.x source knowledge; reflected
// class: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/SubsystemCollection.h"
#include "Engine/EngineBaseTypes.h"

class UWorld;
class UEngine;
class ULocalPlayer;
class APlayerController;
class UGameViewportClient;
class FTimerManager;
struct FWorldContext;
struct FLatentActionManager;

class UGameInstance : public UObject
{
	UE_STUB_CLASS_BODY(UGameInstance, UObject)
public:
	virtual UWorld* GetWorld() const final;
	UEngine* GetEngine() const;
	FWorldContext* GetWorldContext() const;
	UGameViewportClient* GetGameViewportClient() const;
	virtual void Init();
	virtual void Shutdown();
	virtual void OnStart();
	virtual void StartGameInstance();
	virtual void OnWorldChanged(UWorld* OldWorld, UWorld* NewWorld) {}
	virtual void LoadComplete(const float LoadTime, const FString& MapName) {}
	ULocalPlayer* GetFirstGamePlayer() const;
	ULocalPlayer* GetLocalPlayerByIndex(const int32 Index) const;
	int32 GetNumLocalPlayers() const;
	const TArray<ULocalPlayer*>& GetLocalPlayers() const;
	APlayerController* GetFirstLocalPlayerController(const UWorld* World = nullptr) const;
	APlayerController* GetPrimaryPlayerController(bool bRequiresValidUniqueId = true) const;
	FTimerManager& GetTimerManager() const;
	FLatentActionManager& GetLatentActionManager() const;
	template <typename TSubsystemClass>
	TSubsystemClass* GetSubsystem() const
	{
		static_assert(std::is_base_of_v<UGameInstanceSubsystem, TSubsystemClass>, "UGameInstance::GetSubsystem<T>: T must derive from UGameInstanceSubsystem");
		return nullptr;
	}
	template <typename TSubsystemClass>
	static TSubsystemClass* GetSubsystem(const UGameInstance* GameInstance)
	{
		static_assert(std::is_base_of_v<UGameInstanceSubsystem, TSubsystemClass>, "UGameInstance::GetSubsystem<T>: T must derive from UGameInstanceSubsystem");
		return nullptr;
	}
	template <typename TSubsystemClass> const TArray<TSubsystemClass*>& GetSubsystemArray() const UE_STUB_BODY
	virtual void ReturnToMainMenu();
protected:
	UPROPERTY()
	TArray<TObjectPtr<ULocalPlayer>> LocalPlayers;
};
