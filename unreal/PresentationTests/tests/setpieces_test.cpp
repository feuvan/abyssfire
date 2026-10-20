// U4 set-piece presentation rules (Source/Abyssfire/Private/World/AbyssSetPieceRules.h) on synthetic placements and on
// the shipped tables (Data/set_pieces.json placed by abyss/world/SetPieces.h, the art manifest copy Data/assets.json
// for sockets, bounds and the MI_AF_Echo figures, the exported strings). The core's own checks (every piece on its map,
// art resolving, nothing covering the bot's route) live in CoreTests/tests/setpieces_test.cpp.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "doctest/doctest.h"

#include "abyss/base/Enums.h"
#include "abyss/base/I18n.h"
#include "abyss/base/Json.h"
#include "abyss/base/Log.h"
#include "abyss/base/Math.h"
#include "abyss/data/DataStore.h"
#include "abyss/data/MapData.h"
#include "abyss/data/QuestData.h"
#include "abyss/world/MapGen.h"

#include "World/AbyssSetPieceRules.h"
#include "World/AbyssThemeRules.h"

namespace
{
	using namespace AbyssSetPieceRules;

	std::string SpReadBytes(const std::string& Path)
	{
		std::ifstream In(Path, std::ios::binary);
		if (!In)
		{
			return {};
		}
		std::ostringstream Ss;
		Ss << In.rdbuf();
		return Ss.str();
	}

	std::string SpDataDir() { return std::string(ABYSS_UNREAL_DIR) + "/Data"; }

