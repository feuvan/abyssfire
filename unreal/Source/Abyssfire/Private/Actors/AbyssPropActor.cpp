#include "Actors/AbyssPropActor.h"

#include "Abyssfire.h"
#include "Animation/AnimSequence.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

#include "World/AbyssAssetLibrary.h"

namespace AbyssPropPrivate
{
	// Web px -> cm for upright motion (world-map-nav.md 1.3: x 2.552).
	constexpr float UprightPxToCm = 2.552f;

	void ConfigureMesh(UPrimitiveComponent* Component)
	{
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
	}

	struct FBobStyle
	{
		float AmplitudeCm = 0.f;
		float PeriodSec = 1.f;
		float RockDeg = 0.f;
		float SpinDegS = 0.f;
	};

	FBobStyle BobFor(EAbyssPropStyle Style)
	{
		switch (Style)
		{
		case EAbyssPropStyle::Loot: return { 5.f * UprightPxToCm, 1.6f, 0.f, 0.f };     // 800 ms yoyo = 1.6 s cycle
		case EAbyssPropStyle::Potion: return { 5.f * UprightPxToCm, 1.2f, 0.f, 0.f };   // 600 ms yoyo
		case EAbyssPropStyle::Pickup: return { 6.f * UprightPxToCm, 1.8f, 8.f, 0.f };   // 900 ms yoyo, rock +-8 deg
		case EAbyssPropStyle::Ghost: return { 6.f * UprightPxToCm, 2.8f, 0.f, 0.f };    // 1400 ms yoyo
		default: return {};
		}
	}
}

AAbyssPropActor::AAbyssPropActor()
{
	using namespace AbyssPropPrivate;
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Visual"));
	VisualRoot->SetupAttachment(SceneRoot);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(VisualRoot);
	ConfigureMesh(Mesh);
	Mesh->SetCastShadow(false);

	SkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
	SkeletalMesh->SetupAttachment(VisualRoot);
	ConfigureMesh(SkeletalMesh);
	SkeletalMesh->SetCastShadow(false);
	SkeletalMesh->SetVisibility(false);
	SkeletalMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	Pool = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pool"));
	Pool->SetupAttachment(SceneRoot);
	ConfigureMesh(Pool);
	Pool->SetCastShadow(false);
	Pool->SetReceivesDecals(false);
	Pool->SetVisibility(false);
	Pool->SetTranslucentSortPriority(-5);
}

