# 焚境 · The Burning March: the dark remake of Abyssfire's quests and monsters

> Status: design, approved for implementation planning (2026-10). Scope: the Unreal remake only (`unreal/`); the web
> game under `src/` is not touched. Player-facing text is zh-CN first, English second.

| Document | What it holds |
|---|---|
| [story-bible.md](story-bible.md) | World, history, the corruption, factions, antagonists, the hero, the five-chapter descent, recurring characters, lore delivery, the zh-CN / en voice guide, what to avoid |
| [chapter1.md](chapter1.md) | Chapter 1 in implementable detail: hub, NPCs, every quest with objectives and lines, story beats, lore, events, sub-area, mini-boss and boss fights, mood, map and spawn changes |
| [monsters.md](monsters.md) | The full roster: Chapter 1 monsters in detail (stats, behaviour, telegraphs, on-death, packs, spawns, sound, 3D art brief), chapters 2-5 families in outline |
| [implementation-plan.md](implementation-plan.md) | How to build it: content ownership, core features with specs and tests, AutoPilot, art, audio, work packages with disjoint file ownership and acceptance checks |

## Logline

四十天前，一场圣战死在了东境。赤幡原喝下了太多的血，地底的封印裂开了。战死的大军仍在执行最后的军令——死守；食腐的东西在尸野上分肉；裂口深处，有东西在听。你从丧钟堡外的焚尸坑里爬出来，掌心烙着五焰一空。每到黄昏，那口钟就为当天数到的每一个名字敲一下。

Forty days ago a holy war died on the eastern border. The Red Field drank so much blood that the seal beneath it
cracked. The slain army still obeys its last order, *hold*; carrion things pick the field clean; and deep in the
breach, something listens. You crawl out of the burn pit outside Knellwatch, the crusade's last fort, with five flames
around an empty heart branded on your palm. Every dusk the fort's bell tolls once for every name counted that day.

The story bible's spine is unchanged: the Abyssfire, the six keepers, Ignaroth the Oathburner (焚誓者·伊格纳罗斯)
whispering to those who fear the end, the five seals he calls "lamps", the brand, the twist that the relit flames are
his key, and the ending where the hero gives the Heartflame back. What changes is who the player fights, what a broken
seal does to ordinary people, and how the story is told.

## Tone pillars

1. **Horror of the familiar.** The enemies wore the same tabard as the people you protect. Chapter 1's first kills
   are the burial detail that walked out at dusk with one lantern.
2. **Every name counts.** Loss is counted, named and tolled for. The villain is the one whose name was chiselled
   away: 「他们为每一个名字敲钟。从来没有人，为我敲过。」 (They toll a bell for every name. No one ever tolled one
   for me.)
3. **Oaths that will not let go.** The Oathburner's power makes other people's oaths eternal. The dead keep their last
   order, zealots keep their blood-oath, deserters are hunted by the oath they broke.
4. **Darkness you can read.** A dark, desaturated world with readable actors: ink outline and rim kept, one marker
   colour per enemy family, and one reserved enemy-hazard language that no hero effect uses.
5. **Warmth is rare and earned.** The hub hearth, the Ember Tower and the ley-beasts are deliberately warm; they are the
   counterweight that makes the march read as dark.
6. **Aftermath, not atrocity; ash, not gore.** Suffering is shown after the fact. Ash husks are the default for every
   locale (zh-CN first); blood is an optional layer.

## What changes for the player

| Today (Chapter 1, 翡翠平原) | Dark remake (Chapter 1, 赤幡原 / The Red Field) |
|---|---|
| A sunny village, an elder, cosy errands (herbs, slime gel, a pendant thief, hide-and-seek with a sprite) | Knellwatch (丧钟堡), the crusade's last fort; a captain who ordered the retreat that saved two hundred, a chaplain who still prays, a field surgeon, a deserter. Every quest is a plea, an order or a dying word |
| Slimes, goblins, a goblin shaman, a goblin chief with 160 HP | Four families: the Unburied (risen crusaders who still hold the line), the Living (Red-Oath zealots, deserters), the Gravegnawers (carrion things), Breach-blood (clots and exploding bloats). A corpse-herding mini-boss and Lord-Marshal Valdras, a three-phase boss |
| One melee or bolt attack per monster, a fixed wind-up tint | Telegraphed ground attacks in a reserved hazard language, raisers and corpse-eaters, exploders with fuses, frenzy, shield walls, packs that break when their leader falls (Wave 2) |
| A bright plains grade, pollen, fruit trees | Ash-grey march under falling ash, dead orchards, banners on stakes; the only warm light is the hub hearth; a violet uplight from the breach |
| A sunlit chapter finale | A dawn siege at the Boneyard Gate, then the bell tolls and every risen dead on the field lies down at once |
| No descent | The Pit of the Faithful under the burn pit where you woke: a dark cleanse that lights up when cleared (Wave 3) |

The hero, classes, skills, loot, the Ember Tower and pets are unchanged in function. The Lanternwisp (灯魂, the
re-dressed `pet_sprite`) stays warm on purpose.

