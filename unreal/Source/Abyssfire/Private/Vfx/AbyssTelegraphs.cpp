#include "Vfx/AbyssTelegraphs.h"

#include "Abyssfire.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#include "abyss/base/Math.h"
#include "abyss/data/DataStore.h"
#include "abyss/data/MonsterData.h"
#include "abyss/monsters/Phases.h"

#include "Framework/AbyssGameInstance.h"
#include "Framework/AbyssSimDriver.h"
#include "Framework/AbyssText.h"
#include "Framework/AbyssUnits.h"
#include "Framework/AbyssWorldView.h"
#include "Vfx/AbyssTelegraphRules.h"
#include "Vfx/AbyssVfxSystem.h"
#include "World/AbyssAssetLibrary.h"
#include "World/AbyssThemeRules.h"

static TAutoConsoleVariable<int32> CVarAbyssDebugLeash(
	TEXT("abyss.DebugLeash"),
	0,
	TEXT("Dark remake 2G debug overlay: 1 draws the M1 leash (MonsterView leashCentre / leashRange: past it a monster walks ")
	TEXT("home, a phase-table boss resets) of every living monster within 24 tiles of the hero as a rim-only telegraph ring ")
	TEXT("(last in the decal budget)."),
	ECVF_Default);

namespace AbyssTelegraphsPrivate
{
	using namespace AbyssTelegraphRules;

	constexpr double LeashOverlayRangeTiles = 24.0;
	/** A raise marker is forgotten this long after its contact (its recipe ends by itself). */
	constexpr double MarkerForgetAfterMs = 1500.0;

	enum class EDecalKind : uint8
	{
		Strike,   // an ability's ground strike
		Fuse,     // an on-death effect's fuse
		Hazard,   // a lingering ground hazard
		Arena,    // a boss phase's closing ring
		Leash,    // the abyss.DebugLeash overlay
	};

	FName ToName(std::string_view Id)
	{
		return Id.empty() ? FName(NAME_None) : FName(*AbyssText::ToFString(Id));
	}
}

/** Everything the subsystem tracks (std containers: decals carry std::string ids; no UObject references here). */
struct FAbyssTelegraphState
{
	struct FDecal
	{
		AbyssTelegraphsPrivate::EDecalKind Kind = AbyssTelegraphsPrivate::EDecalKind::Strike;
		uint32 Serial = 0;
		abyss::EntityId Entity = abyss::kNoEntity;
		std::string AbilityId;
		uint32 HazardId = 0;
		abyss::AbilityShapeDef Shape;
		abyss::Vec2 Origin;
		abyss::Vec2 Dir{ 1, 0 };
		AbyssTelegraphRules::EStyle Style = AbyssTelegraphRules::EStyle::Normal;
		AbyssTelegraphRules::FGeometry Geo;
		double GroundZ = 0.0;
		double StartMs = 0.0;
		double ContactMs = 0.0;   // strikes / fuses
		double EndMs = -1.0;      // zones: untilMs / the clear (< 0 open)
		double CancelMs = -1.0;   // a cancelled windup
		bool bContactPlayed = false;
		int32 RecipeHandle = 0;   // a hazard's / an arena's loop
		// arena: the ring's timeline (abyss::ArenaRadiusAt)
		double FromRadius = 0.0;
		double TargetRadius = 0.0;
		double RingStartMs = 0.0;
		double RingEndMs = 0.0;
		// drawing
		int32 Component = INDEX_NONE;
		std::array<float, AbyssTelegraphRules::Cpd::Count> LastCpd{};
		bool bCpdValid = false;
		bool bTransformValid = false;
		bool bSeen = false;
	};

	/**
	 * A telegraph drawn without a decal: a raise / summon marker (its recipe handle), or one with no look at all (bSilent:
	 * Gravecold's slow ring), remembered so the snapshot's view of it is not taken for an unannounced one.
	 */
	struct FMarker
	{
		abyss::EntityId Entity = abyss::kNoEntity;
		std::string AbilityId;
		abyss::Vec2 Origin;
		double StartMs = 0.0;
		double ContactMs = 0.0;
		int32 Handle = 0;
		bool bSilent = false;
	};

	AbyssTelegraphRules::FContract Contract = AbyssTelegraphRules::DefaultContract();
	bool bContractLoaded = false;
	std::vector<FDecal> Decals;
	std::vector<FMarker> Markers;
	std::unordered_map<uint32, std::string> DefIds;
	uint32 NextSerial = 1;

	// Per-frame scratch of Tick (assigned / cleared each frame, never shrunk: no allocation once a fight's peak is
	// reached; 2G review m3).
	std::vector<AbyssTelegraphRules::FState> FrameStates;
	std::vector<bool> FrameRemove;
	std::vector<AbyssTelegraphRules::FBudgetEntry> FrameEntries;
	std::vector<size_t> FrameEntryDecal;
	std::vector<bool> FrameDraw;
	std::vector<size_t> FrameOrder;
	std::vector<bool> FramePicked;

