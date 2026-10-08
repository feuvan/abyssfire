# export-data — web TS tables → `unreal/Data/*.json`

Imports the web game's TypeScript modules **directly** (so every number is the web's) and writes the JSON tables the
C++ core loads at boot, plus the golden map fixtures for `CoreTests`. Schema of every output: `unreal/Data/README.md`.

## Run

From the repo root (Node 22, after `npm install` at the root; nothing to install here):

```bash
node unreal/Tools/export-data/run.mjs           # write unreal/Data/*.json + unreal/CoreTests/golden/maps/*.json
node unreal/Tools/export-data/run.mjs --check   # export to a temp dir, diff against the committed files (exit 1 if stale)
node_modules/.bin/tsc -p unreal/Tools/export-data/tsconfig.json   # type-check the exporter (and the TS it imports)
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
   `src/main.ts` if it is new). Keep TS field names; use `plain()` for data and keep array order.
2. If you need a module-private binding, add it to `src/expose.ts`.
3. Pin any constant you transcribe from code with `assertSource`, or better, call the TS function that uses it.
4. Document the file in `unreal/Data/README.md`, run the exporter and `--check`.

## Not exported (on purpose)

Render-only layout tables (HUD rects, minimap colours, damage-number styles, camp prop offsets) are rebuilt in Slate /
the world builder; SFX synthesis recipes and music tracks belong to the audio renderer (`unreal/Tools/audio`);
animation events come from the art manifest (copied to `Data/assets.json` once `unreal/Art/Export/manifest.json`
exists); the font glyph list is produced by the font-subsetting step.
