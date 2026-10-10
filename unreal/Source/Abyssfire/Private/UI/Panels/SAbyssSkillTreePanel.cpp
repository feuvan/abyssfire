#include "UI/Panels/SAbyssSkillTreePanel.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include <algorithm>
#include <string>
#include <vector>

#include "abyss/data/DataStore.h"
#include "abyss/hero/Skills.h"

#include "Framework/AbyssText.h"
#include "Input/AbyssInputTypes.h"
#include "UI/AbyssUiStyle.h"
#include "UI/Widgets/AbyssUiWidgets.h"

DECLARE_DELEGATE_TwoParams(FOnAbyssSkillCardPress, const FVector2D& /*AbsolutePosition*/, bool /*bTouch*/);

namespace
{
	/** The last tab of the skill tree (7.4: the active tab persists for the session). */
	int32 GAbyssSkillTreeLastTab = 0;

	enum class EAbyssSkillCardState : uint8
	{
		Locked,
		Investable,
		Learned,
		Maxed,
	};

	struct FAbyssSkillCardData
	{
		int32 SkillIndex = -1;
		std::string SkillId;
		FString Name;
		FString EnglishName;
		int32 Level = 0;
		int32 MaxLevel = 1;
		EAbyssSkillCardState State = EAbyssSkillCardState::Locked;
		bool bCanInvest = false;
		bool bPassive = false;
		bool bSynergy = false;
		bool bSelected = false;
		int32 BoundSlot = -1;
		FString StatsLine;
		bool bLockLine = false;
		FLinearColor TreeColor = FLinearColor::White;
		FLinearColor TypeColor = FLinearColor::White;
	};

	FString AbyssSkillTree_Fixed1(double Value)
	{
		return FAbyssUiContext::Fixed(Value, 1);
	}

	/** Buff stat label: ui.stat.<stat>, then data.buffStat.<stat>, else the id. */
	FString AbyssSkillTree_BuffStatLabel(const FAbyssUiContext& Ctx, abyss::BuffStat Stat)
	{
		const std::string Id(abyss::EnumName(Stat));
		if (Ctx.HasKey("ui.stat." + Id))
		{
			return Ctx.LocStr("ui.stat." + Id);
		}
		return Ctx.NameOr("data.buffStat." + Id, Id);
	}
}

// =====================================================================================================================
// SAbyssSkillCard: one 600 x 72 card (hover tooltip, click / tap, "+" button)
// =====================================================================================================================

class SAbyssSkillCard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssSkillCard) {}
		SLATE_EVENT(FOnAbyssSkillCardPress, OnPressed)
		SLATE_EVENT(FSimpleDelegate, OnLearn)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const FAbyssSkillCardData& InData)
	{
		Context = InContext;
		Data = InData;
		OnPressed = InArgs._OnPressed;
		const FSimpleDelegate OnLearn = InArgs._OnLearn;
		const FAbyssUiPalette& C = InContext->Style().Colors();
		const bool bTouch = InContext->IsTouch();

		TSharedRef<SVerticalBox> Text = SNew(SVerticalBox);
		TSharedRef<SHorizontalBox> NameRow = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Bottom)
			[
				AbyssUi::Label(*InContext, FText::AsCultureInvariant(Data.Name), 14.f,
					Data.State == EAbyssSkillCardState::Locked ? C.Dim : C.Parchment, true, 1)
			];
		if (!Data.EnglishName.IsEmpty() && Data.EnglishName != Data.Name)
		{
			NameRow->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Bottom)
				.Padding(FMargin(6.f, 0.f, 0.f, 1.f))
				[
					AbyssUi::Label(*InContext, FText::AsCultureInvariant(Data.EnglishName), 10.f, C.Muted, false, 1)
				];
		}
		if (Data.bPassive)
		{
			NameRow->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Bottom)
				.Padding(FMargin(6.f, 0.f, 0.f, 1.f))
				[
					AbyssUi::Label(*InContext, InContext->LocOr("ui.skillTree.passive", TEXT("Passive")), 10.f, C.Info, true, 1)
				];
		}
		if (Data.bSynergy)
		{
			NameRow->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Bottom)
				.Padding(FMargin(6.f, 0.f, 0.f, 1.f))
				[
					AbyssUi::Label(*InContext, InContext->LocOr("ui.skillTree.synergy", TEXT("Synergy")), 10.f, C.GoldBright, true, 1)
				];
		}
		if (Data.BoundSlot >= 0)
		{
			NameRow->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Bottom)
				.Padding(FMargin(6.f, 0.f, 0.f, 1.f))
				[
					AbyssUi::Label(*InContext, FText::AsCultureInvariant(FString::Printf(TEXT("[%d]"), Data.BoundSlot + 1)), 10.f,
						FAbyssUiStyle::Rgb(0x8be9fd), true, 1)
				];
		}
		Text->AddSlot().AutoHeight()[NameRow];
		Text->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 18.f, 0.f, 0.f))
			[
				AbyssUi::Label(*InContext, FText::AsCultureInvariant(Data.StatsLine), 11.f,
					Data.bLockLine ? FAbyssUiStyle::Rgb(0xc07a6a) : C.TextSoft, false, 1)
			];

		TSharedRef<SOverlay> Overlay = SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SAbyssCanvas, InContext, [this](FAbyssPainter& P, const FVector2D& Size) { PaintCard(P, Size); })
			]
			+ SOverlay::Slot()
			.Padding(FMargin(68.f, 8.f, 40.f, 6.f))
			[
				Text
			];
		if (Data.bCanInvest)
		{
			Overlay->AddSlot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.Padding(FMargin(0.f, 0.f, 10.f, 0.f))
				[
					SNew(SAbyssButton, InContext)
					.Text(FText::AsCultureInvariant(TEXT("+")))
					.Kind(EAbyssButtonKind::Success)
					.FontPx(16.f)
					.Width(bTouch ? 40.f : 26.f)
					.Height(bTouch ? 40.f : 26.f)
					.IsEnabled(!InContext->IsHeroDying())
					.OnClicked(OnLearn)
				];
		}
		ChildSlot
		[
			SNew(SBox)
			.WidthOverride(600.f)
			.HeightOverride(72.f)
			[
				Overlay
			]
		];
	}

	virtual ~SAbyssSkillCard() override
	{
		if (Context.IsValid())
		{
			Context->HideTooltip(this);
		}
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.IsTouchEvent())
		{
			bTouchPress = true;
			TouchStart = FVector2D(MouseEvent.GetScreenSpacePosition());
			return FReply::Handled();
		}
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			Context->HideTooltip(this);
			OnPressed.ExecuteIfBound(FVector2D(MouseEvent.GetScreenSpacePosition()), false);
		}
		return FReply::Handled();
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (bTouchPress && MouseEvent.IsTouchEvent())
		{
			bTouchPress = false;
			const FVector2D End(MouseEvent.GetScreenSpacePosition());
			if (FVector2D::Distance(End, TouchStart) <= FSlateApplication::Get().GetDragTriggerDistance() * 1.5f && MyGeometry.IsUnderLocation(End))
			{
				OnPressed.ExecuteIfBound(End, true);
			}
		}
		return FReply::Handled();
	}

	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
		if (MouseEvent.IsTouchEvent() || Context->IsTouch())
		{
			return;
		}
		if (const abyss::Snapshot* Snap = Context->GetSnapshot())
		{
			Context->ShowTooltip(SAbyssSkillTreePanel::MakeSkillTooltip(Context.ToSharedRef(), *Snap, Data.SkillIndex),
				FVector2D(MouseEvent.GetScreenSpacePosition()), this);
		}
	}

	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
	{
		SCompoundWidget::OnMouseLeave(MouseEvent);
		bTouchPress = false;
		Context->HideTooltip(this);
	}

	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override
	{
		return FCursorReply::Cursor(EMouseCursor::Hand);
	}

