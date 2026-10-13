// UAbyssAutoTestSubsystem (Framework/AbyssAutoTestSubsystem.h): the in-game autotest. Everything lives in
// FAbyssAutoTestRun; the subsystem only exists when -abyssautotest is on the command line.
#include "Framework/AbyssAutoTestSubsystem.h"

#include "Abyssfire.h"
#include "DynamicRHI.h"
#include "HAL/CriticalSection.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
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
#include <variant>

#include "abyss/base/Json.h"
#include "abyss/base/Math.h"
#include "abyss/data/DataStore.h"
#include "abyss/items/Shop.h"
#include "abyss/quests/Dialogue.h"
#include "abyss/quests/QuestWorld.h"
#include "abyss/tools/AutoPilot.h"

#include "Framework/AbyssAutoTest.h"
#include "Framework/AbyssEventRouter.h"
#include "Framework/AbyssGameInstance.h"
#include "Framework/AbyssPlayerController.h"
#include "Framework/AbyssText.h"
#include "Input/AbyssInputSubsystem.h"
#include "Input/AbyssInputTypes.h"

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
		if (IConsoleVariable* MaxFps = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))
		{
			const float Current = MaxFps->GetFloat();
			if (Current <= 0.0f || Current > 60.0f)
			{
				MaxFps->Set(60.0f, ECVF_SetByCode);
			}
			MaxFpsApplied = MaxFps->GetFloat();
		}

		bCanShoot = FApp::CanEverRender();
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
			bCanShoot ? TEXT("on") : TEXT("off (no RHI renders: -nullrhi)"), MaxFpsApplied, *Options.OutputDir);
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
			Files.Delete(*(Options.OutputDir / TEXT("watchdog.txt")), false, true, true);
		}
		else if (Files.DirectoryExists(*SaveDir) || Files.DirectoryExists(*ShotDir))
		{
			Options.Problems.Add(FString::Printf(TEXT("%s has a SaveGames/ or screenshots/ directory that no autotest run left (no %s): nothing deleted; use an empty directory"),
				*Options.OutputDir, AbyssAutoTest_MarkerFileName));
			return false;
		}
		FFileHelper::SaveStringToFile(FString(TEXT("Abyssfire autotest output (-abyssautotest): the next run here deletes SaveGames/, screenshots/, report.json and watchdog.txt.\n")), *Marker);
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
	}

	// ---- per engine frame (core ticker) -----------------------------------------------------------------------------

	void Tick()
	{
		const double Now = RunSeconds();
		if (LastTickSeconds >= 0.0 && (Phase == EAbyssAutoTestPhase::Playing || Phase == EAbyssAutoTestPhase::Ending))
		{
			FrameMs.Add(static_cast<float>((Now - LastTickSeconds) * 1000.0));
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
				// The main menu's own call (SAbyssMainMenu::StartGame): slot 0 of the isolated save directory, Normal.
				if (!GameInstance->StartNewGame(Options.HeroClass, 0, abyss::Difficulty::Normal))
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
		if (Phase != EAbyssAutoTestPhase::Playing)
		{
			return;
		}
		const double Now = RunSeconds();
		if (const abyss::EvStoryBeat* Beat = std::get_if<abyss::EvStoryBeat>(&Event))
		{
			// The first beat of each kind (prologue, chapter card, cutscene, boss intro) plays a moment for its picture
			// before the bot skips it.
			const uint32 Bit = 1u << static_cast<uint32>(Beat->kind);
			if (Beat->phase == abyss::EvStoryBeat::Phase::Began && bCanShoot && (StoryKindsShot & Bit) == 0)
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
		if (!Pilot.IsValid() || (Phase != EAbyssAutoTestPhase::Playing && Phase != EAbyssAutoTestPhase::Ending))
		{
			return;
		}
		Pilot->EndFrame(Snap, Frame.StepsThisFrame);
		if (Phase == EAbyssAutoTestPhase::Playing)
		{
			const double Now = RunSeconds();
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
	}

	/** Commands of the bot take the input layer's path: AAbyssPlayerController::SubmitCommand (GI->Submit without one). */
	void SubmitAsInput(const abyss::Command& Command)
	{
		++CommandsSubmitted;
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
				ReleaseHold();
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
		if (bCanShoot)
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
			if (ShotDone(TourShot) || Now - TourStageSeconds > 15.0)
			{
				PressPanel(TourIndex);
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
		TourIndex = INDEX_NONE;
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
		if (!ScenarioCompleted())
		{
			Finish(EAbyssAutoTestResult::Failed, FailureReason.IsEmpty() ? FString(TEXT("the scenario did not complete")) : FailureReason);
		}
		else if (Counts.Errors > 0 || Counts.Ensures > 0)
		{
			Finish(EAbyssAutoTestResult::Failed, FString::Printf(TEXT("the scenario completed, but the log has %d Error and %d Ensure line(s)"),
				Counts.Errors, Counts.Ensures));
		}
		else
		{
			Finish(EAbyssAutoTestResult::Passed, FString());
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
		[[maybe_unused]] const bool bWritten = WriteReport(ReportPath, ExitCode);
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
		Num("maxFps", MaxFpsApplied);
		Flag("fixedTimeStep", FApp::UseFixedTimeStep());
		Flag("scenarioCompleted", ScenarioCompleted());

		W.Key("durations");
		W.StartObject();
		Num("totalSec", FinishedSeconds);
		Num("bootSec", MenuSeconds >= 0.0 ? MenuSeconds : FinishedSeconds);
		Num("menuToGameSec", MenuSeconds >= 0.0 && SessionSeconds >= 0.0 ? SessionSeconds - MenuSeconds : 0.0);
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
		W.String("wall-clock time between engine frames while in game (FPlatformTime), zone loads included");
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
