#include "UI/Hud/SAbyssHudBottom.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"

#include <string>

#include "abyss/data/DataStore.h"
#include "abyss/items/Inventory.h"

#include "Framework/AbyssText.h"
#include "UI/AbyssUiStyle.h"
#include "UI/Core/AbyssUiDraw.h"
#include "UI/Widgets/AbyssUiWidgets.h"

namespace
{
	FVector2D AbyssHudBottom_Local(double X, double Y)
	{
		return FVector2D(X, Y - SAbyssHudBottom::RegionTop);
	}

	FLinearColor AbyssHudBottom_AutoLootColor(const FAbyssUiStyle& UiStyle, abyss::AutoLootMode Mode)
	{
		switch (Mode)
		{
		case abyss::AutoLootMode::Off: return FAbyssUiStyle::Rgb(0xb0a8b4);
		case abyss::AutoLootMode::All: return FAbyssUiStyle::Rgb(0xe0d8cc);
		case abyss::AutoLootMode::Magic: return UiStyle.QualityColor(abyss::ItemQuality::Magic);
		case abyss::AutoLootMode::Rare: return UiStyle.QualityColor(abyss::ItemQuality::Rare);
		case abyss::AutoLootMode::Legendary: return UiStyle.QualityColor(abyss::ItemQuality::Legendary);
		}
		return FAbyssUiStyle::Rgb(0xb0a8b4);
	}
}

void SAbyssHudBottom::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const TSharedRef<FAbyssHudState>& InState)
{
	Ctx = InContext;
	State = InState;
	SetVisibility(EVisibility::SelfHitTestInvisible);
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(1280.f)
		.HeightOverride(RegionHeight)
		[
			SAssignNew(HitCanvas, SCanvas)
			.Visibility(EVisibility::SelfHitTestInvisible)
		]
	];
	RebuildLayout();
}

SAbyssHudBottom::FLayout SAbyssHudBottom::MakeLayout(bool bTouch)
{
	FLayout L;
	L.bTouch = bTouch;
	if (!bTouch)
	{
		// save-ui-input 6.2 desktop column.
		L.OrbR = 44.f;
		L.HpOrb = AbyssHudBottom_Local(361.0, 668.0);
		L.MpOrb = AbyssHudBottom_Local(919.0, 668.0);
		L.PlatePos = AbyssHudBottom_Local(389.0, 628.0);
		L.PlateSize = FVector2D(502.0, 96.0);
		L.SpiritPos = AbyssHudBottom_Local(447.0, 639.0);
		L.SpiritSize = FVector2D(254.0, 6.0);
		L.SpiritLabel = AbyssHudBottom_Local(407.0, 642.0);
		L.ExpPos = AbyssHudBottom_Local(407.0, 705.0);
		L.ExpSize = FVector2D(466.0, 8.0);
		L.SkillSize = 44.f;
		L.SkillStart = AbyssHudBottom_Local(407.0, 654.0);
		L.SkillGap = 6.f;
		L.UtilityStart = AbyssHudBottom_Local(713.0, 654.0);
		L.UtilitySize = FVector2D(50.0, 44.0);
		L.UtilityGap = 5.f;
		// I4 potion quick slots: medallions at the inner top of each orb (port addition).
		L.PotionCenters[0] = AbyssHudBottom_Local(322.0, 616.0);
		L.PotionCenters[1] = AbyssHudBottom_Local(958.0, 616.0);
		L.PotionR = 15.f;
		L.PromptCenter = AbyssHudBottom_Local(640.0, 612.0);
		L.PortalCenter = AbyssHudBottom_Local(640.0, 586.0);
		L.FontScale = 1.f;
	}
	else
	{
		// save-ui-input 6.2 touch column (no skill row / utility: the touch layer has them).
		L.OrbR = 50.f;
		L.HpOrb = AbyssHudBottom_Local(350.0, 662.0);
		L.MpOrb = AbyssHudBottom_Local(768.0, 662.0);
		L.PlatePos = AbyssHudBottom_Local(350.0, 636.0);
		L.PlateSize = FVector2D(418.0, 88.0);
		L.SpiritPos = AbyssHudBottom_Local(456.0, 654.0);
		L.SpiritSize = FVector2D(192.0, 8.0);
		L.SpiritLabel = AbyssHudBottom_Local(412.0, 658.0);
		L.ExpPos = AbyssHudBottom_Local(412.0, 690.0);
		L.ExpSize = FVector2D(294.0, 12.0);
		L.SkillSize = 0.f;
		L.PromptCenter = AbyssHudBottom_Local(559.0, 616.0);
		L.PortalCenter = AbyssHudBottom_Local(559.0, 596.0);
		L.FontScale = 1.6f;
	}
	return L;
}

