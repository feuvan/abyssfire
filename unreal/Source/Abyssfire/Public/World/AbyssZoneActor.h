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
// * Light and look (DECISIONS P10, P11, P14 as revised by the art pass): the movable directional sun (manifest key light,
//   pi lux x the mood's sun colour, lighting channel 0 only; the shadow-catcher toon reads its shadow term), CSM per tier
//   (1 cascade: desktop every tier, high-tier mobile; a 5 % fade-out) over 1.35 x the live camera distance, contact shadows on
//   desktop balanced / high; an ExponentialHeightFog per mood (zone_moods.json port.fog) starting 1 m beyond the camera
//   focus distance; the unbound post-process: manual exposure, tone curve 0.4, the built-in grade at 55 % of the mood
//   (no M_AF_PP_Grade), vignette, bloom and SSAO (+ M_AF_PP_AOComposite) per tier. UpdateView re-fits the shadow
//   distance and the fog start to the camera zoom and applies the look console variables live.
// * Occlusion fade (R1, world-map-nav.md 15.6): tall dressing between the camera and the hero / nearby living monsters
//   fades to alpha .25 (decor, camp props) / .45 (walls) with the web's 110 ms approach. Ground loot near the hero also fades
//   the low dressing it lies in or behind (bushes, grass, rocks; in 3D a bag inside a berry bush is otherwise hidden).
//   With temporal AA (TSR / TAA) the masked toon / ink materials dither the fade with a pattern that changes every
//   jitter sample, which the AA resolves; without it (desktop low FXAA, mobile MSAA) an instance faded by .3 or more is
//   hidden in its ISM and drawn by a translucent copy in a parallel "fade ghost" ISM (M_AF_Toon_Fade / _Foliage_Fade,
//   ink hull hidden; abyss.FadeGhost). Trees, rocks and tall camp props (well, banners) fade when a target's ray toward
//   the camera enters their bounds ellipsoid from behind; walls, stakes and gates use a cylinder.
// * Picking support: GetGroundHeight / RaycastGround on the terrain height field (ue58-platform.md 8.4).
// * Shader gate (uncooked editor-binary runs only, WITH_EDITOR): an instanced mesh whose material is still compiling
//   for this RHI stays hidden until its shader map is ready (abyss.DecorShaderGate), instead of drawing with the
//   engine's default material (grey, black where the sun does not reach). Cooked builds ship compiled shaders.
// * Presentation theme (dark remake U1, Private/World/AbyssThemeRules.h): a map with `presentationTheme` (emerald_plains:
//   march) shows `<gameId>@<theme>` art before `<gameId>` for every dressing id (decor, outcrops, palisades, camp props,
//   story props, exits, skirt), paints terrain style / MI_AF_Terrain_<Theme> of that theme, plays `<recipe>@<theme>`
//   zone loops when they exist, and is lit by its mood state: the base mood, the dawn mood once the chapter is complete,
//   a mood region (the Breach Rim) blended in near its centre (UpdateMood, every frame; the light / fog / grade are
//   re-applied only when the blend moved by more than 0.01). Maps without a presentation theme or regions keep today's
//   look exactly.
// * Set pieces (dark remake U4, Private/World/AbyssSetPieces.cpp; rules AbyssSetPieceRules.h): the zone's authored prop
//   groups of Data/set_pieces.json (Knellwatch's bell frame, hearth dressing and Name Wall, Ashwheat's cottages and
//   burned bridge, the Sheepfold's wagon ring, the Breach Rim's dais / ash circles / banner ring, the helmet stakes, the
//   Boneyard Gate, the sealed Pit mouth), placed through the same instanced dressing (occlusion fade for the tall ones)
//   after the walls and before the decorations, which leave out non-blocking dressing on a piece's tiles. Presentation
//   only: no collision, no walkability. Their sockets give zone loops (smoulder, smoke), audio emitter spots (the
//   knocking doors), cue spots (the bells ripple when the audio system plays a bell cue) and echo anchors; a yielding
//   piece (the Boneyard Gate) is hidden while a defend target of the same art stands on it.
// * Sub-dungeons (dark remake U6, Wave 3: the Pit of the Faithful; rules Private/World/AbyssSubDungeonRules.h and the
//   theme parent chain of AbyssThemeRules.h): a presentation theme with a parent (pit -> march) resolves every themed
//   id, recipe and style as `<id>@pit`, `<id>@march`, `<id>`; a wall block whose art says `placement.yawStepDeg` turns in
//   those steps and, inside a hand-authored sub-dungeon (AbyssSubDungeonRules::CutFromRock; the generated caves keep
//   the surface's rules), stands only on wall tiles that touch an open tile (the rest is void: black terrain, no
//   skirt); the stair (exit_portal@pit) faces the floor and plays its recipe at its `fx_vortex` socket; the
//   lamps are placed as two meshes at one transform (unlit shown, lit hidden) and swapped by the cleanse sequence
//   (StartCleanse / TickSubDungeon: nearest first, 420 ms apart; the mood and the materials' ambient go to the cleansed
//   values over 6 s; a lit lamp's flame loop burns at its `flame` socket with its light pool under the `light` socket;
//   the niche's glow and marks wait until the niche is in view), or all lit at once when the zone is entered cleansed;
//   the surface's entrance piece shows its
//   `_sealed` twin while the entrance is sealed. The light-radius grade (vfx_recipes.json dark.<theme>.lightRadius) is
//   handed to the world builder (GetGrade), which writes it to MPC_AF_Lighting.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include <string>

