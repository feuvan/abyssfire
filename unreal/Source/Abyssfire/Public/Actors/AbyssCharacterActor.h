// AAbyssCharacterActor: the presentation of one living core entity - hero, monster, NPC, pet, escort
// (ARCHITECTURE 6, art-inventory-ch1.md 2.4-2.5 / 3-5, combat-feel.md 10-13).
//
// * Skeletal mesh from the art manifest (meshes face +Y: the body is turned by the manifest's yaw offset), weapons on the
//   weapon_r / weapon_l sockets (I6 / R4), blob shadow, elite crown + affix aura.
// * Animation through UAbyssAnimInstance: locomotion (Idle / Walk / Run at ground speed / refSpeed) chosen from the
//   snapshot; actions (attack, cast, dodge, death, signature) started by EvPlayAnim at the rate and offset that put the
//   authored beat on the core's contact time; hurt = full Hurt clip when idle / moving, HurtAdd jolt during actions.
// * Hit feedback from the EvHit profile: white flash (flashMs), hero pain tint, recoil (snap along the blow, squash,
//   tilt, eased back), per-actor hit-stop (ApplyHitStop), monster wind-up telegraph tint, status tints (R8), thrown death
//   + fade (combat-feel.md 13.2). All material feedback goes through custom primitive data (AbyssCpd).
// * Selection rim (DECISIONS R16, WorldContract.md 7): a hull-only follower (leader-pose copy of the body + weapon copies)
//   drawn with MI_AF_SelectRim outside the whole figure's silhouette; the world builder picks the kind every frame.
// * The actor does not tick: UAbyssWorldBuilder calls PresentFrame once per rendered frame with the interpolated state.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "abyss/base/Types.h"
#include "abyss/sim/Events.h"

#include "Framework/AbyssPresenter.h"
#include "Framework/AbyssTypes.h"
#include "World/AbyssArtManifest.h"
#include "World/AbyssWorldTypes.h"

#include "AbyssCharacterActor.generated.h"

class UAbyssAnimInstance;
class UAbyssAssetLibrary;
class UAnimSequence;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPoseableMeshComponent;
class USceneComponent;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

namespace abyss
{
	struct AnimTimingTable;
}

/** Everything the world builder resolved for a new character (valid during InitCharacter only). */
struct FAbyssCharacterSetup
{
	abyss::EntityId Id = abyss::kNoEntity;
	abyss::EntityKind Kind = abyss::EntityKind::None;
	FString DefId;
	FString ArtId;
	const FAbyssArtAsset* Art = nullptr;
	float VisualScale = 1.f;
	/** anim_timing.json preset of the entity's rig (combat-feel.md 10.4). */
	float HurtKnockbackPx = 8.f;
	float HurtDurationMs = 200.f;
	float DeathDurationMs = 500.f;
	float DodgeDurationMs = 260.f;
	const abyss::AnimTimingTable* AnimTiming = nullptr;
	/** Hero pain tint (hit_feedback.json painTint). */
	FLinearColor PainTintColor = FLinearColor(1.f, 0.42f, 0.42f);
	float PainTintMs = 100.f;
	bool bFlying = false;
	float HoverCm = 0.f;
	bool bElite = false;
	TArray<FLinearColor> AffixColors;
	bool bCastShadow = true;
	bool bOptimizeAnimation = false;   // URO + tick only when rendered (monsters, ambient NPCs)
	/**
	 * Radial rigs (the slime): the web has one front (se) view and only mirrors it (CharacterAnimator.setFacing without
	 * views), so the face always shows. The actor turns to the camera-facing 3/4 direction on the side of the desired
	 * facing instead of the facing itself (aimed actions stay in the camera-facing half plane).
	 */
	bool bSingleView = false;
	/** The fixed camera yaw (W1, degrees) single-view facings are relative to. */
	float CameraYawDeg = 45.f;
	/** Ghost / afterimage tint (class Spirit colour). */
	FLinearColor SpiritColor = FLinearColor(1.f, 0.7f, 0.36f);
};