	FDecal* FindStrike(abyss::EntityId Entity, std::string_view AbilityId, double StartMs, abyss::Vec2 Origin)
	{
		for (FDecal& D : Decals)
		{
			if ((D.Kind == AbyssTelegraphsPrivate::EDecalKind::Strike || D.Kind == AbyssTelegraphsPrivate::EDecalKind::Fuse)
				&& AbyssTelegraphRules::SameTelegraph(D.Entity, D.AbilityId, D.StartMs, D.Origin, Entity, AbilityId, StartMs, Origin))
			{
				return &D;
			}
		}
		return nullptr;
	}

	FMarker* FindMarker(abyss::EntityId Entity, std::string_view AbilityId, double StartMs, abyss::Vec2 Origin)
	{
		for (FMarker& M : Markers)
		{
			if (AbyssTelegraphRules::SameTelegraph(M.Entity, M.AbilityId, M.StartMs, M.Origin, Entity, AbilityId, StartMs, Origin))
			{
				return &M;
			}
		}
		return nullptr;
	}
};

// =====================================================================================================================
// Lifecycle
// =====================================================================================================================

UAbyssTelegraphSystem* UAbyssTelegraphSystem::Get(const UObject* WorldContextObject)
{
	const UWorld* OwningWorld = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return OwningWorld ? OwningWorld->GetSubsystem<UAbyssTelegraphSystem>() : nullptr;
}

bool UAbyssTelegraphSystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UAbyssTelegraphSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UAbyssVfxSystem>();
	State = MakeShared<FAbyssTelegraphState>();
	BindEvents();
}

void UAbyssTelegraphSystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	BindEvents();
}

void UAbyssTelegraphSystem::Deinitialize()
{
	UnbindEvents();
	ClearAll();
	if (DecalActor != nullptr)
	{
		DecalActor->Destroy();
		DecalActor = nullptr;
	}
	Pool.Reset();
	FreePool.Reset();
	State.Reset();
	Super::Deinitialize();
}

void UAbyssTelegraphSystem::BindEvents()
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
	Router.On<abyss::EvTelegraph>().AddUObject(this, &UAbyssTelegraphSystem::HandleTelegraph);
	Router.On<abyss::EvHazard>().AddUObject(this, &UAbyssTelegraphSystem::HandleHazard);
	Router.On<abyss::EvArena>().AddUObject(this, &UAbyssTelegraphSystem::HandleArena);
	Router.On<abyss::EvMonsterAttackCancelled>().AddUObject(this, &UAbyssTelegraphSystem::HandleAttackCancelled);
	Router.On<abyss::EvEntitySpawned>().AddUObject(this, &UAbyssTelegraphSystem::HandleEntitySpawned);
	Router.OnSessionEnded.AddUObject(this, &UAbyssTelegraphSystem::ClearAll);
	bEventsBound = true;
}

void UAbyssTelegraphSystem::UnbindEvents()
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

void UAbyssTelegraphSystem::ApplyQuality(const FAbyssQualityProfile& Profile)
{
	DecalCap = AbyssTelegraphRules::DecalCap(Profile.Tier, Profile.bMobile);
	bOuterWash = AbyssTelegraphRules::ArenaOuterWash(Profile.Tier, Profile.bMobile);
}

void UAbyssTelegraphSystem::ClearAll()
{
	// The VFX system clears its own emitters with the zone; the loop handles held here are simply forgotten.
	if (State.IsValid())
	{
		State->Decals.clear();
		State->Markers.clear();
		State->DefIds.clear();
	}
	FreePool.Reset();
	for (int32 Index = 0; Index < Pool.Num(); ++Index)
	{
		if (UStaticMeshComponent* Component = Pool[Index].Get())
		{
			Component->SetVisibility(false);
		}
		FreePool.Add(Index);
	}
	DrawnDecals = 0;
}

// =====================================================================================================================
// Resources
// =====================================================================================================================