#include "abyss/base/Enums.h"
#include "abyss/base/Types.h"

#include "World/AbyssTerrain.h"
#include "World/AbyssWorldTypes.h"

#include "AbyssZoneActor.generated.h"

class UAbyssAssetLibrary;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPostProcessComponent;
class UProceduralMeshComponent;
class USceneComponent;
class UStaticMesh;
class UTexture2D;
struct FAbyssArtAsset;
struct FAbyssSetPieceState;
struct FAbyssSubDungeonLamp;
struct FAbyssSubDungeonState;

namespace abyss
{
	class DataStore;
	struct Snapshot;
	struct ZoneMoodDef;
}

/** A spot where the world builder plays a looping recipe (campfire, torch, exit portal, lore pool, ...). */
struct FAbyssZoneEffectSpot
{
	FName RecipeId;
	/**
	 * `<RecipeId>@<theme>` for every theme of the zone's presentation theme chain, nearest first (pit: `exit.portal@pit`,
	 * `exit.portal@march`); the first that exists plays instead of RecipeId. Empty without a presentation theme.
	 */
	TArray<FName, TInlineAllocator<2>> ThemedRecipeIds;
	/** Only a themed id may play (AbyssThemeRules::ThemedOnlyRecipe): no fall back to RecipeId when none exists. */
	bool bThemedOnly = false;
	FVector Location = FVector::ZeroVector;
	FLinearColor Color = FLinearColor::White;
	bool bHasColor = false;
	/**
	 * U6: the recipe's light at its own spot instead of Location (a lit lamp: the flame at socket `flame`, the pool on
	 * the ground under socket `light`; LightHeightCm = the socket over that ground point), and - when set - with its own
	 * radius and colour (FAbyssVfxContext's light fields).
	 */
	bool bHasLightSpot = false;
	FVector LightSpot = FVector::ZeroVector;
	float LightHeightCm = 0.f;
	float LightRadiusCm = 0.f;
	bool bHasLightColor = false;
	FLinearColor LightColor = FLinearColor::White;

	/** The ids to try, in order (UAbyssVfxSystem::PlayFirst): the themed ones, then RecipeId unless bThemedOnly. */
	void GetRecipes(TArray<FName, TInlineAllocator<4>>& Out) const
	{
		Out.Reset();
		Out.Append(ThemedRecipeIds);
		if (!bThemedOnly)
		{
			Out.Add(RecipeId);
		}
	}
};

