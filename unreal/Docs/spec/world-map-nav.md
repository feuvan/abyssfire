# Port Spec — World, Maps, Navigation, Camera & Atmosphere

Area owner: world / maps / navigation. Web source of truth: branch `claude/unreal-rebuild`, TypeScript under `src/`.
Target: portable C++20 core (`AbyssCore`: no exceptions, no RTTI, no UE types) + thin UE5 module (render, input, UI).

This document describes **what the web game does today**, precisely enough to re-implement it without reading the
TypeScript, and proposes the 3D equivalent for everything that is a 2D/Phaser artefact. Markers used below:

* **CORE** — gameplay rule; lives in the C++ core and is unit-tested.
* **RENDER-ONLY** — Phaser/DOM presentation; the UE module re-creates it, the proposed 3D equivalent is given.
* **QUIRK Wn** — web behaviour that is a bug or a 2D artefact. Each has a recommendation: **PARITY** (keep) or **FIX**.
  The summary is in §20; unresolved choices are in §22.

Citations are `path:line` at the time of writing. Sibling specs referenced: `combat-feel.md` (hit timing, dodge,
attack approach, camera shake), `monsters-ai.md` (spawning, safe-zone AI, respawn, event spawns — its §6.2–6.4),
`classes-stats-skills.md` (where `moveSpeed` comes from), `loot-items-inventory.md` (loot generation),
`save-ui-input.md` (input bindings, `InputFrame`, touch layout; §7.4 lists the follow-ups it owns).

---

## 0. Conventions

| Topic | Web | Port rule |
|---|---|---|
| Space | Grid of tiles. Positions are `(tileCol, tileRow)` **floats**; integer values are tile **centres**. `col` = x-like, `row` = y-like. Arrays are `tiles[row][col]`. | Core works in tile units on the ground plane (`double col,row`). Grid storage row-major: `idx = row*cols + col`. |
| Distances | Euclidean on (col,row) (`distanceSq`, `src/utils/IsometricUtils.ts:35-39`) unless a rule says Chebyshev/Manhattan. Most proximity rules compare **squared** distance with a squared threshold — keep the `<` vs `<=` exactly as written here. | Same. |
| Screen px | World px of the 2:1 iso projection (§1.2). Only the hero's path speed, light radii and a few VFX are in px. | Converted to tiles with the constants in §1.3. |
| Time | ms of the Phaser scene clock (`time.now`, monotonic across zone changes). Per-frame rules use the real frame `delta`. | Core owns `nowMs` advanced by a **fixed 60 Hz** sim tick (`dt = 16.667 ms`) so per-frame web rules keep their 60 fps behaviour. |
| RNG | Two kinds: (a) **seeded** `SeededRandom` (Park–Miller) for map generation — must be **bit-exact**; (b) `Math.random()` for runtime rolls (events, pet spawns, ambush positions) — distribution parity only. | (a) port verbatim (§4.1); (b) the core's injectable PRNG (`float01()`), same draw conditions/order. |
| Rounding | `Math.round` = floor(x+0.5); `Math.floor`; `Math.ceil`. | Use `std::floor(x + 0.5)` for `Math.round` (differs from `std::round` on negative halves). |

### 0.1 Floating-point determinism (MapGenerator only)

The generator's output (tile grid + 1 269 decorations for emerald_plains) is a golden artefact; the C++ port must hash
identically (§4.9). Requirements for the translation unit(s) that contain the generator and `groveNoise`:

1. IEEE-754 binary64 everywhere (`double`), evaluation order exactly as written in this spec (left-to-right).
2. **No FMA contraction**: compile with `-ffp-contract=off` (clang/gcc; Apple clang and Android NDK clang on arm64
   contract `a*b+c` by default), MSVC `/fp:precise` without `/fp:contract`. Alternatively `#pragma STDC FP_CONTRACT OFF`
   at the top of those files. No `-ffast-math`.
3. `Math.hypot(a,b)` must use the V8 algorithm, **not** `std::hypot` nor `sqrt(a*a+b*b)` (they differ from V8 in ~34 %
   of integer inputs; verified on 68 121 integer pairs, 0 mismatches with the emulation below):
   ```cpp
   double jsHypot2(double a, double b) {          // V8 Math.hypot, Kahan-compensated
     double x[2] = {std::fabs(a), std::fabs(b)};
     double mx = std::max(x[0], x[1]); if (mx == 0) return 0;
     double sum = 0, comp = 0;
     for (double v : x) { double n = v / mx; double s = n*n - comp; double p = sum + s; comp = (p - sum) - s; sum = p; }
     return std::sqrt(sum) * mx;
   }
   ```
   (For emerald_plains `sqrt` happens to give the same map, but other seeds are not guaranteed.)
4. `x | 0` (JS ToInt32) on an integral double: `int32_t(uint32_t(int64_t(fmod(x, 4294967296.0))))` for x ≥ 0; all
   generator inputs are non-negative. If the double sum can exceed 2^53 (labyrinth random seeds), compute the sum **as a
   double** first (same order), then apply ToInt32 — do not do it in int64.
5. `Math.imul(a,b)` = `int32_t(uint32_t(a) * uint32_t(b))`; `x >>> k` = `uint32_t(x) >> k`.

`std::sqrt`, `std::floor`, `std::ceil`, `+ - * /` are correctly rounded and match JS.

### 0.2 What Chapter 1 (emerald_plains) needs from this area

* The baked emerald_plains map (120×120, tiles + decorations, §3) **and** the generator port verified against it (the
  labyrinth reuses the generator with random seeds later).
* Tile walkability, the 8-way A* (§5), hero path following + keyboard/joystick/gamepad movement (§6), click-to-move with
  the exact click-priority chain and hold-to-move (§7).
* Fixed oblique follow camera (§8), zone-entry fade, zone exits (one exit to `twilight_forest`, gated in milestone 1 —
  §9.6), town portal to the camp (reachable from keyboard+mouse, touch and gamepad, §7.4), respawn at the camp, save
  position restore.
* Pointer-free world actions (§7.4): `interact` with `FindInteractTarget`, `townPortal`, auto-combat and auto-loot
  toggles, and the touch portal button.
* Exploration grid for the hidden area `hidden_ep_elven_cache` (§10), minimap (§11), world-map panel (§9.7).
* World interactables: 4 lore pickups, 3 story decorations, hidden-area chest + gold pile, potion auto-collect (§12).
* Random events with emerald_plains data (§13).
* Plains mood: ambient/vignette/haze/colour grade, camp fire + torch lights, pollen + dust ambience (§14).
* 3D terrain, moss-boulder/hedge walls, camp palisades and props, decoration meshes, water (§15).

---

## 1. Coordinate systems and the 3D mapping

### 1.1 Tile space (CORE)

* Tile `(c, r)` is the unit square centred on `(c, r)`; it covers `[c−0.5, c+0.5] × [r−0.5, r+0.5]`.
* An entity's "tile" for walkability checks is `(round(col), round(row))` (`Math.round`).
* Out-of-bounds (`col<0 || col>=cols || row<0 || row>=rows`) is never walkable.

### 1.2 Web isometric projection (RENDER-ONLY, `src/utils/IsometricUtils.ts:3-26`, `src/config.ts:4-5`)

`TILE_WIDTH = 64`, `TILE_HEIGHT = 32`.
```
cartToIso(c, r)   = { x: (c − r) * 32,  y: (c + r) * 16 }          // tile centre → world px
isoToCart(x, y)   = { c: (x/32 + y/16) / 2,  r: (y/16 − x/32) / 2 }
worldToTile(x, y) = { col: floor(isoToCart.c), row: floor(isoToCart.r) }   // NOTE floor, see QUIRK W1
```
Screen axes: `+col` → right-down, `+row` → left-down, `(−1,−1)` → straight up, `(+1,−1)` → straight right. This is a
true 2:1 dimetric view: camera yaw 45° to the grid, elevation 30° (`sin 30° = 0.5` foreshortening). Derived scales:

| Quantity | Value | Derivation |
|---|---|---|
| Ground px per tile, screen-horizontal | **45.255** | 64/√2 |
| Ground px per tile, screen-vertical (depth) | **22.627** | 45.255 × sin 30° |
| Upright px per tile of height | **39.192** | 45.255 × cos 30° |
| RMS ground px per tile (any direction) | **35.777** | √(32²+16²); also exactly the length of a cardinal tile step |

Depth sorting (RENDER-ONLY): ground tiles depth = `row`; props/characters depth = `screenY + offset` (player +100,
monsters +50, upright decor +70, flat decor +5, wall overlays +55). In 3D the z-buffer replaces all of it.

### 1.3 Proposed 3D mapping (decision for the UE module)

| Item | Proposal |
|---|---|
| Units | **`TILE_UU = 100`** (1 tile = 1 m = 100 Unreal units). Same constant as `combat-feel.md` §0 and `monsters-ai.md` §0. |
| Axes | `UE.X = col × 100`, `UE.Y = row × 100`, `UE.Z` up. Tile (c,r) centre at `(100c, 100r, 0)`; the 120×120 map spans `X,Y ∈ [−50, 11 950]`. |
| Ground height | Flat: every walkable tile at `Z = 0`. The web has no elevation and no gameplay depends on height. Water surface `Z = −12`, water bed `Z ≈ −60` (render-only). |
| Camera yaw | **−135°** (camera forward on the ground = `(−col, −row)`, camera right = `(+col, −row)`): reproduces the web screen orientation exactly (W/screen-up = `(−1,−1)`, screen-right = `(+1,−1)`). |
| Upright sizes | sprite px → cm: **height × 2.552** (100/39.192), **width × 2.210** (100/45.255). Hero sprite ≈ 66 px tall → **≈ 168 cm**; plains oak (ground line 164 px) ≈ 4.2 m; round tree ≈ 3.3 m; bush ≈ 1.0 m (sizes in §15.5). |
| Light radii | px radius → cm: × 2.21 for the horizontal reach (lights are screen circles in the web). |
| Core ↔ UE | The core never sees cm. UE converts `pos_uu = tile × 100` on output and `tile = uu / 100` on input. |

### 1.4 Picking the ground under the cursor / finger (RENDER-ONLY → CORE input)

Web: `worldToTile(pointer.worldX, pointer.worldY)` (`src/scenes/ZoneScene.ts:783`). 3D: line-trace from the camera
through the cursor against a dedicated ground plane / terrain channel at `Z = 0` (ignore props), `tileF = hit.XY / 100`.
Then `tile = round(tileF)` (FIX for W1) or `floor(tileF)` (PARITY). Pass `tileF` to the core as well (hold-to-move uses
the integer tile; nothing in this area needs the fractional part).

**QUIRK W1** — `worldToTile` floors the cartesian coordinate, so the upper/left half of every diamond maps to the
neighbouring tile (half-tile bias toward screen-up). The generous 1.5-tile hit boxes (§7.1) hide it. **FIX**: use
`round` in 3D (the trace hit point is exact).

If the cursor ray hits an actor's collision (monster, NPC, loot, chest), UE may substitute that actor's tile position as
the click tile — equivalent to clicking its feet in the web, and always inside the 1.5-tile hit boxes.

---

## 2. Tile types and map data model

### 2.1 Tile types (CORE, `src/systems/MapGenerator.ts:3-10`, `:699-706`)

| id | name | walkable | minimap colour (`src/scenes/UIScene.ts:2908-2910`) | notes |
|---|---|---|---|---|
| 0 | grass | ✔ | `0x4a8c3f` | plains primary ground |
| 1 | dirt (path) | ✔ | `0x8b7355` | carved paths + 8 % specks; desert primary |
| 2 | stone | ✔ | `0x6a6a6a` | mountain/abyss primary; **absent from emerald_plains** |
| 3 | water (lava in abyss) | ✘ | `0x1a5276` | lakes |
| 4 | wall | ✘ | `0x4a4a4a` | single-tile obstacles + the border ring; rendered as outcrops |
| 5 | camp ground | ✔ | `0x9e7c52` | 11×11 camp interior |
| 6 | camp wall (palisade) | ✘ | none → `0x222222` | camp fence |

`walkable(t) = t ∉ {3, 4, 6}`. The `collisions[row][col]` boolean grid is derived from tiles at generation time **and
then mutated once at zone load** (§3.4: 8 barrel/crate tiles become blocked). `true` = walkable.

### 2.2 `MapData` (`src/data/types.ts:214-240`) — export to JSON

| Field | Type | Meaning |
|---|---|---|
| `id` | string | zone id (`emerald_plains`) |
| `name` | string | zh-CN display name (i18n key `getZoneName`) |
| `cols`, `rows` | int | grid size |
| `tiles` | int[rows][cols] | tile ids (empty in source → generated) |
| `collisions` | bool[rows][cols] | walkability (derived) |
| `spawns` | `{col,row,monsterId,count}[]` | monster packs (`monsters-ai.md` §6.1) |
| `camps` | `{col,row,npcs:string[]}[]` | camp centres + NPC ids in slot order (§3.4) |
| `playerStart` | `{col,row}` | new-game spawn / default arrival |
| `exits` | `{col,row,targetMap,targetCol,targetRow}[]` | zone exits (§9.2) |
| `levelRange` | `[int,int]` | zone level band (emerald_plains `[1,7]`) |
| `bgColor?` | string | render-only clear colour |
| `theme?` | `'plains'|'forest'|'mountain'|'desert'|'abyss'` | generator + terrain + mood selector |
| `seed?` | int | generator seed (default 42) |
| `decorations?` | `{col,row,type}[]` | generated scatter (§4.7) |
| `safeZoneRadius?` | number | camp safe radius, default **9** wherever read |
| `petSpawns?` | `{col,row,petId,chance}[]` | rare ley-beast spots (later milestones) |
| `hiddenAreas?` | `HiddenArea[]` | §10.3 |
| `subDungeonEntrances?` | `{id,name,col,row,targetSubDungeon}[]` | later milestones |
| `storyDecorations?` | `StoryDecoration[]` | §12.2 |
| `fieldNpcs?` | `{col,row,npcId}[]` | NPCs outside camps |

`HiddenArea` (`src/data/types.ts:243-261`): `id, name, col, row, radius, startCol?, startRow?, endCol?, endRow?,
rewards: {type:'chest'|'gold_pile'|'rare_spawn'|'lore', value?: string, col, row}[], discoveryText`.
`StoryDecoration` (`:305-315`): `id, name, description, col, row, spriteType ∈ {ruins, skeletal_remains, ancient_statue,
broken_altar, war_banner, charred_tree, collapsed_pillar, ritual_circle, frozen_corpse, sand_buried_structure}`.

### 2.3 Registry (`src/data/maps/index.ts:35-62`)

* `AllMaps`: emerald_plains, twilight_forest, anvil_mountains, scorching_desert, abyss_rift, ember_tower. Every map with
  empty `tiles` and a `theme` is generated **once at module load** with `MapGenerator.generate(map, externalLandmarks(id))`
  (`:45-52`). ember_tower is hand-authored (§16).
* `MapOrder = [emerald_plains, twilight_forest, anvil_mountains, scorching_desert, abyss_rift]` (world-map panel order).
* Unknown map id at zone start falls back to `emerald_plains` (`src/scenes/ZoneScene.ts:355`).

### 2.4 Zone table (all milestones)

| id | size | theme | seed | levelRange | playerStart | exits (col,row → target @ col,row) | walkable | decor |
|---|---|---|---|---|---|---|---|---|
| emerald_plains | 120² | plains | 12345 | 1–7 | (15,22) | (119,60) → twilight_forest @ (2,58) | 13 167 | 1 269 |
| twilight_forest | 120² | forest | 24690 | 8–17 | (3,58) | (0,58) → emerald_plains @ (118,60); (119,119) → anvil_mountains @ (2,58) | 11 668 | 1 333 |
| anvil_mountains | 120² | mountain | 37035 | 18–27 | (3,58) | (0,58) → twilight_forest @ (118,118); (119,119) → scorching_desert @ (2,58) | 12 336 | 1 015 |
| scorching_desert | 120² | desert | 49380 | 28–37 | (3,58) | (0,58) → anvil_mountains @ (118,118); (119,119) → abyss_rift @ (2,58) | 13 225 | 974 |
| abyss_rift | 120² | abyss | 61725 | 38–48 | (3,58) | (0,58) → scorching_desert @ (118,118) | 12 489 | 998 |
| ember_tower | 48² | plains (hand-authored) | 4817 | 1–50 | (24,41) | none (portal, §16) | 1 673 | 185 |

---

## 3. Emerald Plains (Chapter 1) — complete data

Source: `src/data/maps/emerald_plains.ts:7-88`. `safeZoneRadius` unset → 9.

### 3.1 Authored anchors

