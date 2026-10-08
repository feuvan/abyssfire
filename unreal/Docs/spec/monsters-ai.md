# Port Spec — Monsters, Monster AI, Spawning, Mini-bosses, Quest Hunts, Boss Intros

Area owner: monsters. Web source of truth: branch `claude/unreal-rebuild`, TypeScript under `src/`.
Target: portable C++20 core (`AbyssCore`, no UE types, no exceptions/RTTI) + thin UE5 module (render/input/UI).

This document describes **what the web game does today**, precisely enough to re-implement it without
reading the TypeScript. Where the web behaviour is a bug or a 2D artefact it is called out as a
**QUIRK** with a recommendation. As in the sibling specs, the default is *keep the behaviour* unless
the recommendation says "FIX" and the matching open question at the end is resolved that way.

Citations are `path:line` (line numbers at the time of writing).

**Ownership split with sibling specs** (do not duplicate, follow the links):

| Topic | Owner |
|---|---|
| Damage formula, monster swing scheduling + strike resolution + hit application on the hero | `combat-feel.md` §2, §5 |
| Contact-beat timing (attack animation → damage ms), hit feedback profiles, hit-stop, death animation | `combat-feel.md` §10, §11, §13.2 |
| Kill rewards (exp/gold/loot call), difficulty multipliers, elite affix definitions + behaviours | `combat-feel.md` §13.1, §14, §17 |
| Status effects (freeze/stun/slow/DoT on monsters; monster → hero on-hit statuses) | `classes-stats-skills.md` §13 |
| **Monster definitions, runtime entity, AI state machine, movement, leash, spawning, packs, safe zones, respawn, mini-bosses, quest hunts, boss intros/boss bar (monster side), kill-hook contract, spatial index, event spawns (monster side)** | **this spec** |

---

## 0. Conventions

| Topic | Web | Port rule |
|---|---|---|
| Space | Grid of tiles; positions are `(tileCol, tileRow)` **floats**; integers are tile centres. All ranges (aggro, attack, leash, safe zone, spawn jitter) are in tiles. | Core works in tiles on the ground plane. UE maps `col → X`, `row → Y` with `TILE_SIZE_UU` (architecture doc; 100 uu suggested, same as `combat-feel.md` §0). |
| Walkability | `mapData.collisions[row][col]` is a `boolean[][]`; **`true` = walkable** (despite the name). Out-of-range = blocked. | `WalkGrid::walkable(col,row)`; out-of-range returns false. |
| Distance | Euclidean in tiles (`euclideanDistance`, `distanceSq`, `src/utils/IsometricUtils.ts:30-39`). | Same. |
| Time | ms on the scene clock (`scene.time.now`). Per-frame `delta` in ms. | Core `SimClock` (ms, monotonic). |
| Tick | Monster AI runs **once per rendered frame** and contains per-frame rules (leash heal, acceleration). | Fixed **60 Hz** sim tick (`dt = 1000/60` ms) — same decision as `combat-feel.md` §0. All "per tick" numbers below assume 60 Hz. |
| Timers | `scene.time.delayedCall(ms, fn)`. Phaser fires due timers on the scene `UPDATE` event, i.e. **before** `ZoneScene.update()` runs in the same frame (`node_modules/phaser/src/scene/Systems.js:360-366`, `node_modules/phaser/src/time/Clock.js:162-163`). Timers die with the scene (zone change). | Core timer queue keyed on `SimClock`; drain due timers at the **start** of each tick; clear the zone's timers on zone unload. |
| RNG | `Math.random()`; `randomInt(a,b) = floor(r*(b-a+1))+a` inclusive (`src/utils/MathUtils.ts:5-7`). | Injectable seeded RNG; same helper semantics. Draw **order** is documented for scripted tests; bit parity with JS not required. |
| Rounding | `Math.round` (half → +∞), `Math.floor`. | `floor(x+0.5)` for `Math.round`. |

### 0.1 What Chapter 1 (emerald_plains, Lv 1–10) needs from this area

* Monster defs: `slime_green`, `goblin`, `goblin_chief` (zone list), `miniboss_goblin_shaman` (mini-boss),
  quest hunts `hunt_pendant_thief` and `hunt_redcap_gruk` (built from `goblin`). Full stats in §2.
* AI: idle / patrol / chase / attack / leash, safe-zone repel, activity culling. All Chapter-1 monsters are **melee**
  (no `attackRange > 2.5`), but the ranged path must still exist (later zones; see §4.3).
* Zone spawns (10 spawn entries, ≈52 monsters), 15 s respawn, elite affixes on the chief/shaman/hunt leaders.
* Mini-boss 哥布林萨满 with its pre-fight dialogue (once per save).
* Quest hunts for `q_lost_pendant` (tracked, revealed after 3 clues) and `q_bandit_trouble` (bounty, on accept).
* Boss intro + boss bar for `goblin_chief` (碎牙·格罗克), triggered within 9 tiles.
* Event spawns: random-event ambush/rescue (slimes/goblins); escort quest `q_escort_merchant_plains` (monsters hit the escort).
* Kill hook feeding exp/gold/loot/quests/story/achievements (contract in §11).
* Not needed in Chapter 1 (but data/hook must stay general): labyrinth scaling, seal keepers, sub-dungeon mini-bosses,
  defend waves, pet interception, mercenary tanking, ranged monsters, monster on-hit statuses.

---

## 1. Data model

### 1.1 `MonsterDefinition` (`src/data/types.ts:72-94`) — export verbatim

| Field | Type | Unit / meaning | Runtime consumers |
|---|---|---|---|
| `id` | string | Unique key. Kill-quest target, achievement key, story trigger key, i18n `data.monster.<id>`. | everywhere |
| `name` | string | zh-CN display name (fallback). The in-world label shows this raw string (QUIRK Q11). | label, logs |
| `level` | int | Monster level. | loot item level (`LootSystem.ts:34`), pet exp (`ZoneScene.ts:3805`), `CombatEntity.level` |
| `hp` | int | Max HP at spawn. | `Monster.ts:87-88` |
| `damage` | int | Base damage per hit. | `CombatEntity.baseDamage`, `stats.str`, escort/defend chip damage |
| `defense` | int | Defense. | `CombatEntity.defense` |
| `speed` | int | Move speed. **tiles/s = `speed × 0.03`** (§3.4). Also `stats.dex = floor(speed×0.1)`. | movement |
| `aggroRange` | float | Tiles. Idle/patrol → chase when hero `dist ≤ aggroRange`; chase drops at `> 1.5×`. Mini-boss dialogue radius. | AI |
| `attackRange` | float | Tiles. Chase → attack at `dist ≤ attackRange`; attack → chase at `> 1.2×`. **Ranged iff `> 2.5`** (`ZoneScene.ts:2949`). | AI, strike |
| `attackSpeed` | int | **ms between swing starts** (not a rate). Also compresses the attack animation (§4.2). | `ZoneScene.ts:2823` |
| `expReward` | int | Exp on kill (before bonuses). | `ZoneScene.ts:3799` |
| `goldReward` | [int,int] | Inclusive `randomInt(min,max)` gold on kill. | `ZoneScene.ts:3800` |
| `spriteKey` | string | Art id (`monster_<x>`). **Also gameplay**: substring rules pick monster→hero on-hit statuses (`fire`/`phoenix`/`lava`, `poison`/`venom`/`spider`, `ice`/`frost`; `ZoneScene.ts:6204-6237`) and the projectile tint (`ZoneScene.ts:2951-2952`). | art, status, VFX |
| `elite?` | bool | Rolls elite affixes at spawn/respawn (zone spawns), crown icon, red label, loot: 80 % base drop + 50 % second drop (`LootSystem.ts:35,47`), ley-fruit 12 % vs 1.5 % (`PetSystem.ts:98-100`), kill slow-mo. | many |
| `isMiniBoss?` | bool | Loot floor: at least one **magic+** equipment piece (`LootSystem.ts:62-65`). Set on zone mini-bosses, quest hunts, labyrinth gatekeepers. | loot |
| `isSubDungeonMiniBoss?` | bool | Loot floor **rare+** instead of magic+. | loot |
| `lootTable?` | `LootEntry[]` `{itemId?, quality?, dropRate, levelRange?}` | **Data only — never read at runtime** (QUIRK Q12). Only tests read it. | — |
| `bossSkills?` | string[] | **Data only — never read at runtime** (demon_lord, labyrinth bosses). | — |
| `animCategory?` | `'humanoid'│'slime'│'beast'│'large'│'flying'│'serpentine'│'demonic'` (`types.ts:70`), default `humanoid` | Picks the `AnimConfig` preset (`CharacterAnimator.ts:94-250`): attack duration (→ contact timing scale), death style. | animation |

Lookup (`src/data/monsters/index.ts:9-27`): `MonstersByZone[zoneId]` lists each zone's regular monsters;
`getMonsterDef(id)` searches labyrinth-exclusive monsters first (`AllDungeonMonsters`), then every zone list in
insertion order. Mini-boss definitions live only in `MiniBossByZone` (`src/data/miniBosses.ts:134-140`) and
sub-dungeon bosses in `SubDungeonMiniBosses` (`src/data/subDungeons.ts:54`) — `getMonsterDef` does **not** find them.

**Port additions (recommended derived data fields, generated by the exporter from the rules above):**
`isRanged = attackRange > 2.5`; `onHitStatus[]` from the spriteKey/id keyword rules (only `fire_elemental` and `phoenix`
match today: burn, chance 0.30, value `max(1, floor(damage×0.2))`, 3000 ms); `projectileColor` (`fire`/`phoenix` →
`0xff6600`, `ice` → `0x4488ff`, else `0xcc44cc`). The core must still behave exactly as the keyword rules (each rule rolls
independently; a monster matching several rolls each).

### 1.2 Runtime instance — `Monster` (`src/entities/Monster.ts:35-152`)

| Field | Init | Notes |
|---|---|---|
| `id` | `"monster_" + counter++` (`:82`) | Static counter, never reset (unique across zones in a session). Core: `uint32 EntityId`, monotonic per session. Used as key by status effects, targeting, pet taunts. |
| `definition` | ctor arg | **Current** def; replaced by a modified copy when elite affixes apply (§1.3). |
| `originalDefinition` | ctor arg | Def as constructed (post-difficulty, **pre-affix**). Used for respawn so affixes never compound. |
| `hp`, `maxHp` | `def.hp` | `double` in the core: leash heal and regen curse add fractional HP (`:171`, `ZoneScene.ts:1188`). |
| `mana`, `maxMana` | 0, 0 | Unused (monsters have no skills). |
| `stats` | `{str: floor(def.damage×0.8), dex: floor(def.speed×0.1), vit: floor(def.hp×0.1), int:3, spi:3, lck:3}` (`:89-96`) | Used by `calculateDamage` (`combat-feel.md` §2). After affixes only `str` is recomputed (`:405`). |
| `buffs` | `[]` | `ActiveBuff[]`; receives `damageAmplify` (death_mark) and `taunted` (taunt_roar, unread). Never pruned (`combat-feel.md` §1.3). |
| `eliteAffixes` | `[]` | `{definition, lastTeleportTime:0, lastCurseTickTime:0}[]` |
| `tileCol`, `tileRow` | spawn tile (int) | Float position. |
| `spawnCol`, `spawnRow` | spawn tile | Leash anchor and patrol centre. Fixed for the life of this instance. |
| `state` | `'idle'` | §3.1 |
| `lastAttackTime` | 0 | Scene ms of last swing start (also set by pet interception). |
| `patrolTarget` | null | `{col,row}` int tile. |
| `patrolTimer` | 0 | ms accumulated in idle. |
| `leashRange` | 8 | tiles (`:69`). Constant for all monsters. |
| `currentMoveSpeed` | 0 | **Per-frame displacement in tiles** (not tiles/s), smoothed (§3.4). |
| `moveAccel` | 6 | 1/s (`:72`). |
| `lastHitFrom` | null | Screen point of the last hit that had a source; the corpse is thrown away from it (render). |
| `lastDamagedAt` | 0 | Scene ms of the last `takeDamage` (`:282,293`); read by the labyrinth regen curse. |