private:
	void PaintCard(FAbyssPainter& P, const FVector2D& Size) const
	{
		const FAbyssUiPalette& C = P.Style.Colors();
		const bool bHovered = IsHovered();
		FLinearColor Fill = FAbyssUiStyle::Rgb(0x121015);
		FLinearColor Border = FAbyssUiStyle::Rgb(0x2e2a32);
		FLinearColor Strip = FLinearColor::Transparent;
		FLinearColor Glow = FLinearColor::Transparent;
		float IconAlpha = 0.35f;
		switch (Data.State)
		{
		case EAbyssSkillCardState::Maxed:
			Fill = FAbyssUiStyle::Rgb(0x2a2114);
			Border = FAbyssUiStyle::Rgb(0xffd98a);
			Glow = FAbyssUiStyle::Rgb(0xffc860);
			Strip = C.Gold;
			IconAlpha = 1.f;
			break;
		case EAbyssSkillCardState::Learned:
			Fill = FAbyssUiStyle::Rgb(0x1f1b24);
			Border = Data.TreeColor;
			Strip = Data.TreeColor;
			IconAlpha = 1.f;
			break;
		case EAbyssSkillCardState::Investable:
			Fill = FAbyssUiStyle::Rgb(0x18201a);
			Border = FAbyssUiStyle::Rgb(0x6fd35a);
			Glow = FAbyssUiStyle::Rgb(0x6fd35a);
			IconAlpha = 0.75f;
			break;
		case EAbyssSkillCardState::Locked:
			break;
		}
		if (bHovered)
		{
			Fill = FAbyssUiStyle::Lighten(Fill, 0.06f);
		}
		P.Card(FVector2D::ZeroVector, Size, Fill, Border, Strip, Glow);
		if (Data.bSelected)
		{
			P.RoundBox(FVector2D(-2.0, -2.0), Size + FVector2D(4.0, 4.0), FLinearColor::Transparent, 6.f, C.GoldBright, 2.f);
		}

		// 42 px icon in a well framed by the damage-type colour
		const FVector2D IconPos(12.0, (Size.Y - 46.0) * 0.5);
		P.Well(IconPos, FVector2D(46.0, 46.0), 5.f);
		P.RoundBox(IconPos, FVector2D(46.0, 46.0), FLinearColor::Transparent, 5.f, FAbyssUiStyle::WithAlpha(Data.TypeColor, IconAlpha), 1.5f);
		const FLinearColor IconTint = Data.State == EAbyssSkillCardState::Locked ? FLinearColor(0.55f, 0.55f, 0.55f, IconAlpha)
			: FLinearColor(1.f, 1.f, 1.f, IconAlpha);
		if (const FSlateBrush* Icon = Context->SkillIcon(Data.SkillId))
		{
			P.Brush(IconPos + FVector2D(2.0, 2.0), FVector2D(42.0, 42.0), Icon, IconTint);
		}
		else
		{
			P.TextCentered(IconPos + FVector2D(23.0, 23.0), Data.Name.Left(1), P.Style.Body(18.f, true, 2),
				FAbyssUiStyle::WithAlpha(Data.TypeColor, IconAlpha));
		}

		// level pips (8 px diamonds, <= 20, gap 10, clipped 80 px from the right) + "lv/max"
		const double PipX0 = 68.0;
		const double PipY = 33.0;
		const double PipLimit = Size.X - 80.0 - (Data.bCanInvest ? 30.0 : 0.0);
		const int32 Pips = FMath::Min(Data.MaxLevel, 20);
		double PipEnd = PipX0;
		for (int32 Pip = 0; Pip < Pips; ++Pip)
		{
			const double X = PipX0 + Pip * 10.0;
			if (X > PipLimit - 40.0)
			{
				break;
			}
			const bool bOn = Pip < Data.Level;
			P.Diamond(FVector2D(X + 4.0, PipY), 7.f, bOn ? (Data.State == EAbyssSkillCardState::Maxed ? C.GoldBright : Data.TreeColor)
				: FAbyssUiStyle::Rgb(0x2e2a32));
			PipEnd = X + 10.0;
		}
		P.Text(FVector2D(PipEnd + 6.0, PipY - 8.0), FString::Printf(TEXT("%d/%d"), Data.Level, Data.MaxLevel), P.Style.Body(11.f, true, 1),
			Data.Level > 0 ? C.Parchment : C.Dim);
	}

	TSharedPtr<FAbyssUiContext> Context;
	FAbyssSkillCardData Data;
	FOnAbyssSkillCardPress OnPressed;
	bool bTouchPress = false;
	FVector2D TouchStart = FVector2D::ZeroVector;
};