float SAbyssHudBottom::Px(float Desktop) const
{
	// save-ui-input 6.1 hfs(n): desktop n, touch max(round(1.6 n), 18).
	return Layout.bTouch ? FMath::Max(FMath::RoundToFloat(Desktop * 1.6f), 18.f) : Desktop;
}

TSharedRef<SWidget> SAbyssHudBottom::MakeHitArea(TFunction<void()> OnPress, TFunction<void()> OnRightPress, TSharedPtr<SWidget>& OutWidget)
{
	FSimpleDelegate Press;
	if (OnPress)
	{
		Press = FSimpleDelegate::CreateLambda(MoveTemp(OnPress));
	}
	FSimpleDelegate RightPress;
	if (OnRightPress)
	{
		RightPress = FSimpleDelegate::CreateLambda(MoveTemp(OnRightPress));
	}
	TSharedRef<SWidget> Area = SNew(SAbyssHitArea, Ctx.ToSharedRef())
		.OnPressed(Press)
		.OnRightPressed(RightPress)
		[
			SNullWidget::NullWidget
		];
	OutWidget = Area;
	return Area;
}

void SAbyssHudBottom::RebuildLayout()
{
	Layout = MakeLayout(Ctx->IsTouch());
	HitCanvas->ClearChildren();
	for (TSharedPtr<SWidget>& Widget : SkillButtons)
	{
		Widget.Reset();
	}
	for (TSharedPtr<SWidget>& Widget : UtilityButtons)
	{
		Widget.Reset();
	}
	for (TSharedPtr<SWidget>& Widget : PotionButtons)
	{
		Widget.Reset();
	}
	for (TSharedPtr<SWidget>& Widget : BlockAreas)
	{
		Widget.Reset();
	}

	// Blockers over the plate and the orbs (clicks there never walk the hero, Q12).
	const auto AddBlock = [this](int32 Index, const FVector2D& Pos, const FVector2D& Size)
	{
		HitCanvas->AddSlot()
			.Position(Pos)
			.Size(Size)
			[
				MakeHitArea(nullptr, nullptr, BlockAreas[Index])
			];
	};
	AddBlock(0, Layout.PlatePos, FVector2D(Layout.PlateSize.X, RegionHeight - Layout.PlatePos.Y));
	AddBlock(1, Layout.HpOrb - FVector2D(Layout.OrbR, Layout.OrbR), FVector2D(Layout.OrbR * 2.f, Layout.OrbR * 2.f));
	AddBlock(2, Layout.MpOrb - FVector2D(Layout.OrbR, Layout.OrbR), FVector2D(Layout.OrbR * 2.f, Layout.OrbR * 2.f));

	if (Layout.bTouch)
	{
		return;
	}
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	for (int32 Slot = 0; Slot < 6; ++Slot)
	{
		const FVector2D Pos = Layout.SkillStart + FVector2D((Layout.SkillSize + Layout.SkillGap) * Slot, 0.0);
		const EAbyssInputAction Action = static_cast<EAbyssInputAction>(static_cast<int32>(EAbyssInputAction::Skill1) + Slot);
		HitCanvas->AddSlot()
			.Position(Pos)
			.Size(FVector2D(Layout.SkillSize, Layout.SkillSize))
			[
				// Skill-bar clicks share the keyboard path (UAbyssInputSubsystem::PressAction): no cursor aim (6.6).
				MakeHitArea([Context, Action]() { Context->PressAction(Action); }, nullptr, SkillButtons[Slot])
			];
	}
	const EAbyssInputAction UtilityActions[2] = { EAbyssInputAction::ToggleAutoCombat, EAbyssInputAction::CycleAutoLoot };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FVector2D Pos = Layout.UtilityStart + FVector2D((Layout.UtilitySize.X + Layout.UtilityGap) * Index, 0.0);
		TFunction<void()> OnPress;
		if (Index < 2)
		{
			const EAbyssInputAction Action = UtilityActions[Index];
			OnPress = [Context, Action]() { Context->PressAction(Action); };
		}
		else
		{
			OnPress = [Context]()
			{
				if (IAbyssUiHost* Host = Context->GetHost())
				{
					Host->TogglePanel(abyss::PanelId::Inventory);
				}
			};
		}
		HitCanvas->AddSlot()
			.Position(Pos)
			.Size(Layout.UtilitySize)
			[
				MakeHitArea(MoveTemp(OnPress), nullptr, UtilityButtons[Index])
			];
	}
	for (int32 Slot = 0; Slot < 2; ++Slot)
	{
		const EAbyssInputAction Action = Slot == 0 ? EAbyssInputAction::PotionHp : EAbyssInputAction::PotionMp;
		const float R = Layout.PotionR + 2.f;
		HitCanvas->AddSlot()
			.Position(Layout.PotionCenters[Slot] - FVector2D(R, R))
			.Size(FVector2D(R * 2.f, R * 2.f))
			[
				MakeHitArea([Context, Action]() { Context->PressAction(Action); }, [this, Slot]() { ShowPotionPopup(Slot); }, PotionButtons[Slot])
			];
	}
}

