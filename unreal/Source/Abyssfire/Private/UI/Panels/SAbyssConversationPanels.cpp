#include "UI/Panels/SAbyssConversationPanels.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include <string>
#include <vector>

#include "abyss/data/DataStore.h"
#include "abyss/quests/Dialogue.h"
#include "abyss/quests/Lore.h"
#include "abyss/world/RandomEvents.h"

#include "Framework/AbyssText.h"
#include "UI/AbyssUiStyle.h"
#include "UI/Widgets/AbyssUiWidgets.h"

namespace
{
	/** Rough wrapped height of a text block (the panels size themselves to their text; the text scrolls beyond the cap). */
	double AbyssConversation_TextHeight(const FString& Text, const FSlateFontInfo& Font, double WrapWidth)
	{
		TArray<FString> Paragraphs;
		Text.ParseIntoArray(Paragraphs, TEXT("\n"), false);
		const double LineHeight = FAbyssPainter::Measure(TEXT("Ag\x6E0A"), Font).Y;
		double Lines = 0.0;
		for (const FString& Paragraph : Paragraphs)
		{
			const double Width = FAbyssPainter::Measure(Paragraph, Font).X;
			Lines += FMath::Max(1.0, FMath::CeilToDouble(Width / FMath::Max(1.0, WrapWidth)));
		}
		return FMath::Max(1.0, Lines) * LineHeight;
	}

	/** Round portrait medallion (story / NPC art, emblem fallback). */
	TSharedRef<SWidget> AbyssConversation_Portrait(const TSharedRef<FAbyssUiContext>& Ctx, const std::string& ArtId, float Diameter,
		const FLinearColor& Rim)
	{
		return SNew(SAbyssCanvas, Ctx, [Ctx, ArtId, Rim](FAbyssPainter& P, const FVector2D& Size)
		{
			const FVector2D Center = Size * 0.5;
			const float R = static_cast<float>(FMath::Min(Size.X, Size.Y) * 0.5);
			P.Circle(Center + FVector2D(0.0, 2.0), R, FLinearColor(0.f, 0.f, 0.f, 0.45f));
			P.Circle(Center, R, FAbyssUiStyle::Rgb(0x0c0b0e), Rim, 2.f);
			if (const FSlateBrush* Portrait = Ctx->Portrait(ArtId))
			{
				P.Brush(Center - FVector2D(R - 3.0, R - 3.0), FVector2D((R - 3.0) * 2.0, (R - 3.0) * 2.0), Portrait);
			}
			else
			{
				P.Diamond(Center, R * 0.9f, FAbyssUiStyle::WithAlpha(Rim, 0.4f));
			}
		})
		.Size(FVector2D(Diameter, Diameter));
	}
}

// =====================================================================================================================
// Dialogue (tree + linear)
// =====================================================================================================================

void SAbyssDialoguePanel::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const std::string& InNpcId)
{
	SAbyssPanelBase::Construct(SAbyssPanelBase::FArguments(), InContext);
	NpcId = InNpcId;
	SetPanelContent(SAssignNew(Content, SBox));
}

