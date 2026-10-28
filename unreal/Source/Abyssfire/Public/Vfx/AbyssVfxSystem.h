// UAbyssVfxSystem: the code-driven particle system of the game world (DECISIONS P6, R8; ue58-platform.md 6.9;
// art-inventory-ch1.md 8; combat-feel.md 11.4).
//
// * A port of the web FxEngine: particles live on the game thread (cap 700 desktop / 450 mobile, DECISIONS R20) and
//   are drawn by pooled UInstancedStaticMeshComponents - one per (sprite texture, blend) or per FX mesh - whose
//   instances are rewritten every frame (BatchUpdateInstancesTransforms + per-instance custom data, AbyssVfxIcd).
// * What to play comes from FAbyssVfxLibrary recipes (AbyssVfxRecipe.h). The system subscribes to the core's
//   presentation events itself and maps them to recipe ids (WorldContract.md 5.3): hits, deaths, rewards, level-up,
//   dodge / dash / blink / town portal, loot drops, quest pops, skill VFX, projectiles (heads follow the core's flight
//   time, `.hit` on arrival), ground effects (loops for their duration, `.trigger`, `.end`), status loops (R8) from the
//   snapshot's status masks. The world builder plays world dressing (camps, exits, props, ambience) through Play().
// * Ticked by UAbyssWorldBuilder from IAbyssWorldView::SyncFrame (visual delta = real delta x the S6 dilation, so
//   particles slow down with the elite-kill slow motion; projectile positions follow the sim clock).
// * Fake lights (DECISIONS R18, P11 revised): recipe lights draw additive ground pools at intensity x the zone's lightScale
//   and light the toon characters through the MPC_AF_Lighting slots Light<i>PosR / Light<i>Col (nearest 8 desktop / 4
//   mobile to the camera focus, the hero halo excluded), refreshed every LightingUpdateIntervalMs.
// * Density (DECISIONS R20): hit.impact.* at most 2 live bursts per target (half count within 120 ms), smoke layers at
//   half count while more than 40 smoke / dust particles live; the recipe library grades the Smoke sprite layers.
// * Loop budget (dark remake 2G review X4, AbyssVfxBudgetRules.h): in a zone with a presentation theme (Chapter 1;
//   SetPresentationTheme from the zone build) continuous emitters stop at 75 % of the cap (bursts keep the rest), spawn
//   nothing beyond 30 m of the camera focus, and emit at most at the balanced frequency on mobile. Other zones: as before.
#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Subsystems/WorldSubsystem.h"

#include "abyss/sim/Events.h"
#include "abyss/sim/Snapshot.h"

#include "Framework/AbyssTypes.h"
#include "Vfx/AbyssVfxLighting.h"
#include "Vfx/AbyssVfxRecipe.h"
#include "World/AbyssWorldTypes.h"

#include "AbyssVfxSystem.generated.h"

class AActor;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UTexture;

/** Everything a recipe needs from the event that plays it. Positions are world (ground points unless noted). */
struct FAbyssVfxContext
{
	FVector Origin = FVector::ZeroVector;
	FVector Target = FVector::ZeroVector;
	FVector Point = FVector::ZeroVector;
	/** Anchor actors (sockets for chest / hand / overhead heights; `attach` layers follow them). */
	TWeakObjectPtr<AActor> OriginActor;
	TWeakObjectPtr<AActor> TargetActor;
	/** EvSkillVfx::points (chain lightning, multishot), ground points. */
	TArray<FVector> Points;
	/**
	 * The monster at each of Points (EvSkillVfx::pointTargets, DECISIONS C14), null for a ground point: a `points`
	 * anchor follows it, so a staggered chain link lands on the body wherever it stepped since the release.
	 */
	TArray<TWeakObjectPtr<AActor>> PointActors;
	float StaggerMs = 0.f;
	/** > 0: `fan` layers spread over this full angle (the core's cone, SkillPortDef::coneDeg) instead of their coneDeg. */
	float FanConeDeg = 0.f;
	/** Ground direction of the blow / facing (unit, XY). */
	FVector2D Blow = FVector2D(1.0, 0.0);
	/** AoE / ground-effect radius (cm) for `unit: radius` layers. */
	float RadiusCm = 100.f;
	/** Hit profile ring radius (cm) for `unit: ring` layers, spark count for `count: sparks`. */
	float RingRadiusCm = 29.f;
	int32 SparkCount = 6;
	/** Crit / kill (`only: big`). */
	bool bBig = false;
	bool bHasColor = false;
	FLinearColor Color = FLinearColor::White;
	bool bHasElement = false;
	abyss::DamageType Element = abyss::DamageType::Physical;
	/** Multiplies burst counts (gold burst: 6 coins on a kill, 8 on a pickup). */
	float CountScale = 1.f;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAbyssVfxShake, float /*DurationMs*/, float /*Intensity*/);

