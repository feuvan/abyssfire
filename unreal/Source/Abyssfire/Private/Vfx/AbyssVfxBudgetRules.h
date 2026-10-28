// The particle budget of a zone with a presentation theme (dark remake 2G cross-stage review X4; DECISIONS DR16 (43)).
//
// UAbyssVfxSystem keeps one particle pool, capped at 700 live particles on desktop and 450 on mobile (DECISIONS R20);
// a spawn past the cap is dropped, whoever asks. Chapters 2-5 keep exactly that (LoopBudgetFor(false, ...) is today's
// behaviour). Chapter 1 adds continuous loops - the march's ashfall, Valdras's banner ring, auras, hazards, Ashnumb -
// and every zone loop (the camps' torches, the set pieces' smoulders) emits wherever the camera is, so in Valdras's last
// phase the loops alone filled the mobile cap and the hit sparks, the contact dust and the death bursts (the fight's
// feedback) were the first spawns dropped. In a zone with a presentation theme:
//
// * Ceiling: continuous emitters stop spawning at ThemedLoopShare (75 %) of the cap; one-shot bursts always keep the
//   last quarter (SpawnLimit).
// * Culling: a continuous emitter whose spawn point lies farther than ThemedLoopCullRadiusCm from the camera focus
//   spawns nothing (LoopInRange; its particles already alive live out). FarthestVisibleCm bounds what any zoom and
//   aspect can show: the radius is ~1.6 x that, so nothing on screen is culled; the far camps' torches no longer eat a
//   fight's budget.
// * Density: on mobile the loops emit at most at the balanced profile's frequency (interval multiplier >= 1.5), so the
//   worst Chapter 1 scene's loops fit under the ceiling on every tier (vfxbudget_test.cpp).
//
// SteadyParticles estimates a recipe's live particles once its loops have run a while (rate / multiplier x mean life,
// as UpdateEmitters spawns them); vfxbudget_test.cpp sums the worst Chapter 1 scenes with it per platform and tier.
//
// UE-free and header-only (std + AbyssCore public headers), tested in unreal/PresentationTests/tests/vfxbudget_test.cpp.
#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>

#include "abyss/base/Json.h"

namespace AbyssVfxBudgetRules
{
	inline constexpr double Pi = 3.14159265358979323846;

	/** DECISIONS R20: the live particle cap (the web FxEngine's MAX_PARTICLES was 900; art-inventory-ch1.md 1.9 had 600). */
	inline constexpr int DesktopMaxParticles = 700;
	inline constexpr int MobileMaxParticles = 450;

	/** A themed zone's continuous emitters stop at this share of the cap: one-shot bursts keep the rest. */
	inline constexpr double ThemedLoopShare = 0.75;
	/** A themed zone's continuous emitters farther than this from the camera focus spawn nothing (cm). */
	inline constexpr double ThemedLoopCullRadiusCm = 3000.0;
	/** The least loop interval multiplier on a mobile tier in a themed zone (render_quality.json balanced). */
	inline constexpr double ThemedMobileMinFrequencyMultiplier = 1.5;
	/** UpdateEmitters' clamp of the quality profile's multiplier (as before). */
	inline constexpr double MinFrequencyMultiplier = 0.25;
	/** The recipe parser's rate clamp (FAbyssVfxLibrary::ParseLayer). */
	inline constexpr double MaxLayerRate = 400.0;

	inline int MaxParticles(bool bMobile)
	{
		return bMobile ? MobileMaxParticles : DesktopMaxParticles;
	}

	struct FLoopBudget
	{
		bool bThemed = false;
		int MaxParticles = DesktopMaxParticles;
		/** Continuous emitters spawn only while fewer particles live (MaxParticles outside a themed zone). */
		int LoopCeiling = DesktopMaxParticles;
		/** > 0: continuous emitters farther than this from the camera focus spawn nothing (cm). */
		double CullRadiusCm = 0.0;
		/** Continuous emission interval multiplier: a layer spawns rate / FrequencyMultiplier particles per second. */
		double FrequencyMultiplier = 1.0;
	};

