// U6 (dark remake Wave 3): the Pit of the Faithful's presentation rules (Source/Abyssfire/Private/World/
// AbyssSubDungeonRules.h, the theme parent chain and the cleansed mood of AbyssThemeRules.h) on the shipped tables
// (unreal/Data/*.json, the art manifest copy Data/assets.json, Art/Export/VFX/vfx_recipes.json) plus synthetic cases.
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "doctest/doctest.h"

#include "abyss/base/Json.h"
#include "abyss/base/Log.h"
#include "abyss/data/DataStore.h"
#include "abyss/data/SetPieceData.h"
#include "abyss/world/MapGen.h"
#include "abyss/world/SetPieces.h"

#include "Actors/AbyssAnimActionRules.h"
#include "Audio/AbyssAmbienceRules.h"
#include "Framework/AbyssSmoothnessRules.h"
#include "World/AbyssSubDungeonRules.h"
#include "World/AbyssThemeRules.h"

namespace
{
	using namespace AbyssSubDungeonRules;

	constexpr const char* PitId = "sub_rf_pit";

	std::string ReadBytes(const std::string& Path)
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

	std::string UnrealDir() { return std::string(ABYSS_UNREAL_DIR); }

	const abyss::DataStore& Data()
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
					Out = ReadBytes(UnrealDir() + "/Data/" + std::string(Name));
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

	const abyss::JsonValue& Manifest()
	{
		static abyss::JsonValue Root;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			abyss::ParseJson(ReadBytes(UnrealDir() + "/Data/assets.json"), Root);
		}
		const abyss::JsonValue* Inner = Root.Find("manifest");
		return Inner != nullptr ? *Inner : Root;
	}

	const abyss::JsonValue& Recipes()
	{
		static abyss::JsonValue Root;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			abyss::ParseJson(ReadBytes(UnrealDir() + "/Art/Export/VFX/vfx_recipes.json"), Root);
		}
		return Root;
	}

	const abyss::JsonValue* Styles() { return Data().RawTable("terrain_styles.json"); }

	/** gameId -> sorted asset names (FAbyssArtManifest's index). */
	const std::map<std::string, std::vector<std::string>>& GameIds()
	{
		static std::map<std::string, std::vector<std::string>> Index;
		if (Index.empty())
		{
			for (const abyss::JsonMember& Asset : Manifest().Get("assets").Members())
			{
				for (const abyss::JsonValue& Id : Asset.value.Get("gameIds").Items())
				{
					Index[std::string(Id.AsString())].push_back(std::string(Asset.key));
				}
			}
			for (auto& Pair : Index)
			{
				std::sort(Pair.second.begin(), Pair.second.end());
			}
		}
		return Index;
	}

	/** The asset a chain resolves to under a theme chain, as FAbyssArtManifest::ResolveFirst(Ids, Hash, Theme). */
	std::string Resolve(const std::vector<std::string>& Ids, std::string_view Theme, uint32_t Hash = 0)
	{
		const auto& Index = GameIds();
		auto Lookup = [&Index, Hash](const std::string& Id) -> const std::string*
		{
			const auto It = Index.find(Id);
			return It != Index.end() && !It->second.empty() ? &It->second[Hash % It->second.size()] : nullptr;
		};
		const std::vector<std::string> Chain = AbyssThemeRules::ThemeChain(Styles(), Theme);
		const std::string* Found = AbyssThemeRules::ResolveChainFirst<const std::string*>(Ids, Chain.size(),
			[&](size_t ThemeIndex, const std::string& Id) { return Lookup(AbyssThemeRules::ThemedId(Id, Chain[ThemeIndex])); },
			[&](const std::string& Id) { return Lookup(Id); });
		return Found != nullptr ? *Found : std::string();
	}

	const abyss::MapDef* PitMap() { return Data().FindMap(PitId); }

	const abyss::ZoneGrid& PitGrid()
	{
		static const abyss::ZoneGrid Grid = abyss::BuildZoneGrid(Data(), *PitMap());
		return Grid;
	}

	FCleanseSpec PitSpec() { return ParseCleanseSpec(Recipes().Get("dark").Get("pit")); }

	std::vector<abyss::SetPiecePlacement> PitPlacements()
	{
		std::vector<abyss::SetPiecePlacement> Out;
		const abyss::SetPieceZoneDef* Zone = Data().SetPieces().ForZone(PitId);
		if (Zone != nullptr && PitMap() != nullptr)
		{
			const abyss::SetPieceFrame Frame{ -90.0, Data().World().constants.cameraYawDeg };
			abyss::ExpandSetPieces(*Zone, *PitMap(), Data(), Frame, Out);
		}
		return Out;
	}

	std::vector<abyss::Vec2> PitLamps(const FCleanseSpec& Spec)
	{
		std::vector<abyss::Vec2> Out;
		for (const abyss::SetPiecePlacement& P : PitPlacements())
		{
			if (P.def->gameId == Spec.LampUnlit)
			{
				Out.push_back(P.pos);
			}
		}
		return Out;
	}

	bool Near(double A, double B, double Eps = 1e-9) { return std::fabs(A - B) <= Eps; }

	double TileDist(abyss::Vec2 A, abyss::Vec2 B) { return std::sqrt((A.x - B.x) * (A.x - B.x) + (A.y - B.y) * (A.y - B.y)); }
}