/** Interpolated per-frame state handed to the actor by the world builder. */
struct FAbyssCharacterFrame
{
	FVector Location = FVector::ZeroVector;   // feet on the ground
	FVector2D Facing = FVector2D::ZeroVector; // desired facing (UE XY, unit); zero = keep
	float SpeedCmS = 0.f;
	bool bMoving = false;
	bool bAlive = true;
	bool bWindingUp = false;
	bool bExhausted = false;
	bool bTalking = false;
	uint32 StatusMask = 0;
	/** Monsters: the world builder's elite / affix state (re-rolled on respawn). */
	bool bElite = false;
	/** Monsters (K1): a pose the core holds (MonsterView::holdAction: Taunt while the mini-boss's dialogue holds it). */
	abyss::AnimAction HoldAction = abyss::AnimAction::Idle;
	/** Monsters: stunned (status Stun): the Stun clip loops while no action plays. */
	bool bStunned = false;
	/** Monsters: the zone mood's wind-up tints (DR5, zone_moods.json port.windupTint); unset = the web port's tints. */
	bool bHasWindupTint = false;
	FLinearColor WindupTint = FLinearColor::White;
	FLinearColor WindupTintElite = FLinearColor::White;
};

class AAbyssCharacterActor;
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAbyssCharacterNotify, AAbyssCharacterActor* /*Actor*/, FName /*Notify*/);

/**
 * Selection rim of a character (DECISIONS R16, WorldContract.md 7, contract C1). Colour and width beyond the ink at 720
 * rows: Hero #FFF4DC 2 px (always, while alive), Target #FF5A3C 1.5 px (attack target / ring indicator), Hover #FFB03C
 * 1.5 px (hovered monster that is not the target).
 */
enum class EAbyssSelectionRim : uint8
{
	None,
	Hero,
	Target,
	Hover,
};

enum class EAbyssCharacterAction : uint8
{
	None,
	Attack,
	Cast,
	Hurt,
	Dodge,
	Death,
	Signature,
	Work,
	Talk,
	Emote,   // K1 monster actions without a beat: Roar, Taunt, Stun, Rise (AbyssAnimActionRules)
};

UCLASS(NotBlueprintable)
class ABYSSFIRE_API AAbyssCharacterActor : public AActor, public IAbyssPresenter
{
	GENERATED_BODY()

public:
	AAbyssCharacterActor();

	/** Loads the mesh / clips / weapons and configures the components. Returns false without art (actor stays hidden). */
	bool InitCharacter(const FAbyssCharacterSetup& Setup, UAbyssAssetLibrary& Assets);
	/** Per rendered frame. VisualDeltaSec is dilated by the S6 slow motion; RealDeltaSec is not (hit-stop timers). */
	void PresentFrame(const FAbyssCharacterFrame& Frame, const FAbyssFrameInfo& Info, float VisualDeltaSec, float RealDeltaSec);
	/** Places the actor without blending (spawn / zone entry / respawn). */
	void SnapTo(const FVector& Location, const FVector2D& Facing);

	// ---- IAbyssPresenter ----
	virtual void OnPlayAnim(const abyss::EvPlayAnim& Event, const FAbyssFrameInfo& Frame) override;
	virtual void OnHitTaken(const abyss::EvHit& Event) override;
	virtual void OnHitDealt(const abyss::EvHit& Event) override;
	virtual void ApplyHitStop(float DurationMs) override;
	virtual void OnStatusApplied(const abyss::EvStatusApplied& Event) override;
	virtual void OnStatusExpired(const abyss::EvStatusExpired& Event) override;
	virtual void OnTeleported(const abyss::EvEntityTeleported& Event) override;
	virtual void OnAttackCancelled(const abyss::EvMonsterAttackCancelled& Event) override;
	virtual void OnRenamed(const abyss::EvMonsterRenamed& Event) override;

