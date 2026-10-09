#include "Framework/AbyssSimDriver.h"

#include "Abyssfire.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

#include <span>
#include <variant>

#include "abyss/base/SimClock.h"
#include "abyss/sim/GameSim.h"

#include "Framework/AbyssActorRegistry.h"
#include "Framework/AbyssGameInstance.h"
#include "Framework/AbyssPresenter.h"
#include "Framework/AbyssUiRoot.h"

UAbyssSimDriver* UAbyssSimDriver::Get(const UObject* WorldContextObject)
{
	const UWorld* OwningWorld = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return OwningWorld ? OwningWorld->GetSubsystem<UAbyssSimDriver>() : nullptr;
}

bool UAbyssSimDriver::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UAbyssSimDriver::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UAbyssActorRegistry>();
}

void UAbyssSimDriver::Deinitialize()
{
	StopVisualSlowMotion();
	WorldView.Reset();
	SnapshotIndex.Reset();
	bSessionActive = false;
	Super::Deinitialize();
}

TStatId UAbyssSimDriver::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAbyssSimDriver, STATGROUP_Tickables);
}

UAbyssGameInstance* UAbyssSimDriver::GetAbyssGameInstance() const
{
	const UWorld* OwningWorld = GetWorld();
	return OwningWorld ? Cast<UAbyssGameInstance>(OwningWorld->GetGameInstance()) : nullptr;
}

// =====================================================================================================================
// Registration
// =====================================================================================================================

void UAbyssSimDriver::RegisterWorldView(IAbyssWorldView* View)
{
	check(IsInGameThread());
	WorldView = TWeakInterfacePtr<IAbyssWorldView>(View);
	// Register from the world subsystem's Initialize: a view registered while a session already runs has missed the
	// zone's spawn events (the core does not replay them).
	const UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
	if (View != nullptr && GameInstance != nullptr && GameInstance->HasSession() && !GameInstance->AreSessionEventsPending())
	{
		UE_LOG(LogAbyss, Warning, TEXT("World view registered during a running session: the current zone's entities were missed"));
	}
}

void UAbyssSimDriver::UnregisterWorldView(IAbyssWorldView* View)
{
	if (WorldView.Get() == View)
	{
		WorldView.Reset();
	}
}

IAbyssWorldView* UAbyssSimDriver::GetWorldView() const
{
	return WorldView.Get();
}

// =====================================================================================================================
// Frame pump
// =====================================================================================================================

void UAbyssSimDriver::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* OwningWorld = GetWorld();
	UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
	if (OwningWorld == nullptr || GameInstance == nullptr)
	{
		return;
	}
	// The core is fed undilated real time: the S6 slow motion is applied by the core itself, and the world's global time
	// dilation (visuals) must not slow the sim a second time (combat-feel.md 11.6).
	const double RealDeltaMs = static_cast<double>(OwningWorld->DeltaRealTimeSeconds) * 1000.0;
	UpdateVisualSlowMotion(RealDeltaMs);

	if (!GameInstance->HasSession())
	{
		if (bSessionActive)
		{
			EndSession();
		}
		return;
	}
	if (!bSessionActive)
	{
		bSessionActive = true;
		FrameInfo = FAbyssFrameInfo();
		SnapshotIndex.Reset();
	}

	// 1. events emitted by NewGame / LoadGame (GameSim::Frame would clear them).
	if (GameInstance->AreSessionEventsPending())
	{
		DispatchEvents(*GameInstance);
		if (!GameInstance->HasSession())
		{
			return;   // a deferred flow call ended the session
		}
	}

	// 2. fixed-step simulation (the core's accumulator, spiral-of-death clamp and S6 dilation).
	abyss::GameSim* Sim = GameInstance->GetSim();
	const int32 Steps = Sim->Frame(RealDeltaMs);
	{
		const abyss::Snapshot& Snap = Sim->View();
		FrameInfo.RealDeltaMs = RealDeltaMs;
		FrameInfo.StepsThisFrame = Steps;
		FrameInfo.SimNowMs = Snap.simNowMs;
		FrameInfo.Alpha = FMath::Clamp(Snap.interpolationAlpha, 0.0, 1.0);
		FrameInfo.RenderSimMs = Snap.simNowMs - (1.0 - FrameInfo.Alpha) * abyss::kSimStepMs;
		FrameInfo.bFrozen = Snap.frozen;
		FrameInfo.bCinematic = Snap.cinematic;
		FrameInfo.VisualTimeDilation = SlowMotionScale;
		++FrameInfo.FrameNumber;
		SnapshotIndex.Rebuild(Snap);
	}

	// 3-4. this frame's events, then the requested autosave and deferred flow calls.
	DispatchEvents(*GameInstance);
	if (!GameInstance->HasSession())
	{
		return;
	}

	// 5. per-frame presentation sync.
	SyncFrame(*GameInstance, RealDeltaMs, Steps);
}

