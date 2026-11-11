// The in-game autotest's smoothness measurement (performance audit F2 and section 6; UAbyssAutoTestSubsystem).
//
// The first engine run could not say how smooth the game is: its frame statistics mixed the test's own screenshots, a
// first-run shader backlog and zone loads into one untagged list. Everything the autotest now derives from a run lives
// here, so it is tested without the engine (unreal/PresentationTests/tests/smoothness_test.cpp):
//
//   * FProbeFrame       what the module's own hooks counted during one engine frame (Framework/AbyssPerfProbe.h)
//   * FFrameSample      one engine frame: wall-clock ms, tags, scene, sim steps, thread times (negative = unavailable)
//   * FSceneTracker     which scene a frame belongs to (menu, prologue, camp, each panel, field travel, each fight,
//                       each boss phase, ...)
//   * Summarize         percentiles and counts over 33 / 50 / 100 ms for all frames and for "play" frames (no tag of
//                       NonPlayMask), overall, in game, per scene, over the last 60 s; game / real ratio (play frames
//                       that follow a play frame, RatioFrame); the longest frames of the run and of play; the bounded
//                       frame timeline
//   * FCommandTracker   ms from a player command to the sim frame that acts on it and to the end of that engine frame
//   * FContactTracker   how far the shown pose is from the hero attack's contact beat when its hit is presented
//   * Judge             the "smoothness" verdict against plain thresholds (FThresholds, -abysssmooth=)
//   * WritePerf / WriteSmoothness   the report.json blocks, AppendTimelineRow the CSV
//
// A value the engine did not provide is written as JSON null (CSV: an empty field), never as 0.
//
// UE-free and header-only (std + AbyssCore public headers).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "abyss/base/Json.h"
#include "abyss/base/SimClock.h"
#include "abyss/sim/Commands.h"

namespace AbyssSmoothnessRules
{
	// =================================================================================================================
	// Tags
	// =================================================================================================================

	/** Why a frame is not ordinary play (the first six), and what else was true of it (informational). */
	enum ETag : std::uint32_t
	{
		TagShot = 1u << 0,            // an autotest screenshot was requested, drawn or read back
		TagZoneBuild = 1u << 1,       // UAbyssWorldBuilder::BuildZone ran
		TagCompileWait = 1u << 2,     // the uncooked zone-entry compile wait blocked (abyss.WaitForCompilation)
		TagStory = 1u << 3,           // a story beat plays (prologue, chapter card, cutscene, boss intro)
		TagPanel = 1u << 4,           // a HUD panel or a core modal is open
		TagLoading = 1u << 5,         // boot, new game until the session, a zone transition and its fade-in
		// informational: these do not take a frame out of the play statistics
		TagBacklog = 1u << 6,         // shader jobs or asset builds were outstanding (uncooked runs)
		TagStepCap = 1u << 7,         // the sim ran its step budget and dropped time: slow motion for the player
		TagDilated = 1u << 8,         // S6 slow motion (elite kill): the sim is slow on purpose
		TagFrozen = 1u << 9,          // the world is frozen (pause, dialogue, cinematic)
		TagQualityRefresh = 1u << 10, // UAbyssWorldBuilder::RefreshQuality applied something
	};
	inline constexpr std::uint32_t NonPlayMask = TagShot | TagZoneBuild | TagCompileWait | TagStory | TagPanel | TagLoading;
	inline constexpr int TagCount = 11;

	inline const char* TagName(int Bit)
	{
		static const char* const Names[TagCount] = { "shot", "zoneBuild", "compileWait", "story", "panel", "loading",
			"backlog", "stepCap", "dilated", "frozen", "qualityRefresh" };
		return Bit >= 0 && Bit < TagCount ? Names[Bit] : "";
	}

	/** "shot|zoneBuild", "" for an untagged frame (the CSV column). */
	inline std::string TagList(std::uint32_t Tags)
	{
		std::string Out;
		for (int Bit = 0; Bit < TagCount; ++Bit)
		{
			if ((Tags & (1u << Bit)) != 0)
			{
				if (!Out.empty())
				{
					Out += '|';
				}
				Out += TagName(Bit);
			}
		}
		return Out;
	}

	inline bool IsPlay(std::uint32_t Tags)
	{
		return (Tags & NonPlayMask) == 0;
	}

	// =================================================================================================================
	// What the module's hooks count during one engine frame
	// =================================================================================================================

	enum class ECounter : std::uint8_t
	{
		LogRebuilds,            // SAbyssCombatLog::Rebuild
		HoverPicks,             // AbyssPicking picks (hover and click)
		HoverVisited,           // registry entries a pick looked at
		HoverProjected,         // boxes a pick projected corner by corner (after the pre-cull)
		MaterialLookups,        // UAbyssAssetLibrary::LoadMaterial calls that missed the resolved-name cache (ms)
		PackageProbes,          // FPackageName::DoesPackageExist through the asset library
		SyncLoads,              // FSoftObjectPath::TryLoad in UAbyssAssetLibrary::LoadObjectAt (ms)
		UiTextureLoads,         // UAbyssUiSubsystem::FindUiTexture cache misses (ms)
		VfxPoolCreations,       // UAbyssVfxSystem::FindOrCreatePool creations (ms)
		Spawns,                 // UAbyssWorldBuilder::SpawnEntity (ms)
		ZoneBuilds,             // UAbyssWorldBuilder::BuildZone (ms)
		CompileWaits,           // UAbyssAssetLibrary::WaitForPendingCompilation that waited (ms)
		QualityRefreshCalls,    // UAbyssWorldBuilder::RefreshQuality calls (ms)
		QualityRefreshApplied,  // ... that changed a console variable or fanned out the profile
		SimFrame,               // GameSim::Frame (ms)
		DriverTick,             // UAbyssSimDriver::Tick with a session: sim, dispatch, world and UI sync (ms)
		Count,
	};
	inline constexpr int CounterCount = static_cast<int>(ECounter::Count);

	inline const char* CounterName(ECounter Counter)
	{
		static const char* const Names[CounterCount] = { "logRebuilds", "picks", "pickVisited", "pickProjected",
			"materialLookups", "packageProbes", "syncLoads", "uiTextureLoads", "vfxPoolCreations", "spawns", "zoneBuilds",
			"compileWaits", "qualityRefreshCalls", "qualityRefreshApplied", "simFrames", "driverTicks" };
		const int Index = static_cast<int>(Counter);
		return Index >= 0 && Index < CounterCount ? Names[Index] : "";
	}

	/** Written by the hooks (game thread), read and reset by the autotest once per engine frame. */
	struct FProbeFrame
	{
		std::uint32_t N[CounterCount] = {};
		double Ms[CounterCount] = {};
		/** Floating texts alive at the world layer's last paint (-1: not painted this frame). */
		int FloatTexts = -1;
		/** Pending work the compile wait saw before it blocked (-1: no wait counted this frame, or no counts). */
		int ShaderJobsAtWait = -1;
		int AssetBuildsAtWait = -1;
		/** FPlatformTime seconds when GameSim::Frame returned (0: the sim did not run this frame). */
		double SimFrameEndSec = 0.0;
		/** What the sim was fed (the world's undilated delta, ms). */
		double SimDtMs = 0.0;

		void Add(ECounter Counter, std::uint32_t Count = 1, double Milliseconds = 0.0)
		{
			N[static_cast<int>(Counter)] += Count;
			Ms[static_cast<int>(Counter)] += Milliseconds;
		}
		std::uint32_t Num(ECounter Counter) const { return N[static_cast<int>(Counter)]; }
		double Millis(ECounter Counter) const { return Ms[static_cast<int>(Counter)]; }
		void Reset() { *this = FProbeFrame(); }
	};

	/** Run totals of the hook counters (count and inclusive ms; nested scopes overlap, e.g. loads inside a spawn). */
	struct FProbeTotals
	{
		std::uint64_t N[CounterCount] = {};
		double Ms[CounterCount] = {};
		std::uint32_t PeakPerFrame[CounterCount] = {};
		double PeakMsPerFrame[CounterCount] = {};

		void Add(const FProbeFrame& Frame)
		{
			for (int Index = 0; Index < CounterCount; ++Index)
			{
				N[Index] += Frame.N[Index];
				Ms[Index] += Frame.Ms[Index];
				PeakPerFrame[Index] = Frame.N[Index] > PeakPerFrame[Index] ? Frame.N[Index] : PeakPerFrame[Index];
				PeakMsPerFrame[Index] = Frame.Ms[Index] > PeakMsPerFrame[Index] ? Frame.Ms[Index] : PeakMsPerFrame[Index];
			}
		}
	};

	// =================================================================================================================
	// The frame sample
	// =================================================================================================================

	inline constexpr float Unavailable = -1.f;