void SAbyssDialoguePanel::Refresh(const abyss::Snapshot& Snap)
{
	const abyss::DataStore* Data = Ctx->GetData();
	if (Data == nullptr || Snap.dialogue == nullptr || !Snap.dialogue->open)
	{
		return;  // closing: EvPanelRequest{close} removes the panel
	}
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const abyss::DialogueView& View = *Snap.dialogue;
	if (!View.npcId.empty())
	{
		NpcId = View.npcId;
	}
	const abyss::NpcDef* Npc = Data->FindNpc(NpcId);
	const bool bTouch = Ctx->IsTouch();
	const float WrapWidth = 432.f;

	// text: tree node (data.dialogue.<treeId>.<nodeId>.text) or the linear line (data.npc.<id>.dialogue.<1|0>)
	FString Text;
	const abyss::DialogueNode* Node = View.tree != nullptr ? View.tree->FindNode(View.nodeId) : nullptr;
	if (!View.linear && View.tree != nullptr && Node != nullptr)
	{
		Text = Ctx->NameOr("data.dialogue." + View.tree->id + "." + Node->id + ".text", Node->text);
	}
	else if (Npc != nullptr && !Npc->dialogue.empty())
	{
		const size_t Line = Npc->dialogue.size() > 1 ? 1 : 0;
		Text = Ctx->NameOr("data.npc." + NpcId + ".dialogue." + std::to_string(Line), Npc->dialogue[Line]);
	}

	// buttons from the core (7.1 step 3)
	TSharedRef<SVerticalBox> Buttons = SNew(SVerticalBox);
	int32 ButtonCount = 0;
	for (int32 Index = 0; Index < static_cast<int32>(View.buttons.size()); ++Index)
	{
		const abyss::DialogueButton& Button = View.buttons[static_cast<size_t>(Index)];
		FText Label;
		EAbyssButtonKind Kind = EAbyssButtonKind::Secondary;
		switch (Button.kind)
		{
		case abyss::DialogueButtonKind::Choice:
		{
			FString ChoiceText;
			if (View.tree != nullptr && Node != nullptr && Button.choiceIndex >= 0 && static_cast<size_t>(Button.choiceIndex) < Node->choices.size())
			{
				ChoiceText = Ctx->NameOr("data.dialogue." + View.tree->id + "." + Node->id + ".choice." + std::to_string(Button.choiceIndex),
					Node->choices[static_cast<size_t>(Button.choiceIndex)].text);
			}
			if (Button.inProgress)
			{
				ChoiceText += Ctx->LocOrStr("ui.dialogue.inProgress", TEXT(" (In Progress)"));
				Kind = EAbyssButtonKind::Ghost;
			}
			else
			{
				Kind = ButtonCount == 0 ? EAbyssButtonKind::Primary : EAbyssButtonKind::Secondary;
			}
			Label = FText::AsCultureInvariant(ChoiceText);
			break;
		}
		case abyss::DialogueButtonKind::Continue:
			Label = Ctx->LocOr("ui.dialogue.continue", TEXT("Continue"));
			Kind = EAbyssButtonKind::Primary;
			break;
		case abyss::DialogueButtonKind::Back:
			Label = Ctx->LocOr("ui.dialogue.back", TEXT("Back"));
			Kind = EAbyssButtonKind::Ghost;
			break;
		case abyss::DialogueButtonKind::Leave:
			Label = Ctx->LocOr("ui.dialogue.leave", TEXT("Leave"));
			Kind = EAbyssButtonKind::Ghost;
			break;
		}
		Buttons->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.f, 3.f))
			[
				SNew(SAbyssButton, Context)
				.Text(Label)
				.Kind(Kind)
				.FontPx(bTouch ? 16.f : 13.f)
				.Width(480.f - 64.f)
				.Height(bTouch ? 48.f : 32.f)
				.OnClicked_Lambda([Context, Index]() { Context->Submit(abyss::CmdDialogueChoose{ Index }); })
			];
		++ButtonCount;
	}
	const bool bLinear = View.buttons.empty();
	const FSlateFontInfo TextFont = Ctx->Style().Body(14.f, false, 1);
	const double TextH = AbyssConversation_TextHeight(Text, TextFont, WrapWidth);
	const double ButtonsH = ButtonCount * ((bTouch ? 48.0 : 32.0) + 6.0);
	const double MaxTextH = FMath::Max(80.0, 520.0 - 36.0 - 96.0 - ButtonsH - 40.0);
	const double ShownTextH = FMath::Min(TextH + 8.0, MaxTextH);
	const bool bScrolls = TextH + 8.0 > MaxTextH;
	Height = FMath::Clamp(36.0 + 96.0 + ShownTextH + 12.0 + ButtonsH + (bLinear || bScrolls ? 24.0 : 0.0) + 18.0, 220.0, 560.0);

	TSharedRef<SScrollBox> TextScroll = AbyssUi::ScrollBox(*Ctx);
	TSharedRef<STextBlock> TextBlock = AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Text), 14.f, C.Text, false, 1, WrapWidth);
	TextBlock->SetJustification(bLinear ? ETextJustify::Center : ETextJustify::Left);
	TextScroll->AddSlot()[TextBlock];

	const std::string ArtId = Npc != nullptr && !Npc->spriteId.empty() ? Npc->spriteId : NpcId;
	TSharedRef<SVerticalBox> Column = SNew(SVerticalBox)
		// portrait + subtitle
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			AbyssConversation_Portrait(Context, ArtId, 64.f, C.Gold)
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 4.f, 0.f, 8.f))
		[
			AbyssUi::Label(*Ctx, Ctx->LocOr("ui.dialogue.subtitle", TEXT("Dialogue")), 11.f, C.Muted, false, 1)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(static_cast<float>(ShownTextH))
			[
				TextScroll
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 12.f, 0.f, 0.f))
		[
			Buttons
		];
	if (bLinear || bScrolls)
	{
		const FText Hint = bScrolls
			? (bTouch ? Ctx->LocOr("ui.dialogue.scrollHintTouch", TEXT("Drag to read more")) : Ctx->LocOr("ui.dialogue.scrollHint", TEXT("Scroll for more")))
			: Ctx->LocOr("ui.dialogue.closeHint", TEXT("Click outside to close"));
		Column->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.f, 6.f, 0.f, 0.f))
			[
				AbyssUi::Label(*Ctx, Hint, 11.f, C.Muted, false, 1)
			];
	}

	Content->SetContent(
		SNew(SAbyssPanelFrame, Context)
		.Size(GetDesignSize())
		.Title(FText::AsCultureInvariant(Ctx->NpcName(NpcId)))
		.OnClose_Lambda([this]() { RequestClose(); })
		[
			Column
		]);
}

