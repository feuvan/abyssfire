// The corpse layer's presentation rules (dark remake U3, implementation-plan.md 4 U3 / 9 2G; DECISIONS DR6, DR16). What
// UAbyssCorpseLayer (Private/Actors/AbyssCorpses.cpp) shows for the Wave 2 core's corpse registry (EvCorpseAdded /
// EvCorpseRemoved / Snapshot::corpses, EvMonsterRaised, EvMonsterFeed) and when:
//
// * Contracts: the M_AF_AshDissolve material (manifest `materials.M_AF_AshDissolve`: ash / shade / edge colours, the
//   edge width, the noise tile and the timeline) and each asset's `corpse` block (ParseCorpseBlock: the look - husk,
//   carcass, puddle -, whether it has a registry entry, the Death clip's hold frame, the story hold, the rise, the
//   recipes, the husk's top). Every field defaults to the contract as shipped. The block's geometry is the asset's at
//   scale 1: HuskGeometry scales it by the body's drawn scale (a hunt leader is drawn x1.25).
// * The ash timeline on the sim clock (Evaluate): the Death clip plays at rate 1 to its hold frame (holdFromMs), holds
//   holdMs, greys over greyMs (toon colours to the ash colours, emissive markers out), crumbles by plates to crumbleTo
//   over crumbleMs (corpse.crumble at its start), stays a husk until the core removes the entry. The removal picks the
//   end (PlanRemoval): expired (the TTL or the cap) dissolves to 1 over expireMs with corpse.expire; raised goes at once
//   (the risen monster reverses the ash over the first 60 % of its Rise: RiseAsh); consumed crumbles away quickly (the
//   Feed's drain or the gnawer's channel led there); burned plays corpse.burn in the fire's colour (BurnColor: the side
//   of EvCorpseRemoved::burner), greys at once, then crumbles and blows away. Map seeds and late views are born husks
//   (posed at the clip's end).
// * The story hold (StoryHoldActive): a body whose timeline waits for its beat (the Corpseherd's cs_rf_herd, Valdras's
//   cs_rf_marshal_falls: `holdUntil story`) keeps its death pose while that beat plays or the director is busy with
//   beats that have not reached it yet; the hold starts again when it ends. Otherwise the timeline runs at once.
// * Bookkeeping: the snapshot reconcile (Reconcile: entries the events did not announce appear as husks, entries gone
//   without a removal dissolve), the visible cap per quality tier (OverflowHusks: mobile tiers keep fewer husks; seeds
//   and story-held bodies first, then the newest), the burn colour (BurnColor), the blob shadow's alpha.
// * Breach-blood (ParsePuddle, PuddleRecipeJson): a body whose block looks `puddle` (no registry entry) leaves its Death
//   clip's tar puddle at the death, drawn with the shipped Puddle decal sprite: radiusFactor x the body's half-width W
//   (the art's `footprintRadiusCm`, the convention of every `radiusFactor` / `radiusFactorOfWidth` it authors); it forms
//   over the clip and dries in 3 s.
//
// UE-free and header-only (std + AbyssCore public headers), tested in unreal/PresentationTests/tests/corpse_test.cpp.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Json.h"
#include "abyss/base/Types.h"
#include "abyss/data/MonsterData.h"
#include "abyss/sim/SimTypes.h"

namespace AbyssCorpseRules
{
	// =================================================================================================================
	// The M_AF_AshDissolve contract
	// =================================================================================================================

	/** The timeline of a husk (ms; the corpse block's `timeline`, else the contract's). */
	struct FTimeline
	{
		double HoldMs = 450.0;
		double GreyMs = 380.0;
		double CrumbleMs = 900.0;
		double CrumbleTo = 0.38;
		double ExpireMs = 700.0;
		double TtlMs = 20000.0;
	};

	struct FAshContract
	{
		/** sRGB colours (the content build bakes them as the ash masters' parameter defaults). */
		uint32_t AshRgb = 0x6E6A62;
		uint32_t ShadeRgb = 0x3E3B37;
		uint32_t EdgeRgb = 0xB8B2A6;
		double EdgeWidth = 0.06;
		bool bEmissiveOff = true;
		/** The noise map spans this many cm of the body (object-space XZ / 60 cm). */
		double NoiseTileCm = 60.0;
		std::string NoiseTexture = "T_FX_AshNoise";
		FTimeline Timeline;
		/** Rise: the grey and the dissolve reverse over this fraction of the Rise clip. */
		double RiseFraction = 0.6;
		bool bFromManifest = false;
	};

	/** A consumed body (eaten or fed on) crumbles away over this long. */
	inline constexpr double QuickCrumbleMs = 350.0;
	/** A Feed drains the held body to this dissolve by its due time (the rest goes with the consume). */
	inline constexpr double FeedDrainTo = 0.85;
	/** The blob shadow's alpha at the crumble's end (manifest corpse `shadow`: "fades with the crumble to 0.35"). */
	inline constexpr double HuskShadowAlpha = 0.35;
	/** A corpse-holding death waits this long past its clip for the core's entry before it fades like today's. */
	inline constexpr double AdoptTimeoutMs = 1000.0;

