# Port Spec — Quests, NPCs, Dialogue, Story & Chapter 1 Content

Area owner: quests / NPCs / dialogue / story. Web source of truth: branch `claude/unreal-rebuild`, TypeScript under `src/`.
Target: portable C++20 core (`AbyssCore`, no UE types, no exceptions/RTTI) + thin UE5 module (render/input/UI).

This document describes **what the web game does today**, precisely enough to re-implement it without reading the
TypeScript. Where the web behaviour is a bug or a 2D artefact it is called out as a **QUIRK Qn** (§15) with a
recommendation; the default is *keep the behaviour* unless the recommendation says FIX and the open question is resolved
that way. Everything marked **render-only** is presentation; the 3D equivalent is given next to it.

Citations are `path:line` (line numbers at the time of writing).

---

## 0. Scope, boundaries, conventions

### 0.1 What this spec owns

| System | Web source | Core (C++) / UE |
|---|---|---|
| Quest data model, quest state machine, progress, availability, tracking, turn-in | `src/data/types.ts:329-428`, `src/systems/QuestSystem.ts` | `QuestSystem` (core) |
| Quest rewards (fixed items, pick-one gear, embers, pets) | `src/systems/QuestRewards.ts`, `ZoneScene.turnInQuest` (`src/scenes/ZoneScene.ts:4096-4134`) | `QuestRewards` (core) |
| Quest world: gather nodes, clue marks, quest-item pickups, guide arrow | `src/systems/QuestWorld.ts` | `QuestWorld` (core logic) + UE actors |
| Guide routing | `src/systems/QuestGuide.ts` (pure) | `QuestGuide` (core) |
| Explore / talk / escort / defend / craft runtimes | `ZoneScene.ts:3762-3787, 4136-4177, 6579-7121` | `QuestRuntime` (core) |
| NPC definitions, placement, interaction, quest markers | `src/data/npcs.ts`, `src/entities/NPC.ts`, `src/ui/QuestNPCIndicators.ts` | data + `NpcService` (core) + UE actor |
| Dialogue trees (quest NPCs) and linear NPC lines | `src/data/dialogueTrees.ts`, `UIScene.ts:3041-3085, 3398-3725` | `DialogueRunner` (core) + UMG |
| Quest card, tracker, quest log, toasts, banners, popups (UI contracts) | `src/ui/QuestCardUI.ts`, `src/ui/QuestTrackerHUD.ts`, `UIScene.ts` | UMG |
| Story progress, StoryDirector (beats queue, triggers, cinematic freeze, boss intros/bar) | `src/systems/StoryProgress.ts`, `src/systems/StoryDirector.ts`, `src/data/story/*` | `StoryDirector` (core) + `UStoryPresenter` (UE) |
| StoryScene presentation (sequences, chapter cards, cutscenes, letterbox, portraits, whispers, title cards) | `src/scenes/StoryScene.ts` | UMG overlay + camera director (UE) |
| Lore collectibles, hidden areas, story decorations (Chapter 1 content) | `src/data/loreCollectibles.ts`, `src/data/maps/emerald_plains.ts:42-87`, `ZoneScene.ts:4675-5030, 5437-5590` | data + `ExplorationService` (core) |
| Achievements | `src/systems/AchievementSystem.ts` | `AchievementSystem` (core) |

### 0.2 Owned elsewhere (referenced, not repeated)
* Quest **hunt** monster spawning/definition, zone **mini-boss** spawn + pre-fight dialogue, **boss bar** monster side,
  kill-hook order: `monsters-ai.md` §8, §9, §10, §11. This spec only restates the parts quests depend on (§3.8).
* Pick-one gear **item creation** (`createItem`, affixes, candidate bases per level): `loot-items-inventory.md` §4, §5.6.
  Shops (buy/sell/buyback/forge): `loot-items-inventory.md` §12–13. This spec owns the NPC table that lists `shopItems`.
* Pathfinding, map generation (walkability grids that gather spots/clue marks depend on): map/world spec.

### 0.3 Conventions
* **Distances are in tiles** (grid `col,row` floats), Euclidean unless noted. Same as `combat-feel.md` §0. Player position
  is a float tile; NPC tiles are integers.
* **Time** is ms on the monotonic game clock (`scene.time.now`). Periodic checks use either an accumulator
  (`timer -= dt; if timer <= 0 → timer = period`) or the `SimulationScheduler.due(phase, now, period)` rule
  (`src/systems/SimulationScheduler.ts:4-10`: fire when `now >= deadline`, then `deadline = now + period`).
* **RNG**: `Math.random()` (`chance(p)`/`randomInt(a,b)` as in `combat-feel.md` §0). The only deterministic RNG in this
  area is the gather-spot hash (§3.4) — it **must** be bit-exact.
* **Rounding**: JS `Math.round` = `floor(x + 0.5)` (matters for negative offsets in §3.4).
* **i18n**: all player-facing strings are keys resolved through `t(key, params)`; a missing key resolves to the key itself
  and accessors then fall back to the zh-CN text stored in the data (`src/i18n/gameAccessors.ts:202-336`). Port: the same
  “key → fallback” rule.
* Event names below are the web `GameEvents` (`src/utils/EventBus.ts:23-64`); the core should expose the same as typed
  signals (§13).

### 0.4 What Chapter 1 (zone `emerald_plains`, Lv 1–10) needs from this area
* **11 quests** (5 main + 6 side), all given in `emerald_plains` (§10.1): objective kinds used — `kill`, `collect`
  (`drop` and `gather` sources), `explore`, `talk` (delivery), `investigate_clue`, `escort`; plus 2 quest hunts.
  Not used in Ch1 but must stay general: `defend_wave`, `craft_*`, failed→re-accept beyond escort, `monster_killed` /
  `quest_accepted` / `zone_entered` story triggers, `grantPet` triggers.
* **NPCs**: camp (15,15) `blacksmith`, `merchant`, `quest_elder`; camp (95,100) `merchant`; field `plains_herbalist`
  (shop + delivery target), `plains_wanderer` (bounty giver); escort NPC 旅行商人 (quest-spawned).
* **Dialogue**: the elder's branching tree (12 nodes, §10.3); linear lines for every NPC; the mini-boss 哥布林萨满
  pre-fight lines (monsters spec §8.3).
* **Story beats**: prologue (6 slides), chapter card `emerald_plains`, cutscenes `cs_ep_mark`, `cs_ep_whisper`,
  `cs_ep_finale`, boss intro `cs_boss_goblin_chief` + boss bar (§10.2). Cinematic freeze, letterbox, portraits,
  whispers, title cards, skip.
* **Exploration content**: 4 lore collectibles, 1 hidden area, 3 story decorations (§10.4).
* **Achievements** reachable in Ch1 (§10.5) and their stat bonuses.
* **Ley-beast slice** (§18; this is not a later-milestone hook). Turning in `q_pet_sprite_friend` gives `pet_sprite`,
  which in the web is immediately a live companion: it follows, shoots, heals, soaks stray swings, gains exp from
  kills, and its passive adds kill exp. Ch1 therefore needs `PetSystem`, the `PetCompanion` runtime (flying ranged
  beast, `heal` + `shield` abilities, evolution stage 1), the P panel with Ley Fruit feeding, the HUD medallion and
  the `pet_sprite` mesh (stages 0–1). §18.10 is the exact fallback if the companion is cut.
* **Hooks for later milestones that fire in Ch1** (data must be recorded even if the feature ships later):
  embers on every turn-in **and on every elite / mini-boss / affixed kill** (§4.4), `pet_sprite` from
  `q_pet_sprite_friend` (§4.5), Ember Tower unlock on `q_explore_goblin_camp` turn-in and the herb-garden wing on
  `q_secure_plains` turn-in (§4.6), and from then on **herb-garden growth on every kill** (§4.7). All of these are
  saved state that a later milestone reads back, so the Ch1 build must compute them exactly even with no tower UI.

---

## 1. Data model

All tables are exported to JSON verbatim (§14). Field order and array order matter where noted (array order = UI order
and iteration order).

### 1.1 `QuestDefinition` (`src/data/types.ts:329-357`)

| Field | Type | Meaning / consumer |
|---|---|---|
| `id` | string | Unique. i18n: `data.quest.<id>.name`, `.desc`, `.offer`, `.complete` (`.progress` is read by `getQuestStory` but no quest defines it). |
| `name`, `description` | string (zh-CN) | Fallback text. |
| `zone` | string | Map id the quest belongs to. Zone-filters gather nodes, clues, drops, explore checks, hunts, escort/defend spawn, guide. **Not** used to filter kill/talk progress (§2.3). |
| `type` | `'kill'｜'collect'｜'explore'｜'talk'｜'escort'｜'defend'｜'investigate'｜'craft'` | Badge label (`sys.quest.type.<type>`); `escort`/`defend`/`craft` switch on runtimes. |
| `category` | `'main'｜'side'` | Guide priority, tracker/log sort, default reward-choice quality, default embers. |
| `objectives` | `QuestObjective[]` | Ordered. Index = progress slot. |
| `rewards` | `QuestReward` | §1.4. |
| `prereqQuests?` | string[] | All must be `turned_in` to accept. |
| `level` | int | Availability gate: hidden while `level > playerLevel + 5`; base item level for reward choices; escort/defend HP. |
| `questArea?` | `{col,row,radius}` | Minimap circle (main gold / side grey); last-resort guide target; map-gen walkable landmark. |
| `escortNpc?` | `{name, spriteKey, startCol, startRow, destCol, destRow}` | Escort runtime (§3.9). |
| `defendTarget?` | `{name, spriteKey, col, row, totalWaves}` | Defend runtime (§3.10). |
| `clues?` | `{id,name,col,row}[]` | **Only** used as walkable landmarks for map generation (`src/data/maps/index.ts:31`). Clue gameplay uses the objectives' `location`. |
| `craftPhases?` | `{materials:{itemId,name,required}[], craftNpc, deliverNpc}` | Craft runtime (§3.11); `materials` is unused at runtime. |
| `reacceptable?` | bool | A `failed` quest may be accepted again (progress reset). |
| `hunts?` | `QuestHunt[]` | Named quest monsters (monsters spec §9.1). |

### 1.2 `QuestObjective` (`src/data/types.ts:382-395`)

| Field | Type | Meaning |
|---|---|---|
| `type` | `'kill'｜'collect'｜'explore'｜'talk'｜'escort'｜'defend_wave'｜'investigate_clue'｜'craft_collect'｜'craft_craft'｜'craft_deliver'` | Progress kind (§2.3). |
| `targetId` | string | What advances it: monster def id / hunt id (kill), quest-item id (collect), area id (explore), NPC id (talk), clue id, escort id, defend id, craft ids. i18n `data.questTarget.<targetId>`. |
| `targetName` | string (zh) | Fallback label. |
| `required` | int ≥ 1 | Goal count. |
| `current` | int | Always 0 in data (template only; live values are in `QuestProgress`). |
| `location?` | `{col,row,radius}` | explore: trigger circle; investigate_clue: clue spot (radius **unused**, examine range is the constant 2); escort: destination (radius unused, arrival radii are constants). |
| `source?` | `QuestItemSource` | collect / craft_collect only (§1.3). |
| `itemKind?` | string | Icon / world-node look (`QuestItemKind`, `src/graphics/icons/QuestItemIcons.ts:19-22`); default by `targetId` table, else `'relic'` (`src/systems/QuestWorld.ts:80-91`). |
| `labelKey?` | string | i18n key overriding the objective label (none in Ch1). |

Label resolution for any objective (`getQuestTargetName`, `gameAccessors.ts:321-330`): `t(labelKey)` if set and found →
`t('data.questTarget.<targetId>')` if found → `targetName`.

### 1.3 `QuestItemSource` (`src/data/types.ts:403-405`)
```
{ kind: 'drop',   monsters: string[], chance: float 0..1 }          // each qualifying kill rolls once per objective
{ kind: 'gather', area: {col,row,radius}, count: int }              // count deterministic world nodes inside area
```
Quest items are **not inventory items**: collecting only increments the objective counter and plays a pickup animation;
nothing is added to or removed from the bag at any point (`ZoneScene.ts:4073-4094`, `QuestWorld.ts:268-296`).

### 1.4 `QuestReward` (`src/data/types.ts:407-422`)

| Field | Type | Meaning |
|---|---|---|
| `exp`, `gold` | int | Paid on turn-in. |
| `items?` | string[] | Item **base ids**; each entry → one `createItem(id, heroLevel, 'normal')` (duplicates = 2 items, which stack). |
| `petReward?` | string | Pet id granted on turn-in. |
| `embers?` | int | Homestead currency; default when absent: main 2 / side 1 (§4.4). |
| `choices?` | `('weapon'｜'armor'｜'helmet'｜'gloves'｜'boots'｜'belt'｜'jewelry'｜'offhand')[]` | One generated item per slot; the player picks one (§4.2). |
| `choiceQuality?` | `'magic'｜'rare'｜'legendary'` | Default rare for main, magic for side. |

### 1.5 `QuestProgress` (save format, `src/data/types.ts:424-428`)
```
{ questId: string, status: 'available'|'active'|'completed'|'turned_in'|'failed', objectives: { current: int }[] }
```
`'available'` is declared but never stored: “available” is computed (§2.5). A quest with no progress record has never
been accepted.

### 1.6 NPC (`src/data/types.ts:463-474`)

| Field | Type | Meaning |
|---|---|---|
| `id` | string | i18n `data.npc.<id>.name`, `data.npc.<id>.dialogue.<n>`. |
| `name` | string (zh) | Fallback. |
| `type` | `'blacksmith'｜'merchant'｜'quest'｜'stash'` | Interaction (§6.3). |
| `dialogue` | string[] (zh) | Linear lines; `[0]` is logged on every interaction; `[1]` (or `[0]`) is the panel text for a quest NPC with nothing to offer and no tree. |
| `shopItems?` | string[] | Shop wares (loot spec §12). |
| `quests?` | string[] | Quests this NPC **gives and receives** (array order = quest-card order). Invariant: each quest id appears in exactly one NPC, of type `quest`, standing in the quest's zone (`src/__tests__/QuestContent.test.ts:46-51`). |
| `dialogueTree?` | `DialogueTree` | Branching dialogue (§7). |
| `spriteId?` | string | Borrow another NPC's look (tower allies; later milestone). |

Placement is map data, not NPC data: `MapData.camps[] {col,row,npcs[]}` and `MapData.fieldNpcs[] {col,row,npcId}` (§6.2).

