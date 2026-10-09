#include "Input/Touch/SAbyssTouchControls.h"

#include "Layout/Geometry.h"
#include "Widgets/Layout/Anchors.h"
#include "Widgets/Layout/SConstraintCanvas.h"

#include <string>

#include "abyss/base/Math.h"
#include "abyss/data/DataStore.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/sim/Snapshot.h"

#include "Framework/AbyssGameInstance.h"
#include "Framework/AbyssText.h"
#include "Input/AbyssInputMath.h"
#include "Input/AbyssInputSubsystem.h"
#include "Input/Touch/SAbyssTouchButton.h"
#include "Input/Touch/SAbyssVirtualJoystick.h"

namespace
{
	// Q6: the touch Talk / Use button appears within 2.5 tiles of the core's interact prompt target.
	constexpr double AbyssTouch_InteractButtonRange = 2.5;

	int32 AbyssTouch_Index(EAbyssTouchControl Control)
	{
		return static_cast<int32>(Control);
	}

	EAbyssTouchControl AbyssTouch_SkillControl(int32 Slot)
	{
		return static_cast<EAbyssTouchControl>(static_cast<int32>(EAbyssTouchControl::Skill1) + Slot);
	}

	EAbyssInputAction AbyssTouch_SkillAction(int32 Slot)
	{
		return static_cast<EAbyssInputAction>(static_cast<int32>(EAbyssInputAction::Skill1) + Slot);
	}

	/** First two characters of a skill name (web skill-bar fallback when the icon is missing). */
	FString AbyssTouch_ShortName(const FString& Name)
	{
		return Name.Left(2);
	}
}

SAbyssTouchControls::~SAbyssTouchControls()
{
	if (Joystick.IsValid())
	{
		Joystick->Reset();
	}
}

void SAbyssTouchControls::Construct(const FArguments& InArgs)
{
	InputSubsystem = InArgs._InputSubsystem;
	Visuals = InArgs._Visuals;
	Buttons.SetNum(AbyssTouchControlCount);
	PreviousRemainingMs.Init(0.0, AbyssTouchControlCount);
	SlotSkillIds.SetNum(6);

	SetVisibility(TAttribute<EVisibility>::CreateSP(this, &SAbyssTouchControls::GetLayerVisibility));
	SetColorAndOpacity(TAttribute<FLinearColor>::CreateSP(this, &SAbyssTouchControls::GetLayerColor));

	ChildSlot
	[
		SAssignNew(Canvas, SConstraintCanvas)
		.Visibility(EVisibility::SelfHitTestInvisible)
	];
}

// =====================================================================================================================
// State sources
// =====================================================================================================================

UAbyssGameInstance* SAbyssTouchControls::GetGameInstance() const
{
	const UAbyssInputSubsystem* Subsystem = InputSubsystem.Get();
	return Subsystem ? Subsystem->GetAbyssGameInstance() : nullptr;
}

bool SAbyssTouchControls::ComputeShouldShow() const
{
	const UAbyssGameInstance* GameInstance = GetGameInstance();
	if (GameInstance == nullptr || GameInstance->GetAppState() != EAbyssAppState::InGame || !GameInstance->IsTouchMode()
		|| !GameInstance->IsDataReady())
	{
		return false;
	}
	const abyss::Snapshot* Snap = GameInstance->GetSnapshot();
	return Snap != nullptr && !Snap->cinematic;   // 6.14: hidden while a story beat plays
}

EVisibility SAbyssTouchControls::GetLayerVisibility() const
{
	const bool bShow = ComputeShouldShow();
	if (bShow != bShown)
	{
		bResetPending = true;   // a hidden layer never ticks: reset the stick / pops when it shows again
	}
	bShown = bShow;
	return bShow ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
}

FLinearColor SAbyssTouchControls::GetLayerColor() const
{
	const UAbyssGameInstance* GameInstance = GetGameInstance();
	const float Opacity = GameInstance ? GameInstance->GetUserSettings().TouchControlOpacity : 1.0f;
	return FLinearColor(1.0f, 1.0f, 1.0f, FMath::Clamp(Opacity, 0.0f, 1.0f));
}

FString SAbyssTouchControls::LocalizeOr(std::string_view Key, const TCHAR* Fallback) const
{
	const UAbyssGameInstance* GameInstance = GetGameInstance();
	if (GameInstance == nullptr || !GameInstance->IsDataReady() || !GameInstance->GetStrings().Has(Key))
	{
		return Fallback;
	}
	return AbyssText::LocalizeString(GameInstance->GetStrings(), Key);
}