/** The particle budget's state and counters since the last zone build (`abyss.Vfx.Status`, the in-game autotest). */
struct FAbyssVfxBudgetStats
{
	int32 LiveParticles = 0;
	int32 MaxParticles = 0;
	/** Continuous emitters spawn only below this (MaxParticles outside a themed zone). */
	int32 LoopCeiling = 0;
	/** 0 = no culling. */
	float LoopCullRadiusCm = 0.f;
	float LoopFrequencyMultiplier = 1.f;
	bool bThemedZone = false;
	int32 PeakLiveParticles = 0;
	int32 ContinuousEmitters = 0;
	/** Continuous emitters out of the cull radius on the last frame. */
	int32 LoopsCulled = 0;
	/** Continuous spawns dropped at the loop ceiling, burst spawns dropped at the cap (since the zone build). */
	int32 LoopSpawnsDropped = 0;
	int32 BurstSpawnsDropped = 0;
};

UCLASS()
class ABYSSFIRE_API UAbyssVfxSystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UAbyssVfxSystem* Get(const UObject* WorldContextObject);

	// ---- USubsystem ----
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// ---- driven by the world builder ----
	/** Budgets of the render tier (particle cap, emission frequency multiplier, light pool count). */
	void ApplyQuality(const FAbyssQualityProfile& Profile);
	/**
	 * Dark remake 2G (review X4): whether the zone being built has a presentation theme; its loops then run on the themed
	 * budget (AbyssVfxBudgetRules::LoopBudgetFor). Resets the budget counters.
	 */
	void SetPresentationTheme(bool bThemed);
	/** Per frame, after the actors were placed. */
	void Tick(float VisualDeltaSec, const FAbyssFrameInfo& Frame, const abyss::Snapshot& Snap);
	/** Zone teardown / session end: every emitter, particle, projectile, ground effect and status loop goes. */
	void ClearAll();

	// ---- playing ----
	/** Plays a recipe; returns a handle (0 when the recipe does not exist or nothing can be drawn). */
	int32 Play(FName RecipeId, const FAbyssVfxContext& Context);
	/** The first recipe of the list that exists (specific -> generic). */
	int32 PlayFirst(TConstArrayView<FName> RecipeIds, const FAbyssVfxContext& Context);
	/**
	 * Dark remake 2G (U3): plays a recipe the runtime builds from an art contract (RecipeJson: one recipe object; the
	 * breach puddle from the manifest's puddleDecal, AbyssCorpseRules::PuddleRecipeJson), parsed once per Id and kept
	 * for the session apart from the library (so live emitters never point into a map that grows). 0 when it does not
	 * parse (logged once) or nothing can be drawn.
	 */
	int32 PlayRuntimeRecipe(FName Id, std::string_view RecipeJson, const FAbyssVfxContext& Context);
	/** Stops the continuous emitters of a handle (bKillParticles also removes its live particles). */
	void Stop(int32 Handle, bool bKillParticles = false);
	bool IsPlaying(int32 Handle) const;
	/** Moves the origin of a handle's emitters (fixed anchors only; actor anchors follow the actor). */
	void SetHandleOrigin(int32 Handle, const FVector& Origin);
	/**
	 * Dark remake 2G: the RadiusCm a handle's emitters scale their `unit: radius` layers by from now on (particles already
	 * spawned keep theirs): the closing arena ring's `hazard.banner_ring` follows the ring's radius.
	 */
	void SetHandleRadius(int32 Handle, float RadiusCm);
	/**
	 * Dark remake 2G (U3): the blow (unit, XY) a handle's emitters spawn along from now on (particles already spawned keep
	 * theirs): a monster loop played in its body's frame (the zealot's frenzy breath before the hood) turns with it.
	 */
	void SetHandleBlow(int32 Handle, const FVector2D& Blow);
	/** Named anim notify of a character (FX_HammerStrike, FX_*, Foot*): plays `notify.<Name>` / `npc.<Name>` when it exists. */
	void PlayNotify(AActor* Actor, FName NotifyName);

	const FAbyssVfxLibrary& GetLibrary() const { return Library; }
	int32 GetLiveParticleCount() const { return Particles.Num(); }
	FAbyssVfxBudgetStats GetBudgetStats() const;
	/** One log line of GetBudgetStats (`abyss.Vfx.Status`). */
	FString DescribeBudget() const;
	/**
	 * The zone's fake-light scale (DECISIONS R18, world-map-nav.md 14.1): max(0.12, ambientAlpha x (1 - luma(ambient))
	 * x 1.4) of the zone mood - plains / ember tower / mountain / desert 0.12, forest 0.1605, abyss 0.2522. Light pools and
	 * the hero halo draw at intensity x this; the toon light slots carry it too.
	 */
	float GetLightScale() const { return LightScale; }
	/**
	 * Dark remake (U1): the zone mood's light scale (zone_moods.json port.lightScale, blended over a mood region by the
	 * zone actor), pushed by the world builder every frame. <= 0: no override, the R18 formula of the zone mood.
	 */
	void SetMoodLightScale(float Scale);

	/** Recipe-authored camera shakes (the web FxEngine.shake, own 120 ms throttle). Bound by the world builder. */
	FOnAbyssVfxShake OnShake;

	/** World-space anchor of an actor at a height class (character sockets, prop anchors, else actor + defaults). */
	static FVector ResolveActorAnchor(const AActor* Actor, EAbyssVfxHeight Height, float CustomCm);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	// ---- internal types ----
	struct FAnchor
	{
		TWeakObjectPtr<AActor> Actor;
		EAbyssVfxHeight Height = EAbyssVfxHeight::Ground;
		float HeightCm = 0.f;
		FVector Fixed = FVector::ZeroVector;   // used when no actor (or the actor is gone: last position)
		uint32 ProjectileId = 0;               // follows a projectile head
		bool bInUse = false;
		bool bMarked = false;
		FVector Current = FVector::ZeroVector;
	};

	struct FEmitter
	{
		int32 Handle = 0;
		const FAbyssVfxRecipe* Recipe = nullptr;
		int32 LayerIndex = 0;
		FAbyssVfxContext Context;
		FAbyssVfxPalette Palette;
		int32 SpawnAnchor = INDEX_NONE;
		int32 BeamEndAnchor = INDEX_NONE;
		int32 PointIndex = 0;
		float ElapsedMs = 0.f;
		float StartDelayMs = 0.f;
		float SpawnAccumulator = 0.f;
		uint32 SpawnCounter = 0;
		/** Particles of the burst being spawned (`fan` layers: particle i of N), 0 for continuous emission. */
		int32 FanCount = 0;
		bool bBurstDone = false;
		bool bStopped = false;
		bool bContinuous = false;
	};

	struct FParticle
	{
		int32 Pool = INDEX_NONE;
		int32 Handle = 0;
		int32 Anchor = INDEX_NONE;     // attached particles: Position is relative to the anchor
		int32 BeamEnd = INDEX_NONE;    // beams: the other end anchor
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float AgeMs = 0.f;
		float LifeMs = 1.f;
		float Size0 = 1.f;
		float Size1 = 1.f;
		float SizePow = 1.f;
		float Aspect = 1.f;
		float Stretch = 0.f;
		float Alpha0 = 1.f;
		float Alpha1 = 0.f;
		float FadeIn = 0.f;
		float AlphaPow = 1.f;
		float Flicker = 0.f;
		float Gravity = 0.f;
		float Drag = 0.f;
		float RotationDeg = 0.f;
		float SpinDegS = 0.f;
		float Variant = 0.f;
		FLinearColor Color = FLinearColor::White;
		EAbyssVfxOrient Orient = EAbyssVfxOrient::Billboard;
		bool bOrbit = false;
		float OrbitAngleDeg = 0.f;
		float OrbitDegS = 0.f;
		float OrbitR0 = 0.f;
		float OrbitR1 = 0.f;
		bool bMesh = false;
		float MeshDiameterCm = 100.f;
	};

	struct FPool
	{
		FName Key;
		bool bMesh = false;
		EAbyssVfxBlend Blend = EAbyssVfxBlend::Additive;
		int32 Component = INDEX_NONE;   // index into PoolComponents
		int32 Capacity = 0;
		int32 UsedLastFrame = 0;
		float MeshDiameterCm = 100.f;
		/** Smoke / Dust sprite pool (the live smoke count of the density cap). */
		bool bSmoke = false;
		TArray<FTransform> Transforms;
		TArray<float> CustomData;
	};

	struct FProjectile
	{
		uint32 Id = 0;
		abyss::ProjectileKind Kind = abyss::ProjectileKind::HeroSkill;
		FName VfxId;
		FVector From = FVector::ZeroVector;
		FVector To = FVector::ZeroVector;
		TWeakObjectPtr<AActor> TargetActor;
		double LaunchMs = 0.0;
		double TravelMs = 1.0;
		float ArcCm = 0.f;
		int32 Handle = 0;
		int32 Anchor = INDEX_NONE;
		FLinearColor Color = FLinearColor::White;
		bool bHasColor = false;
		FVector Position = FVector::ZeroVector;
		/**
		 * A presentation-only flight (multishot's per-target arrows, DECISIONS C14 (4)): no core projectile ends it, so it
		 * ends itself on arrival. Its id has the top bit set (core entity ids never reach it).
		 */
		bool bVisualOnly = false;
	};

	struct FGroundEffect
	{
		uint32 Id = 0;
		FName VfxId;
		FVector Center = FVector::ZeroVector;
		float RadiusCm = 100.f;
		int32 Handle = 0;
		abyss::DamageType Element = abyss::DamageType::Physical;
		bool bHasElement = false;
	};

	struct FLight
	{
		int32 Handle = 0;
		int32 Anchor = INDEX_NONE;
		float RadiusCm = 0.f;
		float Alpha = 0.f;
		bool bFlicker = false;
		/** Light-only recipes live until stopped; others die with their last emitter / particle. */
		bool bPersistent = false;
		float Seed = 0.f;
		FLinearColor Color = FLinearColor::White;
		/** The flame above the anchor (FAbyssVfxRecipe::LightHeightCm): where the toon light slot sits. */
		float HeightCm = 0.f;
	};

	/** The last hit.impact bursts on one target (DECISIONS R20 density cap). */
	struct FHitBurstTrack
	{
		int32 Handles[2] = { 0, 0 };
		double LastRealSec = -1000.0;
	};

	// ---- events ----
	void BindEvents();
	void UnbindEvents();
	void HandleHit(const abyss::EvHit& Event);
	void HandleDespawned(const abyss::EvEntityDespawned& Event);
	void HandleHeroDied(const abyss::EvHeroDied& Event);
	void HandleLevelUp(const abyss::EvLevelUp& Event);
	void HandleItemPicked(const abyss::EvItemPicked& Event);
	void HandlePotionPicked(const abyss::EvPotionPicked& Event);
	void HandleDodge(const abyss::EvDodgeStarted& Event);
	void HandleDash(const abyss::EvHeroDash& Event);
	void HandleTeleported(const abyss::EvEntityTeleported& Event);
	void HandleTownPortal(const abyss::EvTownPortal& Event);
	void HandleLootDropped(const abyss::EvLootDropped& Event);
	void HandleQuestUpdate(const abyss::EvQuestUpdate& Event);
	void HandleSkillVfx(const abyss::EvSkillVfx& Event);
	void HandleProjectileLaunched(const abyss::EvProjectileLaunched& Event);
	void HandleProjectileEnded(const abyss::EvProjectileEnded& Event);
	/**
	 * Multishot (DECISIONS C14 (4)): one presentation-only arrow per monster the core hit, from the caster's hand, homing
	 * on it and landing on the core's per-target arrow delay (SkillArrowDelayMs), when that hit lands.
	 */
	void LaunchPerTargetArrows(const abyss::EvSkillVfx& Event, const abyss::SkillDef& Skill);
	void HandleGroundStarted(const abyss::EvGroundEffectStarted& Event);
	void HandleGroundTriggered(const abyss::EvGroundEffectTriggered& Event);
	void HandleGroundEnded(const abyss::EvGroundEffectEnded& Event);
	void HandleHeroRespawned(const abyss::EvHeroRespawned& Event);
	void HandleResonance(const abyss::EvResonance& Event);

	// ---- helpers ----
	bool EnsureRenderer();
	AActor* FindActor(abyss::EntityId Id) const;
	FVector TileToGround(const abyss::Vec2& Tile) const;
	double GroundZ(double X, double Y) const;
	/** Ground point of an entity (actor, else snapshot position), true when found. */
	bool EntityGround(abyss::EntityId Id, FVector& Out) const;
	FAbyssVfxPalette ResolvePalette(const FAbyssVfxRecipe& Recipe, const FAbyssVfxContext& Context) const;
	/** Play() of a resolved recipe (library or runtime; it must outlive its emitters). */
	int32 PlayRecipe(const FAbyssVfxRecipe& Recipe, const FAbyssVfxContext& Context);
	int32 AddAnchor(AActor* Actor, EAbyssVfxHeight Height, float HeightCm, const FVector& Fixed, uint32 ProjectileId = 0);
	FVector AnchorPosition(int32 AnchorIndex) const;
	void UpdateAnchors();
	void SweepAnchors();
	int32 ResolveSpawnAnchor(const FEmitter& Emitter, EAbyssVfxAnchor At, EAbyssVfxHeight Height, float HeightCm,
		int32 PointIndex);
	void SpawnFromEmitter(FEmitter& Emitter, int32 Count);
	void SpawnParticle(FEmitter& Emitter, const FAbyssVfxLayer& Layer, float UnitScale);
	int32 FindOrCreatePool(const FAbyssVfxLayer& Layer);
	void UpdateEmitters(float DeltaMs);
	void UpdateParticles(float DeltaSec);
	void UpdateProjectiles(const FAbyssFrameInfo& Frame);
	void UpdateStatusLoops(const abyss::Snapshot& Snap);
	void UpdateLights(float DeltaSec);
	/** DECISIONS R18: recomputes LightScale from the zone mood when the zone changes. */
	void UpdateLightScale(const abyss::Snapshot& Snap);
	/**
	 * DECISIONS P11 revised (contract C2): writes the toon light slots of MPC_AF_Lighting every LightingUpdateIntervalMs
	 * (real time) - the nearest 8 (desktop) / 4 (mobile) lights to the camera focus, hero halo excluded:
	 * Light<i>PosR = (flame position cm, radius cm), Light<i>Col = (linear colour x alpha (flicker) x LightScale x
	 * abyss.CharLightGain, 1); the last used slot cross-fades over 150 cm, unused slots are 0. Missing parameters (an
	 * older content build) are skipped.
	 */
	void UpdateLightSlots(float RealDeltaSec);
	/** A light's alpha now (the web flicker included), before the light scale. */
	float LightAlphaNow(const FLight& Light) const;
	/** True while a handle still has an emitter or a live particle. */
	bool IsHandleLive(int32 Handle) const;
	/**
	 * Hero attachments from the art manifest fx block (art-inventory-ch1.md 3.1, Art/blender/README.md hero section):
	 * halo light pool, weapon trail history; plus the spirit-resonance aura kept alive while Snapshot.hero.resonating.
	 */
	void UpdateHeroFx(float DeltaSec, const abyss::Snapshot& Snap);
	/** Visor glow card, weapon trail quads, cast blade glow and embers (called from Render). */
	void RenderHeroFx();
	void EmitInstance(int32 PoolIndex, const FTransform& Transform, const FLinearColor& Color, float Alpha, float Variant, float Age);
	/** Spirit-profile colour of the hero's class (classes 14 / combat-feel 16), the manifest spiritColor when present. */
	FLinearColor HeroSpiritColor(const abyss::Snapshot* Snap) const;
	void StartResonanceAura(AActor* Hero, const FLinearColor& Color);
	void Render();
	void RequestShake(float DurationMs, float Intensity);
	FName StatusRecipeId(abyss::StatusType Type) const;
	void StartLight(int32 Handle, const FAbyssVfxRecipe& Recipe, int32 Anchor, const FAbyssVfxPalette& Palette,
		const FAbyssVfxContext& Context);

	FAbyssVfxLibrary Library;
	/** PlayRuntimeRecipe's parsed recipes by id (heap-owned: stable addresses for the emitters); nullptr = did not parse. */
	TMap<FName, TUniquePtr<FAbyssVfxRecipe>> RuntimeRecipes;
	FRandomStream Random;

	UPROPERTY(Transient)
	TObjectPtr<AActor> RenderActor;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> PoolComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PoolMaterials;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> QuadMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> AdditiveMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> TranslucentMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> MeshMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UTexture> FallbackSprite;

	TArray<FPool> Pools;
	TMap<FName, int32> PoolIndexByKey;
	TArray<FAnchor> Anchors;
	TArray<int32> FreeAnchors;
	TArray<FEmitter> Emitters;
	TArray<FParticle> Particles;
	TArray<FLight> Lights;
	TMap<uint32, FProjectile> Projectiles;
	TMap<uint32, FGroundEffect> GroundEffects;
	/** (entity << 8 | status) -> handle of the running status loop. */
	TMap<uint64, int32> StatusLoops;
	int32 PortalHandle = 0;

	FAbyssQualityProfile Quality;
	int32 MaxParticles = 700;
	/** The loop budget (RefreshLoopBudget from the quality and the zone's theme; AbyssVfxBudgetRules). */
	void RefreshLoopBudget();
	bool bThemedZone = false;
	int32 LoopCeiling = 700;
	float LoopCullRadiusCm = 0.f;
	float LoopFrequencyMultiplier = 1.5f;
	int32 PeakLiveParticles = 0;
	int32 LoopsCulledLastFrame = 0;
	int32 LoopSpawnsDropped = 0;
	int32 BurstSpawnsDropped = 0;
	int32 NextHandle = 1;
	uint32 NextVisualProjectile = 1;
	double LastShakeRealSec = -1000.0;
	double RealClockSec = 0.0;
	double RenderSimMs = 0.0;
	uint64 FrameCounter = 0;
	bool bRendererResolved = false;
	bool bRendererAvailable = false;
	bool bEventsBound = false;

	// hero attachments (UpdateHeroFx / RenderHeroFx)
	struct FTrailSample
	{
		FVector Tip = FVector::ZeroVector;
		FVector Mid = FVector::ZeroVector;
		double TimeSec = 0.0;
	};
	TArray<FTrailSample> TrailSamples;
	bool bTrailIsCast = false;
	double HeroFxClockSec = 0.0;
	int32 HeroHaloHandle = 0;
	TWeakObjectPtr<AActor> HeroHaloActor;
	int32 ResonanceAuraHandle = 0;
	TWeakObjectPtr<AActor> ResonanceAuraActor;

	// fake lights (DECISIONS R18 / P11 revised)
	float LightScale = 0.12f;
	/** The zone mood's own scale (port.lightScale, else R18's formula) and the world builder's live override (<= 0: none). */
	float ZoneLightScale = 0.12f;
	float MoodLightScale = -1.f;
	std::string LightScaleMapId;
	bool bLightScaleKnown = false;
	float LightSlotRefreshSec = 0.f;
	/** Every slot of MPC_AF_Lighting holds 0 (nothing to write until a light appears). */
	bool bLightSlotsClear = false;
	TArray<AbyssVfxLighting::FSlotPick> SlotPicks;

	// density (DECISIONS R20)
	TMap<uint32, FHitBurstTrack> HitBursts;
	int32 LiveSmokeParticles = 0;

	// camera (refreshed every Tick)
	FVector CameraLocation = FVector::ZeroVector;
	FVector CameraForward = FVector::ForwardVector;
	FVector CameraRight = FVector::RightVector;
	FVector CameraUp = FVector::UpVector;
	FVector CameraFocus = FVector::ZeroVector;
};
