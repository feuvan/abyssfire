// UAbyssAutoTestSubsystem (Framework/AbyssAutoTestSubsystem.h): the in-game autotest. Everything lives in
// FAbyssAutoTestRun; the subsystem only exists when -abyssautotest is on the command line.
#include "Framework/AbyssAutoTestSubsystem.h"

#include "Abyssfire.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/CriticalSection.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformProperties.h"
#include "HAL/PlatformTime.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Logging/LogMacros.h"
#include "Logging/LogVerbosity.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "UnrealClient.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "abyss/base/Json.h"
#include "abyss/base/Math.h"
#include "abyss/data/DataStore.h"
#include "abyss/items/Shop.h"
#include "abyss/quests/Dialogue.h"
#include "abyss/quests/QuestWorld.h"
#include "abyss/sim/SimContext.h"
#include "abyss/tools/AutoPilot.h"

#include "Actors/AbyssCharacterActor.h"
#include "Actors/AbyssCorpses.h"
#include "Framework/AbyssActorRegistry.h"
#include "Framework/AbyssAutoTest.h"
#include "Framework/AbyssConsoleVars.h"
#include "Framework/AbyssEventRouter.h"
#include "Framework/AbyssGameInstance.h"
#include "Framework/AbyssPerfProbe.h"
#include "Framework/AbyssPlayerController.h"
#include "Framework/AbyssPresenceRules.h"
#include "Framework/AbyssSimDriver.h"
#include "Framework/AbyssSmoothnessRules.h"
#include "Framework/AbyssText.h"
#include "Input/AbyssInputSubsystem.h"
#include "Input/AbyssInputTypes.h"
#include "Platform/AbyssPlatform.h"
#include "Vfx/AbyssTelegraphs.h"
#include "World/AbyssAssetLibrary.h"
#include "World/AbyssThemeRules.h"
#include "World/AbyssWorldBuilder.h"
#include "World/AbyssWorldStateRules.h"

// File-local category. Shipping builds compile logging out (NO_LOGGING): use it in UE_LOG only.
DEFINE_LOG_CATEGORY_STATIC(LogAbyssAutoTest, Log, All);

// =====================================================================================================================
// Switches
// =====================================================================================================================

namespace
{
	FString AbyssAutoTest_SwitchValue(const TCHAR* Match, bool bStopOnSeparator = true)
	{
		FString Value;
		FParse::Value(FCommandLine::Get(), Match, Value, bStopOnSeparator);
		return Value.TrimStartAndEnd();
	}

	FString AbyssAutoTest_OutputDir()
	{
		FString Dir = AbyssAutoTest_SwitchValue(TEXT("abyssautotestdir="), /*bStopOnSeparator*/ false);
		if (Dir.IsEmpty())
		{
			Dir = FPaths::ProjectSavedDir() / TEXT("AutoTest");
		}
		else if (FPaths::IsRelative(Dir))
		{
			Dir = FPaths::LaunchDir() / Dir;   // a relative directory is relative to where the game was started
		}
		Dir = FPaths::ConvertRelativePathToFull(Dir);
		FPaths::NormalizeDirectoryName(Dir);
		return Dir;
	}

	/** Left in the output directory by a run: only a directory carrying it is cleaned by the next run. */
	const TCHAR* const AbyssAutoTest_MarkerFileName = TEXT(".abyssautotest");

	/** Path is Dir or lies inside it: full, normalised paths compared ignoring case (as macOS and Windows volumes do). */
	bool AbyssAutoTest_IsSameOrInside(const FString& Path, const FString& Dir)
	{
		FString FullPath = FPaths::ConvertRelativePathToFull(Path);
		FString FullDir = FPaths::ConvertRelativePathToFull(Dir);
		FPaths::NormalizeDirectoryName(FullPath);
		FPaths::NormalizeDirectoryName(FullDir);
		return FullPath.Equals(FullDir, ESearchCase::IgnoreCase) || FullPath.StartsWith(FullDir + TEXT("/"), ESearchCase::IgnoreCase);
	}

	/**
	 * Why OutputDir must not be used, or empty: its SaveGames/ would be, or contain, the player's own saves
	 * (FAbyssSaveStorage's <Project>/Saved/SaveGames), e.g. -abyssautotestdir=<Project>/Saved.
	 */
	FString AbyssAutoTest_SaveDirConflict(const FString& OutputDir)
	{
		const FString PlayerSaveDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("SaveGames"));
		if (!AbyssAutoTest_IsSameOrInside(PlayerSaveDir, OutputDir / TEXT("SaveGames")))
		{
			return FString();
		}
		return FString::Printf(TEXT("-abyssautotestdir=%s would put the run's saves on the player's own (%s): use a directory of its own, e.g. %s"),
			*OutputDir, *PlayerSaveDir, *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("AutoTest")));
	}

	/** An -abyssseed= value: a whole number from 1 to 2^63-1 (the core test's ABYSS_PT_SWEEP_BASE takes the same range). */
	bool AbyssAutoTest_ParseSeed(const FString& Text, uint64& OutSeed)
	{
		if (Text.IsEmpty() || Text.Len() > 19)
		{
			return false;
		}
		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			if (!FChar::IsDigit(Text[Index]))
			{
				return false;
			}
		}
		const uint64 Parsed = FCString::Strtoui64(*Text, nullptr, 10);   // 19 digits cannot overflow 64 bits
		if (Parsed == 0 || Parsed > static_cast<uint64>(TNumericLimits<int64>::Max()))
		{
			return false;
		}
		OutSeed = Parsed;
		return true;
	}
}

namespace AbyssAutoTest
{
	bool IsRequested()
	{
#if UE_BUILD_SHIPPING
		return false;
#else
		if (GIsEditor)
		{
			return false;   // PIE: the editor's own session; the autotest is for -game runs and packaged builds
		}
		const TCHAR* CommandLine = FCommandLine::Get();
		FString Value;
		return FParse::Param(CommandLine, TEXT("abyssautotest")) || FParse::Value(CommandLine, TEXT("abyssautotest="), Value);
#endif
	}

	FAbyssAutoTestOptions ParseOptions()
	{
		FAbyssAutoTestOptions Options;
		const FString ScenarioText = AbyssAutoTest_SwitchValue(TEXT("abyssautotest=")).ToLower();
		if (!ScenarioText.IsEmpty())
		{
			abyss::AutoPilotScenario Parsed = abyss::AutoPilotScenario::Smoke;
			if (abyss::ParseEnum(AbyssText::ToStd(ScenarioText), Parsed) && Parsed != abyss::AutoPilotScenario::None)
			{
				Options.Scenario = Parsed;
			}
			else
			{
				Options.Problems.Add(FString::Printf(TEXT("unknown scenario '%s' (-abyssautotest=smoke|chapter)"), *ScenarioText));
			}
		}
		const FString ClassText = AbyssAutoTest_SwitchValue(TEXT("abyssclass=")).ToLower();
		if (!ClassText.IsEmpty() && !abyss::ParseEnum(AbyssText::ToStd(ClassText), Options.HeroClass))
		{
			Options.Problems.Add(FString::Printf(TEXT("unknown class '%s' (-abyssclass=warrior|mage|rogue)"), *ClassText));
		}
		Options.OutputDir = AbyssAutoTest_OutputDir();
		Options.MaxSeconds = Options.Scenario == abyss::AutoPilotScenario::Chapter1 ? 2400.0 : 600.0;
		const FString MaxText = AbyssAutoTest_SwitchValue(TEXT("abyssautotestmaxsec="));
		if (!MaxText.IsEmpty())
		{
			const double Parsed = FCString::Atod(*MaxText);
			if (Parsed > 0.0)
			{
				Options.MaxSeconds = Parsed;
			}
			else
			{
				Options.Problems.Add(FString::Printf(TEXT("-abyssautotestmaxsec=%s is not a positive number of seconds"), *MaxText));
			}
		}
		const FString ShotText = AbyssAutoTest_SwitchValue(TEXT("abyssautotestshotsec="));
		if (!ShotText.IsEmpty())
		{
			Options.ShotIntervalSeconds = FMath::Max(0.0, FCString::Atod(*ShotText));
		}
		Options.BuildSha = AbyssAutoTest_SwitchValue(TEXT("abyssbuildsha="));
		// Smoothness measurement (audit F2 / section 6).
		Options.bNoShots = FParse::Param(FCommandLine::Get(), TEXT("abyssnoshots"));
		Options.bStatUnit = FParse::Param(FCommandLine::Get(), TEXT("abyssstatunit"));
		Options.bSmoothGate = FParse::Param(FCommandLine::Get(), TEXT("abysssmoothgate"));
		const FString SmoothText = AbyssAutoTest_SwitchValue(TEXT("abysssmooth="), /*bStopOnSeparator*/ false);
		std::string SmoothError;
		if (!SmoothText.IsEmpty() && !AbyssSmoothnessRules::ParseThresholds(AbyssText::ToStd(SmoothText), Options.Smooth, SmoothError))
		{
			Options.Problems.Add(FString::Printf(TEXT("-abysssmooth=%s: %s"), *SmoothText, *AbyssText::ToFString(SmoothError)));
		}
		const FString TimelineText = AbyssAutoTest_SwitchValue(TEXT("abysstimelinerows="));
		if (!TimelineText.IsEmpty())
		{
			Options.TimelineRows = FMath::Clamp(FCString::Atoi(*TimelineText), 0, 2000000);
		}
		const FString SeedText = AbyssAutoTest_SwitchValue(TEXT("abyssseed="));
		uint64 SwitchSeed = 0;
		if (!SeedText.IsEmpty() && !AbyssAutoTest_ParseSeed(SeedText, SwitchSeed))
		{
			Options.Problems.Add(FString::Printf(TEXT("-abyssseed=%s is not a whole number from 1 to 9223372036854775807"), *SeedText));
		}
		Options.Seed = GetNewGameSeed();
		TArray<FString> IgnoredParts;
		AbyssAutoTest_SwitchValue(TEXT("abyssautotestignore="), /*bStopOnSeparator*/ false).ParseIntoArray(IgnoredParts, TEXT(","), true);
		for (FString& Part : IgnoredParts)
		{
			Part.TrimStartAndEndInline();
			if (!Part.IsEmpty())
			{
				Options.IgnoredLogCategories.AddUnique(FName(*Part));
			}
		}
		return Options;
	}

	FString GetSaveDirOverride()
	{
		if (!IsRequested())
		{
			return FString();
		}
		const FString OutputDir = AbyssAutoTest_OutputDir();
		// Never the player's own saves: a run refused for its directory (FAbyssAutoTestRun::PrepareOutputDir) keeps its
		// files apart all the same.
		if (!AbyssAutoTest_SaveDirConflict(OutputDir).IsEmpty())
		{
			return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("AutoTest") / TEXT("RefusedRun") / TEXT("SaveGames"));
		}
		return OutputDir / TEXT("SaveGames");
	}

	uint64 GetNewGameSeed()
	{
		if (!IsRequested())
		{
			return 0;
		}
		// Once per process: StartNewGame plays the seed that ParseOptions puts in report.json. Drawn seeds stay below
		// 2^63 so the core test can replay them (ABYSS_PT_SWEEP_BASE).
		static const uint64 Seed = []() -> uint64
		{
			uint64 Parsed = 0;
			if (AbyssAutoTest_ParseSeed(AbyssAutoTest_SwitchValue(TEXT("abyssseed=")), Parsed))
			{
				return Parsed;
			}
			const uint64 Drawn = (FPlatformTime::Cycles64() ^ static_cast<uint64>(FDateTime::UtcNow().GetTicks()))
				& static_cast<uint64>(TNumericLimits<int64>::Max());
			return Drawn != 0 ? Drawn : 1;
		}();
		return Seed;
	}
}

// =====================================================================================================================
// Log capture (GLog device, any thread) and the watchdog thread
// =====================================================================================================================

/** Counts the run's Error / Warning / Ensure lines and keeps the first ones. */
class FAbyssAutoTestLogCapture final : public FOutputDevice
{
public:
	static constexpr int32 MaxKeptLines = 50;

	struct FLine
	{
		FString Verbosity;
		FString Category;
		FString Text;
		double Seconds = 0.0;
	};

	struct FCounts
	{
		int32 Errors = 0;
		int32 Warnings = 0;
		int32 Ensures = 0;
		int32 IgnoredErrors = 0;
	};

	FAbyssAutoTestLogCapture(const TArray<FName>& InIgnoredCategories, FName InOwnCategory, double InStartSeconds)
		: IgnoredCategories(InIgnoredCategories)
		, OwnCategory(InOwnCategory)
		, StartSeconds(InStartSeconds)
	{
	}

	using FOutputDevice::Serialize;   // keep the overload with a timestamp visible (it forwards to the one below)

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		if (V == nullptr || Category == OwnCategory)
		{
			return;
		}
		const ELogVerbosity::Type Level = static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask);
		const bool bEnsure = FCString::Strifind(V, TEXT("Ensure condition failed")) != nullptr;
		const bool bError = Level == ELogVerbosity::Error || Level == ELogVerbosity::Fatal;
		const bool bWarning = Level == ELogVerbosity::Warning;
		if (!bEnsure && !bError && !bWarning)
		{
			return;
		}
		const bool bIgnored = bError && !bEnsure && IgnoredCategories.Contains(Category);
		FLine Line;
		Line.Verbosity = bEnsure ? TEXT("Ensure") : (bError ? (bIgnored ? TEXT("Error (ignored)") : TEXT("Error")) : TEXT("Warning"));
		Line.Category = Category.ToString();
		Line.Text = FString(V).Left(1000);
		Line.Seconds = FPlatformTime::Seconds() - StartSeconds;

		FScopeLock Lock(&Mutex);
		if (bEnsure)
		{
			++Counts.Ensures;
		}
		if (bError)
		{
			if (bIgnored)
			{
				++Counts.IgnoredErrors;
			}
			else
			{
				++Counts.Errors;
			}
		}
		else if (bWarning)
		{
			++Counts.Warnings;
		}
		TArray<FLine>& Kept = bWarning && !bEnsure ? Warnings : Problems;
		if (Kept.Num() < MaxKeptLines)
		{
			Kept.Add(MoveTemp(Line));
		}
	}

	virtual bool CanBeUsedOnAnyThread() const override
	{
		return true;   // own lock; errors on worker threads are counted as they happen
	}

	void Read(FCounts& OutCounts, TArray<FLine>& OutProblems, TArray<FLine>& OutWarnings) const
	{
		FScopeLock Lock(&Mutex);
		OutCounts = Counts;
		OutProblems = Problems;
		OutWarnings = Warnings;
	}

private:
	mutable FCriticalSection Mutex;
	TArray<FName> IgnoredCategories;
	FName OwnCategory;
	double StartSeconds = 0.0;
	FCounts Counts;
	TArray<FLine> Problems;   // Error / Ensure lines
	TArray<FLine> Warnings;
};

