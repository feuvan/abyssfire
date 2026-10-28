// World-state presentation of the dark remake's Wave 2 (2G stage H; K6 world flags, NPC lines and visibility, the
// Ashnumb status; implementation-plan.md 2E / 2F "For 2G", DECISIONS DR16): decided without the engine so it is
// unit-tested (PresentationTests/tests/worldstate_test.cpp, hud_test.cpp).
//
// - Dawn (U1, chapter1.md 15 / 16): the zone's `<theme>_dawn` mood once a world flag the chapter's closing quest
//   (`chapterCompleteQuest`) sets is in Snapshot::worldFlags - the same flag the core's dawn spawn variants read, so the
//   grade and the trenches turn together. A closing quest that sets no flag falls back to Wave 1's rule (turned in). No
//   content id is named here. A map without a dawn mood (every zone of Chapters 2-5) keeps its mood either way.
// - The linear NPC panel's line (K6 DialogueView::lineKey): the key the core chose at open when it has one, else the web
//   rule (dialogue[1] ?? [0]).
// - NPCs a story beat still needs (K6 visibleWhen): the core hides an NPC the moment its condition stops holding
//   (EvEntityDespawned{Npc, Removed} at the next ZoneRuntime::Tick), which can be the turn-in that queues a cutscene
//   naming that NPC (Marl: hidden by M5's turn-in while cs_rf_finale focuses her and gives her a line). FStoryNpcHolds
//   expects the beats a quest event triggers (story.json triggers `quest_turned_in` / `quest_accepted` with that quest)
//   and the NPCs they name (focus targets and say speakers `{npc}`); the world builder keeps such an NPC's actor drawn
//   until that beat has ended. A hold also ends when the director is idle (Snapshot::storyBusy false: nothing playing or
//   queued, so a beat seen before - never queued - releases at once), on a zone change, or after a safety time.
// - Ashnumb on the hero (K6 HeroView::ashnumbStacks; vfx_recipes.json dark.materials.AshnumbTint): the band's top
//   (min(.12 + .10 x stacks, .62) of the hero's height) that M_AF_Toon_Ashnumb draws, and the StatusTint fallback where
//   the band is not drawn (mobile low, or the twin master missing): #8A8480 multiply at .25 + .05 x stacks. The character
//   status tints keep the web port's six entries and priority; Ashnumb's fallback comes after them. Nothing changes for
//   a character without Ashnumb (every character of Chapters 2-5).
//
// Std + AbyssCore public headers only (no CoreMinimal, no UE macros).
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Json.h"
#include "abyss/data/QuestData.h"
#include "abyss/data/StoryData.h"

namespace AbyssWorldStateRules
{
	// =================================================================================================================
	// Dawn
	// =================================================================================================================

	/** Whether Flags (a sorted set, Snapshot::worldFlags; nullptr = none) holds Flag. */
	inline bool HasFlag(const std::vector<std::string>* Flags, std::string_view Flag)
	{
		if (Flags == nullptr || Flag.empty())
		{
			return false;
		}
		const auto It = std::lower_bound(Flags->begin(), Flags->end(), Flag,
			[](const std::string& A, std::string_view B) { return std::string_view(A) < B; });
		return It != Flags->end() && std::string_view(*It) == Flag;
	}

	/**
	 * The chapter is complete for the presentation: a flag the closing quest sets is set; a closing quest without flags
	 * falls back to its turn-in (bTurnedIn). No closing quest: never.
	 */
	inline bool ChapterDawn(const abyss::QuestDef* ClosingQuest, const std::vector<std::string>* Flags, bool bTurnedIn)
	{
		if (ClosingQuest == nullptr)
		{
			return false;
		}
		if (ClosingQuest->setsFlags.empty())
		{
			return bTurnedIn;
		}
		for (const std::string& Flag : ClosingQuest->setsFlags)
		{
			if (HasFlag(Flags, Flag))
			{
				return true;
			}
		}
		return false;
	}

