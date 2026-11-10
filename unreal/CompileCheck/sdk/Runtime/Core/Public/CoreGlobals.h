// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/CoreGlobals.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Logging/LogMacros.h"
#include "UObject/NameTypes.h"

class FConfigCacheIni;
class FOutputDeviceRedirector;
class FOutputDeviceConsole;
class FFeedbackContext;
class FOutputDeviceError;

DECLARE_LOG_CATEGORY_EXTERN(LogHAL, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogSerialization, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogUnrealMath, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMemory, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogProfilingDebugging, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogCore, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogOutputDevice, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogStats, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogStreaming, Display, All);
DECLARE_LOG_CATEGORY_EXTERN(LogInit, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogExit, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogExec, Warning, All);
DECLARE_LOG_CATEGORY_EXTERN(LogScript, Warning, All);
DECLARE_LOG_CATEGORY_EXTERN(LogLocalization, Error, All);
DECLARE_LOG_CATEGORY_EXTERN(LogProcess, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogLoad, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogTemp, Log, All);

FOutputDeviceRedirector* GetGlobalLogSingleton();
#define GLog GetGlobalLogSingleton()
extern FConfigCacheIni* GConfig;
extern FFeedbackContext* GWarn;
extern FOutputDeviceError* GError;
extern FOutputDeviceConsole* GLogConsole;

extern bool GIsRunningUnattendedScript;
#if WITH_ENGINE
extern bool PRIVATE_GIsRunningCommandlet;
#endif
#if WITH_EDITORONLY_DATA
extern bool GIsEditor;
extern bool GIsImportingT3D;
extern bool GIsUCCMakeStandaloneHeaderGenerator;
extern bool GIsTransacting;
extern bool GIsReconstructingBlueprintInstances;
extern bool GForceLoadEditorOnly;
extern bool GIsCookerLoadingPackage;
#else
#define GIsEditor false
#define GIsUCCMakeStandaloneHeaderGenerator false
#define GIsTransacting false
#define GIsReconstructingBlueprintInstances false
#define GForceLoadEditorOnly false
#define GIsCookerLoadingPackage false
#endif
extern bool GIsClient;
extern bool GIsServer;
extern bool GIsCriticalError;
extern bool GIsRunning;
extern bool GIsFirstInstance;
extern bool GIsGameThreadIdInitialized;
extern bool GIsSilent;
extern bool GIsSlowTask;
extern bool GSlowTaskOccurred;
extern bool GIsGuarded;
extern bool GIsBuildMachine;
extern bool GIsHighResScreenshot;
extern bool GIsDumpingMovie;
extern bool GIsPlayInEditorWorld;
extern bool GIsInitialLoad;
extern bool GAreScreenMessagesEnabled;
extern bool GScreenMessagesRestoreState;
extern bool GEdSelectionLock;
extern int32 GPlayInEditorID;
extern uint64 GFrameCounter;
extern uint64 GFrameCounterRenderThread;
extern uint32 GFrameNumber;
extern uint32 GFrameNumberRenderThread;
extern uint32 GGameThreadId;
extern uint32 GRenderThreadId;
extern double GStartTime;
extern float GNearClippingPlane;
extern int32 GScreenshotResolutionX;
extern int32 GScreenshotResolutionY;
extern TCHAR GInternalProjectName[64];
extern const TCHAR* GForeignEngineDir;
extern FString GEngineIni;
extern FString GEditorIni;
extern FString GEditorPerProjectIni;
extern FString GInputIni;
extern FString GGameIni;
extern FString GGameUserSettingsIni;
extern FString GDeviceProfilesIni;
extern FString GScalabilityIni;
extern FString GSystemStartTime;

bool IsRunningCommandlet();
bool IsRunningCookCommandlet();
bool IsRunningDedicatedServer();
bool IsRunningGame();
bool IsRunningClientOnly();
bool IsAllowCommandletRendering();
bool IsAllowCommandletAudio();
bool IsEngineExitRequested();
void RequestEngineExit(const TCHAR* ReasonString);
void RequestEngineExit(const FString& ReasonString);
bool IsInGameThread();
bool IsInSlateThread();
bool IsInAudioThread();
bool IsInRenderingThread();
bool IsInParallelRenderingThread();
bool IsInActualRenderingThread();
bool IsInRHIThread();
bool IsInParallelGameThread();
