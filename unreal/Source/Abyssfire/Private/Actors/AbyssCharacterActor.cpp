#include "Actors/AbyssCharacterActor.h"

#include "Abyssfire.h"
#include "Animation/AnimSequence.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

#include "abyss/data/CombatData.h"

#include "Anim/AbyssAnimInstance.h"
#include "Framework/AbyssUnits.h"
#include "World/AbyssAssetLibrary.h"

namespace AbyssCharacterPrivate
{
	constexpr float TurnRateDegPerSec = 720.f;   // M11 (render-only), also used for the hero
	constexpr float YawDeadbandDeg = 2.f;
	constexpr float MinMovingSpeedCmS = 5.f;
	constexpr float CrownSpinDegPerSec = 45.f;
	constexpr float AuraPeriodSec = 1.2f;
	constexpr float AfterimageLifeSec = 0.26f;
	constexpr float JoltPainSec = 0.09f;

	// Render-only feedback colours (art-inventory-ch1.md 1.8 / R8; sRGB hex in the spec).
	FLinearColor TelegraphNormal() { return AbyssWorldUtil::ColorFromRgb(0xFFC4B0); }
	FLinearColor TelegraphElite() { return AbyssWorldUtil::ColorFromRgb(0xFF7A5C); }

	struct FStatusTint
	{
		abyss::StatusType Type;
		uint32 Rgb;
		float Alpha;
	};
	// Priority order: the first status present tints the character.
	constexpr FStatusTint StatusTints[] = {
		{ abyss::StatusType::Freeze, 0x7FD0FF, 0.45f },
		{ abyss::StatusType::Stun, 0x7FD0FF, 0.45f },
		{ abyss::StatusType::Burn, 0xFF7A3A, 0.35f },
		{ abyss::StatusType::Poison, 0x8FE04A, 0.35f },
		{ abyss::StatusType::Bleed, 0xC8323C, 0.22f },
		{ abyss::StatusType::Slow, 0x9FD8FF, 0.18f },
	};

	bool HasStatus(uint32 Mask, abyss::StatusType Type)
	{
		return (Mask & (1u << static_cast<uint32>(Type))) != 0;
	}

	void ConfigureAttachmentMesh(UStaticMeshComponent* Mesh)
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetReceivesDecals(false);
	}

	FVector2D Flat(const FVector& V)
	{
		return FVector2D(V.X, V.Y);
	}

	FName ClipName(const std::string& Text)
	{
		return Text.empty() ? NAME_None : FName(UTF8_TO_TCHAR(Text.c_str()));
	}
}

AAbyssCharacterActor::AAbyssCharacterActor()
{
	using namespace AbyssCharacterPrivate;
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Visual"));
	VisualRoot->SetupAttachment(SceneRoot);

	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(VisualRoot);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetGenerateOverlapEvents(false);
	Body->SetCanEverAffectNavigation(false);
	Body->SetReceivesDecals(false);

	MainHandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MainHand"));
	MainHandMesh->SetupAttachment(Body);
	ConfigureAttachmentMesh(MainHandMesh);

	OffHandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OffHand"));
	OffHandMesh->SetupAttachment(Body);
	ConfigureAttachmentMesh(OffHandMesh);

	BlobShadow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlobShadow"));
	BlobShadow->SetupAttachment(SceneRoot);
	ConfigureAttachmentMesh(BlobShadow);
	BlobShadow->SetCastShadow(false);
	BlobShadow->SetTranslucentSortPriority(-10);
}

UAbyssAnimInstance* AAbyssCharacterActor::GetAnim() const
{
	return Body ? Cast<UAbyssAnimInstance>(Body->GetAnimInstance()) : nullptr;
}

bool AAbyssCharacterActor::InitCharacter(const FAbyssCharacterSetup& Setup, UAbyssAssetLibrary& Assets)
{
	EntityId = Setup.Id;
	EntityKind = Setup.Kind;
	DefId = Setup.DefId;
	AnimTiming = Setup.AnimTiming;
	VisualScale = FMath::Max(0.05f, Setup.VisualScale);
	HurtKnockbackPx = Setup.HurtKnockbackPx;
	HurtDurationMs = FMath::Max(1.f, Setup.HurtDurationMs);
	DeathDurationMs = FMath::Max(1.f, Setup.DeathDurationMs);
	DodgeDurationMs = FMath::Max(1.f, Setup.DodgeDurationMs);
	PainTintColor = Setup.PainTintColor;
	PainTintMs = Setup.PainTintMs;
	SpiritColor = Setup.SpiritColor;
	bFlying = Setup.bFlying;
	HoverCm = Setup.HoverCm;
	MeshYawOffsetDeg = Assets.GetManifest().GetShading().MeshYawOffsetDeg;

	bHasArt = false;
	const FAbyssArtAsset* Art = Setup.Art;
	USkeletalMesh* Mesh = Art != nullptr ? Assets.LoadSkeletalMesh(*Art) : nullptr;
	if (Mesh == nullptr)
	{
		UE_LOG(LogAbyss, Warning, TEXT("No skeletal mesh for entity %u (def %s, art %s): actor hidden"), EntityId, *Setup.DefId,
			*Setup.ArtId);
		SetActorHiddenInGame(true);
		return false;
	}

	const float AssetScale = FMath::Max(0.01f, Art->Scale) * VisualScale;
	Body->SetSkeletalMeshAsset(Mesh);
	Body->SetRelativeRotation(FRotator(0.f, MeshYawOffsetDeg, 0.f));
	Body->SetRelativeScale3D(FVector(AssetScale));
	Body->SetCastShadow(Setup.bCastShadow);
	if (Setup.bOptimizeAnimation)
	{
		Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		Body->bEnableUpdateRateOptimizations = true;
	}

	ArtClips = Art->Clips;
	ArtFx = Art->Fx;
	Clips.Reset();
	for (const FAbyssArtClip& Clip : ArtClips)
	{
		if (Clip.bStaticPose)
		{
			continue;   // portrait poses are for offline renders
		}
		if (UAnimSequence* Seq = Assets.LoadClip(Clip))
		{
			Clips.Add(Clip.Name, Seq);
		}
	}

	Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Body->SetAnimInstanceClass(UAbyssAnimInstance::StaticClass());
	if (UAbyssAnimInstance* Anim = GetAnim())
	{
		Anim->OnNotify.AddUObject(this, &AAbyssCharacterActor::HandleAnimNotify);
	}

	// Weapons: the class default attachments of the manifest (I6 swaps them for the equipped weapon type later).
	DefaultMainHand = nullptr;
	DefaultOffHand = nullptr;
	DefaultMainSocket = FName(TEXT("weapon_r"));
	DefaultOffSocket = FName(TEXT("weapon_l"));
	for (const FAbyssArtAttachment& Attachment : Art->Attachments)
	{
		const FAbyssArtAsset* WeaponArt = Assets.GetManifest().FindAsset(Attachment.Object);
		UStaticMesh* WeaponMesh = WeaponArt != nullptr ? Assets.LoadStaticMesh(*WeaponArt) : nullptr;
		if (WeaponMesh == nullptr)
		{
			continue;
		}
		if (Attachment.Socket == DefaultOffSocket)
		{
			DefaultOffHand = WeaponMesh;
		}
		else
		{
			DefaultMainHand = WeaponMesh;
			DefaultMainSocket = Attachment.Socket.IsNone() ? DefaultMainSocket : Attachment.Socket;
		}
	}
	MainHandMesh->SetCastShadow(Setup.bCastShadow);
	OffHandMesh->SetCastShadow(Setup.bCastShadow);
	SetWeaponMeshes(nullptr, NAME_None, nullptr, NAME_None);

	// Sizes: manifest bounds / height (R2), blob radius (art-inventory-ch1.md 1.4: radius per character).
	VisualHeightCm = (Art->GetVisualHeightCm() > 1.f ? Art->GetVisualHeightCm() : 170.f) * AssetScale;
	BlobRadiusCm = (Art->BlobShadowRadiusCm > 0.f ? Art->BlobShadowRadiusCm * AssetScale : VisualHeightCm * 0.22f);
	UStaticMesh* QuadMesh = Assets.LoadContentMesh(TEXT("FX"), TEXT("SM_FX_Quad"));
	UMaterialInterface* BlobMaterial = Assets.LoadMaterial(TEXT("M_AF_BlobShadow"));
	if (QuadMesh != nullptr && BlobMaterial != nullptr)
	{
		BlobShadow->SetStaticMesh(QuadMesh);
		BlobShadow->SetMaterial(0, BlobMaterial);
		BlobShadow->SetRelativeLocation(FVector(0.f, 0.f, 1.5f));
		BlobShadow->SetRelativeScale3D(FVector(BlobRadiusCm * 2.f / 100.f, BlobRadiusCm * 2.f / 100.f, 1.f));
		bHasBlobAssets = true;
		BlobShadow->SetVisibility(bBlobWanted);
	}
	else
	{
		bHasBlobAssets = false;
		BlobShadow->SetVisibility(false);
	}

	bHasArt = true;
	ResetPresentation();
	return true;
}