// =====================================================================================================================
// Panel
// =====================================================================================================================

void SAbyssSkillTreePanel::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	SAbyssPanelBase::Construct(SAbyssPanelBase::FArguments(), InContext);
	ActiveTab = GAbyssSkillTreeLastTab;
	SetPanelContent(
		SNew(SAbyssPanelFrame, InContext)
		.Size(GetDesignSize())
		.Title(InContext->LocOr("ui.skillTree.title", TEXT("Skill Tree")))
		.ContentPadding(FMargin(0.f))
		.OnClose_Lambda([this]() { RequestClose(); })
		[
			SAssignNew(Body, SCanvas)
		]);
}

bool SAbyssSkillTreePanel::HandleBack()
{
	if (SelectedSkill >= 0)
	{
		SelectedSkill = -1;  // Esc first drops the picked skill
		MarkDirty();
		return true;
	}
	return false;
}

void SAbyssSkillTreePanel::Refresh(const abyss::Snapshot& Snap)
{
	if (Scroll.IsValid() && ScrollTab == ActiveTab)
	{
		SavedScroll = Scroll->GetScrollOffset();
	}
	Body->ClearChildren();
	Scroll.Reset();
	const abyss::DataStore* Data = Ctx->GetData();
	if (Data == nullptr || Snap.skills == nullptr)
	{
		return;
	}
	BuildHeader(Snap);
	BuildCards(Snap);
	BuildHotbar(Snap);
}

void SAbyssSkillTreePanel::BuildHeader(const abyss::Snapshot& Snap)
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const abyss::DataStore& Data = *Ctx->GetData();
	const abyss::SkillBook& Book = *Snap.skills;
	const int32 Points = Snap.hero.freeSkillPoints;

	Body->AddSlot().Position(FVector2D(0.0, 40.0)).Size(FVector2D(660.0, 20.0))
	[
		SNew(SBox)
		.HAlign(HAlign_Center)
		[
			AbyssUi::Label(*Ctx, Ctx->LocArgsOr("ui.skillTree.skillPoints", TEXT("{className} \x00B7 Skill Points: {points}"),
				{ FAbyssUiContext::Arg("className", Ctx->ClassName(Snap.hero.cls)), FAbyssUiContext::Arg("points", Points) }), 13.f,
				Points > 0 ? FAbyssUiStyle::Rgb(0xffd98a) : Ctx->Style().Colors().TextSoft, Points > 0, 1)
		]
	];

	// tabs: the class's trees in tab order, learned-skill count badges, tree colours
	std::vector<const abyss::SkillTreeDef*> Trees;
	for (const abyss::SkillTreeDef& Tree : Data.Classes().trees)
	{
		if (Tree.classId == Snap.hero.cls)
		{
			Trees.push_back(&Tree);
		}
	}
	std::stable_sort(Trees.begin(), Trees.end(), [](const abyss::SkillTreeDef* A, const abyss::SkillTreeDef* B) { return A->tabOrder < B->tabOrder; });
	TreeIds.Reset();
	TArray<SAbyssTabBar::FTab> Tabs;
	for (const abyss::SkillTreeDef* Tree : Trees)
	{
		int32 Learned = 0;
		for (size_t Index = 0; Index < Book.SkillCount(); ++Index)
		{
			const abyss::SkillDef& Skill = Book.Skill(static_cast<int32>(Index));
			if (Skill.tree == Tree->id && Book.Level(static_cast<int32>(Index)) > 0)
			{
				++Learned;
			}
		}
		SAbyssTabBar::FTab Tab;
		Tab.Label = FText::AsCultureInvariant(Ctx->NameOr(Tree->nameKey.empty() ? "data.skillTree." + Tree->id : Tree->nameKey, Tree->id));
		Tab.Accent = FAbyssUiStyle::Rgb(Tree->color);
		Tab.Badge = Learned > 0 ? FText::AsCultureInvariant(FString::FromInt(Learned)) : FText::GetEmpty();
		Tabs.Add(Tab);
		TreeIds.Add(Tree->id);
	}
	ActiveTab = FMath::Clamp(ActiveTab, 0, FMath::Max(0, TreeIds.Num() - 1));
	Body->AddSlot().Position(FVector2D(16.0, 64.0)).Size(FVector2D(628.0, 30.0))
	[
		SNew(SAbyssTabBar, Context)
		.Tabs(Tabs)
		.ActiveIndex_Lambda([this]() { return ActiveTab; })
		.OnTabSelected_Lambda([this](int32 Index)
		{
			if (Index != ActiveTab)
			{
				ActiveTab = Index;
				GAbyssSkillTreeLastTab = Index;
				SavedScroll = 0.f;
				MarkDirty();
			}
		})
	];
}

