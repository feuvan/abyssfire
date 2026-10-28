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

## Dark remake telegraphs, hazards, arenas (2G U2)

The Wave 2 core (implementation-plan.md 2A-2E) tells every enemy ability ahead of its contact. The decisions of this
layer live in `Private/Vfx/AbyssTelegraphRules.h` (plus the play-rate / death-runtime part of
`Private/Actors/AbyssAnimActionRules.h` and `AbyssMusicRules::PhaseStinger`; UE-free, header-only), tested by
`PresentationTests/tests/telegraph_test.cpp`, `anim_test.cpp`, `audio_music_test.cpp` on the real `Data/`, the art
manifest's `materials.M_AF_Telegraph` contract and `vfx_recipes.json`. `UAbyssTelegraphSystem`
(`Public|Private/Vfx/AbyssTelegraphs.*`, a world subsystem ticked by `UAbyssWorldBuilder::SyncFrame` after the
characters and before `UAbyssVfxSystem::Tick`) only converts. DECISIONS DR5 (the hazard language), DR16.

* **Strike decals** (`EvTelegraph`, a late view from `Snapshot::telegraphs`): one pooled `SM_FX_Quad` per decal with
  `M_AF_Telegraph` (`Scripts/abyss_content`, unlit translucent, sort priority -1: over blob shadows / prop pools /
  water, under the VFX; 8 cm over the ground), yawed along the shape and sized to it; the material measures each pixel
  against the core's shape (circle, ring, cone, line = the core's rectangle) from five CPD groups (WorldContract 3.7).
  Look: the crimson rim #D11F2E (rim strip `T_FX_TelegraphRim`), the bone fill #FFE8DC sweeping from the caster to the
  rim on the **sim clock** (`Frame.RenderSimMs`: full on the contact frame, frozen in a cinematic) with a #FFF4EE front,
  the fill grain `T_FX_TelegraphFill`; elite rim 30 cm + inner rim, boss 36 cm + inner rim, fill .27; the urgency pulse
  over the last 30 % (boss 50 %); at the contact a 110 ms flash (rim #FF3A48, fill .35) then a 170 ms fade, and the
  recipe `telegraph.contact` (`.cone`, `.line`). A cancelled windup (`EvMonsterAttackCancelled`, or a view gone before
  its contact) fades where it stood; so does one aborted in the frame that reached its contact (a stun in the contact
  step aborts inside the core's contact timer: `AbyssTelegraphRules::CancelTimeFor`, no flash, no contact dust). Every ground strike draws (Keen's slow ring too); raise / summon windups draw no
  decal but the breach-violet `monster.raise.mark` recipe at the windup's origin (stopped on a cancel).
* **On-death fuses** (`EvTelegraph` source OnDeath): presented only in a zone with a presentation theme (Chapter 1's
  march; `AbyssTelegraphRules::PresentsOnDeath`). There a decal only when the effect damages (the Cinderbloat's
  2.2-tile burst, the Scorched elite's burst; Gravecold's slow ring has none, DR5-slow); a fuse from an elite affix
  plays `elite.<type>.death` from the kill. Chapters 2-5 and the Labyrinth keep today's look: their global elite fuses
  (2B) and the Labyrinth's `curse.volatile` burst go off with neither decal nor recipe. The Cinderbloat's Death is its fuse (the art manifest's Death clip `runtime`
  block): it plays at rate 1 for 900 ms, standing (no throw / hop / spin), not faded before 900 ms, so `FX_Burst` lands
  on the core's 800 ms burst.
* **Hazards** (`EvHazard`, `Snapshot::hazards`): the hazard look (no fill, rim .75 pulsing on a 1.2 s cycle) until
  `untilMs`, then the fade; the loop `hazard.<abilityId>`, else `hazard.breach_seep` (a line) / `hazard.breach_fire`,
  stopped at `untilMs`.
* **Arenas** (`EvArena`, `Snapshot::arenas`): Valdras's P3 soft enrage (ringShrink 9 -> 5 tiles over 40 s, fire outside)
  as a rim just outside the circle and an outer crimson wash (250 cm band, fading over its last 100 cm), closing with
  `abyss::ArenaRadiusAt`; the `hazard.banner_ring` loop follows the same radius (`UAbyssVfxSystem::SetHandleRadius`).
  Gone on `cleared`, a newer arena of the same monster, the zone change.
* **Budget** (`ApplyQuality`): desktop 24 / 32 / 32 decals (low / balanced / high), mobile 16 / 24 / 32; the arena
  first, then boss > elite > normal, nearest the hero first (Chapter 1 never fills it). Mobile low drops the arena's
  outer wash: that trims the quad from 23.1 m to 18.6 m across at 9 tiles (about 35 % less area), but one translucent
  quad still spans the whole disc; its blank inside (and every decal's blank corners, a ring's hole) exits
  `AF_Telegraph` before any texture sample, so it costs rasterisation and blending, not shading. Budget the arena as an
  18.6 m square of translucency on mobile low; an annulus mesh waits for the device profile (*Mac playtest script* 5.3). One hidden quad is registered as soon as the quad and material load (M_AF_Telegraph's pipeline states
  precache before the first strike).
* **Dash** (`EvEntityTeleported{Dash}`: Valdras's Lunge, `hasDash`): the actor slides from where it stood to `dashTo`
  over 200 ms on the dodge's offset ease (`AbyssWorldUtil::EaseOutQuad`), no blink effect.
* **Ability play rates** (`AbyssAnimActionRules::ActionPlayRate`): an ability clip plays so that its authored beat
  (Contact, else Release) lands on the core's contact, clamped to .25..4; a looping clip the windup outlasts (the
  drummer's 2000 ms `war_drum` on the 1000 ms Cast01 loop) loops at rate 1 with a beat on the contact.
* **Phase stingers** (`EvMonsterPhase`): the event's `stinger`, else `bosses.<id>.phaseStingers` in the music director
  (Valdras: `stinger_valdras_roll_call` at P2, `stinger_valdras_no_retreat` at P3) through the encounter stinger's
  component and duck; nothing on a `reset`.
* **Fallbacks**: without `M_AF_Telegraph` / `SM_FX_Quad` (an older content build) the decals are skipped with one
  warning (`Telegraph decals disabled: ... missing`), the recipes and the wind-up tint still play; a missing recipe id
  plays nothing. Chapters 2-5 have no ability definitions and no presentation theme: they (and the Labyrinth) look and
  sound exactly as before.
* **Debug**: `abyss.DebugLeash 1` draws every living monster's M1 leash (`MonsterView::leashCentre` / `leashRange`,
  within 24 tiles of the hero) as a rim-only ring, last in the budget.

**On the Mac**: *Mac playtest script* below (1.1 the master; P6.2, P8.3, P9, P10.2, P11.1, P14 the fights; 4.2
Chapters 2-5; 5.1 and 5.3 the darkest tile and the arena on the devices).

## Dark remake corpses and monster states (2G U3)

The Wave 2 core's corpse registry (K3), phases (K4), packs, morale, blocks and buffs (K5) shown on screen. The decisions
live in `Private/Actors/AbyssCorpseRules.h` (the ash contract, the corpse block, the timeline, removals, the story hold,
reconcile, the cap, the burn colour) and `Private/Actors/AbyssMonsterLookRules.h` (flight and shield facing, held
items, phase looks, auras, pulses, block / flight cues, the elite sigil, the champion glyph, the death recipe chain,
the art warm-up) - UE-free, header-only, tested by `PresentationTests/tests/corpse_test.cpp` and
`monsterlook_test.cpp` (and `theme_test.cpp` for the sigil chain) on the real `Data/` (`monsters.json`, `maps.json`,
`assets.json`), `vfx_recipes.json` and the audio manifest. `UAbyssCorpseLayer` (`Public/Actors/AbyssCorpses.h`,
`Private/Actors/AbyssCorpses.cpp`, a world subsystem ticked by `UAbyssWorldBuilder::SyncFrame` after the characters
and before the telegraphs / VFX) only converts. DECISIONS DR6 (ash mode), DR5, DR16.

* **Ash husks**: a monster whose definition leaves a registry entry (`corpse.leaves` corpse / remains) and whose art
  carries a drawable `corpse` block holds its Death pose (`AAbyssCharacterActor` corpse-holding death: the Death clip
  at rate 1 to `holdFromMs`, standing where it fell: no throw, hop, spin or fade; the elite marker and affix halo go).
  Once the held pose has been drawn three frames on screen the body's component tick stops (no pose evaluation or bone
  refresh for a lying husk); a reset, a rise or the fade fallback resumes it. The block's geometry (centre, radius,
  top) is authored at scale 1 and scaled by the body's drawn scale (`GetBodyScale`: a hunt leader is x1.25).
  On `EvCorpseAdded` the world builder hands the actor over (`ReleaseCorpseActor`) and the layer runs the timeline on
  the **sim clock** (`Frame.RenderSimMs`): hold 450 ms, grey 380 ms (every toon / ink slot of the body and its held
  items on the twin masters `M_AF_Toon_Ash` / `M_AF_Outline_Ash`, a dynamic instance per slot with the slot's own
  parameters; scalars `Grey`, `Dissolve`, `HuskTopCm`; ash #6E6A62 lit / #3E3B37 shadow, emissive markers out),
  crumble by plates to .38 over 900 ms with `corpse.crumble` at the husk centre, then a husk until the core removes the
  entry (20 s). The blob shadow follows the pelvis and thins to .35 with the crumble. The removal picks the end:
  `expired` dissolves to nothing over 700 ms with `corpse.expire` (the ash blows along world +X); `raised` goes at once
  (`monster.raise` at the spot; the risen monster starts with the husk's grey / dissolve and reverses it over the
  first 60 % of its Rise); `consumed` crumbles away in 350 ms; `burned` flares `corpse.burn` in the colour of the
  fire's side, named by the core (`EvCorpseRemoved::burner`, 2G review: the hero for its skills, the monster whose
  strike, hazard or on-death burst it was, the killer of a fire killing blow): the hero's or its beast's fire is ember
  #FF7A2A (or the impact colour of the fire kill that made this very body), an enemy's breach violet #B02FD0 whether
  or not it hit anyone, no source the recipe's grey; then it greys, crumbles and blows away.