TEST_SUITE("subdungeon")
{
	TEST_CASE("theme chain: pit shows <id>@pit, then <id>@march, then <id>")
	{
		using namespace AbyssThemeRules;
		REQUIRE(Styles() != nullptr);
		REQUIRE(PitMap() != nullptr);
		CHECK(ThemeOf(PitMap()) == "pit");
		CHECK(ParentTheme(Styles(), "pit") == "march");
		CHECK(ThemeChain(Styles(), "pit") == std::vector<std::string>{ "pit", "march" });
		CHECK(ThemeChain(Styles(), "march") == std::vector<std::string>{ "march" });
		CHECK(ThemeChain(Styles(), "").empty());
		CHECK(ThemeChain(nullptr, "pit") == std::vector<std::string>{ "pit" });

		const std::vector<std::string> Ids = { "decor_tree", "tree" };
		CHECK(ExpandThemed(Ids, "pit", Styles())
			== std::vector<std::string>{ "decor_tree@pit", "tree@pit", "decor_tree@march", "tree@march", "decor_tree", "tree" });
		// one theme: exactly the old order; no theme: the ids
		CHECK(ExpandThemed(Ids, "march", Styles()) == ExpandThemed(Ids, std::string_view("march")));
		CHECK(ExpandThemed(Ids, "", Styles()) == Ids);
		CHECK(ExpandThemed(Ids, std::vector<std::string>{}) == Ids);

		// a looping / malformed table ends the chain
		abyss::JsonValue Loop;
		REQUIRE(abyss::ParseJson(R"({"themeParents": {"a": "b", "b": "a", "deep1": "deep2", "deep2": "deep3", "deep3": "deep4",
			"deep4": "deep5", "self": "self"}})", Loop));
		CHECK(ThemeChain(&Loop, "a") == std::vector<std::string>{ "a", "b" });
		CHECK(ThemeChain(&Loop, "self") == std::vector<std::string>{ "self" });
		CHECK(ThemeChain(&Loop, "deep1") == std::vector<std::string>{ "deep1", "deep2", "deep3", "deep4", "deep5" });
		// the cap: a chain cut at MaxThemeChain has lost its root, and says so (the zone actor warns); loops and the
		// shipped chain are whole
		std::string Long = "{\"themeParents\": {";
		for (int I = 0; I < 12; ++I)
		{
			Long += (I > 0 ? ", \"t" : "\"t") + std::to_string(I) + "\": \"t" + std::to_string(I + 1) + "\"";
		}
		Long += "}}";
		abyss::JsonValue LongTable;
		REQUIRE(abyss::ParseJson(Long, LongTable));
		CHECK(ThemeChain(&LongTable, "t0").size() == MaxThemeChain);
		CHECK(ThemeChainCut(&LongTable, "t0"));
		CHECK(!ThemeChainCut(&LongTable, "t" + std::to_string(12 - static_cast<int>(MaxThemeChain) + 1)));   // ends exactly at the cap
		CHECK(!ThemeChainCut(&Loop, "a"));
		CHECK(!ThemeChainCut(&Loop, "deep1"));
		CHECK(!ThemeChainCut(Styles(), "pit"));
		CHECK(!ThemeChainCut(nullptr, "pit"));

		// the shipped art: the Pit's own wall and stair, the march's look for everything else, the plain art last
		const std::vector<std::string> Wall = { "outcrop_plains", "wall_plains", "outcrop", "wall" };
		CHECK(Resolve(Wall, "pit").rfind("SM_Pit_Wall_", 0) == 0);
		CHECK(Resolve({ "exit_portal" }, "pit") == "SM_Pit_AshSteps");
		CHECK(Resolve(Wall, "march") != Resolve(Wall, "pit"));
		CHECK(Resolve(Wall, "march") != Resolve(Wall, ""));
		for (const char* Id : { "elite_crown", "exit_gate_sealed", "camp_campfire", "decor_tree" })
		{
			const std::string IdName(Id);
			CAPTURE(IdName);
			CHECK(Resolve({ IdName }, "pit") == Resolve({ IdName }, "march"));
		}
		CHECK(!Resolve({ "elite_crown" }, "pit").empty());
		CHECK(Resolve({ "elite_crown" }, "pit") != Resolve({ "elite_crown" }, ""));
		// the terrain style: the Pit's own
		CHECK(StyleThemeName(Styles(), ThemeChain(Styles(), "pit"), "plains") == "pit");
		CHECK(StyleThemeName(Styles(), ThemeChain(Styles(), "march"), "plains") == "march");
		CHECK(StyleThemeName(Styles(), {}, "forest") == "forest");
		CHECK(StyleThemeName(Styles(), std::vector<std::string>{ "nostyle" }, "plains") == "plains");
		CHECK(Manifest().Get("terrain").Get("pit").IsObject());
	}

	TEST_CASE("recipe chain: exit.portal@pit before exit.portal@march before exit.portal; themed-only loops; named themes")
	{
		using namespace AbyssThemeRules;
		const std::vector<std::string> Pit = ThemeChain(Styles(), "pit");
		CHECK(RecipeChain("exit.portal", Pit) == std::vector<std::string>{ "exit.portal@pit", "exit.portal@march", "exit.portal" });
		CHECK(RecipeChain("camp.torch", { "march" }) == std::vector<std::string>{ "camp.torch@march", "camp.torch" });
		CHECK(RecipeChain("camp.torch@march", Pit) == std::vector<std::string>{ "camp.torch@march" });
		CHECK(RecipeChain("exit.sealed", Pit) == std::vector<std::string>{ "exit.sealed@pit", "exit.sealed@march" });
		CHECK(RecipeChain("exit.sealed", {}) == std::vector<std::string>{ "exit.sealed" });
		CHECK(RecipeChain("exit.portal", {}) == std::vector<std::string>{ "exit.portal" });
		// the recipes the chain and the cleanse name exist in the shipped library
		const abyss::JsonValue& Library = Recipes().Get("recipes");
		for (const std::string_view Id : { std::string_view("exit.portal@pit"), EmbersRecipe, LampLightRecipe, LampFlameRecipe,
				 NicheRecipe, MarkRecipe, std::string_view("ambient.pit_embers"), std::string_view("notify.FX_PitDig"),
				 std::string_view("notify.FX_PitCollapse") })
		{
			const std::string IdName(Id);
			CAPTURE(IdName);
			CHECK(Library.Get(Id).IsObject());
		}
		// the first recipe of the stair's chain that exists is the Pit's own (no teal vortex)
		std::string First;
		for (const std::string& Id : RecipeChain("exit.portal", Pit))
		{
			if (First.empty() && Library.Get(Id).IsObject())
			{
				First = Id;
			}
		}
		CHECK(First == "exit.portal@pit");
	}

	TEST_CASE("wall yaw: 90 degree steps where the art says placement.yawStepDeg, the hashed yaw elsewhere")
	{
		for (uint32_t Hash : { 0u, 0x100u, 0x2FFu, 0x300u, 0x12345678u, 0xFFFFFFFFu, 0xABCDEF01u })
		{
			CAPTURE(Hash);
			CHECK(WallYawDeg(Hash, 90.0) == static_cast<float>((Hash >> 8) % 4u * 90u));
			CHECK(WallYawDeg(Hash, 0.0) == static_cast<float>((Hash >> 8) % 360u));
			CHECK(WallYawDeg(Hash, -5.0) == static_cast<float>((Hash >> 8) % 360u));
			CHECK(WallYawDeg(Hash, 45.0) == static_cast<float>((Hash >> 8) % 8u * 45u));
			CHECK(WallYawDeg(Hash, 360.0) == 0.f);   // a full turn: never turn (re-pinned in the stage P repair, P-L5)
			CHECK(WallYawDeg(Hash, 200.0) == 0.f);   // fewer than two steps fit: no turn
		}
		// the manifest: the crypt blocks carry the step, no other wall art does
		int Stepped = 0;
		for (const abyss::JsonMember& Asset : Manifest().Get("assets").Members())
		{
			const double Step = Asset.value.Get("placement").Get("yawStepDeg").AsDouble(0.0);
			if (Step > 0.0)
			{
				++Stepped;
				CHECK(Step == 90.0);
				CHECK(std::string(Asset.key).rfind("SM_Pit_Wall_", 0) == 0);
			}
		}
		CHECK(Stepped == 3);
		const std::string MarchWall = Resolve({ "outcrop_plains" }, "march");
		REQUIRE(!MarchWall.empty());
		CHECK(Manifest().Get("assets").Get(MarchWall).Get("placement").Get("yawStepDeg").AsDouble(0.0) == 0.0);
	}

	TEST_CASE("the Pit's grid: blocks only on wall tiles that touch an open tile; the stair faces the floor")
	{
		REQUIRE(PitMap() != nullptr);
		const abyss::ZoneGrid& Grid = PitGrid();
		REQUIRE(Grid.Cols() == 40);
		REQUIRE(Grid.Rows() == 40);
		auto IsWall = [&Grid](int C, int R)
		{
			const abyss::TileType T = Grid.Tile(C, R);
			return T == abyss::TileType::Wall || T == abyss::TileType::CampWall;
		};
		int Walls = 0;
		int Blocks = 0;
		for (int R = 0; R < Grid.Rows(); ++R)
		{
			for (int C = 0; C < Grid.Cols(); ++C)
			{
				if (!IsWall(C, R))
				{
					continue;
				}
				++Walls;
				Blocks += BuildsWallBlock(true, WallTouchesOpen(Grid.Cols(), Grid.Rows(), C, R, IsWall)) ? 1 : 0;
			}
		}
		CHECK(Walls == 1020);
		CHECK(Blocks == 240);   // implementation-plan.md 3D note 23
		CHECK(BuildsWallBlock(false, false));   // the surface keeps every outcrop
		CHECK(BuildsWallBlock(false, true));
		CHECK(!BuildsWallBlock(true, false));
		// the map's border never looks outside the grid for an open tile
		CHECK(!WallTouchesOpen(3, 3, 0, 0, [](int, int) { return true; }));
		CHECK(WallTouchesOpen(3, 3, 0, 0, [](int C, int R) { return !(C == 1 && R == 1); }));

		// every wall overlay's tile keeps its block (it stands proud of it)
		for (const abyss::SetPiecePlacement& P : PitPlacements())
		{
			if (P.def->onWall)
			{
				const abyss::TilePos Tile = abyss::RoundToTile(P.def->tile);
				CAPTURE(P.def->id);
				REQUIRE(IsWall(Tile.col, Tile.row));
				CHECK(WallTouchesOpen(Grid.Cols(), Grid.Rows(), Tile.col, Tile.row, IsWall));
			}
		}

		// the stair: exit (5,10), the wall row behind it at row 11, the floor toward -row
		const abyss::SubDungeonDef* Sub = Data().World().FindEnterableSubDungeon(PitId);
		REQUIRE(Sub != nullptr);
		auto IsWalkable = [&Grid](int C, int R) { return Grid.Walkable(C, R); };
		const FFloorSide Side = ExitFloorSide(Sub->exitPos.col, Sub->exitPos.row, IsWalkable);
		REQUIRE(Side.bFound);
		CHECK(Side.Dc == 0);
		CHECK(Side.Dr == -1);
		CHECK(Near(FacingDeg(Side.Dc, Side.Dr), -90.0));
		// facing + the mesh yaw offset is the overlay yaw the art kit names (yaw 180 faces -row)
		const double ActorYaw = std::fmod(FacingDeg(Side.Dc, Side.Dr) - 90.0 + 720.0, 360.0);
		CHECK(Near(ActorYaw, OverlayYawDeg(Side.Dc, Side.Dr)));
		CHECK(Near(OverlayYawDeg(0, -1), 180.0));
		CHECK(Near(OverlayYawDeg(-1, 0), 90.0));
		CHECK(Near(OverlayYawDeg(0, 1), 0.0));
		CHECK(Near(OverlayYawDeg(1, 0), 270.0));
		// synthetic: a tile open on two sides takes -row first; one with no wall behind any side takes the first open
		CHECK(ExitFloorSide(1, 1, [](int C, int R) { return (C == 1 && R == 0) || (C == 0 && R == 1); }).Dr == -1);
		const FFloorSide Open = ExitFloorSide(1, 1, [](int, int) { return true; });
		CHECK(Open.bFound);
		CHECK(Open.Dr == -1);
		CHECK(!ExitFloorSide(1, 1, [](int, int) { return false; }).bFound);
	}

	TEST_CASE("wall overlays: the core's placement is the art kit's yaw, on the wall tile's centre")
	{
		const std::vector<abyss::SetPiecePlacement> Placements = PitPlacements();
		CHECK(Placements.size() == 87);
		int OnWall = 0;
		for (const abyss::SetPiecePlacement& P : Placements)
		{
			if (!P.def->onWall)
			{
				continue;
			}
			++OnWall;
			CAPTURE(P.def->id);
			const abyss::TilePos Tile = abyss::RoundToTile(P.def->tile);
			const int Dc = static_cast<int>(std::lround(P.def->toward.x)) - Tile.col;
			const int Dr = static_cast<int>(std::lround(P.def->toward.y)) - Tile.row;
			REQUIRE(std::abs(Dc) + std::abs(Dr) == 1);
			// the yaw the UE build places is the core's (AbyssSetPieces.cpp passes Placement.yawDeg through)
			CHECK(Near(P.yawDeg, OverlayYawDeg(Dc, Dr), 1e-6));
			// anchored on the wall tile: the offset only slides along the wall (the tag strands), never off the tile
			CHECK(std::fabs(P.pos.x - P.def->tile.x) <= 0.45 + 1e-9);
			CHECK(std::fabs(P.pos.y - P.def->tile.y) <= 0.45 + 1e-9);
			CHECK(Near((P.pos.x - P.def->tile.x) * Dc + (P.pos.y - P.def->tile.y) * Dr, 0.0));
		}
		CHECK(OnWall == 78);
	}

	TEST_CASE("the entrance piece: open while unsealed, the sealed twin while sealed")
	{
		const std::vector<std::string> Authored = { "setpiece_pit_mouth_open", "setpiece_pit_mouth_sealed" };
		CHECK(EntranceIdChain(Authored, false) == Authored);
		CHECK(EntranceIdChain(Authored, true) == std::vector<std::string>{ "setpiece_pit_mouth_sealed" });
		CHECK(EntranceIdChain({ "setpiece_pit_mouth_open" }, true) == std::vector<std::string>{ "setpiece_pit_mouth_sealed" });
		CHECK(EntranceIdChain({ "setpiece_gate" }, true) == std::vector<std::string>{ "setpiece_gate" });
		// the shipped piece and both arts
		const abyss::SetPieceZoneDef* Zone = Data().SetPieces().ForZone("emerald_plains");
		REQUIRE(Zone != nullptr);
		const abyss::SetPieceDef* Mouth = nullptr;
		for (const abyss::SetPieceDef& Def : Zone->pieces)
		{
			if (Def.id == "pit_mouth")
			{
				Mouth = &Def;
			}
		}
		REQUIRE(Mouth != nullptr);
		CHECK(Mouth->anchor == abyss::SetPieceAnchorKind::Entrance);
		CHECK(Mouth->gameId == "setpiece_pit_mouth_open");
		CHECK(Resolve({ "setpiece_pit_mouth_open" }, "march") == "SM_Prop_March_PitMouthOpen");
		CHECK(Resolve(EntranceIdChain({ Mouth->gameId }, true), "march") == "SM_Prop_March_PitMouthSealed");
	}

	TEST_CASE("dark.pit: the grade and the cleanse numbers are read from the recipe file")
	{
		const FCleanseSpec Spec = PitSpec();
		REQUIRE(Spec.bValid);
		REQUIRE(Spec.bHasCleanse);
		CHECK(Spec.Ambient[0] == 0.21);
		CHECK(Spec.Ambient[1] == 0.23);
		CHECK(Spec.Ambient[2] == 0.30);
		CHECK(Spec.AmbientCleansed[0] == 0.52);
		CHECK(Spec.AmbientCleansed[1] == 0.44);
		CHECK(Spec.AmbientCleansed[2] == 0.36);
		CHECK(Spec.Ceil == 1.12);
		CHECK(Spec.Hero.RadiusTiles == 5.6);
		CHECK(Spec.Hero.Power == 1.5);
		CHECK(Spec.Hero.Gain == 0.95);
		CHECK(Spec.Lamp.RadiusTiles == 2.9);
		CHECK(Spec.GradeMs == 6000.0);
		CHECK(Spec.LampFirstMs == 600.0);
		CHECK(Spec.LampStepMs == 420.0);
		CHECK(Spec.NicheAtMs == 600.0);
		CHECK(Spec.MarkStepMs == 180.0);
		// the two moments were found in the contract (a numeric field or the step that opens with the words), not defaulted:
		// a reworded sequence fails here instead of silently keeping 600 ms
		CHECK(Spec.bLampFirstRead);
		CHECK(Spec.bNicheAtRead);
		// Wave 3 close-out: the contract states the three moments as numbers beside lampStepMs (the sentences stay)
		const abyss::JsonValue& Cleanse = Recipes().Get("dark").Get("pit").Get("cleanse");
		CHECK(Cleanse.Get("lampFirstMs").AsDouble(-1.0) == 600.0);
		CHECK(Cleanse.Get("lampStepMs").AsDouble(-1.0) == 420.0);
		CHECK(Cleanse.Get("nicheAtMs").AsDouble(-1.0) == 600.0);
		CHECK(Cleanse.Get("markStepMs").AsDouble(-1.0) == 180.0);
		CHECK(Spec.Lamp.Gain == 0.85);
		CHECK(Spec.Lamp.Power == 1.4);
		CHECK(Spec.Lamp.Color[1] == 0.66);
		CHECK(Spec.MoodFrom == "pit");
		CHECK(Spec.MoodTo == "pit_cleansed");
		// the contract's own target mood is the one blended to
		{
			abyss::MapTheme Target = abyss::MapTheme::Plains;
			REQUIRE(AbyssThemeRules::FindCleansedMood(Data().World().moods, abyss::MapTheme::Pit, Target, Spec.MoodTo));
			CHECK(Target == abyss::MapTheme::PitCleansed);
			CHECK(!AbyssThemeRules::FindCleansedMood(Data().World().moods, abyss::MapTheme::Pit, Target, "no_such_mood"));
			CHECK(!AbyssThemeRules::FindCleansedMood(Data().World().moods, abyss::MapTheme::Pit, Target, "pit"));
		}
		// numeric fields win over the prose; without either the defaults stay and the flags say so
		{
			abyss::JsonValue Numeric;
			REQUIRE(abyss::ParseJson(R"({"lightRadius": {"hero": {"radiusTiles": 4, "gain": 1, "power": -2}},
				"cleanse": {"lampFirstMs": 250, "nicheAtMs": 900, "markStepMs": 90, "lampStepMs": 100,
				"sequence": [{"atMs": 600, "do": "the lamps light one by one"}]}})", Numeric));
			const FCleanseSpec Read = ParseCleanseSpec(Numeric);
			CHECK(Read.LampFirstMs == 250.0);
			CHECK(Read.NicheAtMs == 900.0);
			CHECK(Read.MarkStepMs == 90.0);
			CHECK(Read.bLampFirstRead);
			CHECK(Read.bNicheAtRead);
			CHECK(Read.Hero.Power == MinLightPower);   // a power the shader could not take is clamped where it is read
			abyss::JsonValue Mute;
			REQUIRE(abyss::ParseJson(R"({"lightRadius": {}, "cleanse": {"sequence": [{"atMs": 5, "do": "something else"}]}})", Mute));
			const FCleanseSpec Defaults = ParseCleanseSpec(Mute);
			CHECK(Defaults.LampFirstMs == 600.0);
			CHECK(!Defaults.bLampFirstRead);
			CHECK(!Defaults.bNicheAtRead);
		}
		CHECK(Spec.LampUnlit == "setpiece_pit_lamp");
		CHECK(Spec.LampLit == "setpiece_pit_lamp_lit");
		// both lamp meshes ship, with the sockets the sequence plays at
		for (const std::string& Id : { Spec.LampUnlit, Spec.LampLit })
		{
			const std::string Asset = Resolve({ Id }, "pit");
			CAPTURE(Id);
			REQUIRE(!Asset.empty());
			std::set<std::string> Sockets;
			for (const abyss::JsonValue& Socket : Manifest().Get("assets").Get(Asset).Get("sockets").Items())
			{
				Sockets.insert(std::string(Socket.Get("name").AsString()));
			}
			CHECK(Sockets.count(std::string(FlameSocket)) == 1);
			CHECK(Sockets.count(std::string(LightSocket)) == 1);
		}
		// no theme without a `dark` entry is graded
		CHECK(!ParseCleanseSpec(Recipes().Get("dark").Get("march")).bValid);
		CHECK(!ParseCleanseSpec(abyss::JsonValue()).bValid);
		// the multiplier: the ceiling under the hero, the bare ambient at the radius and beyond
		double M[3];
		GradeMultiplier(Spec, 0.0, 0.0, M);
		CHECK(M[0] == 1.12);
		CHECK(Near(M[2], 0.30 + 0.80 * 0.95));
		GradeMultiplier(Spec, 0.0, 5.6, M);
		CHECK(M[0] == 0.21);
		GradeMultiplier(Spec, 0.0, 50.0, M);
		CHECK(M[2] == 0.30);
		GradeMultiplier(Spec, 0.0, 2.8, M);
		CHECK(Near(M[0], 0.21 + 0.95 * std::pow(0.5, 1.5)));
		GradeMultiplier(Spec, 1.0, 50.0, M);
		CHECK(M[0] == 0.52);
		GradeMultiplier(Spec, 0.5, 50.0, M);
		CHECK(Near(M[1], 0.335));
	}

	TEST_CASE("lamps: fourteen, nearest first, 420 ms apart from 600 ms")
	{
		const FCleanseSpec Spec = PitSpec();
		const std::vector<abyss::Vec2> Lamps = PitLamps(Spec);
		REQUIRE(Lamps.size() == 14);
		const abyss::Vec2 Cut{ 28.0, 26.0 };   // the Pitwarden's tile: where the cleanse usually happens
		const std::vector<int> Order = LampOrder(Lamps, Cut);
		REQUIRE(Order.size() == 14);
		std::set<int> Seen(Order.begin(), Order.end());
		CHECK(Seen.size() == 14);
		for (size_t K = 1; K < Order.size(); ++K)
		{
			const double A = TileDist(Lamps[static_cast<size_t>(Order[K - 1])], Cut);
			const double B = TileDist(Lamps[static_cast<size_t>(Order[K])], Cut);
			CHECK(A <= B);
			if (A == B)
			{
				CHECK(Order[K - 1] < Order[K]);   // ties in placement order
			}
		}
		// ties, synthetic
		CHECK(LampOrder({ { 2, 0 }, { 0, 2 }, { 1, 0 }, { -2, 0 } }, { 0, 0 }) == std::vector<int>{ 2, 0, 1, 3 });
		CHECK(LampOrder({}, { 0, 0 }).empty());

		CHECK(LampLitAtMs(Spec, 0) == 600.0);
		CHECK(LampLitAtMs(Spec, 1) == 1020.0);
		CHECK(LampLitAtMs(Spec, 13) == 6060.0);
		CHECK(LampsLitAt(Spec, 14, 0.0) == 0);
		CHECK(LampsLitAt(Spec, 14, 599.9) == 0);
		CHECK(LampsLitAt(Spec, 14, 600.0) == 1);
		CHECK(LampsLitAt(Spec, 14, 1019.9) == 1);
		CHECK(LampsLitAt(Spec, 14, 1020.0) == 2);
		CHECK(LampsLitAt(Spec, 14, 6059.9) == 13);
		CHECK(LampsLitAt(Spec, 14, 6060.0) == 14);
		CHECK(LampsLitAt(Spec, 14, 1.0e9) == 14);
		CHECK(LampsLitAt(Spec, 0, 1.0e9) == 0);
		for (int K = 0; K < 14; ++K)
		{
			CHECK(LampsLitAt(Spec, 14, LampLitAtMs(Spec, K)) == K + 1);
		}
		CHECK(GradeAlpha(Spec, -1.0) == 0.0);
		CHECK(GradeAlpha(Spec, 3000.0) == 0.5);
		CHECK(GradeAlpha(Spec, 6000.0) == 1.0);
		CHECK(GradeAlpha(Spec, 9000.0) == 1.0);
		CHECK(MarkNumber("mark_1") == 1);
		CHECK(MarkNumber("mark_5") == 5);
		CHECK(MarkNumber("mark_") == 0);
		CHECK(MarkNumber("mark_x") == 0);
		CHECK(MarkNumber("niche") == 0);
		// the marks glow in number order; a gap or a doubled number leaves no hole
		CHECK(MarkOrder({ 3, 0, 1, 5, 2, 4 }) == std::vector<int>{ 2, 4, 0, 5, 3 });
		CHECK(MarkOrder({ 3, 1, 3, 0 }) == std::vector<int>{ 1, 0 });
		CHECK(MarkOrder({}).empty());
		CHECK(MarkAtMs(Spec, 1) == 780.0);
		CHECK(MarkAtMs(Spec, 5) == 1500.0);
		CHECK(SequenceEndMs(Spec, 14, 5) == 6060.0);
		CHECK(SequenceEndMs(Spec, 3, 5) == 6000.0);
		CHECK(SequenceEndMs(Spec, 0, 0) == 6000.0);
		// the Flame Niche carries the niche and the five marks
		const std::string Niche = Resolve({ "setpiece_pit_flame_niche" }, "pit");
		REQUIRE(!Niche.empty());
		int Marks = 0;
		bool bNiche = false;
		for (const abyss::JsonValue& Socket : Manifest().Get("assets").Get(Niche).Get("sockets").Items())
		{
			Marks += MarkNumber(Socket.Get("name").AsString()) > 0 ? 1 : 0;
			bNiche = bNiche || Socket.Get("name").AsString() == NicheSocket;
		}
		CHECK(Marks == 5);
		CHECK(bNiche);
	}

	TEST_CASE("the cleanse sequence: every lamp once, in order, whatever the frame times; the end state at once on a later visit")
	{
		const FCleanseSpec Spec = PitSpec();
		const std::vector<abyss::Vec2> Lamps = PitLamps(Spec);
		const abyss::Vec2 Pos{ 28.0, 26.0 };
		const std::vector<int> Order = LampOrder(Lamps, Pos);
		for (const double FrameMs : { 16.6667, 33.3, 250.0, 2000.0, 20000.0 })
		{
			CAPTURE(FrameMs);
			FCleanseState State = EnterState(false, 14);
			CHECK(State.Phase == ECleansePhase::Dark);
			CHECK(StateAlpha(Spec, State, 5000.0) == 0.0);
			CHECK(AdvanceCleanse(Spec, State, 5, 5000.0).LightLamps.empty());   // nothing before the event
			StartCleanse(State, Lamps, Pos, 10000.0);
			CHECK(State.Phase == ECleansePhase::Playing);
			std::vector<int> Lit;
			std::vector<int> Marks;
			int Niches = 0;
			int Finishes = 0;
			double LastAlpha = 0.0;
			for (double Now = 10000.0; Now <= 10000.0 + 30000.0; Now += FrameMs)
			{
				const FCleanseStep Step = AdvanceCleanse(Spec, State, 5, Now);
				for (const int Lamp : Step.LightLamps)
				{
					// never before its time
					CHECK(Now - 10000.0 >= LampLitAtMs(Spec, static_cast<int>(Lit.size())));
					Lit.push_back(Lamp);
				}
				Marks.insert(Marks.end(), Step.PlayMarks.begin(), Step.PlayMarks.end());
				Niches += Step.bPlayNiche ? 1 : 0;
				Finishes += Step.bJustFinished ? 1 : 0;
				const double Alpha = StateAlpha(Spec, State, Now);
				CHECK(Alpha >= LastAlpha);
				LastAlpha = Alpha;
			}
			CHECK(Lit == Order);
			CHECK(Marks == std::vector<int>{ 1, 2, 3, 4, 5 });
			CHECK(Niches == 1);
			CHECK(Finishes == 1);
			CHECK(State.Phase == ECleansePhase::Lit);
			CHECK(State.bFinished);
			CHECK(State.LampsLit == 14);
			CHECK(LastAlpha == 1.0);
			// a second EvAreaCleansed (never sent; a defensive rule) restarts nothing
			StartCleanse(State, Lamps, Pos, 99999.0);
			CHECK(State.Phase == ECleansePhase::Lit);
		}
		// at 60 Hz the lamps come one per step of 420 ms: lamp k inside the frame after its time
		FCleanseState State = EnterState(false, 14);
		StartCleanse(State, Lamps, Pos, 0.0);
		CHECK(AdvanceCleanse(Spec, State, 5, 599.0).LightLamps.empty());
		CHECK(AdvanceCleanse(Spec, State, 5, 600.0).LightLamps == std::vector<int>{ Order[0] });
		CHECK(AdvanceCleanse(Spec, State, 5, 1019.0).LightLamps.empty());
		CHECK(AdvanceCleanse(Spec, State, 5, 1021.0).LightLamps == std::vector<int>{ Order[1] });
		CHECK(Near(StateAlpha(Spec, State, 1500.0), 0.25));
		// entering an already cleansed Pit: lit at once, no sequence, nothing to play
		FCleanseState Later = EnterState(true, 14);
		CHECK(Later.Phase == ECleansePhase::Lit);
		CHECK(Later.LampsLit == 14);
		CHECK(!Later.bFinished);
		CHECK(StateAlpha(Spec, Later, 0.0) == 1.0);
		const FCleanseStep None = AdvanceCleanse(Spec, Later, 5, 1.0e6);
		CHECK(None.LightLamps.empty());
		CHECK(!None.bPlayNiche);
		CHECK(None.PlayMarks.empty());
		CHECK(!NichePending(Later, 5));
	}

	TEST_CASE("the cleanse in the Cut: the Flame Niche's glow and marks wait until the hero sees the niche")
	{
		// chapter1.md 9.2: the Pitwarden stands at (28, 26), the niche at (20, 38) - 14.4 tiles, out of view. The glow and
		// the five marks are held (through the sequence's end) and play when the hero walks up for S7's last objective.
		const FCleanseSpec Spec = PitSpec();
		const std::vector<abyss::Vec2> Lamps = PitLamps(Spec);
		const abyss::Vec2 Cut{ 28.0, 26.0 };
		const abyss::Vec2 Niche{ 20.0, 38.0 };
		CHECK(!NicheInView(Niche, Cut));
		CHECK(NicheInView(Niche, abyss::Vec2{ 20.0, 36.0 }));   // the explore spot of S7
		CHECK(NicheInView(Niche, abyss::Vec2{ 20.0, 31.0 }));
		CHECK(!NicheInView(Niche, abyss::Vec2{ 20.0, 30.9 }));
		for (const double FrameMs : { 16.6667, 250.0 })
		{
			CAPTURE(FrameMs);
			FCleanseState State = EnterState(false, 14);
			StartCleanse(State, Lamps, Cut, 0.0);
			CHECK(NichePending(State, 5));
			int Niches = 0;
			std::vector<int> Marks;
			std::vector<double> MarkTimes;
			double NicheAt = -1.0;
			const double SeenAt = 20000.0;   // the hero reaches the niche 20 s after the cleanse
			for (double Now = 0.0; Now <= 40000.0; Now += FrameMs)
			{
				const FCleanseStep Step = AdvanceCleanse(Spec, State, 5, Now, /*bNicheInView*/ Now >= SeenAt);
				if (Step.bPlayNiche)
				{
					++Niches;
					NicheAt = Now;
				}
				for (const int Mark : Step.PlayMarks)
				{
					Marks.push_back(Mark);
					MarkTimes.push_back(Now);
				}
				if (Now < SeenAt)
				{
					CHECK(!Step.bPlayNiche);
					CHECK(Step.PlayMarks.empty());
				}
				if (Now > 7000.0 && Now < SeenAt)
				{
					CHECK(State.Phase == ECleansePhase::Lit);   // the sequence itself ended on time
					CHECK(State.bFinished);
					CHECK(NichePending(State, 5));
				}
			}
			CHECK(Niches == 1);
			CHECK(NicheAt >= SeenAt);
			CHECK(NicheAt < SeenAt + FrameMs);
			CHECK(Marks == std::vector<int>{ 1, 2, 3, 4, 5 });
			for (size_t K = 0; K < MarkTimes.size(); ++K)
			{
				// 180 ms apart from the moment the glow played, never all at once
				CHECK(MarkTimes[K] >= NicheAt + static_cast<double>(K + 1) * Spec.MarkStepMs - 1.0e-6);
				CHECK(MarkTimes[K] < NicheAt + static_cast<double>(K + 1) * Spec.MarkStepMs + FrameMs);
			}
			CHECK(!NichePending(State, 5));
			CHECK(State.LampsLit == 14);
		}
		// in view from the start: the contract's schedule (the niche at 600 ms, mark k at 600 + 180 k)
		FCleanseState Near = EnterState(false, 14);
		StartCleanse(Near, Lamps, abyss::Vec2{ 20.0, 36.0 }, 0.0);
		CHECK(!AdvanceCleanse(Spec, Near, 5, 599.0, true).bPlayNiche);
		CHECK(AdvanceCleanse(Spec, Near, 5, 601.0, true).bPlayNiche);
		CHECK(AdvanceCleanse(Spec, Near, 5, 779.0, true).PlayMarks.empty());
		CHECK(AdvanceCleanse(Spec, Near, 5, 781.0, true).PlayMarks == std::vector<int>{ 1 });
		CHECK(AdvanceCleanse(Spec, Near, 5, 1500.0, true).PlayMarks == std::vector<int>{ 2, 3, 4, 5 });
	}

	TEST_CASE("mood: pit, the blend to pit_cleansed over the grade, the embers from the first moment")
	{
		using namespace AbyssThemeRules;
		const abyss::ZoneMoodTables& Moods = Data().World().moods;
		FMoodInputs In;
		In.Moods = &Moods;
		In.Map = PitMap();
		In.ZoneId = PitId;
		In.GeneratorTheme = PitMap()->theme;
		In.bHasHero = true;
		In.HeroCol = 5.0;
		In.HeroRow = 5.0;
		const FMoodState Dark = EffectiveMood(In);
		CHECK(Dark.Base == abyss::MapTheme::Pit);
		CHECK(Dark.Ambience.empty());
		CHECK(Near(Dark.LightScale, 0.5f, 1e-6));
		CHECK(Dark.bLightScaleOverride);
		abyss::MapTheme Cleansed = abyss::MapTheme::Plains;
		REQUIRE(FindCleansedMood(Moods, abyss::MapTheme::Pit, Cleansed));
		CHECK(Cleansed == abyss::MapTheme::PitCleansed);
		CHECK(!FindCleansedMood(Moods, abyss::MapTheme::March, Cleansed));
		CHECK(!FindCleansedMood(Moods, abyss::MapTheme::Plains, Cleansed));

		FMoodState None = Dark;
		BlendCleansed(None, Moods, 0.0, true);
		CHECK(None.CleanseAlpha == 0.0);
		CHECK(None.Mood.ambient == Dark.Mood.ambient);
		CHECK(None.Ambience.empty());
		CHECK(!LookChanged(Dark, None));

		FMoodState Start = Dark;
		BlendCleansed(Start, Moods, 0.02, true);
		CHECK(Start.Ambience == "pit_embers");   // the embers start with the cleanse, not at the blend's middle
		CHECK(AmbienceRecipeId(Start) == "ambient.pit_embers");
		CHECK(AmbienceChanged(Dark, Start));
		CHECK(LookChanged(Dark, Start));
		CHECK(Start.Mood.theme == abyss::MapTheme::Pit);

		FMoodState Half = Dark;
		BlendCleansed(Half, Moods, 0.5, true);
		CHECK(Near(Half.LightScale, 0.4f, 1e-6));
		CHECK(Half.Mood.theme == abyss::MapTheme::PitCleansed);
		FMoodState Lit = Dark;
		BlendCleansed(Lit, Moods, 1.0, true);
		const abyss::ZoneMoodDef& Def = Moods.moods[static_cast<size_t>(abyss::MapTheme::PitCleansed)];
		CHECK(Lit.Mood.ambient == Def.ambient);
		CHECK(Lit.Mood.fogColor == Def.fogColor);
		CHECK(Lit.Mood.saturation == Def.saturation);
		CHECK(Near(Lit.LightScale, 0.3f, 1e-6));
		CHECK(Lit.Dominant == abyss::MapTheme::PitCleansed);
		CHECK(Lit.CleanseAlpha == 1.0);
		// the last step lands exactly although it is below the re-apply epsilon
		FMoodState Almost = Dark;
		BlendCleansed(Almost, Moods, 0.995, true);
		CHECK(LookChanged(Almost, Lit));
		FMoodState Step = Dark;
		BlendCleansed(Step, Moods, 0.503, true);
		CHECK(!LookChanged(Half, Step));
		// the march has no cleansed mood: nothing blends there
		In.Map = Data().FindMap("emerald_plains");
		In.ZoneId = "emerald_plains";
		FMoodState March = EffectiveMood(In);
		const FMoodState Before = March;
		BlendCleansed(March, Moods, 1.0, true);
		CHECK(March.Mood.ambient == Before.Mood.ambient);
		CHECK(March.Ambience == Before.Ambience);
		CHECK(March.CleanseAlpha == 0.0);
	}

	TEST_CASE("staggers: the rank rises along its line, whatever the entity ids are")
	{
		CHECK(RiseDelayMs(0) == 0.0);
		CHECK(RiseDelayMs(1) == 50.0);
		CHECK(RiseDelayMs(5) == 250.0);
		// a delayed Rise ends when the core's does; the delay never eats more than 40 % of the clip
		const FStaggeredStart Late = StaggerStart(1000.0, 1300.0, RiseDelayMs(5));
		CHECK(Late.StartMs == 1250.0);
		CHECK(Late.DurationMs == 1050.0);
		CHECK(Late.StartMs + Late.DurationMs == 2300.0);
		const FStaggeredStart Capped = StaggerStart(0.0, 500.0, 300.0);
		CHECK(Capped.StartMs == 200.0);
		CHECK(Capped.DurationMs == 300.0);
		CHECK(StaggerStart(10.0, 0.0, 100.0).StartMs == 10.0);
		CHECK(StaggerStart(10.0, 800.0, 0.0).DurationMs == 800.0);
		CHECK(RiseDelayMs(100) == RiseStaggerMaxMs);
		CHECK(RiseDelayMs(-3) == 0.0);
		// the Helm Wall's six rise inside half a second, well inside the Rise clip the core roots them for
		CHECK(RiseDelayMs(5) < 500.0);
		// the rank is the place along the line, not the id: any order of arrival gives 0..5 from one end to the other
		const std::vector<abyss::Vec2> Shuffled = { { 22, 11 }, { 25, 11 }, { 20, 11 }, { 23, 11 }, { 21, 11 }, { 24, 11 } };
		CHECK(LineRanks(Shuffled) == std::vector<int>{ 2, 5, 0, 3, 1, 4 });
		// a column (the guards' line steps two rows), a single monster, nobody
		CHECK(LineRanks({ { 9, 14 }, { 9, 10 }, { 9, 12 } }) == std::vector<int>{ 2, 0, 1 });
		CHECK(LineRanks({ { 4, 4 } }) == std::vector<int>{ 0 });
		CHECK(LineRanks({}).empty());
		// two on one tile: the given order, every rank once
		CHECK(LineRanks({ { 1, 1 }, { 1, 1 }, { 0, 1 } }) == std::vector<int>{ 1, 2, 0 });
		// the rank in the data: six footmen on (20..25, 11), dormant with a Rise; the guard dormant without one
		const abyss::SubDungeonDef* Sub = Data().World().FindEnterableSubDungeon(PitId);
		REQUIRE(Sub != nullptr);
		int Dormant = 0;
		for (const abyss::MapSpawnDef& Spawn : Sub->map.spawns)
		{
			Dormant += Spawn.dormant.present || Spawn.guard ? Spawn.count : 0;
		}
		CHECK(Dormant == 8);   // the rank's six and the two guards (AutoPilotStats::dormantWakes on a clean run)
		// every dormant line of the shipped Pit: the members' ranks rise monotonically along the line, delays 0, 50, 100 ...
		for (const abyss::MapSpawnDef& Spawn : Sub->map.spawns)
		{
			if (!Spawn.dormant.present || (Spawn.dormant.lineStep.col == 0 && Spawn.dormant.lineStep.row == 0))
			{
				continue;
			}
			std::vector<abyss::Vec2> Line;
			for (int K = Spawn.count - 1; K >= 0; --K)   // handed over back to front: the order must not matter
			{
				const abyss::TilePos Tile = abyss::DormantLineTile(Spawn, K);
				Line.push_back(abyss::Vec2{ static_cast<double>(Tile.col), static_cast<double>(Tile.row) });
			}
			const std::vector<int> Ranks = LineRanks(Line);
			REQUIRE(Ranks.size() == Line.size());
			for (size_t A = 0; A < Line.size(); ++A)
			{
				for (size_t B = 0; B < Line.size(); ++B)
				{
					const double MainA = Line[A].x != Line[B].x ? Line[A].x : Line[A].y;
					const double MainB = Line[A].x != Line[B].x ? Line[B].x : Line[B].y;
					if (MainA < MainB)
					{
						CHECK(Ranks[A] < Ranks[B]);
						CHECK(RiseDelayMs(Ranks[A]) < RiseDelayMs(Ranks[B]));
					}
				}
			}
		}
	}

	// Wave 3 close-out (3D note 36): what still stands at the cleanse lies down body by body.
	TEST_CASE("the cleanse's collapse: nearest to the hero first, 120 ms apart, the last at most 1.2 s late")
	{
		using namespace AbyssSubDungeonRules;
		CHECK(CollapseStepMs == 120.0);
		CHECK(CollapseMaxMs == 1200.0);
		CHECK(CollapseDelayMs(0) == 0.0);
		CHECK(CollapseDelayMs(1) == 120.0);
		CHECK(CollapseDelayMs(4) == 480.0);
		CHECK(CollapseDelayMs(10) == 1200.0);
		CHECK(CollapseDelayMs(11) == 1200.0);   // capped
		CHECK(CollapseDelayMs(1000) == 1200.0);
		CHECK(CollapseDelayMs(-3) == 0.0);

		// S7 leaves five standing at the most: the last falls 480 ms after the first
		const abyss::QuestDef* Quest = Data().FindQuest("q_rf_the_pit");
		REQUIRE(Quest != nullptr);
		REQUIRE(Quest->objectives.size() == 3);
		CHECK(CollapseDelayMs(Quest->objectives[1].maxRemaining - 1) == 480.0);

		const abyss::Vec2 Hero(20.0, 30.0);
		// the snapshot's order has nothing to do with the distance
		const std::vector<abyss::Vec2> Standing{ abyss::Vec2(26, 30), abyss::Vec2(20, 31), abyss::Vec2(17, 26), abyss::Vec2(20, 33),
			abyss::Vec2(12, 30) };
		const std::vector<int> Ranks = CollapseRanks(Standing, Hero);
		CHECK(Ranks == std::vector<int>{ 3, 0, 2, 1, 4 });   // 6, 1, 5, 3, 8 tiles
		// every rank once, whatever the input
		std::vector<int> Seen(Ranks.size(), 0);
		for (int Rank : Ranks)
		{
			REQUIRE(Rank >= 0);
			REQUIRE(Rank < static_cast<int>(Ranks.size()));
			++Seen[static_cast<size_t>(Rank)];
		}
		CHECK(Seen == std::vector<int>(Ranks.size(), 1));
		// ties keep the given order; a body on the hero's tile is first
		CHECK(CollapseRanks({ abyss::Vec2(23, 30), abyss::Vec2(20, 33), abyss::Vec2(20, 30), abyss::Vec2(17, 30) }, Hero)
			== std::vector<int>{ 1, 2, 0, 3 });
		CHECK(CollapseRanks({}, Hero).empty());
		CHECK(CollapseRanks({ abyss::Vec2(1, 1) }, Hero) == std::vector<int>{ 0 });
		// a position that is not a number falls last and takes no rank from the others
		const double NaN = std::nan("");
		CHECK(CollapseRanks({ abyss::Vec2(NaN, 30), abyss::Vec2(25, 30), abyss::Vec2(21, 30) }, Hero) == std::vector<int>{ 2, 1, 0 });
		// the same input gives the same ranks (no state)
		CHECK(CollapseRanks(Standing, Hero) == Ranks);
		// more bodies than the cap has steps: ranks 10 and up fall together at 1.2 s
		std::vector<abyss::Vec2> Crowd;
		for (int I = 0; I < 14; ++I)
		{
			Crowd.push_back(abyss::Vec2(20.0 + I, 30.0));
		}
		const std::vector<int> CrowdRanks = CollapseRanks(Crowd, Hero);
		double Latest = 0.0;
		for (size_t I = 0; I < Crowd.size(); ++I)
		{
			CHECK(CrowdRanks[I] == static_cast<int>(I));
			Latest = (std::max)(Latest, CollapseDelayMs(CrowdRanks[I]));
		}
		CHECK(Latest == CollapseMaxMs);

		// the actor's side: the cleanse's own Death is held until its delayed start, any other Death plays at once
		const double Armed = 50000.0;
		CHECK(!DefersCollapse(Armed, Armed, 0.0, Armed));                 // rank 0: on the core's time
		CHECK(DefersCollapse(Armed, Armed, 120.0, Armed));                // held
		CHECK(DefersCollapse(Armed, Armed, 120.0, Armed + 119.0));
		CHECK(!DefersCollapse(Armed, Armed, 120.0, Armed + 120.0));       // due
		CHECK(!DefersCollapse(Armed, Armed, 120.0, Armed + 5000.0));      // a long frame: it falls now
		CHECK(DefersCollapse(Armed + 16.7, Armed, 480.0, Armed + 16.7));  // the event's own step time
		CHECK(!DefersCollapse(Armed + 251.0, Armed, 480.0, Armed + 251.0));   // a later death is not the cleanse's
		CHECK(!DefersCollapse(Armed - 251.0, Armed, 480.0, Armed));
		CHECK(!DefersCollapse(Armed, Armed, -5.0, Armed));
	}

	TEST_CASE("only the hand-authored sub-dungeon is cut from rock; the generated caves keep the surface's zone rules")
	{
		// maps.json has three sub-dungeons and the core calls all three `inside`; void rock, the wall cull, the missing
		// skirt and the stair belong to the one with an authored map (stage P repair, P-E1).
		int Authored = 0;
		int Generated = 0;
		for (const char* Id : { "sub_rf_pit", "sub_dwarf_mine", "sub_demon_altar" })
		{
			CAPTURE(Id);
			const abyss::SubDungeonDef* Sub = Data().World().FindSubDungeon(Id);
			REQUIRE(Sub != nullptr);
			const bool bCut = CutFromRock(true, Sub->hasMap);
			CHECK(bCut == (std::string_view(Id) == PitId));
			Authored += bCut ? 1 : 0;
			Generated += bCut ? 0 : 1;
			// a cave's deep wall tile still gets its outcrop; the Pit's does not
			CHECK(BuildsWallBlock(bCut, /*bTouchesOpen*/ false) == !bCut);
			CHECK(BuildsWallBlock(bCut, /*bTouchesOpen*/ true));
		}
		CHECK(Authored == 1);
		CHECK(Generated == 2);
		CHECK(!CutFromRock(false, true));   // the surface, whatever the map
	}

	TEST_CASE("small rules: the spent guardian, the world map's zone, objectives by zone, the audio zone")
	{
		CHECK(ShowsEliteMarker(true, false));
		CHECK(!ShowsEliteMarker(true, true));
		CHECK(!ShowsEliteMarker(false, false));
		CHECK(WorldZoneId(true, "emerald_plains", PitId) == "emerald_plains");
		CHECK(WorldZoneId(false, "emerald_plains", "twilight_forest") == "twilight_forest");
		CHECK(WorldZoneId(true, "", PitId) == PitId);
		CHECK(ObjectiveInMap("emerald_plains", PitId, PitId));
		CHECK(!ObjectiveInMap("emerald_plains", PitId, "emerald_plains"));
		CHECK(ObjectiveInMap("emerald_plains", "", "emerald_plains"));
		CHECK(!ObjectiveInMap("emerald_plains", "", PitId));
		// the tracker: S7 is "of this place" inside the Pit (its zone is the parent's, two objectives name the Pit)
		CHECK(QuestInMap("emerald_plains", PitId, true, "emerald_plains", false));
		CHECK(QuestInMap("twilight_forest", PitId, true, "emerald_plains", true));
		CHECK(!QuestInMap("twilight_forest", PitId, true, "emerald_plains", false));
		CHECK(QuestInMap("emerald_plains", "emerald_plains", false, "", false));
		CHECK(!QuestInMap("emerald_plains", "twilight_forest", false, "emerald_plains", false));   // a parent only counts inside
		// the entrance prompt carries the place's name
		CHECK(EnterPrompt("Enter", "The Pit of the Faithful") == "Enter The Pit of the Faithful");
		CHECK(EnterPrompt("Enter", "") == "Enter");
		CHECK(EnterPrompt("\xE8\xBF\x9B\xE5\x85\xA5", "\xE4\xBF\xA1\xE8\x80\x85\xE4\xB9\x8B\xE5\x9D\x91")
			== "\xE8\xBF\x9B\xE5\x85\xA5\xE4\xBF\xA1\xE8\x80\x85\xE4\xB9\x8B\xE5\x9D\x91");
		CHECK(AudioZoneId(true, "emerald_plains", PitId, false) == "emerald_plains");
		CHECK(AudioZoneId(true, "emerald_plains", PitId, true) == PitId);
		CHECK(AudioZoneId(false, "", "emerald_plains", false) == "emerald_plains");
		// the ambience bed: the Pit has none of its own and keeps the march's (the chain), other maps as before
		CHECK(AbyssAmbienceRules::BedThemeNames(PitMap(), "plains", Styles()) == std::vector<std::string>{ "pit", "march" });
		CHECK(AbyssAmbienceRules::BedThemeNames(Data().FindMap("emerald_plains"), "plains", Styles())
			== std::vector<std::string>{ "march" });
		CHECK(AbyssAmbienceRules::BedThemeNames(Data().FindMap("twilight_forest"), "forest", Styles())
			== std::vector<std::string>{ "forest" });
		CHECK(AbyssAmbienceRules::BedThemeNames(nullptr, "abyss", nullptr) == std::vector<std::string>{ "abyss" });
		// S7 as shipped: the kill and the explore are done in the Pit, the quest is the surface's
		const abyss::QuestDef* Quest = Data().FindQuest("q_rf_the_pit");
		REQUIRE(Quest != nullptr);
		REQUIRE(Quest->objectives.size() == 3);
		CHECK(Quest->objectives[1].type == abyss::ObjectiveType::ClearArea);
		CHECK(Quest->objectives[2].type == abyss::ObjectiveType::Explore);
		CHECK(ObjectiveInMap(Quest->zone, Quest->objectives[2].zone, PitId));
		CHECK(!ObjectiveInMap(Quest->zone, Quest->objectives[2].zone, Quest->zone));
		// the quest card's rows have their strings (clear_area's type label, the Pit's name, the niche's)
		abyss::JsonValue En;
		REQUIRE(abyss::ParseJson(ReadBytes(UnrealDir() + "/Data/i18n_en.json"), En));
		auto HasString = [&En](std::string_view Key) { return En.Get("strings").Get(Key).IsString(); };
		CHECK(HasString("sys.quest.objType.clear_area"));
		CHECK(HasString("data.questTarget.sub_rf_pit"));
		CHECK(HasString("data.questTarget.zone_rf_flame_niche"));
		CHECK(HasString("data.subDungeon.sub_rf_pit.name"));
	}

	// Wave 3 close-out: the tracker's clear_area line ends in "N left" while the hero is in the Pit.
	TEST_CASE("the tracker's clear_area line: N left above the objective's maxRemaining, the guardian counted")
	{
		using namespace AbyssSubDungeonRules;
		const abyss::QuestDef* Quest = Data().FindQuest("q_rf_the_pit");
		REQUIRE(Quest != nullptr);
		REQUIRE(Quest->objectives.size() == 3);
		const abyss::QuestObjectiveDef& Clear = Quest->objectives[1];
		REQUIRE(Clear.type == abyss::ObjectiveType::ClearArea);
		CHECK(Clear.targetId == PitId);
		CHECK(Clear.maxRemaining == 5);
		const int32_t Max = Clear.maxRemaining;
		// the Pit as the hero walks in: every monster alive, the guardian among them
		CHECK(ClearAreaLeft(true, false, false, 23, Max, true) == 18);
		CHECK(ClearAreaLeft(true, false, false, 6, Max, true) == 1);
		CHECK(ClearAreaLeft(true, false, false, 6, Max, false) == 1);
		// five or fewer: only the guardian is left to kill; once he is dead there is no line (the objective completes,
		// or waits for its `after`)
		CHECK(ClearAreaLeft(true, false, false, 5, Max, true) == 1);
		CHECK(ClearAreaLeft(true, false, false, 1, Max, true) == 1);
		CHECK(ClearAreaLeft(true, false, false, 5, Max, false) == -1);
		CHECK(ClearAreaLeft(true, false, false, 0, Max, false) == -1);
		// not in the objective's sub-dungeon (the count is another zone's), done, or cleansed: no line
		CHECK(ClearAreaLeft(false, false, false, 23, Max, true) == -1);
		CHECK(ClearAreaLeft(true, true, false, 23, Max, true) == -1);
		CHECK(ClearAreaLeft(true, false, true, 23, Max, true) == -1);
		// a negative maxRemaining counts as 0
		CHECK(ClearAreaLeft(true, false, false, 3, -4, false) == 3);

		// the string, in the three locales (zh-TW through the exporter's conversion)
		auto Template = [](const char* Locale) {
			abyss::JsonValue Table;
			REQUIRE(abyss::ParseJson(ReadBytes(UnrealDir() + "/Data/i18n_" + Locale + ".json"), Table));
			const abyss::JsonValue& Text = Table.Get("strings").Get(ClearAreaLeftKey);
			REQUIRE(Text.IsString());
			return std::string(Text.AsString());
		};
		CHECK(ObjectiveProgressText(0, 1, 18, Template("en")) == "18 left");
		CHECK(ObjectiveProgressText(0, 1, 18, Template("zh-CN")) == "\xE5\x89\xA9\xE4\xBD\x99 18");   // 剩余 18
		// zh-TW is the web converter's output, which keeps 余 (as in the web's own "剩余 {time}" of the caravan page)
		CHECK(ObjectiveProgressText(0, 1, 1, Template("zh-TW")) == "\xE5\x89\xA9\xE4\xBD\x99 1");
		// no line, or a locale without the string: the plain count, capped
		CHECK(ObjectiveProgressText(0, 1, -1, Template("en")) == "0/1");
		CHECK(ObjectiveProgressText(0, 1, 18, "") == "0/1");
		CHECK(ObjectiveProgressText(0, 1, 18, "left") == "0/1");
		CHECK(ObjectiveProgressText(7, 5, -1, "") == "5/5");
		CHECK(ObjectiveProgressText(-2, 5, 0, "{n} left") == "0/5");
		CHECK(ObjectiveProgressText(0, 1, 3, "{n} of {n}") == "3 of 3");
	}

	TEST_CASE("the Pitwarden: the data's beats are the clips' own, his kit is in the manifest")
	{
		const abyss::MonsterDef* Def = Data().FindMonster("sub_pitwarden");
		REQUIRE(Def != nullptr);
		const std::string Asset = Resolve({ Def->spriteKey }, "pit");
		CHECK(Asset == "SK_Mon_Pitwarden");
		const abyss::JsonValue& Art = Manifest().Get("assets").Get(Asset);
		REQUIRE(Art.IsObject());
		std::map<std::string, const abyss::JsonValue*> Clips;
		for (const abyss::JsonValue& Clip : Art.Get("anims").Items())
		{
			Clips[std::string(Clip.Get("name").AsString())] = &Clip;
		}
		auto NotifyMs = [&Clips](const std::string& Clip, std::string_view Notify) -> double
		{
			const auto It = Clips.find(Clip);
			if (It == Clips.end())
			{
				return -1.0;
			}
			for (const abyss::JsonValue& N : It->second->Get("notifies").Items())
			{
				if (N.Get("name").AsString() == Notify)
				{
					return N.Get("ms").AsDouble(-1.0);
				}
			}
			return -1.0;
		};
		for (const char* Clip : { "Idle", "Idle_Order", "Walk", "Attack01", "Attack02", "Cast01", "Hurt", "Death", "Rise", "Stun" })
		{
			const std::string ClipName(Clip);
			CAPTURE(ClipName);
			CHECK(Clips.count(ClipName) == 1);
		}
		CHECK(NotifyMs("Attack01", "Contact") == 620.0);
		CHECK(NotifyMs("Attack02", "Contact") == 700.0);
		CHECK(NotifyMs("Cast01", "Plant") == 330.0);
		CHECK(NotifyMs("Cast01", "Contact") == 800.0);
		CHECK(NotifyMs("Cast01", "FX_PitDig") == 330.0);
		CHECK(NotifyMs("Cast01", "FX_PitCollapse") == 800.0);
		CHECK(NotifyMs("Idle_Order", "FX_PitDig") == 900.0);
		// each ability plays its clip at the clip's own beat: the core's windup is the Contact notify, play rate 1
		REQUIRE(Def->abilities.size() == 2);
		for (const abyss::MonsterAbilityDef& Ability : Def->abilities)
		{
			CAPTURE(Ability.id);
			const abyss::JsonValue& Listed = Art.Get("abilities").Get(Ability.id);
			REQUIRE(Listed.IsObject());
			CHECK(Listed.Get("clip").AsString() == Ability.clip);
			CHECK(Listed.Get("contactMs").AsDouble(0.0) == Ability.windupMs);
			const double Contact = NotifyMs(Ability.clip, "Contact");
			CHECK(Contact == Ability.windupMs);
			AbyssAnimActionRules::FRateInput Rate;
			Rate.BeatSec = static_cast<float>(Contact / 1000.0);
			Rate.ContactMs = Ability.windupMs;
			Rate.LengthSec = static_cast<float>(Clips[Ability.clip]->Get("lengthMs").AsDouble(0.0) / 1000.0);
			Rate.DurationMs = Clips[Ability.clip]->Get("lengthMs").AsDouble(0.0);
			CHECK(AbyssAnimActionRules::ActionPlayRate(Rate).Rate == doctest::Approx(1.0f));
		}
		CHECK(Def->abilities[0].kind == abyss::AbilityKind::Line);    // dig: the line telegraph
		CHECK(Def->abilities[1].kind == abyss::AbilityKind::Cone);    // spade_sweep: the cone telegraph
		// the spade in his right hand, the sigil's own socket, the portrait
		REQUIRE(Art.Get("attachments").Size() == 1);
		CHECK(Art.Get("attachments").At(0).Get("object").AsString() == "SM_MonWeapon_PitSpade");
		CHECK(Art.Get("attachments").At(0).Get("socket").AsString() == "weapon_r");
		CHECK(Art.Get("eliteMarker").Get("socket").AsString() == "fx_elite");
		bool bEliteSocket = false;
		for (const abyss::JsonValue& Socket : Art.Get("sockets").Items())
		{
			bEliteSocket = bEliteSocket || Socket.Get("name").AsString() == "fx_elite";
		}
		CHECK(bEliteSocket);
		CHECK(Art.Get("portrait").AsString() == "Portraits/T_UI_Portrait_Pitwarden.png");
		// the idle variant's timing rule
		using namespace AbyssAnimActionRules;
		CHECK(IdleVariantClip == "Idle_Order");
		// which shipped assets play it: one Idle_Order and no sibling (the footman's depends on his weapon look: none)
		std::map<std::string, std::string> Variants;
		for (const abyss::JsonMember& Entry : Manifest().Get("assets").Members())
		{
			std::vector<std::string> Names;
			for (const abyss::JsonValue& EntryClip : Entry.value.Get("anims").Items())
			{
				Names.push_back(std::string(EntryClip.Get("name").AsString()));
			}
			const std::string_view Variant = IdleVariantFor(Names);
			if (!Variant.empty())
			{
				Variants[std::string(Entry.key)] = std::string(Variant);
			}
		}
		CHECK(Variants.count("SK_Mon_Pitwarden") == 1);
		CHECK(Variants.count("SK_Mon_RisenFootman") == 0);
		CHECK(Variants.size() == 4);   // + the standard bearer, the arbalest, the shieldbearer
		CHECK(IdleVariantFor(std::vector<std::string>{ "Idle", "Walk" }).empty());
		CHECK(IdleVariantFor(std::vector<std::string>{ "Idle_Order_Dig" }).empty());
		for (uint32_t Seed : { 0u, 1u, 77u, 0xFFFFFFFFu })
		{
			const double Wait = IdleVariantWaitMs(Seed, 0);
			CHECK(Wait >= IdleVariantMinWaitMs);
			CHECK(Wait < IdleVariantMinWaitMs + IdleVariantSpreadMs);
			CHECK(IdleVariantWaitMs(Seed, 0) == Wait);   // deterministic per entity and round
		}
		CHECK(IdleVariantWaitMs(1u, 0) != IdleVariantWaitMs(1u, 1));
		CHECK(!IdleVariantDue(3999.0, 4000.0, false, false, false));
		CHECK(IdleVariantDue(4000.0, 4000.0, false, false, false));
		CHECK(!IdleVariantDue(9000.0, 4000.0, /*bDormant*/ true, false, false));
		CHECK(!IdleVariantDue(9000.0, 4000.0, false, /*bAggro*/ true, false));
		CHECK(!IdleVariantDue(9000.0, 4000.0, false, false, /*bHeld*/ true));
	}

	TEST_CASE("the autotest: a scene label for the sub-dungeon and its cleanse, the probe's verdict")
	{
		using namespace AbyssSmoothnessRules;
		FSceneTracker Tracker;
		FSceneInput In;
		In.Phase = EPhase::Playing;
		In.RealSec = 100.0;
		const std::uint16_t Travel = Tracker.Update(In);
		CHECK(Tracker.Scenes()[Travel].Label == "travel");
		In.SubDungeon = PitId;
		const std::uint16_t Pit = Tracker.Update(In);
		CHECK(Tracker.Scenes()[Pit].Label == "dungeon_sub_rf_pit");
		CHECK(Tracker.Scenes()[Pit].Kind == ESceneKind::Travel);
		In.bCleansing = true;
		const std::uint16_t Cleanse = Tracker.Update(In);
		CHECK(Tracker.Scenes()[Cleanse].Label == "dungeon_sub_rf_pit_cleanse");
		// a fight, a boss and a story beat keep their own labels inside
		In.bInCombat = true;
		CHECK(Tracker.Scenes()[Tracker.Update(In)].Label == "fight_001");
		In.bBoss = true;
		In.BossId = "sub_pitwarden";
		CHECK(Tracker.Scenes()[Tracker.Update(In)].Label == "boss_sub_pitwarden_p0");
		In.bStory = true;
		In.bReachedCamp = true;
		CHECK(Tracker.Scenes()[Tracker.Update(In)].Label == "story");
		// back on the surface
		In = FSceneInput();
		In.Phase = EPhase::Playing;
		In.RealSec = 200.0;
		CHECK(Tracker.Update(In) == Travel);

		FProbe Probe;
		CHECK(ProbeVerdict(Probe).empty());   // the Pit not visited (the smoke scenario): nothing to say
		Probe.Visits = 1;
		Probe.WallBlocks = 237;
		CHECK(!ProbeVerdict(Probe).empty());  // built, theme not resolved
		Probe.bThemeResolved = true;
		CHECK(ProbeVerdict(Probe).empty());
		Probe.bGradeExpected = true;
		CHECK(ProbeVerdict(Probe).find("grade") != std::string::npos);   // the HARD acceptance item: a contract, no grade written
		Probe.bGraded = true;
		Probe.LampsExpected = 14;
		CHECK(ProbeVerdict(Probe).find("0 of 14 lamps have their lit twin") != std::string::npos);
		Probe.Lamps = 14;
		Probe.DormantSeen = 8;
		CHECK(ProbeVerdict(Probe).find("dormant") != std::string::npos);
		Probe.DormantWakes = 8;
		CHECK(ProbeVerdict(Probe).empty());   // not cleansed yet: unlit lamps are right
		// Wave 3 close-out: the wakes against the count seen. Five woke, two fell in their still pose at the cleanse, one
		// still stands: all eight are accounted for; one dropped without a wake or a fall is a fault, and so are more
		// wakes than monsters shown.
		CHECK(DormantUnaccounted(8, 8, 0, 0) == 0);
		CHECK(DormantUnaccounted(8, 5, 2, 1) == 0);
		CHECK(DormantUnaccounted(8, 5, 2, 0) == 1);
		CHECK(DormantUnaccounted(8, 0, 0, 8) == 0);
		CHECK(DormantUnaccounted(8, 3, 0, 0) == 5);
		CHECK(DormantUnaccounted(8, 9, 0, 0) == 0);     // never negative (the verdict's other line reports it)
		CHECK(DormantUnaccounted(8, -1, -1, -1) == 8);
		{
			FProbe Partial = Probe;
			Partial.DormantWakes = 5;
			Partial.DormantFell = 2;
			CHECK(ProbeVerdict(Partial).empty());   // the eighth still stands, or the bot left
			Partial.DormantUnaccounted = DormantUnaccounted(8, 5, 2, 0);
			CHECK(ProbeVerdict(Partial) == "1 of 8 dormant monsters left their still pose without a wake or a fall being shown");
			FProbe Over = Probe;
			Over.DormantWakes = 7;
			Over.DormantFell = 2;
			CHECK(ProbeVerdict(Over) == "7 wakes and 2 falls for 8 dormant monsters shown");
		}
		Probe.Cleanses = 1;
		Probe.SequencesStarted = 1;
		Probe.PeakLampsLit = 9;
		CHECK(ProbeVerdict(Probe) == "the cleanse sequence did not finish");   // still playing at the run's end
		Probe.SequencesFinished = 1;
		CHECK(ProbeVerdict(Probe) == "9 of 14 lamps lit after the cleanse");
		Probe.PeakLampsLit = 14;
		CHECK(ProbeVerdict(Probe).empty());
		// the zone rebuilt or left under the sequence (a death, a quality change, a save and load, the bot leaving): the
		// next build enters lit without a sequence - not a fault
		FProbe Cut = Probe;
		Cut.SequencesFinished = 0;
		Cut.SequencesCut = 1;
		Cut.PeakLampsLit = 6;
		CHECK(ProbeVerdict(Cut).empty());
		// a cleanse the builder had no built sub-dungeon for starts no sequence and owes none
		FProbe NoZone = Probe;
		NoZone.SequencesStarted = NoZone.SequencesFinished = 0;
		NoZone.PeakLampsLit = 0;
		CHECK(ProbeVerdict(NoZone).empty());
	}
}
