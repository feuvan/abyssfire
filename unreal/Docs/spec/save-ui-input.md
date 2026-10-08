# Port Spec — Game Flow, Save/Load, HUD & Panels, Input (keyboard / mouse / gamepad / touch), UiKit, i18n

Area owner: game flow + persistence + presentation shell. Web source of truth: branch `claude/unreal-rebuild`,
TypeScript under `src/`. Target: portable C++20 core (`AbyssCore`, no UE types, no exceptions/RTTI) + thin UE5
module (render / input / UMG).

This document describes **what the web game does today**, precisely enough to re-implement it without reading the
TypeScript. Web behaviour that is a bug or a 2D artefact is called out as a **QUIRK (Qn)** with a recommendation; the
default is *keep*, unless the recommendation says **FIX** (resolution tracked in §16 Open questions).

Citations are `path:line` at the time of writing. "px" always means **logical pixels of the 1280×720 layout**
(`src/config.ts:9-12`, `DPR = 1`, so every `px(n)` / `fs(n)` in the UI code is just `n`).

---

## 0. Scope, boundaries, conventions

### 0.1 What this spec owns
| Topic | Web source |
|---|---|
| Boot → menu → gameplay flow, zone transitions, return to menu | `src/main.ts`, `src/scenes/BootScene.ts`, `src/scenes/MenuScene.ts`, `src/scenes/GameplayLoader.ts`, `src/scenes/ZoneScene.ts` (flow parts) |
| `GameSession` (state that survives zone changes) | `src/game/GameSession.ts` |
| Save format, versioning, migrations, save triggers, restore order | `src/systems/SaveSystem.ts`, `src/data/types.ts:609-670`, `ZoneScene.autoSave/restoreFromSave` (`ZoneScene.ts:4251-4416`) |
| Other persisted settings (locale, audio, render quality) | `src/i18n/index.ts`, `src/systems/audio/AudioManager.ts:25-31, 394-420`, `src/rendering/RenderQuality.ts:71-84` |
| Keyboard, mouse, gamepad, hero movement from input, hold-to-move, town portal | `ZoneScene.ts:113, 615-665, 774-890, 2113-2245, 5801-5884`, `src/entities/Player.ts:184-310` |
| Touch controls and mobile page shell | `src/systems/MobileControlsSystem.ts`, `src/systems/MobileShell.ts` |
| HUD (orbs, spirit bar, exp bar, skill bar, target frame, combat log, info plate, minimap, quest tracker, loot notices, boss bar, toasts, banners) | `src/scenes/UIScene.ts` (+ `src/ui/QuestTrackerHUD.ts`) |
| Every panel's layout and interactions | `src/scenes/UIScene.ts`, `src/ui/*.ts` |
| Visual language (palette, frames, buttons, fonts) | `src/ui/UiKit.ts`, `docs/art-direction.md` |
| i18n (`t()`, locales, zh-TW conversion, key conventions, data accessors) | `src/i18n/*` |
| EventBus contract between gameplay and UI | `src/utils/EventBus.ts` |

### 0.2 Owned elsewhere (referenced, not repeated)
* Item rules, tooltip **content**, shop/forge/stash/socket **rules**, auto-loot rule → `loot-items-inventory.md` (§7-§15).
* Quest card, dialogue tree runtime, quest log content, NPC quest markers, quest guide → `quests-story-ch1.md` §5, §7.
* Story playback (StoryScene, cinematic freeze) → `quests-story-ch1.md` §8.
* Skill request/buffer, dodge, target cycling, auto-battle, death → `combat-feel.md` §6, §8, §9, §13; skill tree
  **rules** (investment gates) and hotbar loadout → `classes-stats-skills.md` §7.
* Mini-boss dialogue trigger, boss intro trigger → `monsters-ai.md`.
* Pathfinding internals (`PathfindingSystem.findPath`) — whoever owns world/maps; this spec only uses it.

### 0.3 Conventions
| Topic | Web | Port rule |
|---|---|---|
| Layout units | 1280×720 logical px; the canvas is rendered at `RENDER_SCALE` (1/1.5/2) and every camera is zoomed by it (`config.ts:14-29`). | UMG design size 1280×720; DPI scale rule "shortest side" with 720 → 1.0 (phones in landscape) so every px number here maps 1:1 to Slate units. |
| Time | `scene.time.now` ms; UI animation via tweens. | Core `nowMs`; UI animations via UMG animations / tickers with the same durations. |
| Events | `EventBus` (Phaser `EventEmitter`), string event names (`EventBus.ts:5-85`). | Core emits typed events into a queue; UE `UAbyssEventRouter` re-broadcasts as dynamic multicast delegates (contract §4, API §11). |
| Strings | All player-facing text through `t(key, params)`. | Core `Localizer::T` with identical semantics (§10). |
| Persistence | IndexedDB (Dexie) for saves; `localStorage` for settings. | JSON files in the platform save dir (§3.10); settings in a separate JSON. |

### 0.4 What Chapter 1 (zone `emerald_plains`, Lv 1–10) needs from this area
* **Flow**: boot/loading screen → main menu (Continue card, New Game, Controls/help, Language; OST + Credits
  optional) → class select (3 classes) → `emerald_plains` at `playerStart (15,22)`; Esc → (autosave) → main menu;
  difficulty selector on Continue (it only appears once a difficulty has been completed — the `demon_lord` kill in Abyss
  Rift, chapter 5 — but the code path must exist).
* **Save**: one autosave slot (web), schema v3, all fields below written even if their feature is a later milestone
  (embers, pets, homestead, abyss, mercenary → defaults). Save triggers of §3.6 that fire in Ch1: zone entry, quest
  turn-in, story beat end, soul-echo claim, return to menu (+ the recommended periodic/app-pause saves).
* **HUD**: everything in §6 except boss-bar-for-abyss, abyss run widget, pet medallion (needs a pet; Ch1 can grant
  `pet_sprite`, see quests spec OQ3). Embers text appears once `q_explore_goblin_camp` is turned in (§6.9).
* **Panels**: inventory, character, skill tree, quest log (+ lore tab), world map, shop (merchant + blacksmith with
  forge tab), socket, simple dialogue, dialogue tree, quest card, mini-boss dialogue, lore popup, audio settings,
  achievements, item tooltip + context popup + confirms. The **stash** panel is general but not reachable in Ch1 (the
  `stash` NPC first stands in the anvil_mountains / scorching_desert / abyss_rift camps and the Ember Tower,
  `src/data/maps/*.ts`). Homestead (H), ley-beasts (P), companion (U) are
  later-milestone panels (§7.17); decide whether their keys are hidden in the Ch1 build (OQ-UI-3).
* **Input**: full keyboard map (§5.1), mouse click/hold-to-move/right-click portal, gamepad (§5.5), touch controls
  (§5.7) on iOS/Android, touch adaptations of every Ch1 panel (§7.0.6).
* **i18n**: zh-CN (source), en, zh-TW (generated) — all keys the Ch1 content and UI use; locale switch from the menu.

---

## 1. Application flow

### 1.1 Boot (`src/main.ts`, `src/scenes/BootScene.ts`)
1. `initializeFontManager(getLocale())` loads Cinzel 400/700 (latin only; `src/rendering/FontManager.ts:7-38`); CJK
   text uses the system "Noto Sans SC" fallback (no bundled CJK font). Then `installTextResolution()`, `new
   Phaser.Game(config)` with scenes `[BootScene, MenuScene]`, then `installMobileShell(game)` (§5.8).
2. `BootScene.preload` (`BootScene.ts:19-73`) shows a loading screen: gradient bg `0x0a0a14→0x1a1020`, title
   `t('boot.title')` ("渊  火", 36 px Cinzel `#c0934a`, pulsing alpha 1↔0.75, 1500 ms yoyo), subtitle
   `boot.subtitle`, 15 rising embers, progress bar 300×6 (fill `#c0934a`), status text `boot.loading` →
   `boot.ready` on completion. Missing external PNGs are silently ignored.
3. `BootScene.create` generates procedural textures (sprites, skill VFX, skill icons, fog tiles) and starts
   `MenuScene`.
* **Render-only.** UE: engine startup movie → `L_MainMenu` loads async with a UMG loading widget using the same
  look. Nothing is generated at runtime (art is baked by the Blender/asset pipeline).

### 1.2 Main menu (`src/scenes/MenuScene.ts`)
`create()` (`:70-85`): screen camera; `audioManager.playTrack('menu','explore')`; build background + title; start
BGM; `checkForSaves()`; subscribe `LOCALE_CHANGED`.

**State machine** (`activePanel ∈ {menu, class, help, jukebox, credits, difficulty, lang}`, `:64`):

```
                     ┌──────────────── Back ─────────────────┐
checkForSaves() ──► MENU ──New Game──► CLASS ──pick class──► startGame(classId)
   (loadAutoSave)    │  ──Continue card──► (selector needed?) ─yes─► DIFFICULTY ──pick──► loadGame(save)
                     │                                       └─no──► loadGame(save)
                     │  ──Controls──► HELP (modal overlay, menu stays underneath)
                     │  ──Soundtrack──► JUKEBOX (modal overlay)
                     │  ──Credits──► CREDITS (modal overlay)
                     └  ──Language──► LANG ──pick──► setLocale() (stays in LANG, re-rendered) ──Back──► checkForSaves()
```
* `checkForSaves` (`:450-454`): `new SaveSystem().loadAutoSave()` → `showMainMenu(save ?? null)`. Only the
  `autosave` record is ever read (QUIRK Q3).
* `onLocaleChanged` (`:87-125`) rebuilds the title and re-renders whichever panel is active (jukebox restarts at
  track 1; difficulty only if a save is known).

**Title block** (`buildTitle`, `:362-410`; all at x = 640): gold divider w 440 at y 78; `menu.title` ("ABYSSFIRE",
never translated) 58 px Cinzel bold, vertical gradient `#fff3c4 → #f0b84e (0.45) → #c46a1c (0.7) → #7a2a0c`, stroke
`#2a1606` 6, drop shadow, breathing scale 1↔1.015 (3000 ms yoyo); `menu.subtitle` 30 px `#e8c77a` at y 190 with
title flourishes; divider w 360 (no diamond) at y 222; version text `v0.22.0` 12 px Cinzel `#6a5a48` at y 702.

**Background** (`buildBackground`, `:131-197`, render-only): vertical gradient `0x050508 → 0x1a0808`; pulsing fire
glow at bottom (alpha 0.15↔0.30 over 8 s, scale ±5 % over 10 s); ember particles (tints `ff4400/ff6600/ff8800/ffaa00`,
lifespan 3–6 s, one per 100 ms) and sparks from a 20 px strip at the bottom; 5 drifting smoke circles; title glow;
rune circle (rings r 240/226/170/158, 48 ticks, hexagram, 12 dots) at (640,150) displayed 420 px, rotating 360° per
120 s, alpha 0.09↔0.17; vignette. **UE**: a small 3D menu level (brazier + rune circle decal, Niagara embers/sparks,
post-process vignette) behind the UMG menu, or a UMG material background with the same layers.

**Main menu layout** (`showMainMenu`, `:456-558`), centred at x 640:

| Element | y (with save / without) | Size | Notes |
|---|---|---|---|
| Continue card | 296 / — | 380×86, `tooltip` frame, accent `0xffd98a` | Round portrait well r 33 (class sprite idle, masked r 31) at left+46; line 1 `t('menu.continue',{class: t('data.class.<id>.name'), level})` 17 px parchment; line 2 `"<zone name>  ·  <difficulty label>"` 13 px textSoft (`getZoneName(save.player.currentMap)`, `menu.difficulty.<d>`); line 3 `menu.continueSubtitle` 12 px italic `#e8c77a`; "▶" at right. Hover: fill `0xffc860` α 0.08, frame tint `0xfff0d0`. |
| New Game | 380 / 320 | 320×50, `primary` if no save else `secondary`, font 20 | → class select |
| Controls / Soundtrack / Credits / Language | +56, then +46 each | 280×38 `ghost` font 15 | keys `menu.help`, `menu.ost`, `menu.credits`, `menu.language` |

**Continue click** (`:518-530`): `save.completedDifficulties = deriveCompletedDifficulties(save.difficulty ??
'normal', save.completedDifficulties)`; if `shouldShowDifficultySelector(save.difficulty, completed)` → difficulty
selector, else `loadGame(save)`.
* `deriveCompletedDifficulties(d, list)` (`DifficultySystem.ts:118-133`): non-empty list → copy; else
  `nightmare → ['normal']`, `hell → ['normal','nightmare']`, else `[]`.
* `shouldShowDifficultySelector(d, list)` (`:139-146`): `d && d !== 'normal'` OR `list.length > 0`.
* `getDifficultyStates(list)` (`:154-176`): start `{normal: available, nightmare: locked, hell: locked}`; `'normal'`
  in list → normal `completed`, nightmare `available`; `'nightmare'` in list → nightmare `completed`, hell
  `available`; `'hell'` in list → hell `completed`.

**Difficulty selector** (`:1191-1307`): title `menu.difficulty.title` 22 px at y 272; one card per `DIFFICULTY_ORDER
= [normal, nightmare, hell]` at y = 340 + i·76, 360×64 `tooltip` frame, accent `normal 0x4a8c4a / nightmare 0xc0392b
/ hell 0x8b0000` (locked `0x3f3845`, frame alpha 0.6). Label 20 px: `"✓ "` prefix when completed, `"🔒 "` when locked;
colour locked `#6a635c`, current `#ffffff`, else `normal #4ade80 / nightmare #ef4444 / hell #ff4444`. Description 13 px:
locked → `menu.difficulty.locked`, else `menu.difficulty.desc.<d>`. Current (and not locked) → tag
`menu.difficulty.current` 11 px `#ffe7a0` at top-right. Click (not locked) → `save.difficulty = d; loadGame(save)`.
Back (180×38 ghost at y 576) → `showMainMenu(save)`. Changing difficulty keeps the same hero, map and position
(D2-style; Q33).

**Class select** (`:560-703`): heading `menu.classSelect.title` 20 px at y 250; three cards 250×356, gap 26, top 274,
centres x = 640 + (i−1)·276, order warrior/mage/rogue, colours `0xd0473a / 0x9b59d6 / 0x3fb86a`, accent text
`#ff8a72 / #d0a0ff / #8ff0a8`. Card content: coloured radial light + pedestal; animated class preview (idle; on hover
plays `attack` once then idle) scaled to 170 px tall; name `menu.classSelect.<id>.name` 20 px at top+222; description
`menu.classSelect.<id>.desc` 13 px wrapped to cardW−36 at top+248; up to 4 skill icons 30 px (gap 8) at top+292 (the
first 4 class skills that have an icon); `menu.classSelect.confirm` primary button (cardW−60)×34 at bottom−30.
Clicking the card body or the button → `startGame(id)`. Hover: card rises 8 px (160 ms Quad.easeOut), glow α 0.35.
Entrance: alpha 0→1 and y +24→0 over 360 ms Cubic.easeOut, delay 80·i ms. Back (160×34 ghost at y 668) →
`checkForSaves()`.

**Language selector** (`:1313-1361`): title `menu.language` at y 290; buttons 320×46 at y = 360 + i·62 for
`zh-CN, zh-TW, en` (labels `menu.langSelect.zhCN/zhTW/en`, self-named), current one `primary` + tag
`menu.difficulty.current`; click → `setLocale(id)` (§10.2). Back at y 560 → `checkForSaves()`.

**Help (Controls) modal** (`:705-802`): backdrop `0x000000` α 0.72 (click closes), panel 460×500 centred, title
`menu.helpPanel.title`; three categories (`movement`, `combat`, `ui`) listing key-caps and descriptions (exact rows:
`W / A / S / D`, `Mouse LMB`; `1 - 6`, `SPACE`, `Q`, `TAB`, `R / RMB`; `I C K J M H P U O ESC`). Close button 140×32.
The list omits `V` (achievements) — Q25.

**Soundtrack (jukebox) modal** (`:804-1065`): panel 460×530; 11 tracks (`JUKEBOX_TRACKS`, `:30-42`) with nominal
durations (s) `[120,180,150,210,150,180,150,180,150,210,180]` (total 31:00, shown in `menu.jukebox.subtitle`); rows
30 px, alternate rows darkened, active row highlighted `0xd4a54a` α 0.16; click row → play; progress bar (click to
seek: `elapsed = ratio·duration`); ⏮ = restart if `elapsed > 3` s else previous; ⏯ toggles a *temporary* music mute
(`audioManager.setMusicTempMute`, not persisted); ⏭ next; a 250 ms timer advances `elapsed`; at the end of a track
auto-advance, after the last one pause at the end. Each play calls `audioManager.playTrack(zoneId, state)`. Close →
un-mute, `EventBus.emit(ZONE_ENTERED, {mapId:'menu'})` (menu music back). Durations are display-only (QUIRK: not the
real file lengths).

**Credits modal** (`:1067-1176`): 500×600; static content (Kenney tiles, 9 BGM credits with licences — one is
**GPL 2.0**, "Desert Battle Theme" — and Phaser). **Port**: replace with the UE build's credits (Blender pipeline,
Unreal Engine notice, fonts' OFL, kept music); see OQ-FLOW-4 for the GPL track.

**Menu music unlock** (`startBGM`, `:425-447`): emits `ZONE_ENTERED {mapId:'menu'}`; a DOM `pointerdown`/`keydown`
listener resumes the WebAudio context on the first gesture. **Render-only** (UE has no autoplay restriction).

### 1.3 Entering gameplay
* New game (`startGame`, `:1363-1366`): `ensureGameplayScenes()` (lazy-loads ZoneScene+UIScene code,
  `GameplayLoader.ts:5-17`) then `scene.start('ZoneScene', {classId, mapId: 'emerald_plains'})`.
* Load (`loadGame`, `:1178-1185`): `scene.start('ZoneScene', {classId: save.classId, mapId:
  save.player.currentMap, saveData: save})`.
* **The new game silently overwrites the only save** at the end of the first zone `create` (autosave, §3.6) — Q2.

### 1.4 Zone lifecycle (`ZoneScene.init` `:337-365`, `create` `:367-704`)
`init(data)` payload (all optional except `classId`, `mapId`):
`{classId, mapId, saveData?, playerStats?, subDungeon?, parentZoneInfo?{mapId,returnCol,returnRow},
discoveredHiddenAreas?, miniBossDialogueSeen?, loreCollected?, targetCol?, targetRow?, dungeonRun?, dungeonFloor?}`.
* Unknown `mapId` (not a labyrinth floor, not a sub-dungeon) → `'emerald_plains'` (`:357`).

`create` order (flow-relevant steps):
1. Reset transient guards (`isTransitioning`, `isPortaling`, input buffer, dodge controller, gamepad edge state).
2. `if (!session) session = new GameSession()` (`:399`) — **the session is created once per ZoneScene instance and
   never reset** (Q1). `session.beginZone(mapId, levelRange, safeZoneRadius ?? 9)` → per-zone systems (§2).
