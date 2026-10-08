# Port Spec — Classes, Hero Stats, Leveling, Skills, Buffs, Status Effects, Spirit

Status: reference spec for the UE5 rebuild (core C++20 library + thin UE module).
Source of truth: the web game at `src/` (Phaser 3 + TypeScript), branch `claude/unreal-rebuild`.
Every rule below cites `path:line`. "Web quirk" marks behaviour that is almost certainly a bug or an
unfinished feature; section 19 lists each one with a recommended port decision. Unless a decision
says otherwise, **port the web behaviour exactly**.

---

## 0. Conventions used throughout

| Topic | Rule |
|---|---|
| Time | All durations, cooldowns and timestamps are **milliseconds** (`double` in the core, or `int64` ms where integral). The web game uses one global monotonic clock (`scene.time.now`, Phaser's game-loop clock). Buff `startTime`s and cooldown end times are stored as absolute times on that clock and carry across zone changes (`ZoneScene.ts:453`). The core needs one `SimClock` in ms that never resets during a session. **It does not advance while the world is frozen** (story cinematic, labyrinth modal). Every ms value in this spec is on that clock unless §19.1 assigns it to the presentation clock. §19.1 is the binding decision table for every port spec (D13). |
| Space | The core works in **tile units** on the 2D ground plane: `col` (x) and `row` (y), as floats. Distances are Euclidean in tile units (`distanceSq`, `src/utils/IsometricUtils.ts:36-40`). Ranges and radii in skill data are in tiles. |
| Iso pixels | A few web timings are expressed in iso screen pixels (projectile flight). Helper (exact): `isoPx(dc, dr) = sqrt(((dc - dr) * 32)^2 + ((dc + dr) * 16)^2)` where `dc, dr` are tile deltas (tile 64×32, `src/utils/IsometricUtils.ts:3-8`, `src/config.ts:4-5`). Keep this helper in the core so the timings match. |
| Numbers | Use IEEE `double` everywhere the web uses JS numbers. Reproduce the **same summation order** for `tieredScale` (a loop, not a closed form) and the same `floor`/`ceil`/`round` calls. (A check over all 40 skills × levels 1–20 found no float-floor differences, but keep the loop order anyway.) `Math.round` in JS rounds .5 toward +∞: use `floor(x + 0.5)`, not `std::round`, when porting a `Math.round`. |
| RNG | Web uses unseeded `Math.random()` (`src/utils/MathUtils.ts:5-15`). `chance(p)` = `rand01() * 100 < p`. The core should take an injectable seeded RNG stream; this spec gives the **order of draws** for each operation so that tests can script rolls. |
| IDs | Keep every string id (`warrior`, `slash`, `damageReduction`, …) unchanged; they are save-file keys. |
| Text | Player-facing text lives in i18n tables (`data.skill.<id>.name/.desc`, etc.). The `name`/`description` fields in class data are zh-CN fallbacks. |

### What Chapter 1 (emerald_plains, Lv 1–10) needs from this area

- All 3 classes with their full skill lists (data for all 40 skills; tier-3 skills unlock at hero Lv 12, so they are data-complete but rarely reachable in Chapter 1. Tier-2 skills unlock at Lv 6 and are reachable).
- Derived stats, regen, leveling curve, 5 stat points + 1 skill point per level, stat allocation UI, skill-tree investment UI.
- Skill execution for every skill (the hero can reach any tier-1/2 skill by Lv 10), buffs, status effects (burn/freeze/poison/bleed/slow/stun on monsters).
- Spirit / Resonance (it is always on for every class from Lv 1). It **does** affect Chapter 1.
- Chapter-1 monsters (`slime_green`, `goblin`, `goblin_chief`, mini-boss `miniboss_goblin_shaman`) never apply status effects to the hero (their ids/sprite keys match none of the fire/poison/ice keywords, section 13.6). Elite affixes can still slow the hero (cosmetic only, see 13.7).

---

## 1. Data model

### 1.1 `Stats` — primary attributes (`src/data/types.ts:1-8`)

```
struct Stats { double str, dex, vit, int_, spi, lck; }   // web: integers in practice
```
JSON keys: `str, dex, vit, int, spi, lck` (力量/敏捷/体质/智力/精神/幸运).

### 1.2 `SkillScaling` (`src/data/types.ts:16-23`)

| field | type | meaning | default when the field (or the whole `scaling`) is absent |
|---|---|---|---|
| `damagePerLevel` | double | added to `damageMultiplier` per level | **0.05** (`CombatSystem.ts:143`) |
| `manaCostPerLevel` | double | added to `manaCost` per level | **0.5** (`CombatSystem.ts:150`) |
| `cooldownReductionPerLevel` | double? | ms removed from `cooldown` per level | 0 (`CombatSystem.ts:157`) |
| `aoeRadiusPerLevel` | double? | tiles added to `aoeRadius` per level | 0 (`CombatSystem.ts:165`) |
| `buffValuePerLevel` | double? | added to `buff.value` per level | **0.02** (`CombatSystem.ts:172`) |
| `buffDurationPerLevel` | double? | ms added to `buff.duration` per level | 0 (`CombatSystem.ts:179`) |

Every skill in the shipped data defines `scaling`, and `damagePerLevel`/`manaCostPerLevel` are always
present, so the 0.05/0.5 defaults never fire for current data; keep them for future data.

### 1.3 `SkillSynergy` (`src/data/types.ts:26-29`)
`{ skillId: string, damagePerLevel: double }` — "each level of `skillId` adds `damagePerLevel` (fraction) to this skill's damage multiplier factor".

### 1.4 `SkillDefinition` (`src/data/types.ts:31-58`)

| field | type | notes |
|---|---|---|
| `id` | string | unique within the class |
| `name` / `nameEn` / `description` | string | zh name, en name, zh description (fallbacks for i18n) |
| `tree` | string | tree id (section 2.2) |
| `tier` | int 1..3 | drives unlock gates (section 7) |
| `maxLevel` | int | 20 for every shipped skill |
| `manaCost` | double | base mana at level 1 |
| `cooldown` | double ms | base cooldown at level 1 (0 for passives) |
| `range` | double tiles | targeting reach (+1 tile slack, section 9.3) |
| `damageMultiplier` | double | base multiplier at level 1 (0 = no damage) |
| `damageType` | enum | `physical, fire, ice, lightning, poison, arcane` |
| `aoe` | bool? | AoE skill |
| `aoeRadius` | double? tiles | base AoE radius |
| `buff` | `{stat: string, value: double, duration: double ms}`? | buff applied on cast (section 11) |
| `icon` | string | icon key (render-only; the skill-tree UI actually draws `skill_icon_<id>`, `UIScene.ts:2314`) |
| `scaling` | SkillScaling? | |
| `synergies` | SkillSynergy[]? | |
| `critBonus` | double? | flat crit-chance percentage points added when this skill deals damage |
| `stunDuration` | double ms? | stun/freeze length (fixed; it does **not** scale with level) |

Recommended **additive** fields for the port's JSON (derived from this spec, not present in web data):
`execKind` (section 9.5 table: `teleport | shadow_step | death_mark | slow_trap | buff | aoe | single`),
`passive` (`true` for `dual_wield_mastery`, `unyielding`, `life_regen`), `groundAnchored` (the 6 ids in
`GROUND_AOE_SKILLS`), `projectile` (`fireball`, `ice_arrow`, `poison_arrow`), `statusRule` (section 13.5).
They let the core avoid string matching on ids, but their values must reproduce the id-based web rules exactly.

### 1.5 `ClassDefinition` (`src/data/types.ts:60-68`)
`{ id, name, nameEn, description, baseStats: Stats, statGrowth: Stats, skills: SkillDefinition[] }`.
**Skill order matters**: it sets the hotbar order (section 7.5) and the auto-combat priority (section 9.9).
Registry: `AllClasses = { warrior, mage, rogue }` (`src/data/classes/index.ts:6-10`). Unknown class id falls back to warrior (`ZoneScene.ts:436`).

### 1.6 `ActiveBuff` (`src/systems/CombatSystem.ts:94-101`)
`{ stat: string, value: double, duration: double ms, startTime: double ms, tag?: string }`.
Active while `now - startTime < duration`.

### 1.7 Hero runtime state (`src/entities/Player.ts:19-57`)

| field | init | notes |
|---|---|---|
| `classData` | ctor | |
| `level` | 1 | no level cap anywhere |
| `exp` | 0 | exp **into the current level** (not cumulative) |
| `gold` | 0 | |
| `hp`, `maxHp`, `mana`, `maxMana` | derived at ctor (section 3) | `hp`/`mana` are doubles (regen is fractional); UI shows `ceil` |
| `stats: Stats` | copy of `baseStats` | allocated points are added here |
| `freeStatPoints`, `freeSkillPoints` | 0 | |
| `skillLevels: map<string,int>` | starter levels (section 7.1) | absent key = 0 |
| `skillCooldowns: map<string,double>` | 0 for every skill | **absolute** ready time; not saved |
| `buffs: ActiveBuff[]` | [] | |
| `spirit: SpiritSystem` | per class (section 14) | |
| `moveSpeed` | 120 | iso px/s (movement spec) |
| `attackSpeed` | 1000 | basic-attack interval in ms |
| `attackRange` | **1.5** | tiles; never changed for any class or weapon |
| `baseDamage` | 10 then derived | |
| `defense` | 5 then derived | |
| `autoCombat` | false | |
| `autoLootMode` | `'off'` | loot spec |
| `autoSkillPriority` | class skill ids in definition order (`Player.ts:102`) | |
| `attackTarget` | null | selected monster id |
| `lastAttackTime` | 0 | |

### 1.8 `EquipStats` — the merged bonus bag consumed by this area (`CombatSystem.ts:6-54`)

All fields are doubles, default 0 (`emptyEquipStats`, `CombatSystem.ts:56-73`). Built from gear + gems + sets +
legendaries (inventory/loot spec), then the runtime adds achievement bonuses, the active ley-beast passive
(except `expBonus`/`magicFind`), the altar blessing and Abyss boons (`ZoneScene.getEquipStats`, `ZoneScene.ts:3143-3171`).
Fields: `damage, damagePercent, defense, defensePercent, maxHp, maxHpPercent, maxMana, maxManaPercent,
critRate, critDamage, attackSpeed, lifeSteal, manaSteal, hpRegen, manaRegen, fireDamage, iceDamage,
lightningDamage, poisonDamage, fireResist, iceResist, lightningResist, poisonResist, allResist, moveSpeed,
magicFind, expBonus, cooldownReduction, knockback, str, dex, int, vit, spi, lck, killHealPercent, deathSave,
critDoubleStrike, doubleShot, freeCast, elementalDamagePercent, ignoreDefense, damageReduction, thornsHeal, dodgeCounter`.
Percent-type fields are whole percents (e.g. `attackSpeed: 10` = 10 %).

---

## 2. Classes

### 2.1 Base stats and growth

| class | zh | base str/dex/vit/int/spi/lck | statGrowth (per level) | source |
|---|---|---|---|---|
| `warrior` | 战士 — 近战坦克，擅长肉搏和防御 | 12 / 8 / 10 / 5 / 5 / 5 | 3 / 1 / 2 / 0 / 0 / 0 | `src/data/classes/warrior.ts:3-9` |
| `mage` | 法师 — 远程AOE/控制，擅长元素魔法 | 4 / 6 / 6 / 14 / 10 / 5 | 0 / 0 / 1 / 3 / 2 / 0 | `src/data/classes/mage.ts:3-9` |
| `rogue` | 盗贼 — 高爆发敏捷型，擅长暗杀和陷阱 | 7 / 14 / 6 / 5 / 5 / 8 | 1 / 3 / 0 / 0 / 0 / 2 | `src/data/classes/rogue.ts:3-9` |

**Web quirk Q1: `statGrowth` is never applied to the hero.** Only the balance test simulates it
(`src/__tests__/NumericalBalance.test.ts:44-52`); `Player.addExp` grants only free points (`Player.ts:156-177`).
A hero's primary stats change only through manual allocation (and gear). Export `statGrowth` anyway (decision D1).

### 2.2 Skill trees

| class | tree id | zh (i18n `data.skillTree.<id>`) | en | UI colour (render-only) |
|---|---|---|---|---|
| warrior | `combat_master` | 战斗大师 | Combat Master | `#d4a017` |
| warrior | `guardian` | 守护者 | Guardian | `#f1c40f` |
| warrior | `berserker` | 狂战士 | Berserker | `#cc3333` |
| mage | `fire` | 烈焰 | Fire | `#e74c3c` |
| mage | `frost` | 冰霜 | Frost | `#5dade2` |
| mage | `arcane` | 奥术 | Arcane | `#8e44ad` |
| rogue | `assassination` | 刺杀 | Assassination | `#27ae60` |
| rogue | `archery` | 箭术 | Archery | `#cc8844` |
| rogue | `traps` | 陷阱 | Traps | `#ff6600` |

Sources: `src/i18n/locales/zh-CN.ts:656-664`, `en.ts:660-668`, `UIScene.ts:2037-2046`. Tab order in the UI =
first appearance of the tree in the class skill list; cards in a tab are sorted by tier (stable).

### 2.3 Skill rosters (definition order = hotbar/auto order)

- **warrior** (15): slash, whirlwind, war_stomp, shield_wall, taunt_roar, vengeful_wrath, charge, lethal_strike, dual_wield_mastery, iron_fortress, unyielding, life_regen, frenzy, bleed_strike, rampage.
- **mage** (12): fireball, meteor, blizzard, ice_armor, chain_lightning, mana_shield, fire_wall, combustion, ice_arrow, freeze, teleport, arcane_torrent.
- **rogue** (13): backstab, poison_blade, vanish, multishot, arrow_rain, shadow_step, death_mark, piercing_arrow, poison_arrow, explosive_trap, poison_cloud, slow_trap, chain_trap.

Starter (tier-1) skills at level 1: warrior `slash, shield_wall, life_regen, frenzy`; mage `fireball, ice_armor, mana_shield, ice_arrow`; rogue `backstab, multishot, explosive_trap`.
Starter hotbar (section 7.5): warrior `[slash, shield_wall, life_regen, frenzy]`, mage `[fireball, ice_armor, mana_shield, ice_arrow]`, rogue `[backstab, multishot, explosive_trap]`.

### 2.4 Derived values with base stats (no gear, no allocated points)

| class | lvl | maxHp | maxMana | baseDamage | defense | dodge% | crit% | crit× | hp/s | mp/s | basic-attack base (baseDamage + str×0.5) |
|---|---|---|---|---|---|---|---|---|---|---|---|
| warrior | 1 | 150 | 85 | 19.6 | 9 | 2.4 | 4.1 | 1.55 | 1.0 | 1.5 | 25.6 |
| warrior | 10 | 285 | 157 | 37.6 | 18 | 2.4 | 4.1 | 1.55 | 1.0 | 1.5 | 43.6 |
| mage | 1 | 110 | 152 | 13.2 | 7 | 1.8 | 3.7 | 1.55 | 0.8 | 2.0 | 15.2 |
| mage | 10 | 245 | 224 | 31.2 | 16 | 1.8 | 3.7 | 1.55 | 0.8 | 2.0 | 33.2 |
| rogue | 1 | 110 | 85 | 15.6 | 7 | 4.2 | 6.8 | 1.58 | 0.8 | 1.5 | 19.1 |
| rogue | 10 | 245 | 157 | 33.6 | 16 | 4.2 | 6.8 | 1.58 | 0.8 | 1.5 | 37.1 |

---

## 3. Derived stats (`Player.recalcDerived`, `Player.ts:114-146`)

Called every frame with the merged `EquipStats` (`ZoneScene.ts:1363-1364`), and **without** gear at
construction (`Player.ts:66-69`), after level-up (`Player.ts:164`), after a stat point is spent
(`UIScene.ts:2673`), on zone entry (`ZoneScene.ts:458`) and on save restore (`ZoneScene.ts:4312`).

```
eq = equipStats (or all zeros if absent)
eStr = stats.str + eq.str ; eVit = stats.vit + eq.vit ; eSpi = stats.spi + eq.spi ; eInt = stats.int + eq.int
hp  = 50 + eVit*10 + (level-1)*15
mp  = 30 + eSpi*8 + eInt*3 + (level-1)*8
dmg = 8 + eStr*0.8 + level*2            // fractional, never rounded
def = 3 + eVit*0.5 + level              // fractional, never rounded
spd = 120 ; aspd = 1000
if (equipStats present) {               // only these 4 lines are skipped when called without gear
  hp  = floor((hp + eq.maxHp) * (1 + eq.maxHpPercent/100))
  mp  = floor((mp + eq.maxMana) * (1 + eq.maxManaPercent/100))
  spd = floor(spd * (1 + eq.moveSpeed/100))
  aspd = max(200, floor(aspd * (1 - eq.attackSpeed/100)))
}
maxHp = hp ; maxMana = mp ; baseDamage = dmg ; defense = def
moveSpeed  = floor(spd * spirit.moveSpeedMultiplier)
attackSpeed = aspd
```
Notes:
- `recalcDerived` does **not** clamp current `hp`/`mana` to the new maxima. Regen stops at the max
  (section 4) and heals clamp, but a lowered max (unequipping) leaves `hp > maxHp` until damage.
  Port the same (no clamp) unless decision D2 changes it.
- `eq.defense` / `eq.defensePercent` are **not** in `defense`; they are added inside the damage formula (section 12).
- `attackRange` stays 1.5 (`Player.ts:51`).

### 3.1 Where gear primary stats count (exact matrix)

| consumer | uses gear-inclusive primaries? | source |
|---|---|---|
| maxHp (vit), maxMana (spi, int), baseDamage (str), defense (vit) | yes | `Player.ts:116-124` |
| dodge chance (defender dex) | yes (`stats.dex + eq.dex`) | `CombatSystem.ts:241-242` |
| crit chance (dex, lck) and crit multiplier (lck) | yes | `CombatSystem.ts:250-259` |
| skill stat bonus (`str` for physical, `int` otherwise) | **no** (raw `stats`) | `CombatSystem.ts:267` |
| basic attack `str*0.5` term | **no** (raw) | `CombatSystem.ts:274` |
| HP regen (vit), mana regen (spi) | **no** (raw) | `Player.ts:148-154` |
| Spirit gain (spi) | **no** (raw) | `Player.ts:360` |
| Loot luck bonus (lck) | **no** (raw `player.stats.lck`) | `ZoneScene.ts:3892` |

---

## 4. Resources and regeneration

### 4.1 Per-frame regen (`Player.update`, `Player.ts:213-250`)
Skipped entirely while `hp <= 0` (also skips spirit drain).
```
recovery = campfire ? {hp: 50, mp: 50} : {hp: 1, mp: 1}       // ZoneScene.ts:83-86,1584-1594
if (hero poisoned) recovery.hp *= 0.5                         // ZoneScene.ts:1366-1369
if (mana < maxMana) mana = min(maxMana, mana + (1 + stats.spi*0.1 + eq.manaRegen) * recovery.mp * dt/1000)
if (0 < hp < maxHp) hp = min(maxHp, hp + (0.5 + stats.vit*0.05 + eq.hpRegen) * recovery.hp * dt/1000)
```
- Campfire: hero within **5 tiles** (`distSq <= 25`) of any camp centre → both multipliers ×50 (`CAMPFIRE_RECOVERY_RADIUS = 5`).
- Poison on the hero halves the HP-regen multiplier (base + gear + Life Regen passive). It does not affect potions, life steal or heals.
- `dt` = frame delta in ms.

### 4.2 Warrior passive Life Regen — runs before `Player.update` (`ZoneScene.ts:1372-1379`)
`if (L = skillLevels.life_regen) > 0 and 0 < hp < maxHp: hp = min(maxHp, hp + 2*L * dt/1000 * recovery.hp)` — **linear** (+2 HP/s per level, not tiered), multiplied by the same campfire/poison factor.

### 4.3 Mana spending
- Cast cost = `ceil(getSkillManaCost(skill, L) * spirit.manaCostMultiplier)` (`Player.ts:350-357`); deducted at cast start (`Player.ts:385`, floor at 0).
- `freeCast` (gear %): after the deduction roll `rand01()*100 < eq.freeCast` → refund the full cast cost (clamped to max) (`ZoneScene.ts:2493-2500`, `CombatSystem.ts:462-465`). Cooldown is not refunded.
- Mana potions / HP potions: loot spec (amounts: `c_hp_potion_s/m/l` 50/150/400 HP, `c_mp_potion_s/m` 30/80 MP, `ZoneScene.ts:3900-3905`).
- Mana steal: `+manaStolen` after each player hit (section 12.3).

---

## 5. Leveling

### 5.1 Curve (`Player.expToNextLevel`, `Player.ts:179-182`)
`expToNext(L) = floor(3*L*L + 25*L)` (exp needed to go from L to L+1).

| L | to next | cumulative at start of L | | L | to next | cumulative |
|---|---|---|---|---|---|---|
| 1 | 28 | 0 | | 11 | 638 | 2530 |
| 2 | 62 | 28 | | 12 | 732 | 3168 |
| 3 | 102 | 90 | | 13 | 832 | 3900 |
| 4 | 148 | 192 | | 14 | 938 | 4732 |
| 5 | 200 | 340 | | 15 | 1050 | 5670 |
| 6 | 258 | 540 | | 16 | 1168 | 6720 |
| 7 | 322 | 798 | | 17 | 1292 | 7888 |
| 8 | 392 | 1120 | | 18 | 1422 | 9180 |
| 9 | 468 | 1512 | | 19 | 1558 | 10602 |
| 10 | 550 | 1980 | | 20 | 1700 | 12160 |

There is no level cap.

### 5.2 `addExp(amount)` (`Player.ts:156-177`) — exact algorithm
```
exp += amount
needed = expToNext(level)
if (exp >= needed) {                 // ONE level per call, never a loop
  exp -= needed ; level += 1
  freeStatPoints += 5 ; freeSkillPoints += 1
  recalcDerived()                    // WITHOUT gear
  hp = maxHp ; mana = maxMana        // gear-less maxima (web quirk Q2)
  emit PLAYER_LEVEL_UP {level} ; log sys.player.levelUp {level}
}
emit PLAYER_EXP_CHANGED {exp, needed: expToNext(level)}
```
- Overflow beyond one level stays in `exp` (can exceed the new `expToNext`) and converts on the **next**
  `addExp` call (e.g. a 500-exp quest at L1 → L2 with 472 exp; the next kill pushes L3).
- Q2: the full heal uses gear-less maxima; the next frame's `recalcDerived(eq)` raises `maxHp` but `hp` stays at the gear-less value.
- `PLAYER_LEVEL_UP` also triggers the level-up banner and the level achievement check (`ZoneScene.ts:1289-1293`).

### 5.3 Exp sources
- Kill (`ZoneScene.ts:3796-3801`): `exp = floor(def.expReward * (1 + homeBonus.expBonus/100 + eq.expBonus/100))`, where `homeBonus = merge(homestead.getTotalBonuses(), petSystem.getBonuses())`. `expReward` is already difficulty-scaled at spawn (monster spec). Also +gold `randomInt(goldReward[0], goldReward[1])`.
- Quest reward `reward.exp` and dialogue rewards `exp` are added raw (no bonus) (`ZoneScene.ts:4116`, `ZoneScene.ts:1278`).
- Death toll (soul echo) can remove exp within the current level only, never de-levels (`SoulEcho.ts:30-34`, `ZoneScene.ts:1218-1227`; death spec).

---

## 6. Stat points

- +5 per level-up (section 5.2). No points at level 1.
- Allocation (character panel `C`, `UIScene.ts:2668-2677`): each "+" press: `if freeStatPoints > 0 { freeStatPoints--; stats[key]++; recalcDerived() }` on any of the six stats. One point per press, no undo, no respec, no per-stat cap.
- Current `hp`/`mana` are not changed by allocation (max changes only).
- Character panel derived display (render-only formulas, `UIScene.ts:2684-2696`): crit% shown = `(dex+eq.dex)*0.2 + (lck+eq.lck)*0.5 + eq.critRate` (unclamped, 1 decimal); crit damage shown = `150 + eq.critDamage` % (ignores the LCK term). Attack shows `floor(baseDamage)` (+ flat/percent gear); defense `floor(defense)` (+ gear flat).

---

## 7. Skill progression (`src/systems/SkillProgressionSystem.ts`)

### 7.1 Starter levels (`:48-50`)
New hero: every skill with `tier == 1` at level 1, all others 0.

### 7.2 Gates (`:26-36`)

| tier | min hero level | min points already invested in the same tree | previous-tier requirement |
|---|---|---|---|
| 1 | 1 | 0 | – |
| 2 | 6 | 4 | at least one tier-1 skill of the tree with level > 0 |
| 3 | 12 | 9 | at least one tier-2 skill of the tree with level > 0 |
| other N (future) | `1 + (N-1)*6` | `max(0, (N-1)*5)` | at least one tier N-1 skill learned |

"Invested tree points" = sum of the current levels of all skills in that tree (`:62-70`), **including the free starter levels**.

### 7.3 Investment state (`getSkillInvestmentState`, `:72-107`) — checks in this exact order
1. `currentLevel >= maxLevel` → `maxed`
2. `freeSkillPoints <= 0` → `no_points`
3. `heroLevel < requiredLevel` → `player_level`
4. `treePoints < requiredTreePoints` → `tree_points`
5. `tier > 1` and no learned skill in the same tree with `tier == this.tier - 1` → `previous_tier`
6. otherwise `ready` (`canInvest = true`)

Return `{canInvest, reason, requiredPlayerLevel, requiredTreePoints, investedTreePoints}`.
`investSkillPoint` (`:109-140`) returns a **new** map with `+1` on success and `freeSkillPoints - 1`; on failure returns an unchanged copy. After success the UI emits `SKILL_LEVEL_CHANGED {skillId, level}` (`UIScene.ts:2440-2447`), which refreshes the hotbar (`ZoneScene.ts:1341-1344`).
Lock text keys: `ui.skillTree.lock.playerLevel | treePoints | previousTier | noPoints`.

### 7.4 What a Chapter-1 hero can reach
By Lv 10 a hero has 9 skill points. Tier 2 needs Lv 6 and 4 tree points (the starter level counts, so 3 more points in one tree). Tier 3 (Lv 12, 9 tree points) is out of reach without over-levelling.

### 7.5 Hotbar loadout (`getLearnedSkillLoadout`, `:52-60`)
`loadout = class skills (definition order) filtered to level > 0, first 6`. Keyboard `1`..`6` → loadout[0..5] (`ZoneScene.ts:2207-2211`); gamepad buttons `[0, 2, 3, 5]` → loadout[0..3] (`ZoneScene.ts:2237-2245`); touch buttons emit the skill id. Slots are positional: learning a skill earlier in definition order **shifts** later skills to the next key (web quirk Q3, decision D3). Passives appear on the hotbar (Q4).

---

## 8. Skill scaling formulas (`src/systems/CombatSystem.ts:125-197`)

```
tieredScale(per, L):            // CombatSystem.ts:129-138
  if L <= 1: return 0
  total = 0
  for i in 2..L: total += (i <= 8) ? per : (i <= 16) ? per*0.75 : per*0.5
  return total
dmgMult(s, L)   = s.damageMultiplier + tieredScale(s.scaling.damagePerLevel ?? 0.05, L)
manaCost(s, L)  = floor(s.manaCost + tieredScale(s.scaling.manaCostPerLevel ?? 0.5, L))
cooldown(s,L,cdr)= floor( max(500, floor(s.cooldown - tieredScale(s.scaling.cooldownReductionPerLevel ?? 0, L)))
                          * (1 - clamp(cdr, 0, 50)/100) )
aoeRadius(s, L) = (s.aoeRadius ?? 0) + tieredScale(s.scaling.aoeRadiusPerLevel ?? 0, L)
buffValue(s, L) = (s.buff?.value ?? 0) + tieredScale(s.scaling.buffValuePerLevel ?? 0.02, L)
buffDuration(s,L)= floor((s.buff?.duration ?? 0) + tieredScale(s.scaling.buffDurationPerLevel ?? 0, L))
synergyFactor(s, levels) = 1 + Σ syn.damagePerLevel * levels[syn.skillId]   (1 if no synergies)
castMana(s, L)  = ceil(manaCost(s, L) * spirit.manaCostMultiplier)          // Player.ts:350-357
```
- Weight per level step: levels 2–8 ×1.0, 9–16 ×0.75, 17–20 ×0.5. Sum of weights at L20 = 7 + 6 + 2 = 15.
- Cooldown floor 500 ms applies before CDR; passives (`cooldown 0`) therefore get 500 ms. `cdr` = `eq.cooldownReduction` (clamped 0–50). UI tooltips call `cooldown(s, L)` without CDR.
- Synergy uses the **raw** levels of the source skills (a level-0 source adds 0). Synergies listed on non-damaging skills (`dual_wield_mastery`, `iron_fortress`, `unyielding`, `life_regen`, `teleport`, and on buff-path skills such as `poison_blade`, `vanish`, `shadow_step`) have no effect because those skills never call the damage formula with that skill (or deal only the 1-damage passive hit, Q4).
- `stunDuration` is **not** scaled (descriptions that promise longer stuns are not implemented; Q5).

---

## 9. Skill execution pipeline

### 9.1 Request → buffer (`ZoneScene.requestSkill`, `:2269-2294`; `CombatInputBuffer`, `CombatInputSystem.ts:15-62`)
1. If the skill is unknown or level 0 → log `zone.combat.skillLocked`, stop.
2. `decision = buffer.request(id, now, canExecuteSkill(id, now))`:
   - `canExecute` true → clear the buffer, call `tryUseSkill` now.
   - false → store `{actionId, requestedAt: now, expiresAt: now + 180}` (single slot, newest replaces older); emit `SKILL_BUFFERED {skillId, expiresAt}`.
3. Every frame (after input, `ZoneScene.ts:1362`): `consumeReady(now, canExecute)`: if expired (`now > expiresAt`) drop it; else if `canExecute` → clear and `tryUseSkill`.

### 9.2 `canExecuteSkill(id, now)` (`ZoneScene.ts:2248-2267`)
False if: hero dead; skill unknown; level ≤ 0; `now < skillCooldowns[id]`; `mana < castMana`; `id == teleport` and hero immobilized (freeze/stun).
Then `target = preferredTarget()`:
- no target → true only if the skill has a `buff`, or `aoe`, or is `teleport`;
- target and (buff or aoe or teleport) → true;
- else true iff `distSq(hero, target) <= (range + 1)^2`.

`preferredTarget()` (`:2335-2344`): the selected `attackTarget` if alive, else (clearing a dead selection) the nearest alive monster **anywhere on the map** (`findNearestAliveMonster`, radius = max(cols, rows)).

### 9.3 `tryUseSkill(id, now)` (`:2462-2528`)
1. Abort silently if dead, unknown, not ready. Level 0 → log `skillLocked`.
2. `cost = castMana(skill, L)`; if `mana < cost` → log `zone.combat.manaInsufficient`, abort.
3. `teleport` while immobilized → log `zone.teleport.blockedByCC`, abort.
4. `target = preferredTarget()`; if no target and the skill has no `buff`, no `aoe` and is not `teleport` → abort silently.
5. If target and no `buff` and not `teleport` and not `aoe`: abort silently if `distSq > (range+1)^2`.
   (So **buff skills and AoE skills ignore range**; this includes `death_mark` and `shadow_step`, which have buffs — Q6.)
6. **Commit**: `skillCooldowns[id] = now + cooldown(skill, L, eq.cooldownReduction)`; `mana = max(0, mana - cost)`; emit `SKILL_USED {skillId, damageType}` and `PLAYER_MANA_CHANGED` (`Player.ts:379-388`).
7. `freeCast` roll (section 4.3).
8. Animation and release delay:
   - if `buff` or `aoe` or `range > 2`: `releaseDelay = playCast(target pos)` when `target && !buff`, else `playCast()` (no facing);
   - else (melee single target: `slash, lethal_strike, bleed_strike, backstab`, and the passives `dual_wield_mastery, life_regen`): `releaseDelay = playAttack(target)` if a target exists else `playCast()`.
9. If `id` is `teleport` or `shadow_step`, or `releaseDelay <= 0` → `releaseSkill` immediately with the original target.
   Otherwise schedule `releaseSkill` after `releaseDelay` ms; at that time abort if the hero is dead or a zone transition started (cooldown and mana stay spent); the target becomes `target` if still alive, else `preferredTarget()` again (no range re-check, Q7); the release uses the clock time at release.

### 9.4 Release timings (contact/release beats)
Values for the shipped hero rigs (render-derived, but gameplay-relevant because damage waits for them):

| class | cast release `round(castDuration*0.46)` | melee contact `round(4 * 1000/attackFrameRate * s)` |
|---|---|---|
| warrior | castDuration 725 → **334 ms** | attackFrameRate 13 → **308 ms** at s = 1 |
| mage | 500 → **230 ms** | 15 → **267 ms** |
| rogue | 535 → **246 ms** | 18 → **222 ms** |

- Cast: `CharacterAnimator.playFrameCast`, `CharacterAnimator.ts:934-990` (`chargeMs = castDuration * 0.46`). Presets `CharacterAnimator.ts:187-243`.
- Melee contact: contact frame = `round((8 - 1) * 0.55) = 4` of the 8-frame attack (`src/graphics/sprites/types.ts:23-31`, `CharacterAnimator.ts:862-867`), times `s = attackSpeedScale(attackDuration, hero.attackSpeed) = clamp(attackSpeed*0.9/attackDuration, 0.35, 1)` (`HitFeedback.ts:72-75`; attackDuration warrior 610, mage 535, rogue 445). Only below `attackSpeed ≈ 678 / 594 / 494` ms does `s` drop under 1.
- In UE these are the anim-notify times of the Release / Contact notifies. The core should receive them as parameters (per class, per action) so gameplay timing stays data-driven. Exact values above are the parity targets.

### 9.5 `releaseSkill` branches (`ZoneScene.ts:2530-2806`) — evaluated in this order

| # | condition | behaviour |
|---|---|---|
| 1 | `id == teleport` | section 10 (mage) |
| 2 | `id == shadow_step` and target | section 10 (rogue) |
| 3 | `id == death_mark` and target | section 10 (rogue) |
| 4 | `id == slow_trap` | section 10 (rogue) |
| 5 | skill has `buff` | generic buff (9.6). Also catches `shadow_step`/`death_mark` with no target (Q8) |
| 6 | `aoe` and `aoeRadius(L) > 0` | AoE (9.7) |
| 7 | target exists | single target (9.8) |
| – | otherwise | nothing (cost already paid) |

### 9.6 Generic buff
`hero.buffs.push({stat: buff.stat, value: buffValue(L), duration: buffDuration(L), startTime: releaseTime})`; log `zone.combat.skillActivated {skillName}`. Re-casting while active pushes another entry (stacks, section 11.2). `taunt_roar` extra: see section 10.

### 9.7 AoE
```
R = aoeRadius(skill, L)
anchor = (id in GROUND_AOE) ? findGroundAoeAnchor(target, skill.range) : null
GROUND_AOE = {meteor, blizzard, fire_wall, arcane_torrent, arrow_rain, poison_cloud}   // ZoneScene.ts:95
findGroundAoeAnchor(t, range):           // ZoneScene.ts:2305-2318
  reachSq = (range+1)^2
  if t alive and distSq(hero,t) <= reachSq: return t
  return the alive monster with the smallest distSq <= reachSq (strict '<' keeps the first found), else null
centre = anchor ? anchor.position : hero.position
targets =
  (id == piercing_arrow && target) ? monstersAlongLine(target, skill.range, R*0.6)
                                   : alive monsters with distSq(centre, m) <= R^2
monstersAlongLine(t, range, halfWidth):  // ZoneScene.ts:2321-2333
  d = t.pos - hero.pos ; len = |d| ; if len < 0.001 → [t] if alive else []
  u = d/len ; for each alive m within (range + halfWidth) of hero:
     r = m.pos - hero.pos ; along = r·u ; perp = |r.x*u.y - r.y*u.x|
     keep if 0 <= along <= range and perp <= halfWidth
```
Target list and per-target delays are fixed at release time:
- `aoeDelay = projectileTravelMs(id)` (only `meteor` = **300 ms**, `SkillEffectSystem.ts:9-10,38-48`). If `aoeDelay > 0`, after it apply the hit to every captured target (skipping dead ones), aborting if the hero died or a zone transition started.
- else, per target: `delay = (id in {multishot, piercing_arrow}) ? min(260, isoPx(target - hero) * 1.1) : 0` ms; delay 0 → apply now, else schedule (skip if the hero died / transition / target dead).
- Apply hit (per target): `r = calculateDamage(heroEntity, monsterEntity, skill, L, skillLevels)`; `dmg = r.damage`; if `id == combustion` and the target has `burn` → `dmg = floor(dmg*1.5)`; `monster.takeDamage(dmg, from, {isCrit})`; `applySteal(r)` (section 12.3 — uses `r`, not the ×1.5 value); `applySkillStatusEffect(target, skill, dmg)`; if the monster died → `onMonsterKilled`.
  Knock-back origin: blast skills with an anchor push from the anchor (unless the target is within 4 px of it), arrows from the hero (render/feel only).
- A dodged result (damage 0) is **not** special-cased in skill paths: the 0-damage hit still runs hurt reactions and status effects (Q9).
- If `piercing_arrow` has no target at all it falls back to the caster-centred radius query with R = 1.5.

### 9.8 Single target
Projectile delay `travel = projectileTravelMs(id, hero, target)`:

| id | travel ms |
|---|---|
| `fireball` | `clamp(isoPx * 1.5, 300, 600)` |
| `ice_arrow`, `poison_arrow` | `clamp(isoPx * 1.5, 250, 500)` |
| all others | 0 (instant) |

On arrival (or immediately): skip if the target died, the hero died, or a transition started; then same hit
application as 9.7 (with the combustion rule). No range re-check on arrival.

### 9.9 Auto-combat skill use (`ZoneScene.handleAutoCombat`, `:3071-3108`; toggled with Tab)
Each frame while `autoCombat`: (target acquisition is in the combat spec) then
`for id in autoSkillPriority (class order): if L>0 and ready and mana >= castMana: requestSkill(id); break`.
Only the first eligible skill is requested each frame; if it cannot execute (out of range) it is buffered and nothing else is tried that frame. Passives with level > 0 are eligible (Q4).

---

## 10. Skill catalogue — all 40 skills

Legend: **dmg×** = `damageMultiplier`, **mp** = `manaCost`, **cd** = cooldown ms, **rng** = range (tiles),
**R** = aoeRadius, scaling `d/m/cd/R/bv/bd` = damage/mana/cooldown/radius/buff-value/buff-duration per level.
"Stat" for damage = `str` if `physical`, else `int` (section 12). All `maxLevel = 20`.

### 10.1 Warrior (`src/data/classes/warrior.ts`)

| id | zh / en | tree · tier | dmg× | type | mp | cd | rng | R | buff (stat, value, dur) | other | scaling d / m / cd / R / bv / bd | synergies |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `slash` (:13) | 猛击 / Slash | combat_master · 1 | 1.5 | physical | 8 | 2000 | 1.5 | – | – | – | .18 / .5 / – / – / – / – | whirlwind .08 |
| `whirlwind` (:35) | 旋风斩 / Whirlwind | combat_master · 2 | 1.2 | physical | 15 | 4000 | 2.5 | 2.5 | – | aoe | .14 / 1.0 / 50 / .08 / – / – | slash .06, war_stomp .05 |
| `war_stomp` (:62) | 战争践踏 / War Stomp | combat_master · 3 | 1.8 | physical | 22 | 8000 | 1.0 | 2.0 | – | aoe, stun 2000 | .20 / 1.5 / 80 / .05 / – / – | whirlwind .06 |
| `shield_wall` (:91) | 盾墙 / Shield Wall | guardian · 1 | 0 | physical | 12 | 10000 | 0 | – | damageReduction 0.5, 5000 | – | 0 / .5 / 100 / – / .015 / 150 | – |
| `taunt_roar` (:114) | 嘲讽怒吼 / Taunt Roar | guardian · 2 | 0 | physical | 14 | 12000 | 0 | 3.0 | defenseBonus 0.3, 4000 | aoe | 0 / .5 / 120 / .06 / .02 / 200 | – |
| `vengeful_wrath` (:140) | 复仇之怒 / Vengeful Wrath | guardian · 3 | 0 | physical | 20 | 20000 | 0 | – | damageBonus 0.25, 6000 | – | 0 / 1.0 / 200 / – / .02 / 250 | – |
| `charge` (:165) | 冲锋 / Charge | combat_master · 2 | 2.0 | physical | 14 | 6000 | 5 | – | – | – | .18 / .8 / 60 / – / – / – | slash .06, lethal_strike .08 |
| `lethal_strike` (:189) | 致命一击 / Lethal Strike | combat_master · 3 | 2.8 | physical | 22 | 8000 | 1.5 | – | – | critBonus 15 | .22 / 1.2 / 80 / – / – / – | charge .07, whirlwind .05 |
| `dual_wield_mastery` (:214) | 双持精通 / Dual Wield Mastery | combat_master · 3 | 0 | physical | 0 | 0 | 0 | – | – | passive | 0 / 0 / – | slash .04, lethal_strike .04 |
| `iron_fortress` (:239) | 铁壁 / Iron Fortress | guardian · 2 | 0 | physical | 16 | 14000 | 0 | – | damageReduction 0.4, 6000 | – | 0 / .8 / 120 / – / .018 / 200 | shield_wall .05, taunt_roar .04 |
| `unyielding` (:266) | 不屈 / Unyielding | guardian · 2 | 0 | physical | 0 | 60000 | 0 | – | damageReduction 0.35, 5000 | passive proc | 0 / 0 / 500 / – / .015 / 150 | shield_wall .03, iron_fortress .04 |
| `life_regen` (:293) | 生命回复 / Life Regen | guardian · 1 | 0 | physical | 0 | 0 | 0 | – | – | passive | 0 / 0 | shield_wall .03, unyielding .03 |
| `frenzy` (:318) | 狂乱 / Frenzy | berserker · 1 | 0 | physical | 18 | 15000 | 0 | – | damageBonus 0.2, 8000 | – | 0 / 1.0 / 150 / – / .018 / 250 | bleed_strike .06, whirlwind .05 |
| `bleed_strike` (:345) | 裂伤斩 / Bleed Strike | berserker · 2 | 1.8 | physical | 16 | 5000 | 1.5 | – | – | bleed | .16 / .8 / 50 / – / – / – | frenzy .07, slash .05 |
| `rampage` (:369) | 狂暴冲击 / Rampage | berserker · 3 | 2.2 | physical | 24 | 10000 | 1.5 | 2.5 | – | aoe | .20 / 1.2 / 100 / .06 / – / – | frenzy .08, bleed_strike .06, whirlwind .05 |

Runtime behaviour (what the code actually does):
- **slash** — melee single target (attack anim). Hit = section 9.8.
- **whirlwind** — caster-centred AoE, every alive monster within R of the hero, instant.
- **war_stomp** — caster-centred AoE + `stun` 2000 ms on each hit target (physical + stunDuration rule, 13.5); stun is not level-scaled (Q5).
- **shield_wall**, **iron_fortress** — self buff `damageReduction` (summed, capped 0.9 incl. gear DR).
- **taunt_roar** — self buff `defenseBonus`; additionally every alive monster within `aoeRadius(L)` of the hero gets `{stat:'taunted', value:1, duration: buffDuration(L)}` and, if its AI state is `idle` or `patrol`, is set to `chase`; log `zone.combat.tauntRoar {count}` when ≥1 (`ZoneScene.ts:2677-2695`). The `taunted` buff is never read (Q10).
- **vengeful_wrath**, **frenzy** — self buff `damageBonus` (multiplier on final damage). Neither grants attack speed (Q11).
- **charge** — single-target hit at up to `range+1 = 6` tiles; the hero does **not** move (VFX only, Q12).
- **lethal_strike** — melee single target, +15 crit chance on its own hit.
- **dual_wield_mastery** — passive, each frame (`ZoneScene.ts:1396-1410`): if level > 0 and both `weapon` and `offhand` slots are filled (any offhand, shields included) and no buff tagged `dualWieldMastery` exists → push `{stat:'damageBonus', value: 0.03*L (linear), duration: 2000, startTime: now, tag:'dualWieldMastery'}`. Effectively permanent while equipped; lingers ≤2 s after unequipping.
- **unyielding** — passive proc, each frame (`ZoneScene.ts:1381-1394`): if level > 0, hp > 0, `hp/maxHp < 0.3` and `now >= skillCooldowns.unyielding` → push `damageReduction buffValue(L)` for `buffDuration(L)`, set cooldown `now + cooldown(skill, L, cdr)`, log `zone.combat.unyieldingProc`. Never on cooldown at start, so the first drop below 30 % procs immediately.
- **life_regen** — passive (+2 HP/s per level, section 4.2).
- **bleed_strike** — melee single target + `bleed` (25 % of the hit per second for 5 s).
- **rampage** — caster-centred AoE, instant. The described "+15 % per extra enemy" is not implemented (Q13).

### 10.2 Mage (`src/data/classes/mage.ts`)

| id | zh / en | tree · tier | dmg× | type | mp | cd | rng | R | buff | other | scaling d / m / cd / R / bv / bd | synergies |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `fireball` (:13) | 火球术 / Fireball | fire · 1 | 1.8 | fire | 10 | 2000 | 6 | – | – | projectile | .20 / .8 / – | meteor .14 |
| `meteor` (:35) | 陨石 / Meteor | fire · 3 | 2.5 | fire | 35 | 8000 | 5 | 2.5 | – | aoe, ground, 300 ms fall | .28 / 2.0 / 100 / .06 | fireball .12 |
| `blizzard` (:63) | 暴风雪 / Blizzard | frost · 2 | 1.0 | ice | 20 | 5000 | 5 | 3 | – | aoe, ground | .16 / 1.2 / 60 / .08 | ice_armor .10 |
| `ice_armor` (:89) | 冰甲 / Ice Armor | frost · 1 | 0 | ice | 12 | 15000 | 0 | – | damageReduction 0.2, 10000 | – | 0 / .5 / 150 / – / .012 / 300 | – |
| `chain_lightning` (:114) | 连锁闪电 / Chain Lightning | arcane · 2 | 1.3 | lightning | 18 | 3500 | 5 | 4 | – | aoe | .16 / 1.0 / 40 / .10 | mana_shield .08 |
| `mana_shield` (:140) | 魔法盾 / Mana Shield | arcane · 1 | 0 | arcane | 15 | 12000 | 0 | – | manaShield 0.3, 8000 | – | 0 / .8 / 120 / – / .015 / 250 | – |
| `fire_wall` (:165) | 火墙 / Fire Wall | fire · 2 | 1.2 | fire | 18 | 6000 | 5 | 2.0 | – | aoe, ground | .14 / 1.0 / 60 / .06 | fireball .08, combustion .06 |
| `combustion` (:192) | 燃爆 / Combustion | fire · 3 | 2.0 | fire | 24 | 7000 | 5 | – | – | ×1.5 vs burning | .22 / 1.4 / 70 | fireball .10, fire_wall .08, meteor .06 |
| `ice_arrow` (:219) | 冰箭 / Ice Arrow | frost · 1 | 1.6 | ice | 10 | 2500 | 6 | – | – | projectile | .16 / .6 / 30 | blizzard .08, freeze .06 |
| `freeze` (:243) | 冰封 / Freeze | frost · 3 | 0.8 | ice | 22 | 10000 | 5 | – | – | freeze 2000 | .10 / 1.2 / 100 | ice_armor .06, blizzard .08, ice_arrow .05 |
| `teleport` (:271) | 传送 / Teleport | arcane · 2 | 0 | arcane | 16 | 8000 | 6 | – | – | blink | 0 / .5 / 80 | mana_shield .04, chain_lightning .04 |
| `arcane_torrent` (:295) | 奥术洪流 / Arcane Torrent | arcane · 3 | 1.8 | arcane | 28 | 8000 | 5 | 2.5 | – | aoe, ground | .20 / 1.5 / 80 / .06 | chain_lightning .08, mana_shield .06 |

Runtime behaviour:
- **fireball** — projectile, damage on arrival; 60 % burn chance.
- **meteor** — ground AoE at the anchor; damage 300 ms after release to the targets captured at release; 60 % burn each.
- **blizzard** — ground AoE, instant one-shot damage (no lingering storm); **every** hit target gets `freeze` 2000 ms (subject to diminishing returns). The described slow is not what happens (Q14).
- **ice_armor** — self `damageReduction` buff. The described "attackers slowed" is not implemented (Q14).
- **chain_lightning** — caster-centred AoE: hits **every** alive monster within `aoeRadius(L)` (4 → 5.5 tiles) of the hero, not a 3-target chain (Q15).
- **mana_shield** — self `manaShield` buff (section 12.2). Web quirk Q16: the mana part is never deducted.
- **fire_wall** — ground AoE, instant one-shot (no persistent wall); 40 % burn.
- **combustion** — single target, instant; ×1.5 (floored) if the target has `burn`; 40 % burn.
- **ice_arrow** — projectile; 35 % chance of `slow` 40 for 3000 ms.
- **freeze** — single target, instant; `freeze` for `stunDuration` 2000 ms (fixed).
- **teleport** — (`ZoneScene.ts:2539-2585`) instant, no anim delay:
  1. Destination tile = the tile under the pointer (`worldToTile` = floor of the iso→tile conversion). Touch cast from a button: if the joystick is deflected (`len > 0.2`) → `hero + dir/len * 6` tiles; else the target's tile; else the hero's tile.
  2. `destCol = round(col)`, `destRow = round(row)`, clamped to `[1, cols-2]` / `[1, rows-2]`.
  3. If the tile is not walkable: search `r = 1..3`, `dr = -r..r` (outer), `dc = -r..r` (inner) for the first walkable tile inside `[1, size-2]`. None → log `zone.teleport.unreachable`, refund the mana cost (cooldown stays), stop.
  4. Move the hero there, clear path and `attackTarget`, log `skillActivated`.
  Range is **not** enforced (Q17). Blocked while frozen/stunned. For UE: destination = ground-plane hit under the cursor (mouse) or the stick direction (gamepad/touch).
- **arcane_torrent** — ground AoE, instant one-shot.

### 10.3 Rogue (`src/data/classes/rogue.ts`)

| id | zh / en | tree · tier | dmg× | type | mp | cd | rng | R | buff | other | scaling d / m / cd / R / bv / bd | synergies |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `backstab` (:13) | 背刺 / Backstab | assassination · 1 | 2.0 | physical | 10 | 2500 | 1.5 | – | – | critBonus 20 | .22 / .5 / – | poison_blade .10, vanish .08 |
| `poison_blade` (:37) | 毒刃 / Poison Blade | assassination · 2 | 0.5 | poison | 12 | 8000 | 0 | – | poisonDamage 0.5, 6000 | – | .08 / .5 / 80 / – / .03 / 200 | backstab .06 |
| `vanish` (:63) | 消失 / Vanish | assassination · 3 | 0 | physical | 20 | 15000 | 0 | – | stealthDamage 1.0, 3000 | – | 0 / .8 / 200 / – / .05 / 100 | backstab .06, death_mark .05 |
| `multishot` (:92) | 多重射击 / Multi Shot | archery · 1 | 0.8 | physical | 15 | 3000 | 5 | 3 | – | aoe, per-target arrow delay | .12 / 1.0 / 30 / .08 | arrow_rain .10 |
| `arrow_rain` (:118) | 箭雨 / Arrow Rain | archery · 3 | 0.6 | physical | 25 | 7000 | 6 | 3.5 | – | aoe, ground | .10 / 1.5 / 80 / .10 | multishot .08, explosive_trap .06 |
| `shadow_step` (:147) | 暗影步 / Shadow Step | assassination · 3 | 0 | physical | 18 | 12000 | 5 | – | critBonus 0.3, 4000 | critBonus 30 | 0 / .8 / 120 / – / .02 / 100 | backstab .08, vanish .06 |
| `death_mark` (:175) | 死亡印记 / Death Mark | assassination · 2 | 0.5 | physical | 16 | 10000 | 4 | – | damageAmplify 0.25, 8000 | debuff on target | .06 / .8 / 100 / – / .02 / 200 | backstab .08, poison_blade .06 |
| `piercing_arrow` (:204) | 穿透箭 / Piercing Arrow | archery · 2 | 1.8 | physical | 14 | 4000 | 6 | 1.5 | – | aoe (line) | .16 / .8 / 40 | multishot .07, arrow_rain .05 |
| `poison_arrow` (:230) | 毒箭 / Poison Arrow | archery · 2 | 1.2 | poison | 12 | 3500 | 6 | – | – | projectile | .14 / .6 / 35 | poison_blade .08, multishot .05 |
| `explosive_trap` (:256) | 爆炸陷阱 / Explosive Trap | traps · 1 | 1.5 | fire | 14 | 5000 | 3 | 2 | – | aoe | .16 / .8 / 50 / .06 | arrow_rain .06 |
| `poison_cloud` (:284) | 毒雾 / Poison Cloud | traps · 2 | 1.0 | poison | 18 | 7000 | 4 | 2.5 | – | aoe, ground | .12 / 1.0 / 60 / .06 | explosive_trap .06, poison_blade .08 |
| `slow_trap` (:311) | 减速陷阱 / Slow Trap | traps · 2 | 0.6 | physical | 14 | 8000 | 3 | 2.0 | slowEffect 0.4, 5000 | aoe | .08 / .6 / 80 / .05 / .02 / 150 | explosive_trap .05, chain_trap .06 |
| `chain_trap` (:341) | 连锁陷阱 / Chain Trap | traps · 3 | 1.4 | physical | 22 | 9000 | 4 | 3.0 | – | aoe, stun 1000 | .14 / 1.2 / 80 / .06 | explosive_trap .06, slow_trap .07, arrow_rain .04 |

Runtime behaviour:
- **backstab** — melee single target, +20 crit chance. No positional ("from behind") requirement.
- **poison_blade** — self buff `poisonDamage`: every damage calculation by the hero (basic attacks and skills) adds `baseDamage × value` flat to the elemental term (section 12.1). It does not apply the poison status, and its own `damageMultiplier` is unused (buff path).
- **vanish** — self buff `stealthDamage`: final damage × (1 + value) for every hit (skills included) until a **basic attack lands** (not dodged) — then all `stealthDamage` buffs are removed (`ZoneScene.ts:2875-2878`) — or it expires. No invisibility/aggro drop.
- **multishot** — caster-centred AoE (360°, every alive monster within R), each hit delayed `min(260, isoPx*1.1)` ms. Not a 5-arrow fan (Q15).
- **arrow_rain** — ground AoE, instant one-shot (not 3 s).
- **shadow_step** — instant (`ZoneScene.ts:2588-2616`): with a target (any distance, Q6): `d = hero - target; len = |d| or 1`; `behind = round(target - d/len)` per axis, clamped to `[1, size-2]`; if not walkable use `round(target)`; move the hero there, clear path, set `attackTarget = target`, push buff `critBonus` (value/duration scaled). The `critBonus` buff stat is never read and the `critBonus: 30` field is never used, because the skill deals no damage (Q18). With no target: generic buff path (cost paid, useless buff).
- **death_mark** — (`ZoneScene.ts:2619-2635`) with a target at any distance (Q6): push onto the **monster** `{stat:'damageAmplify', value: buffValue(L), duration: buffDuration(L)}`, then a normal hit with this skill (0.5× …) without status rules; log `zone.combat.deathMarkApplied`. Monster buffs are never expired (Q19) so the mark lasts until the monster dies. Additive, uncapped stacking. With no target at all: the generic buff path puts `damageAmplify` on the **hero** (Q8).
- **piercing_arrow** — line AoE from the hero toward the target: length `range` (6), half-width `R(L) * 0.6` (= 0.9 tiles; R has no per-level growth); each hit delayed `min(260, isoPx*1.1)` ms.
- **poison_arrow** — projectile; `poison` status (20 % of the hit per second, 4 s).
- **explosive_trap** — immediate **caster-centred** blast (no trap is placed, Q20); 40 % burn. Being `fire`, its stat bonus uses `int`.
- **poison_cloud** — ground AoE, instant one-shot + `poison` status on each target.
- **slow_trap** — (`ZoneScene.ts:2638-2661`) immediate caster-centred: for every alive monster within `aoeRadius(L)` of the hero: hit (9.7 formula, no status rules); if it survived apply `slow` with value `round(buffValue(L)*100)` (40 at L1, 70 at L20) for `buffDuration(L)` ms. Log `zone.combat.slowTrapHit {count}` if ≥1 target.
- **chain_trap** — immediate caster-centred AoE + `stun` 1000 ms (fixed) on each target.

### 10.4 Computed values (L1 / L5 / L10 / L20)

Format: `x<dmgMult> <mana>mp <cooldown>ms r<radius> b<buffValue>/<buffDuration>ms` (no CDR, no Resonance).

| warrior | L1 | L5 | L10 | L20 |
|---|---|---|---|---|
| slash | x1.5 8mp 2000ms | x2.22 10mp 2000ms | x3.03 12mp 2000ms | x4.2 15mp 2000ms |
| whirlwind | x1.2 15mp 4000ms r2.5 | x1.76 19mp 3800ms r2.82 | x2.39 23mp 3575ms r3.18 | x3.3 30mp 3250ms r3.7 |
| war_stomp | x1.8 22mp 8000ms r2 | x2.6 28mp 7680ms r2.2 | x3.5 34mp 7320ms r2.425 | x4.8 44mp 6800ms r2.75 |
| shield_wall | 12mp 10000ms b0.5/5000 | 14mp 9600ms b0.56/5600 | 16mp 9150ms b0.628/6275 | 19mp 8500ms b0.725/7250 |
| taunt_roar | 14mp 12000ms r3 b0.3/4000 | 16mp 11520ms r3.24 b0.38/4800 | 18mp 10980ms r3.51 b0.47/5700 | 21mp 10200ms r3.9 b0.6/7000 |
| vengeful_wrath | 20mp 20000ms b0.25/6000 | 24mp 19200ms b0.33/7000 | 28mp 18300ms b0.42/8125 | 35mp 17000ms b0.55/9750 |
| charge | x2 14mp 6000ms | x2.72 17mp 5760ms | x3.53 20mp 5490ms | x4.7 26mp 5100ms |
| lethal_strike | x2.8 22mp 8000ms | x3.68 26mp 7680ms | x4.67 32mp 7320ms | x6.1 40mp 6800ms |
| dual_wield_mastery | 0mp 500ms | 0mp 500ms | 0mp 500ms | 0mp 500ms |
| iron_fortress | 16mp 14000ms b0.4/6000 | 19mp 13520ms b0.472/6800 | 22mp 12980ms b0.553/7700 | 28mp 12200ms b0.67/9000 |
| unyielding | 0mp 60000ms b0.35/5000 | 0mp 58000ms b0.41/5600 | 0mp 55750ms b0.478/6275 | 0mp 52500ms b0.575/7250 |
| life_regen | 0mp 500ms | 0mp 500ms | 0mp 500ms | 0mp 500ms |
| frenzy | 18mp 15000ms b0.2/8000 | 22mp 14400ms b0.272/9000 | 26mp 13725ms b0.353/10125 | 33mp 12750ms b0.47/11750 |
| bleed_strike | x1.8 16mp 5000ms | x2.44 19mp 4800ms | x3.16 22mp 4575ms | x4.2 28mp 4250ms |
| rampage | x2.2 24mp 10000ms r2.5 | x3 28mp 9600ms r2.74 | x3.9 34mp 9150ms r3.01 | x5.2 42mp 8500ms r3.4 |

| mage | L1 | L5 | L10 | L20 |
|---|---|---|---|---|
| fireball | x1.8 10mp 2000ms | x2.6 13mp 2000ms | x3.5 16mp 2000ms | x4.8 22mp 2000ms |
| meteor | x2.5 35mp 8000ms r2.5 | x3.62 43mp 7600ms r2.74 | x4.88 52mp 7150ms r3.01 | x6.7 65mp 6500ms r3.4 |
| blizzard | x1 20mp 5000ms r3 | x1.64 24mp 4760ms r3.32 | x2.36 30mp 4490ms r3.68 | x3.4 38mp 4100ms r4.2 |
| ice_armor | 12mp 15000ms b0.2/10000 | 14mp 14400ms b0.248/11200 | 16mp 13725ms b0.302/12550 | 19mp 12750ms b0.38/14500 |
| chain_lightning | x1.3 18mp 3500ms r4 | x1.94 22mp 3340ms r4.4 | x2.66 26mp 3160ms r4.85 | x3.7 33mp 2900ms r5.5 |
| mana_shield | 15mp 12000ms b0.3/8000 | 18mp 11520ms b0.36/9000 | 21mp 10980ms b0.428/10125 | 27mp 10200ms b0.525/11750 |
| fire_wall | x1.2 18mp 6000ms r2 | x1.76 22mp 5760ms r2.24 | x2.39 26mp 5490ms r2.51 | x3.3 33mp 5100ms r2.9 |
| combustion | x2 24mp 7000ms | x2.88 29mp 6720ms | x3.87 35mp 6405ms | x5.3 45mp 5950ms |
| ice_arrow | x1.6 10mp 2500ms | x2.24 12mp 2380ms | x2.96 15mp 2245ms | x4 19mp 2050ms |
| freeze | x0.8 22mp 10000ms | x1.2 26mp 9600ms | x1.65 32mp 9150ms | x2.3 40mp 8500ms |
| teleport | 16mp 8000ms | 18mp 7680ms | 20mp 7320ms | 23mp 6800ms |
| arcane_torrent | x1.8 28mp 8000ms r2.5 | x2.6 34mp 7680ms r2.74 | x3.5 40mp 7320ms r3.01 | x4.8 50mp 6800ms r3.4 |

| rogue | L1 | L5 | L10 | L20 |
|---|---|---|---|---|
| backstab | x2 10mp 2500ms | x2.88 12mp 2500ms | x3.87 14mp 2500ms | x5.3 17mp 2500ms |
| poison_blade | 12mp 8000ms b0.5/6000 | 14mp 7680ms b0.62/6800 | 16mp 7320ms b0.755/7700 | 19mp 6800ms b0.95/9000 |
| vanish | 20mp 15000ms b1/3000 | 23mp 14200ms b1.2/3400 | 26mp 13300ms b1.425/3850 | 32mp 12000ms b1.75/4500 |
| multishot | x0.8 15mp 3000ms r3 | x1.28 19mp 2880ms r3.32 | x1.82 23mp 2745ms r3.68 | x2.6 30mp 2550ms r4.2 |
| arrow_rain | x0.6 25mp 7000ms r3.5 | x1 31mp 6680ms r3.9 | x1.45 37mp 6320ms r4.35 | x2.1 47mp 5800ms r5 |
| shadow_step | 18mp 12000ms b0.3/4000 | 21mp 11520ms b0.38/4400 | 24mp 10980ms b0.47/4850 | 30mp 10200ms b0.6/5500 |
| death_mark | x0.5 16mp 10000ms b0.25/8000 | x0.74 19mp 9600ms b0.33/8800 | x1.01 22mp 9150ms b0.42/9700 | x1.4 28mp 8500ms b0.55/11000 |
| piercing_arrow | x1.8 14mp 4000ms r1.5 | x2.44 17mp 3840ms r1.5 | x3.16 20mp 3660ms r1.5 | x4.2 26mp 3400ms r1.5 |
| poison_arrow | x1.2 12mp 3500ms | x1.76 14mp 3360ms | x2.39 17mp 3202ms | x3.3 21mp 2975ms |
| explosive_trap | x1.5 14mp 5000ms r2 | x2.14 17mp 4800ms r2.24 | x2.86 20mp 4575ms r2.51 | x3.9 26mp 4250ms r2.9 |
| poison_cloud | x1 18mp 7000ms r2.5 | x1.48 22mp 6760ms r2.74 | x2.02 26mp 6490ms r3.01 | x2.8 33mp 6100ms r3.4 |
| slow_trap | x0.6 14mp 8000ms r2 b0.4/5000 | x0.92 16mp 7680ms r2.2 b0.48/5600 | x1.28 19mp 7320ms r2.425 b0.57/6275 | x1.8 23mp 6800ms r2.75 b0.7/7250 |
| chain_trap | x1.4 22mp 9000ms r3 | x1.96 26mp 8680ms r3.24 | x2.59 32mp 8320ms r3.51 | x3.5 40mp 7800ms r3.9 |

(Multipliers shown rounded to 3 decimals; poison_blade's 0.5→1.7 multiplier is unused.)

---

## 11. Buffs and debuffs

### 11.1 Buff stats and where they are read

| stat | on | consumer | cap (`BUFF_CAPS`, `CombatSystem.ts:115-123`) |
|---|---|---|---|
| `damageReduction` | hero (shield_wall, iron_fortress, unyielding, ice_armor) | defender: `final *= 1 - DR` (DR includes gear `eq.damageReduction/100`, total capped) | 0.9 |
| `defenseBonus` | hero (taunt_roar) | defender: `effectiveDefense *= 1 + v` | 2.0 |
| `damageBonus` | hero (frenzy, vengeful_wrath, dual wield passive) | attacker: `final *= 1 + v` | 3.0 |
| `attackSpeed` | – (no current source) | nothing reads it | 1.0 |
| `poisonDamage` | hero (poison_blade) | attacker: `elementalFlat += baseDamage * v` | 5.0 |
| `stealthDamage` | hero (vanish) | attacker: `final *= 1 + v`; removed by a landed basic attack | 5.0 |
| `manaShield` | hero (mana_shield) | defender: redirect `floor(final*v)` to mana | 0.9 |
| `damageAmplify` | monster (death_mark, pet mark `PetCompanion.ts:626`); hero (elite curse aura, Q8) | defender: `final *= 1 + v` | **none** |
| `critBonus` | hero (shadow_step) | nothing (Q18) | – |
| `slowEffect` | – (only as slow_trap data) | nothing (slow_trap converts it to a status) | – |
| `taunted` | monster (taunt_roar) | nothing (Q10) | – |

`getBuffValue(entity, stat)` (`CombatSystem.ts:214-226`) = Σ values of all buffs with that stat (no expiry check — expiry happens by pruning), then `min(total, cap)` if a cap exists.

### 11.2 Lifetime and stacking
- Hero buffs are pruned at the start of `handleCombat` every frame: remove where `now - startTime >= duration` (`ZoneScene.ts:2811-2815`). Buffs are **not** cleared on death or respawn; they **are** carried through zone transitions (`ZoneScene.ts:4245`, `:453`).
- Every cast pushes a new entry → same-stat buffs stack additively up to the cap (e.g. shield_wall recast with high CDR, or shield_wall + iron_fortress + unyielding).
- Monster buffs are never pruned (Q19); a monster's buff list resets only when it respawns (a new instance, `ZoneScene.ts:5598-5625`).
- Elite curse aura (`ZoneScene.ts:6038-6066`): while within the affix radius, the hero has one `damageAmplify` buff tagged `curseAura` (value = affix reduction, 2000 ms, refreshed each frame).

---

## 12. Damage formula (stat use) — `CombatSystem.calculateDamage` (`CombatSystem.ts:229-381`)

Inputs: attacker A, defender D (CombatEntity: stats, baseDamage, defense, buffs, mana, equipStats?, outgoingDamageMultiplier?), optional skill + level + skillLevels map, `forceCrit`.
The hero's entity: `stats` = raw stats, `baseDamage/defense` = derived, `equipStats` = merged bag, `outgoingDamageMultiplier = spirit.damageMultiplier` (`Player.ts:390-408`). A monster's entity: `stats = {str: floor(damage*0.8), dex: floor(speed*0.1), vit: floor(hp*0.1), int: 3, spi: 3, lck: 3}`, `baseDamage = def.damage`, `defense = def.defense`, no equipStats (`Monster.ts:85-92, 355-372`).

```
// RNG draw 1 (always)
dodgeRate = clamp((D.stats.dex + D.eq.dex) * 0.3, 0, 30)
if rand01()*100 < dodgeRate: return {damage 0, isDodged true, isCrit false, type 'physical', steals 0}
// RNG draw 2 (skipped when forceCrit)
critRate = clamp((A.dex + A.eq.dex)*0.2 + (A.lck + A.eq.lck)*0.5 + skill.critBonus + A.eq.critRate, 0, 75)
isCrit = forceCrit || rand01()*100 < critRate
critMul = isCrit ? 1.5 + (A.lck + A.eq.lck)*0.01 + A.eq.critDamage/100 : 1
type = skill ? skill.damageType : 'physical'
if skill: base = A.baseDamage + (type=='physical' ? A.stats.str : A.stats.int)*0.5
          mult = dmgMult(skill, level) * (skillLevels ? synergyFactor : 1)
else:     base = A.baseDamage + A.stats.str*0.5 ; mult = 1
base += A.eq.damage
if A.eq.damagePercent > 0: base *= 1 + A.eq.damagePercent/100
elemFlat = A.eq.fireDamage + A.eq.iceDamage + A.eq.lightningDamage + A.eq.poisonDamage
if A.eq.elementalDamagePercent > 0: elemFlat *= 1 + A.eq.elementalDamagePercent/100
elemFlat += A.baseDamage * buff(A,'poisonDamage')
DR = min(buff(D,'damageReduction') + (D.eq.damageReduction > 0 ? D.eq.damageReduction/100 : 0), 0.9)
def = D.defense ; if D.eq.defense > 0: def += D.eq.defense
if D.eq.defensePercent > 0: def *= 1 + D.eq.defensePercent/100
if buff(D,'defenseBonus') > 0: def *= 1 + buff(D,'defenseBonus')
if A.eq.ignoreDefense > 0: def *= 1 - A.eq.ignoreDefense/100
out = clamp(A.outgoingDamageMultiplier ?? 1, 0, 10)
raw = (base * mult * critMul + elemFlat) * out
final = max(1, raw - def*0.5) * (1 - DR)
if damageBonus > 0: final *= 1 + buff(A,'damageBonus')
if stealth > 0:     final *= 1 + buff(A,'stealthDamage')
if amplify > 0:     final *= 1 + buff(D,'damageAmplify')
if type != 'physical': final *= 1 - clamp(D.eq.allResist + D.eq.<type>Resist, 0, 75)/100   // arcane: allResist only; D without eq → 0
final = max(1, floor(final))
if buff(D,'manaShield') > 0 and D.mana > 0:
    absorb = min(floor(final * buff(D,'manaShield')), D.mana)     // D.mana may be fractional
    final = max(1, final - absorb) ; manaDamage = absorb
lifeStolen = A.eq.lifeSteal > 0 ? floor(final * A.eq.lifeSteal/100) : 0
manaStolen = A.eq.manaSteal > 0 ? floor(final * A.eq.manaSteal/100) : 0
return {damage: final, isCrit, isDodged false, type, lifeStolen, manaStolen, manaDamage}
```
Notes: monster `level` is unused; DoT ticks bypass this formula entirely (section 13.3).

### 12.1 Combat-resource side effects (`applySteal`, `ZoneScene.ts:3179-3191`)
Called after every hero-dealt hit (basic, extra double-strike/double-shot hits, every skill target): if `damage > 0` → `spirit.gain('hit', isCrit)`; `hp += lifeStolen` (if alive, clamp max); `mana += manaStolen` (clamp).

### 12.2 Mana Shield — web quirk Q16
The runtime applies monster hits with `hp -= result.damage` (`ZoneScene.ts:2996-2998`) and never calls `CombatSystem.applyDamage` (`CombatSystem.ts:383-410`, the only code that subtracts `manaDamage` from mana; the unit tests expect that subtraction, `src/__tests__/CombatSystemBugs.test.ts:378`). So in the web game Mana Shield reduces HP damage at no mana cost.

### 12.3 Worked test vectors (no dodge, no crit unless stated; normal difficulty, unscaled goblin def 4/dex 5; hero with base stats, no gear)

| case | result |
|---|---|
| warrior L1 `slash` L1 vs goblin | 36 |
| warrior L1 basic attack vs goblin | 23 |
| mage L1 `fireball` L1 vs goblin | 34 |
| rogue L1 `backstab` L1 vs goblin | 36 (crit: 58) |
| rogue L1 `explosive_trap` L1 vs goblin (uses `int`) | 25 |
| mage L1 `fireball` during Resonance (out 1.2) | 41 |
| warrior L5 `slash` L5 with `whirlwind` L3 (synergy 1.24) | 90 |
| goblin → warrior L1 | 6 (crit: 12) |
| goblin → warrior L1 with shield_wall L1 | 3 |
| goblin → mage L1 with mana_shield L1, mana 152 | 5 (manaDamage 2) |
| rogue L1 basic with poison_blade L1 + vanish L1 | 49 |
| warrior L1 basic vs goblin with death_mark L1 | 29 |
| first roll 0.0 (dodge) | damage 0, isDodged |

---

## 13. Status effects (`src/systems/StatusEffectSystem.ts`)

### 13.1 Model
`StatusEffectType = burn | freeze | poison | bleed | slow | stun` (`:9`).
`StatusEffect {type, value, duration ms, tickInterval ms, startTime, lastTickTime, sourceId}` (`:14-28`).
Per-entity list keyed by entity id (`'player'` for the hero). Constants (`:43-58`):
`DIMINISH_FACTOR 0.5`, `DIMINISH_IMMUNITY_DURATION 3000`, `DIMINISH_WINDOW 6000`, `SLOW_MIN_SPEED_FACTOR 0.2`,
tick intervals burn/poison/bleed 1000, freeze/slow/stun 0. One system instance per zone (`GameSession.beginZone`, `src/game/GameSession.ts:46-56`); `clearAll()` on zone shutdown (`ZoneScene.ts:7266`).

### 13.2 `apply(target, type, value, duration, source, now) → effectiveDuration` (`:79-158`)
1. `duration <= 0 || value <= 0` → 0.
2. `freeze`/`stun`: `duration = diminish(target, type, duration, now)`; if 0 → return 0.
3. `poison` and one exists: refresh `startTime = now`, `duration = duration`, `value = max(old, new)`, `sourceId = source` (`lastTickTime` unchanged); log `sys.statusEffect.refreshed`; return duration.
4. `slow` and one exists: same refresh/keep-stronger (no log); return duration.
5. `freeze`/`stun`: remove the existing effect of that type (only one).
6. Push a new effect `{type, value, duration, tickInterval: DEFAULT[type], startTime: now, lastTickTime: now, sourceId}`; log `sys.statusEffect.applied`; return duration.
`burn` and `bleed` **stack** (each application is a separate effect).

Diminishing returns per `(target, type)` (`:297-336`), record `{applyCount 0, lastApplyTime 0, immuneUntil 0}`:
```
if now < immuneUntil: return 0
if now - lastApplyTime > 6000: applyCount = 0
applyCount++ ; lastApplyTime = now
if applyCount == 2: d = floor(base*0.5); immuneUntil = now + d + 3000; return d
if applyCount > 2:  immuneUntil = now + 3000; return 0     // also refreshes lastApplyTime
return base
```
Records are deleted with `clearEntity` (death) and `clearAll`.

### 13.3 Tick and expire — once per frame for every tracked entity (`ZoneScene.updateStatusEffects`, `:6078-6178`), after combat
For each effect with `tickInterval > 0`: `elapsed = now - lastTickTime`; if `elapsed >= interval`: emit `floor(elapsed/interval)` ticks of `value` damage and set `lastTickTime = now` (remainder dropped). Then `expire`: remove effects with `now - startTime >= duration` (ticks of that frame are applied first).
- Tick on the hero: skip if dead; `hp = max(0, hp - dmg)`; damage number; `COMBAT_DAMAGE {targetId:'player', …}`; if `hp <= 0` → `killPlayer()`.
- Tick on a monster: `monster.takeDamage(dmg, isTick)` — **raw**: no defense, resist, DR or amplify; kills go through `onMonsterKilled` (spirit `kill`, exp, loot). Ticks never grant spirit `hit`.
- Expiry logs `zone.statusEffect.expired {effectName}`.
Nominal tick counts: burn 3 s → up to 3 ticks, poison 4 s → up to 4, bleed 5 s → up to 5; with frame jitter the last tick can be lost (Q21).

### 13.4 Queries and gameplay effects
- `isImmobilized(id)` = has freeze or stun. `getSpeedMultiplier(id)` = 0 if immobilized; else `max(0.2, 1 - clamp(slow.value,0,100)/100)` with the (single) slow; 1 if none (`:261-275`).
- **Monsters**: immobilized → their AI update is skipped (no movement/state change, animator only), they cannot start an attack, and a strike already in wind-up is cancelled at its contact time (`ZoneScene.ts:1444-1449, 2822, 2948`). Slow multiplies movement speed only (`ZoneScene.ts:1464-1469`), not attack rate.
- **Hero**: immobilized blocks dodge and teleport only; movement, attacks and other skills still work (Q22). Slow on the hero has no gameplay effect. Poison halves HP regen (section 4.1).
- Death of a monster: `clearEntity(monster.id)` (`ZoneScene.ts:3792`). Hero death: `clearEntity('player')` (`ZoneScene.ts:898`).

### 13.5 Skill → status rules (`applySkillStatusEffect`, `ZoneScene.ts:6244-6291`)
Called after each skill hit from the AoE and single-target paths (not from death_mark or slow_trap), with `dmg` = final damage dealt (after combustion ×1.5), only if the target is still alive. Evaluated in this order; several can fire:

| rule | condition | effect |
|---|---|---|
| burn | `damageType == fire` | roll `rand01() < (id contains 'meteor' or 'fireball' ? 0.6 : 0.4)` → `burn`, value `max(1, floor(dmg*0.15))`, 3000 ms |
| freeze | `damageType == ice` and (`stunDuration` or id contains `freeze` or id == `blizzard`) | `freeze`, value 1, `stunDuration ?? 2000` ms (no roll) |
| ice slow | `damageType == ice` otherwise | roll `rand01() < 0.35` → `slow`, value 40, 3000 ms |
| poison | `damageType == poison` | `poison`, value `max(1, floor(dmg*0.2))`, 4000 ms |
| bleed | id contains `bleed`, `lacerate` or `rend` | `bleed`, value `max(1, floor(dmg*0.25))`, 5000 ms |
| stun | `stunDuration` and `damageType == physical` | `stun`, value 1, `stunDuration` ms |

Resulting per skill: fireball/meteor burn 60 %; fire_wall/combustion/explosive_trap burn 40 %; blizzard/freeze freeze (100 %); ice_arrow slow 35 %; poison_arrow/poison_cloud poison; bleed_strike bleed; war_stomp stun 2000; chain_trap stun 1000.
A dodged hit (dmg 0) still applies these (DoT value floors at 1) — Q9.

### 13.6 Monster → hero rules (`applyMonsterStatusEffect`, `ZoneScene.ts:6204-6237`) — after a landed monster hit
- spriteKey or monster id contains `fire`, `phoenix` or `lava`: 30 % → `burn` `max(1, floor(def.damage*0.2))`, 3000 ms.
- contains `poison`, `venom` or `spider`: 25 % → `poison` `max(1, floor(def.damage*0.15))`, 4000 ms.
- contains `ice` or `frost`: 20 % → `slow` 30, 3000 ms.
None match Chapter-1 monsters. Port as a data field per monster (`onHitStatus`) generated from these keyword rules (monster spec).

### 13.7 Other sources
Elite affix "frozen": `freezeChance` roll → hero `slow` 30 for 2500 ms (`ZoneScene.ts:3048-3052`). Pets: bleed/burn/stun/slow on monsters (`PetCompanion.ts:661, 707, 734-736`; pet spec).

### 13.8 Render-only
Tints per effect (burn → burn tint, freeze/stun → freeze tint, poison → poison tint), re-applied on expiry (`ZoneScene.ts:6083-6176`). UE: material parameter / overlay material per effect on the skeletal mesh, plus Niagara loop (embers, ice crust, green bubbles, blood drips, stun stars, slow ripples).

---

## 14. Spirit (灵力) and Resonance (共鸣) — `src/systems/SpiritSystem.ts`

Always active for every class from level 1; it is the class resource mechanic of Chapter 1.

### 14.1 Profiles (`:32-75`)

| class | profile id | colour | max | hit | kill | dodge | crit bonus | resonance ms | dmg bonus | mana-cost × | move bonus |
|---|---|---|---|---|---|---|---|---|---|---|---|
| warrior | `emberheart` | `#ffb45c` | 100 | 8 | 15 | 12 | 4 | 6000 | 0.30 | 0.85 | 0.12 |
| mage | `astral_focus` | `#a98bff` | 100 | 7 | 12 | 16 | 5 | 7000 | 0.20 | 0.60 | 0.08 |
| rogue | `shadow_rhythm` | `#66e58a` | 100 | 6 | 13 | 20 | 7 | 5500 | 0.25 | 0.75 | 0.18 |
Unknown class → warrior profile (`:77-79`).

### 14.2 State machine
State: `value ∈ [0, max]`, `resonanceRemainingMs ≥ 0`. `isResonating = remaining > 0 && value > 0`.
- `gainFromCombat(source, spi, isCrit)` (`:122-131`): `amount = (base[source] + (isCrit ? critBonusGain : 0)) * (1 + clamp(spi,0,200)*0.015)` where `spi` = hero raw `stats.spi`; then `gain(amount)`.
- `gain(amount)` (`:133-148`): ignore if not finite, `<= 0`, or resonating. `prev = value; value = clamp(prev+amount, 0, max); gained = value - prev; started = prev < max && value >= max; if started: remaining = resonanceDurationMs`.
- `update(dt)` (`:150-168`, called from `Player.update` only while alive): if resonating: `e = min(dt, remaining)`; `remaining = max(0, remaining - e)`; `value = max(0, value - max/resonanceDurationMs * e)`; if `remaining <= 0 || value <= 0` → `value = 0, remaining = 0`, ended.
- Multipliers while resonating: `damageMultiplier = 1 + dmgBonus` (attacker `outgoingDamageMultiplier`, section 12), `manaCostMultiplier` (section 8), `moveSpeedMultiplier = 1 + moveBonus` (section 3).
- `reset()` on hero death (`Player.ts:410-423`).
- Sources (`ZoneScene`): `hit` — every hero hit with damage > 0 (per AoE target), crit adds the crit bonus (`:3181`); `kill` — every monster death processed by `onMonsterKilled`, whatever killed it (hero, DoT, mercenary, pet) (`:3793`); `dodge` — a monster hit evaded by the hero's dodge roll (`:2987`), or the first monster hit absorbed during dodge i-frames per dodge (`:2973-2976`, `DodgeController.claimAvoidanceReward`).

Per-class gains with base SPI (no gear, no points): warrior ×1.075 → hit 8.6 (crit 12.9), kill 16.125, dodge 12.9; mage ×1.15 → hit 8.05 (crit 13.8), kill 13.8, dodge 18.4; rogue ×1.075 → hit 6.45 (crit 13.975), kill 13.975, dodge 21.5.

### 14.3 Events (`Player.ts:359-377`, `:225-235`)
On gain > 0: `PLAYER_SPIRIT_CHANGED {value, maxValue, resonating, gained, source}`; on start: `SPIRIT_RESONANCE_STARTED {profileId, durationMs}` + aura burst (render). On end / death: `PLAYER_SPIRIT_CHANGED {value:0, maxValue, resonating:false}` + `SPIRIT_RESONANCE_ENDED {profileId}`.

### 14.4 Persistence
`SpiritSaveState {value, resonanceRemainingMs}` in `SaveData.player.spirit` (save v3+). `restore`: non-finite → 0; clamp value to `[0,max]`, remaining to `[0,duration]`; if either ≤ 0 → remaining = 0 (`:170-188`). Save migration v2→v3 creates it via `restore` (`SaveSystem.ts:111-118`). Zone transitions carry it (`ZoneScene.ts:4244`, `:452`).

### 14.5 Render-only
HUD bar: width = ratio, label `ui.hud.spirit` (灵力 / SPIRIT), text `floor(value)/max`; while resonating the bar pulses (`alpha = 0.82 + sin(t*0.012)*0.18`) and shows `ui.hud.resonance {seconds}` with remaining/1000 to 1 decimal (`UIScene.ts:5791-5809`). Dodge afterimages are tinted with the profile colour. UE: UMG progress bar with the class colour, Niagara aura on the hero while resonating.

---

## 15. Dodge (class-specific parameters)

`DodgeController` (`CombatInputSystem.ts:69-142`): cooldown 900 ms, i-frames 220 ms. Distance by class (`ZoneScene.ts:2395-2397`): rogue 2.6, mage 2.25, warrior 1.8 tiles. Direction = requested or last move direction (default `(1,-1)` normalized). Destination: first of `d = dist, dist-0.25, … ≥ 0.5` whose rounded tile is in bounds and walkable; none → no dodge (no cooldown). Blocked while dead or immobilized. Events: `DODGE_STARTED {cooldownMs, invulnerabilityMs}`. Full movement/i-frame rules belong to the combat/movement spec.

---

## 16. Per-frame order (`ZoneScene.update`, `:1346-1494`) — relevant parts

1. Return early (world frozen) during a story cinematic, the Abyss tier/boon picker (`:1348-1351`). In the web the clock still advances. In the port the SimClock holds, no timers drain and the freeze-begin rules run (§19.1, D13).
2. `recovery` = campfire modifiers.
3. Input: keyboard movement, hold-move, skill keys (`requestSkill`), gamepad, `consumeBufferedSkill`.
4. `eq = getEquipStats(); hero.recalcDerived(eq)`.
5. Poison → `recovery.hp *= 0.5`.
6. Life Regen passive heal; Unyielding proc; Dual Wield buff.
7. `hero.update`: movement, spirit drain, mana regen, HP regen, animator.
8. Monster AI (immobilized skip, slow multiplier).
9. `handleCombat`: prune hero buffs; monster attacks; hero basic attack.
10. Mercenary, pet, escort/defend, elite affix behaviours (curse aura buff).
11. `updateStatusEffects` (ticks, then expiry).
12. Auto-combat (`handleAutoCombat`) if enabled.
Delayed callbacks (skill release, projectile arrival, AoE delays, strike contact) fire from the timer system at their due time, before the scene update of that frame. Port: the core timer queue drains at the start of each sim step, never while frozen (step algorithm in §19.1).

---

## 17. Events emitted by this area

| event | payload | when |
|---|---|---|
| `PLAYER_LEVEL_UP` (`player:levelup`) | `{level}` | level-up |
| `PLAYER_EXP_CHANGED` (`player:exp`) | `{exp, needed}` | every `addExp`, death toll |
| `PLAYER_HEALTH_CHANGED` | `{hp, maxHp}` | when hp/maxHp changed this frame, and after heals |
| `PLAYER_MANA_CHANGED` | `{mana, maxMana}` | same, and after casts / refunds |
| `PLAYER_SPIRIT_CHANGED` | `{value, maxValue, resonating, gained?, source?}` | spirit gain / end / death |
| `SPIRIT_RESONANCE_STARTED` / `_ENDED` | `{profileId, durationMs}` / `{profileId}` | resonance |
| `SKILL_USED` (`skill:used`) | `{skillId, damageType}` | cast commit (audio cue) |
| `SKILL_BUFFERED` | `{skillId, expiresAt}` | buffered request |
| `SKILL_LEVEL_CHANGED` | `{skillId, level}` | skill point spent |
| `DODGE_STARTED` | `{cooldownMs, invulnerabilityMs}` | dodge |
| `PLAYER_DIED` | `{}` | death |
| `LOG_MESSAGE` | `{text, type}` | keys: `sys.player.levelUp/death/respawn`, `zone.combat.skillLocked/manaInsufficient/freeCast/skillActivated/deathMarkApplied/slowTrapHit/tauntRoar/unyieldingProc/autoCombat`, `zone.teleport.blockedByCC/unreachable`, `sys.statusEffect.applied/refreshed`, `zone.statusEffect.expired` |

---

## 18. Save / load / transitions

- `SaveData.player` (`src/data/types.ts:614-631`): `level, exp, gold, hp, maxHp, mana, maxMana, stats, freeStatPoints, freeSkillPoints, skillLevels (Record<string,int>), spirit?, tileCol, tileRow, currentMap`. `classId` at the root. `settings.autoCombat` / `autoLootMode`. Current version 3 (`SaveSystem.ts:6`).
- Not saved: buffs, cooldowns, status effects, dodge state. (Spirit is saved.)
- Restore (`ZoneScene.ts:4298-4314`): copy level/exp/gold/stats/points/skillLevels (missing keys = 0), restore spirit, `recalcDerived()` **without gear**, then `hp = min(saved.hp, maxHp)`, `mana = min(saved.mana, maxMana)` — gear HP/MP above the gear-less max is lost on load (Q2).
- Zone transition (`getPlayerTransitionStats`, `:4230-4249`; init `:435-458`): carries level, exp, gold, hp, mana, stats, points, skillLevels, spirit, **buffs**, autoCombat, autoLootMode. The hero object is rebuilt, so **all skill cooldowns reset to 0** on every zone change (Q23).
- Death (`handlePlayerDied`, `:896-955`): clear hero status effects, death penalty, after 1100 ms (SimClock, §19.1 T12) respawn at camp 0 with full hp/mana (or, inside a dungeon, rebuild the parent zone with full hp/mana). Buffs are not cleared.

---

## 19. Web quirks — recommended port decisions

The orchestrator should confirm these. Recommendation in **bold**.

| id | quirk | evidence | recommendation |
|---|---|---|---|
| Q1/D1 | `statGrowth` never applied | `Player.ts:156-177` | **Keep web behaviour (free points only)** for parity; keep `statGrowth` in JSON for a later balance pass. |
| Q2/D2 | Level-up heal and save restore use gear-less maxima | `Player.ts:164-166`, `ZoneScene.ts:4312-4314` | **Fix**: recompute with gear before healing / clamping. |
| Q3/D3 | Hotbar slots shift when an earlier skill is learned | `SkillProgressionSystem.ts:52-60` | Keep the auto-fill order, but **persist explicit slot bindings** once the player edits them (UE UI). |
| Q4 | Passives (`life_regen`, `dual_wield_mastery`, `unyielding`) are on the hotbar and castable (1-damage hit / manual Unyielding) and in auto-combat priority | `ZoneScene.ts:2248-2267, 3095-3107` | **Fix**: exclude `passive` skills from the hotbar, manual casting and auto-combat. |
| Q5 | `stunDuration` does not scale with level though descriptions say it does | `ZoneScene.ts:6266, 6289` | Keep (data-driven; can add `stunDurationPerLevel` later). |
| Q6 | Buff-carrying targeted skills (`death_mark`, `shadow_step`) ignore range | `ZoneScene.ts:2486-2489` | **Fix**: enforce `range + 1` for them. |
| Q7 | Retarget after wind-up has no range check | `ZoneScene.ts:2525` | **Fix**: re-check range; if out of range, fizzle (cost already paid) or pick a target in range. |
| Q8 | `death_mark` / `shadow_step` with no living monster fall into the generic buff path (`death_mark` amplifies damage taken by the hero) | `ZoneScene.ts:2588, 2619, 2663` | **Fix**: such casts are rejected before paying cost (require a target). |
| Q9 | Dodged skill hits still apply status effects (DoT value 1, full freeze/stun) | `ZoneScene.ts:2737-2748` | **Fix**: skip hit reaction and status rules when `isDodged`. |
| Q10 | `taunted` buff unused; taunt only flips idle/patrol → chase | `ZoneScene.ts:2683-2687` | Keep; monster spec may later read it. |
| Q11 | Frenzy / Vengeful Wrath give no attack speed | data + `ZoneScene.ts:2663-2667` | Keep (descriptions should be edited in i18n instead). |
| Q12 | Charge does not move the hero | `SkillEffectSystem.ts:320-345` | Open: implementing a dash is a design change; **keep** for Chapter 1. |
| Q13 | Rampage "+15 % per extra target" missing | `ZoneScene.ts:2700-2768` | Keep. |
| Q14 | Blizzard freezes instead of slowing; Ice Armor does not slow attackers | `ZoneScene.ts:6265` | Keep. |
| Q15 | Chain Lightning / Multishot hit every enemy in a circle around the hero | `ZoneScene.ts:2708-2711` | Keep (it is what players experienced). |
| Q16 | Mana Shield never drains mana | `ZoneScene.ts:2996-2998` vs `CombatSystem.ts:394-397` | **Fix**: subtract `manaDamage` from hero mana (matches the tested intent). |
| Q17 | Teleport range unlimited | `ZoneScene.ts:2539-2556` | **Fix**: clamp the destination to `range` (6) tiles along the aim ray before the walkable search. |
| Q18 | Shadow Step crit buff does nothing | section 11.1 | **Fix**: read `critBonus` buff in crit chance as percentage points `value*100`, consumed by the next hero hit. |
| Q19 | Monster buffs never expire (death mark permanent, uncapped stacking) | no prune of `monster.buffs` | **Fix**: prune monster buffs by duration each frame. |
| Q20 | Traps are instant caster-centred blasts | `ZoneScene.ts:2700-2768` | Keep for Chapter 1. |
| Q21 | DoT ticks drop the remainder (`lastTickTime = now`); last tick can be lost; after a pause many ticks fire at once | `StatusEffectSystem.ts:178-185` | **Fix**: `lastTickTime += tickCount*interval`, and cap ticks so that total ticks ≤ `floor(duration/interval)`. |
| Q22 | Hero freeze/stun/slow do not affect movement | `ZoneScene.ts` (no checks) | Keep for Chapter 1 (no monster applies them); revisit for later chapters. |
| Q23 | Zone change resets skill cooldowns | `Player.ts:99-101` | Keep (harmless). |
| Q24 | deathSave never re-arms after a zone change: the 60 s re-arm is a scene `delayedCall`, which dies when the scene restarts, and `_deathSaveUsed` is a field that `init`/`create` never reset. `scene.restart` reuses the instance, so the flag stays set until the page reloads | `ZoneScene.ts:186, 3058-3061, 337-400`; `phaser/src/time/Clock.js:436-457` | **Fix**: `deathSaveReadyAtMs` stamp on the SimClock in hero/session state. It survives zone changes and is not saved (§19.1 T11). |
| D13 | The game clock runs during cinematics/modals; buffs, cooldowns and statuses tick down while the world is frozen. Only the monster strike contact checks the cinematic; every other scheduled callback lands during it | `ZoneScene.ts:1346-1351, 2944-2946` | **Decided: §19.1.** The SimClock pauses with the world. A cinematic cancels pending monster strikes and projectiles when it begins. Hero-side pending actions resume afterwards. Story, camera and UI timings run on the presentation clock. |

### 19.1 Clock domains — decision D13 (binding for every port spec)

**Decision.** The core has exactly one gameplay clock, `SimClock`. It counts ms as a `double`, starts at 0 for the session, is never reset on a zone change and never runs backwards. It advances only inside a sim step, by the fixed step `kSimStepMs = 1000/60`. **It does not advance while the world is frozen.** Everything the core does not own runs on the **presentation clock**: the story overlay, cutscene camera, UI timers and feel effects. UE owns that clock, and it is undilated real time.

This settles a conflict between specs. D13 and `quests-story-ch1.md` Q10/OQ7 say "pause the clock". Other sections describe the web's monotonic clock and timers that keep firing during a cinematic:
- `combat-feel.md` §0 and §5.2
- `monsters-ai.md` §0 (Time, Timers), §3.9 steps 0–1 and §4.3
- `world-map-nav.md` §0 (Time)
- `quests-story-ch1.md` §8.2 ("wait delay ms (game clock)") and §8.4 last bullet

Those sections remain correct descriptions of the web. The port follows this section instead:
- Where they say "abort at contact if a story cinematic is active", the port reaches the same result by cancelling at freeze begin (rule F2).
- Where they say "drain due timers at the start of each tick", that holds only for unfrozen steps.

**Web behaviour being replaced (verified):**
- `ZoneScene.update` returns at the top while `storyDirector.cinematic || dungeonChoosing || abyssModalOpen()` (`ZoneScene.ts:1346-1351`). The scene clock keeps running anyway:
  - `Clock.now` is the game-loop timestamp.
  - Every `time.delayedCall` keeps accumulating frame `delta` (`node_modules/phaser/src/time/Clock.js:358-389`). The ZoneScene clock is never paused or time-scaled.
- So during a freeze, three things happen:
  - **(a) Absolute stamps keep elapsing.** This covers buff `startTime`, cooldown end times, status `startTime`/`lastTickTime`, `lastAttackTime`, dodge and the input buffer. Their effects are processed in a burst on the first unfrozen frame: buffs are pruned, DoTs fire `floor(elapsed/interval)` catch-up ticks even past expiry (Q21), and both swing timers are instantly ready.
  - **(b) Every scheduled callback fires on time.**
  - **(c) Per-frame `delta` accumulators inside `update` stop.** These include spirit drain (`Player.ts:225`), regen, the boss scan (`StoryDirector.ts:297-300`), the escort/defend timers, the Ember Tower `tick` and the dungeon curse.
- Only one scheduled callback checks the cinematic: `resolveMonsterStrike` returns at contact (`ZoneScene.ts:2946`). The others do not, so they deal damage, kill monsters and pay out exp/loot during a cutscene:
  - the monster projectile arrival (`:2956-2960`; an FxEngine task advanced on `POST_UPDATE` by real `delta` clamped to 100 ms, `SkillEffectSystem.ts:183-211`, `FxEngine.ts:84-88, 163-166`)
  - the hero strike contact (`:2843`, no check in `:2849`)
  - the skill release (`:2522-2527`)
  - the AoE hit delays (`:2754`, `:2764`)
  - skill projectile arrivals (`:2798-2801`)

  A monster projectile already in flight can hurt, or kill, the hero while the cutscene plays.
- Web artefact: `time.now` and the `delayedCall` timers drift apart. `time.now` is the raw rAF timestamp, while `delayedCall` advances by the sum of the smoothed and clamped `delta` (`node_modules/phaser/src/core/TimeStep.js:563-585, 705-742`). They diverge on frame hitches. After a hidden tab or the portrait-mode sleep they jump apart: stamps expire, but delayed calls do not advance. Ignore this; the port uses one SimClock for both.

**Freeze predicate.** `WorldFrozen() = story.IsCinematic() || modal.IsOpen()`.
- `modal` is the labyrinth boon picker (`dungeonChoosing`, `ZoneScene.ts:1135-1143`) or any Abyss modal: tier picker, boon cards or run summary (`AbyssRunUI.ts:313-315`). This is a later milestone.
- These do **not** freeze the world:
  - inventory, shop, stash, dialogue, quest card, skill tree and character panels
  - the mini-boss pre-fight dialogue (it holds only the mini-boss)
  - a story beat's trigger delay
  - hero death
- `IsCinematic()` is set by the core `StoryDirector` when a beat **starts**: at once for a 0 ms beat, or when its delay timer (T15) fires. It stays set until the presenter calls `OnBeatPresentationFinished()`, so it includes the 450 ms camera return at the end of a cutscene (`StoryDirector.ts:207-212`).
- Web parity:
  - `setCinematic(true)` runs synchronously when the beat's `run` begins (`:89, :190, :204`).
  - The rest of that frame's update still completes.
  - A queued 0 ms beat follows the previous one with no unfrozen frame in between.
- `PopNextBeat()` hands the presenter the already-started beat. The presenter must not wait `BeatCommand.delayMs` a second time.

**Step algorithm (core):**
```
void Sim::Advance(double realDtMs) {                 // once per rendered frame, undilated delta from UE
    if (WorldFrozen()) {
        if (!wasFrozen) { OnFreezeBegin(story.IsCinematic()); wasFrozen = true; }
        acc = 0; return;                             // SimClock, timers, AI, regen, statuses all hold; no catch-up later
    }
    wasFrozen = false;
    acc += std::min(realDtMs, 250.0);                // clamp (ue58-platform §13: resume from background)
    while (acc >= kSimStepMs && !WorldFrozen()) {
        acc -= kSimStepMs; now += kSimStepMs;
        timers.DrainDue(now);                        // ascending due time; equal times in scheduling order
        if (WorldFrozen()) break;                    // a timer started a beat (T15) → web: update() returns this frame
        Update(kSimStepMs);                          // §16 order; a kill inside may start a 0 ms beat → frozen next step
    }
}   // a freeze that began inside the loop runs OnFreezeBegin at the top of the next Advance, before any further sim
```

**`OnFreezeBegin(bool cinematic)`:**
- **F1 (every freeze).**
  - Clear hold-move (`ZoneScene.ts:1349`) and the 180 ms skill input buffer. In the web the buffer simply expires during the freeze.
  - Cinematic only: also clear the hero path and attack target (`:710-715`).
  - Boon picker: also clear the hero path (`:1136`).
- **F2 (cinematic only).**
  - Cancel every pending monster strike contact (T1) and destroy every in-flight monster projectile (T2).
  - Emit `MonsterAttackCancelled{monsterId, kind: Contact|Projectile}` for each.
  - `lastAttackTime` keeps the cancelled swing, so the next swing waits the rest of `attackSpeed` in sim time.
  - This is exact web parity for contacts. Every `cutscene()` beat (quest and kill triggers, boss intros) lasts ≥ 450 ms because of the camera return, and every monster-sheet contact is due ≤ 250 ms after the swing (`monsters-ai.md` §4.2). So in the web every pending contact comes due during the cinematic and aborts.
  - Sequences and chapter cards have no return pan, so a skip could end one sooner. They start on zone entry, on a new game or after the final boss; F2 cancels regardless.
  - For projectiles F2 is a **FIX**: the web lands them.
- **F3 (everything else).**
  - Every other pending sim timer, including hero-side actions, is **kept** and resumes with the SimClock, with its exact remaining time.
  - Nothing is cancelled on a modal freeze, so a monster swing that started before the boon picker lands after it closes (the web lands it during the modal).
- There is no `OnFreezeEnd` work, because no stamp moved.

Accepted deviation: after a cutscene the web lets an attacking monster swing on the first frame, because its timer ran during the cutscene. The port makes it wait the rest of its `attackSpeed`. Hero buffs, cooldowns and statuses resume with exactly the time they had left.

**Input while frozen:** the core rejects every hero command while frozen: move, skill, dodge, target, interact and town portal.
- In the web, polled input is skipped (`:1346-1351`) and world pointer-down is ignored during a cinematic (`:775`).
- The UI-event paths `handleUiSkillClick` (`:1333-1335`) and `handleUiDodgeRequest` (`:1337-1339`) are not guarded. They are unreachable in practice because the HUD camera is hidden (`UIScene.ts:5763-5764`) and the story overlay sits on top. The port guards in the core instead.

**Decision table** (Sim = SimClock in the core, held while frozen; Pres = presentation clock in UE, real time, keeps running while frozen, stops only when the app is backgrounded):

| # | Timer / duration | Web source (clock) | Web during a freeze | Port |
|---|---|---|---|---|
| T1 | Monster strike contact (`resolveMonsterStrike`, ≤ 250 ms after swing start) | `ZoneScene.ts:2828`, checks `:2944-2948` (zone clock) | fires; **aborts** if cinematic (`:2946`); lands during a modal | **Sim.** Cancelled at cinematic begin (F2); held across a modal. The cinematic check at contact remains as a defensive assert. |
| T2 | Monster projectile flight `clamp(2·isoPx, 200, 500)` ms | `:2949-2961`, `SkillEffectSystem.ts:183-211` (FxEngine task, rAF delta) | lands and damages: no cinematic check (QUIRK) | **Sim.** Destroyed at cinematic begin (FIX); held across a modal. UE moves the projectile actor from core progress. |
| T3 | Hero basic-attack contact (`resolvePlayerStrike`) | `:2843`, resolve `:2849` | lands; can kill → loot, exp, story triggers | **Sim.** Held; resolves after the freeze at its remaining delay. |
| T4 | Skill release after wind-up; AoE per-target delays; skill projectile arrival | `:2522-2527`, `:2754`, `:2764`, `:2798-2801` | lands | **Sim.** Held. |
| T5 | Hero buffs (`startTime + duration`, incl. Unyielding / Dual Wield), skill cooldown end times, dodge cooldown 900 / i-frames 220, skill input buffer 180 | `:2810-2814`, `:1381-1405`, `Player.skillCooldowns`, `CombatInputSystem.ts:15-62, 91-105` | elapse; pruned on the first unfrozen frame | **Sim.** Remaining time preserved. Input buffer cleared at freeze begin (F1). |
| T6 | Status effects: duration, DoT `lastTickTime`, DR windows and immunity | `StatusEffectSystem.ts:79-215`, driven from `ZoneScene.ts:1487` | stamps elapse; on resume DoTs burst `floor(elapsed/interval)` ticks, also past expiry (Q21), then expire | **Sim.** Held; no burst. |
| T7 | Swing timers: hero `lastAttackTime` vs `attackSpeed`, monster `lastAttackTime` vs `def.attackSpeed` | `:2822-2824`, `:2840-2841` | elapse; both sides can swing on the first unfrozen frame | **Sim.** Held (see accepted deviation). |
| T8 | Per-step accumulators: HP/MP regen, Life Regen, spirit/resonance drain, boss scan 250, active-monster set 250, world visibility 100, quest observers 500, auto-loot 300, escort/defend 2000, random events, elite-affix behaviours, curse regen 500 | `:1352-1560`, `Player.ts:213-250`, `StoryDirector.ts:297-300`, `SimulationScheduler.ts:4-10` | stopped (inside the skipped `update`) | **Sim.** Stopped. Same as the web. |
| T9 | Monster respawn 15 000 | `:3927` → `respawnMonster` `:5598` | elapses; monster pops in during the cutscene (AI frozen) | **Sim.** Held. |
| T10 | Ground-item despawn 60 000; potion despawn 30 000 | `:3987`, `:4021` | elapse; drops can vanish during a cutscene | **Sim.** Held. |
| T11 | deathSave re-arm 60 000 | `:3061` | elapses; lost on zone change (Q24) | **Sim.** `deathSaveReadyAtMs = now + 60000` in hero state, so it survives zone changes (FIX Q24). |
| T12 | Death → respawn 1100 | `handlePlayerDied` `:914` | elapses | **Sim.** Held. |
| T13 | Town-portal channel 1500 | `useTownPortal` `:5868` | elapses; the hero can teleport during a cutscene | **Sim.** Held. |
| T14 | Hold-move re-path (`HOLD_MOVE_REPATH_MS` 120) | `:113, 855-886` | hold-move cleared on every frozen frame (`:1349`) | **Sim.** Cleared at freeze begin (F1). |
| T15 | Story trigger delay: 650 (quest accepted / turned in), 900 (`zone_entered`), 0 (`monster_killed`) | `StoryDirector.ts:120`; wait `:136` on the **zone** clock | runs; the world is live and `busy` is true from enqueue (`:72-74, 145-148`) | **Sim (core `StoryDirector`).** See T15 details below. |
| T16 | Cutscene step timings, `focus(target, ms)` pans, shake/flash hooks, 450 ms return pan (Sine.easeInOut) then follow lerp 0.08; chapter card; prologue / epilogue / credits | `StoryScene.ts:104, 112, 273, 371, 497`; `StoryDirector.ts:203-212, 247-254` | real time (StoryScene clock and the zone camera) | **Pres.** Skip (Esc / button) collapses StoryScene waits but **not** the 450 ms return pan, which is on the zone camera. Focus targets read positions from the frozen core snapshot. |
| T17 | Quest-card chain offer: 900 ms; then, if `busy`, wait for `STORY_STATE{active:false}` + 300 ms; offer only if no card or dialogue is open | `UIScene.ts:3309-3327` (UI scene clock) | runs (the UI scene is never frozen) | **Pres (UI).** Same rule; reads core `IsBusy()` and the `StoryState` event. A turn-in cutscene keeps `busy` true at 900 ms (≥ 650 + 450 ms), so the card always waits for the cutscene to end. |
| T18 | Elite-kill slow-mo: 200 ms at 0.4 on tweens and global sprite anims (basic-attack and proc kills only) | `ZoneScene.ts:2931-2940`, `VFXManager.ts:310-320`; reset on zone exit `:406-411` | real time; unaffected by the freeze | **Pres.** Real time; scales world-actor animation and VFX only. Never the sim, story overlay, UI or camera rig. See combat-feel §11.6. |
| T19 | Feel and UI effects: hit-stop, white flash 70, pain tint, recoil, impact burst, camera shake, floating text, quest popups, banners (zone 3000 + 800, quest complete 3000, level-up 2500), toasts, danger-vignette pulse | combat-feel §11–12; `ZoneScene.ts:5905, 5934, 5969` | real time (a running hit-stop gets stuck while frozen because `animator.update` is skipped; artefact) | **Pres.** |
| T20 | Rescue-event completion poll, 500 ms loop | `:3503` | runs | **Sim.** Held. |
| T21 | Later milestones: volatile-corpse fuse 650; dungeon HUD 120; run-summary 700 then 500 ms polls while story busy; autosave 800 | `:1201`, `:1011`, `:985-990` | runs | Volatile fuse: **Sim.** The rest: **Pres (UI).** |

**T15 details:**
- The delay starts when the beat reaches the head of an idle queue, which is what the web's sequential `pump` does.
- It pauses only under a modal or app background.
- When it fires, the beat starts and `IsCinematic()` becomes true.
- Zone unload discards the queue. A beat still waiting on its delay is lost, because it was already marked seen (`quests-story-ch1.md` Q14).
- Chained quest beats each wait their own delay, so the world unfreezes for 650 ms between them: `q_seal_fire_rift` → `cs_sd_finale`, then `cs_sd_helia` (`script.ts:411-412`, Ch4). Recommendation for later: a delay applies only to the first beat of a pump. Ch1 has no chained triggers.

**Presentation of a frozen world (render-only):**
- Do **not** implement the freeze with `UGameplayStatics::SetGamePaused` or a global time dilation of 0. The UE world keeps ticking so that the camera rig, the story presenter (`WBP_Story`), Niagara ambience, idle loops and widgets keep running; only the core stops stepping.
- Drive `Sim::Advance` with `FApp::GetDeltaTime()`, which is undilated. Never use `UWorld::GetTimeSeconds()`: it is dilated by slow-mo and stops under game pause.
- On app deactivate or background, stop both clocks. Resume without catch-up (250 ms clamp).
- **Actors.**
  - Locomotion stops because the path is cleared; actors play idle loops on Pres time. The web keeps looping the current sprite animation, so a walking hero walks in place — an artefact.
  - A hero attack or cast montage whose contact or release the core is still waiting for (T3/T4) is held at its current pose (play rate 0) and resumed on unfreeze, so the hit still lands on the notify. Hero projectile actors hold position.
  - Monster swing montages cancelled by F2 blend to idle in about 150 ms. Cancelled monster projectiles play a fizzle.
  - Ambient VFX, water, foliage and fire run on Pres time.
- **HUD.** Hidden while cinematic (`UIScene.ts:5763-5764`). Visible under a modal freeze.

---

## 20. Render-only items and their 3D equivalents

| web | 3D / UE equivalent |
|---|---|
| Phaser sprite-sheet cast/attack animations, contact frame index | Skeletal-mesh montages with `Release` / `Contact` anim notifies; the times in 9.4 are the targets |
| `SkillEffectSystem.play(id, …)` VFX (particles, streaks, shake) | One Niagara system per skill id (+ camera shake asset); projectile actors move for exactly `travelMs` |
| Damage numbers, crit styling, dodge "miss" text | World-space UMG widgets / Niagara text |
| Status tints (`preFX`) | Overlay material params per effect + Niagara loops |
| Skill-tree panel (tabs per tree, cards sorted by tier, pips, lock text, tooltip with current/next level values, synergy badge) | UMG skill tree; on touch the "+" acts on release and the list scrolls by drag |
| Character panel (6 stat rows with "+", derived block) | UMG character sheet |
| HUD skill bar with cooldown sweep, mana/HP orbs, spirit bar, resonance timer | UMG HUD; touch layout with skill buttons and joystick |
| Camera shake / hit-stop / slow motion | Camera shake assets, global time dilation (combat-feel spec) |
| Pointer → tile (`worldToTile`) for teleport | Line trace to the ground plane under the cursor, then world → tile |
| World freeze during a cinematic or modal (`update` returns early; the scene clock, tweens and sprite loops keep running) | The core stops stepping and the SimClock holds. The UE world keeps ticking on the presentation clock: idle loops, held hero montages, cancelled monster swings blending out, and a hidden HUD while cinematic. Never use `SetGamePaused` or dilation 0 (§19.1). |

---

## 21. Data to export to JSON

| file | content | source |
|---|---|---|
| `classes.json` | the 3 `ClassDefinition`s with every `SkillDefinition` field verbatim, in definition order, plus the recommended additive fields of 1.4 | `src/data/classes/*.ts` |
| `skill_rules.json` | `TIER_PLAYER_LEVEL {1:1,2:6,3:12}`, `TIER_TREE_POINTS {1:0,2:4,3:9}`, fallback formulas, `LOADOUT_SIZE 6`, `GROUND_AOE_SKILLS`, tier weights `[1.0 (2–8), 0.75 (9–16), 0.5 (17+)]`, cooldown floor 500, CDR cap 50, scaling defaults (0.05, 0.5, 0.02) | `SkillProgressionSystem.ts:26-36`, `CombatSystem.ts:125-181`, `ZoneScene.ts:95` |
| `skill_trees.json` | tree ids → class, i18n key, colour | section 2.2 |
| `hero_formulas.json` | HP 50/10/15, MP 30/8/3/8, damage 8/0.8/2, defense 3/0.5/1, move 120, attack interval 1000 (min 200), attack range 1.5, regen 0.5+0.05·vit and 1+0.1·spi, campfire radius 5 ×50, poison regen ×0.5, life regen 2/level, points per level 5/1, exp curve 3·L²+25·L, dodge 0.3·dex (cap 30), crit 0.2·dex+0.5·lck (cap 75), crit× 1.5+0.01·lck, resist cap 75, defence factor 0.5 | sections 3–5, 12 |
| `buff_caps.json` | `BUFF_CAPS` | `CombatSystem.ts:115-123` |
| `status_effects.json` | tick intervals, DR constants, slow floor, skill→status rule table (13.5), monster keyword rule table (13.6) | `StatusEffectSystem.ts:43-58`, `ZoneScene.ts:6204-6291` |
| `spirit_profiles.json` | the 3 profiles + SPI factor 0.015 (clamp 0–200) | `SpiritSystem.ts:32-75, 129` |
| `combat_input.json` | input buffer 180 ms, dodge cooldown 900 / i-frames 220, dodge distances 1.8 / 2.25 / 2.6, step 0.25, min 0.5, gamepad skill buttons `[0,2,3,5]` | `CombatInputSystem.ts`, `ZoneScene.ts:171-172, 2237, 2395-2414` |
| `projectile_timing.json` | fireball 300–600 (×1.5 px), ice/poison arrow 250–500, meteor 300, arrow per-target `min(260, 1.1·px)`; hero cast/contact timings 9.4 | `SkillEffectSystem.ts:38-48`, `ZoneScene.ts:2724-2728`, `CharacterAnimator.ts:187-243` |
| i18n | `data.class.*`, `data.skill.<id>.name/.desc` (zh-CN + en), `data.skillTree.*`, `data.statusEffect.*`, `data.stat.*`, `ui.character.*`, `ui.skillTree.*`, `ui.hud.spirit`, `ui.hud.resonance`, `sys.player.*`, `sys.statusEffect.*`, `zone.statusEffect.expired`, `zone.combat.*`, `zone.teleport.*` | `src/i18n/locales/zh-CN.ts`, `en.ts` |

---

## 22. Unit-test checklist for the core library

1. `tieredScale(0.18, L)` for L = 1, 2, 8, 9, 16, 17, 20 → 0, 0.18, 1.26, 1.395, 2.34, 2.43, 2.7.
2. Every cell of the tables in 10.4 (all 40 skills × L1/5/10/20).
3. `castMana` during Resonance: warrior slash L1 `ceil(8*0.85) = 7`; mage meteor L1 `ceil(35*0.6) = 21`; rogue backstab L1 `ceil(10*0.75) = 8`.
4. `cooldown(fireball, 1, cdr 60)` = `floor(2000*0.5) = 1000` (CDR clamped to 50).
5. Investment order: warrior Lv5 whirlwind → `player_level`; Lv6 with slash 1 → `tree_points`; slash 4 → `ready`; war_stomp at Lv12 with slash 10 but whirlwind 0 → `previous_tier`.
6. `addExp`: L1 + 500 → L2 with exp 472, points 5/1; next `addExp(0)` → L3 with exp 410.
7. Derived table in 2.4.
8. Damage vectors in 12.3 (with scripted RNG: first draw = dodge, second = crit).
9. Status diminishing returns (stun, base 2000, fresh record): t=10000 → 2000; t=11000 → 1000, immuneUntil 15000; t=14000 → 0 (immune; record unchanged); t=15500 → 0 (count 3, immuneUntil 18500, lastApply 15500); t=21501 → 2000 (window reset). Separate scenario: t=10000 → 2000, then t=17000 → 2000 (more than 6000 ms since the last apply). Poison refresh keeps the stronger value and does not reset `lastTickTime`; slow value 90 → speed multiplier 0.2 (floor); two burns coexist.
10. Spirit: warrior from 0 with 12 hits (8.6 each) → resonance on the 12th (103.2 clamps to 100), 6000 ms drain to 0 in exactly 6000 ms; gains ignored while resonating; `restore({value: 50, resonanceRemainingMs: 0})` → not resonating, value 50.
11. Clock domains (§19.1). Drive `Sim::Advance` with 16.667 ms frames; a cinematic is a scripted beat that ends after N real ms.
    - **(a)** A hero buff with 5000 ms left, then a 20 000 ms cinematic → 5000 ms are still left after it, and `SimClock` advanced by 0 during it.
    - **(b)** Poison applied at sim 0 (duration 5000, interval 1000), then a 20 000 ms freeze at sim 2500. Expect:
      - 2 ticks before the freeze;
      - none while frozen, and no burst on resume;
      - ticks at the first steps at or after sim 3000, 4000 and 5000, then expiry, for 5 in total (tick-then-expire order, Q21 cap).
    - **(c)** A monster swing starts 100 ms before a beat starts → on the first frozen `Advance` the contact is cancelled. Expect one `MonsterAttackCancelled{kind: Contact}`, hero HP unchanged and `lastAttackTime` unchanged. The same swing before a boon-picker modal lands 150 ms of sim time after the modal closes (contact 250).
    - **(d)** An in-flight monster projectile at cinematic begin → destroyed, no damage.
    - **(e)** A hero skill whose release is 200 ms away at cinematic begin → released 200 ms of sim time after unfreeze, against the then-live target.
    - **(f)** Turn in `q_kill_slimes` at sim time t → `IsBusy()` at once and `IsCinematic()` at t+650 sim ms. With a modal open for 1000 ms inside that window, the beat starts 1650 real ms after the turn-in.
    - **(g)** A 0 ms `monster_killed` beat started by a kill inside step k → step k completes, and no further step runs until the beat finishes.
    - **(h)** Monster respawn: kill at t, then a 10 000 ms cinematic → the respawn happens 25 000 real ms after the kill. Ground-item 60 s and potion 30 s despawns shift the same way.
    - **(i)** deathSave procs at t, the zone changes at t+10 000 → available again at t+60 000 (Q24 FIX).
