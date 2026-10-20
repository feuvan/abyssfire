// The character actor's action map (Source/Abyssfire/Private/Actors/AbyssAnimActionRules.h): K1 monster actions to clips,
// blend states, holds, and the layer priority death > action > hold > stun > move > idle.
#include <string_view>

#include "doctest/doctest.h"

#include "abyss/base/Enums.h"
#include "abyss/sim/SimTypes.h"

#include "Actors/AbyssAnimActionRules.h"

TEST_SUITE("anim")
{
	using namespace AbyssAnimActionRules;
	using abyss::AnimAction;

	TEST_CASE("every AnimAction has a kind; the K1 actions play the clip of the same name")
	{
		struct FRow
		{
			AnimAction Action;
			EActionKind Kind;
			std::string_view Clip;
			const char* Blend;
		};
		const FRow Rows[] = {
			{ AnimAction::Idle, EActionKind::None, "", "cast" },
			{ AnimAction::Walk, EActionKind::None, "", "cast" },
			{ AnimAction::Attack, EActionKind::Attack, "", "attack" },
			{ AnimAction::Cast, EActionKind::Cast, "", "cast" },
			{ AnimAction::Hurt, EActionKind::Hurt, "", "hurt" },
			{ AnimAction::Dodge, EActionKind::Dodge, "", "dodge" },
			{ AnimAction::Death, EActionKind::Death, "", "cast" },
			{ AnimAction::Signature, EActionKind::Signature, "", "cast" },
			{ AnimAction::Work, EActionKind::Work, "", "cast" },
			{ AnimAction::Talk, EActionKind::Talk, "", "cast" },
			{ AnimAction::Roar, EActionKind::Emote, "Roar", "cast" },
			{ AnimAction::Taunt, EActionKind::Emote, "Taunt", "cast" },
			{ AnimAction::Stun, EActionKind::Emote, "Stun", "cast" },
			{ AnimAction::Attack02, EActionKind::Attack, "Attack02", "attack" },
			{ AnimAction::Cast02, EActionKind::Cast, "Cast02", "cast" },
			{ AnimAction::Rise, EActionKind::Emote, "Rise", "cast" },
		};
		// The table covers the whole enum (a new AnimAction must be mapped here and in the actor).
		CHECK(sizeof(Rows) / sizeof(Rows[0]) == abyss::EnumCount<AnimAction>());
		for (const FRow& Row : Rows)
		{
			CAPTURE(abyss::EnumName(Row.Action));
			CHECK(KindOf(Row.Action) == Row.Kind);
			CHECK(NamedClip(Row.Action) == Row.Clip);
			CHECK(std::string_view(BlendState(KindOf(Row.Action))) == Row.Blend);
		}
		// The core's spellings (story focus `anim`, AutoPilot) name the same clips, capitalised.
		CHECK(abyss::EnumName(AnimAction::Roar) == "roar");
		CHECK(abyss::EnumName(AnimAction::Attack02) == "attack02");
	}

	TEST_CASE("holds: Taunt and Stun loop; Idle / Walk / Death are never holds")
	{
		CHECK(IsHoldAction(AnimAction::Taunt));
		CHECK(IsHoldAction(AnimAction::Stun));
		for (const AnimAction Action : { AnimAction::Idle, AnimAction::Walk, AnimAction::Death, AnimAction::Roar,
				 AnimAction::Rise, AnimAction::Attack, AnimAction::Attack02, AnimAction::Cast02 })
		{
			CAPTURE(abyss::EnumName(Action));
			CHECK_FALSE(IsHoldAction(Action));
			CHECK_FALSE(ForcesLoop(Action));
		}
		CHECK(ForcesLoop(AnimAction::Taunt));   // the goblin shaman's Taunt clip is not authored as a loop
		// Beat-less one-shots stretch to the event's duration; attacks keep the contact rate; loops keep their rate.
		CHECK(StretchesToDuration(AnimAction::Roar));
		CHECK(StretchesToDuration(AnimAction::Rise));
		CHECK(StretchesToDuration(AnimAction::Cast02));
		CHECK_FALSE(StretchesToDuration(AnimAction::Attack));
		CHECK_FALSE(StretchesToDuration(AnimAction::Attack02));
		CHECK_FALSE(StretchesToDuration(AnimAction::Stun));
		CHECK_FALSE(StretchesToDuration(AnimAction::Taunt));
	}

	TEST_CASE("layer priority: death > action > hold > stun > move > idle")
	{
		FLayerInputs In;
		CHECK(PickLayer(In) == ELayer::Idle);
		In.bMoving = true;
		CHECK(PickLayer(In) == ELayer::Move);
		In.bStunClip = true;
		CHECK(PickLayer(In) == ELayer::Stun);
		In.bHoldClip = true;
		CHECK(PickLayer(In) == ELayer::Hold);
		In.bInAction = true;
		CHECK(PickLayer(In) == ELayer::Action);
		In.bDying = true;
		CHECK(PickLayer(In) == ELayer::Death);   // a hold never suppresses a death
		FLayerInputs HoldOnly;
		HoldOnly.bHoldClip = true;
		HoldOnly.bDying = true;
		CHECK(PickLayer(HoldOnly) == ELayer::Death);
		FLayerInputs NoClip;   // a hold without its clip keeps the current pose: locomotion as before
		NoClip.bMoving = false;
		CHECK(PickLayer(NoClip) == ELayer::Idle);
	}
}
