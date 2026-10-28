#include "Actors/AbyssCharacterActor.h"

#include "Abyssfire.h"
#include "Animation/AnimSequence.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"

#include <array>
#include <limits>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/data/CombatData.h"

#include "Actors/AbyssAnimActionRules.h"
#include "Actors/AbyssCorpseRules.h"
#include "Actors/AbyssMonsterLookRules.h"
#include "Anim/AbyssAnimInstance.h"
#include "Framework/AbyssText.h"
#include "Framework/AbyssUnits.h"
#include "Vfx/AbyssTelegraphRules.h"
#include "World/AbyssAssetLibrary.h"
#include "World/AbyssWorldStateRules.h"

namespace AbyssCharacterPrivate
{
	constexpr float TurnRateDegPerSec = 720.f;   // M11 (render-only), also used for the hero
	constexpr float YawDeadbandDeg = 2.f;
	// DECISIONS C14 (4): a hero attack / cast faces its aim before the beat - the turn ends by half the Contact / Release
	// offset and never takes longer than this (a 180 degree turn reads as a quick pivot, not a snap).
	constexpr float HeroAimTurnMaxSec = 0.1f;
	constexpr float HeroAimTurnBeatFraction = 0.5f;
	constexpr double SingleViewMirrorDeadband = 0.05;   // |screen-x| of a unit facing below which the mirror side holds
	constexpr float MinMovingSpeedCmS = 5.f;
	constexpr float CrownSpinDegPerSec = 45.f;
	constexpr float AuraPeriodSec = 1.2f;
	constexpr float AfterimageLifeSec = 0.26f;
	constexpr float JoltPainSec = 0.09f;

	// Render-only feedback colours (art-inventory-ch1.md 1.8 / R8; sRGB hex in the spec). The wind-up tint comes from the
	// zone mood (zone_moods.json port.windupTint, DR5: the march's crimson) through the frame; these are the web port's
	// tints every mood without an entry keeps (AbyssThemeRules::DefaultWindupNormal / Elite).
	FLinearColor TelegraphNormal() { return AbyssWorldUtil::ColorFromRgb(0xFFC4B0); }
	FLinearColor TelegraphElite() { return AbyssWorldUtil::ColorFromRgb(0xFF7A5C); }

	/**
	 * The manifest clip name of a K1 action (Roar, Taunt, Stun, Attack02, Cast02, Rise); NAME_None for the others. The
	 * names are made once (UpdateLocomotion asks for the hold / stun clip of every held or stunned monster each frame).
	 */
	FName NamedActionClip(abyss::AnimAction Action)
	{
		using FNamedClips = std::array<FName, abyss::EnumCount<abyss::AnimAction>()>;
		static const FNamedClips Names = []()
		{
			FNamedClips Out;
			for (size_t I = 0; I < Out.size(); ++I)
			{
				const std::string_view Name = AbyssAnimActionRules::NamedClip(static_cast<abyss::AnimAction>(I));
				Out[I] = Name.empty() ? FName(NAME_None) : FName(*AbyssText::ToFString(Name));
			}
			return Out;
		}();
		const size_t Index = static_cast<size_t>(Action);
		return Index < Names.size() ? Names[Index] : FName(NAME_None);
	}

	// Status tints (R8): AbyssWorldStateRules::StatusTints (the web port's six, priority order) and, 2G stage H, Ashnumb's
	// fallback multiply where its band is not drawn (CharacterStatusTint).

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

	// ---- blob shadow (DECISIONS P10 revised) ----
	constexpr float BlobAlphaUnderSunShadow = 0.7f;   // the sun CSM draws the cast shadow; the blob only grounds the feet

	// ---- selection rim (DECISIONS R16, WorldContract.md 7) ----
	constexpr int32 MaxRimmedCharacters = 3;
	constexpr float RimDepthPushCm = 30.f;
	/**
	 * Characters holding a rim (the budget). Every holder returns its slot on None / EndPlay; an actor destroyed without
	 * EndPlay drops out on the next acquire (weak pointer), so the budget never stays full.
	 */
	TArray<TWeakObjectPtr<const AAbyssCharacterActor>>& RimHolders()
	{
		static TArray<TWeakObjectPtr<const AAbyssCharacterActor>> Holders;
		return Holders;
	}

	struct FRimLook
	{
		uint32 Rgb = 0xFFF4DC;   // sRGB
		float Px720 = 0.f;       // width beyond the ink at 720 rows
	};

	FRimLook RimLookFor(EAbyssSelectionRim Kind)
	{
		switch (Kind)
		{
		case EAbyssSelectionRim::Hero: return { 0xFFF4DC, 2.f };
		case EAbyssSelectionRim::Target: return { 0xFF5A3C, 1.5f };
		case EAbyssSelectionRim::Hover: return { 0xFFB03C, 1.5f };
		case EAbyssSelectionRim::None: break;
		}
		return {};
	}

	/** An ink-hull slot material (M_AF_Outline / M_AF_Outline_Foliage or an instance of them). */
	bool IsInkHull(UMaterialInterface* Material)
	{
		static const FName OutlineName(TEXT("M_AF_Outline"));
		static const FName OutlineFoliageName(TEXT("M_AF_Outline_Foliage"));
		const UMaterial* Base = Material != nullptr ? Material->GetBaseMaterial() : nullptr;
		const FName BaseName = Base != nullptr ? Base->GetFName() : NAME_None;
		return BaseName == OutlineName || BaseName == OutlineFoliageName;
	}

	/** The ink-hull slots of a mesh component; none recognised -> slot 1 (the art manifest's hull slot, toon is slot 0). */
	TArray<int32, TInlineAllocator<4>> FindHullSlots(const UPrimitiveComponent& Source)
	{
		TArray<int32, TInlineAllocator<4>> Slots;
		const int32 NumSlots = Source.GetNumMaterials();
		for (int32 Slot = 0; Slot < NumSlots; ++Slot)
		{
			if (IsInkHull(Source.GetMaterial(Slot)))
			{
				Slots.Add(Slot);
			}
		}
		if (Slots.Num() == 0 && NumSlots >= 2)
		{
			Slots.Add(1);
		}
		return Slots;
	}

	/** OutlinePx1080 / BakedWidthCm of the source hull, so the rim widens exactly like that ink (MI defaults otherwise). */
	void CopyHullWidth(UMaterialInstanceDynamic& Rim, UMaterialInterface* SourceHull)
	{
		if (SourceHull == nullptr)
		{
			return;
		}
		for (const TCHAR* Name : { TEXT("OutlinePx1080"), TEXT("BakedWidthCm") })
		{
			const FName ParameterName(Name);
			float Value = 0.f;
			if (SourceHull->GetScalarParameterValue(FHashedMaterialParameterInfo(ParameterName), Value))
			{
				Rim.SetScalarParameterValue(ParameterName, Value);
			}
		}
	}

	// ---- dark remake U3: corpses and the ash look ----
	/** Rendered frames a frozen held pose is drawn before the body's tick stops (UpdateHeldPoseTick). */
	constexpr int32 HeldPoseSettleFrames = 3;
	/** The bone the husk's blob shadow follows while the body lies (manifest corpse `shadow`). */
	const FName& PelvisBone()
	{
		static const FName Name(TEXT("pelvis"));
		return Name;
	}

	/** The ash twin of a slot's master (M_AF_Toon -> M_AF_Toon_Ash, M_AF_Outline -> M_AF_Outline_Ash), else nullptr. */
	UMaterialInterface* AshTwinFor(UMaterialInterface* Material, UMaterialInterface* ToonAsh, UMaterialInterface* InkAsh)
	{
		static const FName ToonName(TEXT("M_AF_Toon"));
		static const FName OutlineName(TEXT("M_AF_Outline"));
		const UMaterial* Base = Material != nullptr ? Material->GetBaseMaterial() : nullptr;
		const FName BaseName = Base != nullptr ? Base->GetFName() : NAME_None;
		return BaseName == ToonName ? ToonAsh : (BaseName == OutlineName ? InkAsh : nullptr);
	}