bool AAbyssPropActor::InitProp(const FAbyssPropSetup& Setup, UAbyssAssetLibrary& Assets)
{
	using namespace AbyssPropPrivate;
	EntityId = Setup.Id;
	EntityKind = Setup.Kind;
	Style = Setup.Style;
	PhaseSec = Setup.PhaseSec;
	BaseScale = FMath::Max(0.05f, Setup.Scale);
	MeshYawOffsetDeg = Assets.GetManifest().GetShading().MeshYawOffsetDeg;
	bHasArt = false;
	bSkeletal = false;
	for (float& Value : CpdValues)
	{
		Value = 0.f;
	}
	bCpdDirty = true;

	const FAbyssArtAsset* Art = Setup.Art;
	const FRotator BodyRotation(0.f, Setup.YawDeg + MeshYawOffsetDeg, 0.f);
	if (Art != nullptr && Art->IsSkeletal())
	{
		if (USkeletalMesh* Skel = Assets.LoadSkeletalMesh(*Art))
		{
			bSkeletal = true;
			SkeletalMesh->SetSkeletalMeshAsset(Skel);
			SkeletalMesh->SetRelativeRotation(BodyRotation);
			SkeletalMesh->SetRelativeScale3D(FVector(FMath::Max(0.01f, Art->Scale) * BaseScale));
			SkeletalMesh->SetCastShadow(Setup.bCastShadow);
			SkeletalMesh->SetVisibility(true);
			Mesh->SetStaticMesh(nullptr);
			Mesh->SetVisibility(false);
			Clips.Reset();
			for (const FAbyssArtClip& Clip : Art->Clips)
			{
				if (!Clip.bStaticPose)
				{
					if (UAnimSequence* Seq = Assets.LoadClip(Clip))
					{
						Clips.Add(Clip.Name, Seq);
					}
				}
			}
			UAnimSequence* Idle = nullptr;
			for (const TCHAR* Name : { TEXT("Idle"), TEXT("IdleNPC"), TEXT("Work") })
			{
				if (const TObjectPtr<UAnimSequence>* Found = Clips.Find(FName(Name)))
				{
					Idle = Found->Get();
					break;
				}
			}
			if (Idle != nullptr)
			{
				SkeletalMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
				SkeletalMesh->PlayAnimation(Idle, /*bLooping*/ true);
			}
			bHasArt = true;
		}
	}
	else if (Art != nullptr)
	{
		if (UStaticMesh* Static = Assets.LoadStaticMesh(*Art))
		{
			Mesh->SetStaticMesh(Static);
			Mesh->SetRelativeRotation(BodyRotation);
			Mesh->SetRelativeScale3D(FVector(FMath::Max(0.01f, Art->Scale) * BaseScale));
			Mesh->SetCastShadow(Setup.bCastShadow);
			Mesh->SetVisibility(true);
			SkeletalMesh->SetVisibility(false);
			bHasArt = true;
		}
	}
	if (!bHasArt)
	{
		Mesh->SetVisibility(false);
		SkeletalMesh->SetVisibility(false);
		UE_LOG(LogAbyss, Verbose, TEXT("Prop %u has no art (asset %s)"), EntityId,
			Art != nullptr ? *Art->Name.ToString() : TEXT("none"));
	}
	VisualHeightCm = Art != nullptr && Art->GetVisualHeightCm() > 1.f ? Art->GetVisualHeightCm() * BaseScale : 50.f * BaseScale;

	// Ghost (soul echo): every material slot uses the ghost material; colour + alpha from the custom data.
	if (Style == EAbyssPropStyle::Ghost)
	{
		if (UMaterialInterface* Ghost = Assets.LoadMaterial(TEXT("M_AF_Ghost")))
		{
			UMeshComponent* Target = bSkeletal ? static_cast<UMeshComponent*>(SkeletalMesh.Get()) : static_cast<UMeshComponent*>(Mesh.Get());
			for (int32 Slot = 0; Slot < Target->GetNumMaterials(); ++Slot)
			{
				Target->SetMaterial(Slot, Ghost);
			}
		}
		GhostAlpha = 0.55f;
	}
	SetCpd(AbyssCpd::TintR, Setup.Tint.R);
	SetCpd(AbyssCpd::TintR + 1, Setup.Tint.G);
	SetCpd(AbyssCpd::TintR + 2, Setup.Tint.B);
	SetCpd(AbyssCpd::TintAmount, Setup.TintAmount);

	// Light pool.
	PoolRadiusCm = Setup.PoolRadiusCm;
	PoolColor = Setup.PoolColor;
	PoolAlphaMin = Setup.PoolAlphaMin;
	PoolAlphaMax = Setup.PoolAlphaMax;
	PoolPeriodSec = FMath::Max(0.1f, Setup.PoolPeriodSec);
	UStaticMesh* Quad = PoolRadiusCm > 0.f ? Assets.LoadContentMesh(TEXT("FX"), TEXT("SM_FX_Quad")) : nullptr;
	UMaterialInterface* PoolMaterial = PoolRadiusCm > 0.f ? Assets.LoadMaterial(TEXT("M_AF_LightPool")) : nullptr;
	if (Quad != nullptr && PoolMaterial != nullptr)
	{
		Pool->SetStaticMesh(Quad);
		Pool->SetMaterial(0, PoolMaterial);
		Pool->SetRelativeLocation(FVector(0.f, 0.f, 2.f));
		const float Diameter = PoolRadiusCm * 2.f / 100.f;
		Pool->SetRelativeScale3D(FVector(Diameter, Diameter, 1.f));
		Pool->SetCustomPrimitiveDataVector4(AbyssCpd::TintR, FVector4(PoolColor.R, PoolColor.G, PoolColor.B, PoolAlphaMax));
		Pool->SetVisibility(true);
	}
	else
	{
		Pool->SetVisibility(false);
	}

	FadeElapsedSec = 0.f;
	FadeTotalSec = 0.f;
	bFadeFinished = false;
	bOpened = false;
	OpenElapsedSec = -1.f;
	OcclusionFade = 0.f;
	SetActorHiddenInGame(false);
	ApplyCpd();
	return bHasArt;
}

