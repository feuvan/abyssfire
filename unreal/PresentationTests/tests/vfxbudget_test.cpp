// The particle budget of a zone with a presentation theme (2G cross-stage review X4; AbyssVfxBudgetRules.h,
// UAbyssVfxSystem::UpdateEmitters / SpawnFromEmitter):
// * the rules: outside a theme exactly today's budget; inside, loops stop at 75 % of the cap, spawn nothing beyond the
//   cull radius, and emit at most at the balanced density on mobile; the steady-state estimate on synthetic recipes;
// * the cull radius is beyond anything the zone camera can show (every zoom, a 21:9 / 2.4:1 screen, a loop's particles
//   up to 6 m tall, 3 m of drift);
// * the worst Chapter 1 scenes, built from the shipped data (maps.json camps / exits / spawns / regions, set_pieces.json,
//   monsters.json abilities / phases / auras / affixes, the art manifest's body loops, zone_moods.json weather,
//   vfx_recipes.json rates and lives, render_quality.json multipliers): every scene's steady loops fit under the loop
//   ceiling on every platform and tier, so one-shot bursts (hit sparks, contact dust, death bursts) always keep at least
//   a quarter of the cap. Without the themed budget the same scenes overflow the mobile cap's loop share (the finding).
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "doctest/doctest.h"

#include "abyss/base/Enums.h"
#include "abyss/base/Json.h"
#include "abyss/base/Types.h"
#include "abyss/data/CombatData.h"
#include "abyss/data/DataStore.h"
#include "abyss/data/MapData.h"
#include "abyss/data/MonsterData.h"
#include "abyss/data/SetPieceData.h"
#include "abyss/world/MapGen.h"
#include "abyss/world/SetPieces.h"

#include "Actors/AbyssCorpseRules.h"
#include "Actors/AbyssMonsterLookRules.h"
#include "Vfx/AbyssTelegraphRules.h"
#include "Vfx/AbyssVfxBudgetRules.h"
#include "World/AbyssThemeRules.h"
#include "audio_test_data.h"

namespace
{
	using namespace AbyssVfxBudgetRules;

	const abyss::JsonValue& LoadJson(const char* RelativePath)
	{
		static std::map<std::string, abyss::JsonValue, std::less<>> Cache;
		auto It = Cache.find(std::string_view(RelativePath));
		if (It == Cache.end())
		{
			abyss::JsonValue Root;
			const std::string Text = audio_test::ReadBytes(audio_test::UnrealDir() + "/" + std::string(RelativePath));
			if (Text.empty() || !abyss::ParseJson(Text, Root))
			{
				Root = abyss::JsonValue();
			}
			It = Cache.emplace(RelativePath, std::move(Root)).first;
		}
		return It->second;
	}

	const abyss::JsonValue& Recipes() { return LoadJson("Art/Export/VFX/vfx_recipes.json"); }

	const abyss::JsonValue& ArtManifest()
	{
		const abyss::JsonValue& Root = LoadJson("Data/assets.json");
		const abyss::JsonValue* Manifest = Root.Find("manifest");
		return Manifest != nullptr ? *Manifest : Root;
	}

	const abyss::DataStore& Data() { return audio_test::Data(); }

	/** The art asset a monster resolves to in a zone of Theme (`<spriteKey>@<theme>`, the spriteKey, the def id). */
	const abyss::JsonValue& ArtOf(const abyss::MonsterDef& Def, std::string_view Theme)
	{
		static const abyss::JsonValue Null;
		std::vector<std::string> Ids;
		if (!Theme.empty())
		{
			Ids.push_back(AbyssThemeRules::ThemedId(Def.spriteKey, Theme));
		}
		Ids.push_back(Def.spriteKey);
		Ids.push_back(Def.id);
		for (const std::string& Id : Ids)
		{
			for (const abyss::JsonMember& Member : ArtManifest().Get("assets").Members())
			{
				for (const abyss::JsonValue& GameId : Member.value.Get("gameIds").Items())
				{
					if (GameId.AsString() == Id)
					{
						return Member.value;
					}
				}
			}
		}
		return Null;
	}

	bool HasRecipe(std::string_view Id) { return FindRecipe(Recipes(), Id) != nullptr; }

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