/** The light-radius grade of a zone (vfx_recipes.json dark.<theme>.lightRadius) as MPC_AF_Lighting takes it. */
struct FAbyssZoneGrade
{
	/** rgb = the ambient the graded materials multiply by, a = the ceiling of ambient + pools. */
	FLinearColor AmbientCeil = FLinearColor(1.f, 1.f, 1.f, 1.f);
	/** rgb = the hero pool's colour x gain, a = its falloff power (0 = no pool). */
	FLinearColor HeroColorPower = FLinearColor(0.f, 0.f, 0.f, 0.f);
	float HeroRadiusCm = 0.f;
};

/** What the sub-dungeon layer showed (the in-game autotest's probe, report.json `subDungeon`). */
struct FAbyssSubDungeonStatus
{
	bool bInside = false;
	/** A hand-authored sub-dungeon (AbyssSubDungeonRules::CutFromRock): the place the autotest's probe counts as a visit. */
	bool bCutFromRock = false;
	/** Set pieces of the contract's unlit lamp the map placed (Lamps: those with a lit twin). */
	int32 LampsExpected = 0;
	bool bGraded = false;
	bool bThemeResolved = false;
	int32 WallTiles = 0;
	int32 WallBlocks = 0;
	int32 Lamps = 0;
	int32 LampsLit = 0;
	bool bCleansing = false;
	bool bSequenceFinished = false;
	float CleanseAlpha = 0.f;
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

	/** Render tier: sun shadows, fog, bloom / grading / AO, post-process (ue58-platform.md 6.8, P10, P14). */
	void ApplyQuality(const FAbyssQualityProfile& Quality);

	/**
	 * Per frame (world builder, after the camera rig): the live camera distance. A change of more than 2 % (zoom, level-up
	 * pulse) re-fits the CSM distance (1.35 x, P10) and the height-fog start (+ 1 m, P14); a change of the look console
	 * variables (abyss.ExposureBias, abyss.ToneCurve, abyss.Bloom, abyss.Fog, abyss.FogScale) re-applies the post-process
	 * / fog the same frame (Mac calibration: exposure palette chart, fog against the look-dev).
	 */
	void UpdateView(float CameraDistanceCm);

	/** abyss.SunScale: the lit ground's sun term as a fraction of the pi-lux sun (MPC_AF_Lighting SunScale). */
	static float GetSunScale();

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

	/**
	 * Per frame (world builder): the mood state at the hero's tile (AbyssThemeRules::EffectiveMood: base / dawn mood,
	 * mood region blend). Re-applies the sun colour, fog and post-process grade when the blend moved by more than
	 * MoodReapplyEpsilon, and updates the ambience recipe / tint, light scale and wind-up tints the getters return.
	 */
	void UpdateMood(const abyss::Snapshot& Snap, const abyss::DataStore& Data);
	/** Brightness setting (settings.json video.brightness) as an exposure bias added to abyss.ExposureBias (EV). */
	void SetUserExposureEv(float Ev);

	// ---- sub-dungeons (U6) ----
	/** EvAreaCleansed: the cleanse sequence starts from Pos (the hero's tile) at the sim clock SimNowMs. */
	void StartCleanse(const abyss::Vec2& Pos, double SimNowMs);
	/**
	 * Per frame (world builder, before UpdateMood): advances the cleanse sequence on the sim clock. Lamps whose time
	 * has come swap to their lit mesh; OutBursts gets the one-shot recipes to play now (the lamp catching, the niche's
	 * glow, its marks) and OutLoops the loops to start (each lit lamp's flame).
	 */
	void TickSubDungeon(const abyss::Snapshot& Snap, TArray<FAbyssZoneEffectSpot>& OutBursts, TArray<FAbyssZoneEffectSpot>& OutLoops);
	/** The grade of this frame (the ambient at the cleanse's progress, the hero's pool); false in an ungraded zone. */
	bool GetGrade(FAbyssZoneGrade& Out) const;
	FAbyssSubDungeonStatus GetSubDungeonStatus() const;

