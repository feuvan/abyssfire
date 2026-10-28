// AbyssMusicRules: which track plays in the dark remake's zone (U7; manifest `musicDirector`, AU3 dark_scores.json).
//
// The core's MusicDirector still decides the zone / combat / boss / victory state and sends EvMusic; this helper layers
// the remake's director on top, in its priority order (musicDirector.priority):
//
//   story    a story sequence (prologue / epilogue / credits) plays the core's track; a story hold (cs_rf_finale from
//            its step `stepIndex`: silence, then `afterSec` after the beat the `then` track) beats everything else
//   victory  the core's victory track (a "<theme>_victory" key)
//   boss     the core's boss score (a bossMusic key, e.g. boss_ch1)
//   siege    a quest-bound track (cs_rf_siege: `track` while `quest` is active and its beat is not playing; `then`
//            once the quest's objectives are done, until the turn-in)
//   region   the smallest region holding the hero whose `while` holds (track "" = silence)
//   zone     the core's zone track (explore / combat after the manifest's zoneOverrides)
//
// Siege, region and the story hold only apply in the director's zone. A change keeps the A2 resume (restart = false)
// except a fresh core request, which keeps the core's own fades and restart flag; a zone-overridden core request that
// resolves to the track already sounding is a no-op even with restart = true (the combat edge of a one-track zone:
// musicDirector.zoneTracks.combat = the drones). Fades: entering a layer, the outgoing track fades over the leaving
// layer's fadeOutSec (else the entering layer's, else the core's stateFadeSec) and the layer's track fades in over its
// fadeInSec (else the core's fadeInSec); returning to the core's track fades in over the core's fadeInSec.
//
// Also: the encounter stingers (`encounters`: engaged / slain per monster), the boss phase stingers (EvMonsterPhase:
// the event's id, else `bosses.<id>.phaseStingers`; 2G) and every stinger's asset and duck.
//
// Pure helper (std + AbyssCore public headers only); tested in PresentationTests/tests/audio_music_test.cpp.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "abyss/base/Json.h"

#include "Audio/AbyssAudioDataRules.h"

namespace AbyssMusicRules
{
	struct Region
	{
		std::string id;
		AbyssAudioData::TileCircle area;
		std::string track;  // "" = silence
		AbyssAudioData::WhileCondition when;
		double fadeInSec = -1.0;   // < 0: the default
		double fadeOutSec = -1.0;
	};

	struct Stinger
	{
		std::string id;
		std::string asset;
		double duckDb = 0.0;
		double duckInSec = 0.5;
		double duckOutSec = 0.5;
		double lengthSec = 0.0;
	};

	struct Encounter
	{
		std::string id;  // "hunt", "miniboss"
		std::vector<std::string> monsters;
		std::string engaged;  // stinger ids ("" = none)
		std::string slain;
	};

	/** A story row bound to a quest (cs_rf_siege): `track` while the quest is active, `then` once it is completed. */
	struct QuestTrack
	{
		std::string beat;
		std::string quest;
		std::string track;
		bool hasThen = false;
		std::string thenTrack;
		double fadeInSec = -1.0;
		double fadeOutSec = -1.0;
	};

	/** A story row from a cutscene step on (cs_rf_finale): `track` until `afterSec` past the beat, then `thenTrack`. */
	struct StoryHold
	{
		std::string beat;
		int stepIndex = 0;
		std::string track;
		double fadeOutSec = -1.0;
		bool hasThen = false;
		double thenAfterSec = 0.0;
		std::string thenTrack;
		double thenFadeInSec = -1.0;
	};

	struct Boss
	{
		std::string monster;
		std::string track;
		std::string victory;
		/** 2G (U2): `phaseStingers` {"<phase>": stinger id}: EvMonsterPhase::phase (1 = the first row entered). */
		std::vector<std::pair<int, std::string>> phaseStingers;
	};