Scene-side bookkeeping in `ZoneScene` (roles): `monsters: Monster[]` (all, including dead ones until replaced),
`monsterGrid: SpatialGrid<Monster>` (§12), `activeMonsters` (§3.8), `miniBossMonster` (single ref, `:226`),
`questHuntMonsters: Map<huntId, Monster>` (`:228`), `questSpawned: WeakSet<Monster>` = never respawn (`:239-240`),
`defendWaveMonsters`, rescue-event monster ids.

**Core recommendation:** `MonsterInstance { EntityId id; DefRef def; DefRef originalDef; MonsterRole role; bool noRespawn; std::string huntId; ... }`
with `MonsterRole = Regular | ZoneMiniBoss | SubDungeonMiniBoss | HuntLeader | HuntMinion | AmbushSpawn | RescueSpawn | DefendWave | LabyrinthFloor | SealKeeper`.
Web respawn eligibility is derived in §7; keep it as an explicit flag in the core.

### 1.3 Definition pipeline (order matters for rounding)

```
base def ─▶ role transform ─▶ difficulty scale ─▶ new Monster(def) ─▶ elite affixes (if rolled)
             hunt: makeHuntDefinition (§9.3)          (originalDefinition = this def)
             labyrinth: DungeonSystem.scaleMonster/makeGatekeeper (replaces the difficulty step, §13)
```
* Difficulty: `DifficultySystem.scaleMonster(def, diff)` (`src/systems/DifficultySystem.ts:198-212`); `normal` returns the
  **same object**; otherwise `Math.round` of hp/damage/defense/expReward and both gold bounds (gold uses the exp
  multiplier). Multipliers (`:64-72`): nightmare hp 1.5 dmg 1.5 def 1.3 exp 2.0; hell 2.0/2.0/1.6/3.0.
* Defend waves are the exception: difficulty **then** wave scaling (`ZoneScene.ts:7036-7039`, §6.4).
* Elite affixes (`Monster.applyEliteAffixes`, `Monster.ts:389-416`; definitions and rolling in `combat-feel.md` §17):
  `maxHp = hp = floor(maxHp×hpMult)`, then `definition = {...definition, damage: floor(d×dmgMult), speed: floor(s×spdMult), defense: floor(def×defMult)}`,
  `stats.str = floor(damage×0.8)`; label text `[名1·名2] 名` in `#ff6600`, always visible.
* Who rolls affixes: zone spawns and respawns with `def.elite`, every zone/sub-dungeon mini-boss, every hunt leader,
  every labyrinth seal keeper. **Never**: ambush/rescue/defend spawns and hunt minions, even if their base def is elite.

---

## 2. Chapter 1 roster (emerald_plains) — full stats

### 2.1 Base definitions (normal difficulty)

| id | zh / en | Lv | HP | Dmg | Def | speed → tiles/s | aggro | atkRange | type | atkSpeed ms | exp | gold | elite | role | spriteKey / animCategory | source |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `slime_green` | 绿色史莱姆 / Green Slime | 1 | 30 | 5 | 2 | 40 → **1.20** | 4 | 1.2 | melee | 1500 | 12 | 2–4 | – | regular | `monster_slime` / slime | `src/data/monsters/emerald_plains.ts:4-19` |
| `goblin` | 哥布林 / Goblin | 3 | 55 | 8 | 4 | 55 → **1.65** | 5 | 1.5 | melee | 1200 | 18 | 3–6 | – | regular | `monster_goblin` / humanoid | `:20-35` |
| `goblin_chief` | 哥布林首领 / Goblin Chief (story name 碎牙·格罗克 / Grokk Brokentooth) | 5 | 160 | 14 | 8 | 50 → **1.50** | 6 | 1.8 | melee | 1000 | 55 | 8–15 | ✔ | regular (respawns) + story boss | `monster_goblin_chief` / humanoid | `:36-52` |
| `miniboss_goblin_shaman` | 哥布林萨满 / Goblin Shaman | 6 | 280 | 16 | 10 | 45 → **1.35** | 7 | 2.5 | **melee** (2.5 is not > 2.5; QUIRK Q5) | 1300 | 90 | 15–30 | ✔ (+`isMiniBoss`) | zone mini-boss | `monster_goblin_shaman` / humanoid | `src/data/miniBosses.ts:9-31` |
| `hunt_pendant_thief` | 小贼斯尼克 / Sneek the Thief | 3 | 165 | 10 | 5 | 55 → 1.65 | 7 | 1.5 | melee | 1200 | 54 | 9–18 | ✔ (+`isMiniBoss`) | hunt leader (from `goblin`, hp×3, dmg×1.2) | `monster_goblin` / humanoid, drawn ×1.25 | `src/data/quests/all_quests.ts:133-135` + `QuestHunts.ts:48-65` |
| `hunt_redcap_gruk` | 红帽格鲁克 / Gruk Redcap | 3 | 275 | 13 | 5 | 55 → 1.65 | 7 | 1.5 | melee | 1200 | 90 | 9–18 | ✔ (+`isMiniBoss`) | hunt leader (from `goblin`, hp×5, dmg×1.6) | `monster_goblin` / humanoid, drawn ×1.25 | `all_quests.ts:151-153` |

Mini-boss `lootTable` (data only, Q12): magic 0.8, rare 0.35, legendary 0.03.

### 2.2 Derived per-instance numbers (normal, before affixes)

| id | stats str/dex/vit (int,spi,lck = 3) | chase drops at (`1.5×aggro`) | attack exits at (`1.2×atkRange`) | melee whiff reach (`atkRange×1.35+0.5`) | contact ms / telegraph ms | escort chip dmg `max(1,floor(dmg×0.3))` |
|---|---|---|---|---|---|---|
| slime_green | 4 / 4 / 3 | 6.0 | 1.44 | 2.12 | 250 / 155 | 1 |
| goblin | 6 / 5 / 5 | 7.5 | 1.8 | 2.525 | 250 / 155 | 2 |
| goblin_chief | 11 / 5 / 16 | 9.0 | 2.16 | 2.93 | 250 / 155 | 4 |
| miniboss_goblin_shaman | 12 / 4 / 28 | 10.5 | 3.0 | 3.875 | 250 / 155 | 4 |
| hunt_pendant_thief | 8 / 5 / 16 | 10.5 | 1.8 | 2.525 | 250 / 155 | 3 |
| hunt_redcap_gruk | 10 / 5 / 27 | 10.5 | 1.8 | 2.525 | 250 / 155 | 3 |

Contact: all four rigs use a 500 ms attack preset; `attackSpeedScale(500, atkSpeed) = clamp(atkSpeed×0.9/500, 0.35, 1) = 1`
for every Chapter-1 monster → contact 250 ms, wind-up tint `0.62×250 = 155` ms (`combat-feel.md` §10.1, §10.3).

### 2.3 Difficulty variants (computed with the exact web rounding)

| id | normal hp/dmg/def/exp/gold | nightmare | hell |
|---|---|---|---|
| slime_green | 30/5/2/12/2–4 | 45/8/3/24/4–8 | 60/10/3/36/6–12 |
| goblin | 55/8/4/18/3–6 | 83/12/5/36/6–12 | 110/16/6/54/9–18 |
| goblin_chief | 160/14/8/55/8–15 | 240/21/10/110/16–30 | 320/28/13/165/24–45 |
| miniboss_goblin_shaman | 280/16/10/90/15–30 | 420/24/13/180/30–60 | 560/32/16/270/45–90 |
| hunt_pendant_thief | 165/10/5/54/9–18 | 248/15/7/108/18–36 | 330/20/8/162/27–54 |
| hunt_redcap_gruk | 275/13/5/90/9–18 | 413/20/7/180/18–36 | 550/26/8/270/27–54 |

(`round(82.5)=83`, `round(247.5)=248`, `round(412.5)=413` — half-up rounding is visible here.)

### 2.4 Elite-affix variants (normal; emerald_plains always rolls exactly 1 affix, `EliteAffixSystem.ts:205-211`)

Values after `applyEliteAffixes` — hp / dmg / speed (tiles/s) / def / str:

| affix | goblin_chief | miniboss_goblin_shaman | hunt_pendant_thief | hunt_redcap_gruk |
|---|---|---|---|---|
| fire_enhanced 炎魔 | 192/14/50 (1.5)/8/11 | 336/16/45 (1.35)/10/12 | 198/10/55/5/8 | 330/13/55/5/10 |
| swift 迅捷 | 160/14/**80 (2.4)**/8/11 | 280/16/72 (2.16)/10/12 | 165/10/88 (2.64)/5/8 | 275/13/88 (2.64)/5/10 |
| teleporting 瞬移 | 184/15/50/8/12 | 322/17/45/10/13 | 189/11/55/5/8 | 316/14/55/5/11 |
| extra_strong 狂暴 | 208/18/50/9/14 | 364/21/45/11/16 | 214/13/55/5/10 | 357/17/55/5/13 |
| curse_aura 诅咒 | 192/15/50/8/12 | 336/17/45/10/13 | 198/11/55/5/8 | 330/14/55/5/11 |
| vampiric 吸血 | 200/15/50/8/12 | 350/17/45/10/13 | 206/11/55/5/8 | 343/14/55/5/11 |
| frozen 冰封 | 192/14/45 (1.35)/8/11 | 336/16/40 (1.2)/11/12 | 198/10/49 (1.47)/5/8 | 330/13/49 (1.47)/5/10 |

### 2.5 Art / attack style per Chapter-1 monster (render-only; for the Blender pipeline)

| spriteKey | Look (from the procedural rig) | Attack style | Views in web |
|---|---|---|---|
| `monster_slime` | Glossy green jelly (`0x5fcf5a`), darker core, drifting bubbles, half-digested bone, big glaring eyes. Hops to move, rears up and body-slams; splats into a puddle on death (`src/graphics/sprites/monsters/Slime.ts:1-5`). | body slam | single view, mirrored (radial) |
| `monster_goblin` | Hunched, big-headed raider, swept-back ears, hooked nose, toothy grin, bone necklace, crude stone-tipped spear (`Goblin.ts:1-4`). Hunt leaders reuse it at ×1.25 scale. | `thrust` (`Goblin.ts:348`) | se + ne |
| `monster_goblin_chief` | Bulkier warlord: horned iron helm with nose guard, wolf-pelt mantle with skull pauldron, studded leather, red war paint, huge rusted cleaver; faint ember glow `0xff5a2a` in the eyes (`GoblinChief.ts:1-5`). | `overhead` chop (`GoblinChief.ts:324`) | se + ne |
| `monster_goblin_shaman` | Hunched spirit-caller: cracked bird-skull mask, feather headdress, mangy fur shawl, hide skirt with bone charms, gnarled totem staff crowned by a horned skull burning with green spirit-fire (`#7dffa0`); green rune ring on the ground while casting (`GoblinShaman.ts:1-6`). | `cast` (raises totem, hurls spirit-fire) — but resolves as melee (Q5) | se + ne |

Animation set every monster needs (web sheet layout, `src/graphics/SpriteGenerator.ts:114-121,868-877`): idle 4 frames @6 fps
loop (≈667 ms cycle), walk 6 @10 fps loop (600 ms), attack 4 @12 fps once (contact on the last frame = 250 ms), hurt 2 @10 fps
(200 ms), death 4 @6 fps. Monsters have no cast action. 3D: author Idle/Walk loops, Attack (≈500 ms, `Contact` notify; the core,
not the notify, decides damage time — §4.2), HitReact (≈200 ms, additive), Death (500 ms; `large` 800 ms).

---

## 3. AI

### 3.1 States (`Monster.ts:23`)

`MonsterState = 'idle' | 'patrol' | 'chase' | 'attack' | 'dead'`.
`isAggro() = state == chase || state == attack` (`:377-379`). `isAlive() = state != dead && hp > 0` (`:373-375`).