	double Steady(std::string_view Id, double Multiplier)
	{
		const abyss::JsonValue* Recipe = Id.empty() ? nullptr : FindRecipe(Recipes(), Id);
		return Recipe != nullptr ? SteadyParticles(*Recipe, Multiplier) : 0.0;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Scenes
	// ---------------------------------------------------------------------------------------------------------------

	/** A loop that may run in a scene: at a tile (culled by its distance to the camera focus), or on the camera. */
	struct FLoop
	{
		std::string Recipe;
		abyss::Vec2 Tile;
		bool bOnCamera = false;
		int Count = 1;
		std::string Why;
	};

	struct FScene
	{
		std::string Label;
		abyss::Vec2 Centre;
		std::vector<FLoop> Loops;
	};

	/** Ground loot a fight leaves lying (beams loop until picked up): three rare beams, a margin for the drops. */
	constexpr int LootBeamAllowance = 3;

	std::string_view ThemeOfMap(const abyss::MapDef& Map) { return AbyssThemeRules::ThemeOf(&Map); }

	/** The zone loops UAbyssWorldBuilder::BuildZone plays: camp fires and torches, exits, set-piece socket loops. */
	std::vector<FLoop> ZoneLoops(const abyss::MapDef& Map)
	{
		std::vector<FLoop> Out;
		const std::string_view Theme = ThemeOfMap(Map);
		auto Chain = [Theme](const std::string& Id)
		{
			std::vector<std::string> Ids;
			if (!Theme.empty())
			{
				Ids.push_back(AbyssThemeRules::ThemedId(Id, Theme));
			}
			if (Theme.empty() || !AbyssThemeRules::ThemedOnlyRecipe(Id))
			{
				Ids.push_back(Id);
			}
			return FirstRecipe(Ids);
		};
		for (const abyss::CampProp& Prop : abyss::CampProps(Map))
		{
			if (!AbyssThemeRules::PlacesCampProp(Theme, Prop.campIndex, Prop.type)
				|| (Prop.type != "campfire" && Prop.type != "torch"))
			{
				continue;
			}
			Out.push_back({ Chain("camp." + Prop.type), Prop.centre, false, 1, "camp " + Prop.type });
		}
		for (const abyss::MapExitDef& Exit : Map.exits)
		{
			// Sealed or open over the chapter: the costlier of the two loops.
			const std::string Sealed = Chain("exit.sealed");
			const std::string Open = Chain("exit.portal");
			const std::string& Worse = Steady(Sealed, 1.0) >= Steady(Open, 1.0) ? Sealed : Open;
			Out.push_back({ Worse, abyss::Vec2(Exit.pos.col, Exit.pos.row), false, 1, "exit" });
		}
		for (const abyss::SetPieceZoneDef& Zone : Data().SetPieces().zones)
		{
			if (Zone.zone != Map.id)
			{
				continue;
			}
			std::vector<abyss::SetPiecePlacement> Placements;
			abyss::ExpandSetPieces(Zone, Map, Data(), abyss::SetPieceFrame{ -90.0, Data().World().constants.cameraYawDeg },
				Placements);
			for (const abyss::SetPiecePlacement& P : Placements)
			{
				for (const abyss::SetPieceEffectDef& Effect : P.def->effects)
				{
					Out.push_back({ Effect.recipe, P.pos, false, 1, "set piece " + P.def->id });   // played as named
				}
			}
		}
		return Out;
	}

	/** The weather loop at a tile (on the camera): the costlier of the night and the dawn ambience there. */
	FLoop WeatherAt(const abyss::MapDef& Map, abyss::Vec2 Tile)
	{
		FLoop Best;
		Best.bOnCamera = true;
		Best.Why = "weather";
		for (const bool bDawn : { false, true })
		{
			AbyssThemeRules::FMoodInputs In;
			In.Moods = &Data().World().moods;
			In.Map = &Map;
			In.ZoneId = Map.id;
			In.GeneratorTheme = Map.theme;
			In.bChapterComplete = bDawn;
			In.bHasHero = true;
			In.HeroCol = Tile.x;
			In.HeroRow = Tile.y;
			const std::string Id = AbyssThemeRules::AmbienceRecipeId(AbyssThemeRules::EffectiveMood(In));
			if (Steady(Id, 1.0) > Steady(Best.Recipe, 1.0))
			{
				Best.Recipe = Id;
			}
		}
		return Best;
	}

	/** Loop-carrying affixes an elite of this family may roll in the zone (at most the zone's affix count). */
	int AffixLoops(std::string_view Zone, const abyss::MonsterDef& Def)
	{
		const abyss::EliteAffixTable& Table = Data().Combat().eliteAffixes;
		int32_t Min = 0;
		int32_t Max = 0;
		Table.CountFor(Zone, Min, Max);
		int Loops = 0;
		for (const abyss::EliteAffixType Type : Table.order)
		{
			Loops += Table.RollableFor(Type, Def.family) && !AbyssMonsterLookRules::AffixLoopRecipe(Type).empty() ? 1 : 0;
		}
		return (std::min)(Loops, static_cast<int>(Max));
	}

	std::string AffixLoopId()
	{
		return std::string(AbyssMonsterLookRules::AffixLoopRecipe(abyss::EliteAffixType::Unburied));
	}

	/** What a living monster loops wherever it stands (UAbyssCorpseLayer::UpdateMonsterLoops): aura, body, affixes. */
	void AddResidentLoops(const abyss::MapDef& Map, const abyss::MonsterDef& Def, bool bElite, abyss::Vec2 Tile, int Count,
		std::vector<FLoop>& Out)
	{
		const abyss::JsonValue& Art = ArtOf(Def, ThemeOfMap(Map));
		if (AbyssMonsterLookRules::WantsAuraLoop(true, Def.aura.present ? Def.aura.radius : 0.0))
		{
			Out.push_back({ FirstRecipe(AbyssMonsterLookRules::AuraChain(Def.id, Def.spriteKey)), Tile, false, Count, Def.id + " aura" });
		}
		if (const std::string_view Body = AbyssMonsterLookRules::BodyLoopRecipe(Art); !Body.empty())
		{
			Out.push_back({ std::string(Body), Tile, false, Count, Def.id + " body" });
		}
		if (bElite)
		{
			Out.push_back({ AffixLoopId(), Tile, false, Count * AffixLoops(Map.id, Def), Def.id + " affix" });
		}
	}

	/**
	 * What a monster's fight adds: its last phase's loop, its arena ring, the hazards its cooldowns keep alive at once,
	 * the statuses it lays on the hero, and every body it (or its phases) stands up, with their own loops.
	 */
	void AddFightLoops(const abyss::MapDef& Map, const abyss::MonsterDef& Def, bool bElite, abyss::Vec2 Tile, int Count,
		std::vector<FLoop>& Out, std::set<abyss::StatusType>& Statuses, int Depth = 0)
	{
		AddResidentLoops(Map, Def, bElite, Tile, Count, Out);
		const abyss::JsonValue& Art = ArtOf(Def, ThemeOfMap(Map));
		std::string PhaseLoop;
		for (int Phase = 1; Phase <= static_cast<int>(Def.phases.size()); ++Phase)
		{
			const std::string Loop = AbyssMonsterLookRules::PhaseLookFor(Def.id, &Def, Phase, false, Art).Loop;
			PhaseLoop = Steady(Loop, 1.0) > Steady(PhaseLoop, 1.0) ? Loop : PhaseLoop;   // one phase loop at a time
		}
		if (!PhaseLoop.empty())
		{
			Out.push_back({ PhaseLoop, Tile, false, Count, Def.id + " phase" });
		}
		for (const abyss::StatusRule& Rule : Def.onHitStatus)
		{
			Statuses.insert(Rule.status);
		}
		for (const abyss::MonsterAbilityDef& A : Def.abilities)
		{
			if (A.status.present)
			{
				Statuses.insert(A.status.type);
			}
			if (A.hazard.present)
			{
				if (A.hazard.status.present)
				{
					Statuses.insert(A.hazard.status.type);
				}
				const int Alive = A.cooldownMs > 0.0 ? static_cast<int>(std::ceil(A.hazard.durationMs / A.cooldownMs)) : 1;
				const abyss::AbilityShapeKind Kind = A.hazard.hasShape ? A.hazard.shape.kind : A.shape.kind;
				Out.push_back({ FirstRecipe(AbyssTelegraphRules::HazardRecipeChain(A.id, Kind)), Tile, false,
					Count * (std::max)(1, Alive), Def.id + " hazard " + A.id });
			}
			if ((A.kind == abyss::AbilityKind::Summon || A.kind == abyss::AbilityKind::Raise) && Depth < 2)
			{
				if (const abyss::MonsterDef* Body = Data().FindMonster(A.summon.monsterId); Body != nullptr && Body->id != Def.id)
				{
					const int Alive = A.summon.capAlive > 0 ? A.summon.capAlive : A.summon.count;
					AddFightLoops(Map, *Body, false, Tile, Count * Alive, Out, Statuses, Depth + 1);
				}
			}
		}
		for (const abyss::MonsterPhaseDef& Phase : Def.phases)
		{
			if (Phase.arena.present)
			{
				Out.push_back({ std::string(AbyssTelegraphRules::ArenaRecipe), Tile, false, Count, Def.id + " arena" });
			}
			for (const abyss::PhaseSummonDef& S : Phase.summons)
			{
				if (const abyss::MonsterDef* Body = Data().FindMonster(S.monsterId); Body != nullptr && Depth < 2)
				{
					AddFightLoops(Map, *Body, false, Tile, Count * S.count, Out, Statuses, Depth + 1);
				}
			}
		}
	}

	struct FGroup
	{
		std::string Label;
		bool bHasTile = false;
		abyss::Vec2 Tile;
		std::vector<std::pair<const abyss::MonsterDef*, std::pair<bool, int>>> Members;   // def, (elite, count)
	};

	void AddMember(FGroup& G, const std::string& Id, bool bElite, int Count)
	{
		if (const abyss::MonsterDef* Def = Data().FindMonster(Id); Def != nullptr && Count > 0)
		{
			G.Members.push_back({ Def, { bElite || Def->elite || Def->isMiniBoss || !Def->phases.empty(), Count } });
		}
	}

	/** Every group a themed map fields: spawn entries (and their dawn variants), hunts, the champion ambush, the mini-boss. */
	std::vector<FGroup> Groups(const abyss::MapDef& Map)
	{
		std::vector<FGroup> Out;
		for (const abyss::MapSpawnDef& Spawn : Map.spawns)
		{
			FGroup G;
			G.Label = Map.id + ":" + Spawn.monsterId;
			G.bHasTile = true;
			G.Tile = abyss::Vec2(Spawn.pos.col, Spawn.pos.row);
			if (Spawn.mix.empty())
			{
				AddMember(G, Spawn.monsterId, false, Spawn.count);
			}
			bool bLeaderTaken = false;
			for (const abyss::MapSpawnMixDef& Mix : Spawn.mix)
			{
				int Count = Mix.count;
				if (Spawn.leader.present && Spawn.leader.elite && !bLeaderTaken && Mix.monsterId == Spawn.leader.monsterId)
				{
					AddMember(G, Mix.monsterId, true, 1);
					bLeaderTaken = true;
					--Count;
				}
				AddMember(G, Mix.monsterId, false, Count);
			}
			Out.push_back(G);
			for (const abyss::MapSpawnVariantDef& Variant : Spawn.variants)
			{
				FGroup V = G;
				V.Label = G.Label + "@" + Variant.whenFlag;
				V.Members.clear();
				AddMember(V, Variant.monsterId, false, Variant.count);
				Out.push_back(V);
			}
		}
		for (const abyss::HuntDef& Hunt : Data().Monsters().hunts)
		{
			if (Hunt.zone != Map.id)
			{
				continue;
			}
			FGroup G;
			G.Label = Hunt.huntId;
			G.bHasTile = true;
			G.Tile = abyss::Vec2(Hunt.spawn.col, Hunt.spawn.row);
			G.Members.push_back({ &Hunt.defNormal, { true, 1 } });
			if (Hunt.hasMinions)
			{
				AddMember(G, Hunt.minionMonsterId, false, Hunt.minionCount);
			}
			Out.push_back(G);
		}
		if (const abyss::ZoneEventDataDef* Events = Data().World().randomEvents.ForZone(Map.id); Events != nullptr
			&& Events->ambushChampion.present)
		{
			FGroup G;
			G.Label = Map.id + ":ambushChampion";   // anywhere in the field: evaluated at every spawn tile
			for (const abyss::MapSpawnMixDef& Mix : Events->ambushChampion.mix)
			{
				AddMember(G, Mix.monsterId, Events->ambushChampion.sharedAffix || Mix.monsterId == Events->ambushChampion.leader,
					Mix.count);
			}
			Out.push_back(G);
		}
		if (const abyss::MiniBossEntry* Mini = Data().Monsters().MiniBossFor(Map.id); Mini != nullptr && Mini->hasSpawn)
		{
			FGroup G;
			G.Label = Map.id + ":" + Mini->monsterId;
			G.bHasTile = true;
			G.Tile = abyss::Vec2(Mini->spawn.col, Mini->spawn.row);
			AddMember(G, Mini->monsterId, true, 1);
			Out.push_back(G);
		}
		return Out;
	}

	/** One fight scene: the weather there, every zone loop and resident monster loop around, the group's fight loops. */
	FScene SceneOf(const abyss::MapDef& Map, const FGroup& Fight, abyss::Vec2 Centre, const std::vector<FGroup>& All)
	{
		FScene S;
		S.Label = Fight.Label;
		S.Centre = Centre;
		S.Loops.push_back(WeatherAt(Map, Centre));
		for (const FLoop& L : ZoneLoops(Map))
		{
			S.Loops.push_back(L);
		}
		for (const FGroup& Other : All)
		{
			if (&Other == &Fight || !Other.bHasTile)
			{
				continue;   // the fight's own group is counted below; a tile-less group (the ambush) is the fight or absent
			}
			for (const auto& [Def, EliteCount] : Other.Members)
			{
				AddResidentLoops(Map, *Def, EliteCount.first, Other.Tile, EliteCount.second, S.Loops);
			}
		}
		std::set<abyss::StatusType> Statuses{ abyss::StatusType::Ashnumb };   // the hero may carry stacks into any fight
		for (const auto& [Def, EliteCount] : Fight.Members)
		{
			AddFightLoops(Map, *Def, EliteCount.first, Centre, EliteCount.second, S.Loops, Statuses);
		}
		for (const abyss::StatusType Status : Statuses)
		{
			S.Loops.push_back({ "status." + std::string(abyss::EnumName(Status)), Centre, false, 1, "hero status" });
		}
		S.Loops.push_back({ "loot.beam.rare", Centre, false, LootBeamAllowance, "loot" });
		return S;
	}

	/** Live loop particles of a scene at this multiplier, the camera on its centre, loops beyond CullRadiusCm culled. */
	double SceneParticles(const FScene& S, double Multiplier, double CullRadiusCm)
	{
		double Total = 0.0;
		for (const FLoop& L : S.Loops)
		{
			const double DistCm = std::hypot(L.Tile.x - S.Centre.x, L.Tile.y - S.Centre.y) * 100.0;
			if (!L.bOnCamera && !LoopInRange(CullRadiusCm, DistCm * DistCm))
			{
				continue;
			}
			Total += L.Count * Steady(L.Recipe, Multiplier);
		}
		return Total;
	}

	/** Every fight scene of every themed map (a tile-less group at every spawn tile of its map). */
	std::vector<FScene> ThemedScenes()
	{
		std::vector<FScene> Out;
		for (const abyss::MapDef& Map : Data().World().maps)
		{
			if (ThemeOfMap(Map).empty())
			{
				continue;
			}
			const std::vector<FGroup> All = Groups(Map);
			for (const FGroup& G : All)
			{
				if (G.bHasTile)
				{
					Out.push_back(SceneOf(Map, G, G.Tile, All));
					continue;
				}
				for (const abyss::MapSpawnDef& Spawn : Map.spawns)
				{
					FScene S = SceneOf(Map, G, abyss::Vec2(Spawn.pos.col, Spawn.pos.row), All);
					S.Label += " near " + Spawn.monsterId;
					Out.push_back(std::move(S));
				}
			}
		}
		return Out;
	}

	struct FTier
	{
		const char* Name;
		bool bMobile;
		int Tier;
	};

	double ProfileMultiplier(int Tier)
	{
		return Data().RenderQualityProfiles().profiles[static_cast<size_t>(Tier)].particleFrequencyMultiplier;
	}
}

TEST_SUITE("vfxbudget")
{
	TEST_CASE("outside a presentation theme the budget is exactly today's")
	{
		for (const bool bMobile : { false, true })
		{
			for (const double Profile : { 0.1, 1.0, 1.5, 2.0 })
			{
				CAPTURE(bMobile);
				CAPTURE(Profile);
				const FLoopBudget B = LoopBudgetFor(false, bMobile, Profile);
				CHECK_FALSE(B.bThemed);
				CHECK(B.MaxParticles == (bMobile ? 450 : 700));
				CHECK(B.LoopCeiling == B.MaxParticles);
				CHECK(B.CullRadiusCm == 0.0);
				// UpdateEmitters' FMath::Max(0.25f, multiplier): the same float once narrowed
				CHECK(static_cast<float>(B.FrequencyMultiplier) == (std::max)(0.25f, static_cast<float>(Profile)));
				CHECK(SpawnLimit(B, true) == B.MaxParticles);
				CHECK(SpawnLimit(B, false) == B.MaxParticles);
				CHECK(LoopInRange(B, 1e18));
			}
		}
	}

	TEST_CASE("a themed zone: a loop ceiling at 75 % of the cap, a 30 m cull radius, the balanced density floor on mobile")
	{
		const FLoopBudget Desk = LoopBudgetFor(true, false, 1.0);
		CHECK(Desk.LoopCeiling == 525);
		CHECK(Desk.MaxParticles - Desk.LoopCeiling >= Desk.MaxParticles / 4);
		CHECK(Desk.CullRadiusCm == 3000.0);
		CHECK(Desk.FrequencyMultiplier == 1.0);   // desktop keeps the tier's density
		CHECK(LoopBudgetFor(true, false, 2.0).FrequencyMultiplier == 2.0);
		const FLoopBudget Mob = LoopBudgetFor(true, true, 1.0);
		CHECK(Mob.LoopCeiling == 337);
		CHECK(Mob.MaxParticles - Mob.LoopCeiling >= Mob.MaxParticles / 4);
		CHECK(Mob.FrequencyMultiplier == 1.5);   // mobile high emits at the balanced density
		CHECK(LoopBudgetFor(true, true, 1.5).FrequencyMultiplier == 1.5);
		CHECK(LoopBudgetFor(true, true, 2.0).FrequencyMultiplier == 2.0);   // mobile low keeps its own
		CHECK(SpawnLimit(Mob, true) == 337);
		CHECK(SpawnLimit(Mob, false) == 450);
		CHECK(LoopInRange(Mob, 3000.0 * 3000.0));
		CHECK_FALSE(LoopInRange(Mob, 3000.0 * 3000.0 + 1.0));
		CHECK(LoopInRange(0.0, 1e18));
	}

	TEST_CASE("the steady-state estimate: rate / multiplier x mean life, loops only")
	{
		const abyss::JsonValue Recipe = audio_test::Json(R"({"layers": [
			{"rate": 10, "lifeMs": [400, 600]},
			{"rate": 4, "lifeMs": 1000, "durationMs": -1},
			{"rate": 50, "lifeMs": 1000, "durationMs": 800},
			{"count": 30, "lifeMs": 2000},
			{"emitter": "motes", "rate": 2},
			{"rate": 1000, "lifeMs": 100},
			{"emitter": "nonesuch", "rate": 1}
		]})");
		// 10 x .5 + 4 x 1 + motes 2 x .55 + the clamp 400 x .1 + an unknown life 1 x 1; the finite and burst layers none
		const double Expected = 5.0 + 4.0 + 1.1 + 40.0 + 1.0;
		CHECK(SteadyParticles(Recipe, 1.0) == doctest::Approx(Expected));
		CHECK(SteadyParticles(Recipe, 2.0) == doctest::Approx(Expected / 2.0));
		CHECK(SteadyParticles(Recipe, 0.1) == doctest::Approx(Expected / 0.25));   // UpdateEmitters' .25 floor
		CHECK(PresetMeanLifeMs("flames") == doctest::Approx(410.0));
		CHECK(PresetMeanLifeMs("nonesuch") < 0.0);
		// the shipped loops: every steady layer names its life or a known preset (no 1 s guess anywhere)
		size_t Steady = 0;
		for (const abyss::JsonMember& Member : Recipes().Get("recipes").Members())
		{
			for (const abyss::JsonValue& Layer : Member.value.Get("layers").Items())
			{
				if (!IsSteadyLayer(Layer))
				{
					continue;
				}
				CAPTURE(Member.key);
				double LifeMs = 0.0;
				CHECK(LayerMeanLifeMs(Layer, LifeMs));
				++Steady;
			}
		}
		CHECK(Steady > 200);
		// the finding's numbers: Valdras's banner ring alone keeps ~200 alive at the high multiplier
		CHECK(::Steady("hazard.banner_ring", 1.0) == doctest::Approx(203.2).epsilon(0.01));
		CHECK(::Steady("ambient.ashfall", 1.0) > 250.0);
	}

	TEST_CASE("the cull radius lies beyond anything the zone camera shows")
	{
		// geometry sanity: straight down the far corner is the screen's half diagonal; a horizon in view has no bound
		CHECK(FarthestVisibleCm(90.0, -90.0, 1000.0, 16.0 / 9.0, 0.0)
			== doctest::Approx(std::hypot(1000.0 * std::tan(Pi / 4.0), 1000.0 * std::tan(Pi / 4.0) / (16.0 / 9.0))));
		CHECK(std::isinf(FarthestVisibleCm(35.0, -5.0, 2000.0, 16.0 / 9.0, 0.0)));
		CHECK(FarthestVisibleCm(35.0, -50.0, 2000.0, 16.0 / 9.0, 0.0) < FarthestVisibleCm(35.0, -50.0, 2500.0, 16.0 / 9.0, 0.0));
		CHECK(FarthestVisibleCm(35.0, -50.0, 2000.0, 16.0 / 9.0, 0.0) < FarthestVisibleCm(35.0, -50.0, 2000.0, 2.4, 0.0));
		// a tall loop near the camera pokes up from the bottom edge farther from the focus than any ground point shows
		CHECK(FarthestVisibleCm(35.0, -50.0, 2000.0, 16.0 / 9.0, 1300.0) > FarthestVisibleCm(35.0, -50.0, 2000.0, 16.0 / 9.0, 0.0));
		// the shipped framing (world_constants.json camera; AAbyssCameraRig: framingTiles across the 16:9 FOV), at the
		// farthest zoom with a 10 % pulse, a 2.4:1 screen, a loop 6 m tall (Valdras's standard fire tops out near 3.7 m)
		const abyss::WorldConstants& C = Data().World().constants;
		const double Distance = C.cameraFramingTilesW * 100.0 * 0.5 / std::tan(0.5 * C.cameraFovDeg * Pi / 180.0);
		CHECK(Distance == doctest::Approx(2125.0).epsilon(0.002));
		const double Farthest = FarthestVisibleCm(C.cameraFovDeg, C.cameraPitchDeg, Distance * C.cameraZoomMax * 1.1, 2.4, 600.0);
		constexpr double DriftCm = 300.0;   // a loop's particles drift from their spawn point (ash, smoke, embers)
		MESSAGE("farthest visible " << Farthest << " cm + drift " << DriftCm << " vs cull radius " << ThemedLoopCullRadiusCm);
		CHECK(Farthest + DriftCm < ThemedLoopCullRadiusCm);
		CHECK(Farthest == doctest::Approx(1663.3).epsilon(0.01));
		CHECK(Farthest > 1000.0);   // not a degenerate camera
	}

	TEST_CASE("the worst Chapter 1 scenes' loops fit under the loop ceiling on every platform and tier")
	{
		const std::vector<FScene> Scenes = ThemedScenes();
		REQUIRE(Scenes.size() >= 10);
		bool bValdras = false;
		for (const FScene& S : Scenes)
		{
			bValdras = bValdras || S.Label.find("valdras") != std::string::npos;
			for (const FLoop& L : S.Loops)
			{
				CAPTURE(S.Label);
				CAPTURE(L.Why);
				// every loop the scene names exists (the status loops of statuses without a look excepted)
				CHECK((L.Recipe.empty() || HasRecipe(L.Recipe) || L.Why == "hero status" || L.Why == "weather"));
			}
		}
		CHECK(bValdras);
		const FTier Tiers[] = { { "desktop low", false, 0 }, { "desktop balanced", false, 1 }, { "desktop high", false, 2 },
			{ "mobile low", true, 0 }, { "mobile balanced", true, 1 }, { "mobile high", true, 2 } };
		double OldWorstMobileHigh = 0.0;
		for (const FTier& T : Tiers)
		{
			CAPTURE(T.Name);
			const FLoopBudget Themed = LoopBudgetFor(true, T.bMobile, ProfileMultiplier(T.Tier));
			const FLoopBudget Old = LoopBudgetFor(false, T.bMobile, ProfileMultiplier(T.Tier));
			double Worst = 0.0;
			double OldWorst = 0.0;
			std::string WorstLabel;
			for (const FScene& S : Scenes)
			{
				const double Now = SceneParticles(S, Themed.FrequencyMultiplier, Themed.CullRadiusCm);
				if (Now > Worst)
				{
					Worst = Now;
					WorstLabel = S.Label;
				}
				OldWorst = (std::max)(OldWorst, SceneParticles(S, Old.FrequencyMultiplier, Old.CullRadiusCm));
			}
			MESSAGE(std::string(T.Name) << ": worst scene " << WorstLabel << " " << static_cast<int>(Worst) << " loop particles, ceiling "
				<< Themed.LoopCeiling << " of " << Themed.MaxParticles << " (before the themed budget: "
				<< static_cast<int>(OldWorst) << ")");
			CHECK(Worst <= Themed.LoopCeiling);
			CHECK(Themed.MaxParticles - Worst >= 0.25 * Themed.MaxParticles);   // the bursts' quarter is never needed by loops
			if (T.bMobile && T.Tier == 2)
			{
				OldWorstMobileHigh = OldWorst;
			}
		}
		// the finding: without the themed budget the loops alone took more than the mobile cap's loop share
		CHECK(OldWorstMobileHigh > LoopBudgetFor(true, true, 1.0).LoopCeiling);
	}

	TEST_CASE("husks cast the sun's CSM on desktop only (mobile: the blob alone)")
	{
		CHECK(AbyssCorpseRules::HuskCastsSunShadow(false));
		CHECK_FALSE(AbyssCorpseRules::HuskCastsSunShadow(true));
	}

	TEST_CASE("Valdras's last phase: the culled scene drops the far camps, the arena ring is the largest loop")
	{
		const std::vector<FScene> Scenes = ThemedScenes();
		const FScene* Boss = nullptr;
		for (const FScene& S : Scenes)
		{
			Boss = S.Label.find(":valdras") != std::string::npos ? &S : Boss;
		}
		REQUIRE(Boss != nullptr);
		double Arena = 0.0;
		double Largest = 0.0;
		int CampLoopsCulled = 0;
		for (const FLoop& L : Boss->Loops)
		{
			const double Each = L.Count * Steady(L.Recipe, 1.0);
			Largest = (std::max)(Largest, Each);
			Arena = L.Why.find("arena") != std::string::npos ? Each : Arena;
			const double DistCm = std::hypot(L.Tile.x - Boss->Centre.x, L.Tile.y - Boss->Centre.y) * 100.0;
			CampLoopsCulled += L.Why.rfind("camp", 0) == 0 && !LoopInRange(ThemedLoopCullRadiusCm, DistCm * DistCm) ? 1 : 0;
		}
		CHECK(Arena == Largest);
		CHECK(Arena > 150.0);
		CHECK(CampLoopsCulled >= 16);   // both camps' fires and torches lie 80 tiles away
		const double Culled = SceneParticles(*Boss, 1.0, ThemedLoopCullRadiusCm);
		const double Unculled = SceneParticles(*Boss, 1.0, 0.0);
		MESSAGE("Valdras P3 at the high multiplier: " << static_cast<int>(Culled) << " culled, " << static_cast<int>(Unculled)
			<< " with every zone loop");
		CHECK(Unculled - Culled > 100.0);
	}
}