	struct DirectorTables
	{
		std::string zone;
		std::vector<Region> regions;
		std::vector<AbyssAudioData::TileCircle> regionAreas;  // regions[i].area (for the tracker)
		std::vector<Stinger> stingers;
		std::vector<Encounter> encounters;
		std::vector<QuestTrack> questTracks;
		std::vector<StoryHold> holds;
		std::vector<Boss> bosses;
		bool loaded = false;
	};

	inline DirectorTables Parse(const abyss::JsonValue& Director)
	{
		using namespace AbyssAudioData;
		DirectorTables Out;
		if (!Director.IsObject())
		{
			return Out;
		}
		Out.zone = Str(Director.Get("zone"));
		for (const abyss::JsonValue& Row : Director.Get("regions").Items())
		{
			Region R;
			R.id = Str(Row.Get("id"));
			R.area = ReadCircle(Row);
			R.track = Str(Row.Get("track"));
			R.when = ReadWhile(Row);
			R.fadeInSec = Num(Row.Get("fadeInSec"), -1.0);
			R.fadeOutSec = Num(Row.Get("fadeOutSec"), -1.0);
			if (!R.id.empty() && R.area.radius > 0.0)
			{
				Out.regionAreas.push_back(R.area);
				Out.regions.push_back(std::move(R));
			}
		}
		for (const abyss::JsonMember& Row : Director.Get("stingers").Members())
		{
			Stinger S;
			S.id = Row.key;
			S.asset = Str(Row.value.Get("asset"));
			const abyss::JsonValue& Duck = Row.value.Get("duck");
			S.duckDb = Num(Duck.Get("db"), 0.0);
			S.duckInSec = Num(Duck.Get("inSec"), 0.5);
			S.duckOutSec = Num(Duck.Get("outSec"), 0.5);
			S.lengthSec = Num(Row.value.Get("lengthSec"), 0.0);
			if (!S.asset.empty())
			{
				Out.stingers.push_back(std::move(S));
			}
		}
		for (const abyss::JsonMember& Row : Director.Get("encounters").Members())
		{
			Encounter E;
			E.id = Row.key;
			E.monsters = Strings(Row.value.Get("monsters"));
			E.engaged = Str(Row.value.Get("engaged"));
			E.slain = Str(Row.value.Get("slain"));
			Out.encounters.push_back(std::move(E));
		}
		for (const abyss::JsonValue& Row : Director.Get("story").Items())
		{
			const std::string Beat = Str(Row.Get("beat"));
			if (Beat.empty() || !Row.Has("track"))
			{
				continue;  // a stinger / note row (the chapter card stinger comes from stingers.chapterCard)
			}
			if (Row.Has("quest"))
			{
				QuestTrack Q;
				Q.beat = Beat;
				Q.quest = Str(Row.Get("quest"));
				Q.track = Str(Row.Get("track"));
				Q.hasThen = Row.Get("then").IsString();
				Q.thenTrack = Str(Row.Get("then"));
				Q.fadeInSec = Num(Row.Get("fadeInSec"), -1.0);
				Q.fadeOutSec = Num(Row.Get("fadeOutSec"), -1.0);
				Out.questTracks.push_back(std::move(Q));
			}
			else if (Row.Get("stepIndex").IsNumber())
			{
				StoryHold H;
				H.beat = Beat;
				H.stepIndex = Row.Get("stepIndex").AsInt(0);
				H.track = Str(Row.Get("track"));
				H.fadeOutSec = Num(Row.Get("fadeOutSec"), -1.0);
				const abyss::JsonValue& Then = Row.Get("then");
				if (Then.IsObject())
				{
					H.hasThen = true;
					H.thenAfterSec = Num(Then.Get("afterSec"), 0.0);
					H.thenTrack = Str(Then.Get("track"));
					H.thenFadeInSec = Num(Then.Get("fadeInSec"), -1.0);
				}
				Out.holds.push_back(std::move(H));
			}
		}
		for (const abyss::JsonMember& Row : Director.Get("bosses").Members())
		{
			Boss B{ Row.key, Str(Row.value.Get("track")), Str(Row.value.Get("victory")), {} };
			for (const abyss::JsonMember& Phase : Row.value.Get("phaseStingers").Members())
			{
				int Index = 0;
				bool bDigits = !Phase.key.empty();
				for (const char C : Phase.key)
				{
					if (C < '0' || C > '9' || Index > 100000)
					{
						bDigits = false;
						break;
					}
					Index = Index * 10 + (C - '0');
				}
				const std::string Id = Str(Phase.value);
				if (bDigits && !Id.empty())
				{
					B.phaseStingers.emplace_back(Index, Id);
				}
			}
			Out.bosses.push_back(std::move(B));
		}
		Out.loaded = true;
		return Out;
	}

