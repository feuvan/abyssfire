#include "UI/Panels/SAbyssQuestLogPanel.h"

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
#include "abyss/quests/Lore.h"
#include "abyss/quests/QuestSystem.h"

#include "Framework/AbyssText.h"
#include "UI/AbyssUiStyle.h"
#include "UI/Widgets/AbyssUiWidgets.h"

namespace
{
	constexpr int32 GAbyssQuestLogPerPage = 13;
	constexpr double GAbyssQuestLogRowH = 28.0;
	constexpr uint32 GAbyssQuestLogTabAccent[3] = { 0x5a9fe0, 0x6fd35a, 0xd4a54a };

	struct FAbyssQuestLogRow
	{
		std::string Id;
		FString Label;
		FLinearColor Color;
		bool bDone = false;
		bool bMain = false;
		bool bTracked = false;
	};

	TSharedRef<SWidget> AbyssQuestLog_Row(const TSharedRef<FAbyssUiContext>& Ctx, const FAbyssQuestLogRow& Row, bool bSelected,
		TFunction<void()> OnClick)
	{
		const FString Label = Row.Label;
		const FLinearColor Color = Row.Color;
		const bool bDone = Row.bDone;
		const bool bTracked = Row.bTracked;
		return SNew(SAbyssHitArea, Ctx)
			.OnPressed_Lambda([OnClick]()
			{
				if (OnClick)
				{
					OnClick();
				}
			})
			[
				SNew(SAbyssCanvas, Ctx, [Ctx, Label, Color, bDone, bSelected, bTracked](FAbyssPainter& P, const FVector2D& Size)
				{
					const FAbyssUiPalette& C = P.Style.Colors();
					if (bSelected)
					{
						P.Card(FVector2D::ZeroVector, Size - FVector2D(0.0, 2.0), C.CardHover, C.Gold, C.Gold);
					}
					else
					{
						P.Card(FVector2D::ZeroVector, Size - FVector2D(0.0, 2.0), C.Card, FAbyssUiStyle::Rgb(0x2a2530));
					}
					const FSlateFontInfo Font = P.Style.Body(12.f, bSelected, 1);
					FString Shown = Label;
					if (bTracked)
					{
						Shown = TEXT("\x27A4 ") + Shown;
					}
					if (bDone)
					{
						Shown += TEXT(" \x2713");
					}
					P.Text(FVector2D(10.0, (Size.Y - 2.0 - FAbyssPainter::Measure(Shown, Font).Y) * 0.5), Shown, Font, Color);
				})
				.Size(FVector2D(250.0, GAbyssQuestLogRowH))
			];
	}
}

void SAbyssQuestLogPanel::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	SAbyssPanelBase::Construct(SAbyssPanelBase::FArguments(), InContext);
	SetPanelContent(
		SNew(SAbyssPanelFrame, InContext)
		.Size(GetDesignSize())
		.Title(InContext->LocOr("ui.questLog.title", TEXT("Quest Log")))
		.ContentPadding(FMargin(0.f))
		.OnClose_Lambda([this]() { RequestClose(); })
		[
			SAssignNew(Body, SCanvas)
		]);
}

