// Stub SDK. Real header: Engine/Source/Runtime/ApplicationCore/Public/GenericPlatform/GenericPlatformApplicationMisc.h
// (UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"

enum class EScreenPhysicalAccuracy
{
	Unknown,
	Approximation,
	Truth,
};

struct FGenericPlatformApplicationMisc
{
	static void PreInit();
	static void Init();
	static void PostInit();
	static void TearDown();
	static bool IsHighDPIAwarenessEnabled();
	static bool IsThisApplicationForeground();
	static void RequestMinimize();
	static bool RequiresVirtualKeyboard();
	static void PumpMessages(bool bFromMainLoop);
	static void PreventScreenSaver();
	enum EScreenSaverAction
	{
		Disable,
		Enable,
	};
	static bool IsScreensaverEnabled();
	static bool ControlScreensaver(EScreenSaverAction Action);
	static struct FLinearColor GetScreenPixelColor(const FVector2D& InScreenPos, float InGamma = 1.0f);
	static bool GetWindowTitleMatchingText(const TCHAR* TitleStartsWith, FString& OutTitle);
	static void SetHighDPIMode();
	static float GetDPIScaleFactorAtPoint(float X, float Y);
	static bool IsGamepadAttached();
	static bool IsMouseAttached();
	static void ClipboardCopy(const TCHAR* Str);
	static void ClipboardPaste(class FString& Dest);
	static EScreenPhysicalAccuracy GetPhysicalScreenDensity(int32& OutScreenDensity);
	static EScreenPhysicalAccuracy ComputePhysicalScreenDensity(int32& OutScreenDensity);
	static EScreenPhysicalAccuracy ConvertInchesToPixels(float Inches, float& OutPixels);
	static EScreenPhysicalAccuracy ConvertPixelsToInches(float Pixels, float& OutInches);
};