| Kind | Data |
|---|---|
| Camps | **(15,15)** npcs `[blacksmith, merchant, quest_elder]`; **(95,100)** npcs `[merchant]` |
| playerStart | (15,22) |
| Exit | (119,60) → `twilight_forest` (2,58) — on the east border wall |
| Spawns | (25,20) slime_green×8 · (35,40) goblin×6 · (20,45) slime_green×5 · (85,18) slime_green×6 · (95,35) goblin×6 · (75,50) goblin×4 · (30,80) goblin×8 · (15,95) goblin_chief×1 · (80,75) goblin×5 · (95,90) goblin×6 |
| Field NPCs | (50,30) `plains_herbalist`; (70,65) `plains_wanderer` |
| Hidden area | `hidden_ep_elven_cache` 精灵族秘密宝库, centre (108,108), radius 6 → bounds (102..114, 102..114); rewards: `chest` value `rare` @ (108,108), `gold_pile` value `"200"` @ (110,107) |
| Story decorations | `story_ep_ruined_tower` 倒塌的精灵塔 (55,45) `ruins`; `story_ep_goblin_totem` 哥布林图腾柱 (40,70) `war_banner`; `story_ep_ancient_well` 干涸的古井 (88,55) `broken_altar` |
| levelRange | [1, 7] |

### 3.2 External landmarks passed to the generator (`src/data/maps/index.ts:13-33`)

`externalLandmarks(mapId)` collects points defined outside the map file that must stay dry and walkable, each with a
margin (tiles). Order matters only for documentation (the generator treats them as a set). For emerald_plains it yields
exactly these 28 points (verified: generating with this list reproduces the shipped map hash):

| Source | Points (col,row) | margin |
|---|---|---|
| Lore (`src/data/loreCollectibles.ts`) | lore_ep_01 (45,35) · lore_ep_02 (105,15) · lore_ep_03 (15,108) · lore_ep_04 (85,80) | 2 |
| Mini-boss (`src/data/miniBosses.ts:260`) | (60,55) | 5 |
| Quests in zone (`src/data/quests/all_quests.ts`), in quest order: per objective `location` (m3) and `source.kind=='gather'` area (m3), then `questArea` (m4), `defendTarget` (m6), `escortNpc` start/dest (m3), `clues` (m2) | q_kill_slimes area (32,12)·m4 · q_kill_goblins area (34,40)·m4 · q_explore_goblin_camp loc (40,52)·m3 · q_find_goblin_chief area (25,65)·m4 · q_secure_plains loc (65,30)·m3, (50,55)·m3 · q_collect_slime_gel area (25,20)·m4 · q_herb_gathering gather (22,26)·m3, area (20,25)·m4 · q_lost_pendant loc (30,24)·m3, (42,34)·m3, (54,44)·m3, area (54,44)·m4 · q_bandit_trouble area (80,75)·m4 · q_pet_sprite_friend loc (78,12),(92,10),(100,22),(86,28)·m3, area (88,18)·m4 · q_escort_merchant_plains loc (50,90)·m3, area (50,90)·m4, escort start (30,40)·m3, dest (50,90)·m3 | as listed |

The core should compute this list from the exported quest/lore/mini-boss tables with the same rules (needed when the
generator runs for other zones); for the baked chapter-1 map it only matters for the golden test.

### 3.3 Generated result (golden)

| Item | Value |
|---|---|
| Tile counts | grass 11 003 · dirt 1 972 · stone 0 · water 363 · wall 820 · camp 192 · camp wall 50 |
| Walkable after generation | **13 167** (13 159 after the 8 barrel/crate tiles of §3.4) |
| Lakes | `lakeCount = 4`: centres (73,107) r 5.84 · (44,100) r 5.00 · (54,108) r 6.68 · (63,108) r 5.28 → two water bodies: 67 tiles (cols 39–48, rows 96–103) and 296 tiles (cols 49–79, rows 101–114), both in the south-west/south |
| Decorations | 1 269: grass 538 · flower 260 · bush 203 · rock 105 · tree 66 · tree_round 51 · mushroom_red 45 · boulder 1 |
| RNG checkpoints (`SeededRandom.state` after phase / cumulative draws) | secondary scatter 114752593 / 13 924 · wall scatter 1438865421 / 27 380 · drunk walks 804188721 / 31 383 · lakes 2123587627 / 31 440 · decorations (final) 1699819340 / 43 822 |
| `tilesSha` | `141fbb8218bb104c` — first 16 hex of SHA-256 of rows joined by `;`, each row's ids joined by `,` |
| `decorSha` | `2450ba59a1db0194` — SHA-256 prefix of decorations as `col,row,type` joined by `;` in generation order |

Anchor tiles after generation: playerStart and all 10 spawn centres are dirt (carved path starts); field NPCs, story
decorations, lore, mini-boss, hidden-area reward tiles are grass; camp NPC tiles are camp ground; exit (119,60) is wall
(border) and its inner tile (118,60) is grass.

The full map is reproduced in Appendix A.

### 3.4 Camps (CORE layout + RENDER props)

Generator stamp per camp centre `(c, r)` (`src/systems/MapGenerator.ts:517-557`), `halfSize = 5`:
1. `clearArea(c, r, 6, primary, skipCamp = false)` — 13×13 → primary (grass), leaving a 1-tile grass ring around the camp.
2. 11×11 `(c−5..c+5, r−5..r+5)` → camp ground (5).
3. Palisade (6): the `r−5` row (−row side, screen upper-right edge), `dc ∈ [−5,5]` except **`dc ∈ {−1, 0}` (2-tile
   gate)**; the `c−5` and `c+5` columns for `dr ∈ [−5, 3]` (rows `r−5 … r+3`). Rows `r+4`, `r+5` have no side walls and
   the `r+5` row (+row side, screen lower-left edge) has no wall: the camp is **open along its whole +row side** (plus
   the 2-tile gate). 25 palisade tiles per camp. ("North/south" below = −row/+row.)
4. All writes are bounds-checked to the interior `(0 < c < cols−1, 0 < r < rows−1)`.

Camp 1 (15,15): ground cols/rows 10–20; gate at (14,10),(15,10); walls rows 10–18 at cols 10 and 20. Camp 2 (95,100):
ground cols 90–100, rows 95–105; gate (94,95),(95,95).

Runtime camp props (`ZoneScene.buildCampDecorations`, `src/scenes/ZoneScene.ts:1887-1920`), offsets from the centre:

| Prop | Offsets (dc,dr) | Blocks movement? |
|---|---|---|
| campfire | (0,0) | no (the hero respawns on it) |
| well | (+1,−1) | no |
| banner | (−1,−4), (+2,−4), (−5,+4), (+5,+4) | no |
| tent | (−3,−2), (+3,−2), (−2,+2), (+2,+2) | no |
| barrel | (−2,0), (+3,−3) | **yes** |
| crate | (+2,0), (−3,−3) | **yes** |
| torch | (−5,+5), (+5,+5), (−5,−2), (−5,+1), (+5,−2), (+5,+1), (−2,−5), (+3,−5) | no |

After `rebuildWorldCaches()` the scene sets `collisions[round(row)][round(col)] = false` for every barrel/crate
(`src/scenes/ZoneScene.ts:549-557`). This mutates the shared map object (idempotent across visits); the pathfinder
holds a reference to the same grid. Port: apply in the core when loading a zone (`ApplyCampBlockers`).

NPC slots inside a camp (`ZoneScene.spawnNPCs`, `:4467-4484`): `npcs[i]` stands at offset `slots[i % 6]` with
`slots = [(−3,−2), (+3,−2), (−3,+2), (+3,+2), (0,−3), (0,+3)]` — camp 1: blacksmith (12,13), merchant (18,13),
quest_elder (12,17); camp 2: merchant (92,98). Field NPCs at their map positions (`:4795-4804`).

Safe zone / camp effects that touch this area: hero within **5** tiles (`distSq <= 25`) of any camp centre gets ×50 HP
and MP regen (`:83-86`, `:1584-1594`); camp radius 9 (`distSq < 81`, strict) suppresses monster aggro/spawns
(`monsters-ai.md` §6.2), random events (§13) and the town portal (§9.4).

### 3.5 Route facts (golden A* results on the shipped map)

| From → to | steps | diagonal steps | cost (1 / 1.414) | first steps |
|---|---|---|---|---|
| (15,22) → (15,15) camp 1 | 7 | 0 | 7.000 | (15,21),(15,20),(15,19) — enters through the open +row side |
| (15,22) → (118,60) exit inner | 103 | 38 | 118.732 | (16,22),(17,23),(18,24) |
| (15,22) → (108,108) hidden chest | 98 | 81 | 131.534 | (16,22),(17,23),(18,24) |
| (15,15) → (95,100) camp 2 | 86 | 79 | 118.706 | (16,16),(17,17),(18,18) |
| (15,22) → (60,55) mini-boss | 45 | 33 | 58.662 | (16,22),(17,22),(18,23) |
| (15,22) → (18,25) | 4 | 2 | 4.828 | (16,23),(17,23),(18,24),(18,25) — the direct diagonal is blocked by the corner rule (wall at (17,25)) |

---

## 4. MapGenerator (CORE, `src/systems/MapGenerator.ts`)

`generate(map, avoid = []) → map with {tiles, collisions, decorations}` (`:452-717`). Inputs: anchors of `MapData`, theme
(default `plains`), seed (default 42), and `avoid: {col,row,margin?}[]` (margin default 4).

### 4.1 `SeededRandom` (`:13-36`) — Park–Miller minimal standard

```
ctor(seed): state = seed % 2147483647;  if (state <= 0) state += 2147483646
next():     state = (state * 16807) % 2147483647;  return (state − 1) / 2147483646      // [0,1)
nextInt(a,b) = floor(next() * (b − a + 1)) + a                                           // inclusive
chance(p)    = next() < p
```
Use `int64_t` for `state * 16807` (max ≈ 3.6e13, exact) and divide as `double(state − 1) / 2147483646.0`.
JS `%` on a negative seed keeps the sign; seeds are positive in all data. Test vector, seed 12345: states
`207482415, 1790989824, 2035175616, 77048696, 24794531`; `next()` values
`0.096616528086938450, 0.83399462730995810, 0.94770249766083670, 0.035878594532495915, 0.011545852768743274`.

### 4.2 Theme configuration (`:49-95`) — export `map_gen_themes.json`

| theme | primary | secondary | wallDensity | lakeCount | lakeSize | decorDensity |
|---|---|---|---|---|---|---|
| plains | grass 0 | dirt 1 | 0.03 | [2,4] | [8,16] | 0.06 |
| forest | grass 0 | dirt 1 | 0.17 | [2,3] | [6,12] | 0.08 |
| mountain | stone 2 | dirt 1 | 0.12 | [1,3] | [4,10] | 0.05 |
| desert | dirt 1 | stone 2 | 0.04 | [2,4] | [4,8] | 0.04 |
| abyss | stone 2 | dirt 1 | 0.10 | [3,5] | [6,14] | 0.05 |

(`decorTypes` exists but is only a fallback when a theme has no `DECOR_POOLS` entry — never the case.)

Decoration pools (`:274-300`), `[type, weight]`:

| theme | grove | open | tall set |
|---|---|---|---|
| plains | tree 5, tree_round 3, bush 3, grass 2 | grass 6, flower 4, rock 1.5, bush 1, mushroom_red 0.6, boulder 0.5, tree_round 0.5 | tree, tree_round, boulder |
| forest | tree_forest 5, tree_forest_tall 3, fern 3, mushroom 2 | grass_forest 5, fern 2, rock_moss 2, mushroom 1, crystal_blue 0.3, tree_forest_tall 0.6 | tree_forest, tree_forest_tall, crystal_blue |
| mountain | pine 5, boulder_snow 1, rock_slate 1 | rock_slate 4, boulder_snow 1.5, grass_dry 3, dry_shrub 1.5, dead_tree 0.5, pine 0.6 | pine, boulder_snow, dead_tree |
| desert | cactus 1.6, cactus_barrel 3, dry_shrub 2.5, rock_sand 2, boulder_sand 0.6 | rock_sand 3, boulder_sand 1, bones 1.5, grass_dry 2, cactus_barrel 1, dead_tree 0.4 | cactus, boulder_sand, dead_tree |
| abyss | charred_tree 4, crystal 3, boulder_basalt 1, rock_basalt 1 | rock_basalt 4, bones 2, grass_ash 3, mushroom 1, crystal 0.5 | charred_tree, crystal, boulder_basalt |

### 4.3 Pipeline — exact order (every RNG draw is listed; `rng` is the single `SeededRandom(seed)`)

```
tiles = rows×cols filled with primary
(b) border ring = WALL (row 0, row rows−1, col 0, col cols−1)
(s) for r in 1..rows−2, for c in 1..cols−2:  if rng.chance(0.08): tiles[r][c] = secondary        // 1 draw per cell
(w) for r in 2..rows−3, for c in 2..cols−3:  if rng.chance(wallDensity): tiles[r][c] = WALL      // 1 draw per cell
(m) if theme == mountain: ridges (below)
(c) for each camp (array order): camp stamp (§3.4)
(x) clearings, all with skipCamp = true (camp ground/walls are left untouched):
      spawns r=2 · exits r=1 · playerStart r=2 · fieldNpcs r=1 · subDungeonEntrances r=1 · storyDecorations r=1 · avoid r=1
(d) paths with drunkWalk (§4.4), pathTile = DIRT, `carved` grid marks walked tiles:
      1. playerStart → camps[0]                       (if any camp)
      2. camps[i] → camps[i+1], i = 0..n−2
      3. lastCamp → each exit (exit order)            (if any camp)
      4. playerStart → each exit
      5. each spawn (array order) → nearest anchor among [playerStart, camps...] by Manhattan distance,
         strict `<` (ties keep the earlier candidate; playerStart is first)
(e) lakes (§4.5) then cellular automata (§4.6); apply water for r in 2..rows−3, c in 2..cols−3
(b') border ring = WALL again
(g) for each exit lying on the border: inner = (exit.col==0 ? 1 : exit.col==cols−1 ? cols−2 : exit.col,
                                               exit.row==0 ? 1 : exit.row==rows−1 ? rows−2 : exit.row)
      tiles[inner] = DIRT; then clearArea(inner, r=1, primary, skipCamp = false)   // inner ends as primary
(j) collisions[r][c] = tiles[r][c] ∉ {WATER, WALL, CAMP_WALL}
(k) decorations = scatter (§4.7)
```

`clearArea(cc, cr, radius, fill, skipCamp)` (`:214-232`): for `dr` in `−radius..radius` (outer), `dc` in
`−radius..radius`: `r = cr+dr, c = cc+dc`; if `0 < r < rows−1 && 0 < c < cols−1` (and, when `skipCamp`, the tile is not
camp/camp-wall) set `tiles[r][c] = fill`. Square (Chebyshev) area, never touches the border ring.

Mountain ridges (`:491-515`, later milestones) — draw order matters:
```
ridgeCount = nextInt(3,6)
repeat ridgeCount:
  rc = nextInt(5, cols−6); rr = nextInt(5, rows−6); length = nextInt(6,15)
  dirCol = chance(0.5) ? 1 : 0; dirRow = dirCol==1 ? 0 : 1
  repeat length:
    if (1 < rc < cols−2 && 1 < rr < rows−2):
      tiles[rr][rc] = WALL
      if chance(0.4):
        offR = rr + (dirCol==1 ? nextInt(−1,1) : 0)      // draw only in the taken branch
        offC = rc + (dirRow==1 ? nextInt(−1,1) : 0)
        if (1 < offR < rows−2 && 1 < offC < cols−2): tiles[offR][offC] = WALL
    rc += dirCol + (chance(0.3) ? nextInt(−1,1) : 0)      // these draws happen even outside the interior
    rr += dirRow + (chance(0.3) ? nextInt(−1,1) : 0)
```

### 4.4 `drunkWalk(from, to)` (`:149-211`)

```
col,row = from; maxSteps = (cols + rows) * 3; steps = 0
while (col,row) != to && steps < maxSteps:
  steps++
  if 0 < col < cols−1 && 0 < row < rows−1:                       // interior gate for BOTH carve and widen
    if !isCamp(tiles[row][col]): tiles[row][col] = DIRT; carved[row][col] = true
    if rng.chance(0.5):                                          // widen
      ac = col + rng.nextInt(−1,1); ar = row + rng.nextInt(−1,1) // col draw first
      if 0 < ac < cols−1 && 0 < ar < rows−1 && !isCamp(tiles[ar][ac]): tiles[ar][ac] = DIRT; carved[ar][ac] = true
  dx = to.col − col; dy = to.row − row
  if rng.chance(0.7):                                            // directed step
    if |dx| > |dy|: col += sign(dx) else: row += sign(dy)        // tie → row
  else:                                                          // random step, may be a no-op
    d = rng.nextInt(0,3)
    d==0 && col < cols−2 → col++ ; d==1 && col > 1 → col−− ; d==2 && row < rows−2 → row++ ; d==3 && row > 1 → row−−
if 0 < to.col < cols−1 && 0 < to.row < rows−1 && !isCamp(tiles[to]): tiles[to] = DIRT     // NOT marked carved
```
`isCamp(t) = t == 5 || t == 6`. A walk toward a border exit reaches the border column; while there, the interior gate
skips carving **and** the widen draw.