void AAbyssCharacterActor::SetShadowMode(bool bBlob, bool bCastDynamic)
{
	bBlobWanted = bBlob;
	if (BlobShadow != nullptr)
	{
		BlobShadow->SetVisibility(bBlob && bHasBlobAssets);
	}
	for (UPrimitiveComponent* Component : { static_cast<UPrimitiveComponent*>(Body.Get()),
			 static_cast<UPrimitiveComponent*>(MainHandMesh.Get()), static_cast<UPrimitiveComponent*>(OffHandMesh.Get()) })
	{
		if (Component != nullptr)
		{
			Component->SetCastShadow(bCastDynamic);
		}
	}
}

void AAbyssCharacterActor::ResetPresentation()
{
	ActionState = FActionState();
	LocomotionClip = NAME_None;
	FreezeRemainingMs = 0.f;
	ActionBeatSimMs = -1.0;
	bWorldHold = false;
	FlashRemainingSec = FlashTotalSec = 0.f;
	PainRemainingSec = PainTotalSec = 0.f;
	RecoilElapsedSec = -1.f;
	DodgeElapsedSec = -1.f;
	DodgeOffset = FVector::ZeroVector;
	bDying = false;
	bDeathFinished = false;
	bHeroDeath = false;
	DeathElapsedSec = 0.f;
	for (float& Value : CpdValues)
	{
		Value = 0.f;
	}
	bCpdDirty = true;
	if (UAbyssAnimInstance* Anim = GetAnim())
	{
		Anim->SetFrozen(false);
	}
	if (VisualRoot != nullptr)
	{
		VisualRoot->SetRelativeLocation(FVector(0.f, 0.f, HoverCm));
		VisualRoot->SetRelativeRotation(FRotator::ZeroRotator);
		VisualRoot->SetRelativeScale3D(FVector::OneVector);
	}
	SetActorHiddenInGame(!bHasArt);
	// Idle pose right away (no blend from the reference pose).
	if (const FAbyssArtClip* Idle = FindArtClip(FName(TEXT("Idle"))))
	{
		if (UAbyssAnimInstance* Anim = GetAnim())
		{
			if (UAnimSequence* Seq = FindClip(Idle->Name))
			{
				Anim->Play(Seq, true, 1.f, 0.f, 0.f, Idle->Notifies);
				LocomotionClip = Idle->Name;
				CurrentState = "idle";
			}
		}
	}
}

// =====================================================================================================================
// Clips
// =====================================================================================================================

UAnimSequence* AAbyssCharacterActor::FindClip(FName InClipName) const
{
	const TObjectPtr<UAnimSequence>* Found = Clips.Find(InClipName);
	return Found ? Found->Get() : nullptr;
}

const FAbyssArtClip* AAbyssCharacterActor::FindArtClip(FName InClipName) const
{
	if (InClipName.IsNone() || !Clips.Contains(InClipName))
	{
		return nullptr;
	}
	return ArtClips.FindByPredicate([InClipName](const FAbyssArtClip& Clip) { return Clip.Name == InClipName; });
}

float AAbyssCharacterActor::BlendSecFor(const char* FromState, const char* ToState) const
{
	if (AnimTiming == nullptr)
	{
		return 0.08f;
	}
	return static_cast<float>(AnimTiming->TransitionMs(FromState, ToState)) / 1000.f;
}

void AAbyssCharacterActor::PlayClip(const FAbyssArtClip& Clip, float Rate, float StartSec, const char* ToState)
{
	UAbyssAnimInstance* Anim = GetAnim();
	UAnimSequence* Seq = FindClip(Clip.Name);
	if (Anim == nullptr || Seq == nullptr)
	{
		return;
	}
	Anim->Play(Seq, Clip.bLoop, Rate, BlendSecFor(CurrentState, ToState), StartSec, Clip.Notifies);
	CurrentState = ToState;
}

