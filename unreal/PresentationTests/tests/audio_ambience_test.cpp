// U7 ambience (Source/Abyssfire/Private/Audio/AbyssAmbienceRules.h) on the committed audio manifest and the real map
// data: the bed by presentation theme, the Breach Rim region bed, the Gnawed Field layer, the emitters' spots and their
// distance attenuation.
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "doctest/doctest.h"

#include "abyss/base/Enums.h"
#include "abyss/data/DataStore.h"
#include "abyss/data/QuestData.h"

#include "Audio/AbyssAmbienceRules.h"
#include "audio_test_data.h"

namespace
{
	using namespace AbyssAmbienceRules;

	const AmbienceTables& Tables()
	{
		static const AmbienceTables T = Parse(audio_test::Manifest().Get("ambience"));
		return T;
	}

	const Emitter& EmitterById(std::string_view Id)
	{
		for (const Emitter& E : Tables().emitters)
		{
			if (E.id == Id)
			{
				return E;
			}
		}
		static const Emitter None;
		FAIL("no emitter " << std::string(Id));
		return None;
	}

	std::string Bed(std::string_view ZoneId, std::string_view ThemeName)
	{
		const abyss::JsonValue& Amb = audio_test::Manifest().Get("ambience");
		if (const abyss::JsonValue* Zone = Amb.Get("zones").Find(ZoneId))
		{
			return std::string(Zone->AsString());
		}
		if (const abyss::JsonValue* Theme = Amb.Get("themes").Find(ThemeName))
		{
			return std::string(Theme->AsString());
		}
		return {};
	}
}

