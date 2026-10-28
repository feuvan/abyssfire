// The character actor's action map (Source/Abyssfire/Private/Actors/AbyssAnimActionRules.h): K1 monster actions to clips,
// blend states, holds, and the layer priority death > action > hold > stun > move > idle.
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

#include "doctest/doctest.h"

#include "abyss/base/Enums.h"
#include "abyss/combat/HitFeedback.h"
#include "abyss/data/DataStore.h"
#include "abyss/monsters/MonsterAbilities.h"
#include "abyss/sim/SimTypes.h"

#include "Actors/AbyssAnimActionRules.h"
#include "audio_test_data.h"

TEST_SUITE("anim")
{
	using namespace AbyssAnimActionRules;
	using abyss::AnimAction;

	TEST_CASE("every AnimAction has a kind; the K1 actions play the clip of the same name")
	{
		struct FRow
		{
			AnimAction Action;
			EActionKind Kind;
			std::string_view Clip;
			const char* Blend;
		};
		const FRow Rows[] = {
			{ AnimAction::Idle, EActionKind::None, "", "cast" },
			{ AnimAction::Walk, EActionKind::None, "", "cast" },
			{ AnimAction::Attack, EActionKind::Attack, "", "attack" },
			{ AnimAction::Cast, EActionKind::Cast, "", "cast" },
			{ AnimAction::Hurt, EActionKind::Hurt, "", "hurt" },
			{ AnimAction::Dodge, EActionKind::Dodge, "", "dodge" },
			{ AnimAction::Death, EActionKind::Death, "", "cast" },
			{ AnimAction::Signature, EActionKind::Signature, "", "cast" },
			{ AnimAction::Work, EActionKind::Work, "", "cast" },
			{ AnimAction::Talk, EActionKind::Talk, "", "cast" },
			{ AnimAction::Roar, EActionKind::Emote, "Roar", "cast" },
			{ AnimAction::Taunt, EActionKind::Emote, "Taunt", "cast" },
			{ AnimAction::Stun, EActionKind::Emote, "Stun", "cast" },
			{ AnimAction::Attack02, EActionKind::Attack, "Attack02", "attack" },
			{ AnimAction::Cast02, EActionKind::Cast, "Cast02", "cast" },
			{ AnimAction::Rise, EActionKind::Emote, "Rise", "cast" },
		};
		// The table covers the whole enum (a new AnimAction must be mapped here and in the actor).
		CHECK(sizeof(Rows) / sizeof(Rows[0]) == abyss::EnumCount<AnimAction>());
		for (const FRow& Row : Rows)
		{
			CAPTURE(abyss::EnumName(Row.Action));
			CHECK(KindOf(Row.Action) == Row.Kind);
			CHECK(NamedClip(Row.Action) == Row.Clip);
			CHECK(std::string_view(BlendState(KindOf(Row.Action))) == Row.Blend);
		}
		// The core's spellings (story focus `anim`, AutoPilot) name the same clips, capitalised.
		CHECK(abyss::EnumName(AnimAction::Roar) == "roar");
		CHECK(abyss::EnumName(AnimAction::Attack02) == "attack02");
	}

	TEST_CASE("holds: Taunt and Stun loop; Idle / Walk / Death are never holds")
	{
		CHECK(IsHoldAction(AnimAction::Taunt));
		CHECK(IsHoldAction(AnimAction::Stun));
		for (const AnimAction Action : { AnimAction::Idle, AnimAction::Walk, AnimAction::Death, AnimAction::Roar,
				 AnimAction::Rise, AnimAction::Attack, AnimAction::Attack02, AnimAction::Cast02 })
		{
			CAPTURE(abyss::EnumName(Action));
			CHECK_FALSE(IsHoldAction(Action));
			CHECK_FALSE(ForcesLoop(Action));
		}
		CHECK(ForcesLoop(AnimAction::Taunt));   // the goblin shaman's Taunt clip is not authored as a loop
		// Beat-less one-shots stretch to the event's duration; attacks keep the contact rate; loops keep their rate.
		CHECK(StretchesToDuration(AnimAction::Roar));
		CHECK(StretchesToDuration(AnimAction::Rise));
		CHECK(StretchesToDuration(AnimAction::Cast02));
		CHECK_FALSE(StretchesToDuration(AnimAction::Attack));
		CHECK_FALSE(StretchesToDuration(AnimAction::Attack02));
		CHECK_FALSE(StretchesToDuration(AnimAction::Stun));
		CHECK_FALSE(StretchesToDuration(AnimAction::Taunt));
	}

	TEST_CASE("layer priority: death > action > hold > stun > move > idle")
	{
		FLayerInputs In;
		CHECK(PickLayer(In) == ELayer::Idle);
		In.bMoving = true;
		CHECK(PickLayer(In) == ELayer::Move);
		In.bStunClip = true;
		CHECK(PickLayer(In) == ELayer::Stun);
		In.bHoldClip = true;
		CHECK(PickLayer(In) == ELayer::Hold);
		In.bInAction = true;
		CHECK(PickLayer(In) == ELayer::Action);
		In.bDying = true;
		CHECK(PickLayer(In) == ELayer::Death);   // a hold never suppresses a death
		FLayerInputs HoldOnly;
		HoldOnly.bHoldClip = true;
		HoldOnly.bDying = true;
		CHECK(PickLayer(HoldOnly) == ELayer::Death);
		FLayerInputs NoClip;   // a hold without its clip keeps the current pose: locomotion as before
		NoClip.bMoving = false;
		CHECK(PickLayer(NoClip) == ELayer::Idle);
	}

	TEST_CASE("play rates: the beat on the contact, clamped; a loop that the windup outlasts loops at rate 1")
	{
		FRateInput In;
		In.Action = AnimAction::Attack02;
		In.bHasClip = true;
		In.BeatSec = 0.7f;
		In.LengthSec = 1.3f;
		In.ContactMs = 700.0;
		In.DurationMs = 1300.0;
		FRatePlan P = ActionPlayRate(In);
		CHECK(P.Rate == doctest::Approx(1.0));
		CHECK_FALSE(P.bLoopThrough);
		CHECK(P.StartPhaseSec == 0.f);
		CHECK(P.DurationSec == 0.f);
		In.ContactMs = 1400.0;   // a slower windup: half speed
		CHECK(ActionPlayRate(In).Rate == doctest::Approx(0.5));
		In.ContactMs = 100.0;    // 7x: clamped
		CHECK(ActionPlayRate(In).Rate == doctest::Approx(MaxPlayRate));
		In.ContactMs = 7000.0;   // a one-shot is never looped, only clamped
		CHECK(ActionPlayRate(In).Rate == doctest::Approx(MinPlayRate));
		CHECK_FALSE(ActionPlayRate(In).bLoopThrough);

		// The drummer's war_drum: Cast01 is a 1000 ms loop, Release 125 ms, the call's windup 2000 ms.
		FRateInput Drum;
		Drum.Action = AnimAction::Cast;
		Drum.bHasClip = true;
		Drum.BeatSec = 0.125f;
		Drum.LengthSec = 1.f;
		Drum.bLoopClip = true;
		Drum.ContactMs = 2000.0;
		Drum.DurationMs = 16000.0;   // the core's: the whole clip at the slowed rate (0.0625)
		Drum.EventRate = 0.0625;
		const FRatePlan D = ActionPlayRate(Drum);
		CHECK(D.Rate == 1.f);
		CHECK(D.bLoopThrough);
		CHECK(D.StartPhaseSec == doctest::Approx(0.125));
		CHECK(D.DurationSec == doctest::Approx(2.875));   // the contact, then the rest of the beat's pass
		// A drum beat lands on the contact (and every loop before it).
		CHECK(PlanClipTime(D, 2.f, Drum.LengthSec) == doctest::Approx(0.125).epsilon(1e-4));
		CHECK(PlanClipTime(D, 1.f, Drum.LengthSec) == doctest::Approx(0.125).epsilon(1e-4));
		CHECK(PlanClipTime(D, 0.5f, Drum.LengthSec) == doctest::Approx(0.625));
		// A loop whose beat the windup does not outlast keeps the beat rate.
		Drum.ContactMs = 500.0;
		CHECK(ActionPlayRate(Drum).Rate == doctest::Approx(0.25));
		CHECK_FALSE(ActionPlayRate(Drum).bLoopThrough);

		// No beat: Consume (Attack + clip, loop, 2000 ms channel) keeps the event's rate 1 (its loop flag loops it).
		FRateInput Consume;
		Consume.Action = AnimAction::Attack;
		Consume.bHasClip = true;
		Consume.LengthSec = 1.f;
		Consume.bLoopClip = true;
		Consume.DurationMs = 2000.0;
		CHECK(ActionPlayRate(Consume).Rate == 1.f);
		CHECK_FALSE(ActionPlayRate(Consume).bLoopThrough);
		// No beat, a stretching action: the clip fills the duration (Rise 1500 ms clip over 1200 ms).
		FRateInput Rise;
		Rise.Action = AnimAction::Rise;
		Rise.bHasClip = true;
		Rise.LengthSec = 1.5f;
		Rise.DurationMs = 1200.0;
		CHECK(ActionPlayRate(Rise).Rate == doctest::Approx(1.25));
		// No clip: the event's rate as is.
		FRateInput None;
		None.EventRate = 0.4;
		CHECK(ActionPlayRate(None).Rate == doctest::Approx(0.4));
		None.EventRate = 0.0;
		CHECK(ActionPlayRate(None).Rate == 1.f);
	}

	TEST_CASE("every Chapter 1 ability clip exists on its art and lands its beat on the core's contact (or loops)")
	{
		const abyss::DataStore& Data = audio_test::Data();
		const abyss::AssetManifest& Manifest = Data.Assets();
		REQUIRE(Manifest.loaded);
		struct FCaster
		{
			const abyss::MonsterDef* Def;
			std::vector<abyss::MonsterAbilityDef> Abilities;
			abyss::TelegraphStyle Style;
		};
		std::vector<FCaster> Casters;
		for (const abyss::MonsterDef& Def : Data.Monsters().defs)
		{
			if (!Def.abilities.empty())
			{
				const bool bBoss = Def.id == "valdras" || Def.id == "miniboss_corpseherd";
				Casters.push_back({ &Def, Def.abilities, bBoss ? abyss::TelegraphStyle::Boss : abyss::TelegraphStyle::Normal });
			}
		}
		for (const abyss::HuntDef& Hunt : Data.Monsters().hunts)
		{
			if (!Hunt.abilities.empty())
			{
				Casters.push_back({ &Hunt.defNormal, Hunt.abilities, abyss::TelegraphStyle::Elite });
			}
		}
		REQUIRE(Casters.size() >= 7);
		int Checked = 0;
		int Looped = 0;
		for (const FCaster& Caster : Casters)
		{
			const abyss::MonsterDef& Def = *Caster.Def;
			const abyss::AssetEntryDef* Art = Manifest.FindByGameId(abyss::MonsterArtGameId(Manifest, Def.spriteKey, Def.id));
			CAPTURE(Def.id);
			REQUIRE(Art != nullptr);
			for (const abyss::MonsterAbilityDef& A : Caster.Abilities)
			{
				CAPTURE(A.id);
				if (A.clip.empty())
				{
					// Clipless (Breach-seep, Hungerbell's supper bell): no EvPlayAnim, the decal / call is the tell.
					CHECK((A.kind == abyss::AbilityKind::Hazard || A.kind == abyss::AbilityKind::Call));
					continue;
				}
				const abyss::AnimClipDef* Clip = Art->FindClip(A.clip);
				REQUIRE_MESSAGE(Clip != nullptr, A.clip);
				const abyss::AbilityTiming Timing = abyss::ComputeAbilityTiming(A, Clip,
					abyss::AbilityMinWindupMs(Caster.Style), Data.Combat().anim, Def.animCategory);
				const AnimAction Action = abyss::AbilityAnimAction(A.clip);
				// The actor plays the clip of that name (K1 actions by name; Attack / Cast with the explicit clip).
				if (Action == AnimAction::Attack02 || Action == AnimAction::Cast02 || Action == AnimAction::Roar)
				{
					CHECK(NamedClip(Action) == A.clip);
				}
				FRateInput In;
				In.Action = Action;
				In.bHasClip = true;
				const double BeatMs = Clip->hasContactMs && Clip->contactMs > 0 ? Clip->contactMs
					: (Clip->hasReleaseMs ? Clip->releaseMs : -1.0);
				In.BeatSec = static_cast<float>(BeatMs / 1000.0);
				In.LengthSec = static_cast<float>(Clip->lengthMs / 1000.0);
				In.bLoopClip = Clip->loop;
				In.ContactMs = Timing.contactMs;
				In.DurationMs = Timing.durationMs;
				In.EventRate = Timing.playRate;
				REQUIRE(BeatMs > 0.0);
				const FRatePlan Plan = ActionPlayRate(In);
				if (Plan.bLoopThrough)
				{
					// The beat of the loop falls on the contact.
					const float T = PlanClipTime(Plan, static_cast<float>(Timing.contactMs / 1000.0), In.LengthSec);
					CHECK(std::fabs(T - In.BeatSec) * 1000.f <= 1.f);
					CHECK(Plan.Rate == 1.f);
					++Looped;
				}
				else
				{
					// Unclamped: the beat lands on the contact within 1 ms.
					CHECK(Plan.Rate >= MinPlayRate);
					CHECK(Plan.Rate <= MaxPlayRate);
					CHECK(std::fabs(BeatMs / Plan.Rate - Timing.contactMs) <= 1.0);
				}
				++Checked;
			}
		}
		CHECK(Checked >= 10);
		CHECK(Looped == 1);   // the drum
		// The loop rule changes no other clip: the drummer's Cast01 is the only looping clip with a beat (heroes, NPCs and
		// the web's monsters keep their rates).
		int LoopsWithBeat = 0;
		for (const abyss::AssetEntryDef& Asset : Manifest.assets)
		{
			for (const abyss::AnimClipDef& Clip : Asset.anims)
			{
				if (Clip.loop && ((Clip.hasContactMs && Clip.contactMs > 0) || (Clip.hasReleaseMs && Clip.releaseMs > 0)))
				{
					++LoopsWithBeat;
					CHECK(Asset.name == "SK_Mon_DeserterDrummer");
					CHECK(Clip.name == "Cast01");
				}
			}
		}
		CHECK(LoopsWithBeat == 1);
		// The real beats (art manifest) the plan was written against.
		auto Beat = [&Manifest](const char* Game, const char* ClipName)
		{
			const abyss::AssetEntryDef* Art = Manifest.FindByGameId(Game);
			const abyss::AnimClipDef* Clip = Art != nullptr ? Art->FindClip(ClipName) : nullptr;
			return Clip == nullptr ? -1.0 : (Clip->hasContactMs ? Clip->contactMs : Clip->releaseMs);
		};
		CHECK(Beat("monster_valdras", "Attack02") == 700.0);
		CHECK(Beat("monster_valdras", "Cast01") == 900.0);
		CHECK(Beat("monster_valdras", "Lunge") == 800.0);
		CHECK(Beat("monster_miniboss_corpseherd", "Cast01") == 650.0);
		CHECK(Beat("monster_miniboss_corpseherd", "Cast02") == 800.0);
		CHECK(Beat("monster_bonegorger", "Attack02") == 800.0);
		CHECK(Beat("monster_dead_standard_bearer", "Cast01") == 900.0);
		CHECK(Beat("monster_deserter_drummer", "Cast01") == 125.0);
	}

	TEST_CASE("death runtime: the Cinderbloat's fuse plays its Death at rate 1 for 900 ms, standing; others as today")
	{
		const std::string Text = audio_test::ReadBytes(audio_test::UnrealDir() + "/Data/assets.json");
		abyss::JsonValue Root;
		REQUIRE(abyss::ParseJson(Text, Root));
		const abyss::JsonValue& Assets = Root.Get("manifest").Get("assets");
		REQUIRE(Assets.IsObject());
		const FDeathRuntime Bloat = ParseDeathRuntime(Assets.Get("SK_Mon_Cinderbloat"));
		CHECK(Bloat.bValid);
		CHECK(Bloat.DeathDurationMs == 900.0);
		CHECK(Bloat.PlayRate == 1.0);
		CHECK(Bloat.BodyFadeNotBeforeMs == 900.0);
		CHECK_FALSE(Bloat.bThrow);
		CHECK_FALSE(Bloat.bHop);
		CHECK_FALSE(Bloat.bSpin);
		// FX_Burst (@800 ms of the clip) at rate 1 lands on the core's 800 ms burst (the fuse's EvTelegraph contact).
		const abyss::MonsterDef* Def = audio_test::Data().FindMonster("cinderbloat");
		REQUIRE(Def != nullptr);
		CHECK(Def->onDeath.delayMs == 800.0);
		bool bBurst = false;
		for (const abyss::JsonValue& Clip : Assets.Get("SK_Mon_Cinderbloat").Get("anims").Items())
		{
			if (Clip.Get("name").AsString() != "Death")
			{
				continue;
			}
			CHECK(Clip.Get("lengthMs").AsDouble() == Bloat.DeathDurationMs);
			for (const abyss::JsonValue& Notify : Clip.Get("notifies").Items())
			{
				if (Notify.Get("name").AsString() == "FX_Burst")
				{
					bBurst = true;
					CHECK(Notify.Get("ms").AsDouble() / Bloat.PlayRate == Def->onDeath.delayMs);
				}
			}
		}
		CHECK(bBurst);
		// The trimmed form (GetAssetJson): a clip's runtime alone.
		const FDeathRuntime Trimmed = ParseDeathRuntime(audio_test::Json(
			R"({"clips": {"Death": {"runtime": {"deathDurationMs": 700, "deathSpin": false}}}})"));
		CHECK(Trimmed.bValid);
		CHECK(Trimmed.DeathDurationMs == 700.0);
		CHECK(Trimmed.PlayRate == 1.0);
		CHECK(Trimmed.bThrow);
		CHECK_FALSE(Trimmed.bSpin);
		CHECK_FALSE(ParseDeathRuntime(audio_test::Json(R"({"fx": {"fuse": {"runtime": {"deathDurationMs": 0}}}})")).bValid);
		// Every other asset keeps today's death: only the Cinderbloat carries the block.
		int WithRuntime = 0;
		for (const abyss::JsonMember& Asset : Assets.Members())
		{
			if (ParseDeathRuntime(Asset.value).bValid)
			{
				++WithRuntime;
				CHECK(Asset.key == "SK_Mon_Cinderbloat");
			}
		}
		CHECK(WithRuntime == 1);
	}
}