void SAbyssSkillTreePanel::BuildCards(const abyss::Snapshot& Snap)
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const abyss::DataStore& Data = *Ctx->GetData();
	const abyss::SkillBook& Book = *Snap.skills;
	const abyss::SkillRules& Rules = Data.Classes().skillRules;
	const abyss::EquipStats Eq = FAbyssUiContext::ApproxEquipStats(Snap);
	const double Cdr = Eq.Get(abyss::Stat::CooldownReduction);
	if (!TreeIds.IsValidIndex(ActiveTab))
	{
		return;
	}
	const std::string& TreeId = TreeIds[ActiveTab];
	const abyss::SkillTreeDef* TreeDef = Data.Classes().FindTree(TreeId);
	const FLinearColor TreeColor = TreeDef != nullptr ? FAbyssUiStyle::Rgb(TreeDef->color) : Ctx->Style().Colors().Gold;
	const bool bEnglish = Data.Strings().Current() == abyss::LocaleId::En;

	// skills of the tree sorted by tier (definition order within a tier)
	std::vector<int32> Indices;
	for (size_t Index = 0; Index < Book.SkillCount(); ++Index)
	{
		if (Book.Skill(static_cast<int32>(Index)).tree == TreeId)
		{
			Indices.push_back(static_cast<int32>(Index));
		}
	}
	std::stable_sort(Indices.begin(), Indices.end(), [&Book](int32 A, int32 B) { return Book.Skill(A).tier < Book.Skill(B).tier; });

	SAssignNew(Scroll, SScrollBox)
		.ScrollBarStyle(&Ctx->Style().ScrollBar())
		.ScrollBarThickness(FVector2D(6.0, 6.0))
		.ConsumeMouseWheel(EConsumeMouseWheel::Always)
		.AllowOverscroll(EAllowOverscroll::No);
	for (size_t Position = 0; Position < Indices.size(); ++Position)
	{
		const int32 SkillIndex = Indices[Position];
		const abyss::SkillDef& Skill = Book.Skill(SkillIndex);
		const int32 Level = Book.Level(SkillIndex);
		const abyss::InvestState Invest = Book.GetInvestState(SkillIndex, Snap.hero.level, Snap.hero.freeSkillPoints);

		FAbyssSkillCardData Card;
		Card.SkillIndex = SkillIndex;
		Card.SkillId = Skill.id;
		Card.Name = Ctx->SkillName(Skill);
		Card.EnglishName = bEnglish ? FString() : AbyssText::ToFString(Skill.nameEn);
		Card.Level = Level;
		Card.MaxLevel = FMath::Max(1, Skill.maxLevel);
		Card.bCanInvest = Invest.canInvest;
		Card.bPassive = Skill.passive;
		Card.TreeColor = TreeColor;
		Card.TypeColor = FAbyssUiStyle::Rgb(Data.Classes().damageTypeColors[static_cast<size_t>(Skill.damageType)]);
		Card.BoundSlot = Book.HotbarSlotOf(SkillIndex);
		Card.bSelected = SkillIndex == SelectedSkill;
		if (Level >= Card.MaxLevel)
		{
			Card.State = EAbyssSkillCardState::Maxed;
		}
		else if (Level > 0)
		{
			Card.State = EAbyssSkillCardState::Learned;
		}
		else if (Invest.canInvest)
		{
			Card.State = EAbyssSkillCardState::Investable;
		}
		for (const abyss::SkillSynergy& Synergy : Skill.synergies)
		{
			if (Level > 0 && Book.Level(Synergy.skillId) > 0)
			{
				Card.bSynergy = true;
			}
		}
		// numbers row: locked -> the lock reason, else "{mult}%  MP{mana}  CD{s}s  {type}" at max(1, level)
		if (Level == 0 && !Invest.canInvest)
		{
			Card.bLockLine = true;
			switch (Invest.reason)
			{
			case abyss::InvestBlock::PlayerLevel:
				Card.StatsLine = Ctx->LocArgsOrStr("ui.skillTree.lock.playerLevel", TEXT("Requires player level {level}"),
					{ FAbyssUiContext::Arg("level", Invest.requiredPlayerLevel) });
				break;
			case abyss::InvestBlock::TreePoints:
				Card.StatsLine = Ctx->LocArgsOrStr("ui.skillTree.lock.treePoints", TEXT("Tree points {current}/{required}"),
					{ FAbyssUiContext::Arg("current", Invest.investedTreePoints), FAbyssUiContext::Arg("required", Invest.requiredTreePoints) });
				break;
			case abyss::InvestBlock::PreviousTier:
				Card.StatsLine = Ctx->LocOrStr("ui.skillTree.lock.previousTier", TEXT("Learn a previous-tier skill first"));
				break;
			default:
				Card.StatsLine = Ctx->LocOrStr("ui.skillTree.lock.noPoints", TEXT("No skill points available"));
				break;
			}
		}
		else
		{
			const int32 At = FMath::Max(1, Level);
			Card.StatsLine = FString::Printf(TEXT("%d%%  MP%d  CD%ss  %s"),
				FMath::RoundToInt(abyss::SkillDamageMultiplier(Rules, Skill, At) * 100.0),
				abyss::SkillManaCost(Rules, Skill, At),
				*AbyssSkillTree_Fixed1(abyss::SkillCooldownMs(Rules, Skill, At, Cdr) / 1000.0),
				*Ctx->DamageTypeName(Skill.damageType));
		}

		// arrow from the previous card (learned: tree colour 0.6 + glow; else 0x3a3a4e 0.15)
		if (Position > 0)
		{
			const bool bLit = Level > 0;
			Scroll->AddSlot()
			[
				SNew(SAbyssCanvas, Context, [bLit, TreeColor](FAbyssPainter& P, const FVector2D& Size)
				{
					const FLinearColor Color = bLit ? FAbyssUiStyle::WithAlpha(TreeColor, 0.6f) : FAbyssUiStyle::Rgb(0x3a3a4e, 0.15f);
					const double X = 35.0;
					if (bLit)
					{
						P.Line(FVector2D(X, 0.0), FVector2D(X, Size.Y - 3.0), FAbyssUiStyle::WithAlpha(TreeColor, 0.2f), 6.f);
					}
					P.Line(FVector2D(X, 0.0), FVector2D(X, Size.Y - 3.0), Color, 2.f);
					TArray<FVector2D> Head;
					Head.Add(FVector2D(X, Size.Y));
					Head.Add(FVector2D(X - 5.0, Size.Y - 6.0));
					Head.Add(FVector2D(X + 5.0, Size.Y - 6.0));
					P.ConvexPolygon(Head, Color);
				})
				.Size(FVector2D(600.0, 12.0))
			];
		}
		const TWeakPtr<SAbyssSkillTreePanel> WeakPanel = StaticCastSharedRef<SAbyssSkillTreePanel>(AsShared());
		Scroll->AddSlot()
		[
			SNew(SAbyssSkillCard, Context, Card)
			.OnPressed_Lambda([WeakPanel, SkillIndex](const FVector2D& Position2D, bool bTouch)
			{
				if (const TSharedPtr<SAbyssSkillTreePanel> Panel = WeakPanel.Pin())
				{
					Panel->OnCardClicked(SkillIndex, Position2D, bTouch);
				}
			})
			.OnLearn_Lambda([WeakPanel, SkillIndex]()
			{
				if (const TSharedPtr<SAbyssSkillTreePanel> Panel = WeakPanel.Pin())
				{
					Panel->OnLearnClicked(SkillIndex);
				}
			})
		];
	}
	Body->AddSlot().Position(FVector2D(30.0, 100.0)).Size(FVector2D(614.0, 368.0))
	[
		SNew(SBox)
		.Clipping(EWidgetClipping::ClipToBounds)
		[
			Scroll.ToSharedRef()
		]
	];
	if (ScrollTab == ActiveTab)
	{
		Scroll->SetScrollOffset(SavedScroll);
	}
	ScrollTab = ActiveTab;
}

