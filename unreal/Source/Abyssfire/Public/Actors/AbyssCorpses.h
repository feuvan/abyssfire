// UAbyssCorpseLayer: the corpses and the monster states of the dark remake's Wave 2 core (U3 and the U2 / U5 monster
// parts, implementation-plan.md 4 U3 / 9 2G; DECISIONS DR5, DR6, DR16; rules: Private/Actors/AbyssCorpseRules.h and
// Private/Actors/AbyssMonsterLookRules.h).
//
// * Ash husks (EvCorpseAdded / EvCorpseRemoved, Snapshot::corpses for a late view): a kill whose definition leaves a
//   registry entry and whose art carries a corpse block holds its Death clip's last frame (AAbyssCharacterActor's
//   corpse-holding death); on EvCorpseAdded the world builder hands that actor over (ReleaseCorpseActor) and this layer
//   runs its timeline on the sim clock: hold, grey (the slots on the M_AF_Toon_Ash / M_AF_Outline_Ash twins), crumble by
//   plates to crumbleTo with corpse.crumble, a husk until the core removes the entry. The removal picks the end: expired
//   dissolves with corpse.expire (the ash blowing along world +X), raised goes at once (the risen monster reverses the
//   ash over the first 60 % of its Rise; monster.raise at the spot), consumed crumbles away (after the gnawer's
//   corpse.consume or a boss Feed's drain), burned flares corpse.burn in the colour of the fire's side
//   (EvCorpseRemoved::burner: the hero's ember or its own fire kill's impact colour, an enemy's breach violet, none:
//   grey). The corpse block's geometry follows the body's drawn scale (a hunt leader x1.25). Map seeds and late views
//   are spawned as posed husks (world builder SpawnHuskActor). A story-held body (the Corpseherd's
//   cs_rf_herd, Valdras's cs_rf_marshal_falls) keeps its death pose while its beat plays. A visible cap per quality tier
//   (overflow retires the oldest early). The blob shadow follows the pelvis and thins with the crumble.
// * Breach-blood (a corpse block that looks `puddle`: no registry entry): the Death clip's tar puddle at the death, the
//   shipped Puddle decal sprite through a runtime recipe (UAbyssVfxSystem::PlayRuntimeRecipe): it forms over the clip
//   and dries in 3 s.
// * Without the ash twins (an older content build) no death holds: today's thrown fade everywhere, nothing drawn here
//   (logged once). Chapters 2-5 have no corpse-leaving definitions and no puddle looks: nothing changes there.
// * Monster states: the gnawer's corpse.consume on its Consume channel (stopped on a cancel); a boss Feed's
//   monster.<defId>.feed (the ash streaming into the feeder); the aura loop (monster.<defId>.aura) while
//   MonsterView::auraRadius > 0; the Unburied affix loop (elite.unburied); the phase looks of EvMonsterPhase (the
//   zealot's frenzy loop, Valdras's burning standard; dropped on a reset or the death); the art's body loop (the
//   Cinderbloat's monster.ash_flakes); the buff / call pulse of
//   EvMonsterBuffCast (its own recipe, else the family's when the ability's own clip shows none); the block spark
//   (monster.block at the shield, EvBlock). Loops played in the body's frame turn with it once it turned
//   (UAbyssVfxSystem::SetHandleBlow).
//
// Ticked by UAbyssWorldBuilder::SyncFrame after the characters are placed and before the telegraphs and the VFX system
// (recipes it starts are emitted the same frame); cleared with the zone (ClearZone) and at session end.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include <string>

#include "abyss/sim/Events.h"
#include "abyss/sim/Snapshot.h"

#include "Framework/AbyssTypes.h"
#include "World/AbyssWorldTypes.h"

#include "AbyssCorpses.generated.h"

class AAbyssCharacterActor;
struct FAbyssCorpseState;

UCLASS()
class ABYSSFIRE_API UAbyssCorpseLayer : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UAbyssCorpseLayer* Get(const UObject* WorldContextObject);

	// ---- USubsystem ----
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// ---- driven by the world builder ----
	/** The render tier: how many husks draw at once (overflow retires the oldest). */
	void ApplyQuality(const FAbyssQualityProfile& Profile);
	/** Per frame, after the characters are placed and before the telegraphs' and the VFX system's ticks. */
	void Tick(const FAbyssFrameInfo& Frame, const abyss::Snapshot& Snap, float VisualDeltaSec, float RealDeltaSec);
	/** Zone teardown / session end: every husk actor goes; loop handles are forgotten (the VFX system clears them). */
	void ClearAll();

	/**
	 * Whether a monster of definition DefId drawn with art asset ArtName holds its death pose for this layer (its
	 * definition leaves a registry entry, the art carries a drawable corpse block, the ash twins exist); OutHoldFromMs:
	 * the Death clip's hold frame, OutHuskTopCm: the husk's top (the dissolve's height).
	 */
	bool CorpseHoldFor(const std::string& DefId, FName ArtName, float& OutHoldFromMs, float& OutHuskTopCm);
	/** M_AF_Toon_Ash and M_AF_Outline_Ash load (resolved once the asset library has its manifest). */
	bool HasAshMaterials();

	/** Husk actors drawn now (autotest / diagnostics). */
	int32 GetHuskCount() const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void BindEvents();
	void UnbindEvents();
	void HandleCorpseAdded(const abyss::EvCorpseAdded& Event);
	void HandleCorpseRemoved(const abyss::EvCorpseRemoved& Event);
	void HandleMonsterRaised(const abyss::EvMonsterRaised& Event);
	void HandleMonsterFeed(const abyss::EvMonsterFeed& Event);
	void HandlePlayAnim(const abyss::EvPlayAnim& Event);
	/** A breach body's tar puddle at its death (the corpse block looks `puddle`: no registry entry). */
	void SpawnPuddle(abyss::EntityId Entity);
	void HandleAttackCancelled(const abyss::EvMonsterAttackCancelled& Event);
	void HandleMonsterPhase(const abyss::EvMonsterPhase& Event);
	void HandleBuffCast(const abyss::EvMonsterBuffCast& Event);
	void HandleBlock(const abyss::EvBlock& Event);
	void HandleHit(const abyss::EvHit& Event);
	void HandleStoryBeat(const abyss::EvStoryBeat& Event);

	/** The M_AF_AshDissolve contract from the art manifest (once it is loaded). */
	void EnsureContract();
	/** A posed husk for entry Index (a seed or a late view); false when its art draws none. */
	bool SpawnHusk(size_t Index);
	/** Destroys entry Index's actor (if any) and forgets the entry. */
	void RemoveEntry(size_t Index);
	/** World location of a husk's centre (the manifest's husk centreCm in the actor's frame), on the ground. */
	FVector HuskCentre(size_t Index) const;
	/** Plays RecipeId at entry Index's husk centre (Blow: its facing, else world +X when bWorldX). */
	int32 PlayAtHusk(size_t Index, const std::string& RecipeId, bool bWorldX, bool bHasColor = false, uint32 Rgb = 0);
	/** The monster loops (aura, affix, phase) against the snapshot: started, turned with the body, stopped with the life. */
	void UpdateMonsterLoops(const abyss::Snapshot& Snap);
	/** The snapshot of this dispatch (the driver's index, else the sim's view for the session's first batch). */
	const abyss::Snapshot* CurrentSnapshot() const;
	double SimNowMs() const;
	double GroundZ(double X, double Y) const;

	TSharedPtr<FAbyssCorpseState> State;
	int32 HuskCap = 32;
	bool bEventsBound = false;
};
