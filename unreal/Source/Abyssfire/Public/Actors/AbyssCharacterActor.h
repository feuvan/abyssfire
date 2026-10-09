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
};

class AAbyssCharacterActor;
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAbyssCharacterNotify, AAbyssCharacterActor* /*Actor*/, FName /*Notify*/);

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

	// ---- queries ----
	abyss::EntityId GetEntityId() const { return EntityId; }
	abyss::EntityKind GetEntityKind() const { return EntityKind; }
	FVector GetAnchorLocation(EAbyssAnchor Anchor) const;
	float GetVisualHeightCm() const { return VisualHeightCm; }
	float GetBlobRadiusCm() const { return BlobRadiusCm; }
	USkeletalMeshComponent* GetBody() const { return Body; }
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
		float TelegraphRemainingSec = 0.f;
		float TelegraphTotalSec = 0.f;
	};

	UAnimSequence* FindClip(FName ClipName) const;
	const FAbyssArtClip* FindArtClip(FName ClipName) const;
	/** Clip for an EvPlayAnim (signature / skill-listed / variant cycling), or nullptr. */
	const FAbyssArtClip* ResolveActionClip(const abyss::EvPlayAnim& Event);
	void PlayClip(const FAbyssArtClip& Clip, float Rate, float StartSec, const char* ToState);
	void UpdateLocomotion(const FAbyssCharacterFrame& Frame);
	void UpdateFacing(const FAbyssCharacterFrame& Frame, float DeltaSec);
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

	/** Clips by manifest clip name (kept alive here; also cached by the asset library). */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UAnimSequence>> Clips;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> DefaultMainHand;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> DefaultOffHand;

	TArray<FAbyssArtClip> ArtClips;
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
	const char* CurrentState = "idle";   // anim_timing.json transition state names
	int32 AttackVariant = 0;
	float FreezeRemainingMs = 0.f;
	float CurrentYawDeg = 0.f;
	bool bHasYaw = false;

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
};
