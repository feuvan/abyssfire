#include "Camera/AbyssCameraRig.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

#include "abyss/data/MapData.h"

#include "World/AbyssArtManifest.h"
#include "World/AbyssWorldTypes.h"

static TAutoConsoleVariable<float> CVarAbyssShakeScale(
	TEXT("abyss.ShakeScale"),
	1.0f,
	TEXT("Global camera-shake feel scale (combat-feel.md 11.5): 1 = web parity peak amplitude, 0 = off."),
	ECVF_Default);

namespace AbyssCameraRigPrivate
{
	constexpr float ReferenceAspect = 16.f / 9.f;
	constexpr float ZoomNotchFraction = 0.1f;   // one wheel notch = 10 % of the zoom range
	constexpr float ZoomSmoothingSec = 0.08f;
	constexpr float SoftFollowSec = 0.6f;       // after a story focus the camera glides back
	constexpr float SoftFollowLagSec = 0.35f;
	constexpr float ShakeDecayRate = 4.f;       // exp(-4 t / duration): ~2 % of the peak at the end (C9)
	constexpr double ShakeThrottleSec = 0.1;    // VFXManager: a shake < 100 ms after the last accepted one is ignored
}

AAbyssCameraRig::AAbyssCameraRig()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SceneRoot);
	Camera->bUsePawnControlRotation = false;
	Camera->bConstrainAspectRatio = false;
	Camera->SetFieldOfView(FovHDeg);
	Camera->PostProcessBlendWeight = 0.f;   // the zone's unbound post-process component owns the look
	ShakeRandom.Initialize(0x4A5B1E);
}

void AAbyssCameraRig::Configure(const FAbyssArtShading& Shading, const abyss::WorldConstants* Constants)
{
	YawDeg = Shading.CameraYawDeg;
	PitchDeg = Shading.CameraPitchDeg;
	FovHDeg = Shading.CameraFovHDeg;
	DefaultDistance = Shading.CameraDistanceCm;
	FocusZ = Shading.CameraFocusZCm;
	if (Constants != nullptr)
	{
		// world_constants.json is the gameplay-side copy of W1; the manifest wins for the exact framing it was rendered
		// with, the constants give zoom range and lag.
		ZoomMin = static_cast<float>(Constants->cameraZoomMin);
		ZoomMax = static_cast<float>(Constants->cameraZoomMax);
		FollowLagSec = static_cast<float>(Constants->cameraFollowLagSec);
		if (!Shading.bValid)
		{
			YawDeg = static_cast<float>(Constants->cameraYawDeg);
			PitchDeg = static_cast<float>(Constants->cameraPitchDeg);
			FovHDeg = static_cast<float>(Constants->cameraFovDeg);
			// framing width (tiles) across the horizontal FOV at the focus
			DefaultDistance = static_cast<float>(Constants->cameraFramingTilesW * 100.0 * 0.5
				/ FMath::Tan(FMath::DegreesToRadians(Constants->cameraFovDeg * 0.5)));
		}
	}
	if (ZoomMin > ZoomMax)
	{
		Swap(ZoomMin, ZoomMax);
	}
	ZoomTarget = ZoomCurrent = FMath::Clamp(1.f, ZoomMin, ZoomMax);
	UpdateFov();
}

float AAbyssCameraRig::GetCurrentDistance() const
{
	float Pulse = 1.f;
	if (PulseElapsedSec >= 0.f)
	{
		const float Total = PulseInSec + PulseOutSec;
		if (PulseElapsedSec < PulseInSec && PulseInSec > 0.f)
		{
			Pulse = FMath::Lerp(1.f, PulseScale, AbyssWorldUtil::EaseOutQuad(PulseElapsedSec / PulseInSec));
		}
		else if (PulseElapsedSec < Total && PulseOutSec > 0.f)
		{
			Pulse = FMath::Lerp(PulseScale, 1.f, AbyssWorldUtil::EaseInOutSine((PulseElapsedSec - PulseInSec) / PulseOutSec));
		}
	}
	return DefaultDistance * ZoomCurrent * Pulse;
}

FVector AAbyssCameraRig::GetGroundForward() const
{
	const float Yaw = FMath::DegreesToRadians(YawDeg);
	return FVector(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0);
}

