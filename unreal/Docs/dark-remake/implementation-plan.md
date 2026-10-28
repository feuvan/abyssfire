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
gold. No player saves exist yet (U5, I8). K6 (2E) adds `worldFlags` and `permanentRewards` without a version bump:
written after `contentVersion` only when not empty, absent = none, kept by the content-version upgrade (*2E outcomes*).

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
- **Event champion packs** (added by the 2D review: the ambush's champion Muster, chapter1.md's ambush row and
  monsters.md 3, needs core work that neither 2D nor 2F (content only) owns). A zone's random-event data may name a
  champion pack for its ambush (a remake field behind `REMAKE_SCHEMA`, e.g. `random_events.json
  zones.<zone>.ambushChampion {chance, mix [{monsterId, count}], leader, sharedAffix?}`): only when present, an ambush
  draws once more on the Events stream (no draw without it: golden-neutral) and on a hit spawns the champion pack in
  place of the ambush (one K5 pack id, the leader `packLeader`, the zone's affixes rolled once and given to every member
  when `sharedAffix`). 2F authors the Muster row (a standard-bearer and three footmen, 10 %). Cut order: champion packs
  go third (the risk table's *Scope* row). (The 2E review added `leaderLoot`, the Muster's guaranteed magic drop of
  monsters.md 2.8, which no package owned: *2E outcomes*.)
- **Tests** (`world_state_test.cpp`): ashnumb arithmetic and cleanse; permanent rewards once per difficulty and the save
  round-trip; flags save round-trip and an old save loads with none; spawn variant selection; NPC line selection; a
  champion ambush (the draw only with the block, the pack and its leader, the shared affix).

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
drummers first); do not stand on the dais in Valdras P2; leave cinderbloats to burst away from the hero. *As built
(2F):* the telegraph, hazard and arena rule (with a dodge roll for a contact due within 200 ms), priority from the data
(a support caster or an aura owner, then a pack leader; no `botPriority` field), Raised lured out of a Feed zone before
they die, a boss followed home and its fight kept inside its leash; on-death fuses are telegraphs like any other
(*2F outcomes*). *2F review:* the frame-perfect bot is one of two models. `AutoPilotConfig::reactionMs` makes a
telegraph or hazard a danger only that long after it appeared, and `dodgeRoll` false walks out without the i-frame roll;
the sweep runs both (`ABYSS_PT_REACTION_MS=400 ABYSS_PT_NO_DODGE=1`: the reaction-limited bot, a human-like 400 ms, no
dodge roll). The fence keeps a named fight inside the monster's real leash (`MonsterView::leashCentre` / `leashRange`).
`AutoPilotStats` adds `maxBurstTakenPct` (the most damage taken within any 1000 ms, a percent of max life) and, per named
fight, the abilities each monster started and how many of them were cancelled before contact (`AutoPilotFight::abilities`).

