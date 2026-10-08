# Port Spec — Combat Resolution & Combat Feel

Area owner: combat. Web source of truth: branch `claude/unreal-rebuild`, TypeScript under `src/`.
Target: portable C++20 core (`AbyssCore`, no UE types, no exceptions/RTTI) + thin UE5 module (render/input/UI).

This document describes **what the web game does today**, precisely enough to re-implement it without
reading the TypeScript. Where the web behaviour is a bug or a 2D artefact, it is called out as a
**QUIRK** with a recommendation; the default is *keep the behaviour* unless the recommendation says
"FIX" and the open-questions list at the end is resolved that way.

Citations are `path:line` (line numbers at the time of writing).

---

## 0. Conventions

| Topic | Web | Port rule |
|---|---|---|
| Distance unit | **tiles** (grid col/row as floats). All ranges, radii, aggro, leash, dodge distances are in tiles. | Core works in tiles (float). UE converts with `TILE_SIZE_UU` (architecture doc; suggestion 100 uu = 1 tile). |
| Distance metric | Euclidean on (col,row) unless noted (`distanceSq`, `src/utils/IsometricUtils.ts:35`). | Same. |
| Screen px (render-only) | Iso projection `x=(col-row)*32, y=(col+row)*16` (`src/utils/IsometricUtils.ts:3`). FX code uses `TILE_R = 45` world px per tile of ground radius (`src/graphics/vfx/FxKit.ts:23`) and `FLAT = 0.5` vertical squash. | **px → tiles: divide by 45** for horizontal/ground sizes and for heights of upright things (sprites are not foreshortened). For gameplay timings that the web derives from iso *screen* distance (projectile flight), use `tileDist × 36` as the px equivalent (36 ≈ RMS iso px per tile). |
| Time | ms, `scene.time.now` (monotonic game clock, survives zone changes). `time.delayedCall(ms)` = scheduled callback on that clock. | Core owns a monotonic `nowMs` (uint64/double) advanced by the sim tick; scheduled events in a timer queue keyed on it. Never reset on level load (buff `startTime`s cross zones). |
| RNG | `Math.random()` everywhere. Helpers: `chance(p) = rand*100 < p` (p in percent), `randomInt(a,b) = floor(rand*(b-a+1))+a` (inclusive) (`src/utils/MathUtils.ts:5,13`). | Core uses an injectable seedable PRNG (e.g. PCG32) with `float01()`; same helper semantics. Bit-exact sequence parity with JS is **not** required; distributions and draw *conditions* are. |
| Rounding | JS `Math.round` (half up toward +∞), `Math.floor`, `Math.ceil`. | Use `std::floor(x + 0.5)` for `Math.round` (not `std::round`, which rounds half away from zero — differs for negatives). |
| Frame-rate dependence | A few web rules are per-frame (noted). | Core sim ticks at a fixed step (suggest 60 Hz) so per-frame rules stay identical. |

---

## 1. Data model

### 1.1 `EquipStats` — aggregated equipment-derived modifiers (`src/systems/CombatSystem.ts:6-54`)
All fields `number`, default 0 (`emptyEquipStats`, `:56-73`). Percent fields are whole percents (10 = 10 %).

| Field | Meaning in combat | Consumer |
|---|---|---|
| `damage` | flat added to base damage | calc §2 step 6 |
| `damagePercent` | % multiplier on base damage (only if > 0) | §2 step 7 |
| `defense`, `defensePercent` | defender flat / % defense | §2 step 11 |
| `maxHp`, `maxHpPercent`, `maxMana`, `maxManaPercent` | derived stats | §1.4 |
| `critRate` | + crit chance (percent points) | §2 step 2 |
| `critDamage` | + crit multiplier (percent → /100) | §2 step 3 |
| `attackSpeed` | % faster basic attacks | §1.4 |
| `lifeSteal`, `manaSteal` | % of final damage returned | §2 step 17 |
| `hpRegen`, `manaRegen` | flat /s regen | Player regen |
| `fireDamage`, `iceDamage`, `lightningDamage`, `poisonDamage` | flat elemental added to *every* hit (untyped sum) | §2 step 8 |
| `fireResist`, `iceResist`, `lightningResist`, `poisonResist`, `allResist` | defender resist (%) | §2 step 15 |
| `moveSpeed` | % move speed | §1.4 |
| `magicFind`, `expBonus` | loot / exp on kill | §11.1 |
| `cooldownReduction` | % skill CDR, clamped 0..50 | §3 |
| `knockback` | **unused** in combat code | — |
| `str,dex,int,vit,spi,lck` | primary-stat bonuses | §1.4, §2 |
| `killHealPercent` | heal % maxHp on kill | §11.1 |
| `deathSave` | > 0 ⇒ once per 60 s survive lethal hit at 30 % HP | §5.4 |
| `critDoubleStrike` | % chance of an extra basic hit after a crit | §4.3 |
| `doubleShot` | % chance of extra basic hit for **ranged** attackers (attackRange > 2) | §4.3 |
| `freeCast` | % chance to refund a skill's mana | §6.3 |
| `elementalDamagePercent` | % multiplier on the flat elemental sum | §2 step 8 |
| `ignoreDefense` | % of defender defense ignored | §2 step 12 |
| `damageReduction` | defender permanent DR in % (added to buff DR) | §2 step 10 |
| `thornsHeal` | heal % maxHp each time the hero takes a monster hit | §5.3 |
| `dodgeCounter` | > 0 ⇒ after a stat-dodge the next basic attack is a guaranteed crit | §4.3 |

Hero equip stats = inventory aggregate (inventory spec) **+** achievement bonuses **+** active pet passive
(except `expBonus`/`magicFind`) **+** altar blessing **+** labyrinth boons when in a run. Cached, invalidated on
equip change / pet change (`src/scenes/ZoneScene.ts:3143-3176`). Monsters have **no** equip stats.

### 1.2 `CombatEntity` (`CombatSystem.ts:75-92`)
`{ id, name, hp, maxHp, mana, maxMana, stats{str,dex,vit,int,spi,lck}, level, baseDamage, defense, attackSpeed(ms),
attackRange(tiles), buffs: ActiveBuff[], equipStats?, outgoingDamageMultiplier? }`.

* Hero → `Player.toCombatEntity(eq)` (`src/entities/Player.ts:390-408`): `stats` = **base allocated stats only**
  (no gear), `baseDamage`/`defense` = derived (§1.4, gear included), `equipStats = eq`,
  `outgoingDamageMultiplier = spirit.damageMultiplier` (§9). `buffs` is the live array (by reference).
* Monster → `Monster.toCombatEntity()` (`src/entities/Monster.ts:355-371`): `baseDamage = def.damage`,
  `defense = def.defense`, `level = def.level`, `stats` from construction (§1.5), live `buffs`, no equip.

### 1.3 `ActiveBuff` (`CombatSystem.ts:94-101`)
`{ stat: string, value: number, duration: ms, startTime: ms, tag?: string }`. Active while
`now - startTime < duration`.

Buff stats read by combat (all additive per stat, then capped by `BUFF_CAPS`, `CombatSystem.ts:115-123`):

| stat | cap | read on | effect |
|---|---|---|---|
| `damageReduction` | 0.9 (also caps buff+gear total) | defender | `× (1 − DR)` |
| `defenseBonus` | 2.0 | defender | effective defense × (1 + v) |
| `damageBonus` | 3.0 | attacker | final × (1 + v) |
| `attackSpeed` | 1.0 | — | **readable but not applied anywhere** (QUIRK, keep) |
| `poisonDamage` | 5.0 | attacker | + `attacker.baseDamage × v` to the flat elemental sum |
| `stealthDamage` | 5.0 | attacker | final × (1 + v); removed after the hero's next basic attack |
| `manaShield` | 0.9 | defender | fraction of final damage redirected to mana |
| `damageAmplify` | none | defender | final × (1 + v) (death_mark on monsters, curse aura on hero) |
| `taunted` | none | monster | inert marker (§6.5) |
| `critBonus`, `slowEffect` | none | — | written by shadow_step / slow_trap definitions, **never read** (QUIRK) |

`getBuffValue(entity, stat)` sums **all** entries with that stat regardless of age, then caps
(`CombatSystem.ts:214-226`). Expiry is only by pruning:
* Hero: pruned every frame in `handleCombat` while alive (`ZoneScene.ts:2811-2815`).
* Mercenary: pruned in `updateMercenary` (`ZoneScene.ts:6426`).
* **Monsters: never pruned** → death_mark's `damageAmplify` lasts until death and stacks uncapped on recast
  (QUIRK; recommend FIX: prune monster buffs each tick by `startTime+duration`).

### 1.4 Hero derived stats (`Player.ts:106-154`, recomputed every frame with current equip stats)
```
eStr = stats.str + eq.str ; eVit = stats.vit + eq.vit ; eSpi = stats.spi + eq.spi ; eInt = stats.int + eq.int
maxHp     = floor((50 + eVit*10 + (level-1)*15 + eq.maxHp) * (1 + eq.maxHpPercent/100))
maxMana   = floor((30 + eSpi*8 + eInt*3 + (level-1)*8 + eq.maxMana) * (1 + eq.maxManaPercent/100))
baseDamage = 8 + eStr*0.8 + level*2              (float, not floored)
defense    = 3 + eVit*0.5 + level                (float)
moveSpeed  = floor( floor(120 * (1 + eq.moveSpeed/100)) * spirit.moveSpeedMultiplier )
attackSpeed (ms between basic attacks) = max(200, floor(1000 * (1 - eq.attackSpeed/100)))
attackRange = 1.5 tiles (all classes; mage/rogue basic attacks are melee too)
hpRegen/s   = 0.5 + stats.vit*0.05 + eq.hpRegen      (base stats only for the stat part)
manaRegen/s = 1 + stats.spi*0.1 + eq.manaRegen
```
Regen multipliers: within 5 tiles of a camp ×50 HP and ×50 MP (`ZoneScene.ts:83-86,1584-1594`); poisoned ×0.5 HP
(`:1366-1369`). Passive `life_regen`: +2 HP/s per level × the same HP multiplier (`:1373-1379`).

### 1.5 Monster combat stats (`Monster.ts:87-96`)
`maxHp = def.hp`; `stats = { str: floor(def.damage*0.8), dex: floor(def.speed*0.1), vit: floor(def.hp*0.1), int: 3, spi: 3, lck: 3 }`.
Definitions are already difficulty-scaled (§12) before construction; elite affixes then modify them (§13.3).

---

## 2. Damage calculation — `CombatSystem.calculateDamage` (`CombatSystem.ts:229-381`)

Signature: `calculateDamage(attacker, defender, skill?, skillLevel=1, skillLevels?, forceCrit=false) → DamageResult`.
`DamageResult = { damage:int, isCrit, isDodged, damageType, lifeStolen:int, manaStolen:int, manaDamage?:int }`.

Exact pipeline (A = attacker, D = defender, `aEq`/`dEq` = their equipStats or none):

1. **Dodge**: `dodge% = clamp((D.stats.dex + dEq.dex) * 0.3, 0, 30)`; draw `chance(dodge%)` (**always one RNG draw**).
   Dodged ⇒ return `{damage:0, isCrit:false, isDodged:true, damageType:'physical', steals 0}`.
2. **Crit chance**: `crit% = clamp((A.stats.dex + aEq.dex)*0.2 + (A.stats.lck + aEq.lck)*0.5 + skill.critBonus + aEq.critRate, 0, 75)`.
   `isCrit = forceCrit || chance(crit%)` (no draw when forced).
3. **Crit multiplier** = `isCrit ? 1.5 + (A.stats.lck + aEq.lck)*0.01 + aEq.critDamage/100 : 1`.
4. `damageType = skill?.damageType ?? 'physical'`.
5. **Base**: with skill: `base = A.baseDamage + (physical ? A.stats.str : A.stats.int) * 0.5`,
   `mult = getSkillDamageMultiplier(skill, lvl) × (skillLevels ? getSynergyBonus(skill, skillLevels) : 1)` (§3).
   Without skill: `base = A.baseDamage + A.stats.str * 0.5`, `mult = 1`. (`stats` = base stats, not gear.)
6. `base += aEq.damage`.
7. if `aEq.damagePercent > 0`: `base *= 1 + aEq.damagePercent/100`.
8. **Flat elemental** `elem = aEq.fire+ice+lightning+poisonDamage`; if `aEq.elementalDamagePercent > 0`: `elem *= 1 + pct/100`.
   Then `elem += A.baseDamage × buff(A,'poisonDamage')` if that buff > 0.
9. `stealth = buff(A,'stealthDamage')`, `dmgBonus = buff(A,'damageBonus')`.
10. **DR** = `min(buff(D,'damageReduction') + dEq.damageReduction/100, 0.9)`.
11. **Effective defense** `def = D.defense`; `+ dEq.defense` if > 0; `× (1 + dEq.defensePercent/100)` if > 0;
    `× (1 + buff(D,'defenseBonus'))` if > 0.
12. `def *= 1 − aEq.ignoreDefense/100` if > 0.
13. `out = clamp(A.outgoingDamageMultiplier ?? 1, 0, 10)`.
    `raw = (base × mult × critMult + elem) × out` (note: elemental is **not** crit-multiplied).