void SAbyssQuestLogPanel::Refresh(const abyss::Snapshot& Snap)
{
	Body->ClearChildren();
	if (Ctx->GetData() == nullptr || Snap.quests == nullptr)
	{
		return;
	}
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	TArray<SAbyssTabBar::FTab> Tabs;
	const char* TabKeys[3] = { "ui.questLog.tab.active", "ui.questLog.tab.completed", "ui.questLog.tab.lore" };
	const TCHAR* TabFallbacks[3] = { TEXT("Active"), TEXT("Completed"), TEXT("Lore") };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		SAbyssTabBar::FTab TabInfo;
		TabInfo.Label = Ctx->LocOr(TabKeys[Index], TabFallbacks[Index]);
		TabInfo.Accent = FAbyssUiStyle::Rgb(GAbyssQuestLogTabAccent[Index]);
		Tabs.Add(TabInfo);
	}
	Body->AddSlot().Position(FVector2D(16.0, 46.0)).Size(FVector2D(440.0, 28.0))
	[
		SNew(SAbyssTabBar, Context)
		.Tabs(Tabs)
		.TabWidth(140.f)
		.TabHeight(28.f)
		.ActiveIndex_Lambda([this]() { return Tab; })
		.OnTabSelected_Lambda([this](int32 Index)
		{
			if (Index != Tab)
			{
				Tab = Index;
				Page = 0;
				MarkDirty();
			}
		})
	];
	// list | detail divider
	Body->AddSlot().Position(FVector2D(272.0, 84.0)).Size(FVector2D(2.0, 420.0))
	[
		SNew(SAbyssCanvas, Context, [](FAbyssPainter& P, const FVector2D& Size)
		{
			const FLinearColor Gold = P.Style.Colors().Gold;
			P.ColoredQuad(FVector2D::ZeroVector, FVector2D(1.0, Size.Y * 0.5), FAbyssUiStyle::WithAlpha(Gold, 0.f),
				FAbyssUiStyle::WithAlpha(Gold, 0.f), FAbyssUiStyle::WithAlpha(Gold, 0.5f), FAbyssUiStyle::WithAlpha(Gold, 0.5f));
			P.ColoredQuad(FVector2D(0.0, Size.Y * 0.5), FVector2D(1.0, Size.Y * 0.5), FAbyssUiStyle::WithAlpha(Gold, 0.5f),
				FAbyssUiStyle::WithAlpha(Gold, 0.5f), FAbyssUiStyle::WithAlpha(Gold, 0.f), FAbyssUiStyle::WithAlpha(Gold, 0.f));
		})
	];
	if (Tab == 2)
	{
		BuildLoreList(Snap);
	}
	else
	{
		BuildQuestList(Snap);
	}
}

void SAbyssQuestLogPanel::AddPager(int32 Total)
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const int32 Pages = FMath::Max(1, (Total + GAbyssQuestLogPerPage - 1) / GAbyssQuestLogPerPage);
	if (Pages <= 1)
	{
		return;
	}
	Body->AddSlot().Position(FVector2D(16.0, 456.0)).Size(FVector2D(250.0, 28.0))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SAbyssButton, Context)
			.Text(Ctx->LocOr("ui.questLog.prevPage", TEXT("Prev")))
			.Kind(EAbyssButtonKind::Ghost).FontPx(11.f).Width(78.f).Height(26.f)
			.IsEnabled(Page > 0)
			.OnClicked_Lambda([this]()
			{
				--Page;
				MarkDirty();
			})
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			AbyssUi::Label(*Ctx, FText::AsCultureInvariant(FString::Printf(TEXT("%d/%d"), Page + 1, Pages)), 11.f, Ctx->Style().Colors().TextSoft)
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SAbyssButton, Context)
			.Text(Ctx->LocOr("ui.questLog.nextPage", TEXT("Next")))
			.Kind(EAbyssButtonKind::Ghost).FontPx(11.f).Width(78.f).Height(26.f)
			.IsEnabled(Page + 1 < Pages)
			.OnClicked_Lambda([this]()
			{
				++Page;
				MarkDirty();
			})
		]
	];
}

// =====================================================================================================================
// Quests
// =====================================================================================================================