const FAbyssArtClip* AAbyssCharacterActor::ResolveActionClip(const abyss::EvPlayAnim& Event)
{
	using namespace AbyssCharacterPrivate;
	const FName Requested = ClipName(Event.clip);
	auto ByName = [this](const TCHAR* Name) { return FindArtClip(FName(Name)); };
	auto BySkillOrName = [this](FName Name) -> const FAbyssArtClip*
	{
		for (const FAbyssArtClip& Clip : ArtClips)
		{
			if (Clip.bSignature && Clip.Skills.Contains(Name) && Clips.Contains(Clip.Name))
			{
				return &Clip;
			}
		}
		for (const FAbyssArtClip& Clip : ArtClips)
		{
			if (Clip.Skills.Contains(Name) && Clips.Contains(Clip.Name))
			{
				return &Clip;
			}
		}
		return FindArtClip(Name);
	};

	switch (Event.action)
	{
	case abyss::AnimAction::Attack:
	{
		static const FName Attack01(TEXT("Attack01"));
		if (!Requested.IsNone() && Requested != Attack01)
		{
			if (const FAbyssArtClip* Clip = BySkillOrName(Requested))
			{
				return Clip;
			}
		}
		const FAbyssArtClip* Base = ByName(TEXT("Attack01"));
		if (Base == nullptr || EntityKind != abyss::EntityKind::Hero)
		{
			return Base;
		}
		// Hero basic attacks cycle Attack01..03 when they share the same contact beat (each strike its own silhouette,
		// identical core timing).
		TArray<const FAbyssArtClip*, TInlineAllocator<4>> Variants;
		for (const TCHAR* Name : { TEXT("Attack01"), TEXT("Attack02"), TEXT("Attack03") })
		{
			const FAbyssArtClip* Clip = ByName(Name);
			if (Clip != nullptr && Clip->Skills.Num() == 0 && FMath::IsNearlyEqual(Clip->ContactSec, Base->ContactSec, 0.002f))
			{
				Variants.Add(Clip);
			}
		}
		if (Variants.Num() == 0)
		{
			return Base;
		}
		const FAbyssArtClip* Chosen = Variants[AttackVariant % Variants.Num()];
		AttackVariant = (AttackVariant + 1) % FMath::Max(1, Variants.Num());
		return Chosen;
	}
	case abyss::AnimAction::Cast:
	case abyss::AnimAction::Signature:
		if (!Requested.IsNone())
		{
			if (const FAbyssArtClip* Clip = BySkillOrName(Requested))
			{
				return Clip;
			}
		}
		return ByName(TEXT("Cast01"));
	case abyss::AnimAction::Dodge:
		return ByName(TEXT("Dodge"));
	case abyss::AnimAction::Death:
		return ByName(TEXT("Death"));
	case abyss::AnimAction::Hurt:
		return ByName(TEXT("Hurt"));
	case abyss::AnimAction::Work:
		return !Requested.IsNone() && FindArtClip(Requested) ? FindArtClip(Requested) : ByName(TEXT("Work"));
	case abyss::AnimAction::Talk:
		return !Requested.IsNone() && FindArtClip(Requested) ? FindArtClip(Requested) : ByName(TEXT("Talk"));
	case abyss::AnimAction::Idle:
	case abyss::AnimAction::Walk:
		break;
	}
	return nullptr;
}

// =====================================================================================================================
// IAbyssPresenter
// =====================================================================================================================

void AAbyssCharacterActor::OnPlayAnim(const abyss::EvPlayAnim& Event, const FAbyssFrameInfo& Frame)
{
	if (!bHasArt)
	{
		return;
	}
	if (Event.action == abyss::AnimAction::Death)
	{
		BeginDeath(Event);
		return;
	}
	if (bDying || Event.action == abyss::AnimAction::Idle || Event.action == abyss::AnimAction::Walk)
	{
		return;   // locomotion comes from the snapshot
	}

	// The action began at Event.startMs (sim time); the interpolated pose shows Frame.RenderSimMs.
	const float ElapsedSec = static_cast<float>(FMath::Max(0.0, Frame.RenderSimMs - Event.startMs) / 1000.0);
	const FAbyssArtClip* Clip = ResolveActionClip(Event);

	float Rate = Event.playRate > 0.0 ? static_cast<float>(Event.playRate) : 1.f;
	if (Clip != nullptr)
	{
		const float BeatSec = Clip->GetBeatSec();
		if (Event.contactMs > 0.0 && BeatSec > 0.f)
		{
			// P5: play the clip so its authored Contact / Release lands exactly on the core's beat.
			Rate = BeatSec / static_cast<float>(Event.contactMs / 1000.0);
		}
		else if (Event.durationMs > 0.0 && Clip->LengthSec > 0.f && Event.action != abyss::AnimAction::Attack)
		{
			Rate = Clip->LengthSec / static_cast<float>(Event.durationMs / 1000.0);
		}
		Rate = FMath::Clamp(Rate, 0.25f, 4.f);
	}

	ActionState = FActionState();
	// The beat the core waits for (T3 / T4): a frozen world holds the clip before it (PresentFrame).
	ActionBeatSimMs = Event.contactMs > 0.0 ? Event.startMs + Event.contactMs : -1.0;
	switch (Event.action)
	{
	case abyss::AnimAction::Attack: ActionState.Action = EAbyssCharacterAction::Attack; break;
	case abyss::AnimAction::Cast: ActionState.Action = EAbyssCharacterAction::Cast; break;
	case abyss::AnimAction::Dodge: ActionState.Action = EAbyssCharacterAction::Dodge; break;
	case abyss::AnimAction::Hurt: ActionState.Action = EAbyssCharacterAction::Hurt; break;
	case abyss::AnimAction::Signature: ActionState.Action = EAbyssCharacterAction::Signature; break;
	case abyss::AnimAction::Work: ActionState.Action = EAbyssCharacterAction::Work; break;
	case abyss::AnimAction::Talk: ActionState.Action = EAbyssCharacterAction::Talk; break;
	default: ActionState.Action = EAbyssCharacterAction::Cast; break;
	}
	float DurationSec = Event.durationMs > 0.0 ? static_cast<float>(Event.durationMs / 1000.0) : 0.f;
	if (DurationSec <= 0.f && Clip != nullptr)
	{
		DurationSec = Clip->LengthSec / Rate;
	}
	ActionState.RemainingSec = FMath::Max(0.05f, DurationSec - ElapsedSec);
	if (Event.hasFaceTarget)
	{
		const FVector Target = AbyssUnits::TileToWorld(Event.faceTarget, GetActorLocation().Z);
		const FVector2D Dir = AbyssCharacterPrivate::Flat(Target - GetActorLocation()).GetSafeNormal();
		if (!Dir.IsNearlyZero())
		{
			ActionState.bFaceLock = true;
			ActionState.FaceDir = Dir;
		}
	}
	if (Event.windupMs > 0.0)
	{
		ActionState.TelegraphTotalSec = static_cast<float>(Event.windupMs / 1000.0);
		ActionState.TelegraphRemainingSec = FMath::Max(0.f, ActionState.TelegraphTotalSec - ElapsedSec);
	}

	if (Clip != nullptr)
	{
		const char* State = "cast";
		switch (ActionState.Action)
		{
		case EAbyssCharacterAction::Attack: State = "attack"; break;
		case EAbyssCharacterAction::Dodge: State = "dodge"; break;
		case EAbyssCharacterAction::Hurt: State = "hurt"; break;
		default: State = "cast"; break;
		}
		PlayClip(*Clip, Rate, ElapsedSec * Rate, State);
		LocomotionClip = NAME_None;
	}
}

void AAbyssCharacterActor::OnHitTaken(const abyss::EvHit& Event)
{
	if (!bHasArt || Event.dodged || bDying)
	{
		return;
	}
	const abyss::HitProfileDef& Profile = Event.profile;
	const bool bHero = EntityKind == abyss::EntityKind::Hero;
	if (!bHero)
	{
		// White flash for flashMs, restarting on re-hit (combat-feel.md 11.3); DoT ticks only flash.
		FlashTotalSec = FlashRemainingSec = static_cast<float>(Profile.flashMs / 1000.0);
	}
	else if (FlashRemainingSec <= 0.f)
	{
		PainTotalSec = PainRemainingSec = PainTintMs / 1000.f;
	}
	if (Event.tick || Event.killed)
	{
		return;   // a kill plays the thrown death (EvPlayAnim Death) instead of the recoil
	}
	const float Strength = static_cast<float>(Profile.recoil);
	if (Strength <= 0.f)
	{
		return;
	}
	const FVector FromWorld = Event.hasFrom ? AbyssUnits::TileToWorld(Event.from, GetActorLocation().Z)
											: GetActorLocation() - GetActorForwardVector() * 100.0;
	StartRecoil(FromWorld, Strength);

	const bool bInAction = ActionState.Action == EAbyssCharacterAction::Attack || ActionState.Action == EAbyssCharacterAction::Cast
		|| ActionState.Action == EAbyssCharacterAction::Dodge || ActionState.Action == EAbyssCharacterAction::Signature;
	const FAbyssArtClip* Hurt = FindArtClip(FName(TEXT("Hurt")));
	if (bInAction || Hurt == nullptr)
	{
		StartJolt(Strength);
		return;
	}
	ActionState = FActionState();
	ActionState.Action = EAbyssCharacterAction::Hurt;
	ActionState.RemainingSec = FMath::Max(HurtDurationMs / 1000.f, 0.05f);
	const float Rate = Hurt->LengthSec > 0.f ? FMath::Clamp(Hurt->LengthSec / ActionState.RemainingSec, 0.5f, 3.f) : 1.f;
	PlayClip(*Hurt, Rate, 0.f, "hurt");
	LocomotionClip = NAME_None;
}