14. `afterDef = max(1, raw − def × 0.5)`; `final = afterDef × (1 − DR)`;
    `× (1+dmgBonus)` if > 0; `× (1+stealth)` if > 0; `× (1 + buff(D,'damageAmplify'))` if > 0.
15. **Resistance** (only if `damageType != 'physical'`): `res = clamp(dEq.allResist + dEq.<type>Resist, 0, 75)`
    (arcane has no specific resist → allResist only); `final *= 1 − res/100`. Monsters have no resist.
16. `final = max(1, floor(final))`.
17. **Mana shield**: `ms = buff(D,'manaShield')`; if `ms > 0 && D.mana > 0`: `redirect = floor(final × ms)`,
    `absorb = min(redirect, D.mana)`, `final = max(1, final − absorb)`, `manaDamage = absorb`.
    *The caller must subtract `manaDamage` from the defender's mana* — **QUIRK: ZoneScene never does** (only the
    unused `applyDamage` does, `:394-397`), so mana shield currently reduces damage for free. Recommend FIX
    (deduct mana in the hit-apply path).
18. **Steal**: `lifeStolen = floor(final × aEq.lifeSteal/100)`, `manaStolen = floor(final × aEq.manaSteal/100)` (0 when pct ≤ 0).

Minimum damage of an undodged hit is always **1**. A dodged result has `damage 0`.

Callers that ignore `isDodged` (skills, mercenaries, critDoubleStrike/doubleShot extra hits) apply 0 damage and show a
"0" number with a light hit reaction (QUIRK; recommend FIX: treat as MISS — see Open Questions).

`applyDamage` (`:383-410`, emits `COMBAT_DAMAGE` and `MONSTER_DIED`) is **dead code** in the web game.

### 2.1 Worked examples (use as C++ unit-test vectors; RNG forced "no dodge")
Warrior Lv1 (str12 dex8 vit10 int5 spi5 lck5, no gear): maxHp 150, maxMana 85, baseDamage 19.6, defense 9.
* Basic vs `slime_green` (def 2): base 25.6 → raw 25.6 → 25.6−1 = 24.6 → **24**; crit (×1.55): 39.68−1 → **38**. Crit chance 4.1 %; slime dodge 1.2 %.
* Basic vs `goblin` (def 4): **23**.
* `goblin` (dmg 8 → str 6) hits warrior: base 11 − 4.5 → **6**; with shield_wall (DR 0.5): 6.5×0.5 → **3**.
* `goblin_chief` (dmg 14 → str 11): 19.5 − 4.5 → **15**; with `extra_strong` (dmg 18, str 14): 25 − 4.5 → **20**.

Mage Lv1 (str4 dex6 vit6 int14 spi10 lck5): baseDamage 13.2.
* `fireball` L1 vs goblin: base 13.2 + 14×0.5 = 20.2; mult 1.8 → 36.36 − 2 → **34** (fire; monster resist 0); crit 54.358 → **54**.
* `fireball` L3: mult 1.8 + 2×0.20 = 2.2.

---

## 3. Skill scaling helpers (`CombatSystem.ts:129-197`)

```
tieredScale(perLevel, level) = Σ_{i=2..level} ( i<=8 ? perLevel : i<=16 ? 0.75*perLevel : 0.5*perLevel )   (0 if level<=1)
damageMult(s,L)   = s.damageMultiplier + tieredScale(s.scaling?.damagePerLevel ?? 0.05, L)
manaCost(s,L)     = floor(s.manaCost + tieredScale(s.scaling?.manaCostPerLevel ?? 0.5, L))
cooldown(s,L,cdr) = floor( max(500, floor(s.cooldown - tieredScale(s.scaling?.cooldownReductionPerLevel ?? 0, L)))
                           * (1 - clamp(cdr,0,50)/100) )          // min 500 applies before CDR → can reach 250
aoeRadius(s,L)    = (s.aoeRadius ?? 0) + tieredScale(s.scaling?.aoeRadiusPerLevel ?? 0, L)
buffValue(s,L)    = (s.buff?.value ?? 0) + tieredScale(s.scaling?.buffValuePerLevel ?? 0.02, L)
buffDuration(s,L) = floor((s.buff?.duration ?? 0) + tieredScale(s.scaling?.buffDurationPerLevel ?? 0, L))
synergy(s, levels)= 1 + Σ syn.damagePerLevel × levels[syn.skillId]
```
Hero mana cost actually charged = `ceil(manaCost(s,L) × spirit.manaCostMultiplier)` (`Player.ts:350-357`).
Cooldown/mana are committed at cast start (`Player.useSkill`, `Player.ts:379-388`): `cooldownUntil = now + cooldown`,
`mana = max(0, mana − cost)`, emits `SKILL_USED {skillId, damageType}`. Ready when `now >= cooldownUntil`.

---

## 4. Hero basic attack (auto-attack)

### 4.1 Loop — `handleCombat` (`ZoneScene.ts:2808-2846`), every frame while hero HP > 0
1. Prune expired hero buffs.
2. Monster attacks (§5.1).
3. Pick target: if hold-to-move is active → **no auto-attack**; else `attackTarget` (if alive) else the **nearest
   aggro monster** (state chase/attack) anywhere on the map.
4. If `dist² <= attackRange²` (1.5 tiles) and `now − lastAttackTime >= attackSpeed`:
   `lastAttackTime = now`; `delay = player.playAttack(target)` (contact ms, §10); schedule `resolvePlayerStrike(target)` at `now+delay`.

`lastAttackTime` starts at 0 → first swing is immediate. Basic attacks keep firing while a skill is mid-cast and while
walking (the attack animation interrupts the visual cast/walk; gameplay timers are independent).

### 4.2 Contact resolution — `resolvePlayerStrike` (`ZoneScene.ts:2849-2928`)
Abort if hero dead, target dead, or zone transitioning. Then in order:
1. `forceCrit = dodgeCounterReady`; if set, clear it and log `zone.combat.dodgeCounterCrit` (consumed even if the
   target then dodges — QUIRK, keep).
2. `r = calculateDamage(hero(eq), target, no skill, 1, -, forceCrit)`.
3. If `r.isDodged`: floating **MISS** at target; **return** (no flash/impact/procs, stealth not consumed).
4. `weight = target.takeDamage(r.damage, heroPos, {isCrit})` (§11.2 target-side reaction).
5. `applySteal(r)`: if damage > 0 gain Spirit `'hit'` (crit flag); add lifeStolen/manaStolen (capped at max).
6. Floating number (monster style, no element colour).
7. Remove **all** `stealthDamage` buffs.
8. Emit `COMBAT_DAMAGE {targetId, damage, isDodged:false, isCrit, isPlayerTarget:false, targetMaxHP}`;
   `playHitImpact(target, weight)` (§11.3 attacker-side).
9. Slash trail at target chest (render: thin lens streak 170 ms + wide afterglow 220 ms, colour `0xffffcc`).
10. **critDoubleStrike**: if crit && `eq.critDoubleStrike > 0` && target alive && `rand*100 < pct`: second
    `calculateDamage` (fresh rolls, no force, dodge ignored), takeDamage, steal, number 20 px higher, log
    `zone.combat.comboTrigger`, playHitImpact.
11. **doubleShot**: if `eq.doubleShot > 0` && target alive && `attackRange > 2` && roll → same as above, number offset
    (+15,−15) px, basic-attack VFX, log `zone.combat.doubleArrow`. Hero attackRange is 1.5 so this **never fires** today.
12. If target died: `onMonsterKilled` (§11.1), clear `attackTarget` if it was this one, emit `TARGET_CHANGED {null}`.

### 4.3 Proc helpers (`CombatSystem.ts:439-493`)
`checkCritDoubleStrike(pct, isCrit, rng) = isCrit && pct>0 && rng*100 < pct`; `checkDoubleShot(pct, range, rng) = pct>0 && range>2 && rng*100<pct`;
`checkFreeCast(pct, rng) = pct>0 && rng*100<pct`; `calcKillHeal/ThornsHeal = floor(maxHp*pct/100)`.

---

## 5. Monster attacks on the hero

### 5.1 Scheduling (`ZoneScene.ts:2817-2830`)
For each monster within 12 tiles of the hero: alive, `state == 'attack'`, not immobilized (freeze/stun), and
`now − lastAttackTime >= def.attackSpeed` → (pet may intercept: later milestone hook `PetCompanion.interceptMonsterAttack`)
→ `lastAttackTime = now`; `delay = monster.playAttack(heroPos)` (contact ms + wind-up telegraph, §10.3);
schedule `resolveMonsterStrike` at `now+delay`.

Monster AI state relevant to combat (`Monster.ts:160-239`; full AI in the monster spec): idle → chase when hero within
`aggroRange`; chase → attack when `dist <= attackRange`; attack → chase when `dist > attackRange × 1.2`;
chase → idle when `dist > aggroRange × 1.5`; leash: a non-idle monster > 8 tiles from its spawn → state idle, one
movement step toward spawn and a one-off heal of 1 % maxHp (it then drifts home only through idle→patrol, which every
3000 ms picks a walkable tile within ±2 of spawn, and re-aggros if the hero is inside `aggroRange`). While in `attack`,
the monster faces the hero.
Safe zones (camp radius `mapData.safeZoneRadius ?? 9`): monsters inside are forced idle; non-aggro monsters do not
see a hero who stands in a safe zone (`ZoneScene.ts:1430-1470`).

### 5.2 Contact — `resolveMonsterStrike` (`ZoneScene.ts:2944-2968`)
Abort if monster dead, hero dead, transitioning, story cinematic active, or the monster is now immobilized
(stun/freeze interrupts the swing).
* **Ranged** iff `def.attackRange > 2.5`: launch a projectile at the hero's position *at contact time*; flight
  `clamp(dist_px*2, 200, 500)` ms (`SkillEffectSystem.ts:183-212`; port: `dist_px = tileDist×36`). On arrival, if
  monster and hero alive → `applyMonsterHit(ranged=true)`. **No re-check of distance**: ranged hits always land
  unless dodged/iframed. Projectile tint: spriteKey contains `fire`/`phoenix` → `0xff6600`; `ice` → `0x4488ff`; else `0xcc44cc`.
* **Melee**: whiff silently if `dist > attackRange×1.35 + 0.5` (hero stepped away during the wind-up); else `applyMonsterHit(false)`.

### 5.3 Hit application — `applyMonsterHit` (`ZoneScene.ts:2970-3069`)
`r = calculateDamage(monster, hero(eq))` is rolled **first** (RNG consumed even when iframed). Then:
1. **Dodge-roll iframes active** (§8.1): if this is the first hit avoided in this iframe window → Spirit `'dodge'`.
   MISS text on hero; emit `COMBAT_DAMAGE {targetId:'player', damage:0, isDodged:true, isPlayerTarget:true}`;
   white glint + 6 sparks on hero ("perfect evade"). Nothing else.
2. **Stat dodge** (`r.isDodged`): Spirit `'dodge'` (every time); MISS text; if `eq.dodgeCounter > 0` arm
   `dodgeCounterReady` + log `zone.combat.dodgeCounterReady`. (No `COMBAT_DAMAGE` emitted → no miss SFX; QUIRK, keep.)
3. **Hit**: `hp = max(0, hp − r.damage)` (difficulty already in the monster def).
   * `thornsHeal > 0` and alive: `hp += floor(maxHp × thornsHeal/100)` (capped).
   * Feedback: `w = classifyHit({damage, maxHp:hero.maxHp, isCrit})` (never `kill`); `hero.playHurt(from monster, HIT_PROFILES[w].recoil)`;
     `monster.triggerHitFreeze(round(HIT_PROFILES[w].attackerStopMs × 0.6))`; red floating number; melee only: claw-rake VFX on hero;
     emit `COMBAT_DAMAGE {targetId:'player', damage, isCrit, isPlayerTarget:true, targetMaxHP}` (drives player-hit shake + SFX).
   * Monster on-hit status effects (§7.3).
   * Elite on-hit (§13.4): fire_enhanced extra fire damage, vampiric heal, frozen slow.
   * **Death check**: if `hp <= 0`: if `eq.deathSave > 0` and not used → `hp = floor(maxHp×0.3)`, mark used, re-arm after
     60 000 ms, log `zone.combat.deathImmunity`, heal burst VFX. Else `killPlayer()` (pet revive hook, then `Player.die()`, §11.4).

Hero DoT deaths and the labyrinth volatile-corpse burst call `killPlayer()` **without** the deathSave check (QUIRK, keep).

---

## 6. Skills — input, gating, execution

### 6.1 Input → request (`ZoneScene.ts:2158-2302`)
* Keys `1..6` → loadout slot i; gamepad buttons 0,2,3,5 → slots 0..3; HUD/touch buttons emit `UI_SKILL_CLICK {index, skillId}`.
* Loadout = the class's skills with level > 0, in definition order, first 6 (`SkillProgressionSystem.ts:52-60`).
  Passives (`life_regen`, `unyielding`, `dual_wield_mastery`) are included and pressable (QUIRK, see §6.6).
* `requestSkill(id)`: not learned → log `zone.combat.skillLocked`. Else `CombatInputBuffer.request(id, now, canExecute(id))`:
  executable → `tryUseSkill`; otherwise buffered and `SKILL_BUFFERED {skillId, expiresAt}` emitted.
