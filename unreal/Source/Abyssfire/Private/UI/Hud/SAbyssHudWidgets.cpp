#include "UI/Hud/SAbyssHudWidgets.h"

#include "Algo/StableSort.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include <utility>
#include <vector>

#include "abyss/data/DataStore.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/quests/QuestSystem.h"

#include "Framework/AbyssText.h"
#include "UI/AbyssUiStyle.h"
#include "UI/Core/AbyssUiDraw.h"
#include "UI/Widgets/AbyssUiWidgets.h"

// =====================================================================================================================
// Combat log (6.10)
// =====================================================================================================================

void SAbyssCombatLog::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	Ctx = InContext;
	bTouch = InContext->IsTouch();
	bExpanded = !bTouch;
	SetVisibility(EVisibility::SelfHitTestInvisible);
	Rebuild();
}

FVector2D SAbyssCombatLog::GetDesignSize() const
{
	if (!bTouch)
	{
		return FVector2D(290.0, 150.0);
	}
	return bExpanded ? FVector2D(470.0, 300.0) : FVector2D(470.0, 120.0);
}

FLinearColor SAbyssCombatLog::TypeColor(abyss::LogType Type) const
{
	switch (Type)
	{
	case abyss::LogType::System: return FAbyssUiStyle::Rgb(0xe8c77a);
	case abyss::LogType::Combat: return FAbyssUiStyle::Rgb(0xff8a72);
	case abyss::LogType::Loot: return FAbyssUiStyle::Rgb(0x7ed36a);
	case abyss::LogType::Info: return FAbyssUiStyle::Rgb(0x7fb6ff);
	case abyss::LogType::Quest: return FAbyssUiStyle::Rgb(0xb8b0a4);
	}
	return FAbyssUiStyle::Rgb(0xb8b0a4);
}

float SAbyssCombatLog::LineAlpha(int32 IndexFromNewest, double Age) const
{
	if (bExpanded)
	{
		return FMath::Max(0.55f, 1.f - IndexFromNewest * 0.07f);
	}
	// collapsed: alpha clamp((12000 - age) / 3000, 0, 1) x [0.95, 0.75, 0.55][i]
	static const float Weights[3] = { 0.95f, 0.75f, 0.55f };
	const float Fade = FMath::Clamp(static_cast<float>((12.0 - Age) / 3.0), 0.f, 1.f);
	return IndexFromNewest < 3 ? Fade * Weights[IndexFromNewest] : 0.f;
}

void SAbyssCombatLog::AddLine(const abyss::LocText& Text, abyss::LogType Type)
{
	FLine Line;
	Line.Loc = Text;
	Line.Type = Type;
	Line.Time = Ctx->Now();
	Buffer.Add(MoveTemp(Line));
	while (Buffer.Num() > 8)
	{
		Buffer.RemoveAt(0);
	}
	Rebuild();
}

void SAbyssCombatLog::AddLocalLine(const FString& Text, abyss::LogType Type)
{
	FLine Line;
	Line.Resolved = Text;
	Line.Type = Type;
	Line.Time = Ctx->Now();
	Buffer.Add(MoveTemp(Line));
	while (Buffer.Num() > 8)
	{
		Buffer.RemoveAt(0);
	}
	Rebuild();
}

void SAbyssCombatLog::Clear()
{
	Buffer.Reset();
	Rebuild();
}

void SAbyssCombatLog::SetExpanded(bool bInExpanded)
{
	if (bExpanded != bInExpanded)
	{
		bExpanded = bInExpanded;
		Rebuild();
	}
}

void SAbyssCombatLog::SetTouch(bool bInTouch)
{
	if (bTouch != bInTouch)
	{
		bTouch = bInTouch;
		bExpanded = !bTouch;
		Rebuild();
	}
}

void SAbyssCombatLog::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	// Collapsed (touch): lines older than 12 s disappear; refreshed every 250 ms like the web.
	if (!bExpanded && InCurrentTime >= NextCollapsedRefresh)
	{
		NextCollapsedRefresh = InCurrentTime + 0.25;
		Rebuild();
	}
}

