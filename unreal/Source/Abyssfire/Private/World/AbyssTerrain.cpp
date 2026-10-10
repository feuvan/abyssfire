#include "World/AbyssTerrain.h"

#include "abyss/world/Grid.h"

#include "Framework/AbyssUnits.h"
#include "World/AbyssWorldTypes.h"

namespace AbyssTerrainPrivate
{
	constexpr uint8 TileGrass = 0;
	constexpr uint8 TileWater = 3;
	constexpr uint8 TileWall = 4;
	constexpr uint8 TileCampWall = 6;

	float SmoothStep01(float T)
	{
		const float X = FMath::Clamp(T, 0.f, 1.f);
		return X * X * (3.f - 2.f * X);
	}

	/** Lattice value noise in [0, 1] (deterministic, from the web tile hash). */
	float ValueNoise(double X, double Y, int32 Salt)
	{
		const int32 X0 = FMath::FloorToInt32(X);
		const int32 Y0 = FMath::FloorToInt32(Y);
		const float Fx = SmoothStep01(static_cast<float>(X - X0));
		const float Fy = SmoothStep01(static_cast<float>(Y - Y0));
		auto Lattice = [Salt](int32 Lx, int32 Ly)
		{
			return static_cast<float>(AbyssWorldUtil::TileHash(Lx, Ly, Salt) & 0xFFFF) / 65535.f;
		};
		const float A = FMath::Lerp(Lattice(X0, Y0), Lattice(X0 + 1, Y0), Fx);
		const float B = FMath::Lerp(Lattice(X0, Y0 + 1), Lattice(X0 + 1, Y0 + 1), Fx);
		return FMath::Lerp(A, B, Fy);
	}
}

void FAbyssTerrainMesh::Reset()
{
	Vertices.Reset();
	Triangles.Reset();
	Normals.Reset();
	UV0.Reset();
	UV1.Reset();
	Colors.Reset();
}

void FAbyssTerrainField::Reset()
{
	Cols = 0;
	Rows = 0;
	Tiles.Reset();
	Walkable.Reset();
	Heights.Reset();
	VertexColors.Reset();
	VertsX = 0;
	VertsY = 0;
	NumChunksX = 0;
	NumChunksY = 0;
	MinHeight = 0.f;
	MaxHeight = 0.f;
}

void FAbyssTerrainField::Build(const abyss::ZoneGrid& Grid, const FAbyssTerrainSettings& InSettings)
{
	Reset();
	Settings = InSettings;
	Settings.Subdivisions = FMath::Clamp(Settings.Subdivisions, 1, 4);
	Settings.SkirtTiles = FMath::Max(0, Settings.SkirtTiles);
	Settings.ChunkTiles = FMath::Max(4, Settings.ChunkTiles);
	Cols = Grid.Cols();
	Rows = Grid.Rows();
	if (Cols <= 0 || Rows <= 0)
	{
		Reset();
		return;
	}

	Tiles.SetNumUninitialized(Cols * Rows);
	Walkable.SetNumUninitialized(Cols * Rows);
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		for (int32 Col = 0; Col < Cols; ++Col)
		{
			Tiles[Row * Cols + Col] = static_cast<uint8>(Grid.Tile(Col, Row));
			Walkable[Row * Cols + Col] = Grid.Walkable(Col, Row) ? 1 : 0;
		}
	}

	const int32 ExtTilesX = Cols + 2 * Settings.SkirtTiles;
	const int32 ExtTilesY = Rows + 2 * Settings.SkirtTiles;
	Step = 1.0 / static_cast<double>(Settings.Subdivisions);
	OriginX = -0.5 - Settings.SkirtTiles;
	OriginY = -0.5 - Settings.SkirtTiles;
	VertsX = ExtTilesX * Settings.Subdivisions + 1;
	VertsY = ExtTilesY * Settings.Subdivisions + 1;
	NumChunksX = FMath::DivideAndRoundUp(ExtTilesX, Settings.ChunkTiles);
	NumChunksY = FMath::DivideAndRoundUp(ExtTilesY, Settings.ChunkTiles);

	Heights.SetNumUninitialized(VertsX * VertsY);
	VertexColors.SetNumUninitialized(VertsX * VertsY);
	MinHeight = TNumericLimits<float>::Max();
	MaxHeight = TNumericLimits<float>::Lowest();
	for (int32 J = 0; J < VertsY; ++J)
	{
		for (int32 I = 0; I < VertsX; ++I)
		{
			const FVector2D P = VertexTilePos(I, J);
			float Water = 0.f;
			float Wall = 0.f;
			float Skirt = 0.f;
			const float H = ComputeHeight(P.X, P.Y, Water, Wall, Skirt);
			Heights[VertexIndex(I, J)] = H;
			VertexColors[VertexIndex(I, J)] = FLinearColor(Water, Wall, Skirt, 1.f);
			MinHeight = FMath::Min(MinHeight, H);
			MaxHeight = FMath::Max(MaxHeight, H);
		}
	}
}

