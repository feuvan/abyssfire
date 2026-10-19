# unreal/GameData — the remake's own content

TypeScript content modules the Unreal remake owns (dark remake, `unreal/Docs/dark-remake/`, decision DR2). The data
exporter loads them **in place of** the web game's content modules, so `unreal/Data/*.json` is generated from the remake
content while the web game under `src/` stays untouched. Not to be confused with `unreal/Content` (the UE asset folder).

Spec: `unreal/Docs/dark-remake/implementation-plan.md` §2. Exporter side: `unreal/Tools/export-data/README.md`
("Remake content"). Output conventions (`remake` blocks): `unreal/Data/README.md`.

## Export

```bash
node unreal/Tools/export-data/run.mjs                 # write unreal/Data/*.json + CoreTests/golden/maps
node unreal/Tools/export-data/run.mjs --check         # fail if unreal/Data is stale (CI: data-check)
node unreal/Tools/export-data/run.mjs --out=/tmp/x    # export elsewhere and `diff -r` to review a content change
node_modules/.bin/tsc -p unreal/Tools/export-data/tsconfig.json   # type-check GameData + the exporter (every export runs it first)
```

Arguments are strict (`--check` or `--out=DIR`; anything else, including `--out DIR`, exits 2 before exporting).

Every content change is a change here followed by the export (never hand-edit `unreal/Data`). Keep `git diff -- src/`
empty: the web game is read-only for the remake.

## Layout

| File | What | Replaces / feeds |
|---|---|---|
| `redirects.json` | web module path → GameData module path (below) | the exporter's redirect plugin |
| `quests/all_quests.ts` | quests (slot order = the web's; generator inputs per slot) | `src/data/quests/all_quests.ts` |
| `npcs.ts`, `dialogueTrees.ts` | NPCs (ids kept as role keys, DR3) and dialogue trees | `src/data/npcs.ts`, `src/data/dialogueTrees.ts` |
| `miniBosses.ts` | mini-boss defs, spawns and pre-fight dialogues | `src/data/miniBosses.ts` |
| `monsters/emerald_plains.ts` | the Chapter 1 roster (other zones stay web until redesigned) | `src/data/monsters/emerald_plains.ts` |
| `maps/emerald_plains.ts` | the Chapter 1 map anchors (generator inputs) | `src/data/maps/emerald_plains.ts` |
| `loreCollectibles.ts` | lore entries | `src/data/loreCollectibles.ts` |
| `story/script.ts` (+ `story/types.ts`) | prologue, chapter cards, cutscenes, boss intros, triggers | `src/data/story/script.ts` |
| `homestead.ts` | Ember Tower data (unlock quests, buildings, ...) | `src/data/homestead.ts` |
| `config.ts` | chapter config: story boss (M7), chapter-complete quest, finale cutscene, final boss, sealed gate (W7) | ids the exporter used to hard-code (a changed value is recorded as `remake.replaced: ["config.<key>"]`) |
| `world.ts` | `REMAKE_OVERRIDES` for tables inside web system files (below) | achievements, events, moods, affixes, terrain |
| `types.ts` | web types re-exported + remake-only fields (below) | — |
| `i18n/zh-CN.ts`, `i18n/en.ts` | remake strings (below) | merged over the web tables |
| `tsconfig.json` | editor / `tsc -p` project (`@web/*` paths) | — |

Added by later packages (implementation-plan.md §2.1, §9): `setPieces.ts` and `autopilot/ch1.ts` with their exporter
tables (`set_pieces.json`, `autopilot_routes.json`) in the content switch (C1), `maps/sub_rf_pit.ts` in Wave 3, and the
per-area string files (`i18n/ch1.ts`, `story.ts`, `affixes.ts`, ...) as the tables grow.

## Snapshots (Phase 0)

Each redirected module started as a byte-for-byte copy of the web module it replaces, so the first export equals the
web's. Imports are unchanged where they resolve to a GameData sibling (`./types`, `../types`, `./dialogueTrees` mirror
the web layout); imports of web code were rewritten to `@web/...` (only `homestead.ts`: `@web/systems/CombatSystem`,
`@web/data/items/bases`). `redirects.json` `snapshots` records each copy's sha256 (import / export specifiers blanked)
when it is taken; content edits follow in later commits. Once a module differs from its recorded digest, every file
built from it — tables whose `source` cites the web path, tables and golden maps that read it through web code (the
generator's landmarks: lore, mini-boss spawns, quest locations) — gets `remake.modules` naming the GameData module. The
live web file is never compared, so a web-only edit of a redirected module changes nothing in `unreal/Data`.

## Redirects and imports

`redirects.json` → `{"modules": {"src/<web path>.ts": "unreal/GameData/<path>.ts"}, "snapshots": {"unreal/GameData/<path>.ts":
"<sha256>"}}`, repo-relative. The redirect works on resolved module ids, so any importer (web systems, the exporter
tables) gets the GameData module. Rules, each enforced by the export:

* Both ends must exist, be normalised `.ts` paths (no `.`, `..` or empty segment) inside `src/` and `unreal/GameData/`,
  and each target is used once (checked before Vite starts). Every target has a `snapshots` digest (a missing one fails
  and prints the digest to record while the module is still a copy of the web one).
* Every listed web path must actually be loaded during the export; an entry nobody imports is stale and fails it.
* Every GameData module must be loaded during the export (`types.ts` and `story/types.ts` are type-only and may not
  export values): a module that nothing loads — a dropped `redirects.json` entry, a new module with no redirect or
  exporter table — fails it instead of silently exporting the web content.
* A GameData module imports web **code** through `@web/...` (`@web/data/items/bases`), never `../../src/...` or
  `/src/...`.
* A GameData module never imports a **redirected** web path (`@web/data/dialogueTrees`): import the GameData sibling
  (`./dialogueTrees`) so the remake content is used everywhere.
* Types come from `./types` (`../types`), not from `@web/data/types`, so remake fields reach every module.

To take over another web content module: copy it here (rewrite its web imports to `@web/...`), add the redirect, run the
export and record the snapshot digest it prints under `snapshots`, run it again (still byte-identical), then edit the
content.

## Overrides (`world.ts`)

Tables that live inside web system files (code, so not redirected) are overridden per entry: `REMAKE_OVERRIDES.<table>`
= `{replace?: {key: entry}, add?: {key: entry}, remove?: [key]}`. `replace` keeps the entry's slot (in id-keyed lists the
new entry may carry a new id: a rename in place), `add` appends, `remove` drops. Entries are replaced whole (spread the
web entry from `@web/...` to change one field). Tables: `achievements`, `randomEvents` (per zone; also feeds the
wandering merchant), `zoneMoods.{themeByZone, moods, palettes, weather, weatherByTheme, campThemes, fog}`,
`eliteAffixes.{definitions, zoneAffixCounts}`, `terrainStyles`. A table name the exporter does not know, a `replace` /
`remove` key the web table lacks or an `add` key it already has fails the export. Each change is recorded in the written
file's `remake` block (`"replaced": ["achievements.ach_kill_slime"]`).