	inline const Stinger* FindStinger(const DirectorTables& Tables, std::string_view Id)
	{
		for (const Stinger& S : Tables.stingers)
		{
			if (!Id.empty() && S.id == Id)
			{
				return &S;
			}
		}
		return nullptr;
	}

	/**
	 * 2G (U2): the stinger of a boss phase (EvMonsterPhase): the event's own id (the core's phases[].stinger) when it is a
	 * stinger of the director, else the manifest's bosses.<defId>.phaseStingers row for the phase; none on a reset (the
	 * boss walked home: phase 0, no music beat) or for a monster without either (the oath zealot's frenzy).
	 */
	inline const Stinger* PhaseStinger(const DirectorTables& Tables, std::string_view DefId, int Phase,
		std::string_view EventStinger, bool bReset)
	{
		if (bReset || Phase <= 0)
		{
			return nullptr;
		}
		if (const Stinger* Own = FindStinger(Tables, EventStinger))
		{
			return Own;
		}
		for (const Boss& B : Tables.bosses)
		{
			if (B.monster != DefId)
			{
				continue;
			}
			for (const std::pair<int, std::string>& Row : B.phaseStingers)
			{
				if (Row.first == Phase)
				{
					return FindStinger(Tables, Row.second);
				}
			}
		}
		return nullptr;
	}

	enum class EncounterBeat : unsigned char
	{
		Engaged,
		Slain,
	};

	/** The stinger of an encounter monster for that beat (nullptr: not an encounter monster / no stinger). */
	inline const Stinger* EncounterStinger(const DirectorTables& Tables, std::string_view DefId, EncounterBeat Beat)
	{
		for (const Encounter& E : Tables.encounters)
		{
			if (AbyssAudioData::Contains(E.monsters, DefId))
			{
				return FindStinger(Tables, Beat == EncounterBeat::Engaged ? E.engaged : E.slain);
			}
		}
		return nullptr;
	}

	// ---- the core's request ----------------------------------------------------------------------------------------

	enum class RequestKind : unsigned char
	{
		Zone,     // "<theme>_explore" / "<theme>_combat" (and "" = a zone without music)
		Victory,  // "<theme>_victory"
		Boss,     // a boss score (the core's bossMusic values, the manifest's bosses[].track)
	};

	inline bool EndsWith(std::string_view Text, std::string_view Suffix)
	{
		return Text.size() >= Suffix.size() && Text.substr(Text.size() - Suffix.size()) == Suffix;
	}

	/** Classifies a core key (EvMusic::trackKey before the zone override). */
	inline RequestKind Classify(std::string_view CoreKey, const std::vector<std::string>& BossScores)
	{
		if (CoreKey.empty())
		{
			return RequestKind::Zone;
		}
		if (AbyssAudioData::Contains(BossScores, CoreKey))
		{
			return RequestKind::Boss;
		}
		return EndsWith(CoreKey, "_victory") ? RequestKind::Victory : RequestKind::Zone;
	}

