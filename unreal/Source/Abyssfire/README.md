# Abyssfire UE module — backbone

The `Abyssfire` module is the presentation layer of the UE 5.8 rebuild. It **renders core state, turns input into
commands, plays animation / VFX / audio from core events and builds the Slate UI**. Every rule and all game state live
in `AbyssCore` (`Source/AbyssCore`, pure C++20). Read `Docs/ARCHITECTURE.md`, `Docs/DECISIONS.md` and
`Docs/spec/ue58-platform.md` first.

```
 UAbyssGameInstance  (app lifetime)                      UWorld (L_Main, one persistent map)
 ├─ abyss::DataStore    Data/*.json, i18n tables          ├─ AAbyssGameMode       no pawn; BeginPlay -> NotifyGameWorldReady
 ├─ abyss::GameSim      one per session (New/Continue)    ├─ AAbyssPlayerController input -> abyss::Command
 ├─ FAbyssSaveStorage   3 slots + settings.json           ├─ UAbyssSimDriver      ticks the core, dispatches events
 ├─ FAbyssEventRouter   typed event delegates             ├─ UAbyssActorRegistry  EntityId -> actor
 ├─ FAbyssUserSettings  device settings (U9)              ├─ IAbyssWorldView      (world agent) zone + actors
 └─ IAbyssUiRoot        (ui agent) registered             └─ IAbyssPresenter      (world agent) per character actor
```

## Classes

| Class | File | Lifetime | Role |
|---|---|---|---|
| `UAbyssGameInstance` | `Framework/AbyssGameInstance.h` | app | Loads `Data/*.json` (pak-aware `FFileHelper`) into `abyss::DataStore`; owns the `GameSim` of the session, the save storage, settings, the event router; app state `Boot -> MainMenu <-> InGame` (or `DataError`); app lifecycle (background = freeze + synchronous save). |
| `UAbyssSimDriver` | `Framework/AbyssSimDriver.h` | game world (`UTickableWorldSubsystem`) | Calls `GameSim::Frame(realDtMs)` once per frame, dispatches the frame's events in order, writes requested autosaves, syncs the world view / UI, visual slow motion (S6). |
| `FAbyssEventRouter` | `Framework/AbyssEventRouter.h` | app (in the GI) | One `TMulticastDelegate<void(const Ev&)>` per `abyss::Event` alternative + `OnAnyEvent`, `OnFrame`, `OnSessionStarted`, `OnSessionEnded`. |
| `UAbyssActorRegistry` | `Framework/AbyssActorRegistry.h` | game world | `EntityId -> AActor` (weak), kind, reverse lookup, presenter lookup. |
| `IAbyssWorldView` | `Framework/AbyssWorldView.h` | world (world agent implements) | Build / clear zone, spawn / despawn entity actors, per-frame transforms, ground height. |
| `IAbyssPresenter` | `Framework/AbyssPresenter.h` | actor (world agent implements) | Entity-addressed events: anim, hit taken / dealt, per-actor hit-stop, statuses, teleport, telegraph cancel, rename. |
| `IAbyssUiRoot` | `Framework/AbyssUiRoot.h` | GI (ui agent implements) | App-state screens, per-frame HUD sync, Back handling, locale change, error screen. |
| `AAbyssGameMode` | `Framework/AbyssGameMode.h` | world | No pawn; tells the GI the world (and viewport) is ready. |
| `AAbyssPlayerController` | `Framework/AbyssPlayerController.h` | world | Cursor / input mode, `SubmitCommand`, `DeprojectToGround/Tile`, `GetUiDpiScale`; input hooks for the input agent. |
| `FAbyssSnapshotIndex`, `FAbyssFrameInfo` | `Framework/AbyssTypes.h` | per frame | EntityId lookups into the snapshot, frame timing (alpha, render sim time). |
| `AbyssUnits`, `AbyssText` | `Framework/AbyssUnits.h`, `AbyssText.h` | — | Tile <-> world (S4: X = col·100, Y = row·100), UTF-8 <-> FString / FText, i18n resolution. |
| `FAbyssSaveStorage` | `Platform/AbyssSaveStorage.h` | GI | `abyss::ISaveStorage`: `Saved/SaveGames/abyssfire_slot{0..2}.json`, tmp -> flush -> `.bak` -> rename, async chained writes. |
| `FAbyssUserSettings`, `AbyssPlatform` | `Platform/` | — | settings.json schema; touch-layout and quality-tier resolution; console vars `abyss.Quality`, `abyss.RenderScale`. |

