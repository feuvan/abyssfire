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
| `config.ts` | chapter config: story boss (M7), chapter-complete quest, finale cutscene, final boss, sealed gate (W7), `contentUpgrade` (beats an older save replays, DR3) | ids the exporter used to hard-code (a changed value is recorded as `remake.replaced: ["config.<key>"]`) |
| `world.ts` | `REMAKE_OVERRIDES` for tables inside web system files (below) | achievements, events, moods, affixes, terrain |
| `types.ts` | web types re-exported + remake-only fields (below) | — |
| `i18n/zh-CN.ts`, `i18n/en.ts` | remake strings (below) | merged over the web tables |
| `i18n/ch1World.ts` | Chapter 1 world strings as `[zh-CN, en]` pairs (zone, roster, mini-boss lines, hunts, boss intro, lore, hidden area, story decorations, the sealed Pit, new NPCs, achievements, events, elite affixes) | spread into both locales |
| `i18n/ch1Quests.ts`, `i18n/ch1Npcs.ts`, `i18n/ch1Story.ts` | Chapter 1 narrative strings, same form: quests (name, quest-log line, offer, progress, turn-in), targets, clue notes, choice labels, the escort and the gate; the re-cast NPCs, their quest-gated lines (Wave 2), the Captain's tree, the Lanternwisp; prologue, chapter card, cutscenes | spread into both locales |
| `autopilot/ch1.ts` (+ `autopilot/types.ts`) | the bot's Chapter 1 route: checkpoints, notes and steps (talk, dialogue tree, quest card, shop, gather, hunt, boss by role, clues, explore, escort, defend, walk, portal, equip) | new: `autopilot_routes.json` (no web module) |
| `setPieces.ts` | the UE set-piece layer (1C U4, DECISIONS DR15): authored prop groups per zone (Knellwatch's bell frame, hearth dressing and Name Wall, the sealed Pit mouth, the Boneyard Gate, Ashwheat's bridge and cottages, the Sheepfold's wagon ring, the Breach Rim's dais / ash circles / banner ring, helmet stakes) with anchor (tile, camp prop, entrance, region, defend target), offset, ring, facing, art id chain / variant, socket effects / audio emitters / cue ripples, echo poses, occluder, `walkOver` reason, `yieldsToDefendTarget`. Presentation only: not generator input, not walkability | new: `set_pieces.json` (no web module) |
| `tsconfig.json` | editor / `tsc -p` project (`@web/*` paths) | — |

Added by later packages (implementation-plan.md §2.1, §9): `maps/sub_rf_pit.ts` in Wave 3.

**Set pieces (package 1C U4).** `setPieces.ts` is checked twice: by the exporter (`src/tables/setpieces.ts`: zone,
anchors, art ids in the manifest, recipes in `Art/Export/VFX/vfx_recipes.json`, emitters / cue ids in
`Audio/Export/audio_manifest.json`, echoes = echo lore of the zone) and by `CoreTests/tests/setpieces_test.cpp` on the
placed pieces: on the map, off water / rock / blocking decorations, art resolving, and no piece without a `walkOver`
reason covering a tile the bot's route walks or an interaction point (NPC, lore, clue, explore centre, story
decoration, entrance, defend target). Moving a piece means re-running both; positions follow chapter1.md and the art
lane's vignettes (`Art/blender/README.md` §29), the notes in the file say why each one stands where it does.

**The bot's route (package 1B part 3, implementation-plan.md §6).** `autopilot/ch1.ts` is the script the headless
playthrough test and the in-game autotest play (`abyss::AutoPilot` interprets it; `playthrough_test.cpp` checks the
state at its checkpoints). It is content: the exporter checks every id it names against the exported quests, NPCs,
dialogue trees, monsters, hunts, mini-boss and story boss, every tile against the map and the clue / explore circles,
and fails when a quest of the route's zone is never accepted by a step, so a content change that strands the bot fails
the export. A quest, NPC or tree change usually needs a route change and a re-pin of `playthrough_test.cpp`.

