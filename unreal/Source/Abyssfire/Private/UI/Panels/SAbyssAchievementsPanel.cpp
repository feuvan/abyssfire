#include "UI/Panels/SAbyssAchievementsPanel.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include <string>

#include "abyss/data/DataStore.h"
#include "abyss/quests/Achievements.h"

#include "Framework/AbyssText.h"
#include "UI/AbyssUiStyle.h"
#include "UI/Widgets/AbyssUiWidgets.h"

namespace
{
	FString AbyssAchievement_Text(const FAbyssUiContext& Ctx, const abyss::AchievementDef& Def, const char* Field, const std::string& Fallback)
	{
		return Ctx.NameOr("data.achievement." + Def.id + "." + Field, Fallback);
	}
}

void SAbyssAchievementsPanel::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	SAbyssPanelBase::Construct(SAbyssPanelBase::FArguments(), InContext);
	SetPanelContent(
		SNew(SAbyssPanelFrame, InContext)
		.Size(GetDesignSize())
		.Title(InContext->LocOr("ui.achievement.title", TEXT("Achievements")))
		.OnClose_Lambda([this]() { RequestClose(); })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SAssignNew(Header, SVerticalBox)
			]
			+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(0.f, 8.f, 0.f, 0.f))
			[
				SAssignNew(Scroll, SScrollBox)
				.ScrollBarStyle(&InContext->Style().ScrollBar())
				.ScrollBarThickness(FVector2D(6.0, 6.0))
				.ConsumeMouseWheel(EConsumeMouseWheel::Always)
			]
		]);
}