	inline bool ParseHex(std::string_view Text, uint32_t& Out)
	{
		if (!Text.empty() && Text.front() == '#')
		{
			Text.remove_prefix(1);
		}
		if (Text.size() != 6)
		{
			return false;
		}
		uint32_t Value = 0;
		for (const char C : Text)
		{
			uint32_t Digit = 0;
			if (C >= '0' && C <= '9') Digit = static_cast<uint32_t>(C - '0');
			else if (C >= 'a' && C <= 'f') Digit = static_cast<uint32_t>(C - 'a' + 10);
			else if (C >= 'A' && C <= 'F') Digit = static_cast<uint32_t>(C - 'A' + 10);
			else return false;
			Value = (Value << 4) | Digit;
		}
		Out = Value;
		return true;
	}

	inline double PositiveOr(const abyss::JsonValue& Value, double Fallback)
	{
		const double V = Value.AsDouble(Fallback);
		return Value.IsNumber() && V > 0.0 ? V : Fallback;
	}

	inline FTimeline ParseTimeline(const abyss::JsonValue& Json, const FTimeline& Defaults)
	{
		FTimeline Out = Defaults;
		if (!Json.IsObject())
		{
			return Out;
		}
		Out.HoldMs = (std::max)(0.0, Json.Get("holdMs").AsDouble(Defaults.HoldMs));
		Out.GreyMs = PositiveOr(Json.Get("greyMs"), Defaults.GreyMs);
		Out.CrumbleMs = PositiveOr(Json.Get("crumbleMs"), Defaults.CrumbleMs);
		Out.CrumbleTo = std::clamp(Json.Get("crumbleTo").AsDouble(Defaults.CrumbleTo), 0.0, 1.0);
		Out.ExpireMs = PositiveOr(Json.Get("expireMs"), Defaults.ExpireMs);
		Out.TtlMs = PositiveOr(Json.Get("ttlMs"), Defaults.TtlMs);
		return Out;
	}

	/** Materials: the manifest's `materials` object (FAbyssArtManifest::GetMaterialsJson). */
	inline FAshContract ParseAshContract(const abyss::JsonValue& Materials)
	{
		FAshContract Out;
		const abyss::JsonValue& Ash = Materials.Get("M_AF_AshDissolve");
		if (!Ash.IsObject())
		{
			return Out;
		}
		Out.bFromManifest = true;
		const abyss::JsonValue& Params = Ash.Get("params");
		ParseHex(Params.Get("AshColor").AsString(), Out.AshRgb);
		ParseHex(Params.Get("AshShade").AsString(), Out.ShadeRgb);
		ParseHex(Params.Get("EdgeColor").AsString(), Out.EdgeRgb);
		const double Edge = Params.Get("EdgeWidth").AsDouble(Out.EdgeWidth);
		Out.EdgeWidth = Edge >= 0.0 && Edge <= 1.0 ? Edge : Out.EdgeWidth;
		if (Params.Get("EmissiveOff").IsBool())
		{
			Out.bEmissiveOff = Params.Get("EmissiveOff").AsBool();
		}
		const abyss::JsonValue& Noise = Ash.Get("textures").Get("Noise");
		if (const std::string_view Texture = Noise.Get("texture").AsString(); !Texture.empty())
		{
			Out.NoiseTexture = std::string(Texture);
		}
		// "object-space XZ / 60 cm": the tile is the number before "cm" in the map's description.
		const std::string_view Map = Noise.Get("map").AsString();
		if (const size_t Cm = Map.find(" cm"); Cm != std::string_view::npos)
		{
			size_t Start = Cm;
			while (Start > 0 && ((Map[Start - 1] >= '0' && Map[Start - 1] <= '9') || Map[Start - 1] == '.'))
			{
				--Start;
			}
			double Value = 0.0;
			if (Start < Cm && abyss::ParseJsonNumber(Map.substr(Start, Cm - Start), Value) && Value > 0.0)
			{
				Out.NoiseTileCm = Value;
			}
		}
		Out.Timeline = ParseTimeline(Ash.Get("timeline"), Out.Timeline);
		return Out;
	}

	// =================================================================================================================
	// The asset's corpse block
	// =================================================================================================================

	enum class ELook : unsigned char
	{
		None,
		Husk,      // a body that turns to ash (Unburied, Living, the Corpseherd, Valdras)
		Carcass,   // the Gravegnawers' carcass (same ash treatment, not raised or eaten)
		Puddle,    // breach-blood: a tar puddle, no registry entry (drawn at the death by def: ParsePuddle)
	};

	struct FCorpseBlock
	{
		/** A block whose look the layer draws: a registry entry (husk or carcass) with a timeline. */
		bool bValid = false;
		ELook Look = ELook::None;
		bool bRegistry = false;
		std::string DeathClip = "Death";
		/** The Death clip's frame the pose holds from (ms at rate 1: its end). */
		double HoldFromMs = 0.0;
		FTimeline Timeline;
		/** `timeline.holdUntil story`: the hold waits for StoryBeat. */
		bool bStoryHold = false;
		std::string StoryBeat;
		bool bHasRise = false;
		std::string RiseClip = "Rise";
		double RiseLengthMs = 0.0;
		std::string RaisedAs;
		bool bRaisable = true;
		bool bConsumable = true;
		std::string BurnRecipe = "corpse.burn";
		std::string CrumbleRecipe = "corpse.crumble";
		std::string ExpireRecipe = "corpse.expire";
		std::string ConsumeRecipe = "corpse.consume";
		std::string RaiseRecipe = "monster.raise";
		/** The husk's top above the ground (cm, the Death clip's last frame): the dissolve's height fraction. */
		double HuskTopCm = 0.0;
		double HuskRadiusCm = 0.0;
		/** Husk centre in the actor frame (forward, right, up cm): where its recipes play. */
		double CenterForwardCm = 0.0;
		double CenterRightCm = 0.0;
	};