/** Last resort for a stuck game thread: past the deadline it leaves watchdog.txt and force-exits with 2. */
class FAbyssAutoTestWatchdog final : public FRunnable
{
public:
	FAbyssAutoTestWatchdog(double InDeadlineSeconds, FString InNotePath)
		: DeadlineSeconds(InDeadlineSeconds)
		, NotePath(MoveTemp(InNotePath))
	{
	}

	virtual uint32 Run() override
	{
		while (!bStopping.load())
		{
			if (FPlatformTime::Seconds() >= DeadlineSeconds)
			{
				const FString Note = TEXT("AbyssAutoTest watchdog: the run did not end within its wall-clock limit (+120 s grace); ")
					TEXT("the game thread is presumed stuck. Forced exit with status 2.\n");
				FFileHelper::SaveStringToFile(Note, *NotePath);
				FPlatformMisc::LowLevelOutputDebugString(*Note);
				FPlatformMisc::RequestExitWithStatus(true, 2);
				return 1;
			}
			FPlatformProcess::Sleep(0.25f);
		}
		return 0;
	}

	virtual void Stop() override
	{
		bStopping.store(true);
	}

private:
	std::atomic<bool> bStopping{false};
	double DeadlineSeconds = 0.0;
	FString NotePath;
};

// =====================================================================================================================
// The run
// =====================================================================================================================

namespace
{
	// ---- engine values of the smoothness measurement, each behind a compile-time hedge (CompileCheck/API_UNCERTAIN.md
	// #9, #10): a value this engine does not give stays Unavailable and is written as null.

	struct FAbyssAutoTestThreadMs
	{
		float Game = AbyssSmoothnessRules::Unavailable;
		float Render = AbyssSmoothnessRules::Unavailable;
		float Rhi = AbyssSmoothnessRules::Unavailable;
		float Gpu = AbyssSmoothnessRules::Unavailable;
	};

	/** The frame-rate cap's sleep of this frame (FApp::GetIdleTime), ms. */
	template <typename AppType>
	float AbyssAutoTest_IdleMs()
	{
		if constexpr (requires { AppType::GetIdleTime(); })
		{
			return static_cast<float>(AppType::GetIdleTime() * 1000.0);
		}
		else
		{
			return AbyssSmoothnessRules::Unavailable;
		}
	}

	template <typename AppType>
	constexpr bool AbyssAutoTest_HasIdleTime()
	{
		return requires { AppType::GetIdleTime(); };
	}

	/** `stat unit` shows 0 for a value it does not have (no GPU timer on this RHI, or the stat is not drawn). */
	float AbyssAutoTest_PositiveMs(double Value)
	{
		return Value > 0.0 ? static_cast<float>(Value) : AbyssSmoothnessRules::Unavailable;
	}

	/** The unfiltered values `stat unit` computed at its last draw, each member looked up on its own. */
	template <typename DataType>
	void AbyssAutoTest_CopyStatUnit(const DataType& Data, FAbyssAutoTestThreadMs& Out)
	{
		if constexpr (requires { Data.RawGameThreadTime; })
		{
			Out.Game = AbyssAutoTest_PositiveMs(static_cast<double>(Data.RawGameThreadTime));
		}
		if (Out.Game < 0.f)
		{
			return;   // the stat is not being drawn: nothing below is current
		}
		if constexpr (requires { Data.RawRenderThreadTime; })
		{
			Out.Render = AbyssAutoTest_PositiveMs(static_cast<double>(Data.RawRenderThreadTime));
		}
		if constexpr (requires { Data.RawRHITTime; })
		{
			Out.Rhi = AbyssAutoTest_PositiveMs(static_cast<double>(Data.RawRHITTime));
		}
		if constexpr (requires { Data.RawGPUFrameTime[0]; })
		{
			Out.Gpu = AbyssAutoTest_PositiveMs(static_cast<double>(Data.RawGPUFrameTime[0]));
		}
	}

	template <typename ClientType>
	constexpr bool AbyssAutoTest_HasStatUnitData()
	{
		return requires(const ClientType* Client) { Client->GetStatUnitData(); };
	}

	template <typename ClientType>
	void AbyssAutoTest_ReadStatUnit(const ClientType* Client, FAbyssAutoTestThreadMs& Out)
	{
		if constexpr (requires { Client->GetStatUnitData(); })
		{
			if (Client != nullptr)
			{
				if (const auto* Data = Client->GetStatUnitData())
				{
					AbyssAutoTest_CopyStatUnit(*Data, Out);
				}
			}
		}
	}

	/** `stat unit` is already on (an operator's console command, a config): the toggle must not be sent again. */
	template <typename ClientType>
	bool AbyssAutoTest_IsStatUnitOn(const ClientType* Client)
	{
		if constexpr (requires { static_cast<bool>(Client->IsStatEnabled(FString())); })
		{
			return Client != nullptr && Client->IsStatEnabled(FString(TEXT("Unit")));
		}
		else
		{
			return false;
		}
	}

	template <typename ClientType>
	constexpr bool AbyssAutoTest_HasIsStatEnabled()
	{
		return requires(const ClientType* Client) { static_cast<bool>(Client->IsStatEnabled(FString())); };
	}

	/**
	 * Turns `stat unit` on for this viewport without toggling and without depending on which viewport the engine's
	 * `stat` command would address (an unattended window that never had focus). False: this engine has no such entry
	 * point, or there is no viewport (the caller sends the console command).
	 */
	template <typename EngineType, typename ClientType>
	bool AbyssAutoTest_SetStatUnit(EngineType* Engine, UWorld* World, ClientType* Client)
	{
		if constexpr (requires { Engine->SetEngineStat(World, Client, FString(), true); })
		{
			if (Engine != nullptr && Client != nullptr)
			{
				Engine->SetEngineStat(World, Client, FString(TEXT("Unit")), true);
				return true;
			}
		}
		return false;
	}

	template <typename MiscType>
	FString AbyssAutoTest_GpuName()
	{
		if constexpr (requires { MiscType::GetPrimaryGPUBrand(); })
		{
			return MiscType::GetPrimaryGPUBrand();
		}
		else
		{
			return FString();
		}
	}

	template <typename MiscType>
	FString AbyssAutoTest_CpuName()
	{
		if constexpr (requires { MiscType::GetCPUBrand(); })
		{
			return MiscType::GetCPUBrand();
		}
		else
		{
			return FString();
		}
	}

	const TCHAR* AbyssAutoTest_BuildConfiguration()
	{
#if UE_BUILD_SHIPPING
		return TEXT("Shipping");
#elif defined(UE_BUILD_TEST) && UE_BUILD_TEST
		return TEXT("Test");
#elif defined(UE_BUILD_DEBUG) && UE_BUILD_DEBUG
		return TEXT("Debug");
#elif defined(UE_BUILD_DEVELOPMENT) && UE_BUILD_DEVELOPMENT
		return TEXT("Development");
#else
		return TEXT("Unknown");
#endif
	}

	/** A console variable's value as text, "" when this engine has no such variable (read-only: nothing is recorded). */
	FString AbyssAutoTest_ConsoleValue(const TCHAR* Name)
	{
		const IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name, /*bTrackFrequentCalls*/ false);
		return Variable != nullptr ? Variable->GetString() : FString();
	}

	// Read at the end of the run into report.json `environment.consoleVariables` (audit section 6 and the A/B list of
	// its section 5); a name this engine does not have goes to `consoleVariablesMissing`.
	const TCHAR* const GAbyssAutoTestConsoleVariables[] = {
		TEXT("r.ScreenPercentage"), TEXT("r.SecondaryScreenPercentage.GameViewport"), TEXT("r.AntiAliasingMethod"),
		TEXT("r.TSR.History.ScreenPercentage"), TEXT("r.PSOPrecaching"), TEXT("r.VSync"), TEXT("t.MaxFPS"),
		TEXT("r.Shadow.MaxCSMResolution"), TEXT("r.Shadow.CSM.MaxCascades"), TEXT("r.Shadow.RadiusThreshold"),
		TEXT("r.AmbientOcclusionLevels"), TEXT("r.ContactShadows"), TEXT("r.MaterialQualityLevel"),
		TEXT("sg.ResolutionQuality"), TEXT("sg.ViewDistanceQuality"), TEXT("sg.AntiAliasingQuality"), TEXT("sg.ShadowQuality"),
		TEXT("sg.GlobalIlluminationQuality"), TEXT("sg.ReflectionQuality"), TEXT("sg.PostProcessQuality"),
		TEXT("sg.TextureQuality"), TEXT("sg.EffectsQuality"), TEXT("sg.FoliageQuality"), TEXT("sg.ShadingQuality"),
		TEXT("sg.LandscapeQuality"),
		TEXT("abyss.Quality"), TEXT("abyss.RenderScale"), TEXT("abyss.WaitForCompilation"), TEXT("abyss.WaitForCompilationMaxSec"),
	};

	// What the report cannot hold: engine statistics with no entry point the module could call behind a hedge (they are
	// plain globals or stat-system groups). Written into report.json `perf.operatorConsole` as the steps to do by hand.
	const char* const GAbyssAutoTestOperatorSteps[] = {
		"stat unit: Frame / Game / Draw / RHIT / GPU ms (recorded per frame only with -abyssstatunit; check that GPU is not 0 on Metal)",
		"stat rhi: draw calls and triangles drawn, standing in an open field and in a fight (audit M2, F8, U7)",
		"stat scenerendering: mesh draw calls with 0, 10 and 20 monsters in view (U7)",
		"stat slate: Num Batches, Num Layers and elements in the field with the HUD up (U1)",
		"stat gpu or ProfileGPU: BasePass, ShadowDepths, ShadowProjection, AO, CustomDepth, TSR (M3, F7, U3, U4)",
		"stat psoprecache (cooked build) and r.PSOPrecache.Validation 2: precache misses during the run (U6)",
		"Slate.DumpFontCacheStats before and after a 20-kill fight: font atlas pages and cache entries (U5)",
		"dumpticks enabled: prop-owned skeletal ticks (F6)",
		"console A/B, one at a time, with stat gpu before and after: r.Shadow.CSM.MaxCascades 1; r.Shadow.MaxCSMResolution 2048; "
			"r.AmbientOcclusionLevels 0; r.ContactShadows 0; r.TSR.History.ScreenPercentage 100; "
			"r.SecondaryScreenPercentage.GameViewport 50; showflag.materials 0; HUD hidden",
		"the log line 'Audio: N sounds loaded (M missing) in X ms' (M9)",
	};

	struct FAbyssAutoTestZoneEntry
	{
		FString Map;
		double Seconds = 0.0;
		double FrameMs = 0.0;
		double BuildMs = 0.0;
		int32 Spawns = 0;
		double SpawnMs = 0.0;
		double CompileWaitMs = 0.0;
		int32 ShaderJobs = -1;
		int32 AssetBuilds = -1;
	};

	enum class EAbyssAutoTestPhase : uint8
	{
		Boot,      // waiting for the title screen
		Menu,      // title screen up: settle, photograph
		Starting,  // StartNewGame called, waiting for the session
		Playing,   // the bot plays
		Ending,    // the bot finished: last screenshot
		Done,      // report written, exit requested
	};

	enum class EAbyssAutoTestResult : uint8
	{
		None,
		Passed,
		Failed,
		Timeout,
		Aborted,
	};

	const TCHAR* AbyssAutoTest_ResultName(EAbyssAutoTestResult Result)
	{
		switch (Result)
		{
		case EAbyssAutoTestResult::Passed: return TEXT("passed");
		case EAbyssAutoTestResult::Failed: return TEXT("failed");
		case EAbyssAutoTestResult::Timeout: return TEXT("timeout");
		case EAbyssAutoTestResult::Aborted: return TEXT("aborted");
		case EAbyssAutoTestResult::None: break;
		}
		return TEXT("none");
	}

	struct FAbyssAutoTestPanel
	{
		EAbyssInputAction Action;
		const TCHAR* Name;
	};

	// The HUD panels toured at the camp, opened and closed through the input layer (UAbyssInputSubsystem::PressAction).
	const FAbyssAutoTestPanel GAbyssAutoTestPanels[] = {
		{ EAbyssInputAction::PanelInventory, TEXT("inventory") },
		{ EAbyssInputAction::PanelCharacter, TEXT("character") },
		{ EAbyssInputAction::PanelSkills, TEXT("skills") },
		{ EAbyssInputAction::PanelQuestLog, TEXT("quest_log") },
		{ EAbyssInputAction::PanelWorldMap, TEXT("world_map") },
		{ EAbyssInputAction::PanelSettings, TEXT("settings") },
	};
	constexpr int32 GAbyssAutoTestPanelCount = UE_ARRAY_COUNT(GAbyssAutoTestPanels);
	// The same panels as scene labels of the smoothness measurement ("panel_<label>").
	const char* const GAbyssAutoTestPanelLabels[] = { "inventory", "character", "skills", "quest_log", "world_map", "settings" };
	static_assert(UE_ARRAY_COUNT(GAbyssAutoTestPanelLabels) == UE_ARRAY_COUNT(GAbyssAutoTestPanels));

	struct FAbyssAutoTestShot
	{
		FString Tag;
		FString Caption;
		FString File;              // relative to the output directory
		double DueSeconds = 0.0;   // run seconds
		double RequestedSeconds = -1.0;
		double SimMs = 0.0;
		bool bReleaseHold = false;
		bool bRequested = false;
		bool bDone = false;
		bool bTimedOut = false;
	};

	struct FAbyssAutoTestMilestone
	{
		FString Kind;
		FString Detail;
		double Seconds = 0.0;
		double SimMs = 0.0;
	};
}

class FAbyssAutoTestRun
{
public:
	FAbyssAutoTestRun(UAbyssAutoTestSubsystem& InOwner, FAbyssAutoTestOptions InOptions)
		: Owner(InOwner)
		, Options(MoveTemp(InOptions))
	{
	}

	~FAbyssAutoTestRun()
	{
		StopWatchdog();
	}

	FAbyssAutoTestRun(const FAbyssAutoTestRun&) = delete;
	FAbyssAutoTestRun& operator=(const FAbyssAutoTestRun&) = delete;

	// ---- lifetime (subsystem Initialize / Deinitialize) -------------------------------------------------------------

