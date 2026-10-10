// AAbyssCameraRig: the fixed-angle oblique camera (DECISIONS W1, world-map-nav.md 8, combat-feel.md 11.5, C9).
//
// * Yaw 45, pitch -50, horizontal FOV 35 at 16:9 (wider screens keep the vertical extent, narrower keep the horizontal
//   one), default distance from the art manifest camera block (25.37 m = 16 x 12 tiles), focus 50 cm above the hero.
// * Smooth follow with a 0.12 s time constant (world_constants.json camera.followLagSec); snaps on zone entry and long
//   teleports. Zoom 0.75x .. 1.25x of the default distance (wheel / pinch through AddZoomInput, input agent).
// * Shake (EvCameraShake, web units): same peak amplitude as the web (I x 3.24 x visible width / height at the focus),
//   re-randomised every frame, exponential decay over the duration; honours the camera-shake setting. The web's
//   throttles apply to every source (EAbyssShakeSource): no override while a shake runs, 100 ms VFXManager throttle.
// * Camera fades: zone transitions, hero death, EvCameraFlash, all composed into the player camera manager's manual fade.
// * Story `focus` steps pan the look-at pivot to a target with Sine in-out over the core's step time.
// The rig does not tick: UAbyssWorldBuilder updates it from IAbyssWorldView::SyncFrame (before the camera manager).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "AbyssCameraRig.generated.h"

class UCameraComponent;
class USceneComponent;
struct FAbyssArtShading;

namespace abyss
{
	struct WorldConstants;
}

/** Where a camera shake request comes from (combat-feel.md 11.5 throttles). */
enum class EAbyssShakeSource : uint8
{
	/** EvCameraShake: the core's ShakeThrottle already applied the 100 ms / no-override rules on the sim clock. */
	Core,
	/** UE-originated VFXManager-style shakes (legendary / set drop): 100 ms throttle shared with Core, no override. */
	Throttled,
	/**
	 * Direct camera shakes outside VFXManager: skill-authored recipe shakes (FxEngine.shake, whose own 120 ms throttle
	 * UAbyssVfxSystem applies) and story `shake` steps. No override; they do not start the 100 ms window.
	 */
	Direct,
};

UCLASS(NotBlueprintable)
class ABYSSFIRE_API AAbyssCameraRig : public AActor
{
	GENERATED_BODY()

public:
	AAbyssCameraRig();

	/** Framing from the art manifest (camera block) and the world constants (zoom range, follow lag). */
	void Configure(const FAbyssArtShading& Shading, const abyss::WorldConstants* Constants);

	/** Per frame: follow the hero focus point (world, already at focus height). Real time drives the camera. */
	void UpdateRig(float RealDeltaSec, const FVector& HeroFocus, bool bHasHero);
	/** Next update places the camera on the focus without lag (zone entry, respawn, long teleports). */
	void SnapToFocus(const FVector& HeroFocus);

	// ---- zoom (input agent) ----
	/** Positive = zoom in. One wheel notch = 1.0 (10 % of the range). */
	void AddZoomInput(float Notches);
	void SetZoomFactor(float DistanceFactor);
	float GetZoomFactor() const { return ZoomTarget; }
	/** Level-up zoom pulse (combat-feel.md 11.8): distance x Scale over InSec, back over OutSec. */
	void PulseZoom(float Scale, float InSec, float OutSec);

	// ---- shake / fades ----
	void SetShakeEnabled(bool bEnabled)
	{
		bShakeEnabled = bEnabled;
		if (!bEnabled)
		{
			StopShake();
		}
	}
	/**
	 * Web rules (combat-feel.md 11.5): a request while a shake is running is dropped (Phaser: no override); a Throttled
	 * request within 100 ms (real time) of the last accepted Core / Throttled shake is dropped (VFXManager throttle).
	 */
	void StartShake(float DurationMs, float Intensity, EAbyssShakeSource Source = EAbyssShakeSource::Core);
	void StopShake();
	/** Full-screen colour flash fading Alpha -> 0 over DurationMs (EvCameraFlash). */
	void Flash(const FLinearColor& Color, float DurationMs, float Alpha);
	/** Base fade (zone transition, death): animate to TargetAlpha over DurationSec in Color. */
	void FadeTo(float TargetAlpha, const FLinearColor& Color, float DurationSec);
	void ClearFades();

