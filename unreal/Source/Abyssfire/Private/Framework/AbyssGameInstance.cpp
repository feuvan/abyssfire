#include "Framework/AbyssGameInstance.h"

#include "Abyssfire.h"
#include "Async/Async.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CoreDelegates.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include <utility>
#include <variant>

#include "abyss/data/DataStore.h"
#include "abyss/sim/GameSim.h"

#include "Framework/AbyssSimDriver.h"
#include "Framework/AbyssText.h"
#include "Platform/AbyssPlatform.h"
#include "Platform/AbyssSaveStorage.h"

UAbyssGameInstance::UAbyssGameInstance() = default;

UAbyssGameInstance::~UAbyssGameInstance() = default;

UAbyssGameInstance* UAbyssGameInstance::Get(const UObject* WorldContextObject)
{
	const UWorld* OwningWorld = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return OwningWorld ? Cast<UAbyssGameInstance>(OwningWorld->GetGameInstance()) : nullptr;
}

// =====================================================================================================================
// Init / Shutdown
// =====================================================================================================================

void UAbyssGameInstance::Init()
{
	Super::Init();
	check(IsInGameThread());

	SaveStorage = MakeUnique<FAbyssSaveStorage>();
	LoadSettings();
	LoadData();
	ApplySettingsSideEffects(FAbyssUserSettings(), /*bInitial*/ true);

	if (!bLifecycleBound)
	{
		// ue58-platform.md 13: pause + save when the app is backgrounded, resume on return. On phones / tablets a
		// deactivation (call, control centre) pauses too; desktop keeps running when another window has focus (web parity:
		// the page only muted audio when hidden), so deactivation is not bound there. Terminate may never fire on mobile -
		// the background save is the one that counts.
		if (AbyssPlatform::IsMobilePlatform())
		{
			FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this, &UAbyssGameInstance::HandleAppWillEnterBackground);
			FCoreDelegates::ApplicationHasReactivatedDelegate.AddUObject(this, &UAbyssGameInstance::HandleAppHasEnteredForeground);
		}
		FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(this, &UAbyssGameInstance::HandleAppWillEnterBackground);
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddUObject(this, &UAbyssGameInstance::HandleAppHasEnteredForeground);
		FCoreDelegates::GetApplicationWillTerminateDelegate().AddUObject(this, &UAbyssGameInstance::HandleAppWillTerminate);
		bLifecycleBound = true;
	}

	UE_LOG(LogAbyss, Log, TEXT("GameInstance ready: state %s, touch %d, quality %s, saves in %s"),
		LexToString(AppState), bTouchMode ? 1 : 0, LexToString(QualityTier),
		SaveStorage ? *SaveStorage->GetSaveDir() : TEXT("-"));
}

void UAbyssGameInstance::Shutdown()
{
	if (bLifecycleBound)
	{
		FCoreDelegates::ApplicationWillDeactivateDelegate.RemoveAll(this);
		FCoreDelegates::ApplicationWillEnterBackgroundDelegate.RemoveAll(this);
		FCoreDelegates::ApplicationHasReactivatedDelegate.RemoveAll(this);
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.RemoveAll(this);
		FCoreDelegates::GetApplicationWillTerminateDelegate().RemoveAll(this);
		bLifecycleBound = false;
	}

	DeferredFlow.Reset();
	if (Sim)
	{
		// Window close / editor stop: finish a pending death (C12) and save synchronously.
		if (Sim->HasSession())
		{
			Sim->Submit(abyss::CmdResolvePendingDeath{});
			Sim->Step();
			SaveNow(/*bSync*/ true);
		}
		// The game world is being torn down already: do not ask it to clear actors.
		EndSession(/*bNotifyWorld*/ false);
	}
	if (SaveStorage)
	{
		SaveStorage->Flush();
		SaveStorage.Reset();
	}
	UiRoot.Reset();
	Store.reset();   // the GameSim (which references the store) is gone already
	Super::Shutdown();
}

// =====================================================================================================================
// Data and settings
// =====================================================================================================================

