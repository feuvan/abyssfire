// The dark remake's presentation-theme and mood rules (implementation-plan.md 4 U1; dark-remake README, chapter1.md 9 /
// 15, DECISIONS DR5 / R18): which art a zone with a presentation theme shows (`<gameId>@<theme>` before `<gameId>`), the
// terrain style it paints, the mood it is lit with (its base mood, the dawn mood once the chapter is complete, a mood
// region blended in near its centre), the ambience and light scale that mood brings, the monsters' wind-up tint, the
// echo lore look and the brightness setting.
//
// UE-free and header-only (std + AbyssCore public headers; no CoreMinimal, no UE macros), so the rules are unit-tested
// outside the engine (unreal/PresentationTests, theme_test.cpp) and the macro shim compiles them after every UE / Windows
// macro. The UE glue (AAbyssZoneActor, UAbyssWorldBuilder, FAbyssArtManifest, AAbyssCharacterActor, FAbyssUiContext,
// FAbyssUserSettings) converts FName / FLinearColor at the boundary and keeps no rule of its own.
//
// Gating (chapters 2-5, Ember Tower, labyrinth floors look exactly as before): a map without a presentation theme
// expands no id, keeps its generator theme's terrain style, has no dawn mood and no tint override; a mood without
// `port.lightScale` keeps the R18 formula (the caller keeps its own); a mood without `port.windupTint` keeps the
// web-port wind-up tints; only a map's `regions` blend.
#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Json.h"
#include "abyss/data/MapData.h"

namespace AbyssThemeRules
{
	// =================================================================================================================
	// Presentation theme and themed game ids
	// =================================================================================================================

	/** `<gameId>@<theme>`: the art manifest's theme variant of a game id (Art/blender/README.md, A1 `@theme` ids). */
	inline constexpr char ThemeSeparator = '@';

	/** The map's presentation theme name ("march"), or empty when it has none (every web zone, ember_tower, floors). */
	inline std::string_view ThemeOf(const abyss::MapDef* Map)
	{
		return Map != nullptr && Map->hasPresentationTheme ? abyss::EnumName(Map->presentationTheme) : std::string_view();
	}

	/**
	 * Whether a camp prop is placed (1C review F3): in a zone with a presentation theme the hub camp (the map's first
	 * camp: Knellwatch) wears the theme's whole camp kit, while another camp (Crowmarket, the Gleaners' market) keeps the
	 * theme's generic pieces (well, supply crates, ration sacks, braziers, the hearth as its cookfire) but not the hub's
	 * identity: camp_banner@march is the fort's own bell standard, so its banners are left out (non-blocking: no
	 * walkability change). A Gleaners banner / salvage carts are an art-lane handoff (implementation-plan U4 "Not
	 * placed"). An unthemed zone places every prop, as today.
	 */
	inline bool PlacesCampProp(std::string_view Theme, int CampIndex, std::string_view PropType)
	{
		return Theme.empty() || CampIndex == 0 || PropType != "banner";
	}

	/**
	 * Zone loops that, in a zone with a presentation theme, play only their `@theme` recipe and never fall back to
	 * today's (1C review F8): the sealed exit's crimson ground ring and runes (exit.sealed, palette `blood`) would read as
	 * an enemy telegraph (DECISIONS DR5 keeps crimson ground decals for those) next to the march's wagon barricade, which
	 * carries the seal by itself. Until A12 ships exit.sealed@march (smoke / ash at the barricade) nothing loops there.
	 * Unthemed zones keep today's loop.
	 */
	inline bool ThemedOnlyRecipe(std::string_view RecipeId)
	{
		return RecipeId == "exit.sealed";
	}

	/** `<Id>@<Theme>`; the plain id when Theme is empty. */
	inline std::string ThemedId(std::string_view Id, std::string_view Theme)
	{
		std::string Out(Id);
		if (!Theme.empty())
		{
			Out += ThemeSeparator;
			Out += Theme;
		}
		return Out;
	}

	/** Splits `X@theme` into its base id and theme; false (outputs untouched) for a plain id or a malformed one. */
	inline bool SplitThemedId(std::string_view Id, std::string_view& OutBase, std::string_view& OutTheme)
	{
		const size_t At = Id.find(ThemeSeparator);
		if (At == std::string_view::npos || At == 0 || At + 1 >= Id.size() || Id.find(ThemeSeparator, At + 1) != std::string_view::npos)
		{
			return false;
		}
		OutBase = Id.substr(0, At);
		OutTheme = Id.substr(At + 1);
		return true;
	}

