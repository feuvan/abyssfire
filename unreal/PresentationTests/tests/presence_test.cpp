// The in-game autotest's presentation probe (2G cross-stage review X6; Private/Framework/AbyssPresenceRules.h, used by
// UAbyssAutoTestSubsystem): the verdict on synthetic tallies, which telegraphs it expects a decal for, and a real
// Chapter 1 playthrough (the AutoPilot the `chapter` scenario plays) tallied the way the autotest tallies the router's
// events: by the named bosses' kills the core has announced strikes, hazards, Valdras's arena and corpses, so a
// presentation that draws none of them fails the run there, while one that draws them passes.
#include <memory>
#include <string>
#include <vector>

#include "doctest/doctest.h"

#include "abyss/base/Enums.h"
#include "abyss/base/SimClock.h"
#include "abyss/data/DataStore.h"
#include "abyss/sim/GameSim.h"
#include "abyss/sim/Snapshot.h"
#include "abyss/tools/AutoPilot.h"

#include "Framework/AbyssPresenceRules.h"
#include "World/AbyssThemeRules.h"
#include "audio_test_data.h"

namespace
{
	using namespace AbyssPresenceRules;

	FTally Announced(int Strikes, int Hazards, int Arenas, int Corpses, int AshnumbFrames)
	{
		FTally T;
		T.StrikeTelegraphs = Strikes;
		T.Hazards = Hazards;
		T.Arenas = Arenas;
		T.Corpses = Corpses;
		T.AshnumbFrames = AshnumbFrames;
		return T;
	}
}