void SAbyssQuestLogPanel::BuildQuestList(const abyss::Snapshot& Snap)
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const abyss::DataStore& Data = *Ctx->GetData();
	const abyss::QuestSystem& Quests = *Snap.quests;
	const FAbyssUiPalette& C = Ctx->Style().Colors();

	// active tab: active + completed (not failed); completed tab: turned in
	std::vector<const abyss::QuestDef*> Defs;
	for (const abyss::QuestProgress& Progress : Quests.AllProgress())
	{
		const bool bWanted = Tab == 0 ? (Progress.status == abyss::QuestStatus::Active || Progress.status == abyss::QuestStatus::Completed)
			: Progress.status == abyss::QuestStatus::TurnedIn;
		if (!bWanted)
		{
			continue;
		}
		if (const abyss::QuestDef* Def = Data.FindQuest(Progress.questId))
		{
			Defs.push_back(Def);
		}
	}
	std::stable_sort(Defs.begin(), Defs.end(), [](const abyss::QuestDef* A, const abyss::QuestDef* B)
	{
		const bool bMainA = A->category == abyss::QuestCategory::Main;
		const bool bMainB = B->category == abyss::QuestCategory::Main;
		if (bMainA != bMainB)
		{
			return bMainA;
		}
		return A->level < B->level;
	});

	std::string& Current = Selected[Tab];
	bool bFound = false;
	for (const abyss::QuestDef* Def : Defs)
	{
		bFound = bFound || Def->id == Current;
	}
	if (!bFound)
	{
		Current = Defs.empty() ? std::string() : Defs.front()->id;
	}

	const int32 Total = static_cast<int32>(Defs.size());
	const int32 Pages = FMath::Max(1, (Total + GAbyssQuestLogPerPage - 1) / GAbyssQuestLogPerPage);
	Page = FMath::Clamp(Page, 0, Pages - 1);
	if (Defs.empty())
	{
		Body->AddSlot().Position(FVector2D(16.0, 92.0)).Size(FVector2D(250.0, 40.0))
		[
			AbyssUi::Label(*Ctx, Tab == 0 ? Ctx->LocOr("ui.questLog.noActive", TEXT("No active quests"))
				: Ctx->LocOr("ui.questLog.noCompleted", TEXT("No completed quests")), 12.f, C.Dim, false, 1, 248.f)
		];
	}
	const std::string Tracked = Quests.Tracked();
	for (int32 Row = 0; Row < GAbyssQuestLogPerPage; ++Row)
	{
		const int32 Index = Page * GAbyssQuestLogPerPage + Row;
		if (Index >= Total)
		{
			break;
		}
		const abyss::QuestDef& Def = *Defs[static_cast<size_t>(Index)];
		bool bHasRecord = false;
		const abyss::QuestStatus Status = Quests.StatusOf(Def.id, bHasRecord);
		FAbyssQuestLogRow Entry;
		Entry.Id = Def.id;
		Entry.bMain = Def.category == abyss::QuestCategory::Main;
		Entry.bDone = Status == abyss::QuestStatus::Completed;
		Entry.bTracked = Def.id == Tracked;
		Entry.Label = FString::Printf(TEXT("%s %s"),
			*(Entry.bMain ? Ctx->LocOrStr("ui.questLog.mainTag", TEXT("[M]")) : Ctx->LocOrStr("ui.questLog.sideTag", TEXT("[S]"))),
			*Ctx->QuestName(Def));
		Entry.Color = Entry.bDone ? FAbyssUiStyle::Rgb(0xffd98a) : (Entry.bMain ? C.Heading : C.Text);
		const std::string Id = Def.id;
		const int32 TabIndex = Tab;
		Body->AddSlot().Position(FVector2D(16.0, 86.0 + Row * GAbyssQuestLogRowH)).Size(FVector2D(250.0, GAbyssQuestLogRowH))
		[
			AbyssQuestLog_Row(Context, Entry, Id == Current, [this, Id, TabIndex]()
			{
				Selected[TabIndex] = Id;
				MarkDirty();
			})
		];
	}
	AddPager(Total);

	if (!Current.empty())
	{
		Body->AddSlot().Position(FVector2D(286.0, 84.0)).Size(FVector2D(420.0, 422.0))
		[
			BuildQuestDetail(Snap, Current)
		];
	}
}