* **Input buffer** (`CombatInputSystem.ts:15-61`, window **180 ms**, `ZoneScene.ts:171`): single slot, newest request
  replaces older. Each frame `consumeReady(now, canExecute)`: drop if `now > expiresAt`; if executable → consume and
  `tryUseSkill`. (A press up to 180 ms before cooldown/mana/range allows fires automatically.)

### 6.2 `canExecuteSkill` (`ZoneScene.ts:2248-2267`)
False if hero dead, unknown/unlearned, on cooldown, `mana < cost`, or `teleport` while immobilized.
`target = preferredTarget()` (§9.2). No target → allowed only for buff / aoe / teleport skills. With a target: buff,
aoe, teleport always allowed; otherwise `dist² <= (range+1)²`.

### 6.3 `tryUseSkill` (`ZoneScene.ts:2462-2528`)
Re-checks the same gates (messages: `zone.combat.manaInsufficient`, `zone.teleport.blockedByCC`). Then:
1. `useSkill` (cooldown + mana + `SKILL_USED`).
2. **freeCast**: `eq.freeCast > 0 && rand*100 < pct` → refund the mana just spent, log `zone.combat.freeCast`.
3. Animation choice:
   * `buff || aoe || range > 2` → `playCast()` (faces the target when there is one and the skill is not a buff).
   * else (melee single target) → `playAttack(target)` (uses basic-attack contact timing, compressed by attackSpeed); no target → `playCast()`.
4. `releaseDelay` = returned contact/charge ms (§10). `teleport` and `shadow_step` (or delay ≤ 0) release **immediately**;
   others release at `now + releaseDelay`. At release: abort if hero dead / transitioning; if the original target died,
   **retarget** to `preferredTarget()` (may be null).

Mana and cooldown are spent even if the release then finds nothing to hit.

### 6.4 `releaseSkill` behaviour classes (`ZoneScene.ts:2530-2806`), checked in this order

