// Ground-item labels on demand (DECISIONS R17): which item labels show, how they stack and when a combat float dims
// them. Pure (CoreMinimal math and containers only), so the rules are unit-tested outside UE; SAbyssWorldLayer paints.
#pragma once

#include "CoreMinimal.h"

namespace AbyssGroundLabels
{
	/** Every item label within this many tiles of the hero shows (all platforms). */
	inline constexpr double ProximityTiles = 2.0;
	/** A rare-or-better drop shows its label this long after it appears. */
	inline constexpr double RareDropShowSec = 4.0;
	/**
	 * Rows a label may climb over the ones already placed before it is dropped: 2 lifts = at most 3 rows (DECISIONS R17);
	 * Alt held (every item on screen): 6 lifts, 7 rows.
	 */
	inline constexpr int32 MaxLifts = 2;
	inline constexpr int32 MaxLiftsAlt = 6;
	/** Clearance between two labels, and the gap added to a label's height for one row. */
	inline constexpr double OverlapGapPx = 2.0;
	inline constexpr double RowGapPx = 2.0;
	/** A label crossing a combat float younger than this draws at DimmedAlpha (floats always paint above labels). */
	inline constexpr double FloatDimAgeSec = 0.6;
	inline constexpr float DimmedAlpha = 0.35f;
	/** Alt mode and hover: a #0E0A14 plate behind the name. */
	inline constexpr uint32 PlateRgb = 0x0E0A14;
	inline constexpr float PlateAlpha = 0.65f;
	inline constexpr double PlatePaddingPx = 3.0;

	struct FVisibilityInput
	{
		bool bHovered = false;
		bool bPickupTarget = false;
		bool bAltHeld = false;
		double HeroDistanceTiles = 1.0e9;
		bool bRareOrBetter = false;
		double AgeSec = 1.0e9;
	};

	/**
	 * Hovered, Alt held (desktop), the hero within 2 tiles, or a rare+ drop for its first 4 s. bPickupTarget is for a future
	 * snapshot pickup target (none today: SAbyssWorldLayer leaves it false).
	 */
	inline bool ShouldShow(const FVisibilityInput& In)
	{
		return In.bHovered || In.bPickupTarget || In.bAltHeld || In.HeroDistanceTiles <= ProximityTiles
			|| (In.bRareOrBetter && In.AgeSec >= 0.0 && In.AgeSec < RareDropShowSec);
	}

	/** Screen rect (Slate px, Y down). */
	struct FRect
	{
		double MinX = 0.0;
		double MinY = 0.0;
		double MaxX = 0.0;
		double MaxY = 0.0;
	};

	/** True when the rects overlap or come closer than Gap. */
	inline bool Meets(const FRect& A, const FRect& B, double Gap)
	{
		return A.MinX < B.MaxX + Gap && B.MinX < A.MaxX + Gap && A.MinY < B.MaxY + Gap && B.MinY < A.MaxY + Gap;
	}

	/** One label to place: its projected anchor (the bottom centre of the label) and its box (with the plate padding). */
	struct FItem
	{
		double AnchorX = 0.0;
		double AnchorY = 0.0;
		double Width = 0.0;
		double Height = 0.0;
	};

	struct FPlacement
	{
		FRect Rect;
		int32 Lifts = 0;
		bool bShown = false;
	};

	/**
	 * Lays the labels out nearest-first (the largest anchor Y first, so the label in front keeps its spot; equal anchors in
	 * input order): each starts on its anchor and climbs one row (its height + RowGapPx) while it meets a label placed
	 * before it (+OverlapGapPx), at most MaxLiftCount rows; one still meeting a label after that is dropped
	 * (bShown false). Out[i] is the placement of Items[i].
	 */
	inline void Layout(TConstArrayView<FItem> Items, int32 MaxLiftCount, TArray<FPlacement>& Out)
	{
		const int32 Count = Items.Num();
		Out.Reset();
		Out.SetNum(Count);
		TArray<int32> Order;
		Order.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Order.Add(Index);
		}
		Order.Sort([&Items](const int32 A, const int32 B)
		{
			return Items[A].AnchorY > Items[B].AnchorY || (Items[A].AnchorY == Items[B].AnchorY && A < B);
		});
		TArray<FRect> Placed;
		Placed.Reserve(Count);
		for (const int32 Index : Order)
		{
			const FItem& Item = Items[Index];
			const double Row = Item.Height + RowGapPx;
			for (int32 Lift = 0; Lift <= FMath::Max(0, MaxLiftCount); ++Lift)
			{
				FRect Rect;
				Rect.MinX = Item.AnchorX - Item.Width * 0.5;
				Rect.MaxX = Item.AnchorX + Item.Width * 0.5;
				Rect.MaxY = Item.AnchorY - Row * Lift;
				Rect.MinY = Rect.MaxY - Item.Height;
				bool bFree = true;
				for (const FRect& Other : Placed)
				{
					if (Meets(Rect, Other, OverlapGapPx))
					{
						bFree = false;
						break;
					}
				}
				if (bFree)
				{
					FPlacement& Placement = Out[Index];
					Placement.Rect = Rect;
					Placement.Lifts = Lift;
					Placement.bShown = true;
					Placed.Add(Rect);
					break;
				}
			}
		}
	}
}
