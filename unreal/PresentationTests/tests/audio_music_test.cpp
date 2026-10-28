// U7 music director (Source/Abyssfire/Private/Audio/AbyssMusicRules.h) on the committed audio manifest
// (musicDirector, zoneOverrides): request classes, region choice with hysteresis and `while`, the priority story >
// victory > boss > siege > region > zone, the same-track no-op that released the held combat override, the finale's
// silence then lute, encounter stingers.
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "doctest/doctest.h"

#include "Audio/AbyssMusicRules.h"
#include "audio_test_data.h"

namespace
{
	using namespace AbyssMusicRules;
	using AbyssAudioData::TileOf;

	const DirectorTables& Tables()
	{
		static const DirectorTables T = Parse(audio_test::Manifest().Get("musicDirector"));
		return T;
	}

	int RegionIndex(std::string_view Id)
	{
		for (std::size_t I = 0; I < Tables().regions.size(); ++I)
		{
			if (Tables().regions[I].id == Id)
			{
				return static_cast<int>(I);
			}
		}
		return -1;
	}

	/** The zone override of the committed manifest (emerald_plains: core key -> track). */
	std::string Override(std::string_view CoreKey)
	{
		const abyss::JsonValue& Row = audio_test::Manifest().Get("zoneOverrides").Get("emerald_plains").Get(CoreKey);
		return Row.IsString() ? std::string(Row.AsString()) : std::string(CoreKey);
	}

	CoreRequest Request(std::string_view CoreKey, bool bRestart, double FadeOut = 1.5, double FadeIn = 1.0)
	{
		CoreRequest R;
		R.track = Override(CoreKey);
		R.overridden = R.track != CoreKey;
		R.kind = Classify(CoreKey, { "boss_ch1" });
		R.fadeOutSec = FadeOut;
		R.fadeInSec = FadeIn;
		R.restart = bRestart;
		R.loop = R.kind != RequestKind::Victory;
		return R;
	}

	struct World
	{
		std::vector<std::string> alive{ "valdras", "miniboss_corpseherd", "hunt_hungerbell" };
		AbyssAudioData::CircleTracker tracker;

		int Region(double Col, double Row)
		{
			tracker.Update(Tables().regionAreas, TileOf(Col, Row));
			return PickRegion(Tables(), tracker, [this](std::string_view Id) { return AbyssAudioData::Contains(alive, Id); });
		}
	};

	/** The test's music player: applies the arbiter's decisions and remembers what sounds. */
	struct Player
	{
		Arbiter arbiter;
		std::string sounding;
		double now = 0.0;
		DefaultFades fades;

		Decision Frame(const FrameState& State)
		{
			now += 0.016;
			const Decision D = arbiter.Update(Tables(), State, fades, sounding, now);
			if (D.change)
			{
				sounding = D.track;
			}
			return D;
		}
	};

	FrameState InZone(int Region)
	{
		FrameState S;
		S.inDirectorZone = true;
		S.region = Region;
		return S;
	}
}

