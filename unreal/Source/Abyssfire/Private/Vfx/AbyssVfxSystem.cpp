#include "Vfx/AbyssVfxSystem.h"

#include "Abyssfire.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

#include <string>
#include <string_view>

#include "abyss/base/Enums.h"
#include "abyss/data/DataStore.h"
#include "abyss/data/SkillData.h"

#include "Actors/AbyssCharacterActor.h"
#include "Actors/AbyssPropActor.h"
#include "Framework/AbyssActorRegistry.h"
#include "Framework/AbyssGameInstance.h"
#include "Framework/AbyssSimDriver.h"
#include "Framework/AbyssText.h"
#include "Framework/AbyssUnits.h"
#include "Framework/AbyssWorldView.h"
#include "World/AbyssAssetLibrary.h"

namespace AbyssVfxSystemPrivate
{
	constexpr int32 DesktopMaxParticles = 900;   // web FxEngine MAX_PARTICLES
	constexpr int32 MobileMaxParticles = 600;    // art-inventory-ch1.md 1.9
	constexpr float ShakeThrottleSec = 0.12f;    // FxEngine.shake own throttle
	constexpr float MaxEmitterBacklogMs = 250.f; // continuous emitters never burst more than this after a hitch
	constexpr float VfxPxToCm = 100.f / 45.f;

	// Default anchor heights (cm) when the anchor has no actor / socket.
	constexpr float DefaultChestCm = 80.f;
	constexpr float DefaultHeadCm = 140.f;
	constexpr float DefaultOverheadCm = 200.f;
	constexpr float DefaultHandCm = 95.f;

	FName MakeId(const TCHAR* Prefix, std::string_view Id, const TCHAR* Suffix = nullptr)
	{
		FString Text(Prefix);
		Text += AbyssText::ToFString(Id);
		if (Suffix != nullptr)
		{
			Text += Suffix;
		}
		return FName(*Text);
	}

	FName MakeId(const TCHAR* Prefix, FName Id, const TCHAR* Suffix = nullptr)
	{
		FString Text(Prefix);
		Text += Id.ToString();
		if (Suffix != nullptr)
		{
			Text += Suffix;
		}
		return FName(*Text);
	}

	EAbyssAnchor ToAnchor(EAbyssVfxHeight Height)
	{
		switch (Height)
		{
		case EAbyssVfxHeight::Chest: return EAbyssAnchor::Chest;
		case EAbyssVfxHeight::Head: return EAbyssAnchor::Head;
		case EAbyssVfxHeight::Overhead: return EAbyssAnchor::Overhead;
		case EAbyssVfxHeight::Hand: return EAbyssAnchor::HandR;
		default: return EAbyssAnchor::Feet;
		}
	}

	float DefaultHeight(EAbyssVfxHeight Height, float CustomCm)
	{
		switch (Height)
		{
		case EAbyssVfxHeight::Chest: return DefaultChestCm;
		case EAbyssVfxHeight::Head: return DefaultHeadCm;
		case EAbyssVfxHeight::Overhead: return DefaultOverheadCm;
		case EAbyssVfxHeight::Hand: return DefaultHandCm;
		case EAbyssVfxHeight::Custom: return CustomCm;
		case EAbyssVfxHeight::Ground: return 0.f;
		}
		return 0.f;
	}

	FLinearColor QualityColor(abyss::ItemQuality Quality)
	{
		// Canonical loot palette (art-inventory-ch1.md 7, QUIRK A1).
		switch (Quality)
		{
		case abyss::ItemQuality::Magic: return AbyssWorldUtil::ColorFromRgb(0x4F8CFF);
		case abyss::ItemQuality::Rare: return AbyssWorldUtil::ColorFromRgb(0xFFD84A);
		case abyss::ItemQuality::Legendary: return AbyssWorldUtil::ColorFromRgb(0xFF8A2A);
		case abyss::ItemQuality::Set: return AbyssWorldUtil::ColorFromRgb(0x3ECF6A);
		case abyss::ItemQuality::Normal: break;
		}
		return FLinearColor::White;
	}

	/** Random unit vector inside a cone (full angle ConeDeg) around Axis. */
	FVector ConeDirection(FRandomStream& Random, const FVector& Axis, float ConeDeg)
	{
		if (ConeDeg >= 359.f)
		{
			return Random.GetUnitVector();
		}
		if (ConeDeg <= 0.f)
		{
			return Axis;
		}
		return Random.VRandCone(Axis, FMath::DegreesToRadians(ConeDeg * 0.5f));
	}

	/** Horizontal direction rotated by a random yaw within +-ConeDeg / 2. */
	FVector FlatCone(FRandomStream& Random, const FVector2D& Dir, float ConeDeg)
	{
		const float Half = FMath::Min(ConeDeg, 360.f) * 0.5f;
		const float Yaw = FMath::DegreesToRadians(Random.FRandRange(-Half, Half));
		const float C = FMath::Cos(Yaw);
		const float S = FMath::Sin(Yaw);
		return FVector(Dir.X * C - Dir.Y * S, Dir.X * S + Dir.Y * C, 0.0);
	}

	FTransform AxesTransform(const FVector& Position, const FVector& UnitX, const FVector& UnitY, float ScaleX, float ScaleY)
	{
		const FVector UnitZ = FVector::CrossProduct(UnitX, UnitY);
		const FMatrix Basis(UnitX, UnitY, UnitZ, FVector::ZeroVector);
		return FTransform(FQuat(Basis), Position, FVector(ScaleX, ScaleY, 1.0));
	}

	FTransform HiddenTransform()
	{
		return FTransform(FQuat::Identity, FVector(0.0, 0.0, -100000.0), FVector::ZeroVector);
	}
}

// =====================================================================================================================
// Lifecycle
// =====================================================================================================================

UAbyssVfxSystem* UAbyssVfxSystem::Get(const UObject* WorldContextObject)
{
	const UWorld* OwningWorld = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return OwningWorld ? OwningWorld->GetSubsystem<UAbyssVfxSystem>() : nullptr;
}

bool UAbyssVfxSystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UAbyssVfxSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UAbyssActorRegistry>();
	Random.Initialize(0x5EEDF00Du);
	Library.LoadAll();
	MaxParticles = Quality.bMobile ? AbyssVfxSystemPrivate::MobileMaxParticles : AbyssVfxSystemPrivate::DesktopMaxParticles;
	BindEvents();
}

void UAbyssVfxSystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	BindEvents();
}

void UAbyssVfxSystem::Deinitialize()
{
	UnbindEvents();
	ClearAll();
	if (RenderActor != nullptr)
	{
		RenderActor->Destroy();
		RenderActor = nullptr;
	}
	PoolComponents.Reset();
	PoolMaterials.Reset();
	Pools.Reset();
	PoolIndexByKey.Reset();
	Library.Reset();
	Super::Deinitialize();
}

void UAbyssVfxSystem::BindEvents()
{
	if (bEventsBound)
	{
		return;
	}
	const UWorld* OwningWorld = GetWorld();
	UAbyssGameInstance* GameInstance = OwningWorld ? Cast<UAbyssGameInstance>(OwningWorld->GetGameInstance()) : nullptr;
	if (GameInstance == nullptr)
	{
		return;
	}
	FAbyssEventRouter& Router = GameInstance->GetEventRouter();
	Router.On<abyss::EvHit>().AddUObject(this, &UAbyssVfxSystem::HandleHit);
	Router.On<abyss::EvEntityDespawned>().AddUObject(this, &UAbyssVfxSystem::HandleDespawned);
	Router.On<abyss::EvHeroDied>().AddUObject(this, &UAbyssVfxSystem::HandleHeroDied);
	Router.On<abyss::EvHeroRespawned>().AddUObject(this, &UAbyssVfxSystem::HandleHeroRespawned);
	Router.On<abyss::EvLevelUp>().AddUObject(this, &UAbyssVfxSystem::HandleLevelUp);
	Router.On<abyss::EvItemPicked>().AddUObject(this, &UAbyssVfxSystem::HandleItemPicked);
	Router.On<abyss::EvPotionPicked>().AddUObject(this, &UAbyssVfxSystem::HandlePotionPicked);
	Router.On<abyss::EvDodgeStarted>().AddUObject(this, &UAbyssVfxSystem::HandleDodge);
	Router.On<abyss::EvHeroDash>().AddUObject(this, &UAbyssVfxSystem::HandleDash);
	Router.On<abyss::EvEntityTeleported>().AddUObject(this, &UAbyssVfxSystem::HandleTeleported);
	Router.On<abyss::EvTownPortal>().AddUObject(this, &UAbyssVfxSystem::HandleTownPortal);
	Router.On<abyss::EvLootDropped>().AddUObject(this, &UAbyssVfxSystem::HandleLootDropped);
	Router.On<abyss::EvQuestUpdate>().AddUObject(this, &UAbyssVfxSystem::HandleQuestUpdate);
	Router.On<abyss::EvSkillVfx>().AddUObject(this, &UAbyssVfxSystem::HandleSkillVfx);
	Router.On<abyss::EvProjectileLaunched>().AddUObject(this, &UAbyssVfxSystem::HandleProjectileLaunched);
	Router.On<abyss::EvProjectileEnded>().AddUObject(this, &UAbyssVfxSystem::HandleProjectileEnded);
	Router.On<abyss::EvGroundEffectStarted>().AddUObject(this, &UAbyssVfxSystem::HandleGroundStarted);
	Router.On<abyss::EvGroundEffectTriggered>().AddUObject(this, &UAbyssVfxSystem::HandleGroundTriggered);
	Router.On<abyss::EvGroundEffectEnded>().AddUObject(this, &UAbyssVfxSystem::HandleGroundEnded);
	Router.OnSessionEnded.AddUObject(this, &UAbyssVfxSystem::ClearAll);
	bEventsBound = true;
}

void UAbyssVfxSystem::UnbindEvents()
{
	if (!bEventsBound)
	{
		return;
	}
	const UWorld* OwningWorld = GetWorld();
	if (UAbyssGameInstance* GameInstance = OwningWorld ? Cast<UAbyssGameInstance>(OwningWorld->GetGameInstance()) : nullptr)
	{
		GameInstance->GetEventRouter().RemoveAll(this);
	}
	bEventsBound = false;
}

void UAbyssVfxSystem::ApplyQuality(const FAbyssQualityProfile& Profile)
{
	Quality = Profile;
	MaxParticles = Quality.bMobile ? AbyssVfxSystemPrivate::MobileMaxParticles : AbyssVfxSystemPrivate::DesktopMaxParticles;
}