void AAbyssCharacterActor::OnHitDealt(const abyss::EvHit& Event)
{
	// Attacker feedback is the hit-stop the driver applies right after this call (EvHit::attackerStopMs).
}

void AAbyssCharacterActor::ApplyHitStop(float DurationMs)
{
	if (DurationMs <= 0.f || bDying)
	{
		return;
	}
	FreezeRemainingMs = FMath::Max(FreezeRemainingMs, DurationMs);
	if (UAbyssAnimInstance* Anim = GetAnim())
	{
		Anim->SetFrozen(true);
	}
}

void AAbyssCharacterActor::OnStatusApplied(const abyss::EvStatusApplied& Event)
{
	// Tints follow Snapshot statusMask every frame (PresentFrame); particles are the VFX system's (R8).
}

void AAbyssCharacterActor::OnStatusExpired(const abyss::EvStatusExpired& Event)
{
}

void AAbyssCharacterActor::OnTeleported(const abyss::EvEntityTeleported& Event)
{
	const FVector From = AbyssUnits::TileToWorld(Event.from, GetActorLocation().Z);
	const FVector To = AbyssUnits::TileToWorld(Event.to, GetActorLocation().Z);
	if (Event.reason == abyss::TeleportReason::Dodge)
	{
		// The core moved the hero instantly; the mesh slides from the old spot over the dodge (art-inventory-ch1.md 2.5).
		DodgeOffset = FVector(From.X - To.X, From.Y - To.Y, 0.0);
		DodgeTotalSec = DodgeDurationMs / 1000.f;
		DodgeElapsedSec = 0.f;
	}
	else
	{
		DodgeOffset = FVector::ZeroVector;
		DodgeElapsedSec = -1.f;
	}
	SetActorLocation(To);
}

void AAbyssCharacterActor::OnAttackCancelled(const abyss::EvMonsterAttackCancelled& Event)
{
	ActionState.TelegraphRemainingSec = 0.f;
	if (ActionState.Action == EAbyssCharacterAction::Attack)
	{
		ActionState.RemainingSec = 0.f;
	}
}

void AAbyssCharacterActor::OnRenamed(const abyss::EvMonsterRenamed& Event)
{
	// The nameplate reads MonsterView::nameKey (IAbyssWorldUi); nothing changes on the mesh.
}

// =====================================================================================================================
// Feedback helpers
// =====================================================================================================================

void AAbyssCharacterActor::StartRecoil(const FVector& FromWorld, float Strength)
{
	FVector2D Dir = AbyssCharacterPrivate::Flat(GetActorLocation() - FromWorld).GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		Dir = -AbyssCharacterPrivate::Flat(GetActorForwardVector()).GetSafeNormal();
	}
	RecoilDir = FVector(Dir.X, Dir.Y, 0.0);
	RecoilStrength = Strength;
	// recoil = min(14, hurtKnockback x 0.65 x s) px along n -> cm (45 px per tile).
	RecoilPeakCm = FMath::Min(14.f, HurtKnockbackPx * 0.65f * Strength) * AbyssWorldUtil::VfxPxToCm;
	RecoilElapsedSec = 0.f;
}

void AAbyssCharacterActor::StartJolt(float Strength)
{
	// combat-feel.md 10.4: a hurt during an action plays a jolt (HurtAdd + 90 ms pain tint + hit-freeze min(40, 20 s)).
	if (const FAbyssArtClip* HurtAdd = FindArtClip(FName(TEXT("HurtAdd"))))
	{
		if (UAbyssAnimInstance* Anim = GetAnim())
		{
			Anim->PlayAdditive(FindClip(HurtAdd->Name), 1.f, 1.f);
		}
	}
	if (FlashRemainingSec <= 0.f)
	{
		PainTotalSec = PainRemainingSec = AbyssCharacterPrivate::JoltPainSec;
	}
	ApplyHitStop(FMath::Min(40.f, 20.f * Strength));
}

void AAbyssCharacterActor::BeginDeath(const abyss::EvPlayAnim& Event)
{
	bDying = true;
	bDeathFinished = false;
	bHeroDeath = EntityKind == abyss::EntityKind::Hero;
	DeathElapsedSec = 0.f;
	ActionState = FActionState();
	ActionState.Action = EAbyssCharacterAction::Death;
	FreezeRemainingMs = 0.f;
	ActionBeatSimMs = -1.0;
	bWorldHold = false;
	RecoilElapsedSec = -1.f;
	if (UAbyssAnimInstance* Anim = GetAnim())
	{
		Anim->SetFrozen(false);
	}
	const float DurationMs = Event.durationMs > 0.0 ? static_cast<float>(Event.durationMs) : DeathDurationMs;
	DeathTotalSec = FMath::Max(0.001f, DurationMs / 1000.f);
	if (const FAbyssArtClip* Death = FindArtClip(FName(TEXT("Death"))))
	{
		const float Rate = Death->LengthSec > 0.f ? FMath::Clamp(Death->LengthSec / (DurationMs / 1000.f), 0.5f, 2.f) : 1.f;
		PlayClip(*Death, Rate, 0.f, "death");
		LocomotionClip = NAME_None;
	}

	DeathThrowCm = 0.f;
	DeathHopCm = 0.f;
	DeathSpinDeg = 0.f;
	DeathAirSec = 0.f;
	if (!bHeroDeath && Event.hasFaceTarget)
	{
		// combat-feel.md 13.2: thrown away from the last hit source.
		const bool bHeavy = DurationMs >= 750.f;
		const FVector Source = AbyssUnits::TileToWorld(Event.faceTarget, GetActorLocation().Z);
		const FVector2D Dir = AbyssCharacterPrivate::Flat(GetActorLocation() - Source).GetSafeNormal();
		DeathThrowDir = FVector(Dir.X, Dir.Y, 0.0);
		DeathThrowCm = (bHeavy ? 6.f : 16.f) * AbyssWorldUtil::VfxPxToCm;
		DeathAirSec = FMath::Max(160.f, 0.35f * DurationMs) / 1000.f;
		DeathHopCm = (bHeavy ? 3.f : 9.f) * AbyssWorldUtil::VfxPxToCm;
		DeathSpinDeg = bHeavy ? 8.f : 22.f;
		DeathFadeDelaySec = DeathAirSec + 0.12f;
		DeathFadeSec = FMath::Max(200.f, 0.6f * DurationMs) / 1000.f;
	}
	else
	{
		// In-place sink and fade (hero; monsters killed by a DoT with no directional hit).
		DeathThrowDir = FVector::ZeroVector;
		DeathFadeDelaySec = 0.25f * DurationMs / 1000.f;
		DeathFadeSec = bHeroDeath ? 0.4f : FMath::Max(200.f, 0.6f * DurationMs) / 1000.f;
	}
}