### 4.5 Lakes (`:637-663`) and `groveNoise` (`:313-330`)

```
lakeCount = rng.nextInt(lakeCount.min, lakeCount.max)
keepClear = getPoiPoints(map) ++ avoid (margin ?? 4)
  getPoiPoints: playerStart m5 · camps m9 · spawns m4 · exits m4 · fieldNpcs m3 · subDungeonEntrances m3 ·
                hiddenAreas m4 · storyDecorations m2 · petSpawns m2                        (`:114-127`)
for lake in 0..lakeCount−1:
  radius = 2.2 + rng.nextInt(lakeSize.min, lakeSize.max) * 0.28                       // 1 draw, before attempts
  for attempt in 0..23:
    lc = rng.nextInt(10, cols−11); lr = rng.nextInt(10, rows−11)                     // 2 draws per attempt
    if any p in keepClear: jsHypot2(p.col − lc, p.row − lr) < radius*1.3 + p.margin → continue
    reach = ceil(radius * 1.3)
    if touchesCarved(lc, lr, reach + 2) → continue       // any carved tile in the square |dc|,|dr| <= reach+2 (clamped)
    for dr in −reach..reach: for dc in −reach..reach:
      c = lc+dc; r = lr+dr; if c < 3 || c > cols−4 || r < 3 || r > rows−4: continue
      edge = radius * (0.7 + 0.6 * groveNoise(c, r, seed + lake*131, 3.5))
      if jsHypot2(dc, dr) < edge: water[r][c] = true
    break                                                // one successful placement per lake; 24 failures = no lake
```

`groveNoise(c, r, seed, scale)` — smooth value noise in [0,1], independent of `rng`:
```
h(x,y): n = ToInt32(x*374761393 + y*668265263 + seed*2246822519)      // double sum, left to right
        n = imul(n ^ (n >>> 13), 1274126177)
        return uint32(n ^ (n >>> 16)) / 4294967296
x = c/scale; y = r/scale; x0 = floor(x); y0 = floor(y); fx = x−x0; fy = y−y0
sx = fx*fx*(3 − 2*fx); sy = fy*fy*(3 − 2*fy)
a = h(x0,y0) + (h(x0+1,y0) − h(x0,y0))*sx
b = h(x0,y0+1) + (h(x0+1,y0+1) − h(x0,y0+1))*sx
return a + (b − a)*sy
```
Test vectors (seed 12345, scale 9): (0,0) 0.29843593621626496 · (2,2) 0.32999298519119385 · (10,7) 0.32898032832131274 ·
(50,50) 0.34534253995646536 · (119,3) 0.53900245003280522. Seed 12476, scale 3.5: (10,10) 0.28213746321251321 ·
(40,100) 0.58164606030258292.

### 4.6 Cellular automata (`:240-271`)

`cellularAutomata(water, iterations = 2, birth = 5, death = 4)`: double-buffered; for `r` in `1..rows−2`, `c` in
`1..cols−2`, `n` = number of the 8 neighbours set in the **previous** buffer; alive stays alive iff `n >= 4`, dead
becomes alive iff `n >= 5`; border cells copied unchanged. Water is then written over **any** tile type in
`r ∈ [2, rows−3], c ∈ [2, cols−3]`.

### 4.7 Decoration scatter (`scatterDecorations`, `:332-440`)

Pre-computation (no RNG):
* `blocked` (no decoration at all) and `noTall` (no tall prop) masks, square radii, bounds-checked:
  exits blocked 3 / noTall 5 · camps 6 / 9 · playerStart 2 / 5 · fieldNpcs 1 / 2 · subDungeonEntrances 2 / 3 ·
  storyDecorations – / 2 · spawns – / 2.
* `dirtIsGround = (primary == DIRT)` (desert only).
* `isPath(r,c)` = tile is DIRT **and** at least 2 of its 8 neighbours are DIRT (out-of-bounds = not dirt).
* `distTo(isSource, cap=3)`: multi-source BFS over 8-neighbours giving Chebyshev distance capped at 3 (values 0,1,2,3).
  `pathDist = dirtIsGround ? all 9 : distTo(isPath)`; `obstacleDist = distTo(tile ∈ {WALL, WATER})`.
* Tall rates: forest `open 0.04, grove 0.22, rim 0.5, hug 0.2`; every other theme `open 0.1, grove 0.45, rim 0.8,
  hug 0.3`.
* `lowCover` = `open` pool without tall types, keeping only types matching `/grass|flower|rock|bones|mushroom|fern/` that
  do not start with `boulder` (plains: grass 6, flower 4, rock 1.5, mushroom_red 0.6).

Scan (row-major) `r` in `2..rows−3`, `c` in `2..cols−3`:
```
skip (no draw) if !collisions[r][c] || isCamp(tile) || (tile == DIRT && !dirtIsGround) || blocked[r][c]
inGrove = groveNoise(c, r, seed, 9) > 0.6
density = decorDensity * (inGrove ? 3.2 : 1.3)
if !rng.chance(density): continue                                     // draw 1
type = pickWeighted(inGrove ? grove : open, rng.next())              // draw 2
if type ∈ tall:
  rim  = min(c, r, cols−1−c, rows−1−r) <= 6
  hug  = obstacleDist[r][c] <= 1
  rate = rim ? rimRate : hug ? hugRate : inGrove ? groveRate : openRate
  ok = !noTall[r][c] && pathDist[r][c] > 2 && rng.chance(rate)        // draw 3 ONLY if both prior terms are true
  if ok: for dr in −2..2, dc in −2..2: if (|dr|<=1 && |dc|<=1) || dr==dc || |dr−dc|==1:
            if tallAt[r+dr][c+dc] (in bounds): ok = false
  if ok: tallAt[r][c] = true
  else:  type = pickWeighted(lowCover non-empty ? lowCover : open, rng.next())   // draw 4
push {col: c, row: r, type}
```
`pickWeighted(pool, u)`: `total = Σw` (left to right); `x = u*total`; for each `(t,w)`: `x −= w; if x <= 0 return t`;
fallback last type. Decorations never block movement (web parity; see §22 Q6).

### 4.8 Module-load use and later callers

* Zone maps: generated once at boot with `externalLandmarks` (§3.2). The port ships **baked JSON** for each zone and
  runs the C++ generator only in tests (golden equality) and for runtime callers.
* Runtime callers (later milestones): Abyss Labyrinth floors (`DungeonSystem.generateFloorMap`,
  `src/systems/DungeonSystem.ts:243-315`, random seeds, no avoid list).

### 4.9 Golden tests for the C++ port

1. `SeededRandom` vectors (§4.1); `groveNoise` vectors (§4.5); `jsHypot2` against a table dumped from Node.
2. For every zone in §2.4: `tilesSha`, `decorSha`, walkable count must match (values: emerald_plains in §3.3;
   twilight_forest `69726f1529497960`/`2badccd8450f9f42`; anvil_mountains `d95cade88db96fdd`/`af1cfaccfa3243ad`;
   scorching_desert `da688f33090a24c0`/`11c339464ce5b157`; abyss_rift `02ea788855bd5589`/`1f548ca0721573b8`;
   ember_tower (hand-built, no generator) `71d784ebd7cd5c57`/`f81acaad0c96b137`).
3. emerald_plains RNG checkpoints (§3.3) — pinpoint the first diverging phase.
4. Invariants from `src/__tests__/PathfindingAndMaps.test.ts:468-586` and `MapLiquids.test.ts`: border intact,
   playerStart walkable, a path exists from playerStart to the inner tile of every exit, between all camps and spawns,
   every POI reachable, water present and blocking.

---

## 5. Pathfinding — A* (CORE, `src/systems/PathfindingSystem.ts`)

`findPath(startCol, startRow, endCol, endRow) → (col,row)[]` (`:100-199`) on the zone's `collisions` grid (shared
reference: later mutations such as §3.4 are seen).

```
sc,sr,ec,er = Math.round of the inputs
if !walkable(ec,er) → []        // checked first
if !walkable(sc,sr) → []
if (sc,sr) == (ec,er) → []
g[start] = 0; h = octile(start,end); push start (f = g + h)
dirs = [(−1,0),(1,0),(0,−1),(0,1),(−1,−1),(−1,1),(1,−1),(1,1)]          // (dc,dr), this order
while open not empty:
  cur = pop min f
  if cur == end → return path (start excluded, end included)
  closed[cur] = true
  for (dc,dr) in dirs:
    n = cur + (dc,dr); if !walkable(n) or closed[n]: continue
    if dc != 0 && dr != 0 && (!walkable(cur.col+dc, cur.row) || !walkable(cur.col, cur.row+dr)): continue   // no corner cutting
    g' = cur.g + (diagonal ? 1.414 : 1)
    if g' < g[n]:
      g[n] = g'
      if n has an open node: node.g = g'; node.f = g' + node.h; node.parent = cur; decreaseKey(node)
      else: new node {g', h = octile(n,end), f = g'+h, parent = cur}; push
return []                                                                // unreachable
octile(a,b) = max(|dx|,|dy|) + 0.414 * min(|dx|,|dy|)
```

Binary min-heap on `f` (`:16-87`) — tie-breaking is part of the result and must be copied:
* `push`: append, bubble up while `node.f < parent.f` (stops on `>=`, so an equal-f newcomer stays below).
* `pop`: take root, move last to root, sink down: pick the smaller child with strict `<` (left wins ties), swap while the
  child is strictly smaller.
* `decreaseKey`: bubble up from the node's index.

Costs are `1` and `1.414` (not √2) and the heuristic uses `0.414`. No path smoothing, no max-node limit (worst case the
whole 14 400-cell grid; the web test budget is 100 random paths < 2 s). The golden paths in §3.5 must reproduce exactly.

Consumers (all use rounded entity positions as start): hero click-to-move, attack-target approach, loot approach,
hold-to-move, auto-combat approach (`src/scenes/ZoneScene.ts:824,845,890,3086,4033`), escort NPC (`:6762`).
Monsters do **not** path-find (straight-line steering, `monsters-ai.md` §3.4).

---

## 6. Hero locomotion (CORE)

### 6.1 State (`src/entities/Player.ts:36-46`)

`tileCol, tileRow` (float) · `path: (col,row)[]` · `isMoving` · `moveSpeed` (int, **iso px/s**, base 120,
`floor(floor(120 × (1 + eq.moveSpeed/100)) × spirit.moveSpeedMultiplier)`, `:131-144`, see `classes-stats-skills.md`) ·
`currentSpeed` (px/s, smoothed) · `directMoveMs` · constants `acceleration = 8`, `deceleration = 12`.

### 6.2 Path following (`Player.updateMovement`, `:252-315`) — web metric is iso screen px

Per tick (`dt = delta/1000`), after `ZoneScene` input handling:
```
if path empty && directMoveMs > 0: directMoveMs −= delta; (foot dust every 320 ms); return      // keyboard grace
if path empty:
  currentSpeed *= (1 − 12*dt); if currentSpeed < 0.5: currentSpeed = 0; isMoving = false; anim idle
  return
anim walk
currentSpeed += (moveSpeed − currentSpeed) * 8 * dt                     // first-order, rate 8/s
target = path[0]; T = cartToIso(target); P = hero screen position (== cartToIso(tileCol,tileRow))
dist = |T − P| (px); step = currentSpeed * dt
face toward path[1] (or target) if that vector is > 1.5 px
if dist <= step:  snap (tileCol,tileRow) = target; path.shift(); if path empty: isMoving = false     // leftover step discarded
else:             P += (T−P)/dist * step;  tileCol += (target.col − tileCol) * step/dist; tileRow likewise
```
Notes:
* Speed is measured in **screen px**, so tile speed depends on direction (§6.4).
* `currentSpeed` is never reset by `setPath` (a new path continues at the current speed) and starts at 0 for a fresh walk:
  from rest, 63 % of full speed after 125 ms, 95 % after 375 ms.
* `setPath(p)`: `path = p; isMoving = p.length > 0` (`:208-211`). `moveTo(c,r)` teleports (`:184-190`).
* On arrival the hero stops exactly on the node; the walk animation keeps playing until `currentSpeed` decays below 0.5
  (~25 frames ≈ 0.4 s from 120 px/s) — **QUIRK W2** (render-only): in 3D drive the locomotion blend from the actual
  velocity, not from `currentSpeed`.
* Dead hero (`hp <= 0`): path cleared, no movement (`:219-223`).

### 6.3 Direct movement: keyboard, gamepad, touch joystick (`ZoneScene.handleKeyboardMovement`, `src/scenes/ZoneScene.ts:2113-2156`)

```
if hero dead: return
dx = dy = 0
W or ↑: dx −= 1, dy −= 1      S or ↓: dx += 1, dy += 1      A or ←: dx −= 1, dy += 1      D or →: dx += 1, dy −= 1
if dx == dy == 0 && touch joystick active: (dx,dy) = (jx + jy, −jx + jy)       // jx,jy ∈ [−1,1] screen, no dead zone
if dx == dy == 0 && gamepad: s = left stick; if hypot(s) > 0.18: (dx,dy) = (s.x + s.y, −s.x + s.y)
if (dx,dy) != 0:
  holdMove = null; path = []
  len = sqrt(dx²+dy²); lastMoveDirection = (dx/len, dy/len)                   // used by dodge (combat-feel.md)
  step = moveSpeed * (delta/1000) * 0.015                                       // tiles this tick → 1.8 tiles/s at 120
  n = (tileCol, tileRow) + (dx,dy)/len * step
  if round(n) in bounds && collisions[round(n)]: hero.moveDirect(n)            // else: no movement this tick
```
`moveDirect` = `moveTo` + `path = []`, `isMoving = true`, `directMoveMs = 120`, walk anim, face the motion
(`src/entities/Player.ts:197-206`). Joystick mapping `src/systems/MobileControlsSystem.ts:202-209`; thumb offset is
clamped to the base radius and normalised by it (`:283-297`). The analog magnitude is discarded (always full speed).

Properties to keep in mind: only the destination tile is checked — no corner check, no sliding along walls (pushing
diagonally into a wall stops the hero completely: **QUIRK W3**, recommend FIX = try the col-only then the row-only
component); no acceleration.

### 6.4 Speed model for the 3D port (decision needed, default proposed)

Web tile speeds at `moveSpeed = 120`:

| Mode / direction | tiles/s | screen look |
|---|---|---|
| Path, cardinal tile step (±col or ±row; screen diagonal) | 120/35.777 = **3.354** | 120 px/s |
| Path, (+1,+1)/(−1,−1) step (screen vertical) | 120/32×√2 = **5.303** | 120 px/s |
| Path, (+1,−1)/(−1,+1) step (screen horizontal) | 120/64×√2 = **2.652** | 120 px/s |
| Keyboard / stick / joystick, any direction | 120×0.015 = **1.800** | 60–120 px/s |

**QUIRK W4** — the hero moves at constant *screen* speed along paths (2× faster in world terms toward/away from the
camera) and at a much lower uniform tile speed with the keyboard. With a 3D camera this makes feet slide.
**Recommended FIX (default):** one uniform ground speed for every input mode:
`heroTilesPerSec = moveSpeed / 35.777` (= 3.354 tiles/s at 120; identical to the web on cardinal steps, and the same
px→tile factor 36 that `combat-feel.md` uses), first-order acceleration (rate 8/s) and decay (12/s) on that tile speed
for path movement; direct input uses the same target speed (decide whether it keeps the instant start — §22 Q1).
Implement as `HeroSpeedModel { UniformTiles (default), IsoPixelParity }` so a parity build can reproduce the web exactly
(IsoPixelParity = §6.2/§6.3 verbatim, with `cartToIso` as the metric).

### 6.5 Facing (RENDER-ONLY)

The web faces the screen-space move/target vector with hysteresis (`CharacterAnimator.resolveFacing`). In 3D: yaw the
mesh toward the ground-plane velocity (paths: toward `path[1]` when present, to avoid zig-zag flips), interpolated
(suggest 720°/s); while attacking face the target (combat spec).

---

## 7. Pointer input: click-to-move and hold-to-move (CORE logic, UE supplies the tile)

### 7.1 Click (press) priority chain (`ZoneScene.handlePointerDown`, `src/scenes/ZoneScene.ts:774-857`)

Executed on press (not release). `tile` = picked tile (§1.4). `hero` = hero float position. First match wins:

