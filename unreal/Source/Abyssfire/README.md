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
| `FAbyssUserSettings`, `AbyssPlatform` | `Platform/` | — | settings.json schema; touch-layout and quality-tier resolution; console vars `abyss.Quality`, `abyss.RenderScale`; switches `-abysstouch`, `-abyssquality=`, `-abyssscreenmessages` (keeps UE's on-screen message block, which non-shipping `-game` runs hide: `ApplyScreenMessagePolicy`, `Scripts/README.md` §3). |
| `UAbyssAutoTestSubsystem`, `AbyssAutoTest` | `Private/Framework/AbyssAutoTest*.h` | GI (only with `-abyssautotest`) | In-game autotest: drives the shared `abyss::AutoPilot` bot through the real game, screenshots with UI, frame-time and log statistics, `report.json`, exit code. See *In-game autotest* below. |

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

## Dark remake presentation: theme, mood, actions (1C U1)

Chapter 1 is "The Burning March" (`Docs/dark-remake/`). The decisions of this layer live in UE-free, header-only helpers
(`Private/World/AbyssThemeRules.h`, `Private/Actors/AbyssAnimActionRules.h`; std + AbyssCore public headers only), unit
tested by `unreal/PresentationTests/run.sh` (GCC + Clang, doctest; real `Data/*.json` + synthetic tables; a macro shim
compiles every `Private/**/Abyss*Rules.h` after the UE / Windows / Apple macros). The glue below only converts types.

* **Presentation theme.** `maps.json` `presentationTheme` (emerald_plains: `march`). In such a zone every art chain
  resolves `<gameId>@<theme>` for **all** of its ids before any plain id (`FAbyssArtManifest::ResolveFirst(Ids, Hash,
  Theme)`; `{decor_tree, tree}` reaches `tree@march` before the plain `decor_tree`); a missing variant falls back to
  today's asset. Covered: zone dressing (decor, outcrops / palisades / gate posts / reeds on the generator-theme id
  strings, e.g. `outcrop_plains@march`, camp kit `camp_*@march`, story decor, the sealed exit, skirt), characters and
  props (`npc_*@march`, lore / gather / chest / puzzle props, random-event NPC looks; the hero and its soul echo never),
  the elite crown (`elite_crown@march`, else `SM_FX_EliteCrown`), zone loops (`camp.torch@march`: the brazier's smoulder,
  no light pool) and portraits (the resolved asset's manifest `portrait`; elsewhere the name probes keep today's, and the
  asset portrait only replaces the generic emblem, e.g. the Corpseherd). Two exceptions (1C review): only the hub camp
  (the map's first: Knellwatch) flies the themed camp banner, the fort's own standard (`AbyssThemeRules::PlacesCampProp`;
  Crowmarket keeps the rest of the kit), and a themed-only loop (`ThemedOnlyRecipe`: `exit.sealed`) plays its `@theme`
  recipe or nothing, never today's crimson ring (DR5), until `exit.sealed@march` exists. The theme is derived per call from the map id
  of the snapshot / event at hand (spawns of the next zone arrive before `BuildZone`), never cached at the zone build.
  The manifest load warns when a variant's footprint / blocking differs from its plain id (the core bakes walkability
  from the plain ids). The terrain style (`terrain_styles.json themes.march`) and `MI_AF_Terrain_March` follow the
  presentation theme (the procedural `M_AF_Terrain` in march colours until the content build made the instance).
