// The 400 ms readability rule (monsters.md 1.3; 2G stage H, AbyssTelegraphRules::ReadableWindowMs), driven through the
// stage T telegraph rules (2G review H-m2) rather than the core's floors alone:
// * every ground strike (hazard windups included) of every Chapter 1 definition and hunt, timed as the core times it
//   (ComputeAbilityTiming on the caster's real clip and the style's floor), and every on-death fuse that draws a decal,
//   is played through the real M_AF_Telegraph contract on the real sim clock (abyss::SimClock: the UE driver's frames,
//   RenderSimMs = simNow - (1 - alpha) x step) at 30, 45 and 60 fps: the first frame StrikeState / PackCpd draw it (rim
//   and fill above 0) to the contact flash is at least 400 ms, the decal never drops out in between, and
//   ReadableWindowMs is a lower bound of that;
// * the budget: the worst scene the 2G review named (Valdras's last phase - arena, a windup with the previous strike
//   still fading, every hazard his cooldowns can keep alive, his phase summons and theirs - plus the two Chapter 1 packs
//   that can hold the most decals at once) stays inside the lowest cap, so SelectVisible drops nothing;
// * a real Chapter 1 playthrough (the AutoPilot, warrior / mage / rogue, at 30 fps): every EvTelegraph / EvHazard /
//   EvArena / cancel goes through a model of UAbyssTelegraphSystem::Tick built from the same rules (TelegraphLook, late
//   views, CancelTimeFor, StrikeState / ZoneState, RankOf, SelectVisible at the lowest cap): no decal is dropped, and
//   every strike and fuse that reached its contact was on screen at least 400 ms before it.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "doctest/doctest.h"

#include "abyss/base/Enums.h"
#include "abyss/base/Json.h"
#include "abyss/base/Math.h"
#include "abyss/base/SimClock.h"
#include "abyss/base/Units.h"
#include "abyss/combat/HitFeedback.h"
#include "abyss/data/DataStore.h"
#include "abyss/monsters/MonsterAbilities.h"
#include "abyss/monsters/OnDeath.h"
#include "abyss/monsters/Phases.h"
#include "abyss/sim/GameSim.h"
#include "abyss/sim/SimTypes.h"
#include "abyss/sim/Snapshot.h"
#include "abyss/tools/AutoPilot.h"

#include "Vfx/AbyssTelegraphRules.h"
#include "World/AbyssThemeRules.h"
#include "audio_test_data.h"

namespace
{
	using namespace AbyssTelegraphRules;