TSharedRef<SWidget> SAbyssQuestLogPanel::BuildQuestDetail(const abyss::Snapshot& Snap, const std::string& QuestId)
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const abyss::DataStore& Data = *Ctx->GetData();
	const abyss::QuestSystem& Quests = *Snap.quests;
	const abyss::QuestDef* Def = Data.FindQuest(QuestId);
	const abyss::QuestProgress* Progress = Quests.Progress(QuestId);
	if (Def == nullptr || Progress == nullptr)
	{
		return SNullWidget::NullWidget;
	}
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const bool bMain = Def->category == abyss::QuestCategory::Main;
	const float Wrap = 404.f;
	TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox);
	const auto AddText = [&Lines, &Context, Wrap](const FString& Text, float Px, const FLinearColor& Color, bool bBold, float Top)
	{
		if (Text.IsEmpty())
		{
			return;
		}
		Lines->AddSlot().AutoHeight().Padding(FMargin(0.f, Top, 0.f, 0.f))
		[
			AbyssUi::Label(*Context, FText::AsCultureInvariant(Text), Px, Color, bBold, 1, Wrap)
		];
	};

	AddText(Ctx->QuestName(*Def), 16.f, bMain ? FAbyssUiStyle::Rgb(0xffd98a) : C.Parchment, true, 0.f);
	const std::string TypeId(abyss::EnumName(Def->type));
	AddText(FString::Printf(TEXT("%s  \x00B7  %s"),
		*(bMain ? Ctx->LocOrStr("ui.questLog.mainQuest", TEXT("Main Quest")) : Ctx->LocOrStr("ui.questLog.sideQuest", TEXT("Side Quest"))),
		*Ctx->LocArgsOrStr("ui.questLog.typeLabel", TEXT("Type: {type}"), { FAbyssUiContext::Arg("type", Ctx->NameOr("sys.quest.type." + TypeId, TypeId)) })),
		11.f, C.Muted, false, 2.f);
	AddText(Ctx->NameOr(Def->descKey.empty() ? "data.quest." + Def->id + ".desc" : Def->descKey, Def->description), 12.f, C.TextSoft, false, 6.f);

	// special-type summary: clues / waves / craft phase
	int32 ClueFound = 0;
	int32 ClueTotal = 0;
	for (size_t Index = 0; Index < Def->objectives.size(); ++Index)
	{
		const abyss::QuestObjectiveDef& Objective = Def->objectives[Index];
		const int32 Current = Index < Progress->objectives.size() ? Progress->objectives[Index] : 0;
		if (Objective.type == abyss::ObjectiveType::InvestigateClue)
		{
			ClueFound += FMath::Min(Current, Objective.required);
			ClueTotal += Objective.required;
		}
		if (Objective.type == abyss::ObjectiveType::DefendWave)
		{
			AddText(Ctx->LocArgsOrStr("ui.questLog.waveProgress", TEXT("Wave {current}/{total}"),
				{ FAbyssUiContext::Arg("current", FMath::Min(Current, Objective.required)), FAbyssUiContext::Arg("total", Objective.required) }),
				12.f, C.Info, true, 6.f);
		}
	}
	if (ClueTotal > 0)
	{
		AddText(Ctx->LocArgsOrStr("ui.questLog.clueProgress", TEXT("Clues {found}/{total}"),
			{ FAbyssUiContext::Arg("found", ClueFound), FAbyssUiContext::Arg("total", ClueTotal) }), 12.f, C.Info, true, 6.f);
	}
	const std::string_view Phase = Quests.CraftPhaseKey(*Def, *Progress);
	if (!Phase.empty())
	{
		AddText(Ctx->LocArgsOrStr("ui.questLog.craftPhase", TEXT("Current Phase: {phase}"),
			{ FAbyssUiContext::ArgKey("phase", std::string(Phase)) }), 12.f, C.Info, true, 6.f);
	}

	// objectives with progress bars (done 0x5cc04a, else 0x3a7fd0)
	AddText(Ctx->LocOrStr("ui.questLog.objectives", TEXT("Objectives:")), 12.f, C.Heading, true, 8.f);
	for (size_t Index = 0; Index < Def->objectives.size(); ++Index)
	{
		const abyss::QuestObjectiveDef& Objective = Def->objectives[Index];
		const int32 Current = FMath::Min(Index < Progress->objectives.size() ? Progress->objectives[Index] : 0, Objective.required);
		const bool bDone = Current >= Objective.required || Progress->status == abyss::QuestStatus::TurnedIn;
		const FString Label = FString::Printf(TEXT("%s %s %s"), bDone ? TEXT("\x2713") : TEXT("\x25CB"),
			*Ctx->ObjectiveTypeLabel(Objective, Def->type), *Ctx->ObjectiveTargetLabel(*Def, static_cast<int32>(Index)));
		const float Fraction = Objective.required > 0 ? static_cast<float>(Current) / static_cast<float>(Objective.required) : 1.f;
		const FString Count = FString::Printf(TEXT("%d/%d"), Current, Objective.required);
		Lines->AddSlot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Label), 12.f, bDone ? C.Good : C.Text, false, 1, 330.f)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Count), 12.f, bDone ? C.Good : C.TextSoft, true, 1)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(14.f, 2.f, 0.f, 0.f))
			[
				SNew(SAbyssCanvas, Context, [Fraction, bDone](FAbyssPainter& P, const FVector2D& Size)
				{
					P.Bar(FVector2D::ZeroVector, Size, bDone ? 1.f : Fraction, bDone ? FAbyssUiStyle::Rgb(0x5cc04a) : FAbyssUiStyle::Rgb(0x3a7fd0));
				})
				.Size(FVector2D(300.0, 7.0))
			]
		];
	}

	// rewards
	const abyss::QuestRewardDef& Rewards = Def->rewards;
	TArray<FString> RewardParts;
	if (Rewards.exp > 0)
	{
		RewardParts.Add(Ctx->LocArgsOrStr("ui.questLog.rewardExp", TEXT("EXP +{exp}"), { FAbyssUiContext::Arg("exp", Rewards.exp) }));
	}
	if (Rewards.gold > 0)
	{
		RewardParts.Add(Ctx->LocArgsOrStr("ui.questLog.rewardGold", TEXT("Gold +{gold}"), { FAbyssUiContext::Arg("gold", Rewards.gold) }));
	}
	if (!Rewards.items.empty())
	{
		RewardParts.Add(Ctx->LocArgsOrStr("ui.questLog.rewardItems", TEXT("Items x{count}"),
			{ FAbyssUiContext::Arg("count", static_cast<int64>(Rewards.items.size())) }));
	}
	if (!Rewards.choices.empty())
	{
		RewardParts.Add(Ctx->LocArgsOrStr("sys.questCard.rewardChoice", TEXT("pick 1 of {count} gear"),
			{ FAbyssUiContext::Arg("count", static_cast<int64>(Rewards.choices.size())) }));
	}
	if (!Rewards.petReward.empty())
	{
		RewardParts.Add(Ctx->LocOrStr("sys.questCard.rewardPet", TEXT("Pet")));
	}
	if (RewardParts.Num() > 0)
	{
		AddText(Ctx->LocOrStr("ui.questLog.rewards", TEXT("Rewards:")), 12.f, C.Heading, true, 8.f);
		AddText(FString::Join(RewardParts, TEXT("   ")), 12.f, FAbyssUiStyle::Rgb(0xffd35a), false, 2.f);
	}
	// prerequisites (names via the accessor, Q18)
	if (!Def->prereqQuests.empty())
	{
		TArray<FString> Names;
		for (const std::string& Prereq : Def->prereqQuests)
		{
			const abyss::QuestDef* PrereqDef = Data.FindQuest(Prereq);
			Names.Add(PrereqDef != nullptr ? Ctx->QuestName(*PrereqDef) : AbyssText::ToFString(Prereq));
		}
		AddText(Ctx->LocArgsOrStr("ui.questLog.prereqs", TEXT("Prerequisite: {names}"), { FAbyssUiContext::Arg("names", FString::Join(Names, TEXT(", "))) }),
			11.f, C.Muted, false, 8.f);
	}

	TSharedRef<SScrollBox> Scroll = AbyssUi::ScrollBox(*Ctx);
	Scroll->AddSlot()[Lines];
	TSharedRef<SVerticalBox> Pane = SNew(SVerticalBox)
		+ SVerticalBox::Slot().FillHeight(1.f)[Scroll];
	// track / untrack (guide arrow and tracker pin, quests 2.6)
	if (Progress->status == abyss::QuestStatus::Active || Progress->status == abyss::QuestStatus::Completed)
	{
		const bool bTracked = Quests.Tracked() == QuestId;
		const std::string Id = QuestId;
		Pane->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(FMargin(0.f, 6.f, 8.f, 0.f))
			[
				SNew(SAbyssButton, Context)
				.Text(bTracked ? Ctx->LocOr("ui.questLog.untrack", TEXT("Stop tracking")) : Ctx->LocOr("ui.questLog.track", TEXT("Track")))
				.Kind(bTracked ? EAbyssButtonKind::Ghost : EAbyssButtonKind::Secondary)
				.FontPx(12.f)
				.Width(Ctx->IsTouch() ? 150.f : 120.f)
				.Height(Ctx->IsTouch() ? 44.f : 28.f)
				.OnClicked_Lambda([Context, Id, bTracked]()
				{
					Context->Submit(abyss::CmdQuestTrack{ bTracked ? std::string() : Id });
				})
			];
	}
	return Pane;
}

