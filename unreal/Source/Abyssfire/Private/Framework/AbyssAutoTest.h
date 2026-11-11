// Command-line switches of the in-game autotest (Framework/AbyssAutoTestSubsystem.h runs it; README.md "In-game
// autotest"). Non-Shipping builds only; without -abyssautotest nothing here has any effect.
//
//   -abyssautotest[=smoke|chapter]   run the autotest (default smoke)
//   -abyssclass=warrior|mage|rogue   hero class (default warrior)
//   -abyssautotestdir=<dir>          output directory (default <Project>/Saved/AutoTest; relative = launch directory).
//                                    Never the player's saves: a directory whose SaveGames/ would be, or contain,
//                                    <Project>/Saved/SaveGames is refused; a later run cleans only a directory an
//                                    earlier run marked (.abyssautotest)
//   -abyssautotestmaxsec=<seconds>   hard wall-clock limit (default 600 smoke, 2400 chapter)
//   -abyssbuildsha=<sha>             recorded in report.json
//   -abyssseed=<n>                   seed of the run's new game (1 .. 2^63-1; default: drawn, recorded in report.json)
//   -abyssautotestshotsec=<seconds>  periodic screenshot interval while in game (default 60, 0 = off)
//   -abyssautotestignore=<LogA,LogB> log categories whose Error lines are recorded but do not fail the run
// Smoothness measurement (performance audit F2 / section 6; Framework/AbyssSmoothnessRules.h):
//   -abyssnoshots                    no event or periodic screenshots (a measurement run: a screenshot stalls its frame)
//   -abyssstatunit                   turns `stat unit` on (its overlay shows, in every screenshot too: meant with
//                                    -abyssnoshots) and records its game / render / RHI / GPU ms per frame; without
//                                    it those report fields are null
//   -abysssmooth=<name:value,...>    thresholds of the smoothness summary: p50, p95, p99 (ms), hitch (frames over
//                                    100 ms per minute), max (ms), ratio (game / real), frames (minimum play frames)
//   -abysssmoothgate                 a summary other than "smooth" fails the run (default: informational)
//   -abysstimelinerows=<n>           most rows of frames.csv (default 120000; 0 = no file)
#pragma once

#include "CoreMinimal.h"

#include "abyss/base/Enums.h"
#include "abyss/tools/AutoPilot.h"

#include "Framework/AbyssSmoothnessRules.h"

struct FAbyssAutoTestOptions
{
	abyss::AutoPilotScenario Scenario = abyss::AutoPilotScenario::Smoke;
	abyss::ClassId HeroClass = abyss::ClassId::Warrior;
	/** Absolute output directory (report.json, screenshots/, SaveGames/). */
	FString OutputDir;
	double MaxSeconds = 600.0;
	double ShotIntervalSeconds = 60.0;
	FString BuildSha;
	/** Seed of the new game (GetNewGameSeed). */
	uint64 Seed = 0;
	TArray<FName> IgnoredLogCategories;
	/** -abyssnoshots: no screenshot is ever requested (the pacing of the picture points is kept). */
	bool bNoShots = false;
	/** -abyssstatunit: `stat unit` on, thread and GPU times sampled. */
	bool bStatUnit = false;
	/** -abysssmoothgate: a smoothness summary other than "smooth" fails the run. */
	bool bSmoothGate = false;
	/** -abysssmooth=: the summary's limits. */
	AbyssSmoothnessRules::FThresholds Smooth;
	/** -abysstimelinerows=: most rows of frames.csv. */
	int32 TimelineRows = 120000;
	/** Unusable switch values (unknown scenario / class): the run reports them and fails without playing. */
	TArray<FString> Problems;
};

namespace AbyssAutoTest
{
	/** -abyssautotest or -abyssautotest=<scenario> is on the command line (always false in Shipping and in the editor). */
	bool IsRequested();
	/** All switches, with defaults. */
	FAbyssAutoTestOptions ParseOptions();
	/**
	 * Save slots and settings.json of an autotest run: <output dir>/SaveGames, isolated from the player's saves
	 * (FAbyssSaveStorage); a separate directory under <Project>/Saved/AutoTest when the output directory is refused.
	 * Empty when no autotest runs.
	 */
	FString GetSaveDirOverride();
	/**
	 * Seed for the autotest's new game (UAbyssGameInstance::StartNewGame): -abyssseed=<n> when given, else drawn once
	 * per process, so the game plays the seed report.json records. 0 when no autotest runs (StartNewGame draws its own).
	 */
	uint64 GetNewGameSeed();
}