	void ConfigureRimComponent(UPrimitiveComponent& Component)
	{
		Component.SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component.SetGenerateOverlapEvents(false);
		Component.SetCanEverAffectNavigation(false);
		Component.SetReceivesDecals(false);
		Component.SetCastShadow(false);
		Component.SetCastContactShadow(false);
		Component.SetVisibility(false);
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
	bSingleView = Setup.bSingleView;
	bSingleViewLeft = false;
	SingleViewCameraYawDeg = Setup.CameraYawDeg;
	MeshYawOffsetDeg = Assets.GetManifest().GetShading().MeshYawOffsetDeg;

	ClearAshnumbBand();   // 2G stage H: a re-initialised body starts on its own materials
	bBandNoSlots = false;
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
	BodyScale = AssetScale;
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
	ArtName = Art->Name;
	// Dark remake 2G stage H (K6): the hero's Ashnumb band twin, loaded with the hero (not at the first stack), and the
	// height the band measures (the art's heightCm: the pre-skinned, object-space z of the feet is 0).
	AshnumbMaster = nullptr;
	if (EntityKind == abyss::EntityKind::Hero)
	{
		AshnumbMaster = Assets.LoadMaterial(TEXT("M_AF_Toon_Ashnumb"));
		static bool bAshnumbLogged = false;
		UE_CLOG(AshnumbMaster == nullptr && !bAshnumbLogged, LogAbyss, Warning,
			TEXT("M_AF_Toon_Ashnumb not found (run Scripts/build_content.py): Ashnumb shows as the status tint"));
		bAshnumbLogged = bAshnumbLogged || AshnumbMaster == nullptr;
		BandHeightCm = Art->HeightCm > 1.f ? Art->HeightCm : FMath::Max(1.f, Art->GetVisualHeightCm());
	}
	// Dark remake U3: a death that holds its pose for the corpse layer (the world builder decided: AbyssCorpseRules).
	bCorpseHold = Setup.bCorpseHold && EntityKind != abyss::EntityKind::Hero && Setup.CorpseHoldFromMs > 0.f;
	CorpseHoldFromSec = Setup.CorpseHoldFromMs / 1000.f;
	HuskTopCm = Setup.HuskTopCm * AssetScale;   // authored at scale 1; the dissolve measures the body as drawn
	// Dark remake 2G: a death the art times itself (the Cinderbloat's fuse; every other asset keeps today's death).
	const AbyssAnimActionRules::FDeathRuntime Runtime =
		AbyssAnimActionRules::ParseDeathRuntime(Assets.GetManifest().GetAssetJson(Art->Name));
	bDeathRuntime = Runtime.bValid && EntityKind != abyss::EntityKind::Hero;
	DeathRuntimeMs = static_cast<float>(Runtime.DeathDurationMs);
	DeathRuntimeRate = static_cast<float>(Runtime.PlayRate);
	DeathFadeNotBeforeSec = static_cast<float>(Runtime.BodyFadeNotBeforeMs / 1000.0);
	bDeathRuntimeThrow = Runtime.bThrow;
	bDeathRuntimeHop = Runtime.bHop;
	bDeathRuntimeSpin = Runtime.bSpin;
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

	// Weapons: the class default attachments of the manifest (I6 swaps them for the equipped weapon type later). The main
	// and off hand by AbyssMonsterLookRules::ClassifyAttachments; any further attachment (dark remake U3: the Unburied's
	// kettle helm on `head`, which used to replace their weapon) is a held item of its own on its socket.
	DefaultMainHand = nullptr;
	DefaultOffHand = nullptr;
	DefaultMainSocket = FName(TEXT("weapon_r"));
	DefaultOffSocket = FName(TEXT("weapon_l"));
	std::vector<std::string> SocketNames;
	for (const FAbyssArtAttachment& Attachment : Art->Attachments)
	{
		SocketNames.push_back(Attachment.Socket.IsNone() ? std::string() : AbyssText::ToStd(Attachment.Socket.ToString()));
	}
	const std::vector<std::string_view> SocketViews(SocketNames.begin(), SocketNames.end());
	const AbyssMonsterLookRules::FAttachmentSlots Slots = AbyssMonsterLookRules::ClassifyAttachments(SocketViews);
	auto LoadAttachment = [&Assets, Art](int32 Index) -> UStaticMesh*
	{
		if (Index < 0 || Index >= Art->Attachments.Num())
		{
			return nullptr;
		}
		const FAbyssArtAsset* WeaponArt = Assets.GetManifest().FindAsset(Art->Attachments[Index].Object);
		return WeaponArt != nullptr ? Assets.LoadStaticMesh(*WeaponArt) : nullptr;
	};
	if (UStaticMesh* OffMesh = LoadAttachment(Slots.Off))
	{
		DefaultOffHand = OffMesh;
	}
	if (UStaticMesh* MainMesh = LoadAttachment(Slots.Main))
	{
		DefaultMainHand = MainMesh;
		const FName MainSocket = Art->Attachments[Slots.Main].Socket;
		DefaultMainSocket = MainSocket.IsNone() ? DefaultMainSocket : MainSocket;
	}
	TArray<TPair<UStaticMesh*, FName>> Extras;
	for (const int Extra : Slots.Extras)
	{
		if (UStaticMesh* ExtraMesh = LoadAttachment(Extra))
		{
			Extras.Emplace(ExtraMesh, Art->Attachments[Extra].Socket);
		}
	}
	MainHandMesh->SetCastShadow(Setup.bCastShadow);
	OffHandMesh->SetCastShadow(Setup.bCastShadow);
	SetWeaponMeshes(nullptr, NAME_None, nullptr, NAME_None);
	SetExtraItems(Extras);
	for (UStaticMeshComponent* Item : ExtraItems)
	{
		if (Item != nullptr)
		{
			Item->SetCastShadow(Setup.bCastShadow);
		}
	}

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
	if (RimBody != nullptr && RimBody->GetSkeletalMeshAsset() != Body->GetSkeletalMeshAsset())
	{
		SyncRimBody();
		SyncRimWeapons();
	}
	ResetPresentation();
	return true;
}

void AAbyssCharacterActor::SetShadowMode(bool bBlob, bool bCastDynamic)
{
	using namespace AbyssCharacterPrivate;
	bBlobWanted = bBlob;
	bBlobUnderSunShadow = bCastDynamic;
	if (BlobShadow != nullptr)
	{
		BlobShadow->SetVisibility(bBlob && bHasBlobAssets);
		// DECISIONS P10 revised: under the sun CSM the blob keeps 0.7 of its alpha (Fade 0.3), so the feet stay grounded
		// without doubling the cast shadow; PresentFrame folds the death fade in.
		BlobShadow->SetCustomPrimitiveDataFloat(AbyssCpd::Fade,
			1.f - (1.f - CpdValues[AbyssCpd::Fade]) * (bCastDynamic ? BlobAlphaUnderSunShadow : 1.f));
	}
	TArray<UPrimitiveComponent*, TInlineAllocator<6>> Casters = { Body.Get(), MainHandMesh.Get(), OffHandMesh.Get() };
	for (UStaticMeshComponent* Item : ExtraItems)
	{
		Casters.Add(Item);
	}
	for (UPrimitiveComponent* Component : Casters)
	{
		if (Component != nullptr)
		{
			Component->SetCastShadow(bCastDynamic);
			// Screen-space contact shadows (DECISIONS P10 revised: contact shadow length .03 on desktop balanced / high) seat
			// the feet and the held weapon where the CSM texels are too coarse.
			Component->SetCastContactShadow(true);
		}
	}
}

// =====================================================================================================================
// Selection rim (DECISIONS R16, WorldContract.md 7, contract C1)
// =====================================================================================================================

bool AAbyssCharacterActor::AcquireRimBudget()
{
	using namespace AbyssCharacterPrivate;
	if (bRimBudgetHeld)
	{
		return true;
	}
	TArray<TWeakObjectPtr<const AAbyssCharacterActor>>& Holders = RimHolders();
	Holders.RemoveAll([](const TWeakObjectPtr<const AAbyssCharacterActor>& Holder) { return !Holder.IsValid(); });
	if (Holders.Num() >= MaxRimmedCharacters)
	{
		UE_LOG(LogAbyss, Verbose, TEXT("Selection rim budget (%d) full: entity %u keeps no rim"), MaxRimmedCharacters, EntityId);
		return false;
	}
	Holders.Add(TWeakObjectPtr<const AAbyssCharacterActor>(this));
	bRimBudgetHeld = true;
	return true;
}

void AAbyssCharacterActor::ReleaseRimBudget()
{
	using namespace AbyssCharacterPrivate;
	if (bRimBudgetHeld)
	{
		const AAbyssCharacterActor* Self = this;
		RimHolders().RemoveAll([Self](const TWeakObjectPtr<const AAbyssCharacterActor>& Holder)
		{
			return !Holder.IsValid() || Holder.Get() == Self;
		});
		bRimBudgetHeld = false;
	}
}

void AAbyssCharacterActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RimKind = EAbyssSelectionRim::None;
	ReleaseRimBudget();
	Super::EndPlay(EndPlayReason);
}

void AAbyssCharacterActor::SetSelectionRim(EAbyssSelectionRim Kind)
{
	if (Kind == RimKind)
	{
		return;
	}
	if (Kind == EAbyssSelectionRim::None)
	{
		RimKind = EAbyssSelectionRim::None;
		ReleaseRimBudget();
		UpdateSelectionRimVisibility();
		return;
	}
	if (!AcquireRimBudget())
	{
		return;   // three rims are up; asked again next frame
	}
	if (!EnsureSelectionRim())
	{
		ReleaseRimBudget();
		return;
	}
	RimKind = Kind;
	ApplyRimLook();
	UpdateSelectionRimVisibility();
}

