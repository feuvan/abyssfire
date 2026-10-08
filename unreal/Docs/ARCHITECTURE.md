# Abyssfire Unreal Rebuild — Architecture

Read with `Docs/DECISIONS.md` (binding answers) and the port specs in `Docs/spec/` (the behaviour to reproduce).
Platform and build details: `Docs/spec/ue58-platform.md` (§3.3 core language rules, §4 layout, §5 config) — they are
binding and not repeated here.

## 1. Layers

```
                ┌───────────────────────────────────────────────────────────┐
  Blender  ───► │ Art/Export/*.fbx + Art/Export/manifest.json + icons/portraits │
  scripts       └───────────────┬───────────────────────────────────────────┘
                                │ Scripts/build_content.py (UE editor Python): import, materials, L_Main
                                ▼
 web TS data ─► Tools/export-data ─► Data/*.json ──(bytes)──► AbyssCore (pure C++20) ◄── CoreTests (CMake, doctest)
                                                                │ commands ▲ events / snapshots
                                                                ▼          │
                                                     Abyssfire UE module (actors, camera, Slate UI, input, audio, VFX)
```

* **AbyssCore** owns every rule and all game state. It never includes UE headers (except `Private/UE/AbyssCoreModule.cpp`).
* **Abyssfire** (UE module) only renders state, feeds input as commands, plays animations/VFX/audio from core events,
  and builds the UI. It must not contain gameplay rules (no damage maths, no drop rolls, no quest logic).
* **Data** is exported from the TypeScript source so the numbers are identical to the web game; the C++ never
  hardcodes tables that exist in the TS data.

## 2. Repository layout (under `unreal/`)

As in ue58-platform.md §4.1, plus:

```
Source/AbyssCore/Public/abyss/
  base/      Types.h (ids, Vec2/TilePos), Rng.h, SimClock.h, Math.h, Assert.h, Log.h, Json.h, I18n.h, Units.h
  data/      DataStore.h (+ one header per table: ClassData.h, SkillData.h, ItemData.h, MonsterData.h, MapData.h,
             QuestData.h, NpcData.h, DialogueData.h, StoryData.h, LoreData.h, PetData.h, AudioData.h, AssetManifest.h)
  hero/      Hero.h (stats, derived, leveling, points), Skills.h (learn, progression, hotbar), Spirit.h, Buffs.h
  combat/    Damage.h, SkillExec.h, Projectiles.h, StatusEffects.h, HitFeedback.h, CombatInput.h, Difficulty.h,
             EliteAffixes.h, SoulEcho.h
  monsters/  Monster.h, MonsterAI.h, Spawner.h, MiniBoss.h, Hunts.h
  items/     Item.h, LootGen.h, Inventory.h, Equipment.h, Stash.h, Shop.h, Crafting.h, ItemCompare.h
  quests/    QuestSystem.h, QuestWorld.h, QuestGuide.h, Dialogue.h, Achievements.h, Lore.h
  story/     StoryDirector.h
  world/     Grid.h, MapGen.h, Pathfinding.h, FogOfWar.h, Zone.h (runtime zone: camps, exits, decor, hidden areas,
             random events, weather/day mood), Movement.h (click/hold/keyboard/stick), Camera.h (shake params only)
  pets/      Pets.h, PetCompanion.h
  save/      SaveData.h, SaveIO.h (v4 JSON read/write, migration)
  sim/       GameSim.h  — the facade: owns everything, steps at 60 Hz, takes Commands, emits Events
             Commands.h, Events.h, Snapshot.h (read-only views for rendering/UI)
Source/AbyssCore/Private/...      mirrors Public
Source/AbyssCore/ThirdParty/rapidjson/   vendored, namespace-renamed (DECISIONS U3)
CoreTests/                        CMakeLists.txt, doctest (vendored), tests/<area>_test.cpp, golden/ fixtures
Tools/export-data/                Node/TS script: imports src/data/**, src/i18n/** → Data/*.json (+ schema version)
Data/                             generated JSON (committed) — never hand-edited
Art/
  blender/                        Python generator scripts (bpy 5.0), shared kit modules, render-preview harness
  Export/                         FBX per asset + manifest.json + Textures/ + Icons/ + Portraits/ (committed)
  Previews/                       review renders (committed, small PNG)
Audio/
  render/                         Python/numpy synth port
  Export/                         OGG renders (committed)
Scripts/                          UE editor Python: build_content.py (import → materials → map), checks
Fonts/                            subset CJK + Latin fonts (OFL licensed)
```