	// ---- world builder hooks ----
	/** Hero equipment (I6): mesh per hand socket; nullptr = the class default attachment of that socket. */
	void SetWeaponMeshes(UStaticMesh* MainHand, FName MainSocket, UStaticMesh* OffHand, FName OffSocket);
	/** Elite state can change on respawn-in-place (affix re-roll). */
	void SetElite(bool bInElite, const TArray<FLinearColor>& AffixColors, UStaticMesh* CrownMesh, UStaticMesh* AuraMesh,
		UMaterialInterface* AuraMaterial);
	/** Two dodge afterimages along the path (art-inventory-ch1.md 8.7 NS_Hero_DodgeAfterimage). */
	void SpawnAfterimages(const FVector& From, const FVector& To, UMaterialInterface* GhostMaterial);
	/** Hero death / respawn presentation (sink + fade, combat-feel.md 13.3). */
	void OnHeroRespawned();
	/** A hard reset of feedback state (zone change for the reused hero actor). */
	void ResetPresentation();
	/**
	 * Shadow presentation of the render tier (DECISIONS P10 revised): the blob shadow shows whenever bBlob (every tier),
	 * at 0.7 of its alpha where the sun casts a CSM (bCastDynamic: the body and weapons cast the sun shadow and contact
	 * shadows); without CSM the blob is the only ground contact, at full alpha.
	 */
	void SetShadowMode(bool bBlob, bool bCastDynamic);
	/**
	 * Selection rim (DECISIONS R16, contract C1): UAbyssWorldBuilder::UpdateSelectionRims calls it every frame; a repeat of
	 * the current kind is ignored. The first non-None call creates the hull-only follower (a leader-pose copy of the body
	 * showing only its ink-hull sections, weapon copies on the same sockets) drawn with dynamic instances of
	 * MI_AF_SelectRim; None hides it (the components are kept for reuse). The rim dithers with the body's Fade, casts no
	 * shadow and hides while the actor is hidden, dying or ghosted. At most 3 characters hold a rim at a time.
	 */
	void SetSelectionRim(EAbyssSelectionRim Kind);
	EAbyssSelectionRim GetSelectionRim() const { return RimKind; }

	// ---- AActor ----
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---- queries ----
	abyss::EntityId GetEntityId() const { return EntityId; }
	abyss::EntityKind GetEntityKind() const { return EntityKind; }
	FVector GetAnchorLocation(EAbyssAnchor Anchor) const;
	float GetVisualHeightCm() const { return VisualHeightCm; }
	float GetBlobRadiusCm() const { return BlobRadiusCm; }
	USkeletalMeshComponent* GetBody() const { return Body; }
	/** Main-hand weapon mesh (trail sockets tip / mid / guard), nullptr when nothing is held. */
	UStaticMeshComponent* GetMainHandMesh() const { return MainHandMesh; }
	/** fx attachments of the art asset (heroes: visor glow, trails, blade glow, halo, bloom; empty otherwise). */
	const FAbyssArtFx& GetArtFx() const { return ArtFx; }
	EAbyssCharacterAction GetCurrentAction() const { return ActionState.Action; }
	/** Death clip progress 0..1 (0 while alive). */
	float GetDeathProgress() const { return bDying ? FMath::Clamp(DeathElapsedSec / FMath::Max(0.001f, DeathTotalSec), 0.f, 1.f) : 0.f; }
	bool IsDying() const { return bDying; }
	bool IsDeathFinished() const { return bDeathFinished; }
	float GetPresentationOpacity() const { return 1.f - CpdValues[AbyssCpd::Fade]; }
	bool HasArt() const { return bHasArt; }
	/** Named anim notifies of this actor's clips (FootL / FootR / FX_*), for audio / VFX hooks. */
	FOnAbyssCharacterNotify OnCharacterNotify;

private:
	struct FActionState
	{
		EAbyssCharacterAction Action = EAbyssCharacterAction::None;
		float RemainingSec = 0.f;
		bool bFaceLock = false;
		FVector2D FaceDir = FVector2D::ZeroVector;
		/**
		 * DECISIONS C14 (4), hero only: seconds left to finish turning toward FaceDir, so the hero faces what the core
		 * releases at before the Contact / Release beat (half the beat, at most HeroAimTurnMaxSec). 0 = the M11 turn rate.
		 */
		float AimTurnBudgetSec = 0.f;
		/** A skill blink (teleport, shadow step) moved the actor during the action: the lock follows the core facing. */
		bool bFaceFollowsCore = false;
		float TelegraphRemainingSec = 0.f;
		float TelegraphTotalSec = 0.f;
	};