bool AAbyssCharacterActor::EnsureSelectionRim()
{
	using namespace AbyssCharacterPrivate;
	if (RimBody != nullptr)
	{
		if (Body != nullptr && RimBody->GetSkeletalMeshAsset() != Body->GetSkeletalMeshAsset())
		{
			SyncRimBody();
			SyncRimWeapons();
		}
		return true;
	}
	if (bRimUnavailable || !bHasArt || Body == nullptr || Body->GetSkeletalMeshAsset() == nullptr)
	{
		return false;
	}
	UAbyssAssetLibrary* Assets = UAbyssAssetLibrary::Get(this);
	RimParent = Assets != nullptr ? Assets->LoadMaterial(TEXT("MI_AF_SelectRim")) : nullptr;
	if (RimParent == nullptr && Assets != nullptr)
	{
		RimParent = Assets->LoadMaterial(TEXT("M_AF_SelectRim"));
	}
	if (RimParent == nullptr)
	{
		static bool bLogged = false;
		UE_CLOG(!bLogged, LogAbyss, Warning,
			TEXT("MI_AF_SelectRim not found (run Scripts/build_content.py): characters draw no selection rim"));
		bLogged = true;
		bRimUnavailable = true;
		return false;
	}
	RimMask = UMaterialInstanceDynamic::Create(RimParent, this);
	if (RimMask == nullptr)
	{
		bRimUnavailable = true;
		return false;
	}
	RimMask->SetScalarParameterValue(FName(TEXT("BodyMask")), 1.f);

	RimBody = NewObject<USkeletalMeshComponent>(this, TEXT("SelectionRim"));
	ConfigureRimComponent(*RimBody);
	// The follower is the body's shape: its bounds are the leader's (no bounds of its own to compute).
	RimBody->bUseBoundsFromLeaderPoseComponent = true;
	RimBody->SetupAttachment(Body);
	RimBody->RegisterComponent();

	RimMainHand = NewObject<UStaticMeshComponent>(this, TEXT("SelectionRimMainHand"));
	ConfigureRimComponent(*RimMainHand);
	RimMainHand->SetupAttachment(MainHandMesh);
	RimMainHand->RegisterComponent();

	RimOffHand = NewObject<UStaticMeshComponent>(this, TEXT("SelectionRimOffHand"));
	ConfigureRimComponent(*RimOffHand);
	RimOffHand->SetupAttachment(OffHandMesh);
	RimOffHand->RegisterComponent();

	bRimShown = false;
	SyncRimBody();
	SyncRimWeapons();
	ApplyCpdTo(RimBody);
	return true;
}

UMaterialInstanceDynamic* AAbyssCharacterActor::MakeRimMaterial(UMaterialInterface* SourceHull)
{
	UMaterialInstanceDynamic* Rim = RimParent != nullptr ? UMaterialInstanceDynamic::Create(RimParent, this) : nullptr;
	if (Rim != nullptr)
	{
		Rim->SetScalarParameterValue(FName(TEXT("BodyMask")), 0.f);
		AbyssCharacterPrivate::CopyHullWidth(*Rim, SourceHull);
	}
	return Rim;
}

void AAbyssCharacterActor::SyncRimBody()
{
	using namespace AbyssCharacterPrivate;
	if (RimBody == nullptr || Body == nullptr)
	{
		return;
	}
	USkeletalMesh* Mesh = Body->GetSkeletalMeshAsset();
	if (RimBody->GetSkeletalMeshAsset() != Mesh)
	{
		RimBody->SetSkeletalMeshAsset(Mesh);
	}
	RimBody->SetLeaderPoseComponent(Body);

	// Slot materials: the ink hull gets its rim instance (that hull's width), every other slot the BodyMask instance.
	const TArray<int32, TInlineAllocator<4>> HullSlots = FindHullSlots(*Body);
	RimBodyMaterials.Reset();
	const int32 NumSlots = Body->GetNumMaterials();
	for (int32 Slot = 0; Slot < NumSlots; ++Slot)
	{
		UMaterialInstanceDynamic* Rim = HullSlots.Contains(Slot) ? MakeRimMaterial(Body->GetMaterial(Slot)) : nullptr;
		if (Rim != nullptr)
		{
			RimBodyMaterials.Add(Rim);
		}
		RimBody->SetMaterial(Slot, Rim != nullptr ? static_cast<UMaterialInterface*>(Rim) : static_cast<UMaterialInterface*>(RimMask.Get()));
	}

	// Only the hull draws: in every LOD each section whose material is not the ink hull is hidden (no draw at all; the
	// BodyMask instance on those slots keeps them invisible should a section stay shown). The section's material index
	// goes through the LOD's material map like the scene proxy's.
	const FSkeletalMeshRenderData* RenderData = Mesh != nullptr ? Mesh->GetResourceForRendering() : nullptr;
	if (RenderData != nullptr)
	{
		for (int32 Lod = 0; Lod < RenderData->LODRenderData.Num(); ++Lod)
		{
			const FSkeletalMeshLODInfo* LodInfo = Mesh->GetLODInfo(Lod);
			const TArray<FSkelMeshRenderSection>& Sections = RenderData->LODRenderData[Lod].RenderSections;
			for (int32 Section = 0; Section < Sections.Num(); ++Section)
			{
				int32 MaterialIndex = Sections[Section].MaterialIndex;
				if (LodInfo != nullptr && LodInfo->LODMaterialMap.IsValidIndex(Section) && LodInfo->LODMaterialMap[Section] != INDEX_NONE)
				{
					MaterialIndex = LodInfo->LODMaterialMap[Section];
				}
				RimBody->ShowMaterialSection(MaterialIndex, Section, HullSlots.Contains(MaterialIndex), Lod);
			}
		}
	}
	RimBody->MarkRenderStateDirty();
	ApplyRimLook();
}

void AAbyssCharacterActor::SyncRimWeapons()
{
	using namespace AbyssCharacterPrivate;
	if (RimBody == nullptr)
	{
		return;
	}
	while (RimWeaponMaterials.Num() < 2 + ExtraItems.Num())
	{
		RimWeaponMaterials.Add(nullptr);
	}
	auto Sync = [this](UStaticMeshComponent* Rim, UStaticMeshComponent* Source, int32 Index)
	{
		if (Rim == nullptr)
		{
			return;
		}
		UStaticMesh* Mesh = nullptr;
		if (Source != nullptr && Source->IsVisible())
		{
			Mesh = Source->GetStaticMesh();
		}
		if (Mesh == nullptr)
		{
			Rim->SetStaticMesh(nullptr);
			Rim->SetVisibility(false);
			return;
		}
		if (Rim->GetStaticMesh() != Mesh)
		{
			Rim->SetStaticMesh(Mesh);
		}
		if (Rim->GetAttachParent() != Source)
		{
			// On the weapon itself (same socket, same transform): the copy follows every grip of the held mesh.
			Rim->AttachToComponent(Source, FAttachmentTransformRules::SnapToTargetIncludingScale);
		}
		// Slot 0 (toon) draws nothing (BodyMask 1); the hull slot gets the rim with that hull's width.
		const TArray<int32, TInlineAllocator<4>> HullSlots = FindHullSlots(*Source);
		const int32 NumSlots = Rim->GetNumMaterials();
		for (int32 Slot = 0; Slot < NumSlots; ++Slot)
		{
			UMaterialInterface* Material = RimMask.Get();
			if (HullSlots.Contains(Slot))
			{
				TObjectPtr<UMaterialInstanceDynamic>& WeaponRim = RimWeaponMaterials[Index];
				if (WeaponRim == nullptr)
				{
					WeaponRim = MakeRimMaterial(Source->GetMaterial(Slot));
				}
				else
				{
					CopyHullWidth(*WeaponRim, Source->GetMaterial(Slot));
				}
				if (WeaponRim != nullptr)
				{
					Material = WeaponRim.Get();
				}
			}
			Rim->SetMaterial(Slot, Material);
		}
		ApplyCpdTo(Rim);
	};
	Sync(RimMainHand, MainHandMesh, 0);
	Sync(RimOffHand, OffHandMesh, 1);
	// U3: the held items beyond the two hands (the Unburied's kettle helm) are part of the silhouette too.
	while (RimExtraItems.Num() < ExtraItems.Num())
	{
		UStaticMeshComponent* Rim = NewObject<UStaticMeshComponent>(this);
		ConfigureRimComponent(*Rim);
		Rim->SetupAttachment(ExtraItems[RimExtraItems.Num()].Get());
		Rim->RegisterComponent();
		RimExtraItems.Add(Rim);
	}
	for (int32 Index = 0; Index < RimExtraItems.Num(); ++Index)
	{
		Sync(RimExtraItems[Index], ExtraItems.IsValidIndex(Index) ? ExtraItems[Index].Get() : nullptr, 2 + Index);
	}
	ApplyRimLook();
	UpdateSelectionRimVisibility(/*bForce*/ true);
}