FVector2D FAbyssTerrainField::VertexTilePos(int32 I, int32 J) const
{
	return FVector2D(OriginX + I * Step, OriginY + J * Step);
}

uint8 FAbyssTerrainField::TileAt(int32 Col, int32 Row) const
{
	if (Cols <= 0 || Rows <= 0)
	{
		return AbyssTerrainPrivate::TileGrass;
	}
	return Tiles[FMath::Clamp(Row, 0, Rows - 1) * Cols + FMath::Clamp(Col, 0, Cols - 1)];
}

bool FAbyssTerrainField::IsWaterTile(int32 Col, int32 Row) const
{
	return InMap(Col, Row) && Tiles[Row * Cols + Col] == AbyssTerrainPrivate::TileWater;
}

float FAbyssTerrainField::ComputeWaterDistance(double X, double Y) const
{
	// Distance (tiles) from a point inside a water tile to the nearest non-water tile square; 0 on land.
	const int32 C = FMath::FloorToInt32(X + 0.5);
	const int32 R = FMath::FloorToInt32(Y + 0.5);
	if (!IsWaterTile(C, R))
	{
		return 0.f;
	}
	double Best = 2.0;
	for (int32 Dr = -1; Dr <= 1; ++Dr)
	{
		for (int32 Dc = -1; Dc <= 1; ++Dc)
		{
			const int32 Nc = C + Dc;
			const int32 Nr = R + Dr;
			if (IsWaterTile(Nc, Nr))
			{
				continue;
			}
			const double Dx = FMath::Max(0.0, FMath::Abs(X - Nc) - 0.5);
			const double Dy = FMath::Max(0.0, FMath::Abs(Y - Nr) - 0.5);
			Best = FMath::Min(Best, FMath::Sqrt(Dx * Dx + Dy * Dy));
		}
	}
	return static_cast<float>(Best);
}

float FAbyssTerrainField::ComputeHeight(double X, double Y, float& OutWaterDepth01, float& OutWall01, float& OutSkirt01) const
{
	using namespace AbyssTerrainPrivate;
	// Water basin (inside the map only).
	const float WaterDistance = ComputeWaterDistance(X, Y);
	OutWaterDepth01 = Settings.BankWidthTiles > 0.f ? FMath::Clamp(WaterDistance / Settings.BankWidthTiles, 0.f, 1.f)
													: (WaterDistance > 0.f ? 1.f : 0.f);
	float Height = Settings.WaterBedCm * SmoothStep01(OutWaterDepth01);

	// Wall coverage: bilinear indicator over the four surrounding tile centres.
	{
		const int32 C0 = FMath::FloorToInt32(X);
		const int32 R0 = FMath::FloorToInt32(Y);
		const float Fx = static_cast<float>(X - C0);
		const float Fy = static_cast<float>(Y - R0);
		auto IsWall = [this](int32 C, int32 R)
		{
			const uint8 T = TileAt(C, R);
			return (T == TileWall || T == TileCampWall) ? 1.f : 0.f;
		};
		OutWall01 = FMath::Lerp(FMath::Lerp(IsWall(C0, R0), IsWall(C0 + 1, R0), Fx),
			FMath::Lerp(IsWall(C0, R0 + 1), IsWall(C0 + 1, R0 + 1), Fx), Fy);
	}

	// Skirt hills outside the map rectangle.
	const double Dx = FMath::Max3(0.0, -0.5 - X, X - (Cols - 0.5));
	const double Dy = FMath::Max3(0.0, -0.5 - Y, Y - (Rows - 0.5));
	const double Outside = FMath::Sqrt(Dx * Dx + Dy * Dy);
	OutSkirt01 = Settings.SkirtRampTiles > 0.f ? SmoothStep01(static_cast<float>(Outside / Settings.SkirtRampTiles)) : 0.f;
	if (Outside > 0.0)
	{
		const float Broad = ValueNoise(X / 6.0, Y / 6.0, 11);
		const float Fine = ValueNoise(X / 2.5, Y / 2.5, 23);
		Height += Settings.SkirtHillCm * OutSkirt01 * (0.3f + 0.55f * Broad + 0.15f * Fine);
	}
	return Height;
}