	void Start()
	{
		StartSeconds = FPlatformTime::Seconds();
		IFileManager& Files = IFileManager::Get();
		const bool bOutputDirUsable = PrepareOutputDir(Files);

		// The bot's timers count frames tuned for 60 Hz (abyss/tools/AutoPilot.h): never run it faster (an uncapped
		// -nullrhi run would shrink every timeout). Slower frame rates only make it react later in sim time.
		if (IConsoleVariable* MaxFps = AbyssConsoleVars::FindForWrite(TEXT("t.MaxFPS")))
		{
			const float Current = MaxFps->GetFloat();
			if (Current <= 0.0f || Current > 60.0f)
			{
				MaxFps->Set(60.0f, ECVF_SetByCode);
			}
			MaxFpsApplied = MaxFps->GetFloat();
		}

		// -abyssnoshots: a measurement run never requests a screenshot (a request stalls its frame, audit F2 / U8).
		bCanShoot = FApp::CanEverRender() && !Options.bNoShots;
		// The module's hooks count into this run's frame record from now on (Framework/AbyssPerfProbe.h).
		const abyss::SimConfig SimDefaults;
		SimLimits.MaxStepsPerFrame = SimDefaults.maxStepsPerFrame;
		SimLimits.MaxFrameMs = SimDefaults.maxFrameMs;
		Contacts = AbyssSmoothnessRules::FContactTracker(SimLimits.MaxStepsPerFrame);
		Samples.reserve(40000);
		GAbyssPerfProbe = &ProbeFrame;
		RhiName = GDynamicRHI != nullptr ? FString(GDynamicRHI->GetName()) : FString(TEXT("none"));
		if (bCanShoot && bOutputDirUsable)
		{
			Files.MakeDirectory(*(Options.OutputDir / TEXT("screenshots")), true);
		}

		LogCapture = MakeUnique<FAbyssAutoTestLogCapture>(Options.IgnoredLogCategories, FName(TEXT("LogAbyssAutoTest")), StartSeconds);
		if (GLog != nullptr)
		{
			GLog->AddOutputDevice(LogCapture.Get());
		}
		if (FPlatformProcess::SupportsMultithreading())
		{
			Watchdog = MakeUnique<FAbyssAutoTestWatchdog>(StartSeconds + Options.MaxSeconds + 120.0, Options.OutputDir / TEXT("watchdog.txt"));
			WatchdogThread = FRunnableThread::Create(Watchdog.Get(), TEXT("AbyssAutoTestWatchdog"), 0, TPri_BelowNormal);
		}
		UE_LOG(LogAbyssAutoTest, Display, TEXT("AutoTest: scenario %s, class %s, seed %llu, limit %.0f s, rhi %s, screenshots %s, t.MaxFPS %.0f, output %s"),
			*AbyssText::ToFString(abyss::EnumName(Options.Scenario)), *AbyssText::ToFString(abyss::EnumName(Options.HeroClass)),
			static_cast<unsigned long long>(Options.Seed), Options.MaxSeconds, *RhiName,
			bCanShoot ? TEXT("on") : (Options.bNoShots ? TEXT("off (-abyssnoshots)") : TEXT("off (no RHI renders: -nullrhi)")),
			MaxFpsApplied, *Options.OutputDir);
		UE_LOG(LogAbyssAutoTest, Display, TEXT("AutoTest: frame samples with FApp::GetIdleTime %s, stat unit data %s%s; smoothness %s"),
			AbyssAutoTest_HasIdleTime<FApp>() ? TEXT("yes") : TEXT("NOT on this engine (idleMs null)"),
			AbyssAutoTest_HasStatUnitData<UGameViewportClient>() ? TEXT("readable") : TEXT("NOT on this engine (thread times null)"),
			Options.bStatUnit ? TEXT(" (-abyssstatunit)") : TEXT(" (off: pass -abyssstatunit)"),
			Options.bSmoothGate ? TEXT("gates the run (-abysssmoothgate)") : TEXT("is informational"));
	}

	/**
	 * A fresh run in the output directory. It never touches the player's saves, and it deletes only what an earlier run
	 * left in a directory that run marked as its own (.abyssautotest), so a mistyped -abyssautotestdir cannot delete
	 * anybody's files. Anything else is a problem that fails the run before it plays. Returns whether the directory is
	 * used.
	 */
	bool PrepareOutputDir(IFileManager& Files)
	{
		const FString Conflict = AbyssAutoTest_SaveDirConflict(Options.OutputDir);
		if (!Conflict.IsEmpty())
		{
			Options.Problems.Add(Conflict);
			return false;
		}
		Files.MakeDirectory(*Options.OutputDir, true);
		const FString Marker = Options.OutputDir / AbyssAutoTest_MarkerFileName;
		const FString SaveDir = Options.OutputDir / TEXT("SaveGames");
		const FString ShotDir = Options.OutputDir / TEXT("screenshots");
		if (Files.FileExists(*Marker))
		{
			// An earlier run's outputs go (FAbyssSaveStorage creates SaveGames/ again right after the subsystems are
			// initialised).
			Files.DeleteDirectory(*SaveDir, false, true);
			Files.DeleteDirectory(*ShotDir, false, true);
			Files.Delete(*(Options.OutputDir / TEXT("report.json")), false, true, true);
			Files.Delete(*(Options.OutputDir / TEXT("frames.csv")), false, true, true);
			Files.Delete(*(Options.OutputDir / TEXT("watchdog.txt")), false, true, true);
		}
		else if (Files.DirectoryExists(*SaveDir) || Files.DirectoryExists(*ShotDir))
		{
			Options.Problems.Add(FString::Printf(TEXT("%s has a SaveGames/ or screenshots/ directory that no autotest run left (no %s): nothing deleted; use an empty directory"),
				*Options.OutputDir, AbyssAutoTest_MarkerFileName));
			return false;
		}
		FFileHelper::SaveStringToFile(FString(TEXT("Abyssfire autotest output (-abyssautotest): the next run here deletes SaveGames/, screenshots/, report.json, frames.csv and watchdog.txt.\n")), *Marker);
		return true;
	}

	void Shutdown()
	{
		if (Phase != EAbyssAutoTestPhase::Done)
		{
			Finish(EAbyssAutoTestResult::Aborted, TEXT("the game shut down before the run ended"));
		}
		if (GLog != nullptr && LogCapture.IsValid())
		{
			GLog->RemoveOutputDevice(LogCapture.Get());
		}
		LogCapture.Reset();
		StopWatchdog();
		Pilot.Reset();
		if (GAbyssPerfProbe == &ProbeFrame)
		{
			GAbyssPerfProbe = nullptr;
		}
	}

	// ---- per engine frame (core ticker) -----------------------------------------------------------------------------

	void Tick()
	{
		const double Now = RunSeconds();
		if (LastTickSeconds >= 0.0 && (Phase == EAbyssAutoTestPhase::Playing || Phase == EAbyssAutoTestPhase::Ending))
		{
			FrameMs.Add(static_cast<float>((Now - LastTickSeconds) * 1000.0));
		}
		if (Phase != EAbyssAutoTestPhase::Done)
		{
			CloseFrame(Now);   // the tagged sample of the frame that just ended, in every phase (before ProcessShots)
		}
		LastTickSeconds = Now;
		if (Phase == EAbyssAutoTestPhase::Done)
		{
			return;
		}
		if (Now > Options.MaxSeconds)
		{
			Finish(EAbyssAutoTestResult::Timeout, FString::Printf(TEXT("wall-clock limit of %.0f s reached (phase %d, bot step '%s')"),
				Options.MaxSeconds, static_cast<int32>(Phase), Pilot.IsValid() ? *AbyssText::ToFString(Pilot->CurrentStep()) : TEXT("-")));
			return;
		}
		UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
		if (GameInstance == nullptr)
		{
			return;
		}
		ProcessShots(Now);

		switch (Phase)
		{
		case EAbyssAutoTestPhase::Boot:
			if (Options.Problems.Num() > 0)
			{
				Finish(EAbyssAutoTestResult::Failed, FString::Join(Options.Problems, TEXT("; ")));
			}
			else if (GameInstance->GetAppState() == EAbyssAppState::DataError)
			{
				Finish(EAbyssAutoTestResult::Failed, TEXT("data error: ") + GameInstance->GetDataError().Left(400));
			}
			else if (GameInstance->GetAppState() == EAbyssAppState::MainMenu)
			{
				EnterPhase(EAbyssAutoTestPhase::Menu, Now);
				MenuSeconds = Now;
				AddMilestone(TEXT("mainMenu"), TEXT("title screen"), Now);
				EnableStatUnit();
			}
			break;
		case EAbyssAutoTestPhase::Menu:
			if (MenuShot == INDEX_NONE && !bMenuShotQueued && Now - PhaseSeconds >= 3.0)
			{
				bMenuShotQueued = true;
				MenuShot = QueueShot(TEXT("main_menu"), TEXT("main menu (title screen, save slots)"), Now, false);
			}
			if (bMenuShotQueued && ShotDone(MenuShot))
			{
				EnterPhase(EAbyssAutoTestPhase::Starting, Now);
				// menuIdleSec ends and startNewGameSec begins here: the title screen's settle and screenshot are the
				// test's own time, not the cost of starting a game (audit F2).
				StartNewGameSeconds = Now;
				Loading.Begin();
				// The main menu's own call (SAbyssMainMenu::StartGame): slot 0 of the isolated save directory, Normal.
				const bool bStarted = GameInstance->StartNewGame(Options.HeroClass, 0, abyss::Difficulty::Normal);
				StartNewGameCallMs = (RunSeconds() - Now) * 1000.0;
				if (!bStarted)
				{
					Finish(EAbyssAutoTestResult::Failed, TEXT("UAbyssGameInstance::StartNewGame refused"));
				}
			}
			break;
		case EAbyssAutoTestPhase::Starting:
			if (Now - PhaseSeconds > 120.0 && !GameInstance->HasSession())
			{
				Finish(EAbyssAutoTestResult::Failed, TEXT("no session 120 s after StartNewGame"));
			}
			break;
		case EAbyssAutoTestPhase::Playing:
			TickPanelTour(Now);
			// -abyssnoshots: the holds a picture would have released end at the picture's time.
			for (int32 Index = TimedReleases.Num() - 1; Index >= 0; --Index)
			{
				if (Now >= TimedReleases[Index])
				{
					TimedReleases.RemoveAtSwap(Index);
					ReleaseHold();
				}
			}
			// Safety net: a picture that never comes must not park the bot (the camp tour has its own timeouts).
			if (TourIndex == INDEX_NONE && HoldCount > 0 && HoldSince >= 0.0 && Now - HoldSince > 60.0)
			{
				UE_LOG(LogAbyssAutoTest, Display, TEXT("AutoTest: bot held for 60 s, releasing"));
				HoldCount = 1;
				ReleaseHold();
			}
			break;
		case EAbyssAutoTestPhase::Ending:
			if (AllShotsDone() || Now - PhaseSeconds > 20.0)
			{
				FinishWithVerdict();
			}
			break;
		case EAbyssAutoTestPhase::Done:
			break;
		}
	}

	// ---- session and core events (FAbyssEventRouter) ----------------------------------------------------------------

	void HandleSessionStarted()
	{
		UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
		const abyss::DataStore* Data = GameInstance != nullptr ? GameInstance->GetData() : nullptr;
		if (Data == nullptr || Phase == EAbyssAutoTestPhase::Done || Pilot.IsValid())
		{
			return;
		}
		abyss::AutoPilotConfig Config;
		Config.cls = Options.HeroClass;
		Config.scenario = Options.Scenario;
		Config.storyMode = abyss::AutoPilotStoryMode::SkipAll;
		Config.onNote = [](const std::string& Line)
		{
			UE_LOG(LogAbyssAutoTest, Log, TEXT("bot %s"), *AbyssText::ToFString(Line));
		};
		Config.onMilestone = [this](const abyss::AutoPilotMilestoneRecord& Record) { HandlePilotMilestone(Record); };
		Pilot = MakeUnique<abyss::AutoPilot>(*Data, MoveTemp(Config), [this](const abyss::Command& Command) { SubmitAsInput(Command); });
		Pilot->SetHold(HoldCount > 0);
		const double Now = RunSeconds();
		SessionSeconds = Now;
		NextPeriodicSeconds = Now + Options.ShotIntervalSeconds;
		EnterPhase(EAbyssAutoTestPhase::Playing, Now);
		AddMilestone(TEXT("session"), TEXT("new game, slot 0, normal"), Now);
	}

	void HandleSessionEnded()
	{
		if (Phase == EAbyssAutoTestPhase::Playing || Phase == EAbyssAutoTestPhase::Ending)
		{
			Finish(EAbyssAutoTestResult::Failed, TEXT("the session ended while the bot was playing"));
		}
		Pilot.Reset();
	}

	void HandleCoreEvent(const abyss::Event& Event)
	{
		if (Pilot.IsValid())
		{
			Pilot->ObserveEvent(Event);
		}
		ObservePerfEvent(Event);
		if (Phase != EAbyssAutoTestPhase::Playing)
		{
			return;
		}
		TallyPresenceEvent(Event);
		const double Now = RunSeconds();
		if (const abyss::EvStoryBeat* Beat = std::get_if<abyss::EvStoryBeat>(&Event))
		{
			// The first beat of each kind (prologue, chapter card, cutscene, boss intro) plays a moment for its picture
			// before the bot skips it.
			const uint32 Bit = 1u << static_cast<uint32>(Beat->kind);
			if (Beat->phase == abyss::EvStoryBeat::Phase::Began && (bCanShoot || Options.bNoShots) && (StoryKindsShot & Bit) == 0)
			{
				StoryKindsShot |= Bit;
				const FString Kind = AbyssText::ToFString(abyss::EnumName(Beat->kind));
				const double Delay = Beat->kind == abyss::StoryBeatKind::Chapter ? 3.0 : 2.0;
				AddHold(Now);
				QueueShot(TEXT("story_") + Kind, FString::Printf(TEXT("story beat %s (%s), %.0f s in"),
					*AbyssText::ToFString(Beat->beatId), *Kind, Delay), Now + Delay, true);
			}
		}
		else if (const abyss::EvSkillUsed* Skill = std::get_if<abyss::EvSkillUsed>(&Event))
		{
			if (Skill->slot >= 0 && Skill->slot < 8 && (SkillSlotsShot & (1u << Skill->slot)) == 0)
			{
				SkillSlotsShot |= 1u << Skill->slot;
				QueueShot(FString::Printf(TEXT("skill_%d"), Skill->slot + 1), FString::Printf(TEXT("hotbar slot %d cast: %s"),
					Skill->slot + 1, *AbyssText::ToFString(Skill->skillId)), Now + 0.3, false);
			}
		}
		else if (const abyss::EvHit* Hit = std::get_if<abyss::EvHit>(&Event))
		{
			if (Hit->killed && Hit->targetFaction == abyss::Faction::Monster && !bShotLoot && LootCheckSeconds < 0.0)
			{
				LootCheckSeconds = Now + 0.7;   // the drop lands, then look for loot around the hero
			}
		}
	}

	void HandleFrame(const abyss::Snapshot& Snap, const FAbyssFrameInfo& Frame)
	{
		NotePerfFrame(Snap, Frame);
		if (!Pilot.IsValid() || (Phase != EAbyssAutoTestPhase::Playing && Phase != EAbyssAutoTestPhase::Ending))
		{
			return;
		}
		Pilot->EndFrame(Snap, Frame.StepsThisFrame);
		if (Phase == EAbyssAutoTestPhase::Playing)
		{
			const double Now = RunSeconds();
			SamplePresence(Snap);
			FrameShots(Snap, Now);
			for (int32 Guard = 0; Guard < 32 && !bPilotFinished; ++Guard)
			{
				const abyss::AutoPilotStatus Status = Pilot->Advance(Snap);
				if (Status == abyss::AutoPilotStatus::Checkpoint)
				{
					HandleCheckpoint(Now);
					continue;
				}
				if (Status == abyss::AutoPilotStatus::Finished)
				{
					HandlePilotFinished(Now);
				}
				break;
			}
		}
		Pilot->PrepareFrame(Snap);
	}

private:
	// ---- helpers ----------------------------------------------------------------------------------------------------