void AAbyssCharacterActor::ApplyRimLook()
{
	using namespace AbyssCharacterPrivate;
	if (RimKind == EAbyssSelectionRim::None)
	{
		return;
	}
	const FRimLook Look = RimLookFor(RimKind);
	const FLinearColor Color = AbyssWorldUtil::ColorFromRgb(Look.Rgb);   // linear, as M_AF_SelectRim's RimColor
	static const FName RimColorName(TEXT("RimColor"));
	static const FName RimPxName(TEXT("RimPx720"));
	static const FName RimPushName(TEXT("RimDepthPushCm"));
	auto Apply = [&Color, &Look](UMaterialInstanceDynamic* Rim)
	{
		if (Rim != nullptr)
		{
			Rim->SetVectorParameterValue(RimColorName, Color);
			Rim->SetScalarParameterValue(RimPxName, Look.Px720);
			Rim->SetScalarParameterValue(RimPushName, RimDepthPushCm);
		}
	};
	for (UMaterialInstanceDynamic* Rim : RimBodyMaterials)
	{
		Apply(Rim);
	}
	for (UMaterialInstanceDynamic* Rim : RimWeaponMaterials)
	{
		Apply(Rim);
	}
}

void AAbyssCharacterActor::UpdateSelectionRimVisibility(bool bForce)
{
	if (RimBody == nullptr)
	{
		return;
	}
	// Hidden while there is no rim, the actor or body is hidden, dying / dead, or ghosted (the rim only frames a living
	// figure; the body's own Fade dithers it during fades).
	const bool bShow = RimKind != EAbyssSelectionRim::None && bRimBudgetHeld && bHasArt && !IsHidden() && !bDying
		&& !bDeathFinished && CpdValues[AbyssCpd::Ghost] <= 0.f && Body != nullptr && Body->IsVisible();
	if (bShow == bRimShown && !bForce)
	{
		return;
	}
	bRimShown = bShow;
	RimBody->SetVisibility(bShow);
	if (RimMainHand != nullptr)
	{
		RimMainHand->SetVisibility(bShow && RimMainHand->GetStaticMesh() != nullptr && MainHandMesh != nullptr && MainHandMesh->IsVisible());
	}
	if (RimOffHand != nullptr)
	{
		RimOffHand->SetVisibility(bShow && RimOffHand->GetStaticMesh() != nullptr && OffHandMesh != nullptr && OffHandMesh->IsVisible());
	}
	for (int32 Index = 0; Index < RimExtraItems.Num(); ++Index)
	{
		UStaticMeshComponent* Rim = RimExtraItems[Index];
		const UStaticMeshComponent* Item = ExtraItems.IsValidIndex(Index) ? ExtraItems[Index].Get() : nullptr;
		if (Rim != nullptr)
		{
			Rim->SetVisibility(bShow && Rim->GetStaticMesh() != nullptr && Item != nullptr && Item->IsVisible());
		}
	}
}

void AAbyssCharacterActor::ResetPresentation()
{
	ActionState = FActionState();
	LocomotionClip = NAME_None;
	HoldFaceDir = FVector2D::ZeroVector;
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
	bCorpseDeath = false;
	bCorpseAdopted = false;
	bPoseHeld = false;
	UpdateHeldPoseTick();   // the body ticks again
	HuskShadowAlpha = 1.f;
	bAshRise = false;
	ClearAshLook();
	ClearAshnumbBand();
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
	UpdateSelectionRimVisibility();
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

void AAbyssCharacterActor::PlayClip(const FAbyssArtClip& Clip, float Rate, float StartSec, const char* ToState, bool bForceLoop)
{
	UAbyssAnimInstance* Anim = GetAnim();
	UAnimSequence* Seq = FindClip(Clip.Name);
	if (Anim == nullptr || Seq == nullptr)
	{
		return;
	}
	Anim->Play(Seq, Clip.bLoop || bForceLoop, Rate, BlendSecFor(CurrentState, ToState), StartSec, Clip.Notifies);
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
	case abyss::AnimAction::Roar:
	case abyss::AnimAction::Taunt:
	case abyss::AnimAction::Stun:
	case abyss::AnimAction::Attack02:
	case abyss::AnimAction::Cast02:
	case abyss::AnimAction::Rise:
		// K1 contract (SimTypes.h): the clip of the same name; an asset without it keeps its current pose (nullptr).
		if (!Requested.IsNone())
		{
			if (const FAbyssArtClip* Clip = FindArtClip(Requested))
			{
				return Clip;
			}
		}
		return FindArtClip(NamedActionClip(Event.action));
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

	// P5: the clip plays so its authored Contact / Release lands exactly on the core's beat (clamped to [.25, 4]); a
	// looping clip whose windup outlasts it loops at rate 1 with a beat on the contact (2G: the drummer's 2000 ms drum on
	// its 1000 ms Cast01 loop crawled at the clamped .25); no beat: stretched to the duration where the action stretches.
	AbyssAnimActionRules::FRateInput RateIn;
	RateIn.Action = Event.action;
	RateIn.bHasClip = Clip != nullptr;
	RateIn.BeatSec = Clip != nullptr ? Clip->GetBeatSec() : -1.f;
	RateIn.LengthSec = Clip != nullptr ? Clip->LengthSec : 0.f;
	RateIn.bLoopClip = Clip != nullptr && Clip->bLoop;
	RateIn.ContactMs = Event.contactMs;
	RateIn.DurationMs = Event.durationMs;
	RateIn.EventRate = Event.playRate;
	const AbyssAnimActionRules::FRatePlan RatePlan = AbyssAnimActionRules::ActionPlayRate(RateIn);
	const float Rate = RatePlan.Rate;

	ActionState = FActionState();
	// The beat the core waits for (T3 / T4): a frozen world holds the clip before it (PresentFrame).
	ActionBeatSimMs = Event.contactMs > 0.0 ? Event.startMs + Event.contactMs : -1.0;
	// AbyssAnimActionRules: Attack02 is an attack (contact beat, wind-up tint), Cast02 a cast (release beat), Roar /
	// Taunt / Stun / Rise an emote (no beat: a frozen world does not hold it, the boss intro's Roar plays through).
	switch (AbyssAnimActionRules::KindOf(Event.action))
	{
	case AbyssAnimActionRules::EActionKind::Attack: ActionState.Action = EAbyssCharacterAction::Attack; break;
	case AbyssAnimActionRules::EActionKind::Cast: ActionState.Action = EAbyssCharacterAction::Cast; break;
	case AbyssAnimActionRules::EActionKind::Dodge: ActionState.Action = EAbyssCharacterAction::Dodge; break;
	case AbyssAnimActionRules::EActionKind::Hurt: ActionState.Action = EAbyssCharacterAction::Hurt; break;
	case AbyssAnimActionRules::EActionKind::Signature: ActionState.Action = EAbyssCharacterAction::Signature; break;
	case AbyssAnimActionRules::EActionKind::Work: ActionState.Action = EAbyssCharacterAction::Work; break;
	case AbyssAnimActionRules::EActionKind::Talk: ActionState.Action = EAbyssCharacterAction::Talk; break;
	case AbyssAnimActionRules::EActionKind::Emote: ActionState.Action = EAbyssCharacterAction::Emote; break;
	case AbyssAnimActionRules::EActionKind::None:
	case AbyssAnimActionRules::EActionKind::Death: ActionState.Action = EAbyssCharacterAction::Cast; break;
	}
	float DurationSec = RatePlan.DurationSec > 0.f ? RatePlan.DurationSec
		: (Event.durationMs > 0.0 ? static_cast<float>(Event.durationMs / 1000.0) : 0.f);
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
			// K1: the mini-boss keeps facing the hero for as long as its dialogue holds the Taunt (UpdateFacing).
			if (AbyssAnimActionRules::IsHoldAction(Event.action))
			{
				HoldFaceDir = Dir;
			}
			const bool bAimedAction = ActionState.Action == EAbyssCharacterAction::Attack
				|| ActionState.Action == EAbyssCharacterAction::Cast || ActionState.Action == EAbyssCharacterAction::Signature;
			if (EntityKind == abyss::EntityKind::Hero && bAimedAction)
			{
				// C14 (4): turned toward the target / no-target aim before the beat, not at the M11 rate through it.
				const float BeatSec = static_cast<float>(FMath::Max(0.0, Event.contactMs) / 1000.0);
				const float TurnSec = BeatSec > 0.f
					? FMath::Min(AbyssCharacterPrivate::HeroAimTurnMaxSec, BeatSec * AbyssCharacterPrivate::HeroAimTurnBeatFraction)
					: AbyssCharacterPrivate::HeroAimTurnMaxSec;
				ActionState.AimTurnBudgetSec = FMath::Max(0.001f, TurnSec - ElapsedSec);
			}
		}
	}
	if (Event.windupMs > 0.0)
	{
		ActionState.TelegraphTotalSec = static_cast<float>(Event.windupMs / 1000.0);
		ActionState.TelegraphRemainingSec = FMath::Max(0.f, ActionState.TelegraphTotalSec - ElapsedSec);
	}

	if (Clip != nullptr)
	{
		const char* State = AbyssAnimActionRules::BlendState(AbyssAnimActionRules::KindOf(Event.action));
		PlayClip(*Clip, Rate, AbyssAnimActionRules::PlanClipTime(RatePlan, ElapsedSec, Clip->LengthSec), State,
			AbyssAnimActionRules::ForcesLoop(Event.action) || RatePlan.bLoopThrough);
		LocomotionClip = NAME_None;
	}
}