## How it was chosen

Three directions were pitched and scored by three judges (tone and IP, feasibility, player experience).

| Pitch | Premise | Judges' picks |
|---|---|---|
| A, 长明之下 / Beneath the Everlight | Plague town under an abbey that burns the dead as lamp-oil | Feasibility judge |
| **B, 焚境 / The Burning March** | A broken holy war; oath-bound dead; a fort that tolls for every name | **Tone judge, player-experience judge** |
| C, 溺契 / The Tithe Below | A fen village that tithes drowned strangers to a voice under the mere | none |

B wins on Diablo feel without copying Diablo's premise, on brand fit (oaths made eternal is the Oathburner's own
power; the bell for every name answers the erased name), on the zh-CN voice, on its boss lesson chain and on Chapter 2-5
scaling. Its weaknesses were fixed with grafts from A and C (listed in the decisions below and in each document).

## Decisions taken

| ID | Decision |
|---|---|
| DR1 | **Direction B, "The Burning March", with grafts.** From A: the hero wakes in a burn pit, so 「灰烬中醒来」 and the prophecy stay literal; the finale image of every risen dead lying down at once; saved world-state changes; trade-miming idles; the boss's Feed counterplay; a shield-wall role; the mini-boss on the main path. From C: hue discipline and one marker colour per family; packs that break unless a sustaining role lives; labelled moral reward choices; quest-gated NPC lines; a diegetic toll before ambushes; the hooded-shaman trick that makes the mini-boss a grieving human. |
| DR2 | **The remake owns its content.** UE-owned TypeScript modules under `unreal/GameData/` replace the web content modules through a module-redirect Vite plugin in the exporter. `unreal/Data/*.json` stays generated and `--check`-able. The web game stays untouched. |
| DR3 | **New remake ids** for quests (`q_rf_*`), monsters, cutscenes (`cs_rf_*`), lore (`lore_rf_*`), hidden area and story decorations. **NPC ids are kept** as role keys (`quest_elder`, `blacksmith`, `merchant`, `stash`, `plains_herbalist`, `plains_wanderer`); new NPCs get new ids (`knell_chaplain`, `crow_trader`). Zone ids are kept (`emerald_plains` displays as 赤幡原). Dev saves reset Chapter 1 progress on the content-version bump. |
| DR4 | **One planned golden-map re-baseline**, inside the content switch: M5's two explore landmarks become one defend target, the Pit entrance and three echo lore spots are added. Every other generator input keeps its tile, margin and order. |
| DR5 | **Hazard language.** Enemy ground telegraphs are decals with a warning-crimson rim (#D11F2E) and a bone-white fill (#FFE8DC) that sweeps to full at the moment of contact; no hero effect draws a filling ground shape. Ember orange (#FF7A2A family) is reserved for the hero, the Abyssfire, loot and refuges, and is never an enemy emissive. Enemy corruption ("breach fire") is breach-violet #B02FD0 around near-black cores, kept ≥ 30° of hue from the mage's mana shield (#8A5CFF). An automated hue audit enforces it. |
| DR6 | **Ash mode is the default in every locale**: corpses crumble to ash husks, breach-blood reads as tar and ash, no exposed skeletons in Chapter 1. An optional gore layer (blood decals, persistent corpses) is off by default and absent from zh-CN builds. |
| DR7 | **The descent is the Pit of the Faithful**, an optional Den-of-Evil-style cleanse with a permanent reward. The act boss holds the surface Breach Rim because holding the line is who he is; the chapter's critical path never waits on sub-dungeon travel. Later chapters put their act bosses at the bottom of their descents once that runtime exists. |
| DR8 | **Three waves.** Wave 1: a dark, fully playable Chapter 1 on today's core plus small content hooks; every monster has a stated today-core fallback. Wave 2: the monster systems that carry the Diablo feel (abilities and telegraphs, on-death effects, corpses, boss phases, packs and morale, kite and block). Wave 3: the descent (sub-dungeon travel, the Pit, light-radius play, cleansed grade). Art and audio run in parallel with the core waves. |
| DR9 | **Balance anchors.** Chapter 1 quest rewards stay near today's 2660 exp / 505 gold (Wave 1: 2740 / 510), the population stays 55, so the playthrough bands (hero level 12-14, embers ≥ 40, kills ≥ 40) hold. Raisers are capped; exploders have fuses of at least 800 ms and capped damage; no single enemy hit exceeds 35 % of a same-level hero's maximum life on Normal; there is no invulnerable boss phase. |
| DR10 | **Original content only.** Diablo's tone and structure inspire; its names, characters, places, plots and iconic designs never appear (list in the story bible). No real-world religious iconography; the Church of the Kept Flame's sigil is an open palm cradling a flame. No plague, quarantine or lockdown imagery (doors nailed from outside, cordons, "we are not sick" pleas). |
| DR11 | **Chapters 2-5 are redesigned at outline level now** (story bible, monsters.md) and written in full when each chapter is built in 3D. Their web-exported data stays as is until then; the sealed east gate (W7) stays closed until Chapter 2's dark pass ships. |
