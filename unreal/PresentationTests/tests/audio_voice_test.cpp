// U7 voice families (Source/Abyssfire/Private/Audio/AbyssVoiceRules.h) on the committed audio manifest and the real
// monster table: the Chapter 1 roster speaks its AU1 family, the later chapters keep their animCategory (today's
// humanoid / slime rule), silent monsters have no family cue, the named cues replace / add to the family cues.
#include <string>
#include <string_view>
#include <vector>

#include "doctest/doctest.h"

#include "abyss/base/Enums.h"
#include "abyss/data/DataStore.h"
#include "abyss/data/MonsterData.h"

#include "Audio/AbyssVoiceRules.h"
#include "audio_test_data.h"

namespace
{
	using namespace AbyssVoiceRules;

	const VoiceTables& Tables()
	{
		static const VoiceTables T = Parse(audio_test::Manifest().Get("voiceFamilies"));
		return T;
	}

	/** The voice of a real monster def (animCategory from the data, no design family: withheld in W1). */
	Voice Of(std::string_view DefId)
	{
		const abyss::MonsterDef* Def = audio_test::Data().FindMonster(DefId);
		REQUIRE(Def != nullptr);
		return ResolveFamily(Tables(), DefId, "", abyss::EnumName(Def->animCategory));
	}
}