## 3. The core API contract

### 3.1 Facade

```cpp
namespace abyss {
class GameSim {
public:
  static std::unique_ptr<GameSim> Create(const DataStore& data, const SimConfig& cfg);  // nullptr: store not finalized
  // Session
  bool NewGame(ClassId cls, Difficulty diff, uint64_t seed, int32_t slot = 0);
  // None on success; VersionTooNew / ParseFailed / NotAnObject / Invalid leave the current session unchanged.
  // difficultyOverride = the Continue -> difficulty selector choice (save-ui-input 1.2; SaveSlotInfo carries the rule).
  SaveError LoadGame(std::string_view saveJson, std::string* err,
                     std::optional<Difficulty> difficultyOverride = std::nullopt);
  SaveError LoadGame(const SaveData& save, std::string* err);
  std::string SaveGame(int64_t unixMs = 0) const;     // v4 JSON
  // Loop (S1): UE accumulates real time and calls Step() with exactly kStepMs per call.
  void Step();                                        // one 60 Hz tick (sim clock may be frozen, S2)
  void Submit(const Command& cmd);                    // queued, applied at the start of the next Step
  std::span<const Event> Events() const;              // events emitted during the last Step (cleared each Step)
  const Snapshot& View() const;                       // read-only state for rendering & UI
  void AdvanceRealTime(double realMs);                // UI/story timers that run on real time (S2)
};
}
```

* **Commands** are plain structs in a `std::variant` (MoveTo{tile}, MoveDir{vec}, Stop, AttackTarget{entity},
  CastSkill{slot, target/point}, Dodge{dir}, UsePotion{slot}, Interact{entity}, PickUp{groundItem}, EquipItem,
  UnequipItem, MoveItem, Buy/Sell/Buyback, StashPut/Take, Craft*, LearnSkill, AllocStat, SetHotbar, DialogueChoose,
  QuestTrack, QuestTurnIn{choice}, StorySkip/Advance, TownPortal, ToggleAutoBattle, SetLock, OpenPanel/ClosePanel
  (for U7 freezes), Settings changes that affect the sim). Every UI action goes through a command.
* **Panels** (`SimTypes.h` ownership rules): dialogue, quest card, shop / forge, stash, mini-boss dialogue, lore text and
  puzzle are **core-owned** modals — the owning system opens them and its state is the truth; GameSim derives the S2
  freeze and the U7 input block from that state (a hero command later in the same batch as the opening interaction is
  already rejected) and emits `EvPanelRequest` on every change. UE reports only its own panels (HUD panels, system menu,
  socket, confirm) with `CmdOpenPanel` / `CmdClosePanel`; `CmdClosePanel` on a core-owned modal closes it through the
  owner. Zone exit and the hero's death close all core-owned modals.
* **Rewards**: exp, gold and item grants from every area go through `RewardService` (`hero/Rewards.h`: Dying gates,
  overflow policy, presentation events with the source / reason), never through `Hero::AddExp` / `SetGold` directly.
* **Events** are plain structs in a `std::variant` and carry everything presentation needs: entity spawned/despawned
  (with art id), animation requests with timing (e.g. `PlayAnim{entity, anim, startMs, contactMs}`), hit landed
  (`Hit{target, amount, weight, crit, element, impactColor}` — the HitFeedback profile is in the event), projectile
  launched (from, to, travelMs, vfx id), ground effect start/stop, status applied/expired, floating text, loot dropped,
  pickup, gold, exp, level up, quest progress/complete, dialogue open/close, story beat begin/line/end, zone change,
  music state, sfx cue (id + position), camera shake/hit-stop/slow-mo, UI toasts/log lines (i18n key + args).
