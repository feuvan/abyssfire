// The monster-state presentation rules of the dark remake's Wave 2 core (U2 / U3 / U5 monster parts, implementation-plan
// 4 / 9 2G; DECISIONS DR5, DR16). What the world builder, UAbyssCorpseLayer, the VFX, audio and world-UI layers show for
// the K3-K6 monster states:
//
// * Body: a fleeing monster (MonsterState::Flee) walks / runs and faces its heading; a blocker (MonsterView::blockArcDeg
//   > 0) faces its heading even while it attacks, so a shield wall keeps one front (CountsAsMoving, FacesHeading).
// * Held items (ClassifyAttachments): the main hand, the off hand and every further attachment on its own socket (the
//   Unburied's kettle helm on `head` no longer replaces the sword, crossbow, mace or standard pole they hold).
// * Phase looks (PhaseLookFor): a definition whose art carries `fx.frenzy` (the oath zealot) loops `fx.frenzy.loop` from
//   its first phase on and plays its recipe at the entry unless the row's enter action plays the clip whose notify fires
//   it (the as-built zealot has no enter action: 2F-D8, the surge plays at the EvMonsterPhase); Valdras's standard burns
//   from his third phase (a small table: the art lane has no manifest field for it). A reset drops the look; a death
//   ends it.
// * Auras, pulses, blocks, flight: the aura loop (`monster.<defId>.aura`, else `monster.<art id without monster_>.aura`)
//   while MonsterView::auraRadius > 0; the buff / call pulse on EvMonsterBuffCast (`monster.<defId>.<abilityId>`, else -
//   unless the clip the ability casts with shows it through its own FX notifies (AbilityCastClip: the drummer's Cast01;
//   the Hungerbell's supper bell has no clip) - the bell's breach-violet ring for the breach-touched (Unburied, breach)
//   and the drum's pale dust for every other family (the Living, the beasts: no violet, no light)); loops in the body's
//   frame turn with it only once its facing moved (BlowTurned); `monster.block` at the shield's `face` socket
//   (manifest `fx.block`) with the clang cue chain; the Living's hissed flight cue on EvMonsterFlee.
// * Markers: the elite marker in a themed zone is the Wave 2 sigil (`elite_sigil`, socket `fx_elite` else `fx_overhead`,
//   absolute scale, floating 6 cm with a symmetric 3 cm bob over 1600 ms and a 45 deg/s turn) unless the asset opts out
//   (manifest `eliteMarker: "none"`: Valdras); elsewhere today's crown. A pack leader (MonsterView::packLeader: a K5 mix's
//   leader, the champion ambush's Muster) shows the champion glyph on its nameplate. The Unburied affix loops
//   `elite.unburied` (the only affix with a loop recipe of its own); the Cinderbloat's vent sheds `monster.ash_flakes`
//   while it lives (BodyLoopRecipe).
// * Kills (DeathFxFor): `death.<defId>`, else `death.<family>`, else today's `death.burst`; the coin burst only for a body
//   that pays gold (never a Raised one).
// * Art warm-up (PreloadDefIds): every definition a zone can stand up (spawn mixes, leaders, variants, raise / summon
//   abilities, phase summons, on-death spawns, corpse seeds, its hunts and mini-boss).
//
// UE-free and header-only (std + AbyssCore public headers), tested in unreal/PresentationTests/tests/monsterlook_test.cpp
// (and theme_test.cpp for the sigil chain).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Json.h"
#include "abyss/data/CombatData.h"
#include "abyss/data/DataStore.h"
#include "abyss/data/MapData.h"
#include "abyss/data/MonsterData.h"
#include "abyss/monsters/Monster.h"
#include "abyss/monsters/Phases.h"

namespace AbyssMonsterLookRules
{
	inline constexpr double Pi = 3.14159265358979323846;

	// =================================================================================================================
	// Body
	// =================================================================================================================

	/** The snapshot speed below which a monster stands (tiles / s). */
	inline constexpr double MovingSpeedTiles = 0.05;