void AAbyssCharacterActor::OnHeroRespawned()
{
	ResetPresentation();
}

void AAbyssCharacterActor::SetWeaponMeshes(UStaticMesh* MainHand, FName MainSocket, UStaticMesh* OffHand, FName OffSocket)
{
	auto Apply = [this](UStaticMeshComponent* Component, UStaticMesh* Mesh, FName Socket)
	{
		if (Component == nullptr)
		{
			return;
		}
		const bool bValid = Mesh != nullptr && Body != nullptr && !Socket.IsNone() && Body->DoesSocketExist(Socket);
		if (!bValid)
		{
			Component->SetStaticMesh(nullptr);
			Component->SetVisibility(false);
			return;
		}
		if (Component->GetStaticMesh() != Mesh)
		{
			Component->SetStaticMesh(Mesh);
		}
		if (Component->GetAttachSocketName() != Socket || Component->GetAttachParent() != Body)
		{
			Component->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
		}
		Component->SetVisibility(true);
		ApplyCpdTo(Component);
	};
	Apply(MainHandMesh, MainHand != nullptr ? MainHand : DefaultMainHand.Get(), MainSocket.IsNone() ? DefaultMainSocket : MainSocket);
	Apply(OffHandMesh, OffHand != nullptr ? OffHand : DefaultOffHand.Get(), OffSocket.IsNone() ? DefaultOffSocket : OffSocket);
}

void AAbyssCharacterActor::SetElite(bool bInElite, const TArray<FLinearColor>& InAffixColors, UStaticMesh* InCrownMesh,
	UStaticMesh* InAuraMesh, UMaterialInterface* AuraMaterial)
{
	bElite = bInElite;
	AffixColors = InAffixColors;
	if (bElite && InCrownMesh != nullptr && CrownMesh == nullptr)
	{
		CrownMesh = NewObject<UStaticMeshComponent>(this, TEXT("EliteCrown"));
		AbyssCharacterPrivate::ConfigureAttachmentMesh(CrownMesh);
		CrownMesh->SetCastShadow(false);
		CrownMesh->SetupAttachment(VisualRoot);
		CrownMesh->RegisterComponent();
	}
	if (CrownMesh != nullptr)
	{
		CrownMesh->SetStaticMesh(InCrownMesh);
		const FName Overhead = AbyssAnchorSocketName(EAbyssAnchor::Overhead);
		if (Body != nullptr && Body->DoesSocketExist(Overhead))
		{
			CrownMesh->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Overhead);
		}
		else
		{
			CrownMesh->AttachToComponent(VisualRoot, FAttachmentTransformRules::KeepRelativeTransform);
			CrownMesh->SetRelativeLocation(FVector(0.f, 0.f, VisualHeightCm + 25.f));
		}
		CrownMesh->SetVisibility(bElite && InCrownMesh != nullptr);
	}

	const bool bWantAura = bElite && AffixColors.Num() > 0 && InAuraMesh != nullptr && AuraMaterial != nullptr;
	if (bWantAura && AuraMesh == nullptr)
	{
		AuraMesh = NewObject<UStaticMeshComponent>(this, TEXT("AffixAura"));
		AbyssCharacterPrivate::ConfigureAttachmentMesh(AuraMesh);
		AuraMesh->SetCastShadow(false);
		AuraMesh->SetupAttachment(SceneRoot);
		AuraMesh->RegisterComponent();
	}
	if (AuraMesh != nullptr)
	{
		if (bWantAura)
		{
			AuraMesh->SetStaticMesh(InAuraMesh);
			AuraMesh->SetMaterial(0, AuraMaterial);
			AuraMesh->SetRelativeLocation(FVector(0.f, 0.f, 2.f));
		}
		AuraMesh->SetVisibility(bWantAura);
	}
}

void AAbyssCharacterActor::SpawnAfterimages(const FVector& From, const FVector& To, UMaterialInterface* GhostMaterial)
{
	if (!bHasArt || Body == nullptr || Body->GetSkeletalMeshAsset() == nullptr || GhostMaterial == nullptr)
	{
		return;
	}
	static constexpr float Fractions[] = { 1.f / 3.f, 2.f / 3.f };
	static constexpr float Alphas[] = { 0.28f, 0.21f };
	static constexpr float Scales[] = { 1.0f, 1.04f };
	while (Afterimages.Num() < 2)
	{
		UPoseableMeshComponent* Ghost = NewObject<UPoseableMeshComponent>(this);
		Ghost->SetUsingAbsoluteLocation(true);
		Ghost->SetUsingAbsoluteRotation(true);
		Ghost->SetUsingAbsoluteScale(true);
		Ghost->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ghost->SetCastShadow(false);
		Ghost->SetupAttachment(SceneRoot);
		Ghost->RegisterComponent();
		Afterimages.Add(Ghost);
		AfterimageAge.Add(-1.f);
		AfterimageAlpha.Add(0.f);
	}
	for (int32 Index = 0; Index < 2; ++Index)
	{
		UPoseableMeshComponent* Ghost = Afterimages[Index];
		if (Ghost->GetSkinnedAsset() != Body->GetSkeletalMeshAsset())
		{
			Ghost->SetSkinnedAssetAndUpdate(Body->GetSkeletalMeshAsset(), true);
		}
		for (int32 Slot = 0; Slot < Ghost->GetNumMaterials(); ++Slot)
		{
			Ghost->SetMaterial(Slot, GhostMaterial);
		}
		const FVector Location = FMath::Lerp(From, To, Fractions[Index]);
		Ghost->SetWorldLocationAndRotation(FVector(Location.X, Location.Y, Body->GetComponentLocation().Z), Body->GetComponentQuat());
		Ghost->SetWorldScale3D(Body->GetComponentScale() * Scales[Index]);
		Ghost->CopyPoseFromSkeletalComponent(Body);
		Ghost->SetCustomPrimitiveDataVector4(AbyssCpd::TintR, FVector4(SpiritColor.R, SpiritColor.G, SpiritColor.B, 1.f));
		Ghost->SetCustomPrimitiveDataFloat(AbyssCpd::Ghost, Alphas[Index]);
		Ghost->SetVisibility(true);
		AfterimageAge[Index] = 0.f;
		AfterimageAlpha[Index] = Alphas[Index];
	}
}

void AAbyssCharacterActor::HandleAnimNotify(FName NotifyName)
{
	OnCharacterNotify.Broadcast(this, NotifyName);
}

void AAbyssCharacterActor::SetCpd(int32 Index, float Value)
{
	if (!FMath::IsNearlyEqual(CpdValues[Index], Value, 0.0005f))
	{
		CpdValues[Index] = Value;
		bCpdDirty = true;
	}
}

