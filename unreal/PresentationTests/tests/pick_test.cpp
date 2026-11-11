// World entity picking (performance audit F4; Private/Input/AbyssPickRules.h, used by Input/AbyssPicking.cpp): the
// conservative screen bound never rejects a box whose exact rectangle contains the pointer, and the pick with the
// pre-cull chooses exactly what the old pick chose (every candidate projected corner by corner, no cull) on random
// scenes: the click path shares the hover's code, so it must not change.
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "doctest/doctest.h"

#include "Input/AbyssPickRules.h"

namespace
{
	using namespace AbyssPickRules;

	struct FRng
	{
		std::uint64_t State = 0x9e3779b97f4a7c15ull;
		double Next()   // [0, 1)
		{
			State ^= State << 13;
			State ^= State >> 7;
			State ^= State << 17;
			return static_cast<double>(State >> 11) / 9007199254740992.0;
		}
		double Range(double Lo, double Hi) { return Lo + (Hi - Lo) * Next(); }
	};

	/** A UE-style view-projection (row vectors; x forward, y right, z up in view space before the axis swap): a camera at
	 *  `Eye` looking along yaw / pitch, reversed-z perspective with clip w = view depth. */
	FView MakeView(double EyeX, double EyeY, double EyeZ, double YawDeg, double PitchDeg, double HalfFovDeg, double W, double H,
		double RectX = 0.0, double RectY = 0.0)
	{
		const double Pi = 3.14159265358979323846;
		const double Yaw = YawDeg * Pi / 180.0;
		const double Pitch = PitchDeg * Pi / 180.0;
		// camera basis in world space
		const double Fx = std::cos(Pitch) * std::cos(Yaw), Fy = std::cos(Pitch) * std::sin(Yaw), Fz = std::sin(Pitch);
		const double Rx = -std::sin(Yaw), Ry = std::cos(Yaw), Rz = 0.0;
		const double Ux = Fy * Rz - Fz * Ry, Uy = Fz * Rx - Fx * Rz, Uz = Fx * Ry - Fy * Rx;   // forward x right
		const double UpSign = Uz < 0.0 ? -1.0 : 1.0;
		const double Tan = std::tan(HalfFovDeg * Pi / 180.0);
		const double Sx = 1.0 / Tan;
		const double Sy = (W / H) / Tan;
		const double Near = 10.0;
		FView V;
		// clip.x = dot(P - Eye, Right) * Sx ; clip.y = dot(P - Eye, Up) * Sy ; clip.z = Near ; clip.w = dot(P - Eye, Fwd)
		const double Right[3] = { Rx, Ry, Rz };
		const double Up[3] = { Ux * UpSign, Uy * UpSign, Uz * UpSign };
		const double Fwd[3] = { Fx, Fy, Fz };
		const double Eye[3] = { EyeX, EyeY, EyeZ };
		double Tx = 0.0, Ty = 0.0, Tw = 0.0;
		for (int Axis = 0; Axis < 3; ++Axis)
		{
			V.M[Axis][0] = Right[Axis] * Sx;
			V.M[Axis][1] = Up[Axis] * Sy;
			V.M[Axis][2] = 0.0;
			V.M[Axis][3] = Fwd[Axis];
			Tx -= Eye[Axis] * Right[Axis] * Sx;
			Ty -= Eye[Axis] * Up[Axis] * Sy;
			Tw -= Eye[Axis] * Fwd[Axis];
		}
		V.M[3][0] = Tx;
		V.M[3][1] = Ty;
		V.M[3][2] = Near;
		V.M[3][3] = Tw;
		V.RectX = RectX;
		V.RectY = RectY;
		V.RectW = W;
		V.RectH = H;
		return V;
	}

	struct FCandidate
	{
		std::uint32_t Id = 0;
		int Priority = 0;
		double Min[3] = {};
		double Max[3] = {};
		double Origin[3] = {};
	};

