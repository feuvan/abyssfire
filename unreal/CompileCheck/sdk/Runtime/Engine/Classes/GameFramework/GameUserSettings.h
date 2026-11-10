// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/GameFramework/GameUserSettings.h (UE 5.x source knowledge;
// UFUNCTION-marked members: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

UENUM(BlueprintType)
namespace EWindowMode
{
	enum Type : int
	{
		Fullscreen,
		WindowedFullscreen,
		Windowed,
		NumWindowModes UMETA(Hidden),
	};
}

class UGameUserSettings : public UObject
{
	UE_STUB_CLASS_BODY(UGameUserSettings, UObject)
public:
	UFUNCTION(BlueprintCallable, Category = Settings, meta = (bCheckForCommandLineOverrides = true))
	virtual void ApplySettings(bool bCheckForCommandLineOverrides);
	UFUNCTION(BlueprintCallable, Category = Settings)
	virtual void ApplyNonResolutionSettings();
	UFUNCTION(BlueprintCallable, Category = Settings)
	void ApplyResolutionSettings(bool bCheckForCommandLineOverrides);
	UFUNCTION(BlueprintPure, Category = Settings)
	FIntPoint GetScreenResolution() const;
	UFUNCTION(BlueprintPure, Category = Settings)
	FIntPoint GetLastConfirmedScreenResolution() const;
	UFUNCTION(BlueprintCallable, Category = Settings)
	FIntPoint GetDesktopResolution() const;
	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetScreenResolution(FIntPoint Resolution);
	UFUNCTION(BlueprintPure, Category = Settings)
	EWindowMode::Type GetFullscreenMode() const;
	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetFullscreenMode(EWindowMode::Type InFullscreenMode);
	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetVSyncEnabled(bool bEnable);
	UFUNCTION(BlueprintPure, Category = Settings)
	bool IsVSyncEnabled() const;
	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetFrameRateLimit(float NewLimit);
	UFUNCTION(BlueprintPure, Category = Settings)
	float GetFrameRateLimit() const;
	UFUNCTION(BlueprintCallable, Category = Settings)
	virtual void SetOverallScalabilityLevel(int32 Value);
	UFUNCTION(BlueprintCallable, Category = Settings)
	virtual int32 GetOverallScalabilityLevel() const;
	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetResolutionScaleNormalized(float NewScaleNormalized);
	UFUNCTION(BlueprintCallable, Category = Settings)
	virtual void SaveSettings();
	UFUNCTION(BlueprintCallable, Category = Settings)
	virtual void LoadSettings(bool bForceReload = false);
	UFUNCTION(BlueprintCallable, Category = Settings)
	virtual void SetToDefaults();
	UFUNCTION(BlueprintCallable, Category = Settings)
	virtual void RunHardwareBenchmark(int32 WorkScale = 10, float CPUMultiplier = 1.0f, float GPUMultiplier = 1.0f);
	UFUNCTION(BlueprintCallable, Category = Settings)
	virtual void ApplyHardwareBenchmarkResults();
	UFUNCTION(BlueprintCallable, Category = Settings)
	static UGameUserSettings* GetGameUserSettings();
};