	/**
	 * The lookup order of a fallback chain under a theme: every id's theme variant first (in chain order), then the plain
	 * chain. A chain such as {decor_tree, tree} must not stop at the plain decor_tree when tree@march exists, so the
	 * themed pass covers the whole chain before any plain id. No theme: the chain unchanged.
	 */
	inline std::vector<std::string> ExpandThemed(const std::vector<std::string>& Ids, std::string_view Theme)
	{
		std::vector<std::string> Out;
		Out.reserve(Ids.size() * 2);
		if (!Theme.empty())
		{
			for (const std::string& Id : Ids)
			{
				Out.push_back(ThemedId(Id, Theme));
			}
		}
		for (const std::string& Id : Ids)
		{
			Out.push_back(Id);
		}
		return Out;
	}

	/**
	 * ExpandThemed's order without building strings: Themed(id) for every id (when bThemed), then Plain(id) for every id;
	 * the first non-null result wins. ResultT is a pointer type (the UE glue passes manifest lookups on FNames).
	 */
	template <class ResultT, class RangeT, class ThemedFnT, class PlainFnT>
	ResultT ResolveThemedFirst(const RangeT& Ids, bool bThemed, ThemedFnT&& Themed, PlainFnT&& Plain)
	{
		if (bThemed)
		{
			for (const auto& Id : Ids)
			{
				if (ResultT Found = Themed(Id))
				{
					return Found;
				}
			}
		}
		for (const auto& Id : Ids)
		{
			if (ResultT Found = Plain(Id))
			{
				return Found;
			}
		}
		return ResultT{};
	}

	/**
	 * The terrain style / painted terrain material theme (terrain_styles.json themes.<name>, MI_AF_Terrain_<Name>): the
	 * presentation theme when the map has one, else the generator theme. Id patterns built from the generator theme
	 * (outcrop_plains, palisade_plains, lily_pad_plains) keep the generator name: their theme variants hang off it
	 * (outcrop_plains@march).
	 */
	inline std::string_view TerrainThemeName(const abyss::MapDef* Map, std::string_view GeneratorThemeName)
	{
		const std::string_view Theme = ThemeOf(Map);
		return Theme.empty() ? GeneratorThemeName : Theme;
	}

	// =================================================================================================================
	// Moods: base, dawn, regions, blend
	// =================================================================================================================

	/** A mood region ramps in over its last RegionRampTiles inside `radius` (smoothstep), full weight further in. */
	inline constexpr double RegionRampTiles = 4.0;
	/** The region's mood owns the ambience / tint from this weight on, and gives it back below RegionLeaveWeight. */
	inline constexpr double RegionEnterWeight = 0.5;
	inline constexpr double RegionLeaveWeight = 0.4;
	/** The UE glue re-applies the light / fog / post-process only when the blend moved by more than this. */
	inline constexpr double MoodReapplyEpsilon = 0.01;
	/** DECISIONS R18 floor of the fake-light scale (AbyssVfxLighting::MinLightScale). */
	inline constexpr float MinLightScale = 0.12f;
	/** Suffix of the dawn mood of a presentation theme (march -> march_dawn; W2 replaces the convention by a flag). */
	inline constexpr std::string_view DawnSuffix = "_dawn";

	/** 0xRRGGBB per-channel lerp (8-bit, rounded); T = 0 / 1 return A / B exactly. */
	inline uint32_t LerpRgb(uint32_t A, uint32_t B, double T)
	{
		if (T <= 0.0)
		{
			return A;
		}
		if (T >= 1.0)
		{
			return B;
		}
		uint32_t Out = 0;
		for (int Shift = 16; Shift >= 0; Shift -= 8)
		{
			const double Ca = static_cast<double>((A >> Shift) & 0xFFu);
			const double Cb = static_cast<double>((B >> Shift) & 0xFFu);
			const double C = std::floor(Ca + (Cb - Ca) * T + 0.5);
			Out |= static_cast<uint32_t>(C < 0.0 ? 0.0 : (C > 255.0 ? 255.0 : C)) << Shift;
		}
		return Out;
	}

	inline double LerpD(double A, double B, double T)
	{
		return T <= 0.0 ? A : (T >= 1.0 ? B : A + (B - A) * T);
	}