void SAbyssSkillTreePanel::BuildHotbar(const abyss::Snapshot& Snap)
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const abyss::SkillBook& Book = *Snap.skills;
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const bool bTouch = Ctx->IsTouch();
	Body->AddSlot().Position(FVector2D(16.0, 476.0)).Size(FVector2D(628.0, 18.0))
	[
		AbyssUi::SectionHeader(Context, Ctx->LocOr("ui.skillTree.hotbar", TEXT("Skill bar")), 628.f)
	];
	const int32 Capacity = Book.HotbarCapacity();
	constexpr double SlotSize = 50.0;
	constexpr double Gap = 12.0;
	const double Left = (660.0 - (6.0 * SlotSize + 5.0 * Gap)) * 0.5;
	const TWeakPtr<SAbyssSkillTreePanel> WeakPanel = StaticCastSharedRef<SAbyssSkillTreePanel>(AsShared());
	for (int32 Slot = 0; Slot < abyss::SkillBook::kHotbarSlots; ++Slot)
	{
		const int32 Bound = Book.HotbarSkill(Slot);
		const bool bLocked = Slot >= Capacity;
		const std::string SkillId = Bound >= 0 ? Book.Skill(Bound).id : std::string();
		const FString Name = Bound >= 0 ? Ctx->SkillName(SkillId) : FString();
		const FString Key = Ctx->KeyHint(static_cast<EAbyssInputAction>(static_cast<int32>(EAbyssInputAction::Skill1) + Slot));
		const bool bTarget = SelectedSkill >= 0 && !bLocked;
		const bool bHeld = Bound >= 0 && Bound == SelectedSkill;
		Body->AddSlot().Position(FVector2D(Left + Slot * (SlotSize + Gap), 500.0)).Size(FVector2D(SlotSize, SlotSize))
		[
			SNew(SAbyssHitArea, Context)
			.IsEnabled(!bLocked)
			.OnPressed_Lambda([WeakPanel, Slot, bTouch]()
			{
				if (const TSharedPtr<SAbyssSkillTreePanel> Panel = WeakPanel.Pin())
				{
					Panel->OnHotbarSlotClicked(Slot, FSlateApplication::Get().GetCursorPos(), bTouch);
				}
			})
			.OnRightPressed_Lambda([WeakPanel, Slot]()
			{
				if (const TSharedPtr<SAbyssSkillTreePanel> Panel = WeakPanel.Pin())
				{
					Panel->OnHotbarSlotCleared(Slot);
				}
			})
			[
				SNew(SAbyssCanvas, Context, [Context, SkillId, Name, Key, bLocked, bTarget, bHeld](FAbyssPainter& P, const FVector2D& Size)
				{
					const FAbyssUiPalette& Palette = P.Style.Colors();
					P.Well(FVector2D::ZeroVector, Size, 6.f);
					const FLinearColor Border = bHeld ? Palette.GoldBright : (bTarget ? FAbyssUiStyle::Rgb(0x6fd35a) : FAbyssUiStyle::Rgb(0x4a4350));
					P.RoundBox(FVector2D::ZeroVector, Size, FLinearColor::Transparent, 6.f, Border, bTarget || bHeld ? 2.f : 1.2f);
					if (bLocked)
					{
						P.Diamond(Size * 0.5, 14.f, FAbyssUiStyle::Rgb(0x2e2a32));
						return;
					}
					if (!SkillId.empty())
					{
						if (const FSlateBrush* Icon = Context->SkillIcon(SkillId))
						{
							P.Brush(FVector2D(4.0, 4.0), Size - FVector2D(8.0, 8.0), Icon);
						}
						else
						{
							P.TextCentered(Size * 0.5, Name.Left(2), P.Style.Body(14.f, true, 2), Palette.Parchment);
						}
					}
					if (!Key.IsEmpty())
					{
						P.Text(FVector2D(4.0, 2.0), Key, P.Style.Body(10.f, true, 2), Palette.GoldBright);
					}
				})
			]
		];
	}
	const FText Hint = bTouch ? Ctx->LocOr("ui.skillTree.hotbarHintTouch", TEXT("Tap a learned skill, then a slot. Tap a slot to move or clear it."))
		: Ctx->LocOr("ui.skillTree.hotbarHint", TEXT("Click a learned skill, then a slot. Right-click a slot to clear it."));
	Body->AddSlot().Position(FVector2D(16.0, 556.0)).Size(FVector2D(628.0, 34.0))
	[
		SNew(SBox)
		.HAlign(HAlign_Center)
		[
			AbyssUi::Label(*Ctx, SelectedSkill >= 0 ? Ctx->LocArgsOr("ui.skillTree.hotbarPick", TEXT("Pick a slot for {name}"),
				{ FAbyssUiContext::Arg("name", Ctx->SkillName(Book.Skill(SelectedSkill).id)) }) : Hint, 11.f,
				SelectedSkill >= 0 ? C.GoldBright : C.Muted, false, 1, 620.f)
		]
	];
}

