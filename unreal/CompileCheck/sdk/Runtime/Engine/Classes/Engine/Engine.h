// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/Engine.h (UE 5.x source knowledge; reflected class:
// checked against the 5.6 stub). UEngine + GEngine + EGetWorldErrorMode.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Misc/Exec.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "EngineGlobals.h"
#include "Subsystems/EngineSubsystem.h"

class UGameViewportClient;
class UGameUserSettings;
class ULocalPlayer;
class UFont;
class UTexture2D;
class FViewport;
class FAudioDeviceHandle;

enum class EGetWorldErrorMode
{
	ReturnNull,
	LogAndReturnNull,
	Assert,
};

struct FWorldContext
{
	UE_STUB_STRUCT_BODY(FWorldContext)
	TEnumAsByte<EWorldType::Type> WorldType;
	FName ContextHandle;
	TObjectPtr<UGameInstance> OwningGameInstance;
	TObjectPtr<UGameViewportClient> GameViewport;
	int32 PIEInstance;
	UWorld* World() const;
};

class UEngine : public UObject, public FExec
{
	UE_STUB_CLASS_BODY(UEngine, UObject)
public:
	UPROPERTY()
	TObjectPtr<UGameViewportClient> GameViewport;
	UPROPERTY(config, EditAnywhere, Category = Framerate)
	uint32 bSmoothFrameRate : 1;
	UPROPERTY(config, EditAnywhere, Category = Framerate)
	uint32 bUseFixedFrameRate : 1;
	UPROPERTY(config, EditAnywhere, Category = Framerate)
	float FixedFrameRate;
	UWorld* GetWorldFromContextObject(const UObject* Object, EGetWorldErrorMode ErrorMode) const;
	UWorld* GetWorldFromContextObjectChecked(const UObject* Object) const;
	UWorld* GetCurrentPlayWorld(UWorld* PossiblePlayWorld = nullptr) const;
	FWorldContext* GetWorldContextFromWorld(const UWorld* InWorld);
	UGameUserSettings* GetGameUserSettings();
	const UGameUserSettings* GetGameUserSettings() const;
	virtual float GetMaxFPS() const;
	virtual void SetMaxFPS(const float MaxFPS);
	virtual float GetMaxTickRate(float DeltaTime, bool bAllowFrameRateSmoothing = true) const;
	float GetTimeBetweenGarbageCollectionPasses() const;
	void ForceGarbageCollection(bool bFullPurge = false);
	void AddOnScreenDebugMessage(uint64 Key, float TimeToDisplay, FColor DisplayColor, const FString& DebugMessage, bool bNewerOnTop = true, const FVector2D& TextScale = FVector2D::UnitVector);
	void AddOnScreenDebugMessage(int32 Key, float TimeToDisplay, FColor DisplayColor, const FString& DebugMessage, bool bNewerOnTop = true, const FVector2D& TextScale = FVector2D::UnitVector);
	void ClearOnScreenDebugMessages();
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Out = *GLog) override;
	ULocalPlayer* GetFirstGamePlayer(UWorld* InWorld);
	ULocalPlayer* GetLocalPlayerFromControllerId(UWorld* InWorld, const int32 ControllerId) const;
	APlayerController* GetFirstLocalPlayerController(const UWorld* InWorld);
	bool IsEditor();
	bool IsInitialized() const;
	bool UseSound() const;
	FAudioDeviceHandle GetMainAudioDevice() const;
	FAudioDeviceHandle GetActiveAudioDevice();
	static UFont* GetTinyFont();
	static UFont* GetSmallFont();
	static UFont* GetMediumFont();
	static UFont* GetLargeFont();
	/** The engine: `return EngineSubsystemCollection.GetSubsystem<TSubsystemClass>(TSubsystemClass::StaticClass());`, whose
	 *  body static_asserts that TSubsystemClass derives from UEngineSubsystem. */
	template <typename TSubsystemClass>
	TSubsystemClass* GetEngineSubsystem() const
	{
		static_assert(std::is_base_of_v<UEngineSubsystem, TSubsystemClass>, "TSubsystemClass must be derived from TBaseType");
		(void)TSubsystemClass::StaticClass();
		return nullptr;
	}
};