	/**
	 * A mood between A and B (every colour per channel in sRGB bytes, every number linearly). T <= 0 returns A, T >= 1
	 * returns B, field for field. The result keeps A's theme / flags; lightScale and moodWeather are not blended here
	 * (EffectiveMood blends the light scale and picks the dominant mood's weather).
	 */
	inline abyss::ZoneMoodDef LerpMood(const abyss::ZoneMoodDef& A, const abyss::ZoneMoodDef& B, double T)
	{
		if (T <= 0.0)
		{
			return A;
		}
		if (T >= 1.0)
		{
			return B;
		}
		abyss::ZoneMoodDef Out = A;
		Out.ambient = LerpRgb(A.ambient, B.ambient, T);
		Out.ambientAlpha = LerpD(A.ambientAlpha, B.ambientAlpha, T);
		Out.vignette = LerpRgb(A.vignette, B.vignette, T);
		Out.vignetteAlpha = LerpD(A.vignetteAlpha, B.vignetteAlpha, T);
		Out.haze = LerpRgb(A.haze, B.haze, T);
		Out.hazeAlpha = LerpD(A.hazeAlpha, B.hazeAlpha, T);
		Out.saturation = LerpD(A.saturation, B.saturation, T);
		Out.contrast = LerpD(A.contrast, B.contrast, T);
		for (size_t I = 0; I < 3; ++I)
		{
			Out.lift[I] = LerpD(A.lift[I], B.lift[I], T);
			Out.gain[I] = LerpD(A.gain[I], B.gain[I], T);
		}
		Out.fogColor = LerpRgb(A.fogColor, B.fogColor, T);
		Out.fogMaxOpacity = LerpD(A.fogMaxOpacity, B.fogMaxOpacity, T);
		Out.fogDensity = LerpD(A.fogDensity, B.fogDensity, T);
		return Out;
	}

	/**
	 * DECISIONS R18's formula (the web lightScale): max(0.12, ambientAlpha x (1 - luma601(ambient)) x 1.4), the same
	 * arithmetic as AbyssVfxLighting::LightScaleFromMood (float, sRGB 0..1 channels).
	 */
	inline float FormulaLightScale(uint32_t AmbientRgb, double AmbientAlpha)
	{
		const float R = static_cast<float>((AmbientRgb >> 16) & 0xFFu) / 255.f;
		const float G = static_cast<float>((AmbientRgb >> 8) & 0xFFu) / 255.f;
		const float B = static_cast<float>(AmbientRgb & 0xFFu) / 255.f;
		const float Luma = 0.299f * R + 0.587f * G + 0.114f * B;
		const float Scale = static_cast<float>(AmbientAlpha) * (1.f - Luma) * 1.4f;
		return Scale > MinLightScale ? Scale : MinLightScale;
	}

	/** A mood's fake-light scale: its `port.lightScale` override when > 0, else the formula. */
	inline float MoodLightScale(const abyss::ZoneMoodDef& Mood)
	{
		return Mood.lightScale > 0.0 ? static_cast<float>(Mood.lightScale) : FormulaLightScale(Mood.ambient, Mood.ambientAlpha);
	}

	/** The zone's base mood: zone_moods.json themeByZone, else Fallback (the zone's generator theme), as today. */
	inline abyss::MapTheme BaseMood(const abyss::ZoneMoodTables& Moods, std::string_view ZoneId, abyss::MapTheme Fallback)
	{
		abyss::MapTheme Out = Fallback;
		Moods.ThemeFor(ZoneId, Out);
		return Out;
	}

	/** The presentation theme's dawn mood (`<theme>_dawn`) when it is a defined mood; false otherwise. */
	inline bool FindDawnMood(const abyss::ZoneMoodTables& Moods, const abyss::MapDef* Map, abyss::MapTheme& Out)
	{
		const std::string_view Theme = ThemeOf(Map);
		if (Theme.empty())
		{
			return false;
		}
		std::string Name(Theme);
		Name += DawnSuffix;
		abyss::MapTheme Dawn = abyss::MapTheme::Plains;
		if (!abyss::ParseEnum(Name, Dawn) || !Moods.moods[static_cast<size_t>(Dawn)].defined)
		{
			return false;
		}
		Out = Dawn;
		return true;
	}

	/** The mood the zone is lit with outside its regions: the dawn mood once the chapter is complete, else Base. */
	inline abyss::MapTheme DawnMood(const abyss::ZoneMoodTables& Moods, const abyss::MapDef* Map, abyss::MapTheme Base,
		bool bChapterComplete)
	{
		abyss::MapTheme Dawn = Base;
		return bChapterComplete && FindDawnMood(Moods, Map, Dawn) ? Dawn : Base;
	}

