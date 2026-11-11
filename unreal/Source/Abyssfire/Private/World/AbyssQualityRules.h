// The quality refresh's decisions (UAbyssWorldBuilder::RefreshQuality, AbyssConsoleVars; performance audit F1 / F7).
//
// The refresh used to run every 60th frame and re-apply everything: it wrote six console variables with SetByCode
// whatever their value (one of them, r.Shadow.CSMShadowDistanceFadeoutFraction, does not exist: the first Mac run logged
// 31 failed lookups), which overwrote or fought every console A/B once a second, and fanned out to the zone actor (a
// rebuilt FPostProcessSettings), every character and every husk. The rules here make it apply on change only:
//
//   - FRefreshState: what the last refresh applied (the tier's profile, the viewport rows, the render scale). The
//     profile fan-out (VFX, telegraphs, husks, the zone actor, the characters' shadow mode) runs when the profile
//     changes or the caller forces it (world begin, zone build); the screen percentage follows rows x scale, so a
//     window resize still reaches it.
//   - FConsoleLedger: a console variable is written when the value WE want changes, and then only if the engine's
//     value differs. A console or device-profile edit therefore stays until the tier or the window changes what we
//     want. A name the engine does not know is reported once (one Warning naming it) and kept in a list the autotest
//     writes to report.json ("missingConsoleVariables"), so a wrong name shows up in the next engine run.
//
// The sun's cascade count and shadow-distance fade live here too (AAbyssZoneActor::ApplyQuality): the fade is a
// property of the directional light component, not a console variable.
//
// UE-free and header-only (std only), tested in unreal/PresentationTests/tests/quality_test.cpp.
#pragma once

#include <cmath>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace AbyssQualityRules
{
	// ---- the sun (DECISIONS P10 revised; audit F7) ----
	/** Desktop CSM cascades. One: with a linear split the near one of two covered the air under the camera (the visible
	 *  ground lies at 66-90 % of the shadow distance at pitch -50) and still took every zone-sized caster's draw. */
	inline constexpr int DesktopCascades = 1;
	inline constexpr int MobileCascades = 1;
	/** The CSM fades out over the last 5 % of the shadow distance (UDirectionalLightComponent::ShadowDistanceFadeoutFraction). */
	inline constexpr float SunShadowFadeoutFraction = 0.05f;

	inline constexpr int SunCascades(bool bMobile) { return bMobile ? MobileCascades : DesktopCascades; }

	// ---- the 3D resolution cap (ue58-platform.md 6.8) ----
	inline constexpr float BaseRows = 720.f;
	inline constexpr float MinScreenPercentage = 25.f;
	inline constexpr float MaxScreenPercentage = 100.f;

	/** r.ScreenPercentage for a viewport of `ViewportRows` px: 720 x scale rows of 3D at most, never upscaled past
	 *  native, never under 25 %. 0 when the viewport has no size yet (nothing to set). */
	inline float ScreenPercentage(int ViewportRows, float ResolutionScale)
	{
		if (ViewportRows <= 0 || !(ResolutionScale > 0.f))
		{
			return 0.f;
		}
		const float Raw = 100.f * BaseRows * ResolutionScale / static_cast<float>(ViewportRows);
		return Raw < MinScreenPercentage ? MinScreenPercentage : (Raw > MaxScreenPercentage ? MaxScreenPercentage : Raw);
	}

	// ---- apply on change ----
	/** The fields of FAbyssQualityProfile (World/AbyssWorldTypes.h), UE-free. */
	struct FProfileKey
	{
		int Tier = -1;
		int MaxDynamicLights = 0;
		float LightingUpdateIntervalMs = 0.f;
		float ParticleFrequencyMultiplier = 0.f;
		float ResolutionScale = 0.f;
		bool bBloom = false;
		bool bColorGrading = false;
		bool bMobile = false;

		friend bool operator==(const FProfileKey&, const FProfileKey&) = default;
	};

	struct FRefreshState
	{
		bool bApplied = false;
		FProfileKey Profile;
		int ViewportRows = 0;
	};

	struct FRefreshPlan
	{
		/** Fan the profile out to the systems and actors. */
		bool bProfile = false;
		/** Re-evaluate the console variables (each still goes through the ledger). */
		bool bConsole = false;
	};

	/** What a refresh has to do, and the state after it. `bForce`: world begin / zone build (new receivers). */
	inline FRefreshPlan PlanRefresh(FRefreshState& State, const FProfileKey& Profile, int ViewportRows, bool bForce)
	{
		FRefreshPlan Plan;
		const bool bProfileChanged = !State.bApplied || !(State.Profile == Profile);
		Plan.bProfile = bForce || bProfileChanged;
		Plan.bConsole = bProfileChanged || State.ViewportRows != ViewportRows;
		State.bApplied = true;
		State.Profile = Profile;
		State.ViewportRows = ViewportRows;
		return Plan;
	}

	/** Two console values are the same setting (floats come back through the engine's own storage). */
	inline bool SameValue(double A, double B)
	{
		return std::fabs(A - B) <= 1e-4 * (std::fabs(A) > 1.0 ? std::fabs(A) : 1.0);
	}

	class FConsoleLedger
	{
	public:
		/** True when `Wanted` differs from what this ledger last wanted for `Name` (first call included): only then may
		 *  the variable be written. Records the new wish. */
		bool WantsChange(std::string_view Name, double Wanted)
		{
			const auto It = Wishes.find(Name);
			if (It != Wishes.end() && SameValue(It->second, Wanted))
			{
				return false;
			}
			if (It != Wishes.end())
			{
				It->second = Wanted;
			}
			else
			{
				Wishes.emplace(std::string(Name), Wanted);
			}
			return true;
		}

		/** After WantsChange: write only when the engine's value differs. */
		static bool NeedsWrite(double Current, double Wanted) { return !SameValue(Current, Wanted); }

		/** A failed lookup. True the first time the name is seen (log one Warning then); the wish is dropped so the
		 *  variable is tried again if it appears later (a plugin's variable). */
		bool NoteMissing(std::string_view Name)
		{
			const auto Wish = Wishes.find(Name);
			if (Wish != Wishes.end())
			{
				Wishes.erase(Wish);
			}
			for (const std::string& Known : MissingNames)
			{
				if (Known == Name)
				{
					return false;
				}
			}
			MissingNames.emplace_back(Name);
			return true;
		}

		/** A new world (a second play-in-editor session): what we want is asserted once more, since anything may have
		 *  changed the variables in between. The missing names stay known (no second Warning). */
		void ForgetWishes() { Wishes.clear(); }

		/** Every name whose lookup failed, in first-seen order (report.json "missingConsoleVariables"). */
		const std::vector<std::string>& Missing() const { return MissingNames; }

	private:
		std::map<std::string, double, std::less<>> Wishes;
		std::vector<std::string> MissingNames;
	};
}