	UAbyssGameInstance* GetAbyssGameInstance() const
	{
		return Cast<UAbyssGameInstance>(Owner.GetGameInstance());
	}

	double RunSeconds() const
	{
		return FPlatformTime::Seconds() - StartSeconds;
	}

	double SimMsNow() const
	{
		const UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
		const abyss::Snapshot* Snap = GameInstance != nullptr ? GameInstance->GetSnapshot() : nullptr;
		return Snap != nullptr ? Snap->simNowMs : 0.0;
	}

	void EnterPhase(EAbyssAutoTestPhase NewPhase, double Now)
	{
		Phase = NewPhase;
		PhaseSeconds = Now;
	}

	void AddMilestone(const FString& Kind, const FString& Detail, double Now)
	{
		FAbyssAutoTestMilestone& Milestone = Milestones.AddDefaulted_GetRef();
		Milestone.Kind = Kind;
		Milestone.Detail = Detail;
		Milestone.Seconds = Now;
		Milestone.SimMs = SimMsNow();
		UE_LOG(LogAbyssAutoTest, Display, TEXT("AutoTest milestone %s %s (%.1f s)"), *Kind, *Detail, Now);
	}

	void HandlePilotMilestone(const abyss::AutoPilotMilestoneRecord& Record)
	{
		AddMilestone(AbyssText::ToFString(abyss::EnumName(Record.kind)), AbyssText::ToFString(Record.detail), RunSeconds());
		if (Record.kind == abyss::AutoPilotMilestone::BossKill)
		{
			CheckPresence(TEXT("bossKill ") + AbyssText::ToFString(Record.detail));   // the Bonegorger, the Corpseherd, Valdras, ...
		}
	}

	// ---- presentation probe (2G review X6, AbyssPresenceRules) ------------------------------------------------------

	/** What the core announced that the presentation must draw (counted in a zone with a presentation theme). */
	void TallyPresenceEvent(const abyss::Event& Event)
	{
		const UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
		const abyss::DataStore* Data = GameInstance != nullptr ? GameInstance->GetData() : nullptr;
		const abyss::Snapshot* Snap = GameInstance != nullptr ? GameInstance->GetSnapshot() : nullptr;
		const bool bThemed = Data != nullptr && Snap != nullptr && !AbyssThemeRules::ThemeOf(Data->FindMap(Snap->zone.mapId)).empty();
		if (const abyss::EvTelegraph* Telegraph = std::get_if<abyss::EvTelegraph>(&Event))
		{
			Presence.StrikeTelegraphs += AbyssPresenceRules::ExpectsDecal(Telegraph->kind, Telegraph->source, bThemed) ? 1 : 0;
		}
		else if (std::holds_alternative<abyss::EvHazard>(Event))
		{
			Presence.Hazards += bThemed ? 1 : 0;
		}
		else if (const abyss::EvArena* Arena = std::get_if<abyss::EvArena>(&Event))
		{
			Presence.Arenas += Arena->cleared ? 0 : 1;
		}
		else if (std::holds_alternative<abyss::EvCorpseAdded>(Event))
		{
			++Presence.Corpses;
		}
	}

	/** What the presentation drew this frame (the world view synced before the router's OnFrame). */
	void SamplePresence(const abyss::Snapshot& Snap)
	{
		UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
		UWorld* GameWorld = GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
		if (GameWorld == nullptr)
		{
			return;
		}
		const UAbyssTelegraphSystem* Telegraphs = UAbyssTelegraphSystem::Get(GameWorld);
		const UAbyssCorpseLayer* Corpses = UAbyssCorpseLayer::Get(GameWorld);
		const UAbyssWorldBuilder* Builder = UAbyssWorldBuilder::Get(GameWorld);
		const AAbyssCharacterActor* Hero = Builder != nullptr ? Builder->FindCharacter(abyss::kHeroEntityId) : nullptr;
		const EAbyssQualityTier Tier = GameInstance->GetQualityTier();
		const int32 TierIndex = Tier == EAbyssQualityTier::Low ? 0 : (Tier == EAbyssQualityTier::High ? 2 : 1);
		const bool bAshnumbExpected = Snap.hero.ashnumbStacks > 0 && Snap.hero.life == abyss::HeroLife::Alive && Hero != nullptr
			&& Hero->HasArt() && AbyssWorldStateRules::BandAllowed(TierIndex, AbyssPlatform::IsMobilePlatform());
		AbyssPresenceRules::Sample(Presence, Telegraphs != nullptr ? Telegraphs->GetDrawnDecalCount() : 0,
			Telegraphs != nullptr ? Telegraphs->GetDrawnArenaCount() : 0, Corpses != nullptr ? Corpses->GetHuskCount() : 0,
			bAshnumbExpected, Hero != nullptr && Hero->IsAshnumbBandShown());
	}

	/** At a named boss's kill and at the end: the first time the core announced what nothing drew, the run fails. */
	void CheckPresence(const FString& At)
	{
		const std::string Missing = AbyssPresenceRules::Verdict(Presence);
		if (Missing.empty() || !PresenceFailure.IsEmpty())
		{
			return;
		}
		PresenceFailure = FString::Printf(TEXT("presentation (at %s): %s"), *At, *AbyssText::ToFString(Missing));
		UE_LOG(LogAbyssAutoTest, Warning, TEXT("AutoTest %s"), *PresenceFailure);
	}

	/**
	 * Commands of the bot go where the input layer sends its own: AAbyssPlayerController::SubmitCommand (GI->Submit
	 * without one). The bot is called from the router's OnFrame, after the frame's sim: its commands wait for the next
	 * frame's sim (feel.commands.toSimStep is the frame period by construction; see commandsNote).
	 */
	void SubmitAsInput(const abyss::Command& Command)
	{
		++CommandsSubmitted;
		++FrameCommands;
		Commands.Submit(AbyssSmoothnessRules::ClassifyCommand(Command), RunSeconds());
		UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
		UAbyssInputSubsystem* Input = GameInstance != nullptr ? GameInstance->GetSubsystem<UAbyssInputSubsystem>() : nullptr;
		if (AAbyssPlayerController* PlayerController = Input != nullptr ? Input->GetPlayerController() : nullptr)
		{
			PlayerController->SubmitCommand(Command);
		}
		else if (GameInstance != nullptr)
		{
			GameInstance->Submit(Command);
		}
	}

	void PressPanel(int32 Index)
	{
		UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
		UAbyssInputSubsystem* Input = GameInstance != nullptr ? GameInstance->GetSubsystem<UAbyssInputSubsystem>() : nullptr;
		if (Input != nullptr && Index >= 0 && Index < GAbyssAutoTestPanelCount)
		{
			Input->PressAction(GAbyssAutoTestPanels[Index].Action, EAbyssInputDevice::KeyboardMouse);
		}
	}

	void AddHold(double Now)
	{
		if (HoldCount++ == 0)
		{
			HoldSince = Now;
		}
		if (Pilot.IsValid())
		{
			Pilot->SetHold(true);
		}
	}

	void ReleaseHold()
	{
		HoldCount = FMath::Max(0, HoldCount - 1);
		if (HoldCount == 0)
		{
			HoldSince = -1.0;
		}
		if (Pilot.IsValid())
		{
			Pilot->SetHold(HoldCount > 0);
		}
	}

	// ---- screenshots ------------------------------------------------------------------------------------------------

	/** Queues a screenshot WITH the UI. Returns its index, or INDEX_NONE when nothing renders (-nullrhi). */
	int32 QueueShot(const FString& Tag, const FString& Caption, double DueSeconds, bool bReleaseHold)
	{
		if (!bCanShoot)
		{
			SkippedShots.Add(Tag);
			if (bReleaseHold)
			{
				if (Options.bNoShots)
				{
					TimedReleases.Add(DueSeconds);   // same pacing as a run with pictures, so the scenes compare
				}
				else
				{
					ReleaseHold();
				}
			}
			return INDEX_NONE;
		}
		FAbyssAutoTestShot& Shot = Shots.AddDefaulted_GetRef();
		Shot.Tag = Tag;
		Shot.Caption = Caption;
		Shot.File = FString::Printf(TEXT("screenshots/%03d_%s.png"), Shots.Num(), *Tag);
		Shot.DueSeconds = DueSeconds;
		Shot.bReleaseHold = bReleaseHold;
		return Shots.Num() - 1;
	}

	bool ShotDone(int32 Index) const
	{
		return !Shots.IsValidIndex(Index) || Shots[Index].bDone;
	}

	bool AllShotsDone() const
	{
		for (const FAbyssAutoTestShot& Shot : Shots)
		{
			if (!Shot.bDone)
			{
				return false;
			}
		}
		return true;
	}

	void CompleteShot(int32 Index, bool bTimedOut)
	{
		FAbyssAutoTestShot& Shot = Shots[Index];
		Shot.bDone = true;
		Shot.bTimedOut = bTimedOut;
		UE_LOG(LogAbyssAutoTest, Display, TEXT("AutoTest screenshot %s%s: %s"), *Shot.File, bTimedOut ? TEXT(" (not processed)") : TEXT(""), *Shot.Caption);
		if (Shot.bReleaseHold)
		{
			ReleaseHold();
		}
	}

	/** One FScreenshotRequest at a time (the engine keeps a single pending request), in due order. */
	void ProcessShots(double Now)
	{
		if (Shots.IsValidIndex(ShotInFlight))
		{
			if (!FScreenshotRequest::IsScreenshotRequested())
			{
				CompleteShot(ShotInFlight, false);
				ShotInFlight = INDEX_NONE;
			}
			else if (Now - Shots[ShotInFlight].RequestedSeconds > 10.0)
			{
				CompleteShot(ShotInFlight, true);   // the viewport never drew it
				ShotInFlight = INDEX_NONE;
			}
			return;
		}
		int32 Next = INDEX_NONE;
		for (int32 Index = 0; Index < Shots.Num(); ++Index)
		{
			const FAbyssAutoTestShot& Shot = Shots[Index];
			if (!Shot.bRequested && Shot.DueSeconds <= Now && (Next == INDEX_NONE || Shot.DueSeconds < Shots[Next].DueSeconds))
			{
				Next = Index;
			}
		}
		if (Next == INDEX_NONE)
		{
			return;
		}
		FAbyssAutoTestShot& Shot = Shots[Next];
		Shot.bRequested = true;
		Shot.RequestedSeconds = Now;
		Shot.SimMs = SimMsNow();
		FScreenshotRequest::RequestScreenshot(Options.OutputDir / Shot.File, /*bInShowUI*/ true, /*bAddFilenameSuffix*/ false);
		ShotInFlight = Next;
	}

	/** Per frame while the bot plays: panels it opened, combat, loot, periodic pictures. */
	void FrameShots(const abyss::Snapshot& Snap, double Now)
	{
		if (bCanShoot || Options.bNoShots)
		{
			// A core modal the bot opened stays up for its picture (the bot is held; the world is frozen or blocked).
			if (!bShotQuestCard && Snap.questCard != nullptr && Snap.questCard->open)
			{
				bShotQuestCard = true;
				AddHold(Now);
				QueueShot(TEXT("quest_card"), FString::Printf(TEXT("quest card of %s"), *AbyssText::ToFString(Snap.questCard->npcId)), Now + 0.8, true);
			}
			if (!bShotDialogue && Snap.dialogue != nullptr && Snap.dialogue->open)
			{
				bShotDialogue = true;
				AddHold(Now);
				QueueShot(TEXT("dialogue"), TEXT("dialogue tree"), Now + 0.8, true);
			}
			if (!bShotShop && Snap.shop != nullptr && Snap.shop->open)
			{
				bShotShop = true;
				AddHold(Now);
				QueueShot(TEXT("shop"), TEXT("shop panel"), Now + 0.8, true);
			}
		}
		if (!bShotCombat && Snap.hero.inCombat && Snap.hero.target != abyss::kNoEntity)
		{
			for (const abyss::MonsterView& Monster : Snap.monsters)
			{
				if (Monster.id == Snap.hero.target && Monster.alive && abyss::Dist(Monster.pos, Snap.hero.pos) <= 3.5)
				{
					bShotCombat = true;
					QueueShot(TEXT("combat"), FString::Printf(TEXT("mid-combat: %s vs %s"),
						*AbyssText::ToFString(abyss::EnumName(Options.HeroClass)), *AbyssText::ToFString(Monster.defId)), Now, false);
					break;
				}
			}
		}
		if (!bShotBossFight && Snap.bossBar.show)
		{
			for (const abyss::MonsterView& Monster : Snap.monsters)
			{
				if (Monster.id == Snap.bossBar.monster && Monster.alive && abyss::Dist(Monster.pos, Snap.hero.pos) <= 5.0)
				{
					bShotBossFight = true;
					QueueShot(TEXT("boss_fight"), FString::Printf(TEXT("boss fight: %s"), *AbyssText::ToFString(Monster.defId)), Now, false);
					break;
				}
			}
		}
		if (LootCheckSeconds >= 0.0 && Now >= LootCheckSeconds)
		{
			LootCheckSeconds = -1.0;
			int32 Drops = 0;
			for (const abyss::GroundItemView& Item : Snap.groundItems)
			{
				Drops += abyss::Dist(Item.pos, Snap.hero.pos) <= 8.0 ? 1 : 0;
			}
			for (const abyss::PotionDropView& Potion : Snap.potions)
			{
				Drops += abyss::Dist(Potion.pos, Snap.hero.pos) <= 8.0 ? 1 : 0;
			}
			if (Drops > 0)
			{
				bShotLoot = true;
				QueueShot(TEXT("loot"), FString::Printf(TEXT("loot on the ground after a kill (%d drop(s) near the hero)"), Drops), Now, false);
			}
		}
		if (Options.ShotIntervalSeconds > 0.0 && Now >= NextPeriodicSeconds)
		{
			NextPeriodicSeconds = Now + Options.ShotIntervalSeconds;
			++PeriodicShots;
			QueueShot(FString::Printf(TEXT("periodic_%02d"), PeriodicShots), FString::Printf(TEXT("every %.0f s: bot step '%s'"),
				Options.ShotIntervalSeconds, *AbyssText::ToFString(Pilot->CurrentStep())), Now, false);
		}
	}

	// ---- checkpoints, the camp panel tour, the end ------------------------------------------------------------------

	void HandleCheckpoint(double Now)
	{
		const std::string& Id = Pilot->CurrentCheckpoint();
		if (Id == "intro" && !bTourStarted)
		{
			// Arrival at the camp (prologue and chapter card over, world idle): the camp, then every HUD panel.
			bTourStarted = true;
			CampShot = QueueShot(TEXT("camp"), TEXT("camp on arrival"), Now + 0.5, false);
			AddHold(Now);
			TourIndex = 0;
			TourStage = 0;
			TourStageSeconds = Now;
		}
	}

