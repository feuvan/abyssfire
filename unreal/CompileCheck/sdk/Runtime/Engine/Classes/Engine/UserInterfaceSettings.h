// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/UserInterfaceSettings.h (UE 5.x source knowledge;
// reflected class: checked against the 5.6 stub). The project's UI DPI scaling rule.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

UENUM()
enum class EUIScalingRule : uint8
{
	ShortestSide,
	LongestSide,
	Horizontal,
	Vertical,
	ScaleToFit,
	Custom,
};

class UUserInterfaceSettings : public UDeveloperSettings
{
	UE_STUB_CLASS_BODY(UUserInterfaceSettings, UDeveloperSettings)
public:
	UPROPERTY(config, EditAnywhere, Category = "DPI Scaling")
	EUIScalingRule UIScaleRule;
	UPROPERTY(config, EditAnywhere, Category = "DPI Scaling")
	float ApplicationScale;
	UPROPERTY(config, EditAnywhere, Category = "DPI Scaling")
	bool bAllowHighDPIInGameMode;
	float GetDPIScaleBasedOnSize(FIntPoint Size) const;
};