const FSlateBrush* SAbyssTouchControls::HudIcon(const TCHAR* IconId) const
{
	return Visuals.HudIcon ? Visuals.HudIcon(FName(IconId)) : nullptr;
}

FSlateFontInfo SAbyssTouchControls::BaseFont() const
{
	return Visuals.Font.HasValidFont() ? Visuals.Font : AbyssTouchStyle::FallbackFont(14.0f);
}

TSharedPtr<SAbyssTouchButton> SAbyssTouchControls::Button(EAbyssTouchControl Control) const
{
	const int32 ControlIndex = AbyssTouch_Index(Control);
	return Buttons.IsValidIndex(ControlIndex) ? Buttons[ControlIndex] : nullptr;
}

// =====================================================================================================================
// Tick
// =====================================================================================================================

void SAbyssTouchControls::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	UAbyssGameInstance* GameInstance = GetGameInstance();
	if (!bShown || GameInstance == nullptr || GameInstance->GetData() == nullptr)
	{
		return;
	}
	const abyss::Snapshot* Snap = GameInstance->GetSnapshot();
	if (Snap == nullptr)
	{
		return;
	}
	if (bResetPending)
	{
		ResetTransientState();
		bResetPending = false;
	}

	// Layout inputs: layer size, k (DPR / DPI scale, clamped) x the U9 size setting, bound hotbar slots, pets (U6), locale.
	Metrics = FAbyssTouchMetrics::FromTheme(GameInstance->GetData()->UiTheme());
	const float DpiScale = AllottedGeometry.Scale;
	const float UnitK = AbyssInputMath::ComputeTouchUnitScale(AbyssInputMath::GetDevicePixelRatio(), DpiScale,
		Metrics.ScaleMin, Metrics.ScaleMax);
	const float UnitScale = UnitK * FMath::Max(0.1f, GameInstance->GetUserSettings().TouchControlScale);

	FLayoutKey Key;
	const FVector2D LayerSize = FVector2D(AllottedGeometry.GetLocalSize());
	Key.LayerSize = FIntPoint(FMath::RoundToInt(LayerSize.X), FMath::RoundToInt(LayerSize.Y));
	Key.UnitScaleCenti = FMath::RoundToInt(UnitScale * 100.0f);
	for (int32 Slot = 0; Slot < 6; ++Slot)
	{
		if (Snap->hero.hotbar[static_cast<size_t>(Slot)].skillIndex >= 0)
		{
			Key.SkillSlotMask |= static_cast<uint8>(1u << Slot);
		}
	}
	Key.bShowPets = Snap->pets != nullptr && !Snap->pets->Owned().empty();
	Key.Locale = GameInstance->GetStrings().Current();
	if (Key.LayerSize.X <= 0 || Key.LayerSize.Y <= 0)
	{
		return;
	}
	if (!bHasLayout || !(Key == CurrentKey))
	{
		RebuildLayout(Key, UnitScale);
	}
	UpdateDynamicState(*Snap, *GameInstance);
}

void SAbyssTouchControls::ResetTransientState()
{
	if (Joystick.IsValid())
	{
		Joystick->Reset();
	}
	for (double& Remaining : PreviousRemainingMs)
	{
		Remaining = 0.0;
	}
}

// =====================================================================================================================
// Layout
// =====================================================================================================================