void SAbyssCombatLog::Rebuild()
{
	const FAbyssUiStyle& UiStyle = Ctx->Style();
	const FVector2D Size = GetDesignSize();
	const float FontPx = bTouch ? 19.f : 12.f;
	const int32 Outline = bTouch ? 3 : 2;
	const double Now = Ctx->Now();
	SAssignNew(Lines, SVerticalBox);

	const int32 Count = Buffer.Num();
	int32 Shown = 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int32 FromNewest = Count - 1 - Index;
		const FLine& Line = Buffer[Index];
		const double Age = Now - Line.Time;
		if (!bExpanded && (FromNewest >= 3 || Age >= 12.0))
		{
			continue;
		}
		const FString Text = Line.Resolved.IsEmpty() ? Ctx->LocStr(Line.Loc) : Line.Resolved;
		FLinearColor Color = TypeColor(Line.Type);
		Color.A = LineAlpha(FromNewest, Age);
		Lines->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 1.f, 0.f, 0.f))
			[
				SNew(STextBlock)
				.Text(FText::AsCultureInvariant(Text))
				.Font(UiStyle.Body(FontPx, false, Outline))
				.ColorAndOpacity(FSlateColor(Color))
				.AutoWrapText(false)
				.WrapTextAt(static_cast<float>(Size.X - 20.0))
			];
		++Shown;
	}

	const FText Header = Ctx->LocOr("ui.hud.combatLog", TEXT("Combat log"));
	const bool bFramed = bExpanded;
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(static_cast<float>(Size.X))
		.HeightOverride(static_cast<float>(Size.Y))
		.Clipping(EWidgetClipping::ClipToBounds)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SAbyssCanvas, Context, [bFramed](FAbyssPainter& P, const FVector2D& LocalSize)
				{
					if (bFramed)
					{
						P.Frame(FVector2D::ZeroVector, LocalSize, 2, P.Style.Colors().Gold, 0.f, 0.9f);
					}
				})
			]
			+ SOverlay::Slot()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Left)
			.Padding(FMargin(10.f, 6.f, 10.f, 0.f))
			[
				SNew(STextBlock)
				.Visibility(bFramed ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
				.Text(Header)
				.Font(UiStyle.Font(EAbyssFontFace::Title, bTouch ? 16.f : 11.f, true, 1))
				.ColorAndOpacity(FSlateColor(UiStyle.Colors().Heading))
			]
			+ SOverlay::Slot()
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(10.f, bFramed ? 22.f : 0.f, 10.f, 6.f))
			[
				Lines.ToSharedRef()
			]
		]
	];
}

// =====================================================================================================================
// Quest tracker (6.12)
// =====================================================================================================================

void SAbyssQuestTracker::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	Ctx = InContext;
	bTouch = InContext->IsTouch();
	ChildSlot
	[
		SAssignNew(Content, SBox)
		.Visibility(EVisibility::SelfHitTestInvisible)
	];
}

void SAbyssQuestTracker::SetTouch(bool bInTouch)
{
	if (bTouch != bInTouch)
	{
		bTouch = bInTouch;
		Signature.Reset();
	}
}

