// UAbyssSimDriver: the one ticker of the core (ue58-platform.md 13 "One driver"). Per rendered frame, in the world's
// tickable phase (after the actor tick groups up to TG_PostPhysics, before the camera update):
//
//   1. events still pending from a session start (NewGame / LoadGame emit before the first frame) are dispatched
//   2. GameSim::Frame(undilated real dt ms): the core accumulates real time, runs 0..4 fixed 60 Hz steps (spiral-of-death
//      clamp SimConfig::maxStepsPerFrame, maxFrameMs 250 after a resume), applies its own S6 dilation to the sim, then
//      AdvanceRealTime (story / music timers on real time). The UE side must NOT run a second accumulator: the snapshot's
//      interpolationAlpha and the S6 dilation live in the core's SimClock.
//   3. dispatch of this frame's events, per event in emission order:
//        world view pre-hooks (SpawnEntity, BuildZone) -> presenters (IAbyssPresenter routing, per-actor hit-stop)
//        -> system handling (EvSaveRequested, EvSlowMotion) -> FAbyssEventRouter (OnAnyEvent, typed delegate)
//        -> world view post-hooks (DespawnEntity, ClearZone)
//   4. the requested autosave is written (once per frame, async), deferred GameInstance flow calls run
//   5. snapshot index rebuilt, IAbyssWorldView::SyncFrame, IAbyssUiRoot::SyncFrame, router OnFrame
//
// Time: the core is fed WORLD->DeltaRealTimeSeconds (undilated). EvSlowMotion (elite kill, S6 / combat-feel 11.6) sets
// the world's global time dilation for its real duration so animations, VFX and camera slow down with the sim; it is
// restored on expiry, on EvSlowMotion{1, 0} (zone exit) and when the session ends. Hit-stop is per actor (presenters).
//
// Threading: game thread only, like the core.
#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakInterfacePtr.h"

#include "abyss/sim/Events.h"
#include "abyss/sim/Snapshot.h"

#include "Framework/AbyssTypes.h"
#include "Framework/AbyssWorldView.h"

#include "AbyssSimDriver.generated.h"

class UAbyssGameInstance;

UCLASS()
class ABYSSFIRE_API UAbyssSimDriver : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UAbyssSimDriver* Get(const UObject* WorldContextObject);

	// ---- registration ----
	/** The world agent's world builder (one per world). Replaces any previous one. */
	void RegisterWorldView(IAbyssWorldView* View);
	void UnregisterWorldView(IAbyssWorldView* View);
	IAbyssWorldView* GetWorldView() const;

	// ---- per-frame state ----
	const FAbyssFrameInfo& GetFrameInfo() const { return FrameInfo; }
	/** Lookups into the current snapshot (valid until the next call into GameSim). */
	const FAbyssSnapshotIndex& GetSnapshotIndex() const { return SnapshotIndex; }
	/** True while events are being delivered (flow calls made now are deferred by the GameInstance). */
	bool IsDispatching() const { return bDispatching; }

	// ---- used by UAbyssGameInstance ----
	/** Applies queued commands now with one GameSim::Step (pending events first), then dispatches (app background). */
	void StepNow();
	/** Session ended: world view ClearZone, registry reset, visual time dilation restored, router OnSessionEnded. */
	void EndSession();

	// ---- USubsystem / FTickableGameObject ----
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	UAbyssGameInstance* GetAbyssGameInstance() const;
	/** Delivers GameSim::Events() (pending session-start events or this frame's). */
	void DispatchEvents(UAbyssGameInstance& GameInstance);
	void DispatchOne(UAbyssGameInstance& GameInstance, const abyss::Event& Event, const abyss::Snapshot& Snap);
	void RouteToPresenters(const abyss::Event& Event);
	void SyncFrame(UAbyssGameInstance& GameInstance, double RealDeltaMs, int32 Steps);
	void StartVisualSlowMotion(double TimeScale, double RealDurationMs);
	void UpdateVisualSlowMotion(double RealDeltaMs);
	void StopVisualSlowMotion();

	TWeakInterfacePtr<IAbyssWorldView> WorldView;
	FAbyssFrameInfo FrameInfo;
	FAbyssSnapshotIndex SnapshotIndex;
	double SlowMotionRemainingRealMs = 0.0;
	float SlowMotionScale = 1.f;
	bool bDispatching = false;
	bool bSessionActive = false;
};
