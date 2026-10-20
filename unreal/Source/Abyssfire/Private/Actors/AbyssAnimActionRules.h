// How a character actor presents the core's animation actions (P5; K1 monster actions, implementation-plan.md 3 K1 /
// 4 U1-U2; SimTypes.h AnimAction contract: "each plays the clip of the same name; an asset without that clip keeps its
// current pose"): the action kind an EvPlayAnim starts, the clip name it looks up, the anim_timing.json blend state, which
// actions a snapshot hold keeps looping (MonsterView::holdAction) and the order in which death, actions, holds, the stun
// pose and locomotion own the body.
//
// UE-free and header-only (std + AbyssCore public headers), so the rules are unit-tested outside the engine
// (unreal/PresentationTests, anim_test.cpp); AAbyssCharacterActor maps the kinds onto EAbyssCharacterAction and plays.
#pragma once

#include <string_view>

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
