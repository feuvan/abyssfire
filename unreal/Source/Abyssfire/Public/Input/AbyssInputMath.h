// Small pure helpers of the input layer (no UObjects): camera-relative direction mapping, touch unit scale, pinch zoom,
// cooldown text. Unit-testable outside UE.
#pragma once

#include "CoreMinimal.h"

#include "abyss/base/Types.h"

namespace AbyssInputMath
{
	/**
	 * Camera yaw that reproduces the web's screen orientation (world-map-nav.md 1.3: screen-up = tile (-1,-1), screen-right
	 * = tile (+1,-1)). Used when no game camera is active yet. abyss::ScreenDirToTile assumes exactly this yaw.
	 */
	inline constexpr double DefaultCameraYawDegrees = -135.0;

	/**
	 * Screen-space input (UE axis convention: X = screen right, Y = screen up) -> unit tile-space direction on the ground,
	 * for the actual camera yaw (forward = (cos, sin), right = (-sin, cos); tile col = UE X, row = UE Y, S4). Robust to any
	 * camera yaw the world agent picks (W1). Zero input -> zero.
	 */
	ABYSSFIRE_API abyss::Vec2 ScreenInputToTileDir(const FVector2D& InputRightUp, double CameraYawDegrees);

	/**
	 * Inverse of abyss::ScreenDirToTile ((sx + sy, -sx + sy)): the unit "core screen" vector (x right, y down) the core maps
	 * back to TileDir. Commands whose directions always go through ScreenDirToTile (CmdDodge::dir, CmdCastSkill::stickDir)
	 * get this, so they stay correct whatever the real camera yaw is.
	 */
	ABYSSFIRE_API abyss::Vec2 TileDirToCoreScreen(const abyss::Vec2& TileDir);

	/**
	 * Touch unit scale k (save-ui-input.md 5.7.1, ue58-platform.md 9.5): game px per CSS px = clamp(DPR / DpiScale, MinK,
	 * MaxK), DpiScale = viewport px per Slate unit.
	 */
	ABYSSFIRE_API float ComputeTouchUnitScale(float DevicePixelRatio, float DpiScale, float MinK, float MaxK);

	/**
	 * Device pixel ratio (CSS px -> physical px): Android densityDpi / 160, iOS ppi / 163, desktop and unknown density 1
	 * (FPlatformApplicationMisc::GetPhysicalScreenDensity). Cached after the first call.
	 */
	ABYSSFIRE_API float GetDevicePixelRatio();

	/** Pinch distance ratio (current / previous) -> zoom steps: a 10 % spread is one wheel notch (+1, zoom in). */
	ABYSSFIRE_API float PinchRatioToZoomSteps(float Ratio);

	/** Cooldown label (save-ui-input 5.7.3): >= 1000 ms -> ceil(seconds), else seconds with one decimal; "" when ready. */
	ABYSSFIRE_API FString FormatCooldown(double RemainingMs);
}