void UAbyssVfxSystem::ClearAll()
{
	Emitters.Reset();
	Particles.Reset();
	Lights.Reset();
	Projectiles.Reset();
	GroundEffects.Reset();
	StatusLoops.Reset();
	Anchors.Reset();
	FreeAnchors.Reset();
	PortalHandle = 0;
	// Hide every instance right away (the next Render would do it too, but a zone teardown may not render again).
	for (FPool& Pool : Pools)
	{
		UInstancedStaticMeshComponent* Component = PoolComponents.IsValidIndex(Pool.Component) ? PoolComponents[Pool.Component].Get() : nullptr;
		if (Component != nullptr && Pool.Capacity > 0)
		{
			Pool.Transforms.Init(AbyssVfxSystemPrivate::HiddenTransform(), Pool.Capacity);
			Component->BatchUpdateInstancesTransforms(0, Pool.Transforms, /*bWorldSpace*/ false, /*bMarkRenderStateDirty*/ true,
				/*bTeleport*/ true);
		}
		Pool.UsedLastFrame = 0;
	}
}

// =====================================================================================================================
// Renderer resources
// =====================================================================================================================

bool UAbyssVfxSystem::EnsureRenderer()
{
	if (bRendererResolved)
	{
		return bRendererAvailable;
	}
	UWorld* OwningWorld = GetWorld();
	UAbyssAssetLibrary* Assets = UAbyssAssetLibrary::Get(this);
	if (OwningWorld == nullptr || Assets == nullptr || !OwningWorld->HasBegunPlay())
	{
		return false;   // try again later (assets / world not ready)
	}
	bRendererResolved = true;

	static const TCHAR* const MeshFolders[] = { TEXT("FX"), TEXT("FX/Meshes") };
	QuadMesh = Cast<UStaticMesh>(Assets->LoadFromFolders(MeshFolders, FName(TEXT("SM_FX_Quad")), UStaticMesh::StaticClass()));
	if (QuadMesh == nullptr)
	{
		// Editor / uncooked fallback: the engine plane has the same layout (100 x 100 uu, XY plane, normal +Z).
		QuadMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	}
	AdditiveMaterial = Assets->LoadMaterial(TEXT("M_AF_FX_Additive"));
	TranslucentMaterial = Assets->LoadMaterial(TEXT("M_AF_FX_Translucent"));
	MeshMaterial = Assets->LoadMaterial(TEXT("M_AF_FX_Mesh"));
	FallbackSprite = Assets->LoadFxTexture(FName(TEXT("Glow")));
	if (QuadMesh == nullptr || AdditiveMaterial == nullptr)
	{
		UE_LOG(LogAbyss, Error, TEXT("VFX disabled: SM_FX_Quad / M_AF_FX_Additive missing (run Scripts/build_content.py)"));
		bRendererAvailable = false;
		return false;
	}
	if (TranslucentMaterial == nullptr)
	{
		TranslucentMaterial = AdditiveMaterial;
	}

	FActorSpawnParameters Params;
	Params.Name = MakeUniqueObjectName(OwningWorld->PersistentLevel, AActor::StaticClass(), FName(TEXT("AbyssVfx")));
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	RenderActor = OwningWorld->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (RenderActor == nullptr)
	{
		bRendererAvailable = false;
		return false;
	}
	USceneComponent* Root = NewObject<USceneComponent>(RenderActor, TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RenderActor->SetRootComponent(Root);
	Root->RegisterComponent();
	bRendererAvailable = true;
	return true;
}

int32 UAbyssVfxSystem::FindOrCreatePool(const FAbyssVfxLayer& Layer)
{
	UAbyssAssetLibrary* Assets = UAbyssAssetLibrary::Get(this);
	if (!EnsureRenderer() || Assets == nullptr)
	{
		return INDEX_NONE;
	}

	// Mesh layers: one pool per mesh (its own materials, or M_AF_FX_Mesh when it exists). Missing mesh -> sprite.
	if (!Layer.Mesh.IsNone())
	{
		const FName Key = AbyssVfxSystemPrivate::MakeId(TEXT("M:"), Layer.Mesh);
		if (const int32* Found = PoolIndexByKey.Find(Key))
		{
			return *Found;
		}
		if (UStaticMesh* Mesh = Assets->LoadStaticMeshByName(Layer.Mesh))
		{
			UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(RenderActor, NAME_None, RF_Transient);
			Component->SetStaticMesh(Mesh);
			Component->SetMobility(EComponentMobility::Movable);
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Component->SetGenerateOverlapEvents(false);
			Component->SetCanEverAffectNavigation(false);
			Component->SetCastShadow(false);
			Component->SetNumCustomDataFloats(AbyssVfxIcd::Count);
			if (MeshMaterial != nullptr)
			{
				for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
				{
					Component->SetMaterial(Slot, MeshMaterial);
				}
			}
			Component->SetupAttachment(RenderActor->GetRootComponent());
			Component->RegisterComponent();
			FPool& Pool = Pools.AddDefaulted_GetRef();
			Pool.Key = Key;
			Pool.bMesh = true;
			Pool.Blend = EAbyssVfxBlend::Translucent;
			Pool.Component = PoolComponents.Add(Component);
			Pool.MeshDiameterCm = FMath::Max(1.f, static_cast<float>(Mesh->GetBounds().SphereRadius * 2.0));
			const int32 Index = Pools.Num() - 1;
			PoolIndexByKey.Add(Key, Index);
			return Index;
		}
		PoolIndexByKey.Add(Key, INDEX_NONE);   // remember the miss; the caller falls back to the sprite
	}

	const bool bTranslucent = Layer.Blend == EAbyssVfxBlend::Translucent;
	const FName Key = AbyssVfxSystemPrivate::MakeId(bTranslucent ? TEXT("T:") : TEXT("A:"), Layer.Sprite);
	if (const int32* Found = PoolIndexByKey.Find(Key))
	{
		return *Found;
	}
	UTexture* Sprite = Assets->LoadFxTexture(Layer.Sprite);
	if (Sprite == nullptr)
	{
		Sprite = FallbackSprite;
	}
	UMaterialInterface* Base = bTranslucent ? TranslucentMaterial.Get() : AdditiveMaterial.Get();
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this);
	if (Sprite != nullptr)
	{
		Material->SetTextureParameterValue(FName(TEXT("Sprite")), Sprite);
	}
	Material->SetScalarParameterValue(FName(TEXT("Cells")), static_cast<float>(Library.GetSpriteCells(Layer.Sprite)));
	PoolMaterials.Add(Material);

	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(RenderActor, NAME_None, RF_Transient);
	Component->SetStaticMesh(QuadMesh);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);
	Component->SetCastShadow(false);
	Component->SetNumCustomDataFloats(AbyssVfxIcd::Count);
	Component->SetMaterial(0, Material);
	// Ground layers (decals, rings) under the upright ones; translucent bodies under additive glows.
	Component->SetTranslucentSortPriority(bTranslucent ? 1 : 2);
	Component->SetupAttachment(RenderActor->GetRootComponent());
	Component->RegisterComponent();

	FPool& Pool = Pools.AddDefaulted_GetRef();
	Pool.Key = Key;
	Pool.bMesh = false;
	Pool.Blend = Layer.Blend;
	Pool.Component = PoolComponents.Add(Component);
	const int32 Index = Pools.Num() - 1;
	PoolIndexByKey.Add(Key, Index);
	return Index;
}

// =====================================================================================================================
// Anchors
// =====================================================================================================================

FVector UAbyssVfxSystem::ResolveActorAnchor(const AActor* Actor, EAbyssVfxHeight Height, float CustomCm)
{
	using namespace AbyssVfxSystemPrivate;
	if (Actor == nullptr)
	{
		return FVector::ZeroVector;
	}
	if (Height == EAbyssVfxHeight::Ground || Height == EAbyssVfxHeight::Custom)
	{
		return Actor->GetActorLocation() + FVector(0.0, 0.0, Height == EAbyssVfxHeight::Custom ? CustomCm : 0.0);
	}
	if (const AAbyssCharacterActor* Character = Cast<AAbyssCharacterActor>(Actor))
	{
		return Character->GetAnchorLocation(ToAnchor(Height));
	}
	if (const AAbyssPropActor* Prop = Cast<AAbyssPropActor>(Actor))
	{
		return Prop->GetAnchorLocation(ToAnchor(Height));
	}
	return Actor->GetActorLocation() + FVector(0.0, 0.0, DefaultHeight(Height, CustomCm));
}

int32 UAbyssVfxSystem::AddAnchor(AActor* Actor, EAbyssVfxHeight Height, float HeightCm, const FVector& Fixed, uint32 ProjectileId)
{
	int32 Index = INDEX_NONE;
	if (FreeAnchors.Num() > 0)
	{
		Index = FreeAnchors.Pop(EAllowShrinking::No);
	}
	else
	{
		Index = Anchors.AddDefaulted();
	}
	FAnchor& Anchor = Anchors[Index];
	Anchor = FAnchor();
	Anchor.Actor = Actor;
	Anchor.Height = Height;
	Anchor.HeightCm = HeightCm;
	Anchor.Fixed = Fixed;
	Anchor.ProjectileId = ProjectileId;
	Anchor.bInUse = true;
	Anchor.Current = AnchorPosition(Index);
	return Index;
}

FVector UAbyssVfxSystem::AnchorPosition(int32 AnchorIndex) const
{
	using namespace AbyssVfxSystemPrivate;
	if (!Anchors.IsValidIndex(AnchorIndex))
	{
		return FVector::ZeroVector;
	}
	const FAnchor& Anchor = Anchors[AnchorIndex];
	if (const AActor* Actor = Anchor.Actor.Get())
	{
		return ResolveActorAnchor(Actor, Anchor.Height, Anchor.HeightCm);
	}
	if (Anchor.ProjectileId != 0)
	{
		return Anchor.Fixed;   // moved by UpdateProjectiles
	}
	return Anchor.Fixed + FVector(0.0, 0.0, DefaultHeight(Anchor.Height, Anchor.HeightCm));
}

void UAbyssVfxSystem::UpdateAnchors()
{
	for (int32 Index = 0; Index < Anchors.Num(); ++Index)
	{
		FAnchor& Anchor = Anchors[Index];
		if (!Anchor.bInUse)
		{
			continue;
		}
		if (Anchor.Actor.IsValid() || Anchor.ProjectileId != 0)
		{
			Anchor.Current = AnchorPosition(Index);
		}
		else if (!Anchor.Actor.IsExplicitlyNull())
		{
			// The actor is gone: freeze at its last position.
			Anchor.Actor.Reset();
			Anchor.Fixed = Anchor.Current;
			Anchor.Height = EAbyssVfxHeight::Ground;
		}
		else
		{
			Anchor.Current = AnchorPosition(Index);
		}
	}
}