	inline ELook LookFromName(std::string_view Name)
	{
		if (Name == "husk") return ELook::Husk;
		if (Name == "carcass") return ELook::Carcass;
		if (Name == "puddle") return ELook::Puddle;
		return ELook::None;
	}

	/**
	 * AssetJson: FAbyssArtManifest::GetAssetJson (its `corpse` block) or the manifest's asset object. Contract: the ash
	 * contract (the timeline defaults). A block without a registry entry, a husk / carcass look or a positive hold frame
	 * is not drawn by the layer (bValid false: today's fade).
	 */
	inline FCorpseBlock ParseCorpseBlock(const abyss::JsonValue& AssetJson, const FAshContract& Contract)
	{
		FCorpseBlock Out;
		Out.Timeline = Contract.Timeline;
		const abyss::JsonValue& Block = AssetJson.Get("corpse");
		if (!Block.IsObject())
		{
			return Out;
		}
		Out.Look = LookFromName(Block.Get("look").AsString());
		Out.bRegistry = Block.Get("registry").AsBool(false);
		if (const std::string_view Clip = Block.Get("deathClip").AsString(); !Clip.empty())
		{
			Out.DeathClip = std::string(Clip);
		}
		Out.HoldFromMs = (std::max)(0.0, Block.Get("holdFromMs").AsDouble(0.0));
		const abyss::JsonValue& Timeline = Block.Get("timeline");
		Out.Timeline = ParseTimeline(Timeline, Contract.Timeline);
		if (Timeline.Get("holdUntil").AsString() == "story")
		{
			Out.StoryBeat = std::string(Timeline.Get("story").AsString());
			Out.bStoryHold = !Out.StoryBeat.empty();
		}
		const abyss::JsonValue& Rise = Block.Get("rise");
		if (Rise.IsObject())
		{
			Out.bHasRise = true;
			if (const std::string_view Clip = Rise.Get("clip").AsString(); !Clip.empty())
			{
				Out.RiseClip = std::string(Clip);
			}
			Out.RiseLengthMs = (std::max)(0.0, Rise.Get("lengthMs").AsDouble(0.0));
		}
		Out.RaisedAs = std::string(Block.Get("raisedAs").AsString());
		Out.bRaisable = Block.Get("raisable").AsBool(true);
		Out.bConsumable = Block.Get("consumable").AsBool(true);
		const abyss::JsonValue& Recipes = Block.Get("recipes");
		auto Recipe = [&Recipes](const char* Key, std::string& Field)
		{
			if (const abyss::JsonValue* Value = Recipes.Find(Key); Value != nullptr && Value->IsString())
			{
				Field = std::string(Value->AsString());
			}
		};
		Recipe("burn", Out.BurnRecipe);
		Recipe("crumble", Out.CrumbleRecipe);
		Recipe("expire", Out.ExpireRecipe);
		Recipe("consume", Out.ConsumeRecipe);
		Recipe("raise", Out.RaiseRecipe);
		const abyss::JsonValue& Husk = Block.Get("husk");
		Out.HuskTopCm = (std::max)(0.0, Husk.Get("topCm").AsDouble(0.0));
		Out.HuskRadiusCm = (std::max)(0.0, Husk.Get("radiusCm").AsDouble(0.0));
		const std::span<const abyss::JsonValue> Centre = Husk.Get("centerCm").Items();
		if (Centre.size() >= 2)
		{
			Out.CenterForwardCm = Centre[0].AsDouble(0.0);
			Out.CenterRightCm = Centre[1].AsDouble(0.0);
		}
		Out.bValid = Out.bRegistry && (Out.Look == ELook::Husk || Out.Look == ELook::Carcass) && Out.HoldFromMs > 0.0
			&& Timeline.IsObject();
		return Out;
	}

	/** Whether a death holds its pose for the corpse layer (a definition that leaves an entry, art that draws it). */
	inline bool HoldsCorpsePose(abyss::CorpseLeaves Leaves, const FCorpseBlock& Block, bool bAshMaterials)
	{
		return Leaves != abyss::CorpseLeaves::None && Block.bValid && bAshMaterials;
	}

	/** A husk's geometry as drawn (cm in the actor frame / above the ground). */
	struct FHuskGeometry
	{
		double CenterForwardCm = 0.0;
		double CenterRightCm = 0.0;
		double RadiusCm = 0.0;
		double TopCm = 0.0;
	};