	struct CoreRequest
	{
		std::string track;  // after the zone override; "" = silence
		RequestKind kind = RequestKind::Zone;
		double fadeOutSec = 0.0;
		double fadeInSec = 0.0;
		bool loop = true;
		bool restart = false;
		bool overridden = false;  // the zone override changed the key
	};

	/** The core's default fades (MusicDirectorDef stateFadeSec / fadeInSec), used for layer changes. */
	struct DefaultFades
	{
		double stateFadeSec = 1.5;
		double fadeInSec = 1.0;
	};

	// ---- per-frame selection ---------------------------------------------------------------------------------------

	/** The region the hero is in now: the smallest (radius) region holding the hero whose `while` holds; -1 = none. */
	template <class IsAliveFn>
	int PickRegion(const DirectorTables& Tables, const AbyssAudioData::CircleTracker& Tracker, IsAliveFn&& IsAlive)
	{
		int Best = -1;
		for (std::size_t I = 0; I < Tables.regions.size(); ++I)
		{
			const Region& R = Tables.regions[I];
			if (!Tracker.IsInside(I) || !AbyssAudioData::Holds(R.when, IsAlive))
			{
				continue;
			}
			if (Best < 0 || R.area.radius < Tables.regions[static_cast<std::size_t>(Best)].area.radius)
			{
				Best = static_cast<int>(I);
			}
		}
		return Best;
	}

	enum class QuestPhase : unsigned char
	{
		None,       // not accepted, turned in or failed
		Active,     // accepted, objectives not done
		Completed,  // objectives done, not turned in
	};

	struct QuestTrackPick
	{
		int index = -1;
		bool then = false;
	};

	/** The quest-bound row that plays now. PhaseOf(std::string_view questId) -> QuestPhase. PlayingBeat: the story beat
	 *  playing now ("" = none): a row waits for the end of its own beat. */
	template <class PhaseFn>
	QuestTrackPick PickQuestTrack(const DirectorTables& Tables, std::string_view PlayingBeat, PhaseFn&& PhaseOf)
	{
		for (std::size_t I = 0; I < Tables.questTracks.size(); ++I)
		{
			const QuestTrack& Q = Tables.questTracks[I];
			if (Q.beat == PlayingBeat)
			{
				continue;
			}
			const QuestPhase Phase = PhaseOf(std::string_view(Q.quest));
			if (Phase == QuestPhase::Active)
			{
				return QuestTrackPick{ static_cast<int>(I), false };
			}
			if (Phase == QuestPhase::Completed && Q.hasThen)
			{
				return QuestTrackPick{ static_cast<int>(I), true };
			}
		}
		return QuestTrackPick{};
	}

	enum class Source : unsigned char
	{
		None,
		Core,     // the core's request (zone, victory, boss, or a sequence's track)
		Hold,     // a story hold (or its `then`)
		Siege,    // a quest-bound row
		Region,
	};

	/** What the layers say this frame (computed by the glue; Siege / Region only in the director's zone). */
	struct FrameState
	{
		bool inDirectorZone = false;
		bool sequence = false;  // a story sequence beat plays: the core's track
		QuestTrackPick siege;
		int region = -1;
	};

	struct Decision
	{
		bool change = false;
		std::string track;  // "" = silence
		double fadeOutSec = 0.0;
		double fadeInSec = 0.0;
		bool loop = true;
		bool restart = false;
		Source source = Source::None;
	};

	/**
	 * The arbitration between the core's request and the remake's layers. Feed it the core's EvMusic (SetCore), the
	 * story steps and beat ends, then call Update once per frame with the layer state and the track sounding now; apply
	 * the Decision when `change` (a crossfade to `track`).
	 */
	class Arbiter
	{
	public:
		void Reset()
		{
			*this = Arbiter();
		}

		/** An EvMusic arrived (the latest wins until the next Update). */
		void SetCore(const CoreRequest& Request)
		{
			Core = Request;
			bHasCore = true;
			bCorePending = true;
		}