	// =================================================================================================================
	// The linear NPC panel's line
	// =================================================================================================================

	struct FPanelLine
	{
		std::string Key;          // i18n key ("" = no line)
		bool bFromLines = false;  // the K6 line the core chose
		std::size_t WebIndex = 0; // the web line (dialogue index) when not bFromLines
	};

	/** LineKey: DialogueView::lineKey; DialogueCount: the NPC's web dialogue lines. */
	inline FPanelLine LinearPanelLine(std::string_view LineKey, std::string_view NpcId, std::size_t DialogueCount)
	{
		FPanelLine Out;
		if (!LineKey.empty())
		{
			Out.Key = std::string(LineKey);
			Out.bFromLines = true;
			return Out;
		}
		if (DialogueCount == 0 || NpcId.empty())
		{
			return Out;
		}
		Out.WebIndex = DialogueCount > 1 ? 1 : 0;
		Out.Key = "data.npc." + std::string(NpcId) + ".dialogue." + std::to_string(Out.WebIndex);
		return Out;
	}

	// =================================================================================================================
	// NPCs a story beat still needs
	// =================================================================================================================

	/** The NPC ids a cutscene names (focus targets and say speakers `{npc}`), each once, in step order. */
	inline std::vector<std::string> CutsceneNpcs(const abyss::Cutscene& Beat)
	{
		std::vector<std::string> Out;
		const auto Add = [&Out](const abyss::StoryActor& Actor)
		{
			if (Actor.kind == abyss::StoryActorKind::Npc && !Actor.id.empty()
				&& std::find(Out.begin(), Out.end(), Actor.id) == Out.end())
			{
				Out.push_back(Actor.id);
			}
		};
		for (const abyss::CutsceneStep& Step : Beat.steps)
		{
			if (Step.kind == abyss::StoryStepKind::Focus)
			{
				Add(Step.target);
			}
			else if (Step.kind == abyss::StoryStepKind::Say)
			{
				Add(Step.speaker);
			}
		}
		return Out;
	}

	/** A focus step's NPC target ("" when the step is not a focus on an NPC). */
	inline std::string_view FocusNpc(const abyss::CutsceneStep& Step)
	{
		return Step.kind == abyss::StoryStepKind::Focus && Step.target.kind == abyss::StoryActorKind::Npc
			? std::string_view(Step.target.id) : std::string_view();
	}

	/** How long a hold may wait for its beat to begin, and how long it may last at all (real seconds). */
	inline constexpr double HoldWaitBeginSec = 300.0;
	inline constexpr double HoldMaxSec = 900.0;

	class FStoryNpcHolds
	{
	public:
		struct FExpected
		{
			std::string BeatId;
			std::vector<std::string> NpcIds;
			double SinceSec = 0.0;
			bool bBegan = false;
		};

		/**
		 * A quest event (On = QuestTurnedIn for EvQuestUpdate{TurnedIn}, QuestAccepted for {Accepted}): expects every
		 * cutscene its triggers name that names an NPC. Returns how many beats were added.
		 */
		int Expect(const abyss::StoryScript& Story, abyss::StoryTriggerOn On, std::string_view QuestId, double NowSec)
		{
			int Added = 0;
			if (QuestId.empty())
			{
				return Added;
			}
			for (const abyss::StoryTriggerDef& Trigger : Story.triggers)
			{
				if (Trigger.on != On || Trigger.subjectId != QuestId || Trigger.cutscene.empty())
				{
					continue;
				}
				const abyss::Cutscene* Beat = Story.FindCutscene(Trigger.cutscene);
				if (Beat == nullptr)
				{
					continue;
				}
				std::vector<std::string> Npcs = CutsceneNpcs(*Beat);
				if (Npcs.empty())
				{
					continue;
				}
				FExpected* Existing = FindBeat(Trigger.cutscene);
				if (Existing != nullptr)
				{
					Existing->SinceSec = NowSec;   // re-armed (a reacceptable quest's trigger again)
					continue;
				}
				Expected.push_back(FExpected{ Trigger.cutscene, std::move(Npcs), NowSec, false });
				++Added;
			}
			return Added;
		}