float FAbyssTerrainField::GetHeight(double WorldX, double WorldY) const
{
	if (!IsValid())
	{
		return 0.f;
	}
	const double Fi = (WorldX / AbyssUnits::TileUU - OriginX) / Step;
	const double Fj = (WorldY / AbyssUnits::TileUU - OriginY) / Step;
	const double Ci = FMath::Clamp(Fi, 0.0, static_cast<double>(VertsX - 1));
	const double Cj = FMath::Clamp(Fj, 0.0, static_cast<double>(VertsY - 1));
	const int32 I0 = FMath::Min(FMath::FloorToInt32(Ci), VertsX - 2);
	const int32 J0 = FMath::Min(FMath::FloorToInt32(Cj), VertsY - 2);
	if (I0 < 0 || J0 < 0)
	{
		return Heights[0];
	}
	const float Tx = static_cast<float>(Ci - I0);
	const float Ty = static_cast<float>(Cj - J0);
	const float H00 = Heights[VertexIndex(I0, J0)];
	const float H10 = Heights[VertexIndex(I0 + 1, J0)];
	const float H01 = Heights[VertexIndex(I0, J0 + 1)];
	const float H11 = Heights[VertexIndex(I0 + 1, J0 + 1)];
	return FMath::Lerp(FMath::Lerp(H00, H10, Tx), FMath::Lerp(H01, H11, Tx), Ty);
}

bool FAbyssTerrainField::Raycast(const FVector& Origin, const FVector& Dir, double MaxDistance, FVector& OutHit) const
{
	const FVector D = Dir.GetSafeNormal();
	if (D.IsNearlyZero())
	{
		return false;
	}
	if (!IsValid())
	{
		if (D.Z > -UE_KINDA_SMALL_NUMBER)
		{
			return false;
		}
		const double T = -Origin.Z / D.Z;
		if (T < 0.0)
		{
			return false;
		}
		OutHit = Origin + D * T;
		return true;
	}
	// Restrict the march to the slab of possible heights.
	const double Top = MaxHeight + 1.0;
	const double Bottom = MinHeight - 1.0;
	double TStart = 0.0;
	double TEnd = MaxDistance;
	if (FMath::Abs(D.Z) > UE_KINDA_SMALL_NUMBER)
	{
		const double TTop = (Top - Origin.Z) / D.Z;
		const double TBottom = (Bottom - Origin.Z) / D.Z;
		TStart = FMath::Max(TStart, FMath::Min(TTop, TBottom));
		TEnd = FMath::Min(TEnd, FMath::Max(TTop, TBottom));
	}
	else if (Origin.Z > Top || Origin.Z < Bottom)
	{
		return false;
	}
	if (TEnd < TStart)
	{
		return false;
	}
	const double StepLen = FMath::Max(5.0, Step * AbyssUnits::TileUU * 0.5);
	auto Above = [this, &Origin, &D](double T)
	{
		const FVector P = Origin + D * T;
		return P.Z - GetHeight(P.X, P.Y);
	};
	double PrevT = TStart;
	if (Above(PrevT) <= 0.0)
	{
		OutHit = Origin + D * PrevT;
		return true;
	}
	for (double T = TStart + StepLen; T <= TEnd + StepLen; T += StepLen)
	{
		const double ClampedT = FMath::Min(T, TEnd);
		const double F = Above(ClampedT);
		if (F <= 0.0)
		{
			double Lo = PrevT;
			double Hi = ClampedT;
			for (int32 Iteration = 0; Iteration < 12; ++Iteration)
			{
				const double Mid = 0.5 * (Lo + Hi);
				if (Above(Mid) > 0.0)
				{
					Lo = Mid;
				}
				else
				{
					Hi = Mid;
				}
			}
			OutHit = Origin + D * Hi;
			OutHit.Z = GetHeight(OutHit.X, OutHit.Y);
			return true;
		}
		PrevT = ClampedT;
		if (ClampedT >= TEnd)
		{
			break;
		}
	}
	return false;
}