	/**
	 * The budget of a zone. Outside a presentation theme it is exactly today's: the full cap for everything, no culling,
	 * the profile's multiplier (clamped to .25 as UpdateEmitters always did).
	 */
	inline FLoopBudget LoopBudgetFor(bool bThemedZone, bool bMobile, double ProfileMultiplier)
	{
		FLoopBudget B;
		B.bThemed = bThemedZone;
		B.MaxParticles = MaxParticles(bMobile);
		const double Profile = (std::max)(MinFrequencyMultiplier, ProfileMultiplier);
		B.FrequencyMultiplier = Profile;
		B.LoopCeiling = B.MaxParticles;
		if (!bThemedZone)
		{
			return B;
		}
		B.LoopCeiling = static_cast<int>(std::floor(static_cast<double>(B.MaxParticles) * ThemedLoopShare));
		B.CullRadiusCm = ThemedLoopCullRadiusCm;
		if (bMobile)
		{
			B.FrequencyMultiplier = (std::max)(Profile, ThemedMobileMinFrequencyMultiplier);
		}
		return B;
	}

	/** The live count past which a spawn is dropped: the loop ceiling for a continuous emitter, the cap for a burst. */
	inline int SpawnLimit(const FLoopBudget& B, bool bContinuous)
	{
		return bContinuous ? B.LoopCeiling : B.MaxParticles;
	}

	/** Whether a continuous emitter at this squared ground distance (cm^2) from the camera focus may spawn (radius 0: always). */
	inline bool LoopInRange(double CullRadiusCm, double DistSqCm)
	{
		return !(CullRadiusCm > 0.0) || DistSqCm <= CullRadiusCm * CullRadiusCm;
	}

	inline bool LoopInRange(const FLoopBudget& B, double DistSqCm)
	{
		return LoopInRange(B.CullRadiusCm, DistSqCm);
	}

	// =================================================================================================================
	// Steady-state estimate (what UpdateEmitters keeps alive)
	// =================================================================================================================

	/** The mean default lifeMs of an emitter preset (AbyssVfxRecipe.cpp ApplyPreset); < 0 for an unknown preset. */
	inline double PresetMeanLifeMs(std::string_view Emitter)
	{
		struct FPreset
		{
			std::string_view Name;
			double MinMs;
			double MaxMs;
		};
		static constexpr FPreset Presets[] = {
			{ "glow", 300.0, 300.0 }, { "flash", 160.0, 160.0 }, { "glint", 200.0, 200.0 }, { "sparks", 180.0, 340.0 },
			{ "streak", 200.0, 200.0 }, { "motes", 400.0, 700.0 }, { "flames", 300.0, 520.0 }, { "smoke", 500.0, 900.0 },
			{ "debris", 420.0, 560.0 }, { "ring", 400.0, 400.0 }, { "shock", 400.0, 400.0 }, { "decal", 1400.0, 1400.0 },
			{ "slash", 150.0, 230.0 }, { "swipe", 230.0, 230.0 }, { "beam", 700.0, 700.0 }, { "gather", 260.0, 340.0 },
			{ "bolt", 240.0, 240.0 }, { "mesh", 400.0, 400.0 },
		};
		for (const FPreset& P : Presets)
		{
			if (P.Name == Emitter)
			{
				return 0.5 * (P.MinMs + P.MaxMs);
			}
		}
		return -1.0;
	}

	/** A layer that emits for as long as its loop plays: `rate` > 0 and no finite `durationMs`. */
	inline bool IsSteadyLayer(const abyss::JsonValue& Layer)
	{
		const double Rate = Layer.Get("rate").AsDouble(0.0);
		const abyss::JsonValue* Duration = Layer.Find("durationMs");
		return Rate > 0.0 && (Duration == nullptr || Duration->AsDouble(-1.0) < 0.0);
	}

	/** A layer's mean particle life (ms): its `lifeMs` (a number or [min, max]), else its preset's; false when neither. */
	inline bool LayerMeanLifeMs(const abyss::JsonValue& Layer, double& OutMs)
	{
		const abyss::JsonValue& Life = Layer.Get("lifeMs");
		if (Life.IsNumber())
		{
			OutMs = (std::max)(1.0, Life.AsDouble());
			return true;
		}
		if (Life.IsArray() && Life.Size() >= 2)
		{
			const double Min = (std::max)(1.0, Life.At(0).AsDouble());
			OutMs = 0.5 * (Min + (std::max)(Min, Life.At(1).AsDouble()));
			return true;
		}
		const double Preset = PresetMeanLifeMs(Layer.Get("emitter").AsString());
		if (Preset > 0.0)
		{
			OutMs = Preset;
			return true;
		}
		return false;
	}