bool UAbyssTelegraphSystem::EnsureResources()
{
	if (bResourcesResolved)
	{
		return bDecalsAvailable;
	}
	UWorld* OwningWorld = GetWorld();
	UAbyssAssetLibrary* Assets = UAbyssAssetLibrary::Get(this);
	if (OwningWorld == nullptr || Assets == nullptr || !OwningWorld->HasBegunPlay())
	{
		return false;   // try again later
	}
	bResourcesResolved = true;
	QuadMesh = Assets->LoadContentMesh(TEXT("FX"), TEXT("SM_FX_Quad"));
	DecalMaterial = Assets->LoadMaterial(TEXT("M_AF_Telegraph"));
	if (QuadMesh == nullptr || DecalMaterial == nullptr)
	{
		UE_LOG(LogAbyss, Warning, TEXT("Telegraph decals disabled: %s missing (run Scripts/build_content.py); the recipes and ")
			TEXT("the wind-up tints still play"), QuadMesh == nullptr ? TEXT("SM_FX_Quad") : TEXT("M_AF_Telegraph"));
		bDecalsAvailable = false;
		return false;
	}
	FActorSpawnParameters Params;
	Params.Name = MakeUniqueObjectName(OwningWorld->PersistentLevel.Get(), AActor::StaticClass(), FName(TEXT("AbyssTelegraphs")));
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	DecalActor = OwningWorld->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (DecalActor == nullptr)
	{
		bDecalsAvailable = false;
		return false;
	}
	USceneComponent* Root = NewObject<USceneComponent>(DecalActor, TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	DecalActor->SetRootComponent(Root);
	Root->RegisterComponent();
	bDecalsAvailable = true;
	return true;
}

void UAbyssTelegraphSystem::EnsureContract()
{
	if (!State.IsValid() || State->bContractLoaded)
	{
		return;
	}
	const UAbyssAssetLibrary* Assets = UAbyssAssetLibrary::Get(this);
	if (Assets == nullptr || !Assets->HasManifest())
	{
		return;   // the built-in copy until the manifest is there
	}
	State->Contract = AbyssTelegraphRules::ParseContract(Assets->GetManifest().GetMaterialsJson());
	State->bContractLoaded = true;
	UE_CLOG(!State->Contract.bFromManifest, LogAbyss, Warning,
		TEXT("Telegraphs: the art manifest has no materials.M_AF_Telegraph; using the built-in contract"));
}

int32 UAbyssTelegraphSystem::AcquireComponent()
{
	if (FreePool.Num() > 0)
	{
		return FreePool.Pop(EAllowShrinking::No);
	}
	if (DecalActor == nullptr || QuadMesh == nullptr || DecalMaterial == nullptr)
	{
		return INDEX_NONE;
	}
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(DecalActor, NAME_None, RF_Transient);
	Component->SetStaticMesh(QuadMesh);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);
	Component->SetCastShadow(false);
	Component->SetReceivesDecals(false);
	Component->SetMaterial(0, DecalMaterial);
	// Over blob shadows (-10), prop pools (-5) and water (-2); under the VFX sprites (1 / 2).
	Component->SetTranslucentSortPriority(AbyssTelegraphRules::SortPriority);
	Component->SetupAttachment(DecalActor->GetRootComponent());
	Component->RegisterComponent();
	Component->SetVisibility(false);
	return Pool.Add(Component);
}

void UAbyssTelegraphSystem::ReleaseComponent(int32 Index)
{
	if (!Pool.IsValidIndex(Index))
	{
		return;
	}
	if (UStaticMeshComponent* Component = Pool[Index].Get())
	{
		Component->SetVisibility(false);
	}
	FreePool.Add(Index);
}

double UAbyssTelegraphSystem::SimNowMs() const
{
	const UAbyssSimDriver* Driver = UAbyssSimDriver::Get(this);
	const abyss::Snapshot* Snap = Driver != nullptr ? Driver->GetSnapshotIndex().GetSnapshot() : nullptr;
	return Snap != nullptr ? Snap->simNowMs : 0.0;
}

double UAbyssTelegraphSystem::GroundZ(double X, double Y) const
{
	const UAbyssSimDriver* Driver = UAbyssSimDriver::Get(this);
	const IAbyssWorldView* View = Driver != nullptr ? Driver->GetWorldView() : nullptr;
	return View != nullptr ? View->GetGroundHeight(X, Y) : 0.0;
}

// =====================================================================================================================
// Events
// =====================================================================================================================

void UAbyssTelegraphSystem::HandleEntitySpawned(const abyss::EvEntitySpawned& Event)
{
	if (State.IsValid() && Event.kind == abyss::EntityKind::Monster)
	{
		State->DefIds[Event.id] = Event.defId;
	}
}

