// AbyssAmbienceRules: the remake's ambience beyond the zone bed (U7; manifest `ambience`, AU2 dark_ambience.py).
//
// * Bed: the zone row (`zones`), else the theme row (`themes`) of the map's presentation theme when it has one (the
//   dark remake's `march`), else of its generator theme (today's rule for every other zone).
// * Region beds (`regions`): inside the region (geometry and hysteresis of AbyssAudioData) the region's loop fades in
//   over fadeInSec and, with `replacesBed`, the bed fades out by the same weight (a crossfade); leaving reverses it
//   over fadeOutSec. Layers (`layers`): a loop over the bed inside the area while `while` holds.
// * Emitters (`emitters`): looping point sources; one asset per spot (spot i plays assets[i % n]); spots come from the
//   set pieces (UAbyssAudioSystem::SetZoneEmitterSpots), else the placement: the camp slot's centre (`campSlot` of
//   camp `camp`, 1-based, via abyss::CampProps), the `anchor` tile, or the `area` centre (one spot). Volume: full
//   within innerTiles of the hero, then linear in amplitude to silence at innerTiles + falloffTiles.
//
// Pure helper (std + AbyssCore public headers only); tested in PresentationTests/tests/audio_ambience_test.cpp.
#pragma once

#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Json.h"
#include "abyss/data/MapData.h"
#include "abyss/world/MapGen.h"

#include "Audio/AbyssAudioDataRules.h"

namespace AbyssAmbienceRules
{
	/** A region bed or a layer: a loop played inside an area of a zone. */
	struct AreaLoop
	{
		std::string id;
		std::string asset;
		std::string zone;
		AbyssAudioData::TileCircle area;
		AbyssAudioData::WhileCondition when;
		bool replacesBed = false;
		double fadeInSec = 3.0;
		double fadeOutSec = 3.0;
	};

	struct Emitter
	{
		std::string id;
		std::vector<std::string> assets;
		double innerTiles = 1.0;
		double falloffTiles = 6.0;
		std::string zone;
		std::string campSlot;  // camp prop type ("campfire"), with camp (1-based)
		int camp = 0;
		bool hasAnchor = false;
		double anchorCol = 0.0;
		double anchorRow = 0.0;
		bool hasArea = false;
		double areaCol = 0.0;
		double areaRow = 0.0;
	};

	struct AmbienceTables
	{
		std::vector<AreaLoop> regions;
		std::vector<AreaLoop> layers;
		std::vector<Emitter> emitters;
		bool loaded = false;
	};

	inline AreaLoop ReadAreaLoop(const std::string& Id, const abyss::JsonValue& Row, bool bRegion)
	{
		using namespace AbyssAudioData;
		AreaLoop Out;
		Out.id = Id;
		Out.asset = Str(Row.Get("asset"));
		Out.zone = Str(Row.Get("zone"));
		Out.area = ReadCircle(Row);
		Out.when = ReadWhile(Row);
		Out.replacesBed = bRegion && Row.Get("replacesBed").AsBool(false);
		Out.fadeInSec = Num(Row.Get("fadeInSec"), 3.0);
		Out.fadeOutSec = Num(Row.Get("fadeOutSec"), 3.0);
		return Out;
	}

	inline AmbienceTables Parse(const abyss::JsonValue& Ambience)
	{
		using namespace AbyssAudioData;
		AmbienceTables Out;
		if (!Ambience.IsObject())
		{
			return Out;
		}
		for (const abyss::JsonMember& Row : Ambience.Get("regions").Members())
		{
			AreaLoop Loop = ReadAreaLoop(Row.key, Row.value, /*bRegion*/ true);
			if (!Loop.asset.empty() && Loop.area.radius > 0.0)
			{
				Out.regions.push_back(std::move(Loop));
			}
		}
		for (const abyss::JsonMember& Row : Ambience.Get("layers").Members())
		{
			AreaLoop Loop = ReadAreaLoop(Row.key, Row.value, /*bRegion*/ false);
			if (!Loop.asset.empty() && Loop.area.radius > 0.0)
			{
				Out.layers.push_back(std::move(Loop));
			}
		}
		for (const abyss::JsonMember& Row : Ambience.Get("emitters").Members())
		{
			Emitter E;
			E.id = Row.key;
			E.assets = Strings(Row.value.Get("assets"));
			const abyss::JsonValue& Att = Row.value.Get("attenuation");
			E.innerTiles = Num(Att.Get("innerTiles"), 1.0);
			E.falloffTiles = Num(Att.Get("falloffTiles"), 6.0);
			const abyss::JsonValue& Place = Row.value.Get("placement");
			E.zone = Str(Place.Get("zone"));
			E.campSlot = Str(Place.Get("campSlot"));
			E.camp = Place.Get("camp").AsInt(0);
			if (Place.Get("anchor").IsObject())
			{
				E.hasAnchor = true;
				E.anchorCol = Num(Place.Get("anchor").Get("col"), 0.0);
				E.anchorRow = Num(Place.Get("anchor").Get("row"), 0.0);
			}
			if (Place.Get("area").IsObject())
			{
				E.hasArea = true;
				E.areaCol = Num(Place.Get("area").Get("col"), 0.0);
				E.areaRow = Num(Place.Get("area").Get("row"), 0.0);
			}
			if (!E.assets.empty())
			{
				Out.emitters.push_back(std::move(E));
			}
		}
		Out.loaded = true;
		return Out;
	}