| # | Class | Skills | Rule |
|---|---|---|---|
| 1 | **Blink to cursor** | `teleport` | Destination = pointer ground tile (desktop; **no range limit**, only map clamp `[1, size−2]`). Touch (pointer claimed by a button): joystick dir × 6 tiles if `length(dir) > 0.2`, else current target tile, else self. Rounded; if not walkable search square rings r=1..3 (row-major `dr=-r..r`, `dc=-r..r`) for the first walkable in bounds; none → refund mana (cooldown kept), log `zone.teleport.unreachable`. Then instant move, clear path + attackTarget, VFX. |
| 2 | **Step behind** | `shadow_step` (tier 3), **only with a target** (else falls to #5) | Tile = `round(target − unit(hero−target))` clamped; if not walkable → target's tile. Instant move, set attackTarget, push its buff (inert `critBonus`). |
| 3 | **Mark** | `death_mark`, **only with a target** (else falls to #5 — quirk 16) | Push `damageAmplify` (buffValue, buffDuration) onto the **monster**; if `damageMultiplier > 0` also one hit (calculateDamage with skill, dodge ignored) — kill check, no status effect, no impact burst. Log `zone.combat.deathMarkApplied`. Because the skill has a `buff`, **no range check** is applied anywhere (§6.2/§6.3): works on the preferred target at any distance (same for shadow_step). |
| 4 | **Slow trap** | `slow_trap` | Monsters within `aoeRadius(L)` of the **hero**: damage each (dodge ignored), kill check; survivors get status `slow` value `round(buffValue×100)` (40 at L1) for `buffDuration` (5000). No impact burst. Log `zone.combat.slowTrapHit {count}`. |
| 5 | **Self buff** | any skill with `buff` (shield_wall, ice_armor, mana_shield, frenzy, iron_fortress, taunt_roar, poison_blade, vengeful_wrath, vanish …) | Push `{stat: buff.stat, value: buffValue(L), duration: buffDuration(L), startTime: now}` on the hero, log `zone.combat.skillActivated`, VFX at hero; heal burst if stat is `hp` or id contains `heal`. **No damage** even if `damageMultiplier > 0` (poison_blade). `taunt_roar` additionally: every alive monster within `aoeRadius(L)` gets a `taunted` buff and idle/patrol → chase; log `zone.combat.tauntRoar {count}`. |
| 6 | **AoE** | `aoe && aoeRadius(L) > 0` | See §6.5. |
| 7 | **Single target** | everything else with a target | VFX caster→target; `travel = getProjectileTravelMs` (below). Hit at `now + travel` (abort if target or hero dead) or immediately. Hit = calculateDamage(skill, L, skillLevels) [dodge ignored] → `combustion` ×1.5 (floor) if target burning → takeDamage → steal → element-coloured number → skill status (§7.4) → kill check → impact burst (`skillImpactColor`) → ground scorch if `damageType != physical` or `damageMultiplier > 1.5` (type: id has `fire`/`meteor` → fire; `ice`/`blizzard` → ice; else **lightning** — QUIRK: poison/physical get a lightning scorch; recommend map by damageType). |

No target and not covered above → nothing happens (mana already spent). **There are no channelled skills** in the
web game; every skill resolves as one instant (or once-delayed) burst. A future channel type would be a new class here.

`charge` has no movement — it is a range-5 instant single-target hit (QUIRK, keep for ch1; the VFX streaks sell a dash).

**Projectile flight** (`SkillEffectSystem.ts:38-48`), `dist_px` = iso screen distance caster(−16 px)→target(−16 px); port `tileDist×36`:
`fireball` `clamp(dist×1.5, 300, 600)`; `ice_arrow`, `poison_arrow` `clamp(dist×1.5, 250, 500)`; `meteor` 300 (fall); others 0.
Projectiles are target-locked: damage lands on the target wherever it is on arrival.

### 6.5 AoE resolution (`ZoneScene.ts:2700-2768`)
* `R = aoeRadius(L)`. **Ground-targeted** set `GROUND_AOE_SKILLS = {meteor, blizzard, fire_wall, arcane_torrent, arrow_rain, poison_cloud}` (`:95`):
  anchor = chosen target if alive and `dist² <= (range+1)²`, else nearest alive monster with `dist² <= (range+1)²`, else none (`:2305-2318`).
  Centre = anchor's position or the hero.
* Target set (computed **at release**, positions not re-checked for delayed hits):
  * `piercing_arrow` with a target: line from hero toward target, length `range`, half-width `R×0.6`; monster qualifies if
    `0 <= along <= range` and `|perp| <= halfWidth` (`:2321-2333`). Without a target: circle R around the hero (QUIRK).
  * everything else: alive monsters with `dist(centre) <= R` (Euclidean on tile coords).
* Delays: `aoeDelay = getProjectileTravelMs(id)` (only `meteor` > 0 → 300 ms, all targets together). Else per target:
  `multishot`/`piercing_arrow` hit at `min(260, dist_px × 1.1)` ms (port `tileDist×36×1.1`); all others immediate
  (`chain_lightning` hits everyone at once though its bolts are drawn 55 ms apart).
* Per-target hit (`applyHit`): skip if already dead; `from` = blast centre for ground skills (except multishot/piercing)
  when the target is > 4 px (~0.1 tile) from it, else the hero; calculateDamage (dodge ignored) → combustion bonus →
  takeDamage(from, isCrit) → steal → number → skill status (§7.4) → kill check → impact burst toward the target.
* After the immediate batch (or the meteor batch): if ≥ 1 hit, `cameraShake(100, 0.004 + hits×0.001)` (subject to throttles, §11.5).
* `chain_lightning` / `multishot` VFX receive the list of target points; others receive the centre.

### 6.6 Passive skills in the loadout (QUIRK)
`life_regen` (L1 from start) and `dual_wield_mastery` have no buff/aoe, range 0, cost 0: pressing one with a monster
within 1 tile casts a 0-multiplier melee "hit" (damage floors to 1) on a 500 ms cooldown. `unyielding` has a `buff`, so
pressing it casts its damage-reduction buff manually (cost 0) and starts the 60 s cooldown it shares with the auto-proc.
Auto-battle walks the same list and does all of this (§9.4). Recommend FIX: add `passive: true` to skill data and exclude
passives from loadout, casting and auto-priority.
Passive effects themselves: `life_regen` regen (§1.4); `unyielding` auto-proc when `hp/maxHp < 0.3` and ready → push
`damageReduction` buff (buffValue, buffDuration), set cooldown (60 s base), VFX, log `zone.combat.unyieldingProc`
(`ZoneScene.ts:1381-1394`); `dual_wield_mastery` (tier 3) keeps a 2000 ms `damageBonus` buff of `0.03×level` tagged
`dualWieldMastery` while weapon+offhand equipped (`:1396-1410`).

### 6.7 Skill behaviour table (all 40 skills; **Ch1** = tiers 1–2, unlockable at hero Lv ≤ 10)
Tier gates: tier 1 at Lv1 (all tier-1 skills start at level 1), tier 2 at Lv6 with ≥ 4 points in the tree,
tier 3 at Lv12 with ≥ 9 points and a previous-tier skill (`SkillProgressionSystem.ts:26-36,48-50`).

| Skill | Tier | Class (§6.4) | Anim | Release | Shape / notes | Status (§7.4) | Ch1 |
|---|---|---|---|---|---|---|---|
| slash | 1 | single | attack | contact | melee r1.5 | – | ✔ |
| whirlwind | 2 | AoE | cast | charge | circle R2.5 on hero | – | ✔ |
| war_stomp | 3 | AoE | cast | charge | circle R2 on hero | stun 2000 | |
| shield_wall | 1 | buff | cast | charge | DR 0.5 / 5 s | – | ✔ |
| taunt_roar | 2 | buff+taunt | cast | charge | defenseBonus 0.3 / 4 s; taunt R3 | – | ✔ |
| vengeful_wrath | 3 | buff | cast | charge | damageBonus 0.25 / 6 s | – | |
| charge | 2 | single | cast | charge | range 5, no movement | – | ✔ |
| lethal_strike | 3 | single | attack | contact | critBonus 15 | – | |
| dual_wield_mastery | 3 | passive | – | – | §6.6 | – | |
| iron_fortress | 2 | buff | cast | charge | DR 0.4 / 6 s | – | ✔ |
| unyielding | 2 | passive proc | – | – | §6.6 | – | ✔ |
| life_regen | 1 | passive | – | – | §6.6 | – | ✔ |
| frenzy | 1 | buff | cast | charge | damageBonus 0.2 / 8 s | – | ✔ |
| bleed_strike | 2 | single | attack | contact | melee | bleed | ✔ |
| rampage | 3 | AoE | cast | charge | circle R2.5 on hero | – | |
| fireball | 1 | single proj | cast | charge + flight 300–600 | range 6 | burn 60 % | ✔ |
| meteor | 3 | AoE ground | cast | charge + 300 fall | R2.5 | burn 60 % | |
| blizzard | 2 | AoE ground | cast | charge | R3 | freeze 2000 | ✔ |
| ice_armor | 1 | buff | cast | charge | DR 0.2 / 10 s | – | ✔ |
| chain_lightning | 2 | AoE | cast | charge | circle R4 on hero, all at once | – | ✔ |
| mana_shield | 1 | buff | cast | charge | manaShield 0.3 / 8 s | – | ✔ |
| fire_wall | 2 | AoE ground | cast | charge | R2, instant (no lingering wall) | burn 40 % | ✔ |
| combustion | 3 | single | cast | charge | ×1.5 vs burning | burn 40 % | |
| ice_arrow | 1 | single proj | cast | charge + flight 250–500 | range 6 | slow 35 % | ✔ |
| freeze | 3 | single | cast | charge | stunDuration 2000 | freeze | |
| teleport | 2 | blink | cast | **instant** | §6.4 #1 | – | ✔ |
| arcane_torrent | 3 | AoE ground | cast | charge | R2.5 | – | |
| backstab | 1 | single | attack | contact | critBonus 20 | – | ✔ |
| poison_blade | 2 | buff | cast | charge | poisonDamage 0.5 / 6 s | – | ✔ |
| vanish | 3 | buff | cast | charge | stealthDamage 1.0 / 3 s | – | |
| multishot | 1 | AoE | cast | charge + per-target ≤260 | circle R3 on hero | – | ✔ |
| arrow_rain | 3 | AoE ground | cast | charge | R3.5 | – | |
| shadow_step | 3 | step | cast | **instant** | §6.4 #2 | – | |
| death_mark | 2 | mark | cast | charge | damageAmplify 0.25 / 8 s + 0.5× hit | – | ✔ |
| piercing_arrow | 2 | AoE line | cast | charge + per-target ≤260 | len 6, half-width R×0.6 | – | ✔ |
| poison_arrow | 2 | single proj | cast | charge + flight 250–500 | range 6 | poison | ✔ |
| explosive_trap | 1 | AoE | cast | charge | circle R2 **on hero** | burn 40 % | ✔ |
| poison_cloud | 2 | AoE ground | cast | charge | R2.5 | poison | ✔ |
| slow_trap | 2 | slow trap | cast | charge | R2 on hero | slow 40 (always) | ✔ |
| chain_trap | 3 | AoE | cast | charge | R3 on hero | stun 1000 | |

("charge" = cast release beat, §10.2; "contact" = attack contact beat, §10.1.) Numeric skill data (cost, cooldown,
range, multipliers, scaling, synergies) belongs to the class/skill spec and is exported with the class tables.

---

## 7. Status effects — `StatusEffectSystem` (`src/systems/StatusEffectSystem.ts`)

### 7.1 Model
`StatusEffect { type: burn|freeze|poison|bleed|slow|stun, value, duration ms, tickInterval ms, startTime, lastTickTime, sourceId }`,
stored per entity id (`'player'` or monster id). Tick intervals: burn/poison/bleed 1000, others 0 (`:51-58`).

### 7.2 Rules
* `apply(target, type, value, duration, source, now)` → effective duration (0 = blocked). Rejects `duration <= 0 || value <= 0` (`:87`).
* **Diminishing returns** for freeze & stun, keyed `(entity, type)` (`:297-336`): if `now < immuneUntil` → blocked;
  if `now − lastApplyTime > 6000` reset count; `count++`; count 1 → full; count 2 → `floor(d×0.5)` and
  `immuneUntil = now + that + 3000`; count > 2 → blocked and `immuneUntil = now + 3000`.
* **poison**: one instance; reapply refreshes start & duration, keeps the higher value, updates source.
* **slow**: one instance; same refresh/keep-higher rule.
* **freeze/stun**: replaces an existing instance of the same type.
* **burn, bleed**: stack as independent instances.
* Every new application logs `sys.statusEffect.applied`; poison refresh logs `…refreshed`; expiry logs `…expired`.
* `tick(now)` (per frame, per tracked entity, `:167-189`): for each ticking effect with `elapsed = now − lastTickTime >= interval`:
  emit `floor(elapsed/interval)` ticks of `value`, then `lastTickTime = now` (remainder dropped → slight drift; a 3000 ms
  burn yields 3 ticks at 60 Hz, sometimes 2). Ticks run **before** expiry each frame.
* `expire(now)`: remove where `now − startTime >= duration`.
* `isImmobilized` = has freeze or stun. `speedMultiplier` = 0 if immobilized, else `max(0.2, 1 − clamp(slow,0,100)/100)`.
* `clearEntity` on death (also clears DR records); `clearAll` on zone shutdown.

Effects in the world (`ZoneScene.ts:1435-1473,2817-2822,6078-6178`):
* Monster immobilized → AI skipped (animation still updates), cannot start or land attacks.
* Monster slow → movement speed × multiplier (attack speed unaffected).
* Hero poisoned → HP regen × 0.5. Hero immobilized → no dodge roll, no teleport.
* **Hero slow has no gameplay effect** (only monsters read the speed multiplier) — QUIRK; recommend FIX (scale hero move speed).
* DoT tick on a monster: `takeDamage(value, isTick)` (no defense/resist), number coloured fire (burn) / poison (poison) / white (bleed); kill → `onMonsterKilled`.
* DoT tick on the hero: direct HP loss, red number with element colour, `COMBAT_DAMAGE (player)`; HP 0 → `killPlayer()`.
* Render: tints while active — burn hue −20 sat 0.6; freeze/stun hue 200 sat 0.8 bright 1.2; poison hue 90 sat 0.5
  (`VFXManager.ts:236-246`). 3D: material tint/overlay params per status.

### 7.3 Monster → hero on-hit statuses (`ZoneScene.ts:6204-6237`), after every landed monster hit
Match on `spriteKey` or monster id substrings (recommend exporting as an explicit per-monster `onHitStatus` list):
* `fire|phoenix|lava` → 30 %: burn `max(1, floor(def.damage×0.2))`/s for 3000.
* `poison|venom|spider` → 25 %: poison `max(1, floor(def.damage×0.15))`/s for 4000.
* `ice|frost` → 20 %: slow 30 for 3000.
Chapter 1 monsters match none of these.

### 7.4 Skill → monster statuses (`ZoneScene.ts:6244-6291`), after each skill hit (single-target and AoE paths only; target alive)
* fire: burn chance 0.6 if id contains `meteor`/`fireball`, else 0.4 → burn `max(1, floor(dmg×0.15))`/s, 3000.
* ice: if `stunDuration` or id contains `freeze` or id == `blizzard` → freeze for `stunDuration ?? 2000`; else 35 % → slow 40 for 3000.
* poison: always poison `max(1, floor(dmg×0.2))`/s, 4000.
* id contains `bleed|lacerate|rend` → bleed `max(1, floor(dmg×0.25))`/s, 5000.
* `stunDuration && physical` → stun for `stunDuration`.
(`dmg` = final damage dealt by that hit.) Recommend exporting these as per-skill `statusOnHit` data instead of id matching.

---

## 8. Dodge

### 8.1 Dodge roll (active) — `performDodge` (`ZoneScene.ts:2377-2430`), `DodgeController` (`CombatInputSystem.ts:63-130`)
* Input: Space, gamepad button 1, touch dodge button (`UI_DODGE_REQUEST {dx,dy}` = joystick tile direction).
* Blocked if hero dead, `now < cooldownEndsAt`, or hero immobilized.
* Direction: requested (touch) else `lastMoveDirection` (updated **only** by keyboard/stick/joystick movement; initial
  `(1,−1)` = screen-right). Normalised; if its length ≤ 0.001 (e.g. touch dodge with the joystick released sends `{0,0}`)
  the raw vector `(1,−1)` is used **un-normalised**, so the roll travels √2× the class distance toward screen-right (QUIRK).
  QUIRK: mouse users dodge in their last *keyboard* direction; recommend FIX: fall back to hero facing.
* Distance by class: rogue 2.6, mage 2.25, warrior 1.8 tiles. Try `d = max; d >= 0.5; d -= 0.25`, first whose **rounded**
  landing tile is in bounds and walkable (no line check: can pass over obstacles). None → no dodge (cooldown not spent).
* `tryStart(now)`: `cooldownEndsAt = now + 900`, `invulnerableUntil = now + 220`, reset avoidance-reward flag.
* Effect: clear path, **instant** reposition (`moveTo`), `playDodge(screen dir)` anim, 2 afterimage ghosts at 1/3 and 2/3
  of the path (alpha 0.28 / 0.21, tinted with the class Spirit colour, additive, 260 ms fade), emit
  `DODGE_STARTED {cooldownMs: 900, invulnerabilityMs: 220}`. attackTarget is kept.
* Iframes are tested when a monster hit **lands** (§5.3): a roll started during a wind-up avoids the hit if contact
  falls inside the 220 ms window. `claimAvoidanceReward` grants one Spirit `'dodge'` per window.
* `cooldownProgress = clamp((now−lastStart)/900, 0, 1)`; `cooldownRemaining = max(0, cooldownEndsAt − now)` (HUD).

### 8.2 Stat dodge (passive evade)
Part of `calculateDamage` step 1: `clamp(DEX_total × 0.3, 0, 30)` % for both heroes and monsters.

---

## 9. Targeting, lock, auto-battle, combat state

### 9.1 Click / touch targeting (`ZoneScene.ts:774-857`)
Pointer down (not during cinematic, not on a touch control): right button → town portal. Dead hero → ignore.
Priority on the clicked tile: loot → NPC within 3 tiles → Ember Tower props → sub-dungeon entrance (≤ 3 tiles) →
labyrinth portal → hidden chest (≤ 2 tiles) → **monster** → exit → ground move.
Monster pick (`findMonsterAt`, `:5652-5659`): first alive monster with `|Δcol| < 1.5 && |Δrow| < 1.5` of the clicked tile.
On pick: `attackTarget = id`, `TARGET_CHANGED {id, name}`, path to the monster's tile (the hero walks onto it; auto-attack
fires as soon as within 1.5 tiles — QUIRK: recommend stopping the path once in attack range).
Ground click clears attackTarget and starts hold-to-move (§ movement spec); while holding, auto-attack is suppressed.
3D: raycast pawns first (capsule), then ground; keep the 1.5-tile tolerance around the ground hit as fallback.

### 9.2 Preferred skill target (`ZoneScene.ts:2335-2344`)
`attackTarget` if alive (else cleared) → nearest alive monster on the map (any state).

### 9.3 Target cycling (`cycleTargetId`, `CombatInputSystem.ts:139-152`; `ZoneScene.ts:2346-2371`)
Q / gamepad button 4 / touch corner button (`UI_TARGET_CYCLE`). Candidates = all monsters; keep alive with
`dist² <= 14²`; sort by `dist²` asc then id (string compare); pick the entry after the current one (wrap); none → null.
Sets `attackTarget` and emits `TARGET_CHANGED`.

### 9.4 Auto-battle (`ZoneScene.ts:3071-3108`)
Toggle: Tab, HUD button, touch toggle; persisted in save settings (`autoCombat`). When on, each frame (hero alive):
1. If there is no attackTarget: if the hero is moving (click path) → **do nothing this frame (no skills either)**; else
   nearest alive monster; if within *that monster's* `aggroRange` → target it (`TARGET_CHANGED`), and if beyond
   attackRange path to it.
2. Walk `autoSkillPriority` (= all class skills in definition order, not configurable): the **first** skill that is
   learned, off cooldown and affordable is `requestSkill`ed, then stop — even if it is only buffered (not executable,
   e.g. out of range). QUIRK: a ready-but-unusable top skill starves lower ones; and passives can be "cast" (§6.6).
   Recommend FIX: pick the first skill that `canExecute`. Potions are **not** used by auto-battle.

### 9.5 Target indicator (`ZoneScene.ts:3110-3140`)
Shown under `attackTarget`, else under the nearest aggro monster: red ring (ellipse 36×12 px ≈ 0.8×0.27 tile, stroke 1.5 px,
`0xff4444` α 0.6) at the target's feet. Emits `TARGET_CHANGED` whenever the shown target changes. 3D: ground decal ring.

### 9.6 Combat state (`ZoneScene.ts:3193-3210`)
`fighting = any alive monster in 'attack' state (whole map) || attackTarget alive`. Rising edge → immediately
`COMBAT_STATE_CHANGED {inCombat:true}`; falling edge → after a **1500 ms** debounce (cancelled if fighting resumes) → `{inCombat:false}`. Drives combat music.

---

## 10. Contact-beat timing (animation-driven hit timing)

All damage from attacks/casts lands on the animation's contact/release beat, never at input time.

### 10.1 Attack contact — `CharacterAnimator.playAttack` (`CharacterAnimator.ts:677-867`)
```
speed     = attackSpeedScale(cfg.attackDuration, attackIntervalMs)      // HitFeedback.ts:72
          = (interval>0 && animMs>0) ? clamp(interval*0.9/animMs, 0.35, 1) : 1
contactMs = frameContactMs * speed, returned as round(contactMs)
frameContactMs = round((frames-1) * cfg.attackContact) * (1000 / attackFrameRate)
```
* Hero sheets: attack = 8 frames; `attackContact = 0.55` → contact frame 4. Monster sheets: attack = 4 frames at
  **12 fps** (hard-coded, `SpriteGenerator.ts:116,871-877`), `attackContact = 1` → frame 3 = 250 ms.
* The hero passes `attackIntervalMs = player.attackSpeed`; monsters pass `def.attackSpeed`.
* Frame playback speed = `1/speed`; phases: wind-up `0.62×contact`, strike `0.38×contact`, recover `max(70, total−contact)`
  where `total = attackDuration × speed`.
* Fallback (no frame anims; legacy) = `computeImpactDelay(windup×speed, 80×speed) = round(windup×speed + 48×speed)`.
  The 3D port always has authored anims → use the frame rule.

| Rig | attackDuration | fps | contact @speed 1 | speed < 1 when interval < |
|---|---|---|---|---|
| warrior | 610 | 13 | **308 ms** | 677.8 ms (gear attackSpeed > 32.2 %) |
| mage | 535 | 15 | **267 ms** | 594.4 ms |
| rogue | 445 | 18 | **222 ms** | 494.4 ms |
| monster humanoid/slime/flying/serpentine/demonic | 500 | 12 (sheet) | **250 ms** | 555.6 ms |
| monster beast | 250 | 12 | 250 ms | 277.8 ms |
| monster large | 450 | 12 | 250 ms | 500 ms |

**3D rule**: author attack montages so the contact AnimNotify sits at exactly these ms at play rate 1; play rate = `1/speed`;
the core schedules the hit at `round(contact × speed)` independently of the renderer (the notify is cosmetic sync only).
Min speed 0.35 means contact can shrink to ~108 ms (warrior).

### 10.2 Cast release — `playCast` (`CharacterAnimator.ts:875-991`)
Frame rigs: `release = round(castDuration × 0.46)`; phases charge 0.46 / release 0.20 / recover 0.34; **not** scaled by
attack speed. Mage rig adds a 120 ms violet glow flash (`0xb9a5ff`) at release (`castGlow`).

| Rig | castDuration | release beat |
|---|---|---|
| warrior | 725 | **334 ms** |
| mage | 500 | **230 ms** |
| rogue | 535 | **246 ms** |
| (legacy fallback) | — | round(castDuration × 0.4) |

### 10.3 Monster wind-up telegraph
During the wind-up (`0.62 × contact` ≈ 155 ms at speed 1) the monster sprite is tinted `0xffc4b0` (normal) / `0xff7a5c`
(elite) (`Monster.ts:348-353`, `CharacterAnimator.ts:814-819`). 3D: emissive/overlay tint on the mesh for the wind-up.

### 10.4 Other action timings (ms) (`CharacterAnimator.ts:94-245`)
| Rig | dodge | hurt | death | hurtKnockback px | attackLunge px |
|---|---|---|---|---|---|
| humanoid (default) | 260 | 200 | 500 | 8 | 14 |
| slime | 260 | 200 | 500 (splat) | 8 | 14 |
| beast | 260 | 200 | 500 | 8 | 16 |
| large | 260 | 200 | 800 | 8 | 10 |
| flying | 260 | 200 | 500 | 8 | 14 |
| serpentine / demonic | 260 | 200 | 500 (dissolve) | 8 | 14 |
| warrior | 300 | 330 | 750 | 8 | 16 |
| mage | 275 | 310 | 670 | 8 | 6 |
| rogue | 240 | 270 | 550 | 8 | 14 |

(`deathStyle` collapse/splat/dissolve only drives the legacy no-sheet fallback; every sheet-based rig uses the thrown
death of §13.2 when a hit direction is known (a monster killed only by DoT with no prior directional hit, and the hero,
use the in-place sink-and-fade).)

Anim-state rules (map to AnimGraph): idle/walk requests are ignored while attack/cast/hurt/dodge play; a hurt during
attack/cast/dodge does **not** interrupt — it plays a *jolt* (pain tint 90 ms + hit-freeze `min(40, 20×strength)`)
(`:1150-1155,1371-1374`). Action facing is locked until the action ends. Blend times (`TRANSITION_MS`, `:306-328`):
idle→walk 90, walk→idle 110, idle→attack 45, walk→attack 55, attack→idle 75, attack→walk 70, idle→cast 65, walk→cast 70,
attack→cast 55, cast→attack 55, cast→idle 85, cast→walk 75, idle→dodge 35, walk→dodge 30, attack→dodge 25,
cast→dodge 25, hurt→dodge 40, dodge→idle 65, dodge→walk 55, hurt→idle 105, hurt→walk 90, default 80.
Facing (render-only): 2 authored views (front/back 3/4) + mirroring with 0.28 hysteresis (`:75-90`) — in 3D the mesh
simply yaws toward the move/target vector; keep a small yaw deadband to avoid jitter.

---

## 11. Hit feedback (HitFeedback + VFXManager)

### 11.0 Classification — `classifyHit` (`src/systems/HitFeedback.ts:48-56`)
`isTick → 'tick'`; `killed → 'kill'`; `isCrit → 'crit'`; else `ratio = maxHp>0 ? damage/maxHp : 0`;
`ratio >= 0.25 → 'heavy'`; `>= 0.06 → 'normal'`; else `'light'`.

### 11.0b `HIT_PROFILES` (`HitFeedback.ts:29-36`) — export verbatim
| weight | targetStopMs | attackerStopMs | flashMs | recoil | shakeMs | shakeIntensity | sparks | ringRadius px |
|---|---|---|---|---|---|---|---|---|
| tick | 0 | 0 | 40 | 0 | 0 | 0 | 0 | 0 |
| light | 40 | 30 | 55 | 0.6 | 0 | 0 | 4 | 10 |
| normal | 60 | 45 | 70 | 1.0 | 60 | 0.0018 | 6 | 13 |
| heavy | 85 | 65 | 85 | 1.35 | 90 | 0.0032 | 9 | 17 |
| crit | 110 | 90 | 100 | 1.6 | 120 | 0.0045 | 12 | 22 |
| kill | 130 | 100 | 110 | 2.0 | 140 | 0.005 | 14 | 24 |

### 11.1 Where each knob is applied
| Situation | Target side | Attacker side | Camera |
|---|---|---|---|
| Hero basic attack hits monster | `Monster.takeDamage`: white flash `flashMs`; if killed → death (no recoil/freeze); else hurt recoil `recoil` + hit-stop `targetStopMs` (`Monster.ts:291-322`) | hero hit-stop `attackerStopMs`; impact burst (class colour); **kill of an elite → slow-mo 200 ms @0.4** (`ZoneScene.ts:2931-2941`) | profile shake via impact burst |
| Hero skill hits monster | same `takeDamage` | **no** attacker hit-stop; impact burst (skill colour) except death_mark/slow_trap | profile shake; AoE extra shake |
| DoT tick on monster | `isTick` → flash 40 ms only | – | – |
| Mercenary hits monster | `takeDamage` (single: isCrit passed; AoE: not) | – | – |
| Monster hits hero | `hero.playHurt(recoil of classifyHit(dmg, hero.maxHp, isCrit))`; pain tint `0xff6b6b` 100 ms (no white flash) | monster hit-stop `round(attackerStopMs×0.6)` | player-hit shake (§11.5) |

Colours: class basic-impact `warrior 0xffd98a, mage 0xc7a6ff, rogue 0x9dffc8`, fallback `0xfff2c0` (`ZoneScene.ts:89-93`).
Skill impact (`skillImpactColor`, `:98-105`): id has `fire` / meteor / combustion → `0xff6600`; `ice` / blizzard / freeze →
`0x4488ff`; `lightning` → `0x5dade2`; `poison` → `0x7ed957`; arcane_torrent → `0xb07cff`; else physical `0xf1c40f`, other `0xf39c12`.

### 11.2 Hit-stop semantics (render, but timing is gameplay-visible)
`triggerHitFreeze(ms)`: `freezeTimer = max(freezeTimer, ms)`; pauses that character's frame animation and its own
motion tweens; the animator's update decrements the timer by frame delta and resumes at 0 (`CharacterAnimator.ts:472-487,539-547`).
It does **not** stop gameplay: movement, AI, timers and other actors continue. **3D**: per-actor animation pause
(e.g. `USkeletalMeshComponent::GlobalAnimRateScale = 0` or montage play-rate 0) for the duration; do not use global
time dilation; the core sim is unaffected.

### 11.3 Flash, recoil
* **White flash** (`flashWhite`, `:490-499`): solid white silhouette (`setTintFill`) for `flashMs`, restarting on re-hit.
  3D: material scalar `HitFlash` → 1 (emissive white override) for `flashMs`.
* **Pain tint** (`hurtFlash`): multiply tint `0xff6b6b` 100 ms (90 ms in jolt), skipped while a white flash is showing.
* **Recoil** (`playFrameHurt`, `:1147-1209`), `n` = unit vector source→victim, `s = strength`:
  snap offset `recoil = min(14, hurtKnockback×0.65×s)` px along `n` (y component ×0.45, +2 px down);
  squash `q = min(0.2, 0.1×s)`: scaleX ×(1+q), scaleY ×(1−0.8q); tilt `±5°×min(1.6, s)` (sign of n.x).
  Then ease to 70 % offset / scale (0.94, 1.06) over `hurtDuration×0.3` (expo-out), then back to rest over
  `hurtDuration×0.62` (cubic-out). Hurt is skipped when `s <= 0`. 3D: additive hit-react (or procedural spring) along
  the hit direction with peak displacement `recoil/45` tiles, same timings.

### 11.4 Impact burst (`VFXManager.impactBurst`, `:265-304`) — design target for a Niagara system `NS_ImpactBurst`
Inputs: position = target chest (feet −18 px ≈ 0.4 tile up), `angle` attacker→target, weight, colour. Skipped for tick.
`r = ringRadius`, `big = crit||kill`:
* hot white core pushed +3 px along the blow: 100 ms (140 big), scale r/32×0.5 → r/32×1.3, alpha 1→0;
* coloured soft glow: 160 ms (220 big), r/64×1.2 → r/64×2.6, alpha 0.45→0;
* ground shock ring under the feet: radius `0.3r → 1.25r` (1.7r big), 240 ms (320 big);
* `sparks` streaks sprayed away from the attacker within ±0.45 rad (±0.625 big), speed 160–280 px/s (220–420 big),
  life 160–280 ms, drag 4, gravity 120 px/s², alternating white / colour;
* big: four-point cross glint 200 ms;
* then `cameraShake(shakeMs, shakeIntensity)` if `shakeMs > 0`.
Sizes in tiles = px/45.

### 11.5 Camera shake
* Hit shakes come from the profile (via impact burst). Player-hit shake (`VFXManager.ts:38-62`): crit → (150 ms, 0.008);
  else `ratio = dmg/maxHp`, intensity `clamp(ratio×0.01, 0.002, 0.006)`, duration `clamp(50 + ratio×100, 50, 120)` ms.
* Skill-authored shakes (`FxEngine.shake`, own 120 ms throttle): charge 150/0.008, lethal_strike 160/0.008,
  war_stomp 240/0.011, rampage 280/0.012, fireball explosion 90/0.003, meteor 320/0.015, combustion 140/0.006,
  explosive_trap 200/0.009, chain_trap 120/0.004. Quest-hunt reveal 260/0.004.
* Throttles: VFXManager ignores a shake < 100 ms after its last accepted one; Phaser ignores a new shake while one is
  running (no override). **Shape**: every frame a fresh uniform random offset in `±I×W` (x) / `±I×H` (y), **no decay**,
  for the duration. Effective amplitude at render scale 1 = `I × 1280 × 1.8²` logical px ≈ `I × 3.24 × screen width`
  (normal hit ≈ ±0.6 % width; meteor ≈ ±4.9 %). (Web amplitude also grows with RENDER_SCALE² — artefact, ignore.)
* 3D: a camera-space translational shake, offset re-randomised each frame, amplitude `I × 3.24 × viewportWidth` in
  screen px converted to world units at the focus distance; same throttle/no-override rules; expose a global feel scale (default 1).

### 11.6 Slow motion (`VFXManager.slowMotion`, `:310-320`)
Only for killing an **elite** with a hero basic attack: 200 ms (scene clock, real time) at 0.4 scale applied to
tweens, sprite animations and FX particles; projectile flight tasks and all gameplay timers are unscaled.
3D: `SetGlobalTimeDilation(0.4)` for 200 ms *real* time is acceptable **only if** the core sim is ticked with undilated
real delta (gameplay must not slow); restore on zone exit.

### 11.7 Low-HP danger vignette (`VFXManager.ts:377-401`)
Active while `0 < hp/maxHp < 0.3`: `severity = 1 − ratio/0.3`; strength `0.2 + 0.25×severity + 0.05×sin(now×0.005)`;
radius `0.7 + 0.15×severity`. 3D: post-process vignette material (red-tinted per the design intent).

### 11.8 Other combat feedback
* Player death: camera fade to rgb(80,10,10) over 220 ms (`VFXManager.ts:75-77`), white camera flash 80 ms α0.6, death burst `0xcc2222`.
* Level-up (not combat but fires mid-fight): gold flash 200 ms α0.5, shake 100/0.004, zoom pulse to 1.5/1.8 of base over 200 ms and back in 120 ms, golden pillar burst.
* Legendary/set drop: flash 220 ms α0.35 in quality colour + shake 160/0.005.

---

## 12. Floating combat text (`ZoneScene.showDamageText`, `:5698-5783`) — render-only spec

| Kind | Text | Colour | Size px | Weight |
|---|---|---|---|---|
| Miss | `MISS` | `#7f8c8d` | 14 | normal |
| Hero damaged | `-N` | crit `#ff4444` else `#e74c3c` | crit 24 / 20 | bold if crit |
| Monster damaged | `N` | crit `#ffd700`; else element: fire `#ff6600`, ice `#66ccff`, lightning `#a8e6ff`, poison `#66ff66`, arcane `#cc66ff`; else `#ffffff` | crit 26 / 20 | bold if crit |
Font Cinzel (serif), black outline 4 px (crit) / 3 px.

Placement & stacking: anchor = entity feet; stack key = `(round(x/28), round(y/28))`; a new number on the same key within
320 ms gets `index = min(prev+1, 4)`, else 0. Start = `(x + randInt(−6,6), y − 30 − index×11)`; sideways drift
`(index even ? +1 : −1) × (crit ? 14 : 10) + randInt(−4,4)` px. Later numbers draw on top.
Motion: Miss: scale 0.8→1 in 90 ms, then after 120 ms rise 22 px and fade over 650 ms.
Others: pop from scale 0.35 (crit, rotated −8°) / 0.5 → peak 1.5 / 1.2 in 90 / 70 ms (quad-out, rotation→0) → settle
1.1 / 1.0 in 160 / 110 ms (back-out); life `L = 1050` (crit) / `780`; x eases to `start+drift` over L (sine-out);
y rises `40`/`28` px over 0.45L (quad-out), then sinks 8 px while fading to 0 over 0.55L (quad-in). Pooled objects.
Kill rewards: `+N EXP` (`#b39ddb`, 13 px, at y−40, rise 35 px, 1500 ms) and `+NG` (`#ffd700`, 13 px, at (x+15, y−28), rise 30, 1200 ms).
3D: screen-space widgets anchored to projected world points (head height), same timings; offsets in px are screen px.

---

## 13. Death

### 13.1 Monster kill — `onMonsterKilled` (`ZoneScene.ts:3789-3929`), in order
1. Labyrinth hook (later milestone). 2. Clear its status effects. 3. Hero Spirit `'kill'`.
4. `exp = floor(def.expReward × (1 + homeExpBonus/100 + eq.expBonus/100))`, `gold = randomInt(goldMin, goldMax)`; addExp, add gold.
5. Pet exp hook; mercenary exp share. 6. `killHealPercent` heal (+heal burst). 7. VFX: death burst (`0xff4444`) + 6-coin gold burst; EXP/gold texts.
8. Kill counters, achievements, quest `kill` progress, story director, Ember Tower embers (later), difficulty completion check (§14).
9. Ley-fruit drop roll, quest drops, loot roll (`LootSystem.generateLoot(def, luck, affixLootBonus, difficulty)` — loot spec;
   `luck = hero.stats.lck + homestead MF (+ dungeon MF)`, `affixLootBonus = Σ affix lootQualityBonus (+ dungeon bonus)`),
   potions drop as auto-pickups (hp S/M/L 50/150/400, mp S/M 30/80; collected within 2 tiles).
10. Log `zone.monsterKill`. 11. Respawn in **15 000 ms** unless mini-boss, quest-spawned or in a labyrinth.
Respawn (`:5598-5634`): up to 8 tries at `spawn ± randomInt(−2,2)` (walkable, outside safe zones), else the spawn tile;
new monster from the (difficulty-scaled) original definition; elites re-roll affixes.
`MONSTER_DIED` is **not** emitted (QUIRK → monster-death SFX never plays). Recommend FIX: emit from this flow.

### 13.2 Monster death animation (`CharacterAnimator.ts:1217-1365`)
Killing blow → thrown death away from the last hit source: `heavy = deathDuration >= 750`; throw 6 px (heavy) / 16 px,
air `max(160, 0.35×dur)`, spin 8°/22°, hop 3/9 px up over 0.4 air then land with squash over 0.6 air, then after 120 ms
fade out over `max(200, 0.6×dur)`; then the actor is destroyed (humanoid ≈ 600 ms total). Ground decals: none.
3D: physics-lite knockback impulse (≈0.35 tile) + death anim, then dissolve/fade; corpse removed at the same time.

### 13.3 Hero death — `Player.die` + `handlePlayerDied` (`Player.ts:410-429`, `ZoneScene.ts:896-955`)
1. Spirit reset (emit spirit events), death anim in place (frame rig: after 0.25×dur sink 5 px & fade to α0.12 over 0.75×dur),
   `PLAYER_DIED {}` → SFX + camera fade; log `sys.player.death`.
2. Handler: clear hero statuses, clear `isPortaling` (`:899` — this does **not** cancel a portal channel already
   running: its 1500 ms timer still moves the corpse to the camp and sets α 1, `:5868-5884`; save-ui-input Q23),
   **apply death penalty** (§15), camera flash + death burst, big centred text `zone.death.text` (fade in 250 ms).
3. At +1100 ms (`time.delayedCall`, `:914`, unconditional — it fires even if the hero was healed meanwhile): text
   fades (250 ms). In labyrinth / sub-dungeon → `scene.restart` of the parent zone at its `camps[0]` (fallback
   `playerStart`, then (3,3)) with `hp = maxHp, mana = maxMana` (labyrinth run ends `'fallen'`); that zone's `create`
   autosaves. Otherwise `respawnAtCamp(camp[0])` (`Player.ts:431-452`): HP/MP full, position reset, path/target
   cleared, alpha/angle and anim reset, `PLAYER_HEALTH_CHANGED`, log `sys.player.respawn`, camera fade-in 300 ms —
   **no autosave** (save-ui-input Q36).

**Dead window — exact web gating.** There is no explicit dead state; call sites test `player.hp <= 0`.
*Gated*: movement, world click and hold-move (`ZoneScene.ts:2114, 782, 869`), skill execution (`:2249` — the request
is still **buffered** for 180 ms), dodge (`:2379`), town portal (`:5802`), auto-combat (`:3072`), the hero combat loop
incl. monster swings and buff expiry (`:2809`; strikes already in flight are dropped at contact, `:2945, 2958`),
hazard / DoT damage to the hero (`:1204, 6111`), HP and MP regen (`Player.ts:219-223`, `ZoneScene.ts:1376`), soul-echo
claim (`:1269`). *Not gated*: `Esc` → return to menu with an autosave of `hp = 0` (`:2203-2205, 4223-4228,
4264-4266`), all panel hotkeys and panel actions (UIScene has no `PLAYER_DIED` listener), bag potion use
(`UIScene.ts:4297-4301`), potion auto-pickup (`:1496-1518`), auto-loot, healer-mercenary heal (`:6498-6503`), the
level-up HP/MP refill (`Player.ts:156-166`). Consequences: a save made in the window loads as a 0-HP hero that can never
act or respawn (`:4313`, soft-lock); a heal in the window revives the hero in its death pose, it reclaims the fresh
soul echo at its feet (penalty refunded), and a second death in the window applies a second penalty and fades the
first echo.

**Port (FIX, normative — full tables in `save-ui-input.md` §3.4, §3.5, §5.1.1; quirks Q34–Q36):**
* Core state `HeroLife { Alive, Dying }`; `KillHero()` (after the `deathSave` check of §5.3 — monster hits only — and
  the pet revive) enters `Dying` once — a repeat call is a no-op; the 1100 ms respawn timer runs on the session clock.
* While `Dying` the core rejects every gameplay input and every state-changing UI action (movement, skills — buffer
  cleared, no buffering — dodge, targeting, portal — an active channel is cancelled — interaction, pickups,
  consumables, equip, trade, quests, dialogue, point spending) and every HP/MP gain other than the respawn; exp still
  accrues but a level-up does not refill. Still allowed: opening/closing information panels (read-only), the `Tab`
  auto-combat preference, `Esc` → system menu. UE closes modal NPC panels on `HeroDied` and re-enables panel actions
  on `HeroRespawned`.
* Never save a `Dying` hero: triggers are deferred until the respawn; Return to menu / Quit / app background call
  `ResolvePendingDeath()` (respawn now, same effects as step 3, no second penalty) and then save. Autosave after
  every respawn.
* Loading a save with `hp ≤ 0` (or non-finite) = completed respawn: zone `camps[0]` (fallback `playerStart`), full
  HP/MP after equipment, echo kept as saved, no penalty.

Later-milestone hook: `killPlayer()` first asks the pet companion to revive (`ZoneScene.ts:7171-7175`).

---

## 14. Difficulty modes — `DifficultySystem` (`src/systems/DifficultySystem.ts`)
| Difficulty | hp | damage | defense | exp (also gold) |
|---|---|---|---|---|
| normal | 1.0 | 1.0 | 1.0 | 1.0 |
| nightmare | 1.5 | 1.5 | 1.3 | 2.0 |
| hell | 2.0 | 2.0 | 1.6 | 3.0 |
`scaleMonster(def, diff)` (`:198-212`): normal → unchanged; else `Math.round` each of hp, damage, defense, expReward and
both goldReward bounds (gold uses the **exp** multiplier). Applied at spawn to zone monsters, mini-bosses, quest hunts and
their minions (`ZoneScene.ts:4436,4596,4620`); labyrinth uses its own scaler (later milestone).
Unlocks: killing `demon_lord` in `abyss_rift` (not in a labyrinth) marks the current difficulty completed (autosave);
next unlocked = nightmare if normal done & nightmare not, hell if nightmare done. States: normal always available;
completed list → `completed/available/locked` (`:154-177`). Migration: empty completed list + saved `nightmare` → `['normal']`;
`hell` → `['normal','nightmare']`. Selector shown if difficulty ≠ normal or any completed. Difficulty also feeds loot rolls
and soul-echo shares. Chapter 1 plays on normal (but must load any difficulty).

---

## 15. Soul echo (death penalty) — `src/systems/SoulEcho.ts`
* `computeDeathPenalty({level, gold, exp, expToNext, difficulty})` (`:30-35`): level < 5 → `{0,0}`; else
  `gold = floor(max(0,gold) × {0.10, 0.15, 0.20})`, `exp = min(max(0,exp), floor(expToNext × {0, 0.05, 0.05}))`
  (normal/nightmare/hell). Exp is only taken from current-level progress (never de-levels).
* On death (`ZoneScene.ts:1218-1245`): subtract both from the hero (emit exp change if exp > 0).
  In a labyrinth/sub-dungeon: the loss is permanent, log `zone.soulEcho.lostInDungeon`, no echo.
  Else `leave({mapId, col: round(col), row: round(row), gold, exp})`: the previous echo (if any) is returned as lost →
  log `zone.soulEcho.faded {gold}`; the new echo is stored only if `gold > 0 || exp > 0` (a free death still destroys the
  old echo). Log `zone.soulEcho.left {gold}` and spawn the visual.
* At most one echo in the world; persisted in save (`SaveData.soulEcho = {mapId, col, row, gold, exp} | null`); `load`
  rejects non-finite col/row.
* Claim: each frame while the echo's visual exists in this zone and the hero is alive, `tryClaim(mapId, col, row)` succeeds
  if same map and `hypot(Δcol,Δrow) <= 1.5` → gold += echo.gold, `addExp(echo.exp)`, log `zone.soulEcho.claimed`, SFX
  `resonance`, burst `0x7fd8ff`, visual rises 30 px & fades 500 ms, autosave.
* Visual (render): pale cyan ghost of the hero (tint `0x9fe6ff`, α 0.55↔0.35 bob 6 px, 1400 ms yoyo) over an additive
  ground glow `0x7fd8ff`, label `zone.soulEcho.label {gold}` (`#bfefff`). 3D: translucent ghost mesh of the hero class
  (kneeling pose) + ground glow decal + world label.

---

## 16. Spirit (class combat resource) — `src/systems/SpiritSystem.ts`
| class | profile | colour | hit | kill | dodge | +crit | resonance ms | dmg bonus | mana cost × | move bonus |
|---|---|---|---|---|---|---|---|---|---|---|
| warrior | emberheart | `0xffb45c` | 8 | 15 | 12 | 4 | 6000 | 0.30 | 0.85 | 0.12 |
| mage | astral_focus | `0xa98bff` | 7 | 12 | 16 | 5 | 7000 | 0.20 | 0.60 | 0.08 |
| rogue | shadow_rhythm | `0x66e58a` | 6 | 13 | 20 | 7 | 5500 | 0.25 | 0.75 | 0.18 |
Max 100. `gainFromCombat(src, spi, crit) = gain((base + (crit ? critBonus : 0)) × (1 + clamp(spi,0,200)×0.015))` using
the hero's **base** SPI. No gain while resonating. Reaching max starts Resonance (`resonanceRemaining = duration`, emits
`SPIRIT_RESONANCE_STARTED`, cast-glow ring in the class colour); during it the meter drains linearly to 0 over the duration;
multipliers: outgoing damage `×(1+dmgBonus)` (calc step 13), mana costs `×manaMult` (then `ceil`), move speed `×(1+moveBonus)`.
Sources: `'hit'` whenever a hero hit deals damage (basic or skill, via `applySteal`), `'kill'` per kill, `'dodge'` per stat
dodge and once per dodge-roll iframe window. Reset on death; saved as `{value, resonanceRemainingMs}`.

