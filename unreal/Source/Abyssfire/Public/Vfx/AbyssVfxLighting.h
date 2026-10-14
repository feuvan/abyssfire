// Pure helpers of the fake lights (DECISIONS R18, P11 revised): the zone's lightScale and the choice of the toon light
// slots (MPC_AF_Lighting Light<i>PosR / Light<i>Col) written by UAbyssVfxSystem. Header-only and engine-light (CoreMinimal
// math and containers), so they are unit-tested outside UE.
#pragma once

#include "CoreMinimal.h"

namespace AbyssVfxLighting
{
	/** Floor of the light scale (world-map-nav.md 14.1: the bright moods). */
	inline constexpr float MinLightScale = 0.12f;
	/** Toon light slots of MPC_AF_Lighting (desktop); mobile writes the first MobileLightSlots and clears the rest. */
	inline constexpr int32 MaxLightSlots = 8;
	inline constexpr int32 MobileLightSlots = 4;
	/** The last used slot fades out over this distance margin to the first light left out (no popping). */
	inline constexpr float SlotCrossFadeCm = 150.f;

	/**
	 * The web lightScale (DECISIONS R18, world-map-nav.md 14.1): max(0.12, ambientAlpha x (1 - luma601(ambient)) x 1.4)
	 * with the mood's ambient colour as 0xRRGGBB sRGB (luma on the 0..1 sRGB values, as LightingSystem.ts). Plains,
	 * mountain, desert 0.12, forest 0.1605, abyss 0.2522.
	 */
	inline float LightScaleFromMood(uint32 AmbientRgb, float AmbientAlpha)
	{
		const float R = static_cast<float>((AmbientRgb >> 16) & 0xFFu) / 255.f;
		const float G = static_cast<float>((AmbientRgb >> 8) & 0xFFu) / 255.f;
		const float B = static_cast<float>(AmbientRgb & 0xFFu) / 255.f;
		const float Luma = 0.299f * R + 0.587f * G + 0.114f * B;
		return FMath::Max(MinLightScale, AmbientAlpha * (1.f - Luma) * 1.4f);
	}

	/** One chosen light slot: the candidate's index and its weight (1, or the cross-fade of the last used slot). */
	struct FSlotPick
	{
		int32 Candidate = INDEX_NONE;
		float Weight = 1.f;
	};

	/**
	 * The MaxSlots candidates nearest the focus, nearest first (equal distances keep the lower index). The last used slot
	 * is weighted saturate((d_next - d_last) / CrossFadeCm), d_next being the nearest candidate left out, so a light
	 * entering the set fades in while the one it replaces fades out; with no candidate left out every weight is 1.
	 * Returns the number of slots used (<= MaxSlots).
	 */
	inline int32 SelectLightSlots(TConstArrayView<float> DistancesCm, int32 MaxSlots, float CrossFadeCm, TArray<FSlotPick>& Out)
	{
		Out.Reset();
		const int32 Count = DistancesCm.Num();
		const int32 Wanted = FMath::Min(FMath::Max(0, MaxSlots), Count);
		if (Wanted <= 0)
		{
			return 0;
		}
		// Partial selection (a zone holds a few dozen lights at most): Wanted + 1 rounds of the nearest unused candidate,
		// the extra round finding d_next.
		TArray<bool> Used;
		Used.Init(false, Count);
		float NextDistance = -1.f;
		for (int32 Round = 0; Round <= Wanted; ++Round)
		{
			int32 Best = INDEX_NONE;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				if (!Used[Index] && (Best == INDEX_NONE || DistancesCm[Index] < DistancesCm[Best]))
				{
					Best = Index;
				}
			}
			if (Best == INDEX_NONE)
			{
				break;
			}
			Used[Best] = true;
			if (Round == Wanted)
			{
				NextDistance = DistancesCm[Best];
				break;
			}
			FSlotPick Pick;
			Pick.Candidate = Best;
			Out.Add(Pick);
		}
		if (NextDistance >= 0.f && Out.Num() > 0)
		{
			const float LastDistance = DistancesCm[Out.Last().Candidate];
			const float Margin = CrossFadeCm > 0.f ? (NextDistance - LastDistance) / CrossFadeCm : 1.f;
			Out.Last().Weight = FMath::Clamp(Margin, 0.f, 1.f);
		}
		return Out.Num();
	}
}