		/** A cutscene step started: a story hold whose beat and step match begins (silence from here). */
		void OnStoryStep(const DirectorTables& Tables, std::string_view Beat, int Index, bool bIsSlide)
		{
			if (bIsSlide)
			{
				return;
			}
			for (std::size_t I = 0; I < Tables.holds.size(); ++I)
			{
				if (Tables.holds[I].beat == Beat && Tables.holds[I].stepIndex == Index)
				{
					HoldIndex = static_cast<int>(I);
					bHoldBeatEnded = false;
					bHoldThenPending = false;
				}
			}
		}

		/** A story beat ended: the hold of that beat ends `thenAfterSec` later. */
		void OnStoryEnded(const DirectorTables& Tables, std::string_view Beat, double NowSec)
		{
			if (HoldIndex >= 0 && Tables.holds[static_cast<std::size_t>(HoldIndex)].beat == Beat && !bHoldBeatEnded)
			{
				bHoldBeatEnded = true;
				HoldEndsAtSec = NowSec + Tables.holds[static_cast<std::size_t>(HoldIndex)].thenAfterSec;
			}
		}

		/** The hero left the zone: a story hold ends without its `then`. */
		void OnZoneChanged()
		{
			HoldIndex = -1;
			bHoldBeatEnded = false;
			bHoldThenPending = false;
		}

		bool IsHolding() const { return HoldIndex >= 0; }
		Source CurrentSource() const { return Last; }
		const CoreRequest& GetCore() const { return Core; }