FVector AAbyssCameraRig::GetGroundRight() const
{
	const float Yaw = FMath::DegreesToRadians(YawDeg);
	return FVector(-FMath::Sin(Yaw), FMath::Cos(Yaw), 0.0);
}

FVector2D AAbyssCameraRig::ScreenToGroundDirection(const FVector2D& ScreenDir) const
{
	const FVector Ground = GetGroundRight() * ScreenDir.X + GetGroundForward() * ScreenDir.Y;
	const FVector2D Out(Ground.X, Ground.Y);
	return Out.GetSafeNormal() * FMath::Min(1.0, ScreenDir.Size());
}

void AAbyssCameraRig::SnapToFocus(const FVector& HeroFocus)
{
	Focus = HeroFocus;
	bSnapNext = true;
}

void AAbyssCameraRig::AddZoomInput(float Notches)
{
	const float Range = ZoomMax - ZoomMin;
	SetZoomFactor(ZoomTarget - Notches * Range * AbyssCameraRigPrivate::ZoomNotchFraction);
}

void AAbyssCameraRig::SetZoomFactor(float DistanceFactor)
{
	ZoomTarget = FMath::Clamp(DistanceFactor, ZoomMin, ZoomMax);
}

void AAbyssCameraRig::PulseZoom(float Scale, float InSec, float OutSec)
{
	PulseScale = Scale;
	PulseInSec = FMath::Max(0.f, InSec);
	PulseOutSec = FMath::Max(0.f, OutSec);
	PulseElapsedSec = 0.f;
}

void AAbyssCameraRig::StartShake(float DurationMs, float Intensity, EAbyssShakeSource Source)
{
	if (!bShakeEnabled || DurationMs <= 0.f || Intensity <= 0.f)
	{
		return;
	}
	// combat-feel.md 11.5: Phaser drops a new shake while one runs (no override, whatever its strength) ...
	if (ShakeDurationSec > 0.f && ShakeElapsedSec < ShakeDurationSec)
	{
		return;
	}
	// ... and VFXManager drops a shake < 100 ms after its last accepted one. Core shakes passed the core's ShakeThrottle
	// already (sim clock); they still start this window so UE-side VFXManager shakes (legendary drop) respect it.
	if (Source == EAbyssShakeSource::Throttled && RigRealClockSec - LastThrottledShakeSec < AbyssCameraRigPrivate::ShakeThrottleSec)
	{
		return;
	}
	if (Source != EAbyssShakeSource::Direct)
	{
		LastThrottledShakeSec = RigRealClockSec;
	}
	ShakeDurationSec = DurationMs / 1000.f;
	ShakeElapsedSec = 0.f;
	ShakeIntensity = Intensity;
}

void AAbyssCameraRig::StopShake()
{
	ShakeDurationSec = 0.f;
	ShakeElapsedSec = 0.f;
	ShakeIntensity = 0.f;
	ShakeOffset = FVector::ZeroVector;
}

void AAbyssCameraRig::Flash(const FLinearColor& Color, float DurationMs, float Alpha)
{
	if (DurationMs <= 0.f || Alpha <= 0.f)
	{
		return;
	}
	FlashColor = Color;
	FlashAlpha0 = FMath::Clamp(Alpha, 0.f, 1.f);
	FlashDurationSec = DurationMs / 1000.f;
	FlashElapsedSec = 0.f;
}

void AAbyssCameraRig::FadeTo(float TargetAlpha, const FLinearColor& Color, float DurationSec)
{
	BaseFadeFrom = BaseFadeAlpha;
	BaseFadeTarget = FMath::Clamp(TargetAlpha, 0.f, 1.f);
	BaseFadeColor = Color;
	BaseFadeDurationSec = FMath::Max(0.f, DurationSec);
	BaseFadeElapsedSec = 0.f;
	if (BaseFadeDurationSec <= 0.f)
	{
		BaseFadeAlpha = BaseFadeTarget;
	}
}

void AAbyssCameraRig::ClearFades()
{
	BaseFadeAlpha = BaseFadeFrom = BaseFadeTarget = 0.f;
	BaseFadeDurationSec = BaseFadeElapsedSec = 0.f;
	FlashAlpha0 = FlashDurationSec = FlashElapsedSec = 0.f;
	if (bFadeApplied)
	{
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			if (PC->PlayerCameraManager != nullptr)
			{
				PC->PlayerCameraManager->StopCameraFade();
			}
		}
		bFadeApplied = false;
	}
}