	/**
	 * The block's husk geometry (authored at scale 1) on a body drawn at BodyScale (the art's scale x the instance's
	 * visual scale: a hunt leader's x1.25), so its recipes, the puddle and the dissolve's height fraction follow the body
	 * as drawn. A block without a radius takes DefaultRadiusCm (scaled too).
	 */
	inline FHuskGeometry HuskGeometry(const FCorpseBlock& Block, double BodyScale, double DefaultRadiusCm)
	{
		const double K = BodyScale > 0.0 ? BodyScale : 1.0;
		FHuskGeometry Out;
		Out.CenterForwardCm = Block.CenterForwardCm * K;
		Out.CenterRightCm = Block.CenterRightCm * K;
		Out.RadiusCm = (Block.HuskRadiusCm > 0.0 ? Block.HuskRadiusCm : (std::max)(0.0, DefaultRadiusCm)) * K;
		Out.TopCm = Block.HuskTopCm * K;
		return Out;
	}

	// =================================================================================================================
	// The timeline
	// =================================================================================================================

	enum class EPhase : unsigned char
	{
		Falling,   // the Death clip plays (to the hold frame)
		Hold,      // the death pose holds (holdMs, or while a story beat holds it)
		Grey,      // the colours turn to ash
		Crumble,   // plates fall to crumbleTo
		Husk,      // the husk stays until the entry goes
		Ending,    // the removal's look (expire / burn / consume)
		Gone,      // nothing left to draw
	};

	enum class EEnd : unsigned char
	{
		None,
		Expired,
		Raised,
		Consumed,
		Burned,
	};

	inline EEnd EndOf(abyss::CorpseRemoveReason Reason)
	{
		switch (Reason)
		{
		case abyss::CorpseRemoveReason::Expired: return EEnd::Expired;
		case abyss::CorpseRemoveReason::Raised: return EEnd::Raised;
		case abyss::CorpseRemoveReason::Consumed: return EEnd::Consumed;
		case abyss::CorpseRemoveReason::Burned: return EEnd::Burned;
		}
		return EEnd::Expired;
	}

	/** What the layer keeps per body (sim ms throughout). */
	struct FHuskState
	{
		double DeathMs = 0.0;
		/** Where the hold starts: the clip's hold frame (DeathMs + HoldFromMs), moved by a story hold's release. */
		double HoldStartMs = 0.0;
		/** A story beat holds the pose: no hold count until ReleaseStoryHold. */
		bool bStoryHeld = false;
		/** Born a husk (a map seed, a late view of an old body): grey and crumbled from the start. */
		bool bBornHusk = false;
		// a Feed (EvMonsterFeed): the body drains toward the feeder
		bool bFeeding = false;
		double FeedStartMs = 0.0;
		double FeedDueMs = 0.0;
		double FeedFromDissolve = 0.0;
		double FeedFromGrey = 0.0;
		// the removal (EvCorpseRemoved, or a reconcile / overflow retirement)
		EEnd End = EEnd::None;
		double EndStartMs = 0.0;
		double EndFromGrey = 0.0;
		double EndFromDissolve = 0.0;
	};

	struct FAshLook
	{
		EPhase Phase = EPhase::Falling;
		double Grey = 0.0;
		double Dissolve = 0.0;
		/** The blob shadow's alpha (1 while falling / holding, to HuskShadowAlpha with the crumble, 0 at the end). */
		double ShadowAlpha = 1.0;
		/** The actor can go (Gone). */
		bool bDone = false;
	};

	inline double Clamp01(double X)
	{
		return X < 0.0 ? 0.0 : (X > 1.0 ? 1.0 : X);
	}

	inline double Ramp(double NowMs, double StartMs, double DurationMs)
	{
		if (!(DurationMs > 0.0))
		{
			return NowMs >= StartMs ? 1.0 : 0.0;
		}
		return Clamp01((NowMs - StartMs) / DurationMs);
	}

	inline double Lerp(double A, double B, double T)
	{
		return A + (B - A) * T;
	}

	/** A body that died at DeathMs and holds its clip's frame HoldFromMs (a seed: bBornHusk). */
	inline FHuskState MakeHusk(double DeathMs, double HoldFromMs, bool bBornHusk, bool bStoryHeld)
	{
		FHuskState S;
		S.DeathMs = DeathMs;
		S.HoldStartMs = DeathMs + (std::max)(0.0, HoldFromMs);
		S.bBornHusk = bBornHusk;
		S.bStoryHeld = bStoryHeld && !bBornHusk;
		return S;
	}

	/** The story beat let the body go at NowMs: the hold counts from then (never before the clip's hold frame). */
	inline void ReleaseStoryHold(FHuskState& S, double NowMs, double HoldFromMs)
	{
		if (!S.bStoryHeld)
		{
			return;
		}
		S.bStoryHeld = false;
		S.HoldStartMs = (std::max)(S.DeathMs + (std::max)(0.0, HoldFromMs), NowMs);
	}