	const abyss::DataStore& SpData()
	{
		static abyss::DataStore Store;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			abyss::DataLoadReport Report;
			const bool bOk = Store.LoadAll(
				[](std::string_view Name, std::string& Out)
				{
					Out = SpReadBytes(SpDataDir() + "/" + std::string(Name));
					return !Out.empty();
				},
				Report);
			if (!bOk)
			{
				abyss::Log(abyss::LogLevel::Error, Report.Summary());
			}
		}
		return Store;
	}

	/** The art manifest object (Data/assets.json `manifest`). */
	const abyss::JsonValue& SpManifest()
	{
		static abyss::JsonValue Root;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			abyss::ParseJson(SpReadBytes(SpDataDir() + "/assets.json"), Root);
		}
		const abyss::JsonValue* Inner = Root.Find("manifest");
		return Inner != nullptr ? *Inner : Root;
	}

	/** An asset's static-mesh sockets, as FAbyssArtManifest reads them (relLocCm, else locCm). */
	std::vector<Socket> SpSockets(const std::string& AssetName)
	{
		std::vector<Socket> Out;
		const abyss::JsonValue& Asset = SpManifest().Get("assets").Get(AssetName);
		for (const abyss::JsonValue& S : Asset.Get("sockets").Items())
		{
			const abyss::JsonValue& Rel = S.Get("relLocCm");
			const abyss::JsonValue& Loc = Rel.IsArray() ? Rel : S.Get("locCm");
			Out.push_back(Socket{ std::string(S.Get("name").AsString()), Loc.At(0).AsDouble(0.0), Loc.At(1).AsDouble(0.0) });
		}
		return Out;
	}

	/** Variants of a game id the zone actor tries (FAbyssArtManifest::ResolveFirst with the zone's theme): every id's
	 *  `<id>@<theme>` first, then the plain chain; asset-name order. */
	std::vector<const abyss::AssetEntryDef*> SpVariants(const std::vector<std::string>& Chain, const abyss::MapDef& Map)
	{
		const abyss::AssetManifest& Assets = SpData().Assets();
		if (Map.hasPresentationTheme)
		{
			for (const std::string& Id : Chain)
			{
				std::vector<const abyss::AssetEntryDef*> Found =
					Assets.FindAllByGameId(Id + "@" + std::string(abyss::EnumName(Map.presentationTheme)));
				if (!Found.empty())
				{
					return Found;
				}
			}
		}
		for (const std::string& Id : Chain)
		{
			std::vector<const abyss::AssetEntryDef*> Found = Assets.FindAllByGameId(Id);
			if (!Found.empty())
			{
				return Found;
			}
		}
		return {};
	}

	/** The art the zone actor places for a placement (its main mesh: VariantHash(variant, TileHash) % variants, and its
	 *  partners at the same hash), as BuildSetPieces hands it to Collect. */
	PlacedArt SpArtFor(const abyss::SetPiecePlacement& P, const abyss::MapDef& Map, uint32_t TileHash)
	{
		PlacedArt Out;
		const uint32_t Hash = VariantHash(P.variant, TileHash);
		const std::vector<const abyss::AssetEntryDef*> Main = SpVariants(IdChain(*P.def), Map);
		if (Main.empty())
		{
			return Out;
		}
		const abyss::AssetEntryDef* Mesh = Main[Hash % Main.size()];
		Out.bResolved = true;
		Out.Sockets = SpSockets(Mesh->name);
		const auto AddBounds = [&Out](const abyss::AssetEntryDef* A)
		{
			if (A->hasBounds)
			{
				Out.Bounds.push_back({ A->boundsMinCm[0], A->boundsMinCm[1], A->boundsMaxCm[0], A->boundsMaxCm[1] });
			}
		};
		AddBounds(Mesh);
		for (const std::string& Partner : P.def->partners)
		{
			const std::vector<const abyss::AssetEntryDef*> Found = SpVariants({ Partner }, Map);
			if (!Found.empty())
			{
				AddBounds(Found[Hash % Found.size()]);
			}
		}
		return Out;
	}

	/** One synthetic piece (tile anchor, yaw facing). */
	abyss::SetPieceDef SpDef(const std::string& Id)
	{
		abyss::SetPieceDef D;
		D.id = Id;
		D.gameId = "g_" + Id;
		D.anchor = abyss::SetPieceAnchorKind::Tile;
		D.facing = abyss::SetPieceFacingKind::Yaw;
		return D;
	}

	abyss::SetPiecePlacement SpAt(const abyss::SetPieceDef& D, double Col, double Row, double YawDeg, int32_t Member = 0)
	{
		abyss::SetPiecePlacement P;
		P.def = &D;
		P.member = Member;
		P.pos = abyss::Vec2(Col, Row);
		P.yawDeg = YawDeg;
		return P;
	}

	PlacedArt SpArt(std::vector<Socket> Sockets, std::vector<std::array<double, 4>> Bounds = {})
	{
		PlacedArt A;
		A.bResolved = true;
		A.Sockets = std::move(Sockets);
		A.Bounds = std::move(Bounds);
		return A;
	}

	/** Echo figures stay on screen next to their lore spot (the hero reads the echo there; the view is ~16 tiles wide). */
	constexpr double kEchoFigureReachTiles = 3.5;
}