void SAbyssQuestTracker::Refresh(const abyss::Snapshot& Snap, bool bForce)
{
	const abyss::DataStore* Data = Ctx->GetData();
	if (Data == nullptr || Snap.quests == nullptr)
	{
		Content->SetContent(SNullWidget::NullWidget);
		Signature.Reset();
		return;
	}
	const abyss::QuestSystem& Quests = *Snap.quests;
	const std::string& Guided = Quests.Tracked();
	const std::string& Zone = Snap.zone.mapId;
	const FAbyssUiPalette& C = Ctx->Style().Colors();

	// getActiveQuests (active + completed), ranked guided / this zone / other, then stably main before side and
	// unfinished before completed (QuestTrackerHUD.ts:144-176).
	struct FRanked
	{
		const abyss::QuestDef* Def = nullptr;
		const abyss::QuestProgress* Progress = nullptr;
		int32 Rank = 2;
	};
	TArray<FRanked> Ranked;
	for (const std::pair<const abyss::QuestDef*, const abyss::QuestProgress*>& Open : Quests.OpenQuests())
	{
		if (Open.first == nullptr || Open.second == nullptr)
		{
			continue;
		}
		FRanked Entry;
		Entry.Def = Open.first;
		Entry.Progress = Open.second;
		Entry.Rank = Open.first->id == Guided ? 0 : (Open.first->zone == Zone ? 1 : 2);
		Ranked.Add(Entry);
	}
	Algo::StableSortBy(Ranked, [](const FRanked& Entry) { return Entry.Rank; });
	Algo::StableSort(Ranked, [](const FRanked& A, const FRanked& B)
	{
		const bool bMainA = A.Def->category == abyss::QuestCategory::Main;
		const bool bMainB = B.Def->category == abyss::QuestCategory::Main;
		if (bMainA != bMainB)
		{
			return bMainA;
		}
		const bool bDoneA = A.Progress->status == abyss::QuestStatus::Completed;
		const bool bDoneB = B.Progress->status == abyss::QuestStatus::Completed;
		return !bDoneA && bDoneB;
	});

	TArray<FEntry> Entries;
	FString NewSignature;
	for (const FRanked& Item : Ranked)
	{
		const abyss::QuestDef& Def = *Item.Def;
		const abyss::QuestProgress& Progress = *Item.Progress;
		FEntry Entry;
		Entry.QuestId = Def.id;
		Entry.bCompleted = Progress.status == abyss::QuestStatus::Completed;
		Entry.bMain = Def.category == abyss::QuestCategory::Main;
		Entry.bGuided = Def.id == Guided;
		const FString TagText = Entry.bMain ? Ctx->LocOrStr("ui.questTracker.mainTag", TEXT("[Main]")) : Ctx->LocOrStr("ui.questTracker.sideTag", TEXT("[Side]"));
		Entry.Title = FString::Printf(TEXT("%s%s %s%s"), Entry.bGuided ? TEXT("\x25B6 ") : TEXT(""), *TagText, *Ctx->QuestName(Def),
			Entry.bCompleted ? TEXT(" \x2713") : TEXT(""));
		Entry.TitleColor = Entry.bCompleted ? FAbyssUiStyle::Rgb(0xf1c40f) : (Entry.bMain ? FAbyssUiStyle::Rgb(0xe8c252) : FAbyssUiStyle::Rgb(0xa89060));
		int32 Done = 0;
		for (size_t Index = 0; Index < Def.objectives.size(); ++Index)
		{
			const abyss::QuestObjectiveDef& Objective = Def.objectives[Index];
			const int32 Current = Index < Progress.objectives.size() ? Progress.objectives[Index] : 0;
			const bool bDone = Current >= Objective.required;
			Done += bDone ? 1 : 0;
			const FString Line = FString::Printf(TEXT("    %s %s %s"), *Ctx->ObjectiveTypeLabel(Objective, Def.type),
				*Ctx->ObjectiveTargetLabel(Def, static_cast<int32>(Index)),
				bDone ? TEXT("\x2713") : *FString::Printf(TEXT("%d/%d"), FMath::Min(Current, Objective.required), Objective.required));
			Entry.Objectives.Add(TPair<FString, bool>(Line, bDone));
		}
		if (Entry.bCompleted)
		{
			Entry.Summary = Ctx->LocOrStr("sys.tracker.completed", TEXT("Completed - return to the NPC"));
			Entry.SummaryColor = FAbyssUiStyle::Rgb(0xf1c40f);
		}
		else if (Def.objectives.size() == 1)
		{
			const abyss::QuestObjectiveDef& Objective = Def.objectives[0];
			const int32 Current = Progress.objectives.empty() ? 0 : Progress.objectives[0];
			Entry.Summary = FString::Printf(TEXT("%s %d/%d"), *Ctx->ObjectiveTypeLabel(Objective, Def.type), FMath::Min(Current, Objective.required),
				Objective.required);
			Entry.SummaryColor = FAbyssUiStyle::Rgb(0xaaaaaa);
		}
		else
		{
			Entry.Summary = Ctx->LocArgsOrStr("sys.tracker.doneCount", TEXT("{done}/{total} done"),
				{ FAbyssUiContext::Arg("done", Done), FAbyssUiContext::Arg("total", static_cast<int64>(Def.objectives.size())) });
			Entry.SummaryColor = FAbyssUiStyle::Rgb(0xaaaaaa);
		}
		NewSignature += FString::Printf(TEXT("%s|%d|%s\n"), *AbyssText::ToFString(Def.id), Entry.bCompleted ? 1 : 0, *Entry.Summary);
		Entries.Add(MoveTemp(Entry));
	}
	TArray<FString> Expanded = ExpandedQuests.Array();
	Expanded.Sort();
	NewSignature += TEXT("|E:") + FString::Join(Expanded, TEXT(",")) + TEXT("|G:") + AbyssText::ToFString(Guided) + (bTouch ? TEXT("|T") : TEXT(""));
	if (!bForce && NewSignature == Signature)
	{
		return;
	}
	Signature = NewSignature;

	if (Entries.Num() == 0)
	{
		Content->SetContent(SNullWidget::NullWidget);
		return;
	}
	const float Width = GetWidth();
	const float TitlePx = bTouch ? 19.f : 12.f;
	const float SummaryPx = bTouch ? 16.f : 10.f;
	const float ObjectivePx = bTouch ? 14.f : 9.f;
	const int32 MaxVisible = bTouch ? 3 : 5;
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.f, 0.f, 0.f, 3.f))
		[
			SNew(STextBlock)
			.Text(Ctx->LocOr("ui.questTracker.header", TEXT("Quests")))
			.Font(Ctx->Style().Font(EAbyssFontFace::Title, bTouch ? 18.f : 12.f, true, 1))
			.ColorAndOpacity(FSlateColor(C.Heading))
		];
	for (int32 Index = 0; Index < Entries.Num() && Index < MaxVisible; ++Index)
	{
		const FEntry& Entry = Entries[Index];
		const FString QuestKey = AbyssText::ToFString(Entry.QuestId);
		const bool bExpanded = ExpandedQuests.Contains(QuestKey);
		const std::string QuestId = Entry.QuestId;
		TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::AsCultureInvariant(Entry.Title))
				.Font(Ctx->Style().Body(TitlePx, true, 2))
				.ColorAndOpacity(FSlateColor(Entry.TitleColor))
				.AutoWrapText(false)
				.WrapTextAt(Width - 6.f)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::AsCultureInvariant(Entry.Summary))
				.Font(Ctx->Style().Body(SummaryPx, false, 2))
				.ColorAndOpacity(FSlateColor(Entry.SummaryColor))
			];
		if (bExpanded)
		{
			for (const TPair<FString, bool>& Objective : Entry.Objectives)
			{
				Lines->AddSlot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::AsCultureInvariant(Objective.Key))
						.Font(Ctx->Style().Body(ObjectivePx, false, 1))
						.ColorAndOpacity(FSlateColor(Objective.Value ? FAbyssUiStyle::Rgb(0x66aa66) : FAbyssUiStyle::Rgb(0x888888)))
						.AutoWrapText(false)
						.WrapTextAt(Width - 6.f)
					];
			}
		}
		Box->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 3.f, 0.f, 0.f))
			[
				// A click on the entry pins the guide (setTracked) and toggles the objective lines (6.12).
				SNew(SAbyssHitArea, Context)
				.OnPressed_Lambda([this, Context, QuestId, QuestKey]()
				{
					Context->Submit(abyss::CmdQuestTrack{ QuestId });
					if (ExpandedQuests.Contains(QuestKey))
					{
						ExpandedQuests.Remove(QuestKey);
					}
					else
					{
						ExpandedQuests.Add(QuestKey);
					}
					Signature.Reset();
					if (const abyss::Snapshot* Snapshot = Context->GetSnapshot())
					{
						Refresh(*Snapshot, true);
					}
				})
				[
					Lines
				]
			];
	}
	if (Entries.Num() > MaxVisible)
	{
		Box->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 3.f, 0.f, 0.f))
			[
				SNew(STextBlock)
				.Text(Ctx->LocArgsOr("ui.questTracker.scrollIndicator", TEXT("+{count} more quests"),
					{ FAbyssUiContext::Arg("count", static_cast<int64>(Entries.Num() - MaxVisible)) }))
				.Font(Ctx->Style().Body(SummaryPx, false, 1))
				.ColorAndOpacity(FSlateColor(C.Muted))
			];
	}
	Content->SetContent(
		SNew(SBox)
		.WidthOverride(Width)
		[
			SNew(SAbyssCanvas, Context, [](FAbyssPainter& P, const FVector2D& Size)
			{
				P.Frame(FVector2D::ZeroVector, Size, 2, P.Style.Colors().Gold, 0.f, 0.85f);
			})
			.Padding(FMargin(9.f, 6.f, 9.f, 8.f))
			[
				Box
			]
		]);
}

