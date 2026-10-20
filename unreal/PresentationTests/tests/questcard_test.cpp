// U5 quest card view model (Source/Abyssfire/Private/UI/Panels/AbyssQuestCardRules.h): progress-only cards, progress
// lines beside entries, labelled reward choices that follow the generated items, the accept card's labelled preview.
// Real exported tables (unreal/Data) for the Chapter 1 quests + the core's reward generator; synthetic cards otherwise.
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "doctest/doctest.h"

#include "abyss/base/Log.h"
#include "abyss/base/Rng.h"
#include "abyss/data/DataStore.h"
#include "abyss/items/Item.h"
#include "abyss/items/LootGen.h"
#include "abyss/quests/QuestWorld.h"

#include "UI/Panels/AbyssQuestCardRules.h"

namespace
{
	using namespace AbyssQuestCardRules;

	std::string ReadFileBytes(const std::string& Path)
	{
		std::ifstream In(Path, std::ios::binary);
		if (!In)
		{
			return {};
		}
		std::ostringstream Ss;
		Ss << In.rdbuf();
		return Ss.str();
	}

	const abyss::DataStore& Data()
	{
		static abyss::DataStore Store;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			abyss::DataLoadReport Report;
			const bool bOk = Store.LoadAll(
				[](std::string_view Name, std::string& Out)
				{
					Out = ReadFileBytes(std::string(ABYSS_UNREAL_DIR) + "/Data/" + std::string(Name));
					return !Out.empty();
				},
				Report);
			if (!bOk)
			{
				abyss::Log(abyss::LogLevel::Error, Report.Summary());
			}
		}
		return Store;
	}

	const abyss::QuestDef& Quest(const char* Id)
	{
		const abyss::QuestDef* Def = Data().FindQuest(Id);
		REQUIRE_MESSAGE(Def != nullptr, Id);
		return *Def;
	}

	/** The first base of a group / slot in the real item table (the generator's pools). */
	const abyss::ItemBaseDef& BaseOf(const char* Group, abyss::EquipSlot Slot, bool bAnySlot = false)
	{
		for (const abyss::ItemBaseDef& Base : Data().Items().bases)
		{
			if (Base.group == Group && (bAnySlot || (Base.hasSlot && Base.slot == Slot)))
			{
				return Base;
			}
		}
		FAIL("no base in group " << Group);
		return Data().Items().bases.front();
	}

	abyss::ItemInstance ItemOf(const abyss::ItemBaseDef& Base)
	{
		abyss::ItemInstance Item;
		Item.baseId = Base.id;
		return Item;
	}

	bool HasBothLocales(const std::string& Key)
	{
		const abyss::I18n& Strings = Data().Strings();
		return Strings.Lookup(abyss::LocaleId::ZhCN, Key) != nullptr && Strings.Lookup(abyss::LocaleId::En, Key) != nullptr;
	}

	/** The Chapter 1 quests with a progress line (chapter1.md: every main and side quest of the march). */
	const char* const kProgressQuests[] = { "q_rf_burial_detail", "q_rf_gnawed_field", "q_rf_ashwheat", "q_rf_marshal", "q_rf_last_toll",
		"q_rf_black_blood", "q_rf_barrowmoss", "q_rf_tag_thief", "q_rf_oathless", "q_rf_lanternwisp", "q_rf_hallowed_ground" };
}