	/** The M_AF_Telegraph contract as shipped (Data/assets.json `manifest.materials`, what UAbyssTelegraphSystem parses). */
	const FContract& RealContract()
	{
		static FContract Contract;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			abyss::JsonValue Root;
			const std::string Text = audio_test::ReadBytes(audio_test::UnrealDir() + "/Data/assets.json");
			if (!Text.empty() && abyss::ParseJson(Text, Root))
			{
				const abyss::JsonValue* Manifest = Root.Find("manifest");
				Contract = ParseContract((Manifest != nullptr ? *Manifest : Root).Get("materials"));
			}
		}
		return Contract;
	}

	struct FCaster
	{
		std::string Id;
		const abyss::MonsterDef* Def = nullptr;
		abyss::TelegraphStyle Style = abyss::TelegraphStyle::Normal;
	};

	std::vector<FCaster> Casters()
	{
		const abyss::DataStore& Data = audio_test::Data();
		std::vector<FCaster> Out;
		for (const abyss::MonsterDef& Def : Data.Monsters().defs)
		{
			if (Def.abilities.empty())
			{
				continue;
			}
			const bool bBoss = Def.id == "valdras" || Def.id == "miniboss_corpseherd";
			Out.push_back({ Def.id, &Def, bBoss ? abyss::TelegraphStyle::Boss : abyss::TelegraphStyle::Normal });
			if (!bBoss)
			{
				// The same definition as an elite (its style, its floor): the window must hold for both.
				Out.push_back({ Def.id + "@elite", &Def, abyss::TelegraphStyle::Elite });
			}
		}
		for (const abyss::HuntDef& Hunt : Data.Monsters().hunts)
		{
			// The hunt's definition: its base's abilities, then its own (MakeHuntDefinition; Hungerbell's slam and bell).
			if (!Hunt.defNormal.abilities.empty())
			{
				Out.push_back({ Hunt.huntId, &Hunt.defNormal, abyss::TelegraphStyle::Elite });
			}
		}
		return Out;
	}

	/**
	 * Whether the decal reads at full windup strength this frame (what PackCpd sends): a quad, the rim at least at the
	 * style's alpha x the urgency pulse's trough, and the fill at least at its start alpha where the style sweeps one. A
	 * fade-in or a decal held back fails it.
	 */
	bool Drawable(const FContract& C, const FGeometry& Geo, const FStyleLook& Look, const FState& S)
	{
		if (!S.bVisible || !(Geo.SizeXCm > 0.0) || !(Geo.SizeYCm > 0.0))
		{
			return false;
		}
		const std::array<float, Cpd::Count> Data = PackCpd(Geo, Look, S, true);
		const double RimFloor = Look.RimAlpha * (C.Urgency.Base - std::fabs(C.Urgency.Amp));
		return RimFloor > 0.0 && Data[Cpd::RimAlpha] + 1e-6 >= RimFloor
			&& (!Look.bFill || Data[Cpd::FillAlpha] + 1e-6 >= Look.FillAlphaStart);
	}

	/** One strike's on-screen reading at one frame rate (render clock ms). */
	struct FStrikeReading
	{
		double FirstMs = -1.0;     // the first frame the decal is drawn
		double ContactMs = -1.0;   // the contact frame (the flash)
		bool bGap = false;         // a frame between the two without a drawable windup
		double WindowMs() const { return ContactMs - FirstMs; }
	};

	/**
	 * A strike whose event the core emits in the step that reaches StartMs, rendered at FrameMs per frame on the real sim
	 * clock (abyss::SimClock, as GameSim::Frame drives it; the first frame's accumulator at AccPhaseMs): the decal is
	 * created after the frame that ran that step (UE dispatches the frame's events after its steps) and drawn from the
	 * first frame StrikeState shows it at the render clock (FAbyssFrameInfo::RenderSimMs).
	 */
	FStrikeReading ReadStrike(const FContract& C, EStyle Style, const FGeometry& Geo, double StartMs, double ContactMs,
		double FrameMs, double AccPhaseMs)
	{
		abyss::SimClock Clock;
		Clock.SetMaxStepsPerFrame(8);
		Clock.AccumulateRealTime(AccPhaseMs);
		Clock.EndFrame();
		const FStyleLook& Look = C.Style(Style);
		FStrikeReading R;
		bool bCreated = false;
		for (int Frame = 0; Frame < 100000; ++Frame)
		{
			Clock.AccumulateRealTime(FrameMs);
			while (Clock.ConsumeStep())
			{
				Clock.AdvanceStep();
			}
			Clock.EndFrame();
			const double SimNow = Clock.NowMs();
			bCreated = bCreated || SimNow + 1e-9 >= StartMs;
			if (!bCreated)
			{
				continue;
			}
			const double Now = SimNow - (1.0 - Clock.Accumulator() / abyss::kSimStepMs) * abyss::kSimStepMs;
			const FState S = StrikeState(C, Style, Now, StartMs, ContactMs);
			if (S.Phase == EPhase::Flash)
			{
				R.ContactMs = Now;
				break;
			}
			if (R.FirstMs < 0.0)
			{
				if (Drawable(C, Geo, Look, S))
				{
					R.FirstMs = Now;
				}
			}
			else if (S.Phase != EPhase::Windup || !Drawable(C, Geo, Look, S))
			{
				R.bGap = true;
			}
		}
		return R;
	}

	inline constexpr double FrameRates[] = { 30.0, 45.0, 60.0 };

	/**
	 * The tightest on-screen window of a strike of `Shape` / `Style` lasting `WindupMs` over the frame rates, the event's
	 * step within the frame and the accumulator's phase; CHECKs no gap, a drawn first frame and a contact frame, and that
	 * ReadableWindowMs never overstates it.
	 */
	double TightestWindow(const abyss::AbilityShapeDef& Shape, EStyle Style, double WindupMs)
	{
		const FContract& C = RealContract();
		const FGeometry Geo = MakeGeometry(Shape, abyss::Vec2(10.0, 10.0), abyss::Vec2(1.0, 0.0),
			OutsideReachCm(C.Style(Style), true));
		CHECK(Geo.SizeXCm > 0.0);
		CHECK(Geo.SizeYCm > 0.0);
		double Tightest = 1e9;
		for (const double Fps : FrameRates)
		{
			for (int Step = 0; Step < 4; ++Step)
			{
				for (const double Phase : { 0.0, 0.3, 0.6, 0.9 })
				{
					const double StartMs = static_cast<double>(600 + Step) * 1000.0 / 60.0;
					const FStrikeReading R = ReadStrike(C, Style, Geo, StartMs, StartMs + WindupMs, 1000.0 / Fps,
						Phase * abyss::kSimStepMs);
					CAPTURE(Fps);
					CAPTURE(Step);
					CAPTURE(Phase);
					CHECK(R.FirstMs >= StartMs);
					CHECK(R.ContactMs >= StartMs + WindupMs);
					CHECK_FALSE(R.bGap);
					CHECK(R.WindowMs() + 1e-6 >= ReadableWindowMs(StartMs, StartMs + WindupMs));
					Tightest = (std::min)(Tightest, R.WindowMs());
				}
			}
		}
		return Tightest;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The budget's worst scene (2G review H-m2)
	// ---------------------------------------------------------------------------------------------------------------

	bool DamagingFuse(const abyss::MonsterOnDeathDef& D)
	{
		// What draws a decal (CombatSystem emits the fuse's EvTelegraph; TelegraphLook keeps the damaging ones).
		return D.present && D.kind != abyss::OnDeathKind::Spawn && D.delayMs > 0.0 && (D.hasDmg || D.hasHpPct);
	}

	bool CastsGroundStrikes(const abyss::MonsterDef& Def)
	{
		for (const abyss::MonsterAbilityDef& A : Def.abilities)
		{
			if (abyss::AbilityHasGroundShape(A.kind))
			{
				return true;
			}
		}
		return false;
	}

	/** Damaging affix fuses an elite of `Family` may carry in `Zone` (at most the zone's affix count). */
	int AffixFuses(const abyss::DataStore& Data, std::string_view Zone, std::string_view Family)
	{
		const abyss::EliteAffixTable& Table = Data.Combat().eliteAffixes;
		int32_t Min = 0;
		int32_t Max = 0;
		Table.CountFor(Zone, Min, Max);
		int Fuses = 0;
		for (const abyss::EliteAffixType Type : Table.order)
		{
			if (Table.RollableFor(Type, Family) && DamagingFuse(Table.Def(Type).onDeath))
			{
				++Fuses;
			}
		}
		return (std::min)(Fuses, static_cast<int>(Max));
	}

	/**
	 * Strike / fuse decals one monster can hold at once: a ground-strike caster 2 (a windup and its previous strike still
	 * flashing / fading: back to back), its own damaging fuse 1, an elite's damaging affix fuses.
	 */
	struct FPotential
	{
		int Strikes = 0;
		int Fuses = 0;
		int Total() const { return Strikes + Fuses; }
	};

	FPotential PotentialOf(const abyss::DataStore& Data, std::string_view Zone, const abyss::MonsterDef& Def, bool bElite)
	{
		FPotential P;
		P.Strikes = CastsGroundStrikes(Def) ? 2 : 0;
		P.Fuses = (DamagingFuse(Def.onDeath) ? 1 : 0) + (bElite || Def.elite ? AffixFuses(Data, Zone, Def.family) : 0);
		return P;
	}

	/** The bodies a monster's summon / raise abilities keep alive at once (capAlive, else count), with their potentials. */
	FPotential SummonedPotential(const abyss::DataStore& Data, std::string_view Zone, const abyss::MonsterDef& Def)
	{
		FPotential P;
		for (const abyss::MonsterAbilityDef& A : Def.abilities)
		{
			if (A.kind != abyss::AbilityKind::Summon && A.kind != abyss::AbilityKind::Raise)
			{
				continue;
			}
			const abyss::MonsterDef* Body = Data.FindMonster(A.summon.monsterId);
			if (Body == nullptr)
			{
				continue;
			}
			const int Alive = A.summon.capAlive > 0 ? A.summon.capAlive : A.summon.count;
			const FPotential One = PotentialOf(Data, Zone, *Body, false);   // raised / summoned bodies roll no affixes
			P.Strikes += Alive * One.Strikes;
			P.Fuses += Alive * One.Fuses;
		}
		return P;
	}

	struct FGroup
	{
		std::string Label;
		FPotential P;
	};

	void AddMember(const abyss::DataStore& Data, std::string_view Zone, const std::string& MonsterId, int Count, bool bElite,
		FGroup& G)
	{
		const abyss::MonsterDef* Def = Data.FindMonster(MonsterId);
		if (Def == nullptr || Count <= 0)
		{
			return;
		}
		const FPotential One = PotentialOf(Data, Zone, *Def, bElite);
		const FPotential Summoned = SummonedPotential(Data, Zone, *Def);
		G.P.Strikes += Count * (One.Strikes + Summoned.Strikes);
		G.P.Fuses += Count * (One.Fuses + Summoned.Fuses);
	}

	/** Every pack the themed zones can field (spawn entries and their dawn variants, hunts, the champion ambush, the mini-boss). */
	std::vector<FGroup> ThemedPacks(const abyss::DataStore& Data, std::string_view SkipMonster)
	{
		std::vector<FGroup> Out;
		for (const abyss::MapDef& Map : Data.World().maps)
		{
			if (!Map.hasPresentationTheme)
			{
				continue;
			}
			for (const abyss::MapSpawnDef& Spawn : Map.spawns)
			{
				if (Spawn.monsterId == SkipMonster)
				{
					continue;   // the boss's own entry: its scene is counted on its own
				}
				FGroup G;
				G.Label = Map.id + ":" + Spawn.monsterId;
				if (Spawn.mix.empty())
				{
					AddMember(Data, Map.id, Spawn.monsterId, Spawn.count, false, G);
				}
				bool bLeaderTaken = false;
				for (const abyss::MapSpawnMixDef& Mix : Spawn.mix)
				{
					int Count = Mix.count;
					if (Spawn.leader.present && Spawn.leader.elite && !bLeaderTaken && Mix.monsterId == Spawn.leader.monsterId)
					{
						AddMember(Data, Map.id, Mix.monsterId, 1, true, G);   // the elite leader rolls the zone's affixes
						bLeaderTaken = true;
						--Count;
					}
					AddMember(Data, Map.id, Mix.monsterId, Count, false, G);
				}
				Out.push_back(G);
				for (const abyss::MapSpawnVariantDef& Variant : Spawn.variants)
				{
					FGroup V;
					V.Label = G.Label + "@" + Variant.whenFlag;
					AddMember(Data, Map.id, Variant.monsterId, Variant.count, false, V);
					Out.push_back(V);
				}
			}
			for (const abyss::HuntDef& Hunt : Data.Monsters().hunts)
			{
				if (Hunt.zone != Map.id)
				{
					continue;
				}
				FGroup G;
				G.Label = Hunt.huntId;
				const FPotential Leader = PotentialOf(Data, Map.id, Hunt.defNormal, true);
				G.P.Strikes += Leader.Strikes;
				G.P.Fuses += Leader.Fuses;
				if (Hunt.hasMinions)
				{
					AddMember(Data, Map.id, Hunt.minionMonsterId, Hunt.minionCount, false, G);
				}
				Out.push_back(G);
			}
			if (const abyss::ZoneEventDataDef* Events = Data.World().randomEvents.ForZone(Map.id))
			{
				if (Events->ambushChampion.present)
				{
					FGroup G;
					G.Label = Map.id + ":ambushChampion";
					for (const abyss::MapSpawnMixDef& Mix : Events->ambushChampion.mix)
					{
						const bool bElite = Events->ambushChampion.sharedAffix || Mix.monsterId == Events->ambushChampion.leader;
						AddMember(Data, Map.id, Mix.monsterId, Mix.count, bElite, G);
					}
					Out.push_back(G);
				}
			}
			if (const abyss::MiniBossEntry* Mini = Data.Monsters().MiniBossFor(Map.id))
			{
				FGroup G;
				G.Label = Map.id + ":" + Mini->monsterId;
				AddMember(Data, Map.id, Mini->monsterId, 1, true, G);   // the mini-boss keeps its pinned roll
				Out.push_back(G);
			}
		}
		return Out;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// A model of UAbyssTelegraphSystem (Private/Vfx/AbyssTelegraphs.cpp) built from the stage T rules: the event
	// handlers (HandleTelegraph / HandleHazard / HandleArena / HandleAttackCancelled), Tick's late views, timelines and
	// budget, and the world builder's clear on a zone change. It draws nothing; it records what each decal showed.
	// ---------------------------------------------------------------------------------------------------------------

	enum class EModelKind : unsigned char { Strike, Fuse, Hazard, Arena };

	struct FModelDecal
	{
		EModelKind Kind = EModelKind::Strike;
		EStyle Style = EStyle::Normal;
		uint32_t Serial = 0;
		abyss::EntityId Entity = abyss::kNoEntity;
		std::string AbilityId;
		uint32_t HazardId = 0;
		abyss::AbilityShapeDef Shape;
		abyss::Vec2 Origin;
		abyss::Vec2 Dir{ 1, 0 };
		FGeometry Geo;
		double StartMs = 0.0;
		double ContactMs = 0.0;
		double EndMs = -1.0;
		double CancelMs = -1.0;
		bool bContactPlayed = false;
		bool bSeen = false;
		bool bLate = false;       // drawn from a snapshot view (a load / zone build), not its event
		bool bCleared = false;    // a zone change dropped it
		// readings
		double FirstDrawnMs = -1.0;
		double ContactFrameMs = -1.0;
		bool bDropped = false;    // visible on a frame the budget did not draw it
		bool bGap = false;        // no drawable windup on a frame between its first and its contact
	};

	struct FModelMarker
	{
		abyss::EntityId Entity = abyss::kNoEntity;
		std::string AbilityId;
		abyss::Vec2 Origin;
		double StartMs = 0.0;
	};

	class FTelegraphLayerModel
	{
	public:
		FTelegraphLayerModel(const abyss::DataStore& InData, int InCap) : Data(InData), Cap(InCap) {}

		void BeginFrame(const abyss::Snapshot& Snap)
		{
			if (Snap.zone.mapId != MapId)
			{
				// UAbyssWorldBuilder::ClearZone on the rebuild: every decal goes silently.
				for (FModelDecal& D : Decals)
				{
					D.bCleared = true;
					Archive.push_back(D);
				}
				Decals.clear();
				Markers.clear();
				MapId = Snap.zone.mapId;
			}
		}

		void OnEvent(const abyss::Event& E, const abyss::Snapshot& Snap, int Steps)
		{
			if (const auto* Spawned = std::get_if<abyss::EvEntitySpawned>(&E))
			{
				if (Spawned->kind == abyss::EntityKind::Monster)
				{
					DefIds[Spawned->id] = Spawned->defId;
				}
			}
			else if (const auto* Tg = std::get_if<abyss::EvTelegraph>(&E))
			{
				HandleTelegraph(*Tg, Snap, false);
			}
			else if (const auto* Hz = std::get_if<abyss::EvHazard>(&E))
			{
				HandleHazard(*Hz, false);
			}
			else if (const auto* Ar = std::get_if<abyss::EvArena>(&E))
			{
				HandleArena(*Ar, Snap.simNowMs, false);
			}
			else if (const auto* Cancel = std::get_if<abyss::EvMonsterAttackCancelled>(&E))
			{
				for (FModelDecal& D : Decals)
				{
					if (D.Kind == EModelKind::Strike && D.Entity == Cancel->monster && D.CancelMs < 0.0)
					{
						const double At = CancelTimeFor(D.ContactMs, D.bContactPlayed, Snap.simNowMs, Steps);
						if (At >= 0.0)
						{
							D.CancelMs = At;
						}
					}
				}
			}
		}

		void Tick(const abyss::Snapshot& Snap)
		{
			const FContract& C = RealContract();
			const double Now = Snap.simNowMs - (1.0 - Snap.interpolationAlpha) * abyss::kSimStepMs;
			const double SimNow = Snap.simNowMs;
			// ---- late views and what the snapshot dropped ----
			for (FModelDecal& D : Decals)
			{
				D.bSeen = false;
			}
			for (const abyss::TelegraphView& V : Snap.telegraphs)
			{
				if (FModelDecal* Known = FindStrike(V.entity, V.abilityId, V.startMs, V.origin))
				{
					Known->bSeen = true;
					continue;
				}
				if (FindMarker(V.entity, V.abilityId, V.startMs, V.origin) || !LateViewWorthDrawing(Now, V.contactMs))
				{
					continue;
				}
				abyss::EvTelegraph Late;
				Late.entity = V.entity;
				Late.abilityId = V.abilityId;
				Late.kind = V.kind;
				Late.shape = V.shape;
				Late.origin = V.origin;
				Late.dir = V.dir;
				Late.startMs = V.startMs;
				Late.contactMs = V.contactMs;
				Late.style = V.style;
				Late.source = V.source;
				const size_t Before = Decals.size();
				HandleTelegraph(Late, Snap, true);
				if (Decals.size() > Before)
				{
					Decals.back().bSeen = true;
				}
			}
			for (FModelDecal& D : Decals)
			{
				if (D.Kind == EModelKind::Strike && !D.bSeen && D.CancelMs < 0.0 && MissingMeansCancelled(SimNow, D.ContactMs))
				{
					D.CancelMs = SimNow;
					++MissingCancels;
				}
			}
			for (const abyss::HazardView& V : Snap.hazards)
			{
				bool bKnown = false;
				for (const FModelDecal& D : Decals)
				{
					bKnown = bKnown || (D.Kind == EModelKind::Hazard && D.HazardId == V.id);
				}
				if (!bKnown && LateViewWorthDrawing(Now, V.untilMs))
				{
					abyss::EvHazard Late;
					Late.id = V.id;
					Late.source = V.source;
					Late.abilityId = V.abilityId;
					Late.shape = V.shape;
					Late.origin = V.origin;
					Late.dir = V.dir;
					Late.startMs = V.startMs;
					Late.untilMs = V.untilMs;
					HandleHazard(Late, true);
				}
			}
			for (const abyss::ArenaView& V : Snap.arenas)
			{
				bool bKnown = false;
				for (const FModelDecal& D : Decals)
				{
					bKnown = bKnown || (D.Kind == EModelKind::Arena && D.Entity == V.entity && D.EndMs < 0.0);
				}
				if (!bKnown)
				{
					abyss::EvArena Late;
					Late.entity = V.entity;
					Late.centre = V.centre;
					Late.radius = V.fromRadius;
					Late.targetRadius = V.targetRadius;
					Late.startMs = V.startMs;
					Late.endMs = V.endMs;
					HandleArena(Late, SimNow, true);
				}
			}
			for (FModelDecal& D : Decals)
			{
				if (D.Kind != EModelKind::Arena || D.EndMs >= 0.0)
				{
					continue;
				}
				bool bListed = false;
				for (const abyss::ArenaView& V : Snap.arenas)
				{
					bListed = bListed || V.entity == D.Entity;
				}
				if (!bListed)
				{
					D.EndMs = SimNow;
				}
			}
			// ---- timelines ----
			std::vector<FState> States(Decals.size());
			for (size_t I = 0; I < Decals.size(); ++I)
			{
				FModelDecal& D = Decals[I];
				switch (D.Kind)
				{
				case EModelKind::Strike:
				case EModelKind::Fuse:
					States[I] = StrikeState(C, D.Style, Now, D.StartMs, D.ContactMs, D.CancelMs);
					if (!D.bContactPlayed && D.CancelMs < 0.0 && Now >= D.ContactMs)
					{
						D.bContactPlayed = true;
					}
					break;
				case EModelKind::Hazard:
				case EModelKind::Arena:
					States[I] = ZoneState(C, D.Style, Now, D.StartMs, D.EndMs);
					break;
				}
			}
			// ---- the budget ----
			std::vector<FBudgetEntry> Entries;
			std::vector<size_t> EntryDecal;
			for (size_t I = 0; I < Decals.size(); ++I)
			{
				if (States[I].Phase == EPhase::Done || !States[I].bVisible)
				{
					continue;
				}
				const FModelDecal& D = Decals[I];
				FBudgetEntry Entry;
				Entry.Rank = RankOf(D.Style);
				const abyss::Vec2 Centre(D.Geo.CentreXCm / abyss::kTileSizeUU, D.Geo.CentreYCm / abyss::kTileSizeUU);
				Entry.DistSq = abyss::DistSq(Centre, Snap.hero.pos);
				Entry.StartMs = D.StartMs;
				Entry.Serial = D.Serial;
				Entries.push_back(Entry);
				EntryDecal.push_back(I);
			}
			Peak = (std::max)(Peak, static_cast<int>(Entries.size()));
			const std::vector<bool> Picked = SelectVisible(Entries, Cap);
			std::vector<bool> Drawn(Decals.size(), false);
			for (size_t E = 0; E < Picked.size(); ++E)
			{
				Drawn[EntryDecal[E]] = Picked[E];
				if (!Picked[E])
				{
					Decals[EntryDecal[E]].bDropped = true;
				}
			}
			// ---- readings, then the finished decals ----
			for (size_t I = 0; I < Decals.size(); ++I)
			{
				FModelDecal& D = Decals[I];
				const FState& S = States[I];
				if (D.Kind == EModelKind::Strike || D.Kind == EModelKind::Fuse)
				{
					const bool bShows = Drawn[I] && Drawable(C, D.Geo, C.Style(D.Style), S);
					if (S.Phase == EPhase::Flash && D.ContactFrameMs < 0.0)
					{
						D.ContactFrameMs = Now;
					}
					else if (S.Phase == EPhase::Windup)
					{
						if (D.FirstDrawnMs < 0.0)
						{
							D.FirstDrawnMs = bShows ? Now : -1.0;
						}
						else if (!bShows)
						{
							D.bGap = true;
						}
					}
				}
			}
			for (size_t I = Decals.size(); I-- > 0;)
			{
				if (States[I].Phase == EPhase::Done)
				{
					Archive.push_back(Decals[I]);
					Decals.erase(Decals.begin() + static_cast<std::ptrdiff_t>(I));
				}
			}
		}

		void Finish()
		{
			for (const FModelDecal& D : Decals)
			{
				Archive.push_back(D);
			}
			Decals.clear();
		}

		std::vector<FModelDecal> Archive;
		int Peak = 0;
		int MissingCancels = 0;

	private:
		FModelDecal* FindStrike(abyss::EntityId Entity, std::string_view AbilityId, double StartMs, abyss::Vec2 Origin)
		{
			for (FModelDecal& D : Decals)
			{
				if ((D.Kind == EModelKind::Strike || D.Kind == EModelKind::Fuse)
					&& SameTelegraph(D.Entity, D.AbilityId, D.StartMs, D.Origin, Entity, AbilityId, StartMs, Origin))
				{
					return &D;
				}
			}
			return nullptr;
		}

		bool FindMarker(abyss::EntityId Entity, std::string_view AbilityId, double StartMs, abyss::Vec2 Origin) const
		{
			for (const FModelMarker& M : Markers)
			{
				if (SameTelegraph(M.Entity, M.AbilityId, M.StartMs, M.Origin, Entity, AbilityId, StartMs, Origin))
				{
					return true;
				}
			}
			return false;
		}

		void HandleTelegraph(const abyss::EvTelegraph& Event, const abyss::Snapshot& Snap, bool bLate)
		{
			if (FindStrike(Event.entity, Event.abilityId, Event.startMs, Event.origin) != nullptr
				|| FindMarker(Event.entity, Event.abilityId, Event.startMs, Event.origin))
			{
				return;
			}
			bool bDamages = true;
			bool bThemed = false;
			if (Event.source == abyss::TelegraphSource::OnDeath)
			{
				bThemed = !AbyssThemeRules::ThemeOf(Data.FindMap(Snap.zone.mapId)).empty();
				const abyss::MonsterDef* Def = nullptr;
				if (bThemed)
				{
					const auto Found = DefIds.find(Event.entity);
					if (Found != DefIds.end())
					{
						Def = Data.FindMonster(Found->second);
					}
				}
				bDamages = OnDeathDamages(Event.abilityId, Def, &Data.Combat().eliteAffixes);
			}
			if (TelegraphLook(Event.kind, Event.source, bDamages, bThemed) != ELook::Decal)
			{
				Markers.push_back({ Event.entity, Event.abilityId, Event.origin, Event.startMs });
				return;
			}
			FModelDecal& D = Decals.emplace_back();
			D.Kind = Event.source == abyss::TelegraphSource::OnDeath ? EModelKind::Fuse : EModelKind::Strike;
			D.Serial = NextSerial++;
			D.Entity = Event.entity;
			D.AbilityId = Event.abilityId;
			D.Shape = Event.shape;
			D.Origin = Event.origin;
			D.Dir = Event.dir;
			D.Style = StyleOf(Event.style);
			D.Geo = MakeGeometry(D.Shape, D.Origin, D.Dir, OutsideReachCm(RealContract().Style(D.Style), false));
			D.StartMs = Event.startMs;
			D.ContactMs = Event.contactMs;
			D.bLate = bLate;
		}

		void HandleHazard(const abyss::EvHazard& Event, bool bLate)
		{
			for (const FModelDecal& D : Decals)
			{
				if (D.Kind == EModelKind::Hazard && D.HazardId == Event.id)
				{
					return;
				}
			}
			FModelDecal& D = Decals.emplace_back();
			D.Kind = EModelKind::Hazard;
			D.Serial = NextSerial++;
			D.Entity = Event.source;
			D.AbilityId = Event.abilityId;
			D.HazardId = Event.id;
			D.Shape = Event.shape;
			D.Origin = Event.origin;
			D.Dir = Event.dir;
			D.Style = EStyle::Hazard;
			D.Geo = MakeGeometry(D.Shape, D.Origin, D.Dir, OutsideReachCm(RealContract().Style(D.Style), false));
			D.StartMs = Event.startMs;
			D.EndMs = Event.untilMs;
			D.bLate = bLate;
		}

		void HandleArena(const abyss::EvArena& Event, double SimNow, bool bLate)
		{
			for (FModelDecal& D : Decals)
			{
				if (D.Kind == EModelKind::Arena && D.Entity == Event.entity && D.EndMs < 0.0)
				{
					D.EndMs = SimNow;
				}
			}
			if (Event.cleared)
			{
				return;
			}
			FModelDecal& D = Decals.emplace_back();
			D.Kind = EModelKind::Arena;
			D.Serial = NextSerial++;
			D.Entity = Event.entity;
			D.Shape.kind = abyss::AbilityShapeKind::Circle;
			D.Shape.radius = abyss::ArenaRadiusAt(Event.radius, Event.targetRadius, Event.endMs - Event.startMs,
				SimNow - Event.startMs);
			D.Origin = Event.centre;
			D.Style = EStyle::Arena;
			D.Geo = MakeGeometry(D.Shape, D.Origin, D.Dir, OutsideReachCm(RealContract().Style(D.Style), false));
			D.StartMs = Event.startMs;
			D.bLate = bLate;
		}

		const abyss::DataStore& Data;
		int Cap = 0;
		std::string MapId;
		std::vector<FModelDecal> Decals;
		std::vector<FModelMarker> Markers;
		std::unordered_map<abyss::EntityId, std::string> DefIds;
		uint32_t NextSerial = 1;
	};

	/** The lowest decal cap of any tier (mobile low): the budget the playthrough and the worst scene must fit. */
	int LowestCap()
	{
		int Lowest = 1 << 20;
		for (int Tier = 0; Tier <= 2; ++Tier)
		{
			Lowest = (std::min)(Lowest, (std::min)(DecalCap(Tier, false), DecalCap(Tier, true)));
		}
		return Lowest;
	}
}

TEST_SUITE("readability")
{
	TEST_CASE("the rule: contact at least 400 ms after the first frame the decal can show")
	{
		CHECK(MinReadableMs == 400.0);
		CHECK(FirstFrameMs == doctest::Approx(33.333).epsilon(0.001));
		CHECK(ReadableWindowMs(1000.0, 1600.0) == doctest::Approx(600.0 - FirstFrameMs));
		CHECK(Readable(0.0, 434.0));
		CHECK_FALSE(Readable(0.0, 433.0));
		CHECK(Readable(500.0, 500.0 + 400.0 + FirstFrameMs));
		CHECK_FALSE(Readable(500.0, 500.0 + 399.0 + FirstFrameMs));
		// The core's floors pass by themselves (the fairness floors, the exploder's fuse).
		CHECK(Readable(0.0, abyss::kAbilityMinWindupMs));
		CHECK(Readable(0.0, abyss::kAbilityBossMinWindupMs));
		CHECK(Readable(0.0, abyss::kOnDeathBurstMinFuseMs));
		CHECK(RealContract().bFromManifest);
	}

	TEST_CASE("the stage T timeline shows a strike from its first frame: drawn at once, no gap, contact on the flash")
	{
		// A synthetic 600 ms circle per style: the first drawn frame is the first one the render clock reaches the start
		// (never later: no fade-in), and the flash is the first frame at or after the contact.
		abyss::AbilityShapeDef Circle;
		Circle.kind = abyss::AbilityShapeKind::Circle;
		Circle.radius = 2.0;
		for (const EStyle Style : { EStyle::Normal, EStyle::Elite, EStyle::Boss })
		{
			CAPTURE(StyleName(Style));
			const FContract& C = RealContract();
			const FGeometry Geo = MakeGeometry(Circle, abyss::Vec2(4.0, 4.0), abyss::Vec2(1.0, 0.0),
				OutsideReachCm(C.Style(Style), true));
			const double StartMs = 600.0 * 1000.0 / 60.0;
			const FStrikeReading R = ReadStrike(C, Style, Geo, StartMs, StartMs + 600.0, 1000.0 / 30.0, 0.0);
			CHECK(R.FirstMs >= StartMs);
			CHECK(R.FirstMs < StartMs + FirstFrameMs + 1e-6);
			CHECK(R.ContactMs >= StartMs + 600.0);
			CHECK(R.ContactMs < StartMs + 600.0 + FirstFrameMs + 1e-6);
			CHECK_FALSE(R.bGap);
			CHECK(R.WindowMs() >= MinReadableMs);
		}
	}

	TEST_CASE("every Chapter 1 ground strike and hazard windup is on screen 400 ms before its contact")
	{
		const abyss::DataStore& Data = audio_test::Data();
		const abyss::AssetManifest& Manifest = Data.Assets();
		REQUIRE(Manifest.loaded);
		int Checked = 0;
		double Tightest = 1e9;
		for (const FCaster& Caster : Casters())
		{
			const abyss::MonsterDef& Def = *Caster.Def;
			const abyss::AssetEntryDef* Art = Manifest.FindByGameId(abyss::MonsterArtGameId(Manifest, Def.spriteKey, Def.id));
			CAPTURE(Caster.Id);
			CHECK(Art != nullptr);
			if (Art == nullptr)
			{
				continue;
			}
			for (const abyss::MonsterAbilityDef& A : Def.abilities)
			{
				if (!abyss::AbilityHasGroundShape(A.kind))
				{
					continue;   // raise / summon markers and calls: no crimson decal to read
				}
				CAPTURE(A.id);
				// The decal the event draws (TelegraphLook) ...
				CHECK(TelegraphLook(A.kind, abyss::TelegraphSource::Ability, true, true) == ELook::Decal);
				// ... timed as the core times it ...
				const abyss::AnimClipDef* Clip = A.clip.empty() ? nullptr : Art->FindClip(A.clip);
				CHECK((A.clip.empty() || Clip != nullptr));
				const abyss::AbilityTiming Timing = abyss::ComputeAbilityTiming(A, Clip,
					abyss::AbilityMinWindupMs(Caster.Style), Data.Combat().anim, Def.animCategory);
				CHECK(Timing.contactMs >= abyss::AbilityMinWindupMs(Caster.Style));
				// ... through the stage T rules on the sim clock at 30 / 45 / 60 fps.
				const double Window = TightestWindow(A.shape, StyleOf(Caster.Style), Timing.contactMs);
				CHECK(Window >= MinReadableMs);
				Tightest = (std::min)(Tightest, Window);
				++Checked;
			}
		}
		// Bonegorger's slam (also the Hungerbell's), Valdras's four, Keen.
		CHECK(Checked >= 8);
		MESSAGE("tightest strike window on screen " << Tightest << " ms");
		CHECK(Tightest >= MinReadableMs);
	}

	TEST_CASE("every on-death fuse Chapter 1 draws is on screen 400 ms before it goes off; the others draw nothing")
	{
		const abyss::DataStore& Data = audio_test::Data();
		const abyss::EliteAffixTable& Affixes = Data.Combat().eliteAffixes;
		int Drawn = 0;
		int Undrawn = 0;
		// FuseLabel: the event's abilityId ("onDeath" for the definition's own, "affix.<type>"), as CombatSystem names it.
		auto Check = [&](const std::string& FuseLabel, const abyss::MonsterDef* Def, const abyss::MonsterOnDeathDef& D,
			bool bThemed, abyss::TelegraphStyle Style)
		{
			if (!D.present || D.kind == abyss::OnDeathKind::Spawn || !(D.delayMs > 0.0))
			{
				return;   // no fuse: CombatSystem emits no EvTelegraph
			}
			CAPTURE(FuseLabel);
			CAPTURE(Def != nullptr ? Def->id : std::string());
			CAPTURE(abyss::EnumName(Style));
			const ELook Look = TelegraphLook(abyss::OnDeathTelegraphKind(D.shape), abyss::TelegraphSource::OnDeath,
				OnDeathDamages(FuseLabel, Def, &Affixes), bThemed);
			if (Look != ELook::Decal)
			{
				++Undrawn;
				return;
			}
			CHECK(D.delayMs >= abyss::kOnDeathBurstMinFuseMs);
			CHECK(TightestWindow(D.shape, StyleOf(Style), D.delayMs) >= MinReadableMs);
			++Drawn;
		};
		for (const abyss::MonsterDef& Def : Data.Monsters().defs)
		{
			Check(std::string(OnDeathLabel), &Def, Def.onDeath, true, abyss::TelegraphStyle::Normal);
			Check(std::string(OnDeathLabel), &Def, Def.onDeath, true, abyss::TelegraphStyle::Elite);
		}
		for (std::size_t I = 0; I < abyss::EnumCount<abyss::EliteAffixType>(); ++I)
		{
			const std::string Label = std::string(AffixLabelPrefix) + std::string(abyss::EnumName(Affixes.defs[I].type));
			Check(Label, nullptr, Affixes.defs[I].onDeath, true, abyss::TelegraphStyle::Elite);
			Check(Label, nullptr, Affixes.defs[I].onDeath, true, abyss::TelegraphStyle::Boss);
		}
		// The Labyrinth's volatile curse goes off in an unthemed zone: no decal (as before 2G).
		for (const abyss::CurseDef& Curse : Data.AbyssRun().curses)
		{
			const abyss::MonsterOnDeathDef D = abyss::VolatileCurseOnDeath(Curse);
			if (D.present)
			{
				CHECK(TelegraphLook(abyss::OnDeathTelegraphKind(D.shape), abyss::TelegraphSource::OnDeath, true, false)
					== ELook::None);
			}
		}
		// The Cinderbloat's fuse (normal and elite), Scorched (elite and boss); Gravecold's slow ring draws none (its
		// rime pre-cue elite.frozen.death is the read, DR5-slow).
		CHECK(Drawn >= 4);
		CHECK(Undrawn >= 2);
	}

	TEST_CASE("the budget's worst scene - Valdras's last phase and the two fullest packs - stays inside the lowest cap")
	{
		const abyss::DataStore& Data = audio_test::Data();
		const int Cap = LowestCap();
		CHECK(Cap == 16);
		const abyss::MonsterDef* Valdras = Data.FindMonster("valdras");
		REQUIRE(Valdras != nullptr);
		std::string BossZone;
		for (const abyss::MapDef& Map : Data.World().maps)
		{
			for (const abyss::MapSpawnDef& Spawn : Map.spawns)
			{
				BossZone = Spawn.monsterId == Valdras->id ? Map.id : BossZone;
			}
		}
		CHECK_FALSE(BossZone.empty());
		REQUIRE_FALSE(Valdras->phases.empty());
		std::vector<FBudgetEntry> Entries;
		std::vector<bool> IsStrike;   // a strike or a fuse (what the readability rule needs drawn)
		uint32_t Serial = 0;
		auto Add = [&](int Count, EStyle Style, double DistSq, bool bStrike)
		{
			for (int I = 0; I < Count; ++I)
			{
				FBudgetEntry E;
				E.Rank = RankOf(Style);
				E.DistSq = DistSq;
				E.Serial = ++Serial;
				Entries.push_back(E);
				IsStrike.push_back(bStrike);
			}
		};
		// Valdras's last phase: the arena, his windup and the previous strike still fading, every hazard his cooldowns
		// can keep alive at once, his own affix fuse; the hazards nearest the hero (they win the ties against strikes).
		Add(1, EStyle::Arena, 0.0, false);
		const FPotential Boss = PotentialOf(Data, BossZone, *Valdras, true);
		CHECK(Boss.Strikes == 2);
		Add(Boss.Strikes, EStyle::Boss, 1.0, true);
		Add(Boss.Fuses, EStyle::Boss, 1.0, true);
		int Hazards = 0;
		for (const abyss::MonsterAbilityDef& A : Valdras->abilities)
		{
			if (A.hazard.present && A.cooldownMs > 0.0)
			{
				Hazards += static_cast<int>(std::ceil(A.hazard.durationMs / A.cooldownMs));
			}
		}
		CHECK(Hazards >= 2);
		Add(Hazards, EStyle::Hazard, 0.0, false);
		// The bodies his phases stand up (and the bodies those summon): every phase's, as they may all be up.
		FGroup Summoned;
		for (const abyss::MonsterPhaseDef& Phase : Valdras->phases)
		{
			for (const abyss::PhaseSummonDef& S : Phase.summons)
			{
				AddMember(Data, BossZone, S.monsterId, S.count, false, Summoned);
			}
		}
		Add(Summoned.P.Strikes, EStyle::Normal, 4.0, true);
		Add(Summoned.P.Fuses, EStyle::Normal, 4.0, true);
		// The two packs that can hold the most strike / fuse decals at once (pack aggro pulls both into the fight).
		std::vector<FGroup> Packs = ThemedPacks(Data, Valdras->id);
		std::sort(Packs.begin(), Packs.end(), [](const FGroup& A, const FGroup& B)
		{
			return A.P.Total() != B.P.Total() ? A.P.Total() > B.P.Total() : A.Label < B.Label;
		});
		REQUIRE(Packs.size() >= 2);
		for (size_t I = 0; I < 2; ++I)
		{
			Add(Packs[I].P.Strikes, EStyle::Normal, 9.0, true);
			Add(Packs[I].P.Fuses, EStyle::Normal, 9.0, true);
		}
		MESSAGE("worst scene: arena 1, Valdras " << Boss.Total() << " + hazards " << Hazards << " + summoned "
			<< Summoned.P.Total() << ", packs " << Packs[0].Label << " " << Packs[0].P.Total() << " and " << Packs[1].Label
			<< " " << Packs[1].P.Total() << ": " << Entries.size() << " decals, lowest cap " << Cap);
		CHECK(Packs[0].P.Total() >= 3);
		const std::vector<bool> Picked = SelectVisible(Entries, Cap);
		int DroppedStrikes = 0;
		for (size_t I = 0; I < Picked.size(); ++I)
		{
			DroppedStrikes += !Picked[I] && IsStrike[I] ? 1 : 0;
		}
		CHECK(DroppedStrikes == 0);
		CHECK(static_cast<int>(Entries.size()) <= Cap);
	}

	TEST_CASE("a Chapter 1 playthrough's telegraphs at 30 fps and the lowest cap: none dropped, each read 400 ms ahead")
	{
		const abyss::DataStore& Data = audio_test::Data();
		const int Cap = LowestCap();
		const double FrameMs = 2.0 * abyss::kSimStepMs;   // 30 fps: two sim steps per rendered frame
		std::map<std::string, double> TightestByAbility;
		std::set<std::string> Seen;
		int Strikes = 0, Fuses = 0, Hazards = 0, Arenas = 0, Cancelled = 0, Late = 0, Dropped = 0, Peak = 0;
		double Tightest = 1e9;
		struct FRun
		{
			abyss::ClassId Cls;
			uint64_t Seed;
		};
		for (const FRun Run : { FRun{ abyss::ClassId::Warrior, 101 }, FRun{ abyss::ClassId::Mage, 202 },
			FRun{ abyss::ClassId::Rogue, 303 } })
		{
			CAPTURE(abyss::EnumName(Run.Cls));
			std::unique_ptr<abyss::GameSim> Sim = abyss::GameSim::Create(Data, abyss::SimConfig{});
			REQUIRE(Sim != nullptr);
			REQUIRE(Sim->NewGame(Run.Cls, abyss::Difficulty::Normal, Run.Seed));
			abyss::AutoPilotConfig Config;
			Config.cls = Run.Cls;
			Config.scenario = abyss::AutoPilotScenario::Chapter1;
			Config.storyMode = abyss::AutoPilotStoryMode::PlayerLike;
			abyss::GameSim& SimRef = *Sim;
			abyss::AutoPilot Pilot(Data, Config, [&SimRef](const abyss::Command& Command) { SimRef.Submit(Command); });
			FTelegraphLayerModel Model(Data, Cap);
			Model.BeginFrame(Sim->View());
			for (const abyss::Event& E : Sim->Events())
			{
				Pilot.ObserveEvent(E);
				Model.OnEvent(E, Sim->View(), 0);
			}
			bool bFinished = false;
			for (int64_t Guard = 0; Guard < 3 * 3600 * 30 && !bFinished; ++Guard)
			{
				const abyss::AutoPilotStatus Status = Pilot.Advance(Sim->View());
				if (Status == abyss::AutoPilotStatus::Finished)
				{
					bFinished = true;
					break;
				}
				if (Status == abyss::AutoPilotStatus::Checkpoint)
				{
					continue;
				}
				Pilot.PrepareFrame(Sim->View());
				const int32_t Steps = Sim->Frame(FrameMs);
				const abyss::Snapshot& View = Sim->View();
				Model.BeginFrame(View);
				for (const abyss::Event& E : Sim->Events())
				{
					Pilot.ObserveEvent(E);
					Model.OnEvent(E, View, Steps);
				}
				Pilot.EndFrame(View, Steps);
				Model.Tick(View);
			}
			Model.Finish();
			CHECK(bFinished);
			CHECK(Pilot.Ok());
			if (!Pilot.Ok())
			{
				MESSAGE("bot failure: " << Pilot.Failure() << "\n" << Pilot.RecentNotes());
			}
			CHECK(Pilot.Reached(abyss::AutoPilotMilestone::ChapterDone));
			CHECK(Model.MissingCancels == 0);
			Peak = (std::max)(Peak, Model.Peak);
			for (const FModelDecal& D : Model.Archive)
			{
				CAPTURE(D.AbilityId);
				CAPTURE(D.StartMs);
				Dropped += D.bDropped ? 1 : 0;
				CHECK_FALSE(D.bDropped);
				if (D.Kind == EModelKind::Hazard || D.Kind == EModelKind::Arena)
				{
					(D.Kind == EModelKind::Hazard ? Hazards : Arenas) += 1;
					continue;
				}
				if (D.bCleared || D.CancelMs >= 0.0)
				{
					Cancelled += 1;
					continue;
				}
				if (D.bLate)
				{
					Late += 1;
					continue;
				}
				(D.Kind == EModelKind::Fuse ? Fuses : Strikes) += 1;
				CHECK(D.FirstDrawnMs >= D.StartMs);
				CHECK(D.FirstDrawnMs < D.StartMs + FrameMs + 1e-6);   // drawn on the first frame the render clock reaches it
				CHECK(D.ContactFrameMs >= D.ContactMs);
				CHECK_FALSE(D.bGap);
				const double Window = D.ContactFrameMs - D.FirstDrawnMs;
				CHECK(Window >= MinReadableMs);
				CHECK(Window + 1e-6 >= ReadableWindowMs(D.StartMs, D.ContactMs));
				Tightest = (std::min)(Tightest, Window);
				const std::string Key = D.Kind == EModelKind::Fuse ? "fuse:" + D.AbilityId : D.AbilityId;
				Seen.insert(Key);
				const auto Found = TightestByAbility.find(Key);
				TightestByAbility[Key] = Found == TightestByAbility.end() ? Window : (std::min)(Found->second, Window);
			}
		}
		std::string Summary;
		for (const auto& [Id, Ms] : TightestByAbility)
		{
			Summary += " " + Id + "=" + std::to_string(static_cast<int>(Ms));
		}
		MESSAGE("3 playthroughs at 30 fps, cap " << Cap << ": " << Strikes << " strikes, " << Fuses << " fuses, " << Hazards
			<< " hazards, " << Arenas << " arenas (" << Cancelled << " cancelled or cleared, " << Late
			<< " late views), peak " << Peak << " decals at once, dropped " << Dropped << "; tightest window " << Tightest
			<< " ms;" << Summary);
		// Every Chapter 1 ground strike was met and read, and the Cinderbloat's fuse.
		for (const char* Id : { "clapper_slam", "banner_sweep", "oath_brand", "breach_seep", "lunge", "keen", "fuse:onDeath" })
		{
			CAPTURE(Id);
			CHECK(Seen.count(Id) == 1);
		}
		CHECK(Hazards > 0);
		CHECK(Arenas > 0);
		CHECK(Peak <= Cap);
		CHECK(Tightest >= MinReadableMs);
	}
}