| # | Condition | Action |
|---|---|---|
| 0 | story cinematic playing | ignore |
| 1 | touch landed on a touch control (joystick/skill/button) | ignore (`claimsPointer`) |
| 2 | **right button** (mouse only: touch never has one, §7.3) | town portal (§9.4); evaluated before the dead check |
| 3 | hero dead | ignore |
| 4 | loot drop with `|l.col−tile.col| < 1.5 && |l.row−tile.row| < 1.5` (first in drop order) | `pickupLoot`: if `distSq(hero, loot) > 4` → path to the loot tile (no auto-pickup on arrival, but auto-loot may grab it); else pick up now |
| 5 | NPC with `distSq(npc, tile) < 3.24` (nearest) **and** hero within 3 tiles (Euclidean `<= 3`) of that NPC | interact (shop / quest card / stash; `talk` progress) — far NPCs fall through to the next rows (QUIRK W5) |
| 6 | Ember Tower object (later milestone) | handled by the homestead |
| 7 | sub-dungeon entrance within the 1.5 box and hero `distSq <= 9` | enter (later; click-only, no proximity entry) |
| 8 | labyrinth portal: `|tile − (60,60)| < 2` on both axes (abyss_rift only) and hero `distSq((60,60)) <= 9` | tier picker (later) |
| 9 | hidden-area reward within 1.5 box and hero `distSq <= 4` | collect (§10.3) |
| 10 | living monster with `|m−tile| < 1.5` on both axes (spatial query radius 2, grid order) | `attackTarget = m`, `TARGET_CHANGED{id,name}`, path to the monster's rounded tile |
| 11 | exit with `|e−tile| < 1.5` on both axes | `changeZone` immediately (labyrinth: leave floor; sub-dungeon: exit) — **no distance check** (QUIRK W6) |
| 12 | tile in bounds | path to tile; if the path is non-empty: `setPath`, `attackTarget = null`, `TARGET_CHANGED{null}`; **always** start hold-to-move `{pointerId, col, row, repathAt = now + 120}` |

**QUIRK W5** — clicking an NPC farther than 3 tiles just walks to its tile; the player must click again on arrival.
Recommend FIX (D2 feel): remember a pending interaction and fire it when the hero comes within 3 tiles; keep PARITY if
undecided.
**QUIRK W6** — clicking an exit tile from anywhere on screen (~8 tiles) changes zone instantly. Recommend FIX: path to
the exit's inner tile and let the proximity trigger (§9.2) fire.

### 7.2 Hold-to-move (`ZoneScene.updateHoldMove`, `:865-894`; `HOLD_MOVE_REPATH_MS = 120`, `:113`)

Runs every tick after keyboard handling (any keyboard/stick input clears `holdMove`):
```
if holdMove == null: return
if pointer released || hero dead: holdMove = null; return            // the hero finishes the current path
tile = clamp(pick(pointer screen pos re-projected NOW), [0..cols−1] × [0..rows−1])   // camera moved → re-pick every tick
if jsHypot(tile.col − hero.col, tile.row − hero.row) < 0.6: path = []; return       // stand still under the cursor
moved = tile != (hold.col, hold.row)
if !moved && now < hold.repathAt && path non-empty: return
hold.col,row = tile; hold.repathAt = now + 120
goal = collisions[tile] ? tile : findWalkableNear(tile, 3)     // Chebyshev rings 1..3, dr outer, dc inner (`:6655-6666`)
if goal == null: return
p = findPath(round(hero), goal); if p non-empty: setPath(p)
```
While `holdMove` is active the hero's auto-attack is suspended (`ZoneScene.handleCombat`, `:2833`). Every cinematic /
labyrinth modal clears it (`:1347-1351`). Clicking a wall/water tile therefore also moves to the nearest walkable tile
within 3 rings on the following tick (the press is still down).

### 7.3 Touch (web behaviour; RENDER-ONLY mapping)

What the web does (`src/systems/MobileControlsSystem.ts`, created only when `isMobileDevice()`, `:16-24`):

* A finger on the world is a **left press** and runs the §7.1 chain. Row 2 never fires on touch: a touch pointer
  never reports a right button (Phaser sets `buttons = 1` on `touchstart`, `node_modules/phaser/src/input/Pointer.js:762`),
  and the long-press context menu is suppressed (`src/systems/MobileShell.ts:54`). Tapping
  an NPC within 3 tiles talks to it (row 5). Tapping loot picks it up or walks to it (row 4). Tapping a monster targets
  it, and tapping the ground walks there.
* Keeping the finger down is hold-to-move **from the first frame**: row 12 always arms `holdMove` on the press, and
  there is no long-press delay. The same pointer-id rules as §7.2 apply.
* A press that starts on a touch control is claimed and never reaches the world (`claimsPointer`, `:162-168`;
  `src/scenes/ZoneScene.ts:777`).
* Controls built (`build`, `:213-220`) and their layout constants (`CSS`, `:30-56`):
  * the joystick (bottom-left, `:245-273`, feeds §6.3);
  * the corner LOCK button → `UI_TARGET_CYCLE` (`:314-318`);
  * dodge → `UI_DODGE_REQUEST` (`:322-325`);
  * up to 6 skill buttons → `UI_SKILL_CLICK` (`:329-340`);
  * the top-left toggles: auto-combat, auto-loot and combat log (`:397-421`);
  * the top-right panel row: bag, character, skills, map, homestead, pets and quest (`:460-494`).
* **There is no town-portal control.** `useTownPortal` has no EventBus event, and only two places call it: the right
  button (`src/scenes/ZoneScene.ts:778-781`) and key R (`:2191-2193`). A touch player therefore cannot use the town
  portal, the game's only fast travel to camp (§9.4). Only walking or dying (§9.5) brings them back. The
  port adds a button (§7.4).

UE: Enhanced Input with a touch interface. A world touch becomes the same press as a left click, through the ground and
actor trace of §1.4. Virtual controls are UMG widgets that consume their touches (`FReply::Handled()`, the equivalent
of `claimsPointer`).

### 7.4 Pointer-free world actions: town portal, interact, auto toggles (port decision, CORE + input layouts)

**Web gap (verified).** Several world actions are reachable only through the mouse or keyboard:

| Action | Keyboard + mouse | Touch | Gamepad (`handleGamepadInput`, `src/scenes/ZoneScene.ts:2225-2245`) |
|---|---|---|---|
| Town portal (§9.4) | `R` (`:2191-2193`), RMB (`:778-781`) | **none** | **none** |
| Talk to an NPC (shop, quest card, stash, `talk` progress) | LMB on the NPC (§7.1 row 5) | tap on the NPC | **none**: no pointer |
| Pick up loot | LMB (row 4) or auto-loot | tap or auto-loot | auto-loot only. It defaults to `off` (`src/entities/Player.ts:56`) and the pad cannot change it |
| Hidden-area reward (row 9); random-event puzzle prop (`pointerdown` on the prop, no distance check, `src/scenes/ZoneScene.ts:3601-3604`) | LMB | tap | **none** |
| Toggle auto-combat | `Tab` (`:2160-2163`) | toggle (`MobileControlsSystem.ts:404-410`) | **none** |
| Cycle auto-loot | HUD button (`save-ui-input.md` §6.2) | toggle (`:412-416`) | **none** |
| Advance / skip a story beat | Space, Enter or click / Esc (`src/scenes/StoryScene.ts:64-67`) | tap / skip button | **none**. The `waitInput()` beats (`StoryScene.ts:95-98`, used at `:248`, `:360`, `:431`, `:507`) wait forever |

The web pad maps only the left stick (move), buttons 0/2/3/5 (skills 1–4), button 1 (dodge) and button 4 (target cycle).
The town portal therefore cannot be used on touch or gamepad. A gamepad-only player can walk and fight, but cannot get
past the prologue. They also cannot accept or turn in a quest, shop, pick up gear or open the hidden cache, so
**Chapter 1 cannot be completed with a gamepad alone**. The panels themselves (dialogue tree, quest card, shop, stash,
inventory) also have no gamepad navigation; `save-ui-input.md` §5.5 / OQ-INPUT-3 owns that. **QUIRK W16**, FIX:

**1. Core actions** (fields of the `InputFrame` in `save-ui-input.md` §5.6; they are applied where `handleSkillInput`
runs, §17 step 4, with the same freeze gates: cinematic, labyrinth modal, `hp <= 0`):

* `townPortal` → `UseTownPortal()`, exactly as in §9.4, with every refusal and log unchanged.
* `toggleAutoCombat`: flip `autoCombat` and log `zone.combat.autoCombat {state}`, the same as `Tab`.
* `cycleAutoLoot`: `off → all → magic → rare → legendary → off`, the same as the touch toggle.
* `interact` → `FindInteractTarget(hero)`, then the handler the pointer chain would call for that object. Each kind
  keeps the web's own range for that object:

| Order | Kind | In range when | Action |
|---|---|---|---|
| 1 | loot drop | `distSq(hero, drop) <= 4` (the immediate-pickup range of `pickupLoot`) | pick up now (`pickupLoot`; bag full → log `sys.inventory.bagFull` and the drop stays, `src/systems/InventorySystem.ts:43-45`) |
| 2 | NPC | Euclidean `dist(hero, npc) <= 3` (the row 5 `isNearPlayer(…, 3)` rule) | `interactNPC` (§7.1 row 5) |
| 3 | Ember Tower object (later) | the tower's own rule (§16: within 1.6 tiles of the object, hero within 3) | as the click |
| 4 | sub-dungeon entrance (later) | `distSq <= 9` (row 7) | enter |
| 5 | labyrinth portal (later) | `distSq(hero, (60,60)) <= 9` (row 8) | tier picker |
| 6 | hidden-area reward | `distSq <= 4` (row 9) | collect (§10.3) |
| 7 | unresolved random-event puzzle prop | `distSq <= 4` (port range: a web click has none) | open the puzzle prompt (§13.3) |

The rule picks the in-range candidate with the smallest `distSq` from the hero. Ties go to the lower order above (the
§7.1 row order), then to the first entry in the object's own list order (`lootDrops`, `npcs`, …). Nothing in range is a
no-op with no log. Exits, gather nodes, clues, lore pickups, story decorations and potions stay proximity-only, so they
need no interact action.

The core also publishes `InteractPrompt { kind, id, col, row } | none` every tick: the current `FindInteractTarget`
result. The UI uses it to highlight the target and show a button hint such as "RT 交谈 / 拾取 / 打开". The core
emits `InteractPromptChanged` when it changes.

**2. Default bindings** (rebindable later; the web bindings stay as they are):

| Action | Keyboard + mouse | Touch | Gamepad (W3C standard index) |
|---|---|---|---|
| Town portal | `R`, RMB (parity) | **new button** (below) | **D-pad Down (13)** |
| Interact | LMB on the object (parity); **`E`** (new; free once the dev-only `Ctrl+Shift+E` export is dropped) | tap on the object (parity). No extra button; the W5 fix (pending interaction) makes one tap on a far NPC walk there *and* talk | **RT (7)** |
| Auto-combat | `Tab` | top-left toggle | **D-pad Up (12)** |
| Auto-loot cycle | HUD button | top-left toggle | **D-pad Right (15)** |
| Story beat next / skip | Space, Enter, click / Esc | tap / skip button | **A (0)** next, **Menu (9)** skip. A story beat freezes the world tick (§17 step 1), so A does not also cast skill 1. The UE layer must swallow the A press that closes the last beat: skill 1 needs a fresh press after the beat ends. The web would re-fire it, because its pad edge state is not updated while frozen (`:2229-2245` are skipped) |

The pad keeps its web layout: left stick moves, A/X/Y/RB cast skills 1–4, B dodges and LB cycles the target. D-pad Left
(14), LT (6), View (8), L3/R3 and the right stick stay free for `save-ui-input.md` OQ-INPUT-3 (panel access, skills 5–6,
UI focus). A gamepad-complete Chapter 1 also needs CommonUI focus navigation of the dialogue tree, the quest card (accept,
turn-in, reward choice), the shop, the stash and the inventory. That work belongs to the UI spec. Recommendation: ship it
in milestone 1, because macOS and Windows players use controllers and so do MFi and Android pads (§22 Q10).

**3. Touch town-portal button** (RENDER-ONLY layout, CORE action):

* **Placement.** A 4th square in the top-left toggle row, after the log toggle: 44×44 CSS px with a 6 CSS px gap, so its
  left edge is at `m + 66 + 6 + 66 + 6 + 44 + 6 = 206` CSS px (`k·206` logical px; `k`, `m` and the toggle sizes are
  from `CSS`, `MobileControlsSystem.ts:30-44`). It shares the toggles' `y`, their `secondary` frame at α 0.9 and their
  icon-over-caption square style.
* **Icon and caption.** A new HUD icon `portal` (a blue swirl ring `0x4488ff`, the same colour as the portal VFX) with
  the caption `sys.mobile.portal` (zh-CN `回城`, en `Portal`).
* **Why there.** The button sits away from the joystick and the skill fan on purpose, so a combat thumb cannot open a
  1.5 s channel by accident.
* **Collision check.** At `k ≥ 1.66` the row's right edge (≈ 250·k logical px) reaches the touch target frame (x 414–674,
  y 10–62, `save-ui-input.md` §6.2). The row spans y `m … m + 44·k`. The UMG layout must then start the target frame right of the row (or wrap the row).
* **Press.** The press claims the touch, so the world never sees it, and sets `townPortal` on press like every web touch
  button.
* **Feedback.** While the portal channels, show the 1 500 ms channel as the same radial sweep the skill buttons use
  (total 1500). Dim the button to α 0.5 while `CanUseTownPortal()` would refuse (dead, already within the camp's safe
  radius, already portaling). A press while dimmed still runs the action, so the web's refusal log appears.
* **Cinematics.** The button is inert during cinematics, like all touch controls.

**4. Cross-spec follow-ups** (owned by `save-ui-input.md`; listed here because this file is the source of the
decision):

* Add "town portal" to Q30's list of touch features that cannot be reached.
* Add `IA_Interact` and `IA_AutoLootCycle` to §5.6.
* Add `interact` and `cycleAutoLoot` to its `InputFrame`. Its `toggleAuto` is `toggleAutoCombat` here.
* Add the new pad buttons to §5.5 and the portal button to the §5.7.2 layout table.

---

## 8. Camera (RENDER-ONLY, rules the UE module must honour)

Web (`src/scenes/ZoneScene.ts:111,566-567,609`):
* Follows the hero sprite (feet) with `startFollow(sprite, roundPixels, lerp 0.08, 0.08)`: snaps onto the hero at zone
  start, then per **frame** `scroll += (target − scroll) × 0.08` → at 60 fps an exponential approach with
  **k = 5.0 s⁻¹** (`alpha = 1 − e^(−5.0·dt)`). No dead zone, no bounds (the camera shows the void beyond the map edge).
* Fixed zoom `ZONE_CAMERA_ZOOM = 1.8` (× render scale). No player zoom control, no rotation.
* Visible area: 711×400 world px = **15.7 tiles wide × 17.7 tiles deep** of ground around the hero.
* Fade-in 400 ms on zone entry; fade-out 400 ms before a zone change (`VFXManager.zoneTransition`,
  `src/systems/VFXManager.ts:250-256`); camera flashes (death 80 ms α0.6 white, town portal 200 ms α0.5 `#4488ff`,
  throttled to one per 200 ms, `:110-123`); shakes per hit weight (throttle 100 ms, `:103-108`; values in
  `combat-feel.md`).

3D proposal:

| Parameter | Value |
|---|---|
| Rig | Spring-arm-less fixed rig: camera position = focus − forward × distance; no collision probe (occluders fade instead, §15.6). |
| Focus | hero location, Z = +50 cm, smoothed with k = 5.0 s⁻¹ (frame-rate independent form); snap on zone entry / teleport / respawn. |
| Yaw / pitch | yaw **−135°** (§1.3); pitch **−30°** for parity framing (ground depth 17.7 tiles), allowed range −30°…−45° (decide, §22 Q2). |
| FOV / distance | horizontal FOV **30°**, distance **2 932 cm** → 15.7 m wide at the focus (`d = 785.5 / tan 15°`). Near-orthographic look; keeps aggro/attack ranges readable as in the web. |
| Aspect | Keep the 16:9 framing: for wider screens maintain the vertical extent (UE `MaintainYFOV`), for narrower (iPad 4:3) maintain the horizontal one (`MajorAxisFOV` behaviour). The web letterboxes instead. |
| Bounds | none (parity); the terrain skirt (§15.1) hides the void. |
| Zoom | none in milestone 1 (optional pinch/wheel later, §22 Q2). |

---

## 9. Zone transitions, exits, town portal, death, save position

### 9.1 Zone entry (`ZoneScene.init/create`, `:337-707`) — world-relevant order

1. Resolve map (unknown → emerald_plains); `campPositions = camps.map(col,row)`; set zone palette (theme).
2. Fresh per-zone systems (`GameSession.beginZone`, `src/game/GameSession.ts:46-57`): combat, loot, status effects,
   elite affixes, **RandomEventSystem** (cooldown starts fresh). Session systems (inventory, quests, homestead, pets,
   achievements, saves, mercenaries) persist.