	struct FFrameSample
	{
		/** Run seconds at the end of the frame (the autotest's ticker). */
		double RealSec = 0.0;
		/** Wall-clock time since the previous frame's end. */
		float FrameMs = 0.f;
		/** Engine values, Unavailable when the engine did not give them: the frame-cap sleep, the stat unit numbers. */
		float IdleMs = Unavailable;
		float GameMs = Unavailable;
		float RenderMs = Unavailable;
		float RhiMs = Unavailable;
		float GpuMs = Unavailable;
		/** Our own scopes, Unavailable without a session. */
		float SimMs = Unavailable;
		float DriverMs = Unavailable;
		/** What the sim was fed this frame (ms; 0 without a session). */
		float SimDtMs = 0.f;
		std::uint32_t Tags = 0;
		std::uint16_t Scene = 0;
		std::uint8_t Steps = 0;
		/** Outstanding compile work, -1 when the engine gives no count (cooked builds). */
		std::int32_t ShaderJobs = -1;
		std::int32_t AssetBuilds = -1;
		// what happened this frame (the longest-frames context)
		std::uint16_t EvLogLines = 0;
		std::uint16_t LogRebuilds = 0;
		std::uint16_t Spawns = 0;
		std::uint16_t UiTextureLoads = 0;
		std::uint16_t VfxPoolCreations = 0;
		std::uint16_t SyncLoads = 0;
		std::uint16_t PickProjected = 0;
		std::uint16_t Commands = 0;
		float SpawnMs = 0.f;
		float ZoneBuildMs = 0.f;
		float CompileWaitMs = 0.f;
		float LoadMs = 0.f;          // UI texture loads + VFX pool creations + sync loads + material lookups
		// gauges (-1: unknown)
		std::int16_t Husks = -1;
		std::int16_t Characters = -1;
		std::int16_t Registry = -1;
		std::int16_t FloatTexts = -1;
	};

	inline std::uint16_t Clamp16(std::uint32_t Value)
	{
		return static_cast<std::uint16_t>(Value > 65535u ? 65535u : Value);
	}

	/** The hook counters of a frame, copied into its sample. */
	inline void ApplyProbe(FFrameSample& Sample, const FProbeFrame& Probe, bool bHasSession)
	{
		Sample.LogRebuilds = Clamp16(Probe.Num(ECounter::LogRebuilds));
		Sample.Spawns = Clamp16(Probe.Num(ECounter::Spawns));
		Sample.UiTextureLoads = Clamp16(Probe.Num(ECounter::UiTextureLoads));
		Sample.VfxPoolCreations = Clamp16(Probe.Num(ECounter::VfxPoolCreations));
		Sample.SyncLoads = Clamp16(Probe.Num(ECounter::SyncLoads));
		Sample.PickProjected = Clamp16(Probe.Num(ECounter::HoverProjected));
		Sample.SpawnMs = static_cast<float>(Probe.Millis(ECounter::Spawns));
		Sample.ZoneBuildMs = static_cast<float>(Probe.Millis(ECounter::ZoneBuilds));
		Sample.CompileWaitMs = static_cast<float>(Probe.Millis(ECounter::CompileWaits));
		Sample.LoadMs = static_cast<float>(Probe.Millis(ECounter::UiTextureLoads) + Probe.Millis(ECounter::VfxPoolCreations)
			+ Probe.Millis(ECounter::SyncLoads) + Probe.Millis(ECounter::MaterialLookups));
		if (bHasSession && Probe.Num(ECounter::DriverTick) > 0)
		{
			Sample.SimMs = static_cast<float>(Probe.Millis(ECounter::SimFrame));
			Sample.DriverMs = static_cast<float>(Probe.Millis(ECounter::DriverTick));
			Sample.SimDtMs = static_cast<float>(Probe.SimDtMs);
		}
		if (Probe.Num(ECounter::ZoneBuilds) > 0)
		{
			Sample.Tags |= TagZoneBuild;
		}
		if (Probe.Num(ECounter::CompileWaits) > 0)
		{
			Sample.Tags |= TagCompileWait;
		}
		if (Probe.Num(ECounter::QualityRefreshApplied) > 0)
		{
			Sample.Tags |= TagQualityRefresh;
		}
		if (Probe.FloatTexts >= 0)
		{
			Sample.FloatTexts = static_cast<std::int16_t>(Probe.FloatTexts > 32767 ? 32767 : Probe.FloatTexts);
		}
	}

	/**
	 * The sim ran its whole step budget and certainly dropped time: it was fed at least one step more than it could run
	 * (SimClock::EndFrame drops whole steps then). The player sees slow motion. A frame over the budget by less than a
	 * step may or may not drop one (the accumulator's remainder decides), so it is not counted.
	 */
	inline bool StepCapHit(int Steps, int MaxSteps, double SimDtMs)
	{
		return MaxSteps > 0 && Steps >= MaxSteps && SimDtMs >= (MaxSteps + 1) * abyss::kSimStepMs;
	}

	/**
	 * TagFrozen: the world was frozen when the frame's sim ran or when it returned. The first and the last frame of a
	 * freeze run no step by design (the accumulator is dropped), so neither belongs in the game / real ratio.
	 */
	inline bool FrozenFrame(bool bFrozenBefore, bool bFrozenAfter)
	{
		return bFrozenBefore || bFrozenAfter;
	}

	/**
	 * Sim time such a frame lost against the wall clock, to within one step: the real time it was fed minus what ran.
	 * Not clamped to SimConfig::maxFrameMs: the player loses the time the clock's clamp drops as well, and the game /
	 * real ratio counts the same unclamped time (ClampLostMs is the clamp's part of it).
	 */
	inline double LostSimMs(int Steps, double SimDtMs)
	{
		const double Lost = SimDtMs - Steps * abyss::kSimStepMs;
		return Lost > 0.0 ? Lost : 0.0;
	}

	/** The part of LostSimMs the clock dropped before stepping: what the frame was fed beyond SimConfig::maxFrameMs. */
	inline double ClampLostMs(double SimDtMs, double MaxFrameMs)
	{
		return MaxFrameMs > 0.0 && SimDtMs > MaxFrameMs ? SimDtMs - MaxFrameMs : 0.0;
	}

	/**
	 * A frame whose sim time counts in the play statistics (game / real ratio, step-cap frames, lost sim time): the
	 * frame is a play frame and so was the one before it. The engine feeds frame N the duration of frame N - 1, so the
	 * frame after a screenshot, a zone build or a compile wait is fed that whole hitch and runs its step budget: that
	 * is the tagged frame's cost, not slow motion in play. (A long untagged play frame still counts: the frame after
	 * it is a ratio frame.)
	 */
	inline bool RatioFrame(std::uint32_t Tags, std::uint32_t PrevTags)
	{
		return IsPlay(Tags) && IsPlay(PrevTags);
	}

	// =================================================================================================================
	// Scenes
	// =================================================================================================================

	enum class EPhase : std::uint8_t { Boot, Menu, Starting, Playing, Ending };

	enum class ESceneKind : std::uint8_t { Boot, Menu, Starting, Prologue, Story, Panel, Camp, Travel, Fight, Boss, Ending };

	inline const char* SceneKindName(ESceneKind Kind)
	{
		switch (Kind)
		{
		case ESceneKind::Boot: return "boot";
		case ESceneKind::Menu: return "menu";
		case ESceneKind::Starting: return "starting";
		case ESceneKind::Prologue: return "prologue";
		case ESceneKind::Story: return "story";
		case ESceneKind::Panel: return "panel";
		case ESceneKind::Camp: return "camp";
		case ESceneKind::Travel: return "travel";
		case ESceneKind::Fight: return "fight";
		case ESceneKind::Boss: return "boss";
		case ESceneKind::Ending: return "ending";
		}
		return "";
	}

	/** Frames of a session with the world on screen (what "in game" statistics cover). */
	inline bool IsInGameKind(ESceneKind Kind)
	{
		return Kind != ESceneKind::Boot && Kind != ESceneKind::Menu && Kind != ESceneKind::Starting;
	}

	struct FSceneInput
	{
		EPhase Phase = EPhase::Boot;
		double RealSec = 0.0;
		bool bStory = false;          // Snapshot::storyBusy or cinematic
		bool bReachedCamp = false;    // the bot's "intro" checkpoint passed: later beats are "story", not "prologue"
		std::string_view Panel;       // "" = none; the panel or modal on screen
		bool bInSafeZone = false;     // the hero is inside a camp's safe radius
		bool bInCombat = false;       // HeroView::inCombat
		bool bBoss = false;           // the boss bar shows
		std::string_view BossId;      // the boss's monster definition id
		int BossPhase = 0;
		/** Wave 3: the sub-dungeon the hero is in (ZoneView::subDungeon.id; "" on the surface). */
		std::string_view SubDungeon;
		/** Wave 3: its cleanse sequence plays (the lamps, the grade: AbyssSubDungeonRules::ECleansePhase::Playing). */
		bool bCleansing = false;
	};

	struct FScene
	{
		std::string Label;
		ESceneKind Kind = ESceneKind::Boot;
	};

	/**
	 * Gives every frame a scene. One label per place of the run ("menu", "camp", "travel", "panel_inventory"), one per
	 * fight ("fight_001", ...: a fight ends when the hero has been out of combat for FightGapSec; past MaxFights they
	 * share "fight_more") and one per boss phase ("boss_<id>_p<phase>"). Order of precedence: the run's phase, a story
	 * beat, a panel, the boss, a fight, a sub-dungeon ("dungeon_<id>", "dungeon_<id>_cleanse"), the camp, travel.
	 */
	class FSceneTracker
	{
	public:
		static constexpr double FightGapSec = 2.0;
		static constexpr int MaxFights = 64;
		static constexpr std::size_t MaxScenes = 400;

