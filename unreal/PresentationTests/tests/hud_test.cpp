// 2G stage H HUD (Source/Abyssfire/Private/UI/Hud/AbyssHudRules.h) and the hero's Ashnumb presentation
// (Private/World/AbyssWorldStateRules.h: the AshnumbTint contract, the band, the StatusTint fallback, the character
// tints): the boss bar's phase ticks on the real phase tables, the status row with Ashnumb's stacks, the permanent-reward
// toast on the real quests and strings, and Keen reading as a plain Slow. Chapters 2-5: no phase tables, no Ashnumb, the
// six web statuses in today's colours and order.
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "doctest/doctest.h"

#include "abyss/base/Enums.h"
#include "abyss/base/I18n.h"
#include "abyss/base/Json.h"
#include "abyss/data/DataStore.h"
#include "abyss/sim/Events.h"

#include "UI/Hud/AbyssHudRules.h"
#include "World/AbyssWorldStateRules.h"
#include "audio_test_data.h"

namespace
{
	using abyss::StatusType;

	uint32_t Bit(StatusType Status)
	{
		return 1u << static_cast<uint32_t>(Status);
	}

	/** Art/Export/VFX/vfx_recipes.json as committed (Null when it cannot be read). */
	const abyss::JsonValue& VfxRecipes()
	{
		static abyss::JsonValue Root;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			const std::string Text = audio_test::ReadBytes(audio_test::UnrealDir() + "/Art/Export/VFX/vfx_recipes.json");
			if (Text.empty() || !abyss::ParseJson(Text, Root))
			{
				Root = abyss::JsonValue();
			}
		}
		return Root;
	}

	const abyss::JsonValue& AshnumbContract()
	{
		return VfxRecipes().Get("dark").Get("materials").Get("AshnumbTint");
	}

	bool HasBothLocales(std::string_view Key)
	{
		const abyss::I18n& Strings = audio_test::Data().Strings();
		return Strings.Lookup(abyss::LocaleId::ZhCN, Key) != nullptr && Strings.Lookup(abyss::LocaleId::En, Key) != nullptr;
	}

	std::string Str(abyss::LocaleId Locale, std::string_view Key)
	{
		const std::string* Text = audio_test::Data().Strings().Lookup(Locale, Key);
		return Text != nullptr ? *Text : std::string();
	}

	/** The thresholds BossBarView carries for a definition (K4: its phase rows' hpBelowPct, row order). */
	std::vector<double> ThresholdsOf(const abyss::MonsterDef& Def)
	{
		std::vector<double> Out;
		for (const abyss::MonsterPhaseDef& Row : Def.phases)
		{
			Out.push_back(Row.hpBelowPct);
		}
		return Out;
	}
}