// =====================================================================================================================
// Target frame (6.7)
// =====================================================================================================================

void SAbyssTargetFrame::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const TSharedRef<FAbyssHudState>& InState)
{
	Ctx = InContext;
	State = InState;
	SetVisibility(EVisibility::HitTestInvisible);
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(280.f)
		.HeightOverride(52.f)
	];
}

int32 SAbyssTargetFrame::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FAbyssHudState& S = *State;
	if (!S.bValid || S.Target == abyss::kNoEntity)
	{
		return LayerId;
	}
	FAbyssPainter P(AllottedGeometry, OutDrawElements, LayerId, InWidgetStyle, Ctx->Style());
	const FVector2D Size = S.bTouch ? FVector2D(260.0, 52.0) : FVector2D(280.0, 44.0);
	const FVector2D Pos((AllottedGeometry.GetLocalSize().X - Size.X) * 0.5, 0.0);
	P.Frame(Pos, Size, 2, FAbyssUiStyle::Rgb(0xc0503c), 0.f, 0.9f);
	const FString Text = S.TargetName.IsEmpty()
		? Ctx->LocOrStr("ui.hud.targetNone", TEXT("Target: none"))
		: Ctx->LocArgsOrStr("ui.hud.target", TEXT("Target: {targetName}"), { FAbyssUiContext::Arg("targetName", S.TargetName) });
	P.TextCentered(Pos + FVector2D(Size.X * 0.5, S.bTouch ? 16.0 : 14.0), Text, Ctx->Style().Body(S.bTouch ? 18.f : 12.f, true, 2),
		S.TargetName.IsEmpty() ? FAbyssUiStyle::Rgb(0x777788) : FAbyssUiStyle::Rgb(0xffb09a));
	const FVector2D BarPos = Pos + FVector2D(18.0, Size.Y - 15.0);
	const float Fraction = S.bTargetFound ? static_cast<float>(S.TargetHp / FMath::Max(1.0, S.TargetMaxHp)) : 0.f;
	P.Bar(BarPos, FVector2D(Size.X - 36.0, 8.0), Fraction, FAbyssUiStyle::Rgb(0xc0281e));
	return P.Layer;
}

