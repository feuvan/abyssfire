// The in-game autotest's smoothness measurement (performance audit F2 / section 6;
// Private/Framework/AbyssSmoothnessRules.h, used by UAbyssAutoTestSubsystem): tags and the "play" rule, the probe frame,
// scene segmentation, percentiles and the counts over 33 / 50 / 100 ms, the game / real ratio and the step cap, the
// longest-frames list, the bounded timeline and its CSV, command latency, the attack-contact error, the thresholds,
// the verdict and the gate, the report.json blocks (null for what the engine did not give), and a real smoke
// playthrough sampled the way the autotest samples the driver's frames.
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "doctest/doctest.h"

#include "abyss/base/Enums.h"
#include "abyss/base/Json.h"
#include "abyss/base/Math.h"
#include "abyss/base/SimClock.h"
#include "abyss/data/DataStore.h"
#include "abyss/sim/GameSim.h"
#include "abyss/sim/Snapshot.h"
#include "abyss/tools/AutoPilot.h"

#include "Framework/AbyssSmoothnessRules.h"
#include "audio_test_data.h"

namespace
{
	using namespace AbyssSmoothnessRules;

	FFrameSample Frame(double RealSec, float FrameMs, std::uint32_t Tags = 0, std::uint16_t Scene = 0)
	{
		FFrameSample S;
		S.RealSec = RealSec;
		S.FrameMs = FrameMs;
		S.Tags = Tags;
		S.Scene = Scene;
		return S;
	}

	/** A session frame: the sim was fed DtMs and ran Steps. */
	FFrameSample SimFrame(double RealSec, float DtMs, int Steps, std::uint32_t Tags = 0, std::uint16_t Scene = 0)
	{
		FFrameSample S = Frame(RealSec, DtMs, Tags, Scene);
		S.SimDtMs = DtMs;
		S.DriverMs = 1.f;
		S.SimMs = 0.5f;
		S.Steps = static_cast<std::uint8_t>(Steps);
		if (StepCapHit(Steps, 4, static_cast<double>(DtMs)))
		{
			S.Tags |= TagStepCap;
		}
		return S;
	}

	int Columns(std::string_view Line)
	{
		int Count = 1;
		for (const char C : Line)
		{
			Count += C == ',' ? 1 : 0;
		}
		return Count;
	}

	abyss::JsonValue Parse(const std::string& Text)
	{
		abyss::JsonValue Root;
		abyss::JsonParseError Error;
		REQUIRE(abyss::ParseJson(Text, Root, &Error));
		return Root;
	}
}

