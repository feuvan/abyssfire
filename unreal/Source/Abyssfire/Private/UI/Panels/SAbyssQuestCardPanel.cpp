#include "UI/Panels/SAbyssQuestCardPanel.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include <string>
#include <vector>

#include "abyss/data/DataStore.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/quests/QuestWorld.h"

#include "Framework/AbyssText.h"
#include "UI/AbyssUiStyle.h"
#include "UI/Panels/AbyssQuestCardRules.h"
#include "UI/Widgets/AbyssUiWidgets.h"

namespace
{
	/** Rough wrapped height of a line of text (the card sizes itself to its content). */
	double AbyssQuestCard_TextHeight(const FString& Text, const FSlateFontInfo& Font, double WrapWidth)
	{
		if (Text.IsEmpty())
		{
			return 0.0;
		}
		TArray<FString> Paragraphs;
		Text.ParseIntoArray(Paragraphs, TEXT("\n"), false);
		const double LineHeight = FAbyssPainter::Measure(TEXT("Ag"), Font).Y * 1.15;
		double Lines = 0.0;
		for (const FString& Paragraph : Paragraphs)
		{
			Lines += FMath::Max(1.0, FMath::CeilToDouble(FAbyssPainter::Measure(Paragraph, Font).X / FMath::Max(1.0, WrapWidth)));
		}
		return FMath::Max(1.0, Lines) * LineHeight;
	}
}

void SAbyssQuestCardPanel::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const std::string& InNpcId)
{
	SAbyssPanelBase::Construct(SAbyssPanelBase::FArguments(), InContext);
	NpcId = InNpcId;
	InContext->PlaySound(abyss::SfxId::Click);  // 5.3 audio: click on open
	SetPanelContent(SAssignNew(Content, SBox));
}

