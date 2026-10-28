// How a character actor presents the core's animation actions (P5; K1 monster actions, implementation-plan.md 3 K1 /
// 4 U1-U2; SimTypes.h AnimAction contract: "each plays the clip of the same name; an asset without that clip keeps its
// current pose"): the action kind an EvPlayAnim starts, the clip name it looks up, the anim_timing.json blend state, which
// actions a snapshot hold keeps looping (MonsterView::holdAction) and the order in which death, actions, holds, the stun
// pose and locomotion own the body.
//
// UE-free and header-only (std + AbyssCore public headers), so the rules are unit-tested outside the engine
// (unreal/PresentationTests, anim_test.cpp); AAbyssCharacterActor maps the kinds onto EAbyssCharacterAction and plays.
//
// Dark remake U2 (2G): the play rate of an action clip (ActionPlayRate: the beat on the core's contact, except a looping
// clip whose windup outlasts it, which loops at rate 1 with a beat on the contact: the drummer's 2000 ms drum on a
// 1000 ms Cast01 loop) and a death clip's runtime block (ParseDeathRuntime: the Cinderbloat's fuse plays its Death at
// rate 1 for 900 ms, standing, unfaded until FX_Burst).
#pragma once

#include <algorithm>
#include <cmath>
#include <string_view>

#include "abyss/base/Json.h"
#include "abyss/sim/SimTypes.h"

namespace AbyssAnimActionRules
{
	/** What an EvPlayAnim starts on the actor (AAbyssCharacterActor's EAbyssCharacterAction, one to one). */
	enum class EActionKind : unsigned char
	{
		None,        // Idle / Walk: locomotion comes from the snapshot
		Attack,      // Attack, Attack02: contact beat, wind-up telegraph, "attack" blend
		Cast,        // Cast, Cast02: release beat
		Hurt,
		Dodge,
		Death,
		Signature,
		Work,
		Talk,
		Emote,       // Roar, Taunt, Stun, Rise: a named clip without a beat ("cast" blend)
	};

	inline EActionKind KindOf(abyss::AnimAction Action)
	{
		switch (Action)
		{
		case abyss::AnimAction::Idle:
		case abyss::AnimAction::Walk: return EActionKind::None;
		case abyss::AnimAction::Attack:
		case abyss::AnimAction::Attack02: return EActionKind::Attack;
		case abyss::AnimAction::Cast:
		case abyss::AnimAction::Cast02: return EActionKind::Cast;
		case abyss::AnimAction::Hurt: return EActionKind::Hurt;
		case abyss::AnimAction::Dodge: return EActionKind::Dodge;
		case abyss::AnimAction::Death: return EActionKind::Death;
		case abyss::AnimAction::Signature: return EActionKind::Signature;
		case abyss::AnimAction::Work: return EActionKind::Work;
		case abyss::AnimAction::Talk: return EActionKind::Talk;
		case abyss::AnimAction::Roar:
		case abyss::AnimAction::Taunt:
		case abyss::AnimAction::Stun:
		case abyss::AnimAction::Rise: return EActionKind::Emote;
		}
		return EActionKind::None;
	}

	/**
	 * The manifest clip an action of the K1 set plays (the clip of the same name); empty for the older actions, whose
	 * clip choice (Attack01 variants, skill signatures, Cast01, Work / Talk overrides) stays with the actor.
	 */
	inline std::string_view NamedClip(abyss::AnimAction Action)
	{
		switch (Action)
		{
		case abyss::AnimAction::Roar: return "Roar";
		case abyss::AnimAction::Taunt: return "Taunt";
		case abyss::AnimAction::Stun: return "Stun";
		case abyss::AnimAction::Attack02: return "Attack02";
		case abyss::AnimAction::Cast02: return "Cast02";
		case abyss::AnimAction::Rise: return "Rise";
		default: return {};
		}
	}

	/** anim_timing.json transition state the action blends into (the attack / dodge / hurt rows, "cast" for the rest). */
	inline const char* BlendState(EActionKind Kind)
	{
		switch (Kind)
		{
		case EActionKind::Attack: return "attack";
		case EActionKind::Dodge: return "dodge";
		case EActionKind::Hurt: return "hurt";
		default: return "cast";
		}
	}

	/**
	 * A pose the snapshot can hold: Taunt (the mini-boss while its pre-fight dialogue holds it) and Stun. Idle, Walk and
	 * Death are never holds (Idle is MonsterView::holdAction's "none").
	 */
	inline bool IsHoldAction(abyss::AnimAction Action)
	{
		return Action == abyss::AnimAction::Taunt || Action == abyss::AnimAction::Stun;
	}

