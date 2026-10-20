# export-data — web TS tables → `unreal/Data/*.json`

Imports the web game's TypeScript modules **directly** (so every number is the web's) and writes the JSON tables the
C++ core loads at boot, plus the golden map fixtures for `CoreTests`. Schema of every output: `unreal/Data/README.md`.
Content the remake owns (the dark remake, `unreal/Docs/dark-remake/`) lives in `unreal/GameData/` and replaces web
content modules through a module redirect (below; `unreal/GameData/README.md`).

## Run

From the repo root (Node 22, after `npm install` at the root; nothing to install here):

```bash
node unreal/Tools/export-data/run.mjs           # write unreal/Data/*.json + unreal/CoreTests/golden/maps/*.json
node unreal/Tools/export-data/run.mjs --check   # export to a temp dir, diff against the committed files (exit 1 if stale)
node unreal/Tools/export-data/run.mjs --out=DIR # export into DIR/Data + DIR/CoreTests (review a content change; diff -r)
node_modules/.bin/tsc -p unreal/Tools/export-data/tsconfig.json   # type-check the exporter, unreal/GameData and the TS they import
```

or `npm run export | check | typecheck` inside this directory. Every run type-checks first (the tsc line above; a type
error fails the export) and rejects any other argument (`--out DIR` with a space exits 2 instead of overwriting the
committed data). A run takes ~6 s and prints a record-count summary.
Output is deterministic (same source → byte-identical files); re-run and commit the outputs whenever `src/data`,
`src/i18n` or a cited system changes.

## How it works

* `run.mjs` starts Vite's dev server in middleware mode (the repo's own Vite, no `tsx`/`vite-node` needed) and loads
  `src/main.ts` with `server.ssrLoadModule`, so TS modules are transpiled exactly as the game imports them.
  Aliases: `phaser` → `src/phaser-shim.ts` (the repo's test mock + `Math.Distance.Between` + inert stubs so systems
  that extend Phaser classes still load). `__BGM_MANIFEST__` is defined the same way as `vite.config.ts` does.
* **Private constants** (module-level `const`s the web does not export, e.g. `THEME_CONFIGS`, `SPIRIT_PROFILES`,
  `GROUND_AOE_SKILLS`) are listed in `src/expose.ts`; a Vite transform appends `export { X as __expose_X }` to those
  modules **in memory**. Nothing under `src/` is modified. A renamed constant makes the module fail to load.
* `Math.random` is replaced by a seeded PRNG while the exporter runs, so module-load randomness cannot leak in.
* One module per area under `src/tables/` (`skills, combat, items, monsters, maps, world, quests, story, pets, audio,
  i18n, ui, assets, autopilot, setpieces`); every exported function named `export*` returns `TableResult`s, and `src/main.ts` writes them
  with `{schemaVersion: 1, source: [...]}` and the deterministic formatter in `src/util.ts`, then `Data/index.json`.

### Remake content (unreal/GameData)

implementation-plan.md §2 (dark remake). Everything below is a no-op while GameData equals the web content (Phase 0:
the export is byte-identical to the web's).

* **Redirect plugin** (`run.mjs`, `abyss-export:redirect`, enforce `pre`): every import that *resolves* to a web path
  listed in `unreal/GameData/redirects.json` loads the GameData module instead, so every web module the exporter reads
  (MapGenerator with `externalLandmarks`, `questGiverOf`, `makeHuntDefinition`, the ZoneScene probes, EmberTower, ...)
  sees the remake content. Failures, all loud: a target or web path that does not exist or is not a normalised path
  inside `src/` / `unreal/GameData/` (checked before Vite starts), a redirected web path that no module loaded during
  the export (`checkRedirects` in `main.ts`), a GameData module that no module loaded (`checkGameDataLoaded`: a dropped
  entry or an unwired module; the `load` hook records them, the summary lists them, `types.ts` / `story/types.ts` are
  type-only), a GameData module that imports a redirected web path, or one that reaches `src/` by a relative or
  root-relative path instead of `@web/...`.
  `assertSource` / `readSource` and `src/expose.ts` entries for a redirected web path apply to its GameData module.
* **`@web` alias** = the web's `src/` (Vite alias here, `paths` in `tsconfig.json`; `unreal/GameData/tsconfig.json`
  maps it for editors).