TEST_SUITE("smoothness")
{
	TEST_CASE("tags: the first six take a frame out of play, the others only describe it")
	{
		CHECK(IsPlay(0));
		for (const std::uint32_t Tag : { TagShot, TagZoneBuild, TagCompileWait, TagStory, TagPanel, TagLoading })
		{
			CHECK_FALSE(IsPlay(Tag));
			CHECK((NonPlayMask & Tag) != 0);
		}
		for (const std::uint32_t Tag : { TagBacklog, TagStepCap, TagDilated, TagFrozen, TagQualityRefresh })
		{
			CHECK(IsPlay(Tag));
		}
		CHECK(TagList(0).empty());
		CHECK(TagList(TagShot | TagZoneBuild) == "shot|zoneBuild");
		CHECK(TagList(TagLoading | TagBacklog | TagQualityRefresh) == "loading|backlog|qualityRefresh");
		// every bit has a name, and no name holds a CSV separator
		for (int Bit = 0; Bit < TagCount; ++Bit)
		{
			const std::string Name = TagName(Bit);
			CHECK_FALSE(Name.empty());
			CHECK(Name.find(',') == std::string::npos);
			CHECK(Name.find('|') == std::string::npos);
		}
		CHECK(std::string(TagName(TagCount)).empty());
	}

	TEST_CASE("the probe frame: hooks add counts and inclusive ms, the sample takes them with their tags")
	{
		FProbeFrame Probe;
		Probe.Add(ECounter::Spawns, 1, 2.5);
		Probe.Add(ECounter::Spawns, 1, 1.5);
		Probe.Add(ECounter::ZoneBuilds, 1, 40.0);
		Probe.Add(ECounter::UiTextureLoads, 3, 6.0);
		Probe.Add(ECounter::VfxPoolCreations, 1, 4.0);
		Probe.Add(ECounter::SyncLoads, 2, 10.0);
		Probe.Add(ECounter::MaterialLookups, 5, 1.0);
		Probe.Add(ECounter::HoverProjected, 7);
		Probe.Add(ECounter::LogRebuilds);
		Probe.Add(ECounter::SimFrame, 1, 0.75);
		Probe.Add(ECounter::DriverTick, 1, 3.25);
		Probe.SimDtMs = 16.5;
		Probe.FloatTexts = 12;
		CHECK(Probe.Num(ECounter::Spawns) == 2);
		CHECK(Probe.Millis(ECounter::Spawns) == doctest::Approx(4.0));

		FFrameSample S;
		ApplyProbe(S, Probe, /*bHasSession*/ true);
		CHECK(S.Spawns == 2);
		CHECK(S.SpawnMs == doctest::Approx(4.0));
		CHECK(S.ZoneBuildMs == doctest::Approx(40.0));
		CHECK(S.UiTextureLoads == 3);
		CHECK(S.VfxPoolCreations == 1);
		CHECK(S.SyncLoads == 2);
		CHECK(S.LoadMs == doctest::Approx(21.0));
		CHECK(S.PickProjected == 7);
		CHECK(S.LogRebuilds == 1);
		CHECK(S.SimMs == doctest::Approx(0.75));
		CHECK(S.DriverMs == doctest::Approx(3.25));
		CHECK(S.SimDtMs == doctest::Approx(16.5));
		CHECK(S.FloatTexts == 12);
		CHECK((S.Tags & TagZoneBuild) != 0);
		CHECK((S.Tags & TagCompileWait) == 0);
		CHECK((S.Tags & TagQualityRefresh) == 0);

		// without a session the sim times stay unavailable, not 0
		FFrameSample Menu;
		ApplyProbe(Menu, FProbeFrame{}, /*bHasSession*/ false);
		CHECK(Menu.SimMs < 0.f);
		CHECK(Menu.DriverMs < 0.f);
		CHECK(Menu.FloatTexts == -1);
		CHECK(Menu.Tags == 0);

		// a quality-refresh call that applied nothing is not tagged; one that did is
		FProbeFrame Quality;
		Quality.Add(ECounter::QualityRefreshCalls, 1, 0.01);
		FFrameSample Q;
		ApplyProbe(Q, Quality, true);
		CHECK((Q.Tags & TagQualityRefresh) == 0);
		Quality.Add(ECounter::QualityRefreshApplied);
		Quality.Add(ECounter::CompileWaits, 1, 20000.0);
		ApplyProbe(Q, Quality, true);
		CHECK((Q.Tags & TagQualityRefresh) != 0);
		CHECK((Q.Tags & TagCompileWait) != 0);
		CHECK(Q.CompileWaitMs == doctest::Approx(20000.0));

		// totals and per-frame peaks
		FProbeTotals Totals;
		Totals.Add(Probe);
		Totals.Add(Quality);
		FProbeFrame Busy;
		Busy.Add(ECounter::Spawns, 60, 90.0);
		Totals.Add(Busy);
		CHECK(Totals.N[static_cast<int>(ECounter::Spawns)] == 62);
		CHECK(Totals.Ms[static_cast<int>(ECounter::Spawns)] == doctest::Approx(94.0));
		CHECK(Totals.PeakPerFrame[static_cast<int>(ECounter::Spawns)] == 60);
		CHECK(Totals.PeakMsPerFrame[static_cast<int>(ECounter::Spawns)] == doctest::Approx(90.0));
		CHECK(Totals.N[static_cast<int>(ECounter::QualityRefreshCalls)] == 1);

		// Reset clears everything, and the counter names are distinct and complete
		Probe.Reset();
		CHECK(Probe.Num(ECounter::Spawns) == 0);
		CHECK(Probe.FloatTexts == -1);
		CHECK(Probe.ShaderJobsAtWait == -1);
		std::vector<std::string> Names;
		for (int Index = 0; Index < CounterCount; ++Index)
		{
			const std::string Name = CounterName(static_cast<ECounter>(Index));
			CHECK_FALSE(Name.empty());
			for (const std::string& Other : Names)
			{
				CHECK(Other != Name);
			}
			Names.push_back(Name);
		}
		CHECK(Clamp16(70000u) == 65535);
	}

	TEST_CASE("the step cap: a frame fed a whole step more than the budget lost sim time")
	{
		const double Step = abyss::kSimStepMs;
		CHECK_FALSE(StepCapHit(1, 4, Step));
		CHECK_FALSE(StepCapHit(3, 4, 50.0));
		CHECK_FALSE(StepCapHit(4, 4, 4 * Step));          // exactly the budget: nothing dropped
		CHECK_FALSE(StepCapHit(4, 4, 4.9 * Step));        // may or may not drop a step: not counted
		CHECK(StepCapHit(4, 4, 5 * Step));
		CHECK(StepCapHit(4, 4, 100.0));
		CHECK(StepCapHit(4, 4, 59000.0));
		CHECK_FALSE(StepCapHit(0, 4, 500.0));             // frozen world: no steps, nothing lost
		CHECK_FALSE(StepCapHit(4, 0, 500.0));
		CHECK(LostSimMs(4, 100.0) == doctest::Approx(100.0 - 4 * Step));
		// against the wall clock: what the clock's clamp drops is lost to the player too (the ratio counts it as well)
		CHECK(LostSimMs(4, 59000.0) == doctest::Approx(59000.0 - 4 * Step));
		CHECK(ClampLostMs(59000.0, 250.0) == doctest::Approx(58750.0));
		CHECK(ClampLostMs(250.0, 250.0) == doctest::Approx(0.0));
		CHECK(ClampLostMs(100.0, 250.0) == doctest::Approx(0.0));
		CHECK(ClampLostMs(900.0, 0.0) == doctest::Approx(0.0));
		CHECK(LostSimMs(4, 60.0) == doctest::Approx(0.0));
		// the first and the last frame of a freeze count as frozen
		CHECK(FrozenFrame(true, false));
		CHECK(FrozenFrame(false, true));
		CHECK(FrozenFrame(true, true));
		CHECK_FALSE(FrozenFrame(false, false));
	}

	TEST_CASE("scenes: phase, story, panel, boss, fight, camp, travel in that order")
	{
		FSceneTracker Tracker;
		FSceneInput In;
		const auto Label = [&Tracker](const FSceneInput& Input) { return Tracker.Scenes()[Tracker.Update(Input)].Label; };
		In.Phase = EPhase::Boot;
		CHECK(Label(In) == "boot");
		In.Phase = EPhase::Menu;
		CHECK(Label(In) == "menu");
		In.Phase = EPhase::Starting;
		CHECK(Label(In) == "starting");
		In.Phase = EPhase::Playing;
		In.bStory = true;
		CHECK(Label(In) == "prologue");
		In.bStory = false;
		In.bInSafeZone = true;
		In.bReachedCamp = true;
		CHECK(Label(In) == "camp");
		In.Panel = "inventory";
		CHECK(Label(In) == "panel_inventory");
		In.Panel = "questCard";
		CHECK(Label(In) == "panel_questCard");
		In.Panel = {};
		In.bStory = true;
		CHECK(Label(In) == "story");   // a beat after the camp was reached is not the prologue
		In.bStory = false;
		In.bInSafeZone = false;
		CHECK(Label(In) == "travel");
		In.bInSafeZone = true;
		CHECK(Label(In) == "camp");    // the same label again: one scene per place, not per visit
		CHECK(Tracker.Scenes().size() == 9);
		In.Phase = EPhase::Ending;
		CHECK(Label(In) == "ending");
		// kinds and what counts as in game
		for (const FScene& Scene : Tracker.Scenes())
		{
			const bool bOutside = Scene.Label == "boot" || Scene.Label == "menu" || Scene.Label == "starting";
			CHECK(IsInGameKind(Scene.Kind) == !bOutside);
			CHECK_FALSE(std::string(SceneKindName(Scene.Kind)).empty());
		}
	}

	TEST_CASE("scenes: each fight gets its own label, a short lull does not split one, boss phases are scenes")
	{
		FSceneTracker Tracker;
		FSceneInput In;
		In.Phase = EPhase::Playing;
		In.bReachedCamp = true;
		const auto Label = [&Tracker, &In](double Sec)
		{
			In.RealSec = Sec;
			return Tracker.Scenes()[Tracker.Update(In)].Label;
		};
		CHECK(Label(10.0) == "travel");
		In.bInCombat = true;
		CHECK(Label(11.0) == "fight_001");
		CHECK(Label(12.0) == "fight_001");
		In.bInCombat = false;
		CHECK(Label(12.5) == "travel");
		In.bInCombat = true;
		CHECK(Label(13.5) == "fight_001");   // back in combat within FightGapSec: the same fight
		In.bInCombat = false;
		CHECK(Label(14.0) == "travel");
		In.bInCombat = true;
		CHECK(Label(14.0 + FSceneTracker::FightGapSec + 0.6) == "fight_002");
		CHECK(Tracker.Fights() == 2);
		// a story beat or a panel during a fight is its own scene, and the fight goes on under it
		In.bStory = true;
		CHECK(Label(17.0) == "story");
		In.bStory = false;
		CHECK(Label(17.5) == "fight_002");
		// the boss bar shows: one scene per boss and phase, whatever the fight number
		In.bBoss = true;
		In.BossId = "valdras";
		In.BossPhase = 0;
		CHECK(Label(18.0) == "boss_valdras_p0");
		In.BossPhase = 2;
		CHECK(Label(19.0) == "boss_valdras_p2");
		In.bInCombat = false;   // the bar still shows while the hero is out of combat (walking back in)
		CHECK(Label(19.5) == "travel");
		In.bBoss = false;
		// past MaxFights the fights share one label
		for (int Fight = 0; Fight < FSceneTracker::MaxFights + 5; ++Fight)
		{
			In.bInCombat = true;
			Label(100.0 + Fight * 10.0);
			In.bInCombat = false;
			Label(101.0 + Fight * 10.0);
		}
		In.bInCombat = true;
		CHECK(Label(2000.0) == "fight_more");
		CHECK(Tracker.Fights() > FSceneTracker::MaxFights);
		int FightLabels = 0;
		for (const FScene& Scene : Tracker.Scenes())
		{
			FightLabels += Scene.Kind == ESceneKind::Fight ? 1 : 0;
		}
		CHECK(FightLabels == FSceneTracker::MaxFights + 1);
	}

	TEST_CASE("scenes: the label list stays bounded")
	{
		FSceneTracker Tracker;
		FSceneInput In;
		In.Phase = EPhase::Playing;
		In.bInCombat = true;
		In.bBoss = true;
		std::vector<std::string> Ids;
		for (int Index = 0; Index < 1000; ++Index)
		{
			Ids.push_back("boss" + std::to_string(Index));
		}
		for (const std::string& Id : Ids)
		{
			In.BossId = Id;
			const std::uint16_t Scene = Tracker.Update(In);
			REQUIRE(Scene < Tracker.Scenes().size());
		}
		CHECK(Tracker.Scenes().size() <= FSceneTracker::MaxScenes + 11);
		CHECK(Tracker.Scenes().back().Label == "boss_more");
	}

	TEST_CASE("the loading window: from a transition until the fade-in after the zone was entered")
	{
		FLoadingWindow Loading;
		CHECK_FALSE(Loading.IsLoading(5.0));
		Loading.Begin();
		CHECK(Loading.IsLoading(5.0));
		CHECK(Loading.IsLoading(500.0));   // open until the zone is entered, however long the load
		Loading.ZoneEntered(20.0);
		CHECK(Loading.IsLoading(20.0 + FLoadingWindow::FadeSec));
		CHECK_FALSE(Loading.IsLoading(20.0 + FLoadingWindow::FadeSec + 0.01));
		// a zone entered without a transition (new game) still fades in
		FLoadingWindow Fresh;
		Fresh.ZoneEntered(3.0);
		CHECK(Fresh.IsLoading(3.2));
		CHECK_FALSE(Fresh.IsLoading(4.0));
	}

	TEST_CASE("statistics: nearest-rank percentiles and the counts over 33, 50 and 100 ms")
	{
		std::vector<float> Empty;
		const FStats None = ComputeStats(Empty);
		CHECK(None.Count == 0);
		CHECK(None.P50Ms == 0.0);
		CHECK(None.HitchesPerMinute() == 0.0);

		// 100 frames: 1 .. 100 ms
		std::vector<float> Ramp;
		for (int Ms = 100; Ms >= 1; --Ms)
		{
			Ramp.push_back(static_cast<float>(Ms));
		}
		const FStats S = ComputeStats(Ramp);
		CHECK(S.Count == 100);
		CHECK(S.P50Ms == doctest::Approx(50.0));
		CHECK(S.P95Ms == doctest::Approx(95.0));
		CHECK(S.P99Ms == doctest::Approx(99.0));
		CHECK(S.MaxMs == doctest::Approx(100.0));
		CHECK(S.AvgMs == doctest::Approx(50.5));
		CHECK(S.TotalSec == doctest::Approx(5.05));
		CHECK(S.Over33 == 67);    // 34 .. 100 (33.33 is the line)
		CHECK(S.Over50 == 50);    // strictly over
		CHECK(S.Over100 == 0);
		CHECK(Ramp.front() == 1.f);   // sorted in place

		// the same rule the report's `frames` block always used: ceil(P * N) - 1
		std::vector<float> Few = { 10.f, 20.f, 30.f };
		CHECK(Percentile(Few, 0.5) == doctest::Approx(20.0));
		CHECK(Percentile(Few, 0.95) == doctest::Approx(30.0));
		CHECK(Percentile(Few, 0.0) == doctest::Approx(10.0));
		CHECK(Percentile(Few, 1.0) == doctest::Approx(30.0));

		// hitches per minute of the frames' own time
		std::vector<float> Hitchy(3598, 16.6667f);
		Hitchy.push_back(150.f);
		Hitchy.push_back(101.f);
		const FStats H = ComputeStats(Hitchy);
		CHECK(H.Over100 == 2);
		CHECK(H.HitchesPerMinute() == doctest::Approx(2.0 * 60.0 / H.TotalSec));
		CHECK(H.HitchesPerMinute() == doctest::Approx(2.0).epsilon(0.02));
	}

	TEST_CASE("Summarize: all against play frames, per scene, and the engine values that were not there")
	{
		std::vector<FScene> Scenes = { { "menu", ESceneKind::Menu }, { "camp", ESceneKind::Camp }, { "fight_001", ESceneKind::Fight },
			{ "prologue", ESceneKind::Prologue } };
		std::vector<FFrameSample> Samples;
		double Now = 0.0;
		const auto Add = [&Samples, &Now](FFrameSample S)
		{
			Now += static_cast<double>(S.FrameMs) / 1000.0;
			S.RealSec = Now;
			Samples.push_back(S);
		};
		// menu: 100 frames, one of them the screenshot's 170 s stall
		for (int Index = 0; Index < 99; ++Index)
		{
			Add(Frame(0, 16.f, 0, 0));
		}
		Add(Frame(0, 170000.f, TagShot, 0));
		// prologue: a zone build with the compile wait, then story frames
		Add(SimFrame(0, 59000.f, 0, TagZoneBuild | TagCompileWait | TagLoading | TagStory | TagFrozen, 3));
		for (int Index = 0; Index < 50; ++Index)
		{
			Add(SimFrame(0, 20.f, 0, TagStory | TagFrozen, 3));
		}
		// camp: 200 play frames at 60 fps with a compile backlog on half of them, 2 with GPU numbers
		for (int Index = 0; Index < 200; ++Index)
		{
			FFrameSample S = SimFrame(0, 16.6667f, 1, Index < 100 ? TagBacklog : 0u, 1);
			S.Husks = static_cast<std::int16_t>(Index % 5);
			if (Index < 2)
			{
				S.GameMs = 4.f + Index;
				S.RenderMs = 3.f;
				S.GpuMs = 9.f;
			}
			Add(S);
		}
		// fight: 100 frames at 40 ms (2.4 steps a frame on average: 2, 2, 3, ...) and one 120 ms hitch that hit the cap
		for (int Index = 0; Index < 100; ++Index)
		{
			Add(SimFrame(0, 40.f, Index % 5 == 4 ? 4 : 2, 0, 2));
		}
		Add(SimFrame(0, 120.f, 4, 0, 2));

		const FSummary Sum = Summarize(Samples, Scenes, FSimLimits{});
		CHECK(Sum.Run.All.Count == static_cast<int>(Samples.size()));
		CHECK(Sum.Run.All.MaxMs == doctest::Approx(170000.0));
		CHECK(Sum.Run.Play.MaxMs == doctest::Approx(120.0));          // the stalls are tagged: not play
		CHECK(Sum.Run.Play.Count == 99 + 200 + 101);
		CHECK(Sum.Run.PlayNoBacklog.Count == 99 + 100 + 101);
		CHECK(Sum.Run.TagFrames[0] == 1);                              // shot
		CHECK(Sum.Run.TagFrames[3] == 51);                             // story
		// in game: no menu frame
		CHECK(Sum.InGame.All.Count == 51 + 200 + 101);
		CHECK(Sum.InGame.Play.Count == 301);
		CHECK(Sum.InGame.Play.Over33 == 101);
		CHECK(Sum.InGame.Play.Over100 == 1);
		CHECK(Sum.InGame.Play.P50Ms == doctest::Approx(16.6667).epsilon(0.001));
		CHECK(Sum.InGame.Play.P95Ms == doctest::Approx(40.0));
		// scenes in order of first appearance, each with both groups
		REQUIRE(Sum.Scenes.size() == 4);
		CHECK(Sum.Scenes[0].Label == "menu");
		CHECK(Sum.Scenes[0].Kind == "menu");
		CHECK(Sum.Scenes[0].All.Count == 100);
		CHECK(Sum.Scenes[0].Play.Count == 99);
		CHECK_FALSE(Sum.Scenes[0].HasRatio());                         // no session: no game / real ratio
		const FGroup& Camp = Sum.Scenes[1];
		CHECK(Camp.Label == "camp");
		CHECK(Camp.FirstRealSec < Camp.LastRealSec);
		CHECK(Camp.HasRatio());
		CHECK(Camp.GameRealRatio() == doctest::Approx(1.0).epsilon(0.0001));
		CHECK(Camp.Husks.Peak == 4);
		CHECK(Camp.Husks.Avg() == doctest::Approx(2.0));
		CHECK(Camp.Game.Count == 2);                                   // only the frames that had the value
		CHECK(Camp.Game.AvgMs == doctest::Approx(4.5));
		CHECK(Camp.Gpu.MaxMs == doctest::Approx(9.0));
		CHECK(Camp.Rhi.Count == 0);                                    // never available: written as null
		CHECK(Camp.Sim.Count == 200);
		const FGroup& Fight = Sum.Scenes[2];
		CHECK(Fight.Label == "fight_001");
		CHECK(Fight.StepCapFrames == 1);
		CHECK(Fight.LostSimMs == doctest::Approx(120.0 - 4 * abyss::kSimStepMs));
		// 80 frames x 2 steps + 20 x 4 + the hitch's 4 = 244 steps over 4120 ms
		CHECK(Fight.GameRealRatio() == doctest::Approx(244.0 * abyss::kSimStepMs / 4120.0));
		CHECK(Fight.GameRealRatio() < 1.0);
		const FGroup& Prologue = Sum.Scenes[3];
		CHECK(Prologue.Play.Count == 0);
		CHECK_FALSE(Prologue.HasRatio());                              // frozen frames are not slow motion
		CHECK(Sum.Fights.All.Count == 101);
		CHECK(Sum.Field.All.Count == 200);
		// the last 60 s before the last in-game frame's end: here the whole session (a frame's time is its end, so the
		// 59 s stall that ended 8.6 s before the last frame is inside), and never a menu frame
		CHECK(Sum.Last.All.Count == Sum.InGame.All.Count);
		CHECK(Sum.Last.Kind == "window");
		// longest: 50, longest first
		REQUIRE(Sum.Longest.size() == 50);
		CHECK(Samples[Sum.Longest[0]].FrameMs == doctest::Approx(170000.f));
		CHECK(Samples[Sum.Longest[1]].FrameMs == doctest::Approx(59000.f));
		CHECK(Samples[Sum.Longest[2]].FrameMs == doctest::Approx(120.f));
		for (std::size_t Index = 1; Index < Sum.Longest.size(); ++Index)
		{
			CHECK(Samples[Sum.Longest[Index - 1]].FrameMs >= Samples[Sum.Longest[Index]].FrameMs);
		}
		// fewer frames than the list length
		const std::vector<FFrameSample> Three(Samples.begin(), Samples.begin() + 3);
		CHECK(Summarize(Three, Scenes, FSimLimits{}).Longest.size() == 3);
		CHECK(Summarize({}, Scenes, FSimLimits{}).Run.All.Count == 0);
		// a sample whose scene is unknown still counts for the run
		std::vector<FFrameSample> Stray = { Frame(1.0, 16.f, 0, 99) };
		const FSummary StraySum = Summarize(Stray, Scenes, FSimLimits{});
		CHECK(StraySum.Run.All.Count == 1);
		CHECK(StraySum.Scenes.empty());
	}

	TEST_CASE("the last-60-seconds window covers the end of the session, not the menu before it")
	{
		std::vector<FScene> Scenes = { { "travel", ESceneKind::Travel }, { "menu", ESceneKind::Menu } };
		std::vector<FFrameSample> Samples;
		for (int Index = 0; Index < 2000; ++Index)
		{
			Samples.push_back(SimFrame(Index * 0.1, 100.f, 4, 0, 0));       // 200 s of 10 fps
		}
		for (int Index = 0; Index < 600; ++Index)
		{
			Samples.push_back(SimFrame(200.05 + Index * 0.1, 16.f, 1, 0, 0));   // then 60 s (by timestamp) of good frames
		}
		const FSummary Sum = Summarize(Samples, Scenes, FSimLimits{});
		CHECK(Sum.Last.All.Count == 600);
		CHECK(Sum.Last.All.P95Ms == doctest::Approx(16.0));
		CHECK(Sum.InGame.All.P50Ms == doctest::Approx(100.0));
		CHECK(Sum.Last.Label == "last60s");
	}

	TEST_CASE("the timeline: every frame while it fits, else every slow frame and a stride of the rest")
	{
		std::vector<FFrameSample> Samples;
		for (int Index = 0; Index < 1000; ++Index)
		{
			Samples.push_back(Frame(Index * 0.016, Index % 100 == 7 ? 80.f : 16.f));   // 10 slow frames
		}
		const FTimelinePlan All = PlanTimeline(Samples, 1000);
		CHECK(All.Rows.size() == 1000);
		CHECK(All.Stride == 1);
		CHECK(PlanTimeline(Samples, 0).Rows.empty());

		const FTimelinePlan Bounded = PlanTimeline(Samples, 100);
		CHECK(Bounded.Rows.size() <= 100);
		CHECK(Bounded.Rows.size() >= 95);
		CHECK(Bounded.SlowKept == 10);
		CHECK(Bounded.SlowDropped == 0);
		CHECK(Bounded.Stride == 11);   // 990 ordinary frames into 90 rows
		int Slow = 0;
		for (std::size_t Index = 0; Index < Bounded.Rows.size(); ++Index)
		{
			if (Index > 0)
			{
				CHECK(Bounded.Rows[Index] > Bounded.Rows[Index - 1]);   // ascending, no duplicate
			}
			Slow += Samples[Bounded.Rows[Index]].FrameMs > 33.4f ? 1 : 0;
		}
		CHECK(Slow == 10);

		// more slow frames than half the rows: the longest ones stay
		for (int Index = 0; Index < 1000; ++Index)
		{
			Samples[static_cast<std::size_t>(Index)].FrameMs = 40.f + static_cast<float>(Index % 50);
		}
		const FTimelinePlan Rough = PlanTimeline(Samples, 100);
		CHECK(Rough.Rows.size() == 50);
		CHECK(Rough.SlowKept == 50);
		CHECK(Rough.SlowDropped == 950);
		for (const std::size_t Row : Rough.Rows)
		{
			CHECK(Samples[Row].FrameMs >= 87.f);   // 40 + 47 .. 49: the 60 longest of 1000 are the top three values (20 each)
		}
		const FTimelinePlan One = PlanTimeline(Samples, 1);
		CHECK(One.Rows.size() == 1);
	}

	TEST_CASE("the timeline CSV: one row per frame with the header's columns, empty fields for what is unavailable")
	{
		const std::string Header = TimelineHeader();
		CHECK(Header.back() == '\n');
		const int Wanted = Columns(Header);
		CHECK(Wanted == 33);

		FFrameSample Bare = Frame(12.3456, 16.667f);
		std::string Row;
		AppendTimelineRow(Row, 7, Bare, "menu");
		CHECK(Row.back() == '\n');
		CHECK(Columns(Row) == Wanted);
		// frame, realSec, frameMs, then seven unavailable timings (idle .. driver) as empty fields, simDt 0
		CHECK(Row.rfind("7,12.3456,16.667,,,,,,,,0.000,0,0,,menu,,,", 0) == 0);
		CHECK(Row.substr(Row.size() - 7) == ",0,,,,\n");   // no commands; husks, characters, registry, floatTexts unknown

		FFrameSample Full = SimFrame(99.5, 120.f, 4, TagShot | TagBacklog, 0);
		Full.IdleMs = 0.f;
		Full.GameMs = 30.25f;
		Full.RenderMs = 12.f;
		Full.RhiMs = 2.f;
		Full.GpuMs = 14.5f;
		Full.ShaderJobs = 49;
		Full.AssetBuilds = 0;
		Full.EvLogLines = 6;
		Full.Spawns = 3;
		Full.SpawnMs = 4.5f;
		Full.Husks = 8;
		Full.Characters = 31;
		Full.Registry = 90;
		Full.FloatTexts = 5;
		Row.clear();
		AppendTimelineRow(Row, 123456, Full, "fight_002");
		CHECK(Columns(Row) == Wanted);
		CHECK(Row.rfind("123456,99.5000,120.000,0.000,30.250,12.000,2.000,14.500,0.500,1.000,120.000,4,1,shot|backlog|stepCap,fight_002,49,0,6,", 0) == 0);
		CHECK(Row.substr(Row.size() - 11) == ",8,31,90,5\n");
	}

	TEST_CASE("the nearest milestone to a frame")
	{
		std::vector<FMilestone> Milestones = { { "mainMenu", "title screen", 38.7 }, { "session", "new game", 215.0 },
			{ "zoneEntered", "emerald_plains", 223.3 } };
		CHECK(NearestMilestone({}, 5.0) == -1);
		CHECK(NearestMilestone(Milestones, 0.0) == 0);
		CHECK(NearestMilestone(Milestones, 213.5) == 1);
		CHECK(NearestMilestone(Milestones, 220.0) == 2);
		CHECK(NearestMilestone(Milestones, 9999.0) == 2);
	}

	TEST_CASE("command latency: to the sim frame that applies it and to the end of that engine frame")
	{
		CHECK(ClassifyCommand(abyss::CmdMoveTo{}) == ECommandClass::Move);
		CHECK(ClassifyCommand(abyss::CmdSetMoveInput{}) == ECommandClass::Move);
		CHECK(ClassifyCommand(abyss::CmdAttackTarget{}) == ECommandClass::Attack);
		CHECK(ClassifyCommand(abyss::CmdCastSkill{}) == ECommandClass::Skill);
		CHECK(ClassifyCommand(abyss::CmdDodge{}) == ECommandClass::Dodge);
		CHECK(ClassifyCommand(abyss::CmdUsePotion{}) == ECommandClass::Potion);
		CHECK(ClassifyCommand(abyss::CmdInteract{}) == ECommandClass::Interact);
		CHECK(ClassifyCommand(abyss::CmdPickUp{}) == ECommandClass::Interact);
		CHECK(ClassifyCommand(abyss::CmdStorySkip{}) == ECommandClass::Ui);
		CHECK(ClassifyCommand(abyss::CmdQuestAccept{}) == ECommandClass::Ui);
		// the world-driving classes are exactly the commands the core rejects while frozen
		CHECK(abyss::IsHeroGameplayCommand(abyss::CmdCastSkill{}));
		CHECK_FALSE(abyss::IsHeroGameplayCommand(abyss::CmdStorySkip{}));
		for (int Index = 0; Index < CommandClassCount; ++Index)
		{
			CHECK_FALSE(std::string(CommandClassName(static_cast<ECommandClass>(Index))).empty());
		}

		FCommandTracker Tracker;
		// frame 1: the bot submits at 1.000 (after this frame's sim ran at 0.990); the frame ends at 1.005
		Tracker.OnSimFrame(0.990, 1, false);
		Tracker.Submit(ECommandClass::Skill, 1.000);
		Tracker.OnFrameEnd(1.005);
		CHECK(Tracker.PendingCount() == 1);
		CHECK(Tracker.SimStats(ECommandClass::Skill).Count == 0);   // this frame's sim ran before the command
		// frame 2: no step (a fast frame): the command waits
		Tracker.OnSimFrame(1.010, 0, false);
		Tracker.OnFrameEnd(1.012);
		CHECK(Tracker.PendingCount() == 1);
		// frame 3: a step at 1.020, the frame ends at 1.030
		Tracker.OnSimFrame(1.020, 1, false);
		CHECK(Tracker.SimStats(ECommandClass::Skill).Count == 1);
		CHECK(Tracker.MedianSimMs(ECommandClass::Skill) == doctest::Approx(20.0));
		CHECK(Tracker.FrameStats(ECommandClass::Skill).Count == 0);
		Tracker.OnFrameEnd(1.030);
		CHECK(Tracker.PendingCount() == 0);
		CHECK(Tracker.MedianFrameMs(ECommandClass::Skill) == doctest::Approx(30.0));
		// a UI command while the world is frozen: applied by the next sim frame although it runs no step
		Tracker.Submit(ECommandClass::Ui, 2.000);
		Tracker.OnSimFrame(2.016, 0, true);
		Tracker.OnFrameEnd(2.020);
		CHECK(Tracker.MedianSimMs(ECommandClass::Ui) == doctest::Approx(16.0));
		CHECK(Tracker.MedianFrameMs(ECommandClass::Ui) == doctest::Approx(20.0));
		// a hitch: the sim frame comes 300 ms later
		Tracker.Submit(ECommandClass::Skill, 3.000);
		Tracker.OnSimFrame(3.300, 4, false);
		Tracker.OnFrameEnd(3.320);
		const FValueStats Sim = Tracker.SimStats(ECommandClass::Skill);
		CHECK(Sim.Count == 2);
		CHECK(Sim.MaxMs == doctest::Approx(300.0));
		CHECK(Tracker.FrameStats(ECommandClass::Skill).MaxMs == doctest::Approx(320.0));
		CHECK(Tracker.SubmittedCount(ECommandClass::Skill) == 2);
		CHECK(Tracker.SubmittedCount(ECommandClass::Move) == 0);
		CHECK(Tracker.SimStats(ECommandClass::Move).Count == 0);
		// every class together
		CHECK(Tracker.AllSimStats().Count == 3);
		CHECK(Tracker.AllFrameStats().MaxMs == doctest::Approx(320.0));
		CHECK(Tracker.AllMedianSimMs() == doctest::Approx(20.0));
		CHECK(Tracker.AllMedianFrameMs() == doctest::Approx(30.0));
		CHECK(FCommandTracker().AllSimStats().Count == 0);
	}

	TEST_CASE("the attack contact: the shown pose against the contact beat on the frame the hit is presented")
	{
		const double Step = abyss::kSimStepMs;
		FContactTracker Tracker;
		double Error = 0.0;
		// 60 fps: the swing starts at 1000 with its contact 300 ms in; the step at 1300 resolves it, the frame shows the
		// pose one step behind (alpha 0)
		Tracker.OnHeroAttackAnim(1000.0, 300.0);
		CHECK(Tracker.OnHeroHit(1300.0, 1300.0 - Step, Error));
		CHECK(Error == doctest::Approx(-Step));
		// a slow frame ran 4 steps and the contact was the first of them: the pose is 2.5 steps past it
		Tracker.OnHeroAttackAnim(2000.0, 250.0);
		CHECK(Tracker.OnHeroHit(2250.0 + 3 * Step, 2250.0 + 2.5 * Step, Error));
		CHECK(Error == doctest::Approx(2.5 * Step));
		// an animation without a contact beat is not a swing
		Tracker.OnHeroAttackAnim(3000.0, 0.0);
		CHECK_FALSE(Tracker.OnHeroHit(3010.0, 3000.0, Error));
		// a cleave's second target: no contact of its own
		Tracker.OnHeroAttackAnim(4000.0, 200.0);
		CHECK(Tracker.OnHeroHit(4200.0, 4190.0, Error));
		CHECK_FALSE(Tracker.OnHeroHit(4200.0, 4190.0, Error));
		// a projectile lands long after its release beat: not measured
		Tracker.OnHeroAttackAnim(5000.0, 200.0);
		CHECK_FALSE(Tracker.OnHeroHit(5200.0 + Tracker.MatchWindowMs() + 1.0, 5300.0, Error));
		// a hit cannot precede its contact by more than a step
		Tracker.OnHeroAttackAnim(6000.0, 400.0);
		CHECK_FALSE(Tracker.OnHeroHit(6100.0, 6090.0, Error));
		CHECK(Tracker.OnHeroHit(6400.0, 6395.0, Error));
		CHECK(Error == doctest::Approx(-5.0));
		// 60 ms past the contact: late by more than 33 ms
		Tracker.OnHeroAttackAnim(7000.0, 100.0);
		CHECK(Tracker.OnHeroHit(7170.0, 7160.0, Error));

		const FContactTracker::FResult R = Tracker.Result();
		CHECK(R.Count == 5);
		CHECK(R.Unmatched == 4);
		CHECK(R.MaxAbsMs == doctest::Approx(60.0));
		CHECK(R.LateOver33 == 2);   // 2.5 steps (41.7 ms) and 60 ms
		CHECK(R.MeanMs == doctest::Approx((-Step + 2.5 * Step - 10.0 - 5.0 + 60.0) / 5.0));
		CHECK(R.P50Ms == doctest::Approx(-5.0));
		CHECK(FContactTracker().Result().Count == 0);
	}

	TEST_CASE("thresholds: the defaults, the switch's text, and what it refuses")
	{
		FThresholds T;
		std::string Error;
		CHECK(ParseThresholds("", T, Error));
		CHECK(T.P95Ms == doctest::Approx(33.4));
		CHECK(T.MinFrames == 600);
		CHECK(ParseThresholds("p50:18, p95=30 ,p99:45,hitch:0.5,max:200,ratio:0.99,frames:100", T, Error));
		CHECK(T.P50Ms == doctest::Approx(18.0));
		CHECK(T.P95Ms == doctest::Approx(30.0));
		CHECK(T.P99Ms == doctest::Approx(45.0));
		CHECK(T.HitchesPerMin == doctest::Approx(0.5));
		CHECK(T.MaxMs == doctest::Approx(200.0));
		CHECK(T.MinGameRealRatio == doctest::Approx(0.99));
		CHECK(T.MinFrames == 100);
		// a subset keeps the rest
		FThresholds Subset;
		CHECK(ParseThresholds("p99:70", Subset, Error));
		CHECK(Subset.P99Ms == doctest::Approx(70.0));
		CHECK(Subset.P95Ms == doctest::Approx(33.4));
		// frames: a huge value is clamped before it becomes an int; 0 would grade a run that measured nothing
		FThresholds Huge;
		CHECK(ParseThresholds("frames:1e300", Huge, Error));
		CHECK(Huge.MinFrames == 10000000);
		CHECK(ParseThresholds("frames:1", Huge, Error));
		CHECK(Huge.MinFrames == 1);
		for (const char* Bad : { "p95", "p95:", ":33", "p95:fast", "p95:-1", "fps:60", "p95:33x", "p95:nan", "frames:0", "frames:0.5" })
		{
			CAPTURE(Bad);
			FThresholds Scratch;
			Error.clear();
			CHECK_FALSE(ParseThresholds(Bad, Scratch, Error));
			CHECK_FALSE(Error.empty());
		}
	}

	TEST_CASE("the verdict: smooth, uneven on a failing tail, rough on the median, p95 or the ratio; the gate")
	{
		const FThresholds T;
		const auto GroupOf = [](int Good, int Slow40, int Hitch150, double Ratio)
		{
			std::vector<FFrameSample> Samples;
			double Now = 0.0;
			const auto Push = [&Samples, &Now, Ratio](float Ms)
			{
				Now += static_cast<double>(Ms) / 1000.0;
				FFrameSample S = Frame(Now, Ms);
				S.DriverMs = 1.f;
				S.SimDtMs = 1000.f;   // the ratio is steps x step / fed: 60 steps of 1000 ms = 1
				S.Steps = static_cast<std::uint8_t>(std::lround(60.0 * Ratio));
				Samples.push_back(S);
			};
			for (int Index = 0; Index < Good; ++Index) { Push(16.6667f); }
			for (int Index = 0; Index < Slow40; ++Index) { Push(40.f); }
			for (int Index = 0; Index < Hitch150; ++Index) { Push(150.f); }
			const std::vector<FScene> Scenes = { { "travel", ESceneKind::Travel } };
			return Summarize(Samples, Scenes, FSimLimits{}).InGame;
		};
		// 60 fps with nothing else
		const FVerdict Smooth = Judge(GroupOf(6000, 0, 0, 1.0), T);
		CHECK(Smooth.Grade == "smooth");
		CHECK(Smooth.IsSmooth());
		CHECK(Smooth.PlayFrames == 6000);
		REQUIRE(Smooth.Checks.size() == 6);
		for (const FCheck& Check : Smooth.Checks)
		{
			CHECK(Check.bOk);
		}
		CHECK(GateFailure(Smooth, true).empty());
		// 3 % of the frames at 40 ms: every limit still holds (p99 is 40 ms)
		CHECK(Judge(GroupOf(5820, 180, 0, 1.0), T).Grade == "smooth");
		// 80 hitches of 150 ms among them: the median and p95 hold, the tail does not
		const FVerdict Uneven = Judge(GroupOf(5820, 100, 80, 1.0), T);
		CHECK(Uneven.Grade == "uneven");
		CHECK(Uneven.Checks[0].bOk);
		CHECK(Uneven.Checks[1].bOk);
		CHECK_FALSE(Uneven.Checks[2].bOk);   // p99 150 ms
		CHECK_FALSE(Uneven.Checks[3].bOk);   // hitches per minute
		CHECK(Uneven.Checks[4].bOk);         // the longest frame is under 250 ms
		CHECK(GateFailure(Uneven, false).empty());   // informational without the switch
		CHECK(GateFailure(Uneven, true).rfind("smoothness gate: uneven: p99Ms 150.00 (limit 50.00), hitchesPerMin ", 0) == 0);
		// half speed: the step cap turned the game into slow motion
		const FVerdict Slow = Judge(GroupOf(6000, 0, 0, 0.5), T);
		CHECK(Slow.Grade == "rough");
		CHECK_FALSE(Slow.Checks[5].bOk);
		CHECK(Slow.Checks[5].Value == doctest::Approx(0.5));
	}

	TEST_CASE("the verdict's grades, case by case")
	{
		const FThresholds T;
		const auto Make = [](double P50, double P95, double P99, double Max, int Over100, int Count, double Ratio, bool bRatio)
		{
			FGroup G;
			G.Play.Count = Count;
			G.Play.P50Ms = P50;
			G.Play.P95Ms = P95;
			G.Play.P99Ms = P99;
			G.Play.MaxMs = Max;
			G.Play.Over100 = Over100;
			G.Play.TotalSec = 600.0;
			if (bRatio)
			{
				G.RatioRealMs = 1000.0;
				G.RatioSimMs = 1000.0 * Ratio;
			}
			return G;
		};
		CHECK(Judge(Make(16.7, 17.5, 22.0, 48.0, 0, 30000, 1.0, true), T).Grade == "smooth");
		CHECK(Judge(Make(16.7, 17.5, 22.0, 48.0, 10, 30000, 1.0, true), T).Grade == "smooth");     // 1 hitch a minute is the limit
		CHECK(Judge(Make(16.7, 17.5, 22.0, 240.0, 11, 30000, 1.0, true), T).Grade == "uneven");   // 1.1 a minute
		CHECK(Judge(Make(16.7, 17.5, 60.0, 90.0, 0, 30000, 1.0, true), T).Grade == "uneven");     // p99
		CHECK(Judge(Make(16.7, 17.5, 22.0, 900.0, 1, 30000, 1.0, true), T).Grade == "uneven");    // one long frame
		CHECK(Judge(Make(39.9, 270.0, 637.0, 9000.0, 400, 2300, 0.4, true), T).Grade == "rough"); // the first Mac run
		CHECK(Judge(Make(33.3, 33.4, 34.0, 40.0, 0, 30000, 1.0, true), T).Grade == "rough");      // a steady 30 fps: the median
		CHECK(Judge(Make(16.7, 40.0, 45.0, 48.0, 0, 30000, 1.0, true), T).Grade == "rough");      // p95
		CHECK(Judge(Make(16.7, 17.5, 22.0, 48.0, 0, 30000, 0.9, true), T).Grade == "rough");      // slow motion
		CHECK(Judge(Make(16.7, 17.5, 22.0, 48.0, 0, 30000, 0.0, false), T).Grade == "smooth");    // no ratio measured: not held against it
		CHECK(Judge(Make(16.7, 17.5, 22.0, 48.0, 0, 599, 1.0, true), T).Grade == "notMeasured");
		CHECK(Judge(Make(0, 0, 0, 0, 0, 0, 0, false), T).Grade == "notMeasured");

		// the checks carry value, limit and rule
		const FVerdict Rough = Judge(Make(39.9, 270.0, 637.0, 9000.0, 400, 2300, 0.4, true), T);
		REQUIRE(Rough.Checks.size() == 6);
		CHECK(std::string(Rough.Checks[0].Name) == "p50Ms");
		CHECK_FALSE(Rough.Checks[0].bOk);
		CHECK_FALSE(Rough.Checks[0].bTail);
		CHECK(std::string(Rough.Checks[3].Name) == "hitchesPerMin");
		CHECK(Rough.Checks[3].Value == doctest::Approx(40.0));
		CHECK(Rough.Checks[3].bTail);
		CHECK(std::string(Rough.Checks[5].Name) == "gameRealRatio");
		CHECK_FALSE(Rough.Checks[5].bAtMost);
		CHECK(Rough.Checks[5].Value == doctest::Approx(0.4));
		// the gate
		CHECK(GateFailure(Rough, false).empty());
		const std::string Reason = GateFailure(Rough, true);
		CHECK(Reason.rfind("smoothness gate: rough: p50Ms 39.90 (limit 20.00), p95Ms 270.00 (limit 33.40)", 0) == 0);
		CHECK(Reason.find("gameRealRatio 0.40 (limit 0.97)") != std::string::npos);
		const FVerdict Few = Judge(Make(16.7, 17.5, 22.0, 48.0, 0, 12, 1.0, true), T);
		CHECK(GateFailure(Few, true) == "smoothness gate: notMeasured (12 play frames)");
		// tuned limits change the grade
		FThresholds Loose;
		std::string Error;
		REQUIRE(ParseThresholds("p50:34,p95:40", Loose, Error));
		CHECK(Judge(Make(33.3, 33.4, 34.0, 40.0, 0, 30000, 1.0, true), Loose).Grade == "smooth");
	}

	TEST_CASE("report.json: the group, the longest frame and the smoothness block, null where the engine gave nothing")
	{
		std::vector<FScene> Scenes = { { "camp", ESceneKind::Camp } };
		std::vector<FFrameSample> Samples;
		for (int Index = 0; Index < 700; ++Index)
		{
			FFrameSample S = SimFrame((Index + 1) * 0.0166667, 16.6667f, 1, 0, 0);
			S.Husks = 3;
			Samples.push_back(S);
		}
		FFrameSample Hitch = SimFrame(700 * 0.0166667 + 0.4, 400.f, 4, TagZoneBuild | TagLoading, 0);
		Hitch.ZoneBuildMs = 310.f;
		Hitch.Spawns = 64;
		Hitch.SpawnMs = 70.f;
		Hitch.ShaderJobs = 49;
		Samples.push_back(Hitch);
		const FSummary Sum = Summarize(Samples, Scenes, FSimLimits{});
		const std::vector<FMilestone> Milestones = { { "zoneEntered", "emerald_plains", Hitch.RealSec - 0.25 } };

		abyss::JsonWriter W(true);
		W.StartObject();
		W.Key("group");
		WriteGroup(W, Sum.InGame);
		W.Key("longest");
		W.StartArray();
		for (const std::size_t Index : Sum.Longest)
		{
			WriteLongestFrame(W, Index, Samples[Index], Scenes, Milestones);
		}
		W.EndArray();
		const FVerdict Verdict = Judge(Sum.InGame, FThresholds{});
		WriteSmoothness(W, Verdict, FThresholds{}, /*bGate*/ true, /*bShotsOff*/ false);
		W.EndObject();
		const std::string Text = W.Take();
		REQUIRE_FALSE(Text.empty());
		const abyss::JsonValue Root = Parse(Text);

		const abyss::JsonValue& Group = Root.Get("group");
		CHECK(Group.Get("label").AsString() == "inGame");
		CHECK(Group.Get("all").Get("count").AsInt() == 701);
		CHECK(Group.Get("play").Get("count").AsInt() == 700);
		CHECK(Group.Get("play").Get("p50Ms").AsDouble() == doctest::Approx(16.67));
		CHECK(Group.Get("play").Get("over33Ms").AsInt() == 0);
		CHECK(Group.Get("all").Get("over100Ms").AsInt() == 1);
		CHECK(Group.Get("playNoBacklog").Get("count").AsInt() == 700);
		CHECK(Group.Get("tagFrames").Get("zoneBuild").AsInt() == 1);
		CHECK(Group.Get("tagFrames").Get("shot").AsInt() == 0);
		CHECK(Group.Get("tagFrames").Size() == static_cast<size_t>(TagCount));
		CHECK(Group.Get("gameRealRatio").IsNumber());
		CHECK(Group.Get("stepCapFrames").AsInt() == 0);                       // the zone build's frame is not play
		CHECK(Group.Get("lostSimSec").AsDouble() == doctest::Approx(0.0));
		CHECK(Group.Get("lostToClampSec").AsDouble() == doctest::Approx(0.0));
		CHECK(Group.Get("simAllFrames").Get("stepCapFrames").AsInt() == 1);
		CHECK(Group.Get("simAllFrames").Get("lostSimSec").AsDouble() == doctest::Approx((400.0 - 4 * abyss::kSimStepMs) / 1000.0).epsilon(0.01));
		CHECK(Group.Get("simAllFrames").Get("gameRealRatio").IsNumber());
		const abyss::JsonValue& Threads = Group.Get("threads");
		CHECK(Threads.Get("game").IsNull());     // no stat unit numbers: null, never 0
		CHECK(Threads.Get("render").IsNull());
		CHECK(Threads.Get("rhi").IsNull());
		CHECK(Threads.Get("gpu").IsNull());
		CHECK(Threads.Get("idle").IsNull());
		CHECK(Threads.Get("sim").IsObject());
		CHECK(Threads.Get("sim").Get("avgMs").AsDouble() == doctest::Approx(0.5));
		CHECK(Group.Get("load").Get("husks").Get("peak").AsInt() == 3);
		CHECK(Group.Get("load").Get("floatTexts").IsNull());

		const abyss::JsonValue& Longest = Root.Get("longest");
		REQUIRE(Longest.Size() == 50);
		const abyss::JsonValue& First = Longest.At(0);
		CHECK(First.Get("frameMs").AsDouble() == doctest::Approx(400.0));
		CHECK(First.Get("frame").AsInt() == 700);
		CHECK(First.Get("scene").AsString() == "camp");
		CHECK_FALSE(First.Get("play").AsBool(true));
		REQUIRE(First.Get("tags").Size() == 3);
		CHECK(First.Get("tags").At(0).AsString() == "zoneBuild");
		CHECK(First.Get("tags").At(1).AsString() == "loading");
		CHECK(First.Get("tags").At(2).AsString() == "stepCap");
		CHECK(First.Get("stepCap").AsBool());
		CHECK(First.Get("gameMs").IsNull());
		CHECK(First.Get("gpuMs").IsNull());
		CHECK(First.Get("shaderJobs").AsInt() == 49);
		CHECK(First.Get("assetBuilds").IsNull());
		CHECK(First.Get("nearestMilestone").Get("kind").AsString() == "zoneEntered");
		CHECK(First.Get("nearestMilestone").Get("deltaSec").AsDouble() == doctest::Approx(0.25));
		CHECK(First.Get("events").Get("spawns").AsInt() == 64);
		CHECK(First.Get("events").Get("zoneBuildMs").AsDouble() == doctest::Approx(310.0));
		CHECK_FALSE(First.Get("events").Has("uiTextureLoads"));   // only what happened
		CHECK(Longest.At(1).Get("events").Size() == 0);
		CHECK(Longest.At(1).Get("play").AsBool());
		CHECK(Longest.At(1).Get("load").Get("husks").AsInt() == 3);

		const abyss::JsonValue& Smooth = Root.Get("smoothness");
		CHECK(Smooth.Get("verdict").AsString() == "smooth");
		CHECK(Smooth.Get("gate").AsBool());
		CHECK_FALSE(Smooth.Get("gateFailed").AsBool(true));
		CHECK(Smooth.Get("playFrames").AsInt() == 700);
		REQUIRE(Smooth.Get("checks").Size() == 6);
		CHECK(Smooth.Get("checks").At(0).Get("name").AsString() == "p50Ms");
		CHECK(Smooth.Get("checks").At(0).Get("rule").AsString() == "atMost");
		CHECK(Smooth.Get("checks").At(0).Get("ok").AsBool());
		CHECK(Smooth.Get("checks").At(5).Get("rule").AsString() == "atLeast");
		CHECK(Smooth.Get("notes").Size() == 1);   // screenshots were on

		// a run with no session frames: no value is invented
		abyss::JsonWriter Empty(false);
		Empty.StartObject();
		Empty.Key("group");
		WriteGroup(Empty, Summarize({}, Scenes, FSimLimits{}).InGame);
		const FVerdict None = Judge(Summarize({}, Scenes, FSimLimits{}).InGame, FThresholds{});
		WriteSmoothness(Empty, None, FThresholds{}, true, true);
		Empty.EndObject();
		const abyss::JsonValue EmptyRoot = Parse(Empty.Take());
		CHECK(EmptyRoot.Get("group").Get("play").Get("p50Ms").IsNull());
		CHECK(EmptyRoot.Get("group").Get("gameRealRatio").IsNull());
		CHECK(EmptyRoot.Get("smoothness").Get("verdict").AsString() == "notMeasured");
		CHECK(EmptyRoot.Get("smoothness").Get("gateFailed").AsBool());
		CHECK(EmptyRoot.Get("smoothness").Get("checks").At(0).Get("value").IsNull());
		CHECK(EmptyRoot.Get("smoothness").Get("checks").At(5).Get("value").IsNull());
	}

	TEST_CASE("a smoke playthrough sampled as the autotest samples it: scenes, the ratio, command latency, the contact")
	{
		const abyss::DataStore& Data = audio_test::Data();
		struct FRun
		{
			FSummary Summary;
			std::vector<FScene> Scenes;
			FContactTracker::FResult Contact;
			double ToSimMs = 0.0;
			double ToFrameMs = 0.0;
			int Measured = 0;
			int Fights = 0;
			bool bOk = false;
		};
		const auto Play = [&Data](double FrameMs)
		{
			FRun Out;
			std::unique_ptr<abyss::GameSim> Sim = abyss::GameSim::Create(Data, abyss::SimConfig{});
			REQUIRE(Sim != nullptr);
			REQUIRE(Sim->NewGame(abyss::ClassId::Warrior, abyss::Difficulty::Normal, 404));
			FSceneTracker Tracker;
			FCommandTracker Commands;
			FContactTracker Contacts;
			FLoadingWindow Loading;
			std::vector<FFrameSample> Samples;
			double RealSec = 0.0;
			bool bReachedCamp = false;
			abyss::AutoPilotConfig Config;
			Config.cls = abyss::ClassId::Warrior;
			Config.scenario = abyss::AutoPilotScenario::Smoke;
			Config.storyMode = abyss::AutoPilotStoryMode::SkipAll;
			abyss::GameSim& SimRef = *Sim;
			abyss::AutoPilot Pilot(Data, Config, [&SimRef, &Commands, &RealSec](const abyss::Command& Command)
			{
				Commands.Submit(ClassifyCommand(Command), RealSec);
				SimRef.Submit(Command);
			});
			// UAbyssAutoTestSubsystem::ObservePerfEvent
			const auto Observe = [&Contacts, &Loading, &RealSec](const abyss::Event& Event, double SimNowMs, double RenderSimMs)
			{
				if (const abyss::EvPlayAnim* Anim = std::get_if<abyss::EvPlayAnim>(&Event))
				{
					if (Anim->entity == abyss::kHeroEntityId && Anim->action == abyss::AnimAction::Attack)
					{
						Contacts.OnHeroAttackAnim(Anim->startMs, Anim->contactMs, Anim->clip);
					}
				}
				else if (const abyss::EvHit* Hit = std::get_if<abyss::EvHit>(&Event))
				{
					if (Hit->source == abyss::kHeroEntityId && !Hit->tick)
					{
						double Error = 0.0;
						Contacts.OnHeroHit(SimNowMs, RenderSimMs, Error, Hit->skillId);
					}
				}
				else if (const abyss::EvZone* Zone = std::get_if<abyss::EvZone>(&Event))
				{
					if (Zone->phase == abyss::EvZone::Phase::TransitionBegan)
					{
						Loading.Begin();
					}
					else if (Zone->phase == abyss::EvZone::Phase::Entered)
					{
						Loading.ZoneEntered(RealSec);
					}
				}
			};
			for (const abyss::Event& E : Sim->Events())
			{
				Pilot.ObserveEvent(E);
				Observe(E, 0.0, 0.0);
			}
			bool bFinished = false;
			bool bFrozenBefore = false;
			for (int64_t Guard = 0; Guard < 40 * 60 * 60 && !bFinished; ++Guard)
			{
				const abyss::AutoPilotStatus Status = Pilot.Advance(Sim->View());
				if (Status == abyss::AutoPilotStatus::Finished)
				{
					bFinished = true;
					break;
				}
				if (Status == abyss::AutoPilotStatus::Checkpoint)
				{
					bReachedCamp = bReachedCamp || Pilot.CurrentCheckpoint() == "intro";
					continue;
				}
				Pilot.PrepareFrame(Sim->View());
				// the next engine frame: the frame cap sleeps first, the sim runs late in it, the frame ends FrameMs later
				const double SimSec = RealSec + 0.8 * FrameMs / 1000.0;
				const int32_t Steps = Sim->Frame(FrameMs);
				const abyss::Snapshot& View = Sim->View();
				const double Alpha = View.interpolationAlpha < 0.0 ? 0.0 : (View.interpolationAlpha > 1.0 ? 1.0 : View.interpolationAlpha);
				const double RenderSimMs = View.simNowMs - (1.0 - Alpha) * abyss::kSimStepMs;   // UAbyssSimDriver::Tick
				for (const abyss::Event& E : Sim->Events())
				{
					Pilot.ObserveEvent(E);
					Observe(E, View.simNowMs, RenderSimMs);
				}
				Commands.OnSimFrame(SimSec, Steps, View.frozen);
				RealSec += FrameMs / 1000.0;
				Commands.OnFrameEnd(RealSec);

				FFrameSample S = SimFrame(RealSec, static_cast<float>(FrameMs), Steps);
				S.Tags |= (View.storyBusy || View.cinematic) ? TagStory : 0u;
				S.Tags |= FrozenFrame(bFrozenBefore, View.frozen) ? TagFrozen : 0u;
				bFrozenBefore = View.frozen;
				S.Tags |= !View.modals.empty() ? TagPanel : 0u;
				S.Tags |= Loading.IsLoading(RealSec) ? TagLoading : 0u;
				FSceneInput In;
				In.Phase = EPhase::Playing;
				In.RealSec = RealSec;
				In.bStory = View.storyBusy || View.cinematic;
				In.bReachedCamp = bReachedCamp;
				const std::string Panel = View.modals.empty() ? std::string() : std::string(abyss::EnumName(View.modals.front()));
				In.Panel = Panel;
				for (const abyss::Vec2& Camp : View.zone.camps)
				{
					In.bInSafeZone = In.bInSafeZone || abyss::Dist(Camp, View.hero.pos) <= View.zone.safeZoneRadius;
				}
				In.bInCombat = View.hero.inCombat;
				In.bBoss = View.bossBar.show;
				S.Scene = Tracker.Update(In);
				Samples.push_back(S);
				Pilot.EndFrame(View, Steps);
				// the bot acts after the frame, as from the router's OnFrame (the next Advance submits at RealSec)
			}
			Out.bOk = bFinished && Pilot.Ok() && Pilot.Reached(abyss::AutoPilotMilestone::ScenarioDone);
			Out.Scenes = Tracker.Scenes();
			Out.Summary = Summarize(Samples, Out.Scenes, FSimLimits{});
			Out.Contact = Contacts.Result();
			Out.ToSimMs = Commands.AllMedianSimMs();
			Out.ToFrameMs = Commands.AllMedianFrameMs();
			Out.Measured = Commands.AllFrameStats().Count;
			Out.Fights = Tracker.Fights();
			return Out;
		};

		const FRun At60 = Play(abyss::kSimStepMs);
		REQUIRE(At60.bOk);
		const auto Find = [](const FRun& Run, std::string_view Label) -> const FGroup*
		{
			for (const FGroup& Group : Run.Summary.Scenes)
			{
				if (Group.Label == Label)
				{
					return &Group;
				}
			}
			return nullptr;
		};
		for (const FGroup& Group : At60.Summary.Scenes)
		{
			MESSAGE("60 fps scene " << Group.Label << ": " << Group.All.Count << " frames, " << Group.Play.Count << " play, ratio "
				<< (Group.HasRatio() ? Group.GameRealRatio() : -1.0));
		}
		// the scenes the smoke scenario passes through
		REQUIRE(Find(At60, "prologue") != nullptr);
		REQUIRE(Find(At60, "camp") != nullptr);
		REQUIRE(Find(At60, "travel") != nullptr);
		REQUIRE(Find(At60, "fight_001") != nullptr);
		CHECK(Find(At60, "panel_questCard") != nullptr);
		// the trench hunt and the Seep: two fights at least (stage H: with the burial detail at (30,24), spawn `at`, the
		// bot clears the trench without a 2 s lull, so it is one fight; the pin was >= 3 with the detail at (25,20))
		CHECK(At60.Fights >= 2);
		CHECK(Find(At60, "prologue")->Play.Count == 0);           // story frames are never play
		CHECK(Find(At60, "camp")->Play.Count > 60);
		CHECK(Find(At60, "travel")->Play.Count > 60);
		CHECK(At60.Summary.Fights.Play.Count > 200);
		// exactly 60 fps: one step a frame, nothing lost
		CHECK(At60.Summary.InGame.GameRealRatio() == doctest::Approx(1.0).epsilon(0.002));
		CHECK(At60.Summary.InGame.StepCapFrames == 0);
		CHECK(Judge(At60.Summary.InGame, FThresholds{}).Grade == "smooth");
		// every scene of the world in play runs at full speed (the frames around a freeze are not held against it)
		CHECK(Find(At60, "camp")->GameRealRatio() == doctest::Approx(1.0).epsilon(0.002));
		CHECK(Find(At60, "travel")->GameRealRatio() == doctest::Approx(1.0).epsilon(0.002));
		// a command the bot submits after a frame reaches the sim in the next frame, and the screen at its end
		MESSAGE("60 fps commands: " << At60.Measured << " measured, to sim " << At60.ToSimMs << " ms, to frame end " << At60.ToFrameMs);
		CHECK(At60.Measured > 50);
		CHECK(At60.ToSimMs == doctest::Approx(0.8 * abyss::kSimStepMs).epsilon(0.01));
		CHECK(At60.ToFrameMs == doctest::Approx(abyss::kSimStepMs).epsilon(0.01));
		// the warrior's swings: the hit shows on the contact pose, the pose at most one step behind
		MESSAGE("60 fps contact: " << At60.Contact.Count << " hits, mean " << At60.Contact.MeanMs << " ms, p95 abs "
			<< At60.Contact.P95AbsMs << ", max abs " << At60.Contact.MaxAbsMs << ", unmatched " << At60.Contact.Unmatched);
		CHECK(At60.Contact.Count > 20);
		CHECK(At60.Contact.MaxAbsMs <= abyss::kSimStepMs + 0.001);
		CHECK(At60.Contact.MeanMs <= 0.0);
		CHECK(At60.Contact.LateOver33 == 0);

		// 20 fps (the core tests play the smoke scenario there too): three steps a frame, no time lost, yet rough,
		// and the contact pose is up to three steps off
		const FRun At20 = Play(50.0);
		REQUIRE(At20.bOk);
		CHECK(At20.Summary.InGame.GameRealRatio() == doctest::Approx(1.0).epsilon(0.01));
		CHECK(At20.Summary.InGame.StepCapFrames == 0);
		CHECK(Judge(At20.Summary.InGame, FThresholds{}).Grade == "rough");
		CHECK(At20.Contact.Count > 20);
		CHECK(At20.Contact.MaxAbsMs > At60.Contact.MaxAbsMs);
		CHECK(At20.Contact.MaxAbsMs <= 50.0 + 0.001);
		CHECK(At20.ToSimMs == doctest::Approx(40.0).epsilon(0.01));
		CHECK(At20.ToFrameMs == doctest::Approx(50.0).epsilon(0.01));

		// 10 fps: the sim runs 4 of the 6 steps a frame is worth: slow motion, measured as a ratio of 2/3
		std::unique_ptr<abyss::GameSim> Slow = abyss::GameSim::Create(Data, abyss::SimConfig{});
		REQUIRE(Slow != nullptr);
		REQUIRE(Slow->NewGame(abyss::ClassId::Warrior, abyss::Difficulty::Normal, 404));
		std::vector<FFrameSample> SlowSamples;
		const std::vector<FScene> OneScene = { { "travel", ESceneKind::Travel } };
		for (int Index = 0; Index < 600; ++Index)
		{
			const int32_t Steps = Slow->Frame(100.0);
			FFrameSample S = SimFrame((Index + 1) * 0.1, 100.f, Steps);
			S.Tags |= FrozenFrame(Index > 0 && (SlowSamples.back().Tags & TagFrozen) != 0, Slow->View().frozen) ? TagFrozen : 0u;
			SlowSamples.push_back(S);
		}
		const FSummary SlowSum = Summarize(SlowSamples, OneScene, FSimLimits{});
		if (SlowSum.InGame.HasRatio())   // the prologue freezes the world for a while: only unfrozen frames count
		{
			CHECK(SlowSum.InGame.GameRealRatio() == doctest::Approx(4.0 * abyss::kSimStepMs / 100.0).epsilon(0.001));
			CHECK(SlowSum.InGame.StepCapFrames > 0);
			CHECK(SlowSum.InGame.LostSimMs == doctest::Approx(SlowSum.InGame.StepCapFrames * (100.0 - 4.0 * abyss::kSimStepMs)));
		}
	}

	TEST_CASE("the game / real ratio judges play: the frame that swallows a tagged frame's hitch is not slow motion")
	{
		// The engine feeds frame N the duration of frame N - 1: after a zone build, a compile wait or a screenshot the
		// next frame is fed the whole stall and runs its step budget.
		const std::vector<FScene> Scenes = { { "travel", ESceneKind::Travel } };
		const float Dt = static_cast<float>(abyss::kSimStepMs);
		const auto Play = [Dt](std::vector<FFrameSample>& Out, int Count)
		{
			for (int Index = 0; Index < Count; ++Index)
			{
				Out.push_back(SimFrame(0.0, Dt, 1));
			}
		};
		const auto Stamp = [](std::vector<FFrameSample>& List)
		{
			double Now = 0.0;
			for (FFrameSample& S : List)
			{
				Now += static_cast<double>(S.FrameMs) / 1000.0;
				S.RealSec = Now;
			}
		};
		CHECK(RatioFrame(0u, 0u));
		CHECK(RatioFrame(TagBacklog | TagStepCap, TagQualityRefresh));
		CHECK_FALSE(RatioFrame(0u, TagShot));
		CHECK_FALSE(RatioFrame(0u, TagCompileWait));
		CHECK_FALSE(RatioFrame(TagLoading, 0u));
		CHECK_FALSE(RatioFrame(TagPanel, TagPanel));

		// 90 s of perfect play
		std::vector<FFrameSample> Clean;
		Play(Clean, 5400);
		Stamp(Clean);
		const FSummary CleanSum = Summarize(Clean, Scenes, FSimLimits{});
		CHECK(Judge(CleanSum.InGame, FThresholds{}).Grade == "smooth");

		// the same with two zone re-entries of 1.5 s (the build frame, then a loading frame fed 1500 ms: 4 steps), a
		// 3 s compile wait whose hitch lands on an UNTAGGED frame, and 30 screenshots of 120 ms
		std::vector<FFrameSample> Run;
		Play(Run, 1000);
		for (int Entry = 0; Entry < 2; ++Entry)
		{
			FFrameSample Build = SimFrame(0.0, Dt, 1, TagZoneBuild | TagLoading);
			Build.FrameMs = 1500.f;
			Run.push_back(Build);
			FFrameSample After = SimFrame(0.0, 1500.f, 4, TagLoading);
			After.FrameMs = Dt;
			Run.push_back(After);
			Play(Run, 1000);
		}
		FFrameSample Wait = SimFrame(0.0, Dt, 1, TagCompileWait);
		Wait.FrameMs = 3000.f;
		Run.push_back(Wait);
		FFrameSample AfterWait = SimFrame(0.0, 3000.f, 4);   // past the loading window: no tag of its own
		AfterWait.FrameMs = Dt;
		Run.push_back(AfterWait);
		Play(Run, 1000);
		for (int Shot = 0; Shot < 30; ++Shot)
		{
			FFrameSample Draw = SimFrame(0.0, Dt, 1, TagShot);
			Draw.FrameMs = 120.f;
			Run.push_back(Draw);
			FFrameSample ReadBack = SimFrame(0.0, 120.f, 4, TagShot);
			ReadBack.FrameMs = Dt;
			Run.push_back(ReadBack);
			Play(Run, 40);
		}
		Stamp(Run);
		const FSummary Sum = Summarize(Run, Scenes, FSimLimits{});
		const FGroup& G = Sum.InGame;
		CHECK(G.Play.MaxMs == doctest::Approx(abyss::kSimStepMs));
		CHECK(G.Play.Over100 == 0);
		// play: nothing was slow
		CHECK(G.GameRealRatio() == doctest::Approx(1.0).epsilon(0.0001));
		CHECK(G.StepCapFrames == 0);
		CHECK(G.LostSimMs == doctest::Approx(0.0));
		CHECK(G.TagFrames[7] == 2 + 1 + 30);   // the stepCap tag itself stays on the frames
		const FVerdict V = Judge(G, FThresholds{});
		CHECK(V.Grade == "smooth");
		CHECK(GateFailure(V, true).empty());
		// every frame: the swallowed stalls show, for information
		CHECK(G.AllStepCapFrames == 2 + 1 + 30);
		CHECK(G.AllGameRealRatio() < 0.95);
		CHECK(G.AllLostSimMs == doctest::Approx(2 * 1500.0 + 3000.0 + 30 * 120.0 - 33 * 4 * abyss::kSimStepMs));
		CHECK(Sum.Scenes[0].GameRealRatio() == doctest::Approx(1.0).epsilon(0.0001));
		CHECK(Sum.Run.StepCapFrames == 0);

		// a long UNTAGGED play frame is slow motion: the frame after it is a ratio frame
		std::vector<FFrameSample> Hitchy;
		Play(Hitchy, 600);
		for (int Hitch = 0; Hitch < 20; ++Hitch)
		{
			FFrameSample Long = SimFrame(0.0, Dt, 1);
			Long.FrameMs = 400.f;
			Hitchy.push_back(Long);
			FFrameSample After = SimFrame(0.0, 400.f, 4);
			After.FrameMs = Dt;
			Hitchy.push_back(After);
			Play(Hitchy, 20);
		}
		Stamp(Hitchy);
		const FGroup HitchyGame = Summarize(Hitchy, Scenes, FSimLimits{}).InGame;
		CHECK(HitchyGame.StepCapFrames == 20);
		CHECK(HitchyGame.GameRealRatio() < 0.97);
		// (the run's very first frame has no frame before it: it is in the all-frames ratio only)
		CHECK(HitchyGame.GameRealRatio() == doctest::Approx(HitchyGame.AllGameRealRatio()).epsilon(0.001));
		// lost against the wall clock, and the part of it the 250 ms clamp dropped; ratio and lostSimSec reconcile
		CHECK(HitchyGame.LostSimMs == doctest::Approx(20 * (400.0 - 4 * abyss::kSimStepMs)));
		CHECK(HitchyGame.ClampLostMs == doctest::Approx(20 * 150.0));
		CHECK(HitchyGame.RatioRealMs - HitchyGame.RatioSimMs == doctest::Approx(HitchyGame.LostSimMs).epsilon(0.001));
		CHECK(Judge(HitchyGame, FThresholds{}).Grade == "rough");

		// the JSON keeps the play keys and adds the all-frames block
		abyss::JsonWriter W;
		WriteGroup(W, G);
		const abyss::JsonValue Root = Parse(W.Take());
		CHECK(Root.Get("gameRealRatio").AsDouble() == doctest::Approx(1.0));
		CHECK(Root.Get("stepCapFrames").AsInt() == 0);
		CHECK(Root.Get("lostToClampSec").AsDouble() == doctest::Approx(0.0));
		CHECK(Root.Get("simAllFrames").Get("stepCapFrames").AsInt() == 33);
		CHECK(Root.Get("simAllFrames").Get("gameRealRatio").AsDouble() < 0.95);
		CHECK(Root.Get("simAllFrames").Get("lostSimSec").AsDouble() == doctest::Approx(G.AllLostSimMs / 1000.0).epsilon(0.001));
	}

	TEST_CASE("the game / real ratio leaves out S6 slow motion and frozen frames")
	{
		const std::vector<FScene> Scenes = { { "fight_001", ESceneKind::Fight } };
		const float Dt = static_cast<float>(abyss::kSimStepMs);
		std::vector<FFrameSample> Samples;
		for (int Index = 0; Index < 300; ++Index)
		{
			Samples.push_back(SimFrame((Index + 1) * 0.0167, Dt, 1));
		}
		// an elite kill: 60 frames at a third of the speed (a step every third frame), on purpose
		for (int Index = 0; Index < 60; ++Index)
		{
			Samples.push_back(SimFrame((301 + Index) * 0.0167, Dt, Index % 3 == 0 ? 1 : 0, TagDilated));
		}
		// a pause: no steps at all
		for (int Index = 0; Index < 60; ++Index)
		{
			Samples.push_back(SimFrame((361 + Index) * 0.0167, Dt, 0, TagFrozen));
		}
		const FGroup Game = Summarize(Samples, Scenes, FSimLimits{}).InGame;
		CHECK(Game.TagFrames[8] == 60);
		CHECK(Game.TagFrames[9] == 60);
		CHECK(Game.GameRealRatio() == doctest::Approx(1.0).epsilon(0.0001));
		CHECK(Game.AllGameRealRatio() == doctest::Approx(1.0).epsilon(0.0001));
		CHECK(Game.RatioRealMs == doctest::Approx(299 * abyss::kSimStepMs).epsilon(0.0001));   // the run's first frame has no frame before it
	}

	TEST_CASE("stat unit values move back one sample: they time the frame before the one they were read in")
	{
		std::vector<FFrameSample> Samples;
		for (int Index = 0; Index < 4; ++Index)
		{
			Samples.push_back(Frame(Index + 1.0, 16.f));
		}
		// frame 1 is a 180 ms hitch; read at each frame's end, its split arrives with frame 2
		Samples[1].FrameMs = 180.f;
		Samples[0].GameMs = Unavailable;   // the stat was not drawn yet
		Samples[1].GameMs = 8.f;   Samples[1].RenderMs = 6.f;   Samples[1].RhiMs = 1.f;  Samples[1].GpuMs = 5.f;
		Samples[2].GameMs = 170.f; Samples[2].RenderMs = 12.f;  Samples[2].RhiMs = 2.f;  Samples[2].GpuMs = 7.f;
		Samples[3].GameMs = 9.f;   Samples[3].RenderMs = 6.5f;  Samples[3].RhiMs = 1.5f; Samples[3].GpuMs = Unavailable;
		Samples[1].SimMs = 3.f;    // our own scopes are measured in their frame: they stay
		ShiftThreadTimesBack(Samples);
		CHECK(Samples[0].GameMs == doctest::Approx(8.f));
		CHECK(Samples[1].FrameMs == doctest::Approx(180.f));
		CHECK(Samples[1].GameMs == doctest::Approx(170.f));
		CHECK(Samples[1].RenderMs == doctest::Approx(12.f));
		CHECK(Samples[1].RhiMs == doctest::Approx(2.f));
		CHECK(Samples[1].GpuMs == doctest::Approx(7.f));
		CHECK(Samples[1].SimMs == doctest::Approx(3.f));
		CHECK(Samples[2].GameMs == doctest::Approx(9.f));
		CHECK(Samples[2].GpuMs < 0.f);
		CHECK(Samples[3].GameMs < 0.f);    // the last frame's values were never read
		CHECK(Samples[3].RenderMs < 0.f);
		CHECK(Samples[3].RhiMs < 0.f);
		CHECK(Samples[3].GpuMs < 0.f);
		// the hitch's longest-frames entry now carries its own split
		const std::vector<FScene> Scenes = { { "camp", ESceneKind::Camp } };
		const FSummary Sum = Summarize(Samples, Scenes, FSimLimits{});
		abyss::JsonWriter W;
		WriteLongestFrame(W, Sum.Longest[0], Samples[Sum.Longest[0]], Scenes, {});
		const abyss::JsonValue Root = Parse(W.Take());
		CHECK(Root.Get("frameMs").AsDouble() == doctest::Approx(180.0));
		CHECK(Root.Get("gameMs").AsDouble() == doctest::Approx(170.0));
		std::vector<FFrameSample> None;
		ShiftThreadTimesBack(None);
		CHECK(None.empty());
		std::vector<FFrameSample> One = { Frame(1.0, 16.f) };
		One[0].GameMs = 5.f;
		ShiftThreadTimesBack(One);
		CHECK(One[0].GameMs < 0.f);
	}

	TEST_CASE("the longest play frames: boot, loads, shots and story do not crowd out the hitches the verdict judges")
	{
		const std::vector<FScene> Scenes = { { "menu", ESceneKind::Menu }, { "camp", ESceneKind::Camp }, { "story", ESceneKind::Story } };
		std::vector<FFrameSample> Samples;
		double Now = 0.0;
		const auto Add = [&Samples, &Now](float FrameMs, std::uint32_t Tags, std::uint16_t Scene)
		{
			Now += static_cast<double>(FrameMs) / 1000.0;
			Samples.push_back(Frame(Now, FrameMs, Tags, Scene));
		};
		for (int Index = 0; Index < 80; ++Index)
		{
			Add(500.f + Index, 0u, 0);              // menu: untagged but not in game
		}
		for (int Index = 0; Index < 80; ++Index)
		{
			Add(300.f + Index, TagShot, 1);         // camp, screenshots
			Add(250.f, TagStory | TagFrozen, 2);    // story
		}
		for (int Index = 0; Index < 400; ++Index)
		{
			Add(Index % 100 == 7 ? 140.f + Index / 100 : 16.f, Index % 2 == 0 ? TagBacklog : 0u, 1);
		}
		const FSummary Sum = Summarize(Samples, Scenes, FSimLimits{}, 5);
		REQUIRE(Sum.Longest.size() == 5);
		for (const std::size_t Index : Sum.Longest)
		{
			CHECK(Samples[Index].Scene == 0);       // the run's list: all menu frames
		}
		REQUIRE(Sum.LongestPlay.size() == 5);
		CHECK(Samples[Sum.LongestPlay[0]].FrameMs == doctest::Approx(143.f));
		CHECK(Samples[Sum.LongestPlay[1]].FrameMs == doctest::Approx(142.f));
		CHECK(Samples[Sum.LongestPlay[2]].FrameMs == doctest::Approx(141.f));
		CHECK(Samples[Sum.LongestPlay[3]].FrameMs == doctest::Approx(140.f));
		CHECK(Samples[Sum.LongestPlay[4]].FrameMs == doctest::Approx(16.f));
		for (const std::size_t Index : Sum.LongestPlay)
		{
			CHECK(IsPlay(Samples[Index].Tags));
			CHECK(Samples[Index].Scene == 1);
		}
		// fewer play frames than the list length; none at all
		CHECK(Summarize(Samples, Scenes, FSimLimits{}, 1000).LongestPlay.size() == 400);
		const std::vector<FFrameSample> MenuOnly(Samples.begin(), Samples.begin() + 80);
		CHECK(Summarize(MenuOnly, Scenes, FSimLimits{}).LongestPlay.empty());
		CHECK(Summarize(MenuOnly, Scenes, FSimLimits{}).Longest.size() == 50);
	}

	TEST_CASE("the attack contact: a hit only takes a contact of its own kind, due, and within the step budget")
	{
		const double Step = abyss::kSimStepMs;
		double Error = 0.0;
		CHECK(IsBasicSwingClip(""));
		CHECK(IsBasicSwingClip("Attack01"));
		CHECK(IsBasicSwingClip("Attack02"));
		CHECK_FALSE(IsBasicSwingClip("whirlwind"));
		CHECK_FALSE(IsBasicSwingClip("death_mark"));
		CHECK(FContactTracker().MatchWindowMs() == doctest::Approx(5 * Step));
		CHECK(FContactTracker(8).MatchWindowMs() == doctest::Approx(9 * Step));
		CHECK(FContactTracker(0).MatchWindowMs() == doctest::Approx(2 * Step));
		{
			// a basic swing whose own hit never came (the target died), then a skill's hit 130 ms after that contact
			FContactTracker Tracker;
			Tracker.OnHeroAttackAnim(1000.0, 300.0, "Attack01");
			CHECK_FALSE(Tracker.OnHeroHit(1320.0, 1310.0, Error, "whirlwind"));   // another kind, even in the window
			CHECK_FALSE(Tracker.OnHeroHit(1430.0, 1420.0, Error));                // a basic hit, but 130 ms late: not this swing's
			CHECK(Tracker.Result().Count == 0);
			CHECK(Tracker.Result().Unmatched == 2);
			CHECK(Tracker.Result().LateOver33 == 0);
		}
		{
			// a skill's swing and its own hit; a basic hit in between does not take the skill's contact
			FContactTracker Tracker;
			Tracker.OnHeroAttackAnim(2000.0, 200.0, "whirlwind");
			CHECK_FALSE(Tracker.OnHeroHit(2200.0, 2195.0, Error));
			CHECK_FALSE(Tracker.OnHeroHit(2200.0, 2195.0, Error, "cleave"));
			CHECK(Tracker.OnHeroHit(2200.0, 2195.0, Error, "whirlwind"));
			CHECK(Error == doctest::Approx(-5.0));
			CHECK_FALSE(Tracker.OnHeroHit(2200.0, 2195.0, Error, "whirlwind"));   // its second target
		}
		{
			// both kinds pending at once: each hit finds its own
			FContactTracker Tracker;
			Tracker.OnHeroAttackAnim(3000.0, 100.0, "Attack01");
			Tracker.OnHeroAttackAnim(3000.0, 120.0, "cleave");
			CHECK(Tracker.OnHeroHit(3125.0, 3120.0, Error, "cleave"));
			CHECK(Error == doctest::Approx(0.0));
			CHECK(Tracker.OnHeroHit(3125.0, 3120.0, Error));
			CHECK(Error == doctest::Approx(20.0));
		}
		{
			// a hit cannot precede its contact: one step of slack, never two or more
			for (int StepsEarly = 2; StepsEarly <= 8; ++StepsEarly)
			{
				CAPTURE(StepsEarly);
				FContactTracker Tracker;
				Tracker.OnHeroAttackAnim(4000.0, 400.0);
				CHECK_FALSE(Tracker.OnHeroHit(4400.0 - StepsEarly * Step, 4400.0 - (StepsEarly + 1) * Step, Error));
				CHECK(Tracker.Result().Unmatched == 1);
				CHECK(Tracker.OnHeroHit(4400.0, 4400.0 - Step, Error));               // the contact is still pending
			}
			FContactTracker Tracker;
			Tracker.OnHeroAttackAnim(4000.0, 400.0);
			CHECK(Tracker.OnHeroHit(4400.0 - Step, 4400.0 - 2 * Step, Error));       // within the slack
		}
		{
			// the window is the frame's step budget + 1 steps: a frame that ran 4 steps presents a hit at most 4 steps late
			FContactTracker Tracker;
			Tracker.OnHeroAttackAnim(5000.0, 200.0);
			CHECK(Tracker.OnHeroHit(5200.0 + 4 * Step, 5200.0 + 3.5 * Step, Error));
			Tracker.OnHeroAttackAnim(6000.0, 200.0);
			CHECK_FALSE(Tracker.OnHeroHit(6200.0 + 5 * Step + 1.0, 6200.0 + 5 * Step, Error));
			CHECK_FALSE(Tracker.OnHeroHit(6200.0 + 5 * Step + 1.0, 6200.0 + 5 * Step, Error));   // and the stale contact is gone
		}
	}

	TEST_CASE("the verdict needs play frames, whatever the minimum is")
	{
		FThresholds T;
		T.MinFrames = 0;   // not reachable from the switch (frames:0 is refused), still not a verdict
		const FGroup Empty;
		const FVerdict V = Judge(Empty, T);
		CHECK(V.Grade == "notMeasured");
		CHECK_FALSE(GateFailure(V, true).empty());
		T.MinFrames = 1;
		std::vector<FFrameSample> Samples = { SimFrame(0.016, 16.f, 1), SimFrame(0.032, 16.f, 1) };
		const std::vector<FScene> Scenes = { { "camp", ESceneKind::Camp } };
		CHECK(Judge(Summarize(Samples, Scenes, FSimLimits{}).InGame, T).Grade == "smooth");
	}
}