	/** The exact rectangle of a box: the 8 corners through the engine's projection. False when one is behind the camera. */
	bool ExactRect(const FView& View, const FCandidate& C, FRect& Out)
	{
		FVec2 Corners[8];
		for (int Index = 0; Index < 8; ++Index)
		{
			if (!Project(View, (Index & 1) ? C.Max[0] : C.Min[0], (Index & 2) ? C.Max[1] : C.Min[1],
				(Index & 4) ? C.Max[2] : C.Min[2], Corners[Index]))
			{
				return false;
			}
		}
		Out = RectOf(Corners);
		return true;
	}

	double DepthOf(const double Eye[3], const FCandidate& C)
	{
		const double Dx = C.Origin[0] - Eye[0], Dy = C.Origin[1] - Eye[1], Dz = C.Origin[2] - Eye[2];
		return Dx * Dx + Dy * Dy + Dz * Dz;
	}

	/** The pick as AbyssPicking.cpp had it before the audit, transcribed: no cull, FVector2D min / max per corner, the
	 *  slop, the containment test, the centredness and the three-way comparison, in registry order. 0 = nothing. */
	std::uint32_t ReferencePick(const FView& View, const std::vector<FCandidate>& Candidates, const FVec2& Pointer,
		double SlopPixels, const double Eye[3])
	{
		const double Slop = SlopPixels > 0.0 ? SlopPixels : 0.0;
		int BestPriority = (std::numeric_limits<int>::max)();
		double BestCentredness = (std::numeric_limits<double>::max)();
		double BestDepth = (std::numeric_limits<double>::max)();
		std::uint32_t Best = 0;
		for (const FCandidate& C : Candidates)
		{
			if (C.Priority > BestPriority)
			{
				continue;
			}
			double MinX = (std::numeric_limits<double>::max)(), MinY = MinX;
			double MaxX = std::numeric_limits<double>::lowest(), MaxY = MaxX;
			bool bBehind = false;
			for (int Index = 0; Index < 8 && !bBehind; ++Index)
			{
				FVec2 P;
				if (!Project(View, (Index & 1) ? C.Max[0] : C.Min[0], (Index & 2) ? C.Max[1] : C.Min[1],
					(Index & 4) ? C.Max[2] : C.Min[2], P))
				{
					bBehind = true;
					break;
				}
				MinX = P.X < MinX ? P.X : MinX;
				MinY = P.Y < MinY ? P.Y : MinY;
				MaxX = P.X > MaxX ? P.X : MaxX;
				MaxY = P.Y > MaxY ? P.Y : MaxY;
			}
			if (bBehind)
			{
				continue;
			}
			MinX -= Slop;
			MinY -= Slop;
			MaxX += Slop;
			MaxY += Slop;
			if (Pointer.X < MinX || Pointer.X > MaxX || Pointer.Y < MinY || Pointer.Y > MaxY)
			{
				continue;
			}
			const double CentreX = (MinX + MaxX) * 0.5, CentreY = (MinY + MaxY) * 0.5;
			const double HalfX = (MaxX - MinX) * 0.5, HalfY = (MaxY - MinY) * 0.5;
			const double HalfMax = HalfX > HalfY ? HalfX : HalfY;
			const double Dist = std::sqrt((Pointer.X - CentreX) * (Pointer.X - CentreX) + (Pointer.Y - CentreY) * (Pointer.Y - CentreY));
			const double Centredness = Dist / (HalfMax > 1.0 ? HalfMax : 1.0);
			const double Depth = DepthOf(Eye, C);
			const bool bBetter = C.Priority < BestPriority
				|| (C.Priority == BestPriority && (Centredness < BestCentredness - 1.0e-3
					|| (std::fabs(Centredness - BestCentredness) <= 1.0e-3 && Depth < BestDepth)));
			if (!bBetter)
			{
				continue;
			}
			BestPriority = C.Priority;
			BestCentredness = Centredness;
			BestDepth = Depth;
			Best = C.Id;
		}
		return Best;
	}