## Frame pipeline (per rendered frame, game thread)

`UAbyssSimDriver::Tick` runs in the world's tickable phase (after the actor tick groups up to `TG_PostPhysics`, before
the camera manager update):

1. Events left by `NewGame` / `LoadGame` are dispatched first (`GameSim::Frame` clears the event list).
2. `GameSim::Frame(World->DeltaRealTimeSeconds * 1000)`: the core accumulates real time, runs 0..4 fixed 60 Hz steps
   (spiral-of-death clamp, 250 ms max frame after a resume), applies its own S6 dilation, then `AdvanceRealTime` (story,
   music). **Never add a second accumulator in UE** — `Snapshot::interpolationAlpha` and the dilation live in the core.
3. Each event, in emission order:
   world view pre-hook (`SpawnEntity` on `EvEntitySpawned`, `BuildZone` on `EvZone{Entered}`) ->
   presenter routing (registry) -> backbone (`EvSaveRequested` -> autosave flag, `EvSlowMotion` -> global time
   dilation for the real duration) -> router (`OnAnyEvent`, typed delegate) ->
   world view post-hook (`DespawnEntity` on `EvEntityDespawned`, `ClearZone` on `EvZone{Exited}`).
4. The requested autosave is written (once per frame, async); flow calls deferred during the dispatch run.
5. `FAbyssSnapshotIndex` rebuilt, `IAbyssWorldView::SyncFrame`, `IAbyssUiRoot::SyncFrame`, router `OnFrame`.

Core facts to rely on: on a zone change the batch is `despawns(ZoneUnload) ... EvZone{Exited} ... EvEntitySpawned(hero,
NPCs, monsters, props, pet) ... EvZone{Entered} ... log, banner, EvSaveRequested`, so spawns come **before** `BuildZone`;
the hero (`kHeroEntityId` = 1) is re-announced on every zone entry without a despawn.

Hit-stop is **per actor** (combat-feel.md 11.2): the driver calls `IAbyssPresenter::ApplyHitStop` with
`profile.targetStopMs` (monster target, resolved non-killing non-tick hit) and `EvHit::attackerStopMs` (attacker, already
resolved by the core). Slow motion (S6) is the only global time dilation and the core is always fed undilated time.

## Threading

Game thread only, like the core. The only worker code is the save file I/O (`FAbyssSaveStorage::Write`, a chained
`UE::Tasks` task that owns a copy of the bytes). Never call into `GameSim`, the snapshot or the router from another
thread; never keep `abyss::Snapshot` pointer members (`inventory`, `quests`, `story`, ...) or event payloads across
frames — copy what you need.

## Subscribing to events

```cpp
#include "Framework/AbyssGameInstance.h"

void UMyAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    FAbyssEventRouter& Router = CastChecked<UAbyssGameInstance>(GetGameInstance())->GetEventRouter();
    Router.On<abyss::EvMusic>().AddUObject(this, &UMyAudioSubsystem::HandleMusic);   // void HandleMusic(const abyss::EvMusic&)
    Router.On<abyss::EvSfx>().AddWeakLambda(this, [this](const abyss::EvSfx& E) { PlayCue(E); });
    Router.OnFrame.AddUObject(this, &UMyAudioSubsystem::HandleFrame);               // (const abyss::Snapshot&, const FAbyssFrameInfo&)
}

void UMyAudioSubsystem::Deinitialize()
{
    if (UAbyssGameInstance* GI = Cast<UAbyssGameInstance>(GetGameInstance())) { GI->GetEventRouter().RemoveAll(this); }
    Super::Deinitialize();
}
```

Order between two subscribers of the same event type is unspecified. If you need "the actor exists", rely on the world
view hooks (spawn before subscribers, despawn after). Handlers may `Submit` commands; flow calls (`StartNewGame`,
`ContinueSlot`, `ReturnToMainMenu`, `QuitGame`) made inside a handler are deferred to the end of the dispatch.