void SAbyssTouchControls::RebuildLayout(const FLayoutKey& Key, float UnitScale)
{
	CurrentKey = Key;
	bHasLayout = true;
	if (Joystick.IsValid())
	{
		Joystick->Reset();
	}
	Canvas->ClearChildren();
	Joystick.Reset();
	for (TSharedPtr<SAbyssTouchButton>& Existing : Buttons)
	{
		Existing.Reset();
	}
	for (FString& SkillId : SlotSkillIds)
	{
		SkillId.Reset();
	}
	LabelledAutoCombat = INDEX_NONE;
	LabelledAutoLoot = INDEX_NONE;
	LabelledPromptKind = INDEX_NONE;

	FAbyssTouchLayoutInput LayoutInput;
	LayoutInput.LayerSize = FVector2D(Key.LayerSize.X, Key.LayerSize.Y);
	LayoutInput.UnitScale = UnitScale;
	LayoutInput.SkillSlotMask = Key.SkillSlotMask;
	LayoutInput.bShowPets = Key.bShowPets;
	AbyssTouchLayout::Compute(Metrics, LayoutInput, Placements);

	const FSlateFontInfo Font = BaseFont();
	const FLinearColor GoldOutline = AbyssTouchStyle::FromRgb(AbyssTouchStyle::Gold, 0.85f);
	const FSlateBrush* Medallion = HudIcon(TEXT("medallion"));

	for (const FAbyssTouchPlacement& Placement : Placements)
	{
		TSharedPtr<SWidget> Widget;
		if (Placement.Control == EAbyssTouchControl::Joystick)
		{
			SAssignNew(Joystick, SAbyssVirtualJoystick)
				.BaseRadius(static_cast<float>(Placement.DrawSize.X * 0.5))
				.GrabScale(Placement.HitScale)
				.DeadZone(0.15f)
				.RingColor(GoldOutline)
				.InputSubsystem(InputSubsystem);
			Widget = Joystick;
		}
		else
		{
			FLinearColor Fill = AbyssTouchStyle::FromRgb(AbyssTouchStyle::SkillFill, 0.92f);
			FLinearColor Outline = GoldOutline;
			FString Label;
			const FSlateBrush* Icon = nullptr;
			const FSlateBrush* Face = nullptr;
			bool bCaption = false;
			switch (Placement.Control)
			{
			case EAbyssTouchControl::Lock:
				Fill = AbyssTouchStyle::FromRgb(AbyssTouchStyle::CornerFill, 0.92f);
				Label = LocalizeOr("sys.mobile.target", TEXT("LOCK"));
				Icon = HudIcon(TEXT("lock"));
				Face = Medallion;
				break;
			case EAbyssTouchControl::Skill1:
			case EAbyssTouchControl::Skill2:
			case EAbyssTouchControl::Skill3:
			case EAbyssTouchControl::Skill4:
			case EAbyssTouchControl::Skill5:
			case EAbyssTouchControl::Skill6:
				Fill = AbyssTouchStyle::FromRgb(AbyssTouchStyle::SkillFill, 0.92f);
				Face = Medallion;
				break;   // icon / label from the hotbar in UpdateDynamicState
			case EAbyssTouchControl::Dodge:
				Fill = AbyssTouchStyle::FromRgb(AbyssTouchStyle::DodgeFill, 0.92f);
				Label = LocalizeOr("sys.mobile.dodge", TEXT("DODGE"));
				Icon = HudIcon(TEXT("dodge"));
				Face = Medallion;
				break;
			case EAbyssTouchControl::PotionHp:
				Fill = AbyssTouchStyle::FromRgb(AbyssTouchStyle::PotionHpFill, 0.92f);
				Label = LocalizeOr("sys.mobile.potion.hp", TEXT("HP"));
				Icon = HudIcon(TEXT("potion_hp"));
				Face = Medallion;
				break;
			case EAbyssTouchControl::PotionMp:
				Fill = AbyssTouchStyle::FromRgb(AbyssTouchStyle::PotionMpFill, 0.92f);
				Label = LocalizeOr("sys.mobile.potion.mp", TEXT("MP"));
				Icon = HudIcon(TEXT("potion_mp"));
				Face = Medallion;
				break;
			case EAbyssTouchControl::Interact:
				Fill = AbyssTouchStyle::FromRgb(AbyssTouchStyle::InteractFill, 0.94f);
				Outline = AbyssTouchStyle::FromRgb(AbyssTouchStyle::GoldBright, 0.95f);
				Face = Medallion;
				break;   // label / icon follow the prompt kind
			case EAbyssTouchControl::AutoCombat:
			case EAbyssTouchControl::AutoLoot:
				Fill = AbyssTouchStyle::FromRgb(AbyssTouchStyle::SecondaryFill, 0.9f);
				Icon = HudIcon(Placement.Control == EAbyssTouchControl::AutoCombat ? TEXT("auto") : TEXT("loot"));
				bCaption = true;
				break;   // label follows the state
			case EAbyssTouchControl::Log:
				Fill = AbyssTouchStyle::FromRgb(AbyssTouchStyle::SecondaryFill, 0.9f);
				Label = LocalizeOr("sys.mobile.log", TEXT("Log"));
				Icon = HudIcon(TEXT("log"));
				bCaption = true;
				break;
			case EAbyssTouchControl::Portal:
				Fill = AbyssTouchStyle::FromRgb(AbyssTouchStyle::SecondaryFill, 0.9f);
				Outline = AbyssTouchStyle::FromRgb(AbyssTouchStyle::PortalRing, 0.9f);
				Label = LocalizeOr("sys.mobile.portal", TEXT("Portal"));
				Icon = HudIcon(TEXT("portal"));
				bCaption = true;
				break;
			case EAbyssTouchControl::PanelInventory:
				Fill = AbyssTouchStyle::FromRgb(AbyssTouchStyle::GhostFill, 0.88f);
				Label = LocalizeOr("sys.mobile.panel.inventory", TEXT("Bag"));
				Icon = HudIcon(TEXT("inventory"));
				bCaption = true;
				break;
			case EAbyssTouchControl::PanelCharacter:
				Fill = AbyssTouchStyle::FromRgb(AbyssTouchStyle::GhostFill, 0.88f);
				Label = LocalizeOr("sys.mobile.panel.character", TEXT("Hero"));
				Icon = HudIcon(TEXT("character"));
				bCaption = true;
				break;
			case EAbyssTouchControl::PanelSkills:
				Fill = AbyssTouchStyle::FromRgb(AbyssTouchStyle::GhostFill, 0.88f);
				Label = LocalizeOr("sys.mobile.panel.skills", TEXT("Skills"));
				Icon = HudIcon(TEXT("skills"));
				bCaption = true;
				break;
			case EAbyssTouchControl::PanelWorldMap:
				Fill = AbyssTouchStyle::FromRgb(AbyssTouchStyle::GhostFill, 0.88f);
				Label = LocalizeOr("sys.mobile.panel.map", TEXT("Map"));
				Icon = HudIcon(TEXT("map"));
				bCaption = true;
				break;
			case EAbyssTouchControl::PanelPets:
				Fill = AbyssTouchStyle::FromRgb(AbyssTouchStyle::GhostFill, 0.88f);
				Label = LocalizeOr("sys.mobile.panel.pets", TEXT("Pets"));
				Icon = HudIcon(TEXT("pets"));
				bCaption = true;
				break;
			case EAbyssTouchControl::PanelQuestLog:
				Fill = AbyssTouchStyle::FromRgb(AbyssTouchStyle::GhostFill, 0.88f);
				Label = LocalizeOr("sys.mobile.panel.quest", TEXT("Quest"));
				Icon = HudIcon(TEXT("quest"));
				bCaption = true;
				break;
			case EAbyssTouchControl::Menu:
				Fill = AbyssTouchStyle::FromRgb(AbyssTouchStyle::GhostFill, 0.88f);
				Label = LocalizeOr("sys.mobile.panel.menu", TEXT("Menu"));
				Icon = HudIcon(TEXT("menu"));
				bCaption = true;
				break;
			default:
				break;
			}

			TSharedRef<SAbyssTouchButton> NewButton = SNew(SAbyssTouchButton)
				.Shape(Placement.Shape)
				.DrawSize(Placement.DrawSize)
				.HitScale(Placement.HitScale)
				.FillColor(Fill)
				.OutlineColor(Outline)
				.BackgroundBrush(Face)
				.CaptionBelowIcon(bCaption)
				.Font(Font)
				.OnPressed(FSimpleDelegate::CreateSP(this, &SAbyssTouchControls::HandleControlPressed, Placement.Control));
			NewButton->SetLabel(Label);
			NewButton->SetIcon(Icon);
			if (Placement.Control == EAbyssTouchControl::Interact)
			{
				NewButton->SetVisibility(EVisibility::Collapsed);
			}
			Buttons[AbyssTouch_Index(Placement.Control)] = NewButton;
			Widget = NewButton;
		}

		const FVector2D WidgetSize = Placement.WidgetSize();
		Canvas->AddSlot()
			.Anchors(FAnchors(0.0f, 0.0f))
			.Alignment(FVector2D(0.5, 0.5))
			.AutoSize(false)
			.Offset(FMargin(static_cast<float>(Placement.Center.X), static_cast<float>(Placement.Center.Y),
				static_cast<float>(WidgetSize.X), static_cast<float>(WidgetSize.Y)))
			[
				Widget.ToSharedRef()
			];
	}
}