	UAnimSequence* FindClip(FName ClipName) const;
	const FAbyssArtClip* FindArtClip(FName ClipName) const;
	/** Clip for an EvPlayAnim (signature / skill-listed / variant cycling), or nullptr. */
	const FAbyssArtClip* ResolveActionClip(const abyss::EvPlayAnim& Event);
	/** bForceLoop: hold / stun poses loop whatever the clip's own loop flag (AbyssAnimActionRules::ForcesLoop). */
	void PlayClip(const FAbyssArtClip& Clip, float Rate, float StartSec, const char* ToState, bool bForceLoop = false);
	void UpdateLocomotion(const FAbyssCharacterFrame& Frame);
	void UpdateFacing(const FAbyssCharacterFrame& Frame, float DeltaSec);
	/** bSingleView: the facing shown for a desired one (se / sw toward the camera; bAimed keeps an aim in front). */
	FVector2D SingleViewFacing(const FVector2D& Desired, bool bAimed);
	void UpdateRecoil(float DeltaSec);
	void UpdateDeath(float DeltaSec);
	void UpdateMaterialFeedback(const FAbyssCharacterFrame& Frame, float DeltaSec);
	void UpdateEliteVisuals(float DeltaSec);
	void UpdateAfterimages(float DeltaSec);
	void UpdateDodgeOffset(float DeltaSec);
	void StartRecoil(const FVector& FromWorld, float Strength);
	void StartJolt(float Strength);
	void BeginDeath(const abyss::EvPlayAnim& Event);
	void SetCpd(int32 Index, float Value);
	void SetCpdColor(int32 Index, const FLinearColor& Color);
	void ApplyCpdTo(UPrimitiveComponent* Component) const;
	float BlendSecFor(const char* FromState, const char* ToState) const;
	void HandleAnimNotify(FName NotifyName);
	UAbyssAnimInstance* GetAnim() const;
	/** Selection rim: creates the follower components on first use (false without art / MI_AF_SelectRim / budget). */
	bool EnsureSelectionRim();
	/** The rim body follows the body's mesh: leader pose, rim on the ink-hull slots, every other section hidden. */
	void SyncRimBody();
	/** The rim weapon copies follow the visible weapon meshes (SetWeaponMeshes). */
	void SyncRimWeapons();
	/** A dynamic MI_AF_SelectRim for one ink-hull slot, its OutlinePx1080 / BakedWidthCm copied from that hull material. */
	UMaterialInstanceDynamic* MakeRimMaterial(UMaterialInterface* SourceHull);
	/** RimColor / RimPx720 of the current kind on every rim instance. */
	void ApplyRimLook();
	/**
	 * Shows the rim while a kind is set and the body can show one (not hidden, dying, finished or ghosted). bForce
	 * re-applies every component's visibility (the weapon copies changed).
	 */
	void UpdateSelectionRimVisibility(bool bForce = false);
	/** Takes / returns a slot of the rim budget (3 rimmed characters). */
	bool AcquireRimBudget();
	void ReleaseRimBudget();