3. Player created at `(targetCol, targetRow)` or `mapData.playerStart` (`:427-430`); if `playerStats` → copy level,
   exp, gold, hp, mana, stats, points, skillLevels (record or legacy entry array), spirit, **buffs**, autoCombat,
   autoLootMode (`:431-446`). `refreshSkillLoadout`, `recalcDerived`.
4. Restore mini-boss/lore sets from payload; if `saveData` pending → `restoreFromSave` (§3.5).
5. Pathfinding, spawns, NPCs, world systems, terrain, camera follow (lerp 0.08) at zoom `ZONE_CAMERA_ZOOM ×
   RENDER_SCALE`, lighting, VFX, weather, `fadeIn(400)`.
6. Input setup (§5); mobile controls if touch device.
7. `UIScene` launched (`{player, zone}`) the first time, otherwise `EventBus.emit('ui:refresh', {player, zone})`
   (`:663-667`) — **UIScene survives zone changes** (combat log, open panels persist).
8. Subscriptions; `exploredZones.add`; achievement `explore`; emit `ZONE_ENTERED {mapId}`; log `zone.enterZone
   {zoneName, min, max}` (system).
9. `storyDirector.start()` → chapter card on first visit, else `showZoneBanner()` (§6.13); **`autoSave()`**.

**Zone change** (`changeZone`, `:4187-4204`): guard `isTransitioning`; `autoSave()` (fire-and-forget, saves the
*old* zone/position); camera fade-out 400 ms (`VFXManager.zoneTransition`, `VFXManager.ts:250-256`); then
`scene.restart({classId, mapId: target, targetCol/Row, miniBossDialogueSeen, loreCollected, discoveredHiddenAreas,
playerStats: getPlayerTransitionStats()})`. Triggers: hero within `distSq < 2.25` of an exit tile
(`checkExitProximity`, `:4206-4221`, every frame) or clicking an exit (§5.3). Skill cooldowns reset on every zone
change (hero rebuilt; classes spec Q23).

**Return to menu** (`returnToMenu`, `:4223-4228`, bound to `Esc`): `await autoSave()`; stop UIScene; start
MenuScene; stop ZoneScene. No confirmation; open panels are discarded (Q10). **No life check**: pressed during
the 1100 ms death window it saves the dead hero (`hp = 0`) and the scene stop discards the pending respawn timer —
loading that save soft-locks (Q34/Q35; web behaviour and port rules in §3.4, §3.5, §5.1.1).

**Death**: see `combat-feel.md` §13.3 (respawn at `campPositions[0]` in the overworld; in a dungeon/sub-dungeon,
restart in the parent zone at its first camp with full HP/MP). The overworld respawn does **not** autosave (Q36).
What the hero can and cannot do while dead: §5.1.1.

### 1.5 World freeze gates
`ZoneScene.update` returns immediately (no input, no AI, `holdMove = null`) while
`storyDirector.cinematic || dungeonChoosing || abyssModalOpen()` (`:1346-1352`). UIScene hides its whole camera while
`zone.storyDirector.cinematic` (`UIScene.ts:5763-5765`). Mobile touch controls stay visible but inert (world taps are
ignored during cinematics, `ZoneScene.ts:775`).

### 1.6 UE mapping (recommended)
| Web | UE |
|---|---|
| `GameSession` field on the ZoneScene instance | `UAbyssSessionSubsystem : UGameInstanceSubsystem` owning `abyss::Session` (core). Created on New Game / Load, destroyed on Return-to-menu (FIX Q1). |
| `ZoneScene` restart per zone | One persistent map per zone (`L_Zone_emerald_plains`) loaded with `OpenLevel`/world partition; `AAbyssZoneGameMode` builds actors from core state on `BeginPlay`. Transition payload = `abyss::ZoneEntryRequest {mapId, targetTile?, subDungeon?, parent?, dungeonRun?}` stored in the subsystem, not in URL options. |
| `UIScene` (parallel overlay scene) | `UAbyssHUDRoot` (CommonUI `UCommonActivatableWidgetStack` layers: HUD, Menu, Modal, Tooltip, Toast) owned by the local player; survives level loads via the subsystem re-creating it in each level's PlayerController. |
| `MenuScene` | `L_MainMenu` + `WBP_MainMenu`. |
| `BootScene` | engine loading screen + async asset loading. |

---

## 2. GameSession (`src/game/GameSession.ts`)

Long-lived (survive zone changes) members, constructed once (`:26-44`):

| Member | Type | Notes |
|---|---|---|
| `inventory` | `InventorySystem` | bag, equipment, stash, buyback (loot spec §7) |
| `quests` | `QuestSystem` | `registerQuests(AllQuests)` in the constructor |
| `homestead` | `HomesteadSystem` | building levels + `tower: HomesteadTower` |
| `pets` | `PetSystem` | `setBuildingLevelSource(id → homestead.getBuildingLevel(id))`, `setAwaySource(id → homestead.tower.isPetAway(id))` |
| `achievements` | `AchievementSystem` | |
| `saves` | `SaveSystem` | stateless wrapper |
| `mercenaries` | `MercenarySystem` | |
| `story` | `StoryProgress` | set of seen beat ids |
| `soulEcho` | `SoulEchoState` | |
| `abyss` | `AbyssRecord {unlockedTier, bestTier, bestTimeMs?}` | default `{unlockedTier: 1, bestTier: 0}` |

Per-zone runtime (`beginZone(zoneId, levelRange, safeZoneRadius)`, `:46-57`): fresh `CombatSystem`, `LootSystem`,
`StatusEffectSystem`, `EliteAffixSystem`, `RandomEventSystem({zoneId, levelRange}, {safeZoneRadius})`.

**Not in the session but persisted across zones by the transition payload / ZoneScene fields** (port: move into the
core session): hero state (level, exp, gold, hp, mana, stats, free points, skill levels, spirit, buffs, autoCombat,
autoLootMode), `miniBossDialogueSeen`, `loreCollected`, `discoveredHiddenAreas`, `difficulty`,
`completedDifficulties`, `exploredZones`, `fogData`, and (in UIScene!) `dialogueTreeState` (Q26).

---

## 3. Save system

### 3.1 Storage (`SaveSystem.ts:8-19`)
IndexedDB database `AbyssfireDB`, schema version 1, one table `saves` with primary key `id` (string) and index
`timestamp`. Records are structured-cloned JS objects (`SaveData`).

API (`:171-209`):

| Method | Behaviour |
|---|---|
| `save(data)` | `data.timestamp = Date.now(); data.version = CURRENT_SAVE_VERSION (3); put(data)` (overwrite by id). |
| `load(id)` | `get(id)`; if found and `version < 3` → `migrateSaveData(data)` **and write it back** (so migration runs once). Returns the record or `undefined`. |
| `listSaves()` | all records ordered by timestamp desc — **unused** |
| `deleteSave(id)` | **unused** |
| `autoSave(data)` | `data.id = 'autosave'; save(data)` — the only writer in the game |
| `loadAutoSave()` | `load('autosave')` — the only reader (menu) |
| `quickSave(data, slot)` | `id = 'save_<slot>'` — **unused** (intent for slots, Q3) |

Errors: `ZoneScene.autoSave` wraps everything in `try/catch` and silently ignores failures (`:4294`).

### 3.2 `SaveData` schema (current version 3) — `src/data/types.ts:609-670`
Field order below = order written by `autoSave` (`ZoneScene.ts:4259-4293`). "Opt" = may be absent in older saves.

| Field | Type | Written value | Read on load (§3.5) |
|---|---|---|---|
| `id` | string | `'autosave'` | key |
| `version` | int | 3 | migration |
| `timestamp` | int (Unix ms) | `Date.now()` | menu ordering (unused) |
| `classId` | `'warrior'\|'mage'\|'rogue'` | hero class | ZoneScene `classId` (unknown → warrior, `ZoneScene.ts:427`) |
| `player.level` | int ≥ 1 | | yes |
| `player.exp` | int | exp into current level | yes |
| `player.gold` | int ≥ 0 | | yes |
| `player.hp`, `player.mana` | float | current | clamped to max after recalc (Q8) |
| `player.maxHp`, `player.maxMana` | int | derived | **ignored** (re-derived) |
| `player.stats` | `{str,dex,vit,int,spi,lck}` ints | allocated base stats | yes |
| `player.freeStatPoints`, `player.freeSkillPoints` | int | | yes |
| `player.skillLevels` | `Record<skillId, int>` (legacy: `[skillId,int][]`) | `Object.fromEntries(map)` | both shapes accepted (`:4305`) |
| `player.spirit` (opt, v3) | `{value: float, resonanceRemainingMs: float}` | | `spirit.restore` (clamps, see §3.3) |
| `player.tileCol`, `player.tileRow` | float (tile units) | hero position; **in a labyrinth run: 15, 22** | walkability check (§3.7) |
| `player.currentMap` | string | zone id; **in a labyrinth run: `'abyss_rift'`**; in a sub-dungeon: the sub-dungeon id (Q7) | `init.mapId` |
| `inventory` | `ItemInstance[]` | bag order | every item `identified = true` (Q9) |
| `equipment` | `Partial<Record<EquipSlot, ItemInstance>>` | | same |
| `stash` | `ItemInstance[]` | | same |
| `quests` | `QuestProgress[] {questId, status, objectives:[{current}]}` | `Array.from(progress.values())` | `loadProgress` (quests spec §2.9) |
| `exploration` | `Record<mapId, boolean[][]>` | ZoneScene `fogData` — **always `{}` in practice** (Q6) | restored into `fogData`, unused |
| `homestead.buildings` | `Record<buildingId, int>` | building levels | yes |
| `homestead.embers` | int | `HomesteadTower.toSave()` (`HomesteadTower.ts:214-222`) | `tower.load` (§3.3) |
| `homestead.garden` | `{progress:int, stock: Record<itemId,int>}` | | |
| `homestead.expedition` | `{petId, optionId, kills, killsRequired, remainingMs} \| null` | | |
| `homestead.blessing` | `{id, level, remainingMs} \| null` | | |
| `homestead.towerReturn` | `{mapId, col, row} \| null` | | |
| `homestead.pets`, `homestead.activePet` (legacy, opt) | old pet list | not written any more | read by `migratePetSave` |
| `pets` (opt) | `{owned: PetSaveInstance[], active: string\|null}` | `PetSystem.toSave()` | `loadSave` (§3.3) |
| `achievements` | `Record<string, number>` | unlocked achievement ids → 1, plus progress counters keyed `type` or `type:targetId` (`AchievementSystem.ts:94-110`) | keys that are achievement ids → unlocked; others → progress |
| `settings.autoCombat` | bool | hero flag | yes |
| `settings.musicVolume` | number | **constant 0.5** (Q5) | ignored |
| `settings.sfxVolume` | number | **constant 0.7** (Q5) | ignored |
| `settings.autoLootMode` | `'off'\|'all'\|'magic'\|'rare'\|'legendary'` | | yes |
| `difficulty` | `'normal'\|'nightmare'\|'hell'` | | yes (default normal) |
| `completedDifficulties` | string[] | | yes (default []) |
| `mercenary` (opt) | `{type, level, exp, hp, mana, equipment:{weapon?, armor?}, alive}` | only if hired | `loadFromSave` (hp clamped; dead → 0) |
| `dialogueState` (opt) | `Record<npcId, {visitedNodes: string[], choicesMade: Record<nodeId, choiceKey>}>` | read from UIScene | written into UIScene |
| `miniBossDialogueSeen` (opt) | string[] | | Set |
| `loreCollected` (opt) | string[] | | Set |
| `discoveredHiddenAreas` (opt) | string[] | | Set |
| `storySeen` (opt) | string[] | `StoryProgress.toSave()` | missing → `['prologue']` (veterans skip the prologue) |
| `soulEcho` (opt) | `{mapId, col, row, gold, exp} \| null` | | kept only if `col,row` finite |
| `abyss` (opt) | `{unlockedTier, bestTier, bestTimeMs?}` | copy of session record | `unlockedTier = max(1, v ?? 1)`, `bestTier = max(0, v ?? 0)` |

Nested shapes owned elsewhere: `ItemInstance` (loot spec §2.4, field names verbatim), `QuestProgress` (quests spec
§1.5), `PetSaveInstance {petId, level 1–20, exp, evolved 0–2, bond 0–5, bondProgress 0–99}` (`types.ts:543-559`),
`MercenarySaveData` (`types.ts:582-593`), `SpiritSaveState` (`types.ts:595-598`), `SoulEchoData` (`types.ts:601-607`).

### 3.3 Versioning and migrations
`CURRENT_SAVE_VERSION = 3` (`SaveSystem.ts:6`). `migrateSaveData(save)` (`:121-125`): `if (version < 2)
migrateV1toV2; if (version < 3) migrateV2toV3` — chained, idempotent, mutates in place.

**v1 → v2** (`:42-105`) — set only when missing (`!field`):
1. `inventory = []`, `equipment = {}`, `stash = []`, `quests = []`, `exploration = {}`, `achievements = {}`.
2. `homestead` missing → `{buildings: {}, pets: []}`; else fill `buildings = {}` / `pets = []`.
3. `settings` missing → `{autoCombat: false, musicVolume: 0.5, sfxVolume: 0.7, autoLootMode: 'off'}`.
4. `difficulty = 'normal'`, `completedDifficulties = []`.
5. `mercenary` present but not an object → `undefined` (absent = no mercenary).
6. `dialogueState = {}`, `miniBossDialogueSeen = []`, `loreCollected = []`, `discoveredHiddenAreas = []`.
7. Every item in inventory, stash and equipment without `sockets` → `sockets = []`.
8. `version = 2`.

**v2 → v3** (`:111-118`): `player.spirit = SpiritSystem(classId, player.spirit).toSaveState()`; `version = 3`.
`SpiritSystem.restore` (`SpiritSystem.ts:170-188`): non-object → `{0,0}`; `value` non-finite → 0, clamp
`[0, maxValue(=100)]`; `resonanceRemainingMs` non-finite → 0, clamp `[0, profile.resonanceDurationMs]` (warrior 6000,
mage 7000, rogue 5500; classes spec §14.1); if `value ≤ 0 || remaining ≤ 0` → remaining 0.

**Load-time normalisations that are not version-gated** (run on every load, §3.5):
* Quests: an `active`/`completed` progress whose objective count differs from the current definition → reset to
  `active` with all `current = 0` (`QuestSystem.ts:266-278`).
* Pets: `migratePetSave(save)` (`PetSystem.ts:435-457`): source `save.pets.owned` else legacy `homestead.pets`;
  active = `save.pets.active` if `pets` exists else legacy `homestead.activePet`; drop entries with non-string /
  unknown `petId` and duplicates; `level = clampInt(level, 1, 20, 1)`; `exp = level ≥ 20 ? 0 : clampInt(exp, 0,
  60 + 40·level − 1, 0)`; `evolved = max(evolutionForLevel(level) (= count of [10,20] ≤ level), clampInt(evolved,
  0, 2, 0))`; `bond = clampInt(bond, 0, 5, 0)`; `bondProgress = clampInt(bp, 0, 99, 0)`; active must be owned else
  `null`. `clampInt(v, lo, hi, d)` = finite number → `floor(v)` else `d`, then clamp.
* Homestead tower: `HomesteadTower.load` (`HomesteadTower.ts:225-255`): reset to defaults, then `num(v, d)` = finite
  number → `max(0, v)` else `d`; garden stock keeps entries with `n > 0`; expedition kept only if `petId` string and
  `optionId` ∈ `EXPEDITION_OPTIONS` (`killsRequired = max(1, …)`); blessing kept only if id ∈ `BLESSINGS` and
  `remainingMs > 0` (`level = max(1, …)`); `towerReturn` kept if `mapId` is a string.
* Achievements: keys equal to a known achievement id → unlocked; everything else → progress counter.
* Story: `storySeen ?? ['prologue']`. Soul echo: kept only if `col`/`row` finite. Abyss: clamps above.
* All items `identified = true`.

**Port versioning plan** (recommendation): keep the JSON field names and v1→v3 migrations byte-compatible so a web
save exported as JSON can be imported (debug tool); bump to **v4** only when a UE-only field is added (e.g.
`slot`, `playTimeMs`, `parentZone` for Q7); v3 → v4 = add defaults. A save with `version > kCurrent` must be refused
(never downgraded) with a UI message.

### 3.4 Building a save — `autoSave()` (`ZoneScene.ts:4251-4295`)
Exactly the mapping in §3.2, with one override: when `isInDungeon` (Abyss Labyrinth run) the position is the Abyss
Rift camp entrance `(ABYSS_ENTRANCE_COL 15, ABYSS_ENTRANCE_ROW 22)` on map `'abyss_rift'` (`:318-319, 4254-4257`) —
runs are ephemeral, the run state is not saved. Gold, exp, items gained in the run *are* saved. Arrays are copied
(`[...set]`); inventory/equipment/stash/buildings are the live objects (structured clone at write).

**Saving a dead hero (web behaviour, QUIRK Q34).** `autoSave` has no life check: it writes `player.hp` verbatim
(`ZoneScene.ts:4264-4266`). Every hero-damage writer clamps with `max(0, …)` (`:1206, 2998, 3031, 6112`), so a save
taken between `Player.die()` and the respawn stores `hp = 0`, `mana` as it was, the **death-spot** position
(labyrinth: still overridden to `abyss_rift (15, 22)`), the gold/exp **after** the death penalty, and the new soul echo
at the death spot (`ZoneScene.ts:1218-1245`). The death window is the `time.delayedCall(1100)` of `handlePlayerDied`
(`ZoneScene.ts:914-953`); `scene.stop` discards that timer, so after a return to menu the respawn never runs.
Save triggers that can fire inside the window:

| Trigger | Why it is reachable while dead | Source |
|---|---|---|
| `Esc` → `returnToMenu` (awaited save, then scene stop) | `handleSkillInput` reads `Esc` every frame with no hp check | `ZoneScene.ts:2203-2205, 4223-4228` |
| Quest turn-in from an NPC panel left open | UIScene never subscribes to `PLAYER_DIED`; panels stay interactive | `ZoneScene.ts:4112-4132` |
| Difficulty completed | `demon_lord` killed by a DoT / mercenary / pet after the hero fell → `onMonsterKilled` | `ZoneScene.ts:3855` (later milestone) |
| Story queue finished | `StoryDirector.update` enqueues boss intros by distance with no hero-alive check; queue end saves. In practice the cutscene outlasts the window, so the respawn has run before the save | `StoryDirector.ts:297-317, 162` |
| Ember Tower page actions (harvest, gem combine, …) | panel buttons are not life-gated | `EmberTower.ts:260, 299` (later milestone) |

Conversely, the **overworld respawn does not save**: neither `Player.respawnAtCamp` (`Player.ts:431-452`) nor the
handler (`ZoneScene.ts:949-952`) calls `autoSave`, so the on-disk save keeps the pre-death gold and no echo until the
next trigger of §3.6 — closing the app right after a death undoes the penalty (Q36). The labyrinth / sub-dungeon death
path *does* save: the parent zone is restarted with `playerStats.hp = maxHp` (`:947`) and its `create` autosaves (`:703`).

