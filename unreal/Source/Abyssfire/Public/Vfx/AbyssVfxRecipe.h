// VFX recipes: the data the code-driven particle system plays (DECISIONS P6, ue58-platform.md 6.9, art-inventory-ch1.md 8).
//
// A recipe is a list of LAYERS; every layer is one emitter of camera-facing (or ground-flat / velocity-stretched /
// upright) quads or small meshes, drawn from pooled instanced-static-mesh components (UAbyssVfxSystem). The layer model
// is the web FxEngine particle model (FxEngine.ts:13-60): delay, count or rate, life, spawn disc, speed + direction
// cone, gravity, drag, size over life (ease-out power), alpha over life (fade-in then ease-in power), flicker, spin,
// orbit, colour from a palette slot / the event colour / a fixed colour, alternating colours. The web's primitive
// helpers (glow, flash, sparks, motes, flames, smoke, debris, ring, shock, decal, slash, beam, gather, bolt, ...) are
// PRESETS: `"emitter": "sparks"` fills the defaults of that helper and every other field overrides them.
//
// Sources, later wins per recipe id:
//   1. built-in defaults (Private/Vfx/AbyssVfxDefaults.cpp: combat feedback, rewards, world, Chapter-1 skills),
//   2. the first file that exists of: Data/vfx_recipes.json (staged; the exporter copies the art export there), then
//      Art/Export/VFX/vfx_recipes.json (editor / uncooked runs).
// The JSON schema is documented in Public/World/WorldContract.md section 5 (and by the built-in defaults themselves).
//
// Units: centimetres, cm/s, cm/s^2, milliseconds, degrees. Web px convert with px x 100 / 45 (combat-feel.md 0).
#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

#include <string_view>

#include "abyss/base/Enums.h"

namespace abyss
{
	class JsonValue;
}

/** How a particle quad is oriented. */
enum class EAbyssVfxOrient : uint8
{
	Billboard,   // faces the camera
	Ground,      // flat on the ground (rings, decals, shocks, pools)
	Velocity,    // faces the camera, long axis along the screen-projected velocity (streaks, sparks, comets)
	Upright,     // vertical, turned to the camera yaw (flames, beams, pillars)
	Beam,        // stretched between the spawn point and the layer's `to` anchor (bolts, chains)
};

enum class EAbyssVfxBlend : uint8
{
	Additive,
	Translucent,   // alpha blended (smoke, debris bodies, ink strokes)
};

/** Spawn anchor of a layer (resolved from the event context). */
enum class EAbyssVfxAnchor : uint8
{
	Origin,   // the caster / source / effect centre
	Target,   // the target entity / end point
	Point,    // the event's aim point (skill point, projectile end)
	Points,   // every point of EvSkillVfx::points (staggered)
	Path,     // spread along the origin -> target segment
	Camera,   // around the camera focus (ambient weather volume)
};

/** Height of the anchor point above the ground (sockets of the anchor actor when present). */
enum class EAbyssVfxHeight : uint8
{
	Ground,
	Chest,
	Head,
	Overhead,
	Hand,
	Custom,   // HeightCm above the ground
};

/** Initial velocity direction. */
enum class EAbyssVfxDir : uint8
{
	Random,    // uniform on the sphere
	Up,        // +Z (cone around it)
	Down,
	Blow,      // along the context's blow direction (attacker -> target), cone around it, horizontal
	Back,      // against the blow
	Out,       // radially away from the anchor on the ground plane
	In,        // radially towards the anchor
	Flat,      // random horizontal direction
	Tangent,   // around the anchor (whirl)
};

/** Where a colour comes from. */
enum class EAbyssVfxColor : uint8
{
	Fixed,
	Event,   // the event colour (impact colour, projectile colour, quality colour), else the palette mid
	Core,    // palette slots (FxKit PAL)
	Mid,
	Rim,
	Dark,
	White,
};

/** What a layer's sizes / spawn radius are measured in. */
enum class EAbyssVfxUnit : uint8
{
	Cm,
	Ring,     // multiples of the hit profile ring radius (HitFeedback ringRadius px -> cm)
	Radius,   // multiples of the event radius (AoE / ground effect radius)
};

/** [Min, Max] uniformly sampled. */
struct FAbyssVfxRange
{
	float Min = 0.f;
	float Max = 0.f;

	FAbyssVfxRange() = default;
	FAbyssVfxRange(float InValue) : Min(InValue), Max(InValue) {}
	FAbyssVfxRange(float InMin, float InMax) : Min(InMin), Max(InMax) {}
	float Sample(FRandomStream& Random) const { return Min == Max ? Min : Random.FRandRange(Min, Max); }
};