void SAbyssHudBottom::ShowPotionPopup(int32 Slot)
{
	// I4: bind a quick slot to a potion base in the bag (or back to "best available").
	const abyss::Snapshot* Snap = Ctx->GetSnapshot();
	const abyss::DataStore* Data = Ctx->GetData();
	if (Snap == nullptr || Data == nullptr || Snap->inventory == nullptr)
	{
		return;
	}
	const abyss::ConsumableEffect Wanted = Slot == 0 ? abyss::ConsumableEffect::Heal : abyss::ConsumableEffect::Mana;
	TArray<std::string> Bases;
	for (const abyss::ItemInstance& Item : Snap->inventory->Bag())
	{
		const abyss::ItemBaseDef* Base = Data->FindItemBase(Item.baseId);
		if (Base != nullptr && Base->consumableEffect == Wanted && !Bases.Contains(Item.baseId))
		{
			Bases.Add(Item.baseId);
		}
	}
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const abyss::PotionSlot PotionSlot = Slot == 0 ? abyss::PotionSlot::Hp : abyss::PotionSlot::Mp;
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	List->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.f, 0.f, 0.f, 4.f))
		[
			SNew(SAbyssButton, Context)
			.Text(Context->LocOr("ui.hud.potionBest", TEXT("Best available")))
			.Kind(EAbyssButtonKind::Ghost)
			.FontPx(12.f)
			.Width(160.f)
			.Height(26.f)
			.OnClicked_Lambda([Context, PotionSlot]()
			{
				Context->Submit(abyss::CmdSetPotionSlot{ PotionSlot, std::string() });
				Context->ClosePopup();
			})
		];
	for (const std::string& BaseId : Bases)
	{
		List->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 0.f, 0.f, 4.f))
			[
				SNew(SAbyssButton, Context)
				.Text(FText::AsCultureInvariant(Context->ItemBaseName(BaseId)))
				.Kind(EAbyssButtonKind::Secondary)
				.FontPx(12.f)
				.Width(160.f)
				.Height(26.f)
				.OnClicked_Lambda([Context, PotionSlot, BaseId]()
				{
					Context->Submit(abyss::CmdSetPotionSlot{ PotionSlot, BaseId });
					Context->ClosePopup();
				})
			];
	}
	const TSharedPtr<SWidget> Anchor = PotionButtons[Slot];
	FVector2D Position = FSlateApplication::Get().GetCursorPos();
	if (Anchor.IsValid())
	{
		Position = FVector2D(Anchor->GetTickSpaceGeometry().GetAbsolutePosition());
	}
	Context->ShowPopup(SNew(SAbyssCardFrame, Context)[List], Position);
}

// =====================================================================================================================
// Paint
// =====================================================================================================================

int32 SAbyssHudBottom::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (!State->bValid)
	{
		return LayerId;
	}
	const double Now = Ctx->Now();
	FAbyssPainter P(AllottedGeometry, OutDrawElements, LayerId, InWidgetStyle, Ctx->Style());
	PaintPortal(P);
	PaintPrompt(P);
	PaintPlate(P);
	PaintSpirit(P, Now);
	PaintExp(P);
	if (!Layout.bTouch)
	{
		for (int32 Slot = 0; Slot < 6; ++Slot)
		{
			PaintSkillSlot(P, Slot, Now);
		}
		PaintUtility(P);
	}
	P.NextLayer();
	PaintOrb(P, Layout.HpOrb, Layout.OrbR, true, Now);
	PaintOrb(P, Layout.MpOrb, Layout.OrbR, false, Now);
	if (!Layout.bTouch)
	{
		PaintPotion(P, 0);
		PaintPotion(P, 1);
	}
	const int32 Layer = P.Layer;
	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, Layer + 1, InWidgetStyle, bParentEnabled);
}