// =====================================================================================================================
// Boss bar (6.13)
// =====================================================================================================================

void SAbyssBossBar::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const TSharedRef<FAbyssHudState>& InState)
{
	Ctx = InContext;
	State = InState;
	SetVisibility(EVisibility::HitTestInvisible);
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(600.f)
		.HeightOverride(76.f)
	];
}

void SAbyssBossBar::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	const FAbyssHudState& S = *State;
	// fade in / out over 400 ms
	const float Target = S.bValid && S.bBossBar ? 1.f : 0.f;
	Alpha = FMath::FInterpConstantTo(Alpha, Target, InDeltaTime, 1.f / 0.4f);
	const float Fraction = FMath::Clamp(static_cast<float>(S.BossHp / FMath::Max(1.0, S.BossMaxHp)), 0.f, 1.f);
	if (FMath::Abs(Fraction - ShownFraction) >= 0.001f)
	{
		ShownFraction = Fraction;
	}
}

int32 SAbyssBossBar::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (Alpha <= 0.f)
	{
		return LayerId;
	}
	FWidgetStyle Faded = InWidgetStyle;
	Faded.BlendColorAndOpacityTint(FLinearColor(1.f, 1.f, 1.f, Alpha));
	FAbyssPainter P(AllottedGeometry, OutDrawElements, LayerId, Faded, Ctx->Style());
	const FAbyssHudState& S = *State;
	const FVector2D Size(560.0, 14.0);
	const FVector2D Center(AllottedGeometry.GetLocalSize().X * 0.5, 40.0);
	const FVector2D Pos = Center - Size * 0.5;
	// name 18 px serif above, epithet 12 px below
	P.TextCentered(Center - FVector2D(0.0, 20.0), S.BossName, Ctx->Style().Serif(18.f, true, 2), FAbyssUiStyle::Rgb(0xffe2a8));
	P.Box(Pos - FVector2D(4.0, 4.0), Size + FVector2D(8.0, 8.0), FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0x0a0604), 0.9f));
	P.Box(Pos, Size, FAbyssUiStyle::Rgb(0x2a0606));
	const double FillW = Size.X * ShownFraction;
	P.Box(Pos, FVector2D(FillW, Size.Y), FAbyssUiStyle::Rgb(0xb3121e));
	P.Box(Pos, FVector2D(FillW, Size.Y / 3.0), FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0xff6a5a), 0.35f));
	P.RoundBox(Pos - FVector2D(4.0, 4.0), Size + FVector2D(8.0, 8.0), FLinearColor::Transparent, 1.f, FAbyssUiStyle::Rgb(0xc9a45a), 2.f);
	P.Diamond(FVector2D(Pos.X - 6.0, Center.Y), 12.f, FAbyssUiStyle::Rgb(0xc9a45a));
	P.Diamond(FVector2D(Pos.X + Size.X + 6.0, Center.Y), 12.f, FAbyssUiStyle::Rgb(0xc9a45a));
	if (!S.BossEpithet.IsEmpty())
	{
		P.TextCentered(Center + FVector2D(0.0, 22.0), S.BossEpithet, Ctx->Style().Serif(12.f, false, 1), FAbyssUiStyle::Rgb(0xd9b98a));
	}
	return P.Layer;
}