void AAbyssCharacterActor::SetCpdColor(int32 Index, const FLinearColor& Color)
{
	SetCpd(Index, Color.R);
	SetCpd(Index + 1, Color.G);
	SetCpd(Index + 2, Color.B);
}

void AAbyssCharacterActor::ApplyCpdTo(UPrimitiveComponent* Component) const
{
	if (Component == nullptr)
	{
		return;
	}
	for (int32 Index = 0; Index < AbyssCpd::Count; Index += 4)
	{
		Component->SetCustomPrimitiveDataVector4(Index,
			FVector4(CpdValues[Index], CpdValues[Index + 1], CpdValues[Index + 2], CpdValues[Index + 3]));
	}
}

FVector AAbyssCharacterActor::GetAnchorLocation(EAbyssAnchor Anchor) const
{
	const FName Socket = AbyssAnchorSocketName(Anchor);
	if (Body != nullptr && bHasArt && Body->DoesSocketExist(Socket))
	{
		return Body->GetSocketLocation(Socket);
	}
	const FVector Base = VisualRoot != nullptr ? VisualRoot->GetComponentLocation() : GetActorLocation();
	switch (Anchor)
	{
	case EAbyssAnchor::Feet: return Base;
	case EAbyssAnchor::Chest: return Base + FVector(0.f, 0.f, VisualHeightCm * 0.55f);
	case EAbyssAnchor::Head: return Base + FVector(0.f, 0.f, VisualHeightCm * 0.85f);
	case EAbyssAnchor::Overhead: return Base + FVector(0.f, 0.f, VisualHeightCm + 25.f);
	case EAbyssAnchor::HandR:
	case EAbyssAnchor::HandL: return Base + FVector(0.f, 0.f, VisualHeightCm * 0.5f);
	}
	return Base;
}

// =====================================================================================================================
// Per frame
// =====================================================================================================================

void AAbyssCharacterActor::SnapTo(const FVector& Location, const FVector2D& Facing)
{
	SetActorLocation(Location);
	if (!Facing.IsNearlyZero())
	{
		CurrentYawDeg = FMath::RadiansToDegrees(FMath::Atan2(Facing.Y, Facing.X));
		bHasYaw = true;
		SetActorRotation(FRotator(0.f, CurrentYawDeg, 0.f));
	}
}

void AAbyssCharacterActor::UpdateFacing(const FAbyssCharacterFrame& Frame, float DeltaSec)
{
	using namespace AbyssCharacterPrivate;
	if (bDying)
	{
		return;
	}
	FVector2D Desired = Frame.Facing;
	if (ActionState.bFaceLock && ActionState.Action != EAbyssCharacterAction::None)
	{
		Desired = ActionState.FaceDir;
	}
	if (Desired.IsNearlyZero())
	{
		return;
	}
	const float DesiredYaw = FMath::RadiansToDegrees(FMath::Atan2(Desired.Y, Desired.X));
	if (!bHasYaw)
	{
		CurrentYawDeg = DesiredYaw;
		bHasYaw = true;
	}
	else
	{
		const float Delta = FMath::FindDeltaAngleDegrees(CurrentYawDeg, DesiredYaw);
		if (FMath::Abs(Delta) < YawDeadbandDeg)
		{
			return;
		}
		CurrentYawDeg = FMath::FixedTurn(CurrentYawDeg, DesiredYaw, TurnRateDegPerSec * DeltaSec);
	}
	SetActorRotation(FRotator(0.f, CurrentYawDeg, 0.f));
}

void AAbyssCharacterActor::UpdateLocomotion(const FAbyssCharacterFrame& Frame)
{
	using namespace AbyssCharacterPrivate;
	UAbyssAnimInstance* Anim = GetAnim();
	if (Anim == nullptr || bDying || ActionState.Action != EAbyssCharacterAction::None)
	{
		return;
	}
	const FAbyssArtClip* Chosen = nullptr;
	float Rate = 1.f;
	const char* State = "idle";
	if (Frame.bMoving && Frame.SpeedCmS > MinMovingSpeedCmS)
	{
		// Pick the locomotion clip whose authored ground speed is closest (log ratio), then match the stride (R9).
		const FAbyssArtClip* Run = FindArtClip(FName(TEXT("Run")));
		const FAbyssArtClip* Walk = FindArtClip(FName(TEXT("Walk")));
		if (Run != nullptr && Walk != nullptr && Run->RefSpeedCmS > 0.f && Walk->RefSpeedCmS > 0.f)
		{
			const float RunScore = FMath::Abs(FMath::Loge(Frame.SpeedCmS / Run->RefSpeedCmS));
			const float WalkScore = FMath::Abs(FMath::Loge(Frame.SpeedCmS / Walk->RefSpeedCmS));
			Chosen = WalkScore < RunScore ? Walk : Run;
		}
		else
		{
			Chosen = Run != nullptr ? Run : Walk;
		}
		if (Chosen != nullptr && Chosen->RefSpeedCmS > 0.f)
		{
			Rate = FMath::Clamp(Frame.SpeedCmS / Chosen->RefSpeedCmS, 0.4f, 2.5f);
		}
		State = "walk";
	}
	if (Chosen == nullptr)
	{
		if (EntityKind == abyss::EntityKind::Npc && Frame.bTalking)
		{
			Chosen = FindArtClip(FName(TEXT("Talk")));
		}
		if (Chosen == nullptr && EntityKind == abyss::EntityKind::Npc)
		{
			Chosen = FindArtClip(FName(TEXT("IdleNPC")));
		}
		if (Chosen == nullptr)
		{
			Chosen = FindArtClip(FName(TEXT("Idle")));
		}
		Rate = Frame.bExhausted ? 0.6f : 1.f;
		State = "idle";
	}
	if (Chosen == nullptr)
	{
		return;
	}
	if (LocomotionClip != Chosen->Name || Anim->GetCurrentSequence() != FindClip(Chosen->Name))
	{
		PlayClip(*Chosen, Rate, 0.f, State);
		LocomotionClip = Chosen->Name;
	}
	else
	{
		Anim->SetPlayRate(Rate);
	}
}

void AAbyssCharacterActor::UpdateRecoil(float DeltaSec)
{
	if (RecoilElapsedSec < 0.f)
	{
		return;
	}
	RecoilElapsedSec += DeltaSec;
	const float SnapSec = HurtDurationMs * 0.3f / 1000.f;
	const float BackSec = HurtDurationMs * 0.62f / 1000.f;
	if (RecoilElapsedSec >= SnapSec + BackSec)
	{
		RecoilElapsedSec = -1.f;
	}
}

void AAbyssCharacterActor::UpdateDodgeOffset(float DeltaSec)
{
	if (DodgeElapsedSec < 0.f)
	{
		return;
	}
	DodgeElapsedSec += DeltaSec;
	if (DodgeElapsedSec >= DodgeTotalSec)
	{
		DodgeElapsedSec = -1.f;
		DodgeOffset = FVector::ZeroVector;
	}
}

void AAbyssCharacterActor::UpdateDeath(float DeltaSec)
{
	if (!bDying)
	{
		return;
	}
	DeathElapsedSec += DeltaSec;
	if (!bDeathFinished && DeathElapsedSec >= DeathFadeDelaySec + DeathFadeSec)
	{
		bDeathFinished = true;
		if (!bHeroDeath)
		{
			SetActorHiddenInGame(true);
		}
	}
}