void SAbyssHudBottom::PaintPlate(FAbyssPainter& P) const
{
	// 8.5 HUD plate: iron-framed, #2c2730 -> #1a171d -> #0e0c10.
	const FVector2D Pos = Layout.PlatePos;
	const FVector2D Size(Layout.PlateSize.X, RegionHeight - Pos.Y + 6.0);
	P.RoundBox(Pos + FVector2D(0.0, -3.0), Size + FVector2D(0.0, 3.0), FLinearColor(0.f, 0.f, 0.f, 0.4f), 8.f);
	P.RoundBox(Pos, Size, FAbyssUiStyle::Rgb(0x1a171d), 7.f);
	P.VerticalGradient3(Pos + FVector2D(2.0, 6.0), Size - FVector2D(4.0, 6.0), FAbyssUiStyle::Rgb(0x2c2730), FAbyssUiStyle::Rgb(0x1a171d), 0.5f,
		FAbyssUiStyle::Rgb(0x0e0c10), 10);
	P.RoundBox(Pos, Size, FLinearColor::Transparent, 7.f, FAbyssUiStyle::Rgb(0x4a444f), 3.f);
	P.RoundBox(Pos + FVector2D(1.5, 1.5), Size - FVector2D(3.0, 3.0), FLinearColor::Transparent, 6.f,
		FAbyssUiStyle::WithAlpha(Ctx->Style().Colors().Gold, 0.5f), 1.f);
	P.Line(Pos + FVector2D(8.0, 1.5), Pos + FVector2D(Size.X - 8.0, 1.5), FLinearColor(1.f, 0.9f, 0.8f, 0.18f), 1.f);
}

void SAbyssHudBottom::PaintOrb(FAbyssPainter& P, const FVector2D& Center, float Radius, bool bHp, double Now) const
{
	const FAbyssHudState& S = *State;
	const FLinearColor Liquid = bHp ? FAbyssUiStyle::Rgb(0xc0281e) : FAbyssUiStyle::Rgb(0x2a5fd6);
	const float Level = bHp ? S.HpLevel : S.MpLevel;
	const double Value = bHp ? S.Hp : S.Mana;
	const double Max = FMath::Max(1.0, bHp ? S.MaxHp : S.MaxMana);
	const FAbyssUiPalette& C = Ctx->Style().Colors();

	// dark back, liquid (masked to the circle), glass shine, ornate rim (6.3)
	P.Circle(Center + FVector2D(0.0, 3.0), Radius + 4.f, FLinearColor(0.f, 0.f, 0.f, 0.5f));
	P.Circle(Center, Radius, FAbyssUiStyle::Rgb(0x0a080c));
	FLinearColor LiquidColor = Liquid;
	const double Ratio = Value / Max;
	if (bHp && Ratio > 0.0 && Ratio < 0.3)
	{
		LiquidColor.A = static_cast<float>(0.6 + FMath::Sin(Now * 8.0) * 0.4);
	}
	const float Surface = FMath::Clamp(Level, 0.f, 1.f);
	const float Phase = S.WavePhase * (bHp ? 1.f : -0.8f);
	P.CircleLiquid(Center, Radius - 1.f, Surface, Phase, 2.2f, FAbyssUiStyle::Darken(LiquidColor, 0.25f));
	P.CircleLiquid(Center, Radius - 1.f, FMath::Max(0.f, Surface - 0.04f), Phase + 1.2f, 1.6f, LiquidColor);
	// glass shine and rim
	P.Circle(Center + FVector2D(-Radius * 0.3, -Radius * 0.38), Radius * 0.34f, FLinearColor(1.f, 1.f, 1.f, 0.08f));
	P.Circle(Center + FVector2D(-Radius * 0.36, -Radius * 0.46), Radius * 0.14f, FLinearColor(1.f, 1.f, 1.f, 0.16f));
	P.Ring(Center, Radius + 1.f, FAbyssUiStyle::Rgb(0x050407), 2.f);
	P.Ring(Center, Radius + 3.5f, FAbyssUiStyle::Rgb(0x4a444f), 3.f);
	P.Ring(Center, Radius + 5.f, FAbyssUiStyle::WithAlpha(C.Gold, 0.85f), 1.2f);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const float A = UE_HALF_PI * Index - UE_HALF_PI;
		P.Diamond(Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (Radius + 4.f), 6.f, C.Gold);
	}
	const FString Text = FString::Printf(TEXT("%s/%s"), *FAbyssUiContext::Int(static_cast<int64>(FMath::CeilToDouble(FMath::Max(0.0, Value)))),
		*FAbyssUiContext::Int(static_cast<int64>(FMath::RoundToDouble(Max))));
	P.TextCentered(Center, Text, Ctx->Style().Body(Layout.bTouch ? 21.f : 13.f, true, 3), FLinearColor::White);
}

