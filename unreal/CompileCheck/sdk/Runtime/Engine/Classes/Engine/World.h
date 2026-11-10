// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/World.h (UE 5.x source knowledge; reflected class:
// checked against the 5.6 stub). UWorld, FActorSpawnParameters, FWorldDelegates.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/ObjectPtr.h"
#include "Misc/Guid.h"
#include "Templates/Casts.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/HitResult.h"
#include "Subsystems/WorldSubsystem.h"
#include "Subsystems/SubsystemCollection.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"

class AActor;
class APawn;
class APlayerController;
class AController;
class AGameModeBase;
class AGameStateBase;
class AWorldSettings;
class ULevel;
class UGameViewportClient;
class ULocalPlayer;
class UNetDriver;
class UMaterialParameterCollection;
class UMaterialParameterCollectionInstance;
struct FCollisionQueryParams;
struct FCollisionResponseParams;
struct FCollisionObjectQueryParams;
struct FCollisionShape;
struct FLatentActionManager;

struct FActorSpawnParameters
{
	FActorSpawnParameters();
	FName Name;
	AActor* Template;
	AActor* Owner;
	APawn* Instigator;
	ULevel* OverrideLevel;
	ESpawnActorCollisionHandlingMethod SpawnCollisionHandlingOverride;
	uint8 bNoFail : 1;
	uint8 bDeferConstruction : 1;
	uint8 bAllowDuringConstructionScript : 1;
#if WITH_EDITOR
	uint8 bTemporaryEditorActor : 1;
	uint8 bHideFromSceneOutliner : 1;
	uint8 bCreateActorPackage : 1;
#endif
	enum class ESpawnActorNameMode : uint8 { Required_Fatal, Required_ErrorAndReturnNull, Required_ReturnNull, Requested };
	ESpawnActorNameMode NameMode;
	EObjectFlags ObjectFlags;
	TFunction<void(AActor*)> CustomPreSpawnInitalization;
};

class UWorld final : public UObject
{
	UE_STUB_CLASS_BODY(UWorld, UObject)
public:
	UPROPERTY(Transient)
	TObjectPtr<ULevel> PersistentLevel;
	TEnumAsByte<EWorldType::Type> WorldType;
	uint8 bIsTearingDown : 1;
	uint8 bBegunPlay : 1;
	uint8 bIsWorldInitialized : 1;
	uint8 bMatchStarted : 1;
	double TimeSeconds;
	double RealTimeSeconds;
	float DeltaTimeSeconds;
	float DeltaRealTimeSeconds;

	double GetTimeSeconds() const;
	double GetUnpausedTimeSeconds() const;
	double GetRealTimeSeconds() const;
	double GetAudioTimeSeconds() const;
	float GetDeltaSeconds() const;
	float TimeSince(double Time) const;
	bool IsPaused() const;
	bool IsGameWorld() const;
	bool IsEditorWorld() const;
	bool IsPreviewWorld() const;
	bool IsPlayInEditor() const;
	bool IsInitialized() const;
	bool HasBegunPlay() const;
	bool AreActorsInitialized() const;
	ENetMode GetNetMode() const;
	virtual UWorld* GetWorld() const override;
	UGameInstance* GetGameInstance() const;
	template <class T> T* GetGameInstance() const { return Cast<T>(GetGameInstance()); }
	template <class T> T* GetGameInstanceChecked() const { return CastChecked<T>(GetGameInstance()); }
	void SetGameInstance(UGameInstance* NewGI);
	FTimerManager& GetTimerManager() const;
	FLatentActionManager& GetLatentActionManager();
	AGameModeBase* GetAuthGameMode() const;
	template <class T> T* GetAuthGameMode() const { return Cast<T>(GetAuthGameMode()); }
	AGameStateBase* GetGameState() const;
	template <class T> T* GetGameState() const { return Cast<T>(GetGameState()); }
	AWorldSettings* GetWorldSettings(bool bCheckStreamingPersistent = false, bool bChecked = true) const;
	UGameViewportClient* GetGameViewport() const;
	APlayerController* GetFirstPlayerController() const;
	template <class T> T* GetFirstPlayerController() const { return Cast<T>(GetFirstPlayerController()); }
	ULocalPlayer* GetFirstLocalPlayerFromController() const;
	template <class T> T* GetFirstLocalPlayerFromController() const { return Cast<T>(GetFirstLocalPlayerFromController()); }
	int32 GetNumPlayerControllers() const;
	ULevel* GetCurrentLevel() const;
	const TArray<ULevel*>& GetLevels() const;
	FString GetMapName() const;
	const FString GetLocalURL() const;