* **Snapshot** exposes const views: hero (pos, facing, stats, derived, buffs, cooldowns, spirit), monsters, NPCs,
  ground items, projectiles, zone (grid, decor, camps, exits, hidden areas, weather/mood), inventory/equipment/stash,
  quests and tracker, story state (current beat/line), dialogue state, shop state, minimap/fog, pets.
* Entities are referenced by stable `EntityId` (uint32, never reused within a zone visit).
* Time: all core timings are sim ms; animation contact/release times come from the **asset manifest** (§5) loaded into
  `DataStore`, exactly as the web derived them from sheet frames (`AnimConfig.attackContact`).

### 3.2 Ownership and style

* Each subsystem is a class owned by `GameSim`; subsystems talk through `GameSim` internals and an internal event bus,
  never through globals. No singletons.
* All text out of the core is **i18n keys + args**; `I18n` resolves them with the exported locale tables (zh-CN, en).
* RNG streams per domain (S3); a subsystem receives its stream by reference.
* Language rules: ue58-platform.md §3.3 (no exceptions/RTTI/iostreams/unordered iteration in gameplay order, stable
  sort, FP contract off, no non-ASCII literals, no UE macro names). Public headers write `(std::min)` / `(std::max)`.
* Build rules (`Public/abyss/base/Platform.h`): precise FP semantics in both modules (`FPSemantics =
  FPSemanticsMode.Precise` in AbyssCore.Build.cs and Abyssfire.Build.cs; a fast-math core TU fails to compile);
  `ABYSS_API` on every public class with out-of-line members and every public free function (`ABYSS_CORE_DLL` public,
  `ABYSS_CORE_BUILDING` private); `-Wundef` / `/we4668`.

### 3.3 Tests

* `CoreTests` builds the core with CMake (`-Wall -Wextra -Wshadow -Wundef -Werror`, `-fno-exceptions -fno-rtti`,
  `-ffp-contract=off`) on Linux GCC and Clang here, and runs doctest suites (`run.sh`). `ABYSS_SHARED=1 ./run.sh`
  builds the core as a hidden-visibility shared library (a missing `ABYSS_API` fails to link, like a UE modular build);
  `ABYSS_SANITIZE=1 ./run.sh` adds ASan + UBSan + float-cast-overflow. `shim/MacroShim.cpp` compiles every public
  header after the Windows / UE / Apple macros a game-module TU sees.
* Port the relevant web Vitest cases (`src/__tests__/`) as golden tests, plus each spec's "unit-test checklist".
* A headless **playthrough test** drives `GameSim` through Chapter 1 with scripted commands (new game → every Ch1
  quest → finale → save → load → continue) and asserts quest/story/level/inventory state at checkpoints.

## 4. Data export