* **Override hook** (`src/remake.ts`): tables that live inside web *system* files (code, not redirected) take
  `unreal/GameData/world.ts` `REMAKE_OVERRIDES` per entry — `replace` / `add` / `remove`, via `overrideEntries` /
  `overrideById` — in `world.ts` (achievements, random-event zones + the wandering merchant, zone moods: themeByZone,
  moods, palettes, weather, weatherByTheme, campThemes, fog), `combat.ts` (elite affix definitions + `order`, zone affix
  counts) and `ui.ts` (terrain styles). Unknown override tables, entry keys that the web table lacks (replace / remove)
  or already has (add), and a table present in REMAKE_OVERRIDES but never applied fail the export.
* **`remake` blocks**: `main.ts` appends `{"remake": {...}}` to each written table and golden fixture the remake
  changed — override records (`replaced` / `added` / `removed`; `config.<key>` for a GameData/config.ts value that
  differs from the web chapter's, `noteConfig`), the redirected modules a file is built from once their GameData module
  no longer matches its snapshot digest (`modules`; `redirects.json` `snapshots`, import specifiers blanked — never the
  live web file), i18n `swept` keys and string counts. "Built from" = the table's `source` plus its provenance-only
  `deps` (modules read through web code: the generator landmarks for maps.json and the golden maps, the finale trigger
  for monster_ai.json, hunt base monsters, quest embers).
* **`REMAKE_SCHEMA`** (`main.ts`) lists the remake-only features the C++ core parses. `REMAKE_FIELDS` (`src/remake.ts`)
  registers every remake-only field per record kind (monster, quest, map, npc, cutscene) with its feature;
  `withholdRemakeFields` first fails on a record key that is neither a web field nor a remake field (`checkRecordKeys`;
  per-level key lists in `src/remakeKeys.ts`, pinned to the web and GameData types by `satisfies`, so tsc fails when a
  type gains a key the lists lack; a typed remake field without a REMAKE_FIELDS row fails at start), then strips the
  registered fields whose feature is not listed (the run summary prints what it withheld) and
  `gateRemakeValues` fails on a remake-only value (objective type `clear_area`). A core package adds its feature ids in
  the change that teaches the loaders to read them. K1 (package 1A) enabled `quest.progressKey`, `quest.choiceLabels`,
  `quest.defendWaves`, `quest.defendApproach`, `map.sealedEntrances` and `story.focusAnim`; the 1B review added
  `story.triggerWhileQuest` (a story trigger's `whileQuest`, record kind `trigger`; story.ts checks it names a quest and sits
  on a monster_killed trigger, and writes `GameData/config.ts` `contentUpgrade` into story.json); 1B part 1 enabled
  `map.presentationTheme` and `map.regions` (world.ts checks that each names a mood); `quests.ts`, `maps.ts` and
  `story.ts` check their shapes (one label per choice slot, one composition per wave with counts ≥ 1, `sealed` with
  `sealedTextKey`, `anim` only on a `{monster}` focus step) before the core loader would refuse them. Authored monster `onHitStatus` / `projectileColor` need no feature:
  they replace the keyword rules in `derived` (which the core already reads) and are never written at top level.
* **Chapter config**: `monsters.ts` (DECISIONS M7 story boss), `audio.ts` (`music.json` `bossMusic`: the story boss
  scores `boss_ch1`), `story.ts` / `monsters.ts` (`finalBoss`) and `world.ts` (W7 sealed gate) read
  `unreal/GameData/config.ts` and check every id against the exported content.
* **Remake mood tables** (`world.ts`): `zoneMoods.lightScale` and `zoneMoods.moodWeather` are override tables with no web
  entries (an empty web table, `add` only), written to `zone_moods.json` `port.lightScale` / `port.moodWeather` when
  non-empty; every mood-keyed table, zone theme, map `presentationTheme` and region mood must name a `moods` entry.
* **autopilot** (`src/tables/autopilot.ts`, implementation-plan.md §6): writes `autopilot_routes.json` from
  `unreal/GameData/autopilot/ch1.ts` verbatim (step order = play order, plus `stepKinds`, which must match the core's
  `AutoPilotStepKind`). It checks the route as content: every quest, NPC, dialogue-tree path, monster / hunt id, role,
  objective index and count against the tables it exports, every tile against the map, clue tiles and explore tiles
  against their objective's circle, at most one `smokeEnd`, unique checkpoints, and that a step accepts every quest of
  the route's zone. Runs before i18n.
* **set pieces** (`src/tables/setpieces.ts`, 1C U4): writes `set_pieces.json` from `unreal/GameData/setPieces.ts`
  with every optional field filled (`variant` -1, `offset` [0, 0], `ring` null or complete with `stepDeg` = 360 / count,
  `turnDeg` / `echoTurnDeg` 0, `scale` 1, empty lists, `walkOver` ""). Checks: a zone the maps export, unique ids,
  anchors (camp index, entrance, region, a defend-target quest of the zone), rings (count 1-64, radius > 0, skip in
  range), facings, the art ids in `Data/assets.json`, effect / cue recipes in `Art/Export/VFX/vfx_recipes.json`, emitters
  in `Audio/Export/audio_manifest.json` `ambience.emitters` and cue ids in its `cues`, echoes = echo lore of the zone,
  `echoTurnDeg` only with echoes, a non-empty `walkOver` reason.
* **i18n** (`src/tables/i18n.ts`, runs last and reads every other table): web table → `PORT_STRINGS` →
  `unreal/GameData/i18n/{zh-CN,en}.ts` (same key set in both, else the export fails; the remake wins). Orphan sweep:
  in the content namespaces (`CONTENT_NAMESPACES`: quest, questTarget, questClue, questChoice, monster, npc, dialogue
  node, miniBossDialogue node, lore, storyDeco, hiddenArea, escortNpc, defendTarget, achievement, `story.cs_*`,
  `story.boss`, and the slide keys `story.prologue/epilogue/credits.<n>.<field>`) a key whose id the exported tables no
  longer reference is dropped, as is an NPC line `data.npc.<id>.dialogue.<n>` past the end of its exported `dialogue`
  (`npcLineOrphan`; the re-cast Chapter 1 NPCs speak fewer lines than the web's); `WEB_I18N_DEBT` lists the keys the
  web itself never referenced (kept; each entry must stay unreferenced) and a GameData key that would be swept fails.
  Parity: every key-shaped string in the exported tables plus each namespace's required keys (quest name / desc,
  monster and hunt names, NPC names, dialogue node text and choices, lore, story decorations, hidden areas,
  achievements) must exist in zh-CN and en, and no content key may exist in only one of them.
  zh-TW: every key is converted from the merged zh-CN with the web converter (`src/i18n/converter.ts`); a GameData key
  then goes through `remakeToTraditional` (`unreal/GameData/i18n/zhTWExtra.ts`: `ZH_TW_PHRASES` matched on the zh-CN
  text first, longest key first, the web converter between them, then `ZH_TW_CHARS` for what it left Simplified), so
  web-sourced zh-TW strings stay byte-identical. `remake.zhTWSupplement` in `i18n_zh-TW.json` counts the keys, the
  strings the supplement changed and the table sizes; a `ZH_TW_CHARS` entry the web map already covers fails the export.
  `scripts/zh_simplified_only.py <OpenCC dictionary dir>` regenerates `CoreTests/golden/zh_simplified_only.txt`, the
  Simplified-only list the Chapter 1 zh-TW test checks against (not an export output).

### Derived fields and parity checks

DECISIONS C6 replaces the web's id / substring rules with explicit data (`classes.json` skills' `derived`,
`monsters.json` defs' `derived`). Those values live in **explicit mapping tables** in `src/tables/skills.ts` and
`src/tables/monsters.ts`, each citing the TS code it replaces, and every table is verified against the web code on
each run:

| Check | How |
|---|---|
| `statusRule` (skills), `onHitStatus` (monsters) | calls the real `ZoneScene.prototype.applySkillStatusEffect` / `applyMonsterStatusEffect` with a recording `statusEffects` stub and scripted `Math.random`; roll chances are found by bisection |
| `impactColor` | calls `skillImpactColor` (exposed from ZoneScene) |
| `projectile`, `aoeDelayMs` | calls `SkillEffectSystem.prototype.getProjectileTravelMs` at several distances |
| `groundAnchored` | compared with `GROUND_AOE_SKILLS` |
| `execKind`, `scorchWeb`, arrow delays, line target, passives, slow-trap rule | transcription of the `releaseSkill` branches + `assertSource` on the exact TS lines |
| hero formulas, regen, exp curve | `Player.prototype.recalcDerived / expToNextLevel / get*RegenPerSecond` on a fake `this` |
| loot quality thresholds | `LootSystem.rollQuality` with scripted draws over a grid of level / luck / elite / affix bonus |
| consumable effects, crafting costs, salvage yields, hunt definitions, difficulty scaling, spirit gain, hit classes, audio event → cue rules | the real functions (`useConsumable`, `craftCost`, `salvageYield`, `makeHuntDefinition`, `scaleMonster`, `gainFromCombat`, `classifyHit`, the live `AudioManager` listeners with `playSFX` spied) |
| constants that exist only inside code | `assertSource(file, snippet)` — whitespace-normalised substring check; the export stops with "TS source drift" if the line changed |

Generated maps: `maps.json` carries the authored anchors + generator inputs (theme, seed, `externalLandmarks`),
because the C++ MapGen reproduces the generation bit-exactly (DECISIONS W10). The TS generator output is written to
`unreal/CoreTests/golden/maps/<id>.json` (tiles, decorations, hashes) together with RNG / noise / `Math.hypot`
vectors (`_vectors.json`); the exporter warns if the hashes differ from those quoted in `world-map-nav.md`
(`SPEC_GOLDEN`; `emerald_plains` carries the DR4 re-baseline's hashes, noted in that spec's header).

Decision overrides (port values that differ from the web, e.g. I9 sell multipliers, C4 skill upgrades) are applied only
from explicit tables in the exporter and recorded in the output (`overrides`, `port` blocks) with their decision id.
`monsters.ts` `OVERRIDES` is empty since the remake roster: M5's range 3.0 for the web goblin shaman is authored by the
mini-boss holding its slot (`miniboss_corpseherd`), so `monsters.json` `overrides` is `[]`.

## Adding a table

1. Write `export function exportX(ctx): TableResult[]` in a `src/tables/*.ts` module (add the module to the list in
   `src/main.ts` if it is new, before `i18n`). Keep TS field names; use `plain()` for data and keep array order. A
   table embedded in a web system that the remake changes goes through `overrideEntries` / `overrideById`
   (`src/remake.ts`: add its path to `OverridePath`, its type to `unreal/GameData/world.ts`).
2. If you need a module-private binding, add it to `src/expose.ts`.
3. Pin any constant you transcribe from code with `assertSource`, or better, call the TS function that uses it.
4. Document the file in `unreal/Data/README.md`, run the exporter and `--check`.

## Not exported (on purpose)

Render-only layout tables (HUD rects, minimap colours, damage-number styles, camp prop offsets) are rebuilt in Slate /
the world builder; SFX synthesis recipes and music tracks belong to the audio renderer (`unreal/Tools/audio`);
animation events come from the art manifest (copied to `Data/assets.json` once `unreal/Art/Export/manifest.json`
exists); the font glyph list is produced by the font-subsetting step.