	void TickPanelTour(double Now)
	{
		if (TourIndex == INDEX_NONE)
		{
			return;
		}
		switch (TourStage)
		{
		case 0:   // wait for the camp picture
			if (ShotDone(CampShot) || Now - TourStageSeconds > 15.0)
			{
				TourStage = 1;
			}
			break;
		case 1:   // open
			PressPanel(TourIndex);
			OpenTourPanel = GAbyssAutoTestPanelLabels[TourIndex];
			TourStage = 2;
			TourStageSeconds = Now;
			break;
		case 2:   // let it build, then the picture
			if (Now - TourStageSeconds >= 1.2)
			{
				TourShot = QueueShot(FString::Printf(TEXT("panel_%s"), GAbyssAutoTestPanels[TourIndex].Name),
					FString::Printf(TEXT("%s panel (opened through the input layer)"), GAbyssAutoTestPanels[TourIndex].Name), Now, false);
				TourStage = 3;
			}
			break;
		case 3:   // close
			// -abyssnoshots: the panel stays up 2.5 s, so its scene has frames to measure.
			if ((ShotDone(TourShot) && (!Options.bNoShots || Now - TourStageSeconds >= 2.5)) || Now - TourStageSeconds > 15.0)
			{
				PressPanel(TourIndex);
				OpenTourPanel.clear();
				TourStage = 4;
				TourStageSeconds = Now;
			}
			break;
		default:  // closed: next panel
			if (Now - TourStageSeconds >= 0.6)
			{
				if (++TourIndex >= GAbyssAutoTestPanelCount)
				{
					TourIndex = INDEX_NONE;
					AddMilestone(TEXT("panelTour"), FString::Printf(TEXT("%d HUD panels opened and closed"), GAbyssAutoTestPanelCount), Now);
					ReleaseHold();
					return;
				}
				TourStage = 1;
			}
			break;
		}
	}

	void HandlePilotFinished(double Now)
	{
		bPilotFinished = true;
		HoldCount = 1;
		ReleaseHold();
		TimedReleases.Reset();
		TourIndex = INDEX_NONE;
		OpenTourPanel.clear();
		if (Pilot->Ok())
		{
			QueueShot(TEXT("end"), FString::Printf(TEXT("scenario %s done"), *AbyssText::ToFString(abyss::EnumName(Options.Scenario))), Now + 1.0, false);
		}
		else
		{
			FailureReason = TEXT("bot: ") + AbyssText::ToFString(Pilot->Failure());
			QueueShot(TEXT("failure"), TEXT("where the bot failed"), Now, false);
		}
		EnterPhase(EAbyssAutoTestPhase::Ending, Now);
	}

	bool ScenarioCompleted() const
	{
		return Pilot.IsValid() && Pilot->Ok() && Pilot->Finished() && Pilot->Reached(abyss::AutoPilotMilestone::ScenarioDone);
	}

	void FinishWithVerdict()
	{
		FAbyssAutoTestLogCapture::FCounts Counts;
		if (LogCapture.IsValid())
		{
			TArray<FAbyssAutoTestLogCapture::FLine> Ignored1;
			TArray<FAbyssAutoTestLogCapture::FLine> Ignored2;
			LogCapture->Read(Counts, Ignored1, Ignored2);
		}
		CheckPresence(TEXT("the scenario's end"));
		if (!ScenarioCompleted())
		{
			Finish(EAbyssAutoTestResult::Failed, FailureReason.IsEmpty() ? FString(TEXT("the scenario did not complete")) : FailureReason);
		}
		else if (!PresenceFailure.IsEmpty())
		{
			Finish(EAbyssAutoTestResult::Failed, PresenceFailure);   // 2G review X6: a presentation that drew nothing
		}
		else if (Counts.Errors > 0 || Counts.Ensures > 0)
		{
			Finish(EAbyssAutoTestResult::Failed, FString::Printf(TEXT("the scenario completed, but the log has %d Error and %d Ensure line(s)"),
				Counts.Errors, Counts.Ensures));
		}
		else
		{
			// -abysssmoothgate: an otherwise green run fails on a smoothness summary other than "smooth".
			SummarizePerf();
			const std::string GateFailure = AbyssSmoothnessRules::GateFailure(Verdict, Options.bSmoothGate);
			if (!GateFailure.empty())
			{
				Finish(EAbyssAutoTestResult::Failed, AbyssText::ToFString(GateFailure));
			}
			else
			{
				Finish(EAbyssAutoTestResult::Passed, FString());
			}
		}
	}

	void Finish(EAbyssAutoTestResult InResult, const FString& Reason)
	{
		if (Phase == EAbyssAutoTestPhase::Done)
		{
			return;
		}
		Result = InResult;
		if (InResult != EAbyssAutoTestResult::Passed && !Reason.IsEmpty())
		{
			FailureReason = Reason;
		}
		EnterPhase(EAbyssAutoTestPhase::Done, RunSeconds());
		FinishedSeconds = PhaseSeconds;
		const int32 ExitCode = Result == EAbyssAutoTestResult::Passed ? 0 : 2;
		const FString ReportPath = Options.OutputDir / TEXT("report.json");
		SummarizePerf();
		WriteTimeline();
		[[maybe_unused]] const bool bWritten = WriteReport(ReportPath, ExitCode);
		UE_LOG(LogAbyssAutoTest, Display, TEXT("AutoTest smoothness: %s (%d in-game play frames, p50 %.1f ms, p95 %.1f ms, p99 %.1f ms, ")
			TEXT("%d over 100 ms, game / real %.3f)%s"), *AbyssText::ToFString(Verdict.Grade), PerfSummary.InGame.Play.Count,
			PerfSummary.InGame.Play.P50Ms, PerfSummary.InGame.Play.P95Ms, PerfSummary.InGame.Play.P99Ms, PerfSummary.InGame.Play.Over100,
			PerfSummary.InGame.GameRealRatio(), bCanShoot ? TEXT(" - screenshots were on: measure with -abyssnoshots") : TEXT(""));
		UE_LOG(LogAbyssAutoTest, Display, TEXT("AutoTest %s after %.1f s%s%s: exit %d, report %s%s"), AbyssAutoTest_ResultName(Result),
			FinishedSeconds, FailureReason.IsEmpty() ? TEXT("") : TEXT(" - "), *FailureReason, ExitCode, *ReportPath,
			bWritten ? TEXT("") : TEXT(" (NOT WRITTEN)"));
		if (GLog != nullptr)
		{
			GLog->Flush();
		}
		FPlatformMisc::RequestExitWithStatus(false, static_cast<uint8>(ExitCode));
	}

	void StopWatchdog()
	{
		if (WatchdogThread != nullptr)
		{
			WatchdogThread->Kill(/*bShouldWait*/ true);
			delete WatchdogThread;
			WatchdogThread = nullptr;
		}
		Watchdog.Reset();
	}

	// ---- smoothness measurement (performance audit F2 / section 6; Framework/AbyssSmoothnessRules.h) ---------------

	/** Core events the measurement reads (any phase with a session). */
	void ObservePerfEvent(const abyss::Event& Event)
	{
		if (std::holds_alternative<abyss::EvLog>(Event))
		{
			++FrameEvLogLines;
			++TotalEvLogLines;
		}
		else if (const abyss::EvZone* Zone = std::get_if<abyss::EvZone>(&Event))
		{
			if (Zone->phase == abyss::EvZone::Phase::TransitionBegan)
			{
				Loading.Begin();
			}
			else if (Zone->phase == abyss::EvZone::Phase::Entered)
			{
				Loading.ZoneEntered(RunSeconds());
				PendingZoneMap = AbyssText::ToFString(Zone->mapId);
			}
		}
		else if (const abyss::EvPlayAnim* Anim = std::get_if<abyss::EvPlayAnim>(&Event))
		{
			if (Anim->entity == abyss::kHeroEntityId && Anim->action == abyss::AnimAction::Attack)
			{
				Contacts.OnHeroAttackAnim(Anim->startMs, Anim->contactMs, Anim->clip);
			}
		}
		else if (const abyss::EvHit* Hit = std::get_if<abyss::EvHit>(&Event))
		{
			if (Hit->source == abyss::kHeroEntityId && !Hit->tick)
			{
				// The frame that presents this hit draws the pose of RenderSimMs (the driver set it before dispatching).
				const UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
				const UAbyssSimDriver* Driver = GameInstance != nullptr ? UAbyssSimDriver::Get(GameInstance) : nullptr;
				if (Driver != nullptr)
				{
					double ErrorMs = 0.0;
					// Matched by kind: a basic hit (skillId empty) against a basic swing, a skill's against its own swing.
					Contacts.OnHeroHit(Driver->GetFrameInfo().SimNowMs, Driver->GetFrameInfo().RenderSimMs, ErrorMs, Hit->skillId);
				}
			}
		}
	}

	/** What the driver's frame showed (router OnFrame): the scene inputs and gauges of the frame sample being built. */
	void NotePerfFrame(const abyss::Snapshot& Snap, const FAbyssFrameInfo& Frame)
	{
		bFrameHadSim = true;
		FrameSteps = Frame.StepsThisFrame;
		bFrameFrozen = AbyssSmoothnessRules::FrozenFrame(bPrevFrozen, Frame.bFrozen);
		bPrevFrozen = Frame.bFrozen;
		bFrameDilated = Frame.VisualTimeDilation < 0.999f;
		bFrameStory = Snap.storyBusy || Snap.cinematic;
		bFrameInCombat = Snap.hero.inCombat;
		bFrameInSafeZone = false;
		for (const abyss::Vec2& Camp : Snap.zone.camps)
		{
			bFrameInSafeZone = bFrameInSafeZone || abyss::Dist(Camp, Snap.hero.pos) <= Snap.zone.safeZoneRadius;
		}
		FramePanel = OpenTourPanel;
		if (FramePanel.empty() && !Snap.modals.empty())
		{
			FramePanel = std::string(abyss::EnumName(Snap.modals.front()));
		}
		bFrameBoss = Snap.bossBar.show;
		FrameBossPhase = Snap.bossBar.phase;
		if (bFrameBoss && Snap.bossBar.monster != FrameBossEntity)
		{
			FrameBossEntity = Snap.bossBar.monster;
			FrameBossId.clear();
			for (const abyss::MonsterView& Monster : Snap.monsters)
			{
				if (Monster.id == Snap.bossBar.monster)
				{
					FrameBossId = Monster.defId;
					break;
				}
			}
		}
		FrameCharacters = static_cast<int32>(Snap.monsters.size() + Snap.npcs.size()) + 1;
	}

	/** Autotest switch -abyssstatunit: the engine's own frame split, drawn by `stat unit` and read back per frame. */
	void EnableStatUnit()
	{
		if (!Options.bStatUnit || bStatUnitRequested || GEngine == nullptr || !FApp::CanEverRender())
		{
			return;
		}
		bStatUnitRequested = true;
		const UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
		UGameViewportClient* Client = GameInstance != nullptr ? GameInstance->GetGameViewportClient() : nullptr;
		UWorld* World = GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
		const bool bAlreadyOn = AbyssAutoTest_IsStatUnitOn(Client);
		// First the call that names the game viewport and sets instead of toggling (hedged). The console command is the
		// fallback: it toggles, so it is sent once and never when the stat already shows; and the engine routes it to
		// the viewport it last processed input for, which an unattended window may never have been.
		const bool bSet = !bAlreadyOn && AbyssAutoTest_SetStatUnit(GEngine, World, Client);
		const bool bSetTookEffect = bSet && (!AbyssAutoTest_HasIsStatEnabled<UGameViewportClient>() || AbyssAutoTest_IsStatUnitOn(Client));
		const bool bHandled = bAlreadyOn || bSetTookEffect || GEngine->Exec(World, TEXT("stat unit"), *GLog);
		UE_LOG(LogAbyssAutoTest, Display, TEXT("AutoTest: stat unit %s (-abyssstatunit: game / render / RHI / GPU ms per frame)"),
			bAlreadyOn ? TEXT("was already on")
				: (bSetTookEffect ? TEXT("set on the game viewport") : (bHandled ? TEXT("requested by console command") : TEXT("not handled by this build"))));
		if (bCanShoot)
		{
			UE_LOG(LogAbyssAutoTest, Warning, TEXT("AutoTest: -abyssstatunit with screenshots on: the stat overlay is in every picture, and a "
				"screenshot stalls its frame. A measurement run takes -abyssnoshots as well."));
		}
	}