// =====================================================================================================================
// Mini-boss
// =====================================================================================================================

void SAbyssMiniBossPanel::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const std::string& InNameKey,
	const std::string& InMonsterId, const std::vector<std::string>& InLineKeys)
{
	SAbyssPanelBase::Construct(SAbyssPanelBase::FArguments(), InContext);
	NameKey = InNameKey;
	MonsterId = InMonsterId;
	LineKeys = InLineKeys;
	SetPanelContent(SAssignNew(Content, SBox));
}

void SAbyssMiniBossPanel::Refresh(const abyss::Snapshot& Snap)
{
	if (bBuilt)
	{
		return;
	}
	const abyss::DataStore* Data = Ctx->GetData();
	if (Data == nullptr)
	{
		return;
	}
	bBuilt = true;
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const bool bTouch = Ctx->IsTouch();
	const FLinearColor Red = FAbyssUiStyle::Rgb(0xe0503c);

	// lines: from EvMiniBossDialogue, else the linear tree (start -> nextNodeId until isEnd) of the monster
	std::vector<std::string> Keys = LineKeys;
	if (Keys.empty() && !MonsterId.empty())
	{
		if (const abyss::DialogueTree* Tree = Data->Dialogues().Find(MonsterId))
		{
			const abyss::DialogueNode* Node = Tree->FindNode(Tree->startNodeId);
			for (int32 Guard = 0; Node != nullptr && Guard < 64; ++Guard)
			{
				Keys.push_back("data.miniBossDialogue." + MonsterId + "." + Node->id);
				if (Node->isEnd || Node->nextNodeId.empty())
				{
					break;
				}
				Node = Tree->FindNode(Node->nextNodeId);
			}
		}
	}
	const FString BossName = !NameKey.empty() ? Ctx->LocStr(NameKey) : Ctx->MonsterName(MonsterId);
	TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox);
	const FSlateFontInfo LineFont = Ctx->Style().Serif(14.f, false, 1);
	double TextH = 0.0;
	for (const std::string& Key : Keys)
	{
		const FString Line = FString::Printf(TEXT("\x201C%s\x201D"), *Ctx->LocStr(Key));
		TextH += AbyssConversation_TextHeight(Line, LineFont, 440.0) + 8.0;
		TSharedRef<STextBlock> Block = SNew(STextBlock)
			.Text(FText::AsCultureInvariant(Line))
			.Font(LineFont)
			.ColorAndOpacity(FSlateColor(FAbyssUiStyle::Rgb(0xf0dcc8)))
			.WrapTextAt(440.f)
			.Justification(ETextJustify::Center);
		Lines->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 4.f))[Block];
	}
	Height = FMath::Clamp(36.0 + 24.0 + TextH + 20.0 + (bTouch ? 52.0 : 34.0) + 26.0, 260.0, 600.0);
	TSharedRef<SScrollBox> Scroll = AbyssUi::ScrollBox(*Ctx);
	Scroll->AddSlot()[Lines];

	Content->SetContent(
		SNew(SAbyssPanelFrame, Context)
		.Size(GetDesignSize())
		.Accent(Red)
		.Title(FText::AsCultureInvariant(FString::Printf(TEXT("\x2694 %s \x2694"), *BossName)))
		.OnClose_Lambda([this]() { RequestClose(); })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(0.f, 8.f, 0.f, 0.f))
			[
				Scroll
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 10.f, 0.f, 4.f))
			[
				SNew(SAbyssButton, Context)
				.Text(Ctx->LocOr("ui.miniBoss.fight", TEXT("Fight")))
				.Kind(EAbyssButtonKind::Danger)
				.FontPx(bTouch ? 18.f : 14.f)
				.Width(bTouch ? 220.f : 180.f)
				.Height(bTouch ? 52.f : 34.f)
				.OnClicked_Lambda([this]()
				{
					// 7.11: the button (or the backdrop) dismisses the panel and the fight starts
					RequestClose();
				})
			]
		]);
}

