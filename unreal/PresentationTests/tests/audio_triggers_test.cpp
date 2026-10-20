// U7 cue triggers (Source/Abyssfire/Private/Audio/AbyssCueTriggerRules.h) on the committed audio manifest and the real
// story data: which story beats, events and timers ring the bells (0-based EvStoryStep indices: the siege self-ring
// is index 1, the dawn toll index 3), the near / far toll, bursts bound to their beat, `every` timers.
#include <string>
#include <string_view>
#include <vector>

#include "doctest/doctest.h"

#include "abyss/data/DataStore.h"
#include "abyss/data/StoryData.h"

#include "Audio/AbyssCueTriggerRules.h"
#include "audio_test_data.h"

namespace
{
	using namespace AbyssCueTriggerRules;

	const TriggerTables& Tables()
	{
		static const TriggerTables T = Parse(audio_test::Manifest().Get("cueTriggers"));
		return T;
	}

	const Trigger& Row(std::size_t Index) { return Tables().rows[Index]; }

	std::vector<std::string> Cues(const std::vector<std::size_t>& Rows)
	{
		std::vector<std::string> Out;
		for (const std::size_t I : Rows)
		{
			Out.push_back(Row(I).cue);
		}
		return Out;
	}

	std::size_t RowById(std::string_view Id)
	{
		for (std::size_t I = 0; I < Tables().rows.size(); ++I)
		{
			if (Tables().rows[I].id == Id)
			{
				return I;
			}
		}
		FAIL("no trigger " << std::string(Id));
		return 0;
	}
}