## Submitting commands

Every UI / input action is an `abyss::Command` (`abyss/sim/Commands.h`), queued for the next step:

```cpp
GI->Submit(abyss::CmdCastSkill{.slot = 2});                      // or PlayerController->SubmitCommand(...)
GI->Submit(abyss::CmdOpenPanel{abyss::PanelId::Inventory});      // UE-owned panels only (SimTypes.h ownership)
GI->Submit(abyss::CmdClosePanel{abyss::PanelId::Dialogue});      // closes a core-owned modal through its owner
```

The core rejects hero gameplay commands while `Snapshot::inputBlocked` (frozen / modal / Dying); there is no need to
gate them in UE. Tiles are `abyss::Vec2{col, row}`; use `AAbyssPlayerController::DeprojectToTile` and
`AbyssUnits::TileToWorld / WorldToTile`.

## Reading state and interpolating

```cpp
const abyss::Snapshot* Snap = GI->GetSnapshot();                       // nullptr without a session
const FAbyssFrameInfo& Frame = UAbyssSimDriver::Get(this)->GetFrameInfo();
const abyss::Vec2 Tile = AbyssUnits::LerpTile(Snap->hero.prevPos, Snap->hero.pos, Frame.Alpha);
const FVector Location = AbyssUnits::TileToWorld(Tile, WorldView->GetGroundHeight(...));
const abyss::MonsterView* Monster = UAbyssSimDriver::Get(this)->GetSnapshotIndex().FindMonster(Id);
```

Anything timed in sim ms (`EvProjectileLaunched::launchMs`, `EvPlayAnim::startMs`, ground effects) is placed with
`FAbyssFrameInfo::RenderSimMs`, the sim time the interpolated pose shows.

## Implementing the interfaces

```cpp
// World agent: one world-view object per game world, registered from Initialize (spawn events are never replayed).
UCLASS() class UAbyssWorldBuilder : public UWorldSubsystem, public IAbyssWorldView { ... };
void UAbyssWorldBuilder::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (UAbyssSimDriver* Driver = Collection.InitializeDependency<UAbyssSimDriver>())   // null outside Game / PIE worlds
    {
        Driver->RegisterWorldView(this);
    }
}
// Deinitialize: Driver->UnregisterWorldView(this). Restrict the subsystem to Game / PIE worlds (DoesSupportWorldType).
// SpawnEntity: spawn / reuse the actor, then UAbyssActorRegistry::Get(this)->Register(E.id, Actor, E.kind).
// Character actors implement IAbyssPresenter (the driver finds them through the registry).

// UI agent: one root per game instance.
UCLASS() class UAbyssUiSubsystem : public UGameInstanceSubsystem, public IAbyssUiRoot { ... };
void UAbyssUiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    CastChecked<UAbyssGameInstance>(GetGameInstance())->RegisterUiRoot(this);   // calls OnAppStateChanged(current)
}
```

`OnAppStateChanged(MainMenu)` arrives once the game world began play (the viewport exists): add Slate widgets with
`GEngine->GameViewport->AddViewportWidgetContent` then, not in `Initialize`.

## Saves, settings, lifecycle

* Slots: `UAbyssGameInstance::ListSlots` (`abyss::SaveSlotInfo`, incl. `showDifficultySelector`), `StartNewGame(class,
  slot)`, `ContinueSlot(slot, difficultyOverride, bFromBackup)` (returns `FAbyssLoadResult`: `VersionTooNew` vs corrupt,
  `bBackupAvailable`), `DeleteSlot`, `ReturnToMainMenu`, `QuitGame`. The UI confirms overwrites / deletes (U1).
* Autosaves are requested by the core (`EvSaveRequested`: zone entry, turn-in, level-up, 60 s, respawn, ...) and written
  by the GI after the dispatch. App background / deactivate: `CmdAppBackground{true}` + one immediate step +
  synchronous write; foreground: `CmdAppBackground{false}`. Window close / PIE stop: resolve death, synchronous save.