void SAbyssQuestCardPanel::Refresh(const abyss::Snapshot& Snap)
{
	const abyss::DataStore* Data = Ctx->GetData();
	if (Data == nullptr || Snap.questCard == nullptr || !Snap.questCard->open || Snap.quests == nullptr)
	{
		return;
	}
	namespace QC = AbyssQuestCardRules;
	const abyss::QuestCardState& Card = *Snap.questCard;
	// U5 (AbyssQuestCardRules.h): the page's entry, or the K1 progress lines alone (a progress-only card used to draw
	// nothing: the core opened its modal and the panel returned on the empty entry list).
	const QC::FCardModel Model = QC::BuildCardModel(Card, EntryIndex, Data);
	if (!Model.bVisible)
	{
		return;
	}
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const bool bTouch = Ctx->IsTouch();
	const bool bDying = Ctx->IsHeroDying();
	NpcId = Card.npcId;
	const int32 EntryCount = Model.EntryCount;
	EntryIndex = Model.EntryIndex;
	const abyss::QuestCardEntry* const EntryPtr = Model.Entry;
	const abyss::QuestDef* const QuestPtr = EntryPtr != nullptr ? EntryPtr->quest : nullptr;
	if (QuestPtr == nullptr)
	{
		ShownQuest.clear();
		Choice = -1;
	}
	else if (QuestPtr->id != ShownQuest)
	{
		ShownQuest = QuestPtr->id;
		Choice = EntryPtr->choices.empty() ? -1 : 0;  // the index resets the choice selection
	}
	const bool bMain = QuestPtr != nullptr ? QuestPtr->category == abyss::QuestCategory::Main
		: (!Model.Progress.empty() && Model.Progress.front().bMain);
	const float Wrap = 380.f;
	double ContentH = 0.0;
	TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox);
	const auto AddText = [&Lines, &Context, &ContentH, Wrap](const FString& Text, float Px, const FLinearColor& Color, bool bBold, float Top,
		bool bSerif = false)
	{
		if (Text.IsEmpty())
		{
			return;
		}
		const FSlateFontInfo Font = bSerif ? Context->Style().Serif(Px, bBold, 1) : Context->Style().Body(Px, bBold, 1);
		ContentH += Top + AbyssQuestCard_TextHeight(Text, Font, Wrap);
		Lines->AddSlot().AutoHeight().Padding(FMargin(0.f, Top, 0.f, 0.f))
		[
			SNew(STextBlock)
			.Text(FText::AsCultureInvariant(Text))
			.Font(Font)
			.ColorAndOpacity(FSlateColor(Color))
			.WrapTextAt(Wrap)
		];
	};
	// caption texts of a reward choice: the narrative label (when its key resolves) and the slot name
	const auto LabelText = [this](const std::string& Key)
	{
		return !Key.empty() && Ctx->HasKey(Key) ? Ctx->LocStr(Key) : FString();
	};
	const auto SlotName = [this](const std::string& SlotId)
	{
		return SlotId.empty() ? FString() : Ctx->NameOr(QC::SlotCaptionKey(SlotId), SlotId);
	};

	// navigation < n/N > when the NPC has several entries
	if (EntryCount > 1)
	{
		ContentH += 30.0;
		Lines->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.f, 0.f, 0.f, 4.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SAbyssButton, Context)
					.Text(FText::AsCultureInvariant(TEXT("\x25C0")))
					.Kind(EAbyssButtonKind::Ghost).FontPx(12.f).Width(bTouch ? 48.f : 32.f).Height(bTouch ? 40.f : 24.f)
					.IsEnabled(EntryIndex > 0)
					.OnClicked_Lambda([this]()
					{
						--EntryIndex;
						MarkDirty();
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(12.f, 0.f))
				[
					AbyssUi::Label(*Ctx, FText::AsCultureInvariant(FString::Printf(TEXT("%d/%d"), EntryIndex + 1, EntryCount)), 12.f, C.TextSoft, true, 1)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SAbyssButton, Context)
					.Text(FText::AsCultureInvariant(TEXT("\x25B6")))
					.Kind(EAbyssButtonKind::Ghost).FontPx(12.f).Width(bTouch ? 48.f : 32.f).Height(bTouch ? 40.f : 24.f)
					.IsEnabled(EntryIndex + 1 < EntryCount)
					.OnClicked_Lambda([this]()
					{
						++EntryIndex;
						MarkDirty();
					})
				]
			];
	}

	if (QuestPtr != nullptr)
	{
		const abyss::QuestCardEntry& Entry = *EntryPtr;
		const abyss::QuestDef& Quest = *QuestPtr;

		// name, badge, giver, type
		AddText(Ctx->QuestName(Quest), 17.f, bMain ? FAbyssUiStyle::Rgb(0xffd98a) : C.Parchment, true, 0.f);
		const std::string TypeId(abyss::EnumName(Quest.type));
		AddText(FString::Printf(TEXT("%s  %s  \x00B7  %s"),
			*(bMain ? Ctx->LocOrStr("ui.questCard.mainBadge", TEXT("[Main]")) : Ctx->LocOrStr("ui.questCard.sideBadge", TEXT("[Side]"))),
			*Ctx->NpcName(NpcId), *Ctx->NameOr("sys.quest.type." + TypeId, TypeId)), 11.f, C.Muted, false, 2.f);

		// the NPC's speech: offer line on an accept card, complete line on a turn-in card (in quotes when the key exists)
		const std::string SpeechKey = Entry.turnIn ? (Quest.completeKey.empty() ? "data.quest." + Quest.id + ".complete" : Quest.completeKey)
			: (Quest.offerKey.empty() ? "data.quest." + Quest.id + ".offer" : Quest.offerKey);
		const bool bSpeech = Ctx->HasKey(SpeechKey);
		if (bSpeech)
		{
			AddText(FString::Printf(TEXT("\x201C%s\x201D"), *Ctx->LocStr(SpeechKey)), 13.f, FAbyssUiStyle::Rgb(0xf0dcc8), false, 8.f, true);
		}
		if (!(Entry.turnIn && bSpeech))
		{
			AddText(Ctx->NameOr(Quest.descKey.empty() ? "data.quest." + Quest.id + ".desc" : Quest.descKey, Quest.description), 12.f, C.TextSoft, false, 8.f);
		}

		// objectives
		const abyss::QuestProgress* Progress = Snap.quests->Progress(Quest.id);
		AddText(Ctx->LocOrStr("ui.questCard.objectives", TEXT("Objectives:")), 12.f, C.Heading, true, 8.f);
		for (size_t Index = 0; Index < Quest.objectives.size(); ++Index)
		{
			const abyss::QuestObjectiveDef& Objective = Quest.objectives[Index];
			const int32 Current = Progress != nullptr && Index < Progress->objectives.size()
				? FMath::Min(Progress->objectives[Index], Objective.required) : 0;
			const bool bDone = Current >= Objective.required;
			const FString ObjectiveText = FString::Printf(TEXT("%s %s %s  %d/%d"), bDone ? TEXT("\x2713") : TEXT("\x25CB"),
				*Ctx->ObjectiveTypeLabel(Objective, Quest.type), *Ctx->ObjectiveTargetLabel(Quest, static_cast<int32>(Index)), Current,
				Objective.required);
			const FLinearColor ObjectiveColor = bDone ? C.Good : C.Text;
			const FSlateBrush* KindIcon = Ctx->QuestObjectiveIcon(Objective);
			if (KindIcon == nullptr)
			{
				AddText(ObjectiveText, 12.f, ObjectiveColor, false, 2.f);
				continue;
			}
			// U11: a collect objective shows its quest-item icon (T_UI_QuestItem_<kind>, 20 px) before the line.
			const FSlateFontInfo ObjectiveFont = Context->Style().Body(12.f, false, 1);
			const float TextWrap = Wrap - 24.f;
			ContentH += 2.0 + FMath::Max(20.0, AbyssQuestCard_TextHeight(ObjectiveText, ObjectiveFont, TextWrap));
			Lines->AddSlot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 0.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 4.f, 0.f))
				[
					AbyssUi::Icon(Context, KindIcon, 20.f, bDone ? FLinearColor(1.f, 1.f, 1.f, 0.75f) : FLinearColor::White)
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::AsCultureInvariant(ObjectiveText))
					.Font(ObjectiveFont)
					.ColorAndOpacity(FSlateColor(ObjectiveColor))
					.WrapTextAt(TextWrap)
				]
			];
		}

		// reward summary (embers are not listed)
		const abyss::QuestRewardDef& Rewards = Quest.rewards;
		TArray<FString> Parts;
		if (Rewards.exp > 0)
		{
			Parts.Add(Ctx->LocArgsOrStr("sys.questCard.rewardExp", TEXT("{exp} EXP"), { FAbyssUiContext::Arg("exp", Rewards.exp) }));
		}
		if (Rewards.gold > 0)
		{
			Parts.Add(Ctx->LocArgsOrStr("sys.questCard.rewardGold", TEXT("{gold} Gold"), { FAbyssUiContext::Arg("gold", Rewards.gold) }));
		}
		if (!Rewards.items.empty())
		{
			Parts.Add(Ctx->LocArgsOrStr("sys.questCard.rewardItems", TEXT("{count} Items"), { FAbyssUiContext::Arg("count", static_cast<int64>(Rewards.items.size())) }));
		}
		if (!Rewards.choices.empty())
		{
			Parts.Add(Ctx->LocArgsOrStr("sys.questCard.rewardChoice", TEXT("pick 1 of {count} gear"),
				{ FAbyssUiContext::Arg("count", static_cast<int64>(Rewards.choices.size())) }));
		}
		if (!Rewards.petReward.empty())
		{
			Parts.Add(Ctx->LocOrStr("sys.questCard.rewardPet", TEXT("Pet")));
		}
		if (Parts.Num() > 0)
		{
			AddText(Ctx->LocOrStr("ui.questCard.rewards", TEXT("Rewards:")), 12.f, C.Heading, true, 8.f);
			AddText(FString::Join(Parts, TEXT("  \x00B7  ")), 12.f, FAbyssUiStyle::Rgb(0xffd35a), false, 2.f);
		}
		// fixed reward items
		if (!Rewards.items.empty())
		{
			TSharedRef<SHorizontalBox> Icons = SNew(SHorizontalBox);
			for (const std::string& BaseId : Rewards.items)
			{
				abyss::ItemInstance Item;
				Item.baseId = BaseId;
				Item.quality = abyss::ItemQuality::Normal;
				FAbyssSlotItem SlotEntry;
				SlotEntry.Item = Item;
				SlotEntry.bCompare = false;
				Icons->AddSlot().AutoWidth().Padding(FMargin(0.f, 0.f, 4.f, 0.f))
				[
					SNew(SAbyssItemSlot, Context).SlotSize(34.f).Entry(SlotEntry)
				];
			}
			ContentH += 40.0;
			Lines->AddSlot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f))[Icons];
		}
		// pick-one gear: generated choices on a turn-in card, the slot preview on an accept card
		if (!Rewards.choices.empty())
		{
			if (Entry.turnIn && !Entry.choices.empty())
			{
				AddText(Ctx->LocOrStr("ui.questCard.chooseReward", TEXT("Choose one reward")), 12.f, C.Heading, true, 8.f);
				// K1 labelled choices: each item carries the label of the slot it was generated for (primary caption, the
				// narrative choice) over its slot name; the columns share the card width and wrap their captions. An
				// unlabelled card keeps today's layout (slot name under each item).
				bool bLabelled = false;
				for (const QC::FChoiceCaption& Caption : Model.Choices)
				{
					bLabelled = bLabelled || !LabelText(Caption.LabelKey).IsEmpty();
				}
				const int32 ChoiceCount = static_cast<int32>(Entry.choices.size());
				const float SlotSize = bTouch ? 56.f : 44.f;
				const float ColumnW = QC::ChoiceColumnWidth(ChoiceCount, Wrap, SlotSize, 10.f, 150.f);
				const FSlateFontInfo SecondaryFont = Context->Style().Body(bTouch ? 11.f : 9.f, false, 1);
				double RowH = 0.0;
				TSharedRef<SHorizontalBox> Choices = SNew(SHorizontalBox);
				for (int32 Index = 0; Index < ChoiceCount; ++Index)
				{
					const QC::FChoiceCaption Caption = Index < static_cast<int32>(Model.Choices.size())
						? Model.Choices[static_cast<size_t>(Index)] : QC::FChoiceCaption();
					const bool bSelected = Index == Choice;
					FAbyssSlotItem SlotEntry;
					SlotEntry.Item = Entry.choices[static_cast<size_t>(Index)];
					SlotEntry.bSelected = bSelected;
					const TSharedRef<SWidget> ItemSlot = SNew(SAbyssItemSlot, Context)
						.SlotSize(SlotSize)
						.Entry(SlotEntry)
						.OnClicked_Lambda([this, Index, Context](const FVector2D& Position)
						{
							Choice = Index;
							MarkDirty();
							if (Context->IsTouch())
							{
								// touch: the card of the chosen item (hover tooltips do not exist on touch)
								if (const abyss::Snapshot* Current = Context->GetSnapshot())
								{
									if (Current->questCard != nullptr && EntryIndex < static_cast<int32>(Current->questCard->entries.size()))
									{
										const abyss::QuestCardEntry& Live = Current->questCard->entries[static_cast<size_t>(EntryIndex)];
										if (Index < static_cast<int32>(Live.choices.size()))
										{
											AbyssItemTooltip::FOptions Options;
											Context->ShowPopup(AbyssItemTooltip::MakeCard(Context, Live.choices[static_cast<size_t>(Index)], Options), Position);
										}
									}
								}
							}
						});
					const FString LabelStr = LabelText(Caption.LabelKey);
					const FString SlotStr = SlotName(Caption.SlotId);
					if (!bLabelled)
					{
						Choices->AddSlot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
							[
								ItemSlot
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 2.f, 0.f, 0.f))
							[
								AbyssUi::Label(*Ctx, FText::AsCultureInvariant(SlotStr), 10.f, bSelected ? C.GoldBright : C.Muted, bSelected, 1)
							]
						];
						continue;
					}
					const FString Primary = LabelStr.IsEmpty() ? SlotStr : LabelStr;
					const FString Secondary = LabelStr.IsEmpty() ? FString() : SlotStr;
					const FSlateFontInfo PrimaryFont = Context->Style().Body(bTouch ? 13.f : 11.f, bSelected, 1);
					double ColumnH = SlotSize + 2.0 + AbyssQuestCard_TextHeight(Primary, PrimaryFont, ColumnW);
					TSharedRef<SVerticalBox> Column = SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
						[
							ItemSlot
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 2.f, 0.f, 0.f))
						[
							SNew(STextBlock)
							.Text(FText::AsCultureInvariant(Primary))
							.Font(PrimaryFont)
							.ColorAndOpacity(FSlateColor(bSelected ? C.GoldBright : C.Text))
							.Justification(ETextJustify::Center)
							.WrapTextAt(ColumnW)
						];
					if (!Secondary.IsEmpty())
					{
						ColumnH += 1.0 + AbyssQuestCard_TextHeight(Secondary, SecondaryFont, ColumnW);
						Column->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 1.f, 0.f, 0.f))
						[
							SNew(STextBlock)
							.Text(FText::AsCultureInvariant(Secondary))
							.Font(SecondaryFont)
							.ColorAndOpacity(FSlateColor(C.Muted))
							.Justification(ETextJustify::Center)
							.WrapTextAt(ColumnW)
						];
					}
					RowH = FMath::Max(RowH, ColumnH);
					Choices->AddSlot().AutoWidth().Padding(FMargin(0.f, 0.f, Index + 1 < ChoiceCount ? 10.f : 0.f, 0.f))
					[
						SNew(SBox)
						.WidthOverride(ColumnW)
						[
							Column
						]
					];
				}
				ContentH += bLabelled ? 4.0 + RowH : (bTouch ? 80.0 : 66.0);
				Lines->AddSlot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f))[Choices];
			}
			else if (!Entry.turnIn)
			{
				// slot names, each after its narrative label when the quest labels its choices ("label \x00B7 slot")
				TArray<FString> SlotNames;
				for (const QC::FChoiceCaption& Caption : Model.Preview)
				{
					const FString LabelStr = LabelText(Caption.LabelKey);
					const FString SlotStr = SlotName(Caption.SlotId);
					SlotNames.Add(LabelStr.IsEmpty() ? SlotStr : FString::Printf(TEXT("%s \x00B7 %s"), *LabelStr, *SlotStr));
				}
				AddText(Ctx->LocArgsOrStr("ui.questCard.choicePreview", TEXT("Reward choice: {slots}"),
					{ FAbyssUiContext::Arg("slots", FString::Join(SlotNames, TEXT(" / "))) }), 11.f, C.Info, false, 4.f);
			}
		}
	}

	// K1 progress lines: the giver's line for each of its active quests (quest name, then the line in the speech style).
	// Alone on a progress-only card; under an "Active" rule after the entry otherwise.
	if (!Model.Progress.empty())
	{
		const bool bAlone = QuestPtr == nullptr;
		if (!bAlone)
		{
			ContentH += 12.0 + 18.0;
			Lines->AddSlot().AutoHeight().Padding(FMargin(0.f, 12.f, 0.f, 0.f))
			[
				AbyssUi::SectionHeader(Context, Ctx->LocOr("ui.questLog.tab.active", TEXT("Active")), Wrap, 12.f)
			];
		}
		for (size_t Index = 0; Index < Model.Progress.size(); ++Index)
		{
			const QC::FProgressLine& Line = Model.Progress[Index];
			const float NameTop = Index == 0 ? (bAlone ? 0.f : 4.f) : (bAlone ? 14.f : 10.f);
			AddText(Ctx->QuestName(*Line.Quest), bAlone ? 15.f : 12.f, Line.bMain ? FAbyssUiStyle::Rgb(0xffd98a) : C.Parchment, true, NameTop);
			const FString Said = LabelText(Line.LineKey);
			if (!Said.IsEmpty())
			{
				AddText(FString::Printf(TEXT("\x201C%s\x201D"), *Said), 13.f, FAbyssUiStyle::Rgb(0xf0dcc8), false, 4.f, true);
			}
		}
	}

	// buttons: Accept / Turn in (entry), View story (NPC with a dialogue tree), Leave (progress-only card)
	const abyss::NpcDef* Npc = Data->FindNpc(NpcId);
	const bool bHasTree = Npc != nullptr && !Npc->dialogueTreeId.empty();
	const std::string Giver = NpcId;
	TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
	if (QuestPtr != nullptr)
	{
		const std::string QuestId = QuestPtr->id;
		const bool bTurnIn = Model.Action == QC::ECardAction::TurnIn;
		const int32 SelectedChoice = FMath::Max(0, Choice);
		Buttons->AddSlot()
			.AutoWidth()
			.Padding(FMargin(6.f, 0.f))
			[
				SNew(SAbyssButton, Context)
				.Text(bTurnIn ? Ctx->LocOr("ui.questCard.turnIn", TEXT("Turn In")) : Ctx->LocOr("ui.questCard.accept", TEXT("Accept")))
				.Kind(bTurnIn ? EAbyssButtonKind::Success : EAbyssButtonKind::Primary)
				.FontPx(bTouch ? 17.f : 14.f)
				.Width(bTouch ? 170.f : 140.f)
				.Height(bTouch ? 50.f : 34.f)
				.IsEnabled(!bDying)
				.OnClicked_Lambda([Context, QuestId, Giver, bTurnIn, SelectedChoice]()
				{
					if (bTurnIn)
					{
						// T17: the host arms the chain offer only once the core reports the turn-in (not on a rejected one).
						if (IAbyssUiHost* Host = Context->GetHost())
						{
							Host->RequestQuestChainOffer(QuestId, Giver);
						}
						Context->Submit(abyss::CmdQuestTurnIn{ QuestId, SelectedChoice });
					}
					else
					{
						Context->Submit(abyss::CmdQuestAccept{ QuestId });
					}
				})
			];
	}
	if (bHasTree)
	{
		Buttons->AddSlot()
			.AutoWidth()
			.Padding(FMargin(6.f, 0.f))
			[
				SNew(SAbyssButton, Context)
				.Text(Ctx->LocOr("ui.questCard.viewStory", TEXT("View Story")))
				.Kind(EAbyssButtonKind::Ghost)
				.FontPx(bTouch ? 15.f : 13.f)
				.Width(bTouch ? 150.f : 120.f)
				.Height(bTouch ? 50.f : 34.f)
				.OnClicked_Lambda([Context, Giver]() { Context->Submit(abyss::CmdDialogueOpenTree{ Giver }); })
			];
	}
	if (QuestPtr == nullptr)
	{
		// a progress-only card has no quest action: Leave closes it (same as the frame's close button)
		Buttons->AddSlot()
			.AutoWidth()
			.Padding(FMargin(6.f, 0.f))
			[
				SNew(SAbyssButton, Context)
				.Text(Ctx->LocOr("ui.dialogue.leave", TEXT("Leave")))
				.Kind(EAbyssButtonKind::Secondary)
				.FontPx(bTouch ? 17.f : 14.f)
				.Width(bTouch ? 170.f : 140.f)
				.Height(bTouch ? 50.f : 34.f)
				.OnClicked_Lambda([this]() { RequestClose(); })
			];
	}

	const double MaxBody = 560.0;
	const double BodyH = FMath::Min(ContentH + 10.0, MaxBody);
	Height = FMath::Clamp(36.0 + 6.0 + BodyH + 14.0 + (bTouch ? 50.0 : 34.0) + 16.0, 240.0, 700.0);
	TSharedRef<SScrollBox> Scroll = AbyssUi::ScrollBox(*Ctx);
	Scroll->AddSlot()[Lines];
	Content->SetContent(
		SNew(SAbyssPanelFrame, Context)
		.Size(GetDesignSize())
		.Accent(bMain ? FAbyssUiStyle::Rgb(0xffd98a) : C.Gold)
		.Title(FText::AsCultureInvariant(Ctx->NpcName(NpcId)))
		.OnClose_Lambda([this]() { RequestClose(); })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				Scroll
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 10.f, 0.f, 2.f))
			[
				Buttons
			]
		]);
}