void SAbyssHudBottom::PaintSpirit(FAbyssPainter& P, double Now) const
{
	const FAbyssHudState& S = *State;
	const FAbyssUiStyle& UiStyle = Ctx->Style();
	const FSlateFontInfo LabelFont = UiStyle.Body(Px(10.f), true, 1);
	const FString Label = Ctx->LocOrStr("ui.hud.spirit", TEXT("Spirit"));
	const FVector2D LabelSize = FAbyssPainter::Measure(Label, LabelFont);
	P.Text(FVector2D(Layout.SpiritLabel.X, Layout.SpiritLabel.Y - LabelSize.Y * 0.5), Label, LabelFont, FAbyssUiStyle::Rgb(0xf0b060));
	const float Fraction = static_cast<float>(S.Spirit / FMath::Max(1.0, S.SpiritMax));
	FLinearColor Fill = S.SpiritColor;
	if (S.bResonating)
	{
		Fill.A = static_cast<float>(0.82 + FMath::Sin(Now * 12.0) * 0.18);
	}
	P.Bar(Layout.SpiritPos, Layout.SpiritSize, Fraction, Fill);
	const FSlateFontInfo ValueFont = UiStyle.Body(Px(10.f), false, 1);
	const FString Value = FString::Printf(TEXT("%s/%s"), *FAbyssUiContext::Int(static_cast<int64>(FMath::FloorToDouble(S.Spirit))),
		*FAbyssUiContext::Num(S.SpiritMax));
	const FVector2D ValueSize = FAbyssPainter::Measure(Value, ValueFont);
	P.Text(FVector2D(Layout.SpiritPos.X + Layout.SpiritSize.X + 6.0, Layout.SpiritPos.Y + Layout.SpiritSize.Y * 0.5 - ValueSize.Y * 0.5), Value,
		ValueFont, FAbyssUiStyle::Rgb(0xf0c080));
	if (S.bResonating)
	{
		const FString Text = Ctx->LocArgsOrStr("ui.hud.resonance", TEXT("Resonance {seconds}s"),
			{ FAbyssUiContext::Arg("seconds", FAbyssUiContext::Fixed(S.ResonanceRemainingMs / 1000.0, 1)) });
		P.TextCentered(Layout.SpiritPos + FVector2D(Layout.SpiritSize.X * 0.5, -(Layout.bTouch ? 22.0 : 14.0)), Text, UiStyle.Body(Px(11.f), true, 2),
			FAbyssUiStyle::Rgb(0xffd27a));
	}
}

void SAbyssHudBottom::PaintExp(FAbyssPainter& P) const
{
	const FAbyssHudState& S = *State;
	const float Fraction = FMath::Clamp(static_cast<float>(static_cast<double>(S.Exp) / FMath::Max<double>(1.0, static_cast<double>(S.ExpToNext))), 0.f, 1.f);
	P.Bar(Layout.ExpPos, Layout.ExpSize, Fraction, FAbyssUiStyle::Rgb(0x9b4fd0), true);
	// 6.5: "Lv.{level}  ({exp}/{expToNext})" (no i18n key in the web either).
	const FString Text = FString::Printf(TEXT("Lv.%d  (%lld/%lld)"), S.Level, static_cast<long long>(S.Exp), static_cast<long long>(S.ExpToNext));
	P.TextCentered(Layout.ExpPos + Layout.ExpSize * 0.5, Text, Ctx->Style().Body(Px(10.f), true, 2), FAbyssUiStyle::Rgb(0xecd9ff));
}