TEST_SUITE("presence")
{
	TEST_CASE("the verdict names what the core announced and nothing drew")
	{
		CHECK(Verdict(FTally{}).empty());   // nothing announced: nothing to draw (chapters 2-5)
		FTally Drawn = Announced(12, 3, 1, 9, MinBandFrames);
		Sample(Drawn, 2, 1, 4, true, true);
		CHECK(Verdict(Drawn).empty());
		const FTally Nothing = Announced(12, 3, 1, 9, MinBandFrames);
		const std::string All = Verdict(Nothing);
		CHECK(All.find("no decal") != std::string::npos);
		CHECK(All.find("no arena ring") != std::string::npos);
		CHECK(All.find("no husk") != std::string::npos);
		CHECK(All.find("no band") != std::string::npos);
		// each check alone
		FTally NoDecals = Drawn;
		NoDecals.PeakDecals = 0;
		CHECK(Verdict(NoDecals).find("no decal") != std::string::npos);
		FTally HazardsOnly = Announced(0, 2, 0, 0, 0);
		CHECK_FALSE(Verdict(HazardsOnly).empty());
		FTally NoArena = Drawn;
		NoArena.PeakArenaDecals = 0;
		CHECK(Verdict(NoArena) == "1 arenas but no arena ring drawn");
		FTally NoHusks = Drawn;
		NoHusks.PeakHusks = 0;
		CHECK(Verdict(NoHusks).find("M_AF_Toon_Ash") != std::string::npos);
		FTally NoBand = Drawn;
		NoBand.BandFrames = 0;
		CHECK(Verdict(NoBand).find("M_AF_Toon_Ashnumb") != std::string::npos);
		// a few frames of Ashnumb before the band is up are not a failure
		FTally Brief = Drawn;
		Brief.BandFrames = 0;
		Brief.AshnumbFrames = MinBandFrames - 1;
		CHECK(Verdict(Brief).empty());
	}

	TEST_CASE("Sample keeps the peaks and counts the band only where it is expected")
	{
		FTally T;
		Sample(T, 3, 0, 5, false, true);
		Sample(T, 1, 1, 7, true, false);
		Sample(T, 2, 0, 2, true, true);
		CHECK(T.PeakDecals == 3);
		CHECK(T.PeakArenaDecals == 1);
		CHECK(T.PeakHusks == 7);
		CHECK(T.AshnumbFrames == 2);
		CHECK(T.BandFrames == 1);   // the frame without stacks does not count, even with the band up
	}

	TEST_CASE("which telegraphs expect a decal: ground strikes in a themed zone, never fuses or raise markers")
	{
		using abyss::AbilityKind;
		using abyss::TelegraphSource;
		for (const AbilityKind Kind : { AbilityKind::Circle, AbilityKind::Cone, AbilityKind::Line, AbilityKind::Ring, AbilityKind::Hazard })
		{
			CAPTURE(abyss::EnumName(Kind));
			CHECK(ExpectsDecal(Kind, TelegraphSource::Ability, true));
			CHECK_FALSE(ExpectsDecal(Kind, TelegraphSource::Ability, false));
			CHECK_FALSE(ExpectsDecal(Kind, TelegraphSource::OnDeath, true));   // fuses: some draw nothing (Gravecold)
		}
		for (const AbilityKind Kind : { AbilityKind::Raise, AbilityKind::Summon, AbilityKind::Bolt, AbilityKind::Buff, AbilityKind::Call })
		{
			CAPTURE(abyss::EnumName(Kind));
			CHECK_FALSE(ExpectsDecal(Kind, TelegraphSource::Ability, true));
		}
	}

	TEST_CASE("a Chapter 1 playthrough: by Valdras's kill the core announced everything the probe checks")
	{
		const abyss::DataStore& Data = audio_test::Data();
		const double FrameMs = abyss::kSimStepMs;   // 60 fps, as the autotest caps t.MaxFPS
		std::unique_ptr<abyss::GameSim> Sim = abyss::GameSim::Create(Data, abyss::SimConfig{});
		REQUIRE(Sim != nullptr);
		REQUIRE(Sim->NewGame(abyss::ClassId::Warrior, abyss::Difficulty::Normal, 404));
		FTally Tally;
		struct FCheck
		{
			std::string Boss;
			FTally At;
		};
		std::vector<FCheck> Checks;
		abyss::AutoPilotConfig Config;
		Config.cls = abyss::ClassId::Warrior;
		Config.scenario = abyss::AutoPilotScenario::Chapter1;
		Config.storyMode = abyss::AutoPilotStoryMode::SkipAll;   // as the autotest
		Config.onMilestone = [&Tally, &Checks](const abyss::AutoPilotMilestoneRecord& Record)
		{
			if (Record.kind == abyss::AutoPilotMilestone::BossKill)
			{
				Checks.push_back({ Record.detail, Tally });
			}
		};
		abyss::GameSim& SimRef = *Sim;
		abyss::AutoPilot Pilot(Data, Config, [&SimRef](const abyss::Command& Command) { SimRef.Submit(Command); });
		// UAbyssAutoTestSubsystem::TallyPresenceEvent, on the router's events
		auto Tally1 = [&Data, &Tally](const abyss::Event& Event, const abyss::Snapshot& View)
		{
			const bool bThemed = !AbyssThemeRules::ThemeOf(Data.FindMap(View.zone.mapId)).empty();
			if (const abyss::EvTelegraph* Telegraph = std::get_if<abyss::EvTelegraph>(&Event))
			{
				Tally.StrikeTelegraphs += ExpectsDecal(Telegraph->kind, Telegraph->source, bThemed) ? 1 : 0;
			}
			else if (std::holds_alternative<abyss::EvHazard>(Event))
			{
				Tally.Hazards += bThemed ? 1 : 0;
			}
			else if (const abyss::EvArena* Arena = std::get_if<abyss::EvArena>(&Event))
			{
				Tally.Arenas += Arena->cleared ? 0 : 1;
			}
			else if (std::holds_alternative<abyss::EvCorpseAdded>(Event))
			{
				++Tally.Corpses;
			}
		};
		for (const abyss::Event& E : Sim->Events())
		{
			Pilot.ObserveEvent(E);
			Tally1(E, Sim->View());
		}
		bool bFinished = false;
		for (int64_t Guard = 0; Guard < 3 * 3600 * 60 && !bFinished; ++Guard)
		{
			const abyss::AutoPilotStatus Status = Pilot.Advance(Sim->View());
			if (Status == abyss::AutoPilotStatus::Finished)
			{
				bFinished = true;
				break;
			}
			if (Status == abyss::AutoPilotStatus::Checkpoint)
			{
				continue;
			}
			Pilot.PrepareFrame(Sim->View());
			const int32_t Steps = Sim->Frame(FrameMs);
			const abyss::Snapshot& View = Sim->View();
			for (const abyss::Event& E : Sim->Events())
			{
				Pilot.ObserveEvent(E);
				Tally1(E, View);
			}
			// the band frames (desktop: BandAllowed), as SamplePresence counts them
			Tally.AshnumbFrames += View.hero.ashnumbStacks > 0 && View.hero.life == abyss::HeroLife::Alive ? 1 : 0;
			Pilot.EndFrame(View, Steps);
		}
		CHECK(bFinished);
		CHECK(Pilot.Ok());
		CHECK(Pilot.Reached(abyss::AutoPilotMilestone::ChapterDone));
		const FCheck* Valdras = nullptr;
		for (const FCheck& C : Checks)
		{
			MESSAGE("bossKill " << C.Boss << ": " << C.At.StrikeTelegraphs << " strikes, " << C.At.Hazards << " hazards, "
				<< C.At.Arenas << " arenas, " << C.At.Corpses << " corpses, " << C.At.AshnumbFrames << " Ashnumb frames");
			Valdras = C.Boss == "valdras" ? &C : Valdras;
		}
		REQUIRE(Valdras != nullptr);
		CHECK(Valdras->At.StrikeTelegraphs > 0);
		CHECK(Valdras->At.Hazards > 0);
		CHECK(Valdras->At.Arenas > 0);
		CHECK(Valdras->At.Corpses > 0);
		// a content build without the Wave 2 masters (nothing drawn) fails at that kill; a working one passes
		CHECK_FALSE(Verdict(Valdras->At).empty());
		FTally Working = Valdras->At;
		Sample(Working, 3, 1, 6, false, false);
		Working.BandFrames = Working.AshnumbFrames;
		CHECK(Verdict(Working).empty());
		// and already at the first named kill after the first telegraph (the probe does not wait for the finale)
		bool bEarly = false;
		for (const FCheck& C : Checks)
		{
			bEarly = bEarly || (&C != Valdras && C.At.StrikeTelegraphs > 0 && !Verdict(C.At).empty());
		}
		CHECK(bEarly);
		MESSAGE("whole run: " << Tally.StrikeTelegraphs << " strikes, " << Tally.Hazards << " hazards, " << Tally.Arenas
			<< " arenas, " << Tally.Corpses << " corpses, " << Tally.AshnumbFrames << " Ashnumb frames");
	}
}
