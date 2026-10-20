// Quest card view model (dark remake 1C U5, Wave 1 parts; implementation-plan.md 4 U5, DECISIONS DR14): what
// SAbyssQuestCardPanel draws for a core QuestCardState, decided without the engine so it is unit-tested
// (PresentationTests/tests/questcard_test.cpp).
//
// - Progress lines (K1, QuestCardState::progress): the giver's line for each of its active quests that has a
//   progressKey. The card opens for these alone (no offer, no turn-in): a progress-only card shows the lines, View Story
//   when the NPC has a dialogue tree, and Leave. On a card with entries the lines follow the entry. A line whose quest
//   is also an entry of the card is dropped (the core never sends both; the entry wins).
// - Labelled reward choices (K1, QuestCardEntry::choiceLabels): the turn-in card's pick-one items keep the label of the
//   slot each was generated for (the labels follow the generated items, so a slot that yielded nothing drops its label).
//   The label is the primary caption (the narrative choice), the slot name (sys.questCard.choice.<slot>) the secondary
//   one; an unlabelled choice keeps today's single slot caption. The accept card's preview line reads
//   rewards.choiceLabels slot by slot.
// - The slot of a generated item: the item index when every slot yielded an item; otherwise (a slot's pool was empty)
//   the slots are matched in order, by the item's label, else by the pool its base belongs to (LootGen PickRewardBase),
//   so the slot caption follows the item too (today's card took rewards.choices[i] for item i).
//
// Std + AbyssCore public headers only (no CoreMinimal, no UE macros): the module includes it after CoreMinimal.h, the
// macro shim (PresentationTests/shim) compiles it under the UE / Windows / Apple macros.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/data/DataStore.h"
#include "abyss/data/ItemData.h"
#include "abyss/data/QuestData.h"
#include "abyss/items/Item.h"
#include "abyss/quests/QuestWorld.h"

namespace AbyssQuestCardRules
{
	/** The card's primary button. */
	enum class ECardAction : uint8_t
	{
		None,    // progress-only card: View Story (tree) + Leave
		Accept,
		TurnIn,
	};

	/** One progress line: the quest it belongs to and the giver's line (i18n key). */
	struct FProgressLine
	{
		const abyss::QuestDef* Quest = nullptr;
		std::string LineKey;
		bool bMain = false;
	};

	/** Caption of one reward choice (turn-in item or accept-card slot). */
	struct FChoiceCaption
	{
		std::string LabelKey;  // narrative label (i18n key); "" = unlabelled
		std::string SlotId;    // RewardSlot name ("weapon", ...); "" = the item's slot could not be told
		int32_t SlotIndex = -1;  // index into the quest's rewards.choices; -1 = unknown
	};

	/** Caption keys of a choice in drawing order: the primary line, then the secondary one ("" = none). */
	struct FCaptionKeys
	{
		std::string Primary;
		std::string Secondary;
	};

	struct FCardModel
	{
		bool bVisible = false;       // anything to draw (an open card with an entry or a progress line)
		bool bProgressOnly = false;  // no entry: only progress lines
		int32_t EntryCount = 0;
		int32_t EntryIndex = 0;      // clamped to [0, EntryCount - 1] (0 when there is no entry)
		const abyss::QuestCardEntry* Entry = nullptr;  // nullptr on a progress-only card
		ECardAction Action = ECardAction::None;
		std::vector<FProgressLine> Progress;
		std::vector<FChoiceCaption> Choices;  // turn-in card: one per Entry->choices item
		std::vector<FChoiceCaption> Preview;  // accept card: one per rewards.choices slot
	};

	inline constexpr std::string_view kSlotCaptionPrefix = "sys.questCard.choice.";

	/** Slot caption key of a RewardSlot name ("" for an unknown slot). */
	inline std::string SlotCaptionKey(const std::string& SlotId)
	{
		return SlotId.empty() ? std::string() : std::string(kSlotCaptionPrefix) + SlotId;
	}

	/** Primary / secondary caption keys: label then slot when labelled, the slot alone otherwise. */
	inline FCaptionKeys CaptionKeys(const FChoiceCaption& Caption)
	{
		FCaptionKeys Keys;
		if (!Caption.LabelKey.empty())
		{
			Keys.Primary = Caption.LabelKey;
			Keys.Secondary = SlotCaptionKey(Caption.SlotId);
		}
		else
		{
			Keys.Primary = SlotCaptionKey(Caption.SlotId);
		}
		return Keys;
	}