	/** Smoothstep on [0, 1]. */
	inline double SmoothStep01(double X)
	{
		const double T = X <= 0.0 ? 0.0 : (X >= 1.0 ? 1.0 : X);
		return T * T * (3.0 - 2.0 * T);
	}

	/**
	 * Weight of a mood region at a tile: 1 within radius - RampTiles of the centre, 0 at radius and beyond, a smoothstep
	 * between (a radius under RampTiles ramps over the whole radius).
	 */
	inline double RegionWeight(double Col, double Row, const abyss::MapRegionDef& Region, double RampTiles = RegionRampTiles)
	{
		if (Region.radius <= 0.0)
		{
			return 0.0;
		}
		const double Dx = Col - static_cast<double>(Region.center.col);
		const double Dy = Row - static_cast<double>(Region.center.row);
		const double Distance = std::sqrt(Dx * Dx + Dy * Dy);
		const double Ramp = RampTiles > 0.0 ? (RampTiles < Region.radius ? RampTiles : Region.radius) : 0.0;
		if (Distance >= Region.radius)
		{
			return 0.0;
		}
		if (Ramp <= 0.0 || Distance <= Region.radius - Ramp)
		{
			return 1.0;
		}
		return SmoothStep01((Region.radius - Distance) / Ramp);
	}

	struct FRegionPick
	{
		const abyss::MapRegionDef* Region = nullptr;
		double Weight = 0.0;
	};

	/** The region with the highest weight at the tile (the first one on a tie); none when every weight is 0. */
	inline FRegionPick DominantRegion(const abyss::MapDef* Map, double Col, double Row)
	{
		FRegionPick Pick;
		if (Map == nullptr)
		{
			return Pick;
		}
		for (const abyss::MapRegionDef& Region : Map->regions)
		{
			const double Weight = RegionWeight(Col, Row, Region);
			if (Weight > Pick.Weight)
			{
				Pick.Region = &Region;
				Pick.Weight = Weight;
			}
		}
		return Pick;
	}

	/**
	 * The zone's own weather as AAbyssZoneActor has always resolved it (world-map-nav.md 14.2): its zone_moods.json
	 * weather entry, else the entry of the first zone sharing its mood theme.
	 */
	inline const abyss::ZoneWeatherDef* ZoneWeather(const abyss::ZoneMoodTables& Moods, std::string_view ZoneId,
		abyss::MapTheme MoodTheme)
	{
		if (const abyss::ZoneWeatherDef* Own = Moods.WeatherFor(ZoneId))
		{
			return Own;
		}
		for (const std::pair<std::string, abyss::MapTheme>& Entry : Moods.themeByZone)
		{
			if (Entry.second == MoodTheme)
			{
				if (const abyss::ZoneWeatherDef* Shared = Moods.WeatherFor(Entry.first))
				{
					return Shared;
				}
			}
		}
		return nullptr;
	}

	struct FMoodInputs
	{
		const abyss::ZoneMoodTables* Moods = nullptr;
		const abyss::MapDef* Map = nullptr;   // nullptr for places without map data (labyrinth floors)
		std::string_view ZoneId;
		abyss::MapTheme GeneratorTheme = abyss::MapTheme::Plains;   // Snapshot::zone.theme
		bool bChapterComplete = false;                               // chapterCompleteQuest turned in
		bool bHasHero = false;
		double HeroCol = 0.0;
		double HeroRow = 0.0;
		/** The previous frame's state owned the ambience by its region (hysteresis RegionEnter / LeaveWeight). */
		bool bRegionWasDominant = false;
	};

	struct FMoodState
	{
		abyss::MapTheme Base = abyss::MapTheme::Plains;      // base or dawn mood
		abyss::MapTheme Region = abyss::MapTheme::Plains;    // the dominant region's mood (when bHasRegion)
		bool bHasRegion = false;
		double RegionWeight = 0.0;
		bool bRegionDominant = false;                        // the region owns the ambience / tint (hysteresis)
		abyss::MapTheme Dominant = abyss::MapTheme::Plains;  // Region when bRegionDominant, else Base
		/** The blended mood (Base -> Region by RegionWeight). */
		abyss::ZoneMoodDef Mood;
		/** Blended fake-light scale; bLightScaleOverride: a contributing mood carries port.lightScale. */
		float LightScale = MinLightScale;
		bool bLightScaleOverride = false;
		/** Ambience of the dominant mood's port.moodWeather, else the zone's weather ("" = none). */
		std::string Ambience;
		/** Weather tint for the ambience recipe's colour (only on maps with a presentation theme). */
		bool bHasTint = false;
		uint32_t Tint = 0;
	};