// =====================================================================================================================
// Per-frame state
// =====================================================================================================================

void SAbyssTouchControls::UpdateCooldownButton(EAbyssTouchControl Control, double RemainingMs, double TotalMs, bool bBlocked)
{
	const TSharedPtr<SAbyssTouchButton> Target = Button(Control);
	if (!Target.IsValid())
	{
		return;
	}
	const double Remaining = FMath::Max(0.0, RemainingMs);
	// save-ui-input 5.7.3: total 0 (unknown) -> full dark sweep while cooling.
	const float Fraction = Remaining <= 0.0 ? 0.0f : (TotalMs > 0.0 ? static_cast<float>(FMath::Clamp(Remaining / TotalMs, 0.0, 1.0)) : 1.0f);
	Target->SetCooldown(Fraction, AbyssInputMath::FormatCooldown(Remaining));
	Target->SetDimAlpha(Remaining > 0.0 ? 0.7f : 1.0f);
	Target->SetBlocked(bBlocked);
	double& Previous = PreviousRemainingMs[AbyssTouch_Index(Control)];
	if (Previous > 0.0 && Remaining <= 0.0)
	{
		Target->PlayReadyPop();
	}
	Previous = Remaining;
}

void SAbyssTouchControls::UpdateDynamicState(const abyss::Snapshot& Snap, const UAbyssGameInstance& GameInstance)
{
	const abyss::HeroView& Hero = Snap.hero;
	// 5.1.1: touch skill / dodge buttons show disabled while the hero is Dying.
	const bool bDead = Hero.life != abyss::HeroLife::Alive || Hero.hp <= 0.0;

	// ---- skills (C3 hotbar) ----
	for (int32 Slot = 0; Slot < 6; ++Slot)
	{
		const EAbyssTouchControl Control = AbyssTouch_SkillControl(Slot);
		const TSharedPtr<SAbyssTouchButton> SkillButton = Button(Control);
		if (!SkillButton.IsValid())
		{
			continue;
		}
		const abyss::SkillSlotView& SlotView = Hero.hotbar[static_cast<size_t>(Slot)];
		const FString SkillId = AbyssText::ToFString(SlotView.skillId);
		if (SkillId != SlotSkillIds[Slot])
		{
			SlotSkillIds[Slot] = SkillId;
			SkillButton->SetIcon(Visuals.SkillIcon ? Visuals.SkillIcon(SkillId) : nullptr);
			const std::string NameKey = "data.skill." + SlotView.skillId + ".name";
			SkillButton->SetLabel(AbyssTouch_ShortName(LocalizeOr(NameKey, *SkillId)));
		}
		UpdateCooldownButton(Control, SlotView.cooldownRemainingMs, SlotView.cooldownTotalMs, bDead);
	}

	// ---- dodge, lock ----
	UpdateCooldownButton(EAbyssTouchControl::Dodge, Hero.dodgeCooldownRemainingMs, Hero.dodgeCooldownMs, bDead);
	if (const TSharedPtr<SAbyssTouchButton> LockButton = Button(EAbyssTouchControl::Lock))
	{
		LockButton->SetActive(Hero.target != abyss::kNoEntity);
		LockButton->SetBlocked(bDead);
	}

	// ---- potions (I4) ----
	const EAbyssTouchControl PotionControls[2] = { EAbyssTouchControl::PotionHp, EAbyssTouchControl::PotionMp };
	for (int32 PotionIndex = 0; PotionIndex < 2; ++PotionIndex)
	{
		if (const TSharedPtr<SAbyssTouchButton> PotionButton = Button(PotionControls[PotionIndex]))
		{
			const int32 Count = Snap.potionSlots[static_cast<size_t>(PotionIndex)].count;
			PotionButton->SetBadge(FString::FromInt(FMath::Max(0, Count)));
			PotionButton->SetDimAlpha(Count > 0 ? 1.0f : 0.5f);
			PotionButton->SetBlocked(bDead);
		}
	}

	// ---- town portal (W3, world 7.4 section 3): channel sweep, dim while the core would refuse ----
	if (const TSharedPtr<SAbyssTouchButton> PortalButton = Button(EAbyssTouchControl::Portal))
	{
		if (Hero.portaling)
		{
			PortalButton->SetCooldown(1.0f - static_cast<float>(FMath::Clamp(Hero.portalProgress, 0.0, 1.0)), FString());
			PortalButton->SetDimAlpha(1.0f);
		}
		else
		{
			PortalButton->SetCooldown(0.0f, FString());
			PortalButton->SetDimAlpha(Hero.portalRefusal == abyss::PortalRefusal::None ? 1.0f : 0.5f);
		}
		PortalButton->SetBlocked(bDead);
	}

	// ---- context Talk / Use button (Q6) ----
	if (const TSharedPtr<SAbyssTouchButton> InteractButton = Button(EAbyssTouchControl::Interact))
	{
		const abyss::InteractPromptView& Prompt = Snap.prompt;
		const bool bShowInteract = !bDead && Prompt.kind != abyss::InteractKind::None
			&& abyss::Dist(Hero.pos, Prompt.pos) <= AbyssTouch_InteractButtonRange;
		InteractButton->SetVisibility(bShowInteract ? EVisibility::Visible : EVisibility::Collapsed);
		const int32 PromptKind = static_cast<int32>(Prompt.kind);
		if (bShowInteract && PromptKind != LabelledPromptKind)
		{
			LabelledPromptKind = PromptKind;
			switch (Prompt.kind)
			{
			case abyss::InteractKind::Npc:
				InteractButton->SetLabel(LocalizeOr("sys.mobile.interact.talk", TEXT("Talk")));
				InteractButton->SetIcon(HudIcon(TEXT("talk")));
				break;
			case abyss::InteractKind::Loot:
				InteractButton->SetLabel(LocalizeOr("sys.mobile.interact.pickup", TEXT("Pick up")));
				InteractButton->SetIcon(HudIcon(TEXT("pickup")));
				break;
			case abyss::InteractKind::HiddenReward:
				InteractButton->SetLabel(LocalizeOr("sys.mobile.interact.open", TEXT("Open")));
				InteractButton->SetIcon(HudIcon(TEXT("open")));
				break;
			default:
				InteractButton->SetLabel(LocalizeOr("sys.mobile.interact.use", TEXT("Use")));
				InteractButton->SetIcon(HudIcon(TEXT("use")));
				break;
			}
		}
	}

	// ---- top-left toggles ----
	if (const TSharedPtr<SAbyssTouchButton> AutoButton = Button(EAbyssTouchControl::AutoCombat))
	{
		const int32 State = Hero.autoCombat ? 1 : 0;
		if (State != LabelledAutoCombat)
		{
			LabelledAutoCombat = State;
			AutoButton->SetLabel(Hero.autoCombat ? LocalizeOr("sys.mobile.autoCombat.on", TEXT("Auto\nON"))
												 : LocalizeOr("sys.mobile.autoCombat.off", TEXT("Auto\nOFF")));
			AutoButton->SetLabelColor(AbyssTouchStyle::FromRgb(Hero.autoCombat ? AbyssTouchStyle::ToggleOn : AbyssTouchStyle::ToggleOff));
			AutoButton->SetActive(Hero.autoCombat);
		}
	}
	if (const TSharedPtr<SAbyssTouchButton> LootButton = Button(EAbyssTouchControl::AutoLoot))
	{
		const int32 Mode = static_cast<int32>(Hero.autoLoot);
		if (Mode != LabelledAutoLoot)
		{
			LabelledAutoLoot = Mode;
			const std::string_view ModeName = abyss::EnumName(Hero.autoLoot);
			const std::string LabelKey = std::string("ui.hud.autoLoot.") + std::string(ModeName);
			LootButton->SetLabel(LocalizeOr(LabelKey, *AbyssText::ToFString(ModeName)));
			const abyss::UiThemeDef* Theme = GameInstance.GetData() ? &GameInstance.GetData()->UiTheme() : nullptr;
			FLinearColor ModeColor = AbyssTouchStyle::FromRgb(AbyssTouchStyle::ToggleOff);
			switch (Hero.autoLoot)
			{
			case abyss::AutoLootMode::Off:
				break;
			case abyss::AutoLootMode::All:
				ModeColor = AbyssTouchStyle::FromRgb(AbyssTouchStyle::AutoLootAll);
				break;
			case abyss::AutoLootMode::Magic:
			case abyss::AutoLootMode::Rare:
			case abyss::AutoLootMode::Legendary:
				if (Theme != nullptr)
				{
					const abyss::ItemQuality Quality = Hero.autoLoot == abyss::AutoLootMode::Magic ? abyss::ItemQuality::Magic
						: Hero.autoLoot == abyss::AutoLootMode::Rare ? abyss::ItemQuality::Rare
						: abyss::ItemQuality::Legendary;
					ModeColor = AbyssTouchStyle::FromHex(
						AbyssText::ToFString(Theme->qualityHex[abyss::EnumIndex(Quality)]), ModeColor);
				}
				break;
			}
			LootButton->SetLabelColor(ModeColor);
			LootButton->SetActive(Hero.autoLoot != abyss::AutoLootMode::Off);
		}
	}
}

