# Implementation plan (dark remake)

> How to build [chapter1.md](chapter1.md) and [monsters.md](monsters.md) in the Unreal remake without touching the web
> game. Order: content pipeline first, then the Wave 1 content switch on today's core (with small hooks), then the
> Wave 2 monster systems, then the Wave 3 descent; art, audio and UE presentation run beside the core lanes. Every
> package lists the files it owns (no two packages own a file at the same time), what it depends on, and the checks that
> prove it here and on the Mac.

## 1. Principles

- **The web game is read-only.** Nothing under `src/` changes. The exporter reads web modules and, through a redirect,
  remake modules in their place. `data:check` fails if the committed `unreal/Data` differs from what the sources export.
- **Data stays generated.** `unreal/Data/*.json` is never hand-edited (Data/README.md); every content change is a
  `unreal/GameData/**` change followed by `node unreal/Tools/export-data/run.mjs`.
- **The core stays data-driven and deterministic.** New behaviour is data on the monster or quest; new state is ticked
  on the 60 Hz sim clock with the `Ai` / `Combat` RNG streams; event order is stable (S1, S3). Core sources stay ASCII
  (`run.sh` rejects non-ASCII: no Chinese in `Source/AbyssCore`).
- **One golden re-baseline**, inside the content switch (C1). Wave 2 is golden-neutral (spawn mixes, abilities and set
  pieces are not generator inputs). Wave 3's Pit entrance is already placed in C1.
- **Tests stop depending on shipped content** where they test engines (synthetic fixtures); content pins live in one
  file per chapter, so a content change re-pins one file.
- **Every wave ends playable**: the three-class playthrough passes, the in-game autotest passes on the Mac.

## 2. Content ownership architecture

### 2.1 Layout

```
unreal/GameData/                       (TypeScript, remake-owned; NOT unreal/Content, which is the UE asset folder)
  README.md                            what lives here, how to export, the redirect rules
  redirects.json                       web module path -> GameData module path (§2.2)
  config.ts                            chapter config: storyBoss, chapterCompleteQuest, finalBoss, sealed exits
  types.ts                             re-exports @web/data/types and extends them (§2.6)
  quests/all_quests.ts                 snapshot of the web file; Chapter 1 entries rewritten (slot order kept)
  npcs.ts  dialogueTrees.ts  miniBosses.ts  loreCollectibles.ts  homestead.ts
  monsters/emerald_plains.ts           Chapter 1 roster (other zones stay web until redesigned)
  maps/emerald_plains.ts               positions kept except the planned re-baseline (chapter1.md §17)
  maps/sub_rf_pit.ts                   [W3] hand-authored 40 x 40 tiles
  story/script.ts                      snapshot; Chapter 1 sequences, cutscenes, triggers, boss intro rewritten
  world.ts                             overrides for tables embedded in web systems (§2.4)
  setPieces.ts                         UE set-piece groups anchored to tiles (not generator inputs)
  autopilot/ch1.ts                     the bot's Chapter 1 route (§6)
  i18n/zh-CN.ts  i18n/en.ts            remake strings, split per area (ch1.ts, story.ts, affixes.ts, ...)
```

Snapshots: the first commit copies each redirected web module byte-for-byte (rewriting only its imports to `@web/...`),
so the export is identical; content edits follow in later commits.

### 2.2 The redirect plugin (`unreal/Tools/export-data/run.mjs`)

Next to the existing `expose` plugin:

```js
const REDIRECTS = JSON.parse(readFileSync(resolve(repoRoot, 'unreal/GameData/redirects.json'), 'utf8')).modules;
function redirectPlugin() {
  return {
    name: 'abyss-export:redirect',
    enforce: 'pre',
    async resolveId(source, importer, options) {
      const r = await this.resolve(source, importer, { ...options, skipSelf: true });
      if (!r) return null;
      const rel = relative(repoRoot, r.id.split('?')[0]).replace(/\\/g, '/');
      const to = REDIRECTS[rel];
      if (!to) return null;
      if (importer && relative(repoRoot, importer).startsWith('unreal/GameData/'))
        throw new Error(`GameData module ${importer} imports redirected web module ${rel}; import the GameData one`);
      return resolve(repoRoot, to);
    },
  };
}
// createServer({... resolve: { alias: { phaser: ..., '@web': resolve(repoRoot, 'src') } }, plugins: [redirectPlugin(), exposePlugin()] })
```

Because the redirect works on resolved ids, every web module the exporter loads sees the remake content: the web
`MapGenerator` with `externalLandmarks` (so the goldens regenerate from the remake anchors), `questGiverOf`,
`makeHuntDefinition`, the `ZoneScene` on-hit probes, `EmberTower.STORY_PETS`. `tsconfig.json` adds `../../GameData` to
`include` and `paths: { "@web/*": ["../../../src/*"] }`. A self-test in `main.ts` asserts every redirect target exists
and every redirected path is actually loaded during the export (a stale entry fails the export).

`redirects.json` (Wave 1):

| Web module | GameData module |
|---|---|
| `src/data/quests/all_quests.ts` | `quests/all_quests.ts` |
| `src/data/npcs.ts` | `npcs.ts` |
| `src/data/dialogueTrees.ts` | `dialogueTrees.ts` |
| `src/data/miniBosses.ts` | `miniBosses.ts` |
| `src/data/monsters/emerald_plains.ts` | `monsters/emerald_plains.ts` |
| `src/data/maps/emerald_plains.ts` | `maps/emerald_plains.ts` |
| `src/data/loreCollectibles.ts` | `loreCollectibles.ts` |
| `src/data/story/script.ts` | `story/script.ts` |
| `src/data/homestead.ts` | `homestead.ts` |

Chapters 2-5 entries inside the snapshots stay as copied until each chapter's dark pass.

### 2.3 Exporter tables and their sources

