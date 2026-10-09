#include "Platform/AbyssPlatform.h"

#include "Abyssfire.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "GenericPlatform/GenericPlatformApplicationMisc.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// Project console variables (ue58-platform.md 6.8, 14). Device profiles may set them before this module registers
// them (+CVars=abyss.Quality=0 in DefaultDeviceProfiles.ini): the console manager keeps such values.
static TAutoConsoleVariable<int32> CVarAbyssQuality(
	TEXT("abyss.Quality"),
	-1,
	TEXT("Render quality tier override: -1 = settings / automatic, 0 = low, 1 = balanced, 2 = high."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarAbyssRenderScale(
	TEXT("abyss.RenderScale"),
	0.0f,
	TEXT("3D render scale override (web ?res=): 0 = automatic per quality tier, else 1 / 1.5 / 2."),
	ECVF_Default);

namespace
{
	bool AbyssPlatform_ParseQualityTier(const FString& Text, EAbyssQualityTier& Out)
	{
		if (Text == TEXT("0") || Text.Equals(TEXT("low"), ESearchCase::IgnoreCase))
		{
			Out = EAbyssQualityTier::Low;
			return true;
		}
		if (Text == TEXT("1") || Text.Equals(TEXT("balanced"), ESearchCase::IgnoreCase)
			|| Text.Equals(TEXT("mid"), ESearchCase::IgnoreCase))
		{
			Out = EAbyssQualityTier::Balanced;
			return true;
		}
		if (Text == TEXT("2") || Text.Equals(TEXT("high"), ESearchCase::IgnoreCase))
		{
			Out = EAbyssQualityTier::High;
			return true;
		}
		return false;
	}
}

namespace AbyssPlatform
{
	bool IsMobilePlatform()
	{
#if PLATFORM_ANDROID || PLATFORM_IOS
		return true;
#else
		return false;
#endif
	}

	bool ResolveTouchMode(EAbyssControlLayout Layout)
	{
		switch (Layout)
		{
		case EAbyssControlLayout::Desktop:
			return false;
		case EAbyssControlLayout::Touch:
			return true;
		case EAbyssControlLayout::Auto:
			break;
		}
		if (IsMobilePlatform())
		{
			return true;
		}
		const TCHAR* CommandLine = FCommandLine::Get();
		return FParse::Param(CommandLine, TEXT("faketouches")) || FParse::Param(CommandLine, TEXT("abysstouch"));
	}

	EAbyssQualityTier ResolveQualityTier(EAbyssQualitySetting Setting)
	{
		switch (Setting)
		{
		case EAbyssQualitySetting::Low: return EAbyssQualityTier::Low;
		case EAbyssQualitySetting::Balanced: return EAbyssQualityTier::Balanced;
		case EAbyssQualitySetting::High: return EAbyssQualityTier::High;
		case EAbyssQualitySetting::Auto: break;
		}

		// Command line (testing) beats the console variable (device profiles / console).
		FString CommandLineValue;
		EAbyssQualityTier Parsed = EAbyssQualityTier::Balanced;
		if (FParse::Value(FCommandLine::Get(), TEXT("abyssquality="), CommandLineValue) && AbyssPlatform_ParseQualityTier(CommandLineValue, Parsed))
		{
			return Parsed;
		}
		const int32 CVarValue = CVarAbyssQuality.GetValueOnGameThread();
		if (CVarValue >= 0)
		{
			return CVarValue == 0 ? EAbyssQualityTier::Low : (CVarValue == 1 ? EAbyssQualityTier::Balanced : EAbyssQualityTier::High);
		}
		if (IsMobilePlatform())
		{
			// Mobile tiers come from the device profile (abyss.Quality); unknown devices get balanced.
			return EAbyssQualityTier::Balanced;
		}

		// Web rule (src/rendering/RenderQuality.ts:50-62) on the default window (physical pixels).
		const int32 Cores = FPlatformMisc::NumberOfCores();
		const uint32 MemoryGB = FPlatformMemory::GetConstants().TotalPhysicalGB;
		if (Cores <= 4 || MemoryGB <= 4)
		{
			return EAbyssQualityTier::Low;
		}
		const float DpiScale = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(0.0f, 0.0f);
		FIntPoint Resolution(1280, 720);
		if (const UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
		{
			const FIntPoint Configured = UserSettings->GetScreenResolution();
			if (Configured.X > 0 && Configured.Y > 0)
			{
				Resolution = Configured;
			}
		}
		const double PixelBudget = static_cast<double>(Resolution.X) * static_cast<double>(Resolution.Y)
			* static_cast<double>(DpiScale) * static_cast<double>(DpiScale);
		if (PixelBudget > 5000000.0)
		{
			return EAbyssQualityTier::Low;
		}
		if (DpiScale >= 2.0f && PixelBudget <= 3700000.0)
		{
			return EAbyssQualityTier::High;
		}
		return EAbyssQualityTier::Balanced;
	}

	float GetRenderScaleOverride()
	{
		return FMath::Max(0.0f, CVarAbyssRenderScale.GetValueOnGameThread());
	}

	void SetScreenSaverAllowed(bool bAllowed)
	{
		FPlatformApplicationMisc::ControlScreensaver(bAllowed ? FGenericPlatformApplicationMisc::EScreenSaverAction::Enable
															  : FGenericPlatformApplicationMisc::EScreenSaverAction::Disable);
	}
}