---

## 17. Elite affixes — `src/systems/EliteAffixSystem.ts`

### 17.1 Definitions (`:60-180`) — export verbatim
| type | zh | dmg× | spd× | hp× | def× | extraFire | special | colour | lootQ+ |
|---|---|---|---|---|---|---|---|---|---|
| fire_enhanced | 炎魔 | 1.0 | 1.0 | 1.2 | 1.0 | 0.3 | – | `0xff4400` | 5 |
| swift | 迅捷 | 1.0 | 1.6 | 1.0 | 1.0 | 0 | – | `0x44ff88` | 3 |
| teleporting | 瞬移 | 1.1 | 1.0 | 1.15 | 1.0 | 0 | teleportCooldownMs 5000 | `0xaa44ff` | 5 |
| extra_strong | 狂暴 | 1.35 | 1.0 | 1.3 | 1.15 | 0 | – | `0xff2222` | 8 |
| curse_aura | 诅咒 | 1.1 | 1.0 | 1.2 | 1.0 | 0 | radius 4 tiles, reduction 0.15 | `0x8844aa` | 6 |
| vampiric | 吸血 | 1.1 | 1.0 | 1.25 | 1.0 | 0 | lifesteal 0.2 | `0xcc0000` | 5 |
| frozen | 冰封 | 1.05 | 0.9 | 1.2 | 1.1 | 0 | freezeChance 0.25 | `0x4488ff` | 5 |

