// SAbyssUiRoot: the one Slate root of the game UI (save-ui-input.md 1, 6, 7; ue58-platform.md 9). Added to the game
// viewport by UAbyssUiSubsystem; everything the player sees in 2D lives under it, in these layers (back to front):
//
//   world layer       nameplates, HP bars, loot labels, floating combat text (full bleed, never hit-testable)
//   main menu         title / save slots / class select / difficulty / language (+ its painted background)
//   game layer        HUD + the touch control layer, inside the safe zone; hidden while a story beat plays (6.14)
//   panel layer       HUD panels and modal panels (one stack, 7.0), inside the safe zone
//   story overlay     letterbox, narration, dialogue boxes, title cards, sequences, credits, chapter cards
//   confirm layer     confirm dialogs (U1 overwrite / delete, discard, sell, salvage)
//   popup layer       context popups (closed by an outside press)
//   tooltip layer     item / skill tooltips (desktop hover, touch tap)
//   toast layer       toasts outside a session (in game the HUD notices show them)
//   error layer       system errors (data load failure, save write failure)
//
// Panel rules (7.0 / U4 / U7, SimTypes.h ownership):
// * At most one HUD panel is open; opening another closes it. The socket panel sits over the inventory; settings and
//   achievements opened from the system menu sit over it. Pressing a panel's key while it is open closes it.
// * UE-owned panels are reported to the core with CmdOpenPanel / CmdClosePanel (HUD panels pause the sim on touch, the
//   system menu freezes, socket / confirm block gameplay input). Core-owned modals (dialogue, quest card, shop / forge,
//   stash, mini-boss, lore, puzzle) are opened and closed by EvPanelRequest; a UE close sends CmdClosePanel.
// * Back (Esc / Android back / pad Start, U4): popup -> confirm -> the top panel (its inner state first) -> with nothing
//   open, the system menu (never while the hero is Dying, C12).
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include <string>
#include <vector>

#include "abyss/sim/Events.h"
#include "abyss/sim/SimTypes.h"
#include "abyss/sim/Snapshot.h"

#include "Framework/AbyssTypes.h"
#include "Input/AbyssInputTypes.h"
#include "UI/Core/AbyssUiContext.h"

class FAbyssMinimapTexture;
class SAbyssHud;
class SAbyssMainMenu;
class SAbyssNotices;
class SAbyssPanelBase;
class SAbyssStoryOverlay;
class SAbyssTouchControls;
class SAbyssWorldLayer;
class SBox;
class SConstraintCanvas;
class SOverlay;

/** Mini-boss pre-fight lines (EvMiniBossDialogue), kept until the panel opens. */
struct FAbyssMiniBossLines
{
	abyss::EntityId Monster = abyss::kNoEntity;
	std::string MonsterId;
	std::string NameKey;
	std::vector<std::string> LineKeys;
};

