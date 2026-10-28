// 2G stage H world state (Source/Abyssfire/Private/World/AbyssWorldStateRules.h) on the real exported data: dawn from
// the world flag the chapter's closing quest sets (and the mood it picks), the linear NPC panel's line, and the NPCs a
// story beat still needs after the core hid them (Marl through cs_rf_finale). Chapters 2-5: no dawn mood, no NPC with
// visibleWhen, so nothing changes there.
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "doctest/doctest.h"

#include "abyss/base/Enums.h"
#include "abyss/base/I18n.h"
#include "abyss/data/DataStore.h"
#include "abyss/data/StoryData.h"

#include "World/AbyssThemeRules.h"
#include "World/AbyssWorldStateRules.h"
#include "audio_test_data.h"

namespace
{
	using namespace AbyssWorldStateRules;

	const abyss::DataStore& Data()
	{
		return audio_test::Data();
	}

	bool HasBothLocales(std::string_view Key)
	{
		const abyss::I18n& Strings = Data().Strings();
		return Strings.Lookup(abyss::LocaleId::ZhCN, Key) != nullptr && Strings.Lookup(abyss::LocaleId::En, Key) != nullptr;
	}

	const abyss::QuestDef* ClosingQuest()
	{
		return Data().FindQuest(Data().Monsters().ai.chapterCompleteQuest);
	}

	abyss::MapTheme BaseMoodAt(std::string_view ZoneId, bool bDawn)
	{
		const abyss::MapDef* M = Data().FindMap(ZoneId);
		AbyssThemeRules::FMoodInputs In;
		In.Moods = &Data().World().moods;
		In.Map = M;
		In.ZoneId = ZoneId;
		In.GeneratorTheme = M != nullptr ? M->theme : abyss::MapTheme::Plains;
		In.bChapterComplete = bDawn;
		return AbyssThemeRules::EffectiveMood(In).Base;
	}

	const abyss::Cutscene& Beat(std::string_view Id)
	{
		const abyss::Cutscene* Found = Data().Story().FindCutscene(Id);
		REQUIRE_MESSAGE(Found != nullptr, std::string(Id));
		return *Found;
	}
}