	/** Closes the engine frame that ended now: one sample (every phase), then the per-frame state starts over. */
	void CloseFrame(double Now)
	{
		namespace Rules = AbyssSmoothnessRules;
		if (LastTickSeconds < 0.0)
		{
			ProbeFrame.Reset();
			return;
		}
		UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
		UWorld* GameWorld = GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
		Rules::FFrameSample Sample;
		Sample.RealSec = Now;
		Sample.FrameMs = static_cast<float>((Now - LastTickSeconds) * 1000.0);
		Rules::ApplyProbe(Sample, ProbeFrame, bFrameHadSim);
		Sample.Steps = static_cast<uint8>(FMath::Clamp(FrameSteps, 0, 255));
		Sample.EvLogLines = Rules::Clamp16(static_cast<uint32>(FrameEvLogLines));
		Sample.Commands = Rules::Clamp16(static_cast<uint32>(FrameCommands));

		// -- tags
		if (Shots.IsValidIndex(ShotInFlight) || ShotTagFrames > 0)
		{
			Sample.Tags |= Rules::TagShot;
			ShotTagFrames = Shots.IsValidIndex(ShotInFlight) ? 1 : ShotTagFrames - 1;   // and the frame after: the read-back
		}
		const bool bPlaying = Phase == EAbyssAutoTestPhase::Playing || Phase == EAbyssAutoTestPhase::Ending;
		if (Phase == EAbyssAutoTestPhase::Boot || Phase == EAbyssAutoTestPhase::Starting || (bPlaying && Loading.IsLoading(Now)))
		{
			Sample.Tags |= Rules::TagLoading;
		}
		if (bFrameHadSim)
		{
			Sample.Tags |= bFrameStory ? Rules::TagStory : 0u;
			Sample.Tags |= !FramePanel.empty() ? Rules::TagPanel : 0u;
			Sample.Tags |= bFrameFrozen ? Rules::TagFrozen : 0u;
			Sample.Tags |= bFrameDilated ? Rules::TagDilated : 0u;
			Sample.Tags |= Rules::StepCapHit(FrameSteps, SimLimits.MaxStepsPerFrame, ProbeFrame.SimDtMs) ? Rules::TagStepCap : 0u;
		}
		int32 ShaderJobs = -1;
		int32 AssetBuilds = -1;
		UAbyssAssetLibrary::GetPendingCompilation(ShaderJobs, AssetBuilds);
		Sample.ShaderJobs = FMath::Max(ShaderJobs, ProbeFrame.ShaderJobsAtWait);
		Sample.AssetBuilds = FMath::Max(AssetBuilds, ProbeFrame.AssetBuildsAtWait);
		if (Sample.ShaderJobs > 0 || Sample.AssetBuilds > 0)
		{
			Sample.Tags |= Rules::TagBacklog;
		}

		// -- engine timings (each hedged: Unavailable stays when this engine does not give the value)
		Sample.IdleMs = AbyssAutoTest_IdleMs<FApp>();
		if (bStatUnitRequested)
		{
			FAbyssAutoTestThreadMs Threads;
			AbyssAutoTest_ReadStatUnit(GameInstance != nullptr ? GameInstance->GetGameViewportClient() : nullptr, Threads);
			Sample.GameMs = Threads.Game;
			Sample.RenderMs = Threads.Render;
			Sample.RhiMs = Threads.Rhi;
			Sample.GpuMs = Threads.Gpu;
			ThreadSamples += Threads.Game >= 0.f ? 1 : 0;
			GpuSamples += Threads.Gpu >= 0.f ? 1 : 0;
		}

		// -- scene
		Rules::FSceneInput SceneInput;
		switch (Phase)
		{
		case EAbyssAutoTestPhase::Boot: SceneInput.Phase = Rules::EPhase::Boot; break;
		case EAbyssAutoTestPhase::Menu: SceneInput.Phase = Rules::EPhase::Menu; break;
		case EAbyssAutoTestPhase::Starting: SceneInput.Phase = Rules::EPhase::Starting; break;
		case EAbyssAutoTestPhase::Playing: SceneInput.Phase = bFrameHadSim ? Rules::EPhase::Playing : Rules::EPhase::Starting; break;
		case EAbyssAutoTestPhase::Ending:
		case EAbyssAutoTestPhase::Done: SceneInput.Phase = Rules::EPhase::Ending; break;
		}
		SceneInput.RealSec = Now;
		SceneInput.bStory = bFrameStory;
		SceneInput.bReachedCamp = bTourStarted;
		SceneInput.Panel = FramePanel;
		SceneInput.bInSafeZone = bFrameInSafeZone;
		SceneInput.bInCombat = bFrameInCombat;
		SceneInput.bBoss = bFrameBoss;
		SceneInput.BossId = FrameBossId;
		SceneInput.BossPhase = FrameBossPhase;
		Sample.Scene = SceneTracker.Update(SceneInput);

		// -- gauges (session frames)
		if (bFrameHadSim && GameWorld != nullptr)
		{
			const UAbyssCorpseLayer* Corpses = UAbyssCorpseLayer::Get(GameWorld);
			const UAbyssActorRegistry* Registry = UAbyssActorRegistry::Get(GameWorld);
			Sample.Husks = Corpses != nullptr ? static_cast<int16>(FMath::Min(Corpses->GetHuskCount(), 32767)) : int16(-1);
			Sample.Registry = Registry != nullptr ? static_cast<int16>(FMath::Min(Registry->Num(), 32767)) : int16(-1);
			Sample.Characters = static_cast<int16>(FMath::Min(FrameCharacters, 32767));
		}

		// -- feel: the commands this frame's sim acted on, presented now
		if (ProbeFrame.SimFrameEndSec > 0.0)
		{
			Commands.OnSimFrame(ProbeFrame.SimFrameEndSec - StartSeconds, FrameSteps, bFrameFrozen);
		}
		Commands.OnFrameEnd(Now);

		// -- a zone entry: what it cost and the compile backlog it met (warm or cold cache)
		if (ProbeFrame.Num(Rules::ECounter::ZoneBuilds) > 0)
		{
			FAbyssAutoTestZoneEntry& Entry = ZoneEntries.AddDefaulted_GetRef();
			Entry.Map = PendingZoneMap;
			Entry.Seconds = Now;
			Entry.FrameMs = Sample.FrameMs;
			Entry.BuildMs = ProbeFrame.Millis(Rules::ECounter::ZoneBuilds);
			Entry.Spawns = static_cast<int32>(ProbeFrame.Num(Rules::ECounter::Spawns));
			Entry.SpawnMs = ProbeFrame.Millis(Rules::ECounter::Spawns);
			Entry.ShaderJobs = Sample.ShaderJobs;
			Entry.AssetBuilds = Sample.AssetBuilds;
			ZoneWaitFramesLeft = 4;   // UAbyssWorldBuilder waits in the zone's first 3 frames
		}
		if (ZoneWaitFramesLeft > 0 && ZoneEntries.Num() > 0)
		{
			--ZoneWaitFramesLeft;
			FAbyssAutoTestZoneEntry& Entry = ZoneEntries.Last();
			Entry.CompileWaitMs += ProbeFrame.Millis(Rules::ECounter::CompileWaits);
			Entry.ShaderJobs = FMath::Max(Entry.ShaderJobs, Sample.ShaderJobs);
			Entry.AssetBuilds = FMath::Max(Entry.AssetBuilds, Sample.AssetBuilds);
		}

		ProbeTotals.Add(ProbeFrame);
		if (Samples.size() < MaxSamples)
		{
			Samples.push_back(Sample);
		}
		else
		{
			++SamplesDropped;
		}
		ProbeFrame.Reset();
		bFrameHadSim = false;
		FrameSteps = 0;
		FrameEvLogLines = 0;
		FrameCommands = 0;
	}

	/** The run's statistics and its smoothness verdict, once (Finish), before the report and the gate. */
	void SummarizePerf()
	{
		namespace Rules = AbyssSmoothnessRules;
		if (bPerfSummarized)
		{
			return;
		}
		bPerfSummarized = true;
		if (bStatUnitRequested)
		{
			// `stat unit` values read at a frame's end time the frame before it: put them next to their own frameMs
			// (longestFrames, frames.csv). Once: bPerfSummarized guards it.
			Rules::ShiftThreadTimesBack(Samples);
		}
		PerfSummary = Rules::Summarize(Samples, SceneTracker.Scenes(), SimLimits);
		Verdict = Rules::Judge(PerfSummary.InGame, Options.Smooth);
	}

	/** frames.csv next to the report: one row per frame, at most -abysstimelinerows rows (AbyssSmoothnessRules::PlanTimeline). */
	void WriteTimeline()
	{
		namespace Rules = AbyssSmoothnessRules;
		if (Options.TimelineRows <= 0 || Samples.empty())
		{
			return;
		}
		TimelinePlan = Rules::PlanTimeline(Samples, static_cast<std::size_t>(Options.TimelineRows));
		std::string Text = Rules::TimelineHeader();
		Text.reserve(Text.size() + TimelinePlan.Rows.size() * 160);
		const std::vector<Rules::FScene>& Scenes = SceneTracker.Scenes();
		for (const std::size_t Index : TimelinePlan.Rows)
		{
			const Rules::FFrameSample& Sample = Samples[Index];
			Rules::AppendTimelineRow(Text, Index, Sample, Sample.Scene < Scenes.size() ? std::string_view(Scenes[Sample.Scene].Label) : std::string_view());
		}
		const FString Path = Options.OutputDir / TEXT("frames.csv");
		bTimelineWritten = FFileHelper::SaveArrayToFile(
			TArrayView<const uint8>(reinterpret_cast<const uint8*>(Text.data()), static_cast<int32>(Text.size())), *Path);
	}

