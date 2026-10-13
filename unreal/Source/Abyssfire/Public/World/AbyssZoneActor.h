// AAbyssZoneActor: everything static of one zone, built at runtime from the core's zone data (DECISIONS P9, W1, W5, W7,
// R1; ue58-platform.md 6.2, 6.4-6.8, 10.4; world-map-nav.md 3.4, 14, 15; art-inventory-ch1.md 6).
//
// * Terrain: FAbyssTerrainField meshes as UProceduralMeshComponent sections (ground chunks with the theme's painted
//   MI_AF_Terrain_<Theme> when the content build made one, else the procedural M_AF_Terrain; water surfaces with
//   M_AF_Water), a transient tile-id texture (B8G8R8A8, point sampled) and the zone theme's terrain style colours as
//   material parameters (terrain_styles.json). Skirt hills beyond the border ring.
// * Instanced dressing, one UInstancedStaticMeshComponent per mesh (GPU-scene instance culling on every platform;
//   per-instance custom data AbyssDecorIcd): decorations (ZoneGrid::Decorations with the core's 15.5 jitter),
//   outcrops on wall tiles (tileHash(c, r, 3) variants, ZoneTerrain), palisade stakes on camp-wall tiles (4-bit
//   neighbour mask), camp props (abyss::CampProps), story decorations, exit portal / sealed gate (W7), skirt foliage,
//   lily pads.
// * Light and look: the movable directional sun (manifest key light, CSM per tier), the unbound post-process of the
//   zone mood (manual exposure, no tone curve, grade, vignette, bloom per tier; or M_AF_PP_Grade when it exists).
// * Occlusion fade (R1, world-map-nav.md 15.6): tall dressing between the camera and the hero / nearby living monsters
//   fades to alpha .25 (decor, tents) / .45 (walls) with the web's 110 ms approach. Ground loot near the hero also fades
//   the low dressing it lies in or behind (bushes, grass, rocks; in 3D a bag inside a berry bush is otherwise hidden).
//   With temporal AA (TSR / TAA) the masked toon / ink materials dither the fade with a pattern that changes every
//   jitter sample, which the AA resolves; without it (desktop low FXAA, mobile MSAA) an instance faded by .3 or more is
//   hidden in its ISM and drawn by a translucent copy in a parallel "fade ghost" ISM (M_AF_Toon_Fade / _Foliage_Fade,
//   ink hull hidden; abyss.FadeGhost). Trees, rocks and tents fade when a target's ray toward the camera enters their
//   bounds ellipsoid from behind; walls, stakes and gates use a cylinder.
// * Picking support: GetGroundHeight / RaycastGround on the terrain height field (ue58-platform.md 8.4).
// * Shader gate (uncooked editor-binary runs only, WITH_EDITOR): an instanced mesh whose material is still compiling
//   for this RHI stays hidden until its shader map is ready (abyss.DecorShaderGate), instead of drawing with the
//   engine's default material (grey, black where the sun does not reach). Cooked builds ship compiled shaders.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include <string>

#include "abyss/base/Enums.h"

#include "World/AbyssTerrain.h"
#include "World/AbyssWorldTypes.h"

#include "AbyssZoneActor.generated.h"

class UAbyssAssetLibrary;
class UDirectionalLightComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPostProcessComponent;
class UProceduralMeshComponent;
class USceneComponent;
class UStaticMesh;
class UTexture2D;
struct FAbyssArtAsset;

namespace abyss
{
	class DataStore;
	struct Snapshot;
}

/** A spot where the world builder plays a looping recipe (campfire, torch, exit portal, lore pool, ...). */
struct FAbyssZoneEffectSpot
{
	FName RecipeId;
	FVector Location = FVector::ZeroVector;
	FLinearColor Color = FLinearColor::White;
	bool bHasColor = false;
};

UCLASS(NotBlueprintable)
class ABYSSFIRE_API AAbyssZoneActor : public AActor
{
	GENERATED_BODY()

public:
	AAbyssZoneActor();

	/** Ticks only while the shader gate holds hidden dressing (WITH_EDITOR runs); otherwise tick stays disabled. */
	virtual void Tick(float DeltaSeconds) override;

	/** Builds the whole zone from the snapshot (zone grid, markers, camps) and the data tables. */
	void BuildZone(const abyss::Snapshot& Snap, const abyss::DataStore& Data, UAbyssAssetLibrary& Assets,
		const FAbyssQualityProfile& Quality);
	/** Removes every generated component (the actor is destroyed afterwards by the world builder). */
	void ClearZone();

	/** Render tier: sun shadows, bloom / grading, post-process (ue58-platform.md 6.8, P10). */
	void ApplyQuality(const FAbyssQualityProfile& Quality);