	/**
	 * Hold and stun poses loop for as long as the state lasts, whatever the clip's own loop flag (the Corpseherd's
	 * Taunt loops, the goblin shaman's does not; Stun clips are authored as loops).
	 */
	inline bool ForcesLoop(abyss::AnimAction Action)
	{
		return IsHoldAction(Action);
	}

	/**
	 * Whether an action's clip is stretched to the event's durationMs (no contact beat): one-shots are; a pose that
	 * loops (Stun) keeps its authored rate for the whole duration instead of one slowed cycle.
	 */
	inline bool StretchesToDuration(abyss::AnimAction Action)
	{
		return Action != abyss::AnimAction::Attack && Action != abyss::AnimAction::Attack02 && !ForcesLoop(Action);
	}

	// =================================================================================================================
	// Play rates (P5; dark remake U2)
	// =================================================================================================================

	/** The slowest and fastest an action clip plays (a beat further off is a data problem, not a reason to crawl). */
	inline constexpr float MinPlayRate = 0.25f;
	inline constexpr float MaxPlayRate = 4.f;

	/** What OnPlayAnim knows about the clip and the event. */
	struct FRateInput
	{
		abyss::AnimAction Action = abyss::AnimAction::Attack;
		bool bHasClip = false;
		float BeatSec = -1.f;      // FAbyssArtClip::GetBeatSec (Contact, else Release; < 0 none)
		float LengthSec = 0.f;
		bool bLoopClip = false;    // the clip is authored as a loop
		double ContactMs = 0.0;    // EvPlayAnim::contactMs (the beat offset; 0 none)
		double DurationMs = 0.0;   // EvPlayAnim::durationMs
		double EventRate = 1.0;    // EvPlayAnim::playRate
	};

	struct FRatePlan
	{
		float Rate = 1.f;
		/** Loop the clip at Rate for DurationSec (a channel on a looping clip) whatever its own loop flag. */
		bool bLoopThrough = false;
		/** Where the clip starts (seconds of clip time, before the event's elapsed time): puts a loop's beat on the contact. */
		float StartPhaseSec = 0.f;
		/** > 0: the action's length (seconds) instead of the event's durationMs. */
		float DurationSec = 0.f;
	};

	/**
	 * The rate an action clip plays at:
	 * * a beat and a contact: beat / contact (the authored Contact / Release lands on the core's beat), clamped to
	 *   [MinPlayRate, MaxPlayRate] - except a looping clip whose contact comes after its whole length (the drummer's
	 *   war_drum: a 2000 ms windup on the 1000 ms Cast01 loop, Release 125 ms, which ComputeAbilityTiming slows to
	 *   0.0625): it loops at rate 1, started so that a beat falls on the contact, and lasts until the contact plus the
	 *   rest of the clip after its beat (the core's durationMs is the whole clip at the slowed rate: 16 s for the drum);
	 * * no beat: a one-shot stretched to durationMs when the action stretches (StretchesToDuration), else the event's rate
	 *   (Consume: rate 1, looped by its own flag for its 2000 ms channel).
	 */
	inline FRatePlan ActionPlayRate(const FRateInput& In)
	{
		FRatePlan Plan;
		const float EventRate = In.EventRate > 0.0 ? static_cast<float>(In.EventRate) : 1.f;
		if (!In.bHasClip)
		{
			Plan.Rate = EventRate;
			return Plan;
		}
		const float ContactSec = static_cast<float>(In.ContactMs / 1000.0);
		if (In.ContactMs > 0.0 && In.BeatSec > 0.f)
		{
			if (In.bLoopClip && In.LengthSec > 0.f && ContactSec > In.LengthSec + 1e-4f)
			{
				Plan.Rate = 1.f;
				Plan.bLoopThrough = true;
				Plan.StartPhaseSec = std::fmod(In.BeatSec - ContactSec, In.LengthSec);
				if (Plan.StartPhaseSec < 0.f)
				{
					Plan.StartPhaseSec += In.LengthSec;
				}
				Plan.DurationSec = ContactSec + (In.LengthSec - In.BeatSec);
				return Plan;
			}
			Plan.Rate = std::clamp(In.BeatSec / ContactSec, MinPlayRate, MaxPlayRate);
			return Plan;
		}
		if (In.DurationMs > 0.0 && In.LengthSec > 0.f && StretchesToDuration(In.Action))
		{
			Plan.Rate = std::clamp(In.LengthSec / static_cast<float>(In.DurationMs / 1000.0), MinPlayRate, MaxPlayRate);
			return Plan;
		}
		Plan.Rate = std::clamp(EventRate, MinPlayRate, MaxPlayRate);
		return Plan;
	}

