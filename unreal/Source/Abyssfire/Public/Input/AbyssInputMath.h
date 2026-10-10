// Small pure helpers of the input layer (no UObjects): camera-relative direction mapping, touch unit scale, pinch zoom,
// cooldown text. Unit-testable outside UE.
#pragma once

#include "CoreMinimal.h"

#include "abyss/base/Types.h"

namespace AbyssInputMath
{
	/**
	 * Fallback camera yaw (degrees) for the few frames before the player camera manager has a view (the world agent's
	 * AAbyssCameraRig default, DECISIONS W1). Every direction mapping below takes the LIVE camera yaw, so movement stays
	 * screen-relative whatever yaw the rig uses (world-map-nav.md 1.3 proposed -135, the rig ships 45).
	 */
	inline constexpr double DefaultCameraYawDegrees = 45.0;

	/**
	 * Screen-space input (UE axis convention: X = screen right, Y = screen up) -> unit tile-space direction on the ground,
	 * for the actual camera yaw (ground forward = (cos, sin), right = (-sin, cos); tile col = UE X, row = UE Y, S4).
	 * Zero input -> zero. Magnitude is dropped (save-ui-input Q15: any deflection = full speed).
	 */
	ABYSSFIRE_API abyss::Vec2 ScreenInputToTileDir(const FVector2D& InputRightUp, double CameraYawDegrees);

	/**
	 * Inverse of abyss::ScreenDirToTile ((sx + sy, -sx + sy)): the unit "core screen" vector (x right, y down) the core maps
	 * back to TileDir. Commands whose directions always go through ScreenDirToTile in the core (CmdDodge::dir,
	 * CmdCastSkill::stickDir) get this, so they stay correct whatever the real camera yaw is.
	 */
	ABYSSFIRE_API abyss::Vec2 TileDirToCoreScreen(const abyss::Vec2& TileDir);

	/** ScreenInputToTileDir followed by TileDirToCoreScreen (zero stays zero). */
	ABYSSFIRE_API abyss::Vec2 ScreenInputToCoreScreen(const FVector2D& InputRightUp, double CameraYawDegrees);

	/** Clamps a tile-space point into the zone ([0, Cols - 1] x [0, Rows - 1]); unchanged when the zone size is unknown. */
	ABYSSFIRE_API abyss::Vec2 ClampTileToZone(const abyss::Vec2& Tile, int32 Cols, int32 Rows);

	/**
	 * Touch unit scale k (save-ui-input.md 5.7.1, ue58-platform.md 9.5): game px per CSS px = clamp(DPR / DpiScale, MinK,
	 * MaxK), DpiScale = viewport px per Slate unit.
	 */
	ABYSSFIRE_API float ComputeTouchUnitScale(float DevicePixelRatio, float DpiScale, float MinK, float MaxK);

	/**
	 * Render pixels per CSS px / point / dp of the game viewport (the DPR of ue58-platform.md 9.5).
	 * * Desktop: the OS DPI scale (Mac backing scale, Windows monitor scale).
	 * * Android: FPlatformApplicationMisc::GetPhysicalScreenDensity / 160 (dp).
	 * * iOS: density / 163 only when the engine derived it from the view's content scale (Approximation); a device-profile
	 *   value (ios.PhysicalScreenDensity) is divided by the content scale by the engine and no longer measures render
	 *   pixels, so it is not used.
	 * * Both mobile platforms sanity-check the result (the viewport's short side must be 280..1400 points) and otherwise
	 *   estimate from the size class: short side / 400 points on phones (aspect >= 1.7), / 820 on tablets.
	 */
	ABYSSFIRE_API float EstimateDevicePixelRatio(const FVector2D& ViewportSizePx);

	/** Pinch distance ratio (current / previous) -> zoom steps: a 10 % spread is one wheel notch (+1, zoom in). */
	ABYSSFIRE_API float PinchRatioToZoomSteps(float Ratio);

	/** Cooldown label (save-ui-input 5.7.3): >= 1000 ms -> ceil(seconds), else seconds with one decimal; "" when ready. */
	ABYSSFIRE_API FString FormatCooldown(double RemainingMs);
}
