// Stage P review (adversarial): AbyssSubDungeonRules.h / AbyssThemeRules.h on inputs the shipped data does not have.
// Suite `subdungeon_adv`. The cases marked DEFECT failed on the reviewed tree and pass since the stage P repair (the
// rules were fixed; where a case was re-pinned instead, a comment in it says how); the others pin behaviour that held.
#include <cmath>
#include <string>
#include <vector>

#include "doctest/doctest.h"

#include "abyss/base/Json.h"

#include "Framework/AbyssSmoothnessRules.h"
#include "World/AbyssSubDungeonRules.h"
#include "World/AbyssThemeRules.h"

namespace
{
	using namespace AbyssSubDungeonRules;

	abyss::JsonValue Parse(const std::string& Text)
	{
		abyss::JsonValue Root;
		abyss::ParseJson(Text, Root);
		return Root;
	}

	std::vector<abyss::Vec2> LineOfLamps(int Count)
	{
		std::vector<abyss::Vec2> Out;
		for (int I = 0; I < Count; ++I)
		{
			Out.push_back(abyss::Vec2{ static_cast<double>(I), 0.0 });
		}
		return Out;
	}
}

TEST_SUITE("subdungeon_adv")
{
	// ---- the cleanse sequence ------------------------------------------------------------------------------------

	TEST_CASE("DEFECT cleanse: a step that is not a whole number of ms can end the sequence with a lamp unlit")
	{
		// lampStepMs 416.7, 14 lamps: SequenceEndMs = 600 + 13 x 416.7 = 6017.099999999999, but
		// floor((6017.099999999999 - 600) / 416.7) + 1 = 13. A frame that lands on the end moment turns the state Lit
		// (AdvanceCleanse does nothing in Lit) with the last lamp never swapped.
		FCleanseSpec Spec;
		Spec.bValid = Spec.bHasCleanse = true;
		Spec.LampStepMs = 416.7;
		const std::vector<abyss::Vec2> Lamps = LineOfLamps(14);
		FCleanseState State = EnterState(false, 14);
		StartCleanse(State, Lamps, abyss::Vec2{ 0.0, 0.0 }, 0.0);
		const double End = SequenceEndMs(Spec, 14, 0);
		int Lit = 0;
		Lit += static_cast<int>(AdvanceCleanse(Spec, State, 0, End).LightLamps.size());
		Lit += static_cast<int>(AdvanceCleanse(Spec, State, 0, End + 60000.0).LightLamps.size());
		CHECK(State.Phase == ECleansePhase::Lit);
		CHECK(Lit == 14);
		CHECK(State.LampsLit == 14);
		// the contract of LampsLitAt's comment: "rank k is lit from LampLitAtMs(k) on"
		for (int K = 0; K < 14; ++K)
		{
			CAPTURE(K);
			CHECK(LampsLitAt(Spec, 14, LampLitAtMs(Spec, K)) >= K + 1);
		}
	}

	TEST_CASE("DEFECT cleanse: the niche's glow is skipped when the sequence ends before NicheAtMs")
	{
		// SequenceEndMs counts the grade, the last lamp and the last mark, not the niche. A contract with a short grade
		// (overMs 300), no lamps and a niche without marks ends at 300 ms; whether the niche plays then depends on the
		// frame times ("whatever the frame times" is the shipped test's own claim).
		FCleanseSpec Spec;
		Spec.bValid = Spec.bHasCleanse = true;
		Spec.GradeMs = 300.0;
		for (const double FrameMs : { 16.0, 5000.0 })
		{
			CAPTURE(FrameMs);
			FCleanseState State = EnterState(false, 0);
			StartCleanse(State, {}, abyss::Vec2{ 0.0, 0.0 }, 0.0);
			int Niches = 0;
			for (double Now = 0.0; Now <= 10000.0; Now += FrameMs)
			{
				Niches += AdvanceCleanse(Spec, State, 0, Now).bPlayNiche ? 1 : 0;
			}
			CHECK(State.Phase == ECleansePhase::Lit);
			CHECK(Niches == 1);
		}
	}

	TEST_CASE("cleanse: order, restarts and a clock that runs backwards")
	{
		FCleanseSpec Spec;
		Spec.bValid = Spec.bHasCleanse = true;
		const std::vector<abyss::Vec2> Lamps = LineOfLamps(5);
		// lamps on one spot: placement order
		CHECK(LampOrder({ { 1, 1 }, { 1, 1 }, { 1, 1 } }, { 9, 9 }) == std::vector<int>{ 0, 1, 2 });
		FCleanseState State = EnterState(false, 5);
		StartCleanse(State, Lamps, abyss::Vec2{ 4.0, 0.0 }, 1000.0);
		// a second event while playing keeps the order and the start
		StartCleanse(State, Lamps, abyss::Vec2{ 0.0, 0.0 }, 5000.0);
		CHECK(State.StartMs == 1000.0);
		CHECK(State.Order == std::vector<int>{ 4, 3, 2, 1, 0 });
		// the clock before the start: nothing, no finish, alpha 0
		const FCleanseStep Early = AdvanceCleanse(Spec, State, 5, 0.0);
		CHECK(Early.LightLamps.empty());
		CHECK(!Early.bPlayNiche);
		CHECK(!Early.bJustFinished);
		CHECK(StateAlpha(Spec, State, 0.0) == 0.0);
		// one long frame: all five, in order, every mark, one niche, one finish
		const FCleanseStep All = AdvanceCleanse(Spec, State, 5, 1000.0 + 1.0e7);
		CHECK(All.LightLamps == std::vector<int>{ 4, 3, 2, 1, 0 });
		CHECK(All.PlayMarks == std::vector<int>{ 1, 2, 3, 4, 5 });
		CHECK(All.bPlayNiche);
		CHECK(All.bJustFinished);
		CHECK(AdvanceCleanse(Spec, State, 5, 1000.0 + 2.0e7).LightLamps.empty());
		// a step of 0 or below: every lamp at LampFirstMs
		Spec.LampStepMs = -50.0;
		CHECK(LampsLitAt(Spec, 5, Spec.LampFirstMs - 1.0) == 0);
		CHECK(LampsLitAt(Spec, 5, Spec.LampFirstMs) == 5);
	}

	TEST_CASE("DEFECT marks: a socket number beyond two digits stretches the sequence")
	{
		// MarkNumber's guard (`Number > 99` before the digit is added) lets three digits through: mark_999 is 999, the
		// glue sizes Marks to 999 and SequenceEndMs becomes 600 + 999 x 180 ms = 180 s of "Playing".
		CHECK(MarkNumber("mark_99") == 99);
		CHECK(MarkNumber("mark_999") == 0);
		CHECK(MarkNumber("mark_1000") == 0);
	}

	TEST_CASE("dark.<theme>: malformed contracts parse without a grade or with the defaults")
	{
		CHECK(!ParseCleanseSpec(Parse("{}")).bValid);
		CHECK(!ParseCleanseSpec(Parse("[]")).bValid);
		CHECK(!ParseCleanseSpec(Parse("{\"lightRadius\": 3}")).bValid);
		const FCleanseSpec Bare = ParseCleanseSpec(Parse("{\"lightRadius\": {}}"));
		CHECK(Bare.bValid);
		CHECK(!Bare.bHasCleanse);
		CHECK(!Bare.Hero.IsOn());
		CHECK(Bare.LampUnlit.empty());
		double Out[3];
		GradeMultiplier(Bare, 0.5, 0.0, Out);
		CHECK(Out[0] == 1.0);
		const FCleanseSpec Odd = ParseCleanseSpec(Parse(
			"{\"lightRadius\": {\"ambient\": [0.1, 0.2], \"hero\": 4, \"ceil\": \"x\"},"
			" \"cleanse\": {\"sequence\": {\"atMs\": 5}, \"grade\": [], \"lampStepMs\": null},"
			" \"lampSwap\": {\"unlit\": 7, \"lit\": \"\"}}"));
		CHECK(Odd.bValid);
		CHECK(Odd.bHasCleanse);
		CHECK(Odd.Ambient[0] == 1.0);            // a pair is not a colour
		CHECK(Odd.LampFirstMs == 600.0);
		CHECK(Odd.LampStepMs == 420.0);
		CHECK(Odd.GradeMs == 6000.0);
		CHECK(Odd.LampUnlit.empty());
		CHECK(Odd.LampLit.empty());
	}

	TEST_CASE("DEFECT dark.<theme>: the lamps' first moment is read from the prose of another step")
	{
		// LampFirstMs / NicheAtMs are found by substring ("lamps light", "Flame Niche") in the steps' free text, first
		// match wins. A step at 0 ms whose sentence mentions the lamps moves every lamp to 0 ms.
		const FCleanseSpec Spec = ParseCleanseSpec(Parse(
			"{\"lightRadius\": {}, \"cleanse\": {\"lampStepMs\": 420, \"sequence\": ["
			"{\"atMs\": 0, \"do\": \"play the embers; they are gone before the lamps light\"},"
			"{\"atMs\": 600, \"do\": \"the lamps light one by one\"},"
			"{\"atMs\": 900, \"do\": \"if the Flame Niche is on screen: the glow\"}]}}"));
		CHECK(Spec.LampFirstMs == 600.0);
		CHECK(Spec.NicheAtMs == 900.0);
	}

	// ---- walls, the stair, overlays --------------------------------------------------------------------------------

	TEST_CASE("wall yaw: always in [0, 360), a multiple of the step")
	{
		for (const double Step : { 90.0, 45.0, 120.0, 60.0, 30.0, 180.0, 22.5, 100.0, 359.0 })
		{
			CAPTURE(Step);
			for (uint32_t Hash = 0; Hash < 200000u; Hash += 257u)
			{
				const double Yaw = WallYawDeg(Hash, Step);
				CHECK(Yaw >= 0.0);
				CHECK(Yaw < 360.0);
				CHECK(std::fabs(std::fmod(Yaw, Step)) < 1.0e-3);
			}
		}
		// no step, a negative step, NaN: the hashed yaw as before
		CHECK(WallYawDeg(0x00012C00u, 0.0) == static_cast<float>((0x00012C00u >> 8) % 360u));
		CHECK(WallYawDeg(0x00012C00u, -90.0) == static_cast<float>((0x00012C00u >> 8) % 360u));
		CHECK(WallYawDeg(0x00012C00u, std::nan("")) == static_cast<float>((0x00012C00u >> 8) % 360u));
		// a step too small to count in a turn (the cast would overflow): no step
		CHECK(WallYawDeg(0x00012C00u, 1.0e-9) == static_cast<float>((0x00012C00u >> 8) % 360u));
		CHECK(WallYawDeg(0x00012C00u, 0.5) == static_cast<float>((0x00012C00u >> 8) % 360u));
	}

	TEST_CASE("DEFECT wall yaw: a step of a full turn (never turn the block) gets the free hashed yaw")
	{
		// 180 < step < 360 gives one step (yaw 0); step == 360 falls into the "no step" branch and turns the block
		// by any of 360 degrees - the opposite of what the art asked for.
		CHECK(WallYawDeg(0x00002D00u, 359.0) == 0.f);
		CHECK(WallYawDeg(0x00002D00u, 360.0) == 0.f);
		CHECK(WallYawDeg(0x00007B00u, 720.0) == 0.f);
	}

	TEST_CASE("wall blocks and the stair on synthetic grids")
	{
		// a 3 x 3 grid of walls with one open centre: all eight neighbours get a block, a 5 x 5 ring's outer wall none
		auto IsWall3 = [](int C, int R) { return !(C == 1 && R == 1); };
		for (int R = 0; R < 3; ++R)
		{
			for (int C = 0; C < 3; ++C)
			{
				if (IsWall3(C, R))
				{
					CHECK(WallTouchesOpen(3, 3, C, R, IsWall3));
				}
			}
		}
		// solid rock: nothing touches open, and the grid's edge is not "open"
		auto Solid = [](int, int) { return true; };
		CHECK(!WallTouchesOpen(4, 4, 0, 0, Solid));
		CHECK(!WallTouchesOpen(1, 1, 0, 0, Solid));
		CHECK(!WallTouchesOpen(0, 0, 0, 0, Solid));
		CHECK(BuildsWallBlock(false, false));
		CHECK(!BuildsWallBlock(true, false));

		// the stair: dead end open to +col only
		auto East = [](int C, int R) { return C == 6 && R == 5; };
		FFloorSide Side = ExitFloorSide(5, 5, East);
		CHECK(Side.bFound);
		CHECK(Side.Dc == 1);
		CHECK(Side.Dr == 0);
		// a corridor (open on both sides of each axis): the first walkable side, the same on every build
		auto Open = [](int, int) { return true; };
		Side = ExitFloorSide(5, 5, Open);
		CHECK(Side.bFound);
		CHECK(Side.Dc == 0);
		CHECK(Side.Dr == -1);
		// walled in (reachable only on a diagonal): not found
		auto None = [](int, int) { return false; };
		CHECK(!ExitFloorSide(5, 5, None).bFound);
	}

	TEST_CASE("overlay yaw: every neighbour in [0, 360), the facing minus the mesh offset")
	{
		for (int Dr = -1; Dr <= 1; ++Dr)
		{
			for (int Dc = -1; Dc <= 1; ++Dc)
			{
				CAPTURE(Dc);
				CAPTURE(Dr);
				const double Yaw = OverlayYawDeg(Dc, Dr);
				CHECK(Yaw >= 0.0);
				CHECK(Yaw < 360.0);
				if (Dc != 0 || Dr != 0)
				{
					double Facing = FacingDeg(Dc, Dr) - 90.0;
					Facing = Facing < 0.0 ? Facing + 360.0 : Facing;
					CHECK(std::fabs(Yaw - Facing) < 1.0e-9);
				}
			}
		}
		CHECK(OverlayYawDeg(0, 0) == 0.0);
	}

	TEST_CASE("the entrance chain on odd ids")
	{
		using Ids = std::vector<std::string>;
		CHECK(EntranceIdChain({}, true).empty());
		CHECK(EntranceIdChain({ "_open" }, true) == Ids{ "_sealed" });
		CHECK(EntranceIdChain({ "a_open", "a_sealed", "a_open" }, true) == Ids{ "a_sealed" });
		CHECK(EntranceIdChain({ "a_open", "a_sealed", "a_open" }, false) == Ids{ "a_open", "a_sealed" });
		CHECK(EntranceIdChain({ "a_opened", "open" }, true) == Ids{ "a_opened", "open" });
	}

	// ---- the theme chain -------------------------------------------------------------------------------------------

	TEST_CASE("theme chain: cycles, self parents, missing and malformed parents")
	{
		using namespace AbyssThemeRules;
		using Names = std::vector<std::string>;
		const abyss::JsonValue Loop = Parse("{\"themeParents\": {\"pit\": \"march\", \"march\": \"pit\"}}");
		CHECK(ThemeChain(&Loop, "pit") == Names{ "pit", "march" });
		CHECK(ThemeChain(&Loop, "march") == Names{ "march", "pit" });
		const abyss::JsonValue Self = Parse("{\"themeParents\": {\"pit\": \"pit\"}}");
		CHECK(ThemeChain(&Self, "pit") == Names{ "pit" });
		const abyss::JsonValue Three = Parse("{\"themeParents\": {\"a\": \"b\", \"b\": \"c\", \"c\": \"a\"}}");
		CHECK(ThemeChain(&Three, "b") == Names{ "b", "c", "a" });
		// a parent that is no theme: kept in the chain (its lookups miss), the style falls back
		const abyss::JsonValue Missing = Parse("{\"themeParents\": {\"pit\": \"nosuch\"}, \"themes\": {\"march\": {\"ground\": {}}}}");
		CHECK(ThemeChain(&Missing, "pit") == Names{ "pit", "nosuch" });
		CHECK(StyleThemeName(&Missing, ThemeChain(&Missing, "pit"), "plains") == "plains");
		// malformed tables
		for (const char* Text : { "{}", "[]", "{\"themeParents\": []}", "{\"themeParents\": {\"pit\": 3}}",
				 "{\"themeParents\": {\"pit\": \"\"}}", "{\"themeParents\": {\"pit\": {\"march\": 1}}}" })
		{
			CAPTURE(Text);
			const abyss::JsonValue Bad = Parse(Text);
			CHECK(ThemeChain(&Bad, "pit") == Names{ "pit" });
		}
		CHECK(ThemeChain(nullptr, "pit") == Names{ "pit" });
		CHECK(ThemeChain(nullptr, "").empty());
		CHECK(ThemeChain(&Loop, "unknown") == Names{ "unknown" });
		// the expansion under odd chains
		CHECK(ExpandThemed({ "wall" }, Names{}) == Names{ "wall" });
		CHECK(ExpandThemed({ "wall" }, Names{ "", "pit" }) == Names{ "wall@pit", "wall" });
		CHECK(ExpandThemed({}, Names{ "pit", "march" }).empty());
		CHECK(RecipeChain("exit.sealed", Names{ "pit", "march" }) == Names{ "exit.sealed@pit", "exit.sealed@march" });
		CHECK(RecipeChain("exit.sealed", Names{}) == Names{ "exit.sealed" });
		CHECK(RecipeChain("camp.torch@march", Names{ "pit", "march" }) == Names{ "camp.torch@march" });
	}

	TEST_CASE("DEFECT theme chain: a chain longer than MaxThemeChain loses its root without a word")
	{
		// e -> d -> c -> b -> march: the chain stopped at four names, so the theme never reached the march's art and fell
		// to the plain (web) ids, and nothing reported the cut. Repair: the cap is 8 and ThemeChainCut reports a chain the
		// cap cuts (the zone actor warns; subdungeon_test pins it on a 12-deep table).
		using namespace AbyssThemeRules;
		const abyss::JsonValue Deep = Parse(
			"{\"themeParents\": {\"e\": \"d\", \"d\": \"c\", \"c\": \"b\", \"b\": \"march\"},"
			" \"themes\": {\"march\": {\"ground\": {}}}}");
		const std::vector<std::string> Chain = ThemeChain(&Deep, "e");
		CHECK(Chain.back() == "march");
		CHECK(StyleThemeName(&Deep, Chain, "plains") == "march");
		CHECK(!ThemeChainCut(&Deep, "e"));
	}

	// ---- the autotest's probe --------------------------------------------------------------------------------------

	TEST_CASE("DEFECT probe: a cleanse with no lamp twin placed passes the verdict")
	{
		// The lit twin's art missing (AbyssSetPieces.cpp adds a lamp only when the lit mesh placed) leaves Lamps == 0:
		// the sequence still "finishes" (the grade), and the verdict says nothing although no lamp was ever lit.
		FProbe Probe;
		Probe.Visits = 1;
		Probe.bThemeResolved = true;
		Probe.WallBlocks = 240;
		Probe.Cleanses = 1;
		Probe.Lamps = 0;
		Probe.PeakLampsLit = 0;
		// Repair: the probe now carries what the map and the contract promise (LampsExpected: the unlit lamps placed) and
		// counts sequences instead of one flag, so the case states them; the verdict names the missing twins.
		Probe.SequencesStarted = Probe.SequencesFinished = 1;
		Probe.LampsExpected = 14;
		CHECK(!ProbeVerdict(Probe).empty());
		CHECK(ProbeVerdict(Probe).find("0 of 14 lamps") != std::string::npos);
	}

	TEST_CASE("DEFECT probe: a sub-dungeon built without one wall block, or never graded, passes the verdict")
	{
		FProbe Probe;
		Probe.Visits = 1;
		Probe.bThemeResolved = true;
		Probe.WallBlocks = 0;
		Probe.bGraded = true;
		CHECK(!ProbeVerdict(Probe).empty());
		Probe.WallBlocks = 240;
		// Repair: the grade is owed only where the theme chain has a dark contract (the two generated caves have none and
		// must not fail the run), so the case states the expectation.
		Probe.bGradeExpected = true;
		Probe.bGraded = false;   // vfx_recipes.json's acceptance: "HARD acceptance item ... the whole look of the Pit is this grade"
		CHECK(!ProbeVerdict(Probe).empty());
	}

	TEST_CASE("probe and scene labels: the cases that held")
	{
		FProbe Probe;
		Probe.Cleanses = 3;   // cleansed, never built inside: no visit, nothing to say
		CHECK(ProbeVerdict(Probe).empty());
		using namespace AbyssSmoothnessRules;
		FSceneTracker Tracker;
		FSceneInput In;
		In.Phase = EPhase::Playing;
		In.bCleansing = true;   // without a sub-dungeon the flag names no scene
		CHECK(Tracker.Scenes()[Tracker.Update(In)].Label == "travel");
		In.SubDungeon = "x";
		In.bInSafeZone = true;
		CHECK(Tracker.Scenes()[Tracker.Update(In)].Label == "dungeon_x_cleanse");
	}

	// ---- staggers --------------------------------------------------------------------------------------------------

	TEST_CASE("staggers: never later than the core's end, never negative")
	{
		for (uint32_t Id = 0; Id < 64; ++Id)
		{
			for (const double Duration : { 0.0, 1.0, 100.0, 620.0, 5000.0 })
			{
				// Repair: the rank is the place along the line (LineRanks), no longer the entity id modulo six; any rank.
				const FStaggeredStart Late = StaggerStart(1000.0, Duration, RiseDelayMs(static_cast<int>(Id)));
				CHECK(Late.StartMs >= 1000.0);
				CHECK(Late.DurationMs >= Duration * (1.0 - StaggerMaxShare) - 1.0e-9);
				CHECK(std::fabs(Late.StartMs + Late.DurationMs - (1000.0 + Duration)) < 1.0e-9);
			}
		}
		CHECK(RiseDelayMs(-5) == 0.0);
		CHECK(RiseDelayMs(1000) == RiseStaggerMaxMs);
		CHECK(StaggerStart(0.0, 500.0, std::nan("")).DurationMs == 500.0);
	}
}
