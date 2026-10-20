// AbyssCueTriggerRules: when the remake's bells and beat-bound voices play (U7; manifest `cueTriggers`, from AU2
// dark_ambience.BELL_TRIGGERS and AU1 dark_vox.VOX_TRIGGERS).
//
// Rows (`on`): storyEnd {beat}, storyStep {beat, index}, storySlide {sequence, index} (indices are the core's 0-based
// EvStoryStep::index), randomEvent {event, zone} (EvRandomEvent not resolved), decorFocus {decor}
// (EvStoryDecorFocus), every {everySec [lo, hi], zone, whileQuestActive?, within?} (a random interval in [lo, hi]
// while the condition holds; the first one an interval after it starts holding; never while a story beat plays).
// Options: farCue + nearRegion (outside that musicDirector region the far cue plays), tile (a 3d cue is emitted there,
// else at the hero), count + spacingSec (a burst; one started by a story beat stops when that beat ends).
//
// Pure helper (std + AbyssCore public headers only); tested in PresentationTests/tests/audio_triggers_test.cpp.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Json.h"

#include "Audio/AbyssAudioDataRules.h"

namespace AbyssCueTriggerRules
{
	enum class On : unsigned char
	{
		Unknown,
		StoryEnd,
		StoryStep,
		StorySlide,
		RandomEvent,
		DecorFocus,
		Every,
	};

	inline On ParseOn(std::string_view Name)
	{
		if (Name == "storyEnd") return On::StoryEnd;
		if (Name == "storyStep") return On::StoryStep;
		if (Name == "storySlide") return On::StorySlide;
		if (Name == "randomEvent") return On::RandomEvent;
		if (Name == "decorFocus") return On::DecorFocus;
		if (Name == "every") return On::Every;
		return On::Unknown;
	}

	struct Trigger
	{
		std::string id;
		std::string cue;
		std::string farCue;      // "" = none
		std::string nearRegion;  // musicDirector region id
		On on = On::Unknown;
		std::string beat;        // storyEnd / storyStep
		std::string sequence;    // storySlide
		int index = -1;          // storyStep / storySlide
		std::string event;       // randomEvent ("ambush", ...: RandomEventType names)
		std::string zone;        // randomEvent / every ("" = any zone)
		std::string decor;       // decorFocus
		std::string whileQuestActive;
		bool hasWithin = false;
		AbyssAudioData::TileCircle within;
		double everyLoSec = 0.0;
		double everyHiSec = 0.0;
		bool hasTile = false;
		double tileCol = 0.0;
		double tileRow = 0.0;
		int count = 1;
		double spacingSec = 0.0;
	};

	struct TriggerTables
	{
		std::vector<Trigger> rows;
		bool loaded = false;
	};