	const FAbyssTerrainField& GetTerrain() const { return Terrain; }
	double GetGroundHeight(double WorldX, double WorldY) const;
	bool RaycastGround(const FVector& Origin, const FVector& Direction, FVector& OutHit) const;
	const TArray<FAbyssZoneEffectSpot>& GetEffectSpots() const { return EffectSpots; }
	const FAbyssMoodLook& GetMood() const { return Mood; }
	abyss::MapTheme GetTheme() const { return Theme; }
	/** Ambient weather recipe of the zone (ambient.<pollen|wisps|dust_motes|sparks|ashfall>), NAME_None for none. */
	FName GetAmbienceRecipe() const { return AmbienceRecipe; }
	/** The weather tint for the ambience recipe's colour (only in zones with a presentation theme). */
	bool HasAmbienceColor() const { return bAmbienceHasColor; }
	FLinearColor GetAmbienceColor() const { return AmbienceColor; }
	/** The mood's port.lightScale override blended over the regions (> 0), or < 0: keep DECISIONS R18's formula. */
	float GetMoodLightScaleOverride() const { return MoodLightScaleOverride; }
	/** Monster wind-up tint of the current mood (zone_moods.json port.windupTint, DR5; else #FFC4B0 / #FF7A5C). */
	FLinearColor GetWindupTint(bool bElite) const { return bElite ? WindupTintElite : WindupTintNormal; }
	/** The map's presentation theme (march) or NAME_None. */
	FName GetPresentationTheme() const { return PresentationTheme; }
	/** Direction towards the key light (manifest shading), sun colour and ambient fill for MPC_AF_Lighting. */
	FVector GetDirToLight() const { return DirToLight; }
	FLinearColor GetSunColor() const { return SunColor; }
	FLinearColor GetAmbientFill() const { return AmbientFill; }
	FLinearColor GetRimColor() const { return RimColor; }

	// ---- set pieces (U4) ----
	/** The zone's set-piece audio emitter spots: emitter id -> tile coordinates (UAbyssAudioSystem::SetZoneEmitterSpots). */
	void GetSetPieceEmitterSpots(TArray<TPair<FName, TArray<FVector2D>>>& Out) const;
	/** The recipes a played audio cue ripples at set-piece sockets (bell_toll at the bell frame; Out is appended). */
	void GetSetPieceCueSpots(FName CueId, TArray<FAbyssZoneEffectSpot>& Out) const;
	/** Where a lore echo's figures stand when a set piece poses them: the piece's tile and the yaw they turn by. */
	bool GetSetPieceEchoAnchor(const std::string& LoreId, const abyss::Vec2& LoreTile, abyss::Vec2& OutTile,
		double& OutYawDeg) const;
	/** Per frame: hides each yielding piece a defend target stands on and shows it again once none does. */
	void UpdateSetPieceYields(TConstArrayView<abyss::Vec2> DefendTargetTiles);
	/** The facing (FAbyssPropSetup::YawDeg convention) a defend target at Tile takes from the piece it stands on. */
	bool GetSetPieceYieldFacing(const abyss::Vec2& Tile, float& OutFacingYawDeg) const;

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
		bool bEllipsoid = false;    // R1 decor (trees, rocks, camp props, story props): bounds-ellipsoid test; walls: cylinder
		bool bStructure = false;    // a set-piece structure (U4): upright-cylinder test (AbyssSetPieceRules::StructureCoversRay)
		bool bGhosted = false;      // drawn by its fade ghost (own instance hidden), FFadeGhost::Occluders lists it
		bool bSuppressed = false;   // a set piece hidden for a defend target (U4): never covers, stays unfaded
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
	/** OutArt / OutTransform (optional): the asset and the instance transform placed. */
	bool PlaceArt(TConstArrayView<FName> GameIds, uint32 Hash, const FVector& Location, float YawDeg, float Scale,
		bool bOccluder, float FadeTarget, FName StoryId = NAME_None, const FAbyssArtAsset** OutArt = nullptr,
		FTransform* OutTransform = nullptr);
	/** PlaceArt for an asset the caller resolved already (ResolveArt(GameIds, Hash): the walls read its yaw step first);
	 *  GameIds only names the miss when Art is null. */
	bool PlaceResolvedArt(const FAbyssArtAsset* Art, TConstArrayView<FName> GameIds, uint32 Hash, const FVector& Location,
		float YawDeg, float Scale, bool bOccluder, float FadeTarget, FName StoryId = NAME_None,
		const FAbyssArtAsset** OutArt = nullptr, FTransform* OutTransform = nullptr);