		std::uint16_t Update(const FSceneInput& In)
		{
			switch (In.Phase)
			{
			case EPhase::Boot: return Find("boot", ESceneKind::Boot);
			case EPhase::Menu: return Find("menu", ESceneKind::Menu);
			case EPhase::Starting: return Find("starting", ESceneKind::Starting);
			case EPhase::Ending: return Find("ending", ESceneKind::Ending);
			case EPhase::Playing: break;
			}
			if (In.bInCombat)
			{
				// A fight goes on under a story beat or a panel: the gap is measured on combat alone.
				if (FightNumber == 0 || In.RealSec - LastCombatSec > FightGapSec)
				{
					++FightNumber;
				}
				LastCombatSec = In.RealSec;
			}
			if (In.bStory)
			{
				return In.bReachedCamp ? Find("story", ESceneKind::Story) : Find("prologue", ESceneKind::Prologue);
			}
			if (!In.Panel.empty())
			{
				return Find("panel_" + std::string(In.Panel), ESceneKind::Panel);
			}
			if (In.bBoss && In.bInCombat)
			{
				return Find("boss_" + std::string(In.BossId) + "_p" + std::to_string(In.BossPhase), ESceneKind::Boss);
			}
			if (In.bInCombat)
			{
				if (FightNumber > MaxFights)
				{
					return Find("fight_more", ESceneKind::Fight);
				}
				char Label[24];
				std::snprintf(Label, sizeof(Label), "fight_%03d", FightNumber);
				return Find(Label, ESceneKind::Fight);
			}
			if (!In.SubDungeon.empty())
			{
				// Wave 3: a sub-dungeon is its own place ("dungeon_sub_rf_pit"), its cleanse sequence its own scene (the six
				// seconds the lamps, the grade and the embers run in); kind Travel, so the summaries group it as before.
				// The label ("dungeon_sub_rf_pit" is past the small-string buffer) is built when the place or the flag
				// changes, not every frame: the measurement must not allocate in the scene it measures.
				if (!bSubSceneKnown || bSubSceneCleansing != In.bCleansing || SubSceneId != In.SubDungeon)
				{
					SubSceneId.assign(In.SubDungeon);
					bSubSceneCleansing = In.bCleansing;
					SubSceneIndex = Find("dungeon_" + SubSceneId + (In.bCleansing ? "_cleanse" : ""), ESceneKind::Travel);
					bSubSceneKnown = true;
				}
				return SubSceneIndex;
			}
			return In.bInSafeZone ? Find("camp", ESceneKind::Camp) : Find("travel", ESceneKind::Travel);
		}

		const std::vector<FScene>& Scenes() const { return List; }
		int Fights() const { return FightNumber; }

	private:
		std::uint16_t Find(const std::string& Label, ESceneKind Kind)
		{
			for (std::size_t Index = 0; Index < List.size(); ++Index)
			{
				if (List[Index].Label == Label)
				{
					return static_cast<std::uint16_t>(Index);
				}
			}
			if (List.size() >= MaxScenes)
			{
				// Never reached by a real run (a few dozen labels): the kind's catch-all keeps the index valid. There are
				// 11 kinds, so the list ends at MaxScenes + 11 at most.
				const std::string More = std::string(SceneKindName(Kind)) + "_more";
				for (std::size_t Index = 0; Index < List.size(); ++Index)
				{
					if (List[Index].Label == More)
					{
						return static_cast<std::uint16_t>(Index);
					}
				}
				List.push_back(FScene{ More, Kind });
				return static_cast<std::uint16_t>(List.size() - 1);
			}
			List.push_back(FScene{ Label, Kind });
			return static_cast<std::uint16_t>(List.size() - 1);
		}

		std::vector<FScene> List;
		/** The last sub-dungeon scene resolved (its label is built once per place / cleanse flag). */
		std::string SubSceneId;
		bool bSubSceneCleansing = false;
		bool bSubSceneKnown = false;
		std::uint16_t SubSceneIndex = 0;
		int FightNumber = 0;
		double LastCombatSec = -1.0e9;
	};

	/**
	 * Zone transitions as "loading": from EvZone TransitionBegan (or the session's start) until FadeSec after the zone
	 * was entered (the 400 ms fade-in of world_constants.json plus the first frames' late loads).
	 */
	class FLoadingWindow
	{
	public:
		static constexpr double FadeSec = 0.5;

		void Begin() { bOpen = true; }
		void ZoneEntered(double RealSec)
		{
			bOpen = false;
			UntilSec = RealSec + FadeSec;
		}
		bool IsLoading(double RealSec) const { return bOpen || RealSec <= UntilSec; }

	private:
		bool bOpen = false;
		double UntilSec = -1.0;
	};

	// =================================================================================================================
	// Statistics
	// =================================================================================================================

	/** Nearest-rank percentile of an ascending list (the rule the report's `frames` block has always used). */
	inline double Percentile(const std::vector<float>& Sorted, double P)
	{
		if (Sorted.empty())
		{
			return 0.0;
		}
		const double Rank = std::ceil(P * static_cast<double>(Sorted.size()));
		std::ptrdiff_t Index = static_cast<std::ptrdiff_t>(Rank) - 1;
		const std::ptrdiff_t Last = static_cast<std::ptrdiff_t>(Sorted.size()) - 1;
		Index = Index < 0 ? 0 : (Index > Last ? Last : Index);
		return static_cast<double>(Sorted[static_cast<std::size_t>(Index)]);
	}

	inline constexpr double Over33Ms = 1000.0 / 30.0;   // a frame the 30 fps line misses
	inline constexpr double Over50Ms = 50.0;
	inline constexpr double Over100Ms = 100.0;

	struct FStats
	{
		int Count = 0;
		double TotalSec = 0.0;
		double AvgMs = 0.0;
		double P50Ms = 0.0;
		double P95Ms = 0.0;
		double P99Ms = 0.0;
		double MaxMs = 0.0;
		int Over33 = 0;
		int Over50 = 0;
		int Over100 = 0;

		/** Frames over 100 ms per minute of these frames' own time (0 without frames). */
		double HitchesPerMinute() const { return TotalSec > 0.0 ? static_cast<double>(Over100) * 60.0 / TotalSec : 0.0; }
	};

	/** Sorts FrameMs in place. */
	inline FStats ComputeStats(std::vector<float>& FrameMs)
	{
		FStats Stats;
		std::sort(FrameMs.begin(), FrameMs.end());
		Stats.Count = static_cast<int>(FrameMs.size());
		if (FrameMs.empty())
		{
			return Stats;
		}
		double Sum = 0.0;
		for (const float Value : FrameMs)
		{
			Sum += static_cast<double>(Value);
			Stats.Over33 += static_cast<double>(Value) > Over33Ms ? 1 : 0;
			Stats.Over50 += static_cast<double>(Value) > Over50Ms ? 1 : 0;
			Stats.Over100 += static_cast<double>(Value) > Over100Ms ? 1 : 0;
		}
		Stats.TotalSec = Sum / 1000.0;
		Stats.AvgMs = Sum / static_cast<double>(FrameMs.size());
		Stats.P50Ms = Percentile(FrameMs, 0.50);
		Stats.P95Ms = Percentile(FrameMs, 0.95);
		Stats.P99Ms = Percentile(FrameMs, 0.99);
		Stats.MaxMs = static_cast<double>(FrameMs.back());
		return Stats;
	}

	/** Average / p95 / max of a value that may be unavailable on some frames (Count 0: written as null). */
	struct FValueStats
	{
		int Count = 0;
		double AvgMs = 0.0;
		double P95Ms = 0.0;
		double MaxMs = 0.0;
	};

	inline FValueStats ComputeValueStats(std::vector<float>& Values)
	{
		FValueStats Stats;
		std::sort(Values.begin(), Values.end());
		Stats.Count = static_cast<int>(Values.size());
		if (Values.empty())
		{
			return Stats;
		}
		double Sum = 0.0;
		for (const float Value : Values)
		{
			Sum += static_cast<double>(Value);
		}
		Stats.AvgMs = Sum / static_cast<double>(Values.size());
		Stats.P95Ms = Percentile(Values, 0.95);
		Stats.MaxMs = static_cast<double>(Values.back());
		return Stats;
	}

	struct FPeakAvg
	{
		int Count = 0;
		int Peak = 0;
		double Sum = 0.0;
		void Add(int Value)
		{
			if (Value >= 0)
			{
				++Count;
				Sum += Value;
				Peak = Value > Peak ? Value : Peak;
			}
		}
		double Avg() const { return Count > 0 ? Sum / Count : 0.0; }
	};

	/** The statistics of one set of frames (the whole run, one scene, the last 60 s, ...). */
	struct FGroup
	{
		std::string Label;
		std::string Kind;
		double FirstRealSec = 0.0;
		double LastRealSec = 0.0;
		FStats All;
		FStats Play;            // frames without a NonPlayMask tag
		FStats PlayNoBacklog;   // ... and with no outstanding compile work (equal to Play when the engine gives no count)
		int TagFrames[TagCount] = {};
		// game / real: sim time run against the real time the sim was fed, over unfrozen, undilated session frames that
		// are ratio frames (RatioFrame: play frames after a play frame). What the verdict judges.
		double RatioSimMs = 0.0;
		double RatioRealMs = 0.0;
		int StepCapFrames = 0;
		double LostSimMs = 0.0;
		double ClampLostMs = 0.0;
		// the same over every unfrozen, undilated session frame, tagged or not (informational: includes the frames
		// that swallow a zone build, a compile wait or a screenshot)
		double AllRatioSimMs = 0.0;
		double AllRatioRealMs = 0.0;
		int AllStepCapFrames = 0;
		double AllLostSimMs = 0.0;
		FValueStats Idle, Game, Render, Rhi, Gpu, Sim, Driver;
		FPeakAvg Husks, Characters, Registry, FloatTexts, EvLogLines, PickProjected;

		bool HasRatio() const { return RatioRealMs > 0.0; }
		double GameRealRatio() const { return RatioRealMs > 0.0 ? RatioSimMs / RatioRealMs : 0.0; }
		bool HasAllRatio() const { return AllRatioRealMs > 0.0; }
		double AllGameRealRatio() const { return AllRatioRealMs > 0.0 ? AllRatioSimMs / AllRatioRealMs : 0.0; }
	};

