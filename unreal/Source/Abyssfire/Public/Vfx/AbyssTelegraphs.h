// UAbyssTelegraphSystem: the enemy ground telegraphs of the dark remake's Wave 2 core (U2, implementation-plan.md 4 U2 /
// 9 2G; DECISIONS DR5, DR16; rules: Private/Vfx/AbyssTelegraphRules.h).
//
// * Strikes and fuses (EvTelegraph, Snapshot::telegraphs for a late view): one SM_FX_Quad per decal with M_AF_Telegraph
//   (the crimson rim, the bone fill sweeping to the contact on the sim clock, the style's rim width / inner rim /
//   urgency; the contact flash then a fade; a cancelled windup fades where it stood: EvMonsterAttackCancelled). The
//   contact plays `telegraph.contact` (`.cone`, `.line`); a cancel in the frame that reached the contact (a stun in the
//   contact step) still fades it without the flash. An on-death fuse is presented only in a zone with a presentation
//   theme (Chapter 1; chapters 2-5 and the Labyrinth keep today's look): it draws when its effect damages and plays the
//   affix's own `elite.<type>.death` from the kill. Raise / summon markers are the breach-violet `monster.raise.mark`
//   recipe (stopped on a cancel), never the decal.
// * Hazards (EvHazard, Snapshot::hazards): the hazard-style rim (pulsing) until untilMs, then a fade; the loop
//   `hazard.<abilityId>`, else `hazard.breach_seep` (a line) / `hazard.breach_fire`, stopped at untilMs.
// * Arenas (EvArena, Snapshot::arenas): the arena-style ring (rim and wash outside) closing with abyss::ArenaRadiusAt and
//   the `hazard.banner_ring` loop at the same radius (UAbyssVfxSystem::SetHandleRadius); gone on `cleared`, a newer
//   arena of the same monster, the zone unload.
// * Budget: a cap per quality tier (overflow only); the arena first, boss > elite > normal, nearest the hero first. Mobile
//   low drops the arena's outer wash (the quad still spans the disc; its blank inside exits the shader early).
//   `abyss.DebugLeash 1` draws every nearby monster's M1 leash (MonsterView leashCentre / leashRange) as a rim-only
//   ring under the budget.
// * Without M_AF_Telegraph (an older content build) the decals are skipped (logged once); every recipe still plays and
//   the monster's wind-up tint remains.
//
// Ticked by UAbyssWorldBuilder::SyncFrame after the characters are placed and before UAbyssVfxSystem::Tick (recipes it
// starts are emitted the same frame); cleared with the zone (ClearZone) and at session end.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "abyss/sim/Events.h"
#include "abyss/sim/Snapshot.h"

#include "Framework/AbyssTypes.h"
#include "World/AbyssWorldTypes.h"

#include "AbyssTelegraphs.generated.h"

class AActor;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
struct FAbyssTelegraphState;

UCLASS()
class ABYSSFIRE_API UAbyssTelegraphSystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UAbyssTelegraphSystem* Get(const UObject* WorldContextObject);

	// ---- USubsystem ----
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// ---- driven by the world builder ----
	/** The render tier: the decal cap and whether the arena draws its outer wash. */
	void ApplyQuality(const FAbyssQualityProfile& Profile);
	/** Per frame, after the characters are placed and before the VFX system's tick. */
	void Tick(const FAbyssFrameInfo& Frame, const abyss::Snapshot& Snap);
	/** Zone teardown / session end: every decal, marker and loop handle goes (the VFX system clears its emitters). */
	void ClearAll();

	/** Decals drawn this frame (the in-game autotest's presentation probe, AbyssPresenceRules; diagnostics). */
	int32 GetDrawnDecalCount() const { return DrawnDecals; }
	/** Of those, arena rings (Valdras's closing ring). */
	int32 GetDrawnArenaCount() const { return DrawnArenas; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void BindEvents();
	void UnbindEvents();
	void HandleTelegraph(const abyss::EvTelegraph& Event);
	void HandleHazard(const abyss::EvHazard& Event);
	void HandleArena(const abyss::EvArena& Event);
	void HandleAttackCancelled(const abyss::EvMonsterAttackCancelled& Event);
	void HandleEntitySpawned(const abyss::EvEntitySpawned& Event);

	/** The renderer actor, the quad and M_AF_Telegraph (false while the world / assets are not ready). */
	bool EnsureResources();
	/** M_AF_Telegraph's contract from the art manifest (once it is loaded). */
	void EnsureContract();
	/** A pooled quad's index (a new one when none is free), INDEX_NONE without the resources. */
	int32 AcquireComponent();
	void ReleaseComponent(int32 Index);
	double SimNowMs() const;
	double GroundZ(double X, double Y) const;

	TSharedPtr<FAbyssTelegraphState> State;

	UPROPERTY(Transient)
	TObjectPtr<AActor> DecalActor;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> QuadMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DecalMaterial;

	/** Pooled decal quads (one per drawn decal; hidden when free). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Pool;

	TArray<int32> FreePool;
	int32 DecalCap = 32;
	bool bOuterWash = true;
	int32 DrawnDecals = 0;
	int32 DrawnArenas = 0;
	bool bEventsBound = false;
	bool bResourcesResolved = false;
	bool bDecalsAvailable = false;
};