// =====================================================================================================================
// Interactions
// =====================================================================================================================

void SAbyssSkillTreePanel::OnLearnClicked(int32 SkillIndex)
{
	if (Ctx->IsHeroDying())
	{
		return;
	}
	Ctx->Submit(abyss::CmdLearnSkill{ SkillIndex });
}

void SAbyssSkillTreePanel::OnCardClicked(int32 SkillIndex, const FVector2D& AbsolutePosition, bool bTouch)
{
	const abyss::Snapshot* Snap = Ctx->GetSnapshot();
	if (Snap == nullptr || Snap->skills == nullptr)
	{
		return;
	}
	const abyss::SkillBook& Book = *Snap->skills;
	const abyss::SkillDef& Skill = Book.Skill(SkillIndex);
	const bool bAssignable = Book.Level(SkillIndex) > 0 && !Skill.passive;
	if (bAssignable)
	{
		SelectedSkill = SelectedSkill == SkillIndex ? -1 : SkillIndex;
		MarkDirty();
	}
	if (!bTouch)
	{
		return;
	}
	// touch: the tooltip card with the actions (7.0.6: hover affordances become tap -> card + buttons)
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const abyss::InvestState Invest = Book.GetInvestState(SkillIndex, Snap->hero.level, Snap->hero.freeSkillPoints);
	TSharedRef<SVerticalBox> Column = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeSkillTooltip(Context, *Snap, SkillIndex)
		];
	if (Invest.canInvest)
	{
		const TWeakPtr<SAbyssSkillTreePanel> WeakPanel = StaticCastSharedRef<SAbyssSkillTreePanel>(AsShared());
		Column->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.f, 8.f, 0.f, 0.f))
			[
				SNew(SAbyssButton, Context)
				.Text(Ctx->LocOr("ui.skillTree.learn", TEXT("Learn")))
				.Kind(EAbyssButtonKind::Success)
				.FontPx(18.f)
				.Width(200.f)
				.Height(52.f)
				.IsEnabled(!Ctx->IsHeroDying())
				.OnClicked_Lambda([WeakPanel, Context, SkillIndex]()
				{
					Context->ClosePopup();
					if (const TSharedPtr<SAbyssSkillTreePanel> Panel = WeakPanel.Pin())
					{
						Panel->OnLearnClicked(SkillIndex);
					}
				})
			];
	}
	Ctx->ShowPopup(Column, AbsolutePosition);
}

