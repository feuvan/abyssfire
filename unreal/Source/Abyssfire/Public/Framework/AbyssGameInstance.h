// UAbyssGameInstance: app-lifetime owner of everything core-side (ARCHITECTURE 6; save-ui-input.md 1, 3; DECISIONS U1, U2,
// U9; ue58-platform.md 10, 13).
//
// Owns, in destruction order (last first): the DataStore (every Data/*.json, loaded at Init through the pak-aware
// platform file), the user settings (settings.json), the save storage (3 slots), the event router, and the GameSim of
// the current session (created on New Game / Continue, destroyed on return to menu: a fresh session every time, FIX Q1).
//
// Lifetimes:
//   Init                -> data + settings loaded, app state Boot (or DataError)
//   GameMode BeginPlay  -> NotifyGameWorldReady -> MainMenu (the viewport exists now, the UI root shows the title)
//   StartNewGame / ContinueSlot -> InGame (session events are dispatched by UAbyssSimDriver on its next tick)
//   ReturnToMainMenu    -> resolve a pending death, save, end the session -> MainMenu
//   app background      -> CmdAppBackground + one immediate step + synchronous save; foreground -> CmdAppBackground{false}
//   Shutdown            -> final synchronous save, session ended, storage flushed, core objects destroyed
//
// Threading: game thread only (the core is single-threaded). Never cache GetSim() / GetSnapshot() pointers across frames.
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "UObject/WeakInterfacePtr.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Complete core types: std::unique_ptr members need them wherever the (UHT-generated) constructors are compiled.
#include "abyss/base/Enums.h"
#include "abyss/base/I18n.h"
#include "abyss/data/DataStore.h"
#include "abyss/save/SaveIO.h"
#include "abyss/sim/Commands.h"
#include "abyss/sim/GameSim.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

#include "Framework/AbyssEventRouter.h"
#include "Framework/AbyssTypes.h"
#include "Framework/AbyssUiRoot.h"
#include "Platform/AbyssSaveStorage.h"
#include "Platform/AbyssSettings.h"

#include "AbyssGameInstance.generated.h"

class UAbyssSimDriver;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnAbyssAppStateChanged, EAbyssAppState /*NewState*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAbyssSettingsChanged, const FAbyssUserSettings& /*NewSettings*/);
DECLARE_MULTICAST_DELEGATE(FOnAbyssLocaleChanged);

/** Result of UAbyssGameInstance::ContinueSlot (GameSim::LoadGame error semantics). */
struct FAbyssLoadResult
{
	bool bOk = false;
	/** Called while events were being dispatched: the load runs right after the dispatch (errors go to the UI root). */
	bool bDeferred = false;
	bool bSlotMissing = false;
	abyss::SaveError Error = abyss::SaveError::None;
	/** A previous good save (.bak) exists: offer "load the previous save" (ParseFailed / NotAnObject / Invalid). */
	bool bBackupAvailable = false;
	/** English diagnostic (logs); the UI shows its own localized message per Error. */
	FString Message;
};

