// FAbyssTerrainField: the render-only height field of a zone and its generated ground / water meshes
// (world-map-nav.md 1.3, 15.1; art-inventory-ch1.md 6.1, 6.10).
//
// * Walkable ground is flat at Z = 0 (no gameplay depends on height). Water tiles are basins: the bank slopes from the
//   tile edge down to the bed (-60 cm) over the outer 0.3 tile; the translucent surface sits at -12 cm. Outside the map a
//   skirt of rolling hills (SkirtTiles wide) keeps the fixed camera from ever seeing the void.
// * Vertex grid: Subdivisions vertices per tile, covering [-0.5 - Skirt, Cols - 0.5 + Skirt] in tile space.
// * Vertex colours (M_AF_Terrain contract, WorldContract.md): R = water depth 0..1, G = wall-tile coverage 0..1,
//   B = skirt factor 0 (map) .. 1 (4+ tiles outside), A = 1. UV0 = tile-space position (col, row); UV1 = UV0 / 3 (the
//   web's 3x3-tile pattern period). The tile types come from the tile-id texture (BuildTileTexture).
// * Pure data (no UObjects): the zone actor turns the meshes into ProceduralMeshComponent sections.
#pragma once

#include "CoreMinimal.h"

namespace abyss
{
	class ZoneGrid;
}

struct FAbyssTerrainSettings
{
	int32 SkirtTiles = 18;
	int32 Subdivisions = 2;
	int32 ChunkTiles = 32;
	float WaterBedCm = -60.f;
	float WaterSurfaceCm = -12.f;
	float BankWidthTiles = 0.3f;
	float SkirtHillCm = 180.f;
	float SkirtRampTiles = 4.f;
};

struct FAbyssTerrainMesh
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FVector2D> UV1;
	TArray<FLinearColor> Colors;

	void Reset();
	bool IsEmpty() const { return Triangles.Num() == 0; }
};

class ABYSSFIRE_API FAbyssTerrainField
{
public:
	void Build(const abyss::ZoneGrid& Grid, const FAbyssTerrainSettings& InSettings);
	void Reset();
	bool IsValid() const { return Cols > 0 && Rows > 0 && Heights.Num() > 0; }

	int32 GetCols() const { return Cols; }
	int32 GetRows() const { return Rows; }
	const FAbyssTerrainSettings& GetSettings() const { return Settings; }

	/** Tile type (abyss::TileType value) of a tile; out of bounds = the nearest edge tile. */
	uint8 TileAt(int32 Col, int32 Row) const;
	bool IsWaterTile(int32 Col, int32 Row) const;
	bool InMap(int32 Col, int32 Row) const { return Col >= 0 && Row >= 0 && Col < Cols && Row < Rows; }

	/** Ground height (uu) at a UE world XY: bilinear on the vertex grid (exactly 0 on walkable ground). */
	float GetHeight(double WorldX, double WorldY) const;
	/** Ray / height-field intersection (input picking, ue58-platform.md 8.4). Dir need not be normalised. */
	bool Raycast(const FVector& Origin, const FVector& Dir, double MaxDistance, FVector& OutHit) const;

	// ---- meshes ----
	int32 GetNumChunksX() const { return NumChunksX; }
	int32 GetNumChunksY() const { return NumChunksY; }
	/** Ground (and water surface) mesh of one chunk, world space (uu). */
	void BuildChunkMeshes(int32 ChunkX, int32 ChunkY, FAbyssTerrainMesh& OutGround, FAbyssTerrainMesh& OutWater) const;
	/**
	 * BGRA8 bytes, Cols x Rows (M_AF_Terrain contract, WorldContract.md 3.1): R = paint material index (the tile type,
	 * except walls -> WallPaintTile, the zone's dominant ground, and camp walls -> camp ground 5), G = detail variant
	 * (tileHash(c, r, 7) % 12), B = 255 if walkable, A = the raw tile type.
	 */
	void BuildTileTexture(TArray<uint8>& OutBGRA, uint8 WallPaintTile) const;
	FBox GetWorldBounds() const;

private:
	int32 VertexIndex(int32 I, int32 J) const { return J * VertsX + I; }
	FVector2D VertexTilePos(int32 I, int32 J) const;
	float ComputeWaterDistance(double X, double Y) const;
	float ComputeHeight(double X, double Y, float& OutWaterDepth01, float& OutWall01, float& OutSkirt01) const;
	FVector VertexNormal(int32 I, int32 J) const;

	FAbyssTerrainSettings Settings;
	int32 Cols = 0;
	int32 Rows = 0;
	TArray<uint8> Tiles;      // row-major tile types
	TArray<uint8> Walkable;   // row-major
	int32 VertsX = 0;
	int32 VertsY = 0;
	double OriginX = 0.0;     // tile-space position of vertex (0, 0)
	double OriginY = 0.0;
	double Step = 0.5;        // tiles per vertex step
	TArray<float> Heights;    // uu
	TArray<FLinearColor> VertexColors;
	float MinHeight = 0.f;
	float MaxHeight = 0.f;
	int32 NumChunksX = 0;
	int32 NumChunksY = 0;
};