	/** The pick as the glue does it now: CanWin, the pre-cull, the exact rectangle only for survivors, FPicker. */
	std::uint32_t NewPick(const FView& View, const std::vector<FCandidate>& Candidates, const FVec2& Pointer, double SlopPixels,
		const double Eye[3], int* OutProjected = nullptr)
	{
		const double Slop = SlopPixels > 0.0 ? SlopPixels : 0.0;
		FPicker Picker(Pointer, Slop);
		int Projected = 0;
		for (const FCandidate& C : Candidates)
		{
			if (!Picker.CanWin(C.Priority) || !MayContain(View, C.Min, C.Max, Pointer, Slop))
			{
				continue;
			}
			++Projected;
			FRect Rect;
			if (!ExactRect(View, C, Rect))
			{
				continue;
			}
			Picker.Offer(C.Id, C.Priority, Rect, DepthOf(Eye, C));
		}
		if (OutProjected != nullptr)
		{
			*OutProjected = Projected;
		}
		return Picker.HasPick() ? Picker.PickedId() : 0;
	}

	/** A field of pickables around (0, 0): characters, loot, tall props, a few huge or camera-straddling boxes. */
	std::vector<FCandidate> MakeScene(FRng& Rng, int Count, double Spread)
	{
		std::vector<FCandidate> Out;
		for (int Index = 0; Index < Count; ++Index)
		{
			FCandidate C;
			C.Id = static_cast<std::uint32_t>(Index + 1);
			C.Priority = static_cast<int>(Rng.Next() * 5.0);
			const double X = Rng.Range(-Spread, Spread), Y = Rng.Range(-Spread, Spread);
			const double Kind = Rng.Next();
			double Radius = 45.0, Height = 175.0, Base = 0.0;
			if (Kind < 0.3) { Radius = 30.0; Height = 30.0; }                        // loot
			else if (Kind < 0.4) { Radius = Rng.Range(60.0, 250.0); Height = Rng.Range(300.0, 900.0); }   // tall props
			else if (Kind < 0.45) { Radius = Rng.Range(500.0, 3000.0); Height = 50.0; }                  // wide and flat
			else if (Kind < 0.5) { Base = Rng.Range(200.0, 2500.0); }                                    // high in the air
			C.Min[0] = X - Radius; C.Min[1] = Y - Radius; C.Min[2] = Base;
			C.Max[0] = X + Radius; C.Max[1] = Y + Radius; C.Max[2] = Base + Height;
			C.Origin[0] = X + Rng.Range(-20.0, 20.0); C.Origin[1] = Y + Rng.Range(-20.0, 20.0); C.Origin[2] = Base;
			Out.push_back(C);
		}
		return Out;
	}
}