**Port rule (FIX Q34/Q36) — normative:**
1. Only a living hero is ever serialised: `Session::CanSave()` = `hero.life == HeroLife::Alive && hero.hp > 0 &&
   !transitioning` (`HeroLife` per §5.1.1). `BuildSave` asserts it.
2. A save trigger that fires while the hero is `Dying` is **deferred**, not dropped: `RequestSave` sets
   `savePending = true`; the flag is flushed by the save written right after the respawn (rule 4).
3. Requests that cannot wait — Return to menu, Quit, window close, `OnApplicationWillDeactivate` /
   `OnApplicationEnteredBackground` — call `Session::ResolvePendingDeath()` first. It completes the respawn in the core
   immediately, with exactly the effects of the timer (combat-feel §13.3 step 3): overworld → `hp = maxHp`,
   `mana = maxMana`, position = current zone `camps[0]`, path/target/hold-move/skill buffer cleared; labyrinth /
   sub-dungeon → labyrinth run ended `'fallen'`, hero placed at the parent zone's `camps[0]` (fallback `playerStart`,
   then `(3,3)`) with full HP/MP. The death penalty is **never** re-applied (it was taken at death). Then save.
4. Autosave immediately after every completed respawn (overworld and dungeon paths), so a death is always committed
   to disk with the penalty and the echo.

### 3.5 Restoring — `restoreFromSave(save)` (`ZoneScene.ts:4298-4416`), exact order
Runs inside `create` after the hero exists at `mapData.playerStart` (or target) and before spawns:
1. Hero: level, exp, gold, `stats = {...}`, free points, skillLevels (record or entries), `spirit.restore(save.spirit
   ?? {0,0})`, `recalcDerived()` (**no equipment stats yet**), `hp = min(save.hp, maxHp)`, `mana = min(save.mana,
   maxMana)` (Q8).
2. Position: `findNearestWalkablePosition(...)` (§3.7); non-null → log `zone.save.positionReset` (system) and
   `moveTo(camp)`; else `moveTo(save.tileCol, save.tileRow)`.
3. Inventory, equipment, stash (force identified).
4. Quests `loadProgress`; story `load(storySeen ?? ['prologue'])`; soul echo `load`; abyss record (clamped).
5. Homestead buildings (`?? {}`), tower `load(save.homestead)`.
6. Pets `loadSave(save)`.
7. Achievements `loadData` (if present).
8. `fogData = exploration` (if present).
9. `autoCombat = settings?.autoCombat ?? false`, `autoLootMode = settings?.autoLootMode ?? 'off'`, `difficulty ??
   'normal'`, `completedDifficulties ?? []`.
10. Mercenary (only if present — a stale one from a previous session is **not** cleared, Q1).
11. Dialogue state → UIScene (only if present).
12. `miniBossDialogueSeen`, `loreCollected`, `discoveredHiddenAreas` → Sets (only if present).

**Port**: `abyss::ApplySave(Session&, const SaveData&, const ZoneCollision&)` on a **fresh** session; restore
equipment before deriving max HP/MP (FIX Q8); run all normalisations of §3.3 in the core.

**Loading a dead hero (web: soft-lock, QUIRK Q35).** Step 1 does `hp = min(save.hp, maxHp)` (`ZoneScene.ts:4313`),
so a save written during the death window (§3.4) loads a hero with **0 HP** on the saved death spot (walkable, so
step 2 keeps it). Nothing ever calls `die()` or `respawnAtCamp`: death is only entered from a damage path through
`killPlayer` (`:7171-7175`), and every hero-damage path returns early when `hp ≤ 0` (`:1204, 2809, 2945, 2958, 6111`).
Result: the hero cannot move (keyboard `:2114`, click `:782`, hold-move `:869`, `Player.update` early return
`Player.ts:219-223`), attack or cast (`:2249, 2809`), dodge (`:2379`), portal (`:5802`), regenerate HP or MP
(`Player.ts:219-223`, `ZoneScene.ts:1376`), claim the soul echo (`:1269`) or be attacked (`:2809`); the HUD shows
0 HP while the sprite stands upright (a fresh `Player`, no death pose, no death text). `Esc` → Continue reproduces the
same state. The only exits are accidental HP writers without a life check: drinking an HP potion from the bag
(`UIScene.ts:4297-4301`), a healer mercenary's heal (`MercenarySystem.ts:576-580`: hero ratio 0 < `HEAL_THRESHOLD
0.6`, merc mana ≥ 15, every ≥ 2000 ms; applied at `ZoneScene.ts:6498-6503`), auto-pickup of an HP potion dropped
within 2 tiles (`:1496-1518`), or a level-up from exp credited by a mercenary / pet / DoT kill (`Player.addExp`
refills HP, `Player.ts:156-166`). Each revives the hero **on the death spot**, where the saved soul echo lies, so the
echo is claimed on the next frame (range 1.5, `:1267-1271`) and the whole death penalty is refunded.

**Port rule (FIX Q35) — normative**, in `ApplySave`, after equipment is restored and max HP/MP are derived (FIX Q8):
* `!(std::isfinite(save.player.hp) && save.player.hp > 0)` (0, negative, NaN/missing) → **load as a completed
  respawn**: position = `camps[0]` of the zone being loaded (fallback `playerStart`; a labyrinth save is already on
  `abyss_rift`, a sub-dungeon save is first resolved to its parent zone, Q7) — this replaces the §3.7 walkability
  check; `hp = maxHp`, `mana = maxMana`; `life = HeroLife::Alive`; no statuses, no path/target; log
  `sys.player.respawn` (system). The death penalty is **not** applied again and the soul echo is kept exactly as saved
  (the corpse run stays possible). The zone-entry autosave (§3.6) then overwrites the bad save with the healthy state.
* Otherwise `hp = min(save.hp, maxHp)`; `mana` = `maxMana` if non-finite, else `clamp(save.mana, 0, maxMana)`.
* Saves written by the port never contain a dead hero (§3.4 rule 1); this branch exists for web-imported saves and
  for corrupted files.

### 3.6 Save triggers (complete list; there is no timer-based autosave — Q4)
| Trigger | Source |
|---|---|
| End of every zone `create` (zone entry, new game, load, respawn-restart) | `ZoneScene.ts:703` |
| Zone change (before fade; saves old zone) | `:4190` |
| Return to menu (awaited) | `:4224` |
| Quest turn-in (after rewards) | `:4132` |
| Story beat queue finished (after any prologue/card/cutscene/boss intro) | `StoryDirector.ts:162` |
| Soul echo claimed | `ZoneScene.ts:1286` |
| Difficulty completed (demon_lord killed in abyss_rift) | `:3855` |
| Enter labyrinth / sub-dungeon (before transition) | `:5110`, `:5264` |
| Labyrinth run summary shown (800 ms after rift entry) | `:990` |
| Ember Tower actions (harvest, gem combine, expedition send/return, blessing, upgrade) — `save()` host callback wired at `ZoneScene.ts:539` | `EmberTower.ts:260, 299, 315, 339, 347, 356` |

**Port additions (recommended, FIX Q4)**: autosave every 60 s of play while not in combat/cinematic, on
`applicationWillDeactivate`/`OnApplicationEnteredBackground` (iOS/Android) and on window close (desktop).
**Port additions (FIX Q34/Q36)**: autosave after every completed respawn; every trigger above goes through
`RequestSave`, which is deferred while the hero is `Dying` (menu / quit / background first `ResolvePendingDeath()`),
see §3.4 port rule. Not a trigger on the web: the overworld respawn.

### 3.7 `findNearestWalkablePosition(col, row, collisions, camps, cols, rows)` (`SaveSystem.ts:132-165`)
`collisions[r][c] === true` means **walkable**. `rc = round(col)`, `rr = round(row)` (JS round). Unwalkable if
out of bounds or `!collisions[rr][rc]`. Walkable → `null`. Unwalkable and no camps → `null` (hero stays on the bad
tile). Otherwise the camp minimising `(camp.col − col)² + (camp.row − row)²` (unrounded hero coords; first wins on
ties) → `{col, row}` of that camp.

Test vectors (`src/__tests__/SaveMigration.test.ts:428-505`; test map = border walls, camp (5,5)):
`(0,0)` wall → (5,5); `(3,3)` walkable → null; camps (5,5),(15,15), (14,14) blocked → (15,15); `(200,200)` OOB →
(5,5); 120×120 map, camp (15,15), (79,79) blocked → (15,15); `(1,1)` on 20×20 → null; camps (5,5),(25,25), (23,23)
blocked → (25,25).

### 3.8 Other persisted settings (browser `localStorage`, not in the save)
| Key | Content | Default | Source |
|---|---|---|---|
| `abyssfire_locale` | `'zh-CN' \| 'zh-TW' \| 'en'` | `'zh-CN'` | `i18n/index.ts:14-46, 88-102` |
| `abyssfire_audio` | JSON `{bgmVolume 0–1, sfxVolume 0–1, bgmMuted, sfxMuted}` (each field validated by type, else default) | `{0.15, 0.3, false, false}` | `AudioManager.ts:25-31, 394-420` |
| `abyssfire_render_quality` | `'low' \| 'balanced' \| 'high'` (also `?quality=` URL override; `?res=1\|1.5\|2` overrides render scale) | auto (`selectRenderQuality`) | `RenderQuality.ts:71-100`, `RenderScale.ts:13-26` |

**Port**: one `Settings.json` (core-owned schema, UE-owned file I/O) next to the saves: `{locale, audio{...},
renderQuality, (later) keyBindings, touchLayoutScale}`; render quality maps to UE scalability groups (render-only).

### 3.9 Example (illustrative) v3 save right after a new warrior enters `emerald_plains`
```json
{ "id": "autosave", "version": 3, "timestamp": 1760000000000, "classId": "warrior",
  "player": { "level": 1, "exp": 0, "gold": 0, "hp": 150, "maxHp": 150, "mana": 85, "maxMana": 85,
    "stats": { "str": 12, "dex": 8, "vit": 10, "int": 5, "spi": 5, "lck": 5 },
    "freeStatPoints": 0, "freeSkillPoints": 0,
    "skillLevels": { "slash": 1, "...tier-1 skills": 1, "...others": 0 },
    "spirit": { "value": 0, "resonanceRemainingMs": 0 },
    "tileCol": 15, "tileRow": 22, "currentMap": "emerald_plains" },
  "inventory": [], "equipment": {}, "stash": [], "quests": [], "exploration": {},
  "homestead": { "buildings": {}, "embers": 0, "garden": { "progress": 0, "stock": {} },
                 "expedition": null, "blessing": null, "towerReturn": null },
  "pets": { "owned": [], "active": null }, "achievements": {},
  "settings": { "autoCombat": false, "musicVolume": 0.5, "sfxVolume": 0.7, "autoLootMode": "off" },
  "difficulty": "normal", "completedDifficulties": [], "dialogueState": {},
  "miniBossDialogueSeen": [], "loreCollected": [], "discoveredHiddenAreas": [],
  "storySeen": ["prologue", "chapter_emerald_plains"], "soulEcho": null,
  "abyss": { "unlockedTier": 1, "bestTier": 0 } }