	/** Life assumed for a steady layer with neither `lifeMs` nor a known preset (vfxbudget_test: no shipped layer). */
	inline constexpr double UnknownLifeMs = 1000.0;

	/** Live particles a recipe's loops keep up at this interval multiplier (rate / multiplier x mean life, summed). */
	inline double SteadyParticles(const abyss::JsonValue& Recipe, double FrequencyMultiplier)
	{
		const double Multiplier = (std::max)(MinFrequencyMultiplier, FrequencyMultiplier);
		double Total = 0.0;
		for (const abyss::JsonValue& Layer : Recipe.Get("layers").Items())
		{
			if (!IsSteadyLayer(Layer))
			{
				continue;
			}
			double LifeMs = UnknownLifeMs;
			LayerMeanLifeMs(Layer, LifeMs);
			const double Rate = (std::min)(MaxLayerRate, Layer.Get("rate").AsDouble(0.0));
			Total += Rate / Multiplier * LifeMs / 1000.0;
		}
		return Total;
	}

	/** vfx_recipes.json's recipe of this id (through `aliases`), else nullptr. */
	inline const abyss::JsonValue* FindRecipe(const abyss::JsonValue& Root, std::string_view Id)
	{
		if (const abyss::JsonValue* Own = Root.Get("recipes").Find(Id))
		{
			return Own;
		}
		if (const abyss::JsonValue* Alias = Root.Get("aliases").Find(Id); Alias != nullptr && Alias->IsString())
		{
			return Root.Get("recipes").Find(Alias->AsString());
		}
		return nullptr;
	}

	// =================================================================================================================
	// What the zone camera can show (the cull radius must exceed it)
	// =================================================================================================================

	/**
	 * The farthest ground distance (cm) from the camera focus at which something up to MaxHeightCm above the ground can
	 * be on screen, for AAbyssCameraRig's framing: FovHDeg across a 16:9 reference (wider screens keep the vertical
	 * extent, narrower ones the horizontal), PitchDeg below the horizon (negative = down), DistanceCm from the focus.
	 * The far ground corner (a ground point leaves the top edge farthest out) and the near corner of the tallest point
	 * (it pokes up from the bottom edge) bound every height in between. Infinity when the top edge sees the horizon.
	 */
	inline double FarthestVisibleCm(double FovHDeg, double PitchDeg, double DistanceCm, double Aspect, double MaxHeightCm)
	{
		constexpr double ReferenceAspect = 16.0 / 9.0;
		const double Deg = Pi / 180.0;
		const double Pitch = std::fabs(PitchDeg) * Deg;
		const double TanHalfH = std::tan(0.5 * FovHDeg * Deg);
		const double TanHalfRef = Aspect > ReferenceAspect ? TanHalfH / ReferenceAspect * Aspect : TanHalfH;
		const double TanHalfV = TanHalfRef / Aspect;
		const double TanHalfLat = TanHalfRef;
		const double HalfV = std::atan(TanHalfV);
		if (Pitch - HalfV <= 0.0)
		{
			return std::numeric_limits<double>::infinity();
		}
		const double CameraHeight = DistanceCm * std::sin(Pitch);
		const double CameraBack = DistanceCm * std::cos(Pitch);   // horizontal distance from the camera's foot to the focus
		auto Corner = [&](double HorizontalFromFoot, double HeightCm)
		{
			// Depth along the view axis of a point at that horizontal distance and height; its half width on screen there.
			const double Depth = HorizontalFromFoot * std::cos(Pitch) + (CameraHeight - HeightCm) * std::sin(Pitch);
			const double Lateral = Depth * TanHalfLat;
			const double Along = HorizontalFromFoot - CameraBack;
			return std::sqrt(Along * Along + Lateral * Lateral);
		};
		const double FarGround = CameraHeight / std::tan(Pitch - HalfV);
		const double Tall = (std::max)(0.0, (std::min)(MaxHeightCm, CameraHeight * 0.99));
		const double NearTall = (CameraHeight - Tall) / std::tan(Pitch + HalfV);
		return (std::max)(Corner(FarGround, 0.0), Corner(NearTall, Tall));
	}
}