	struct FSimLimits
	{
		int MaxStepsPerFrame = 4;      // SimConfig::maxStepsPerFrame
		double MaxFrameMs = 250.0;     // SimConfig::maxFrameMs
	};

	class FGroupBuilder
	{
	public:
		FGroupBuilder() = default;
		FGroupBuilder(std::string InLabel, std::string InKind)
		{
			Group.Label = std::move(InLabel);
			Group.Kind = std::move(InKind);
		}

		/** bRatioFrame: RatioFrame(S.Tags, the tags of the run's previous sample). */
		void Add(const FFrameSample& S, const FSimLimits& Limits, bool bRatioFrame)
		{
			if (AllMs.empty())
			{
				Group.FirstRealSec = S.RealSec;
			}
			Group.LastRealSec = S.RealSec;
			AllMs.push_back(S.FrameMs);
			if (IsPlay(S.Tags))
			{
				PlayMs.push_back(S.FrameMs);
				if ((S.Tags & TagBacklog) == 0)
				{
					PlayNoBacklogMs.push_back(S.FrameMs);
				}
			}
			for (int Bit = 0; Bit < TagCount; ++Bit)
			{
				Group.TagFrames[Bit] += (S.Tags & (1u << Bit)) != 0 ? 1 : 0;
			}
			if (S.DriverMs >= 0.f && S.SimDtMs > 0.f && (S.Tags & (TagFrozen | TagDilated)) == 0)
			{
				Group.AllRatioSimMs += S.Steps * abyss::kSimStepMs;
				Group.AllRatioRealMs += static_cast<double>(S.SimDtMs);
				if (bRatioFrame)
				{
					Group.RatioSimMs += S.Steps * abyss::kSimStepMs;
					Group.RatioRealMs += static_cast<double>(S.SimDtMs);
				}
			}
			if ((S.Tags & TagStepCap) != 0)
			{
				const double Lost = AbyssSmoothnessRules::LostSimMs(S.Steps, static_cast<double>(S.SimDtMs));
				++Group.AllStepCapFrames;
				Group.AllLostSimMs += Lost;
				if (bRatioFrame)
				{
					++Group.StepCapFrames;
					Group.LostSimMs += Lost;
					Group.ClampLostMs += AbyssSmoothnessRules::ClampLostMs(static_cast<double>(S.SimDtMs), Limits.MaxFrameMs);
				}
			}
			Push(Idle, S.IdleMs);
			Push(Game, S.GameMs);
			Push(Render, S.RenderMs);
			Push(Rhi, S.RhiMs);
			Push(Gpu, S.GpuMs);
			Push(Sim, S.SimMs);
			Push(Driver, S.DriverMs);
			Group.Husks.Add(S.Husks);
			Group.Characters.Add(S.Characters);
			Group.Registry.Add(S.Registry);
			Group.FloatTexts.Add(S.FloatTexts);
			if (S.DriverMs >= 0.f)
			{
				Group.EvLogLines.Add(S.EvLogLines);
				Group.PickProjected.Add(S.PickProjected);
			}
		}

		bool Empty() const { return AllMs.empty(); }

		FGroup Finish()
		{
			Group.All = ComputeStats(AllMs);
			Group.Play = ComputeStats(PlayMs);
			Group.PlayNoBacklog = ComputeStats(PlayNoBacklogMs);
			Group.Idle = ComputeValueStats(Idle);
			Group.Game = ComputeValueStats(Game);
			Group.Render = ComputeValueStats(Render);
			Group.Rhi = ComputeValueStats(Rhi);
			Group.Gpu = ComputeValueStats(Gpu);
			Group.Sim = ComputeValueStats(Sim);
			Group.Driver = ComputeValueStats(Driver);
			return std::move(Group);
		}

	private:
		static void Push(std::vector<float>& List, float Value)
		{
			if (Value >= 0.f)
			{
				List.push_back(Value);
			}
		}

		FGroup Group;
		std::vector<float> AllMs, PlayMs, PlayNoBacklogMs;
		std::vector<float> Idle, Game, Render, Rhi, Gpu, Sim, Driver;
	};

	struct FMilestone
	{
		std::string Kind;
		std::string Detail;
		double RealSec = 0.0;
	};

	/** Index of the milestone nearest in time, -1 without milestones. */
	inline int NearestMilestone(const std::vector<FMilestone>& Milestones, double RealSec)
	{
		int Best = -1;
		double BestGap = 0.0;
		for (std::size_t Index = 0; Index < Milestones.size(); ++Index)
		{
			const double Gap = std::fabs(Milestones[Index].RealSec - RealSec);
			if (Best < 0 || Gap < BestGap)
			{
				Best = static_cast<int>(Index);
				BestGap = Gap;
			}
		}
		return Best;
	}

	inline constexpr int LongestFrameCount = 50;
	inline constexpr double LastWindowSec = 60.0;

	struct FSummary
	{
		FGroup Run;       // every sampled frame
		FGroup InGame;    // session frames (prologue .. ending)
		FGroup Last;      // the last LastWindowSec of the in-game frames
		FGroup Fights;    // every fight and boss frame together
		FGroup Field;     // camp and travel together
		std::vector<FGroup> Scenes;           // one per scene label, in order of first appearance
		std::vector<std::size_t> Longest;     // sample indices, longest first (ties: earlier first): any frame of the run
		std::vector<std::size_t> LongestPlay; // the same over the in-game play frames only (what the verdict judges)
	};

	/**
	 * `stat unit` values are read at the end of a frame and time the frame before it (the engine computes a frame's
	 * thread times when the next one starts): sample i holds frame i - 1's. Moves the four values back by one sample so
	 * they sit next to the frameMs they belong to; the last sample has none. Call once, before Summarize.
	 */
	inline void ShiftThreadTimesBack(std::vector<FFrameSample>& Samples)
	{
		for (std::size_t Index = 0; Index + 1 < Samples.size(); ++Index)
		{
			Samples[Index].GameMs = Samples[Index + 1].GameMs;
			Samples[Index].RenderMs = Samples[Index + 1].RenderMs;
			Samples[Index].RhiMs = Samples[Index + 1].RhiMs;
			Samples[Index].GpuMs = Samples[Index + 1].GpuMs;
		}
		if (!Samples.empty())
		{
			Samples.back().GameMs = Samples.back().RenderMs = Samples.back().RhiMs = Samples.back().GpuMs = Unavailable;
		}
	}

	inline FSummary Summarize(const std::vector<FFrameSample>& Samples, const std::vector<FScene>& Scenes, const FSimLimits& Limits,
		int LongestCount = LongestFrameCount)
	{
		FSummary Out;
		FGroupBuilder Run("run", "run");
		FGroupBuilder InGame("inGame", "inGame");
		FGroupBuilder Last("last60s", "window");
		FGroupBuilder Fights("fights", "fight");
		FGroupBuilder Field("field", "field");
		std::vector<FGroupBuilder> PerScene;
		PerScene.reserve(Scenes.size());
		for (const FScene& Scene : Scenes)
		{
			PerScene.emplace_back(Scene.Label, SceneKindName(Scene.Kind));
		}
		double LastInGameSec = -1.0;
		for (const FFrameSample& S : Samples)
		{
			if (S.Scene < Scenes.size() && IsInGameKind(Scenes[S.Scene].Kind))
			{
				LastInGameSec = S.RealSec;
			}
		}
		std::vector<std::size_t> PlayIndices;
		for (std::size_t SampleIndex = 0; SampleIndex < Samples.size(); ++SampleIndex)
		{
			const FFrameSample& S = Samples[SampleIndex];
			// The run's previous sample, whatever its scene (the first frame of a run has none: not a ratio frame).
			const bool bRatio = SampleIndex > 0 && RatioFrame(S.Tags, Samples[SampleIndex - 1].Tags);
			Run.Add(S, Limits, bRatio);
			if (S.Scene >= Scenes.size())
			{
				continue;
			}
			const ESceneKind Kind = Scenes[S.Scene].Kind;
			PerScene[S.Scene].Add(S, Limits, bRatio);
			if (IsInGameKind(Kind))
			{
				InGame.Add(S, Limits, bRatio);
				if (IsPlay(S.Tags))
				{
					PlayIndices.push_back(SampleIndex);
				}
				if (S.RealSec > LastInGameSec - LastWindowSec)
				{
					Last.Add(S, Limits, bRatio);
				}
			}
			if (Kind == ESceneKind::Fight || Kind == ESceneKind::Boss)
			{
				Fights.Add(S, Limits, bRatio);
			}
			if (Kind == ESceneKind::Camp || Kind == ESceneKind::Travel)
			{
				Field.Add(S, Limits, bRatio);
			}
		}
		Out.Run = Run.Finish();
		Out.InGame = InGame.Finish();
		Out.Last = Last.Finish();
		Out.Fights = Fights.Finish();
		Out.Field = Field.Finish();
		for (FGroupBuilder& Builder : PerScene)
		{
			if (!Builder.Empty())
			{
				Out.Scenes.push_back(Builder.Finish());
			}
		}
		Out.Longest.resize(Samples.size());
		for (std::size_t Index = 0; Index < Samples.size(); ++Index)
		{
			Out.Longest[Index] = Index;
		}
		const std::size_t Wanted = LongestCount < 0 ? 0 : static_cast<std::size_t>(LongestCount);
		const auto Longer = [&Samples](std::size_t A, std::size_t B)
		{
			return Samples[A].FrameMs > Samples[B].FrameMs || (Samples[A].FrameMs == Samples[B].FrameMs && A < B);
		};
		const auto KeepLongest = [&Longer, Wanted](std::vector<std::size_t>& List)
		{
			const std::size_t Keep = Wanted < List.size() ? Wanted : List.size();
			std::partial_sort(List.begin(), List.begin() + static_cast<std::ptrdiff_t>(Keep), List.end(), Longer);
			List.resize(Keep);
		};
		KeepLongest(Out.Longest);
		Out.LongestPlay = std::move(PlayIndices);
		KeepLongest(Out.LongestPlay);
		return Out;
	}