void AAbyssCameraRig::BeginStoryFocus(const FVector& Target, float DurationSec)
{
	bStoryFocus = true;
	StoryFrom = Focus;
	StoryTarget = Target;
	StoryDurationSec = FMath::Max(0.f, DurationSec);
	StoryElapsedSec = 0.f;
}

void AAbyssCameraRig::UpdateStoryFocusTarget(const FVector& Target)
{
	StoryTarget = Target;
}

void AAbyssCameraRig::EndStoryFocus()
{
	if (bStoryFocus)
	{
		bStoryFocus = false;
		SoftFollowRemainingSec = AbyssCameraRigPrivate::SoftFollowSec;
	}
}

void AAbyssCameraRig::ApplyViewTarget()
{
	UWorld* GameWorld = GetWorld();
	APlayerController* PC = GameWorld ? GameWorld->GetFirstPlayerController() : nullptr;
	if (PC != nullptr && PC->GetViewTarget() != this)
	{
		PC->SetViewTarget(this);
	}
}

void AAbyssCameraRig::UpdateFov()
{
	float Aspect = AbyssCameraRigPrivate::ReferenceAspect;
	if (const UWorld* GameWorld = GetWorld())
	{
		if (const UGameViewportClient* Viewport = GameWorld->GetGameViewport())
		{
			FVector2D Size;
			Viewport->GetViewportSize(Size);
			if (Size.X > 1.0 && Size.Y > 1.0)
			{
				Aspect = static_cast<float>(Size.X / Size.Y);
			}
		}
	}
	// W1 framing is defined on 16:9: wider screens keep the vertical extent (more ground left / right), narrower ones
	// (4:3 iPads) keep the horizontal extent (more ground up / down).
	const float TanHalfH = FMath::Tan(FMath::DegreesToRadians(FovHDeg * 0.5f));
	const float TanHalfVRef = TanHalfH / AbyssCameraRigPrivate::ReferenceAspect;
	float TanHalf = TanHalfH;
	if (Aspect > AbyssCameraRigPrivate::ReferenceAspect)
	{
		TanHalf = TanHalfVRef * Aspect;
	}
	CurrentFovH = FMath::RadiansToDegrees(2.f * FMath::Atan(TanHalf));
	TanHalfFovY = TanHalf / Aspect;
	if (Camera != nullptr && !FMath::IsNearlyEqual(Camera->FieldOfView, CurrentFovH, 0.01f))
	{
		Camera->SetFieldOfView(CurrentFovH);
	}
}

void AAbyssCameraRig::UpdateFades(float RealDeltaSec)
{
	if (BaseFadeDurationSec > 0.f && BaseFadeElapsedSec < BaseFadeDurationSec)
	{
		BaseFadeElapsedSec += RealDeltaSec;
		const float T = FMath::Clamp(BaseFadeElapsedSec / BaseFadeDurationSec, 0.f, 1.f);
		BaseFadeAlpha = FMath::Lerp(BaseFadeFrom, BaseFadeTarget, T);
	}
	float FlashAlpha = 0.f;
	if (FlashDurationSec > 0.f && FlashElapsedSec < FlashDurationSec)
	{
		FlashElapsedSec += RealDeltaSec;
		FlashAlpha = FlashAlpha0 * (1.f - FMath::Clamp(FlashElapsedSec / FlashDurationSec, 0.f, 1.f));
	}
	const float Combined = 1.f - (1.f - BaseFadeAlpha) * (1.f - FlashAlpha);
	UWorld* GameWorld = GetWorld();
	APlayerController* PC = GameWorld ? GameWorld->GetFirstPlayerController() : nullptr;
	APlayerCameraManager* CameraManager = PC ? PC->PlayerCameraManager.Get() : nullptr;
	if (CameraManager == nullptr)
	{
		return;
	}
	if (Combined <= 0.001f)
	{
		if (bFadeApplied)
		{
			CameraManager->StopCameraFade();
			bFadeApplied = false;
		}
		return;
	}
	// Flash over the base fade: colour weighted by each layer's contribution.
	const float FlashShare = Combined > 0.f ? FlashAlpha / Combined : 0.f;
	const FLinearColor Color = FMath::Lerp(BaseFadeColor, FlashColor, FMath::Clamp(FlashShare, 0.f, 1.f));
	CameraManager->SetManualCameraFade(Combined, Color, /*bInFadeAudio*/ false);
	bFadeApplied = true;
}