TEST_SUITE("audio")
{
	TEST_CASE("voice tables parse from the manifest")
	{
		const VoiceTables& T = Tables();
		REQUIRE(T.loaded);
		CHECK(T.families.size() == 6);
		CHECK(T.families.front() == "humanoid");
		CHECK(T.families[1] == "slime");
		CHECK(T.monsters.size() >= 17);
		CHECK(T.silent.size() == 1);
		CHECK(T.silent[0] == "false_lantern");
		// Every family a monster maps to is a cue family of monster_aggro (the variants UE plays).
		const abyss::JsonValue& Aggro = audio_test::Manifest().Get("cues").Get("monster_aggro").Get("families");
		for (const auto& Pair : T.monsters)
		{
			bool bFound = false;
			for (const abyss::JsonValue& F : Aggro.Items())
			{
				bFound = bFound || F.AsString() == Pair.second;
			}
			CHECK_MESSAGE(bFound, Pair.first);
		}
	}

	TEST_CASE("chapter 1 monsters speak their dark family, from data")
	{
		const Voice Footman = Of("risen_footman");
		CHECK(Footman.family == "undead");
		CHECK(Footman.source == FamilySource::Monster);
		// breachclot's animCategory is slime, but its voice is the darkened breach set.
		CHECK(Of("breachclot").family == "breach");
		CHECK(Of("gravegnawer").family == "gnawer");
		CHECK(Of("oath_zealot").family == "human");
		// Hunt leaders are announced with their hunt id as def id (MakeHuntDefinition): no monster def, the map names them.
		CHECK(audio_test::Data().FindMonster("hunt_hungerbell") == nullptr);
		CHECK(ResolveFamily(Tables(), "hunt_hungerbell", "", "").family == "gnawer");
		CHECK(ResolveFamily(Tables(), "hunt_dreck", "", "").family == "human");
		CHECK(ResolveFamily(Tables(), "hunt_harke", "", "").family == "human");
		CHECK(Of("valdras").family == "undead");
		CHECK(Of("miniboss_corpseherd").family == "human");
	}

	TEST_CASE("later chapters keep their animCategory (today's rule)")
	{
		int Checked = 0;
		for (const char* Id : { "skeleton", "zombie", "werewolf", "gargoyle", "stone_golem", "sandworm", "imp",
				 "lesser_demon", "demon_lord", "miniboss_shadow_weaver", "dungeon_shade" })
		{
			const abyss::MonsterDef* Def = audio_test::Data().FindMonster(Id);
			if (Def == nullptr)
			{
				continue;  // not in this data set
			}
			++Checked;
			const Voice V = ResolveFamily(Tables(), Id, "", abyss::EnumName(Def->animCategory));
			CHECK(V.source == FamilySource::AnimCategory);
			CHECK(V.family == std::string(abyss::EnumName(Def->animCategory)));
		}
		CHECK(Checked == 11);
		// No Chapter 2-5 monster is in the AU1 map: the roster map only names Chapter 1 ids (and the W3 Pit's warden).
		for (const auto& Pair : Tables().monsters)
		{
			const abyss::MonsterDef* Def = audio_test::Data().FindMonster(Pair.first);
			if (Def != nullptr)
			{
				CHECK_MESSAGE(Def->homeZone == "emerald_plains", Pair.first);
			}
		}
		// Synthetic: an unmapped id falls to its rig, an empty rig to nothing.
		CHECK(ResolveFamily(Tables(), "chapter2_wolf", "", "beast").family == "beast");
		CHECK(ResolveFamily(Tables(), "chapter2_wolf", "", "").source == FamilySource::None);
	}

	TEST_CASE("design family aliases, silent monsters")
	{
		// The monster `family` field (withheld from the export in W1) resolves through the aliases.
		const Voice ViaAlias = ResolveFamily(Tables(), "a_new_unburied", "unburied", "humanoid");
		CHECK(ViaAlias.family == "undead");
		CHECK(ViaAlias.source == FamilySource::Alias);
		CHECK(ResolveFamily(Tables(), "a_new_clot", "breach_blood", "slime").family == "breach");
		CHECK(ResolveFamily(Tables(), "x", "gnawer", "beast").family == "gnawer");  // already a voice family
		CHECK(ResolveFamily(Tables(), "x", "dragons", "flying").family == "flying");  // unknown design family
		// The roster map wins over the design family.
		CHECK(ResolveFamily(Tables(), "risen_footman", "living", "humanoid").family == "undead");
		const Voice Lantern = ResolveFamily(Tables(), "false_lantern", "", "flying");
		CHECK(Lantern.IsSilent());
		CHECK(Lantern.family.empty());
	}

	TEST_CASE("named cues replace the family cues and follow the boss beats")
	{
		const VoiceTables& T = Tables();
		CHECK(OverrideCue(T, "valdras", KeyAggro) == "vox_valdras_order");
		CHECK(OverrideCue(T, "valdras", KeyHurt) == "vox_valdras_hurt");
		CHECK(OverrideCue(T, "valdras", KeyDeathVocal) == "vox_valdras_death");
		CHECK(OverrideCue(T, "valdras", AnimKey("roar")) == "vox_valdras_roar");
		CHECK(OverrideCue(T, "valdras", KeyStorySay) == "vox_valdras_order");
		CHECK(OverrideCue(T, "miniboss_corpseherd", KeyMiniBossOpened) == "vox_corpseherd_lullaby");
		CHECK(OverrideCue(T, "miniboss_corpseherd", KeyDeathVocal) == "vox_corpseherd_home");
		CHECK(OverrideCue(T, "risen_footman", KeyAggro).empty());
		CHECK(OverrideCue(T, "valdras", AnimKey("walk")).empty());
		CHECK(OverrideCue(T, "", KeyAggro).empty());
		// Every named cue has a manifest row (UE plays nothing for a cue without one).
		const abyss::JsonValue& Cues = audio_test::Manifest().Get("cues");
		for (const NamedCue& Row : T.overrides)
		{
			CHECK_MESSAGE(Cues.Has(Row.cue), Row.cue);
		}
		CHECK(SfxOverrideKey("monster_aggro") == KeyAggro);
		CHECK(SfxOverrideKey("monster_hurt") == KeyHurt);
		CHECK(SfxOverrideKey("monster_death") == KeyDeathVocal);
		CHECK(SfxOverrideKey("hit").empty());
	}

	TEST_CASE("a def's voice resolved once matches the one-by-one answers (the per-def cache of the SFX path)")
	{
		const VoiceTables& T = Tables();
		const DefVoice Valdras = ResolveDefVoice(T, "valdras", "", "humanoid");
		CHECK(Valdras.voice.family == "undead");
		CHECK(Valdras.aggroCue == "vox_valdras_order");
		CHECK(Valdras.hurtCue == "vox_valdras_hurt");
		CHECK(Valdras.deathVocalCue == "vox_valdras_death");
		CHECK(Valdras.hasAnimCue);
		const DefVoice Herd = ResolveDefVoice(T, "miniboss_corpseherd", "", "humanoid");
		CHECK(Herd.hasAnimCue);
		CHECK(Herd.aggroCue == "vox_corpseherd_bell");
		const DefVoice Footman = ResolveDefVoice(T, "risen_footman", "", "humanoid");
		CHECK(Footman.voice.family == "undead");
		CHECK(Footman.aggroCue.empty());
		CHECK(Footman.deathVocalCue.empty());
		CHECK_FALSE(Footman.hasAnimCue);
		const DefVoice Lantern = ResolveDefVoice(T, "false_lantern", "", "flying");
		CHECK(Lantern.voice.IsSilent());
		CHECK_FALSE(ResolveDefVoice(T, "", "", "").hasAnimCue);
		CHECK_FALSE(HasAnimCue(T, "skeleton"));

		// Every real monster def and every overridden id: the bundle equals the single lookups.
		std::vector<std::string> Ids;
		for (const abyss::MonsterDef& Def : audio_test::Data().Monsters().defs)
		{
			Ids.push_back(Def.id);
		}
		for (const NamedCue& Row : T.overrides)
		{
			Ids.push_back(Row.monster);
		}
		for (const std::string& Id : Ids)
		{
			CAPTURE(Id);
			const abyss::MonsterDef* Def = audio_test::Data().FindMonster(Id);
			const std::string_view Rig = Def != nullptr ? abyss::EnumName(Def->animCategory) : std::string_view();
			const DefVoice V = ResolveDefVoice(T, Id, "", Rig);
			const Voice One = ResolveFamily(T, Id, "", Rig);
			CHECK(V.voice.family == One.family);
			CHECK(V.voice.source == One.source);
			CHECK(V.aggroCue == OverrideCue(T, Id, KeyAggro));
			CHECK(V.hurtCue == OverrideCue(T, Id, KeyHurt));
			CHECK(V.deathVocalCue == OverrideCue(T, Id, KeyDeathVocal));
			bool bAnim = false;
			for (const char* Action : { "idle", "walk", "attack", "cast", "hurt", "dodge", "death", "signature", "work",
					 "talk", "roar", "taunt", "stun", "attack02", "cast02", "rise" })
			{
				bAnim = bAnim || !OverrideCue(T, Id, AnimKey(Action)).empty();
			}
			CHECK(V.hasAnimCue == bAnim);
		}
	}

	TEST_CASE("a manifest without voiceFamilies keeps today's rule")
	{
		const VoiceTables Empty = Parse(abyss::JsonValue());
		CHECK_FALSE(Empty.loaded);
		CHECK(ResolveFamily(Empty, "risen_footman", "", "humanoid").family == "humanoid");
		CHECK(ResolveFamily(Empty, "breachclot", "", "slime").family == "slime");
		CHECK(OverrideCue(Empty, "valdras", KeyAggro).empty());
	}
}