	// =================================================================================================================
	// The frame timeline (CSV next to the report)
	// =================================================================================================================

	struct FTimelinePlan
	{
		std::vector<std::size_t> Rows;   // sample indices, ascending
		std::size_t Stride = 1;          // every Stride-th ordinary frame is kept
		std::size_t SlowKept = 0;        // frames over 33 ms kept regardless of the stride
		std::size_t SlowDropped = 0;     // ... that did not fit (the shortest ones go first)
	};

	/**
	 * At most MaxRows rows: every frame while they fit; else every frame over 33 ms (the longest ones when even those
	 * are too many: up to half the rows) plus every Stride-th frame of the rest.
	 */
	inline FTimelinePlan PlanTimeline(const std::vector<FFrameSample>& Samples, std::size_t MaxRows)
	{
		FTimelinePlan Plan;
		if (MaxRows == 0)
		{
			return Plan;
		}
		if (Samples.size() <= MaxRows)
		{
			Plan.Rows.resize(Samples.size());
			for (std::size_t Index = 0; Index < Samples.size(); ++Index)
			{
				Plan.Rows[Index] = Index;
			}
			return Plan;
		}
		std::vector<std::size_t> Slow;
		for (std::size_t Index = 0; Index < Samples.size(); ++Index)
		{
			if (static_cast<double>(Samples[Index].FrameMs) > Over33Ms)
			{
				Slow.push_back(Index);
			}
		}
		const std::size_t SlowBudget = MaxRows / 2 > 0 ? MaxRows / 2 : 1;
		if (Slow.size() > SlowBudget)
		{
			std::sort(Slow.begin(), Slow.end(), [&Samples](std::size_t A, std::size_t B)
			{
				return Samples[A].FrameMs > Samples[B].FrameMs || (Samples[A].FrameMs == Samples[B].FrameMs && A < B);
			});
			Plan.SlowDropped = Slow.size() - SlowBudget;
			Slow.resize(SlowBudget);
			std::sort(Slow.begin(), Slow.end());
		}
		Plan.SlowKept = Slow.size();
		const std::size_t RegularBudget = MaxRows - Slow.size();
		const std::size_t Regular = Samples.size() - Slow.size() - Plan.SlowDropped;
		Plan.Stride = RegularBudget == 0 ? Samples.size() + 1 : (Regular + RegularBudget - 1) / RegularBudget;
		Plan.Stride = Plan.Stride < 1 ? 1 : Plan.Stride;
		std::size_t SlowCursor = 0;
		std::size_t RegularSeen = 0;
		std::size_t RegularKept = 0;
		for (std::size_t Index = 0; Index < Samples.size(); ++Index)
		{
			if (SlowCursor < Slow.size() && Slow[SlowCursor] == Index)
			{
				Plan.Rows.push_back(Index);
				++SlowCursor;
				continue;
			}
			if (static_cast<double>(Samples[Index].FrameMs) > Over33Ms)
			{
				continue;   // a slow frame that did not fit
			}
			if (RegularSeen++ % Plan.Stride == 0 && RegularKept < RegularBudget)
			{
				Plan.Rows.push_back(Index);
				++RegularKept;
			}
		}
		return Plan;
	}

	inline const char* TimelineHeader()
	{
		return "frame,realSec,frameMs,idleMs,gameMs,renderMs,rhiMs,gpuMs,simMs,driverMs,simDtMs,steps,stepCap,tags,scene,"
			"shaderJobs,assetBuilds,evLogLines,logRebuilds,spawns,spawnMs,zoneBuildMs,compileWaitMs,uiTextureLoads,"
			"vfxPoolCreations,syncLoads,loadMs,pickProjected,commands,husks,characters,registry,floatTexts\n";
	}

	namespace Detail
	{
		inline void AppendMs(std::string& Out, float Value)
		{
			if (Value >= 0.f)
			{
				char Buffer[32];
				std::snprintf(Buffer, sizeof(Buffer), "%.3f", static_cast<double>(Value));
				Out += Buffer;
			}
			Out += ',';
		}

		inline void AppendInt(std::string& Out, long long Value, bool bAvailable = true)
		{
			if (bAvailable)
			{
				Out += std::to_string(Value);
			}
			Out += ',';
		}
	}

	/** One CSV row (an unavailable value is an empty field). Labels never hold a comma or a quote. */
	inline void AppendTimelineRow(std::string& Out, std::size_t FrameIndex, const FFrameSample& S, std::string_view SceneLabel)
	{
		using namespace Detail;
		char Buffer[48];
		AppendInt(Out, static_cast<long long>(FrameIndex));
		std::snprintf(Buffer, sizeof(Buffer), "%.4f,", S.RealSec);
		Out += Buffer;
		AppendMs(Out, S.FrameMs);
		AppendMs(Out, S.IdleMs);
		AppendMs(Out, S.GameMs);
		AppendMs(Out, S.RenderMs);
		AppendMs(Out, S.RhiMs);
		AppendMs(Out, S.GpuMs);
		AppendMs(Out, S.SimMs);
		AppendMs(Out, S.DriverMs);
		AppendMs(Out, S.SimDtMs);
		AppendInt(Out, S.Steps);
		AppendInt(Out, (S.Tags & TagStepCap) != 0 ? 1 : 0);
		Out += TagList(S.Tags);
		Out += ',';
		Out += SceneLabel;
		Out += ',';
		AppendInt(Out, S.ShaderJobs, S.ShaderJobs >= 0);
		AppendInt(Out, S.AssetBuilds, S.AssetBuilds >= 0);
		AppendInt(Out, S.EvLogLines);
		AppendInt(Out, S.LogRebuilds);
		AppendInt(Out, S.Spawns);
		AppendMs(Out, S.SpawnMs);
		AppendMs(Out, S.ZoneBuildMs);
		AppendMs(Out, S.CompileWaitMs);
		AppendInt(Out, S.UiTextureLoads);
		AppendInt(Out, S.VfxPoolCreations);
		AppendInt(Out, S.SyncLoads);
		AppendMs(Out, S.LoadMs);
		AppendInt(Out, S.PickProjected);
		AppendInt(Out, S.Commands);
		AppendInt(Out, S.Husks, S.Husks >= 0);
		AppendInt(Out, S.Characters, S.Characters >= 0);
		AppendInt(Out, S.Registry, S.Registry >= 0);
		if (S.FloatTexts >= 0)
		{
			Out += std::to_string(S.FloatTexts);
		}
		Out += '\n';
	}

	// =================================================================================================================
	// Feel: command latency
	// =================================================================================================================

	enum class ECommandClass : std::uint8_t { Move, Attack, Skill, Dodge, Potion, Interact, Ui, Count };
	inline constexpr int CommandClassCount = static_cast<int>(ECommandClass::Count);

	inline const char* CommandClassName(ECommandClass Class)
	{
		static const char* const Names[CommandClassCount] = { "move", "attack", "skill", "dodge", "potion", "interact", "ui" };
		const int Index = static_cast<int>(Class);
		return Index >= 0 && Index < CommandClassCount ? Names[Index] : "";
	}

	inline ECommandClass ClassifyCommand(const abyss::Command& Command)
	{
		if (std::holds_alternative<abyss::CmdMoveTo>(Command) || std::holds_alternative<abyss::CmdSetMoveInput>(Command)
			|| std::holds_alternative<abyss::CmdStop>(Command) || std::holds_alternative<abyss::CmdPointerPress>(Command)
			|| std::holds_alternative<abyss::CmdPointerHold>(Command))
		{
			return ECommandClass::Move;
		}
		if (std::holds_alternative<abyss::CmdAttackTarget>(Command) || std::holds_alternative<abyss::CmdCycleTarget>(Command)
			|| std::holds_alternative<abyss::CmdClearTarget>(Command))
		{
			return ECommandClass::Attack;
		}
		if (std::holds_alternative<abyss::CmdCastSkill>(Command))
		{
			return ECommandClass::Skill;
		}
		if (std::holds_alternative<abyss::CmdDodge>(Command))
		{
			return ECommandClass::Dodge;
		}
		if (std::holds_alternative<abyss::CmdUsePotion>(Command))
		{
			return ECommandClass::Potion;
		}
		if (std::holds_alternative<abyss::CmdInteract>(Command) || std::holds_alternative<abyss::CmdPickUp>(Command)
			|| std::holds_alternative<abyss::CmdTownPortal>(Command))
		{
			return ECommandClass::Interact;
		}
		return ECommandClass::Ui;
	}

	/**
	 * Per player command: real ms from its submission to the first GameSim::Frame that applies it (the next one that
	 * runs a step, or any frame while the world is frozen: GameSim::Frame applies UI commands then), and to the end of
	 * that engine frame (the frame is handed to the renderer: what the player sees follows by the render, GPU and
	 * display latency, which the game cannot measure).
	 */
	class FCommandTracker
	{
	public:
		static constexpr std::size_t MaxKept = 20000;   // latencies kept per class (a chapter run submits a few thousand)

		void Submit(ECommandClass Class, double RealSec)
		{
			Pending.push_back(FPending{ Class, RealSec, -1.0 });
			++Submitted[static_cast<int>(Class)];
		}