```
(Building levels and starter skill levels come from their own specs; story ids from `quests-story-ch1.md` §8.)

### 3.10 Port design (core + UE)
* **Core** owns `SaveData` (plain structs mirroring §3.2), `ParseSave(json) → (SaveData, SaveError)`,
  `SerializeSave(SaveData) → std::string`, `MigrateRaw(json)` (operates on the generic JSON tree so missing
  fields can be defaulted before the typed parse), `BuildSave(const Session&)`, `ApplySave(...)`. JSON library must
  work without exceptions (e.g. RapidJSON or yyjson). Field names identical to the web.
* **UE** implements `abyss::ISaveStorage {Read(slot), Write(slot, bytes), Remove(slot), List()}` with files under
  `FPaths::ProjectSavedDir()/SaveGames/` (`autosave.json` or `slot_<n>.json`). Write = write `*.tmp` → flush →
  atomic rename; keep the previous file as `*.bak` and fall back to it when the main file fails to parse.
* Save on a worker thread (serialize on the game thread, write async); never block the frame.
* Slot model: see OQ-SAVE-1 (web = single autosave; recommendation = 3 character slots, each autosaving).

---

## 4. EventBus (UI-relevant contract) — `src/utils/EventBus.ts`
| Event (string) | Payload | Emitter → listener |
|---|---|---|
| `log:message` `LOG_MESSAGE` | `{text, type}`; types used: `system`, `combat`, `loot`, `info`, `quest` | everywhere → combat log |
| `combat:target_changed` | `{targetId \| null, targetName \| null}` | zone → target frame |
| `item:picked` | `{item: ItemInstance}` | zone → loot notice |
| `skill:level_changed` | `{skillId, level}` | skill tree → zone (re-loadout, mobile relayout), UIScene (restart if level==1, Q16) |
| `npc:interact` | `{npcId, npcName, dialogue, actions[], dialogueTree?, completedQuests, questSystem, player, homesteadSystem, achievementSystem, turnedIn}` (`ZoneScene.ts:4161-4176`) | zone → quest card / dialogue tree / simple dialogue |
| `shop:open` | `{npcId, shopItems: string[], type: 'merchant'\|'blacksmith'}` | zone → shop panel |
| `shop:close` | `{npcId}` | shop/stash close → NPC presentation |
| `dialogue:close` | — | simple dialogue close |
| `ui:toggle_panel` | `{panel, npcId?, page?}`; panels `inventory, map, skills, character, homestead, quest, audio, companion, pets, achievement, stash` | keys / touch buttons → UIScene (`UIScene.ts:851-864`; ignored while the abyss boon choice is up) |
| `ui:skill_click` | `{index, skillId}` | skill slot / touch fan → zone `requestSkill` |
| `ui:dodge_request` | `{dx?, dy?}` (tile space) | touch dodge → zone `performDodge` |
| `ui:target_cycle` | `{}` | touch lock button → zone |
| `ui:toggleCombatLog` | — (string literal, not in `GameEvents`) | touch log toggle → UIScene |
| `ui:refresh` | `{player, zone}` (string literal) | zone (re-entry) → UIScene |
| `miniboss:dialogue` | `{bossName, dialogueTree, onDismiss}` | zone → mini-boss panel |
| `lore:collected` | `{entry: LoreEntry}` | zone → lore popup |
| `achievement:unlocked` | `{achievement}` | AchievementSystem → toast |
| `story:boss_bar` | `{name, epithet, hp: () → {hp,maxHp}\|null}` or `null` | StoryDirector → boss bar |
| `story:state` | `{active}` | StoryDirector → audio etc. |
| `quest:*` (accepted, completed, progress, tracked_changed, turned_in, failed) | quests spec §2 | → tracker refresh (`UIScene.ts:777-785`) |
| `locale:changed` | locale id (string) | `setLocale` → menu, UIScene, ZoneScene, mobile controls |
| `zone:entered` | `{mapId}` (`'menu'` from the menu) | → audio |
| `dungeon:*` | payload interfaces at `EventBus.ts:89-137` | labyrinth (later milestone) |

Declared but never emitted: `SAVE_GAME`, `LOAD_GAME`, `ZONE_EXIT`, `INVENTORY_OPEN/CLOSE`, `GEM_SOCKET_OPEN`,
`DUNGEON_BOSS_KILLED` — drop them in the port. Many emitted events have no listener (`PLAYER_HEALTH_CHANGED`,
`INVENTORY_CHANGED`, …); the HUD **polls** the hero every frame instead (§6.1, §6.3–§6.9). Port: the HUD may keep polling a
`HudViewModel` snapshot built by the core each tick (simplest, matches web).

---

## 5. Input

### 5.1 Keyboard bindings (`ZoneScene.ts:615-643, 2113-2223`; all edge-triggered with `JustDown` except movement)
| Key | Action | Notes |
|---|---|---|
| `W`/`↑`, `A`/`←`, `S`/`↓`, `D`/`→` | move (held) | §5.2 |
| `1`–`6` | `requestSkill(loadout[i])` | loadout = learned skills in class order (classes spec §7.5); missing slot → nothing |
| `Space` | dodge | `performDodge(now)` (combat spec §8.1) |
| `Q` | cycle combat target | combat spec §9.3 |
| `Tab` | toggle auto-combat | log `zone.combat.autoCombat {state: zone.combat.autoCombatOn/Off}` (system) |
| `I` | toggle inventory | `UI_TOGGLE_PANEL {panel:'inventory'}` |
| `M` | toggle world map | `'map'` |
| `K` | toggle skill tree | `'skills'` |
| `C` | toggle character | `'character'` |
| `J` | toggle quest log | `'quest'` |
| `H` | toggle homestead | `'homestead'` (later milestone) |
| `P` | toggle ley-beasts | `'pets'` (later milestone) |
| `U` | toggle companion | `'companion'` |
| `V` | toggle achievements | `'achievement'` |
| `O` | toggle audio settings | `'audio'` |
| `R` | town portal | §5.4 |
| `Esc` | return to main menu (autosave first) | no confirm, ignores open panels (Q10); not gated by death — saves a dead hero (Q34, §5.1.1) |
| `Ctrl+Shift+E` | dev-only texture export | `import.meta.env.DEV` only; drop |

Processing order each frame (`update`, `:1358-1362`): keyboard movement → hold-move → `handleSkillInput` (keys in the
order Tab, Q, Space, I, M, K, H, C, J, O, R, P, U, V, Esc, 1–6) → gamepad → `consumeBufferedSkill`.

**Other key handlers**: StoryScene: `Space`/`Enter`/click = next, `Esc` = skip (`StoryScene.ts:64-67`); Abyss UI:
tier picker `Enter` confirm, `-`/`_`/NumpadSubtract and `+`/`=`/NumpadAdd step tier; boon choice `1`–`4`; summary
`Enter` (`AbyssRunUI.ts:486-490, 623-626, 1006`).

**Gating**: none of these are blocked by open panels or dialogs (Q11) — only by the freeze gates of §1.5 and, per
action, by `hp ≤ 0` (exact list in §5.1.1; notably `Esc` and the panel keys are **not** gated). Panel toggles are
ignored by UIScene while the abyss boon choice is open.

#### 5.1.1 Dead hero (`hp ≤ 0`) — what the web gates, and the port rule
The web has no explicit "dead" state: each call site tests `player.hp <= 0`. The hero is dead from `Player.die()`
until the respawn 1100 ms later (overworld `respawnAtCamp`; dungeon paths restart the parent zone), or indefinitely
after loading a save with `hp = 0` (§3.5). Per input / action (verified at each call site):

| Input / action | While `hp ≤ 0` (web) | Source |
|---|---|---|
| Move keys, joystick, left stick | ignored | `ZoneScene.ts:2114` |
| World click / tap (loot, NPC, monster, exit, ground) | ignored; right click still calls the town portal, which refuses | `:778-782` |
| Hold-to-move | cancelled (`holdMove = null`) | `:869` |
| Skills `1`–`6`, skill-bar click, touch skill buttons, pad A/X/Y/RB | not executed, **but buffered**: `canExecuteSkill` is false → `combatInput.request` queues the skill for 180 ms and emits `SKILL_BUFFERED` (HUD buffered glow); a press in the last 180 ms of the window fires right after the respawn | `:2249, 2269-2294, 171`; `CombatInputSystem.ts:23-35` |
| Dodge (`Space`, pad B, touch dodge) | ignored | `:2379` |
| Town portal (`R`, right click) | refused; a channel started **before** death still teleports the corpse to the camp at +1500 ms and sets α 1 (death only clears `isPortaling`, Q23) | `:5802, 899, 5868-5884` |
| `Tab` auto-combat | toggles the flag + log (the auto-combat routine itself is gated) | `:2160-2163, 3072` |
| `Q` / pad LB target cycle | runs (sets `attackTarget`; the respawn clears it) | `:2164-2166, 2346` |
| Panel keys `I M K H C J O P U V` | open/close normally; every panel stays fully interactive | `:2170-2202` |
| `Esc` | **returns to the menu and saves `hp = 0`** → soft-lock on Continue (Q34/Q35) | `:2203-2205, 4223-4228` |
| Bag context menu "Use" on an HP/MP potion | **applied** (`hp = min(maxHp, hp + value)`) → revives mid-death | `UIScene.ts:4297-4301` |
| Equip/unequip, stat & skill points, shop, stash, blacksmith, sockets, NPC dialogue incl. quest accept / turn-in | all work (turn-in autosaves; its exp can level up → HP refill) | UIScene panels; `ZoneScene.ts:4112-4132`; `Player.ts:156-166` |
| Potion auto-pickup (≤ 2 tiles), auto-loot (every 300 ms) | potion pickup **applies HP/MP** → revive; auto-loot picks items up | `ZoneScene.ts:1496-1518, 1520-1524` |
| Soul echo claim | blocked | `:1269` |

Non-input HP writers without a life check (same mid-death revive): healer-mercenary heal (`ZoneScene.ts:6498-6503`,
decision `MercenarySystem.ts:576-580`), the level-up refill when a mercenary / pet / DoT kill credits exp
(`Player.ts:156-166`), the pet `heal` ability (`PetCompanion.ts:582-586`, later milestone). Guarded writers: regen
(`Player.ts:219-223, 245-247`; `ZoneScene.ts:1376`), life steal (`:3183`), thorns heal (`:3002`), kill heal (`:3814`);
the death-save proc and the pet revive act **before** death (`:3055-3064`, `killPlayer` `:7171-7175`).
Consequences of a mid-death revive (QUIRK Q35): the hero becomes controllable while still in the death pose (α 0.12)
until the unconditional respawn at +1100 ms; standing on the just-left soul echo it reclaims it on the next frame
(penalty refunded + autosave, `:1267-1286`); if it is killed again inside the window, `handlePlayerDied` runs a second
time — a second penalty, the first echo is returned by `leave` as "faded" (lost), and two respawn timers fire.

**Port rule (FIX Q34/Q35) — normative:**
* The core owns `enum class HeroLife : uint8_t { Alive, Dying }`. `Dying` starts in `KillHero()` after the death-save
  and pet-revive checks fail (combat-feel §13.3) and ends when the respawn completes (core timer 1100 ms on the
  session clock, pausing with the game; or `ResolvePendingDeath()`). `IsDead() = life != Alive || hp <= 0`. Death is
  entered exactly once: `KillHero()` while `Dying` is a no-op.
* While `Dying`, the core **rejects** (no-op, no buffering, no event): movement, click/tap actions, hold-move, skill
  requests (death also clears the skill buffer), dodge, target cycle, town portal (death cancels an active channel,
  FIX Q23), interaction, item / potion / gold pickup (drops stay on the ground; auto-loot pauses), consumable use,
  equip changes, shop / stash / blacksmith / socket transactions, quest accept / turn-in, dialogue choices, homestead
  actions, stat / skill point spending. `Hero::Heal()` / `RestoreMana()` return 0 unless `Alive`; exp is still
  credited while `Dying` (level, points) but a level-up does not refill HP/MP (the respawn does).
* UE on `HeroDied`: close modal NPC panels (dialogue, shop, stash, blacksmith, socket, quest card, confirms) and the
  item context popup; information panels (inventory, character, skills, quest log, map) may stay open but read-only
  (action buttons disabled, drag-and-drop off) until `HeroRespawned`. Touch skill / dodge buttons show disabled.
* Allowed while `Dying`: opening/closing information panels, the `Tab` auto-combat preference, camera/UI-only input
  and `Esc`. `Esc` opens the system menu (FIX Q10); "Return to menu" / "Quit" there (and app background / window
  close) call `ResolvePendingDeath()` and then save (§3.4 rule 3). No other path may save a `Dying` hero.

### 5.2 Hero movement from input
**Direct (keyboard / joystick / stick)** — `handleKeyboardMovement(delta)` (`ZoneScene.ts:2113-2156`):
1. Skip if `hp ≤ 0`.
2. Keyboard vector in **tile space**: up `(dx,dy) += (−1,−1)`, down `(+1,+1)`, left `(−1,+1)`, right `(+1,−1)`
   (screen directions of the 2:1 iso view: up = north-west diagonal of the grid). Opposite keys cancel.
3. If zero: mobile joystick `getDirection()` (§5.7.2). If still zero: gamepad left stick `(sx, sy)` when
   `hypot > 0.18` → `(dx, dy) = (sx + sy, −sx + sy)`.
4. Non-zero → `holdMove = null`, `path = []`; `speed = moveSpeed × delta/1000 × 0.015` **tiles** (= 1.8 tiles/s at
   `moveSpeed 120`); `len = hypot(dx,dy)`; `lastMoveDirection = (dx,dy)/len`; candidate `(col + dx/len·speed, row +
   dy/len·speed)`; accept only if `round(candidate)` is in bounds and walkable → `player.moveDirect(col,row)` (teleport
   step + walk anim + facing, keeps walk anim alive 120 ms, `Player.ts:197-205`). Blocked → no move (no wall sliding).
* Magnitude is ignored (normalised): any joystick deflection / stick past 0.18 = full speed (Q15).

**Path following (click / hold-to-move)** — `Player.updateMovement` (`Player.ts:252-310`): `currentSpeed += (moveSpeed
− currentSpeed) · 8 · dt` (acceleration 8/s); the hero moves toward `path[0]`'s **screen (iso) position** at
`currentSpeed` iso-px/s; on reaching it snaps to the tile and shifts the path; tile coords are interpolated by the
same fraction. Empty path → `currentSpeed *= 1 − 12·dt` (deceleration), < 0.5 → idle. Because speed is in iso screen
px, ground speed depends on direction: `120 / |cartToIso(dc,dr)|` per step → 3.35 tiles/s along a grid axis, 2.65
tiles/s for (+1,−1) steps, 5.3 tiles/s for (+1,+1) steps (Q13). **Port (recommended)**: one ground speed for both
modes, `tilesPerSec = moveSpeed / 36` (36 = RMS iso px per tile, the conversion `combat-feel.md` §0 uses) — see
OQ-INPUT-1.

**3D input mapping**: the camera has a fixed yaw such that screen-up is the grid's (−1,−1) diagonal. A screen-space
input vector `(sx, sy)` (x right, y down) maps to tile space by `(dx, dy) = (sx + sy, −sx + sy)` (exactly the web
formula for the stick and joystick, and equivalent to the WASD table). Implement once in the core:
`Vec2 ScreenDirToTile(Vec2 s)`.

### 5.3 Mouse / pointer on the world — `handlePointerDown` (`ZoneScene.ts:774-857`)
Order of checks (first match wins):
1. Cinematic → ignore. Pointer claimed by a touch control (§5.7.4) → ignore.
2. **Right button** → `useTownPortal()` (checked before the dead check; the portal itself refuses while dead).
3. Hero dead → ignore.
4. `tile = worldToTile(pointer world)` — `floor` of the iso→cart conversion (half-tile bias, Q14).
5. Loot drop with `|l.col−col| < 1.5 && |l.row−row| < 1.5` → `pickupLoot` (loot spec §6).
6. NPC with `(npc−tile)² < 3.24` (nearest) **and** within 3 tiles (Euclid) of the hero → `interactNPC`. A farther NPC
   falls through (the hero just walks to the clicked tile; no interact on arrival — Q32).
7. Ember Tower objects (`emberTower.handleClick`) (later milestone).
8. Sub-dungeon entrance (±1.5) and hero `distSq ≤ 9` → enter.
9. Labyrinth portal (`|Δ| < 2` from (60,60) in abyss_rift) and hero `distSq ≤ 9` → tier picker.
10. Hidden-area reward chest (±1.5) and hero `distSq ≤ 4` → collect.
11. Alive monster within ±1.5 (spatial grid radius 2) → `attackTarget = id`, emit `TARGET_CHANGED {id, localized
    name}`, path to the monster's rounded tile.
12. Exit within ±1.5 → zone change / floor exit / sub-dungeon exit.
13. In-bounds tile → path from the hero's rounded tile to it; non-empty → `setPath`, clear target (emit
    `TARGET_CHANGED {null,null}`); **always** start hold-to-move `{pointerId, col, row, repathAt: now + 120}`.

**Hold-to-move** (`updateHoldMove`, `:865-894`, `HOLD_MOVE_REPATH_MS = 120`, `:113`) every frame while `holdMove`:
* Pointer released / gone / hero dead → `holdMove = null` (the current path is finished normally).
* Recompute the pointer's world tile (the camera moves), clamp to the map.
* `hypot(target − hero) < 0.6` tiles → `path = []` (stand still).
* If the target tile is unchanged **and** `now < repathAt` **and** a path exists → keep.
* Else `repathAt = now + 120`; goal = target if walkable else `findWalkableNear(col,row,3)` (rings r = 1..3, scan
  `dr = −r..r`, `dc = −r..r`, first walkable cell on the ring perimeter, `:6655-6665`); none → keep; else re-path.

**Other mouse behaviour**: browser context menu suppressed on the canvas (`:331, 662`). Hover on UI items shows
tooltips (desktop only). Mouse wheel scrolls the skill tree (`scrollY += dy·0.5`) and the achievement list (one row per
wheel event). No mouse-over highlighting of monsters/NPCs/loot in the world (render detail; add in UE).

**UE**: ground picking = line trace from the cursor against the terrain/nav floor → world point → tile `(round(x/T),
round(y/T))` (FIX Q14; T = tile size in uu); actors (monsters, NPCs, loot, exits, portals) get collision channels so
the trace can hit them first with the same priority order as above. UI widgets must consume clicks (CommonUI input
routing) so that clicks on any panel never reach the world (FIX Q12).

### 5.4 Town portal (`useTownPortal`, `ZoneScene.ts:5801-5884`; keys `R` / right click)
1. Refuse silently if hero dead, already portaling, or transitioning.
2. Destination: in a sub-dungeon or labyrinth floor → `mapData.exits[0]` (none → refuse); if hero `distSq < 9` from
   it → log `zone.teleport.alreadyAtExit`, refuse. Overworld → `campPositions[0]`; if hero within `safeZoneRadius ?? 9`
   (`distSq < r²`) → log `zone.teleport.alreadyAtCamp`, refuse.
3. `isPortaling = true`; clear path, `isMoving = false`, `attackTarget = null`; log `zone.teleport.opening`.
4. VFX for 1500 ms (expanding blue rings `0x4488ff/0x66aaff/0x2266cc`, hero alpha flicker 0.5, 200 ms ×4).
5. After **1500 ms**: camera flash 200 ms `0x4488ff` α 0.5, SFX `zone_transition`, `moveTo(dest)`, alpha 1,
   `isPortaling = false`, log arrival (`zone.teleport.toCamp` / `toDungeonExit` / `toSubDungeonEntrance`).
* Free and unlimited (the `c_tp_scroll` item does nothing, loot spec §7.4). Movement keys still work during the channel
  (keyboard moves are not blocked), and death does **not** cancel the delayed teleport (Q23).

### 5.5 Gamepad (`handleGamepadInput`, `ZoneScene.ts:2225-2245`; Phaser standard mapping, pad 0)
| Input | Action |
|---|---|
| Left stick (deadzone 0.18) | move (§5.2) |
| Button 0 (A / Cross) | skill slot 1 |
| Button 2 (X / Square) | skill slot 2 |
| Button 3 (Y / Triangle) | skill slot 3 |
| Button 5 (RB / R1) | skill slot 4 |
| Button 1 (B / Circle) | dodge |
| Button 4 (LB / L1) | cycle target |

Edge-detected per button (pressed && !wasPressed). No gamepad UI navigation, no panel toggles, no slots 5–6 (port:
add D-pad/menu-button panel access and CommonUI gamepad navigation — OQ-INPUT-3).

### 5.6 Recommended Enhanced Input setup (UE)
`IMC_Gameplay_KBM`, `IMC_Gameplay_Gamepad`, `IMC_Touch` (virtual controls feed the same actions), `IMC_UI`.
Actions: `IA_Move (Axis2D, screen space)`, `IA_Skill1..6`, `IA_Dodge`, `IA_TargetCycle`, `IA_ToggleAuto`,
`IA_TownPortal`, `IA_Panel_{Inventory,Map,Skills,Character,Quest,Homestead,Pets,Companion,Achievements,Audio}`,
`IA_Back (Esc)`, `IA_Click (LMB)`, `IA_AltClick (RMB)`, `IA_Scroll`. The PlayerController converts `IA_Move` with
`ScreenDirToTile` and forwards discrete actions to the core as an `InputFrame` (`{moveTile, skillPressed[6], dodge,
targetCycle, toggleAuto, townPortal, clickTile?, clickTarget?}`) — the core applies the same rules as the web.

### 5.7 Touch controls — `MobileControlsSystem` (`src/systems/MobileControlsSystem.ts`)

#### 5.7.1 Device detection and units
* `isMobileDevice()` (`:16-24`): touch-capable (`ontouchstart` or `maxTouchPoints > 0`) **and** (mobile UA regex
  `Android|iPhone|iPad|iPod|webOS|BlackBerry|IEMobile|Opera Mini` **or** `innerWidth ≤ 1024`). Drives both the touch
  controls and the UIScene "IS_MOBILE" layout. **Port**: iOS/Android → touch layout; desktop → KBM layout; optional
  setting to force either.
* Sizes are specified in **CSS px** (`CSS`, `:30-44`) and converted with `k = clamp(max(displayScale.x,
  displayScale.y), 1, 2)` game px per CSS px (`measureK`, `:152-156`; `displayScale` = canvas px / CSS px, ≈ 1280 /
  canvas CSS width). `px(c) = round(c·k)`. Rebuilt on resize when k changes by ≥ 0.01, and on skill/locale changes.
  **UE**: CSS px ≈ density-independent points; compute `k = clamp(1280 / viewportWidthInDIP, 1, 2)` at the 1280×720
  design scale (or simply express these sizes in DIP with a separate touch-layer DPI rule).
* Up to 4 simultaneous touches (`TOUCH_POINTERS = 4`).

#### 5.7.2 Layout (landscape; positions in logical px for k = 1 / 1.5 / 2)
| Control | Formula | k=1 | k=1.5 | k=2 |
|---|---|---|---|---|
| Joystick base (r = 58 css) | centre `(m + r + px(6), 720 − m − r)`, m = px(12) | (76,650) r58 | (114,615) r87 | (152,580) r116 |
| Joystick grab zone | circle r·1.35 | | | |
| Corner button "锁定/LOCK" (r 36 css) | `(1280 − px(48), 720 − px(48))` | (1232,672) | (1208,648) | (1184,624) |
| Skills 1–4 (r 28 css) | ring1 radius 100 css at 178°, 210°, 242°, 274° (0° = right, 90° = down) from the corner centre | (1132,675) (1145,622) (1185,584) (1239,572) | (1058,653) … | (984,631) … |
| Dodge "闪避/DODGE" (r 25 css) | ring2 radius 162 css at 190° | (1072,644) | (969,606) | (865,568) |
| Skills 5–6 | ring2 at 220°, 250° | (1108,568) (1177,520) | (1022,492) (1125,420) | (936,416) (1073,320) |
| Toggles (top-left) | y = m + 22 css; auto-combat 66×44 css at x = m, auto-loot next (gap 6 css), log toggle 44×44 square | y 34 | y 51 | y 68 |
| Panel buttons (top-right) | 7 squares 44 css, gap 4 css, right-aligned at margin 12 css, y = m + 22 css: bag, character, skills, map, homestead, pets, quest | x from 936 | x from 764 | x from 592 |

#### 5.7.3 Behaviour
* **Joystick** (`:245-297`): press inside the grab zone claims the pointer and activates; the thumb follows the
  finger clamped to radius r; state `(dx, dy) = offset / r` ∈ [−1,1]; release (pointerup of that pointer) → reset.
  `getDirection()` = `(jx + jy, −jx + jy)` tile space (§5.2), zero when inactive. Not a floating stick (fixed centre).
* **Round buttons** (`createRoundButton`, `:344-395`): medallion texture of the given colour (corner `0x6a2a24`, dodge
  `0x1f4a6a`, skills `0x2a2430`); icon (skill icon / attack glyph) and/or label; hit circle r·1.12; on pointerdown:
  claim, scale 0.92, fire **immediately** (no hold/release semantics); pointerup/out → scale 1.
  Corner → `UI_TARGET_CYCLE {}`; dodge → `UI_DODGE_REQUEST getDirection()` (zero → zone default direction); skill i →
  `UI_SKILL_CLICK {index: i, skillId}`. Skill buttons exist only for the first 6 learned skills.
* **Cooldowns** (`update`/`updateCooldown`, `:498-548`): skill `remaining = cooldownEnd − now`, `total =
  getSkillCooldown(skill, level)`; dodge `remaining = zone.getDodgeCooldownRemaining()`, total 0 → full dark sweep
  while cooling, button alpha 0.7. Sweep: black α 0.62 pie of radius r−2 starting at 12 o'clock covering the remaining
  fraction (redrawn when Δfrac > 0.01); text = `ceil(remaining/1000)` when ≥ 1000 ms else `(remaining/1000).toFixed(1)`;
  on ready: pop scale 1.12 → 1 over 180 ms.
* **Toggles** (`:397-458`): auto-combat (log `sys.mobile.autoCombat.log {state}`; label `sys.mobile.autoCombat.on/off`,
  colour on `#8ff07a` / off `#b0a8b4`), auto-loot (cycles `off → all → magic → rare → legendary → off`; label
  `ui.hud.autoLoot.<mode>`, colours off `#b0a8b4`, all `#e0d8cc`, magic `#4f8cff`, rare `#ffd84a`, legendary
  `#ff8a2a`), log (emits `ui:toggleCombatLog`, label `sys.mobile.log`). Icons from `HudIcons` (`auto`, `loot`, `log`).
* **Panel buttons** (`:460-494`): ghost buttons α 0.88 with icon (56 % of size) and caption `sys.mobile.panel.<id>`
  (shrunk to fit); click → `UI_TOGGLE_PANEL {panel}`. (`achievement`, `companion`, `audio` have no touch button — the
  touch build cannot open them; OQ-UI-4.)