void UAbyssTelegraphSystem::HandleTelegraph(const abyss::EvTelegraph& Event)
{
	using namespace AbyssTelegraphsPrivate;
	if (!State.IsValid())
	{
		return;
	}
	EnsureContract();
	FAbyssTelegraphState& S = *State;
	if (S.FindStrike(Event.entity, Event.abilityId, Event.startMs, Event.origin) != nullptr
		|| S.FindMarker(Event.entity, Event.abilityId, Event.startMs, Event.origin) != nullptr)
	{
		return;   // already drawn from the snapshot (a late view)
	}
	UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this);
	auto GroundAt = [this](abyss::Vec2 Tile)
	{
		FVector World = AbyssUnits::TileToWorld(Tile);
		World.Z = GroundZ(World.X, World.Y);
		return World;
	};
	const FVector OriginWorld = GroundAt(Event.origin);
	const FVector2D Blow = FVector2D(Event.dir.x, Event.dir.y).GetSafeNormal();

	bool bDamages = true;
	bool bThemedZone = false;
	if (Event.source == abyss::TelegraphSource::OnDeath)
	{
		const UWorld* OwningWorld = GetWorld();
		const UAbyssGameInstance* GameInstance = OwningWorld ? Cast<UAbyssGameInstance>(OwningWorld->GetGameInstance()) : nullptr;
		const abyss::DataStore* Data = GameInstance != nullptr && GameInstance->IsDataReady() ? GameInstance->GetData() : nullptr;
		// Chapters 2-5 and the Labyrinth (no presentation theme) keep today's look: their global elite fuses and the
		// volatile curse go off unpresented (PresentsOnDeath; TelegraphLook then remembers a silent marker).
		const UAbyssSimDriver* Driver = UAbyssSimDriver::Get(this);
		const abyss::Snapshot* Snap = Driver != nullptr ? Driver->GetSnapshotIndex().GetSnapshot() : nullptr;
		bThemedZone = Data != nullptr && Snap != nullptr && !AbyssThemeRules::ThemeOf(Data->FindMap(Snap->zone.mapId)).empty();
		// The fuse's own look from the kill (Scorched's smoulder, Gravecold's rime: timed in the recipe to the data's delay).
		const std::string FuseRecipe = FuseStartRecipe(Event.abilityId, bThemedZone);
		if (Vfx != nullptr && !FuseRecipe.empty())
		{
			FAbyssVfxContext Context;
			Context.Origin = Context.Target = Context.Point = OriginWorld;
			Context.Blow = Blow.IsNearlyZero() ? FVector2D(1.0, 0.0) : Blow;
			Context.RadiusCm = static_cast<float>(RecipeRadiusCm(Event.shape));
			const FName Ids[] = { ToName(FuseRecipe) };
			Vfx->PlayFirst(Ids, Context);
		}
		const abyss::MonsterDef* Def = nullptr;
		if (Data != nullptr && bThemedZone)
		{
			const auto Found = S.DefIds.find(Event.entity);
			if (Found != S.DefIds.end())
			{
				Def = Data->FindMonster(Found->second);
			}
			else if (Driver != nullptr)
			{
				if (const abyss::MonsterView* View = Driver->GetSnapshotIndex().FindMonster(Event.entity))
				{
					Def = Data->FindMonster(View->defId);
				}
			}
		}
		bDamages = OnDeathDamages(Event.abilityId, Def, Data != nullptr ? &Data->Combat().eliteAffixes : nullptr);
	}

	switch (TelegraphLook(Event.kind, Event.source, bDamages, bThemedZone))
	{
	case AbyssTelegraphRules::ELook::None:
	{
		FAbyssTelegraphState::FMarker& M = S.Markers.emplace_back();
		M.Entity = Event.entity;
		M.AbilityId = Event.abilityId;
		M.Origin = Event.origin;
		M.StartMs = Event.startMs;
		M.ContactMs = Event.contactMs;
		M.bSilent = true;
		return;
	}
	case AbyssTelegraphRules::ELook::RaiseMarker:
	{
		FAbyssTelegraphState::FMarker& M = S.Markers.emplace_back();
		M.Entity = Event.entity;
		M.AbilityId = Event.abilityId;
		M.Origin = Event.origin;
		M.StartMs = Event.startMs;
		M.ContactMs = Event.contactMs;
		if (Vfx != nullptr)
		{
			FAbyssVfxContext Context;
			Context.Origin = Context.Target = Context.Point = OriginWorld;
			Context.Blow = Blow.IsNearlyZero() ? FVector2D(1.0, 0.0) : Blow;
			Context.RadiusCm = static_cast<float>(RecipeRadiusCm(Event.shape));
			M.Handle = Vfx->Play(ToName(RaiseMarkerRecipe), Context);
		}
		return;
	}
	case AbyssTelegraphRules::ELook::Decal:
		break;
	}
	FAbyssTelegraphState::FDecal& D = S.Decals.emplace_back();
	D.Kind = Event.source == abyss::TelegraphSource::OnDeath ? EDecalKind::Fuse : EDecalKind::Strike;
	D.Serial = S.NextSerial++;
	D.Entity = Event.entity;
	D.AbilityId = Event.abilityId;
	D.Shape = Event.shape;
	D.Origin = Event.origin;
	D.Dir = Event.dir;
	D.Style = StyleOf(Event.style);
	D.Geo = MakeGeometry(D.Shape, D.Origin, D.Dir, OutsideReachCm(S.Contract.Style(D.Style), bOuterWash));
	D.GroundZ = OriginWorld.Z;
	D.StartMs = Event.startMs;
	D.ContactMs = Event.contactMs;
}

void UAbyssTelegraphSystem::HandleHazard(const abyss::EvHazard& Event)
{
	using namespace AbyssTelegraphsPrivate;
	if (!State.IsValid())
	{
		return;
	}
	EnsureContract();
	FAbyssTelegraphState& S = *State;
	for (const FAbyssTelegraphState::FDecal& Existing : S.Decals)
	{
		if (Existing.Kind == EDecalKind::Hazard && Existing.HazardId == Event.id)
		{
			return;
		}
	}
	FAbyssTelegraphState::FDecal& D = S.Decals.emplace_back();
	D.Kind = EDecalKind::Hazard;
	D.Serial = S.NextSerial++;
	D.Entity = Event.source;
	D.AbilityId = Event.abilityId;
	D.HazardId = Event.id;
	D.Shape = Event.shape;
	D.Origin = Event.origin;
	D.Dir = Event.dir;
	D.Style = AbyssTelegraphRules::EStyle::Hazard;
	D.Geo = MakeGeometry(D.Shape, D.Origin, D.Dir, OutsideReachCm(S.Contract.Style(D.Style), bOuterWash));
	FVector OriginWorld = AbyssUnits::TileToWorld(Event.origin);
	OriginWorld.Z = GroundZ(OriginWorld.X, OriginWorld.Y);
	D.GroundZ = OriginWorld.Z;
	D.StartMs = Event.startMs;
	D.EndMs = Event.untilMs;
	if (UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this))
	{
		FAbyssVfxContext Context;
		Context.Origin = Context.Point = OriginWorld;
		const FVector2D Blow = FVector2D(Event.dir.x, Event.dir.y).GetSafeNormal();
		Context.Blow = Blow.IsNearlyZero() ? FVector2D(1.0, 0.0) : Blow;
		Context.RadiusCm = static_cast<float>(RecipeRadiusCm(Event.shape));
		if (Event.shape.kind == abyss::AbilityShapeKind::Line)
		{
			FVector End = AbyssUnits::TileToWorld(Event.origin + D.Dir.Normalized() * Event.shape.length);
			End.Z = GroundZ(End.X, End.Y);
			Context.Target = End;
		}
		else
		{
			Context.Target = OriginWorld;
		}
		TArray<FName, TInlineAllocator<2>> Ids;
		for (const std::string& Id : HazardRecipeChain(Event.abilityId, Event.shape.kind))
		{
			Ids.Add(ToName(Id));
		}
		D.RecipeHandle = Vfx->PlayFirst(Ids, Context);
	}
}