void SAbyssSkillTreePanel::OnHotbarSlotClicked(int32 Slot, const FVector2D& AbsolutePosition, bool bTouch)
{
	const abyss::Snapshot* Snap = Ctx->GetSnapshot();
	if (Snap == nullptr || Snap->skills == nullptr || Ctx->IsHeroDying())
	{
		return;
	}
	const abyss::SkillBook& Book = *Snap->skills;
	const int32 Bound = Book.HotbarSkill(Slot);
	if (SelectedSkill >= 0)
	{
		// C3: bind (the core swaps when the skill already sits in another slot)
		Ctx->Submit(abyss::CmdSetHotbar{ Slot, SelectedSkill });
		SelectedSkill = -1;
		MarkDirty();
		return;
	}
	if (Bound < 0)
	{
		return;
	}
	if (!bTouch)
	{
		SelectedSkill = Bound;  // pick it up to move it
		MarkDirty();
		return;
	}
	// touch: move or clear
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const TWeakPtr<SAbyssSkillTreePanel> WeakPanel = StaticCastSharedRef<SAbyssSkillTreePanel>(AsShared());
	Ctx->ShowPopup(
		SNew(SAbyssCardFrame, Context)
		.Padding(FMargin(8.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 3.f))
			[
				SNew(SAbyssButton, Context)
				.Text(Ctx->LocOr("ui.skillTree.move", TEXT("Move")))
				.Kind(EAbyssButtonKind::Primary)
				.FontPx(18.f)
				.Width(190.f)
				.Height(52.f)
				.OnClicked_Lambda([WeakPanel, Context, Bound]()
				{
					Context->ClosePopup();
					if (const TSharedPtr<SAbyssSkillTreePanel> Panel = WeakPanel.Pin())
					{
						Panel->SelectedSkill = Bound;
						Panel->MarkDirty();
					}
				})
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 3.f))
			[
				SNew(SAbyssButton, Context)
				.Text(Ctx->LocOr("ui.skillTree.clear", TEXT("Clear")))
				.Kind(EAbyssButtonKind::Danger)
				.FontPx(18.f)
				.Width(190.f)
				.Height(52.f)
				.OnClicked_Lambda([WeakPanel, Context, Slot]()
				{
					Context->ClosePopup();
					if (const TSharedPtr<SAbyssSkillTreePanel> Panel = WeakPanel.Pin())
					{
						Panel->OnHotbarSlotCleared(Slot);
					}
				})
			]
		],
		AbsolutePosition);
}

void SAbyssSkillTreePanel::OnHotbarSlotCleared(int32 Slot)
{
	if (Ctx->IsHeroDying())
	{
		return;
	}
	Ctx->Submit(abyss::CmdSetHotbar{ Slot, -1 });
	SelectedSkill = -1;
	MarkDirty();
}

// =====================================================================================================================
// Tooltip (7.4)
// =====================================================================================================================