`Tools/export-data` runs with the repo's Node toolchain (`npx tsx` or `vite-node`) and **imports the TS modules
directly** (so values are exactly the web's), then writes `Data/*.json` with a `schemaVersion`. Tables: classes &
skills, skill rules, items (bases, affixes, sets, legendaries, gems, potions), loot rules, monsters (+ mini-bosses,
hunts, elite affixes, difficulty), maps (raw grids + generator params; the C++ MapGen reproduces the enhancement —
W10; the exporter also writes the TS-generated result as a golden fixture for the test), NPCs, dialogue trees, quests,
story script, lore, achievements, pets, random events, audio cue table, i18n (zh-CN, en). The schema for each file is
documented at the top of the matching spec section; derived fields demanded by DECISIONS (e.g. C6) are added by the
exporter from explicit mapping tables in the exporter source, not computed in C++.

## 5. Art contract

* Generated by Blender scripts in `Art/blender/`; every asset has a preview render in `Art/Previews/` reviewed before
  it ships (no placeholders, DECISIONS R*).
* **Skeletons**: `SK_Humanoid` (heroes, human NPCs), `SK_Goblin` (goblin family, proportion-scaled humanoid with the
  same bone names), `SK_Slime` (blob), `SK_Quadruped` / `SK_Flyer` (later chapters & pets), `SK_Sprite` (pet_sprite).
  Bone naming follows UE mannequin conventions (`root, pelvis, spine_01..03, neck_01, head, clavicle_l/r,
  upperarm_l/r, lowerarm_l/r, hand_l/r, thigh_l/r, calf_l/r, foot_l/r, ball_l/r`) plus `weapon_r`, `weapon_l` sockets.
* **Files**: `Art/Export/<Category>/<AssetName>.fbx` with categories `Characters`, `Monsters`, `NPCs`, `Weapons`,
  `Props`, `Terrain`, `Foliage`, `VFX`, `Pickups`; animations embedded per character FBX as named actions
  (`Idle`, `Run`, `Attack01`…, `Cast01`…, `Hurt`, `Death`, `Dodge`, signature names) or as separate
  `<AssetName>_Anims.fbx` sharing the skeleton.
* **Manifest** `Art/Export/manifest.json` (also copied to `Data/assets.json` by the exporter): game id → asset name,
  skeleton, scale, sockets, material slots (toon base colour textures or vertex colour palettes, outline slot),
  animation list with lengths and **contact/release ms** per attack/cast animation, footprint (tiles) and blocking flag
  for props (W5), icon/portrait file names, and VFX ids → mesh/material parameters.
* Materials are created by `Scripts/build_content.py` from the manifest (toon master `M_Toon`, outline `M_Outline`,
  terrain `M_TerrainToon` with splat blending, foliage with wind WPO, VFX additive/unlit, water).

## 6. UE module structure (`Source/Abyssfire`)

* `AbyssGameInstance` — loads Data bytes → `DataStore`, owns `GameSim`, save slots, settings.
* `AAbyssGameMode` / `AAbyssPlayerController` — runtime Enhanced Input (keyboard/mouse/gamepad/touch) → Commands;
  ground picking by ray-plane/terrain-height intersection (no collision, spec §8.4).
* `AAbyssCameraRig` — fixed-yaw oblique camera (W1), follow lag, zoom, shake/hit-stop from events.
* `UAbyssWorldBuilder` — builds the zone from `Snapshot` zone data: terrain mesh (tile heights + splat), ISM foliage and
  props, water, camps, exits/gates, lights, sky/fog per zone mood; rebuilds on ZoneChanged.
* `AAbyssCharacterActor` (hero, monsters, NPCs, pets) — skeletal mesh + `UAbyssAnimInstance`, interpolated transform
  from snapshots, plays anims from events, weapon sockets, hit flash/recoil, name/HP bars (Slate world widgets).
* `UAbyssVfxSystem` — pooled ISM particle emitters + projectile/ground-effect actors driven by events.
* `UAbyssAudioSystem` — music state machine (explore/combat/boss/story), SFX cues with 3D panning (A4).
* `UI/` — Slate: HUD (orbs, skill bar, potion slots, exp/spirit bars, minimap, log, quest tracker, buffs), panels
  (inventory, character, skills, quest log, map, shop, stash, dialogue, settings, system menu, pets), story overlay,
  menus (title, slots, class select, difficulty), touch controls, tooltips with item compare.
* `Platform/` — save paths, app lifecycle (autosave on background), quality tier selection.

## 7. Milestone 1 (Chapter 1) exit criteria

1. `CoreTests` green on GCC and Clang here, including the scripted Chapter 1 playthrough.
2. Every Ch1 asset generated, previewed and reviewed; FBX + manifest committed.
3. UE project compiles on the user's Mac (UE 5.8.3, Xcode 26.1.1), `Scripts/build_content.py` builds the content, and
   the game is playable from title → Chapter 1 finale on Mac; then packaged for Windows, Android and iOS.
4. Known gaps documented in `Docs/STATUS.md`.
