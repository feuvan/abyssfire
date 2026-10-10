// AAbyssZoneActor: everything static of one zone, built at runtime from the core's zone data (DECISIONS P9, W1, W5, W7,
// R1; ue58-platform.md 6.2, 6.4-6.8, 10.4; world-map-nav.md 3.4, 14, 15; art-inventory-ch1.md 6).
//
// * Terrain: FAbyssTerrainField meshes as UProceduralMeshComponent sections (ground chunks with M_AF_Terrain, water
//   surfaces with M_AF_Water), a transient tile-id texture (B8G8R8A8, point sampled) and the zone theme's terrain style
//   colours as material parameters (terrain_styles.json). Skirt hills beyond the border ring.
// * Instanced dressing, one UInstancedStaticMeshComponent per mesh (GPU-scene instance culling on every platform;
//   per-instance custom data AbyssDecorIcd): decorations (ZoneGrid::Decorations with the core's 15.5 jitter),
//   outcrops on wall tiles (tileHash(c, r, 3) variants, ZoneTerrain), palisade stakes on camp-wall tiles (4-bit
//   neighbour mask), camp props (abyss::CampProps), story decorations, exit portal / sealed gate (W7), skirt foliage,
//   lily pads.
// * Light and look: the movable directional sun (manifest key light, CSM per tier), the unbound post-process of the
//   zone mood (manual exposure, no tone curve, grade, vignette, bloom per tier; or M_AF_PP_Grade when it exists).
// * Occlusion fade (R1, world-map-nav.md 15.6): tall dressing between the camera and the hero / nearby monsters fades
//   to alpha .25 (decor, tents) / .45 (walls) with the web's 110 ms approach.
// * Picking support: GetGroundHeight / RaycastGround on the terrain height field (ue58-platform.md 8.4).
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

	/** Builds the whole zone from the snapshot (zone grid, markers, camps) and the data tables. */
	void BuildZone(const abyss::Snapshot& Snap, const abyss::DataStore& Data, UAbyssAssetLibrary& Assets,
		const FAbyssQualityProfile& Quality);
	/** Removes every generated component (the actor is destroyed afterwards by the world builder). */
	void ClearZone();

	/** Render tier: sun shadows, bloom / grading, post-process (ue58-platform.md 6.8, P10). */
	void ApplyQuality(const FAbyssQualityProfile& Quality);

	/** Occlusion fade toward the targets (hero chest / head, nearby monsters) seen from CameraLocation. */
	void UpdateOcclusion(TConstArrayView<FVector> Targets, const FVector& CameraLocation, float DeltaSec);
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
		float RadiusCm = 50.f;
		float HeightCm = 150.f;
		float FadeTarget = 0.75f;   // 1 - alpha (decor .25 -> .75, walls .45 -> .55)
		float Fade = 0.f;
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
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> InstanceComponents;

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
	/** Occluder indices per 8 x 8 tile bucket (spatial query). */
	TMap<FIntPoint, TArray<int32>> OccluderBuckets;
	/** Story decoration id -> (component, instance). */
	TMap<FName, TPair<int32, int32>> StoryInstances;
	TArray<FPendingStory> PendingStories;
	FName FocusedStory;
	TArray<FAbyssZoneEffectSpot> EffectSpots;
	TArray<FIntPoint> ExitTiles;

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