	/** The ash look before any removal or feed. */
	inline FAshLook EvaluateBase(const FHuskState& S, const FTimeline& T, double NowMs)
	{
		FAshLook Out;
		if (S.bBornHusk)
		{
			Out.Phase = EPhase::Husk;
			Out.Grey = 1.0;
			Out.Dissolve = T.CrumbleTo;
			Out.ShadowAlpha = HuskShadowAlpha;
			return Out;
		}
		if (S.bStoryHeld || NowMs < S.HoldStartMs)
		{
			Out.Phase = S.bStoryHeld && NowMs >= S.HoldStartMs ? EPhase::Hold : EPhase::Falling;
			return Out;
		}
		const double GreyStart = S.HoldStartMs + T.HoldMs;
		const double CrumbleStart = GreyStart + T.GreyMs;
		Out.Grey = Ramp(NowMs, GreyStart, T.GreyMs);
		const double Crumble = Ramp(NowMs, CrumbleStart, T.CrumbleMs);
		Out.Dissolve = Crumble * T.CrumbleTo;
		Out.ShadowAlpha = Lerp(1.0, HuskShadowAlpha, Crumble);
		if (NowMs < GreyStart) Out.Phase = EPhase::Hold;
		else if (NowMs < CrumbleStart) Out.Phase = EPhase::Grey;
		else if (NowMs < CrumbleStart + T.CrumbleMs) Out.Phase = EPhase::Crumble;
		else Out.Phase = EPhase::Husk;
		return Out;
	}

	/** Sim ms the crumble starts (corpse.crumble plays then); +inf while a story beat holds the body or it was born a husk. */
	inline double CrumbleStartMs(const FHuskState& S, const FTimeline& T)
	{
		if (S.bBornHusk || S.bStoryHeld)
		{
			return std::numeric_limits<double>::infinity();
		}
		return S.HoldStartMs + T.HoldMs + T.GreyMs;
	}

	/** The look at NowMs: the base timeline, a Feed's drain, then the removal's end. */
	inline FAshLook Evaluate(const FHuskState& S, const FTimeline& T, double NowMs)
	{
		FAshLook Out = EvaluateBase(S, T, NowMs);
		if (S.bFeeding && S.End == EEnd::None)
		{
			// drawn into the feeder: grey at once, the plates going until the due time (the rest goes with the consume)
			const double K = Ramp(NowMs, S.FeedStartMs, (std::max)(1.0, S.FeedDueMs - S.FeedStartMs));
			Out.Grey = (std::max)(Out.Grey, Lerp(S.FeedFromGrey, 1.0, Ramp(NowMs, S.FeedStartMs, T.GreyMs)));
			Out.Dissolve = (std::max)(Out.Dissolve, Lerp(S.FeedFromDissolve, FeedDrainTo, K));
			Out.ShadowAlpha = (std::min)(Out.ShadowAlpha, Lerp(1.0, HuskShadowAlpha, K));
			Out.Phase = EPhase::Ending;
		}
		switch (S.End)
		{
		case EEnd::None:
			return Out;
		case EEnd::Raised:
			Out.Phase = EPhase::Gone;
			Out.bDone = true;
			Out.ShadowAlpha = 0.0;
			return Out;
		case EEnd::Expired:
		{
			// dissolve to 1 over expireMs (an early eviction greys on the way)
			const double K = Ramp(NowMs, S.EndStartMs, T.ExpireMs);
			Out.Grey = Lerp(S.EndFromGrey, 1.0, K);
			Out.Dissolve = Lerp(S.EndFromDissolve, 1.0, K);
			Out.ShadowAlpha = Lerp(HuskShadowAlpha, 0.0, K);
			Out.Phase = K >= 1.0 ? EPhase::Gone : EPhase::Ending;
			Out.bDone = K >= 1.0;
			return Out;
		}
		case EEnd::Consumed:
		{
			const double K = Ramp(NowMs, S.EndStartMs, QuickCrumbleMs);
			Out.Grey = Lerp(S.EndFromGrey, 1.0, K);
			Out.Dissolve = Lerp(S.EndFromDissolve, 1.0, K);
			Out.ShadowAlpha = Lerp(HuskShadowAlpha, 0.0, K);
			Out.Phase = K >= 1.0 ? EPhase::Gone : EPhase::Ending;
			Out.bDone = K >= 1.0;
			return Out;
		}
		case EEnd::Burned:
		{
			// the grey starts at once under the flare (no hold), the crumble follows as usual, then the ash blows away
			const double GreyLeft = (1.0 - S.EndFromGrey) * T.GreyMs;
			const double CrumbleStart = S.EndStartMs + GreyLeft;
			const double CrumbleLeft = (std::max)(0.0, T.CrumbleTo - S.EndFromDissolve) / (std::max)(1e-6, T.CrumbleTo) * T.CrumbleMs;
			const double ExpireStart = CrumbleStart + CrumbleLeft;
			Out.Grey = Lerp(S.EndFromGrey, 1.0, Ramp(NowMs, S.EndStartMs, GreyLeft));
			const double Crumbled = Lerp(S.EndFromDissolve, (std::max)(S.EndFromDissolve, T.CrumbleTo), Ramp(NowMs, CrumbleStart, CrumbleLeft));
			const double K = Ramp(NowMs, ExpireStart, T.ExpireMs);
			Out.Dissolve = Lerp(Crumbled, 1.0, K);
			Out.ShadowAlpha = Lerp(Lerp(1.0, HuskShadowAlpha, Ramp(NowMs, CrumbleStart, CrumbleLeft)), 0.0, K);
			Out.Phase = K >= 1.0 ? EPhase::Gone : EPhase::Ending;
			Out.bDone = K >= 1.0;
			return Out;
		}
		}
		return Out;
	}