void UAbyssSimDriver::StepNow()
{
	check(IsInGameThread());
	UAbyssGameInstance* GameInstance = GetAbyssGameInstance();
	if (GameInstance == nullptr || !GameInstance->HasSession() || bDispatching)
	{
		return;
	}
	if (GameInstance->AreSessionEventsPending())
	{
		DispatchEvents(*GameInstance);
		if (!GameInstance->HasSession())
		{
			return;
		}
	}
	abyss::GameSim* Sim = GameInstance->GetSim();
	Sim->Step();
	SnapshotIndex.Rebuild(Sim->View());
	DispatchEvents(*GameInstance);
	if (const abyss::Snapshot* Snap = GameInstance->GetSnapshot())
	{
		SnapshotIndex.Rebuild(*Snap);
	}
}

void UAbyssSimDriver::EndSession()
{
	check(IsInGameThread());
	if (IAbyssWorldView* View = WorldView.Get())
	{
		View->ClearZone();
	}
	if (UAbyssActorRegistry* Registry = UAbyssActorRegistry::Get(this))
	{
		Registry->Reset();
	}
	SnapshotIndex.Reset();
	FrameInfo = FAbyssFrameInfo();
	StopVisualSlowMotion();
	bSessionActive = false;
}

// =====================================================================================================================
// Dispatch
// =====================================================================================================================

void UAbyssSimDriver::DispatchEvents(UAbyssGameInstance& GameInstance)
{
	abyss::GameSim* Sim = GameInstance.GetSim();
	if (Sim == nullptr)
	{
		return;
	}
	GameInstance.ClearSessionEventsPending();

	const std::span<const abyss::Event> Events = Sim->Events();
	if (!Events.empty())
	{
		const abyss::Snapshot& Snap = Sim->View();
		// Handlers may Submit (queued) and save (const) but never step / reload the sim: the span stays valid. Session
		// flow calls made now are deferred by the GameInstance until after the loop.
		bDispatching = true;
		for (size_t EventIndex = 0; EventIndex < Events.size(); ++EventIndex)
		{
			DispatchOne(GameInstance, Events[EventIndex], Snap);
		}
		bDispatching = false;
	}
	GameInstance.FlushRequestedSave();
	GameInstance.RunDeferredFlow();
}

void UAbyssSimDriver::DispatchOne(UAbyssGameInstance& GameInstance, const abyss::Event& Event, const abyss::Snapshot& Snap)
{
	// -- world view pre-hooks: the actor / zone exists before anybody else sees the event.
	if (const abyss::EvEntitySpawned* Spawned = std::get_if<abyss::EvEntitySpawned>(&Event))
	{
		if (IAbyssWorldView* View = WorldView.Get())
		{
			View->SpawnEntity(*Spawned, Snap);
		}
	}
	else if (const abyss::EvZone* ZoneEntered = std::get_if<abyss::EvZone>(&Event);
			 ZoneEntered != nullptr && ZoneEntered->phase == abyss::EvZone::Phase::Entered)
	{
		if (IAbyssWorldView* View = WorldView.Get())
		{
			View->BuildZone(Snap);
		}
	}

	// -- presenters (entity-addressed events, per-actor hit-stop).
	RouteToPresenters(Event);

	// -- backbone-handled events.
	if (const abyss::EvSaveRequested* SaveRequest = std::get_if<abyss::EvSaveRequested>(&Event))
	{
		GameInstance.NotifySaveRequested(SaveRequest->reason);
	}
	else if (const abyss::EvSlowMotion* SlowMotion = std::get_if<abyss::EvSlowMotion>(&Event))
	{
		if (SlowMotion->realDurationMs > 0.0 && SlowMotion->timeScale < 1.0)
		{
			StartVisualSlowMotion(SlowMotion->timeScale, SlowMotion->realDurationMs);
		}
		else
		{
			StopVisualSlowMotion();
		}
	}

	// -- subscribers.
	GameInstance.GetEventRouter().Dispatch(Event);

	// -- world view post-hooks: remove after everybody had the chance to use the actor.
	if (const abyss::EvEntityDespawned* Despawned = std::get_if<abyss::EvEntityDespawned>(&Event))
	{
		if (IAbyssWorldView* View = WorldView.Get())
		{
			View->DespawnEntity(*Despawned);
		}
	}
	else if (const abyss::EvZone* ZoneExited = std::get_if<abyss::EvZone>(&Event);
			 ZoneExited != nullptr && ZoneExited->phase == abyss::EvZone::Phase::Exited)
	{
		if (IAbyssWorldView* View = WorldView.Get())
		{
			View->ClearZone();
		}
	}
}