### 17.2 Rolling
Only for monsters spawned as elite (`def.elite`), mini-bosses (always), quest-hunt leaders (always). Count per zone
`ZONE_AFFIX_COUNTS` (`:205-211`): emerald_plains [1,1], twilight_forest [1,1], anvil_mountains [1,1],
scorching_desert [1,2], abyss_rift [2,3], unknown → [1,1]; `count = randomInt(min,max)`. Selection without replacement:
`idx = randomInt(0, len−1)` over the remaining list (initial order = table order above), splice. Sub-dungeon mini-bosses roll
with their parent zone. Respawned elites re-roll.

### 17.3 Application (`Monster.applyEliteAffixes`, `Monster.ts:389-416`)
Combined (`getCombinedStats`, `:264-299`): multipliers multiply; `extraFireDamage`, `lootQualityBonus`, `lifestealFraction`
add; `freezeChance = min(0.5, Σ)`. Then `maxHp = hp = floor(maxHp×hp×)`, `damage = floor(damage×dmg×)`,
`speed = floor(speed×spd×)`, `defense = floor(defense×def×)`, `stats.str = floor(damage×0.8)` (dex not recomputed).
Display name `[名1·名2] 基础名` (`buildAffixName`, `·` separator, localized affix names), label orange `#ff6600`, always visible.