TEST_SUITE("pick")
{
	TEST_CASE("the projection puts the view centre mid-screen and y down")
	{
		// the game's framing: pitch -50, yaw 45, 2125 cm back from the focus
		const double Pi = 3.14159265358979323846;
		const double Pitch = -50.0 * Pi / 180.0, Yaw = 45.0 * Pi / 180.0;
		const double Fwd[3] = { std::cos(Pitch) * std::cos(Yaw), std::cos(Pitch) * std::sin(Yaw), std::sin(Pitch) };
		const FView View = MakeView(-Fwd[0] * 2125.0, -Fwd[1] * 2125.0, -Fwd[2] * 2125.0, 45.0, -50.0, 15.0, 1280.0, 720.0);
		FVec2 P;
		REQUIRE(Project(View, 0.0, 0.0, 0.0, P));
		CHECK(P.X == doctest::Approx(640.0).epsilon(1e-4));
		CHECK(P.Y == doctest::Approx(360.0).epsilon(1e-4));
		FVec2 Above;
		REQUIRE(Project(View, 0.0, 0.0, 200.0, Above));
		CHECK(Above.Y < P.Y);                                  // up in the world is up on screen (smaller y)
		CHECK(Above.X == doctest::Approx(640.0).epsilon(1e-3));
		FVec2 Behind;
		CHECK_FALSE(Project(View, -Fwd[0] * 4000.0, -Fwd[1] * 4000.0, -Fwd[2] * 4000.0, Behind));
		// a constrained view rectangle offsets the result (letterbox bars)
		const FView Boxed = MakeView(-Fwd[0] * 2125.0, -Fwd[1] * 2125.0, -Fwd[2] * 2125.0, 45.0, -50.0, 15.0, 1280.0, 720.0, 0.0, 40.0);
		REQUIRE(Project(Boxed, 0.0, 0.0, 0.0, P));
		CHECK(P.Y == doctest::Approx(400.0).epsilon(1e-4));
	}

	TEST_CASE("the bound contains the exact rectangle; a box at the camera plane is never culled")
	{
		FRng Rng;
		int Bounded = 0;
		for (int Scene = 0; Scene < 400; ++Scene)
		{
			const double Pi = 3.14159265358979323846;
			const double PitchDeg = Rng.Range(-80.0, -20.0), YawDeg = Rng.Range(0.0, 360.0), Dist = Rng.Range(600.0, 4000.0);
			const double Pitch = PitchDeg * Pi / 180.0, Yaw = YawDeg * Pi / 180.0;
			const double Eye[3] = { -std::cos(Pitch) * std::cos(Yaw) * Dist, -std::cos(Pitch) * std::sin(Yaw) * Dist, -std::sin(Pitch) * Dist };
			// (one draw per statement: the order of two draws in one argument list is the compiler's choice)
			const double HalfFovDeg = Rng.Range(8.0, 45.0);
			const double RectY = Rng.Range(0.0, 60.0);
			const FView View = MakeView(Eye[0], Eye[1], Eye[2], YawDeg, PitchDeg, HalfFovDeg, 1920.0, 1080.0, 0.0, RectY);
			for (const FCandidate& C : MakeScene(Rng, 40, 5000.0))
			{
				FRect Exact;
				FRect Bound;
				const bool bExact = ExactRect(View, C, Exact);
				const bool bBound = BoundBox(View, C.Min, C.Max, Bound);
				if (!bExact)
				{
					CHECK_FALSE(bBound);   // a corner behind the camera: no bound claimed, the exact test decides
					continue;
				}
				if (!bBound)
				{
					continue;
				}
				++Bounded;
				CHECK(Bound.MinX <= Exact.MinX);
				CHECK(Bound.MinY <= Exact.MinY);
				CHECK(Bound.MaxX >= Exact.MaxX);
				CHECK(Bound.MaxY >= Exact.MaxY);
			}
		}
		CHECK(Bounded > 10000);
	}

	TEST_CASE("the pick with the pre-cull is the old pick, on random scenes and at rectangle edges")
	{
		FRng Rng;
		int Picks = 0;
		int Hits = 0;
		long long ProjectedNew = 0;
		long long ProjectedOld = 0;
		for (int Scene = 0; Scene < 300; ++Scene)
		{
			const double Pi = 3.14159265358979323846;
			const bool bGameFraming = (Scene % 3) != 0;
			const double PitchDeg = bGameFraming ? -50.0 : Rng.Range(-85.0, -15.0);
			const double YawDeg = bGameFraming ? 45.0 : Rng.Range(0.0, 360.0);
			const double Dist = bGameFraming ? Rng.Range(1500.0, 2800.0) : Rng.Range(500.0, 5000.0);
			const double Pitch = PitchDeg * Pi / 180.0, Yaw = YawDeg * Pi / 180.0;
			const double Eye[3] = { -std::cos(Pitch) * std::cos(Yaw) * Dist, -std::cos(Pitch) * std::sin(Yaw) * Dist, -std::sin(Pitch) * Dist };
			const double W = (Scene % 2) != 0 ? 1280.0 : 2560.0, H = (Scene % 2) != 0 ? 720.0 : 1440.0;
			const double HalfFovDeg = bGameFraming ? 15.0 : Rng.Range(8.0, 45.0);
			const FView View = MakeView(Eye[0], Eye[1], Eye[2], YawDeg, PitchDeg, HalfFovDeg, W, H);
			const int Count = 20 + static_cast<int>(Rng.Next() * 100.0);
			const double Spread = Rng.Range(800.0, 6000.0);
			const std::vector<FCandidate> Candidates = MakeScene(Rng, Count, Spread);
			const double Slop = (Scene % 4) == 0 ? 0.0 : ((Scene % 4) == 1 ? 6.0 : ((Scene % 4) == 2 ? 28.0 : -3.0));

			std::vector<FVec2> Pointers;
			for (int Index = 0; Index < 60; ++Index)
			{
				Pointers.push_back({ Rng.Range(0.0, W), Rng.Range(0.0, H) });
			}
			// on and just around the edges and centres of real rectangles: where a cull that is too tight would differ
			for (const FCandidate& C : Candidates)
			{
				FRect R;
				if (!ExactRect(View, C, R))
				{
					continue;
				}
				const double S = Slop > 0.0 ? Slop : 0.0;
				Pointers.push_back({ (R.MinX + R.MaxX) * 0.5, (R.MinY + R.MaxY) * 0.5 });
				Pointers.push_back({ R.MinX - S, R.MinY - S });
				Pointers.push_back({ R.MaxX + S, R.MaxY + S });
				Pointers.push_back({ R.MinX - S + 1e-7, R.MaxY + S - 1e-7 });
				Pointers.push_back({ R.MaxX + S + 0.5, (R.MinY + R.MaxY) * 0.5 });
			}
			for (const FVec2& Pointer : Pointers)
			{
				int Projected = 0;
				const std::uint32_t Old = ReferencePick(View, Candidates, Pointer, Slop, Eye);
				const std::uint32_t New = NewPick(View, Candidates, Pointer, Slop, Eye, &Projected);
				REQUIRE(New == Old);
				++Picks;
				Hits += Old != 0 ? 1 : 0;
				ProjectedNew += Projected;
				ProjectedOld += static_cast<long long>(Candidates.size());
			}
		}
		CHECK(Picks > 50000);
		CHECK(Hits > 10000);                       // the comparison is not vacuous: many pointers pick something
		CHECK(Picks - Hits > 1000);                // ... and many pick nothing
		CHECK(ProjectedNew * 4 < ProjectedOld);    // the cull does its job: under a quarter of the boxes are projected
	}

	TEST_CASE("the selection: priority, then centredness, then camera distance")
	{
		const FVec2 Pointer{ 100.0, 100.0 };
		FPicker Picker(Pointer, 0.0);
		CHECK_FALSE(Picker.HasPick());
		CHECK_FALSE(Picker.Offer(1, 3, FRect{ 200.0, 200.0, 300.0, 300.0 }, 10.0));   // not under the pointer
		CHECK(Picker.Offer(2, 3, FRect{ 0.0, 0.0, 400.0, 400.0 }, 10.0));               // a monster, off-centre
		CHECK(Picker.Offer(3, 3, FRect{ 50.0, 50.0, 150.0, 150.0 }, 99.0));             // more centred wins
		CHECK_FALSE(Picker.Offer(4, 3, FRect{ 50.0, 50.0, 150.0, 150.0 }, 100.0));      // as centred, farther: no
		CHECK(Picker.Offer(5, 3, FRect{ 50.0, 50.0, 150.0, 150.0 }, 50.0));             // as centred, nearer: yes
		CHECK(Picker.Offer(6, 0, FRect{ 0.0, 0.0, 400.0, 400.0 }, 500.0));              // loot beats any monster
		CHECK_FALSE(Picker.CanWin(1));
		CHECK(Picker.CanWin(0));
		CHECK_FALSE(Picker.Offer(7, 3, FRect{ 99.0, 99.0, 101.0, 101.0 }, 1.0));
		CHECK(Picker.PickedId() == 6);

		// the slop inflates the rectangle (touch) and a negative slop is none
		FPicker Touch(Pointer, 28.0);
		CHECK(Touch.Offer(1, 3, FRect{ 120.0, 120.0, 180.0, 180.0 }, 1.0));
		FPicker Mouse(Pointer, -5.0);
		CHECK_FALSE(Mouse.Offer(1, 3, FRect{ 120.0, 120.0, 180.0, 180.0 }, 1.0));
	}
}