void SAbyssHudBottom::PaintSkillSlot(FAbyssPainter& P, int32 Slot, double Now) const
{
	const FAbyssHudState& S = *State;
	const FAbyssHudSkillSlot& Info = S.Slots[Slot];
	const FAbyssUiStyle& UiStyle = Ctx->Style();
	const float Size = Layout.SkillSize;
	const FVector2D Pos = Layout.SkillStart + FVector2D((Size + Layout.SkillGap) * Slot, 0.0);
	const FVector2D Center = Pos + FVector2D(Size * 0.5f, Size * 0.5f);
	const bool bHovered = SkillButtons[Slot].IsValid() && SkillButtons[Slot]->IsHovered();

	// slot frame (8.5 skill slot): dark well, iron border, hover glow rgba(255,200,110,0.8)
	if (bHovered)
	{
		P.RoundBox(Pos - FVector2D(2.0, 2.0), FVector2D(Size + 4.f, Size + 4.f), FLinearColor::Transparent, 6.f, FLinearColor(1.f, 0.58f, 0.15f, 0.8f), 2.f);
	}
	P.RoundBox(Pos, FVector2D(Size, Size), FAbyssUiStyle::Rgb(0x0c0b0e), 4.f, FAbyssUiStyle::Rgb(0x4a444f), 1.5f);
	const FString KeyLabel = [this, Slot]()
	{
		const FString Hint = Ctx->KeyHint(static_cast<EAbyssInputAction>(static_cast<int32>(EAbyssInputAction::Skill1) + Slot));
		return Hint.IsEmpty() ? FString::FromInt(Slot + 1) : Hint;
	}();
	if (!Info.bBound)
	{
		P.TextCentered(Center + FVector2D(13.0, 14.0), KeyLabel, UiStyle.Body(9.f, true, 1), FAbyssUiStyle::Rgb(0x4a4450));
		return;
	}
	const float IconSize = 36.f;
	const FVector2D IconPos = Center - FVector2D(IconSize * 0.5f, IconSize * 0.5f);
	FLinearColor IconTint = FLinearColor::White;
	if (!Info.bAffordable)
	{
		IconTint = FLinearColor(0.45f, 0.5f, 0.8f, 0.85f);   // not enough mana
	}
	if (const FSlateBrush* Icon = Ctx->SkillIcon(Info.SkillId))
	{
		P.Brush(IconPos, FVector2D(IconSize, IconSize), Icon, IconTint);
	}
	else
	{
		P.RoundBox(IconPos, FVector2D(IconSize, IconSize), FMath::Lerp(FAbyssUiStyle::Rgb(0x2a2430), IconTint, 0.15f), 4.f);
		P.TextCentered(Center, Info.Name.Left(2), UiStyle.Body(13.f, true, 2), IconTint * UiStyle.Colors().Parchment);
	}
	// cooldown sweep (6.6): square-clipped pie from a0 = -90 + (1 - frac) * 360 clockwise to 270, black 0.66, gold hand.
	if (Info.CooldownRemainingMs > 0.0 && Info.CooldownTotalMs > 0.0)
	{
		const float Frac = FMath::Clamp(static_cast<float>(Info.CooldownRemainingMs / Info.CooldownTotalMs), 0.f, 1.f);
		const float A0 = FMath::DegreesToRadians(-90.f + (1.f - Frac) * 360.f);
		const float A1 = FMath::DegreesToRadians(270.f);
		const float Half = Size * 0.5f - 3.f;
		P.SquarePie(Center, Half, A0, A1, FLinearColor(0.f, 0.f, 0.f, 0.66f));
		P.Line(Center, Center + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * Half, FAbyssUiStyle::WithAlpha(UiStyle.Colors().GoldBright, 0.85f), 1.5f);
		const FString Seconds = FAbyssUiContext::Int(static_cast<int64>(FMath::CeilToDouble(Info.CooldownRemainingMs / 1000.0)));
		P.TextCentered(Center, Seconds, UiStyle.Body(15.f, true, 2), FLinearColor::White);
	}
	// ready flash: 38x38 #fff0c0 (additive-looking) alpha 0.55 -> 0 over 320 ms
	const float FlashT = static_cast<float>((Now - Info.ReadyFlashTime) / 0.32);
	if (FlashT >= 0.f && FlashT < 1.f)
	{
		P.RoundBox(Center - FVector2D(19.0, 19.0), FVector2D(38.0, 38.0), FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0xfff0c0), 0.55f * (1.f - FlashT)), 3.f);
	}
	// buffered glow (EvSkillBuffered)
	if (Now < Info.BufferedUntil)
	{
		P.RoundBox(Pos - FVector2D(1.0, 1.0), FVector2D(Size + 2.f, Size + 2.f), FLinearColor::Transparent, 5.f,
			FAbyssUiStyle::WithAlpha(UiStyle.Colors().GoldBright, 0.9f), 2.f);
	}
	// key badge 14x13 with the number at (+13, +14)
	const FVector2D Badge = Center + FVector2D(13.0, 14.0);
	P.RoundBox(Badge - FVector2D(7.0, 6.5), FVector2D(14.0, 13.0), FAbyssUiStyle::Rgb(0x141216), 3.f, FAbyssUiStyle::Rgb(0x6d6573), 1.f);
	P.TextCentered(Badge, KeyLabel, UiStyle.Body(9.f, true, 0), FAbyssUiStyle::Rgb(0xf0dcae));
}