* Everything sits in one root container counter-scaled by `RENDER_SCALE / cameraZoom` so it is screen-fixed
  (render-only).

#### 5.7.4 Pointer claiming (`claimsPointer`, `:162-168`)
Any press that lands on a touch control records `{pointerId, downTime}`; the world handler ignores that exact press
(`ZoneScene.ts:777`). Teleport targeting with a claimed pointer uses the joystick direction (`ZoneScene.ts:2544`,
combat spec). **UE**: virtual controls are UMG widgets that handle the touch and return `FReply::Handled()`.

### 5.8 Mobile page shell (`src/systems/MobileShell.ts`) — touch devices only
* First `pointerup` → request fullscreen (`navigationUI: 'hide'`) and `screen.orientation.lock('landscape')` (both may
  fail silently, e.g. iPhone Safari).
* Portrait (`(orientation: portrait) and (pointer: coarse)`) → show a "rotate your device" card (zh-CN `请横置设备` /
  `渊火需要横屏游玩`, otherwise English — zh-TW gets English, Q22) and **sleep the game loop** (no simulation);
  landscape → wake. Re-checked on orientation change, resize, fullscreen change.
* Long-press context menu and iOS pinch/double-tap gestures suppressed.
* **UE**: lock orientation to landscape (both directions) in the iOS/Android project settings; pause the core
  simulation on `applicationWillResignActive`/background (and save, §3.6); no rotate card needed.

---

## 6. HUD (UIScene) — layout, widgets, behaviour

### 6.1 Global
* UIScene is a parallel overlay scene with a screen camera (`UIScene.ts:354-396`); created once per gameplay
  session, refreshed on zone re-entry (`ui:refresh`, `:866-876`: reset target frame, sync pet medallion, force
  minimap/tracker refresh). It is **restarted** when a skill becomes level 1 (`:808-816`) to rebuild the skill bar
  (Q16).
* HUD depths: plate 2998, frames 2999, widgets 3000–3003, loot notices 3050, boss bar 2900. Panels/tooltips: §7.0.
* Two layouts: desktop and touch (`HUD`, `:152-205`). Font helper `hfs(n)` = desktop `n` px, touch
  `max(round(1.6n), 18)` px (`:147-149`).

### 6.2 Resolved layout constants (logical px; computed from `UIScene.ts:118-205`)
| Item | Desktop | Touch |
|---|---|---|
| Globe radius `GLOBE_R` | 44 | 50 |
| HP globe centre | (361, 668) | (350, 662) |
| MP globe centre | (919, 668) | (768, 662) |
| HUD plate | x 389–891, top 628 → bottom 724 | x 350–768, top 636 → 724 |
| Skill row | 6 slots 44 px, gap 6, start x 407, centre y 676 | none (touch fan) |
| Utility buttons | 3 × 50×44 at x 713 (gap 5): auto-combat (centre 738), auto-loot (793), bag (848) | none (top-left toggles) |
| Spirit bar | x 447, w 254, h 6, centre y 642; label at x 407 | x 456, w 192, h 8, y 658; label at x 412 |
| Exp bar | x 407, w 466, h 8, centre y 709 | x 412, w 294, h 12, y 696 |
| Combat log | frame x 12, y 558, 290×150 | expanded: x 16, y 130, 470×300; collapsed: lines above y 458 |
| Info plate (zone, gold, embers) | x 1058, y 12, 210×46 | x 1058, y 130, 210×56 |
| Minimap | 104×104 at (1155, 77) | (1155, 205) |
| Quest tracker | x 1058, y 204, w 218 | x 26, y 134, w 330 |
| Loot notices | 236×34 at (1032, 674), newest at the bottom | 342-wide (×1.45) at (388, 572) |
| Target frame | centre x 640, y 10, 280×44 | centre x 544, y 10, 260×52 |
| Dodge plate | frame rect (12,12) 158×26 | hidden |
| Boss bar | centre (640, 88), 560×14 | same |
| Achievement toast | 360×62 at (460, 60→70) | same |
| Pet medallion | 34 px at (1117, 94) | none |

(Touch reserves top-left for toggles, top-right row for panel buttons — hence `TOP_RIGHT_OFFSET = 118`, bottom-left
for the joystick, bottom-right for the skill fan.)

### 6.3 HP / MP globes (`createGlobe`, `:426-454`; update `:5770-5791`)
Layered glass orb: dark back, liquid tile (HP `0xc0281e`, MP `0x2a5fd6`) masked to the circle (r−1), glass shine,
ornate rim, centre text. Per frame: `level += (ratio − level) · 0.15` (frame-rate dependent lerp; port: `1 − 0.85^(dt·60)`),
liquid top at `orbBottom − 2r·level − surfaceOffset(7)`, wave scroll `+delta·0.012` px (MP `−0.8×`). Text
`"{ceil(hp)}/{maxHp}"` 13 px (touch 21) white bold stroke 3. HP ratio in (0, 0.3) → liquid alpha `0.6 + sin(now·0.008)
·0.4` (pulse), else 1. **UE**: a material (sphere mask, scrolling wave texture, fill parameter, emissive pulse)
driven by the same parameters; numbers in UMG text.

### 6.4 Spirit bar (`createCombatFeedback`, `:470-520`; update `:5793-5814`)
Label `ui.hud.spirit` 10 px `#f0b060`; framed bar (barFrame + barFill textures, fill colour = class
`spirit.profile.visualColor`), cropped to `round(w · value/max)`; value text `"{floor(value)}/{max}"` 10 px `#f0c080`
right of the bar; while resonating the bar alpha pulses `0.82 + sin(now·0.012)·0.18` and `ui.hud.resonance
{seconds: remaining/1000 toFixed(1)}` (11 px `#ffd27a`) shows 14 px (touch 22) above the bar centre.

### 6.5 Exp bar (`createExpBar`, `:456-468`; update `:5848-5855`)
Framed bar, fill `0x9b4fd0`, 10 tick segments; crop `round(w · clamp(exp/expToNext,0,1))`; centred text
`"Lv.{level}  ({exp}/{expToNext})"` 10 px `#ecd9ff` (no i18n key).

### 6.6 Skill bar (desktop only; `createSkillBar`, `:541-635`; cooldowns `:5893-5927`, sweep `:5930-5950`)
* 6 slots (44 px). Empty slot: "empty" texture + key number `#4a4450` 9 px at (+13, +14). Filled slot: icon at 36 px
  (fallback: first 2 chars of the localized name), cooldown overlay, ready-flash rect 38×38 `0xfff0c0` (ADD blend),
  key badge 14×13 with number `#f0dcae` 9 px at (+13,+14), cooldown seconds text 15 px white centred.
* Hover swaps to the hover texture; pointerdown → `UI_SKILL_CLICK {index, skillId}` (no tooltip on the bar).
* Cooldown: `remaining = cooldownEnd − now`; `frac = clamp(remaining / getSkillCooldown(skill, level), 0, 1)`; redraw
  when |Δfrac| > 0.004: square-clipped pie (radius = slot/2 − 3 projected onto the square) from angle `a0 = −90° +
  (1−frac)·360°` clockwise to `−90°+360°`, black α 0.66, plus a gold hand line (`0xffd98a` α 0.85, 1.5 px) from the
  centre to `a0`; text `ceil(remaining/1000)`. When it becomes ready: overlay cleared, flash α 0.55 → 0 over 320 ms.
* Utility buttons: auto-combat (`ui.hud.autoCombat.on/off`, two-line "AUTO\nON"; colour on `#8ff07a` / off
  `#b0a8b4`; click toggles + log `ui.hud.autoCombatLog.on/off`), auto-loot (cycle order above; labels
  `ui.hud.autoLoot.<mode>`; colours: off `#b0a8b4`, all `#e0d8cc`, magic/rare/legendary = quality colours), bag
  (`ui.hud.inventoryBtn`, primary, toggles inventory).

### 6.7 Target frame (`:493-517`; `handleTargetChanged` `:794-806`; update `:5836-5846`)
Hidden until `TARGET_CHANGED` with an id. Plate frame accent `0xc0503c`; text `ui.hud.target {targetName}` 12 px
`#ffb09a` (none: `ui.hud.targetNone`, `#777788`); HP bar (w−36)×8 red `0xc0281e`, sampled every 100 ms from the zone's
monster list by id; a target no longer in the list shows an empty bar until the next `TARGET_CHANGED`.

### 6.8 Dodge plate (desktop; `:519-538`, update `:5826-5834`)
Text `ui.hud.dodgeReady` ("闪避 [SPACE]") `#9bd7ff` + dot, or `ui.hud.dodgeCooldown {seconds toFixed(1)}` `#778899`
with dot `0x3a4450`.

### 6.9 Info plate (`createInfoDisplay`, `:673-691`; update `:5856-5889`)
Zone name (`getZoneName(currentMapId, map.name)`, 13 px Cinzel parchment) centred at the top; coin icon + gold
(`#ffd35a` 13 px); embers `"✦{embers}"` (`#ff9a4a` 12 px) 10 px right of the gold text, **empty until
`tower.towerUnlocked`** (Ch1: after `q_explore_goblin_camp` turn-in).

### 6.10 Combat log (`createLogPanel` `:647-664`, `handleLogMessage` `:788-792`, `updateLogDisplay` `:943-973`)
* Buffer: last **8** messages `{text, type, at}` (oldest dropped).
* Colours by type: `system #e8c77a`, `combat #ff8a72`, `loot #7ed36a`, `info #7fb6ff`, other (incl. `quest`)
  `#b8b0a4`. 12 px (touch 19) wrapped to w−20, stroke 2 (touch 3).
* Expanded (desktop always; touch after the log toggle): framed plate α 0.9 with section header `ui.hud.combatLog`;
  newest message at the bottom (bottom = y+h−6), older stacked upward by their wrapped height + 1 px; alpha newest 1,
  then `max(0.55, 1 − i·0.07)`; anything that would go above `y + 22` is hidden.
* Collapsed (touch default): no frame; at most **3** newest messages younger than **12 s**, bottom at y 458, top limit
  338; alpha `clamp((12000 − age)/3000, 0, 1) × [0.95, 0.75, 0.55][i]`; refreshed every 250 ms.

### 6.11 Minimap (`createMinimap` `:2890-2898`, `updateMinimap` `:2900-3038`; refresh every 250 ms)
Whole map scaled into 104×104 (`sx = 104/cols`, `sy = 104/rows`), **grid-aligned** (col → right, row → down — i.e.
rotated 45° relative to the iso view), **no fog** (Q24). Layers in order:
1. Tiles α 0.75: `0 grass 0x4a8c3f, 1 dirt 0x8b7355, 2 stone 0x6a6a6a, 3 water 0x1a5276, 4 wall 0x4a4a4a, 5 camp
   0x9e7c52`, other `0x222222`.
2. Hero: black r 4 α 0.8 + `0x7fd4ff` r 2.8.
3. Exits: 4×4 squares `0x00e676`.
4. abyss_rift (not in a run): labyrinth portal at (60,60) r 3 `0xff6600` + ring.
5. Quest NPCs (camp NPCs at the camp tile, field NPCs at their tile): turn-in ready (any of its quests `completed`)
   r 3.2 `0xffd23a`, else available quests r 2.4 `0xf5e6a8`, each on a black disc r+1.2 α 0.7.
6. Guide target (quests spec §5.1): 5-point star r 4.5 (`turn_in` `0xffd23a`, else `0xffb347`) on a dark disc.
7. Living monsters: aggro r 2 `0xff4444` α 0.9, else r 1.5 `0xcc6644` α 0.5.
8. For each active quest of this zone: quest area circle (main `0xf1c40f`, side `0x95a5a6`, fill α 0.25 + outline);
   unfinished `explore` locations 3×3 `0xf39c12` α 0.5; `investigate_clue` 3×3 `0x9b59b6` α 0.6; `escort`
   destination 4×4 `0xe67e22` α 0.6; defend target circle `0xe74c3c`; escort NPC start r 2 `0xe67e22`.
Frame: iron ring texture over a `0x07060a` square. **UE**: build a minimap texture once per zone from the tile grid
(same colours) and draw markers in a UMG widget (or a top-down scene capture for a prettier map — OQ-UI-1).

### 6.12 Quest tracker (`createQuestTracker` `:693-719`, `refreshQuestTracker` `:5952-6106`, data `src/ui/QuestTrackerHUD.ts`)
* Refreshed every 250 ms and immediately on any `quest:*` event; re-rendered only when the signature
  `entries.map(questId|completed|summary).join('\n') + '|E:' + sortedExpanded + '|G:' + guidedId` changes.
* Entries: `getActiveQuests()` (active + completed-not-turned-in) sorted guided first, then this zone, then others,
  then `buildTrackerState` re-sorts **stably** main before side, unfinished before completed (`QuestTrackerHUD.ts:144-176`);
  max visible 5 (touch 3); overflow line `ui.questTracker.scrollIndicator {count}`.
* Per entry: title `"{➤ if guided}{[主线]|[支线]} {name}{ ✓ if completed}"` bold 12 px (touch 19) colour completed
  `#f1c40f`, main `#e8c252`, side `#a89060`; summary line (10 px `#aaaaaa`, completed `#f1c40f`):
  completed → `sys.tracker.completed`; one objective → `"{typeLabel} {cur}/{req}"`; several → `sys.tracker.doneCount
  {done, total}`; expanded entries add one line per objective `"    {typeLabel} {target name} {cur/req | ✓}"` (9 px,
  done `#66aa66`, else `#888888`).
* Line pitches desktop/touch: header 18/28, title 17/26, summary 14/24, objective 13/22, gap 3, scroll line 14/24.
  Frame height `ceil((y + 12)/8)·8`; hidden when no entries. Touch lines are squeezed horizontally to fit.
* Click on a title (hit area 200×22, touch lineW×26): `questSystem.setTracked(id)` (guide arrow) and toggle expanded.
* Header `ui.questTracker.header` 12 px Cinzel `#e8c77a`. Moves down under the labyrinth run widget when present.

### 6.13 Transient notices
* **Loot notice** (`handleItemPicked`, `:721-763`): on `ITEM_PICKED` (pickups, not purchases): tooltip-frame card
  accent = quality colour, 26 px item slot, label `"{display name}{ xN if qty>1}"` 12 px in quality colour (squeezed to
  fit). Newest inserted at index 0 at the anchor; others tween up by 40 px (×1.45 touch) in 150 ms; max **4** (oldest
  destroyed). Slide in from +24 px x with alpha 0→1 over 220 ms; after **3200 ms** fade 400 ms.
* **Achievement toast** (`:4976-5037`): 360×62 tooltip frame accent `0xffd98a` at (460, 60), medal + "★";
  `ui.achievement.toastUnlock {name}` 14 px `#ffd98a`; sub-line `"{description}  |  {stat label}+{v}  {title reward}"`;
  in: y→70, alpha 1, 400 ms Back.easeOut; at 3500 ms out: alpha 0, y→40, 300 ms. Simultaneous toasts overlap (Q20).
  Uses raw Chinese `ach.name/description/title` and `STAT_DISPLAY` labels (Q17).
* **Boss bar** (`handleBossBar` `:1757-1795`, `updateBossBar` `:1797-1807`): `BOSS_BAR` state or `null`; container at
  (640, 88), fade in 400 ms; frame `0x0a0604` α 0.9 + 2 px `0xc9a45a` border + diamond end caps; back `0x2a0606`; fill
  `0xb3121e` and shine (top third, `0xff6a5a` α 0.35) width `560·clamp(hp/max(1,maxHp),0,1)` updated when it changes
  by ≥ 0.001; name 18 px serif `#ffe2a8` above; epithet 12 px `#d9b98a` below.
* **Zone banner** (ZoneScene `showZoneBanner`): screen (0.5, 0.32) → (640, 230): zone name 28 px Cinzel `#c0934a`,
  `"Lv.{min}-{max}"` 16 px `#8a7a5a` 32 px below, two 120 px rules `0xc0934a`; fade in 800 ms, hold to 3000 ms, fade
  out 800 ms. Replaced by the chapter card on a first visit.
* **Level-up banner** (`showLevelUpBanner`, `ZoneScene.ts:5887-5911`): (640, 202): `zone.levelUp.text` 32 px `#ffd700`
  scale 0.5→1 400 ms Back.easeOut; `zone.levelUp.level {level}` 20 px `#ffcc00` 38 px below; at 2500 ms fade out and
  rise 20 px over 600 ms.
* **Quest-complete banner**, quest progress popups, death text: quests spec §5.8 / combat spec §13.3.
* **Pet medallion** (desktop): 34 px medallion left of the minimap with the active beast portrait and a "P" badge;
  click toggles the pet panel (`PetPanel.ts:114-140`) — later milestone.

### 6.14 Cinematic hide
UIScene's camera is invisible while a story beat plays (§1.5); everything (HUD + panels) disappears and returns
unchanged.

---

## 7. Panels

### 7.0 Common panel behaviour (`UIScene.ts:5501-5665`)
1. **Exclusivity**: opening any panel calls `closeAllPanels()` first (`:5635-5665`): closes inventory, shop (emits
   `SHOP_CLOSE`), stash, map, skill tree (+wheel handler, tooltip), character, homestead, quest log, companion, pets,
   socket, achievements, lore popup, audio, item tooltip, context popup, simple dialogue (emits `DIALOGUE_CLOSE`), quest
   card, dismissable abyss panels. **At most one panel is open.** Pressing a panel's key while it is open closes it
   (toggle). Socket panel is the only sub-panel that can sit over inventory.
2. **Depth layers** (`PANEL_STYLE.depth`, `:59-97`): backdrop 3999, panel 4000, sub-panel 4001, tooltip 5000, context
   menu 5001, confirm dialog 5002, toast 6000. UE: CommonUI layers HUD < Menu < Modal < Tooltip < Toast.
3. **Frame**: `createPanelBg(pw, ph, header = 36)` → UiKit `panel` frame (§8.3). Title centred in the 36 px header
   band (18 px Cinzel `#f0dcae`, stroke `#120b04` 3, shadow) with gold flourishes. Close medallion at (pw−22, 19).
4. **Open animation**: scale 0.92 → 1 and alpha 0 → 1, 150 ms Back.easeOut (skipped when rebuilding in place:
   `noPop`).
5. **Backdrop**: modal panels (shop, stash, simple dialogue, dialogue tree, quest card, mini-boss, lore) add a
   full-screen dim+vignette image (alpha 0.6–0.95) that blocks the world and **closes the panel on click**. Non-modal
   panels (inventory, character, skill tree, quest log, map, audio, achievements, companion, pets, homestead) have no
   backdrop; on desktop clicks on their empty areas fall through to the world (Q12).
6. **Touch adaptation** (`fitPanelForMobile`, `:5521-5547`): after build, scale the panel by `s = max(1, min(2,
   (1280−24)/pw, (720−24)/ph))`, re-centre it clamped to a 12 px margin, enlarge the close button to ≥ 84 px
   (texture 30 px), move geometry masks along; panel body swallows taps. Tooltips/popups are scaled ×1.45 and stay
   until the next tap (§7.2). Hover-only affordances are replaced by tap → card + explicit action buttons.
