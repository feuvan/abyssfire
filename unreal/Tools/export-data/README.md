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

or `npm run export | check | typecheck` inside this directory. A run takes ~3 s and prints a record-count summary.
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
  i18n, ui, assets`); every exported function named `export*` returns `TableResult`s, and `src/main.ts` writes them
  with `{schemaVersion: 1, source: [...]}` and the deterministic formatter in `src/util.ts`, then `Data/index.json`.

### Remake content (unreal/GameData)

implementation-plan.md §2 (dark remake). Everything below is a no-op while GameData equals the web content (Phase 0:
the export is byte-identical to the web's).

* **Redirect plugin** (`run.mjs`, `abyss-export:redirect`, enforce `pre`): every import that *resolves* to a web path
  listed in `unreal/GameData/redirects.json` loads the GameData module instead, so every web module the exporter reads
  (MapGenerator with `externalLandmarks`, `questGiverOf`, `makeHuntDefinition`, the ZoneScene probes, EmberTower, ...)
  sees the remake content. Failures, all loud: a target or web path that does not exist (checked before Vite starts),
  a redirected web path that no module loaded during the export (`checkRedirects` in `main.ts`), a GameData module that
  imports a redirected web path, or one that reaches `src/` by a relative path instead of `@web/...`.
  `assertSource` / `readSource` and `src/expose.ts` entries for a redirected web path apply to its GameData module.
* **`@web` alias** = the web's `src/` (Vite alias here, `paths` in `tsconfig.json`; `unreal/GameData/tsconfig.json`
  maps it for editors).
* **Override hook** (`src/remake.ts`): tables that live inside web *system* files (code, not redirected) take
  `unreal/GameData/world.ts` `REMAKE_OVERRIDES` per entry — `replace` / `add` / `remove`, via `overrideEntries` /
  `overrideById` — in `world.ts` (achievements, random-event zones + the wandering merchant, zone moods: themeByZone,
  moods, palettes, weather, weatherByTheme, campThemes, fog), `combat.ts` (elite affix definitions + `order`, zone affix
  counts) and `ui.ts` (terrain styles). Unknown override tables, entry keys that the web table lacks (replace / remove)
  or already has (add), and a table present in REMAKE_OVERRIDES but never applied fail the export.
* **`remake` blocks**: `main.ts` appends `{"remake": {...}}` to each written table the remake changed — override
  records (`replaced` / `added` / `removed`), the redirected modules a table's `source` cites once their GameData module
  diverges from the web one (`modules`; snapshots differ only in import specifiers), i18n `swept` keys and string counts.
* **`REMAKE_SCHEMA`** (`main.ts`) lists the remake-only features the C++ core parses. `REMAKE_FIELDS` (`src/remake.ts`)
  registers every remake-only field per record kind (monster, quest, map, npc, cutscene) with its feature;
  `withholdRemakeFields` strips the ones whose feature is not listed (the run summary prints what it withheld) and
  `gateRemakeValues` fails on a remake-only value (objective type `clear_area`). A core package adds its feature ids in
  the change that teaches the loaders to read them. Authored monster `onHitStatus` / `projectileColor` need no feature:
  they replace the keyword rules in `derived` (which the core already reads) and are never written at top level.
* **Chapter config**: `monsters.ts` (DECISIONS M7 story boss), `story.ts` / `monsters.ts` (`finalBoss`) and `world.ts`
  (W7 sealed gate) read `unreal/GameData/config.ts` and check every id against the exported content.
* **i18n** (`src/tables/i18n.ts`, runs last and reads every other table): web table → `PORT_STRINGS` →
  `unreal/GameData/i18n/{zh-CN,en}.ts` (same key set in both, else the export fails; the remake wins). Orphan sweep:
  in the content namespaces (`CONTENT_NAMESPACES`: quest, questTarget, questClue, questChoice, monster, npc, dialogue
  node, miniBossDialogue node, lore, storyDeco, hiddenArea, escortNpc, defendTarget, achievement, `story.cs_*`,
  `story.boss`) a key whose id the exported tables no longer reference is dropped; `WEB_I18N_DEBT` lists the keys the
  web itself never referenced (kept; each entry must stay unreferenced) and a GameData key that would be swept fails.
  Parity: every key-shaped string in the exported tables plus each namespace's required keys (quest name / desc,
  monster and hunt names, NPC names, dialogue node text and choices, lore, story decorations, hidden areas,
  achievements) must exist in zh-CN and en, and no content key may exist in only one of them.

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
vectors (`_vectors.json`); the exporter warns if the hashes differ from those quoted in `world-map-nav.md`.

Decision overrides (port values that differ from the web, e.g. M5 Goblin Shaman range 3.0, I9 sell multipliers,
C4 skill upgrades) are applied only from explicit tables in the exporter and recorded in the output (`overrides`,
`port` blocks) with their decision id.

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