void SAbyssAchievementsPanel::Refresh(const abyss::Snapshot& Snap)
{
	const float Offset = Scroll->GetScrollOffset();
	Header->ClearChildren();
	Scroll->ClearChildren();
	const abyss::DataStore* Data = Ctx->GetData();
	if (Data == nullptr || Snap.achievements == nullptr)
	{
		return;
	}
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const abyss::AchievementState& State = *Snap.achievements;
	const std::vector<abyss::AchievementDef>& Defs = Data->Quests().achievements;

	int32 Unlocked = 0;
	FString CurrentTitle;
	for (const abyss::AchievementDef& Def : Defs)
	{
		if (State.IsUnlocked(Def.id))
		{
			++Unlocked;
			if (!Def.title.empty() || Ctx->HasKey("data.achievement." + Def.id + ".title"))
			{
				CurrentTitle = AbyssAchievement_Text(*Ctx, Def, "title", Def.title);
			}
		}
	}
	const float Fraction = Defs.empty() ? 0.f : static_cast<float>(Unlocked) / static_cast<float>(Defs.size());
	Header->AddSlot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SAbyssCanvas, Context, [Fraction](FAbyssPainter& P, const FVector2D& Size)
				{
					P.Bar(FVector2D::ZeroVector, Size, Fraction, P.Style.Colors().Gold, true);
				})
				.Size(FVector2D(200.0, 12.0))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.f, 0.f, 0.f, 0.f))
			[
				AbyssUi::Label(*Ctx, Ctx->LocArgsOr("ui.achievement.unlocked", TEXT("Unlocked: {count}/{total}"),
					{ FAbyssUiContext::Arg("count", Unlocked), FAbyssUiContext::Arg("total", static_cast<int64>(Defs.size())) }), 13.f, C.Heading, true, 1)
			]
		];
	if (!CurrentTitle.IsEmpty())
	{
		Header->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 4.f, 0.f, 0.f))
			[
				AbyssUi::Label(*Ctx, Ctx->LocArgsOr("ui.achievement.currentTitle", TEXT("Current Title: {title}"),
					{ FAbyssUiContext::Arg("title", CurrentTitle) }), 12.f, FAbyssUiStyle::Rgb(0xffd98a), true, 1)
			];
	}

	for (const abyss::AchievementDef& Def : Defs)
	{
		const bool bUnlocked = State.IsUnlocked(Def.id);
		const std::string Key = Def.targetId.empty() ? std::string(abyss::EnumName(Def.type))
			: std::string(abyss::EnumName(Def.type)) + ":" + Def.targetId;
		const int64 Progress = bUnlocked ? Def.required : FMath::Min<int64>(State.ProgressOf(Key), Def.required);
		const float RowFraction = Def.required > 0 ? static_cast<float>(Progress) / static_cast<float>(Def.required) : 1.f;
		TArray<FString> RewardParts;
		if (Def.hasReward)
		{
			RewardParts.Add(FString::Printf(TEXT("%s %s"), *Ctx->StatLabel(Def.rewardStat), *Ctx->StatValue(Def.rewardStat, Def.rewardValue)));
		}
		if (!Def.title.empty() || Ctx->HasKey("data.achievement." + Def.id + ".title"))
		{
			RewardParts.Add(Ctx->LocArgsOrStr("ui.achievement.titleReward", TEXT("Title: {title}"),
				{ FAbyssUiContext::Arg("title", AbyssAchievement_Text(*Ctx, Def, "title", Def.title)) }));
		}
		const FString Reward = FString::Join(RewardParts, TEXT("  "));
		const FString Count = FString::Printf(TEXT("%lld/%d"), static_cast<long long>(Progress), Def.required);

		Scroll->AddSlot()
			.Padding(FMargin(0.f, 0.f, 8.f, 6.f))
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SAbyssCanvas, Context, [bUnlocked](FAbyssPainter& P, const FVector2D& Size)
					{
						const FAbyssUiPalette& Palette = P.Style.Colors();
						P.Card(FVector2D::ZeroVector, Size, bUnlocked ? FAbyssUiStyle::Rgb(0x2a2114) : Palette.Card,
							bUnlocked ? FAbyssUiStyle::Rgb(0xffd98a) : FAbyssUiStyle::Rgb(0x3a343f), bUnlocked ? Palette.Gold : FLinearColor::Transparent);
						// medal: gold star when unlocked, grey outline otherwise
						const FVector2D Medal(24.0, Size.Y * 0.5);
						P.Circle(Medal, 15.f, bUnlocked ? Palette.GoldDark : FAbyssUiStyle::Rgb(0x1a171d), bUnlocked ? Palette.Gold : FAbyssUiStyle::Rgb(0x4a4350), 1.5f);
						P.Star(Medal, 9.f, 4.f, bUnlocked ? Palette.GoldBright : FAbyssUiStyle::Rgb(0x4a4350));
					})
				]
				+ SOverlay::Slot()
				.Padding(FMargin(48.f, 6.f, 10.f, 6.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							AbyssUi::Label(*Ctx, FText::AsCultureInvariant(AbyssAchievement_Text(*Ctx, Def, "name", Def.name)), 13.f,
								bUnlocked ? FAbyssUiStyle::Rgb(0xffd98a) : C.Text, true, 1)
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							AbyssUi::Label(*Ctx, FText::AsCultureInvariant(AbyssAchievement_Text(*Ctx, Def, "desc", Def.description)), 11.f,
								C.TextSoft, false, 1, 300.f)
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FMargin(8.f, 0.f, 0.f, 0.f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(SAbyssCanvas, Context, [RowFraction, bUnlocked](FAbyssPainter& P, const FVector2D& Size)
								{
									P.Bar(FVector2D::ZeroVector, Size, RowFraction, bUnlocked ? FAbyssUiStyle::Rgb(0x5cc04a) : FAbyssUiStyle::Rgb(0x3a7fd0));
								})
								.Size(FVector2D(96.0, 8.0))
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(6.f, 0.f, 0.f, 0.f))
							[
								AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Count), 11.f, C.TextSoft, true, 1)
							]
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(FMargin(0.f, 3.f, 0.f, 0.f))
						[
							SNew(SBox)
							.MaxDesiredWidth(150.f)
							[
								AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Reward), 10.f, C.Good, false, 1, 150.f)
							]
						]
					]
				]
			];
	}
	Scroll->SetScrollOffset(Offset);
}