// =====================================================================================================================
// Presses
// =====================================================================================================================

void SAbyssTouchControls::HandleControlPressed(EAbyssTouchControl Control)
{
	UAbyssInputSubsystem* Subsystem = InputSubsystem.Get();
	if (Subsystem == nullptr)
	{
		return;
	}
	FAbyssUiInputRequest UiRequest;
	UiRequest.Device = EAbyssInputDevice::Touch;
	switch (Control)
	{
	case EAbyssTouchControl::Joystick:
		return;
	case EAbyssTouchControl::Lock:
		Subsystem->PressAction(EAbyssInputAction::TargetCycle);
		return;
	case EAbyssTouchControl::Skill1:
	case EAbyssTouchControl::Skill2:
	case EAbyssTouchControl::Skill3:
	case EAbyssTouchControl::Skill4:
	case EAbyssTouchControl::Skill5:
	case EAbyssTouchControl::Skill6:
		Subsystem->PressAction(AbyssTouch_SkillAction(AbyssTouch_Index(Control) - AbyssTouch_Index(EAbyssTouchControl::Skill1)));
		return;
	case EAbyssTouchControl::Dodge:
		Subsystem->PressAction(EAbyssInputAction::Dodge);
		return;
	case EAbyssTouchControl::PotionHp:
		Subsystem->PressAction(EAbyssInputAction::PotionHp);
		return;
	case EAbyssTouchControl::PotionMp:
		Subsystem->PressAction(EAbyssInputAction::PotionMp);
		return;
	case EAbyssTouchControl::Interact:
		Subsystem->PressAction(EAbyssInputAction::Interact);
		return;
	case EAbyssTouchControl::AutoCombat:
		Subsystem->PressAction(EAbyssInputAction::ToggleAutoCombat);
		return;
	case EAbyssTouchControl::AutoLoot:
		Subsystem->PressAction(EAbyssInputAction::CycleAutoLoot);
		return;
	case EAbyssTouchControl::Portal:
		Subsystem->PressAction(EAbyssInputAction::TownPortal);   // a dimmed press still runs: the core logs its refusal
		return;
	case EAbyssTouchControl::Log:
		UiRequest.Kind = EAbyssUiRequest::ToggleCombatLog;
		Subsystem->RouteUiRequest(UiRequest);
		return;
	case EAbyssTouchControl::Menu:
		UiRequest.Kind = EAbyssUiRequest::OpenSystemMenu;
		Subsystem->RouteUiRequest(UiRequest);
		return;
	case EAbyssTouchControl::PanelInventory:
		Subsystem->PressAction(EAbyssInputAction::PanelInventory);
		return;
	case EAbyssTouchControl::PanelCharacter:
		Subsystem->PressAction(EAbyssInputAction::PanelCharacter);
		return;
	case EAbyssTouchControl::PanelSkills:
		Subsystem->PressAction(EAbyssInputAction::PanelSkills);
		return;
	case EAbyssTouchControl::PanelWorldMap:
		Subsystem->PressAction(EAbyssInputAction::PanelWorldMap);
		return;
	case EAbyssTouchControl::PanelPets:
		Subsystem->PressAction(EAbyssInputAction::PanelPets);
		return;
	case EAbyssTouchControl::PanelQuestLog:
		Subsystem->PressAction(EAbyssInputAction::PanelQuestLog);
		return;
	case EAbyssTouchControl::Count:
		return;
	}
}

void SAbyssTouchControls::GetOccupiedRects(TArray<FSlateRect>& OutRects) const
{
	OutRects.Reset();
	if (!bShown || !bHasLayout)
	{
		return;
	}
	for (const FAbyssTouchPlacement& Placement : Placements)
	{
		if (Placement.Control != EAbyssTouchControl::Joystick)
		{
			const TSharedPtr<SAbyssTouchButton> Existing = Button(Placement.Control);
			if (!Existing.IsValid() || Existing->GetVisibility() == EVisibility::Collapsed)
			{
				continue;
			}
		}
		const FVector2D Half = Placement.WidgetSize() * 0.5;
		OutRects.Add(FSlateRect(Placement.Center - Half, Placement.Center + Half));
	}
}