void AAbyssPropActor::SetCpd(int32 Index, float Value)
{
	if (!FMath::IsNearlyEqual(CpdValues[Index], Value, 0.0005f))
	{
		CpdValues[Index] = Value;
		bCpdDirty = true;
	}
}

void AAbyssPropActor::ApplyCpd()
{
	if (!bCpdDirty)
	{
		return;
	}
	bCpdDirty = false;
	for (UPrimitiveComponent* Component : { static_cast<UPrimitiveComponent*>(Mesh.Get()), static_cast<UPrimitiveComponent*>(SkeletalMesh.Get()) })
	{
		if (Component == nullptr)
		{
			continue;
		}
		for (int32 Index = 0; Index < AbyssCpd::Count; Index += 4)
		{
			Component->SetCustomPrimitiveDataVector4(Index,
				FVector4(CpdValues[Index], CpdValues[Index + 1], CpdValues[Index + 2], CpdValues[Index + 3]));
		}
	}
}

void AAbyssPropActor::BeginCollect(float DurationSec)
{
	if (FadeTotalSec > 0.f)
	{
		return;
	}
	FadeTotalSec = FMath::Max(0.05f, DurationSec);
	FadeElapsedSec = 0.f;
	FadeRiseCm = Style == EAbyssPropStyle::Ghost ? 40.f : 25.f;
	if (Style == EAbyssPropStyle::Chest)
	{
		PlayOpen();
		FadeRiseCm = 0.f;
		FadeTotalSec = FMath::Max(FadeTotalSec, 1.2f);
	}
}

void AAbyssPropActor::BeginExpire(float DurationSec)
{
	if (FadeTotalSec > 0.f)
	{
		return;
	}
	FadeTotalSec = FMath::Max(0.05f, DurationSec);
	FadeElapsedSec = 0.f;
	FadeRiseCm = 0.f;
}

void AAbyssPropActor::PlayOpen()
{
	if (bOpened)
	{
		return;
	}
	bOpened = true;
	OpenElapsedSec = 0.f;
	if (bSkeletal)
	{
		if (const TObjectPtr<UAnimSequence>* Open = Clips.Find(FName(TEXT("Open"))))
		{
			SkeletalMesh->PlayAnimation(Open->Get(), /*bLooping*/ false);
		}
	}
}

void AAbyssPropActor::SetHighlight(float Amount)
{
	SetCpd(AbyssCpd::Highlight, FMath::Clamp(Amount, 0.f, 1.f));
}

void AAbyssPropActor::SetOcclusionFade(float Amount)
{
	OcclusionFade = FMath::Clamp(Amount, 0.f, 1.f);
}

FVector AAbyssPropActor::GetAnchorLocation(EAbyssAnchor Anchor) const
{
	const FVector Base = VisualRoot != nullptr ? VisualRoot->GetComponentLocation() : GetActorLocation();
	switch (Anchor)
	{
	case EAbyssAnchor::Feet: return GetActorLocation();
	case EAbyssAnchor::Chest: return Base + FVector(0.f, 0.f, VisualHeightCm * 0.55f);
	case EAbyssAnchor::Head: return Base + FVector(0.f, 0.f, VisualHeightCm * 0.85f);
	case EAbyssAnchor::Overhead: return Base + FVector(0.f, 0.f, VisualHeightCm + 25.f);
	case EAbyssAnchor::HandR:
	case EAbyssAnchor::HandL: return Base + FVector(0.f, 0.f, VisualHeightCm * 0.5f);
	}
	return Base;
}

