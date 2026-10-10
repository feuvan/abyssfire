#include "Input/AbyssInputMath.h"

#include "GenericPlatform/GenericPlatformApplicationMisc.h"
#include "HAL/PlatformApplicationMisc.h"

#if PLATFORM_ANDROID || PLATFORM_IOS
namespace
{
	// Plausible short side of a landscape game viewport, in points / dp (iPhone SE 320 .. iPad Pro 13" 1032).
	constexpr double AbyssInputMath_MinShortSidePoints = 280.0;
	constexpr double AbyssInputMath_MaxShortSidePoints = 1400.0;
	// Size-class fallback: typical landscape short side of a phone / tablet, in points.
	constexpr double AbyssInputMath_PhoneShortSidePoints = 400.0;
	constexpr double AbyssInputMath_TabletShortSidePoints = 820.0;
	constexpr double AbyssInputMath_PhoneMinAspect = 1.7;
}
#endif

namespace AbyssInputMath
{
	abyss::Vec2 ScreenInputToTileDir(const FVector2D& InputRightUp, double CameraYawDegrees)
	{
		if (InputRightUp.IsNearlyZero(1.0e-4))
		{
			return abyss::Vec2();
		}
		const double YawRadians = FMath::DegreesToRadians(CameraYawDegrees);
		const double SinYaw = FMath::Sin(YawRadians);
		const double CosYaw = FMath::Cos(YawRadians);
		// Ground-projected camera axes: forward (screen up) = (cos, sin), right (screen right) = (-sin, cos) - the right
		// vector of FRotator(0, Yaw, 0) in UE's left-handed frame.
		const double WorldX = -SinYaw * InputRightUp.X + CosYaw * InputRightUp.Y;
		const double WorldY = CosYaw * InputRightUp.X + SinYaw * InputRightUp.Y;
		return abyss::Vec2(WorldX, WorldY).Normalized();
	}

	abyss::Vec2 TileDirToCoreScreen(const abyss::Vec2& TileDir)
	{
		// ScreenDirToTile(s) = (sx + sy, -sx + sy)  =>  sx = (tx - ty) / 2, sy = (tx + ty) / 2.
		return abyss::Vec2((TileDir.x - TileDir.y) * 0.5, (TileDir.x + TileDir.y) * 0.5).Normalized();
	}

	abyss::Vec2 ScreenInputToCoreScreen(const FVector2D& InputRightUp, double CameraYawDegrees)
	{
		const abyss::Vec2 TileDir = ScreenInputToTileDir(InputRightUp, CameraYawDegrees);
		return TileDir.LengthSq() > 0.0 ? TileDirToCoreScreen(TileDir) : abyss::Vec2();
	}

	abyss::Vec2 ClampTileToZone(const abyss::Vec2& Tile, int32 Cols, int32 Rows)
	{
		if (Cols <= 0 || Rows <= 0)
		{
			return Tile;
		}
		return abyss::Vec2(FMath::Clamp(Tile.x, 0.0, static_cast<double>(Cols - 1)),
			FMath::Clamp(Tile.y, 0.0, static_cast<double>(Rows - 1)));
	}

	float ComputeTouchUnitScale(float DevicePixelRatio, float DpiScale, float MinK, float MaxK)
	{
		const float SafeDpi = DpiScale > KINDA_SMALL_NUMBER ? DpiScale : 1.0f;
		const float SafeDpr = DevicePixelRatio > KINDA_SMALL_NUMBER ? DevicePixelRatio : 1.0f;
		const float Low = FMath::Min(MinK, MaxK);
		const float High = FMath::Max(MinK, MaxK);
		return FMath::Clamp(SafeDpr / SafeDpi, Low, High);
	}

	float EstimateDevicePixelRatio(const FVector2D& ViewportSizePx)
	{
#if PLATFORM_ANDROID || PLATFORM_IOS
		const double ShortSide = FMath::Min(ViewportSizePx.X, ViewportSizePx.Y);
		const double LongSide = FMath::Max(ViewportSizePx.X, ViewportSizePx.Y);
		if (!(ShortSide > 0.0))
		{
			return 1.0f;
		}
		int32 Density = 0;
		const EScreenPhysicalAccuracy Accuracy = FPlatformApplicationMisc::GetPhysicalScreenDensity(Density);
#if PLATFORM_IOS
		const double PointsPerInch = 163.0;
		const bool bTrustDensity = Accuracy == EScreenPhysicalAccuracy::Approximation;
#else
		const double PointsPerInch = 160.0;
		const bool bTrustDensity = Accuracy != EScreenPhysicalAccuracy::Unknown;
#endif
		if (bTrustDensity && Density > 0)
		{
			const double Ratio = static_cast<double>(Density) / PointsPerInch;
			const double ShortSidePoints = ShortSide / Ratio;
			if (ShortSidePoints >= AbyssInputMath_MinShortSidePoints && ShortSidePoints <= AbyssInputMath_MaxShortSidePoints)
			{
				return static_cast<float>(FMath::Max(Ratio, 1.0));
			}
		}
		const bool bPhone = LongSide / ShortSide >= AbyssInputMath_PhoneMinAspect;
		const double ReferencePoints = bPhone ? AbyssInputMath_PhoneShortSidePoints : AbyssInputMath_TabletShortSidePoints;
		return static_cast<float>(FMath::Max(ShortSide / ReferencePoints, 1.0));
#else
		return FMath::Max(1.0f, FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(0.0f, 0.0f));
#endif
	}

	float PinchRatioToZoomSteps(float Ratio)
	{
		if (!(Ratio > KINDA_SMALL_NUMBER))
		{
			return 0.0f;
		}
		static const float StepLog = FMath::Loge(1.1f);
		return FMath::Loge(Ratio) / StepLog;
	}

	FString FormatCooldown(double RemainingMs)
	{
		if (!(RemainingMs > 0.0))
		{
			return FString();
		}
		if (RemainingMs >= 1000.0)
		{
			return FString::FromInt(FMath::CeilToInt(RemainingMs / 1000.0));
		}
		return FString::Printf(TEXT("%.1f"), RemainingMs / 1000.0);
	}
}
