#include "UI/Root/SAbyssUiRoot.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"

#include <variant>

#include "abyss/items/Inventory.h"
#include "abyss/items/Shop.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/quests/Dialogue.h"
#include "abyss/quests/QuestWorld.h"

#include "Framework/AbyssGameInstance.h"
#include "Input/Touch/SAbyssTouchControls.h"
#include "UI/AbyssUiStyle.h"
#include "UI/Hud/AbyssMinimapTexture.h"
#include "UI/Hud/SAbyssHud.h"
#include "UI/Hud/SAbyssHudWidgets.h"
#include "UI/Hud/SAbyssNotices.h"
#include "UI/Menu/SAbyssMainMenu.h"
#include "UI/Panels/SAbyssAchievementsPanel.h"
#include "UI/Panels/SAbyssCharacterPanel.h"
#include "UI/Panels/SAbyssConversationPanels.h"
#include "UI/Panels/SAbyssInventoryPanel.h"
#include "UI/Panels/SAbyssPanelBase.h"
#include "UI/Panels/SAbyssPetsPanel.h"
#include "UI/Panels/SAbyssQuestCardPanel.h"
#include "UI/Panels/SAbyssQuestLogPanel.h"
#include "UI/Panels/SAbyssSettingsPanel.h"
#include "UI/Panels/SAbyssShopPanel.h"
#include "UI/Panels/SAbyssSkillTreePanel.h"
#include "UI/Panels/SAbyssStashPanel.h"
#include "UI/Panels/SAbyssSystemMenuPanel.h"
#include "UI/Panels/SAbyssWorldMapPanel.h"
#include "UI/Root/SAbyssDialogs.h"
#include "UI/Story/SAbyssStoryOverlay.h"
#include "UI/World/SAbyssWorldLayer.h"

namespace
{
	/** The snapshot handed to panels outside a session (the settings panel in the main menu reads none of it). */
	const abyss::Snapshot& AbyssUiRoot_EmptySnapshot()
	{
		static const abyss::Snapshot Empty;
		return Empty;
	}

	/** Grace period after the UI opened / closed a core-owned modal before the snapshot may contradict it. */
	constexpr double AbyssUiRoot_ModalGraceSeconds = 0.6;
	/** 7.0.4 open animation. */
	constexpr double AbyssUiRoot_OpenSeconds = 0.15;

	uint8 AbyssUiRoot_ModalKey(abyss::PanelId Panel)
	{
		// The blacksmith's shop is reported as Forge, a merchant's as Shop: one panel, one key.
		return static_cast<uint8>(Panel == abyss::PanelId::Forge ? abyss::PanelId::Shop : Panel);
	}

	std::string AbyssUiRoot_CoreModalNpc(const abyss::Snapshot& Snap, abyss::PanelId Panel)
	{
		switch (Panel)
		{
		case abyss::PanelId::Dialogue: return Snap.dialogue ? Snap.dialogue->npcId : std::string();
		case abyss::PanelId::QuestCard: return Snap.questCard ? Snap.questCard->npcId : std::string();
		case abyss::PanelId::Shop:
		case abyss::PanelId::Forge: return Snap.shop ? Snap.shop->npcId : std::string();
		case abyss::PanelId::Stash: return Snap.stash ? Snap.stash->npcId : std::string();
		default: return std::string();
		}
	}
}

// =====================================================================================================================
// Construction
// =====================================================================================================================

void SAbyssUiRoot::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext,
	const TSharedRef<FAbyssMinimapTexture>& InMinimap)
{
	Ctx = InContext;
	MinimapTexture = InMinimap;
	Ctx->SetHost(this);
	bLastTouch = Ctx->IsTouch();
	SetVisibility(EVisibility::SelfHitTestInvisible);

	SAssignNew(WorldLayer, SAbyssWorldLayer, InContext);
	SAssignNew(Hud, SAbyssHud, InContext, InMinimap);
	SAssignNew(Story, SAbyssStoryOverlay, InContext);
	SAssignNew(MenuNotices, SAbyssNotices, InContext);

	TSharedPtr<SDPIScaler> PopupScale;
	TSharedPtr<SDPIScaler> TooltipScale;

	ChildSlot
	[
		SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		// world-anchored labels and floating text: full bleed, never hit-testable
		+ SOverlay::Slot()
		[
			WorldLayer.ToSharedRef()
		]
		// main menu (draws its own background and safe zone)
		+ SOverlay::Slot()
		[
			SAssignNew(MenuHost, SBox)
			.Visibility(EVisibility::Collapsed)
		]
		// HUD + touch controls
		+ SOverlay::Slot()
		[
			SNew(SSafeZone)
			.Visibility(EVisibility::SelfHitTestInvisible)
			.IsTitleSafe(false)
			[
				SAssignNew(GameLayer, SOverlay)
				.Visibility(EVisibility::Collapsed)
				+ SOverlay::Slot()
				[
					Hud.ToSharedRef()
				]
				+ SOverlay::Slot()
				[
					SAssignNew(TouchHost, SBox)
					.Visibility(EVisibility::SelfHitTestInvisible)
				]
			]
		]
		// panels
		+ SOverlay::Slot()
		[
			SNew(SSafeZone)
			.Visibility(EVisibility::SelfHitTestInvisible)
			.IsTitleSafe(false)
			[
				SAssignNew(PanelLayer, SOverlay)
				.Visibility(EVisibility::SelfHitTestInvisible)
			]
		]
		// story overlay (letterbox bars are full bleed)
		+ SOverlay::Slot()
		[
			Story.ToSharedRef()
		]
		// controls reference
		+ SOverlay::Slot()
		[
			SAssignNew(HelpHost, SBox)
			.Visibility(EVisibility::Collapsed)
		]
		// confirms
		+ SOverlay::Slot()
		[
			SAssignNew(ConfirmHost, SBox)
			.Visibility(EVisibility::Collapsed)
		]
		// context popups: an outside press closes them
		+ SOverlay::Slot()
		[
			SAssignNew(PopupLayer, SOverlay)
			.Visibility(EVisibility::Collapsed)
			+ SOverlay::Slot()
			[
				SNew(SAbyssBlocker)
				.OnPressed_Lambda([this]() { ClosePopup(); })
			]
			+ SOverlay::Slot()
			[
				SNew(SConstraintCanvas)
				.Visibility(EVisibility::SelfHitTestInvisible)
				+ SConstraintCanvas::Slot()
				.Anchors(FAnchors(0.f, 0.f))
				.AutoSize(true)
				.Offset(TAttribute<FMargin>::CreateSP(this, &SAbyssUiRoot::GetPopupOffset))
				[
					SAssignNew(PopupScale, SDPIScaler)
					.DPIScale(TAttribute<float>::CreateLambda([this]()
					{
						if (!Ctx->IsTouch() || !PopupBox.IsValid())
						{
							return 1.f;
						}
						// 7.2: touch popups x1.45, shrunk when that would not fit the screen
						const FVector2D Desired = PopupBox->GetDesiredSize();
						const FVector2D Layer = GetLayerSize();
						float Scale = 1.45f;
						if (Desired.X > 1.0 && Desired.Y > 1.0)
						{
							Scale = FMath::Min(Scale, static_cast<float>(FMath::Min((Layer.X - 16.0) / Desired.X, (Layer.Y - 16.0) / Desired.Y)));
						}
						return FMath::Max(1.f, Scale);
					}))
					[
						SAssignNew(PopupBox, SBox)
					]
				]
			]
		]
		// tooltips
		+ SOverlay::Slot()
		[
			SAssignNew(TooltipCanvas, SConstraintCanvas)
			.Visibility(EVisibility::Collapsed)
			+ SConstraintCanvas::Slot()
			.Anchors(FAnchors(0.f, 0.f))
			.AutoSize(true)
			.Offset(TAttribute<FMargin>::CreateSP(this, &SAbyssUiRoot::GetTooltipOffset))
			[
				SAssignNew(TooltipScale, SDPIScaler)
				.DPIScale(TAttribute<float>::CreateLambda([this]() { return Ctx->IsTouch() ? 1.45f : 1.f; }))
				[
					SAssignNew(TooltipBox, SBox)
				]
			]
		]
		// toasts outside a session (in game the HUD notices show them)
		+ SOverlay::Slot()
		[
			MenuNotices.ToSharedRef()
		]
		// system errors
		+ SOverlay::Slot()
		[
			SAssignNew(ErrorHost, SBox)
			.Visibility(EVisibility::Collapsed)
		]
	];
	PopupScaler = PopupScale;
	TooltipScaler = TooltipScale;
	MenuNotices->SetVisibility(EVisibility::HitTestInvisible);
	RefreshLayerVisibility();
}