		/** One frame: the decision to apply (change = false: keep what plays). */
		Decision Update(const DirectorTables& Tables, const FrameState& State, const DefaultFades& Fades,
			std::string_view SoundingTrack, double NowSec)
		{
			Decision Want;
			int WantLayer = -1;
			bool bThen = false;
			// The story hold ends `afterSec` after its beat; its `then` track plays once (then the layers take over).
			if (HoldIndex >= 0 && bHoldBeatEnded && NowSec >= HoldEndsAtSec)
			{
				bHoldThenPending = Tables.holds[static_cast<std::size_t>(HoldIndex)].hasThen;
				ThenIndex = HoldIndex;
				HoldIndex = -1;
			}
			if (State.sequence && bHasCore)
			{
				Want = CoreWant(Source::Core);
			}
			else if (HoldIndex >= 0)
			{
				const StoryHold& H = Tables.holds[static_cast<std::size_t>(HoldIndex)];
				Want.track = H.track;
				Want.source = Source::Hold;
				Want.fadeOutSec = H.fadeOutSec >= 0.0 ? H.fadeOutSec : Fades.stateFadeSec;
				Want.fadeInSec = Fades.fadeInSec;
				WantLayer = HoldIndex;
			}
			else if (bHoldThenPending && ThenIndex >= 0)
			{
				const StoryHold& H = Tables.holds[static_cast<std::size_t>(ThenIndex)];
				bHoldThenPending = false;
				bThen = true;
				Want.track = H.thenTrack;
				Want.source = Source::Hold;
				Want.fadeOutSec = Fades.stateFadeSec;
				Want.fadeInSec = H.thenFadeInSec >= 0.0 ? H.thenFadeInSec : Fades.fadeInSec;
				WantLayer = ThenIndex;
			}
			else if (bHasCore && (Core.kind == RequestKind::Victory || Core.kind == RequestKind::Boss))
			{
				Want = CoreWant(Source::Core);
			}
			else if (State.inDirectorZone && State.siege.index >= 0 &&
				static_cast<std::size_t>(State.siege.index) < Tables.questTracks.size())
			{
				const QuestTrack& Q = Tables.questTracks[static_cast<std::size_t>(State.siege.index)];
				Want.track = State.siege.then ? Q.thenTrack : Q.track;
				Want.source = Source::Siege;
				Want.fadeOutSec = LeavingFadeOut(Tables) >= 0.0 ? LeavingFadeOut(Tables)
					: (Q.fadeOutSec >= 0.0 ? Q.fadeOutSec : Fades.stateFadeSec);
				Want.fadeInSec = Q.fadeInSec >= 0.0 ? Q.fadeInSec : Fades.fadeInSec;
				WantLayer = State.siege.index;
			}
			else if (State.inDirectorZone && State.region >= 0 &&
				static_cast<std::size_t>(State.region) < Tables.regions.size())
			{
				const Region& R = Tables.regions[static_cast<std::size_t>(State.region)];
				Want.track = R.track;
				Want.source = Source::Region;
				Want.fadeOutSec = LeavingFadeOut(Tables) >= 0.0 ? LeavingFadeOut(Tables)
					: (R.fadeOutSec >= 0.0 ? R.fadeOutSec : Fades.stateFadeSec);
				Want.fadeInSec = R.fadeInSec >= 0.0 ? R.fadeInSec : Fades.fadeInSec;
				WantLayer = State.region;
			}
			else if (bHasCore)
			{
				Want = CoreWant(Source::Core);
			}
			else
			{
				return Decision{};  // nothing requested yet: leave the music alone
			}

			const bool bFresh = Want.source == Source::Core && bCorePending;
			bCorePending = false;  // a request a layer shadows is kept (Core) and resumed when the layer ends
			const bool bSame = Want.track == SoundingTrack;
			Decision Out;
			if (Want.source == Source::Core)
			{
				if (bFresh)
				{
					// The core's own fades and restart; a zone-overridden key already sounding is a no-op even with
					// restart (the combat edge of a one-track zone), the same as a resume of the sounding track.
					Out = Want;
					Out.change = !(bSame && (!Core.restart || Core.overridden));
				}
				else if (!bSame && Core.loop)
				{
					// A layer let go: back to the core's track, resumed where it was (A2). A one-shot (victory) plays only
					// when the core asks for it: one that ended, or that a layer interrupted, is not played again.
					Out = Want;
					Out.fadeOutSec = LeavingFadeOut(Tables) >= 0.0 ? LeavingFadeOut(Tables) : Fades.stateFadeSec;
					Out.fadeInSec = Fades.fadeInSec;
					Out.restart = false;
					Out.change = true;
				}
			}
			else if (!bSame)
			{
				Out = Want;
				Out.loop = true;
				Out.restart = false;
				Out.change = true;
			}
			Out.source = Want.source;
			Last = Want.source;
			LastLayer = bThen ? -1 : WantLayer;
			if (bThen)
			{
				Last = Source::None;  // the `then` track was a one-off: the next frame's layers decide freely
			}
			return Out;
		}

	private:
		Decision CoreWant(Source Kind) const
		{
			Decision D;
			D.track = Core.track;
			D.fadeOutSec = Core.fadeOutSec;
			D.fadeInSec = Core.fadeInSec;
			D.loop = Core.loop;
			D.restart = Core.restart;
			D.source = Kind;
			return D;
		}

		/** The fade-out of the layer the music is leaving (< 0: none of its own). */
		double LeavingFadeOut(const DirectorTables& Tables) const
		{
			if (Last == Source::Region && LastLayer >= 0 && static_cast<std::size_t>(LastLayer) < Tables.regions.size())
			{
				return Tables.regions[static_cast<std::size_t>(LastLayer)].fadeOutSec;
			}
			if (Last == Source::Siege && LastLayer >= 0 && static_cast<std::size_t>(LastLayer) < Tables.questTracks.size())
			{
				return Tables.questTracks[static_cast<std::size_t>(LastLayer)].fadeOutSec;
			}
			return -1.0;
		}

		CoreRequest Core;
		bool bHasCore = false;
		bool bCorePending = false;
		int HoldIndex = -1;
		bool bHoldBeatEnded = false;
		double HoldEndsAtSec = 0.0;
		bool bHoldThenPending = false;
		int ThenIndex = -1;
		Source Last = Source::None;
		int LastLayer = -1;
	};
}
