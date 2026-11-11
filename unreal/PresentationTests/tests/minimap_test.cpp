// The minimap's tile palette (Private/UI/Hud/AbyssMinimapRules.h, used by FAbyssMinimapTexture for the HUD minimap and the
// world-map panel): a zone with a presentation theme is coloured from that theme's terrain style in the shipped
// terrain_styles.json (the first Mac run drew the Red March in the plains' green), a zone without one keeps the web
// colours, a theme without a style falls back along its parents (pit -> march), and blocked tiles stay darker than
// any walkable ground.
#include <algorithm>
#include <string>
#include <string_view>

#include "doctest/doctest.h"

#include "abyss/base/Json.h"
#include "abyss/data/DataStore.h"
#include "abyss/data/MapData.h"

#include "UI/Hud/AbyssMinimapRules.h"
#include "World/AbyssThemeRules.h"
#include "audio_test_data.h"

namespace
{
	using namespace AbyssMinimapRules;

	const abyss::JsonValue* Styles()
	{
		return audio_test::Data().RawTable("terrain_styles.json");
	}

	/** Green clearly leads red and blue: the plains' grass, never the march's ash. */
	bool IsGreen(std::uint32_t Rgb)
	{
		return G(Rgb) > R(Rgb) + 24 && G(Rgb) > B(Rgb) + 24;
	}

	abyss::JsonValue Parse(std::string_view Text)
	{
		abyss::JsonValue Root;
		abyss::JsonParseError Error;
		REQUIRE(abyss::ParseJson(Text, Root, &Error));
		return Root;
	}
}