	/** Walk / run clips: chasing (a kite step included), patrolling, walking home, fleeing. */
	inline bool CountsAsMoving(abyss::MonsterState State, double SpeedTilesPerSec)
	{
		if (!(SpeedTilesPerSec > MovingSpeedTiles))
		{
			return false;
		}
		return State == abyss::MonsterState::Chase || State == abyss::MonsterState::Patrol
			|| State == abyss::MonsterState::Returning || State == abyss::MonsterState::Flee;
	}

	/** Whether the body faces MonsterView::heading (else the hero): always but in Attack, where only a blocker keeps it. */
	inline bool FacesHeading(abyss::MonsterState State, double BlockArcDeg)
	{
		return State != abyss::MonsterState::Attack || BlockArcDeg > 0.0;
	}

	// =================================================================================================================
	// Held items
	// =================================================================================================================

	inline constexpr std::string_view MainHandSocket = "weapon_r";
	inline constexpr std::string_view OffHandSocket = "weapon_l";

	struct FAttachmentSlots
	{
		int Main = -1;
		int Off = -1;
		/** Every other attachment, on its own socket, in data order. */
		std::vector<int> Extras;
	};

	/**
	 * Sockets: the asset's `attachments[].socket` in data order ("" = none). The first on the off-hand socket is the off
	 * hand; the first on the main-hand socket (or without one) the main hand, else the first that is not the off hand;
	 * the rest are extras. An asset with at most one of each keeps today's pick (the last of each kind).
	 */
	inline FAttachmentSlots ClassifyAttachments(const std::vector<std::string_view>& Sockets)
	{
		FAttachmentSlots Out;
		for (size_t I = 0; I < Sockets.size(); ++I)
		{
			if (Sockets[I] == OffHandSocket && Out.Off < 0)
			{
				Out.Off = static_cast<int>(I);
			}
		}
		for (size_t I = 0; I < Sockets.size(); ++I)
		{
			if (static_cast<int>(I) != Out.Off && (Sockets[I] == MainHandSocket || Sockets[I].empty()))
			{
				Out.Main = static_cast<int>(I);
				break;
			}
		}
		if (Out.Main < 0)
		{
			for (size_t I = 0; I < Sockets.size(); ++I)
			{
				if (static_cast<int>(I) != Out.Off && Sockets[I] != OffHandSocket)
				{
					Out.Main = static_cast<int>(I);
					break;
				}
			}
		}
		for (size_t I = 0; I < Sockets.size(); ++I)
		{
			if (static_cast<int>(I) != Out.Main && static_cast<int>(I) != Out.Off)
			{
				Out.Extras.push_back(static_cast<int>(I));
			}
		}
		return Out;
	}

	// =================================================================================================================
	// Phase looks
	// =================================================================================================================

	struct FPhaseLook
	{
		/** Played once at the phase's entry ("" = none). */
		std::string Instant;
		/** Looped on the monster until its death or a reset ("" = none). */
		std::string Loop;
	};

	/** The phase loops the art lane has no manifest field for (defId, phase entered, loop recipe). */
	struct FPhaseLoopRow
	{
		std::string_view DefId;
		int Phase;
		std::string_view Loop;
	};
	inline constexpr FPhaseLoopRow PhaseLoopTable[] = {
		{ "valdras", 2, "monster.valdras.standard_fire" },   // P3 No Retreat: the standard ignites (vfx_recipes binding)
	};

	/** The clip the enter action of the row entering phase Phase (Def's phases[Phase - 1]) plays ("" = none / no row). */
	inline std::string_view PhaseEnterClip(const abyss::MonsterDef* Def, int Phase)
	{
		if (Def == nullptr || Phase <= 0 || static_cast<size_t>(Phase) > Def->phases.size())
		{
			return {};
		}
		const abyss::MonsterPhaseDef& Row = Def->phases[static_cast<size_t>(Phase) - 1];
		return Row.hasEnterAction ? abyss::PhaseActionClip(Row.enterAction) : std::string_view();
	}