## Remake-only fields (`types.ts`) and `REMAKE_SCHEMA`

`types.ts` extends the web types with the fields of implementation-plan.md §2.6 (Wave 1 fields typed now: monster
`family`, `damageType`, `onHitStatus`, `projectileColor`; quest `progressKey`, `rewards.choiceLabels`,
`defendTarget.waves` / `approachDeg`; map `presentationTheme`, `regions` (the Breach Rim mood region:
`{id, col, row, radius, mood}`), `subDungeonEntrances[].sealed` / `sealedTextKey`; story `focus` step `anim`). A field
may be authored before the core reads it: the exporter writes a remake-only field only once its feature is in
`REMAKE_SCHEMA` (`unreal/Tools/export-data/src/main.ts`; registry `REMAKE_FIELDS` in `src/remake.ts`) and prints what it
withheld. A remake-only *value* (objective type `clear_area`) fails the export until its feature lands. Authored monster
`onHitStatus` / `projectileColor` replace the keyword rules in `derived` right away (the core already reads `derived`).

Unknown fields fail the export twice over: tsc (run before every export) rejects a misspelt key in a typed record, and the
record key gate (`checkRecordKeys`, key lists in `Tools/export-data/src/remakeKeys.ts`) rejects any key of a monster,
quest (and its `rewards`, `defendTarget`, `objectives[]`), map (`subDungeonEntrances[]`, `spawns[]`), NPC or cutscene
(`steps[]`) that is neither a web field nor a remake field — also one built through a spread or a cast. To add a remake
field: type it here, add it to the level's `remake` list in `remakeKeys.ts` (tsc requires it) and register it in
`REMAKE_FIELDS` with its feature (the export requires it).

## Strings (`i18n/`)

Per locale the exporter merges: web table → the exporter's port strings → `i18n/<locale>.ts` (the remake wins; a replaced
key keeps its position, a new key is appended; zh-TW is converted from the merged zh-CN). Rules, enforced by the export:

* `zh-CN.ts` and `en.ts` define the same keys.
* Orphan sweep: in the content namespaces (`data.quest.`, `data.questTarget.`, `data.questClue.`, `data.questChoice.`,
  `data.monster.`, `data.npc.`, `data.dialogue.<tree>.<node>`, `data.miniBossDialogue.<monster>.<node>`, `data.lore.`,
  `data.storyDeco.`, `data.hiddenArea.`, `data.escortNpc.`, `data.defendTarget.`, `data.achievement.`,
  `story.<cutsceneId>.` (cutscene ids start with `cs_`), `story.boss.`) a web key whose id no exported table references
  any more is dropped and listed in the string tables' `remake.swept`; a remake key that would be swept fails (typo).
* Parity: every key the exported tables reference (key-shaped strings such as `progressKey`, `nameKey`, story step
  text, plus each namespace's required keys) exists in zh-CN and en.
* zh-CN first, then en; player-facing text follows the story bible's voice guide.