	/**
	 * The mood a zone shows at the hero's tile: Base = the zone's mood (BaseMood), or its dawn mood after the chapter;
	 * the strongest region blends its mood in by RegionWeight; the dominant mood (the region from RegionEnterWeight, back
	 * below RegionLeaveWeight) brings its port.moodWeather ambience / tint, else the zone keeps its own weather. A map
	 * without regions and presentation theme yields exactly today's mood, ambience and no tint.
	 */
	inline FMoodState EffectiveMood(const FMoodInputs& In)
	{
		FMoodState Out;
		if (In.Moods == nullptr)
		{
			return Out;
		}
		const abyss::ZoneMoodTables& Moods = *In.Moods;
		const abyss::MapTheme ZoneMood = BaseMood(Moods, In.ZoneId, In.GeneratorTheme);
		Out.Base = DawnMood(Moods, In.Map, ZoneMood, In.bChapterComplete);
		Out.Region = Out.Base;
		if (In.bHasHero)
		{
			const FRegionPick Pick = DominantRegion(In.Map, In.HeroCol, In.HeroRow);
			if (Pick.Region != nullptr && Moods.moods[static_cast<size_t>(Pick.Region->mood)].defined)
			{
				Out.bHasRegion = true;
				Out.Region = Pick.Region->mood;
				Out.RegionWeight = Pick.Weight;
			}
		}
		const abyss::ZoneMoodDef& BaseDef = Moods.moods[static_cast<size_t>(Out.Base)];
		const abyss::ZoneMoodDef& RegionDef = Moods.moods[static_cast<size_t>(Out.Region)];
		const double W = Out.bHasRegion ? Out.RegionWeight : 0.0;
		Out.Mood = LerpMood(BaseDef, RegionDef, W);
		Out.Mood.theme = W >= 0.5 ? Out.Region : Out.Base;

		const float BaseScale = MoodLightScale(BaseDef);
		const float RegionScale = MoodLightScale(RegionDef);
		Out.LightScale = W <= 0.0 ? BaseScale : static_cast<float>(LerpD(BaseScale, RegionScale, W));
		Out.bLightScaleOverride = BaseDef.lightScale > 0.0 || (W > 0.0 && RegionDef.lightScale > 0.0);

		Out.bRegionDominant = Out.bHasRegion
			&& (In.bRegionWasDominant ? Out.RegionWeight >= RegionLeaveWeight : Out.RegionWeight >= RegionEnterWeight);
		Out.Dominant = Out.bRegionDominant ? Out.Region : Out.Base;

		const bool bPresentation = !ThemeOf(In.Map).empty();
		const abyss::ZoneMoodDef& DominantDef = Moods.moods[static_cast<size_t>(Out.Dominant)];
		const abyss::ZoneWeatherDef* Weather = nullptr;
		if (DominantDef.hasMoodWeather)
		{
			Weather = &DominantDef.moodWeather;
		}
		else
		{
			Weather = ZoneWeather(Moods, In.ZoneId, ZoneMood);
		}
		if (Weather != nullptr)
		{
			Out.Ambience = Weather->ambience;
			Out.bHasTint = bPresentation && Weather->hasTint && !Weather->ambience.empty();
			Out.Tint = Out.bHasTint ? Weather->tint : 0u;
		}
		return Out;
	}

	/** `ambient.<ambience>` (the VFX recipe), or empty without an ambience. */
	inline std::string AmbienceRecipeId(const FMoodState& State)
	{
		return State.Ambience.empty() ? std::string() : "ambient." + State.Ambience;
	}

	/** True when the look must be re-applied: another base / region mood, or a weight change beyond the epsilon. */
	inline bool LookChanged(const FMoodState& Applied, const FMoodState& Now)
	{
		if (Applied.Base != Now.Base || Applied.bHasRegion != Now.bHasRegion || (Now.bHasRegion && Applied.Region != Now.Region))
		{
			return true;
		}
		const double Delta = Now.RegionWeight - Applied.RegionWeight;
		// The ends always land exactly (a fast walk out of a region would otherwise stop at weight .008).
		const bool bEndReached = (Now.RegionWeight <= 0.0 || Now.RegionWeight >= 1.0) && Delta != 0.0;
		return bEndReached || Delta > MoodReapplyEpsilon || Delta < -MoodReapplyEpsilon;
	}