**Chapter 1 world data (package 1B part 1, DECISIONS DR3 / DR4 / DR5 *as built*).** The roster, mini-boss, hunts,
story boss, spawn table, lore and echoes, hidden area, story decorations, sealed Pit entrance, camp lists (two new NPC
records), achievements, event pool and world overrides are the remake's; the quest slots keep their web ids and text
and only point into the new world (kill / drop targets, hunts, the M5 defend target of the one golden re-baseline).

**Chapter 1 narrative (package 1B part 2, DECISIONS DR3 *narrative as built*).** The eleven quests carry their remake
ids (`q_rf_*`) in their web slots, with every generator input of the slot kept, a `progressKey` each and labelled choices
on S3 / S4; the kept NPC role keys are re-cast for Knellwatch (`npcs.ts`: names, one Wave 1 line each, the quest lists
per giver), the Captain's 13-node tree replaces the elder's (`dialogueTrees.ts`), and the prologue, chapter card,
`cs_rf_*` cutscenes and their triggers replace the web's Chapter 1 beats (`story/script.ts`). `config.ts` and
`homestead.ts` point the story boss, the finale and the tower wings at the new quests. Chapters 2-5 stay as copied.

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
wandering merchant), `zoneMoods.{themeByZone, moods, palettes, weather, weatherByTheme, campThemes, fog, lightScale,
moodWeather, windupTint}` (the last three are remake tables with no web entries: R18 light-scale overrides, the weather a
mood brings and the monsters' wind-up body tint (DR5), per mood theme, written to `zone_moods.json` `port`),
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

Written now (the core's K1 hooks read them, DECISIONS DR8 *K1 as built*): quest `progressKey`, `rewards.choiceLabels`
(one key per `choices` slot), `defendTarget.waves` (one `{monsterId, count}[]` per wave, exactly `totalWaves`) and
`approachDeg` (tile-space degrees, 0 = +col, 90 = +row), map `subDungeonEntrances[].sealed` + `sealedTextKey` (both or
neither), and the `anim` of a `focus` step on a `{monster}` target (`roar` / `taunt`). The export checks those shapes;
the core checks that every key exists in zh-CN and en and every wave monster exists. Since 1B part 1 also map
`presentationTheme` and `regions` (`MapDef::presentationTheme` / `regions`; each must name a `zone_moods.json` mood,
checked by the export and by DataStore). Since K2 (package 2A, DECISIONS DR8 *K2 as built*) also monster `abilities`
(`RemakeMonsterAbility[]`: kind, clip, windup / cooldowns, conditions, aim, shape, dmgMul / damageType / status, hazard,
summon, buff; the kinds raise / summon / buff / call failed the export until K3 / K5), `damageType` and
`hitCapHeroMaxHpPct` (percent of the hero's max life, scaled by the difficulty's damage multiplier); `Tools/export-data`
`tables/monsters.ts` checks every ability field and key, and DataStore checks them again (implementation-plan.md
*2A outcomes*). Since K3 (package 2B, DECISIONS DR8 *K3 as built*) also monster `family`, `onDeath`
(`RemakeOnDeath`), `corpse` (`RemakeCorpse`) and `riseAgain` (`RemakeRiseAgain`), the raise / summon ability kinds (summon
`radius`), map `corpseSeeds` and, through `world.ts` `eliteAffixes.definitions` (`RemakeEliteAffixDefinition`), an elite
affix's `onDeath`, `riseAgain` and `families` (implementation-plan.md *2B outcomes*). Since K4 (package 2C, DECISIONS
DR8 *K4 as built*) also monster `phases` (`RemakeMonsterPhase[]`: thresholds, the enter action, multipliers, the tether
`leash`, ability switches, phase `summons`, the shrinking-ring `arena`, `feed`, a music `stinger`; a centre left out is the
monster's map spawn tile; implementation-plan.md *2C outcomes*). Since K5 (package 2D, DECISIONS DR8 *K5 as built*) also
monster `packAggro`, `kite` (`RemakeKite`), `block` (`RemakeBlock`), `morale` (`RemakeMorale`), `aura` (`RemakeAura`),
the buff / call ability kinds (`RemakeBuff`: a call's `stat` is optional) and map spawn `mix` / `leader` (`MapSpawn`; a
spawn entry is a pack: the mix sums to its `count` and keeps the entry's `monsterId` among its members;
implementation-plan.md *2D outcomes*). Still withheld: every other Wave 2 / 3 field.

Unknown fields fail the export twice over: tsc (run before every export) rejects a misspelt key in a typed record, and the
record key gate (`checkRecordKeys`, key lists in `Tools/export-data/src/remakeKeys.ts`) rejects any key of a monster,
quest (and its `rewards`, `defendTarget`, `objectives[]`), map (`subDungeonEntrances[]`, `spawns[]`), NPC or cutscene
(`steps[]`) that is neither a web field nor a remake field — also one built through a spread or a cast. To add a remake
field: type it here, add it to the level's `remake` list in `remakeKeys.ts` (tsc requires it) and register it in
`REMAKE_FIELDS` with its feature (the export requires it).

## Strings (`i18n/`)

Per locale the exporter merges: web table → the exporter's port strings → `i18n/<locale>.ts` (the remake wins; a replaced
key keeps its position, a new key is appended; zh-TW is converted from the merged zh-CN). Rules, enforced by the export:

* zh-TW of a key defined here: the web converter misses many characters, so `i18n/zhTWExtra.ts` supplements it
  (`ZH_TW_PHRASES` for the characters whose Traditional form depends on the word: 皇后 / 之后, 一只 / 只是, 石台, 公里,
  干了的, 头发, 关系, ...; `ZH_TW_CHARS` for the characters the web map lacks; Taiwan forms). Web keys are not touched.
  `content_ch1_test` "Chapter 1 zh-TW" fails when a zh-TW string Chapter 1 shows keeps a Simplified-only character
  (`CoreTests/golden/zh_simplified_only.txt`): add the character (and a phrase when its form depends on the word). A
  name with 后 里 于 余 采 尸 叶 涂 needs a phrase entry keeping it.

* `zh-CN.ts` and `en.ts` define the same keys.
* Orphan sweep: in the content namespaces (`data.quest.`, `data.questTarget.`, `data.questClue.`, `data.questChoice.`,
  `data.monster.`, `data.npc.`, `data.dialogue.<tree>.<node>`, `data.miniBossDialogue.<monster>.<node>`, `data.lore.`,
  `data.storyDeco.`, `data.hiddenArea.`, `data.escortNpc.`, `data.defendTarget.`, `data.achievement.`,
  `story.<cutsceneId>.` (cutscene ids start with `cs_`), `story.boss.`, `story.prologue/epilogue/credits.<n>.<field>`)
  a web key whose id no exported table references any more is dropped and listed in the string tables'
  `remake.swept`, and so is an NPC line `data.npc.<id>.dialogue.<n>` past the end of the NPC's exported `dialogue`; a
  remake key that would be swept fails (typo, or a line the NPC record does not have).
* Parity: every key the exported tables reference (key-shaped strings such as `progressKey`, `nameKey`, story step
  text, plus each namespace's required keys) exists in zh-CN and en.
* zh-CN first, then en; player-facing text follows the story bible's voice guide.

**1B review fixes (2026-10, pending review).** Chapter 1 exp: regulars round(0.75 × monsters.md §2.0), Valdras 110,
Hungerbell hp ×2 / dmg ×1.0 (DR9 band 12-14); M5 / S5 embers 7 / 3 (chapter1.md §7: 23 in all); `story/script.ts`
kill triggers of the Corpseherd and Valdras carry `whileQuest` (`story/types.ts` `StoryTrigger`); `world.ts` re-voices
the achievements and three affix names and gives the Red Field's rescue its guards (`rescueMonsters`,
`RemakeZoneEventData`); `i18n/ch1World.ts` adds the jukebox rows, the zone's own event lines and the parked lines (Old
Bern, hub barks). The amendments to the design are listed in `Docs/dark-remake/chapter1.md` §19.