// =====================================================================================================================
// Lore popup
// =====================================================================================================================

void SAbyssLorePanel::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	SAbyssPanelBase::Construct(SAbyssPanelBase::FArguments(), InContext);
	ShownAt = InContext->Now();
	SetPanelContent(SAssignNew(Content, SBox));
}

void SAbyssLorePanel::Refresh(const abyss::Snapshot& Snap)
{
	const abyss::DataStore* Data = Ctx->GetData();
	if (Data == nullptr || Snap.loreText == nullptr || !Snap.loreText->open)
	{
		return;
	}
	if (Snap.loreText->loreId == LoreId)
	{
		return;
	}
	LoreId = Snap.loreText->loreId;
	ShownAt = Ctx->Now();  // a newer pickup restarts the 8 s timer (FIX quests Q15)
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const abyss::LoreEntryDef* Entry = Data->Lore().Find(LoreId);
	const FString Name = Entry != nullptr ? Ctx->NameOr("data.lore." + Entry->id + ".name", Entry->name) : AbyssText::ToFString(LoreId);
	const FString Zone = Entry != nullptr ? Ctx->ZoneName(Entry->zone) : FString();
	const FString Text = Entry != nullptr ? Ctx->NameOr("data.lore." + Entry->id + ".text", Entry->text) : FString();
	TSharedRef<SScrollBox> Scroll = AbyssUi::ScrollBox(*Ctx);
	Scroll->AddSlot()
	[
		AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Text), 13.f, FAbyssUiStyle::Rgb(0xe8dcc0), false, 1, 370.f)
	];
	Content->SetContent(
		SNew(SAbyssPanelFrame, Context)
		.Size(GetDesignSize())
		.Title(FText::AsCultureInvariant(Name))
		.OnClose_Lambda([this]() { RequestClose(); })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Zone), 11.f, C.Muted, false, 1)
			]
			+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(0.f, 6.f, 0.f, 0.f))
			[
				SNew(SAbyssCanvas, Context, [](FAbyssPainter& P, const FVector2D& Size)
				{
					P.RoundBox(FVector2D::ZeroVector, Size, FAbyssUiStyle::Rgb(0x14100b), 5.f, FAbyssUiStyle::Rgb(0x5a4a30), 1.2f);
				})
				.Padding(FMargin(12.f, 8.f))
				[
					Scroll
				]
			]
		]);
}