	// ---- story focus ----
	void BeginStoryFocus(const FVector& Target, float DurationSec);
	/** Moving targets (an NPC / monster): the end point follows them during and after the pan. */
	void UpdateStoryFocusTarget(const FVector& Target);
	/** The beat ended / focus returned to the hero: follow again with a softer lag for a moment. */
	void EndStoryFocus();
	bool IsStoryFocusActive() const { return bStoryFocus; }

	// ---- queries ----
	UCameraComponent* GetCamera() const { return Camera; }
	float GetYawDegrees() const { return YawDeg; }
	float GetPitchDegrees() const { return PitchDeg; }
	float GetCurrentDistance() const;
	FVector GetFocus() const { return Focus; }
	/** Screen direction (x right, y up) -> tile-space unit direction on the ground (keyboard / stick movement). */
	FVector2D ScreenToGroundDirection(const FVector2D& ScreenDir) const;
	/** Ground forward (screen up) and right as UE world directions (Z = 0). */
	FVector GetGroundForward() const;
	FVector GetGroundRight() const;
	/** tan(vertical FOV / 2) of the current viewport (MPC CameraTanHalfFovY). */
	float GetTanHalfFovY() const { return TanHalfFovY; }

private:
	void ApplyViewTarget();
	void UpdateFov();
	void UpdateFades(float RealDeltaSec);

	UPROPERTY(VisibleAnywhere, Category = "Abyss")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Abyss")
	TObjectPtr<UCameraComponent> Camera;

	// framing
	float YawDeg = 45.f;
	float PitchDeg = -50.f;
	float FovHDeg = 35.f;
	float DefaultDistance = 2537.3f;
	float FocusZ = 50.f;
	float ZoomMin = 0.75f;
	float ZoomMax = 1.25f;
	float FollowLagSec = 0.12f;
	float TanHalfFovY = 0.1773f;
	float CurrentFovH = 35.f;

	// follow
	FVector Focus = FVector::ZeroVector;
	bool bSnapNext = true;
	float SoftFollowRemainingSec = 0.f;

	// zoom
	float ZoomTarget = 1.f;
	float ZoomCurrent = 1.f;
	float PulseScale = 1.f;
	float PulseInSec = 0.f;
	float PulseOutSec = 0.f;
	float PulseElapsedSec = -1.f;

	// shake
	bool bShakeEnabled = true;
	float ShakeDurationSec = 0.f;
	float ShakeElapsedSec = 0.f;
	float ShakeIntensity = 0.f;
	FVector ShakeOffset = FVector::ZeroVector;
	/** Real-time clock of the rig (UpdateRig deltas) and the last accepted VFXManager-path shake on it. */
	double RigRealClockSec = 0.0;
	double LastThrottledShakeSec = -1.0e9;
	FRandomStream ShakeRandom;

	// fades
	float BaseFadeAlpha = 0.f;
	float BaseFadeFrom = 0.f;
	float BaseFadeTarget = 0.f;
	float BaseFadeDurationSec = 0.f;
	float BaseFadeElapsedSec = 0.f;
	FLinearColor BaseFadeColor = FLinearColor::Black;
	float FlashAlpha0 = 0.f;
	float FlashDurationSec = 0.f;
	float FlashElapsedSec = 0.f;
	FLinearColor FlashColor = FLinearColor::White;
	bool bFadeApplied = false;

	// story focus
	bool bStoryFocus = false;
	FVector StoryFrom = FVector::ZeroVector;
	FVector StoryTarget = FVector::ZeroVector;
	float StoryDurationSec = 0.f;
	float StoryElapsedSec = 0.f;
};
