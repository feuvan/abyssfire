// Stub SDK. Real headers: Engine/Source/Runtime/Core/Public/HAL/PlatformProperties.h and
// GenericPlatform/GenericPlatformProperties.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"

struct FGenericPlatformProperties
{
	static const char* GetPhysicsFormat();
	static bool HasEditorOnlyData();
	static const char* IniPlatformName();
	static bool IsGameOnly();
	static bool IsServerOnly();
	static bool IsClientOnly();
	static bool IsMonolithicBuild();
	static bool IsProgram();
	static bool IsLittleEndian();
	static const char* PlatformName();
	static bool RequiresCookedData();
	static bool HasSecurePackageFormat();
	static bool RequiresUserCredentials();
	static bool SupportsAutoSDK();
	static bool SupportsGrayscaleSRGB();
	static bool SupportsMultipleGameInstances();
	static bool SupportsWindowedMode();
	static bool AllowsFramerateSmoothing();
	static bool SupportsAudioStreaming();
	static bool SupportsHighQualityLightmaps();
	static bool SupportsLowQualityLightmaps();
	static bool SupportsTextureStreaming();
	static bool SupportsMemoryMappedFiles();
	static bool SupportsVirtualTextureStreaming();
	static bool SupportsLumenGI();
	static bool SupportsHardwareLZDecompression();
	static bool HasFixedResolution();
	static bool SupportsMinimize();
	static bool SupportsQuit();
	static bool AllowsCallStackDumpDuringAssert();
	static const char* GetZlibReplacementFormat();
};
struct FPlatformProperties : public FGenericPlatformProperties
{
};