7. **Rebuild model**: every state change destroys and rebuilds the panel (`toggleX(); toggleX();` or `reopen`).
   Locale change refreshes open panels in place (`handleLocaleChanged`, `:884-927`; dialogue and socket panels are
   closed instead).
8. **Buttons fire on pointerdown** (UiKit, §8.4). Disabled buttons ignore input.
9. **No drag and drop** anywhere: all item moves are click/tap actions.

### 7.1 Inventory (`I`; `toggleInventory`, `:975-1155`) — rules: loot spec §7-§9
Panel 740×450 at (270, 12); title `ui.inventory.title {count, max: '100'}`.
* **Left — paper doll** (x 18, w 236, centre x 136): section header `ui.inventory.equipment` at y 52; 44 px slots on a
  grid (colGap 70, rowPitch 64, rowTop 92), column offsets −1/0/+1:
  `row0: helmet(0)`; `row1: weapon(−1), armor(0), offhand(+1)`; `row2: gloves(−1), belt(0), boots(+1)`;
  `row3: ring1(−1), necklace(0), ring2(+1)`. Empty slot shows a ghost icon (tint `0x6a6070`, α 0.28); label
  `ui.inventory.slot.<slot>` 10 px under it. Equipped item with sockets shows `"◆{n}/{max}"` 9 px `#8be9fd` top-right.
  Faint plinth ellipse behind the doll.
  - Hover equipped → tooltip (desktop). Click equipped (desktop): `maxSockets > 0` → socket panel, else **unequip**
    immediately. Touch: popup [`ui.socket.title` (if sockets), `ui.socket.unequip`].
  - Under the doll (y 336): divider, coin + gold 14 px, hint `ui.inventory.equipHint` 10 px.
* **Right — bag grid**: x 274, w 448, 10 columns × 5 rows = **50 per page**, gap 4, slot 41 px, top 66; header
  `ui.inventory.bag` at y 52. Empty cells α 0.7. Hover → tooltip; click → context popup (§7.2).
* **Toolbar** (y 307): pages when > 50 items (`ui.inventory.prevPage` / `pageLabel {current,total}` / `nextPage`
  56×24); right side `ui.inventory.sort` (76×26 secondary → `sortInventory()`) and `ui.inventory.destroy` (92×26
  danger → `destroyNormalItems()`, page reset to 0, **no confirmation**).
* **Equipment bonus** (y 333): header `ui.inventory.bonusHeader`; text of `getEquipmentStats()` non-zero entries as
  `"{getStatLabel(k)} +{v}{% if percent}"` joined by 3 spaces, 11 px `#9fd4ff`, ≤ 5 lines; none → `ui.inventory.bonusNone`.

### 7.2 Item tooltip, context popup, confirms (`:4019-4371`) — content: loot spec §15.2
* **Tooltip**: card 256 wide (tooltip frame, quality accent). Desktop: placed at cursor +16/−10, flipped left if it
  would leave the screen, clamped 4 px; if the item is equipment and something is worn in its slot, the worn item's
  card (tagged `ui.compare.equippedTag`) sits on the far side (gap 8). Hidden on pointer-out. Touch: single card ×1.45
  (fit to height), placed beside the context popup (or the finger), dismissed with the popup on the next tap.
* **Context popup** (bag click): actions — equipment: `ui.context.equip` (→ `equip(uid)`, stats invalidated);
  consumable/scroll: `ui.context.use` (→ `useConsumable`; heal/mana applied, clamped to max); always
  `ui.context.discard` (danger). Discard of `rare`/`legendary`/`set` → confirm (`ui.context.discardConfirmTitle`, item
  name in quality colour, `[确定]` danger / `[取消]`); others discard immediately. Desktop popup 108 wide, buttons 28
  high, at the cursor; touch 190 wide, buttons 80 high, font 24, placed 36 px beside the finger.
* **Sell confirm** (shop, legendary/set): centred 280-wide card `ui.shop.sellConfirm {name, price}`.

### 7.3 Character (`C`; `toggleCharacter`, `:2622-2711`)
Panel 380×512 at (450, 14); title `ui.character.title {className}`; subtitle `ui.character.subtitle {level, points}`
13 px (gold `#ffd98a` if points > 0).
* Six stat rows (pitch 42 from y 70, card 344×37): `ui.character.stat.<k>` 13 px + `.desc` 10 px; value
  `"{base}"` or `"{base} (+{gearBonus})"` 15 px (`#8be9fd` when bonus > 0); `+` button 26×26 success when free points
  > 0 → `freeStatPoints--`, `stats[k]++`, `recalcDerived()`, rebuild. (No undo / respec.)
* "Combat stats" (`ui.character.derivedHeader`) well with 7 rows (19 px): HP `"{ceil(hp)}/{maxHp}"` `#ff8a72`; MP
  `#7fb6ff`; attack `"{floor(baseDamage)}{ (+eq.damage)}{ +eq.damagePercent%}"`; defense `"{floor(defense)}{
  (+eq.defense)}"`; crit rate `(dexEff·0.2 + lckEff·0.5 + eq.critRate).toFixed(1)%`; crit damage `"{150 +
  eq.critDamage}%"`; gold `"{gold}G"`. The two crit lines are display approximations (no 75 % cap, no `lck·1 %` crit
  multiplier; real formula `combat-feel.md` §2) — Q19. `eq` = gear-only stats (not pets/achievements/blessings).

### 7.4 Skill tree (`K`; `toggleSkillTree` `:2028-2516`, tooltip `:2518-2620`) — rules: classes spec §7
Panel 660×520 at (310, 5). Header: title `ui.skillTree.title`; line `ui.skillTree.skillPoints {className, points}`
13 px at y 48 (gold if points > 0).
* **Tabs**: one per skill tree in class-definition order (3 per class), y 62, h 30, margin 16, gap 4, width
  `floor((628 − 4·(n−1))/n)` (206 for 3); label = localized tree name (13 px), learned-skill count badge; active tab tinted
  with the tree colour. Active tab index persists for the session.
* **Content**: area y 100 → 490 (h 390), clipped; cards 600×72 (gap 12), skills sorted by tier, connected by vertical
  arrows (learned: tree colour α 0.6 + glow; else `0x3a3a4e` α 0.15).
* **Card states**: maxed `{fill 0x2a2114, border 0xffd98a 2, glow 0xffc860, strip gold}`; learned `{0x1f1b24, border
  = tree colour 1.5, strip}`; can invest `{0x18201a, border 0x6fd35a, glow}`; locked `{0x121015, border 0x2e2a32}`.
  Contents: 42 px icon in a well framed by the damage-type colour (α 1 learned / 0.75 investable / 0.35 + grey tint
  locked); name 14 px (+ English name 10 px italic when different); level pips (8 px diamonds, ≤ 20, gap 10, clipped
  80 px from the right) + `"{lv}/{max}"`; stats row 11 px: locked → lock reason (`ui.skillTree.lock.playerLevel
  {level}` / `treePoints {current, required}` / `previousTier` / `noPoints`, colour `#c07a6a`), else
  `"{round(mult·100)}%  MP{mana}  CD{cd/1000 toFixed(1)}s  {damage type}"` at `max(1, level)`; synergy badge
  (`ui.skillTree.synergy`) when learned and any synergy source is learned; `+` button 26×26 when investable →
  `investSkillPoint` → `SKILL_LEVEL_CHANGED {skillId, level}` → rebuild.
* **Tooltip** (280 wide, right of the card or left if no room; touch: ×1.45 pinned to the right edge, dismissed by the
  next tap): header name (+ English); description; damage `ui.skillTree.tooltip.damage {value, type}`, cost, cooldown,
  range, AoE radius (toFixed 1), crit bonus, stun (s), buff `{stat, value·100, duration s}`; synergy header + per
  synergy `{name, perLevel (‰→%), bonus}`; if not maxed: next-level block with damage `(+Δ%)`, changed cost/cooldown.
* Scrolling: wheel `+dy·0.5`; touch drag (threshold 12 px) scrolls and cancels the pending tap; on touch the `+` action
  fires on pointer-up only if no drag happened. Off-screen cards are made non-interactive. Thumb scrollbar 6 px at the
  right edge. Footer hint `ui.skillTree.footer` / `footerTouch`.
* Colours (export): trees `combat_master 0xd4a017, guardian 0xf1c40f, berserker 0xcc3333, fire 0xe74c3c, frost
  0x5dade2, arcane 0x8e44ad, assassination 0x27ae60, archery 0xcc8844, traps 0xff6600`; damage types `physical
  0xcccccc, fire 0xff6633, ice 0x66ccff, lightning 0x5dade2, poison 0x33cc33, arcane 0xbb77ff`.

### 7.5 Quest log (`J`; `:3727-4017`, lore tab `:5418-5499`) — content: quests spec §5.5
Panel 720×520 at (280, 24); tabs (140×28 at y 46): `ui.questLog.tab.active` (accent `0x5a9fe0`), `.completed`
(`0x6fd35a`), `.lore` (`0xd4a54a`). List x 16 w 250, rows 28 from y 86, **13 per page**, sorted main first then by
quest level; selected row highlighted; detail pane x 282 w 420: description, objectives with progress bars
(done `0x5cc04a`, else `0x3a7fd0`), rewards (`ui.questLog.rewardExp/Gold/Items`), prerequisites. Detail objective names
and prerequisite names are raw (Chinese) strings (Q18).

### 7.6 World map (`M`; `toggleMap`, `:1936-1998`)
Width `max(480, n·98 + (n−1)·22 + 56)` (634 for the 5 zones of `MapOrder`), height 236, at y 80; title
`ui.worldMap.title`. One 98×70 card per zone in `MapOrder` (`emerald_plains, twilight_forest, anvil_mountains,
scorching_desert, abyss_rift`), linked by gold arrows; name (12 px, 11 if it wraps) and `"Lv.{min}-{max}"`; the
current zone card is highlighted with a bobbing pin (±3 px, 600 ms yoyo). **Read-only** (no travel). Footer
`ui.worldMap.closeHint` (desktop). Ch1 build: show only zones that exist (OQ-UI-2).

### 7.7 Shop / blacksmith (`openShop`, `:1163-1423`) — rules: loot spec §12-§13
Modal (backdrop α 0.6), 780×480 at (250, 40), divider at x 356. Title `ui.shop.blacksmith` / `ui.shop.shop`.
* Left: blacksmith has tabs `ui.shop.tabBuy` | `ui.shop.tabForge` (accents gold / `0xff8a2a`); merchant shows
  `ui.shop.itemList`. Ware rows 40 px from y 68 (76 with tabs): icon, name, coin + price (`sellPrice × 3`), buy button
  52×24 success (disabled if unaffordable, then text dimmed). Buyback section below the wares; gold line at the bottom.
  Rows that do not fit above y 426 are dropped (loot spec Q7).
* Right: "your bag" grid 8 columns, **40 per page**, gap 5; click/RMB sells (normal/magic/rare immediately,
  legendary/set → confirm); touch tap → card + `ui.shop.sellAction {price}` button. Forge tab: bag click puts gear on
  the anvil (non-gear dimmed), RMB still sells on desktop. Hint line at the bottom (`ui.shop.sellHint(Touch)`,
  `ui.forge.bagHint(Touch)`).
* Close (button or backdrop) → `SHOP_CLOSE {npcId}`.

### 7.8 Stash (`openStash`, `:1832-1934`) — rules: loot spec §10
Modal (backdrop α 0.6), 820×480 at (230, 40), divider x 420. Left stash grid, right bag grid, each 8×5 = 40 per page,
gap 5, top 72, own pagination. Headers `ui.stash.stored {count, cap}` (cap = 80 + homestead `stashSlots`) and
`ui.stash.bag {count}`, each with a `ui.stash.sort` button 56×22. Stash cells beyond capacity shown α 0.25 (locked).
Desktop click moves the item across immediately (`moveFromStash` / `moveToStash(uid, cap)`; bag full → log
`ui.stash.bagFull`); touch tap → card + `ui.stash.withdraw` / `ui.stash.deposit`. Hint `ui.stash.hint(Touch)`. Opened
by the stash-keeper NPC (`UI_TOGGLE_PANEL {panel:'stash', npcId}`); close emits `SHOP_CLOSE {npcId}`.

### 7.9 Socket panel (`openSocketPanel`, `:4373-4569`) — rules: loot spec §9.4
Sub-panel 400×420 at (440, 50) over the inventory.

### 7.10 NPC conversation panels — runtime: quests spec §5.3 / §7
* Routing on `NPC_INTERACT` (`handleNpcInteract`, `:823-849`): quest NPC with actionable quests (available or
  turn-in) → **quest card**; else dialogue tree if the NPC has one; else **simple dialogue**.
* **Quest card** (`:3100-3349`): modal α 0.7, 420 wide, height fits content, centred (y ≥ 10).
* **Dialogue tree** (`:3398-3715`): modal α 0.7, 480 wide, max 520 high, scrollable text; per-NPC state
  `{visitedNodes, choicesMade}` kept in UIScene and saved as `dialogueState` (Q26).
* **Simple dialogue** (`openDialogue`, `:3041-3084`): modal α 0.6, 400 wide; title = NPC name; body 14 px centred;
  action buttons (pw−64)×32 stacked (first primary) — each runs its callback then closes; hint
  `ui.dialogue.closeHint`; close emits `DIALOGUE_CLOSE`.
* Shops and stash are opened directly by `SHOP_OPEN` / `UI_TOGGLE_PANEL` (merchant/blacksmith/stash NPC types,
  `ZoneScene.ts:4155-4182`). Each interaction first logs the NPC's `dialogue[0]` line (`info`).

### 7.11 Mini-boss pre-fight dialogue (`showMiniBossDialogue`, `:5297-5361`)
Modal backdrop α 0.95; 500×260 centred; red accent frame (`0xe0503c`); title `"⚔ {bossName} ⚔"` `#ff8a72`; every line
of the (linear) tree — follow `startNodeId → nextNodeId` until `isEnd` — as italic quoted 14 px text; button
`ui.miniBoss.fight` (danger 180×34). Button **or** backdrop click → close + `onDismiss()` (the fight starts).

### 7.12 Lore popup (`showLoreText`, `:5365-5416`)
Modal α 0.75; 440×240 centred; title = entry name; zone name 11 px italic; text 13 px `#e8dcc0` in a parchment well
(border `0x5a4a30`); close button or backdrop; **auto-closes after 8000 ms**.

### 7.13 Audio settings (`O`; `toggleAudioSettings`, `:5216-5295`)
420×170 centred; title `ui.audio.title`. Two rows (y 66 / 114): label (`ui.audio.bgm` / `ui.audio.sfx`) 14 px at x 22;
slider track 180×8 at x 96 (gold fill, handle r 8 `0xe8c77a`), percentage text; hit area 194×28 — press sets the value
and drags while held; value `clamp((pointerX − panelX)/scale − 96, 0, 180)/180`; changes apply and persist immediately
(`setMusicVolume` / `setSFXVolume`). Mute toggle 64×24: label `ui.audio.muted` (`#ff8a72`) / `ui.audio.unmuted`
(`#a8f090`). This is the **only in-game settings panel**; language is menu-only; render quality has no UI (OQ-UI-5).

### 7.14 Achievements (`V`; `toggleAchievement` `:5039-5132`, row `:5134-5214`)
560×540 at (360, 10). Summary at y 54: progress well 200 wide, `ui.achievement.unlocked {count, total}`, and (if any)
`ui.achievement.currentTitle {title}` = title of the **last unlocked achievement in definition order**. List from y 72,
rows 56 (7 visible); wheel scrolls by one row; position text `"{first}-{last}/{total}"`. Row: medal (★ gold / ☆ grey),
name 13 px, description ≤ 2 lines, progress bar 96×8 + `"{min(cur,req)}/{req}"`, reward text (`"{stat}+{v}"`,
`ui.achievement.titleReward {title}`) squeezed to 110 px. Raw Chinese names (Q17).

### 7.15 Companion (`U`; `buildCompanionPanel`, `:4583-4635`)
520×540 at (380, 10). Mercenary section: `ui.companion.noMerc` hint, or status `ui.companion.mercStatus {name, type,
level, hp, maxHp}` / `ui.companion.mercDead`. Ley-beast section: active beast `"{name}  Lv.{n}  · {ui.pet.active}"` or
`ui.pet.owned {count, total}`, button `ui.pet.openPanel` → pets panel. The hire UI (`renderHirePanel`,
`renderMercenaryInfo`) exists but is **never called** — mercenaries cannot be hired in the web build (Q21).

### 7.16 Abyss Labyrinth UI (later milestone)
`src/ui/AbyssRunUI.ts`: tier picker, boon cards (mandatory, blocks panels), run HUD widget under the minimap (the
quest tracker shifts down), run summary; keyboard shortcuts in §5.1; contract in `EventBus.ts:89-137`.

### 7.17 Homestead (`H`) and Ley-beasts (`P`) (later milestones)
* Homestead panel 560×560 at (360, 8) with pages `buildings / garden / workshop / caravan / altar`
  (`src/ui/HomesteadPanel.ts`); reopens on the last page; `UI_TOGGLE_PANEL {panel:'homestead', page}` switches page.
* Pet panel 660×540 at (310, 10) (`src/ui/PetPanel.ts`).
Specs for these belong to the homestead/pets milestone; Ch1 only records embers and the `pet_sprite` ownership.

---

## 8. UiKit visual style (`src/ui/UiKit.ts`, `docs/art-direction.md` "UI")

Language: dark-fantasy ARPG in the cartoon style — **opaque** panels with carved dark-iron frames, gold filigree
corners, warm parchment headings, light from the upper left (warm highlights, cool/dark shadows), no see-through panels
over the world. Every frame/button/slot is baked once per size+variant and reused.

### 8.1 Palette (`UI_COLORS`, `:22-43`; export verbatim)
| Token | Value | Use |
|---|---|---|
| `parchment` | `#f0dcae` | panel titles, active tabs |
| `heading` | `#e8c77a` | section headers, tracker header |
| `gold` / `goldNum` | `#d4a54a` / `0xd4a54a` | frames, rules, slider fill |
| `goldBright` | `#ffd98a` | highlights, maxed skills, points available |
| `text` | `#e0d8cc` | body text |
| `textSoft` | `#bfb4a2` | secondary text |
| `muted` | `#9a8f80` | hints, footers |
| `dim` | `#6e665c` | disabled / empty |
| `faint` | `#4f4940` | |
| `good` / `bad` / `info` | `#7ed36a` / `#ff6b5a` / `#7fb6ff` | |
| `goldDarkNum` | `0x5a3a10` | medal fill |
| `ironNum` / `ironLightNum` | `0x3a3540` / `0x6d6573` | |
| `cardNum` / `cardHoverNum` / `wellNum` | `0x18151b` / `0x221d25` / `0x0c0b0e` | cards, wells |