void UAbyssVfxSystem::SweepAnchors()
{
	for (FAnchor& Anchor : Anchors)
	{
		Anchor.bMarked = false;
	}
	auto Mark = [this](int32 Index)
	{
		if (Anchors.IsValidIndex(Index))
		{
			Anchors[Index].bMarked = true;
		}
	};
	for (const FEmitter& Emitter : Emitters)
	{
		Mark(Emitter.SpawnAnchor);
		Mark(Emitter.BeamEndAnchor);
	}
	for (const FParticle& Particle : Particles)
	{
		Mark(Particle.Anchor);
		Mark(Particle.BeamEnd);
	}
	for (const FLight& Light : Lights)
	{
		Mark(Light.Anchor);
	}
	for (const TPair<uint32, FProjectile>& Pair : Projectiles)
	{
		Mark(Pair.Value.Anchor);
	}
	for (int32 Index = 0; Index < Anchors.Num(); ++Index)
	{
		FAnchor& Anchor = Anchors[Index];
		if (Anchor.bInUse && !Anchor.bMarked)
		{
			Anchor.bInUse = false;
			Anchor.Actor.Reset();
			FreeAnchors.Add(Index);
		}
	}
}

// =====================================================================================================================
// Playing
// =====================================================================================================================

FAbyssVfxPalette UAbyssVfxSystem::ResolvePalette(const FAbyssVfxRecipe& Recipe, const FAbyssVfxContext& Context) const
{
	static const FName Event(TEXT("event"));
	static const FName Element(TEXT("element"));
	if (Recipe.Palette == Element)
	{
		if (Context.bHasElement)
		{
			return Library.ElementPalette(Context.Element);
		}
		return Context.bHasColor ? FAbyssVfxPalette::FromColor(Context.Color) : Library.ElementPalette(abyss::DamageType::Physical);
	}
	if (Recipe.Palette != Event)
	{
		if (const FAbyssVfxPalette* Named = Library.FindPalette(Recipe.Palette))
		{
			return *Named;
		}
	}
	if (Context.bHasColor)
	{
		return FAbyssVfxPalette::FromColor(Context.Color);
	}
	return Context.bHasElement ? Library.ElementPalette(Context.Element) : Library.ElementPalette(abyss::DamageType::Physical);
}

int32 UAbyssVfxSystem::Play(FName RecipeId, const FAbyssVfxContext& Context)
{
	const FAbyssVfxRecipe* Recipe = Library.Find(RecipeId);
	if (Recipe == nullptr || !EnsureRenderer())
	{
		return 0;
	}
	const int32 Handle = NextHandle++;
	if (NextHandle <= 0)
	{
		NextHandle = 1;
	}
	const FAbyssVfxPalette Palette = ResolvePalette(*Recipe, Context);
	AActor* OriginActor = Context.OriginActor.Get();

	for (int32 LayerIndex = 0; LayerIndex < Recipe->Layers.Num(); ++LayerIndex)
	{
		const FAbyssVfxLayer& Layer = Recipe->Layers[LayerIndex];
		if ((Layer.BigFilter > 0 && !Context.bBig) || (Layer.BigFilter < 0 && Context.bBig))
		{
			continue;
		}
		const int32 PointCount = Layer.At == EAbyssVfxAnchor::Points ? Context.Points.Num() : 1;
		for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
		{
			FEmitter& Emitter = Emitters.AddDefaulted_GetRef();
			Emitter.Handle = Handle;
			Emitter.Recipe = Recipe;
			Emitter.LayerIndex = LayerIndex;
			Emitter.Context = Context;
			Emitter.Palette = Palette;
			Emitter.PointIndex = PointIndex;
			const float Stagger = Layer.StaggerMs >= 0.f ? Layer.StaggerMs : Context.StaggerMs;
			Emitter.StartDelayMs = Layer.DelayMs + Stagger * static_cast<float>(PointIndex);
			Emitter.bContinuous = Layer.Rate > 0.f;
			Emitter.SpawnAnchor = ResolveSpawnAnchor(Emitter, Layer.At, Layer.Height, Layer.HeightCm, PointIndex);
			if (Layer.Orient == EAbyssVfxOrient::Beam)
			{
				// Points beams go from the previous point (the caster's hand for the first) to this one.
				if (Layer.At == EAbyssVfxAnchor::Points)
				{
					Emitter.BeamEndAnchor = Emitter.SpawnAnchor;
					Emitter.SpawnAnchor = PointIndex == 0
						? AddAnchor(OriginActor, EAbyssVfxHeight::Hand, 0.f, Context.Origin)
						: ResolveSpawnAnchor(Emitter, EAbyssVfxAnchor::Points, Layer.Height, Layer.HeightCm, PointIndex - 1);
				}
				else
				{
					Emitter.BeamEndAnchor = ResolveSpawnAnchor(Emitter, Layer.BeamTo, Layer.BeamToHeight, 0.f, PointIndex);
				}
			}
		}
	}

	if (Recipe->LightRadiusCm > 0.f && Recipe->LightAlpha > 0.f)
	{
		const int32 LightAnchor = AddAnchor(OriginActor, EAbyssVfxHeight::Ground, 0.f, Context.Origin);
		StartLight(Handle, *Recipe, LightAnchor, Palette, Context);
	}
	if (Recipe->ShakeMs > 0.f && Recipe->ShakeIntensity > 0.f)
	{
		RequestShake(Recipe->ShakeMs, Recipe->ShakeIntensity);
	}
	return Handle;
}

int32 UAbyssVfxSystem::PlayFirst(TConstArrayView<FName> RecipeIds, const FAbyssVfxContext& Context)
{
	for (const FName Id : RecipeIds)
	{
		if (Library.Find(Id) != nullptr)
		{
			return Play(Id, Context);
		}
	}
	return 0;
}

void UAbyssVfxSystem::Stop(int32 Handle, bool bKillParticles)
{
	if (Handle == 0)
	{
		return;
	}
	for (FEmitter& Emitter : Emitters)
	{
		if (Emitter.Handle == Handle)
		{
			Emitter.bStopped = true;
		}
	}
	Lights.RemoveAllSwap([Handle](const FLight& Light) { return Light.Handle == Handle; });
	if (bKillParticles)
	{
		Particles.RemoveAllSwap([Handle](const FParticle& Particle) { return Particle.Handle == Handle; });
	}
}

bool UAbyssVfxSystem::IsPlaying(int32 Handle) const
{
	if (Handle == 0)
	{
		return false;
	}
	for (const FEmitter& Emitter : Emitters)
	{
		if (Emitter.Handle == Handle && !Emitter.bStopped)
		{
			return true;
		}
	}
	for (const FLight& Light : Lights)
	{
		if (Light.Handle == Handle)
		{
			return true;
		}
	}
	return false;
}

void UAbyssVfxSystem::SetHandleOrigin(int32 Handle, const FVector& Origin)
{
	for (FEmitter& Emitter : Emitters)
	{
		if (Emitter.Handle == Handle && Anchors.IsValidIndex(Emitter.SpawnAnchor) && !Anchors[Emitter.SpawnAnchor].Actor.IsValid())
		{
			Anchors[Emitter.SpawnAnchor].Fixed = Origin;
		}
	}
	for (FLight& Light : Lights)
	{
		if (Light.Handle == Handle && Anchors.IsValidIndex(Light.Anchor) && !Anchors[Light.Anchor].Actor.IsValid())
		{
			Anchors[Light.Anchor].Fixed = Origin;
		}
	}
}

void UAbyssVfxSystem::PlayNotify(AActor* Actor, FName NotifyName)
{
	if (Actor == nullptr || NotifyName.IsNone())
	{
		return;
	}
	const FName Ids[] = {
		AbyssVfxSystemPrivate::MakeId(TEXT("notify."), NotifyName),
		AbyssVfxSystemPrivate::MakeId(TEXT("npc."), NotifyName),
	};
	FAbyssVfxContext Context;
	Context.Origin = Actor->GetActorLocation();
	Context.Target = Context.Origin;
	Context.Point = Context.Origin;
	Context.OriginActor = Actor;
	Context.TargetActor = Actor;
	const FVector Forward = Actor->GetActorForwardVector();
	Context.Blow = FVector2D(Forward.X, Forward.Y).GetSafeNormal();
	PlayFirst(Ids, Context);
}

void UAbyssVfxSystem::StartLight(int32 Handle, const FAbyssVfxRecipe& Recipe, int32 Anchor, const FAbyssVfxPalette& Palette,
	const FAbyssVfxContext& Context)
{
	FLight& Light = Lights.AddDefaulted_GetRef();
	Light.Handle = Handle;
	Light.Anchor = Anchor;
	Light.RadiusCm = Recipe.LightRadiusCm;
	Light.Alpha = Recipe.LightAlpha;
	Light.bFlicker = Recipe.bLightFlicker;
	Light.bPersistent = Recipe.Layers.Num() == 0;
	Light.Seed = Random.FRandRange(0.f, 1000.f);
	Light.Color = Context.bHasColor ? Context.Color : Palette.Mid;
}

void UAbyssVfxSystem::RequestShake(float DurationMs, float Intensity)
{
	if (RealClockSec - LastShakeRealSec < AbyssVfxSystemPrivate::ShakeThrottleSec)
	{
		return;
	}
	LastShakeRealSec = RealClockSec;
	OnShake.Broadcast(DurationMs, Intensity);
}

int32 UAbyssVfxSystem::ResolveSpawnAnchor(const FEmitter& Emitter, EAbyssVfxAnchor At, EAbyssVfxHeight Height, float HeightCm,
	int32 PointIndex)
{
	const FAbyssVfxContext& Context = Emitter.Context;
	switch (At)
	{
	case EAbyssVfxAnchor::Origin:
		return AddAnchor(Context.OriginActor.Get(), Height, HeightCm, Context.Origin);
	case EAbyssVfxAnchor::Target:
		return AddAnchor(Context.TargetActor.Get(), Height, HeightCm, Context.Target);
	case EAbyssVfxAnchor::Point:
	{
		// A hit point next to the target actor uses the actor's sockets (chest of the monster that was hit).
		AActor* Near = Context.TargetActor.Get();
		if (Near != nullptr && FVector::DistSquared2D(Near->GetActorLocation(), Context.Point) > 150.0 * 150.0)
		{
			Near = nullptr;
		}
		return AddAnchor(Height == EAbyssVfxHeight::Ground || Height == EAbyssVfxHeight::Custom ? nullptr : Near, Height, HeightCm,
			Context.Point);
	}
	case EAbyssVfxAnchor::Points:
	{
		const FVector P = Context.Points.IsValidIndex(PointIndex) ? Context.Points[PointIndex] : Context.Point;
		return AddAnchor(nullptr, Height, HeightCm, P);
	}
	case EAbyssVfxAnchor::Path:
		return AddAnchor(nullptr, Height, HeightCm, Context.Origin);   // per-particle lerp origin -> target
	case EAbyssVfxAnchor::Camera:
		return AddAnchor(nullptr, Height, HeightCm, CameraFocus);
	}
	return AddAnchor(nullptr, Height, HeightCm, Context.Origin);
}

// =====================================================================================================================
// Emission
// =====================================================================================================================

