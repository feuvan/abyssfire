// The quality refresh's rules (performance audit F1 / F7; Private/World/AbyssQualityRules.h, used by
// UAbyssWorldBuilder::RefreshQuality, AbyssConsoleVars and AAbyssZoneActor::ApplyQuality): the refresh applies on change
// only, a window resize still reaches the screen percentage, a console variable is written only when the value we want
// changes and the engine's differs, and a name the engine does not know is reported once and listed.
#include <string>
#include <vector>

#include "doctest/doctest.h"

#include "World/AbyssQualityRules.h"

namespace
{
	using namespace AbyssQualityRules;

	FProfileKey Balanced()
	{
		FProfileKey K;
		K.Tier = 1;
		K.MaxDynamicLights = 16;
		K.LightingUpdateIntervalMs = 50.f;
		K.ParticleFrequencyMultiplier = 1.5f;
		K.ResolutionScale = 1.5f;
		K.bBloom = true;
		K.bColorGrading = true;
		return K;
	}

	/** The engine side of one console variable, as AbyssConsoleVars::Apply drives it. */
	struct FFakeEngine
	{
		FConsoleLedger Ledger;
		double Value = 0.0;
		int Writes = 0;

		void Apply(const char* Name, double Wanted)
		{
			if (Ledger.WantsChange(Name, Wanted) && FConsoleLedger::NeedsWrite(Value, Wanted))
			{
				Value = Wanted;
				++Writes;
			}
		}
	};
}