void UAbyssGameInstance::LoadData()
{
	// ue58-platform.md 10.2: UE reads bytes (pak-aware), the core parses. Data/ is staged UFS (Abyssfire.Build.cs).
	const FString DataDir = FPaths::ProjectDir() / TEXT("Data");
	abyss::DataFileReader Reader = [&DataDir](std::string_view FileName, std::string& OutBytes) -> bool
	{
		const FString Path = DataDir / AbyssText::ToFString(FileName);
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent))
		{
			return false;
		}
		OutBytes.assign(reinterpret_cast<const char*>(Bytes.GetData()), static_cast<size_t>(Bytes.Num()));
		return true;
	};

	const double StartSeconds = FPlatformTime::Seconds();
	std::unique_ptr<abyss::DataStore> NewStore = std::make_unique<abyss::DataStore>();
	abyss::DataLoadReport Report;
	const bool bLoaded = NewStore->LoadAll(Reader, Report);
	const double ElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;

	for (const abyss::DataIssue& Issue : Report.warnings)
	{
		UE_LOG(LogAbyss, Warning, TEXT("Data warning %s %s: %s"), *AbyssText::ToFString(Issue.file),
			*AbyssText::ToFString(Issue.path), *AbyssText::ToFString(Issue.message));
	}
	if (!bLoaded || !Report.Ok() || !NewStore->IsFinalized())
	{
		DataError = AbyssText::ToFString(Report.Summary());
		UE_LOG(LogAbyss, Error, TEXT("Data load FAILED (%d errors) from %s:\n%s"), static_cast<int32>(Report.errors.size()),
			*DataDir, *DataError);
		// A data error is fatal for the game (nothing can start); development builds also raise an ensure.
		ensureMsgf(false, TEXT("Abyssfire data tables failed to load - see LogAbyss"));
		Store.reset();
		SetAppState(EAbyssAppState::DataError);
		return;
	}

	Store = std::move(NewStore);
	DataError.Reset();
	// Smoke test for staging on every device (ue58-platform.md 10.1).
	UE_LOG(LogAbyss, Log, TEXT("Loaded %d data files from %s in %.1f ms"), static_cast<int32>(Report.loadedFiles.size()),
		*DataDir, ElapsedMs);
}

void UAbyssGameInstance::LoadSettings()
{
	UserSettings = FAbyssUserSettings();
	std::string Json;
	if (SaveStorage && SaveStorage->ReadSettings(Json))
	{
		if (!FAbyssUserSettings::FromJson(Json, UserSettings))
		{
			UE_LOG(LogAbyss, Warning, TEXT("settings.json is not a JSON object; using defaults"));
		}
	}
	UserSettings.Sanitize();
}

void UAbyssGameInstance::ApplyUserSettings(const FAbyssUserSettings& NewSettings)
{
	check(IsInGameThread());
	const FAbyssUserSettings Previous = UserSettings;
	UserSettings = NewSettings;
	UserSettings.Sanitize();
	if (SaveStorage && !SaveStorage->WriteSettings(UserSettings.ToJson()))
	{
		UE_LOG(LogAbyss, Warning, TEXT("Could not write settings.json"));
	}
	ApplySettingsSideEffects(Previous, /*bInitial*/ false);
	OnSettingsChanged.Broadcast(UserSettings);
}

void UAbyssGameInstance::ApplySettingsSideEffects(const FAbyssUserSettings& Previous, bool bInitial)
{
	const bool bTouchBefore = bTouchMode;
	bTouchMode = AbyssPlatform::ResolveTouchMode(UserSettings.ControlLayout);
	QualityTier = AbyssPlatform::ResolveQualityTier(UserSettings.Quality);

	if (Store && (bInitial || Previous.Locale != UserSettings.Locale))
	{
		Store->MutableStrings().SetLocale(UserSettings.Locale);
		if (!bInitial)
		{
			OnLocaleChanged.Broadcast();
			if (IAbyssUiRoot* Root = GetUiRoot())
			{
				Root->OnLocaleChanged();
			}
		}
	}
	if (!bInitial && bTouchBefore != bTouchMode && HasSession())
	{
		Submit(abyss::CmdSetTouchMode{bTouchMode});   // U7: HUD panels pause the sim on touch
	}
}

bool UAbyssGameInstance::IsDataReady() const
{
	return Store != nullptr && Store->IsFinalized();
}

const abyss::DataStore* UAbyssGameInstance::GetData() const
{
	return Store.get();
}

const abyss::I18n& UAbyssGameInstance::GetStrings() const
{
	check(Store);
	return Store->Strings();
}

FText UAbyssGameInstance::Localize(const abyss::LocText& Text) const
{
	return Store ? AbyssText::Localize(Store->Strings(), Text) : AbyssText::ToText(Text.key);
}

FText UAbyssGameInstance::Localize(std::string_view Key) const
{
	return Store ? AbyssText::Localize(Store->Strings(), Key) : AbyssText::ToText(Key);
}