struct FAbyssVfxLayer
{
	// ---- what is drawn ----
	/** Sprite name: texture T_FX_<Sprite> (Glow, Core, Spark, Streak, Ember, Smoke, Ring, Shock, Slash, ...). */
	FName Sprite = FName(TEXT("Glow"));
	/** Optional mesh (SM_FX_Arrow, SM_FX_HexPlate, SM_FX_Rock_A, SM_Pickup_GoldCoin, ...) instead of a sprite. */
	FName Mesh;
	EAbyssVfxBlend Blend = EAbyssVfxBlend::Additive;
	EAbyssVfxOrient Orient = EAbyssVfxOrient::Billboard;
	/** Sprite variant written to the instance data (bolt 0..3, smoke 0..1); RandomVariants > 1 picks one per particle. */
	int32 Variant = 0;
	int32 RandomVariants = 0;

	// ---- where ----
	EAbyssVfxAnchor At = EAbyssVfxAnchor::Origin;
	EAbyssVfxHeight Height = EAbyssVfxHeight::Ground;
	float HeightCm = 0.f;
	/** Offset in the anchor frame: X along the blow / facing, Y to its right, Z up. */
	FVector OffsetCm = FVector::ZeroVector;
	/** Beam orientation: the other end. */
	EAbyssVfxAnchor BeamTo = EAbyssVfxAnchor::Target;
	EAbyssVfxHeight BeamToHeight = EAbyssVfxHeight::Chest;
	/** Spawn disc on the ground plane (on the ring only with bSpawnOnRing) and vertical jitter. */
	float SpawnRadiusCm = 0.f;
	bool bSpawnOnRing = false;
	float SpawnHeightJitterCm = 0.f;
	EAbyssVfxUnit Unit = EAbyssVfxUnit::Cm;
	/** Particles move with their anchor (attached to an actor / projectile / ground effect). */
	bool bAttach = false;

	// ---- when / how many ----
	float DelayMs = 0.f;
	/** Burst: particles spawned once (after DelayMs). */
	FAbyssVfxRange Count = FAbyssVfxRange(1.f);
	/** Burst count = the context's spark count (hit profile `sparks`) instead of Count. */
	bool bCountFromContext = false;
	/** Burst count scales with the event radius (count x radius / 100 cm, at least 1). */
	bool bCountByRadius = false;
	/** > 0: continuous emission (particles per second, divided by the quality frequency multiplier) for DurationMs. */
	float Rate = 0.f;
	/** Continuous emitters: emission time; < 0 = until the owner stops it (status loops, projectile heads, camps). */
	float DurationMs = -1.f;
	/** Points anchor: per-point delay (chain lightning 55 ms). < 0 = the event's stagger. */
	float StaggerMs = -1.f;
	FAbyssVfxRange LifeMs = FAbyssVfxRange(300.f);

	// ---- motion ----
	FAbyssVfxRange SpeedCmS = FAbyssVfxRange(0.f);
	EAbyssVfxDir Dir = EAbyssVfxDir::Random;
	/** Full cone angle around Dir (degrees; 0 = exact, 360 = any). */
	float ConeDeg = 360.f;
	/** Extra vertical velocity (cm/s, + up). */
	FAbyssVfxRange UpCmS = FAbyssVfxRange(0.f);
	/** Downward acceleration (cm/s^2); negative rises. */
	float GravityCmS2 = 0.f;
	/** v *= exp(-Drag * dt). */
	float Drag = 0.f;
	/** Orbit around the anchor (gather, hex plates): radius R0 -> R1 over life at DegPerSec. */
	bool bOrbit = false;
	FAbyssVfxRange OrbitRadius0Cm = FAbyssVfxRange(0.f);
	FAbyssVfxRange OrbitRadius1Cm = FAbyssVfxRange(0.f);
	FAbyssVfxRange OrbitDegS = FAbyssVfxRange(0.f);

	// ---- size ----
	/**
	 * Start size (cm, or in Unit): quads - the height along the texture's up axis (width = height x Aspect; velocity
	 * quads: the length before stretch; beams: the width); meshes - the bounding-sphere diameter drawn.
	 */
	FAbyssVfxRange SizeCm = FAbyssVfxRange(20.f);
	/** End size = start x Grow; s(t) = s0 + (s1 - s0)(1 - (1 - t)^SizePow). */
	float Grow = 1.f;
	float SizePow = 1.f;
	/** Width / height of the quad (streaks are long and thin: 0.2; flames 0.62; slashes wide: 2). */
	float Aspect = 1.f;
	/** Velocity orientation: extra length per cm/s of speed. */
	float StretchPerSpeed = 0.f;

