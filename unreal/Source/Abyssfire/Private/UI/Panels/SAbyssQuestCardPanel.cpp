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
	if (Data == nullptr || Snap.questCard == nullptr || !Snap.questCard->open || Snap.questCard->entries.empty() || Snap.quests == nullptr)
	{
		return;
	}
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const abyss::QuestCardState& Card = *Snap.questCard;
	const bool bTouch = Ctx->IsTouch();
	const bool bDying = Ctx->IsHeroDying();
	NpcId = Card.npcId;
	const int32 EntryCount = static_cast<int32>(Card.entries.size());
	EntryIndex = FMath::Clamp(EntryIndex, 0, EntryCount - 1);
	const abyss::QuestCardEntry& Entry = Card.entries[static_cast<size_t>(EntryIndex)];
	if (Entry.quest == nullptr)
	{
		return;
	}
	const abyss::QuestDef& Quest = *Entry.quest;
	if (Quest.id != ShownQuest)
	{
		ShownQuest = Quest.id;
		Choice = Entry.choices.empty() ? -1 : 0;  // the index resets the choice selection
	}
	const bool bMain = Quest.category == abyss::QuestCategory::Main;
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
		AddText(FString::Printf(TEXT("%s %s %s  %d/%d"), bDone ? TEXT("\x2713") : TEXT("\x25CB"), *Ctx->ObjectiveTypeLabel(Objective, Quest.type),
			*Ctx->ObjectiveTargetLabel(Quest, static_cast<int32>(Index)), Current, Objective.required), 12.f, bDone ? C.Good : C.Text, false, 2.f);
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
			TSharedRef<SHorizontalBox> Choices = SNew(SHorizontalBox);
			for (int32 Index = 0; Index < static_cast<int32>(Entry.choices.size()); ++Index)
			{
				FAbyssSlotItem SlotEntry;
				SlotEntry.Item = Entry.choices[static_cast<size_t>(Index)];
				SlotEntry.bSelected = Index == Choice;
				const std::string SlotId = static_cast<size_t>(Index) < Rewards.choices.size()
					? std::string(abyss::EnumName(Rewards.choices[static_cast<size_t>(Index)])) : std::string();
				Choices->AddSlot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(SAbyssItemSlot, Context)
						.SlotSize(bTouch ? 56.f : 44.f)
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
						})
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 2.f, 0.f, 0.f))
					[
						AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Ctx->NameOr("sys.questCard.choice." + SlotId, SlotId)), 10.f,
							Index == Choice ? C.GoldBright : C.Muted, Index == Choice, 1)
					]
				];
			}
			ContentH += bTouch ? 80.0 : 66.0;
			Lines->AddSlot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f))[Choices];
		}
		else if (!Entry.turnIn)
		{
			TArray<FString> SlotNames;
			for (const abyss::RewardSlot Slot : Rewards.choices)
			{
				const std::string SlotId(abyss::EnumName(Slot));
				SlotNames.Add(Ctx->NameOr("sys.questCard.choice." + SlotId, SlotId));
			}
			AddText(Ctx->LocArgsOrStr("ui.questCard.choicePreview", TEXT("Reward choice: {slots}"),
				{ FAbyssUiContext::Arg("slots", FString::Join(SlotNames, TEXT(" / "))) }), 11.f, C.Info, false, 4.f);
		}
	}

	// buttons: Accept / Turn in, View story (NPC with a dialogue tree)
	const abyss::NpcDef* Npc = Data->FindNpc(NpcId);
	const bool bHasTree = Npc != nullptr && !Npc->dialogueTreeId.empty();
	const std::string QuestId = Quest.id;
	const std::string Giver = NpcId;
	const bool bTurnIn = Entry.turnIn;
	const int32 SelectedChoice = FMath::Max(0, Choice);
	TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
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
