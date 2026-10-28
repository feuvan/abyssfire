// The corpse layer (Source/Abyssfire/Private/Actors/AbyssCorpseRules.h, dark remake U3 / 2G): the M_AF_AshDissolve
// contract and every roster corpse block of the real art manifest (Data/assets.json) against the real data (which
// definitions leave an entry, the story beats that hold a body, the recipes in vfx_recipes.json), then the ash timeline,
// the story hold, every removal, the Feed, the rise reversal, the burn colour, the reconcile and the visible cap on
// synthetic numbers.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "doctest/doctest.h"

#include "abyss/base/Enums.h"
#include "abyss/base/Json.h"
#include "abyss/base/Types.h"
#include "abyss/data/DataStore.h"
#include "abyss/data/MonsterData.h"
#include "abyss/data/StoryData.h"

#include "Actors/AbyssCorpseRules.h"
#include "Actors/AbyssMonsterLookRules.h"
#include "audio_test_data.h"

namespace
{
	using namespace AbyssCorpseRules;

	const abyss::JsonValue& ArtManifest()
	{
		static abyss::JsonValue Root;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			const std::string Text = audio_test::ReadBytes(audio_test::UnrealDir() + "/Data/assets.json");
			if (Text.empty() || !abyss::ParseJson(Text, Root))
			{
				Root = abyss::JsonValue();
			}
		}
		const abyss::JsonValue* Manifest = Root.Find("manifest");
		return Manifest != nullptr ? *Manifest : Root;
	}

	const abyss::JsonValue& Recipes()
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

	bool HasRecipe(std::string_view Id)
	{
		return Recipes().Get("recipes").Has(Id) || Recipes().Get("aliases").Has(Id);
	}

	const abyss::DataStore& Data() { return audio_test::Data(); }

	/** The asset listing a game id (the first in name order), or Null. */
	const abyss::JsonValue& AssetByGameId(std::string_view GameId, std::string* OutName = nullptr)
	{
		static const abyss::JsonValue Null;
		for (const abyss::JsonMember& Member : ArtManifest().Get("assets").Members())
		{
			for (const abyss::JsonValue& Id : Member.value.Get("gameIds").Items())
			{
				if (Id.AsString() == GameId)
				{
					if (OutName != nullptr)
					{
						*OutName = Member.key;
					}
					return Member.value;
				}
			}
		}
		return Null;
	}

	double ClipLengthMs(const abyss::JsonValue& Asset, std::string_view Clip)
	{
		for (const abyss::JsonValue& Anim : Asset.Get("anims").Items())
		{
			if (Anim.Get("name").AsString() == Clip)
			{
				return Anim.Get("lengthMs").AsDouble(0.0);
			}
		}
		return -1.0;
	}

	FAshContract Contract() { return ParseAshContract(ArtManifest().Get("materials")); }

	FTimeline SyntheticTimeline()
	{
		FTimeline T;   // the contract's numbers
		T.HoldMs = 450.0;
		T.GreyMs = 380.0;
		T.CrumbleMs = 900.0;
		T.CrumbleTo = 0.38;
		T.ExpireMs = 700.0;
		return T;
	}

	/** Every definition (zone tables and hunts) whose body leaves a registry entry. */
	std::vector<const abyss::MonsterDef*> EntryDefs()
	{
		std::vector<const abyss::MonsterDef*> Out;
		for (const abyss::MonsterDef& Def : Data().Monsters().defs)
		{
			if (Def.corpse.leaves != abyss::CorpseLeaves::None)
			{
				Out.push_back(&Def);
			}
		}
		for (const abyss::HuntDef& Hunt : Data().Monsters().hunts)
		{
			if (Hunt.defNormal.corpse.leaves != abyss::CorpseLeaves::None)
			{
				Out.push_back(&Hunt.defNormal);
			}
		}
		return Out;
	}
}