	/** Whether a base can come out of a reward slot's pool (LootGen PickRewardBase's pools, any class). */
	inline bool SlotCanYield(abyss::RewardSlot Slot, const abyss::ItemBaseDef& Base)
	{
		switch (Slot)
		{
		case abyss::RewardSlot::Weapon:
			return Base.group == "weapons" && Base.hasSlot && Base.slot == abyss::EquipSlot::Weapon;
		case abyss::RewardSlot::Offhand:
			// shield classes: an off-hand from the weapons group; the others: any accessory
			return (Base.group == "weapons" && Base.hasSlot && Base.slot == abyss::EquipSlot::Offhand) || Base.group == "accessories";
		case abyss::RewardSlot::Jewelry:
			return Base.group == "accessories";
		case abyss::RewardSlot::Armor:
			return Base.group == "armors" && Base.hasSlot && Base.slot == abyss::EquipSlot::Armor;
		case abyss::RewardSlot::Helmet:
			return Base.group == "armors" && Base.hasSlot && Base.slot == abyss::EquipSlot::Helmet;
		case abyss::RewardSlot::Gloves:
			return Base.group == "armors" && Base.hasSlot && Base.slot == abyss::EquipSlot::Gloves;
		case abyss::RewardSlot::Boots:
			return Base.group == "armors" && Base.hasSlot && Base.slot == abyss::EquipSlot::Boots;
		case abyss::RewardSlot::Belt:
			return Base.group == "armors" && Base.hasSlot && Base.slot == abyss::EquipSlot::Belt;
		}
		return false;
	}

	/**
	 * The rewards.choices slot each generated item came from (-1 = unknown). Every slot yielded an item when the counts
	 * match (the item index). Otherwise the slots are walked in order (the generator keeps it): an item takes the next
	 * slot carrying its label (labels aligned with the items), else the next slot whose pool holds its base; an item
	 * that matches nothing gets -1 and does not move the walk.
	 */
	inline std::vector<int32_t> ChoiceSlotIndices(const abyss::QuestRewardDef& Rewards, const std::vector<abyss::ItemInstance>& Items,
		const std::vector<std::string>& ItemLabels, const abyss::DataStore* Data)
	{
		std::vector<int32_t> Out(Items.size(), -1);
		const size_t SlotCount = Rewards.choices.size();
		if (Items.size() == SlotCount)
		{
			for (size_t Index = 0; Index < Items.size(); ++Index)
			{
				Out[Index] = static_cast<int32_t>(Index);
			}
			return Out;
		}
		const bool bLabelsAligned = ItemLabels.size() == Items.size() && Rewards.choiceLabels.size() == SlotCount;
		size_t Next = 0;
		for (size_t Index = 0; Index < Items.size() && Next < SlotCount; ++Index)
		{
			int32_t Found = -1;
			if (bLabelsAligned && !ItemLabels[Index].empty())
			{
				for (size_t Slot = Next; Slot < SlotCount && Found < 0; ++Slot)
				{
					if (Rewards.choiceLabels[Slot] == ItemLabels[Index])
					{
						Found = static_cast<int32_t>(Slot);
					}
				}
			}
			const abyss::ItemBaseDef* Base = Data != nullptr ? Data->FindItemBase(Items[Index].baseId) : nullptr;
			if (Found < 0 && Base != nullptr)
			{
				for (size_t Slot = Next; Slot < SlotCount && Found < 0; ++Slot)
				{
					if (SlotCanYield(Rewards.choices[Slot], *Base))
					{
						Found = static_cast<int32_t>(Slot);
					}
				}
			}
			if (Found >= 0)
			{
				Out[Index] = Found;
				Next = static_cast<size_t>(Found) + 1;
			}
		}
		return Out;
	}

	/**
	 * Captions of a turn-in card's generated items. Label: the entry's label of that item when the entry's labels are
	 * aligned with its items (the core's contract); otherwise (a length mismatch) the label of the item's slot when the
	 * slot is known, else none. Slot: the item's slot (ChoiceSlotIndices).
	 */
	inline std::vector<FChoiceCaption> TurnInCaptions(const abyss::QuestRewardDef& Rewards, const abyss::QuestCardEntry& Entry,
		const abyss::DataStore* Data)
	{
		const std::vector<int32_t> Slots = ChoiceSlotIndices(Rewards, Entry.choices, Entry.choiceLabels, Data);
		const bool bAligned = Entry.choiceLabels.size() == Entry.choices.size();
		std::vector<FChoiceCaption> Out;
		Out.reserve(Entry.choices.size());
		for (size_t Index = 0; Index < Entry.choices.size(); ++Index)
		{
			FChoiceCaption Caption;
			Caption.SlotIndex = Slots[Index];
			const bool bSlotKnown = Caption.SlotIndex >= 0 && static_cast<size_t>(Caption.SlotIndex) < Rewards.choices.size();
			if (bSlotKnown)
			{
				Caption.SlotId = std::string(abyss::EnumName(Rewards.choices[static_cast<size_t>(Caption.SlotIndex)]));
			}
			if (bAligned)
			{
				Caption.LabelKey = Entry.choiceLabels[Index];
			}
			else if (bSlotKnown && Rewards.choiceLabels.size() == Rewards.choices.size())
			{
				Caption.LabelKey = Rewards.choiceLabels[static_cast<size_t>(Caption.SlotIndex)];
			}
			Out.push_back(std::move(Caption));
		}
		return Out;
	}

