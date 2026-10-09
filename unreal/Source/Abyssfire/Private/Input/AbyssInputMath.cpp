#include "Input/AbyssInputMath.h"

#include "GenericPlatform/GenericPlatformApplicationMisc.h"
#include "HAL/PlatformApplicationMisc.h"

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
		// Ground-projected camera axes (UE: yaw rotates +X toward +Y; the right vector of FRotator(0, Yaw, 0) is
		// (-sin, cos)).
		const double WorldX = -SinYaw * InputRightUp.X + CosYaw * InputRightUp.Y;
		const double WorldY = CosYaw * InputRightUp.X + SinYaw * InputRightUp.Y;
		return abyss::Vec2(WorldX, WorldY).Normalized();
	}

	abyss::Vec2 TileDirToCoreScreen(const abyss::Vec2& TileDir)
	{
		// ScreenDirToTile(s) = (sx + sy, -sx + sy)  =>  sx = (tx - ty) / 2, sy = (tx + ty) / 2.
		return abyss::Vec2((TileDir.x - TileDir.y) * 0.5, (TileDir.x + TileDir.y) * 0.5).Normalized();
	}

	float ComputeTouchUnitScale(float DevicePixelRatio, float DpiScale, float MinK, float MaxK)
	{
		const float SafeDpi = DpiScale > KINDA_SMALL_NUMBER ? DpiScale : 1.0f;
		const float SafeDpr = DevicePixelRatio > KINDA_SMALL_NUMBER ? DevicePixelRatio : 1.0f;
		const float Low = FMath::Min(MinK, MaxK);
		const float High = FMath::Max(MinK, MaxK);
		return FMath::Clamp(SafeDpr / SafeDpi, Low, High);
	}

	float GetDevicePixelRatio()
	{
		static float CachedRatio = -1.0f;
		if (CachedRatio > 0.0f)
		{
			return CachedRatio;
		}
		float Ratio = 1.0f;
#if PLATFORM_ANDROID || PLATFORM_IOS
		int32 Density = 0;
		const EScreenPhysicalAccuracy Accuracy = FPlatformApplicationMisc::GetPhysicalScreenDensity(Density);
		if (Accuracy != EScreenPhysicalAccuracy::Unknown && Density > 0)
		{
#if PLATFORM_IOS
			Ratio = static_cast<float>(Density) / 163.0f;   // ppi per UIKit point
#else
			Ratio = static_cast<float>(Density) / 160.0f;   // Android dp
#endif
		}
#endif
		CachedRatio = FMath::Max(Ratio, 1.0f);
		return CachedRatio;
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
