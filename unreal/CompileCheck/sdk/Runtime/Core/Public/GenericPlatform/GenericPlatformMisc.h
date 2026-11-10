// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/GenericPlatform/GenericPlatformMisc.h
// (from UE 5.x source knowledge). FPlatformMisc derives from this per platform (HAL/PlatformMisc.h).
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Misc/Guid.h"
#include "Misc/VarArgs.h"

enum class EBuildConfiguration : uint8 { Unknown, Debug, DebugGame, Development, Shipping, Test };
enum class EBuildTargetType : uint8 { Unknown, Game, Server, Client, Editor, Program };
namespace EAppReturnType { enum Type { No, Yes, YesAll, NoAll, Cancel, Ok, Retry, Continue }; }
namespace EAppMsgType { enum Type { Ok, YesNo, OkCancel, YesNoCancel, CancelRetryContinue, YesNoYesAllNoAll, YesNoYesAllNoAllCancel, YesNoYesAll }; }
enum class ENetworkConnectionType : uint8 { Unknown, None, AirplaneMode, Cell, WiFi, WiMAX, Bluetooth, Ethernet };
enum class EDeviceScreenOrientation : uint8 { Unknown, Portrait, PortraitUpsideDown, LandscapeLeft, LandscapeRight, FaceUp, FaceDown, PortraitSensor, LandscapeSensor, FullSensor };
enum class EMobileHapticsType : uint8 { FeedbackSuccess, FeedbackWarning, FeedbackError, SelectionChanged, ImpactLight, ImpactMedium, ImpactHeavy };

struct FGenericPlatformMisc
{
	static void PlatformPreInit();
	static void PlatformInit();
	static void LowLevelOutputDebugString(const TCHAR* Message);
	template <typename... Types>
	static void LowLevelOutputDebugStringf(UE::Core::TCheckedFormatString<TCHAR, Types...> Fmt, Types... Args) UE_STUB_BODY
	static void LocalPrint(const TCHAR* Str);
	static bool IsDebuggerPresent();
	static FString GetEnvironmentVariable(const TCHAR* VariableName);
	static void SetEnvironmentVar(const TCHAR* VariableName, const TCHAR* Value);
	static const TCHAR* GetPathVarDelimiter();
	static FString GetDeviceId();
	static FString GetUniqueAdvertisingId();
	static FString GetLoginId();
	static FString GetEpicAccountId();
	static FString GetDeviceMakeAndModel();
	static FString GetCPUVendor();
	static FString GetCPUBrand();
	static FString GetPrimaryGPUBrand();
	static FString GetOSVersion();
	static void GetOSVersions(FString& out_OSVersionLabel, FString& out_OSSubVersionLabel);
	static FString GetDefaultLanguage();
	static FString GetDefaultLocale();
	static FString GetTimeZoneId();
	static void RequestExit(bool Force, const TCHAR* CallSite = nullptr);
	static void RequestExitWithStatus(bool Force, uint8 ReturnCode, const TCHAR* CallSite = nullptr);
	static bool RestartApplication();
	static const TCHAR* GetSystemErrorMessage(TCHAR* OutBuffer, int32 BufferCount, int32 Error);
	static void CreateGuid(FGuid& Result);
	static EAppReturnType::Type MessageBoxExt(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption);
	static bool IsRunningOnBattery();
	static EDeviceScreenOrientation GetDeviceOrientation();
	static void SetDeviceOrientation(EDeviceScreenOrientation NewDeviceOrientation);
	static int32 GetDeviceVolume();
	static int32 NumberOfCores();
	static int32 NumberOfCoresIncludingHyperthreads();
	static int32 NumberOfWorkerThreadsToSpawn();
	static int32 NumberOfIOWorkerThreadsToSpawn();
	static const TCHAR* GetPlatformFeaturesModuleName();
	static const TCHAR* RootDir();
	static const TCHAR* EngineDir();
	static const TCHAR* ProjectDir();
	static FString CloudDir();
	static const TCHAR* GamePersistentDownloadDir();
	static const TCHAR* GeneratedConfigDir();
	static const TCHAR* GetUBTPlatform();
	static const TCHAR* GetUBTTarget();
	static const TCHAR* GetUBTTargetName();
	static const TCHAR* GetDefaultDeviceProfileName();
	static int GetBatteryLevel();
	static float GetDeviceTemperatureLevel();
	static bool IsInLowPowerMode();
	static bool GetDiskTotalAndFreeSpace(const FString& InPath, uint64& TotalNumberOfBytes, uint64& NumberOfFreeBytes);
	static bool SupportsTouchInput();
	static bool SupportsForceTouchInput();
	static bool HasActiveWiFiConnection();
	static ENetworkConnectionType GetNetworkConnectionType();
	static bool HasPlatformFeature(const TCHAR* FeatureName);
	static bool IsRunningInCloud();
	static bool AllowAudioThread();
	static bool AllowRenderThread();
	static bool UseRenderThread();
	static bool DesktopTouchScreen();
	static bool FullscreenSameAsWindowedFullscreen();
	static bool SupportsBackbufferSampling();
	static bool NeedsNonoptionalCPUFeaturesCheck();
	static void PrepareMobileHaptics(EMobileHapticsType Type);
	static void TriggerMobileHaptics();
	static void ReleaseMobileHaptics();
	static void ShareURL(const FString& URL, const FText& Description, int32 LocationHintX, int32 LocationHintY);
	static FString GetOperatingSystemId();
	static bool SetStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName, const FString& InValue);
	static bool GetStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName, FString& OutValue);
	static bool DeleteStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName);
};