TEST_SUITE("audio")
{
	TEST_CASE("ambience tables parse from the manifest")
	{
		const AmbienceTables& T = Tables();
		REQUIRE(T.loaded);
		REQUIRE(T.regions.size() == 1);
		const AreaLoop& Rim = T.regions[0];
		CHECK(Rim.id == "breach_rim");
		CHECK(Rim.asset == "SW_AMB_BreachRim");
		CHECK(Rim.zone == "emerald_plains");
		CHECK(Rim.replacesBed);
		CHECK(Rim.area.col == 15.0);
		CHECK(Rim.area.row == 95.0);
		CHECK(Rim.area.radius == 16.0);
		CHECK(Rim.area.exitRadius == 17.0);
		CHECK(Rim.when.monsterAlive.empty());  // always: the breach stays open
		REQUIRE(T.layers.size() == 1);
		const AreaLoop& Ring = T.layers[0];
		CHECK(Ring.id == "gnawed_field_ringing");
		CHECK(Ring.asset == "SW_AMB_GnawedFieldRinging");
		CHECK_FALSE(Ring.replacesBed);
		CHECK(Ring.when.monsterAlive == "hunt_hungerbell");
		CHECK(Ring.when.untilFlag == "rf_hungerbell_silenced");   // Wave 2 (2G stage H): replaces the `while`
		CHECK(Rim.when.untilFlag.empty());
		CHECK(Ring.fadeInSec == 4.0);
		CHECK(Ring.fadeOutSec == 6.0);
		const Emitter& Knock = EmitterById("ashwheat_knock");
		CHECK(Knock.assets.size() == 3);
		CHECK(Knock.innerTiles == 2.0);
		CHECK(Knock.falloffTiles == 8.0);
		CHECK(Knock.hasArea);
		CHECK(Knock.areaCol == 40.0);
		CHECK(Knock.areaRow == 52.0);
		const Emitter& Hearth = EmitterById("knellwatch_hearth");
		CHECK(Hearth.campSlot == "campfire");
		CHECK(Hearth.camp == 1);
		CHECK(Hearth.hasAnchor);
		// Every loop is a looping ambience asset.
		const abyss::JsonValue& Assets = audio_test::Manifest().Get("assets");
		std::vector<std::string> Names{ Rim.asset, Ring.asset };
		for (const Emitter& E : T.emitters)
		{
			Names.insert(Names.end(), E.assets.begin(), E.assets.end());
		}
		for (const std::string& Name : Names)
		{
			CHECK_MESSAGE(Assets.Get(Name).Get("loop").AsBool(false), Name);
			CHECK_MESSAGE(Assets.Get(Name).Get("kind").AsString() == "ambience", Name);
		}
	}

	TEST_CASE("the bed follows the presentation theme")
	{
		const abyss::DataStore& Data = audio_test::Data();
		const abyss::MapDef* Plains = Data.FindMap("emerald_plains");
		const abyss::MapDef* Tower = Data.FindMap("ember_tower");
		REQUIRE(Plains != nullptr);
		REQUIRE(Tower != nullptr);
		CHECK(BedThemeName(Plains, "plains") == "march");
		CHECK(BedThemeName(Tower, "plains") == "plains");
		CHECK(BedThemeName(nullptr, "abyss") == "abyss");  // labyrinth floors: no map def
		CHECK(Bed("emerald_plains", BedThemeName(Plains, "plains")) == "SW_AMB_March");
		CHECK(Bed("ember_tower", BedThemeName(Tower, "plains")) == "SW_AMB_Plains");
		// A zone without a zone row takes its presentation theme's bed (march), else its theme's.
		CHECK(Bed("a_march_zone", "march") == "SW_AMB_March");
		CHECK(Bed("twilight_forest", "forest").empty());  // no bed today: unchanged
	}

	TEST_CASE("emitter attenuation: full inside, linear to silence")
	{
		CHECK(EmitterGain(0.0, 2.0, 8.0) == 1.0);
		CHECK(EmitterGain(2.0, 2.0, 8.0) == 1.0);
		CHECK(EmitterGain(6.0, 2.0, 8.0) == doctest::Approx(0.5));
		CHECK(EmitterGain(9.0, 2.0, 8.0) == doctest::Approx(0.125));
		CHECK(EmitterGain(10.0, 2.0, 8.0) == 0.0);
		CHECK(EmitterGain(30.0, 2.0, 8.0) == 0.0);
		CHECK(EmitterGain(1.0, 1.5, 0.0) == 1.0);
		CHECK(EmitterGain(1.6, 1.5, 0.0) == 0.0);
		CHECK(Distance(15.0, 15.0, 18.0, 19.0) == doctest::Approx(5.0));
	}

	TEST_CASE("emitter spots: set pieces first, then the placement")
	{
		const abyss::MapDef* Plains = audio_test::Data().FindMap("emerald_plains");
		REQUIRE(Plains != nullptr);
		std::vector<Spot> Spots;
		const Emitter& Knock = EmitterById("ashwheat_knock");
		CHECK(EmitterSpots(Knock, {}, Plains, Spots) == SpotSource::AreaCentre);
		REQUIRE(Spots.size() == 1);
		CHECK(Spots[0].col == 40.0);
		CHECK(Spots[0].row == 52.0);
		CHECK(Spots[0].asset == "SW_AMB_AshwheatKnock_01");
		const std::vector<std::pair<double, double>> Doors{ { 37.4, 53.0 }, { 42.2, 54.6 }, { 43.4, 50.8 }, { 41.0, 49.0 } };
		CHECK(EmitterSpots(Knock, Doors, Plains, Spots) == SpotSource::SetPieces);
		REQUIRE(Spots.size() == 4);
		CHECK(Spots[0].asset == "SW_AMB_AshwheatKnock_01");
		CHECK(Spots[1].asset == "SW_AMB_AshwheatKnock_02");
		CHECK(Spots[2].asset == "SW_AMB_AshwheatKnock_03");
		CHECK(Spots[3].asset == "SW_AMB_AshwheatKnock_01");
		CHECK(Spots[1].col == 42.2);
		// The hearth: camp 1's campfire slot (abyss::CampProps on the real map), the manifest's anchor without a map.
		const Emitter& Hearth = EmitterById("knellwatch_hearth");
		CHECK(EmitterSpots(Hearth, {}, Plains, Spots) == SpotSource::CampSlot);
		REQUIRE(Spots.size() == 1);
		CHECK(Spots[0].col == doctest::Approx(Hearth.anchorCol));
		CHECK(Spots[0].row == doctest::Approx(Hearth.anchorRow));
		CHECK(EmitterSpots(Hearth, {}, nullptr, Spots) == SpotSource::Anchor);
		Emitter Bare;
		Bare.assets = { "A" };
		CHECK(EmitterSpots(Bare, {}, Plains, Spots) == SpotSource::None);
		CHECK(Spots.empty());
	}

	TEST_CASE("the region bed and the layer: geometry, conditions, crossfade weights")
	{
		const AreaLoop& Rim = Tables().regions[0];
		const AreaLoop& Ring = Tables().layers[0];
		AbyssAudioData::CircleTracker Tracker;
		const std::vector<AbyssAudioData::TileCircle> Circles = Areas(Tables().regions);
		Tracker.Update(Circles, AbyssAudioData::TileOf(15.0, 112.0));  // 17 tiles: outside
		CHECK_FALSE(Tracker.IsInside(0));
		Tracker.Update(Circles, AbyssAudioData::TileOf(15.0, 111.0));  // 16: inside
		CHECK(Tracker.IsInside(0));
		Tracker.Update(Circles, AbyssAudioData::TileOf(15.0, 112.0));  // 17: stays
		CHECK(Tracker.IsInside(0));
		Tracker.Update(Circles, AbyssAudioData::TileOf(15.0, 113.0));  // 18: leaves
		CHECK_FALSE(Tracker.IsInside(0));
		// Wave 2: the layer rings until M2's turn-in sets rf_hungerbell_silenced, whether the Hungerbell lives or not (its
		// untilFlag replaces Wave 1's `while`); the region bed holds always.
		std::vector<std::string> Alive{ "hunt_hungerbell" };
		std::vector<std::string> Flags;
		const auto IsAlive = [&Alive](std::string_view Id) { return AbyssAudioData::Contains(Alive, Id); };
		const auto HasFlag = [&Flags](std::string_view Id) { return AbyssAudioData::Contains(Flags, Id); };
		CHECK(AbyssAudioData::Holds(Ring.when, IsAlive, HasFlag));
		Alive.clear();
		CHECK(AbyssAudioData::Holds(Ring.when, IsAlive, HasFlag));   // the hunt is dead, the field still rings
		Flags.push_back("rf_hungerbell_silenced");
		CHECK_FALSE(AbyssAudioData::Holds(Ring.when, IsAlive, HasFlag));
		Alive.push_back("hunt_hungerbell");
		CHECK_FALSE(AbyssAudioData::Holds(Ring.when, IsAlive, HasFlag));   // silenced, even with a Hungerbell about
		CHECK(AbyssAudioData::Holds(Rim.when, IsAlive, HasFlag));
		// The flag is the one M2 sets (a real quest's setsFlags).
		const abyss::QuestDef* M2 = audio_test::Data().FindQuest("q_rf_gnawed_field");
		REQUIRE(M2 != nullptr);
		CHECK(AbyssAudioData::Contains(M2->setsFlags, Ring.when.untilFlag));
		// Without flags (the two-argument form) an untilFlag row holds; a row without one keeps the monster rule.
		Alive.clear();
		CHECK(AbyssAudioData::Holds(Ring.when, IsAlive));
		AbyssAudioData::WhileCondition W1 = Ring.when;
		W1.untilFlag.clear();
		CHECK_FALSE(AbyssAudioData::Holds(W1, IsAlive, HasFlag));
		Alive.push_back("hunt_hungerbell");
		CHECK(AbyssAudioData::Holds(W1, IsAlive, HasFlag));
		const abyss::JsonValue Row = audio_test::Json(R"({"while": {"monsterAlive": "m"}, "untilFlag": "f"})");
		const AbyssAudioData::WhileCondition Parsed = AbyssAudioData::ReadWhile(Row);
		CHECK(Parsed.monsterAlive == "m");
		CHECK(Parsed.untilFlag == "f");
		CHECK(AbyssAudioData::ReadWhile(audio_test::Json("{}")).untilFlag.empty());
		// Crossfade weights: in over fadeInSec, out over fadeOutSec.
		double W = 0.0;
		for (int I = 0; I < 30; ++I)
		{
			W = AbyssAudioData::Ramp(W, true, Rim.fadeInSec, Rim.fadeOutSec, 0.05);
		}
		CHECK(W == doctest::Approx(0.5));
		for (int I = 0; I < 40; ++I)
		{
			W = AbyssAudioData::Ramp(W, true, Rim.fadeInSec, Rim.fadeOutSec, 0.05);
		}
		CHECK(W == 1.0);
		W = AbyssAudioData::Ramp(W, false, Ring.fadeInSec, Ring.fadeOutSec, 3.0);
		CHECK(W == doctest::Approx(0.5));
		CHECK(AbyssAudioData::Ramp(0.2, false, 1.0, 0.0, 0.01) == 0.0);
		CHECK(AbyssAudioData::Ramp(0.2, true, 0.0, 1.0, 0.01) == 1.0);
	}
}