3. Exploration grid reset to all-unexplored (§10.1).
4. Hero created at `(targetCol, targetRow)` if given, else `playerStart`; transition stats restored (§9.3).
5. Load-from-menu: `restoreFromSave` (position rules §9.6).
6. Pathfinder bound to `collisions`. Spawns (monsters, NPCs, field NPCs, rare pets, mini-boss, lore, sub-dungeon
   entrances, story decorations, labyrinth portal, mercenary), QuestWorld + StoryDirector (not in labyrinth), pet,
   escort, defend, hunts, soul echo, camp decorations, Ember Tower, world caches, **camp blockers (§3.4)**.
7. Terrain, camera follow + zoom, lighting (`setZone`), VFX, weather, trails, post FX, ambient dust, fade-in 400 ms,
   colour grade, input, mobile controls, UIScene.
8. `exploredZones.add(id)`; achievement `explore`; emit `ZONE_ENTERED {mapId}`; log `zone.enterZone {zoneName,min,max}`.
9. StoryDirector `start()` → chapter card on first visit, else the zone banner (name 28 px + `Lv.a-b`, fade in 800 ms,
   hold until 3 000 ms, fade out 800 ms, `:5942-5984`). Then `autoSave()`.

### 9.2 Exits (CORE)

* Proximity trigger, every tick (`checkExitProximity`, `:4206-4221`): for each exit in order, if
  `distSq(hero, exit) < 2.25` (1.5 tiles, strict) → labyrinth: try leaving the floor; sub-dungeon: exit; else
  `changeZone(targetMap, targetCol, targetRow)`; stop after the first.
* Click trigger: §7.1 row 11.
* `changeZone` (`:4180-4204`): guard `isTransitioning`; set it; `autoSave()` (not awaited); fade-out 400 ms; then restart
  the zone with `{classId, mapId, targetCol, targetRow, miniBossDialogueSeen, loreCollected, discoveredHiddenAreas,
  playerStats}`.
* Exits sit on the border wall ring, so the exit tile itself is never walkable; the generator guarantees a 3×3 walkable
  pad around the inner tile (§4.3 g). Rendering: an `exit_portal` sprite at the exit tile with a blue glow `0x4488ff`;
  minimap green square.

**QUIRK W7 (bug) — arrival bounce.** Arrival points can lie inside the destination's exit trigger: twilight_forest →
emerald_plains arrives at (118,60), 1 tile from the exit (119,60) (`distSq = 1 < 2.25`), so on the first tick the hero is
sent straight back to the forest; the same happens for every "backward" exit (arrival (118,118) vs exit (119,119),
`distSq = 2`). Forward arrivals at (2,58) vs exits at (0,58) (`distSq = 4`) are fine. **FIX** (recommended): an exit is
*armed* only after the hero has been outside it with `distSq > 6` since arrival (same pattern the Ember Tower portal uses,
`src/systems/EmberTower.ts:400-404`); alternatively move arrival points 2 tiles inward. Chapter 1 is affected only once
chapter 2 exists.

### 9.3 State carried across a zone change (`getPlayerTransitionStats`, `:4230-4249`)

`level, exp, gold, hp, mana, stats{}, freeStatPoints, freeSkillPoints, skillLevels{}, spirit, buffs[] (with their
absolute startTime on the monotonic clock), autoCombat, autoLootMode` + the three id sets above. In the port this is
simply "the session keeps the hero"; only the per-zone runtime (monsters, drops, events, exploration grid) is rebuilt.
Loot and potion drops on the ground are lost on zone change (parity).

### 9.4 Town portal (`useTownPortal`, `:5801-5885`) — the only "waypoint" in the web

Web triggers: key **R** (`handleSkillInput`, `src/scenes/ZoneScene.ts:2191-2193`) and the **right mouse button**
(`handlePointerDown`, `:778-781`). The right button is checked before the dead check, and the portal refuses a dead hero
itself. Nothing else calls `useTownPortal`:

* there is **no touch control** (`MobileControlsSystem` builds none, §7.3);
* there is **no gamepad button** (`handleGamepadInput`, `:2225-2245`);
* there is no EventBus event;
* the shop's 传送卷轴 `c_tp_scroll` (sold by the merchant 商人, `src/data/npcs.ts:12-17`) is consumed by "Use", but its
  `teleport` effect is ignored (`src/systems/InventorySystem.ts:424`, `src/scenes/UIScene.ts:4297-4302`).

The help panel lists only `R / 鼠标右键` (`src/scenes/MenuScene.ts:738`). On touch and gamepad the portal cannot be
used at all (QUIRK W16). **Port:** add the core `townPortal` action, the touch button and pad **D-pad Down** of §7.4.
The loot spec decides whether `c_tp_scroll` also calls `UseTownPortal()` (consumed only on success).

Behaviour:
```
if hero dead || isPortaling || isTransitioning: return
if in sub-dungeon or labyrinth: dest = exits[0] (none → return); if distSq(hero,dest) < 9: log zone.teleport.alreadyAtExit; return
else: dest = camps[0] centre (none → return); if distSq(hero,dest) < safeR² (81): log zone.teleport.alreadyAtCamp; return
isPortaling = true; path = []; isMoving = false; attackTarget = null; log zone.teleport.opening
VFX: two expanding rings + glow at the feet (1200 ms), hero alpha flicker 200 ms ×4
after 1500 ms: camera flash 200 ms α0.5 #4488ff; SFX zone_transition; hero.moveTo(dest); isPortaling = false; log arrival
```
The 1 500 ms channel is **not interrupted** by damage, keyboard movement or death (a dead hero is moved too; respawn
also goes to camp). Destination is always `camps[0]` (15,15 in emerald_plains), not the nearest camp. Keep parity;
§22 Q4 asks whether to make the channel interruptible.

There are **no D2-style waypoints**, no fast travel between zones, and the world-map panel (§9.7) is informational.
Ember Tower hearthstones are a later-milestone teleport (§16).

### 9.5 Death and respawn (`handlePlayerDied`, `:896-959`)

Death penalty first (`SoulEcho`, other spec). Flash 80 ms; "death" text fades in 250 ms; after **1 100 ms**: in a regular
zone `respawnAtCamp(camps[0])` — full HP/MP, `moveTo(camp centre)`, path cleared, target cleared — and camera fade-in
300 ms. In labyrinth / sub-dungeon: restart into the parent zone at its `camps[0]` (fallback playerStart, then (3,3)).

### 9.6 Save position and new game

* Autosave stores the **float** hero position and the map id (in the labyrinth: abyss_rift (15,22)) (`:4251-4296`).
* Load (`:4314-4332`): if `round(saved)` is out of bounds or not walkable → move to the **nearest camp centre** (squared
  distance to the float position; `findNearestWalkablePosition`, `src/systems/SaveSystem.ts:132-165`) and log
  `zone.save.positionReset`; else `moveTo(saved float position)`.
* New game: `MenuScene` starts `emerald_plains` with no target → playerStart (15,22) (`src/scenes/MenuScene.ts:1365`).
* Milestone 1 gating (port decision): the emerald_plains exit leads to chapter 2. Until chapter 2 ships, render the exit
  as a sealed gate and show a "coming soon" line instead of transitioning (core flag `exit.enabled`).

### 9.7 World-map panel (key **M**, `UIScene.toggleMap`, `src/scenes/UIScene.ts:1936-1997`) — RENDER-ONLY UI

A row of cards in `MapOrder`: zone name, `Lv.a-b`, gold road arrows between cards, the current zone highlighted with a
bobbing pin (600 ms yoyo). No interaction beyond close. Port as a UMG panel.

---

## 10. Exploration, fog of war, hidden areas

### 10.1 What the web actually does

The fog-of-war **renderer is not used in zones**: `FogOfWarSystem` only contributes its tile textures at boot
(`src/scenes/BootScene.ts:111`); `ZoneScene` never instantiates it. The game shows no fog; the minimap shows the whole
map. What *is* used is a per-visit exploration grid (CORE):

`ZoneScene.updateExploredTiles` (`src/scenes/ZoneScene.ts:4809-4829`), every tick, `vr = 10`:
```
for r in max(0,floor(hr−10)) .. min(rows−1,ceil(hr+10)), c likewise:
  if (c−hc)² + (r−hr)² <= 100: explored[r*cols+c] = 1
```
The grid is reset on every zone entry (`:396-397`) and is **not saved**. The save field `exploration` (`fogData`) is
never written — always `{}` (dead data, `:196,4276,4372`). Port: `ExplorationGrid` (bitset), same rule; do not persist.

### 10.2 `FogOfWarCore` (CORE, optional; `src/systems/FogOfWarCore.ts`)

Pure logic with unit tests (`src/__tests__/FogOfWarOptimization.test.ts`). Port it so a later milestone can turn on fog
or a fogged minimap; keep it disabled in milestone 1 (§22 Q3).
* State per tile: 0 unexplored / 1 explored; `prevAlpha` (uint8 quantised alpha) for dirty diffing; `viewRadius`
  default 10; `edgeBand = 3`.
* `update(pc, pr)`: if `(pc,pr)` equals the last call → return false (exact float equality). Else mark all tiles with
  `(c−pc)²+(r−pr)² <= vr²` explored, then for **every** tile compute `dist = sqrt(...)` and
  ```
  dist <= vr−3            → alpha 0
  dist <= vr              → alpha = (dist − (vr−3))/3 × 0.15; < 0.01 → 0
  explored (outside vr)   → dist < vr+3 ? 0.18 + (dist−vr)/3 × 0.15 : 0.35
  unexplored              → 0.85
  q = round(alpha×255); if q != prevAlpha[i]: dirty.add(i); prevAlpha[i] = q
  ```
  returns true.
* `getAlpha(c,r) = prevAlpha/255` (0.85 out of bounds); `isExplored`; `getExploredData()/loadExploredData(bool[][])`
  (rejects wrong dimensions; resets `prevAlpha` and the last position); `invalidate()`.
* Renderer (RENDER-ONLY): 16 pre-baked diamond overlays at `alpha = step/16 × 0.85`, `step = clamp(round(alpha/0.85 ×
  16), 1, 16)`, `0` when `alpha < 0.01` (`src/systems/FogOfWarSystem.ts:21-66`). 3D equivalent, if ever enabled: a
  120×120 R8 texture of `alpha` sampled by a post-process / decal material over the ground, bilinear-filtered.

### 10.3 Hidden areas (CORE, `src/scenes/ZoneScene.ts:4831-5037`)