void UAbyssSimDriver::RouteToPresenters(const abyss::Event& Event)
{
	const UAbyssActorRegistry* Registry = UAbyssActorRegistry::Get(this);
	if (Registry == nullptr)
	{
		return;
	}
	if (const abyss::EvPlayAnim* PlayAnim = std::get_if<abyss::EvPlayAnim>(&Event))
	{
		if (IAbyssPresenter* Presenter = Registry->FindPresenter(PlayAnim->entity))
		{
			Presenter->OnPlayAnim(*PlayAnim, FrameInfo);
		}
	}
	else if (const abyss::EvHit* Hit = std::get_if<abyss::EvHit>(&Event))
	{
		if (IAbyssPresenter* Target = Registry->FindPresenter(Hit->target))
		{
			Target->OnHitTaken(*Hit);
			// combat-feel.md 11.1: a monster that survives a resolved hit freezes for targetStopMs (a kill plays the death
			// instead; DoT ticks only flash; the hero gets a pain tint and recoil, no freeze).
			const bool bTargetFreeze = Hit->targetFaction == abyss::Faction::Monster && !Hit->dodged && !Hit->tick
				&& !Hit->killed && Hit->profile.targetStopMs > 0.0;
			if (bTargetFreeze)
			{
				Target->ApplyHitStop(static_cast<float>(Hit->profile.targetStopMs));
			}
		}
		if (Hit->source != abyss::kNoEntity)
		{
			if (IAbyssPresenter* Source = Registry->FindPresenter(Hit->source))
			{
				Source->OnHitDealt(*Hit);
				// Already resolved by the core (hero basic attack only, monster swings x0.6, 0 for skills / ticks / pets).
				if (Hit->attackerStopMs > 0.0)
				{
					Source->ApplyHitStop(static_cast<float>(Hit->attackerStopMs));
				}
			}
		}
	}
	else if (const abyss::EvStatusApplied* StatusApplied = std::get_if<abyss::EvStatusApplied>(&Event))
	{
		if (IAbyssPresenter* Presenter = Registry->FindPresenter(StatusApplied->target))
		{
			Presenter->OnStatusApplied(*StatusApplied);
		}
	}
	else if (const abyss::EvStatusExpired* StatusExpired = std::get_if<abyss::EvStatusExpired>(&Event))
	{
		if (IAbyssPresenter* Presenter = Registry->FindPresenter(StatusExpired->target))
		{
			Presenter->OnStatusExpired(*StatusExpired);
		}
	}
	else if (const abyss::EvEntityTeleported* Teleported = std::get_if<abyss::EvEntityTeleported>(&Event))
	{
		if (IAbyssPresenter* Presenter = Registry->FindPresenter(Teleported->id))
		{
			Presenter->OnTeleported(*Teleported);
		}
	}
	else if (const abyss::EvMonsterAttackCancelled* Cancelled = std::get_if<abyss::EvMonsterAttackCancelled>(&Event))
	{
		if (IAbyssPresenter* Presenter = Registry->FindPresenter(Cancelled->monster))
		{
			Presenter->OnAttackCancelled(*Cancelled);
		}
	}
	else if (const abyss::EvMonsterRenamed* Renamed = std::get_if<abyss::EvMonsterRenamed>(&Event))
	{
		if (IAbyssPresenter* Presenter = Registry->FindPresenter(Renamed->monster))
		{
			Presenter->OnRenamed(*Renamed);
		}
	}
}

void UAbyssSimDriver::SyncFrame(UAbyssGameInstance& GameInstance, double RealDeltaMs, int32 Steps)
{
	const abyss::Snapshot* Snap = GameInstance.GetSnapshot();
	if (Snap == nullptr)
	{
		return;
	}
	FrameInfo.RealDeltaMs = RealDeltaMs;
	FrameInfo.StepsThisFrame = Steps;
	FrameInfo.VisualTimeDilation = SlowMotionScale;
	SnapshotIndex.Rebuild(*Snap);

	if (IAbyssWorldView* View = WorldView.Get())
	{
		View->SyncFrame(*Snap, FrameInfo);
	}
	if (IAbyssUiRoot* Root = GameInstance.GetUiRoot())
	{
		Root->SyncFrame(*Snap, FrameInfo);
	}
	GameInstance.GetEventRouter().OnFrame.Broadcast(*Snap, FrameInfo);
}

// =====================================================================================================================
// Visual slow motion (S6)
// =====================================================================================================================

void UAbyssSimDriver::StartVisualSlowMotion(double TimeScale, double RealDurationMs)
{
	SlowMotionScale = static_cast<float>(FMath::Clamp(TimeScale, 0.05, 1.0));
	SlowMotionRemainingRealMs = RealDurationMs;
	UGameplayStatics::SetGlobalTimeDilation(this, SlowMotionScale);
}

void UAbyssSimDriver::UpdateVisualSlowMotion(double RealDeltaMs)
{
	if (SlowMotionRemainingRealMs <= 0.0)
	{
		return;
	}
	SlowMotionRemainingRealMs -= RealDeltaMs;
	if (SlowMotionRemainingRealMs <= 0.0)
	{
		StopVisualSlowMotion();
	}
}

void UAbyssSimDriver::StopVisualSlowMotion()
{
	const bool bWasActive = SlowMotionRemainingRealMs > 0.0 || SlowMotionScale != 1.f;
	SlowMotionRemainingRealMs = 0.0;
	SlowMotionScale = 1.f;
	if (bWasActive && GetWorld() != nullptr)
	{
		UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	}
}