	inline TriggerTables Parse(const abyss::JsonValue& CueTriggers)
	{
		using namespace AbyssAudioData;
		TriggerTables Out;
		if (!CueTriggers.IsObject())
		{
			return Out;
		}
		for (const abyss::JsonValue& Row : CueTriggers.Get("rows").Items())
		{
			Trigger T;
			T.id = Str(Row.Get("id"));
			T.cue = Str(Row.Get("cue"));
			T.farCue = Str(Row.Get("farCue"));
			T.nearRegion = Str(Row.Get("nearRegion"));
			T.on = ParseOn(Row.Get("on").AsString());
			T.beat = Str(Row.Get("beat"));
			T.sequence = Str(Row.Get("sequence"));
			T.index = Row.Get("index").AsInt(-1);
			T.event = Str(Row.Get("event"));
			T.zone = Str(Row.Get("zone"));
			T.decor = Str(Row.Get("decor"));
			T.whileQuestActive = Str(Row.Get("whileQuestActive"));
			if (Row.Get("within").IsObject())
			{
				T.hasWithin = true;
				T.within = ReadCircle(Row.Get("within"));
			}
			const abyss::JsonValue& Every = Row.Get("everySec");
			if (Every.IsArray() && Every.Size() == 2)
			{
				T.everyLoSec = Num(Every.At(0), 0.0);
				T.everyHiSec = Num(Every.At(1), T.everyLoSec);
				if (T.everyHiSec < T.everyLoSec)
				{
					T.everyHiSec = T.everyLoSec;
				}
			}
			const abyss::JsonValue& Tile = Row.Get("tile");
			if (Tile.IsArray() && Tile.Size() == 2)
			{
				T.hasTile = true;
				T.tileCol = Num(Tile.At(0), 0.0);
				T.tileRow = Num(Tile.At(1), 0.0);
			}
			T.count = Row.Get("count").AsInt(1);
			if (T.count < 1)
			{
				T.count = 1;
			}
			T.spacingSec = Num(Row.Get("spacingSec"), 0.0);
			// An `every` row without a positive interval would fire every frame: it is dropped.
			const bool bValidEvery = T.on != On::Every || T.everyLoSec > 0.0;
			if (!T.cue.empty() && T.on != On::Unknown && bValidEvery)
			{
				Out.rows.push_back(std::move(T));
			}
		}
		Out.loaded = true;
		return Out;
	}

	// ---- matching (indices into rows) ------------------------------------------------------------------------------

	inline std::vector<std::size_t> MatchStoryEnd(const TriggerTables& Tables, std::string_view Beat)
	{
		std::vector<std::size_t> Out;
		for (std::size_t I = 0; I < Tables.rows.size(); ++I)
		{
			if (Tables.rows[I].on == On::StoryEnd && Tables.rows[I].beat == Beat)
			{
				Out.push_back(I);
			}
		}
		return Out;
	}

	/** EvStoryStep: a cutscene step (beat, index) or a sequence slide (sequence id, index). */
	inline std::vector<std::size_t> MatchStoryStep(const TriggerTables& Tables, std::string_view Beat,
		std::string_view SequenceId, int Index, bool bIsSlide)
	{
		std::vector<std::size_t> Out;
		for (std::size_t I = 0; I < Tables.rows.size(); ++I)
		{
			const Trigger& T = Tables.rows[I];
			const bool bMatch = bIsSlide ? (T.on == On::StorySlide && T.sequence == SequenceId && T.index == Index)
				: (T.on == On::StoryStep && T.beat == Beat && T.index == Index);
			if (bMatch)
			{
				Out.push_back(I);
			}
		}
		return Out;
	}

	/** EvRandomEvent: only the start (not resolved), in the row's zone ("" = any). */
	inline std::vector<std::size_t> MatchRandomEvent(const TriggerTables& Tables, std::string_view EventType,
		bool bResolved, std::string_view ZoneId)
	{
		std::vector<std::size_t> Out;
		if (bResolved)
		{
			return Out;
		}
		for (std::size_t I = 0; I < Tables.rows.size(); ++I)
		{
			const Trigger& T = Tables.rows[I];
			if (T.on == On::RandomEvent && T.event == EventType && (T.zone.empty() || T.zone == ZoneId))
			{
				Out.push_back(I);
			}
		}
		return Out;
	}

	/** EvStoryDecorFocus: the focus moved onto that decoration ("" = none: nothing). */
	inline std::vector<std::size_t> MatchDecorFocus(const TriggerTables& Tables, std::string_view DecorId)
	{
		std::vector<std::size_t> Out;
		if (DecorId.empty())
		{
			return Out;
		}
		for (std::size_t I = 0; I < Tables.rows.size(); ++I)
		{
			if (Tables.rows[I].on == On::DecorFocus && Tables.rows[I].decor == DecorId)
			{
				Out.push_back(I);
			}
		}
		return Out;
	}

	/** The cue a row plays for the hero's position: farCue when the hero is outside its near region. */
	inline const std::string& CueFor(const Trigger& T, bool bInsideNearRegion)
	{
		return (!T.farCue.empty() && !T.nearRegion.empty() && !bInsideNearRegion) ? T.farCue : T.cue;
	}