	/** True when the ambience recipe or its colour changed (the looping VFX is swapped). */
	inline bool AmbienceChanged(const FMoodState& Applied, const FMoodState& Now)
	{
		return Applied.Ambience != Now.Ambience || Applied.bHasTint != Now.bHasTint || Applied.Tint != Now.Tint;
	}

	// =================================================================================================================
	// Wind-up tint (DECISIONS DR5): zone_moods.json port.windupTint.<mood>.{normal, elite}
	// =================================================================================================================

	/** The web port's wind-up tints (art-inventory-ch1.md 1.8): every mood without a windupTint entry keeps them. */
	inline constexpr uint32_t DefaultWindupNormal = 0xFFC4B0u;
	inline constexpr uint32_t DefaultWindupElite = 0xFF7A5Cu;

	/** 0xRRGGBB from a JSON number or "#RRGGBB" / "RRGGBB" string; false otherwise. */
	inline bool ParseRgb(const abyss::JsonValue& Value, uint32_t& Out)
	{
		if (Value.IsNumber() && Value.IsInteger())
		{
			const int64_t Number = Value.AsInt64(-1);
			if (Number >= 0 && Number <= 0xFFFFFF)
			{
				Out = static_cast<uint32_t>(Number);
				return true;
			}
			return false;
		}
		if (!Value.IsString())
		{
			return false;
		}
		std::string_view Text = Value.AsString();
		if (!Text.empty() && Text.front() == '#')
		{
			Text.remove_prefix(1);
		}
		if (Text.size() != 6)
		{
			return false;
		}
		uint32_t Parsed = 0;
		for (const char C : Text)
		{
			uint32_t Digit = 0;
			if (C >= '0' && C <= '9')
			{
				Digit = static_cast<uint32_t>(C - '0');
			}
			else if (C >= 'a' && C <= 'f')
			{
				Digit = static_cast<uint32_t>(C - 'a' + 10);
			}
			else if (C >= 'A' && C <= 'F')
			{
				Digit = static_cast<uint32_t>(C - 'A' + 10);
			}
			else
			{
				return false;
			}
			Parsed = (Parsed << 4) | Digit;
		}
		Out = Parsed;
		return true;
	}

	struct FWindupTints
	{
		uint32_t Normal = DefaultWindupNormal;
		uint32_t Elite = DefaultWindupElite;
		bool bFromData = false;
	};

	/**
	 * The wind-up tints of a mood: port.windupTint.<Mood> of the raw zone_moods.json, else of FallbackMood, else the
	 * defaults. A missing `elite` keeps the default elite tint; a malformed colour is ignored.
	 */
	inline FWindupTints WindupTintFor(const abyss::JsonValue* RawZoneMoods, std::string_view Mood, std::string_view FallbackMood)
	{
		FWindupTints Out;
		if (RawZoneMoods == nullptr)
		{
			return Out;
		}
		const abyss::JsonValue& Table = RawZoneMoods->Get("port").Get("windupTint");
		for (const std::string_view Name : { Mood, FallbackMood })
		{
			if (Name.empty())
			{
				continue;
			}
			const abyss::JsonValue* Entry = Table.Find(Name);
			if (Entry == nullptr || !Entry->IsObject())
			{
				continue;
			}
			uint32_t Normal = 0;
			if (!ParseRgb(Entry->Get("normal"), Normal))
			{
				continue;
			}
			Out.Normal = Normal;
			uint32_t Elite = 0;
			if (ParseRgb(Entry->Get("elite"), Elite))
			{
				Out.Elite = Elite;
			}
			Out.bFromData = true;
			return Out;
		}
		return Out;
	}

	// =================================================================================================================
	// Echo lore (manifest materials.MI_AF_Echo): a translucent figure frozen in its last pose for 3 s
	// =================================================================================================================

	/** Lore objects of this sprite type are echoes (GameData loreCollectibles lore_rf_echo_*). */
	inline constexpr std::string_view EchoSpriteType = "echo";

	inline bool IsEchoSpriteType(std::string_view SpriteType)
	{
		return SpriteType == EchoSpriteType;
	}