void UAbyssTelegraphSystem::HandleArena(const abyss::EvArena& Event)
{
	using namespace AbyssTelegraphsPrivate;
	if (!State.IsValid())
	{
		return;
	}
	EnsureContract();
	FAbyssTelegraphState& S = *State;
	UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this);
	const double Now = SimNowMs();
	// A clear, or a newer arena of the same monster: the open one fades out and its ring of fire stops.
	for (FAbyssTelegraphState::FDecal& D : S.Decals)
	{
		if (D.Kind == EDecalKind::Arena && D.Entity == Event.entity && D.EndMs < 0.0)
		{
			D.EndMs = Now;
			if (Vfx != nullptr && D.RecipeHandle != 0)
			{
				Vfx->Stop(D.RecipeHandle);
			}
			D.RecipeHandle = 0;
		}
	}
	if (Event.cleared)
	{
		return;
	}
	FAbyssTelegraphState::FDecal& D = S.Decals.emplace_back();
	D.Kind = EDecalKind::Arena;
	D.Serial = S.NextSerial++;
	D.Entity = Event.entity;
	D.Shape.kind = abyss::AbilityShapeKind::Circle;
	D.FromRadius = Event.radius;
	D.TargetRadius = Event.targetRadius;
	D.RingStartMs = Event.startMs;
	D.RingEndMs = Event.endMs;
	D.Shape.radius = abyss::ArenaRadiusAt(D.FromRadius, D.TargetRadius, D.RingEndMs - D.RingStartMs, Now - D.RingStartMs);
	D.Origin = Event.centre;
	D.Style = AbyssTelegraphRules::EStyle::Arena;
	D.Geo = MakeGeometry(D.Shape, D.Origin, D.Dir, OutsideReachCm(S.Contract.Style(D.Style), bOuterWash));
	FVector Centre = AbyssUnits::TileToWorld(Event.centre);
	Centre.Z = GroundZ(Centre.X, Centre.Y);
	D.GroundZ = Centre.Z;
	D.StartMs = Event.startMs;
	if (Vfx != nullptr)
	{
		FAbyssVfxContext Context;
		Context.Origin = Context.Target = Context.Point = Centre;
		Context.RadiusCm = static_cast<float>(D.Shape.radius * AbyssUnits::TileUU);
		D.RecipeHandle = Vfx->Play(ToName(ArenaRecipe), Context);
	}
}

void UAbyssTelegraphSystem::HandleAttackCancelled(const abyss::EvMonsterAttackCancelled& Event)
{
	using namespace AbyssTelegraphsPrivate;
	if (!State.IsValid())
	{
		return;
	}
	// Dispatched after this frame's steps: the abort happened within them, possibly in the contact timer itself (a stun
	// in the contact step, CombatSystem::ResolveAbility) at a sim time >= the contact (CancelTimeFor).
	const UAbyssSimDriver* Driver = UAbyssSimDriver::Get(this);
	const double FrameEnd = SimNowMs();
	const int32 Steps = Driver != nullptr ? Driver->GetFrameInfo().StepsThisFrame : 1;
	for (FAbyssTelegraphState::FDecal& D : State->Decals)
	{
		// An ability's windup only (an on-death fuse always goes off).
		if (D.Kind == EDecalKind::Strike && D.Entity == Event.monster && D.CancelMs < 0.0)
		{
			const double CancelMs = CancelTimeFor(D.ContactMs, D.bContactPlayed, FrameEnd, Steps);
			if (CancelMs >= 0.0)
			{
				D.CancelMs = CancelMs;
			}
		}
	}
	UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this);
	for (int32 Index = static_cast<int32>(State->Markers.size()) - 1; Index >= 0; --Index)
	{
		FAbyssTelegraphState::FMarker& M = State->Markers[static_cast<size_t>(Index)];
		if (M.Entity == Event.monster && !M.bSilent && CancelTimeFor(M.ContactMs, false, FrameEnd, Steps) >= 0.0)
		{
			if (Vfx != nullptr && M.Handle != 0)
			{
				Vfx->Stop(M.Handle, /*bKillParticles*/ true);   // the raise did not happen: the marker goes at once
			}
			State->Markers.erase(State->Markers.begin() + Index);
		}
	}
}