		/** An NPC a beat that has not ended still needs. */
		bool Wants(std::string_view NpcId) const
		{
			if (NpcId.empty())
			{
				return false;
			}
			for (const FExpected& E : Expected)
			{
				if (std::find(E.NpcIds.begin(), E.NpcIds.end(), NpcId) != E.NpcIds.end())
				{
					return true;
				}
			}
			return false;
		}

		void BeatBegan(std::string_view BeatId)
		{
			if (FExpected* E = FindBeat(BeatId))
			{
				E->bBegan = true;
			}
		}

		/** The beat ended: its NPCs are released. Returns whether it was expected. */
		bool BeatEnded(std::string_view BeatId)
		{
			const std::size_t Before = Expected.size();
			Expected.erase(std::remove_if(Expected.begin(), Expected.end(),
				[BeatId](const FExpected& E) { return E.BeatId == BeatId; }), Expected.end());
			return Expected.size() != Before;
		}

		/**
		 * Per frame, after the events. The director is busy (Snapshot::storyBusy) from the moment a beat is queued until
		 * the queue drains, so while it is idle no expected beat is playing or coming (one seen before is never queued):
		 * every hold ends. A beat that waited HoldWaitBeginSec without beginning, or was held HoldMaxSec, is dropped too.
		 */
		void Update(double NowSec, bool bStoryBusy)
		{
			Expected.erase(std::remove_if(Expected.begin(), Expected.end(),
				[NowSec, bStoryBusy](const FExpected& E)
				{
					const double Age = NowSec - E.SinceSec;
					return !bStoryBusy || (!E.bBegan && Age > HoldWaitBeginSec) || Age > HoldMaxSec;
				}), Expected.end());
		}

		void Clear() { Expected.clear(); }
		bool Empty() const { return Expected.empty(); }
		const std::vector<FExpected>& Entries() const { return Expected; }

	private:
		FExpected* FindBeat(std::string_view BeatId)
		{
			for (FExpected& E : Expected)
			{
				if (E.BeatId == BeatId)
				{
					return &E;
				}
			}
			return nullptr;
		}

		std::vector<FExpected> Expected;
	};

	// =================================================================================================================
	// Ashnumb on the hero
	// =================================================================================================================

	/** vfx_recipes.json dark.materials.AshnumbTint (the defaults are the contract as shipped). */
	struct FAshnumbTint
	{
		uint32_t AshRgb = 0x8A8480;
		uint32_t ShadeRgb = 0x4C4944;
		uint32_t EdgeRgb = 0xB8B2A6;
		double EdgeWidth = 0.04;
		double BandTopBase = 0.12;
		double BandTopPerStack = 0.1;
		double BandSoftness = 0.06;
		double MaxBandTop = 0.62;
		double Strength = 0.85;
		// The StatusTint path where the band is not drawn ("#8A8480 multiply at .25 + .05 x stacks").
		uint32_t FallbackRgb = 0x8A8480;
		double FallbackBase = 0.25;
		double FallbackPerStack = 0.05;
		bool bPresent = false;   // parsed from a contract (else these defaults)
	};

	namespace Detail
	{
		inline int HexDigit(char C)
		{
			if (C >= '0' && C <= '9') return C - '0';
			if (C >= 'a' && C <= 'f') return C - 'a' + 10;
			if (C >= 'A' && C <= 'F') return C - 'A' + 10;
			return -1;
		}