FText UAbyssGameInstance::LocalizeOr(std::string_view Key, const TCHAR* EnglishFallback) const
{
	// Backbone messages use core i18n keys (all player text comes from Data/i18n_*.json); the English fallback covers a
	// missing key and the data-error case where no table could be loaded.
	if (Store && Store->Strings().Has(Key))
	{
		return AbyssText::Localize(Store->Strings(), Key);
	}
	return FText::AsCultureInvariant(FString(EnglishFallback));
}

// =====================================================================================================================
// App state, registration
// =====================================================================================================================

void UAbyssGameInstance::SetAppState(EAbyssAppState NewState)
{
	if (AppState == NewState)
	{
		return;
	}
	UE_LOG(LogAbyss, Log, TEXT("App state %s -> %s"), LexToString(AppState), LexToString(NewState));
	AppState = NewState;
	AbyssPlatform::SetScreenSaverAllowed(NewState != EAbyssAppState::InGame);
	OnAppStateChanged.Broadcast(NewState);
	if (IAbyssUiRoot* Root = GetUiRoot())
	{
		Root->OnAppStateChanged(NewState);
	}
}

void UAbyssGameInstance::NotifyGameWorldReady(UWorld* World)
{
	// The viewport exists from here on: the UI root can show the title screen (or the data error).
	if (AppState == EAbyssAppState::Boot)
	{
		SetAppState(IsDataReady() ? EAbyssAppState::MainMenu : EAbyssAppState::DataError);
	}
	if (AppState == EAbyssAppState::DataError)
	{
		ReportError(LocalizeOr("ui.error.dataTitle", TEXT("Data error")), FText::FromString(DataError));
	}
}

void UAbyssGameInstance::RegisterUiRoot(IAbyssUiRoot* Root)
{
	check(IsInGameThread());
	UiRoot = TWeakInterfacePtr<IAbyssUiRoot>(Root);
	if (Root != nullptr)
	{
		Root->OnAppStateChanged(AppState);
		if (AppState == EAbyssAppState::DataError)
		{
			Root->ShowSystemError(LocalizeOr("ui.error.dataTitle", TEXT("Data error")), FText::FromString(DataError));
		}
	}
}

void UAbyssGameInstance::UnregisterUiRoot(IAbyssUiRoot* Root)
{
	if (UiRoot.Get() == Root)
	{
		UiRoot.Reset();
	}
}

IAbyssUiRoot* UAbyssGameInstance::GetUiRoot() const
{
	return UiRoot.Get();
}

void UAbyssGameInstance::ReportError(const FText& Title, const FText& Message)
{
	UE_LOG(LogAbyss, Error, TEXT("%s: %s"), *Title.ToString(), *Message.ToString());
	if (IAbyssUiRoot* Root = GetUiRoot())
	{
		Root->ShowSystemError(Title, Message);
	}
}

// =====================================================================================================================
// Session
// =====================================================================================================================

bool UAbyssGameInstance::HasSession() const
{
	return Sim != nullptr && Sim->HasSession();
}

abyss::GameSim* UAbyssGameInstance::GetSim() const
{
	return Sim.get();
}

const abyss::Snapshot* UAbyssGameInstance::GetSnapshot() const
{
	return HasSession() ? &Sim->View() : nullptr;
}

void UAbyssGameInstance::Submit(const abyss::Command& Command)
{
	check(IsInGameThread());
	if (Sim)
	{
		Sim->Submit(Command);
	}
}

abyss::SimConfig UAbyssGameInstance::MakeSimConfig() const
{
	abyss::SimConfig Config;
	Config.touchMode = bTouchMode;
	Config.milestone1 = true;   // U6 / Q3
#if UE_BUILD_SHIPPING
	Config.enableDebugCommands = false;
#else
	Config.enableDebugCommands = true;
#endif
	return Config;
}

UAbyssSimDriver* UAbyssGameInstance::FindDriver() const
{
	const UWorld* CurrentWorld = GetWorld();
	return CurrentWorld ? CurrentWorld->GetSubsystem<UAbyssSimDriver>() : nullptr;
}

bool UAbyssGameInstance::IsDispatching() const
{
	const UAbyssSimDriver* Driver = FindDriver();
	return Driver != nullptr && Driver->IsDispatching();
}

bool UAbyssGameInstance::DeferIfDispatching(TFunction<void()> Fn)
{
	if (!IsDispatching())
	{
		return false;
	}
	UE_LOG(LogAbyss, Verbose, TEXT("Flow call during event dispatch: deferred to the end of the dispatch"));
	DeferredFlow.Add(MoveTemp(Fn));
	return true;
}