* Bounds: explicit `startCol/startRow/endCol/endRow`, else `col±radius, row±radius`.
* Discovery check every tick for areas not yet in `discoveredHiddenAreas`: **all five** check points explored —
  the four bound corners and the centre (§10.1 grid). Because the grid resets per visit, all five must be seen during one
  visit (standing at the centre of emerald_plains' cache sees all corners: `√72 = 8.49 <= 10`).
* On discovery: add id to `discoveredHiddenAreas` (saved; carried across zone changes); emit
  `HIDDEN_AREA_DISCOVERED {area}`; log `zone.hiddenArea.discovered {areaName}`; centred banner with `discoveryText`
  (fade 300 ms, hold 3 000 ms); spawn one interactable per reward.
* Collect (§7.1 row 9: click within the 1.5 box, hero `distSq <= 4`):
  `chest` → `lootSystem.generateEquipment(levelRange[1], value=='legendary' ? legendary : value=='rare' ? rare : magic)`
  (emerald_plains: level 7, rare) straight into the inventory; `gold_pile` → `gold += parseInt(value || "100")`
  (emerald_plains 200); `lore` → log only. The prop opens/fades (700 ms + 400 ms) and is removed.

**QUIRK W8** — uncollected rewards are lost forever if the hero leaves the zone: discovery is persisted but rewards are
spawned only at discovery time, and collection is not persisted. **FIX**: persist collected reward indices per area and
respawn uncollected rewards of discovered areas on zone entry.

---

## 11. Minimap (RENDER-ONLY UI, data rules CORE-queried; `src/scenes/UIScene.ts:2889-3036`)

* Size 104 logical px square, top-right under the info plate (`HUD.minimap`, `:118,167`; mobile pushed down 118 px),
  dark backing `0x07060a`, carved frame. Redrawn every **250 ms** (`:5918-5921`).
* **Orientation: tile space, north-up** (col → right, row → down; `sx = 104/cols`). It is *not* rotated like the iso
  view (screen-up on the world = up-left on the minimap). §22 Q5 proposes a 45° rotation in 3D.
* Layers, in draw order:
  1. Every tile, colour by type (§2.1), alpha 0.75. No fog (whole map visible from the start).
  2. Hero: black disc r 4 α0.8 + `0x7fd4ff` disc r 2.8.
  3. Exits: `0x00e676` 4×4 squares.
  4. Labyrinth portal (abyss_rift only): `0xFF6600` disc r 3 + `0xFF3300` ring r 4.
  5. Quest givers: for every camp NPC (drawn at the **camp centre**) and field NPC whose definition has quests:
     turn-in ready → `0xffd23a` r 3.2; otherwise if a quest is available at the hero's level → `0xf5e6a8` r 2.4; black
     halo r+1.2 α0.7.
  6. Guide target of the tracked quest (`questWorld.guideTarget`): 10-point star (outer 4.5, inner ×0.45) on a
     `0x1a0f04` disc, fill `0xffd23a` if reason `turn_in` else `0xffb347`.
  7. Every living monster on the map: aggro `0xff4444` α0.9 r 2; else `0xcc6644` α0.5 r 1.5.
  8. Per active quest in this zone: `questArea` circle (main `0xf1c40f`, side `0x95a5a6`, fill α0.25, stroke α0.6,
     radius = area radius × sx); unfinished `explore` locations `0xf39c12` 3×3 α0.5; `investigate_clue` `0x9b59b6`
     3×3 α0.6; `escort` destination `0xe67e22` 4×4 α0.6; defend target `0xe74c3c` r3 α0.5 + ring r4; escort start
     `0xe67e22` r2 α0.5.
* 3D: bake a 120×120 RGBA texture of tile colours once per zone (nearest filtering), draw markers in UMG from a core
  `MinimapSnapshot` (positions in tile units). Keep the 250 ms marker refresh or update every frame (cheap).

---

## 12. World interactables (CORE rules)

### 12.1 Lore pickups (`spawnLoreCollectibles`/`checkLorePickup`, `src/scenes/ZoneScene.ts:4675-4786`)

Entries of `LoreByZone[zone]` not yet in `loreCollected` are placed at their tile. Every tick: hero `distSq <= 4` →
add id to `loreCollected` (saved, carried across zones), emit `LORE_COLLECTED {entry}`, log `zone.lore.discovered`,
fade the prop (500 ms). Emerald_plains positions: (45,35), (105,15), (15,108), (85,80). Lore text/type data belongs to
the story/lore export.

### 12.2 Story decorations (`:5436-5590`)

Static props at their tile (grass, cleared by the generator); **not blocking**. Every tick find the nearest with hero
`distSq <= 9`: show its name label; if that nearest is at `distSq <= 2` show the description tooltip (and emit
`STORY_DECORATION_INTERACT {decoration}` when it opens); otherwise hide. No other effect.

### 12.3 Drops on the ground

Potions auto-collect at `distSq <= 4` (every tick); loot click rules §7.1 row 4; auto-loot every 300 ms (loot spec).

### 12.4 Rare ley-beast spots (later milestones; emerald_plains has none)

Per zone entry, per `petSpawns[i]`: one `Math.random()` draw; present iff `draw < chance` and the pet is not owned.
Pickup by proximity (`checkRarePetPickup`, `:4557-4582`).

### 12.5 Random-event props

§13.3.

---

## 13. Random events (CORE, `src/systems/RandomEventSystem.ts`; scene glue `src/scenes/ZoneScene.ts:3193-3718`)

### 13.1 Data — export `random_events.json`

Config (`:70-76`): `cooldownMs 30000`, `safeZoneRadius` = map value or 9, `minEventsPerWindow 3`,
`maxEventsPerWindow 8`, `frequencyWindowMs 300000`; `TRIGGER_MOVE_THRESHOLD = 3` tiles (`:187`).

| type | weight | zh name | message (i18n `sys.event.msg.<type>`) |
|---|---|---|---|
| ambush | 30 | 伏击 | 伏兵出现! |
| treasure_cache | 25 | 宝箱 | 发现了一个隐藏的宝箱! |
| wandering_merchant | 15 | 流浪商人 | 一位流浪商人出现在你面前。 |
| rescue | 20 | 救援 | 有人被怪物包围了! 快去营救! |
| environmental_puzzle | 10 | 谜题 | 你发现了一个古老的谜题装置。 |

emerald_plains zone data (`:110-121`): ambushMonsters `[slime_green, goblin]`, ambushCount `[3,5]`, merchantItems
`[iron_sword, leather_armor, hp_potion, mp_potion]`, one puzzle {prompt 石柱上的符文需要按正确顺序触摸。, solution
按下发光的符文, reward 获得了经验和金币!, rewardGold 50, rewardExp 30}, puzzle prop `decor_event_puzzle_rune_pillar`,
rescue NPC 迷路的旅人 / `npc_rescue_lost_traveler`, rescue reward {gold 30, exp 25}. Other zones `:122-169`.
Localised variants come from i18n keys `sys.event.rescue.<zone>`, `sys.event.puzzle.<zone>.{prompt,solution,reward}`.

### 13.2 Trigger (`update`, `:222-274`), called every tick unless the hero is dead

```
moved = (col,row) != (lastCol,lastRow)                          // exact float compare
if moved:
  explorationTime += delta
  movementAccum += max(|col − lastCol|, |row − lastRow|)        // lastCol = col on the very first call (no jump)
  last = (col,row)
if !moved || inCombat || (activeEvent && !activeEvent.resolved): return null
if isInSafeZone: return null           // tileType ∈ {5,6} OR Euclidean distance to any camp < safeZoneRadius (strict)
if now − lastEventTime < 30000: return null                     // lastEventTime = −∞ for a fresh zone system
if movementAccum < 3: return null
eventHistory = history within the last 300 s; if count >= 8: return null
chance = calculateTriggerChance(now)
if rand() > chance: return null                                  // NOTE: movementAccum is NOT reset on a failed roll
movementAccum = 0; type = weighted pick (rand × 100, subtract weights in table order, `<= 0`)
event = createEvent(type, now, col, row); activeEvent = event; lastEventTime = now; history.push(now)
```
`calculateTriggerChance` (`:333-362`): `base = 0.07`; if events-in-window `< 3` and
`p = min(explorationTime, 300000)/300000 > 0.3` → `base += 0.05 × p`; if events-in-window `>= 7` → `base × 0.3`;
`clamp(base, 0.02, 0.25)`.

Inputs from the scene (`checkRandomEvents`, `:3213-3242`): `tileType = tiles[tileRow][tileCol]` using the **float**
position (only defined on exact integers; otherwise undefined — harmless because the camp radius covers every camp tile);
`inCombat` = scene flag: true as soon as any living monster is in `attack` state or the hero has a living
`attackTarget`; false only **1 500 ms** after that stops (`updateCombatState`, `:3193-3211`).

**QUIRK W9** — since `movementAccum` is not reset on a failed roll, once the hero has moved 3 tiles after the cooldown
the roll repeats **every moving tick** (~7 % each at 60 Hz), so an event fires within ~0.25 s: in practice one event per
~30 s of out-of-camp, out-of-combat movement, capped at 8 per 5 min. **PARITY** at a fixed 60 Hz tick reproduces it;
FIX alternative: reset `movementAccum` after every roll (≈ the designed 3–8 per 5 min). §22 Q7.

Only one unresolved event at a time. Each zone entry creates a new system (cooldown and history reset).

### 13.3 Event creation (`createEvent`, `:380-436`) and scene handling (`:3244-3718`)

All positions use the hero position at trigger time `(col,row)` (floats). `findWalkableTile(pc, pr, max=5)`
(`:491-523`): clamp to `[1, size−2]`; return it if walkable; else rings `radius = 1..5` scanning `dr` outer, `dc` inner,
only ring-edge cells, within `[1, size−2]`; else null.

| type | creation (draws in order) | scene handling | resolves |
|---|---|---|---|
| ambush | `count = floor(rand×(max−min+1))+min` | per monster: `id = list[floor(rand×len)]`, difficulty-scaled; `angle = rand×2π`, `dist = 3 + rand×2`; preferred `clamp(round(col + cos×dist), 1, cols−2)`, same for row; `findWalkableTile`; spawn in `chase` state (details `monsters-ai.md` §6.4) | immediately |
| treasure_cache | `lootLevel = floor((lvMin+lvMax)/2)` (emerald 4), `qualityBoost = floor(lootLevel/10)` (0) | loot from a fake elite def `{level lootLevel, goldReward [10+5L, 20+10L]}` via `generateLoot(def, lck, boost)`; gold `randomInt(min,max)` added directly (emerald 30–60); opened chest prop at the event tile fades after 8 s (1.2 s fade); items dropped as loot at the event tile (the ±10/±5 px sprite jitter is visual only; the drop's tile is the event position) | immediately |
| wandering_merchant | items list, `priceMultiplier 1.2` (never applied by the shop — W10) | merchant prop on `findWalkableTile(round(col),round(row))`; opens the shop at once (`SHOP_OPEN {npcId 'wandering_merchant', shopItems, type 'merchant'}`); prop fades (500 ms) when that shop closes | immediately |
| rescue | `count = max(2, floor(rand×(max−min+1))+min−1)` (emerald 2–4) | rescue NPC prop on `findWalkableTile(round)`, "!" marker; monsters at `dist = 2 + rand×3` around the NPC, `chase`; a 500 ms loop completes the event when none of the tracked ids is still in the monster list → gold + exp reward (raw `exp +=`, no level-up check), NPC removed | when hostiles are gone (see W11) |
| environmental_puzzle | `puzzle = list[floor(rand×len)]` | puzzle prop on `findWalkableTile(round)`; clicking it opens a 2-button popup: solution → `gold += rewardGold; exp += rewardExp` (raw), resolve; "leave" closes and keeps the event **unresolved** (blocks all further events in this zone visit) | on solve |

**QUIRK W10** — the merchant's `priceMultiplier` is never used. **QUIRK W11** — dead monsters stay in the list until
their 15 s respawn replaces them with a new id, so a rescue completes ~15 s after the last kill, and event monsters
respawn forever (`monsters-ai.md` QUIRK Q4 — recommend `noRespawn`; then complete on death). Rewards granted via raw
`exp +=` skip the level-up path (FIX: use the normal `addExp`). Logs and events: `LOG_MESSAGE` with the event message,
`RANDOM_EVENT_TRIGGERED {event}`, `RANDOM_EVENT_RESOLVED {type}`.

---

## 14. Atmosphere: lighting, zone mood, weather, ambience, colour grade (RENDER-ONLY)

No day/night cycle exists; every zone has one static mood. Export `zone_moods.json` with the tables below.

### 14.1 Zone mood (`ZONE_MOODS`, `src/graphics/ZonePalette.ts:159-190`)

| theme | ambient (multiply) | ambientα | vignette | vignetteα | haze (add) | hazeα | saturation | contrast | lift RGB | gain RGB |
|---|---|---|---|---|---|---|---|---|---|---|
| plains | `fff0d2` | 0.22 | `2e2410` | 0.32 | `ffe6a0` | 0.05 | 1.06 | 1.04 | .012 .008 −.004 | .03 .02 −.01 |
| forest | `9aa6e0` | 0.34 | `120a2a` | 0.50 | `7a60c8` | 0.07 | 1.05 | 1.05 | −.006 .014 .03 | 0 .02 .035 |
| mountain | `d4def2` | 0.26 | `141c2c` | 0.40 | `c8dcf0` | 0.06 | 0.96 | 1.06 | −.008 .004 .022 | .02 .012 0 |
| desert | `ffe2b0` | 0.24 | `3a200c` | 0.34 | `ffd890` | 0.06 | 1.04 | 1.02 | .02 .01 −.006 | .03 .016 −.014 |
| abyss | `b07896` | 0.40 | `1a0010` | 0.58 | `ff3050` | 0.06 | 1.10 | 1.07 | .024 −.004 .012 | .03 0 0 |

Theme lookup by zone id: emerald_plains and ember_tower → plains, twilight_forest → forest, anvil_mountains → mountain,
scorching_desert → desert, abyss_rift → abyss (`src/systems/LightingSystem.ts:9-17`).

Web composition (`LightingSystem`, `:39-239`):
* Full-screen ambient rectangle (multiply) with the mood colour at `ambientα`, breathing `α + sin(t×0.0015)×0.015`.
* Haze: radial texture (additive), 1.25× screen, tinted, `α = hazeα × (0.8 + sin(t×0.0007)×0.2)`, drifting by
  `(sin(t×0.0008)×0.15 W, cos(t×0.00056)×0.1 H)`.
* Vignette (multiply) radial gradient from 20 % to 62 % of the width: 0 → `0.35α` at 55 % → `α` at the edge.
* Point lights (additive radial sprites, falloff stops 1, .9@.15, .6@.4, .25@.7, 0@1): `alpha = intensity ×
  lightScale`, `lightScale = max(0.12, ambientα × (1 − lum(ambient)) × 1.4)` (plains/mountain/desert 0.12, forest 0.1605,
  abyss 0.2522); flicker `+ sin(t×0.007+s)×0.05 + sin(t×0.013+2.3s)×0.03`, `s` random in [0,1000) per light; nearest
  `maxDynamicLights` (8/16/32 by quality) on screen; refreshed every 100/50/16 ms by quality.
* `deepen(a)` (labyrinth gloom curse): `ambientα = min(0.92, α + (1−α)×a)`.
* Camera post FX (WebGL): bloom (`0xffffff`, offsets 0.6/0.6, blur 0.5, strength 0.8; balanced/high quality only),
  vignette (0.5, 0.5, radius 0.95, strength 0.12).
* Colour grade (`src/graphics/ColorGradePipeline.ts:4-29`), applied last:
  ```
  lum = dot(c, (0.299, 0.587, 0.114)); c = mix(lum, c, sat); c = (c − 0.5)×contrast + 0.5
  sh = 1 − smoothstep(0, 0.55, lum); hi = smoothstep(0.45, 1, lum); c += lift×sh + gain×hi; clamp 0..1
  ```

Lights registered per zone (`registerLightSources`, `src/scenes/ZoneScene.ts:1966-2006`): hero halo (radius 80 px,
`0xffeedd`, 0.4), every campfire (120 px, `0xff8800`, 0.85, flicker, 8 px above ground), every torch (70 px, `0xff6600`,
0.65, flicker, 40 px up). Camp flame colour per theme: plains `0xff8800` (`src/data/camp-themes.ts`).

3D equivalent for plains ("warm afternoon sun"):
* Directional sun, colour ≈ `#FFF0D2`, from the **screen upper-left** (art direction: `docs/art-direction.md:12`): light
  travelling toward +X (screen lower-right), i.e. rotation yaw 0°, pitch ≈ −45°; soft shadows tinted cool.
* Sky/ambient: cool fill so shadows lean blue/purple; exponential height fog tinted with the haze colour at low density.
* Post-process: a blendable material implementing the exact grade formula above (parity), mood vignette, mild bloom.
* Point lights: campfire radius ≈ 300 cm (120 px × 2.21 ≈ 265, rounded up for 3D falloff), torch ≈ 180 cm, hero halo
  ≈ 200 cm very low intensity; flicker with the same two-sine formula on intensity.
* Zone palette rim/outline colours (`ZONE_PALETTES`, `src/graphics/ZonePalette.ts:105-120`, HSL dominant 120°, accents
  90/150, s 0.55, l 0.40 for plains) → per-zone rim-light tint for characters. Computed plains values: entity outline
  `rgba(75,170,75,.22)`, entity rim `rgba(119,187,119,.11)`, NPC outline `rgba(143,174,112,.16)`, NPC rim
  `rgba(163,183,143,.10)`, player outline `rgba(205,213,205,.20)`, player rim `rgba(196,202,196,.10)`.

### 14.2 Weather and ambience (`src/systems/WeatherSystem.ts:15-30`)

| zone | weather | ambience (tint) |
|---|---|---|
| emerald_plains | none | **pollen** (`0xffe8a0`) |
| twilight_forest | none | wisps |
| anvil_mountains | snow | dust motes (`0xdce8f4`) |
| scorching_desert | sand streaks | dust motes (`0xf0d8a8`) |
| abyss_rift | ember storm | sparks |

Unknown zone ids (sub-dungeons, labyrinth floors) use their theme's zone entry. Plains emitters (screen-space, depth
above the world):
* Pollen = dust motes "bright": spawn over a 3×3-screen area, lifespan 8–14 s, speed 1–5 px/s any direction, scale
  0.5→0.8, alpha 0.35→0, tint `0xffe8a0`, one every 420 ms × quality multiplier (1 / 1.5 / 2), additive, parallax 0.3.
* Ambient dust (`createAmbientDust`, `src/scenes/ZoneScene.ts:1922-1964`): tint per zone (emerald `0x88cc88`), lifespan
  6–12 s, speed 2–8 px/s, angle 200–340° (drifting up), scale 0.8→1.5, alpha 0.15→0, every 800 ms, additive,
  parallax 0.3.
* Camp: campfire sparks (every 180 ms, speed 15–45, upward cone 245–295°, life 0.4–0.9 s, tints `ffdd44`/`ff8800`),
  flame flipbooks with scale flicker, pulsing glow discs.
3D: Niagara systems in a camera-attached volume (~20 m around the focus) with matching rates/lifetimes; camp effects as
world-space Niagara at the props. Other weathers are later milestones (same table).

---

## 15. Terrain, walls, camps and decorations in 3D (RENDER-ONLY; inputs are the baked map JSON)

### 15.1 Ground

* One flat ground mesh per zone generated from `tiles` (Blender script or UE procedural/static mesh): 120×120 tiles of
  1 m, with a **skirt** of non-walkable scenery 15–20 m beyond the border ring so the camera never shows the void.
* Material layers by tile type with the web's palette and **rank-based transitions** (higher rank laps over lower at
  tile boundaries; `src/graphics/terrain/TerrainStyles.ts:66-112`, plains ranks grass 4 > stone 2 = camp 2 > dirt 1 >
  water 0; grass has a lighter "lip" `0x9cc65c` on the higher side). Plains materials: grass base `0x74a247` (patches
  `6c9a43/7eab4e`, tufts `5a883b/a9cc68`, details flowers/tufts/stones, accents `f6d65a fbf3dc e8829a f0a848`); dirt
  `0xc09a60` (cracks, pebbles, straw); camp ground `0xb89468` (leaves, pebbles, straw); water `0x3f93b8` with shallows
  `0x6cc3cf`, foam `0xeef9f2`, bank `0x8c7448`, lily pads. Walls stand on the zone's dominant ground (grass); camp walls
  on camp ground (`src/graphics/terrain/ZoneTerrain.ts:84-102`).
* Implementation suggestion: per-vertex (or a 120×120 control texture) material weights blended with a noise mask of
  period 3 tiles — matching the web's `MASK_PERIOD = 3` look — instead of per-tile textures. Ground variant details are
  chosen per tile by `tileHash(col,row,7) % 12` (`< 6` plain, else detail variant `1 + h%3`) if exact detail placement
  is wanted (`src/graphics/terrain/TerrainLattice.ts:67-71`).
* Water: depressed basin (bank slope over the outer 0.3 tile), translucent surface at Z −12 cm with foam at the shore.

### 15.2 Wall tiles (type 4): outcrops

One prop per wall tile (including the border ring). Variant `v = tileHash(col, row, 3) % 6`
(`tileHash` = `imul`-based hash, `src/graphics/terrain/TerrainLattice.ts:67-71`). Plains outcrop kind `mossBoulder`:
`v % 3 == 2` → leafy **hedge** (colours `0x5f9a3e`), else **moss-capped boulder** (rock `0x9c9888`, moss cap `0x7caa48`,
flower accent `0xf6d65a`, contact shadow `0x2f4a26`). Footprint 1 tile + 8 px overhang each side, up to ~76 px tall →
3D: ~1.2 × 1.2 m footprint, 1.4–1.9 m tall, random yaw from the same hash; adjacent border walls should read as a
continuous ridge (e.g. merge instances into ridge meshes for the ring). Other themes: forestRock, crag, mesa, basalt.
At exit tiles replace the outcrop by the exit gate/portal mesh.

### 15.3 Camp walls (type 6): palisade

Segment mesh chosen by the 4-bit neighbour mask `(+col ? 1) | (−col ? 2) | (+row ? 4) | (−row ? 8)` of camp-wall
neighbours (`ZoneTerrain.showOverlay`, `:493-503`): sharpened wooden stakes (plains `stakes`, wood `0x9a6c3e`, band
`0xc9a868`, accent `0x3f8a36`), ~1.5 m. Gate posts at the 2-tile gate gap (−row side); banners at (c−1,r−4),(c+2,r−4) (camp props, §3.4).

### 15.4 Camp props

Meshes at the §3.4 offsets: campfire (flat, logs + flame flipbook, 0.6 m), well (2 m), tents (2.3 m wide, 2.1 m tall,
tint `#5a4028` plains), banners (2.6 m, green `#2a6a1a`), barrels (1 m), crates (0.85 m), torches (1.9 m, flame at the
top). Tents and other tall camp props fade when they occlude the hero (§15.6).