	/** `environment`: what the numbers were measured on. Console variables are read at the end of the run. */
	void WriteEnvironment(abyss::JsonWriter& W) const
	{
		const UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
		const auto Str = [&W](const char* Key, const FString& Value)
		{
			W.Key(Key);
			W.String(AbyssText::ToStd(Value));
		};
		W.Key("environment");
		W.StartObject();
		W.Key("cooked");
		W.Bool(FPlatformProperties::RequiresCookedData());
		W.Key("editorBinary");
		W.Bool(WITH_EDITOR != 0);
		Str("buildConfiguration", AbyssAutoTest_BuildConfiguration());
		// -- the quality tier and what the Auto rule read (AbyssPlatform::ResolveQualityTier)
		W.Key("quality");
		W.StartObject();
		Str("tier", GameInstance != nullptr ? FString(LexToString(GameInstance->GetQualityTier())) : FString());
		Str("setting", GameInstance != nullptr ? FString(LexToString(GameInstance->GetUserSettings().Quality)) : FString());
		Str("switch", AbyssAutoTest_SwitchValue(TEXT("abyssquality=")));
		Str("abyssQualityCVar", AbyssAutoTest_ConsoleValue(TEXT("abyss.Quality")));
		W.Key("mobile");
		W.Bool(AbyssPlatform::IsMobilePlatform());
		W.Key("cores");
		W.Int(FPlatformMisc::NumberOfCores());
		W.Key("memoryGB");
		W.Int(static_cast<int64>(FPlatformMemory::GetConstants().TotalPhysicalGB));
		W.Key("dpiScale");
		W.Double(std::round(static_cast<double>(FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(0.0f, 0.0f)) * 1000.0) / 1000.0);
		const UGameUserSettings* EngineSettings = GEngine != nullptr ? GEngine->GetGameUserSettings() : nullptr;
		W.Key("configuredResolution");
		if (EngineSettings != nullptr)
		{
			const FIntPoint Configured = EngineSettings->GetScreenResolution();
			W.StartArray();
			W.Int(Configured.X);
			W.Int(Configured.Y);
			W.EndArray();
		}
		else
		{
			W.Null();
		}
		W.EndObject();
		// -- the window
		const UGameViewportClient* ViewportClient = GameInstance != nullptr ? GameInstance->GetGameViewportClient() : nullptr;
		const FViewport* Viewport = ViewportClient != nullptr ? ViewportClient->Viewport : nullptr;
		W.Key("viewport");
		if (Viewport != nullptr)
		{
			const FIntPoint Size = Viewport->GetSizeXY();
			W.StartArray();
			W.Int(Size.X);
			W.Int(Size.Y);
			W.EndArray();
		}
		else
		{
			W.Null();
		}
		W.Key("viewportFullscreen");
		if (Viewport != nullptr)
		{
			W.Bool(Viewport->IsFullscreen());
		}
		else
		{
			W.Null();
		}
		W.Key("windowMode");
		if (EngineSettings != nullptr)
		{
			const EWindowMode::Type Mode = EngineSettings->GetFullscreenMode();
			W.String(Mode == EWindowMode::Fullscreen ? "fullscreen" : (Mode == EWindowMode::WindowedFullscreen ? "windowedFullscreen" : "windowed"));
		}
		else
		{
			W.Null();
		}
		W.Key("vsyncSetting");
		if (EngineSettings != nullptr)
		{
			W.Bool(EngineSettings->IsVSyncEnabled());
		}
		else
		{
			W.Null();
		}
		Str("gpu", AbyssAutoTest_GpuName<FPlatformMisc>());
		Str("cpu", AbyssAutoTest_CpuName<FPlatformMisc>());
		Str("rhi", RhiName);
		// -- console variables at the end of the run: a name this engine does not have is listed, never guessed
		W.Key("consoleVariables");
		W.StartObject();
		TArray<FString> MissingNames;
		for (const TCHAR* Name : GAbyssAutoTestConsoleVariables)
		{
			if (const IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name, /*bTrackFrequentCalls*/ false))
			{
				W.Key(AbyssText::ToStd(FString(Name)));
				W.String(AbyssText::ToStd(Variable->GetString()));
			}
			else
			{
				MissingNames.Add(Name);
			}
		}
		W.EndObject();
		W.Key("consoleVariablesMissing");
		W.StartArray();
		for (const FString& Name : MissingNames)
		{
			W.String(AbyssText::ToStd(Name));
		}
		W.EndArray();
		// -- warm or cold: the compile backlog each zone entry met (uncooked runs; null = this build gives no count)
		int32 PeakShaderJobs = -1;
		int32 PeakAssetBuilds = -1;
		for (const FAbyssAutoTestZoneEntry& Entry : ZoneEntries)
		{
			PeakShaderJobs = FMath::Max(PeakShaderJobs, Entry.ShaderJobs);
			PeakAssetBuilds = FMath::Max(PeakAssetBuilds, Entry.AssetBuilds);
		}
		Str("shaderCache", PeakShaderJobs < 0 && PeakAssetBuilds < 0 ? FString(TEXT("unknown"))
			: (PeakShaderJobs > 0 || PeakAssetBuilds > 0 ? FString(TEXT("cold")) : FString(TEXT("warm"))));
		W.EndObject();
	}

	/** `smoothness` and `perf`. */
	void WritePerf(abyss::JsonWriter& W) const
	{
		namespace Rules = AbyssSmoothnessRules;
		using Rules::ECounter;
		const auto Num = [&W](const char* Key, double Value)
		{
			W.Key(Key);
			W.Double(std::round(Value * 1000.0) / 1000.0);
		};
		const auto Int = [&W](const char* Key, int64 Value)
		{
			W.Key(Key);
			W.Int(Value);
		};
		const auto IntOrNull = [&W](const char* Key, int64 Value)
		{
			W.Key(Key);
			if (Value >= 0)
			{
				W.Int(Value);
			}
			else
			{
				W.Null();
			}
		};
		Rules::WriteSmoothness(W, Verdict, Options.Smooth, Options.bSmoothGate, Options.bNoShots || !FApp::CanEverRender());

		W.Key("perf");
		W.StartObject();
		Int("schema", 1);
		W.Key("shots");
		W.String(bCanShoot ? "on" : "off");
		Int("samples", static_cast<int64>(Samples.size()));
		Int("samplesDropped", SamplesDropped);
		W.Key("timeline");
		W.StartObject();
		W.Key("file");
		if (bTimelineWritten)
		{
			W.String("frames.csv");
		}
		else
		{
			W.Null();
		}
		Int("rows", static_cast<int64>(TimelinePlan.Rows.size()));
		Int("maxRows", Options.TimelineRows);
		Int("stride", static_cast<int64>(TimelinePlan.Stride));
		Int("slowFramesDropped", static_cast<int64>(TimelinePlan.SlowDropped));
		W.EndObject();
		W.Key("threadTimes");
		W.StartObject();
		W.Key("source");
		W.String(bStatUnitRequested
			? "stat unit (FStatUnitData raw values). They are read at the end of the frame after the one they time and written one frame back, "
			  "next to the frameMs they belong to (the run's last frame has none). The one-frame offset is the engine's as understood, not yet "
			  "confirmed on a run: on a zone-build frame gameMs should hold that frame's zoneBuildMs; the render and GPU values may lag one frame more"
			: "none (-abyssstatunit not given)");
		Int("framesWithThreadTimes", ThreadSamples);
		Int("framesWithGpuTime", GpuSamples);
		W.EndObject();
		Int("simMaxStepsPerFrame", SimLimits.MaxStepsPerFrame);
		Num("simMaxFrameMs", SimLimits.MaxFrameMs);

		W.Key("run");
		Rules::WriteGroup(W, PerfSummary.Run);
		W.Key("inGame");
		Rules::WriteGroup(W, PerfSummary.InGame);
		W.Key("last60s");
		Rules::WriteGroup(W, PerfSummary.Last);
		W.Key("fights");
		Rules::WriteGroup(W, PerfSummary.Fights);
		W.Key("field");
		Rules::WriteGroup(W, PerfSummary.Field);
		Int("fightCount", SceneTracker.Fights());
		W.Key("scenes");
		W.StartArray();
		for (const Rules::FGroup& Group : PerfSummary.Scenes)
		{
			Rules::WriteGroup(W, Group);
		}
		W.EndArray();

		std::vector<Rules::FMilestone> RuleMilestones;
		RuleMilestones.reserve(static_cast<std::size_t>(Milestones.Num()));
		for (const FAbyssAutoTestMilestone& Milestone : Milestones)
		{
			RuleMilestones.push_back(Rules::FMilestone{ AbyssText::ToStd(Milestone.Kind), AbyssText::ToStd(Milestone.Detail), Milestone.Seconds });
		}
		W.Key("longestFrames");   // any frame of the run: boot, loads, screenshots and story compete here
		W.StartArray();
		for (const std::size_t Index : PerfSummary.Longest)
		{
			Rules::WriteLongestFrame(W, Index, Samples[Index], SceneTracker.Scenes(), RuleMilestones);
		}
		W.EndArray();
		W.Key("longestPlayFrames");   // the in-game play frames only: the hitches the verdict judges
		W.StartArray();
		for (const std::size_t Index : PerfSummary.LongestPlay)
		{
			Rules::WriteLongestFrame(W, Index, Samples[Index], SceneTracker.Scenes(), RuleMilestones);
		}
		W.EndArray();

		// -- timers (ms are inclusive wall-clock scopes of the module's own code)
		W.Key("timers");
		W.StartObject();
		Num("menuIdleSec", MenuSeconds >= 0.0 && StartNewGameSeconds >= 0.0 ? StartNewGameSeconds - MenuSeconds : 0.0);
		Num("startNewGameSec", StartNewGameSeconds >= 0.0 && SessionSeconds >= 0.0 ? SessionSeconds - StartNewGameSeconds : 0.0);
		Num("startNewGameCallMs", StartNewGameCallMs);
		Int("zoneBuilds", static_cast<int64>(ProbeTotals.N[static_cast<int32>(ECounter::ZoneBuilds)]));
		Num("zoneBuildMs", ProbeTotals.Ms[static_cast<int32>(ECounter::ZoneBuilds)]);
		Int("spawns", static_cast<int64>(ProbeTotals.N[static_cast<int32>(ECounter::Spawns)]));
		Num("spawnMs", ProbeTotals.Ms[static_cast<int32>(ECounter::Spawns)]);
		Num("spawnPeakMsPerFrame", ProbeTotals.PeakMsPerFrame[static_cast<int32>(ECounter::Spawns)]);
		Int("compileWaits", static_cast<int64>(ProbeTotals.N[static_cast<int32>(ECounter::CompileWaits)]));
		Num("compileWaitSec", ProbeTotals.Ms[static_cast<int32>(ECounter::CompileWaits)] / 1000.0);
		Int("qualityRefreshCalls", static_cast<int64>(ProbeTotals.N[static_cast<int32>(ECounter::QualityRefreshCalls)]));
		Int("qualityRefreshApplied", static_cast<int64>(ProbeTotals.N[static_cast<int32>(ECounter::QualityRefreshApplied)]));
		Num("qualityRefreshMs", ProbeTotals.Ms[static_cast<int32>(ECounter::QualityRefreshCalls)]);
		Num("simMs", ProbeTotals.Ms[static_cast<int32>(ECounter::SimFrame)]);
		Num("driverMs", ProbeTotals.Ms[static_cast<int32>(ECounter::DriverTick)]);
		W.EndObject();

		// -- counters: every hook, as total / ms / the most in one frame
		W.Key("counters");
		W.StartObject();
		Int("evLogLines", TotalEvLogLines);
		for (int32 Index = 0; Index < Rules::CounterCount; ++Index)
		{
			W.Key(Rules::CounterName(static_cast<ECounter>(Index)));
			W.StartObject();
			Int("count", static_cast<int64>(ProbeTotals.N[Index]));
			Num("ms", ProbeTotals.Ms[Index]);
			Int("peakPerFrame", ProbeTotals.PeakPerFrame[Index]);
			Num("peakMsPerFrame", ProbeTotals.PeakMsPerFrame[Index]);
			W.EndObject();
		}
		W.Key("lastInputDevice");
		W.String(AbyssText::ToStd(LastInputDeviceName()));
		W.EndObject();

		W.Key("zoneEntries");
		W.StartArray();
		for (const FAbyssAutoTestZoneEntry& Entry : ZoneEntries)
		{
			W.StartObject();
			W.Key("map");
			W.String(AbyssText::ToStd(Entry.Map));
			Num("realSec", Entry.Seconds);
			Num("frameMs", Entry.FrameMs);
			Num("buildMs", Entry.BuildMs);
			Int("spawns", Entry.Spawns);
			Num("spawnMs", Entry.SpawnMs);
			Num("compileWaitMs", Entry.CompileWaitMs);
			IntOrNull("pendingShaderJobs", Entry.ShaderJobs);
			IntOrNull("pendingAssetBuilds", Entry.AssetBuilds);
			W.EndObject();
		}
		W.EndArray();

		// -- feel
		W.Key("feel");
		W.StartObject();
		W.Key("commandsNote");
		W.String("the bot's commands, handed to AAbyssPlayerController::SubmitCommand (not through the input layer's polling): ms from the "
			"submission to the GameSim::Frame that applies it, and to the end of that engine frame (render, GPU and display latency come on "
			"top). Worst-case phase by construction: the bot submits just after a frame's sim, so toSimStep is about one frame period and "
			"says nothing more than the frame time; a player's key, polled at the start of a frame, reaches that same frame's sim. Only a "
			"toSimStep well over one frame period (frames that ran no step) is latency the game adds");
		W.Key("commands");
		W.StartObject();
		for (int32 Index = -1; Index < Rules::CommandClassCount; ++Index)   // -1: every class together
		{
			const bool bAll = Index < 0;
			const Rules::ECommandClass Class = static_cast<Rules::ECommandClass>(bAll ? 0 : Index);
			const Rules::FValueStats ToSim = bAll ? Commands.AllSimStats() : Commands.SimStats(Class);
			const Rules::FValueStats ToFrame = bAll ? Commands.AllFrameStats() : Commands.FrameStats(Class);
			W.Key(bAll ? "all" : Rules::CommandClassName(Class));
			W.StartObject();
			Int("submitted", bAll ? CommandsSubmitted : static_cast<int64>(Commands.SubmittedCount(Class)));
			Int("measured", ToFrame.Count);
			const auto Latency = [&W](const char* Key, const Rules::FValueStats& Stats, double Median)
			{
				W.Key(Key);
				if (Stats.Count == 0)
				{
					W.Null();
					return;
				}
				W.StartObject();
				W.Key("p50Ms");
				W.Double(std::round(Median * 100.0) / 100.0);
				W.Key("avgMs");
				W.Double(std::round(Stats.AvgMs * 100.0) / 100.0);
				W.Key("p95Ms");
				W.Double(std::round(Stats.P95Ms * 100.0) / 100.0);
				W.Key("maxMs");
				W.Double(std::round(Stats.MaxMs * 100.0) / 100.0);
				W.EndObject();
			};
			Latency("toSimStep", ToSim, bAll ? Commands.AllMedianSimMs() : Commands.MedianSimMs(Class));
			Latency("toFrameEnd", ToFrame, bAll ? Commands.AllMedianFrameMs() : Commands.MedianFrameMs(Class));
			W.EndObject();
		}
		W.EndObject();
		const Rules::FContactTracker::FResult Contact = Contacts.Result();
		W.Key("attackContact");
		W.StartObject();
		Int("hits", Contact.Count);
		Int("unmatchedHits", Contact.Unmatched);
		const auto ContactMs = [&W, &Contact](const char* Key, double Value)
		{
			W.Key(Key);
			if (Contact.Count > 0)
			{
				W.Double(std::round(Value * 100.0) / 100.0);
			}
			else
			{
				W.Null();
			}
		};
		ContactMs("meanMs", Contact.MeanMs);
		ContactMs("p50Ms", Contact.P50Ms);
		ContactMs("p95AbsMs", Contact.P95AbsMs);
		ContactMs("maxAbsMs", Contact.MaxAbsMs);
		Int("lateOver33Ms", Contact.LateOver33);
		W.Key("note");
		W.String("shown pose's sim time minus the swing's contact time on the frame its hit is presented; "
			"about -17..0 ms is exact at 60 fps, positive = the pose is past the contact. A hit is matched only to a swing of its own "
			"kind (basic hit to basic swing, a skill's hit to that skill's swing) whose contact is at most the frame's step budget + 1 "
			"steps old; projectile hits, a swing's further targets and hits of cancelled swings are unmatchedHits");
		W.EndObject();
		W.EndObject();

		// -- what only the engine console can show (no stable C++ entry point to hedge): read by hand
		W.Key("operatorConsole");
		W.StartArray();
		for (const char* Step : GAbyssAutoTestOperatorSteps)
		{
			W.String(Step);
		}
		W.EndArray();
		W.EndObject();
	}

	FString LastInputDeviceName() const
	{
		const UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
		const UAbyssInputSubsystem* Input = GameInstance != nullptr ? GameInstance->GetSubsystem<UAbyssInputSubsystem>() : nullptr;
		return Input != nullptr ? FString(LexToString(Input->GetLastInputDevice())) : FString();
	}

	// ---- report.json ------------------------------------------------------------------------------------------------

	bool WriteReport(const FString& Path, int32 ExitCode) const
	{
		const UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
		const abyss::Snapshot* Snap = GameInstance != nullptr ? GameInstance->GetSnapshot() : nullptr;
		const bool bPassed = Result == EAbyssAutoTestResult::Passed;
		abyss::JsonWriter W(/*pretty*/ true);
		const auto Str = [&W](const char* Key, const FString& Value)
		{
			W.Key(Key);
			W.String(AbyssText::ToStd(Value));
		};
		const auto Num = [&W](const char* Key, double Value)
		{
			W.Key(Key);
			W.Double(std::round(Value * 1000.0) / 1000.0);
		};
		const auto Int = [&W](const char* Key, int64 Value)
		{
			W.Key(Key);
			W.Int(Value);
		};
		const auto Flag = [&W](const char* Key, bool bValue)
		{
			W.Key(Key);
			W.Bool(bValue);
		};

		W.StartObject();
		Flag("passed", bPassed);
		Str("result", AbyssAutoTest_ResultName(Result));
		Int("exitCode", ExitCode);
		Str("failure", FailureReason);
		Str("scenario", AbyssText::ToFString(abyss::EnumName(Options.Scenario)));
		Str("class", AbyssText::ToFString(abyss::EnumName(Options.HeroClass)));
		// A string: 64-bit values do not survive every JSON reader. Replay: -abyssseed=<seed>, or the core test with
		// ABYSS_PT_SWEEP=1 ABYSS_PT_SWEEP_BASE=<seed>.
		Str("seed", FString::Printf(TEXT("%llu"), static_cast<unsigned long long>(Options.Seed)));
		Str("buildSha", Options.BuildSha);
		Str("rhi", RhiName);
		Flag("screenshotsEnabled", bCanShoot);
		Flag("renderOffScreen", FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen")));
		Flag("nullRhi", !FApp::CanEverRender());
		Str("engineVersion", FEngineVersion::Current().ToString());
		Str("platform", FString(FPlatformProperties::IniPlatformName()));
		Str("outputDir", Options.OutputDir);
		Str("commandLine", FString(FCommandLine::Get()).Left(2000));
		Num("maxSec", Options.MaxSeconds);
		Num("shotIntervalSec", Options.ShotIntervalSeconds);
		Flag("noShots", Options.bNoShots);
		Num("maxFps", MaxFpsApplied);
		Flag("fixedTimeStep", FApp::UseFixedTimeStep());
		Flag("scenarioCompleted", ScenarioCompleted());
		// Audit F1: every console variable the module tried to set that this engine does not have (AbyssConsoleVars).
		// Empty on a healthy run; a name here is a setting that silently never applied.
		W.Key("missingConsoleVariables");
		W.StartArray();
		for (const FString& Name : AbyssConsoleVars::MissingNames())
		{
			W.String(AbyssText::ToStd(Name));
		}
		W.EndArray();

		W.Key("durations");
		W.StartObject();
		Num("totalSec", FinishedSeconds);
		Num("bootSec", MenuSeconds >= 0.0 ? MenuSeconds : FinishedSeconds);
		Num("menuToGameSec", MenuSeconds >= 0.0 && SessionSeconds >= 0.0 ? SessionSeconds - MenuSeconds : 0.0);
		// menuToGameSec split (audit F2): the test's own wait on the title screen (settle + screenshot), then the game's
		// time from the StartNewGame call to the session.
		Num("menuIdleSec", MenuSeconds >= 0.0 && StartNewGameSeconds >= 0.0 ? StartNewGameSeconds - MenuSeconds : 0.0);
		Num("startNewGameSec", StartNewGameSeconds >= 0.0 && SessionSeconds >= 0.0 ? SessionSeconds - StartNewGameSeconds : 0.0);
		Num("inGameSec", SessionSeconds >= 0.0 ? FinishedSeconds - SessionSeconds : 0.0);
		Num("simPlayTimeSec", Snap != nullptr ? Snap->playTimeMs / 1000.0 : 0.0);
		Num("simNowSec", Snap != nullptr ? Snap->simNowMs / 1000.0 : 0.0);
		W.EndObject();

		W.Key("pilot");
		W.StartObject();
		Flag("ok", Pilot.IsValid() && Pilot->Ok());
		Str("failure", Pilot.IsValid() ? AbyssText::ToFString(Pilot->Failure()) : FString());
		Str("checkpoint", Pilot.IsValid() ? AbyssText::ToFString(Pilot->CurrentCheckpoint()) : FString());
		Str("step", Pilot.IsValid() ? AbyssText::ToFString(Pilot->CurrentStep()) : FString());
		Int("frames", Pilot.IsValid() ? Pilot->Frames() : 0);
		Int("commands", CommandsSubmitted);
		W.Key("recentNotes");
		W.StartArray();
		if (Pilot.IsValid())
		{
			TArray<FString> Notes;
			AbyssText::ToFString(Pilot->RecentNotes()).ParseIntoArrayLines(Notes);
			for (int32 Index = FMath::Max(0, Notes.Num() - 25); Index < Notes.Num(); ++Index)
			{
				W.String(AbyssText::ToStd(Notes[Index]));
			}
		}
		W.EndArray();
		W.EndObject();

		if (Pilot.IsValid())
		{
			const abyss::AutoPilotStats& Stats = Pilot->Stats();
			W.Key("stats");
			W.StartObject();
			Int("kills", Stats.kills);
			Int("deaths", Stats.deaths);
			Int("levelUps", Stats.levelUps);
			Int("skillCasts", Stats.skillCasts);
			Int("potionsDrunk", Stats.potionsDrunk);
			Int("itemsPicked", Stats.itemsPicked);
			Int("itemsSold", Stats.itemsSold);
			Int("itemsBought", Stats.itemsBought);
			Int("equipChanges", Stats.equipChanges);
			Int("petFeeds", Stats.petFeeds);
			Int("escortFailures", Stats.escortFailures);
			Int("saveRequests", Stats.saveRequests);
			Int("storyBeats", static_cast<int64>(Stats.beatsEnded.size()));
			W.EndObject();
		}
		// 2G review X6: what the core announced and what the presentation drew (AbyssPresenceRules).
		W.Key("presentation");
		W.StartObject();
		Int("strikeTelegraphs", Presence.StrikeTelegraphs);
		Int("hazards", Presence.Hazards);
		Int("arenas", Presence.Arenas);
		Int("corpses", Presence.Corpses);
		Int("ashnumbFrames", Presence.AshnumbFrames);
		Int("peakDecals", Presence.PeakDecals);
		Int("peakArenaDecals", Presence.PeakArenaDecals);
		Int("peakHusks", Presence.PeakHusks);
		Int("bandFrames", Presence.BandFrames);
		Str("failure", PresenceFailure);
		W.EndObject();

		if (Snap != nullptr)
		{
			W.Key("hero");
			W.StartObject();
			Int("level", Snap->hero.level);
			Int("exp", Snap->hero.exp);
			Int("gold", Snap->hero.gold);
			Num("hp", Snap->hero.hp);
			Num("maxHp", Snap->hero.maxHp);
			Str("map", AbyssText::ToFString(Snap->zone.mapId));
			Flag("alive", Snap->hero.life == abyss::HeroLife::Alive);
			W.EndObject();
		}

		W.Key("milestonesReached");
		W.StartArray();
		TArray<FString> Reached;
		for (const FAbyssAutoTestMilestone& Milestone : Milestones)
		{
			if (!Reached.Contains(Milestone.Kind))
			{
				Reached.Add(Milestone.Kind);
				W.String(AbyssText::ToStd(Milestone.Kind));
			}
		}
		W.EndArray();
		W.Key("milestones");
		W.StartArray();
		for (const FAbyssAutoTestMilestone& Milestone : Milestones)
		{
			W.StartObject();
			Str("kind", Milestone.Kind);
			Str("detail", Milestone.Detail);
			Num("realSec", Milestone.Seconds);
			Num("simSec", Milestone.SimMs / 1000.0);
			W.EndObject();
		}
		W.EndArray();

		WriteFrameStats(W);
		WritePerf(W);
		WriteEnvironment(W);
		WriteLog(W);

		W.Key("screenshots");
		W.StartArray();
		for (const FAbyssAutoTestShot& Shot : Shots)
		{
			const int64 Bytes = IFileManager::Get().FileSize(*(Options.OutputDir / Shot.File));
			W.StartObject();
			Str("file", Shot.File);
			Str("tag", Shot.Tag);
			Str("caption", Shot.Caption);
			Num("realSec", Shot.RequestedSeconds >= 0.0 ? Shot.RequestedSeconds : Shot.DueSeconds);
			Num("simSec", Shot.SimMs / 1000.0);
			Flag("captured", Shot.bDone && !Shot.bTimedOut);
			Flag("exists", Bytes > 0);
			Int("bytes", FMath::Max<int64>(Bytes, 0));
			W.EndObject();
		}
		W.EndArray();
		W.Key("screenshotsSkipped");
		W.StartArray();
		for (const FString& Tag : SkippedShots)
		{
			W.String(AbyssText::ToStd(Tag));
		}
		W.EndArray();
		W.EndObject();

		const std::string Text = W.Take();
		if (Text.empty())
		{
			return false;
		}
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		return FFileHelper::SaveArrayToFile(TArrayView<const uint8>(reinterpret_cast<const uint8*>(Text.data()), static_cast<int32>(Text.size())), *Path);
	}

	void WriteFrameStats(abyss::JsonWriter& W) const
	{
		constexpr double HitchMs = 100.0;
		TArray<float> Sorted = FrameMs;
		Sorted.Sort();
		const auto Percentile = [&Sorted](double P) -> double
		{
			if (Sorted.Num() == 0)
			{
				return 0.0;
			}
			const int32 Index = FMath::Clamp(static_cast<int32>(FMath::CeilToDouble(P * Sorted.Num())) - 1, 0, Sorted.Num() - 1);
			return Sorted[Index];
		};
		double Sum = 0.0;
		int32 Hitches = 0;
		int32 Over50 = 0;
		for (const float FrameTime : Sorted)
		{
			Sum += FrameTime;
			Hitches += FrameTime > HitchMs ? 1 : 0;
			Over50 += FrameTime > 50.0f ? 1 : 0;
		}
		W.Key("frames");
		W.StartObject();
		W.Key("count");
		W.Int(Sorted.Num());
		const auto Ms = [&W](const char* Key, double Value)
		{
			W.Key(Key);
			W.Double(std::round(Value * 100.0) / 100.0);
		};
		Ms("avgMs", Sorted.Num() > 0 ? Sum / Sorted.Num() : 0.0);
		Ms("p50Ms", Percentile(0.50));
		Ms("p95Ms", Percentile(0.95));
		Ms("p99Ms", Percentile(0.99));
		Ms("maxMs", Sorted.Num() > 0 ? Sorted.Last() : 0.0);
		Ms("hitchThresholdMs", HitchMs);
		W.Key("hitches");
		W.Int(Hitches);
		W.Key("over50Ms");
		W.Int(Over50);
		W.Key("note");
		W.String("wall-clock time between engine frames while in game (FPlatformTime), zone loads, screenshots and story beats "
			"included: every frame, untagged, as before. The tagged statistics are in `perf` and `smoothness`");
		W.EndObject();
	}

	void WriteLog(abyss::JsonWriter& W) const
	{
		FAbyssAutoTestLogCapture::FCounts Counts;
		TArray<FAbyssAutoTestLogCapture::FLine> Problems;
		TArray<FAbyssAutoTestLogCapture::FLine> Warnings;
		if (LogCapture.IsValid())
		{
			LogCapture->Read(Counts, Problems, Warnings);
		}
		const auto Lines = [&W](const char* Key, const TArray<FAbyssAutoTestLogCapture::FLine>& List)
		{
			W.Key(Key);
			W.StartArray();
			for (const FAbyssAutoTestLogCapture::FLine& Line : List)
			{
				W.StartObject();
				W.Key("verbosity");
				W.String(AbyssText::ToStd(Line.Verbosity));
				W.Key("category");
				W.String(AbyssText::ToStd(Line.Category));
				W.Key("text");
				W.String(AbyssText::ToStd(Line.Text));
				W.Key("realSec");
				W.Double(std::round(Line.Seconds * 1000.0) / 1000.0);
				W.EndObject();
			}
			W.EndArray();
		};
		W.Key("log");
		W.StartObject();
		W.Key("errors");
		W.Int(Counts.Errors);
		W.Key("warnings");
		W.Int(Counts.Warnings);
		W.Key("ensures");
		W.Int(Counts.Ensures);
		W.Key("ignoredErrors");
		W.Int(Counts.IgnoredErrors);
		W.Key("ignoredCategories");
		W.StartArray();
		for (const FName& Category : Options.IgnoredLogCategories)
		{
			W.String(AbyssText::ToStd(Category.ToString()));
		}
		W.EndArray();
		W.Key("note");
		W.String("lines logged after the game instance started; Error and Ensure lines fail the run");
		Lines("messages", Problems);
		Lines("warningMessages", Warnings);
		W.EndObject();
	}

	// ---- state ------------------------------------------------------------------------------------------------------

	UAbyssAutoTestSubsystem& Owner;
	FAbyssAutoTestOptions Options;
	TUniquePtr<abyss::AutoPilot> Pilot;
	TUniquePtr<FAbyssAutoTestLogCapture> LogCapture;
	TUniquePtr<FAbyssAutoTestWatchdog> Watchdog;
	FRunnableThread* WatchdogThread = nullptr;

	EAbyssAutoTestPhase Phase = EAbyssAutoTestPhase::Boot;
	EAbyssAutoTestResult Result = EAbyssAutoTestResult::None;
	FString FailureReason;
	FString RhiName;
	float MaxFpsApplied = 0.0f;
	bool bCanShoot = false;
	bool bPilotFinished = false;
	int64 CommandsSubmitted = 0;

	double StartSeconds = 0.0;      // FPlatformTime at Start; every other time is relative to it
	double PhaseSeconds = 0.0;
	double MenuSeconds = -1.0;
	double SessionSeconds = -1.0;
	double FinishedSeconds = 0.0;
	double LastTickSeconds = -1.0;
	TArray<float> FrameMs;
	TArray<FAbyssAutoTestMilestone> Milestones;

	int32 HoldCount = 0;
	double HoldSince = -1.0;

	TArray<FAbyssAutoTestShot> Shots;
	TArray<FString> SkippedShots;
	int32 ShotInFlight = INDEX_NONE;
	int32 MenuShot = INDEX_NONE;
	bool bMenuShotQueued = false;
	int32 CampShot = INDEX_NONE;
	int32 TourShot = INDEX_NONE;
	bool bTourStarted = false;
	int32 TourIndex = INDEX_NONE;
	int32 TourStage = 0;
	double TourStageSeconds = 0.0;
	bool bShotQuestCard = false;
	bool bShotDialogue = false;
	bool bShotShop = false;
	bool bShotCombat = false;
	bool bShotBossFight = false;
	bool bShotLoot = false;
	double LootCheckSeconds = -1.0;
	uint32 StoryKindsShot = 0;
	uint32 SkillSlotsShot = 0;
	double NextPeriodicSeconds = 0.0;
	int32 PeriodicShots = 0;
	// ---- smoothness measurement (AbyssSmoothnessRules) ----
	static constexpr std::size_t MaxSamples = 300000;   // 83 minutes at 60 fps; later frames are counted, not kept
	AbyssSmoothnessRules::FProbeFrame ProbeFrame;       // GAbyssPerfProbe points here while the run lives
	AbyssSmoothnessRules::FProbeTotals ProbeTotals;
	AbyssSmoothnessRules::FSimLimits SimLimits;
	std::vector<AbyssSmoothnessRules::FFrameSample> Samples;
	int64 SamplesDropped = 0;
	AbyssSmoothnessRules::FSceneTracker SceneTracker;
	AbyssSmoothnessRules::FLoadingWindow Loading;
	AbyssSmoothnessRules::FCommandTracker Commands;
	AbyssSmoothnessRules::FContactTracker Contacts;
	AbyssSmoothnessRules::FSummary PerfSummary;
	AbyssSmoothnessRules::FVerdict Verdict;
	AbyssSmoothnessRules::FTimelinePlan TimelinePlan;
	bool bPerfSummarized = false;
	bool bTimelineWritten = false;
	bool bStatUnitRequested = false;
	int64 ThreadSamples = 0;
	int64 GpuSamples = 0;
	TArray<FAbyssAutoTestZoneEntry> ZoneEntries;
	int32 ZoneWaitFramesLeft = 0;
	FString PendingZoneMap;
	TArray<double> TimedReleases;
	double StartNewGameSeconds = -1.0;
	double StartNewGameCallMs = 0.0;
	int64 TotalEvLogLines = 0;
	int32 ShotTagFrames = 0;
	// the frame being sampled (set from the router's OnFrame and events, cleared by CloseFrame)
	bool bFrameHadSim = false;
	bool bFrameFrozen = false;
	bool bPrevFrozen = false;
	bool bFrameDilated = false;
	bool bFrameStory = false;
	bool bFrameInCombat = false;
	bool bFrameInSafeZone = false;
	bool bFrameBoss = false;
	int32 FrameSteps = 0;
	int32 FrameEvLogLines = 0;
	int32 FrameCommands = 0;
	int32 FrameCharacters = 0;
	int32 FrameBossPhase = 0;
	abyss::EntityId FrameBossEntity = abyss::kNoEntity;
	std::string FrameBossId;
	std::string FramePanel;
	std::string OpenTourPanel;

	/** 2G review X6: the presentation probe's tally and its first failure ("" = none). */
	AbyssPresenceRules::FTally Presence;
	FString PresenceFailure;
};