void UAbyssGameInstance::RunDeferredFlow()
{
	while (DeferredFlow.Num() > 0)
	{
		TArray<TFunction<void()>> Pending = MoveTemp(DeferredFlow);
		DeferredFlow.Reset();
		for (TFunction<void()>& Fn : Pending)
		{
			Fn();
		}
	}
}

void UAbyssGameInstance::BeginSession(std::unique_ptr<abyss::GameSim> NewSim, int32 Slot)
{
	Sim = std::move(NewSim);
	ActiveSlot = Slot;
	bSaveRequested = false;
	SaveRequestReason.Reset();
	// NewGame / LoadGame emitted the zone entry (spawns, EvZone, music, chapter card, save request): the driver
	// dispatches them on its next tick, before the first GameSim::Frame (which would clear them).
	bSessionEventsPending = true;
	if (bInBackground)
	{
		Sim->Submit(abyss::CmdAppBackground{true});
	}
	EventRouter.OnSessionStarted.Broadcast();
	SetAppState(EAbyssAppState::InGame);
}

void UAbyssGameInstance::EndSession(bool bNotifyWorld)
{
	if (!Sim)
	{
		return;
	}
	EventRouter.OnSessionEnded.Broadcast();
	if (UAbyssSimDriver* Driver = bNotifyWorld ? FindDriver() : nullptr)
	{
		Driver->EndSession();
	}
	Sim.reset();
	ActiveSlot = INDEX_NONE;
	bSessionEventsPending = false;
	bSaveRequested = false;
	SaveRequestReason.Reset();
}

void UAbyssGameInstance::StepImmediately()
{
	if (!Sim)
	{
		return;
	}
	if (UAbyssSimDriver* Driver = FindDriver())
	{
		Driver->StepNow();
		return;
	}
	// No game world (shutdown path): step and honour a save request without presentation.
	Sim->Step();
	for (const abyss::Event& Event : Sim->Events())
	{
		if (const abyss::EvSaveRequested* Request = std::get_if<abyss::EvSaveRequested>(&Event))
		{
			NotifySaveRequested(Request->reason);
		}
	}
	FlushRequestedSave();
}

bool UAbyssGameInstance::StartNewGame(abyss::ClassId ClassId, int32 Slot, abyss::Difficulty Difficulty)
{
	check(IsInGameThread());
	if (DeferIfDispatching([this, ClassId, Slot, Difficulty]() { StartNewGame(ClassId, Slot, Difficulty); }))
	{
		return true;
	}
	if (!IsDataReady() || !FAbyssSaveStorage::IsValidSlot(Slot))
	{
		UE_LOG(LogAbyss, Error, TEXT("StartNewGame refused (data ready %d, slot %d)"), IsDataReady() ? 1 : 0, Slot);
		return false;
	}
	if (Sim)
	{
		ReturnToMainMenu();
	}

	std::unique_ptr<abyss::GameSim> NewSim = abyss::GameSim::Create(*Store, MakeSimConfig());
	if (!NewSim)
	{
		ReportError(LocalizeOr("ui.error.startFailed", TEXT("Cannot start")),
			LocalizeOr("ui.error.coreCreateFailed", TEXT("The game core could not be created (data not finalized).")));
		return false;
	}
	// Seed: S3 deterministic streams, a fresh seed per new game (0 would mean the config default).
	uint64 Seed = FPlatformTime::Cycles64() ^ static_cast<uint64>(FDateTime::UtcNow().GetTicks());
	if (Seed == 0)
	{
		Seed = 1;
	}
	if (!NewSim->NewGame(ClassId, Difficulty, Seed, Slot))
	{
		ReportError(LocalizeOr("ui.error.startFailed", TEXT("Cannot start")),
			LocalizeOr("ui.error.newGameFailed", TEXT("The new game could not be created (unknown class or missing default map).")));
		return false;
	}
	UE_LOG(LogAbyss, Log, TEXT("New game: class %s, difficulty %s, slot %d"),
		*AbyssText::ToFString(abyss::EnumName(ClassId)), *AbyssText::ToFString(abyss::EnumName(Difficulty)), Slot);
	BeginSession(std::move(NewSim), Slot);
	return true;
}

