// Monster states (Source/Abyssfire/Private/Actors/AbyssMonsterLookRules.h, dark remake 2G stage C): flight and the
// shield wall's facing, held items, phase looks, auras, buff / call pulses, block sparks and cues, the flight cue, the
// elite sigil, the champion glyph, the Unburied affix loop, the death recipe chain and the zone's art warm-up, on the real
// exported data (unreal/Data), the art manifest (Data/assets.json), vfx_recipes.json and the audio manifest. Chapters 2-5
// keep today's look: no family, no leader, no phase table, no corpse-holding death there.
#include <algorithm>
#include <cmath>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "doctest/doctest.h"

#include "abyss/base/Enums.h"
#include "abyss/base/Json.h"
#include "abyss/data/CombatData.h"
#include "abyss/data/DataStore.h"
#include "abyss/data/MapData.h"
#include "abyss/data/MonsterData.h"
#include "abyss/monsters/Monster.h"

#include "Actors/AbyssMonsterLookRules.h"
#include "World/AbyssThemeRules.h"
#include "audio_test_data.h"

namespace
{
	using namespace AbyssMonsterLookRules;

	const abyss::JsonValue& ArtManifest()
	{
		static abyss::JsonValue Root;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			const std::string Text = audio_test::ReadBytes(audio_test::UnrealDir() + "/Data/assets.json");
			if (Text.empty() || !abyss::ParseJson(Text, Root))
			{
				Root = abyss::JsonValue();
			}
		}
		const abyss::JsonValue* Manifest = Root.Find("manifest");
		return Manifest != nullptr ? *Manifest : Root;
	}

	const abyss::JsonValue& Recipes()
	{
		static abyss::JsonValue Root;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			const std::string Text = audio_test::ReadBytes(audio_test::UnrealDir() + "/Art/Export/VFX/vfx_recipes.json");
			if (Text.empty() || !abyss::ParseJson(Text, Root))
			{
				Root = abyss::JsonValue();
			}
		}
		return Root;
	}

	bool HasRecipe(std::string_view Id)
	{
		return Recipes().Get("recipes").Has(Id) || Recipes().Get("aliases").Has(Id);
	}

	/** The first recipe of a chain the art provides ("" = none). */
	std::string FirstRecipe(const std::vector<std::string>& Chain)
	{
		for (const std::string& Id : Chain)
		{
			if (HasRecipe(Id))
			{
				return Id;
			}
		}
		return {};
	}

	const abyss::DataStore& Data() { return audio_test::Data(); }

	const abyss::JsonValue& Asset(std::string_view Name)
	{
		return ArtManifest().Get("assets").Get(Name);
	}

	const abyss::JsonValue& AssetByGameId(std::string_view GameId)
	{
		static const abyss::JsonValue Null;
		for (const abyss::JsonMember& Member : ArtManifest().Get("assets").Members())
		{
			for (const abyss::JsonValue& Id : Member.value.Get("gameIds").Items())
			{
				if (Id.AsString() == GameId)
				{
					return Member.value;
				}
			}
		}
		return Null;
	}

	bool HasSocket(const abyss::JsonValue& AssetJson, std::string_view Socket)
	{
		for (const abyss::JsonValue& S : AssetJson.Get("sockets").Items())
		{
			if (S.Get("name").AsString() == Socket)
			{
				return true;
			}
		}
		return false;
	}

	std::vector<const abyss::MonsterDef*> Chapter1Defs()
	{
		std::vector<const abyss::MonsterDef*> Out;
		for (const abyss::MonsterDef& Def : Data().Monsters().defs)
		{
			if (!Def.family.empty())
			{
				Out.push_back(&Def);
			}
		}
		for (const abyss::HuntDef& Hunt : Data().Monsters().hunts)
		{
			if (!Hunt.defNormal.family.empty())
			{
				Out.push_back(&Hunt.defNormal);
			}
		}
		return Out;
	}

	bool InAudioManifest(std::string_view CueId, bool bPlanned)
	{
		const abyss::JsonValue& M = audio_test::Manifest();
		return bPlanned ? M.Get("plannedCues").Has(CueId) : M.Get("cues").Has(CueId);
	}
}