void AAbyssCharacterActor::UpdateMaterialFeedback(const FAbyssCharacterFrame& Frame, float DeltaSec)
{
	using namespace AbyssCharacterPrivate;
	FlashRemainingSec = FMath::Max(0.f, FlashRemainingSec - DeltaSec);
	PainRemainingSec = FMath::Max(0.f, PainRemainingSec - DeltaSec);
	SetCpd(AbyssCpd::HitFlash, FlashRemainingSec > 0.f ? 1.f : 0.f);
	SetCpdColor(AbyssCpd::PainTintR, PainTintColor);
	SetCpd(AbyssCpd::PainTintAmount, PainRemainingSec > 0.f && FlashRemainingSec <= 0.f ? 1.f : 0.f);

	// Wind-up telegraph: ramps over the first 62 % of the contact time, cut at contact (combat-feel.md 10.3).
	float Telegraph = 0.f;
	if (ActionState.TelegraphRemainingSec > 0.f && ActionState.TelegraphTotalSec > 0.f)
	{
		ActionState.TelegraphRemainingSec = FMath::Max(0.f, ActionState.TelegraphRemainingSec - DeltaSec);
		Telegraph = 1.f - ActionState.TelegraphRemainingSec / ActionState.TelegraphTotalSec;
	}
	else if (Frame.bWindingUp)
	{
		Telegraph = 1.f;
	}
	SetCpdColor(AbyssCpd::TelegraphR, (bElite || Frame.bElite) ? TelegraphElite() : TelegraphNormal());
	SetCpd(AbyssCpd::TelegraphAmount, Telegraph > 0.f ? FMath::Clamp(Telegraph, 0.f, 1.f) * 0.85f : 0.f);

	// Status tints (R8).
	FLinearColor Tint = FLinearColor::Black;
	float TintAmount = 0.f;
	for (const FStatusTint& Status : StatusTints)
	{
		if (HasStatus(Frame.StatusMask, Status.Type))
		{
			Tint = AbyssWorldUtil::ColorFromRgb(Status.Rgb);
			TintAmount = Status.Alpha;
			break;
		}
	}
	if (TintAmount > 0.f)
	{
		SetCpdColor(AbyssCpd::TintR, Tint);
	}
	SetCpd(AbyssCpd::TintAmount, TintAmount);

	// Death fade.
	float Fade = 0.f;
	if (bDying && DeathElapsedSec > DeathFadeDelaySec)
	{
		const float T = DeathFadeSec > 0.f ? FMath::Clamp((DeathElapsedSec - DeathFadeDelaySec) / DeathFadeSec, 0.f, 1.f) : 1.f;
		Fade = bHeroDeath ? 0.88f * T : T;
	}
	SetCpd(AbyssCpd::Fade, Fade);

	if (bCpdDirty)
	{
		ApplyCpdTo(Body);
		ApplyCpdTo(MainHandMesh);
		ApplyCpdTo(OffHandMesh);
		ApplyCpdTo(CrownMesh);
		bCpdDirty = false;
	}
}

void AAbyssCharacterActor::UpdateEliteVisuals(float DeltaSec)
{
	using namespace AbyssCharacterPrivate;
	if (!bElite)
	{
		return;
	}
	EliteTimeSec += DeltaSec;
	if (CrownMesh != nullptr && CrownMesh->IsVisible())
	{
		const float Bob = FMath::Sin(EliteTimeSec * 2.f * UE_PI / AuraPeriodSec) * 3.f;
		const FName Overhead = AbyssAnchorSocketName(EAbyssAnchor::Overhead);
		const float BaseZ = CrownMesh->GetAttachSocketName() == Overhead ? 0.f : VisualHeightCm + 25.f;
		CrownMesh->SetRelativeLocation(FVector(0.f, 0.f, BaseZ + Bob));
		CrownMesh->SetWorldRotation(FRotator(0.f, FMath::Fmod(EliteTimeSec * CrownSpinDegPerSec, 360.f), 0.f));
	}
	if (AuraMesh != nullptr && AuraMesh->IsVisible() && AffixColors.Num() > 0)
	{
		// 1200 ms yoyo: scale 1 -> 1.2, alpha .25 -> .1; one affix colour per full cycle.
		const float Cycle = EliteTimeSec / (2.f * AuraPeriodSec);
		const int32 ColorIndex = static_cast<int32>(FMath::FloorToFloat(Cycle)) % AffixColors.Num();
		const float Phase = FMath::Abs(FMath::Fmod(EliteTimeSec, 2.f * AuraPeriodSec) / AuraPeriodSec - 1.f);   // 1 -> 0 -> 1
		const float Yoyo = 1.f - Phase;
		const float Width = FMath::Max(120.f, BlobRadiusCm * 3.f) / 100.f * (1.f + 0.2f * Yoyo);
		AuraMesh->SetRelativeScale3D(FVector(Width, Width, 1.f));
		const FLinearColor& Color = AffixColors[ColorIndex];
		AuraMesh->SetCustomPrimitiveDataVector4(AbyssCpd::TintR, FVector4(Color.R, Color.G, Color.B, FMath::Lerp(0.25f, 0.1f, Yoyo)));
	}
}

void AAbyssCharacterActor::UpdateAfterimages(float DeltaSec)
{
	for (int32 Index = 0; Index < Afterimages.Num(); ++Index)
	{
		if (AfterimageAge[Index] < 0.f || Afterimages[Index] == nullptr)
		{
			continue;
		}
		AfterimageAge[Index] += DeltaSec;
		const float T = FMath::Clamp(AfterimageAge[Index] / AbyssCharacterPrivate::AfterimageLifeSec, 0.f, 1.f);
		const float Alpha = AfterimageAlpha[Index] * (1.f - FMath::Pow(T, 1.3f));
		Afterimages[Index]->SetCustomPrimitiveDataFloat(AbyssCpd::Ghost, Alpha);
		if (T >= 1.f)
		{
			Afterimages[Index]->SetVisibility(false);
			AfterimageAge[Index] = -1.f;
		}
	}
}

