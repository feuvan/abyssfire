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
3. **zh-TW** converted from zh-CN with the web converter, as today; the remake's own keys (GameData) then go through the
   supplement `GameData/i18n/zhTWExtra.ts` (context phrases, then the characters the web map lacks; final review R4),
   and web-sourced keys keep the web output byte for byte.
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
  today's data. Then the switch re-pins `content_ch1_test.cpp` and `playthrough_test.cpp` only (with the `ch1` handles
  in `TestContent.h`); systems rigs that stand on the live Chapter 1 map are re-run after the DR4 re-baseline and may
  move to another open tile (see *Phase 0 outcomes* below).
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

**Phase 0 outcomes (review, 2026-10; pending confirmation).**
- **0A**: on top of the table, every export type-checks GameData first, fails on a GameData module it never loads, on
  a record key that is neither a web nor a registered remake field, and on unnormalised redirect paths; remade modules
  are found by the snapshot digests in `redirects.json`, never by comparing with the live web file.
- **0B**: "test count unchanged" became "no test and no suite loses assertions": splitting mixed tests into a systems
  part and a `content_ch1_test.cpp` part raised the case count from 636 to 653 (GCC 205,555 → 205,696 assertions). The
  moved tests keep their original suites (`monsters`, `quests`, `story`, `world`, `pets`, `items`, `sim`, `data`), so
  `ctest -R core.<area>` still runs them. Several systems rigs stand on the live map (world_test's `WorldRig`, the
  monsters / pets zone rigs, combat_test's open tile, positions relative to `ch1::kExit` / `kCamp`): 1B re-runs the
  whole suite after the switch and moves a rig whose tile the re-baselined decor covers.
- **0C**: both tools run and report, but the expectation "today's goblins pass the gate on grass" does not hold: on
  `plains:grass` Gruk Redcap and the goblin shaman fail and the four passes sit within 1 ΔL* of the threshold. The
  baseline stands as a baseline; the gate's metric definitions (DECISIONS R21) and the hue audit's two relaxations
  (R22) await confirmation.

### Phase 1 — Wave 1: the dark chapter on today's core

| WP | Scope | Owns | Depends | Acceptance |
|---|---|---|---|---|
| **1A Core hooks** | K1 | `Source/AbyssCore/**` (files listed in K1), `CoreTests/tests/content_hooks_test.cpp` | 0B | core, core-asan, core-shared |
| **1B Content switch (C1)** | GameData Chapter 1: quests, NPCs, tree, mini-boss, monsters W1 fields, map (the one re-baseline), lore + echoes, story, homestead, world overrides, set pieces, i18n zh-CN + en, config; regenerated `Data/*.json` and goldens; `AutoPilot.cpp` route interpreter + `autopilot/ch1.ts` + `ApDefend`; re-pinned `content_ch1_test.cpp` and `playthrough_test.cpp`; save content version | `unreal/GameData/**`, `unreal/Data/**`, `unreal/CoreTests/golden/**`, `Source/AbyssCore/Private/tools/AutoPilot.cpp`, `Public/abyss/tools/AutoPilot.h`, `CoreTests/tests/content_ch1_test.cpp`, `playthrough_test.cpp`, save load rule | 0A, 0B, 1A | data, web-untouched, core, core-asan, sweep (20 seeds × 3 classes), zh limits, chapter1.md §17 re-pins only |
| **1C UE Wave 1** | U1, U4, U5 (W1 parts), U7 | `Source/Abyssfire/Private/{World,UI,Audio,Platform}/**` as listed, `Private/Actors/AbyssCharacterActor.cpp` (tint lines) | 1B (data), 1D / 1E assets as they land (fallback to plains assets until then) | scripts; mac-content, mac-autotest, mac-look |
| **1D Art Wave 1** | A1, A2-A8 meshes and W1 clips, A9, A10, A11, A12 (W1 recipes), family passes | `unreal/Art/blender/**` (new files), `unreal/Art/Export/**`, `unreal/Art/Previews/**`, palette registries (append) | 0C; ids from monsters.md (no dependency on 1B) | kit, ship per asset, coherence, darkgate on every new monster, hue audit clean |
| **1E Audio Wave 1** | AU1-AU3 | `unreal/Audio/render/**`, `unreal/Audio/Export/**` | — | audio; loudness per A3 |
| **1F Wave 1 close** | the three-class autotest on the Mac, the look review, a dark-tile screenshot set into `Art/Previews/ch1_dark/` | — | 1A-1E | mac-autotest (smoke + chapter × 3 classes), mac-look |

**1A outcomes (K1 as built, 2026-10; pending review; DECISIONS DR8).**
- **Defend waves.** `DefendTargetDef.waves` (exactly `totalWaves` compositions, counts ≥ 1, unknown monsters fail
  `Finalize`) → `SpawnDefendWave(target, wave, DefendWaveSpec)` spawns the list in order without an RNG draw (the
  existing defend scaling, chase, role and edge clamp apply; no walkability check, as before). `approachDeg` is in tile
  space (0 = +col, 90 = +row; M5's gate away from camp 1 is `atan2(gate − camp)` ≈ 106°) and puts monster k of n at
  `approachDeg + 180 ((k + 0.5) / n − 0.5)` on the r 8 ring, so every spawn is strictly inside the half ring. Without
  `waves` the web rule (3 + w random picks) still runs, on the arc when `approachDeg` is set.
- **Progress line.** `QuestCardState::progress` lists the NPC's active quests that have a `progressKey`; the quest card
  also opens for these lines alone (the giver's tree stays reachable through the card's *View Story*); without a key
  the giver keeps today's fallback. U5 renders the line.
- **Labelled choices.** `GenerateQuestRewardChoices` reports each item's slot, so `QuestCardEntry::choiceLabels`
  follows the generated items (a slot that yields nothing drops its label); accept cards read
  `rewards.choiceLabels` slot by slot.
- **Anim actions.** Appended to `AnimAction` (spellings `roar`, `taunt`, `stun`, `attack02`, `cast02`, `rise`). The
  mini-boss emits `EvPlayAnim{Taunt}` (facing the hero) when its dialogue holds it and `MonsterView::holdAction` is
  Taunt until the dismiss. A focus step's `anim` is allowed only on a `{monster}` focus step and never idle / walk /
  death; it plays on the instance the step focused, right after its `EvStoryStep`. The UE clip map is U2's.
- **Sealed entrance.** Only sealed entrances are interactive in Wave 1 (interact order 4 and press row 7 of
  world-map-nav.md §7, `distSq ≤ 9`, walk-then-act): toast + system log of `sealedTextKey`, nothing else. A sealed
  entrance needs its key and may name a sub-dungeon that Wave 3 adds (`sub_rf_pit`).
- **Story boss / grep gate.** `content_hooks_test.cpp` runs M7 on a synthetic boss id (role, no respawn, not after its
  quest, farmable after the chapter) and plays its intro's Roar; the grep gate scans the string literals of
  `Source/AbyssCore` for the Chapter 1 ids the data names (quests, targets, monsters, hunts, beats, lore, areas,
  achievements; NPC and zone ids are role keys) with `Private/tools/AutoPilot.cpp` exempt until 1B's route interpreter
  (the exemption fails once it no longer hides a hit).
- **Exporter.** `REMAKE_SCHEMA` lists `quest.progressKey`, `quest.choiceLabels`, `quest.defendWaves`,
  `quest.defendApproach`, `map.sealedEntrances` and `story.focusAnim`, with the loader's shape rules checked at export;
  the export stays byte-identical while no record uses them. Monster `family` / `damageType` and map
  `presentationTheme` / `regions` stay withheld until a reader lands (U1 / U7).

**1B part 1 outcomes (Chapter 1 world data, 2026-10; pending review; DECISIONS DR3 / DR4 / DR5 *as built*).**
- **Split.** 1B runs in three parts on the same lane: part 1 the world data (this note), part 2 the quests, NPC lines,
  the Captain's tree and the story script (ids and text), part 3 the AutoPilot route interpreter and the playthrough
  re-pins. Part 1 changed quest slots only where they point into the world (kill / drop targets, hunts, M5's defend
  target), keeping their web ids, so the data validates between the parts.
- **Re-baseline (DR4).** Done here, once: it needs M5's defend target and the echo lore spots, which live outside the
  map. Golden `54ce0471e96c5d0e` / `7a501f14066cbb08` / 13169 walkable, 30 avoid points; RNG checkpoints unchanged.
  Part 2 must keep every generator input of the slots (quest locations, gather areas, clues, escort ends, M5's gate).
- **Roster.** monsters.md §2.0 numbers as written (Valdras at the Wave 1 ~900 HP); `spriteKey` = the art lane's plain
  id `monster_<id>`, so the core times swings by the rig fallback until each asset ships. `family` is authored and still
  withheld (no core reader). `deserter_drummer` is in the zone list without a Wave 1 spawn. Exp per full clear rises
  from 913 to 1447 (Valdras alone 220): the playthrough bands (DR9) are re-checked by part 3.
- **World overrides.** `MapTheme` gains the mood themes `march`, `march_dawn`, `breach_rim` (no generator config uses
  them); `zone_moods.json` gains `port.lightScale` (R18 override) and `port.moodWeather` (two new exporter-owned
  override tables), parsed into `ZoneMoodDef::lightScale` / `moodWeather`; the map's `presentationTheme` and `regions`
  are written (`REMAKE_SCHEMA` `map.presentationTheme`, `map.regions`; `MapDef::presentationTheme` / `regions`,
  DataStore checks that each names a defined mood). `ember_tower` keeps plains and the pollen it used to borrow;
  plains-themed places without weather borrow the tower's. Terrain style and camp kit `march` are data for U1.
- **Not done here.** Set pieces (`setPieces.ts`), the gate's art id (`decor_defend_boneyard_gate` falls back), the
  ambush toll, per-NPC quest lists, the content-version save rule: parts 2 / 3 and U1 / U4.
- **Systems tests touched (disclosed by the 1B review; none lost an assertion).** `monsters_test.cpp` "derived data 1.1"
  skips the Chapter 1 roster from the web keyword rule (the remake authors its statuses and bolt colours, pinned in
  `content_ch1_test.cpp`), and "elite affix definitions 17.1" replaced the pinned FireEnhanced `vfxColor 0xff4400` with the
  DR5 rule (no affix glow in the ember family); the remake's colours are pinned in `content_ch1_test.cpp`.

**1B part 2 outcomes (Chapter 1 narrative, 2026-10; pending review; DECISIONS DR3 *narrative as built*).**
- **Quests.** The eleven quests carry their `q_rf_*` ids in their web slots with every generator input kept (the
  golden maps are byte-identical to part 1's). Rewards follow §5-7: 2740 exp / 510 gold; embers stay 20 (M5's 5 and
  S5's 2 replace the per-quest defaults, as today; chapter1.md §7 adds them on top and counts 23). M3 gained its second
  objective (kill `miniboss_corpseherd`). Every Chapter 1 quest has a `progressKey`; S3 / S4 label their choices. Givers:
  the Captain 6, Aldwin 4, Corran 1 (§7). S1's turn-in line is Marl's (§6) though the Captain's card shows it.
- **Words.** `GameData/i18n/ch1Quests.ts` (names, quest-log lines, offers, progress and turn-in lines, targets, clue
  notes, choice labels, the escort and the gate), `ch1Npcs.ts` (the re-cast NPCs, the quest-gated lines for K6, the
  Captain's tree, the Lanternwisp) and `ch1Story.ts` (prologue, chapter card, the six cutscenes). chapter1.md gives no
  quest-log lines, no tag-thief / lantern clue names, no Lanternwisp clue notes, no reward-ask choice texts and no
  Five Handbells reward line (the web's "获得了经验和金币!" went); this package wrote them in the §11 voice. All zh lines are within the §11 limits (pinned in `content_ch1_test.cpp`).
- **Exporter.** The orphan sweep (§2.5) also covers `story.prologue/epilogue/credits.<n>.<field>` (the web prologue's
  heading lines go) and NPC lines past the end of an NPC's exported `dialogue` (the web's extra lines of the re-cast
  NPCs go).
- **Tests.** `content_ch1_test.cpp` / `TestContent.h` re-pinned (handles, `kPinnedIds`, quest / tree / beat pins, the
  `q_rf_barrowmoss:0` gather vector) plus a narrative pin test. Three systems-test changes, none weakening: the
  cutscene-zone rule counts a zone's mini-boss as standing in it (MonstersIn's rule; `cs_rf_herd` plays on her
  death), the progress-line fallback test runs on a store without `progressKey` (the remade content authors one
  everywhere), and the K1 grep gate's AutoPilot exemption is gone (the hard-coded web route spells no live id any more;
  part 3's route interpreter must keep the gate clean without one). `core.playthrough` fails until part 3: the bot's
  elder-tree walk looks for `other_quests`.
- **Not done here.** The AutoPilot route, `ApDefend` and the playthrough re-pins (part 3); the content-version save rule;
  set pieces (U4); the tower's `tower_elder` and `cs_tower_home` still speak as the web elder (hidden in milestone 1, Q3).

**1B part 3 outcomes (AutoPilot, re-pins, saves, 2026-10; pending review; DECISIONS DR3 *route and saves as built*,
DR9 *as built*, M7 amendment).**
- **Route as data.** `GameData/autopilot/ch1.ts` (types in `autopilot/types.ts`) → `Data/autopilot_routes.json`
  (`src/tables/autopilot.ts` checks every id, tile, clue / explore circle and that each zone quest is accepted by a
  step; `LoadAutoPilot.cpp` + `DataStore::Finalize` check shape and ids again; `stepKinds` must equal the core's
  `AutoPilotStepKind`). `AutoPilot.cpp` builds its script from the starting zone's route (`BuildScenario`): the step
  kinds of §6 plus `note`, `closePanels`, `townTrip`; `boss` hunts by `MonsterRole` (and the kill milestone uses the
  role, not an id prefix); `tree` walks any NPC's tree by node path (`ApDialogueTree`); `ApDefend` stands `standOff`
  tiles out along the gate's `approachDeg`, kills the wave nearest the gate first, retreats to a camp under 30 % life
  without potions, and is retried from the giver like the escort. Smoke stops at `smokeEnd`. The K1 grep gate passes
  without an exemption.
- **Route deviations from §6's outline.** S3 is Aldwin's (taken with M1's turn-in), S2's turn-in is his too. Before each
  portal home the route steps onto Knellwatch's side of the line between the two camps (the portal takes the nearest
  camp; walking back from the Crowmarket through the Lv 6-7 far half put every class at Lv 15, DR9). The escort starts
  where Old Bern already follows the hero (he joins on the way to Ashwheat), not back at his start tile.
- **A soft-lock fixed in the core.** M3 needs the Corpseherd, whose Sheepfold (60,55) is 7 tiles from Dreck (60,48): the
  mage's Dreck fight killed her before M3 existed, and once per visit (M7) kept her dead in a zone milestone 1 cannot
  leave. A zone mini-boss an open quest still needs now comes back on that quest's accept (M7 amendment).
- **Long authored swings.** The art lane's Unburied clips put Contact past the humanoid preset's 500 ms swing (footman
  600 ms in 1080 ms, monsters.md 1.3 / 2.1). `ComputeAttackTiming` keeps the preset swing unless the shipped clip's
  Contact lies at or past it, then uses the clip's length (no web-timed clip changes; the sim strike is unchanged).
- **Saves.** `contentVersion` 2 (§2.7) with the rule extended to every id list a save carries (DR3).
- **Re-pins.** `playthrough_test.cpp` follows the route's checkpoints and the chapter's totals (2740 quest exp, nine
  beats, five named kills, `ach_kill_valdras`). **The level band moves to 13-15** (DR9 *as built*, pending review): the
  remade quests need ~1700 exp of kills (the web's: ~590), so Lv 13 is reached with no extra fight and the bot ends at
  14-15 on every seed; embers ≥ 40 and kills ≥ 40 hold, level-ups ≥ 12. Keeping 12-14 instead needs ~650 exp less per
  chapter (quest rewards or hunt / wave / boss exp), a content decision left to review.
  `content_ch1_test.cpp` gains the Corpseherd test; `save_test.cpp` the content-version tests and the trailing key;
  `combat_test.cpp` the long-swing test.

**1B review fixes (2026-10; pending review; DECISIONS DR3 / DR5 / DR8 / DR9 / M7 notes, chapter1.md §19).**
- **Balance (DR9).** The band stays 12-14 (level-ups >= 11): Chapter 1 regulars give round(0.75 × monsters.md §2.0 Exp),
  Valdras 110, the Corpseherd 95; Hungerbell is hp ×2 / dmg ×1.0. 195 bot runs per compiler (sweeps from seeds 1000 /
  2000 / 5000 plus the pinned runs): 4319-5108 exp, Lv 13-14, no death. Re-measured after the final review fixes (609
  GCC runs: sweeps from seeds 1000-10000 step 1000, 20 per class, plus seeds 7013 / 1023 / 8018 alone; the art lane's
  manifest of 2026-10-19): 4319-5131 exp, Lv 13 (405) / 14 (204), no death, no failure; on the committed art manifest
  (1200 GCC runs, seeds 1000-20000): 4338-5329 exp, Lv 13 (793) / 14 (407), no death, no failure (DECISIONS DR9).
  Per-seed results move with the art manifest (decor footprints drive map blocking); the 12-14 band is what is pinned.
- **Core.** `SpawnStoryBossForQuests` (M7 for the story boss); story triggers `whileQuest` (schema `story.triggerWhileQuest`;
  `cs_rf_herd` / `cs_rf_marshal_falls` play on the kill that counts); the long-swing rule is continuous (the preset's
  contact share, capped by the clip); saves: story.json `contentUpgrade` replays the remade prologue and chapter card for
  older saves, and a hero saved in a locked tower loads at its return zone's camp; random events log a zone's own line
  (`<key>.<zone>`) and `rescueMonsters` picks the rescue's guards; AutoPilot fails fast (a defend / escort quest that was
  never accepted, a `visit` accept the card never offered) and has `AddDefend` for tests; a defend / escort quest that
  already failed when its step starts is a lost attempt, re-taken from its giver (final review fixes, below).
- **Tests.** `content_hooks_test` (story boss respawn on a synthetic id, through the sim; `whileQuest` load / refuse; the
  grep gate's id families: entrances, sub-dungeons, regions, compound dialogue nodes), `save_test` (a fixture with the web
  port's real Chapter 1 ids; the locked tower), `playthrough_test` (a forced gate loss re-taken and held; a defend step
  for a quest that is not open fails at once; retry counters bounded), `combat_test` (the continuous swing; REQUIRE instead
  of a skip guard), `content_ch1_test` (re-pinned exp / Hungerbell / affix names / 23 embers; the shipped M5 waves spawn on
  walkable tiles outside camp 1; the Red Field's event lines and zealot guards; no web plains name or never-used word in a
  string Chapter 1 shows), `world_test` (the event-log checks read the zone's own key when it has one).
- **Content.** Jukebox rows, the re-voiced achievements, the epilogue's Red Field slide, S1's and S5's turn-in lines, the
  echo and the Belfry, zh / en wording (渊血样本, 凝块, "Only in the earth by nightfall", "is warm", "The fire here", the
  Breach capitalised per the glossary), the Corpseherd's family `unburied`, the parked lines (Old Bern, hub barks).
- **Not done here.** The tower's `tower_elder` / `cs_tower_home` (tower milestone, Q3); per-zone event labels over props
  (UE world layer reads `zone.event.*.label`; U4); the handbell puzzle prop and the pilgrim's look (art lane ids); mood
  themes as data instead of `MapTheme` values (U1 lookup); 「那里的火，不要人偿。」 (story owner).

**Sweep fixes (2026-10; DECISIONS Q9).** Two bot stops, each only with one manifest's clip timings (the art lane's
manifest moves hit timing, so the seeds differ by manifest):
- **Escort freeze (mage 7013, HEAD manifest) - a game rule gap.** Old Bern, between tiles at (44.38, 59.78), repathed from
  (44,60) round the map generator's scatter wall (44,61) by (43,60), (43,61); the web follow rule dropped the path's first
  step, walked straight at (43,61), cut the wall's corner, and the next repath started on the wall: `findPath` refuses an
  unwalkable start, so the path stayed empty and he never moved while the hero waited 4.97 tiles away (600 s bot
  timeout). The wall predates the remake (the golden map has it), and no set piece or decor blocker is involved. Fixed in
  `QuestWorld::TickEscort` (Q9: keep the first step; catch up into the hero's component; catch up after 2 s without a
  path). Tests: `content_ch1_test` "3.9 escort (Q9)" x3 (the 7013 geometry, the frozen state, a sealed ring pocket),
  plus the catch-up edge test of the final review fixes.
- **Potion detour timeout (mage 8018, art-lane manifest) - a bot mistake.** `ApLootNearby` walked to a potion with a
  15 s `ApGoTo` that fights on the way; the fights (the last gravegnawers and the Hungerbell) ate the 15 s and the
  timeout failed the whole script, though the hunt it served was already done. The potion walk is now optional
  (`ApGoTo` `required = false`: a timeout or a stuck walk notes "gave up" and the potion is skipped). Test:
  `playthrough_test` "autopilot loot: a potion walk that runs out of time is skipped" (`AutoPilot::AddLootNearby`).
- **Seed pins.** "sweep regression seeds" plays mage 7013 (the escort freeze with the committed manifest), mage 1023
  (the same freeze with the art lane's manifest of 2026-10-19) and mage 8018 (the potion detour, art-lane manifest).
  Which seed meets a given geometry drifts whenever the art manifest changes (clip timings move hit timing), so these
  pins are smoke, not the guard: the Q9 unit tests (the three above and the catch-up edge test) and the potion-walk test
  pin the rules themselves.

**Final review fixes (2026-10-19; DECISIONS M7 / Q9 / DR9 notes, chapter1.md §19).**
- **AutoPilot (R1).** `ApEscort::Start` / `ApDefend::Start` treat a quest that is already `Failed` when the step starts
  (the escort joined earlier and died, the gate fell on an earlier walk) as a lost attempt (`escortFailures` /
  `defendFailures` + 1, the `*WithRetries` wrapper re-takes it from the giver); only a quest that was never accepted
  fails the step. `AutoPilot::AddEscort` joins `AddDefend` for tests. Tests (`playthrough_test`): S6 failed at the
  checkpoint before its step, re-taken and completed; M5's gate brought down before its step, re-taken and held; an
  escort step for a quest never accepted fails at once.
- **Story boss (R2).** `SpawnStoryBossForQuests` leaves a dead boss whose respawn timer is pending to that respawn
  (`port.storyBoss.oncePerVisit` false makes him a Regular): no second boss on the quest's accept. Test
  (`content_hooks_test`): the flag flipped, kill, accept, past `respawnDelayMs` -> exactly one alive.
- **Escort catch-up (Q9).** No catch-up tile when the hero's tile is walkable but no ring tile shares its component (it
  used to fall back to `findWalkableNear` and re-teleport the escort into another component every 2 s), and no
  teleport whose from == to. Test: `content_ch1_test` "3.9 escort (Q9): no catch-up into another component ...".
- **Content.** The abyss_rift jukebox row is 渊火 · 低语 / Abyssfire · The Whisper (the title-menu track keeps 渊火 · 序章);
  `content_ch1_test` checks every jukebox titleKey (Audio/Export/audio_manifest.json) resolves to a distinct title in
  zh-CN, zh-TW and en. The Red Field's rescue line no longer splices `{npcName}` ("You cut the pilgrim loose.");
  the Relic Peddler lays out 一盘圣骨 / a tray of relics, and the Chapter 1 words test bans 白骨 / 骷髅 / 头骨 / 颅骨 and
  skeleton / skull / white bones.
- **zh-TW (R4).** `GameData/i18n/zhTWExtra.ts` (context phrases + the characters the web map lacks) converts the
  remake's 315 strings (131 changed); web-sourced zh-TW stays byte-identical. Test: `content_ch1_test` "Chapter 1
  zh-TW" against `CoreTests/golden/zh_simplified_only.txt` (2657 characters, OpenCC-derived,
  `Tools/export-data/scripts/zh_simplified_only.py`); the shared UI (`ui.*`) stays the web converter's.
- **Seeds and balance.** Mage 1023 pinned beside 7013 and 8018 (above); DR9 re-measured: 4319-5131 exp on the art lane's manifest of that day, 4338-5329 on the committed one, Lv 13-14
  on both.

**1C U1 outcomes (presentation theme, mood, W1 anim map; 2026-10, pending review; DECISIONS DR12).**
- **Test target.** `unreal/PresentationTests/` (1C-0): the UE-free helpers `Source/Abyssfire/Private/**/Abyss*Rules.h`
  built with the core's flags, GCC + Clang, doctest over `tests/*.cpp`, a macro shim (UE / Windows / Apple macros) over
  every helper. U1's helpers: `World/AbyssThemeRules.h` (themed ids, terrain theme, moods, ambience, light scale, wind-up
  tints, echo look, brightness) and `Actors/AbyssAnimActionRules.h` (action kinds, clips, holds, layer priority); tests
  `theme_test.cpp` (real `Data/*.json` + the art manifest) and `anim_test.cpp`.
- **Theme.** Every UE art chain in a zone with a presentation theme tries all its `@march` ids first (zone dressing,
  camp kit, story decor, sealed exit, NPCs, props, elite crown, zone loops, portraits); heroes, the soul echo and loot
  never theme. Terrain style and `MI_AF_Terrain_March` follow the presentation theme. The manifest load warns when a
  variant's footprint / blocking differs (none today; pinned in `theme_test`). The late entity-prop warm loads only the
  variants the zone resolves.
- **Mood.** `AAbyssZoneActor::UpdateMood` every frame: base mood, `march_dawn` after `q_rf_last_toll`, the Breach Rim
  blend (4-tile smoothstep, ambience hysteresis .5 / .4, re-applied past .01), the dominant mood's weather (march
  ashfall #CFCAC0, dawn dust motes, the Rim's violet sparks), the blended `port.lightScale` pushed to the VFX system.
  Web zones: mood, ambience and light scale identical to before (pinned per zone in `theme_test`).
- **Data.** `zone_moods.json port.windupTint` (new exporter override table `zoneMoods.windupTint`, `GameData/world.ts`):
  the march moods' crimson #D11F2E, elites #FF3A48; `ui.settings.brightness` (PORT_STRINGS; 亮度 / Brightness, glyphs
  already in the subsets). The art lane's water handoff is applied: `terrain_styles.json themes.march.ground.3` base
  #262A30, patches #22262B / #2C3036, shallow #30343A, foam #4E5258 (was the tar-black #121418), so the runtime water
  under the now-live march terrain style matches the painted `T_Water_March_BC`.
- **Anim map (W1 part of U2's map).** `AnimAction` Roar / Taunt / Stun / Attack02 / Cast02 / Rise → the clip of the same
  name; the mini-boss Taunt hold and the Stun status loop their clip; the boss intro's Roar plays through the frozen world.
- **Echo, brightness.** As DR12 (6) / (7); `FAbyssUserSettings::Brightness` round-trips (mock-compiled `AbyssSettings.cpp`).
- **For the Mac.** Not compiled here (no UE): the look of the march mood / dawn / Rim blend, the crimson wind-up at 0.85,
  the echo figures' placement against the set pieces (the figures sit at the lore tile + A10's offsets: the dais proposal
  (15,95) is 5.8 tiles from `lore_rf_echo_03` (20,92), so U4 should stand the dais at the echo or the echo moves), and
  the brightness range (mac-look).
- **Known gap (1C review).** The `MI_AF_Echo` contract's `poseOverride` (`lore_rf_echo_03`: arms behind the back, wrists
  crossed) is not read by `ParseEchoLook` (asset, clip, clipAsset, frame, offsetM, yawDeg only): the figure kneels with
  its arms in the Death clip's pose until A9's `Kneel_Bound` clip (the contract's `followUp`) replaces the clip.

**1C U7 outcomes (audio integration; 2026-10, pending review; DECISIONS DR13).**
- **Helpers.** `Source/Abyssfire/Private/Audio/`: `AbyssAudioDataRules.h` (manifest JSON, the region rule with
  hysteresis, `while`, weight ramps), `AbyssVoiceRules.h`, `AbyssMusicRules.h` (tables, request classes, region / siege
  picks, the arbiter), `AbyssAmbienceRules.h`, `AbyssCueTriggerRules.h` (matching, bursts, `every` timers); tests
  `PresentationTests/tests/audio_{voice,music,ambience,triggers}_test.cpp` on the committed audio manifest and the real
  `Data/` (story steps, the emerald_plains map and its camp slots).
- **Voices.** Family = `voiceFamilies.monsters[defId]` (hunt leaders report their hunt id) → design family via
  `aliases` (monster `family` stays withheld: the step is tested, inert in W1) → animCategory (later chapters unchanged);
  `silent` → no family cue. Named cues: the AU1 replacements plus `anim.<action>` (Valdras roar on the intro's focus
  step, plate on attacks; the Corpseherd's handbell on attacks / casts), `miniboss.opened` (the lullaby), `story.say`
  (Valdras's intro lines) — keys documented in `dark_vox.OVERRIDE_KEYS` / manifest `voiceFamilies.overrideKeys`.
- **Music.** EvMusic is arbitrated per frame against `musicDirector` (story > victory > boss > siege > region > zone).
  The held combat override is released (`dark_scores.json` `heldOverrides` deleted, manifest rebuilt with
  `--manifest-only`): a zone-overridden request resolving to the sounding track is a no-op even with restart. The
  director's story rows got structured fields: siege `quest` (+ fades), finale `stepIndex` 0. Layers freeze during a
  cutscene / boss intro. Encounter stingers on their own component and duck; the slain stinger comes from
  `EvSfx{monster_death}` (EvEntityDespawned{Died} is only emitted at the respawn).
- **Ambience.** Bed by presentation theme; the Breach Rim bed crossfades with the bed; the Gnawed Field ringing while
  hunt_hungerbell lives (`untilFlag` waits for W2); emitters with linear attenuation and A4 panning; Ashwheat's knock
  spots come from U4 through `UAbyssAudioSystem::SetZoneEmitterSpots` (fallback: one emitter at the area centre (40,52)),
  the hearth from `abyss::CampProps` (camp 1's campfire, tested at (15,15)).
- **Bells.** Manifest `cueTriggers` (`dark_ambience.BELL_TRIGGERS`, `dark_vox.VOX_TRIGGERS`): prologue slide 6 (ten tolls,
  2.8 s apart, stopped with the prologue), a toll after cs_rf_brand / cs_rf_whisper (near / far by the knellwatch region),
  self-ring at cs_rf_siege index 1 and every 20-30 s while q_rf_last_toll is active, dawn toll at cs_rf_finale index 3,
  ambush tell at the ambush start (no lead: the core spawns at once), the split bell on story_rf_belfry focus and every
  35-60 s within 10 tiles, the Corpseherd's ring at cs_rf_herd index 2. `OnCuePlayed(FName)` for U4's bell ripple.
- **Deferred.** The dusk tolls (no day clock in W1), phase stingers / P3 roar / standard snaps (W2 `EvMonsterPhase`),
  `untilFlag` (W2 flags), the planned non-vocal cues (`plannedCues`, the next AU package). The jukebox row "赤幡原 ·
  探索" still plays the bright web recording `emerald_plains_explore` (the audio owner's call).
- **For the Mac.** Not compiled here (mock-compiled against UE 5.6 signatures only): the Knellwatch lute on arrival
  after the prologue, the drones through a fight (no restart), silence in the Rim / Ashwheat / Sheepfold, the siege and
  the finale's silence → lute, the tolls (near inside the fort, far outside), the bed crossfade at the Rim edge, the
  knock / hearth attenuation and panning, encounter stinger ducks, Valdras's and the Corpseherd's named voices.

**1C U5 outcomes (quest card W1 parts, Chapter 1 text fit; 2026-10, pending review; DECISIONS DR14).**
- **Helpers.** `Source/Abyssfire/Private/UI/Panels/AbyssQuestCardRules.h` (card model: entry page, progress lines,
  choice captions with slot recovery, column width) and `Private/UI/Core/AbyssTextFitRules.h` (greedy wrap with Latin /
  CJK rules, one-line font fit); tests `PresentationTests/tests/questcard_test.cpp` (real Chapter 1 quests; every class ×
  8 seeds through the core's `GenerateQuestRewardChoices`: each caption names the slot and label of its item, also with
  a slot dropped) and `textfit_test.cpp`.
- **Progress lines.** A progress-only card renders (it was blank while the core held its modal): per line the quest name
  and the giver's line in quotes, View Story when the NPC has a tree, Leave. With an entry the lines follow it under an
  *Active* rule. No new UI key (`ui.dialogue.leave`, `ui.questLog.tab.active`).
- **Labelled choices.** Label primary, slot name secondary, columns sharing the card width (≤ 150 px) that wrap; the
  captions follow the generated items (a slot that yields nothing no longer shifts the slot names: today's card took
  `rewards.choices[i]` for item i); the accept preview reads `label · slot`. Unlabelled cards draw as before.
- **Text-fit audit** (`Scripts/tests/test_ui_text_fit.py`: fontTools advance widths of the bundled fonts, zh-CN + en,
  desktop + touch; skipped without fontTools). Pinned: quest card (giver titles, choice captions, progress lines, accept
  preview), quest log rows and details, tracker, toasts (the sealed entrance is the longest: 428 px in English),
  quest-complete and discovery banners, every Chapter 1 event / system line in the combat log (≤ 3 rows; the rescue
  line is the longest), puzzle prompt and answer, lore and dialogue titles. Found and fixed: the tracker wrapped 12 px
  past its frame (Width − 6 against its 9 px padding; "[Side] Bounty: Harke the Oathless"); the puzzle answer "Ring them
  by their numbers, one to five" (253 px at 13 px) overflowed its fixed 200 px button (it now grows and stacks above
  Leave); the hidden-area discovery is ~1080 px in Cinzel 22 px (centre banners now wrap at 720 px: two lines); frame
  titles at or over their band ("Tamsin the Gravewarden" 249 / 232 px in a 420 px frame, "The Marshal's Last Order"
  253 / 252 px) now shrink, down to 13 px. Quest log rows (≤ 208 / 234 px), toasts and combat-log lines fit as they were.
- **Content handoff** (not U5's files): objective labels that already carry a verb read doubled behind the objective
  type the UI prefixes (card, log, tracker): "Escort Escort Bern to the hallowed ground" / "护送 护送伯恩去最后的圣地"
  (q_rf_hallowed_ground), less glaringly "Talk Bring the samples to Marl" and "Defend Hold the Boneyard Gate". Rephrase
  those `data.questTarget.*` strings as targets ("Bern" / "伯恩"), content lane or 1C close.
- **For the Mac.** Not compiled here (no UE; the glue was read against the module's Slate idioms, the logic is in the
  tested helpers): the Captain's progress-only card while *The Sleepless Marshal* runs, desktop and touch (Leave, View
  Story); the Tag-Thief and Oathless turn-in cards (two labelled columns, selection highlight, the touch tap card) and
  their accept previews; the paymaster's discovery banner on two lines; the puzzle answer stacked above Leave in English,
  side by side in Chinese; the tracker's two-line "Bounty: Harke the Oathless"; the lore card title of
  lore_rf_01 in English (17.5 px).

**1C U4 outcomes (set-piece layer; 2026-10, pending review; DECISIONS DR15).**
- **Data.** `GameData/setPieces.ts` → `Data/set_pieces.json` (exporter `src/tables/setpieces.ts`): 19 pieces, 36 placements
  in emerald_plains, each with the reason it stands where it does (chapter1.md §1 / §2 / §5 / §9.1 / §10, the art lane's
  vignettes and `placement` notes). Knellwatch: the hearth dressing on the campfire (`walkOver`: stools, its
  `blockingCells` stay walkable in W1), the bell frame south-west of the hearth facing it (off the south lane and the
  south-east exit road the bot walks), the Name Wall on the camp's west side facing in. The sealed Pit mouth on
  `entrance_rf_pit` (smoke). The Boneyard Gate on `q_rf_last_toll`'s defend target, yaw 180, `walkOver` (the burial road
  runs through the lychgate), yields to the target. Ashwheat: the burned bridge at (38,48) yaw 90 with its ditch bed
  (smoulder; its echo socket lands 0.9 tile from `lore_rf_echo_02`), three cottages (variants A / B / A) off the cart
  track, each knocking. The Sheepfold: six wagons on a 10-ring (r 5.8) with gaps north, south and west (the ruts the
  bot takes). The Breach Rim: the dais at the centre, yaw 0 (crack toward the chasm), ash circles at (11,91) and (19,99),
  12 banners at r 9 (start 3.54° keeps them off the route and the rock), helmet stakes along the Dead Ranks approach and
  the rim. Three label strings: `zone.event.{merchant,puzzle,treasureChest}.label.emerald_plains` (圣骨贩子 / The Relic
  Peddler, 五口小钟 / Five Handbells, 阵亡骑士的铁箱 / A Dead Knight's Strongbox; zh-TW through the converter). Goldens
  unchanged.
- **Core.** `abyss/data/SetPieceData.h` + `LoadSetPieces.cpp` (optional table; shape errors fail, unresolved references
  warn), `abyss/world/SetPieces.h` (anchors, rings, facings → tile + actor yaw, socket / bounds transforms; Det trig),
  `AssetManifest` `boundsCm` + `FindAllByGameId`. `CoreTests/tests/setpieces_test.cpp` (7 cases, 2401 assertions): every
  piece on its map, its art (and partners, every variant) resolving, nothing under it on water / rock / a blocking
  decoration, and every piece without `walkOver` off the A* paths of the bot's whole route and off every interaction
  point. Placements were moved until it held (bell frame, south cottage, wagon ring, banner start, stakes, gate).
- **UE.** `Private/World/AbyssSetPieceRules.h` (`PresentationTests/tests/setpieces_test.cpp`, 8 cases, 459 assertions:
  synthetic socket turns / emitter grouping / covered tiles / yields / labels, and on the shipped data every socket on
  every variant, spots by their piece, each echo figure within 3.5 tiles of its lore spot), `AbyssSetPieces.cpp`
  (`AAbyssZoneActor::BuildSetPieces`), world builder (emitter spots after the build, `OnCuePlayed` ripple, echo anchor,
  defend-target yield + `AAbyssPropActor::SetFacingYaw`), `SAbyssWorldLayer` event labels. Details: module README
  *Dark remake set pieces*.
- **Echo by the dais.** U1's handoff (the dais 5.8 tiles from `lore_rf_echo_03`): the dais keeps the art contract's yaw
  0 and the echo's figure group turns 135° about it (`echoTurnDeg`), so the deserter kneels at the north-east kerb,
  still facing the seal, 2.9 tiles from the lore spot (8.5 before: the screen's edge when the hero reads it).
- **Not placed (no art yet, no placeholders):** Hald's sword heap by the forge, Crowmarket's salvage carts / boot lines,
  the bell-frame lantern's warm light (no `camp.lantern` recipe), the dais breach's violet uplight (no recipe for socket
  `breach`), the cottages' `candle` sockets. The wagon barricade and the order post are not set pieces: U1 shows them
  as the sealed exit (`exit_gate_sealed@march`) and the lore prop (`lore_ancient_tablet@march`). Added by the 1C review
  (chapter1.md presents them; each waits for the art id or recipe named, then becomes a set-piece row):

  | Piece (chapter1.md) | Needs | Owner |
  |---|---|---|
  | the dropped lantern at (29,15) beside `lore_rf_echo_01` (§5 M1; the `MI_AF_Echo` contract: "the real prop beside him") | a `gameId` on the lantern prop (`SM_NPCKit_Prop_Lantern` has none), then a `tile` piece at (29,15) | A10 (id), U4 (row) |
  | the chapel crypt door where the stash lives (§2) | a crypt-door mesh + `gameId` | A10 |
  | the chasm decal of the breach on the dais's west side (§9.1; the dais's yaw-0 crack points at it) | a ground decal mesh / material + `gameId` | A10 |
  | Marl's dressing-station dressing at (50,30) (§1) | cots / bandage line meshes + `gameId` | A10 |
  | Hald's forge warm light (§15: warm light only at the hearth, forge, bell-frame lantern, cookfire) | a `camp.forge` / lantern recipe | A12 |
  | Crowmarket's Gleaners identity (§2: banner, salvage carts, boots strung on lines, crows on the cart rails, a cold cookfire) | meshes + `gameId`s; until then Crowmarket shows the march kit's generic pieces without Knellwatch's banner (1C review) | A10 |
  | the sealed exit's march loop (`exit.sealed@march`: smoke / ash at the barricade, breach violet beyond; no crimson ground ring) | the recipe; until then the march exit plays no loop (1C review) | A12 |
- **For the Mac.** Not compiled here (no UE; the glue was read against the zone actor's instancing / occlusion and the
  VFX pool idioms): the 36 placements in emerald_plains (log line `Zone emerald_plains set pieces: 36 of 36 placements
  drawn`), the R1 fade of the bell frame / Name Wall / cottages / wagons / banners, the bell ripple on the dusk toll and
  the Boneyard Gate's self-ring, the knocking from the three doors, both echoes beside their pieces, the gate hidden under
  the defend target during *The Last Toll* (no double gate, same facing), and the event labels (zh-CN, en).

**1C review outcomes (2026-10, pending review; the findings of the 1C review and what became of them).**
- **Fonts (major).** The bundled subsets were built before the 1B content switch: 130 zh-CN characters of the data
  (赤幡原's 幡, 丧钟堡, 帅, 瓦, 德, 冢, 卒, 贩, ...) drew as tofu boxes. `Scripts/fonts/build_fonts.py --offline` rebuilt
  every face from downloads matching `fonts.lock.json` (sha256); `build_fonts.py --check` passes. New
  `Scripts/tests/test_font_coverage.py` fails whenever a data character is missing from any Simplified Chinese face, so
  the gap now fails the scripts acceptance. **Owner:** whoever changes `Data/*.json` strings re-runs `build_fonts.py`
  and commits `unreal/Fonts` (the test names the command); the fonts are loaded raw at runtime (staged `Fonts/`), no
  re-import.
- **Chapters 2-5 unchanged.** The rescue prop's new ground label is drawn only in a zone with a presentation theme
  (`AbyssSetPieceRules::EventLabelKey(.., bThemedZone, ..)`; `setpieces_test` pins every unthemed zone to today's labels).
- **Audio glue.** A track `PlayMusic` cannot voice (missing sound; `-nosound` / no audio world) counts as asked
  (`UnvoicedTrack`), so the arbiter no longer re-requests it every frame; an emitter started as a plain 2D loop (no
  listener yet) becomes a panned source once a listener exists; a monster def's voice is resolved once per def
  (`AbyssVoiceRules::ResolveDefVoice`, cached in `DefVoices`: two map lookups per SFX / anim event, no string work, no
  table scan); area-loop / emitter assets are FNames converted once, the sounding track string only when it changes.
- **World / UI glue.** The zone's ambience recipe and wind-up tints are recomputed only when the dominant or base mood
  changes; the hold / stun clip names are a once-built FName table; the centre banners bind their wrapped lines by
  reference (filled in `ShowBanner`).
- **Content as built** (chapter1.md §19, second table): Crowmarket keeps the march kit's generic pieces but not
  Knellwatch's banner; `bell_self_ring` ripples Knellwatch's bell frame (the gate's bell echoes it); the Sheepfold's west
  opening stays two slots (the bot's route crosses both); the march sealed exit plays no crimson web loop until
  `exit.sealed@march`.
- **Pending review, not changed:** `lore_rf_echo_03`'s 135° turn (U4) puts the kneeler on the north-east kerb in a
  three-quarter view, which art review r1 had rejected (west gap, back to the camera); A10 decides between the turn, a
  re-pose on the lore spot's side, or moving the lore spot (mac-look item). The contract's `poseOverride` (arms bound
  behind) is not applied in UE; the figure uses the escort rig's Death frame 22 until A9's `Kneel_Bound` clip ships.

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

**2A outcomes (K2 as built, 2026-10; pending review; DECISIONS M9 amendment, DR8 *K2 as built*).**
- **Schema.** `MonsterData.h` `MonsterAbilityDef` carries every K2 field (`abilities[]` as in K2 "Data"), parsed by
  `LoadMonsters.cpp` (`ReadMonsterAbility`; also for hunt `defsNormal`, so a hunt leader keeps its base's abilities) and
  `MonsterDef::damageType` / `hitCapHeroMaxHpPct` / `bossSkills`. Shape rules at load, mirrored by the exporter
  (`tables/monsters.ts` `checkK2Monster`): unknown keys at every level fail (a misspelt optional field would otherwise be
  dropped silently); `windupMs` 600..5000; `cooldownMs` / `initialCooldownMs` 0..600000; `clip` and `aim` required; per
  kind a `shape` (circle radius > 0, ring radius > innerRadius >= 0, cone 0 < arcDeg <= 360, line length / width > 0),
  `hazard` (`durationMs` <= 60000, `tickMs` 100..durationMs; `HazardField::TickCount` saturates at `kHazardMaxTicks`),
  `summon`, `buff`; a status needs `value` > 0 (a status of value 0 is rejected by `StatusEffectSystem::Apply`); a hazard ability
  takes neither `dmgMul` nor `status` (its pulses are the hazard's ticks); `hitCapHeroMaxHpPct` is a percent in (0, 100].
  `DataStore::Finalize`: summoned ids exist; a mini-boss, sub-dungeon mini-boss or the story boss winds up >= 700 ms;
  `bossSkills` ids must name abilities once a monster has any, else they stay inert with one warning per monster (the
  web's `demon_lord` / Labyrinth bosses today); an ability timed by its clip warns when the manifest has no beat for it;
  an ability whose `rangeMin` lies past the swing reach (attackRange x `attackExitMul`) warns "never picked" and one whose
  `rangeMax` does warns "no effect" (the exporter fails the first and warns on the second).
- **Two schema additions the plan did not spell.** `shape.kind` (circle | cone | line | ring): optional where the
  ability's kind implies it, required for a `hazard` ability and for `hazard.shape`, which have no implied geometry.
  `summon.at` is `self` | `anchors` | `corpse` (raise: corpse). Buff `stat` is `damage` | `attackSpeed` | `moveSpeed` |
  `fearImmune` (K5 may append).
- **Aims (resolved once, at windup start).** Ground shapes without a direction (circle, ring, hazard on one) sit on the
  aim point: `heroAtWindup` = the hero's position then, `self` = the caster, `anchor` = the caster's spawn anchor (the
  dais; K4 may move it to the phase's leash centre). Cones and lines start at the caster (the anchor for `anchor`) and
  point at the hero's windup position (`self`: the heading). **Deviation:** `hero` is refused for circle / ring / hazard
  (a decal that followed the hero to contact could not be dodged, monsters.md 1.3) and is the only aim of `bolt` (the
  bolt locks on the hero like the basic ranged swing). `corpse` is K3's.
- **Runtime** (`MonsterAbilities.{h,cpp}` pure; `CombatSystem` runs it; Combat.h *K2* notes). A swing that comes due
  becomes the first ready ability by priority (higher first, ties in data order) whose kind runs, cooldown is over, hero
  distance is in [rangeMin, rangeMax], life is below `hpBelowPct` and phase is listed (`MonsterInstance::phase`, 0 until
  K4). A monster held by a pet's taunt (`PetCompanion::TauntHolds`) starts no ability: the taunt draws the due swing to
  the beast as a basic swing, and the ability clock runs on; otherwise the pick runs before the beast's stray-swing draw,
  so that 25 % roll never takes an ability from the hero. The ability clock starts at the monster's first swing
  opportunity of a fight (each ability ready `initialCooldownMs` later) and restarts after a leash. Cooldowns count from windup start.
  The pending ability is a `PendingStrike` on the `MonsterStrikeContact` timer; the caster is rooted (no AI tick) and
  starts no other swing until contact. Contact = `windupMs`, else the clip's Contact notify, else its Release, never
  below 600 ms (700 ms for TelegraphStyle::Boss); the clip plays at the rate that puts its beat there. At contact: a ground
  shape hits the hero whose 0.35-tile body touches the shape placed at windup start, then every other living monster in it
  when `hitsMonsters` (x `monsterDmgMul`, no provoke), then leaves its hazard; a bolt flies (payload = ability index + 1)
  and hits on arrival; a `hazard` ability places its hazard, whose first pulse lands at once. Aborts (S7): the caster
  dies, or is stunned or frozen at any moment of its windup (`EvMonsterAttackCancelled` at once, from the next
  `TickCombat`, even for a stun that ends before contact; a basic swing keeps the web's check at contact); the hero is
  dead or the zone is changing at contact (silent); F2 cancels it with the swings; the cooldown stays spent. Kinds raise / summon (K3) and buff / call
  (K5) are parsed but never picked (`AbilityKindRuns`), and the exporter value-gates them (`REMAKE_VALUES`
  `monster.abilityRaise` / `abilitySummon` / `abilityBuff` / `abilityCall`) until their package lands.
- **Damage.** Every monster hit on the hero runs `CalculateDamage` with `SkillHitInput::basicType` (the monster's
  `damageType`, an ability's own when set) and `basicMultiplier` (`dmgMul`; 1 for swings), so non-physical hits meet the
  resistance step; the HP taken is capped at `MonsterHitCap` = floor(hero max life x `hitCapHeroMaxHpPct` x the
  difficulty's `damageMul` / 100), at least 1. **Interpretation:** "scaled by the difficulty table" (monsters.md 1.3) is
  the difficulty's monster damage multiplier (Normal 35 %, Nightmare 52.5 %, Hell 70 %), so the cap binds at the same
  point of the scaled damage; no new difficulty field. An ability hit rolls the ability's status instead of the def's
  `onHitStatus`; elite on-hit effects apply to both (Oathfire's extra fire is its own small hit, not capped). No cap and
  physical are the defaults: the shipped data deals what it dealt.
- **Hazards** (`combat/HazardField.h`): per zone, ticked after the status effects every step (`CombatSystem::TickHazards`),
  ticks in (time, id) order. A hazard ability pulses at start + k x tickMs while k x tickMs < durationMs (Breach-seep 1000 /
  500: two pulses, the first at contact); a hazard a strike leaves ticks at start + (k + 1) x tickMs while <= durationMs
  (Oath-Brand 4000 / 500: eight). A tick deals floor(damage x tickDmgMul) (>= 1) less the hero's resistance, capped like a
  hit, with no roll, defense or i-frames (a DoT tick), rolls its status, and hurts monsters inside with `hitsMonsters`.
  Unlike a status DoT tick (whose Death Save skip is the web's 5.3 quirk), a lethal hazard tick checks Death Save as a
  monster hit does: it is monster damage. A zone change drops them.
- **Events.** `EvTelegraph {entity, abilityId, kind, shape (AbilityShapeDef: kind + radius / arcDeg / length / width /
  innerRadius), origin, dir, startMs, contactMs (absolute), style}` after the windup's `EvPlayAnim`, for ground kinds;
  `EvHazard {id, source, abilityId, shape, origin, dir, damageType, startMs, untilMs, tickMs}`; `EvMonsterAttackCancelled`
  also ends an ability's telegraph. **The plan's `EvMonsterAnim {entity, action}` is the existing `EvPlayAnim`**: it
  already carries entity and action plus what UE needs to time the clip (`clip` = the ability's clip, `contactMs` =
  `windupMs` = the contact, `durationMs`, `playRate`, `faceTarget`); actions Attack02 / Cast02 / Roar by clip name, Cast
  for Cast01, else Attack with the explicit clip (UE's Attack branch plays a named clip, so Lunge / Consume need no new
  AnimAction). Both events are appended to `abyss::Event`; the UE router builds its delegates from the variant. Snapshot:
  `telegraphs` (fill progress) and `hazards`; `MonsterView::windingUp` covers an ability's whole windup.
- **Exporter.** `REMAKE_SCHEMA` gains `monster.abilities`, `monster.damageType`, `monster.hitCap`; `tables/monsters.ts`
  calls `gateRemakeValues` on every definition and hunt definition (it did not before). The export is byte-identical (no
  GameData record uses the fields yet; 2F authors them); an end-to-end run with a temporary ability on a monster loaded in
  the core with its hunt carrying it, and a typo / a raise / a 500 ms windup / `aim: hero` on a circle / a status of
  value 0 each failed the export.
- **Tests.** `monster_abilities_test.cpp` (suite `monsters`, 36 cases, 2068 assertions on GCC): schema load and
  refusals (28 broken abilities, the upper bounds), Finalize checks (boss floor, summons, bossSkills, clip beat, the
  swing-reach warnings), shape boundaries (inside / edge / outside, body 0.35), picker order and conditions, timing (clip
  beat, authored windup, floors), hit cap per difficulty, HazardField schedule (and its saturation), aims; runtime through
  the combat systems: contact equals the clip's, priority and cooldown order with the basic swing between, initial
  cooldown, hpBelowPct / range, the leash reset, snapshot vs facing aim (circle, cone, line), rooting, one action at a
  time (a 2400 ms windup on a 1500 ms swing interval), aborts on death / stun / freeze mid-windup (also one that ends
  before contact) / F2 / a hero dead at contact, hazard ticks and expiry, zone drop, damage types through resistances
  (abilities, swings, ticks; an ability, its hazard, its monster hits and a bolt without their own type take the
  monster's), the hit cap on swings, ability hits, hazard pulses and hazard ticks on Normal, Nightmare and Hell, bolts,
  `hitsMonsters` (exact damage = dmgMul x monsterDmgMul through the formula; a cone strike and hazard ticks spare a monster
  inside the reach but outside the shape), telegraph style (elite flag, affixes, hunt leaders) and the boss floor, a pet's
  taunt, Death Save on hazard ticks, kinds not run yet, the GameSim wiring (contact timer, `TickHazards` in the step, the
  snapshot views), and a seeded 20 s fight whose event-log digest is pinned (`aeebb747324d47b9`, identical on GCC and
  Clang). No existing assertion changed. The playthrough's final saves are byte-identical to the previous commit's (22
  runs: the three pinned, the regression seeds, a 5-seed sweep per class).
- **For 2F / 2G.** Content authors `abilities`, `damageType` and `hitCapHeroMaxHpPct` (35) per monster. A pick happens
  only on a due swing, i.e. in the Attack state (within `attackRange`, kept to 1.2x): a `rangeMin` past that never fires
  (the export fails, DataStore warns) and a `rangeMax` past it changes nothing (warned), so a long reach (Lunge 5) needs
  the monster's own range or a later chase rule. Valdras's
  Breach-seep needs three lines (three abilities, or a `count` field the schema does not have yet); Lunge is a line strike
  that does not move the caster (a dash would be a new field). UE (U2): draw `EvTelegraph` / `TelegraphView`, end it at
  contact or on `EvMonsterAttackCancelled`, draw `EvHazard` / `HazardView` until `untilMs`, clear both on zone unload.
- **Review fixes (2A review, 2026-10).** R1: the rules above that no case pinned now have one (one action at a time,
  per-difficulty caps on abilities and ticks, damage-type fallbacks, shape-bound monster hits and their exact damage, a
  dead hero at contact, affix telegraphs); each was checked by mutating the core and watching the case fail. R2: a stun or
  freeze mid-windup aborts the ability at once (it used to be checked only at contact, so a short stun let the slam land
  with no windup clip). R3: a pet's taunt holds the monster's abilities (they used to fire at the hero through it). R4: the
  swing-reach warnings. R5: Death Save on hazard ticks (deviation 5's "like a DoT tick" no longer skips it). R6: upper
  bounds on the timing fields, `TickCount` saturates (an int32 counter overflowed past ~2e11 ms), and the exporter's kind
  check uses `Object.hasOwn` (`'toString' in {}` passed and then threw).

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
