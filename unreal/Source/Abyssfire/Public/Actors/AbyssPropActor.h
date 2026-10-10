// AAbyssPropActor: the presentation of a core entity that is not a living character - ground loot and potions, quest
// markers (gather nodes, clues, quest items), lore pickups, hidden rewards (chests, gold piles), random-event props,
// the soul echo, defend targets - and of static zone props that need per-instance feedback (story decorations).
// (art-inventory-ch1.md 6.6-6.8, 7; world-map-nav.md 12; quests-story-ch1.md 3.2.)
//
// * Static mesh from the art manifest, or a skeletal mesh (chest lid, rescue NPC, the hero's ghost for the soul echo)
//   playing its Idle clip in single-node mode (ue58-platform.md 7.3 Option B: props need no blending).
// * Bob / spin / pulse presentation, a ground light pool (additive quad), quality / ghost tints and the highlight rim
//   through custom primitive data (AbyssCpd), collect fade (rise + fade 500 ms), chest open.
// * The actor does not tick: UAbyssWorldBuilder calls PresentFrame once per rendered frame.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "abyss/base/Types.h"

#include "World/AbyssArtManifest.h"
#include "World/AbyssWorldTypes.h"

#include "AbyssPropActor.generated.h"

class UAbyssAssetLibrary;
class UAnimSequence;
class UMaterialInterface;
class USceneComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/** Presentation style of a prop (chosen by the world builder from the entity / marker kind). */
enum class EAbyssPropStyle : uint8
{
	Static,       // story decorations, event props, defend targets
	Loot,         // ground item: quality tint, bob 13 cm / 800 ms
	Potion,       // potion drop: bob 11 cm / 600 ms, glow
	Pickup,       // quest item / gather node / clue: bob 13 cm / 900-1100 ms, slow rock
	Lore,         // lore collectible on a pulsing light pool
	Chest,        // hidden-reward / event chest (Open on collect)
	Ghost,        // soul echo: the hero's mesh with the ghost material, bob 13 cm / 1400 ms
};

struct FAbyssPropSetup
{
	abyss::EntityId Id = abyss::kNoEntity;
	abyss::EntityKind Kind = abyss::EntityKind::Prop;
	EAbyssPropStyle Style = EAbyssPropStyle::Static;
	const FAbyssArtAsset* Art = nullptr;
	/** Multiply tint (loot quality; ghost colour) and its strength. */
	FLinearColor Tint = FLinearColor::White;
	float TintAmount = 0.f;
	/** Ground light pool (lore pools, gather glow, potion glow, rare+ loot disc): radius 0 = none. */
	float PoolRadiusCm = 0.f;
	FLinearColor PoolColor = FLinearColor::White;
	float PoolAlphaMin = 0.3f;
	float PoolAlphaMax = 0.7f;
	float PoolPeriodSec = 1.5f;
	/** Yaw of the prop (deterministic jitter / facing), degrees. */
	float YawDeg = 0.f;
	float Scale = 1.f;
	/** Phase offset of the bob (gather nodes are staggered 173 ms per spot). */
	float PhaseSec = 0.f;
	bool bCastShadow = false;
};

UCLASS(NotBlueprintable)
class ABYSSFIRE_API AAbyssPropActor : public AActor
{
	GENERATED_BODY()

public:
	AAbyssPropActor();

	/** Loads the mesh and configures the components. False without art (the actor stays hidden but keeps its pick box). */
	bool InitProp(const FAbyssPropSetup& Setup, UAbyssAssetLibrary& Assets);
	/** Per rendered frame: ground location (already on the terrain), visual delta, presentation time. */
	void PresentFrame(const FVector& GroundLocation, float VisualDeltaSec, double TimeSec);

	/** Picked up / collected / claimed: rise and fade over DurationSec, then IsFadeFinished. */
	void BeginCollect(float DurationSec = 0.5f);
	/** Despawned without ceremony (expired / zone unload): fade quickly. */
	void BeginExpire(float DurationSec = 0.25f);
	bool IsFadeFinished() const { return bFadeFinished; }
	bool IsFading() const { return FadeTotalSec > 0.f; }

	/** Chest style: plays the Open clip (skeletal) or tilts the lid mesh away (static fallback). */
	void PlayOpen();
	/** Interactable hover / story-decoration focus rim (AbyssCpd::Highlight), 0..1. */
	void SetHighlight(float Amount);
	/** Occlusion fade (AbyssCpd::Fade) applied by the world builder, 0 = opaque. */
	void SetOcclusionFade(float Amount);

	abyss::EntityId GetEntityId() const { return EntityId; }
	abyss::EntityKind GetEntityKind() const { return EntityKind; }
	EAbyssPropStyle GetStyle() const { return Style; }
	bool HasArt() const { return bHasArt; }
	float GetVisualHeightCm() const { return VisualHeightCm; }
	FVector GetAnchorLocation(EAbyssAnchor Anchor) const;

private:
	void ApplyCpd();
	void SetCpd(int32 Index, float Value);

	UPROPERTY(VisibleAnywhere, Category = "Abyss")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Abyss")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, Category = "Abyss")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Abyss")
	TObjectPtr<USkeletalMeshComponent> SkeletalMesh;

	UPROPERTY(VisibleAnywhere, Category = "Abyss")
	TObjectPtr<UStaticMeshComponent> Pool;

	/** Clips of a skeletal prop (Idle, Open), kept alive here. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UAnimSequence>> Clips;

	abyss::EntityId EntityId = abyss::kNoEntity;
	abyss::EntityKind EntityKind = abyss::EntityKind::Prop;
	EAbyssPropStyle Style = EAbyssPropStyle::Static;
	bool bHasArt = false;
	bool bSkeletal = false;
	float VisualHeightCm = 50.f;
	float MeshYawOffsetDeg = -90.f;
	float BaseScale = 1.f;
	float PhaseSec = 0.f;
	float PoolRadiusCm = 0.f;
	FLinearColor PoolColor = FLinearColor::White;
	float PoolAlphaMin = 0.3f;
	float PoolAlphaMax = 0.7f;
	float PoolPeriodSec = 1.5f;
	float GhostAlpha = 0.f;

	// fades
	float FadeElapsedSec = 0.f;
	float FadeTotalSec = 0.f;
	float FadeRiseCm = 0.f;
	bool bFadeFinished = false;
	float OcclusionFade = 0.f;
	bool bOpened = false;
	float OpenElapsedSec = -1.f;

	float CpdValues[AbyssCpd::Count] = {};
	bool bCpdDirty = true;
};