SAbyssUiRoot::~SAbyssUiRoot()
{
	if (Ctx.IsValid() && Ctx->GetHost() == this)
	{
		Ctx->SetHost(nullptr);
	}
}

// =====================================================================================================================
// Flow
// =====================================================================================================================

void SAbyssUiRoot::SetAppState(EAbyssAppState NewState)
{
	AppState = NewState;
	if (NewState == EAbyssAppState::MainMenu)
	{
		if (!Menu.IsValid())
		{
			SAssignNew(Menu, SAbyssMainMenu, Ctx.ToSharedRef());
			MenuHost->SetContent(Menu.ToSharedRef());
		}
		Menu->Activate();
		MenuHost->SetVisibility(EVisibility::SelfHitTestInvisible);
	}
	else
	{
		MenuHost->SetVisibility(EVisibility::Collapsed);
	}
	if (NewState != EAbyssAppState::InGame)
	{
		// The menu has no world: drop in-game overlays (the session-end hook normally did it already).
		ClosePopup();
		HideTooltip(nullptr);
	}
	RefreshLayerVisibility();
}

void SAbyssUiRoot::RefreshLayerVisibility()
{
	const bool bInGame = AppState == EAbyssAppState::InGame && bSession;
	GameLayer->SetVisibility(bInGame && !bCinematic ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
	WorldLayer->SetVisibility(bInGame ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
	// 6.14: everything in game disappears while a story beat plays and returns unchanged.
	PanelLayer->SetVisibility(bInGame && bCinematic ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible);
	MenuNotices->SetVisibility(bInGame ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
}

void SAbyssUiRoot::OnSessionStarted()
{
	bSession = true;
	bCinematic = false;
	CloseAllPanels(true, false);
	ClosePopup();
	HideTooltip(nullptr);
	CloseHelp();
	if (bConfirmOpen)
	{
		bConfirmReported = false;
		CloseConfirm(false);
	}
	MiniBoss = FAbyssMiniBossLines();
	ChainNpc.clear();
	bChainWaitStory = false;
	PendingChainQuest.clear();
	PendingChainNpc.clear();
	CoreModalChangedAt.Reset();
	Hud->OnSessionStarted();
	Story->Reset();
	RefreshLayerVisibility();
}

void SAbyssUiRoot::OnSessionEnded()
{
	CloseAllPanels(true, false);
	ClosePopup();
	HideTooltip(nullptr);
	if (bConfirmOpen)
	{
		bConfirmReported = false;
		CloseConfirm(false);
	}
	bSession = false;
	bCinematic = false;
	ChainNpc.clear();
	PendingChainQuest.clear();
	PendingChainNpc.clear();
	Hud->OnSessionEnded();
	Story->Reset();
	WorldLayer->ClearWidgets();
	RefreshLayerVisibility();
}

void SAbyssUiRoot::SyncFrame(const abyss::Snapshot& Snap, const FAbyssFrameInfo& Frame)
{
	const double Now = Ctx->Now();
	LastSyncTime = Now;
	if (bCinematic != Snap.cinematic)
	{
		bCinematic = Snap.cinematic;
		if (bCinematic)
		{
			ClosePopup();
			HideTooltip(nullptr);
		}
		RefreshLayerVisibility();
	}
	Hud->Sync(Snap, Frame);
	WorldLayer->Sync(Snap);
	Story->Sync(Snap, Frame);
	ReconcileCoreModals(Snap, Now);
	TickPanels(Snap, Now);
	// GameSim::Frame applies queued commands in every step, and once per frame while the world is frozen (StepOnce
	// without counting a step: the quest card freezes the world).
	TickQuestChain(Now, &Snap, Frame.StepsThisFrame > 0 || Frame.bFrozen);
}

void SAbyssUiRoot::TickPanels(const abyss::Snapshot& Snap, double Now)
{
	// Copy: a refresh may close panels (RequestClose) and modify the stack.
	TArray<TSharedPtr<SAbyssPanelBase>> Open;
	Open.Reserve(Panels.Num());
	for (const FPanelEntry& Entry : Panels)
	{
		Open.Add(Entry.Panel);
	}
	for (const TSharedPtr<SAbyssPanelBase>& Panel : Open)
	{
		if (!Panel.IsValid())
		{
			continue;
		}
		Panel->MaybeRefresh(Snap, Now);
		Panel->TickPanel(Snap, Now);
	}
}

void SAbyssUiRoot::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	const double Now = Ctx->Now();

	if (bLocaleDirty)
	{
		bLocaleDirty = false;
		bLayoutDirty = false;
		ApplyLocaleRefresh();
	}
	else if (bLayoutDirty)
	{
		bLayoutDirty = false;
		Hud->RebuildLayout();
		if (Menu.IsValid())
		{
			Menu->RefreshLocale();
		}
		RebuildPanelsForLocale();
	}

	// 7.0.4: open animation, scale 0.92 -> 1 and alpha 0 -> 1 over 150 ms (Back.easeOut).
	for (FPanelEntry& Entry : Panels)
	{
		if (!Entry.bAnimating || !Entry.Animated.IsValid())
		{
			continue;
		}
		const float T = AbyssEase::Progress(Now, Entry.OpenTime, AbyssUiRoot_OpenSeconds);
		if (T >= 1.f)
		{
			Entry.bAnimating = false;
			Entry.Animated->SetRenderTransform(TOptional<FSlateRenderTransform>());
			Entry.Animated->SetRenderOpacity(1.f);
			continue;
		}
		const float Scale = 0.92f + 0.08f * AbyssEase::BackOut(T);
		Entry.Animated->SetRenderTransform(TOptional<FSlateRenderTransform>(FSlateRenderTransform(Scale)));
		Entry.Animated->SetRenderOpacity(T);
	}

	// Outside a session nothing calls SyncFrame: panels opened from the menu (settings) refresh here.
	const bool bSynced = LastSyncTime >= 0.0 && Now - LastSyncTime < 0.25;
	if (!bSession || !bSynced)
	{
		const abyss::Snapshot* Snap = bSession ? Ctx->GetSnapshot() : nullptr;
		TickPanels(Snap != nullptr ? *Snap : AbyssUiRoot_EmptySnapshot(), Now);
	}
}

// =====================================================================================================================
// Events
// =====================================================================================================================

void SAbyssUiRoot::HandleCoreEvent(const abyss::Event& Event, const abyss::Snapshot* Snap)
{
	Hud->HandleEvent(Event, Snap);

	if (const abyss::EvPanelRequest* Request = std::get_if<abyss::EvPanelRequest>(&Event))
	{
		HandlePanelRequest(*Request);
		return;
	}
	if (const abyss::EvQuestUpdate* Update = std::get_if<abyss::EvQuestUpdate>(&Event))
	{
		if (Snap != nullptr && Update->kind == abyss::EvQuestUpdate::Kind::Progress)
		{
			WorldLayer->HandleQuestUpdate(*Update, *Snap);
		}
		if (Update->kind == abyss::EvQuestUpdate::Kind::TurnedIn && !PendingChainQuest.empty() && Update->questId == PendingChainQuest)
		{
			// T17: offer the giver's next card 900 ms after the confirmed turn-in (or after its cutscene).
			ChainNpc = PendingChainNpc;
			ChainDue = Ctx->Now() + 0.9;
			bChainWaitStory = false;
			PendingChainQuest.clear();
			PendingChainNpc.clear();
		}
		MarkPanelsDirty();
		return;
	}
	if (std::holds_alternative<abyss::EvStoryStep>(Event) || std::holds_alternative<abyss::EvStoryBeat>(Event))
	{
		Story->HandleEvent(Event);
		return;
	}
	if (const abyss::EvStoryState* State = std::get_if<abyss::EvStoryState>(&Event))
	{
		Story->HandleEvent(Event);
		if (!ChainNpc.empty())
		{
			if (State->active)
			{
				bChainWaitStory = true;
			}
			else if (bChainWaitStory)
			{
				// T17: STORY_STATE{false} + 300 ms
				bChainWaitStory = false;
				ChainDue = Ctx->Now() + 0.3;
			}
		}
		return;
	}
	if (const abyss::EvMiniBossDialogue* Boss = std::get_if<abyss::EvMiniBossDialogue>(&Event))
	{
		if (Boss->opened)
		{
			MiniBoss.Monster = Boss->monster;
			MiniBoss.MonsterId = Boss->monsterId;
			MiniBoss.NameKey = Boss->nameKey;
			MiniBoss.LineKeys = Boss->lineKeys;
			const int32 Index = FindPanel(abyss::PanelId::MiniBossDialogue);
			if (Index != INDEX_NONE)
			{
				// The panel opened from an earlier signal: rebuild it with the lines.
				const std::string NpcId;
				RemoveSingle(Index, false);
				PushPanel(abyss::PanelId::MiniBossDialogue, NpcId, false);
			}
		}
		return;
	}
	if (std::holds_alternative<abyss::EvHeroDied>(Event))
	{
		// save-ui-input 5.1.1 (port rule): on HeroDied the UE closes its own modal UI - the socket panel, an in-game
		// confirm and the item context popup (the core closes its modals itself and rejects every command these would
		// send while Dying). Information panels stay open, read-only through IsHeroDying().
		ClosePopup();
		HideTooltip(nullptr);
		if (bConfirmOpen && AppState == EAbyssAppState::InGame)
		{
			CloseConfirm(false);
		}
		const int32 SocketIndex = FindPanel(abyss::PanelId::Socket);
		if (SocketIndex != INDEX_NONE)
		{
			RemoveSingle(SocketIndex, true);   // reports CmdClosePanel{Socket}
			ReturnFocusToGame();
		}
		MarkPanelsDirty();
		return;
	}
	if (const abyss::EvCraftPerformed* Craft = std::get_if<abyss::EvCraftPerformed>(&Event))
	{
		MarkPanelsDirty();
		if (!Craft->ok && !Craft->reason.Empty())
		{
			ShowToast(Ctx->Loc(Craft->reason), Ctx->Style().Colors().Bad);
		}
		return;
	}
	if (const abyss::EvZone* Zone = std::get_if<abyss::EvZone>(&Event))
	{
		if (Zone->phase == abyss::EvZone::Phase::Exited || Zone->phase == abyss::EvZone::Phase::TransitionBegan)
		{
			ClosePopup();
			HideTooltip(nullptr);
		}
		MarkPanelsDirty();
		return;
	}
	if (std::holds_alternative<abyss::EvInventoryChanged>(Event) || std::holds_alternative<abyss::EvEquipmentChanged>(Event)
		|| std::holds_alternative<abyss::EvStashChanged>(Event) || std::holds_alternative<abyss::EvGoldChanged>(Event)
		|| std::holds_alternative<abyss::EvShopOpened>(Event) || std::holds_alternative<abyss::EvSkillLevelChanged>(Event)
		|| std::holds_alternative<abyss::EvHotbarChanged>(Event) || std::holds_alternative<abyss::EvLevelUp>(Event)
		|| std::holds_alternative<abyss::EvExpGained>(Event) || std::holds_alternative<abyss::EvPet>(Event)
		|| std::holds_alternative<abyss::EvAchievementUnlocked>(Event) || std::holds_alternative<abyss::EvLoreCollected>(Event)
		|| std::holds_alternative<abyss::EvDialogue>(Event) || std::holds_alternative<abyss::EvQuestCardOpened>(Event)
		|| std::holds_alternative<abyss::EvHeroDied>(Event) || std::holds_alternative<abyss::EvHeroRespawned>(Event)
		|| std::holds_alternative<abyss::EvPuzzlePrompt>(Event) || std::holds_alternative<abyss::EvHiddenAreaDiscovered>(Event)
		|| std::holds_alternative<abyss::EvEmbersGained>(Event))
	{
		MarkPanelsDirty();
	}
}

void SAbyssUiRoot::HandlePanelRequest(const abyss::EvPanelRequest& Request)
{
	if (!abyss::IsCoreOwnedPanel(Request.panel))
	{
		return;
	}
	const uint8 Key = AbyssUiRoot_ModalKey(Request.panel);
	if (Request.open)
	{
		const int32 Existing = FindPanel(Request.panel);
		if (Existing != INDEX_NONE)
		{
			Panels[Existing].Panel->MarkDirty();
			return;
		}
		// 7.0.1: opening a panel closes the HUD panels (core-owned modals stay: a mini-boss can walk up to a shop).
		ClosePopup();
		HideTooltip(nullptr);
		for (int32 Index = Panels.Num() - 1; Index >= 0; --Index)
		{
			if (!Panels[Index].bCoreOwned)
			{
				RemoveSingle(Index, true);
			}
		}
		PushPanel(Request.panel, Request.npcId, false);
		CoreModalChangedAt.Add(Key, Ctx->Now());
	}
	else
	{
		const int32 Index = FindPanel(Request.panel);
		if (Index != INDEX_NONE)
		{
			RemovePanelsFrom(Index, false);
		}
		CoreModalChangedAt.Add(Key, Ctx->Now());
	}
}

void SAbyssUiRoot::ReconcileCoreModals(const abyss::Snapshot& Snap, double Now)
{
	const auto InGrace = [this, Now](abyss::PanelId Panel)
	{
		const double* Changed = CoreModalChangedAt.Find(AbyssUiRoot_ModalKey(Panel));
		return Changed != nullptr && Now - *Changed < AbyssUiRoot_ModalGraceSeconds;
	};
	// open in the core, missing here
	for (const abyss::PanelId Panel : Snap.modals)
	{
		if (!abyss::IsCoreOwnedPanel(Panel) || FindPanel(Panel) != INDEX_NONE || InGrace(Panel))
		{
			continue;
		}
		abyss::EvPanelRequest Request;
		Request.panel = Panel;
		Request.open = true;
		Request.npcId = AbyssUiRoot_CoreModalNpc(Snap, Panel);
		HandlePanelRequest(Request);
	}
	// open here, closed in the core
	for (int32 Index = Panels.Num() - 1; Index >= 0; --Index)
	{
		if (Index >= Panels.Num())
		{
			continue;
		}
		const FPanelEntry& Entry = Panels[Index];
		if (!Entry.bCoreOwned || InGrace(Entry.Id))
		{
			continue;
		}
		bool bOpenInCore = false;
		for (const abyss::PanelId Panel : Snap.modals)
		{
			if (AbyssUiRoot_ModalKey(Panel) == AbyssUiRoot_ModalKey(Entry.Id))
			{
				bOpenInCore = true;
				break;
			}
		}
		if (!bOpenInCore)
		{
			RemovePanelsFrom(Index, false);
		}
	}
}

// =====================================================================================================================
// Panels
// =====================================================================================================================

TSharedPtr<SAbyssPanelBase> SAbyssUiRoot::CreatePanel(abyss::PanelId Id, const std::string& NpcId, abyss::EquipSlot SocketSlot)
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	switch (Id)
	{
	case abyss::PanelId::Inventory: return SNew(SAbyssInventoryPanel, Context);
	case abyss::PanelId::Character: return SNew(SAbyssCharacterPanel, Context);
	case abyss::PanelId::Skills: return SNew(SAbyssSkillTreePanel, Context);
	case abyss::PanelId::QuestLog: return SNew(SAbyssQuestLogPanel, Context);
	case abyss::PanelId::WorldMap: return SNew(SAbyssWorldMapPanel, Context, Hud->GetState(), MinimapTexture.ToSharedRef());
	case abyss::PanelId::Achievements: return SNew(SAbyssAchievementsPanel, Context);
	case abyss::PanelId::Settings: return SNew(SAbyssSettingsPanel, Context);
	case abyss::PanelId::SystemMenu: return SNew(SAbyssSystemMenuPanel, Context);
	case abyss::PanelId::Pets: return SNew(SAbyssPetsPanel, Context);
	case abyss::PanelId::Dialogue: return SNew(SAbyssDialoguePanel, Context, NpcId);
	case abyss::PanelId::QuestCard: return SNew(SAbyssQuestCardPanel, Context, NpcId);
	case abyss::PanelId::Shop: return SNew(SAbyssShopPanel, Context, NpcId, abyss::PanelId::Shop);
	case abyss::PanelId::Forge: return SNew(SAbyssShopPanel, Context, NpcId, abyss::PanelId::Forge);
	case abyss::PanelId::Stash: return SNew(SAbyssStashPanel, Context, NpcId);
	case abyss::PanelId::Socket: return SNew(SAbyssSocketPanel, Context, SocketSlot);
	case abyss::PanelId::MiniBossDialogue: return SNew(SAbyssMiniBossPanel, Context, MiniBoss.NameKey, MiniBoss.MonsterId, MiniBoss.LineKeys);
	case abyss::PanelId::LoreText: return SNew(SAbyssLorePanel, Context);
	case abyss::PanelId::Puzzle: return SNew(SAbyssPuzzlePanel, Context);
	case abyss::PanelId::Confirm: return nullptr;  // confirms live in their own layer
	}
	return nullptr;
}

int32 SAbyssUiRoot::FindPanel(abyss::PanelId Id) const
{
	const uint8 Key = AbyssUiRoot_ModalKey(Id);
	for (int32 Index = 0; Index < Panels.Num(); ++Index)
	{
		if (Panels[Index].Id == Id || (abyss::IsCoreOwnedPanel(Id) && AbyssUiRoot_ModalKey(Panels[Index].Id) == Key))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

bool SAbyssUiRoot::IsPanelOpen(abyss::PanelId Panel) const
{
	return FindPanel(Panel) != INDEX_NONE;
}

bool SAbyssUiRoot::IsCoreModalOpen() const
{
	for (const FPanelEntry& Entry : Panels)
	{
		if (Entry.bCoreOwned)
		{
			return true;
		}
	}
	return false;
}

bool SAbyssUiRoot::IsBlockingModalOpen() const
{
	for (const FPanelEntry& Entry : Panels)
	{
		if (Entry.bCoreOwned || Entry.Id == abyss::PanelId::SystemMenu)
		{
			return true;
		}
	}
	return false;
}

bool SAbyssUiRoot::HasPets() const
{
	const abyss::Snapshot* Snap = Ctx->GetSnapshot();
	return Snap != nullptr && Snap->pets != nullptr && !Snap->pets->Owned().empty();
}

void SAbyssUiRoot::PushPanel(abyss::PanelId Id, const std::string& NpcId, bool bChild, abyss::EquipSlot SocketSlot)
{
	const TSharedPtr<SAbyssPanelBase> Panel = CreatePanel(Id, NpcId, SocketSlot);
	if (!Panel.IsValid())
	{
		return;
	}
	FPanelEntry Entry;
	Entry.Id = Id;
	Entry.Panel = Panel;
	Entry.NpcId = NpcId;
	Entry.SocketSlot = SocketSlot;
	Entry.bCoreOwned = abyss::IsCoreOwnedPanel(Id);
	Entry.bChild = bChild;
	Entry.OpenTime = Ctx->Now();
	Entry.bAnimating = true;
	Panel->SetOpenedTime(Entry.OpenTime);
	Panel->SetCloseHandler(FSimpleDelegate::CreateSP(this, &SAbyssUiRoot::ClosePanel, Id));
	Entry.Holder = MakePanelHolder(Entry);
	if (Entry.Animated.IsValid())
	{
		Entry.Animated->SetRenderOpacity(0.f);
		Entry.Animated->SetRenderTransform(TOptional<FSlateRenderTransform>(FSlateRenderTransform(0.92f)));
	}
	PanelLayer->AddSlot()
	[
		Entry.Holder.ToSharedRef()
	];
	if (!Entry.bCoreOwned && bSession && AppState == EAbyssAppState::InGame)
	{
		Ctx->Submit(abyss::CmdOpenPanel{ Id });
		Entry.bReported = true;
	}
	Panels.Add(Entry);
	Panel->OnOpened();
	Ctx->PlaySound(abyss::SfxId::PanelOpen);

	// First build at once (no empty frame); later rebuilds follow the dirty / auto-refresh rules.
	const abyss::Snapshot* Snap = bSession ? Ctx->GetSnapshot() : nullptr;
	Panel->MaybeRefresh(Snap != nullptr ? *Snap : AbyssUiRoot_EmptySnapshot(), Ctx->Now());
}

TSharedRef<SWidget> SAbyssUiRoot::MakePanelHolder(FPanelEntry& Entry)
{
	const TSharedRef<SAbyssPanelBase> Panel = Entry.Panel.ToSharedRef();
	const TWeakPtr<SAbyssPanelBase> WeakPanel = Panel;
	TSharedRef<SOverlay> Holder = SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible);
	if (Panel->IsModal())
	{
		// 7.0.5: modal panels add a dim + vignette backdrop that blocks the world and (usually) closes the panel.
		const bool bCloses = Panel->CloseOnBackdrop();
		const abyss::PanelId Id = Entry.Id;
		Holder->AddSlot()
		[
			SNew(SAbyssBackdrop, Ctx.ToSharedRef())
			.Alpha(Panel->GetBackdropAlpha())
			.OnPressed_Lambda([this, Id, bCloses]()
			{
				if (bCloses)
				{
					ClosePanel(Id);
				}
			})
		];
	}
	TSharedPtr<SBox> Animated;
	Holder->AddSlot()
	[
		SNew(SConstraintCanvas)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SConstraintCanvas::Slot()
		.Anchors(FAnchors(0.f, 0.f))
		.AutoSize(true)
		.Offset(TAttribute<FMargin>::CreateLambda([this, WeakPanel]() { return GetPanelOffset(WeakPanel); }))
		[
			SNew(SDPIScaler)
			.DPIScale(TAttribute<float>::CreateLambda([this, WeakPanel]() { return GetPanelScale(WeakPanel); }))
			[
				SAssignNew(Animated, SBox)
				.RenderTransformPivot(FVector2D(0.5, 0.5))
				[
					Panel
				]
			]
		]
	];
	Entry.Animated = Animated;
	return Holder;
}

FVector2D SAbyssUiRoot::GetLayerSize() const
{
	if (!PanelLayer.IsValid())
	{
		return FVector2D(1280.0, 720.0);
	}
	const FVector2D Size(PanelLayer->GetTickSpaceGeometry().GetLocalSize());
	return Size.X > 1.0 && Size.Y > 1.0 ? Size : FVector2D(1280.0, 720.0);
}

float SAbyssUiRoot::GetPanelScale(TWeakPtr<SAbyssPanelBase> WeakPanel) const
{
	const TSharedPtr<SAbyssPanelBase> Panel = WeakPanel.Pin();
	if (!Panel.IsValid() || !Ctx->IsTouch())
	{
		return 1.f;
	}
	// fitPanelForMobile (7.0.6): s = max(1, min(2, (W - 24) / pw, (H - 24) / ph)).
	const FVector2D Layer = GetLayerSize();
	const FVector2D Design = Panel->GetDesignSize();
	if (Design.X <= 0.0 || Design.Y <= 0.0)
	{
		return 1.f;
	}
	const double Fit = FMath::Min((Layer.X - 24.0) / Design.X, (Layer.Y - 24.0) / Design.Y);
	return static_cast<float>(FMath::Max(1.0, FMath::Min(2.0, Fit)));
}

FMargin SAbyssUiRoot::GetPanelOffset(TWeakPtr<SAbyssPanelBase> WeakPanel) const
{
	const TSharedPtr<SAbyssPanelBase> Panel = WeakPanel.Pin();
	if (!Panel.IsValid())
	{
		return FMargin(0.f);
	}
	const FVector2D Layer = GetLayerSize();
	const float Scale = GetPanelScale(WeakPanel);
	const FVector2D Size = Panel->GetDesignSize() * Scale;
	const FVector2D Design = Panel->GetDesignPosition();
	double X = 0.0;
	double Y = 0.0;
	double Margin = 4.0;
	if (Ctx->IsTouch())
	{
		// 7.0.6: re-centred, clamped to a 12 px margin
		X = (Layer.X - Size.X) * 0.5;
		Y = (Layer.Y - Size.Y) * 0.5;
		Margin = 12.0;
	}
	else
	{
		// web top-left in 1280x720, centred in wider / taller layouts
		X = Design.X < 0.0 ? (Layer.X - Size.X) * 0.5 : Design.X + (Layer.X - 1280.0) * 0.5;
		Y = Design.Y < 0.0 ? (Layer.Y - Size.Y) * 0.5 : Design.Y + (Layer.Y - 720.0) * 0.5;
	}
	X = FMath::Clamp(X, Margin, FMath::Max(Margin, Layer.X - Size.X - Margin));
	Y = FMath::Clamp(Y, Margin, FMath::Max(Margin, Layer.Y - Size.Y - Margin));
	return FMargin(static_cast<float>(X), static_cast<float>(Y), 0.f, 0.f);
}

void SAbyssUiRoot::RemoveSingle(int32 Index, bool bReport)
{
	if (!Panels.IsValidIndex(Index))
	{
		return;
	}
	const FPanelEntry Entry = Panels[Index];
	Panels.RemoveAt(Index);
	if (Entry.Holder.IsValid())
	{
		PanelLayer->RemoveSlot(Entry.Holder.ToSharedRef());
	}
	if (Entry.Panel.IsValid())
	{
		Entry.Panel->OnClosed();
	}
	if (Entry.bCoreOwned)
	{
		CoreModalChangedAt.Add(AbyssUiRoot_ModalKey(Entry.Id), Ctx->Now());
	}
	if (bReport && bSession)
	{
		if (Entry.bCoreOwned || Entry.bReported)
		{
			// UE-owned: the core's open set; core-owned: the owner's Close() (SimTypes.h ownership rules).
			Ctx->Submit(abyss::CmdClosePanel{ Entry.Id });
		}
	}
}

void SAbyssUiRoot::RemovePanelsFrom(int32 Index, bool bReport)
{
	if (!Panels.IsValidIndex(Index))
	{
		return;
	}
	for (int32 Top = Panels.Num() - 1; Top >= Index; --Top)
	{
		RemoveSingle(Top, bReport);
	}
	ClosePopup();
	HideTooltip(nullptr);
	Ctx->PlaySound(abyss::SfxId::PanelClose);
	ReturnFocusToGame();
}

void SAbyssUiRoot::CloseAllPanels(bool bIncludeCoreOwned, bool bReport)
{
	for (int32 Index = Panels.Num() - 1; Index >= 0; --Index)
	{
		if (Index < Panels.Num() && (bIncludeCoreOwned || !Panels[Index].bCoreOwned))
		{
			RemoveSingle(Index, bReport);
		}
	}
	ReturnFocusToGame();
}

void SAbyssUiRoot::TogglePanel(abyss::PanelId Panel)
{
	const int32 Index = FindPanel(Panel);
	if (Index != INDEX_NONE)
	{
		RemovePanelsFrom(Index, true);
		return;
	}
	OpenPanel(Panel);
}

void SAbyssUiRoot::OpenPanel(abyss::PanelId Panel)
{
	if (abyss::IsCoreOwnedPanel(Panel) || Panel == abyss::PanelId::Confirm || Panel == abyss::PanelId::Socket)
	{
		return;  // core-owned modals open from EvPanelRequest; the socket panel needs its slot
	}
	if (FindPanel(Panel) != INDEX_NONE)
	{
		return;
	}
	const bool bInGame = AppState == EAbyssAppState::InGame && bSession;
	if (!bInGame && Panel != abyss::PanelId::Settings)
	{
		return;  // only the settings panel exists outside a session
	}
	if (bInGame && bCinematic)
	{
		return;
	}
	if (Panel == abyss::PanelId::SystemMenu && Ctx->IsHeroDying())
	{
		return;  // C12: the core rejects the pause menu during the death window
	}
	if (Panel == abyss::PanelId::Pets && !HasPets())
	{
		return;  // U6: the ley-beast panel exists once a beast is owned
	}
	const int32 MenuIndex = FindPanel(abyss::PanelId::SystemMenu);
	bool bChild = false;
	if ((Panel == abyss::PanelId::Settings || Panel == abyss::PanelId::Achievements) && MenuIndex != INDEX_NONE)
	{
		// opened from the system menu: over it (Back returns to the menu)
		RemovePanelsFrom(MenuIndex + 1, true);
		bChild = true;
	}
	else if (IsCoreModalOpen())
	{
		return;  // U7: a modal keeps the HUD panels shut
	}
	else
	{
		CloseAllPanels(false, true);  // 7.0.1 exclusivity
	}
	ClosePopup();
	HideTooltip(nullptr);
	PushPanel(Panel, std::string(), bChild);
}

void SAbyssUiRoot::ClosePanel(abyss::PanelId Panel)
{
	const int32 Index = FindPanel(Panel);
	if (Index != INDEX_NONE)
	{
		RemovePanelsFrom(Index, true);
	}
}

void SAbyssUiRoot::OpenSocketPanel(abyss::EquipSlot Slot)
{
	const int32 InventoryIndex = FindPanel(abyss::PanelId::Inventory);
	if (InventoryIndex == INDEX_NONE || Ctx->IsHeroDying())
	{
		return;
	}
	RemovePanelsFrom(InventoryIndex + 1, true);
	ClosePopup();
	HideTooltip(nullptr);
	PushPanel(abyss::PanelId::Socket, std::string(), true, Slot);
}

void SAbyssUiRoot::MarkPanelsDirty()
{
	for (FPanelEntry& Entry : Panels)
	{
		if (Entry.Panel.IsValid())
		{
			Entry.Panel->MarkDirty();
		}
	}
}

void SAbyssUiRoot::RebuildPanelsForLocale()
{
	// Recreate every open panel in place (titles and fonts are built at construction). Local view state resets.
	struct FReopen
	{
		abyss::PanelId Id;
		std::string NpcId;
		abyss::EquipSlot SocketSlot;
		bool bChild;
		bool bReported;
		bool bCoreOwned;
	};
	TArray<FReopen> Reopen;
	for (const FPanelEntry& Entry : Panels)
	{
		Reopen.Add(FReopen{ Entry.Id, Entry.NpcId, Entry.SocketSlot, Entry.bChild, Entry.bReported, Entry.bCoreOwned });
	}
	for (int32 Index = Panels.Num() - 1; Index >= 0; --Index)
	{
		const FPanelEntry Entry = Panels[Index];
		Panels.RemoveAt(Index);
		if (Entry.Holder.IsValid())
		{
			PanelLayer->RemoveSlot(Entry.Holder.ToSharedRef());
		}
	}
	for (const FReopen& Item : Reopen)
	{
		const TSharedPtr<SAbyssPanelBase> Panel = CreatePanel(Item.Id, Item.NpcId, Item.SocketSlot);
		if (!Panel.IsValid())
		{
			continue;
		}
		FPanelEntry Entry;
		Entry.Id = Item.Id;
		Entry.Panel = Panel;
		Entry.NpcId = Item.NpcId;
		Entry.SocketSlot = Item.SocketSlot;
		Entry.bCoreOwned = Item.bCoreOwned;
		Entry.bChild = Item.bChild;
		Entry.bReported = Item.bReported;
		Entry.OpenTime = Ctx->Now() - 1.0;
		Entry.bAnimating = false;
		Panel->SetOpenedTime(Entry.OpenTime);
		Panel->SetCloseHandler(FSimpleDelegate::CreateSP(this, &SAbyssUiRoot::ClosePanel, Item.Id));
		Entry.Holder = MakePanelHolder(Entry);
		PanelLayer->AddSlot()
		[
			Entry.Holder.ToSharedRef()
		];
		Panels.Add(Entry);
		Panel->OnOpened();
		const abyss::Snapshot* Snap = bSession ? Ctx->GetSnapshot() : nullptr;
		Panel->MaybeRefresh(Snap != nullptr ? *Snap : AbyssUiRoot_EmptySnapshot(), Ctx->Now());
	}
}

void SAbyssUiRoot::ReturnFocusToGame() const
{
	if (AppState == EAbyssAppState::InGame && FSlateApplication::IsInitialized())
	{
		// Keys stay with the game viewport and its Enhanced Input bindings (save-ui-input 7.0, input agent note).
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

// =====================================================================================================================
// Back / input requests
// =====================================================================================================================

bool SAbyssUiRoot::HandleBack()
{
	if (bErrorOpen)
	{
		if (AppState != EAbyssAppState::DataError)
		{
			CloseError();
		}
		return true;
	}
	if (bPopupOpen)
	{
		ClosePopup();
		return true;
	}
	if (bConfirmOpen)
	{
		CloseConfirm(false);
		return true;
	}
	if (bHelpOpen)
	{
		CloseHelp();
		return true;
	}
	if (Panels.Num() > 0)
	{
		const FPanelEntry& Top = Panels.Last();
		if (Top.Panel.IsValid() && Top.Panel->HandleBack())
		{
			return true;
		}
		ClosePanel(Top.Id);
		return true;
	}
	if (AppState == EAbyssAppState::MainMenu)
	{
		return Menu.IsValid() && Menu->HandleBack();
	}
	if (AppState == EAbyssAppState::InGame && bSession)
	{
		if (bCinematic)
		{
			return false;
		}
		if (!Ctx->IsHeroDying())
		{
			OpenPanel(abyss::PanelId::SystemMenu);  // U4
		}
		return true;  // C12: during the death window Back does nothing
	}
	return false;
}

bool SAbyssUiRoot::HandleUiInput(const FAbyssUiInputRequest& Request)
{
	switch (Request.Kind)
	{
	case EAbyssUiRequest::TogglePanel:
	{
		if (AppState != EAbyssAppState::InGame || !bSession)
		{
			return false;
		}
		if (bCinematic || bConfirmOpen || bErrorOpen || IsBlockingModalOpen())
		{
			return true;  // U7: modals keep the panel keys
		}
		TogglePanel(Request.Panel);
		return true;
	}
	case EAbyssUiRequest::StoryAdvance:
		return Story->HandleAdvance();
	case EAbyssUiRequest::StorySkip:
		return Story->HandleSkip();
	case EAbyssUiRequest::ToggleCombatLog:
		Hud->ToggleCombatLog();
		return true;
	case EAbyssUiRequest::OpenSystemMenu:
	{
		if (AppState != EAbyssAppState::InGame || !bSession)
		{
			return false;
		}
		if (FindPanel(abyss::PanelId::SystemMenu) != INDEX_NONE)
		{
			ClosePanel(abyss::PanelId::SystemMenu);
			return true;
		}
		if (bCinematic || Ctx->IsHeroDying())
		{
			return true;
		}
		ClosePopup();
		CloseAllPanels(true, true);
		OpenPanel(abyss::PanelId::SystemMenu);
		return true;
	}
	}
	return false;
}

// =====================================================================================================================
// Locale / settings
// =====================================================================================================================

void SAbyssUiRoot::RequestLocaleRefresh()
{
	bLocaleDirty = true;
}

void SAbyssUiRoot::ApplyLocaleRefresh()
{
	ClosePopup();
	HideTooltip(nullptr);
	Hud->RebuildLayout();
	if (const TSharedPtr<SAbyssCombatLog> Log = Hud->GetCombatLog())
	{
		Log->Rebuild();
	}
	if (Menu.IsValid())
	{
		Menu->RefreshLocale();
	}
	Story->OnLocaleChanged();
	RebuildPanelsForLocale();
	if (bHelpOpen)
	{
		ShowHelp();
	}
}

void SAbyssUiRoot::OnSettingsChanged()
{
	const bool bTouch = Ctx->IsTouch();
	if (bTouch != bLastTouch)
	{
		bLastTouch = bTouch;
		bLayoutDirty = true;  // touch adaptations (7.0.6) are built into the panels
	}
}

// =====================================================================================================================
// Tooltips / popups
// =====================================================================================================================

void SAbyssUiRoot::ShowTooltip(const TSharedRef<SWidget>& Content, const FVector2D& AbsoluteAnchor, const void* Owner)
{
	if (bPopupOpen)
	{
		return;  // the popup's card already shows the item
	}
	TooltipOwner = Owner;
	TooltipAnchor = AbsoluteAnchor;
	TooltipBox->SetContent(Content);
	TooltipCanvas->SetVisibility(EVisibility::HitTestInvisible);
}

void SAbyssUiRoot::HideTooltip(const void* Owner)
{
	if (Owner != nullptr && Owner != TooltipOwner)
	{
		return;
	}
	TooltipOwner = nullptr;
	if (TooltipBox.IsValid())
	{
		TooltipBox->SetContent(SNullWidget::NullWidget);
	}
	if (TooltipCanvas.IsValid())
	{
		TooltipCanvas->SetVisibility(EVisibility::Collapsed);
	}
}

FMargin SAbyssUiRoot::GetTooltipOffset() const
{
	if (!TooltipCanvas.IsValid() || !TooltipScaler.IsValid())
	{
		return FMargin(0.f);
	}
	const FGeometry& Geometry = TooltipCanvas->GetTickSpaceGeometry();
	const FVector2D Local(Geometry.AbsoluteToLocal(TooltipAnchor));
	FVector2D Layer(Geometry.GetLocalSize());
	if (Layer.X < 1.0 || Layer.Y < 1.0)
	{
		Layer = FVector2D(1280.0, 720.0);
	}
	const FVector2D Size = TooltipScaler->GetDesiredSize();
	// 7.2: cursor + (16, -10), flipped left when it would leave the screen, clamped 4 px.
	double X = Local.X + 16.0;
	double Y = Local.Y - 10.0;
	if (X + Size.X > Layer.X - 4.0)
	{
		X = Local.X - 16.0 - Size.X;
	}
	X = FMath::Clamp(X, 4.0, FMath::Max(4.0, Layer.X - Size.X - 4.0));
	Y = FMath::Clamp(Y, 4.0, FMath::Max(4.0, Layer.Y - Size.Y - 4.0));
	return FMargin(static_cast<float>(X), static_cast<float>(Y), 0.f, 0.f);
}

void SAbyssUiRoot::ShowPopup(const TSharedRef<SWidget>& Content, const FVector2D& AbsoluteAnchor)
{
	HideTooltip(nullptr);
	PopupAnchor = AbsoluteAnchor;
	PopupBox->SetContent(Content);
	PopupLayer->SetVisibility(EVisibility::SelfHitTestInvisible);
	bPopupOpen = true;
}

void SAbyssUiRoot::ClosePopup()
{
	if (!bPopupOpen)
	{
		return;
	}
	bPopupOpen = false;
	PopupBox->SetContent(SNullWidget::NullWidget);
	PopupLayer->SetVisibility(EVisibility::Collapsed);
}

FMargin SAbyssUiRoot::GetPopupOffset() const
{
	if (!PopupLayer.IsValid() || !PopupScaler.IsValid())
	{
		return FMargin(0.f);
	}
	const FGeometry& Geometry = PopupLayer->GetTickSpaceGeometry();
	const FVector2D Local(Geometry.AbsoluteToLocal(PopupAnchor));
	FVector2D Layer(Geometry.GetLocalSize());
	if (Layer.X < 1.0 || Layer.Y < 1.0)
	{
		Layer = FVector2D(1280.0, 720.0);
	}
	const FVector2D Size = PopupScaler->GetDesiredSize();
	double X = Local.X;
	double Y = Local.Y;
	if (Ctx->IsTouch())
	{
		// 7.2: touch popups sit 36 px beside the finger (right, else left), vertically centred on it
		X = Local.X + 36.0;
		if (X + Size.X > Layer.X - 8.0)
		{
			X = Local.X - 36.0 - Size.X;
		}
		Y = Local.Y - Size.Y * 0.5;
	}
	else if (X + Size.X > Layer.X - 4.0)
	{
		X = Local.X - Size.X;
	}
	X = FMath::Clamp(X, 4.0, FMath::Max(4.0, Layer.X - Size.X - 4.0));
	Y = FMath::Clamp(Y, 4.0, FMath::Max(4.0, Layer.Y - Size.Y - 4.0));
	return FMargin(static_cast<float>(X), static_cast<float>(Y), 0.f, 0.f);
}

// =====================================================================================================================
// Confirms, toasts, help, errors
// =====================================================================================================================

void SAbyssUiRoot::ShowConfirm(FAbyssConfirmRequest Request)
{
	if (bConfirmOpen)
	{
		// a new confirm replaces the old one without running its callbacks
		ActiveConfirm = FAbyssConfirmRequest();
	}
	ClosePopup();
	HideTooltip(nullptr);
	ActiveConfirm = MoveTemp(Request);
	bConfirmOpen = true;
	ConfirmHost->SetContent(
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SAbyssBackdrop, Ctx.ToSharedRef())
			.Alpha(0.5f)
		]
		+ SOverlay::Slot()
		[
			SNew(SAbyssConfirmDialog, Ctx.ToSharedRef(), ActiveConfirm)
			.OnResult_Lambda([this](bool bConfirmed) { CloseConfirm(bConfirmed); })
		]);
	ConfirmHost->SetVisibility(EVisibility::Visible);
	if (!bConfirmReported && bSession && AppState == EAbyssAppState::InGame && !Ctx->IsHeroDying())
	{
		Ctx->Submit(abyss::CmdOpenPanel{ abyss::PanelId::Confirm });  // U7: confirms block gameplay input
		bConfirmReported = true;
	}
}

void SAbyssUiRoot::CloseConfirm(bool bConfirmed)
{
	if (!bConfirmOpen)
	{
		return;
	}
	FAbyssConfirmRequest Request = MoveTemp(ActiveConfirm);
	ActiveConfirm = FAbyssConfirmRequest();
	bConfirmOpen = false;
	ConfirmHost->SetContent(SNullWidget::NullWidget);
	ConfirmHost->SetVisibility(EVisibility::Collapsed);
	if (bConfirmReported)
	{
		bConfirmReported = false;
		if (bSession)
		{
			Ctx->Submit(abyss::CmdClosePanel{ abyss::PanelId::Confirm });
		}
	}
	ReturnFocusToGame();
	if (bConfirmed)
	{
		if (Request.OnConfirm)
		{
			Request.OnConfirm();
		}
	}
	else if (Request.OnCancel)
	{
		Request.OnCancel();
	}
}

void SAbyssUiRoot::ShowToast(const FText& Text, const FLinearColor& Color)
{
	if (bSession && AppState == EAbyssAppState::InGame)
	{
		if (const TSharedPtr<SAbyssNotices> Notices = Hud->GetNotices())
		{
			Notices->ShowToast(Text, Color);
		}
		return;
	}
	MenuNotices->ShowToast(Text, Color);
}

void SAbyssUiRoot::AddLocalLog(const FString& Text, abyss::LogType Type)
{
	if (const TSharedPtr<SAbyssCombatLog> Log = Hud->GetCombatLog())
	{
		Log->AddLocalLine(Text, Type);
	}
}

void SAbyssUiRoot::ShowHelp()
{
	HelpHost->SetContent(SNew(SAbyssHelpSheet, Ctx.ToSharedRef()).OnClose_Lambda([this]() { CloseHelp(); }));
	HelpHost->SetVisibility(EVisibility::Visible);
	bHelpOpen = true;
}

void SAbyssUiRoot::CloseHelp()
{
	if (!bHelpOpen)
	{
		return;
	}
	bHelpOpen = false;
	HelpHost->SetContent(SNullWidget::NullWidget);
	HelpHost->SetVisibility(EVisibility::Collapsed);
	ReturnFocusToGame();
}

void SAbyssUiRoot::ShowSystemError(const FText& Title, const FText& Message)
{
	const bool bFatal = AppState == EAbyssAppState::DataError || AppState == EAbyssAppState::Boot;
	ErrorHost->SetContent(
		SNew(SAbyssErrorDialog, Ctx.ToSharedRef())
		.Title(Title)
		.Message(Message)
		.ShowQuit(bFatal)
		.OnDismiss_Lambda([this]() { CloseError(); }));
	ErrorHost->SetVisibility(EVisibility::Visible);
	bErrorOpen = true;
}

void SAbyssUiRoot::CloseError()
{
	if (!bErrorOpen)
	{
		return;
	}
	bErrorOpen = false;
	ErrorHost->SetContent(SNullWidget::NullWidget);
	ErrorHost->SetVisibility(EVisibility::Collapsed);
	ReturnFocusToGame();
}

// =====================================================================================================================
// Misc
// =====================================================================================================================

void SAbyssUiRoot::SetTouchControls(const TSharedRef<SWidget>& InTouchControls)
{
	TouchControls = StaticCastSharedRef<SAbyssTouchControls>(InTouchControls);
	TouchHost->SetContent(InTouchControls);
	Hud->SetTouchControls(TouchControls);
}

void SAbyssUiRoot::SetHoveredEntity(abyss::EntityId Id)
{
	WorldLayer->SetHovered(Id);
}

void SAbyssUiRoot::RequestQuestChainOffer(const std::string& QuestId, const std::string& NpcId)
{
	PendingChainQuest = QuestId;
	PendingChainNpc = NpcId;
}

void SAbyssUiRoot::TickQuestChain(double Now, const abyss::Snapshot* Snap, bool bCommandsApplied)
{
	// The step that applied CmdQuestTurnIn was dispatched before this sync: no TurnedIn event -> the core rejected it.
	if (!PendingChainQuest.empty() && bCommandsApplied)
	{
		PendingChainQuest.clear();
		PendingChainNpc.clear();
	}
	if (ChainNpc.empty() || Snap == nullptr || bChainWaitStory || Now < ChainDue)
	{
		return;
	}
	if (Snap->storyBusy || Snap->cinematic)
	{
		bChainWaitStory = true;  // a turn-in cutscene: wait for STORY_STATE{false} + 300 ms
		return;
	}
	const std::string NpcId = ChainNpc;
	ChainNpc.clear();
	Ctx->Submit(abyss::CmdQuestCardOpen{ NpcId });
}