	// ---- alpha / colour ----
	float Alpha0 = 1.f;
	float Alpha1 = 0.f;
	/** Fraction of the life fading in from 0 to Alpha0. */
	float FadeIn = 0.f;
	/** Alpha0 -> Alpha1 with u^AlphaPow (ease-in) after the fade-in. */
	float AlphaPow = 1.f;
	/** Alpha *= 1 - Flicker * rand each frame. */
	float Flicker = 0.f;
	EAbyssVfxColor ColorSource = EAbyssVfxColor::Mid;
	FLinearColor Color = FLinearColor::White;
	/** Every AltEvery-th particle uses the alternate colour (sparks: every 3rd white). 0 = never. */
	int32 AltEvery = 0;
	EAbyssVfxColor AltColorSource = EAbyssVfxColor::White;
	FLinearColor AltColor = FLinearColor::White;
	/** HDR boost of the colour (bloom picks up > 1). */
	float Intensity = 1.f;

	// ---- rotation ----
	FAbyssVfxRange RotationDeg = FAbyssVfxRange(0.f);
	FAbyssVfxRange SpinDegS = FAbyssVfxRange(0.f);
	/** Ground / billboard rotation follows the blow direction (slashes, claws). */
	bool bAlignToBlow = false;

	// ---- filters ----
	/** 1 = only for big hits (crit / kill), -1 = only for non-big, 0 = always. */
	int8 BigFilter = 0;
};

struct FAbyssVfxRecipe
{
	FName Id;
	/** Default palette (fire, frost, lightning, poison, shadow, holy, steel, blood, rage, arcane, nature, earth), or
	 *  `element` (from the event's damage type) / `event` (built from the event colour). */
	FName Palette = FName(TEXT("event"));
	TArray<FAbyssVfxLayer> Layers;
	/** Projectile heads: parabolic arc height of the flight (cm; arrows ~0, lobbed bombs > 0). */
	float ArcCm = 0.f;
	/** Fake light pool under the effect while it lives (cm radius, 0 = none). */
	float LightRadiusCm = 0.f;
	float LightAlpha = 0.f;
	bool bLightFlicker = false;
	/** Camera shake started with the recipe (skill-authored shakes, web FxEngine.shake). */
	float ShakeMs = 0.f;
	float ShakeIntensity = 0.f;
};

/** Four-tone element palette (FxKit PAL, art-inventory-ch1.md 8.2), linear colours. */
struct FAbyssVfxPalette
{
	FLinearColor Core = FLinearColor::White;
	FLinearColor Mid = FLinearColor::White;
	FLinearColor Rim = FLinearColor::White;
	FLinearColor Dark = FLinearColor::Black;

	/** A palette derived from one colour (event colours without a named palette). */
	static FAbyssVfxPalette FromColor(const FLinearColor& Color);
	FLinearColor Get(EAbyssVfxColor Slot, const FLinearColor& EventColor, bool bHasEventColor) const;
};

class ABYSSFIRE_API FAbyssVfxLibrary
{
public:
	/** Built-in defaults, then Data/vfx_recipes.json, then (editor) Art/Export/VFX/vfx_recipes.json. */
	void LoadAll();
	void Reset();

	/** Parses one recipe document ({schemaVersion, palettes, recipes}); recipes replace same-id entries. */
	bool MergeJson(std::string_view Json, const FString& SourceName, FString& OutError);

	const FAbyssVfxRecipe* Find(FName Id) const;
	/** First existing id of the list (specific -> generic fallbacks). */
	const FAbyssVfxRecipe* FindFirst(TConstArrayView<FName> Ids) const;
	const FAbyssVfxPalette* FindPalette(FName Name) const;
	/** Palette for a damage type (fire, frost, lightning, poison, arcane, steel). */
	const FAbyssVfxPalette& ElementPalette(abyss::DamageType Element) const;
	/** Cells of a sprite texture laid out as a horizontal strip (variants: smoke 2, bolt 4, rock 2), at least 1. */
	int32 GetSpriteCells(FName Sprite) const;
	int32 Num() const { return Recipes.Num(); }

	/** Layer defaults of a web FxKit primitive (glow, flash, glint, sparks, motes, flames, smoke, debris, ring, shock,
	 *  decal, slash, swipe, beam, gather, bolt, streak, mesh). False for an unknown name. */
	static bool ApplyPreset(FName Emitter, FAbyssVfxLayer& Layer);

private:
	bool ParseRecipe(FName Id, const abyss::JsonValue& Json, FAbyssVfxRecipe& Out, FString& OutError) const;
	bool ParseLayer(const abyss::JsonValue& Json, FAbyssVfxLayer& Out, FString& OutError) const;
	void AddBuiltInPalettes();

	TMap<FName, FAbyssVfxRecipe> Recipes;
	TMap<FName, FAbyssVfxPalette> Palettes;
	TMap<FName, int32> SpriteCells;
	FAbyssVfxPalette FallbackPalette;
};

/** Built-in recipe documents (JSON text, one per group), Private/Vfx/AbyssVfxDefaults.cpp. */
namespace AbyssVfxDefaults
{
	TConstArrayView<const char*> Documents();
}