	/** The core removed the entry at NowMs: the end starts from the look it had then. */
	inline void BeginEnd(FHuskState& S, EEnd End, const FTimeline& T, double NowMs)
	{
		if (S.End != EEnd::None || End == EEnd::None)
		{
			return;
		}
		const FAshLook Now = Evaluate(S, T, NowMs);
		S.End = End;
		S.EndStartMs = NowMs;
		S.EndFromGrey = Now.Grey;
		S.EndFromDissolve = Now.Dissolve;
		S.bStoryHeld = false;
	}

	/** EvMonsterFeed: the body drains toward the feeder from StartMs to DueMs. */
	inline void BeginFeed(FHuskState& S, const FTimeline& T, double StartMs, double DueMs)
	{
		if (S.End != EEnd::None || S.bFeeding)
		{
			return;
		}
		const FAshLook Now = Evaluate(S, T, StartMs);
		S.bFeeding = true;
		S.FeedStartMs = StartMs;
		S.FeedDueMs = (std::max)(StartMs, DueMs);
		S.FeedFromGrey = Now.Grey;
		S.FeedFromDissolve = Now.Dissolve;
		S.bStoryHeld = false;
	}

	/** Raised: the risen monster's ash starts where the husk's was (its own Rise reverses it). */
	struct FRiseStart
	{
		double Grey = 0.0;
		double Dissolve = 0.0;
	};

	/** The reversal during the Rise: from the husk's look to none over RiseFraction x RiseMs (sim ms from StartMs). */
	struct FRiseAsh
	{
		double Grey = 0.0;
		double Dissolve = 0.0;
		bool bDone = true;
	};

	inline FRiseAsh RiseAsh(const FRiseStart& Start, double StartMs, double RiseMs, double Fraction, double NowMs)
	{
		FRiseAsh Out;
		if (!(Start.Grey > 0.0) && !(Start.Dissolve > 0.0))
		{
			return Out;   // a body raised before it greyed: nothing to reverse
		}
		const double Span = (std::max)(1.0, RiseMs * Clamp01(Fraction));
		const double K = Ramp(NowMs, StartMs, Span);
		Out.Grey = Start.Grey * (1.0 - K);
		Out.Dissolve = Start.Dissolve * (1.0 - K);
		Out.bDone = K >= 1.0;
		return Out;
	}

	// =================================================================================================================
	// Removals and recipes
	// =================================================================================================================

	struct FRemovalPlan
	{
		EEnd End = EEnd::Expired;
		/** The actor goes now (raised: the risen monster stands where it lay). */
		bool bRemoveNow = false;
		/** The recipe played at the removal ("" = none). */
		std::string Recipe;
		/** The burn's flare takes the fire's colour (BurnColor). */
		bool bFireColor = false;
	};

	inline FRemovalPlan PlanRemoval(abyss::CorpseRemoveReason Reason, const FCorpseBlock& Block)
	{
		FRemovalPlan Out;
		Out.End = EndOf(Reason);
		switch (Out.End)
		{
		case EEnd::Expired: Out.Recipe = Block.ExpireRecipe; break;
		case EEnd::Raised: Out.bRemoveNow = true; break;   // monster.raise plays on EvMonsterRaised
		case EEnd::Consumed: break;                        // corpse.consume / the Feed's recipe led here
		case EEnd::Burned: Out.Recipe = Block.BurnRecipe; Out.bFireColor = true; break;
		case EEnd::None: break;
		}
		return Out;
	}

	/** The fire's colour of a burn: the hero's ember (or its own killing blow's impact colour), the breach's violet. */
	inline constexpr uint32_t HeroFireRgb = 0xFF7A2A;
	inline constexpr uint32_t BreachFireRgb = 0xB02FD0;

	enum class EFireSide : unsigned char
	{
		Unknown,   // no source named (a direct burn): the recipe's own grey
		Hero,      // the hero or its beast
		Enemy,     // a monster's strike, hazard or on-death effect (or a monster's fire killing blow)
	};

	/** The side of EvCorpseRemoved::burner: the hero or its active beast (PetId, kNoEntity = none), else a monster. */
	inline EFireSide FireSideOf(abyss::EntityId Burner, abyss::EntityId PetId)
	{
		if (Burner == abyss::kNoEntity)
		{
			return EFireSide::Unknown;
		}
		return Burner == abyss::kHeroEntityId || (PetId != abyss::kNoEntity && Burner == PetId) ? EFireSide::Hero : EFireSide::Enemy;
	}

	struct FBurnColor
	{
		bool bHas = false;
		uint32_t Rgb = 0;
	};

	/**
	 * The flare's colour from the fire's side (DR5: the hero's fire is ember, the enemy's breach violet): a hero-side burn
	 * takes the impact colour of the fire killing blow that made this very body (KillImpactRgb: its EvHit this frame, 0 =
	 * none / not a kill burn), else the hero's ember; an enemy burn is violet whatever its hit's colour; an unknown source
	 * keeps the recipe's grey (no colour).
	 */
	inline FBurnColor BurnColor(EFireSide Side, uint32_t KillImpactRgb)
	{
		switch (Side)
		{
		case EFireSide::Hero: return { true, KillImpactRgb != 0 ? KillImpactRgb : HeroFireRgb };
		case EFireSide::Enemy: return { true, BreachFireRgb };
		case EFireSide::Unknown: break;
		}
		return {};
	}