	/**
	 * EvMonsterPhase{phase, reset} of definition DefId (Def: its MonsterDef, FindMonsterDef; null = unknown, no enter
	 * action assumed) whose art is AssetJson (GetAssetJson or the manifest asset): the art's `fx.frenzy` on the first
	 * phase - `loop` for the phase, its `recipe` at once unless the row's enter action plays the clip (`fx.frenzy.clip`,
	 * Roar) whose `notify` fires it (then the surge lands on that notify; the as-built zealot enters without one, 2F-D8,
	 * so the surge plays here) - else the table's loop. A reset shows nothing (the layer drops the running loop).
	 */
	inline FPhaseLook PhaseLookFor(std::string_view DefId, const abyss::MonsterDef* Def, int Phase, bool bReset,
		const abyss::JsonValue& AssetJson)
	{
		FPhaseLook Out;
		if (bReset || Phase <= 0)
		{
			return Out;
		}
		const abyss::JsonValue& Frenzy = AssetJson.Get("fx").Get("frenzy");
		if (Frenzy.IsObject() && Phase == 1)
		{
			const std::string_view NotifyClip = Frenzy.Get("clip").AsString().empty() ? std::string_view("Roar") : Frenzy.Get("clip").AsString();
			const bool bClipPlaysIt = !Frenzy.Get("notify").AsString().empty() && PhaseEnterClip(Def, Phase) == NotifyClip;
			if (!bClipPlaysIt)
			{
				Out.Instant = std::string(Frenzy.Get("recipe").AsString());
			}
			Out.Loop = std::string(Frenzy.Get("loop").AsString());
			return Out;
		}
		for (const FPhaseLoopRow& Row : PhaseLoopTable)
		{
			if (Row.DefId == DefId && Row.Phase == Phase)
			{
				Out.Loop = std::string(Row.Loop);
			}
		}
		return Out;
	}

	// =================================================================================================================
	// Auras, pulses, blocks, flight
	// =================================================================================================================

	inline std::string_view ArtBaseId(std::string_view ArtId)
	{
		constexpr std::string_view Prefix = "monster_";
		return ArtId.substr(0, Prefix.size()) == Prefix ? ArtId.substr(Prefix.size()) : ArtId;
	}

	/** The aura loop's candidates (monster.<defId>.aura, then the art's: the roll-call bearer draws as the standard-bearer). */
	inline std::vector<std::string> AuraChain(std::string_view DefId, std::string_view ArtId)
	{
		std::vector<std::string> Out;
		if (!DefId.empty())
		{
			Out.push_back("monster." + std::string(DefId) + ".aura");
		}
		const std::string_view Base = ArtBaseId(ArtId);
		if (!Base.empty() && Base != DefId)
		{
			Out.push_back("monster." + std::string(Base) + ".aura");
		}
		return Out;
	}

	inline bool WantsAuraLoop(bool bAlive, double AuraRadius)
	{
		return bAlive && AuraRadius > 0.0;
	}

	inline constexpr std::string_view LivingFamily = "living";

	/** The families the breach raised or bred (DR5: their magic is breach violet); the Living and the beasts carry none. */
	inline bool BreachTouched(std::string_view Family)
	{
		return Family == "unburied" || Family == "breach";
	}

	/**
	 * The clip buff / call ability AbilityId of Def casts with ("" = clipless - the Hungerbell's supper bell, whose
	 * EvMonsterBuffCast pulse is the whole tell - or no such ability / definition).
	 */
	inline std::string_view AbilityCastClip(const abyss::MonsterDef* Def, std::string_view AbilityId)
	{
		if (Def == nullptr)
		{
			return {};
		}
		for (const abyss::MonsterAbilityDef& Ability : Def->abilities)
		{
			if (Ability.id == AbilityId)
			{
				return Ability.clip;
			}
		}
		return {};
	}

