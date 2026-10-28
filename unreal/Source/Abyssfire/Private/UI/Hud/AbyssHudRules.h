// HUD rules of the dark remake's Wave 2 (2G stage H; implementation-plan.md 4 U5 W2, DECISIONS DR16): what SAbyssHud /
// SAbyssBossBar / SAbyssBuffRow draw for the K4 / K6 snapshot fields, decided without the engine so it is unit-tested
// (PresentationTests/tests/hud_test.cpp).
//
// - Boss bar phase ticks (K4, BossBarView::phase / phaseThresholdsPct): one tick per threshold of the boss's phase table
//   at that share of the bar (row order); the rows already entered (index < phase) are dimmed. A threshold outside
//   (0, 100) has no place on the bar and draws nothing. A boss without a phase table (every boss of Chapters 2-5) has no
//   ticks: the bar is today's.
// - The status row (HeroView::statusMask, K6 ashnumbStacks): one cell per status present, in StatusType order (burn,
//   freeze, poison, bleed, slow, stun, then K6's Ashnumb), each in its colour; Ashnumb (#8A8480, AshnumbTint's ash)
//   carries its stack count. The six web statuses keep today's colours and order, so a row without Ashnumb is today's.
//   Keen (the Corpseherd's status-only strike) arrives as a plain Slow: its cell is the Slow cell.
// - The permanent-reward toast (K6 EvPermanentReward): the title is `data.quest.<id>.permanent` when the locale has it
//   (S1: 玛勒的苦药 / Marl's Bitter Draught), else the quest's name; one `ui.permanentReward.<field>` line ("+N" and the
//   stat) per field above 0, in the event's order (max life, stat points, skill points, respec tokens).
//
// Std + AbyssCore public headers only (no CoreMinimal, no UE macros): the module includes it after CoreMinimal.h, the
// macro shim (PresentationTests/shim) compiles it under the UE / Windows / Apple macros.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/sim/Events.h"

namespace AbyssHudRules
{
	// =================================================================================================================
	// Boss bar phase ticks
	// =================================================================================================================

	struct FPhaseTick
	{
		double Fraction = 0.0;   // share of the bar from its left (empty) end: the threshold / 100
		bool bPassed = false;    // the row is entered (dimmed)
		int32_t Row = 0;         // index into the phase table
	};

	/** Alpha of a tick: an upcoming threshold reads, an entered one is dimmed. */
	inline constexpr float UpcomingTickAlpha = 0.95f;
	inline constexpr float PassedTickAlpha = 0.35f;

	inline float TickAlpha(bool bPassed)
	{
		return bPassed ? PassedTickAlpha : UpcomingTickAlpha;
	}

	/** The ticks of a boss bar, into Out (cleared): one per threshold strictly inside (0, 100), in row order. */
	inline void BossPhaseTicks(int32_t Phase, const std::vector<double>& ThresholdsPct, std::vector<FPhaseTick>& Out)
	{
		Out.clear();
		for (std::size_t Row = 0; Row < ThresholdsPct.size(); ++Row)
		{
			const double Pct = ThresholdsPct[Row];
			if (!(Pct > 0.0) || !(Pct < 100.0))
			{
				continue;
			}
			FPhaseTick Tick;
			Tick.Fraction = Pct / 100.0;
			Tick.bPassed = static_cast<int32_t>(Row) < Phase;
			Tick.Row = static_cast<int32_t>(Row);
			Out.push_back(Tick);
		}
	}

	inline std::vector<FPhaseTick> BossPhaseTicks(int32_t Phase, const std::vector<double>& ThresholdsPct)
	{
		std::vector<FPhaseTick> Out;
		BossPhaseTicks(Phase, ThresholdsPct, Out);
		return Out;
	}

	// =================================================================================================================
	// The status row
	// =================================================================================================================

	/** The cell colour of each status (0xRRGGBB, StatusType order). The first six are the web port's (combat-feel 7). */
	inline uint32_t StatusColor(abyss::StatusType Status)
	{
		switch (Status)
		{
		case abyss::StatusType::Burn: return 0xff6633u;
		case abyss::StatusType::Freeze: return 0x66ccffu;
		case abyss::StatusType::Poison: return 0x33cc33u;
		case abyss::StatusType::Bleed: return 0xcc2222u;
		case abyss::StatusType::Slow: return 0x8899ccu;
		case abyss::StatusType::Stun: return 0xffd84au;
		case abyss::StatusType::Ashnumb: return 0x8a8480u;   // AshnumbTint AshColor (vfx_recipes.json dark.materials)
		}
		return 0xffffffu;
	}