	/** Clip time ElapsedSec after the start (rate applied) of a plan: wrapped into the clip for a loop-through. */
	inline float PlanClipTime(const FRatePlan& Plan, float ElapsedSec, float LengthSec)
	{
		float T = Plan.StartPhaseSec + ElapsedSec * Plan.Rate;
		if (Plan.bLoopThrough && LengthSec > 0.f)
		{
			T = std::fmod(T, LengthSec);
		}
		return T;
	}

	// =================================================================================================================
	// Death runtime (manifest: an asset's `fx.fuse.runtime`, else its Death clip's `runtime`)
	// =================================================================================================================

	/**
	 * How a death clip plays when the art asks for it (the Cinderbloat: its Death is the fuse, FX_Fuse @0 and FX_Burst
	 * @800 on the core's 800 ms burst): the clip's own length at PlayRate (900 ms at 1, not the rig preset's 500 ms that
	 * put FX_Burst at 444 ms), no throw / hop / spin (it stands under its ring decal) and no body fade before
	 * BodyFadeNotBeforeMs. Assets without the block keep today's death (every chapter but the Cinderbloat).
	 */
	struct FDeathRuntime
	{
		bool bValid = false;
		double DeathDurationMs = 0.0;
		double PlayRate = 1.0;
		double BodyFadeNotBeforeMs = 0.0;
		bool bThrow = true;
		bool bHop = true;
		bool bSpin = true;
	};

	/**
	 * AssetJson: FAbyssArtManifest::GetAssetJson (`fx`, `clips.<name>.runtime`) or the manifest's asset object itself
	 * (`fx`, `anims[]`). The block needs a positive deathDurationMs; the rest defaults to today's (rate 1, throw / hop /
	 * spin on, no fade floor).
	 */
	inline FDeathRuntime ParseDeathRuntime(const abyss::JsonValue& AssetJson)
	{
		FDeathRuntime Out;
		const abyss::JsonValue* Block = &AssetJson.Get("fx").Get("fuse").Get("runtime");
		if (!Block->IsObject())
		{
			Block = &AssetJson.Get("clips").Get("Death").Get("runtime");
		}
		if (!Block->IsObject())
		{
			for (const abyss::JsonValue& Clip : AssetJson.Get("anims").Items())
			{
				if (Clip.Get("name").AsString() == "Death" && Clip.Get("runtime").IsObject())
				{
					Block = &Clip.Get("runtime");
					break;
				}
			}
		}
		if (!Block->IsObject())
		{
			return Out;
		}
		const double Duration = Block->Get("deathDurationMs").AsDouble(0.0);
		if (!(Duration > 0.0))
		{
			return Out;
		}
		Out.bValid = true;
		Out.DeathDurationMs = Duration;
		const double Rate = Block->Get("playRate").AsDouble(1.0);
		Out.PlayRate = Rate > 0.0 ? Rate : 1.0;
		Out.BodyFadeNotBeforeMs = (std::max)(0.0, Block->Get("bodyFadeNotBeforeMs").AsDouble(0.0));
		Out.bThrow = Block->Get("deathThrow").AsBool(true);
		Out.bHop = Block->Get("deathHop").AsBool(true);
		Out.bSpin = Block->Get("deathSpin").AsBool(true);
		return Out;
	}

	/** Who owns the body this frame, highest first (death > action > hold > stun > move > idle). */
	enum class ELayer : unsigned char
	{
		Death,
		Action,
		Hold,
		Stun,
		Move,
		Idle,
	};

	struct FLayerInputs
	{
		bool bDying = false;
		bool bInAction = false;     // an EvPlayAnim action is still running
		bool bHoldClip = false;     // the snapshot holds a pose and the asset has its clip
		bool bStunClip = false;     // stunned (status) and the asset has a Stun clip
		bool bMoving = false;
	};

	inline ELayer PickLayer(const FLayerInputs& In)
	{
		if (In.bDying)
		{
			return ELayer::Death;
		}
		if (In.bInAction)
		{
			return ELayer::Action;
		}
		if (In.bHoldClip)
		{
			return ELayer::Hold;
		}
		if (In.bStunClip)
		{
			return ELayer::Stun;
		}
		return In.bMoving ? ELayer::Move : ELayer::Idle;
	}
}