	/** Echo pool / tint colour (Props_March echo.body) when the manifest gives none. */
	inline constexpr uint32_t DefaultEchoRgb = 0xA8B0C4u;

	struct FEchoTimeline
	{
		double FadeInMs = 450.0;
		double HoldMs = 2100.0;
		double FadeOutMs = 450.0;
		double FlickerAmplitude = 0.08;
		double FlickerHz = 4.0;

		double TotalMs() const { return FadeInMs + HoldMs + FadeOutMs; }
	};

	/** The hold's flicker added to Fade: Amplitude x (1 - cos(2 pi Hz t)) / 2, in [0, Amplitude], 0 at the hold's start. */
	inline double EchoFlicker(const FEchoTimeline& Timeline, double HoldElapsedMs)
	{
		constexpr double TwoPi = 6.283185307179586;
		return Timeline.FlickerAmplitude * 0.5 * (1.0 - std::cos(TwoPi * Timeline.FlickerHz * HoldElapsedMs / 1000.0));
	}

	/**
	 * CPD Fade of the echo figure at TMs since it appeared (0 = opaque .. 1 = invisible; WorldContract AbyssCpd::Fade):
	 * 1 -> 0 over FadeInMs (ease out), the flicker during HoldMs, then from the hold's last value to 1 over FadeOutMs
	 * (ease in), 1 from TotalMs on. Continuous everywhere.
	 */
	inline double EchoFade(const FEchoTimeline& Timeline, double TMs)
	{
		if (TMs <= 0.0)
		{
			return 1.0;
		}
		if (TMs < Timeline.FadeInMs)
		{
			const double U = TMs / Timeline.FadeInMs;
			return (1.0 - U) * (1.0 - U);   // 1 - easeOut(U), easeOut = 1 - (1 - U)^2
		}
		const double HoldEnd = Timeline.FadeInMs + Timeline.HoldMs;
		if (TMs < HoldEnd)
		{
			return EchoFlicker(Timeline, TMs - Timeline.FadeInMs);
		}
		if (TMs >= Timeline.TotalMs() || Timeline.FadeOutMs <= 0.0)
		{
			return 1.0;
		}
		const double From = EchoFlicker(Timeline, Timeline.HoldMs);
		const double U = (TMs - HoldEnd) / Timeline.FadeOutMs;
		return From + (1.0 - From) * U * U;   // easeIn
	}

	/**
	 * One figure of an echo, in the game's frame (A10's preview frame: Blender metres around the echo tile, mesh facing
	 * Blender -Y = UE +Y, Blender Z rotation = yawDeg): UE offset (x, -y) metres = tiles, and the mesh turned by
	 * MeshYawDeg = -yawDeg (FacingYawDeg = 90 - yawDeg is the direction the figure looks, UE yaw).
	 */
	struct FEchoFigure
	{
		std::string Asset;       // skeletal mesh asset (SK_NPC_*_March)
		std::string Clip;        // clip name (Death, Cower)
		std::string ClipAsset;   // the asset whose clip list holds Clip (same skeleton); empty = Asset
		int Frame = 0;           // frame of the clip the figure is frozen at
		double OffsetCol = 0.0;  // tiles from the echo tile, UE X
		double OffsetRow = 0.0;  // tiles from the echo tile, UE Y
		double MeshYawDeg = 0.0;
		double FacingYawDeg = 90.0;
	};

	struct FEchoEntry
	{
		std::string LoreId;
		bool bHasTile = false;
		int TileCol = 0;
		int TileRow = 0;
		std::vector<FEchoFigure> Figures;
	};

	struct FEchoLook
	{
		bool bValid = false;
		FEchoTimeline Timeline;
		uint32_t TintRgb = DefaultEchoRgb;   // params.Tint.hex (the pool colour)
		float TintLinear[3] = { 0.1958f, 0.2171f, 0.276f };   // CPD Tint (linear x scale), params.Tint.linear
		float Ghost = 1.f;                   // CPD Ghost, params.Ghost.value
		std::vector<FEchoEntry> Entries;

		const FEchoEntry* Find(std::string_view LoreId) const
		{
			for (const FEchoEntry& Entry : Entries)
			{
				if (Entry.LoreId == LoreId)
				{
					return &Entry;
				}
			}
			return nullptr;
		}
	};