void AAbyssCameraRig::UpdateRig(float RealDeltaSec, const FVector& HeroFocus, bool bHasHero)
{
	using namespace AbyssCameraRigPrivate;
	RigRealClockSec += FMath::Max(0.f, RealDeltaSec);
	ApplyViewTarget();
	UpdateFov();

	// ---- focus ----
	if (bStoryFocus)
	{
		StoryElapsedSec += RealDeltaSec;
		const float T = StoryDurationSec > 0.f ? FMath::Clamp(StoryElapsedSec / StoryDurationSec, 0.f, 1.f) : 1.f;
		Focus = FMath::Lerp(StoryFrom, StoryTarget, AbyssWorldUtil::EaseInOutSine(T));
		bSnapNext = false;
	}
	else if (bHasHero)
	{
		if (bSnapNext)
		{
			Focus = HeroFocus;
			bSnapNext = false;
		}
		else
		{
			float Lag = FollowLagSec;
			if (SoftFollowRemainingSec > 0.f)
			{
				SoftFollowRemainingSec -= RealDeltaSec;
				Lag = SoftFollowLagSec;
			}
			Focus = FMath::Lerp(Focus, HeroFocus, AbyssWorldUtil::ExpBlend(RealDeltaSec, Lag));
		}
	}

	// ---- zoom ----
	ZoomCurrent = FMath::Lerp(ZoomCurrent, ZoomTarget, AbyssWorldUtil::ExpBlend(RealDeltaSec, ZoomSmoothingSec));
	if (PulseElapsedSec >= 0.f)
	{
		PulseElapsedSec += RealDeltaSec;
		if (PulseElapsedSec >= PulseInSec + PulseOutSec)
		{
			PulseElapsedSec = -1.f;
		}
	}
	const float Distance = GetCurrentDistance();

	// ---- shake: uniform jitter re-randomised every frame, exponential decay ----
	ShakeOffset = FVector::ZeroVector;
	const float FeelScale = FMath::Max(0.f, CVarAbyssShakeScale.GetValueOnGameThread());
	if (ShakeDurationSec > 0.f && ShakeElapsedSec < ShakeDurationSec)
	{
		// The clock runs even when the offset is suppressed (setting off / feel scale 0) so a stale shake never blocks
		// later ones through the no-override rule.
		ShakeElapsedSec += RealDeltaSec;
	}
	if (ShakeDurationSec > 0.f && bShakeEnabled && FeelScale > 0.f)
	{
		const float Decay = FMath::Exp(-ShakeDecayRate * FMath::Min(ShakeElapsedSec, ShakeDurationSec) / ShakeDurationSec);
		// Web: offset within +-I*W (x) / +-I*H (y) at zoom 1.8 and render scale 1 -> I x 3.24 x the visible extent.
		const float VisibleWidth = 2.f * Distance * FMath::Tan(FMath::DegreesToRadians(CurrentFovH * 0.5f));
		const float VisibleHeight = 2.f * Distance * TanHalfFovY;
		const float AmpX = ShakeIntensity * 3.24f * VisibleWidth * Decay * FeelScale;
		const float AmpY = ShakeIntensity * 3.24f * VisibleHeight * Decay * FeelScale;
		const FRotator ViewRotation(PitchDeg, YawDeg, 0.f);
		const FVector Right = FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Y);
		const FVector Up = FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Z);
		ShakeOffset = Right * (ShakeRandom.FRandRange(-1.f, 1.f) * AmpX) + Up * (ShakeRandom.FRandRange(-1.f, 1.f) * AmpY);
	}
	if (ShakeDurationSec > 0.f && ShakeElapsedSec >= ShakeDurationSec)
	{
		StopShake();
	}

	// ---- place the camera ----
	const FRotator ViewRotation(PitchDeg, YawDeg, 0.f);
	const FVector Forward = ViewRotation.Vector();
	const FVector Pivot(Focus.X, Focus.Y, bHasHero || bStoryFocus ? Focus.Z : FocusZ);
	SetActorLocationAndRotation(Pivot - Forward * Distance + ShakeOffset, ViewRotation);

	UpdateFades(RealDeltaSec);
}