TEST_SUITE("audio")
{
	TEST_CASE("music director tables parse from the manifest")
	{
		const DirectorTables& T = Tables();
		REQUIRE(T.loaded);
		CHECK(T.zone == "emerald_plains");
		CHECK(T.regions.size() == 5);
		CHECK(T.regionAreas.size() == T.regions.size());
		const Region& Knell = T.regions[static_cast<std::size_t>(RegionIndex("knellwatch"))];
		CHECK(Knell.area.col == 15.0);
		CHECK(Knell.area.row == 15.0);
		CHECK(Knell.area.radius == 9.0);
		CHECK(Knell.area.exitRadius == 11.0);
		CHECK(Knell.track == "knellwatch_lute");
		CHECK(Knell.fadeInSec == 4.0);
		CHECK(Knell.fadeOutSec == 3.0);
		const Region& Rim = T.regions[static_cast<std::size_t>(RegionIndex("breach_rim"))];
		CHECK(Rim.track.empty());
		CHECK(Rim.when.monsterAlive == "valdras");
		CHECK(Rim.fadeInSec < 0.0);
		REQUIRE(T.questTracks.size() == 1);
		CHECK(T.questTracks[0].beat == "cs_rf_siege");
		CHECK(T.questTracks[0].quest == "q_rf_last_toll");
		CHECK(T.questTracks[0].track == "siege");
		CHECK(T.questTracks[0].hasThen);
		CHECK(T.questTracks[0].thenTrack.empty());
		REQUIRE(T.holds.size() == 1);
		CHECK(T.holds[0].beat == "cs_rf_finale");
		CHECK(T.holds[0].stepIndex == 0);
		CHECK(T.holds[0].track.empty());
		CHECK(T.holds[0].fadeOutSec == 3.0);
		CHECK(T.holds[0].hasThen);
		CHECK(T.holds[0].thenAfterSec == 6.0);
		CHECK(T.holds[0].thenTrack == "knellwatch_lute");
		CHECK(T.holds[0].thenFadeInSec == 6.0);
		REQUIRE(T.bosses.size() == 1);
		CHECK(T.bosses[0].track == "boss_ch1");
		// Every track the director names has a music row (the asset UE loads).
		const abyss::JsonValue& Music = audio_test::Manifest().Get("music");
		for (const Region& R : T.regions)
		{
			CHECK_MESSAGE((R.track.empty() || Music.Has(R.track)), R.id);
		}
		CHECK(Music.Has(T.questTracks[0].track));
		CHECK(Music.Has(T.holds[0].thenTrack));
	}

	TEST_CASE("the combat override is live and resolves to the explore track")
	{
		CHECK(Override("emerald_plains_explore") == "march_drones");
		CHECK(Override("emerald_plains_combat") == "march_drones");
		CHECK(Override("emerald_plains_victory") == "valdras_falls");
		CHECK_FALSE(audio_test::Manifest().Get("musicDirector").Has("heldOverrides"));
	}

	TEST_CASE("request classes")
	{
		const std::vector<std::string> Bosses{ "boss_ch1" };
		CHECK(Classify("emerald_plains_explore", Bosses) == RequestKind::Zone);
		CHECK(Classify("emerald_plains_combat", Bosses) == RequestKind::Zone);
		CHECK(Classify("emerald_plains_victory", Bosses) == RequestKind::Victory);
		CHECK(Classify("abyss_rift_victory", Bosses) == RequestKind::Victory);
		CHECK(Classify("boss_ch1", Bosses) == RequestKind::Boss);
		CHECK(Classify("boss_ch1", {}) == RequestKind::Zone);
		CHECK(Classify("", Bosses) == RequestKind::Zone);
	}

	TEST_CASE("regions: enter at the radius, leave past the exit radius")
	{
		World W;
		const int Knell = RegionIndex("knellwatch");
		CHECK(W.Region(15.0, 25.0) == -1);   // 10 tiles out: outside
		CHECK(W.Region(15.0, 24.0) == Knell);  // 9: enters
		CHECK(W.Region(15.0, 25.0) == Knell);  // 10: stays
		CHECK(W.Region(15.0, 26.0) == Knell);  // 11: stays (exitRadius)
		CHECK(W.Region(15.0, 27.0) == -1);   // 12: leaves
		CHECK(W.Region(15.0, 26.0) == -1);   // 11 from outside: not yet
		CHECK(W.Region(15.4, 24.4) == Knell);  // the hero's tile (24 after rounding)
		CHECK(W.Region(15.0, 15.0) == Knell);
	}

	TEST_CASE("regions: the smallest that holds, while its condition holds")
	{
		World W;
		// The dais lies in the Breach Rim (r16) and the Red Field (r24): the Rim is silent while Valdras lives.
		CHECK(W.Region(15.0, 95.0) == RegionIndex("breach_rim"));
		W.alive = { "miniboss_corpseherd" };
		CHECK(W.Region(15.0, 95.0) == RegionIndex("red_field"));
		// The Sheepfold is silent only while the Corpseherd lives.
		CHECK(W.Region(60.0, 55.0) == RegionIndex("sheepfold"));
		W.alive.clear();
		CHECK(W.Region(60.0, 55.0) == -1);
		CHECK(W.Region(40.0, 52.0) == RegionIndex("ashwheat"));
	}

	TEST_CASE("quest-bound track: the siege after its beat, silence after the last wave")
	{
		QuestPhase Phase = QuestPhase::Active;
		const auto PhaseOf = [&Phase](std::string_view Quest) {
			return Quest == "q_rf_last_toll" ? Phase : QuestPhase::None;
		};
		CHECK(PickQuestTrack(Tables(), "cs_rf_siege", PhaseOf).index == -1);  // the beat still plays
		const QuestTrackPick Siege = PickQuestTrack(Tables(), "", PhaseOf);
		CHECK(Siege.index == 0);
		CHECK_FALSE(Siege.then);
		CHECK(PickQuestTrack(Tables(), "cs_rf_brand", PhaseOf).index == 0);
		Phase = QuestPhase::Completed;
		const QuestTrackPick After = PickQuestTrack(Tables(), "", PhaseOf);
		CHECK(After.index == 0);
		CHECK(After.then);
		Phase = QuestPhase::None;
		CHECK(PickQuestTrack(Tables(), "", PhaseOf).index == -1);
	}

	TEST_CASE("zone track, the combat edge and the same-track no-op")
	{
		Player P;
		CHECK_FALSE(P.Frame(InZone(-1)).change);  // nothing requested yet
		P.arbiter.SetCore(Request("emerald_plains_explore", true, 2.0, 1.0));
		Decision D = P.Frame(InZone(-1));
		CHECK(D.change);
		CHECK(D.track == "march_drones");
		CHECK(D.restart);
		CHECK(D.fadeOutSec == 2.0);
		CHECK(D.source == Source::Core);
		// The combat edge (restart = true) resolves to the drones already sounding: no restart (released override).
		P.arbiter.SetCore(Request("emerald_plains_combat", true));
		CHECK_FALSE(P.Frame(InZone(-1)).change);
		P.arbiter.SetCore(Request("emerald_plains_explore", false));
		CHECK_FALSE(P.Frame(InZone(-1)).change);
		// Without an override, a restart of the sounding track still restarts it (today's rule, other zones).
		Player Other;
		CoreRequest Forest;
		Forest.track = "twilight_forest_explore";
		Forest.restart = true;
		Other.arbiter.SetCore(Forest);
		CHECK(Other.Frame(FrameState{}).change);
		Other.arbiter.SetCore(Forest);
		CHECK(Other.Frame(FrameState{}).change);
		Forest.restart = false;
		Other.arbiter.SetCore(Forest);
		CHECK_FALSE(Other.Frame(FrameState{}).change);
		// A zone outside the director's never sees a region (the index is ignored).
		FrameState Elsewhere;
		Elsewhere.region = RegionIndex("knellwatch");
		Other.arbiter.SetCore(Forest);
		CHECK_FALSE(Other.Frame(Elsewhere).change);
	}

	TEST_CASE("regions replace the zone track and give it back")
	{
		Player P;
		const int Knell = RegionIndex("knellwatch");
		P.arbiter.SetCore(Request("emerald_plains_explore", true));
		CHECK(P.Frame(InZone(-1)).track == "march_drones");
		Decision D = P.Frame(InZone(Knell));
		CHECK(D.change);
		CHECK(D.track == "knellwatch_lute");
		CHECK(D.fadeInSec == 4.0);
		CHECK(D.fadeOutSec == 3.0);
		CHECK_FALSE(D.restart);
		CHECK(D.source == Source::Region);
		// A fight inside the fort keeps the lute.
		P.arbiter.SetCore(Request("emerald_plains_combat", true));
		CHECK_FALSE(P.Frame(InZone(Knell)).change);
		// Leaving: the drones come back where they were, the lute fades over the region's fadeOutSec.
		D = P.Frame(InZone(-1));
		CHECK(D.change);
		CHECK(D.track == "march_drones");
		CHECK(D.fadeOutSec == 3.0);
		CHECK(D.fadeInSec == 1.0);
		CHECK_FALSE(D.restart);
		// A silence region: the drones fade out over its fadeOutSec.
		D = P.Frame(InZone(RegionIndex("ashwheat")));
		CHECK(D.change);
		CHECK(D.track.empty());
		CHECK(D.fadeOutSec == 5.0);
	}

	TEST_CASE("boss beats region, victory beats boss, then the region again")
	{
		Player P;
		const int Rim = RegionIndex("breach_rim");
		P.arbiter.SetCore(Request("emerald_plains_explore", true));
		P.Frame(InZone(-1));
		Decision D = P.Frame(InZone(Rim));
		CHECK(D.track.empty());
		CHECK(D.fadeOutSec == 4.0);
		P.arbiter.SetCore(Request("boss_ch1", true, 1.5, 1.0));
		D = P.Frame(InZone(Rim));
		CHECK(D.change);
		CHECK(D.track == "boss_ch1");
		CHECK(D.restart);
		P.arbiter.SetCore(Request("emerald_plains_victory", true));
		D = P.Frame(InZone(Rim));
		CHECK(D.track == "valdras_falls");
		CHECK_FALSE(D.loop);
		// The hold ends: explore resumes in the core; Valdras is dead, so the Red Field's dirge plays here.
		P.arbiter.SetCore(Request("emerald_plains_explore", false));
		D = P.Frame(InZone(RegionIndex("red_field")));
		CHECK(D.change);
		CHECK(D.track == "red_field");
		CHECK(D.fadeInSec == 5.0);
		CHECK_FALSE(D.restart);
	}

	TEST_CASE("a one-shot plays once: not again when it ends or after a layer")
	{
		Player P;
		P.arbiter.SetCore(Request("emerald_plains_victory", true));
		CHECK(P.Frame(InZone(-1)).track == "valdras_falls");
		P.sounding.clear();  // the one-shot reached its end; the core's victory hold still runs
		CHECK_FALSE(P.Frame(InZone(-1)).change);
		CHECK_FALSE(P.Frame(InZone(-1)).change);
		// The core's next request plays as usual.
		P.arbiter.SetCore(Request("emerald_plains_explore", false));
		CHECK(P.Frame(InZone(-1)).track == "march_drones");
		// A victory a story hold cut short is not replayed when the hold ends.
		Player Q;
		Q.arbiter.SetCore(Request("emerald_plains_victory", true));
		Q.Frame(InZone(-1));
		Q.arbiter.OnStoryStep(Tables(), "cs_rf_finale", 0, false);
		CHECK(Q.Frame(InZone(-1)).track.empty());
		Q.arbiter.OnStoryEnded(Tables(), "cs_rf_finale", Q.now);
		Q.now += 6.0;
		CHECK(Q.Frame(InZone(-1)).track == "knellwatch_lute");  // the hold's `then`
		const Decision After = Q.Frame(InZone(-1));
		CHECK_FALSE(After.change);  // the stale victory is not played again (the lute keeps playing)
	}

	TEST_CASE("siege beats region, boss beats siege, silence after the last wave")
	{
		Player P;
		const int Knell = RegionIndex("knellwatch");
		P.arbiter.SetCore(Request("emerald_plains_explore", true));
		P.Frame(InZone(Knell));
		CHECK(P.sounding == "knellwatch_lute");
		FrameState S = InZone(Knell);
		S.siege = QuestTrackPick{ 0, false };
		Decision D = P.Frame(S);
		CHECK(D.change);
		CHECK(D.track == "siege");
		CHECK(D.source == Source::Siege);
		CHECK(D.fadeOutSec == 3.0);  // the lute leaves over Knellwatch's fadeOutSec
		CHECK(D.fadeInSec == 2.0);
		P.arbiter.SetCore(Request("emerald_plains_combat", true));
		CHECK_FALSE(P.Frame(S).change);
		P.arbiter.SetCore(Request("boss_ch1", true));
		CHECK(P.Frame(S).track == "boss_ch1");
		P.arbiter.SetCore(Request("emerald_plains_explore", false));
		D = P.Frame(S);
		CHECK(D.track == "siege");
		S.siege.then = true;
		D = P.Frame(S);
		CHECK(D.change);
		CHECK(D.track.empty());
		CHECK(D.fadeOutSec == 3.0);
	}

	TEST_CASE("the finale: silence from its first step, six seconds after it, then the lute")
	{
		Player P;
		const int Knell = RegionIndex("knellwatch");
		P.arbiter.SetCore(Request("emerald_plains_explore", true));
		P.Frame(InZone(Knell));
		CHECK(P.sounding == "knellwatch_lute");
		P.arbiter.OnStoryStep(Tables(), "cs_rf_finale", 1, false);  // not the hold's step
		CHECK_FALSE(P.Frame(InZone(Knell)).change);
		P.arbiter.OnStoryStep(Tables(), "cs_rf_finale", 0, true);  // a slide never starts a hold
		CHECK_FALSE(P.arbiter.IsHolding());
		P.arbiter.OnStoryStep(Tables(), "cs_rf_finale", 0, false);
		CHECK(P.arbiter.IsHolding());
		Decision D = P.Frame(InZone(Knell));
		CHECK(D.change);
		CHECK(D.track.empty());
		CHECK(D.fadeOutSec == 3.0);
		CHECK(D.source == Source::Hold);
		// A boss / victory request does not break the story hold.
		P.arbiter.SetCore(Request("emerald_plains_victory", true));
		CHECK_FALSE(P.Frame(InZone(Knell)).change);
		P.arbiter.SetCore(Request("emerald_plains_explore", false));
		P.arbiter.OnStoryEnded(Tables(), "cs_rf_brand", P.now);  // another beat: nothing
		P.arbiter.OnStoryEnded(Tables(), "cs_rf_finale", P.now);
		const double Ended = P.now;
		while (P.now < Ended + 5.9)
		{
			CHECK_FALSE(P.Frame(InZone(Knell)).change);
		}
		P.now = Ended + 6.0;
		D = P.Frame(InZone(Knell));
		CHECK(D.change);
		CHECK(D.track == "knellwatch_lute");
		CHECK(D.fadeInSec == 6.0);
		CHECK_FALSE(P.arbiter.IsHolding());
		// Then the layers rule again: the lute is the region's track, so nothing changes.
		CHECK_FALSE(P.Frame(InZone(Knell)).change);
		CHECK(P.Frame(InZone(-1)).track == "march_drones");
	}

	TEST_CASE("a zone change drops the hold; a story sequence plays the core's track")
	{
		Player P;
		P.arbiter.SetCore(Request("emerald_plains_explore", true));
		P.Frame(InZone(-1));
		P.arbiter.OnStoryStep(Tables(), "cs_rf_finale", 0, false);
		CHECK(P.Frame(InZone(-1)).track.empty());
		P.arbiter.OnZoneChanged();
		CHECK_FALSE(P.arbiter.IsHolding());
		CoreRequest Tower;
		Tower.track = "emerald_plains_explore_score";
		Tower.restart = true;
		P.arbiter.SetCore(Tower);
		CHECK(P.Frame(FrameState{}).track == "emerald_plains_explore_score");
		// Prologue: the sequence's track plays even inside a region.
		Player Q;
		FrameState Seq = InZone(RegionIndex("knellwatch"));
		Seq.sequence = true;
		CoreRequest Prologue;
		Prologue.track = "abyss_rift_explore";
		Prologue.restart = true;
		Q.arbiter.SetCore(Prologue);
		CHECK(Q.Frame(Seq).track == "abyss_rift_explore");
		CHECK_FALSE(Q.Frame(Seq).change);
		// The sequence ends: the core sends the zone's explore track; the hero stands in Knellwatch: the lute.
		Q.arbiter.SetCore(Request("emerald_plains_explore", true));
		const Decision D = Q.Frame(InZone(RegionIndex("knellwatch")));
		CHECK(D.track == "knellwatch_lute");
		CHECK(D.fadeInSec == 4.0);
	}

	TEST_CASE("encounter stingers")
	{
		const Stinger* Hunt = EncounterStinger(Tables(), "hunt_hungerbell", EncounterBeat::Engaged);
		REQUIRE(Hunt != nullptr);
		CHECK(Hunt->id == "stinger_hunt");
		CHECK(Hunt->asset == "SW_STG_Hunt");
		CHECK(Hunt->duckDb == -6.0);
		CHECK(Hunt->duckInSec == 0.1);
		CHECK(Hunt->duckOutSec == 1.2);
		CHECK(EncounterStinger(Tables(), "hunt_harke", EncounterBeat::Slain)->id == "stinger_slain");
		CHECK(EncounterStinger(Tables(), "miniboss_corpseherd", EncounterBeat::Engaged)->id == "stinger_miniboss");
		CHECK(EncounterStinger(Tables(), "miniboss_corpseherd", EncounterBeat::Slain)->id == "stinger_slain");
		CHECK(EncounterStinger(Tables(), "risen_footman", EncounterBeat::Engaged) == nullptr);
		CHECK(EncounterStinger(Tables(), "valdras", EncounterBeat::Slain) == nullptr);  // the core's victory instead
		const abyss::JsonValue& Assets = audio_test::Manifest().Get("assets");
		for (const Stinger& S : Tables().stingers)
		{
			CHECK_MESSAGE(Assets.Has(S.asset), S.id);
		}
	}

	TEST_CASE("boss phase stingers (2G): the event's id, else the manifest's row; none on a reset")
	{
		// The manifest's table (bosses.valdras.phaseStingers {"1", "2"}) and the core's data agree.
		const Boss* Valdras = nullptr;
		for (const Boss& B : Tables().bosses)
		{
			if (B.monster == "valdras")
			{
				Valdras = &B;
			}
		}
		REQUIRE(Valdras != nullptr);
		REQUIRE(Valdras->phaseStingers.size() == 2);
		CHECK(Valdras->phaseStingers[0] == std::pair<int, std::string>(1, "stinger_valdras_roll_call"));
		CHECK(Valdras->phaseStingers[1] == std::pair<int, std::string>(2, "stinger_valdras_no_retreat"));
		const abyss::MonsterDef* Def = audio_test::Data().FindMonster("valdras");
		REQUIRE(Def != nullptr);
		REQUIRE(Def->phases.size() == 2);
		for (std::size_t Row = 0; Row < Def->phases.size(); ++Row)
		{
			const int Phase = static_cast<int>(Row) + 1;   // entering row i makes MonsterInstance::phase i + 1
			CAPTURE(Phase);
			const Stinger* FromEvent = PhaseStinger(Tables(), "valdras", Phase, Def->phases[Row].stinger, false);
			const Stinger* FromTable = PhaseStinger(Tables(), "valdras", Phase, "", false);
			REQUIRE(FromEvent != nullptr);
			REQUIRE(FromTable != nullptr);
			CHECK(FromEvent == FromTable);
			CHECK(FromEvent->id == Def->phases[Row].stinger);
		}
		const Stinger* RollCall = PhaseStinger(Tables(), "valdras", 1, "stinger_valdras_roll_call", false);
		REQUIRE(RollCall != nullptr);
		CHECK(RollCall->asset == "SW_STG_ValdrasRollCall");
		CHECK(RollCall->duckDb == -6.0);
		CHECK(RollCall->duckInSec == 0.1);
		CHECK(PhaseStinger(Tables(), "valdras", 2, "", false)->asset == "SW_STG_ValdrasNoRetreat");
		// A reset (the boss walked home: phase 0) and phase 0 play nothing, even with an id.
		CHECK(PhaseStinger(Tables(), "valdras", 0, "", true) == nullptr);
		CHECK(PhaseStinger(Tables(), "valdras", 1, "stinger_valdras_roll_call", true) == nullptr);
		CHECK(PhaseStinger(Tables(), "valdras", 0, "stinger_valdras_roll_call", false) == nullptr);
		// The event's id wins over the table; an id the director does not know falls back to the table.
		CHECK(PhaseStinger(Tables(), "valdras", 1, "stinger_valdras_no_retreat", false)->id == "stinger_valdras_no_retreat");
		CHECK(PhaseStinger(Tables(), "valdras", 1, "stinger_unknown", false)->id == "stinger_valdras_roll_call");
		// A phase past the table, the zealot's frenzy (no stinger, no row) and an unknown monster: nothing.
		CHECK(PhaseStinger(Tables(), "valdras", 3, "", false) == nullptr);
		const abyss::MonsterDef* Zealot = audio_test::Data().FindMonster("oath_zealot");
		REQUIRE(Zealot != nullptr);
		REQUIRE(Zealot->phases.size() == 1);
		CHECK(Zealot->phases[0].stinger.empty());
		CHECK(PhaseStinger(Tables(), "oath_zealot", 1, Zealot->phases[0].stinger, false) == nullptr);
		CHECK(PhaseStinger(Tables(), "demon_lord", 1, "", false) == nullptr);
		// Every stinger a phase row names exists in the director (and has an asset).
		for (const char* Id : { "valdras", "oath_zealot" })
		{
			for (const abyss::MonsterPhaseDef& Row : audio_test::Data().FindMonster(Id)->phases)
			{
				if (!Row.stinger.empty())
				{
					CHECK_MESSAGE(FindStinger(Tables(), Row.stinger) != nullptr, Row.stinger);
				}
			}
		}
		// Synthetic: a malformed phase key is skipped, a table without phaseStingers parses.
		const DirectorTables Synthetic = Parse(audio_test::Json(R"({"stingers": {"s1": {"asset": "SW_A"}},
			"bosses": {"b": {"track": "t", "phaseStingers": {"x": "s1", "1": "s1", "-2": "s1", "3": ""}}, "c": {"track": "t"}}})"));
		REQUIRE(Synthetic.bosses.size() == 2);
		CHECK(Synthetic.bosses[0].phaseStingers.size() == 1);
		CHECK(PhaseStinger(Synthetic, "b", 1, "", false)->id == "s1");
		CHECK(PhaseStinger(Synthetic, "c", 1, "", false) == nullptr);
	}

	TEST_CASE("a manifest without a music director changes nothing")
	{
		const DirectorTables Empty = Parse(abyss::JsonValue());
		CHECK_FALSE(Empty.loaded);
		Arbiter A;
		CoreRequest R;
		R.track = "emerald_plains_explore";
		R.restart = true;
		A.SetCore(R);
		FrameState S;
		S.inDirectorZone = true;
		S.region = 3;  // out of range of the empty table
		const Decision D = A.Update(Empty, S, DefaultFades{}, "", 0.0);
		CHECK(D.change);
		CHECK(D.track == "emerald_plains_explore");
		CHECK(D.source == Source::Core);
	}
}

