// U1 presentation theme and mood rules (Source/Abyssfire/Private/World/AbyssThemeRules.h) on the real exported tables
// (unreal/Data/*.json, the art manifest copy Data/assets.json) plus synthetic tables.
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
#include "abyss/world/MapGen.h"

#include "World/AbyssThemeRules.h"

namespace
{
	using namespace AbyssThemeRules;

	std::string ReadFileBytes(const std::string& Path)
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

	std::string DataDir() { return std::string(ABYSS_UNREAL_DIR) + "/Data"; }

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
					Out = ReadFileBytes(DataDir() + "/" + std::string(Name));
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

	/** The art manifest (Data/assets.json `manifest`), parsed once. */
	const abyss::JsonValue& Manifest()
	{
		static abyss::JsonValue Root;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			const std::string Bytes = ReadFileBytes(DataDir() + "/assets.json");
			abyss::ParseJson(Bytes, Root);
		}
		const abyss::JsonValue* Inner = Root.Find("manifest");
		return Inner != nullptr ? *Inner : Root;
	}

	/** gameId -> asset names, as FAbyssArtManifest indexes them (every asset's list, then the top-level map). */
	const std::map<std::string, std::vector<std::string>>& GameIdIndex()
	{
		static std::map<std::string, std::vector<std::string>> Index;
		if (Index.empty())
		{
			auto Add = [](const std::string& Id, const std::string& Asset)
			{
				std::vector<std::string>& List = Index[Id];
				for (const std::string& Existing : List)
				{
					if (Existing == Asset)
					{
						return;
					}
				}
				List.push_back(Asset);
			};
			for (const abyss::JsonMember& Asset : Manifest().Get("assets").Members())
			{
				for (const abyss::JsonValue& Id : Asset.value.Get("gameIds").Items())
				{
					Add(std::string(Id.AsString()), std::string(Asset.key));
				}
			}
			for (const abyss::JsonMember& Top : Manifest().Get("gameIds").Members())
			{
				Add(std::string(Top.key), std::string(Top.value.AsString()));
			}
			for (auto& Pair : Index)
			{
				std::sort(Pair.second.begin(), Pair.second.end());
			}
		}
		return Index;
	}

	/** The first asset (variant 0) a chain resolves to under a theme, as FAbyssArtManifest::ResolveFirst(Ids, 0, Theme). */
	std::string Resolve(const std::vector<std::string>& Ids, std::string_view Theme)
	{
		const auto& Index = GameIdIndex();
		auto Lookup = [&Index](const std::string& Id) -> const std::string*
		{
			const auto It = Index.find(Id);
			return It != Index.end() && !It->second.empty() ? &It->second.front() : nullptr;
		};
		const std::string* Found = ResolveThemedFirst<const std::string*>(Ids, !Theme.empty(),
			[&](const std::string& Id) { return Lookup(ThemedId(Id, Theme)); },
			[&](const std::string& Id) { return Lookup(Id); });
		return Found != nullptr ? *Found : std::string();
	}

	bool StartsWith(const std::string& Text, std::string_view Prefix)
	{
		return Text.size() >= Prefix.size() && std::string_view(Text).substr(0, Prefix.size()) == Prefix;
	}

	const abyss::MapDef* Map(std::string_view Id)
	{
		return Data().FindMap(Id);
	}

	FMoodState MoodAt(std::string_view ZoneId, double Col, double Row, bool bChapterComplete, bool bWasDominant = false)
	{
		const abyss::MapDef* M = Map(ZoneId);
		FMoodInputs In;
		In.Moods = &Data().World().moods;
		In.Map = M;
		In.ZoneId = ZoneId;
		In.GeneratorTheme = M != nullptr ? M->theme : abyss::MapTheme::Plains;
		In.bChapterComplete = bChapterComplete;
		In.bHasHero = true;
		In.HeroCol = Col;
		In.HeroRow = Row;
		In.bRegionWasDominant = bWasDominant;
		return EffectiveMood(In);
	}

	const abyss::ZoneMoodDef& MoodDef(abyss::MapTheme Theme)
	{
		return Data().World().moods.moods[static_cast<size_t>(Theme)];
	}

	void CheckSameMood(const abyss::ZoneMoodDef& A, const abyss::ZoneMoodDef& B)
	{
		CHECK(A.ambient == B.ambient);
		CHECK(A.ambientAlpha == B.ambientAlpha);
		CHECK(A.vignette == B.vignette);
		CHECK(A.vignetteAlpha == B.vignetteAlpha);
		CHECK(A.haze == B.haze);
		CHECK(A.hazeAlpha == B.hazeAlpha);
		CHECK(A.saturation == B.saturation);
		CHECK(A.contrast == B.contrast);
		CHECK(A.lift == B.lift);
		CHECK(A.gain == B.gain);
		CHECK(A.fogColor == B.fogColor);
		CHECK(A.fogMaxOpacity == B.fogMaxOpacity);
		CHECK(A.fogDensity == B.fogDensity);
	}
}