### 15.5 Decorations

One instance per `decorations[i]` (HISM per type). Deterministic placement jitter from the web (`placeDecorSprite`,
`src/scenes/ZoneScene.ts:1814-1839`) with `seed = i + 1` (generation index):
```
h  = uint32(seed × 2654435761) / 2^32          // exact: seed ≤ 1269 → product < 2^53
h2 = uint32(seed × 1597334677 + 12345) / 2^32
jx = (h − 0.5) × 18 px; jy = (h2 − 0.5) × 8 px; scale = 0.9 + ((h + h2) mod 1) × 0.2
```
3D: offset in tiles = `isoToCart(jx, jy)` = `((jx/32 + jy/16)/2, (jy/16 − jx/32)/2)` (max ≈ ±0.27 tile), uniform scale
`scale`, yaw = `h × 360°` (new; the web has no rotation). Plains decor meshes (sizes from the sprite drawers, converted
with §1.3):

| type | web flags | sprite w×h (ground line) px | 3D target |
|---|---|---|---|
| tree (broad oak) | tall | 128×170 (164) | ~4.2 m tall, canopy ~2.8 m |
| tree_round (fruit) | tall | 100×136 (130) | ~3.3 m, canopy ~2.2 m, red fruit accent |
| boulder (mossy) | tall | 76×62 (56) | ~1.4 m |
| bush (berry) | – | 56×44 (39) | ~1.0 m tall, 1.2 m wide |
| rock (cluster) | – | 44×32 (27) | ~0.7 m |
| mushroom_red | – | 36×30 (26) | ~0.6 m |
| grass (tuft) | flat | 44×28 (24) | ground cover, ~0.4 m |
| flower (patch) | flat | 40×28 (24) | ground cover |

Story-decoration meshes (`ruins`, `war_banner`, `broken_altar` for chapter 1), lore props, treasure chest (closed/open
states), gold pile, puzzle rune pillar, wandering-merchant and rescue-NPC models, exit portal gate: see the art pipeline.
Decorations, story props and camp props other than barrels/crates have **no gameplay collision** (§22 Q6).

### 15.6 Occlusion fading and culling

Web: tall decorations, tents and wall/palisade overlays fade when the hero or a living monster within ~6 tiles stands
behind them: decor/tents to α 0.25, walls to α 0.45, approach `α += (target − α) × min(1, dt/110 ms)`, snap within 0.02
(`src/scenes/ZoneScene.ts:1841-1885`, `src/graphics/terrain/ZoneTerrain.ts:522-536`). 3D: dithered opacity-mask fade
driven by a camera→hero (and nearby monsters) sphere/capsule overlap, same targets and time constant. Viewport culling
of tiles/decor (every 100 ms with 4–7 tile margins) is replaced by UE's own culling/HLOD.

---

## 16. Later milestones (keep systems general)

* Other zones: baked JSON from the same generator (hashes §4.9), themes per §4.2/§14, weather per §14.2, outcrop kinds
  per theme. Their `fieldNpcs`, `hiddenAreas`, `subDungeonEntrances`, `petSpawns`, `storyDecorations` follow §2.2.
* Sub-dungeons: entered from `subDungeonEntrances` (hero `distSq <= 9`, §7.1 row 7); map built by
  `generateSubDungeonMap` (`src/scenes/ZoneScene.ts:5329-5434`: border walls, hash-based stone/wall sprinkle
  `((c×374761393 + r×668265263 + seed) >>> 0) % 100`), exit returns to the parent entrance; death returns to the
  parent's camps[0].
* Abyss Labyrinth floors: `DungeonSystem.generateFloorMap` → `MapGenerator.generate` with run seeds; floor exits sealed
  until the keeper dies; save position in a run = abyss_rift (15,22).
* Ember Tower (`src/data/maps/ember_tower.ts`): hand-authored 48×48 builder using its own LCG
  `seed = (seed×1103515245 + 12345) & 0x7fffffff` in JS doubles (the product exceeds 2^53 — emulate double rounding or,
  simpler, ship the baked JSON; hashes in §4.9). Portal at (24,44) with the armed-on-leave rule; hearthstones beside each
  camp at the first walkable of offsets `(1,4),(−1,4),(0,5),(2,5),(−2,5),(4,3),(−4,3)` (emerald camps → (16,19) and
  (96,104)), clickable within 1.6 tiles when the hero is within 3 (`src/systems/EmberTower.ts:86,375-385,459-475`).

---

## 17. Tick order (world-relevant part of `ZoneScene.update`, `src/scenes/ZoneScene.ts:1346-1582`)

1. Cinematic / labyrinth modal → clear hold-move, skip the tick.
2. Labyrinth curse, Ember Tower tick.
3. Camp regen modifiers (§3.4).
4. **Keyboard/stick/joystick movement** (§6.3) → **hold-to-move** (§7.2) → skill/gamepad input → pointer-free
   actions of the `InputFrame` (§7.4: `townPortal`, `interact`, `toggleAutoCombat`, `cycleAutoLoot`) →
   `InteractPrompt` refresh.
5. Equipment stats → `recalcDerived` (moveSpeed may change every tick) → passives.
6. **Hero update** (path following §6.2, regen).
7. Mini-boss dialogue; monster AI (active set rebuilt every 250 ms); NPCs.
8. Combat, mercenary, pet, escort, defend, elite, status effects, **combat state** (1.5 s debounce),
   **random events** (§13), target indicator, mobile controls, auto-combat.
9. Potion auto-collect, auto-loot (300 ms).
10. **Exit proximity** (§9.2), rare pets, lore pickup, **exploration grid** (§10.1), **hidden-area discovery**,
    story-decoration proximity, sub-dungeon proximity.
11. Occlusion, QuestWorld, soul echo, StoryDirector, terrain flush.
12. Every 100 ms: tile/decor visibility (render). Every 500 ms: explore-objective checks (`dist <= location.radius`,
    `:3762-3787`) and NPC quest markers.
13. Lighting update, HP/MP change events.

Recommended core order is the same; the UE module reads the post-tick snapshot.

---

## 18. Core API and events (proposal)

```cpp
namespace abyss::world {
enum class Tile : uint8_t { Grass, Dirt, Stone, Water, Wall, Camp, CampWall };
constexpr bool Walkable(Tile t) { return t != Tile::Water && t != Tile::Wall && t != Tile::CampWall; }

struct ZoneMap {                       // loaded from maps/<id>.map.json, immutable except collisions
  std::string id; int cols, rows; Theme theme; int seed; int levelMin, levelMax; double safeRadius = 9;
  std::vector<Tile> tiles; std::vector<uint8_t> walk;        // row-major
  Anchors anchors;                                            // spawns, camps, exits, start, fieldNpcs, hidden, story...
  std::vector<Decoration> decor;                              // render data, passed through to UE
  bool IsWalkable(int c, int r) const;
  void ApplyCampBlockers();                                   // §3.4
};
struct GenerateInput { const MapDef& def; std::span<const AvoidPoint> avoid; };
ZoneMap Generate(const GenerateInput&);                        // §4 (bit-exact)

class Pathfinder {                                             // §5
 public: explicit Pathfinder(const ZoneMap&);
  bool FindPath(double sc, double sr, double ec, double er, std::vector<TileCoord>& out) const;  // false = empty
};
struct HeroLocomotion { /* §6 */ void SetPath(...); void TickPath(double dtMs); bool TryDirectMove(Vec2 dir, double dtMs); };
struct PointerController { /* §7: OnPress(tile, button, pointerId), OnHoldTick(tile, down) */ };
enum class InteractKind : uint8_t { Loot, Npc, TowerObject, SubDungeon, LabyrinthPortal, HiddenReward, EventPuzzle };
struct InteractTarget { InteractKind kind; uint32_t id; double col, row; };
// ZoneRuntime = the per-zone live state (drops, NPCs, event props, hidden rewards) built on zone entry (§9.1)
std::optional<InteractTarget> FindInteractTarget(const ZoneRuntime&, double heroCol, double heroRow);   // §7.4
bool Interact(ZoneRuntime&, const InteractTarget&);           // same handlers as the §7.1 rows
enum class PortalRefusal : uint8_t { None, Dead, Busy, NoDestination, AlreadyAtCamp, AlreadyAtExit };
PortalRefusal CanUseTownPortal(const ZoneRuntime&);           // §9.4 checks without side effects (touch button dim)
class ExplorationGrid { void Reveal(double c, double r, double radius = 10); bool IsExplored(int c, int r) const; };
class FogOfWarCore { /* §10.2, optional */ };
class RandomEventDirector { /* §13 */ };
}
```
Events to UE (pull from a per-tick queue): `ZoneEntered{mapId}`, `ZoneTransitionBegin{target, col, row}` (UE fades
400 ms then loads), `ZoneExit{mapId}` (emit it — the web never does, QUIRK W12), `HeroPathSet{nodes}`,
`HeroTeleported{col,row,reason}` (portal, respawn, load), `TownPortalChannel{start|complete}`,
`InteractPromptChanged{kind,id,col,row | none}` (§7.4),
`HiddenAreaDiscovered{id}`, `HiddenRewardSpawned/Collected`, `LoreCollected{id}`, `StoryDecorFocus{id|none, tooltip}`,
`RandomEventTriggered{type, col, row, context}`, `RandomEventResolved{type}`, `CombatStateChanged{inCombat}`,
`TargetChanged{id|none}`, `MinimapSnapshot` (on request).

---

## 19. Data to export to JSON (core loads at boot)

| File | Content | Source |
|---|---|---|
| `maps/<id>.map.json` | all `MapData` fields (§2.2) + baked `tiles` (array of row strings of digits `0`–`6`) + `decorations` as `[col,row,type]` in generation order + `golden {tilesSha, decorSha, walkable}`; collisions are derived, not stored | `AllMaps` after module load (Appendix B) |
| `map_gen_themes.json` | `THEME_CONFIGS`, `DECOR_POOLS` | `src/systems/MapGenerator.ts:49-95,274-300` |
| `zone_landmarks.json` (or computed in core) | §3.2 rules + per-zone result | `src/data/maps/index.ts:13-33` |
| `camp_layout.json` | camp stamp numbers, prop offsets, blocker types, NPC slot offsets, hearth offsets | §3.4, §16 |
| `zone_moods.json` | `ZONE_MOODS`, `ZONE_PALETTES` (+ computed rgba), lighting theme map, light presets, `ZONE_WEATHER`/`THEME_WEATHER`, ambient-dust tints, `CAMP_THEMES`, render-quality presets | §14 |
| `terrain_styles.json` (art) | `TERRAIN_THEMES` ground/outcrop/palisade colours and ranks | `src/graphics/terrain/TerrainStyles.ts` |
| `random_events.json` | config, `RANDOM_EVENT_DEFS`, `ZONE_EVENT_DATA` | §13.1 |
| `minimap.json` | tile colours, marker colours/sizes | §11 |
| `world_constants.json` | hold-move 120 ms, exit radius² 2.25, campfire regen radius 5 / ×50, safe radius 9, explore radius 10, camera k 5.0 / FOV / pitch / yaw, hero speed model, town portal 1500 ms, death respawn 1100 ms, zone fade 400 ms, interact ranges (loot/hidden/puzzle `distSq <= 4`, NPC `dist <= 3`, entrances/portal `distSq <= 9`), default pad bindings of §7.4 | this spec |

---

## 20. Quirks summary

| id | Quirk | Recommendation |
|---|---|---|
| W1 | Cursor→tile uses `floor` → half-tile pick bias | FIX: `round` (§1.4) |
| W2 | Walk animation continues ~0.4 s after arrival (decel of a speed that no longer moves the hero) | FIX (render): drive anim from velocity |
| W3 | Keyboard/stick movement has no wall sliding; diagonal into a wall stops | FIX: axis-separated retry (core) |
| W4 | Path speed is constant in screen px (2.65–5.30 tiles/s); keyboard is 1.8 tiles/s | FIX: uniform `moveSpeed/35.777` tiles/s; parity mode kept behind a flag |
| W5 | Clicking a far NPC only walks to it | FIX (optional): pending interaction on arrival |
| W6 | Clicking an exit changes zone instantly from anywhere on screen | FIX: walk to it, trigger by proximity |
| W7 | Backward exits re-trigger on arrival (bounce) | FIX: exit arming (`distSq > 6` once) |
| W8 | Hidden-area rewards lost if not collected in the discovery visit | FIX: persist collected rewards, respawn the rest |
| W9 | Random-event roll repeats every moving tick after 3 tiles | PARITY at 60 Hz (or FIX: reset per roll) — §22 Q7 |
| W10 | Wandering merchant `priceMultiplier` unused | PARITY (or apply 1.2) |
| W11 | Rescue completes only after the 15 s respawn swap; event monsters respawn forever; raw `exp +=` rewards | FIX with `noRespawn` + `addExp` |
| W12 | `ZONE_EXIT` never emitted (listeners leak) | FIX: emit on leave |
| W13 | Fog-of-war renderer unused; `exploration` save field always empty | PARITY: no fog in milestone 1; drop the dead save field |
| W14 | `tileType` for event safe-zone check indexes tiles with float coordinates | PARITY (no effect: camp radius covers camp tiles) |
| W15 | Town portal channel cannot be interrupted, always targets `camps[0]` | PARITY (Q4) |
| W16 | Town portal has no touch or gamepad trigger; the pad also lacks interact, auto-combat, auto-loot and story-advance, so Ch1 cannot be finished gamepad-only | FIX: core `townPortal`/`interact`/toggle actions, touch portal button, pad D-pad Down / RT / D-pad Up / D-pad Right, A/Menu in story beats (§7.4) |

---

## 21. Test plan (C++ core, GoogleTest/Catch2 via CMake)

1. Generator: vectors and goldens of §4.9 (all six maps), RNG checkpoints for emerald_plains, invariants (border, start
   walkable, exits reachable, camps/spawns connected, water blocks).
2. Camp stamp: camp 1 tile layout (gate at (14,10),(15,10); walls rows 10–18 at cols 10/20; rows 19–20 open); blockers
   make exactly (13,15),(17,15),(12,12),(18,12) and (93,100),(97,100),(92,97),(98,97) unwalkable → 13 159 walkable.
3. A*: the web unit cases (`src/__tests__/PathfindingAndMaps.test.ts:76-343`: straight, diagonal, L-wall, adjacency,
   optimal length, start==end, blocked/out-of-bounds/negative endpoints, enclosed, corner-cutting, fractional rounding,
   corridor, blocked start, determinism) + the six golden routes of §3.5 node-for-node.
4. Locomotion (IsoPixelParity): from rest at (15,22) with path [(16,22)] and moveSpeed 120 at 60 Hz the hero arrives on
   the tick where cumulative `Σ currentSpeed×dt >= 35.777` px; UniformTiles: same with `3.354` tiles/s; keyboard 1.8
   tiles/s; blocked diagonal does not move (parity) / slides (fix).
5. Pointer chain: one test per row of §7.1 (priority, radii, `<` vs `<=`); hold-to-move repath cadence (no repath
   within 120 ms on the same tile unless the path is empty; immediate on tile change; stop within 0.6).
6. Exits: trigger at `distSq 2.24`, not at 2.25; arming fix (W7) round trip forest↔plains does not bounce.
7. Town portal: refused inside 9 tiles of camp 1; teleports after exactly 1 500 ms; respawn after 1 100 ms at (15,15).
   The `InputFrame.townPortal` flag gives the same result as `R`/RMB. `CanUseTownPortal` returns `AlreadyAtCamp` at
   `distSq = 80.99` and `None` at 81.
8. Interact (§7.4):
   * an NPC at Euclidean distance 3.0 is in range, and one at 3.01 is not;
   * loot at `distSq` 4.0 is in range, and loot at 4.01 is not;
   * a loot drop and an NPC at equal `distSq` → loot (order 1);
   * nothing in range → no-op with no log;
   * dead, cinematic or labyrinth modal → ignored;
   * the `InteractPrompt` matches what `interact` would do on the same tick.
9. Exploration/hidden area: standing at (108,108) discovers `hidden_ep_elven_cache` in one tick; a hero who only reaches
   (104,102) does not (corners (102,114) and (114,114) stay unexplored); leaving and re-entering resets the grid;
   rewards: rare equipment at level 7, +200 gold.
10. Random events with a scripted PRNG: cooldown 30 s, threshold 3 tiles, cap 8 per 300 s, chance formula at the
    documented breakpoints, weighted pick boundaries (`rand×100` = 30 → ambush, 30.0001 → treasure), safe-zone strict `<9`.
11. FogOfWarCore (if ported): the web suite `src/__tests__/FogOfWarOptimization.test.ts`.

---

## 22. Open questions

1. **Hero speed model** — adopt UniformTiles `moveSpeed/35.777` tiles/s for clicks *and* keyboard (W4)? Should direct
   input keep the instant start or use the same acceleration?
