// Small shared vocabulary of the world presentation layer (World/, Actors/, Anim/, Vfx/, Camera/): material data layouts
// the content build must honour, anchors, colour helpers and the web's deterministic tile hash (render-only variant
// picking). No UObjects here. The contract with the content agent is documented in Public/World/WorldContract.md.
#pragma once

#include "CoreMinimal.h"

#include "abyss/base/Types.h"

/**
 * Custom primitive data (UPrimitiveComponent::SetCustomPrimitiveDataFloat) written on every presented mesh component:
 * character bodies, attached weapons, props. M_AF_Toon / M_AF_Outline read them through parameters flagged
 * "Use Custom Primitive Data" with these indices (art-inventory-ch1.md 1.8). All default to 0 = no effect.
 */
namespace AbyssCpd
{
	inline constexpr int32 HitFlash = 0;         // 0..1: final colour -> white (combat-feel.md 11.3)
	inline constexpr int32 PainTintR = 1;        // 1..3: multiply tint colour (linear)
	inline constexpr int32 PainTintAmount = 4;   // 0..1
	inline constexpr int32 TelegraphR = 5;       // 5..7: monster wind-up tint (linear)
	inline constexpr int32 TelegraphAmount = 8;  // 0..1
	inline constexpr int32 TintR = 9;            // 9..11: status tint (characters) / quality tint (loot) (linear)
	inline constexpr int32 TintAmount = 12;      // 0..1
	inline constexpr int32 Fade = 13;            // 0 = opaque .. 1 = invisible (DitherTemporalAA opacity mask)
	inline constexpr int32 Ghost = 14;           // 0..1: ghost look (soul echo); colour = Tint
	inline constexpr int32 Highlight = 15;       // 0..1: interactable hover / story-decoration focus rim
	inline constexpr int32 Count = 16;
}

/** Per-instance custom data of the world ISMs (decor, outcrops, palisades): read with PerInstanceCustomData[n]. */
namespace AbyssDecorIcd
{
	inline constexpr int32 Fade = 0;       // occlusion fade, 0 = opaque (world-map-nav.md 15.6)
	inline constexpr int32 Random = 1;     // stable 0..1 per instance (wind phase, colour jitter)
	inline constexpr int32 Highlight = 2;  // 0..1 story-decoration focus rim (EvStoryDecorFocus)
	inline constexpr int32 Count = 3;
}

/** Per-instance custom data of the VFX particle ISMs (Vfx/): colour, alpha, sprite variant, normalised age. */
namespace AbyssVfxIcd
{
	inline constexpr int32 R = 0;
	inline constexpr int32 G = 1;
	inline constexpr int32 B = 2;
	inline constexpr int32 A = 3;
	inline constexpr int32 Variant = 4;  // texture variant (bolt 0..3, smoke 0..1, rock 0..1, ...)
	inline constexpr int32 Age = 5;      // 0..1 over the particle life
	inline constexpr int32 Count = 6;
}

/** Attachment points on presented actors (sockets from the art manifest, art-inventory-ch1.md 2.4). */
enum class EAbyssAnchor : uint8
{
	Feet,      // fx_feet / actor ground point
	Chest,     // fx_chest (impact bursts, web "CH = 18 px")
	Head,      // fx_head
	Overhead,  // fx_overhead (elite crown, nameplates, death mark)
	HandR,     // fx_hand_r (launch points)
	HandL,     // fx_hand_l
};

ABYSSFIRE_API FName AbyssAnchorSocketName(EAbyssAnchor Anchor);

/** The four-tone camera / world framing of the current zone mood (world-map-nav.md 14.1, zone_moods.json). */
struct FAbyssMoodLook
{
	FLinearColor Ambient = FLinearColor::White;   // multiply colour (linear)
	float AmbientAlpha = 0.f;
	FLinearColor Vignette = FLinearColor::Black;
	float VignetteAlpha = 0.f;
	FLinearColor Haze = FLinearColor::Black;      // additive haze colour
	float HazeAlpha = 0.f;
	float Saturation = 1.f;
	float Contrast = 1.f;
	FVector Lift = FVector::ZeroVector;
	FVector Gain = FVector::ZeroVector;
};

/** Resolved render tier settings (render_quality.json, ue58-platform.md 6.8). */
struct FAbyssQualityProfile
{
	int32 Tier = 1;                          // 0 low, 1 balanced, 2 high
	int32 MaxDynamicLights = 16;             // fake light pool cap
	float LightingUpdateIntervalMs = 50.f;   // MPC / flicker refresh
	float ParticleFrequencyMultiplier = 1.5f;// emission interval multiplier (ambient / looping emitters)
	float ResolutionScale = 1.5f;            // 3D resolution cap: 720 * scale px of height
	bool bBloom = true;
	bool bColorGrading = true;
	bool bMobile = false;
};

namespace AbyssWorldUtil
{
	/** 0xRRGGBB (sRGB, data tables / events) -> linear colour. */
	ABYSSFIRE_API FLinearColor ColorFromRgb(uint32 Rgb, float Alpha = 1.f);
	/** "#RRGGBB" / "RRGGBB" / "#RGB" -> linear colour; false when malformed. */
	ABYSSFIRE_API bool ParseHexColor(FStringView Text, FLinearColor& OutColor);

	/** Web tileHash (src/graphics/terrain/TerrainLattice.ts:67-71): stable 32-bit hash of a tile with a salt. */
	ABYSSFIRE_API uint32 TileHash(int32 Col, int32 Row, int32 Salt);

	/**
	 * Web placeDecorSprite jitter (world-map-nav.md 15.5) for decoration i (seed = i + 1): tile-space offset, uniform
	 * scale and a yaw (new in 3D: h * 360 degrees).
	 */
	struct FDecorJitter
	{
		FVector2D OffsetTiles = FVector2D::ZeroVector;
		float Scale = 1.f;
		float YawDegrees = 0.f;
		float Random = 0.f;
	};
	ABYSSFIRE_API FDecorJitter DecorJitter(uint32 Seed);

	/** Web px -> cm for VFX sizes and upright heights (combat-feel.md 0: 45 px per tile, 100 cm per tile). */
	inline constexpr float VfxPxToCm = 100.f / 45.f;

	/** Frame-rate independent exponential approach: returns the blend factor for a time constant (seconds). */
	inline float ExpBlend(float DeltaSec, float TimeConstantSec)
	{
		return TimeConstantSec <= 0.f ? 1.f : 1.f - FMath::Exp(-DeltaSec / TimeConstantSec);
	}

	/** Easing used by the web tweens. */
	inline float EaseOutQuad(float T) { return 1.f - (1.f - T) * (1.f - T); }
	inline float EaseInQuad(float T) { return T * T; }
	inline float EaseOutCubic(float T) { const float U = 1.f - T; return 1.f - U * U * U; }
	inline float EaseInCubic(float T) { return T * T * T; }
	inline float EaseOutExpo(float T) { return T >= 1.f ? 1.f : 1.f - FMath::Pow(2.f, -10.f * T); }
	inline float EaseInOutSine(float T) { return -(FMath::Cos(UE_PI * T) - 1.f) * 0.5f; }
	inline float EaseOutBack(float T)
	{
		constexpr float S = 1.70158f;
		const float U = T - 1.f;
		return 1.f + U * U * ((S + 1.f) * U + S);
	}
}