TEST_SUITE("audio")
{
	TEST_CASE("cue triggers parse and resolve against the story data and the cue table")
	{
		const TriggerTables& T = Tables();
		REQUIRE(T.loaded);
		CHECK(T.rows.size() == 10);
		const abyss::DataStore& Data = audio_test::Data();
		const abyss::JsonValue& CueRows = audio_test::Manifest().Get("cues");
		for (const Trigger& R : T.rows)
		{
			CHECK_MESSAGE(CueRows.Has(R.cue), R.id);
			CHECK_MESSAGE((R.farCue.empty() || CueRows.Has(R.farCue)), R.id);
			if (R.on == On::StoryStep || R.on == On::StoryEnd)
			{
				const abyss::Cutscene* Scene = Data.Story().FindCutscene(R.beat);
				REQUIRE_MESSAGE(Scene != nullptr, R.id);
				if (R.on == On::StoryStep)
				{
					CHECK_MESSAGE((R.index >= 0 && R.index < static_cast<int>(Scene->steps.size())), R.id);
				}
			}
			if (R.on == On::Every)
			{
				CHECK(R.everyLoSec > 0.0);
				CHECK(R.everyHiSec >= R.everyLoSec);
				CHECK(R.zone == "emerald_plains");
			}
		}
		// The prose's 1-based steps are the core's 0-based indices: the self-ring is the narrate "The bell rings with no
		// hand on it" (step 2), the dawn toll the flash (step 4).
		const abyss::Cutscene* Siege = Data.Story().FindCutscene("cs_rf_siege");
		const abyss::Cutscene* Finale = Data.Story().FindCutscene("cs_rf_finale");
		REQUIRE(Siege != nullptr);
		REQUIRE(Finale != nullptr);
		CHECK(Row(RowById("siege_self_ring")).index == 1);
		CHECK(Siege->steps[1].kind == abyss::StoryStepKind::Narrate);
		CHECK(Row(RowById("finale_dawn_toll")).index == 3);
		CHECK(Finale->steps[3].kind == abyss::StoryStepKind::Flash);
		CHECK(Data.Story().prologue.slides.size() == 6);
		CHECK(Row(RowById("prologue_tolls")).index == 5);
	}

	TEST_CASE("story beats ring the bell")
	{
		CHECK(Cues(MatchStoryStep(Tables(), "cs_rf_siege", "", 1, false)) == std::vector<std::string>{ "bell_self_ring" });
		CHECK(MatchStoryStep(Tables(), "cs_rf_siege", "", 0, false).empty());
		CHECK(MatchStoryStep(Tables(), "cs_rf_siege", "", 2, false).empty());
		CHECK(Cues(MatchStoryStep(Tables(), "cs_rf_finale", "", 3, false)) == std::vector<std::string>{ "bell_dawn_toll" });
		CHECK(MatchStoryStep(Tables(), "cs_rf_finale", "", 4, false).empty());
		CHECK(Cues(MatchStoryStep(Tables(), "cs_rf_herd", "", 2, false)) ==
			std::vector<std::string>{ "vox_corpseherd_bell" });
		// A slide matches only slide rows (and a step of the same index never matches a slide row).
		CHECK(Cues(MatchStoryStep(Tables(), "prologue", "prologue", 5, true)) == std::vector<std::string>{ "bell_toll" });
		CHECK(MatchStoryStep(Tables(), "cs_rf_finale", "", 3, true).empty());
		CHECK(MatchStoryStep(Tables(), "prologue", "prologue", 4, true).empty());
		CHECK(MatchStoryStep(Tables(), "epilogue", "epilogue", 5, true).empty());
		CHECK(Cues(MatchStoryEnd(Tables(), "cs_rf_brand")) == std::vector<std::string>{ "bell_toll" });
		CHECK(Cues(MatchStoryEnd(Tables(), "cs_rf_whisper")) == std::vector<std::string>{ "bell_toll" });
		CHECK(MatchStoryEnd(Tables(), "cs_rf_siege").empty());
		CHECK(MatchStoryEnd(Tables(), "cs_tf_hermit").empty());  // later chapters: nothing
	}

	TEST_CASE("near and far tolls")
	{
		const Trigger& Toll = Row(MatchStoryEnd(Tables(), "cs_rf_brand").at(0));
		CHECK(NeedsRegion(Toll));
		CHECK(Toll.nearRegion == "knellwatch");
		CHECK(CueFor(Toll, true) == "bell_toll");
		CHECK(CueFor(Toll, false) == "bell_toll_far");
		const Trigger& Dawn = Row(RowById("finale_dawn_toll"));
		CHECK_FALSE(NeedsRegion(Dawn));
		CHECK(CueFor(Dawn, false) == "bell_dawn_toll");
	}

	TEST_CASE("world events: the ambush tell, the split bell")
	{
		CHECK(Cues(MatchRandomEvent(Tables(), "ambush", false, "emerald_plains")) ==
			std::vector<std::string>{ "bell_ambush_tell" });
		CHECK(MatchRandomEvent(Tables(), "ambush", true, "emerald_plains").empty());  // the resolve
		CHECK(MatchRandomEvent(Tables(), "ambush", false, "twilight_forest").empty());
		CHECK(MatchRandomEvent(Tables(), "treasure_cache", false, "emerald_plains").empty());
		const std::vector<std::size_t> Belfry = MatchDecorFocus(Tables(), "story_rf_belfry");
		REQUIRE(Belfry.size() == 1);
		CHECK(Row(Belfry[0]).cue == "bell_cracked");
		CHECK(Row(Belfry[0]).hasTile);
		CHECK(Row(Belfry[0]).tileCol == 55.0);
		CHECK(Row(Belfry[0]).tileRow == 45.0);
		CHECK(MatchDecorFocus(Tables(), "").empty());
		CHECK(MatchDecorFocus(Tables(), "story_rf_red_banner").empty());
	}

	TEST_CASE("bursts follow their spacing and stop with their beat")
	{
		Scheduler S;
		const std::size_t Prologue = RowById("prologue_tolls");
		CHECK(Row(Prologue).count == 10);
		CHECK(Row(Prologue).spacingSec == doctest::Approx(2.8));
		const auto Never = [](std::size_t) { return false; };
		const auto Half = []() { return 0.5; };
		S.Start(Tables(), Prologue, 100.0, "prologue");
		CHECK(S.Tick(Tables(), 100.0, Never, Half).size() == 1);
		CHECK(S.Tick(Tables(), 102.7, Never, Half).empty());
		CHECK(S.Tick(Tables(), 102.8, Never, Half).size() == 1);
		CHECK(S.Tick(Tables(), 108.5, Never, Half).size() == 2);
		CHECK(S.PendingCount() == 6);
		S.StopBound("cs_rf_brand");
		CHECK(S.PendingCount() == 6);
		S.StopBound("prologue");
		CHECK(S.PendingCount() == 0);
		CHECK(S.Tick(Tables(), 200.0, Never, Half).empty());
		// A single toll (count 1) is one member; an event handler takes it at once (TakeDue), the rest wait.
		S.Start(Tables(), MatchStoryEnd(Tables(), "cs_rf_brand").at(0), 10.0, "");
		CHECK(S.TakeDue(10.0).size() == 1);
		CHECK(S.PendingCount() == 0);
		S.Start(Tables(), Prologue, 20.0, "prologue");
		CHECK(S.TakeDue(20.0) == std::vector<std::size_t>{ Prologue });
		CHECK(S.PendingCount() == 9);
		CHECK(S.Tick(Tables(), 22.8, Never, Half).size() == 1);
	}

	TEST_CASE("every: the siege self-ring and the split bell, an interval after their condition holds")
	{
		Scheduler S;
		const std::size_t Siege = RowById("siege_self_ring_again");
		const std::size_t Belfry = RowById("belfry_now_and_then");
		CHECK(Row(Siege).whileQuestActive == "q_rf_last_toll");
		CHECK(Row(Siege).everyLoSec == 20.0);
		CHECK(Row(Siege).everyHiSec == 30.0);
		CHECK(Row(Belfry).hasWithin);
		CHECK(Row(Belfry).within.radius == 10.0);
		bool bSiege = true;
		const auto Holds = [&](std::size_t I) { return I == Siege && bSiege; };
		const auto Zero = []() { return 0.0; };
		const auto One = []() { return 1.0; };
		CHECK(S.Tick(Tables(), 0.0, Holds, Zero).empty());    // armed: next at 20 s
		CHECK(S.Tick(Tables(), 19.9, Holds, Zero).empty());
		CHECK(S.Tick(Tables(), 20.0, Holds, One) == std::vector<std::size_t>{ Siege });  // next at 50 s
		CHECK(S.Tick(Tables(), 49.0, Holds, Zero).empty());
		CHECK(S.Tick(Tables(), 50.0, Holds, Zero).size() == 1);
		bSiege = false;  // the last wave fell (or a story beat plays)
		CHECK(S.Tick(Tables(), 75.0, Holds, Zero).empty());
		bSiege = true;  // re-armed from now
		CHECK(S.Tick(Tables(), 80.0, Holds, Zero).empty());
		CHECK(S.Tick(Tables(), 99.0, Holds, Zero).empty());
		CHECK(S.Tick(Tables(), 100.0, Holds, Zero).size() == 1);
		CHECK(Scheduler::Interval(Row(Siege), 0.5) == doctest::Approx(25.0));
		CHECK(Scheduler::Interval(Row(Siege), 7.0) == doctest::Approx(30.0));
		S.OnZoneChanged();
		CHECK(S.Tick(Tables(), 101.0, Holds, Zero).empty());  // re-armed after a zone change
	}

	TEST_CASE("malformed rows are dropped")
	{
		const abyss::JsonValue Rows = audio_test::Json(R"({"rows": [
			{"id": "a", "cue": "bell_toll", "on": "every"},
			{"id": "b", "cue": "", "on": "storyEnd", "beat": "x"},
			{"id": "c", "cue": "bell_toll", "on": "dusk"},
			{"id": "d", "cue": "bell_toll", "on": "every", "everySec": [30, 20], "count": 0},
			{"id": "e", "cue": "bell_toll", "on": "storyEnd", "beat": "x"}]})");
		const TriggerTables T = Parse(Rows);
		REQUIRE(T.rows.size() == 2);
		CHECK(T.rows[0].id == "d");
		CHECK(T.rows[0].everyHiSec == 30.0);  // hi < lo reads as lo
		CHECK(T.rows[0].count == 1);
		CHECK(T.rows[1].id == "e");
		CHECK_FALSE(Parse(abyss::JsonValue()).loaded);
	}
}