	UPROPERTY(VisibleAnywhere, Category = "Abyss")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Carries recoil / dodge offset / death throw / hover without disturbing the interpolated root. */
	UPROPERTY(VisibleAnywhere, Category = "Abyss")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, Category = "Abyss")
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Abyss")
	TObjectPtr<UStaticMeshComponent> MainHandMesh;

	UPROPERTY(VisibleAnywhere, Category = "Abyss")
	TObjectPtr<UStaticMeshComponent> OffHandMesh;

	UPROPERTY(VisibleAnywhere, Category = "Abyss")
	TObjectPtr<UStaticMeshComponent> BlobShadow;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CrownMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> AuraMesh;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPoseableMeshComponent>> Afterimages;

	/** Selection rim follower (DECISIONS R16): leader-pose copy of Body showing only its ink-hull sections. */
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> RimBody;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> RimMainHand;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> RimOffHand;

	/** MI_AF_SelectRim (the parent of every rim instance). */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> RimParent;

	/** BodyMask 1: the slots of the rim copies that draw nothing (every slot but the ink hull). */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RimMask;

	/** Rim instances of the body's ink-hull slots. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> RimBodyMaterials;

	/** Rim instances of the weapon copies' ink-hull slots (index 0 main hand, 1 off hand; reused across weapon swaps). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> RimWeaponMaterials;

	/** Clips by manifest clip name (kept alive here; also cached by the asset library). */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UAnimSequence>> Clips;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> DefaultMainHand;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> DefaultOffHand;

	TArray<FAbyssArtClip> ArtClips;
	FAbyssArtFx ArtFx;
	const abyss::AnimTimingTable* AnimTiming = nullptr;
	abyss::EntityId EntityId = abyss::kNoEntity;
	abyss::EntityKind EntityKind = abyss::EntityKind::None;
	FString DefId;
	FName DefaultMainSocket;
	FName DefaultOffSocket;
	bool bHasArt = false;
	float MeshYawOffsetDeg = -90.f;
	float VisualScale = 1.f;
	float VisualHeightCm = 170.f;
	float BlobRadiusCm = 30.f;
	float HoverCm = 0.f;
	bool bFlying = false;
	float HurtKnockbackPx = 8.f;
	float HurtDurationMs = 200.f;
	float DeathDurationMs = 500.f;
	float DodgeDurationMs = 260.f;
	FLinearColor PainTintColor = FLinearColor(1.f, 0.42f, 0.42f);
	float PainTintMs = 100.f;
	FLinearColor SpiritColor = FLinearColor::White;

	// animation state
	FActionState ActionState;
	FName LocomotionClip;
	/** Monsters (K1): facing toward the hero from the Taunt event while the hold lasts (the dialogue freezes the world). */
	FVector2D HoldFaceDir = FVector2D::ZeroVector;
	const char* CurrentState = "idle";   // anim_timing.json transition state names
	int32 AttackVariant = 0;
	float FreezeRemainingMs = 0.f;
	/**
	 * Frozen-world hold (classes-stats-skills 19.1, D13): sim time of the current action's Contact / Release beat
	 * (EvPlayAnim startMs + contactMs; < 0 = none). While the world is frozen before that beat the clip holds at play
	 * rate 0 and resumes on unfreeze, so the hit still lands on the notify. Separate from the hit-stop timer.
	 */
	double ActionBeatSimMs = -1.0;
	bool bWorldHold = false;
	float CurrentYawDeg = 0.f;
	bool bHasYaw = false;
	bool bSingleView = false;
	bool bSingleViewLeft = false;   // mirrored (sw) side of a single-view rig
	float SingleViewCameraYawDeg = 45.f;

	// feedback
	float FlashRemainingSec = 0.f;
	float FlashTotalSec = 0.f;
	float PainRemainingSec = 0.f;
	float PainTotalSec = 0.f;
	FVector RecoilDir = FVector::ZeroVector;
	float RecoilStrength = 0.f;
	float RecoilElapsedSec = -1.f;
	float RecoilPeakCm = 0.f;
	FVector DodgeOffset = FVector::ZeroVector;
	float DodgeElapsedSec = -1.f;
	float DodgeTotalSec = 0.f;

	// death
	bool bDying = false;
	bool bDeathFinished = false;
	bool bHeroDeath = false;
	float DeathElapsedSec = 0.f;
	float DeathTotalSec = 0.5f;
	FVector DeathThrowDir = FVector::ZeroVector;
	float DeathThrowCm = 0.f;
	float DeathAirSec = 0.f;
	float DeathHopCm = 0.f;
	float DeathSpinDeg = 0.f;
	float DeathFadeDelaySec = 0.f;
	float DeathFadeSec = 0.f;

	// elite
	bool bElite = false;
	TArray<FLinearColor> AffixColors;
	float EliteTimeSec = 0.f;

	// afterimages
	TArray<float> AfterimageAge;
	TArray<float> AfterimageAlpha;

	// custom primitive data cache (AbyssCpd)
	float CpdValues[AbyssCpd::Count] = {};
	bool bCpdDirty = true;
	bool bBlobWanted = true;
	bool bHasBlobAssets = false;
	/** The sun casts a CSM on this tier (SetShadowMode): the blob draws at 0.7 of its alpha (DECISIONS P10 revised). */
	bool bBlobUnderSunShadow = false;

	// selection rim (DECISIONS R16)
	EAbyssSelectionRim RimKind = EAbyssSelectionRim::None;
	bool bRimBudgetHeld = false;
	bool bRimShown = false;
	/** MI_AF_SelectRim is missing (older content build): logged once, never retried. */
	bool bRimUnavailable = false;
};