FVector FAbyssTerrainField::VertexNormal(int32 I, int32 J) const
{
	const int32 Il = FMath::Max(I - 1, 0);
	const int32 Ir = FMath::Min(I + 1, VertsX - 1);
	const int32 Jd = FMath::Max(J - 1, 0);
	const int32 Ju = FMath::Min(J + 1, VertsY - 1);
	const double SpanX = (Ir - Il) * Step * AbyssUnits::TileUU;
	const double SpanY = (Ju - Jd) * Step * AbyssUnits::TileUU;
	const double DhDx = SpanX > 0.0 ? (Heights[VertexIndex(Ir, J)] - Heights[VertexIndex(Il, J)]) / SpanX : 0.0;
	const double DhDy = SpanY > 0.0 ? (Heights[VertexIndex(I, Ju)] - Heights[VertexIndex(I, Jd)]) / SpanY : 0.0;
	return FVector(-DhDx, -DhDy, 1.0).GetSafeNormal();
}

void FAbyssTerrainField::BuildChunkMeshes(int32 ChunkX, int32 ChunkY, FAbyssTerrainMesh& OutGround, FAbyssTerrainMesh& OutWater) const
{
	OutGround.Reset();
	OutWater.Reset();
	if (!IsValid())
	{
		return;
	}
	const int32 Sub = Settings.Subdivisions;
	const int32 I0 = ChunkX * Settings.ChunkTiles * Sub;
	const int32 J0 = ChunkY * Settings.ChunkTiles * Sub;
	const int32 I1 = FMath::Min(I0 + Settings.ChunkTiles * Sub, VertsX - 1);
	const int32 J1 = FMath::Min(J0 + Settings.ChunkTiles * Sub, VertsY - 1);
	if (I1 <= I0 || J1 <= J0)
	{
		return;
	}

	// ---- ground ----
	const int32 W = I1 - I0 + 1;
	const int32 H = J1 - J0 + 1;
	OutGround.Vertices.Reserve(W * H);
	OutGround.Normals.Reserve(W * H);
	OutGround.UV0.Reserve(W * H);
	OutGround.UV1.Reserve(W * H);
	OutGround.Colors.Reserve(W * H);
	for (int32 J = J0; J <= J1; ++J)
	{
		for (int32 I = I0; I <= I1; ++I)
		{
			const FVector2D P = VertexTilePos(I, J);
			OutGround.Vertices.Add(FVector(P.X * AbyssUnits::TileUU, P.Y * AbyssUnits::TileUU, Heights[VertexIndex(I, J)]));
			OutGround.Normals.Add(VertexNormal(I, J));
			OutGround.UV0.Add(P);
			OutGround.UV1.Add(P / 3.0);
			OutGround.Colors.Add(VertexColors[VertexIndex(I, J)]);
		}
	}
	OutGround.Triangles.Reserve((W - 1) * (H - 1) * 6);
	for (int32 J = 0; J < H - 1; ++J)
	{
		for (int32 I = 0; I < W - 1; ++I)
		{
			const int32 V00 = J * W + I;
			const int32 V10 = V00 + 1;
			const int32 V01 = V00 + W;
			const int32 V11 = V01 + 1;
			// Front face = (V1 - V0) ^ (V2 - V0) pointing up (+Z): +X then +Y.
			OutGround.Triangles.Append({ V00, V10, V01, V10, V11, V01 });
		}
	}

	// ---- water surface: every water tile whose centre lies in this chunk ----
	const int32 Skirt = Settings.SkirtTiles;
	const int32 TileC0 = ChunkX * Settings.ChunkTiles - Skirt;
	const int32 TileR0 = ChunkY * Settings.ChunkTiles - Skirt;
	const int32 TileC1 = TileC0 + Settings.ChunkTiles;
	const int32 TileR1 = TileR0 + Settings.ChunkTiles;
	for (int32 Row = FMath::Max(TileR0, 0); Row < FMath::Min(TileR1, Rows); ++Row)
	{
		for (int32 Col = FMath::Max(TileC0, 0); Col < FMath::Min(TileC1, Cols); ++Col)
		{
			if (!IsWaterTile(Col, Row))
			{
				continue;
			}
			const int32 Base = OutWater.Vertices.Num();
			for (int32 Sj = 0; Sj <= Sub; ++Sj)
			{
				for (int32 Si = 0; Si <= Sub; ++Si)
				{
					const double X = Col - 0.5 + Si * Step;
					const double Y = Row - 0.5 + Sj * Step;
					const float Distance = ComputeWaterDistance(FMath::Clamp(X, Col - 0.499, Col + 0.499),
						FMath::Clamp(Y, Row - 0.499, Row + 0.499));
					// R = shore factor (foam), G = depth factor (shallow tint).
					const float Shore = 1.f - FMath::Clamp(Distance / 0.6f, 0.f, 1.f);
					const float Depth = Settings.BankWidthTiles > 0.f ? FMath::Clamp(Distance / Settings.BankWidthTiles, 0.f, 1.f) : 1.f;
					OutWater.Vertices.Add(FVector(X * AbyssUnits::TileUU, Y * AbyssUnits::TileUU, Settings.WaterSurfaceCm));
					OutWater.Normals.Add(FVector::UpVector);
					OutWater.UV0.Add(FVector2D(X, Y));
					OutWater.UV1.Add(FVector2D(X, Y) / 3.0);
					OutWater.Colors.Add(FLinearColor(Shore, Depth, 0.f, 1.f));
				}
			}
			const int32 Stride = Sub + 1;
			for (int32 Sj = 0; Sj < Sub; ++Sj)
			{
				for (int32 Si = 0; Si < Sub; ++Si)
				{
					const int32 V00 = Base + Sj * Stride + Si;
					const int32 V10 = V00 + 1;
					const int32 V01 = V00 + Stride;
					const int32 V11 = V01 + 1;
					OutWater.Triangles.Append({ V00, V10, V01, V10, V11, V01 });
				}
			}
		}
	}
}