TEST_SUITE("hud")
{
	TEST_CASE("boss bar: a tick per phase threshold, dimmed once entered (Valdras 65 % / 30 %)")
	{
		using namespace AbyssHudRules;
		const abyss::MonsterDef* Valdras = audio_test::Data().FindMonster("valdras");
		REQUIRE(Valdras != nullptr);
		const std::vector<double> Thresholds = ThresholdsOf(*Valdras);
		REQUIRE(Thresholds.size() == 2);
		CHECK(Thresholds[0] == 65.0);
		CHECK(Thresholds[1] == 30.0);

		std::vector<FPhaseTick> Ticks = BossPhaseTicks(0, Thresholds);
		REQUIRE(Ticks.size() == 2);
		CHECK(Ticks[0].Fraction == doctest::Approx(0.65));
		CHECK(Ticks[1].Fraction == doctest::Approx(0.30));
		CHECK_FALSE(Ticks[0].bPassed);
		CHECK_FALSE(Ticks[1].bPassed);
		CHECK(Ticks[0].Row == 0);
		CHECK(Ticks[1].Row == 1);
		Ticks = BossPhaseTicks(1, Thresholds);   // Roll Call entered
		CHECK(Ticks[0].bPassed);
		CHECK_FALSE(Ticks[1].bPassed);
		Ticks = BossPhaseTicks(2, Thresholds);   // No Retreat
		CHECK(Ticks[0].bPassed);
		CHECK(Ticks[1].bPassed);
		CHECK(TickAlpha(true) < TickAlpha(false));
		CHECK(TickAlpha(false) > 0.9f);

		// A walk home resets the phase to 0: every tick reads again.
		Ticks = BossPhaseTicks(0, Thresholds);
		CHECK_FALSE(Ticks[0].bPassed);

		// Thresholds with no place on the bar draw nothing; the rows keep their index (dimming follows the row).
		const std::vector<double> Odd = { 100.0, 50.0, 0.0, -5.0, 120.0, 10.0 };
		Ticks = BossPhaseTicks(2, Odd);
		REQUIRE(Ticks.size() == 2);
		CHECK(Ticks[0].Row == 1);
		CHECK(Ticks[0].bPassed);
		CHECK(Ticks[1].Row == 5);
		CHECK_FALSE(Ticks[1].bPassed);

		// The caller's buffer is reused (no growth).
		std::vector<FPhaseTick> Buffer;
		Buffer.reserve(4);
		BossPhaseTicks(1, Thresholds, Buffer);
		CHECK(Buffer.size() == 2);
		CHECK(Buffer.capacity() == 4);
		BossPhaseTicks(1, {}, Buffer);
		CHECK(Buffer.empty());
	}

	TEST_CASE("boss bar: every boss of Chapters 2-5 has no phase table, so its bar has no tick")
	{
		const abyss::DataStore& Data = audio_test::Data();
		int Bosses = 0;
		int WithTicks = 0;
		for (const abyss::BossIntroDef& Intro : Data.Story().bossIntros)
		{
			const abyss::MonsterDef* Def = Data.FindMonster(Intro.monsterId);
			if (Def == nullptr)
			{
				continue;
			}
			++Bosses;
			const bool bTicks = !AbyssHudRules::BossPhaseTicks(0, ThresholdsOf(*Def)).empty();
			CAPTURE(Intro.monsterId);
			CHECK(bTicks == (Intro.monsterId == "valdras"));
			WithTicks += bTicks ? 1 : 0;
		}
		CHECK(Bosses >= 5);
		CHECK(WithTicks == 1);
	}

	TEST_CASE("status row: the six web statuses in today's colours and order; Ashnumb last, with its stacks")
	{
		using namespace AbyssHudRules;
		// Today's StatusColors[6] (SAbyssHudWidgets.cpp before 2G).
		const uint32_t Web[6] = { 0xff6633, 0x66ccff, 0x33cc33, 0xcc2222, 0x8899cc, 0xffd84a };
		std::vector<FStatusCell> Cells = StatusCells(0x3Fu, 0);
		REQUIRE(Cells.size() == 6);
		for (int Index = 0; Index < 6; ++Index)
		{
			CHECK(Cells[static_cast<std::size_t>(Index)].Status == static_cast<StatusType>(Index));
			CHECK(Cells[static_cast<std::size_t>(Index)].Rgb == Web[Index]);
			CHECK(Cells[static_cast<std::size_t>(Index)].Stacks == 0);
		}
		CHECK(StatusCells(0u, 0).empty());

		// Ashnumb (bit 6): the AshnumbTint ash, the stack count.
		Cells = StatusCells(Bit(StatusType::Ashnumb), 3);
		REQUIRE(Cells.size() == 1);
		CHECK(Cells[0].Status == StatusType::Ashnumb);
		CHECK(Cells[0].Rgb == 0x8a8480u);
		CHECK(Cells[0].Stacks == 3);
		// The stacks alone (or the bit alone) are enough; the count is at least 1.
		CHECK(StatusCells(0u, 5).size() == 1);
		CHECK(StatusCells(0u, 5)[0].Stacks == 5);
		CHECK(StatusCells(Bit(StatusType::Ashnumb), 0)[0].Stacks == 1);
		// Ashnumb after the six, poison and Ashnumb together (a breach-blood fight: Cinderbloat's burn, the clot's ash).
		Cells = StatusCells(Bit(StatusType::Burn) | Bit(StatusType::Ashnumb), 2);
		REQUIRE(Cells.size() == 2);
		CHECK(Cells[0].Status == StatusType::Burn);
		CHECK(Cells[1].Status == StatusType::Ashnumb);
		// Every status of the enum has a cell (none is skipped, as the old `Index < 6` loop skipped Ashnumb).
		CHECK(StatusCells(0xFFFFFFFFu, 1).size() == abyss::EnumCount<StatusType>());

		// The cell glyph's name: sys.statusEffect.name.ashnumb, 麻灰 / Ashnumb.
		CHECK(StatusNameKey(StatusType::Ashnumb) == "sys.statusEffect.name.ashnumb");
		CHECK(Str(abyss::LocaleId::ZhCN, StatusNameKey(StatusType::Ashnumb)) == "麻灰");
		CHECK(Str(abyss::LocaleId::En, StatusNameKey(StatusType::Ashnumb)) == "Ashnumb");
		for (std::size_t Index = 0; Index < abyss::EnumCount<StatusType>(); ++Index)
		{
			CAPTURE(Index);
			CHECK(HasBothLocales(StatusNameKey(static_cast<StatusType>(Index))));
		}

		// The row's Ashnumb colour is the contract's ash.
		REQUIRE(AshnumbContract().IsObject());
		CHECK(AbyssWorldStateRules::ParseAshnumbTint(AshnumbContract()).AshRgb == StatusColor(StatusType::Ashnumb));
	}

	TEST_CASE("Keen lands as a plain Slow: its cell is the Slow cell, no number")
	{
		const abyss::MonsterDef* Herd = audio_test::Data().FindMonster("miniboss_corpseherd");
		REQUIRE(Herd != nullptr);
		const abyss::MonsterAbilityDef* Keen = nullptr;
		for (const abyss::MonsterAbilityDef& A : Herd->abilities)
		{
			if (A.id == "keen")
			{
				Keen = &A;
			}
		}
		REQUIRE(Keen != nullptr);
		CHECK(Keen->dmgMul == 0.0);   // status-only: no damage roll, so no number
		REQUIRE(Keen->status.present);
		CHECK(Keen->status.type == StatusType::Slow);
		const std::vector<AbyssHudRules::FStatusCell> Cells = AbyssHudRules::StatusCells(Bit(StatusType::Slow), 0);
		REQUIRE(Cells.size() == 1);
		CHECK(Cells[0].Status == StatusType::Slow);
		CHECK(Cells[0].Rgb == 0x8899ccu);
		CHECK(Cells[0].Stacks == 0);
	}

	TEST_CASE("permanent-reward toast: the title and the lines (S1 Marl's Bitter Draught, S6)")
	{
		using namespace AbyssHudRules;
		const auto HasKey = [](std::string_view Key) { return HasBothLocales(Key); };
		abyss::EvPermanentReward S1;
		S1.questId = "q_rf_black_blood";
		S1.maxLife = 15;
		std::vector<FRewardLine> Lines = PermanentRewardLines(S1);
		REQUIRE(Lines.size() == 1);
		CHECK(Lines[0].Key == "ui.permanentReward.maxLife");
		CHECK(Lines[0].Amount == 15);
		CHECK(PermanentRewardTitleKey(S1.questId, HasKey) == "data.quest.q_rf_black_blood.permanent");
		CHECK(Str(abyss::LocaleId::ZhCN, "data.quest.q_rf_black_blood.permanent") == "玛勒的苦药");
		CHECK(Str(abyss::LocaleId::En, "data.quest.q_rf_black_blood.permanent") == "Marl's Bitter Draught");
		CHECK(ShowsRewardToast(S1));

		// Without a `permanent` name the quest's name titles the toast.
		CHECK(PermanentRewardTitleKey("q_rf_hallowed_ground", [](std::string_view) { return false; })
			== "data.quest.q_rf_hallowed_ground.name");
		CHECK(HasBothLocales("data.quest.q_rf_hallowed_ground.name"));

		// Every field, in the event's order; zero and negative fields draw no line; an empty grant shows no toast.
		abyss::EvPermanentReward All;
		All.questId = "q";
		All.maxLife = 1;
		All.statPoints = 2;
		All.skillPoints = 3;
		All.respecTokens = 4;
		Lines = PermanentRewardLines(All);
		REQUIRE(Lines.size() == 4);
		CHECK(Lines[0].Key == "ui.permanentReward.maxLife");
		CHECK(Lines[1].Key == "ui.permanentReward.statPoints");
		CHECK(Lines[2].Key == "ui.permanentReward.skillPoints");
		CHECK(Lines[3].Key == "ui.permanentReward.respecTokens");
		CHECK(Lines[3].Amount == 4);
		abyss::EvPermanentReward Some;
		Some.questId = "q";
		Some.statPoints = -2;
		Some.skillPoints = 1;
		Lines = PermanentRewardLines(Some);
		REQUIRE(Lines.size() == 1);
		CHECK(Lines[0].Key == "ui.permanentReward.skillPoints");
		abyss::EvPermanentReward None;
		None.questId = "q";
		CHECK_FALSE(ShowsRewardToast(None));
		abyss::EvPermanentReward NoQuest = S1;
		NoQuest.questId.clear();
		CHECK_FALSE(ShowsRewardToast(NoQuest));

		// The strings exist in both locales with their {n} (and the toast's {title} / {rewards}); the real quests that
		// grant a permanent reward each show a toast with a title.
		for (const char* Field : { "maxLife", "statPoints", "skillPoints", "respecTokens" })
		{
			const std::string Key = std::string(RewardLinePrefix) + Field;
			CAPTURE(Key);
			REQUIRE(HasBothLocales(Key));
			CHECK(Str(abyss::LocaleId::ZhCN, Key).find("{n}") != std::string::npos);
			CHECK(Str(abyss::LocaleId::En, Key).find("{n}") != std::string::npos);
		}
		REQUIRE(HasBothLocales(RewardToastKey));
		for (const abyss::LocaleId Locale : { abyss::LocaleId::ZhCN, abyss::LocaleId::En })
		{
			const std::string Toast = Str(Locale, RewardToastKey);
			CHECK(Toast.find("{title}") != std::string::npos);
			CHECK(Toast.find("{rewards}") != std::string::npos);
		}
		int Granting = 0;
		for (const abyss::QuestDef& Quest : audio_test::Data().Quests().quests)
		{
			const abyss::QuestPermanentRewardDef& P = Quest.rewards.permanent;
			if (!P.present)
			{
				continue;
			}
			++Granting;
			abyss::EvPermanentReward Grant;
			Grant.questId = Quest.id;
			Grant.maxLife = P.maxLife;
			Grant.statPoints = P.statPoints;
			Grant.skillPoints = P.skillPoints;
			Grant.respecTokens = P.respecTokens;
			CAPTURE(Quest.id);
			CHECK(ShowsRewardToast(Grant));
			CHECK(HasBothLocales(PermanentRewardTitleKey(Quest.id, HasKey)));
		}
		CHECK(Granting >= 2);   // S1 (max life), S6 (stat points); S7 is Wave 3
	}

	TEST_CASE("the AshnumbTint contract parses from vfx_recipes.json to the shipped values")
	{
		using namespace AbyssWorldStateRules;
		REQUIRE(AshnumbContract().IsObject());
		const FAshnumbTint T = ParseAshnumbTint(AshnumbContract());
		const FAshnumbTint D = DefaultAshnumbTint();
		CHECK(T.bPresent);
		CHECK_FALSE(D.bPresent);
		CHECK(T.AshRgb == D.AshRgb);
		CHECK(T.ShadeRgb == D.ShadeRgb);
		CHECK(T.EdgeRgb == D.EdgeRgb);
		CHECK(T.EdgeWidth == D.EdgeWidth);
		CHECK(T.BandTopBase == D.BandTopBase);
		CHECK(T.BandTopPerStack == D.BandTopPerStack);
		CHECK(T.BandSoftness == D.BandSoftness);
		CHECK(T.MaxBandTop == D.MaxBandTop);
		CHECK(T.Strength == D.Strength);
		CHECK(T.FallbackRgb == D.FallbackRgb);
		CHECK(T.FallbackBase == doctest::Approx(D.FallbackBase));
		CHECK(T.FallbackPerStack == doctest::Approx(D.FallbackPerStack));
		// The shipped numbers (the art review r1's band: one stack to mid-shin, five to the hips).
		CHECK(D.AshRgb == 0x8A8480u);
		CHECK(D.ShadeRgb == 0x4C4944u);
		CHECK(D.EdgeRgb == 0xB8B2A6u);
		CHECK(D.FallbackRgb == 0x8A8480u);
		CHECK(D.FallbackBase == 0.25);
		CHECK(D.FallbackPerStack == 0.05);

		// A partial or malformed contract keeps the shipped values field by field.
		const abyss::JsonValue Partial = audio_test::Json(
			R"({"params": {"AshColor": "#102030", "BandTopBase": 0.2, "MaxBandTop": 7, "Strength": "x"},)"
			R"( "map": "... use the StatusTint path: #405060 multiply at .3 + .02 x stacks"})");
		const FAshnumbTint P = ParseAshnumbTint(Partial);
		CHECK(P.AshRgb == 0x102030u);
		CHECK(P.BandTopBase == 0.2);
		CHECK(P.MaxBandTop == D.MaxBandTop);   // out of range
		CHECK(P.Strength == D.Strength);       // not a number
		CHECK(P.ShadeRgb == D.ShadeRgb);
		CHECK(P.FallbackRgb == 0x405060u);
		CHECK(P.FallbackBase == doctest::Approx(0.3));
		CHECK(P.FallbackPerStack == doctest::Approx(0.02));
		const FAshnumbTint Bad = ParseAshnumbTint(audio_test::Json(R"({"params": {"AshColor": "grey"}, "map": "none"})"));
		CHECK(Bad.AshRgb == D.AshRgb);
		CHECK(Bad.FallbackBase == D.FallbackBase);
		CHECK_FALSE(ParseAshnumbTint(abyss::JsonValue()).bPresent);
	}

	TEST_CASE("the band: its top per stack, the fallback multiply, the tiers that draw it")
	{
		using namespace AbyssWorldStateRules;
		const FAshnumbTint T = ParseAshnumbTint(AshnumbContract());
		CHECK(BandTop(T, 0) == 0.0);
		CHECK(BandTop(T, -2) == 0.0);
		CHECK(BandTop(T, 1) == doctest::Approx(0.22));   // over the boots to mid-shin
		CHECK(BandTop(T, 3) == doctest::Approx(0.42));   // the thighs
		CHECK(BandTop(T, 5) == doctest::Approx(0.62));   // the hips (the core's maxStacks)
		CHECK(BandTop(T, 9) == doctest::Approx(0.62));   // capped
		for (int Stacks = 1; Stacks < 5; ++Stacks)
		{
			CHECK(BandTop(T, Stacks) < BandTop(T, Stacks + 1));
		}
		CHECK(FallbackTintAlpha(T, 0) == 0.0);
		CHECK(FallbackTintAlpha(T, 1) == doctest::Approx(0.30));
		CHECK(FallbackTintAlpha(T, 5) == doctest::Approx(0.50));
		CHECK(FallbackTintAlpha(T, 100) == 1.0);
		// The core's Ashnumb row caps at five stacks: the band's top never needs the cap below MaxBandTop.
		const abyss::StatusStackRule& Row = audio_test::Data().Classes().statusRules.Stacks(StatusType::Ashnumb);
		CHECK(Row.maxStacks == 5);
		CHECK(BandTop(T, Row.maxStacks) <= T.MaxBandTop);
		// Mobile low draws the fallback; every other tier the band.
		CHECK_FALSE(BandAllowed(0, true));
		CHECK(BandAllowed(1, true));
		CHECK(BandAllowed(2, true));
		CHECK(BandAllowed(0, false));
		CHECK(BandAllowed(2, false));
	}

	TEST_CASE("character tints: the six keep their priority; Ashnumb's multiply only where the band is not drawn")
	{
		using namespace AbyssWorldStateRules;
		const FAshnumbTint T = DefaultAshnumbTint();
		// Today's table (AbyssCharacterActor.cpp R8 before 2G), priority order.
		struct FOld
		{
			StatusType Type;
			uint32_t Rgb;
			float Alpha;
		};
		const FOld Old[] = {
			{ StatusType::Freeze, 0x7FD0FF, 0.45f }, { StatusType::Stun, 0x7FD0FF, 0.45f },
			{ StatusType::Burn, 0xFF7A3A, 0.35f }, { StatusType::Poison, 0x8FE04A, 0.35f },
			{ StatusType::Bleed, 0xC8323C, 0.22f }, { StatusType::Slow, 0x9FD8FF, 0.18f },
		};
		// Every combination of the six picks what the old loop picked (and Ashnumb never changes it).
		for (uint32_t Mask = 0; Mask < 64u; ++Mask)
		{
			FTintPick Want;
			for (const FOld& Entry : Old)
			{
				if ((Mask & Bit(Entry.Type)) != 0u)
				{
					Want.bFound = true;
					Want.Rgb = Entry.Rgb;
					Want.Alpha = Entry.Alpha;
					break;
				}
			}
			for (const bool bBand : { false, true })
			{
				const FTintPick Got = CharacterStatusTint(Mask, 0, bBand, T);
				CAPTURE(Mask);
				CHECK(Got.bFound == Want.bFound);
				CHECK(Got.Rgb == Want.Rgb);
				CHECK(Got.Alpha == Want.Alpha);
				if (Want.bFound)
				{
					const FTintPick WithAsh = CharacterStatusTint(Mask | Bit(StatusType::Ashnumb), 4, bBand, T);
					CHECK(WithAsh.Rgb == Want.Rgb);
					CHECK(WithAsh.Alpha == Want.Alpha);
				}
			}
		}
		// Ashnumb alone: the band is the read (no tint) where it is drawn, the multiply elsewhere.
		CHECK_FALSE(CharacterStatusTint(Bit(StatusType::Ashnumb), 3, /*bBandShown*/ true, T).bFound);
		const FTintPick Fallback = CharacterStatusTint(Bit(StatusType::Ashnumb), 3, /*bBandShown*/ false, T);
		CHECK(Fallback.bFound);
		CHECK(Fallback.Rgb == 0x8A8480u);
		CHECK(Fallback.Alpha == doctest::Approx(0.40f));
		CHECK(CharacterStatusTint(0u, 2, false, T).Alpha == doctest::Approx(0.35f));
		CHECK(CharacterStatusTint(Bit(StatusType::Ashnumb), 0, false, T).Alpha == doctest::Approx(0.30f));
		CHECK_FALSE(CharacterStatusTint(0u, 0, false, T).bFound);
	}
}