TSharedRef<SWidget> SAbyssSkillTreePanel::MakeSkillTooltip(const TSharedRef<FAbyssUiContext>& Ctx, const abyss::Snapshot& Snap, int32 SkillIndex)
{
	const abyss::DataStore* Data = Ctx->GetData();
	if (Data == nullptr || Snap.skills == nullptr || SkillIndex < 0 || static_cast<size_t>(SkillIndex) >= Snap.skills->SkillCount())
	{
		return SNullWidget::NullWidget;
	}
	const abyss::SkillBook& Book = *Snap.skills;
	const abyss::SkillDef& Skill = Book.Skill(SkillIndex);
	const abyss::SkillRules& Rules = Data->Classes().skillRules;
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const int32 Level = Book.Level(SkillIndex);
	const int32 At = FMath::Max(1, Level);
	const double Cdr = FAbyssUiContext::ApproxEquipStats(Snap).Get(abyss::Stat::CooldownReduction);
	const bool bEnglish = Data->Strings().Current() == abyss::LocaleId::En;
	const float Wrap = 256.f;
	const FLinearColor TypeColor = FAbyssUiStyle::Rgb(Data->Classes().damageTypeColors[static_cast<size_t>(Skill.damageType)]);

	TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox);
	const auto AddLine = [&Lines, &Ctx, Wrap](const FString& Text, float Px, const FLinearColor& Color, bool bBold = false, float Top = 1.f)
	{
		if (Text.IsEmpty())
		{
			return;
		}
		Lines->AddSlot().AutoHeight().Padding(FMargin(0.f, Top, 0.f, 0.f))
		[
			AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Text), Px, Color, bBold, 1, Wrap)
		];
	};

	// header: name (+ English name), level
	AddLine(Ctx->SkillName(Skill), 15.f, C.Parchment, true, 0.f);
	if (!bEnglish && !Skill.nameEn.empty())
	{
		AddLine(AbyssText::ToFString(Skill.nameEn), 10.f, C.Muted);
	}
	AddLine(FString::Printf(TEXT("Lv.%d / %d"), Level, Skill.maxLevel), 11.f, C.TextSoft);
	const std::string DescKey = "data.skill." + Skill.id + ".desc";
	AddLine(Ctx->HasKey(DescKey) ? Ctx->LocStr(DescKey) : (bEnglish ? FString() : AbyssText::ToFString(Skill.description)), 11.f, C.TextSoft, false, 4.f);

	const auto AddNumbers = [&](int32 L, bool bDelta)
	{
		const double Mult = abyss::SkillDamageMultiplier(Rules, Skill, L);
		if (Skill.damageMultiplier > 0.0)
		{
			FString Value = FAbyssUiContext::Int(FMath::RoundToInt(Mult * 100.0));
			if (bDelta)
			{
				const double Prev = abyss::SkillDamageMultiplier(Rules, Skill, At);
				Value += FString::Printf(TEXT(" (+%d%%)"), FMath::RoundToInt((Mult - Prev) * 100.0));
			}
			AddLine(Ctx->LocArgsOrStr("ui.skillTree.tooltip.damage", TEXT("Damage: {value}% {type}"),
				{ FAbyssUiContext::Arg("value", Value), FAbyssUiContext::Arg("type", Ctx->DamageTypeName(Skill.damageType)) }), 11.f, TypeColor);
		}
		const int32 Mana = abyss::SkillManaCost(Rules, Skill, L);
		if (Mana > 0 && (!bDelta || Mana != abyss::SkillManaCost(Rules, Skill, At)))
		{
			AddLine(Ctx->LocArgsOrStr("ui.skillTree.tooltip.cost", TEXT("Cost: {value} MP"), { FAbyssUiContext::Arg("value", Mana) }), 11.f,
				FAbyssUiStyle::Rgb(0x7fb6ff));
		}
		const double Cooldown = abyss::SkillCooldownMs(Rules, Skill, L, Cdr);
		if (Cooldown > 0.0 && (!bDelta || Cooldown != abyss::SkillCooldownMs(Rules, Skill, At, Cdr)))
		{
			AddLine(Ctx->LocArgsOrStr("ui.skillTree.tooltip.cooldown", TEXT("Cooldown: {value}s"),
				{ FAbyssUiContext::Arg("value", AbyssSkillTree_Fixed1(Cooldown / 1000.0)) }), 11.f, C.Text);
		}
	};
	if (!Skill.passive)
	{
		Lines->AddSlot().AutoHeight().Padding(FMargin(0.f, 5.f, 0.f, 3.f))[AbyssUi::Divider(Ctx, Wrap, false)];
		AddNumbers(At, false);
		if (Skill.range > 0.0)
		{
			AddLine(Ctx->LocArgsOrStr("ui.skillTree.tooltip.range", TEXT("Range: {value} tiles"),
				{ FAbyssUiContext::Arg("value", FAbyssUiContext::Num(Skill.range)) }), 11.f, C.Text);
		}
		if (Skill.aoe)
		{
			AddLine(Ctx->LocArgsOrStr("ui.skillTree.tooltip.aoeRadius", TEXT("AOE Radius: {value} tiles"),
				{ FAbyssUiContext::Arg("value", AbyssSkillTree_Fixed1(abyss::SkillAoeRadius(Rules, Skill, At))) }), 11.f, C.Text);
		}
		if (Skill.critBonus > 0.0)
		{
			AddLine(Ctx->LocArgsOrStr("ui.skillTree.tooltip.critBonus", TEXT("Crit Bonus: +{value}%"),
				{ FAbyssUiContext::Arg("value", FAbyssUiContext::Num(Skill.critBonus)) }), 11.f, C.Text);
		}
		if (Skill.hasStunDuration && Skill.stunDurationMs > 0.0)
		{
			AddLine(Ctx->LocArgsOrStr("ui.skillTree.tooltip.stun", TEXT("Stun: {value}s"),
				{ FAbyssUiContext::Arg("value", AbyssSkillTree_Fixed1(Skill.stunDurationMs / 1000.0)) }), 11.f, C.Text);
		}
		if (Skill.hasBuff)
		{
			AddLine(Ctx->LocArgsOrStr("ui.skillTree.tooltip.buff", TEXT("Buff: {stat} +{value}% ({duration}s)"),
				{ FAbyssUiContext::Arg("stat", AbyssSkillTree_BuffStatLabel(*Ctx, Skill.buff.stat)),
					FAbyssUiContext::Arg("value", FAbyssUiContext::Num(FMath::RoundToDouble(abyss::SkillBuffValue(Rules, Skill, At) * 1000.0) / 10.0)),
					FAbyssUiContext::Arg("duration", AbyssSkillTree_Fixed1(abyss::SkillBuffDurationMs(Rules, Skill, At) / 1000.0)) }),
				11.f, C.Good);
		}
	}
	// synergies: per synergy {name, per level %, current bonus %}
	if (!Skill.synergies.empty())
	{
		AddLine(Ctx->LocOrStr("ui.skillTree.tooltip.synergyHeader", TEXT("Synergy Bonuses")), 11.f, C.Heading, true, 5.f);
		for (const abyss::SkillSynergy& Synergy : Skill.synergies)
		{
			const int32 SourceLevel = Book.Level(Synergy.skillId);
			AddLine(Ctx->LocArgsOrStr("ui.skillTree.tooltip.synergyLine", TEXT("{name}: +{perLevel}%/lvl (current +{bonus}%)"),
				{ FAbyssUiContext::Arg("name", Ctx->SkillName(Synergy.skillId)),
					FAbyssUiContext::Arg("perLevel", FAbyssUiContext::Num(FMath::RoundToDouble(Synergy.damagePerLevel * 1000.0) / 10.0)),
					FAbyssUiContext::Arg("bonus", FAbyssUiContext::Num(FMath::RoundToDouble(Synergy.damagePerLevel * SourceLevel * 1000.0) / 10.0)) }),
				11.f, SourceLevel > 0 ? C.GoldBright : C.Dim);
		}
	}
	// next level block
	if (Level < Skill.maxLevel && !Skill.passive)
	{
		AddLine(Ctx->LocArgsOrStr("ui.skillTree.tooltip.nextLevel", TEXT("Next Level (Lv{level})"), { FAbyssUiContext::Arg("level", Level + 1) }),
			11.f, C.Heading, true, 5.f);
		AddNumbers(Level + 1, Level > 0);
	}
	return SNew(SBox)
		.WidthOverride(280.f)
		[
			SNew(SAbyssCardFrame, Ctx)
			.Accent(TypeColor)
			.Padding(FMargin(10.f, 9.f, 10.f, 10.f))
			[
				Lines
			]
		];
}