		/** A GameSim::Frame returned at RealSec. bFrozenBefore: the world was frozen when it ran. */
		void OnSimFrame(double RealSec, int Steps, bool bFrozenBefore)
		{
			if (Steps <= 0 && !bFrozenBefore)
			{
				return;
			}
			for (FPending& Command : Pending)
			{
				if (Command.ActedSec < 0.0 && Command.SubmitSec <= RealSec)
				{
					Command.ActedSec = RealSec;
					Keep(ToSimMs[static_cast<int>(Command.Class)], (RealSec - Command.SubmitSec) * 1000.0);
				}
			}
		}

		/** The engine frame ended at RealSec. */
		void OnFrameEnd(double RealSec)
		{
			std::size_t Write = 0;
			for (std::size_t Read = 0; Read < Pending.size(); ++Read)
			{
				const FPending& Command = Pending[Read];
				if (Command.ActedSec >= 0.0 && RealSec >= Command.ActedSec)
				{
					Keep(ToFrameMs[static_cast<int>(Command.Class)], (RealSec - Command.SubmitSec) * 1000.0);
					continue;
				}
				Pending[Write++] = Command;
			}
			Pending.resize(Write);
		}

		std::size_t PendingCount() const { return Pending.size(); }
		/** Every class together. */
		FValueStats AllSimStats() const { return Merged(ToSimMs); }
		FValueStats AllFrameStats() const { return Merged(ToFrameMs); }
		double AllMedianSimMs() const { return MergedMedian(ToSimMs); }
		double AllMedianFrameMs() const { return MergedMedian(ToFrameMs); }
		int SubmittedCount(ECommandClass Class) const { return Submitted[static_cast<int>(Class)]; }
		/** Sorted copies' statistics; Count 0 when the class was never measured. */
		FValueStats SimStats(ECommandClass Class) const
		{
			std::vector<float> Copy = ToSimMs[static_cast<int>(Class)];
			return ComputeValueStats(Copy);
		}
		FValueStats FrameStats(ECommandClass Class) const
		{
			std::vector<float> Copy = ToFrameMs[static_cast<int>(Class)];
			return ComputeValueStats(Copy);
		}
		double MedianSimMs(ECommandClass Class) const
		{
			std::vector<float> Copy = ToSimMs[static_cast<int>(Class)];
			std::sort(Copy.begin(), Copy.end());
			return Percentile(Copy, 0.5);
		}
		double MedianFrameMs(ECommandClass Class) const
		{
			std::vector<float> Copy = ToFrameMs[static_cast<int>(Class)];
			std::sort(Copy.begin(), Copy.end());
			return Percentile(Copy, 0.5);
		}

	private:
		struct FPending
		{
			ECommandClass Class;
			double SubmitSec;
			double ActedSec;
		};

		static std::vector<float> MergedList(const std::vector<float>* Lists)
		{
			std::vector<float> All;
			for (int Index = 0; Index < CommandClassCount; ++Index)
			{
				All.insert(All.end(), Lists[Index].begin(), Lists[Index].end());
			}
			std::sort(All.begin(), All.end());
			return All;
		}
		static FValueStats Merged(const std::vector<float>* Lists)
		{
			std::vector<float> All = MergedList(Lists);
			return ComputeValueStats(All);
		}
		static double MergedMedian(const std::vector<float>* Lists)
		{
			return Percentile(MergedList(Lists), 0.5);
		}

		static void Keep(std::vector<float>& List, double Milliseconds)
		{
			if (List.size() < MaxKept)
			{
				List.push_back(static_cast<float>(Milliseconds));
			}
		}

		std::vector<FPending> Pending;
		std::vector<float> ToSimMs[CommandClassCount];
		std::vector<float> ToFrameMs[CommandClassCount];
		int Submitted[CommandClassCount] = {};
	};

	// =================================================================================================================
	// Feel: the hero attack's contact beat against the frame that presents the hit
	// =================================================================================================================

	/**
	 * A hero basic swing's clip ("Attack01"; the core's skill swings carry the skill's id as their clip). An empty clip
	 * is a basic swing too.
	 */
	inline bool IsBasicSwingClip(std::string_view Clip)
	{
		return Clip.empty() || Clip.substr(0, 6) == "Attack";
	}

	/**
	 * The core resolves a hero swing at EvPlayAnim::startMs + contactMs (sim clock) and announces it with EvHit in the
	 * frame whose steps reach that time. That frame draws the pose of FAbyssFrameInfo::RenderSimMs. The error is
	 * RenderSimMs - contact time: 0 = the hit shows exactly on the contact pose, positive = the pose is already past the
	 * contact when the hit shows (a slow frame ran several steps), negative = the hit shows before the interpolated pose
	 * reaches the contact (at 60 fps the pose trails the sim by up to one step, so about -17 .. 0 ms is the floor).
	 *
	 * A hit is measured only against a pending contact of its own kind (a basic hit, EvHit::skillId empty, against a
	 * basic swing; a skill's hit against that skill's swing) that is due (one step of slack) and no older than the
	 * match window: the frame that reaches a contact presents its hit, so the hit trails the contact by less than the
	 * frame's step budget (window = budget + 1 steps). Everything else counts as unmatched: a projectile's hit lands on
	 * arrival, long after the release beat; a swing's second and later targets have no contact of their own; a swing
	 * whose own hit never came (the target died, the swing was cancelled) is not handed to a later hit.
	 */
	class FContactTracker
	{
	public:
		static constexpr std::size_t MaxKept = 20000;

		explicit FContactTracker(int MaxStepsPerFrame = 4)
			: WindowMs(((MaxStepsPerFrame > 0 ? MaxStepsPerFrame : 1) + 1) * abyss::kSimStepMs)
		{
		}

		/** How long after its contact time a hit can still be the swing's own. */
		double MatchWindowMs() const { return WindowMs; }

		/** A hero attack animation started. Clip: EvPlayAnim::clip. */
		void OnHeroAttackAnim(double StartMs, double ContactMs, std::string_view Clip = std::string_view())
		{
			if (ContactMs > 0.0)
			{
				Pending.push_back(FPending{ StartMs + ContactMs, IsBasicSwingClip(Clip) ? std::string() : std::string(Clip) });
			}
		}

		/**
		 * A hero hit (not a DoT tick) was dispatched. SkillId: EvHit::skillId ("" for a basic attack). Returns true
		 * and the error when a pending contact of the same kind matches.
		 */
		bool OnHeroHit(double SimNowMs, double RenderSimMs, double& OutErrorMs, std::string_view SkillId = std::string_view())
		{
			// Contacts whose time passed a while ago never landed (the swing was cancelled, or it released a projectile).
			std::size_t Write = 0;
			for (std::size_t Read = 0; Read < Pending.size(); ++Read)
			{
				if (SimNowMs - Pending[Read].ContactAtMs <= WindowMs)
				{
					if (Write != Read)
					{
						Pending[Write] = std::move(Pending[Read]);
					}
					++Write;
				}
			}
			Pending.resize(Write);
			// The earliest contact of this kind that is due by now (a hit cannot precede its contact; one step of slack).
			for (std::size_t Index = 0; Index < Pending.size(); ++Index)
			{
				if (Pending[Index].ContactAtMs <= SimNowMs + abyss::kSimStepMs && Pending[Index].SkillId == SkillId)
				{
					OutErrorMs = RenderSimMs - Pending[Index].ContactAtMs;
					Pending.erase(Pending.begin() + static_cast<std::ptrdiff_t>(Index));
					if (ErrorsMs.size() < MaxKept)
					{
						ErrorsMs.push_back(static_cast<float>(OutErrorMs));
					}
					++Matched;
					return true;
				}
			}
			++Unmatched;
			return false;
		}

		struct FResult
		{
			int Count = 0;
			int Unmatched = 0;
			double MeanMs = 0.0;
			double P50Ms = 0.0;        // signed
			double P95AbsMs = 0.0;
			double MaxAbsMs = 0.0;
			int LateOver33 = 0;        // the pose was more than 33 ms past the contact
		};

		FResult Result() const
		{
			FResult Out;
			Out.Count = Matched;
			Out.Unmatched = Unmatched;
			if (ErrorsMs.empty())
			{
				return Out;
			}
			std::vector<float> Signed = ErrorsMs;
			std::vector<float> Abs;
			Abs.reserve(ErrorsMs.size());
			double Sum = 0.0;
			for (const float Value : ErrorsMs)
			{
				Sum += static_cast<double>(Value);
				Abs.push_back(Value < 0.f ? -Value : Value);
				Out.LateOver33 += static_cast<double>(Value) > Over33Ms ? 1 : 0;
			}
			std::sort(Signed.begin(), Signed.end());
			std::sort(Abs.begin(), Abs.end());
			Out.MeanMs = Sum / static_cast<double>(ErrorsMs.size());
			Out.P50Ms = Percentile(Signed, 0.5);
			Out.P95AbsMs = Percentile(Abs, 0.95);
			Out.MaxAbsMs = static_cast<double>(Abs.back());
			return Out;
		}

	private:
		struct FPending
		{
			double ContactAtMs;
			std::string SkillId;   // "" for a basic swing
		};

		double WindowMs;
		std::vector<FPending> Pending;
		std::vector<float> ErrorsMs;
		int Matched = 0;
		int Unmatched = 0;
	};

	// =================================================================================================================
	// The verdict
	// =================================================================================================================