	AActor* SpawnActor(UClass* InClass, FVector const* Location = nullptr, FRotator const* Rotation = nullptr, const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters());
	AActor* SpawnActor(UClass* Class, FTransform const* Transform, const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters());
	AActor* SpawnActorAbsolute(UClass* Class, FTransform const& AbsoluteTransform, const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters());
	template <class T> requires std::is_base_of_v<AActor, T>
	T* SpawnActor(const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters()) { return CastChecked<T>(SpawnActor(T::StaticClass(), nullptr, nullptr, SpawnParameters), ECastCheckedType::NullAllowed); }
	template <class T> requires std::is_base_of_v<AActor, T>
	T* SpawnActor(FVector const& Location, FRotator const& Rotation, const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters()) { return CastChecked<T>(SpawnActor(T::StaticClass(), &Location, &Rotation, SpawnParameters), ECastCheckedType::NullAllowed); }
	template <class T> requires std::is_base_of_v<AActor, T>
	T* SpawnActor(UClass* Class, const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters()) { return CastChecked<T>(SpawnActor(Class, nullptr, nullptr, SpawnParameters), ECastCheckedType::NullAllowed); }
	template <class T> requires std::is_base_of_v<AActor, T>
	T* SpawnActor(UClass* Class, FVector const& Location, FRotator const& Rotation, const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters()) { return CastChecked<T>(SpawnActor(Class, &Location, &Rotation, SpawnParameters), ECastCheckedType::NullAllowed); }
	template <class T> requires std::is_base_of_v<AActor, T>
	T* SpawnActor(UClass* Class, FTransform const& Transform, const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters()) { return CastChecked<T>(SpawnActor(Class, &Transform, SpawnParameters), ECastCheckedType::NullAllowed); }
	template <class T> requires std::is_base_of_v<AActor, T>
	T* SpawnActorDeferred(UClass* Class, FTransform const& Transform, AActor* Owner = nullptr, APawn* Instigator = nullptr,
		ESpawnActorCollisionHandlingMethod CollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::Undefined) UE_STUB_BODY
	bool DestroyActor(AActor* Actor, bool bNetForce = false, bool bShouldModifyLevel = true);

	bool LineTraceSingleByChannel(struct FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam) const;
	bool LineTraceSingleByChannel(struct FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel) const;
	bool LineTraceSingleByChannel(struct FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params) const;

	template <typename TSubsystemClass>
	TSubsystemClass* GetSubsystem() const
	{
		static_assert(std::is_base_of_v<UWorldSubsystem, TSubsystemClass>, "UWorld::GetSubsystem<T>: T must derive from UWorldSubsystem");
		return nullptr;
	}
	template <typename TSubsystemClass>
	static TSubsystemClass* GetSubsystem(const UWorld* World)
	{
		static_assert(std::is_base_of_v<UWorldSubsystem, TSubsystemClass>, "UWorld::GetSubsystem<T>: T must derive from UWorldSubsystem");
		return nullptr;
	}
	template <typename TSubsystemClass> bool HasSubsystem() const { return GetSubsystem<TSubsystemClass>() != nullptr; }
	/** Gets this world's instance for a given collection. */
	UMaterialParameterCollectionInstance* GetParameterCollectionInstance(const UMaterialParameterCollection* Collection) const;
	void ForceGarbageCollection(bool bFullPurge = false);
	bool ServerTravel(const FString& InURL, bool bAbsolute = false, bool bShouldSkipGameNotify = false);
};

class FWorldDelegates
{
public:
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FWorldCleanupEvent, UWorld*, bool, bool);
	DECLARE_MULTICAST_DELEGATE_OneParam(FWorldEvent, UWorld*);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLevelChanged, ULevel*, UWorld*);
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnWorldTickStart, UWorld*, ELevelTick, float);
	static FOnWorldTickStart OnWorldTickStart;
	static FWorldEvent OnPostWorldCreation;
	static FWorldCleanupEvent OnWorldCleanup;
	static FWorldCleanupEvent OnPostWorldCleanup;
	static FWorldEvent OnPreWorldFinishDestroy;
	static FWorldEvent OnWorldBeginTearDown;
	static FOnLevelChanged LevelAddedToWorld;
	static FOnLevelChanged LevelRemovedFromWorld;
};
