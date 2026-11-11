// The decisions of world entity picking (Input/AbyssPicking.cpp; performance audit F4).
//
// The pick runs every frame under the mouse (hover) and on every press. It used to copy the registry, then for every
// candidate ask the player controller to project the 8 corners of its box, and each of those calls rebuilt the
// view-projection matrix. Now the glue fetches the projection once per pick, and before it projects a candidate's
// corners it asks MayContain: a conservative screen bound of the box from interval arithmetic on the same matrix.
// A candidate the bound rejects cannot contain the pointer, so the exact rectangle test is only run for the few boxes
// near the pointer, and the winner is the one the old code chose (pick_test.cpp plays both on random scenes).
//
// What is NOT done (audit F4 offered it): culling by the actor's origin before its mesh bounds are read. No bound on how
// far a pickable's mesh reaches from its origin can be proven (tall props, a boss mid-leap), and a cull that can miss
// would let a click pick something else than before.
//
// FPicker is the selection itself (web press priority, then the most centred rectangle, then the nearest to the
// camera), moved here unchanged.
//
// UE-free and header-only (std only), tested in unreal/PresentationTests/tests/pick_test.cpp.
#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace AbyssPickRules
{
	struct FVec2
	{
		double X = 0.0;
		double Y = 0.0;
	};

	struct FRect
	{
		double MinX = 0.0;
		double MinY = 0.0;
		double MaxX = 0.0;
		double MaxY = 0.0;
	};

	/** One pick's projection: the engine's view-projection matrix (row-vector convention: clip = (x, y, z, 1) x M) and the
	 *  constrained view rectangle in viewport pixels. */
	struct FView
	{
		double M[4][4] = {};
		double RectX = 0.0;
		double RectY = 0.0;
		double RectW = 0.0;
		double RectH = 0.0;
	};

	/** Screen pixels the conservative bound is widened by: far above the rounding of the engine's projection (it
	 *  narrows the perspective divide to float: under 0.01 px on a 4K viewport). */
	inline constexpr double BoundMarginPx = 1.0;
	/** A box whose nearest possible clip w is under this (cm of view depth) is not bounded: the exact test decides. */
	inline constexpr double MinBoundW = 1.0;

	/** FSceneView::ProjectWorldToScreen as the engine computes it (double transform, float divide and normalise): false
	 *  when the point is behind the camera. The glue calls the engine's own; tests and the reference use this. */
	inline bool Project(const FView& View, double X, double Y, double Z, FVec2& Out)
	{
		const double ClipX = X * View.M[0][0] + Y * View.M[1][0] + Z * View.M[2][0] + View.M[3][0];
		const double ClipY = X * View.M[0][1] + Y * View.M[1][1] + Z * View.M[2][1] + View.M[3][1];
		const double ClipW = X * View.M[0][3] + Y * View.M[1][3] + Z * View.M[2][3] + View.M[3][3];
		if (!(ClipW > 0.0))
		{
			return false;
		}
		const float Rhw = 1.0f / static_cast<float>(ClipW);
		const float NdcX = static_cast<float>(ClipX * static_cast<double>(Rhw));
		const float NdcY = static_cast<float>(ClipY * static_cast<double>(Rhw));
		const float NormX = NdcX / 2.f + 0.5f;
		const float NormY = 1.f - NdcY / 2.f - 0.5f;
		Out.X = static_cast<double>(NormX) * View.RectW + View.RectX;
		Out.Y = static_cast<double>(NormY) * View.RectH + View.RectY;
		return true;
	}

	/** Range of a clip coordinate (column `Col` of the matrix) over the box. */
	inline void ClipRange(const FView& View, int Col, const double Min[3], const double Max[3], double& OutLo, double& OutHi)
	{
		OutLo = View.M[3][Col];
		OutHi = View.M[3][Col];
		for (int Axis = 0; Axis < 3; ++Axis)
		{
			const double A = Min[Axis] * View.M[Axis][Col];
			const double B = Max[Axis] * View.M[Axis][Col];
			OutLo += A < B ? A : B;
			OutHi += A < B ? B : A;
		}
	}

	/** A screen rectangle that contains the projection of every point of the box (so of its 8 corners), widened by
	 *  BoundMarginPx. False when the box may reach the camera plane: then nothing is known and the exact test decides. */
	inline bool BoundBox(const FView& View, const double Min[3], const double Max[3], FRect& Out)
	{
		double XLo = 0.0, XHi = 0.0, YLo = 0.0, YHi = 0.0, WLo = 0.0, WHi = 0.0;
		ClipRange(View, 3, Min, Max, WLo, WHi);
		if (!(WLo >= MinBoundW) || !std::isfinite(WHi))
		{
			return false;
		}
		ClipRange(View, 0, Min, Max, XLo, XHi);
		ClipRange(View, 1, Min, Max, YLo, YHi);
		// x / w over [XLo, XHi] x [WLo, WHi] with w > 0: the extremes are at the corners of that rectangle.
		const double NdcXLo = XLo < 0.0 ? XLo / WLo : XLo / WHi;
		const double NdcXHi = XHi > 0.0 ? XHi / WLo : XHi / WHi;
		const double NdcYLo = YLo < 0.0 ? YLo / WLo : YLo / WHi;
		const double NdcYHi = YHi > 0.0 ? YHi / WLo : YHi / WHi;
		if (!std::isfinite(NdcXLo) || !std::isfinite(NdcXHi) || !std::isfinite(NdcYLo) || !std::isfinite(NdcYHi))
		{
			return false;
		}
		Out.MinX = (NdcXLo / 2.0 + 0.5) * View.RectW + View.RectX - BoundMarginPx;
		Out.MaxX = (NdcXHi / 2.0 + 0.5) * View.RectW + View.RectX + BoundMarginPx;
		// Screen y runs down: the top comes from the largest clip y.
		Out.MinY = (1.0 - NdcYHi / 2.0 - 0.5) * View.RectH + View.RectY - BoundMarginPx;
		Out.MaxY = (1.0 - NdcYLo / 2.0 - 0.5) * View.RectH + View.RectY + BoundMarginPx;
		return true;
	}

	/** The pre-cull. False only when the box's projected rectangle, inflated by `Slop`, cannot contain the pointer. */
	inline bool MayContain(const FView& View, const double Min[3], const double Max[3], const FVec2& Pointer, double Slop)
	{
		FRect Bound;
		if (!BoundBox(View, Min, Max, Bound))
		{
			return true;
		}
		return Pointer.X >= Bound.MinX - Slop && Pointer.X <= Bound.MaxX + Slop && Pointer.Y >= Bound.MinY - Slop
			&& Pointer.Y <= Bound.MaxY + Slop;
	}

	/** The rectangle of 8 projected corners. */
	inline FRect RectOf(const FVec2 Corners[8])
	{
		FRect R{ Corners[0].X, Corners[0].Y, Corners[0].X, Corners[0].Y };
		for (int Index = 1; Index < 8; ++Index)
		{
			R.MinX = Corners[Index].X < R.MinX ? Corners[Index].X : R.MinX;
			R.MinY = Corners[Index].Y < R.MinY ? Corners[Index].Y : R.MinY;
			R.MaxX = Corners[Index].X > R.MaxX ? Corners[Index].X : R.MaxX;
			R.MaxY = Corners[Index].Y > R.MaxY ? Corners[Index].Y : R.MaxY;
		}
		return R;
	}

	/** The selection among the rectangles under the pointer: lower priority wins, then the more centred rectangle (by
	 *  more than 1e-3), then the smaller camera distance. */
	class FPicker
	{
	public:
		FPicker(const FVec2& InPointer, double InSlop) : Pointer(InPointer), Slop(InSlop > 0.0 ? InSlop : 0.0) {}

		/** A candidate of a priority above the best so far cannot win: skip its projection. */
		bool CanWin(int Priority) const { return Priority <= BestPriority; }

		/** Offers a candidate's exact rectangle (before the slop). True when it is the best so far. */
		bool Offer(std::uint32_t Id, int Priority, const FRect& Exact, double DepthSquared)
		{
			const FRect R{ Exact.MinX - Slop, Exact.MinY - Slop, Exact.MaxX + Slop, Exact.MaxY + Slop };
			if (Priority > BestPriority || Pointer.X < R.MinX || Pointer.X > R.MaxX || Pointer.Y < R.MinY || Pointer.Y > R.MaxY)
			{
				return false;
			}
			const double CentreX = (R.MinX + R.MaxX) * 0.5;
			const double CentreY = (R.MinY + R.MaxY) * 0.5;
			const double HalfX = (R.MaxX - R.MinX) * 0.5;
			const double HalfY = (R.MaxY - R.MinY) * 0.5;
			const double Half = HalfX > HalfY ? HalfX : HalfY;
			const double Dx = Pointer.X - CentreX;
			const double Dy = Pointer.Y - CentreY;
			const double Centredness = std::sqrt(Dx * Dx + Dy * Dy) / (Half > 1.0 ? Half : 1.0);
			const bool bBetter = Priority < BestPriority
				|| (Priority == BestPriority && (Centredness < BestCentredness - 1.0e-3
					|| (std::fabs(Centredness - BestCentredness) <= 1.0e-3 && DepthSquared < BestDepth)));
			if (!bBetter)
			{
				return false;
			}
			BestPriority = Priority;
			BestCentredness = Centredness;
			BestDepth = DepthSquared;
			BestId = Id;
			bAny = true;
			return true;
		}

		bool HasPick() const { return bAny; }
		std::uint32_t PickedId() const { return BestId; }

	private:
		FVec2 Pointer;
		double Slop = 0.0;
		int BestPriority = (std::numeric_limits<int>::max)();
		double BestCentredness = (std::numeric_limits<double>::max)();
		double BestDepth = (std::numeric_limits<double>::max)();
		std::uint32_t BestId = 0;
		bool bAny = false;
	};
}