	inline std::vector<AbyssAudioData::TileCircle> Areas(const std::vector<AreaLoop>& Loops)
	{
		std::vector<AbyssAudioData::TileCircle> Out;
		Out.reserve(Loops.size());
		for (const AreaLoop& Loop : Loops)
		{
			Out.push_back(Loop.area);
		}
		return Out;
	}

	/** The theme name the bed table is keyed by: the map's presentation theme when it has one, else its theme. */
	inline std::string_view BedThemeName(const abyss::MapDef* Map, std::string_view SnapshotTheme)
	{
		if (Map != nullptr && Map->hasPresentationTheme)
		{
			return abyss::EnumName(Map->presentationTheme);
		}
		return SnapshotTheme;
	}

	/** Emitter volume for the hero's distance (tiles): 1 within inner, linear to 0 at inner + falloff. */
	inline double EmitterGain(double DistanceTiles, double InnerTiles, double FalloffTiles)
	{
		if (DistanceTiles <= InnerTiles)
		{
			return 1.0;
		}
		if (FalloffTiles <= 0.0 || DistanceTiles >= InnerTiles + FalloffTiles)
		{
			return 0.0;
		}
		return 1.0 - (DistanceTiles - InnerTiles) / FalloffTiles;
	}

	struct Spot
	{
		double col = 0.0;
		double row = 0.0;
		std::string asset;
	};

	enum class SpotSource : unsigned char
	{
		None,
		SetPieces,  // spots handed over by the set-piece layer
		CampSlot,
		Anchor,
		AreaCentre,
	};

	/**
	 * Where an emitter plays in a map: the set pieces' spots (one per spot), else the placement. Map may be null (no
	 * camp lookup). Out: the spots with their asset (spot i plays assets[i % n]).
	 */
	inline SpotSource EmitterSpots(const Emitter& E, const std::vector<std::pair<double, double>>& SetPieceSpots,
		const abyss::MapDef* Map, std::vector<Spot>& Out)
	{
		Out.clear();
		if (E.assets.empty())
		{
			return SpotSource::None;
		}
		if (!SetPieceSpots.empty())
		{
			for (std::size_t I = 0; I < SetPieceSpots.size(); ++I)
			{
				Out.push_back(Spot{ SetPieceSpots[I].first, SetPieceSpots[I].second, E.assets[I % E.assets.size()] });
			}
			return SpotSource::SetPieces;
		}
		if (!E.campSlot.empty() && E.camp > 0 && Map != nullptr)
		{
			for (const abyss::CampProp& Prop : abyss::CampProps(*Map))
			{
				if (Prop.campIndex == E.camp - 1 && Prop.type == E.campSlot)
				{
					Out.push_back(Spot{ Prop.centre.x, Prop.centre.y, E.assets[0] });
					return SpotSource::CampSlot;
				}
			}
		}
		if (E.hasAnchor)
		{
			Out.push_back(Spot{ E.anchorCol, E.anchorRow, E.assets[0] });
			return SpotSource::Anchor;
		}
		if (E.hasArea)
		{
			Out.push_back(Spot{ E.areaCol, E.areaRow, E.assets[0] });
			return SpotSource::AreaCentre;
		}
		return SpotSource::None;
	}

	inline double Distance(double Col0, double Row0, double Col1, double Row1)
	{
		const double Dc = Col1 - Col0;
		const double Dr = Row1 - Row0;
		return std::sqrt(Dc * Dc + Dr * Dr);
	}
}