	/** Captions of an accept card's preview: every rewards.choices slot with its label (when the quest has labels). */
	inline std::vector<FChoiceCaption> PreviewCaptions(const abyss::QuestRewardDef& Rewards)
	{
		const bool bLabelled = Rewards.choiceLabels.size() == Rewards.choices.size();
		std::vector<FChoiceCaption> Out;
		Out.reserve(Rewards.choices.size());
		for (size_t Index = 0; Index < Rewards.choices.size(); ++Index)
		{
			FChoiceCaption Caption;
			Caption.SlotIndex = static_cast<int32_t>(Index);
			Caption.SlotId = std::string(abyss::EnumName(Rewards.choices[Index]));
			if (bLabelled)
			{
				Caption.LabelKey = Rewards.choiceLabels[Index];
			}
			Out.push_back(std::move(Caption));
		}
		return Out;
	}

	/** The card's progress lines in the core's order: known quests with a key, not already an entry of the card. */
	inline std::vector<FProgressLine> ProgressLines(const abyss::QuestCardState& Card)
	{
		std::vector<FProgressLine> Out;
		for (const abyss::QuestCardProgress& Line : Card.progress)
		{
			if (Line.quest == nullptr || Line.progressKey.empty())
			{
				continue;
			}
			bool bIsEntry = false;
			for (const abyss::QuestCardEntry& Entry : Card.entries)
			{
				bIsEntry = bIsEntry || (Entry.quest != nullptr && Entry.quest->id == Line.quest->id);
			}
			bool bSeen = false;
			for (const FProgressLine& Kept : Out)
			{
				bSeen = bSeen || Kept.Quest->id == Line.quest->id;
			}
			if (bIsEntry || bSeen)
			{
				continue;
			}
			FProgressLine Kept;
			Kept.Quest = Line.quest;
			Kept.LineKey = Line.progressKey;
			Kept.bMain = Line.quest->category == abyss::QuestCategory::Main;
			Out.push_back(std::move(Kept));
		}
		return Out;
	}

	/**
	 * The card for `EntryIndex` (the panel's < n/N > page; clamped). Entries without a quest are not drawable: when the
	 * page's entry has none the card falls back to its progress lines (or draws nothing).
	 */
	inline FCardModel BuildCardModel(const abyss::QuestCardState& Card, int32_t EntryIndex, const abyss::DataStore* Data)
	{
		FCardModel Model;
		if (!Card.open)
		{
			return Model;
		}
		Model.Progress = ProgressLines(Card);
		Model.EntryCount = static_cast<int32_t>(Card.entries.size());
		Model.EntryIndex = Model.EntryCount > 0 ? (EntryIndex < 0 ? 0 : (EntryIndex >= Model.EntryCount ? Model.EntryCount - 1 : EntryIndex)) : 0;
		const abyss::QuestCardEntry* Entry = Model.EntryCount > 0 ? &Card.entries[static_cast<size_t>(Model.EntryIndex)] : nullptr;
		if (Entry == nullptr || Entry->quest == nullptr)
		{
			Model.bProgressOnly = !Model.Progress.empty();
			Model.bVisible = Model.bProgressOnly;
			return Model;
		}
		Model.bVisible = true;
		Model.Entry = Entry;
		Model.Action = Entry->turnIn ? ECardAction::TurnIn : ECardAction::Accept;
		const abyss::QuestRewardDef& Rewards = Entry->quest->rewards;
		if (Entry->turnIn)
		{
			Model.Choices = TurnInCaptions(Rewards, *Entry, Data);
		}
		else
		{
			Model.Preview = PreviewCaptions(Rewards);
		}
		return Model;
	}

	/**
	 * Width of one choice column on the turn-in card: the row (WrapWidth) shared by Count columns with Gap between them,
	 * at least the slot itself, at most MaxWidth (two short choices stay compact).
	 */
	inline float ChoiceColumnWidth(int32_t Count, float WrapWidth, float SlotSize, float Gap, float MaxWidth)
	{
		if (Count <= 0)
		{
			return SlotSize;
		}
		const float Shared = (WrapWidth - Gap * static_cast<float>(Count - 1)) / static_cast<float>(Count);
		const float Capped = Shared > MaxWidth ? MaxWidth : Shared;
		return Capped < SlotSize ? SlotSize : Capped;
	}
}  // namespace AbyssQuestCardRules