	/**
	 * Occlusion fade toward the targets (hero chest / head, nearby monsters) seen from CameraLocation. LootTargets
	 * (ground items and potion drops near the hero) fade only the low dressing they sit in or behind (bushes, grass,
	 * rocks, camp props; not the R1 occluders such as trees or walls): a drop that lands in a berry bush is otherwise
	 * hidden inside the mesh.
	 */
	void UpdateOcclusion(TConstArrayView<FVector> Targets, TConstArrayView<FVector> LootTargets, const FVector& CameraLocation,
		float DeltaSec);
	/** Story-decoration focus rim (EvStoryDecorFocus); NAME_None clears. */
	void SetStoryFocus(FName DecorId);

	const FAbyssTerrainField& GetTerrain() const { return Terrain; }
	double GetGroundHeight(double WorldX, double WorldY) const;
	bool RaycastGround(const FVector& Origin, const FVector& Direction, FVector& OutHit) const;
	const TArray<FAbyssZoneEffectSpot>& GetEffectSpots() const { return EffectSpots; }
	const FAbyssMoodLook& GetMood() const { return Mood; }
	abyss::MapTheme GetTheme() const { return Theme; }
	/** Ambient weather recipe of the zone (ambient.<pollen|wisps|dust_motes|sparks>), NAME_None for none. */
	FName GetAmbienceRecipe() const { return AmbienceRecipe; }
	/** Direction towards the key light (manifest shading), sun colour and ambient fill for MPC_AF_Lighting. */
	FVector GetDirToLight() const { return DirToLight; }
	FLinearColor GetSunColor() const { return SunColor; }
	FLinearColor GetAmbientFill() const { return AmbientFill; }
	FLinearColor GetRimColor() const { return RimColor; }

private:
	struct FOccluder
	{
		int32 Component = INDEX_NONE;
		int32 Instance = INDEX_NONE;
		FVector Location = FVector::ZeroVector;
		FTransform Transform = FTransform::Identity;   // world transform of the instance (the fade ghost's copy)
		float RandomValue = 0.5f;   // AbyssDecorIcd::Random of the instance (wind phase of the ghost)
		float RadiusCm = 50.f;
		float HeightCm = 150.f;
		float FadeTarget = 0.75f;   // 1 - alpha (decor .25 -> .75, walls .45 -> .55)
		float Fade = 0.f;
		bool bLootOnly = false;     // low dressing: fades for ground loot only, never for the hero or monsters
		bool bEllipsoid = false;    // R1 decor (trees, rocks, tents, story props): bounds-ellipsoid test; walls: cylinder
		bool bGhosted = false;      // drawn by its fade ghost (own instance hidden), FFadeGhost::Occluders lists it
	};

	/** Fade ghost of one dressing component (index parallel to DecorInstanceComponents). */
	struct FFadeGhost
	{
		TArray<int32> Occluders;    // occluder indices, in ghost instance order
		bool bResolved = false;     // the ghost component was created (or found impossible) once
		bool bDirty = false;        // the occluder set changed: rebuild the ghost instances
	};

	struct FPendingStory
	{
		FName Id;
		UStaticMesh* Mesh = nullptr;   // kept alive by the asset library
		int32 Instance = INDEX_NONE;
	};

	struct FInstanceBatch
	{
		TArray<FTransform> Transforms;
		TArray<float> CustomData;
		TArray<int32> OccluderSlots;   // occluder index per instance, or INDEX_NONE
	};

	/** Adds one mesh instance (batched until FlushInstances). Returns false without a mesh. */
	bool AddInstance(UStaticMesh* Mesh, const FTransform& Transform, float RandomValue, bool bOccluder, float FadeTarget,
		float RadiusCm, float HeightCm, FName StoryId = NAME_None);
	void FlushInstances();
	/** Shader gate: true when the component must stay hidden for now (its material is still compiling). */
	static bool GateUntilShadersReady(const UInstancedStaticMeshComponent& Component);
	/** Shows every gated component whose materials are ready (all of them after the timeout); stops ticking when done. */
	void UpdateShaderGate();
	/** Occlusion fade through translucent ghosts this frame (abyss.FadeGhost; auto: no temporal AA resolves the dither). */
	bool UseFadeGhosts() const;
	/** The fade ghost of a dressing component, created on first use; nullptr when its mesh has another material than
	 *  the toon / ink masters, the fade masters are missing, or (uncooked runs) its shaders are not ready yet. */
	UInstancedStaticMeshComponent* GetFadeGhost(int32 ComponentIndex);
	UInstancedStaticMeshComponent* CreateFadeGhost(UInstancedStaticMeshComponent& Source);
	/** Writes one occluder's fade: on its own instance (dithered), or on its ghost copy with its own instance hidden. */
	void ApplyOccluderFade(int32 Index, bool bGhostMode, TSet<int32>& DirtyComponents, TSet<int32>& DirtyGhosts);
	/** Rebuilds (after a set change) or refreshes the ghost instances of one dressing component. */
	void UpdateFadeGhost(int32 ComponentIndex);
	const FAbyssArtAsset* ResolveArt(TConstArrayView<FName> GameIds, uint32 Hash) const;
	bool PlaceArt(TConstArrayView<FName> GameIds, uint32 Hash, const FVector& Location, float YawDeg, float Scale,
		bool bOccluder, float FadeTarget, FName StoryId = NAME_None);

