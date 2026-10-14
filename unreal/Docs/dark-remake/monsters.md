# Monsters (dark remake)

> Chapter 1 in implementable detail, chapters 2-5 in outline. Tags: **[W1]** works on today's core (one Attack01, melee
> or a bolt when range > 2.5, explicit on-hit statuses, elite affixes, hunts); **[W2]** needs the wave-2 systems
> (abilities and telegraphs, on-death, corpses, phases, packs and morale, kite and block; implementation-plan.md §3);
> **[W3]** needs sub-dungeon travel. Speed is the data unit (tiles/s = speed × 0.03).

## 1. Design rules

### 1.1 Families and roles

Each family has a theme, a silhouette language and roles that combine; each role has one silhouette, one weapon and one
accent. Chapter 1 has four families and nine regular roles, plus three hunts, a mini-boss and the act boss.

| Family | zh | Rig | Roles in Chapter 1 |
|---|---|---|---|
| The Unburied | 不葬军 | SKEL_Human (new monster kit) | shambler, ranged, shield wall, summoner-aura, boss |
| The Living | 赤誓会 / 逃兵 | SKEL_Human (same kit, living palettes) | frenzy melee, skirmisher, sustain (drummer) |
| The Gravegnawers | 啃冢者 | SKEL_Goblin (re-skins) | swarmer + corpse-eater, bruiser |
| Breach-blood | 渊血 | SKEL_Slime (re-skins) | fodder, exploder |
| (mini-boss) | 尸牧婆 | SKEL_Goblin shaman mesh, hooded, human palette | raiser |

### 1.2 Readability and colour discipline (DR5)

The environment is dark and desaturated; the actors stay readable. Keep the baked ink hull (monster class 3.0 px,
per-asset requests 3.5 px), the rim light and the selection rim (R16).

| Colour | Hex | Who may use it | Never |
|---|---|---|---|
| Ember orange | #FF7A2A family (hue 15-35°, sat > .6) | the hero, the Abyssfire, loot beams, refuges, interaction | any enemy emissive, eye or projectile |
| Warning crimson + bone fill | rim #D11F2E, fill #FFE8DC | enemy damage telegraphs (ground decals that fill to full at contact) | hero ground effects (no hero effect draws a filling shape) |
| Breach-violet | #B02FD0 around near-black #1A0820 | enemy magic that is not a damage hazard (raises, buffs, bolts' cores), breach fire, the breach glow, Unburied and Corpseherd eyes (#D08CFF pinpoints) | within 30° of hue of the mage's mana shield (#8A5CFF) at sat > .5 in any hero recipe |
| Crimson accent | #A3221E-#C0283A | the crusade's tabards, the oath-scar, Harke's scarf, danger and low-life UI | — |
| Bone-yellow | #EDE6B0 | Gravegnawer eyes (beasts, not breach-touched) | — |
| No glow | — | the Living: they are alive; their marker is the crimson cloth | — |

Every hostile carries exactly one family marker (violet pinpoint eyes, bone-yellow eyes, or crimson cloth). The art
review adds a **darkest-tile gate**: each monster rendered on the darkest `march` terrain tile at the W1 camera must keep
its rim ≥ 3 px, a lit-side ΔL* ≥ 15 against the ground, and an eye or marker accent ≥ 2 px (implementation-plan.md A0).

### 1.3 Fairness rules

- Windups: every special ≥ 600 ms on Normal (bosses ≥ 700 ms); the contact beat is the clip's Contact notify and the
  telegraph fills to full exactly then.
- No single enemy hit exceeds 35 % of a same-level hero's maximum life on Normal (`hitCapHeroMaxHpPct`, scaled by the
  difficulty table on Nightmare / Hell).
- Exploders: fuse ≥ 800 ms with a visible ring, damage capped at 20 % of the hero's maximum life, and the burst hurts
  monsters too (×2), so the player can use it.
- Raisers: a per-summoner cap of live Raised, at most one raise per window, a corpse can be raised once, Raised give no
  exp or loot and never respawn, and every Raised collapses when its raiser dies.
- No invulnerable phases. Soft enrages shrink space; they never one-shot.

### 1.4 The mechanic ramp

| Level | New idea | Where |
|---|---|---|
| 1-2 | slow shamblers in numbers; blobs | Burial Trenches (footmen), the Seep (clots) |
| 3 | fast swarmers; something that eats the dead | Gnawed Field (gnawers) |
| 4 | exploders | Weeping Ditch (bloats) |
| 5 | ranged in formation | Broken Redoubt (arbalesters) |
| 6 | frenzy at low life; packs that break; the raiser | Penitents' Camp (zealots), Deserters' Hollow, the Sheepfold (mini-boss) |
| 7 | shield walls; a banner that raises and inspires; telegraphed bruisers; champion packs | Dead Ranks, Crowmarket midden, ambush Musters |
| 9 | everything together: the act boss | the Breach Rim |

### 1.5 Corpses and ash (DR6)

A killed Unburied, Living or Raised monster leaves a corpse entry for 20 s (cap 24 per zone) [W2]. In ash mode, the
default for every locale, the body greys and collapses into an **ash husk** that keeps the corpse's position for its TTL
and fades; the same rules (raise, consume, burn) apply to husks. Breach-blood leaves a tar puddle, not a corpse; the
Gravegnawers leave a carcass the other gnawers ignore. A fire killing blow or a fire skill over a corpse burns it
(unraisable, unconsumable). The optional gore layer adds blood decals and keeps the body instead of the husk.

