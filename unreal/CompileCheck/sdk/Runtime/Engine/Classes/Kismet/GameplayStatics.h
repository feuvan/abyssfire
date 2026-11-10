// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Kismet/GameplayStatics.h (UE 5.x source knowledge;
// UFUNCTION-marked members: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/EngineTypes.h"
#include "Templates/SubclassOf.h"

class AActor;
class APawn;
class APlayerController;
class APlayerCameraManager;
class UAudioComponent;
class UGameInstance;
class USceneComponent;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;
class UInitialActiveSoundParams;

class UGameplayStatics : public UBlueprintFunctionLibrary
{
	UE_STUB_CLASS_BODY(UGameplayStatics, UBlueprintFunctionLibrary)
public:
	UFUNCTION(BlueprintPure, Category = "Game", meta = (WorldContext = "WorldContextObject"))
	static class UGameInstance* GetGameInstance(const UObject* WorldContextObject);
	UFUNCTION(BlueprintPure, Category = "Game", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static class APlayerController* GetPlayerController(const UObject* WorldContextObject, int32 PlayerIndex);
	UFUNCTION(BlueprintPure, Category = "Game", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static class APawn* GetPlayerPawn(const UObject* WorldContextObject, int32 PlayerIndex);
	UFUNCTION(BlueprintPure, Category = "Game", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static class APlayerCameraManager* GetPlayerCameraManager(const UObject* WorldContextObject, int32 PlayerIndex);
	UFUNCTION(BlueprintCallable, Category = "Actor", meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "ActorClass"))
	static class AActor* GetActorOfClass(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass);
	UFUNCTION(BlueprintCallable, Category = "Actor", meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "ActorClass", DynamicOutputParam = "OutActors"))
	static void GetAllActorsOfClass(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, TArray<AActor*>& OutActors);
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "2", DisplayName = "Open Level (by Name)"), Category = "Game")
	static void OpenLevel(const UObject* WorldContextObject, FName LevelName, bool bAbsolute = true, FString Options = FString(TEXT("")));
	UFUNCTION(BlueprintPure, Category = "Utilities|Time", meta = (WorldContext = "WorldContextObject"))
	static double GetWorldDeltaSeconds(const UObject* WorldContextObject);
	UFUNCTION(BlueprintPure, Category = "Utilities|Time", meta = (WorldContext = "WorldContextObject"))
	static double GetTimeSeconds(const UObject* WorldContextObject);
	UFUNCTION(BlueprintPure, Category = "Utilities|Time", meta = (WorldContext = "WorldContextObject"))
	static double GetRealTimeSeconds(const UObject* WorldContextObject);
	UFUNCTION(BlueprintPure, Category = "Utilities|Time", meta = (WorldContext = "WorldContextObject"))
	static float GetGlobalTimeDilation(const UObject* WorldContextObject);
	UFUNCTION(BlueprintCallable, Category = "Utilities|Time", meta = (WorldContext = "WorldContextObject"))
	static void SetGlobalTimeDilation(const UObject* WorldContextObject, float TimeDilation);
	UFUNCTION(BlueprintCallable, Category = "Game", meta = (WorldContext = "WorldContextObject"))
	static bool SetGamePaused(const UObject* WorldContextObject, bool bPaused);
	UFUNCTION(BlueprintPure, Category = "Game", meta = (WorldContext = "WorldContextObject"))
	static bool IsGamePaused(const UObject* WorldContextObject);
	UFUNCTION(BlueprintPure, Category = "Game")
	static FString GetPlatformName();
	UFUNCTION(BlueprintCallable, Category = "Audio", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "2", UnsafeDuringActorConstruction = "true"))
	static void PlaySound2D(const UObject* WorldContextObject, USoundBase* Sound, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f, USoundConcurrency* ConcurrencySettings = nullptr, const AActor* OwningActor = nullptr, bool bIsUISound = true);
	UFUNCTION(BlueprintCallable, Category = "Audio", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "2", UnsafeDuringActorConstruction = "true", Keywords = "play"))
	static UAudioComponent* SpawnSound2D(const UObject* WorldContextObject, USoundBase* Sound, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f, USoundConcurrency* ConcurrencySettings = nullptr, bool bPersistAcrossLevelTransition = false, bool bAutoDestroy = true);
	UFUNCTION(BlueprintCallable, Category = "Audio", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "2", UnsafeDuringActorConstruction = "true", Keywords = "play"))
	static UAudioComponent* CreateSound2D(const UObject* WorldContextObject, USoundBase* Sound, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f, USoundConcurrency* ConcurrencySettings = nullptr, bool bPersistAcrossLevelTransition = false, bool bAutoDestroy = true);
	UFUNCTION(BlueprintCallable, Category = "Audio", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "3", UnsafeDuringActorConstruction = "true", Keywords = "play"))
	static void PlaySoundAtLocation(const UObject* WorldContextObject, USoundBase* Sound, FVector Location, FRotator Rotation, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f, class USoundAttenuation* AttenuationSettings = nullptr, USoundConcurrency* ConcurrencySettings = nullptr, const AActor* OwningActor = nullptr, const UInitialActiveSoundParams* InitialParams = nullptr);
	static void PlaySoundAtLocation(const UObject* WorldContextObject, USoundBase* Sound, FVector Location, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f, class USoundAttenuation* AttenuationSettings = nullptr, USoundConcurrency* ConcurrencySettings = nullptr, const UInitialActiveSoundParams* InitialParams = nullptr);
	UFUNCTION(BlueprintCallable, Category = "Audio", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "3", UnsafeDuringActorConstruction = "true", Keywords = "play"))
	static UAudioComponent* SpawnSoundAtLocation(const UObject* WorldContextObject, USoundBase* Sound, FVector Location, FRotator Rotation = FRotator::ZeroRotator, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f, class USoundAttenuation* AttenuationSettings = nullptr, USoundConcurrency* ConcurrencySettings = nullptr, bool bAutoDestroy = true);
	UFUNCTION(BlueprintPure, Category = "Camera", meta = (Keywords = "unproject"))
	static bool DeprojectScreenToWorld(APlayerController const* Player, const FVector2D& ScreenPosition, FVector& WorldPosition, FVector& WorldDirection);
	UFUNCTION(BlueprintPure, Category = "Camera")
	static bool ProjectWorldToScreen(APlayerController const* Player, const FVector& WorldPosition, FVector2D& ScreenPosition, bool bPlayerViewportRelative = false);
};