TEST_SUITE("audio")
{
	TEST_CASE("music regions read world flags too (2G stage H): no row has an untilFlag today; one would stop on its flag")
	{
		for (const Region& R : Tables().regions)
		{
			CHECK(R.when.untilFlag.empty());
		}
		World W;
		const int Knell = RegionIndex("knellwatch");
		REQUIRE(Knell >= 0);
		W.tracker.Update(Tables().regionAreas, TileOf(15.0, 15.0));
		const auto IsAlive = [&W](std::string_view Id) { return AbyssAudioData::Contains(W.alive, Id); };
		std::vector<std::string> Flags;
		const auto HasFlag = [&Flags](std::string_view Id) { return AbyssAudioData::Contains(Flags, Id); };
		CHECK(PickRegion(Tables(), W.tracker, IsAlive, HasFlag) == Knell);
		CHECK(PickRegion(Tables(), W.tracker, IsAlive) == Knell);   // without flags: as before
		DirectorTables Gated = Tables();
		Gated.regions[static_cast<std::size_t>(Knell)].when.untilFlag = "f";
		CHECK(PickRegion(Gated, W.tracker, IsAlive, HasFlag) == Knell);
		Flags.push_back("f");
		CHECK(PickRegion(Gated, W.tracker, IsAlive, HasFlag) != Knell);
	}
}