### 1.7 Dialogue (`src/data/types.ts:430-461`)
```
DialogueTree   { startNodeId: string, nodes: map<string, DialogueNode> }
DialogueNode   { id, text (zh), choices?: DialogueChoice[], nextNodeId?: string, isEnd?: bool }
DialogueChoice { text (zh), nextNodeId: string, questTrigger?: questId, prereqQuests?: questId[],
                 reward?: { gold?: int, items?: string[], exp?: int } }
```
i18n keys exist for every tree string — `data.dialogue.<npcId>.<nodeId>.text` and `.choice.<i>` (i = index in the
node's `choices`) — but the web renders the raw zh text (QUIRK Q11).

### 1.8 Story script (`src/data/story/types.ts`)
```
Speaker      = {npc: npcId} | 'villain' | 'hero' | {monster: monsterId}
FocusTarget  = 'player' | {npc: npcId} | {monster: monsterId} | {col, row}
CutsceneStep = {kind:'narrate', text} | {kind:'say', speaker, text} | {kind:'whisper', text}
             | {kind:'focus', target, ms? = 900} | {kind:'title', title, subtitle}
             | {kind:'shake', intensity? = 0.01, ms? = 500} | {kind:'flash', color? = 0xffffff, ms? = 300}
             | {kind:'wait', ms}
Cutscene     = { id, steps: CutsceneStep[] }                       // 5–14 steps (test invariant)
StorySlide   = { heading?, title?, text? ('\n' = line break), mood?: 'embers'|'dawn'|'night'|'forge'|'sand'|'abyss'|'light' }
StorySequence= { id, slides[], credits?: bool }
Chapter      = { zoneId, number, title, subtitle, text, mood }       // all text fields are i18n keys
BossIntro    = { monsterId, name, epithet, cutscene }
StoryTrigger = { on:'quest_turned_in', questId, cutscene, grantPet? } | { on:'quest_accepted', questId, ... }
             | { on:'monster_killed', monsterId, ... } | { on:'zone_entered', zoneId, ... }
```
Every text field in the script is an **i18n key** (`story.<id>.<n>`, n = 1-based step/slide index; title steps use
`.<n>.title` / `.<n>.subtitle`). Strings live in `src/i18n/locales/story.ts` (`STORY_ZH` / `STORY_EN`).

### 1.9 Lore, hidden areas, story decorations
* `LoreEntry` (`src/data/loreCollectibles.ts:7-22`): `{id, zone, name (zh), text (zh), col, row, spriteType:
  'ancient_tablet'|'old_scroll'|'crystal_shard'|'carved_stone'|'torn_journal'|'rune_pillar', hidden: bool}`. i18n
  `data.lore.<id>.name/.text`. `hidden` is metadata only (no gameplay effect).
* `HiddenArea` (map data): `{id, name, col, row, radius, startCol?, startRow?, endCol?, endRow?, discoveryText,
  rewards: {type:'chest'|'gold_pile'|'lore', value: string, col, row}[]}`; i18n `data.hiddenArea.<id>.name/.discovery`.
* `StoryDecoration` (`src/data/types.ts:305-315`): `{id, name, description, col, row, spriteType}`; i18n
  `data.storyDeco.<id>.name/.desc`.

### 1.10 `AchievementDefinition` (`src/data/types.ts:476-485`)
`{id, name (zh), description (zh), type: 'kill'|'collect'|'explore'|'level'|'quest', targetId?, required, reward?:
{stat, value}, title?}`; i18n `data.achievement.<id>.name/.desc/.title`.

---

## 2. QuestSystem (core state machine) — `src/systems/QuestSystem.ts`

Owned by the session (`src/game/GameSession.ts:28,41`): created once, all quests registered at boot, survives zone
changes. Fields: `quests: map<id, QuestDefinition>`, `progress: insertion-ordered map<id, QuestProgress>` (order =
first-accept order; re-accept keeps the original position), `trackedQuestId: string|null`, `rewardChoiceCache:
map<questId, ItemInstance[]>` (session only, never saved).

### 2.1 States
```
(no record) --accept--> active --all objectives done--> completed --turn in--> turned_in   (terminal)
                          |  ^                                                                    
                          |  +--accept (only if reacceptable; progress reset)-- failed <--fail--+ (from active only)
```
* `completed` cannot regress (no objective decrement exists outside craft ordering, which only runs on `active`).
* `failQuest` only acts on `active`.
* There is no abandon.

### 2.2 `acceptQuest(id) → bool` (`:59-103`)
```
q = quests[id]; if !q: return false
if progress has id:
    if progress.status == failed and q.reacceptable:
        progress[id] = {id, active, objectives: zeros(len)}            // map key keeps its original order
        log t('sys.quest.reaccepted', {name: q.name}); emit QUEST_ACCEPTED {questId, questName: q.name}; return true
    return false
for pre in q.prereqQuests ?? []: if progress[pre]?.status != turned_in: return false
progress[id] = {id, active, zeros}; log t('sys.quest.accepted', {name}); emit QUEST_ACCEPTED; return true
```
* **No level check** here (the level gate is only in the offer lists, §2.5). Re-accept does **not** re-check prereqs.
* Log/emit use the raw zh `q.name` (QUIRK Q12).

### 2.3 `updateProgress(type, targetId, amount = 1)` (`:109-161`)
```
for (questId, prog) in progress (insertion order):
    if prog.status != active: continue; q = quests[questId]; if !q: continue
    advanced = []
    for i, obj in q.objectives:
        if obj.type == type and obj.targetId == targetId:
            if type == 'talk' and not all(prog[j].current >= obj_j.required for j < i): continue   // delivery gating
            before = prog[i].current
            prog[i].current = min(before + amount, obj.required)
            if prog[i].current > before: advanced.push(i)
    if q.type == 'craft': enforceCraftPhaseOrder(q, prog)                                      // §2.4
    allDone = all(prog[i].current >= obj_i.required)
    for i in advanced:
        if prog[i].current <= 0: continue                                                       // undone by craft ordering
        emit QUEST_PROGRESS {questId, objectiveIndex: i, current, required, targetId, amount, completesQuest: allDone}
    if allDone and prog.status == active:
        prog.status = completed
        emit QUEST_COMPLETED {questId, questName: q.name}; log t('sys.quest.completed', {name})
```
* One call can advance several objectives in several quests (e.g. a `slime_green` kill advances `q_kill_slimes`; the
  same id in two objectives of one quest would advance both).
* **Not zone-filtered**: a kill/talk anywhere counts for any active quest. Zone filtering happens in the callers that
  need it (drops, gathers, clues, explore, escort, defend).
* Overflow is clamped; no event when nothing advanced.
* Event order within one call: all `QUEST_PROGRESS` of a quest, then that quest's `QUEST_COMPLETED`, then the next quest.

### 2.4 Craft phase ordering (general; not used in Ch1) (`:285-315`, label `:320-340`)
After applying progress to a `craft` quest: `collectDone` = every `craft_collect` objective complete; `craftDone` = every
`craft_craft` complete. If `!collectDone` → every `craft_craft` and `craft_deliver` current := 0. If `!craftDone` →
every `craft_deliver` current := 0. Phase label: `sys.quest.phase.collect` until collectDone, then `.craft` until
craftDone, then `.deliver`.

### 2.5 Offer lists (three near-identical rules — keep them consistent)
* `getAvailableQuests(npcQuestIds, heroLevel)` (`:203-228`, minimap): ids in NPC order → definition exists; `level <=
  heroLevel + 5`; no record **or** (`failed` and `reacceptable`); all prereqs `turned_in`.
* `gatherNpcQuests(npcQuestIds, …, heroLevel)` (`src/ui/QuestCardUI.ts:83-129`, the quest card): **turn-ins first**
  (`completed`, never level-gated), then available (no record + prereqs met + level gate, or failed+reacceptable — note:
  the failed branch does **not** re-check prereqs). Within each group NPC list order. `active`/`turned_in` excluded.
* `computeNPCIndicator` (`src/ui/QuestNPCIndicators.ts:33-94`, the overhead marker): §5.6.

### 2.6 Lists, tracking, guidance selection
* `getActiveQuests()` (`:230-239`): records with status `active` or `completed`, in insertion order, definitions found.
* `setTracked(id|null)` (`:242-246`): no-op if unchanged; else set and emit `QUEST_TRACKED_CHANGED {questId}`. Called when
  a quest is accepted **from the quest card** (not from a dialogue-tree choice) and when the player taps a tracker title;
  cleared when the tracked quest is turned in. Not saved.
* `getGuidedQuest(zoneId)` (`:253-260`): `open = getActiveQuests().filter(zone == zoneId)`; the pinned one if it is in
  `open`; else the first `main` in `open`; else `open[0]`; else null. (Completed quests count — they lead back to the
  giver.)

### 2.7 `failQuest(id)` (`:173-185`)
Only from `active`: status = failed; emit `QUEST_FAILED {questId, questName}`; log `sys.quest.failed`.

### 2.8 `turnInQuest(id) → QuestReward|null` (`:187-201`)
Only from `completed`: status = turned_in; if tracked → `setTracked(null)`; log `t('sys.quest.turnedIn', {name, exp,
gold})`; emit `QUEST_TURNED_IN {questId, questName}`; return the reward definition. Paying the reward is the caller's job
(§4.1).

### 2.9 Save / load (`:262-278`)
* Save: `progress` values in map order → `SaveData.quests`.
* Load: clear; for each record: if the quest exists **and** status is `active`/`completed` **and** the saved objective
  count differs from the definition → reset to `active` with zeros (quest redesigned since the save). `turned_in` /
  `failed` records are kept as-is; records for unknown ids are kept (and ignored by every list).
* Not saved: `trackedQuestId`, `rewardChoiceCache`, gathered-spot sets (§3.4), escort/defend runtime (§3.9–3.10).

---

## 3. Progress sources (who calls `updateProgress`)

| Objective type | Caller | Condition | Zone-filtered | Rate |
|---|---|---|---|---|
| `kill` | `onMonsterKilled` (`ZoneScene.ts:3845`) | `monster.definition.id == targetId` (hunt leaders carry the hunt id) | no | per kill |
| `collect` (drop) | `rollQuestDrops` (`:4073-4094`) | §3.3 | quest.zone == current map | per kill |
| `collect` / `craft_collect` (gather) | `QuestWorld.gather` (`QuestWorld.ts:268-282`) | hero within 1.3 tiles of a node | yes | per frame |
| `explore` | `checkExploreQuests` (`ZoneScene.ts:3762-3787`) | hero within `location.radius` | yes | every 500 ms |
| `talk` | `interactNPC` (`:4142`) | NPC id == targetId; earlier objectives done | no | per interaction |
| `investigate_clue` | `QuestWorld.examine` (`QuestWorld.ts:217-241`) | hero within 2 tiles of the clue mark | yes | per frame |
| `escort` | `updateEscortNpc` (`:6739-6839`) | §3.9 | yes | per frame |
| `defend_wave` | `updateDefendQuest` (`:6947-7015`) | §3.10 | yes | per frame |
| `craft_craft` / `craft_deliver` | `advanceCraftQuestFromNpc` (`:7074-7121`) | §3.11 | no | per interaction |

All per-frame and periodic observers stop while the world is frozen (cinematic, §8.4) and do not run inside the Abyss
Labyrinth (`QuestWorld` is not created there, `ZoneScene.ts:486`).

### 3.1 Kill
Order inside the kill hook (`ZoneScene.ts:3789-3929`, full list in monsters spec §11): exp/gold → … → achievements
`kill` (§9) → **`updateProgress('kill', defId)`** → `StoryDirector.onMonsterKilled(defId)` → **`EmberTower.onKill`**
(kill embers §4.4.2, garden §4.7, expedition §4.7.3 — runs in Ch1, it is not a later hook) → difficulty check → … →
**`rollQuestDrops`** → equipment loot. Elite/difficulty variants keep the base id; hunt leaders use `huntId` (so they do
**not** count for `goblin` kill objectives); hunt minions are plain ids and do count.

### 3.2 Quest-item pickup visual (render-only) — `dropToPlayer` / `flyTo` (`QuestWorld.ts:287-320`)
Icon (26 px) appears 16 px above the kill point, hops to −46 px in 220 ms (Quad.easeOut), then flies to the hero's chest
(`hero.y − 30 px`, re-targeted every frame) over 420 ms (Cubic.easeIn) with a sine arc of 18 px and scale → 55 %, then a
gold sparkle burst (glow 260 ms + 6 sparks 380 ms). **3D:** small item mesh/billboard pops ≈ 0.7 tile up, homes into the
hero socket `chest` over 0.42 s along a slight arc, scales to 55 %, sparkle Niagara burst. Progress is applied
**immediately** at the kill, not when the flight lands.

### 3.3 Drop rolls — `rollQuestDrops(monster)` (`ZoneScene.ts:4073-4094`)
```
for (quest, prog) in getActiveQuests():
    if prog.status != active or quest.zone != currentMap: continue
    legacyRolled = false
    for i, obj in quest.objectives:
        if obj.type not in {collect, craft_collect} or prog[i].current >= obj.required: continue
        if obj.source?.kind == gather: continue
        if !obj.source and obj.type == craft_collect: continue           // craft mats need explicit sources
        chance = questDropChance(obj, monster.defId)                    // QuestRewards.ts:136-142
        if !obj.source: { if legacyRolled: continue; legacyRolled = true }
        if chance <= 0 or random() >= (obj.source ? chance : 0.25): continue
        dropToPlayer(monster.pos, obj); updateProgress(obj.type, obj.targetId)
```
`questDropChance`: not a collect objective → 0; no source → `FALLBACK_COLLECT_CHANCE = 0.25` (any monster in the quest's
zone; only the first unfinished source-less objective per quest rolls); gather source → 0; drop source → `chance` if the
monster id is listed, else 0. Each qualifying objective rolls **independently** (one `random()` each). Draw order:
active-quest insertion order, then objective order.

### 3.4 Gather nodes (`QuestWorld.ts:128-148, 245-282`; spots `QuestRewards.ts:103-127`)
**Spots** — deterministic per `(questId, objectiveIndex)`, stable across visits and saves:
```
resolveGatherSpots(area, count, walkable, seedKey = "<questId>:<objectiveIndex>"):
    h: uint32 = 2166136261
    for each UTF-16 code unit c of seedKey: h = (h ^ c) * 16777619  (mod 2^32)          // FNV-1a
    rand(): h = (h ^ (h >> 15)) * 2246822519; h = (h ^ (h >> 13)) * 3266489917; h ^= h >> 16
            return h / 4294967296.0                                                      // all uint32 arithmetic
    out = []
    for attempt in 0 .. count*60-1 while len(out) < count:
        a = rand() * 2π;  r = sqrt(rand()) * area.radius
        col = jsround(area.col + cos(a) * r);  row = jsround(area.row + sin(a) * r)     // jsround(x) = floor(x + 0.5)
        if !walkable(col,row): continue
        if any(|p.col-col| + |p.row-row| < 3 for p in out): continue                   // ≥ 3 Manhattan apart
        out.push({col,row})
    return out
```
Test vectors (§16): seed for `"q_herb_gathering:0"` = 3613349914 after FNV; first six `rand()` = 0.924380941,
0.017937220, 0.933055474, 0.561144891, 0.710008817, 0.978309497.

**Node lifecycle** (`sync`, `:128-148`): run on construction (zone entry) and on `QUEST_ACCEPTED / COMPLETED / TURNED_IN /
FAILED / TRACKED_CHANGED` (**not** on `QUEST_PROGRESS`). Wanted nodes = for every `active` quest in this zone, every
collect/craft_collect objective with a gather source and `current < required`, every spot index not in this visit's
`gathered[questId:i]` set. Nodes not wanted are destroyed. `gathered` is per QuestWorld instance (per zone visit): on
re-entry all ungathered-this-visit spots reappear, progress is kept, so a 5-of-7 objective can always be finished.

**Gather** (`update`, `:400-404`, per frame): every node with `hypot(node − hero) <= 1.3` tiles (`GATHER_RANGE`) →
add spot to `gathered`, remove node, burst + fly icon to the hero, `updateProgress(obj.type == craft_collect ?
craft_collect : collect, obj.targetId)`. If the objective is already full (possible in multi-objective quests because
`sync` doesn't run on progress) the node is still consumed with no progress (QUIRK Q5).

Render-only: glow ellipse tinted `0xffd98a` (alpha 0.75 ↔ 0.35, 900 ms yoyo) under a 30 px item icon that bobs 6 px
(900 ms yoyo, Sine, phase delay `(spotIndex·173) mod 900` ms). **3D:** item-kind pickup mesh (herb tuft, gel blob…)
hovering ≈ 0.3 tile above ground with a ground glow decal, same bob/phase; collision-free (proximity trigger only).

### 3.5 Clues — `investigate_clue` (`QuestWorld.ts:153-241`)
* Marks are synced together with nodes: one mark per `active` quest in this zone × `investigate_clue` objective with a
  `location` and `current < required`. Position = `nearestWalkable(location.col, location.row, walkable, maxRing = 8)`
  (`:62-73`: the tile itself if walkable, else rings 1..8, row-major `dr = −r..r` outer, `dc = −r..r` inner, only cells
  with `max(|dc|,|dr|) == r`; first walkable wins; none → no mark).
* **Examine** (per frame): mark within `hypot <= 2` tiles (`CLUE_RANGE`; the objective's own `radius` is ignored) →
  remove mark (fade 380 ms), burst, log `t('zone.quest.clueFound', {targetName})`; if `data.questClue.<targetId>`
  exists, log it too and float `"<name>\n<note>"` above the spot (fade in 300 ms, hold, fade out 600 ms starting at
  3600 ms); then `updateProgress('investigate_clue', targetId)`.
* Clues may be examined in **any order** (the guide leads in objective order). A hunt with `revealAfterPrevious` waits
  for all of them (§3.8).
* Render-only: magnifying-glass icon (26 px) 18 px above a cyan ground ring `0x8fe0ff`, icon bobs 6 px and rocks ±8°,
  1100 ms yoyo. **3D:** floating magnifier glyph (or sparkle) above a cyan ground decal.

### 3.6 Explore (`ZoneScene.ts:3762-3787`)
Every 500 ms (`SimulationScheduler` phase `quest-observers`, shared with the NPC-marker refresh): for each `active` quest
in this zone, each `explore` objective with `location` and `current < required`: if `hypot(hero − location) <=
location.radius` → `updateProgress('explore', targetId)` then log `t('zone.quest.exploreFound', {targetName})`. All of a
quest's explore objectives are checked (any order).

### 3.7 Talk (delivery)
`interactNPC(npc)` (`ZoneScene.ts:4136-4177`) calls `updateProgress('talk', npc.id)` on **every** interaction with any
NPC type (shop NPCs included), before opening anything. A `talk` objective only advances once all objectives before it
are complete (§2.3). Ch1: `q_collect_slime_gel` delivers to `plains_herbalist` (a merchant; the shop opens too).

### 3.8 Quest hunts (summary; full rules in monsters spec §9)
`huntsToSpawn` on zone entry (silent), on `QUEST_ACCEPTED` of a quest in this zone (silent) and on **every**
`QUEST_PROGRESS` (announced: log `t('zone.quest.huntRevealed', {name})` + camera shake 260 ms / 0.004). Due = quest active,
its kill objective unfinished, and (if `revealAfterPrevious`) every earlier objective complete. Leaders/minions never
respawn; kill credit uses `huntId`. Guide for a hunt kill objective: nearest living hunt monster → (no map spawn exists)
→ the hunt's `(col,row)` → questArea.

### 3.9 Escort runtime (`ZoneScene.ts:6667-6866`)
State (scene-local, not saved): `escortQuestId`, float tile `(c,r)`, `dest`, `hp`, `maxHp`, `joined`, `path[]`,
`repathAt`. Constant `ESCORT_CATCH_UP_TILES = 14` (`:109`).

**Spawn** (`spawnEscortNpc`, `:6673-6727`) — on zone entry and on `QUEST_ACCEPTED` of an escort quest in this zone when
none is out (`:6587-6594`): first `getActiveQuests()` entry with `type == escort`, status `active`, zone == current,
`escortNpc` set (**only one escort at a time**): position = start tile, `joined = false`, `hp = maxHp = quest.level × 20 +
100` (Ch1: 200); log `t('zone.escort.npcAppeared', {npcName})`.

**Per frame** (`updateEscortNpc(now, dt)`):
```
d = hypot(hero − escort)
if !joined:
    if d > 5: return                                    // waits at the start tile (no damage, no arrival check)
    joined = true; log t('zone.escort.joined', {npcName})
if d > 14:            escort = findWalkableNear(round(hero), 2) (rings 1..2, centre excluded); path = []   // teleport catch-up
elif d > 2:
    if now >= repathAt or path empty:
        repathAt = now + 400
        p = findPath(round(escort), round(hero)); path = p[1 : max(1, len(p) − 1)]       // stop one tile short
    budget = (hero.moveSpeed / 38) × 0.9 × dt/1000       // tiles; 120 → 2.842 tiles/s
    walk along path consuming budget (straight segments between tile centres)
else: path = []
for m in monsters (alive):                               // abstract damage: monsters never target the escort
    if distSq(m, escort) < 16 and m.isAggro() and now − m.lastEscortAttack > 2000:   // lastEscortAttack starts −∞
        dmg = max(1, floor(m.def.damage × 0.3)); hp −= dmg; m.lastEscortAttack = now; float damage number
        if hp <= 0: handleEscortNpcDeath(); return
if distSq(escort, dest) <= 25 and distSq(hero, dest) <= 36:
    for obj in quest.objectives where type == escort: updateProgress('escort', obj.targetId)
    log t('zone.escort.complete', {npcName}); destroy escort
```
**Death** (`:6848-6866`): death burst + 600 ms shrink/fade; `failQuest` → `QUEST_FAILED`; log `zone.escort.npcDied`.
The quest is `reacceptable` → the giver offers it again (fresh progress, escort respawns at the start tile).

Notes: a zone exit, reload or hero death (scene restart) respawns the escort at its start with full HP and
`joined = false`; the quest stays active. The escort is not blocked by the safe zone. HP bar colour: >50 % green
`0x27ae60`, >25 % orange `0xf39c12`, else red `0xe74c3c`; name label in `#e67e22` (render-only → world-space UMG widget).

### 3.10 Defend runtime (general; Ch2/Ch5 content) (`ZoneScene.ts:6869-7071`)
* Spawn (zone entry / accept): first active `defend` quest in this zone with `defendTarget`. `wave = progress of the
  defend_wave objective` (resumes), `hp = maxHp = level × 30 + 200`, timer = 0, no active wave; log
  `zone.defend.targetNeedsProtection`.
* Per frame: if `wave >= totalWaves` and no wave active → log `zone.defend.allWavesCleared`, remove target (quest already
  completed by progress). If no wave active and `distSq(hero, target) < 225`: if `timer == 0` → `timer = now`; when
  `now − timer > 5000` → spawn wave `wave`, active, timer = 0, log `zone.defend.waveIncoming {current: wave+1, total}`.
  (Leaving the 15-tile radius does not reset a started timer.)
* Wave `i` (0-based): `n = 3 + i` monsters at angles `2πk/n`, radius 8, `round()`ed, clamped to `[2, cols−3] ×
  [2, rows−3]` (**no walkability check**); each a uniformly random def from the zone's monster list (bosses included —
  QUIRK Q8), difficulty-scaled, then `hp = floor(hp × (1 + 0.3i))`, `damage = floor(damage × (1 + 0.2i))`, state
  `chase`, no affixes, never respawn.
* While active: when every wave monster is dead → `wave++`, inactive, `timer = now`, `updateProgress('defend_wave',
  targetId)`. Else each living wave monster with `distSq(m, target) < 9` and `now − m.lastDefendAttack > 2000`
  (`lastDefendAttack` starts 0) deals `max(1, floor(damage × 0.2))`; at `hp <= 0` → fail (800 ms shrink) + log
  `zone.defend.targetDestroyed`.

### 3.11 Craft runtime (general; Ch3/Ch4 content) (`ZoneScene.ts:7074-7121`)
On every NPC interaction, for each active `craft` quest with `craftPhases`: if the NPC is `craftNpc` and all
`craft_collect` are done and the first `craft_craft` is unfinished → `updateProgress('craft_craft', id)` + log
`zone.craft.complete`. If the NPC is `deliverNpc` and all `craft_craft` are done and the first `craft_deliver` is
unfinished → `updateProgress('craft_deliver', id)` + log `zone.deliver.complete`. (Both can happen in one interaction if
the same NPC holds both roles.)

---

## 4. Turn-in and rewards

### 4.1 `turnInQuest(questId, choiceIndex = 0) → bool` (`ZoneScene.ts:4112-4134`) — exact order
```
choices = getQuestRewardChoices(questId)          // generate-or-cache BEFORE the status change (§4.2)
reward  = questSystem.turnInQuest(questId)        // → turned_in, untrack, log, emit QUEST_TURNED_IN (§2.8)
if !reward: return false
hero.addExp(reward.exp)                           // may level up (PLAYER_LEVEL_UP → level achievements)
hero.gold += reward.gold
granted = [createItem(id, hero.level, 'normal') (identified) for id in reward.items]
chosen = choices[clamp(choiceIndex, 0, len−1)];  if chosen: granted.push(chosen)
for item in granted: if !inventory.addItem(item): stash.push(item)   // overflow ignores stash capacity
                     log t('zone.quest.rewardItem', {name})
rewardChoiceCache.delete(questId)
if reward.petReward: pets.addPet(reward.petReward)                   // §4.5
achievements.update('quest')                                         // §9
autoSave()
```
Synchronous listeners of `QUEST_TURNED_IN` run **before** exp/gold are paid: StoryDirector (§8.2 — enqueues the
cutscene), EmberTower (embers + unlocks, §4.4/§4.6), tracker/NPC-marker refresh, quest-world sync, audio.

### 4.2 Pick-one gear (`src/systems/QuestRewards.ts`; item creation rules in loot spec §5.6)
* `getQuestRewardChoices(id)` (`ZoneScene.ts:4096-4105`): cached per session; otherwise generated and cached. The quest
  card calls it when it renders a **turn-in** card, so reopening the card never rerolls; a reload does (QUIRK Q9).
* `generateRewardChoices(quest, classId, heroLevel)` (`:74-91`):
  `L = max(quest.level, min(heroLevel, quest.level + 5))` (`rewardItemLevel`, `:65-67`);
  `quality = rewards.choiceQuality ?? (main ? rare : magic)` (`:69-71`);
  for each slot in `rewards.choices` order: `base = pickRewardBase(slot, classId, L)`; skip if none;
  `item = createItem(base.id, L, quality)`; mark identified.
* `pickRewardBase(slot, classId, L, rand)` (`:48-62`): candidate pool by slot (`:22-41`) — `weapon`: weapon bases whose
  `slot == weapon` and `weaponType ∈ CLASS_WEAPON_TYPES[class]` (warrior sword/axe/mace, mage staff/wand, rogue
  dagger/bow; unknown class → sword); `offhand`: shields for warrior, accessories otherwise; `jewelry`: accessories;
  armour slots: armour bases of that slot. `usable = pool.filter(levelReq <= L + 2)`; if empty → the single lowest
  `levelReq` base. Sort `usable` by `levelReq` descending (stable), keep the first 3, pick `floor(rand() × n)`.
* UI: the selection defaults to index 0; the player taps a slot to change it.

### 4.3 Fixed items
`createItem(baseId, heroLevel, 'normal')` per entry, identified. Ch1 uses `c_hp_potion_s` (小型生命药水, stackable) and
`c_hp_potion_m` (中型生命药水).

### 4.4 Embers (homestead currency; the tower itself is a later milestone)
Embers live in `HomesteadTower.embers` (int ≥ 0, `src/systems/HomesteadTower.ts:23`), saved as `SaveData.homestead.embers`
(§11). There are exactly two sources in Ch1: **quest turn-ins** (§4.4.1) and **kills** (§4.4.2). (Later milestones add
expedition claims `+embers`, `HomesteadTower.ts:151`, and spend them on wing upgrades / altar blessings.) Every add goes
through `addEmbers(n)` (`HomesteadTower.ts:82-86`): `add = max(0, floor(n)); embers += add; return add`. **Neither
source is gated on the tower being unlocked** — embers accrue from the first kill / turn-in of a new game.

The `EmberTower` object that owns both hooks is created by `ZoneScene.create` for **every** zone — field zones,
sub-dungeons and labyrinth floors alike (`ZoneScene.ts:519-547`; its `regularZone` flag only controls the hearthstones),
so kill embers are earned everywhere, not only in field zones.

#### 4.4.1 Quest turn-in embers
On `QUEST_TURNED_IN` (`src/systems/EmberTower.ts:197-203`): `n = addEmbers(embersForQuest(q))` with
`embersForQuest(q) = q.rewards.embers ?? (q.category == main ? 2 : 1)` (`src/data/homestead.ts:94-97`); if `n > 0` log
`t('homestead.log.questEmbers', {n})` (type `loot`). Not shown on the quest card. Then the same listener runs
`syncUnlocks` (§4.6). Ch1 quest embers sum to **20** (main 2+2+2+2+5 = 13; side gel 1, herbs 1, pendant 1, bounty 1,
sprite 2, escort 1 = 7) — this is the *quest* total only, not the chapter's ember income (§4.4.3).

#### 4.4.2 Kill embers — `EmberTower.onKill(def, eliteAffixCount, at)` (`src/systems/EmberTower.ts:187-195`)
Called from the kill hook on **every** kill (`ZoneScene.ts:3847`, after `storyDirector.onMonsterKilled`, before the
difficulty-completion check, the ley-fruit drop, `rollQuestDrops` and loot; full order monsters spec §11 step 7 — the
"Ember Tower embers (later)" wording there and in `combat-feel.md` §13.1 is superseded: this hook is live in Ch1).
Arguments: `def = monster.definition` (the live, possibly difficulty-scaled / hunt / affix-modified copy — all three keep
the `elite` / `isMiniBoss` flags: `DifficultySystem.ts:198-210` and `Monster.ts:389-403` spread the def),
`eliteAffixCount = monster.eliteAffixes.length`, `at = monster.sprite` position (world px at death).
```
onKill(def, affixCount, at):
    n = tower.addEmbers(embersForKill(def, affixCount))
    if n > 0: floatText(at.x - 18, at.y - 52, t('homestead.float.embers', {n}), '#ff9a4a')   // no log line
    tower.onKillGarden()                       // §4.7 (Math.random draws only on a yield kill)
    if tower.onKillExpedition(): log 'homestead.log.expeditionBack'                         // §4.7.3, not Ch1

embersForKill(m, affixCount) (src/data/homestead.ts:87-92) — first match wins:
    m.elite        → 5
    m.isMiniBoss   → 3
    affixCount > 0 → 1
    else           → 0
```
* The `elite` check comes first, so a def that is both `elite` and `isMiniBoss` gives **5**. Every shipped mini-boss def
  is also `elite: true` — zone mini-bosses (`src/data/miniBosses.ts`, e.g. `:23-24`), quest hunts
  (`makeHuntDefinition`, `src/systems/QuestHunts.ts:61-63`), sub-dungeon bosses (`src/data/subDungeons.ts:69-71`),
  labyrinth gatekeepers/keepers (`DungeonSystem.ts:408-409`, `dungeonData.ts:85-86,120-121`) — so the `3` branch is
  unreachable with shipped data (QUIRK Q16). It must still be implemented (unit-tested in
  `src/__tests__/Homestead.test.ts:124-127`).
* Elite affixes are rolled only for `def.elite` monsters in field zones (spawn `ZoneScene.ts:4453-4459`, respawn
  `:5628-5633`, mini-boss `:4628-4631`, hunt leader `:6620-6621`, sub-dungeon boss `:4601-4604`), so in field zones the
  `1` branch never fires either (an affixed monster is already elite → 5). It fires only on labyrinth floors whose curse
  has `eliteChance`, which gives affixes to non-elite monsters (`ZoneScene.ts:998-1003`). Hunt minions and ambush
  monsters never get affixes (`:6630-6642`, `:3264-3304`; Ch1 ambush pool `slime_green`/`goblin`, `RandomEventSystem.ts:111`).
* Float text (render-only): `ZoneScene.ts:541-546` — 12 px Cinzel, colour `#ff9a4a`, 2 px black stroke, depth = floating
  text layer; tween `y → y − 30`, `alpha → 0`, 1400 ms, `Power2` (ease-out quad), destroyed on complete. Placed 18 px left
  and 52 px above the corpse, i.e. just above the `+EXP` (y − 40) and `+gold` (x + 15, y − 28) floats. zh `+{n} 余烬`,
  en `+{n} Embers` (`src/i18n/locales/homestead.ts:17,125`). 3D: the same world-space floating-text widget used for
  `+EXP`/`+G`, anchored at the corpse's overhead point, a step higher and slightly left of the EXP float, same timing/colour.
  It is shown even while the HUD ember counter is still hidden (tower locked) — web behaviour; see OQ10.

**Chapter 1 kill table** (normal difficulty; harder difficulties keep the flags, so the same values):

| Monster | Flags | Embers / kill | How often it can be killed |
|---|---|---|---|
| `slime_green`, `goblin` (incl. ambush spawns, hunt minions) | — (never affixed) | 0 | — |
| `goblin_chief` 哥布林首领 (regular spawn (15,95), `src/data/maps/emerald_plains.ts:27`; `elite`, `monsters/emerald_plains.ts:50`) | elite, 1 affix | **5** | respawns 15 s after every kill (monsters spec §7, Q8) → **unbounded** |
| `miniboss_goblin_shaman` 哥布林萨满 ((60,55)) | elite + isMiniBoss, 1 affix | **5** | once per zone entry (re-spawned on each entry, never within a visit; monsters spec §8.2) |
| `hunt_pendant_thief` 小贼斯尼克 (`q_lost_pendant`) | elite + isMiniBoss (hunt), 1 affix | **5** | once (spawns only while its objective is due) |
| `hunt_redcap_gruk` 红帽格鲁克 (`q_bandit_trouble`) | elite + isMiniBoss (hunt), 1 affix | **5** | once (same) |

#### 4.4.3 Chapter-1 ember balance and the HUD
`embers = Σ embersForQuest(turned-in quests) + 5 × (elite kills) + 1 × (non-elite affixed kills — none in Ch1)`, from the
start of the game, minus nothing (no Ch1 sink). So **"Ch1 total: 20" is only the quest part**; the actual amount is
`20 + 5 × k`, where `k` counts elite kills: a player who finishes every Ch1 quest has `k ≥ 3` (the chief for M4, and
the two hunt leaders for `q_lost_pendant` / `q_bandit_trouble`); the shaman is optional; every further chief or shaman
kill adds 5, and since the chief respawns every 15 s there is no upper bound.

HUD (save-ui spec §6.9; `UIScene.ts:5847-5852`): the info plate shows `"✦" + tower.embers` once `tower.towerUnlocked`
(i.e. right after `q_explore_goblin_camp` is turned in), otherwise empty. The first value it shows is therefore **not**
6 (M1+M2+M3) in general: it is `6 + (side-quest embers turned in so far) + 5 × (elite kills so far)`. Example: a hero who
turned in M1–M3, the gel and herb quests, killed Gruk and the shaman once and the chief once before turning in M3 sees
`6 + 2 + 15 = ✦23`. Integration tests must compute the expected HUD value from the same formula, not from quest data
alone.

### 4.5 Pet reward
`PetSystem.addPet(petId)`: unknown id → no-op; already owned → log `sys.pet.duplicate`, no-op; else append `{petId,
level 1, exp 0, evolved 0, bond 0, bondProgress 0}`, becomes active if none, log `t('sys.pet.obtained', {name})`, emit
`PET_OBTAINED` + `PET_CHANGED`. Ch1: `pet_sprite` (灵脉精灵) from `q_pet_sprite_friend`.

This is **not** record-only. Since it is the first beast it becomes active, and from the next frame the web runs it as
a live companion. Its passive (`expBonus`) applies to every later kill. The quest card shows 「宠物」 in the reward line.
`PET_OBTAINED` has no listener, so there is no toast; only the log line appears and the HUD medallion becomes
visible. The full contract is in §18 (exact turn-in flow §18.7; degraded contract if the companion is cut §18.10).

### 4.6 Later-milestone unlocks triggered by Ch1 turn-ins (record only)
`TOWER_UNLOCK_QUEST = 'q_explore_goblin_camp'` (`src/data/homestead.ts:18`): on its turn-in the Ember Tower opens (log
`homestead.log.towerUnlocked`, hearthstone by each camp). `q_secure_plains` is the `unlockQuest` of the herb-garden wing
(`tower_herbalist`, `homestead.ts:33`; log `homestead.log.wingUnlocked`). The *unlocked* flags derive from the set of
`turned_in` quests, but the unlock **also writes saved state**: a wing that becomes unlocked is raised to level 1 for
free, which (a) is saved in `homestead.buildings` and (b) starts herb-garden growth (§4.7). So the Ch1 build must run
`syncUnlocks` too.

`HomesteadTower.syncUnlocks(turnedIn)` (`src/systems/HomesteadTower.ts:58-68`), called by `EmberTower` in its
constructor (every zone entry / load, `EmberTower.ts:108-110,145-147`) and in the `QUEST_TURNED_IN` listener after the
quest embers (`:204-215`), with the ids of all quests whose status is `turned_in` (QuestSystem sets the status before
emitting, `QuestSystem.ts:193-199`):
```
before = { b.id : b in BUILDINGS, isBuildingUnlocked(b.id) }          // evaluated with the OLD turned-in set
turnedIn = set(turnedIn)
fresh = []
for b in BUILDINGS (array order: herb_garden, pet_house, gem_workshop, training_ground, altar, warehouse):
    if !b.unlockQuest or !isBuildingUnlocked(b.id): continue            // warehouse (no unlockQuest) is skipped: stays Lv0
    if buildings[b.id] <= 0 (missing = 0): buildings[b.id] = 1          // free Lv1, no event, no log here
    if b.id ∉ before: fresh.push(b.id)
return fresh                                                             // listener logs wingUnlocked per fresh id
isBuildingUnlocked(id) = def exists and (!def.unlockQuest or turnedIn.has(def.unlockQuest))
towerUnlocked          = turnedIn.has('q_explore_goblin_camp')
```
Notes: on the first `syncUnlocks` after a load `before` is computed from the reset (empty) set, so every already-unlocked
wing is "fresh" — but the constructor ignores the return value, so nothing is logged. A wing level is never lowered.
In Ch1 the only effect is `buildings.herb_garden = 1` from the `q_secure_plains` turn-in (then autosaved by
`turnInQuest`, §4.1). Its `bonusPerLevel` (`potionDiscount` 5/level) enters `HomesteadSystem.getTotalBonuses()` but is
read nowhere (QUIRK Q17) — no shop price changes.

### 4.7 Kill-driven homestead state (herb garden; expedition counter)
Both run inside `EmberTower.onKill` (§4.4.2) on every kill, in any zone, with no tower-unlock gate of their own. Neither
produces a log line or float text; `onKillGarden`'s return value is ignored by `EmberTower`.

#### 4.7.1 Herb-garden growth — `HomesteadTower.onKillGarden(rng = Math.random)` (`src/systems/HomesteadTower.ts:94-105`)
State: `garden = { progress: int, stock: { itemId: count } }` (`:26`), saved as `SaveData.homestead.garden` (§11).
```
lv = buildings.herb_garden ?? 0
if lv <= 0 or !isBuildingUnlocked('herb_garden'): return null    // Ch1: true until q_secure_plains is turned in
if Σ stock.values >= gardenCapacity(lv): return null              // full: progress does NOT advance
progress += 1
if progress < gardenInterval(lv): return null
progress = 0
id = rollGardenYield(lv, rng); stock[id] = (stock[id] ?? 0) + 1; return id

gardenInterval(lv) = max(6, 16 − 2·lv)                     // kills per yield   (src/data/homestead.ts:102-104)
gardenCapacity(lv) = lv <= 0 ? 0 : 4 + 4·lv                // max items held   (:107-109)
rollGardenYield(lv, rng):                                  // (:112-117) — 1 or 2 draws
    if rng() < 0.12 + 0.03·lv: return 'c_ley_fruit'        // LEY_FRUIT_ID (:20)
    if rng() < 0.6:  return lv >= 4 ? 'c_hp_potion_l' : lv >= 2 ? 'c_hp_potion_m' : 'c_hp_potion_s'
    else:            return lv >= 2 ? 'c_mp_potion_m' : 'c_mp_potion_s'
```

| garden Lv | kills / yield | capacity | P(ley fruit) | P(HP potion) | P(MP potion) | HP / MP item |
|---|---|---|---|---|---|---|
| 1 (Ch1, after `q_secure_plains`) | 14 | 8 | 0.15 | 0.51 | 0.34 | `c_hp_potion_s` / `c_mp_potion_s` |
| 2 | 12 | 12 | 0.18 | 0.492 | 0.328 | `_m` / `_m` |
| 3 | 10 | 16 | 0.21 | 0.474 | 0.316 | `_m` / `_m` |
| 4 | 8 | 20 | 0.24 | 0.456 | 0.304 | `_l` / `_m` |
| 5 | 6 | 24 | 0.27 | 0.438 | 0.292 | `_l` / `_m` |

* Ch1 consequence: after the `q_secure_plains` turn-in every further kill (any monster, including 0-ember slimes) adds 1
  to `progress`; every 14th kill adds one item to `stock`, until 8 items are stocked; then `progress` freezes at whatever
  it was (0 right after the 8th yield) until a harvest frees space. Both values are saved and must round-trip exactly.
* RNG: the web uses `Math.random` (draws happen only on a yield kill, after the ember float and before the ley-fruit drop
  roll / quest drops / loot of the same kill). The core should draw from the gameplay RNG stream at that point of the
  kill hook; exact parity of the stream is not required (web `Math.random` is unseeded).
* Harvesting needs the tower (later milestone): `harvest()` empties `stock`; the caller creates each item via
  `createItem(id, heroLevel, 'normal')`, identified, quantity 1, into the bag; items that do not fit go back with
  `returnToGarden(id, left)` (`HomesteadTower.ts:108-116`, `EmberTower.ts:241-264`). In the Ch1 build there is no
  harvest, so the stock simply caps at 8.
* Load (`HomesteadTower.ts:229-236`): `progress = num(g.progress, 0)`; each stock entry kept only if `num(v) > 0`, where
  `num(v, fb) = finite number ? max(0, v) : fb` (`:253-255`). A loaded `progress ≥ interval` yields on the next kill.

#### 4.7.2 Ember Tower panel/garden UI — render-only, later milestone
The herb-garden plot label (`homestead.plot.harvest`, `EmberTower.ts:516-525`) and the panel exist only in the tower;
the Ch1 build shows nothing for the garden.

#### 4.7.3 Expedition kill counter (general; not reachable in Ch1)
`onKillExpedition()` (`HomesteadTower.ts:199-203`): if an expedition is out and not done, `kills += 1`; returns true
when this kill made it done (`kills >= killsRequired or remainingMs <= 0`) → `EmberTower` logs
`homestead.log.expeditionBack`. Expeditions need the `training_ground` wing (unlocked by Ch4 `q_seal_fire_rift`), so
`expedition` stays `null` throughout Ch1; the call must still exist in the kill hook for later milestones.

---

## 5. Guidance and quest UI

### 5.1 Guide target — `computeGuideTarget(quest, progress, world)` (`src/systems/QuestGuide.ts:106-124`, pure)
```
if status == completed: giver = questGiverOf(quest.id); tile = giver ? world.npcTile(giver) : null
                        return tile ? {tile, reason: turn_in, objectiveIndex: −1} : null    // giver not in this zone → none
if status != active: return null
for i, obj in objectives: if current_i >= required_i: continue
    tile = objectiveTarget(quest, obj, i, world); if tile: return {tile, reason: objective, objectiveIndex: i}
return null                                                       // falls through to later objectives when a target is unknown
```
`objectiveTarget` (`:55-99`), first match wins:
1. `escort`: escort out and `hypot(escort − hero) > 4` → escort tile (“fetch your charge”).
2. `investigate_clue`: the clue mark's (nudged) tile if a mark exists.
3. `obj.location` → its centre.
4. by type: `talk` → `npcTile(targetId)` (**returned even if null**, so a talk target outside the zone yields no target
   and the loop moves on); `defend_wave` → defendTarget tile; `escort` → escort start; `craft_craft` → `npcTile(craftNpc)`;
   `craft_deliver` → `npcTile(deliverNpc)`; gather collect → nearest ungathered node (squared distance, first wins ties)
   or the area centre.
5. monster ids (`objectiveMonsters`, `:49-53`: kill → `[targetId]`; drop collect → `source.monsters`; else none). None →
   questArea centre or null. Else nearest **living** monster with one of the ids → nearest map spawn point of those ids →
   (kill objective of a hunt) the hunt's tile → questArea centre.

`questGiverOf(id)` (`QuestWorld.ts:94-100`): the first NPC (in `NPCDefinitions` key order) whose `quests` contains id.
`npcTile(id)`: tile of that NPC **standing in the current zone**, else null.

### 5.2 Guide refresh and arrow (`QuestWorld.ts:370-396`)
* Every 250 ms (accumulator; forced to the next frame by any quest event that triggers `sync`): `guided =
  getGuidedQuest(zone)`, `guideTarget = guided ? computeGuideTarget(...) : null`.
* Arrow hidden when no target or `hypot(target − hero) < 2.5` tiles (`GUIDE_NEAR`).
* Render-only: 48 px gold arrow texture at scale 0.55, orbiting the hero at radius `44 + 3·sin(5·t)` px (t in s), the
  vertical offset squashed ×0.6 and centred 14 px above the feet, rotated to the screen-space direction to the target;
  alpha 0.95 & white tint for `turn_in`, 0.8 & `0xfff2d0` for objectives. **3D:** a flat arrow mesh/decal on the ground
  ring around the hero (radius ≈ 1 tile, pulsing ±0.07 tile at 5 rad/s), yaw = world direction hero→target; brighter
  for turn-in. The minimap star marker uses the same `guideTarget`.

### 5.3 Quest card (NPC offer / turn-in) — `UIScene.openQuestCard` (`UIScene.ts:3100-3343`), data `QuestCardUI.ts`
* Opened by interacting with a **quest-type** NPC whose `gatherNpcQuests(...)` is non-empty (`UIScene.ts:823-847`).
  One card per entry with ◀ n/N ▶ navigation (index resets the choice selection).
* Content per card (`buildQuestCardData`, `QuestCardUI.ts:136-171`): name (gold for main), badge `ui.questCard.mainBadge`
  /`sideBadge`, NPC name, type label `sys.quest.type.<type>`; **NPC speech** = `data.quest.<id>.offer` (accept card) or
  `.complete` (turn-in card), shown in quotes if the key exists; description (hidden on a turn-in card that has
  speech); objectives `○/✓ <typeLabel> <targetLabel>  cur/req` (type labels: quest types + `sys.quest.objType.<sub>`);
  reward summary `formatRewardSummary` (`:188-200`: `{exp} 经验`, `{gold} 金币`, `{n} 物品`, `{n} 选 1 装备`, `宠物`;
  embers are not listed); fixed item icons; on a turn-in card the generated choice items as selectable slots with
  tooltips and slot labels `sys.questCard.choice.<slot>`; on an accept card with choices the preview line
  `ui.questCard.choicePreview {slots}`.
* **Accept**: `acceptQuest(id)`; `setTracked(id)`; toast `sys.questCard.accepted {name}` (green ✦, 2 s); close.
* **Turn in**: `zone.turnInQuest(id, selectedChoice)`; on success toast `sys.questCard.turnedIn {name}` (gold ✓); close;
  then **chain offer**: if the same NPC now has entries, after 900 ms either reopen the card immediately or, if the
  StoryDirector is busy (a cutscene was queued by the turn-in), wait for `STORY_STATE {active:false}` + 300 ms; the card
  only reopens if no card/dialogue is open.
* Optional `ui.questCard.viewStory` button when the NPC has a dialogue tree → closes the card, opens the tree (§7.1).
* Backdrop tap closes. Audio: `click` on open.

### 5.4 Quest tracker HUD (`src/ui/QuestTrackerHUD.ts`, `UIScene.ts:5952-6060`)
* Refresh every 250 ms (signature-diffed) and immediately on any quest event.
* Entries = `getActiveQuests()` sorted by rank (guided quest 0, same zone 1, other 2), then **re-sorted** stably by
  `buildTrackerState`: main before side, incomplete before completed (`QuestTrackerHUD.ts:146-176`). Max visible 5
  (3 on touch), with `ui.questTracker.scrollIndicator {count}`.
* Title line `[➤ ]<[主线]|[支线]> <name>[ ✓]` (➤ = guided). Summary: completed → `sys.tracker.completed`
  (“已完成 - 返回NPC交付”); one objective → `<typeLabel> cur/req`; else `sys.tracker.doneCount {done,total}`.
  Expanded (tap the title; also pins the quest as tracked): one line per objective `<typeLabel> <target> cur/req|✓`.

### 5.5 Quest log panel (`UIScene.ts:3727-3990`, render contract)
Tabs: 进行中 (active+completed), 已完成 (turned_in), 传说 (lore, §10.4). Lists sorted main first then by `level`; 13 per
page. Detail: name, category, special-type label (escort/defend/investigate/craft), description, type summary (clues
found/total, wave cur/total, craft phase), objectives (`• <raw targetName> cur/req|✓` — raw zh, QUIRK Q11), rewards,
prereqs. Failed quests appear in no tab. No abandon button.

### 5.6 NPC quest markers (`src/ui/QuestNPCIndicators.ts:33-94`)
Only NPCs with a non-empty `quests` list. Priority: any of its quests `completed` → yellow `?` (`#f1c40f`); else any
available (no record, `level <= heroLevel+5`, prereqs turned_in; or failed+reacceptable) → yellow `!`; else any `active`
→ grey `?` (`#888888`); else hidden. Refreshed every 500 ms and on accept/turn-in/complete
(`ZoneScene.ts:675-687, 1295-1303, 1552-1556`). Render-only: 20 px glyph 80 px above the feet, bobbing 4 px (600 ms
yoyo). **3D:** billboard glyph ≈ 0.5 m above the head, same bob.

### 5.7 Minimap quest layer (`UIScene.ts:2940-3035`, render contract)
Quest-giver dots at the **camp centre** (camp NPCs) or field tile: turn-in larger gold `0xffd23a`, available pale
`0xf5e6a8`; guide star (gold turn-in / orange objective); for each active quest in this zone: questArea filled circle
(main gold / side grey, α 0.25), unfinished explore markers (orange squares), clue markers (purple), escort destination
(orange), defend target (red ring), escort start (orange dot).

### 5.8 Feedback (render-only contracts)
* **Progress popup** (`ZoneScene.ts:1309-1334`): above the hero; kill objectives with `required > 3` only show when
  done or when `current % ceil(required/4) == 0` (10 → 3,6,9; 15 → 4,8,12). Text `✓ <target>` when done else
  `[+1 ]<target>  cur/req` (no “+1” for kills). Up to several stacked 16 px apart if within 900 ms; pop 160 ms, hold
  800 ms (1300 ms when done), rise 26 px and fade 700 ms. **3D:** world-anchored UMG text above the hero.
* **Quest complete banner** (`:1295-1303, 5913-5940`): `zone.questComplete` + localized quest name + `zone.quest.returnTo
  {npc}` (giver name), fade-in 500 ms, hold 3000 ms, fade 600 ms, at 22 % screen height.
* **Audio** (`src/systems/audio/AudioManager.ts:352-368`): `QUEST_ACCEPTED` → `npc_interact`; `QUEST_COMPLETED` and
  `QUEST_TURNED_IN` → `quest_complete`; `QUEST_PROGRESS` (not completing the quest): objective done → `quest_objective`,
  else if targetId starts with `mat_` or `clue_` → `quest_progress`; `NPC_INTERACT` → `npc_interact`.

---

## 6. NPCs

### 6.1 Types
| type | Interaction (`ZoneScene.interactNPC`, `:4136-4177`) |
|---|---|
| `blacksmith` | `SHOP_OPEN {npcId, shopItems, type}` (shop + forge tab; loot spec §12–13) |
| `merchant` | `SHOP_OPEN` (shop) |
| `quest` | `NPC_INTERACT {npcId, npcName (localized), dialogue: dialogue[1] ?? dialogue[0], dialogueTree, completedQuests: turned_in ids, ...}` → quest card / tree / linear panel (§5.3, §7) |
| `stash` | `UI_TOGGLE_PANEL {panel:'stash', npcId}` (no stash NPC in Ch1) |

Every interaction first: Ember Tower hook (later), log `dialogue[0]` (raw zh, QUIRK Q11) as `info`,
`updateProgress('talk', npcId)`, craft phase hook — then the type switch.

### 6.2 Placement (`ZoneScene.ts:4467-4484, 4795-4803`)
Camp NPC `i` stands at `camp + offset[i mod 6]` with offsets `(−3,−2), (3,−2), (−3,2), (3,2), (0,−3), (0,3)`. Field NPCs
stand at their tile. Safe zone around each camp: radius `mapData.safeZoneRadius ?? 9` (monsters spec §6.2).

### 6.3 Interaction trigger (`ZoneScene.ts:774-792, 5661-5672`)
A click/tap resolves the tile under the pointer; the nearest NPC with `distSq(npcTile, clickTile) < 3.24` (1.8 tiles)
is selected; it is used only if `hypot(npc − hero) <= 3` tiles. Otherwise the click falls through (move/attack). No
auto-walk-to-NPC, no keyboard interact key (port: open question OQ6 for a touch “Talk” button).

### 6.4 NPC presentation state machine (render-only, `src/entities/NPC.ts:236-339`)
States `working | alert | idle | talking`. `near = hypot(npc − hero) <= 3`. working/idle → alert when near; alert →
working 500 ms after the hero leaves (cancelled if they return); talking on `NPC_INTERACT`/`SHOP_OPEN` for this NPC (and
stash toggle); `DIALOGUE_CLOSE` / `SHOP_CLOSE` → alert. While near, mirror to face the hero when `(heroCol − heroRow) −
(npcCol − npcRow) <= −1` (face left) or `>= 1` (face right). **3D:** play idle-work / alert / talk montages and turn
(yaw-interpolate) toward the hero within 3 tiles.

### 6.5 Chapter 1 NPC table (`src/data/npcs.ts`; positions from `src/data/maps/emerald_plains.ts:32-46`)

| id | zh / en name | type | tile | quests (card order) | shopItems | dialogue lines (zh; en keys `data.npc.<id>.dialogue.<n>`) |
|---|---|---|---|---|---|---|
| `blacksmith` | 铁匠 / Blacksmith | blacksmith | (12,13) | — | `w_short_sword, w_broad_sword, w_dagger, w_stiletto, w_short_bow, w_oak_staff, w_wooden_shield, a_leather_helm, a_leather_armor, a_leather_gloves, a_leather_boots, a_leather_belt` | 欢迎来到我的铁匠铺! / 需要修理装备或者打造新武器吗？ |
| `merchant` | 商人 / Merchant | merchant | (18,13) and (92,98) | — | `c_hp_potion_s, c_hp_potion_m, c_mp_potion_s, c_mp_potion_m, c_antidote, c_tp_scroll, c_id_scroll, g_ruby_1, g_sapphire_1, g_emerald_1, g_topaz_1` | 需要补给吗？我这里应有尽有! / 药水、卷轴、宝石，随你挑选。 |
| `quest_elder` | 村长 / Village Elder | quest | (12,17) | `q_kill_slimes, q_collect_slime_gel, q_herb_gathering, q_kill_goblins, q_explore_goblin_camp, q_lost_pendant, q_find_goblin_chief, q_secure_plains, q_escort_merchant_plains, q_pet_sprite_friend` | — | 勇士，翡翠平原上的怪物越来越多了... / 请帮助我们清除这些威胁! — has `dialogueTree` (§10.3) |
| `plains_herbalist` | 平原药师 / Plains Herbalist | merchant | (50,30) | — (delivery target of `q_collect_slime_gel`) | `c_hp_potion_s, c_hp_potion_m, c_mp_potion_s, c_antidote` | 你好，旅行者！我在这片平原上采集草药已有二十年了。 / 翡翠平原的灵脉之力滋养着这里的一切，连草药都比别处更有药效。 / 需要补给吗？我的药水都是用灵脉草药亲手调配的。 |
| `plains_wanderer` | 流浪剑客 / Wandering Swordsman | quest | (70,65) | `q_bandit_trouble` | — | 我曾是王国的骑士，如今在这平原上漫无目的地游荡。 / 你知道吗？这片看似平和的草原下面，埋藏着精灵族的古老遗迹。 / 我在东边的小丘附近发现了一处隐蔽的入口，但那里面太危险了，我一个人不敢进去。 (no tree → with nothing to offer, the linear panel shows line [1]) |
| escort 旅行商人 / Traveling Merchant | not an NPC def | — | spawns at (30,40) | — | — | name from `escortNpc.name`; unused key `data.escortNpc.escort_merchant_plains` (QUIRK Q11) |

Spawn tiles verified walkable on the generated map. Art (Blender): elder, blacksmith, merchant, herbalist, wanderer,
traveling merchant, plus portrait renders for every cutscene speaker (§8.6).

---

## 7. Dialogue

### 7.1 Branching tree runtime (`UIScene.ts:3398-3725`)
Entered when a quest NPC has nothing for the quest card, or via the card's “查看故事” button. Per-NPC state
`{visitedNodes: string[], choicesMade: map<nodeId, nextNodeId>}` is saved (`SaveData.dialogueState`) but **never read**
by any logic (record only).

Render node `N`:
1. Mark `N.id` visited.
2. **Visible choices** (in order): drop a choice whose `prereqQuests` are not all in `completedQuests` (= quests
   `turned_in` at the time the NPC was clicked); drop a choice with `questTrigger` whose quest is `active` or
   `turned_in` **and** whose target node `isEnd`.
3. Buttons, in order: each visible choice (label suffixed `ui.dialogue.inProgress` “（进行中）” and styled ghost if its
   quest is active/turned_in); **Continue** (`ui.dialogue.continue`) if `N.nextNodeId` and not `isEnd` and no visible
   choices and not back-to-root; **Back** (`ui.dialogue.back` → root) if `N` had choices, all were filtered, `N` is not
   an end node and not the root; **Leave** (`ui.dialogue.leave`) if `N.isEnd` or (no visible choices and no
   `nextNodeId` and no Back).
4. On choice: `choicesMade[N.id] = next`; if `questTrigger` and (no record or `failed`) → `acceptQuest` (may silently
   fail on prereqs/reacceptable; the dialogue still advances); apply `reward` **every time** (gold → log
   `ui.dialogue.gotGold`; exp → `addExp` + log `ui.dialogue.gotExp`; items → `createItem(id, heroLevel, normal)` into the
   bag, overflow lost) (QUIRK Q2); navigate to `nodes[next]` or close if missing.
5. Panel: NPC name title, subtitle `ui.dialogue.subtitle`, text (scrolls if > available height; wheel ±30 px, touch
   drag), backdrop tap closes, `DIALOGUE_CLOSE` emitted on close. Dialogue-tree acceptances do **not** pin the guide.
* `ui.dialogue.turnInPrefix` is dead code (`turnedIn` is always empty).

### 7.2 Linear dialogue
Quest NPC with no actionable quests and no tree (`plains_wanderer`): panel with `npcName` title, text = `dialogue[1]`
(fallback `[0]`), no action buttons, close hint `ui.dialogue.closeHint`; backdrop/X closes.

### 7.3 Mini-boss pre-fight dialogue
Linear tree shown once per save when the hero comes within the boss's `aggroRange`; only the boss is frozen. Full rules
and the 哥布林萨满 lines: monsters spec §8.3 (`SaveData.miniBossDialogueSeen`).

---

## 8. Story

### 8.1 StoryProgress (`src/systems/StoryProgress.ts`)
A set of seen beat ids in the session; saved as `SaveData.storySeen` (array). On load: `story.load(save.storySeen ??
['prologue'])` (`ZoneScene.ts:4348`) — old saves skip the prologue. A new game starts empty.
Beat ids: `prologue`, `chapter_<zoneId>`, each cutscene id (`cs_*`) when played via a trigger, `boss_<monsterId>` for
boss intros (the intro's cutscene id itself is **not** marked), `epilogue` (credits are part of it).

### 8.2 StoryDirector (`src/systems/StoryDirector.ts`) — created per zone (not in the labyrinth)
State: `cinematic: bool`, `queue: [{id, run}]`, `running: bool`, `scanTimer`, `bossBarFor: monsterId|null`, `named`
(weak set of renamed boss instances). Subscribes to `QUEST_TURNED_IN` and `QUEST_ACCEPTED`. `busy = running ||
queue non-empty`.

**`start()` on zone entry** (`:81-101`, called at the end of `ZoneScene.create`, after `ZONE_ENTERED`):
```
if !seen('prologue'): enqueue('prologue', sequence(PROLOGUE, music 'abyss_rift'))
ch = CHAPTERS.find(zoneId == map)
if ch and !seen('chapter_<map>'): enqueue('chapter_<map>', chapterCard(ch)); fire('zone_entered', map); return true
fire('zone_entered', map); return false            // true → the scene skips its plain zone banner
```
**`fire(on, key)`** (`:114-122`): for every trigger in `STORY_TRIGGERS` order with matching `on` and key →
`enqueueCutscene(trigger.cutscene, delay, trigger.grantPet)` with delay **650 ms** for quest triggers, **900 ms** for
`zone_entered`, **0** for `monster_killed`.

**`onMonsterKilled(defId)`** (`:103-112`): `fire('monster_killed', defId)`; if `defId == bossBarFor` → clear the bar;
if `defId == 'demon_lord'` and `!seen('epilogue')` → enqueue `epilogue` (epilogue sequence then credits, music
`abyss_rift`/`victory`).

**Queue** (`:126-165`):
```
enqueue(id, run): if seen(id) or queue has id: return; queue.push; pump()
enqueueCutscene(id, delay, pet): if CUTSCENES[id] missing: return
    enqueue(id, async: wait delay ms (game clock); try play cutscene(id) finally if pet: grantPet(pet))
pump(): if running: return; running = true; emit STORY_STATE {active:true}
        while queue: next = shift(); seen.add(next.id)        // marked BEFORE it plays (a crash/quit mid-beat skips it)
                     try await next.run() catch: log error, continue      // a broken beat never soft-locks
        running = false; save(); emit STORY_STATE {active:false}
```
Beats play strictly one at a time in enqueue order. During a trigger's delay the world is **not** frozen (cinematic is
only set when the beat's presentation starts).

**Presentation wrappers**: `sequence` → `cinematic = true`, play music track, play, restore the zone's `explore` track,
`cinematic = false`. `chapterCard` → cinematic around `playChapter`. `cutscene(id)` (`:200-214`) → cinematic, play
steps with hooks, then pan the camera back to the hero over 450 ms (Sine.easeInOut), resume following (lerp 0.08),
`cinematic = false`.

### 8.3 Boss intros and boss bar (`:286-323`) — summary (monster side: monsters spec §10)
Every 250 ms (accumulator; not while cinematic): nearest living instance per `BOSS_INTROS` entry; the closest one within
14 tiles gets its nameplate renamed to `t(intro.name)` in `#ffcf6a` (once per instance); within **9** tiles and
`!seen('boss_<id>')` → enqueue `boss_<id>` (no delay) playing `intro.cutscene`; within 14 tiles → `BOSS_BAR {name,
epithet, hp()}` (emitted when the boss changes); otherwise clear. Ch1: `goblin_chief` (spawn (15,95), aggro 6) — the intro
can play before `q_find_goblin_chief` is accepted.

### 8.4 Cinematic freeze — exact web semantics
While `cinematic` (`ZoneScene.ts:1347-1351, 710-716, 774-776, 2944-2946`, `UIScene.ts:5763-5764`):
* On entering: hero path and attack target cleared.
* `ZoneScene.update` returns at the top: **no** input, movement, hold-move, skills, monster AI, combat, regen, status
  ticks, auto-combat, escort/defend, pet, quest observers (explore, gather, clues, guide), lore/exit/hidden-area/
  decoration checks, StoryDirector boss scan, Ember Tower ticks.
* Pending monster strikes abort at their contact beat; pointer input on the world is ignored; the HUD camera is hidden.
* The game clock, tweens and scheduled callbacks keep running (cooldowns and buff durations keep elapsing — QUIRK Q10).
Port: the core exposes `IsCinematic()`; the sim step skips everything above while true.

### 8.5 StoryScene presentation (`src/scenes/StoryScene.ts`) — render-only, exact timings
Reference frame 1280×720 logical; all timings in ms. Input: tap/click, Space, Enter = **advance**; Esc or the skip button
(`story.ui.skip` “Esc 跳过” / touch `story.ui.skipTouch` “跳过 ▸▸”, top-right, 28 px font + large padding on touch) =
**skip the whole beat** (all remaining waits resolve instantly, tweens jump to end). Skip resets at the start of each
beat. **3D:** a dedicated UMG overlay (`WBP_Story`) above the HUD at 1280×720 reference scale; camera hooks drive the
gameplay camera.

**Sequence (prologue/epilogue)** (`:174-253`): black full-screen backdrop instantly; mood background (vertical gradient
`top→bottom`, radial glow at 50%/83% height, vignette; `MOODS` table `:35-43`) fades in 900; rising ember particles
tinted per mood. Per slide: if the mood changes → bg fade out 400, swap, fade in 500. Slide block (heading 18 px serif
gold `#c9a45a` letter-spacing 6; title 44 px bold `#f3d9a0`; each text line 22 px `#eadcc0`, wrap 900) is vertically
centred; parts fade in one after another 900 each (an input shows all at once); then a blinking ▼ and wait for input;
fade out 450. End: bg fade out 700. Skip button visible throughout.

| mood | top | bottom | glow | ember tint |
|---|---|---|---|---|
| embers | `#0c0605` | `#2a0f06` | rgba(255,120,40,.35) | `0xff9a3c` |
| night | `#03050c` | `#101a33` | rgba(120,150,255,.22) | `0x9fb8ff` |
| abyss | `#07030a` | `#260a2c` | rgba(200,40,120,.32) | `0xd24cff` |
| dawn | `#15142a` | `#5a3a30` | rgba(255,190,120,.35) | `0xffd08a` |
| forge | `#0e0906` | `#40200c` | rgba(255,140,40,.4) | `0xffb040` |
| sand | `#1c1208` | `#5a3c1a` | rgba(255,200,120,.3) | `0xffd79a` |
| light | `#1d2636` | `#8a7550` | rgba(255,236,180,.45) | `0xfff1c8` |

**Credits** (`:255-276`): one column scrolling up at 42 px/s from below the screen; heading 16 px, title 34 px, text
20 px, +90 px between slides; a tap speeds the roll ×4; Esc ends it.

**Chapter card** (`:280-319`): (no skip button, Esc still works) black shade → α 0.72 in 600; mood glow → 0.55 (900,
parallel); number (20 px, letter-spacing 10) fade 500; title (56 px bold `#f6e0a8`, scale 1.08→1) 900 Cubic.easeOut
with an additive halo (1200, parallel); two gold rules grow from the centre (700, parallel); subtitle 600; body (19 px,
wrap 820) 900; then **hold 3800 or until input**; everything fades 900; shade fades 700. Uncut total ≈ 8.9 s.

**Cutscene** (`:323-343`): skip button on; letterbox bars (black, 78 px each = 10.83 % of height) slide in over 450
(Cubic.easeOut); steps in order (break on skip); bars slide out 450.

| Step | Behaviour (exact) | 3D equivalent |
|---|---|---|
| `narrate` | full-screen shade → α 0.55 (400); centred text 24 px serif `#efe2c4`, wrap 860 fades in 700; wait input; text out 350; shade out 300 | UMG |
| `say` | box 900×150 at `y = 720 − 78 − 150 − 10`, fill `0x100b07` α .93, gold border (villain: `0x12061a`, violet `0x9b4dff`); round portrait medallion r 56 at `(bx+78, centre)` (head-and-shoulders crop of the speaker's art; fallback emblem); name 20 px bold at `(bx+156, by+20)` (`#f0cf86`, villain `#d9a6ff`); body 20 px **typewriter 38 chars/s** (input completes the line); blinking ▼; wait input; fade 160 | UMG + portrait textures (§8.6) |
| `whisper` | violet radial vignette fades in 600; text 28 px `#f0dcff` fades 700, jittering ±1.2 px every 60 ms; two additive chromatic ghosts (`#ff3a8c` at −3 px, `#4a6bff` at +3 px, random extra 0–3 px) to α .45 (400); wait input; fade 600 | UMG material (chromatic offset) |
| `title` | black band 150 px → α .6 (200); name 54 px bold `#ffe2a8` scale 1.25→1 (380, Back.easeOut) with orange halo `0xff5a1a`; slash bar 520×3 `0xff8a3c` grows (380); epithet 22 px fades 400; **hold 2200 or input**; fade 450 | UMG |
| `focus` | camera stops following and pans to the target over `ms` (default 900, Sine.easeInOut); resolves at arrival; missing target → resolves at once. NPC/monster targets aim 30 px above the feet (≈ 0.67 tile → use the actor's chest/head height) | move the camera rig's look-at pivot with the same easing/time; keep the fixed oblique angle |
| `shake` | camera shake `ms`, intensity (fraction of screen) default 0.01 | Camera shake asset scaled by intensity/0.01 |
| `flash` | camera flash `color` over `ms` (default white, 300) | full-screen UMG colour fade 1→0 |
| `wait` | sleep `ms` (skippable) | timer |

Speaker resolution (`StoryDirector.ts:260-282`): `hero` → name `story.speaker.hero` (“你”/“You”), portrait = hero class
art; `villain` → `story.speaker.villain` (“伊格纳罗斯”/“Ignaroth”), always the emblem (violet eye); `{npc}` →
`data.npc.<id>.name`, portrait from the NPC art (its `spriteId` if set); `{monster}` → the boss intro's name if one
exists (`story.boss.<id>.name`) else the monster name; portrait from the nearest living instance's art.

### 8.6 3D presentation assets needed for Ch1 (Blender pipeline)
Portrait renders (head-and-shoulders, transparent, ≥ 256 px) for: `quest_elder`, each hero class (warrior/mage/rogue),
`goblin_chief`; emblems for villain (violet eye) and generic sigil; mood backdrops (7, or a parametric material); ember
particle system; the goblin camp (40,52) must read well under the `cs_ep_whisper` 1400 ms focus.

---

## 9. Achievements (`src/systems/AchievementSystem.ts`)

State: `progress: map<key, int>` with key = `type` or `type:targetId`; `unlocked: set<id>`.
```
update(type, targetId?, amount = 1):
    keys = { key(a) for a in ACH if a not unlocked and a.type == type and (!a.targetId or a.targetId == targetId) }
    for k in keys: progress[k] += amount                       // each distinct key once per call
    for a in ACH (same filter): if progress[key(a)] >= a.required: unlock(a)
checkLevel(level): unlock every 'level' achievement with level >= required
unlock(a): unlocked.add; emit ACHIEVEMENT_UNLOCKED {achievement}; log sys.achievement.unlocked{name} | unlockedWithTitle{name,title}
getBonuses(): sum reward.value per reward.stat over unlocked
```
Callers: each kill calls `update('kill', undefined, 1)` **and** `update('kill', defId, 1)` (`ZoneScene.ts:3841-3843`) →
the generic `kill` counter rises by **2 per kill** (QUIRK Q3); `checkLevel` on each kill and on `PLAYER_LEVEL_UP`;
`update('explore', mapId)` on **every** zone-scene creation (zone change, load, death respawn, sub-dungeon) → the
`explore` counter counts scene creations, not distinct zones (QUIRK Q4); `update('quest')` on each turn-in;
`update('collect')` when a **legendary** item is picked up (click or auto-loot).
Bonuses are added into `EquipStats` (`damage`, `lck`, `str`) in `getEquipStats` (`ZoneScene.ts:3143-3152`) — the cache is
not invalidated on unlock, so they apply at the next equipment/pet/zone change (QUIRK Q13). Titles are display-only.
Save: `getUnlockedData()` = progress map merged with `{<achId>: 1}` for unlocked; load splits keys by whether they are
an achievement id. Toast: `ui.achievement.toastUnlock {name}` + description + reward (`<stat label>+v`) + title.

Full table (`:5-18`):

| id | zh name | type | target | required | reward | title | Ch1? |
|---|---|---|---|---|---|---|---|
| `ach_first_kill` | 初出茅庐 | kill | — | 1 | — | 新手冒险者 | yes (first kill) |
| `ach_kill_100` | 百人斩 | kill | — | 100 | damage +2 | — | yes (50 actual kills, Q3) |
| `ach_kill_500` | 屠戮者 | kill | — | 500 | damage +5 | 屠戮者 | yes with farming (250 kills) |
| `ach_kill_slime` | 史莱姆克星 | kill | slime_green | 50 | lck +2 | — | yes |
| `ach_kill_goblin_chief` | 哥布林终结者 | kill | goblin_chief | 1 | — | 哥布林终结者 | yes |
| `ach_kill_demon_lord` | 恶魔猎人 | kill | demon_lord | 1 | str +10 | 深渊征服者 | no |
| `ach_level_10` | 成长之路 | level | — | 10 | — | — | yes |
| `ach_level_25` | 勇者 | level | — | 25 | — | 勇者 | no (normally) |
| `ach_level_50` | 传说 | level | — | 50 | str +5 | 传说 | no |
| `ach_explore_all` | 探索者 | explore | — | 5 | lck +5 | 大陆探索者 | yes via Q4 (5 scene loads) |
| `ach_quest_10` | 任务达人 | quest | — | 10 | lck +3 | — | yes (11 Ch1 quests) |
| `ach_collect_legendary` | 传奇收藏家 | collect | — | 1 | — | 传奇收藏家 | possible (legendary drops exist) |

---

## 10. Chapter 1 content

### 10.1 Quests (`src/data/quests/all_quests.ts:9-154, 803-818, 876-891`)
Availability = prereqs turned in **and** `heroLevel >= level − 5` (turn-in never gated). All givers stand in
`emerald_plains`. “Embers” = §4.4 value. Choice level `L = max(level, min(heroLevel, level+5))`.

**Main line** (chain; all given by `quest_elder`):

| # | id — zh / en (`data.quest.<id>.name`) | lvl | prereq | Objectives (type · targetId · label · req · where) | Rewards | Turn-in beat |
|---|---|---|---|---|---|---|
| M1 | `q_kill_slimes` 史莱姆之灾 / Slime Plague | 1 | — | kill · `slime_green` · 绿色史莱姆 · 10 | 120 exp, 25 g, embers 2 | `cs_ep_mark` |
| M2 | `q_kill_goblins` 火光之眼 / Eyes of Flame | 3 | M1 | kill · `goblin` · 哥布林 · 15 | 200 exp, 40 g, embers 2 | — |
| M3 | `q_explore_goblin_camp` 篝火营地 / The Bonfire Camp | 5 | M2 | explore · `zone_goblin_camp` · 哥布林营地 · 1 · circle (40,52) r 8 | 250 exp, 35 g, embers 2; **Ember Tower unlock** | `cs_ep_whisper` |
| M4 | `q_find_goblin_chief` 碎牙之王 / King Brokentooth | 7 | M3 | kill · `goblin_chief` · 哥布林首领 · 1 (spawn (15,95); boss intro on sight) | 500 exp, 80 g, embers 2, **choice weapon \| armor**, rare, L∈[7,12] | — |
| M5 | `q_secure_plains` 灵脉之印 / The Seal of Veins | 8 | M4 | explore · `zone_east_plains` · 平原东部 · 1 · (65,30) r 10; explore · `zone_south_plains` · 平原南部 · 1 · (50,55) r 10 (any order) | 400 exp, 60 g, **embers 5**, **choice jewelry \| boots \| gloves**, rare, L∈[8,13]; herb-garden wing unlock | `cs_ep_finale` |

Quest areas (minimap/guide fallback): M1 (32,12) r12; M2 (34,40) r15; M4 (25,65) r10 (does not cover the chief's spawn —
QUIRK Q7); M3/M5 none.

**Side quests:**

| id — zh / en | giver | lvl | prereq | Objectives | Rewards | Notes |
|---|---|---|---|---|---|---|
| `q_collect_slime_gel` 药师的订单 / The Herbalist's Order | quest_elder | 2 | — | (0) collect · `mat_slime_gel` · 史莱姆凝胶 · 6 · drop from `slime_green` @ 0.5 · itemKind `gel`; (1) talk · `plains_herbalist` · 把凝胶送给平原药师 · 1 (only after 0) | 110 exp, 35 g, items `c_hp_potion_s` ×2, embers 1 | type `talk`; questArea (25,20) r10; turn in at the **elder** |
| `q_herb_gathering` 急救草药 / Emergency Herbs | quest_elder | 2 | — | collect · `mat_herb` · 翡翠草药 · 5 · gather area (22,26) r 9, 7 nodes · itemKind `herb` | 70 exp, 20 g, item `c_hp_potion_s`, embers 1 | questArea (20,25) r10. Nodes on the generated map: (23,25) (28,23) (18,32) (23,34) (27,29) (23,18) (28,31) |
| `q_lost_pendant` 偷挂坠的贼 / The Pendant Thief | quest_elder | 4 | M1 | (0–2) investigate_clue · `clue_pendant_fence` 被扒开的篱笆 (30,24) · `clue_pendant_cloth` 挂在荆棘上的破布 (42,34) · `clue_pendant_ashes` 还温热的篝火 (54,44); (3) kill · `hunt_pendant_thief` · 小贼斯尼克 · 1; (4) collect · `mat_pendant` · 传家挂坠 · 1 · drop from `hunt_pendant_thief` @ 1.0 · itemKind `pendant` | 180 exp, 40 g, **choice jewelry** (magic, L∈[4,9]), embers 1 | type `investigate`; questArea (54,44) r10; hunt at (60,48) goblin ×3 HP ×1.2 dmg, `revealAfterPrevious`, 2 goblin minions — appears (announced) after the 3rd clue; the kill and the pendant drop complete it in one blow |
| `q_bandit_trouble` 悬赏：红帽格鲁克 / Bounty: Gruk Redcap | **plains_wanderer** | 7 | M2 | kill · `hunt_redcap_gruk` · 红帽格鲁克 · 1 | 260 exp, 60 g, **choice weapon** (magic, L∈[7,12]), embers 1 | questArea (80,75) r8; hunt (82,72) goblin ×5 HP ×1.6 dmg, 4 goblin minions, spawns on accept (silent) |
| `q_pet_sprite_friend` 捉迷藏的小精灵 / Hide-and-Seek Sprite | quest_elder | 3 | — | investigate_clue ×4: `clue_sprite_1` 蘑菇圈里的笑声 (78,12), `clue_sprite_2` 会发光的花 (92,10), `clue_sprite_3` 树洞里的铃声 (100,22), `clue_sprite_4` 池边的小脚印 (86,28) | 220 exp, 50 g, **pet `pet_sprite`**, embers 2 | questArea (88,18) r14 |
| `q_escort_merchant_plains` 护送商人 / Escort the Merchant | quest_elder | 5 | M2 | escort · `escort_merchant` · 护送商人到南方营地 · 1 · destination (50,90) | 350 exp, 60 g, item `c_hp_potion_m`, embers 1 | `reacceptable`; escortNpc 旅行商人 `npc_escort_traveling_merchant` start (30,40) → dest (50,90); HP 200; questArea (50,90) r8 |

All clue tiles above are walkable as given (`nearestWalkable` returns them unchanged). Clue notes
(`data.questClue.<id>`, zh): fence “篱笆被扒开个小洞，泥里一串小脚印往东去了。”, cloth “荆棘上的绿布一股哥布林臭味，痕迹拐向东南。”, ashes
“篝火还温着，骨头刚啃完——那小贼就在附近！”, sprite_1 “蘑菇圈里咯咯笑：“太慢啦，往东边找！””, sprite_2 “花瓣一闪，光点溜走了：“我躲进树林啦！””,
sprite_3 “树洞里叮铃一响：“最后一次，来池塘边！””, sprite_4 “小脚印在池边停住，一团光扑进你怀里。”

Offer / complete lines: `data.quest.<id>.offer` / `.complete` in `src/i18n/locales/questStory.ts:12-36` (zh) and
`:160-184` (en) for all 11 quests. Totals: exp 2660 (main 1470 + side 1190), gold 505, quest embers 20 (kills add 5 per
elite kill on top, §4.4.3).

**First elder card at a new game** (L1): `q_kill_slimes`, `q_collect_slime_gel`, `q_herb_gathering`,
`q_pet_sprite_friend` (all four available; card order = list order).

**Guide behaviour per quest** (from §5.1): M1/M2/gel(0) → nearest living slime/goblin, else their spawn points; M3 →
(40,52); M4 → nearest living chief, else spawn (15,95); M5 → (65,30) then (50,55); gel(1) → herbalist (50,30);
herbs → nearest node; pendant → clue marks in order, then the thief (or its tile (60,48)); bounty → Gruk; sprite → clue
marks in order; escort → the merchant while > 4 tiles away, else (50,90). Completed → elder (12,17) or wanderer (70,65).

### 10.2 Story beats (Chapter 1) — text keys and text
Order on a new game: **prologue → chapter card** (zone banner suppressed), then by quest turn-ins. Triggers: M1 →
`cs_ep_mark`, M3 → `cs_ep_whisper`, M5 → `cs_ep_finale`, each 650 ms after the turn-in; `goblin_chief` within 9 tiles →
`cs_boss_goblin_chief` (no delay). No Ch1 `quest_accepted` / `monster_killed` / `zone_entered` triggers.

**Prologue** (`PROLOGUE`, `script.ts:11-21`; keys `story.prologue.<n>.heading/.title/.text`; music `abyss_rift`):

| n | mood | heading / title (zh · en) | text (zh) |
|---|---|---|---|
| 1 | embers | 太初 · In the Beginning / 渊火 · The Abyssfire | 太初之时，虚空无光。\n深渊之底燃起一簇火焰，世界自火中锻成——\n山岳是它冷却的铁，江河是它未干的淬水。\n后人称之为：渊火。 |
| 2 | night | 灵脉纪元 · The Age of Veins / 六贤守火 · Six Keepers of the Flame | 渊火的余温沿着灵脉流遍大地，万物由此而生。\n六位贤者立誓守护炉火：精灵贤者、月之祭司、矮人王、日冕女王、守望者——\n以及六人中最耀眼的一位，守火人伊格纳罗斯。 |
| 3 | abyss | 千年之前 · A Thousand Years Ago / 焚誓 · The Burning of the Oath | 千年前，灵脉渐冷。伊格纳罗斯不忍看世界熄灭，\n他焚毁誓言，打开深渊，要将这残缺的世界投回炉中重铸。\n火海漫过大地，精灵高塔崩塌——史称大灾变。 |
| 4 | abyss | 五印 · The Five Seals / 以血为锁 · Blood for a Lock | 五位贤者以血为锁、以魂为钥，将他缚于深渊之底，立下五道封印。\n他们凿去了他的名字，从此世人只知有五贤者。\n而守望者艾瑟琳，从渊火中取走了最纯净的一缕心焰，藏于人间。 |
| 5 | embers | 低语 · The Whispers / 渊火复燃 · The Fire Stirs | 千年来，他在深渊中低语。\n大祭司、巫师、萨满……贪求永恒者一一应声，封印接连碎裂。\n如今，渊火再度燃起。 |
| 6 | dawn | 翡翠平原 · The Emerald Plains / 烙印 · The Brand | 那一夜，平原上的精灵塔废墟中火柱冲天。\n村民在灰烬里找到了你——毫发无伤，\n掌心却烙着一道印记：五焰环绕，中央一空。 |

English texts: `src/i18n/locales/story.ts:299-316`.

**Chapter card** (`CHAPTERS[0]`, mood `dawn`): `story.chapter.emerald_plains.number` 第一章 / Chapter One; `.title` 灰烬中醒来 /
Wake from the Ashes; `.subtitle` 翡翠平原 / The Emerald Plains; `.text` 精灵的灵脉在草原下低鸣，哥布林的篝火彻夜不熄。\n你带着一道不明的烙印醒来，而大地正在颤抖。 /
“Elven ley lines hum beneath the grass, and goblin fires burn all night.\nYou wake with a brand you do not understand,
and the earth is trembling.”

**`cs_ep_mark`** (after M1; `script.ts:35-49`):

| n | step | key → zh |
|---|---|---|
| 1 | focus `{npc: quest_elder}` 900 | — |
| 2 | say elder | `story.cs_ep_mark.2` 别动，孩子。把手伸过来……让我看看你掌心那道烙印。 |
| 3 | focus player 900 | — |
| 4 | narrate | `.4` 烙印在暮色中泛起微光：五道火焰环成一圈，中央空无一物。 |
| 5 | say elder | `.5` 五焰环一空……我家世代为精灵塔守碑，碑上刻的正是这个印记。 |
| 6 | say elder | `.6` 碑文写道：“渊火再燃之夜，灰烬中将有人醒来。其身负五焰，其心藏一空。” |
| 7 | say elder | `.7` “他将行过五地，重燃五印。开门者是他，闭门者亦是他。” |
| 8 | say hero | `.8` ……开门，还是闭门？ |
| 9 | say elder | `.9` 我不知道，孩子。我只知道那夜的火烧光了半片草场，却唯独没伤到你。 |
| 10 | say elder | `.10` 平原上这些怪事，恐怕都和你手上这团火有关。去吧，替我看着点。 |

**`cs_ep_whisper`** (after M3; `:51-66`):

| n | step | key → zh |
|---|---|---|
| 1 | focus (40,52) **1400** | — |
| 2 | narrate | `story.cs_ep_whisper.2` 当夜，你梦回哥布林营地。篝火早已熄灭，灰烬里却仍有火在低语。 |
| 3 | flash `0x7a3cff` 400 | — |
| 4 | whisper | `.4` 你醒了。我在火里看了你很久。 |
| 5 | whisper | `.5` 别怕。那烙印是一件礼物——只不过，是从我这里偷走的。 |
| 6 | whisper | `.6` 哥布林替我叩门，而你，替我点灯。走吧，一直走下去。 |
| 7 | focus player 900 | — |
| 8 | say hero | `.8` 你是谁？ |
| 9 | whisper | `.9` 一个被抹去名字的人。等你走到门前，自会想起来。 |
| 10 | shake 0.006 / 400 | — |
| 11 | narrate | `.11` 你猛然惊醒。掌心的烙印烫得像一块刚出炉的炭。 |

**`cs_ep_finale`** (after M5; `:68-83`):

| n | step | key → zh |
|---|---|---|
| 1 | focus player 900 | — |
| 2 | narrate | `story.cs_ep_finale.2` 你踏过平原的最后一道山丘，地底那持续了数日的颤动，忽然止息。 |
| 3 | flash `0x9dffb0` 500 | — |
| 4 | narrate | `.4` 烙印上的第一道火焰亮了起来——翠绿、温和，像初春的草尖。 |
| 5 | focus `{npc: quest_elder}` 900 | — |
| 6 | say elder | `.6` 灵脉之印稳住了……精灵贤者艾兰迪尔若泉下有知，也该安心了。 |
| 7 | say elder | `.7` 可碑文说的是五印。另一道就在东边的暮色森林，几百年前就碎了。 |
| 8 | say elder | `.8` 我写了封信，托人捎给森林营地的侦察兵。孩子，一路当心，记得回来。 |
| 9 | whisper | `.9` 一盏灯亮了。还有四盏。 |
| 10 | narrate | `.10` 向东，是终年不见天日的森林。你的旅程，才刚刚开始。 |
| 11 | narrate | `.11` 村长托药师带着种子动身，去往精灵塔的废墟。那片灰烬里，将长出一座药草园。 |

**`cs_boss_goblin_chief`** (first sight ≤ 9 tiles; beat id `boss_goblin_chief`; `:307-317`):

| n | step | key → zh / en |
|---|---|---|
| 1 | focus `{monster: goblin_chief}` 800 | — |
| 2 | title | `story.boss.goblin_chief.name` 碎牙·格罗克 / Grokk Brokentooth; `.epithet` 灰烬部落之主 / Chief of the Ashen Tribe |
| 3 | say `{monster: goblin_chief}` | `story.cs_boss_goblin_chief.3` 烙印！就是那只手！火说了，谁砍下那只手，谁就永远不死！ |
| 4 | shake 0.006 / 400 | — |
| 5 | whisper | `.5` 别当真，他不过是条听话的狗。……不过，让我看看你怎么对付狗。 |
| 6 | focus player 600 | — |

English lines: `src/i18n/locales/story.ts:359-386, 489-490`. Speakers: `story.speaker.hero`, `story.speaker.villain`.
Also in Ch1: the mini-boss 哥布林萨满 pre-fight lines (monsters spec §8.3). Writing limits enforced by tests (zh chars):
say/whisper ≤ 60, narrate ≤ 70, slide text ≤ 110, titles ≤ 12, subtitles/epithets ≤ 14, offer ≤ 70, complete ≤ 50
(`src/__tests__/StoryScript.test.ts:146-165`, `docs/story.md` §七).

### 10.3 Elder dialogue tree (`src/data/dialogueTrees.ts:9-104`; en `src/i18n/locales/en.ts:1139-1169`)
Start `root`. `→` = nextNodeId; `[Q:id]` questTrigger; `[pre:id]` prereqQuests; `[+]` reward.

| node | text (zh) | choices |
|---|---|---|
| `root` | 勇士，你来了！翡翠平原正面临严重的威胁。最近村庄周围出现了大量怪物，村民们不敢出门。 | 告诉我发生了什么 → explain; 我能帮什么忙？ → help; 这里有什么奖励吗？ → reward_ask |
| `explain` | 三天前，一群哥布林从南方的山洞里涌出来。它们不仅袭击了几处农田，还抢走了村民的物资。更糟糕的是，史莱姆也开始在北部湿地繁殖失控。 | 我先去清理史莱姆 → accept_slimes [Q:q_kill_slimes]; 哥布林更危险，先对付它们 → goblin_first [pre:q_kill_slimes]; 两边我都处理 → both |
| `help` | 太好了！你愿意帮忙真是太棒了。当前最紧急的是清除平原上泛滥的史莱姆，它们正在侵蚀我们的农田。 | 交给我了！ → accept_slimes [Q:q_kill_slimes]; 还有其他任务吗？ → other_quests |
| `reward_ask` | 村庄虽然不富裕，但我们会尽力回报帮助我们的人。完成任务后你会获得金币和经验奖励，表现出色的话还有特殊物品。这些是提前给你的补给。 | 好的，我来帮忙（获得补给） → help [+ gold 30, exp 50] (repeatable — Q2); 我再想想 → farewell |
| `accept_slimes` (end) | 感谢你！平原北部的湿地区域史莱姆最多，小心它们的腐蚀攻击。消灭10只后回来找我。 | — (Leave) |
| `goblin_first` | 你说得对，哥布林确实更加危险。它们的营地在平原南部，消灭15只哥布林应该能挫败它们的锐气。 | 立刻出发！ → accept_goblins [Q:q_kill_goblins]; 我需要更多信息 → goblin_info |
| `accept_goblins` (end) | 注意安全，勇士。哥布林虽然单个不强，但它们喜欢群体作战。 | — |
| `goblin_info` | 哥布林有三种——普通的巡逻兵、弓箭手和首领。首领比较狡猾，会躲在后排指挥。建议你先消灭弓箭手，再处理近战的。 | 明白了，我去清剿 → accept_goblins [Q:q_kill_goblins]; 我先去做其他准备 → farewell |
| `both` | 真是勇敢！先从史莱姆开始吧，它们虽然弱但数量多。清理完后再去对付哥布林。 | 好的，先去清理史莱姆 → accept_slimes [Q:q_kill_slimes] |
| `other_quests` | 除了史莱姆，药师调伤药还缺史莱姆凝胶，她走不开小摊。村里的伤员也等着西边草坡上的草药救命。 | 我去给药师收集凝胶 → accept_gel [Q:q_collect_slime_gel]; 我去采急救草药 → accept_herbs [Q:q_herb_gathering]; 先处理史莱姆问题 → accept_slimes [Q:q_kill_slimes] |
| `accept_gel` (end) | 史莱姆被打散时会留下凝胶，攒够6份，亲手送到药师的小摊上。她会记着你的好。 | — |
| `accept_herbs` (end) | 翡翠草药长在西边的草坡上，叶子泛着一点微光。采5株回来就好，伤员等不起。 | — |
| `farewell` (end) | 好的，勇士。如果改变主意了，随时来找我。 | — |

Example of the filtering rules: with `q_kill_slimes` active, `both` shows its only choice hidden → a **Back** button.

### 10.4 Lore and exploration content (Chapter 1)
**Lore collectibles** (`src/data/loreCollectibles.ts:25-65`) — auto-collected when `distSq(hero, tile) <= 4`
(2 tiles), every frame (`ZoneScene.ts:4756-4786`): add to `loreCollected` (saved), emit `LORE_COLLECTED {entry}` → modal
panel (title = raw zh name, zone name, text; 440×240; closes on X/backdrop or after **8000 ms**; QUIRK Q11), log
`zone.lore.discovered {loreName}`, sprite fades (500 ms). No reward. Collected entries never respawn. Quest log 传说 tab
lists them per zone (`ui.questLog.loreCollected {count}/{total}`, undiscovered = `ui.questLog.loreUndiscovered`).

| id | tile | spriteType (label colour) | hidden | zh name / en (`data.lore.<id>.name`) | text (zh; en `data.lore.<id>.text`) |
|---|---|---|---|---|---|
| `lore_ep_01` | (45,35) | ancient_tablet `#DAA520` | no | 古老石碑 / Ancient Stone Tablet | 这块石碑上刻着古老的文字："翡翠平原曾是精灵族的圣地，千年前的大灾变摧毁了他们的文明。灵脉之力散布于大地，吸引了各种生灵前来。"石碑的底部还残留着精灵族的纹章。 |
| `lore_ep_02` | (105,15) | torn_journal `#D2B48C` | yes | 褪色日记 / Faded Journal | 一本被风雨侵蚀的日记本，字迹模糊但仍可辨认："第七日，哥布林部落的异变越来越严重。它们的萨满似乎在进行某种古老的仪式，试图唤醒沉睡在平原地下的灵脉之力。如果成功，后果不堪设想……" |
| `lore_ep_03` | (15,108) | crystal_shard `#66CCFF` | yes | 水晶碎片 / Crystal Shard | 一块散发着微弱光芒的水晶碎片。它的内部似乎封印着某种影像——你看到了远古精灵族在平原上建造高塔、与自然和谐共生的画面。水晶的裂痕中渗出淡淡的绿色光芒，那是灵脉残余的能量。 |
| `lore_ep_04` | (85,80) | carved_stone `#B0C4DE` | no | 雕刻石柱 / Carved Stone Pillar | 一根半埋在泥土中的石柱，上面雕刻着精灵族的历史。根据这些浮雕，翡翠平原的灵脉曾经是连接五大区域的能量枢纽。大灾变后灵脉断裂，各区域从此走向不同的命运。 |

Render: themed prop over a pulsing coloured pool of light (α 0.3↔0.7, scale 0.9↔1.1, 1500 ms) and a floating
`✦ <name>` label. **3D:** prop mesh per spriteType + glow decal + world label.

**Hidden area** `hidden_ep_elven_cache` 精灵族秘密宝库 (map data `emerald_plains.ts:49-62`; shared with the world spec):
centre (108,108) radius 6 → bounds (102..114, 102..114). Discovered (once per save, `discoveredHiddenAreas`) when the
four corners and the centre of the bounds are all in this **visit's** explored set (tiles within 10 tiles of the hero
since zone entry, `EXPLORE_VIEW_RADIUS = 10`); then emit `HIDDEN_AREA_DISCOVERED`, log `zone.hiddenArea.discovered`, show
the discovery text (gold, 300 ms in, hold 3000, out 300) and place rewards: chest `rare` at (108,108) → on click within
2 tiles: `generateEquipment(levelRange[1] = 7, 'rare')` into the bag (`addItem` result ignored); gold pile `200` at
(110,107) → +200 gold. Rewards are not saved (QUIRK Q6).

**Story decorations** (`emerald_plains.ts:65-87`; no gameplay effect): `story_ep_ruined_tower` 倒塌的精灵塔 (55,45) ruins;
`story_ep_goblin_totem` 哥布林图腾柱 (40,70) war_banner; `story_ep_ancient_well` 干涸的古井 (88,55) broken_altar. Label
visible within 3 tiles; tooltip (name + description) within √2 tiles of the nearest one; emits
`STORY_DECORATION_INTERACT {decoration}` (no listener). i18n `data.storyDeco.<id>.name/.desc` (web shows raw zh, Q11).

### 10.5 Chapter-1 achievement expectations
After a straight Ch1 run: `ach_first_kill`, `ach_kill_goblin_chief`, `ach_quest_10` (after the 10th turn-in),
likely `ach_kill_100` (50 kills), `ach_level_10`, `ach_kill_slime` (50 slimes incl. side quests), possibly
`ach_explore_all` (Q4), `ach_collect_legendary`.

---

## 11. Save data owned by this area (`src/data/types.ts:609-670`)

| Field | Content | Default for old saves |
|---|---|---|
| `quests` | `QuestProgress[]` (§1.5) | `[]` |
| `storySeen` | beat ids (§8.1) | `['prologue']` |
| `dialogueState` | `{npcId: {visitedNodes[], choicesMade{}}}` | `{}` (`SaveSystem.ts:83`) |
| `miniBossDialogueSeen` | mini-boss ids | `[]` |
| `loreCollected` | lore ids | `[]` |
| `discoveredHiddenAreas` | hidden-area ids | `[]` |
| `achievements` | progress counters + `{achId: 1}` | `{}` |
| `homestead.embers` | int ≥ 0: quest turn-ins + kills (§4.4) | 0 |
| `homestead.buildings` | `{buildingId: level}`; Ch1 writes `herb_garden: 1` on the `q_secure_plains` turn-in (§4.6) | `{}` |
| `homestead.garden` | `{progress: int, stock: {itemId: int}}` — advances on every kill once `herb_garden` ≥ 1 (§4.7.1) | `{progress 0, stock {}}` |
| `homestead.expedition`, `.blessing`, `.towerReturn` | later milestone; always `null` in Ch1 (written by `HomesteadTower.toSave`, `HomesteadTower.ts:214-222`) | `null` |
| `pets` | `{owned: PetInstance[], active: string\|null}` (§18.2). Ch1 writes `pet_sprite` on the `q_pet_sprite_friend` turn-in; its level, exp and bond then change on every kill, every active minute and every feed | `{owned: [], active: null}`; legacy `homestead.pets` migrated (save spec §3.3) |

Autosave happens on zone entry, zone change, quest **turn-in**, end of a story beat queue, difficulty completion,
soul-echo claim, etc. — **not** on accept or progress; progress since the last autosave is lost on a hard quit.

---

## 12. Event flows (Chapter 1 walkthrough, for integration tests)

1. **New game** → `ZoneScene.create` → … → `ZONE_ENTERED {emerald_plains}` → `achievements.update('explore')` →
   `StoryDirector.start()`: enqueue `prologue`, `chapter_emerald_plains` → `STORY_STATE {true}` → prologue (cinematic) →
   chapter card (cinematic) → autosave → `STORY_STATE {false}`.
2. **Talk to elder** (≤ 3 tiles) → log line 0 → `updateProgress('talk','quest_elder')` (nothing) → `NPC_INTERACT` →
   quest card (4 offers) → Accept M1 → `QUEST_ACCEPTED` (+ pin, toast, sfx, markers, world sync, hunts check).
3. **Kill slimes** → each kill: `updateProgress('kill','slime_green')` → `QUEST_PROGRESS` (popup at 3, 6, 9) → 10th →
   `QUEST_PROGRESS{completesQuest:true}` + `QUEST_COMPLETED` → banner “任务完成! / 史莱姆之灾 / 返回 村长 交付任务”, elder `?`.
4. **Turn in M1** → `QUEST_TURNED_IN` → StoryDirector enqueues `cs_ep_mark` (650 ms) → embers +2 → exp/gold → autosave;
   the card closes; 900 ms later the director is busy → wait for `STORY_STATE{false}` + 300 ms → reopen the elder card
   with M2 and newly available side quests.
5. **M3 explore** → within 8 tiles of (40,52) at a 500 ms check → complete → turn in → `cs_ep_whisper`; tower unlock log;
   the HUD ember counter appears showing the running total (§4.4.3 — includes earlier elite-kill embers).
6. **Chief** → within 14 tiles: nameplate “碎牙·格罗克”, boss bar; within 9: `boss_goblin_chief` intro (world frozen);
   kill → `updateProgress('kill','goblin_chief')`, `ach_kill_goblin_chief`, `+5 余烬` float and embers +5, bar cleared →
   turn in with weapon/armor choice. (Every elite kill — chief, shaman, hunt leaders — likewise gives +5, §4.4.2.)
7. **M5** → both circles → turn in (jewelry/boots/gloves) → `cs_ep_finale`, embers +5, herb-garden wing log,
   `buildings.herb_garden = 1` → autosave. From now on: 14 kills → `garden.stock` +1 item, until 8 are stocked (§4.7.1).
8. **Sprite side quest** (offered from Lv 1, quest level 3) → 4 clues → turn in at the elder → the steps in
   §18.7: log `sys.pet.obtained`, `PET_OBTAINED` (no listener), `PET_CHANGED`, medallion shown, autosave with
   `pets.active = 'pet_sprite'`, and the beast spawned beside the hero next frame (peaceful in camp). Every later kill:
   `+3 %` kill exp (floored), beast exp `10 + monster level`, bond +1 (cap 3).

---

## 13. Core API proposal (C++20, no exceptions/RTTI)

```cpp
namespace abyss::quest {
enum class ObjType : uint8_t { Kill, Collect, Explore, Talk, Escort, DefendWave, InvestigateClue, CraftCollect, CraftCraft, CraftDeliver };
enum class QuestStatus : uint8_t { Active, Completed, TurnedIn, Failed };          // "no record" = never accepted
struct ObjectiveProgress { int32_t current = 0; };
struct QuestProgress { QuestId id; QuestStatus status; SmallVector<ObjectiveProgress, 6> objectives; };

class QuestSystem {                                   // data-driven, deterministic, no I/O
public:
  explicit QuestSystem(const QuestDb& db, EventSink& events);
  bool Accept(QuestId);
  void UpdateProgress(ObjType, std::string_view targetId, int32_t amount = 1);
  void Fail(QuestId);
  const QuestReward* TurnIn(QuestId);                 // status change + events only
  void ForEachOpen(FunctionRef<void(const QuestDef&, const QuestProgress&)>) const;   // active+completed, insertion order
  Span<const QuestId> OfferList(NpcId, int heroLevel, OfferRule rule) const;          // Available | QuestCard | Indicator
  NpcMarker Marker(NpcId, int heroLevel) const;
  void SetTracked(std::optional<QuestId>); const QuestDef* Guided(ZoneId) const;
  void Save(SaveWriter&) const; void Load(SaveReader&);
};
GuideTarget ComputeGuideTarget(const QuestDef&, const QuestProgress&, const GuideWorld&);   // pure, §5.1
Vector<TilePoint> ResolveGatherSpots(Area, int count, WalkableFn, std::string_view seedKey); // bit-exact, §3.4
}
namespace abyss::story {
struct BeatCommand { enum Kind { Sequence, Chapter, Cutscene } kind; StringId id; uint32_t delayMs; std::optional<PetId> grantPet; };
class StoryDirector {                                 // queue + state; presentation is external
public:
  bool OnZoneEntered(ZoneId);                         // returns "chapter card queued"
  void OnQuestTurnedIn(QuestId); void OnQuestAccepted(QuestId); void OnMonsterKilled(MonsterDefId);
  void Tick(uint32_t dtMs, const BossScanInput&);     // 250 ms boss scan; emits BossBar / enqueues boss beats
  std::optional<BeatCommand> PopNextBeat();           // marks seen at pop (web parity)
  void OnBeatPresentationFinished();                  // presenter → core (also applies grantPet)
  bool IsCinematic() const; bool IsBusy() const;
};
}
```
Homestead state that Ch1 already mutates (§4.4, §4.6, §4.7) — the full tower API is a later milestone, but this subset
must exist in the Ch1 core so saves are forward-compatible:
```cpp
namespace abyss::home {
int32_t EmbersForKill(bool elite, bool isMiniBoss, int32_t eliteAffixCount);   // 5 / 3 / 1 / 0, first match wins
int32_t EmbersForQuest(QuestCategory, std::optional<int32_t> rewardEmbers);     // reward ?? (main 2 : side 1)
int32_t GardenInterval(int32_t lv); int32_t GardenCapacity(int32_t lv);
ItemBaseId RollGardenYield(int32_t lv, Rng&);
class HomesteadTower {                               // owned by HomesteadState next to `buildings`
public:
  int32_t AddEmbers(int32_t n);                      // max(0,n); returns amount added
  SmallVector<BuildingId, 6> SyncUnlocks(Span<const QuestId> turnedIn);   // raises newly unlocked wings to Lv1
  bool TowerUnlocked() const; bool IsBuildingUnlocked(BuildingId) const;
  std::optional<ItemBaseId> OnKillGarden(Rng&);      // §4.7.1
  bool OnKillExpedition();                           // §4.7.3
  // OnKill(def, affixCount) = AddEmbers(EmbersForKill(..)) → OnKillGarden → OnKillExpedition; emits EmbersGained{n, pos}
};
}
```
Event: `EmbersGained{amount, worldPos, source: Kill|Quest}` (presentation: float for kills, `homestead.log.questEmbers` log
for quests).

UE side: `UStoryPresenter` (owns `WBP_Story`, plays sequences/chapter/cutscene steps, drives the camera rig through
`IStoryCameraHooks {Focus, Shake, Flash}`, reports finish); `AQuestWorldActor` spawns gather/clue/guide visuals from core
snapshots; `UQuestCardWidget`, `UQuestTrackerWidget`, `UQuestLogWidget`, `UDialogueWidget` consume core view-models.
Core tick order inside a sim step (web parity): world sim (skipped while cinematic) → … → quest-world update
(gather/clue/guide) → story scan; 500 ms quest observers (explore + markers).

Events (typed signals, payloads as in the web): `QuestAccepted{questId}`, `QuestProgress{questId, objectiveIndex,
current, required, targetId, amount, completesQuest}`, `QuestCompleted`, `QuestTurnedIn`, `QuestFailed`,
`QuestTrackedChanged{questId?}`, `StoryState{active}`, `BossBar{name, epithet, hpRef}|null`, `NpcInteract`, `ShopOpen
{npcId, items, type}`, `DialogueClose`, `LoreCollected{entry}`, `HiddenAreaDiscovered{area}`, `AchievementUnlocked{id}`,
`MiniBossDialogue`, `ZoneEntered{mapId}`, `LogMessage{text, type}`, `PetObtained`.

---

## 14. Data to export (JSON) and string tables

| File | Source | Shape |
|---|---|---|
| `quests/quests.json` | `AllQuests` (`src/data/quests/all_quests.ts`) — **all chapters** | `QuestDefinition[]` verbatim (incl. `hunts`, `escortNpc`, `defendTarget`, `clues`, `craftPhases`, `reacceptable`), array order kept |
| `npcs/npcs.json` | `NPCDefinitions` (`src/data/npcs.ts`) | object keyed by id (key order = `questGiverOf` tie-break), `dialogueTree` replaced by the tree id |
| `dialogue/trees.json` | `DialogueTrees` (`src/data/dialogueTrees.ts:545-551`) + `MiniBossDialogues` (`src/data/miniBosses.ts:144`) | `{treeId: DialogueTree}` |
| `story/script.json` | `PROLOGUE, CHAPTERS, CUTSCENES, BOSS_INTROS, STORY_TRIGGERS, EPILOGUE, CREDITS` (`src/data/story/script.ts`) | one object; trigger array order matters |
| `story/moods.json` | `MOODS` (`src/scenes/StoryScene.ts:35-43`) | colours (render) |
| `world/lore.json` | `LoreByZone` (`src/data/loreCollectibles.ts:255`) | `{zoneId: LoreEntry[]}` |
| `world/<zone>.json` (map spec) | `camps`, `fieldNpcs`, `hiddenAreas`, `storyDecorations` of each map | as in `MapData` |
| `achievements.json` | `ACHIEVEMENTS` (`src/systems/AchievementSystem.ts:5-18`) | array |
| `tuning/quest_story.json` | constants below | flat object |
| `pets/pets.json`, `tuning/pets.json` | `PETS` + constants (`src/data/pets.ts:94-185`, `PetSystem.ts:38-56`, `PetCompanion.ts:50-56`) | §18.1, §18.5.12; string table `src/i18n/locales/pets.ts` (all keys, zh-CN + en) |

Constants (`tuning/quest_story.json`): `levelGateAbove 5`, `gatherRange 1.3`, `clueRange 2`, `clueSearchRings 8`,
`guideNear 2.5`, `guideRefreshMs 250`, `questObserverMs 500`, `fallbackCollectChance 0.25`, `npcInteractRange 3`,
`npcPickRadius 1.8`, `npcAlertRange 3`, `escort {joinRange 5, catchUp 14, followMin 2, repathMs 400, speedFactor 0.9,
speedDivisor 38, hpBase 100, hpPerLevel 20, threatRangeSq 16, hitIntervalMs 2000, dmgMul 0.3, arriveEscortSq 25,
arriveHeroSq 36, guideFetchRange 4}`, `defend {hpBase 200, hpPerLevel 30, startRangeSq 225, waveDelayMs 5000,
spawnRadius 8, baseCount 3, hpPerWave 0.3, dmgPerWave 0.2, hitRangeSq 9, hitIntervalMs 2000, dmgMul 0.2}`,
`story {bossSight 9, bossBarRange 14, scanMs 250, delayQuestMs 650, delayZoneMs 900, delayKillMs 0, camReturnMs 450,
letterboxPx 78, letterboxMs 450, typewriterCps 38, chapterHoldMs 3800, titleHoldMs 2200, focusDefaultMs 900,
creditsPxPerSec 42}`, `lore {pickupRangeSq 4, panelAutoCloseMs 8000}`, `hiddenArea {exploreViewRadius 10, claimRangeSq 4}`,
`questDropFallbackPerQuest true`, `embers {killElite 5, killMiniBoss 3, killAffixed 1, questMain 2, questSide 1}`,
`garden {intervalBase 16, intervalPerLevel 2, intervalMin 6, capacityBase 4, capacityPerLevel 4, leyFruitBase 0.12,
leyFruitPerLevel 0.03, hpShare 0.6}` (§4.4, §4.7; also exported with the homestead tables when that milestone lands —
keep one source of truth). `homestead/buildings.json` = `BUILDINGS` (`src/data/homestead.ts:27-69`: id, maxLevel,
costPerLevel[{gold, embers?}], bonusPerLevel[{stat, value}], unlockQuest?, allyNpc?) is needed in Ch1 for `syncUnlocks`.

String tables to export (zh-CN + en): `src/i18n/locales/story.ts` (all), `src/i18n/locales/questStory.ts` (all), and from
`zh-CN.ts`/`en.ts` the prefixes `data.quest.`, `data.questTarget.`, `data.questClue.`, `data.npc.`, `data.dialogue.`,
`data.escortNpc.`, `data.lore.`, `data.hiddenArea.`, `data.storyDeco.`, `data.achievement.`, `data.monster.hunt_`,
`sys.quest.`, `sys.questCard.`, `sys.tracker.`, `sys.achievement.`, `sys.pet.`, `ui.questCard.`, `ui.questTracker.`,
`ui.questLog.`, `ui.dialogue.`, `ui.achievement.`, `ui.miniBoss.`, `zone.quest.`, `zone.questComplete`, `zone.escort.`,
`zone.defend.`, `zone.craft.`, `zone.deliver.`, `zone.lore.`, `zone.hiddenArea.`, and `homestead.log.questEmbers`,
`homestead.log.towerUnlocked`, `homestead.log.wingUnlocked`, `homestead.float.embers` (`src/i18n/locales/homestead.ts`).

---

## 15. Quirks (web behaviour that is a bug or a 2D artefact)

| # | Quirk | Where | Recommendation |
|---|---|---|---|
| Q1 | `acceptQuest` ignores the level gate (only offer lists apply it); re-accept of a failed quest skips prereqs. | `QuestSystem.ts:59-103` | Keep (harmless). |
| Q2 | Dialogue-choice rewards are granted **every** time the choice is picked: the elder's “获得补给” gives +30 gold +50 exp infinitely (scout's `explore_first` reward is self-limiting because its quest choice hides). | `UIScene.ts:3624-3656` | **FIX**: one-time per `(npcId,nodeId,choiceIndex)`, persisted in `dialogueState` (OQ1). |
| Q3 | Each kill increments the generic `kill` achievement counter twice (`ach_kill_100` at 50 kills, `ach_kill_500` at 250). | `ZoneScene.ts:3841-3842`, `AchievementSystem.ts:24-51` | **FIX** (count once) unless parity of unlock pace is wanted (OQ2). |
| Q4 | `ach_explore_all` counts zone-scene creations (loads, deaths, re-entries), not distinct zones. | `ZoneScene.ts:692` | **FIX**: count distinct story zones (OQ2). |
| Q5 | Gather nodes are only re-synced on accept/complete/turn-in/fail/track, so a finished gather objective in a multi-objective quest leaves consumable nodes behind. | `QuestWorld.ts:113-122` | FIX: also sync on `QuestProgress` (no Ch1 impact). |
| Q6 | Hidden-area rewards exist only in the visit where the area was discovered; the chest item is lost if the bag is full. | `ZoneScene.ts:4866-5031` | FIX: persist unclaimed rewards; overflow to stash (OQ5). |
| Q7 | `q_find_goblin_chief.questArea` (25,65) does not contain the chief (spawn (15,95)); the minimap circle misleads (the arrow is right). | `all_quests.ts:65`, `emerald_plains.ts:27` | FIX data: centre the area on (15,95) or keep (OQ4). |
| Q8 | Defend waves pick from the zone's whole monster list (zone bosses included) and spawn without a walkability check. | `ZoneScene.ts:7018-7051` | FIX later (not Ch1). |
| Q9 | Reward choices are rerolled after a reload (cache not saved). | `ZoneScene.ts:4096-4105` | Keep, or persist with the save (minor). |
| Q10 | During cinematics the game clock keeps running: cooldowns and buff durations elapse while the world is frozen. | `ZoneScene.ts:1347` | Port: pause the sim clock while cinematic (deviation, OQ7). |
| Q11 | Several strings render raw zh even though i18n keys exist: NPC dialogue lines (`data.npc.<id>.dialogue.<n>`), dialogue trees (`data.dialogue.*`), lore panel (`data.lore.*`), story decorations, escort NPC name (`data.escortNpc.*`), quest-log objective names, achievement log/toast names. | various | **FIX**: always resolve keys. |
| Q12 | Quest system log lines interpolate the raw zh quest name. | `QuestSystem.ts:74,98,156,182,196` | FIX: localized name. |
| Q13 | Achievement stat bonuses apply only after the next equip-stat cache invalidation. | `ZoneScene.ts:3143-3152` | FIX: invalidate on unlock. |
| Q14 | Story beats are marked seen when dequeued, before they play; quitting mid-beat skips it forever. | `StoryDirector.ts:152` | Keep (prevents loops) — or mark on finish (OQ7). |
| Q15 | The lore panel's 8 s auto-close timer can close a newer lore panel opened within those 8 s. | `UIScene.ts:5407` | FIX: cancel the timer on close. |
| Q16 | `embersForKill`'s mini-boss tier (3) is dead with shipped data: every `isMiniBoss` def is also `elite`, so mini-bosses and hunt leaders give 5; the source comment says "mini-bosses 3". | `src/data/homestead.ts:87-92`, `miniBosses.ts`, `QuestHunts.ts:61-63` | Keep parity (5); implement the 3-branch anyway. |
| Q17 | Herb-garden `potionDiscount` (5 %/level) is summed into the homestead bonuses but never read — no shop discount. | `src/data/homestead.ts:32`, `HomesteadSystem.ts:61-71` | Keep (no effect) until the homestead milestone decides. |
| Q18 | Kill embers ignore the tower lock and the respawning `goblin_chief` (15 s) is an unbounded 5-ember farm; the `+N 余烬` float shows before the HUD counter exists. | `EmberTower.ts:187-195`, monsters spec §7 | Keep parity (embers are only spent in later milestones); see OQ10 for the float. |

---

## 16. Test vectors (core unit tests)

1. **Gather hash**: `ResolveGatherSpots({22,26,9}, 7, all-walkable, "q_herb_gathering:0")` = (23,25) (28,23) (20,17) (18,32)
   (23,34) (27,29) (23,18); FNV state after the key = 3613349914; first rands as in §3.4.
   `ResolveGatherSpots({0,0,5}, 3, all-walkable, "abc")` = (−1,−3) (−4,−2) (0,4) (exercises `floor(x+0.5)` on negatives).
   With the web's generated `emerald_plains` walkability: (23,25) (28,23) (18,32) (23,34) (27,29) (23,18) (28,31).
2. **Talk gating**: gel quest, 5/6 gel, talk herbalist → no progress; 6/6 then talk → `QUEST_PROGRESS{1}` +
   `QUEST_COMPLETED`.
3. **Craft ordering**: craft_deliver progress before collect is complete stays 0 and emits nothing.
4. **Offer lists**: L1 new game, elder → card entries `[q_kill_slimes, q_collect_slime_gel, q_herb_gathering,
   q_pet_sprite_friend]`; marker `!`. After M1 completed: marker yellow `?`, card turn-in first.
5. **Guide**: kill objective with no living target falls back to spawn points; escort > 4 tiles → escort tile, else
   (50,90); completed quest with giver in another zone → null.
6. **Story queue**: new game → beats `prologue`, `chapter_emerald_plains` in order, both marked; turn-in M1 →
   `cs_ep_mark` with 650 ms delay; second zone entry → nothing; load of a save without `storySeen` → no prologue.
7. **Boss intro**: chief at 9.0 tiles → enqueue `boss_goblin_chief` once; at 14.0 → bar only.
8. **Rewards**: `rewardItemLevel` for M4 at hero L3 → 7, L10 → 10, L15 → 12; quality main → rare, side → magic;
   `embersForQuest` M5 → 5, M1 → 2, gel → 1, sprite → 2.
   `embersForKill`: `{elite}`,0 → 5; `{elite,isMiniBoss}`,1 → 5 (shaman, hunts); `{isMiniBoss}`,0 → 3; `{}`,2 → 1;
   `{}`,0 → 0. `addEmbers(-3)` → 0 (unchanged); `addEmbers(2.9)` → 2.
9. **Achievements**: with Q3 kept, 50 kills unlock `ach_kill_100`; with the fix, 100.
10. **Save migration**: an active quest saved with 1 objective whose definition now has 2 → reset to active, zeros.
11. **Kill embers in Ch1**: new game, kill 10 slimes + 1 chief + 1 shaman → `embers = 10`; turn in M1 → 12. HUD text stays
   `""` until M3 is turned in, then `"✦" + embers`.
12. **syncUnlocks**: turned-in `{q_explore_goblin_camp}` → `[]`, `towerUnlocked`, buildings unchanged; add
   `q_secure_plains` → `["herb_garden"]`, `buildings.herb_garden == 1`; call again → `[]`; with `buildings.herb_garden == 3`
   beforehand → stays 3. `warehouse` never auto-raised.
13. **Garden**: Lv0 → no change on kills. Lv1: kills 1–13 → `progress` 1…13, stock empty; kill 14 → `progress 0`, one item
   (rng 0.10 → `c_ley_fruit`; rng 0.50, 0.30 → `c_hp_potion_s`; rng 0.50, 0.70 → `c_mp_potion_s`). Lv1 with 8 stocked →
   kill leaves `progress` unchanged. `gardenInterval` 1..5 = 14,12,10,8,6, `gardenInterval(6) = 6`; `gardenCapacity` 0 → 0,
   1..5 = 8,12,16,20,24. Lv4 HP yield → `c_hp_potion_l`, MP → `c_mp_potion_m`.

---

## 17. Open questions

| # | Question | Default if unanswered |
|---|---|---|
| OQ1 | Make dialogue-choice rewards one-time (Q2)? | Yes: one-time per `(npcId, nodeId, choiceIndex)`, saved in `dialogueState`. |
| OQ2 | Fix achievement counting (Q3 double kill count, Q4 explore = scene loads) or keep the web unlock pace? | Fix both (count kills once; count distinct story zones). |
| OQ3 | Chapter-1 milestone: ship the ley-beast slice, or only record `pet_sprite`? And embers without the Ember Tower? | **Ship the §18.0 slice** (PetSystem + `pet_sprite` companion + P panel + medallion) because the web's Ch1 gameplay includes it. Embers are recorded with no tower UI (§4.4). If the companion is cut anyway, apply §18.10 exactly: passive and growth still apply, there is a log line, the 「宠物」 card line stays, and fruit stays feedable through the panel. |
| OQ4 | Re-centre `q_find_goblin_chief.questArea` on the chief's spawn (15,95) (Q7)? | Yes (data fix; the arrow already points correctly). |
| OQ5 | Persist unclaimed hidden-area rewards and route chest overflow to the stash (Q6)? | Yes. |
| OQ6 | 3D/touch NPC interaction: keep “tap within 3 tiles” only, or add tap-to-walk-then-talk and a contextual Talk button on touch? | Add walk-then-talk (same 3-tile range to trigger) and a Talk button on touch; core rule unchanged. |
| OQ7 | Pause the sim clock during cinematics (Q10)? Mark beats seen on finish instead of on dequeue (Q14)? | Pause the clock; keep mark-on-dequeue. |
| OQ8 | `q_explore_goblin_camp` / `q_secure_plains` turn-ins log Ember Tower unlock lines in the web; show them in the Ch1 build? | Suppress until the tower milestone (state still derived from turned-in quests). |
| OQ9 | Escort destination (50,90) is labelled “南方营地” but the southern camp is at (95,100); keep the data? | Keep (parity); revisit in content pass. |
| OQ10 | Show the `+N 余烬` kill float in the Ch1 build (web shows it from the first elite kill, before the HUD counter / tower exist)? | Show it (parity; it explains the HUD value that appears after M3). Kill embers and garden growth are computed and saved regardless. |

---

## 18. Ley-beasts (灵兽, pets) — Chapter-1 contract and minimal pets spec

No separate pets spec exists yet. `loot-items-inventory.md` §7.4 ("pets spec"), `classes-stats-skills.md` §13.7 ("pet
spec"), `save-ui-input.md` §0.4 / §6.13 and `art-inventory-ch1.md` §13 all point here. Until a dedicated `pets.md` is
written for the homestead/pets milestone, **this section is the pets spec**. It covers the general systems (all 8 beasts,
every ability kind) so later chapters only add data. It also lists exactly what Chapter 1 needs.

**What the web does in Chapter 1.** Turning in `q_pet_sprite_friend` gives `pet_sprite` (灵脉精灵). From the next frame
it is a live companion. It follows the hero and fights the hero's target with a ranged arcane bolt (range 4.5 tiles,
one every 1800 ms). It heals the hero (8 % max HP, only below 70 % HP, 12 s cooldown). It sometimes takes swings meant
for the hero (stray swings). Its HP is 45 % of the hero's max HP. It levels from kills, and its passive (`expBonus`
3 + 0.4/level) raises kill exp. Ley Fruit (`c_ley_fruit`) drops in Ch1 and is fed to it from the P panel. None of
this needs the Ember Tower. Source: `src/data/pets.ts:104-113`, `src/systems/PetCompanion.ts:120-136, 162-239`,
`src/systems/PetSystem.ts:268-290, 344-352`, `src/scenes/ZoneScene.ts:673, 1483, 2825, 3154-3157, 3796-3805, 4130,
7133-7175`.

**Overrides in other specs.** This section replaces these "later milestone" statements for the Ch1 slice defined in
§18.0:
* `art-inventory-ch1.md` §13: "the companion ships later" (`:1396-1397`).
* `save-ui-input.md` §0.4 "pet medallion" (`:59-60`), §6.13 "Pet medallion … later milestone" (`:862-863`), the `P`
  key row (`:534`), the "Ch1 only records … ownership" note (`:1056`) and OQ-UI-3 (`:1436-1437`).
* `combat-feel.md` §5.1 "pet may intercept: later milestone hook" (`:246`).
* §4.5 and OQ3 of this spec.

### 18.0 Decision for Chapter 1 (resolves OQ3)
**Default: ship the Chapter-1 ley-beast slice.** It contains:
1. `PetSystem` (core, complete and general, §18.3).
2. The pet bonus routing into the hero (§18.4).
3. `PetCompanion` (§18.5). Ch1 must fully support: a flying ranged beast, the basic attack, the `heal` and `shield`
   ability kinds, stray-swing interception, exhaustion/recovery, and evolution stage 1. Other ability kinds are
   specified for completeness and are exercised from Ch2.
4. The P panel (list, rest/fight toggle, feed) and the desktop HUD medallion (§18.8).
5. The `pet_sprite` 3D asset for stages 0 and 1 (§18.9).

Why: in the web the beast changes Ch1 balance (kill exp, heals, soaked swings, kill credit, Ley Fruit use). The quest's
own turn-in line also assumes a visible companion: `data.quest.q_pet_sprite_friend.complete` = 「瞧，那小家伙赖在你肩头
不肯走了……」 (`src/i18n/locales/questStory.ts:32`). If production cuts the companion anyway, §18.10 gives the exact
degraded contract.

### 18.1 Data — `PetDef` (`src/data/pets.ts:12-92`), exported verbatim as `pets/pets.json`
```
PetRole        = support | scout | melee | assassin | tank | ranged | caster
PetAbilityKind = heal | shield | mark | strike | bolt | cone | nova | taunt | buff | revive
PetElement     = physical | fire | ice | lightning | poison | arcane
PetAbilityDef  { id, kind, unlock: 0|1 (0 = from start, 1 = 觉醒), cooldownMs, range (tiles pet→target; 0 = no target),
                 damage? (× pet attack), value? (heal fraction / DR / amplify / revive HP), durationMs?, radius?, arc? (rad),
                 element?, crit?, leap?, bleed?, burn? (fraction of the hit per tick), slow? (%), stunMs?, hits?, mana?, self? }
PetCombatStyle { style: melee|ranged, range (tiles), attackMs, color (0xRRGGBB), element }
PetDef         { id, chapter 1..5, role, rarity common|rare|epic, flying, animCategory, combat, passive {stat, base, perLevel},
                 hpFraction (× hero max HP), abilities[] (array order = AI priority) }
```
Constants (`:94-101`): `PET_MAX_LEVEL 20`, `PET_EVOLUTION_LEVELS [10, 20]`, `PET_EVOLUTION_MULT [1, 1.5, 2]`,
`PET_MAX_BOND 5`, `LEY_FRUIT_ID 'c_ley_fruit'`. Helpers: `getPetDef(id)` (map lookup), `unlockedAbilities(def, evolved)` =
`abilities.filter(a.unlock <= evolved)` (passives included), `primaryAbility(def)` = first ability whose kind is not
`revive` (`:187-201`).

| id | ch | role | rarity | fly | anim | basic: style / range / ms / colour / element | passive (base + perLevel) | hpFrac |
|---|---|---|---|---|---|---|---|---|
| **`pet_sprite` 灵脉精灵** | **1** | support | common | yes | flying | ranged / 4.5 / 1800 / `0x8ff0c0` / arcane | `expBonus` 3 + 0.4 | 0.45 |
| `pet_owl` | 2 | scout | common | yes | flying | ranged / 5 / 1700 / `0xbfd8ff` / physical | `magicFind` 5 + 0.8 | 0.45 |
| `pet_storm_wolf` | 2 | melee | epic | no | beast | melee / 1.3 / 1200 / `0xdfe8ff` / physical | `attackSpeed` 3 + 0.35 | 0.7 |
| `pet_cat` | 2 | assassin | rare | no | beast | melee / 1.2 / 1100 / `0xb070ff` / physical | `critRate` 2 + 0.25 | 0.55 |
| `pet_jade_tortoise` | 3 | tank | epic | no | large | melee / 1.4 / 1700 / `0x6fe0b0` / physical | `defense` 4 + 1.2 | 1.2 |
| `pet_dragon` | 3 | ranged | rare | yes | flying | ranged / 4.5 / 1600 / `0xff7a2a` / fire | `damagePercent` 3 + 0.45 | 0.6 |
| `pet_phoenix` | 4 | support | epic | yes | flying | ranged / 4.5 / 1800 / `0xffb040` / fire | `hpRegen` 2 + 0.5 | 0.55 |
| `pet_void_butterfly` | 5 | caster | epic | yes | flying | ranged / 5 / 1700 / `0xcc44cc` / arcane | `manaRegen` 1.5 + 0.35 | 0.45 |

Abilities (`:110-183`; all fields not listed are absent):

| pet | ability id | kind | unlock | cd ms | range | fields |
|---|---|---|---|---|---|---|
| sprite | **`sprite_heal_pulse`** 治疗脉冲 | heal | 0 | 12000 | 0 | value 0.08 |
| sprite | **`sprite_ley_ward`** 灵脉护佑 | shield | 1 | 20000 | 0 | value 0.2, durationMs 5000 |
| owl | `owl_moon_mark` | mark | 0 | 10000 | 8 | value 0.15, durationMs 6000 |
| owl | `owl_talon_dive` | strike | 1 | 9000 | 7 | damage 2.2, leap |
| storm_wolf | `wolf_pounce` | strike | 0 | 8000 | 5 | damage 1.5, leap, bleed 0.25, durationMs 4000 |
| storm_wolf | `wolf_moon_howl` | buff | 1 | 22000 | 0 | value 0.15, durationMs 6000 |
| cat | `cat_backstab` | strike | 0 | 7000 | 1.6 | damage 2.2, crit |
| cat | `cat_shadow_flurry` | strike | 1 | 12000 | 1.6 | damage 0.9, hits 3 |
| jade_tortoise | `tortoise_taunt` | taunt | 0 | 14000 | 0 | radius 4, value 0.25, durationMs 5000 |
| jade_tortoise | `tortoise_quake` | nova | 1 | 12000 | 1.8 | damage 1.0, radius 2.5, stunMs 1200, self |
| dragon | `dragon_fire_breath` | cone | 0 | 9000 | 3.5 | damage 1.3, radius 4, arc π/3, fire, burn 0.15, durationMs 3000 |
| dragon | `dragon_magma_orb` | bolt | 1 | 8000 | 6 | damage 2.0, fire, radius 1.5 |
| phoenix | `phoenix_ember_mend` | heal | 0 | 14000 | 0 | value 0.12 |
| phoenix | `phoenix_rekindle` | revive | 0 | 0 | 0 | value 0.4 |
| phoenix | `phoenix_flame_ring` | nova | 1 | 13000 | 2.5 | damage 1.2, radius 2.5, fire, burn 0.12, durationMs 3000 |
| void_butterfly | `butterfly_void_bolt` | bolt | 0 | 6000 | 6 | damage 1.5, arcane, mana 0.05 |
| void_butterfly | `butterfly_void_rift` | nova | 1 | 12000 | 6 | damage 1.1, radius 2.2, arcane, slow 35, durationMs 3000 |

Store `arc` as a number in JSON (`π/3 = 1.0471975511965976`). Strings come from i18n keys only
(`src/i18n/locales/pets.ts`): `data.pet.<id>.name/.desc/.origin`, `data.pet.ability.<abilityId>.name/.desc`,
`data.pet.role.<role>`, `data.pet.stat.<stat>` (e.g. `经验 +{value}%`), `ui.pet.chapter.<n>`. Ch1 strings: `pet_sprite`
= 灵脉精灵 / Ley Sprite; origin 「第一章 · 支线「捉迷藏的小精灵」」; abilities 治疗脉冲 / Healing Pulse and 灵脉护佑 / Ley Ward
(`pets.ts:8-10, 34, 43, 53-56` zh, `:151-153, 177, 186, 196-199` en).

### 18.2 State and save
`PetInstance { petId, level 1..20, exp ≥ 0, evolved 0|1|2, bond 0..5, bondProgress 0..99 }`. `PetSystem` state is
`pets: PetInstance[]` (in acquisition order) plus `activePet: string|null` and a transient `activeMs` (not saved).
Save shape: `SaveData.pets = { owned: PetInstance[], active: string|null }` (`PetSystem.toSave`, `:406-408`). Load and
legacy migration (`migratePetSave`, `:435-456`) are specified in `save-ui-input.md` §3.3 (`:365-372`). Summary: drop
unknown or duplicate ids; clamp level to 1..20; clamp exp to `0..expToNext−1` (0 at level 20); `evolved = max(stage for
level, clamped saved value)`; clamp bond to 0..5 and bondProgress to 0..99; `active` is kept only if that pet is owned.
Note that `activeMs` resets on every load and on every `setActivePet`.

### 18.3 `PetSystem` (core, pure; `src/systems/PetSystem.ts`)
Constants (`:38-56`): `PET_DAMAGE_BASE_FRACTION 0.05`, `PET_DAMAGE_PER_LEVEL_FRACTION 0.005`, `PET_DAMAGE_MAX_FRACTION
0.15`, `BOND_PROGRESS_PER_LEVEL 100`, `BOND_PER_KILL 1`, `BOND_PER_ACTIVE_MINUTE 2`, `BOND_PER_FEED 20`, `FEED_EXP 120`,
`BOND_RESCUE_HP 0.3`, `BOND_RESCUE_COOLDOWN_MS 60000`, `BASE_BOND_CAP 3`.
```
petExpToNext(L)        = 60 + 40·L                                              // :59-61
petKillExp(mL)         = 10 + max(0, floor(mL))                                 // :64-66 (monster definition level)
evolutionForLevel(L)   = #{ t in [10, 20] : L >= t }                            // :69-73
bondMultiplier(b)      = 1 + 0.1·clamp(b, 0, 5)                                 // :75-77
petPassiveValue(def,p) = round( (base + perLevel·(max(1,L)−1)) · EVO[clamp(evolved,0,2)] · bondMultiplier(bond) · 10 ) / 10   // :80-84
petAttackDamage(D, p)  = raw = floor( D · min(0.15, 0.05 + 0.005·L) · EVO[clamp(evolved,0,2)] );  raw > 0 ? max(1, raw) : 0  // :87-95
leyFruitDropChance(e)  = e ? 0.12 : 0.015                                       // :98-100 (rolled on every kill, ZoneScene.ts:3884-3888)
getBondCap()           = min(5, 3 + max(0, level('pet_house')))                 // :249-251 — 月井; Ch1: 3
getExpMultiplier()     = 1 + 0.1·max(0, level('pet_house'))                     // :254-256 — Ch1: 1
mergeBonuses(a, b)     = key-wise sum                                           // :103-107
```
`round` is JS `Math.round` = `floor(x + 0.5)`. `level('pet_house')` comes from the homestead (`setBuildingLevelSource`);
the 月井 unlocks with Ch2's `q_seal_dark_source` (`src/data/homestead.ts:36-41`), so in Ch1 it is 0. `isAway(id)` comes
from the tower expedition (`setAwaySource`), which is always false in Ch1.

Operations:
* `addPet(id, {silent})` (`:268-282`). Unknown id → `false`. Already owned → log `sys.pet.duplicate` (unless silent),
  `false`. Otherwise append `{id, 1, 0, 0, 0, 0}`; if `activePet == null` it becomes active; log
  `sys.pet.obtained {name}` (system, unless silent); emit `PET_OBTAINED {petId, silent}` then `PET_CHANGED {petId}`;
  return `true`.
* `setActivePet(id|null)` (`:284-290`). If id is not owned or is away → no-op. If id equals the current active → no-op.
  Otherwise set it, `activeMs = 0`, emit `PET_CHANGED`.
* `addExp(id, amount, {silent})` (`:293-319`). If not owned, `amount <= 0` or level ≥ 20 → return 0. Then
  `exp += floor(amount)`; while `level < 20 and exp >= petExpToNext(level)`: `exp −= next; level++`; log
  `sys.pet.levelUp {name: displayName, level}` (unless silent); if `evolutionForLevel(level) > evolved`: `before =
  displayName; evolved = stage`; log `sys.pet.evolved {name: before, evolvedName: displayName}` (always, even when
  silent). After the loop, if level ≥ 20 then `exp = 0`. If any level was gained, emit `PET_CHANGED`. Return levels gained.
* `addBond(id, progress)` (`:322-341`). If not owned or `progress <= 0` → 0. `cap = getBondCap()`. If `bond >= cap`:
  set `bondProgress = 0`, return 0. Otherwise `bondProgress += progress`; while `bond < cap and bondProgress >= 100`:
  `bondProgress −= 100; bond++`; log `sys.pet.bondUp {name, bond}` (each step). Then if `bond >= cap`, set
  `bondProgress = 0`. Floor `bondProgress`. Emit `PET_CHANGED` if any bond level was gained.
* `onKill(monsterLevel)` (`:344-352`), called for **every** monster death processed by `ZoneScene.onMonsterKilled`
  (hero, skill, DoT, mercenary or pet kill; `ZoneScene.ts:3805`):
  1. `exp = petKillExp(mL) × getExpMultiplier()`.
  2. If there is an active pet: `addExp(active, exp)` and `addBond(active, 1)`.
  3. If `level('pet_house') = w > 0`: `grantRestingExp(floor(petKillExp(mL) × (0.2 + 0.1·(w−1))))`. This gives that
     exp, silently, to every owned non-active pet.

  This does **not** depend on the companion being spawned, alive or near.
* `tickActive(dtMs)` (`:364-371`): if there is an active pet, `activeMs += dt`; every whole 60000 ms →
  `addBond(active, 2)`. It is called **only** from `PetCompanion.update` (§18.5.2: not paused, hero outside safe zones).
* `canFeed(id)` = owned and (`level < 20 or bond < cap`) (`:373-377`). `feedPet(id)` (`:380-388`): if not `canFeed` →
  `false`. Otherwise log `sys.pet.fed {name}`, `addExp(id, 120 × getExpMultiplier())`, `addBond(id, 20)`, emit
  `PET_CHANGED`, return `true`. The caller removes the fruit (§18.8).
* `getBonuses()` (`:391-396`) = `{ [def.passive.stat]: petPassiveValue(def, active) }` for the active pet, else `{}`.
* `calculatePetDamage(D)` = `petAttackDamage(D, active)` or 0 (`:399-402`).
* `getPetDisplayName(p)`: the name, or `sys.pet.evoName.1` 「{name}·觉醒」 / `.2` 「{name}·至尊」 by `evolved`
  (`:241-246`). The name falls back to the id when the key is missing.

Cumulative exp to **reach** each level (Σ `petExpToNext`):

| L | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| exp | 0 | 100 | 240 | 420 | 640 | 900 | 1200 | 1540 | 1920 | **2340** | 2800 | 3300 | 3840 | 4420 | 5040 | 5700 | 6400 | 7140 | 7920 | **8740** |

### 18.4 How the active beast feeds the hero (`ZoneScene`)
* **Equipment stat cache** (`getEquipStats`, `ZoneScene.ts:3143-3172`, pet part `:3154-3157`): after achievement bonuses,
  each `getBonuses()` entry is added to `EquipStats[stat]` when the key exists, **except `expBonus` and `magicFind`**.
  Those two are applied at the kill instead. `PET_CHANGED` invalidates the cache (`:673`). All other passive stats
  (`attackSpeed`, `critRate`, `defense`, `damagePercent`, `hpRegen`, `manaRegen`) are `EquipStats` keys and work like
  gear (classes spec §3).
* **Kill** (`onMonsterKilled`, `:3789-3805`; combat spec §13.1) — `homeBonus = mergeBonuses(homestead.getTotalBonuses(),
  pets.getBonuses())`; `exp = floor(def.expReward × (1 + homeBonus.expBonus/100 + eq.expBonus/100))`; then gold; then
  `pets.onKill(def.level)`; … later `luckBonus = hero.lck + homeBonus.magicFind + …` feeds loot (loot spec §6).
  In Ch1 `homestead.getTotalBonuses()` holds at most `potionDiscount` (§4.6), so the pet is the only `expBonus`
  source besides gear.
* The passive is active **whenever `activePet != null`**. This holds while the companion is exhausted, while the
  hero is in a safe zone, while the hero is dead, and even if no companion exists. Setting the pet to rest
  (`active = null`) removes it. It never applies to quest-reward exp, dialogue or lore exp, or other non-kill exp
  (§4.1 pays `reward.exp` raw).
* Kill **credit**: a monster killed by the pet goes through the same `onMonsterKilled`. That means hero exp/gold,
  loot, quest `kill` progress, story `monster_killed`, embers, the ley-fruit roll, spirit `kill`, achievements and pet
  exp all apply. If the dead monster was the hero's `attackTarget`, that target is cleared and `TARGET_CHANGED {null}`
  is emitted (`ZoneScene.ts:7152-7158`).

### 18.5 `PetCompanion` — the beast in the world (`src/systems/PetCompanion.ts`)
Port split: the **decision and combat logic is core** (`PetCompanionSim`, deterministic given the RNG stream). UE
only renders and plays animations (`APetActor`). Positions are float tiles. Delays that the web schedules with
`scene.time.delayedCall` become core timer events, the same way the combat spec handles contact beats (combat spec
§10).

#### 18.5.1 Lifecycle and per-visit state
* One instance per zone visit. It is created in `ZoneScene.create` (`:511`, `createPetCompanion` `:7133-7161`) and
  destroyed on shutdown (`:7187-7188`). It also exists when no beast is owned (then it does nothing). Sub-zone,
  dungeon and tower entries are zone visits too.
* Per-instance state, **reset on every zone visit**: `col,row`, `hp`, `maxHp`, `exhaustedUntil 0`, `readyAt {}` (so
  every ability is ready on entry), `basicReadyAt 0`, `lockedUntil 0`, `taunts {monsterId → until}`, `marks {monster →
  until}`, `reviveUsed false` ("once per zone"), `lastRescueAt −∞`.
* Hero death in the overworld does **not** recreate it: the hero respawns at `camps[0]` in place
  (`ZoneScene.ts:946-949`) and the beast teleports to the hero by the follow rule (> 16 tiles). Death in a
  dungeon/sub-dungeon restarts the scene, which creates a new instance.
* Host inputs (`:31-48`, wired at `ZoneScene.ts:7135-7160`):
  * `isWalkable(c,r)` = `collisions[r][c]` truthy.
  * `monstersNear(c,r,rad)` = spatial grid, Euclidean `dx²+dy² <= rad²` (`SpatialGrid.ts:117-139`).
  * `heroDamage()` = `hero.baseDamage + EquipStats.damage`.
  * `inSafeZone(c,r)` = `∃ camp: dist² < r²` with `r = mapData.safeZoneRadius ?? 9`. Ch1 camps are (15,15) and
    (95,100), r = 9.
  * `isPaused()` = `isTransitioning || storyDirector.cinematic`.

#### 18.5.2 Per-frame update — `update(now, dt)` (`:162-239`), called from `ZoneScene.update` after hero/merc combat
(`:1480-1483`; the whole `ZoneScene.update` is skipped while a cinematic, abyss tier pick or boon modal is up, `:1347-1350`)
```
def, inst = active beast; if none: despawn visual; return
if no visual or shown pet/stage changed: spawn(def, inst.evolved)            // §18.5.3
pruneMarks(now)
maxHp = max(1, round(hero.maxHp · def.hpFraction)); hp = min(hp, maxHp)     // follows hero max HP every frame
if isPaused(): tick animation only; return
exhausted = now < exhaustedUntil
if !exhausted and exhaustedUntil > 0 (still faded): recover()                // hp = maxHp, exhaustedUntil = 0, opaque, heal burst
peaceful = inSafeZone(hero tile)
if !peaceful: pets.tickActive(dt)                                            // bond per active minute (also while exhausted)
if !exhausted: hp = min(maxHp, hp + maxHp · (peaceful ? 0.08 : 0.01) · dt/1000)
target = peaceful ? null : pickTarget()
heroDist = |hero − pet|;  heroHpRatio = hero.maxHp > 0 ? hero.hp/hero.maxHp : 1
if !exhausted and shouldBondRescue(inst.bond, heroHpRatio, now, lastRescueAt):   // bond 5, 0 < ratio < 0.3, 60 s cd
    sig = primaryAbility(def); if sig: lastRescueAt = now; log sys.pet.rescue (combat); useAbility(sig, target, forced)
action = now >= lockedUntil ? choosePetAction(ctx) : rest
switch action: ability → useAbility; attack → basicAttack(target);
               approach → moveToward(target, stopAt = max(0.8, 0.85·range), speed 4.2·1.4 = 5.88 t/s);
               follow → follow(exhausted); rest → exhausted ? follow(true) : (now >= lockedUntil ? idle : keep pose)
if target and now >= lockedUntil and action != follow: face target
sync visual, label, animation
```
`ctx` for `choosePetAction`:
* `abilities = unlockedAbilities(def, evolved)`.
* `heroAttackers` = alive monsters in state `attack` within 4 tiles of the hero.
* `targetMarked` = this beast's mark on the target is still running.
* `enemiesNearTarget` = alive monsters within 3 tiles of the target (the target itself included).
* `basicRange = def.combat.range`, leash 11.

#### 18.5.3 Spawn (`buildVisual`, `:250-320`)
* Position: `(hero.col − 1.2, hero.row + 1.2)`. If `round()` of that tile is not walkable, use the hero tile (this check
  is made for flyers too).
* `hp = round(hero.maxHp × hpFraction)`, i.e. full.
* Web detail: `buildVisual` first calls `destroyVisual`, which sets the shown stage to −1. So the "keep position on
  evolution" branch never runs: **every evolution and every `refresh()` (`ZoneScene.spawnPetSprite`, `:7163-7165`)
  re-spawns the beast beside the hero at full HP** (QUIRK QP2).

#### 18.5.4 Targeting (`:417-440`)
1. The hero's `attackTarget`, if alive and within 10 tiles **of the hero**.
2. Otherwise the alive monster nearest the hero, among those within 7 tiles of the hero whose state is `chase` or
   `attack` (aggro).
3. Otherwise none.

Monster aggro depends only on the hero's distance (monsters spec), so pet hits never aggro a monster. The beast can
shoot an idle monster that the hero has selected.

#### 18.5.5 Decision — `choosePetAction(ctx)` (pure, `PetSystem.ts:146-187`)
```
if exhausted: rest
if peaceful or heroDist > (leash ?? 11): follow
ready = abilities where kind != revive and (readyAt[id] ?? 0) <= now           // array order
if heroHpRatio < 0.5: first a in ready with kind ∈ {heal, shield, taunt} and useful(a) → ability a
first a in ready with useful(a) → ability a
if no target: follow
if targetDist <= basicRange: basicReadyAt <= now ? attack : rest               // rest = stand and wait (no follow)
approach
useful(a), with inRange = target and targetDist <= max(a.range, 0.5):
  heal: heroHpRatio < 0.7 | shield: heroAttackers > 0 and heroHpRatio < 0.85 | taunt: heroAttackers > 0
  buff: target and targetDist <= 8 | mark: inRange and !targetMarked | strike, bolt: inRange
  cone, nova: inRange and enemiesNearTarget >= 1 | revive: never
```
Self-targeted kinds (heal, shield, taunt, buff) need no target. So the sprite heals a hurt hero who is walking in the
field with no monster around, but never inside a camp's safe zone and never beyond the 11-tile leash.
`bondRescue(bond, r, now, last)` = `bond >= 5 and 0 < r < 0.3 and now − last >= 60000` (`:190-193`). This is
unreachable in Ch1 because the bond cap is 3.

#### 18.5.6 Movement (`:375-413`)
* `follow`: goal `(hero.col − 1.3, hero.row + 1.3)`. If the distance to the goal is > 16 tiles, teleport onto the hero
  tile. Otherwise move toward the goal with `stopAt 0.35` at speed 8.5 tiles/s (distance > 4) or 4.2 tiles/s, × 0.7
  while exhausted.
* `moveToward(goal, stopAt, speed)`: if already within `stopAt` → idle (when not locked). Otherwise
  `step = min(d − stopAt, speed·dt/1000)` along the straight line. Flyers always move. Walkers move only if the rounded
  new tile is walkable, else try x only, else y only, else, if d > 6, jump to the hero tile. Then face the move
  direction and play walk/fly.
* No pathfinding. `pet_sprite` flies, so in Ch1 it crosses water and walls.
* 3D: the grid offset (−1.3, +1.3) is "screen-left of the hero" under the web's iso projection. Keep the grid offset
  in core; with the world spec's fixed camera yaw it reads the same way.

#### 18.5.7 Basic attack (`basicAttack`, `:524-548`)
* `basicReadyAt = now + combat.attackMs`; face the target.
* **Ranged** (Ch1). Play cast. `release = round(castDuration × 0.46)` (frame-sheet cast, `CharacterAnimator.ts:949-950,
  990`). For the `flying` anim category `castDuration = 350`, so **161 ms** (`:114, 164-170`). Then
  `lockedUntil = now + release + 60`.
  * At release, if the target is alive: launch the monster-ranged projectile from the pet (8 px above its anchor)
    toward the target. Flight time is `clamp(dist_px × 2, 200, 500)` ms (combat spec §5.2;
    `SkillEffectSystem.ts:183-212`; port `dist_px = tileDist × 36`). Colour `0x8ff0c0` falls into the "shadow" palette
    because it is neither fire-like nor frost-like (render).
  * On arrival, if the target is alive: `hit(target, 1)`.
* **Melee** (Ch2+): play attack; `contact` per combat spec §10.1 for the beast's sheet; `lockedUntil = now + contact +
  80`. At contact, if the target is alive: `hit(target, 1)` plus slash VFX.

#### 18.5.8 Damage dealt — `hit(target, mult, {crit, element, color})` (`:453-471`)
```
if target dead: 0
base = petAttackDamage(heroDamage(), active);  if base <= 0: return 0         // nothing lands, no number, no VFX
isCrit = opts.crit or rand() < 0.08
amp = this beast's mark on target active ? max(mark buff values) : 0
dmg = max(1, round(base · mult · (isCrit ? 1.6 : 1) · (1 + amp)))
weight = Monster.takeDamage(dmg, petScreenPos, {isCrit})     // raw HP loss: no CombatSystem, no defense/resist/dodge/elite mods
damage number at the target (element colour unless physical); impact burst unless weight == tick (kill → heavy);
pet hit-freeze round(0.6 · HIT_PROFILES[weight].attackerStopMs)
if target died: host.onMonsterKilled(target)                 // §18.4 kill credit
```
Ch1 consequence (pet Lv1, fraction 0.055): a hero whose `baseDamage + gear damage` is below 1/0.055 ≈ 18.18 makes the sprite deal
**0**. The bolt still flies but nothing lands. Example: a mage at Lv3 without a +damage weapon has 17.2. From pet Lv2
the fraction is 0.06 (≥ 16.67 needed). A warrior at Lv3 (23.6) gets 1, 2 on a crit (QUIRK QP3).

#### 18.5.9 Stray swings and taunts — `interceptMonsterAttack(monster, now)` (`:120-135`)
Called inside the hero's monster loop (`handleCombat`, `ZoneScene.ts:2817-2830`). It runs for each alive monster in
state `attack`, within 12 tiles of the hero, not immobilized, whose swing cooldown is ready, **before** the swing is
aimed at the hero:
```
if no pet visual or pet exhausted or no active pet: false
redirect = taunts[monster.id] > now
if !redirect:
    reach = def.attackRange + 0.5; dPet = |monster − pet|; dHero = |monster − hero|
    redirect = dPet <= reach and dPet < dHero and rand() < 0.25         // rand drawn only when both distance tests pass
if !redirect: false                                                      // the hero is attacked normally
monster.lastAttackTime = now
contact = monster.playAttack(toward pet)                                 // wind-up per monsters spec
at contact: takeHit(monster)
true                                                                     // the hero is not attacked by this swing
```
`takeHit` (`:479-491`): ignored if the pet is gone, the monster is dead or the pet is exhausted. Otherwise
`dmg = max(1, round(def.damage × (0.85 + rand()·0.3) × 0.8))` (`def.damage` as stored on the monster, so already
difficulty- and elite-affix-scaled, `Monster.ts:389-403`; no defense, dodge or
reach re-check; ranged monsters hit the pet at contact without a projectile); `hp −= dmg`; damage number on the pet;
monster-hit VFX; white flash 70 ms and hurt recoil 0.6. If `hp <= 0` → `exhaust()`.

Ch1 numbers (base data, normal difficulty, no damage affix):

| monster | dmg to pet |
|---|---|
| slime | 3–5 |
| goblin | 5–7 |
| chief | 10–13 |
| shaman | 11–15 |

With no taunt in Ch1, "25 % of swings" means: 25 % of the ready swings of monsters that are within
`attackRange + 0.5` of the beast **and** closer to it than to the hero.

#### 18.5.10 Exhaustion (`:493-510`) — beasts never die
* `exhaust()`: `hp = 0`, `exhaustedUntil = now + 5000` (`PET_EXHAUST_MS`), `lockedUntil = 0`, all taunts end, render
  at 45 % opacity, log `sys.pet.exhausted {name}` (combat).
* While exhausted:
  * The action is `rest`, which follows the hero at 0.7× speed.
  * No regen, no interception, incoming hits are ignored.
  * The label gets ` [疲惫]` (`zone.pet.exhaustedTag`).
  * The passive and `onKill` growth are unaffected.
* On the first update after expiry, `recover()`: full HP, opaque, small heal burst.

#### 18.5.11 Abilities — `useAbility(a, target, now, def, forced)` (`:566-745`)
Common:
* `readyAt[a.id] = now + cooldownMs`, set at **decision** time.
* `castMs` = the cast release (161 ms for flyers, §18.5.7), facing the target if any.
* `lockedUntil = now + castMs + 120`.
* The element is `a.element ?? combat.element`. VFX colour: fire `0xff7a2a`, arcane `0xcc44cc`, else `combat.color`.
* Effects resolve at `now + castMs` (for strike: per hit).
* `forced` (bond rescue) also sets `lockedUntil ≥ now + 400`.

| kind | effect at release |
|---|---|
| **heal** (Ch1) | `amount = max(1, floor(hero.maxHp × value))`; `hero.hp = min(maxHp, hp + amount)`; emit `PLAYER_HEALTH_CHANGED`; VFX `life_regen` + heal burst (10) on the hero; float text `+N` `#7dff9a`. Not re-checked at release (see QP1). |
| **shield** (Ch1 from 觉醒) | hero buff `{stat: damageReduction, value, duration, tag: petShield}`, replacing any `petShield` buff; VFX `shield_wall` on the hero. DR stacks with other DR, capped at 0.9 (classes spec §11). |
| buff | hero buff `damageBonus` `value` for `durationMs`, tag `petHowl`. |
| taunt | every alive monster within `radius` of the pet's rounded tile gets `taunts[id] = now + durationMs`; plus a hero `damageReduction` `value` buff with tag `petShield`. |
| mark | if the target is alive: remove any previous `petMark` buff on it, push monster buff `{damageAmplify, value, durationMs, tag petMark}`, `marks[target] = now + durationMs`. Expired or dead marks are pruned each update. |
| strike | optional leap: tween the pet to 0.9 tiles short of the target over `max(120, castMs)` ms (Quad.easeOut). `hits` hits at `castMs + i·140` ms, each `hit(target, damage, {crit})`. `bleed` applies status `bleed` `max(1, round(dmg × bleed))` for `durationMs ?? 4000`, source `pet`. `lockedUntil = now + castMs + hits·140 + 120`. |
| bolt | projectile as in §18.5.7. On arrival: if `radius`, every other alive monster within `radius` of the target takes `hit(m, damage × 0.5)` (+ combustion VFX). The target takes `hit(target, damage)`. If `mana`: `hero.mana += max(1, floor(maxMana × mana))` (capped), float `+N` `#6fb6ff`. |
| cone | origin is the pet. Every alive monster within `radius` (length) of the pet: hit if distance ≤ 0.6, or if the angle to the aim (pet→target) is ≤ `arc/2`. `burn` applies `max(1, round(dmg × burn))` for `durationMs ?? 3000`. |
| nova | the centre is the pet if `self` or there is no target, else the target. Every alive monster within `radius` gets `hit(m, damage)`. If it survives: `stunMs` → status `stun`; `slow` → `slow` (`slow` %, `durationMs ?? 3000`); `burn` as for cone. |
| revive | passive. `tryReviveHero()` (`:141-160`), called by `killPlayer` before the hero dies (`ZoneScene.ts:7171-7175`): once per zone visit, `hero.hp = max(1, floor(maxHp × value))`, log `sys.pet.revive`, and the hero does not die. |

Status application uses `StatusEffectSystem.apply(id, type, value, duration, 'pet', now)` (classes spec §13.2). Hero
buffs use the generic `ActiveBuff` (classes spec §1.6/§11).

#### 18.5.12 Constants (`tuning/pets.json`)
* Beast: `exhaustMs 5000`, `straySwingChance 0.25`, `straySwingReachPad 0.5`, `followSpeed 4.2`, `dashSpeed 8.5`,
  `dashOver 4`, `teleportDist 16`, `followOffset (−1.3, +1.3)`, `followStop 0.35`, `spawnOffset (−1.2, +1.2)`,
  `approachSpeedMul 1.4`, `approachStop max(0.8, 0.85·range)`, `exhaustedSpeedMul 0.7`, `regenPeacefulPerSec 0.08`,
  `regenFieldPerSec 0.01`, `leash 11`, `targetKeepRange 10`, `targetScanRange 7`, `attackersRange 4`,
  `nearTargetRange 3`.
* Hits: `petCritChance 0.08`, `petCritMul 1.6`, `takeHitMul 0.8`, `takeHitJitter [0.85, 1.15]`.
* Timing: `lockAfterRanged 60`, `lockAfterMelee 80`, `lockAfterAbility 120`, `strikeHitSpacingMs 140`,
  `forcedLockMs 400`, `hitFreezeScale 0.6`.
* Plus the `PetSystem` constants of §18.3.

### 18.6 `pet_sprite` in Chapter 1 — worked numbers
* **When**: `q_pet_sprite_friend` (side, Lv3, 4 clues NE, §10.1). The turn-in happens at the elder in camp (15,15),
  inside the safe zone, so the beast appears beside the hero and only follows until the hero leaves the camp.
* **HP**: `round(0.45 × hero maxHp)`. Warrior Lv3 (180) → 81. Mage/rogue Lv3 (140) → 63. Warrior Lv10 (285) → 128.
* **Kill exp to the beast**: slime 11, goblin and hunt leaders 13, chief 15, shaman 16 (`level` 1/3/5/6). Bond +1 per
  kill, +2 per active minute outside camps, +20 per fruit, capped at **3** (no 月井).
* **觉醒 (Lv10, 2340 cumulative exp) is reachable in Ch1**: about 180 goblin or 213 slime kills after obtaining it,
  minus 120 per fruit fed. Nothing caps it in Ch1, so the build must support stage 1: the 灵脉护佑 shield, the
  passive ×1.5, the 「·觉醒」 name, and the stage-1 look with aura (§18.9). 至尊 (Lv20, 8740) is out of reach in
  practice but is not blocked.
* **Passive `expBonus`** (rounded to 0.1): Lv1 bond0 3.0; Lv5 bond3 6.0; Lv9 bond3 8.1; Lv10 (觉醒) bond3 12.9.
  * Kill exp is floored, so at Lv1 the +3 % is invisible on slimes (12 → 12) and goblins (18 → 18). It shows on the
    chief (55 → 56) and the shaman (90 → 92).
* **Heal**: `floor(0.08 × maxHp)`, e.g. 14 for a 180-HP warrior. It fires when the hero is < 70 % HP, out of camp,
  within 11 tiles, off cooldown (12 s, ready on every zone entry).
* **Damage**: §18.5.8; typical Ch1 hits are 0–4 (crit ×1.6). The sprite is a support: its value is the heal, the
  soaked swings, the exp bonus and the occasional kill credit.
* **Ley Fruit sources in Ch1**:
  * The kill roll: 1.5 % per kill, 12 % on elites (chief, shaman, hunt leaders). It is rolled whether or not a beast
    is owned.
  * The consumable loot pool (loot spec §6, L1–4 pool includes `c_ley_fruit`).
  * Herb-garden stock (§4.7.1), which cannot be harvested in Ch1.
  * Item: stackable 20, sells for 12 (`src/data/items/bases.ts:96`).

### 18.7 Event flow — turning in `q_pet_sprite_friend` (exact web order)
1. `turnInQuest` (§4.1): the status changes and `QUEST_TURNED_IN` fires (embers +2 via the listener). Then exp +220,
   gold +50, autosave not yet.
2. `pets.addPet('pet_sprite')` (`ZoneScene.ts:4130`):
   * Log 「灵兽 灵脉精灵 加入了你!」 / "The ley-beast Ley Sprite has joined you!" (`sys.pet.obtained`, type system).
   * Emit `PET_OBTAINED {petId, silent: false}`. **The web has no listener for it**: no toast, sound, popup or
     cutscene (`src/utils/EventBus.ts:50`; QP5).
   * Emit `PET_CHANGED`: the equip-stat cache is invalidated, and the pet panel refreshes the HUD medallion, which
     becomes visible with the portrait (desktop, `PetPanel.ts:144-166`).
   * It is the first beast, so it becomes active and the passive applies from the next kill.
3. `achievements.update('quest')`, `autoSave()`; the save now contains `pets {owned:[{pet_sprite,1,0,0,0,0}], active:
   'pet_sprite'}`.
4. Next frame: `PetCompanion.update` spawns the beast at (hero −1.2, +1.2) with full HP. It is peaceful in camp.
5. Quest card: the reward summary on **both** the offer and the turn-in card includes 「宠物」 / "Pet" with no name
   (`sys.questCard.rewardPet`, `QuestCardUI.ts:188-200`). The quest has no story trigger (`script.ts` has none for it).

### 18.8 UI (render contracts; core exposes view-models)
* **Overhead label and HP bar** (`:311-350`). The text is `"{displayName} Lv.{level}"` plus ` [疲惫]` when
  exhausted, 9 px, `#aaddff`, black stroke. Above it sits a 24×3 px bar shown only when `hp/maxHp < 0.999`; fill green
  `0x6fd35a` above 50 %, orange `0xf39c12` above 25 %, else red `0xe74c3c`. 3D: a world-space widget above the
  pet, same rules.
* **Float texts**: heal `+N` `#7dff9a` on the hero; mana `+N` `#6fb6ff`; damage numbers through the normal
  damage-text path (combat spec §12).
* **HUD medallion** (desktop only, `PetPanel.ts:114-166`, `UIScene.ts:5681-5684`): a 34 px medallion left of the
  minimap with the active beast's portrait and a "P" badge. It is hidden until a beast is owned and has no portrait
  while all beasts rest. Click toggles the panel.
* **P panel** (`PetPanel.ts:168-442`; opened by key `P` `ZoneScene.ts:639, 2195`, the touch panel menu
  `MobileControlsSystem.ts:470`, the companion panel button `UIScene.ts:4955-4974`, or the medallion). It is 660×540
  at (310, 10).
  * Header: `ui.pet.owned {count}/{total = 8}` and a fruit counter `× N` (all `c_ley_fruit` stacks in the bag).
  * List (left, 208 px): all 8 beasts in this order: the active one, then the other owned ones in acquisition order,
    then the unowned ones in data order. Unowned beasts show as a dark silhouette with 「？？？」 and
    `ui.pet.chapter.<n>`.
  * Detail (right):
    * Portrait, display name in the rarity colour, `role · ui.pet.evo.<stage>` (幼体/觉醒/至尊), and the description
      (the origin if not owned).
    * Level/exp bar `exp/need` and 「{n} 级进化」 for the next threshold.
    * Five bond pips; pips above the cap are dimmed. The note is 「上限 {cap}（月井提升）」 while `cap < 5`.
    * The passive (`data.pet.stat.<stat>` with the value).
    * Ability cards, locked ones marked 「🔒 觉醒后解锁」.
  * Buttons: **出战 / 休息** (`setActivePet(id | null)`) and **喂食 ({count})**.
  * With no beast owned the detail shows `ui.pet.none`.
  * Feed (`:424-442`):
    * No fruit → log `sys.pet.noFruit`.
    * `!canFeed` → log `sys.pet.feedFull`.
    * Otherwise remove 1 from the first fruit stack in bag order, then `feedPet`, emit `INVENTORY_CHANGED`, rebuild.
    * The button is disabled when there is no fruit or `!canFeed`.
  * Feeding works anywhere; it does not need the tower.
* **Bag**: the bag context menu offers 「使用」 on `c_ley_fruit`, but `useConsumable` returns null and nothing happens
  (loot spec §7.4, QP6).
* 3D/UMG: same data and actions. On touch, the panel is reached from the panel menu (no medallion), as in the web.

### 18.9 Art and audio for Chapter 1 (supersedes `art-inventory-ch1.md` §13 for this beast)
* **`SK_Pet_Sprite`** (Blender → UE skeletal mesh), web design `src/graphics/sprites/pets/Sprite.ts`:
  * Body: a glowing green-gold "seed" body with a big-eyed face, a two-leaf sprout on the head, four dragonfly wings
    and a wisp tail trailing light. Skin `0xc8f4b0` / `0xd0f8b8` / `0xe0fcc8` (stage 0/1/2), tail `0x8ae07a` /
    `0x9ae880` / `0xb0f090`, leaf `0x5ab84a`, head glow `0xc8ff90`, aura `0xb8f070`.
  * Size: stage scales 1.15 / 1.3 / 1.46. In the web's 96-unit rig frame the creature is 42 units tall (top) with its
    body centre hovering 22 units up (`pets/index.ts:46`). Size it against the hero rig with the art spec's unit rule.
  * **Stage 0 and stage 1 are required for Ch1.** Stage 1 adds a five-leaf crown and 2 orbiting light motes. Stage 2
    (blossom crown, gold-veined wings, halo, 3 motes) can wait.
  * Animations: idle (hover bob), fly/walk, attack, cast (release at 161 ms from start, §18.5.7), hurt. In the web
    these are idle 4f@6, walk 6f@10, attack 4f@12, cast 4f@10, hurt 2f@10 (`PetKit.ts:44-67`). Real 3D needs no se/ne
    views.
  * Ground shadow: a small blob (flyer). Exhausted state: 45 % opacity, e.g. a dithered/translucent material
    parameter. Evolved stage: an additive aura in `combat.color` pulsing α 0.2↔0.45 over 1400 ms (`:301-309`).
* **VFX**:
  * **NEW `NS_Pet_Bolt`.** No Ch1 monster is ranged, so the art spec has no asset for this yet. The web uses
    `playMonsterRangedAttack` with the `shadow` palette (`FxKit.ts:12`: core `#F0DCFF`, mid `#B478FF`, rim
    `#7030C0`), so the sprite's bolt is **purple** even though the beast's colour is mint:
    * Launch: glow (rim) 22 px, 140 ms.
    * Flight: travelling glow (rim) + comet (mid) + white core for the flight time.
    * Trail: every 7 px, a mid-colour glow mote with ±3 px jitter, 260 ms, drifting up.
    * Arrival: flash (rim) 18 px 150 ms, 7 sparks (mid, 70–150 px/s), ring 6→22 px 260 ms
      (`SkillEffectSystem.ts:183-212`).
    * Then the hit's impact burst in mint `0x8ff0c0` (`PetCompanion.ts:466`).

    Parity is purple; tinting the bolt mint is an acceptable render-only change.
  * Heal: `life_regen` effect (art spec `:1105`) + heal burst on the hero.
  * Ward: the `shield_wall` effect (art spec `:1098`) on the hero.
  * Impact bursts per hit weight.
  * Recover: a 6-particle heal burst on the pet.
* **Audio**: the web plays no pet-specific sound (no pet hooks in `src/systems/audio/`). Optional new cues are a
  deviation for the audio spec.

### 18.10 Degraded contract if the companion is cut from the Ch1 build
Apply this only if OQ3 is overridden. It keeps the save identical to what the web (and the later milestone) produces:
1. **Ownership**: `addPet` exactly as §18.3, so `pet_sprite` is owned and **active**, and `SaveData.pets` is written.
2. **Passive exp bonus: yes, apply it.** It is a pure `PetSystem` rule (§18.4) with no companion dependency in the web.
3. **Pet growth: yes.** `onKill` exp and bond on every kill. `tickActive` (+2 bond/min) is normally driven by the
   companion. Drive it from the core sim tick with the same gate (not paused, hero outside safe zones) so bond
   matches the later build.
4. **`PET_OBTAINED`**: log `sys.pet.obtained` (system) as in the web. Like the web, no toast.
5. **Quest card**: keep the 「宠物」 reward line, because the reward is real (ownership is recorded).
6. **`c_ley_fruit`**: keep all drop rolls (they do not depend on ownership). Ship the P panel anyway (it only needs
   `PetSystem` + inventory) so fruit can be fed. If the panel is cut too, fruit is vendor-only (12 g) and 「使用」 stays
   a no-op.
7. **Not present**: no world beast, no heal, no stray swings, no pet kill credit, no shield. The hero takes every swing,
   so Ch1 is slightly harder than the web. Accept this or compensate in tuning, and record it as a parity deviation.
8. Text: the `.complete` line 「那小家伙赖在你肩头不肯走了」 then has no visual payoff. Consider a placeholder look
   (the stage-0 mesh hovering over the hero's shoulder, no AI) rather than nothing.

### 18.11 Core API proposal (C++20, no exceptions/RTTI)
```cpp
namespace abyss::pets {
enum class PetAbilityKind : uint8_t { Heal, Shield, Mark, Strike, Bolt, Cone, Nova, Taunt, Buff, Revive };
struct PetInstance { PetId id; uint8_t level = 1; int32_t exp = 0; uint8_t evolved = 0, bond = 0, bondProgress = 0; };
class PetSystem {                                     // pure; data from pets.json; events via EventSink
public:
  bool AddPet(PetId, bool silent = false);  void SetActive(std::optional<PetId>);
  int  AddExp(PetId, double amount, bool silent = false);  int AddBond(PetId, int progress);
  void OnKill(int monsterLevel);  void TickActive(uint32_t dtMs);
  bool CanFeed(PetId) const;  bool Feed(PetId);       // caller removes one c_ley_fruit first
  SmallMap<StatId, float> Bonuses() const;            // active beast passive
  int  AttackDamage(double heroDamage) const;  int BondCap() const;  double ExpMultiplier() const;
  void SetBuildingLevelSource(FunctionRef<int(BuildingId)>); void SetAwaySource(FunctionRef<bool(PetId)>);
  void Save(SaveWriter&) const; void Load(SaveReader&);   // migratePetSave rules (save spec §3.3)
};
PetAction ChoosePetAction(const PetDecisionContext&);   // pure, §18.5.5
bool ShouldBondRescue(int bond, double heroHpRatio, int64_t now, int64_t lastRescueAt);
class PetCompanionSim {                                 // one per zone visit
public:
  void Tick(int64_t now, uint32_t dtMs, CompanionWorld&, Rng&);       // §18.5.2; emits PetCommand (anim, projectile, vfx, text)
  bool InterceptMonsterSwing(MonsterId, int64_t now, CompanionWorld&, Rng&);   // §18.5.9
  bool TryReviveHero(HeroState&);                                      // §18.5.11 revive
  PetViewModel View() const;                          // pos, stage, hp/maxHp, exhausted, label
};
}
```
Events: `PetObtained{petId, silent}`, `PetChanged{petId}`, plus `LogMessage`. `PetChanged` must invalidate the
hero's derived-stat cache.

### 18.12 Quirks (pets)
| # | Quirk | Where | Recommendation |
|---|---|---|---|
| QP1 | The beast keeps acting while the hero is dead. Its heal checks only `hp/maxHp < 0.7`, so during the 1100 ms death window before the camp respawn it can set `hero.hp > 0`. Monster swings then resume (`handleCombat` only checks `hp > 0`), and a second death runs `die()` + death penalty again. Heal and ward also resolve at release without re-checking. | `PetCompanion.ts:162-239, 582-592`; `ZoneScene.ts:896-950` | **FIX**: when `hero.hp <= 0` the beast only follows (no abilities); skip heal resolution on a dead hero. |
| QP2 | Every evolution / `refresh()` re-spawns the beast beside the hero at full HP; the "keep position on evolution" branch is dead code. | `PetCompanion.ts:250-262, 322-335` | Keep (harmless), or keep position and HP on evolution (minor deviation). |
| QP3 | Pet damage bypasses `CombatSystem` (no defense/resist/elite modifiers) and is 0 while `heroDamage × fraction < 1`. Early Ch1 casters see a bolt that never lands. | `PetSystem.ts:87-95`, `PetCompanion.ts:453-459` | Keep the formula (parity). Optionally skip the cast when `AttackDamage() == 0` so no empty bolt flies. |
| QP4 | Cooldowns, pet HP and "once per zone" revive reset on every zone entry (new companion), so the heal is always ready after a transition. | `ZoneScene.ts:511, 7133-7134` | Keep. |
| QP5 | `PET_OBTAINED` has no listener: obtaining a beast is announced only by a log line and the medallion appearing. | `PetSystem.ts:279` | Keep parity, or add a toast in the achievement-toast style (OQ-P1). |
| QP6 | 「使用」 on `c_ley_fruit` in the bag does nothing; feeding is only in the P panel. | `InventorySystem.ts:407-432`, `UIScene.ts:4296-4305` | Keep parity by default, or route "Use" to "feed the active beast" (OQ-P2). |
| QP7 | `tickActive` accrues bond while the beast is exhausted and while the hero is dead (only "paused" and "in camp" stop it). | `PetCompanion.ts:176-177` | Keep. |

### 18.13 Open questions (pets)
| # | Question | Default if unanswered |
|---|---|---|
| OQ-P1 | Toast/fanfare on `PetObtained`? | No (parity); log line + medallion. |
| OQ-P2 | Make 「使用」 on Ley Fruit feed the active beast? | No (parity). |
| OQ-P3 | Fix QP1 (beast acting on a dead hero)? | Yes. |

### 18.14 Test vectors (pets)
1. `petExpToNext(1)=100, (9)=420, (19)=820`; cumulative table §18.3.
   `petKillExp(1)=11, (3)=13, (5)=15, (6)=16, (−2)=10`.
2. `addExp(L9 exp 400, 30)` → L10, exp 10, evolved 1. Logs in order: `levelUp{灵脉精灵, 10}`,
   `evolved{灵脉精灵 → 灵脉精灵·觉醒}`; one `PET_CHANGED`. `addExp(L19 exp 0, 5000)` → L20, exp 0.
   `addExp(L20, 50)` → 0, unchanged.
3. `petPassiveValue(sprite)`:
   * L1 bond0 → 3.
   * L5 bond3 → 6 (5.98 rounds to 6.0).
   * L9 bond3 → 8.1.
   * L10 evolved1 bond3 → 12.9.
   * L20 evolved2 bond5 → 31.8.
4. `petAttackDamage(D, L, evo)`:
   * (18, 1, 0) → 0.
   * (20, 1, 0) → 1.
   * (40, 5, 0) → 3.
   * (60, 10, 1) → 9.
   * (100, 20, 2) → 30.
5. Bond with cap 3:
   * bond 2, progress 95, `addBond(10)` → bond 3, progress 0, returns 1.
   * bond 3, `addBond(5)` → 0, progress 0.
   * `canFeed` at L20 bond 3 with cap 3 → false.
6. `feedPet` at L1, bond 0, progress 0 → L2, exp 20, bondProgress 20. Logs: `fed`, `levelUp`.
7. Kill exp with active `pet_sprite` L1, no gear exp:
   * slime 12 → 12.
   * goblin 18 → 18.
   * chief 55 → 56.
   * shaman 90 → 92.
   * Resting (`active = null`) → 12 / 18 / 55 / 90.
8. `choosePetAction`, sprite L1 (abilities `[heal]`), now 0, readyAt {}:
   * hero 0.65, not peaceful, heroDist 2 → ability heal.
   * Heal on cooldown, target at 4.0, basicReady → attack.
   * Target at 4.0, basic not ready → rest.
   * Target at 5.0 → approach.
   * No target → follow.
   * peaceful → follow.
   * heroDist 11.5 → follow.
   * exhausted → rest.
9. `choosePetAction`, sprite evolved 1 (abilities `[heal, shield]`), both ready:
   * hero 0.8, attackers 2 → shield.
   * hero 0.4, attackers 2 → heal.
   * hero 0.9, attackers 2 → no ability (basic or move).
10. Intercept, goblin (`attackRange 1.5`, reach 2.0):
    * dPet 1.0, dHero 1.4 → redirect iff rand < 0.25.
    * dPet 1.9, dHero 1.2 → never (no rand drawn).
    * dPet 2.1 → never.
    * Pet exhausted → never.
11. `takeHit` goblin (damage 8):
    * rand 0.5 → 6.
    * rand 0 → 5.
    * rand 0.999 → 7.
    * An 81-HP beast is exhausted by the 12th–17th such hit (ignoring regen), then recovers to full after 5000 ms.
12. Spawn: hero at (40.0, 50.0), (38.8, 51.2) → (39, 51) walkable → spawn there; not walkable → (40, 50).
    Follow from 20 tiles away → teleport onto the hero tile.
13. Save round-trip: `{owned:[{pet_sprite,7,300,0,2,40}], active:'pet_sprite'}` → identical.
    Exp 500 at L7 loads as 339 (`petExpToNext(7) − 1`). `{owned:[{pet_sprite,10,0,0,…}]}` loads `evolved 1`.
    Unknown `active` → `null`.