TEST_SUITE("questcard")
{
	TEST_CASE("real data: every Chapter 1 progress key and choice label exists in zh-CN and en")
	{
		for (const char* Id : kProgressQuests)
		{
			const abyss::QuestDef& Def = Quest(Id);
			INFO(Id);
			CHECK(!Def.progressKey.empty());
			CHECK(HasBothLocales(Def.progressKey));
		}
		for (const char* Id : { "q_rf_tag_thief", "q_rf_oathless" })
		{
			const abyss::QuestDef& Def = Quest(Id);
			INFO(Id);
			REQUIRE(Def.rewards.choiceLabels.size() == Def.rewards.choices.size());
			REQUIRE(!Def.rewards.choiceLabels.empty());
			for (const std::string& Key : Def.rewards.choiceLabels)
			{
				CHECK(HasBothLocales(Key));
			}
		}
		// every slot caption the card can build
		for (const char* Slot : { "weapon", "armor", "helmet", "gloves", "boots", "belt", "jewelry", "offhand" })
		{
			CHECK(HasBothLocales(SlotCaptionKey(Slot)));
		}
	}

	TEST_CASE("closed or empty cards draw nothing")
	{
		abyss::QuestCardState Card;
		CHECK_FALSE(BuildCardModel(Card, 0, &Data()).bVisible);
		Card.open = true;
		Card.npcId = "quest_elder";
		const FCardModel Model = BuildCardModel(Card, 0, &Data());
		CHECK_FALSE(Model.bVisible);
		CHECK_FALSE(Model.bProgressOnly);
		// a progress line without its quest or key is not drawable either
		Card.progress.push_back(abyss::QuestCardProgress{ nullptr, "data.quest.x.progress" });
		Card.progress.push_back(abyss::QuestCardProgress{ &Quest("q_rf_marshal"), "" });
		CHECK_FALSE(BuildCardModel(Card, 0, &Data()).bVisible);
	}

	TEST_CASE("progress-only card: visible, no entry, no primary action, the lines in the core's order")
	{
		abyss::QuestCardState Card;
		Card.open = true;
		Card.npcId = "quest_elder";
		Card.progress.push_back(abyss::QuestCardProgress{ &Quest("q_rf_marshal"), Quest("q_rf_marshal").progressKey });
		Card.progress.push_back(abyss::QuestCardProgress{ &Quest("q_rf_black_blood"), Quest("q_rf_black_blood").progressKey });
		const FCardModel Model = BuildCardModel(Card, 3, &Data());
		CHECK(Model.bVisible);
		CHECK(Model.bProgressOnly);
		CHECK(Model.Entry == nullptr);
		CHECK(Model.EntryCount == 0);
		CHECK(Model.EntryIndex == 0);
		CHECK(Model.Action == ECardAction::None);
		REQUIRE(Model.Progress.size() == 2);
		CHECK(Model.Progress[0].Quest->id == "q_rf_marshal");
		CHECK(Model.Progress[0].LineKey == "data.quest.q_rf_marshal.progress");
		CHECK(Model.Progress[0].bMain);
		CHECK(Model.Progress[1].Quest->id == "q_rf_black_blood");
		CHECK_FALSE(Model.Progress[1].bMain);
		CHECK(Model.Choices.empty());
		CHECK(Model.Preview.empty());
	}

	TEST_CASE("mixed card: the entry is the card, progress lines ride along; duplicates and entry quests are dropped")
	{
		abyss::QuestCardState Card;
		Card.open = true;
		Card.npcId = "quest_elder";
		abyss::QuestCardEntry Offer;
		Offer.quest = &Quest("q_rf_black_blood");
		Card.entries.push_back(Offer);
		Card.progress.push_back(abyss::QuestCardProgress{ &Quest("q_rf_burial_detail"), Quest("q_rf_burial_detail").progressKey });
		Card.progress.push_back(abyss::QuestCardProgress{ &Quest("q_rf_black_blood"), Quest("q_rf_black_blood").progressKey });  // also the entry
		Card.progress.push_back(abyss::QuestCardProgress{ &Quest("q_rf_burial_detail"), Quest("q_rf_burial_detail").progressKey });  // twice
		const FCardModel Model = BuildCardModel(Card, 0, &Data());
		CHECK(Model.bVisible);
		CHECK_FALSE(Model.bProgressOnly);
		REQUIRE(Model.Entry != nullptr);
		CHECK(Model.Entry->quest->id == "q_rf_black_blood");
		CHECK(Model.Action == ECardAction::Accept);
		REQUIRE(Model.Progress.size() == 1);
		CHECK(Model.Progress[0].Quest->id == "q_rf_burial_detail");
	}

	TEST_CASE("the entry index is clamped; an entry without a quest falls back to the progress lines")
	{
		abyss::QuestCardState Card;
		Card.open = true;
		abyss::QuestCardEntry TurnIn;
		TurnIn.quest = &Quest("q_rf_burial_detail");
		TurnIn.turnIn = true;
		abyss::QuestCardEntry Offer;
		Offer.quest = &Quest("q_rf_black_blood");
		Card.entries = { TurnIn, Offer };
		CHECK(BuildCardModel(Card, -4, &Data()).EntryIndex == 0);
		CHECK(BuildCardModel(Card, 9, &Data()).EntryIndex == 1);
		CHECK(BuildCardModel(Card, 9, &Data()).Action == ECardAction::Accept);
		CHECK(BuildCardModel(Card, 0, &Data()).Action == ECardAction::TurnIn);
		CHECK(BuildCardModel(Card, 0, &Data()).EntryCount == 2);

		abyss::QuestCardState Broken;
		Broken.open = true;
		Broken.entries.push_back(abyss::QuestCardEntry{});
		CHECK_FALSE(BuildCardModel(Broken, 0, &Data()).bVisible);
		Broken.progress.push_back(abyss::QuestCardProgress{ &Quest("q_rf_marshal"), Quest("q_rf_marshal").progressKey });
		const FCardModel Model = BuildCardModel(Broken, 0, &Data());
		CHECK(Model.bVisible);
		CHECK(Model.bProgressOnly);
		CHECK(Model.Entry == nullptr);
	}

	TEST_CASE("labelled turn-in: the label is the primary caption, the item's slot the secondary one")
	{
		const abyss::QuestDef& Def = Quest("q_rf_tag_thief");  // jewelry: keep the silver / weapon: give it to Hald
		abyss::QuestCardEntry Entry;
		Entry.quest = &Def;
		Entry.turnIn = true;
		Entry.choices = { ItemOf(BaseOf("accessories", abyss::EquipSlot::Ring1, true)), ItemOf(BaseOf("weapons", abyss::EquipSlot::Weapon)) };
		Entry.choiceLabels = Def.rewards.choiceLabels;
		abyss::QuestCardState Card;
		Card.open = true;
		Card.entries.push_back(Entry);
		const FCardModel Model = BuildCardModel(Card, 0, &Data());
		REQUIRE(Model.Choices.size() == 2);
		CHECK(Model.Preview.empty());
		CHECK(Model.Choices[0].LabelKey == "data.questChoice.q_rf_tag_thief.jewelry");
		CHECK(Model.Choices[0].SlotId == "jewelry");
		CHECK(Model.Choices[0].SlotIndex == 0);
		CHECK(Model.Choices[1].LabelKey == "data.questChoice.q_rf_tag_thief.weapon");
		CHECK(Model.Choices[1].SlotId == "weapon");
		const FCaptionKeys Keys = CaptionKeys(Model.Choices[1]);
		CHECK(Keys.Primary == "data.questChoice.q_rf_tag_thief.weapon");
		CHECK(Keys.Secondary == "sys.questCard.choice.weapon");
	}

	TEST_CASE("labels follow the generated items: a slot that yielded nothing takes its label and its slot name with it")
	{
		const abyss::QuestDef& Def = Quest("q_rf_tag_thief");
		abyss::QuestCardEntry Entry;
		Entry.quest = &Def;
		Entry.turnIn = true;
		// the jewelry slot produced nothing: one weapon, labelled by the core with the weapon slot's key
		Entry.choices = { ItemOf(BaseOf("weapons", abyss::EquipSlot::Weapon)) };
		Entry.choiceLabels = { Def.rewards.choiceLabels[1] };
		const std::vector<FChoiceCaption> Captions = TurnInCaptions(Def.rewards, Entry, &Data());
		REQUIRE(Captions.size() == 1);
		CHECK(Captions[0].LabelKey == "data.questChoice.q_rf_tag_thief.weapon");
		CHECK(Captions[0].SlotIndex == 1);
		CHECK(Captions[0].SlotId == "weapon");  // today's card said rewards.choices[0] = jewelry
	}

	TEST_CASE("unlabelled turn-in: the slot name alone; a skipped slot is recovered from the item's base")
	{
		const abyss::QuestDef& Def = Quest("q_rf_last_toll");  // jewelry, boots, gloves (no labels)
		REQUIRE(Def.rewards.choiceLabels.empty());
		REQUIRE(Def.rewards.choices.size() == 3);
		abyss::QuestCardEntry Entry;
		Entry.quest = &Def;
		Entry.turnIn = true;
		Entry.choices = { ItemOf(BaseOf("accessories", abyss::EquipSlot::Ring1, true)), ItemOf(BaseOf("armors", abyss::EquipSlot::Boots)),
			ItemOf(BaseOf("armors", abyss::EquipSlot::Gloves)) };
		std::vector<FChoiceCaption> Captions = TurnInCaptions(Def.rewards, Entry, &Data());
		REQUIRE(Captions.size() == 3);
		CHECK(Captions[0].SlotId == "jewelry");
		CHECK(Captions[1].SlotId == "boots");
		CHECK(Captions[2].SlotId == "gloves");
		for (const FChoiceCaption& Caption : Captions)
		{
			CHECK(Caption.LabelKey.empty());
			const FCaptionKeys Keys = CaptionKeys(Caption);
			CHECK(Keys.Primary == SlotCaptionKey(Caption.SlotId));
			CHECK(Keys.Secondary.empty());
		}
		// the boots slot yielded nothing: the gloves keep their own name
		Entry.choices = { ItemOf(BaseOf("accessories", abyss::EquipSlot::Ring1, true)), ItemOf(BaseOf("armors", abyss::EquipSlot::Gloves)) };
		Captions = TurnInCaptions(Def.rewards, Entry, &Data());
		REQUIRE(Captions.size() == 2);
		CHECK(Captions[0].SlotId == "jewelry");
		CHECK(Captions[1].SlotId == "gloves");
		CHECK(Captions[1].SlotIndex == 2);
		// an item no slot can yield (or an unknown base) has no slot caption and does not move the walk
		abyss::ItemInstance Unknown;
		Unknown.baseId = "no_such_base";
		Entry.choices = { Unknown, ItemOf(BaseOf("armors", abyss::EquipSlot::Gloves)) };
		Captions = TurnInCaptions(Def.rewards, Entry, &Data());
		REQUIRE(Captions.size() == 2);
		CHECK(Captions[0].SlotIndex == -1);
		CHECK(Captions[0].SlotId.empty());
		CHECK(CaptionKeys(Captions[0]).Primary.empty());
		CHECK(Captions[1].SlotId == "gloves");
	}

	TEST_CASE("label/item length mismatch: the label comes from the item's slot, else none")
	{
		const abyss::QuestDef& Def = Quest("q_rf_oathless");  // weapon: the head to Corran / offhand: the head to the Captain
		abyss::QuestCardEntry Entry;
		Entry.quest = &Def;
		Entry.turnIn = true;
		Entry.choices = { ItemOf(BaseOf("weapons", abyss::EquipSlot::Weapon)), ItemOf(BaseOf("accessories", abyss::EquipSlot::Ring1, true)) };
		Entry.choiceLabels = { Def.rewards.choiceLabels[0] };  // one label for two items: not the core's contract
		std::vector<FChoiceCaption> Captions = TurnInCaptions(Def.rewards, Entry, &Data());
		REQUIRE(Captions.size() == 2);
		CHECK(Captions[0].LabelKey == Def.rewards.choiceLabels[0]);
		CHECK(Captions[1].LabelKey == Def.rewards.choiceLabels[1]);
		CHECK(Captions[1].SlotId == "offhand");
		// an unlabelled quest with stray labels on the entry: slot names only
		const abyss::QuestDef& Plain = Quest("q_rf_marshal");
		abyss::QuestCardEntry PlainEntry;
		PlainEntry.quest = &Plain;
		PlainEntry.turnIn = true;
		PlainEntry.choices = { ItemOf(BaseOf("weapons", abyss::EquipSlot::Weapon)), ItemOf(BaseOf("armors", abyss::EquipSlot::Armor)) };
		PlainEntry.choiceLabels = { "fx.label" };
		Captions = TurnInCaptions(Plain.rewards, PlainEntry, &Data());
		REQUIRE(Captions.size() == 2);
		CHECK(Captions[0].LabelKey.empty());
		CHECK(Captions[1].LabelKey.empty());
		CHECK(Captions[0].SlotId == "weapon");
		CHECK(Captions[1].SlotId == "armor");
	}

	TEST_CASE("the real generator: the captions name the slot each item came from, for every class and Chapter 1 quest")
	{
		for (const abyss::QuestDef& Def : Data().Quests().quests)
		{
			if (Def.zone != "emerald_plains" || Def.rewards.choices.empty())
			{
				continue;
			}
			for (const abyss::ClassId Class : { abyss::ClassId::Warrior, abyss::ClassId::Mage, abyss::ClassId::Rogue })
			{
				for (uint64_t Seed = 1; Seed <= 8; ++Seed)
				{
					abyss::Rng Rng(Seed);
					abyss::ItemUidGenerator Uids;
					const abyss::LootContext Loot{ &Data(), &Rng, &Uids };
					std::vector<int32_t> Slots;
					abyss::QuestCardEntry Entry;
					Entry.quest = &Def;
					Entry.turnIn = true;
					Entry.choices = abyss::GenerateQuestRewardChoices(Loot, Def, Class, Def.level + 1, &Slots);
					Entry.choiceLabels = abyss::QuestRewardChoiceLabels(Def.rewards, Slots);
					const std::vector<FChoiceCaption> Captions = TurnInCaptions(Def.rewards, Entry, &Data());
					INFO(Def.id << " class " << abyss::EnumName(Class) << " seed " << Seed);
					REQUIRE(Captions.size() == Entry.choices.size());
					for (size_t Index = 0; Index < Captions.size(); ++Index)
					{
						CHECK(Captions[Index].SlotIndex == Slots[Index]);
						CHECK(Captions[Index].SlotId == abyss::EnumName(Def.rewards.choices[static_cast<size_t>(Slots[Index])]));
						const abyss::ItemBaseDef* Base = Data().FindItemBase(Entry.choices[Index].baseId);
						REQUIRE(Base != nullptr);
						CHECK(SlotCanYield(Def.rewards.choices[static_cast<size_t>(Slots[Index])], *Base));
						if (!Def.rewards.choiceLabels.empty())
						{
							CHECK(Captions[Index].LabelKey == Def.rewards.choiceLabels[static_cast<size_t>(Slots[Index])]);
						}
						else
						{
							CHECK(Captions[Index].LabelKey.empty());
						}
					}
					// drop the first item and its label, as a slot with an empty pool would: the rest keep their captions
					if (Entry.choices.size() >= 2 && !Def.rewards.choiceLabels.empty())
					{
						Entry.choices.erase(Entry.choices.begin());
						Entry.choiceLabels.erase(Entry.choiceLabels.begin());
						const std::vector<FChoiceCaption> Rest = TurnInCaptions(Def.rewards, Entry, &Data());
						REQUIRE(Rest.size() == Entry.choices.size());
						for (size_t Index = 0; Index < Rest.size(); ++Index)
						{
							CHECK(Rest[Index].SlotIndex == Slots[Index + 1]);
							CHECK(Rest[Index].LabelKey == Def.rewards.choiceLabels[static_cast<size_t>(Slots[Index + 1])]);
						}
					}
				}
			}
		}
	}

	TEST_CASE("accept card preview: every slot, labelled slot by slot when the quest has labels")
	{
		abyss::QuestCardState Card;
		Card.open = true;
		abyss::QuestCardEntry Offer;
		Offer.quest = &Quest("q_rf_oathless");
		Card.entries.push_back(Offer);
		FCardModel Model = BuildCardModel(Card, 0, &Data());
		CHECK(Model.Action == ECardAction::Accept);
		CHECK(Model.Choices.empty());
		REQUIRE(Model.Preview.size() == 2);
		CHECK(Model.Preview[0].SlotId == "weapon");
		CHECK(Model.Preview[0].LabelKey == "data.questChoice.q_rf_oathless.weapon");
		CHECK(Model.Preview[1].SlotId == "offhand");
		CHECK(Model.Preview[1].LabelKey == "data.questChoice.q_rf_oathless.offhand");
		CHECK(CaptionKeys(Model.Preview[1]).Secondary == "sys.questCard.choice.offhand");

		Card.entries[0].quest = &Quest("q_rf_last_toll");
		Model = BuildCardModel(Card, 0, &Data());
		REQUIRE(Model.Preview.size() == 3);
		for (const FChoiceCaption& Caption : Model.Preview)
		{
			CHECK(Caption.LabelKey.empty());
			CHECK(CaptionKeys(Caption).Primary == SlotCaptionKey(Caption.SlotId));
		}
		// a quest without pick-one gear has no preview
		Card.entries[0].quest = &Quest("q_rf_black_blood");
		CHECK(BuildCardModel(Card, 0, &Data()).Preview.empty());
	}

	TEST_CASE("slot pools mirror the reward generator")
	{
		CHECK(SlotCanYield(abyss::RewardSlot::Weapon, BaseOf("weapons", abyss::EquipSlot::Weapon)));
		CHECK_FALSE(SlotCanYield(abyss::RewardSlot::Weapon, BaseOf("armors", abyss::EquipSlot::Armor)));
		CHECK(SlotCanYield(abyss::RewardSlot::Offhand, BaseOf("weapons", abyss::EquipSlot::Offhand)));
		CHECK(SlotCanYield(abyss::RewardSlot::Offhand, BaseOf("accessories", abyss::EquipSlot::Ring1, true)));
		CHECK(SlotCanYield(abyss::RewardSlot::Jewelry, BaseOf("accessories", abyss::EquipSlot::Ring1, true)));
		CHECK_FALSE(SlotCanYield(abyss::RewardSlot::Jewelry, BaseOf("weapons", abyss::EquipSlot::Offhand)));
		CHECK(SlotCanYield(abyss::RewardSlot::Boots, BaseOf("armors", abyss::EquipSlot::Boots)));
		CHECK_FALSE(SlotCanYield(abyss::RewardSlot::Boots, BaseOf("armors", abyss::EquipSlot::Gloves)));
		CHECK(SlotCanYield(abyss::RewardSlot::Belt, BaseOf("armors", abyss::EquipSlot::Belt)));
		CHECK(SlotCanYield(abyss::RewardSlot::Helmet, BaseOf("armors", abyss::EquipSlot::Helmet)));
	}

	TEST_CASE("choice columns share the card width, at least the slot, at most the cap")
	{
		CHECK(ChoiceColumnWidth(0, 380.f, 44.f, 10.f, 150.f) == doctest::Approx(44.f));
		CHECK(ChoiceColumnWidth(1, 380.f, 44.f, 10.f, 150.f) == doctest::Approx(150.f));
		CHECK(ChoiceColumnWidth(2, 380.f, 44.f, 10.f, 150.f) == doctest::Approx(150.f));
		CHECK(ChoiceColumnWidth(3, 380.f, 44.f, 10.f, 150.f) == doctest::Approx(120.f));
		CHECK(ChoiceColumnWidth(4, 380.f, 56.f, 10.f, 150.f) == doctest::Approx(87.5f));
		CHECK(ChoiceColumnWidth(12, 380.f, 56.f, 10.f, 150.f) == doctest::Approx(56.f));
		// n columns never overflow the row unless the slots themselves do
		for (int32_t Count = 1; Count <= 6; ++Count)
		{
			const float Width = ChoiceColumnWidth(Count, 380.f, 44.f, 10.f, 150.f);
			CHECK(Width * static_cast<float>(Count) + 10.f * static_cast<float>(Count - 1) <= doctest::Approx(380.f).epsilon(0.001));
		}
	}
}