### 17.4 Behaviours
* **On hit** (after a landed monster hit, `ZoneScene.ts:3023-3053`): extra fire `floor(finalDmg × extraFire)` taken
  directly from hero HP (no resist; separate fire-coloured number at +10,−5 px and its own `COMBAT_DAMAGE`); vampiric heals
  the monster `floor(finalDmg × lifesteal)` (HP bar not refreshed — QUIRK, FIX: refresh); frozen: `rand < freezeChance` →
  hero `slow 30` for 2500 ms + log `zone.combat.freezeSlow` (cosmetic today, see §7.2).
* **Teleporting** (`:5995-6036`), each frame for aggro monsters: when `now − last >= 5000` (first check immediate): set
  `last = now`; if `4 < dist² < 225` → target `hero ± (1 + rand)` per axis with random sign, clamp `[1,size−2]`, round;
  if walkable → instant blink (no safe-zone check), purple puff r 12 px → ×2.5 fade 400 ms at both ends.
* **Curse aura** (`:6038-6066`): hero within 4 tiles → ensure one hero buff `{damageAmplify 0.15, 2000 ms, tag 'curseAura'}`
  (refresh `startTime` if present) = hero takes +15 % damage; log `zone.combat.curseAura` at most every 2000 ms per monster.
* `swift`, `extra_strong` etc. are pure stat changes. `lootQualityBonus` feeds the loot roll.
* Visuals (render, `Monster.ts:421-549`): per affix a pulsing ground aura ellipse in the affix colour (α 0.25, 1200 ms
  yoyo ×1.2) plus: fire → rising ember glow; swift → speed streak behind; teleporting → arcane shimmer; curse → wide purple
  mist (α 0.08); vampiric → blood glow drip; frozen → ice crystal pulse; extra_strong → red power glow. Elite crown icon above
  every `def.elite` monster. 3D: Niagara aura per affix on a ground ring + head-mounted icon.

---

## 18. VFX catalogue for 3D design (render-only)

Palette (`FxKit.ts:7-20`, `{core, mid, rim, dark}`): fire `fff3c4/ffa532/ff5418/8a1c08`; frost `ffffff/a6f0ff/46b4f0/1e5aa8`;
lightning `ffffff/cfe0ff/8c8cff/4a3aa8`; poison `f0ffc0/9aec40/3fae2e/1c5a1e`; shadow `f0dcff/b478ff/7030c0/24103e`;
holy `fffbe4/ffe27a/f4ac28/8a5a10`; steel `ffffff/eef3fb/aabbd4/56647c`; blood `ffb0a0/e8342c/a01420/4a0810`;
rage `ffe0b0/ff5a2a/d01e1e/5a0a0a`; arcane `ffffff/d8b4ff/8a5cff/34207a`; nature `f6ffe0/a8f07a/40c060/1a5a2a`;
earth `fff0c8/e8c47a/b08850/5a4430`. Style: cel-shaded, additive glows over normal-blend smoke/debris, ground rings/decals
flattened on the floor. Sizes below in tiles (px/45); `R` = skill AoE radius. Chest height ≈ 0.4 tile.

Primitives (`FxKit.ts`): **flash** (white core + coloured bloom, impact frame), **glow** (soft additive pop), **glint**
(4-point star), **sparks** (streaks), **motes** (drifting embers/flakes/plus signs), **flames** (tongues from the ground),
**smoke** (cartoon puffs), **debris** (rocks in arcs), **ring** (expanding ground ring), **shock** (filled ground disc),
**decal** (scorch/frost/puddle/rune/crack, lingering 0.4–2.2 s), **slash/swipe** (crescent arcs), **bolt** (jagged
lightning), **beam** (vertical light column), **gather** (motes spiralling in — anticipation).

Chapter 1 skill effects (`SkillEffectSystem.ts:214-1042`):
* **slash** — gold-edged steel forehand arc through the target (radius clamp(0.85×dist, 0.67–1.3 tiles), 70 ms sweep, 230 ms life) + inner steel arc, holy flash + 8 sparks + glint at target.
* **whirlwind** — two blades chasing round the hero 1.25 turns in 300 ms at 0.62R, 10 white streaks, steel ring 0.4R→1.3R (420 ms), shock disc, dust puffs at the edge.
* **shield_wall** — holy gather, spinning rune decal (900 ms), 8 golden hex plates snap into a ring (r≈0.8) ~780 ms, flash, light beam, motes, ring + shock.
* **taunt_roar** — rage glow at head, 2 waves × 8 crescent shouts flying outward, two rage rings to R, shock disc, sparks.
* **charge** — 9 speed streaks toward the target, dust trail, at +70 ms: shake, holy flash, 12 sparks, earth shock, glint, debris at target.
* **iron_fortress** — gather, rune, two tiers of 6 steel plates forming a dome (800 ms), flash/glint/ring/sparks at +170 ms.
* **unyielding** (proc) — ground crack, earth shock, 8 stone chips spiralling up, holy beam + glow + glint.
* **life_regen** — green glow decal, ring, rising plus-sign motes + embers.
* **frenzy** — two heartbeat rage pulses (glow+ring, 150 ms apart), ring of 10 rage flames, rising sparks/embers.
* **bleed_strike** — backhand blood-red arc, blood flash + spray, 5 drips, blood puddle decal (1.3 s).
* **fireball** — muzzle flash; flaming comet (glow+comet+flame head+core) flying the travel time with flame/ember/smoke trail every 6 px; on arrival: shake 90/0.003, flash, glow, shock disc r≈1, 9 flame petals, sparks, embers, smoke, scorch decal (1.4 s).
* **blizzard** — frost decal R×0.85 (1.3 s), ring, 26 ice shards falling in 3 waves over ~700 ms (each lands with a spark, small ring, shard stuck in the ground), snowflakes, mist.
* **ice_armor** — frost decal, 9 crystal spikes erupt in a ring then shatter into flakes (~420 ms), flash, ring, glint, sparkles.
* **chain_lightning** — flash at hand; for each target in order a jagged bolt from the previous point (55 ms stagger), each hit: flash, sparks, ring, glint. No targets: a bolt strikes the ground ahead.
* **mana_shield** — arcane gather, rune, a bubble pops in with overshoot and fades (760 ms), 8 orbiting sparks.
* **fire_wall** — scorch decal R×0.92, shock, 14 flame pillars (3 tongues each, 520–760 ms) appearing around a ring of radius 0.8R in an 18 ms-per-pillar sweep, glows, embers, smoke.
* **ice_arrow** — frost shard projectile with flake trail; arrival: flash, 7 shard splinters back-sprayed, flakes, ring, frost decal.
* **teleport** — departure implosion (gather, collapsing ring, rune), 6 rift streaks along the path, arrival at +100 ms: rune flare, flash, light beam, ring, sparks, motes.
* **backstab** — two crossing shadow dagger arcs at the target (60 ms apart), flash, glint, sparks, blood spray, puff.
* **multishot** — up to 7 arrows fanned toward targets (or a 1.6 rad fan along aim), 220 ms flight to 1.1R, staggered 12 ms.
* **explosive_trap** — arm blink + rune (80 ms), then shake 200/0.009, flash, glow 1.6R, shock to R, ring, flames, debris, sparks, smoke, scorch decal.
* **poison_blade** — poison gather, ring of green flames at the feet, ring, glow, venom drips, bubbles, puddle decal.
* **death_mark** — shadow rune under the target (1 s), skull sigil stamps over its head and hangs ~1 s, shrinking ring, glint, 6 orbiting embers.
* **piercing_arrow** — glint on the bow, glowing arrow flying 240 px (≈5.3 tiles) in 260 ms with streak trail, 3 shock rings along the path, arrow continues and fades.
* **poison_arrow** — arrow with dripping venom trail; arrival: flash, 8 venom droplets, green smoke, puddle decal, bubbles.
* **poison_cloud** — puddle + glow decals, two layers of billowing toxic smoke (0.9–1.3 s), bubbles, shock.
* **slow_trap** — rune blink, at +100 ms frost flash, frost & rune decals, 3 staggered frost rings, flakes, mist.
Basic attack hit VFX for monsters → hero (`playMonsterAttack`): dark-red + hot-orange claw rake over the hero, sparks, red glow.
Monster ranged projectile: glowing comet in fire/frost/shadow palette with trail; arrival flash, sparks, ring (+ smoke for fire).
Tier-3 effects (meteor, war_stomp, rampage, lethal_strike, combustion, freeze, arcane_torrent, vanish, arrow_rain,
shadow_step, chain_trap, vengeful_wrath, dual_wield_mastery) are described at the cited lines for later milestones.

Generic VFX (`VFXManager.ts`): **deathBurst** (flash, 7 dark smoke puffs, 8 embers in colour, ground ring, rising soul wisp 700 ms);
**goldBurst** (6 spinning coins hop and fall, holy motes); **healBurst** (green glow, ring, rising plus signs + embers);
**hitSparks** (glint + 12 pale-gold sparks); **levelUpBurst** (rune, golden pillar, flash, shock, ring, rising stars);
loot beams per quality (loot spec).

---

## 19. Event contract (EventBus → core events / UE delegates)

| Event | Payload | Emitted by | Consumers |
|---|---|---|---|
| `combat:damage` | `{targetId, damage, isDodged, isCrit, isPlayerTarget?, targetMaxHP?}` | hero basic hits; every monster hit/iframe-avoid on hero; hero DoT ticks; elite extra fire; labyrinth burst | VFX (player-hit shake), Audio (`miss`/`crit`/`hit`) |
| `monster:died` | `{id, name}` | **nobody** (dead `applyDamage`) | Audio `monster_death` |
| `skill:used` | `{skillId, damageType}` | `Player.useSkill` | Audio by damageType (fire/ice/lightning; arcane+poison → buff; else melee) |
| `combat:target_changed` | `{targetId: string or null, targetName: string or null}` | click, cycle, auto-battle, kill, indicator changes | HUD target frame |
| `combat:skill_buffered` | `{skillId, expiresAt}` | skill request not executable | HUD |
| `combat:dodge_started` | `{cooldownMs, invulnerabilityMs}` | dodge roll | HUD sweep, Audio `dodge` |
| `combat:state_changed` | `{inCombat}` | §9.6 | music |
| `player:died` | `{}` | `Player.die` | ZoneScene death flow, VFX fade, Audio |
| `player:spirit`, `spirit:resonance_started/ended` | `{value,maxValue,resonating,gained?,source?}` / `{profileId,durationMs}` / `{profileId}` | Spirit | HUD, Audio |
| `player:health`, `player:mana` | `{hp,maxHp}` / `{mana,maxMana}` | change-only each frame + heals | HUD |
| `ui:skillClick`, `ui:dodgeRequest`, `ui:targetCycle` (UI_*) | `{index,skillId}` / `{dx?,dy?}` / `{}` | HUD & touch controls | ZoneScene |
| `log:message` | `{text, type}` | many (keys listed in §21) | combat log |
Recommend FIX in port: emit `MonsterDied` from the kill flow and `CombatDamage` for skill and DoT hits too (with a
`source` field), so audio/feedback are uniform.

---

## 20. Render-only items → 3D equivalents

| Web (Phaser) | 3D / UE equivalent |
|---|---|
| Sprite-sheet frame anims + tweened offsets (lunge, squash) | Skeletal montages authored with the same contact/release ms; procedural squash optional |
| `setTintFill` white flash, `setTint` telegraph/pain tints, preFX colour-matrix status tints | Dynamic material params: `HitFlash`, `TelegraphTint`, `PainTint`, `StatusTint` |
| Hit-stop (pause sprite anim + tweens) | Per-actor anim pause; never global time |
| Camera `shake(ms, intensity)` | Custom camera shake (uniform jitter, no decay, throttled) |
| `tweens.timeScale`/`anims.globalTimeScale` slow-mo | Global time dilation with undilated core tick, or per-actor dilation of visuals |
| FxEngine pooled sprite particles, decals, rings | Niagara systems (pooled), deferred decals for ground marks |
| Floating `Text` pool | Pooled UMG/Slate widgets projected from world |
| Iso target ellipse | Ground decal ring |
| Camera vignette postFX | Post-process material |
| Afterimage ghosts (sprite copies) | Pose-snapshot ghost meshes (additive translucent material) |
| Depth sorting by y | Real depth |
| Screen-px distances (projectile flight, AoE "4 px" checks) | Tile distances ×36 px equivalent (flight) / ≈0.1 tile |

---