void AAbyssCharacterActor::PresentFrame(const FAbyssCharacterFrame& Frame, const FAbyssFrameInfo& Info, float VisualDeltaSec,
	float RealDeltaSec)
{
	if (!bHasArt)
	{
		SetActorLocation(Frame.Location);
		return;
	}

	// ---- hit-stop: this actor's animation and its own procedural motion pause (real time) ----
	if (FreezeRemainingMs > 0.f)
	{
		FreezeRemainingMs -= RealDeltaSec * 1000.f;
		if (FreezeRemainingMs <= 0.f)
		{
			FreezeRemainingMs = 0.f;
		}
	}
	// ---- frozen world (classes-stats-skills 19.1, D13): an attack / cast / signature whose Contact / Release the core
	// is still waiting for holds its pose (play rate 0) until the world unfreezes, so the hit lands on the notify ----
	const bool bBeatAction = ActionState.Action == EAbyssCharacterAction::Attack || ActionState.Action == EAbyssCharacterAction::Cast
		|| ActionState.Action == EAbyssCharacterAction::Signature;
	bWorldHold = Info.bFrozen && !bDying && bBeatAction && ActionBeatSimMs > 0.0 && Info.RenderSimMs < ActionBeatSimMs;
	const bool bHitStop = FreezeRemainingMs > 0.f;
	if (UAbyssAnimInstance* Anim = GetAnim())
	{
		const bool bAnimFrozen = bHitStop || bWorldHold;
		if (Anim->IsFrozen() != bAnimFrozen)
		{
			Anim->SetFrozen(bAnimFrozen);
		}
	}
	const bool bFrozen = bHitStop || bWorldHold;
	const float ActionDelta = bFrozen ? 0.f : VisualDeltaSec;

	// ---- root ----
	if (!GetActorLocation().Equals(Frame.Location, 0.01))
	{
		SetActorLocation(Frame.Location);
	}
	UpdateFacing(Frame, ActionDelta);

	// ---- actions / locomotion ----
	if (ActionState.Action != EAbyssCharacterAction::None && ActionState.Action != EAbyssCharacterAction::Death)
	{
		ActionState.RemainingSec -= ActionDelta;
		if (ActionState.RemainingSec <= 0.f)
		{
			ActionState.Action = EAbyssCharacterAction::None;
			ActionState.bFaceLock = false;
		}
	}
	if (!bDying && !Frame.bAlive)
	{
		// Dead without a death anim (state restored from a snapshot): stay hidden.
		SetActorHiddenInGame(true);
	}
	if (Info.bFrozen && Frame.bMoving)
	{
		// 19.1: locomotion stops in a frozen world (the core clears the hero's path); nobody runs in place - idle loops
		// keep playing on presentation time.
		FAbyssCharacterFrame Idle = Frame;
		Idle.bMoving = false;
		Idle.SpeedCmS = 0.f;
		UpdateLocomotion(Idle);
	}
	else
	{
		UpdateLocomotion(Frame);
	}

	// ---- procedural offsets on the visual root ----
	UpdateRecoil(ActionDelta);
	UpdateDodgeOffset(VisualDeltaSec);
	UpdateDeath(ActionDelta);

	FVector WorldOffset = FVector::ZeroVector;
	FQuat WorldTilt = FQuat::Identity;
	FVector Squash = FVector::OneVector;
	float Lift = HoverCm;
	if (bFlying)
	{
		Lift += FMath::Sin(Info.RenderSimMs * 0.003) * 4.f;
	}
	if (RecoilElapsedSec >= 0.f)
	{
		const float SnapSec = HurtDurationMs * 0.3f / 1000.f;
		const float BackSec = HurtDurationMs * 0.62f / 1000.f;
		float Factor = 0.f;
		FVector ScaleNow = FVector::OneVector;
		const float Q = FMath::Min(0.2f, 0.1f * RecoilStrength);
		const FVector SnapScale(1.f + Q, 1.f + Q, 1.f - 0.8f * Q);
		const FVector ReboundScale(0.94f, 0.94f, 1.06f);
		if (RecoilElapsedSec < SnapSec)
		{
			const float T = AbyssWorldUtil::EaseOutExpo(RecoilElapsedSec / FMath::Max(SnapSec, 0.001f));
			Factor = FMath::Lerp(1.f, 0.7f, T);
			ScaleNow = FMath::Lerp(SnapScale, ReboundScale, T);
		}
		else
		{
			const float T = AbyssWorldUtil::EaseOutCubic(FMath::Clamp((RecoilElapsedSec - SnapSec) / FMath::Max(BackSec, 0.001f), 0.f, 1.f));
			Factor = FMath::Lerp(0.7f, 0.f, T);
			ScaleNow = FMath::Lerp(ReboundScale, FVector::OneVector, T);
		}
		WorldOffset += RecoilDir * (RecoilPeakCm * Factor);
		Lift -= 2.f * AbyssWorldUtil::VfxPxToCm * Factor * 0.25f;
		const float TiltDeg = 5.f * FMath::Min(1.6f, RecoilStrength) * Factor;
		WorldTilt = FQuat(FVector::CrossProduct(FVector::UpVector, RecoilDir).GetSafeNormal(), FMath::DegreesToRadians(TiltDeg));
		Squash = ScaleNow;
	}
	if (DodgeElapsedSec >= 0.f && DodgeTotalSec > 0.f)
	{
		const float T = AbyssWorldUtil::EaseOutQuad(FMath::Clamp(DodgeElapsedSec / DodgeTotalSec, 0.f, 1.f));
		WorldOffset += DodgeOffset * (1.f - T);
	}
	if (bDying)
	{
		if (DeathThrowCm > 0.f && DeathAirSec > 0.f)
		{
			const float T = FMath::Clamp(DeathElapsedSec / DeathAirSec, 0.f, 1.f);
			WorldOffset += DeathThrowDir * (DeathThrowCm * AbyssWorldUtil::EaseOutQuad(T));
			const float Hop = T < 0.4f ? AbyssWorldUtil::EaseOutQuad(T / 0.4f) : 1.f - AbyssWorldUtil::EaseInQuad((T - 0.4f) / 0.6f);
			Lift += DeathHopCm * Hop;
			const FVector Axis = FVector::CrossProduct(FVector::UpVector, DeathThrowDir).GetSafeNormal();
			if (!Axis.IsNearlyZero())
			{
				WorldTilt = FQuat(Axis, FMath::DegreesToRadians(DeathSpinDeg * AbyssWorldUtil::EaseOutQuad(T))) * WorldTilt;
			}
		}
		else if (DeathElapsedSec > DeathFadeDelaySec)
		{
			// sink 5 px while fading
			const float T = DeathFadeSec > 0.f ? FMath::Clamp((DeathElapsedSec - DeathFadeDelaySec) / DeathFadeSec, 0.f, 1.f) : 1.f;
			Lift -= 5.f * AbyssWorldUtil::VfxPxToCm * T;
		}
	}
	const FQuat RootQuat = GetActorQuat();
	const FVector LocalOffset = RootQuat.UnrotateVector(WorldOffset) + FVector(0.f, 0.f, Lift);
	VisualRoot->SetRelativeLocation(LocalOffset);
	VisualRoot->SetRelativeRotation(RootQuat.Inverse() * WorldTilt * RootQuat);
	VisualRoot->SetRelativeScale3D(Squash);

	// ---- blob shadow follows the body on the ground, shrinking with height (k = max(0.45, 1 - lift / 24 px)) ----
	if (BlobShadow != nullptr && BlobShadow->IsVisible())
	{
		const float LiftPx = FMath::Max(0.f, Lift) / AbyssWorldUtil::VfxPxToCm;
		const float K = FMath::Max(0.45f, 1.f - LiftPx / 24.f);
		const float Diameter = BlobRadiusCm * 2.f / 100.f * K;
		BlobShadow->SetRelativeLocation(FVector(LocalOffset.X, LocalOffset.Y, 1.5f));
		BlobShadow->SetRelativeScale3D(FVector(Diameter, Diameter, 1.f));
		BlobShadow->SetCustomPrimitiveDataFloat(AbyssCpd::Fade, CpdValues[AbyssCpd::Fade]);
	}

	UpdateMaterialFeedback(Frame, VisualDeltaSec);
	UpdateEliteVisuals(VisualDeltaSec);
	UpdateAfterimages(VisualDeltaSec);
}