void UAbyssVfxSystem::UpdateEmitters(float DeltaMs)
{
	for (int32 Index = 0; Index < Emitters.Num();)
	{
		FEmitter& Emitter = Emitters[Index];
		const FAbyssVfxLayer& Layer = Emitter.Recipe->Layers[Emitter.LayerIndex];
		Emitter.ElapsedMs += DeltaMs;
		const float Active = Emitter.ElapsedMs - Emitter.StartDelayMs;
		bool bDone = false;
		if (Active >= 0.f)
		{
			if (!Emitter.bContinuous)
			{
				if (!Emitter.bBurstDone && !Emitter.bStopped)
				{
					float Count = Layer.bCountFromContext ? static_cast<float>(Emitter.Context.SparkCount)
														  : Layer.Count.Sample(Random);
					Count *= Emitter.Context.CountScale;
					if (Layer.bCountByRadius)
					{
						Count *= FMath::Max(1.f, Emitter.Context.RadiusCm / 100.f);
					}
					SpawnFromEmitter(Emitter, FMath::Max(0, FMath::RoundToInt32(Count)));
					Emitter.bBurstDone = true;
				}
				bDone = true;
			}
			else
			{
				const bool bExpired = Layer.DurationMs >= 0.f && Active >= Layer.DurationMs;
				if (Emitter.bStopped || bExpired)
				{
					bDone = true;
				}
				else
				{
					// Web: emission interval x quality multiplier -> rate / multiplier.
					const float Multiplier = FMath::Max(0.25f, Quality.ParticleFrequencyMultiplier);
					const float Rate = Layer.Rate / Multiplier;
					Emitter.SpawnAccumulator += FMath::Min(DeltaMs, AbyssVfxSystemPrivate::MaxEmitterBacklogMs) * Rate / 1000.f;
					const int32 Count = FMath::FloorToInt32(Emitter.SpawnAccumulator);
					if (Count > 0)
					{
						Emitter.SpawnAccumulator -= static_cast<float>(Count);
						SpawnFromEmitter(Emitter, Count);
					}
				}
			}
		}
		else if (Emitter.bStopped && Emitter.bContinuous)
		{
			bDone = true;
		}
		if (bDone)
		{
			Emitters.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
		else
		{
			++Index;
		}
	}
}

void UAbyssVfxSystem::SpawnFromEmitter(FEmitter& Emitter, int32 Count)
{
	const FAbyssVfxLayer& Layer = Emitter.Recipe->Layers[Emitter.LayerIndex];
	float UnitScale = 1.f;
	switch (Layer.Unit)
	{
	case EAbyssVfxUnit::Ring: UnitScale = Emitter.Context.RingRadiusCm; break;
	case EAbyssVfxUnit::Radius: UnitScale = Emitter.Context.RadiusCm; break;
	case EAbyssVfxUnit::Cm: break;
	}
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (Particles.Num() >= MaxParticles)
		{
			return;
		}
		SpawnParticle(Emitter, Layer, UnitScale);
	}
}

void UAbyssVfxSystem::SpawnParticle(FEmitter& Emitter, const FAbyssVfxLayer& Layer, float UnitScale)
{
	using namespace AbyssVfxSystemPrivate;
	int32 PoolIndex = FindOrCreatePool(Layer);
	bool bMesh = PoolIndex != INDEX_NONE && Pools[PoolIndex].bMesh;
	if (PoolIndex == INDEX_NONE && !Layer.Mesh.IsNone())
	{
		// Missing FX mesh: draw the layer's sprite instead.
		FAbyssVfxLayer SpriteLayer = Layer;
		SpriteLayer.Mesh = NAME_None;
		PoolIndex = FindOrCreatePool(SpriteLayer);
		bMesh = false;
	}
	if (PoolIndex == INDEX_NONE)
	{
		return;
	}
	const FAbyssVfxContext& Context = Emitter.Context;
	const uint32 Serial = Emitter.SpawnCounter++;

	FParticle& P = Particles.AddDefaulted_GetRef();
	P.Pool = PoolIndex;
	P.Handle = Emitter.Handle;
	P.bMesh = bMesh;
	P.MeshDiameterCm = bMesh ? Pools[PoolIndex].MeshDiameterCm : 100.f;
	P.Orient = Layer.Orient;
	P.LifeMs = FMath::Max(1.f, Layer.LifeMs.Sample(Random));
	P.Size0 = FMath::Max(0.1f, Layer.SizeCm.Sample(Random) * UnitScale);
	P.Size1 = P.Size0 * Layer.Grow;
	P.SizePow = Layer.SizePow;
	P.Aspect = Layer.Aspect;
	P.Stretch = Layer.StretchPerSpeed;
	P.Alpha0 = Layer.Alpha0;
	P.Alpha1 = Layer.Alpha1;
	P.FadeIn = Layer.FadeIn;
	P.AlphaPow = Layer.AlphaPow;
	P.Flicker = Layer.Flicker;
	P.Gravity = Layer.GravityCmS2;
	P.Drag = Layer.Drag;
	P.SpinDegS = Layer.SpinDegS.Sample(Random);
	P.RotationDeg = Layer.RotationDeg.Sample(Random);
	P.Variant = Layer.RandomVariants > 1 ? static_cast<float>(Random.RandHelper(Layer.RandomVariants)) : static_cast<float>(Layer.Variant);

	// Colour (+ alternate every Nth), HDR intensity.
	const bool bAlt = Layer.AltEvery > 0 && (Serial % static_cast<uint32>(Layer.AltEvery)) == static_cast<uint32>(Layer.AltEvery - 1);
	const EAbyssVfxColor Source = bAlt ? Layer.AltColorSource : Layer.ColorSource;
	FLinearColor Color = Source == EAbyssVfxColor::Fixed ? (bAlt ? Layer.AltColor : Layer.Color)
														 : Emitter.Palette.Get(Source, Context.Color, Context.bHasColor);
	Color *= Layer.Intensity;
	Color.A = 1.f;
	P.Color = Color;

	// Frame of the anchor: X along the blow, Y right, Z up.
	FVector2D Blow = Context.Blow.IsNearlyZero() ? FVector2D(1.0, 0.0) : Context.Blow.GetSafeNormal();
	const FVector Forward(Blow.X, Blow.Y, 0.0);
	const FVector Right(-Blow.Y, Blow.X, 0.0);
	if (Layer.bAlignToBlow)
	{
		// Texture "up" along the blow: ground quads rotate in the plane, billboards by the screen angle of the blow.
		if (P.Orient == EAbyssVfxOrient::Ground)
		{
			P.RotationDeg += FMath::RadiansToDegrees(FMath::Atan2(-Blow.X, Blow.Y));
		}
		else
		{
			// Screen direction of the blow (s_x, s_y): texture up = (-sin r, cos r) on screen -> r = atan2(-s_x, s_y).
			const float ScreenX = static_cast<float>(FVector::DotProduct(Forward, CameraRight));
			const float ScreenY = static_cast<float>(FVector::DotProduct(Forward, CameraUp));
			P.RotationDeg += FMath::RadiansToDegrees(FMath::Atan2(-ScreenX, ScreenY));
		}
	}

	// Spawn point.
	FVector AnchorPos = AnchorPosition(Emitter.SpawnAnchor);
	if (Layer.At == EAbyssVfxAnchor::Path)
	{
		const FVector Start = Context.OriginActor.IsValid()
			? ResolveActorAnchor(Context.OriginActor.Get(), Layer.Height, Layer.HeightCm)
			: Context.Origin + FVector(0.0, 0.0, DefaultHeight(Layer.Height, Layer.HeightCm));
		const FVector End = Context.TargetActor.IsValid()
			? ResolveActorAnchor(Context.TargetActor.Get(), Layer.Height, Layer.HeightCm)
			: Context.Target + FVector(0.0, 0.0, DefaultHeight(Layer.Height, Layer.HeightCm));
		AnchorPos = FMath::Lerp(Start, End, Random.FRand());
	}
	FVector Offset = Forward * Layer.OffsetCm.X + Right * Layer.OffsetCm.Y + FVector(0.0, 0.0, Layer.OffsetCm.Z);
	FVector Disc = FVector::ZeroVector;
	const float SpawnRadius = Layer.SpawnRadiusCm * UnitScale;
	if (SpawnRadius > 0.f)
	{
		const float Angle = Random.FRandRange(0.f, 2.f * UE_PI);
		const float Radius = Layer.bSpawnOnRing ? SpawnRadius : SpawnRadius * FMath::Sqrt(Random.FRand());
		Disc = FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0);
	}
	if (Layer.SpawnHeightJitterCm > 0.f)
	{
		Disc.Z = Random.FRandRange(-Layer.SpawnHeightJitterCm * 0.5f, Layer.SpawnHeightJitterCm * 0.5f);
	}
	Offset += Disc;

	// Velocity.
	FVector Direction = FVector::UpVector;
	switch (Layer.Dir)
	{
	case EAbyssVfxDir::Random: Direction = ConeDirection(Random, FVector::UpVector, Layer.ConeDeg >= 359.f ? 360.f : Layer.ConeDeg); break;
	case EAbyssVfxDir::Up: Direction = ConeDirection(Random, FVector::UpVector, Layer.ConeDeg >= 359.f ? 0.f : Layer.ConeDeg); break;
	case EAbyssVfxDir::Down: Direction = ConeDirection(Random, -FVector::UpVector, Layer.ConeDeg >= 359.f ? 0.f : Layer.ConeDeg); break;
	case EAbyssVfxDir::Blow:
	case EAbyssVfxDir::Back:
	{
		const FVector2D Axis = Layer.Dir == EAbyssVfxDir::Blow ? Blow : -Blow;
		Direction = FlatCone(Random, Axis, Layer.ConeDeg >= 359.f ? 0.f : Layer.ConeDeg);
		Direction.Z = Random.FRandRange(-0.15f, 0.35f);
		Direction.Normalize();
		break;
	}
	case EAbyssVfxDir::Out:
	case EAbyssVfxDir::In:
	case EAbyssVfxDir::Tangent:
	{
		FVector2D Radial(Disc.X, Disc.Y);
		if (Radial.IsNearlyZero())
		{
			const float Angle = Random.FRandRange(0.f, 2.f * UE_PI);
			Radial = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
		}
		Radial.Normalize();
		if (Layer.Dir == EAbyssVfxDir::In)
		{
			Radial = -Radial;
		}
		else if (Layer.Dir == EAbyssVfxDir::Tangent)
		{
			Radial = FVector2D(-Radial.Y, Radial.X);
		}
		Direction = FlatCone(Random, Radial, Layer.ConeDeg >= 359.f ? 0.f : Layer.ConeDeg);
		break;
	}
	case EAbyssVfxDir::Flat:
	{
		const float Angle = Random.FRandRange(0.f, 2.f * UE_PI);
		Direction = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0);
		break;
	}
	}
	P.Velocity = Direction * Layer.SpeedCmS.Sample(Random) + FVector(0.0, 0.0, Layer.UpCmS.Sample(Random));
	if (P.Orient == EAbyssVfxOrient::Ground && Layer.Dir == EAbyssVfxDir::Out && !P.Velocity.IsNearlyZero())
	{
		// Ground crescents flying outward (taunt shouts) face their flight.
		P.RotationDeg += FMath::RadiansToDegrees(FMath::Atan2(-P.Velocity.X, P.Velocity.Y));
	}

	// Orbit.
	if (Layer.bOrbit)
	{
		P.bOrbit = true;
		P.OrbitR0 = Layer.OrbitRadius0Cm.Sample(Random) * UnitScale;
		P.OrbitR1 = Layer.OrbitRadius1Cm.Sample(Random) * UnitScale;
		P.OrbitDegS = Layer.OrbitDegS.Sample(Random);
		const float Count = FMath::Max(1.f, Layer.Count.Max);
		P.OrbitAngleDeg = Layer.Rate > 0.f ? Random.FRandRange(0.f, 360.f) : 360.f * static_cast<float>(Serial) / Count;
	}

	// Attached particles store their offset from the anchor; free particles their world position.
	if (Layer.bAttach || P.bOrbit)
	{
		P.Anchor = Emitter.SpawnAnchor;
		P.Position = Offset;
	}
	else
	{
		P.Position = AnchorPos + Offset;
	}
	if (P.Orient == EAbyssVfxOrient::Beam)
	{
		P.Anchor = Emitter.SpawnAnchor;
		P.BeamEnd = Emitter.BeamEndAnchor;
		P.Position = FVector::ZeroVector;
	}
}