There is **no** flee, return-home, search, alert, kite, assist/call-for-help or stunned state. Aggro is purely
distance-based (see §3.7).

### 3.2 Per-tick update — `Monster.update(time, dt, heroCol, heroRow, walk, speedMul)` (`Monster.ts:160-239`)

Exact algorithm (distances are computed **once, at the top**, before any movement this tick):

```
if state == dead: return
dP = dist(self, hero)            // hero may be (-999,-999), see §3.8
dS = dist(self, spawn)

// Leash (:166-174)
if dS > leashRange(8) and state != idle:
    state = idle
    currentMoveSpeed = 0
    moveToward(spawnCol, spawnRow, dt, walk, speedMul)   // one smoothed step from rest (§3.5)
    hp = min(maxHp, hp + maxHp*0.01)                       // 1 % per tick, fractional
    refreshHpBar()
    return                                                 // no anim drive, no animator.update this tick

switch state:
  idle (:177-190):
    patrolTimer += dt
    if dP <= aggroRange:            state = chase                 // no movement this tick
    else if patrolTimer > 3000:
        patrolTimer = 0
        pc = spawnCol + randomInt(-2,2); pr = spawnRow + randomInt(-2,2)    // 2 RNG draws, col first
        if inBounds(pc,pr) and walk[pr][pc]: state = patrol; patrolTarget = (pc,pr)
        // else stay idle; the timer was already reset → next try in 3000 ms
  patrol (:192-207):
    if dP <= aggroRange:            state = chase; patrolTarget = null
    else if patrolTarget:
        if moveToward(patrolTarget):  state = idle; currentMoveSpeed = 0; patrolTarget = null
    else:                           state = idle; currentMoveSpeed = 0
  chase (:209-218):
    if dP > aggroRange*1.5:         state = idle; currentMoveSpeed = 0
    else if dP <= attackRange:      state = attack                // no movement this tick
    else:                           moveToward(heroCol, heroRow)
  attack (:220-229):
    if dP > attackRange*1.2:        state = chase                 // no movement this tick
    faceToward(hero)                // every tick, even on the exit tick (render facing)
    // swings are scheduled by the scene (§4.1), not here

// Animation drive (:232-238)
if state == idle: anim.setIdle()
else if state == patrol or chase: anim.setWalk()
// attack: no request (the swing montage owns the animation; between swings the animator returns to idle)
anim.update(dt)
```

Notes:
* `patrolTimer` only accumulates in `idle` and is reset only by a patrol attempt; it keeps its value across a chase,
  so a monster that drops aggro after > 3 s of total idle time tries a patrol on its first idle tick.
* Every transition into `chase`/`attack` takes effect for the **next** tick's behaviour (no same-tick move).
* `currentMoveSpeed` is **not** reset on chase→attack or attack→chase; a monster resuming a chase keeps its last speed.
* The `time` argument is unused.

### 3.3 Transition table

| From | Condition (evaluated in this order) | To | Side effects |
|---|---|---|---|
| any non-idle (patrol/chase/attack) | `dS > 8` | idle | leash step + heal 1 % maxHp, tick ends |
| idle | `dP ≤ aggroRange` | chase | – |
| idle | `patrolTimer > 3000` and random tile within ±2 of spawn walkable | patrol | timer = 0 (also when the tile is rejected) |
| patrol | `dP ≤ aggroRange` | chase | target cleared |
| patrol | arrived (`dist(target) < 0.1` at the start of the move) | idle | speed 0 |
| chase | `dP > 1.5×aggroRange` | idle | speed 0 |
| chase | `dP ≤ attackRange` | attack | – |
| attack | `dP > 1.2×attackRange` | chase | – |
| alive | `hp ≤ 0` via `takeDamage` | dead | §5 |
| (external) | §3.10 | various | |

### 3.4 Movement — `moveToward(tc, tr, dt, walk, speedMul) → arrived` (`Monster.ts:241-275`)

```
dx = tc - tileCol; dy = tr - tileRow; d = sqrt(dx²+dy²)
if d < 0.1: return true                                     // "arrived" — no snap, no move
targetStep = def.speed * speedMul * (dt/1000) * 0.03          // tiles this tick
currentMoveSpeed += (targetStep - currentMoveSpeed) * 6 * (dt/1000)
nx = dx/d; ny = dy/d
newCol = tileCol + nx*currentMoveSpeed; newRow = tileRow + ny*currentMoveSpeed
cc = round(newCol); cr = round(newRow)
if inBounds(cc,cr) and walk[cr][cc]: tileCol = newCol; tileRow = newRow     // else: no move at all
heading = (nx, ny)                                           // facing = intended heading, even when blocked
return false
```

* Steady-state speed `= def.speed × 0.03` tiles/s in **every** direction (tile space; unlike the hero, whose speed is in
  iso screen px). First-order approach with rate 6/s: at 60 Hz the per-tick step reaches 95 % of full speed after 29 ticks
  (≈0.48 s).
* Equivalent velocity form for the core (identical at a fixed tick): `v += (speed×0.03×speedMul − v)×6×dt_s; pos += dir×v×dt_s`.
  Store the web's per-tick displacement only if variable `dt` must be reproduced.
* **No pathfinding** (straight line to the target), **no sliding** along walls (a blocked step cancels both axes), **no
  monster–monster or monster–hero collision/separation** (packs converge on the same point next to the hero). A blocked
  monster keeps "walking in place" (walk anim, speed accumulating) until its state changes.
* `speedMul` = status slow multiplier (`StatusEffectSystem.getSpeedMultiplier`, `src/systems/StatusEffectSystem.ts:261-275`):
  1, or `max(0.2, 1 − clamp(slow,0,100)/100)`; slow affects movement only, never attack rate.
* Facing (render): heading vector → screen direction (`tileDeltaToScreen`, `:29-33`) → 2-view + mirror sprite. 3D: core
  exposes `heading` (unit vector, tile space); actor yaw follows it (move) or the hero (attack state).

### 3.5 Leash — exact behaviour and its consequences (QUIRK Q1)

Because the leash branch sets `state = idle` and the leash condition requires `state != idle`, the leash fires for **one tick
only**; it does not walk the monster home. What actually happens to a monster that crosses 8 tiles from its spawn:

1. Tick N (state chase/attack/patrol, `dS > 8`): → idle, one step toward spawn **from rest**
   (`step = speed×0.03×dt_s × 6×dt_s` = 10 % of a normal step at 60 Hz: 0.00275 tiles for a goblin), heal 1 % maxHp.
2. Tick N+1 (idle): if the hero is within `aggroRange` → chase (no move). Otherwise wait for the 3000 ms patrol timer.
3. Tick N+2 (chase, still `dS > 8`): leash again (step home, heal 1 %).