	/**
	 * Plain limits on the in-game play frames (60 fps target, t.MaxFPS 60). Tunable with
	 * -abysssmooth=p50:20,p95:33.4,p99:50,hitch:1,max:250,ratio:0.97,frames:600 (any subset).
	 */
	struct FThresholds
	{
		double P50Ms = 20.0;            // the median frame holds 60 fps with a little slack (50 fps)
		double P95Ms = 33.4;            // 19 frames of 20 hold 30 fps
		double P99Ms = 50.0;            // 99 of 100 hold 20 fps
		double HitchesPerMin = 1.0;     // frames over 100 ms per minute of play
		double MaxMs = 250.0;           // the longest play frame (SimConfig::maxFrameMs: beyond it sim time is lost)
		double MinGameRealRatio = 0.97; // sim time against real time (below: the step cap makes the game slow motion)
		int MinFrames = 600;            // fewer play frames than this: "notMeasured"
	};

	inline constexpr double MaxMinFrames = 10000000.0;

	/** Parses "p95:33.4,p99=50" into Out (unnamed fields keep their value). False and Error on a bad entry. */
	inline bool ParseThresholds(std::string_view Text, FThresholds& Out, std::string& Error)
	{
		std::size_t Pos = 0;
		while (Pos < Text.size())
		{
			std::size_t End = Text.find(',', Pos);
			End = End == std::string_view::npos ? Text.size() : End;
			std::string_view Entry = Text.substr(Pos, End - Pos);
			Pos = End + 1;
			while (!Entry.empty() && Entry.front() == ' ')
			{
				Entry.remove_prefix(1);
			}
			while (!Entry.empty() && Entry.back() == ' ')
			{
				Entry.remove_suffix(1);
			}
			if (Entry.empty())
			{
				continue;
			}
			const std::size_t Sep = Entry.find_first_of(":=");
			if (Sep == std::string_view::npos || Sep == 0 || Sep + 1 >= Entry.size())
			{
				Error = "'" + std::string(Entry) + "' is not name:value";
				return false;
			}
			const std::string Name(Entry.substr(0, Sep));
			const std::string ValueText(Entry.substr(Sep + 1));
			char* ParseEnd = nullptr;
			const double Value = std::strtod(ValueText.c_str(), &ParseEnd);
			if (ParseEnd == ValueText.c_str() || *ParseEnd != '\0' || !(Value >= 0.0) || !std::isfinite(Value))
			{
				Error = "'" + ValueText + "' is not a number >= 0 (" + Name + ")";
				return false;
			}
			if (Name == "p50") { Out.P50Ms = Value; }
			else if (Name == "p95") { Out.P95Ms = Value; }
			else if (Name == "p99") { Out.P99Ms = Value; }
			else if (Name == "hitch") { Out.HitchesPerMin = Value; }
			else if (Name == "max") { Out.MaxMs = Value; }
			else if (Name == "ratio") { Out.MinGameRealRatio = Value; }
			else if (Name == "frames")
			{
				// A verdict needs frames: 0 would grade a run that measured nothing. Clamped before the cast.
				if (Value < 1.0)
				{
					Error = "frames must be at least 1";
					return false;
				}
				Out.MinFrames = static_cast<int>(Value > MaxMinFrames ? MaxMinFrames : Value);
			}
			else
			{
				Error = "unknown threshold '" + Name + "' (p50, p95, p99, hitch, max, ratio, frames)";
				return false;
			}
		}
		return true;
	}

	struct FCheck
	{
		const char* Name = "";
		double Value = 0.0;
		double Limit = 0.0;
		bool bAtMost = true;      // Value <= Limit passes (else Value >= Limit)
		bool bMeasured = true;
		bool bOk = true;
		bool bTail = false;       // a tail check: failing only these is "uneven", not "rough"
	};

	struct FVerdict
	{
		std::string Grade;        // "smooth", "uneven", "rough", "notMeasured"
		std::vector<FCheck> Checks;
		int PlayFrames = 0;
		double PlaySec = 0.0;

		bool IsSmooth() const { return Grade == "smooth"; }
	};

	/**
	 * Judges the in-game play frames. "smooth": every check passes. "uneven": the median, p95 and the game / real ratio
	 * pass but a tail check fails (p99, hitches per minute, the longest frame). "rough": the median, p95 or the ratio
	 * fails. "notMeasured": too few play frames to say (always with none at all, whatever FThresholds::MinFrames is). The
	 * ratio is the group's play ratio (FGroup::RatioSimMs: frames that swallow a tagged frame's hitch are not in it).
	 */
	inline FVerdict Judge(const FGroup& InGame, const FThresholds& T)
	{
		FVerdict Out;
		const FStats& Play = InGame.Play;
		Out.PlayFrames = Play.Count;
		Out.PlaySec = Play.TotalSec;
		const auto AtMost = [&Out](const char* Name, double Value, double Limit, bool bTail)
		{
			FCheck Check;
			Check.Name = Name;
			Check.Value = Value;
			Check.Limit = Limit;
			Check.bTail = bTail;
			Check.bOk = Value <= Limit;
			Out.Checks.push_back(Check);
		};
		AtMost("p50Ms", Play.P50Ms, T.P50Ms, false);
		AtMost("p95Ms", Play.P95Ms, T.P95Ms, false);
		AtMost("p99Ms", Play.P99Ms, T.P99Ms, true);
		AtMost("hitchesPerMin", Play.HitchesPerMinute(), T.HitchesPerMin, true);
		AtMost("maxMs", Play.MaxMs, T.MaxMs, true);
		FCheck Ratio;
		Ratio.Name = "gameRealRatio";
		Ratio.Limit = T.MinGameRealRatio;
		Ratio.bAtMost = false;
		Ratio.bMeasured = InGame.HasRatio();
		Ratio.Value = InGame.GameRealRatio();
		Ratio.bOk = !Ratio.bMeasured || Ratio.Value >= Ratio.Limit;
		Out.Checks.push_back(Ratio);
		if (Play.Count <= 0 || Play.Count < T.MinFrames)
		{
			Out.Grade = "notMeasured";
			return Out;
		}
		bool bCoreFailed = false;
		bool bTailFailed = false;
		for (const FCheck& Check : Out.Checks)
		{
			bCoreFailed = bCoreFailed || (!Check.bOk && !Check.bTail);
			bTailFailed = bTailFailed || (!Check.bOk && Check.bTail);
		}
		Out.Grade = bCoreFailed ? "rough" : (bTailFailed ? "uneven" : "smooth");
		return Out;
	}

	/** -abysssmoothgate: the reason the run fails, or "" (a smooth run, or no gate). */
	inline std::string GateFailure(const FVerdict& Verdict, bool bGate)
	{
		if (!bGate || Verdict.IsSmooth())
		{
			return std::string();
		}
		std::string Out = "smoothness gate: " + Verdict.Grade;
		if (Verdict.Grade == "notMeasured")
		{
			return Out + " (" + std::to_string(Verdict.PlayFrames) + " play frames)";
		}
		char Buffer[96];
		bool bFirst = true;
		for (const FCheck& Check : Verdict.Checks)
		{
			if (!Check.bOk)
			{
				std::snprintf(Buffer, sizeof(Buffer), "%s%s %.2f (limit %.2f)", bFirst ? ": " : ", ", Check.Name, Check.Value, Check.Limit);
				Out += Buffer;
				bFirst = false;
			}
		}
		return Out;
	}

	// =================================================================================================================
	// report.json
	// =================================================================================================================

	namespace Detail
	{
		inline double Round(double Value, double Scale)
		{
			return std::round(Value * Scale) / Scale;
		}

		inline void Ms(abyss::JsonWriter& W, const char* Key, double Value)
		{
			W.Key(Key);
			W.Double(Round(Value, 100.0));
		}

		inline void MsOrNull(abyss::JsonWriter& W, const char* Key, double Value, bool bAvailable)
		{
			W.Key(Key);
			if (bAvailable)
			{
				W.Double(Round(Value, 100.0));
			}
			else
			{
				W.Null();
			}
		}

		inline void IntOrNull(abyss::JsonWriter& W, const char* Key, long long Value, bool bAvailable)
		{
			W.Key(Key);
			if (bAvailable)
			{
				W.Int(Value);
			}
			else
			{
				W.Null();
			}
		}

		inline void WriteStats(abyss::JsonWriter& W, const char* Key, const FStats& S)
		{
			W.Key(Key);
			W.StartObject();
			W.Key("count");
			W.Int(S.Count);
			W.Key("totalSec");
			W.Double(Round(S.TotalSec, 1000.0));
			const bool bAny = S.Count > 0;
			MsOrNull(W, "avgMs", S.AvgMs, bAny);
			MsOrNull(W, "p50Ms", S.P50Ms, bAny);
			MsOrNull(W, "p95Ms", S.P95Ms, bAny);
			MsOrNull(W, "p99Ms", S.P99Ms, bAny);
			MsOrNull(W, "maxMs", S.MaxMs, bAny);
			W.Key("over33Ms");
			W.Int(S.Over33);
			W.Key("over50Ms");
			W.Int(S.Over50);
			W.Key("over100Ms");
			W.Int(S.Over100);
			W.EndObject();
		}

		inline void WriteValueStats(abyss::JsonWriter& W, const char* Key, const FValueStats& S)
		{
			W.Key(Key);
			if (S.Count == 0)
			{
				W.Null();
				return;
			}
			W.StartObject();
			W.Key("frames");
			W.Int(S.Count);
			Ms(W, "avgMs", S.AvgMs);
			Ms(W, "p95Ms", S.P95Ms);
			Ms(W, "maxMs", S.MaxMs);
			W.EndObject();
		}

		inline void WritePeakAvg(abyss::JsonWriter& W, const char* Key, const FPeakAvg& P)
		{
			W.Key(Key);
			if (P.Count == 0)
			{
				W.Null();
				return;
			}
			W.StartObject();
			W.Key("peak");
			W.Int(P.Peak);
			W.Key("avg");
			W.Double(Round(P.Avg(), 100.0));
			W.EndObject();
		}
	}