// =====================================================================================================================
// UAbyssAutoTestSubsystem
// =====================================================================================================================

bool UAbyssAutoTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (IsRunningCommandlet() || IsRunningDedicatedServer() || !AbyssAutoTest::IsRequested())
	{
		return false;
	}
	return Cast<UAbyssGameInstance>(Outer) != nullptr && Super::ShouldCreateSubsystem(Outer);
}

void UAbyssAutoTestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Panels are opened through the input layer's bridge.
	Collection.InitializeDependency<UAbyssInputSubsystem>();

	TestRun = MakeShared<FAbyssAutoTestRun>(*this, AbyssAutoTest::ParseOptions());
	TestRun->Start();
	if (UAbyssGameInstance* GameInstance = Cast<UAbyssGameInstance>(GetGameInstance()))
	{
		FAbyssEventRouter& Router = GameInstance->GetEventRouter();
		Router.OnAnyEvent.AddUObject(this, &UAbyssAutoTestSubsystem::HandleCoreEvent);
		Router.OnFrame.AddUObject(this, &UAbyssAutoTestSubsystem::HandleFrame);
		Router.OnSessionStarted.AddUObject(this, &UAbyssAutoTestSubsystem::HandleSessionStarted);
		Router.OnSessionEnded.AddUObject(this, &UAbyssAutoTestSubsystem::HandleSessionEnded);
		bRouterBound = true;
	}
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UAbyssAutoTestSubsystem::HandleTicker), 0.0f);
}

void UAbyssAutoTestSubsystem::Deinitialize()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
	if (bRouterBound)
	{
		if (UAbyssGameInstance* GameInstance = Cast<UAbyssGameInstance>(GetGameInstance()))
		{
			GameInstance->GetEventRouter().RemoveAll(this);
		}
		bRouterBound = false;
	}
	if (TestRun.IsValid())
	{
		TestRun->Shutdown();
		TestRun.Reset();
	}
	Super::Deinitialize();
}

bool UAbyssAutoTestSubsystem::HandleTicker(float DeltaTime)
{
	if (TestRun.IsValid())
	{
		TestRun->Tick();
	}
	return true;
}

void UAbyssAutoTestSubsystem::HandleSessionStarted()
{
	if (TestRun.IsValid())
	{
		TestRun->HandleSessionStarted();
	}
}

void UAbyssAutoTestSubsystem::HandleSessionEnded()
{
	if (TestRun.IsValid())
	{
		TestRun->HandleSessionEnded();
	}
}

void UAbyssAutoTestSubsystem::HandleCoreEvent(const abyss::Event& Event)
{
	if (TestRun.IsValid())
	{
		TestRun->HandleCoreEvent(Event);
	}
}

void UAbyssAutoTestSubsystem::HandleFrame(const abyss::Snapshot& Snap, const FAbyssFrameInfo& Frame)
{
	if (TestRun.IsValid())
	{
		TestRun->HandleFrame(Snap, Frame);
	}
}