	void BuildTerrain(const abyss::Snapshot& Snap, const abyss::DataStore& Data);
	void ApplyTerrainStyle(const abyss::DataStore& Data);
	void BuildWalls(const abyss::Snapshot& Snap);
	void BuildDecorations(const abyss::Snapshot& Snap, const abyss::DataStore& Data);
	void BuildCamps(const abyss::Snapshot& Snap, const abyss::DataStore& Data);
	void BuildMarkers(const abyss::Snapshot& Snap, const abyss::DataStore& Data);
	void BuildSkirt(const abyss::DataStore& Data);
	void BuildLighting(const abyss::Snapshot& Snap, const abyss::DataStore& Data);
	void ApplyPostProcess();
	FVector GroundPoint(double TileX, double TileY) const;

	UPROPERTY(VisibleAnywhere, Category = "Abyss")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Abyss")
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;

	UPROPERTY(VisibleAnywhere, Category = "Abyss")
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere, Category = "Abyss")
	TObjectPtr<UPostProcessComponent> PostProcess;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> DecorInstanceComponents;

	/** Fade ghost ISMs, parallel to DecorInstanceComponents (nullptr until a ghost is needed, or when it is impossible). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> FadeGhostComponents;

	/** Translucent twins of M_AF_Toon / M_AF_Toon_Foliage for the fade ghosts (content build; nullptr when missing). */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> FadeToonMaster;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> FadeFoliageMaster;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> TileTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TerrainMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WaterMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GradeMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UAbyssAssetLibrary> AssetLibrary;

	FAbyssTerrainField Terrain;
	TMap<TObjectPtr<UStaticMesh>, FInstanceBatch> PendingInstances;
	TMap<TObjectPtr<UStaticMesh>, int32> ComponentByMesh;
	TArray<FOccluder> Occluders;
	/** Parallel to DecorInstanceComponents / FadeGhostComponents. */
	TArray<FFadeGhost> FadeGhosts;
	/** UseFadeGhosts() of the last UpdateOcclusion (a change re-applies every faded occluder). */
	bool bFadeGhostMode = false;
	/** Occluder indices per 8 x 8 tile bucket (spatial query). */
	TMap<FIntPoint, TArray<int32>> OccluderBuckets;
	/** Story decoration id -> (component, instance). */
	TMap<FName, TPair<int32, int32>> StoryInstances;
	TArray<FPendingStory> PendingStories;
	FName FocusedStory;
	TArray<FAbyssZoneEffectSpot> EffectSpots;
	TArray<FIntPoint> ExitTiles;
	/**
	 * Shader gate: DecorInstanceComponents indices hidden until their materials compile; when the wait began (wall
	 * clock, for the log), the last gate tick, and the waited frame time that abyss.DecorShaderGateMaxSec is measured in
	 * (each frame counts at most 0.25 s, so a blocking compile wait does not count).
	 */
	TArray<int32> ShaderGatedComponents;
	double ShaderGateStartSec = 0.0;
	double ShaderGateLastSec = 0.0;
	double ShaderGateWaitedSec = 0.0;
	/** Build diagnostics: first game ids of PlaceArt calls with no manifest asset (one zone-build warning lists them). */
	TSet<FName> MissingArtIds;
	int32 PlacedArtCount = 0;
	int32 UnresolvedArtCount = 0;
	int32 UnloadedArtCount = 0;

	FAbyssQualityProfile QualityProfile;
	/** Art manifest hero fx.bloom.strength (0 = none): the zone bloom intensity unless abyss.Bloom overrides it. */
	float ArtBloomStrength = 0.f;
	FAbyssMoodLook Mood;
	abyss::MapTheme Theme = abyss::MapTheme::Plains;
	std::string ThemeName;
	int32 WallPaintTile = 0;
	FName AmbienceRecipe;
	FVector DirToLight = FVector(0.40558, -0.40558, 0.819152);
	FLinearColor SunColor = FLinearColor::White;
	FLinearColor AmbientFill = FLinearColor(0.55f, 0.55f, 0.6f);
	FLinearColor RimColor = FLinearColor(1.f, 0.925f, 0.784f);
	float MeshYawOffsetDeg = -90.f;
	float CameraYawDeg = 45.f;
};