| Exporter table (`src/tables/`) | Writes | Source after the switch | Edit in the exporter |
|---|---|---|---|
| `quests.ts` | quests.json, quest_tuning.json | GameData quests, npcs, homestead (via redirect) | pass the new quest fields (§2.6) |
| `monsters.ts` | monsters.json, minibosses.json, quest_hunts.json, monster_ai.json | GameData monsters, miniBosses, quests (hunts), story (boss list), maps | `STORY_BOSS` (lines 38-52) reads `GameData/config.ts` instead of asserting `goblin_chief` / `q_find_goblin_chief` / `q_secure_plains` / `cs_ep_finale`; `OVERRIDES` (M5) dropped for the remake mini-boss (range 3.0 is authored); authored `onHitStatus` / `projectileColor` win over the keyword rules (lines 64-71); pass schema-v2 fields |
| `maps.ts` | maps.json, map_gen.json, goldens | GameData maps (via redirect into the web `AllMaps` + `MapGenerator`) | `SPEC_GOLDEN` (lines 27-34) re-baselined in C1; `PORT_CAMP_NPCS` (I7) kept; pass `presentationTheme`, `subDungeonEntrances[].sealed`, corpse seeds [W2], set pieces |
| `world.ts` | npcs.json, dialogue_trees.json, lore.json, achievements.json, random_events.json, zone_moods.json, homestead.json, ... | GameData npcs, dialogueTrees, loreCollectibles, homestead; `GameData/world.ts` overrides | the override hook (§2.4) |
| `story.ts` | story.json | GameData story script | none (moods and timing unchanged) |
| `combat.ts` | elite_affixes.json, status_effects.json, ... | web, plus `GameData/world.ts` overrides (affix display ids, `onDeath` [W2], ashnumb [W2]) | the override hook |
| `ui.ts` | terrain_styles.json, ui_theme.json | web, plus overrides (terrain style `march`, hazard style table) | the override hook |
| `pets.ts` | pets.json | web (the Lanternwisp is a look and i18n change) | none |
| `i18n.ts` | i18n_zh-CN.json, i18n_en.json, i18n_zh-TW.json | web base + PORT_STRINGS + GameData strings (§2.5) | the merge and sweep |
| new `autopilot.ts` | autopilot_routes.json | `GameData/autopilot/ch1.ts` | new table |
| new `setpieces.ts` | set_pieces.json | `GameData/setPieces.ts` | new table |

### 2.4 Overrides for tables embedded in web systems

`ACHIEVEMENTS` (AchievementSystem), `ZONE_EVENT_DATA` (RandomEventSystem), `ZONE_MOODS` / fog (ZonePalette, P14),
`ZONE_WEATHER` (WeatherSystem), `ZONE_THEME_BY_ID` (LightingSystem), `ZONE_AFFIX_COUNTS` and affix names live inside web
system files, which are not redirected (they are code). `GameData/world.ts` exports a typed `REMAKE_OVERRIDES` object;
each exporter table applies `override ?? web` per entry and records what it replaced in a `remake` block next to the
existing `port` block (`{ "remake": { "source": "unreal/GameData/world.ts", "replaced": ["achievements.ach_kill_slime", ...] } }`).
Chapter 1 uses: two achievements re-targeted and renamed, the plains event pools, `themeByZone.emerald_plains = "march"`,
moods `march` / `march_dawn` and the Breach Rim region, fog and weather, light-scale overrides, the camp theme `march`.

### 2.5 i18n split

Merge order for each locale: web table → `PORT_STRINGS` → GameData strings (remake wins). Then:

1. **Orphan sweep.** In the content namespaces (`data.quest.`, `data.questTarget.`, `data.questClue.`, `data.monster.`,
   `data.npc.`, `data.dialogue.`, `data.miniBossDialogue.`, `data.lore.`, `data.storyDeco.`, `data.hiddenArea.`,
   `data.escortNpc.`, `data.defendTarget.`, `data.achievement.`, `story.<cutsceneId>.`, `story.boss.`), drop every key
   whose id is no longer referenced by an exported table (the web's goblin, slime and elder keys disappear; Chapter 2-5
   keys stay because their ids still exist).
2. **Parity.** Every key the exported tables reference exists in zh-CN and en (the exporter fails otherwise; DataStore
   already checks story keys).
3. **zh-TW** converted from zh-CN with the web converter, as today.
4. New namespaces: `data.quest.<id>.progress`, `data.questChoice.<questId>.<slot>`, `data.affix.<id>`.

### 2.6 Schema additions (remake-only, typed in `GameData/types.ts`)

| Type | New fields | Wave |
|---|---|---|
| Monster | `family`, `damageType`, `onHitStatus[]` (authored), `projectileColor`, `abilities[]`, `onDeath`, `aura`, `phases[]`, `kite`, `block`, `morale`, `corpse` (`leaves`, `consumes`), `riseAgain`, `botPriority`, `hitCapHeroMaxHpPct` | W1: family, damageType, onHitStatus, projectileColor; rest W2 |
| Quest | `progressKey`, `rewards.choiceLabels[]`, `defendTarget.waves[]`, `defendTarget.approachDeg`, `setsFlags[]` [W2], `rewards.permanent` [W2], objective `zone` [W3], objective type `clear_area` [W3] | W1 / W2 / W3 |
| Map | `presentationTheme`, `subDungeonEntrances[].sealed`, spawn `mix[]` and `leader` [W2], spawn `variants[]` by flag [W2], `corpseSeeds[]` [W2], `regions[]` (mood) | W1 / W2 |
| NPC | `lines[]` with `when` (flag or quest state) [W2], `visibleWhen` [W2] | W2 |
| Story step | `anim` on `focus` monster steps (play Roar / Taunt) | W1 |

The exporter emits a field only once the core package that parses it has landed (a `REMAKE_SCHEMA` feature list in
`main.ts`), so the core never meets data it cannot read.

### 2.7 Saves