		/** "#RRGGBB" (or "RRGGBB") -> 0xRRGGBB. */
		inline bool ParseRgb(std::string_view Text, uint32_t& Out)
		{
			if (!Text.empty() && Text.front() == '#')
			{
				Text.remove_prefix(1);
			}
			if (Text.size() != 6)
			{
				return false;
			}
			uint32_t Value = 0;
			for (const char C : Text)
			{
				const int D = HexDigit(C);
				if (D < 0)
				{
					return false;
				}
				Value = (Value << 4) | static_cast<uint32_t>(D);
			}
			Out = Value;
			return true;
		}

		/** The first number in Text from Pos ("0.25", ".05", "5"); advances Pos past it. False when none. */
		inline bool NextNumber(std::string_view Text, std::size_t& Pos, double& Out)
		{
			while (Pos < Text.size())
			{
				const char C = Text[Pos];
				const bool bDigit = C >= '0' && C <= '9';
				const bool bDotDigit = C == '.' && Pos + 1 < Text.size() && Text[Pos + 1] >= '0' && Text[Pos + 1] <= '9';
				if (bDigit || bDotDigit)
				{
					break;
				}
				++Pos;
			}
			if (Pos >= Text.size())
			{
				return false;
			}
			double Whole = 0.0;
			while (Pos < Text.size() && Text[Pos] >= '0' && Text[Pos] <= '9')
			{
				Whole = Whole * 10.0 + static_cast<double>(Text[Pos] - '0');
				++Pos;
			}
			if (Pos < Text.size() && Text[Pos] == '.')
			{
				++Pos;
				double Scale = 0.1;
				while (Pos < Text.size() && Text[Pos] >= '0' && Text[Pos] <= '9')
				{
					Whole += static_cast<double>(Text[Pos] - '0') * Scale;
					Scale *= 0.1;
					++Pos;
				}
			}
			Out = Whole;
			return true;
		}

		inline void ReadNumber(const abyss::JsonValue& V, double& InOut, double Lo, double Hi)
		{
			if (V.IsNumber())
			{
				const double X = V.AsDouble(InOut);
				if (X >= Lo && X <= Hi)
				{
					InOut = X;
				}
			}
		}
	}

	inline FAshnumbTint DefaultAshnumbTint()
	{
		return FAshnumbTint();
	}

	/**
	 * The AshnumbTint object (vfx_recipes.json `dark.materials.AshnumbTint`): its params, and the fallback from its `map`
	 * text ("... use the StatusTint path: #8A8480 multiply at .25 + .05 x stacks"). Missing or malformed fields keep the
	 * shipped values.
	 */
	inline FAshnumbTint ParseAshnumbTint(const abyss::JsonValue& Contract)
	{
		FAshnumbTint Out;
		if (!Contract.IsObject())
		{
			return Out;
		}
		Out.bPresent = true;
		const abyss::JsonValue& P = Contract.Get("params");
		Detail::ParseRgb(P.Get("AshColor").AsString(), Out.AshRgb);
		Detail::ParseRgb(P.Get("AshShade").AsString(), Out.ShadeRgb);
		Detail::ParseRgb(P.Get("EdgeColor").AsString(), Out.EdgeRgb);
		Detail::ReadNumber(P.Get("EdgeWidth"), Out.EdgeWidth, 0.0, 1.0);
		Detail::ReadNumber(P.Get("BandTopBase"), Out.BandTopBase, 0.0, 1.0);
		Detail::ReadNumber(P.Get("BandTopPerStack"), Out.BandTopPerStack, 0.0, 1.0);
		Detail::ReadNumber(P.Get("BandSoftness"), Out.BandSoftness, 0.0, 1.0);
		Detail::ReadNumber(P.Get("MaxBandTop"), Out.MaxBandTop, 0.0, 1.0);
		Detail::ReadNumber(P.Get("Strength"), Out.Strength, 0.0, 1.0);
		const std::string_view Map = Contract.Get("map").AsString();
		const std::size_t Tint = Map.find("StatusTint");
		if (Tint != std::string_view::npos)
		{
			const std::string_view Tail = Map.substr(Tint);
			const std::size_t Hash = Tail.find('#');
			if (Hash != std::string_view::npos)
			{
				Detail::ParseRgb(Tail.substr(Hash, 7), Out.FallbackRgb);
			}
			const std::size_t At = Tail.find("multiply at");
			if (At != std::string_view::npos)
			{
				std::size_t Pos = At;
				double Base = 0.0;
				double PerStack = 0.0;
				if (Detail::NextNumber(Tail, Pos, Base) && Detail::NextNumber(Tail, Pos, PerStack) && Base <= 1.0
					&& PerStack <= 1.0)
				{
					Out.FallbackBase = Base;
					Out.FallbackPerStack = PerStack;
				}
			}
		}
		return Out;
	}