Item quality (`QUALITY_HEX`, `:45-51`): normal `#c8c8c8`, magic `#4f8cff`, rare `#ffd84a`, legendary `#ff8a2a`,
set `#3ecf6a`. (A second, older palette — `getQualityTextColor` `#5dade2/#f1c40f/#e67e22/#2ecc71`, `:5722-5730` — is
used only for buyback names; port: use `QUALITY_HEX` everywhere.)

### 8.2 Typography
* Body: `"Noto Sans SC", sans-serif` (`UI_FONT`); titles: `"Cinzel", "Noto Sans SC", serif` (`UI_TITLE_FONT`); boss
  bar and story text: `"Noto Serif SC"`. Default text has a black stroke 2–3 px; titles add a 2–4 px drop shadow.
* Sizes used: titles 18 (panels) / 20–22 (menu) / 58 (logo); body 11–14; small 9–10; HUD numbers 13; skill cooldown 15.
* **UE**: composite fonts `F_AbyssBody` = Noto Sans SC (Latin + SC) with Noto Sans TC fallback for zh-TW, `F_AbyssTitle`
  = Cinzel with Noto Sans SC/TC fallback for CJK, `F_AbyssSerif` = Noto Serif SC/TC. Outline material/`FSlateFontInfo`
  outline 2 px black. Subset CJK fonts to the glyphs used by the string tables + GB2312/Big5 common sets (pipeline
  script) to keep mobile packages small. All OFL.

### 8.3 Frames (`frameTexture`, `:307-440`)
Variants with outer margin (texture extends beyond the rect for the shadow): `panel` 14, `tooltip` 10, `plate` 8.
Corner radius panel 7 / tooltip 5 / plate 6; iron band width 5 / 3 / 3.5. Layers: drop shadow (panel blur 14, y 5;
others blur 8, y 3) → body vertical gradient `#241f26 → #19161c (0.45) → #0f0d11` with speckle noise, warm radial
light at upper-left (`rgba(255,190,110,0.07)`), vignette to `rgba(0,0,0,0.5)` → optional header band (gradient
`rgba(120,78,30,0.34) → rgba(70,42,16,0.2) → rgba(20,12,6,0.35)`, gold rule with diamond at its bottom) → iron frame
(dark outline `#050407`, iron gradient band `#77707c → #4a444f → #221e26`, light bevel `rgba(255,240,220,0.22)`, dark
inner edge `#08070a`, accent hairline in `accent` colour α 0.6 (tooltip 0.85), default accent `0xd4a54a`) → panels:
rivets every 110 px along the band and corner ornaments with a gem (default `0xb3202a`); tooltip/plate: smaller corner
ornaments without gems. Optional body alpha (plates 0.9).
**UE**: generate these as 9-slice textures with a Python (Pillow/Cairo) script that reproduces the layers at 2× and
3× scale (or as UMG materials: rounded-box SDF + gradients); corner ornaments as separate overlay images.

### 8.4 Buttons (`UiButton`, `:509-701`)
Variants (top / bottom gradient, border, text, hover text):

| Variant | Top | Bottom | Border | Text | Hover text |
|---|---|---|---|---|---|
| primary | `#6b4a1e` | `#2c1d0b` | `#d4a54a` | `#ffe7b0` | `#fff4d6` |
| secondary | `#3a353f` | `#17151a` | `#8a7a64` | `#e0d8cc` | `#fff4e0` |
| danger | `#5a1c16` | `#220a08` | `#c0503c` | `#ffb8a6` | `#ffe0d6` |
| success | `#27451f` | `#0e1b0c` | `#6fb35a` | `#c6f0b4` | `#eaffe0` |
| ghost | `#1d1a20` | `#141216` | `#4d4552` | `#bfb4a2` | `#f0dcae` |

States: normal (drop shadow blur 3 y 2, top sheen `rgba(255,240,210,0.16)`), hover (top +18 % lighter, bottom +12 %,
border +30 %, coloured glow blur 8), pressed (gradient inverted, top −25 %, 1 px down, label +1 px), disabled
(gradient `#2a282c → #141316`, border `#3d3a40`, text `#6a635c`). Radius `min(5, h/3)`; dark outline + 1.2 px border; small diamond studs at
both ends when w ≥ 70 and h ≥ 22. Label default 13 px (panels pass 12), stroke 2, centred, `\n` allowed.
**Fires `onClick` on pointer-down** (port: fire on press for parity on desktop; on touch inside scroll views fire on
release without drag, as the skill tree does). Legacy labels `"[…]"`/`"【…】"` are stripped (`btnLabel`, `:207-209`).
Close button: 30 px round iron medallion with a red X (`#e0634e`, hover `#ffb09a` + red glow), press scale 0.9.

### 8.5 Other primitives
* **Tabs** (`tabTexture`, `:703-743`): rounded top; active = accent-tinted gradient, accent border, 2.5 px accent
  underline, sheen; inactive `#1a171d → #0f0d11`, border `#3a343f`. Label 13 px, active parchment, inactive `#8a8290`.
* **Divider** (gold rule with optional centre diamond, 12 px tall), **vertical divider** (gold fading at both ends),
  **title flourishes** (46×12 curl + diamond, 8 px from the text on both sides), **section header** (12 px bold label
  in `heading` colour + fading gold rule to the right).
* **Card** (`drawCard`, `:851-879`): radius 4; optional glow (4 px α 0.12 + 2 px α 0.22); shadow (black α 0.45, +1.5 y);
  fill (default `0x18151b`); top sheen `0xfff0d8` α 0.045; bottom shade; optional 3 px left strip; border 1 px
  (default `0x3a343f`).
* **Well** (`drawWell`): `0x060508` rounded, inner top shadow, border `0x3a343f`. **Bar fill**: lighten 22 / darken 30
  gradient + top highlight α 0.18.
* **Item slot** (`slotTexture`, `:909-984`): radius 4; radial gradient tinted by quality (non-normal: quality α 0.28
  centre), recessed top shadow, quality-coloured frame (1.6 px non-normal, 1.2 normal; empty `#4a4350` α 0.85),
  lower-right bevel; rare/legendary/set add corner ticks; legendary/set and hover add an outer glow.
* **Bars**: frame trough 3 px padding, pill shaped, gold hairline; fill gradient (+35 % top, −45 % bottom) with top
  highlight; EXP tick overlay.
* **Orb pieces**, **skill slot** (hover glow `rgba(255,200,110,0.8)`), **key badge**, **HUD plate** (iron-framed
  plate, `#2c2730 → #1a171d → #0e0c10`), **minimap frame** (iron ring + vignette + gold top gem), **coin**, **pip**
  (diamond), **backdrop** (radial `rgba(0,0,0,0.25)` → `0.7`), **joystick** (translucent base, gold ring, iron thumb with
  gold gem), **medallion** (radial colour gradient + rim).
* HUD icons (`src/ui/HudIcons.ts`): `inventory, character, skills, map, homestead, quest, pets, auto, loot, log` —
  inked cartoon icons (96 px). Port: bake as PNG icons in the asset pipeline.

---

## 9. Settings and preferences (summary)
The web game has no unified settings screen. Where each preference lives today:

| Preference | UI | Persisted in | Notes |
|---|---|---|---|
| Language | main menu → Language (§1.2) | `localStorage abyssfire_locale` | not changeable in game |
| Music / SFX volume + mute | in-game `O` panel (§7.13) | `localStorage abyssfire_audio` | defaults 0.15 / 0.3; the jukebox pause is a non-persisted temp mute |
| Render quality / resolution | none (`?quality=`, `?res=`, `localStorage abyssfire_render_quality`) | localStorage | auto-selected by device (render-only) |
| Auto-combat | `Tab`, HUD button, touch toggle | save `settings.autoCombat` | per character |
| Auto-loot mode | HUD button, touch toggle | save `settings.autoLootMode` | per character |
| Difficulty | Continue → difficulty selector (§1.2) | save `difficulty` | per character |
| Key bindings | fixed (help panel lists them) | — | |

**Port (OQ-UI-4/5)**: one Settings panel reachable from the main menu and the in-game system menu: Language, Music,
SFX, Graphics quality (UE scalability), Touch-control scale (mobile), Controls reference (and later rebinding);
device-level settings in `Settings.json` (§3.8), character-level toggles stay in the save.

---

## 10. i18n (`src/i18n/`)

### 10.1 `t(key, params?)` (`index.ts:53-82`)
1. `value = locale[current][key]`.
2. Missing and current is `zh-TW` → `locale['zh-CN'][key]`.
3. Still missing and current ≠ `en` → `locale['en'][key]`.
4. Still missing → return **the key itself** (callers detect "missing" by comparing the result to the key).
5. If `params`: for each `(name, value)`, replace **every** `{name}` with `String(value)` (regex `\{name\}` global).
   Unknown placeholders stay literally (`{x}`). JS `String.replace` interprets `$&`, `$1`… inside the replacement
   value (Q27) — the port must replace literally.
* Values may contain `\n` (rendered as line breaks, e.g. `ui.hud.autoCombat.on = "AUTO\nON"`).
* Numbers are passed pre-formatted by callers (`toFixed(1)` for seconds, raw integers, no thousands separators).

### 10.2 Locales and switching (`index.ts:14-112`)
* Supported: `zh-CN` (default, source of truth), `zh-TW` (generated), `en`. Persisted in `localStorage
  'abyssfire_locale'`; invalid stored value → default.
* `setLocale(id)`: unsupported → ignore; same as current → no-op; else set, persist, emit `LOCALE_CHANGED(id)`.
* Listeners: MenuScene re-renders the active panel + title; UIScene refreshes open panels (§7.0.7); ZoneScene updates
  world labels and the mobile controls relayout. Only the menu offers switching.
* **Locale data** = one flat `Record<string,string>` per locale, assembled by spreading modules
  (`locales/zh-CN.ts:12-2249`, `en.ts`): main table + `QUEST_STORY_*` + `STORY_*` + `ABYSS_RUN_*` + `HOMESTEAD_*` +
  `PETS_*`. Currently ≈ 3,290 keys per locale.

### 10.3 zh-TW generation (`converter.ts`)
`zh-TW[key] = convertToTraditional(zh-CN[key])` for every key at startup (`index.ts:19-31`):
1. Char-by-char mapping from two equal-length codepoint strings `SC`/`TC` (≈ 1,000 pairs, `:11-12`); unmapped chars pass
   through. A length mismatch throws at module load.
2. Phrase fix-ups applied to the result, in order: `日志 → 日誌`, `鍛造日志 → 鍛造日誌`, `雜志 → 雜誌` (`:54-58`).
* **Port**: run the converter at **export time** and ship `zh-TW.json` (no runtime conversion); keep the SC/TC table
  + fix-ups in the export tool and a few unit vectors (e.g. `铁剑 → 鐵劍`, `战斗日志 → 戰鬥日誌`).

### 10.4 Key namespaces and conventions
Dot-separated, lowerCamel segments; ids embedded verbatim (`data.item.<baseId>.name`). Namespaces (counts ≈ zh-CN):

| Prefix | Content |
|---|---|
| `boot.*` | loading screen |
| `menu.*` | main menu, class select, help, jukebox, credits, difficulty, language |
| `ui.*` | HUD (`ui.hud`), panels (`ui.inventory`, `ui.character`, `ui.skillTree`, `ui.questLog`, `ui.shop`, `ui.forge`, `ui.stash`, `ui.socket`, `ui.tooltip`, `ui.compare`, `ui.context`, `ui.dialogue`, `ui.questCard`, `ui.questTracker`, `ui.worldMap`, `ui.audio`, `ui.achievement`, `ui.companion`, `ui.pet`, `ui.homestead`, `ui.abyss`, `ui.miniBoss`), stat labels `ui.stat.<statKey>`, quality `ui.tooltip.quality.<q>`, compass `ui.compass.<dir>` |
| `zone.*` | gameplay log/banners (`zone.combat`, `zone.teleport`, `zone.save`, `zone.levelUp`, `zone.death`, `zone.quest`, …) |
| `sys.*` | system messages and labels from systems (`sys.mobile`, `sys.quest`, `sys.tracker`, `sys.inventory`, `sys.pet`, `sys.difficulty`, …) |
| `data.*` | content names/texts: `data.item.<id>.name/.desc`, `data.affix.<id>.name`, `data.set.<id>.name/.bonus.<i>`, `data.skill.<id>.name/.desc`, `data.skillTree.<tree>`, `data.class.<id>.name`, `data.zone.<id>`, `data.monster.<id>`, `data.npc.<id>.name/.dialogue.<i>`, `data.quest.<id>.name/.desc/.offer/.progress/.complete`, `data.questTarget.<id>`, `data.dialogue.<npc>.<node>.text/...`, `data.lore.<id>.name/.text`, `data.achievement.<id>.name/.desc/.title`, `data.difficulty.<d>.*`, `data.statusEffect.<t>`, `data.eliteAffix.<id>`, `data.damageType.<t>`, `data.pet.<id>.*`, `data.homestead.<id>.*`, `data.mercenary.<t>.*`, `data.hiddenArea.<id>.*`, `data.subDungeon.<id>.*`, … |
| `story.*` | prologue, chapter cards, cutscenes, boss intros, epilogue, credits |
| `dungeon.*`, `homestead.*` | labyrinth and Ember Tower (later milestones) |