Net effect while the hero stays within `aggroRange`: the monster is **pinned** just outside its 8-tile ring, alternates
idle/chase every tick (`isAggro()` true every other tick), drifts home at ~5 % speed, **can never reach `attack` state**
(so never swings), and heals **1 % maxHp every 2 ticks = 30 %/s at 60 Hz** (72 %/s at 144 Hz in the web — frame-rate bug).
A goblin then regenerates 16.5 HP/s — on the order of a low-level hero's entire basic-attack DPS — so it is close to unkillable
while pinned (the hero's auto-attack also only sees it as a target on its `chase` ticks). When the hero is outside `aggroRange`, the monster only creeps
home by one 10 % step (and heals 1 %) per accepted patrol attempt (every 3 s); once back inside 8 tiles, patrol works
normally and walks it home.

Teleporting elites (`combat-feel.md` §17.4) and safe-zone repel never move `spawn`, so they can also put a monster outside its
ring. Respawn picks a new spawn tile (§7), so the ring moves with each respawn.

**Recommendation (FIX, Q1):** add an explicit `returning` state: on leash, `isAggro = false`, ignore the hero, walk to spawn with
normal `moveToward` (acceleration kept), heal `0.6 × maxHp` per second (= the intended 1 %/tick at 60 Hz), and switch to idle on
arrival (`< 0.1`) or when `dS ≤ 1`. Keep the web behaviour available behind `AiConfig::leashMode = WebParity` for A/B tests.

### 3.6 Patrol / wander

* Interval: one attempt per `> 3000` ms of idle time (strictly greater). Target: `spawn + (randomInt(−2,2), randomInt(−2,2))`
  (25 tiles incl. the spawn itself), accepted only if in bounds and walkable; no safe-zone check.
* A patrol to a tile behind an obstacle never arrives (no pathfinding) → the monster walks in place in `patrol` until the hero
  aggroes it or it leashes (QUIRK Q13; recommend FIX: patrol timeout 4000 ms → idle).
* Arrival threshold 0.1 tiles; no snapping.

### 3.7 Behaviours that do not exist (state explicitly in the port)

* **No flee**, no low-HP behaviour, no kiting/keeping distance for ranged monsters (they close to `attackRange` and stand).
* **No aggro on damage** (QUIRK Q2): `takeDamage` never changes state. A hero hitting a monster from beyond its `aggroRange`
  (e.g. a ranged skill at 6+ tiles on a 4-tile slime) gets no reaction. Recommend (optional FIX): "provoked" — on a hero hit
  while idle/patrol, enter chase and ignore the `1.5×aggro` drop for 5000 ms (leash still applies).
* **No social aggro** / pack assist: monsters of one spawn entry act independently.
* **No line-of-sight** checks for aggro or ranged attacks (aggro and shots go through walls) — Q15.
* **No pathfinding** for monsters (heroes and escorts use A*, monsters never do).
* **No target other than the hero** for AI decisions (pets, mercenaries, escorts and defend targets are only hit opportunistically, §4.4).

### 3.8 Scene-level driver (`ZoneScene.update`, `ZoneScene.ts:1415-1473`)

Per tick, after hero input/movement/regen and `checkMiniBossDialogue()` (§8.3):

1. **Activity set** (`:1417-1427`): every 250 ms (`SimulationScheduler.due('active-monsters', now, 250)`, first call is
   immediately due, `src/systems/SimulationScheduler.ts:4-10`) rebuild `activeMonsters` = `monsterGrid.queryRadius(hero, 30)`
   (`MONSTER_AI_CULL_DIST_SQ = 30²`, `:327`) **plus every aggro monster anywhere**. Monsters outside the set are fully frozen
   (no patrol, no animation tick). Monsters spawned/respawned between rebuilds join at the next rebuild (≤ 250 ms).
2. `safeR = mapData.safeZoneRadius ?? 9` (emerald_plains: 9); `heroInSafe = ∃ camp: distSq(hero, camp) < safeR²` (strict).
3. For each monster in `activeMonsters`, in list order:
   * dead → skip.
   * Mini-boss dialogue active and this is the mini-boss → `animator.update(dt)` only; skip AI (`:1438-1442`).
   * Immobilized (`freeze` or `stun`, `StatusEffectSystem.ts:253-255`) → `animator.update(dt)` only; skip AI, safe-zone check
     and grid update (`:1444-1449`).
   * **Safe-zone repel** (`:1451-1461`): if the monster stands inside any camp radius (`distSq < safeR²`) and `isAggro()` → `state = idle`.
   * `speedMul = getSpeedMultiplier(id)`.
   * If `heroInSafe && !isAggro()` → `update(..., heroCol = -999, heroRow = -999, ...)` (hero invisible); else real hero position.
   * `monsterGrid.update(monster)` (`:1472`).

Consequences worth testing: (a) a hero in camp can't be newly aggroed; (b) monsters already chasing follow the hero to the camp
edge, then are forced idle the moment *they* enter the radius; (c) a monster standing inside a camp radius while the hero is
outside but within `aggroRange` flips idle→chase→idle every tick and never moves (frozen at the edge).

### 3.9 Tick order (monster-relevant parts of `ZoneScene.update`, `:1345-1560`)

0. Due timers (strike contacts, projectile arrivals, respawns, skill hits) fire **before** the update (§0).
1. If `storyDirector.cinematic || dungeonChoosing || abyssModal` → **skip the whole update** (world frozen; timers still fire,
   but `resolveMonsterStrike` aborts during a cinematic, `:2946`). UI panels (inventory, shop, dialogue) do **not** pause the world.
2. Hero input, movement, passives, `player.update` (regen).
3. `checkMiniBossDialogue()` (§8.3).
4. Monster AI loop (§3.8).
5. NPC updates.
6. `handleCombat` (`:2808-2846`): prune hero buffs; **monster swings** (§4.1); hero auto-attack.
7. `handleMercenaryCombat`, mercenary, pet, `updateEscortNpc` (monsters chip the escort), `updateDefendQuest`,
   `updateEliteAffixBehaviors` (teleport/curse, `:5990-6066`), `updateStatusEffects` (DoT ticks may kill → kill hook),
   `updateCombatState`, `checkRandomEvents` (ambush/rescue spawns), target indicator, auto-combat.
8. Later: quest world, `storyDirector.update` (boss scan every 250 ms, §10).

### 3.10 External writes to monster state (complete list)

| Writer | Effect | Source |
|---|---|---|
| Safe-zone repel | aggro monster inside a camp radius → idle | `ZoneScene.ts:1451-1461` |
| Mini-boss dialogue | on trigger → idle (frozen while open); on dismiss → chase | `:4649`, `:4667` |
| Warrior `taunt_roar` | alive monsters within the skill's AoE radius: push buff `{stat:'taunted', value 1, duration = buff duration, startTime}` (unread); idle/patrol → chase | `:2677-2694` |
| Ambush / rescue / defend-wave spawns | created directly in `chase` | `:3296`, `:3485`, `:7046` |
| Teleporting affix | instant reposition near the hero (spawn unchanged) | `:5995-6034` |
| Pet interception (later) | sets `lastAttackTime` and swings at the pet | `src/systems/PetCompanion.ts:118-134` |
| Status effects | freeze/stun skip AI + cancel pending strikes at contact; slow scales movement | §3.8, §4.3 |

Externally forced `chase` is undone on the next tick if `dP > 1.5×aggroRange` (taunt/ambush only "stick" near the hero).

---

## 4. Attacks

### 4.1 Swing scheduling (`ZoneScene.ts:2817-2830`; details in `combat-feel.md` §5.1)

Each tick while the hero is alive, for every monster in `monsterGrid.queryRadius(hero, 12)`:
`alive && state == attack && !immobilized && now − lastAttackTime ≥ definition.attackSpeed` →
(pet may intercept) → `lastAttackTime = now`; `contactMs = playAttack(heroScreenPos)`; schedule `resolveMonsterStrike` at `now + contactMs`.

* `lastAttackTime` starts at 0 → the **first swing starts on the first tick in `attack` state**. It is not reset by state
  changes, so re-entering `attack` within `attackSpeed` of the last swing waits for the remainder.
* Hurt reactions, hit-stop and slow never delay or cancel a swing (hit-stop is visual only, `CharacterAnimator.ts:472-479`).
  Freeze/stun at contact time cancels it (§4.3). Leaving attack range does not cancel it (melee whiff check at contact).
* While the hero is dead (`hp ≤ 0`) no swings start (`:2809`); monsters keep their AI states around the corpse.

### 4.2 Contact timing (`combat-feel.md` §10.1 — monster rows)

`contactMs = round(frameContact × speed)`, `frameContact = round((4−1)×attackContact(=1)) × 1000/12 = 250 ms` for every
monster sheet; `speed = clamp(attackSpeed×0.9 / attackDuration, 0.35, 1)` (`src/systems/HitFeedback.ts:72-75`).

| animCategory | attackDuration ms | speed < 1 when attackSpeed < | death ms |
|---|---|---|---|
| humanoid, slime, flying, serpentine, demonic | 500 | 555.6 | 500 (slime "splat", serpentine/demonic "dissolve" in the legacy fallback) |
| beast | 250 | 277.8 | 500 |
| large | 450 | 500 | 800 |

Wind-up (telegraph tint) = `0.62 × contactMs`; strike = remainder; recover = `max(70, attackDuration×speed − contactMs)`.
3D: the core owns `contactMs`; the montage is played at a rate that puts its authored `Contact` notify on that time
(play rate = authoredContactMs / contactMs). Never derive damage timing from the UE notify.

### 4.3 Ranged vs melee and strike resolution (`ZoneScene.ts:2944-2968`; full hit application `combat-feel.md` §5.2-5.3)

At contact: abort if the monster or hero is dead, a zone transition is running, a story cinematic is playing, or the monster is
now frozen/stunned.
* **Ranged** iff `definition.attackRange > 2.5` (data, not AI — AI is identical for both): projectile from the monster to the
  hero's position *at contact*, flight `clamp(isoPxDist×2, 200, 500)` ms (`combat-feel.md` §5.2), then `applyMonsterHit` if both
  are still alive (no distance re-check, no LOS, no cinematic check on arrival).
* **Melee**: whiff silently if `dist > attackRange×1.35 + 0.5`; else `applyMonsterHit`.
* No Chapter-1 monster is ranged. Ranged today: `fire_elemental` 3.5, `phoenix` 3.0, `succubus` 3.5, `demon_lord` 3.0,
  `miniboss_shadow_weaver` 3.0, `miniboss_sand_wraith` 3.0, `miniboss_void_herald` 3.5, `sub_altar_keeper` 3.0, `dungeon_abyss_lord` 3.5
  (`mountain_troll`, `sandworm`, `dungeon_shade`, `dungeon_mid_boss` at exactly 2.5 are melee).

### 4.4 Non-hero victims

| Victim | Rule | Ch1? |
|---|---|---|
| Ley-beast (pet) | Before a swing: if the pet taunted this monster (until `taunts[id] > now`) or (pet within `attackRange+0.5`, closer than the hero, and `rand < 0.25`) → the swing goes to the pet (`PetCompanion.ts:53,118-134`). | later |
| Tank mercenary | `handleMercenaryCombat` (`ZoneScene.ts:6521-6555`): monster in `attack` within `(attackRange+0.5)` of a tank merc that is closer than the hero, and swing timer ready → instant hit on the merc (no animation). **QUIRK Q14**: `handleCombat` runs first and already consumed the swing timer, so this only happens while the hero is dead. | rare |
| Escort NPC | `updateEscortNpc` (`:6791-6806`): every **aggro** monster with `distSq(monster, escort) < 16` deals `max(1, floor(def.damage×0.3))` every 2000 ms per monster (first hit immediate; no animation, no RNG, no defense). Escort HP = `quest.level×20+100` (merchant: 200, `:6691`). | **yes** (`q_escort_merchant_plains`) |
| Defend target | wave monsters with `distSq < 9` deal `max(1, floor(def.damage×0.2))` every 2000 ms (`:6992-7004`). Their AI still chases the hero (QUIRK, later milestone). | no |

---

## 5. Taking damage, healing, death (`Monster.ts:282-345`)

* `takeDamage(amount, srcX?, srcY?, {isCrit, isTick})` → if dead: return `'tick'`. Else `lastDamagedAt = now`;
  `hp = max(0, hp − amount)`; refresh bar; `weight = classifyHit({damage, maxHp, isCrit, killed: wasAlive && hp ≤ 0, isTick})`;
  record `lastHitFrom` when a source point is given; white flash `HIT_PROFILES[w].flashMs`; if `hp ≤ 0` → `die()`; else hurt
  recoil + target hit-stop. Returns the weight (`combat-feel.md` §11). **No AI reaction** (§3.7).
* `heal(amount)`: ignored if dead, `amount ≤ 0`, or full; `hp = min(maxHp, hp+amount)`; refresh bar. The vampiric affix heals by
  writing `hp` directly without a bar refresh (`ZoneScene.ts:3040-3046`, QUIRK; FIX: use `heal`).
* `die()`: `state = dead`, hide bar/label, remove affix visuals, play death (thrown away from `lastHitFrom`), destroy the sprite at
  the end. The `Monster` object stays in `monsters` (and in the spatial grid) as a dead entry until respawn replaces it or the
  zone unloads; all queries filter `isAlive()`.
* **Kill credit exactly once:** every damage path calls `onMonsterKilled(m)` right after `takeDamage` when `!m.isAlive()`
  (basic attack `:2924`, skills `:2630,2649,2748,2785`, DoT `:6137`, mercenary `:6473,6493`, pet `:7152`); delayed hits re-check
  `isAlive()` first. Core: emit `MonsterKilled` from the single alive→dead transition inside `applyDamage` instead.
* HP bar rule (render): hidden at full HP; `ratio > 0.5` green `0x2ecc71`, `> 0.25` orange `0xf39c12`, else red `0xe74c3c`.

---

## 6. Spawning

### 6.1 Zone spawn points and packs — `spawnMonsters()` (`ZoneScene.ts:4423-4465`)

Map data: `spawns: { col, row, monsterId, count }[]` (`src/data/types.ts:221`). A spawn entry **is** a pack: `count` monsters of
one id scattered around the anchor; there is no pack logic beyond placement.

```
defs = MonstersByZone[zone] ?? []
safeR = mapData.safeZoneRadius ?? 9
for spawn in mapData.spawns (data order):
    def = defs.find(id) ?? getMonsterDef(id);  if !def: continue
    def = inLabyrinth ? DungeonSystem.scaleMonster(def, floorCfg, runDifficulty) : DifficultySystem.scaleMonster(def, difficulty)
    repeat spawn.count times:
        c = clamp(spawn.col + randomInt(-3,3), 1, cols-2)      // RNG draw 1
        r = clamp(spawn.row + randomInt(-3,3), 1, rows-2)      // RNG draw 2
        if !walk[r][c]: continue                               // ONE attempt — the monster is simply missing
        if ∃ camp: distSq((c,r), camp) < safeR²: continue        // ONE attempt
        m = new Monster(def, c, r)                              // spawn anchor = (c,r)
        if def.elite: affixes = rollAffixes(zone, true)         // RNG: count draw + one draw per affix
                      if affixes: m.applyEliteAffixes(affixes)
        add to monsters + grid
```

* Monsters are **not saved**. Every zone entry (new game, load, zone transition, death in a labyrinth) rebuilds the whole
  population from the data with fresh jitter, full HP and fresh affixes. Pending respawn timers die with the scene.
* Spawn order on zone entry (`ZoneScene.ts:476-516`): `spawnMonsters` → NPCs → field NPCs → rare pet spawns (1 RNG draw each) →
  `spawnMiniBoss` → lore → sub-dungeon entrances → story decor → labyrinth portal → mercenary → QuestWorld + StoryDirector →
  pet companion → escort NPC → defend target → **`spawnQuestHunts(false)`**.
* Map generation guarantees a walkable 5×5 square around every spawn anchor (`clearArea(..., 2, ...)`,
  `src/systems/MapGenerator.ts:559-561`) and a carved path from each anchor to the nearest camp (`:622-634`); the outer ring of
  the ±3 jitter depends on terrain.

### 6.2 Safe zones

Every camp (`mapData.camps[].col/row`) is a safe zone of radius `safeZoneRadius ?? 9` tiles (strict `<`). Effects on monsters:
no zone spawn/respawn inside (spawn and respawn attempts are rejected), aggro monsters inside are forced idle each tick,
non-aggro monsters cannot see a hero inside (§3.8). Ambush/rescue/defend/hunt/mini-boss spawns do **not** check safe zones.
(The hero also gets ×50 regen within 5 tiles of a camp — `combat-feel.md` §1.4.)

### 6.3 Chapter-1 spawn table (`src/data/maps/emerald_plains.ts:16-35`)

Map 120×120 (13 167 walkable tiles with the generator seed 12345), camps (15,15) [blacksmith, merchant, quest_elder] and
(95,100) [merchant], safe radius 9, hero start (15,22). "Expected" = `count × usable offsets / 49`, measured on the generated map
(walls + safe-zone rejections).

| # | Anchor | Monster | count | usable offsets / 49 | expected alive at entry | Notes |
|---|---|---|---|---|---|---|
| 1 | (25,20) | slime_green | 8 | 42 (7 inside camp 1 radius) | 6.86 | first quest area `q_kill_slimes` (32,12 r12) |
| 2 | (35,40) | goblin | 6 | 48 | 5.88 | escort start (30,40) is next to it |
| 3 | (20,45) | slime_green | 5 | 49 | 5.00 | |
| 4 | (85,18) | slime_green | 6 | 49 | 6.00 | sprite-friend quest area |
| 5 | (95,35) | goblin | 6 | 48 | 5.88 | |
| 6 | (75,50) | goblin | 4 | 49 | 4.00 | |
| 7 | (30,80) | goblin | 8 | 47 | 7.67 | |
| 8 | (15,95) | goblin_chief | 1 | 49 | 1.00 | main-quest boss; `q_find_goblin_chief` questArea is (25,65) r10 |
| 9 | (80,75) | goblin | 5 | 49 | 5.00 | red-cap bounty (82,72) is here |
| 10 | (95,90) | goblin | 6 | 34 (14 inside camp 2 radius) | 4.16 | |

Totals at entry ≈ 17.9 slimes, 32.6 goblins, 1 chief (+ mini-boss, + due hunts). If the core's map generator differs from the
web one, a spawn may lose more offsets; a jittered spot that is blocked silently drops that monster (QUIRK Q16; recommend FIX: retry
up to 8 times like respawn, then fall back to the anchor).

### 6.4 Event spawns (monster side)

**Random-event ambush** (`ZoneScene.ts:3264-3305`; event data `src/systems/RandomEventSystem.ts:110-121,385-392`), event at the
hero's position: `count = randomInt(ambushCount)` (emerald_plains `[3,5]`); per monster `id = monsterIds[floor(rand×len)]`
(emerald_plains `['slime_green','goblin']`), difficulty-scaled; `angle = rand×2π`, `dist = 3 + rand×2`,
`(round(col+cos×dist), round(row+sin×dist))` clamped to `[1, size−2]`, then `findWalkableTile` (preferred tile, else rings
r = 1..5, row-major within the ring, `:491-525`); state `chase`; no affixes. Event auto-resolves.

**Rescue** (`:3431-3500`): count `max(2, randomInt(min,max) − 1)` (emerald_plains → 2–4), same picking, `dist = 2 + rand×3`
around the rescue NPC; state `chase`; the event completes when all of them are dead.

**Defend waves** (later; `:7018-7052`): `3 + waveIndex` monsters on a circle of radius 8 around the target at angles `2πi/n`,
clamped to `[2, size−3]`, **no walkability check**; random zone monster each; `hp = floor(hp×(1+0.3w))`,
`damage = floor(damage×(1+0.2w))` after difficulty; state `chase`.

**QUIRK Q4:** none of these are added to `questSpawned`, so when killed they **respawn 15 s later at their spawn tile, forever**
(the zone population grows with every ambush/rescue/wave). Recommend FIX: mark them `noRespawn`.

### 6.5 Labyrinth / sub-dungeon spawns (later milestones)

Same `spawnMonsters` loop with the labyrinth scaler (§13); labyrinth monsters never respawn; a seal keeper is placed near the exit
(`ZoneScene.ts:1025-1058`). Sub-dungeons use their own spawn lists and a fixed mini-boss (§8.5); their regular monsters respawn.

---

## 7. Respawn (`ZoneScene.ts:3920-3928`, `5598-5633`)

At the end of the kill hook:
```
if monster is the current miniBossMonster:      miniBossMonster = null            // no respawn
else if questSpawned.has(monster) or inLabyrinth:  questHuntMonsters.delete(huntId) if it is that hunt's leader   // no respawn
else: schedule respawn(monster) in 15000 ms
```
`respawnMonster(dead)`:
```
idx = monsters.indexOf(dead); if idx < 0: return
(c, r) = (dead.spawnCol, dead.spawnRow)
for attempt in 0..7:                          // up to 8 tries, 2 RNG draws each (col, row)
    tc = dead.spawnCol + randomInt(-2,2); tr = dead.spawnRow + randomInt(-2,2)
    if inBounds and walk[tr][tc]:
        if inside any camp radius: continue
        (c, r) = (tc, tr); break
grid.remove(dead)
monsters[idx] = new Monster(dead.originalDefinition, c, r)   // fresh id, full HP, idle, lastAttackTime 0
grid.insert(new)
if originalDefinition.elite: re-roll affixes for the current zone and apply
```
* The fallback (all 8 tries failed) is the old spawn tile without a safe-zone check.
* No check for the hero's proximity: a monster may pop in next to (or on) the hero and aggro immediately.
* **QUIRK Q3 (anchor drift):** the new instance's spawn anchor is the respawn tile, so each respawn random-walks the anchor by up
  to ±2 tiles (bounded only by walkability and camps). Recommend FIX: keep the original anchor (`homeCol/homeRow`) for the jitter
  and leash, i.e. jitter around the zone-entry anchor every time.
* Respawn applies to: regular zone monsters (including `goblin_chief` — the main-quest/story boss respawns every 15 s, Q8),
  sub-dungeon regular monsters, and (Q4) ambush/rescue/defend spawns.

---

## 8. Mini-bosses (`src/data/miniBosses.ts`)

### 8.1 Data
One per zone (`MiniBossByZone`, `:134-140`), fixed spawn tile (`MiniBossSpawns`, `:259-265`), pre-fight dialogue tree
(`MiniBossDialogues`, `:144-255`, `DialogueTree {startNodeId, nodes{id:{id, text, nextNodeId?, isEnd?}}}`). All have
`elite: true, isMiniBoss: true`. The map generator keeps a 5-tile margin around the spawn walkable (`src/data/maps/index.ts:17-18`).

| zone | id | Lv | HP | Dmg | Def | speed | aggro | range | atkSpeed | exp | gold | anim | spawn |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| emerald_plains | `miniboss_goblin_shaman` 哥布林萨满 | 6 | 280 | 16 | 10 | 45 | 7 | 2.5 | 1300 | 90 | 15–30 | humanoid | **(60,55)** |
| twilight_forest | `miniboss_shadow_weaver` 暗影织者 | 14 | 520 | 28 | 16 | 55 | 7 | 3.0 | 1100 | 140 | 22–45 | humanoid | (55,30) |
| anvil_mountains | `miniboss_iron_guardian` 铁甲守卫 | 24 | 1100 | 38 | 35 | 30 | 7 | 2.0 | 1600 | 240 | 38–75 | large | (50,70) |
| scorching_desert | `miniboss_sand_wraith` 沙漠亡灵 | 34 | 1800 | 50 | 30 | 65 | 8 | 3.0 | 1000 | 350 | 60–120 | demonic | (70,50) |
| abyss_rift | `miniboss_void_herald` 虚空先驱 | 44 | 3200 | 65 | 42 | 50 | 8 | 3.5 | 1000 | 450 | 100–200 | demonic | (45,55) |

### 8.2 Spawn — `spawnMiniBoss()` (`ZoneScene.ts:4584-4635`)
On **every** zone entry: `miniBossMonster = null`; in a sub-dungeon spawn that sub-dungeon's boss (§8.5) and return; else take
the zone's def and spawn tile (none → return); difficulty-scale; if the tile is outside the map → return (no walkability check);
create; roll affixes for the zone (always, it is elite); store as `miniBossMonster`. It is **never persisted as killed**: it comes
back on every zone entry (farmable, Q7) and never respawns within a visit.

### 8.3 Pre-fight dialogue — `checkMiniBossDialogue()` (`ZoneScene.ts:4637-4671`), every tick before the AI loop
```
if dialogueActive or no live miniBoss: return
if distSq(hero, boss) > boss.aggroRange²: return               // 7 tiles for the shaman
if seen.has(boss.def.id): return
boss.state = idle; dialogueActive = true; seen.add(id)          // seen BEFORE showing
tree = MiniBossDialogues[id]; if !tree: dialogueActive = false; return
emit MINIBOSS_DIALOGUE { bossName: boss.definition.name, dialogueTree: tree, onDismiss }
onDismiss: dialogueActive = false; boss.state = chase
```
* While active, only the mini-boss is frozen; **the rest of the world keeps running** (other monsters keep attacking).
* `seen` (`miniBossDialogueSeen`) persists in the save (`SaveData.miniBossDialogueSeen: string[]`, `src/data/types.ts:659`) and is
  passed across zone transitions. On zone entry the flag `dialogueActive` resets to false.
* UI (`src/scenes/UIScene.ts:929-931` → `showMiniBossDialogue`): closes panels, dark backdrop (α 0.95), 500×260 panel with red
  accent, header `⚔ {bossName} ⚔`, **all** lines of the linear tree at once (walk `startNodeId → nextNodeId` until `isEnd`),
  button `ui.miniBoss.fight` ("[ 开战 ]"); clicking the button **or the backdrop** dismisses.
* Header uses the raw zh `definition.name`, and the dialogue lines exist only in zh-CN (QUIRK Q10; FIX: i18n keys
  `data.miniboss.<id>.line<n>`).
* Shaman lines (`miniBosses.ts:145-164`): "哈哈哈……又一个不自量力的人类闯入了我的领地。你以为消灭几只小哥布林就能阻止我们吗？" /
  "我是部落的萨满祭司，这片翡翠平原上的灵脉之力已被我掌控。你们的村庄不过是待宰的羔羊！" /
  "来吧，让我用古老的咒术将你化为灰烬！这片土地的秘密，你永远无法触及！"

### 8.4 Loot / rewards
Standard kill hook (§11) with `isMiniBoss` → guaranteed magic+ equipment (rare+ for sub-dungeon bosses), elite drop rates, affix
loot bonus. No special story trigger for any zone mini-boss.

### 8.5 Sub-dungeon mini-bosses (later)
`SubDungeonMiniBosses` (`src/data/subDungeons.ts:54-104`): `sub_mine_guardian` (sub_dwarf_mine, parent anvil_mountains) and
`sub_altar_keeper` (sub_demon_altar, parent abyss_rift), fixed tile from the sub-dungeon data, difficulty-scaled, affixes rolled
**with the parent zone's count**, `isSubDungeonMiniBoss: true`. No dialogue (no tree). Chapter 1 has no sub-dungeon.

---

## 9. Quest hunts (`src/systems/QuestHunts.ts`, spawning in `ZoneScene.ts:6587-6665`)

Named quest monsters (bounties, tracked quarries, lair beasts) built from a regular monster.

### 9.1 Data — `QuestHunt` (`src/data/types.ts:364-380`), listed in `QuestDefinition.hunts?`

| Field | Type | Meaning |
|---|---|---|
| `huntId` | string | Unique monster id used as the kill objective's `targetId`; i18n `data.monster.<huntId>`. |
| `monsterId` | string | Base monster (rig, attacks, stats base). |
| `name` | string | zh-CN display name (fallback for i18n). |
| `col`, `row` | int | Spawn tile. |
| `hpMul?` | number | Default 4. |
| `dmgMul?` | number | Default 1.5. |
| `revealAfterPrevious?` | bool | Stay hidden until every objective **before** its kill objective is complete. |
| `minions?` | `{monsterId, count}` | Pack spawned around it. |

### 9.2 Due logic (pure)
* `huntObjectiveIndex(quest, hunt)` = first objective with `type == 'kill' && targetId == huntId`, or −1 (`:15-17`).
* `isHuntDue(quest, progress, hunt)` (`:20-32`): `progress.status == 'active'`; index ≥ 0; that objective's `current < required`;
  if `revealAfterPrevious`, every objective `i < idx` has `current ≥ required`.
* `huntsToSpawn(openQuests, zoneId, presentHuntIds)` (`:35-45`): for each open quest (in `getActiveQuests()` order) with
  `quest.zone == zoneId` and hunts, each hunt (definition order) not present and due.

### 9.3 Definition — `makeHuntDefinition(base, hunt, name)` (`:48-65`)
```
{ ...base, id: huntId, name,                       // name = getMonsterName(huntId, hunt.name) (localized at spawn)
  hp: round(base.hp × (hpMul ?? 4)), damage: round(base.damage × (dmgMul ?? 1.5)), defense: round(base.defense × 1.2),
  expReward: round(base.expReward × max(3, hpMul ?? 4)), goldReward: [base.gold0 × 3, base.gold1 × 3],
  aggroRange: max(base.aggroRange, 7), elite: true, isMiniBoss: true }
```
Speed, attackRange, attackSpeed, level, spriteKey, animCategory are inherited. Then `DifficultySystem.scaleMonster`.

### 9.4 Spawning — `spawnQuestHunts(announce)` (`ZoneScene.ts:6603-6652`)
```
if inLabyrinth: return
drop dead entries from questHuntMonsters
for (quest, hunt) in huntsToSpawn(activeQuests, zone, keys(questHuntMonsters)):
    base = zoneDefs.find(hunt.monsterId) ?? getMonsterDef(hunt.monsterId); if !base: continue
    spot = walk[hunt.row][hunt.col] ? (col,row) : findWalkableNear(col,row, 6); if !spot: continue   // retried on the next trigger
    def = scaleMonster(makeHuntDefinition(base, hunt, localizedName), difficulty)
    leader = new Monster(def, spot); roll + apply affixes (always); visual scale ×1.25
    add; questSpawned.add(leader); questHuntMonsters[huntId] = leader
    if hunt.minions and base(minion) exists:
        mdef = scaleMonster(minionBase, difficulty)              // NO affixes, even if elite
        repeat count: c = spot.col + randomInt(-3,3); r = spot.row + randomInt(-3,3)   // no clamp
                      if !walk[r][c]: continue                                         // one attempt each
                      minion = new Monster(mdef, c, r); add; questSpawned.add(minion)
    if announce: log t('zone.quest.huntRevealed', {name}) ("{name} 现身了！"); camera shake 260 ms, intensity 0.004
```
`findWalkableNear(col,row,radius)` (`:6655-6665`): rings `r = 1..radius`, iterate `dr = −r..r` (outer), `dc = −r..r` (inner),
only cells with `max(|dc|,|dr|) == r`; first walkable wins; the centre tile itself is never returned.

Triggers: zone entry (`announce = false`, `:515`), `QUEST_ACCEPTED` for a quest of this zone (`false`, `:6587-6594`),
**every** `QUEST_PROGRESS` (`true`, `:6597-6600`). Leaders and minions never respawn. Minions are not tracked: a zone re-entry
with the hunt still due spawns a fresh leader **and** a fresh pack; if the leader dies, its minions stay until the zone unloads.
Kill credit: the leader's `definition.id` is the `huntId` (does **not** count for `goblin` kill objectives); minions are plain
`goblin`s (they do count).

Quest guide (`src/systems/QuestGuide.ts:93-98`): a kill objective points to the nearest living monster with that id, else the
nearest map spawn of that id, else the hunt's `(col,row)`, else the quest area.

### 9.5 Chapter-1 hunts

| Quest | Hunt | Base | Spot | hpMul / dmgMul | Gate | Minions | Final (normal) | Related objectives |
|---|---|---|---|---|---|---|---|---|
| `q_lost_pendant` 偷挂坠的贼 (side, Lv4, prereq `q_kill_slimes`) | `hunt_pendant_thief` 小贼斯尼克 | goblin | (60,48) (walkable) | 3 / 1.2 | `revealAfterPrevious` — after 3 `investigate_clue`s (30,24), (42,34), (54,44) | 2× goblin | HP 165, dmg 10, def 5, exp 54, gold 9–18, aggro 7 | kill it (obj 3); collect `mat_pendant` from it with drop chance 1.0 (obj 4) |
| `q_bandit_trouble` 悬赏：红帽格鲁克 (side, Lv7, prereq `q_kill_goblins`) | `hunt_redcap_gruk` 红帽格鲁克 | goblin | (82,72) (walkable) | 5 / 1.6 | none — appears on accept (silently) | 4× goblin | HP 275, dmg 13, def 5, exp 90, gold 9–18, aggro 7 | kill it (obj 0) |

Both spots have all 49 minion offsets walkable on the generated map. The red-cap camp overlaps regular spawn #9 (80,75).

---

## 10. Boss intros and boss bar (monster side of `StoryDirector`, `src/systems/StoryDirector.ts`)

Data `BOSS_INTROS` (`src/data/story/script.ts:391-399`): `{monsterId, name (i18n key), epithet (i18n key), cutscene}`.
Chapter 1: `goblin_chief` → name `story.boss.goblin_chief.name` = 碎牙·格罗克 / "Grokk Brokentooth", epithet 灰烬部落之主 /
"Chief of the Ashen Tribe", cutscene `cs_boss_goblin_chief` (`script.ts:307-317`: focus camera on the chief 800 ms → title card →
chief says `story.cs_boss_goblin_chief.3` → shake 0.006/400 ms → villain whisper `.5` → focus back on the hero 600 ms).

`StoryDirector.update(dt)` (`:297-325`), scanning every **250 ms** (accumulator):
```
near = argmin over intros of dist(hero, nearestAlive(intro.monsterId))      // nearest alive instance of each id
if near and near.d <= 14 (BOSS_BAR_RANGE) and that instance not yet renamed and has a label:
    rename label to t(intro.name), colour #ffcf6a, alpha 1                 // overrides the affix name/orange
if near and near.d <= 9 (BOSS_SIGHT) and !story.has('boss_<id>'):  enqueue story beat 'boss_<id>' → play intro.cutscene
if near and near.d <= 14: if bossBarFor != id → emit BOSS_BAR {name, epithet, hp: () => alive ? {hp,maxHp} : null}
else if bossBarFor: emit BOSS_BAR null
```
* The intro plays once per save (story beat ids persist in `storySeen`); while it plays the world is frozen (`cinematic`, §3.9)
  and pending monster strikes abort at contact.
* `onMonsterKilled(defId)` (`:103-112`) fires `monster_killed` story triggers (none in Chapter 1; `demon_lord`, `werewolf_alpha`
  later) and clears the bar if it showed that id. A respawned chief within 14 tiles brings the bar (and the gold name) back on the
  next scan.
* The bar UI (`UIScene.ts` `handleBossBar`/`updateBossBar`): 560×14 px at the top centre under the target frame, name above
  (18 px, `#ffe2a8`), epithet below (12 px), fill `0xb3121e`, polls `hp()`. 3D: UMG widget, same data contract.

---

## 11. Kill hook / drops contract (`onMonsterKilled`, `ZoneScene.ts:3789-3929`)

Order (details of each step: `combat-feel.md` §13.1, loot spec, quest spec):
1. Labyrinth kill hook (later). 2. Clear the monster's status effects. 3. Hero Spirit `'kill'`.
4. `exp = floor(def.expReward × (1 + homeExpBonus/100 + eq.expBonus/100))`; `gold = randomInt(goldMin, goldMax)`.
5. Pet exp `onKill(def.level)` (later); mercenary exp share.
6. `killHealPercent`; VFX + floating `+exp`/`+gold`.
7. Counters: `totalKills++`, achievements `'kill'` (any) and `'kill', def.id`; `questSystem.updateProgress('kill', def.id)`
   (synchronously emits `QUEST_PROGRESS` → may reveal hunts, §9.4); `storyDirector.onMonsterKilled(def.id)`; Ember Tower embers
   (later); difficulty completion (`demon_lord` in `abyss_rift`).
8. Ley-fruit drop `rand < (elite ? 0.12 : 0.015)` (`PetSystem.ts:98-100`).
9. Quest drops `rollQuestDrops` (`:4073-4094`): per active quest **of this zone**, per unfinished collect objective whose source is
   not `gather` (and `craft_collect` only with an explicit source): `chance = questDropChance(obj, def.id)`
   (`src/systems/QuestRewards.ts:136-142`: `drop` source → `chance` if the id is listed else 0; no source → 0.25 but only the first
   such objective per quest rolls); `rand < chance` → item flies to the hero + `updateProgress(type, targetId)`.
   Chapter 1: `mat_slime_gel` 0.5 from `slime_green`; `mat_pendant` 1.0 from `hunt_pendant_thief`.
10. Loot: `LootSystem.generateLoot(def, luck, affixLootBonus, difficulty)` (`src/systems/LootSystem.ts:31-80`) with
    `luck = hero.lck + homestead MF (+ labyrinth MF)`, `affixLootBonus = Σ affix lootQualityBonus (+ labyrinth bonus)`.
    Uses only `level`, `elite`, `isMiniBoss`, `isSubDungeonMiniBoss` from the def — **not `lootTable`**. Potions become auto-pickups.
11. Log `zone.monsterKill` with `getMonsterName(def.id, def.name)`.
12. Respawn decision (§7).

`MONSTER_DIED` is not emitted by this flow (QUIRK in `combat-feel.md` §13.1; FIX there).

Core contract: `MonsterKilled { EntityId id; DefId defId; int level; bool elite, isMiniBoss, isSubDungeonMiniBoss; int affixLootBonus;
Vec2 pos; MonsterRole role; }` dispatched synchronously to subscribers in the order above (order matters: quest progress before
quest drops; hunts may spawn during step 7).

---

## 12. Spatial index — `SpatialGrid<T>` (`src/systems/SpatialGrid.ts`)

Uniform grid over the map, `cellSize = 16` tiles (ZoneScene uses 16, `ZoneScene.ts:386`), `gridCols = ceil(cols/16)`,
`gridRows = ceil(rows/16)` (120×120 → 8×8 cells). Entities keyed by `id`.

* `cellIndex(col,row) = clamp(floor(row/16),0,gridRows−1) × gridCols + clamp(floor(col/16),0,gridCols−1)` (`:57-61`).
* `insert`, `remove` (by id), `update` (moves cell only when the cell changed; inserts if unknown), `clear` (`:66-106`).
* `queryRadius(col,row,r)` (`:117-140`): scan the clamped cell rectangle covering `[col−r, col+r] × [row−r, row+r]`; return
  entities with `dx²+dy² ≤ r²` (inclusive), order = cell row-major then per-cell insertion order. Dead entities are included
  (callers filter).
* `findNearest(col,row,maxR, filter?)` (`:148-179`): same scan; `dSq ≤ maxR²` and strictly smaller than the best (first found
  wins ties).
* Updated after every AI move (`ZoneScene.ts:1472`) and teleport (`:6020`); dead monsters stay until respawn (`remove` + `insert`).

Queries using it: activity set (30), monster swings (12), mercenary victims (12), click picking `findMonsterAt` (radius 2 then
`|Δcol| < 1.5 && |Δrow| < 1.5`, `:5652-5659`), nearest alive/aggro monster for hero targeting (radius `max(cols,rows)`,
`:5636-5650`), skill AoEs, taunt, pet lookups.
Core: keep the same API (a templated grid, no RTTI); the result order only matters for RNG-draw order in tests.

---

## 13. Later milestones (keep the systems general)

### 13.1 Other zones' regular monsters (`src/data/monsters/*.ts`) — export with the same schema

| zone | id (zh) | Lv | HP | Dmg | Def | speed | aggro | range | atkSpeed | exp | gold | elite | anim |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| twilight_forest | skeleton 骷髅兵 | 8 | 100 | 14 | 7 | 70 | 6 | 1.5 | 900 | 35 | 6–12 | | humanoid |
| | zombie 僵尸 | 10 | 180 | 12 | 12 | 30 | 4 | 1.2 | 2000 | 45 | 8–16 | | humanoid |
| | werewolf 狼人 | 13 | 240 | 22 | 14 | 80 | 7 | 1.8 | 800 | 65 | 12–24 | | beast |
| | werewolf_alpha 狼人首领 | 16 | 650 | 35 | 20 | 75 | 8 | 2.0 | 750 | 160 | 25–50 | ✔ | beast |
| anvil_mountains | gargoyle 石像鬼 | 18 | 320 | 28 | 20 | 65 | 7 | 2.0 | 1100 | 110 | 18–35 | | flying |
| | stone_golem 石魔像 | 21 | 550 | 24 | 32 | 25 | 5 | 1.5 | 2200 | 140 | 22–45 | | large |
| | mountain_troll 山岭巨魔 | 25 | 1400 | 45 | 30 | 35 | 8 | 2.5 | 1800 | 280 | 40–80 | ✔ | large |
| scorching_desert | fire_elemental 火元素 | 28 | 480 | 35 | 22 | 55 | 7 | 3.5 (ranged) | 1800 | 180 | 30–60 | | flying |
| | desert_scorpion 沙漠蝎 | 30 | 650 | 40 | 28 | 60 | 6 | 1.8 | 1000 | 210 | 35–70 | | beast |
| | sandworm 沙虫 | 32 | 900 | 36 | 34 | 30 | 5 | 2.5 | 2000 | 240 | 38–75 | | serpentine |
| | phoenix 凤凰 | 36 | 2200 | 58 | 36 | 70 | 9 | 3.0 (ranged) | 900 | 400 | 70–140 | ✔ | flying |
| abyss_rift | imp 小恶魔 | 38 | 620 | 42 | 26 | 85 | 7 | 1.5 | 700 | 260 | 45–90 | | flying |
| | lesser_demon 下级恶魔 | 41 | 1000 | 52 | 35 | 50 | 7 | 2.0 | 1200 | 320 | 55–110 | | demonic |
| | succubus 魅魔 | 44 | 850 | 58 | 30 | 60 | 8 | 3.5 (ranged) | 1100 | 380 | 60–120 | | demonic |
| | demon_lord 魔王 | 48 | 4500 | 75 | 48 | 45 | 10 | 3.0 (ranged) | 1000 | 500 | 120–250 | ✔ (bossSkills data only) | large |

Story bosses with intros later: `werewolf_alpha`, `mountain_troll`, `phoenix`, `demon_lord`, `dungeon_abyss_lord` (`script.ts:391-399`).
Beast `attackDuration 250` means `beast` monsters with `attackSpeed < 277.8` would compress — none today.

### 13.2 Labyrinth scaling (`src/systems/DungeonSystem.ts:358-411`)
* `raiseToLevel(def, L)`: unchanged if `def.level ≥ L − 6`; else `m = L / max(1, def.level)`: level L, `hp round(hp×m^1.1)`,
  `damage round(×m^0.95)`, `defense round(×m^0.9)`, `exp round(×m^1.1)`, gold bounds `round(×m)`.
* `scaleMonster(def, cfg, diff)`: raise to `cfg.levelTarget`; curse `speedMul` → `speed round(×s)`, `attackSpeed round(/s)`;
  `hp round(hp × cfg.hpMultiplier × diff.hp)`, damage/defense likewise; `exp round(exp × (1+(floor−1)×0.15) × diff.exp)`;
  gold `round(g × (1+(floor−1)×0.1))`.
* `makeGatekeeper`: scaled; `hp × (base mini-boss/elite ? 1.6 : 4)`, `damage × 1.3`, `exp × 4`, gold × 3, `aggro max(·,8)`, elite,
  isMiniBoss, id `GATEKEEPER_ID`, themed name; drawn ×1.3; spawned at `exit + (start − exit) × 0.22` (rounded) or the nearest
  walkable within 10; never respawns.
* Regen curse: alive monsters not damaged for 3000 ms heal `maxHp × regenPerSec × dt_s` (`ZoneScene.ts:1179-1190`, uses `lastDamagedAt`).
  Volatile corpses: burst on kill (`:1085,1193`). Curse elite chance: non-elite floor monsters roll affixes with `eliteChance`.

---

## 14. Core API and events (proposal)

```
// Core (no UE): AbyssCore::Monsters
struct MonsterSpawnParams { DefId def; Vec2i tile; MonsterRole role; std::string huntId; float visualScale = 1; };
EntityId MonsterSystem::Spawn(const MonsterDef& scaledDef, const MonsterSpawnParams&);   // applies affixes when rolled by caller
void     MonsterSystem::Tick(SimTime now, double dtMs, const HeroView& hero, const WalkGrid&, const StatusQuery&);
HitWeight MonsterSystem::ApplyDamage(EntityId, double amount, std::optional<Vec2> from, DamageFlags);  // emits MonsterKilled once
void     MonsterSystem::Heal(EntityId, double);
```
Events to the UE layer (one per change; UE never writes core state):

| Event | Payload | UE use |
|---|---|---|
| `MonsterSpawned` | id, defId, pos, heading, displayNameKey/affix names, elite, affix types, role, visualScale, isStoryBoss | spawn/pool actor, crown, aura |
| `MonsterMoved` (or per-tick snapshot) | id, pos, heading, speed (tiles/s) | interpolate transform, locomotion blend |
| `MonsterStateChanged` | id, state | idle/walk anim, minimap dot colour |
| `MonsterAttackStarted` | id, targetPos, contactMs, windupMs, isRanged | montage at computed rate, telegraph tint |
| `MonsterProjectileLaunched` | id, from, to, flightMs, color | Niagara projectile |
| `MonsterDamaged` | id, amount, weight, from, isCrit, isTick, hpRatio | flash, hit-react, HP bar, numbers |
| `MonsterHealed` | id, hpRatio | HP bar |
| `MonsterKilled` | §11 | death anim/throw, loot visuals |
| `MonsterRespawned` | old id, new id, pos | replace actor |
| `MonsterTeleported` | id, from, to | purple puffs |
| `HuntRevealed` | huntId, name | log + camera shake |
| `MiniBossDialogue` | bossDefId, nameKey, lines | modal; UE calls `DismissMiniBossDialogue()` |
| `BossBar` | {nameKey, epithetKey, monster id} or null | boss bar widget (poll HP from snapshot) |
| `MonsterRenamed` | id, nameKey, colour | story boss gold label |

---

## 15. Render-only items → 3D equivalents

| Web (render-only) | Source | 3D equivalent |
|---|---|---|
| Container at `cartToIso(col,row)`, depth `y+50` | `Monster.ts:98-100,265-272` | Actor at `(col×T, row×T, groundZ)`; interpolate between 60 Hz sim states; real depth. |
| 2 views (se/ne) + horizontal mirror, facing hysteresis 0.28 | `CharacterAnimator.ts:74-90,506-525` | Continuous yaw toward core `heading` (move) / hero (attack); smooth turn ≈ 720°/s; no hysteresis needed. |
| Sheet anims idle/walk/attack/hurt/death at fixed fps | `SpriteGenerator.ts:868-877` | Skeletal AnimBP: Idle/Walk blend by speed (avoid foot sliding; web walk rate is fixed 10 fps), Attack montage, additive HitReact, Death. |
| Wind-up telegraph tint `0xffc4b0` / elite `0xff7a5c` for `0.62×contact` | `Monster.ts:348-353` | Emissive overlay pulse on the mesh for the wind-up. |
| White flash, recoil, hit-stop (anim pause) | `Monster.ts:313-321` | Overlay material flash; additive flinch; pause montage/anim for `targetStopMs` (visual only). |
| Death throw + fade, sprite destroyed | `combat-feel.md` §13.2 | Knockback impulse ≈0.35 tile away from last hit + death anim + dissolve; despawn/pool at end. |
| HP bar 40×4 px above head (hidden at full HP; colours §5) | `Monster.ts:125-131,324-334` | World-space UMG widget on a head socket; same visibility/colour rules. |
| Name label (12 px Cinzel; `#cccccc`, elite `#e74c3c`, affix elite `#ff6600` always shown, story boss `#ffcf6a`) | `Monster.ts:133-139,410-412`; `StoryDirector.ts:309-313` | Same widget; text from i18n `data.monster.<id>` (FIX Q11) + localized affix names. |
| Elite crown icon above elites | `Monster.ts:141-148` | Small icon/billboard above the head. |
| Affix auras (pulsing ground ellipse in affix colour, per-affix particles) | `Monster.ts:421-549` | Ground decal ring + Niagara per affix (`combat-feel.md` §17.4). |
| Hunt leaders drawn ×1.25, gatekeepers ×1.3 | `ZoneScene.ts:6622-6624`, `:1051-1054` | Actor scale (render-only; gameplay radius unchanged). |
| Elite size 48 vs 36 px (only offsets the bar/label) | `Monster.ts:106` | Widget height from the mesh bounds. |
| Minimap dots: aggro red `0xff4444` α0.9 r2, else `0xcc6644` α0.5 r1.5, all alive monsters (no fog filter) | `UIScene.ts:2975-2985` | Same on the minimap widget. |
| Teleport puffs (r 12 → ×2.5, 400 ms, `0xaa44ff`) | `ZoneScene.ts:6008-6031` | Niagara burst at both ends. |
| Hunt reveal camera shake 260 ms / 0.004 | `ZoneScene.ts:6649` | Camera shake asset. |
| Sheet generation/caching per zone | `SpriteGenerator.ts:299-301` | Soft-referenced meshes/anims, preloaded for the zone's roster on zone load. |
| Mini-boss dialogue modal, boss bar | `UIScene.ts` | UMG modal / boss bar. |
| No spawn VFX (monsters pop in) | — | Optional short fade-in (render-only). |

---

## 16. Web quirks (summary)

| # | Quirk | Source | Recommendation |
|---|---|---|---|
| Q1 | Leash fires one tick only: monster pinned at the 8-tile ring, never attacks, heals 1 %/tick on alternate ticks (30 %/s at 60 Hz, frame-rate dependent), barely walks home | `Monster.ts:166-174` | **FIX**: `returning` state (§3.5); keep parity mode for tests |
| Q2 | Damage never aggroes | `Monster.ts:291-322` | Optional FIX: 5 s "provoked" chase |
| Q3 | Respawn anchor drifts ±2 per respawn | `ZoneScene.ts:5604-5626` | FIX: keep the original anchor |
| Q4 | Ambush/rescue/defend spawns respawn forever | `ZoneScene.ts:3294-3298,3484-3487,7045-7048` | FIX: `noRespawn` |
| Q5 | Shaman `attackRange 2.5` resolves as melee although its art hurls spirit-fire | `miniBosses.ts:18`, `ZoneScene.ts:2949` | Decide: keep melee (parity) or set 3.0 (ranged) |
| Q6 | No pathfinding/sliding/separation: monsters stick on walls, packs stack on one point | `Monster.ts:241-275` | Keep core rule; optional separation steering (render-only offset or core) — decide |
| Q7 | Mini-boss returns on every zone entry (farmable), never persisted | `ZoneScene.ts:4584-4635` | Keep (D2-like) unless design says otherwise |
| Q8 | Story boss `goblin_chief` is a regular spawn and respawns every 15 s | `emerald_plains.ts:27`, `ZoneScene.ts:3927` | Decide: keep or make it a once-per-visit boss |
| Q9 | Externally forced chase reverts next tick when the hero is beyond `1.5×aggro` | `Monster.ts:210-212` | Keep |
| Q10 | Mini-boss dialogue header/lines are zh-CN only | `ZoneScene.ts:4662`, `miniBosses.ts:144-255` | FIX: i18n keys |
| Q11 | In-world name label uses raw zh `definition.name` (hunts are localized at spawn) | `Monster.ts:134` | FIX: `getMonsterName(id)` |
| Q12 | `lootTable` and `bossSkills` are never read | `LootSystem.ts:31-80` | Keep as data; wire later if designed |
| Q13 | Patrol to a blocked tile never ends | `Monster.ts:192-207` | FIX: 4000 ms patrol timeout |
| Q14 | Tank mercenary is only hit while the hero is dead (swing timer already consumed) | `ZoneScene.ts:2817-2830,6521-6555` | Mercenary spec decides |
| Q15 | No line-of-sight for aggro/shots | §3.7 | Keep for Ch1 |
| Q16 | One placement attempt per zone-spawn monster; blocked jitter drops it | `ZoneScene.ts:4440-4451` | FIX: up to 8 tries, then the anchor |
| Q17 | Vampiric heal doesn't refresh the HP bar | `ZoneScene.ts:3040-3046` | FIX: use `heal()` |
| Q18 | `dex` not recomputed after the swift/frozen speed change | `Monster.ts:404-405` | Keep (parity) |

---

## 17. Data tables to export to JSON (core loads at boot)

Keep the TypeScript field names so the exporter is a straight dump (`attackSpeed` stays in ms).

| File | Source | Shape |
|---|---|---|
| `monsters.json` | `MonstersByZone` (`src/data/monsters/index.ts:9-15` + per-zone files), `AllDungeonMonsters` (`src/data/dungeonData.ts:172`), `MiniBossByZone` (`miniBosses.ts:134-140`), `SubDungeonMiniBosses` (`subDungeons.ts:54`) | `{ "byZone": { zoneId: [defId…] }, "defs": { defId: MonsterDefinition + derived {isRanged, onHitStatus[], projectileColor} } }`; flag where each def came from (`zone`/`miniBoss`/`subDungeonBoss`/`dungeon`) so `getMonsterDef` lookup order can be reproduced |
| `minibosses.json` | `MiniBossByZone`, `MiniBossSpawns` (`:259-265`), `MiniBossDialogues` (`:144-255`) | `{ byZone: {zone: defId}, spawns: {zone: {col,row}}, dialogues: {defId: DialogueTree} }` (+ i18n keys, Q10) |
| `maps/<zone>.json` (map spec owns the file) | `mapData.spawns`, `camps`, `safeZoneRadius`, `playerStart` | `spawns: [{col,row,monsterId,count}]` etc. |
| `quests.json` (quest spec owns the file) | `QuestDefinition.hunts` | `QuestHunt` as §9.1 |
| `story/boss_intros.json` (story spec owns) | `BOSS_INTROS` | `{monsterId, name, epithet, cutscene}[]` |
| `elite_affixes.json` (combat spec owns) | `ELITE_AFFIX_DEFINITIONS`, `ZONE_AFFIX_COUNTS` | — |
| `monster_anim.json` | `CharacterAnimator.ts:94-245` (monster categories) + sheet fps | `{category: {attackDuration, attackContact, attackWindup, hurtDuration, deathDuration, deathStyle}}`, `sheet: {idle:[4,6], walk:[6,10], attack:[4,12], hurt:[2,10], death:[4,6]}` |
| `random_events.json` (events spec owns) | `ZONE_EVENT_DATA` (`RandomEventSystem.ts:110-...`) | monster part: `ambushMonsters`, `ambushCount` |
| `monster_ai.json` (constants) | this spec | see below |

```json
{
  "leashRange": 8, "leashHealFractionPerTick": 0.01, "leashMode": "WebParity",
  "patrolIntervalMs": 3000, "patrolRadius": 2, "arriveEpsilon": 0.1,
  "chaseDropMul": 1.5, "attackExitMul": 1.2, "moveSpeedScale": 0.03, "moveAccel": 6,
  "rangedThreshold": 2.5, "meleeReachMul": 1.35, "meleeReachAdd": 0.5, "swingQueryRadius": 12,
  "aiCullRadius": 30, "activeRefreshMs": 250, "simTickHz": 60,
  "spawnJitter": 3, "respawnDelayMs": 15000, "respawnTries": 8, "respawnJitter": 2, "safeZoneRadiusDefault": 9,
  "hunt": { "hpMul": 4, "dmgMul": 1.5, "defMul": 1.2, "expMulMin": 3, "goldMul": 3, "aggroMin": 7,
            "minionJitter": 3, "spotSearchRadius": 6, "visualScale": 1.25, "revealShakeMs": 260, "revealShakeIntensity": 0.004 },
  "boss": { "sightRange": 9, "barRange": 14, "scanMs": 250, "storyNameColor": "#ffcf6a" },
  "escortChip": { "radiusSq": 16, "intervalMs": 2000, "damageMul": 0.3 },
  "defendChip": { "radiusSq": 9, "intervalMs": 2000, "damageMul": 0.2 }
}
```

---

## 18. Chapter-1 checklist

- [ ] `monsters.json` with `slime_green`, `goblin`, `goblin_chief`; `minibosses.json` with `miniboss_goblin_shaman` + dialogue.
- [ ] MonsterSystem: instance state (§1.2), definition pipeline (§1.3), AI tick (§3.2) incl. leash decision (Q1), movement (§3.4).
- [ ] Scene driver: activity set 30 tiles / 250 ms, safe-zone repel and invisibility, immobilize skip, slow multiplier (§3.8).
- [ ] Swing scheduling + contact 250 ms + melee whiff (§4); escort chip damage (§4.4).
- [ ] Zone spawn (§6.1) on emerald_plains spawn table (§6.3); respawn 15 s (§7); ambush/rescue spawns (§6.4).
- [ ] Mini-boss spawn at (60,55), dialogue once per save (§8).
- [ ] Hunts: pendant thief (gated by 3 clues) + red-cap (on accept), minions, announce on reveal (§9).
- [ ] Boss intro + gold rename + boss bar for `goblin_chief` (§10).
- [ ] Kill hook order and `MonsterKilled` event (§11); quest drops for slime gel / pendant.
- [ ] Spatial grid (§12).
- [ ] UE: actors, AnimBP (idle/walk/attack/hit/death) for 4 meshes (slime, goblin, chief, shaman; hunts reuse goblin at ×1.25),
      telegraph tint, HP bar/name widget, crown, affix auras, minimap dots, mini-boss modal, boss bar.

---

## 19. Unit-test vectors for the C++ core

All at a fixed 60 Hz tick (`dt = 16.6667 ms`), all-walkable grid unless stated, normal difficulty.

1. **Movement ramp** (slime, speed 40, from rest, target far away in a straight line): per-tick step after tick 1 = 0.002000,
   tick 2 = 0.003800 (cumulative 0.005800), tick 10 = 0.013026 (cum 0.082762), tick 30 = 0.019152 (cum 0.427630),
   tick 60 = 0.019964 (cum **1.020323** tiles). Steady state 0.02 tiles/tick = 1.2 tiles/s.
2. **Aggro edge**: goblin spawned at (10,10), idle, hero at (15,10) (`dP = 5 = aggro`) → after 1 tick state chase, position unchanged.
   Hero at (15.01,10) → stays idle.
3. **Chase → attack → chase**: goblin chasing, hero at distance 1.5 → attack (no move that tick); hero moves to 1.79 → stays attack;
   1.81 → chase. (Avoid exact boundaries in tests: in IEEE doubles `1.5×1.2 = 1.7999999999999998`, so a hero at exactly 1.8 already exits.)
4. **Chase drop**: goblin chasing, hero at 7.5 → keeps chasing (moves); 7.5001 → idle with `currentMoveSpeed = 0`.
5. **Leash parity mode**: goblin (maxHp 55, hp 40) spawn (10,10) at (18.5,10) in chase, hero at (20,10): tick → idle,
   col = 18.5 − 0.00275 = 18.49725, hp = 40.55. Next tick → chase (no move). Next → leash again (hp 41.10). Never reaches attack.
6. **Leash FIX mode** (if Q1 resolved): same setup → `returning`, walks to (10,10) with the normal ramp, `isAggro()` false throughout,
   heals 0.6×55 = 33 HP/s, idle on arrival.
7. **Patrol**: idle monster, scripted RNG draws (−2, +1) → on the tick where `patrolTimer` first exceeds 3000 ms (tick 181 when
   `dt = 1000.0/60` is accumulated by addition: tick 180 sums to 2999.999999999995) state patrol with target (spawn−2, spawn+1);
   a blocked target tile → stays idle, timer 0.
8. **Safe zone**: camp (15,15), radius 9; hero at (15,20) (inside), goblin idle at (15,24.5) with aggro 5 → stays idle (hero hidden).
   Goblin in chase at (15,23.9) (inside) → forced idle that tick.
9. **Swing timing**: monster enters attack at t=10 000 with `lastAttackTime=0` → swing starts at t=10 000, strike resolves at 10 250;
   next swing ≥ t + attackSpeed (goblin 11 200).
10. **Melee whiff**: goblin swing, hero at 2.52 at contact → hit; 2.53 → whiff (reach 2.525).
11. **Ranged rule**: `attackRange` 2.5 → melee; 2.5001 → ranged.
12. **Hunt def**: `makeHuntDefinition(goblin, {hpMul 5, dmgMul 1.6})` → hp 275, damage 13, defense 5, exp 90, gold [9,18], aggro 7,
    elite, isMiniBoss, id `hunt_redcap_gruk`; pendant thief → hp 165, damage 10, defense 5, exp 54. Nightmare thief → hp 248.
13. **isHuntDue**: pendant with objectives [0,1] done → false; [0,1,2] done → true; status `available` → false; kill objective done → false.
14. **findWalkableNear** order: centre (5,5) blocked, (4,4) walkable and (5,4) walkable → returns (4,4) (dr=−1, dc=−1 first).
15. **Respawn**: kill a regular goblin at t → new instance at t+15 000 within ±2 of the old anchor (8 tries) or on it; mini-boss,
    hunt leader, hunt minion, labyrinth monster → no respawn.
16. **Elite affix application** (`goblin_chief` + `teleporting`): maxHp 184, damage 15, speed 50, defense 8, str 12, dex 5 (unchanged).
17. **Difficulty**: goblin nightmare → hp 83, damage 12, defense 5, exp 36, gold [6,12]; hell → 110/16/6/54/[9,18].
18. **SpatialGrid**: 120×120, cell 16 → 8×8; `queryRadius(16,16,0)` returns an entity exactly at (16,16) (inclusive), not one at
    (16,16.0001); `findNearest` tie → first encountered in cell row-major/insertion order.
19. **Boss scan**: chief alive at distance 13.9 → bar shown + renamed; at 8.9 and beat unseen → intro enqueued once; killed → bar null.
20. **Escort chip**: 2 aggro goblins within 4 tiles of the escort (HP 200), ticks from t = 0 to t = 4100 ms → hits at 0, 2016.7 and
    4033.3 ms (`now − last > 2000`, strictly; first hit immediate) → 3 hits × 2 dmg × 2 goblins = 12 damage, escort HP 188.

---

## Open questions (decisions for the architecture owner)

1. **Q1 Leash**: adopt the `returning` state (heal 0.6×maxHp/s, ignore hero until home) or keep the web's one-tick leash with its
   pinned, non-attacking, fast-regenerating monster? (Recommended: FIX, keep parity mode behind a flag.)
2. **Q2 Damage aggro**: should a hit from outside `aggroRange` provoke the monster (proposed 5 s provoked chase)?
3. **Q3 Respawn anchor**: keep the zone-entry anchor for respawn jitter and leash, or keep the web's drifting anchor?
4. **Q4 Event spawns**: confirm ambush/rescue/defend monsters must not respawn.
5. **Q5 Goblin Shaman**: keep `attackRange 2.5` as melee (parity) or make it a ranged caster (e.g. 3.0) to match its 3D art?
6. **Q6 Movement in 3D**: keep straight-line grid stepping (parity) or add wall sliding / pathfinding / pack separation? Stacking is
   much more visible in 3D than in the iso sprite view.
7. **Q7/Q8 Bosses**: the zone mini-boss reappears on every zone entry, and the main-quest boss `goblin_chief` respawns every 15 s —
   keep both as farmable, or make the chief a once-per-visit (or once-per-quest) spawn?
8. **Q10/Q11 Localization**: move mini-boss dialogue lines and monster labels to i18n keys (recommended).
9. **Q12**: should `lootTable` / `bossSkills` stay inert data in the port, or is a design pass planned to wire them?
10. **Q13/Q16**: approve the small robustness fixes (patrol timeout 4000 ms; up to 8 placement tries for zone spawns).
11. **Facing/turn rate** (render-only): confirm a smooth yaw turn rate (proposed 720°/s) for monsters in 3D.
12. **Sim tick**: confirm the fixed 60 Hz tick for AI (the web's leash heal, acceleration and patrol timing are per frame).