	/** The band's top as a share of the hero's height (0 = no band: no stacks). */
	inline double BandTop(const FAshnumbTint& T, int32_t Stacks)
	{
		if (Stacks <= 0)
		{
			return 0.0;
		}
		const double Top = T.BandTopBase + static_cast<double>(Stacks) * T.BandTopPerStack;
		return Top < T.MaxBandTop ? Top : T.MaxBandTop;
	}

	/** The fallback multiply's strength (0 = none: no stacks), capped at 1. */
	inline double FallbackTintAlpha(const FAshnumbTint& T, int32_t Stacks)
	{
		if (Stacks <= 0)
		{
			return 0.0;
		}
		const double A = T.FallbackBase + static_cast<double>(Stacks) * T.FallbackPerStack;
		return A < 1.0 ? A : 1.0;
	}

	/** The render tiers that draw the per-stack band: every one but mobile low (the contract's fallback there). */
	inline bool BandAllowed(int Tier, bool bMobile)
	{
		return !(bMobile && Tier <= 0);
	}

	/** A character's status tint (R8): the first status present in priority order tints the body. */
	struct FStatusTint
	{
		abyss::StatusType Type;
		uint32_t Rgb;
		float Alpha;
	};

	/** The web port's six (priority order, AbyssCharacterActor R8); Ashnumb's fallback comes after them. */
	inline constexpr FStatusTint StatusTints[] = {
		{ abyss::StatusType::Freeze, 0x7FD0FF, 0.45f },
		{ abyss::StatusType::Stun, 0x7FD0FF, 0.45f },
		{ abyss::StatusType::Burn, 0xFF7A3A, 0.35f },
		{ abyss::StatusType::Poison, 0x8FE04A, 0.35f },
		{ abyss::StatusType::Bleed, 0xC8323C, 0.22f },
		{ abyss::StatusType::Slow, 0x9FD8FF, 0.18f },
	};

	struct FTintPick
	{
		bool bFound = false;
		uint32_t Rgb = 0;
		float Alpha = 0.f;
	};

	/**
	 * The tint of a character: the first of the six present; else, while it carries Ashnumb (its bit or stacks) and the
	 * band is not drawn (bBandShown false), the fallback multiply for its stacks (at least one).
	 */
	inline FTintPick CharacterStatusTint(uint32_t StatusMask, int32_t AshnumbStacks, bool bBandShown, const FAshnumbTint& T)
	{
		FTintPick Out;
		for (const FStatusTint& Status : StatusTints)
		{
			if ((StatusMask & (1u << static_cast<uint32_t>(Status.Type))) != 0u)
			{
				Out.bFound = true;
				Out.Rgb = Status.Rgb;
				Out.Alpha = Status.Alpha;
				return Out;
			}
		}
		const bool bAshnumb = AshnumbStacks > 0
			|| (StatusMask & (1u << static_cast<uint32_t>(abyss::StatusType::Ashnumb))) != 0u;
		if (bAshnumb && !bBandShown)
		{
			Out.bFound = true;
			Out.Rgb = T.FallbackRgb;
			Out.Alpha = static_cast<float>(FallbackTintAlpha(T, AshnumbStacks > 0 ? AshnumbStacks : 1));
		}
		return Out;
	}
}