TEST_SUITE("quality")
{
	TEST_CASE("the sun: one cascade everywhere, a 5 % fade at the shadow distance")
	{
		CHECK(SunCascades(/*bMobile*/ false) == 1);   // audit F7 (was 2: the near one drew the air under the camera)
		CHECK(SunCascades(true) == 1);
		CHECK(SunShadowFadeoutFraction == doctest::Approx(0.05f));
	}

	TEST_CASE("the screen percentage caps the 3D rows at 720 x scale")
	{
		CHECK(ScreenPercentage(720, 1.f) == doctest::Approx(100.f));
		CHECK(ScreenPercentage(720, 2.f) == doctest::Approx(100.f));      // never above native
		CHECK(ScreenPercentage(1440, 1.f) == doctest::Approx(50.f));
		CHECK(ScreenPercentage(1440, 1.5f) == doctest::Approx(75.f));
		CHECK(ScreenPercentage(2160, 1.5f) == doctest::Approx(50.f));
		CHECK(ScreenPercentage(4320, 1.f) == doctest::Approx(25.f));      // floor
		CHECK(ScreenPercentage(0, 1.5f) == 0.f);                           // no viewport yet: nothing to set
		CHECK(ScreenPercentage(-5, 1.5f) == 0.f);
		CHECK(ScreenPercentage(1080, 0.f) == 0.f);
	}

	TEST_CASE("a refresh applies once, then only on change")
	{
		FRefreshState State;
		FRefreshPlan Plan = PlanRefresh(State, Balanced(), 720, /*bForce*/ false);
		CHECK(Plan.bProfile);
		CHECK(Plan.bConsole);

		// the 60-frame poll with nothing changed: no fan-out, no console write
		for (int Poll = 0; Poll < 100; ++Poll)
		{
			Plan = PlanRefresh(State, Balanced(), 720, false);
			CHECK_FALSE(Plan.bProfile);
			CHECK_FALSE(Plan.bConsole);
		}

		// a window resize reaches the screen percentage without re-applying the profile
		Plan = PlanRefresh(State, Balanced(), 1440, false);
		CHECK_FALSE(Plan.bProfile);
		CHECK(Plan.bConsole);
		Plan = PlanRefresh(State, Balanced(), 1440, false);
		CHECK_FALSE(Plan.bConsole);

		// abyss.RenderScale (the profile's resolution scale) and a tier change re-apply both
		FProfileKey Scaled = Balanced();
		Scaled.ResolutionScale = 1.f;
		Plan = PlanRefresh(State, Scaled, 1440, false);
		CHECK(Plan.bProfile);
		CHECK(Plan.bConsole);
		FProfileKey Low = Scaled;
		Low.Tier = 0;
		Low.bBloom = false;
		Plan = PlanRefresh(State, Low, 1440, false);
		CHECK(Plan.bProfile);
		CHECK(Plan.bConsole);

		// a zone build forces the fan-out (new receivers) but leaves the console variables alone
		Plan = PlanRefresh(State, Low, 1440, /*bForce*/ true);
		CHECK(Plan.bProfile);
		CHECK_FALSE(Plan.bConsole);
	}

	TEST_CASE("a console variable is written only when our wish changes and the engine differs")
	{
		FFakeEngine Engine;
		Engine.Value = 2048.0;                                 // the engine's default
		Engine.Apply("r.Shadow.MaxCSMResolution", 4096.0);
		CHECK(Engine.Writes == 1);
		CHECK(Engine.Value == 4096.0);
		Engine.Apply("r.Shadow.MaxCSMResolution", 4096.0);   // the same wish again (a resize re-evaluates every name)
		CHECK(Engine.Writes == 1);

		// a console A/B: the operator types 2048; no later refresh with the same wish puts 4096 back
		Engine.Value = 2048.0;
		for (int Refresh = 0; Refresh < 10; ++Refresh)
		{
			Engine.Apply("r.Shadow.MaxCSMResolution", 4096.0);
		}
		CHECK(Engine.Value == 2048.0);
		CHECK(Engine.Writes == 1);

		// a tier change is a new wish: written; and a wish the engine already holds is not written at all
		Engine.Apply("r.Shadow.MaxCSMResolution", 2048.0);
		CHECK(Engine.Writes == 1);
		Engine.Apply("r.Shadow.MaxCSMResolution", 4096.0);
		CHECK(Engine.Writes == 2);
		CHECK(Engine.Value == 4096.0);

		// a new world forgets the wishes (not the engine's values): the same wish is asserted once more
		Engine.Value = 1024.0;
		Engine.Apply("r.Shadow.MaxCSMResolution", 4096.0);
		CHECK(Engine.Value == 1024.0);
		Engine.Ledger.ForgetWishes();
		Engine.Apply("r.Shadow.MaxCSMResolution", 4096.0);
		CHECK(Engine.Value == 4096.0);
		CHECK(Engine.Writes == 3);

		// names are independent, floats compare with a tolerance
		FConsoleLedger Ledger;
		CHECK(Ledger.WantsChange("r.ScreenPercentage", 75.0));
		CHECK_FALSE(Ledger.WantsChange("r.ScreenPercentage", 75.00001));
		CHECK(Ledger.WantsChange("r.ScreenPercentage", 50.0));
		CHECK(Ledger.WantsChange("r.AntiAliasingMethod", 4.0));
		CHECK_FALSE(FConsoleLedger::NeedsWrite(0.05f, 0.05));
		CHECK(FConsoleLedger::NeedsWrite(1.0, 4.0));
	}

	TEST_CASE("an unknown console variable is reported once and listed")
	{
		FConsoleLedger Ledger;
		CHECK(Ledger.Missing().empty());
		CHECK(Ledger.NoteMissing("r.Shadow.CSMShadowDistanceFadeoutFraction"));   // log one Warning
		for (int Refresh = 0; Refresh < 30; ++Refresh)                              // the first Mac run logged 31
		{
			CHECK_FALSE(Ledger.NoteMissing("r.Shadow.CSMShadowDistanceFadeoutFraction"));
		}
		CHECK(Ledger.NoteMissing("r.NoSuchThing"));
		CHECK(Ledger.Missing() == std::vector<std::string>{ "r.Shadow.CSMShadowDistanceFadeoutFraction", "r.NoSuchThing" });

		// a name that was known, then is gone (a plugin unloaded): the wish is dropped, so it is written again if it returns
		CHECK(Ledger.WantsChange("r.Plugin.Thing", 1.0));
		CHECK(Ledger.NoteMissing("r.Plugin.Thing"));
		CHECK(Ledger.WantsChange("r.Plugin.Thing", 1.0));		Ledger.ForgetWishes();
		CHECK(Ledger.Missing().size() == 3u);                        // the missing names stay known
		CHECK_FALSE(Ledger.NoteMissing("r.NoSuchThing"));
	}
}
