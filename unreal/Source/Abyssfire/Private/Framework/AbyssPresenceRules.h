// The in-game autotest's presentation probe (dark remake 2G cross-stage review X6; UAbyssAutoTestSubsystem).
//
// Every 2G presentation path degrades with a Warning when its content is missing (no M_AF_Telegraph / SM_FX_Quad, no
// materials.M_AF_Telegraph contract, no M_AF_Toon_Ash / M_AF_Outline_Ash, no M_AF_Toon_Ashnumb), and the autotest fails
// only on Error / Ensure lines, so a content build without the Wave 2 masters played the whole chapter green with no
// decals, husks or band. The probe tallies what the core announced (ground-strike telegraphs and hazards in a zone
// with a presentation theme, arenas, corpse entries, frames the hero carried Ashnumb where the tier draws the band)
// against what the presentation drew (the peak decals / arena rings drawn in one frame, the peak husks, frames the band
// showed). At every named boss's kill (AutoPilot milestone bossKill: the Bonegorger, the Corpseherd, Valdras, ...) and
// at the scenario's end, Verdict names what the core announced but nothing drew; the run then fails ("presentation:
// ..."), and report.json carries the tally (`presentation`).
//
// UE-free and header-only (std + AbyssCore public headers + the telegraph rules), tested in
// unreal/PresentationTests/tests/presence_test.cpp.
#pragma once

#include <string>
#include <string_view>

#include "abyss/base/Enums.h"
#include "abyss/sim/Events.h"

#include "Vfx/AbyssTelegraphRules.h"

namespace AbyssPresenceRules
{
	/** Frames of Ashnumb (where the tier draws the band) after which the band must have shown at least once. */
	inline constexpr int MinBandFrames = 30;

	struct FTally
	{
		// announced by the core
		int StrikeTelegraphs = 0;   // ground strikes in a themed zone (TelegraphLook: Decal; on-death fuses not counted)
		int Hazards = 0;            // EvHazard in a themed zone
		int Arenas = 0;             // EvArena (not `cleared`)
		int Corpses = 0;            // EvCorpseAdded (kills and seeds: only themed zones have corpse-leaving monsters)
		int AshnumbFrames = 0;      // frames with Ashnumb stacks on the hero and the band allowed (BandAllowed)
		// drawn by the presentation
		int PeakDecals = 0;         // UAbyssTelegraphSystem::GetDrawnDecalCount, the most in one frame
		int PeakArenaDecals = 0;    // GetDrawnArenaCount
		int PeakHusks = 0;          // UAbyssCorpseLayer::GetHuskCount
		int BandFrames = 0;         // frames AAbyssCharacterActor::IsAshnumbBandShown on the hero
	};

	/** An EvTelegraph the probe expects a crimson decal for: a ground strike (not an on-death fuse) in a themed zone. */
	inline bool ExpectsDecal(abyss::AbilityKind Kind, abyss::TelegraphSource Source, bool bThemedZone)
	{
		return bThemedZone && Source != abyss::TelegraphSource::OnDeath
			&& AbyssTelegraphRules::TelegraphLook(Kind, Source, false, bThemedZone) == AbyssTelegraphRules::ELook::Decal;
	}

	/** Folds one frame's drawn counts into the tally. */
	inline void Sample(FTally& T, int DrawnDecals, int DrawnArenas, int Husks, bool bAshnumbExpected, bool bBandShown)
	{
		T.PeakDecals = DrawnDecals > T.PeakDecals ? DrawnDecals : T.PeakDecals;
		T.PeakArenaDecals = DrawnArenas > T.PeakArenaDecals ? DrawnArenas : T.PeakArenaDecals;
		T.PeakHusks = Husks > T.PeakHusks ? Husks : T.PeakHusks;
		T.AshnumbFrames += bAshnumbExpected ? 1 : 0;
		T.BandFrames += bAshnumbExpected && bBandShown ? 1 : 0;
	}

	/** "" while the presentation keeps up with the core; else what the core announced that nothing drew. */
	inline std::string Verdict(const FTally& T)
	{
		std::string Out;
		auto Add = [&Out](const std::string& Line)
		{
			Out += Out.empty() ? Line : "; " + Line;
		};
		if (T.StrikeTelegraphs + T.Hazards > 0 && T.PeakDecals == 0)
		{
			Add(std::to_string(T.StrikeTelegraphs) + " ground-strike telegraphs and " + std::to_string(T.Hazards)
				+ " hazards but no decal drawn (M_AF_Telegraph / SM_FX_Quad / the manifest's M_AF_Telegraph contract?)");
		}
		if (T.Arenas > 0 && T.PeakArenaDecals == 0)
		{
			Add(std::to_string(T.Arenas) + " arenas but no arena ring drawn");
		}
		if (T.Corpses > 0 && T.PeakHusks == 0)
		{
			Add(std::to_string(T.Corpses) + " corpse entries but no husk drawn (M_AF_Toon_Ash / M_AF_Outline_Ash?)");
		}
		if (T.AshnumbFrames >= MinBandFrames && T.BandFrames == 0)
		{
			Add(std::to_string(T.AshnumbFrames) + " frames of Ashnumb but no band on the hero (M_AF_Toon_Ashnumb?)");
		}
		return Out;
	}
}