void SAbyssLorePanel::TickPanel(const abyss::Snapshot& Snap, double Now)
{
	// 7.12: the popup closes itself after 8000 ms
	if (!LoreId.empty() && Now - ShownAt >= 8.0)
	{
		LoreId.clear();
		RequestClose();
	}
}

// =====================================================================================================================
// Puzzle prompt (world 13.3)
// =====================================================================================================================

void SAbyssPuzzlePanel::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	SAbyssPanelBase::Construct(SAbyssPanelBase::FArguments(), InContext);
	SetPanelContent(SAssignNew(Content, SBox));
}

void SAbyssPuzzlePanel::Refresh(const abyss::Snapshot& Snap)
{
	if (Snap.puzzle == nullptr || !Snap.puzzle->open)
	{
		return;
	}
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const abyss::PuzzlePromptState& Puzzle = *Snap.puzzle;
	const bool bTouch = Ctx->IsTouch();
	const abyss::EntityId Prop = Puzzle.prop;
	// keys: sys.event.puzzle.<zone>.<index>.<field>, else sys.event.puzzle.<zone>.<field>, else the fallback entry
	const auto PuzzleText = [this, &Puzzle](const char* Field) -> FString
	{
		const std::string Indexed = "sys.event.puzzle." + Puzzle.zoneId + "." + std::to_string(Puzzle.puzzleIndex) + "." + Field;
		if (Puzzle.puzzleIndex >= 0 && Ctx->HasKey(Indexed))
		{
			return Ctx->LocStr(Indexed);
		}
		const std::string Zoned = "sys.event.puzzle." + Puzzle.zoneId + "." + Field;
		if (Ctx->HasKey(Zoned))
		{
			return Ctx->LocStr(Zoned);
		}
		return Ctx->LocStr("sys.event.puzzle.fallback." + std::string(Field));
	};
	const FString Prompt = PuzzleText("prompt");
	const FString Solution = PuzzleText("solution");
	Content->SetContent(
		SNew(SAbyssPanelFrame, Context)
		.Size(GetDesignSize())
		.Title(Ctx->LocOr("ui.puzzle.title", TEXT("An Ancient Puzzle")))
		.OnClose_Lambda([this]() { RequestClose(); })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1.f).VAlign(VAlign_Center).HAlign(HAlign_Center)
			[
				AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Prompt), 14.f, C.Text, false, 1, 370.f)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 8.f, 0.f, 4.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(6.f, 0.f))
				[
					SNew(SAbyssButton, Context)
					.Text(FText::AsCultureInvariant(Solution))
					.Kind(EAbyssButtonKind::Primary)
					.FontPx(bTouch ? 16.f : 13.f)
					.Width(200.f)
					.Height(bTouch ? 50.f : 34.f)
					.IsEnabled(!Ctx->IsHeroDying())
					.OnClicked_Lambda([Context, Prop]() { Context->Submit(abyss::CmdPuzzleAnswer{ Prop, abyss::kPuzzleChoiceSolve }); })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(6.f, 0.f))
				[
					SNew(SAbyssButton, Context)
					.Text(Ctx->LocOr("ui.dialogue.leave", TEXT("Leave")))
					.Kind(EAbyssButtonKind::Ghost)
					.FontPx(bTouch ? 16.f : 13.f)
					.Width(140.f)
					.Height(bTouch ? 50.f : 34.f)
					.OnClicked_Lambda([Context, Prop]() { Context->Submit(abyss::CmdPuzzleAnswer{ Prop, abyss::kPuzzleChoiceLeave }); })
				]
			]
		]);
}