	struct FStatusCell
	{
		abyss::StatusType Status = abyss::StatusType::Burn;
		uint32_t Rgb = 0;
		int32_t Stacks = 0;   // > 0: the count drawn on the cell (Ashnumb); 0: none
	};

	inline bool HasStatusBit(uint32_t Mask, abyss::StatusType Status)
	{
		return (Mask & (1u << static_cast<uint32_t>(Status))) != 0u;
	}

	/**
	 * The cells of the status row, into Out (cleared), in StatusType order. Ashnumb shows while its bit is set or the hero
	 * carries stacks (the core sets both; either is enough), with its count (at least 1).
	 */
	inline void StatusCells(uint32_t StatusMask, int32_t AshnumbStacks, std::vector<FStatusCell>& Out)
	{
		Out.clear();
		for (std::size_t Index = 0; Index < abyss::EnumCount<abyss::StatusType>(); ++Index)
		{
			const abyss::StatusType Status = static_cast<abyss::StatusType>(Index);
			const bool bAshnumb = Status == abyss::StatusType::Ashnumb;
			if (!HasStatusBit(StatusMask, Status) && !(bAshnumb && AshnumbStacks > 0))
			{
				continue;
			}
			FStatusCell Cell;
			Cell.Status = Status;
			Cell.Rgb = StatusColor(Status);
			Cell.Stacks = bAshnumb ? (AshnumbStacks > 1 ? AshnumbStacks : 1) : 0;
			Out.push_back(Cell);
		}
	}

	inline std::vector<FStatusCell> StatusCells(uint32_t StatusMask, int32_t AshnumbStacks)
	{
		std::vector<FStatusCell> Out;
		StatusCells(StatusMask, AshnumbStacks, Out);
		return Out;
	}

	/** The status's display-name key (the cell's glyph is its first character): sys.statusEffect.name.<id>. */
	inline std::string StatusNameKey(abyss::StatusType Status)
	{
		return "sys.statusEffect.name." + std::string(abyss::EnumName(Status));
	}

	// =================================================================================================================
	// The permanent-reward toast
	// =================================================================================================================

	struct FRewardLine
	{
		std::string Key;      // ui.permanentReward.<field>: "{n}" is the amount
		int32_t Amount = 0;
	};

	inline constexpr std::string_view RewardLinePrefix = "ui.permanentReward.";
	/** The toast's format: {title} and {rewards} (the lines joined by RewardLineSeparator). */
	inline constexpr std::string_view RewardToastKey = "ui.permanentReward.toast";
	inline constexpr std::string_view RewardLineSeparator = "  ";

	/** The lines of a grant: one per field above 0, in the event's order. */
	inline std::vector<FRewardLine> PermanentRewardLines(const abyss::EvPermanentReward& Event)
	{
		std::vector<FRewardLine> Out;
		const auto Add = [&Out](std::string_view Field, int32_t Amount)
		{
			if (Amount > 0)
			{
				Out.push_back(FRewardLine{ std::string(RewardLinePrefix) + std::string(Field), Amount });
			}
		};
		Add("maxLife", Event.maxLife);
		Add("statPoints", Event.statPoints);
		Add("skillPoints", Event.skillPoints);
		Add("respecTokens", Event.respecTokens);
		return Out;
	}

	/** The toast's title key: the quest's `permanent` name when the locale has it (HasKey), else its name. */
	template <class HasKeyFn>
	std::string PermanentRewardTitleKey(std::string_view QuestId, HasKeyFn&& HasKey)
	{
		std::string Permanent = "data.quest." + std::string(QuestId) + ".permanent";
		if (HasKey(std::string_view(Permanent)))
		{
			return Permanent;
		}
		return "data.quest." + std::string(QuestId) + ".name";
	}

	/** Whether a grant shows a toast at all (a grant of nothing - which the loader refuses - shows none). */
	inline bool ShowsRewardToast(const abyss::EvPermanentReward& Event)
	{
		return !Event.questId.empty() && !PermanentRewardLines(Event).empty();
	}
}