	/** The music-director region id a row's near / far choice reads ("" = none). */
	inline bool NeedsRegion(const Trigger& T)
	{
		return !T.farCue.empty() && !T.nearRegion.empty();
	}

	/**
	 * Bursts and `every` timers (real seconds). Start() a matched row (the first member is due now), StopBound() when a
	 * story beat ends, Tick() every frame: it returns the rows due now (one entry per member).
	 */
	class Scheduler
	{
	public:
		void Reset()
		{
			Pending.clear();
			NextEverySec.clear();
		}

		/** A matched row fires at NowSec; a burst's later members follow every spacingSec, bound to BoundBeat. */
		void Start(const TriggerTables& Tables, std::size_t Row, double NowSec, std::string_view BoundBeat)
		{
			if (Row >= Tables.rows.size())
			{
				return;
			}
			const Trigger& T = Tables.rows[Row];
			for (int K = 0; K < T.count; ++K)
			{
				Pending.push_back(Member{ Row, NowSec + K * T.spacingSec, std::string(BoundBeat) });
			}
		}

		/** A story beat ended: its burst members that have not played are dropped. */
		void StopBound(std::string_view Beat)
		{
			std::vector<Member> Keep;
			for (Member& M : Pending)
			{
				if (M.boundBeat.empty() || M.boundBeat != Beat)
				{
					Keep.push_back(std::move(M));
				}
			}
			Pending.swap(Keep);
		}

		/** The zone changed: pending bursts and `every` timers start over. */
		void OnZoneChanged()
		{
			Pending.clear();
			NextEverySec.clear();
		}

		/**
		 * Rows due now. Holds(std::size_t row) -> bool: the `every` row's condition (zone, quest, within, no story beat);
		 * Rand01() -> double in [0, 1): picks each interval.
		 */
		template <class HoldsFn, class RandFn>
		std::vector<std::size_t> Tick(const TriggerTables& Tables, double NowSec, HoldsFn&& Holds, RandFn&& Rand01)
		{
			std::vector<std::size_t> Due = TakeDue(NowSec);
			NextEverySec.resize(Tables.rows.size(), -1.0);
			for (std::size_t I = 0; I < Tables.rows.size(); ++I)
			{
				const Trigger& T = Tables.rows[I];
				if (T.on != On::Every)
				{
					continue;
				}
				if (!Holds(I))
				{
					NextEverySec[I] = -1.0;
					continue;
				}
				if (NextEverySec[I] < 0.0)
				{
					NextEverySec[I] = NowSec + Interval(T, Rand01());
				}
				else if (NowSec >= NextEverySec[I])
				{
					Due.push_back(I);
					NextEverySec[I] = NowSec + Interval(T, Rand01());
				}
			}
			return Due;
		}

		/** Burst members due now (an event handler plays a started row's first member at once with this). */
		std::vector<std::size_t> TakeDue(double NowSec)
		{
			std::vector<std::size_t> Due;
			std::vector<Member> Keep;
			for (Member& M : Pending)
			{
				if (M.dueSec <= NowSec)
				{
					Due.push_back(M.row);
				}
				else
				{
					Keep.push_back(std::move(M));
				}
			}
			Pending.swap(Keep);
			return Due;
		}

		std::size_t PendingCount() const { return Pending.size(); }

		static double Interval(const Trigger& T, double Unit)
		{
			const double U = Unit < 0.0 ? 0.0 : (Unit > 1.0 ? 1.0 : Unit);
			return T.everyLoSec + (T.everyHiSec - T.everyLoSec) * U;
		}

	private:
		struct Member
		{
			std::size_t row = 0;
			double dueSec = 0.0;
			std::string boundBeat;
		};
		std::vector<Member> Pending;
		std::vector<double> NextEverySec;
	};
}