**Playthrough pins** (`playthrough_test.cpp`): three classes, seeds 101 / 202 / 303; hero level 12-14 at the end of
Chapter 1, embers ≥ 40, kills ≥ 40, levelUps ≥ 11; achievements `ach_first_kill`, `ach_kill_valdras`, `ach_quest_10`,
`ach_level_10`; checkpoints renamed to the route's. A sweep (`ABYSS_PT_SWEEP=1 ABYSS_PT_SWEEP_BASE=<seed>
unreal/CoreTests/run.sh`) over 20 seeds per class must pass before each wave closes. *2F review:* every run also holds
its largest 1 s burst at or under 50 % of max life and its story boss fight at or under 400 s with no reset; a sweep of
at least ten seeds per class holds DR9's per-class band (*2F review*): the median story boss fight 30-180 s with its 90th
percentile under 240 s, hero deaths in at most 2 % of those fights (10 % for the reaction-limited bot), the mini-boss's
opening raise in at least 95 % of its fights and Keen in at least 60 %.

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
| **2E** | K6 status, rewards, flags, NPC lines, spawn variants, event champion packs (2D review) | core lane + `world_state_test.cpp` | 2D | same + save round-trip |
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
  dais; K4 may move it to the phase's leash centre; since the 2C review the caster's authored spawn tile,
  `MonsterInstance::homeTile`, not the tile M10's placement jitter picked). Cones and lines start at the caster (the anchor for `anchor`) and
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

**2B outcomes (K3 as built, 2026-10; pending review; DECISIONS DR8 *K3 as built*, DR6, M4 / M5 notes).**
- **Schema** (`MonsterData.h`, parsed by `LoadMonsters.cpp`, mirrored by the exporter's `tables/monsters.ts`; unknown keys
  fail at every level). Monster `family` (an id: `unburied`, `living`, `gravegnawers`, `breach`), `onDeath {kind: burst |
  ring | puddle | spawn, delayMs, shape, dmg | hpPct, damageType, status, hitsMonsters, monsterDmgMul, capHeroMaxHpPct,
  puddle {durationMs, tickMs}, spawn {monsterId, count}}`, `corpse {leaves: corpse | remains | none, consumes}`,
  `riseAgain {chance, delayMs, hpPct, notOnCrit}`; the K2 `summon` block gains `radius`. Map `corpseSeeds [{col, row,
  monsterId}]` (`LoadWorld.cpp`). Elite affix definitions (`LoadCombat.cpp`, `tables/combat.ts`) take optional `onDeath`,
  `riseAgain` and `families`, and the core knows an eighth type, `unburied` (appended to `EliteAffixType`; in the roll pool
  only when elite_affixes.json defines it and lists it in `order`). Rules: a burst's fuse >= 800 ms and its
  `capHeroMaxHpPct` required (monsters.md 1.3); a ring or puddle deals damage or carries a status; a burst is a circle, a
  ring or puddle a circle or a ring (a ring's shape defaults to a ring, a puddle's to a circle); `puddle` / `spawn` blocks
  only on their kind (spawn 1..20 bodies, no damage fields); `dmg` and `hpPct` exclusive; delays <= 10 s (on-death) / 60 s
  (rise); `riseAgain` needs `corpse.leaves: corpse`; summon `radius` 0..30 and none at `anchors`; summon `windowMs`
  <= 600000. `DataStore::Finalize`: on-death spawn ids (monster and affix) exist; a corpse seed lies on its grid and names a
  monster whose body leaves an entry; a rise that waits as long as a corpse lasts warns ("never rises"). Exporter:
  `REMAKE_SCHEMA` gains `monster.family`, `monster.onDeath`, `monster.corpse`, `monster.riseAgain`, `map.corpseSeeds` and
  lifts the value gates of `monster.abilityRaise` / `monster.abilitySummon` (buff / call stay gated for K5). Since the core
  reads `family`, the export now writes the Wave 1 families: the only Data change is `family` on the 13 Chapter 1
  definitions in monsters.json and the 3 hunts in quest_hunts.json (index.json follows); nothing in the core acts on a
  family alone, and the playthrough's final saves stay byte-identical.
- **Corpses** (`monsters/Corpses.h`, owned by MonsterSystem, per zone). An entry is `{id, defId, artId, family, kind:
  corpse | remains, pos, deathMs, expiresMs, seeded, spent, monster, holder}`; ids count up for the session. A kill's body
  lasts 20 s (`MonsterAiDef::corpseTtlMs`); at most 24 kill bodies (`corpseCap`): the 25th pushes the oldest (death time,
  then id) out with reason `expired`; map seeds never expire and are outside the cap, and are seeded again on every zone
  entry (`SpawnZonePopulation`, after the population; no RNG). A `corpse` is raised (unless `spent`: the body of a Raised,
  which rose once) and eaten; `remains` are neither (the Gravegnawers' carcass, a boss's body); both burn. A holder (a
  raise winding up, a consume channel, a pending Unburied rise) keeps everyone else off a corpse; fire, the TTL and the
  cap remove it anyway and the holder finds it gone. Queries sort by distance, then death time, then id. A zone change
  drops every body silently.
- **Kill pipeline** (`SimWiring.cpp`, new step 8b after the quest drops, before the loot): `MonsterSystem::OnMonsterDied`
  lets go of the dead monster's holds, registers its body when its definition leaves one (`MonsterKilledMsg` now carries
  the killing blow's `element` and `crit`: a fire killing blow registers the body burned, EvCorpseAdded then
  EvCorpseRemoved{burned}), schedules its Unburied rise, then collapses every Raised it stood up; then
  `CombatSystem::OnMonsterDeathEffects` schedules its on-death effects. A Raised body's kill pays nothing (Spirit 'kill',
  exp, gold - no Loot draw -, the kill heal and their floating texts in step 1; pet exp, embers / garden / expedition,
  quest drops and loot are skipped by role) but still counts as a kill (achievements, quest kill objectives, story
  triggers, totalKills).
- **Raised** (`MonsterRole::Raised`, appended): a raise's or summon's body (raiser = the caster), the Unburied rise's
  (raiser none) or an on-death spawn's (raiser none); `def.expReward` and gold zeroed, `noRespawn`, `raiserId`; it stands up
  chasing, rooted while its art's Rise clip plays (`EvPlayAnim{Rise}`; the clip's length, else `riseMsDefault` 1200 ms).
  When its raiser dies, every Raised of it **collapses** (`MonsterSystem::Collapse`): EvPlayAnim{Death},
  EvMonsterCollapsed {entity, raiser}, no MonsterKilledMsg (no credit, no on-death effect, no rise, no respawn), its body
  joins the registry spent, and its own Raised collapse after it.
- **Raise / summon abilities run, as timed casts.** A raise / summon competes on a due swing like any ability, and between
  swings - from Chase or Attack, no swing due - `CombatSystem::TryStartSummonCast` picks among the raise / summon kinds
  alone (`AbilityPickInput::summonsOnly`) on their own cooldown and `rangeMin` / `rangeMax` to the hero (not bound by the
  swing reach: DataStore no longer warns and the exporter no longer fails on them), never while a swing or ability of
  the caster is on its way to contact (`CombatSystem::HasPendingStrike`) or under a pet's taunt; such a caster's ability
  clock starts at its first Chase / Attack step within the swing scan (12 tiles of the hero). Either way the picker sees
  a raise / summon only when the runtime planned bodies for it (`AbilityPickInput::available`; `CombatSystem::PlanSummon`): `count`, cut to the room under `capAlive` (the caster's
  live Raised) and under `perWindowCap` (the bodies the caster stood up within `windowMs`, all its abilities together); a
  raise's free raisable corpses within `summon.radius` of the caster (default 6, query order); a summon's spots on its
  `anchors` (round robin; the anchor tile, else the nearest walkable) or on a ring of `radius` around the caster (default
  1.5; body k at 2 pi k / n). The pick holds those corpses, plays the clip and draws one breach-violet marker per corpse /
  spot (EvTelegraph kind Raise / Summon, radius `raiseMarkerRadius` 0.75); at contact the bodies still there stand up
  (EvCorpseRemoved{raised, by caster}, EvEntitySpawned, EvMonsterRaised, EvPlayAnim{Rise}) at `hpPct` of their life, the
  live cap checked again. Every abort of K2 (death, stun / freeze, F2, a dead hero at contact) lets the corpses go.
- **The Unburied rise.** At a death that leaves a raisable corpse, the first of the monster's affixes with `riseAgain`
  (the `unburied` affix), else its own trait, may stand the body up once: not after a fire killing blow, nor a critical one
  when `notOnCrit`; never for a Raised or a boss role (mini-boss, sub-dungeon mini-boss, story boss, seal keeper, hunt
  leader); `chance` < 1 draws once on RngStream::Ai. The pending rise holds the corpse; `delayMs` later it stands up as
  Raised (EvMonsterRaised{selfRise}) - the dead instance's definition, affixes and maximum life at `hpPct` - unless fire, the
  TTL or the cap took the body first. Elite affixes with `families` roll only for monsters of those families
  (`EliteAffixTable::RollableFor`; the shipped seven have none, so their pool and draws are unchanged).
- **Consume.** A monster with `corpse.consumes`, idle or patrolling (or below half its life in a fight; never while
  returning, casting or with a swing on its way to contact: the bite lands first), starts eating the nearest consumable corpse within 1 tile instead of its AI step: a 2 s channel
  (rooted, no swing; `EvPlayAnim{Attack, clip Consume}`), then it heals floor(30 % of its maximum life) and the body goes
  (EvCorpseRemoved{consumed, by}). Damage does not interrupt it; a stun or freeze, or the corpse vanishing (fire), ends it
  with EvMonsterAttackCancelled and no heal. Constants in `MonsterAiDef` (port tuning, not loaded).
- **On-death effects** (`monsters/OnDeath.h` pure rules, CombatSystem runtime). Scheduled at the death spot in the order:
  the definition's, its affixes' (in affix order), the zone's (`CombatSystem::SetZoneOnDeath`); each fixes its damage at the
  death (`dmg` x the difficulty's monster damage multiplier, or `hpPct` of the dying monster's maximum life, rounded half
  up, >= 1) and draws its fuse (EvTelegraph{source OnDeath, kind = its shape's}; also a TelegraphView). `TickOnDeath`
  (GameSim, after TickHazards) resolves the due ones in (time, schedule) order: a burst or ring hits the hero whose 0.35
  body touches it (dodge-roll i-frames avoid it; else the damage less the hero's resistance, capped by the smaller of
  `capHeroMaxHpPct` and the monster's own hit cap, both x the difficulty's damage multiplier; Death Save holds; then its
  status) and, with `hitsMonsters`, each living monster in it (raw, x `monsterDmgMul`, KillSource::Environment, no
  provoke); a puddle joins the HazardField (first pulse at once, both caps); a spawn stands its bodies up around the spot.
  A cinematic does not cancel a fuse; a zone change drops them.
- **Fire burns bodies** (MonsterSystem::BurnCorpses, a body touching the area): a fire killing blow, a fire ground strike at
  its contact, every tick of a fire hazard (abilities' and puddles'), a fire burst / ring, and a hero fire skill where its
  area lands (ground / trap / nova circles, cones and lines at release; the meteor at its fall; each tick of a fire wall or
  an armed trap).
- **The volatile curse** maps onto a burst: `VolatileCurseOnDeath(curse)` = fire, `hpPct` = deathBurst x 100 (12), radius 2
  (the web's), fuse 800 ms, cap 20 %, monsters spared; the Labyrinth runtime installs it with
  `SetZoneOnDeath(VolatileCurseOnDeath(curse), "curse.<id>")` on a floor's entry (no Labyrinth runtime exists yet).
- **Events** (appended to `abyss::Event`; the UE router needs no change): `EvCorpseAdded {corpseId, monster, defId, artId,
  family, kind, pos, deathMs, expiresMs, seeded, spent}`, `EvCorpseRemoved {corpseId, reason: expired | raised | consumed |
  burned, by}`, `EvMonsterRaised {entity, raiser, corpseId, defId, abilityId, pos, startMs, riseMs, selfRise}`,
  `EvMonsterCollapsed {entity, raiser}`; `EvTelegraph` gains `source` (Ability | OnDeath). Snapshot: `corpses`
  (CorpseView), TelegraphView `kind` / `source` (strikes, raise / summon markers, on-death fuses), MonsterView `raiser` /
  `consumingCorpse`.
- **Deviations** (all pending review). (1) `corpse.leaves` has three values: the plan named the field, not its values;
  `remains` covers the art lane's carcasses and boss bodies (`raisable: false`). (2) The core reads `family` (corpse
  entries, the affix family restriction), so `monster.family` is exported now. (3) A fire killing blow registers the body
  and burns it at once (two events) so UE can play the burn. (4) The cap's eviction reports `expired`. (5) Seeds never
  expire and do not count toward the cap (a seed that faded in 20 s would be gone before the hero reaches the Sheepfold).
  (6) The Unburied rise is a new Raised instance, not the dead one revived: its second death pays nothing; bosses and
  Raised never rise. (7) Raised bodies still count as kills; every kill reward is withheld (exp, gold, loot, pet exp, embers,
  quest drops; since the review also Spirit 'kill' and life on kill). (8) A collapse is not a kill. (9) An
  on-death hit takes no roll or defense (like a hazard tick) but the dodge-roll i-frames avoid a burst / ring, and Death
  Save holds; the volatile curse therefore takes the fairness fuse (800, the web's 650), the exploder cap (20 %) and Death
  Save (combat-feel 5.3's quirk exempted it), but keeps the web's radius 2 and spares monsters. (10) `dmg` scales with the
  difficulty's monster damage. (11) K3 also runs `summon` (2A assigned it to K3); `summon.radius` is new; `perWindowCap`
  counts the caster's bodies across its abilities. (12) Consume never walks to a corpse (it eats what lies within a tile).
  (13) Fire ground strikes of monsters burn bodies too. (14) The `unburied` affix type and the affix `families` restriction.
  (15) The plan's entry flags `raised`, `consumed`, `burned` are removal reasons (`EvCorpseRemoved.reason`): a raised,
  eaten or burned body leaves the registry at once; `spent` marks a Raised's own body (it rose once). (16) Since the
  review a raise / summon is a cast between swings (from Chase or Attack), not only a pick on a due swing (K2's "the pick
  replaces a basic swing"): Muster and Call the Flock are timed in the design (monsters.md 2.2 / 2.5, chapter1.md), and
  a raise bound to the swing fired only while the hero stood in the caster's swing reach (never from a 3-5 tile kite).
- **Tests.** `corpses_test.cpp` (suite `monsters`; 24 cases, 2024 assertions on GCC and Clang): the schema (loads, 32
  refused monster blocks, 6 refused affix rows, seed fields; Finalize: spawn ids, seeds), the pure rules, the picker's
  availability, the Corpses container (cap and eviction order, seeds, TTL, query order, holds; six seeds beside a full
  cap), and the runtime: bodies (corpse / remains / none, fire killing blow, TTL, the cap), a raise (holds, markers,
  contact = the clip's, 40 % life, Rise root), raise caps (live, per window, the opening raise of two), Raised rewards
  and respawn, collapse (chained), burning (fire strike, hazard tick, a corpse burned mid-windup is not raised, the
  hero's fire wall and meteor), consume (idle, under half life, remains ignored, stun abort, spent corpses, burned
  mid-channel), the Unburied trait (delay, hp, once, fire / crit / burned / boss stop it, the Ai chance) and affix (crit
  does not stop it, the copy keeps its affixes; the family-restricted roll), a burst (fuse telegraph and view, cap,
  monsters x2, burns, radius, i-frames, Death Save), Scorched / Gravecold, a puddle's ticks, a spawn, the volatile curse
  and its zone exit, summons at anchors and around the caster, a zone exit, the GameSim wiring (seeds at zone entry, the
  K3 step, TickOnDeath / TickCorpses, snapshot views, a Raised kill through the real pipeline), and a seeded 30 s fight
  whose event-log digest is pinned (`746f38be25201751`, identical on GCC and Clang). Existing tests: no assertion
  changed; two comments in `monster_abilities_test.cpp` now say why the K2 "kinds not run yet" cases still hold (a raise
  without a corpse in reach is not usable); the K1 grep gate runs 4 more checks (one per new core file). Totals 770
  cases (GCC 215241 / Clang 215177 assertions; was 746 / 213213 on GCC); the 20-seed playthrough dumps (base 1000) are
  byte-identical to 2A's on GCC and Clang. An exporter end to end (temporary GameData edits, then restored and
  checksummed): a valid set (footman trait, gnawer consumes, bloat burst, standard-bearer raise, two seeds, Scorched's
  burst and the `unburied` affix) exported and loaded in the core with 0 errors; 13 broken ones (a 700 ms fuse, a burst
  without a cap, an unknown on-death key, a rise on remains, a seed of a puddle / unknown / off-grid monster, a raise
  radius 0, a 700 s window, an unknown spawn, an affix typo, an unknown affix type, an affix burst without a cap) each
  failed the export with its message.
- **Review fixes (2B review, 2026-10; pending review).** R1: raise / summon are casts between swings (above; deviation
  16): `TryStartSummonCast`, `AbilityPickInput::summonsOnly`, `CombatSystem::HasPendingStrike`; DataStore and the
  exporter exempt them from the swing-reach rules (an exporter end to end: a raise with `rangeMin` 3 / `rangeMax` 9 and a
  summon with `rangeMax` 9 on the footman (reach 1.8) export with no warning; a circle with `rangeMin` 2 still fails and
  one with `rangeMax` 5 still warns). R2: cases for every release of a hold (a stun, F2, a dead hero at contact and the
  raiser's death mid-raise free both corpses and a second raiser takes them; an eater killed mid-channel frees its
  corpse for another eater). R3: a consume channel waits for a swing already on its way (it bit during the channel);
  cases for that, for no swing or bite while eating and for no channel while returning. R4: the Raised reward skips
  through GameSim (an elite body, an active beast, a quest whose drop comes from it, life on kill): no exp, gold, pet exp,
  embers, quest progress, Spirit 'kill' or kill heal, and the Loot and Quests streams do not move; the same body as a
  regular monster pays each. R5: a Raised kill withholds Spirit 'kill' and life on kill (they were paid; raisers fed a
  renewable heal). R6: on-death damage on Nightmare and Hell (damage and cap x the difficulty multiplier), through a fire
  resistance, under the monster's smaller hit cap, a puddle's own cap on every tick, and two effects due in one step in
  (time, schedule) order. R7: every boss role stays down, the affix's rise wins over the trait, a sure rise draws nothing
  from RngStream::Ai (a 50 % one draws once), a collapse ends the Raised's windup (EvMonsterAttackCancelled), its
  statuses and the hero's lock. R8: a hero's fire nova, ground circle, cone and line (one area skill per shape from the
  real data, turned to fire in memory) burn the body inside their area at release and not the one outside; a fire
  beast's kill burns its body through the combat path and through `PetCompanion::Hit`'s fallback. Each rule was checked
  by mutating the core in a scratch copy and watching a new case fail: the hold releases in ClearPendingAbility and
  OnMonsterDied, the consume rules (Returning, the TickCombat skip, the pending-swing wait), the four Raised skips in
  SimWiring, the Loot draw, Spirit and the kill heal, the resistance step, the difficulty factor of the cap, both caps,
  the puddle's cap, the (time, schedule) sort, the boss roles, affix-before-trait, the chance-1 draw, the collapse
  call and its cleanup, the ReleaseAoe burn, the pet's element, and the R1 cast (the between-swings call, the pending
  strike guard, `summonsOnly`, the DataStore exemption). Totals after the review: `corpses_test.cpp` 32 cases / 3071
  assertions (GCC = Clang); 778 cases (GCC 216288 / Clang 216224; shared Clang 773 / 216098); the replay digest is
  re-pinned to `022a6e97d326a70c` (GCC = Clang: the raiser of that fight now casts between swings and the eater waits
  for its swing); one existing case changed its event loop, not its assertions (the raise caps case reads the log in
  order, since a cast may now start in the step its predecessor stood its bodies up). The 20-seed playthrough dumps
  (base 1000) stay byte-identical to 2A's on GCC and Clang (no shipped definition has a raise, summon or consume yet).
- **For 2F (content).** Corpse blocks per family: Unburied / Living `corpse`, the gnawers `remains` (+ `consumes` on the
  gravegnawer), Breach-blood `none`, the Corpseherd and Valdras `remains` (their bodies are story beats). The footman's
  trait `riseAgain {chance .25, delayMs 3000, hpPct 40, notOnCrit}`; the cinderbloat's burst (800, r2.2, 18 fire,
  `hitsMonsters`, x2, cap 20); Scorched / Gravecold `onDeath` and the `unburied` affix row (`families: [unburied]`;
  `sys.eliteAffix.name.unburied` in zh-CN / en; note the Corpseherd is Unburied family and would roll it unless the row
  says otherwise); the Sheepfold's six `corpseSeeds`; Call the Flock (`raise`, count 1 / 8 s, `capAlive` 3,
  `perWindowCap` 1 per 8000, hpPct 40, plus the opening raise of two with a long cooldown and a higher priority) and the
  standard-bearer's Muster (`raise`, 10 s, `capAlive` 2); Valdras's P2 circles as a `summon` at `anchors`. Raises and
  summons are casts between swings (review R1): they need only `cooldownMs`, the caps and a `rangeMax` to the hero (say
  the aggro range); the opening raise of two fires on the fight's first step, from Chase. Feed (Valdras P2) is not `consumes` (a boss healing from Raised that die near the dais): K4.
  The art manifest's corpse blocks name the gnawers' family `gnawers`; GameData says `gravegnawers`.
- **For 2G (UE).** U3: hold the death pose and crumble to the husk / carcass on EvCorpseAdded (look by `artId` /
  `defId`), keep it per CorpseView, remove it on EvCorpseRemoved by reason (expired: fade; raised: remove, the risen
  monster's own Rise plays; consumed: after the Consume loop; burned: `corpse.burn`), clear all on zone unload; seeds show
  as husks at zone build. U2: draw raise / summon markers (TelegraphView kind Raise / Summon: breach-violet, DR5) and
  on-death fuses (source OnDeath: the crimson decal on the dead monster's spot, ended at contact); play `Rise` on
  EvPlayAnim{Rise} and `Consume` on EvPlayAnim{Attack, clip Consume} (looped for its durationMs, stopped by
  EvMonsterAttackCancelled); EvMonsterCollapsed is a fall without a kill (no loot beam, no kill vocal).

**2C outcomes (K4 as built, 2026-10; pending review; DECISIONS DR8 *K4 as built*, M1 amendment).**
- **Schema** (`MonsterData.h` `MonsterPhaseDef`, parsed by `LoadMonsters.cpp` `ReadMonsterPhase`, mirrored by the
  exporter's `tables/monsters.ts` `checkK4Monster`; unknown keys fail at every level). Monster `phases [{hpBelowPct,
  enterAction, speedMul, attackSpeedMul, damageTakenMul, leash {centre, radius}, abilitiesOn[], abilitiesOff[], summons[],
  arena {kind: ringShrink, centre, fromR, toR, overMs, dpsPctMaxHp, damageType}, feed {centre, radius, healPct, delayMs},
  stinger}]` as K4 "Data" lists, plus `feed` (below). Rules: thresholds strictly decrease in (0, 100]; `enterAction` is an
  AnimAction string but idle / walk / death (the K1 focus `anim` convention); multipliers in (0, 10], default 1; a tether,
  feed or summon ring within 30 tiles, an arena within 60 (`0 < toR <= fromR`: a ring shrinks or holds), `overMs` <= 10
  min, `dpsPctMaxHp` in (0, 100]; a summon `{monsterId, count 1..20, at: self | anchors, anchors[], radius, hpPct, delayMs
  <= 60 s, respawnMs <= 10 min, respawns 0..10}`; a feed `{radius, healPct, delayMs <= 10 s}`; `abilitiesOn` / `abilitiesOff`
  name the monster's own abilities, never both in one row; a `centre` is `{col, row}` (default: the monster's authored
  spawn tile, `MonsterInstance::homeTile`: the dais; 2C review). `DataStore::Finalize`: phase summon ids exist; warnings
  for an ability whose `phases[]` lists only phases the monster never reaches (the exporter warns too), for an enter
  action whose clip the monster's art lacks (it then roots for `phaseActionMsDefault`, 1200 ms) and (2C review; the
  exporter's `warnTetherPastLeash` too) for a tether that can reach past the M1 leash range from the spawn anchor: the
  centre's farthest offset from it (0 for a default centre on a non-map spawn; + spawnJitter x sqrt 2 for a map spawn,
  whose anchor is jittered; the distance to each known spawn for an authored centre) + radius + 0.25 slack > leashRange. Exporter: `REMAKE_SCHEMA` gains `monster.phases`; the export stays byte-identical (no
  GameData record has phases; 2F authors them).
- **Indices.** `MonsterInstance::phase` counts the rows entered: 0 = the monster as defined (Valdras's P1), row i entered =
  phase i + 1 (P2 = 1, P3 = 2; the zealot's frenzy = 1). An ability's `phases[]` (K2) lists these numbers; EvMonsterPhase
  carries them.
- **Runtime** (`monsters/Phases.h` pure, `Private/monsters/MonsterPhases.cpp` MonsterSystem; Combat.h / MonsterSystem.h
  *K4* notes). `MonsterSystem::TickPhases` runs in GameSim right after `TickCorpses` (all of the step's damage is in; no
  RNG): (1) every living monster with a table enters each row its life crossed, one at a time, in order (`hp < hpBelowPct`
  strictly, the abilities' rule; a hit through two thresholds enters both, in that step; a kill enters none; nothing goes
  back but the reset); (2) arenas tick; (3) queued phase summons stand up; (4) Feeds come due. Entering a row: its
  multipliers and tether replace the previous row's (x the affixed definition: `MonsterMoveToward` top speed,
  `MonsterInstance::AttackIntervalMs` = attackSpeed / attackSpeedMul for the swing scan and its anim timing (also the
  pet's stray swing), `PhaseScaledDamage` on every hit in `CombatSystem::DamageMonster` - hero, pet, DoT tick,
  environment - floor(amount x mul), never below min(amount, 1)); the switches apply in row order (an ability whose first
  switch is an `abilitiesOn` starts off; one the entry makes usable is ready `initialCooldownMs` after the entry when the
  fight's ability clock runs; the picker reads `AbilityPickInput::off`); with an `enterAction`, the monster's swing or
  ability on its way to contact, a swing drawn to the beast (2C review: a pet timer, which `CombatSystem::CancelMonsterStrike`
  cancels through `PetCompanion::CancelMonsterSwing`) and a consume channel are cancelled (`CombatSystem::CancelMonsterStrike`,
  EvMonsterAttackCancelled), then EvMonsterPhase, then `EvPlayAnim{enterAction, clip}` (the clip by the action's name:
  Roar, Taunt, Cast02, ...; Attack01 / Cast01 for Attack / Cast) and the monster is rooted - no AI step, swing or cast - for
  the clip's length (`assets.json`), else `phaseActionMsDefault`; every hit lands meanwhile (monsters.md 1.3: no invulnerable
  phase); its arena lights (EvArena; a later row's arena replaces it, a row without one keeps it); its summons are queued.
- **Tether** (`leash`; MonsterAI): while the row holds, a chase or patrol goal outside the circle becomes the circle's
  point nearest it (`TetherGoal`), so the monster waits at the edge nearest the hero, and it attacks only from inside
  (0.25 tiles of slack): one standing outside when the row starts walks back in first. A row without `leash` frees it.
- **Arena** (ringShrink): radius `fromR + (toR - fromR) x clamp(t / overMs)` about its centre; every `arenaTickMs` (500)
  from the entry the hero strictly outside the radius at the tick's time takes `CombatSystem::ArenaTickHero` =
  floor(maxHp x dpsPctMaxHp / 100 x 0.5 s x the difficulty's damage multiplier) (>= 1), less its resistance, capped by the
  monster's hit cap; a DoT-like tick (no roll, defense or i-frames) that Death Save holds (monster damage, as K2's hazard
  ticks). It ticks only while its monster fights (Chase / Attack) and never on a hero inside a camp's safe zone. Monsters
  are never hurt by it. It goes out (EvArena{cleared}) when its monster dies, collapses or resets.
- **Summons** stand up through K3's `SummonAt` (Raised, raiser = the monster: no rewards, no respawn, they collapse with it;
  EvMonsterRaised `abilityId` "phase.<n>") on a ring of `radius` around the monster (slot k of `count` at 2 pi k / count;
  default 1.5) or on its `anchors` (round robin), `delayMs` after the entry (a row entered in a hit through two thresholds
  still stands its bodies up); a body killed while its row is still the current phase stands up again `respawnMs` later in
  its slot (the same anchor; the same ring slot around where the monster stands then), at most `respawns` times. They
  count toward the monster's own raise / summon `capAlive`.
- **Feed** (the 2B handoff; chapter1.md 11): when a Raised is killed (a kill, not a collapse) within a feeding row's `radius`
  of its centre, the first such monster (list order) holds its corpse (EvMonsterFeed {entity, fed, corpseId, pos, startMs,
  dueMs}); `delayMs` later, if fire, the TTL or the cap has not taken it, EvCorpseRemoved{consumed, by} and the monster heals
  floor(maxHp x healPct / 100). A fire killing blow (the body burns at once) or a fire over the held corpse denies it
  ("burn their corpses"); the feeder's death or reset lets the corpse go. Its own abilities do not (2C review: an
  ability's contact, abort or cancel lets go only of the corpses that ability held, `CombatSystem::ClearPendingAbility`),
  so no eater or raise takes the body meanwhile.
- **The reset** (M1 amendment): a monster with a phase table walks home at its normal speed (2C review: a row's `speedMul`
  does not apply while Returning, M1) and, when it arrives (Returning ends), heals to full and goes back to phase 0 (EvMonsterPhase{0, reset} when it had entered a row), its multipliers, tether, switches and root back to phase
  0, its arena out, its queued summons and Feeds dropped, every Raised it stood up collapses. A zone change drops arenas,
  queued summons and Feeds silently.
- **Events** (appended to `abyss::Event`; the UE router needs no change): `EvMonsterPhase {entity, phase, phaseCount,
  hpBelowPct, stinger, reset}`, `EvArena {entity, kind, centre, radius, targetRadius, startMs, endMs, tickMs, dpsPctMaxHp,
  damageType, cleared}` (the plan's {centre, radius, targetRadius, endMs} plus what UE needs to draw and clear it),
  `EvMonsterFeed`. Snapshot: `MonsterView::phase`, `arenas` (ArenaView: the radius now), `BossBarView::phase` and
  `phaseThresholdsPct` (the bar's ticks).
- **Deviations / interpretations** (all pending review). (1) `leash` is a tether, not the M1 leash: a Returning leash at r3
  would have reset the fight (K4's own reset rule) every time the hero stepped off the dais; "he holds the dais" / "leaves
  the dais" are a tether on P2 and none on P3. (2) Multipliers are per row, x the base: P3's "move x1.15" is not x P2's 0.6.
  (3) Damage taken applies to every hit through `DamageMonster` (DoT ticks too, unlike the web's death-mark amplify), so the
  zealot's "+10 %" is felt by every class. (4) The enter action cancels a windup (a phase change interrupts) and roots the
  monster; a row without an action changes nothing in flight. (5) An arena ticks only while its monster fights and not on a
  hero in a camp (a boss that lost the hero would otherwise burn it anywhere in the zone); a later row without an arena
  keeps the earlier one. (6) The arena's damage scales with the difficulty's damage multiplier (like K3's `dmg`). (7) The
  reset also collapses the monster's Raised and drops its queued work (the fight it started is undone). (8) Two schema
  additions the plan did not spell: `feed` (2B handed Feed to K4) and the phase summon's `delayMs` / `respawnMs` /
  `respawns` (chapter1.md 11: "one new bearer rises 20 s later, once"). (9) The K2 `anchor` aim keeps the dais, not
  the tether's centre: every K4 default centre and the `anchor` aim are the monster's authored spawn tile
  (`MonsterInstance::homeTile`; 2C review: they used to read the spawn anchor, which M10 jitters up to 3 tiles per axis
  per seed for a map spawn, so the dais moved off its art and its ash circles), so the dais is one fixed point. The M1
  leash and the M3 respawn keep the jittered spawn anchor (no RNG draw changed: the playthrough is byte-identical). (10) A row's switches use "first switch" order, so an ability
  can be on, off, then on again. (11) Only the M1 walk home resets: a monster that drops the fight where it stands (the
  hero died, ran past 1.5x its aggro range inside the leash range, or reached a camp) keeps its phase, life and arena,
  and the arena does not tick until it fights again. (12) The attacker's side of a hit (life / mana steal, computed by
  `CalculateDamage`) reads the hit as dealt, before the target's `damageTakenMul`; the numbers, the HitWeight and the
  life lost read it after. (13) A row entered during an earlier row's enter action plays its own action at once and the
  root runs to the later of the two ends.
- **Tests.** `phases_test.cpp` (suite `monsters`; 30 cases, 4690 assertions on GCC and Clang since the 2C review): the
  schema (every field and its defaults; 50 refused rows; Finalize: summon ids, unreachable phases, the enter action's clip;
  the tether past the M1 leash and a hunt's unreachable phases), the pure rules
  (entry order, switches, the picker skipping a switched-off ability, the arena radius / edge / tick, the tether goal,
  scaled damage, clips), and the runtime: single in-order entry (the threshold itself, a heal back, a hit through two and
  through three, a kill), multipliers on top of Swift (the first moveToward step, swing starts every 1300 / 1.35 ms with
  the anim timed on it, hits and ticks x1.1, the HitWeight of the scaled hit; a second row's multipliers x the base),
  a pet's hits (through CombatSystem and through PetCompanion::Hit's fallback) and a swing drawn to the beast (timed on
  the row's interval; an enter action cancels it), switches (an
  ability off then on, ready `initialCooldownMs` after the entry, a `phases[]`-listed one; off and on again inside its
  cooldown; a summon cast between swings switched on by a row), the enter action (cancel, Roar for the clip's length, rooted, no swing / cast / hit, every hit
  lands; a row without one; the default length; a consume channel ended; a row entered during another's action and the
  later end of the root), summons (anchors, the delayed ring body, one replacement, none after its phase, collapse on
  death; a hit through the summoning row and the next), the arena (EvArena / ArenaView and a reused snapshot, inside /
  outside, four ticks in 2 s less resistance, the radius at the tick's time, toR after overMs, idle, a camp, cleared at
  death; the difficulty multiplier, the hit cap, Death Save, a tick resistance rounds to 0, a zone transition; a later
  row's arena replacing it, a row without one keeping it, a collapse putting it out), the reset (full life, phase 0,
  switches, EvArena{cleared}, a collapse, the Feed let go, queued work dropped, re-entry, the walk home at normal speed;
  home before the first row: full life, no event; a monster without phases unchanged), the tether (the edge nearest the
  hero, following it, walking back in, the 0.25 slack, the path dropped when the tether changes, a later row freeing
  it; a patrol goal), Feed (the hold, the
  delay, the heal; range, fire killing blow, burned while held, the feeder's death, phase 0, an authored centre; the
  hold kept through the feeder's ability contacts and a later row's enter action, an eater kept off), the dais (default
  centres and the `anchor` aim on the authored tile, kept by a respawn), the zealot's one-row frenzy, a zone exit, the
  GameSim wiring (TickPhases in the step, MonsterView / ArenaView / the boss bar's ticks on the story boss given a table
  in memory; on four seeds, the story boss placed by the zone population keeps its authored tile as every default
  centre while M10 jittered its spawn anchor on at least one), and
  a seeded 40 s boss fight whose event-log digest is pinned (`4567bd8648a82662`, GCC = Clang). Each rule was also
  checked by mutating the core in a scratch copy: 42 of 43 mutations (entry, multipliers, switches, the root, the
  arena's edge / idle / camp / difficulty / cap / Death Save / resistance / replacement / collapse, the reset, Feed,
  respawns, the tether on chase and patrol, the loader, the snapshot) make a K4 case fail; the 43rd (a body killed after
  its phase ended is queued for a replacement anyway) is equivalent, because the due-time check drops it. The 2C review
  added 22 mutations (each review fix undone, and the review's survivors: the pet fallback and the pet's stray-swing
  timing, the first stand-up of a row passed in one hit, the HitWeight under damageTakenMul, readiness across off / on,
  a reused snapshot, the hunt Finalize pass, the transition guard and the min-1 tick, the slack, the path re-plan): all
  22 make a K4 case fail. Five more are equivalent and stay uncovered: the arena's radius read at `now` instead of the
  tick's time (500 ms is exactly 30 steps of 1000 / 60 ms, so a tick is handled on its own step), one tick per step
  instead of four (no step spans two 500 ms ticks), readiness written while the ability clock is stopped
  (`StartAbilityClock` rewrites every ability when the fight starts), and a collapse or a dead feeder keeping its queued
  work (the collapse already lets every hold go, and the due-time checks drop a dead owner's work silently). Existing
  tests: no assertion changed. The 20-seed playthrough dumps (base 1000, and base 3000 since the review) are
  byte-identical to a build of 4a796202 on GCC and Clang.
- **2C review fixes** (2026-10, pending review). (R1) Every K4 default centre and K2's `anchor` aim read
  `MonsterInstance::homeTile`, the authored spawn tile (`MonsterSpawnParams::hasHome` / `home`: SpawnZonePopulation passes
  the map spawn's tile; other spawns stand on theirs; a respawn keeps it), not `spawnAnchor`, which M10 jitters up to 3
  tiles per axis per seed: the tether, the Feed zone and the ring used to sit off the dais and its ash circles, by a
  different amount each visit. No RNG draw changed. (R2) `CombatSystem::ClearPendingAbility` lets go only of the
  ability's own corpses (`MonsterSystem::ReleaseCorpse`; `ReleaseCorpses(holder)` is gone): an ability's contact, abort or
  cancel used to drop a Feed's hold, so an eater could start eating the fed body. (R3) An enter action also cancels a
  swing drawn to the beast (`PetCompanion::CancelMonsterSwing`, called by `CombatSystem::CancelMonsterStrike`): it is a
  pet timer, not a strike, and used to land on the beast mid-Roar. (R4) Cases for the review's surviving mutations (the
  Tests bullet). (R5) The walk home ignores the row's `speedMul` (M1: normal speed). (R6) A tether that can reach past
  the M1 leash range is a DataStore and exporter warning (Schema bullet). (R7) CORE_OWNERSHIP lists the K4 files.
- **For 2F (content).** Valdras: P2 = `{hpBelowPct 65, enterAction roar, speedMul 0.6, leash {radius 3}, summons
  [{the bearer (a dedicated definition whose summon ability raises at the two ash circles every 10 s, capAlive 4), at
  anchors, respawnMs 20000, respawns 1}], feed {radius 4, healPct 3, delayMs ~1000}, stinger stinger_valdras_roll_call}`;
  P3 = `{hpBelowPct 30, enterAction roar, attackSpeedMul 1.25, speedMul 1.15, abilitiesOn [lunge], arena {ringShrink,
  fromR 9, toR 5, overMs 40000, dpsPctMaxHp 4, damageType fire}, stinger stinger_valdras_no_retreat}` (Lunge's
  `abilitiesOn` keeps it off in P1 / P2; Breach-seep in P2 via `phases [1]`). The zealot: one row `{hpBelowPct 50,
  enterAction roar, attackSpeedMul 1.35, speedMul 1.2, damageTakenMul 1.1}` (its Roar clip carries FX_Frenzy). Valdras's
  centres can stay unauthored: each defaults to his map spawn tile (15,95), the dais, wherever M10's jitter stands him; a
  default-centred P2 tether up to r 3 stays inside the M1 leash (r 4 warns: 4 + 3 x sqrt 2 + 0.25 > 8). The bot
  (AutoPilot) should stand inside `arenas` and kill adds away from a feeding boss's centre.
- **For 2G (UE).** Play `stinger` and the phase look on EvMonsterPhase (drop it on `reset`); draw EvArena / ArenaView
  (a ring that closes from radius to targetRadius by endMs; remove it on `cleared` and on zone unload); EvMonsterFeed: the
  corpse drawn into the monster until dueMs (then EvCorpseRemoved{consumed}); boss bar ticks from
  `BossBarView::phaseThresholdsPct`.

**2D outcomes (K5 as built, 2026-10; pending review; DECISIONS DR8 *K5 as built*, M1 / M2 / M5 notes).**
- **Schema** (`MonsterData.h` "K5", parsed by `LoadMonsters.cpp` `ReadMonsterK5`; `MapData.h` `MapSpawnDef::mix` /
  `leader`, parsed by `LoadWorld.cpp`; mirrored by the exporter's `tables/monsters.ts` `checkK5Monster` and
  `tables/maps.ts`; unknown keys fail at every level). Monster `packAggro` (tiles, (0, 30]), `kite {min, max}` (0 < min
  < max <= 30), `block {arcDeg (0, 360], damageMul [0, 1), wall?}` (wall in (0, 30] tiles), `morale {fleeOnLeaderDeathMs
  (0, 60000], fleeRadius?, sustainFamily?, sustainRadius?}` (radii in (0, 30]; a sustain family needs its radius), `aura
  {stat, mul? (0, 10], radius (0, 30], family?}`. The K2 buff block: a `buff` names its `stat` and `ms` (<= 600000),
  `mul` in (0, 10], `radius` 0..30 (0 = the caster alone); a `call` needs its block for a `radius` > 0 and buffs only
  when it names a `stat` (with its `ms`; `ms` / `mul` alone are refused). Map spawn `mix [{monsterId, count >= 1}]` and
  `leader {monsterId, elite?}`. `DataStore::Finalize`: a mix sums to the entry's `count`, names monsters and never the
  story boss (its once-per-visit spawn reads the entry's `monsterId`), and the entry's own `monsterId` is one of its
  members (2D review: the story-boss spawn and QuestWorld's guide spawn points read it); the leader is a member (the
  entry's own monster without a mix) and not the story boss; warnings for a kite on a melee monster (attackRange <=
  rangedThreshold) and for a kite `max` past the monster's `attackRange` (2D review: a kite runs in Chase and ends in
  Attack only with the hero within attackRange, Chase's own entry rule, so a band edge between attackRange and the swing
  reach x attackExitMul still walks it back in before it shoots; the exporter fails on both); buff / call join raise /
  summon outside the swing-reach rules (casts between swings). A sub-dungeon spawn takes no `mix` / `leader` until K7
  (the loader refuses them: nothing spawns sub-dungeons yet and the pack checks read the maps only; its spawns come from
  the web source, so the exporter cannot write one). Exporter: `REMAKE_SCHEMA` gains `monster.packAggro` (a new field
  row), `monster.kite`, `monster.block`, `monster.morale`, `monster.aura`, `map.spawnMix` and lifts the value gates
  `monster.abilityBuff` / `monster.abilityCall`; the export stays byte-identical (no GameData record uses them; 2F
  authors them). Not generator inputs: the golden maps do not move.
- **Packs** (`monsters/Packs.h` pure rules, `Private/monsters/MonsterPacks.cpp`; MonsterSystem.h "K5"). A map spawn
  entry is a pack (monsters-ai.md 6.1): `SpawnZonePopulation` gives each entry a pack id (`MonsterInstance::packId`, 1,
  2, ... per zone entry) and spawns its members in mix order (`ExpandSpawnMembers`; no mix: `count` x `monsterId`), each
  placed by the same M10 draws as before, so a mix of the entry's count stands where the plain entry stood
  (golden-neutral, positions unchanged; pinned). The leader is the first member of the leader's id
  (`MonsterInstance::packLeader`); `leader.elite` makes that member elite (it rolls the zone's affixes right after its
  placement, as an elite definition does; its `originalDef` stays elite, so a respawn rolls again). That roll draws on
  the Ai stream, which also places the zone's monsters, so an elite leader moves every later placement of the zone and
  the hunts drawn after the population, exactly as authoring an elite definition would (2D review: not golden-neutral;
  pinned: the placements up to and including the leader's stay, later ones move). A hunt's leader leads its minions'
  pack. A respawn keeps its pack and leadership. Event spawns (ambush, rescue, defend waves), Raised bodies and phase
  summons have no pack.
- **Pack aggro.** A member with `packAggro` is provoked (M2 `ProvokeMonster`: idle / patrolling, or chasing with the
  hero beyond its aggro range; MonsterAggroMsg when it leaves Idle / Patrol) when another living member of its pack
  within its `packAggro` tiles is provoked by a hit (`ApplyDamage`), enters Chase from Idle / Patrol / Returning
  (`TickAI`) or is forced to (`ForceChase`): one wave per trigger (`MonsterSystem::PackAlarm`), never passed on by a
  member it provoked, nor by a call; a Returning member ignores it (M1). Without `packAggro` in the data nothing happens
  (no RNG, no state).
- **Morale and Flee** (`MonsterState::Flee`, appended after Dead; enum strings `... "dead", "flee"`). The kill
  pipeline's K5 step (`MonsterSystem::OnMonsterDied`, after the K3 / K4 steps): a pack leader's death breaks its pack:
  each living member with `morale` (list order; not one walking home or already fleeing: a second break leaves a flee
  under way alone), within `fleeRadius` of the body (none = the whole pack), neither fearImmune (a live fearImmune buff
  or aura, `MonsterFearImmune`) nor `Sustained` (a living monster other than itself, of `sustainFamily`, any when empty,
  with a buff / call that grants fearImmune, within `sustainRadius`), flees (the step first drops every buff the dead
  monster gave, `DropBuffsOf`: a buff ends with its caster, so the drum holds the deserters only while the drummer
  lives): its swing or ability on the way, a swing drawn to the beast and a consume channel are cancelled
  (`CombatSystem::CancelMonsterStrike`, EvMonsterAttackCancelled), state Flee until `fleeOnLeaderDeathMs` from now, away
  from the body (EvMonsterFlee). The AI (`MonsterFleeStep`): each tick the first of the away vector (from the body; on
  the body: from the hero) and it turned 45 and 90 degrees each way whose point `fleeProbeTiles` (1) ahead is walkable,
  in a straight walkable line, inside the M1 leash range and the K4 tether, outside every camp's safe zone (point and
  line, `MonsterWorld::safeZone`; 2D review), at least `fleeMinGainTiles` (0.1) farther from the body, and not turned
  more than 120 degrees from its last flee step (`MonsterInstance::fleeDir`; 2D review: at a curved edge, a tether, a
  camp, the two 90-degree sidesteps took turns being open and the monster dithered on the spot for its whole flee);
  none, or a step that does not move it: it fights instead (Chase, EvMonsterFlee{ended, blocked}); its time out: Chase
  (expired). `TickAI` ends a flee a sustain or a fearImmune buff now steadies (sustained) and one that reaches a camp's
  safe zone (Idle, interrupted; the leash too). A fleeing monster stays in the activity set (like Returning), never
  swings, casts or eats, is not aggro, and a hit does not stop it (M2 provokes ignore Flee).
- **Kite** (MonsterAI.h "K5"). In Chase or Attack, with the hero visible and closer than `kite.min`, no strike of its
  own on the way (`MonsterAiInput::strikePending`) and a strike since its last kite (`kiteNeedsSwing`, `lastStrikeMs`: a
  swing at the hero or a beast, or a damaging ability; 2D review: a support cast between swings sets `lastAttackMs` but
  is no strike, so a raiser that kites, e.g. the Corpseherd, cannot kite-cast-kite without shooting), a monster picks
  its kite target (`MonsterKiteTarget`: from `kite.max` tiles from the hero down to its distance + `kiteSampleTiles`
  (0.5), every 0.5, and at each distance the away vector then it turned 30 and 60 degrees each way about the hero: the
  first point on a walkable tile, inside the M1 leash range and the tether, outside every camp's safe zone, reached in a
  straight walkable line that stays out of the camps; so it keeps the band's far edge and circles the hero when straight
  back is closed) and walks there in Chase (no movement on the tick it starts; `MonsterView::kiting`); it ends on
  arrival, when blocked, or at `kiteUntilMs` (the later of `kiteTimeoutMs` 2000 and twice the straight walk at its top
  speed): Attack when the hero is in reach, else Chase. No target: it fights. Raise / summon / buff / call casts may
  start while it kites.
- **Block** (`CombatSystem::DamageMonster`). A hit on a monster with `block`, not a tick, with a direction (`hasFrom`,
  `from` off the monster), from within `arcDeg` (total) around its heading (`BlockArcContains`; the edge counts), and
  the monster not stunned or frozen, is `BlockedDamage`d (floor(amount x damageMul), at least min(amount, 1)) before the
  K4 damageTakenMul and emits EvBlock {entity, source, from, amount (the damage it took), blocked (what the shield
  stopped, in the same units: both after damageTakenMul; 2D review)} right before its EvHit (which carries the reduced
  amount; the HitWeight is classified on it; life / mana steal read the hit as dealt, as for damageTakenMul). Any
  attacker counts: a beast or another monster's strike behind it gets through (flanking). A fighting blocker squares up
  to the hero every AI tick (Attack; there is no sim turn rate, M11 is presentation only), so the hero alone cannot
  flank a lone fighting shieldbearer: flanks come from a beast, another monster's strike, a blast centred behind it, or
  a blocker that is not fighting (2D review). **Wall**: after the AI loop (`AlignShieldWalls`), living blockers with
  `block.wall` that are fighting (Chase / Attack; 2D review: a neighbour walking home, fleeing or idle turned a
  fighter's guard away from the hero) and stand within the smaller of their walls of each other (chained, list order)
  share one heading, the mean of theirs (`WallHeading`; opposite headings: the first's); a lone one keeps its own.
- **Buff / call abilities** (`AbilityKindRuns`: every kind runs). Like raise / summon they are support casts: on a due
  swing they compete by priority, and between swings, from Chase or Attack, `CombatSystem::TryStartSupportCast` (the K3
  `TryStartSummonCast`, renamed; `AbilityPickInput::castsOnly`, the K3 `summonsOnly`, renamed;
  `AbilityKindCastsBetweenSwings`) starts them on their own cooldown and hero range, not bound by the swing reach; one
  action at a time, a pet's taunt holds them, every K2 abort applies; no telegraph (the clip is the tell). At contact
  `MonsterSystem::ResolveBuffCast`: within `buff.radius` of the caster (list order; radius 0 = the caster alone) a call
  provokes (M2) every other living monster of its `family` (no pack alarm), and a buff with a stat gives each living
  monster of its family, the caster included, `stat` x `mul` until now + `ms` (one buff per caster and stat: a recast
  refreshes it; the target's multipliers refresh at once; the buff ends early when its caster dies); EvMonsterBuffCast.
  Stats: damage (the damage formula's outgoing multiplier on the monster's swings, ability contact hits on the hero and
  on monsters, and bolts, read at the hit: `MonsterDamageBuffMul`; not its hazards' or on-death effects' fixed damage,
  nor its hits on the beast), attackSpeed (`MonsterInstance::AttackIntervalMs` / mul), moveSpeed (`MonsterMoveToward`'s
  top speed, also while Returning: not a phase rule), fearImmune (morale). Buffs of one stat do not stack: the largest
  live buff or aura applies.
- **Auras** (`aura`). At the start of every `TickAI` each living aura owner (list order) reaches every living monster
  within its radius, itself included, of its family: per stat the largest aura holds (`MonsterInstance::auraMul` /
  `auraMask`); then every monster's multipliers are refreshed from its live buffs and auras (`RefreshMonsterBuffs`,
  expired buffs dropped) and hold for the step. A dead owner's aura is gone the next step.
- **Events** (appended to `abyss::Event`; the UE router needs no change): `EvBlock {entity, source, from, amount,
  blocked}`, `EvMonsterFlee {entity, leader, from, startMs, untilMs, ended, reason: expired | sustained | blocked |
  interrupted}` (`FleeEnd`, SimTypes.h; `leader` / `startMs` on the start), `EvMonsterBuffCast {entity, abilityId, kind,
  centre, radius, family, hasBuff, stat, mul, untilMs, buffed, called}`. Snapshot `MonsterView`: `packId`, `packLeader`,
  `fleeUntilMs` (while fleeing), `kiting`, `buffMask` (bit per MonsterBuffStat raised now), `auraRadius` / `auraStat` (a
  living owner's halo), `blockArcDeg` (its guard faces `heading`, which a wall shares). Port-only tuning
  (`MonsterAiDef`, not loaded): `kiteTimeoutMs` 2000, `kiteSampleTiles` 0.5, `fleeProbeTiles` 1, `fleeMinGainTiles` 0.1.
- **Deviations / interpretations** (all pending review). (1) Pack aggro is opt-in per monster (`packAggro`, a field the
  plan did not list): the plan's "a member provoked or entering Chase provokes members within 6 tiles once" applied to
  every pack would change every existing spawn entry's fights (the playthrough must stay byte-identical until 2F) and
  monsters.md gives it to the gravegnawers; the radius is the listening member's (6 in the design), "once" = one wave
  per trigger, never chained. (2) `morale.fleeRadius` is added ("every deserter within 8 tiles", monsters.md 2.3); none
  = the whole pack. (3) A sustain member is a living monster of `sustainFamily` (any when empty) whose buff / call
  grants fearImmune (the plan's "the drummer's call buff fearImmune"); the deserters and the drummer share the Living
  family, so the family alone cannot single the drummer out; a live fearImmune buff or aura also holds a monster. (4)
  Flee is bounded by the M1 leash range (a flee into the leash would walk the monster home and heal it to full: a reward
  for breaking it), and a flee step must gain 0.1 tile (a 90-degree sidestep always gains a hair, so a monster at its
  leash edge would orbit the body for its whole flee) and never turns more than 120 degrees from its last step (2D
  review: at a curved edge the two sidesteps took turns and the monster dithered in place). (5) A kite needs a strike
  between two kites ("then attacks": a swing or a damaging ability, not a support cast), keeps to the band's far edge by
  turning up to 60 degrees about the hero before it shortens, and does not start while its own strike is on the way; it
  runs in Chase (UE's walk). (6) Block: ticks, hits without a direction and stunned / frozen blockers are not blocked
  (the core's "shield-breaking" until a skill names it: no skill data flags one); every attacker counts (a beast behind
  it flanks); the hero alone cannot flank a fighting blocker (it squares up to him every AI tick; a sim turn rate would
  be a design change). (7) `wall` is a radius (2 in the design) and the shared heading is the mean of the fighting
  (Chase / Attack) members' headings. (8) buff / call are support casts between swings (like raise / summon since the 2B
  review): the drum is timed ("2 s every 8 s"); a call needs a radius and may buff (`stat` optional), so "every slam
  calls" (Hungerbell) is a separate call ability, not a slam option. (9) `aura` (plan 2.6 lists the field with no K
  package; 2F authors "auras") lands here: it shares the buff stats. (10) An elite leader (`leader.elite`) is the Hollow
  Watch's "elite leader with one affix"; the ambush Muster (a standard-bearer and three footmen sharing one affix, 10 %
  of ambushes) is not built here: it needs an event-spawn pack with a leader, a shared affix roll and a new Events draw,
  core work that 2F (content only) cannot do, so the 2D review assigns it to 2E (K6 "Event champion packs"; cut order:
  champion packs go third). (11) Event spawns have no pack (until K6 builds the Muster). (12) `MonsterState::Flee` is
  appended after `Dead` (the numeric values of the others stay). (13) A flee or a kite never steps into a camp's safe
  zone (2D review: the repel would drop it to Idle and the aggro scan put it back in Chase on a no-movement tick every
  step, frozen at the camp's edge while the hero outside struck it for free). (14) A buff ends with its caster (2D
  review: "while he drums", monsters.md 2.3; the drum outliving the drummer silently held the pack when the leader fell
  soon after).
- **Tests.** `packs_test.cpp` (suite `monsters`; 29 cases, 9170 assertions since the 2D review, GCC = Clang): the schema
  (every block and its defaults; 35 refused monster blocks and 3 refused stats; spawn mix / leader loads and 11
  refusals; the Finalize warnings, buff / call outside the reach rules), the pure rules (members and the leader, the
  block arc front / edge / side / back / no direction / 360, blocked damage, the wall heading, buff multipliers and the
  mask, families and sustain casters, the cast-between-swings kinds; the kite target in the open, a corridor, the leash,
  the tether, a block behind it, a dead end, no gain; the flee step away, turned, cornered, a probe past a one-tile
  wall, on the body, at the leash), and the runtime: the population (a mix stands where the plain entry stood, one pack
  per entry, the leader, an elite leader's affixes), hunts (the leader leads; a hunt's minions break), a respawn keeps
  its pack; pack aggro (radius per member, no chain, no packAggro (one standing on the provoked one's spot included), no
  pack, another pack, a repeated provoke, the AI's scan, ForceChase, Returning); morale (flee time, the body, direction,
  no swing, a cancelled swing, fleeRadius, a member without morale, one walking home, a non-leader's death; an ability
  windup and a consume channel cancelled, a wounded eater does not stop to eat, a hit does not stop it; cornered:
  blocked; the leash bound without a heal; a camp: interrupted); sustain (a drummer holds and steadies, kill order,
  another family, no sustainRadius, a fearImmune buff and aura); the kite (start, target, arrival, a swing first, not
  during its strike, a second target circling the hero, a dead end, the leash); block (front, 45 degrees, side, 63
  degrees, back, a tick, no direction, a beast behind and in front, before damageTakenMul, min 1, stunned, in the
  fight); the wall (chained, a narrow wall, no wall, the block reads it, the snapshot, a fallen link); the drum (a cast
  between swings out of reach, contact at the Release, no telegraph, call / buff counts, family, the call passes no
  alarm, fearImmune and its expiry, the cooldown); a buff (attack speed, radius 0, a recast refreshes, a stun aborts, a
  contact refreshes the multipliers and the snapshot at once); auras (owner included, family, radius, the largest, a
  dead owner, attack and move speed, the damage reaching its blows); the GameSim wiring (a mixed pack with an elite
  leader through the population, the snapshot, the kill pipeline breaking it, a provoke alarming it, the quest guide's
  spawn points for mix members); the 2D review's cases (camps closed to a flee and a kite with the hero just outside, a
  kiter pushed into a camp; a buff ending with its caster, the drummer then the leader; a drummer not holding itself; a
  flee bounded by a tether, ending blocked without dithering; the leash interrupting a flee; a second break, and a later
  flee starting afresh; a kiting archer that breaks; the kite's end inside / outside a tether; a support cast that is no
  strike; a damage buff on hitsMonsters ability hits; fighting-only walls next to a returning and an idle blocker;
  EvBlock's units; the kite band past attackRange; a stale mixed monsterId and a sub-dungeon pack refused; an elite
  leader moving the later placements); a seeded 40 s pack fight whose event-log digest is pinned (`3126917caf246261`
  since the 2D review, `99df5ac2d13e51bc` before; GCC = Clang; all three members now break: the drummer falls first).
  Existing assertions changed where the plan says behaviour changes (buff / call run): monster_abilities_test.cpp "K2
  picker" (`PickMonsterAbility(later, in2) == -1` became `== 2`, the buff, and a new check keeps raise / summon unpicked
  without bodies) and corpses_test.cpp "K3 picker" (`CHECK_FALSE (AbilityKindRuns(k))` for buff / call became
  `CHECK(AbilityKindRuns(k))` plus `CHECK_FALSE(AbilityKindSummons(k))`). Mutation check (scratch copy, GCC): 83
  single-point mutations of the K5 core (pack alarm, morale, sustain, flee, kite, block, wall, buff / call, auras, packs
  and hunts, respawn, Finalize and loader checks, snapshot, consume) all fail a packs_test case (5 survived a first
  pass: a Returning member breaking, the flee's line of walk, a buff's immediate refresh, a fleeing eater, a coincident
  member without packAggro; each got the check above); the 2D review reverted each of its own fixes in a scratch copy
  (27 mutations): 25 fail a case and 2 are equivalent (the flee / kite point's safe-zone test, which the line's last
  sample repeats; the guide's own-monsterId match, which Finalize makes a mix member). Other suites: the world suite's
  K1 grep gate gains 3 assertions (one per new core source file). The 20-seed playthrough dumps (base 1000 and 3000) are
  byte-identical to a build of da0a3ab3 on GCC and Clang (before and after the 2D review).
- **2D review (2026-10; R1-R10).** Fixed: (R1) the flee step and the kite target treat a camp's safe zone as closed
  (point and line; `MonsterWorld::safeZone`): a monster fleeing or kiting toward a camp with the hero just outside used
  to walk in and freeze (repel to Idle, the aggro scan back to Chase on a no-movement tick, every step, never swinging).
  (R2) A buff ends with its caster (`DropBuffsOf` in the kill pipeline, before the morale step): the drum outlived a
  drummer killed just before the leader and silently held the pack. (R3) The kite band ends within `attackRange`, not
  the swing reach (DataStore warning, exporter failure, docs; the 2F handoff's Corpseherd band is [2, 3]). (R4) Walls
  form from fighting (Chase / Attack) blockers only. (R5) Cases for the documented rules a mutation pass found untested
  (above), which also found a flee dithering at a curved edge: a flee step never turns more than 120 degrees from its
  last one (`MonsterInstance::fleeDir`). (R6) `leader.elite`'s Ai-stream roll moves the zone's later placements: stated
  and pinned, not changed (an elite definition already does the same; a derived RNG stream would be a new convention).
  (R7) A mixed entry's `monsterId` must be one of its members (Finalize, exporter), QuestWorld's guide spawn points read
  the mix, and a sub-dungeon spawn refuses `mix` / `leader` until K7. (R8) The kite gate reads `lastStrikeMs` (a swing
  or a damaging ability), not `lastAttackMs` (which support casts set). (R9) The Muster's core is assigned to 2E (K6)
  and the docs state that the hero alone cannot flank a fighting blocker. (R10) EvBlock's `blocked` is in the units of
  `amount`; the unused `MonsterSystem::scratchMembers_` is gone. Not changed: the pre-existing safe-zone repel itself (a
  monster pushed into a camp still flip-flops Idle / Chase there; it is the web's rule and changing it could move the
  playthrough), and a support cast still restarts the swing clock (`lastAttackMs`, the K3 rule in Combat.h), so a cast
  whose cooldown is shorter than the swing interval starves the swings (2F: keep support cooldowns above the interval).
  The playthrough dumps stay byte-identical to da0a3ab3 (no GameData record uses a K5 field).
- **For 2F (content).** Gravegnawer `packAggro: 6` (Gnawed Field, the Crowmarket mix, Hungerbell's minions); the W2
  mixes of chapter1.md 17 (Weeping Ditch, Broken Redoubt, Dead Ranks with `leader: {monsterId: dead_standard_bearer}`,
  Deserters' Hollow with `leader: {monsterId: deserter, elite: true}`, Crowmarket); deserter `morale
  {fleeOnLeaderDeathMs: 5000, fleeRadius: 8, sustainFamily: living, sustainRadius: 8}` (Dreck's and Harke's minions
  break with it: a hunt's leader leads them); the drummer's drum: a `call` (clip Cast01, its 1000 ms loop; windupMs for
  the channel, `cooldownMs` 8000, `buff {stat: fearImmune, ms: ~2000, radius: 8, family: living}`) plus a `buff` of
  `attackSpeed` 1.1 (or fold the attack speed into an aura while it lives); the arbalester `kite {min: 2 or 3, max: 6}`
  (range 6: the band ends within it); the Corpseherd's "kite 3-5" does not fit her range 3.0 (the export fails a `max`
  past her attackRange): a band of [2, 3], or her range raised with an M5 amendment; the shieldbearer `block {arcDeg:
  100, damageMul: 0.5, wall: 2}`; the standard-bearer `aura {stat: damage, mul: 1.15, radius: 5, family: unburied}`;
  Hungerbell's social call: a `call` `{radius: 10, family: gravegnawers}` on its own cooldown (clip Attack02 or Roar).
  The zealot and the footman never flee (no `morale`). The bot (AutoPilot) should kill the drummer before the Hollow's
  leader (the drum ends with him). A mixed entry keeps a `monsterId` that is one of its members (Finalize and the
  exporter refuse a stale one); `leader.elite` moves the zone's later placements (expect the playthrough to move when
  the Hollow Watch lands); `content_ch1_test`'s "zone population" case matches monsters to entries by `s.monsterId`:
  read `MapSpawnDef::mix` there once mixes land. The ambush Muster's core is 2E's (K6); 2F authors its row.
- **For 2G (UE).** Treat `MonsterState::Flee` as moving (walk / run, face `heading`; today only Chase / Patrol /
  Returning set `bMoving`), and play the flee vocal ("走！") on EvMonsterFlee (not `ended`); a kiting monster is in Chase
  (it backs off facing away); spark the shield on EvBlock (the art's EvBlock VFX at socket `face`) with the clang; a
  blocker (`blockArcDeg` > 0) faces `MonsterView::heading` even in Attack (the wall's shared heading); draw the aura
  halo from `auraRadius` / `auraStat` (crimson for the banner), the buffed glow from `buffMask`, the drum / call pulse
  on EvMonsterBuffCast (breach-violet, DR5), and a champion marker on `packLeader`. EvBlock's `amount` and `blocked` are
  in the same units (amount + blocked = the unblocked hit).

**2E outcomes (K6 as built, 2026-10; pending review, 2E review fixes folded in; DECISIONS DR8 *K6 as built*).**
- **Schema** (all parsed with unknown keys refused at every level; mirrored by the exporter first). Status rows
  (`SkillData.h` `StatusStacking::StackCount`, `StatusStackRule`; `LoadClasses.cpp`): `status_effects.json` takes
  `stacking: stackCount` and a `stacks.<type> {maxStacks 1..100, healingReceivedPerStack [0, 1], slowAtMaxStacksPct [0,
  100]}` row; a `stackCount` type ticks no damage (`tickIntervalMs` 0, else refused). Quests (`QuestData.h`,
  `LoadQuests.cpp`): `setsFlags` (non-empty ids, each once; an empty list refused) and `rewards.permanent {maxLife
  0..1000, statPoints / skillPoints / respecTokens 0..100}` (at least one above 0: "grants nothing (omit it)"). NPCs
  (`NpcData.h`, `data/WorldStateData.h`): `lines [{key, when?}]` (non-empty keys, a non-empty list) and `visibleWhen`; a
  condition is an object of `flag`, `notFlag`, `quest` + `state` (`notStarted`, `active`, `completed`, `open`,
  `turnedIn`, `notTurnedIn`; a state needs its quest and a quest its state) and `any` (leaves naming exactly one of
  flag / notFlag / quest; no nesting), every key holding together; an empty condition or `any` is refused. Map spawns
  (`MapData.h` `MapSpawnVariantDef`, `LoadWorld.cpp`): `variants [{whenFlag, monsterId, count 0..100}]`, one per flag,
  map spawns only (a sub-dungeon spawn refuses them until K7, like K5's mix). Random events (`MapData.h`
  `ZoneEventDataDef::AmbushChampionDef`): `zones.<zone>.ambushChampion {chance (0, 1], mix [{monsterId, count >= 1}] (20
  members at most), leader? (a member's id), sharedAffix? (false), leaderLoot? (magic | rare; needs leader)}`.
  `DataStore::Finalize`: a `stackCount` status with
  its row needs `sys.statusEffect.name.<type>`; a monster `onHitStatus`, ability / hazard / on-death status (definitions
  and hunts), an elite affix's on-death status, a keyword rule or `eliteFrozenOnHit` naming a status that never lands (a
  `stackCount` type without its row) is an error; NPC line keys exist in zh-CN and en; a condition's quest exists; a flag
  that a condition or a variant reads and no quest sets is a warning (K7's cleanse may set one); a variant names a
  monster of its zone, never the story boss, and the story boss's entry has none; a champion member is a monster of its
  zone and not the story boss; the visibility validator (below). Exporter: `REMAKE_SCHEMA` gains `quest.setsFlags`,
  `quest.permanentRewards`, `map.spawnVariants`, `npc.lines`, `npc.visibleWhen`, `status.ashnumb` and
  `events.ambushChampion`; the last two are override-table features (`remake.ts` `REMAKE_TABLE_FEATURES` +
  `requireTableFeature`: `GameData/world.ts` `REMAKE_OVERRIDES.statusEffects` (a new `add`-only override path, the
  `RemakeStatusEffectRow`) and the `randomEvents` override's `ambushChampion`), and the `ashnumb` value on a monster's
  `onHitStatus[].status`, `abilities[].status.type`, `abilities[].hazard.status.type` and `onDeath.status.type` is a
  `REMAKE_VALUES` row of `status.ashnumb`. `tables/combat.ts` builds the added rows (`stacks` written only when a row is
  added) and records the name key (`noteRequiredKey`, required by the i18n parity check); `tables/quests.ts`
  `checkK6Quest`, `tables/maps.ts` (variants) and `tables/world.ts` (`checkK6Npcs`, `npcVisibilityIssues`,
  `checkAmbushChampion`) refuse what the loader and Finalize refuse. The export stays byte-identical (no GameData record
  uses a K6 field; 2F authors them).
- **Ashnumb** (`StatusType::Ashnumb`, appended, "ashnumb"; its default stacking is `StackCount` even without a row, so it
  never lands without one: `StatusEffectRules::Lands`). One instance per target whose value counts stacks: an
  application adds `max(1, floor(value))` stacks up to `maxStacks` and restarts the shared timer with its duration (the
  first is Applied, later ones Refreshed; `EvStatusApplied::value` is the stacks after the application). Healing
  received is multiplied by `max(0, 1 - stacks x healingReceivedPerStack)` (`StatusEffectSystem::HealingReceivedMul`)
  in `Hero::Heal` (every hero heal: potions used or picked up, life steal, kill heal, thorns heal, pet heals) and on the hero's HP
  regeneration step (`CombatSystem::TickPassives`, next to the poison multiplier); at `maxStacks` the bearer is slowed by
  `slowAtMaxStacksPct` (`SpeedMultiplier`: the stronger of it and a Slow). The antidote removes every stack
  (`EvStatusExpired`, after poison's). An untouched path without stacks: a heal multiplies only when the multiplier is
  below 1, so the web's heals stay bit-exact. Monster hits put it on through the existing status paths (`onHitStatus`,
  abilities, hazards, on-death), one roll per landed hit as any status.
- **Permanent rewards** (`QuestWorld::TurnIn`, after the items and the cache, before the pet). `ClaimPermanentReward`
  records `(questId, difficulty)` in `SessionState::permanentRewards` and adds the reward to its running totals unless
  that pair was claimed (the totals saturate at int32 max, a negative one counting as 0; 2E review); then the stat / skill
  points go to the hero's free points (`Hero::GrantFreePoints`, saturating too), `maxLife`
  is a flat `MaxHp` in `SimImpl::RebuildEquipStats` (added only when above 0: the merge is unchanged without it;
  `EquipStatsDirtyMsg` on a grant), `respecTokens` are stored for the respec UI, and `EvPermanentReward` carries the
  grant. No log line (its strings would move the export; UE can show one from the event). A turned-in quest is never
  turned in again today (a reacceptable quest restarts only after it failed; a difficulty switch keeps the quest log),
  so the per-difficulty key matters once quest progress resets per difficulty; the ledger is ready for it.
- **World flags** (`SessionState::worldFlags`, a sorted vector; `HasFlag` / `SetFlag`). `SimImpl::SetQuestFlags` is the
  first `QuestTurnedInMsg` listener: it sets the quest's `setsFlags` in data order and emits `EvWorldFlag {flag,
  questId}` per newly set flag, before the audio cue, the cutscene enqueue, the embers and the rewards. Flags are never
  cleared. The snapshot points at them (`Snapshot::worldFlags`).
- **Spawn variants** (`world/WorldState.h` `ActiveSpawnVariant`, `ExpandSpawnMembersNow`). The first variant (list order)
  whose flag is set replaces the entry's members with `count` x its monster, one pack without a leader. The zone's spawn
  reads it (each instance records its entry and member slot: `MonsterInstance::spawnEntry` / `spawnMember`), and so
  does every respawn (before any draw): with a variant active the slot comes back as the variant's monster
  (difficulty-scaled, no leadership), a slot past the variant's count (or with an unknown monster) stays down like a
  no-respawn monster until the next zone entry, which spawns the variant's full count; without one the respawn is
  unchanged. QuestWorld's guide spawn points read the active variant (none for count 0).
- **NPC lines and visibility** (`WorldState.h` `NpcLinesNow`, `NpcPanelLineNow`, `NpcVisibleNow`). An NPC's `lines`
  whose `when` holds (none: always) replace its web `dialogue` as what it says: `QuestWorld::InteractNpc` logs the first
  holding line, the linear panel shows the second (else the first; `DialogueView::lineKey`, set at open); none holding
  keeps the web's lines. `visibleWhen`: `ZoneRuntime` keeps every placement (ids unchanged) with a `visible` flag,
  computed silently at the zone's entry (SimZone announces only the visible ones) and at the start of each
  `ZoneRuntime::Tick` (`EvEntitySpawned` / `EvEntityDespawned{Removed}` on a change); a hidden NPC is not in the
  snapshot, not an interact or pointer target, not found by `FindNpc` / `FindNpcEntity`, and `InteractNpc` ignores it.
  **The validator** (`CheckNpcVisibility`, mirrored by the exporter's `npcVisibilityIssues`) checks every NPC with a
  `visibleWhen` against each quest that needs it (it lists the quest: giver and receiver; a talk objective names it; the
  quest's craft phases name it) with a three-valued static model of the quest graph: while quest Q is offered / active /
  completed, Q's prerequisites (transitively) are turned in, the quests needing Q (transitively) have no record, and a
  flag is set when one of Q's prerequisites sets it, unset when no quest sets it or only Q and the quests after it do,
  else unknown; an unknown leaf counts as "could hide". A giver that could hide while Q is active or completed, or a
  talk / craft NPC while Q is active, is an error. The offer phases (Q has no record; a `reacceptable` Q also once it
  failed: a Failed record, re-offered by QuestSystem) are checked when the open ones are fine: a giver hidden *for sure*
  there is an error (Q could never be accepted) unless another route offers Q then (`WsOtherOfferRoute`: another NPC
  that lists Q or owns a dialogue tree with a choice whose `questTrigger` is Q, with no `visibleWhen` or one that holds
  for sure in that phase; `CheckNpcVisibility`'s `trees`, DataStore passes its dialogue trees); a giver that only
  *could* hide there, or is hidden while another route offers Q, is a warning (the quest is not offered there
  meanwhile). (2E review R1 / R2: an offer-phase False was a warning, and the model had no Failed phase, so a giver
  shown only `{quest: Q, state: open}`, or only `notStarted` / `open` for a reacceptable Q, loaded and could never give
  Q.) Flags set outside `setsFlags` (K7) are outside the model until their package extends it.
- **Event champion packs** (`RandomEventSystem::TriggerEvent`, `MonsterSystem::SpawnChampionPack`). Only in a zone with
  an `ambushChampion` block, an ambush draws once more on the Events stream right after its count; below `chance` it is
  a champion ambush (`ActiveRandomEvent::champion`, `EvRandomEvent::champion`): the mix's members in list order, each
  placed as `SpawnAmbush` places one (angle and distance on the Events stream; no id draw; an unknown id skipped before
  its draws), chasing, `MonsterRole::AmbushSpawn` (no respawn), one K5 pack (`NewPackId`) led by the first member of the
  leader's id. With `sharedAffix` the zone's affixes are rolled once first (RngStream::Ai; the leader's family pool, else
  the first member's) and given to every member, which keeps those its family may roll (`MonsterSpawnParams::
  givenAffixes`, `EliteAffixTable::RollableFor`) and is elite when it keeps one; without it nothing is rolled (no Ai
  draw) and the members stay plain. `leaderLoot` (2E review R5: monsters.md 2.8's "a champion Muster drops one
  guaranteed magic item" had no owner) marks the leader (`MonsterSpawnParams` / `MonsterInstance::guaranteedLoot`, not
  carried by a respawn; ambush members never respawn); its kill (`MonsterKilledMsg::guaranteedLoot`) makes
  `GroundLootSystem::OnMonsterKilled` drop one more `GenerateEquipment` item of exactly that quality at the monster's
  level after its regular loot (the Loot stream; no draw without it). A Raised leader would get nothing (the Raised
  loot skip), but a champion leader is never Raised.
- **Saves** (`SaveData::worldFlags`, `permanentRewards {claimed [{questId, difficulty}], maxLife, statPoints,
  skillPoints, respecTokens}`; `SaveIO`, `SimSave`). Written after `contentVersion` and only when not empty, so a save
  without world state serialises byte-identically to before (no version bump); a save without them (every save before
  K6) loads with none. Parsing sorts the flags and drops empty / duplicate / non-string ids, drops claims without a quest
  id or a known difficulty and repeated pairs, and floors the totals, clamped to [0, int32 max]; the totals are trusted
  (what the hero earned stays even if the data later changes). `ApplyContentVersion` keeps both (they are not data ids).
  `ApplySave` restores them before the zone entry, so visibility and variants hold from the first step, and normalises
  them as parsing does (`NormalizeWorldFlags`, `PermanentRewardsState::Normalize`; 2E review R4: `LoadGame(const
  SaveData&)` with unsorted flags made every binary search miss).
- **Events and snapshot.** New events (appended to `Event`): `EvWorldFlag {flag, questId}`, `EvPermanentReward
  {questId, difficulty, maxLife, statPoints, skillPoints, respecTokens}`; `EvRandomEvent::champion`; `EvStatusApplied::
  value` = stacks for a stackCount status. Snapshot: `worldFlags`, `permanentRewards` (set before the hero-less early
  return), `HeroView::ashnumbStacks`, `HeroView::healingReceivedMul`; `DialogueView::lineKey`.
- **Byte identity.** Without K6 data nothing changes: the K6 save fields are omitted, Ashnumb never lands without its
  row, a variant or champion block draws nothing when absent, `SetQuestFlags` emits nothing without `setsFlags`, a heal
  is multiplied only below 1, a leader without `leaderLoot` draws nothing more. The 20-seed playthrough dumps (base 1000
  and 3000) are byte-identical to a build of 644f7b7e on GCC and Clang, re-checked after the 2E review fixes; `node
  unreal/Tools/export-data/run.mjs --check` reports the committed data up to date.
- **Tests.** `world_state_test.cpp` (suite `world`; 24 cases, 3768 assertions, GCC = Clang) on synthetic `fx_k6_*` data
  (no Chapter 1 content): the schema (every block loads; the loader's refusals of broken status rows, quests,
  conditions, lines, variants and champion blocks; Finalize's statuses that never land, missing keys, unknown condition
  quests, the validator's errors and warnings, variants (unknown, the story boss, on the story boss's entry) and
  champion members); the pure rules (quest gates, flag leaves, all /
  any; lines now and the panel's line; the validator over prerequisites, dependents, flag setters, offer-only and talk /
  craft needs; variant selection and members; one claim per (quest, difficulty); the sorted flag set); Ashnumb (the
  stack arithmetic, the cap, the shared timer, healing and the slow at the cap, heals / potions / regeneration on the
  hero, the antidote, the snapshot, a monster's hits stacking to five); permanent rewards at the turn-in (max life, free
  points, tokens, once per pair, the save round trip); flags at the turn-in (first listener, data order, the snapshot,
  the save round trip, an old save loading with none); the save format (written last and only when present,
  normalisation, the content-version upgrade); variants at the zone's spawn and at respawns, count 0, the guide, a led
  entry's leader slot coming back leaderless; NPC lines (logged, panel, fallback) and visibility (hidden at the entry
  and never announced or despawned there, appearing and leaving, ids kept, not interactable); champion ambushes (no
  block draws exactly as before, a block draws once more; the pack in mix order, one pack id, the leader (the first of
  several of its id), chasing, no respawn, the shared affix and the plain members without it); a seeded replay
  digest (`c7818c21b25eac9b`, GCC = Clang). Other suites: base's "Enums round-trip their data spellings" gains 2
  assertions (its StatusType loop meets `ashnumb`) and the world suite's K1 grep gate 3 (one per new core source file);
  no existing assertion changed. Totals 861 cases (GCC 233930 assertions, Clang 233866; 837 / 230157 / 230093 before).
  Mutation check (scratch copy, GCC): 88 single-point mutations of the K6 core (conditions, lines, visibility and the
  validator, variants at spawn / respawn / guide, claims, flags, saves and parsing, Ashnumb stacking / timer / healing /
  slow / antidote / snapshot, permanent rewards, champion draws / pack / leader / affixes, Finalize and loader checks):
  a first pass left 8 alive (a first application above the cap, the silent zone-entry refresh, a variant slot keeping
  its leader, the guide at a count-0 variant, a leader id with several members, the story boss's entry / champion
  member checks, and one equivalent: the champion's `monsterCount` reset, which nothing reads once the ambush is pruned
  in the same call); each but the equivalent got the cases above and now fails one (87 / 88; 7 first-pass mutations
  that did not compile were rewritten and fail too). A GCC 13 `-Warray-bounds` false positive in the ASan build (a
  one-element `= {...}` assignment) made the test use `assign`.
  A scratch end-to-end run (exporter + core) exported a full synthetic K6 set and loaded it with no error, and failed or
  warned on each broken variant as the loader / Finalize do.
  *2E review:* 28 cases, 4438 assertions (GCC = Clang; digest unchanged): the validator's offer phases (an offer-phase
  False is an error; another NPC's list or dialogue tree is a route only when visible for sure, the giver's own tree is
  none; Unknown warns; a reacceptable quest's re-offer), saturating claims and free points (a parsed `1e12` total, one
  grant more), `Normalize` / `NormalizeWorldFlags` and `ApplySave` of a `SaveData` built in code, `leaderLoot` (loader
  refusals; two sims on one seed whose data differ only in it: the leader's kill drops exactly one magic item more, last,
  a member's the same), the chaplain's two-line order, a healing share past 100 % (no heal or regeneration takes life),
  Nightmare variant spawns / respawns and champion members scaled like any spawn, a pointer press passing over a hidden
  NPC to a visible one, an NPC appearing as that step's prompt, the hunt / keyword-rule / `eliteFrozenOnHit` never-lands
  refusals and a story boss in a mix entry refusing variants. Totals 865 cases (GCC 234600 assertions, Clang 234536).
  Mutation re-check (the working tree, GCC, world suite): the review's 10 survivors and 20 mutations of the fixes; 29 / 30
  fail a case (the pointer case first pressed next to a hidden NPC that shared a camp slot's tile with a visible one, so
  M1 survived until the NPC moved to a slot of its own; R1's mutation was rewritten to compile); 1 is equivalent
  (`WsOtherOfferRoute` skipping the NPC itself: its own condition never holds there for sure, so it is never its own
  route). A scratch exporter + core run (38 cases, the K6 set above plus R1 / R2 / R5's) agrees with the core on every new
  refusal and warning.
- **Deviations.** (1) Ashnumb needs a new stacking rule and a status row: `stackCount` + `stacks` (the plan said "a row
  via the override"; the web has no per-type stack cap or heal factor to reuse). (2) It scales every `Hero::Heal` (also
  kill heal, thorns, pet heals), not only the plan's three sources: one rule at the single heal entry point. (3) The
  plan's `rewards.permanent` maxLife joins the equip-stat merge as a flat MaxHp (derived stats follow), points are given
  once as free points (the hero's save holds them), tokens are stored only. (4) "Once per (quest, difficulty)" is
  recorded, but no flow turns a quest in twice today (above). (5) Conditions are flat objects (no nesting beyond one
  `any`); a quest leaf requires its `state`. (6) Lines fall back to the web `dialogue` when none holds; the panel line
  comes from `DialogueView::lineKey`. (7) A hidden NPC keeps its placement and entity id. (8) The validator is static:
  "unknown" is an error in an open phase; in an offer phase a giver hidden for sure with no other route to the quest is
  an error, one that only could hide (or is hidden while another NPC offers the quest) a warning. (9) Variants: the first set flag
  wins; `count` 0 empties the entry; a respawned slot past the count stays down until the next zone entry; no leader;
  refused on the story boss's entry and on sub-dungeon spawns (until K7). (10) Champion packs: the shared roll uses the
  leader's family pool; members keep only family-rollable affixes; role `AmbushSpawn`; the guaranteed drop is a block
  field (`leaderLoot`, one extra item of exactly that quality, the leader only), not a loot-table floor; enabled through
  `REMAKE_TABLE_FEATURES` (an override table has no record gate). (11) No log line for a permanent
  reward or a flag (strings would change the export; the events carry them).
- **2E review fixes** (2026-10, pending review). (R1) An offer-phase False (the giver hidden whenever its quest is
  offered) is an error unless another route offers the quest (*NPC lines and visibility* above); the case that pinned
  `{quest: Q, state: open}` on Q's only giver as a warning now expects the error. (R2) The model's re-offer phase: a
  `reacceptable` quest is checked with a Failed record too. (R3) `ClaimPermanentReward` and `Hero::GrantFreePoints`
  saturate at int32 max (a loaded total of int32 max used to wrap negative and drop the hero's permanent max life).
  (R4) `ApplySave` normalises the K6 save fields like `ParseSave`. (R5) `ambushChampion.leaderLoot`, the guaranteed
  champion drop (*Event champion packs* above), with loader / exporter checks and a two-sim case. (R6) The 2F guidance on
  quest-gated lines (below) and chapter1.md §3; a two-line case pins the chaplain's order. (R7) Cases for the review's
  surviving mutations: a healing share past 100 % floors at 0 (no heal or regeneration takes life), a Nightmare variant
  spawn / respawn and champion pack scaled like any spawn, a pointer press passing over a hidden NPC to a visible one, an
  NPC appearing as that step's prompt, the hunt / keyword-rule / `eliteFrozenOnHit` never-lands refusals and a story boss
  in a mix entry refusing variants. (R8) The UE status tables in *For 2G*.
- **For 2F (content).** Ashnumb: add the row through `REMAKE_OVERRIDES.statusEffects` (`add: {ashnumb: {tickIntervalMs:
  0, stacking: 'stackCount', stacks: {maxStacks: 5, healingReceivedPerStack: 0.1, slowAtMaxStacksPct: 20}}}`) with
  `sys.statusEffect.name.ashnumb` (麻灰 / Ashnumb) in both locales, then put `ashnumb` on the breach-blood monsters'
  `onHitStatus` (value `fixed` 1 = one stack per landed hit, `durationMs` 6000) in place of W1's poison. World flags:
  `setsFlags: ['rf_hungerbell_silenced']` on M2, `['rf_chapter_dawn']` on M5; spawn `variants` on the trench and Dead
  Ranks entries (`{whenFlag: 'rf_chapter_dawn', monsterId: 'gravegnawer', count}`; not on Valdras's entry). Permanent
  rewards: S1 `{maxLife: 15}`, S6 `{statPoints: 3}`, S7 `{skillPoints: 1}` (W3, with the Pit). Quest-gated lines as
  `lines` with `data.npc.<id>.after.<questId>` keys and `when: {quest: '<questId>', state: 'turnedIn'}` (e.g. Marl's
  「血里有声音……」 after `q_rf_black_blood`). The log takes the first holding line and the linear panel the second (else the
  first), so keep the slots stable: gate the W1 line the after line replaces with `{quest: '<questId>', state:
  'notTurnedIn'}` and keep the other W1 lines condition-free. The chaplain (two W1 lines, the second always on, chapter1.md
  §3) is `[{key: after.q_rf_last_toll, when: turnedIn}, {key: dialogue.0, when: notTurnedIn}, {key: dialogue.1}]`; a
  one-line NPC (`blacksmith`, `quest_elder`: the logged line only) is `[{after, when: turnedIn}, {dialogue.0, when:
  notTurnedIn}]` or the after line with the W1 line condition-free behind it. (2E review R6: the first guidance, the after
  line in front of every W1 line, would have shown the chaplain's line 0 on the panel after the quest and dropped his
  always-on line.) Any `visibleWhen` must pass the validator (the export fails otherwise; a giver hidden whenever its
  quest is offered is an error too). The Muster: `randomEvents` override `zones.emerald_plains.ambushChampion {chance:
  0.1, mix: [{monsterId: 'dead_standard_bearer', count: 1}, {monsterId: 'risen_footman', count: 3}], leader:
  'dead_standard_bearer', sharedAffix: true, leaderLoot: 'magic'}` (an ambush draws once more: the playthrough moves
  when it lands; the standard-bearer's kill draws one more item on the Loot stream). AutoPilot has no antidote rule: a
  bot rule for Ashnumb (`HeroView::ashnumbStacks`) is 2F's if the sweep's fight lengths need it.
- **For 2G (UE).** The Ashnumb tint band from `HeroView::ashnumbStacks` (or `EvStatusApplied::value` for Ashnumb) and its
  removal on `EvStatusExpired`; re-read moods / audio emitters from `Snapshot::worldFlags` on `EvWorldFlag` (the Gnawed
  Field's ringing stops at `rf_hungerbell_silenced`, `march_dawn` at `rf_chapter_dawn`); a permanent-reward line or
  toast from `EvPermanentReward`; the linear NPC panel shows `DialogueView::lineKey` when it is not empty; NPC actors
  appear and leave on `EvEntitySpawned` / `EvEntityDespawned{Removed}` mid-zone; a champion marker on an
  `EvRandomEvent::champion` ambush (with K5's `packLeader`). Two UE tables still know only the web's six statuses and
  skip Ashnumb (bit 6) without a crash (2E review R8): the HUD status row (`Source/Abyssfire/Private/UI/Hud/
  SAbyssHudWidgets.cpp`, `StatusColors[6]` and its `Index < 6` loop: extend it to `EnumCount<StatusType>()` with an
  Ashnumb colour and the stack count from `HeroView::ashnumbStacks`) and the character tint table
  (`Source/Abyssfire/Private/Actors/AbyssCharacterActor.cpp` `StatusTints[]`: an Ashnumb entry, or hand Ashnumb to the
  band). The VFX status loops already iterate `EnumCount<StatusType>()` (a missing recipe is remembered, not retried).
- **For 3A (K7).** A flag set by the cleanse needs the validator's model and the "no quest sets flag" warnings extended;
  sub-dungeon spawns take `variants` (the loader refuses them now) once sub-dungeons spawn.

**2F outcomes (Content W2 / C2 as built, 2026-10; pending review; DECISIONS DR9 *2F*, M5 *2F note*, DR8 *2F*).**
- **Monsters** (`GameData/monsters/emerald_plains.ts`, `miniBosses.ts`; monsters.md 2, chapter1.md 10 / 11). Every
  Chapter 1 definition, the Corpseherd and the three hunts carry `hitCapHeroMaxHpPct: 35` and an authored `windupMs` on
  every ability (>= 600 ms; >= 700 for a boss special). Breach-blood: `breachclot` lays one Ashnumb stack per landed hit
  (`fixed` 1, 6 s, chance 1; W1's poison gone), `cinderbloat` keeps its 30 % burn and lays Ashnumb too, and bursts 800
  ms after death (circle r2.2, 18 fire, `hitsMonsters` x2, capped at 20 % of the hero's life); both leave no body. The
  Unburied (`risen_footman`, `risen_arbalest`, `risen_shieldbearer`, `dead_standard_bearer`): `corpse`, the family trait
  `riseAgain {0.25, 3000, 40 %, notOnCrit}`; the arbalester kites (`kite {3, 6}` inside its range 6); the shieldbearer
  blocks (`block {arcDeg 100, damageMul 0.5, wall 2}`); the standard-bearer musters (`muster`: raise, Cast01, 900 ms,
  every 10 s, one footman at 40 % from a corpse within 6 tiles, `capAlive` 2, one per 10 s) under its banner (`aura`
  damage x1.15, r5, the Unburied). The Living: `oath_zealot` frenzies below 50 % (a phase row: Roar, attack speed x1.35,
  move x1.2, damage taken x1.1); `deserter` flees its dead leader (`morale {5000, r8, sustain living r8}`); the drummer
  drums (`war_drum`: a call, Cast01, 2000 ms, every 8 s, `fearImmune` 6 s to the Living within 8; `aura` attack speed
  x1.1, r8, the Living), so the Hollow holds while it lives. The Gravegnawers: `gravegnawer` `packAggro 6`, eats corpses
  (`corpse {remains, consumes}`); `bonegorger` slams (`clapper_slam`: circle r2, Attack02, 800 ms, x1.6, every 6 s),
  `remains`. The Corpseherd: Call the Flock as two raises (`call_the_flock_pull`: two footmen at the pull, priority 2, a
  600 s cooldown; `call_the_flock`: one per 8 s, `capAlive` 3, `perWindowCap` 1 / 8000; both 900 ms, at 40 %), Keen
  below 50 % (circle r4 on herself, Cast02, 800 ms, x0.4, slow 25 % 3 s), `kite {2, 3}` (range 3.0 kept: M5 *2F note*),
  `remains`; HP 300 unchanged. Valdras (chapter1.md 11): P1 Banner Sweep (cone r2.6 120 degrees, Attack02, 700 ms, x1.3,
  5 s) and Oath-Brand (circle r1.8, Cast01, 900 ms, x1.8 fire, a 4 s brand ticking x0.15 every 500 ms, 8 s); P2 below 65
  % (Roar, move x0.6, the tether r3 on the dais, Feed r4 / 3 % / 1 s, a `rollcall_standard_bearer` summoned at the west
  ash circle (11, 91), once more 20 s after its death, Breach-seep: a `hazard` line 8 x 1 from the dais, 700 ms, two
  fire ticks x0.5 and slow 30 % 2 s, every 6 s, P2 only, stinger `stinger_valdras_roll_call`); P3 below 30 % (Roar,
  attack speed x1.25, move x1.15, Lunge switched on: line 5 x 1.2, 800 ms, x1.5, 7 s; the ring closing from 9 to 5 over
  40 s, 4 % of max life per second of fire outside; stinger `stinger_valdras_no_retreat`); `remains`. **Numbers: 14000
  HP, damage 32** (below). `rollcall_standard_bearer` 亡旗手 (new, appended to the zone list; the standard-bearer's numbers
  and look, `spriteKey monster_dead_standard_bearer`): `roll_call` (summon, Cast01, 900 ms, every 10 s from 3 s, a
  footman at each ash circle (11, 91) and (19, 99), 40 %, `capAlive` 4) and the banner aura. Hungerbell's bell is a hunt
  ability (`supper_bell`: a call, Attack02, 800 ms, every 9 s, provokes the gnawers within 10; below).
- **World** (`maps/emerald_plains.ts`, `world.ts`, `quests/all_quests.ts`, `npcs.ts`; chapter1.md 3 / 7 / 8 / 10 / 13 /
  17). The §17 W2 mixes (same counts, 55): the Weeping Ditch 3 bloats + 3 clots; the Broken Redoubt 3 arbalesters + 2
  shieldbearers; the Dead Ranks 3 shieldbearers + 2 footmen + the standard-bearer (leader); the Hollow 5 deserters (the
  first an elite leader) + the drummer; the midden a bonegorger + 2 gnawers. Dawn variants (`rf_chapter_dawn`): the
  trenches become 8 gnawers, the Dead Ranks 6. The Sheepfold's six `corpseSeeds` (footmen). The Muster: `ambushChampion
  {0.1, standard-bearer + 3 footmen, leader the standard-bearer, sharedAffix, leaderLoot magic}`. The Ashnumb row
  (`statusEffects.add.ashnumb`: 5 stacks, -10 % healing each, slowed 20 % at 5) with `sys.statusEffect.name.ashnumb` 麻灰
  / Ashnumb; Scorched bursts (1000 ms, r2, 20 fire, cap 20 %) and Gravecold rings (600 ms, r2.5, ice, slow 30 % 2.5 s)
  on death; the `unburied` affix 不葬 / Unburied (`riseAgain {1, 3000, 50 %}`, `families [unburied]`, loot +3, no stat
  change, the Unburied violet #D08CFF). Flags: M2 sets `rf_hungerbell_silenced`, M5 `rf_chapter_dawn`. Permanent
  rewards: S1 +15 max life, S6 +3 stat points. Quest-gated lines (the `after` strings 1B wrote): the blacksmith after
  S3, the Captain after S4, Marl after S1 (each the after line in front of its condition-free W1 line), Brother Aldwin
  after M5 (his line 0 gated `notTurnedIn`, line 1 kept; the 2E guidance). New strings:
  `data.monster.rollcall_standard_bearer`, `sys.eliteAffix.name.unburied`, `sys.statusEffect.name.ashnumb` (zh-CN, en;
  zh-TW derived).
- **Hunt abilities** (new remake feature `quest.huntAbilities`; the plan's schema had none for a hunt). A quest's hunt
  may carry `abilities` (`GameData/types.ts` `RemakeQuestHunt`, the key gate's `hunts[]` level, `REMAKE_FIELDS`); the
  exporter writes them on the `quest_hunts.json` hunt row (`hunts[].abilities`) and merges them after the base's into
  `defsNormal` (checked by `checkK2Monster`; an id the base already has fails), and keeps `quests.json`'s hunts
  web-only. Core: `HuntDef::abilities` (loaded with the monster ability reader, duplicate ids refused),
  `MakeHuntDefinition` appends them to the base's (the runtime builds a hunt from its base, so `defsNormal` alone never
  reached a fight), and `DataStore::Finalize` refuses a hunt ability that repeats a base id and warns when
  `defsNormal`'s ability ids are not the base's followed by the hunt's own.
- **Core fix: an elite blink stays in bounds** (`MonsterSystem::TickEliteBehaviours`). With the M1 Returning leash a
  Breachwalker blink that would land past the leash range of the monster's spawn anchor, or outside a K4 tether, is
  skipped (its draws made as before, like a blink onto a blocked tile; web-parity leash mode unchanged). Found by the
  sweep: a Breachwalker Valdras blinked off his tethered dais after a hero who was luring Raised out of the Feed zone,
  landed past his leash and walked home healed to full and back to P1, five times in one fight (mage, seed 2018: 850 s,
  three deaths); the same seed now fights him in 149 s.
- **AutoPilot W2 rules** (`AutoPilot.cpp`; plan 6; no content id, no class branch: every rule reads the snapshot and the
  definitions). (1) *Telegraphs and hazards*: every damaging `TelegraphView` (circle / cone / line / ring / hazard
  strikes, on-death fuses) before its contact and every live `HazardView` is a danger (`ShapeContains` with the body
  radius); a hero inside one, or outside a K4 arena's ring, walks to the nearest safe spot (16 directions on rings of
  0.5-5.5 tiles, straight walkable, inside the arenas, nearest the fight) and dodge-rolls (`CmdDodge`, i-frames) when
  the contact is due within 200 ms; a named fight's spot stays inside its leash (below). While the approach to the
  target crosses a strike about to land (a hazard only in its last 1.5 s) the hero holds (3 s at most). (2) *Priority
  targets*: a monster with an aura or a between-swings ability (raise / summon / buff / call; the definition or the
  hunt's `defsNormal`) ranks 2, a pack leader 1; the fight picks the best rank within the threat radius + 1.5 and
  switches to a higher rank every 20 frames (the standard-bearer before its shieldbearers, the drummer before the
  deserters, the roll-call bearer before Valdras, the Corpseherd before her flock). (3) *Feed*: a Raised standing in a
  feeding phase's zone (the row's centre, else the boss's map spawn tile) is lured 2.5 tiles past its edge before it
  dies (5 s at most). (4) *Boss phases and the leash*: a monster walking home is followed in, not shot from outside; in
  a named fight the hero stays within the leash range less 4 tiles of the monster's first-seen spot (its home); a target
  lock whose approach ended short (a tether, a kite) is clicked again. `boss` / `hunt` steps and the walks pick targets
  with (2). The route's Valdras step allows 900 s (was 400: the rogue's long fight).
- **Instrumentation** (`AutoPilotStats`, observational: dumps of base 1000 stay identical with it on the W1 data):
  `fights` (named fights: start / end, hero deaths, affixes, phase entry times, Feeds, resets), `maxHitTakenPct` /
  `maxHitTakenBy` (the largest non-tick hit as a percent of the hero's life), `oneHitDeaths` (a blow from >= 50 % to 0),
  `telegraphDodges`, `priorityTargets` and W2 event counters (raised, Unburied rises, blocks, flees, support casts,
  corpses eaten, on-death fuses, Ashnumb hits and peak stacks, champion ambushes, flags, permanent rewards, phases).
  `playthrough_test.cpp` checks `oneHitDeaths == 0` and `maxHitTakenPct <= 35` on every run and appends one line per run
  to `ABYSS_PT_STATS=<file>` (class, seed, level, exp, kills, deaths, the hit numbers, dodges, priority switches,
  minutes, the W2 counters, the fights).
- **Balance** (DR9, plan 6 targets). With the W1 numbers the bot warrior killed a three-phase Valdras in about 2 s (the
  hero's Lv 12-13 damage against 900-1100 HP). Valdras is **14000 HP / 32 damage** (x12.7 / x1.6 the design's ~1100 /
  20): the bot warrior's fight lands in the 60-90 s target at the median; his largest hit stays under the cap. Sweep
  (GCC: 600 runs, 20 seeds x 3 classes from each of the bases 1000-10000 step 1000; Clang: bases 1000 and 7000, their
  final saves identical to GCC's): every run passes; Lv 13 (402) / 14 (198) (warrior 139 / 61, mage 132 / 68, rogue 131
  / 69), 4331-5276 exp, 91-147 kills, so DR9's 12-14 band holds (no amendment). Valdras: the warrior's fight takes a
  median 74.1 s (p10 49.6, p90 99.3, range 24.6-128.2; 113 of 200 inside 60-90, 46 shorter, 41 longer; the affix moves
  it: Driven / Unburied medians 56 / 61, the rest 75-84), the mage's 136.1 s (40.5-255.3), the rogue's 228.9 s
  (65.3-348.7: his mana-bound damage); no fight reset (before the blink fix one mage fight reset five times). Hunts and
  the Corpseherd keep the W1 numbers and die in seconds (medians 0.2-3.9 s, at most 25.9 s). Deaths: warrior 0, mage 2,
  rogue 7 (one per run at most; 8 of the 9 to Valdras, 7 of them a Brutish Valdras); no death to a single hit; the
  largest hit taken is 23.4 % of the hero's life (rogue, Valdras; warrior 13.9 %, mage 16.9 %), under the 35 % cap. The
  W2 systems show up in every run: per run 10-15 bodies raised, 2.5-8 Unburied rises, 6-14 blocked hits, 3.5 flees, 1.3
  support casts (the drummer and the bearers die first), 0.3-0.9 corpses eaten, 4.8 bloat fuses, 4-7 Ashnumb stacks laid
  (5 at once at the peak), 3.6-5.7 phase rows, both flags and both permanent rewards; a champion Muster in about 6 % of
  the runs. The bot steps out of 45 / 72 / 112 telegraphs per run (warrior / mage / rogue) and switches to a priority
  target about 4 times.
- **Tests.** `content_ch1_test.cpp` re-pins: the roster table (Valdras 14000 / 32, the roll-call bearer), derived stats
  and difficulty rows (Valdras str 25 / vit 1400; NM 21000 / 48, Hell 28000 / 64), the affix application table (8
  affixes; Unburied changes no number), the zone population (members matched through `spawnEntry` and the mix; 2
  leaders; the Hollow leader with one affix; per-definition counts), the defend-wave fallback list (13 ids), the W1
  on-hit table (Ashnumb on clots and bloats), the roster size (39) and zone list, the affix look (Unburied); new cases
  pin the W2 ability table and telegraph floors, families / corpses / traits / burst / packs / morale / kite / block /
  auras / the zealot, Valdras's phases (and his dais and circles walkable), the world (mixes, leaders, variants, seeds,
  the Muster) and the world state (flags, permanent rewards, the NPC lines before and after their quests through
  `NpcLinesNow` / `NpcPanelLineNow`, Ashnumb, the remade affixes), and every hunt's ability ids against `defsNormal`.
  Engine cases that read the shipped data moved off it: `data_test` (8 affixes), `monsters_test` (Unburied last; a new
  blink-bounds case: past the leash, inside it, past a tether, inside a wider one), `packs_test` (leaders counted
  against a plain run), `corpses_test` (the Unburied row is shipped: the edit appends it once; the shipped-table
  assertions live in content_ch1), `world_state_test` (the Ashnumb row is shipped: the bare-rules cases take it out of a
  copy; `K6StripChampions` removes the shipped Muster; the K6 replay digest re-pinned to `cf95a829ec9b134e`, GCC =
  Clang). Totals: 870 cases (GCC 235593 assertions, Clang 235529; the gap of 64 is the world suite's, as before).
- **Deviations.** (1) `botPriority` (plan 2.6) is not a field: the bot ranks targets from the data it already has
  (abilities, auras, pack leaders), so no record names one. (2) Breach-seep is one line, not three (no `count` field;
  three abilities would share one cooldown poorly): the dais line aims at the hero (`aim anchor`). (3) The Corpseherd's
  kite is [2, 3], not 3-5 (her range 3.0 kept; M5). (4) The arbalester kites below 3 tiles (band [3, 6]; monsters.md
  says "within 2 tiles (band 3-6)"). (5) Valdras 14000 / 32 (above; chapter1.md 11's numbers were "tuning targets set by
  the gate"). (6) The P2 muster circles are a `rollcall_standard_bearer` (a new definition with `roll_call` at both
  circles) rather than the zone's `dead_standard_bearer` raising at both (its muster is a raise at a corpse; the circles
  need a summon at anchors). (7) The trench / Dead Ranks dawn variants keep the entries' counts (8 / 6). (8)
  Hungerbell's call is its own 9 s ability (monsters.md 2.4 "every slam calls"; one ability cannot both slam and call).
  (9) The Unburied affix can roll on an Unburied boss (the Corpseherd, Valdras: one affix in Chapter 1, now 1 in 8)
  where it does nothing (K3: bosses never rise) but still gives its loot bonus: excluding it needs a core rule (an affix
  whose only effect cannot apply is not rollable) for a later package. (10) The core changes above (hunt abilities,
  blink bounds) are outside "content only".
- **For 2G (UE).** The drummer's `war_drum` and Hungerbell's `supper_bell` are calls with long windups on short clips
  (Cast01, Attack02): ComputeAbilityTiming slows the clip to the windup (playRate = clip contact / windupMs), so the
  drum loop plays at a crawl until a looping drum clip (or a 2000 ms Cast01) ships; Hungerbell's call plays its slam
  clip with no damage (a bell-ring clip would read better). Draw the roll-call bearer with the standard-bearer's art
  (its `spriteKey`), the P2 / P3 stingers on `EvMonsterPhase`, the Unburied affix in the Unburied violet, the Muster
  leader's champion marker (`packLeader` on an `EvRandomEvent::champion` ambush), the dawn variants' gnawers in the
  trenches after M5. The 2E UE status tables (Ashnumb) now see real stacks in Chapter 1.

**2F review outcomes (2026-10; pending review; DECISIONS M1 / M7 / DR8 / DR9 *2F review*, chapter1.md §19, monsters.md
§1.3 / §2 / §3).** The review's 17 findings (2F-D1..D11, CORE-R1, BOT-R2, BAL-R3, BAL-R4, SAVE-R5, BOT-R6) were all
real; every one is fixed, three with a recorded remainder (below).
- **Core rules** (DECISIONS DR8 *2F review*). *Ranged specials* (2F-D1): a damaging ability whose `rangeMax` passes
  attackRange x attackExitMul (`AbilityIsRangedSpecial`) also starts between swings from Chase or Attack, like a raise
  or a call (`TryStartSupportCast`; `AbilityPickInput::swingReach`); the DataStore / exporter "no effect past the reach"
  warning is gone (a `rangeMin` past the reach still warns / fails: never picked). *Clipless abilities*: a hazard or a
  call may leave out `clip` (it then authors `windupMs`); it emits no EvPlayAnim and does not root its caster (2F-D9).
  *Status-only strikes*: `dmgMul` 0 on a ground strike rolls no damage; a body inside takes its status, the dodge roll's
  i-frames avoid it (`CombatSystem::ApplyMonsterStatusOnly`; the loader wants `status` and no `hitsMonsters`; 2F-D6).
  *Charges*: `dash` on a line ability (not aim `anchor`): the end is fixed at windup start (`MonsterSystem::DashEnd`
  over `AbilityDashEnd`: every 0.25 tiles walkable, outside the camps, inside the leash and the tether), shown on
  `EvTelegraph` / TelegraphView `hasDash` / `dashTo`; at contact, after the hit, the caster is teleported there
  (`TeleportReason::Dash`, appended; a stun or death mid-windup cancels it). *The bound fight's leash* (CORE-R1, 2F-D3;
  DECISIONS M1 *2F review*): a monster whose phase table holds an arena or a tether is leashed to its authored home over
  max(leashRange, `PhaseFightReach`) (`MonsterLeashCentre` / `MonsterLeashRange`, used by the M1 check, the Returning
  walk, flee / kite bounds, the elite blink and the dash; `MonsterView::leashCentre` / `leashRange`); Valdras's is 11.4
  tiles from the dais; the 2C review's tether-past-leash warning is removed (DataStore and `warnTetherPastLeash`).
  *Affix rollability* (2F-D5): `RollEliteAffixes(..., canRise)` leaves the rise affixes out for a monster that can never
  rise (`MonsterMayRise`: boss roles, Raised, a body that is not a corpse), and a shared champion roll's member drops
  them. *The on-death window cap* (2F-D7): bursts and rings that land on the hero share 35 % of its max life per 1000 ms
  (`OnDeathWindowRoom`, x the difficulty's damage multiplier) on top of each one's own cap. *Ambush variants* (2F-D11;
  feature `events.ambushVariants`): `ambushVariants [{whenFlag, monsters}]` replaces a zone's ambush list (and the
  rescue guards' fallback) while its flag is set; a champion's `notFlag` turns the champion off with no extra draw.
  *Boss affix pins* (SAVE-R5; DECISIONS M7 *2F review*): the story boss's and a zone mini-boss's roll is pinned per
  (zone, monster id) at its first fight (`PinAffixesInFight`, Chase / Attack in TickAI), dropped at its death, saved
  (`SaveData::bossAffixes`, written last only when present, normalised, dropped by the content-version upgrade for an
  unknown monster); a spawn takes the pin without a draw. *A held lock re-approached* (BOT-R6): when the locked target
  leaves the hero's reach by itself after the hero reached it, an idle hero walks to it again (C7 approach, within
  targetCycleRangeTiles) until the player's next move order (`HeroLocomotion::MoveGeneration`); the bot's own re-click
  stays for its dodges and step-outs (they end the rule like a player's move); a zone exit drops the rule's lock.
- **AutoPilot** (BOT-R2, 2F-D4). `AutoPilotConfig::reactionMs` (a telegraph / hazard is a danger only that long after it
  appeared) and `dodgeRoll`; the fence reads `MonsterView::leashCentre` / `leashRange` (the first-seen spot is gone);
  `AutoPilotStats::maxBurstTakenPct` (the most damage within any 1000 ms) and `AutoPilotFight::abilities` (per named
  fight: each ability started, and cancelled before contact). `playthrough_test.cpp`: env `ABYSS_PT_REACTION_MS` /
  `ABYSS_PT_NO_DODGE`; per run a burst <= 50 %, the story boss fight <= 400 s with no reset; per sweep (>= 10 seeds per
  class) DR9's per-class band (plan 6). The route's Valdras step is back to 450 s (from 900).
- **Content.** Valdras 9000 HP / 20 damage, the design's damage (BAL-R3, 2F-D2: at 24 a Brutish Valdras killed the
  reaction-limited rogue in 10 of 18 fights); Breach-seep clipless with `rangeMax` 10; Lunge a fire charge at 2.5-5
  tiles (`rangeMin` 2.5, `rangeMax` 5, `dash`); Banner Sweep fire. The Corpseherd 600 HP (BAL-R4, 2F-D4); Keen `dmgMul`
  0. Scorched's burst `hitsMonsters` x2. The zealot's frenzy without its Roar (2F-D8). Hungerbell's `supper_bell`
  clipless. Marl `visibleWhen {any: [M5 not turned in, S1 active]}`; `data.quest.q_rf_black_blood.permanent` 玛勒的苦药 /
  Marl's Bitter Draught (2F-D10). The Broken Redoubt's dawn variant (5 gravegnawers), `ambushVariants` (gravegnawers
  after dawn) and the Muster's `notFlag` (2F-D11). The hunts keep their numbers (their exp is hpMul-scaled: a sturdier
  hunt pushed the hero to Lv 14-15). Recorded: the drummer's aura (2F-D6) and Breach-seep's one line (2F deviation 2)
  stay as built.
- **Balance** (DR9 *2F review*; GCC sweeps, bases 1000-10000 step 1000, 20 seeds x 3 classes each, in both bot models:
  1200 runs, every one passing; Clang bases 1000 / 7000 in the frame-perfect model: final saves and stats lines
  identical to GCC's). Frame-perfect bot: Lv 13 (405) / 14 (195), 4334-5331 exp, 92-153 kills; Valdras median / p90 /
  max: warrior 38.0 / 51.3 / 71.8 s, mage 50.1 / 77.6 / 104.9 s, rogue 142.1 / 184.2 / 239.1 s; no death to him and no
  reset; the Corpseherd's opening raise in every fight, Keen in 85 / 86 / 99 % of them (warrior / mage / rogue); the
  largest hit 10.2 % of the hero's life, the largest 1 s burst 17.2 %; one rogue death in 600 runs (outside the named
  fights). Reaction-limited bot (400 ms, no dodge roll): Lv 13 (395) / 14 (205), 4241-5181 exp; Valdras median / p90 /
  max 35.9 / 48.6 / 60.2, 54.2 / 90.0 / 116.0 and 131.4 / 161.2 / 200.0 s; deaths to him 0 / 0 / 1 in 200 fights each
  (the one a Brutish Valdras); the largest hit 15.6 %, the largest 1 s burst 19.6 %; Keen in 90 / 89 / 99.5 %. At 24
  damage the same sweep had 0 / 2 / 12 deaths to him, 12 of them against Brutish (the rogue died in 10 of its 18 Brutish
  fights); before the review the reaction-limited model died 2 / 89 / 197 times in 100 runs per class (the review's
  harness). Hunts: medians 0.2-3.1 s; Hungerbell's slam lands in 90-100 % of its fights, its call in all.
- **Tests.** New cases: `monster_abilities_test.cpp` (ranged specials, pure and at runtime from Chase and from a tether;
  clipless hazards and the loader's rules; status-only strikes with and without i-frames; the dash end, teleport, stun
  cancel, snapshot and loader rules, and a wall, a camp's safe zone, a tether and the leash each stopping a dash short), `phases_test.cpp` (the bound fight's leash: a hero at the ring's edge opposite the
  jittered anchor keeps the boss fighting; dragged past the reach it walks home to the dais and resets; a monster
  without a bound fight leashes as before), `corpses_test.cpp` (the on-death window cap; `MonsterMayRise` and the affix
  pool), `world_state_test.cpp` (ambush variants and `notFlag`: load, refusals, Finalize, no extra draw, the variant
  list; boss affix pins: serialisation, normalisation, content version, pinned at the fight, a load spawns the pin, a
  kill ends it), `combat_test.cpp` (the held lock re-approached, a move order ending it, the cycle range),
  `content_ch1_test.cpp` (the review's content pins), `monsters_test.cpp` (blink bounds; the mini-boss life order from
  the second zone: the remade Corpseherd outlasts the web's zone-2 mini-boss behind the sealed gate). Re-pinned: the K6
  replay digest (`2a75b9e104969bc9`: the content's numbers and the affix rule; GCC = Clang); the K2-K5 digests are
  unchanged. The scripts check found the 2F affix name 不葬 missing from the bundled CJK subsets: `build_fonts.py` rebuilt
  the eight Noto subsets (one glyph, 葬, added to each). Totals: 881 cases, GCC 241053 assertions, Clang 240989 (the
  world suite's known gap of 64).
- **Verification of the leash fix** (CORE-R1, 2F-D3). The review's probe (the bot stopped at P3's entry, the hero held
  0.7 tiles inside the ring on the side away from the jittered spawn anchor for 15 s) on seeds 30000-30029 x 3 classes:
  0 of 87 probes walked him home (the review: 68 of 90); held 7 tiles off the dais in P1, 0 of 87 (44 of 90). With the
  bot's fence switched off (a scratch build of AutoPilot.cpp), 100 seeds per class in each bot model: 0 resets in 600
  fights (the review: 3 rogue resets in 300, one 452.7 s fight).
- **Remainders.** (1) The rogue's fight is long (median 131-142 s against the warrior's 36-38 s) because his damage is
  mana-bound (class balance, out of this package). (2) A Brutish Valdras is still the hardest roll (the one
  reaction-limited death); excluding damage affixes from the story boss stays a design call. (3) The hunts still die in
  seconds at the level the hero meets them.
- **For 2G (UE).** Draw `EvTelegraph` and `HazardView` before any W2 content reaches a playtest build (the
  reaction-limited bot is the evidence: without visible decals players die). A telegraph with `hasDash` is a charge:
  slide Valdras toward `dashTo` through the windup or on the contact's `EvEntityTeleported {reason: dash}` (not a blink
  effect). Clipless abilities (Breach-seep, Hungerbell's call) have no EvPlayAnim: the crack decal and the call's ripple
  are the whole tell. Keen lands as `EvStatusApplied` (slow) or a dodged `EvHit` (i-frames), never a damage number. The
  zealot's frenzy has no Roar: play its look on `EvMonsterPhase`. Marl hides when M5 is turned in (`NpcView` visibility)
  while `cs_rf_finale` still focuses her: keep her drawn until the cutscene ends. The S1 toast reads
  `data.quest.<id>.permanent` when present. The drummer's aura (MonsterView aura fields) needs a look under the Living's
  no-glow rule. `MonsterView::leashCentre` / `leashRange` are there for a debug overlay.

**2G outcomes (UE Wave 2, 2026-10; pending review; DECISIONS DR16).**
- **Stage T: telegraphs, hazards, arenas, dash, phase stingers, ability play rates, fuse swell.** Rules in
  `Source/Abyssfire/Private/Vfx/AbyssTelegraphRules.h` (contract parse, shape geometry and SDFs, strike / zone
  timelines, CPD packing, decal decisions, recipe ids, budget, late views, dash slide),
  `Private/Actors/AbyssAnimActionRules.h` (`ActionPlayRate`, `ParseDeathRuntime`) and `AbyssMusicRules::PhaseStinger`;
  tests `PresentationTests/tests/telegraph_test.cpp` (15 cases), `anim_test.cpp` (+3), `audio_music_test.cpp` (+1) on
  the real `Data/`, the manifest's `materials.M_AF_Telegraph` and `vfx_recipes.json` (100 cases, 6921 assertions on GCC
  and Clang; 23 of 23 hand mutations of the rules caught). Glue: `UAbyssTelegraphSystem`
  (`Public|Private/Vfx/AbyssTelegraphs.*`, ticked by the world builder between the characters and the VFX),
  `UAbyssVfxSystem::SetHandleRadius`, `FAbyssArtManifest::GetAssetJson`, the character actor's play plan / dash slide /
  death runtime, `UAbyssAudioSystem::HandleMonsterPhase`. Content: `M_AF_Telegraph` master (`Scripts/abyss_content`:
  `hlsl.telegraph_node`, `manifest.telegraph_contract`, the two `VFX/` textures in the plan; Scripts tests 125,
  `test_hlsl` runs the node against the art's preview port). Counts after the review fixes: below.
- **What it presents.** Every ground strike's crimson decal with the bone fill reaching the rim on the contact frame
  (sim clock), the style's rim / inner rim / urgency, the contact flash + `telegraph.contact*`, a cancel's fade;
  in Chapter 1 (the themed zone) damaging on-death fuses (Cinderbloat, Scorched) as decals, affix fuses'
  `elite.<type>.death` from the kill; raise / summon windups as `monster.raise.mark`; hazards pulsing with `hazard.*`
  until `untilMs`; Valdras's P3 arena ring closing 9 -> 5 tiles with its wash and `hazard.banner_ring` on the same
  radius; the Lunge as a 200 ms slide; Valdras's P2 / P3 stingers; ability clips with their beat on the core's contact
  (the drummer's loop at rate 1); the Cinderbloat's Death as its 900 ms fuse; `abyss.DebugLeash`. README *Dark remake
  telegraphs* lists the Mac look checks.
- **Interpretations** (DR16): Keen keeps its crimson ring (status-only strikes still draw); Gravecold's fuse draws no
  decal (its `elite.frozen.death` is the pre-cue, DR5-slow); a line is the core's rectangle, not the contract's rounded
  end; the arena's wash is a 250 cm band outside the rim fading over its last 100 cm, dropped on mobile low (the
  quad still spans the disc: budget it as an 18.6 m square of translucency there); unknown
  on-death effects draw (fail visible); pulses run on the sim clock; the texture wrap is done in the shader (the FX
  import clamps); sort priority -1 and an 8 cm lift; caps 24 / 32 / 32 desktop, 16 / 24 / 32 mobile (overflow only).
- **Chapters 2-5.** No ability definitions, so no decals, stingers or rate changes; the global elite on-death fuses (2B)
  and the Labyrinth's volatile curse go off unpresented there, as before 2G (on-death presentation needs a zone with a
  presentation theme). Everything else is unchanged; AbyssCore untouched.
- **Stage T review fixes** (DR16 (11)-(14)). B1: `M_AF_Telegraph`'s five CPD vectors reach `AF_Telegraph` as float4
  (`Graph.float4`: AppendVector(RGB, A); a vector parameter's output 0 is RGB, so `.a` would not compile and no decal
  would draw); `fake_unreal.output_width` models the engine's pin widths and `test_flow` checks every Custom-node input
  of every master. B2: `UAbyssAudioSystem::PlayStinger` takes plain values (the public header named the private
  `AbyssMusicRules::Stinger`: three translation units would not compile); `Scripts/tests/test_module_headers.py` fails
  any public header that names an `Abyss*Rules::` type it does not include or declare. M1: on-death fuses are presented
  only in a themed zone (`PresentsOnDeath`; chapters 2-5 and the Labyrinth, curse included, keep today's look). m1: a
  cancel emitted in the step the contact timer fires (a stun at the contact) still fades the decal without the flash
  (`CancelTimeFor`). m2: blank pixels exit `AF_Telegraph` before any texture sample (`test_hlsl` checks the exit is
  exact on a dense grid); the mobile-low claim is corrected (the arena's quad still spans the disc). m3: the tick reuses
  its scratch buffers (`SelectVisible` with caller buffers). m4: the CPD cache keeps what was sent per float4 group
  (`UpdateCpdCache`). m5: the dead `DashSlideRemaining` is gone (the slide is the actor's dodge-offset ease). Counts after
  the fixes: PresentationTests 102 cases, 7206 assertions on GCC and Clang (5 of 5 hand mutations of the new rules
  caught); Scripts 129 tests.
- **Deferred (stage T).** The optional additive `fx.fuse.glow` swell on top of the Death clip's own swell (the clip and
  its `FX_Fuse` / `FX_Burst` notifies carry the read); a ghost marker at `dashTo` during the Lunge windup (the line
  decal already points there); the darkest-tile readability capture (stage H / mac-look); EvBlock and the other U2 / U3
  / U5 parts belong to the other 2G stages.
- **Stage C: corpses, raising, monster states.** Rules in `Source/Abyssfire/Private/Actors/AbyssCorpseRules.h` (the
  `M_AF_AshDissolve` contract, the corpse block, the ash timeline on the sim clock, removals and their recipes, the story
  hold, the Feed drain, the rise reversal, the burn colour, reconcile, the visible cap, the adoption timeout, the breach
  puddle and its runtime recipe) and `Private/Actors/AbyssMonsterLookRules.h` (flight / shield facing, held items, phase
  looks, aura / body / affix loops, pulse / block / flight chains, the elite sigil and its motion, the champion glyph,
  the death recipe chain, the zone's art warm-up); tests `PresentationTests/tests/corpse_test.cpp` (15 cases) and
  `monsterlook_test.cpp` (12) on the real `Data/` (the 13 roster corpse blocks), `vfx_recipes.json` and the audio
  manifest, `theme_test.cpp` (+ the sigil chain). Glue: `UAbyssCorpseLayer` (`Public/Actors/AbyssCorpses.h`,
  `Private/Actors/AbyssCorpses.cpp`, a world subsystem the world builder ticks after the characters), the character
  actor (corpse-holding death, adoption, posed husks, the ash twins as dynamic instances per slot, the rise reversal, the
  husk shadow on the pelvis, held items beyond the two hands, the sigil marker), the world builder (`ReleaseCorpseActor`,
  `SpawnHuskActor`, flight / shield facing, the sigil chain, the warm-up), `UAbyssVfxSystem` (the death chain,
  `SetHandleBlow`, `PlayRuntimeRecipe`), `UAbyssAudioSystem` (`EvBlock` clang, `EvMonsterFlee` cue), the world layer's
  champion glyph, `FAbyssArtManifest::GetAssetJson` (`eliteMarker`). Content: the twin masters `M_AF_Toon_Ash` /
  `M_AF_Outline_Ash` (`Scripts/abyss_content`: `hlsl.ash_mask_node` / `ash_grey_node`, `manifest.ash_contract`, the
  `T_FX_AshNoise` plan entry; every existing master's material fingerprint unchanged). Counts: PresentationTests 129
  cases, 8549 assertions on GCC and Clang (27 of 27 hand mutations of the two rules headers caught); Scripts 135 tests;
  the strict content plan 0 errors, 0 warnings.
- **What it presents** (README *Dark remake corpses and monster states* lists the Mac look checks). A Chapter 1 kill
  holds its Death pose where it fell, greys and crumbles into an ash husk by plates (sim clock: frozen in a cinematic),
  blows away at the TTL; the removals' looks (expire, raise with the risen body reversing the ash, consume, burn in the
  fire's colour); posed seed husks; the Corpseherd's and Valdras's bodies held through their beats; the gnawer's
  `corpse.consume`, Valdras's Feed; breach-blood tar puddles; blockers' sparks + clang, the shield wall's front; fleeing
  monsters walk / run (the deserter's hissed cue once rendered); the drummer's / Hungerbell's pulses, the bearer's aura
  stain, the Unburied affix's cracks, the Cinderbloat's ash flakes, the zealot's frenzy surge and breath, Valdras's burning
  standard; the Wave 2 sigil over march elites, the champion diamond on pack leaders; family death recipes without
  coins for Raised bodies; the Unburied's kettle helm worn with the weapon (it used to replace it).
- **Interpretations** (DR16 (15)-(27)): the husk is the monster's own actor (seeds / late views get posed actors); the ash
  look is twin masters with per-slot dynamic instances (no free custom-primitive-data slot); a burn's colour is the
  side of the fire's source the core names (`EvCorpseRemoved::burner`): the hero's ember (or its own fire kill's
  impact colour), an enemy's breach violet, none: the recipe's grey; a consumed body crumbles in 350 ms, a Feed drains
  it to .85 by its due time; the cap retires the oldest (seeds and story bodies kept) with `corpse.expire`;
  `monster.block` keeps its authored chest frame with its point on the shield's `face`; the zealot's
  `notify.FX_Frenzy` plays at the phase entry (his row has no enter action, 2F-D8: only a row entering with the clip
  that carries the notify would leave it to that notify); a pulse's family fallback plays only when the clip the
  ability's definition casts with has FX notifies (the drummer's four beats already are the pulse; the Hungerbell's
  call has no clip), violet for the breach-touched, pale dust for the Living and the beasts; the sigil floats in world
  units along world up (absolute scale); the puddle's radius is radiusFactor x the body's half-width
  (`footprintRadiusCm`) x the drawn scale, drawn by a runtime recipe of the shipped `decal` emitter (no art recipe
  carries it); the corpse block's geometry is scaled by the body's drawn scale (hunt leaders x1.25).
- **Chapters 2-5.** No corpse-leaving definitions, families, phases, packs, blockers, auras, puddle looks or
  presentation theme: every kill bursts red with its coins and fades as before, elites wear today's crown; the W1
  slime's puddle decal (no corpse block) stays undrawn. AbyssCore: one additive event field (`EvCorpseRemoved::burner`,
  review C-m2), read by nothing in the sim; the K3 / K4 replay digests and the playthrough's final saves are unchanged.
- **Fixed in passing** (the stage's §6): an asset's attachments beyond `weapon_r` / `weapon_l` (the Unburied's kettle helm
  on `head`) replaced the main-hand weapon; each is now a held item of its own on its socket.
- **Deferred (stage C).** The zealot's scar emissive ramp (`fx.frenzy.scarEmissive` .25 -> .7: needs a per-swatch emissive
  on `M_AF_Toon`, which would change every master's fingerprint; the art review measured the scar at ~1 px at 1080p,
  the breath loop is the read); the sigil's optional additive ember glow (`fx.glow` on its `ember` socket: no glow
  recipe ships for it); the `buffMask` buffed glow (no recipe: no look, as the survey decided); a rendered
  `sfx_shieldbearer_block` / `sfx_deserter_flee` (the audio lane: the clang falls back to `block`, the flee cue is silent);
  the deserter drummer's aura look (the 2F handoff: the Living's no-glow rule; no `monster.deserter_drummer.aura` recipe
  ships - the art lane's follow-up is a dust stain with no light; nothing is drawn until then, the aura still works);
  the Hungerbell's own call recipe (`monster.hunt_hungerbell.supper_bell`: until it ships the supper bell pulses the
  drum's pale dust).
- **Review fixes (stage C).** C-M1 the zealot's frenzy surge plays at the phase entry (the as-built row has no Roar);
  C-m1 the puddle's radius scales the body's half-width, not the husk's footprint (Breachclot 80 -> 48 cm, Cinderbloat
  46 -> 71 cm); C-m2 the burn colour comes from the fire's source (`EvCorpseRemoved::burner`, a core field) instead of
  the frame's hits (no global enemy-hit flag, no 3-tile guess, no hazard probe); C-m3 the pulse's clip test reads the
  ability's definition (not the playing action) and beasts pulse dust, not violet; C-m4 the husk geometry follows the
  body's drawn scale; C-m5 a settled husk stops its body's tick, the cap sorts a scratch buffer, loops are re-turned
  only when the body turned; C-m6 the extra held items get selection-rim copies; C-m7 the drummer's aura gap is
  recorded above. Counts after the review: PresentationTests 131 cases, 8631 assertions on GCC and Clang (19 hand
  mutations of the new rules caught); CoreTests 881 cases on GCC and Clang (the burner checks in `corpses_test`; the
  K3 / K4 digests unchanged; the three playthroughs' final saves byte-identical to HEAD's).

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