TEST_SUITE("theme")
{
	TEST_CASE("themed ids: split, themed-first order, no expansion without a theme")
	{
		std::string_view Base;
		std::string_view Theme;
		CHECK(SplitThemedId("tree@march", Base, Theme));
		CHECK(Base == "tree");
		CHECK(Theme == "march");
		CHECK(SplitThemedId("outcrop_plains@march", Base, Theme));
		CHECK(Base == "outcrop_plains");
		CHECK_FALSE(SplitThemedId("tree", Base, Theme));
		CHECK_FALSE(SplitThemedId("@march", Base, Theme));
		CHECK_FALSE(SplitThemedId("tree@", Base, Theme));
		CHECK_FALSE(SplitThemedId("a@b@c", Base, Theme));
		CHECK(ThemedId("camp_tent", "march") == "camp_tent@march");
		CHECK(ThemedId("camp_tent", "") == "camp_tent");

		// Every themed id of a chain comes before any plain id: {decor_tree, tree} must reach tree@march.
		const std::vector<std::string> Expanded = ExpandThemed({ "decor_tree", "tree" }, "march");
		REQUIRE(Expanded.size() == 4);
		CHECK(Expanded[0] == "decor_tree@march");
		CHECK(Expanded[1] == "tree@march");
		CHECK(Expanded[2] == "decor_tree");
		CHECK(Expanded[3] == "tree");
		CHECK(ExpandThemed({ "decor_tree", "tree" }, "") == std::vector<std::string>{ "decor_tree", "tree" });

		// ResolveThemedFirst visits in the same order and stops at the first hit.
		std::vector<std::string> Visited;
		const char* const Hit = "hit";
		const char* Result = ResolveThemedFirst<const char*>(std::vector<std::string>{ "a", "b" }, true,
			[&](const std::string& Id) -> const char* { Visited.push_back(Id + "@t"); return Id == "b" ? Hit : nullptr; },
			[&](const std::string& Id) -> const char* { Visited.push_back(Id); return nullptr; });
		CHECK(Result == Hit);
		CHECK(Visited == std::vector<std::string>{ "a@t", "b@t" });
		Visited.clear();
		Result = ResolveThemedFirst<const char*>(std::vector<std::string>{ "a", "b" }, false,
			[&](const std::string& Id) -> const char* { Visited.push_back(Id + "@t"); return Hit; },
			[&](const std::string& Id) -> const char* { Visited.push_back(Id); return Id == "b" ? Hit : nullptr; });
		CHECK(Result == Hit);
		CHECK(Visited == std::vector<std::string>{ "a", "b" });
	}

	TEST_CASE("themed ids resolve the shipped march art, plain art without the theme or the variant")
	{
		REQUIRE(Manifest().IsObject());
		// Decor (the generator's ids and the zone actor's decor_ chains), outcrops / palisades on the generator theme's
		// id strings, camp props, NPC looks, lore / chest / event props, the sealed exit, the elite crown.
		CHECK(StartsWith(Resolve({ "tree", "decor_tree" }, "march"), "SM_Foliage_March_DeadOak"));
		CHECK(StartsWith(Resolve({ "decor_tree", "tree" }, "march"), "SM_Foliage_March_DeadOak"));
		CHECK(StartsWith(Resolve({ "tree", "decor_tree" }, ""), "SM_Foliage_Plains"));
		CHECK(StartsWith(Resolve({ "outcrop_plains", "wall_plains", "outcrop", "wall" }, "march"), "SM_Outcrop_March_V"));
		CHECK(StartsWith(Resolve({ "palisade_plains", "palisade", "camp_wall" }, "march"), "SM_Palisade_March_Pew"));
		CHECK(Resolve({ "palisade_gate_plains", "palisade_gate" }, "march") == "SM_Palisade_March_GatePost");
		CHECK(StartsWith(Resolve({ "lily_pad_plains", "decor_lily_pad", "lily_pad" }, "march"), "SM_Foliage_March_Reeds"));
		CHECK(Resolve({ "camp_tent", "decor_camp_tent", "tent" }, "march") == "SM_Prop_March_BellFrame");
		CHECK(Resolve({ "camp_campfire", "decor_camp_campfire", "campfire" }, "march") == "SM_Prop_March_Hearth");
		CHECK(Resolve({ "camp_torch", "decor_camp_torch", "torch" }, "march") == "SM_Prop_March_Brazier");
		CHECK(Resolve({ "npc_quest_elder", "quest_elder" }, "march") == "SK_NPC_QuestElder_March");
		CHECK(Resolve({ "npc_quest_elder", "quest_elder" }, "") == "SK_NPC_QuestElder");
		CHECK(Resolve({ "lore_ancient_tablet", "ancient_tablet", "decor_ancient_tablet" }, "march") == "SM_Prop_March_OrderPost");
		CHECK(Resolve({ "decor_treasure_chest", "treasure_chest" }, "march") == "SK_Prop_March_Strongbox");
		CHECK(Resolve({ "exit_gate_sealed", "exit_sealed", "exit_portal" }, "march") == "SM_Prop_March_WagonBarricade");
		CHECK(Resolve({ "gather_herb", "quest_item_herb", "herb", "gather_node" }, "march") == "SM_Prop_Gather_Barrowmoss");
		CHECK(Resolve({ "elite_crown" }, "march") == "SM_FX_EliteCrown_March");
		// 2G stage C: in a themed zone the Wave 2 sigil goes before the crown chain (AbyssMonsterLookRules::
		// EliteMarkerFor); it has no theme variant, and an unthemed zone never asks for it.
		CHECK(Resolve({ "elite_sigil" }, "march") == "SM_FX_EliteSigil");
		// one themed chain would try elite_crown@march before the plain sigil: the glue resolves the sigil on its own
		CHECK(Resolve({ "elite_sigil", "elite_crown" }, "march") == "SM_FX_EliteCrown_March");
		CHECK(Resolve({ "elite_sigil" }, "") == "SM_FX_EliteSigil");
		CHECK(Resolve({ "elite_crown" }, "") == "SM_FX_EliteCrown");
		// No march variant: today's plains asset.
		const std::string Portal = Resolve({ "exit_portal" }, "");
		CHECK_FALSE(Portal.empty());
		CHECK(Resolve({ "exit_portal" }, "march") == Portal);
		CHECK(Resolve({ "loot_bag" }, "march") == Resolve({ "loot_bag" }, ""));
		// A theme no asset is authored for falls back to the plain chain.
		CHECK(Resolve({ "tree" }, "forest") == Resolve({ "tree" }, ""));
	}

	TEST_CASE("every @march id names a plain base id with the same footprint and blocking")
	{
		// The core bakes walkability from the plain ids (MapGen W5); a theme variant must not move a blocker
		// (FAbyssArtManifest warns at load, Art/blender verify_march.py asserts it at ship).
		const auto& Index = GameIdIndex();
		const abyss::JsonValue& Assets = Manifest().Get("assets");
		int Themed = 0;
		for (const auto& [Id, List] : Index)
		{
			std::string_view Base;
			std::string_view Theme;
			if (!SplitThemedId(Id, Base, Theme))
			{
				continue;
			}
			++Themed;
			CAPTURE(Id);
			CHECK(Theme == "march");
			const auto Plain = Index.find(std::string(Base));
			REQUIRE(Plain != Index.end());
			auto Key = [&Assets](const std::string& Asset)
			{
				const abyss::JsonValue& A = Assets.Get(Asset);
				return abyss::WriteJson(A.Get("footprintTiles")) + "|" + abyss::WriteJson(A.Get("blocking"));
			};
			std::set<std::string> PlainKeys;
			std::set<std::string> ThemedKeys;
			for (const std::string& Asset : Plain->second)
			{
				PlainKeys.insert(Key(Asset));
			}
			for (const std::string& Asset : List)
			{
				ThemedKeys.insert(Key(Asset));
			}
			CHECK(PlainKeys == ThemedKeys);
		}
		CHECK(Themed >= 44);
	}

	TEST_CASE("presentation theme and terrain style per map: only emerald_plains is themed")
	{
		REQUIRE(Map("emerald_plains") != nullptr);
		CHECK(ThemeOf(Map("emerald_plains")) == "march");
		CHECK(TerrainThemeName(Map("emerald_plains"), "plains") == "march");
		for (const char* Id : { "twilight_forest", "anvil_mountains", "scorching_desert", "abyss_rift", "ember_tower" })
		{
			const std::string IdName(Id);
			CAPTURE(IdName);
			REQUIRE(Map(Id) != nullptr);
			CHECK(ThemeOf(Map(Id)).empty());
			CHECK(TerrainThemeName(Map(Id), "forest") == "forest");
		}
		CHECK(ThemeOf(nullptr).empty());   // labyrinth floors have no map data
		CHECK(TerrainThemeName(nullptr, "plains") == "plains");
		// The terrain style the themed zone switches to exists.
		const abyss::JsonValue* Styles = Data().RawTable("terrain_styles.json");
		REQUIRE(Styles != nullptr);
		CHECK(Styles->Get("themes").Get("march").Get("ground").IsObject());
	}

	TEST_CASE("camp kit: the hub camp flies the themed banner, another camp of a themed zone does not")
	{
		CHECK(PlacesCampProp("", 1, "banner"));
		CHECK(PlacesCampProp("march", 0, "banner"));
		CHECK_FALSE(PlacesCampProp("march", 1, "banner"));
		for (const char* Type : { "campfire", "well", "torch", "crate", "barrel" })
		{
			CHECK(PlacesCampProp("march", 1, Type));
		}
		// Shipped maps: emerald_plains' Knellwatch (camp 0) keeps its 4 banners, Crowmarket (camp 1) none; every other
		// prop of both camps and every prop of an unthemed zone's camps stays.
		for (const abyss::MapDef& M : Data().World().maps)
		{
			CAPTURE(M.id);
			const std::string_view Theme = ThemeOf(&M);
			std::map<int, int> Banners;
			size_t Skipped = 0;
			for (const abyss::CampProp& Prop : abyss::CampProps(M))
			{
				const bool bPlaced = PlacesCampProp(Theme, Prop.campIndex, Prop.type);
				Skipped += bPlaced ? 0 : 1;
				CHECK((bPlaced || Prop.type == "banner"));
				CHECK((bPlaced || !Prop.blocking));
				Banners[Prop.campIndex] += bPlaced && Prop.type == "banner" ? 1 : 0;
			}
			if (Theme.empty())
			{
				CHECK(Skipped == 0);
			}
			else if (M.camps.size() > 1)
			{
				CHECK(Banners[0] == 4);
				for (size_t Camp = 1; Camp < M.camps.size(); ++Camp)
				{
					CHECK(Banners[static_cast<int>(Camp)] == 0);
				}
			}
		}
		REQUIRE(Map("emerald_plains") != nullptr);
		CHECK(Map("emerald_plains")->camps.size() == 2);
	}

	TEST_CASE("themed-only loops: the sealed exit never falls back to the crimson web ring in a themed zone")
	{
		CHECK(ThemedOnlyRecipe("exit.sealed"));
		CHECK_FALSE(ThemedOnlyRecipe("exit.portal"));
		CHECK_FALSE(ThemedOnlyRecipe("camp.torch"));
		CHECK_FALSE(ThemedOnlyRecipe("camp.campfire"));
		CHECK_FALSE(ThemedOnlyRecipe("exit.sealed@march"));
		CHECK_FALSE(ThemedOnlyRecipe(""));
	}

	TEST_CASE("dawn mood: only for the presentation theme, only after the chapter")
	{
		const abyss::ZoneMoodTables& Moods = Data().World().moods;
		CHECK(DawnMood(Moods, Map("emerald_plains"), abyss::MapTheme::March, false) == abyss::MapTheme::March);
		CHECK(DawnMood(Moods, Map("emerald_plains"), abyss::MapTheme::March, true) == abyss::MapTheme::MarchDawn);
		CHECK(DawnMood(Moods, Map("twilight_forest"), abyss::MapTheme::Forest, true) == abyss::MapTheme::Forest);
		CHECK(DawnMood(Moods, Map("ember_tower"), abyss::MapTheme::Plains, true) == abyss::MapTheme::Plains);
		CHECK(DawnMood(Moods, nullptr, abyss::MapTheme::Abyss, true) == abyss::MapTheme::Abyss);
		// A presentation theme without a defined <theme>_dawn mood keeps its base.
		abyss::ZoneMoodTables NoDawn = Moods;
		NoDawn.moods[static_cast<size_t>(abyss::MapTheme::MarchDawn)].defined = false;
		CHECK(DawnMood(NoDawn, Map("emerald_plains"), abyss::MapTheme::March, true) == abyss::MapTheme::March);
		CHECK(BaseMood(Moods, "emerald_plains", abyss::MapTheme::Plains) == abyss::MapTheme::March);
		CHECK(BaseMood(Moods, "sub_unknown_floor", abyss::MapTheme::Desert) == abyss::MapTheme::Desert);
	}

	TEST_CASE("region weight: full inside, smoothstep over the last 4 tiles, 0 at the radius")
	{
		const abyss::MapDef* Plains = Map("emerald_plains");
		REQUIRE(Plains != nullptr);
		REQUIRE(Plains->regions.size() == 1);
		const abyss::MapRegionDef& Rim = Plains->regions[0];
		CHECK(Rim.id == "breach_rim");
		CHECK(Rim.radius == doctest::Approx(16.0));
		const double Cx = Rim.center.col;
		const double Cy = Rim.center.row;
		CHECK(RegionWeight(Cx, Cy, Rim) == 1.0);
		CHECK(RegionWeight(Cx + 12.0, Cy, Rim) == 1.0);           // radius - ramp
		CHECK(RegionWeight(Cx + 14.0, Cy, Rim) == doctest::Approx(0.5));
		CHECK(RegionWeight(Cx, Cy + 13.0, Rim) == doctest::Approx(0.84375));   // smoothstep(0.75)
		CHECK(RegionWeight(Cx + 16.0, Cy, Rim) == 0.0);
		CHECK(RegionWeight(Cx + 20.0, Cy, Rim) == 0.0);
		// Monotonic through the ramp.
		double Previous = 1.0;
		for (double D = 12.0; D <= 16.0; D += 0.25)
		{
			const double W = RegionWeight(Cx + D, Cy, Rim);
			CHECK(W <= Previous);
			CHECK(W >= 0.0);
			Previous = W;
		}
		// A region smaller than the ramp ramps over its whole radius; no radius = no region.
		abyss::MapRegionDef Small;
		Small.radius = 2.0;
		CHECK(RegionWeight(0.0, 0.0, Small) == 1.0);
		CHECK(RegionWeight(1.0, 0.0, Small) == doctest::Approx(0.5));
		Small.radius = 0.0;
		CHECK(RegionWeight(0.0, 0.0, Small) == 0.0);
		CHECK(DominantRegion(Plains, 15.0, 15.0).Region == nullptr);
		CHECK(DominantRegion(Plains, Cx, Cy).Region == &Rim);
		CHECK(DominantRegion(nullptr, Cx, Cy).Region == nullptr);
	}

	TEST_CASE("light scale: the march overrides against R18's formula")
	{
		const abyss::ZoneMoodDef& March = MoodDef(abyss::MapTheme::March);
		CHECK(March.lightScale == doctest::Approx(0.32));
		CHECK(FormulaLightScale(March.ambient, March.ambientAlpha) == doctest::Approx(0.134).epsilon(0.01));
		CHECK(MoodLightScale(March) == doctest::Approx(0.32));
		CHECK(MoodLightScale(MoodDef(abyss::MapTheme::MarchDawn)) == doctest::Approx(0.22));
		CHECK(MoodLightScale(MoodDef(abyss::MapTheme::BreachRim)) == doctest::Approx(0.40));
		// Web moods keep the formula (DECISIONS R18 table: plains 0.12, forest 0.1605, abyss 0.2522).
		CHECK(MoodDef(abyss::MapTheme::Forest).lightScale == 0.0);
		CHECK(MoodLightScale(MoodDef(abyss::MapTheme::Plains)) == doctest::Approx(0.12));
		CHECK(MoodLightScale(MoodDef(abyss::MapTheme::Forest)) == doctest::Approx(0.1605).epsilon(0.002));
		CHECK(MoodLightScale(MoodDef(abyss::MapTheme::Abyss)) == doctest::Approx(0.2522).epsilon(0.002));
	}

	TEST_CASE("mood lerp: exact ends, per-channel colours")
	{
		const abyss::ZoneMoodDef& A = MoodDef(abyss::MapTheme::March);
		const abyss::ZoneMoodDef& B = MoodDef(abyss::MapTheme::BreachRim);
		CheckSameMood(LerpMood(A, B, 0.0), A);
		CheckSameMood(LerpMood(A, B, 1.0), B);
		CheckSameMood(LerpMood(A, B, -1.0), A);
		const abyss::ZoneMoodDef Mid = LerpMood(A, B, 0.5);
		CHECK(Mid.ambientAlpha == doctest::Approx((A.ambientAlpha + B.ambientAlpha) * 0.5));
		CHECK(Mid.fogDensity == doctest::Approx((A.fogDensity + B.fogDensity) * 0.5));
		CHECK(LerpRgb(0x000000u, 0xFF8040u, 0.5) == 0x804020u);
		CHECK(LerpRgb(0x102030u, 0x102030u, 0.37) == 0x102030u);
		CHECK(LerpRgb(0x00FF00u, 0xFF0000u, 0.0) == 0x00FF00u);
	}

	TEST_CASE("effective mood: the march, the Breach Rim, the dawn, and the web zones untouched")
	{
		// Knellwatch (camp 1): the march, ashfall tinted #CFCAC0, light scale 0.32.
		const FMoodState Camp = MoodAt("emerald_plains", 15.0, 15.0, false);
		CHECK(Camp.Base == abyss::MapTheme::March);
		CHECK_FALSE(Camp.bHasRegion);
		CHECK(Camp.Dominant == abyss::MapTheme::March);
		CheckSameMood(Camp.Mood, MoodDef(abyss::MapTheme::March));
		CHECK(Camp.LightScale == doctest::Approx(0.32));
		CHECK(Camp.bLightScaleOverride);
		CHECK(Camp.Ambience == "ashfall");
		CHECK(AmbienceRecipeId(Camp) == "ambient.ashfall");
		CHECK(Camp.bHasTint);
		CHECK(Camp.Tint == 0xCFCAC0u);

		// The Breach Rim's centre: its mood, sparks tinted breach-violet, light scale 0.40.
		const abyss::MapRegionDef& Rim = Map("emerald_plains")->regions[0];
		const FMoodState Dais = MoodAt("emerald_plains", Rim.center.col, Rim.center.row, false);
		CHECK(Dais.bHasRegion);
		CHECK(Dais.Region == abyss::MapTheme::BreachRim);
		CHECK(Dais.RegionWeight == 1.0);
		CHECK(Dais.bRegionDominant);
		CheckSameMood(Dais.Mood, MoodDef(abyss::MapTheme::BreachRim));
		CHECK(Dais.LightScale == doctest::Approx(0.40));
		CHECK(Dais.Ambience == "sparks");
		CHECK(Dais.Tint == 0xB02FD0u);

		// The ramp: half way the mood and light scale are half way; the ambience switches with hysteresis.
		const FMoodState Half = MoodAt("emerald_plains", Rim.center.col + 14.0, Rim.center.row, false);
		CHECK(Half.RegionWeight == doctest::Approx(0.5));
		CHECK(Half.LightScale == doctest::Approx(0.36));
		CHECK(Half.bRegionDominant);
		const double LeaveCol = Rim.center.col + 14.16;   // weight 0.44: inside the hysteresis band
		const FMoodState Entering = MoodAt("emerald_plains", LeaveCol, Rim.center.row, false, /*bWasDominant*/ false);
		const FMoodState Leaving = MoodAt("emerald_plains", LeaveCol, Rim.center.row, false, /*bWasDominant*/ true);
		CHECK(Entering.RegionWeight > RegionLeaveWeight);
		CHECK(Entering.RegionWeight < RegionEnterWeight);
		CHECK_FALSE(Entering.bRegionDominant);
		CHECK(Entering.Ambience == "ashfall");
		CHECK(Leaving.bRegionDominant);
		CHECK(Leaving.Ambience == "sparks");

		// After the chapter: the dawn mood, dust motes without a tint, light scale 0.22; the Breach Rim still blends.
		const FMoodState Dawn = MoodAt("emerald_plains", 15.0, 15.0, true);
		CHECK(Dawn.Base == abyss::MapTheme::MarchDawn);
		CheckSameMood(Dawn.Mood, MoodDef(abyss::MapTheme::MarchDawn));
		CHECK(Dawn.Ambience == "dust_motes");
		CHECK_FALSE(Dawn.bHasTint);
		CHECK(Dawn.LightScale == doctest::Approx(0.22));
		CHECK(MoodAt("emerald_plains", Rim.center.col, Rim.center.row, true).Ambience == "sparks");

		// Web zones: today's mood, weather and formula light scale; no tint, no override, chapter state ignored.
		struct FWeb
		{
			const char* Zone;
			abyss::MapTheme Mood;
			const char* Ambience;
		};
		for (const FWeb& Web : { FWeb{ "twilight_forest", abyss::MapTheme::Forest, "wisps" },
				 FWeb{ "anvil_mountains", abyss::MapTheme::Mountain, "dust_motes" },
				 FWeb{ "scorching_desert", abyss::MapTheme::Desert, "dust_motes" },
				 FWeb{ "abyss_rift", abyss::MapTheme::Abyss, "sparks" },
				 FWeb{ "ember_tower", abyss::MapTheme::Plains, "pollen" } })
		{
			const std::string ZoneName(Web.Zone);
			CAPTURE(ZoneName);
			for (const bool bComplete : { false, true })
			{
				const FMoodState S = MoodAt(Web.Zone, 20.0, 20.0, bComplete);
				CHECK(S.Base == Web.Mood);
				CHECK_FALSE(S.bHasRegion);
				CheckSameMood(S.Mood, MoodDef(Web.Mood));
				CHECK_FALSE(S.bLightScaleOverride);
				CHECK(S.LightScale == doctest::Approx(FormulaLightScale(MoodDef(Web.Mood).ambient, MoodDef(Web.Mood).ambientAlpha)));
				CHECK(S.Ambience == Web.Ambience);
				CHECK_FALSE(S.bHasTint);   // their weather tints exist (ember_tower #FFE8A0) but stay unused
			}
		}

		// A place without map data (a labyrinth floor) uses its generator theme's mood and the weather of a zone with it.
		FMoodInputs Floor;
		Floor.Moods = &Data().World().moods;
		Floor.ZoneId = "abyss_floor_3";
		Floor.GeneratorTheme = abyss::MapTheme::Abyss;
		Floor.bHasHero = true;
		const FMoodState FloorMood = EffectiveMood(Floor);
		CHECK(FloorMood.Base == abyss::MapTheme::Abyss);
		CHECK(FloorMood.Ambience == "sparks");
		CHECK_FALSE(FloorMood.bHasTint);
	}

	TEST_CASE("look / ambience change detection")
	{
		FMoodState A;
		A.Base = abyss::MapTheme::March;
		FMoodState B = A;
		CHECK_FALSE(LookChanged(A, B));
		B.bHasRegion = true;
		B.Region = abyss::MapTheme::BreachRim;
		B.RegionWeight = 0.005;
		CHECK(LookChanged(A, B));   // a region appears
		A = B;
		B.RegionWeight = 0.012;
		CHECK_FALSE(LookChanged(A, B));
		B.RegionWeight = 0.02;
		CHECK(LookChanged(A, B));
		A = B;
		B.RegionWeight = 1.0;
		CHECK(LookChanged(A, B));
		A = B;
		CHECK_FALSE(LookChanged(A, B));
		B.Base = abyss::MapTheme::MarchDawn;
		CHECK(LookChanged(A, B));
		CHECK_FALSE(AmbienceChanged(A, A));
		B = A;
		B.Tint = 1;
		CHECK(AmbienceChanged(A, B));
	}

	TEST_CASE("wind-up tints: data per mood, fallback mood, today's constants without data")
	{
		abyss::JsonValue Raw;
		REQUIRE(abyss::ParseJson(R"({"port":{"windupTint":{
			"march":{"normal":13705006,"elite":"#FF3A48"},
			"breach_rim":{"normal":"b02fd0"},
			"bad":{"normal":"#12"}}}})", Raw));
		FWindupTints March = WindupTintFor(&Raw, "march", "");
		CHECK(March.bFromData);
		CHECK(March.Normal == 0xD11F2Eu);
		CHECK(March.Elite == 0xFF3A48u);
		const FWindupTints Rim = WindupTintFor(&Raw, "breach_rim", "march");
		CHECK(Rim.Normal == 0xB02FD0u);
		CHECK(Rim.Elite == DefaultWindupElite);   // no elite entry
		const FWindupTints Dawn = WindupTintFor(&Raw, "march_dawn", "march");
		CHECK(Dawn.Normal == 0xD11F2Eu);   // the fallback mood
		const FWindupTints Bad = WindupTintFor(&Raw, "bad", "");
		CHECK_FALSE(Bad.bFromData);
		const FWindupTints Forest = WindupTintFor(&Raw, "forest", "");
		CHECK_FALSE(Forest.bFromData);
		CHECK(Forest.Normal == 0xFFC4B0u);
		CHECK(Forest.Elite == 0xFF7A5Cu);
		CHECK(WindupTintFor(nullptr, "march", "").Normal == DefaultWindupNormal);

		// The exported data: the remake's moods are crimson (DR5), every web mood keeps today's tints.
		const abyss::JsonValue* Moods = Data().RawTable("zone_moods.json");
		REQUIRE(Moods != nullptr);
		for (const char* Remake : { "march", "march_dawn", "breach_rim" })
		{
			const std::string RemakeName(Remake);
			CAPTURE(RemakeName);
			const FWindupTints T = WindupTintFor(Moods, Remake, "");
			CHECK(T.bFromData);
			CHECK(T.Normal == 0xD11F2Eu);
		}
		for (const char* Web : { "plains", "forest", "mountain", "desert", "abyss" })
		{
			const std::string WebName(Web);
			CAPTURE(WebName);
			const FWindupTints T = WindupTintFor(Moods, Web, "");
			CHECK_FALSE(T.bFromData);
			CHECK(T.Normal == 0xFFC4B0u);
			CHECK(T.Elite == 0xFF7A5Cu);
		}
	}

	TEST_CASE("echo look: the manifest's figures and timeline, the fade curve")
	{
		const FEchoLook Look = ParseEchoLook(Manifest().Get("materials").Get("MI_AF_Echo"));
		REQUIRE(Look.bValid);
		CHECK(Look.Timeline.FadeInMs == 450.0);
		CHECK(Look.Timeline.HoldMs == 2100.0);
		CHECK(Look.Timeline.FadeOutMs == 450.0);
		CHECK(Look.Timeline.TotalMs() == 3000.0);
		CHECK(Look.TintRgb == 0xA8B0C4u);
		CHECK(Look.TintLinear[0] == doctest::Approx(0.1958));
		CHECK(Look.Ghost == doctest::Approx(1.0));

		// Every echo lore spot of the data has figures, at its own tile.
		int Echoes = 0;
		for (const abyss::LoreEntryDef& Lore : Data().Lore().entries)
		{
			if (!IsEchoSpriteType(Lore.spriteType))
			{
				continue;
			}
			++Echoes;
			CAPTURE(Lore.id);
			const FEchoEntry* Entry = Look.Find(Lore.id);
			REQUIRE(Entry != nullptr);
			CHECK_FALSE(Entry->Figures.empty());
			CHECK(Entry->bHasTile);
			CHECK(Entry->TileCol == Lore.pos.col);
			CHECK(Entry->TileRow == Lore.pos.row);
			for (const FEchoFigure& Figure : Entry->Figures)
			{
				CHECK(GameIdIndex().size() > 0);
				CHECK(Manifest().Get("assets").Find(Figure.Asset) != nullptr);
				const std::string& ClipOwner = Figure.ClipAsset.empty() ? Figure.Asset : Figure.ClipAsset;
				bool bClip = false;
				for (const abyss::JsonValue& Clip : Manifest().Get("assets").Get(ClipOwner).Get("anims").Items())
				{
					bClip = bClip || Clip.Get("name").AsString() == Figure.Clip;
				}
				CHECK(bClip);
			}
		}
		CHECK(Echoes == 3);
		const FEchoEntry* Two = Look.Find("lore_rf_echo_02");
		REQUIRE(Two != nullptr);
		REQUIRE(Two->Figures.size() == 2);
		CHECK(Two->Figures[0].OffsetCol == doctest::Approx(-2.6));
		CHECK(Two->Figures[0].OffsetRow == doctest::Approx(0.55));   // Blender -Y = UE +Y
		CHECK(Two->Figures[0].MeshYawDeg == doctest::Approx(-95.0));
		CHECK(Two->Figures[0].FacingYawDeg == doctest::Approx(-5.0));
		const FEchoEntry* Three = Look.Find("lore_rf_echo_03");
		REQUIRE(Three != nullptr);
		CHECK(Three->Figures[0].ClipAsset == "SK_NPC_EscortTravelingMerchant_March");
		CHECK(Three->Figures[0].Frame == 22);
		CHECK(ClipFrameSec(22, 60.0) == doctest::Approx(22.0 / 60.0));
		CHECK(ClipFrameSec(22, 0.0) == doctest::Approx(22.0 / 60.0));

		// The timeline: invisible at 0, opaque at 450, the hold's flicker in [0, 0.08], the fade-out from the hold's end
		// value, invisible again at 3000 and after; continuous at both joins, monotonic in and out.
		const FEchoTimeline& T = Look.Timeline;
		CHECK(EchoFade(T, 0.0) == 1.0);
		CHECK(EchoFade(T, -5.0) == 1.0);
		CHECK(EchoFade(T, 450.0) == doctest::Approx(0.0));
		CHECK(EchoFade(T, 449.9) == doctest::Approx(0.0).epsilon(1e-3));
		const double HoldEnd = EchoFade(T, 2549.999);
		CHECK(EchoFade(T, 2550.0) == doctest::Approx(HoldEnd).epsilon(1e-3));
		CHECK(EchoFade(T, 3000.0) == 1.0);
		CHECK(EchoFade(T, 5000.0) == 1.0);
		double Previous = 2.0;
		for (double Ms = 0.0; Ms <= 450.0; Ms += 10.0)
		{
			const double F = EchoFade(T, Ms);
			CHECK(F <= Previous);
			Previous = F;
		}
		double MaxFlicker = 0.0;
		for (double Ms = 450.0; Ms < 2550.0; Ms += 5.0)
		{
			const double F = EchoFade(T, Ms);
			CHECK(F >= 0.0);
			CHECK(F <= 0.08 + 1e-9);
			MaxFlicker = F > MaxFlicker ? F : MaxFlicker;
		}
		CHECK(MaxFlicker > 0.07);   // it does flicker
		Previous = -1.0;
		for (double Ms = 2550.0; Ms <= 3000.0; Ms += 10.0)
		{
			const double F = EchoFade(T, Ms);
			CHECK(F >= Previous);
			Previous = F;
		}
		CHECK(IsEchoSpriteType("echo"));
		CHECK_FALSE(IsEchoSpriteType("ancient_tablet"));
		CHECK_FALSE(ParseEchoLook(abyss::JsonValue()).bValid);
	}

	TEST_CASE("brightness: clamp and exposure bias")
	{
		CHECK(ClampBrightness(1.f) == 1.f);
		CHECK(ClampBrightness(0.1f) == BrightnessMin);
		CHECK(ClampBrightness(9.f) == BrightnessMax);
		CHECK(ClampBrightness(std::nanf("")) == BrightnessDefault);
		CHECK(BrightnessToEv(1.f) == 0.f);
		CHECK(BrightnessToEv(0.5f) == doctest::Approx(-1.0));
		CHECK(BrightnessToEv(1.5f) == doctest::Approx(0.585).epsilon(0.001));
		CHECK(BrightnessToEv(0.f) == doctest::Approx(-1.0));
	}
}
