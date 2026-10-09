// IAbyssWorldView: the world builder (world agent: World/, Actors/, Anim/, Vfx/, Camera/). Exactly one per game world,
// registered with UAbyssSimDriver::RegisterWorldView (typically a UWorldSubsystem that calls it from Initialize via
// Collection.InitializeDependency<UAbyssSimDriver>()).
//
// Call order inside one dispatch (UAbyssSimDriver), per event in core emission order:
//   EvEntitySpawned  -> SpawnEntity            BEFORE presenters / router subscribers see the event
//   EvZone{Entered}  -> BuildZone              BEFORE subscribers
//   EvEntityDespawned-> DespawnEntity          AFTER presenters / subscribers (VFX / UI can still find the actor)
//   EvZone{Exited}   -> ClearZone              AFTER subscribers
// then once per frame, after the event loop: SyncFrame (before the UI root and router OnFrame).
//
// Facts about the core's order (SimZone.cpp): on a zone change the batch is
//   despawns (ZoneUnload) ... EvZone{Exited} ... EvEntitySpawned (hero, NPCs, monsters, props, pet) ... EvZone{Entered}
// so SpawnEntity runs BEFORE BuildZone of the new zone: create the actor, register it, and place it on the terrain in
// SyncFrame. The hero (kHeroEntityId) is re-announced by EvEntitySpawned on every zone entry without a despawn:
// SpawnEntity must reuse an existing actor for an id that is already registered. The snapshot passed in is always
// the end-of-frame state (the new zone during a zone change).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "abyss/sim/Events.h"
#include "abyss/sim/Snapshot.h"

#include "Framework/AbyssTypes.h"

#include "AbyssWorldView.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UAbyssWorldView : public UInterface
{
	GENERATED_BODY()
};

class ABYSSFIRE_API IAbyssWorldView
{
	GENERATED_BODY()

public:
	/** Build terrain / props / water / camps / exits / lights / sky / post-process from Snap.zone (grid valid). */
	virtual void BuildZone(const abyss::Snapshot& Snap) = 0;
	/** Tear the zone down (EvZone{Exited}, or the session ended). Must also unregister / destroy zone actors. */
	virtual void ClearZone() = 0;
	/** Create (or reuse) the actor for an entity and register it in UAbyssActorRegistry. */
	virtual void SpawnEntity(const abyss::EvEntitySpawned& Event, const abyss::Snapshot& Snap) = 0;
	/** Remove the entity's actor (Event.reason: Died -> corpse / death anim, PickedUp, ZoneUnload, ...) and unregister it. */
	virtual void DespawnEntity(const abyss::EvEntityDespawned& Event) = 0;
	/** Per frame: interpolated transforms (Lerp(prevPos, pos, Frame.Alpha)), facing, markers, camera follow target. */
	virtual void SyncFrame(const abyss::Snapshot& Snap, const FAbyssFrameInfo& Frame) = 0;

	/** Ground height (uu) at a UE world XY: used by input picking (ray / terrain intersection) and world UI. */
	virtual double GetGroundHeight(double WorldX, double WorldY) const { return 0.0; }
};