// =====================================================================================================================
// Simulation and rendering
// =====================================================================================================================

void UAbyssVfxSystem::UpdateParticles(float DeltaSec)
{
	const float DeltaMs = DeltaSec * 1000.f;
	for (int32 Index = 0; Index < Particles.Num();)
	{
		FParticle& P = Particles[Index];
		P.AgeMs += DeltaMs;
		if (P.AgeMs >= P.LifeMs)
		{
			Particles.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			continue;
		}
		if (P.Drag > 0.f)
		{
			P.Velocity *= FMath::Exp(-P.Drag * DeltaSec);
		}
		P.Velocity.Z -= P.Gravity * DeltaSec;
		P.Position += P.Velocity * DeltaSec;
		P.RotationDeg += P.SpinDegS * DeltaSec;
		if (P.bOrbit)
		{
			P.OrbitAngleDeg += P.OrbitDegS * DeltaSec;
		}
		++Index;
	}
}

void UAbyssVfxSystem::UpdateLights(float DeltaSec)
{
	if (Lights.Num() == 0)
	{
		return;
	}
	// A light lives while its handle still has emitters or particles (light-only recipes: until Stop).
	TSet<int32> LiveHandles;
	for (const FEmitter& Emitter : Emitters)
	{
		LiveHandles.Add(Emitter.Handle);
	}
	for (const FParticle& Particle : Particles)
	{
		LiveHandles.Add(Particle.Handle);
	}
	Lights.RemoveAllSwap([this, &LiveHandles](const FLight& Light)
	{
		if (!Anchors.IsValidIndex(Light.Anchor) || !Anchors[Light.Anchor].bInUse)
		{
			return true;
		}
		return !Light.bPersistent && !LiveHandles.Contains(Light.Handle);
	});
}

void UAbyssVfxSystem::Render()
{
	using namespace AbyssVfxSystemPrivate;
	for (FPool& Pool : Pools)
	{
		Pool.Transforms.Reset();
		Pool.CustomData.Reset();
	}

	auto Emit = [this](int32 PoolIndex, const FTransform& Transform, const FLinearColor& Color, float Alpha, float Variant, float Age)
	{
		FPool& Pool = Pools[PoolIndex];
		Pool.Transforms.Add(Transform);
		Pool.CustomData.Append({ Color.R, Color.G, Color.B, Alpha, Variant, Age });
	};

	for (const FParticle& P : Particles)
	{
		const float T = FMath::Clamp(P.AgeMs / P.LifeMs, 0.f, 1.f);
		const float SizeT = 1.f - FMath::Pow(1.f - T, P.SizePow);
		const float Size = FMath::Lerp(P.Size0, P.Size1, SizeT);
		float Alpha = 0.f;
		if (P.FadeIn > 0.f && T < P.FadeIn)
		{
			Alpha = P.Alpha0 * (T / P.FadeIn);
		}
		else
		{
			const float U = P.FadeIn < 1.f ? (T - P.FadeIn) / (1.f - P.FadeIn) : 1.f;
			Alpha = P.Alpha0 + (P.Alpha1 - P.Alpha0) * FMath::Pow(FMath::Clamp(U, 0.f, 1.f), P.AlphaPow);
		}
		if (P.Flicker > 0.f)
		{
			Alpha *= 1.f - P.Flicker * Random.FRand();
		}
		if (Alpha <= 0.002f || Size <= 0.01f)
		{
			continue;
		}

		FVector Position = P.Position;
		if (P.Anchor != INDEX_NONE && P.Orient != EAbyssVfxOrient::Beam)
		{
			const FVector AnchorPos = Anchors.IsValidIndex(P.Anchor) ? Anchors[P.Anchor].Current : FVector::ZeroVector;
			if (P.bOrbit)
			{
				const float Radius = FMath::Lerp(P.OrbitR0, P.OrbitR1, T);
				const float Angle = FMath::DegreesToRadians(P.OrbitAngleDeg);
				Position = AnchorPos + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0) + P.Position;
			}
			else
			{
				Position = AnchorPos + P.Position;
			}
		}

		const float Rotation = FMath::DegreesToRadians(P.RotationDeg);
		const float Cos = FMath::Cos(Rotation);
		const float Sin = FMath::Sin(Rotation);
		FTransform Transform;
		if (P.bMesh)
		{
			const float Scale = Size / FMath::Max(1.f, P.MeshDiameterCm);
			FQuat Rotator = FQuat::Identity;
			if (P.Orient == EAbyssVfxOrient::Velocity && !P.Velocity.IsNearlyZero())
			{
				Rotator = FRotationMatrix::MakeFromYZ(P.Velocity.GetSafeNormal(), FVector::UpVector).ToQuat();
			}
			else if (P.Orient == EAbyssVfxOrient::Ground || P.Orient == EAbyssVfxOrient::Upright)
			{
				Rotator = FQuat(FVector::UpVector, Rotation);
			}
			else
			{
				Rotator = FRotator(P.RotationDeg * 0.7f, P.RotationDeg, P.RotationDeg * 1.3f).Quaternion();
			}
			Transform = FTransform(Rotator, Position, FVector(Scale));
		}
		else
		{
			const float Height = Size / 100.f;
			const float Width = Size * P.Aspect / 100.f;
			switch (P.Orient)
			{
			case EAbyssVfxOrient::Billboard:
			{
				const FVector X = CameraRight * Cos + CameraUp * Sin;
				const FVector Y = CameraUp * Cos - CameraRight * Sin;
				Transform = AxesTransform(Position, X, Y, Width, Height);
				break;
			}
			case EAbyssVfxOrient::Ground:
			{
				const FVector X(Cos, Sin, 0.0);
				const FVector Y(-Sin, Cos, 0.0);
				Transform = AxesTransform(Position, X, Y, Width, Height);
				break;
			}
			case EAbyssVfxOrient::Upright:
			{
				FVector FlatRight(CameraRight.X, CameraRight.Y, 0.0);
				FlatRight = FlatRight.GetSafeNormal(UE_SMALL_NUMBER, FVector(1.0, 0.0, 0.0));
				const FVector X = FlatRight * Cos + FVector::UpVector * Sin;
				const FVector Y = FVector::UpVector * Cos - FlatRight * Sin;
				// Upright quads stand on their base: shift the centre up by half the height.
				Transform = AxesTransform(Position + Y * (Size * 0.5f), X, Y, Width, Height);
				break;
			}
			case EAbyssVfxOrient::Velocity:
			{
				const float Speed = static_cast<float>(P.Velocity.Size());
				const float ScreenX = static_cast<float>(FVector::DotProduct(P.Velocity, CameraRight));
				const float ScreenY = static_cast<float>(FVector::DotProduct(P.Velocity, CameraUp));
				FVector Y = CameraUp;
				if (FMath::Abs(ScreenX) + FMath::Abs(ScreenY) > UE_KINDA_SMALL_NUMBER)
				{
					Y = (CameraRight * ScreenX + CameraUp * ScreenY).GetSafeNormal();
				}
				const FVector X = FVector::CrossProduct(Y, CameraForward).GetSafeNormal();
				const float Length = (Size + P.Stretch * Speed) / 100.f;
				Transform = AxesTransform(Position, X, Y, Width, Length);
				break;
			}
			case EAbyssVfxOrient::Beam:
			{
				const FVector A = Anchors.IsValidIndex(P.Anchor) ? Anchors[P.Anchor].Current : Position;
				const FVector B = Anchors.IsValidIndex(P.BeamEnd) ? Anchors[P.BeamEnd].Current : A + FVector(0.0, 0.0, 100.0);
				const FVector Span = B - A;
				const float Length = static_cast<float>(Span.Size());
				if (Length < 1.f)
				{
					continue;
				}
				const FVector Y = Span / Length;
				FVector X = FVector::CrossProduct(Y, CameraForward);
				X = X.GetSafeNormal(UE_SMALL_NUMBER, CameraRight);
				Transform = AxesTransform((A + B) * 0.5, X, Y, Size / 100.f, Length / 100.f);
				break;
			}
			}
		}
		Emit(P.Pool, Transform, P.Color, Alpha, P.Variant, T);
	}

	// Fake light pools (ue58-platform.md 6.2): additive ground glows, nearest to the focus first, capped by the tier.
	if (Lights.Num() > 0)
	{
		FAbyssVfxLayer PoolLayer;
		PoolLayer.Sprite = FName(TEXT("LightPool"));
		const int32 PoolIndex = FindOrCreatePool(PoolLayer);
		if (PoolIndex != INDEX_NONE)
		{
			TArray<int32, TInlineAllocator<64>> Order;
			for (int32 Index = 0; Index < Lights.Num(); ++Index)
			{
				Order.Add(Index);
			}
			const FVector Focus = CameraFocus;
			Order.Sort([this, Focus](int32 A, int32 B)
			{
				return FVector::DistSquared2D(Anchors[Lights[A].Anchor].Current, Focus)
					< FVector::DistSquared2D(Anchors[Lights[B].Anchor].Current, Focus);
			});
			const int32 Limit = FMath::Min(Order.Num(), FMath::Max(1, Quality.MaxDynamicLights));
			const double TimeMs = RealClockSec * 1000.0;
			for (int32 Rank = 0; Rank < Limit; ++Rank)
			{
				const FLight& Light = Lights[Order[Rank]];
				float Alpha = Light.Alpha;
				if (Light.bFlicker)
				{
					// world-map-nav.md 14.1: + sin(t x 0.007 + s) x 0.05 + sin(t x 0.013 + 2.3 s) x 0.03
					Alpha += static_cast<float>(FMath::Sin(TimeMs * 0.007 + Light.Seed) * 0.05
						+ FMath::Sin(TimeMs * 0.013 + 2.3 * Light.Seed) * 0.03);
				}
				const FVector Ground = Anchors[Light.Anchor].Current + FVector(0.0, 0.0, 2.0);
				const float Diameter = Light.RadiusCm * 2.f / 100.f;
				Emit(PoolIndex, AxesTransform(Ground, FVector::ForwardVector, FVector::RightVector, Diameter, Diameter),
					Light.Color, FMath::Clamp(Alpha, 0.f, 1.f) * 0.5f, 0.f, 0.f);
			}
		}
	}

	// Upload.
	for (FPool& Pool : Pools)
	{
		UInstancedStaticMeshComponent* Component = PoolComponents.IsValidIndex(Pool.Component) ? PoolComponents[Pool.Component].Get() : nullptr;
		if (Component == nullptr)
		{
			continue;
		}
		const int32 Used = Pool.Transforms.Num();
		if (Used == 0 && Pool.UsedLastFrame == 0)
		{
			continue;
		}
		if (Used > Pool.Capacity)
		{
			const int32 Grow = FMath::Max(Used - Pool.Capacity, 16);
			for (int32 Add = 0; Add < Grow; ++Add)
			{
				Component->AddInstance(HiddenTransform(), /*bWorldSpace*/ false);
			}
			Pool.Capacity += Grow;
		}
		const int32 Touched = FMath::Max(Used, Pool.UsedLastFrame);
		Pool.Transforms.Reserve(Touched);
		for (int32 Index = Used; Index < Touched; ++Index)
		{
			Pool.Transforms.Add(HiddenTransform());
			Pool.CustomData.Append({ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f });
		}
		Component->BatchUpdateInstancesTransforms(0, Pool.Transforms, /*bWorldSpace*/ false, /*bMarkRenderStateDirty*/ false,
			/*bTeleport*/ true);
		for (int32 Index = 0; Index < Touched; ++Index)
		{
			Component->SetCustomData(Index, TArrayView<const float>(Pool.CustomData.GetData() + Index * AbyssVfxIcd::Count,
				AbyssVfxIcd::Count), /*bMarkRenderStateDirty*/ false);
		}
		Component->MarkRenderStateDirty();
		Pool.UsedLastFrame = Used;
	}
}