* **Mood** (`AAbyssZoneActor::UpdateMood`, every frame from the world builder): the zone's mood (themeByZone), its
  `<theme>_dawn` mood once `monster_ai.json chapterCompleteQuest` is turned in (march -> march_dawn; a naming convention
  until W2's flag), and a map `regions` mood (the Breach Rim, r 16 around (15, 95)) blended in over the last 4 tiles of its
  radius (smoothstep). Sun colour, fog and grade are re-applied only when the blend moved by more than 0.01. The dominant
  mood (region weight >= .5 to enter, < .4 to leave) brings its `port.moodWeather` ambience, else the zone's weather; the
  world builder swaps the ambience loop when its recipe or tint changes. The weather tint colours the recipe only in a
  zone with a presentation theme (ashfall #CFCAC0, the Rim's violet sparks; other zones' tints stay unused). The blended
  `port.lightScale` reaches `UAbyssVfxSystem::SetMoodLightScale` (march .32, dawn .22, Rim .40; moods without one keep
  R18's formula).
* **Wind-up tint** (DR5): `zone_moods.json port.windupTint.<mood> {normal, elite}` of the dominant mood, else the base
  mood (the march moods: crimson #D11F2E, elites #FF3A48), else the web port's #FFC4B0 / #FF7A5C; through
  `FAbyssCharacterFrame::WindupTint*` into CPD Telegraph (x 0.85).
* **Echo lore** (lore `spriteType` echo, manifest `materials.MI_AF_Echo`): the lore prop has no mesh, only the
  `prop.lore` ground mark in #A8B0C4. On `EvLoreCollected` the figures (`asset`, `clip` from `clipAsset`, `frame`,
  `offsetM` around the lore tile with UE Y = -Blender Y, `yawDeg` -> mesh yaw -yawDeg) spawn as single-node skeletal
  components frozen at the frame (`PlayAnimation`, `SetPosition`, `SetPlayRate(0)`), every slot `M_AF_Ghost`, CPD Tint =
  the linear echo tint, Ghost 1, Fade along the timeline (1 -> 0 in 450 ms ease-out, flicker <= 0.08 at 4 Hz for 2100 ms,
  -> 1 in 450 ms ease-in; real time) and `prop.echo` at each figure; the echo zone's figure meshes / clips load at the zone
  build.
* **Brightness**: `FAbyssUserSettings::Brightness` 0.5..1.5 (settings.json `video.brightness`, default 1; older files
  load as 1), the settings panel's Display slider, `log2(brightness)` EV added to `abyss.ExposureBias` on the zone
  post-process (`AAbyssZoneActor::SetUserExposureEv`).
* **Monster actions** (K1 `AnimAction` Roar / Taunt / Stun / Attack02 / Cast02 / Rise): the clip of the same name (an
  asset without it keeps its pose); Attack02 is an attack (contact beat, wind-up tint), Cast02 a cast, the rest an
  `Emote` (blend state "cast", no beat: the boss intro's Roar plays through the frozen world). Holds: while
  `MonsterView::holdAction` is Taunt (the mini-boss's pre-fight dialogue) the Taunt clip loops (forced loop; the
  `EvPlayAnim{Taunt}` that starts it sets the facing toward the hero for the whole hold), and a stunned monster (status
  Stun) loops its Stun clip while no action plays. Priority death > action > hold > stun > move > idle.

## Dark remake audio (1C U7)

The audio decisions of the remake's Chapter 1 live in UE-free helpers beside the player (`Private/Audio/`:
`AbyssAudioDataRules.h` (manifest JSON, region geometry and hysteresis, `while`), `AbyssVoiceRules.h`,
`AbyssMusicRules.h`, `AbyssAmbienceRules.h`, `AbyssCueTriggerRules.h`), tested by `unreal/PresentationTests/`
(`tests/audio_*_test.cpp` on the committed `Audio/Export/audio_manifest.json` and the real `Data/`). The manifest keeps
the sections as JSON (`FAbyssAudioManifest::{VoiceFamilies,MusicDirector,Ambience,CueTriggers}Json`); a manifest
without them changes nothing. `UAbyssAudioSystem` (details: `unreal/Audio/README.md`, *Runtime*):

* **Voices**: the family of a monster comes from `voiceFamilies.monsters[defId]` (hunt ids included), else the design
  family (withheld from the W1 export), else its animCategory (every later chapter, as before); silent monsters play no
  family cue; named cues replace the family's aggro / hurt / death vocal and play on beats (`EvPlayAnim` → `anim.<action>`,
  `EvMiniBossDialogue` opened → `miniboss.opened`, a cutscene `say` step whose speaker is the monster → `story.say`).
  Spawns are remembered by def id (`EntityDefs`), so a death after the snapshot dropped the monster keeps its voice; a
  def's voice is resolved once (`VoiceOfDef` → `AbyssVoiceRules::ResolveDefVoice`: family, silent, the three named
  EvSfx cues, whether an `anim.<action>` cue exists, encounter / mini-boss flags) and cached in `DefVoices`, so the
  per-event `monster_aggro` / `monster_hurt` / `monster_death` / `EvPlayAnim` path is two map lookups.
* **Music**: `HandleMusic` hands the core's request (after `zoneOverrides`, classified zone / victory / boss) to the
  `AbyssMusicRules::Arbiter`; `HandleFrame` → `TickRemake` resolves it against the director's layers once per frame
  (story > victory > boss > siege > region > zone; the decision is applied in the same rendered frame, the router's
  `OnFrame` follows the event dispatch) and crossfades through `PlayMusic`. Layers keep their choice during a cutscene
  / boss intro. A zone-overridden request resolving to the track already sounding is a no-op even with restart (the
  released combat row), a stale one-shot is never replayed. A track `PlayMusic` could not voice (missing sound; no
  component under `-nosound` / without an audio world) counts as asked (`UnvoicedTrack`; a missing component is
  retried once a second), so the arbiter does not request it every frame. `abyss.Audio.Music` in a session goes
  through the arbiter.
* **Ambience**: the bed key uses the map's presentation theme; region beds / layers / emitters run in `TickRemakeAmbience`
  on their own persistent components (`AreaLoopComponents`, `EmitterComponents`), volumes on the ambience bus (SFX
  volume, cinematic duck). The set-piece layer hands emitter spots over with
  `UAbyssAudioSystem::SetZoneEmitterSpots(EmitterId, Tiles)` when it builds a zone (cleared on `EvZone{Exited}`; the
  spots are re-read on the next frame); without spots an emitter plays at its manifest placement. An emitter that had
  to start as a plain 2D loop (no listener on its first frames) is recreated as a panned source once a listener exists.
* **Cue triggers**: `EvStoryBeat` (end), `EvStoryStep` (step / slide), `EvRandomEvent`, `EvStoryDecorFocus` and timers
  (`AbyssCueTriggerRules::Scheduler`, real time) play the bells and the beat-bound voices; every one played is reported
  on `UAbyssAudioSystem::OnCuePlayed(FName CueId)` (e.g. `bell_toll` / `bell_toll_far`; bind it for the bell frame's
  ripple).
* **Encounter stingers**: `EvSfx{monster_aggro}` of a hunt (first per instance), `EvMiniBossDialogue{opened = false}`,
  `EvSfx{monster_death}` (the core's `EvEntityDespawned{Died}` only comes at the respawn), through
  `EncounterStingerComponent` with the stinger's duck (it joins the story ducks: the deepest wins).
* `abyss.Audio.Status` lists the region, siege, hold, arbitration source, bed scale, region / layer weights and the
  emitters.

## Dark remake UI: quest card and text fit (1C U5, Wave 1 parts)

The quest card's decisions live in `Private/UI/Panels/AbyssQuestCardRules.h` and the text fitting of painted / fixed-box
text in `Private/UI/Core/AbyssTextFitRules.h` (UE-free, header-only; `unreal/PresentationTests/tests/questcard_test.cpp`
on the real Chapter 1 quests and the core's reward generator, `textfit_test.cpp`). DECISIONS DR14.

* **Progress lines** (K1 `QuestCardState::progress`): `BuildCardModel` turns the card into the page's entry plus the
  giver's lines (entry quests and duplicates dropped). A card with lines only (the core opens it for an NPC whose active
  quest has a `progressKey`, e.g. the Captain during *The Sleepless Marshal*) is a progress-only card: quest name, the
  line in quotes in the speech style, View Story when the NPC has a tree, and Leave (`ui.dialogue.leave`; it used to
  render nothing while the core held its modal). With an entry the lines follow it under an *Active* rule
  (`ui.questLog.tab.active`).
* **Labelled choices** (K1 `QuestCardEntry::choiceLabels`): the label is the primary caption (11 / 13 px, bold when
  selected) and the slot name (`sys.questCard.choice.<slot>`) the secondary one, in columns that share the card's 380 px
  (`ChoiceColumnWidth`, ≤ 150 px) and wrap. The slot of each generated item is recovered from the labels / item bases
  when a slot yielded nothing (`ChoiceSlotIndices`), so caption and item never disagree; an unlabelled card keeps today's
  layout. The accept card's preview line reads `label · slot` per slot.
* **Text fit** (every Chapter 1 string measured against these in `Scripts/tests/test_ui_text_fit.py`, zh-CN + en,
  desktop + touch): centre banners (quest complete, discovery, coming soon) wrap at 720 px and stack their lines
  (`SAbyssNotices::ShowBanner` wraps once, `PaintBanners` draws); `SAbyssPanelFrame` shrinks a title wider than its
  header band (panel width - 188 px) down to 13 px; the puzzle's answer button grows from 200 px to the content width,
  then shrinks its font, and stacks above Leave when the two no longer fit side by side; the quest tracker wraps its
  title and objectives inside the frame's 9 px padding. Toasts, the quest log rows, the combat log and the card
  captions fit as they are (pinned by the test).

## Dark remake set pieces (1C U4)

The set-piece layer places the authored prop groups of `Data/set_pieces.json` (`unreal/GameData/setPieces.ts`, DECISIONS
DR15) when a zone is built. Placements are the core's (`abyss/world/SetPieces.h`: anchor on the map, offset, ring,
facing -> tile position + actor yaw; CoreTests `setpieces_test` keeps every piece on the map, off rock / water /
blocking decorations and, unless it names a `walkOver` reason, off the bot's route and every interaction point). The
decisions on top live in `Private/World/AbyssSetPieceRules.h` (UE-free; `PresentationTests/tests/setpieces_test.cpp` on
the real tables and art manifest); `Private/World/AbyssSetPieces.cpp` only converts.

* **Placement** (`AAbyssZoneActor::BuildSetPieces`, after the walls, before the decorations): each placement's id chain
  (`gameId`, `fallbackIds`; `<id>@<theme>` first) through `PlaceArt` at its tile and yaw, the variant by its fixed index
  or the tile hash (salt 37), `partners` (the bridge's ditch bed, the circles' poured ash) at the same spot; tall pieces
  (`occluder`) are R1 occluders (bounds ellipsoid, decor fade). Nothing collides and nothing changes walkability; the
  zone decorations that are not blocking are left out on the tiles a piece covers (no grass through a cottage floor or
  the dais). One log line per zone (`Zone emerald_plains set pieces: 36 of 36 placements drawn ...`); unresolved art or
  missing sockets: one warning, the piece (or the spot) is skipped.
* **Spots** (socket `locCm` turned by the yaw): `effects` join the zone loops (the sealed Pit mouth's smoke and the burned
  bridge's smoulder: `camp.torch@march`, played as named), `emitters` go to `UAbyssAudioSystem::SetZoneEmitterSpots`
  right after the build (`ashwheat_knock` from the three barred doors' `knock` sockets), `cues` wait for the audio
  system's `OnCuePlayed`: `bell_toll` / `bell_toll_far` / `bell_dawn_toll` / `bell_self_ring` ripple the Knellwatch bell
  frame (`camp.bell_toll` at its `bell` socket; the bell that rings by itself at the siege is the fort's), and
  `bell_self_ring` also ripples the Boneyard Gate's bell (its echo, where the player stands during the siege).
* **Echoes**: an echo a piece lists (`lore_rf_echo_02` at the burned bridge, `lore_rf_echo_03` at the dais) shows its
  `MI_AF_Echo` figures by that piece, offsets and facings turned by the piece's yaw plus `echoTurnDeg` (the dais keeps its
  crack toward the chasm at yaw 0 and turns the kneeling deserter's group 135 deg so he kneels on the lore spot's side);
  other echoes stay at their lore tile.
* **Yield**: while a defend target stands within 1.5 tiles of a `yieldsToDefendTarget` piece (the Boneyard Gate under
  *The Last Toll*'s gate, same mesh), the piece's instance is hidden (scale 0, its occluder suppressed) and the target
  prop takes the piece's facing (`AAbyssPropActor::SetFacingYaw`); the piece returns when the target is gone.
* **Event labels** (`SAbyssWorldLayer`): the treasure cache, wandering merchant and puzzle read
  `zone.event.<kind>.label.<zone>` before `zone.event.<kind>.label` (the core's RevZoneKey rule; emerald_plains:
  圣骨贩子 / The Relic Peddler, 五口小钟 / Five Handbells, 阵亡骑士的铁箱 / A Dead Knight's Strongbox); in a zone with a
  presentation theme only, the rescue's captive is labelled by `sys.event.rescue.<zone>` (被缚的朝圣者 / The Bound
  Pilgrim), else `zone.event.rescue.fallbackName`. Chapters 2-5 keep today's unlabelled rescue prop (setpieces_test pins
  every unthemed zone to the generic labels and no rescue label).

## In-game autotest (`-abyssautotest`)

`UAbyssAutoTestSubsystem` (`Private/Framework/AbyssAutoTestSubsystem.{h,cpp}`, switches in `AbyssAutoTest.h`) plays
the game unattended and reports. It is platform-neutral (macOS, Windows, Linux) and works with a real RHI (also
off-screen with `-RenderOffScreen`) and with `-nullrhi` (no screenshots, everything else). Without the switch the
subsystem is not created at all (no cost); Shipping builds and the editor (PIE) never run it.

| Switch | Default | Meaning |
|---|---|---|
| `-abyssautotest[=smoke\|chapter]` | `smoke` | `smoke`: prologue, camp NPCs (shop, stash, the elder's dialogue tree and quest card), gathering, the slime hunt, M1 turned in with its cutscene. `chapter`: all of chapter 1 to the finale cutscene |
| `-abyssclass=warrior\|mage\|rogue` | `warrior` | hero class |
| `-abyssautotestdir=<dir>` | `<Project>/Saved/AutoTest` | output directory (relative = the launch directory); see *Isolated saves* below for what it refuses |
| `-abyssautotestmaxsec=<s>` | 600 smoke, 2400 chapter | hard wall-clock limit from game-instance start: past it the run writes `report.json` (`result: timeout`) and exits 2; a watchdog thread force-exits 2 (and writes `watchdog.txt`) if the game thread is stuck 120 s longer |
| `-abyssbuildsha=<sha>` | — | recorded in `report.json` |
| `-abyssseed=<n>` | drawn | seed of the run's new game, 1 to 2^63-1; the seed played is always logged (`AutoTest: ... seed`, `New game: ... seed`) and recorded in `report.json` (`seed`) |
| `-abyssautotestshotsec=<s>` | 60 | periodic screenshot while in game (0 = off) |
| `-abyssautotestignore=<LogA,LogB>` | — | Error lines of these log categories are recorded but do not fail the run (Ensures always fail) |

What a run does:

1. **Isolated saves**: `FAbyssSaveStorage` uses `<dir>/SaveGames` (slots and `settings.json`), so the player's saves
   are never read or written. A directory whose `SaveGames/` would be, or contain, the player's
   `<Project>/Saved/SaveGames` (e.g. `-abyssautotestdir=<Project>/Saved`) is refused: the run fails before it plays and
   keeps its saves under `Saved/AutoTest/RefusedRun/`. A run writes `.abyssautotest` into its directory; only a
   directory carrying that marker is cleaned at the next start (`SaveGames/`, `screenshots/`, `report.json`,
   `watchdog.txt`). A directory with a `SaveGames/` or `screenshots/` but no marker is refused, never emptied.
2. **Main menu** (screenshot), then `UAbyssGameInstance::StartNewGame(class, slot 0, Normal)` — the call the main menu's
   class card makes.
3. **The bot**: `abyss::AutoPilot` (`Source/AbyssCore/Public/abyss/tools/AutoPilot.h`, the same bot the core's
   `CoreTests/tests/playthrough_test.cpp` plays chapter 1 with) runs one step per frame from the router's `OnFrame`
   (`EndFrame` → `Advance` → `PrepareFrame`; events through `OnAnyEvent`). Its commands go through
   `AAbyssPlayerController::SubmitCommand`, the input layer's submission path, so the whole presentation runs. Story
   beats are skipped (the first beat of each kind first plays ~2–3 s for its picture).
4. **Screenshots with UI** (`FScreenshotRequest::RequestScreenshot(..., bShowUI = true)`, one at a time, into
   `<dir>/screenshots/NNN_<tag>.png`): `main_menu`; `camp` on arrival, then every HUD panel opened and closed through the
   input layer (`UAbyssInputSubsystem::PressAction`: `panel_inventory`, `panel_character`, `panel_skills`,
   `panel_quest_log`, `panel_world_map`, `panel_settings`); `story_<kind>` (prologue, chapter card, cutscene, boss
   intro); the first `shop`, `quest_card` and `dialogue` (the bot waits while they are photographed); `combat` (hero
   engaged in melee range) and `boss_fight`; `loot` after the first kill that drops something; `skill_<n>` after the
   first cast of each hotbar slot; `periodic_NN`; `end` (or `failure`). Under `-nullrhi` (`FApp::CanEverRender()`
   false) the same points are listed in `screenshotsSkipped`.
5. **Statistics**: wall-clock frame times while in game (avg / p50 / p95 / p99 / max, hitches > 100 ms), Error / Warning /
   Ensure lines through an `FOutputDevice` on `GLog` (lines logged before the game instance started are not seen).
6. **Exit**: `FPlatformMisc::RequestExitWithStatus` with **0** when the scenario completed and the log has no Error and no
   Ensure line, else **2**. Always check `report.json` too (`passed`, `result`).

`report.json`: `passed`, `result` (`passed` / `failed` / `timeout` / `aborted`), `exitCode`, `failure`, `scenario`,
`class`, `seed` (a string; replay it with `-abyssseed=<seed>`, or in the core with
`ABYSS_PT_SWEEP=1 ABYSS_PT_SWEEP_BASE=<seed> CoreTests/run.sh` — same RNG streams, though UE's frame pacing makes the
bot's timing differ), `buildSha`, `rhi` (`GDynamicRHI->GetName()`: `Vulkan`, `Null`, ...), `screenshotsEnabled`, `renderOffScreen`,
`nullRhi`, `engineVersion`, `platform`, `commandLine`, `durations` (`totalSec`, `bootSec`, `menuToGameSec`, `inGameSec`,
`simPlayTimeSec`, `simNowSec`), `scenarioCompleted`, `pilot` (bot ok / failure / last checkpoint / step / frames / commands
/ last notes), `stats` (kills, deaths, level-ups, casts, potions, loot, ...), `hero`, `milestonesReached` and
`milestones` (main menu, session, zone entered, first kill, boss kills, quests accepted / completed / turned in, story
beats, level-ups, pet, checkpoints, panel tour, scenario done — with real and sim seconds), `frames`, `log` (counts,
first 50 Error / Ensure `messages`, first 50 `warningMessages`), `screenshots` (`file`, `tag`, `caption`, `realSec`,
`simSec`, `captured`, `exists`, `bytes`), `screenshotsSkipped`. The bot's diary is in the log (`LogAbyssAutoTest: bot
[...]`), milestones and screenshots as `AutoTest ...` lines.

macOS (uncooked project, editor binary; the content build must have run):

```bash
UE="/Users/Shared/Epic Games/UE_5.8"; PROJ="$PWD/unreal/Abyssfire.uproject"; OUT="$PWD/out/autotest/warrior"
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" -game -windowed -ResX=1280 -ResY=720 \
  -unattended -nosplash -nosound -log -stdout -FullStdOutLogOutput \
  -abyssautotest=smoke -abyssclass=warrior -abyssautotestdir="$OUT" -abyssautotestmaxsec=600 \
  -abyssbuildsha="$(git rev-parse HEAD)"
echo "exit $?"   # 0 passed, 2 failed / timed out: see $OUT/report.json and $OUT/screenshots/

# no RHI (logic and log checks only, no screenshots)
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" -game -nullrhi -unattended -nosplash \
  -nosound -log -stdout -FullStdOutLogOutput -abyssautotest=chapter -abyssclass=mage -abyssautotestdir="$OUT" \
  -abyssbuildsha="$(git rev-parse HEAD)"
```

A packaged Development build takes the same switches (`Abyssfire.app/Contents/MacOS/Abyssfire ...`, without the
project path and `-game`); Windows / Linux editor binaries take them the same way. Pacing: the bot counts frames
(its timers assume 60 per second), so the autotest caps `t.MaxFPS` at 60
(an uncapped `-nullrhi` run would shrink every bot timeout). At lower frame rates the core still runs up to 4 sim steps
per frame and the bot just reacts a little later in sim time (the core tests play the smoke scenario at 20 and 30 fps
too); add UE's `-UseFixedTimeStep -FPS=60` to get exactly one 60 Hz step per frame like the core test (game time is
then no longer tied to the wall clock: with `-nullrhi` a run can finish faster than real time, with a slow software
renderer it takes longer). `report.json` records `maxFps` and `fixedTimeStep`.

## File ownership for the next agents

| Agent | Owns | Must not edit |
|---|---|---|
| **world** | `Public|Private/World/`, `Actors/`, `Anim/`, `Vfx/`, `Camera/` — implements `IAbyssWorldView` (world builder, terrain, props, lights, post-process, quality-tier application) and `IAbyssPresenter` (character actors, `UAbyssAnimInstance`), VFX from router events, the camera rig (view target, follow, zoom, shake from `EvCameraShake` honouring `bCameraShake`) | `Framework/`, `Platform/` (request changes) |
| **input** | `Public|Private/Input/` and the "input agent region" of `Framework/AbyssPlayerController.h` + all of `Private/Input/AbyssPlayerControllerInput.cpp` (runtime Enhanced Input, KBM / gamepad / touch, picking, virtual-control injection, `IA_Back -> IAbyssUiRoot::HandleBack`) | the rest of `Framework/` |
| **ui** | `Public|Private/UI/` — implements `IAbyssUiRoot` (menus, HUD, panels, story overlay, touch controls, settings panel, error screen), Slate style with the bundled fonts, world-anchored widgets | `Framework/`, `Platform/` |
| **audio** | `Public|Private/Audio/` and `unreal/Audio/` — music state machine from `EvMusic`, SFX from `EvSfx` (A4 panning), volumes from `FAbyssUserSettings`, menu music on `MainMenu`; the dark remake's voices, music director, ambience layers / emitters and cue triggers (`Private/Audio/Abyss*Rules.h`, *Dark remake audio*) | `Framework/` |
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