void FAbyssTerrainField::BuildTileTexture(TArray<uint8>& OutBGRA, uint8 WallPaintTile) const
{
	using namespace AbyssTerrainPrivate;
	constexpr uint8 TileCamp = 5;
	OutBGRA.SetNumZeroed(FMath::Max(0, Cols * Rows * 4));
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		for (int32 Col = 0; Col < Cols; ++Col)
		{
			const int32 Index = (Row * Cols + Col) * 4;
			const uint8 Raw = Tiles[Row * Cols + Col];
			// Walls stand on the zone's dominant ground, camp walls on camp ground (ZoneTerrain.ts:84-102).
			const uint8 Paint = Raw == TileWall ? WallPaintTile : (Raw == TileCampWall ? TileCamp : Raw);
			OutBGRA[Index + 0] = Walkable[Row * Cols + Col] ? 255 : 0;                                   // B
			OutBGRA[Index + 1] = static_cast<uint8>(AbyssWorldUtil::TileHash(Col, Row, 7) % 12u);      // G
			OutBGRA[Index + 2] = Paint;                                                                 // R
			OutBGRA[Index + 3] = Raw;                                                                   // A
		}
	}
}

FBox FAbyssTerrainField::GetWorldBounds() const
{
	if (!IsValid())
	{
		return FBox(ForceInit);
	}
	const FVector2D Min = VertexTilePos(0, 0) * AbyssUnits::TileUU;
	const FVector2D Max = VertexTilePos(VertsX - 1, VertsY - 1) * AbyssUnits::TileUU;
	return FBox(FVector(Min.X, Min.Y, MinHeight), FVector(Max.X, Max.Y, MaxHeight));
}