// =====================================================================================================================
// Lore (quests 10.4)
// =====================================================================================================================

void SAbyssQuestLogPanel::BuildLoreList(const abyss::Snapshot& Snap)
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const abyss::DataStore& Data = *Ctx->GetData();
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const std::vector<abyss::LoreEntryDef>& Entries = Data.Lore().entries;
	int32 Collected = 0;
	for (const abyss::LoreEntryDef& Entry : Entries)
	{
		if (Snap.lore != nullptr && Snap.lore->IsCollected(Entry.id))
		{
			++Collected;
		}
	}
	Body->AddSlot().Position(FVector2D(470.0, 50.0)).Size(FVector2D(236.0, 20.0))
	[
		SNew(SBox)
		.HAlign(HAlign_Right)
		[
			AbyssUi::Label(*Ctx, Ctx->LocArgsOr("ui.questLog.loreCollected", TEXT("{count}/{total} collected"),
				{ FAbyssUiContext::Arg("count", Collected), FAbyssUiContext::Arg("total", static_cast<int64>(Entries.size())) }), 12.f, C.Heading, true, 1)
		]
	];
	if (Entries.empty() || Collected == 0)
	{
		Body->AddSlot().Position(FVector2D(16.0, 92.0)).Size(FVector2D(250.0, 40.0))
		[
			AbyssUi::Label(*Ctx, Ctx->LocOr("ui.questLog.noLore", TEXT("No lore discovered yet")), 12.f, C.Dim, false, 1, 248.f)
		];
	}
	std::string& Current = Selected[2];
	if (Current.empty() || Snap.lore == nullptr || !Snap.lore->IsCollected(Current))
	{
		Current.clear();
		for (const abyss::LoreEntryDef& Entry : Entries)
		{
			if (Snap.lore != nullptr && Snap.lore->IsCollected(Entry.id))
			{
				Current = Entry.id;
				break;
			}
		}
	}
	if (Collected > 0)
	{
		const int32 Total = static_cast<int32>(Entries.size());
		const int32 Pages = FMath::Max(1, (Total + GAbyssQuestLogPerPage - 1) / GAbyssQuestLogPerPage);
		Page = FMath::Clamp(Page, 0, Pages - 1);
		for (int32 Row = 0; Row < GAbyssQuestLogPerPage; ++Row)
		{
			const int32 Index = Page * GAbyssQuestLogPerPage + Row;
			if (Index >= Total)
			{
				break;
			}
			const abyss::LoreEntryDef& Entry = Entries[static_cast<size_t>(Index)];
			const bool bKnown = Snap.lore != nullptr && Snap.lore->IsCollected(Entry.id);
			FAbyssQuestLogRow Line;
			Line.Id = Entry.id;
			Line.Label = bKnown ? Ctx->NameOr("data.lore." + Entry.id + ".name", Entry.name)
				: FString::Printf(TEXT("%s \x00B7 %s"), *Ctx->ZoneName(Entry.zone), *Ctx->LocOrStr("ui.questLog.loreUndiscovered", TEXT("Undiscovered")));
			Line.Color = bKnown ? FAbyssUiStyle::Rgb(0xe8dcc0) : C.Dim;
			const std::string Id = Entry.id;
			Body->AddSlot().Position(FVector2D(16.0, 86.0 + Row * GAbyssQuestLogRowH)).Size(FVector2D(250.0, GAbyssQuestLogRowH))
			[
				AbyssQuestLog_Row(Context, Line, Id == Current, bKnown ? TFunction<void()>([this, Id]()
				{
					Selected[2] = Id;
					MarkDirty();
				}) : TFunction<void()>())
			];
		}
		AddPager(Total);
	}
	if (!Current.empty())
	{
		Body->AddSlot().Position(FVector2D(286.0, 84.0)).Size(FVector2D(420.0, 422.0))
		[
			BuildLoreDetail(Snap, Current)
		];
	}
}