	// =================================================================================================================
	// The story hold
	// =================================================================================================================

	/**
	 * Whether a body whose block holds for a story beat keeps its pose now: while that beat plays, or while the story
	 * director is busy (beats queued or playing) and the body's beat has not ended since the body appeared.
	 */
	inline bool StoryHoldActive(const FCorpseBlock& Block, std::string_view PlayingBeat, bool bStoryBusy, bool bBeatEndedSinceAdded)
	{
		if (!Block.bStoryHold)
		{
			return false;
		}
		if (!PlayingBeat.empty() && PlayingBeat == Block.StoryBeat)
		{
			return true;
		}
		return bStoryBusy && !bBeatEndedSinceAdded;
	}

	// =================================================================================================================
	// Reconcile and the visible cap
	// =================================================================================================================

	struct FReconcile
	{
		/** In the snapshot, not known: a late view (drawn as a husk born at its death time). */
		std::vector<uint32_t> Add;
		/** Known and live, gone from the snapshot without a removal event: dissolved like an expiry. */
		std::vector<uint32_t> Remove;
	};

	/** Known: the live (not removed) entries of the layer; Present: the snapshot's corpse ids. */
	inline void Reconcile(const std::vector<uint32_t>& Known, const std::vector<uint32_t>& Present, FReconcile& Out)
	{
		Out.Add.clear();
		Out.Remove.clear();
		for (const uint32_t Id : Present)
		{
			if (std::find(Known.begin(), Known.end(), Id) == Known.end())
			{
				Out.Add.push_back(Id);
			}
		}
		for (const uint32_t Id : Known)
		{
			if (std::find(Present.begin(), Present.end(), Id) == Present.end())
			{
				Out.Remove.push_back(Id);
			}
		}
	}

	/** Husks drawn at once per quality tier (the core keeps 24 kill bodies + the map's seeds; overflow only). */
	inline int HuskCap(int Tier, bool bMobile)
	{
		if (!bMobile)
		{
			return 32;
		}
		return Tier <= 0 ? 12 : (Tier == 1 ? 20 : 32);
	}

	/**
	 * Whether a husk (a held kill or a posed seed) casts the sun's CSM (2G review X4): on desktop as the living do; on
	 * mobile never - it keeps its blob - so mobile high's 32 lying bodies (body, weapons, kettle helm) stay out of the
	 * shadow depth pass, and a seed on mobile low / balanced no longer draws its blob at the under-CSM alpha with no CSM.
	 */
	inline bool HuskCastsSunShadow(bool bMobile)
	{
		return !bMobile;
	}

	struct FHuskEntry
	{
		uint32_t Id = 0;
		double DeathMs = 0.0;
		bool bSeeded = false;
		bool bStoryHeld = false;
	};

	/**
	 * The live husks beyond the cap, to retire (dissolve early, presentation only): seeds and story-held bodies are kept
	 * first, then the newest (death time, then id); the rest go oldest first. Out is cleared and filled; Scratch is the
	 * caller's reusable sort buffer (nothing is allocated per frame once both have grown; untouched under the cap).
	 */
	inline void OverflowHusks(const std::vector<FHuskEntry>& Entries, int Cap, std::vector<FHuskEntry>& Scratch,
		std::vector<uint32_t>& Out)
	{
		Out.clear();
		if (Cap < 0 || Entries.size() <= static_cast<size_t>(Cap))
		{
			return;
		}
		Scratch.assign(Entries.begin(), Entries.end());
		std::stable_sort(Scratch.begin(), Scratch.end(), [](const FHuskEntry& A, const FHuskEntry& B)
		{
			const bool PinA = A.bSeeded || A.bStoryHeld;
			const bool PinB = B.bSeeded || B.bStoryHeld;
			if (PinA != PinB) return PinA;
			if (A.DeathMs != B.DeathMs) return A.DeathMs > B.DeathMs;
			return A.Id > B.Id;
		});
		for (size_t I = static_cast<size_t>(Cap); I < Scratch.size(); ++I)
		{
			Out.push_back(Scratch[I].Id);
		}
	}

	/** A corpse-holding death fades like today's when no entry adopted it within the clip plus this margin. */
	inline bool AdoptionTimedOut(double SinceDeathMs, double ClipMs)
	{
		return SinceDeathMs > (std::max)(0.0, ClipMs) + AdoptTimeoutMs;
	}

	// =================================================================================================================
	// Breach-blood: the tar puddle (no registry entry, drawn at the death by def)
	// =================================================================================================================

	/** The shipped decal sprite the puddle is drawn with (vfx_recipes.json sprites.Puddle, translucent). */
	inline constexpr std::string_view PuddleSprite = "Puddle";
	/** The decal's lift over the ground (cm: the shipped puddle decals' 8 cm). */
	inline constexpr double PuddleLiftCm = 8.0;