	/** Parses the manifest's materials.MI_AF_Echo (Art/Export/manifest.json via Data/assets.json). */
	inline FEchoLook ParseEchoLook(const abyss::JsonValue& Echo)
	{
		FEchoLook Out;
		if (!Echo.IsObject())
		{
			return Out;
		}
		Out.bValid = true;
		const abyss::JsonValue& Timeline = Echo.Get("timeline");
		Out.Timeline.FadeInMs = Timeline.Get("fadeInMs").AsDouble(Out.Timeline.FadeInMs);
		Out.Timeline.HoldMs = Timeline.Get("holdMs").AsDouble(Out.Timeline.HoldMs);
		Out.Timeline.FadeOutMs = Timeline.Get("fadeOutMs").AsDouble(Out.Timeline.FadeOutMs);
		Out.Timeline.FlickerAmplitude = Timeline.Get("flicker").Get("amplitude").AsDouble(Out.Timeline.FlickerAmplitude);
		Out.Timeline.FlickerHz = Timeline.Get("flicker").Get("hz").AsDouble(Out.Timeline.FlickerHz);
		const abyss::JsonValue& Params = Echo.Get("params");
		ParseRgb(Params.Get("Tint").Get("hex"), Out.TintRgb);
		const abyss::JsonValue& Linear = Params.Get("Tint").Get("linear");
		if (Linear.IsArray() && Linear.Size() == 3)
		{
			for (size_t I = 0; I < 3; ++I)
			{
				Out.TintLinear[I] = static_cast<float>(Linear.At(I).AsDouble(Out.TintLinear[I]));
			}
		}
		Out.Ghost = static_cast<float>(Params.Get("Ghost").Get("value").AsDouble(Out.Ghost));
		for (const abyss::JsonMember& Member : Echo.Get("figures").Members())
		{
			FEchoEntry& Entry = Out.Entries.emplace_back();
			Entry.LoreId = std::string(Member.key);
			const abyss::JsonValue& Tile = Member.value.Get("tile");
			if (Tile.IsArray() && Tile.Size() == 2 && Tile.At(0).IsNumber() && Tile.At(1).IsNumber())
			{
				Entry.bHasTile = true;
				Entry.TileCol = static_cast<int>(Tile.At(0).AsDouble(0.0));
				Entry.TileRow = static_cast<int>(Tile.At(1).AsDouble(0.0));
			}
			for (const abyss::JsonValue& Figure : Member.value.Get("figures").Items())
			{
				FEchoFigure F;
				F.Asset = std::string(Figure.Get("asset").AsString());
				F.Clip = std::string(Figure.Get("clip").AsString());
				F.ClipAsset = std::string(Figure.Get("clipAsset").AsString());
				F.Frame = static_cast<int>(Figure.Get("frame").AsDouble(0.0));
				const abyss::JsonValue& Offset = Figure.Get("offsetM");
				F.OffsetCol = Offset.At(0).AsDouble(0.0);
				F.OffsetRow = -Offset.At(1).AsDouble(0.0);
				const double YawDeg = Figure.Get("yawDeg").AsDouble(0.0);
				F.MeshYawDeg = -YawDeg;
				F.FacingYawDeg = 90.0 - YawDeg;
				if (!F.Asset.empty())
				{
					Entry.Figures.push_back(std::move(F));
				}
			}
		}
		return Out;
	}

	/** Seconds into a clip of a frame (the clip's fps, else the manifest's units.fps, else 60). */
	inline double ClipFrameSec(int Frame, double Fps)
	{
		const double Rate = Fps > 0.0 ? Fps : 60.0;
		return Frame > 0 ? static_cast<double>(Frame) / Rate : 0.0;
	}

	// =================================================================================================================
	// Brightness (settings.json video.brightness; exposure bias on the zone post-process)
	// =================================================================================================================

	inline constexpr float BrightnessMin = 0.5f;
	inline constexpr float BrightnessMax = 1.5f;
	inline constexpr float BrightnessDefault = 1.f;

	/** Clamped to [BrightnessMin, BrightnessMax]; a non-finite value is the default. */
	inline float ClampBrightness(float Brightness)
	{
		if (!std::isfinite(Brightness))
		{
			return BrightnessDefault;
		}
		return Brightness < BrightnessMin ? BrightnessMin : (Brightness > BrightnessMax ? BrightnessMax : Brightness);
	}

	/** The exposure bias (EV) the brightness adds to the zone post-process: log2(brightness) (1 -> 0, 0.5 -> -1). */
	inline float BrightnessToEv(float Brightness)
	{
		return static_cast<float>(std::log2(static_cast<double>(ClampBrightness(Brightness))));
	}
}