// =====================================================================================================================
// Info plate (6.9), dodge plate (6.8)
// =====================================================================================================================

void SAbyssInfoPlate::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const TSharedRef<FAbyssHudState>& InState)
{
	Ctx = InContext;
	State = InState;
	SetVisibility(EVisibility::HitTestInvisible);
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(210.f)
		.HeightOverride(56.f)
	];
}

int32 SAbyssInfoPlate::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FAbyssHudState& S = *State;
	if (!S.bValid)
	{
		return LayerId;
	}
	FAbyssPainter P(AllottedGeometry, OutDrawElements, LayerId, InWidgetStyle, Ctx->Style());
	const FVector2D Size(210.0, S.bTouch ? 56.0 : 46.0);
	P.Frame(FVector2D::ZeroVector, Size, 2, Ctx->Style().Colors().Gold, 0.f, 0.9f);
	P.TextCentered(FVector2D(Size.X * 0.5, 13.0), S.ZoneName, Ctx->Style().Title(S.bTouch ? 15.f : 13.f, true, 2), Ctx->Style().Colors().Parchment);
	// coin + gold (#ffd35a 13 px); embers stay hidden in milestone 1 (DECISIONS Q3)
	const FString Gold = FAbyssUiContext::Int(S.Gold);
	const FSlateFontInfo Font = Ctx->Style().Body(S.bTouch ? 16.f : 13.f, true, 2);
	const FVector2D GoldSize = FAbyssPainter::Measure(Gold, Font);
	const double RowY = S.bTouch ? 38.0 : 32.0;
	const double StartX = Size.X * 0.5 - (GoldSize.X + 18.0) * 0.5;
	if (const FSlateBrush* Coin = Ctx->Glyph(TEXT("gold")))
	{
		P.Brush(FVector2D(StartX, RowY - 7.0), FVector2D(14.0, 14.0), Coin);
	}
	else
	{
		P.Circle(FVector2D(StartX + 7.0, RowY), 6.f, FAbyssUiStyle::Rgb(0xe3b44c), FAbyssUiStyle::Rgb(0x8a5a10), 1.f);
	}
	P.Text(FVector2D(StartX + 18.0, RowY - GoldSize.Y * 0.5), Gold, Font, FAbyssUiStyle::Rgb(0xffd35a));
	return P.Layer;
}

void SAbyssDodgePlate::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const TSharedRef<FAbyssHudState>& InState)
{
	Ctx = InContext;
	State = InState;
	SetVisibility(EVisibility::HitTestInvisible);
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(158.f)
		.HeightOverride(26.f)
	];
}

int32 SAbyssDodgePlate::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FAbyssHudState& S = *State;
	if (!S.bValid || S.bTouch)
	{
		return LayerId;
	}
	FAbyssPainter P(AllottedGeometry, OutDrawElements, LayerId, InWidgetStyle, Ctx->Style());
	const FVector2D Size(158.0, 26.0);
	P.Frame(FVector2D::ZeroVector, Size, 2, Ctx->Style().Colors().Gold, 0.f, 0.9f);
	const bool bReady = S.DodgeRemainingMs <= 0.0;
	FString Text;
	if (bReady)
	{
		Text = Ctx->LocOrStr("ui.hud.dodgeReady", TEXT("Dodge [SPACE]"));
	}
	else
	{
		Text = Ctx->LocArgsOrStr("ui.hud.dodgeCooldown", TEXT("Dodge {seconds}s"),
			{ FAbyssUiContext::Arg("seconds", FAbyssUiContext::Fixed(S.DodgeRemainingMs / 1000.0, 1)) });
	}
	P.Circle(FVector2D(14.0, Size.Y * 0.5), 4.f, bReady ? FAbyssUiStyle::Rgb(0x9bd7ff) : FAbyssUiStyle::Rgb(0x3a4450));
	const FSlateFontInfo Font = Ctx->Style().Body(11.f, true, 1);
	const FVector2D TextSize = FAbyssPainter::Measure(Text, Font);
	P.Text(FVector2D(26.0, Size.Y * 0.5 - TextSize.Y * 0.5), Text, Font, bReady ? FAbyssUiStyle::Rgb(0x9bd7ff) : FAbyssUiStyle::Rgb(0x778899));
	return P.Layer;
}