	/** Whether a clip's notifies show the pulse themselves (any FX_* notify: the drummer's FX_DrumBeat_1..4). */
	inline bool NotifiesShowPulse(const std::vector<std::string_view>& NotifyNames)
	{
		for (const std::string_view Name : NotifyNames)
		{
			if (Name.substr(0, 3) == "FX_")
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * The pulse of a buff / call (EvMonsterBuffCast) of caster DefId (family CasterFamily): its own recipe, else - when
	 * the clip the ability casts with carries no FX notifies of its own (bClipShowsPulse false; true for the drummer's
	 * Cast01, whose FX_DrumBeat_1..4 already beat the pulse; the decision comes from the ability's definition and art,
	 * never from whatever clip happens to play: the Hungerbell's clipless call keeps its pulse during a Clapper Slam) - the
	 * bell's breach-violet ring for the breach-touched (enemy magic, DR5), else the drum's pale dust (no violet, no light:
	 * the Living, the Gravegnawers).
	 */
	inline std::vector<std::string> PulseChain(std::string_view DefId, std::string_view AbilityId, std::string_view CasterFamily,
		bool bClipShowsPulse)
	{
		std::vector<std::string> Out;
		if (!DefId.empty() && !AbilityId.empty())
		{
			Out.push_back("monster." + std::string(DefId) + "." + std::string(AbilityId));
		}
		if (!bClipShowsPulse)
		{
			Out.push_back(BreachTouched(CasterFamily) ? "notify.FX_bell_ring" : "notify.FX_DrumBeat_1");
		}
		return Out;
	}

	/** A loop's frame follows the body only once its facing turned more than this (a per-frame push of every handle is waste). */
	inline constexpr double BlowTurnDeg = 2.0;

	/** Whether facing (X, Y) turned more than MinDeg from the last pushed (PrevX, PrevY) (a zero previous = never pushed). */
	inline bool BlowTurned(double PrevX, double PrevY, double X, double Y, double MinDeg = BlowTurnDeg)
	{
		const double PrevLen = std::sqrt(PrevX * PrevX + PrevY * PrevY);
		const double Len = std::sqrt(X * X + Y * Y);
		if (!(PrevLen > 1e-9) || !(Len > 1e-9))
		{
			return Len > 1e-9;
		}
		const double Cos = (PrevX * X + PrevY * Y) / (PrevLen * Len);
		return Cos < std::cos(MinDeg * Pi / 180.0);
	}

	struct FBlockFx
	{
		bool bValid = false;
		std::string Recipe = "monster.block";
		/** The held item the spark sits on (a static mesh asset name) and its socket. */
		std::string Item;
		std::string Socket = "face";
	};

	/** The art's `fx.block {item, socket}` (the risen shieldbearer's tower shield face). */
	inline FBlockFx ParseBlockFx(const abyss::JsonValue& AssetJson)
	{
		FBlockFx Out;
		const abyss::JsonValue& Block = AssetJson.Get("fx").Get("block");
		if (!Block.IsObject())
		{
			return Out;
		}
		Out.bValid = true;
		Out.Item = std::string(Block.Get("item").AsString());
		if (const std::string_view Socket = Block.Get("socket").AsString(); !Socket.empty())
		{
			Out.Socket = std::string(Socket);
		}
		return Out;
	}

	/** EvBlock's sound: the shieldbearer's own clang once rendered (planned), else the port's block cue. */
	inline std::vector<std::string_view> BlockCueChain()
	{
		return { "sfx_shieldbearer_block", "block" };
	}

	/** EvMonsterFlee's sound (a start, never `ended`): the Living's hissed "go!" (planned: silent until rendered). */
	inline std::string_view FleeCue(bool bEnded, std::string_view Family)
	{
		return !bEnded && Family == LivingFamily ? std::string_view("sfx_deserter_flee") : std::string_view();
	}

	// =================================================================================================================
	// Markers
	// =================================================================================================================

	enum class EEliteMarker : unsigned char
	{
		None,
		Crown,   // today's SM_FX_EliteCrown (elite_crown@<theme> first)
		Sigil,   // Wave 2: SM_FX_EliteSigil (elite_sigil), crown chain behind it
	};

	inline constexpr std::string_view SigilGameId = "elite_sigil";
	inline constexpr std::string_view CrownGameId = "elite_crown";

	/**
	 * The elite marker of a marked monster (elite or mini-boss): in a zone with a presentation theme the sigil first
	 * (the crown chain when no sigil asset exists), none for art that opts out (AssetJson `eliteMarker: "none"`);
	 * elsewhere today's crown.
	 */
	inline EEliteMarker EliteMarkerFor(bool bMarked, bool bThemedZone, std::string_view AssetEliteMarker)
	{
		if (!bMarked)
		{
			return EEliteMarker::None;
		}
		if (!bThemedZone)
		{
			return EEliteMarker::Crown;
		}
		return AssetEliteMarker == "none" ? EEliteMarker::None : EEliteMarker::Sigil;
	}

	struct FSigilMotion
	{
		/** Level float over the socket, the symmetric bob about it, its period, the turn about its own axis. */
		double FloatCm = 6.0;
		double BobCm = 3.0;
		double BobPeriodMs = 1600.0;
		double SpinDegPerSec = 45.0;
		bool bAbsoluteScale = true;
		std::string Socket = "fx_elite";
		std::string FallbackSocket = "fx_overhead";
	};

	/** SigilJson: the sigil asset (GetAssetJson / the manifest's SM_FX_EliteSigil): `fx.motion`, `fx.socket`. */
	inline FSigilMotion ParseSigilMotion(const abyss::JsonValue& SigilJson)
	{
		FSigilMotion Out;
		const abyss::JsonValue& Fx = SigilJson.Get("fx");
		const abyss::JsonValue& Motion = Fx.Get("motion");
		if (Motion.Get("bobCm").IsNumber())
		{
			Out.BobCm = (std::max)(0.0, Motion.Get("bobCm").AsDouble());
		}
		if (Motion.Get("bobPeriodMs").AsDouble(0.0) > 0.0)
		{
			Out.BobPeriodMs = Motion.Get("bobPeriodMs").AsDouble();
		}
		if (Motion.Get("spinDegPerSec").IsNumber())
		{
			Out.SpinDegPerSec = Motion.Get("spinDegPerSec").AsDouble();
		}
		if (Fx.Get("absoluteScale").IsBool())
		{
			Out.bAbsoluteScale = Fx.Get("absoluteScale").AsBool();
		}
		if (const std::string_view Socket = Fx.Get("socket").AsString(); !Socket.empty())
		{
			Out.Socket = std::string(Socket);
		}
		return Out;
	}

	/** Height over the socket at TimeMs: the float plus a symmetric sine bob (trough = float - bob). */
	inline double SigilLiftCm(const FSigilMotion& M, double TimeMs)
	{
		return M.FloatCm + M.BobCm * std::sin(2.0 * Pi * TimeMs / (std::max)(1.0, M.BobPeriodMs));
	}

	/** World yaw at TimeMs (degrees, [0, 360)). */
	inline double SigilYawDeg(const FSigilMotion& M, double TimeMs)
	{
		const double Yaw = std::fmod(TimeMs / 1000.0 * M.SpinDegPerSec, 360.0);
		return Yaw < 0.0 ? Yaw + 360.0 : Yaw;
	}

	/** The champion glyph's crimson (the sigil's ember, DR5's crimson accent: never ember orange). */
	inline constexpr uint32_t ChampionRgb = 0xC0283A;

	/** The nameplate's champion glyph: a living pack leader (a K5 mix's leader, the champion ambush's Muster). */
	inline bool ShowsChampionMarker(bool bPackLeader, bool bAlive)
	{
		return bPackLeader && bAlive;
	}

	/**
	 * A loop the art carries on the living body ("" = none): `monster.ash_flakes` for the manifest's `fx.ashFlakes` (the
	 * Cinderbloat's vent), from its spawn to its death.
	 */
	inline std::string_view BodyLoopRecipe(const abyss::JsonValue& AssetJson)
	{
		return AssetJson.Get("fx").Get("ashFlakes").IsObject() ? std::string_view("monster.ash_flakes") : std::string_view();
	}

	/** An affix's own loop on the monster ("" = none: the affix aura quad is every affix's look today). */
	inline std::string_view AffixLoopRecipe(abyss::EliteAffixType Affix)
	{
		return Affix == abyss::EliteAffixType::Unburied ? std::string_view("elite.unburied") : std::string_view();
	}

	// =================================================================================================================
	// Kills
	// =================================================================================================================

	inline constexpr std::string_view DeathFallbackRecipe = "death.burst";

	struct FDeathFx
	{
		/** Candidates, most specific first; the last is today's death.burst. */
		std::vector<std::string> Chain;
		/** The kill's coin burst (reward.gold): not for a body that pays nothing (a Raised one). */
		bool bCoins = true;
	};

	inline FDeathFx DeathFxFor(std::string_view DefId, std::string_view Family, abyss::MonsterRole Role)
	{
		FDeathFx Out;
		if (!DefId.empty())
		{
			Out.Chain.push_back("death." + std::string(DefId));
		}
		if (!Family.empty())
		{
			Out.Chain.push_back("death." + std::string(Family));
		}
		Out.Chain.emplace_back(DeathFallbackRecipe);
		Out.bCoins = Role != abyss::MonsterRole::Raised;
		return Out;
	}

	/** A definition's or family's death recipe plays in the victim's frame (Blow = its facing); death.burst keeps the hit's. */
	inline bool DeathUsesVictimFacing(std::string_view RecipeId)
	{
		return RecipeId != DeathFallbackRecipe;
	}

	// =================================================================================================================
	// Art warm-up
	// =================================================================================================================

	/** A definition by id: the monster tables, else a hunt's own (quest_hunts.json: Dreck, Harke, Hungerbell). */
	inline const abyss::MonsterDef* FindMonsterDef(const abyss::DataStore& Data, std::string_view Id)
	{
		if (const abyss::MonsterDef* Def = Data.FindMonster(Id))
		{
			return Def;
		}
		const abyss::HuntDef* Hunt = Data.Monsters().FindHunt(Id);
		return Hunt != nullptr ? &Hunt->defNormal : nullptr;
	}

	/**
	 * Every definition the zone can show (data order, unique): its spawns (mix members, leaders, flag variants), its
	 * corpse seeds, its hunts (leaders and minions) and mini-boss, and transitively what they stand up (raise / summon
	 * abilities, phase summons, on-death spawns).
	 */
	inline std::vector<std::string> PreloadDefIds(const abyss::DataStore& Data, const abyss::MapDef& Map)
	{
		std::vector<std::string> Out;
		auto Add = [&Out](std::string_view Id)
		{
			if (!Id.empty() && std::find(Out.begin(), Out.end(), Id) == Out.end())
			{
				Out.emplace_back(Id);
			}
		};
		for (const abyss::MapSpawnDef& Spawn : Map.spawns)
		{
			Add(Spawn.monsterId);
			for (const abyss::MapSpawnMixDef& Mix : Spawn.mix)
			{
				Add(Mix.monsterId);
			}
			if (Spawn.leader.present)
			{
				Add(Spawn.leader.monsterId);
			}
			for (const abyss::MapSpawnVariantDef& Variant : Spawn.variants)
			{
				Add(Variant.monsterId);
			}
		}
		for (const abyss::MapCorpseSeedDef& Seed : Map.corpseSeeds)
		{
			Add(Seed.monsterId);
		}
		for (const abyss::HuntDef& Hunt : Data.Monsters().hunts)
		{
			if (Hunt.zone == Map.id)
			{
				Add(Hunt.huntId);
				Add(Hunt.minionMonsterId);
			}
		}
		if (const abyss::MiniBossEntry* MiniBoss = Data.Monsters().MiniBossFor(Map.id))
		{
			Add(MiniBoss->monsterId);
		}
		// what those stand up (each new id is visited in turn)
		for (size_t I = 0; I < Out.size(); ++I)
		{
			const abyss::MonsterDef* Def = FindMonsterDef(Data, Out[I]);
			if (Def == nullptr)
			{
				continue;
			}
			for (const abyss::MonsterAbilityDef& Ability : Def->abilities)
			{
				if (Ability.summon.present)
				{
					Add(Ability.summon.monsterId);
				}
			}
			for (const abyss::MonsterPhaseDef& Phase : Def->phases)
			{
				for (const abyss::PhaseSummonDef& Summon : Phase.summons)
				{
					Add(Summon.monsterId);
				}
			}
			if (Def->onDeath.present && Def->onDeath.kind == abyss::OnDeathKind::Spawn)
			{
				Add(Def->onDeath.spawn.monsterId);
			}
		}
		return Out;
	}
}