## 21. Known web quirks (summary)
Keep-by-default unless resolved otherwise (see Open Questions):
1. Mana shield never deducts mana (§2 step 17).
2. Monster buffs never expire (death_mark permanent, stacks).
3. Skills/merc/extra hits ignore `isDodged` → "0" number with a light hit reaction.
4. Hero slow from monsters/affixes has no effect.
5. Passive skills castable; auto-battle can starve on an unusable top-priority skill.
6. `MONSTER_DIED` never emitted; skills and monster DoTs don't emit `COMBAT_DAMAGE`.
7. Vampiric heal doesn't refresh the HP bar.
8. Dodge direction ignores mouse movement.
9. Click-to-attack path walks onto the monster tile.
10. `charge` doesn't move; `fire_wall` is instant; chain lightning damage is simultaneous.
11. Scorch type fallback "lightning" for non-fire/ice skills.
12. `critBonus`/`slowEffect`/`attackSpeed`/`taunted` buffs are inert; `knockback` equip stat unused.
13. Teleport has no range limit (map clamp only).
14. DoT tick drift drops remainders; hero DoT/volatile deaths bypass deathSave.
15. `doubleShot` can't trigger for heroes (attackRange 1.5).
16. `death_mark` (and `shadow_step`) with no living monster in the zone fall through to the self-buff branch: death_mark
    then puts `damageAmplify 0.25` on the **hero** (hero takes +25 % damage for 8 s). Recommend FIX: require a target.
17. Leashed monsters don't walk home directly (idle + patrol drift).
18. Any skill with a `buff` skips range checks: `death_mark` (range 4) and `shadow_step` (range 5) reach the preferred
    target at any distance. Recommend FIX: enforce `range+1` for targeted buff-type skills.

Log keys used (zh-CN in `src/i18n/locales/zh-CN.ts:1920-2198`): `zone.combat.{autoCombat,autoCombatOn,autoCombatOff,
manaInsufficient,skillLocked,freeCast,skillActivated,deathMarkApplied,slowTrapHit,tauntRoar,dodgeCounterReady,
dodgeCounterCrit,comboTrigger,doubleArrow,freezeSlow,deathImmunity,unyieldingProc,restoreHp,restoreMana,curseAura}`,
`zone.teleport.{blockedByCC,unreachable}`, `zone.death.{text,logMessage}`, `zone.soulEcho.{left,faded,claimed,label,lostInDungeon}`,
`zone.monsterKill`, `sys.statusEffect.{applied,expired,refreshed,name.*}`, `sys.eliteAffix.name.*`, `sys.difficulty.*`,
`sys.player.{death,respawn}`.

---

## 22. Data tables to export to JSON (core loads at boot)

| File (suggested) | Content | Source |
|---|---|---|
| `combat/constants.json` | dodge 0.3/cap 30; crit DEX 0.2, LCK 0.5, cap 75; crit base 1.5 + LCK 0.01; stat-to-damage 0.5; defense factor 0.5; resist cap 75; DR cap 0.9; outgoing clamp 0..10; `BUFF_CAPS`; min damage 1; hero derived-stat coefficients; regen coefficients; campfire radius 5 / ×50; safe-zone default 9 | `CombatSystem.ts:115-381`, `Player.ts:106-154`, `ZoneScene.ts:83-86` |
| `combat/skill_scaling.json` | tier brackets (≤8: 1, ≤16: 0.75, else 0.5), defaults (dmg 0.05, mana 0.5, buff 0.02, cd/aoe/dur 0), cooldown floor 500, CDR cap 50 | `CombatSystem.ts:129-197` |
| `combat/hit_profiles.json` | `HIT_PROFILES` + thresholds 0.25 / 0.06 + attackSpeedScale (0.9, clamp 0.35..1) + monster-hit attacker-stop factor 0.6 + elite-kill slow-mo (200, 0.4) | `HitFeedback.ts:29-75`, `ZoneScene.ts:2931-2941,3010` |
| `combat/anim_timing.json` | per rig: attackDuration, attackWindup, attackContact, castDuration, dodgeDuration, hurtDuration, deathDuration, hurtKnockback, attackLunge, frame rates, frame counts (hero 6/8/8/4/6/6/8; monster 4/6/4/2/4 @6/10/12/10/6 fps), cast phase ratios (0.46/0.20), TRANSITION_MS; derived contact table §10.1 | `CharacterAnimator.ts:94-328`, `sprites/types.ts:23-31`, `SpriteGenerator.ts:114-120,871-877` |
| `combat/status_effects.json` | tick intervals, DR (0.5, 3000, 6000), slow floor 0.2, poison heal ×0.5; monster on-hit status rules; skill status rules (better: per-skill `statusOnHit`) | `StatusEffectSystem.ts:43-58`, `ZoneScene.ts:6204-6291` |
| `combat/skill_behaviour.json` | per skill id: behaviour class (§6.4), `groundTarget`, `projectile {minMs,maxMs,msPerPx}` or fixed delay, per-target arrow delay rule, line half-width factor 0.6, impact colour, anim kind, VFX id, `passive` flag | `ZoneScene.ts:95-105,2530-2806`, `SkillEffectSystem.ts:38-48` |
| `combat/dodge.json` | cooldown 900, iframes 220, distances {rogue 2.6, mage 2.25, warrior 1.8}, step 0.25, min 0.5, afterimages | `CombatInputSystem.ts:68-71`, `ZoneScene.ts:2395-2414` |
| `combat/input.json` | buffer window 180, target-cycle range 14, monster scan radius 12, melee reach `×1.35 + 0.5`, ranged threshold 2.5, monster projectile `clamp(px×2, 200, 500)`, combat-state debounce 1500, respawn 15000 | `ZoneScene.ts` cited above |
| `combat/elite_affixes.json` | `ELITE_AFFIX_DEFINITIONS`, `ZONE_AFFIX_COUNTS`, freeze-chance cap 0.5, curse buff duration 2000 / log interval 2000, teleport window 2²..15² and offset 1..2, frozen slow 30/2500 | `EliteAffixSystem.ts:60-211`, `ZoneScene.ts:5990-6066,3048-3052` |
| `combat/difficulty.json` | multipliers, unlock boss/zone, order | `DifficultySystem.ts:64-133` |
| `combat/soul_echo.json` | min level 5, gold shares, exp shares, claim range 1.5 | `SoulEcho.ts:14-18` |
| `combat/spirit_profiles.json` | three profiles + SPI factor 0.015 (cap 200) | `SpiritSystem.ts:32-131` |
| `combat/feedback.json` | class impact colours, skill impact colour rules, damage-number styles & timings, player-hit shake formula, vignette formula, VFX palette | `ZoneScene.ts:89-105,5698-5783`, `VFXManager.ts:38-62,377-401`, `FxKit.ts:7-20` |
Monster, skill/class, item/affix tables are exported by their own specs; this area consumes `MonsterDefinition`
(`hp, damage, defense, speed, aggroRange, attackRange, attackSpeed, expReward, goldReward, elite, animCategory, spriteKey`) and
`SkillDefinition` (`manaCost, cooldown, range, damageMultiplier, damageType, aoe, aoeRadius, buff, scaling, synergies, critBonus, stunDuration`).

---

## 23. Chapter 1 checklist (emerald_plains, Lv 1–10, normal difficulty)
* Monsters: `slime_green` (L1, hp30 dmg5 def2, atk 1500 ms, range 1.2, slime rig), `goblin` (L3, 55/8/4, 1200 ms, 1.5),
  `goblin_chief` (L5 elite, 160/14/8, 1000 ms, 1.8 — rolls 1 affix), mini-boss `miniboss_goblin_shaman` (L6 elite,
  280/16/10, 1300 ms, range **2.5 → melee** with reach 3.875, 1 affix), quest hunts `hunt_pendant_thief` (goblin ×3 hp ×1.2 dmg)
  and `hunt_redcap_gruk` (goblin ×5 hp ×1.6 dmg) — both roll 1 affix, mesh ×1.25, reveal shake 260/0.004 — with goblin minions.
  All melee, no on-hit statuses; contact 250 ms; all 7 affix types possible on elites.
* Hero: all three classes, basic attacks (contact 308/267/222 ms), tier-1 and tier-2 skills (§6.7 ✔ rows), dodge roll,
  stat dodge, targeting (click, Q-cycle, touch lock), auto-battle, input buffer, Spirit + resonance.
* Systems: full damage pipeline, buffs (DR, defenseBonus, damageBonus, manaShield, poisonDamage, damageAmplify),
  status effects (burn, freeze, poison, bleed, slow; stun only via tier 3 — keep implemented), HitFeedback + all feedback,
  floating text, monster/hero death, respawn, soul echo (from Lv5), death save/thorns/killHeal/lifesteal procs (gear-driven),
  elite affixes, difficulty scaling (normal = identity, but implement all three).
* VFX needed: impact burst, death/gold/heal bursts, hit sparks, claw rake, slash trail, dodge ghosts, target ring, soul echo,
  danger vignette, 25 chapter-1 skill effects (§18), elite affix auras + crown, status tints.

---

## 24. Unit-test vectors for the C++ core
* §2.1 damage examples (with an RNG stub returning 0.99 for "no dodge, no crit" and 0.0 for "crit").
* `classifyHit`: (1/100 killed+crit) kill; (1/100 crit) crit; 30/100 heavy; 10/100 normal; 2/100 light; tick wins over kill; maxHp 0 → light.
* `attackSpeedScale(610, 1000) = 1`; `(610, 300) = 0.4426…`; `(610, 100) = 0.35`; `(0, x) = 1`; `(x, undefined) = 1`.
* Contact: warrior `round(4×1000/13) = 308`, mage 267, rogue 222, monster 250; warrior at interval 400 → speed 0.5902 → `round(307.69×0.5902) = 182`.
* `tieredScale(0.2, 10) = 7×0.2 + 2×0.15 = 1.7`; `tieredScale(x,1) = 0`; level 17 adds 0.5×x.
* `cooldown(base 2000, L1, cdr 60) = floor(2000×0.5) = 1000`; floor-500 then CDR 50 → 250.
* Status DR (blocked attempts during immunity do not touch the record): stun 2000 @t0 → 2000; @t100 → 1000, immune until
  4100; @t3000 → 0; @t4200 → 0 (count 3, last=4200, immune until 7200); @t13300 (> 6000 since last 4200) → count resets → 2000.
  Note: attempts arriving < 6000 ms apart after the 2nd keep the target immune indefinitely.
* Soul echo: L4 → 0/0; L5 gold 1000 normal → 100 gold, 0 exp; hell exp 50 of toNext 1200 → min(50, 60) = 50; claim at distance 1.5 succeeds, 1.51 fails, other map fails.
* Difficulty: goblin nightmare → hp 83 (82.5 rounds up), dmg 12, def 5 (5.2), exp 36, gold [6,12].
* Elite: extra_strong+vampiric combined dmg×1.485, hp×1.625, lifesteal 0.2, lootQ 13; freeze chance cap 0.5.
* Target cycle: ids/distances sorted, wrap, out-of-range excluded, dead excluded, empty → null.
* Input buffer: request not-executable at t → executes on first frame ≤ t+180 that becomes executable; dropped after.
* Dodge controller: start at 0 → ready at 900, invulnerable < 220, one avoidance reward per window.
* Spirit: warrior spi 5 hit = 8×1.075 = 8.6; crit hit = 12×1.075 = 12.9; resonance at 100 → drains to 0 over 6000 ms; no gain while resonating.

---

## Open questions (decisions for the architecture owner)
1. **Quirk policy**: keep or fix each item in §21. Strong FIX candidates: mana shield mana deduction; monster buff expiry;
   skills/merc/extra hits honouring `isDodged` (show MISS); hero slow; passive skills non-castable + auto-battle picking the
   first *executable* skill; death_mark/shadow_step requiring a target; emitting `MonsterDied` and `CombatDamage` for all hits.
2. **Units**: `TILE_SIZE_UU` (suggest 100) and the px→tile factor for timing formulas (spec uses 36 px/tile for projectile
   flight / arrow delays, 45 px/tile for VFX sizes).
3. **Determinism**: is a seeded, replayable combat RNG required (tests only vs. save/replay)? Spec assumes injectable PRNG,
   distribution parity only.
4. **Sim tick**: fixed 60 Hz assumed so per-frame rules (DoT drift, leash step, mercenary 0.06 tiles/frame) match; confirm.
5. **Camera shake scale**: canonical amplitude = web at render scale 1 (`I × 3.24 × viewport width`, uniform jitter, no
   decay)? Many UE shakes decay — keep the flat profile or allow a decay curve?
6. **Slow-motion** on elite kills: global time dilation with an undilated core tick, or per-actor visual dilation?
7. **Mini-boss `miniboss_goblin_shaman`** has attackRange 2.5, which the web treats as *melee* (threshold is `> 2.5`):
   keep a 2.5-tile claw swipe, or make it a ranged caster (threshold `>= 2.5` or data flag `ranged`)?
8. **Skill fidelity**: `charge` without movement, instant `fire_wall`, simultaneous `chain_lightning` damage, unlimited
   `teleport` range — keep for Chapter 1 or upgrade (dash, lingering wall, 55 ms staggered chain, range clamp 6)?
9. **Status data**: replace substring rules (§7.3/§7.4, scorch type, impact colour) with explicit per-monster / per-skill
   data (`onHitStatus`, `statusOnHit`, `impactColor`, `scorch`) — proposed; confirm schema owner (skills/monsters spec).
10. **Click-to-attack**: stop the approach path at attack range (recommended) vs. walk onto the monster's tile (web).
11. **Touch**: teleport destination and dodge direction on mobile with the 3D camera (web: joystick ×6 tiles / target /
    self; released joystick dodges √2× toward screen-right).
