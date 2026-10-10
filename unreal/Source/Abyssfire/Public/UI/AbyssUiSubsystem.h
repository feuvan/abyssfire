// UAbyssUiSubsystem: the Slate UI of Abyssfire (ui agent: UI/). One per game instance.
//
// Implements
// * IAbyssUiRoot (Framework/AbyssUiRoot.h): app-state screens (data error / title + 3 save slots + class select +
//   difficulty + language + settings + controls + credits / in game), the per-frame HUD sync, Back (U4), locale changes,
//   system errors. Registered with UAbyssGameInstance::RegisterUiRoot in Initialize.
// * IAbyssWorldUi (World/AbyssWorldUi.h): world-anchored nameplates, HP bars, loot / lore labels, quest markers and the
//   floating combat text, in one Slate layer projected every paint (ue58-platform.md 9.6). Registered with the world
//   builder of each game world (UAbyssWorldBuilder::SetWorldUi; the builder also finds it through the UI root).
//
// Consumes
// * FAbyssEventRouter::OnAnyEvent (one subscription; the root widget routes each event to the HUD, panels, notices and the
//   story overlay), OnSessionStarted / OnSessionEnded; UAbyssGameInstance settings / locale.
// * UAbyssInputSubsystem::SetUiInputHandler (panel hotkeys, story advance / skip with the two-tap rule, touch log toggle,
//   touch menu button), OnHoveredEntityChanged (nameplate hover) and CreateTouchControls (hosted inside the HUD's safe
//   zone); desktop HUD buttons inject their actions with UAbyssInputSubsystem::PressAction so they share the keyboard
//   path.
//
// Every player action is an abyss::Command (GI->Submit); panels are driven by the snapshot and EvPanelRequest (core-owned
// modals) and report the UE-owned ones with CmdOpenPanel / CmdClosePanel (SimTypes.h ownership, U7). All text comes from
// the core's i18n tables (keys the tables do not have yet show an English fallback; see the report for the key list).
//
// UI-originated sound cues (clicks, panel toggles) are broadcast as abyss::EvSfx{cue, spatial = false, source =
// kNoEntity} through the router's EvSfx delegate, so the audio layer has a single SFX path; they never reach the core.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectPtr.h"

#include "abyss/base/Enums.h"
#include "abyss/data/AudioData.h"
#include "abyss/sim/Events.h"
#include "abyss/sim/Snapshot.h"

#include "Framework/AbyssTypes.h"
#include "Framework/AbyssUiRoot.h"
#include "Input/AbyssInputTypes.h"
#include "World/AbyssWorldUi.h"

#include "AbyssUiSubsystem.generated.h"

class FAbyssMinimapTexture;
class FAbyssUiContext;
class FAbyssUiStyle;
class SAbyssUiRoot;
class UAbyssGameInstance;
class UGameViewportClient;
class UTexture2D;
class UWorld;
struct FAbyssUserSettings;

UCLASS()
class ABYSSFIRE_API UAbyssUiSubsystem : public UGameInstanceSubsystem, public IAbyssUiRoot, public IAbyssWorldUi
{
	GENERATED_BODY()

public:
	UAbyssUiSubsystem();
	virtual ~UAbyssUiSubsystem() override;

	static UAbyssUiSubsystem* Get(const UObject* WorldContextObject);

	// ---- USubsystem ----
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---- IAbyssUiRoot ----
	virtual void OnAppStateChanged(EAbyssAppState NewState) override;
	virtual void SyncFrame(const abyss::Snapshot& Snap, const FAbyssFrameInfo& Frame) override;
	virtual bool HandleBack() override;
	virtual void OnLocaleChanged() override;
	virtual void ShowSystemError(const FText& Title, const FText& Message) override;

	// ---- IAbyssWorldUi ----
	virtual void AddWorldWidget(const FAbyssWorldWidgetDesc& Desc) override;
	virtual void RemoveWorldWidget(abyss::EntityId Id) override;
	virtual void ClearWorldWidgets() override;
	virtual void UpdateWorldWidgets(TConstArrayView<FAbyssWorldWidgetFrame> Frames) override;
	virtual void ShowFloatingText(const FAbyssFloatingTextRequest& Request) override;

	// ---- services for the UI widgets ----
	UAbyssGameInstance* GetAbyssGameInstance() const;
	const FAbyssUiStyle& GetStyle() const;
	TSharedPtr<FAbyssUiContext> GetContext() const { return Context; }
	EAbyssAppState GetAppState() const { return AppState; }
	/**
	 * A UI texture by asset name (T_UI_ItemIcon_*, T_UI_SkillIcon_*, T_UI_Portrait_*, T_UI_Emblem_*, T_UI_Glyph_*,
	 * T_UI_QuestItem_*, T_UI_HudIcon_*), searched under /Game/Abyssfire/{UI/Icons, UI/Portraits, UI, Icons, Portraits,
	 * Textures}. Loaded textures stay referenced for the game instance's lifetime; misses are remembered (nullptr).
	 */
	UTexture2D* FindUiTexture(FName AssetName);
	/** Keeps a runtime UObject (minimap texture) alive while the UI uses it. */
	void KeepAlive(UObject* Object);
	void ReleaseKeptAlive(UObject* Object);
	/** UI sound cue (2D), delivered to the audio layer as abyss::EvSfx through the router. */
	void PlayUiSound(abyss::SfxId Cue) const;

private:
	void EnsureRootWidget();
	void AttachRootToViewport();
	void DetachRootFromViewport();
	void RegisterWithWorld();
	void HandleCoreEvent(const abyss::Event& Event);
	void HandleSessionStarted();
	void HandleSessionEnded();
	void HandleSettingsChanged(const FAbyssUserSettings& NewSettings);
	void HandleHoveredEntityChanged(abyss::EntityId Entity);
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	bool HandleUiInputRequest(const FAbyssUiInputRequest& Request);
	void BindInputHandler();
	void RefreshStyleLocale();

	TSharedPtr<FAbyssUiContext> Context;
	TSharedPtr<FAbyssUiStyle> Style;
	TSharedPtr<FAbyssMinimapTexture> Minimap;
	TSharedPtr<SAbyssUiRoot> RootWidget;
	TWeakObjectPtr<UGameViewportClient> AttachedViewport;

	/** Textures referenced by Slate brushes (icons, portraits) and runtime textures (minimap). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> KeptAssets;

	TMap<FName, TWeakObjectPtr<UTexture2D>> TextureCache;
	TSet<FName> MissingTextures;

	/** A system error reported before the root widget could be shown (no viewport yet). */
	TOptional<TPair<FText, FText>> PendingError;

	FDelegateHandle SettingsHandle;
	FDelegateHandle HoverHandle;
	FDelegateHandle WorldCleanupHandle;
	EAbyssAppState AppState = EAbyssAppState::Boot;
	bool bEventsBound = false;
	bool bInputHandlerBound = false;
	bool bThemeApplied = false;
	TWeakObjectPtr<UWorld> RegisteredWorld;
};