FAbyssLoadResult UAbyssGameInstance::ContinueSlot(int32 Slot, std::optional<abyss::Difficulty> DifficultyOverride, bool bFromBackup)
{
	check(IsInGameThread());
	FAbyssLoadResult Result;
	if (DeferIfDispatching([this, Slot, DifficultyOverride, bFromBackup]()
		{
			const FAbyssLoadResult Deferred = ContinueSlot(Slot, DifficultyOverride, bFromBackup);
			if (!Deferred.bOk)
			{
				ReportError(LocalizeOr("ui.error.loadFailed", TEXT("Cannot load")), FText::FromString(Deferred.Message));
			}
		}))
	{
		Result.bDeferred = true;
		return Result;
	}
	if (!IsDataReady() || !SaveStorage || !FAbyssSaveStorage::IsValidSlot(Slot))
	{
		Result.Error = abyss::SaveError::Invalid;
		Result.Message = TEXT("data not ready or invalid slot");
		return Result;
	}
	if (Sim)
	{
		ReturnToMainMenu();
	}

	std::string Json;
	const bool bRead = bFromBackup ? SaveStorage->ReadBackup(Slot, Json) : SaveStorage->Read(Slot, Json);
	if (!bRead)
	{
		Result.bSlotMissing = true;
		Result.bBackupAvailable = !bFromBackup && SaveStorage->HasBackup(Slot);
		Result.Message = FString::Printf(TEXT("slot %d has no %s file"), Slot, bFromBackup ? TEXT("backup") : TEXT("save"));
		return Result;
	}

	std::unique_ptr<abyss::GameSim> NewSim = abyss::GameSim::Create(*Store, MakeSimConfig());
	if (!NewSim)
	{
		Result.Error = abyss::SaveError::Invalid;
		Result.Message = TEXT("GameSim::Create failed");
		return Result;
	}
	std::string Error;
	const abyss::SaveError LoadError = NewSim->LoadGame(Json, &Error, DifficultyOverride);
	if (LoadError != abyss::SaveError::None)
	{
		Result.Error = LoadError;
		Result.Message = FString::Printf(TEXT("%s: %s"), *AbyssText::ToFString(abyss::EnumName(LoadError)), *AbyssText::ToFString(Error));
		Result.bBackupAvailable = !bFromBackup && LoadError != abyss::SaveError::VersionTooNew && SaveStorage->HasBackup(Slot);
		UE_LOG(LogAbyss, Warning, TEXT("Loading slot %d%s failed: %s"), Slot, bFromBackup ? TEXT(" (backup)") : TEXT(""), *Result.Message);
		return Result;
	}
	UE_LOG(LogAbyss, Log, TEXT("Loaded slot %d%s"), Slot, bFromBackup ? TEXT(" (backup)") : TEXT(""));
	BeginSession(std::move(NewSim), Slot);
	Result.bOk = true;
	return Result;
}

void UAbyssGameInstance::ReturnToMainMenu()
{
	check(IsInGameThread());
	if (DeferIfDispatching([this]() { ReturnToMainMenu(); }))
	{
		return;
	}
	if (Sim)
	{
		if (Sim->HasSession())
		{
			// save-ui-input 3.4 rule 3 / C12: finish a Dying hero's respawn now, then save.
			Sim->Submit(abyss::CmdResolvePendingDeath{});
			StepImmediately();
			SaveNow(/*bSync*/ false);
		}
		EndSession();
	}
	if (AppState != EAbyssAppState::DataError)
	{
		SetAppState(EAbyssAppState::MainMenu);
	}
}

void UAbyssGameInstance::QuitGame()
{
	check(IsInGameThread());
	if (DeferIfDispatching([this]() { QuitGame(); }))
	{
		return;
	}
	if (Sim && Sim->HasSession())
	{
		Sim->Submit(abyss::CmdResolvePendingDeath{});
		StepImmediately();
		SaveNow(/*bSync*/ true);
	}
	if (SaveStorage)
	{
		SaveStorage->Flush();
	}
	UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, /*bIgnorePlatformRestrictions*/ false);
}

bool UAbyssGameInstance::DeleteSlot(int32 Slot)
{
	check(IsInGameThread());
	if (!SaveStorage || !FAbyssSaveStorage::IsValidSlot(Slot) || (HasSession() && Slot == ActiveSlot))
	{
		return false;
	}
	return SaveStorage->Remove(Slot);
}

void UAbyssGameInstance::ListSlots(std::vector<abyss::SaveSlotInfo>& Out)
{
	check(IsInGameThread());
	Out.clear();
	if (SaveStorage)
	{
		SaveStorage->List(Out);
	}
}

// =====================================================================================================================
// Saves
// =====================================================================================================================