TEST_SUITE("minimap")
{
	TEST_CASE("a zone without a presentation theme keeps the web minimap colours")
	{
		const FPalette Web = WebPalette();
		CHECK(Web.Tile[TileGrass] == 0x4a8c3f);
		CHECK(Web.Tile[TileDirt] == 0x8b7355);
		CHECK(Web.Tile[TileStone] == 0x6a6a6a);
		CHECK(Web.Tile[TileWater] == 0x1a5276);
		CHECK(Web.Tile[TileWall] == 0x4a4a4a);
		CHECK(Web.Tile[TileCamp] == 0x9e7c52);
		CHECK(Web.Tile[TileCampWall] == 0x5c4834);
		CHECK(Web.Unknown == 0x222222);
		CHECK(Web.Style.empty());

		REQUIRE(Styles() != nullptr);
		const FPalette Unthemed = PaletteFor(Styles(), "");
		CHECK(Unthemed.Tile == Web.Tile);
		CHECK(Unthemed.Style.empty());
		CHECK(PaletteFor(nullptr, "march").Tile == Web.Tile);          // no style table: the web colours, not black
		CHECK(PaletteFor(Styles(), "no_such_theme").Tile == Web.Tile);
		CHECK(ColorOf(Web, 3) == 0x1a5276);
		CHECK(ColorOf(Web, 7) == 0x222222);
		CHECK(ColorOf(Web, 255) == 0x222222);
		CHECK(ColorOf(Web, -1) == 0x222222);
	}

	TEST_CASE("the Red March is drawn in its own ground colours, not the plains' green")
	{
		REQUIRE(Styles() != nullptr);
		// the shipped zone: emerald_plains is the march (generator theme plains, presentation theme march)
		const abyss::MapDef* March = audio_test::Data().FindMap("emerald_plains");
		REQUIRE(March != nullptr);
		const std::string_view Theme = AbyssThemeRules::ThemeOf(March);
		REQUIRE(Theme == "march");
		const FPalette P = PaletteFor(Styles(), Theme);
		CHECK(P.Style == "march");
		const abyss::JsonValue& Ground = Styles()->Get("themes").Get("march").Get("ground");
		CHECK(P.Tile[TileGrass] == static_cast<std::uint32_t>(Ground.Get("0").Get("base").AsInt64()));
		CHECK(P.Tile[TileDirt] == static_cast<std::uint32_t>(Ground.Get("1").Get("base").AsInt64()));
		CHECK(P.Tile[TileStone] == static_cast<std::uint32_t>(Ground.Get("2").Get("base").AsInt64()));
		// S-A review R3: the march's bases are greys a few luma levels apart. The camp is lifted to the map's
		// brightest walkable colour (its own hue), the grey pools are capped like the other blocked tiles.
		const std::uint32_t CampBase = static_cast<std::uint32_t>(Ground.Get("5").Get("base").AsInt64());
		const std::uint32_t WaterBase = static_cast<std::uint32_t>(Ground.Get("3").Get("base").AsInt64());
		CHECK(Luma(CampBase) < Luma(P.Tile[TileGrass]) * 1.1);   // the shipped data: camp lost in the field
		CHECK(Luma(P.Tile[TileCamp]) >= Luma(P.Tile[TileGrass]) * CampOverGround);
		CHECK(Luma(P.Tile[TileCamp]) >= Luma(P.Tile[TileStone]) * CampOverGround - 1.5);
		CHECK(Luma(P.Tile[TileCamp]) - Luma(P.Tile[TileGrass]) >= 24.0);
		CHECK(HueSpread(P.Tile[TileCamp]) <= HueSpread(CampBase) * 2 + 2);   // lifted, not recoloured
		CHECK(Luma(P.Tile[TileCampWall]) < Luma(P.Tile[TileCamp]) * 0.5);    // the palisade ring reads on the camp
		CHECK(HueSpread(WaterBase) < WaterHueSpread);
		CHECK(Luma(P.Tile[TileWater]) <= Luma(P.Tile[TileDirt]) * BlockedBelowGround + 1.0);
		CHECK(Luma(P.Tile[TileWater]) <= Luma(WaterBase));
		CHECK(IsGreen(WebPalette().Tile[TileGrass]));
		for (int Tile = 0; Tile < TileKinds; ++Tile)
		{
			CAPTURE(Tile);
			CHECK_FALSE(IsGreen(P.Tile[static_cast<size_t>(Tile)]));
			CHECK(P.Tile[static_cast<size_t>(Tile)] != WebPalette().Tile[static_cast<size_t>(Tile)]);
		}
		// the other zones have no presentation theme: the web colours, as before
		for (const char* Id : { "twilight_forest", "anvil_mountains", "scorching_desert", "abyss_rift", "ember_tower" })
		{
			const std::string IdName(Id);
			CAPTURE(IdName);
			CHECK(PaletteFor(Styles(), AbyssThemeRules::ThemeOf(audio_test::Data().FindMap(Id))).Tile == WebPalette().Tile);
		}
	}

	TEST_CASE("every style in the data gives a readable palette: blocked tiles under the darkest walkable ground")
	{
		REQUIRE(Styles() != nullptr);
		int Themes = 0;
		for (const abyss::JsonMember& Member : Styles()->Get("themes").Members())
		{
			const std::string Name(Member.key);
			CAPTURE(Name);
			const FPalette P = PaletteFor(Styles(), Name);
			REQUIRE(P.Style == Name);
			++Themes;
			double Darkest = 255.0;
			for (const int Walkable : { TileGrass, TileDirt, TileStone, TileCamp })
			{
				Darkest = (std::min)(Darkest, Luma(P.Tile[static_cast<size_t>(Walkable)]));
			}
			CHECK(Luma(P.Tile[TileWall]) <= Darkest * BlockedBelowGround + 1.0);
			CHECK(Luma(P.Tile[TileCampWall]) <= Darkest * BlockedBelowGround + 1.0);
			CHECK(Luma(P.Tile[TileWall]) > 12.0);   // still off the fog (7, 6, 10)
			CHECK(Darkest > 30.0);
			// the camp is the brightest walkable colour, by CampOverGround unless 8 bits run out (the desert's sand)
			double Brightest = 0.0;
			for (const int Ground : { TileGrass, TileDirt, TileStone })
			{
				Brightest = (std::max)(Brightest, Luma(P.Tile[static_cast<size_t>(Ground)]));
			}
			CHECK(Luma(P.Tile[TileCamp]) >= (std::min)(Brightest * CampOverGround - 1.5, 225.0));
			CHECK(Luma(P.Tile[TileCamp]) > Brightest + 20.0);
			// water is told from walkable ground by its hue or, without one, by being as dark as a wall
			const bool bHue = HueSpread(P.Tile[TileWater]) >= WaterHueSpread;
			CHECK((bHue || Luma(P.Tile[TileWater]) <= Darkest * BlockedBelowGround + 1.0));
		}
		CHECK(Themes >= 6);   // plains, forest, mountain, desert, abyss, march (+ pit once Wave 3's content is in)
	}

	TEST_CASE("the Pit: its own style when the data has one, else its parent's (march)")
	{
		CHECK(ParentTheme("pit") == "march");
		CHECK(ParentTheme("pit_cleansed") == "march");
		CHECK(ParentTheme("march").empty());
		CHECK(ParentTheme("plains").empty());
		CHECK(StyleChain("pit") == std::vector<std::string>{ "pit", "march" });
		CHECK(StyleChain("march") == std::vector<std::string>{ "march" });
		CHECK(StyleChain("").empty());

		REQUIRE(Styles() != nullptr);
		const bool bShipped = Styles()->Get("themes").Get("pit").Get("ground").IsObject();
		const FPalette P = PaletteFor(Styles(), "pit");
		CHECK(P.Style == (bShipped ? "pit" : "march"));
		CHECK_FALSE(IsGreen(P.Tile[TileGrass]));

		// a table with both: the Pit's own colours win; with the Pit's ground missing: the march's
		const abyss::JsonValue Both = Parse(R"({"themes": {
			"march": {"ground": {"0": {"base": 4999488}, "2": {"base": 5591628}}, "palisade": {"wood": 3812900}},
			"pit": {"ground": {"0": {"base": 3815219}, "1": {"base": 3551532}, "2": {"base": 3552048}, "3": {"base": 1052948},
				"5": {"base": 3880756}}}}})");
		const FPalette Pit = PaletteFor(&Both, "pit");
		CHECK(Pit.Style == "pit");
		CHECK(Pit.Tile[TileGrass] == 3815219u);
		CHECK(Pit.Tile[TileWater] == 1052948u);
		CHECK(Pit.Tile[TileCampWall] == Pit.Tile[TileWall]);   // no palisade in the style: the wall colour
		const abyss::JsonValue OnlyMarch = Parse(R"({"themes": {
			"march": {"ground": {"0": {"base": 4999488}, "2": {"base": 5591628}}, "palisade": {"wood": 3812900}},
			"pit": {"outcrop": {"rock": 1}}}})");
		const FPalette Fallback = PaletteFor(&OnlyMarch, "pit");
		CHECK(Fallback.Style == "march");
		CHECK(Fallback.Tile[TileGrass] == 4999488u);
		CHECK(Fallback.Tile[TileDirt] == WebPalette().Tile[TileDirt]);   // a ground type the style does not list
		// malformed colours are not taken
		const abyss::JsonValue Bad = Parse(R"({"themes": {"march": {"ground": {"0": {"base": "green"}, "1": {"base": -4},
			"2": {"base": 16777216}}}}})");
		CHECK(PaletteFor(&Bad, "march").Style.empty());
		CHECK(PaletteFor(&Bad, "march").Tile == WebPalette().Tile);
	}

	TEST_CASE("colour helpers")
	{
		CHECK(Scale(0x6a6a6a, WallOfStone) == 0x4a4a4a);   // the web's own wall from its stone
		CHECK(Scale(0xffffff, 2.0) == 0xffffff);
		CHECK(Scale(0x102030, 0.0) == 0x000000);
		CHECK(Luma(0xffffff) == doctest::Approx(255.0));
		CHECK(Luma(0x000000) == 0.0);
		CHECK(CapLuma(0x808080, 200.0) == 0x808080);         // never brightens
		CHECK(Luma(CapLuma(0x808080, 64.0)) == doctest::Approx(64.0).epsilon(0.02));
		CHECK(LiftLuma(0x808080, 64.0) == 0x808080);         // never darkens
		CHECK(Luma(LiftLuma(0x47423c, 102.0)) == doctest::Approx(102.0).epsilon(0.02));
		CHECK(LiftLuma(0x000000, 100.0) == 0x646464);
		CHECK(LiftLuma(0xf0f0f0, 400.0) == 0xffffff);        // clamps
		CHECK(HueSpread(0x808080) == 0);
		CHECK(HueSpread(0x262a30) == 10);                    // the march's pools: grey
		CHECK(HueSpread(0xf2601e) == 212);                   // the abyss's lava
		CHECK(HueSpread(0x1a5276) == 92);                    // the web's water
		// a coloured water keeps its base; a grey one is capped
		const abyss::JsonValue Lava = Parse(R"({"themes": {"t": {"ground": {"0": {"base": 5655130}, "1": {"base": 6180952},
			"2": {"base": 4405834}, "3": {"base": 15884318}, "5": {"base": 5129296}}}}})");
		CHECK(PaletteFor(&Lava, "t").Tile[TileWater] == 15884318u);
	}
}