UCLASS()
class ABYSSFIRE_API UAbyssGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UAbyssGameInstance();
	virtual ~UAbyssGameInstance();

	static UAbyssGameInstance* Get(const UObject* WorldContextObject);

	// ---- UGameInstance ----
	virtual void Init() override;
	virtual void Shutdown() override;

	// ---- app state ----
	EAbyssAppState GetAppState() const { return AppState; }
	FOnAbyssAppStateChanged OnAppStateChanged;

	// ---- data ----
	bool IsDataReady() const;
	/** Null until the tables loaded (and in DataError). */
	const abyss::DataStore* GetData() const;
	/** The i18n tables (current locale). Only valid when IsDataReady(). */
	const abyss::I18n& GetStrings() const;
	FText Localize(const abyss::LocText& Text) const;
	FText Localize(std::string_view Key) const;
	/** Human-readable (English) data load report when the state is DataError. */
	const FString& GetDataError() const { return DataError; }

	// ---- session ----
	bool HasSession() const;
	/** The running GameSim, or nullptr. */
	abyss::GameSim* GetSim() const;
	/** The current snapshot, or nullptr without a session. Valid until the next call into GameSim. */
	const abyss::Snapshot* GetSnapshot() const;
	/** Queues a command for the next sim step (no-op without a session). Every UI / input action goes through here. */
	void Submit(const abyss::Command& Command);
	/** Slot (0..2) of the running session, or INDEX_NONE. */
	int32 GetActiveSlot() const { return ActiveSlot; }
	FAbyssEventRouter& GetEventRouter() { return EventRouter; }
	const FAbyssEventRouter& GetEventRouter() const { return EventRouter; }

	// ---- flow (called by the UI; the UI confirms overwrites / deletes first, U1) ----
	/** New hero in Slot (overwrites it on the first autosave). Ends a running session first (saving it). */
	bool StartNewGame(abyss::ClassId ClassId, int32 Slot, abyss::Difficulty Difficulty = abyss::Difficulty::Normal);
	/**
	 * Loads Slot (save-ui-input.md 1.2). DifficultyOverride = the difficulty selector's choice when
	 * SaveSlotInfo::showDifficultySelector. bFromBackup loads the slot's .bak (offered after a parse failure).
	 */
	FAbyssLoadResult ContinueSlot(int32 Slot, std::optional<abyss::Difficulty> DifficultyOverride = std::nullopt,
		bool bFromBackup = false);
	/** System menu "Save & return to menu" (U4): resolves a Dying hero (C12), saves, ends the session. */
	void ReturnToMainMenu();
	/** System menu "Quit": like ReturnToMainMenu with a synchronous save, then quits the app (desktop / Android). */
	void QuitGame();
	/** Deletes a slot (not the one in play). */
	bool DeleteSlot(int32 Slot);
	/** One entry per slot (exists flag + SummarizeSave), for the title screen. */
	void ListSlots(std::vector<abyss::SaveSlotInfo>& Out);
	/** UE-initiated save of the active slot (core-requested autosaves are handled automatically). */
	bool SaveNow(bool bSync = false);

	// ---- settings ----
	const FAbyssUserSettings& GetUserSettings() const { return UserSettings; }
	/** Sanitizes, applies (locale, touch mode, quality), persists settings.json and broadcasts. */
	void ApplyUserSettings(const FAbyssUserSettings& NewSettings);
	bool IsTouchMode() const { return bTouchMode; }
	EAbyssQualityTier GetQualityTier() const { return QualityTier; }
	FOnAbyssSettingsChanged OnSettingsChanged;
	FOnAbyssLocaleChanged OnLocaleChanged;

	// ---- UI root registration (ui agent) ----
	void RegisterUiRoot(IAbyssUiRoot* Root);
	void UnregisterUiRoot(IAbyssUiRoot* Root);
	IAbyssUiRoot* GetUiRoot() const;

	// ---- hooks for AAbyssGameMode / UAbyssSimDriver (not for the UI) ----
	void NotifyGameWorldReady(UWorld* World);
	bool AreSessionEventsPending() const { return bSessionEventsPending; }
	void ClearSessionEventsPending() { bSessionEventsPending = false; }
	void NotifySaveRequested(const std::string& Reason);
	/** Writes the save requested by EvSaveRequested during the last dispatch (once per frame). */
	void FlushRequestedSave();
	/** Runs flow calls that were made during a dispatch. */
	void RunDeferredFlow();
	bool IsDispatching() const;

private:
	void LoadData();
	void LoadSettings();
	void ApplySettingsSideEffects(const FAbyssUserSettings& Previous, bool bInitial);
	void SetAppState(EAbyssAppState NewState);
	abyss::SimConfig MakeSimConfig() const;
	void BeginSession(std::unique_ptr<abyss::GameSim> NewSim, int32 Slot);
	/** Ends the session without saving (callers save first). bNotifyWorld: let the driver clear the world view. */
	void EndSession(bool bNotifyWorld = true);
	/** Applies queued commands now (one Step) and dispatches the events. */
	void StepImmediately();
	UAbyssSimDriver* FindDriver() const;
	void ReportError(const FText& Title, const FText& Message);
	/** Core i18n key, else the English fallback (no table / missing key). */
	FText LocalizeOr(std::string_view Key, const TCHAR* EnglishFallback) const;
	/** Defers Fn to the end of the current dispatch; returns true when it was deferred. */
	bool DeferIfDispatching(TFunction<void()> Fn);

	// app lifecycle (ue58-platform.md 13)
	void HandleAppWillEnterBackground();
	void HandleAppHasEnteredForeground();
	void HandleAppWillTerminate();
	void EnterBackground();
	void ExitBackground();

	static int64 UnixMsNow();

	std::unique_ptr<abyss::DataStore> Store;
	std::unique_ptr<abyss::GameSim> Sim;
	TUniquePtr<FAbyssSaveStorage> SaveStorage;
	FAbyssEventRouter EventRouter;
	FAbyssUserSettings UserSettings;
	TWeakInterfacePtr<IAbyssUiRoot> UiRoot;
	TArray<TFunction<void()>> DeferredFlow;
	FString DataError;
	FString SaveRequestReason;
	EAbyssAppState AppState = EAbyssAppState::Boot;
	EAbyssQualityTier QualityTier = EAbyssQualityTier::Balanced;
	int32 ActiveSlot = INDEX_NONE;
	bool bTouchMode = false;
	bool bSessionEventsPending = false;
	bool bSaveRequested = false;
	bool bSyncSaves = false;
	bool bInBackground = false;
	bool bLifecycleBound = false;
};