void AAbyssPropActor::PresentFrame(const FVector& GroundLocation, float VisualDeltaSec, double TimeSec)
{
	using namespace AbyssPropPrivate;
	if (!GetActorLocation().Equals(GroundLocation, 0.01))
	{
		SetActorLocation(GroundLocation);
	}
	const double T = TimeSec + PhaseSec;
	const FBobStyle Bob = BobFor(Style);

	FVector Offset = FVector::ZeroVector;
	FRotator Rock = FRotator::ZeroRotator;
	if (Bob.AmplitudeCm > 0.f)
	{
		// Yoyo sine: 0 -> amplitude -> 0 over the period (web tweens are Sine in-out yoyo).
		const float Phase = static_cast<float>(FMath::Fmod(T, static_cast<double>(Bob.PeriodSec)) / Bob.PeriodSec);
		Offset.Z = Bob.AmplitudeCm * 0.5f * (1.f - FMath::Cos(2.f * UE_PI * Phase));
		if (Bob.RockDeg > 0.f)
		{
			Rock.Roll = Bob.RockDeg * FMath::Sin(2.f * UE_PI * Phase);
		}
	}

	// Fades (collect: rise + fade; expire: fade).
	float Fade = OcclusionFade;
	if (FadeTotalSec > 0.f)
	{
		FadeElapsedSec += VisualDeltaSec;
		const float U = FMath::Clamp(FadeElapsedSec / FadeTotalSec, 0.f, 1.f);
		Offset.Z += FadeRiseCm * AbyssWorldUtil::EaseOutQuad(U);
		Fade = FMath::Max(Fade, Style == EAbyssPropStyle::Chest ? FMath::Clamp((U - 0.6f) / 0.4f, 0.f, 1.f) : U);
		if (U >= 1.f && !bFadeFinished)
		{
			bFadeFinished = true;
			SetActorHiddenInGame(true);
		}
	}
	SetCpd(AbyssCpd::Fade, Fade);

	// Static chest fallback: the "Open" is a small hop when there is no lid bone.
	if (bOpened && !bSkeletal && OpenElapsedSec >= 0.f)
	{
		OpenElapsedSec += VisualDeltaSec;
		const float U = FMath::Clamp(OpenElapsedSec / 0.4f, 0.f, 1.f);
		Offset.Z += 8.f * FMath::Sin(UE_PI * U);
	}

	if (Style == EAbyssPropStyle::Ghost)
	{
		// alpha .55 <-> .35 with the bob (1400 ms yoyo).
		const float Phase = static_cast<float>(FMath::Fmod(T, 2.8) / 2.8);
		GhostAlpha = FMath::Lerp(0.55f, 0.35f, 0.5f * (1.f - FMath::Cos(2.f * UE_PI * Phase)));
		SetCpd(AbyssCpd::Ghost, GhostAlpha * (1.f - Fade));
	}
	VisualRoot->SetRelativeLocationAndRotation(Offset, Rock);

	if (Pool != nullptr && Pool->IsVisible())
	{
		const float Phase = static_cast<float>(FMath::Fmod(T, static_cast<double>(PoolPeriodSec)) / PoolPeriodSec);
		const float Wave = 0.5f * (1.f - FMath::Cos(2.f * UE_PI * Phase));
		const float Alpha = FMath::Lerp(PoolAlphaMin, PoolAlphaMax, Wave) * (1.f - Fade);
		const float Diameter = PoolRadiusCm * 2.f / 100.f * FMath::Lerp(0.9f, 1.1f, Wave);
		Pool->SetRelativeScale3D(FVector(Diameter, Diameter, 1.f));
		Pool->SetCustomPrimitiveDataFloat(AbyssCpd::TintAmount, Alpha);
	}
	ApplyCpd();
}