bool AAbyssCharacterActor::ClipHasFxNotifies(FName InClipName) const
{
	const FAbyssArtClip* Clip = FindArtClip(InClipName);
	return Clip != nullptr && Clip->Notifies.ContainsByPredicate([](const FAbyssArtNotify& Notify)
	{
		return Notify.Name.ToString().StartsWith(TEXT("FX_"));
	});
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
	else if (Event.reason == abyss::TeleportReason::Dash)
	{
		// Dark remake 2G (a charge: EvTelegraph::hasDash, Valdras's Lunge): the core stands the monster at the line's end
		// after the hit; the mesh slides there from where it struck (ease-out over AbyssTelegraphRules::DashSlideMs, the
		// dodge's offset), never a blink.
		const AbyssTelegraphRules::FDashSlide Slide = AbyssTelegraphRules::DashSlide(Event.from, Event.to);
		DodgeOffset = Slide.bSlide ? FVector(Slide.OffsetXCm, Slide.OffsetYCm, 0.0) : FVector::ZeroVector;
		DodgeTotalSec = static_cast<float>(Slide.DurationMs / 1000.0);
		DodgeElapsedSec = Slide.bSlide ? 0.f : -1.f;
	}
	else
	{
		DodgeOffset = FVector::ZeroVector;
		DodgeElapsedSec = -1.f;
	}
	if (Event.reason == abyss::TeleportReason::Skill && ActionState.bFaceLock)
	{
		// C14 (4): the lock was aimed from where the blink started. The core re-faced the hero on landing (shadow step:
		// back toward its target; teleport: along the blink), so the rest of the action follows the snapshot facing.
		ActionState.bFaceFollowsCore = true;
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
	bCorpseDeath = false;
	bCorpseAdopted = false;
	bPoseHeld = false;
	if (bCorpseHold && !bHeroDeath)
	{
		BeginCorpseDeath();
		return;
	}
	// Dark remake 2G: a death the art times (the Cinderbloat's fuse: its Death clip at rate 1 for 900 ms, so FX_Burst @800
	// lands on the core's 800 ms burst; the rig preset's 500 ms played it at 1.8 and put FX_Burst at 444 ms).
	const bool bArtTimed = bDeathRuntime && !bHeroDeath && DeathRuntimeMs > 0.f;
	const float DurationMs = bArtTimed ? DeathRuntimeMs
		: (Event.durationMs > 0.0 ? static_cast<float>(Event.durationMs) : DeathDurationMs);
	DeathTotalSec = FMath::Max(0.001f, DurationMs / 1000.f);
	if (const FAbyssArtClip* Death = FindArtClip(FName(TEXT("Death"))))
	{
		const float Rate = bArtTimed ? DeathRuntimeRate
			: (Death->LengthSec > 0.f ? FMath::Clamp(Death->LengthSec / (DurationMs / 1000.f), 0.5f, 2.f) : 1.f);
		PlayClip(*Death, Rate, 0.f, "death");
		LocomotionClip = NAME_None;
	}

	DeathThrowCm = 0.f;
	DeathHopCm = 0.f;
	DeathSpinDeg = 0.f;
	DeathAirSec = 0.f;
	if (!bHeroDeath && Event.hasFaceTarget && (!bArtTimed || bDeathRuntimeThrow))
	{
		// combat-feel.md 13.2: thrown away from the last hit source.
		const bool bHeavy = DurationMs >= 750.f;
		const FVector Source = AbyssUnits::TileToWorld(Event.faceTarget, GetActorLocation().Z);
		const FVector2D Dir = AbyssCharacterPrivate::Flat(GetActorLocation() - Source).GetSafeNormal();
		DeathThrowDir = FVector(Dir.X, Dir.Y, 0.0);
		DeathThrowCm = (bHeavy ? 6.f : 16.f) * AbyssWorldUtil::VfxPxToCm;
		DeathAirSec = FMath::Max(160.f, 0.35f * DurationMs) / 1000.f;
		DeathHopCm = (bArtTimed && !bDeathRuntimeHop) ? 0.f : (bHeavy ? 3.f : 9.f) * AbyssWorldUtil::VfxPxToCm;
		DeathSpinDeg = (bArtTimed && !bDeathRuntimeSpin) ? 0.f : (bHeavy ? 8.f : 22.f);
		DeathFadeDelaySec = DeathAirSec + 0.12f;
		DeathFadeSec = FMath::Max(200.f, 0.6f * DurationMs) / 1000.f;
	}
	else
	{
		// In-place sink and fade (hero; monsters killed by a DoT with no directional hit; an art-timed death standing
		// under its ring decal).
		DeathThrowDir = FVector::ZeroVector;
		DeathFadeDelaySec = 0.25f * DurationMs / 1000.f;
		DeathFadeSec = bHeroDeath ? 0.4f : FMath::Max(200.f, 0.6f * DurationMs) / 1000.f;
	}
	if (bArtTimed)
	{
		DeathFadeDelaySec = FMath::Max(DeathFadeDelaySec, DeathFadeNotBeforeSec);   // no fade before FX_Burst
	}
}

void AAbyssCharacterActor::BeginCorpseDeath()
{
	// Dark remake U3 (DR6 ash mode): the Death clip at rate 1 to its hold frame, standing where it fell (no throw, hop or
	// spin: the husk lies where the core's corpse entry is), never faded here: the corpse layer adopts the body on
	// EvCorpseAdded and turns it to ash; without an entry in time the fallback fade of UpdateDeath runs.
	bCorpseDeath = true;
	bAshRise = false;   // a risen body that falls again: the corpse layer's timeline owns the ash from here
	DeathTotalSec = FMath::Max(0.001f, CorpseHoldFromSec);
	if (const FAbyssArtClip* Death = FindArtClip(FName(TEXT("Death"))))
	{
		PlayClip(*Death, 1.f, 0.f, "death");
		LocomotionClip = NAME_None;
	}
	DeathThrowDir = FVector::ZeroVector;
	DeathThrowCm = 0.f;
	DeathHopCm = 0.f;
	DeathSpinDeg = 0.f;
	DeathAirSec = 0.f;
	DeathFadeDelaySec = std::numeric_limits<float>::max();
	DeathFadeSec = 0.f;
	// The marker and the affix halo leave with the life (they would float over the husk for its 20 s).
	if (CrownMesh != nullptr)
	{
		CrownMesh->SetVisibility(false);
	}
	if (AuraMesh != nullptr)
	{
		AuraMesh->SetVisibility(false);
	}
	bElite = false;
}

void AAbyssCharacterActor::AdoptAsCorpse()
{
	if (bCorpseDeath)
	{
		bCorpseAdopted = true;
	}
}

void AAbyssCharacterActor::PoseAsHusk()
{
	if (!bHasArt)
	{
		return;
	}
	bDying = true;
	bDeathFinished = false;
	bHeroDeath = false;
	ActionState = FActionState();
	ActionState.Action = EAbyssCharacterAction::Death;
	BeginCorpseDeath();
	bCorpseAdopted = true;
	// Posed at the clip's hold frame (the echo idiom's frozen pose, through the actor's own player): no notifies fire.
	DeathElapsedSec = DeathTotalSec;
	if (const FAbyssArtClip* Death = FindArtClip(FName(TEXT("Death"))))
	{
		if (UAbyssAnimInstance* Anim = GetAnim())
		{
			if (UAnimSequence* Seq = FindClip(Death->Name))
			{
				Anim->Play(Seq, false, 1.f, 0.f, FMath::Min(CorpseHoldFromSec, Death->LengthSec), {});
				CurrentState = "death";
				LocomotionClip = NAME_None;
			}
		}
	}
	bPoseHeld = true;
	if (UAbyssAnimInstance* Anim = GetAnim())
	{
		Anim->SetFrozen(true);
	}
}

bool AAbyssCharacterActor::EnsureAshMaterials(float TopCm)
{
	using namespace AbyssCharacterPrivate;
	if (bAshApplied)
	{
		return true;
	}
	if (bAshUnavailable || !bHasArt || Body == nullptr)
	{
		return false;
	}
	UAbyssAssetLibrary* Assets = UAbyssAssetLibrary::Get(this);
	UMaterialInterface* ToonAsh = Assets != nullptr ? Assets->LoadMaterial(TEXT("M_AF_Toon_Ash")) : nullptr;
	UMaterialInterface* InkAsh = Assets != nullptr ? Assets->LoadMaterial(TEXT("M_AF_Outline_Ash")) : nullptr;
	if (ToonAsh == nullptr || InkAsh == nullptr)
	{
		static bool bLogged = false;
		UE_CLOG(!bLogged, LogAbyss, Warning,
			TEXT("M_AF_Toon_Ash / M_AF_Outline_Ash not found (run Scripts/build_content.py): corpses fade as before"));
		bLogged = true;
		bAshUnavailable = true;
		return false;
	}
	static const FName TopName(TEXT("HuskTopCm"));
	AshSlots.Reset();
	AshMaterials.Reset();
	// Every toon / ink slot of the body and of each held item takes the same dissolve (no weapon lingers on the ground).
	TArray<UPrimitiveComponent*, TInlineAllocator<6>> Components = { Body.Get(), MainHandMesh.Get(), OffHandMesh.Get() };
	for (UStaticMeshComponent* Item : ExtraItems)
	{
		Components.Add(Item);
	}
	for (UPrimitiveComponent* Component : Components)
	{
		if (Component == nullptr || !Component->IsVisible())
		{
			continue;
		}
		const int32 NumSlots = Component->GetNumMaterials();
		for (int32 Slot = 0; Slot < NumSlots; ++Slot)
		{
			UMaterialInterface* Material = Component->GetMaterial(Slot);
			UMaterialInterface* Twin = AshTwinFor(Material, ToonAsh, InkAsh);
			UMaterialInstanceDynamic* Ash = Twin != nullptr ? UMaterialInstanceDynamic::Create(Twin, this) : nullptr;
			if (Ash == nullptr)
			{
				continue;
			}
			if (UMaterialInstance* Instance = Cast<UMaterialInstance>(Material))
			{
				Ash->CopyParameterOverrides(Instance);   // palette textures, look scalars, the hull's widths
			}
			Ash->SetScalarParameterValue(TopName, TopCm);
			FAshSlot& Record = AshSlots.AddDefaulted_GetRef();
			Record.Component = Component;
			Record.Slot = Slot;
			Record.Original = Material;
			AshMaterials.Add(Ash);
			Component->SetMaterial(Slot, Ash);
		}
	}
	bAshApplied = AshSlots.Num() > 0;
	AshGrey = AshDissolve = -1.f;
	AshTopCm = TopCm;
	return bAshApplied;
}

bool AAbyssCharacterActor::SetAshLook(float Grey, float Dissolve, float TopCm)
{
	const float G = FMath::Clamp(Grey, 0.f, 1.f);
	const float D = FMath::Clamp(Dissolve, 0.f, 1.f);
	if (!bAshApplied && G <= 0.f && D <= 0.f)
	{
		return !bAshUnavailable;   // nothing to show yet: the slots keep their own materials
	}
	const float Top = TopCm > 0.f ? TopCm : (HuskTopCm > 0.f ? HuskTopCm : VisualHeightCm);
	if (!EnsureAshMaterials(Top))
	{
		return false;
	}
	static const FName GreyName(TEXT("Grey"));
	static const FName DissolveName(TEXT("Dissolve"));
	static const FName TopName(TEXT("HuskTopCm"));
	const bool bGrey = !FMath::IsNearlyEqual(G, AshGrey, 0.0005f);
	const bool bDissolve = !FMath::IsNearlyEqual(D, AshDissolve, 0.0005f);
	const bool bTop = !FMath::IsNearlyEqual(Top, AshTopCm, 0.01f);
	for (UMaterialInstanceDynamic* Ash : AshMaterials)
	{
		if (Ash == nullptr)
		{
			continue;
		}
		if (bGrey)
		{
			Ash->SetScalarParameterValue(GreyName, G);
		}
		if (bDissolve)
		{
			Ash->SetScalarParameterValue(DissolveName, D);
		}
		if (bTop)
		{
			Ash->SetScalarParameterValue(TopName, Top);
		}
	}
	AshGrey = G;
	AshDissolve = D;
	AshTopCm = Top;
	return true;
}

void AAbyssCharacterActor::ClearAshLook()
{
	for (const FAshSlot& Record : AshSlots)
	{
		UPrimitiveComponent* Component = Record.Component.Get();
		UMaterialInterface* Original = Record.Original.Get();
		if (Component != nullptr && Original != nullptr)
		{
			Component->SetMaterial(Record.Slot, Original);
		}
	}
	AshSlots.Reset();
	AshMaterials.Reset();
	bAshApplied = false;
	AshGrey = AshDissolve = AshTopCm = -1.f;
}

bool AAbyssCharacterActor::UpdateAshnumbBand(int32 Stacks, bool bAllowed)
{
	// The band on the body's toon slots only: the ink hull and the held items keep their materials (AshnumbTint). A body
	// on the ash twins (a husk: never the hero) is left alone.
	const double Top = AbyssWorldStateRules::BandTop(AbyssWorldStateRules::DefaultAshnumbTint(), Stacks);
	if (!(Top > 0.0) || !bAllowed || AshnumbMaster == nullptr || Body == nullptr || !bHasArt || bAshApplied || bBandNoSlots)
	{
		ClearAshnumbBand();
		return false;
	}
	if (!bBandApplied)
	{
		static const FName ToonName(TEXT("M_AF_Toon"));
		static const FName HeightName(TEXT("BandHeightCm"));
		BandSlots.Reset();
		BandMaterials.Reset();
		const int32 NumSlots = Body->GetNumMaterials();
		for (int32 Slot = 0; Slot < NumSlots; ++Slot)
		{
			UMaterialInterface* Material = Body->GetMaterial(Slot);
			const UMaterial* Base = Material != nullptr ? Material->GetBaseMaterial() : nullptr;
			if (Base == nullptr || Base->GetFName() != ToonName)
			{
				continue;
			}
			UMaterialInstanceDynamic* Band = UMaterialInstanceDynamic::Create(AshnumbMaster, this);
			if (Band == nullptr)
			{
				continue;
			}
			if (UMaterialInstance* Instance = Cast<UMaterialInstance>(Material))
			{
				Band->CopyParameterOverrides(Instance);   // the palette textures and the look scalars of the hero's slot
			}
			Band->SetScalarParameterValue(HeightName, BandHeightCm);
			FAshSlot& Record = BandSlots.AddDefaulted_GetRef();
			Record.Component = Body.Get();
			Record.Slot = Slot;
			Record.Original = Material;
			BandMaterials.Add(Band);
			Body->SetMaterial(Slot, Band);
		}
		bBandApplied = BandSlots.Num() > 0;
		bBandNoSlots = !bBandApplied;
		BandTopApplied = -1.f;
		if (!bBandApplied)
		{
			return false;
		}
	}
	const float TopValue = static_cast<float>(Top);
	if (!FMath::IsNearlyEqual(TopValue, BandTopApplied, 0.0005f))
	{
		static const FName TopName(TEXT("BandTop"));
		for (UMaterialInstanceDynamic* Band : BandMaterials)
		{
			if (Band != nullptr)
			{
				Band->SetScalarParameterValue(TopName, TopValue);
			}
		}
		BandTopApplied = TopValue;
	}
	return true;
}

void AAbyssCharacterActor::ClearAshnumbBand()
{
	if (!bBandApplied && BandSlots.Num() == 0)
	{
		return;
	}
	for (const FAshSlot& Record : BandSlots)
	{
		UPrimitiveComponent* Component = Record.Component.Get();
		UMaterialInterface* Original = Record.Original.Get();
		if (Component != nullptr && Original != nullptr)
		{
			Component->SetMaterial(Record.Slot, Original);
		}
	}
	BandSlots.Reset();
	BandMaterials.Reset();
	bBandApplied = false;
	BandTopApplied = -1.f;
}

void AAbyssCharacterActor::BeginAshRise(float StartGrey, float StartDissolve, double StartSimMs, double SpanMs)
{
	if (!(StartGrey > 0.f) && !(StartDissolve > 0.f))
	{
		return;   // raised before it greyed (or summoned): nothing to reverse
	}
	bAshRise = true;
	RiseStartGrey = StartGrey;
	RiseStartDissolve = StartDissolve;
	RiseStartSimMs = StartSimMs;
	RiseSpanMs = FMath::Max(1.0, SpanMs);
	if (!SetAshLook(StartGrey, StartDissolve, VisualHeightCm))
	{
		bAshRise = false;
	}
}

void AAbyssCharacterActor::UpdateAshRise(double RenderSimMs)
{
	if (!bAshRise)
	{
		return;
	}
	const AbyssCorpseRules::FRiseAsh Ash = AbyssCorpseRules::RiseAsh(AbyssCorpseRules::FRiseStart{ RiseStartGrey, RiseStartDissolve },
		RiseStartSimMs, RiseSpanMs, 1.0, RenderSimMs);
	if (Ash.bDone)
	{
		bAshRise = false;
		ClearAshLook();
		return;
	}
	SetAshLook(static_cast<float>(Ash.Grey), static_cast<float>(Ash.Dissolve), VisualHeightCm);
}

void AAbyssCharacterActor::UpdateHeldPoseTick()
{
	// A husk's pose never changes once held: after it has been drawn a few times with the clock frozen (so the frozen
	// frame is the evaluated one - a posed seed starts its clip at the hold frame and needs one evaluation), the body's
	// component tick stops (no anim graph, no bone refresh, no skinning update for up to HuskCap bodies); the material
	// scalars and the blob shadow do not need it. Off screen (OnlyTickPoseWhenRendered skips the pose) nothing counts.
	if (Body == nullptr)
	{
		return;
	}
	if (!bPoseHeld)
	{
		HeldPoseRenderedFrames = 0;
		if (bBodyTickPaused)
		{
			Body->SetComponentTickEnabled(true);
			bBodyTickPaused = false;
		}
		return;
	}
	if (bBodyTickPaused)
	{
		return;
	}
	if (Body->WasRecentlyRendered(0.1f))
	{
		++HeldPoseRenderedFrames;
	}
	if (HeldPoseRenderedFrames >= AbyssCharacterPrivate::HeldPoseSettleFrames)
	{
		Body->SetComponentTickEnabled(false);
		bBodyTickPaused = true;
	}
}

void AAbyssCharacterActor::SetHuskShadowAlpha(float Alpha)
{
	HuskShadowAlpha = FMath::Clamp(Alpha, 0.f, 1.f);
}

bool AAbyssCharacterActor::GetHeldItemSocket(FName ItemAsset, FName Socket, FVector& OutLocation) const
{
	TArray<const UStaticMeshComponent*, TInlineAllocator<6>> Items = { MainHandMesh.Get(), OffHandMesh.Get() };
	for (const UStaticMeshComponent* Item : ExtraItems)
	{
		Items.Add(Item);
	}
	for (const UStaticMeshComponent* Item : Items)
	{
		const UStaticMesh* Mesh = Item != nullptr && Item->IsVisible() ? Item->GetStaticMesh() : nullptr;
		if (Mesh != nullptr && (ItemAsset.IsNone() || Mesh->GetFName() == ItemAsset) && Item->DoesSocketExist(Socket))
		{
			OutLocation = Item->GetSocketLocation(Socket);
			return true;
		}
	}
	return false;
}

void AAbyssCharacterActor::SetExtraItems(const TArray<TPair<UStaticMesh*, FName>>& Items)
{
	using namespace AbyssCharacterPrivate;
	while (ExtraItems.Num() < Items.Num())
	{
		UStaticMeshComponent* Item = NewObject<UStaticMeshComponent>(this);
		ConfigureAttachmentMesh(Item);
		Item->SetupAttachment(Body);
		Item->RegisterComponent();
		ExtraItems.Add(Item);
	}
	ExtraItemSockets.SetNum(ExtraItems.Num());
	for (int32 Index = 0; Index < ExtraItems.Num(); ++Index)
	{
		UStaticMeshComponent* Item = ExtraItems[Index];
		if (Item == nullptr)
		{
			continue;
		}
		const bool bValid = Items.IsValidIndex(Index) && Items[Index].Key != nullptr && Body != nullptr
			&& Body->DoesSocketExist(Items[Index].Value);
		if (!bValid)
		{
			Item->SetStaticMesh(nullptr);
			Item->SetVisibility(false);
			ExtraItemSockets[Index] = NAME_None;
			continue;
		}
		Item->SetStaticMesh(Items[Index].Key);
		Item->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Items[Index].Value);
		Item->SetVisibility(true);
		ExtraItemSockets[Index] = Items[Index].Value;
		ApplyCpdTo(Item);
	}
	if (RimBody != nullptr)
	{
		SyncRimWeapons();   // their selection-rim copies follow them (R16: the whole figure's silhouette)
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
	if (RimBody != nullptr)
	{
		SyncRimWeapons();   // the selection rim's weapon copies follow the held meshes (R16)
	}
}

void AAbyssCharacterActor::SetElite(bool bInElite, const TArray<FLinearColor>& InAffixColors, UStaticMesh* InCrownMesh,
	UStaticMesh* InAuraMesh, UMaterialInterface* AuraMaterial, const FAbyssEliteMarker& Marker)
{
	bElite = bInElite;
	AffixColors = InAffixColors;
	EliteMarker = Marker;
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
		// Wave 2 sigil (AbyssMonsterLookRules::FSigilMotion): its own fx_elite socket (fx_overhead on older rigs), one world
		// size on every body (absolute scale); today's crown keeps fx_overhead and the body's scale.
		FName Socket = AbyssAnchorSocketName(EAbyssAnchor::Overhead);
		if (EliteMarker.bSigil && Body != nullptr)
		{
			if (!EliteMarker.Socket.IsNone() && Body->DoesSocketExist(EliteMarker.Socket))
			{
				Socket = EliteMarker.Socket;
			}
			else if (!EliteMarker.FallbackSocket.IsNone())
			{
				Socket = EliteMarker.FallbackSocket;
			}
		}
		CrownMesh->SetUsingAbsoluteScale(EliteMarker.bSigil && EliteMarker.bAbsoluteScale);
		CrownMesh->SetRelativeScale3D(FVector::OneVector);
		if (Body != nullptr && Body->DoesSocketExist(Socket))
		{
			CrownMesh->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
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
		const FVector2D Shown = bSingleView ? SingleViewFacing(Facing, /*bAimed*/ false) : Facing;
		CurrentYawDeg = FMath::RadiansToDegrees(FMath::Atan2(Shown.Y, Shown.X));
		bHasYaw = true;
		SetActorRotation(FRotator(0.f, CurrentYawDeg, 0.f));
	}
}

FVector2D AAbyssCharacterActor::SingleViewFacing(const FVector2D& Desired, bool bAimed)
{
	using namespace AbyssCharacterPrivate;
	// Ground axes of the fixed camera (AAbyssCameraRig::GetGroundForward / GetGroundRight): screen up, screen right.
	const float Yaw = FMath::DegreesToRadians(SingleViewCameraYawDeg);
	const FVector2D ScreenUp(FMath::Cos(Yaw), FMath::Sin(Yaw));
	const FVector2D ScreenRight(-FMath::Sin(Yaw), FMath::Cos(Yaw));
	const FVector2D Dir = Desired.GetSafeNormal();
	const double ScreenX = FVector2D::DotProduct(Dir, ScreenRight);
	// The web mirrors on the sign of the screen-x step and keeps the side when that step is ~0.
	if (FMath::Abs(ScreenX) >= SingleViewMirrorDeadband)
	{
		bSingleViewLeft = ScreenX < 0.0;
	}
	const FVector2D Side = bSingleViewLeft ? -ScreenRight : ScreenRight;
	if (bAimed)
	{
		// The slam keeps its aim while that shows the front; a target up-screen is struck side-on, never with the back
		// to the viewer.
		if (FVector2D::DotProduct(Dir, ScreenUp) <= 0.0)
		{
			return Dir;
		}
		return FMath::Abs(ScreenX) >= SingleViewMirrorDeadband ? (ScreenRight * ScreenX).GetSafeNormal() : Side;
	}
	// The se / sw view: halfway between screen right / left and the viewer (the web's +col / +row screen steps).
	return (Side - ScreenUp).GetSafeNormal();
}

void AAbyssCharacterActor::UpdateFacing(const FAbyssCharacterFrame& Frame, float DeltaSec)
{
	using namespace AbyssCharacterPrivate;
	if (bDying)
	{
		return;
	}
	FVector2D Desired = Frame.Facing;
	bool bAimed = false;
	// K1 hold: the Taunt's facing toward the hero lasts while the core holds the pose; it ends with the hold.
	if (!AbyssAnimActionRules::IsHoldAction(Frame.HoldAction))
	{
		HoldFaceDir = FVector2D::ZeroVector;
	}
	else if (!HoldFaceDir.IsNearlyZero())
	{
		Desired = HoldFaceDir;
	}
	if (ActionState.bFaceLock && ActionState.Action != EAbyssCharacterAction::None)
	{
		if (ActionState.bFaceFollowsCore && !Frame.Facing.IsNearlyZero())
		{
			ActionState.FaceDir = Frame.Facing;
		}
		Desired = ActionState.FaceDir;
		bAimed = true;
	}
	// C14 (4): the hero's aimed attack / cast completes its turn within the aim budget (action time, so a hit-stop or a
	// frozen world pauses it with the clip).
	float AimBudgetSec = 0.f;
	if (bAimed && ActionState.AimTurnBudgetSec > 0.f)
	{
		AimBudgetSec = ActionState.AimTurnBudgetSec;
		ActionState.AimTurnBudgetSec = FMath::Max(0.f, AimBudgetSec - DeltaSec);
	}
	if (Desired.IsNearlyZero())
	{
		return;
	}
	if (bSingleView)
	{
		Desired = SingleViewFacing(Desired, bAimed);
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
		float RateDegPerSec = TurnRateDegPerSec;
		if (AimBudgetSec > 0.f && DeltaSec > 0.f)
		{
			// The rest of the turn over the budget left (the last frame of the budget closes it).
			RateDegPerSec = FMath::Max(RateDegPerSec, FMath::Abs(Delta) / FMath::Max(AimBudgetSec, DeltaSec));
		}
		CurrentYawDeg = FMath::FixedTurn(CurrentYawDeg, DesiredYaw, RateDegPerSec * DeltaSec);
	}
	SetActorRotation(FRotator(0.f, CurrentYawDeg, 0.f));
}

void AAbyssCharacterActor::UpdateLocomotion(const FAbyssCharacterFrame& Frame)
{
	using namespace AbyssCharacterPrivate;
	UAbyssAnimInstance* Anim = GetAnim();
	if (Anim == nullptr)
	{
		return;
	}
	// Who owns the body (AbyssAnimActionRules): death > action > hold (K1 Taunt) > stun pose > locomotion > idle. A hold or
	// stun whose clip the asset lacks keeps today's locomotion.
	const FAbyssArtClip* HoldClip = AbyssAnimActionRules::IsHoldAction(Frame.HoldAction)
		? FindArtClip(NamedActionClip(Frame.HoldAction)) : nullptr;
	const FAbyssArtClip* StunClip = Frame.bStunned ? FindArtClip(NamedActionClip(abyss::AnimAction::Stun)) : nullptr;
	AbyssAnimActionRules::FLayerInputs LayerInputs;
	LayerInputs.bDying = bDying;
	LayerInputs.bInAction = ActionState.Action != EAbyssCharacterAction::None;
	LayerInputs.bHoldClip = HoldClip != nullptr;
	LayerInputs.bStunClip = StunClip != nullptr;
	LayerInputs.bMoving = Frame.bMoving && Frame.SpeedCmS > MinMovingSpeedCmS;
	const AbyssAnimActionRules::ELayer BodyLayer = AbyssAnimActionRules::PickLayer(LayerInputs);
	if (BodyLayer == AbyssAnimActionRules::ELayer::Death || BodyLayer == AbyssAnimActionRules::ELayer::Action)
	{
		return;
	}
	if (BodyLayer == AbyssAnimActionRules::ELayer::Hold || BodyLayer == AbyssAnimActionRules::ELayer::Stun)
	{
		const FAbyssArtClip* Pose = BodyLayer == AbyssAnimActionRules::ELayer::Hold ? HoldClip : StunClip;
		if (Anim->GetCurrentSequence() == FindClip(Pose->Name))
		{
			// Already showing it (the EvPlayAnim{Taunt} that started the hold): keep looping, no restart.
			LocomotionClip = Pose->Name;
			return;
		}
		PlayClip(*Pose, 1.f, 0.f, "idle", /*bForceLoop*/ true);
		LocomotionClip = Pose->Name;
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
	if (bCorpseDeath)
	{
		// U3: the Death clip stops on its hold frame (the anim clock freezes there); the body stays until the corpse layer
		// lets it go. Not adopted within the clip + 1 s (the core registered no body for it): today's in-place fade from
		// here.
		if (!bPoseHeld && DeathElapsedSec >= DeathTotalSec)
		{
			bPoseHeld = true;
		}
		if (!bCorpseAdopted && AbyssCorpseRules::AdoptionTimedOut(static_cast<double>(DeathElapsedSec) * 1000.0,
				static_cast<double>(DeathTotalSec) * 1000.0))
		{
			bCorpseDeath = false;
			bPoseHeld = false;
			DeathFadeDelaySec = DeathElapsedSec;
			DeathFadeSec = 0.3f;
		}
		return;
	}
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
	const bool bEliteTint = bElite || Frame.bElite;
	const FLinearColor WindupTint = Frame.bHasWindupTint ? (bEliteTint ? Frame.WindupTintElite : Frame.WindupTint)
														 : (bEliteTint ? TelegraphElite() : TelegraphNormal());
	SetCpdColor(AbyssCpd::TelegraphR, WindupTint);
	SetCpd(AbyssCpd::TelegraphAmount, Telegraph > 0.f ? FMath::Clamp(Telegraph, 0.f, 1.f) * 0.85f : 0.f);

	// Status tints (R8): the first of the web's six present; K6 Ashnumb shows as the hero's band where the tier draws it
	// (M_AF_Toon_Ashnumb), else as its fallback multiply (AbyssWorldStateRules::CharacterStatusTint).
	const bool bAshnumbBand = UpdateAshnumbBand(Frame.AshnumbStacks, Frame.bAshnumbBand);
	const AbyssWorldStateRules::FTintPick Pick = AbyssWorldStateRules::CharacterStatusTint(Frame.StatusMask,
		Frame.AshnumbStacks, bAshnumbBand, AbyssWorldStateRules::DefaultAshnumbTint());
	FLinearColor Tint = FLinearColor::Black;
	float TintAmount = 0.f;
	if (Pick.bFound)
	{
		Tint = AbyssWorldUtil::ColorFromRgb(Pick.Rgb);
		TintAmount = Pick.Alpha;
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
		for (UStaticMeshComponent* Item : ExtraItems)
		{
			ApplyCpdTo(Item);
		}
		// Selection rim (R16): the follower carries the body's custom data, so M_AF_SelectRim dithers its Fade (13) on the
		// same pixels as the body.
		ApplyCpdTo(RimBody);
		ApplyCpdTo(RimMainHand);
		ApplyCpdTo(RimOffHand);
		for (UStaticMeshComponent* Rim : RimExtraItems)
		{
			ApplyCpdTo(Rim);
		}
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
	if (CrownMesh != nullptr && CrownMesh->IsVisible() && EliteMarker.bSigil)
	{
		// The sigil: a level float over its socket with a symmetric bob about it (never into the head) and a steady turn,
		// the offset taken in world units along world up whatever the socket's pose and scale.
		AbyssMonsterLookRules::FSigilMotion Motion;
		Motion.FloatCm = EliteMarker.FloatCm;
		Motion.BobCm = EliteMarker.BobCm;
		Motion.BobPeriodMs = EliteMarker.BobPeriodMs;
		Motion.SpinDegPerSec = EliteMarker.SpinDegPerSec;
		const double TimeMs = static_cast<double>(EliteTimeSec) * 1000.0;
		const float LiftCm = static_cast<float>(AbyssMonsterLookRules::SigilLiftCm(Motion, TimeMs));
		const FName Socket = CrownMesh->GetAttachSocketName();
		if (!Socket.IsNone() && Body != nullptr && CrownMesh->GetAttachParent() == Body.Get())
		{
			const FTransform SocketXf = Body->GetSocketTransform(Socket, RTS_World);
			CrownMesh->SetRelativeLocation(SocketXf.InverseTransformVector(FVector(0.f, 0.f, LiftCm)));
		}
		else
		{
			CrownMesh->SetRelativeLocation(FVector(0.f, 0.f, VisualHeightCm + 25.f + LiftCm));
		}
		CrownMesh->SetWorldRotation(FRotator(0.f, static_cast<float>(AbyssMonsterLookRules::SigilYawDeg(Motion, TimeMs)), 0.f));
	}
	else if (CrownMesh != nullptr && CrownMesh->IsVisible())
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
		const bool bAnimFrozen = bHitStop || bWorldHold || bPoseHeld;
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
	UpdateAshRise(Info.RenderSimMs);
	UpdateHeldPoseTick();

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
		FVector BlobOffset(LocalOffset.X, LocalOffset.Y, 1.5f);
		if (bCorpseDeath && Body != nullptr && Body->DoesSocketExist(AbyssCharacterPrivate::PelvisBone()))
		{
			// U3: a lying body's shadow sits under its pelvis, not under the feet it stood on.
			const FVector Pelvis = RootQuat.UnrotateVector(Body->GetSocketLocation(AbyssCharacterPrivate::PelvisBone()) - GetActorLocation());
			BlobOffset = FVector(Pelvis.X, Pelvis.Y, 1.5f);
		}
		BlobShadow->SetRelativeLocation(BlobOffset);
		BlobShadow->SetRelativeScale3D(FVector(Diameter, Diameter, 1.f));
		// The body's fade, and 0.7 of the blob's alpha where the sun CSM casts the real shadow (DECISIONS P10 revised);
		// a husk's shadow thins with its ash (HuskShadowAlpha).
		const float BlobAlpha = (1.f - CpdValues[AbyssCpd::Fade]) * HuskShadowAlpha
			* (bBlobUnderSunShadow ? AbyssCharacterPrivate::BlobAlphaUnderSunShadow : 1.f);
		BlobShadow->SetCustomPrimitiveDataFloat(AbyssCpd::Fade, 1.f - BlobAlpha);
	}

	UpdateMaterialFeedback(Frame, VisualDeltaSec);
	UpdateEliteVisuals(VisualDeltaSec);
	UpdateAfterimages(VisualDeltaSec);
	UpdateSelectionRimVisibility();
}