## 2. Chapter 1 roster

### 2.0 Summary

Reference numbers today: slime L1 30 HP / 5 dmg / 2 def / 40 speed / aggro 4 / range 1.2 / 1500 ms / 12 exp; goblin L3
55 / 8 / 4 / 55 / 5 / 1.5 / 1200 / 18; chief L5 160 / 14 / 8 / 50 / 6 / 1.8 / 1000 / 55; shaman L6 280 / 16 / 10 / 45 / 7
/ 3.0 / 1300 / 90.

| Id | zh / en | Family, role | L | HP | Dmg | Def | Spd | Aggro | Range | Atk ms | Exp | Gold | vs today |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `breachclot` | 渊血凝块 / Breachclot | Breach, fodder | 1 | 28 | 5 | 2 | 38 | 4 | 1.2 | 1500 | 11 | 2-4 | slime, a little softer and slower |
| `risen_footman` | 复起步卒 / Risen Footman | Unburied, shambler | 2 | 50 | 7 | 5 | 32 | 4 | 1.5 | 1600 | 15 | 2-5 | between slime and goblin; slow, sturdy |
| `gravegnawer` | 啃冢者 / Gravegnawer | Gnawers, swarmer | 3 | 42 | 7 | 3 | 66 | 6 | 1.3 | 950 | 17 | 1-4 | goblin, frailer, 20 % faster, quicker bites |
| `cinderbloat` | 烬胀囊 / Cinderbloat | Breach, exploder | 4 | 40 | 4 | 2 | 30 | 5 | 1.2 | 1800 | 18 | 1-3 | slow; dangerous when it dies |
| `risen_arbalest` | 复起弩手 / Risen Arbalester | Unburied, ranged | 5 | 46 | 11 | 4 | 34 | 7 | 6.0 | 2200 | 24 | 3-6 | first ranged regular |
| `oath_zealot` | 赤誓狂信徒 / Red-Oath Zealot | Living, frenzy | 6 | 90 | 11 | 6 | 52 | 6 | 1.5 | 1100 | 30 | 4-8 | goblin ×1.6 HP; bleeds you |
| `deserter` | 逃兵 / Deserter | Living, skirmisher | 6 | 80 | 12 | 7 | 56 | 5 | 1.6 | 1150 | 28 | 6-12 | more gold; breaks when the leader falls |
| `deserter_drummer` | 军鼓手 / Deserter Drummer | Living, sustain | 6 | 70 | 8 | 5 | 50 | 6 | 1.5 | 1400 | 28 | 4-9 | W2 mixes only |
| `risen_shieldbearer` | 复起盾卒 / Risen Shieldbearer | Unburied, shield wall | 7 | 130 | 12 | 14 | 30 | 5 | 1.6 | 1700 | 34 | 3-7 | chief-class HP, very high defence, slow |
| `dead_standard_bearer` | 亡旗手 / Dead Standard-Bearer | Unburied, summoner-aura | 7 | 140 | 8 | 8 | 30 | 6 | 1.6 | 1700 | 40 | 4-8 | the priority target |
| `bonegorger` | 饕骸者 / Bonegorger | Gnawers, bruiser | 7 | 210 | 17 | 9 | 42 | 6 | 2.0 | 1700 | 48 | 6-12 | chief ×1.3 HP, slower, one big slam |
| `miniboss_corpseherd` | 尸牧婆 / the Corpseherd | mini-boss, raiser | 6 | 300 | 15 | 10 | 45 | 7 | 3.0 | 1300 | 95 | 15-30 | shaman ×1.07 HP |
| `valdras` | 焚旗元帅·瓦德拉斯 / Lord-Marshal Valdras | act boss | 9 | ~1100 | 20 | 12 | 48 | 8 | 2.0 | 1300 | 220 | 30-60 | chief: +4 levels, ×6.9 HP, three phases |
| `sub_pitwarden` [W3] | 掘坑官 / the Pitwarden | Unburied, sub-dungeon mini-boss | 8 | 420 | 17 | 11 | 40 | 7 | 1.8 | 1500 | 110 | 20-40 | — |

Hunts (quest_hunts.json; hunt defaults def ×1.2, exp × max(3, hpMul), gold ×3, aggro ≥ 7, visual scale 1.25):

| Hunt | Base | HP × | Dmg × | Minions | Where | Quest |
|---|---|---|---|---|---|---|
| `hunt_hungerbell` 饿钟 / Hungerbell | `bonegorger` | 3 | 1.3 | 3 `gravegnawer` | (45,35), revealed after objective 1 | M2 |
| `hunt_dreck` 拾银者·德雷克 / Dreck the Silverpicker | `deserter` | 3 | 1.2 | 2 `deserter` | (60,48), revealed after the clues | S3 |
| `hunt_harke` 断誓者·哈克 / Harke the Oathless | `deserter` | 5 | 1.6 | 4 `deserter` | (82,72), spawns on accept | S4 |

Hunt leaders are elite with the mini-boss loot floor and roll the zone's one affix, as today; their minions do not.

**Raised** is a role, not a definition: a corpse raised by the Corpseherd, a standard-bearer or Valdras stands as
`risen_footman` (Unburied or Living corpses only) at 40 % HP, role `Raised` (no exp, no loot, no respawn, collapses
with its raiser) [W2].