* **Breach-blood**: a breach body (the Breachclot, the Cinderbloat: corpse block `look: puddle`, no registry entry)
  leaves its Death clip's tar puddle (`puddleDecal`: #2A0E14 at .7 / the Cinderbloat's scorched #1A0A0A at .6, radius
  1.1 / 1.2 x the body's half-width W, the art's `footprintRadiusCm` - the convention of its `radiusFactorOfWidth`
  and the blob shadow's -, x the drawn scale: 48 cm / 71 cm) at the death: the shipped `Puddle` decal sprite through a
  runtime recipe
  (`AbyssCorpseRules::PuddleRecipeJson`, `UAbyssVfxSystem::PlayRuntimeRecipe`), fading in over the Death clip and drying
  in 3 s; the body itself dies as today. The Wave 1 slime's puddle decal has no corpse block: it is not drawn.
* **Seeds and late views**: map `corpseSeeds` (and registry entries the events never announced) are spawned as posed
  husks (`SpawnHuskActor` + `PoseAsHusk`: the Death clip's hold frame, frozen; grey, crumbled), at a yaw spread by the
  entry id, on the ground once the zone is built. Entries gone from the snapshot without an event dissolve like an
  expiry.
* **Story hold**: the Corpseherd's body during `cs_rf_herd` and Valdras's during `cs_rf_marshal_falls` keep their death
  pose (no grey) while the beat plays or the director is busy before it ends; the hold counts from the beat's end.
* **Cap** (`ApplyQuality`): 32 husks on desktop; mobile 12 / 20 / 32 (low / balanced / high). Overflow retires the
  oldest early (seeds and story-held bodies kept first) with `corpse.expire`.
* **Gnawers and the Feed**: the gnawer's Consume channel (`EvPlayAnim{clip Consume}`) plays `corpse.consume` once from
  its jaws toward the husk (stopped when the channel is cut short: `EvMonsterAttackCancelled`, a stun, the body
  burned); the eaten body crumbles away. A boss Feed (`EvMonsterFeed`) drains the held body toward .85 dissolve by its
  due time and plays `monster.<defId>.feed` from the body to the feeder (Valdras: `monster.valdras.feed`).
* **Monster states**: a fleeing monster (`MonsterState::Flee`) walks / runs and faces its heading; a blocker
  (`blockArcDeg` > 0) keeps facing its heading while it attacks (the shield wall keeps one front). `EvBlock` sparks
  `monster.block` across the blow at the blocker's chest (its point on the tower shield's `face` socket, manifest
  `fx.block`) and the audio system plays the clang. The aura loop (`monster.<defId>.aura`, else the art's: the
  roll-call bearer shows the standard-bearer's) runs while `auraRadius` > 0 at the aura's radius (no decal); the
  Unburied affix loops `elite.unburied`; `EvMonsterPhase` starts the phase look: the zealot's frenzy from his phase 1
  - the surge `notify.FX_Frenzy` at the phase entry (his row has no enter action, 2F-D8: no Roar plays; a row that
  entered with the Roar would leave it to the clip's own `FX_Frenzy` notify) and the breath loop -, Valdras's burning
  standard from his third phase (`monster.valdras.standard_fire`); a reset or the death drops it. Loops in the body's
  frame turn with it (`UAbyssVfxSystem::SetHandleBlow`, pushed only once the facing moved more than 2 degrees). The
  Cinderbloat's vent sheds `monster.ash_flakes` while it lives (manifest `fx.ashFlakes`). A buff / call
  (`EvMonsterBuffCast`) pulses `monster.<defId>.<abilityId>`, else - when the clip the ability's definition casts with
  carries no FX notifies of its own (the drummer's Cast01 beats `FX_DrumBeat_1..4`, so nothing doubles them; the
  Hungerbell's supper bell has no clip, so a Clapper Slam playing at the time no longer hides it) - the bell's violet
  ring (`notify.FX_bell_ring`) for the breach-touched (Unburied, breach) and the drum's pale dust
  (`notify.FX_DrumBeat_1`) for the Living and the Gravegnawers (beasts carry no violet). The drummer's own aura has no
  look yet (no `monster.deserter_drummer.aura` recipe: an art follow-up, dust and no light).
* **Markers**: in a zone with a presentation theme an elite or mini-boss carries the Wave 2 sigil (`elite_sigil`,
  `SM_FX_EliteSigil`) on `fx_elite` (else `fx_overhead`), at one world size on every body (absolute scale), floating
  6 cm over the socket with a symmetric 3 cm bob over 1600 ms and a 45 deg/s turn; art with `eliteMarker: "none"`
  (Valdras) shows none; elsewhere (chapters 2-5, the Labyrinth) today's crown. A living pack leader (a mix's leader, the
  champion ambush's Muster) always shows its name with a crimson champion diamond (#C0283A on a dark keyline) before
  it.
* **Kills**: the death recipe is `death.<defId>`, else `death.<family>` (unburied, living, gravegnawers, breach: their
  own palette, in the victim's frame), else today's `death.burst` (#FF4444 along the blow); a Raised body drops no
  coins. Monsters without a family keep the burst and the coins.
* **Held items**: every attachment of the art beyond the two hands is a component of its own on its socket (the
  Unburied's kettle helm on `head` no longer replaces the sword, crossbow, mace or standard pole); it casts, takes the
  CPD feedback, gets its own selection-rim copy (R16: the whole silhouette) and turns to ash with the body.
* **Art warm-up**: a zone transition preloads every definition the zone can show (spawn mixes, leaders, variants,
  corpse seeds, hunts, the mini-boss, what they raise / summon / spawn on death) and the art a corpse rises as.
* **Fallbacks**: without the ash twins (an older content build: one warning `Corpses: M_AF_Toon_Ash / M_AF_Outline_Ash
  missing`) no death holds and nothing is drawn here: every monster fades as before. An art asset without a corpse
  block, a missing recipe or sigil asset falls back to today's fade, nothing, or the crown. Chapters 2-5 have no
  corpse-leaving definitions, families, leaders, phases, blockers or auras and no presentation theme: they look and
  sound exactly as before.

**On the Mac**: *Mac playtest script* below (1.1 the twins; P4 the husk timeline and the warm-up; P5.1 and P9 the
puddles; P6.3, P8 and P14 the raises, feeds and burns; P10 to P12 the monster states and markers; 4.2 Chapters 2-5;
5.2 and 5.4 the devices).

## Dark remake HUD and world state (2G U5 / K6)

What the Wave 2 core's K4 boss phases and K6 world state (Ashnumb, permanent rewards, world flags, NPC lines and
visibility) look and sound like. The decisions live in `Private/UI/Hud/AbyssHudRules.h` (boss bar ticks, the status
row, the permanent-reward toast) and `Private/World/AbyssWorldStateRules.h` (dawn from the flag, the linear panel's
line, the NPCs a story beat still needs, the `AshnumbTint` contract with its band and tint fallback, the character
status tints), plus `untilFlag` in `Private/Audio/AbyssAudioDataRules.h` and the readability window in
`Private/Vfx/AbyssTelegraphRules.h` - UE-free, header-only, tested by `PresentationTests/tests/hud_test.cpp`,
`worldstate_test.cpp`, `readability_test.cpp`, `audio_ambience_test.cpp` and `audio_music_test.cpp` on the real
`Data/`, the audio manifest and `vfx_recipes.json`. DECISIONS DR16 (items 28-39).

* **Boss bar phase ticks** (`BossBarView::phase / phaseThresholdsPct`): a thin tick (and a small diamond over it) per
  threshold of the boss's phase table at that share of the bar, pale gold while ahead, dimmed to .35 once its row is
  entered; a walk home (phase 0) brightens them again. Valdras: 65 % and 30 %. Every other boss has no phase table:
  its bar is today's.
* **Ashnumb on the HUD** (`HeroView::ashnumbStacks`, status bit 6): the status row now walks every `StatusType` (the
  old `Index < 6` loop skipped it): after the web's six (unchanged colours and order) a grey #8A8480 cell, glyph 麻 /
  A (`sys.statusEffect.name.ashnumb` 麻灰 / Ashnumb), its stack count in the corner. Keen (the Corpseherd's status-only
  ring) shows as the plain Slow cell, with no number.
* **Ashnumb on the hero** (`vfx_recipes.json dark.materials.AshnumbTint`, art review r1): while the hero carries stacks
  its toon slots move to `M_AF_Toon_Ashnumb` (`Scripts/abyss_content`: `M_AF_Toon`'s graph plus `AF_AshnumbBand`
  after the split) as dynamic instances copying the slot's parameters: an ash band from the feet up to
  min(.12 + .10 x stacks, .62) of the hero's height (`BandTop`; the art's `heightCm` as `BandHeightCm`, measured on the
  pre-skinned object-space z, handed to the pixel shader through a `VertexInterpolator`, so the band stays on the legs
  in every pose): 1 stack mid-shin, 3 the thighs, 5 the hips;
  #8A8480 in the toon's lit bands, #4C4944 in its shade band and in cast shadow, at strength .85 with a .06 soft top,
  and a #B8B2A6 line .04 tall on the top edge. The band sits under the toon's feedback (2G review H-m1): the ash targets
  take the same hit flash / pain / wind-up / status tint as the rest of the body, so a hit flashes the banded legs white
  and Slow or Burn tint the ash. The ink hull and the held items keep their materials; no Slow look at 5 stacks. The antidote (or the stacks running out) puts the slots back at once. Mobile low, or a content build without
  the twin (one warning: `M_AF_Toon_Ashnumb not found`), shows the contract's StatusTint path instead: a #8A8480
  multiply at .25 + .05 x stacks after the web's six tints (`AbyssWorldStateRules::CharacterStatusTint`, the old
  priority kept). The `status.ashnumb` loop (ash flaking off) already played.
* **Permanent rewards** (`EvPermanentReward`): a gold toast `<title>: <lines>`, the title the quest's
  `data.quest.<id>.permanent` (S1: 玛勒的苦药 / Marl's Bitter Draught) else its name, one `ui.permanentReward.*` line
  per field above 0 (最大生命 +15 / Max Life +15, 属性点 / Stat Points, 技能点 / Skill Points, 洗点券 / Respec Tokens),
  two spaces apart. New PORT_STRINGS; the CJK subsets were rebuilt for 券 and 洗.
* **Dawn** (`Snapshot::worldFlags`): `march_dawn` once a flag the chapter's closing quest sets (`rf_chapter_dawn`, M5) is
  set - the flag the core's dawn spawn variants read, so the grade warms, the ashfall gives way to dust motes and the
  trenches fill with gravegnawers together; a closing quest without flags keeps Wave 1's turn-in rule. Read every
  frame, so `EvWorldFlag` needs no handler.
* **The Gnawed Field's ringing** (`ambience.layers.gnawed_field_ringing`): its `untilFlag` replaces its `while`: the
  ringing plays inside the field until M2's turn-in sets `rf_hungerbell_silenced` (Wave 1: only while the Hungerbell
  lived), then fades out over 6 s. Music regions read flags the same way (none uses one today).
* **NPCs a beat still needs** (K6 `visibleWhen`): the core hides Marl (`plains_herbalist`) at M5's turn-in while the
  `cs_rf_finale` it queues focuses her and gives her a line. On `EvQuestUpdate{TurnedIn | Accepted}` the world builder
  expects the cutscenes that quest's `quest_turned_in` / `quest_accepted` triggers queue and the NPCs they name (focus
  targets, say speakers); an `EvEntityDespawned{Npc, Removed}` of such an NPC keeps its actor drawn where it stood, out
  of the live world (unregistered, no nameplate or rim, not presented), until that beat ends (`EvStoryBeat{Ended}`),
  the director is idle (`Snapshot::storyBusy` false: a beat seen before is never queued, so she goes at once), the zone
  changes, or after a safety time (300 s waiting to begin, 900 s in all). A focus step the core resolved without a
  target (`hasFocus` false) on such an NPC pans the camera to the kept actor over the step's length. An NPC that shows
  again meanwhile replaces its held self.
* **Quest-gated lines** (`DialogueView::lineKey`): the linear NPC panel shows the line the core chose at open, else the
  web's `dialogue[1] ?? [0]`.
* **Readability** (`AbyssTelegraphRules::ReadableWindowMs`): every Chapter 1 ground strike, hazard windup and on-death
  fuse that draws is on screen at least 400 ms before its contact, counting the frame it waits to be drawn at 30 fps.
  `readability_test.cpp` checks it through the stage T rules (2G review H-m2), not just the core's floors: each real
  ability's strike played on the core's `SimClock` at 30 / 45 / 60 fps is drawn at full windup strength
  (`StrikeState` / `PackCpd`) from its first frame to the flash (tightest 689 ms); the budget's worst scene (Valdras's
  last phase - arena, a windup and the previous strike fading, both hazards, his affix fuse, his summons - plus the two
  packs that can hold the most decals: the champion Muster's four elite fuses and a three-decal pack such as the
  Cinderbloats') needs 13 of the lowest cap's 16;
  and three AutoPilot playthroughs of Chapter 1 (warrior, mage, rogue at 30 fps) replayed through a model of
  `UAbyssTelegraphSystem::Tick` at that cap drop no decal (peak 4 at once) and read every strike and fuse at least
  700 ms ahead. The model mirrors the glue's arrangement of the rules; the glue itself compiles on the Mac.
* **Not shipped**: the gore setting (no gore art; DR6 keeps it out of zh-CN builds entirely). Chapters 2-5 have no phase
  tables, Ashnumb, permanent rewards, flags, NPC visibility or dawn mood: they look and sound as before.

**On the Mac**: *Mac playtest script* below (1.1 the band twin; P5.2 and P8.3 Ashnumb; P5.3, P6.1, P7.2, P15 and P16
the world state; P14.2 to P14.6 the boss bar; 4.3 Chapters 2-5; 5.1 and 5.3 the darkest-tile capture and the arena's
GPU time).

## Dark remake budgets, warm-up and the autotest probe (2G cross-stage review)

What the three stages share, from the cross-stage review (DECISIONS DR16 (43)-(47)):

* **Particle budget** (`Private/Vfx/AbyssVfxBudgetRules.h`, `vfxbudget_test.cpp`): the VFX system's cap (700
  desktop / 450 mobile, R20) drops any spawn past it, and Chapter 1's loops (the march's ashfall, the Rim's sparks,
  Valdras's banner ring and standing fire, hazards, auras, Ashnumb, every camp torch of the zone) filled the mobile
  cap in Valdras's last phase, so hit sparks, contact dust and death bursts went first. In a zone with a presentation
  theme (`UAbyssVfxSystem::SetPresentationTheme` from the zone build): continuous emitters stop at 75 % of the cap
  (bursts keep a quarter), a continuous emitter farther than 30 m from the camera focus spawns nothing (the zone camera
  shows at most ~17 m at the widest zoom on a 2.4:1 screen), and mobile tiers emit loops at most at the balanced
  density (interval x1.5; mobile low keeps its x2). The test builds every Chapter 1 fight scene from the data (zone
  loops, the weather where it stands, every monster loop within the cull radius, the fight's phase loop, arena,
  hazards, summons, the hero's statuses, three loot beams) and checks each fits under the ceiling on every platform and
  tier (worst: the champion ambush in the ashfall, 455 of 525 on desktop high, 303 of 337 on mobile high; Valdras's
  P3 432 on desktop high, where every zone loop would have made 651). `abyss.Vfx.Status` logs the budget. Other zones
  keep the old budget exactly.
* **Husk shadows**: on mobile a husk keeps its blob and casts no sun CSM (`AbyssCorpseRules::HuskCastsSunShadow`), so
  mobile high's 32 lying bodies stay out of the shadow depth pass; desktop as before.
* **Pipeline warm-up** (the telegraph quad's idiom for every new master): at a themed zone build
  `UAbyssCorpseLayer::WarmAshPipelines` registers one hidden copy per distinct body / held-item mesh of the zone's
  corpse-holding monsters, its toon / ink slots on `M_AF_Toon_Ash` / `M_AF_Outline_Ash`; the hero registers a hidden
  copy of its body with `M_AF_Toon_Ashnumb` on its toon slots when it is built. Their pipeline states precache behind
  the zone fade, not at the first kill's grey or the first Ashnumb stack.
* **Autotest probe** (`Private/Framework/AbyssPresenceRules.h`, `presence_test.cpp`): see *In-game autotest*.

## Mac playtest script (1C + 2G)

Everything this module cannot prove on Linux, in one ordered session: the content build and the first real compile,
the autotest runs, one pass through Chapter 1 in quest order (look and listen, grouped by place), Chapter 2
regressions, then a mobile pass on a device. It replaces the per-stage lists of 1C (implementation-plan.md, the "For
the Mac" bullets of U1 / U7 / U5 / U4 and the `mac-look` row) and 2G (T, C, H). Items are numbered so a report can
cite them (`P14.6`); "as before" means as the last accepted Mac build.

**Reaching states.** Development builds take the console commands `abyss.NewGame <class> <slot>`, `abyss.Save`,
`abyss.Continue <slot>`, `abyss.ReturnToMenu`, `abyss.Locale en|zh-CN`, `abyss.Quality 0|1|2` (low / balanced / high) and
`abyss.Debug <op> <arg> <value> <col> <row>`, whose core ops are `teleport` (e.g. `abyss.Debug teleport _ 0 15 95`),
`addExp`, `addGold`, `freeze` and `unfreeze` (no spawn, status or flag ops: states come from play). Saves hold no
fight state (no monster, windup, hazard or arena is saved), so a reload restarts a fight from a fresh zone. Keep a
copy of `Saved/SaveGames/abyssfire_slot0.json` after M5 (P15) as the "finale seen" fixture. Statuses come from the
monsters that lay them (Ashnumb: Breachclots at the Seep; Slow: the Corpseherd's Keen). Elite affixes are rolled:
re-enter the zone until the elite you need (Scorched, Gravecold, Unburied) stands.

**Thresholds.** Frame times from `stat unit` averaged over 10 s in the named scene, with no hitch over 100 ms (the
autotest's hitch measure; Insights for the frame that spikes). Desktop (the Mac, every tier): Frame <= 16.7 ms.
Device: high and balanced <= 16.7 ms, low <= 33.3 ms (the 30 fps profile). Particles from `abyss.Vfx.Status` (one
log line: live / peak / cap, the loop ceiling, loops culled, spawns dropped since the zone build): in every Chapter 1
fight the burst spawns dropped stay 0 and the loop spawns dropped stay 0 (a non-zero loop count names a scene whose
loops outgrew `vfxbudget_test`'s model: report it); Valdras's P3 at the high multiplier keeps about 430 loop
particles alive on desktop (ceiling 525) and about 290 on a high-tier device (ceiling 337).

### 1. Build (mac-content)

1. `Scripts/build_content.sh` (headless `build_content.py`): the report has 0 errors and 0 warnings and imports
   `T_FX_TelegraphRim`, `T_FX_TelegraphFill`, `T_FX_AshNoise`. Every master compiles clean (the build log and the
   material editor's stats), with no "Pre-skinned position is only available in the vertex shader" (`AF_AshMask` and
   `AF_AshnumbBand` read it through a `VertexInterpolator`):
   - `/Game/Abyssfire/Materials/FX/M_AF_Telegraph`: translucent, unlit, two-sided, the custom node `AF_Telegraph`;
     its preview is a half-filled 1 m circle.
   - `M_AF_Toon_Ash` / `M_AF_Outline_Ash`: custom nodes `AF_AshMask` and `AF_AshGrey`, scalars `Grey` / `Dissolve` /
     `HuskTopCm` / `EdgeWidth` / `NoiseTileCm`, texture `Noise` = `T_FX_AshNoise`; with `Grey` 1 and `Dissolve` .38 the
     preview is a grey body with plates missing from the top down and a pale edge.
   - `M_AF_Toon_Ashnumb`: masked, DefaultLit like `M_AF_Toon`, custom nodes `AF_ToonSplit` and `AF_AshnumbBand`, the
     Ashnumb group's `BandTop` / `BandHeightCm`.
2. Build `AbyssfireEditor` (Development). This is the 2G glue's first compile: the calls each stage asked the Mac to
   confirm are listed in implementation-plan.md *2G outcomes* (stages T, C, H and the cross-stage review).
3. Launch once and read the log for the emerald_plains build: `Zone emerald_plains set pieces: 36 of 36 placements
   drawn`, `Corpses: N ash pipeline warm-up components (M meshes) registered for the zone` with N > 0, and none of
   `Telegraph decals disabled`, `the art manifest has no materials.M_AF_Telegraph`, `M_AF_Toon_Ash / M_AF_Outline_Ash
   missing`, `M_AF_Toon_Ashnumb not found`.

### 2. Autotest (mac-autotest)

1. `-abyssautotest=smoke` and `-abyssautotest=chapter` for `-abyssclass=warrior`, `mage` and `rogue` (six runs; the
   commands under *In-game autotest*, one `-abyssautotestdir` each). Each exits 0 with `report.json` `passed`, and
   its `presentation.failure` is empty (the probe below fails a run that announced strikes, arenas, corpses or
   Ashnumb and drew none).
2. In each `chapter` report: `presentation.peakDecals` >= 1, `peakArenaDecals` >= 1, `peakHusks` >= 1, and
   `bandFrames` > 0 whenever `ashnumbFrames` >= 30; `log.warningMessages` holds none of 1.3's lines.
3. `frames`: `p95Ms` <= 16.7 on a real-RHI run (balanced / high), and `hitches` no higher than the last accepted run's;
   the screenshots `story_*`, `combat`, `boss_fight` show the march mood, not the plains.
4. One more warrior `chapter` run with `-abyssautotestshotsec=10`: its screenshots are the darkest-tile set for
   `Art/Previews/ch1_dark/` (1F).

### 3. Chapter 1 in quest order (look and listen; warrior unless named, desktop high unless named)

**P1. Main menu and settings.** The brightness slider spans 0.5 to 1.5 and moves the exposure (`log2` EV) live.

**P2. Prologue and the burn pit (15,22).**
1. The prologue's last slide tolls ten times, 2.8 s apart, and stops with the prologue; the Knellwatch lute starts on
   arrival.
2. The march mood: sun, fog and grade; the ashfall (#CFCAC0); ink and rim read on the march terrain; the crimson
   wind-up tint (x .85) on the first enemy swings. Darkest-tile review: the hero, a footman and a decal stay readable
   on the darkest march tile at brightness 1.

**P3. Knellwatch (15,15).**
1. The set pieces stand (1.3's 36); the bell frame, Name Wall, cottages, wagons and banners fade (R1) when the hero
   walks behind them; only Knellwatch flies the fort's banner.
2. The hearth emitter: full inside its radius, fading to silence with distance, panned like the world cues. Tolls
   after `cs_rf_brand` / `cs_rf_whisper` are `bell_toll` inside the fort and `bell_toll_far` outside, and ripple the
   bell frame.
3. The event labels in zh-CN and en (圣骨贩子 / The Relic Peddler, 五口小钟 / Five Handbells, 阵亡骑士的铁箱 / A Dead
   Knight's Strongbox; the rescue's 被缚的朝圣者 / The Bound Pilgrim) when those random events appear.
4. The seeded husks of the march lie grey at once on the first entry.

**P4. The Burial Trenches (25,20), M1.**
1. A Risen Footman kill: the Death clip at normal speed where it fell, a ~0.45 s hold, grey over ~0.4 s (the ink stays
   dark, no glowing eyes), a ~0.9 s crumble with ash dust (`death.unburied` on the kill); the blob shadow under the
   pelvis fading to a smudge; after ~20 s the husk blows away along +X. Its kettle helm stays on in life and turns to
   ash with the body (the sword still in hand); hovering it, the selection rim rings the helm too.
2. The first kill of the session shows no frame spike and no flicker as the body greys (`stat unit` / Insights: the
   ash twins precached at the zone build: 1.3's warm-up line).
3. As many husks as the core keeps (24 kill bodies plus the Trenches' seeds): `stat anim` falls back to the living
   bodies' cost a few frames after each husk settles; panning away and back, no husk pops to its reference pose.
4. The drones play through the fight without restarting; an encounter stinger ducks the music.

**P5. The Seep (20,45), S1 Black Blood; Marl's dressing station (50,30).**
1. A Breachclot kill leaves a dark tar puddle (about 1 m across, a little wider than the body) that spreads and dries
   in ~3 s (`death.breach`).
2. Ashnumb, on the warrior, then the mage and the rogue (`abyss.NewGame mage 1`, `abyss.Debug addExp _ 3000`,
   teleport here; `ashnumb_band.py`'s previews are the reference): the band climbs from mid-shin (1 stack) to the hips
   (5), the pale edge line marks its top (on the warrior's steel too), the lit side greys lighter than the shade side,
   and it stays on the legs while running and dodging; ink and weapons untouched. The status row shows the grey cell
   (麻 / A) with its count. At 5 stacks a hit still flashes the whole hero white. The antidote clears it at once; a
   zone change keeps it while the stacks last. The first stack shows no frame spike or flicker (the band twin is
   precached with the hero).
3. S1 turned in: the gold toast 玛勒的苦药：最大生命 +15 / Marl's Bitter Draught: Max Life +15 for ~2.4 s, one line in
   both locales. Talking to Marl afterwards: the combat log gets her `after.q_rf_black_blood` line, the panel the
   core's `lineKey` (her `dialogue.0`).

**P6. The Gnawed Field (35,40), M2; the Hungerbell.**
1. At night the ringing plays over the bed; it goes on after the Hungerbell dies and stops (6 s fade) only when M2 is
   turned in; re-entering the field afterwards stays quiet.
2. The Hungerbell's (a Bonegorger hunt, (45,35)) Clapper Slam (circle 2) on the darkest tile, every desktop tier: the
   crimson rim reads, the bone fill reaches the rim exactly as the clapper lands (Attack02's beat at 800 ms), then the
   flash and the fade; the decal never hides the hero's blob shadow or ink, no z-fighting on slopes. Its supper bell
   puffs pale dust (no violet), also when it calls during a Clapper Slam.
3. A Gravegnawer eating a husk: ash and grit at its jaws for 2 s and the husk crumbles away; stunned mid-channel the
   tearing stops at once; its kill plays `death.gravegnawers`.
4. A hunt leader's kill (the Hungerbell, Dreck, Harke: drawn x1.25): the crumble dust, the expiry and a burn play on
   the body's middle, and the dissolve eats the whole body from the top.

**P7. Side quests from Knellwatch (S3 The Tag-Thief, S4 Bounty: Harke, S5, S6 Hallowed Ground).**
1. The Tag-Thief and Oathless turn-in cards: two labelled columns, the selection highlight, the touch tap card
   (`-abysstouch`), and their accept previews; the tracker's two-line "Bounty: Harke the Oathless"; the lore card title
   of `lore_rf_01` in English (17.5 px).
2. S6 turned in: its quest name and 属性点 +3 / Stat Points +3 in one line, both locales.

**P8. Ashwheat (40,52), M3, and the Sheepfold (60,55): the Corpseherd.**
1. Ashwheat: the burned bridge's smoulder (38,48) and `lore_rf_echo_02`'s figures beside it; knocking from the three
   barred doors, attenuated and panned; the split bell (55,45) on its focus and every 35-60 s within 10 tiles; silence
   in Ashwheat and the Sheepfold; the puzzle's answer stacks above Leave in English, sits beside it in Chinese.
2. The Corpseherd's pre-fight Taunt loop, its lullaby and `stinger_miniboss`; it carries the Wave 2 sigil (the
   mini-boss marker), and its boss bar has no ticks.
3. Keen (circle 4, slow) draws its decal; on the hero it is the plain Slow cell with no number, and on Ashnumb legs
   it tints the ash blue like the rest of the body. Call the Flock shows the violet raise mark, not a decal; stunning
   the Corpseherd during a windup fades the decal or stops the mark, also when the stun lands in the windup's last
   frame (no flash).
4. Call the Flock raising a husk: the ash bursts, the footman stands up grey and the grey drains over the first ~60 %
   of its Rise. Killing a raised footman drops no coins.
5. Killing the Corpseherd collapses its flock into husks; its own body stays posed (no grey) through `cs_rf_herd` (its
   bell rings at step 2), then greys; `stinger_slain` plays.

**P9. The Weeping Ditch (85,18): Cinderbloats.**
1. A Cinderbloat sheds ash flakes from its vent while it lives.
2. Its kill: the 2.2-tile ring fills over 800 ms while it stands and swells, the burst lands on the full ring, the
   body fades only after its 900 ms clip; it leaves a scorched puddle (about 1.4 m) with its shards in it.

**P10. The Penitents' Camp (95,35) and the Broken Redoubt (75,50): zealots and rolled elites.**
1. An oath zealot at <= 50 % life: no Roar, he surges at once (the `FX_Frenzy` breath puff and a pale ring at his
   head), then the breath cloud before the hood every ~0.3 s, staying in front as he turns; gone on his death or when
   he walks home.
2. A Scorched elite's death: its decal and `elite.fire_enhanced.death`; a Gravecold elite's: the recipe only.
3. A march elite carries the sigil level over the head at the same size on a hunt leader (x1.25) and a regular, never
   dipping into the helm, turning slowly; an Unburied-affix elite shows its violet cracks under its feet.

**P11. Deserters' Hollow (80,75) and Corran's culvert (70,65).**
1. The Deserter Drummer's war drum: Cast01 loops at normal speed for the 2 s windup and beats dust four times, with
   no extra pulse; its aura shows nothing yet (the art follow-up).
2. Breaking a deserter pack (kill its leader): the leader showed the crimson champion diamond before its name; the
   deserters run (walk / run clips, facing away); `sfx_deserter_flee` is silent until rendered. Their kills play
   `death.living`.

**P12. The Dead Ranks (30,80).**
1. The shieldbearer wall: hits from the front spark white on the shield face with a clang (the port's `block` cue
   until `sfx_shieldbearer_block` is rendered); the wall keeps facing its heading while swinging.
2. The Dead Standard-Bearer's crimson aura stain (5 tiles) breathes on the ground while it lives; its Muster shows
   the violet raise mark.

**P13. Crowmarket (95,100) and the Paymaster's Wagon (108,108).** Crowmarket keeps the march kit's generic pieces
without Knellwatch's banner; the paymaster's discovery banner wraps onto two lines; a champion ambush (when it rolls)
shows its leader's diamond; the bonegorgers at the midden (95,90) Clapper Slam like P6.2.

**P14. The Breach Rim (15,95): Valdras (M4).** One fight, in this order (re-engage after each interruption).
1. Approach: the Rim's bed crossfades with the march bed over the region's edge; violet sparks, light scale .40, no
   music; `lore_rf_echo_03`'s figures by the dais (the 135 deg turn is A10's open call). The Captain's progress-only
   card while M4 runs (desktop and touch: Leave, View Story).
2. The boss intro's Roar plays through the frozen world with his named voice. P1: no sigil over him; Banner Sweep
   (120 deg cone 2.6) and Oath Brand (circle 1.8, then its 4 s pulsing hazard with `hazard.breach_fire`) with the boss
   rim width and the longer urgency pulse; his boss bar carries two ticks (65 %, 30 %).
3. P2 (65 %): the roll-call stinger; the 65 % tick dims; Breach-seep's 8-tile line decal hands over to its 1 s pulsing
   hazard (`hazard.breach_seep`) with no gap and no double rim; the roll-call bearer shows the standard-bearer's aura;
   a raised footman killed near him streams violet into the standard while its body drains (Feed).
4. P3 (30 %): the no-retreat stinger; the 30 % tick dims; the ring closes 9 -> 5 tiles with its outer wash and the
   banner-ring loop on the same radius; the standard burns violet, turning with him; the Lunge's line decal, then a
   200 ms slide (no teleport pop). `abyss.DebugLeash 1`: his ring and the Dead Ranks' pack's. With ~10 decals up:
   P3's frame time and `abyss.Vfx.Status` against the thresholds.
5. Burns in P3: his Oath Brand hazard burning a husk flares violet; his Banner Sweep missing the hero (a dodge) over
   husks flares violet, not grey; the mage's fire area over husks while the ring burns the hero flares ember.
6. One leash reset (walk him home from P3): no stinger; the ring and its banner-ring loop go; the standing fire goes;
   both boss-bar ticks brighten. Re-engage: P2 and P3 play again with their stingers.
7. Teardown mid-P3 (a strike decal, an Oath Brand hazard, the ring, the standing fire, an aura and a footman kill just
   before): `abyss.ReturnToMenu`, then `abyss.Continue 0`. The fight restarts fresh (no fight state is saved): no
   orphan decal, ring, loop, standing fire, husk or held body anywhere, and `abyss.Vfx.Status` counts only the
   zone's loops.
8. The kill: `death.valdras`; his body holds through `cs_rf_marshal_falls`; `stinger_slain`.

**P15. The Last Toll (M5): the Boneyard Gate (10,32) and the finale.**
1. The siege: the bell rings by itself at `cs_rf_siege`'s step 2 and every 20-30 s while the quest is active, rippling
   the fort's bell frame and the Boneyard Gate's bell; the gate set piece hides under the defend target (no double
   gate, same facing) and returns when the target is gone.
2. M5 turned in: during `cs_rf_finale` Marl stands where she was (no nameplate), the camera pans to her at her focus
   step and her line plays; she is gone when it ends; the dawn toll at step 3; the finale's silence, then the lute.
3. The dawn: the grade warms and dust motes replace the ashfall the moment the flag is set; the trenches respawn as
   gravegnawers; leaving and re-entering keeps the dawn. The chaplain after M5: the log his
   `after.q_rf_last_toll` line, the panel his `dialogue.1`. Copy `abyssfire_slot0.json` now (the fixture).

**P16. The finale-seen fixture.** `abyss.Continue` the copy and turn in nothing: Marl is simply absent.

### 4. Chapter 2 regressions (the east road (119,60) to the Twilight Forest)

1. Leaving the march with husks lying and loops playing: no hitch; nothing of the march stays.
2. Monsters attack and die as before: no decal, no `elite.*.death` (not even for a Scorched or Gravecold elite),
   kills burst red with coins and fade, elites wear today's crown (no sigil), no husks.
3. Status rows, boss bars (no ticks), hero materials, moods and ambience as before; `abyss.Vfx.Status` reports the full
   cap as the loop ceiling and no culling.
4. A Labyrinth floor with the volatile curse needs the Zone 6 endgame (the Abyss Rift portal; curses from floor 2),
   which no Chapter 1 save reaches and no committed fixture holds: run it from a Chapter 5 save when one exists (no
   decal or recipe for a cursed corpse's burst). Until then `telegraph_test` pins `curse.*` to neither.

### 5. Mobile pass (an iOS device, Development; `-abyssquality=low|balanced|high`)

1. Per tier: per-vertex fog, no CSM on low and balanced; Chapter 1 reads on the darkest march tile (P2.2) with every
   Chapter 1 decal: the Clapper Slam, Valdras's four, Keen, the Cinderbloat's fuse, the arena.
2. Low: Ashnumb is a grey multiply on the whole hero (no band), stronger with the stacks; the arena draws its rim only
   (no outer wash); 12 husks at most, the oldest blowing away early.
3. Valdras's P3 on low (one 18.6 m translucent quad): GPU time with the ring up within 2 ms of the same frame without
   it; build the annulus mesh only if it is over.
4. Valdras's P3 and the Trenches' husks on high: the frame-time threshold, `abyss.Vfx.Status` with no burst spawn
   dropped, husks with their blob only (no CSM: AbyssCorpseRules::HuskCastsSunShadow) up to 32.
5. Touch: the progress-only card (P14.1) and the turn-in tap card (P7.1).

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
6. **Presentation probe** (2G cross-stage review X6; `Private/Framework/AbyssPresenceRules.h`, tested by
   `PresentationTests/tests/presence_test.cpp` on a real Chapter 1 playthrough): the 2G paths degrade with a Warning
   when their content is missing, which alone would never fail a run. The run tallies what the core announced (ground
   strike telegraphs and hazards in a zone with a presentation theme, arenas, corpse entries, frames the hero carried
   Ashnumb where the tier draws the band) against what was drawn (the most decals and arena rings
   `UAbyssTelegraphSystem` drew in one frame, the most husks `UAbyssCorpseLayer` held, frames the hero's band showed).
   At every `bossKill` milestone (the Hungerbell, the Corpseherd, Valdras, ...) and at the scenario's end, something
   announced that nothing drew (30 Ashnumb frames for the band) fails the run: `failure` "presentation (at bossKill
   <id>): ...". A Chapter 1 run has announced strikes, hazards, the arena and corpses by Valdras's kill.
7. **Exit**: `FPlatformMisc::RequestExitWithStatus` with **0** when the scenario completed, the presentation probe passed
   and the log has no Error and no Ensure line, else **2**. Always check `report.json` too (`passed`, `result`).

`report.json`: `passed`, `result` (`passed` / `failed` / `timeout` / `aborted`), `exitCode`, `failure`, `scenario`,
`class`, `seed` (a string; replay it with `-abyssseed=<seed>`, or in the core with
`ABYSS_PT_SWEEP=1 ABYSS_PT_SWEEP_BASE=<seed> CoreTests/run.sh` — same RNG streams, though UE's frame pacing makes the
bot's timing differ), `buildSha`, `rhi` (`GDynamicRHI->GetName()`: `Vulkan`, `Null`, ...), `screenshotsEnabled`, `renderOffScreen`,
`nullRhi`, `engineVersion`, `platform`, `commandLine`, `durations` (`totalSec`, `bootSec`, `menuToGameSec`, `inGameSec`,
`simPlayTimeSec`, `simNowSec`), `scenarioCompleted`, `pilot` (bot ok / failure / last checkpoint / step / frames / commands
/ last notes), `stats` (kills, deaths, level-ups, casts, potions, loot, ...), `presentation` (`strikeTelegraphs`,
`hazards`, `arenas`, `corpses`, `ashnumbFrames`, `peakDecals`, `peakArenaDecals`, `peakHusks`, `bandFrames`, `failure`),
`hero`, `milestonesReached` and
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