// =====================================================================================================================
// Buff row
// =====================================================================================================================

void SAbyssBuffRow::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const TSharedRef<FAbyssHudState>& InState)
{
	Ctx = InContext;
	State = InState;
	SetVisibility(EVisibility::HitTestInvisible);
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(320.f)
		.HeightOverride(28.f)
	];
}

int32 SAbyssBuffRow::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FAbyssHudState& S = *State;
	if (!S.bValid)
	{
		return LayerId;
	}
	FAbyssPainter P(AllottedGeometry, OutDrawElements, LayerId, InWidgetStyle, Ctx->Style());
	const float Cell = S.bTouch ? 26.f : 22.f;
	double X = 0.0;
	// statuses (combat-feel 7): colour per status, the status name's first character as the glyph
	static const uint32 StatusColors[6] = { 0xff6633, 0x66ccff, 0x33cc33, 0xcc2222, 0x8899cc, 0xffd84a };
	for (int32 Index = 0; Index < 6; ++Index)
	{
		if ((S.StatusMask & (1u << Index)) == 0)
		{
			continue;
		}
		const abyss::StatusType Status = static_cast<abyss::StatusType>(Index);
		const std::string Id(abyss::EnumName(Status));
		FString Name = Ctx->NameOr("sys.statusEffect.name." + Id, Id);
		if (Ctx->HasKey("data.statusEffect." + Id))
		{
			Name = Ctx->LocStr("data.statusEffect." + Id);
		}
		const FLinearColor Color = FAbyssUiStyle::Rgb(StatusColors[Index]);
		P.RoundBox(FVector2D(X, 2.0), FVector2D(Cell, Cell), FAbyssUiStyle::Darken(Color, 0.55f), 4.f, Color, 1.5f);
		P.TextCentered(FVector2D(X + Cell * 0.5, 2.0 + Cell * 0.5), Name.Left(1), Ctx->Style().Body(Cell * 0.55f, true, 1), FLinearColor::White);
		X += Cell + 4.0;
	}
	// buffs: gold frame (bonuses) or violet (taunt / slow), remaining-time sweep
	for (const FAbyssHudBuff& Buff : S.Buffs)
	{
		const bool bDebuff = Buff.Stat == abyss::BuffStat::SlowEffect || Buff.Stat == abyss::BuffStat::Taunted;
		const FLinearColor Color = bDebuff ? FAbyssUiStyle::Rgb(0xa968ff) : Ctx->Style().Colors().GoldBright;
		const FVector2D Pos(X, 2.0);
		const FVector2D Center = Pos + FVector2D(Cell * 0.5, Cell * 0.5);
		P.RoundBox(Pos, FVector2D(Cell, Cell), FAbyssUiStyle::Rgb(0x1a171d), 4.f, Color, 1.5f);
		// glyph: an arrow up for bonuses, down for debuffs; a shield bar for mitigation buffs
		const bool bShield = Buff.Stat == abyss::BuffStat::DamageReduction || Buff.Stat == abyss::BuffStat::DefenseBonus
			|| Buff.Stat == abyss::BuffStat::ManaShield;
		if (bShield)
		{
			TArray<FVector2D> Shield;
			Shield.Add(Center + FVector2D(0.0, -Cell * 0.32));
			Shield.Add(Center + FVector2D(Cell * 0.28, -Cell * 0.2));
			Shield.Add(Center + FVector2D(Cell * 0.2, Cell * 0.12));
			Shield.Add(Center + FVector2D(0.0, Cell * 0.32));
			Shield.Add(Center + FVector2D(-Cell * 0.2, Cell * 0.12));
			Shield.Add(Center + FVector2D(-Cell * 0.28, -Cell * 0.2));
			P.ConvexPolygon(Shield, Color);
		}
		else
		{
			const double Dir = bDebuff ? 1.0 : -1.0;
			TArray<FVector2D> Arrow;
			Arrow.Add(Center + FVector2D(0.0, Dir * Cell * 0.3));
			Arrow.Add(Center + FVector2D(Cell * 0.26, -Dir * Cell * 0.02));
			Arrow.Add(Center + FVector2D(-Cell * 0.26, -Dir * Cell * 0.02));
			P.ConvexPolygon(Arrow, Color);
			P.Box(Center + FVector2D(-Cell * 0.08, bDebuff ? -Cell * 0.3 : 0.0), FVector2D(Cell * 0.16, Cell * 0.3), Color);
		}
		if (Buff.DurationMs > 0.0)
		{
			const float Frac = FMath::Clamp(static_cast<float>(1.0 - Buff.RemainingMs / Buff.DurationMs), 0.f, 1.f);
			if (Frac > 0.f)
			{
				P.SquarePie(Center, Cell * 0.5f - 1.f, -UE_HALF_PI, -UE_HALF_PI + Frac * UE_TWO_PI, FLinearColor(0.f, 0.f, 0.f, 0.55f));
			}
		}
		X += Cell + 4.0;
		if (X > AllottedGeometry.GetLocalSize().X - Cell)
		{
			break;
		}
	}
	return P.Layer;
}