### 2.1 Breach-blood (SKEL_Slime)

#### `breachclot` 渊血凝块 / Breachclot — fodder

| | |
|---|---|
| Behaviour [W1] | today's idle / patrol / chase / attack with leash; a slow bite (the slime's hop-slam). On hit: poison 20 % for 3 s [W1 explicit `onHitStatus`]; [W2] **Ashnumb** instead (§4) |
| On death | sinks into a tar puddle that dries in 3 s (no corpse). Drops `mat_breach_ichor` for S1 |
| Packs, spawns | the Seep (20,45) ×7; mixed into the Weeping Ditch [W2]; ambush pools; the Pit's Ash Steps [W3] |
| Sound | the slime voice family darkened: a thicker squelch, a faint sizzle, no cute pitch |
| Art brief | **Re-theme of `slime.py`** (~1 task). Silhouette unchanged (0.43 × 0.88 m dome). Translucent shell black-crimson (`M_AF_Toon_Slime` params: body #2A0E14, opacity .62) with a breach-violet fresnel rim #B02FD0; the cartoon eyes are removed; inside, where the bone and bubbles were: a dented kettle helm and two crossbow bolt-heads suspended, plus 3-4 violet sparks drifting (emissive #D08CFF, small). Clips unchanged (Idle, Walk hop, Attack01 slam, Hurt, Death melt). Palette swatches (`Monsters_March`): `clot.shell` #2A0E14, `clot.rim` #B02FD0, `clot.helm` #4A4D52, `clot.spark` #D08CFF (emissive). Budget as today: 2.9k / 1.5k |

#### `cinderbloat` 烬胀囊 / Cinderbloat — exploder

| | |
|---|---|
| Behaviour | [W1] a slow tank with burn on hit (30 %, explicit); [W2] **on-death burst**: on death it swells (bone-scale Death clip, 800 ms) under a crimson ring decal r2.2, then bursts: 18 fire damage, capped at 20 % of the hero's maximum life on Normal, ×2 against monsters, burns corpses in the radius. [W2 stretch] it lights its own fuse when it reaches melee range |
| Packs, spawns | Weeping Ditch (85,18) ×6 [W1] / ×3 with 3 clots [W2]; siege wave 2 |
| Sound | a crackling wheeze while it walks; a rising hiss on the fuse; a dull whump |
| Art brief | **Variant of the clot** (~1 task) at ×1.35. Opaque cracked crust instead of a clear shell (#3A1A12 crust, #1A0A0A cracks) with breach-violet fissures (#B02FD0 emissive, never ember); ash flakes drift off it. Death becomes the fuse: the crust bulges along the fissures, light swells, then the burst VFX (`death.burst` recoloured: violet-black flame, ash ring; no gore) |

### 2.2 The Unburied (SKEL_Human, new Unburied monster kit)

**Shared look.** The crusade's dead in ash mode: skin like burnt clay, ash-grey and finely cracked (#8C8F86, shadow
#4C4F4A), no open wounds and no exposed bone. Torn crimson tabards (#6B1F1F, tarnished trim #B49A5E) with the church's
open-palm sigil scorched; kettle helms or a bare cracked scalp; mail shreds (#5A5D63); broken arrow shafts in the back.
Pinpoint breach-violet eyes (#D08CFF emissive). Rest pose hunched (lean 12°, head tilted 15° like a broken neck, one
shoulder dropped). **They keep their last order**: idle variants mime it (a sentry paces a line that is no longer there,
an arbalester works an empty crossbow, the burial detail digs).

**Shared clips** (`monsters/uclips.py`, adapted from `mclips.GoblinAnims` to human proportions): Idle, Idle_Order (the
mime), Walk (a dragging foot; FootL / FootR notifies), Attack01 (WindupEnd + Contact), Hurt, Death (folds forward,
crumbles to an ash husk), **Rise** (Death reversed and retimed: used by raises, spawns and the Pit's dormant footmen),
Stun. Voice family **Undead**: a dry rasp, mail rattle, a groan that is almost a word.

**Budget.** LOD0 ≤ 7k, LOD1 ≤ 3k triangles; 1.70-1.80 m (the hero is never shorter than a townsperson, R2). The kit
reuses `npcs/folk.py` body and kit parts plus new crusader parts (tabard, kettle helm, mail skirt, gambeson).

#### `risen_footman` 复起步卒 / Risen Footman — shambler

| | |
|---|---|
| Behaviour | [W1] plain chase / attack / leash; never flees. Attack01: a slow, heavy overhead chop, Contact ≈ 600 ms, so the clip-derived wind-up tint reads. [W2] the **不葬 Unburied** family trait: 25 % chance to rise once, 3 s after death, at 40 % HP; prevented by a fire killing blow, a crit or a burned corpse |
| Packs, spawns | Burial Trenches (25,20) ×8 (M1); Dead Ranks mix [W2]; siege waves 1 and 3; ambush; the Pit's Helm Wall (dormant, aggro 3) [W3]; the default Raised body |
| Sound | Undead voice; a sword scraping the ground |
| Art brief | **First member of the kit** (~3 tasks with the kit and clip library). A notched arming sword (re-used `SM_Weapon_Sword` T1). Variant looks per instance: kettle helm / bare scalp / hood; one in three carries a shovel instead of a sword (the burial detail) |

#### `risen_arbalest` 复起弩手 / Risen Arbalester — ranged

| | |
|---|---|
| Behaviour | [W1] today's ranged path (range 6.0 > 2.5): Attack01 aims and looses, Contact ≈ 450 ms; the bolt is bone-white with a breach-violet trail (`projectileColor` #B02FD0). Holds the Broken Redoubt. [W2] kite: steps back while the hero is within 2 tiles (band 3-6) |
| Packs, spawns | Broken Redoubt (75,50) ×5 [W1] / ×3 with 2 shieldbearers [W2]; the Pit's Tag Cellar [W3] |
| Sound | a crossbow crank, a dry twang |
| Art brief | **Attachment variant** (~1 task + 1 for the crossbow mesh): a heavy crossbow (new held item `SM_MonWeapon_Crossbow`) and a bolt quiver; a pavise slung on the back |

#### `risen_shieldbearer` 复起盾卒 / Risen Shieldbearer — shield wall

| | |
|---|---|
| Behaviour | [W1] slow melee with very high defence; Attack01 a shield bash, Contact ≈ 500 ms. [W2] **frontal block**: a 100° arc in front takes −50 % damage and sparks (`EvBlock`); flanking or a shield-breaking skill gets through. [W2] **Wall**: shieldbearers within 2 tiles of each other share the arc's facing (the line holds) |
| Packs, spawns | Dead Ranks (30,80) ×6 [W1] / ×3 [W2]; Broken Redoubt [W2]; siege wave 3; the Pitwarden's guard [W3] |
| Sound | a dull clang on block; boots in step |
| Art brief | **Variant** (~1.5 tasks): a tall tower shield (new held item, crimson field with the scorched sigil, ash-streaked), a mail coif, a mace (re-used `SM_Weapon_Mace`). The shield must read from the camera as a flat slab covering the left side of the body |

#### `dead_standard_bearer` 亡旗手 / Dead Standard-Bearer — summoner and aura (champion role)

| | |
|---|---|
| Behaviour | [W1] slow melee with the pole. [W2] **Muster**: every 10 s raises one corpse within 6 tiles (breach-violet marker ring, 900 ms), cap 2 Raised. **Aura**: Unburied within 5 tiles deal +15 % damage (a crimson ground halo, visible). Leader of Dead Ranks packs. Kill it first; it is Valdras's P2 lesson |
| Packs, spawns | Dead Ranks leader [W2]; siege wave 3; Valdras P2; the ambush **Muster** champion pack [W2] |
| Sound | the banner snapping in wind it should not have; a low horn note when it raises |
| Art brief | **Variant** (~1.5 tasks): the tallest silhouette in the family, a pole with a tattered crimson banner (cloth chain bones, wind loop), a sword at the hip. Clip additions: Cast01 (plants the pole, raises) |

#### `valdras` — see §2.6.

#### `sub_pitwarden` 掘坑官 / the Pitwarden [W3]

An Unburied officer with a spade who dug the pits before the battle (the deserter's journal pays off). SubDungeonMiniBoss
role in the Pit's fourth room. Mechanics: a spade sweep (cone 100°, r2.2, 700 ms) and **Dig** (every 12 s he drives
the spade in: a crimson line of 4 tiles collapses after 800 ms, slow 30 %); two shieldbearers guard him. Art: a footman
variant with an officer's sash, a gorget and a long spade (~1 task).

### 2.3 The Living (SKEL_Human, living palettes)

The same kit as the Unburied with living palettes and no glow, so the player sees that the dead and the living wore the
same tabard. Voice family **Human**: breath, effort, chant fragments.

#### `oath_zealot` 赤誓狂信徒 / Red-Oath Zealot — frenzy melee

| | |
|---|---|
| Behaviour | [W1] fast melee with a flail, Contact ≈ 420 ms; on hit bleed 15 % (explicit). Never flees. [W2] a one-row phase table: at ≤ 50 % life it **frenzies** (attack speed ×1.35, move ×1.2, takes +10 % damage): the robe whips, the oath-scar flares, a shouted chant |
| Packs, spawns | Penitents' Camp (95,35) ×5; siege wave 3; rescue-event guards; [stretch] S8's relic fragments |
| Sound | chant fragments (「死守——」), a flail's chain whirr |
| Art brief | **Variant** (~1 task + 0.5 for the flail): hooded red robe over mail (#7E1416), rope belts, bare forearms with the **oath-scar** (a fist crushing a flame) burned in, faint crimson emissive #C0283A; a flail (new held item, or the mace re-used in W1). Skin #B88A6E |

#### `deserter` 逃兵 / Deserter — skirmisher (and `deserter_drummer`, sustain)

| | |
|---|---|
| Behaviour | [W1] melee with an axe or falchion, Contact ≈ 450 ms; drops more gold. [W2] **morale**: if the pack's leader dies, every deserter within 8 tiles flees for 5 s (`Flee` state, away from the leader's corpse; a blocked deserter fights instead), unless a living `deserter_drummer` is within 8 tiles: while he drums (a channel, 2 s every 8 s), deserters nearby cannot be feared and attack 10 % faster. Kill order puzzle: drummer, then leader |
| Packs, spawns | Deserters' Hollow (80,75) ×6 [W1] / 4 + drummer + elite leader [W2]; minions of `hunt_dreck` and `hunt_harke` |
| Uniques | `hunt_dreck`: the loot sack attachment re-used plus a bandolier of silver name-tags (the pendant chain re-used). `hunt_harke`: the red cap re-modelled as a red scarf (#A3221E, the only bright red on a deserter) plus a captain's gorget; [W2] his men break when he falls |
| Sound | grunts, a hissed 「走！」 when they flee; the drummer's march beat |
| Art brief | **Variant** (~1 task; drummer +0.5): drab gambeson with torn-off insignia (#4E5040), hood and a scarf over the face (#2F2B26), a looted pack, leather (#5E4630); axe or falchion (re-used weapons). Drummer: a battered company drum on a strap, sticks in both hands, Cast01 = drumming |

### 2.4 The Gravegnawers (SKEL_Goblin re-skins)

Carrion things drawn by the battle: hairless, ash-grey, long-armed, needle-toothed. They eat what the bell would toll
for. They are beasts, not breach-touched: **bone-yellow eyes** (#EDE6B0). They never travel with the Corpseherd; they
are her rivals (they eat her flock). Voice family **Gnawer**: the goblin voices re-pitched lower, wet chittering.

#### `gravegnawer` 啃冢者 / Gravegnawer — swarmer, corpse-eater

| | |
|---|---|
| Behaviour | [W1] fast melee, quick bites (Contact ≈ 300 ms), low silhouette. [W2] **pack aggro** (provoking one wakes its pack within 6 tiles). [W2] **Consume**: a gnawer near a corpse and out of combat (or below 50 % life) tears at it for 2 s to heal 30 % and remove it (denies raises); in ash mode it digs at the husk |
| Packs, spawns | Gnawed Field (35,40) ×8 (M2); Crowmarket midden mix [W2]; siege wave 2; ambush; minions of Hungerbell; the Pit's Tag Cellar [W3]; after `rf_chapter_dawn` they replace the Unburied spawns [W2] |
| Sound | chittering, a snapping bite, scrabbling claws |
| Art brief | **Re-skin of `goblin.py` through a new `monsters/gnawers.py`** (subclasses `GoblinKit`; `goblins.py` is not edited; ~2 tasks): arms +15 %, an emaciated ribbed torso (rib strokes with `surface_paint`), hairless ash hide (#6F6A60, shadow #3A3631, belly #9C8F7A), the big ears cut to torn stubs, a long needle-toothed muzzle (jaw bone driven), claws #D8CCB0, scraps of crusade tabard tied on as trophies (#5A1E1E), a broken crusade spearhead lashed to a stick (the goblin spear shortened; no bone weapon). Knuckle-drag idle. Same clips as the goblin, Walk retimed lower and faster |

#### `bonegorger` 饕骸者 / Bonegorger — bruiser

| | |
|---|---|
| Behaviour | [W1] slow heavy melee, Contact ≈ 700 ms (the long wind-up reads). [W2] **钟锤 Clapper Slam**: circle r2 at the target, windup 800 ms (the shipped chief `Roar` or a new Attack02), ×1.6, cooldown 6 s; the bell rings on impact |
| Packs, spawns | Crowmarket midden (95,90) ×3 [W1] / 1 + 2 gnawers [W2]; Hungerbell's base |
| Unique `hunt_hungerbell` | ×1.25 scale; its clapper still carries a jagged lip of Ashwheat's bell; a necklace of name-tags. [W2] every slam calls every gnawer within 10 tiles (social aggro) |
| Sound | wet breathing, a low bell note on every swing |
| Art brief | **Re-skin of `goblin_chief.py`** (~1.5 tasks + 0.5 for Hungerbell's attachments): a bloated gnawer with mangy tufts and an oversized jaw; a church-bell clapper as a club on `weapon_r` replaces the cleaver (bronze #8A6A3A). **No apron, no cleaver, no hooks, no larder.** Hide #5F5A50, belly #8F826C |

### 2.5 Mini-boss: `miniboss_corpseherd` 尸牧婆 / the Corpseherd — raiser

Fight design in chapter1.md §10. Summary: L6, 300 HP, ranged 3.0 (decision M5's role kept), 1300 ms, elite + mini-boss,
one affix, once per visit, three pre-fight lines.

| | |
|---|---|
| Behaviour | [W1] ranged elite: a bell-toll bolt (a breach-violet ripple, `projectileColor` #B02FD0), on hit slow 20 % for 2 s; Taunt clip during the pre-fight lines. [W2] **召羊 Call the Flock** (raise cap 3, one raise per 8 s, two at the pull), kite 3-5 tiles, Raised collapse on her death; [stretch] Keen below 50 % |
| On death | `cs_rf_herd`: the dead around her sit down |
| Sound | a cracked handbell for every cast; a hummed lullaby under the pre-fight lines; a hoarse 「回家吧」 |
| Art brief | **Re-dress of `goblin_shaman.py` through `monsters/corpseherd.py`** (~2 tasks + portrait 0.5). Scale ×1.3 (≈ 1.21 m): with the hunch she reads as an old woman. A deep hood and a widow's veil hide the goblin head, ears and face entirely: only the violet eyes (#D08CFF) and a thin grey chin show. Long-fingered hands in a human ashen palette (#A89A8C). A long skirt and a shawl patched from crusade banners (#5E1012, #3B3530 rags). The totem becomes a shepherd's crook of pale ash-wood (#D9CEB4) with a cracked bronze handbell (#8A6A3A) tied on; Cast01 rings it through an `FX_bell_ring` notify (recipe `notify.bell_ring`: a breach-violet ripple). Portrait `T_UI_Portrait_Corpseherd` (hood, veil, eyes, bell) |

### 2.6 Act boss: `valdras` 焚旗元帅·瓦德拉斯 / Lord-Marshal Valdras

Fight design (three phases, adds, telegraphs, Feed, soft enrage, numbers) in chapter1.md §11.

| | |
|---|---|
| Behaviour | [W1] single phase, ~900 HP: a two-hit Attack01 with an authored long contact (≈ 650 ms); Roar under the intro. [W2] P1 Banner Sweep + Oath-Brand; P2 Roll Call (standard-bearer muster, Feed, Breach-seep); P3 No Retreat (frenzy multipliers, Lunge, the closing banner ring) |
| Damage | physical combo; specials `fire` (breach fire); hit cap 35 % of a level-9 hero's max life |
| Sound | plate grinding, a voice like a drill-yard order; the standard cracking in a wind that isn't there; on P3 the banner's fire roars |
| Art brief | **New mesh on SKEL_Human** through `monsters/valdras.py` (~5 tasks + portrait). Scale ×1.4 (≈ 2.4 m with the standard). Blackened plate (#2E2F33) whose seams leak pale breach-violet (#B02FD0 rim, #F0E0FF core; never ember); a great helm with a burned plume and a single glowing visor slit (#E8D8FF; the manifest's `visorGlow` is already parsed); no face, no crown, no skull, no throne. **The crusade's scorched standard has grown through his spine** and rises above his head (crimson cloth #5E1012 charred to #1A1210 at the edges; cloth chain bones with a wind loop): the silhouette is "the banner that will not fall". A two-handed oath-sword (#6A6D73) whose grip has melted into his gauntlet. Clips: Idle (at attention), Walk (march), Attack01 (two-hit), Attack02 (sweep), Cast01 (slam), Roar (phase change: the standard flares), Lunge, Hurt, Death (kneels, sword point-down, the standard droops). `attackTrail` breach-violet. Budget LOD0 ≤ 11k, LOD1 ≤ 4k (a boss exception above the 9k family pass; the standard's cloth is most of it). Portrait `T_UI_Portrait_Valdras` |

### 2.7 Stretch monsters (cut first)

| Id | zh / en | Role | Notes |
|---|---|---|---|
| `false_lantern` | 鬼灯 / False Lantern | ranged lure | Cold lights drifting among S5's lantern graves; idle it is a pretty still light, on aggro a lamprey mouth opens behind it (range 6, slow bolt). A new mesh on SKEL_Pet_Sprite (~3 tasks); it must read as the opposite of the Lanternwisp (cold #CFE8FF vs warm, a mouth vs a bell) |
| `ash_outrider` | 灰斥候 / the Ash Outrider | roaming stalker | A dead outrider on foot (no horse rig) who starts following the hero past leash after 60 kills, scales to the hero's level, drops a guaranteed rare. Footman kit variant with a lance (~1 task) plus a small core rule (roaming spawn) |

### 2.8 Loot hooks

| Source | Hook |
|---|---|
| Regulars | today's level-based drops (`loot_rules.json`); deserters roll the higher gold range; Gravegnawers drop little gold (they carry nothing) |
| Elites and champions | the affix loot bonus (3-8 per affix) as today; a champion Muster drops one guaranteed magic item [W2] |
| Hunts | the mini-boss floor plus their quest items: `mat_tag_pouch` (Dreck, chance 1); Hungerbell and Harke drop a guaranteed magic item |
| Mini-boss | the existing mini-boss floor (magic .8 / rare .35 / legendary .03, `lootTable` still inert per M9); first kill per visit only |
| Act boss | the existing story-boss floor; farmable once per visit after the finale (M7) |
| Quest drops | `mat_breach_ichor` from `breachclot` (S1, chance .5) |
| Raised, defend waves, ambush adds | Raised: nothing; defend and ambush monsters: today's rules (no respawn, M4 / W9) |
| Stretch | S8's relic fragments from `oath_zealot`; the Ash Outrider's guaranteed rare |

## 3. Elite affixes, champions, the elite marker

The seven affix ids and their stats are unchanged; display names are re-voiced; several gain telegraphs or death
effects [W2]. `emerald_plains` keeps exactly one affix.

| Id | Today | Remake | Added [W2] |
|---|---|---|---|
| `fire_enhanced` | 炎魔 | 焚誓 / Oathfire | on death: a breach-fire burst after 1 s, r2, crimson decal, capped |
| `swift` | 迅捷 | 驱策 / Driven | — |
| `teleporting` | 瞬移 | 渊行 / Breachwalker | a violet smear on each blink |
| `extra_strong` | 狂暴 | 赤誓 / Red-Sworn | — |
| `curse_aura` | 诅咒 | 丧钟 / Knelled | the r4 aura drawn as a ring of bell ripples on the ground |
| `vampiric` | 吸血 | 噬渊 / Breach-Drinker | — |
| `frozen` | 冰封 | 墓寒 / Gravecold | on death: a frost ring r2.5 after 600 ms, slow 30 % for 2.5 s |
| (new) `unburied` | — | 不葬 / Unburied | Unburied family only: rises once at 50 % HP after 3 s; fire or a burned corpse prevents it |

**Champion packs [W2]:** the **Muster** (a standard-bearer and three footmen sharing one affix, from the ambush event),
the **Hollow Watch** (Deserters' Hollow leader, elite with one affix, drummer and four deserters). **The elite marker:**
W1 recolours `SM_FX_EliteCrown` to tarnished iron with a crimson stone; W2 replaces it with `SM_FX_EliteSigil`, a small
floating broken iron halo with a crimson ember (~0.5 task).

## 4. Status: 麻灰 / Ashnumb [W2]

From breach-blood contact. Each stack reduces healing received by 10 % (potions, regeneration, life steal), up to 5
stacks; at 5 stacks the hero is also slowed 20 %. Stacks last 6 s and refresh on hit. The antidote cleanses it (as
poison, I4). Visual: grey ash creeping up the hero's limbs (a CPD tint band plus ash flakes). W1 fallback: poison.

## 5. Art plan for Chapter 1 monsters

What the previews show today (`Art/Previews/mon_family/lineup_game.png`, `ch1_coherence/lineup_characters*.png`): a
bright, saturated cartoon cast on vivid green grass: lime-green goblins with big ears and cute proportions, a jelly slime
with cartoon pupils, a red-and-gold knight, thick black ink and strong rim light. The ink and rim are the readability
asset and stay. What changes: **palettes** (desaturated, low value, one marker accent per family), **proportions** away
from cute (smaller heads, longer limbs, no pupils, hunched or towering silhouettes), **materials** (cracked ash, tar,
dull iron) and **emissives** used sparingly as markers. The toon bands stay; the shade band gets deeper (palette shadow
mixes, not a renderer change).

| Asset | Kind | Rig / source | Generator (new files) | Est. tasks | Budget LOD0 / LOD1 |
|---|---|---|---|---|---|
| `Monsters_March` palette atlas | new family atlas | `kit/palette.py` registry | `palettes/Monsters_March.json` | 0.5 | — |
| Unburied kit + clip library + `risen_footman` | new mesh family | SKEL_Human + folk parts + new crusader parts | `monsters/unburied.py`, `monsters/uclips.py`, `monsters/risen_footman.py` | 3 | 7k / 3k |
| `risen_arbalest` + crossbow | variant | Unburied kit | `monsters/risen_arbalest.py` | 2 | 7k / 3k |
| `risen_shieldbearer` + tower shield | variant | Unburied kit | `monsters/risen_shieldbearer.py` | 1.5 | 7k / 3k |
| `dead_standard_bearer` | variant | Unburied kit | `monsters/dead_standard_bearer.py` | 1.5 | 7.5k / 3k |
| `oath_zealot` + flail | variant (living) | Unburied kit | `monsters/oath_zealot.py` | 1.5 | 7k / 3k |
| `deserter` + drummer + Dreck / Harke attachments | variant (living) | Unburied kit | `monsters/deserter.py` | 2 | 7k / 3k |
| `gravegnawer` | re-skin | SKEL_Goblin | `monsters/gnawers.py`, `monsters/gravegnawer.py` | 2 | 6.1k / 3.2k |
| `bonegorger` + Hungerbell | re-skin | SKEL_Goblin (chief) | `monsters/bonegorger.py` | 2 | 9k / 3.3k |
| `miniboss_corpseherd` + portrait | re-dress | SKEL_Goblin (shaman) | `monsters/corpseherd.py` | 2.5 | 9k / 3.4k |
| `breachclot`, `cinderbloat` | re-theme | SKEL_Slime | `monsters/breach.py` | 2 | 2.9k / 1.5k |
| `valdras` + portrait | new mesh | SKEL_Human ×1.4 | `monsters/valdras.py` | 5.5 | 11k / 4k |
| `SM_FX_EliteSigil` | new FX mesh | — | `monsters/elite_sigil.py` | 0.5 | — |
| Family passes (budget, LOD1, re-ship, review, lineup on march terrain) | pass | all | `monsters/lineup.py` (re-pointed) | 3 | — |
| [W3] `sub_pitwarden` | variant | Unburied kit | `monsters/pitwarden.py` | 1 | 7k / 3k |
| **Total (W1 + W2 Chapter 1)** | | | | **~29** | |

The shipped goblin, chief, shaman and slime FBX stay in the repository (the web-exported Chapters 2-5 data and tests may
still reference their game ids); the remake's Chapter 1 manifests point at the new assets.

## 6. Chapters 2-5 (outline)

Each later chapter reuses the Chapter 1 human kit for its living humans and its own dead, and adds one new rig family.
Existing data ids are slots; the remake re-casts them.

### Chapter 2 · 缢林 / The Hanging Wood (`twilight_forest`, Lv 10-20)

| Slot (data id) | Remake family / role | Notes | Rig |
|---|---|---|---|
| `skeleton` L8 | 缢亡者 / the Hanged: dormant ambushers that drop from branches | always executed victims, shrouded and rope-bound (never bare skeletons); the 「找替身」 rule: they seek a substitute | SKEL_Human (Unburied kit) |
| `zombie` L10 | 沉沼尸 / the Fenbound: bog-drowned, slow | poison puddle on death | SKEL_Human |
| `werewolf` L13 | 月病者 / the Moon-Sick: villagers mid-turn, lunging | the turn shown in posture and hands, not gore | SKEL_Human lean variant |
| (new) | 食月犬 / Moon-hounds: pack hunters | first quadruped; pack aggro, flanking | **SKEL_Quadruped** (new) |
| (new) | 执灯人残部 / Lamp-Bearer remnants: lantern and net throwers | root / slow; living | SKEL_Human |
| `shadow_weaver` (mini-boss) | 缢林领唱 / the Cantor of the Hanged (Brother Aldwin) | raises the Hanged; a chant aura; kites | SKEL_Human (NPC look turned) |
| `werewolf_alpha` (boss) | 噬月者·沃尔甘 / Volgan the Moon-Eater | P1 chained to the gallows-oak (chain sweeps, limited radius); P2 breaks the chain, howl summons hounds; P3 eclipse: the arena darkens to the hero's light radius | SKEL_Quadruped (large) |
| hunts | 灰鬃 Greymane, 裂颚 Riftjaw, 缚绳人 the Rope-Binder, 沼母 the Fen-Mother | re-voiced | — |

### Chapter 3 · 锁砧山 / The Chained Anvil (`anvil_mountains`, Lv 20-30)

| Slot | Remake | Notes | Rig |
|---|---|---|---|
| (new) | 锁链工 / the Chained: dead smiths and miners still working | hammers and chain-flails; Work idles; a chain-pull | SKEL_Human (short dwarf spec) |
| `gargoyle` | 誓石像 / Oathstone gargoyles | perched dormant statues; hunt anyone leaving the mountain | **SKEL_Winged** |
| `stone_golem` | 渣魔 / slag golems | one huge wind-up slam, molten puddles | **SKEL_Large** |
| (new) | 征调营残兵 / requisition guards | the crusade's living rearguard; shield walls | SKEL_Human |
| `iron_guardian` (mini-boss) | 铁甲守卫 / the Iron Guardian | frontal block, gate-lock (seals the room until it falls) | SKEL_Large |
| `mountain_troll` (boss) | 戈尔姆 / Gorm in the dead king's chains | P1 chain pull; P2 throne slam with cave-in hazards; P3 the chains whisper (adds of the Chained) | SKEL_Large |

### Chapter 4 · 盐葬海 / The Salt Pyre (`scorching_desert`, Lv 30-40)

| Slot | Remake | Notes | Rig |
|---|---|---|---|
| (new) | 盐殓者 / the Embalmed | salt-statued dead (never bone); a salt crust that cracks off as armour | SKEL_Human |
| `fire_elemental` | 焚祭魂 / pyre-souls | the sacrificed, ranged burn; tragic, not demonic | SKEL_Pet_Sprite-like hover or SKEL_Human robe |
| `desert_scorpion` | 盐蝎 / salt scorpions | sting slow, burrow | arthropod variant of SKEL_Quadruped |
| `sandworm` | 吞路虫 / the Road-Eater | burrow telegraph lines, emerge slam | **SKEL_Serpent** |
| (new) | 日冠信徒 / sun-cultists | casters who draw lots | SKEL_Human |
| `sand_wraith` (mini-boss) | 不死大祭司 / the Deathless High Priest | the one who broke the Sun Seal | SKEL_Human |
| `phoenix` (boss) | 赫莉娅 / Helia, chained to the pyre | P1 chained (pyre-ring hazards); P2 one chain breaks (dive lines); P3 rebirth flame as an interruptible heal channel (break the pyre anchors; no invulnerability). Freed, she grants `pet_phoenix` | SKEL_Winged (large) |

### Chapter 5 · 渊之创口 / The Open Wound (`abyss_rift`, Lv 40-50)

| Slot | Remake | Notes | Rig |
|---|---|---|---|
| (new) | 渊骑 / breach-knights | crusaders fused with abyssal plate; shield and charge | SKEL_Human |
| `imp` | 裂蛆 / rift-grubs | burrowing swarmers that burst; deliberately not small red imps | SKEL_Serpent (small) |
| `lesser_demon` | 裂角兽 / riftborn brutes | bruisers that brand the hero | SKEL_Large |
| `succubus` | 哀泣者 / the Keening | eyeless robed wailing spirits, ranged slow; deliberately non-sexualised | SKEL_Human robe |
| `void_herald` (mini-boss) | 守焰圣主 / the Hierarch | the first signatory; summons the remade host | SKEL_Human |
| `demon_lord` (boss) | 渊火化身·伊格纳罗斯 / Ignaroth, Abyssfire Incarnate | `bossSkills` become real: 焚誓之雨 hellfire (falling breach-fire circles), 暗星 dark nova (an expanding ring), 召唤铸废 summon (the remade host) | SKEL_Large |

### The Bottomless Stair (Labyrinth)

Floor themes reuse each chapter's families; gatekeepers are re-dressed per theme; the eight curses are re-voiced as
oaths (`volatile` → 焚誓 Pyre-Oath, using the on-death burst at `deathBurst` .12); 卡萨诺尔 stays the lord of the deepest
dark. No new data until the Labyrinth runtime exists.

### New rig families (cost per first member, from the Chapter 1 history)

| Rig | First needed | First member cost | Later members |
|---|---|---|---|
| SKEL_Quadruped | Ch2 Moon-hounds | ~5-6 tasks (skeleton, clip library, LOD / budget, voices, UE single-view check) | ~2 |
| SKEL_Large | Ch3 slag golem | ~5-6 | ~2-3 |
| SKEL_Winged | Ch3 gargoyle | ~5-6 | ~2-3 |
| SKEL_Serpent | Ch4 road-eater | ~5-6 | ~2 |