### 10.5 Data accessors (`src/i18n/gameAccessors.ts`) — "key or fallback" pattern
Every accessor builds a key, calls `t`, and returns the fallback (the data file's Chinese `name`, or the id) when the
result equals the key. Accessors: `getItemBaseName(baseId)` (en: key, else `base.nameEn`, else `name`),
`getItemBaseDesc`, `getItemDisplayName(item)` (affix composition — loot spec §15.1), `getAffixName`, `getStatLabel
(ui.stat.<k> → STAT_DISPLAY label)`, `isStatPercent`, `getQualityLabel`, `getSetName`, `getSetBonusDesc`,
`getClassName (→ id)`, `getDirection(dc, dr)` (8-way from `atan2(dr, dc)` in 45° sectors centred on east: `ui.compass.
east/southeast/south/southwest/west/northwest/north/northeast`), `getSkillName/Desc`, `getSkillTreeName`,
`getDamageTypeName`, `getQuestName/Desc`, `getQuestStory(id, phase) (→ '')`, `getZoneName(id, fallback ?? id)`,
`getMercenaryName/Desc/TypeLabel`, `getBuildingName/Desc`, `getPetName/Desc`, `getAchievementName/Desc/Title`,
`getLoreName/Text`, `getNpcName`, `getQuestTargetName(id, fallback, labelKey?)` (labelKey first), `getMonsterName`,
`getPetStatLabel`, `getHiddenAreaName/DiscoveryText`, `getStatusEffectName`, `getEliteAffixName`,
`getSubDungeonEntranceName`, `getRescueNpcName`, `getSubDungeonName`.
**Port**: the exported string tables are complete for every content id the tests cover, so the core accessor can be one
generic `NameOf(category, id, fallback)`; keep the fallback for safety.

### 10.6 Invariants enforced by tests (`src/__tests__/i18n-full-validation.test.ts`, `i18n.test.ts`)
zh-CN and en have identical key sets; zh-TW resolves every zh-CN key; namespaces `menu ≥ 60, boot ≥ 4, ui ≥ 250,
zone ≥ 70, data ≥ 1000, sys ≥ 100` keys; all values non-empty; every content id (monsters, zones, classes, skills,
quests, NPCs, items, affixes, sets, legendaries, achievements, mercenaries, status effects, elite affixes,
difficulties, lore, homestead, pets, dialogue trees, random events, story decorations, damage types, skill trees,
hidden areas, sub-dungeons, mini-boss dialogues) resolves in all 3 locales; no hardcoded Chinese in scene/system code;
every templated key resolves without leftover `{…}`. **Port**: re-implement these as a CI check over the exported JSON
+ content tables (core test target), and a UE editor validator that no widget uses a literal string.

### 10.7 UE approach (recommended)
* The **core** owns `Localizer` (loads `zh-CN.json`, `en.json`, `zh-TW.json`; `T(key, params)` with the exact §10.1
  semantics) because game logic composes localized strings (log lines, item names).
* Core events that the UI renders (log lines, banners, toasts) carry `{key, params}` *and* the resolved text; the
  presentation calls `FText::FromString(core.T(...))`. On `LocaleChanged` the HUD/panels re-pull their texts (same as
  the web refresh).
* Do not use UE's gather/compile localization pipeline for game text (single source of truth = the JSON tables); UE
  engine strings (platform dialogs) may use it.
* Mobile rotate card is not needed (orientation locked).

---

## 11. Core API proposal (C++20, no exceptions / RTTI) — this area
```cpp
namespace abyss {
// ---- Save ----
inline constexpr int kCurrentSaveVersion = 3;            // 4 once UE-only fields are added (§3.3)
enum class SaveError : uint8_t { None, NotFound, ParseFailed, VersionTooNew, Io };
struct SaveData { /* fields of §3.2, names identical, std::optional for "opt" */ };
bool MigrateRaw(JsonDoc& raw);                           // v1→v2→v3 (+ v3→v4) on the generic tree
SaveError ParseSave(std::string_view json, SaveData& out);   // migrate + typed parse + §3.3 normalisations
std::string SerializeSave(const SaveData& s);            // stable key order = §3.2
SaveData BuildSave(const Session& s, int64_t unixMs);    // §3.4 incl. labyrinth override
void ApplySave(Session& fresh, const SaveData& s, const ZoneCollision& z, Log& log);  // §3.5 (FIX Q8, Q35: hp<=0 → respawn)
// ---- Hero life / save gating (§3.4, §3.5, §5.1.1; combat-feel §13.3) ----
enum class HeroLife : uint8_t { Alive, Dying };
bool CanSave(const Session& s);                          // Alive && hp > 0 && !transitioning
void RequestSave(Session& s);                            // Dying → savePending (flushed after respawn); else save now
void ResolvePendingDeath(Session& s, ZoneRuntime& z);    // finish a Dying hero's respawn now (menu/quit/background)
std::optional<TileI> FindNearestWalkablePosition(float col, float row, const ZoneCollision& z,
                                                 std::span<const TileI> camps);      // §3.7
struct ISaveStorage { virtual bool Read(std::string_view slot, std::string& out) = 0;
                      virtual bool Write(std::string_view slot, std::string_view bytes) = 0;
                      virtual bool Remove(std::string_view slot) = 0;
                      virtual void List(std::vector<SlotInfo>& out) = 0; virtual ~ISaveStorage() = default; };

// ---- Session / flow ----
class Session { /* inventory, quests, homestead, pets, achievements, mercs, story, soulEcho, abyss,
                   hero, difficulty, completedDifficulties, miniBossSeen, loreCollected, hiddenAreas,
                   dialogueState, exploredZones */ };
struct ZoneEntryRequest { std::string mapId; std::optional<TileF> target; /* subDungeon, parent, dungeonRun */ };
DifficultyStates GetDifficultyStates(std::span<const Difficulty> completed);         // §1.2
std::vector<Difficulty> DeriveCompletedDifficulties(Difficulty d, std::span<const Difficulty> list);
bool ShouldShowDifficultySelector(Difficulty d, std::span<const Difficulty> completed);

// ---- Input ----
Vec2 ScreenDirToTile(Vec2 screen);                       // (sx+sy, -sx+sy)
struct InputFrame { Vec2 moveTile; std::array<bool,6> skill{}; bool dodge{}, targetCycle{}, toggleAuto{},
                    townPortal{}; std::optional<ClickQuery> click; bool clickHeld{}; };
void ApplyInput(Session&, ZoneRuntime&, const InputFrame&, double nowMs);  // §5.2-§5.5 rules

// ---- HUD view model (polled by UMG every frame) ----
struct HudViewModel { float hp, maxHp, mana, maxMana, spirit, spiritMax; bool resonating; float resonanceMs;
                      int level; int64_t exp, expToNext, gold; int embers; bool towerUnlocked;
                      std::array<SlotCd,6> skills; float dodgeCdMs; bool autoCombat; AutoLootMode autoLoot;
                      std::optional<TargetInfo> target; std::string zoneName; };
void BuildHud(const Session&, const ZoneRuntime&, double nowMs, HudViewModel& out);

// ---- i18n ----
enum class LocaleId : uint8_t { ZhCN, ZhTW, En };
class Localizer { public: bool Load(LocaleId, std::string_view json); void SetLocale(LocaleId);
                  LocaleId Current() const; std::string T(std::string_view key,
                  std::span<const std::pair<std::string_view, std::string_view>> params = {}) const; };
}
```

---

## 12. QUIRKS (web behaviour → port decision)
| # | Quirk | Source | Recommendation |
|---|---|---|---|
| Q1 | `GameSession` (and UIScene dialogue state) is never reset: Esc → menu → **New Game** keeps the old inventory, quests, achievements, mercenary, etc.; **Load** overwrites most but not mercenary/dialogue/mini-boss/lore sets when absent | `ZoneScene.ts:399`, `:4384-4415` | **FIX**: fresh session on every New Game / Load |
| Q2 | New Game silently overwrites the single autosave on first zone entry | `ZoneScene.ts:703`, `MenuScene.ts:1363` | **FIX**: confirm dialog when a save exists, or slots (OQ-SAVE-1) |
| Q3 | Only the `autosave` record exists; `quickSave/listSaves/deleteSave` unused | `SaveSystem.ts:188-208` | decide (OQ-SAVE-1) |
| Q4 | No timed autosave; closing the tab loses everything since the last trigger | §3.6 | **FIX**: 60 s timer + app background + quit |
| Q5 | `settings.musicVolume/sfxVolume` written as constants 0.5/0.7 and never read; real audio settings live in localStorage | `ZoneScene.ts:4283` | keep fields for compatibility; real settings in `Settings.json` |
| Q6 | `exploration` is always `{}` (fog not persisted; `FogOfWarSystem` not instantiated) | `ZoneScene.ts:196, 4276, 4372` | keep the field; persist explored tiles only if fog ships |
| Q7 | Autosave inside a sub-dungeon stores the sub-dungeon id/position; loading falls back to `emerald_plains` with those coordinates | `ZoneScene.ts:337-358, 4254` | **FIX**: save parent zone + entrance (v4 field or reuse `currentMap`) |
| Q8 | Load clamps HP/MP to the **gear-less** maximum (equipment restored afterwards) | `ZoneScene.ts:4311-4314` | **FIX** order |
| Q9 | Every loaded item becomes `identified` | `ZoneScene.ts:4335-4343` | keep (identification vestigial) |
| Q10 | `Esc` returns to the main menu immediately, even with a panel open, no confirm | `ZoneScene.ts:2203-2205` | **FIX**: Esc closes the top panel; with none open, opens a system menu (Resume / Settings / Return to menu) |
| Q11 | Movement, skills and hotkeys stay active under panels and dialogs | §5.1 | decide (OQ-INPUT-2); recommend modal panels block gameplay input |
| Q12 | Desktop clicks on non-interactive parts of non-modal panels walk the hero | `UIScene.ts:5501-5513` | **FIX**: UI consumes all clicks inside its widgets |
| Q13 | Keyboard speed 1.8 tiles/s vs click-move 120 iso px/s (2.65–5.3 tiles/s, direction-dependent) | §5.2 | decide (OQ-INPUT-1); recommend one ground speed `moveSpeed/36` |
| Q14 | `worldToTile` uses `floor` → half-tile bias of click targets | `IsometricUtils.ts:21-24` | **FIX**: nearest tile from the 3D trace |
| Q15 | Joystick/stick magnitude ignored; joystick has no dead zone | §5.2 | keep full speed; add a 0.15 dead zone on the joystick |
| Q16 | UIScene restarts when a skill reaches level 1 → combat log and loot notices lost | `UIScene.ts:808-816` | **FIX**: rebuild only the skill bar |
| Q17 | Achievement panel/toast use raw Chinese `name/description/title` and `STAT_DISPLAY` labels | `UIScene.ts:4999-5011, 5172-5205` | **FIX**: `getAchievementName/Desc/Title`, `getStatLabel` |
| Q18 | Quest log detail uses raw `obj.targetName` and prerequisite `quest.name` | `UIScene.ts:3970, 4006` | **FIX**: accessors |
| Q19 | Character panel crit rate/damage display ignores the 75 % cap and `lck` crit multiplier | `UIScene.ts:2687-2694` | **FIX**: show the real formula |
| Q20 | Several achievement toasts at once overlap | `UIScene.ts:4976` | **FIX**: queue/stack |
| Q21 | Mercenary hire UI is dead code | `UIScene.ts:4637-4954` | later milestone decision |
| Q22 | Rotate card has only zh-CN/en text | `MobileShell.ts:13-16` | n/a in UE (orientation locked) |
| Q23 | Town portal: movement not frozen during the 1.5 s channel; death does not cancel the teleport | `ZoneScene.ts:5801-5884` | **FIX**: cancel on death; keep movement allowed but cancel on move? (OQ-INPUT-4) |
| Q24 | Minimap shows unexplored terrain and is grid-aligned (45° off the camera) | §6.11 | decide (OQ-UI-1) |
| Q25 | Help panel omits `V` | `MenuScene.ts:723-756` | **FIX** |
| Q26 | Dialogue-tree state lives in the UI scene | `UIScene.ts:323, 3410-3411, 3717-3725` | **FIX**: core session |
| Q27 | `t()` param values containing `$&`, `$1`… are interpreted by `String.replace` | `i18n/index.ts:75-78` | **FIX**: literal replace |
| Q28 | Far NPC click walks to the tile but does not interact on arrival | §5.3 step 6 | decide (OQ-INPUT-5); recommend auto-interact on arrival |
| Q29 | Jukebox durations are nominal, not the real track lengths | `MenuScene.ts:30-42` | **FIX**: use real lengths |
| Q30 | Touch build cannot open achievements, companion or audio settings (no buttons, no keys) | §5.7.3 | **FIX**: add a "more" button / system menu |
| Q31 | HP orb / log alpha lerps are per frame (frame-rate dependent) | §6.3 | port with dt-correct easing |
| Q32 | `Destroy` (normal items) in the bag has no confirm | §7.1 | keep (only white items) |
| Q33 | Changing difficulty on Continue keeps hero, map and position | §1.2 | keep (D2-like) |
| Q34 | `Esc` (and other save triggers) during the 1100 ms death window save `hp = 0` at the death spot; the scene stop discards the pending respawn timer | `ZoneScene.ts:2203-2205, 4223-4228, 4264-4266, 914-953` | **FIX**: never save a `Dying` hero — defer, or `ResolvePendingDeath()` first (§3.4) |
| Q35 | A save with `hp ≤ 0` loads a frozen 0-HP hero (soft-lock, nothing calls die/respawn); un-gated HP writers (bag potion, potion pickup, healer merc, level-up refill) revive a dead hero mid-death, refund the soul echo and can cause a double death | `ZoneScene.ts:4313, 1496-1518, 6498-6503`; `UIScene.ts:4297-4301`; `Player.ts:156-166, 219-223` | **FIX**: load as a respawn at `camps[0]` with full HP/MP (§3.5); `HeroLife::Dying` blocks heals and UI actions (§5.1.1) |
| Q36 | The overworld respawn does not autosave (penalty/echo not on disk until the next trigger) | `ZoneScene.ts:949-952`, `Player.ts:431-452` | **FIX**: autosave after every respawn |

---

## 13. Render-only items → 3D / UE equivalents
| Web | UE |
|---|---|
| Canvas-baked UiKit textures, rebuilt per size | 9-slice PNGs (2×/3×) generated by a Python script from §8 parameters, or UMG materials; Slate brushes with margins |
| HUD orbs (TileSprite + geometry mask) | UMG `Image` with a dynamic material (fill, wave scroll, pulse) |
| Cooldown sweep via Graphics polygons | radial-wipe material parameter on the slot icon |
| Menu particles, glows, rune circle | 3D menu level + Niagara, or animated UMG materials |
| Zone/level-up/quest banners drawn in the world camera | UMG overlay widgets with the same timings |
| Minimap redrawn with Graphics every 250 ms | per-zone generated texture + UMG markers (or `USceneCaptureComponent2D` orthographic) |
| `fitPanelForMobile` scaling | DPI scaling + per-platform panel layouts (touch variants of the same widgets) |
| Phaser pointer `downTime` claim | UMG input handling (`Handled`) |
| `RENDER_SCALE`, text resolution, `?res=` / `?quality=` | UE scalability settings, screen percentage |
| DOM rotate card, fullscreen API | project orientation settings |
| IndexedDB / localStorage | files in `Saved/` |

---

## 14. Data to export (JSON) — owned by this area
| File (suggested) | Content | Source |
|---|---|---|
| `i18n/zh-CN.json`, `i18n/en.json` | merged flat tables (main + questStory + story + abyssRun + homestead + pets) | `src/i18n/locales/*.ts` |
| `i18n/zh-TW.json` | generated with `convertToTraditional` over zh-CN | `src/i18n/converter.ts` |
| `i18n/tc_map.json` (tooling/tests) | SC/TC pair strings + phrase fix-ups | `converter.ts:11-58` |
| `ui/theme.json` | `UI_COLORS`, `QUALITY_HEX`, `BUTTON_STYLE`, frame margins/radii/bands, `PANEL_STYLE` (header, depths, tooltip), log colours, difficulty colours, skill-tree tree/damage colours, minimap tile + marker colours, auto-loot/auto-combat label colours, orb liquid colours, EXP fill, boss bar colours | `UiKit.ts:22-51, 321, 512-518`, `UIScene.ts:59-102, 945, 2037-2049, 2908-2910`, `MenuScene.ts:1225-1234` |
| `ui/hud_layout.json` | resolved §6.2 tables (desktop + touch) and panel rects of §7 | `UIScene.ts:118-205` + panel builders |
| `ui/touch_controls.json` | `CSS` sizes, ring radii/angles, `MIN_K/MAX_K`, `TOUCH_POINTERS`, button colours | `MobileControlsSystem.ts:30-56` |
| `input/default_bindings.json` | §5.1 and §5.5 tables (also authored as IMC assets) | `ZoneScene.ts:615-643, 2158-2245` |
| `menu/jukebox.json` | track keys, zone/state, durations | `MenuScene.ts:30-42` |
| `menu/credits.json` | rewritten for the UE build | `MenuScene.ts:1067-1176` (BGM list `:1116-1126`) |
| `save/save_schema.json` + fixtures | JSON Schema of §3.2; v1/v2/v3 fixtures from the tests | `types.ts:609-670`, `src/__tests__/SaveMigration.test.ts`, `SpiritSaveMigration.test.ts` |

Export with a one-off `tsx` script that imports the TS modules and writes the files (the locale modules are plain
objects).

---

## 15. Unit-test vectors for the core
1. **Migration v1→v3** (from `SaveMigration.test.ts` / `SpiritSaveMigration.test.ts`): minimal v1 `{id, version 1,
   timestamp, classId, player{…}}` → all defaults of §3.3, `version 3`, `player.spirit {0,0}`; v2 with spirit `{64,
   2500}` preserved; `{NaN, −10}` → `{0,0}`; rogue `{100, 9000}` → `{100, 5500}`; items without sockets get `[]`,
   existing sockets kept; running migration on a v3 save changes nothing.
2. **FindNearestWalkablePosition**: the seven vectors of §3.7.
3. **Pet migration**: legacy `homestead.pets [{petId:'<valid>', level 25, exp 999}]`, `activePet` = that id →
   `owned[0] = {level 20, exp 0, evolved 2, bond 0, bondProgress 0}`, active kept; unknown id dropped; duplicate dropped;
   `pets` present → legacy ignored.
4. **Quest load reset**: progress `{status:'completed', objectives:[{current 5}]}` for a quest that now has 2 objectives
   → `{status:'active', objectives:[{0},{0}]}`.
5. **Difficulty**: `derive('hell', []) = [normal, nightmare]`; `derive('normal', [normal]) = [normal]`;
   `shouldShow('normal', []) = false`, `shouldShow('nightmare', []) = true`; `states([normal]) = {normal: completed,
   nightmare: available, hell: locked}`.
6. **t()**: `t('menu.continue', {class:'战士', level:'7'}) = '继续游戏 - 战士 Lv.7'`; missing key → key; zh-TW missing
   key falls back to zh-CN then en; repeated placeholder replaced everywhere; value `"$&"` inserted literally (port);
   unknown placeholder kept.
7. **zh-TW**: `convertToTraditional('铁剑') = '鐵劍'`; `'战斗日志' → '戰鬥日誌'`; unmapped characters pass through.
8. **Input mapping**: `ScreenDirToTile((0,−1)) = (−1,−1)`; W+D → `(0,−2)` → normalised `(0,−1)`; stick `(0.1,0.1)`
   (hypot 0.141 < 0.18) → no move; joystick `(0.05, 0)` → full-speed move along `(1,−1)/√2`.
9. **Keyboard step**: delta 16 ms, moveSpeed 120 → step `0.0288` tiles; blocked target tile → position unchanged.
10. **Hold-to-move**: pointer tile unchanged within 120 ms and path non-empty → no re-path; pointer 0.5 tiles from hero →
    path cleared; target wall with walkable neighbour at ring 1 → goal = first ring cell in `dr, dc` scan order.
11. **HUD formatting**: level text `"Lv.3  (120/450)"`; desktop cooldown text `ceil(1400/1000) = "2"`; touch cooldown
    `1400 → "2"`, `600 → "0.6"`; dodge text `ui.hud.dodgeCooldown {seconds:'0.7'}`.
12. **Combat log buffer**: 9 messages → the oldest dropped; collapsed mode shows ≤ 3, none older than 12 s.
13. **Loot notices**: 5 pickups → 4 notices, newest first.
14. **Auto-loot cycle**: `off → all → magic → rare → legendary → off`.
15. **Save round-trip**: `SerializeSave(ParseSave(fixture))` equals the fixture modulo key order; labyrinth override
    writes `(15,22,'abyss_rift')`.
16. **No save while dying** (Q34): Lv 6 normal hero, gold 1000, at (40.3, 40.6) in `emerald_plains` takes lethal damage
    → gold 900, echo `{emerald_plains, 40, 41, gold 100, exp 0}`, `life = Dying`; `CanSave = false`;
    `RequestSave` → nothing written, `savePending = true`; advance 1100 ms → respawn at `camps[0]`, `hp = maxHp`,
    `mana = maxMana`, exactly one save written with those values, gold 900 and the echo; `savePending = false`.
17. **ResolvePendingDeath**: same hero 300 ms into `Dying` → `ResolvePendingDeath` → `life = Alive`, position
    `camps[0]`, HP/MP full, gold still 900 (no second penalty), echo unchanged, the 1100 ms timer no longer fires
    (no second respawn / log). Labyrinth variant: run ends `'fallen'`, hero at `abyss_rift` `camps[0]`.
18. **Load dead save** (Q35): `player.hp` ∈ {0, −5, NaN} at a walkable (40, 41), echo at (40, 41) → hero at the zone's
    `camps[0]`, `hp = maxHp` and `mana = maxMana` **with equipment bonuses**, `life = Alive`, gold unchanged, echo kept
    and **not** claimed on the first frame (fixture camp > 1.5 tiles from it), log `sys.player.respawn`. `hp = 37.5` → kept (≤ maxHp).
19. **Dying gates** (Q35): while `Dying`, `UseConsumable(c_hp_potion_s)` → refused, item quantity unchanged, `hp = 0`;
    HP potion drop within 2 tiles → stays on the ground; healer-merc heal → 0; skill key → not executed and not
    buffered; `KillHero()` again → no second penalty / echo change.

---

## 16. Open questions (decisions for the architecture owner)
* **OQ-SAVE-1** Slots: keep the web's single autosave, or 3 character slots (menu list with portrait/class/level/zone/
  difficulty/last played, delete with confirm)? Recommendation: 3 slots, each autosaving; New Game picks an empty slot or
  confirms overwrite.
* **OQ-SAVE-2** Keep JSON field-compatibility with web saves (import tool) and stay at v3 until a UE-only field is
  needed? Recommendation: yes; v4 adds `parentZone` (Q7) and `playTimeMs`.
* **OQ-SAVE-3** JSON library for the exception-free core (RapidJSON vs yyjson).
* **OQ-FLOW-1** System menu on Esc (Resume / Settings / Controls / Return to menu / Quit) replacing the instant return
  (Q10)?
* **OQ-FLOW-2** Cloud saves (iCloud / Google Play Games) — out of scope for Chapter 1?
* **OQ-FLOW-3** Should the Ch1 build hide later-milestone menu entries (Soundtrack) and gameplay keys (H, P, U)?
* **OQ-FLOW-4** Music licensing: "Desert Battle Theme" is GPL-2.0 — not acceptable in a closed-source App Store build;
  replace before shipping chapter 4 content (Ch1 uses CC0 tracks only).
* **OQ-INPUT-1** Single ground move speed (`moveSpeed/36` tiles/s) for click and keyboard, or reproduce the web's two
  speeds (Q13)?
* **OQ-INPUT-2** Which panels block gameplay input (Q11)? Recommendation: modal ones (dialogue, quest card, shop,
  stash, mini-boss, lore, confirms) block movement/skills; non-modal ones do not.
* **OQ-INPUT-3** Gamepad UI navigation and panel access (CommonUI) for desktop/console — needed for macOS/Windows
  Chapter 1?
* **OQ-INPUT-4** Town portal channel: cancel on movement input like D2's casting, or keep moving allowed (Q23)?
* **OQ-INPUT-5** Auto-interact when arriving next to a clicked far NPC/loot/chest (Q28)?
* **OQ-UI-1** Minimap: grid-aligned like the web, or rotated to match the fixed camera yaw; reveal unexplored areas or
  apply fog (Q24)?
* **OQ-UI-2** World map in the Ch1 build: show all 5 zones (locked ones greyed) or only the playable zone?
* **OQ-UI-3** Homestead / pets / companion panels in Ch1: hidden, or shipped with the Ch1-relevant parts (embers count,
  `pet_sprite` ownership)?
* **OQ-UI-4/5** One in-game Settings panel (audio + language + graphics quality + touch layout scale + key help) for
  all platforms instead of the audio-only panel and menu-only language switch?