// =====================================================================================================================
// Pet medallion (18.8)
// =====================================================================================================================

void SAbyssPetMedallion::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const TSharedRef<FAbyssHudState>& InState)
{
	Ctx = InContext;
	State = InState;
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(34.f)
		.HeightOverride(34.f)
	];
	SetVisibility(TAttribute<EVisibility>::CreateLambda([WeakState = TWeakPtr<FAbyssHudState>(InState)]()
	{
		const TSharedPtr<FAbyssHudState> Pinned = WeakState.Pin();
		return Pinned.IsValid() && Pinned->bValid && Pinned->bHasPets && !Pinned->bTouch ? EVisibility::Visible : EVisibility::Collapsed;
	}));
}

FReply SAbyssPetMedallion::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	Ctx->PlaySound(abyss::SfxId::Click);
	if (IAbyssUiHost* Host = Ctx->GetHost())
	{
		Host->TogglePanel(abyss::PanelId::Pets);
	}
	return FReply::Handled();
}

int32 SAbyssPetMedallion::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FAbyssHudState& S = *State;
	FAbyssPainter P(AllottedGeometry, OutDrawElements, LayerId, InWidgetStyle, Ctx->Style());
	const FVector2D Center(17.0, 17.0);
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	P.Circle(Center + FVector2D(0.0, 1.5), 17.f, FLinearColor(0.f, 0.f, 0.f, 0.5f));
	P.Circle(Center, 16.f, FAbyssUiStyle::Rgb(0x1a171d), IsHovered() ? C.GoldBright : C.Gold, 1.5f);
	if (!S.ActivePet.empty())
	{
		if (const FSlateBrush* Portrait = Ctx->PetPortrait(S.ActivePet))
		{
			P.Brush(Center - FVector2D(13.0, 13.0), FVector2D(26.0, 26.0), Portrait,
				S.bActivePetExhausted ? FLinearColor(0.5f, 0.5f, 0.5f, 1.f) : FLinearColor::White);
		}
		else
		{
			uint32 Color = 0x8fd4ff;
			if (const abyss::DataStore* Data = Ctx->GetData())
			{
				if (const abyss::PetDef* Def = Data->FindPet(S.ActivePet))
				{
					Color = Def->color;
				}
			}
			P.Glow(Center, 13.f, FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(Color), 0.9f), 6);
			P.TextCentered(Center, Ctx->PetName(S.ActivePet, S.ActivePetStage).Left(1), Ctx->Style().Body(13.f, true, 2), FLinearColor::White);
		}
	}
	// "P" badge
	const FString Key = Ctx->KeyHint(EAbyssInputAction::PanelPets);
	const FVector2D Badge(29.0, 29.0);
	P.Circle(Badge, 6.f, FAbyssUiStyle::Rgb(0x2a1a08), C.Gold, 1.f);
	P.TextCentered(Badge, Key.IsEmpty() ? FString(TEXT("P")) : Key.Left(1), Ctx->Style().Body(8.f, true, 0), C.GoldBright);
	return P.Layer;
}