Bump the content version (save v4 → v4 with `contentVersion: 2`). On load, ids that no longer exist in data are dropped
from quests, `storySeen`, `miniBossDialogueSeen` and `dialogueOnce` (QuestSystem::Load already keeps unknown quest ids;
the new rule removes them for the content version), so dev saves lose Chapter 1 progress and keep the hero, items and
gold. No player saves exist yet (U5, I8).

## 3. Core features

Each item: what it does, where it lives, the data, the events, and the tests (doctest, `CoreTests/tests/`, existing suite
names so CMake needs no change; both compilers, ASan, the shared-library build).

### K1 Content hooks [W1] (~2 tasks)

| Hook | Spec | Files | Tests (`content_hooks_test.cpp`) |
|---|---|---|---|
| Defend wave composition | `DefendTargetDef.waves` (per wave a list of `{monsterId, count}`); `SpawnDefendWave(target, wave, spec)` spawns exactly that list (fallback: today's random pick from the zone list); `approachDeg` restricts spawn angles to a 180° arc centred on it | `QuestData.h`, quest loader, `MonsterSystem.{h,cpp}` (`SpawnDefendWave`), `QuestWorld.cpp` (call at :842) | composition honoured per wave; ring positions within the arc; no spawn within the camp safe radius for the M5 geometry; determinism |
| Quest progress line | `QuestDef.progressKey`; `QuestView` carries it; the giver's card shows it while the quest is active | `QuestData.h`, loader, `Snapshot.h` quest view | loader round-trip |
| Labelled reward choices | `QuestRewardDef.choiceLabels` (keys aligned with `choices`); the view carries them | `QuestData.h`, loader, `Rewards` view | loader; length must equal `choices` |
| Monster anim actions | `AnimAction` += `Roar`, `Taunt`, `Stun`, `Attack02`, `Cast02`, `Rise`; the mini-boss plays `Taunt` while its pre-fight dialogue holds it; a story `focus` step with `anim` emits the action on the monster | `SimTypes.h`, `MonsterSystem.cpp` (:186 / :537), `StoryDirector.cpp`, `StoryData.h` | enum strings; Taunt during the hold; story step emits |
| Sealed sub-dungeon entrance | `InteractKind::SubDungeon` handled: a sealed entrance logs its `sealedTextKey` (a toast) and does nothing else | `QuestWorld.cpp` / interaction dispatch, `MapData.h` | interact → log line, no zone change |
| Story-boss config | `monster_ai.json port.storyBoss` already drives M7; verify no Chapter 1 id is hard-coded in the core (grep gate) | — | a test that M7 works with a synthetic boss id |
| Remaining hard-coded content | `MapData.h:223` `sealedGateExitTo`, `:307` `defaultMap` stay (zone ids are kept) | — | — |

### K2 Monster abilities and telegraphs [W2] (~4-5 tasks; activates the inert `bossSkills`, M9)

- **Data.** `abilities[]`: `{id, kind (circle | cone | line | ring | bolt | raise | summon | hazard | buff | call), clip,
  windupMs (≥ 600; contact from the clip's Contact notify when omitted), cooldownMs, initialCooldownMs, rangeMin,
  rangeMax, hpBelowPct, phases[], priority, aim (hero | heroAtWindup | self | corpse | anchor), shape {radius, arcDeg,
  length, width, innerRadius}, dmgMul, damageType, status {type, chance, ms, value}, hitsMonsters, monsterDmgMul,
  hazard {durationMs, tickMs, tickDmgMul, status, shape}, summon {monsterId, count, at, anchors[], capAlive,
  perWindowCap, windowMs, hpPct}, buff {stat, mul, ms, radius, family}}`. `bossSkills` ids map to abilities.
- **Runtime.** `MonsterAbilities.{h,cpp}` (pure: pick the first ready ability by priority whose range / HP / phase
  conditions hold; shape tests) and per-instance cooldowns plus one pending ability on `MonsterInstance`. The pick
  replaces a basic swing; the pending ability resolves at contact through the existing pending-strike path (aborts if the
  monster dies or is stunned, S7). `aim: heroAtWindup` snapshots the target at windup start (slams); cones and lines take
  the facing at windup start. Damage goes through `ApplyMonsterHit` with `damageType` (monster attacks stop being
  physical-only) and the hit cap `min(dmg, heroMaxHp × hitCapHeroMaxHpPct[difficulty])`. Hazards are entries in a
  per-zone `HazardField` ticked on the sim clock.
- **Events.** `EvTelegraph {entity, abilityId, shape, origin, dir, radius, arcDeg, length, width, startMs, contactMs,
  style (normal | elite | boss)}`, `EvHazard {id, shape, untilMs}`, `EvMonsterAnim {entity, action}`.
- **Tests** (`monster_abilities_test.cpp`, suite `monsters`): shape boundaries (inside, edge, outside; hero radius 0.35);
  contact time equals the clip's; priority and cooldown order; snapshot vs facing aim; hit cap per difficulty; abort on
  death and stun; hazard tick count and expiry; damage type reaches resistances; a seeded replay produces an identical
  event log on GCC and Clang.

### K3 On-death effects and the corpse registry [W2] (~3 tasks)

- **On-death.** `onDeath {kind (burst | ring | puddle | spawn), delayMs (≥ 800 for bursts), shape, dmg | hpPct,
  damageType, status, hitsMonsters, monsterDmgMul, capHeroMaxHpPct, spawn {monsterId, count}}`; elite affixes get
  `onDeath` in `elite_affixes.json` (Oathfire, Gravecold); the Labyrinth `volatile` curse maps `deathBurst` .12 onto it.
  Scheduled from the kill pipeline in `SimWiring.cpp` after quest drops and before `MonsterSystem` respawn; resolved on
  the sim clock; emits `EvTelegraph` for the fuse.
- **Corpses.** `Corpses.{h,cpp}`: per zone `{corpseId, defId, family, pos, deathMs, raised, consumed, burned}`, TTL 20 s,
  cap 24 (oldest evicted), cleared on zone change, seeded from map data (`corpseSeeds`, the Sheepfold's six). Queries
  sort by distance, then death time, then id. A fire killing blow or a fire hazard / skill tick over a corpse burns it.
- **Raised.** `MonsterRole::Raised` (no exp, gold or loot; never respawns; `raiserId`; collapses when the raiser dies).
  The `raise` ability picks a valid corpse; the `Unburied` trait / affix re-raise the monster's own corpse once.
- **Consume.** A monster with `corpse.consumes` (gravegnawer) out of combat or below 50 % life channels 2 s on a corpse
  within 1 tile: heals 30 %, removes it.
- **Events.** `EvCorpseAdded`, `EvCorpseRemoved {reason: expired | raised | consumed | burned}`, `EvMonsterRaised`.
- **Tests** (`corpses_test.cpp`, suite `monsters`): TTL and eviction order; raise caps (alive and per window); Raised
  rewards are zero and they never respawn; collapse with the raiser; burn blocks raise; consume heals and removes;
  burst after the fuse hits the hero within radius, capped, and monsters ×2; `volatile` burst; deterministic replay.

### K4 Boss phases and arenas [W2] (~3 tasks)

- **Data.** `phases[] {hpBelowPct, enterAction, speedMul, attackSpeedMul, damageTakenMul, leash {centre, radius},
  abilitiesOn[], abilitiesOff[], summons[], arena {kind: ringShrink, centre, fromR, toR, overMs, dpsPctMaxHp, damageType},
  stinger}`. A one-row table is the zealot's frenzy.
- **Runtime.** Entering a phase happens once, in order, when damage crosses its threshold; multipliers compose with
  affixes; the enter action (Roar) never makes the boss invulnerable; the arena ring damages the hero outside the radius
  every 500 ms; a boss that returns home (M1) heals to full and resets to phase 0 and clears its arena.
- **Events.** `EvMonsterPhase {entity, index}` (boss bar ticks, stinger), `EvArena {centre, radius, targetRadius, endMs}`.
- **Tests** (`phases_test.cpp`): single entry per threshold, including a hit that crosses two; multipliers; abilities
  toggled; arena radius over time and damage outside only; reset on leash return; damage applies during Roar.

### K5 Packs, morale, kite, block [W2] (~3 tasks)

- **Packs.** Spawn `mix[] {monsterId, count}` (sum = the entry's `count`; a validator checks) and `leader`; a pack id per
  instance; a member provoked or entering Chase provokes members within 6 tiles once. Golden-neutral (positions unchanged).
- **Morale.** `morale {fleeOnLeaderDeathMs, sustainFamily, sustainRadius}`; a new `MonsterState::Flee` (enum strings
  updated): move away from the leader's corpse along walkable tiles; a monster with no step that increases the distance
  fights instead. A living `sustain` member (the drummer's `call` buff `fearImmune`) cancels it.
- **Kite.** `kite {min, max}`: a ranged monster closer than `min` steps to a walkable tile near `max` along the away
  vector, bounded by leash; then attacks.
- **Block.** `block {arcDeg, damageMul, wall}`: in the monster damage path, a hit from within the front arc (heading vs
  the direction to the attacker) is multiplied and emits `EvBlock`; `wall` aligns the heading of shieldbearers within 2
  tiles.
- **Social call.** The `call` ability provokes a family within a radius (Hungerbell).
- **Tests** (`packs_test.cpp`): mix counts and leader; pack aggro radius; flee duration, direction and the blocked
  fallback; sustain cancels fear; kite band respects leash and walls; block arc front / side / back; determinism.

### K6 Status, permanent rewards, world flags, NPC lines, spawn variants [W2] (~2-3 tasks)

- `StatusType::Ashnumb`: −10 % healing received per stack (potions, regeneration, life steal), max 5, 20 % slow at 5,
  6 s refresh; the antidote cleanses it. `status_effects.json` row via the override.
- `rewards.permanent {maxLife, statPoints, skillPoints, respecTokens}` granted once per (quest, difficulty); save field
  `permanentRewards`; the hero stat pipeline adds them.
- `SaveData.worldFlags` (a sorted set); `QuestDef.setsFlags` on turn-in; consumers: spawn `variants[] {whenFlag,
  monsterId, count}` (MonsterSystem picks at spawn and respawn), the snapshot exposes flags (UE mood, audio emitters).
- NPC `lines[] {key, when}` and `visibleWhen`; the validator refuses an NPC that would hide while an open quest needs it.
- **Tests** (`world_state_test.cpp`): ashnumb arithmetic and cleanse; permanent rewards once per difficulty and the save
  round-trip; flags save round-trip and an old save loads with none; spawn variant selection; NPC line selection.

### K7 Sub-dungeon travel and the cleanse [W3] (~4 tasks)

- Handle `InteractKind::SubDungeon` (today declared, never handled): enter the sub-dungeon map (a hand-authored map in
  `maps.json` with tiles, so the generator is skipped as for `ember_tower`), remember the parent entrance, fire
  `zone_entered`, no respawn inside, death returns to the parent camp (Combat.cpp:778), the exit tile returns to the
  entrance, saving inside saves at the entrance.
- Objectives get an optional `zone`; the "kill target exists in the quest's zone" rule extends to that zone's
  sub-dungeons; the guide arrow points at the entrance when the tracked objective is inside.
- New objective `clear_area {zoneId, maxRemaining, after}`: completes when the guardian is dead and at most
  `maxRemaining` hostiles remain; emits `EvAreaCleansed`; sets the `rf_pit_cleansed` flag (K6).
- **Tests** (`subdungeon_test.cpp`, suite `world`): enter / exit round trip; no respawn; death return; clear_area
  completion and the flag; save inside; the guide arrow.

## 4. UE presentation

| Package | What | Files (owner) |
|---|---|---|
| **U1** Presentation theme and mood [W1] | `presentationTheme` resolution: decor game id `tree@march` before `tree` in the art manifest; terrain style `march`; camp theme `march`. Moods `march` / `march_dawn` (switch on `chapterCompleteQuest` in W1, on the flag in W2) and the Breach Rim region blend; the `ashfall` ambience; light-scale overrides; wind-up tints from the hazard style table (replacing the hard-coded #FFC4B0 / #FF7A5C at `AbyssCharacterActor.cpp:40-41`); the echo lore look (a translucent figure frozen in its last pose for 3 s); the brightness / gamma setting | `Private/World/*`, `Private/Actors/AbyssCharacterActor.cpp` (tint only), `Private/Platform/AbyssSettings.cpp` |
| **U4** Set-piece layer [W1] | `set_pieces.json` → prop groups placed at tiles at zone build (Knellwatch Name Wall and forge, Ashwheat built from the shipped but never placed goblin-camp meshes re-themed, the burned bridge, the Sheepfold wagons, the Breach Rim dais / circles / banner ring / helmet stakes, the Boneyard Gate, the sealed Pit mouth, the wagon barricade, Crowmarket); non-blocking by default; audio emitters (knocking) | `Private/World/AbyssSetPieces.cpp` (new) |
| **U5** UI [W1 / W2] | quest card progress line and labelled choice cards [W1]; permanent reward line, boss bar phase ticks, gore setting [W2] | `Private/UI/Panels/*`, `Private/UI/Hud/*` |
| **U2** Telegraphs and ability presentation [W2] | a pooled ground-decal actor with `M_AF_Telegraph` (rim + fill progress to contact; crimson rim, bone fill; elite / boss variants); `EvTelegraph`, `EvHazard`, `EvArena`, `EvBlock`, `EvMonsterPhase`; monster `AnimAction` mapping for Roar / Taunt / Stun / Attack02 / Cast02 / Rise (`AbyssCharacterActor.cpp:802-853`); the fuse swell | `Private/Vfx/AbyssTelegraphs.cpp` (new), `Private/Actors/AbyssCharacterActor.cpp` (anim map) |
| **U3** Corpses and ash [W2] | hold the death pose, crumble to an ash husk (dissolve to ash colour + the ash recipe), keep it per `EvCorpseAdded/Removed`, play Rise on `EvMonsterRaised`; the optional gore layer (blood decals, persistent bodies; off by default, compiled out of zh-CN builds) | `Private/Actors/AbyssCorpses.cpp` (new) |
| **U6** Sub-dungeon presentation [W3] | the transition, the Pit's dressing from its map data, the light radius emphasis, the cleansed grade on `EvAreaCleansed` | `Private/World/*` (after U1 / U4 land) |
| **U7** Audio integration [W1] | voice families from data instead of the hard-coded `{Humanoid, Slime}` (`AbyssAudioManifest.cpp:108`); ambience per presentation theme (`:126`); silence regions; bell cues | `Private/Audio/*` |

The content build (`Scripts/build_content.py`, `Scripts/abyss_content/`) creates `M_AF_Telegraph` and the ash-dissolve
material parameters in the same package as U2 / U3 (owner: those packages, `Scripts/abyss_content/*` files they add).

## 5. Art (Blender kit)

| Package | Assets | New / re-themed | Owner files | Est. |
|---|---|---|---|---|
| **A0** Readability tooling | darkest-tile gate (each monster on the darkest `march` tile at the W1 camera: rim ≥ 3 px, lit-side ΔL* ≥ 15, marker ≥ 2 px); hue audit over `vfx_recipes.json` (no hero recipe within 30° of breach-violet at sat > .5; no hero filling ground shape; no enemy ember) | tooling | `coherence/darkgate.py`, `vfx/hue_audit.py` (new) | 1 |
| **A1** Palettes and theme variants | `Monsters_March`, `Props_March`, `Foliage_March` atlases; NPC swatches appended (registries are append-only); manifest `@theme` game-id variants | new atlases | `palettes/*_March.json`, `kit/palette.py` (registration only) | 1 |
| **A2-A8** Monsters | the Chapter 1 roster: Unburied kit and four variants, Living, Gnawers, Breach-blood, Corpseherd, Valdras, elite sigil, family passes (monsters.md §5) | 4 new-kit members, 1 new boss mesh, 6 re-skins / re-dresses | `monsters/<new files>` (no edits to `goblins.py`, `slime.py`, `goblin_shaman.py`: subclass or call them) | ~29 |
| **A9** NPCs | re-dress of the nine looks + `knell_chaplain` and `crow_trader`; kit parts (half-plate and gorget, cassock, handbell, barrow attachment); portraits for the Captain, Aldwin, Marl, Corran, Mag | re-dress | `npcs/looks.py`, `npcs/npc_kit_parts.py` (append), `npcs/npc_dressing.py` | 3 |
| **A10** Environment | march foliage and rocks on the same game ids and footprints (dead oak, charred orchard, briar, ash grass, ash poppy, ash-cap, broken stone, ash boulder); terrain style `march`; camp kit `march` (bell frame in the tent slot, hearth, pew palisade, banner, crates); story decor re-skins; set-piece props (Ashwheat cottages from the goblin-camp kit, the burned bridge, Sheepfold wagons, the dais, ritual circles, banner ring, helmet stakes, the Boneyard Gate, the sealed Pit mouth, the wagon barricade, the order post, the Name Wall); the echo material | re-theme + new props | `environment/*_march.py`, `environment/setpieces_march.py` (new) | 10 |
| **A11** Pickups, icons, pet | quest items `ichor`, `moss` (+ gather node), `tags`; the Lanternwisp re-skin of `pet_sprite` (three stages) | re-skin | `items/`, `icons/`, `pets/sprite_lanternwisp.py` (new) | 2 |
| **A12** VFX | telegraph decal textures (rim, fill), breach fire family, ash crumble, raise marker, bell ripple, violet death bursts, block spark, ashfall particles, the echo ghost; elite affix recolours | new recipes | `vfx/fx_recipes_dark.py` (new, registered from `vfx/fx_recipes.py`) → `Art/Export/VFX/vfx_recipes.json` | 3 |
| **A13** Pit crypt kit [W3] | walls, helmet niches, tag strings, the flame niche, stairs; `sub_pitwarden` | new | `environment/crypt_pit.py`, `monsters/pitwarden.py` | 5 |

Per asset the existing ship pipeline applies (`mship.MonsterShip` / `envkit`): bake, FBX + LOD1, manifest, per-frame QA,
ink gate, re-import check, previews into `Art/Previews/<asset>/`, then the family lineup and coherence sheets.

## 6. AutoPilot and the playthrough

**Route as data.** `GameData/autopilot/ch1.ts` → `Data/autopilot_routes.json`. `AutoPilot.cpp` replaces the hard-coded
Chapter 1 script (`:1870-2003`), the elder path (`:1354`) and the boss detection by id prefix (`:554`) with a route
interpreter whose step kinds map onto the existing task classes:

| Step | Existing task | Example |
|---|---|---|
| `talk {npc}` / `tree {npc, path[]}` / `visit {npc, accept[]}` / `shop {npc, hp, mp}` | talkTo, ApElderTree, visit, shop | `tree quest_elder [reward_ask, help, other_work, accept_ichor]` |
| `gather {quest, obj, count}` | ApGather | `gather q_rf_barrowmoss 0 5` |
| `hunt {monsters[], anchor, until, maxSec}` | hunt | `hunt [risen_footman] (25,20) until questDone(q_rf_burial_detail) && obj(q_rf_black_blood,0) >= 6` |
| `clues {quest, tiles[]}` / `explore {quest, obj, tile}` / `escort {quest}` | clues, explore, ApEscortWithRetries | `escort q_rf_hallowed_ground` |
| `defend {quest}` (new task: stand at the target, fight waves, retreat-heal under 30 % life) | new `ApDefend` | `defend q_rf_last_toll` |
| `boss {role}` | hunt with a role predicate (`storyBoss`, `miniBoss`, `hunt`) | `boss storyBoss` |
| `portalHome`, `townTrip`, `equipUpgrades`, `goTo`, `waitIdle`, `checkpoint`, `smokeEnd` | as today | |

Chapter 1 route outline: camp tour (blacksmith, stash, the Captain's tree, Aldwin: S2 + S5) → barrowmoss → trenches until
M1 and six ichor (via the Seep) → Captain: M1 (`cs_rf_brand`), take M2 + S3 → **smoke ends here** → clues 1-2, Marl
(S1 talk), clue 3, Dreck → gnawers + Hungerbell (M2) → lantern graves (S5) → home: M2, take M3 + S6 → Ashwheat explore,
the Corpseherd (M3), escort Bern (S6) → Corran: S4, Harke → home: M3 (`cs_rf_whisper`), take M4 → Valdras (M4) → home:
M4, take M5 (`cs_rf_siege`) → the Boneyard Gate siege → home: M5 (`cs_rf_finale`) → equip → done. W3 inserts the Pit
after M2.

**W2 bot rules.** Step out of active telegraphs (for each `EvTelegraph` whose shape contains the hero, move to the
nearest walkable tile outside it before contact); target priority from `botPriority` (raisers, standard-bearers,
drummers first); do not stand on the dais in Valdras P2; leave cinderbloats to burst away from the hero.

**Playthrough pins** (`playthrough_test.cpp`): three classes, seeds 101 / 202 / 303; hero level 12-14 at the end of
Chapter 1, embers ≥ 40, kills ≥ 40, levelUps ≥ 11; achievements `ach_first_kill`, `ach_kill_valdras`, `ach_quest_10`,
`ach_level_10`; checkpoints renamed to the route's. A sweep (`ABYSS_PT_SWEEP=1 ABYSS_PT_SWEEP_BASE=<seed>
unreal/CoreTests/run.sh`) over 20 seeds per class must pass before each wave closes.

## 7. Audio

| Package | Work | Owner files |
|---|---|---|
| **AU1** Voices [W1] | families `Undead` (dry rasp, mail, near-words), `Human` (breath, effort, chant fragments), `Gnawer` (the goblin set re-pitched lower, wet chitter), `Breach` (the slime set darkened, sizzle); aggro / hurt / death per family | `Audio/render/abyss_audio/recipes/dark_vox.py` (new), `port_sfx.py` family list |
| **AU2** Ambience and bell [W1] | the march bed (wind, crows, distant bells, silences), the Gnawed Field's night ringing, Ashwheat knocking, the breach hum, bell variants (single toll, cracked note, self-ring, dawn toll), the ambush toll tell | `recipes/dark_ambience.py` (new) |
| **AU3** Music [W1] | a sparse detuned plucked camp piece (our own; never a 12-string town theme), the Red Field drum and wordless choir, `boss_ch1` reworked for Valdras with phase stingers, silence regions | `Audio/render/data/dark_scores.json` (new, loaded beside `port_scores.json`) |

## 8. Tests strategy

- **T0 (before the switch, on today's content).** Move every assertion that names a Chapter 1 id out of `quests_test`,
  `monsters_test`, `world_test`, `combat_test`, `pets_test`, `data_test`, `save_test`, `items_test`, `hero_test`,
  `sim_test`, `base_test` into `content_ch1_test.cpp` (suites `quests` / `monsters` / `world`), and move the combat and
  monster fixtures that only need "a melee monster" or "an elite" to a synthetic builder `TestContent.h`. All green on
  today's data. Then the switch re-pins `content_ch1_test.cpp` and `playthrough_test.cpp` only.
- **Content invariants stay generic** and keep running on real data: one quest-type giver per quest in its zone; kill /
  talk / drop targets exist in the zone; side quests differ in shape; every finale offers a gear choice (re-pointed to
  `q_rf_last_toll`); cutscenes 5-14 steps; zh length limits; speakers stand in the zone.
- **New content invariants** (C1): the 10 spawn entries sum to 55; every remake monster has `family`; every enemy
  projectile colour and emissive passes the hue rule (data-level check of `projectileColor`); every quest has
  `progressKey`; `choiceLabels` length matches `choices`; defend waves reference zone-valid monsters.

## 9. Work packages

Order: **0** foundation → **1** Wave 1 (dark chapter on today's core) → **2** Wave 2 (monster systems) → **3** Wave 3
(the descent) → **4** later chapters. Inside each wave the dependency order is content data → core → art → UE
presentation: content is authored first and fixes the ids and fields (fields the core cannot read yet are withheld from
the export by `REMAKE_SCHEMA`), the core makes them run (the content switch merges once its hooks are in), the art ships
against those ids, and the UE layer presents what the core emits; each falls back to today's assets or behaviour until
its inputs land. Within a phase, packages on different lanes run in parallel; the core lane is
serial because K-packages share `MonsterSystem.cpp`, `MonsterAI.cpp`, `Combat.cpp`, `SimWiring.cpp`, `MonsterData.h`
and the loaders.

### Checks referenced below

| Name | Command (from the repo root) |
|---|---|
| core | `unreal/CoreTests/run.sh` (GCC and Clang) |
| core-asan | `ABYSS_SANITIZE=1 unreal/CoreTests/run.sh gcc` |
| core-shared | `ABYSS_SHARED=1 unreal/CoreTests/run.sh clang` |
| sweep | `ABYSS_PT_SWEEP=1 ABYSS_PT_SWEEP_BASE=<seed> unreal/CoreTests/run.sh` |
| data | `node unreal/Tools/export-data/run.mjs --check` |
| web-untouched | `git diff --exit-code -- src/` |
| scripts | `python3 -m unittest discover -s unreal/Scripts/tests` |
| kit | `EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/tests/smoke_test.py` |
| ship `<g>` | `EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/<g>.py` (previews to `Art/Previews/<asset>/`, reviewed) |
| coherence | `python3 unreal/Art/blender/coherence/checklist.py` + `EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/coherence/lineup.py` |
| audio | `/opt/venvs/audio/bin/python -m pytest -q unreal/Audio/render/tests` + `render_all.py --only <groups>` |
| mac-content | the `ue:content` job: build `AbyssfireEditor`, headless `Scripts/build_content.py` |
| mac-autotest | `-abyssautotest=smoke` and `=chapter` for warrior, mage and rogue (Source/Abyssfire/README.md, *In-game autotest*), exit 0 and `report.json passed` |
| mac-look | on the Mac: the darkest-tile review in game, brightness slider range, telegraphs on every quality tier, mobile preview (per-vertex fog, no CSM on low / mid), ink and rim on the march terrain |

### Phase 0 — foundation

| WP | Scope | Owns | Depends | Acceptance |
|---|---|---|---|---|
| **0A Pipeline** | redirect plugin, `@web` alias, GameData scaffold with byte-identical snapshots, override hook, i18n merge + sweep + parity, `remake` blocks, `REMAKE_SCHEMA`, READMEs | `unreal/Tools/export-data/**`, `unreal/GameData/**` (scaffold), `unreal/Data/README.md` | — | data (zero diff in `unreal/Data`), web-untouched, a deliberate stale redirect fails the export |
| **0B Test isolation** | `content_ch1_test.cpp`, `TestContent.h`, fixtures moved | `unreal/CoreTests/tests/**` | — | core, core-asan, sweep; test count unchanged |
| **0C Readability tooling** | A0 | `Art/blender/coherence/darkgate.py`, `Art/blender/vfx/hue_audit.py` | — | kit; both tools run on today's assets and report (today's goblins pass the gate on grass; the report is the baseline) |
| **0D Decisions** | DECISIONS.md rows DR1-DR11 (from README.md) and amendments to Q4 / M5 / M7 / W7 wording; spec headers point here | `unreal/Docs/DECISIONS.md`, `unreal/Docs/spec/*.md` (headers only) | — | review |

### Phase 1 — Wave 1: the dark chapter on today's core

| WP | Scope | Owns | Depends | Acceptance |
|---|---|---|---|---|
| **1A Core hooks** | K1 | `Source/AbyssCore/**` (files listed in K1), `CoreTests/tests/content_hooks_test.cpp` | 0B | core, core-asan, core-shared |
| **1B Content switch (C1)** | GameData Chapter 1: quests, NPCs, tree, mini-boss, monsters W1 fields, map (the one re-baseline), lore + echoes, story, homestead, world overrides, set pieces, i18n zh-CN + en, config; regenerated `Data/*.json` and goldens; `AutoPilot.cpp` route interpreter + `autopilot/ch1.ts` + `ApDefend`; re-pinned `content_ch1_test.cpp` and `playthrough_test.cpp`; save content version | `unreal/GameData/**`, `unreal/Data/**`, `unreal/CoreTests/golden/**`, `Source/AbyssCore/Private/tools/AutoPilot.cpp`, `Public/abyss/tools/AutoPilot.h`, `CoreTests/tests/content_ch1_test.cpp`, `playthrough_test.cpp`, save load rule | 0A, 0B, 1A | data, web-untouched, core, core-asan, sweep (20 seeds × 3 classes), zh limits, chapter1.md §17 re-pins only |
| **1C UE Wave 1** | U1, U4, U5 (W1 parts), U7 | `Source/Abyssfire/Private/{World,UI,Audio,Platform}/**` as listed, `Private/Actors/AbyssCharacterActor.cpp` (tint lines) | 1B (data), 1D / 1E assets as they land (fallback to plains assets until then) | scripts; mac-content, mac-autotest, mac-look |
| **1D Art Wave 1** | A1, A2-A8 meshes and W1 clips, A9, A10, A11, A12 (W1 recipes), family passes | `unreal/Art/blender/**` (new files), `unreal/Art/Export/**`, `unreal/Art/Previews/**`, palette registries (append) | 0C; ids from monsters.md (no dependency on 1B) | kit, ship per asset, coherence, darkgate on every new monster, hue audit clean |
| **1E Audio Wave 1** | AU1-AU3 | `unreal/Audio/render/**`, `unreal/Audio/Export/**` | — | audio; loudness per A3 |
| **1F Wave 1 close** | the three-class autotest on the Mac, the look review, a dark-tile screenshot set into `Art/Previews/ch1_dark/` | — | 1A-1E | mac-autotest (smoke + chapter × 3 classes), mac-look |

### Phase 2 — Wave 2: the monster systems

| WP | Scope | Owns | Depends | Acceptance |
|---|---|---|---|---|
| **2A** | K2 abilities + telegraphs | core lane + `monster_abilities_test.cpp` | 1F | core, core-asan, core-shared, a replay determinism test |
| **2B** | K3 on-death + corpses | core lane + `corpses_test.cpp` | 2A | same |
| **2C** | K4 phases + arenas | core lane + `phases_test.cpp` | 2B | same |
| **2D** | K5 packs, morale, kite, block | core lane + `packs_test.cpp` | 2C | same |
| **2E** | K6 status, rewards, flags, NPC lines, spawn variants | core lane + `world_state_test.cpp` | 2D | same + save round-trip |
| **2F Content W2 (C2)** | Chapter 1 abilities, phases, mixes, on-death, auras, morale, Ashnumb, permanent rewards, flags, quest-gated lines, champion Muster; AutoPilot W2 rules | `GameData/**`, `Data/**`, `AutoPilot.cpp`, `content_ch1_test.cpp`, `playthrough_test.cpp` | 2A-2E (each lands its schema in `REMAKE_SCHEMA`) | data, core, sweep (fight lengths: Valdras 60-90 s for the bot warrior; no bot death to a single hit) |
| **2G UE Wave 2** | U2, U3, U5 (W2 parts) | `Private/Vfx/AbyssTelegraphs.cpp`, `Private/Actors/AbyssCorpses.cpp`, anim map in `AbyssCharacterActor.cpp`, UI files, `Scripts/abyss_content/*` (telegraph material) | 2A-2E events | scripts, mac-content, mac-autotest, mac-look (telegraphs read on the darkest tile, mobile tiers) |
| **2H Art Wave 2** | Rise / Attack02 / Cast01 clips where W1 shipped without them, elite sigil, A12 W2 recipes | Art files of 1D (re-ship) | 1D | kit, ship, coherence, hue audit |

### Phase 3 — Wave 3: the descent

| WP | Scope | Owns | Depends | Acceptance |
|---|---|---|---|---|
| **3A** | K7 sub-dungeon travel + `clear_area` | core lane + `subdungeon_test.cpp` | 2E | core, core-asan |
| **3B Content W3 (C3)** | `sub_rf_pit` map, S7, `cs_rf_pit`, `sub_pitwarden`, the Pit route step; the entrance unsealed | `GameData/**`, `Data/**`, `AutoPilot.cpp`, content tests | 3A | data, core, sweep (exp totals re-tuned for +300) |
| **3C Art W3** | A13 | `environment/crypt_pit.py`, `monsters/pitwarden.py` | 1D | kit, ship, coherence |
| **3D UE W3** | U6 | `Private/World/**` | 3A, 3B | mac-content, mac-autotest (chapter route includes the Pit), mac-look |

### Phase 4 — later chapters

One content + art + audio pass per chapter when it is built in 3D (story-bible.md §7, monsters.md §6), each starting
with its own `content_ch<N>_test.cpp`, the first member of its new rig family, and the W7 sealed gate opening only after
Chapter 2's dark pass ships. The Labyrinth's curses re-voiced as oaths need the Labyrinth runtime first.

### Size

| Lane | Tasks |
|---|---|
| Pipeline and content (0A, C1, C2, C3) | ~9 |
| Core (K1-K7) | ~21 |
| Tests and bot (0B, AutoPilot, re-pins) | ~6 |
| UE (U1-U7) | ~10 |
| Art (A0-A13) | ~55 |
| Audio (AU1-AU3) | ~4 |
| **Total** | **~105** (Wave 1 ≈ 55, Wave 2 ≈ 30, Wave 3 ≈ 15, plus the Phase 0 tasks) |

## 10. Risks

| Risk | Mitigation |
|---|---|
| Too dark to read | the darkest-tile gate (A0) on every monster; grade the world, not the actors; ink + rim + markers; the brightness setting; telegraphs in a hue no hero effect uses (hue audit) |
| Determinism (raises, corpse TTLs, fuses, phases, flee) | all timers on the sim clock, stable sort keys (distance, death time, id), RNG streams per domain; a replay test per K-package on both compilers; the 20-seed sweep |
| The bot cannot finish the harder chapter | W1 numbers keep today's bands; the bot's W2 rules land in the same package as the content that needs them (2F); fight-length targets in the acceptance |
| Redirect fragility | a stale or unused redirect fails the export; GameData may not import redirected web paths; `data:check` and `web-untouched` in CI |
| IP proximity (Valdras near a corrupted king, the raiser near a shaman package, the church near a corrupted faith) | the avoid list in the story bible; review of names and silhouettes per asset (no crown, throne, crypt or skull face; human raiser with a bell crook; no imps) |
| Sensitive material (holy war, executions, hanging in Ch2, corpses) | aftermath only; doors barred from inside; ash mode default; fictional church iconography; a zh-CN sensitivity read of all Chapter 1 text before C1 merges |
| Mainland-China content rules (blood, corpses, skeletons) | ash mode is the default everywhere; the gore layer is compiled out of zh-CN builds; no exposed bone in Chapter 1; recheck the current rules before release |
| Scope (~105 tasks) | Wave 1 alone ships a dark, playable chapter; cut order inside waves: stretch monsters, S8, champion packs, kite, social call, then the Pit (never the telegraphs, raiser or boss phases) |
| Tone seam with Chapters 2-5 (still web data) | the east gate stays sealed (W7); Chapters 2-5 text stays out of the 3D build until their pass |