	void BuildTerrain(const abyss::Snapshot& Snap, const abyss::DataStore& Data);
	void ApplyTerrainStyle(const abyss::DataStore& Data);
	void BuildWalls(const abyss::Snapshot& Snap);
	/** U4 set pieces (AbyssSetPieces.cpp): placements, art, spots; before BuildDecorations (it reads the covered tiles). */
	void BuildSetPieces(const abyss::Snapshot& Snap, const abyss::DataStore& Data);
	/**
	 * Hides (scale 0 where it stands, its occluder suppressed) or restores a keyed set-piece instance: a yielding
	 * piece, a lamp's unlit or lit twin (U6). Why, when given, is logged with the change.
	 */
	void SetSetPieceHidden(FName Key, bool bHidden, const TCHAR* Why = nullptr);
	void BuildDecorations(const abyss::Snapshot& Snap, const abyss::DataStore& Data);
	void BuildCamps(const abyss::Snapshot& Snap, const abyss::DataStore& Data);
	void BuildMarkers(const abyss::Snapshot& Snap, const abyss::DataStore& Data);
	void BuildSkirt(const abyss::DataStore& Data);
	/** U6: the sub-dungeon state of the build (the theme's dark contract, the entry's cleanse state); before BuildLighting. */
	void BeginSubDungeon(const abyss::Snapshot& Snap, const abyss::DataStore& Data);
	/** U6: a lit lamp's flame loop - at its `flame` socket, its light pool at the `light` socket (the contract's lamp light). */
	FAbyssZoneEffectSpot MakeLampFlameSpot(const FAbyssSubDungeonLamp& Lamp) const;
	/** U6: after FlushInstances - hides the twin of every lamp that is not its state's (unlit shown, or lit when cleansed). */
	void ApplyLampStates();
	void BuildLighting(const abyss::Snapshot& Snap, const abyss::DataStore& Data);
	/** The mood's look: grade (FAbyssMoodLook), sun colour, fog. bLive also re-applies the post-process now. */
	void ApplyMoodDef(const abyss::ZoneMoodDef& MoodDef, bool bLive);
	/** A looping zone recipe at a spot; in a zone with a presentation theme also its `<recipe>@<theme>` variant ids. */
	void AddEffectSpot(FName RecipeId, const FVector& Location);
	/** A spot of RecipeId at Location with the themed ids of the zone's chain (AbyssThemeRules::RecipeChain). */
	FAbyssZoneEffectSpot MakeEffectSpot(FName RecipeId, const FVector& Location) const;
	void ApplyPostProcess();
	/** Sun CSM distance and fog start for ViewDistanceCm (P10 / P14). */
	void ApplyViewDistance();
	/** Height fog of the mood (colour, opacity, density / ln 2 x abyss.FogScale, visibility abyss.Fog). */
	void ApplyFog();
	FVector GroundPoint(double TileX, double TileY) const;

	UPROPERTY(VisibleAnywhere, Category = "Abyss")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Abyss")
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;

