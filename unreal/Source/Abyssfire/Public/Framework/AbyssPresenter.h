// IAbyssPresenter: implemented by every actor that presents one core entity (hero, monsters, NPCs, pet; optionally
// props / ground items). The world agent's actors implement it and register themselves in UAbyssActorRegistry under
// their EntityId; UAbyssSimDriver then routes the entity-addressed events to them (no subscription needed).
//
// Routing done by the driver (in event order, before the router broadcast of the same event):
//   EvPlayAnim{entity}               -> OnPlayAnim
//   EvHit{target, source}            -> target: OnHitTaken, then ApplyHitStop(targetStopMs) when the target is a monster
//                                       hit by a resolved, non-killing, non-tick hit (combat-feel.md 11.1);
//                                       source: OnHitDealt, then ApplyHitStop(attackerStopMs) when > 0 (the core has
//                                       already resolved 11.1: hero basic attack only, monster swings x0.6, ...)
//   EvStatusApplied / Expired{target}-> OnStatusApplied / OnStatusExpired
//   EvEntityTeleported{id}           -> OnTeleported (snap the interpolation, play blink VFX)
//   EvMonsterAttackCancelled{monster}-> OnAttackCancelled (drop the telegraph)
//   EvMonsterRenamed{monster}        -> OnRenamed
// Everything else (hero-only events such as EvDodgeStarted / EvHeroDash / EvHeroDied, VFX, UI) goes through
// FAbyssEventRouter. Spawn / despawn / per-frame transforms go through IAbyssWorldView.
//
// Hit-stop (combat-feel.md 11.2) is PER ACTOR: pause this actor's animation (and its own procedural motion) for
// max(remaining, DurationMs) real ms; never global time dilation; the core sim is unaffected.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "abyss/sim/Events.h"

#include "Framework/AbyssTypes.h"

#include "AbyssPresenter.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UAbyssPresenter : public UInterface
{
	GENERATED_BODY()
};

class ABYSSFIRE_API IAbyssPresenter
{
	GENERATED_BODY()

public:
	/** Play an action (P5: the core owns timing; contactMs / releaseMs are relative to startMs, sim ms). */
	virtual void OnPlayAnim(const abyss::EvPlayAnim& Event, const FAbyssFrameInfo& Frame) {}
	/** This entity was hit (or missed: Event.dodged). Flash / recoil / pain tint / death handled from the payload. */
	virtual void OnHitTaken(const abyss::EvHit& Event) {}
	/** This entity's attack landed on Event.target. */
	virtual void OnHitDealt(const abyss::EvHit& Event) {}
	/** Per-actor animation freeze for max(remaining, DurationMs) of real time (combat-feel.md 11.2). */
	virtual void ApplyHitStop(float DurationMs) {}
	virtual void OnStatusApplied(const abyss::EvStatusApplied& Event) {}
	virtual void OnStatusExpired(const abyss::EvStatusExpired& Event) {}
	virtual void OnTeleported(const abyss::EvEntityTeleported& Event) {}
	virtual void OnAttackCancelled(const abyss::EvMonsterAttackCancelled& Event) {}
	virtual void OnRenamed(const abyss::EvMonsterRenamed& Event) {}
};