void UAbyssVfxSystem::Tick(float VisualDeltaSec, const FAbyssFrameInfo& Frame, const abyss::Snapshot& Snap)
{
	RealClockSec += Frame.RealDeltaMs / 1000.0;
	RenderSimMs = Frame.RenderSimMs;
	++FrameCounter;
	if (!bEventsBound)
	{
		BindEvents();
	}

	// Camera basis (billboards).
	if (const UWorld* OwningWorld = GetWorld())
	{
		if (const APlayerController* Controller = OwningWorld->GetFirstPlayerController())
		{
			if (const APlayerCameraManager* CameraManager = Controller->PlayerCameraManager.Get())
			{
				CameraLocation = CameraManager->GetCameraLocation();
				const FRotationMatrix Basis(CameraManager->GetCameraRotation());
				CameraForward = Basis.GetUnitAxis(EAxis::X);
				CameraRight = Basis.GetUnitAxis(EAxis::Y);
				CameraUp = Basis.GetUnitAxis(EAxis::Z);
				// Focus: the camera ray on the ground plane (ambient volumes around it).
				if (CameraForward.Z < -0.05)
				{
					const double T = -CameraLocation.Z / CameraForward.Z;
					CameraFocus = CameraLocation + CameraForward * T;
					CameraFocus.Z = GroundZ(CameraFocus.X, CameraFocus.Y);
				}
			}
		}
	}
	// Camera-anchored emitters follow the focus.
	for (FEmitter& Emitter : Emitters)
	{
		const FAbyssVfxLayer& Layer = Emitter.Recipe->Layers[Emitter.LayerIndex];
		if (Layer.At == EAbyssVfxAnchor::Camera && Anchors.IsValidIndex(Emitter.SpawnAnchor))
		{
			Anchors[Emitter.SpawnAnchor].Fixed = CameraFocus;
		}
	}

	UpdateStatusLoops(Snap);
	UpdateProjectiles(Frame);
	UpdateAnchors();
	const float DeltaSec = FMath::Clamp(VisualDeltaSec, 0.f, 0.25f);
	UpdateEmitters(DeltaSec * 1000.f);
	UpdateParticles(DeltaSec);
	if ((FrameCounter & 31u) == 0u)
	{
		SweepAnchors();
	}
	UpdateLights(DeltaSec);
	if (bRendererAvailable)
	{
		Render();
	}
}

// =====================================================================================================================
// World queries
// =====================================================================================================================

AActor* UAbyssVfxSystem::FindActor(abyss::EntityId Id) const
{
	const UAbyssActorRegistry* Registry = UAbyssActorRegistry::Get(this);
	return Registry != nullptr && Id != abyss::kNoEntity ? Registry->Find(Id) : nullptr;
}

double UAbyssVfxSystem::GroundZ(double X, double Y) const
{
	const UAbyssSimDriver* Driver = UAbyssSimDriver::Get(this);
	const IAbyssWorldView* View = Driver != nullptr ? Driver->GetWorldView() : nullptr;
	return View != nullptr ? View->GetGroundHeight(X, Y) : 0.0;
}

FVector UAbyssVfxSystem::TileToGround(const abyss::Vec2& Tile) const
{
	FVector World = AbyssUnits::TileToWorld(Tile);
	World.Z = GroundZ(World.X, World.Y);
	return World;
}

bool UAbyssVfxSystem::EntityGround(abyss::EntityId Id, FVector& Out) const
{
	if (const AActor* Actor = FindActor(Id))
	{
		Out = Actor->GetActorLocation();
		return true;
	}
	const UAbyssSimDriver* Driver = UAbyssSimDriver::Get(this);
	abyss::Vec2 Prev;
	abyss::Vec2 Cur;
	if (Driver != nullptr && Driver->GetSnapshotIndex().FindEntityPosition(Id, Prev, Cur))
	{
		Out = TileToGround(Cur);
		return true;
	}
	if (Id == abyss::kHeroEntityId && Driver != nullptr && Driver->GetSnapshotIndex().GetSnapshot() != nullptr)
	{
		Out = TileToGround(Driver->GetSnapshotIndex().GetSnapshot()->hero.pos);
		return true;
	}
	return false;
}

// =====================================================================================================================
// Projectiles
// =====================================================================================================================

void UAbyssVfxSystem::UpdateProjectiles(const FAbyssFrameInfo& Frame)
{
	for (TPair<uint32, FProjectile>& Pair : Projectiles)
	{
		FProjectile& Projectile = Pair.Value;
		const double T = FMath::Clamp((Frame.RenderSimMs - Projectile.LaunchMs) / FMath::Max(1.0, Projectile.TravelMs), 0.0, 1.0);
		FVector To = Projectile.To;
		if (const AActor* Target = Projectile.TargetActor.Get())
		{
			To = ResolveActorAnchor(Target, EAbyssVfxHeight::Chest, 0.f);
		}
		FVector Position = FMath::Lerp(Projectile.From, To, T);
		Position.Z += Projectile.ArcCm * 4.0 * T * (1.0 - T);
		Projectile.Position = Position;
		if (Anchors.IsValidIndex(Projectile.Anchor))
		{
			Anchors[Projectile.Anchor].Fixed = Position;
		}
	}
}

void UAbyssVfxSystem::HandleProjectileLaunched(const abyss::EvProjectileLaunched& Event)
{
	using namespace AbyssVfxSystemPrivate;
	if (!EnsureRenderer())
	{
		return;
	}
	FProjectile Projectile;
	Projectile.Id = Event.projectile;
	Projectile.Kind = Event.kind;
	Projectile.VfxId = FName(*AbyssText::ToFString(Event.vfxId));
	Projectile.LaunchMs = Event.launchMs;
	Projectile.TravelMs = FMath::Max(1.0, Event.travelMs);
	Projectile.bHasColor = Event.color != 0;
	Projectile.Color = AbyssWorldUtil::ColorFromRgb(Event.color);
	AActor* Source = FindActor(Event.source);
	AActor* Target = FindActor(Event.target);
	Projectile.From = Source != nullptr ? ResolveActorAnchor(Source, EAbyssVfxHeight::Hand, 0.f)
										: TileToGround(Event.from) + FVector(0.0, 0.0, DefaultHandCm);
	Projectile.To = Target != nullptr ? ResolveActorAnchor(Target, EAbyssVfxHeight::Chest, 0.f)
									  : TileToGround(Event.to) + FVector(0.0, 0.0, DefaultChestCm * 0.75f);
	Projectile.TargetActor = Target;
	Projectile.Position = Projectile.From;

	const FName KindId = Event.kind == abyss::ProjectileKind::MonsterBolt
		? FName(TEXT("proj.monster_bolt"))
		: (Event.kind == abyss::ProjectileKind::PetBolt ? FName(TEXT("proj.pet_bolt")) : FName(TEXT("proj.default")));
	const FName Ids[] = { MakeId(TEXT("proj."), Projectile.VfxId), KindId, FName(TEXT("proj.default")) };
	const FAbyssVfxRecipe* Recipe = Library.FindFirst(Ids);
	Projectile.ArcCm = Recipe != nullptr ? Recipe->ArcCm : 0.f;
	Projectile.Anchor = AddAnchor(nullptr, EAbyssVfxHeight::Ground, 0.f, Projectile.From, Projectile.Id);

	FAbyssVfxContext Context;
	Context.Origin = Projectile.From;
	Context.Target = Projectile.To;
	Context.Point = Projectile.To;
	Context.OriginActor = Source;
	Context.TargetActor = Target;
	Context.bHasColor = Projectile.bHasColor;
	Context.Color = Projectile.Color;
	Context.Blow = FVector2D(Projectile.To - Projectile.From).GetSafeNormal();
	if (const UAbyssGameInstance* GameInstance = UAbyssGameInstance::Get(this))
	{
		if (const abyss::DataStore* Data = GameInstance->GetData())
		{
			if (const abyss::SkillDef* Skill = Data->FindSkill(Event.vfxId))
			{
				Context.bHasElement = true;
				Context.Element = Skill->damageType;
			}
		}
	}
	// Launch flash (optional), then the head: every emitter of the head recipe spawns at the projectile anchor.
	Play(MakeId(TEXT("proj."), Projectile.VfxId, TEXT(".launch")), Context);
	if (Recipe != nullptr)
	{
		FAbyssVfxContext HeadContext = Context;
		HeadContext.OriginActor = nullptr;
		Projectile.Handle = Play(Recipe->Id, HeadContext);
		for (FEmitter& Emitter : Emitters)
		{
			if (Emitter.Handle == Projectile.Handle)
			{
				Emitter.SpawnAnchor = Projectile.Anchor;
			}
		}
	}
	Projectiles.Add(Projectile.Id, Projectile);
}