// =====================================================================================================================
// Per frame
// =====================================================================================================================

void UAbyssTelegraphSystem::Tick(const FAbyssFrameInfo& Frame, const abyss::Snapshot& Snap)
{
	using namespace AbyssTelegraphsPrivate;
	if (!State.IsValid())
	{
		return;
	}
	if (!bEventsBound)
	{
		BindEvents();
	}
	EnsureContract();
	FAbyssTelegraphState& S = *State;
	const AbyssTelegraphRules::FContract& C = S.Contract;
	const double Now = Frame.RenderSimMs;
	const double SimNow = Snap.simNowMs;
	UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this);

	// ---- late views: what the snapshot holds that no event announced (a load, a zone build), and what it dropped ----
	for (FAbyssTelegraphState::FDecal& D : S.Decals)
	{
		D.bSeen = false;
	}
	for (const abyss::TelegraphView& V : Snap.telegraphs)
	{
		FAbyssTelegraphState::FDecal* Known = S.FindStrike(V.entity, V.abilityId, V.startMs, V.origin);
		if (Known != nullptr)
		{
			Known->bSeen = true;
			continue;
		}
		if (S.FindMarker(V.entity, V.abilityId, V.startMs, V.origin) != nullptr
			|| !LateViewWorthDrawing(Now, V.contactMs))
		{
			continue;
		}
		abyss::EvTelegraph Late;
		Late.entity = V.entity;
		Late.abilityId = V.abilityId;
		Late.kind = V.kind;
		Late.shape = V.shape;
		Late.origin = V.origin;
		Late.dir = V.dir;
		Late.startMs = V.startMs;
		Late.contactMs = V.contactMs;
		Late.style = V.style;
		Late.source = V.source;
		Late.hasDash = V.hasDash;
		Late.dashTo = V.dashTo;
		const size_t Before = S.Decals.size();
		HandleTelegraph(Late);
		if (S.Decals.size() > Before)
		{
			S.Decals.back().bSeen = true;
		}
	}
	for (FAbyssTelegraphState::FDecal& D : S.Decals)
	{
		if (D.Kind == EDecalKind::Strike && !D.bSeen && D.CancelMs < 0.0 && MissingMeansCancelled(SimNow, D.ContactMs))
		{
			D.CancelMs = SimNow;   // gone from the core without its contact (a cancel whose event this view never saw)
		}
	}
	for (const abyss::HazardView& V : Snap.hazards)
	{
		bool bKnown = false;
		for (const FAbyssTelegraphState::FDecal& D : S.Decals)
		{
			bKnown = bKnown || (D.Kind == EDecalKind::Hazard && D.HazardId == V.id);
		}
		if (!bKnown && LateViewWorthDrawing(Now, V.untilMs))
		{
			abyss::EvHazard Late;
			Late.id = V.id;
			Late.source = V.source;
			Late.abilityId = V.abilityId;
			Late.shape = V.shape;
			Late.origin = V.origin;
			Late.dir = V.dir;
			Late.damageType = V.damageType;
			Late.startMs = V.startMs;
			Late.untilMs = V.untilMs;
			HandleHazard(Late);
		}
	}
	for (const abyss::ArenaView& V : Snap.arenas)
	{
		bool bKnown = false;
		for (const FAbyssTelegraphState::FDecal& D : S.Decals)
		{
			bKnown = bKnown || (D.Kind == EDecalKind::Arena && D.Entity == V.entity && D.EndMs < 0.0);
		}
		if (!bKnown)
		{
			abyss::EvArena Late;
			Late.entity = V.entity;
			Late.kind = V.kind;
			Late.centre = V.centre;
			Late.radius = V.fromRadius;
			Late.targetRadius = V.targetRadius;
			Late.startMs = V.startMs;
			Late.endMs = V.endMs;
			HandleArena(Late);
		}
	}
	for (FAbyssTelegraphState::FDecal& D : S.Decals)
	{
		if (D.Kind != EDecalKind::Arena || D.EndMs >= 0.0)
		{
			continue;
		}
		bool bListed = false;
		for (const abyss::ArenaView& V : Snap.arenas)
		{
			bListed = bListed || V.entity == D.Entity;
		}
		if (!bListed)
		{
			D.EndMs = SimNow;   // the core dropped it without a clear this view saw
			if (Vfx != nullptr && D.RecipeHandle != 0)
			{
				Vfx->Stop(D.RecipeHandle);
			}
			D.RecipeHandle = 0;
		}
	}

	// ---- the debug leash overlay (abyss.DebugLeash) ----
	const bool bLeash = CVarAbyssDebugLeash.GetValueOnGameThread() != 0;
	for (FAbyssTelegraphState::FDecal& D : S.Decals)
	{
		if (D.Kind == EDecalKind::Leash)
		{
			D.bSeen = false;
		}
	}
	if (bLeash)
	{
		for (const abyss::MonsterView& M : Snap.monsters)
		{
			if (!M.alive || !(M.leashRange > 0.0)
				|| abyss::DistSq(M.pos, Snap.hero.pos) > LeashOverlayRangeTiles * LeashOverlayRangeTiles)
			{
				continue;
			}
			FAbyssTelegraphState::FDecal* Ring = nullptr;
			for (FAbyssTelegraphState::FDecal& D : S.Decals)
			{
				if (D.Kind == EDecalKind::Leash && D.Entity == M.id)
				{
					Ring = &D;
				}
			}
			if (Ring == nullptr)
			{
				Ring = &S.Decals.emplace_back();
				Ring->Kind = EDecalKind::Leash;
				Ring->Serial = S.NextSerial++;
				Ring->Entity = M.id;
				Ring->Style = AbyssTelegraphRules::EStyle::Hazard;
				Ring->Shape.kind = abyss::AbilityShapeKind::Circle;
				Ring->StartMs = Now;
			}
			Ring->bSeen = true;
			if (Ring->Shape.radius != M.leashRange || Ring->Origin != M.leashCentre)
			{
				Ring->Shape.radius = M.leashRange;
				Ring->Origin = M.leashCentre;
				Ring->Geo = MakeGeometry(Ring->Shape, Ring->Origin, Ring->Dir, OutsideReachCm(C.Style(Ring->Style), bOuterWash));
				const FVector Centre = AbyssUnits::TileToWorld(M.leashCentre);
				Ring->GroundZ = GroundZ(Centre.X, Centre.Y);
				Ring->bTransformValid = false;
			}
		}
	}

	// ---- timelines, contact recipes, loops; what draws this frame ----
	std::vector<AbyssTelegraphRules::FState>& States = S.FrameStates;
	std::vector<bool>& Remove = S.FrameRemove;
	States.assign(S.Decals.size(), AbyssTelegraphRules::FState());
	Remove.assign(S.Decals.size(), false);
	for (size_t Index = 0; Index < S.Decals.size(); ++Index)
	{
		FAbyssTelegraphState::FDecal& D = S.Decals[Index];
		AbyssTelegraphRules::FState& St = States[Index];
		switch (D.Kind)
		{
		case EDecalKind::Strike:
		case EDecalKind::Fuse:
		{
			St = StrikeState(C, D.Style, Now, D.StartMs, D.ContactMs, D.CancelMs);
			if (!D.bContactPlayed && D.CancelMs < 0.0 && Now >= D.ContactMs)
			{
				D.bContactPlayed = true;
				if (Vfx != nullptr && Now <= D.ContactMs + C.FlashMs + C.FadeMs)
				{
					// The contact beat: the rim snap and the dust (circle / ring at the radius; cone along the blow; line
					// along its length).
					const FVector2D Blow = FVector2D(D.Geo.DirX, D.Geo.DirY);
					FVector Origin = AbyssUnits::TileToWorld(D.Origin);
					Origin.Z = D.GroundZ;
					FAbyssVfxContext Context;
					Context.Origin = Context.Point = Origin;
					Context.Target = Origin;
					Context.Blow = Blow;
					Context.RadiusCm = static_cast<float>(RecipeRadiusCm(D.Shape));
					if (D.Shape.kind == abyss::AbilityShapeKind::Line)
					{
						FVector End = AbyssUnits::TileToWorld(D.Origin + abyss::Vec2(D.Geo.DirX, D.Geo.DirY) * D.Shape.length);
						End.Z = GroundZ(End.X, End.Y);
						Context.Target = End;
					}
					Vfx->Play(ToName(ContactRecipe(D.Shape.kind)), Context);
				}
			}
			break;
		}
		case EDecalKind::Hazard:
			St = ZoneState(C, D.Style, Now, D.StartMs, D.EndMs);
			if (D.RecipeHandle != 0 && Now >= D.EndMs)
			{
				if (Vfx != nullptr)
				{
					Vfx->Stop(D.RecipeHandle);   // the breach fire dies down with its last tick
				}
				D.RecipeHandle = 0;
			}
			break;
		case EDecalKind::Arena:
		{
			St = ZoneState(C, D.Style, Now, D.StartMs, D.EndMs);
			const double Radius = abyss::ArenaRadiusAt(D.FromRadius, D.TargetRadius, D.RingEndMs - D.RingStartMs,
				Now - D.RingStartMs);
			const double Margin = OutsideReachCm(C.Style(D.Style), bOuterWash);
			if (std::fabs(Radius - D.Shape.radius) * AbyssUnits::TileUU > 0.5
				|| std::fabs(D.Geo.SizeXCm - 2.0 * (Radius * AbyssUnits::TileUU + Margin)) > 0.5)
			{
				D.Shape.radius = Radius;
				D.Geo = MakeGeometry(D.Shape, D.Origin, D.Dir, Margin);
				D.bTransformValid = false;
				if (Vfx != nullptr && D.RecipeHandle != 0)
				{
					Vfx->SetHandleRadius(D.RecipeHandle, static_cast<float>(Radius * AbyssUnits::TileUU));
				}
			}
			break;
		}
		case EDecalKind::Leash:
			if (D.bSeen)
			{
				St.Phase = AbyssTelegraphRules::EPhase::Zone;
				St.bVisible = true;
				St.Progress = 1.0;
				St.RimAlpha = 0.6;
			}
			else
			{
				St.Phase = AbyssTelegraphRules::EPhase::Done;
			}
			break;
		}
		Remove[Index] = St.Phase == AbyssTelegraphRules::EPhase::Done;
	}

	// ---- the budget ----
	std::vector<AbyssTelegraphRules::FBudgetEntry>& Entries = S.FrameEntries;
	std::vector<size_t>& EntryDecal = S.FrameEntryDecal;
	Entries.clear();
	EntryDecal.clear();
	for (size_t Index = 0; Index < S.Decals.size(); ++Index)
	{
		if (Remove[Index] || !States[Index].bVisible)
		{
			continue;
		}
		const FAbyssTelegraphState::FDecal& D = S.Decals[Index];
		AbyssTelegraphRules::FBudgetEntry E;
		E.Rank = D.Kind == EDecalKind::Leash ? 4 : RankOf(D.Style);
		const abyss::Vec2 Centre(D.Geo.CentreXCm / AbyssUnits::TileUU, D.Geo.CentreYCm / AbyssUnits::TileUU);
		E.DistSq = abyss::DistSq(Centre, Snap.hero.pos);
		E.StartMs = D.StartMs;
		E.Serial = D.Serial;
		Entries.push_back(E);
		EntryDecal.push_back(Index);
	}
	std::vector<bool>& Draw = S.FrameDraw;
	Draw.assign(S.Decals.size(), false);
	if (EnsureResources())
	{
		if (Pool.Num() == 0)
		{
			// One hidden quad registered up front: its registration precaches M_AF_Telegraph's pipeline states, so the
			// first telegraph of a session does not hitch on a shader compile.
			ReleaseComponent(AcquireComponent());
		}
		std::vector<bool>& Picked = S.FramePicked;
		SelectVisible(Entries, DecalCap, S.FrameOrder, Picked);
		for (size_t Entry = 0; Entry < Picked.size(); ++Entry)
		{
			Draw[EntryDecal[Entry]] = Picked[Entry];
		}
	}

	// ---- draw ----
	DrawnDecals = 0;
	for (size_t Index = 0; Index < S.Decals.size(); ++Index)
	{
		FAbyssTelegraphState::FDecal& D = S.Decals[Index];
		if (!Draw[Index])
		{
			if (D.Component != INDEX_NONE)
			{
				ReleaseComponent(D.Component);
				D.Component = INDEX_NONE;
				D.bCpdValid = false;
				D.bTransformValid = false;
			}
			continue;
		}
		if (D.Component == INDEX_NONE)
		{
			D.Component = AcquireComponent();
			if (D.Component == INDEX_NONE)
			{
				continue;
			}
			D.bCpdValid = false;
			D.bTransformValid = false;
		}
		UStaticMeshComponent* Component = Pool.IsValidIndex(D.Component) ? Pool[D.Component].Get() : nullptr;
		if (Component == nullptr)
		{
			continue;
		}
		if (!D.bTransformValid)
		{
			Component->SetRelativeLocation(FVector(D.Geo.CentreXCm, D.Geo.CentreYCm, D.GroundZ + GroundLiftCm));
			Component->SetRelativeRotation(FRotator(0.0, D.Geo.YawDeg, 0.0));
			Component->SetRelativeScale3D(FVector(D.Geo.SizeXCm / 100.0, D.Geo.SizeYCm / 100.0, 1.0));
			D.bTransformValid = true;
		}
		const std::array<float, Cpd::Count> Values = PackCpd(D.Geo, C.Style(D.Style), States[Index], bOuterWash);
		// Only the groups sent update the cache (UpdateCpdCache): a slow drift is sent once it adds up.
		const std::array<bool, CpdGroups> Send = UpdateCpdCache(D.LastCpd, D.bCpdValid, Values);
		for (int32 Group = 0; Group < CpdGroups; ++Group)
		{
			if (Send[Group])
			{
				const int32 First = Group * 4;
				Component->SetCustomPrimitiveDataVector4(First,
					FVector4(Values[First], Values[First + 1], Values[First + 2], Values[First + 3]));
			}
		}
		if (!Component->IsVisible())
		{
			Component->SetVisibility(true);
		}
		++DrawnDecals;
	}

	// ---- forget what is done ----
	for (int32 Index = static_cast<int32>(S.Decals.size()) - 1; Index >= 0; --Index)
	{
		if (!Remove[static_cast<size_t>(Index)])
		{
			continue;
		}
		FAbyssTelegraphState::FDecal& D = S.Decals[static_cast<size_t>(Index)];
		if (D.Component != INDEX_NONE)
		{
			ReleaseComponent(D.Component);
		}
		if (Vfx != nullptr && D.RecipeHandle != 0)
		{
			Vfx->Stop(D.RecipeHandle);
		}
		S.Decals.erase(S.Decals.begin() + Index);
	}
	S.Markers.erase(std::remove_if(S.Markers.begin(), S.Markers.end(),
		[Now](const FAbyssTelegraphState::FMarker& M) { return Now > M.ContactMs + MarkerForgetAfterMs; }), S.Markers.end());
}