2. **Camera** — parity framing pitch −30° / FOV 30° / 29.3 m, or a steeper Diablo-like −40°…−45° (re-check aggro/attack
   readability)? Allow player zoom (wheel/pinch) later?
3. **Fog of war** — keep "no fog" (web parity) for milestone 1, or enable `FogOfWarCore` for the world or the minimap?
4. **Town portal** — keep the uninterruptible 1.5 s channel to `camps[0]`, or interrupt on damage/move and target the
   nearest camp?
5. **Minimap orientation** — keep north-up tile space (parity) or rotate 45° to match the camera?
6. **Decoration collision** — trees/boulders/tents are walk-through in the web; in 3D walking through a 4 m oak trunk may
   look wrong. Keep walk-through (no grid change), or block tall-decor tiles (changes pathing and requires regenerated
   goldens/tests)?
7. **Random-event frequency** — keep the per-tick roll (≈ one event per 30 s of exploring, W9) or reset per roll?
8. **Milestone-1 exit** — sealed gate with a "coming soon" message, or hide the exit entirely until chapter 2?
9. **Click on far NPC / exit** — adopt the D2-style "walk then act" fixes (W5, W6)? Recommended at least for W5,
   because touch interaction (§7.4) relies on it for one-tap talk.
10. **Gamepad-complete Chapter 1** — §7.4 decides the world-side pad bindings: D-pad Down portal, RT interact, D-pad Up
    auto-combat, D-pad Right auto-loot, A/Menu in story beats. Confirm that milestone 1 also ships CommonUI focus
    navigation for the Ch1 panels (`save-ui-input.md` OQ-INPUT-3). Without it, the Controls page must state that NPC
    panels need a mouse or touch. Recommendation: ship it.

---

## Appendix A — emerald_plains generated tiles (120×120)

Legend: `.` grass 0 · `:` dirt 1 · `~` water 3 · `#` wall 4 · `c` camp ground 5 · `P` palisade 6. Row number on the
left, column ruler on top (tens). Camp 1 is top-left at (15,15), camp 2 bottom-right at (95,100), the exit is on the
right border at row 60. Authoritative data is the JSON export; this is a visual cross-check.

```
     0         10        20        30        40        50        60        70        80        90        100       110       
  0  ########################################################################################################################
  1  #...:::...........:.................:....................:................:.........................::....:.:..........#
  2  #...#:......:......................#:....:...............#...:............:......................:.....:....:#...#.:...#
  3  #................#..........:.:#..:............::......:..........:..........:....:........:...#................#.:....#
  4  #.....:....:.........................:...........:......:..........#..#.........:............:.:..#........:...:.#.::..#
  5  #:........#..................:................#...#......:..................#..........................:...........#...#
  6  #..............:................:........:.............#.......:........:...:...:.................#.............:...:..#
  7  #..........:....:..:..........:...#:.#..............:.:...................:.....................:..........#..:........#
  8  #...:....:..:....:...................:..........................:...#.......:..........#..#.......#:.........:.........#
  9  #....#...........................#....:...:#....:.:...##..................:........:.....................#............:#
 10  #:...:....PPPPccPPPPP.....:.:.........:.:.:....................:.................:........................#........:...#
 11  #.........PcccccccccP.....:..............#.............:...:.........::.....:.........:................:.............:.#
 12  #..:....#.PcccccccccP..:..::...............:..:.::.:.....#....................................:..........:.............#
 13  #.........PcccccccccP....:........:...........:................:...........#.....:#....:.........:........#......::....#
 14  #:...:....PcccccccccP..#.............:.......#....##..............#.............:............#...#.::..........:....:..#
 15  #...::....PcccccccccP......#......:..:...:::.:........:...::.#..................:.:.............:......................#
 16  #......:..PcccccccccP..:.::::::::::::::::::::::::.::.....:::::::::::::.........:........:..#....::.........#...........#
 17  #..#:.....PcccccccccP::::::.........:..::.:....:::::::::::.:......::::::::...##....:...............:..::...............#
 18  #..:......PcccccccccP..:......:......:............:::.:..:.:.....:...:.:::::::::::::::................:.........:......#
 19  #......:..ccccccccccc....:..................::::::::::......::::::...::..::......:...:........:...........:......:...:.#
 20  #.........ccccccccccc..:::...:....::..:..:::::......:::::::::::::::::::::::::::::...................:..................#
 21  #.:............:::::::::::.....:..::::::::..#:...:....:.#.::........#:......:..::......:...#...#...................:...#
 22  #....#..:#...:::::::::::::::::::::::...:........:.:....................:........:::.....................#.........:..:.#
 23  #...........::::::::..:.:.::....................#...#.....:..#.......:.::.:......:::......:...................#.....:..#
 24  #.:.........::..:::::.:.............................:...........:.......#.......:.:::.....:....:.......................#
 25  #..........::....#::::...................:...::..........:........:...:......#.....:::....:............#......#.......:#
 26  #.:.:...:.::......::::...............:.........:.......#...:...#.........:..........::.............:...:........#......#
 27  #........::::#...:#:::....:..:.........::....##................:.....:...............::.........#...::.......:...:.#...#
 28  #......:::.........:::::.........:....:.................:.....#......:................::::........#....:..#.:..........#
 29  #.:...:::............:::::.:....................:......#......:...........:..:........:.::........::..:.:.:............#
 30  #...:.::......::.....:.::::........#....:.....:..........................................:.::.............:............#
 31  #...:.::.............:::::::....:............:..:........................#...............:::...................:.......#
 32  #.:....:.......#..:.::..::::.:..............:.....#....:....:..:.....#.........:.#:........::............#..:..........#
 33  #.....::::........:.::..:::::.....:.............::.:.................................:.....::.....#.......:............#
 34  #.......:::..........:....::::::....#.::........:.......#........:...#......................::....:.................:..#
 35  #......#::.........#::...:.:::::................#.#..........:......:................:..:...::::.....................:.#
 36  #......:.::.:........:.......:::::................#...:....:......:.:....:.....:...........:.:::.:..:.#....:...........#
 37  #........:.:...#....::::.....::::..............:............:............#.:................:..:::..:..............#...#
 38  #........:...:......::......#..::::...........#:....:........:.....#.............#:.:...::.....::::..:..:........:.....#
 39  #.......::..........:..:.......:::::........#............#....#......:......::........:.:.:#....:::::..........#:......#
 40  #......:::........#.:..........:::::..#....#.............#...:....#..#...:....:.................:..::.....#.....:......#
 41  #...:...:::.......#.:....#.....::.:::::..........#..:##............#...:#...........:.......#.:.:#.::.......:..........#
 42  #....:..:.....:.#...::.....:...::....::......#..#......#..:.........#............#..........:.:::...:::..:.......:.....#
 43  #:#...:.:...........:......:...:.....::.......#.............:...........................:..:..::.:...::.....:..........#
 44  #.......:.:........::...:.:....:...:..::...........::.....#..........................:........#:..:..:::...............#
 45  #.......::.....:....:..........::......::...........................:..........:...............:......:::....:...:.:...#
 46  #........:.........:...........:.#....:::::..:............:......#.........:.................:::.:......::.............#
 47  #.......::....#:.:.........#...::........:::...............:.............:................:...::........:...:......#...#
 48  #.......::......:.............::......:...:::....:..::...........:...................:.......:::....#...::::..........:#
 49  #........:....:...:...........::........:..::.......:.....:......:....:........#....:.....::..:.........:::::.:#...:...#
 50  #.......::.......:........:....:............::.................:...:....:.::..............:..::...........:.::.:...:#..#
 51  #.......::.:.....#...:.........:....:.#.....:::...:........................:..................:..#.:.....:...::........#
 52  #.#.....::..........:....::....:....:.......:.::.:...::...............:....::...............#.:..:.........#..::....:.:#
 53  #...:....:::......:.....#.....::....:.........:::.....:..........::#.......:.................::::.......#......::::....#
 54  #........:..................:..:...........:....::.:...:...........:.:..#..::..:.:...........:.:#:......#......:::.....#
 55  #.......::....:..........#.:..::.......:....::...::........................:.................:.:.:...........:...::.:..#
 56  #.....:..::..............:....::..:...............::................:.....::.........:....#....:#.....:#........:.::...#
 57  #......:::......:.............:...................:::................::..:.::...:#.......:....:::...........#.....:::..#
 58  #...#....::..:................::..................:.:::..........::.#......:...:.....::........::.......:..:...#:..:::.#
 59  #..#:...::...............#...::..................:..::.:......:..:.:..#....:::..:..::#.........:.::....:............:..#
 60  #........::....:...........::.:..:#......#...........::::.:.......#......::.::...#.......#.....:...............#....:..#
 61  #........::................:..:...:........:#.......#:.::......#.......:....:..:.........:..#..:.:.........:...::......#
 62  #...:...:::....:.....#..::...::.........:...........:...::..#...............:........::.......::..........:#:...:.::::.#
 63  #.....#:..:.:.:...:....:....:.::..:.#......:..#..........::......:.:......:::.#..#.....#.......:.:..:.....:......::::..#
 64  #.........:::.......:........::................:....#....:::...............:::.:..........:...::.............:...::....#
 65  #...:....::::.......#:#.......:...........:....:...........::.:....#.......:.....#.........:..::........#......:.::....#
 66  #:...::..:..:..#..............::...................:...:..::::....:.....#..:.................:::................:::::..#
 67  #...........:..............:#:::.#................#..........:::...........::......##........:#..#.............:::.....#
 68  #:.:#:...#.::..#.......:..:....::...........................:..:..#..::::..::...#.......:....:.#....#.....:...:::...:..#
 69  #..........::.....:..:.....:.:..:.........#.....::......:....#::::...:.....::..:.............:......:........::::....:.#
 70  #..........::...:....:.......#..:.......................:......:::::...:..:.::.......:...:..:::..........:.:::#::......#
 71  #.........::........:...:......::............:...........:..::...:::#.:....::#.............:.:...........:::::.......#.#
 72  #..........:.#:#.:.............:.#..........................:...:.::.:..:...:.:.........:...::...........:::...........#
 73  #......:...:.:........#:.:.....:.......:#......:...:...............::.:.....:...............::::.........::....:..:.:..#
 74  #.:........::..................::..............:..........:........:::...:..:....:.:.........::....:....::.............#
 75  #..:...:...:::...:..:.....#.:..:.......:............#...#...........:::..#.:::..::...........:.........:::...:.:#......#
 76  #.......:..::::..:...:.....::..:....:.................#..............:::..#.:...:............:.:......::..........#....#
 77  #.......:..:.:...............#::...:...:.....................:......#:.:::..:...:............::......::....::.........:#
 78  #............:.................:.....:.......:..::.....:................::..:...::::........::....:.:::........:.......#
 79  #............:...#......#.....::........:....:...:...:...........:.#.....::.:.:.:............:.....::..................#
 80  #...:.......::......:#.:...#..:............#.....::.............:........::::...::::.........:....:::..:...............#
 81  #...#........:.....:.........................:...................:.........:::.::.........:..:...::................:.#.#
 82  #...........:::.............................:.#.................:.:........::::.::........#:.:...:..:............:.#.:.#
 83  #:...........:..........:.........:.......................:..............:.::::.:......:.....::..::.............:......#
 84  #....:.....#.::..#.....:.......:.:.........#:...........:..:::............:...:::............::.::.:.......#...........#
 85  #............:::..:......#....................:............................:.:.::.....#.#....:..::.#.........#.........#
 86  #.....:..:...:.:::..:.......:........:....:.........:....:..:.........#......:.::::.........::..::....:....:.......:...#
 87  #..#.........:.::....#.........#..::....:.......:..#..:.........................:::::.:......:..:................:.....#
 88  #......#:.....#::.:...........#...............:..............#......:.............::.:.....:.:..::..:...............#..#
 89  #..:...........::.:...:......:......:....:......................#:#:..:...........::::.:.....::.:......................#
 90  #.............::...:....:..................#........:.......................:...#.::::::.....::::................:.....#
 91  #.:.........:.#::.:.....:....:..................#........:.......................:..::::....::::::#................:...#
 92  #...:..........::::........................:......#.......#..:....:....:#.....#......::::....:.:::.....:...:...#.......#
 93  #..............:......................:......................#..................:......::...::.::.........::#......##..#
 94  #........:....::..........:...:.........:...............#..:.:..............:...........:::..::::.......:..............#
 95  #......:..:....:...:..:..........:........................:...:.............:..........:.:PPPPccPPPPP..:..:.:..........#
 96  #....:........::............:..::...:..:~~~~~~~..:......#.............:.........:...:.....PcccccccccP.......#..........#
 97  #....................:....:.........:..~~~~~~~~~......:.....#....:...:.........:........:.PcccccccccP......:...........#
 98  #......................##......#.......~~~~~~~~~....#........:.:.......#..:...............PcccccccccP..............:..:#
 99  #.......:..............:........#...:..~~~~~~~~~~...#.....................#...............PcccccccccP....#.............#
100  #..........#..#...:.........:..:.....:..~~~~~~~~~...................:..........#..........PcccccccccP...:...........:..#
101  #.....:......#......:.................:.~~~~~~~~~..............#.#......~~~........#......PcccccccccP.:......:.#.......#
102  #...............:.:.:.........:.:......#~~~~~~~~:...........:..........~~~~~....:.........PcccccccccP..#:..........::..#
103  #.............:.....................#....~~~~~~.....~~~~~.......:.....~~~~~~~:....:.......PcccccccccP.......:......#..:#
104  #.......................:..............:.:#.....#:.~~~~~~~~~~~~~~~~~~~~~~~~~~~~......:....ccccccccccc..................#
105  #.::..............................................~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~....#.....ccccccccccc..........:#......#
106  #:...:.:...................:.....:......:#.....:..~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~.............................:...#.....#
107  #....:.............#.............#.:....#:.....:.~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~.:......#.................:............#
108  #:...........#..............:..:............:....~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~....................:..:.::........:...#
109  #.................:.............:.........#.:....~~~~~~~~~~~~~~~~~~~~~~~~~~~~~.:.....................:.:....:..........#
110  #....:....:.............#........................~~~~~~~~~~~~~~~~~~~~~~~~~~~~....#..:..:...:....#......................#
111  #.:...:............:.#:#...:.........#...........~~~~~~~~~~~~~~~~~~~~~~~~~~~.......:.......:......:.:.................:#
112  #...................:............................~~~~~~~~~~~~~~~~~~~~~~~~~..........:.:..........:...#.:....#.#....:...#
113  #...:..:..#..:.............................#......~~~~~~~~~...:..~~:......::..:.......:................................#
114  #.........................:.:....:......:....#..:..#~~~~~~...................:............:............................#
115  #.....#..............:.:...#...........:.....:.:...#.........:.....:......:...................:.......:.:.....#....:...#
116  #......:...........................:.:......:...:..#.......:..........:.................................#..............#
117  #.....#......:......:.#..:.......#......:..............::........:...........:......:.:.......#.......:.........#.#....#
118  #......................:.......:........................:.................................:.:.........................:#
119  ########################################################################################################################```

## Appendix B — exporting the baked maps from the web build

The maps are generated when `src/data/maps/index.ts` is imported, so the simplest exporter is a throw-away Vitest file
run with the repo's Phaser mock (no change to `src/` needed). Example config + test (paths absolute to the checkout):

```js
// export.config.mjs
export default {
  resolve: { alias: { phaser: '<repo>/src/__mocks__/phaser.ts' } },
  test: { include: ['**/*.export.test.ts'], environment: 'node', root: '<dir of this file>' },
};
```
```ts
// maps.export.test.ts
import { test } from 'vitest';
import { writeFileSync } from 'fs';
import { createHash } from 'crypto';
import { AllMaps } from '<repo>/src/data/maps/index';
const sha = (s: string) => createHash('sha256').update(s).digest('hex').slice(0, 16);
test('export maps', () => {
  for (const [id, m] of Object.entries(AllMaps)) {
    const tiles = m.tiles.map(r => r.join(''));
    const decorations = (m.decorations ?? []).map(d => [d.col, d.row, d.type]);
    const walkable = m.collisions.flat().filter(Boolean).length;
    const golden = {
      tilesSha: sha(m.tiles.map(r => r.join(',')).join(';')),
      decorSha: sha((m.decorations ?? []).map(d => `${d.col},${d.row},${d.type}`).join(';')),
      walkable,
    };
    const { tiles: _t, collisions: _c, decorations: _d, ...anchors } = m;
    writeFileSync(`<out>/${id}.map.json`, JSON.stringify({ ...anchors, tiles, decorations, golden }));
  }
});
```
Run from the repo root: `npx vitest run --config <dir>/export.config.mjs`. Export before the camp blockers of §3.4 are
applied (they are applied by `ZoneScene`, not at module load), so `golden.walkable` is 13 167 for emerald_plains.