TEST_SUITE("setpieces")
{
	TEST_CASE("set pieces: id chain, variant hash and instance keys")
	{
		abyss::SetPieceDef D = SpDef("gate");
		D.fallbackIds = { "f1", "g_gate", "", "f2", "f1" };
		CHECK(IdChain(D) == std::vector<std::string>{ "g_gate", "f1", "f2" });
		CHECK(VariantHash(1, 999u) == 1u);
		CHECK(VariantHash(0, 999u) == 0u);
		CHECK(VariantHash(-1, 999u) == 999u);
		CHECK(InstanceKey(SpAt(D, 3, 4, 0, 7)) == "setpiece:gate#7");
		abyss::SetPiecePlacement Orphan;
		CHECK(InstanceKey(Orphan) == "setpiece:?#0");
		CHECK(InstanceKey(SpAt(D, 0, 0, 0)).rfind(std::string(InstanceKeyPrefix), 0) == 0);
	}

	TEST_CASE("set pieces: Collect turns sockets with the placement and groups emitters")
	{
		abyss::SetPieceDef Fx = SpDef("fx");
		Fx.effects = { { "s", "recipe.a" } };
		abyss::SetPieceDef Door = SpDef("door");
		Door.emitters = { { "knock", "door_knock" } };
		abyss::SetPieceDef Wind = SpDef("wind");
		Wind.emitters = { { "s", "wind_chime" } };
		abyss::SetPieceDef Bell = SpDef("bell");
		Bell.cues = { { { "toll", "toll_far" }, "s", "recipe.bell" } };
		Bell.yieldsToDefendTarget = true;

		const std::vector<abyss::SetPiecePlacement> Placements = {
			SpAt(Fx, 10, 10, 0),       // socket (100, 0) cm -> one tile toward +col
			SpAt(Fx, 10, 10, 90),      // -> +row
			SpAt(Fx, 10, 10, 315),     // -> (0.7071, -0.7071)
			SpAt(Door, 20, 5, 180),    // knock (0, 50) -> (0, -0.5)
			SpAt(Wind, 1, 1, 0),
			SpAt(Door, 30, 5, 0, 1),
			SpAt(Bell, 40, 40, 90),
		};
		const std::vector<PlacedArt> Arts = {
			SpArt({ { "s", 100, 0 } }), SpArt({ { "s", 100, 0 } }), SpArt({ { "s", 100, 0 } }),
			SpArt({ { "knock", 0, 50 } }), SpArt({ { "s", 0, 0 } }), SpArt({ { "knock", 0, 50 } }),
			SpArt({ { "s", 200, 0 } }),
		};
		const ZoneSpots Spots = Collect(Placements, Arts);
		CHECK(Spots.Unresolved.empty());
		CHECK(Spots.MissingSockets.empty());

		REQUIRE(Spots.Effects.size() == 3);
		CHECK(Spots.Effects[0].Recipe == "recipe.a");
		CHECK(Spots.Effects[0].Tile.x == doctest::Approx(11.0));
		CHECK(Spots.Effects[0].Tile.y == doctest::Approx(10.0));
		CHECK(Spots.Effects[1].Tile.x == doctest::Approx(10.0));
		CHECK(Spots.Effects[1].Tile.y == doctest::Approx(11.0));
		CHECK(Spots.Effects[2].Tile.x == doctest::Approx(10.0 + std::sqrt(0.5)));
		CHECK(Spots.Effects[2].Tile.y == doctest::Approx(10.0 - std::sqrt(0.5)));

		// one entry per emitter, first-seen order; the spots in placement order
		REQUIRE(Spots.Emitters.size() == 2);
		CHECK(Spots.Emitters[0].Emitter == "door_knock");
		REQUIRE(Spots.Emitters[0].Tiles.size() == 2);
		CHECK(Spots.Emitters[0].Tiles[0].x == doctest::Approx(20.0));
		CHECK(Spots.Emitters[0].Tiles[0].y == doctest::Approx(4.5));
		CHECK(Spots.Emitters[0].Tiles[1].x == doctest::Approx(30.0));
		CHECK(Spots.Emitters[0].Tiles[1].y == doctest::Approx(5.5));
		CHECK(Spots.Emitters[1].Emitter == "wind_chime");
		CHECK(Spots.Emitters[1].Tiles.size() == 1);

		REQUIRE(Spots.Cues.size() == 1);
		CHECK(Spots.Cues[0].Recipe == "recipe.bell");
		CHECK(Spots.Cues[0].Tile.x == doctest::Approx(40.0));
		CHECK(Spots.Cues[0].Tile.y == doctest::Approx(42.0));

		REQUIRE(Spots.Yielders.size() == 1);
		CHECK(Spots.Yielders[0].Key == "setpiece:bell#0");
		CHECK(Spots.Yielders[0].YawDeg == doctest::Approx(90.0));

		// no bounds: each placement covers the tile under its origin only
		CHECK(Spots.Covered.size() == 5);
		CHECK(std::is_sorted(Spots.Covered.begin(), Spots.Covered.end(), TileLess));
	}

	TEST_CASE("set pieces: the piece's scale reaches its sockets")
	{
		abyss::SetPieceDef Big = SpDef("big");
		Big.scale = 2.0;
		Big.effects = { { "s", "r" } };
		const ZoneSpots Spots = Collect({ SpAt(Big, 5, 5, 0) }, { SpArt({ { "s", 50, 25 } }) });
		REQUIRE(Spots.Effects.size() == 1);
		CHECK(Spots.Effects[0].Tile.x == doctest::Approx(6.0));
		CHECK(Spots.Effects[0].Tile.y == doctest::Approx(5.5));
	}

	TEST_CASE("set pieces: unresolved art and missing sockets are reported, echoes still anchor")
	{
		abyss::SetPieceDef Ring = SpDef("ring");
		Ring.effects = { { "flame", "r" } };
		Ring.echoes = { "lore_x" };
		abyss::SetPieceDef Gate = SpDef("gate");
		Gate.effects = { { "nope", "r" } };
		Gate.cues = { { { "c" }, "nope", "r" } };
		Gate.emitters = { { "s", "e" } };
		Gate.yieldsToDefendTarget = true;

		const std::vector<abyss::SetPiecePlacement> Placements = {
			SpAt(Ring, 3, 3, 0, 0), SpAt(Ring, 4, 3, 0, 1), SpAt(Gate, 9, 9, 0),
			SpAt(Ring, 6, 3, 0, 2),   // no art entry at all (Arts is shorter)
		};
		PlacedArt Unresolved;
		const std::vector<PlacedArt> Arts = { Unresolved, Unresolved, SpArt({ { "s", 0, 0 } }) };
		const ZoneSpots Spots = Collect(Placements, Arts);
		CHECK(Spots.Unresolved == std::vector<std::string>{ "ring" });
		CHECK(Spots.MissingSockets == std::vector<std::string>{ "gate.nope" });
		CHECK(Spots.Effects.empty());
		CHECK(Spots.Cues.empty());
		REQUIRE(Spots.Emitters.size() == 1);   // the gate's good socket still counts
		CHECK(Spots.Emitters[0].Tiles.size() == 1);
		CHECK(Spots.Yielders.size() == 1);
		// nothing drawn = nothing covered, but the echo figures stand where the mesh would
		CHECK(Spots.Covered == std::vector<abyss::TilePos>{ abyss::TilePos(9, 9) });
		CHECK(Spots.Echoes.size() == 3);

		// a null def is skipped
		abyss::SetPiecePlacement Orphan;
		CHECK(Collect({ Orphan }, { SpArt({}) }).Covered.empty());
	}

	TEST_CASE("set pieces: covered tiles follow the turned bounds and hide only non-blocking decoration")
	{
		abyss::SetPieceDef Hut = SpDef("hut");
		// 3 x 1 tiles along the mesh's X: at yaw 0 it spans columns, at yaw 90 rows
		const PlacedArt Art = SpArt({}, { { { -150.0, -40.0, 150.0, 40.0 } } });
		const ZoneSpots Flat = Collect({ SpAt(Hut, 10, 10, 0) }, { Art });
		CHECK(Flat.Covered == std::vector<abyss::TilePos>{ { 9, 10 }, { 10, 10 }, { 11, 10 } });
		const ZoneSpots Turned = Collect({ SpAt(Hut, 10, 10, 90) }, { Art });
		CHECK(Turned.Covered == std::vector<abyss::TilePos>{ { 10, 9 }, { 10, 10 }, { 10, 11 } });

		// two placements overlapping: sorted (row, col) and unique
		const ZoneSpots Both = Collect({ SpAt(Hut, 10, 10, 0), SpAt(Hut, 10, 10, 90) }, { Art, Art });
		CHECK(Both.Covered == std::vector<abyss::TilePos>{ { 10, 9 }, { 9, 10 }, { 10, 10 }, { 11, 10 }, { 10, 11 } });

		CHECK(HidesDecoration(Flat, abyss::TilePos(11, 10), false));
		CHECK_FALSE(HidesDecoration(Flat, abyss::TilePos(11, 10), true));   // a blocking one stays (the core blocks it)
		CHECK_FALSE(HidesDecoration(Flat, abyss::TilePos(12, 10), false));
		CHECK_FALSE(HidesDecoration(ZoneSpots{}, abyss::TilePos(10, 10), false));
	}

	TEST_CASE("set pieces: cue spots, echo anchors, figure poses and yielders")
	{
		ZoneSpots Spots;
		Spots.Cues = { CueSpot{ { "bell_toll", "bell_far" }, "r1", abyss::Vec2(1, 1) },
			CueSpot{ { "bell_self" }, "r2", abyss::Vec2(2, 2) }, CueSpot{ { "bell_far" }, "r3", abyss::Vec2(3, 3) } };
		const std::vector<const CueSpot*> Far = CueSpotsFor(Spots, "bell_far");
		REQUIRE(Far.size() == 2);
		CHECK(Far[0]->Recipe == "r1");
		CHECK(Far[1]->Recipe == "r3");
		CHECK(CueSpotsFor(Spots, "bell_self").size() == 1);
		CHECK(CueSpotsFor(Spots, "bell").empty());

		Spots.Echoes = { EchoAnchor{ "echo_a", abyss::Vec2(10, 10), 0.0 }, EchoAnchor{ "echo_a", abyss::Vec2(30, 30), 45.0 },
			EchoAnchor{ "echo_b", abyss::Vec2(12, 12), 90.0 } };
		const EchoAnchor* Near = EchoAnchorFor(Spots, "echo_a", abyss::Vec2(28, 29));
		REQUIRE(Near != nullptr);
		CHECK(Near->YawDeg == doctest::Approx(45.0));
		CHECK(EchoAnchorFor(Spots, "echo_a", abyss::Vec2(0, 0))->Tile.x == doctest::Approx(10.0));
		CHECK(EchoAnchorFor(Spots, "echo_c", abyss::Vec2(0, 0)) == nullptr);

		// a figure authored at (-2, 0.5) tiles facing 30 beside a mesh at yaw 0, posed beside it at yaw 0 and 90
		const FigurePose Same = EchoFigurePose(0.0, -2.0, 0.5, 30.0);
		CHECK(Same.Offset.x == doctest::Approx(-2.0));
		CHECK(Same.Offset.y == doctest::Approx(0.5));
		CHECK(Same.FacingYawDeg == doctest::Approx(30.0));
		const FigurePose Turned = EchoFigurePose(90.0, -2.0, 0.5, 30.0);
		CHECK(Turned.Offset.x == doctest::Approx(-0.5));
		CHECK(Turned.Offset.y == doctest::Approx(-2.0));
		CHECK(Turned.FacingYawDeg == doctest::Approx(120.0));

		Spots.Yielders = { Yielder{ "setpiece:a#0", abyss::Vec2(5, 5), 180.0 }, Yielder{ "setpiece:b#0", abyss::Vec2(6, 5), 0.0 } };
		CHECK(YielderNear(Spots, abyss::Vec2(5.2, 5.0))->Key == "setpiece:a#0");
		CHECK(YielderNear(Spots, abyss::Vec2(5.8, 5.0))->Key == "setpiece:b#0");
		CHECK(YielderNear(Spots, abyss::Vec2(7.5, 5.0)) != nullptr);              // exactly the radius
		CHECK(YielderNear(Spots, abyss::Vec2(7.6, 5.0)) == nullptr);
		CHECK(YielderNear(Spots, abyss::Vec2(7.6, 5.0), 2.0) != nullptr);
	}

	TEST_CASE("set pieces: event label keys prefer the zone's own string")
	{
		const std::set<std::string> Keys = { "zone.event.treasureChest.label.z1", "zone.event.merchant.label.z1",
			"sys.event.rescue.z1" };
		const auto Has = [&Keys](std::string_view K) { return Keys.count(std::string(K)) > 0; };
		CHECK(EventLabelKey("treasure_cache", "z1", true, Has) == "zone.event.treasureChest.label.z1");
		CHECK(EventLabelKey("treasure_cache", "z2", true, Has) == "zone.event.treasureChest.label");
		CHECK(EventLabelKey("wandering_merchant", "z1", true, Has) == "zone.event.merchant.label.z1");
		CHECK(EventLabelKey("environmental_puzzle", "z1", true, Has) == "zone.event.puzzle.label");
		CHECK(EventLabelKey("rescue", "z1", true, Has) == "sys.event.rescue.z1");
		CHECK(EventLabelKey("rescue", "z2", true, Has) == "zone.event.rescue.fallbackName");
		CHECK(EventLabelKey("rescue", "", true, Has) == "zone.event.rescue.fallbackName");
		CHECK(EventLabelKey("treasure_cache", "", true, Has) == "zone.event.treasureChest.label");
		CHECK(EventLabelKey("monster_ambush", "z1", true, Has).empty());
		// an unthemed zone: the rescue prop draws no label (as before U4), the other event props keep their labels
		CHECK(EventLabelKey("rescue", "z1", false, Has).empty());
		CHECK(EventLabelKey("rescue", "z2", false, Has).empty());
		CHECK(EventLabelKey("treasure_cache", "z1", false, Has) == "zone.event.treasureChest.label.z1");
		CHECK(EventLabelKey("treasure_cache", "z2", false, Has) == "zone.event.treasureChest.label");
		CHECK(ZoneVariantKey("a.b", "z1", [](std::string_view) { return true; }) == "a.b.z1");

		// the shipped strings: every key a zone's event prop can ask for exists; chapters 2-5 (no presentation theme)
		// draw exactly today's labels: the generic treasure / merchant / puzzle strings and no rescue label
		const abyss::DataStore& Data = SpData();
		REQUIRE(Data.IsFinalized());
		const abyss::I18n& Strings = Data.Strings();
		const auto HasString = [&Strings](std::string_view K) { return Strings.Has(K); };
		size_t Unthemed = 0;
		for (const abyss::MapDef& Map : Data.World().maps)
		{
			CAPTURE(Map.id);
			const bool bThemed = !AbyssThemeRules::ThemeOf(&Map).empty();
			Unthemed += bThemed ? 0 : 1;
			for (const char* Prop : { "treasure_cache", "wandering_merchant", "environmental_puzzle", "rescue" })
			{
				CAPTURE(Prop);
				const std::string Key = EventLabelKey(Prop, Map.id, bThemed, HasString);
				if (!bThemed && std::string_view(Prop) == "rescue")
				{
					CHECK(Key.empty());
					continue;
				}
				CHECK(Strings.Has(Key));
				if (!bThemed)
				{
					CHECK(Key == (std::string_view(Prop) == "treasure_cache" ? "zone.event.treasureChest.label"
						: std::string_view(Prop) == "wandering_merchant" ? "zone.event.merchant.label"
						: "zone.event.puzzle.label"));
				}
			}
		}
		CHECK(Unthemed >= 4);
		CHECK(EventLabelKey("rescue", "twilight_forest",
			!AbyssThemeRules::ThemeOf(Data.FindMap("twilight_forest")).empty(), HasString).empty());
	}

	TEST_CASE("set pieces (shipped): every socket resolves, spots stand by their piece, echoes stay by their lore")
	{
		const abyss::DataStore& Data = SpData();
		REQUIRE(Data.IsFinalized());
		REQUIRE(Data.SetPieces().loaded);
		REQUIRE(Data.Assets().loaded);
		const abyss::SetPieceFrame Frame{ -90.0, Data.World().constants.cameraYawDeg };
		const AbyssThemeRules::FEchoLook Echo =
			AbyssThemeRules::ParseEchoLook(SpManifest().Get("materials").Get("MI_AF_Echo"));
		size_t Echoes = 0;
		for (const abyss::SetPieceZoneDef& Zone : Data.SetPieces().zones)
		{
			CAPTURE(Zone.zone);
			const abyss::MapDef* Map = Data.FindMap(Zone.zone);
			REQUIRE(Map != nullptr);
			const abyss::ZoneGrid Grid = abyss::BuildZoneGrid(Data, *Map);
			std::vector<abyss::SetPiecePlacement> Placements;
			abyss::ExpandSetPieces(Zone, *Map, Data, Frame, Placements);
			REQUIRE_FALSE(Placements.empty());

			// every variant the tile hash can pick carries the sockets the table names
			size_t MaxVariants = 1;
			for (const abyss::SetPiecePlacement& P : Placements)
			{
				MaxVariants = (std::max)(MaxVariants, SpVariants(IdChain(*P.def), *Map).size());
			}
			for (uint32_t Hash = 0; Hash < MaxVariants; ++Hash)
			{
				CAPTURE(Hash);
				std::vector<PlacedArt> Arts;
				for (const abyss::SetPiecePlacement& P : Placements)
				{
					Arts.push_back(SpArtFor(P, *Map, Hash));
				}
				const ZoneSpots Spots = Collect(Placements, Arts);
				CHECK(Spots.Unresolved.empty());
				CHECK(Spots.MissingSockets.empty());
			}

			std::vector<PlacedArt> Arts;
			size_t WantEffects = 0;
			size_t WantEmitterSpots = 0;
			size_t WantCues = 0;
			size_t WantYielders = 0;
			for (const abyss::SetPiecePlacement& P : Placements)
			{
				Arts.push_back(SpArtFor(P, *Map, 0));
				WantEffects += P.def->effects.size();
				WantEmitterSpots += P.def->emitters.size();
				WantCues += P.def->cues.size();
				WantYielders += P.def->yieldsToDefendTarget ? 1 : 0;
			}
			const ZoneSpots Spots = Collect(Placements, Arts);
			CHECK(Spots.Effects.size() == WantEffects);
			size_t EmitterSpotCount = 0;
			for (const EmitterSpots& E : Spots.Emitters)
			{
				EmitterSpotCount += E.Tiles.size();
			}
			CHECK(EmitterSpotCount == WantEmitterSpots);
			CHECK(Spots.Cues.size() == WantCues);
			CHECK(Spots.Yielders.size() == WantYielders);

			// a socket spot stands on (or right beside) its mesh, on the map
			const auto NearSomePiece = [&Placements](abyss::Vec2 Tile)
			{
				for (const abyss::SetPiecePlacement& P : Placements)
				{
					if (abyss::DistSq(P.pos, Tile) <= 3.0 * 3.0)
					{
						return true;
					}
				}
				return false;
			};
			for (const EffectSpot& E : Spots.Effects)
			{
				CHECK(NearSomePiece(E.Tile));
				CHECK(Grid.InBounds(abyss::RoundToTile(E.Tile).col, abyss::RoundToTile(E.Tile).row));
			}
			for (const EmitterSpots& E : Spots.Emitters)
			{
				for (const abyss::Vec2& T : E.Tiles)
				{
					CHECK(NearSomePiece(T));
				}
			}
			for (const CueSpot& C : Spots.Cues)
			{
				CHECK(NearSomePiece(C.Tile));
				for (const std::string& Cue : C.Cues)
				{
					CHECK_FALSE(CueSpotsFor(Spots, Cue).empty());
				}
			}
			if (Zone.zone == "emerald_plains")
			{
				// The bell that rings by itself at the siege is Knellwatch's (the bell frame); the Boneyard Gate's bell
				// echoes it. Every toll of the fort's bell ripples the bell frame.
				const auto RipplesPiece = [&](std::string_view Cue, std::string_view PieceId)
				{
					for (const CueSpot* Spot : CueSpotsFor(Spots, Cue))
					{
						for (const abyss::SetPiecePlacement& P : Placements)
						{
							if (P.def->id == PieceId && abyss::Dist(P.pos, Spot->Tile) <= 3.0)
							{
								return true;
							}
						}
					}
					return false;
				};
				for (const char* Cue : { "bell_toll", "bell_toll_far", "bell_dawn_toll", "bell_self_ring" })
				{
					CAPTURE(Cue);
					CHECK(RipplesPiece(Cue, "knellwatch_bell_frame"));
				}
				CHECK(RipplesPiece("bell_self_ring", "boneyard_gate"));
				CHECK(CueSpotsFor(Spots, "bell_self_ring").size() == 2);
			}

			// a yielding piece stands on its quest's defend target (the target hides it and takes its yaw)
			for (const abyss::SetPiecePlacement& P : Placements)
			{
				if (!P.def->yieldsToDefendTarget || P.def->anchor != abyss::SetPieceAnchorKind::DefendTarget)
				{
					continue;
				}
				const abyss::QuestDef* Quest = Data.FindQuest(P.def->ref);
				REQUIRE(Quest != nullptr);
				const Yielder* Y = YielderNear(Spots, Quest->defendTarget.pos.Center());
				REQUIRE(Y != nullptr);
				CHECK(Y->Key == InstanceKey(P));
			}

			// every covered tile is on the map; the decorations hidden there are dressing only
			for (const abyss::TilePos& T : Spots.Covered)
			{
				CHECK(Grid.InBounds(T.col, T.row));
			}
			for (const abyss::Decoration& D : Grid.Decorations())
			{
				const abyss::TilePos T(static_cast<int32_t>(D.col), static_cast<int32_t>(D.row));
				if (D.blocking)
				{
					CHECK_FALSE(HidesDecoration(Spots, T, true));
				}
			}

			// echoes: each lore echo a piece names is anchored once; its MI_AF_Echo figures, posed in the piece's
			// frame, stay on screen from the lore spot and off water and rock
			for (const abyss::SetPieceDef& Def : Zone.pieces)
			{
				for (const std::string& LoreId : Def.echoes)
				{
					CAPTURE(LoreId);
					++Echoes;
					const abyss::LoreEntryDef* Lore = Data.Lore().Find(LoreId);
					REQUIRE(Lore != nullptr);
					const EchoAnchor* Anchor = EchoAnchorFor(Spots, LoreId, Lore->pos.Center());
					REQUIRE(Anchor != nullptr);
					const AbyssThemeRules::FEchoEntry* Entry = Echo.Find(LoreId);
					REQUIRE_MESSAGE(Entry != nullptr, "the art contract has no figures for " << LoreId);
					REQUIRE_FALSE(Entry->Figures.empty());
					for (const AbyssThemeRules::FEchoFigure& F : Entry->Figures)
					{
						CAPTURE(F.Asset);
						const FigurePose Pose = EchoFigurePose(Anchor->YawDeg, F.OffsetCol, F.OffsetRow, F.FacingYawDeg);
						const abyss::Vec2 At = Anchor->Tile + Pose.Offset;
						CHECK(abyss::Dist(At, Lore->pos.Center()) <= kEchoFigureReachTiles);
						const abyss::TilePos T = abyss::RoundToTile(At);
						REQUIRE(Grid.InBounds(T.col, T.row));
						CHECK(Grid.Tile(T.col, T.row) != abyss::TileType::Water);
						CHECK(Grid.Tile(T.col, T.row) != abyss::TileType::Wall);
					}
				}
			}
		}
		CHECK(Echoes > 0);
	}
}