void UAbyssVfxSystem::HandleProjectileEnded(const abyss::EvProjectileEnded& Event)
{
	using namespace AbyssVfxSystemPrivate;
	FProjectile Projectile;
	if (!Projectiles.RemoveAndCopyValue(Event.projectile, Projectile))
	{
		return;
	}
	Stop(Projectile.Handle, false);
	FAbyssVfxContext Context;
	Context.Origin = Projectile.From;
	Context.Point = Event.hit ? (Projectile.TargetActor.IsValid() ? Projectile.TargetActor->GetActorLocation() : Projectile.To)
							  : Projectile.Position;
	Context.Point.Z = GroundZ(Context.Point.X, Context.Point.Y);
	Context.Target = Context.Point;
	Context.TargetActor = Projectile.TargetActor;
	Context.bHasColor = Projectile.bHasColor;
	Context.Color = Projectile.Color;
	Context.Blow = FVector2D(Projectile.To - Projectile.From).GetSafeNormal();
	const TCHAR* Suffix = Event.hit ? TEXT(".hit") : TEXT(".fizzle");
	const FName KindId = Projectile.Kind == abyss::ProjectileKind::MonsterBolt
		? FName(TEXT("proj.monster_bolt"))
		: (Projectile.Kind == abyss::ProjectileKind::PetBolt ? FName(TEXT("proj.pet_bolt")) : FName(TEXT("proj.default")));
	const FName Ids[] = { MakeId(TEXT("proj."), Projectile.VfxId, Suffix), MakeId(TEXT(""), KindId, Suffix),
		FName(*(FString(TEXT("proj.default")) + Suffix)) };
	PlayFirst(Ids, Context);
}

// =====================================================================================================================
// Ground effects
// =====================================================================================================================

void UAbyssVfxSystem::HandleGroundStarted(const abyss::EvGroundEffectStarted& Event)
{
	using namespace AbyssVfxSystemPrivate;
	FGroundEffect Effect;
	Effect.Id = Event.id;
	Effect.VfxId = FName(*AbyssText::ToFString(Event.vfxId.empty() ? Event.skillId : Event.vfxId));
	Effect.Center = TileToGround(Event.center);
	Effect.RadiusCm = static_cast<float>(FMath::Max(0.25, Event.radius) * AbyssUnits::TileUU);
	if (const UAbyssGameInstance* GameInstance = UAbyssGameInstance::Get(this))
	{
		if (const abyss::DataStore* Data = GameInstance->GetData())
		{
			if (const abyss::SkillDef* Skill = Data->FindSkill(Event.skillId))
			{
				Effect.bHasElement = true;
				Effect.Element = Skill->damageType;
			}
		}
	}
	FAbyssVfxContext Context;
	Context.Origin = Effect.Center;
	Context.Target = Effect.Center;
	Context.Point = Effect.Center;
	Context.RadiusCm = Effect.RadiusCm;
	Context.bHasElement = Effect.bHasElement;
	Context.Element = Effect.Element;
	const FName Ids[] = { MakeId(TEXT("ground."), Effect.VfxId), FName(TEXT("ground.default")) };
	Effect.Handle = PlayFirst(Ids, Context);
	GroundEffects.Add(Effect.Id, Effect);
}

void UAbyssVfxSystem::HandleGroundTriggered(const abyss::EvGroundEffectTriggered& Event)
{
	using namespace AbyssVfxSystemPrivate;
	const FGroundEffect* Effect = GroundEffects.Find(Event.id);
	if (Effect == nullptr)
	{
		return;
	}
	Stop(Effect->Handle, false);
	FAbyssVfxContext Context;
	Context.Origin = Effect->Center;
	Context.Target = Effect->Center;
	Context.Point = Effect->Center;
	Context.RadiusCm = Effect->RadiusCm;
	Context.bHasElement = Effect->bHasElement;
	Context.Element = Effect->Element;
	const FName Ids[] = { MakeId(TEXT("ground."), Effect->VfxId, TEXT(".trigger")), FName(TEXT("ground.default.trigger")) };
	PlayFirst(Ids, Context);
}

void UAbyssVfxSystem::HandleGroundEnded(const abyss::EvGroundEffectEnded& Event)
{
	using namespace AbyssVfxSystemPrivate;
	FGroundEffect Effect;
	if (!GroundEffects.RemoveAndCopyValue(Event.id, Effect))
	{
		return;
	}
	Stop(Effect.Handle, false);
	FAbyssVfxContext Context;
	Context.Origin = Effect.Center;
	Context.Target = Effect.Center;
	Context.Point = Effect.Center;
	Context.RadiusCm = Effect.RadiusCm;
	Context.bHasElement = Effect.bHasElement;
	Context.Element = Effect.Element;
	const FName Ids[] = { MakeId(TEXT("ground."), Effect.VfxId, TEXT(".end")), FName(TEXT("ground.default.end")) };
	PlayFirst(Ids, Context);
}

// =====================================================================================================================
// Combat and reward events
// =====================================================================================================================

void UAbyssVfxSystem::HandleHit(const abyss::EvHit& Event)
{
	using namespace AbyssVfxSystemPrivate;
	FAbyssVfxContext Context;
	if (!EntityGround(Event.target, Context.Target))
	{
		return;
	}
	Context.TargetActor = FindActor(Event.target);
	Context.Point = Context.Target;
	if (Event.hasFrom)
	{
		Context.Origin = TileToGround(Event.from);
	}
	else if (!EntityGround(Event.source, Context.Origin))
	{
		Context.Origin = Context.Target;
	}
	Context.OriginActor = FindActor(Event.source);
	const FVector2D Delta(Context.Target - Context.Origin);
	Context.Blow = Delta.IsNearlyZero() ? FVector2D(1.0, 0.0) : Delta.GetSafeNormal();
	Context.bHasColor = Event.impactColor != 0;
	Context.Color = AbyssWorldUtil::ColorFromRgb(Event.impactColor);
	Context.bHasElement = true;
	Context.Element = Event.element;
	Context.RingRadiusCm = static_cast<float>(Event.profile.ringRadius) * VfxPxToCm;
	Context.SparkCount = Event.profile.sparks;
	Context.bBig = Event.crit || Event.killed;

	if (Event.dodged)
	{
		if (Event.iframeAvoided)
		{
			Play(FName(TEXT("hit.evade")), Context);
		}
		return;   // MISS: the floating text says it, no burst (combat-feel.md 11.4)
	}
	if (Event.impactBurst)
	{
		Play(FName(TEXT("hit.impact")), Context);
	}
	const bool bHeroBasic = Event.skillId.empty() && Event.source == abyss::kHeroEntityId && !Event.tick
		&& Event.targetFaction == abyss::Faction::Monster;
	if (bHeroBasic)
	{
		Play(FName(TEXT("hit.basic")), Context);
	}
	if (Event.targetFaction == abyss::Faction::Hero && Event.melee && !Event.tick)
	{
		Play(FName(TEXT("hit.claw")), Context);
	}
	// Single-target elemental skill marks (art-inventory-ch1.md 8.5 NS_Hit_SkillGroundMark).
	if (!Event.skillId.empty() && !Event.tick && Event.element != abyss::DamageType::Physical)
	{
		bool bSingleTarget = true;
		if (const UAbyssGameInstance* GameInstance = UAbyssGameInstance::Get(this))
		{
			if (const abyss::DataStore* Data = GameInstance->GetData())
			{
				if (const abyss::SkillDef* Skill = Data->FindSkill(Event.skillId))
				{
					bSingleTarget = !Skill->aoe;
				}
			}
		}
		if (bSingleTarget)
		{
			const TCHAR* Mark = Event.element == abyss::DamageType::Fire ? TEXT("hit.mark.fire")
				: (Event.element == abyss::DamageType::Ice ? TEXT("hit.mark.ice") : TEXT("hit.mark.other"));
			Play(FName(Mark), Context);
		}
	}
	// Kill: death burst + coins at the victim (combat-feel.md 13.1 step 7).
	if (Event.killed && Event.targetFaction == abyss::Faction::Monster)
	{
		FAbyssVfxContext Death = Context;
		Death.bHasColor = true;
		Death.Color = AbyssWorldUtil::ColorFromRgb(0xFF4444);
		Play(FName(TEXT("death.burst")), Death);
		Death.CountScale = 1.f;   // 6 coins
		Play(FName(TEXT("reward.gold")), Death);
	}
}

void UAbyssVfxSystem::HandleDespawned(const abyss::EvEntityDespawned& Event)
{
	// Escort death burst (quests 3.9: #E67E22). Monster kills burst on the killing hit (HandleHit).
	if (Event.reason == abyss::DespawnReason::Died && Event.kind == abyss::EntityKind::Escort)
	{
		FAbyssVfxContext Context;
		if (EntityGround(Event.id, Context.Target))
		{
			Context.TargetActor = FindActor(Event.id);
			Context.Origin = Context.Point = Context.Target;
			Context.bHasColor = true;
			Context.Color = AbyssWorldUtil::ColorFromRgb(0xE67E22);
			Play(FName(TEXT("death.burst")), Context);
		}
	}
	// Status loops of an entity that is gone.
	for (auto It = StatusLoops.CreateIterator(); It; ++It)
	{
		if (static_cast<uint32>(It.Key() >> 8) == Event.id)
		{
			Stop(It.Value(), true);
			It.RemoveCurrent();
		}
	}
}

void UAbyssVfxSystem::HandleHeroDied(const abyss::EvHeroDied& Event)
{
	FAbyssVfxContext Context;
	Context.Target = TileToGround(Event.pos);
	Context.TargetActor = FindActor(abyss::kHeroEntityId);
	if (Context.TargetActor.IsValid())
	{
		Context.Target = Context.TargetActor->GetActorLocation();
	}
	Context.Origin = Context.Point = Context.Target;
	Context.bHasColor = true;
	Context.Color = AbyssWorldUtil::ColorFromRgb(0xCC2222);
	Play(FName(TEXT("death.burst")), Context);
	if (PortalHandle != 0)
	{
		Stop(PortalHandle, true);
		PortalHandle = 0;
	}
}

void UAbyssVfxSystem::HandleHeroRespawned(const abyss::EvHeroRespawned& Event)
{
	for (auto It = StatusLoops.CreateIterator(); It; ++It)
	{
		if (static_cast<uint32>(It.Key() >> 8) == abyss::kHeroEntityId)
		{
			Stop(It.Value(), true);
			It.RemoveCurrent();
		}
	}
}