TEST_SUITE("corpse")
{
	TEST_CASE("the M_AF_AshDissolve contract comes from the manifest; defaults are the contract as shipped")
	{
		const FAshContract C = Contract();
		REQUIRE(C.bFromManifest);
		CHECK(C.AshRgb == 0x6E6A62u);
		CHECK(C.ShadeRgb == 0x3E3B37u);
		CHECK(C.EdgeRgb == 0xB8B2A6u);
		CHECK(C.EdgeWidth == doctest::Approx(0.06));
		CHECK(C.bEmissiveOff);
		CHECK(C.NoiseTileCm == doctest::Approx(60.0));
		CHECK(C.NoiseTexture == "T_FX_AshNoise");
		CHECK(C.Timeline.HoldMs == doctest::Approx(450.0));
		CHECK(C.Timeline.GreyMs == doctest::Approx(380.0));
		CHECK(C.Timeline.CrumbleMs == doctest::Approx(900.0));
		CHECK(C.Timeline.CrumbleTo == doctest::Approx(0.38));
		CHECK(C.Timeline.ExpireMs == doctest::Approx(700.0));
		CHECK(C.RiseFraction == doctest::Approx(0.6));
		// the built-in copy is the same contract (a manifest without materials.M_AF_AshDissolve)
		const FAshContract Built = ParseAshContract(abyss::JsonValue());
		CHECK_FALSE(Built.bFromManifest);
		CHECK(Built.AshRgb == C.AshRgb);
		CHECK(Built.ShadeRgb == C.ShadeRgb);
		CHECK(Built.EdgeRgb == C.EdgeRgb);
		CHECK(Built.EdgeWidth == doctest::Approx(C.EdgeWidth));
		CHECK(Built.NoiseTileCm == doctest::Approx(C.NoiseTileCm));
		CHECK(Built.Timeline.CrumbleTo == doctest::Approx(C.Timeline.CrumbleTo));
		// a malformed colour keeps the default
		const abyss::JsonValue Bad = audio_test::Json(
			R"({"M_AF_AshDissolve": {"params": {"AshColor": "grey", "EdgeWidth": 3}, "timeline": {"greyMs": -5}}})");
		const FAshContract B = ParseAshContract(Bad);
		CHECK(B.bFromManifest);
		CHECK(B.AshRgb == 0x6E6A62u);
		CHECK(B.EdgeWidth == doctest::Approx(0.06));
		CHECK(B.Timeline.GreyMs == doctest::Approx(380.0));
	}

	TEST_CASE("every roster corpse block: registry bodies draw, puddles do not; clips, recipes, story holds, rises")
	{
		const FAshContract C = Contract();
		int Blocks = 0;
		int Drawn = 0;
		std::set<std::string> StoryBeats;
		for (const abyss::JsonMember& Member : ArtManifest().Get("assets").Members())
		{
			const abyss::JsonValue& Asset = Member.value;
			if (!Asset.Get("corpse").IsObject())
			{
				continue;
			}
			++Blocks;
			CAPTURE(Member.key);
			const FCorpseBlock B = ParseCorpseBlock(Asset, C);
			const bool bRegistry = Asset.Get("corpse").Get("registry").AsBool(false);
			CHECK(B.bValid == bRegistry);
			if (!bRegistry)
			{
				CHECK(B.Look == ELook::Puddle);   // breach-blood: drawn by the death, not this layer
				continue;
			}
			++Drawn;
			CHECK((B.Look == ELook::Husk || B.Look == ELook::Carcass));
			// the hold frame is the Death clip's end at rate 1
			CHECK(B.DeathClip == "Death");
			CHECK(B.HoldFromMs == doctest::Approx(ClipLengthMs(Asset, B.DeathClip)));
			CHECK(B.Timeline.HoldMs == doctest::Approx(C.Timeline.HoldMs));
			CHECK(B.Timeline.GreyMs == doctest::Approx(C.Timeline.GreyMs));
			CHECK(B.Timeline.CrumbleTo == doctest::Approx(C.Timeline.CrumbleTo));
			CHECK(B.Timeline.TtlMs == doctest::Approx(20000.0));
			CHECK(B.HuskTopCm > 0.0);
			for (const std::string& Id : { B.BurnRecipe, B.CrumbleRecipe, B.ExpireRecipe })
			{
				CHECK_MESSAGE(HasRecipe(Id), Id);
			}
			if (B.Look == ELook::Carcass)
			{
				CHECK_FALSE(B.bRaisable);
				CHECK_FALSE(B.bConsumable);
			}
			if (B.bHasRise)
			{
				CHECK(B.RiseLengthMs == doctest::Approx(ClipLengthMs(Asset, B.RiseClip)));
			}
			if (!B.RaisedAs.empty())
			{
				CHECK_FALSE(AssetByGameId(B.RaisedAs).IsNull());
			}
			if (B.bStoryHold)
			{
				StoryBeats.insert(B.StoryBeat);
			}
		}
		CHECK(Blocks == 13);
		CHECK(Drawn == 11);
		// only the two story bodies wait for their beat, and each beat is the one their kill triggers
		CHECK(StoryBeats == std::set<std::string>{ "cs_rf_herd", "cs_rf_marshal_falls" });
		for (const std::string& Beat : StoryBeats)
		{
			CAPTURE(Beat);
			CHECK(Data().Story().FindCutscene(Beat) != nullptr);
			bool bKilledTrigger = false;
			for (const abyss::StoryTriggerDef& T : Data().Story().triggers)
			{
				bKilledTrigger = bKilledTrigger || (T.cutscene == Beat && T.on == abyss::StoryTriggerOn::MonsterKilled);
			}
			CHECK(bKilledTrigger);
		}
		const FCorpseBlock Herd = ParseCorpseBlock(AssetByGameId("miniboss_corpseherd"), C);
		CHECK(Herd.StoryBeat == "cs_rf_herd");
		CHECK(Herd.HoldFromMs == doctest::Approx(1400.0));
		const FCorpseBlock Valdras = ParseCorpseBlock(AssetByGameId("monster_valdras"), C);
		CHECK(Valdras.StoryBeat == "cs_rf_marshal_falls");
		CHECK(Valdras.HoldFromMs == doctest::Approx(2000.0));
		CHECK_FALSE(Valdras.bRaisable);
		// the rise reversal's recipe (monster.raise) and the consume channel's exist
		CHECK(HasRecipe(Herd.RaiseRecipe));
		CHECK(HasRecipe(Herd.ConsumeRecipe));
	}

	TEST_CASE("every definition that leaves an entry has art that draws it; chapters 2-5 leave none")
	{
		const FAshContract C = Contract();
		const std::vector<const abyss::MonsterDef*> Defs = EntryDefs();
		CHECK(Defs.size() >= 12u);
		for (const abyss::MonsterDef* Def : Defs)
		{
			CAPTURE(Def->id);
			std::string Name;
			const abyss::JsonValue& Asset = AssetByGameId(Def->spriteKey, &Name);
			REQUIRE_FALSE(Asset.IsNull());
			const FCorpseBlock B = ParseCorpseBlock(Asset, C);
			CHECK(B.bValid);
			CHECK(HoldsCorpsePose(Def->corpse.leaves, B, true));
			CHECK_FALSE(HoldsCorpsePose(Def->corpse.leaves, B, false));   // no ash masters: today's fade
			// remains are the carcasses and the boss bodies: nothing raises or eats them
			if (Def->corpse.leaves == abyss::CorpseLeaves::Remains)
			{
				CHECK_FALSE(B.bRaisable);
			}
			// only Chapter 1 families
			CHECK(std::set<std::string>{ "unburied", "living", "gravegnawers" }.count(Def->family) == 1);
		}
		// breach-blood leaves no entry (its tar puddle is no corpse); every other definition (chapters 2-5, the
		// Labyrinth's) neither: their deaths stay today's fade
		for (const abyss::MonsterDef& Def : Data().Monsters().defs)
		{
			if (Def.family.empty())
			{
				CAPTURE(Def.id);
				CHECK(Def.corpse.leaves == abyss::CorpseLeaves::None);
				CHECK_FALSE(HoldsCorpsePose(Def.corpse.leaves, ParseCorpseBlock(AssetByGameId(Def.spriteKey), C), true));
			}
		}
	}

	TEST_CASE("the ash timeline: fall, hold, grey, crumble by plates to crumbleTo, husk")
	{
		const FTimeline T = SyntheticTimeline();
		const FHuskState S = MakeHusk(1000.0, 900.0, false, false);
		CHECK(S.HoldStartMs == doctest::Approx(1900.0));
		auto At = [&](double Ms) { return Evaluate(S, T, Ms); };
		CHECK(At(1000.0).Phase == EPhase::Falling);
		CHECK(At(1899.0).Phase == EPhase::Falling);
		CHECK(At(1899.0).Grey == 0.0);
		CHECK(At(1900.0).Phase == EPhase::Hold);
		CHECK(At(2349.0).Phase == EPhase::Hold);
		CHECK(At(2349.0).Grey == 0.0);
		// grey 2350 .. 2730
		CHECK(At(2350.0).Phase == EPhase::Grey);
		CHECK(At(2540.0).Grey == doctest::Approx(0.5));
		CHECK(At(2540.0).Dissolve == 0.0);
		CHECK(At(2730.0).Grey == doctest::Approx(1.0));
		// crumble 2730 .. 3630 to .38 (corpse.crumble at its start)
		CHECK(CrumbleStartMs(S, T) == doctest::Approx(2730.0));
		CHECK(At(2730.0).Phase == EPhase::Crumble);
		CHECK(At(3180.0).Dissolve == doctest::Approx(0.19));
		CHECK(At(3180.0).ShadowAlpha == doctest::Approx(1.0 - 0.5 * (1.0 - HuskShadowAlpha)));
		CHECK(At(3630.0).Phase == EPhase::Husk);
		CHECK(At(3630.0).Dissolve == doctest::Approx(0.38));
		CHECK(At(21000.0).Phase == EPhase::Husk);
		CHECK(At(21000.0).Dissolve == doctest::Approx(0.38));
		CHECK(At(21000.0).ShadowAlpha == doctest::Approx(HuskShadowAlpha));
		CHECK_FALSE(At(21000.0).bDone);
		// monotone in time
		double LastGrey = 0.0, LastDissolve = 0.0;
		for (double Ms = 1000.0; Ms <= 4000.0; Ms += 16.0)
		{
			const FAshLook L = At(Ms);
			CHECK(L.Grey >= LastGrey - 1e-12);
			CHECK(L.Dissolve >= LastDissolve - 1e-12);
			LastGrey = L.Grey;
			LastDissolve = L.Dissolve;
		}
		// a seed (or a late view) is born a husk
		const FHuskState Seed = MakeHusk(500.0, 900.0, true, true);
		CHECK_FALSE(Seed.bStoryHeld);
		CHECK(Evaluate(Seed, T, 500.0).Phase == EPhase::Husk);
		CHECK(Evaluate(Seed, T, 500.0).Grey == 1.0);
		CHECK(Evaluate(Seed, T, 500.0).Dissolve == doctest::Approx(0.38));
		CHECK(std::isinf(CrumbleStartMs(Seed, T)));
	}

	TEST_CASE("the story hold keeps the pose while its beat plays or waits, then the hold starts again")
	{
		const FAshContract C = Contract();
		const FCorpseBlock Herd = ParseCorpseBlock(AssetByGameId("miniboss_corpseherd"), C);
		const FCorpseBlock Footman = ParseCorpseBlock(AssetByGameId("monster_risen_footman"), C);
		// the decision
		CHECK(StoryHoldActive(Herd, "cs_rf_herd", true, false));
		CHECK(StoryHoldActive(Herd, "cs_rf_herd", false, false));
		CHECK(StoryHoldActive(Herd, "", true, false));                 // queued behind another beat
		CHECK_FALSE(StoryHoldActive(Herd, "", true, true));            // its beat already ended
		CHECK_FALSE(StoryHoldActive(Herd, "", false, false));          // no beat at all (seen, or no quest): at once
		CHECK_FALSE(StoryHoldActive(Herd, "cs_rf_brand", false, false));
		CHECK_FALSE(StoryHoldActive(Footman, "cs_rf_herd", true, false));   // only the story bodies
		// the timeline
		const FTimeline& T = Herd.Timeline;
		FHuskState S = MakeHusk(1000.0, Herd.HoldFromMs, false, true);
		CHECK(S.bStoryHeld);
		CHECK(Evaluate(S, T, 30000.0).Grey == 0.0);
		CHECK(Evaluate(S, T, 30000.0).Phase == EPhase::Hold);
		CHECK(std::isinf(CrumbleStartMs(S, T)));
		// the beat ends while the sim clock stood (a cinematic): the hold counts from the clip's end
		FHuskState Frozen = S;
		ReleaseStoryHold(Frozen, 1000.0, Herd.HoldFromMs);
		CHECK(Frozen.HoldStartMs == doctest::Approx(2400.0));
		// the beat ends later on the sim clock: the hold counts from then
		ReleaseStoryHold(S, 9000.0, Herd.HoldFromMs);
		CHECK_FALSE(S.bStoryHeld);
		CHECK(S.HoldStartMs == doctest::Approx(9000.0));
		CHECK(Evaluate(S, T, 9449.0).Grey == 0.0);
		CHECK(Evaluate(S, T, 9450.0 + 190.0).Grey == doctest::Approx(0.5));
		CHECK(CrumbleStartMs(S, T) == doctest::Approx(9000.0 + 450.0 + 380.0));
		// a second release changes nothing
		ReleaseStoryHold(S, 20000.0, Herd.HoldFromMs);
		CHECK(S.HoldStartMs == doctest::Approx(9000.0));
	}

	TEST_CASE("removals: expired dissolves, raised goes at once, consumed crumbles away, burned greys under the flare")
	{
		const FTimeline T = SyntheticTimeline();
		// expired from the husk: .38 -> 1 over 700 ms
		{
			FHuskState S = MakeHusk(0.0, 900.0, false, false);
			BeginEnd(S, EEnd::Expired, T, 20000.0);
			CHECK(S.EndFromGrey == doctest::Approx(1.0));
			CHECK(S.EndFromDissolve == doctest::Approx(0.38));
			CHECK(Evaluate(S, T, 20350.0).Dissolve == doctest::Approx(0.69));
			CHECK(Evaluate(S, T, 20350.0).Phase == EPhase::Ending);
			CHECK_FALSE(Evaluate(S, T, 20699.0).bDone);
			CHECK(Evaluate(S, T, 20700.0).bDone);
			CHECK(Evaluate(S, T, 20700.0).Dissolve == doctest::Approx(1.0));
			CHECK(Evaluate(S, T, 20700.0).ShadowAlpha == doctest::Approx(0.0));
			// a later end does not restart it
			BeginEnd(S, EEnd::Burned, T, 20400.0);
			CHECK(S.End == EEnd::Expired);
		}
		// evicted by the cap while it still falls: greys on the way out
		{
			FHuskState S = MakeHusk(0.0, 900.0, false, false);
			BeginEnd(S, EEnd::Expired, T, 300.0);
			CHECK(Evaluate(S, T, 650.0).Grey == doctest::Approx(0.5));
			CHECK(Evaluate(S, T, 650.0).Dissolve == doctest::Approx(0.5));
			CHECK(Evaluate(S, T, 1000.0).bDone);
		}
		// raised: gone now
		{
			FHuskState S = MakeHusk(0.0, 900.0, false, false);
			BeginEnd(S, EEnd::Raised, T, 5000.0);
			CHECK(Evaluate(S, T, 5000.0).bDone);
			CHECK(Evaluate(S, T, 5000.0).Phase == EPhase::Gone);
		}
		// consumed: QuickCrumbleMs
		{
			FHuskState S = MakeHusk(0.0, 900.0, false, false);
			BeginEnd(S, EEnd::Consumed, T, 6000.0);
			CHECK(Evaluate(S, T, 6000.0 + QuickCrumbleMs / 2.0).Dissolve == doctest::Approx(0.69));
			CHECK(Evaluate(S, T, 6000.0 + QuickCrumbleMs).bDone);
		}
		// burned by the fire killing blow (Added + Removed{burned} in one frame): grey at once, the crumble, the expire
		{
			FHuskState S = MakeHusk(0.0, 900.0, false, false);
			BeginEnd(S, EEnd::Burned, T, 0.0);
			CHECK(Evaluate(S, T, 190.0).Grey == doctest::Approx(0.5));
			CHECK(Evaluate(S, T, 190.0).Dissolve == 0.0);
			CHECK(Evaluate(S, T, 380.0 + 450.0).Dissolve == doctest::Approx(0.19));
			CHECK(Evaluate(S, T, 380.0 + 900.0).Dissolve == doctest::Approx(0.38));
			CHECK(Evaluate(S, T, 380.0 + 900.0 + 350.0).Dissolve == doctest::Approx(0.69));
			CHECK_FALSE(Evaluate(S, T, 380.0 + 900.0 + 699.0).bDone);
			CHECK(Evaluate(S, T, 380.0 + 900.0 + 700.0).bDone);
		}
		// burned as a husk (already grey and crumbled): straight to the blow-away
		{
			FHuskState S = MakeHusk(0.0, 900.0, false, false);
			BeginEnd(S, EEnd::Burned, T, 10000.0);
			CHECK(Evaluate(S, T, 10350.0).Dissolve == doctest::Approx(0.69));
			CHECK(Evaluate(S, T, 10700.0).bDone);
		}
	}

	TEST_CASE("removal plans and their recipes on the real blocks")
	{
		const FCorpseBlock B = ParseCorpseBlock(AssetByGameId("monster_deserter"), Contract());
		REQUIRE(B.bValid);
		const FRemovalPlan Expired = PlanRemoval(abyss::CorpseRemoveReason::Expired, B);
		CHECK(Expired.End == EEnd::Expired);
		CHECK(Expired.Recipe == "corpse.expire");
		CHECK_FALSE(Expired.bRemoveNow);
		const FRemovalPlan Raised = PlanRemoval(abyss::CorpseRemoveReason::Raised, B);
		CHECK(Raised.bRemoveNow);
		CHECK(Raised.Recipe.empty());
		const FRemovalPlan Consumed = PlanRemoval(abyss::CorpseRemoveReason::Consumed, B);
		CHECK(Consumed.End == EEnd::Consumed);
		CHECK(Consumed.Recipe.empty());
		const FRemovalPlan Burned = PlanRemoval(abyss::CorpseRemoveReason::Burned, B);
		CHECK(Burned.Recipe == "corpse.burn");
		CHECK(Burned.bFireColor);
		for (const FRemovalPlan& P : { Expired, Burned })
		{
			CHECK(HasRecipe(P.Recipe));
		}
	}

	TEST_CASE("a Feed drains the held body toward the feeder, the consume finishes it")
	{
		const FTimeline T = SyntheticTimeline();
		FHuskState S = MakeHusk(0.0, 900.0, false, false);
		BeginFeed(S, T, 500.0, 2500.0);   // still falling: nothing yet
		CHECK(S.FeedFromDissolve == 0.0);
		CHECK(Evaluate(S, T, 500.0 + 190.0).Grey == doctest::Approx(0.5));
		CHECK(Evaluate(S, T, 1500.0).Dissolve == doctest::Approx(FeedDrainTo * 0.5));
		CHECK(Evaluate(S, T, 2500.0).Dissolve == doctest::Approx(FeedDrainTo));
		CHECK(Evaluate(S, T, 2500.0).Phase == EPhase::Ending);
		BeginEnd(S, EEnd::Consumed, T, 2500.0);
		CHECK(S.EndFromDissolve == doctest::Approx(FeedDrainTo));
		CHECK(Evaluate(S, T, 2500.0 + QuickCrumbleMs).bDone);
		// a second feed or a feed after the removal is ignored
		FHuskState Done = MakeHusk(0.0, 900.0, false, false);
		BeginEnd(Done, EEnd::Expired, T, 100.0);
		BeginFeed(Done, T, 200.0, 900.0);
		CHECK_FALSE(Done.bFeeding);
	}

	TEST_CASE("the rise reverses the husk's ash over the first 60 % of the Rise clip")
	{
		const FAshContract C = Contract();
		const FRiseStart Husk{ 1.0, 0.38 };
		const double RiseMs = 1200.0;
		CHECK(RiseAsh(Husk, 0.0, RiseMs, C.RiseFraction, 0.0).Grey == doctest::Approx(1.0));
		CHECK(RiseAsh(Husk, 0.0, RiseMs, C.RiseFraction, 360.0).Grey == doctest::Approx(0.5));
		CHECK(RiseAsh(Husk, 0.0, RiseMs, C.RiseFraction, 360.0).Dissolve == doctest::Approx(0.19));
		CHECK_FALSE(RiseAsh(Husk, 0.0, RiseMs, C.RiseFraction, 719.0).bDone);
		const FRiseAsh End = RiseAsh(Husk, 0.0, RiseMs, C.RiseFraction, 720.0);
		CHECK(End.bDone);
		CHECK(End.Grey == 0.0);
		CHECK(End.Dissolve == 0.0);
		// raised before it greyed (or a summon): nothing to reverse
		CHECK(RiseAsh(FRiseStart{ 0.0, 0.0 }, 0.0, RiseMs, C.RiseFraction, 0.0).bDone);
	}

	TEST_CASE("the burn colour follows the fire's source: the hero's ember (its kill's impact), the enemy's violet, else grey")
	{
		constexpr abyss::EntityId Pet = 7001;
		constexpr abyss::EntityId Monster = 42;
		// EvCorpseRemoved::burner's side
		CHECK(FireSideOf(abyss::kHeroEntityId, abyss::kNoEntity) == EFireSide::Hero);
		CHECK(FireSideOf(Pet, Pet) == EFireSide::Hero);                 // the hero's beast
		CHECK(FireSideOf(Pet, abyss::kNoEntity) == EFireSide::Enemy);   // no beast out: an entity that is not the hero's
		CHECK(FireSideOf(Monster, Pet) == EFireSide::Enemy);            // a strike, a hazard, an on-death burst
		CHECK(FireSideOf(abyss::kNoEntity, Pet) == EFireSide::Unknown);
		// the hero's fire: its own fire kill's impact colour, else ember (a fire area that killed nothing, a fire wall)
		CHECK(BurnColor(EFireSide::Hero, 0x123456).bHas);
		CHECK(BurnColor(EFireSide::Hero, 0x123456).Rgb == 0x123456u);
		CHECK(BurnColor(EFireSide::Hero, 0).Rgb == HeroFireRgb);
		// the enemy's fire is breach violet whatever its hit looked like (DR5), even when it struck nobody
		CHECK(BurnColor(EFireSide::Enemy, 0).Rgb == BreachFireRgb);
		CHECK(BurnColor(EFireSide::Enemy, 0xFF7A2A).Rgb == BreachFireRgb);
		// no source: the recipe's own grey
		CHECK_FALSE(BurnColor(EFireSide::Unknown, 0).bHas);
		CHECK_FALSE(BurnColor(EFireSide::Unknown, 0x123456).bHas);
		// the colour language: ember is warm, violet is the breach's
		CHECK(((HeroFireRgb >> 16) & 0xFF) > (HeroFireRgb & 0xFF));
		CHECK((BreachFireRgb & 0xFF) > ((BreachFireRgb >> 8) & 0xFF));
	}

	TEST_CASE("the husk's geometry follows the body as drawn (a hunt leader is drawn x1.25)")
	{
		const FAshContract C = Contract();
		const FCorpseBlock Block = ParseCorpseBlock(AssetByGameId("monster_deserter"), C);
		REQUIRE(Block.bValid);
		REQUIRE(Block.HuskTopCm > 0.0);
		const FHuskGeometry One = HuskGeometry(Block, 1.0, 60.0);
		CHECK(One.CenterForwardCm == Block.CenterForwardCm);
		CHECK(One.CenterRightCm == Block.CenterRightCm);
		CHECK(One.RadiusCm == Block.HuskRadiusCm);
		CHECK(One.TopCm == Block.HuskTopCm);
		const FHuskGeometry Big = HuskGeometry(Block, 1.25, 60.0);
		CHECK(Big.CenterForwardCm == doctest::Approx(1.25 * Block.CenterForwardCm));
		CHECK(Big.CenterRightCm == doctest::Approx(1.25 * Block.CenterRightCm));
		CHECK(Big.RadiusCm == doctest::Approx(1.25 * Block.HuskRadiusCm));
		CHECK(Big.TopCm == doctest::Approx(1.25 * Block.HuskTopCm));
		// no radius in the block: the default, scaled too; a bad scale counts as 1
		FCorpseBlock Bare;
		CHECK(HuskGeometry(Bare, 2.0, 60.0).RadiusCm == doctest::Approx(120.0));
		CHECK(HuskGeometry(Block, 0.0, 60.0).TopCm == Block.HuskTopCm);
		// the scale the hunts are drawn at (the core's default): the hunt leaders with a husk lie that much bigger
		CHECK(abyss::MonsterAiDef().huntVisualScale == doctest::Approx(1.25));
	}

	TEST_CASE("reconcile: late views appear as husks, entries gone without an event dissolve")
	{
		FReconcile R;
		Reconcile({ 1, 2, 3 }, { 2, 3, 4, 5 }, R);
		CHECK(R.Add == std::vector<uint32_t>{ 4, 5 });
		CHECK(R.Remove == std::vector<uint32_t>{ 1 });
		Reconcile({}, {}, R);
		CHECK(R.Add.empty());
		CHECK(R.Remove.empty());
	}

	TEST_CASE("the visible cap keeps seeds and story bodies, then the newest")
	{
		CHECK(HuskCap(0, false) == 32);
		CHECK(HuskCap(2, false) == 32);
		CHECK(HuskCap(0, true) == 12);
		CHECK(HuskCap(1, true) == 20);
		CHECK(HuskCap(2, true) == 32);
		// the core's worst case (24 kill bodies + the Sheepfold's 6 seeds) fits every desktop tier
		CHECK(HuskCap(0, false) >= 30);
		std::vector<FHuskEntry> Entries;
		for (uint32_t I = 1; I <= 6; ++I)
		{
			Entries.push_back({ I, 0.0, true, false });            // seeds, the oldest
		}
		for (uint32_t I = 7; I <= 30; ++I)
		{
			Entries.push_back({ I, 1000.0 * I, false, I == 8 });   // kills; #8 waits for its beat
		}
		std::vector<FHuskEntry> Scratch;
		std::vector<uint32_t> Out = { 999u };   // cleared
		OverflowHusks(Entries, 12, Scratch, Out);
		CHECK(Out.size() == 18u);
		for (uint32_t Keep : { 1u, 2u, 3u, 4u, 5u, 6u, 8u, 30u, 29u, 28u, 27u, 26u })
		{
			CHECK(std::find(Out.begin(), Out.end(), Keep) == Out.end());
		}
		CHECK(std::find(Out.begin(), Out.end(), 7u) != Out.end());
		CHECK(std::find(Out.begin(), Out.end(), 25u) != Out.end());
		CHECK(std::find(Out.begin(), Out.end(), 999u) == Out.end());
		// the input is untouched (taken by reference, sorted in the scratch buffer)
		CHECK(Entries.front().Id == 1u);
		CHECK(Entries.back().Id == 30u);
		// under the cap: nothing, and the scratch buffer is not even filled
		Scratch.clear();
		OverflowHusks(Entries, 32, Scratch, Out);
		CHECK(Out.empty());
		CHECK(Scratch.empty());
		// the same buffers again (no growth once they hold a frame's worth)
		OverflowHusks(Entries, 12, Scratch, Out);
		const size_t Capacity = Scratch.capacity();
		OverflowHusks(Entries, 12, Scratch, Out);
		CHECK(Scratch.capacity() == Capacity);
		CHECK(Out.size() == 18u);
	}

	TEST_CASE("an unadopted corpse-holding death falls back to the fade")
	{
		CHECK_FALSE(AdoptionTimedOut(900.0 + AdoptTimeoutMs, 900.0));
		CHECK(AdoptionTimedOut(900.0 + AdoptTimeoutMs + 1.0, 900.0));
	}

	TEST_CASE("breach-blood: the tar puddle forms over the Death clip and dries in 3 s; nothing else leaves one")
	{
		const FAshContract C = Contract();
		const abyss::JsonValue& Clot = AssetByGameId("monster_breachclot");
		const FCorpseBlock ClotBlock = ParseCorpseBlock(Clot, C);
		CHECK(ClotBlock.Look == ELook::Puddle);
		CHECK_FALSE(ClotBlock.bValid);   // no registry entry: the layer's husk timeline never runs for it
		// the radius factors scale the body's half-width W (footprintRadiusCm = W x 100; the blob shadow is W x 1.05), not
		// the melted last frame's footprint (corpse.husk.radiusCm)
		CHECK(BodyHalfWidthCm(Clot) == doctest::Approx(44.0));
		CHECK(Clot.Get("blobShadowRadiusCm").AsDouble() == doctest::Approx(1.05 * BodyHalfWidthCm(Clot)).epsilon(0.002));
		const FPuddle P = ParsePuddle(Clot, ClotBlock, 1.0);
		REQUIRE(P.bValid);
		CHECK(P.Rgb == 0x2A0E14u);
		CHECK(P.Alpha == doctest::Approx(0.7));
		CHECK(P.RadiusCm == doctest::Approx(1.1 * 44.0));   // 48.4 cm: about 0.97 m across
		CHECK(P.RadiusCm < ClotBlock.HuskRadiusCm);
		CHECK(ParsePuddle(Clot, ClotBlock, 1.25).RadiusCm == doctest::Approx(1.25 * 1.1 * 44.0));   // a body drawn bigger
		CHECK(P.DriesMs == 3000.0);
		CHECK(P.FormMs == ClotBlock.HoldFromMs);
		CHECK(P.FormMs == ClipLengthMs(Clot, "Death"));   // it melts in over the whole Death clip
		const abyss::JsonValue& Bloat = AssetByGameId("monster_cinderbloat");
		const FCorpseBlock BloatBlock = ParseCorpseBlock(Bloat, C);
		const FPuddle Q = ParsePuddle(Bloat, BloatBlock, 1.0);
		REQUIRE(Q.bValid);
		CHECK(Q.Rgb == 0x1A0A0Au);
		CHECK(Q.Alpha == doctest::Approx(0.6));
		CHECK(BodyHalfWidthCm(Bloat) == doctest::Approx(59.4));
		CHECK(Q.RadiusCm == doctest::Approx(1.2 * 59.4));   // 71.3 cm: the shards lie within 0.55 m, inside the decal
		CHECK(Q.RadiusCm > 55.0);
		CHECK(Q.FormMs == 900.0);
		// the trimmed GetAssetJson form (corpse block + the Death clip's puddleDecal + the half-width) reads the same
		const FCorpseBlock TrimmedBlock =
			ParseCorpseBlock(audio_test::Json(R"({"corpse": {"look": "puddle", "holdFromMs": 500, "husk": {"radiusCm": 50}}})"), C);
		const FPuddle Trimmed = ParsePuddle(audio_test::Json(
			R"({"corpse": {"look": "puddle", "holdFromMs": 500, "husk": {"radiusCm": 50}}, "footprintRadiusCm": 40,
			    "clips": {"Death": {"puddleDecal": {"color": "#102030", "alpha": 0.5, "radiusFactor": 1.5, "driesMs": 2000}}}})"),
			TrimmedBlock, 1.0);
		REQUIRE(Trimmed.bValid);
		CHECK(Trimmed.Rgb == 0x102030u);
		CHECK(Trimmed.RadiusCm == doctest::Approx(60.0));
		CHECK(Trimmed.DriesMs == 2000.0);
		// without a footprint, the blob shadow over its width factor; without either, no puddle (never the husk radius)
		const FPuddle FromBlob = ParsePuddle(audio_test::Json(
			R"({"blobShadowRadiusCm": 42, "fx": {"blobShadow": {"radiusFactorOfWidth": 1.05}},
			    "clips": {"Death": {"puddleDecal": {"color": "#102030", "alpha": 0.5, "radiusFactor": 1.5}}}})"),
			TrimmedBlock, 1.0);
		REQUIRE(FromBlob.bValid);
		CHECK(FromBlob.RadiusCm == doctest::Approx(60.0));
		CHECK_FALSE(ParsePuddle(audio_test::Json(
			R"({"clips": {"Death": {"puddleDecal": {"color": "#102030", "alpha": 0.5, "radiusFactor": 1.5}}}})"), TrimmedBlock, 1.0).bValid);
		// a husk block leaves a husk even with a puddle decal on its Death clip
		CHECK_FALSE(ParsePuddle(audio_test::Json(
			R"({"footprintRadiusCm": 40, "clips": {"Death": {"puddleDecal": {"color": "#102030", "alpha": 0.5, "radiusFactor": 1.5}}}})"),
			ParseCorpseBlock(audio_test::Json(R"({"corpse": {"look": "husk", "registry": true, "holdFromMs": 500,
				"husk": {"radiusCm": 50}}})"), C), 1.0).bValid);
		// a husk / carcass leaves no puddle; the Wave 1 slime's puddle decal has no corpse block (today's death)
		CHECK_FALSE(ParsePuddle(AssetByGameId("monster_deserter"), ParseCorpseBlock(AssetByGameId("monster_deserter"), C), 1.0).bValid);
		const abyss::JsonValue& Slime = AssetByGameId("monster_slime");
		REQUIRE(Slime.IsObject());
		CHECK(Slime.Get("fx").Get("puddle").IsObject());
		CHECK_FALSE(ParsePuddle(Slime, ParseCorpseBlock(Slime, C), 1.0).bValid);
		// every puddle look of the roster is a breach body
		int Puddles = 0;
		for (const abyss::JsonMember& Member : ArtManifest().Get("assets").Members())
		{
			const FCorpseBlock B = ParseCorpseBlock(Member.value, C);
			if (B.Look == ELook::Puddle)
			{
				CAPTURE(Member.key);
				CHECK(ParsePuddle(Member.value, B, 1.0).bValid);
				// the puddle's factor and the blob shadow's scale the same W (the art lane's one convention)
				CHECK(Member.value.Get("blobShadowRadiusCm").AsDouble()
					== doctest::Approx(Member.value.Get("fx").Get("blobShadow").Get("radiusFactorOfWidth").AsDouble() * BodyHalfWidthCm(Member.value))
						   .epsilon(0.002));
				CHECK(Member.value.Get("corpse").Get("family").AsString() == "breach");
				++Puddles;
			}
		}
		CHECK(Puddles == 2);
	}

	TEST_CASE("the puddle's runtime recipe: the shipped decal emitter and Puddle sprite, in the puddle's colour")
	{
		FPuddle P;
		P.bValid = true;
		P.Rgb = 0x2A0E14;
		P.Alpha = 0.7;
		P.RadiusCm = 80.0;
		P.DriesMs = 3000.0;
		P.FormMs = 500.0;
		abyss::JsonValue Recipe;
		REQUIRE(abyss::ParseJson(PuddleRecipeJson(P), Recipe));
		CHECK(Recipe.Get("palette").AsString() == "event");
		REQUIRE(Recipe.Get("layers").Items().size() == 1u);
		const abyss::JsonValue& L = Recipe.Get("layers").Items()[0];
		CHECK(L.Get("emitter").AsString() == "decal");
		CHECK(L.Get("sprite").AsString() == PuddleSprite);
		CHECK(L.Get("blend").AsString() == "translucent");
		CHECK(L.Get("at").AsString() == "point");
		CHECK(L.Get("count").AsDouble() == 1.0);
		CHECK(L.Get("height").AsDouble() == PuddleLiftCm);
		CHECK(L.Get("sizeCm").AsDouble() == doctest::Approx(160.0));
		CHECK(L.Get("grow").AsDouble() == 1.0);
		CHECK(L.Get("lifeMs").AsDouble() == doctest::Approx(3500.0));
		CHECK(L.Get("fadeIn").AsDouble() == doctest::Approx(500.0 / 3500.0));
		CHECK(L.Get("alpha").Items()[0].AsDouble() == doctest::Approx(0.7));
		CHECK(L.Get("alpha").Items()[1].AsDouble() == 0.0);
		CHECK(L.Get("color").AsString() == "#2A0E14");
		// the sprite ships translucent (vfx_recipes.json), the decal emitter draws shipped puddles today (skill.bleed_strike)
		CHECK(Recipes().Get("sprites").Get(PuddleSprite).Get("blend").AsString() == "translucent");
		bool bShippedDecal = false;
		for (const abyss::JsonValue& Layer : Recipes().Get("recipes").Get("skill.bleed_strike").Get("layers").Items())
		{
			bShippedDecal = bShippedDecal || (Layer.Get("emitter").AsString() == "decal" && Layer.Get("sprite").AsString() == PuddleSprite);
		}
		CHECK(bShippedDecal);
	}
}