class SAbyssUiRoot : public SCompoundWidget, public IAbyssUiHost
{
public:
	SLATE_BEGIN_ARGS(SAbyssUiRoot) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const TSharedRef<FAbyssMinimapTexture>& InMinimap);
	virtual ~SAbyssUiRoot() override;

	// ---- flow (UAbyssUiSubsystem) ----
	void SetAppState(EAbyssAppState NewState);
	void SyncFrame(const abyss::Snapshot& Snap, const FAbyssFrameInfo& Frame);
	/** One core event in dispatch order (Snap = the current snapshot, may be null between sessions). */
	void HandleCoreEvent(const abyss::Event& Event, const abyss::Snapshot* Snap);
	void OnSessionStarted();
	void OnSessionEnded();
	bool HandleBack();
	bool HandleUiInput(const FAbyssUiInputRequest& Request);
	/** Locale switched: every visible text is rebuilt on the next tick (never inside the click that changed it). */
	void RequestLocaleRefresh();
	/** Device settings changed (touch layout, damage numbers, ...). */
	void OnSettingsChanged();
	void ShowSystemError(const FText& Title, const FText& Message);
	void SetTouchControls(const TSharedRef<SWidget>& InTouchControls);
	void SetHoveredEntity(abyss::EntityId Id);
	SAbyssWorldLayer& GetWorldLayer() const { return *WorldLayer; }

	// ---- IAbyssUiHost ----
	virtual void ShowTooltip(const TSharedRef<SWidget>& Content, const FVector2D& AbsoluteAnchor, const void* Owner) override;
	virtual void HideTooltip(const void* Owner) override;
	virtual void ShowPopup(const TSharedRef<SWidget>& Content, const FVector2D& AbsoluteAnchor) override;
	virtual void ClosePopup() override;
	virtual void ShowConfirm(FAbyssConfirmRequest Request) override;
	virtual void ShowToast(const FText& Text, const FLinearColor& Color) override;
	virtual void TogglePanel(abyss::PanelId Panel) override;
	virtual void OpenPanel(abyss::PanelId Panel) override;
	virtual void ClosePanel(abyss::PanelId Panel) override;
	virtual bool IsPanelOpen(abyss::PanelId Panel) const override;
	virtual void OpenSocketPanel(abyss::EquipSlot Slot) override;
	virtual void MarkPanelsDirty() override;
	virtual void ScheduleQuestChainOffer(const std::string& NpcId) override;
	virtual void AddLocalLog(const FString& Text, abyss::LogType Type) override;
	virtual void ShowHelp() override;

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	struct FPanelEntry
	{
		abyss::PanelId Id = abyss::PanelId::Inventory;
		TSharedPtr<SAbyssPanelBase> Panel;
		TSharedPtr<SWidget> Holder;
		TSharedPtr<SWidget> Animated;
		std::string NpcId;
		abyss::EquipSlot SocketSlot = abyss::EquipSlot::Weapon;
		bool bCoreOwned = false;
		bool bReported = false;
		/** Opened over another panel (socket over inventory, settings / achievements over the system menu). */
		bool bChild = false;
		double OpenTime = 0.0;
		bool bAnimating = true;
	};

	// ---- panels ----
	TSharedPtr<SAbyssPanelBase> CreatePanel(abyss::PanelId Id, const std::string& NpcId, abyss::EquipSlot SocketSlot);
	void PushPanel(abyss::PanelId Id, const std::string& NpcId, bool bChild, abyss::EquipSlot SocketSlot = abyss::EquipSlot::Weapon);
	/** Removes one entry. bReport: tell the core (UE-owned close / core-owned close request). */
	void RemoveSingle(int32 Index, bool bReport);
	/** Removes the entry at Index and every entry above it (its sub-panels). */
	void RemovePanelsFrom(int32 Index, bool bReport);
	void CloseAllPanels(bool bIncludeCoreOwned, bool bReport);
	int32 FindPanel(abyss::PanelId Id) const;
	bool IsCoreModalOpen() const;
	bool IsBlockingModalOpen() const;
	TSharedRef<SWidget> MakePanelHolder(FPanelEntry& Entry);
	FMargin GetPanelOffset(TWeakPtr<SAbyssPanelBase> WeakPanel) const;
	float GetPanelScale(TWeakPtr<SAbyssPanelBase> WeakPanel) const;
	FVector2D GetLayerSize() const;
	void TickPanels(const abyss::Snapshot& Snap, double Now);
	void RebuildPanelsForLocale();
	void HandlePanelRequest(const abyss::EvPanelRequest& Request);
	/** Safety net for missed events: the core's open modals (Snapshot::modals) against the stack. */
	void ReconcileCoreModals(const abyss::Snapshot& Snap, double Now);
	bool HasPets() const;
	void ReturnFocusToGame() const;

	// ---- overlays ----
	FMargin GetTooltipOffset() const;
	FMargin GetPopupOffset() const;
	void CloseConfirm(bool bConfirmed);
	void CloseHelp();
	void CloseError();
	void RefreshLayerVisibility();
	void TickQuestChain(double Now, const abyss::Snapshot* Snap);
	void ApplyLocaleRefresh();

	TSharedPtr<FAbyssUiContext> Ctx;
	TSharedPtr<FAbyssMinimapTexture> MinimapTexture;
	EAbyssAppState AppState = EAbyssAppState::Boot;

	// layers
	TSharedPtr<SAbyssWorldLayer> WorldLayer;
	TSharedPtr<SBox> MenuHost;
	TSharedPtr<SAbyssMainMenu> Menu;
	TSharedPtr<SOverlay> GameLayer;
	TSharedPtr<SAbyssHud> Hud;
	TSharedPtr<SBox> TouchHost;
	TSharedPtr<SAbyssTouchControls> TouchControls;
	TSharedPtr<SOverlay> PanelLayer;
	TSharedPtr<SAbyssStoryOverlay> Story;
	TSharedPtr<SBox> HelpHost;
	TSharedPtr<SBox> ConfirmHost;
	TSharedPtr<SOverlay> PopupLayer;
	TSharedPtr<SBox> PopupBox;
	TSharedPtr<SWidget> PopupScaler;
	TSharedPtr<SConstraintCanvas> TooltipCanvas;
	TSharedPtr<SBox> TooltipBox;
	TSharedPtr<SWidget> TooltipScaler;
	TSharedPtr<SAbyssNotices> MenuNotices;
	TSharedPtr<SBox> ErrorHost;

	// panel stack (bottom .. top)
	TArray<FPanelEntry> Panels;
	FAbyssMiniBossLines MiniBoss;

	// tooltip / popup / confirm state
	const void* TooltipOwner = nullptr;
	FVector2D TooltipAnchor = FVector2D::ZeroVector;
	FVector2D PopupAnchor = FVector2D::ZeroVector;
	bool bPopupOpen = false;
	bool bConfirmOpen = false;
	bool bConfirmReported = false;
	FAbyssConfirmRequest ActiveConfirm;
	bool bErrorOpen = false;
	bool bHelpOpen = false;
	/** Real time a core-owned modal was last closed / opened by the UI (reconciliation grace period). */
	TMap<uint8, double> CoreModalChangedAt;

	// quest card chain offer (T17)
	std::string ChainNpc;
	double ChainDue = 0.0;
	bool bChainWaitStory = false;

	bool bCinematic = false;
	bool bSession = false;
	bool bLocaleDirty = false;
	bool bLayoutDirty = false;
	bool bLastTouch = false;
	double LastSyncTime = -1.0;
};
