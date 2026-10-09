// Tile space <-> UE world space (DECISIONS S4, world-map-nav.md 1.3). The core works in tiles on the ground plane;
// UE X = col * 100, Y = row * 100, Z up (from the terrain, owned by the world view). Never duplicate the 100 uu
// constant: it comes from abyss/base/Units.h.
#pragma once

#include "CoreMinimal.h"

#include "abyss/base/Types.h"
#include "abyss/base/Units.h"

namespace AbyssUnits
{
	/** UE units per tile (100 uu = 1 m). */
	inline constexpr double TileUU = abyss::kTileSizeUU;

	/** Tile-space point -> UE world location on the ground plane at height Z. */
	inline FVector TileToWorld(const abyss::Vec2& Tile, double Z = 0.0)
	{
		return FVector(Tile.x * TileUU, Tile.y * TileUU, Z);
	}

	inline FVector TileToWorld(const FVector2D& Tile, double Z = 0.0)
	{
		return FVector(Tile.X * TileUU, Tile.Y * TileUU, Z);
	}

	/** UE world location -> tile space (Z ignored). Input picking: hit point on the ground -> CmdMoveTo / pointer tile. */
	inline abyss::Vec2 WorldToTile(const FVector& World)
	{
		return abyss::Vec2(World.X / TileUU, World.Y / TileUU);
	}

	inline FVector2D ToVector2D(const abyss::Vec2& V)
	{
		return FVector2D(V.x, V.y);
	}

	inline abyss::Vec2 ToVec2(const FVector2D& V)
	{
		return abyss::Vec2(V.X, V.Y);
	}

	/** Interpolated tile position for rendering (S1): Lerp(prevPos, pos, alpha). */
	inline abyss::Vec2 LerpTile(const abyss::Vec2& Prev, const abyss::Vec2& Cur, double Alpha)
	{
		return abyss::Vec2(Prev.x + (Cur.x - Prev.x) * Alpha, Prev.y + (Cur.y - Prev.y) * Alpha);
	}

	/** Tile-space direction (facing / heading) -> UE yaw in degrees (0 = +X = +col, 90 = +Y = +row). */
	inline double FacingToYawDegrees(const abyss::Vec2& Facing)
	{
		if (Facing.x == 0.0 && Facing.y == 0.0)
		{
			return 0.0;
		}
		return FMath::RadiansToDegrees(FMath::Atan2(Facing.y, Facing.x));
	}

	inline FRotator FacingToRotation(const abyss::Vec2& Facing)
	{
		return FRotator(0.0, FacingToYawDegrees(Facing), 0.0);
	}
}