TEST_SUITE("monsterlook")
{
	TEST_CASE("a fleeing monster walks and faces its heading; a blocker keeps its front in Attack")
	{
		using abyss::MonsterState;
		CHECK(CountsAsMoving(MonsterState::Flee, 1.0));
		CHECK(CountsAsMoving(MonsterState::Chase, 1.0));
		CHECK(CountsAsMoving(MonsterState::Patrol, 1.0));
		CHECK(CountsAsMoving(MonsterState::Returning, 1.0));
		CHECK_FALSE(CountsAsMoving(MonsterState::Flee, 0.0));   // cornered: stands
		CHECK_FALSE(CountsAsMoving(MonsterState::Attack, 1.0));
		CHECK_FALSE(CountsAsMoving(MonsterState::Idle, 1.0));
		CHECK_FALSE(CountsAsMoving(MonsterState::Dead, 1.0));
		CHECK(FacesHeading(MonsterState::Flee, 0.0));
		CHECK(FacesHeading(MonsterState::Chase, 0.0));   // a kite step backs off facing its heading
		CHECK_FALSE(FacesHeading(MonsterState::Attack, 0.0));
		CHECK(FacesHeading(MonsterState::Attack, 100.0));
		// the shield wall's blockers are the shieldbearers
		for (const abyss::MonsterDef* Def : Chapter1Defs())
		{
			if (Def->block.present)
			{
				CHECK(Def->block.arcDeg > 0.0);
			}
		}
	}

	TEST_CASE("held items: the Unburied keep their weapon and wear the helm; every other asset keeps today's pick")
	{
		const FAttachmentSlots Footman = ClassifyAttachments({ "weapon_r", "head" });
		CHECK(Footman.Main == 0);
		CHECK(Footman.Off == -1);
		CHECK(Footman.Extras == std::vector<int>{ 1 });
		const FAttachmentSlots Shield = ClassifyAttachments({ "weapon_r", "weapon_l", "head" });
		CHECK(Shield.Main == 0);
		CHECK(Shield.Off == 1);
		CHECK(Shield.Extras == std::vector<int>{ 2 });
		const FAttachmentSlots Lone = ClassifyAttachments({ "back" });
		CHECK(Lone.Main == 0);
		CHECK(Lone.Extras.empty());
		const FAttachmentSlots Empty = ClassifyAttachments({});
		CHECK(Empty.Main == -1);
		CHECK(Empty.Off == -1);
		// the real manifest
		int Extras = 0;
		for (const abyss::JsonMember& Member : ArtManifest().Get("assets").Members())
		{
			const std::span<const abyss::JsonValue> Attachments = Member.value.Get("attachments").Items();
			if (Attachments.empty())
			{
				continue;
			}
			CAPTURE(Member.key);
			std::vector<std::string_view> Sockets;
			int TodayMain = -1, TodayOff = -1, Mains = 0, Offs = 0;
			for (size_t I = 0; I < Attachments.size(); ++I)
			{
				const std::string_view Socket = Attachments[I].Get("socket").AsString();
				Sockets.push_back(Socket);
				if (Socket == OffHandSocket)
				{
					TodayOff = static_cast<int>(I);
					++Offs;
				}
				else
				{
					TodayMain = static_cast<int>(I);
					++Mains;
				}
			}
			const FAttachmentSlots Slots = ClassifyAttachments(Sockets);
			if (Mains <= 1 && Offs <= 1)
			{
				CHECK(Slots.Main == TodayMain);   // heroes, NPCs, chapters 2-5: unchanged
				CHECK(Slots.Off == TodayOff);
				CHECK(Slots.Extras.empty());
				continue;
			}
			// today the last one won: the helm on the head replaced the weapon
			REQUIRE(Slots.Main >= 0);
			CHECK(Sockets[static_cast<size_t>(Slots.Main)] == MainHandSocket);
			for (const int Extra : Slots.Extras)
			{
				CHECK(Attachments[static_cast<size_t>(Extra)].Get("object").AsString() == "SM_Mon_Unburied_Att_KettleHelm");
				++Extras;
			}
		}
		CHECK(Extras == 4);   // the footman, the arbalest, the shieldbearer, the standard-bearer
	}

	TEST_CASE("phase looks: the zealot's frenzy from its first phase, Valdras's standard from his third")
	{
		const abyss::JsonValue& Zealot = Asset("SK_Mon_OathZealot");
		const abyss::MonsterDef* ZealotDef = Data().FindMonster("oath_zealot");
		const abyss::MonsterDef* ValdrasDef = Data().FindMonster("valdras");
		REQUIRE(ZealotDef != nullptr);
		REQUIRE(ValdrasDef != nullptr);
		// the as-built zealot enters his frenzy without an enter action (2F-D8): no Roar plays, so the surge plays at the
		// phase entry and the breath loops from then on
		REQUIRE(ZealotDef->phases.size() == 1u);
		CHECK_FALSE(ZealotDef->phases[0].hasEnterAction);
		CHECK(PhaseEnterClip(ZealotDef, 1).empty());
		const FPhaseLook Frenzy = PhaseLookFor("oath_zealot", ZealotDef, 1, false, Zealot);
		CHECK(Frenzy.Instant == "notify.FX_Frenzy");
		CHECK(Frenzy.Loop == "monster.oath_zealot.frenzy");
		CHECK(HasRecipe(Frenzy.Instant));
		CHECK(HasRecipe(Frenzy.Loop));
		// an unknown definition assumes no enter action: the surge still shows
		CHECK(PhaseLookFor("oath_zealot", nullptr, 1, false, Zealot).Instant == "notify.FX_Frenzy");
		// the art's Roar carries FX_Frenzy @480: a row that does enter with Roar lets that notify play the surge instead
		bool bRoarNotify = false;
		for (const abyss::JsonValue& Clip : Zealot.Get("anims").Items())
		{
			if (Clip.Get("name").AsString() != "Roar")
			{
				continue;
			}
			for (const abyss::JsonValue& Notify : Clip.Get("notifies").Items())
			{
				bRoarNotify = bRoarNotify || (Notify.Get("name").AsString() == "FX_Frenzy" && Notify.Get("ms").AsDouble() == 480.0);
			}
		}
		CHECK(bRoarNotify);
		CHECK(Zealot.Get("fx").Get("frenzy").Get("clip").AsString() == "Roar");
		abyss::MonsterDef RoaringZealot = *ZealotDef;
		RoaringZealot.phases[0].hasEnterAction = true;
		RoaringZealot.phases[0].enterAction = abyss::AnimAction::Roar;
		CHECK(PhaseEnterClip(&RoaringZealot, 1) == "Roar");
		const FPhaseLook Roared = PhaseLookFor("oath_zealot", &RoaringZealot, 1, false, Zealot);
		CHECK(Roared.Instant.empty());
		CHECK(Roared.Loop == "monster.oath_zealot.frenzy");
		// an enter action whose clip carries no FX_Frenzy (a Taunt) still plays the surge at the entry
		RoaringZealot.phases[0].enterAction = abyss::AnimAction::Taunt;
		CHECK(PhaseLookFor("oath_zealot", &RoaringZealot, 1, false, Zealot).Instant == "notify.FX_Frenzy");
		// a frenzy block without a clip notify plays its recipe at the entry, whatever the row plays
		abyss::JsonValue Bare;
		CHECK(abyss::ParseJson(R"({"fx":{"frenzy":{"recipe":"notify.FX_Frenzy","loop":"monster.oath_zealot.frenzy"}}})", Bare));
		RoaringZealot.phases[0].enterAction = abyss::AnimAction::Roar;
		CHECK(PhaseLookFor("oath_zealot", &RoaringZealot, 1, false, Bare).Instant == "notify.FX_Frenzy");
		CHECK(PhaseLookFor("oath_zealot", ZealotDef, 1, true, Zealot).Loop.empty());   // a reset drops it
		CHECK(PhaseLookFor("oath_zealot", ZealotDef, 1, true, Zealot).Instant.empty());
		CHECK(PhaseLookFor("oath_zealot", ZealotDef, 0, false, Zealot).Loop.empty());
		CHECK(PhaseEnterClip(ZealotDef, 2).empty());   // past the table
		const abyss::JsonValue& Valdras = Asset("SK_Mon_Valdras");
		CHECK(PhaseLookFor("valdras", ValdrasDef, 1, false, Valdras).Loop.empty());   // P2: the roll call (stinger, summons)
		CHECK(PhaseLookFor("valdras", ValdrasDef, 1, false, Valdras).Instant.empty());
		const FPhaseLook Fire = PhaseLookFor("valdras", ValdrasDef, 2, false, Valdras);
		CHECK(Fire.Instant.empty());
		CHECK(Fire.Loop == "monster.valdras.standard_fire");
		CHECK(HasRecipe(Fire.Loop));
		CHECK(PhaseLookFor("valdras", ValdrasDef, 2, true, Valdras).Loop.empty());
		// the data they key on: Valdras has two rows (his third phase is index 2), both entered with a Roar
		CHECK(ValdrasDef->phases.size() == 2u);
		CHECK(PhaseEnterClip(ValdrasDef, 1) == "Roar");
		CHECK(PhaseEnterClip(ValdrasDef, 2) == "Roar");
		// no other definition has a phase look: chapters 2-5 have no phase table at all
		for (const abyss::MonsterDef& Def : Data().Monsters().defs)
		{
			if (Def.family.empty())
			{
				CHECK(Def.phases.empty());
			}
		}
	}

	TEST_CASE("aura loops resolve through the art; the drummer's aura has no look yet")
	{
		CHECK(AuraChain("rollcall_standard_bearer", "monster_dead_standard_bearer")
			== std::vector<std::string>{ "monster.rollcall_standard_bearer.aura", "monster.dead_standard_bearer.aura" });
		CHECK(AuraChain("dead_standard_bearer", "monster_dead_standard_bearer")
			== std::vector<std::string>{ "monster.dead_standard_bearer.aura" });
		CHECK(WantsAuraLoop(true, 5.0));
		CHECK_FALSE(WantsAuraLoop(false, 5.0));
		CHECK_FALSE(WantsAuraLoop(true, 0.0));
		std::set<std::string> Lit;
		std::set<std::string> Unlit;
		for (const abyss::MonsterDef* Def : Chapter1Defs())
		{
			if (!Def->aura.present)
			{
				continue;
			}
			(FirstRecipe(AuraChain(Def->id, Def->spriteKey)).empty() ? Unlit : Lit).insert(Def->id);
		}
		CHECK(Lit == std::set<std::string>{ "dead_standard_bearer", "rollcall_standard_bearer" });
		CHECK(Unlit == std::set<std::string>{ "deserter_drummer" });   // the Living's no-glow aura: art follow-up
	}

	TEST_CASE("body loops: the Cinderbloat's vent sheds ash flakes; nobody else carries one")
	{
		CHECK(BodyLoopRecipe(Asset("SK_Mon_Cinderbloat")) == "monster.ash_flakes");
		CHECK(HasRecipe("monster.ash_flakes"));
		CHECK(BodyLoopRecipe(Asset("SK_Mon_Deserter")).empty());
		int Loops = 0;
		for (const abyss::JsonMember& Member : ArtManifest().Get("assets").Members())
		{
			Loops += BodyLoopRecipe(Member.value).empty() ? 0 : 1;
		}
		CHECK(Loops == 1);
	}

	TEST_CASE("buff / call pulses: the clip's own beats, else the bell's violet ring for the breach-touched, the drum's dust for the rest")
	{
		// families: violet is the breach's (DR5); the Living and the beasts carry none
		CHECK(BreachTouched("unburied"));
		CHECK(BreachTouched("breach"));
		CHECK_FALSE(BreachTouched("living"));
		CHECK_FALSE(BreachTouched("gravegnawers"));
		CHECK_FALSE(BreachTouched(""));
		CHECK(PulseChain("deserter_drummer", "war_drum", "living", false).back() == "notify.FX_DrumBeat_1");
		CHECK(PulseChain("hunt_hungerbell", "supper_bell", "gravegnawers", false).back() == "notify.FX_DrumBeat_1");
		CHECK(PulseChain("some_raiser", "call", "unburied", false).back() == "notify.FX_bell_ring");
		CHECK(PulseChain("some_bloat", "call", "breach", false).back() == "notify.FX_bell_ring");
		// a cast clip with FX notifies of its own shows the pulse on its beats: only the ability's own recipe may add to it
		const std::vector<std::string> Beats = PulseChain("deserter_drummer", "war_drum", "living", true);
		REQUIRE(Beats.size() == 1);
		CHECK(Beats.front() == "monster.deserter_drummer.war_drum");
		CHECK(PulseChain("", "war_drum", "living", true).empty());
		// the fallbacks' palettes: the dust has no light and no violet; the ring is the breach's
		CHECK(Recipes().Get("recipes").Get("notify.FX_DrumBeat_1").Get("palette").AsString() == "ash");
		CHECK(Recipes().Get("recipes").Get("notify.FX_bell_ring").Get("palette").AsString() == "breach");
		// the clip comes from the ability's definition (never from whatever clip plays at the cast)
		const abyss::MonsterDef* Drummer = FindMonsterDef(Data(), "deserter_drummer");
		const abyss::MonsterDef* Hungerbell = FindMonsterDef(Data(), "hunt_hungerbell");
		REQUIRE(Drummer != nullptr);
		REQUIRE(Hungerbell != nullptr);
		CHECK(AbilityCastClip(Drummer, "war_drum") == "Cast01");
		CHECK(AbilityCastClip(Hungerbell, "supper_bell").empty());   // clipless: its pulse is the whole tell
		CHECK(AbilityCastClip(Hungerbell, "no_such_ability").empty());
		CHECK(AbilityCastClip(nullptr, "war_drum").empty());
		CHECK(NotifiesShowPulse({ "Hit", "FX_DrumBeat_1" }));
		CHECK_FALSE(NotifiesShowPulse({ "Hit", "Footstep" }));
		CHECK_FALSE(NotifiesShowPulse({ "FXless", "Fx_Lower" }));   // the FX_ prefix exactly (the manifest's notify names)
		CHECK_FALSE(NotifiesShowPulse({}));
		// the art behind it: the drummer's Cast01 beats the drum four times; the Hungerbell's Attack02 (the Clapper Slam)
		// rings its bell, which no longer hides the clipless call's pulse while it plays
		auto ClipNotifies = [](const abyss::JsonValue& AssetJson, std::string_view ClipName)
		{
			std::vector<std::string> Out;
			for (const abyss::JsonValue& Clip : AssetJson.Get("anims").Items())
			{
				if (Clip.Get("name").AsString() == ClipName)
				{
					for (const abyss::JsonValue& Notify : Clip.Get("notifies").Items())
					{
						Out.emplace_back(Notify.Get("name").AsString());
					}
				}
			}
			return Out;
		};
		auto Views = [](const std::vector<std::string>& Names)
		{
			return std::vector<std::string_view>(Names.begin(), Names.end());
		};
		const std::vector<std::string> Drum = ClipNotifies(Asset("SK_Mon_DeserterDrummer"), "Cast01");
		for (const char* Beat : { "FX_DrumBeat_1", "FX_DrumBeat_2", "FX_DrumBeat_3", "FX_DrumBeat_4" })
		{
			CHECK(std::find(Drum.begin(), Drum.end(), Beat) != Drum.end());
		}
		CHECK(NotifiesShowPulse(Views(Drum)));
		const std::vector<std::string> Bell = ClipNotifies(Asset("SK_Mon_Bonegorger"), "Attack02");
		CHECK(std::find(Bell.begin(), Bell.end(), "FX_BellRing") != Bell.end());
		CHECK(HasRecipe("notify.FX_DrumBeat_1"));
		CHECK(HasRecipe("notify.FX_BellRing"));
		// every Chapter 1 buff / call shows a pulse: its clip's beats, else a recipe of the chain; the supper bell's is the
		// pale dust (the Gravegnawers are beasts: no violet)
		int Calls = 0;
		int ClipPulses = 0;
		for (const abyss::MonsterDef* Def : Chapter1Defs())
		{
			for (const abyss::MonsterAbilityDef& A : Def->abilities)
			{
				if (A.kind != abyss::AbilityKind::Buff && A.kind != abyss::AbilityKind::Call)
				{
					continue;
				}
				CAPTURE(Def->id);
				CAPTURE(A.id);
				const std::string_view Clip = AbilityCastClip(Def, A.id);
				CHECK(Clip == A.clip);
				const bool bClipShows = !Clip.empty() && NotifiesShowPulse(Views(ClipNotifies(AssetByGameId(Def->spriteKey), Clip)));
				ClipPulses += bClipShows ? 1 : 0;
				const std::string First = FirstRecipe(PulseChain(Def->id, A.id, Def->family, bClipShows));
				CHECK((bClipShows || !First.empty()));
				if (Def->id == "hunt_hungerbell")
				{
					CHECK(First == "notify.FX_DrumBeat_1");
				}
				++Calls;
			}
		}
		CHECK(Calls >= 2);        // the war drum, the supper bell
		CHECK(ClipPulses >= 1);   // the war drum's beats
	}

	TEST_CASE("loops turn with the body only once its facing moved")
	{
		CHECK(BlowTurned(0.0, 0.0, 1.0, 0.0));   // never pushed
		CHECK_FALSE(BlowTurned(1.0, 0.0, 1.0, 0.0));
		const double Small = 1.0 * Pi / 180.0;
		const double Large = 3.0 * Pi / 180.0;
		CHECK_FALSE(BlowTurned(1.0, 0.0, std::cos(Small), std::sin(Small)));
		CHECK(BlowTurned(1.0, 0.0, std::cos(Large), std::sin(Large)));
		CHECK(BlowTurned(1.0, 0.0, -1.0, 0.0));
		CHECK(BlowTurned(2.0, 0.0, 0.0, 5.0));          // lengths do not matter
		CHECK_FALSE(BlowTurned(1.0, 0.0, 0.0, 0.0));   // no facing: nothing to push
	}

	TEST_CASE("the block spark sits on the shield's face; the clang falls back to the port's block cue")
	{
		const FBlockFx Fx = ParseBlockFx(Asset("SK_Mon_RisenShieldbearer"));
		CHECK(Fx.bValid);
		CHECK(Fx.Item == "SM_MonWeapon_TowerShield");
		CHECK(Fx.Socket == "face");
		CHECK(HasRecipe(Fx.Recipe));
		CHECK(HasSocket(Asset(Fx.Item), Fx.Socket));
		CHECK_FALSE(ParseBlockFx(Asset("SK_Mon_RisenFootman")).bValid);
		for (const abyss::MonsterDef* Def : Chapter1Defs())
		{
			if (Def->block.present)
			{
				CAPTURE(Def->id);
				CHECK(ParseBlockFx(AssetByGameId(Def->spriteKey)).bValid);
			}
		}
		const std::vector<std::string_view> Cues = BlockCueChain();
		REQUIRE(Cues.size() == 2u);
		CHECK(InAudioManifest(Cues[0], true));    // planned: silent until rendered
		CHECK_FALSE(InAudioManifest(Cues[0], false));
		CHECK(InAudioManifest(Cues[1], false));   // SW_SFX_Block_*
		CHECK(FleeCue(false, "living") == "sfx_deserter_flee");
		CHECK(InAudioManifest(FleeCue(false, "living"), true));
		CHECK(FleeCue(true, "living").empty());
		CHECK(FleeCue(false, "unburied").empty());
		// every morale monster is one of the Living
		for (const abyss::MonsterDef* Def : Chapter1Defs())
		{
			if (Def->morale.present)
			{
				CHECK(Def->family == "living");
			}
		}
	}

	TEST_CASE("the elite marker: the sigil in a themed zone (Valdras opts out), today's crown elsewhere")
	{
		CHECK(EliteMarkerFor(false, true, "") == EEliteMarker::None);
		CHECK(EliteMarkerFor(true, false, "") == EEliteMarker::Crown);
		CHECK(EliteMarkerFor(true, false, "none") == EEliteMarker::Crown);   // chapters 2-5: as before
		CHECK(EliteMarkerFor(true, true, "") == EEliteMarker::Sigil);
		CHECK(EliteMarkerFor(true, true, "none") == EEliteMarker::None);
		CHECK(Asset("SK_Mon_Valdras").Get("eliteMarker").AsString() == "none");
		// the sigil's motion contract
		const abyss::JsonValue& Sigil = AssetByGameId(SigilGameId);
		REQUIRE_FALSE(Sigil.IsNull());
		const FSigilMotion M = ParseSigilMotion(Sigil);
		CHECK(M.BobCm == doctest::Approx(3.0));
		CHECK(M.BobPeriodMs == doctest::Approx(1600.0));
		CHECK(M.SpinDegPerSec == doctest::Approx(45.0));
		CHECK(M.bAbsoluteScale);
		CHECK(M.Socket == "fx_elite");
		CHECK(M.FloatCm == doctest::Approx(6.0));
		// a symmetric bob about the float: rest 6, crest 9, trough 3
		CHECK(SigilLiftCm(M, 0.0) == doctest::Approx(6.0));
		CHECK(SigilLiftCm(M, 400.0) == doctest::Approx(9.0));
		CHECK(SigilLiftCm(M, 1200.0) == doctest::Approx(3.0));
		CHECK(SigilYawDeg(M, 1000.0) == doctest::Approx(45.0));
		CHECK(SigilYawDeg(M, 9000.0) == doctest::Approx(45.0));   // wraps at 8 s
		// every Chapter 1 monster's art carries the socket the sigil rides (else it falls back to fx_overhead)
		for (const abyss::MonsterDef* Def : Chapter1Defs())
		{
			CAPTURE(Def->id);
			const abyss::JsonValue& Art = AssetByGameId(Def->spriteKey);
			REQUIRE_FALSE(Art.IsNull());
			CHECK(HasSocket(Art, M.Socket));
		}
	}

	TEST_CASE("the champion glyph marks living pack leaders; only Chapter 1 has leaders")
	{
		CHECK(ShowsChampionMarker(true, true));
		CHECK_FALSE(ShowsChampionMarker(true, false));
		CHECK_FALSE(ShowsChampionMarker(false, true));
		// crimson, never the ember family (DR5)
		const uint32_t R = (ChampionRgb >> 16) & 0xFF, G = (ChampionRgb >> 8) & 0xFF, B = ChampionRgb & 0xFF;
		CHECK(R > G);
		CHECK(R > B);
		CHECK(G < 0x40u);
		int Leaders = 0;
		for (const char* MapId : { "emerald_plains", "twilight_forest", "anvil_mountains", "scorching_desert", "abyss_rift" })
		{
			const abyss::MapDef* Map = Data().FindMap(MapId);
			REQUIRE(Map != nullptr);
			int Here = 0;
			for (const abyss::MapSpawnDef& Spawn : Map->spawns)
			{
				Here += Spawn.leader.present ? 1 : 0;
			}
			if (std::string_view(MapId) == "emerald_plains")
			{
				CHECK(Here > 0);
				CHECK_FALSE(AbyssThemeRules::ThemeOf(Map).empty());
			}
			else
			{
				CHECK(Here == 0);
			}
			Leaders += Here;
		}
		CHECK(Leaders > 0);
	}

	TEST_CASE("the Unburied affix loops elite.unburied; no other affix gains a loop")
	{
		for (size_t I = 0; I < abyss::EnumCount<abyss::EliteAffixType>(); ++I)
		{
			const auto Affix = static_cast<abyss::EliteAffixType>(I);
			CAPTURE(abyss::EnumName(Affix));
			if (Affix == abyss::EliteAffixType::Unburied)
			{
				CHECK(AffixLoopRecipe(Affix) == "elite.unburied");
				CHECK(HasRecipe(AffixLoopRecipe(Affix)));
			}
			else
			{
				CHECK(AffixLoopRecipe(Affix).empty());
			}
		}
		// the affix rolls only for the Unburied family (chapters 2-5 never show it)
		const abyss::EliteAffixDef& Def = Data().Combat().eliteAffixes.Def(abyss::EliteAffixType::Unburied);
		CHECK(Def.families == std::vector<std::string>{ "unburied" });
		CHECK(Def.vfxColor == 0xD08CFFu);
	}

	TEST_CASE("kills: death.<defId>, death.<family>, death.burst; no coins for a Raised body")
	{
		const FDeathFx Footman = DeathFxFor("risen_footman", "unburied", abyss::MonsterRole::Regular);
		CHECK(Footman.Chain == std::vector<std::string>{ "death.risen_footman", "death.unburied", "death.burst" });
		CHECK(Footman.bCoins);
		CHECK_FALSE(DeathFxFor("risen_footman", "unburied", abyss::MonsterRole::Raised).bCoins);
		CHECK(DeathUsesVictimFacing("death.unburied"));
		CHECK_FALSE(DeathUsesVictimFacing("death.burst"));
		CHECK(FirstRecipe(DeathFxFor("valdras", "unburied", abyss::MonsterRole::StoryBoss).Chain) == "death.valdras");
		CHECK(FirstRecipe(DeathFxFor("cinderbloat", "breach", abyss::MonsterRole::Regular).Chain) == "death.cinderbloat");
		// every Chapter 1 definition finds its family's (or its own) recipe
		for (const abyss::MonsterDef* Def : Chapter1Defs())
		{
			CAPTURE(Def->id);
			const std::string First = FirstRecipe(DeathFxFor(Def->id, Def->family, abyss::MonsterRole::Regular).Chain);
			CHECK(First != "death.burst");
			CHECK_FALSE(First.empty());
		}
		// every other definition keeps today's burst (chapters 2-5, the Labyrinth)
		for (const abyss::MonsterDef& Def : Data().Monsters().defs)
		{
			if (Def.family.empty())
			{
				CAPTURE(Def.id);
				CHECK(FirstRecipe(DeathFxFor(Def.id, Def.family, abyss::MonsterRole::Regular).Chain) == "death.burst");
			}
		}
	}

	TEST_CASE("the zone warm-up covers every body Chapter 1 can stand up; other zones keep at least today's")
	{
		const abyss::MapDef* Plains = Data().FindMap("emerald_plains");
		REQUIRE(Plains != nullptr);
		const std::vector<std::string> Ids = PreloadDefIds(Data(), *Plains);
		const std::set<std::string> Set(Ids.begin(), Ids.end());
		CHECK(Set.size() == Ids.size());
		// the corpse seeds are warmed on their own (a map whose seeds are no spawn of it)
		abyss::MapDef SeedsOnly = *Plains;
		SeedsOnly.id = "seeds_only";
		SeedsOnly.spawns.clear();
		REQUIRE_FALSE(SeedsOnly.corpseSeeds.empty());
		const std::vector<std::string> SeedIds = PreloadDefIds(Data(), SeedsOnly);
		for (const abyss::MapCorpseSeedDef& Seed : SeedsOnly.corpseSeeds)
		{
			CHECK(std::find(SeedIds.begin(), SeedIds.end(), Seed.monsterId) != SeedIds.end());
		}
		for (const char* Id : { "risen_footman", "deserter", "deserter_drummer", "gravegnawer", "valdras",
				 "miniboss_corpseherd", "hunt_dreck", "hunt_harke", "hunt_hungerbell" })
		{
			CHECK_MESSAGE(Set.count(Id) == 1, Id);
		}
		// every id resolves to a definition and its art
		for (const std::string& Id : Ids)
		{
			CAPTURE(Id);
			const abyss::MonsterDef* Def = FindMonsterDef(Data(), Id);
			REQUIRE(Def != nullptr);
			CHECK_FALSE(AssetByGameId(Def->spriteKey).IsNull());
		}
		// what Chapter 1's monsters stand up is covered (raise / summon targets, phase summons)
		for (const abyss::MonsterDef* Def : Chapter1Defs())
		{
			if (Set.count(Def->id) == 0)
			{
				continue;
			}
			for (const abyss::MonsterAbilityDef& A : Def->abilities)
			{
				if (A.summon.present)
				{
					CHECK(Set.count(A.summon.monsterId) == 1);
				}
			}
			for (const abyss::MonsterPhaseDef& P : Def->phases)
			{
				for (const abyss::PhaseSummonDef& S : P.summons)
				{
					CHECK(Set.count(S.monsterId) == 1);
				}
			}
		}
		for (const char* MapId : { "twilight_forest", "anvil_mountains", "scorching_desert", "abyss_rift" })
		{
			const abyss::MapDef* Map = Data().FindMap(MapId);
			REQUIRE(Map != nullptr);
			const std::vector<std::string> Zone = PreloadDefIds(Data(), *Map);
			for (const abyss::MapSpawnDef& Spawn : Map->spawns)
			{
				CHECK(std::find(Zone.begin(), Zone.end(), Spawn.monsterId) != Zone.end());   // today's preload
			}
		}
	}
}