TEST_SUITE("worldstate")
{
	TEST_CASE("dawn: the closing quest's world flag, not its turn-in (the trenches and the grade turn together)")
	{
		const abyss::QuestDef* Closing = ClosingQuest();
		REQUIRE(Closing != nullptr);
		CHECK(Closing->id == "q_rf_last_toll");
		REQUIRE(Closing->setsFlags.size() == 1);
		CHECK(Closing->setsFlags[0] == "rf_chapter_dawn");

		const std::vector<std::string> None;
		const std::vector<std::string> Silenced = { "rf_hungerbell_silenced" };
		const std::vector<std::string> Dawn = { "rf_chapter_dawn", "rf_hungerbell_silenced" };   // sorted, as the core keeps them
		CHECK_FALSE(ChapterDawn(Closing, &None, /*bTurnedIn*/ false));
		CHECK_FALSE(ChapterDawn(Closing, &Silenced, false));
		CHECK(ChapterDawn(Closing, &Dawn, false));
		CHECK(ChapterDawn(Closing, &Dawn, true));
		// The flag decides, as the core's dawn spawn variants do: a turn-in without the flag (a save from before K6) is no
		// dawn; a missing flag set (no snapshot list) neither.
		CHECK_FALSE(ChapterDawn(Closing, &None, true));
		CHECK_FALSE(ChapterDawn(Closing, nullptr, true));
		// The flag M5 sets is the flag the dawn variants read (one id, no content id in the rules).
		int Variants = 0;
		for (const abyss::MapSpawnDef& Spawn : Data().FindMap("emerald_plains")->spawns)
		{
			for (const abyss::MapSpawnVariantDef& Variant : Spawn.variants)
			{
				CHECK(Variant.whenFlag == Closing->setsFlags[0]);
				++Variants;
			}
		}
		CHECK(Variants >= 3);

		// A closing quest without flags keeps Wave 1's rule (its turn-in); no closing quest: never.
		abyss::QuestDef NoFlags = *Closing;
		NoFlags.setsFlags.clear();
		CHECK(ChapterDawn(&NoFlags, &None, true));
		CHECK_FALSE(ChapterDawn(&NoFlags, &Dawn, false));
		CHECK_FALSE(ChapterDawn(nullptr, &Dawn, true));
	}

	TEST_CASE("flag lookups in the sorted set")
	{
		const std::vector<std::string> Flags = { "a", "rf_chapter_dawn", "rf_hungerbell_silenced", "z" };
		CHECK(HasFlag(&Flags, "a"));
		CHECK(HasFlag(&Flags, "z"));
		CHECK(HasFlag(&Flags, "rf_hungerbell_silenced"));
		CHECK_FALSE(HasFlag(&Flags, "rf_chapter"));
		CHECK_FALSE(HasFlag(&Flags, "rf_hungerbell_silenced_x"));
		CHECK_FALSE(HasFlag(&Flags, ""));
		CHECK_FALSE(HasFlag(nullptr, "a"));
		const std::vector<std::string> Empty;
		CHECK_FALSE(HasFlag(&Empty, "a"));
	}

	TEST_CASE("dawn mood: march -> march_dawn on the flag in the Red Field only; Chapters 2-5 keep their moods")
	{
		const abyss::QuestDef* Closing = ClosingQuest();
		const std::vector<std::string> Dawn = { "rf_chapter_dawn" };
		const std::vector<std::string> None;
		CHECK(BaseMoodAt("emerald_plains", ChapterDawn(Closing, &None, true)) == abyss::MapTheme::March);
		CHECK(BaseMoodAt("emerald_plains", ChapterDawn(Closing, &Dawn, true)) == abyss::MapTheme::MarchDawn);
		// march_dawn's weather is the dust motes: the ashfall stops with the flag.
		const abyss::ZoneMoodDef& DawnDef = Data().World().moods.moods[static_cast<size_t>(abyss::MapTheme::MarchDawn)];
		CHECK(DawnDef.defined);
		int Others = 0;
		for (const abyss::MapDef& Map : Data().World().maps)
		{
			if (Map.id == "emerald_plains")
			{
				continue;
			}
			++Others;
			CAPTURE(Map.id);
			CHECK(BaseMoodAt(Map.id, true) == BaseMoodAt(Map.id, false));
		}
		CHECK(Others >= 5);
	}

	TEST_CASE("the linear panel: the core's line key first, else the web's dialogue[1] ?? [0]")
	{
		FPanelLine L = LinearPanelLine("data.npc.plains_herbalist.after.q_rf_black_blood", "plains_herbalist", 2);
		CHECK(L.bFromLines);
		CHECK(L.Key == "data.npc.plains_herbalist.after.q_rf_black_blood");
		L = LinearPanelLine("", "blacksmith", 2);
		CHECK_FALSE(L.bFromLines);
		CHECK(L.WebIndex == 1);
		CHECK(L.Key == "data.npc.blacksmith.dialogue.1");
		L = LinearPanelLine("", "blacksmith", 1);
		CHECK(L.WebIndex == 0);
		CHECK(L.Key == "data.npc.blacksmith.dialogue.0");
		CHECK(LinearPanelLine("", "blacksmith", 0).Key.empty());
		CHECK(LinearPanelLine("", "", 2).Key.empty());

		// Every K6 line key of the real NPCs exists in both locales (the panel shows any of them), and the web fallback
		// of every Red Field NPC is a real key (elsewhere, e.g. the tower allies, the panel keeps its zh-CN text
		// fallback as today: NameOr).
		const abyss::MapDef* Plains = Data().FindMap("emerald_plains");
		REQUIRE(Plains != nullptr);
		std::vector<std::string> RedField;
		for (const abyss::MapCampDef& Camp : Plains->camps)
		{
			RedField.insert(RedField.end(), Camp.npcs.begin(), Camp.npcs.end());
		}
		for (const abyss::FieldNpcDef& Field : Plains->fieldNpcs)
		{
			RedField.push_back(Field.npcId);
		}
		int WithLines = 0;
		int WebChecked = 0;
		for (const abyss::NpcDef& Npc : Data().Npcs().npcs)
		{
			CAPTURE(Npc.id);
			for (const abyss::NpcLineDef& Line : Npc.lines)
			{
				CHECK(HasBothLocales(LinearPanelLine(Line.key, Npc.id, Npc.dialogue.size()).Key));
			}
			WithLines += Npc.lines.empty() ? 0 : 1;
			const FPanelLine Web = LinearPanelLine("", Npc.id, Npc.dialogue.size());
			if (!Npc.dialogue.empty() && std::find(RedField.begin(), RedField.end(), Npc.id) != RedField.end())
			{
				CHECK(HasBothLocales(Web.Key));
				++WebChecked;
			}
		}
		CHECK(WithLines >= 3);
		CHECK(WebChecked >= 3);
	}

	TEST_CASE("cutscene NPCs: focus targets and say speakers, once each, in step order")
	{
		const std::vector<std::string> Finale = CutsceneNpcs(Beat("cs_rf_finale"));
		CHECK(Finale == std::vector<std::string>{ "quest_elder", "plains_herbalist", "knell_chaplain" });
		const abyss::Cutscene& F = Beat("cs_rf_finale");
		int FocusOnMarl = 0;
		for (const abyss::CutsceneStep& Step : F.steps)
		{
			if (FocusNpc(Step) == "plains_herbalist")
			{
				++FocusOnMarl;
			}
			if (Step.kind != abyss::StoryStepKind::Focus)
			{
				CHECK(FocusNpc(Step).empty());
			}
		}
		CHECK(FocusOnMarl == 1);
		abyss::Cutscene Synthetic;
		abyss::CutsceneStep Tile;
		Tile.kind = abyss::StoryStepKind::Focus;
		Tile.target.kind = abyss::StoryActorKind::Tile;
		abyss::CutsceneStep Monster;
		Monster.kind = abyss::StoryStepKind::Focus;
		Monster.target.kind = abyss::StoryActorKind::Monster;
		Monster.target.id = "valdras";
		abyss::CutsceneStep Narrate;
		Narrate.kind = abyss::StoryStepKind::Narrate;
		Narrate.speaker.kind = abyss::StoryActorKind::Npc;   // not a say: ignored
		Narrate.speaker.id = "x";
		Synthetic.steps = { Tile, Monster, Narrate };
		CHECK(CutsceneNpcs(Synthetic).empty());
		CHECK(FocusNpc(Monster).empty());
	}

	TEST_CASE("Marl: M5's turn-in hides her while cs_rf_finale, which it queues, focuses her: the hold keeps her drawn")
	{
		// The data that makes the case (2F review 2F-D10): Marl's visibleWhen fails once M5 is turned in (unless S1 is
		// active), and M5's turn-in queues cs_rf_finale, which focuses her and gives her a line.
		const abyss::NpcDef* Marl = Data().FindNpc("plains_herbalist");
		REQUIRE(Marl != nullptr);
		REQUIRE(Marl->hasVisibleWhen);
		bool bGatedOnM5 = false;
		for (const abyss::WorldConditionLeaf& Leaf : Marl->visibleWhen.any)
		{
			bGatedOnM5 = bGatedOnM5 || (Leaf.kind == abyss::WorldLeafKind::Quest && Leaf.id == "q_rf_last_toll"
				&& Leaf.state == abyss::QuestGate::NotTurnedIn);
		}
		CHECK(bGatedOnM5);

		const abyss::StoryScript& Story = Data().Story();
		FStoryNpcHolds Holds;
		CHECK(Holds.Expect(Story, abyss::StoryTriggerOn::QuestTurnedIn, "q_rf_last_toll", 10.0) == 1);
		REQUIRE(Holds.Entries().size() == 1);
		CHECK(Holds.Entries()[0].BeatId == "cs_rf_finale");
		CHECK(Holds.Wants("plains_herbalist"));
		CHECK(Holds.Wants("quest_elder"));
		CHECK_FALSE(Holds.Wants("blacksmith"));
		CHECK_FALSE(Holds.Wants(""));
		// The beat is queued (the director busy) through its T15 delay: kept.
		Holds.Update(10.5, /*bStoryBusy*/ true);
		CHECK(Holds.Wants("plains_herbalist"));
		Holds.BeatBegan("cs_rf_finale");
		Holds.Update(70.0, true);   // a long finale (say steps wait for input)
		CHECK(Holds.Wants("plains_herbalist"));
		// Another beat's end releases nothing; the finale's end releases her.
		CHECK_FALSE(Holds.BeatEnded("cs_rf_siege"));
		CHECK(Holds.Wants("plains_herbalist"));
		CHECK(Holds.BeatEnded("cs_rf_finale"));
		CHECK_FALSE(Holds.Wants("plains_herbalist"));
		CHECK(Holds.Empty());
	}

	TEST_CASE("holds end when the beat is not coming, on a zone change, and after the safety times")
	{
		const abyss::StoryScript& Story = Data().Story();
		FStoryNpcHolds Holds;
		// Seen before: the director never queues it, so it is idle at the end of the turn-in's frame.
		Holds.Expect(Story, abyss::StoryTriggerOn::QuestTurnedIn, "q_rf_last_toll", 0.0);
		Holds.Update(0.0, /*bStoryBusy*/ false);
		CHECK_FALSE(Holds.Wants("plains_herbalist"));
		// The director goes idle (the beat ended without an Ended we saw): released.
		Holds.Expect(Story, abyss::StoryTriggerOn::QuestTurnedIn, "q_rf_last_toll", 0.0);
		Holds.BeatBegan("cs_rf_finale");
		Holds.Update(5.0, false);
		CHECK(Holds.Empty());
		// Waiting to begin past HoldWaitBeginSec while the director is busy with other beats: dropped.
		Holds.Expect(Story, abyss::StoryTriggerOn::QuestTurnedIn, "q_rf_last_toll", 0.0);
		Holds.Update(HoldWaitBeginSec - 1.0, true);
		CHECK(Holds.Wants("plains_herbalist"));
		Holds.Update(HoldWaitBeginSec + 1.0, true);
		CHECK(Holds.Empty());
		// Playing past HoldMaxSec: dropped.
		Holds.Expect(Story, abyss::StoryTriggerOn::QuestTurnedIn, "q_rf_last_toll", 0.0);
		Holds.BeatBegan("cs_rf_finale");
		Holds.Update(HoldMaxSec - 1.0, true);
		CHECK_FALSE(Holds.Empty());
		Holds.Update(HoldMaxSec + 1.0, true);
		CHECK(Holds.Empty());
		// Clear (zone change, session end).
		Holds.Expect(Story, abyss::StoryTriggerOn::QuestTurnedIn, "q_rf_last_toll", 0.0);
		Holds.Clear();
		CHECK(Holds.Empty());
		// A quest without a trigger, an unknown quest, an empty id: nothing expected. Accepting M5 queues the siege.
		CHECK(Holds.Expect(Story, abyss::StoryTriggerOn::QuestTurnedIn, "q_rf_black_blood", 0.0) == 0);
		CHECK(Holds.Expect(Story, abyss::StoryTriggerOn::QuestTurnedIn, "nope", 0.0) == 0);
		CHECK(Holds.Expect(Story, abyss::StoryTriggerOn::QuestTurnedIn, "", 0.0) == 0);
		CHECK(Holds.Expect(Story, abyss::StoryTriggerOn::QuestAccepted, "q_rf_last_toll", 0.0) <= 1);
		// The same beat expected twice is one entry (re-armed).
		Holds.Clear();
		CHECK(Holds.Expect(Story, abyss::StoryTriggerOn::QuestTurnedIn, "q_rf_last_toll", 0.0) == 1);
		CHECK(Holds.Expect(Story, abyss::StoryTriggerOn::QuestTurnedIn, "q_rf_last_toll", 50.0) == 0);
		CHECK(Holds.Entries().size() == 1);
		CHECK(Holds.Entries()[0].SinceSec == 50.0);
	}

	TEST_CASE("every NPC that can hide is covered when a quest beat names it; Chapters 2-5 have none")
	{
		// The NPCs with a visibleWhen (only Chapter 1's) and the quest beats that name them: each such beat is found by
		// the quest event that queues it, so the hold can keep that NPC through it.
		const abyss::StoryScript& Story = Data().Story();
		int Hiding = 0;
		for (const abyss::NpcDef& Npc : Data().Npcs().npcs)
		{
			if (!Npc.hasVisibleWhen)
			{
				continue;
			}
			++Hiding;
			for (const abyss::StoryTriggerDef& Trigger : Story.triggers)
			{
				if ((Trigger.on != abyss::StoryTriggerOn::QuestTurnedIn && Trigger.on != abyss::StoryTriggerOn::QuestAccepted)
					|| Trigger.cutscene.empty())
				{
					continue;
				}
				const abyss::Cutscene* Cs = Story.FindCutscene(Trigger.cutscene);
				REQUIRE(Cs != nullptr);
				const std::vector<std::string> Npcs = CutsceneNpcs(*Cs);
				if (std::find(Npcs.begin(), Npcs.end(), Npc.id) == Npcs.end())
				{
					continue;
				}
				FStoryNpcHolds Holds;
				Holds.Expect(Story, Trigger.on, Trigger.subjectId, 0.0);
				CAPTURE(Npc.id);
				CAPTURE(Trigger.cutscene);
				CHECK(Holds.Wants(Npc.id));
			}
			// Every hiding NPC stands in the Red Field (the only zone whose NPCs have K6 visibility).
			bool bInRedField = false;
			const abyss::MapDef* Plains = Data().FindMap("emerald_plains");
			for (const abyss::MapCampDef& Camp : Plains->camps)
			{
				bInRedField = bInRedField || std::find(Camp.npcs.begin(), Camp.npcs.end(), Npc.id) != Camp.npcs.end();
			}
			for (const abyss::FieldNpcDef& Field : Plains->fieldNpcs)
			{
				bInRedField = bInRedField || Field.npcId == Npc.id;
			}
			CAPTURE(Npc.id);
			CHECK(bInRedField);
		}
		CHECK(Hiding >= 1);
	}
}