	struct FPuddle
	{
		bool bValid = false;
		uint32_t Rgb = 0x2A0E14;
		double Alpha = 0.7;
		/** radiusFactor x the body's half-width W (BodyHalfWidthCm) x the body's drawn scale. */
		double RadiusCm = 0.0;
		double DriesMs = 3000.0;
		/** The body melts into it over its Death clip (to the hold frame), then it dries. */
		double FormMs = 0.0;
	};

	/**
	 * The body half-width W the art's radius factors scale (cm at scale 1): `footprintRadiusCm` (W x 100, the manifest's
	 * convention: `blobShadowRadiusCm` = W x fx.blobShadow.radiusFactorOfWidth), else blobShadowRadiusCm over that
	 * factor; 0 = unknown. Not the husk's radiusCm (the melted last frame's footprint, a different base).
	 */
	inline double BodyHalfWidthCm(const abyss::JsonValue& AssetJson)
	{
		const double Footprint = AssetJson.Get("footprintRadiusCm").AsDouble(0.0);
		if (Footprint > 0.0)
		{
			return Footprint;
		}
		const double Blob = AssetJson.Get("blobShadowRadiusCm").AsDouble(0.0);
		const double Factor = AssetJson.Get("fx").Get("blobShadow").Get("radiusFactorOfWidth").AsDouble(0.0);
		return Blob > 0.0 && Factor > 0.0 ? Blob / Factor : 0.0;
	}

	/**
	 * A breach body's puddle (corpse block look `puddle`, no registry entry): the Death clip's `puddleDecal` (GetAssetJson's
	 * `clips.<deathClip>`, else the manifest asset's `anims[]`), else `fx.puddle`; its radius is the decal's `radiusFactor`
	 * (`radiusFactorOfWidth`) x BodyHalfWidthCm x BodyScale (the body as drawn). Invalid for every other look (the W1
	 * slime's puddle decal has no corpse block: it keeps today's death) and for art without a half-width.
	 */
	inline FPuddle ParsePuddle(const abyss::JsonValue& AssetJson, const FCorpseBlock& Block, double BodyScale)
	{
		FPuddle Out;
		const double Width = BodyHalfWidthCm(AssetJson);
		if (Block.Look != ELook::Puddle || !(Width > 0.0))
		{
			return Out;
		}
		const abyss::JsonValue* P = &AssetJson.Get("clips").Get(Block.DeathClip).Get("puddleDecal");
		if (!P->IsObject())
		{
			for (const abyss::JsonValue& Clip : AssetJson.Get("anims").Items())
			{
				if (Clip.Get("name").AsString() == Block.DeathClip && Clip.Get("puddleDecal").IsObject())
				{
					P = &Clip.Get("puddleDecal");
					break;
				}
			}
		}
		if (!P->IsObject())
		{
			P = &AssetJson.Get("fx").Get("puddle");
		}
		if (!P->IsObject() || !ParseHex(P->Get("color").AsString(), Out.Rgb))
		{
			return Out;
		}
		Out.Alpha = std::clamp(P->Get("alpha").AsDouble(Out.Alpha), 0.0, 1.0);
		const abyss::JsonValue& Factor = P->Get("radiusFactor").IsNumber() ? P->Get("radiusFactor") : P->Get("radiusFactorOfWidth");
		Out.RadiusCm = (std::max)(0.0, Factor.AsDouble(1.0)) * Width * (BodyScale > 0.0 ? BodyScale : 1.0);
		Out.DriesMs = PositiveOr(P->Get("driesMs"), Out.DriesMs);
		Out.FormMs = (std::max)(0.0, Block.HoldFromMs);
		Out.bValid = Out.RadiusCm > 0.0 && Out.Alpha > 0.0;
		return Out;
	}

	/**
	 * The puddle as one recipe object for UAbyssVfxSystem::PlayRuntimeRecipe (the shipped `decal` emitter and Puddle
	 * sprite; no art recipe carries it): one translucent ground quad 2 x RadiusCm across at PuddleLiftCm, a random turn,
	 * fading in over FormMs (the body melting into it), then drying linearly to nothing over DriesMs.
	 */
	inline std::string PuddleRecipeJson(const FPuddle& P)
	{
		auto Num = [](double V)
		{
			char Buffer[32];
			std::snprintf(Buffer, sizeof(Buffer), "%.6g", V);
			return std::string(Buffer);
		};
		char Color[8];
		std::snprintf(Color, sizeof(Color), "#%06X", static_cast<unsigned>(P.Rgb & 0xFFFFFFu));
		const double LifeMs = (std::max)(1.0, P.FormMs + P.DriesMs);
		return std::string("{\"palette\":\"event\",\"layers\":[{\"emitter\":\"decal\",\"sprite\":\"") + std::string(PuddleSprite)
			+ "\",\"blend\":\"translucent\",\"at\":\"point\",\"height\":" + Num(PuddleLiftCm) + ",\"count\":1,\"sizeCm\":"
			+ Num(2.0 * P.RadiusCm) + ",\"grow\":1,\"sizePow\":1,\"lifeMs\":" + Num(LifeMs) + ",\"alpha\":[" + Num(P.Alpha)
			+ ",0],\"fadeIn\":" + Num(P.FormMs / LifeMs) + ",\"alphaPow\":1,\"rotationDeg\":[0,360],\"color\":\"" + Color
			+ "\"}]}";
	}
}