void SAbyssHudBottom::PaintUtility(FAbyssPainter& P) const
{
	const FAbyssHudState& S = *State;
	const FAbyssUiStyle& UiStyle = Ctx->Style();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FVector2D Pos = Layout.UtilityStart + FVector2D((Layout.UtilitySize.X + Layout.UtilityGap) * Index, 0.0);
		const FVector2D Size = Layout.UtilitySize;
		const bool bHovered = UtilityButtons[Index].IsValid() && UtilityButtons[Index]->IsHovered();
		const FAbyssButtonColors& Colors = UiStyle.ButtonColors(Index == 2 ? EAbyssButtonKind::Primary : EAbyssButtonKind::Secondary);
		FLinearColor Top = Colors.Top;
		FLinearColor Bottom = Colors.Bottom;
		FLinearColor Border = Colors.Border;
		if (bHovered)
		{
			Top = FAbyssUiStyle::Lighten(Top, 0.18f);
			Bottom = FAbyssUiStyle::Lighten(Bottom, 0.12f);
			Border = FAbyssUiStyle::Lighten(Border, 0.3f);
		}
		P.RoundBox(Pos + FVector2D(0.0, 2.0), Size, FLinearColor(0.f, 0.f, 0.f, 0.45f), 5.f);
		P.RoundBox(Pos, Size, FMath::Lerp(Top, Bottom, 0.5f), 5.f);
		P.VerticalGradient(Pos + FVector2D(1.0, 5.0), Size - FVector2D(2.0, 10.0), Top, Bottom, 5);
		P.RoundBox(Pos, Size, FLinearColor::Transparent, 5.f, Border, 1.2f);
		FString Label;
		FLinearColor LabelColor = Colors.Label;
		switch (Index)
		{
		case 0:
			Label = S.bAutoCombat ? Ctx->LocOrStr("ui.hud.autoCombat.on", TEXT("AUTO\nON")) : Ctx->LocOrStr("ui.hud.autoCombat.off", TEXT("AUTO\nOFF"));
			LabelColor = S.bAutoCombat ? FAbyssUiStyle::Rgb(0x8ff07a) : FAbyssUiStyle::Rgb(0xb0a8b4);
			break;
		case 1:
		{
			const std::string Mode(abyss::EnumName(S.AutoLoot));
			Label = Ctx->LocOrStr("ui.hud.autoLoot." + Mode, TEXT("LOOT"));
			LabelColor = AbyssHudBottom_AutoLootColor(UiStyle, S.AutoLoot);
			break;
		}
		default:
			Label = Ctx->LocOrStr("ui.hud.inventoryBtn", TEXT("Bag\n(I)"));
			LabelColor = bHovered ? Colors.LabelHover : Colors.Label;
			break;
		}
		// two-line labels: draw each line centred
		TArray<FString> Lines;
		Label.ParseIntoArray(Lines, TEXT("\n"), true);
		const FSlateFontInfo Font = UiStyle.Body(10.f, true, 1);
		const double LineH = 12.0;
		const double StartY = Pos.Y + Size.Y * 0.5 - LineH * (Lines.Num() - 1) * 0.5;
		for (int32 Line = 0; Line < Lines.Num(); ++Line)
		{
			P.TextCentered(FVector2D(Pos.X + Size.X * 0.5, StartY + LineH * Line), Lines[Line], Font, LabelColor);
		}
	}
}

void SAbyssHudBottom::PaintPotion(FAbyssPainter& P, int32 Slot) const
{
	const FAbyssHudState& S = *State;
	const abyss::PotionSlotView& View = S.Potions[Slot];
	const FVector2D Center = Layout.PotionCenters[Slot];
	const float R = Layout.PotionR;
	const bool bHovered = PotionButtons[Slot].IsValid() && PotionButtons[Slot]->IsHovered();
	const FLinearColor Medallion = Slot == 0 ? FAbyssUiStyle::Rgb(0x6a1c1c) : FAbyssUiStyle::Rgb(0x1c2c6a);
	const bool bEmpty = View.count <= 0 || View.baseId.empty();
	P.Circle(Center + FVector2D(0.0, 1.5), R + 2.f, FLinearColor(0.f, 0.f, 0.f, 0.5f));
	P.Circle(Center, R, bEmpty ? FAbyssUiStyle::Darken(Medallion, 0.5f) : Medallion, bHovered ? Ctx->Style().Colors().GoldBright : Ctx->Style().Colors().Gold,
		1.5f);
	if (!bEmpty)
	{
		if (const FSlateBrush* Icon = Ctx->ItemIcon(View.baseId))
		{
			P.Brush(Center - FVector2D(R * 0.8f, R * 0.8f), FVector2D(R * 1.6f, R * 1.6f), Icon);
		}
		else
		{
			P.Circle(Center + FVector2D(0.0, 2.0), R * 0.45f, Slot == 0 ? FAbyssUiStyle::Rgb(0xe0283c) : FAbyssUiStyle::Rgb(0x2f6cf0));
			P.Box(Center + FVector2D(-2.0, -R * 0.62), FVector2D(4.0, R * 0.4), FAbyssUiStyle::Rgb(0xbfb4a2));
		}
		const FString Count = FAbyssUiContext::Int(View.count);
		P.TextCentered(Center + FVector2D(R * 0.7, R * 0.72), Count, Ctx->Style().Body(10.f, true, 2), FLinearColor::White);
	}
	const FString Hint = Ctx->KeyHint(Slot == 0 ? EAbyssInputAction::PotionHp : EAbyssInputAction::PotionMp);
	if (!Hint.IsEmpty())
	{
		P.TextCentered(Center + FVector2D(-R * 0.75, -R * 0.75), Hint, Ctx->Style().Body(9.f, true, 2), FAbyssUiStyle::Rgb(0xf0dcae));
	}
}