	UPROPERTY(VisibleAnywhere, Category = "Abyss")
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere, Category = "Abyss")
	TObjectPtr<UPostProcessComponent> PostProcess;

	UPROPERTY(VisibleAnywhere, Category = "Abyss")
	TObjectPtr<UExponentialHeightFogComponent> HeightFog;

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

	/** M_AF_PP_AOComposite (content build; nullptr when missing): SSAO composite blendable on desktop balanced / high. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> AOCompositeMaterial;

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
	/** U4: the zone's set-piece spots and yield state (Private/World/AbyssSetPieces.h); null before a build. */
	TSharedPtr<FAbyssSetPieceState> SetPieceState;
	/** U6: the sub-dungeon layer's state (Private/World/AbyssSubDungeonState.h); null before a build. */
	TSharedPtr<FAbyssSubDungeonState> SubDungeonState;
	/** The presentation theme's chain, nearest first (AbyssThemeRules::ThemeChain: pit, march); empty without a theme. */
	TArray<FName, TInlineAllocator<4>> ThemeChainNames;
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
	/** Live camera distance (UpdateView) and the one the shadow distance / fog start were last fitted to. */
	float ViewDistanceCm = 2125.f;
	float AppliedViewDistanceCm = -1.f;
	/** Look console variables the post-process / fog were last built with (UpdateView applies changes live). */
	float AppliedExposureBias = 0.f;
	float AppliedToneCurve = 0.f;
	float AppliedBloom = 0.f;
	float AppliedFogScale = 0.f;
	int32 AppliedFogMode = -1;
	/** Art manifest hero fx.bloom.strength (0 = none): the zone bloom intensity unless abyss.Bloom overrides it. */
	float ArtBloomStrength = 0.f;
	FAbyssMoodLook Mood;
	abyss::MapTheme Theme = abyss::MapTheme::Plains;
	std::string ThemeName;
	/** Presentation theme (U1): the `@theme` suffix of themed art ids, NAME_None without one. */
	FName PresentationTheme;
	/** Terrain style / painted terrain theme: the presentation theme's name, else ThemeName. */
	std::string TerrainThemeName;
	int32 WallPaintTile = 0;
	FName AmbienceRecipe;
	bool bAmbienceHasColor = false;
	FLinearColor AmbienceColor = FLinearColor::White;
	// Mood state applied last (AbyssThemeRules::FMoodState fields; LookChanged compares against them).
	bool bMoodApplied = false;
	abyss::MapTheme AppliedMoodBase = abyss::MapTheme::Plains;
	abyss::MapTheme AppliedMoodRegion = abyss::MapTheme::Plains;
	bool bAppliedMoodHasRegion = false;
	double AppliedMoodRegionWeight = 0.0;
	/** U6: the cleanse's progress the look was last applied with (AbyssThemeRules::FMoodState::CleanseAlpha). */
	double AppliedCleanseAlpha = 0.0;
	bool bMoodRegionDominant = false;
	float MoodLightScaleOverride = -1.f;
	FLinearColor WindupTintNormal = FLinearColor::White;
	FLinearColor WindupTintElite = FLinearColor::White;
	// The ambience recipe / colour and the wind-up tints follow the dominant and base moods only (the zone fixed): they
	// are recomputed when either changes or the zone is rebuilt, not every frame (UpdateMood).
	bool bMoodLookCached = false;
	abyss::MapTheme CachedLookDominant = abyss::MapTheme::Plains;
	abyss::MapTheme CachedLookBase = abyss::MapTheme::Plains;
	/** Brightness (U1): added to the exposure bias of the post-process. */
	float UserExposureEv = 0.f;
	float AppliedUserExposureEv = 0.f;
	FVector DirToLight = FVector(0.40558, -0.40558, 0.819152);
	FLinearColor SunColor = FLinearColor::White;
	FLinearColor AmbientFill = FLinearColor(0.55f, 0.55f, 0.6f);
	FLinearColor RimColor = FLinearColor(1.f, 0.925f, 0.784f);
	float MeshYawOffsetDeg = -90.f;
	float CameraYawDeg = 45.f;
};
