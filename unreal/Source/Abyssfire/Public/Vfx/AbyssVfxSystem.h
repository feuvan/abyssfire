// UAbyssVfxSystem: the code-driven particle system of the game world (DECISIONS P6, R8; ue58-platform.md 6.9;
// art-inventory-ch1.md 8; combat-feel.md 11.4).
//
// * A port of the web FxEngine: particles live on the game thread (cap 900 desktop / 600 mobile, web MAX_PARTICLES) and
//   are drawn by pooled UInstancedStaticMeshComponents - one per (sprite texture, blend) or per FX mesh - whose
//   instances are rewritten every frame (BatchUpdateInstancesTransforms + per-instance custom data, AbyssVfxIcd).
// * What to play comes from FAbyssVfxLibrary recipes (AbyssVfxRecipe.h). The system subscribes to the core's
//   presentation events itself and maps them to recipe ids (WorldContract.md 5.3): hits, deaths, rewards, level-up,
//   dodge / dash / blink / town portal, loot drops, quest pops, skill VFX, projectiles (heads follow the core's flight
//   time, `.hit` on arrival), ground effects (loops for their duration, `.trigger`, `.end`), status loops (R8) from the
//   snapshot's status masks. The world builder plays world dressing (camps, exits, props, ambience) through Play().
// * Ticked by UAbyssWorldBuilder from IAbyssWorldView::SyncFrame (visual delta = real delta x the S6 dilation, so
//   particles slow down with the elite-kill slow motion; projectile positions follow the sim clock).
#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Subsystems/WorldSubsystem.h"

#include "abyss/sim/Events.h"
#include "abyss/sim/Snapshot.h"

#include "Framework/AbyssTypes.h"
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
	float StaggerMs = 0.f;
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
	/** Per frame, after the actors were placed. */
	void Tick(float VisualDeltaSec, const FAbyssFrameInfo& Frame, const abyss::Snapshot& Snap);
	/** Zone teardown / session end: every emitter, particle, projectile, ground effect and status loop goes. */
	void ClearAll();

	// ---- playing ----
	/** Plays a recipe; returns a handle (0 when the recipe does not exist or nothing can be drawn). */
	int32 Play(FName RecipeId, const FAbyssVfxContext& Context);
	/** The first recipe of the list that exists (specific -> generic). */
	int32 PlayFirst(TConstArrayView<FName> RecipeIds, const FAbyssVfxContext& Context);
	/** Stops the continuous emitters of a handle (bKillParticles also removes its live particles). */
	void Stop(int32 Handle, bool bKillParticles = false);
	bool IsPlaying(int32 Handle) const;
	/** Moves the origin of a handle's emitters (fixed anchors only; actor anchors follow the actor). */
	void SetHandleOrigin(int32 Handle, const FVector& Origin);
	/** Named anim notify of a character (FX_HammerStrike, FX_*, Foot*): plays `notify.<Name>` / `npc.<Name>` when it exists. */
	void PlayNotify(AActor* Actor, FName NotifyName);

	const FAbyssVfxLibrary& GetLibrary() const { return Library; }
	int32 GetLiveParticleCount() const { return Particles.Num(); }

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
	int32 MaxParticles = 900;
	int32 NextHandle = 1;
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

	// camera (refreshed every Tick)
	FVector CameraLocation = FVector::ZeroVector;
	FVector CameraForward = FVector::ForwardVector;
	FVector CameraRight = FVector::RightVector;
	FVector CameraUp = FVector::UpVector;
	FVector CameraFocus = FVector::ZeroVector;
};