TSharedRef<SWidget> SAbyssQuestLogPanel::BuildLoreDetail(const abyss::Snapshot& Snap, const std::string& LoreId)
{
	const abyss::DataStore& Data = *Ctx->GetData();
	const abyss::LoreEntryDef* Entry = Data.Lore().Find(LoreId);
	if (Entry == nullptr)
	{
		return SNullWidget::NullWidget;
	}
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	TSharedRef<SScrollBox> Scroll = AbyssUi::ScrollBox(*Ctx);
	Scroll->AddSlot()
	[
		AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Ctx->NameOr("data.lore." + Entry->id + ".text", Entry->text)), 13.f,
			FAbyssUiStyle::Rgb(0xe8dcc0), false, 1, 380.f)
	];
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Ctx->NameOr("data.lore." + Entry->id + ".name", Entry->name)), 16.f, C.Parchment, true, 1, 404.f)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 8.f))
		[
			AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Ctx->ZoneName(Entry->zone)), 11.f, C.Muted, false, 1)
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SAbyssCanvas, Context, [](FAbyssPainter& P, const FVector2D& Size)
			{
				// parchment well (border 0x5a4a30)
				P.RoundBox(FVector2D::ZeroVector, Size, FAbyssUiStyle::Rgb(0x14100b), 5.f, FAbyssUiStyle::Rgb(0x5a4a30), 1.2f);
			})
			.Padding(FMargin(12.f, 10.f))
			[
				Scroll
			]
		];
}