void SAbyssHudBottom::PaintPrompt(FAbyssPainter& P) const
{
	const FAbyssHudState& S = *State;
	if (Layout.bTouch || S.PromptKind == abyss::InteractKind::None || S.bDying)
	{
		return;
	}
	// world 7.4: "<key> Talk / Pick up / Open" over the skill bar (touch shows its own Talk / Use button).
	FString Label;
	switch (S.PromptKind)
	{
	case abyss::InteractKind::Npc: Label = Ctx->LocOrStr("sys.mobile.interact.talk", TEXT("Talk")); break;
	case abyss::InteractKind::Loot: Label = Ctx->LocOrStr("sys.mobile.interact.pickup", TEXT("Pick up")); break;
	case abyss::InteractKind::HiddenReward:
	case abyss::InteractKind::EventPuzzle: Label = Ctx->LocOrStr("sys.mobile.interact.open", TEXT("Open")); break;
	default: Label = Ctx->LocOrStr("sys.mobile.interact.use", TEXT("Use")); break;
	}
	const FString Key = Ctx->KeyHint(EAbyssInputAction::Interact);
	const FSlateFontInfo Font = Ctx->Style().Body(12.f, true, 2);
	const FVector2D LabelSize = FAbyssPainter::Measure(Label, Font);
	const double KeyW = Key.IsEmpty() ? 0.0 : FMath::Max(20.0, FAbyssPainter::Measure(Key, Font).X + 10.0);
	const double TotalW = LabelSize.X + KeyW + (KeyW > 0.0 ? 6.0 : 0.0) + 20.0;
	const FVector2D Pos = Layout.PromptCenter - FVector2D(TotalW * 0.5, 13.0);
	P.RoundBox(Pos, FVector2D(TotalW, 26.0), FLinearColor(0.04f, 0.035f, 0.05f, 0.85f), 6.f, FAbyssUiStyle::WithAlpha(Ctx->Style().Colors().Gold, 0.8f), 1.f);
	double X = Pos.X + 10.0;
	if (KeyW > 0.0)
	{
		P.RoundBox(FVector2D(X, Pos.Y + 4.0), FVector2D(KeyW, 18.0), FAbyssUiStyle::Rgb(0x2c2730), 3.f, FAbyssUiStyle::Rgb(0x8a7a64), 1.f);
		P.TextCentered(FVector2D(X + KeyW * 0.5, Pos.Y + 13.0), Key, Font, Ctx->Style().Colors().GoldBright);
		X += KeyW + 6.0;
	}
	P.Text(FVector2D(X, Pos.Y + 13.0 - LabelSize.Y * 0.5), Label, Font, Ctx->Style().Colors().Parchment);
}

void SAbyssHudBottom::PaintPortal(FAbyssPainter& P) const
{
	const FAbyssHudState& S = *State;
	if (!S.bPortaling)
	{
		return;
	}
	// W3: the 1.5 s channel as a bar in the portal colour.
	const FVector2D Size(220.0, 8.0);
	const FVector2D Pos = Layout.PortalCenter - FVector2D(Size.X * 0.5, 0.0);
	const FString Text = Ctx->LocOrStr("zone.teleport.opening", TEXT("Opening a portal..."));
	P.TextCentered(Pos + FVector2D(Size.X * 0.5, -10.0), Text, Ctx->Style().Body(Px(12.f), true, 2), FAbyssUiStyle::Rgb(0x9fc4ff));
	P.Bar(Pos, Size, static_cast<float>(S.PortalProgress), FAbyssUiStyle::Rgb(0x4488ff));
}