* Settings: `GetUserSettings` / `ApplyUserSettings` (persists `settings.json`, switches the core locale, re-resolves touch
  mode and quality tier, broadcasts `OnSettingsChanged` / `OnLocaleChanged`).
* All player-facing text comes from the core's i18n (`GI->Localize(LocText)` / `AbyssText::Localize`); backbone error
  titles use `ui.error.*` keys with an English fallback.

## Fonts

`unreal/Fonts/` holds OFL subsets made by `Scripts/fonts/build_fonts.py` (re-run after the string tables change):
`NotoSansSC/TC-{Regular,Bold}.otf` (body), `NotoSerifSC/TC-{Regular,Bold}.otf` (story, boss bar),
`Cinzel-{Regular,Bold}.ttf` (titles; Latin only), plus the OFL texts. They are staged UFS and loaded at runtime from
`FPaths::ProjectDir() / "Fonts"` (ue58-platform.md 9.4). zh-TW: use the TC face with an SC fallback (the exported zh-TW
table still contains some simplified-only characters).

## File ownership for the next agents

| Agent | Owns | Must not edit |
|---|---|---|
| **world** | `Public|Private/World/`, `Actors/`, `Anim/`, `Vfx/`, `Camera/` — implements `IAbyssWorldView` (world builder, terrain, props, lights, post-process, quality-tier application) and `IAbyssPresenter` (character actors, `UAbyssAnimInstance`), VFX from router events, the camera rig (view target, follow, zoom, shake from `EvCameraShake` honouring `bCameraShake`) | `Framework/`, `Platform/` (request changes) |
| **input** | `Public|Private/Input/` and the "input agent region" of `Framework/AbyssPlayerController.h` + all of `Private/Input/AbyssPlayerControllerInput.cpp` (runtime Enhanced Input, KBM / gamepad / touch, picking, virtual-control injection, `IA_Back -> IAbyssUiRoot::HandleBack`) | the rest of `Framework/` |
| **ui** | `Public|Private/UI/` — implements `IAbyssUiRoot` (menus, HUD, panels, story overlay, touch controls, settings panel, error screen), Slate style with the bundled fonts, world-anchored widgets | `Framework/`, `Platform/` |
| **audio** | `Public|Private/Audio/` and `unreal/Audio/` — music state machine from `EvMusic`, SFX from `EvSfx` (A4 panning), volumes from `FAbyssUserSettings`, menu music on `MainMenu` | `Framework/` |
| **content** | `unreal/Scripts/` (except `Scripts/fonts/`, backbone) — editor Python: import FBX / textures / audio, materials + `MPC_AF_Lighting`, `L_Main` | `Source/` |

Backbone (this agent / lead): `Abyssfire.Build.cs`, `Public/Abyssfire.h`, `Private/Abyssfire.cpp`, `Private/AbyssfirePCH.h`,
`Framework/` (except the input region), `Platform/`, `unreal/Config/`, `Abyssfire.uproject`, `Source/*.Target.cs`,
`unreal/Fonts/`, `Scripts/fonts/`. Changes there go through the lead. New module dependencies (UMG, Niagara,
ProceduralMeshComponent, ...) are requested, not added ad hoc.

## Coding rules (UE side)

* UE 5.8, C++20, IWYU: include what you use; `CoreMinimal.h` first in headers; `.generated.h` last.
* `UPROPERTY()` + `TObjectPtr<>` for every UObject pointer member that must stay alive; `TWeakObjectPtr` otherwise.
  Slate widgets are not UObjects: hold UObjects they use through an owning UObject.
* Shadowing is an error on every UE toolchain: never name a local / parameter / lambda capture like a member of the
  class or of its UE bases (`World`, `Player`, `InputComponent`, `Owner`, `Tags`, ...). Unity builds merge `.cpp`
  files: anonymous-namespace helpers need module-unique names.
* No gameplay rules, no hardcoded tables, no `std::rand`, no exceptions / RTTI. Text = core i18n keys.
* Deprecated APIs to avoid: `FString(int32, const char*)` (use `FString::ConstructFromPtrSize`),
  `FCoreDelegates::ApplicationWillTerminateDelegate` (use `GetApplicationWillTerminateDelegate()`),
  `UInputTriggerCombo`, legacy input mappings.