int64 UAbyssGameInstance::UnixMsNow()
{
	const FDateTime Now = FDateTime::UtcNow();
	return Now.ToUnixTimestamp() * 1000 + Now.GetMillisecond();
}

bool UAbyssGameInstance::SaveNow(bool bSync)
{
	check(IsInGameThread());
	if (!HasSession() || !SaveStorage || !FAbyssSaveStorage::IsValidSlot(ActiveSlot))
	{
		return false;
	}
	// A Dying hero is saved as respawned at the camp by the core (C12); callers resolve the death first when they can.
	const std::string Json = Sim->SaveGame(UnixMsNow());
	if (Json.empty())
	{
		UE_LOG(LogAbyss, Error, TEXT("SaveGame returned nothing (slot %d)"), ActiveSlot);
		return false;
	}
	const bool bWriteSync = bSync || bSyncSaves || bInBackground;
	const bool bOk = bWriteSync ? SaveStorage->WriteSync(ActiveSlot, Json) : SaveStorage->Write(ActiveSlot, Json);
	if (!bOk)
	{
		ReportError(LocalizeOr("ui.error.saveFailedTitle", TEXT("Save failed")),
			LocalizeOr("ui.error.saveFailed", TEXT("The game could not be saved.")));
	}
	return bOk;
}

void UAbyssGameInstance::NotifySaveRequested(const std::string& Reason)
{
	bSaveRequested = true;
	SaveRequestReason = AbyssText::ToFString(Reason);
}

void UAbyssGameInstance::FlushRequestedSave()
{
	if (!bSaveRequested)
	{
		return;
	}
	bSaveRequested = false;
	UE_LOG(LogAbyss, Verbose, TEXT("Autosave (%s) slot %d"), *SaveRequestReason, ActiveSlot);
	SaveNow(/*bSync*/ false);
	SaveRequestReason.Reset();
}

// =====================================================================================================================
// App lifecycle (ue58-platform.md 13)
// =====================================================================================================================

// UE broadcasts the lifecycle delegates on the game thread on iOS and Android; the hop below is only a safety net (the
// core is game-thread only). A hopped background save may run after the OS suspended the app: hence also the 60 s
// autosave and the save points of the core.
void UAbyssGameInstance::HandleAppWillEnterBackground()
{
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis = TWeakObjectPtr<UAbyssGameInstance>(this)]()
		{
			if (UAbyssGameInstance* Self = WeakThis.Get())
			{
				Self->EnterBackground();
			}
		});
		return;
	}
	EnterBackground();
}

void UAbyssGameInstance::HandleAppHasEnteredForeground()
{
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis = TWeakObjectPtr<UAbyssGameInstance>(this)]()
		{
			if (UAbyssGameInstance* Self = WeakThis.Get())
			{
				Self->ExitBackground();
			}
		});
		return;
	}
	ExitBackground();
}

void UAbyssGameInstance::HandleAppWillTerminate()
{
	if (!IsInGameThread())
	{
		return;   // nothing safe to do off the game thread; the background save already ran
	}
	if (HasSession() && Sim->CanSave())
	{
		SaveNow(/*bSync*/ true);
	}
	if (SaveStorage)
	{
		SaveStorage->Flush();
	}
}

void UAbyssGameInstance::EnterBackground()
{
	if (bInBackground)
	{
		return;
	}
	bInBackground = true;
	AbyssPlatform::SetScreenSaverAllowed(true);
	if (HasSession() && !IsDispatching())
	{
		// CmdAppBackground freezes the sim (FreezeReason::Background), resolves a pending death and requests a save; one
		// immediate step applies it and the save is written synchronously (the OS may suspend us right after).
		Sim->Submit(abyss::CmdAppBackground{true});
		bSyncSaves = true;
		StepImmediately();
		bSyncSaves = false;
	}
	if (SaveStorage)
	{
		SaveStorage->Flush();
	}
	UE_LOG(LogAbyss, Log, TEXT("App entered background"));
}

void UAbyssGameInstance::ExitBackground()
{
	if (!bInBackground)
	{
		return;
	}
	bInBackground = false;
	AbyssPlatform::SetScreenSaverAllowed(AppState != EAbyssAppState::InGame);
	if (HasSession())
	{
		// Applied by the next frame; the core discards the accumulated time of the background period (S1 clamp).
		Sim->Submit(abyss::CmdAppBackground{false});
	}
	UE_LOG(LogAbyss, Log, TEXT("App returned to foreground"));
}

