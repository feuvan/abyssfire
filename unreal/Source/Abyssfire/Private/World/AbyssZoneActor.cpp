#include "World/AbyssZoneActor.h"

#include "Abyssfire.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "RHI.h"

#if WITH_EDITOR
#include "MaterialShared.h"
#endif

#include <string>
#include <string_view>

#include "abyss/base/Enums.h"
#include "abyss/base/Json.h"
#include "abyss/data/DataStore.h"
#include "abyss/data/MapData.h"
#include "abyss/data/MonsterData.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/sim/Snapshot.h"
#include "abyss/world/Grid.h"
#include "abyss/world/MapGen.h"

#include "Framework/AbyssText.h"
#include "Framework/AbyssUnits.h"
#include "World/AbyssArtManifest.h"
#include "World/AbyssAssetLibrary.h"
#include "World/AbyssSetPieces.h"
#include "World/AbyssThemeRules.h"
#include "World/AbyssWorldStateRules.h"

static TAutoConsoleVariable<float> CVarAbyssSunScale(
	TEXT("abyss.SunScale"),
	0.55f,
	TEXT("Sun term of the lit ground as a fraction of the pi-lux sun (MPC_AF_Lighting SunScale; ue58-platform.md 6.4 B'): ")
	TEXT("lit ground = base x (N.L x this + ambient). The sun itself is pi lux x SunColor (DECISIONS P11, art pass)."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarAbyssGroundAmbient(
	TEXT("abyss.GroundAmbient"),
	0.55f,
	TEXT("Ambient fill of the lit ground (MPC_AF_Lighting Ambient), so shadowed ground keeps the web's painted-shadow value."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarAbyssExposureBias(
	TEXT("abyss.ExposureBias"),
	0.263f,
	TEXT("Manual exposure bias (EV), applied live. Starts at log2(1.2) (an emissive 1.0 at display white without a tone ")
	TEXT("curve); with abyss.ToneCurve 0.4 recalibrate on the Mac so the T_AF_Palette mid-tones land within +-2/255 (DECISIONS P14)."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarAbyssToneCurve(
	TEXT("abyss.ToneCurve"),
	0.4f,
	TEXT("Post-process ToneCurveAmount on every tier (DECISIONS P14, art pass; 0.6 dulled the white selection rim and the ")
	TEXT("sword highlights in the look-dev). Applied live."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarAbyssFog(
	TEXT("abyss.Fog"),
	1,
	TEXT("Zone height fog (DECISIONS P14): 1 = on (zone_moods.json port.fog), 0 = hidden. Applied live."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarAbyssFogScale(
	TEXT("abyss.FogScale"),
	1.f,
	TEXT("Multiplier on the mood's fog density (UE FogDensity = port.fog density / ln 2 x this). Applied live."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarAbyssBloom(
	TEXT("abyss.Bloom"),
	-1.f,
	TEXT("Bloom intensity of the zone post-process on tiers with bloom (threshold 1). < 0 = the art manifest's hero ")
	TEXT("fx.bloom.strength (the reviewed emissive bloom, 0.8), 0.6 when the manifest has none. Applied live."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarAbyssFadeGhost(
	TEXT("abyss.FadeGhost"),
	1,
	TEXT("Occlusion fade of the zone dressing (DECISIONS R1). 0 = the masked toon / ink materials dither it on every ")
	TEXT("tier (a pattern that changes every TSR / TAA jitter sample, a fixed screen door without temporal AA). ")
	TEXT("1 = auto (default): dithered under TSR / TAA; otherwise (desktop low FXAA, mobile MSAA) an instance faded by .3 ")
	TEXT("or more is hidden and drawn by a translucent copy (M_AF_Toon_Fade / M_AF_Toon_Foliage_Fade, ink hull hidden). ")
	TEXT("2 = the translucent copy on every tier."),
	ECVF_Default);

#if WITH_EDITOR
// Uncooked runs compile a master on demand when the DDC has no shader map for this RHI ("Preparing Shaders (N)"), and
// UE draws its meshes with the engine default material meanwhile: lit WorldGrid, black where the sun does not reach
// (the zones have no sky light), e.g. the plains grass tufts and berry bushes (M_AF_Toon_Foliage / M_AF_Outline_Foliage).
static TAutoConsoleVariable<int32> CVarAbyssDecorShaderGate(
	TEXT("abyss.DecorShaderGate"),
	1,
	TEXT("Uncooked editor-binary runs only: zone dressing whose material is still compiling for this RHI. 0 = draw it at ")
	TEXT("once (engine default material until the shaders finish), 1 = hide it until its shaders are ready (default), ")
	TEXT("2 = finish those shaders while the zone builds (blocks the game thread). build_content.sh --shaders precompiles ")
	TEXT("every master so nothing waits."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarAbyssDecorShaderGateMaxSec(
	TEXT("abyss.DecorShaderGateMaxSec"),
	60.f,
	TEXT("abyss.DecorShaderGate 1: seconds of frames after the zone build after which still-hidden dressing is shown anyway ")
	TEXT("(each frame counts at most 0.25 s, so the zone-entry compile wait of abyss.WaitForCompilation does not count)."),
	ECVF_Default);
#endif

namespace AbyssZonePrivate
{
	constexpr uint8 TileWater = 3;
	constexpr uint8 TileWall = 4;
	constexpr uint8 TileCampWall = 6;
	constexpr int32 BucketTiles = 8;
	constexpr float DecorFadeTarget = 0.75f;   // alpha .25 (world-map-nav.md 15.6)
	constexpr float WallFadeTarget = 0.55f;    // alpha .45
	constexpr float FadeTimeConstantSec = 0.11f;
	constexpr float ShadowCasterMinHeightCm = 45.f;
	// Low dressing (not an R1 occluder) that can hide ground loot: anything taller than the 30 cm height floor of
	// PlaceArt (berry bush 73 cm, grass tufts 32-38, rocks 37-42, camp props). Lily pads and flat bits stay out.
	constexpr float LootCoverMinHeightCm = 30.5f;
	// Lateral slack of the cylinder occlusion test (walls, low dressing over loot): a character's body half-width along a
	// wall line (stakes / outcrops one tile apart fade without gaps); a ground item's footprint is smaller.
	constexpr double OccluderSlackCm = 25.0;
	constexpr double LootOccluderSlackCm = 12.0;
	// Fade ghosts (abyss.FadeGhost) take over a faded instance from fade .3 (alpha .7) and hand it back below .2 (alpha
	// .8; the gap keeps a fade that hovers around one value from switching every frame): the translucent copy has no ink
	// hull and blends its own overlapping parts (trunk, inner lobes) in triangle order, which reads as a broken mesh while
	// it is nearly opaque. Outside it, i.e. the first ~3 frames of a fade-in and the last ~0.25 s of a fade-out (110 ms
	// approach, snap at .02), the masked instance dithers sparsely (at most 5 of 16 pixels dropped, none below fade 1/32)
	// with its outline.
	constexpr float GhostEnterFade = 0.3f;
	constexpr float GhostLeaveFade = 0.2f;

	// ---- light and look (DECISIONS P10 / P11 / P14, art pass) ----
	// Sun CSM: distance = 1.35 x the live camera distance (2870 cm at the 2125 cm default; the top screen edge's ground is
	// ~26 m away), 2 cascades with a linear split on desktop, 1 cascade on high-tier mobile. Re-fitted when the camera
	// distance moves by more than 2 % (zoom, level-up pulse). UE splits from the near plane (DirectionalLightComponent
	// GetSplitDistance), and at pitch -50 the visible ground lies at 66-90 % of the distance in view depth (1907-2573 cm
	// at the default framing, the same ratio at every zoom): with 2 linear cascades all of it falls in the far one
	// [0.5, 1] x distance (0.55 cm per texel at 4096, about as fine as 3 cascades' last one, 0.51), the near cascade
	// covers the air under the camera and draws next to nothing. A third cascade bought nothing on screen; 1 cascade
	// would be 0.79 cm per texel (r.Shadow.CSM.MaxCascades 1 to A/B on the Mac).
	constexpr float ShadowDistanceScale = 1.35f;
	constexpr int32 DesktopCascades = 2;
	constexpr int32 MobileCascades = 1;
	constexpr float CascadeDistributionExponent = 1.f;
	constexpr float ViewRefitFraction = 0.02f;
	// Shadow acne on the shadow-catcher toon (its fake normal is the key light direction): a little more bias than UE's
	// directional defaults (0.5 / 0.5). Tune on the Mac.
	constexpr float SunShadowBias = 0.6f;
	constexpr float SunShadowSlopeBias = 0.8f;
	constexpr float ContactShadowLength = 0.03f;   // screen-space fraction, desktop balanced / high
	// Height fog: starts 1 m beyond the camera's focus distance (the hero's rows are never fogged), gentle falloff.
	constexpr float FogStartBeyondFocusCm = 100.f;
	constexpr float FogHeightFalloff = 0.2f;
	// zone_moods port.fog density is the look-dev extinction: opacity 1 - 2^(-density x metres / 10) past the start near
	// the ground (plains 0.85 -> 2^(-0.085 per m)). UE's height-fog line integral carries a ln 2 (HeightFogCommon.ush
	// CalculateLineIntegralShared: (1 - exp2(-F)) / F, Taylor log(2) at F -> 0; SceneCore.cpp FogDensity / 1000), so
	// transmittance = exp2(-ln2 x FogDensity / 1000 x cm): FogDensity = density / ln 2.
	constexpr float FogDensityToUE = 1.f / 0.69314718f;
	// Built-in grade at 55 % of the zone mood, +6 % saturation (the art pass's look-dev grade).
	constexpr float GradeMoodFraction = 0.55f;
	constexpr float GradeSaturationBoost = 1.06f;
	constexpr float ToneCurveFallback = 0.4f;
	// SSAO (desktop balanced / high): composited at 0.25 by M_AF_PP_AOComposite.
	constexpr float AOIntensity = 1.f;
	constexpr float AORadiusCm = 80.f;
	constexpr float AOQuality = 50.f;

	/**
	 * R1 decor occlusion (trees, boulders, tall camp props, story props; world-map-nav.md 15.6: the camera -> target ray
	 * against the prop): does the ray from a target point toward the camera enter the occluder's bounds ellipsoid
	 * (horizontal semi-axis R, vertical semi-axis H / 2, centred H / 2 over its ground point)? Canopies are widest in their
	 * upper half (plains fruit tree: 104-123 cm at 200-260 cm of 318, an 18-33 cm trunk below), which the ellipsoid
	 * follows, as it does boulders; the bounding cylinder with 25 cm slack counted targets beside a prop as covered (third
	 * Mac playtest: a boulder faded for a goblin next to it, whose chest ray passes over the rock), and a half-spheroid
	 * resting on the ground is narrowest where a canopy is widest (goblins half hidden by a crown stayed unfaded).
	 * Along / Lateral: the occluder centre in the target's frame (Along toward the camera), Z0: the
	 * target height over the ellipsoid centre. With s >= 0 the horizontal distance along the ray:
	 * ((Along - s)^2 + Lateral^2) / R^2 + (Z0 + s TanElevation)^2 / Hs^2 <= 1 for some s, a quadratic in s.
	 */
	bool EllipsoidCoversRay(double Along, double Lateral, double Z0, double TanElevation, double RadiusCm, double SemiHeightCm)
	{
		const double InvR2 = 1.0 / FMath::Max(RadiusCm * RadiusCm, 1.0);
		const double InvH2 = 1.0 / FMath::Max(SemiHeightCm * SemiHeightCm, 1.0);
		const double A = InvR2 + TanElevation * TanElevation * InvH2;
		const double B = 2.0 * (Z0 * TanElevation * InvH2 - Along * InvR2);
		const double C = (Along * Along + Lateral * Lateral) * InvR2 + Z0 * Z0 * InvH2 - 1.0;
		const double S = FMath::Max(0.0, -B / (2.0 * A));
		return (A * S + B) * S + C <= 0.0;
	}

	/** The anti-aliasing in use resolves a dither that changes per jitter sample: TAA (2) or TSR (4); FXAA, MSAA, none do not. */
	bool IsTemporalAntiAliasing(bool bMobile)
	{
		const TCHAR* Name = TEXT("r.AntiAliasingMethod");
		if (bMobile)
		{
			Name = TEXT("r.Mobile.AntiAliasing");   // the mobile renderer's own setting (DefaultEngine.ini: 3 = MSAA)
		}
		// Read every frame (RefreshQuality and the console can change it): no frequent-lookup warning.
		const IConsoleVariable* Method = IConsoleManager::Get().FindConsoleVariable(Name, /*bTrackFrequentCalls*/ false);
		const int32 Value = Method != nullptr ? Method->GetInt() : (bMobile ? 3 : 4);
		return Value == 2 || Value == 4;
	}

	// Asset / parameter id from a pattern with exactly one "%s" (e.g. TEXT("decor_%s")). Not FString::Printf: with
	// checked format strings (UE 5.4+, DefaultBuildSettings Latest) the format must be a TCHAR array literal, so a
	// runtime `const TCHAR*` pattern does not compile.
	FName Id(const TCHAR* Pattern, const FString& A)
	{
		return FName(*FString(Pattern).Replace(TEXT("%s"), *A, ESearchCase::CaseSensitive));
	}

	FString ToF(std::string_view Text)
	{
		return AbyssText::ToFString(Text);
	}

	FLinearColor JsonColor(const abyss::JsonValue& Value, const FLinearColor& Fallback)
	{
		if (Value.IsNumber())
		{
			return AbyssWorldUtil::ColorFromRgb(static_cast<uint32>(Value.AsInt64()));
		}
		FLinearColor Parsed;
		if (Value.IsString() && AbyssWorldUtil::ParseHexColor(AbyssText::ToFString(Value.AsString()), Parsed))
		{
			return Parsed;
		}
		return Fallback;
	}

	FIntPoint BucketOf(const FVector& Location)
	{
		return FIntPoint(FMath::FloorToInt32(Location.X / (AbyssUnits::TileUU * BucketTiles)),
			FMath::FloorToInt32(Location.Y / (AbyssUnits::TileUU * BucketTiles)));
	}

	float HashUnit(uint32 Hash)
	{
		return static_cast<float>(Hash & 0xFFFFu) / 65535.f;
	}

	/** Weighted pick from a decor pool by a hash. */
	const std::string* PickWeighted(const std::vector<abyss::WeightedDecor>& Pool, uint32 Hash)
	{
		double Total = 0.0;
		for (const abyss::WeightedDecor& Entry : Pool)
		{
			Total += Entry.weight;
		}
		if (Pool.empty() || Total <= 0.0)
		{
			return nullptr;
		}
		double X = HashUnit(Hash) * Total;
		for (const abyss::WeightedDecor& Entry : Pool)
		{
			X -= Entry.weight;
			if (X <= 0.0)
			{
				return &Entry.type;
			}
		}
		return &Pool.back().type;
	}

#if WITH_EDITOR
	// UMaterialInterface::GetMaterialResource takes the shader platform in recent engines and the feature level in older
	// ones (same shim as UAbyssContentBuildLibrary; own name, unity builds may join both files in one translation unit).
	template <typename MaterialType>
	FMaterialResource* ShaderGateResource(MaterialType* Material)
	{
		if constexpr (requires(MaterialType* M) { M->GetMaterialResource(GMaxRHIShaderPlatform); })
		{
			return Material->GetMaterialResource(GMaxRHIShaderPlatform);
		}
		else
		{
			return Material->GetMaterialResource(GMaxRHIFeatureLevel);
		}
	}

	/** One warning per run when this engine has no shader-map query, so an inactive gate shows up in the log. */
	void WarnShaderGateInactive()
	{
		static bool bWarned = false;
		if (bWarned)
		{
			return;
		}
		bWarned = true;
		UE_LOG(LogAbyss, Warning, TEXT("Shader gate: FMaterial::IsCompilationFinished not found on this engine, so "
			"abyss.DecorShaderGate is inactive (zone dressing draws with the engine default material while its shaders "
			"compile; abyss.WaitForCompilation still waits at zone entry in -game runs)"));
	}

	/** True while the shader map of this material resource is still being compiled (or fetched from the DDC). */
	template <typename ResourceType>
	bool IsShaderMapPending(ResourceType& Resource)
	{
		if constexpr (requires(ResourceType& R) { R.IsCompilationFinished(); })
		{
			return !Resource.IsCompilationFinished();
		}
		else
		{
			WarnShaderGateInactive();   // no query on this engine: the gate never hides anything
			return false;
		}
	}

	/** Material resources of the component's slots still compiling (a parameter-only instance resolves to its master's). */
	void CollectCompilingMaterials(const UInstancedStaticMeshComponent& Component, TArray<FMaterialResource*>& Out)
	{
		const int32 NumMaterials = Component.GetNumMaterials();
		for (int32 Slot = 0; Slot < NumMaterials; ++Slot)
		{
			UMaterialInterface* Material = Component.GetMaterial(Slot);
			FMaterialResource* Resource = Material != nullptr ? ShaderGateResource(Material) : nullptr;
			if (Resource != nullptr && IsShaderMapPending(*Resource))
			{
				Out.AddUnique(Resource);
			}
		}
	}
#endif
}

AAbyssZoneActor::AAbyssZoneActor()
{
#if WITH_EDITOR
	// Ticks (every frame, so dressing appears the frame its shaders are ready) only while the shader gate holds hidden
	// dressing: FlushInstances enables it, UpdateShaderGate disables it. Post-update work: after the tickable sim driver,
	// so when UAbyssWorldBuilder's zone-entry compile wait (abyss.WaitForCompilation) has just finished the shaders, the
	// dressing is shown in that same frame.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
#else
	PrimaryActorTick.bCanEverTick = false;
#endif
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Terrain"));
	TerrainMesh->SetupAttachment(SceneRoot);
	TerrainMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TerrainMesh->SetGenerateOverlapEvents(false);
	TerrainMesh->SetCanEverAffectNavigation(false);
	TerrainMesh->bUseAsyncCooking = false;
	TerrainMesh->SetCastShadow(false);   // the ground receives sun shadows; hills casting is not worth the cost
	// The water sections are translucent and sort by the whole terrain's bounds (the map centre, not the shore): draw
	// them before the other default-priority translucency (the slime shell, R12) and the VFX (1 / 2); the blob
	// shadows (-10) and prop light pools (-5) stay under them.
	TerrainMesh->SetTranslucentSortPriority(-2);

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(SceneRoot);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetCastShadows(true);
	// Lighting channel 0 only (P11): the shadow-catcher toon's fake normal would mis-light under any other light, so a
	// future desktop point light goes on channel 1.
	Sun->SetLightingChannels(/*bChannel0*/ true, /*bChannel1*/ false, /*bChannel2*/ false);
	Sun->SetShadowBias(AbyssZonePrivate::SunShadowBias);
	Sun->SetShadowSlopeBias(AbyssZonePrivate::SunShadowSlopeBias);

	// Height fog of the zone mood (P14), at the zone origin's height; configured by BuildLighting / UpdateView.
	HeightFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("HeightFog"));
	HeightFog->SetupAttachment(SceneRoot);
	HeightFog->SetMobility(EComponentMobility::Movable);
	HeightFog->SetRelativeLocation(FVector::ZeroVector);

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(SceneRoot);
	PostProcess->bUnbound = true;
	PostProcess->Priority = 0.f;
	PostProcess->BlendWeight = 1.f;
}

// =====================================================================================================================
// Build / clear
// =====================================================================================================================

void AAbyssZoneActor::ClearZone()
{
	for (UInstancedStaticMeshComponent* Component : DecorInstanceComponents)
	{
		if (Component != nullptr)
		{
			Component->DestroyComponent();
		}
	}
	DecorInstanceComponents.Reset();
	for (UInstancedStaticMeshComponent* Ghost : FadeGhostComponents)
	{
		if (Ghost != nullptr)
		{
			Ghost->DestroyComponent();
		}
	}
	FadeGhostComponents.Reset();
	FadeGhosts.Reset();
	bFadeGhostMode = false;
	ShaderGatedComponents.Reset();
	SetActorTickEnabled(false);
	ComponentByMesh.Reset();
	PendingInstances.Reset();
	PendingStories.Reset();
	Occluders.Reset();
	OccluderBuckets.Reset();
	StoryInstances.Reset();
	FocusedStory = NAME_None;
	EffectSpots.Reset();
	ExitTiles.Reset();
	SetPieceState.Reset();
	MissingArtIds.Reset();
	PlacedArtCount = 0;
	UnresolvedArtCount = 0;
	UnloadedArtCount = 0;
	if (TerrainMesh != nullptr)
	{
		TerrainMesh->ClearAllMeshSections();
	}
	TileTexture = nullptr;
	TerrainMaterial = nullptr;
	WaterMaterial = nullptr;
	AOCompositeMaterial = nullptr;
	Terrain.Reset();
}

void AAbyssZoneActor::BuildZone(const abyss::Snapshot& Snap, const abyss::DataStore& Data, UAbyssAssetLibrary& Assets,
	const FAbyssQualityProfile& Quality)
{
	ClearZone();
	AssetLibrary = &Assets;
	QualityProfile = Quality;
	// Fade ghost masters (abyss.FadeGhost), loaded with the zone so an uncooked run compiles them in the zone-entry wait.
	FadeToonMaster = Assets.LoadMaterial(TEXT("M_AF_Toon_Fade"));
	FadeFoliageMaster = Assets.LoadMaterial(TEXT("M_AF_Toon_Foliage_Fade"));
	const FAbyssArtShading& Shading = Assets.GetManifest().GetShading();
	MeshYawOffsetDeg = Shading.MeshYawOffsetDeg;
	CameraYawDeg = Shading.CameraYawDeg;
	DirToLight = Shading.DirToLight;
	RimColor = Shading.RimColor;

	// Emissive bloom strength of the reviewed hero art (manifest fx.bloom; the visor / blade emissive regions).
	ArtBloomStrength = 0.f;
	{
		const FName HeroId(*(FString(TEXT("player_")) + AbyssZonePrivate::ToF(abyss::EnumName(Snap.hero.cls))));
		const FName HeroIds[] = { HeroId };
		if (const FAbyssArtAsset* HeroArt = Assets.GetManifest().ResolveFirst(HeroIds); HeroArt != nullptr && HeroArt->Fx.bHasBloom)
		{
			ArtBloomStrength = HeroArt->Fx.BloomStrength;
		}
	}

	Theme = Snap.zone.theme;
	ThemeName = std::string(abyss::EnumName(Theme));
	WallPaintTile = Data.World().mapGen.Theme(Theme).primaryTile;
	// Presentation theme (U1): derived here from the map id, never cached across zones (spawns of the next zone are
	// announced before this build; the world builder resolves their art from the snapshot's map id itself).
	const abyss::MapDef* PresentationMap = Data.FindMap(Snap.zone.mapId);
	const std::string_view PresentationThemeName = AbyssThemeRules::ThemeOf(PresentationMap);
	PresentationTheme = PresentationThemeName.empty() ? NAME_None : FName(*AbyssZonePrivate::ToF(PresentationThemeName));
	TerrainThemeName = std::string(AbyssThemeRules::TerrainThemeName(PresentationMap, ThemeName));
	bMoodApplied = false;

	BuildLighting(Snap, Data);
	BuildTerrain(Snap, Data);
	BuildWalls(Snap);
	BuildSetPieces(Snap, Data);
	BuildDecorations(Snap, Data);
	BuildCamps(Snap, Data);
	BuildMarkers(Snap, Data);
	BuildSkirt(Data);
	FlushInstances();
	ApplyQuality(Quality);
	UE_LOG(LogAbyss, Log, TEXT("Zone %s built: %d instanced meshes (%d art placed, %d unresolved, %d not loaded), %d occluders, %d effect spots, presentation theme %s"),
		*AbyssText::ToFString(Snap.zone.mapId), DecorInstanceComponents.Num(), PlacedArtCount, UnresolvedArtCount, UnloadedArtCount,
		Occluders.Num(), EffectSpots.Num(), PresentationTheme.IsNone() ? TEXT("none") : *PresentationTheme.ToString());
	if (MissingArtIds.Num() > 0)
	{
		// A stale Data/assets.json (UAbyssAssetLibrary warns) or an id the art pipeline has not shipped yet.
		TArray<FString> Names;
		for (const FName MissingId : MissingArtIds)
		{
			Names.Add(MissingId.ToString());
		}
		Names.Sort();
		UE_LOG(LogAbyss, Warning, TEXT("Zone %s: %d art placements skipped, no manifest asset for %d ids: %s"),
			*AbyssText::ToFString(Snap.zone.mapId), UnresolvedArtCount, Names.Num(), *FString::Join(Names, TEXT(", ")));
	}
}

FVector AAbyssZoneActor::GroundPoint(double TileX, double TileY) const
{
	const double X = TileX * AbyssUnits::TileUU;
	const double Y = TileY * AbyssUnits::TileUU;
	return FVector(X, Y, Terrain.GetHeight(X, Y));
}

double AAbyssZoneActor::GetGroundHeight(double WorldX, double WorldY) const
{
	return Terrain.GetHeight(WorldX, WorldY);
}

bool AAbyssZoneActor::RaycastGround(const FVector& Origin, const FVector& Direction, FVector& OutHit) const
{
	return Terrain.Raycast(Origin, Direction, 1.0e6, OutHit);
}

// =====================================================================================================================
// Terrain
// =====================================================================================================================

void AAbyssZoneActor::BuildTerrain(const abyss::Snapshot& Snap, const abyss::DataStore& Data)
{
	const abyss::ZoneGrid* Grid = Snap.zone.grid;
	if (Grid == nullptr || Grid->Cols() <= 0 || Grid->Rows() <= 0)
	{
		UE_LOG(LogAbyss, Warning, TEXT("Zone has no grid: terrain skipped"));
		return;
	}
	FAbyssTerrainSettings Settings;
	if (QualityProfile.bMobile && QualityProfile.Tier == 0)
	{
		Settings.SkirtTiles = 14;
	}
	Terrain.Build(*Grid, Settings);

	// Tile-id texture (M_AF_Terrain: TileIds, point sampled, clamped).
	TArray<uint8> Pixels;
	Terrain.BuildTileTexture(Pixels, static_cast<uint8>(WallPaintTile));
	const int32 Cols = Terrain.GetCols();
	const int32 Rows = Terrain.GetRows();
	TileTexture = UTexture2D::CreateTransient(Cols, Rows, PF_B8G8R8A8, FName(TEXT("T_Zone_TileIds")));
	if (TileTexture != nullptr)
	{
		TileTexture->Filter = TF_Nearest;
		TileTexture->SRGB = false;
		TileTexture->CompressionSettings = TC_VectorDisplacementmap;
		TileTexture->AddressX = TA_Clamp;
		TileTexture->AddressY = TA_Clamp;
		TileTexture->NeverStream = true;
		TileTexture->UpdateResource();
		uint8* Copy = new uint8[Pixels.Num()];
		FMemory::Memcpy(Copy, Pixels.GetData(), Pixels.Num());
		FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, static_cast<uint32>(Cols), static_cast<uint32>(Rows));
		TileTexture->UpdateTextureRegions(0, 1, Region, static_cast<uint32>(Cols * 4), 4u, Copy,
			[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
			{
				delete[] SrcData;
				delete Regions;
			});
	}

	UAbyssAssetLibrary* Assets = AssetLibrary.Get();
	FString TerrainMaterialName(TEXT("NONE"));
	FString WaterMaterialName(TEXT("NONE"));
	// The painted ground (WorldContract 3.1): MI_AF_Terrain_<Theme> is M_AF_Terrain_Painted with the theme's art layers
	// and detail atlas bound by the content build (manifest terrain.<theme>). Themes without painted art keep the
	// procedural M_AF_Terrain; both take the same TileIds / Tile<i>* / Water* parameters (ApplyTerrainStyle). A zone with a
	// presentation theme paints that theme (MI_AF_Terrain_March; before the content build made it, the procedural
	// material in the march style colours).
	UMaterialInterface* TerrainBase = nullptr;
	if (Assets != nullptr)
	{
		const FString ThemeKey = AbyssZonePrivate::ToF(TerrainThemeName);
		const FString PaintedName = FString(TEXT("MI_AF_Terrain_")) + ThemeKey.Left(1).ToUpper() + ThemeKey.Mid(1);
		if (!ThemeKey.IsEmpty()
			&& Assets->DoesAssetExist(FAbyssArtManifest::MakeObjectPath(TEXT("Materials/Instances"), FName(*PaintedName))))
		{
			TerrainBase = Assets->LoadMaterial(*PaintedName);
		}
		if (TerrainBase == nullptr)
		{
			TerrainBase = Assets->LoadMaterial(TEXT("M_AF_Terrain"));
		}
	}
	if (TerrainBase != nullptr)
	{
		TerrainMaterial = UMaterialInstanceDynamic::Create(TerrainBase, this);
		TerrainMaterialName = TerrainBase->GetName();
	}
	if (UMaterialInterface* Base = Assets != nullptr ? Assets->LoadMaterial(TEXT("M_AF_Water")) : nullptr)
	{
		WaterMaterial = UMaterialInstanceDynamic::Create(Base, this);
		WaterMaterialName = Base->GetName();
	}
	ApplyTerrainStyle(Data);

	const TArray<FProcMeshTangent> NoTangents;
	int32 Section = 0;
	int32 GroundSections = 0;
	int32 WaterSections = 0;
	for (int32 ChunkY = 0; ChunkY < Terrain.GetNumChunksY(); ++ChunkY)
	{
		for (int32 ChunkX = 0; ChunkX < Terrain.GetNumChunksX(); ++ChunkX)
		{
			FAbyssTerrainMesh Ground;
			FAbyssTerrainMesh Water;
			Terrain.BuildChunkMeshes(ChunkX, ChunkY, Ground, Water);
			if (!Ground.IsEmpty())
			{
				if (GroundSections == 0 && Ground.Triangles.Num() >= 3)
				{
					// UE's front face is (P2 - P0) ^ (P1 - P0); M_AF_Terrain is one-sided, so a down-facing ground is
					// back-face culled from the overhead camera and the world shows the black clear colour.
					const FVector& P0 = Ground.Vertices[Ground.Triangles[0]];
					const FVector FrontNormal = FVector::CrossProduct(Ground.Vertices[Ground.Triangles[2]] - P0,
						Ground.Vertices[Ground.Triangles[1]] - P0);
					UE_CLOG(FrontNormal.Z <= 0.0, LogAbyss, Error,
						TEXT("Terrain triangles face down (front normal %s): the ground is back-face culled"), *FrontNormal.ToString());
				}
				TerrainMesh->CreateMeshSection_LinearColor(Section, Ground.Vertices, Ground.Triangles, Ground.Normals, Ground.UV0,
					Ground.Colors, NoTangents, /*bCreateCollision*/ false);
				if (TerrainMaterial != nullptr)
				{
					TerrainMesh->SetMaterial(Section, TerrainMaterial);
				}
				++Section;
				++GroundSections;
			}
			if (!Water.IsEmpty())
			{
				TerrainMesh->CreateMeshSection_LinearColor(Section, Water.Vertices, Water.Triangles, Water.Normals, Water.UV0,
					Water.Colors, NoTangents, /*bCreateCollision*/ false);
				if (WaterMaterial != nullptr)
				{
					TerrainMesh->SetMaterial(Section, WaterMaterial);
				}
				++Section;
				++WaterSections;
			}
		}
	}
	// Without M_AF_Terrain / M_AF_Water (LoadMaterial warns) the sections draw with the engine default material.
	UE_LOG(LogAbyss, Log, TEXT("Terrain %dx%d: %d ground + %d water sections, bounds %s, material %s, water %s, tile ids %s"),
		Cols, Rows, GroundSections, WaterSections, *Terrain.GetWorldBounds().ToString(),
		*TerrainMaterialName, *WaterMaterialName, TileTexture != nullptr ? TEXT("ok") : TEXT("FAILED"));
}

void AAbyssZoneActor::ApplyTerrainStyle(const abyss::DataStore& Data)
{
	using namespace AbyssZonePrivate;
	if (TerrainMaterial != nullptr)
	{
		if (TileTexture != nullptr)
		{
			TerrainMaterial->SetTextureParameterValue(FName(TEXT("TileIds")), TileTexture);
		}
		TerrainMaterial->SetScalarParameterValue(FName(TEXT("MapCols")), static_cast<float>(Terrain.GetCols()));
		TerrainMaterial->SetScalarParameterValue(FName(TEXT("MapRows")), static_cast<float>(Terrain.GetRows()));
		TerrainMaterial->SetScalarParameterValue(FName(TEXT("WallPaintTile")), static_cast<float>(WallPaintTile));
	}
	if (WaterMaterial != nullptr && TileTexture != nullptr)
	{
		WaterMaterial->SetTextureParameterValue(FName(TEXT("TileIds")), TileTexture);
		WaterMaterial->SetScalarParameterValue(FName(TEXT("MapCols")), static_cast<float>(Terrain.GetCols()));
		WaterMaterial->SetScalarParameterValue(FName(TEXT("MapRows")), static_cast<float>(Terrain.GetRows()));
	}

	// terrain_styles.json (raw table): themes.<theme>.ground.<tile>{base, rank, lip, layers[], accents[], liquid{}}.
	const abyss::JsonValue* Styles = Data.RawTable("terrain_styles.json");
	if (Styles == nullptr)
	{
		UE_LOG(LogAbyss, Warning, TEXT("terrain_styles.json not loaded: M_AF_Terrain draws its default tile colours"));
		return;
	}
	// The presentation theme's style (themes.march), else the generator theme's (a theme without a style keeps today's).
	std::string StyleName = TerrainThemeName.empty() ? ThemeName : TerrainThemeName;
	if (!Styles->Get("themes").Get(StyleName).Get("ground").IsObject())
	{
		StyleName = ThemeName;
	}
	const abyss::JsonValue& ThemeStyle = Styles->Get("themes").Get(StyleName);
	const abyss::JsonValue& Ground = ThemeStyle.Get("ground");
	int32 GroundStyles = 0;
	for (const abyss::JsonMember& Member : Ground.Members())
	{
		++GroundStyles;
		const FString Tile = ToF(Member.key);
		const abyss::JsonValue& Style = Member.value;
		const FLinearColor Base = JsonColor(Style.Get("base"), FLinearColor(0.2f, 0.35f, 0.1f));
		FLinearColor PatchA = Base;
		FLinearColor PatchB = Base;
		const abyss::JsonValue& FirstLayer = Style.Get("layers").At(0);
		if (FirstLayer.Get("colors").IsArray())
		{
			PatchA = JsonColor(FirstLayer.Get("colors").At(0), Base);
			PatchB = JsonColor(FirstLayer.Get("colors").At(1), PatchA);
		}
		const bool bHasLip = Style.Has("lip");
		const FLinearColor Lip = bHasLip ? JsonColor(Style.Get("lip"), Base) : FMath::Lerp(Base, FLinearColor(1.f, 0.88f, 0.67f), 0.25f);
		const FLinearColor Accent = JsonColor(Style.Get("accents").At(0), Base);
		if (TerrainMaterial != nullptr)
		{
			TerrainMaterial->SetVectorParameterValue(Id(TEXT("Tile%sBase"), Tile), Base);
			TerrainMaterial->SetVectorParameterValue(Id(TEXT("Tile%sPatchA"), Tile), PatchA);
			TerrainMaterial->SetVectorParameterValue(Id(TEXT("Tile%sPatchB"), Tile), PatchB);
			TerrainMaterial->SetVectorParameterValue(Id(TEXT("Tile%sLip"), Tile), Lip);
			TerrainMaterial->SetVectorParameterValue(Id(TEXT("Tile%sAccent"), Tile), Accent);
			TerrainMaterial->SetScalarParameterValue(Id(TEXT("Tile%sRank"), Tile), static_cast<float>(Style.Get("rank").AsDouble(0.0)));
		}
		const abyss::JsonValue& Liquid = Style.Get("liquid");
		if (Liquid.IsObject())
		{
			const FLinearColor Shallow = JsonColor(Liquid.Get("shallow"), Base);
			const FLinearColor Foam = JsonColor(Liquid.Get("foam"), FLinearColor::White);
			const FLinearColor Bank = JsonColor(Liquid.Get("bank"), Base * 0.6f);
			FLinearColor Wave = FLinearColor::White;
			for (const abyss::JsonValue& Layer : Style.Get("layers").Items())
			{
				if (Layer.Get("kind").AsString() == std::string_view("wave"))
				{
					Wave = JsonColor(Layer.Get("color"), Wave);
				}
			}
			for (UMaterialInstanceDynamic* Material : { TerrainMaterial.Get(), WaterMaterial.Get() })
			{
				if (Material == nullptr)
				{
					continue;
				}
				Material->SetVectorParameterValue(FName(TEXT("WaterBase")), Base);
				Material->SetVectorParameterValue(FName(TEXT("WaterShallow")), Shallow);
				Material->SetVectorParameterValue(FName(TEXT("WaterFoam")), Foam);
				Material->SetVectorParameterValue(FName(TEXT("WaterBank")), Bank);
				Material->SetVectorParameterValue(FName(TEXT("WaterWave")), Wave);
			}
		}
	}
	UE_CLOG(GroundStyles == 0, LogAbyss, Warning, TEXT("terrain_styles.json has no ground styles for theme '%s': "
		"M_AF_Terrain draws its default tile colours"), *ToF(StyleName));
	UE_CLOG(GroundStyles > 0, LogAbyss, Log, TEXT("Terrain style '%s': %d ground tile styles"), *ToF(StyleName), GroundStyles);
}

// =====================================================================================================================
// Instances
// =====================================================================================================================

const FAbyssArtAsset* AAbyssZoneActor::ResolveArt(TConstArrayView<FName> GameIds, uint32 Hash) const
{
	const UAbyssAssetLibrary* Assets = AssetLibrary.Get();
	// U1: `<id>@<theme>` of the whole chain first in a zone with a presentation theme, then today's chain.
	return Assets != nullptr ? Assets->GetManifest().ResolveFirst(GameIds, Hash, PresentationTheme) : nullptr;
}

bool AAbyssZoneActor::PlaceArt(TConstArrayView<FName> GameIds, uint32 Hash, const FVector& Location, float YawDeg, float Scale,
	bool bOccluder, float FadeTarget, FName StoryId, const FAbyssArtAsset** OutArt, FTransform* OutTransform)
{
	const FAbyssArtAsset* Art = ResolveArt(GameIds, Hash);
	UAbyssAssetLibrary* Assets = AssetLibrary.Get();
	if (Art == nullptr || Assets == nullptr)
	{
		++UnresolvedArtCount;
		if (GameIds.Num() > 0)
		{
			MissingArtIds.Add(GameIds[0]);
		}
		return false;
	}
	UStaticMesh* Mesh = Art->IsSkeletal() ? nullptr : Assets->LoadStaticMesh(*Art);
	if (Mesh == nullptr)
	{
		++UnloadedArtCount;   // LoadStaticMesh logs the missing path once
		return false;
	}
	++PlacedArtCount;
	const float FinalScale = FMath::Max(0.05f, Scale * Art->Scale);
	const FTransform Transform(FRotator(0.f, YawDeg, 0.f), Location, FVector(FinalScale));
	if (OutArt != nullptr)
	{
		*OutArt = Art;
	}
	if (OutTransform != nullptr)
	{
		*OutTransform = Transform;
	}
	const FBox Bounds = Art->BoundsCm.IsValid ? Art->BoundsCm : Mesh->GetBoundingBox();
	const FVector Extent = Bounds.GetExtent() * FinalScale;
	// R1 occlusion radius: the bounds (the camp tents' canvas-width override went with the tents, DECISIONS W11 / R1).
	const float Radius = static_cast<float>(FMath::Max(Extent.X, Extent.Y));
	const float Height = static_cast<float>(Bounds.Max.Z * FinalScale);
	return AddInstance(Mesh, Transform, AbyssZonePrivate::HashUnit(Hash >> 3), bOccluder, FadeTarget, FMath::Max(25.f, Radius),
		FMath::Max(30.f, Height), StoryId);
}

bool AAbyssZoneActor::AddInstance(UStaticMesh* Mesh, const FTransform& Transform, float RandomValue, bool bOccluder,
	float FadeTarget, float RadiusCm, float HeightCm, FName StoryId)
{
	using namespace AbyssZonePrivate;
	if (Mesh == nullptr)
	{
		return false;
	}
	FInstanceBatch& Batch = PendingInstances.FindOrAdd(Mesh);
	const int32 InstanceIndex = Batch.Transforms.Add(Transform);
	Batch.CustomData.Append({ 0.f, RandomValue, 0.f });
	int32 OccluderIndex = INDEX_NONE;
	// Low dressing inside the map can hide ground loot: it fades for loot targets only (UpdateOcclusion).
	const FVector InstanceLocation = Transform.GetLocation();
	const double TileCm = AbyssUnits::TileUU;
	const bool bInsideMap = InstanceLocation.X > -TileCm && InstanceLocation.Y > -TileCm
		&& InstanceLocation.X < Terrain.GetCols() * TileCm && InstanceLocation.Y < Terrain.GetRows() * TileCm;
	const bool bLootCover = !bOccluder && bInsideMap && HeightCm > LootCoverMinHeightCm;
	if (bOccluder || bLootCover)
	{
		FOccluder& Occluder = Occluders.AddDefaulted_GetRef();
		Occluder.Instance = InstanceIndex;   // component resolved in FlushInstances
		Occluder.Location = InstanceLocation;
		Occluder.Transform = Transform;
		Occluder.RandomValue = RandomValue;
		Occluder.RadiusCm = RadiusCm;
		Occluder.HeightCm = HeightCm;
		Occluder.FadeTarget = FadeTarget > 0.f ? FadeTarget : DecorFadeTarget;
		Occluder.bLootOnly = bLootCover;
		// Walls, palisade stakes and camp gates (placed with WallFadeTarget) keep the cylinder test with slack, so a wall
		// line fades without gaps; the other R1 dressing is tested against its bounds ellipsoid (EllipsoidCoversRay).
		Occluder.bEllipsoid = bOccluder && Occluder.FadeTarget != WallFadeTarget;
		OccluderIndex = Occluders.Num() - 1;
		OccluderBuckets.FindOrAdd(BucketOf(Occluder.Location)).Add(OccluderIndex);
	}
	Batch.OccluderSlots.Add(OccluderIndex);
	if (!StoryId.IsNone())
	{
		// The component exists only after FlushInstances: remember the mesh, resolve there.
		PendingStories.Add(FPendingStory{ StoryId, Mesh, InstanceIndex });
	}
	return true;
}

void AAbyssZoneActor::FlushInstances()
{
	using namespace AbyssZonePrivate;
	for (TPair<TObjectPtr<UStaticMesh>, FInstanceBatch>& Pair : PendingInstances)
	{
		UStaticMesh* Mesh = Pair.Key.Get();
		FInstanceBatch& Batch = Pair.Value;
		if (Mesh == nullptr || Batch.Transforms.Num() == 0)
		{
			continue;
		}
		UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		Component->SetStaticMesh(Mesh);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
		Component->SetNumCustomDataFloats(AbyssDecorIcd::Count);
		const float MeshHeight = static_cast<float>(Mesh->GetBoundingBox().Max.Z);
		Component->SetCastShadow(MeshHeight >= ShadowCasterMinHeightCm);
		Component->SetupAttachment(SceneRoot);
		// Hidden before registering, so no proxy is ever drawn with the default material (UpdateShaderGate shows it).
		const bool bShaderGated = GateUntilShadersReady(*Component);
		if (bShaderGated)
		{
			Component->SetVisibility(false);
		}
		Component->RegisterComponent();
		Component->AddInstances(Batch.Transforms, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ true);
		for (int32 Index = 0; Index < Batch.Transforms.Num(); ++Index)
		{
			Component->SetCustomData(Index,
				TArrayView<const float>(Batch.CustomData.GetData() + Index * AbyssDecorIcd::Count, AbyssDecorIcd::Count),
				/*bMarkRenderStateDirty*/ false);
		}
		Component->MarkRenderStateDirty();
		const int32 ComponentIndex = DecorInstanceComponents.Add(Component);
		FadeGhostComponents.AddDefaulted();   // null until the first ghosted fade (GetFadeGhost)
		FadeGhosts.AddDefaulted();
		ComponentByMesh.Add(Mesh, ComponentIndex);
		if (bShaderGated)
		{
			ShaderGatedComponents.Add(ComponentIndex);
		}
		for (int32 Index = 0; Index < Batch.OccluderSlots.Num(); ++Index)
		{
			if (Occluders.IsValidIndex(Batch.OccluderSlots[Index]))
			{
				Occluders[Batch.OccluderSlots[Index]].Component = ComponentIndex;
			}
		}
	}
	for (const FPendingStory& Story : PendingStories)
	{
		if (const int32* ComponentIndex = ComponentByMesh.Find(Story.Mesh))
		{
			StoryInstances.Add(Story.Id, TPair<int32, int32>(*ComponentIndex, Story.Instance));
		}
	}
	PendingStories.Reset();
	PendingInstances.Reset();
	if (ShaderGatedComponents.Num() > 0)
	{
		ShaderGateStartSec = ShaderGateLastSec = FPlatformTime::Seconds();
		ShaderGateWaitedSec = 0.0;
		UE_LOG(LogAbyss, Log, TEXT("Shader gate: %d of %d instanced meshes hidden until their materials finish compiling for "
			"this RHI (uncooked run; Scripts/build_content.sh --shaders precompiles the masters, abyss.DecorShaderGate 0 "
			"draws them at once)"), ShaderGatedComponents.Num(), DecorInstanceComponents.Num());
		SetActorTickEnabled(true);
	}
}

// =====================================================================================================================
// Shader gate (uncooked runs)
// =====================================================================================================================

bool AAbyssZoneActor::GateUntilShadersReady(const UInstancedStaticMeshComponent& Component)
{
#if WITH_EDITOR
	using namespace AbyssZonePrivate;
	const int32 Mode = CVarAbyssDecorShaderGate.GetValueOnGameThread();
	if (Mode <= 0)
	{
		return false;
	}
	TArray<FMaterialResource*> Compiling;
	CollectCompilingMaterials(Component, Compiling);
	if (Compiling.Num() == 0)
	{
		return false;
	}
	if (Mode >= 2)
	{
		for (FMaterialResource* Resource : Compiling)
		{
			Resource->FinishCompilation();   // blocks until this shader map is compiled and applied
		}
		return false;
	}
	return true;
#else
	(void)Component;
	return false;
#endif
}

void AAbyssZoneActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateShaderGate();
}

void AAbyssZoneActor::UpdateShaderGate()
{
#if WITH_EDITOR
	using namespace AbyssZonePrivate;
	// The timeout counts the frames the gate waits through, not wall time: a frame longer than 0.25 s counts 0.25 s.
	// The zone-entry compile wait (abyss.WaitForCompilation) blocks the game thread between FlushInstances and this
	// tick, possibly for longer than the timeout, and must not time the gate out by itself.
	const double NowSec = FPlatformTime::Seconds();
	ShaderGateWaitedSec += FMath::Clamp(NowSec - ShaderGateLastSec, 0.0, 0.25);
	ShaderGateLastSec = NowSec;
	const bool bTimedOut = ShaderGateWaitedSec >= static_cast<double>(CVarAbyssDecorShaderGateMaxSec.GetValueOnGameThread());
	const bool bGateOff = CVarAbyssDecorShaderGate.GetValueOnGameThread() <= 0;
	int32 ShownCompiling = 0;
	TArray<FMaterialResource*> Compiling;
	for (int32 Pending = ShaderGatedComponents.Num() - 1; Pending >= 0; --Pending)
	{
		const int32 ComponentIndex = ShaderGatedComponents[Pending];
		UInstancedStaticMeshComponent* Component =
			DecorInstanceComponents.IsValidIndex(ComponentIndex) ? DecorInstanceComponents[ComponentIndex].Get() : nullptr;
		if (Component != nullptr && !bGateOff)
		{
			// Checked after the timeout too: only a component that is really still compiling is shown with the
			// engine default material (and counted in the warning).
			Compiling.Reset();
			CollectCompilingMaterials(*Component, Compiling);
			if (Compiling.Num() > 0)
			{
				if (!bTimedOut)
				{
					continue;
				}
				++ShownCompiling;
			}
		}
		if (Component != nullptr)
		{
			Component->SetVisibility(true);
		}
		ShaderGatedComponents.RemoveAtSwap(Pending, 1, EAllowShrinking::No);
	}
	if (ShaderGatedComponents.Num() == 0)
	{
		SetActorTickEnabled(false);
		const double WallSec = NowSec - ShaderGateStartSec;
		UE_CLOG(ShownCompiling == 0, LogAbyss, Log, TEXT("Shader gate: zone dressing shown after %.1f s"), WallSec);
		UE_CLOG(ShownCompiling > 0, LogAbyss, Warning, TEXT("Shader gate: %d instanced meshes still compiling after %.0f s "
			"of frames (%.0f s since the zone build), shown with the engine default material until their shaders finish "
			"(abyss.DecorShaderGateMaxSec)"), ShownCompiling, ShaderGateWaitedSec, WallSec);
	}
#else
	ShaderGatedComponents.Reset();
	SetActorTickEnabled(false);
#endif
}

// =====================================================================================================================
// Walls, palisades, decorations, camps, markers, skirt
// =====================================================================================================================

void AAbyssZoneActor::BuildWalls(const abyss::Snapshot& Snap)
{
	using namespace AbyssZonePrivate;
	const abyss::ZoneGrid* Grid = Snap.zone.grid;
	if (Grid == nullptr)
	{
		return;
	}
	for (const abyss::WorldMarkerView& Marker : Snap.markers)
	{
		if (Marker.kind == abyss::MarkerKind::Exit)
		{
			ExitTiles.Add(FIntPoint(FMath::RoundToInt32(Marker.pos.x), FMath::RoundToInt32(Marker.pos.y)));
		}
	}
	auto NearExit = [this](int32 Col, int32 Row)
	{
		for (const FIntPoint& Exit : ExitTiles)
		{
			if (FMath::Abs(Exit.X - Col) <= 1 && FMath::Abs(Exit.Y - Row) <= 1)
			{
				return true;
			}
		}
		return false;
	};
	const FString ThemeText = ToF(ThemeName);
	const FName OutcropIds[] = { Id(TEXT("outcrop_%s"), ThemeText), Id(TEXT("wall_%s"), ThemeText), FName(TEXT("outcrop")),
		FName(TEXT("wall")) };
	const FName StakeIds[] = { Id(TEXT("palisade_%s"), ThemeText), FName(TEXT("palisade")), FName(TEXT("camp_wall")) };
	auto IsCampWall = [Grid](int32 Col, int32 Row)
	{
		return Grid->InBounds(Col, Row) && Grid->Tile(Col, Row) == abyss::TileType::CampWall;
	};

	for (int32 Row = 0; Row < Grid->Rows(); ++Row)
	{
		for (int32 Col = 0; Col < Grid->Cols(); ++Col)
		{
			const abyss::TileType Type = Grid->Tile(Col, Row);
			if (Type == abyss::TileType::Wall)
			{
				if (NearExit(Col, Row))
				{
					continue;   // the exit gate / portal stands there (world-map-nav.md 15.2)
				}
				// Variant v = tileHash(c, r, 3) % variants (the manifest sorts the variants by name: V0..V5).
				const uint32 Hash = AbyssWorldUtil::TileHash(Col, Row, 3);
				const float Yaw = static_cast<float>((Hash >> 8) % 360u);
				PlaceArt(OutcropIds, Hash, GroundPoint(Col, Row), Yaw, 1.f, /*bOccluder*/ true, WallFadeTarget);
			}
			else if (Type == abyss::TileType::CampWall)
			{
				// Centre stake + stakes at 0.2 and 0.4 of the line toward every camp-wall neighbour (OutcropPainter mask).
				const FIntPoint Neighbours[] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
				TArray<FVector2D, TInlineAllocator<9>> Spots;
				Spots.Add(FVector2D(Col, Row));
				for (const FIntPoint& D : Neighbours)
				{
					if (IsCampWall(Col + D.X, Row + D.Y))
					{
						Spots.Add(FVector2D(Col + D.X * 0.2, Row + D.Y * 0.2));
						Spots.Add(FVector2D(Col + D.X * 0.4, Row + D.Y * 0.4));
					}
				}
				for (int32 Index = 0; Index < Spots.Num(); ++Index)
				{
					const uint32 Hash = AbyssWorldUtil::TileHash(Col * 5 + Index, Row, 17);
					const float Yaw = static_cast<float>(Hash % 360u);
					PlaceArt(StakeIds, Hash, GroundPoint(Spots[Index].X, Spots[Index].Y), Yaw, 1.f, /*bOccluder*/ true, WallFadeTarget);
				}
			}
			else if (Type == abyss::TileType::Water)
			{
				// Lily pads on ~1 in 6 water tiles (art-inventory-ch1.md 6.1), floating on the surface.
				const uint32 Hash = AbyssWorldUtil::TileHash(Col, Row, 13);
				if (Hash % 6u == 0u)
				{
					const FName PadIds[] = { Id(TEXT("lily_pad_%s"), ThemeText), FName(TEXT("decor_lily_pad")), FName(TEXT("lily_pad")) };
					FVector Location = AbyssUnits::TileToWorld(abyss::Vec2(Col + (HashUnit(Hash >> 4) - 0.5f) * 0.4f,
						Row + (HashUnit(Hash >> 9) - 0.5f) * 0.4f));
					Location.Z = Terrain.GetSettings().WaterSurfaceCm + 0.5f;
					PlaceArt(PadIds, Hash, Location, static_cast<float>(Hash % 360u), 0.9f + 0.2f * HashUnit(Hash >> 13), false, 0.f);
				}
			}
		}
	}

	// Gate posts at the 2-tile gate of every camp (optional asset, art-inventory-ch1.md 6.3).
	const FName GateIds[] = { Id(TEXT("palisade_gate_%s"), ThemeText), FName(TEXT("palisade_gate")) };
	if (ResolveArt(GateIds, 0) != nullptr)
	{
		for (const abyss::Vec2& Camp : Snap.zone.camps)
		{
			const double GateRow = Camp.y - 5.0;
			PlaceArt(GateIds, 1, GroundPoint(Camp.x - 1.5, GateRow), MeshYawOffsetDeg + 90.f, 1.f, true, WallFadeTarget);
			PlaceArt(GateIds, 2, GroundPoint(Camp.x + 0.5, GateRow), MeshYawOffsetDeg + 90.f, 1.f, true, WallFadeTarget);
		}
	}
}

void AAbyssZoneActor::BuildDecorations(const abyss::Snapshot& Snap, const abyss::DataStore& Data)
{
	using namespace AbyssZonePrivate;
	const abyss::ZoneGrid* Grid = Snap.zone.grid;
	if (Grid == nullptr)
	{
		return;
	}
	const abyss::MapThemeDef& ThemeDef = Data.World().mapGen.Theme(Theme);
	for (const abyss::Decoration& Decor : Grid->Decorations())
	{
		const FString Type = ToF(Decor.type);
		// Same lookup order as the core's W5 footprint bake (MapGen: FindByGameId(type), then "decor_" + type), so the
		// mesh shown is the asset whose footprintTiles / blocking the core baked; Scripts/abyss_content/manifest.py
		// rejects variants of one game id that disagree on footprint or blocking (the hash-picked variant is then safe).
		const FName Ids[] = { FName(*Type), Id(TEXT("decor_%s"), Type) };
		const int32 Col = FMath::RoundToInt32(Decor.col);
		const int32 Row = FMath::RoundToInt32(Decor.row);
		// U4: no grass, flowers or bushes through a set piece (a cottage floor, the dais, the bridge's ditch bed); a
		// blocking decoration stays (the core blocks its tile, and CoreTests keeps every piece off them).
		if (SetPieceState.IsValid() && AbyssSetPieceRules::HidesDecoration(SetPieceState->Spots, abyss::TilePos(Col, Row), Decor.blocking))
		{
			continue;
		}
		const uint32 Hash = AbyssWorldUtil::TileHash(Col, Row, 5);
		bool bTall = Decor.blocking;
		for (const std::string& Tall : ThemeDef.tall)
		{
			bTall = bTall || Tall == Decor.type;
		}
		PlaceArt(Ids, Hash, GroundPoint(Decor.col + Decor.offsetCol, Decor.row + Decor.offsetRow), static_cast<float>(Decor.yawDeg),
			static_cast<float>(Decor.scale), bTall, DecorFadeTarget);
	}
}

void AAbyssZoneActor::BuildCamps(const abyss::Snapshot& Snap, const abyss::DataStore& Data)
{
	using namespace AbyssZonePrivate;
	const abyss::MapDef* Map = Data.FindMap(Snap.zone.mapId);
	if (Map == nullptr)
	{
		return;
	}
	const float FaceCameraYaw = CameraYawDeg + 180.f;   // toward the viewer
	const std::string_view CampTheme = AbyssThemeRules::ThemeOf(Map);
	for (const abyss::CampProp& Prop : abyss::CampProps(*Map))
	{
		if (!AbyssThemeRules::PlacesCampProp(CampTheme, Prop.campIndex, Prop.type))
		{
			continue;   // themed zone: only the hub camp flies the hub's banner (Crowmarket is not Knellwatch)
		}
		const FString Type = ToF(Prop.type);
		const FName Ids[] = { Id(TEXT("camp_%s"), Type), Id(TEXT("decor_camp_%s"), Type), FName(*Type) };
		// Camp layout v2 (DECISIONS W11, abyss/world/CampLayout.h; no tents): the core gives the sub-tile centre and the
		// front (the well, barrels and crates turn to the camp centre); a prop without a front faces the camera.
		const FVector Location = GroundPoint(Prop.centre.x, Prop.centre.y);
		const bool bHasFront = Prop.front.x != 0.0 || Prop.front.y != 0.0;
		const float FacingYaw = bHasFront
			? static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Prop.front.y, Prop.front.x)))
			: FaceCameraYaw;
		const bool bTall = Prop.type == "well" || Prop.type == "banner";
		const uint32 Hash = AbyssWorldUtil::TileHash(Prop.pos.col, Prop.pos.row, 23);
		PlaceArt(Ids, Hash, Location, FacingYaw + MeshYawOffsetDeg, 1.f, bTall, DecorFadeTarget);
		if (Prop.type == "campfire")
		{
			AddEffectSpot(FName(TEXT("camp.campfire")), Location);
		}
		else if (Prop.type == "torch")
		{
			AddEffectSpot(FName(TEXT("camp.torch")), Location);   // march: camp.torch@march, the brazier's smoulder (no light)
		}
	}
}

void AAbyssZoneActor::BuildMarkers(const abyss::Snapshot& Snap, const abyss::DataStore& Data)
{
	using namespace AbyssZonePrivate;
	const abyss::MapDef* Map = Data.FindMap(Snap.zone.mapId);
	const FVector2D MapCentre(Snap.zone.cols * 0.5, Snap.zone.rows * 0.5);
	for (const abyss::WorldMarkerView& Marker : Snap.markers)
	{
		if (Marker.id != abyss::kNoEntity)
		{
			continue;   // entity markers are prop actors (world builder)
		}
		const FVector Location = GroundPoint(Marker.pos.x, Marker.pos.y);
		if (Marker.kind == abyss::MarkerKind::Exit)
		{
			const FVector2D Inward = (MapCentre - FVector2D(Marker.pos.x, Marker.pos.y)).GetSafeNormal();
			const float FacingYaw = FMath::RadiansToDegrees(FMath::Atan2(Inward.Y, Inward.X));
			const FName SealedIds[] = { FName(TEXT("exit_gate_sealed")), FName(TEXT("exit_sealed")), FName(TEXT("exit_portal")) };
			const FName OpenIds[] = { FName(TEXT("exit_portal")) };
			if (Marker.sealed)
			{
				PlaceArt(SealedIds, 0, Location, FacingYaw + MeshYawOffsetDeg, 1.f, true, DecorFadeTarget);
			}
			else
			{
				PlaceArt(OpenIds, 0, Location, FacingYaw + MeshYawOffsetDeg, 1.f, true, DecorFadeTarget);
			}
			AddEffectSpot(Marker.sealed ? FName(TEXT("exit.sealed")) : FName(TEXT("exit.portal")), Location);
		}
		else if (Marker.kind == abyss::MarkerKind::StoryDecoration)
		{
			FString SpriteType;
			if (Map != nullptr)
			{
				for (const abyss::StoryDecorationDef& Story : Map->storyDecorations)
				{
					if (Story.id == Marker.key)
					{
						SpriteType = ToF(Story.spriteType);
					}
				}
			}
			const FString DecorId = ToF(Marker.key);
			const FName Ids[] = { Id(TEXT("decor_%s"), SpriteType), FName(*SpriteType), Id(TEXT("story_%s"), DecorId) };
			const uint32 Hash = AbyssWorldUtil::TileHash(FMath::RoundToInt32(Marker.pos.x), FMath::RoundToInt32(Marker.pos.y), 31);
			PlaceArt(Ids, Hash, Location, CameraYawDeg + 180.f + MeshYawOffsetDeg, 1.f, true, DecorFadeTarget, FName(*DecorId));
		}
	}
}

void AAbyssZoneActor::BuildSkirt(const abyss::DataStore& Data)
{
	using namespace AbyssZonePrivate;
	if (!Terrain.IsValid())
	{
		return;
	}
	// Rolling skirt dressing so the camera never shows bare hills (art-inventory-ch1.md 6.10): the theme's grove pool.
	const abyss::MapThemeDef& ThemeDef = Data.World().mapGen.Theme(Theme);
	const std::vector<abyss::WeightedDecor>& Grove = ThemeDef.grove;
	const int32 Skirt = Terrain.GetSettings().SkirtTiles;
	const int32 Cols = Terrain.GetCols();
	const int32 Rows = Terrain.GetRows();
	const uint32 Density = QualityProfile.bMobile && QualityProfile.Tier == 0 ? 11u : 6u;
	for (int32 Row = -Skirt; Row < Rows + Skirt; ++Row)
	{
		for (int32 Col = -Skirt; Col < Cols + Skirt; ++Col)
		{
			const int32 Outside = FMath::Max(FMath::Max(-Col, Col - (Cols - 1)), FMath::Max(-Row, Row - (Rows - 1)));
			if (Outside < 2)
			{
				continue;
			}
			const uint32 Hash = AbyssWorldUtil::TileHash(Col, Row, 29);
			if (Hash % Density != 0u)
			{
				continue;
			}
			const std::string* Type = PickWeighted(Grove, Hash >> 5);
			if (Type == nullptr)
			{
				continue;
			}
			const FString TypeText = ToF(*Type);
			const FName Ids[] = { Id(TEXT("decor_%s"), TypeText), FName(*TypeText) };
			const double Jx = (HashUnit(Hash >> 7) - 0.5) * 0.8;
			const double Jy = (HashUnit(Hash >> 11) - 0.5) * 0.8;
			PlaceArt(Ids, Hash, GroundPoint(Col + Jx, Row + Jy), static_cast<float>((Hash >> 3) % 360u),
				0.9f + 0.25f * HashUnit(Hash >> 15), /*bOccluder*/ Outside <= 4, DecorFadeTarget);
		}
	}
}

// =====================================================================================================================
// Lighting and look
// =====================================================================================================================

void AAbyssZoneActor::BuildLighting(const abyss::Snapshot& Snap, const abyss::DataStore& Data)
{
	using namespace AbyssZonePrivate;
	// The mood at the hero's tile (sun, fog, grade values; ambience; light scale; wind-up tints). The post-process is
	// applied by ApplyQuality at the end of BuildZone.
	bMoodApplied = false;
	UpdateMood(Snap, Data);

	// SSAO composite (P14; desktop balanced / high). The exact web grade (M_AF_PP_Grade) is no longer bound: the built-in
	// grade at 55 % of the mood replaces it (ApplyPostProcess).
	AOCompositeMaterial = nullptr;
	if (UAbyssAssetLibrary* Assets = AssetLibrary.Get())
	{
		static const TCHAR* const Folders[] = { TEXT("Materials"), TEXT("Materials/PostProcess") };
		AOCompositeMaterial = Cast<UMaterialInterface>(
			Assets->LoadFromFolders(Folders, FName(TEXT("M_AF_PP_AOComposite")), UMaterialInterface::StaticClass()));
	}
}

void AAbyssZoneActor::UpdateMood(const abyss::Snapshot& Snap, const abyss::DataStore& Data)
{
	using namespace AbyssZonePrivate;
	// zone_moods.json (world-map-nav.md 14.1-14.2): the zone's mood theme (themeByZone; an unknown zone id uses its
	// generator theme), the dawn mood after the chapter and the mood regions of the dark remake (AbyssThemeRules).
	AbyssThemeRules::FMoodInputs In;
	In.Moods = &Data.World().moods;
	In.Map = Data.FindMap(Snap.zone.mapId);
	In.ZoneId = Snap.zone.mapId;
	In.GeneratorTheme = Theme;
	// Dawn (2G stage H, AbyssWorldStateRules::ChapterDawn): a world flag the closing quest sets (rf_chapter_dawn, the flag
	// the core's dawn spawn variants read) is set; a closing quest without flags keeps Wave 1's rule (its turn-in).
	const std::string& ChapterQuest = Data.Monsters().ai.chapterCompleteQuest;
	const abyss::QuestDef* ClosingQuest = ChapterQuest.empty() ? nullptr : Data.FindQuest(ChapterQuest);
	const bool bClosingTurnedIn = ClosingQuest != nullptr && Snap.quests != nullptr && Snap.quests->IsTurnedIn(ChapterQuest);
	In.bChapterComplete = AbyssWorldStateRules::ChapterDawn(ClosingQuest, Snap.worldFlags, bClosingTurnedIn);
	In.bHasHero = true;
	In.HeroCol = Snap.hero.pos.x;
	In.HeroRow = Snap.hero.pos.y;
	In.bRegionWasDominant = bMoodApplied && bMoodRegionDominant;
	const AbyssThemeRules::FMoodState State = AbyssThemeRules::EffectiveMood(In);

	AbyssThemeRules::FMoodState Applied;
	Applied.Base = AppliedMoodBase;
	Applied.Region = AppliedMoodRegion;
	Applied.bHasRegion = bAppliedMoodHasRegion;
	Applied.RegionWeight = AppliedMoodRegionWeight;
	if (!bMoodApplied || AbyssThemeRules::LookChanged(Applied, State))
	{
		// Live changes (dawn, walking into the Breach Rim) re-apply the fog and post-process now; the zone build applies
		// them once at its end (ApplyQuality).
		ApplyMoodDef(State.Mood, /*bLive*/ bMoodApplied);
		AppliedMoodBase = State.Base;
		AppliedMoodRegion = State.Region;
		bAppliedMoodHasRegion = State.bHasRegion;
		AppliedMoodRegionWeight = State.RegionWeight;
		if (bMoodApplied)
		{
			UE_LOG(LogAbyss, Verbose, TEXT("Zone mood %s -> %s weight %.2f"), *ToF(abyss::EnumName(State.Base)),
				*ToF(abyss::EnumName(State.Region)), State.RegionWeight);
		}
	}
	const bool bFirstFrame = !bMoodApplied;
	bMoodRegionDominant = State.bRegionDominant;
	bMoodApplied = true;

	// DECISIONS R18 override (port.lightScale), blended over the region; < 0 keeps the formula (UAbyssVfxSystem).
	MoodLightScaleOverride = State.bLightScaleOverride ? State.LightScale : -1.f;

	// The ambience and the wind-up tints depend on the dominant and base moods only (the zone and its map are fixed for
	// this actor's build): recomputed when one of them changes or the zone was (re)built, not every frame.
	if (bFirstFrame || !bMoodLookCached || State.Dominant != CachedLookDominant || State.Base != CachedLookBase)
	{
		bMoodLookCached = true;
		CachedLookDominant = State.Dominant;
		CachedLookBase = State.Base;
		// Weather / ambience (world-map-nav.md 14.2): the dominant mood's port.moodWeather, else the zone's own weather;
		// the weather tint colours the recipe only in a zone with a presentation theme (ashfall #CFCAC0, the Rim's sparks).
		const std::string Recipe = AbyssThemeRules::AmbienceRecipeId(State);
		AmbienceRecipe = Recipe.empty() ? NAME_None : FName(*ToF(Recipe));
		bAmbienceHasColor = State.bHasTint;
		AmbienceColor = State.bHasTint ? AbyssWorldUtil::ColorFromRgb(State.Tint) : FLinearColor::White;
		// DR5 wind-up tint of the dominant mood (port.windupTint), else the base mood's, else the web port's tints.
		const AbyssThemeRules::FWindupTints Tints = AbyssThemeRules::WindupTintFor(Data.RawTable("zone_moods.json"),
			abyss::EnumName(State.Dominant), abyss::EnumName(State.Base));
		WindupTintNormal = AbyssWorldUtil::ColorFromRgb(Tints.Normal);
		WindupTintElite = AbyssWorldUtil::ColorFromRgb(Tints.Elite);
	}
}

void AAbyssZoneActor::ApplyMoodDef(const abyss::ZoneMoodDef& MoodDef, bool bLive)
{
	using namespace AbyssZonePrivate;
	Mood.Ambient = AbyssWorldUtil::ColorFromRgb(MoodDef.ambient);
	Mood.AmbientAlpha = static_cast<float>(MoodDef.ambientAlpha);
	Mood.Vignette = AbyssWorldUtil::ColorFromRgb(MoodDef.vignette);
	Mood.VignetteAlpha = static_cast<float>(MoodDef.vignetteAlpha);
	Mood.Haze = AbyssWorldUtil::ColorFromRgb(MoodDef.haze);
	Mood.HazeAlpha = static_cast<float>(MoodDef.hazeAlpha);
	Mood.Saturation = static_cast<float>(MoodDef.saturation);
	Mood.Contrast = static_cast<float>(MoodDef.contrast);
	Mood.Lift = FVector(MoodDef.lift[0], MoodDef.lift[1], MoodDef.lift[2]);
	Mood.Gain = FVector(MoodDef.gain[0], MoodDef.gain[1], MoodDef.gain[2]);

	// Sun: the manifest key light (the toon materials and the ground must agree), warm mood tint. DECISIONS P11 (art
	// pass): the shadow-catcher toon is DefaultLit with BaseColor k (lit - shade) under a fake normal = KeyLightDir, so a
	// pi-lux sun of SunColor (N.L = 1) gives exactly the toon colour in full sun and the shade band in a cast shadow. The
	// lit ground keeps its own sun term through MPC_AF_Lighting SunScale (abyss.SunScale, written by the world builder).
	// The light keeps its colour as 8-bit sRGB bytes and the renderer decodes them (ULightComponent::SetLightColor ->
	// FLinearColor::ToFColor(bSRGB); GetColoredLightBrightness -> FLinearColor(FColor), the sRGB table): the bytes are
	// sRGB-encoded here and SunColor (MPC_AF_Lighting) is their decoded value, so the toon's BaseColor / SunColor and
	// the light colour cancel exactly. (Storing the linear value as raw bytes decoded it twice: up to 48 % less per
	// channel on toon and ground than the MPC said.)
	FColor SunBytes = FMath::Lerp(FLinearColor::White, Mood.Ambient, 0.5f).ToFColor(/*bSRGB*/ true);
	SunBytes.A = 255;
	SunColor = FLinearColor(SunBytes);
	const float Fill = CVarAbyssGroundAmbient.GetValueOnGameThread();
	AmbientFill = FLinearColor(Fill * 0.92f, Fill * 0.95f, Fill * 1.08f, 1.f);
	if (Sun != nullptr)
	{
		Sun->SetWorldRotation((-DirToLight).Rotation());
		Sun->SetIntensity(UE_PI * 1.f);
		Sun->SetLightFColor(SunBytes);
		if (!Sun->GetLightColor().Equals(SunColor, 1.e-6f))
		{
			UE_LOG(LogAbyss, Warning, TEXT("Sun light colour %s differs from MPC SunColor %s: the lit toon will not match"),
				*Sun->GetLightColor().ToString(), *SunColor.ToString());
		}
	}

	// Height fog (P14, zone_moods.json port.fog).
	Mood.FogColor = AbyssWorldUtil::ColorFromRgb(MoodDef.fogColor);
	Mood.FogMaxOpacity = static_cast<float>(FMath::Clamp(MoodDef.fogMaxOpacity, 0.0, 1.0));
	Mood.FogDensity = static_cast<float>(FMath::Max(0.0, MoodDef.fogDensity));
	ApplyFog();
	if (bLive)
	{
		ApplyPostProcess();
	}
}

void AAbyssZoneActor::SetUserExposureEv(float Ev)
{
	UserExposureEv = FMath::IsFinite(Ev) ? Ev : 0.f;
	if (!FMath::IsNearlyEqual(UserExposureEv, AppliedUserExposureEv, 1.e-4f))
	{
		ApplyPostProcess();
	}
}

void AAbyssZoneActor::AddEffectSpot(FName RecipeId, const FVector& Location)
{
	FAbyssZoneEffectSpot& Spot = EffectSpots.AddDefaulted_GetRef();
	Spot.RecipeId = RecipeId;
	Spot.Location = Location;
	// An id that names its theme already (a set piece's camp.torch@march) is played as it is.
	if (!PresentationTheme.IsNone() && !RecipeId.ToString().Contains(TEXT("@")))
	{
		Spot.ThemedRecipeId = FName(*(RecipeId.ToString() + TEXT("@") + PresentationTheme.ToString()));
		Spot.bThemedOnly = AbyssThemeRules::ThemedOnlyRecipe(AbyssText::ToStd(RecipeId.ToString()));
	}
}

float AAbyssZoneActor::GetSunScale()
{
	return FMath::Max(0.f, CVarAbyssSunScale.GetValueOnGameThread());
}

void AAbyssZoneActor::ApplyQuality(const FAbyssQualityProfile& Quality)
{
	using namespace AbyssZonePrivate;
	QualityProfile = Quality;
	if (Sun != nullptr)
	{
		// DECISIONS P10 (revised, art pass; binding over ue58-platform.md 6.2 / 6.8): CSM with 2 cascades and a linear
		// split on desktop (every tier), 1 cascade on high-tier mobile; low / mid mobile have no shadow-depth pass at all
		// (blob shadows only, matching UAbyssWorldBuilder's bCsm = !mobile || tier >= 2). Device profiles cap
		// r.Shadow.CSM.MaxMobileCascades the same way; the world builder sets r.Shadow.MaxCSMResolution & co. The
		// distance follows the camera (ApplyViewDistance).
		const bool bShadows = Quality.bMobile ? Quality.Tier >= 2 : true;
		Sun->SetCastShadows(bShadows);
		Sun->SetDynamicShadowCascades(Quality.bMobile ? MobileCascades : DesktopCascades);
		Sun->SetCascadeDistributionExponent(CascadeDistributionExponent);
		// Screen-space contact shadows (the feet / blade on the ground) on desktop balanced / high.
		const float Contact = !Quality.bMobile && Quality.Tier >= 1 ? ContactShadowLength : 0.f;
		if (!FMath::IsNearlyEqual(Sun->ContactShadowLength, Contact))
		{
			Sun->ContactShadowLength = Contact;
			Sun->MarkRenderStateDirty();
		}
	}
	AppliedViewDistanceCm = -1.f;
	ApplyViewDistance();
	ApplyFog();
	ApplyPostProcess();
}

void AAbyssZoneActor::UpdateView(float CameraDistanceCm)
{
	using namespace AbyssZonePrivate;
	if (CameraDistanceCm > 1.f)
	{
		ViewDistanceCm = CameraDistanceCm;
	}
	if (AppliedViewDistanceCm <= 0.f || FMath::Abs(ViewDistanceCm - AppliedViewDistanceCm) > ViewRefitFraction * AppliedViewDistanceCm)
	{
		ApplyViewDistance();
	}
	// Look console variables, live (a handful of reads per frame).
	const float FogScale = CVarAbyssFogScale.GetValueOnGameThread();
	const int32 FogMode = CVarAbyssFog.GetValueOnGameThread();
	if (FogMode != AppliedFogMode || FogScale != AppliedFogScale)
	{
		ApplyFog();
	}
	if (CVarAbyssExposureBias.GetValueOnGameThread() != AppliedExposureBias
		|| CVarAbyssToneCurve.GetValueOnGameThread() != AppliedToneCurve
		|| CVarAbyssBloom.GetValueOnGameThread() != AppliedBloom)
	{
		ApplyPostProcess();
	}
}

void AAbyssZoneActor::ApplyViewDistance()
{
	using namespace AbyssZonePrivate;
	AppliedViewDistanceCm = ViewDistanceCm;
	if (Sun != nullptr)
	{
		Sun->SetDynamicShadowDistanceMovableLight(ShadowDistanceScale * ViewDistanceCm);
	}
	if (HeightFog != nullptr)
	{
		HeightFog->SetStartDistance(ViewDistanceCm + FogStartBeyondFocusCm);
	}
}

void AAbyssZoneActor::ApplyFog()
{
	using namespace AbyssZonePrivate;
	AppliedFogMode = CVarAbyssFog.GetValueOnGameThread();
	AppliedFogScale = CVarAbyssFogScale.GetValueOnGameThread();
	if (HeightFog == nullptr)
	{
		return;
	}
	// UE's height fog: transmittance = 2^(-ln2 x FogDensity / 1000 x height term x cm along the ray past the start),
	// capped at FogMaxOpacity. Plains 0.85: ~0.20 at the top screen edge (ground ~26 m away at the default framing), 0 on
	// the hero. No directional inscattering (no sky atmosphere), no volumetric fog. Mobile renders it per vertex.
	const bool bOn = AppliedFogMode != 0 && Mood.FogMaxOpacity > 0.f && Mood.FogDensity > 0.f;
	HeightFog->SetVisibility(bOn);
	HeightFog->SetFogInscatteringColor(Mood.FogColor);
	HeightFog->SetFogMaxOpacity(Mood.FogMaxOpacity);
	HeightFog->SetFogDensity(Mood.FogDensity * FogDensityToUE * FMath::Max(0.f, AppliedFogScale));
	HeightFog->SetFogHeightFalloff(FogHeightFalloff);
	HeightFog->SetDirectionalInscatteringColor(FLinearColor::Black);
	HeightFog->SetVolumetricFog(false);
}

void AAbyssZoneActor::ApplyPostProcess()
{
	using namespace AbyssZonePrivate;
	AppliedExposureBias = CVarAbyssExposureBias.GetValueOnGameThread();
	AppliedToneCurve = CVarAbyssToneCurve.GetValueOnGameThread();
	AppliedBloom = CVarAbyssBloom.GetValueOnGameThread();
	if (PostProcess == nullptr)
	{
		return;
	}
	FPostProcessSettings& S = PostProcess->Settings;
	S = FPostProcessSettings();

	// Stable toon colours (ue58-platform.md 6.7): manual exposure, recalibrated for the tone curve (P14).
	S.bOverride_AutoExposureMethod = true;
	S.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	S.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	S.AutoExposureApplyPhysicalCameraExposure = false;
	S.bOverride_AutoExposureBias = true;
	// The brightness setting (U1, settings.json video.brightness: log2 of 0.5..1.5) adds to the calibrated bias.
	AppliedUserExposureEv = UserExposureEv;
	S.AutoExposureBias = AppliedExposureBias + AppliedUserExposureEv;
	// DECISIONS P14 (art pass): a partial filmic tone curve on every tier (Film* keep the engine defaults), no blue
	// correction / gamut expansion, no local exposure.
	S.bOverride_ToneCurveAmount = true;
	S.ToneCurveAmount = FMath::Clamp(AppliedToneCurve >= 0.f ? AppliedToneCurve : ToneCurveFallback, 0.f, 1.f);
	S.bOverride_BlueCorrection = true;
	S.BlueCorrection = 0.f;
	S.bOverride_ExpandGamut = true;
	S.ExpandGamut = 0.f;
	S.bOverride_LocalExposureHighlightContrastScale = true;
	S.LocalExposureHighlightContrastScale = 1.f;
	S.bOverride_LocalExposureShadowContrastScale = true;
	S.LocalExposureShadowContrastScale = 1.f;
	S.bOverride_MotionBlurAmount = true;
	S.MotionBlurAmount = 0.f;
	S.bOverride_SceneFringeIntensity = true;
	S.SceneFringeIntensity = 0.f;
	// Film grain (5.1+ FilmGrain*; the legacy GrainIntensity fields are *_DEPRECATED). 0 is the default; kept explicit.
	S.bOverride_FilmGrainIntensity = true;
	S.FilmGrainIntensity = 0.f;

	// SSAO (P14): desktop balanced / high only (r.AmbientOcclusionLevels 1 / 2 from the world builder), composited by
	// M_AF_PP_AOComposite when the content build made it. Low tier and mobile: none.
	const bool bAO = !QualityProfile.bMobile && QualityProfile.Tier >= 1;
	S.bOverride_AmbientOcclusionIntensity = true;
	S.AmbientOcclusionIntensity = bAO ? AOIntensity : 0.f;
	if (bAO)
	{
		S.bOverride_AmbientOcclusionRadius = true;
		S.AmbientOcclusionRadius = AORadiusCm;
		S.bOverride_AmbientOcclusionQuality = true;
		S.AmbientOcclusionQuality = AOQuality;
	}

	// Bloom (balanced / high only).
	S.bOverride_BloomIntensity = true;
	const float Bloom = AppliedBloom >= 0.f ? AppliedBloom : (ArtBloomStrength > 0.f ? ArtBloomStrength : 0.6f);
	S.BloomIntensity = QualityProfile.bBloom ? Bloom : 0.f;
	S.bOverride_BloomThreshold = true;
	S.BloomThreshold = 1.f;

	// Mood vignette (dark mood colour at the edges).
	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = QualityProfile.bColorGrading ? Mood.VignetteAlpha * 0.9f : 0.f;

	S.WeightedBlendables.Array.Reset();
	if (QualityProfile.bColorGrading)
	{
		// DECISIONS P14: the engine's grade at 55 % of the zone mood (no exact web grade): ambient multiply -> gain (at half
		// its alpha), saturation (+6 %) / contrast, lift -> shadows offset, gain -> highlights offset. The web haze is the
		// height fog now (no global ColorOffset).
		const float K = GradeMoodFraction;
		const FLinearColor AmbientMul = FMath::Lerp(FLinearColor::White, Mood.Ambient, Mood.AmbientAlpha * 0.5f);
		S.bOverride_ColorGain = true;
		S.ColorGain = FVector4(AmbientMul.R, AmbientMul.G, AmbientMul.B, 1.0);
		S.bOverride_ColorSaturation = true;
		S.ColorSaturation = FVector4(1.0, 1.0, 1.0, GradeSaturationBoost * (1.f + (Mood.Saturation - 1.f) * K));
		S.bOverride_ColorContrast = true;
		S.ColorContrast = FVector4(1.0, 1.0, 1.0, 1.f + (Mood.Contrast - 1.f) * K);
		S.bOverride_ColorOffsetShadows = true;
		S.ColorOffsetShadows = FVector4(Mood.Lift.X * K, Mood.Lift.Y * K, Mood.Lift.Z * K, 0.0);
		S.bOverride_ColorOffsetHighlights = true;
		S.ColorOffsetHighlights = FVector4(Mood.Gain.X * K, Mood.Gain.Y * K, Mood.Gain.Z * K, 0.0);
	}
	const bool bAOComposite = bAO && AOCompositeMaterial != nullptr;
	if (bAOComposite)
	{
		S.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, AOCompositeMaterial));
	}
	// The composite darkens the visible ground only: the terrain alone renders custom depth while it is bound, and a pixel
	// is ground where the scene depth reaches it (hlsl.ao_composite_node). Characters, props and foliage keep their
	// colour: the lit toon's GBuffer normal is the constant KeyLightDir (P11), which classic SSAO orients its hemisphere by.
	if (TerrainMesh != nullptr)
	{
		TerrainMesh->SetRenderCustomDepth(bAOComposite);
	}
	PostProcess->MarkRenderStateDirty();
}

// =====================================================================================================================
// Occlusion fade and story focus
// =====================================================================================================================

void AAbyssZoneActor::UpdateOcclusion(TConstArrayView<FVector> Targets, TConstArrayView<FVector> LootTargets,
	const FVector& CameraLocation, float DeltaSec)
{
	using namespace AbyssZonePrivate;
	if (Occluders.Num() == 0)
	{
		return;
	}
	TSet<int32> Covering;
	const int32 TargetCount = Targets.Num() + LootTargets.Num();
	for (int32 TargetIndex = 0; TargetIndex < TargetCount; ++TargetIndex)
	{
		// Characters first (R1 occluders only); ground loot fades only the low dressing (bLootOnly), with a narrower
		// footprint. Trees, walls and camp structures never fade for loot: ground loot persists, so such a fade would
		// last until pickup (the web draws loot behind its decor, ZoneScene.dropLootAtPosition; the nameplate stays).
		const bool bLoot = TargetIndex >= Targets.Num();
		const FVector& Target = bLoot ? LootTargets[TargetIndex - Targets.Num()] : Targets[TargetIndex];
		const double SlackCm = bLoot ? LootOccluderSlackCm : OccluderSlackCm;
		const FVector ToCamera = CameraLocation - Target;
		const FVector2D Flat(ToCamera.X, ToCamera.Y);
		const double FlatLength = Flat.Size();
		if (FlatLength < 1.0)
		{
			continue;
		}
		const FVector2D Dir = Flat / FlatLength;
		const double TanElevation = ToCamera.Z / FlatLength;
		// Buckets around the target, extended toward the camera (tall props up to ~10 m in front can cover it).
		const FIntPoint Centre = BucketOf(Target);
		for (int32 By = Centre.Y - 2; By <= Centre.Y + 2; ++By)
		{
			for (int32 Bx = Centre.X - 2; Bx <= Centre.X + 2; ++Bx)
			{
				const TArray<int32>* Bucket = OccluderBuckets.Find(FIntPoint(Bx, By));
				if (Bucket == nullptr)
				{
					continue;
				}
				for (const int32 Index : *Bucket)
				{
					const FOccluder& Occluder = Occluders[Index];
					if (Occluder.bLootOnly != bLoot || Occluder.bSuppressed)
					{
						continue;
					}
					const FVector2D Rel(Occluder.Location.X - Target.X, Occluder.Location.Y - Target.Y);
					const double Along = FVector2D::DotProduct(Rel, Dir);
					if (Along < -Occluder.RadiusCm || Along > 1200.0)
					{
						continue;
					}
					const double Lateral = FMath::Abs(FVector2D::CrossProduct(Dir, Rel));
					if (Occluder.bEllipsoid)
					{
						// R1 decor: a target standing behind the prop (15.6; behind its centre, seen from the camera)
						// whose ray toward the camera enters the prop's bounds ellipsoid.
						const double SemiHeightCm = Occluder.HeightCm * 0.5;
						if (Along >= 0.0 && Lateral <= Occluder.RadiusCm
							&& EllipsoidCoversRay(Along, Lateral, Target.Z - Occluder.Location.Z - SemiHeightCm, TanElevation,
								Occluder.RadiusCm, SemiHeightCm))
						{
							Covering.Add(Index);
						}
						continue;
					}
					if (Lateral > Occluder.RadiusCm + SlackCm)
					{
						continue;
					}
					const double RayZ = Target.Z + FMath::Max(0.0, Along - Occluder.RadiusCm * 0.5) * TanElevation;
					if (RayZ < Occluder.Location.Z + Occluder.HeightCm)
					{
						Covering.Add(Index);
					}
				}
			}
		}
	}

	// Ghost mode (abyss.FadeGhost) can change with the quality tier or the console: re-apply every faded occluder then.
	const bool bGhostMode = UseFadeGhosts();
	const bool bModeChanged = bGhostMode != bFadeGhostMode;
	bFadeGhostMode = bGhostMode;
	TSet<int32> Dirty;
	TSet<int32> DirtyGhosts;
	const float Blend = FMath::Min(1.f, DeltaSec / FadeTimeConstantSec);
	for (int32 Index = 0; Index < Occluders.Num(); ++Index)
	{
		FOccluder& Occluder = Occluders[Index];
		const float Target = Covering.Contains(Index) ? Occluder.FadeTarget : 0.f;
		if (Occluder.Fade == Target && !(bModeChanged && (Occluder.Fade > 0.f || Occluder.bGhosted)))
		{
			continue;
		}
		float Fade = Occluder.Fade + (Target - Occluder.Fade) * Blend;
		if (FMath::Abs(Fade - Target) < 0.02f)
		{
			Fade = Target;
		}
		Occluder.Fade = Fade;
		ApplyOccluderFade(Index, bGhostMode, Dirty, DirtyGhosts);
	}
	// Incremental instance-data update (5.4+ instance data manager); MarkRenderStateDirty would re-create the decor
	// proxy every frame of a fade (and reset its TSR history).
	for (const int32 ComponentIndex : Dirty)
	{
		DecorInstanceComponents[ComponentIndex]->MarkRenderInstancesDirty();
	}
	for (const int32 ComponentIndex : DirtyGhosts)
	{
		UpdateFadeGhost(ComponentIndex);
	}
}

bool AAbyssZoneActor::UseFadeGhosts() const
{
	using namespace AbyssZonePrivate;
	const int32 Mode = CVarAbyssFadeGhost.GetValueOnGameThread();
	if (Mode <= 0 || FadeToonMaster == nullptr)
	{
		return false;
	}
	return Mode >= 2 || !IsTemporalAntiAliasing(QualityProfile.bMobile);
}

void AAbyssZoneActor::ApplyOccluderFade(int32 Index, bool bGhostMode, TSet<int32>& DirtyComponents, TSet<int32>& DirtyGhosts)
{
	FOccluder& Occluder = Occluders[Index];
	UInstancedStaticMeshComponent* Component =
		DecorInstanceComponents.IsValidIndex(Occluder.Component) ? DecorInstanceComponents[Occluder.Component].Get() : nullptr;
	if (Component == nullptr)
	{
		return;
	}
	const bool bWasGhosted = Occluder.bGhosted;
	const float MinFade = bWasGhosted ? AbyssZonePrivate::GhostLeaveFade : AbyssZonePrivate::GhostEnterFade;
	const bool bGhost = bGhostMode && Occluder.Fade >= MinFade && GetFadeGhost(Occluder.Component) != nullptr;
	if (bGhost != bWasGhosted)
	{
		FFadeGhost& Ghost = FadeGhosts[Occluder.Component];
		if (bGhost)
		{
			Ghost.Occluders.Add(Index);
		}
		else
		{
			Ghost.Occluders.Remove(Index);
		}
		Ghost.bDirty = true;
		Occluder.bGhosted = bGhost;
	}
	// A ghosted instance is hidden in its masked ISM: fade 1 keeps no pixel of AF_Dither (body and ink hull) and the
	// shadow pass ignores the instance fade, so the tree still casts its shadow. Written once when it is ghosted.
	if (!bGhost || !bWasGhosted)
	{
		Component->SetCustomDataValue(Occluder.Instance, AbyssDecorIcd::Fade, bGhost ? 1.f : Occluder.Fade, false);
		DirtyComponents.Add(Occluder.Component);
	}
	if (bGhost || bWasGhosted)
	{
		DirtyGhosts.Add(Occluder.Component);
	}
}

UInstancedStaticMeshComponent* AAbyssZoneActor::GetFadeGhost(int32 ComponentIndex)
{
	if (!FadeGhosts.IsValidIndex(ComponentIndex) || !FadeGhostComponents.IsValidIndex(ComponentIndex)
		|| !DecorInstanceComponents.IsValidIndex(ComponentIndex))
	{
		return nullptr;
	}
	FFadeGhost& Ghost = FadeGhosts[ComponentIndex];
	if (!Ghost.bResolved)
	{
		Ghost.bResolved = true;
		UInstancedStaticMeshComponent* Source = DecorInstanceComponents[ComponentIndex].Get();
		FadeGhostComponents[ComponentIndex] = Source != nullptr ? CreateFadeGhost(*Source) : nullptr;
	}
	UInstancedStaticMeshComponent* GhostComponent = FadeGhostComponents[ComponentIndex].Get();
	// Uncooked runs: not while the dressing itself waits for its shaders, nor while the ghost's materials compile (the
	// dithered fade stays meanwhile instead of a ghost drawn with the engine default material).
	if (GhostComponent == nullptr || ShaderGatedComponents.Contains(ComponentIndex) || GateUntilShadersReady(*GhostComponent))
	{
		return nullptr;
	}
	return GhostComponent;
}

UInstancedStaticMeshComponent* AAbyssZoneActor::CreateFadeGhost(UInstancedStaticMeshComponent& Source)
{
	using namespace AbyssZonePrivate;
	UStaticMesh* Mesh = Source.GetStaticMesh();
	if (Mesh == nullptr)
	{
		return nullptr;
	}
	// Slot materials of the ghost: the toon body -> its translucent twin with the instance's parameters (palette
	// textures, look scalars); the ink hull -> hidden (a blended inverted hull would show its far side through the
	// body; BodyOpacity 0 makes M_AF_Outline's mask keep no pixel). Any other master: no ghost, the dither stays.
	const FName ToonName(TEXT("M_AF_Toon"));
	const FName ToonFoliageName(TEXT("M_AF_Toon_Foliage"));
	const FName OutlineName(TEXT("M_AF_Outline"));
	const FName OutlineFoliageName(TEXT("M_AF_Outline_Foliage"));
	TArray<UMaterialInterface*, TInlineAllocator<4>> Materials;
	const int32 NumMaterials = Source.GetNumMaterials();
	for (int32 Slot = 0; Slot < NumMaterials; ++Slot)
	{
		UMaterialInterface* Material = Source.GetMaterial(Slot);
		UMaterial* Base = Material != nullptr ? Material->GetBaseMaterial() : nullptr;
		const FName BaseName = Base != nullptr ? Base->GetFName() : NAME_None;
		UMaterialInterface* Twin = BaseName == ToonName ? FadeToonMaster.Get()
			: (BaseName == ToonFoliageName ? FadeFoliageMaster.Get() : nullptr);
		if (Twin != nullptr)
		{
			UMaterialInstanceDynamic* Body = UMaterialInstanceDynamic::Create(Twin, this);
			if (Body == nullptr)
			{
				return nullptr;
			}
			if (UMaterialInstance* Instance = Cast<UMaterialInstance>(Material))
			{
				Body->CopyParameterOverrides(Instance);
			}
			Materials.Add(Body);
		}
		else if (BaseName == OutlineName || BaseName == OutlineFoliageName)
		{
			UMaterialInstanceDynamic* Hull = UMaterialInstanceDynamic::Create(Material, this);
			if (Hull == nullptr)
			{
				return nullptr;
			}
			Hull->SetScalarParameterValue(FName(TEXT("BodyOpacity")), 0.f);
			Materials.Add(Hull);
		}
		else
		{
			return nullptr;
		}
	}
	if (Materials.Num() == 0)
	{
		return nullptr;
	}
	UInstancedStaticMeshComponent* Ghost = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
	Ghost->SetStaticMesh(Mesh);
	Ghost->SetMobility(EComponentMobility::Movable);
	Ghost->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ghost->SetGenerateOverlapEvents(false);
	Ghost->SetCanEverAffectNavigation(false);
	Ghost->SetNumCustomDataFloats(AbyssDecorIcd::Count);
	// The hidden source instance keeps casting the shadow (M_AF_Toon's shadow pass ignores the instance fade).
	Ghost->SetCastShadow(false);
	// Translucent sorting: the default priority, like the slime shell; water (-2) and blob shadows (-10) draw before,
	// VFX (1 / 2) after.
	Ghost->SetTranslucentSortPriority(0);
	for (int32 Slot = 0; Slot < Materials.Num(); ++Slot)
	{
		Ghost->SetMaterial(Slot, Materials[Slot]);
	}
	Ghost->SetupAttachment(SceneRoot);
	Ghost->SetVisibility(false);   // shown while it holds instances (UpdateFadeGhost)
	Ghost->RegisterComponent();
	UE_LOG(LogAbyss, Verbose, TEXT("Fade ghost created for %s"), *Mesh->GetName());
	return Ghost;
}

void AAbyssZoneActor::UpdateFadeGhost(int32 ComponentIndex)
{
	using namespace AbyssZonePrivate;
	UInstancedStaticMeshComponent* Ghost =
		FadeGhostComponents.IsValidIndex(ComponentIndex) ? FadeGhostComponents[ComponentIndex].Get() : nullptr;
	if (Ghost == nullptr || !FadeGhosts.IsValidIndex(ComponentIndex))
	{
		return;
	}
	FFadeGhost& State = FadeGhosts[ComponentIndex];
	const int32 Count = State.Occluders.Num();
	if (!State.bDirty && Count == 0)
	{
		return;
	}
	if (State.bDirty)
	{
		// The ghosted set changed (rare: a fade starts or ends): rebuild the few instances.
		State.bDirty = false;
		Ghost->ClearInstances();
		if (Count > 0)
		{
			TArray<FTransform> Transforms;
			Transforms.Reserve(Count);
			for (const int32 Index : State.Occluders)
			{
				Transforms.Add(Occluders[Index].Transform);
			}
			Ghost->AddInstances(Transforms, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ true);
		}
		Ghost->SetVisibility(Count > 0);
	}
	const TPair<int32, int32>* Focus = FocusedStory.IsNone() ? nullptr : StoryInstances.Find(FocusedStory);
	for (int32 Slot = 0; Slot < Count; ++Slot)
	{
		const FOccluder& Occluder = Occluders[State.Occluders[Slot]];
		const bool bFocused = Focus != nullptr && Focus->Key == ComponentIndex && Focus->Value == Occluder.Instance;
		float Data[AbyssDecorIcd::Count] = {};
		Data[AbyssDecorIcd::Fade] = Occluder.Fade;   // M_AF_Toon_Fade: alpha 1 - fade
		Data[AbyssDecorIcd::Random] = Occluder.RandomValue;
		Data[AbyssDecorIcd::Highlight] = bFocused ? 1.f : 0.f;
		Ghost->SetCustomData(Slot, TArrayView<const float>(Data, AbyssDecorIcd::Count), /*bMarkRenderStateDirty*/ false);
	}
	Ghost->MarkRenderInstancesDirty();
}

void AAbyssZoneActor::SetStoryFocus(FName DecorId)
{
	auto Apply = [this](FName Story, float Value)
	{
		const TPair<int32, int32>* Found = StoryInstances.Find(Story);
		if (Found == nullptr || !DecorInstanceComponents.IsValidIndex(Found->Key) || DecorInstanceComponents[Found->Key] == nullptr)
		{
			return;
		}
		DecorInstanceComponents[Found->Key]->SetCustomDataValue(Found->Value, AbyssDecorIcd::Highlight, Value, true);
	};
	if (FocusedStory == DecorId)
	{
		return;
	}
	const FName Previous = FocusedStory;
	if (!FocusedStory.IsNone())
	{
		Apply(FocusedStory, 0.f);
	}
	FocusedStory = DecorId;
	if (!DecorId.IsNone())
	{
		Apply(DecorId, 1.f);
	}
	// A story decoration drawn by its fade ghost shows the focus rim on the ghost (UpdateFadeGhost reads FocusedStory).
	for (const FName& Story : { Previous, DecorId })
	{
		if (const TPair<int32, int32>* Found = StoryInstances.Find(Story))
		{
			UpdateFadeGhost(Found->Key);
		}
	}
}