	/** One group object: the same keys for the run, a scene, the last 60 s. */
	inline void WriteGroup(abyss::JsonWriter& W, const FGroup& G)
	{
		using namespace Detail;
		W.StartObject();
		W.Key("label");
		W.String(G.Label);
		W.Key("kind");
		W.String(G.Kind);
		W.Key("firstRealSec");
		W.Double(Round(G.FirstRealSec, 1000.0));
		W.Key("lastRealSec");
		W.Double(Round(G.LastRealSec, 1000.0));
		WriteStats(W, "all", G.All);
		WriteStats(W, "play", G.Play);
		WriteStats(W, "playNoBacklog", G.PlayNoBacklog);
		W.Key("tagFrames");
		W.StartObject();
		for (int Bit = 0; Bit < TagCount; ++Bit)
		{
			W.Key(TagName(Bit));
			W.Int(G.TagFrames[Bit]);
		}
		W.EndObject();
		W.Key("gameRealRatio");
		if (G.HasRatio())
		{
			W.Double(Round(G.GameRealRatio(), 10000.0));
		}
		else
		{
			W.Null();
		}
		W.Key("simFedSec");
		W.Double(Round(G.RatioRealMs / 1000.0, 1000.0));
		W.Key("stepCapFrames");
		W.Int(G.StepCapFrames);
		W.Key("lostSimSec");
		W.Double(Round(G.LostSimMs / 1000.0, 1000.0));
		W.Key("lostToClampSec");
		W.Double(Round(G.ClampLostMs / 1000.0, 1000.0));
		// every unfrozen, undilated session frame, tagged or not (informational; the four keys above are play only)
		W.Key("simAllFrames");
		W.StartObject();
		W.Key("gameRealRatio");
		if (G.HasAllRatio())
		{
			W.Double(Round(G.AllGameRealRatio(), 10000.0));
		}
		else
		{
			W.Null();
		}
		W.Key("simFedSec");
		W.Double(Round(G.AllRatioRealMs / 1000.0, 1000.0));
		W.Key("stepCapFrames");
		W.Int(G.AllStepCapFrames);
		W.Key("lostSimSec");
		W.Double(Round(G.AllLostSimMs / 1000.0, 1000.0));
		W.EndObject();
		W.Key("threads");
		W.StartObject();
		WriteValueStats(W, "idle", G.Idle);
		WriteValueStats(W, "game", G.Game);
		WriteValueStats(W, "render", G.Render);
		WriteValueStats(W, "rhi", G.Rhi);
		WriteValueStats(W, "gpu", G.Gpu);
		WriteValueStats(W, "sim", G.Sim);
		WriteValueStats(W, "driver", G.Driver);
		W.EndObject();
		W.Key("load");
		W.StartObject();
		WritePeakAvg(W, "husks", G.Husks);
		WritePeakAvg(W, "characters", G.Characters);
		WritePeakAvg(W, "registry", G.Registry);
		WritePeakAvg(W, "floatTexts", G.FloatTexts);
		WritePeakAvg(W, "evLogLines", G.EvLogLines);
		WritePeakAvg(W, "pickProjected", G.PickProjected);
		W.EndObject();
		W.EndObject();
	}

	/** One entry of `longestFrames`. */
	inline void WriteLongestFrame(abyss::JsonWriter& W, std::size_t FrameIndex, const FFrameSample& S, const std::vector<FScene>& Scenes,
		const std::vector<FMilestone>& Milestones)
	{
		using namespace Detail;
		W.StartObject();
		W.Key("frame");
		W.Int(static_cast<long long>(FrameIndex));
		W.Key("realSec");
		W.Double(Round(S.RealSec, 1000.0));
		Ms(W, "frameMs", static_cast<double>(S.FrameMs));
		W.Key("tags");
		W.StartArray();
		for (int Bit = 0; Bit < TagCount; ++Bit)
		{
			if ((S.Tags & (1u << Bit)) != 0)
			{
				W.String(TagName(Bit));
			}
		}
		W.EndArray();
		W.Key("play");
		W.Bool(IsPlay(S.Tags));
		W.Key("scene");
		W.String(S.Scene < Scenes.size() ? std::string_view(Scenes[S.Scene].Label) : std::string_view());
		W.Key("steps");
		W.Int(S.Steps);
		W.Key("stepCap");
		W.Bool((S.Tags & TagStepCap) != 0);
		MsOrNull(W, "idleMs", static_cast<double>(S.IdleMs), S.IdleMs >= 0.f);
		MsOrNull(W, "gameMs", static_cast<double>(S.GameMs), S.GameMs >= 0.f);
		MsOrNull(W, "renderMs", static_cast<double>(S.RenderMs), S.RenderMs >= 0.f);
		MsOrNull(W, "rhiMs", static_cast<double>(S.RhiMs), S.RhiMs >= 0.f);
		MsOrNull(W, "gpuMs", static_cast<double>(S.GpuMs), S.GpuMs >= 0.f);
		MsOrNull(W, "simMs", static_cast<double>(S.SimMs), S.SimMs >= 0.f);
		MsOrNull(W, "driverMs", static_cast<double>(S.DriverMs), S.DriverMs >= 0.f);
		IntOrNull(W, "shaderJobs", S.ShaderJobs, S.ShaderJobs >= 0);
		IntOrNull(W, "assetBuilds", S.AssetBuilds, S.AssetBuilds >= 0);
		W.Key("nearestMilestone");
		const int Nearest = NearestMilestone(Milestones, S.RealSec);
		if (Nearest < 0)
		{
			W.Null();
		}
		else
		{
			const FMilestone& M = Milestones[static_cast<std::size_t>(Nearest)];
			W.StartObject();
			W.Key("kind");
			W.String(M.Kind);
			W.Key("detail");
			W.String(M.Detail);
			W.Key("deltaSec");   // the frame's end minus the milestone: positive = the frame came after it
			W.Double(Round(S.RealSec - M.RealSec, 1000.0));
			W.EndObject();
		}
		// Only what happened: an empty object is a frame in which none of the module's own counters moved.
		W.Key("events");
		W.StartObject();
		const auto Count = [&W](const char* Key, int Value)
		{
			if (Value > 0)
			{
				W.Key(Key);
				W.Int(Value);
			}
		};
		const auto Millis = [&W](const char* Key, float Value)
		{
			if (Value > 0.f)
			{
				Ms(W, Key, static_cast<double>(Value));
			}
		};
		Count("evLogLines", S.EvLogLines);
		Count("logRebuilds", S.LogRebuilds);
		Count("spawns", S.Spawns);
		Millis("spawnMs", S.SpawnMs);
		Millis("zoneBuildMs", S.ZoneBuildMs);
		Millis("compileWaitMs", S.CompileWaitMs);
		Count("uiTextureLoads", S.UiTextureLoads);
		Count("vfxPoolCreations", S.VfxPoolCreations);
		Count("syncLoads", S.SyncLoads);
		Millis("loadMs", S.LoadMs);
		Count("pickProjected", S.PickProjected);
		Count("commands", S.Commands);
		W.EndObject();
		W.Key("load");
		W.StartObject();
		IntOrNull(W, "husks", S.Husks, S.Husks >= 0);
		IntOrNull(W, "characters", S.Characters, S.Characters >= 0);
		IntOrNull(W, "registry", S.Registry, S.Registry >= 0);
		IntOrNull(W, "floatTexts", S.FloatTexts, S.FloatTexts >= 0);
		W.EndObject();
		W.EndObject();
	}

	/** `"smoothness": { ... }` (key included). */
	inline void WriteSmoothness(abyss::JsonWriter& W, const FVerdict& Verdict, const FThresholds& T, bool bGate, bool bShotsOff)
	{
		using namespace Detail;
		W.Key("smoothness");
		W.StartObject();
		W.Key("verdict");
		W.String(Verdict.Grade);
		W.Key("gate");
		W.Bool(bGate);
		W.Key("gateFailed");
		W.Bool(bGate && !Verdict.IsSmooth());
		W.Key("basis");
		W.String("in-game frames without a shot / zoneBuild / compileWait / story / panel / loading tag (perf.inGame.play); "
			"gameRealRatio over those of them that follow such a frame (perf.inGame.gameRealRatio)");
		W.Key("playFrames");
		W.Int(Verdict.PlayFrames);
		W.Key("playSec");
		W.Double(Round(Verdict.PlaySec, 1000.0));
		W.Key("minFrames");
		W.Int(T.MinFrames);
		W.Key("checks");
		W.StartArray();
		for (const FCheck& Check : Verdict.Checks)
		{
			W.StartObject();
			W.Key("name");
			W.String(Check.Name);
			W.Key("value");
			if (Check.bMeasured && Verdict.PlayFrames > 0)
			{
				W.Double(Round(Check.Value, 10000.0));
			}
			else
			{
				W.Null();
			}
			W.Key("limit");
			W.Double(Check.Limit);
			W.Key("rule");
			W.String(Check.bAtMost ? "atMost" : "atLeast");
			W.Key("tail");
			W.Bool(Check.bTail);
			W.Key("ok");
			W.Bool(Check.bOk);
			W.EndObject();
		}
		W.EndArray();
		W.Key("notes");
		W.StartArray();
		if (!bShotsOff)
		{
			W.String("screenshots were on: frames around them are tagged, but run a measurement with -abyssnoshots");
		}
		if (Verdict.Grade == "notMeasured")
		{
			W.String("too few play frames for a verdict");
		}
		W.EndArray();
		W.EndObject();
	}
}