void UAbyssVfxSystem::HandleLevelUp(const abyss::EvLevelUp& Event)
{
	FAbyssVfxContext Context;
	if (!EntityGround(abyss::kHeroEntityId, Context.Target))
	{
		return;
	}
	Context.TargetActor = FindActor(abyss::kHeroEntityId);
	Context.Origin = Context.Point = Context.Target;
	Play(FName(TEXT("hero.levelup")), Context);
}

void UAbyssVfxSystem::HandleItemPicked(const abyss::EvItemPicked& Event)
{
	FAbyssVfxContext Context;
	if (!EntityGround(abyss::kHeroEntityId, Context.Target))
	{
		return;
	}
	Context.TargetActor = FindActor(abyss::kHeroEntityId);
	Context.Origin = Context.Point = Context.Target;
	Context.CountScale = 8.f / 6.f;   // 8 coins on a pickup (VFXManager)
	Context.bHasColor = true;
	Context.Color = AbyssVfxSystemPrivate::QualityColor(Event.quality);
	Play(FName(TEXT("reward.gold")), Context);
}

void UAbyssVfxSystem::HandlePotionPicked(const abyss::EvPotionPicked& Event)
{
	FAbyssVfxContext Context;
	if (!EntityGround(abyss::kHeroEntityId, Context.Target))
	{
		return;
	}
	Context.TargetActor = FindActor(abyss::kHeroEntityId);
	Context.Origin = Context.Point = Context.Target;
	if (Event.kind == abyss::PotionKind::Mp)
	{
		Context.bHasColor = true;
		Context.Color = AbyssWorldUtil::ColorFromRgb(0x4A8AFF);
		const FName Ids[] = { FName(TEXT("reward.mana")), FName(TEXT("reward.heal")) };
		PlayFirst(Ids, Context);
		return;
	}
	Play(FName(TEXT("reward.heal")), Context);
}

void UAbyssVfxSystem::HandleDodge(const abyss::EvDodgeStarted& Event)
{
	FAbyssVfxContext Context;
	Context.Origin = TileToGround(Event.from);
	Context.Target = Context.Point = TileToGround(Event.to);
	Context.Blow = FVector2D(Context.Target - Context.Origin).GetSafeNormal();
	Play(FName(TEXT("hero.dodge")), Context);
}

void UAbyssVfxSystem::HandleDash(const abyss::EvHeroDash& Event)
{
	if (Event.phase != abyss::EvHeroDash::Phase::Started)
	{
		return;
	}
	FAbyssVfxContext Context;
	Context.Origin = TileToGround(Event.from);
	Context.Target = Context.Point = TileToGround(Event.to);
	Context.Blow = FVector2D(Context.Target - Context.Origin).GetSafeNormal();
	Play(FName(TEXT("hero.dash")), Context);
}

void UAbyssVfxSystem::HandleTeleported(const abyss::EvEntityTeleported& Event)
{
	if (Event.reason != abyss::TeleportReason::EliteBlink)
	{
		return;   // skill teleports play skill.teleport (EvSkillVfx); dodge / portal / respawn have their own
	}
	FAbyssVfxContext Context;
	Context.Origin = TileToGround(Event.from);
	Context.Target = Context.Point = TileToGround(Event.to);
	Context.Blow = FVector2D(Context.Target - Context.Origin).GetSafeNormal();
	Play(FName(TEXT("teleport.blink")), Context);
}

void UAbyssVfxSystem::HandleTownPortal(const abyss::EvTownPortal& Event)
{
	switch (Event.phase)
	{
	case abyss::EvTownPortal::Phase::Started:
	{
		FAbyssVfxContext Context;
		if (!EntityGround(abyss::kHeroEntityId, Context.Origin))
		{
			return;
		}
		Context.OriginActor = FindActor(abyss::kHeroEntityId);
		Context.Target = Context.Point = Context.Origin;
		Stop(PortalHandle, false);
		PortalHandle = Play(FName(TEXT("portal.channel")), Context);
		break;
	}
	case abyss::EvTownPortal::Phase::Cancelled:
		Stop(PortalHandle, true);
		PortalHandle = 0;
		break;
	case abyss::EvTownPortal::Phase::Completed:
	{
		Stop(PortalHandle, false);
		PortalHandle = 0;
		FAbyssVfxContext Context;
		Context.Origin = Context.Target = Context.Point = TileToGround(Event.destination);
		Play(FName(TEXT("portal.arrive")), Context);
		break;
	}
	}
}

void UAbyssVfxSystem::HandleLootDropped(const abyss::EvLootDropped& Event)
{
	if (Event.quality == abyss::ItemQuality::Normal)
	{
		return;
	}
	FAbyssVfxContext Context;
	Context.Origin = Context.Target = Context.Point = TileToGround(Event.pos);
	Context.bHasColor = true;
	Context.Color = AbyssVfxSystemPrivate::QualityColor(Event.quality);
	switch (Event.quality)
	{
	case abyss::ItemQuality::Magic:
		Play(FName(TEXT("loot.drop")), Context);
		break;
	case abyss::ItemQuality::Rare:
	{
		const FName Ids[] = { FName(TEXT("loot.drop.rare")), FName(TEXT("loot.drop")) };
		PlayFirst(Ids, Context);
		break;
	}
	case abyss::ItemQuality::Legendary:
	case abyss::ItemQuality::Set:
	{
		const FName Ids[] = { Event.quality == abyss::ItemQuality::Set ? FName(TEXT("loot.drop.set")) : FName(TEXT("loot.drop.legendary")),
			FName(TEXT("loot.drop.legendary")), FName(TEXT("loot.drop")) };
		PlayFirst(Ids, Context);
		break;
	}
	case abyss::ItemQuality::Normal:
		break;
	}
}

void UAbyssVfxSystem::HandleQuestUpdate(const abyss::EvQuestUpdate& Event)
{
	if (Event.kind != abyss::EvQuestUpdate::Kind::Progress || !Event.hasFrom)
	{
		return;
	}
	FAbyssVfxContext Context;
	Context.Origin = Context.Target = Context.Point = TileToGround(Event.from);
	Context.TargetActor = FindActor(abyss::kHeroEntityId);
	Play(FName(TEXT("quest.pop")), Context);
}

void UAbyssVfxSystem::HandleSkillVfx(const abyss::EvSkillVfx& Event)
{
	using namespace AbyssVfxSystemPrivate;
	FAbyssVfxContext Context;
	Context.OriginActor = FindActor(Event.caster);
	Context.Origin = Context.OriginActor.IsValid() ? Context.OriginActor->GetActorLocation() : TileToGround(Event.origin);
	Context.Point = TileToGround(Event.point);
	Context.TargetActor = FindActor(Event.target);
	Context.Target = Context.TargetActor.IsValid() ? Context.TargetActor->GetActorLocation() : Context.Point;
	for (const abyss::Vec2& Tile : Event.points)
	{
		Context.Points.Add(TileToGround(Tile));
	}
	Context.StaggerMs = static_cast<float>(Event.staggerMs);
	Context.RadiusCm = static_cast<float>(Event.radius * AbyssUnits::TileUU);
	FVector2D Delta(Context.Target - Context.Origin);
	if (Delta.IsNearlyZero() && Context.OriginActor.IsValid())
	{
		const FVector Forward = Context.OriginActor->GetActorForwardVector();
		Delta = FVector2D(Forward.X, Forward.Y);
	}
	Context.Blow = Delta.IsNearlyZero() ? FVector2D(1.0, 0.0) : Delta.GetSafeNormal();
	if (const UAbyssGameInstance* GameInstance = UAbyssGameInstance::Get(this))
	{
		if (const abyss::DataStore* Data = GameInstance->GetData())
		{
			if (const abyss::SkillDef* Skill = Data->FindSkill(Event.skillId))
			{
				Context.bHasElement = true;
				Context.Element = Skill->damageType;
				if (Skill->impactColor != 0)
				{
					Context.bHasColor = true;
					Context.Color = AbyssWorldUtil::ColorFromRgb(Skill->impactColor);
				}
				if (Context.RadiusCm <= 0.f && Skill->aoeRadius > 0.0)
				{
					Context.RadiusCm = static_cast<float>(Skill->aoeRadius * AbyssUnits::TileUU);
				}
			}
		}
	}
	if (Context.RadiusCm <= 0.f)
	{
		Context.RadiusCm = 100.f;
	}
	const FName Ids[] = { MakeId(TEXT("skill."), Event.vfxId), MakeId(TEXT("skill."), Event.skillId), FName(TEXT("skill.default")) };
	PlayFirst(Ids, Context);
}

// =====================================================================================================================
// Status loops (R8)
// =====================================================================================================================

FName UAbyssVfxSystem::StatusRecipeId(abyss::StatusType Type) const
{
	return AbyssVfxSystemPrivate::MakeId(TEXT("status."), abyss::EnumName(Type));
}

void UAbyssVfxSystem::UpdateStatusLoops(const abyss::Snapshot& Snap)
{
	TMap<uint64, bool> Wanted;
	auto Want = [&Wanted](abyss::EntityId Id, uint32 Mask)
	{
		for (uint32 Bit = 0; Bit < static_cast<uint32>(abyss::EnumCount<abyss::StatusType>()); ++Bit)
		{
			if ((Mask & (1u << Bit)) != 0u)
			{
				Wanted.Add((static_cast<uint64>(Id) << 8) | Bit, true);
			}
		}
	};
	if (Snap.hero.life == abyss::HeroLife::Alive)
	{
		Want(abyss::kHeroEntityId, Snap.hero.statusMask);
	}
	for (const abyss::MonsterView& Monster : Snap.monsters)
	{
		if (Monster.alive && Monster.statusMask != 0u)
		{
			Want(Monster.id, Monster.statusMask);
		}
	}
	// stop the loops that are no longer wanted
	for (auto It = StatusLoops.CreateIterator(); It; ++It)
	{
		if (!Wanted.Contains(It.Key()))
		{
			Stop(It.Value(), false);
			It.RemoveCurrent();
		}
	}
	// start the new ones
	for (const TPair<uint64, bool>& Pair : Wanted)
	{
		if (StatusLoops.Contains(Pair.Key))
		{
			continue;
		}
		const abyss::EntityId Id = static_cast<abyss::EntityId>(Pair.Key >> 8);
		const abyss::StatusType Type = static_cast<abyss::StatusType>(Pair.Key & 0xFFu);
		AActor* Actor = FindActor(Id);
		if (Actor == nullptr)
		{
			continue;
		}
		FAbyssVfxContext Context;
		Context.Origin = Context.Target = Context.Point = Actor->GetActorLocation();
		Context.OriginActor = Actor;
		Context.TargetActor = Actor;
		const int32 Handle = Play(StatusRecipeId(Type), Context);
		StatusLoops.Add(Pair.Key, Handle);   // 0 when the recipe does not exist: remembered, not retried every frame
	}
}
