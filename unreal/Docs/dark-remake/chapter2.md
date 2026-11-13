# Chapter 2 · 被抹去的名字 · 绳林 / The Erased Name · The Hanging Wood

> Tagline: 绳不空悬 / No Rope Hangs Empty. Zone id `twilight_forest` (display 绳林; working name 缢林 in the story
> bible, see §22 O1), 120 × 120, Lv 13-20, the Phase 4 counterpart of [chapter1.md](chapter1.md). Status: **design
> spec, revision 2, nothing built** (2026-11). Revision 2 applies a three-part review (buildability against the core as
> it stands, story and zh-CN voice, fight and pacing); what changed and what was not taken is in §24. The spec names
> **eight core requests** (§20, **[R1]**-**[R8]**), each with a fallback except [R1], [R7] and [R8]. Tiles marked
> *provisional* were checked for walkability on today's golden grid (`CoreTests/golden/maps/twilight_forest.json`), not
> on the re-baselined one (§14). No number in this document has been run through the core.

## 0. At a glance

| | Web (today, behind the sealed gate) | Remake |
|---|---|---|
| Hub | a scouts' camp | 空磨坊 the Hollow Mill, refugees round one fire |
| Main chain | 永夜之林 → 不安的亡者 → 被抹去的名字 → 噬月之狼 → 月辉之印 | 绳道 → 空绳 → 被抹去的名字 → 噬月者 → 月辉之印 |
| Mini-boss | 暗影织者 (ranged elite, 520 HP) | 绳林领唱 the Cantor of the Hanged: Brother Aldwin, turned (raiser, aura, ring), on the main path (M3) |
| Act boss | 狼人首领, 650 HP, one phase | 噬月者·沃尔甘 Volgan the Moon-Eater: the chain, the pack, the eclipse; the chapter's climax fight (M4) |
| Finale | explore the dark source, kill 15 zombies | a short descent: the Root Barrow, its named keeper, the Moon Seal relit, the walk home under moonlight |
| Descent | none | 根冢 the Root Barrow (`sub_hw_barrow`), hand-authored, three rooms, on the main path (M5) |
| Quests | 13 (5 main, 8 side), 14700 exp / 1255 gold | 13 (5 main, 8 side; S7 is a prerequisite of M3), **5420 exp** / 1255 gold (§7, §18) |
| Population | 56 in 10 spawn entries | 56 in the same 10 entries, every entry on a quest's path (§14) |
| New rig family | — | SKEL_Quadruped: `moon_hound` is its first member (§4.4, §17) |

### Id map (slot → remake id)

Quest ids are **kept** (they are referenced by `homestead.ts`, the story triggers and the scout's dialogue tree, and no
save holds Chapter 2 progress: the gate was never open). Cutscene ids the story bible names are kept; monsters, hunts,
lore, the hidden area and story decorations get new ids, as in Chapter 1 (DECISIONS DR3). New ids use the prefix `hw`.

| Quest id (kept) | Name | Type | Listed on (turn-in) | Offered by |
|---|---|---|---|---|
| `q_explore_forest` | 绳道 / The Rope Road | explore | `quest_scout` | Gret's tree |
| `q_kill_undead` | 空绳 / Empty Ropes | kill | `quest_scout` | Gret's tree |
| `q_talk_hermit` | 被抹去的名字 / The Erased Name | talk | `forest_hermit` (moved) | Gret's tree, or Bran |
| `q_kill_werewolf_alpha` | 噬月者 / The Moon-Eater | kill | `forest_hermit` (moved) | Bran |
| `q_seal_dark_source` | 月辉之印 / The Moon Seal | kill (sub-dungeon) | `quest_scout` | Bran's tree, or Gret |
| `q_collect_wolf_pelts` | 悬赏：灰鬃 / Bounty: Ashcoat | kill (hunt) | `forest_tracker` | Gret's tree, or Hedda |
| `q_lost_scout` | 没回来的人 / The One Who Walked Out | investigate | `forest_tracker` | Gret's tree, or Hedda |
| `q_spider_nest` | 空坟 / Empty Graves | collect | `forest_spirit_medium` | Nora |
| `q_ancient_relic` | 塔姆 / Tam | talk | `quest_scout` (moved from `forest_spirit_medium`) | Gret's tree |
| `q_pet_owl` | 月鸮 / The Moon-Owl | collect | `forest_hermit` | Bran |
| `q_pet_cat` | 影猫 / The Shadow Cat | collect + kill | `forest_spirit_medium` | Nora |
| `q_defend_camp_forest` | 割绳之夜 / The Cutting Night | defend | `quest_scout` | Gret |
| `q_investigate_corruption_forest` | 井水 / Well-Water | investigate | `forest_spirit_medium` | Nora |

A dialogue tree may offer a quest that another NPC lists; the web tree already does it (`q_lost_scout` is listed on
`forest_tracker` and triggered from the scout's tree). The quest is turned in to the NPC that lists it.

| Old monster / beat | Remake |
|---|---|
| `skeleton` | `hanged_one` |
| `zombie` | `fenbound` |
| `werewolf` | `moonsick`; new `moon_hound`, `hound_packleader` (SKEL_Quadruped) |
| (new) | `lamp_netter`, `lamp_warden` (the Lamp-Bearer remnants) |
| `werewolf_alpha` | `volgan` (storyBoss) |
| `miniboss_shadow_weaver` | `miniboss_cantor` |
| (new) | `sub_rootbound` (the Root Barrow's keeper, Wystan) |
| `hunt_greymane` / `hunt_scout_ghoul` / `hunt_shade_binder` / `hunt_rot_mother` | `hunt_greymane` (id kept; en display "Ashcoat") / `hunt_riftjaw` / cut / cut (§4.0) |
| `cs_tf_hermit` / `cs_tf_finale` / `cs_tf_moonfang` | kept ids, rewritten |
| `cs_boss_werewolf_alpha` | `cs_boss_volgan`; new `cs_hw_arrival`, `cs_hw_empty_rope`, `cs_hw_dream`, `cs_hw_cantor`, `cs_hw_barrow`, `cs_hw_seal` [R2] |
| `lore_tf_01..04`, `hidden_tf_moonlight_shrine`, `story_tf_*` | `lore_hw_01..04` + `lore_hw_echo_01..03`, `hidden_hw_moon_shrine`, `story_hw_lantern_row` / `story_hw_dry_font` / `story_hw_first_stump`; `story_hw_ledger` in the Barrow |
| NPC role keys `quest_scout`, `forest_hermit`, `forest_tracker`, `forest_spirit_medium`, `tower_hermit` | kept, re-cast (§3); new `mill_smith`, `mill_trader`, `mill_stash`, `mill_wick`, `crow_carter` |

The Labyrinth's `crypt` floor theme (`src/data/abyssRun.ts` `FLOOR_THEMES`, web code that is not redirected) names
`skeleton`, `zombie`, `werewolf` and `werewolf_alpha`. The content package adds an exporter override for that row
(`REMAKE_OVERRIDES.abyssRun.floorThemes.crypt = {monsters: [hanged_one, fenbound, moonsick], gatekeeper:
hound_packleader}`); without it the export fails on unknown ids. The Labyrinth runtime is still not built, so this is
data hygiene only. That the override hook exists in the exporter is **unverified** (4A's first task).

## 1. Premise and tone

**What the place is.** East of the Red Field the crusade's road runs into the moon-oaks. For centuries the church's
witch-finders, the **Lamp-Bearers** (执灯人), hanged people here: first Selenne's last moon-priestesses, then anyone who
could be called one (midwives, herb-gatherers, women out at night). The Moon Seal lies in the roots of the greatest
oak. The first of the Lamp-Bearers, a man who feared death, fused with that tree, and since then **the wood does not
let its dead go**.

**The wood's rule, stated once** (every line in the chapter agrees with this wording):

> 绳不空悬。 **No rope hangs empty.** A rope that is cut pulls its dead back by morning. It lets go for good only when
> someone else is put in the place.

Three things follow, and nothing else does:

1. The dead on the ropes drop on travellers. They do not hate the living; a traveller pulled down under a rope is the
   only way that rope lets its own dead go. They are killed by the wood's dead, never by their own hand (§1 tone rules).
2. Gret's nightly cutting buys each of them **one night on the ground**. By morning the rope has them back. She knows.
3. When many ropes are cut in one night (S7) the wood sends whatever it has to fill them before morning.

The chapter's horror is that the rule is exact, and that the hub's best person spends every night fighting it with a
hook and a ladder for the sake of one night's rest for strangers.

**What the player learns.**

1. **The moon-priestesses.** Selenne's order kept the Moon Seal as a well of still water under the Mother Oak (母橡)
   and kept a sacred wolf, Volgan. They are known only through what is left: an echo, a dry font, a shrine with a
   scraped stone.
2. **The Lamp-Bearers.** The church does not know that "lamp" (灯) is Ignaroth's word for a seal. Its witch-finders
   carried lanterns to "show what is hidden", chained the wolf under the gallows and fed him. A remnant still hunts the
   wood. One of them put his lantern out thirty years ago: Bran, the chapter's "hermit".
3. **The erased name.** The prologue already told the player that there were six keepers and that the sixth's name was
   chiselled away. What this chapter adds is **the name, and where the church's scripture comes from**. The stone with
   the six lines stands at the Moon Shrine (`lore_hw_02`), older than the church. The Lamp-Bearers swear on a
   **rubbing** of it (六行拓本; the sixth line is a blank). The last moon-priestess wrote one name on the back of that
   rubbing before she was taken to the tree, and every copyist since has traced it without daring to read it. Bran was
   the last copyist. He read it, and put his lamp out that night. `cs_tf_hermit` (kept id): the name is Ignaroth, and
   the voice that dictated the scripture uses his words ("lamp", "child"). Chapter 4 later proves it in the high
   priest's own hand; here it is a dead woman's note and the word of a man who hanged people by that scripture.

**How it ties to Chapter 1's end.** `cs_rf_finale` ends with the Captain's 「东边的林子里，还有活人在等消息」 and with
Brother Aldwin's 「我会一直听着它」. Aldwin left Knellwatch with the first wagons through the opened gate, "to hear it
clearly", and reached the wood ahead of the hero. The voice told him that no name is forgotten if no one is ever let
go, and he became the Cantor of the Hanged: he raises them and leads them in a slow chant. The Chapter 1 raiser lesson
returns, now with a face the player knows. What this does to Aldwin in Knellwatch is §3.4.

**The sealed east gate (W7).** In Chapter 1 the east exit (119,60) is the 辎重垒, a barricade of wagons with breach
fire beyond. This chapter's ship date is what opens it, and the opening is tied to the story: the gate opens only once
`rf_chapter_dawn` is set (M5 `q_rf_last_toll` turned in: the breach fire has drawn back and Knellwatch drags the wagons
apart). Today the sealed gate is one config row applied only under `milestone1`; [R1] makes it a list with a condition
and says what `milestone1` means in Phase 4. The hero arrives at (3,58) on the Rope Road. The exit from this zone to
`anvil_mountains` (119,119) becomes the new unconditional sealed gate until Chapter 3's pass
(`zone.exit.sealedChapter3`).

**Tone rules for this chapter** (on top of story-bible.md §11-12; the content test and the art review check them):

- **Hanging is aftermath only.** No one is hanged, hoisted or pulled upward on screen, in a cutscene, in an animation
  or as a mechanic. The Hanged only ever move **down** (they drop). Nothing lifts or holds the hero. A re-hung bundle
  is never seen going up: it is back when the hero is far away (§14, [R4]).
- **Bundles, not bodies.** Every hung figure is a shroud-wrapped, rope-bound bundle: no face, no bare limbs, no neck,
  the rope bound round the whole wrapping. No empty noose appears anywhere in the zone; where a rope has been cut the
  dressing shows a frayed end on the ground.
- **Executed by others, always.** The moon-priestesses and later victims were executed by the Lamp-Bearers; the people
  the wood takes are taken by its dead. Nothing in text or image suggests self-harm. The Chinese folk idiom about a
  hanged ghost seeking a substitute is **not quoted** (to a native reader it is a self-harm story), the Moon-Sick walk
  **toward water**, never "into the wood", and the zh-CN names avoid 缢 (§22 O1). A clue that read as a self-harm
  image was replaced (§6 S2).
- **No quarantine imagery** (story-bible.md §12). Nobody is kept in. The mill's doors are barred from inside by the
  people who sleep there (the Ashwheat pattern); there is no rack of bars under watch and no covered well. The hostile
  Moon-Sick are the long-turned of an emptied hamlet: they know no face and speak no word (§4.4). The mill's newly
  sick are never targets and are not shown as figures.
- **Ash mode** (DR6). The Hanged collapse into an empty, ash-filled shroud. No exposed bone (the web `skeleton`
  becomes a wrapped figure; `story_hw_lantern_row` keeps the web `spriteType` `skeletal_remains` and resolves to
  lantern poles under the `hangwood` theme, §12). Fenbound sink into the bog. The feeding ground under the oak is torn
  cloth and a worn hollow in the earth; what the wolf was fed is told, never shown.
- **No children.** The Moon-Sick and every hung figure are adults. Riftjaw is the miller's grown son.
- **No real iconography.** The moon-priestesses' sign is the **Well-Ring** (井环), a plain ring of beaten silver. The
  rule is *no crescent used as a religious emblem*: no crescent on an altar, banner, collar, seal or vestment. Natural
  moon shapes are allowed and listed: the moon itself, the phases cut on the puzzle stones (§13), the pale mark on the
  cub's brow. Lamp-Bearers carry a lantern on a pole and a catch-net; no pointed hats, crosses or pyres.
- **Avoid list.** Never "the Drowned" or "Coven" as a name (Fenbound, the order); never "Fallen" in English text.
  Proposed additions to story-bible.md §12 (4G): "Greymane", "Moonfang", "Duskwood", "worgen" (wolf names and a wolf
  wood of a well-known online game; the bible lists only action-RPG titles today). The kept ids `hunt_greymane` and
  `cs_tf_moonfang` never reach the player; the English display names are **Ashcoat** and **Sliver**.

## 2. The zone: a gradient from the mill to the oak

West is the mill and the dead who only wait; the east and south-east are the Lamp-Bearers' ground and the oak. One
mechanic arrives per area, in the order the route meets them (§4.7). Tiles are the existing grid's; **(a → b)** means
the spawn entry keeps its generator tile *a* and stands at *b* through the spawn's `at` (feature `map.spawnAt`).

| Area | zh | Tile | Lv | What is there |
|---|---|---|---|---|
| The Rope Road | 绳道 | playerStart (3,58); west exit (0,58) → `emerald_plains` (118,60) | — | the road from the Red Field; the first bundles on the boughs (set dressing; no hostile within 12 tiles of the start) |
| The Hollow Mill (hub) | 空磨坊 | camp 1 (18,52), safe r9 | — | the fire, the stopped wheel, forty refugees |
| The First Stump | 头桩 | story decoration (15,75) | — | the oak the miller's grandfather cut down so no one could be hung from it |
| The Sunk Wheel | 沉轮沼 | spawn (15,45 → 14,40) | 13 | Fenbound in the mill-pond; 12.6 tiles from the camp centre, 10.2 from S3's gather centre |
| The Empty Graves | 空坟场 | explore (10,29) r10; gather area (12,30) r8 | — | graves dug for people who cannot be kept down; S3's rope-ends; S8 clue (20,25) |
| The Low Boughs | 低枝林 | spawn (25,15); explore (26,10) **r4**; echo (27,9) | 14 | the first Hanged: eight bundles over the path, which drop as one (M1) |
| The Middle Boughs | 中枝林 | spawn (80,18 → 38,22) | 14 | six more, hung in a line across the lantern road (M2) |
| The Cutting Oak | 割绳橡 | defend target (40,35) | — | where Gret works at night; S7 |
| The Lantern Row | 灯道 | explore (44,27) **r5**; story decoration (48,28) | — | seven lit poles; the chant is heard from here. 11.4 tiles from the Cantor |
| The Chantry of Lamps | 灯祠 | mini-boss (55,30); echo (57,28) | 17 | a ruined Lamp-Bearer chapel; the Cantor; six bundles laid in a row (corpse seeds) |
| Mosswick | 苔井村 | spawn (40,40 → 46,48) | 16 | a hamlet that emptied two winters ago; the long-turned Moon-Sick; 14.3 tiles from the Cutting Oak |
| The Mill-Stream ford | 浅滩 | S8 clue (50,40) | — | the stream runs silver-green |
| Hedda's blind | 守犬人的窝棚 | field NPC (60,50) | — | the Houndward |
| The Lamp Line | 巡灯线 | spawn (95,40 → 70,50) | 16-17 | a Lamp-Bearer patrol on the road from the ford to the Dark Lantern: a warden and his netters (M3's walk, S6) |
| The Writ Stone | 执灯令碑 | lore (50,65) | — | the order that made the wood |
| The Wolf Run | 狼径 | spawn (30,70); S2 clues (30,60) (36,70) (44,76); Riftjaw (48,79) | 15-17 | the first hound pack and its leader |
| Nora's shroud-yard | 殓布场 | field NPC (35,85); S8's spring (30,90) | — | the Shroud-Wife; lines of drying shrouds |
| Ashcoat's Den | 灰鬃窝 | spawn (20,95); hunt (22,96) | 15-17 | the second hound pack, two leaders; S1 |
| The Owl Wood | 鸮林 | gather (86,50) r8; clue (80,44) | — | S5 |
| The Dry Font | 枯月盆 | story decoration (72,60) | — | a moon-font with the ring scraped off |
| The Dark Lantern | 熄灯庐 | camp 3 (100,62); lore (96,58) | — | Bran's hut; one unlit lantern on the door; his master's notes |
| The Chain Watch | 守链岗 | spawn (100,65 → 97,74) | 16-17 | the Lamp-Bearers who still feed the wolf, on the road from the Dark Lantern to the oak (M4); 12.4 tiles from camp 3, 17.5 from the dais |
| The Kennel Ruins | 犬舍废墟 | spawn (85,80 → 78,74); echo (76,84) | 15-16 | the temple kennels: Moon-Sick and loose hounds; 20 tiles from the oak |
| The Gallows-Oak | 绞架橡 | storyBoss (90,90); lore (80,89) | 19 | the Mother Oak, the chain, Volgan |
| The Barrow mouth | 根口 | entrance (79,95), return (80,96) | — | where a root as thick as a cart breaks the ground; 12.1 tiles from the dais |
| The Moon Shrine | 月祠 | hidden area (110,15) r5; hidden lore (109,24) | — | the priestesses' shrine and the Scraped Stone (optional; nothing guards it) |
| The Crow Carts | 鸦车 | camp 2 (105,100), **safe r4** | — | a Gleaner cart sent ahead; 18.0 tiles from the dais |
| East road | 东山路 | exit (119,119) → `anvil_mountains` | — | sealed until Chapter 3's pass |

**The route** (the bot's; steps and every turn-in in §19): mill → the three places of M1 (the first drop) → mill → M2
at the Sunk Wheel and the Middle Boughs → mill → S7 at the Cutting Oak → mill → the south loop (the Wolf Run, S2,
Riftjaw, Nora, Ashcoat, back by the mill) → the Empty Graves (S3, S8) → the Chantry (the Cantor) → the ford → Hedda →
the Lamp Line → the Dark Lantern (M3 turned in to Bran) → the Owl Wood (S5) → the Chain Watch → Volgan → Bran (M4 in,
M5 taken) → the Root Barrow → **the walk home**: west through a wood where the ropes have let go, by the Kennel Ruins
and Nora, to the mill and the finale.

## 3. Hub and NPCs

### 3.1 空磨坊 / the Hollow Mill (camp 1, (18,52))

A water-mill at the wood's edge. The wheel stopped when the stream went silver-green; the miller has taken the iron
hoops off his stones to make blades. Forty refugees sleep on the grinding floor round one fire and bar the door from
inside themselves. Every night someone counts heads. Some nights one of them gets up and walks for the water, and
Gret's people walk them back.

**Dressing** (camp layout v2 slots re-pointed by the presentation theme `hangwood`, same footprints; extra pieces
through the set-piece layer):

| Camp slot / piece | Remake look |
|---|---|
| palisade, gate posts | wattle hurdles and cart-sides lashed between the mill's fence posts; no rope anywhere on the fence (the refugees burned it) |
| tent | the **mill house**: a squat timber mill with the wheel stopped mid-turn |
| campfire | **the fire** on the threshing floor: the zone's first warm light (#FFB060) |
| banner | none (`PlacesCampProp` returns false for `camp_banner@hangwood`): the mill flies nothing |
| barrels, crates | meal sacks counted in chalk, a sieve rack |
| well | the mill's well with a bucket turned upside down on its lid (nobody draws from it; no cloth, no guard) |
| (set piece) the hook wall | Gret's cutting hooks and the ladder; forty-odd notches cut in the post (Tam's count) |
| (set piece) Aldwin's handbell | hung on the mill door `when {flag: hw_cantor_silenced}` [R5] |

**Life.** No children in the open (they are inside; ambient voices only). Barks: 「昨夜又有人往水边走。是葛蕾领回来的。」
(Someone walked for the water again last night. Gret led them back.) 「别数树上的。它们也在数你。」 (Don't count the ones
in the trees. They're counting you.) 「夜里那一声，是沃尔甘。别应。」 (That sound at night is Volgan. Don't answer it.)

**Other camps.** Camp 2, 鸦车 / the Crow Carts (105,100): one Gleaner cart, a cookfire, coils of salvaged rope nobody
will buy. Its `safeZoneRadius` is **4** (a cart and a fire, not a stockade): the circle's edge is 14.0 tiles from
Volgan's dais, outside his fight reach of 12.6 (§11), so no step inside his arena sends him home. Camp 3, 熄灯庐 / the
Dark Lantern (100,62): Bran's hut, a lectern, an unlit lantern on the door.

### 3.2 NPCs

Role-key ids are kept and re-cast; the three shared camp ids (`blacksmith`, `merchant`, `stash`) are Knellwatch's
people and stay there, so the mill gets its own. Camp lists (not generator inputs): camp 1 `['mill_smith',
'mill_trader', 'quest_scout', 'mill_stash', 'mill_wick']`, camp 2 `['crow_carter']`, camp 3 `['forest_hermit']`.

| Id | Name | Type | Where | Role | Look brief | Line (`data.npc.<id>.dialogue.0`) |
|---|---|---|---|---|---|---|
| `quest_scout` | 割绳人葛蕾 / Gret the Rope-Cutter | quest | camp 1 | lists M1, M2, M5, S4, S7; the 15-node tree | flour-grey smock, a hooked pole across her back, rope-burned palms, hair tied in a rag | 新来的？手给我看看。……烫过的。行，不是执灯的手。 / New here? Show me your hands. ...Burned. Fine. Not a Lamp-Bearer's hands. |
| `mill_smith` (new) | 磨坊主哈尔 / Harl the Miller | blacksmith | camp 1 | forge (the shared `blacksmith` shop table) | leather apron over a miller's coat, a millstone hoop half cut into blades | 石磨的铁箍，我一圈圈拆下来打刀。面是磨不成了。 / I'm cutting the hoops off my millstones for blades. There'll be no more flour. |
| `mill_trader` (new) | 粮婆贝丝 / Bess the Meal-Wife | merchant | camp 1 | wares (the shared `merchant` table) | a tally stick, a meal scoop on a cord | 按人头，一人一勺。别跟我讲价，跟你的肚子讲。 / One scoop each, by the head-count. Don't bargain with me. Bargain with your belly. |
| `mill_stash` (new) | 看仓的奥多 / Odo of the Bins | stash | camp 1 | stash (the empty meal bins) | a miller's man, keys on a thong | 面仓空了，正好放东西。走了的人留下的，我都没动。 / The bins are empty, so they'll hold your things. I haven't touched what the others left. |
| `mill_wick` (new) | 老韦 / Old Wick | quest (no quests) | camp 1; `visibleWhen {notFlag: hw_tam_cut}` | the price of S4's "cut" branch: a man the player has seen every visit | an old sack-counter, a stub of chalk behind his ear | 我数麻袋。数清楚了，夜里才睡得着。 / I count the sacks. When the count is right I can sleep. |
| `forest_hermit` | 熄灯人布兰 / Bran the Lampless | quest | camp 3 | lists M3, M4, S5; a 4-node tree that offers M5; the erased-name beat | an old Lamp-Bearer's black coat with the lamp-cord cut off, ink-stained fingers, an unlit lantern | 灯我不点了。点着的时候，我看见的都是该吊的人。 / I don't light it any more. When it burned, everyone I saw was someone to hang. |
| `forest_tracker` | 守犬人赫达 / Hedda the Houndward | quest | field (60,50) | lists S1, S2 | a kennel-keeper's quilted sleeve, a whistle of silver, a boar spear | 我家替月祭司养了九代狗，都是沃尔甘的种。现在，我替它们收尸。 / Nine generations of my family bred hounds for the moon-priestesses, all of Volgan's line. Now I bury them. |
| `forest_spirit_medium` | 殓婆诺拉 / Nora the Shroud-Wife | quest | field (35,85) | lists S3, S6, S8; S4 talk target | a shroud-sewer: bone needle, linen over one arm, one ear always turned to the trees | 把耳朵贴在绳上，听得见名字。没人问过他们叫什么——除了我。 / Put your ear to the rope and you hear a name. Nobody ever asked them theirs. Only me. |
| `crow_carter` (new) | 拾荒人乔里 / Jory of the Gleaners | merchant | camp 2 | Gleaner wares (Mag Crowe's shop table) | crow-feather collar like Mag's, coils of rope on the cart | 鸦嫂叫我先来探路。林子里捡得到的只有绳子——好绳子，没人要。 / Mag sent me ahead. All there is to glean in this wood is rope. Good rope. Nobody wants it. |
| `tower_hermit` | 割绳人葛蕾 / Gret (at the Ember Tower) | quest | tower | the Moon Well's keeper (`pet_house.allyNpc`; `spriteId` → `quest_scout`); not visible while the tower is hidden ([R1]) | as Gret, with a well-bucket | 井里有月亮。这里的绳子，只用来打水。 / There's a moon in the well. Here, rope is only for drawing water. |
| rescue event | 网里的人 / the Netted Traveller | event | random | rescue | a traveller under a weighted net | 他们说，夜里赶路的，都是去拜月的。 / They said anyone on the road at night is going to pray to the moon. |
| wandering merchant | 拾荒人的车 / a Gleaner's cart | event | random | merchant | a hand-cart, boots and rope | 鸦集的价。路远，加一成。 / Crowmarket prices. Long road, one tenth more. |

Quest-gated lines (K6 `lines`; the table is grouped by NPC, not in authoring order: in data the more specific line
comes first, so Gret's four Tam lines stand before her bell line; a condition's keys hold together):

| NPC | When | zh-CN | en |
|---|---|---|---|
| `quest_scout` | `q_talk_hermit` turned in | 修士的铃，我挂在磨坊门上了。风一吹，还响。 | I hung the monk's bell on the mill door. It still rings when the wind blows. |
| `quest_scout` | `{flag: hw_tam_cut, flag: hw_barrow_cleansed}` [R3] | 塔姆早在土里了。老韦是昨夜才下来的。两个，我都记着。 | Tam has been in the ground a while. Old Wick only came down last night. I keep both of them. |
| `quest_scout` | `{flag: hw_tam_left, flag: hw_barrow_cleansed}` [R3] | 塔姆是最后一个下来的。我在底下接着。 | Tam was the last one down. I was underneath to take him. |
| `quest_scout` | `{flag: hw_tam_cut, notFlag: hw_barrow_cleansed}` [R3] | 他下来了，我亲手埋的。老韦是替他上去的。这笔账，记我头上。 | He's down. I buried him myself. Old Wick went up in his place. That one is on me. |
| `quest_scout` | `{flag: hw_tam_left, notFlag: hw_barrow_cleansed}` [R3] | 我每晚还是从那棵树下过。现在，我抬头了。 | I still walk under that tree every night. Now I look up. |
| `mill_trader` | `{flag: hw_tam_cut, notFlag: hw_barrow_cleansed}` [R3] | 今早点人头，少了老韦。铺盖是空的。葛蕾在割绳橡上认出了他。 | The count was one short this morning. Old Wick's bedroll was empty. Gret knew him in the Cutting Oak. |
| `mill_trader` | `{flag: hw_barrow_cleansed}` | 夜里往水边走的那几个，这两晚都睡在铺上了。 | The ones who kept walking for the water at night have slept in their beds these two nights. |
| `mill_smith` | `q_lost_scout` turned in | 他们说他在林子里走丢了。……我知道，这是好话。 | They say he got lost in the wood. ...I know that's the kind way to say it. |
| `forest_tracker` | `q_kill_werewolf_alpha` turned in | 沃尔甘不嚎了，狗散了一半。剩下的，我一条条去找。 | Volgan's quiet and half the pack has scattered. The rest I'll find one by one. |
| `forest_hermit` | `q_seal_dark_source` turned in | 那个名字，我抄了一遍，贴在门上了。写出来，就不那么怕了。 | I wrote the name out and put it on my door. Written down, it frightens me less. |
| `forest_spirit_medium` | `{flag: hw_barrow_cleansed}` | 绳上没有声音了。……太静，我睡不着。 | The ropes have gone quiet. ...Too quiet. I can't sleep. |

A condition object holds one `flag` key, so the two-flag rows are authored as `{flag: hw_tam_cut, any: [{flag:
hw_barrow_cleansed}]}` (the K6 `any` list with one leaf).

### 3.3 Dialogue trees

**Gret (`quest_scout`, 15 nodes).** New node ids; the web tree's three quest triggers and the one-time reward are
kept, and it also offers M3, S1 and S4.

| Node | zh-CN | en | Choices → |
|---|---|---|---|
| `root` | 新来的？手给我看看。……烫过的。行，不是执灯的手。 | New here? Show me your hands. ...Burned. Fine. Not a Lamp-Bearer's hands. | 你是谁？ Who are you? → `who` · 树上那些…… Those, in the trees... → `ropes` · 要我做什么？ What needs doing? → `work` · 有吃的吗？ Is there food? → `ration` · 告辞。 Farewell. → `farewell` |
| `who` | 葛蕾。白天磨面，夜里割绳子。别人叫我割绳人，随他们叫。 | Gret. I grind by day and cut rope by night. They call me the Rope-Cutter. Let them. | 割绳子？ Cut rope? → `cutting` · 回去 Back → `root` |
| `ropes` | 执灯人挂的。几百年了，林子一个都没放走。 | The Lamp-Bearers hung them. Hundreds of years, and the wood has never let one go. | 回去 Back → `root` |
| `cutting` | 割下来，裹好，埋了。天亮，他们又在树上。可我弟弟说过：割一个，算一个。 | Cut them down, wrap them, bury them. By morning they're back in the tree. But my brother used to say: one cut, one counted. | 你弟弟？ Your brother? (requires `q_kill_undead` turned in) → `tam` · 回去 Back → `root` |
| `tam` | 塔姆。割到第四十个，执灯人把他也吊上去了。……别问我是哪棵树。 | Tam. At his fortieth, the Lamp-Bearers hung him too. ...Don't ask me which tree. | 我替你去问。 I'll ask for you. → `accept_tam` · 回去 Back → `root` |
| `accept_tam` | 诺拉听得见绳子说话。布兰知道规矩。问完了，回来告诉我。 | Nora can hear the ropes. Bran knows the rule. Ask them, then come and tell me. | questTrigger `q_ancient_relic` |
| `work` | 先认路。低枝林、空坟场、灯道。活着回来，再说别的。 | Learn the paths first. The Low Boughs, the Empty Graves, the Lantern Row. Come back alive and we'll talk. | 我去。 I'll go. → `accept_road` · 树上的呢？ And the ones in the trees? (requires `q_explore_forest` turned in) → `accept_empty` · 灯祠里唱歌的是谁？ Who sings in the Chantry? (requires `q_defend_camp_forest` turned in) → `accept_name` · 还有别的吗？ Anything else? → `other` |
| `accept_road` | 走路中间。听见绳子响，就别站着。 | Keep to the middle of the path. If you hear rope creak, don't stand still. | questTrigger `q_explore_forest` |
| `accept_empty` | 六个吊着的，六个泡着的。放倒了，我好去埋。 | Six from the trees, six from the water. Put them down and I'll do the burying. | questTrigger `q_kill_undead` |
| `accept_name` | 就是你认识的那个修士。让他停下。然后往东，去找熄了灯的老头。 | The monk you know. Make him stop. Then go east and find the old man who put his lamp out. | questTrigger `q_talk_hermit` |
| `ration` | 逃难的每人一份。你是从坑里出来的？……拿两份。 | One share for every refugee. You came out of the pit? ...Take two. | one-time 50 gold / 80 exp |
| `other` | 赫达在浅滩那边追狗。诺拉在南边缝殓布。东边还有个熄了灯的老头。 | Hedda's tracking hounds past the ford. Nora sews shrouds in the south. And there's an old man in the east who put his lamp out. | 有人走丢了？ Someone's missing? (requires `q_explore_forest` turned in) → `accept_lost` · 赫达在悬赏什么？ What is Hedda's bounty? (requires `q_kill_undead` turned in) → `accept_bounty` · 回去 Back → `root` |
| `accept_lost` | 磨坊主的儿子，七夜前拔了门闩往溪边去了。赫达在追脚印。 | The miller's son drew the bar seven nights ago and went toward the stream. Hedda is on the tracks. | questTrigger `q_lost_scout` |
| `accept_bounty` | 灰鬃，她家犬舍的头犬，如今领着西南那一窝。她自己下不了手。 | Ashcoat, the lead hound of her kennel. He runs the den in the south-west now. She can't do it herself. | questTrigger `q_collect_wolf_pelts` |
| `farewell` | 天黑别出门。……算了，这里一直是黑的。 | Don't go out after dark. ...Never mind. It's always dark here. | — |

The bot's tree walk is `ration → work → accept_road` (read from data).

**Bran (`forest_hermit`, 4 nodes).** He lists M3, M4 and S5 on his quest card; the tree exists to hand M5 over where
the hero stands, 39 tiles from the Barrow, instead of 82 tiles away at the mill.

| Node | zh-CN | en | Choices → |
|---|---|---|---|
| `root` | 灯我不点了。点着的时候，我看见的都是该吊的人。 | I don't light it any more. When it burned, everyone I saw was someone to hang. | 树底下还有谁？ Who else is under the tree? (requires `q_kill_werewolf_alpha` turned in) → `keeper` · 告辞。 Farewell. → `farewell` |
| `keeper` | 首灯威斯坦。我们入会那天，都要下去见他一面。他念你的名字，再记进灯册。 | Wystan, the First Lamp. The day we took the cord, each of us went down to stand before him. He says your name, and then he writes it in the ledger. | 我下去。 I'll go down. → `accept_seal` · 回去 Back → `root` |
| `accept_seal` | 根口在橡树西南。下去以后，他念谁的名字，你都别应。办完了，去告诉葛蕾。 | The mouth is south-west of the oak. Down there, whatever name he says, don't answer. When it's done, go and tell Gret. | questTrigger `q_seal_dark_source` |
| `farewell` | 路上别提灯。……我是说，别像我那样提。 | Carry no lamp on the road. ...I mean, not the way I carried one. | — |

### 3.4 Brother Aldwin in Knellwatch (a Chapter 1 data amendment)

As shipped, `knell_chaplain` speaks in `cs_rf_finale`, then stands in Knellwatch for good with an after-line, and
lists five Chapter 1 quests (S2, S3, S5, S6 and the Pit). Chapter 2 kills him at the Chantry. The spec therefore adds:

- `knell_chaplain.visibleWhen {any: [{quest: q_explore_forest, state: notStarted}, {quest: q_rf_barrowmoss, state:
  notTurnedIn}, {quest: q_rf_tag_thief, state: notTurnedIn}, {quest: q_rf_lanternwisp, state: notTurnedIn}, {quest:
  q_rf_hallowed_ground, state: notTurnedIn}, {quest: q_rf_the_pit, state: notTurnedIn}]}`: he stands at his stool
  until the hero has taken M1 at the mill **and** has nothing left to do with him. He is on screen for all of
  `cs_rf_finale`, and no Chapter 1 quest loses its giver. Hiding him while his reacceptable quest could be taken again
  is the validator's warning case (K6), not its error case.
- His after-line `data.npc.knell_chaplain.after.q_rf_last_toll` stays (it is what he says between the dawn and the
  hero's leaving).
- The Captain (`quest_elder`) gains one line, first in his list, `when {any: [{quest: q_explore_forest, state: open},
  {quest: q_explore_forest, state: turnedIn}]}`: 「奥德温修士跟着头一批车东去了。钟，现在是我在敲。」 / "Brother Aldwin
  went east with the first wagons. I ring the bell now." His stool needs no new piece: it is already part of the camp.
- **Residual, accepted:** a hero who goes east with one of Aldwin's side quests unfinished and walks back finds him
  still at his stool after killing the Cantor. Closing that hole needs a stand-in giver (an NPC that takes over
  another's quest list when it is hidden), which is a core feature and is **not requested**; §22 O6.
- Chapter 1 pins that move: `npcs.json` (one `visibleWhen`, one line), the `content_ch1_test` NPC-line pins, the i18n
  key count. The Chapter 1 playthrough dump must stay byte-identical (the bot never takes M1 of Chapter 2 there).

## 4. Monster roster

Row format and tags as monsters.md §2. Speed is the data unit (tiles/s = speed × 0.03; the hero walks 3.6 tiles/s).
Every definition carries `family`, `hitCapHeroMaxHpPct: 35` and an authored `windupMs` on every ability (≥ 600; the
mini-boss, the story boss and the Barrow's keeper ≥ 700; fuses ≥ 800). **Every ability names its clip** (the loader
allows a clipless ability only for kinds `hazard` and `call`). An ability without `rangeMax` is used only on a due
swing, that is with the hero inside attack range × 1.2; every ability meant to reach further carries `rangeMax`. A
status on an ability takes an absolute `value` (a slow in percent, a DoT in damage per tick); the percent-of-damage
form exists only on `onHitStatus`.

### 4.0 Summary

**Levels follow the hero that arrives** (Lv 13-14 at the gate, Lv 15-16 at the Cantor, Lv 16-19 at Volgan, §18), not
the web zone's label: Chapter 1's Pit measured what happens otherwise (the safest stretch of the chapter, chapter1.md
§9).

**The HP and damage columns are placeholders.** Chapter 1's first rows were wrong by a factor of up to nine (Valdras
1100 → 10400 HP). The only measured hero figures are whole-fight rates from the Chapter 1 sweeps: about 250 / 150 / 75
damage per second (warrior / mage / rogue) at Lv 9-10 against the Pitwarden and 263 / 195 / 74 at Lv 13 against
Valdras, so between those levels the rate grew about 5 % / 30 % / 0 %. Nothing measured supports a growth figure for
Lv 13-19. 4T therefore **measures first** (hero single-target rate at Lv 13, 15, 17 and 19 per class and bot model,
from the entry fixtures, §19) and then sets each HP as *rate × target seconds* against §18. The placeholders below
assume no growth for the rogue and 30 % for the others, and are there only so that the data loads.

| Id | zh / en | Family, role | L | HP | Dmg | Def | Spd | Aggro | Range | Atk ms | Exp | Gold | vs web slot |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `fenbound` | 沉沼尸 / Fenbound | Wood's Dead, slow tank | 13 | 300 | 20 | 14 | 30 | 4 | 1.3 | 1900 | 14 | 4-9 | zombie; puddle on death |
| `hanged_one` | 绳上人 / the Hanged | Wood's Dead, dormant ambusher | 14 | 180 | 22 | 10 | 62 | 6 (wake 2.5) | 1.6 | 1100 | 15 | 3-7 | skeleton; drops in groups |
| `moon_hound` | 食月犬 / Moon-hound | Moon-Bitten, pack hunter | 15 | 170 | 21 | 8 | **125** | 7 | 1.3 | 850 | 12 | 1-4 | new (SKEL_Quadruped); faster than the hero |
| `moonsick` | 月病者 / Moon-Sick | Moon-Bitten, bruiser that turns | 16 | 340 | 28 | 14 | 60 | 6 | 1.7 | 950 | 20 | 6-12 | werewolf; a cone and a telegraphed Turn |
| `lamp_netter` | 执灯网手 / Lamp-Bearer Netter | Living, ranged control | 16 | 230 | 24 | 11 | 52 | 8 | 6.0 | 2000 | 18 | 8-14 | new |
| `lamp_warden` | 执灯卫 / Lamp-Bearer Warden | Living, zone control, leader | 17 | 430 | 26 | 18 | 48 | 7 | 1.8 | 1400 | 24 | 10-18 | new |
| `hound_packleader` | 头犬 / Pack-Leader | Moon-Bitten, leader (call) | 17 | 470 | 30 | 14 | 115 | 8 | 1.5 | 1000 | 26 | 4-8 | new |
| `miniboss_cantor` | 绳林领唱 / the Cantor of the Hanged | mini-boss, raiser | 17 | 2400 | 30 | 16 | 46 | 8 | 5.0 | 1400 | 240 | 25-50 | shadow weaver |
| `volgan` | 噬月者·沃尔甘 / Volgan the Moon-Eater | act boss | 19 | 14000 | 36 | 20 | 62 | 9 | 2.2 | 1100 | 400 | 50-100 | alpha; three acts in five phase rows |
| `sub_rootbound` | 缚根首灯·威斯坦 / Wystan, the Rootbound First Lamp | sub-dungeon keeper | 19 | 4000 | 34 | 18 | 24 | 9 | 2.2 | 1600 | 260 | 30-60 | new |

Named single-target HP is about 23500 in placeholders (Volgan, Wystan, the Cantor, two hunts) against about 14000 as
built in Chapter 1. The rogue's whole-fight rate did not grow from Lv 9 to Lv 13 (DR9 records it as "his mana-bound
damage: class balance, not this package"); at 74 per second 14000 HP is 189 s before the hounds. **That is a
precondition, not a tuning detail**: §22 O3.

Hunts (hunt defaults as today: def × 1.2, exp × max(3, hpMul), gold × 3, aggro ≥ 7, scale 1.25; leaders are elite with
the zone's one affix and the mini-boss loot floor). Two hunts, each with one ability its base does not have (feature
`quest.huntAbilities`, as Hungerbell's `supper_bell`):

| Hunt | Base | HP × | Dmg × | Minions | Own ability | Where | Quest |
|---|---|---|---|---|---|---|---|
| `hunt_greymane` 灰鬃 / Ashcoat | `hound_packleader` | 4 | 1.5 | 2 `moon_hound` | **长嚎 Long Howl** `long_howl`: kind `call`, clip `Cast01` at 1000 ms, radius 10, family `moonbitten`, `damage × 1.3` for 6 s, cooldown 16 s: kill the hounds while it is down, or wait it out | (22,96), spawns on accept | S1 |
| `hunt_riftjaw` 裂颚 / Riftjaw | `moonsick` | 3.5 | 1.4 | — | **扑落 Leap** `leap`: kind `circle`, r 2 at `heroAtWindup`, clip `Cast01` at 900 ms, × 1.6, `rangeMin` 3, `rangeMax` 7, cooldown 8 s | (48,79), revealed after the clues | S2 |

The web's other two hunts are cut: S6 and S8 end on an item and a place (§6), and four hunts that differed only by a
life multiplier were one idea four times.

**Raised** (the Cantor's, Volgan's and Wystan's summons): the Cantor's raise stands a corpse as `hanged_one` at 50 %
HP; Volgan's phase summons are `moon_hound`; Wystan's are `hanged_one`. All carry the Raised role (no exp, gold or
loot; never respawn; collapse with their raiser).

### 4.1 Colour discipline for Chapter 2

DR5 stands: crimson rim and bone fill for every enemy ground telegraph; ember orange for the hero and refuges only;
breach-violet for breach magic. Chapter 2 adds two enemy accents (proposed rows for monsters.md §1.2; the hue audit
checks both before any art ships, §22 O4):

| Colour | Hex | Who may use it | Never |
|---|---|---|---|
| Moon-tarnish | #9FE0C8 (hue 158°: old silver gone green) around near-black #0C1614 | the fouled Moon Seal: Wood's Dead eye pinpoints behind the shroud weave, the light between Volgan's teeth, raise markers of the Cantor, the Barrow's root glow | any hero recipe within 30° of hue at sat > .5; never a damage telegraph |
| Sallow lamplight | #E6D36A (hue 51°, sat .56) | Lamp-Bearer lanterns (a held prop light and the lantern-flash before a crimson telegraph) | the ember band (hue 15-35°, sat > .6); never a refuge light |
| Silver-white | #E6ECF5, non-emissive eye-shine | the Moon-Bitten (Moon-Sick, hounds): beasts and the turned, not the seal's dead | — |
| True moonlight | #E6ECF5 top light | only after `hw_barrow_cleansed` and in the cleansed Barrow | before the cleanse |

Family markers (one per hostile): Wood's Dead = moon-tarnish pinpoints; Moon-Bitten = silver-white eye-shine; the
Living (Lamp-Bearers) = no glow, a crimson lamp-cord on black (the Living's crimson cloth rule).

### 4.2 The Wood's Dead 林中亡者 (`wooddead`; SKEL_Human, the Unburied kit with a shroud layer)

**Shared look.** Shroud-wrapped figures: coarse grey-brown linen (#7A7468, shadow #3C3A34) wound head to foot and
bound with pale hemp rope (#C9B88A); no face, hands as wrapped mitts, two moon-tarnish pinpoints (#9FE0C8 emissive)
behind the weave where eyes would be. Voice family **Wooddead**: linen rasp, rope creak, a held hum (the chant), no
words.

#### `hanged_one` 绳上人 / the Hanged — dormant ambusher

| | |
|---|---|
| Behaviour | Spawn entries are `dormant {wakeRange: 2.5, rise: true}`: each hangs as a bundle over its tile and the entry **wakes as one** (K7). Then plain chase / attack. **绳鞭 Rope Lash** `rope_lash`: kind `line`, 3.5 × 1 toward the hero, `Attack02` at 700 ms, × 1.2, slow 30 % for 1.5 s (it whips its trailing rope at the legs), cooldown 6 s, `rangeMax` 3.5 (a ranged special past its 1.9-tile swing reach) |
| Two clips for one core action | The core plays the action `Rise` (clip name "Rise", root time from that clip's length) both when a dormant member wakes and when a corpse is raised. Those are opposite motions here, so the asset ships **`Drop`** (the rope parts, it lands crouched and stands) and **`Rise`** (a laid bundle gets up), both exactly 900 ms, and UE picks `Drop` when the monster was `dormant` in the snapshot before the event (a rule in `AbyssMonsterLookRules.h`, tested; no core change). The Cantor's and Wystan's raised Hanged play `Rise`; a phase summon of `hanged_one` plays `Drop` as its spawn intro |
| On death | folds into its shroud; `corpse {leaves: 'corpse'}` (the Cantor raises it; a fire killing blow burns it) |
| Packs, spawns | the Low Boughs (25,15) × 8 and the Middle Boughs (38,22) × 6, both dormant; S7's three waves (wave members arrive walking); the Barrow's Hanging Gallery (a dormant line of six) and the keeper's guard. Not in the ambush pool (an ambush member appears standing, which is not how the Hanged arrive) |
| Never | no upward motion, no rope at a neck, no mechanic that lifts or holds the hero (§1) |
| Sound | a rope creak when the hero comes within `wakeRange` + 3 tiles of a dormant entry (UE, from the snapshot's `dormant` members), linen, the thud of a landing |
| Art brief | **Shroud variant of the Unburied kit** through `monsters/wooddead.py` and `monsters/hanged_one.py` (~2.25 tasks with the kit layer and clips). The wrapped silhouette must read at the W1 camera as a bound sack with a rope tail, not as a person: shoulders rounded by the wrapping, feet bound together until the Drop (the binding tears on landing so it can walk). A 1.2 m rope tail on a cloth chain from the upper back. Clips: `Idle_Hang` (dormant: the bundle turns a few degrees and back, 4000 ms), `Drop` 900 ms (rope parts 200, lands 520), `Rise` 900 ms, `Walk` (short bound steps), `Attack01` 1000 ms / Contact 600, `Attack02` 1200 / **Contact 700**, `Hurt` 300, `Death` 900 (the shroud empties), `Stun` 700. Budget 7k / 3k |

#### `fenbound` 沉沼尸 / Fenbound — slow tank

| | |
|---|---|
| Behaviour | Slow chase from water and reeds. **泥缚 Mire Grasp** `mire_grasp`: kind `circle`, r 1.6 at where the hero stood (`heroAtWindup`), `Cast01` at 800 ms, × 1.1, slow 35 % for 2 s, cooldown 7 s |
| On death | `onDeath {kind: 'puddle', shape {radius: 1.6}, dmg: 6, damageType: 'poison', puddle {durationMs: 4000, tickMs: 500}, capHeroMaxHpPct: 20}`: it sinks and leaves bog water that poisons whoever stands in it. `corpse {leaves: 'none'}` (nothing to raise). Where it matters: S7's waves 2 and 3 put the puddles round the ladder the hero has to hold |
| Packs, spawns | the Sunk Wheel (14,40) × 8; S7 waves 2 and 3 |
| Sound | water pouring off linen, a long exhale |
| Art brief | **Variant** through `monsters/fenbound.py` (~1.25 tasks): the same shroud, black with bog water to the chest (#2A2E26), reeds and a mill-weight tied to one ankle (people the Lamp-Bearers put in the pond). Wading `Walk`; `Attack01` 1700 / Contact 800; `Cast01` 1400 / **Contact 800** (both arms into the ground); `Death` 1200 (sinks through a puddle decal). Budget 7k / 3k |

### 4.3 The Living: Lamp-Bearer remnants 执灯人 (`living`; SKEL_Human, living palettes)

The church's witch-finders, still at work. Black coats, a **crimson lamp-cord** across the chest (the family marker),
iron-shod poles. They talk: 「照——」 before a lantern is raised. Voice family **Human**.

#### `lamp_netter` 执灯网手 / Lamp-Bearer Netter — ranged control

| | |
|---|---|
| Behaviour | Ranged (6.0): a thrown hooked weight (`projectileColor` #D8D2C0), `kite {min: 3, max: 6}`. **抛网 Net** `throw_net`: kind `circle`, r 1.6 at `heroAtWindup`, `Cast01` at 900 ms, `dmgMul: 0` (a status-only strike), slow 60 % for 2.5 s, `rangeMax` 6, cooldown 9 s. `morale {fleeOnLeaderDeathMs: 5000, fleeRadius: 8}`: netters run when their warden goes down |
| Packs, spawns | the Lamp Line (70,50) × 3 with two wardens; the Chain Watch (97,74) × 3 with one warden; the ambush champion pack; rescue-event guards |
| Art brief | **Variant of the Living kit** through `monsters/lamp_bearers.py` (~1.5 tasks, the net included): black gambeson, hood, crimson cord; a weighted net over the left shoulder (cloth chain), a bag of hooked weights. `Attack01` 1500 / Release 600; `Cast01` 1500 / **Contact 900** (the net opens in the air at 600). Budget 7k / 3k |

#### `lamp_warden` 执灯卫 / Lamp-Bearer Warden — zone control, pack leader

| | |
|---|---|
| Behaviour | Melee with the lantern-pole. **照 Lamplight** `lamplight`: kind `hazard`, circle r 2.5 at `heroAtWindup`, `Cast01` at 900 ms, then a burning patch for 4 s (`hazard {durationMs: 4000, tickMs: 500, tickDmgMul: 0.25, damageType: 'fire'}`; no `dmgMul` and no `status` on the ability, as the loader requires of a hazard), `rangeMax` 6, cooldown 10 s: the lantern is raised (sallow flash), the crimson decal fills, the ground the light touched burns the branded. Net plus Lamplight is the pack's combination: do not be slowed inside the patch. Leads its entry (`leader {monsterId: 'lamp_warden'}`) |
| Packs, spawns | the Lamp Line × 2 (one leads); the Chain Watch × 1; the ambush champion's leader |
| Art brief | **Variant** (~1.5 tasks, the lantern-pole included): the tallest Lamp-Bearer, a brimmed iron cap, a 2 m pole with a shuttered lantern (emissive #E6D36A only while the shutter is open: `Cast01`), a coil of rope at the belt (never used on screen). `Attack01` 1300 / Contact 620; `Cast01` 1500 / shutter opens 400 / **Contact 900**. Budget 7.5k / 3k |

### 4.4 The Moon-Bitten 月噬 (`moonbitten`): the first SKEL_Quadruped family

Whatever drank the water or was bred under the fouled moon. Beasts and the long-turned, not the seal's dead:
silver-white eye-shine, no emissive.

#### `moon_hound` 食月犬 / Moon-hound — pack hunter (first member of SKEL_Quadruped)

| | |
|---|---|
| Behaviour | Fast (3.75 tiles/s against the hero's 3.6: a pack cannot be outrun and has to be fought), frail. `packAggro: 8`. **扑 Pounce** `pounce`: kind `line`, 4 × 1.2 toward the hero, `dash: true` (it stands at the line's end after the hit), `rangeMin` 2, `rangeMax` 4.5, clip `Pounce` at 650 ms, × 1.3, cooldown 5 s. `morale {fleeOnLeaderDeathMs: 4000, fleeRadius: 8}`: **the pack always breaks for 4 s when its leader dies** (a dead caster's buffs are dropped before the morale check, so no howl can hold it). `corpse {leaves: 'remains'}` |
| Flanking | Not a core feature and **not requested**: three hounds pouncing from different ranges on a 5 s cooldown already arrive from different sides |
| Packs, spawns | the Wolf Run (30,70) × 5 with one leader; Ashcoat's Den (20,95) × 4 with two leaders; the Kennel Ruins × 2; Volgan's summons; Ashcoat's minions; the zone's ambush pool |
| Sound | Voice family **Hound** (new): panting, a rising whine before the pounce, claws on roots |
| Art brief | **New skeleton and clip library**: `kit/skel_quadruped.py` (spine 4, neck 2, head, jaw, tail 3, four legs of 3 bones + toe, ear pair; sockets `fx_overhead`, `fx_elite`, `bite`, `collar`), `monsters/qclips.py`, `monsters/moon_hound.py` (~5.5 tasks: skeleton 1.5, clip library 1.5, mesh 1, LOD / budget / ship 1, UE single-view check and voices 0.5). A gaunt long-legged hound, shoulder height 0.75 m, length 1.3 m: silver-grey coat gone to slate (#6E7478, shadow #32363A, belly #9AA0A0), ribs showing as paint strokes, a torn leather **temple collar** with a tarnished Well-Ring (#8A9088). Head low, tail straight. Clips: `Idle` 1600, `Idle_Sniff` 2400, `Walk` (trot) 700 with `FootFL / FootFR / FootBL / FootBR` notifies, `Run` 460, `Attack01` (bite) 800 / Contact 320, `Pounce` 1000 / **Contact 650** (leaves the ground at 380), `Hurt` 280, `Death` 900 (rolls onto its side, greys to a husk), `Stun` 700. Budget 5k / 2.5k. The rig must pass the darkest-tile gate on the `hangwood` root tile (a low, dark body on dark ground is this chapter's hardest readability case: rim ≥ 3 px, eye-shine ≥ 2 px) |

#### `hound_packleader` 头犬 / Pack-Leader — leader, call

| | |
|---|---|
| Behaviour | Larger (scale × 1.25), the same Pounce at × 1.4. **嚎 Howl** `howl`: kind `call`, clip `Cast01` at 1000 ms, radius 10, family `moonbitten`: provokes every Moon-Bitten in reach and gives them (and itself) `attackSpeed × 1.2` for 5 s. One ability has one buff stat, so the howl is this and nothing more. Cooldown 10 s. Kill order: the leader, then the pack runs for 4 s. Leads its entry (`leader {monsterId: 'hound_packleader'}`) |
| Unique `hunt_greymane` | one eye; the only hound with an intact Well-Ring on its collar |
| Art brief | **Variant** (~1 task + 0.5 for Ashcoat): heavier shoulders, a grey saddle of fur, a scarred muzzle. `Cast01` (howl) 1400 / **Contact 1000**. Budget 5.5k / 2.5k |

#### `moonsick` 月病者 / Moon-Sick — a bruiser that turns (SKEL_Human, lean variant)

| | |
|---|---|
| Who | The **long-turned**: people of Mosswick and the kennels who drank the water for years. They are past the line: they know no face, they do not speak, and no one in the chapter proposes to bring one home. The newly sick in the mill are a different thing and are never a target (§1) |
| Behaviour | No dash (the hound owns the dash). **抓 Rake** `rake`: kind `cone`, 100°, r 2.2, `Attack02` at 650 ms, × 1.3, bleed `value` 5 per tick for 3 s, cooldown 5 s: the answer is to step **behind** it, not aside. **The Turn**, a one-row phase table `{hpBelowPct: 40, enterAction: 'roar', speedMul: 1.6, attackSpeedMul: 1.3, damageTakenMul: 1.15}`: it stops and howls for the `Roar` clip (900 ms, rooted), then comes on at 2.9 tiles/s. The roar is the window: finish it while it stands, or give ground. `corpse {leaves: 'corpse'}` |
| Packs, spawns | Mosswick (46,48) × 6; the Kennel Ruins (78,74) × 4; Riftjaw's base |
| Sound | Voice family **Moonsick** (new, small): breath through teeth, a throat sound; **no words and no half-words** |
| Art brief | **Lean variant of the Living kit** through `monsters/moonsick.py` (~2.5 tasks): arms + 12 %, a forward crouch, hands half-curled with long nails (the turn is in the hands and the gait), what is left of field clothes gone to rags and moss, hair over the face; skin greyed (#9A948A). No muzzle, no fur, no wounds, nothing that reads as bedclothes. `Idle` (rocks, looks up), `Walk` (a lope), `Attack01` 900 / Contact 420, `Attack02` (Rake) 1100 / **Contact 650**, `Roar` 900, `Hurt`, `Death` 1000 (curls up; a husk). Budget 7k / 3k |

### 4.5 Stretch (cut first)

| Id | zh / en | Role | Notes |
|---|---|---|---|
| `bough_watcher` | 垂枝者 / Bough-Watcher | stationary ranged | a Hanged that does not drop: lashes down from its bough at range 5 (speed 0, dormant, no `rise`). Needs a check that a speed-0 monster is legal data |
| `hound` `flank` | — | chase goal | `flank {arcDeg, radius}`: pack members pick chase goals at spread angles round the hero. Core work; not requested |

### 4.6 Loot hooks

Regulars: today's level-based drops (with the roster at the hero's level the drops are too); Lamp-Bearers roll the
higher gold range; hounds carry almost none. Quest drops: `mat_shadow_shard` from `lamp_netter` / `lamp_warden` (S6,
chance .6), `mat_mill_key` from `hunt_riftjaw` (S2, chance 1). The ambush champion's leader drops one guaranteed magic
item (`leaderLoot: 'magic'`). Mini-boss, keeper and story boss: the existing floors; the keeper pays once per save
(the Pit's rule).

### 4.7 The mechanic ramp (in the order the route meets it)

| Hero Lv | New idea | Where | Quest that takes the hero there |
|---|---|---|---|
| 13-14 | look up: dormant groups that drop as one | the Low Boughs | M1 |
| 14 | ground that hurts after the kill (puddles) | the Sunk Wheel | M2 |
| 14 | a dormant **line** across the path | the Middle Boughs | M2 |
| 14-15 | hold a point while puddles close in | the Cutting Oak | S7 (required for M3) |
| 15 | fast packs with a leader whose death breaks them | the Wolf Run | S2 (optional); else first met as Volgan's summons |
| 15-16 | a raiser with a ring whose safe ground is next to him | the Chantry | M3 |
| 16 | a bruiser to step behind, and a turn to interrupt by killing | Mosswick | on M3's road east (aggro 6 from the path) |
| 16 | control plus a burning patch: do not be slowed inside it | the Lamp Line | on M3's road; S6 |
| 16-19 | a chained boss that still reaches out, then a pack leader with a feeding ground, then a shrinking light | the Gallows-Oak | M4 |
| 18-19 | a rooted keeper whose darkness covers the whole room: come to him | the Root Barrow | M5 |

A main-path-only hero first meets hounds as Volgan's summons. That is accepted (the pack lesson is "kill the leader",
and there the leader is the boss) and it is one of the things the main-only sweep watches (§18).

## 5. Main quests

Rewards: exp / gold; main gear choices rare; embers 2 per main turn-in unless stated. Limits: offer ≤ 70, progress ≤
50, complete ≤ 50 zh code points.

### M1 `q_explore_forest` 绳道 / The Rope Road — explore, L13

| Field | Value |
|---|---|
| Listed on | `quest_scout` (tree `accept_road` or the quest card) |
| Objectives | 1. explore `zone_hw_low_boughs` 低枝林 / the Low Boughs (26,10) **r4** · 2. explore `zone_hw_empty_graves` 空坟场 / the Empty Graves (10,29) r10 · 3. explore `zone_hw_lantern_row` 灯道 / the Lantern Row (44,27) **r5** |
| Rewards | 420 exp / 60 gold |

| Line | zh-CN | en |
|---|---|---|
| offer | 磨坊收了四十个逃难的。去认三处地方：低枝林、空坟场、灯道。走路中间，别抬头太久。灯道尽头的灯祠，先别进。 | The mill has taken in forty refugees. Go and learn three places: the Low Boughs, the Empty Graves, the Lantern Row. Keep to the middle of the path and don't look up for long. Don't go into the Chantry at the end of the Row. Not yet. |
| progress | 听见头顶绳子响，就别站着。 | If you hear rope creak overhead, don't stand still. |
| complete | 灯祠里有人唱歌？……执灯人不唱歌。他们只念名字，念完就吊。 | Someone singing in the Chantry? ...Lamp-Bearers don't sing. They read out a name, and then they hang it. |

Beat: the Low Boughs' explore spot is 5.1 tiles north of the pack's anchor and the straight line from the mill to it
runs under the bundles (at row 15 the line is at col 25), so with r4 a hero who walks from the mill reaches the
pack before the circle: **the first drop is M1's** (the content test pins the geometry; a hero who circles round from
the north can still miss it, and then M2 or the walk back delivers it). The Lantern Row's spot is 11.4 tiles from
the Cantor and its circle ends 6.4 tiles from him, coming from the west: the chant is heard (the chant bed, §16) and
he is not pulled (aggro 8). The web spot at (58,26) r10 put a Lv 13 hero 5.6-7.8 tiles from a 2400 HP elite two quests
early.

### M2 `q_kill_undead` 空绳 / Empty Ropes — kill, L14, prereq M1

| Field | Value |
|---|---|
| Listed on | `quest_scout` (tree `accept_empty` or the quest card) |
| Objectives | 1. kill `hanged_one` × 6 · 2. kill `fenbound` × 6 |
| questArea | (26,31) r17 (moved from (30,30) r15, which held neither pack; the guide arrow falls back to it) |
| Rewards | 520 exp / 75 gold + `c_hp_potion_m` |
| Triggers | turn-in → `cs_hw_empty_rope` |

Sources without any respawn: the Middle Boughs (six, a line across the lantern road: a second kind of drop, not the
first one again) and the Sunk Wheel (eight). The Low Boughs, re-hung, are a second source if [R4] is built.

| Line | zh-CN | en |
|---|---|---|
| offer | 绳子不肯空着。树上的往下扑，沼里的往岸上爬，都是来拉人顶位子的。放倒六个吊着的，六个泡着的。我夜里去埋。 | The ropes will not hang empty. The ones in the trees drop and the ones in the fen climb out, all to pull someone into the place. Put down six from the trees and six from the water. I'll bury them tonight. |
| progress | 别让它们的绳子缠上你的腿。 | Don't let their ropes catch your legs. |
| complete | 十二个，我今晚埋得完。……明天树上要是又满了，你别告诉我。 | Twelve. I can bury twelve tonight. ...If the trees are full again tomorrow, don't tell me. |

### M3 `q_talk_hermit` 被抹去的名字 / The Erased Name — talk, L15, prereq M2 **and S7**

| Field | Value |
|---|---|
| Listed on | `forest_hermit` (turn-in at the Dark Lantern). Offered by Gret's tree (`accept_name`) or by Bran's card |
| Objectives | 1. kill `miniboss_cantor` × 1 · 2. talk `forest_hermit` (`labelKey` `data.questObj.q_talk_hermit.rubbing`: 把拓本带给熄灯人 / Take the rubbing to the Lampless) |
| questArea | (100,62) r6 (kept) |
| Rewards | 640 exp / 50 gold + choice [armor, belt]; `setsFlags: ['hw_cantor_silenced']` |
| Triggers | `monster_killed` `miniboss_cantor` (`whileQuest`) → `cs_hw_cantor`; turn-in → `cs_tf_hermit`, **played at the Dark Lantern with Bran on screen** |

The mini-boss is on the main path, as the Corpseherd was. The rubbing he carried is what Bran reads: the talk
objective's label names it (no collect objective: whether a drop from a zone mini-boss is a legal collect source was
not checked, and the label does the job). The walk from the Chantry to the Dark Lantern (55 tiles) passes Mosswick's
edge and runs through the Lamp Line: the Lamp-Bearer lesson is on the main path.

| Line | zh-CN | en |
|---|---|---|
| offer | 灯祠里领唱的那个，怀里有一卷拓本，是从经台上拿的。让他停下，把拓本带来。那上面有我熄灯的缘故。 | The one leading the chant in the Chantry carries a rubbing he took from the lectern. Make him stop, and bring it to me. The reason I put my lamp out is on it. |
| progress | 灯道往东，过了浅滩，路上有巡灯的。别在他们的灯底下站着。 | East along the Row and past the ford there are Lamp-Bearers on the road. Don't stand under their light. |
| complete | 拓本留在我这儿。你带着它，夜里睡不着。……我有三十年了。 | Leave the rubbing with me. Carry it and you won't sleep. ...I haven't, for thirty years. |

### M4 `q_kill_werewolf_alpha` 噬月者 / The Moon-Eater — kill, L17, prereq M3

| Field | Value |
|---|---|
| Listed on | `forest_hermit` |
| Objectives | 1. kill `lamp_warden` × 1 (`labelKey` `data.questObj.q_kill_werewolf_alpha.watch`: 让守链岗停手 / Stop the Chain Watch) · 2. kill `volgan` × 1 (storyBoss, M7 rules through [R1]) |
| questArea | data (58,58) r10 (kept); re-centred on the boss spawn at runtime (the Q4 fix, made per chapter by [R1]) |
| Rewards | 900 exp / 120 gold + choice [weapon, helmet]; `setsFlags: ['hw_volgan_down']` |
| Triggers | boss intro `cs_boss_volgan` at 9 tiles; kill → `cs_tf_moonfang` (grants `pet_storm_wolf`) |

The first objective no longer asks for six Moon-Sick (a main quest that requires killing the sick, §1). It asks for
the people who still feed the wolf, who stand on the road from the Dark Lantern to the oak.

| Line | zh-CN | en |
|---|---|---|
| offer | 沃尔甘原是月祭司的圣狼。我们把它拴在绞架橡下，吊一个，喂一个。喂它的人还在。先让他们停手，再去把链子解了。 | Volgan was the moon-priestesses' sacred wolf. We chained him under the Gallows-Oak and fed him one for every one we hung. The ones who feed him are still there. Stop them first. Then go and take the chain off. |
| progress | 它一嚎，林子里的狗都听它的。别在树根边上杀狗。 | When he howls, every hound in the wood answers. Don't kill hounds near the roots. |
| complete | 小的跟着你来了？……它认你掌心那块印。喂的事，到我这儿为止。 | The little one followed you? ...It knows the mark on your palm. The feeding ends with me. |

### M5 `q_seal_dark_source` 月辉之印 / The Moon Seal — kill (sub-dungeon), L18, prereq M4 (chapter-complete quest)

| Field | Value |
|---|---|
| Listed on | `quest_scout` (turn-in and finale at the mill). Offered by Bran's tree (`accept_seal`) or by Gret's card |
| Objectives | 1. kill `sub_rootbound` × 1 (`zone: 'sub_hw_barrow'`) · 2. `clear_area` `sub_hw_barrow` (`required` 1, `maxRemaining` 2, `after` 0) |
| questArea | (80,94) r8 (moved from (19,64); part of the re-baseline, §14) |
| Rewards | 1000 exp / 150 gold + choice [jewelry, helmet, belt] + 6 embers + permanent +1 skill point (月井余光 / The Moon Well's Afterlight, once per difficulty); unlocks `pet_house` (月井; not visible while the tower is hidden, [R1]) |
| Triggers | entering `sub_hw_barrow` → `cs_hw_barrow`; `hw_barrow_cleansed` set → `cs_hw_seal` [R2]; turn-in → `cs_tf_finale` (`chapterCompleteQuest` of this zone, [R1]) |

The cleanse needs the quest (the `clear_area` objective sets the flag), so a hero who goes down early can fight but
cannot finish the chapter out of order; an uncleansed Barrow is rebuilt on every entry. **The world changes at the
cleanse, not at the turn-in** (§8): the 75 tiles from the Barrow to the mill are walked through a wood where the ropes
have let go.

| Line | zh-CN | en |
|---|---|---|
| offer | 布兰说，树根底下有个洞，月印在下面。守着它的人叫威斯坦，守了几百年。下去，让他松手。 | Bran says there's a hole under the roots and the Moon Seal is below. The one keeping it is called Wystan, and he has kept it for hundreds of years. Go down and make him let go. |
| progress | 根冢里不见天。跟着那盏黄灯走——然后把它灭了。 | There's no sky in the Root Barrow. Follow the yellow lamp. Then put it out. |
| complete | 井里有月亮了。树上的人，一夜之间都下来了。这一回，没有一个再上去。 | There's a moon in the well. Everyone in the trees came down in one night. This time not one of them went back up. |

## 6. Side quests

Side gear choices magic; embers 1 per side turn-in unless stated. The eight differ in shape by objective types alone:
S1 [kill], S2 [investigate × 3, kill, collect, talk], S3 [collect], S4 [talk, talk], S5 [collect, investigate], S6
[collect, kill], S7 [defend], S8 [investigate × 4]. Only S1 and S2 end on a named hunt.

### S1 `q_collect_wolf_pelts` 悬赏：灰鬃 / Bounty: Ashcoat — kill (hunt), L16, prereq M2, listed on `forest_tracker`

| Field | Value |
|---|---|
| Offered by | Gret's tree (`accept_bounty`) or Hedda's card |
| Objectives | 1. kill `hunt_greymane` |
| questArea | (22,94) r8 (kept) |
| Rewards | 300 exp / 110 gold + choice [armor, helmet] |

| Line | zh-CN | en |
|---|---|---|
| offer | 灰鬃是我家犬舍里最好的一条。项圈上的银环是月祭司的，我家一代代传给头犬。现在它领着一群吃人的。我自己下不了手。 | Ashcoat was the best hound our kennel ever bred. The silver ring on his collar was a moon-priestess's; my family has passed it down from lead hound to lead hound. Now he leads a pack of man-eaters. I can't do it myself. |
| progress | 它只有一只眼。另一只，是执灯人烫的。 | He has one eye. The Lamp-Bearers burned out the other. |
| complete | 你把项圈带回来了。银环还在。……它认得这个。它到最后都认得。 | You brought the collar back. The ring is still on it. ...He knew it. He knew it to the end. |

### S2 `q_lost_scout` 没回来的人 / The One Who Walked Out — investigate, L15, prereq M1, listed on `forest_tracker`

| Field | Value |
|---|---|
| Offered by | Gret's tree (`accept_lost`) or Hedda's card |
| Objectives | 1-3. investigate `clue_hw_doorbar` (30,60), `clue_hw_shoe` (36,70), `clue_hw_shirt` (44,76), r2 each · 4. kill `hunt_riftjaw` · 5. collect `mat_mill_key` × 1 (drop from `hunt_riftjaw`, chance 1, itemKind `pendant`) · 6. talk `quest_scout` (`labelKey` `data.questObj.q_lost_scout.key`: 把钥匙交给葛蕾 / Give the key to Gret) |
| questArea | (44,76) r10 (kept) |
| Rewards | 280 exp / 90 gold + choice [boots, gloves] |

| Clue | zh-CN | en |
|---|---|---|
| `clue_hw_doorbar` | 一根门闩，扔在溪边。是从里面拔下来的。 | A door-bar, thrown down by the stream. It was drawn from inside. |
| `clue_hw_shoe` | 一只跑丢的鞋。再往前，脚印是光脚的。 | One shoe, lost at a run. Past it, the prints are barefoot. |
| `clue_hw_shirt` | 一件衬衣，是从里面撑裂的。 | A shirt, split from the inside. |

(The web's second clue was "blood on a stone"; a first draft's "a pair of shoes set down neatly" was rejected because
it reads as a self-harm image.)

| Line | zh-CN | en |
|---|---|---|
| offer | 磨坊主的儿子七夜前拔了门闩，往溪边走了。七夜，就认不得人了。顺着脚印找到他，别让他再往下走。 | The miller's son drew the bar seven nights ago and walked for the stream. Seven nights, and they no longer know a face. Follow the prints and find him. Don't let him go any further. |
| progress | 脚印越走越深，步子越迈越大。……那已经不是人的步子了。 | The prints get deeper and the stride gets longer. ...That isn't a man's stride any more. |
| complete | 钥匙给葛蕾了？好。她会告诉磨坊主，他儿子是在林子里走丢的。别的，不用说。 | You gave Gret the key? Good. She'll tell the miller his son got lost in the wood. Nothing else needs saying. |

### S3 `q_spider_nest` 空坟 / Empty Graves — collect, L14, prereq M1, listed on `forest_spirit_medium`

| Field | Value |
|---|---|
| Objectives | 1. collect `mat_rope_end` × 5 (gather, area (12,30) r8, 6 nodes, itemKind `rope`) |
| questArea | (12,30) r9 (kept) |
| Rewards | 180 exp / 70 gold + `c_mp_potion_m` |

| Line | zh-CN | en |
|---|---|---|
| offer | 人留不住，坟就一直空着。去空坟场，把树下割断的绳头捡回来。一截绳头埋进去，也算有人躺过。 | Nobody stays down, so the graves stay empty. Go to the Empty Graves and pick up the cut rope-ends under the trees. Bury a rope-end and at least something has lain there. |
| progress | 捡地上的。树上还系着的，别碰。 | Take the ones on the ground. Don't touch any still tied in a tree. |
| complete | 五截。我替他们缝五块殓布。里面没有人，针脚也不能马虎。 | Five. I'll sew five shrouds for them. Nobody inside, and still the stitches have to be right. |

### S4 `q_ancient_relic` 塔姆 / Tam — talk, L15, prereq M2, listed on `quest_scout` (the chapter's dilemma)

| Field | Value |
|---|---|
| Offered by | Gret's tree only (`cutting → tam → accept_tam`) |
| Objectives | 1. talk `forest_spirit_medium` (`data.questObj.q_ancient_relic.nora`: 问诺拉在绳上听见了什么 / Ask Nora what she heard on the rope) · 2. talk `forest_hermit` (`data.questObj.q_ancient_relic.bran`: 问布兰放他下来会怎样 / Ask Bran what cutting him down will do) |
| Rewards | 200 exp / 80 gold + choice [jewelry, gloves] with `choiceLabels`: jewelry = 「留他在树上」 Leave him in the tree (Gret gives you Tam's ring); gloves = 「割断绳子」 Cut the rope (Gret gives you his cutting gloves) |
| World state [R3] | `choiceFlags: ['hw_tam_left', 'hw_tam_cut']` (aligned with `choices`) |

Its prerequisite is M2, not M3: both talks then fall on visits other quests already make (Nora on the south loop, Bran
at M3's turn-in), instead of a 190-tile errand of its own.

**The dilemma under the one rule of §1.** Tam's rope lets go for good only if someone else is put in his place.

| Choice | Until the cleanse | After the cleanse |
|---|---|---|
| **Cut** (`hw_tam_cut`) | Tam is buried and stays buried. The next morning the head-count is one short: **Old Wick** is gone from the mill (`mill_wick.visibleWhen {notFlag: hw_tam_cut}`), and the bundle on the Cutting Oak's east bough is a newer, cleaner one (the set piece's `hw_tam_cut` variant, [R5]). Bess's and Gret's lines (§3.2) | Wick came down with the rest, dead. Under the other choice he would be counting sacks. Gret keeps both names |
| **Leave** (`hw_tam_left`) | Tam hangs. Wick counts sacks. Gret looks up when she passes; a ring of field flowers lies under the bough (set-piece variant) | Tam is the last one down, and Gret is under the bough to take him |

The price is a person the player has passed at every visit, not a spawn table, and it does not go away when the
chapter ends. Fallback without [R3]: the labelled choice only, Wick stays, and one after-line for Gret keyed on the
turn-in: 「割，还是不割，都是我的事了。你替我问过了。」 / "Cut or not, it's mine to carry now. You asked for me."

What Nora says (her K6 line gated `{quest: 'q_ancient_relic', state: 'active'}`): 「塔姆。老橡树，朝东那根枝。他说：别让
我姐来。」 (Tam. The old oak, the bough that faces east. He said: don't let my sister come.) What Bran says, same gate:
「规矩是真的。绳子要真松手，就得另有一个人顶上。我提灯二十年，靠的就是这条规矩。」 (The rule is real. For a rope to let
go for good, someone else has to be put in the place. I carried a lamp for twenty years on the strength of that rule.)

| Line | zh-CN | en |
|---|---|---|
| offer | 诺拉说，她在绳上听见了塔姆的名字。我不敢去问。你替我问她。再问问布兰：把他放下来，会怎么样。 | Nora says she heard Tam's name on a rope. I can't make myself ask. Ask her for me. And ask Bran what happens if he's let down. |
| progress | 我每晚都从那棵树下过。我没抬过头。 | I walk under that tree every night. I have never looked up. |
| complete | 放他下来，绳子就要另找一个人……你说吧。割，还是不割。我听你的。 | Let him down and the rope takes someone else... You say it. Cut, or don't. I'll do what you say. |

### S5 `q_pet_owl` 月鸮 / The Moon-Owl — collect, L16, prereq M3, listed on `forest_hermit`

| Field | Value |
|---|---|
| Objectives | 1. collect `mat_moon_feather` × 4 (gather, area (86,50) r8, 5 nodes, itemKind `feather`) · 2. investigate `clue_hw_owl_hollow` 空心橡树 / The Hollow Oak (80,44) r2 |
| questArea | (84,48) r10 (kept) |
| Rewards | 240 exp / 120 gold + `petReward` `pet_owl` + 4 embers |

| Line | zh-CN | en |
|---|---|---|
| offer | 我熄灯那晚，只有一只猫头鹰没飞走，陪了我三十年。前些天狼一嚎，它惊了。顺着落羽找找。它怕灯，不怕你。 | The night I put my lamp out, one owl didn't fly off. It stayed thirty years. Then the wolf howled and it bolted. Follow the dropped feathers. It's afraid of lamps, not of you. |
| progress | 羽毛是银的，夜里自己发亮。林子里只剩这点真月光了。 | The feathers are silver and shine at night. It's the only true moonlight left in this wood. |
| complete | 它肯站你肩上？……那就跟你吧。我这儿，只剩那卷拓本可看了。 | It will sit on your shoulder? ...Then it's yours. All I have left to look at is the rubbing. |

### S6 `q_pet_cat` 影猫 / The Shadow Cat — collect + kill, L16, prereq M2, listed on `forest_spirit_medium`

| Field | Value |
|---|---|
| Objectives | 1. collect `mat_shadow_shard` × 3 (drop from `lamp_netter`, `lamp_warden`, chance .6, itemKind `essence`) · 2. kill `lamp_warden` × 1 |
| questArea | (70,50) r10 (moved from (40,88): the guide points at the Lamp Line) |
| Rewards | 260 exp / 150 gold + `petReward` `pet_cat` + 4 embers |

The shards used to drop from the Hanged and the Fenbound, which are gone for good once the Barrow is cleansed: a hero
who finished the chapter first could never earn the pet. Lamp-Bearers walk their line before and after (§8), so the
quest can be finished in any world state. Nine of them stand on the main path (the Lamp Line and the Chain Watch);
three shards at .6 from nine kills fail in under 3 % of runs, and both entries respawn.

| Line | zh-CN | en |
|---|---|---|
| offer | 有只猫，谁被吊上去，它就在树下蹲一夜。执灯人拿灯一照，把它的影子照碎了，碎片沾在他们的灯绳上。替它找回来。 | There's a cat that sits a night under every tree where someone was hung. The Lamp-Bearers shone a lamp on it and broke its shadow, and the pieces cling to their lamp-cords. Get them back for it. |
| progress | 执灯的走在浅滩东边的路上。三个网手，跟着一盏灯。 | The Lamp-Bearers walk the road east of the ford. Three netters behind one lamp. |
| complete | 影子拼上了。它蹲在你脚边呢——它只陪要走远路的人。 | The shadow is whole. It's sitting at your feet. It only keeps company with people who have a long way to go. |

### S7 `q_defend_camp_forest` 割绳之夜 / The Cutting Night — defend, L14, prereq M2, listed on `quest_scout`; **a prerequisite of M3**

| Field | Value |
|---|---|
| Objectives | 1. `defend_wave` at `defendTarget` 葛蕾的梯子 / Gret's Ladder (40,35), `totalWaves` 3, `approachDeg` 315 (the half-ring toward the Chantry) |
| Waves | 1: `hanged_one` × 5 · 2: `fenbound` × 4 + `hanged_one` × 2 · 3: `hanged_one` × 4 + `fenbound` × 3 (only families the hero has met in M1 and M2; the Fenbound's puddles close in round the ladder) |
| Rewards | 260 exp / 100 gold + `c_hp_potion_m` + permanent +3 stat points (割一个，算一个 / One Cut, One Counted, once per difficulty) |
| Triggers | turn-in → `cs_hw_dream` (the vision, §8) |

It keeps its side type and its web id, but it stands on the main path and is **not reacceptable**: it is the night
before Gret sends the hero to the Chantry, it puts the chapter's first permanent reward in its first half, and a quest
that says "tonight I cut the old oak" cannot be offered in a wood whose ropes have all let go (there is no field that
withdraws an offer on a flag; a prerequisite does it). Defend-wave kills pay exp and loot (the core withholds rewards
only from the Raised role); whether the wave scaling changes their exp is unverified (§18).

Gret is not on screen on the ladder (no figure works at a rope); the ladder, her hook and her lantern stand under the
oak and the waves pull at the ladder (the defend target's HP, today's formula). Bundles on the ground under the oak
after the last wave are set dressing.

| Line | zh-CN | en |
|---|---|---|
| offer | 今晚我割那棵老橡树，上面有九个。九根空绳，林子不会答应。它有什么，就派什么来。守住梯子，守到我割完。 | Tonight I cut the old oak. There are nine in it. Nine empty ropes: the wood will not stand for it, and it will send whatever it has. Hold the ladder until I'm done. |
| progress | 梯子要是倒了，今晚就白割了。 | If the ladder goes over, tonight's cutting is wasted. |
| complete | 九个，都下来了。天亮以前，他们是地上的人。……这就够了。 | Nine, all down. Until morning they are people on the ground. ...That is enough. |

### S8 `q_investigate_corruption_forest` 井水 / Well-Water — investigate, L14, prereq M1, listed on `forest_spirit_medium`

| Field | Value |
|---|---|
| Objectives | 1-3. investigate `clue_hw_lampole` 沉在水里的灯杆 / A Lantern-Pole Under the Water (15,60), `clue_hw_root` 扎进空坟的树根 / A Root Through an Empty Grave (20,25), `clue_hw_stream` 泛银的溪水 / The Stream Running Silver (50,40), r5 each · 4. investigate `clue_hw_spring` 静泉 / The Still Spring (30,90) r3 (`labelKey` `data.questObj.q_investigate_corruption_forest.shroud`: 把殓布盖在泉眼里的人身上 / Lay the shroud over the one in the spring) |
| questArea | (30,40) r25 (kept) |
| Rewards | 220 exp / 80 gold + permanent +20 max life (诺拉的滤水布 / Nora's Straining-Cloth, once per difficulty) |

It ends at the spring behind the shroud-yard with an act, not a kill: the midwife harmed no one, and the web's hunt
here (the Fen-Mother) is cut.

| Line | zh-CN | en |
|---|---|---|
| offer | 磨坊里又有人夜里往水边走。月病是喝出来的。顺着水找三处：灯杆、树根、溪水。看看是什么泡在水里。这块殓布，你带上。 | Someone else in the mill has started walking for the water at night. The moon-sickness comes from drinking. Follow the water to three places: the lantern-pole, the root, the stream. See what is lying in it. And take this shroud with you. |
| progress | 水发银光的地方，别喝。也别照。 | Where the water shines silver, don't drink it. Don't look into it either. |
| complete | 泡在泉眼里的，是当年被他们绑了石头推下水的接生婆。她没害人，是水记住了她。 | What lay in the spring was a midwife they tied to a stone and pushed in, long ago. She harmed no one. The water remembered her. |

## 7. Totals

| | Exp | Gold | Embers |
|---|---|---|---|
| Main (M1-M5) | 3480 | 455 | 2 × 4 + 6 = 14 |
| Side (S1-S8) | 1940 | 800 | 1 × 6 + 4 + 4 = 14 |
| **Total** | **5420** | **1255** | 28 from turn-ins |
| of which on the main path (M1-M5 + S7) | 3740 | 555 | — |
| Gret's tree, once | +80 | +50 | — |

Web today: 14700 / 1255. Gold is kept; exp is cut to fit the level band (§18): the web total alone would carry a
Lv 14 hero to Lv 23. Permanent rewards: +3 stat points (S7, on the main path), +20 max life (S8), +1 skill point (M5),
each once per difficulty. Quests listed per NPC: `quest_scout` 5, `forest_hermit` 3, `forest_spirit_medium` 3,
`forest_tracker` 2.

## 8. Story beats

**Boss build-up in the bible's four beats** (story-bible.md §7: named early; victims and orders; a vision; the intro):

1. **Named early.** His howl is in the arrival cutscene and Gret names him there; the mill has a bark for it; Hedda's
   first line says every hound in the wood is of his line.
2. **Victims and orders.** The Writ Stone, the Kennel Ruins and the echo on the path from the kennels to the oak
   (`lore_hw_echo_03`, 15 tiles from the dais, readable long before the fight), Ashcoat's burned eye, and the Chain
   Watch: living people who still feed him, standing on M4's road.
3. **A vision.** `cs_hw_dream`, the night after the Cutting Night: the priestess's flat hand, and the whisper's one
   sentence about how he came to eat.
4. **The intro** at 9 tiles, with the music cut inside 16 tiles of the oak.

### Chapter card (`story.chapter.twilight_forest.*`, mood night; keys kept, text rewritten)

| Field | zh-CN | en |
|---|---|---|
| number | 第二章 | Chapter Two |
| title | 被抹去的名字 | The Erased Name |
| subtitle | 绳林 | The Hanging Wood |
| text | 绳不空悬。月光几百年没有落过地，这片林子，不肯放它的死人走。 | No rope hangs empty. No moonlight has touched this ground in hundreds of years, and the wood will not let its dead go. |

### Cutscenes

Step kinds as chapter1.md §8; every cutscene has 5-14 steps; a `say` speaker and a `focus npc` target stands **where
the cutscene plays** (the draft's `focus npc forest_hermit` from the mill, 82 tiles away, is gone). The mini-boss and
the Barrow's keeper are not map spawns, so their beats narrate and focus a tile. Limits: say and whisper ≤ 60,
narrate ≤ 70 zh code points.

**`cs_hw_arrival`** (new) — trigger: `zone_entered` `twilight_forest`.

| # | Step | zh-CN | en |
|---|---|---|---|
| 1 | narrate | 东去的路钻进了林子。头顶的枝上垂着绳子，绳上裹着东西，裹得很仔细。 | The east road runs in under the trees. Ropes hang from the boughs overhead, and something is wrapped on each one, wrapped with care. |
| 2 | narrate | 没有风。有几个，却在慢慢地转。 | There is no wind. A few of them are turning, slowly. |
| 3 | whisper | 抬头看看，孩子。他们就是这样，对待听见别的声音的人。 | Look up, child. This is what they do to people who hear a different voice. |
| 4 | focus npc `quest_scout` | | |
| 5 | say `quest_scout` | 别抬头太久。它们在数人头。 | Don't look up for long. They're counting heads. |
| 6 | narrate | 林子深处传来一声长嚎。枝上的绳子，都跟着颤了一下。 | A long howl comes out of the deep wood. Every rope in the boughs trembles with it. |
| 7 | say `quest_scout` | 那是沃尔甘。别应。磨坊里有火，走路中间。 | That is Volgan. Don't answer it. There's a fire at the mill. Keep to the middle of the path. |

**`cs_hw_empty_rope`** (new) — trigger: `q_kill_undead` turned in. The one place the rule is said aloud.

| # | Step | zh-CN | en |
|---|---|---|---|
| 1 | focus npc `quest_scout` | | |
| 2 | say `quest_scout` | 林子里只有一条规矩：绳不空悬。割断的绳子，天亮前会把人拉回去。 | The wood has one rule: no rope hangs empty. A cut rope pulls its dead back before morning. |
| 3 | say `quest_scout` | 要它真松手，得有另一个人顶上。所以它们往下扑。不是恨你，是想下来。 | For it to let go for good, someone else has to be put in the place. So they drop on you. It isn't hate. They want to come down. |
| 4 | say `quest_scout` | 我夜里去割，割一个，埋一个。埋下去的，只在土里躺一夜。 | I go out at night and cut. Cut one, bury one. The ones I bury lie in the ground one night. |
| 5 | narrate | 北边的林子里，有人在唱歌。调子很慢，像领着一群人念经。 | In the wood to the north someone is singing. The tune is slow, like one voice leading many through a prayer. |
| 6 | say `quest_scout` | 有个修士赶在你前头从磨坊过，摇着手铃，说要去东边听清楚些。 | A monk came through the mill ahead of you, ringing a handbell. He said he was going east to hear more clearly. |
| 7 | say `quest_scout` | 现在灯祠里有人领唱。树上的，都跟着他唱。 | Now someone leads a chant in the Chantry, and the ones in the trees sing after him. |
| 8 | whisper | 他来了。他比你，更早听懂了我。 | He came. He understood me sooner than you have. |
| 9 | say hero | 奥德温。 | Aldwin. |

**`cs_hw_dream`** (new; the vision) — trigger: `q_defend_camp_forest` turned in.

| # | Step | zh-CN | en |
|---|---|---|---|
| 1 | narrate | 那一夜你睡在磨坊的火边。梦里没有火，只有一棵树。 | That night you sleep by the mill fire. There is no fire in the dream, only a tree. |
| 2 | narrate | 树下跪着一个人，一只手平伸着，停在狼头的高度。狼很瘦。它没有看她的手。 | Someone kneels under it with one hand held out flat, at the height of a wolf's head. The wolf is thin. It is not looking at her hand. |
| 3 | whisper | 她求它别吃。它饿到第九天，才吃。我只告诉了它一句：她不会回来了。 | She begged him not to eat. He held out nine days. I told him one thing only: she is not coming back. |
| 4 | narrate | 你醒了。远处有一声长嚎，很久才断。 | You wake. Far off there is a howl, and it is a long time ending. |
| 5 | focus npc `quest_scout` | | |
| 6 | say `quest_scout` | 你也听见了？……灯祠里那个人，今天该去见见了。 | You heard it too? ...It's time you went to see the man in the Chantry. |

**`cs_hw_cantor`** (new) — trigger: `monster_killed` `miniboss_cantor`, `whileQuest` `q_talk_hermit`. It is built on
what only Aldwin has (he knew the hero and the Knellwatch dead) and ends on the hero's act, so that it does not replay
the Corpseherd's scene (kneels, the bell, the dead stop one by one, the whisper explains).

| # | Step | zh-CN | en |
|---|---|---|---|
| 1 | focus tile (55,30) | | |
| 2 | narrate | 领唱的人跪倒在灯祠的石阶上。他还在念，念的是钟垒的名字。 | The cantor goes down on his knees on the Chantry steps. He is still reciting, and the names are Knellwatch's. |
| 3 | narrate | “……佩尔。提灯的那个孩子。你看，孩子，我一个也没忘。” | "...Pell. The boy with the lantern. You see, child, I have not forgotten one." |
| 4 | narrate | “那声音说：不放他们走，就一个也忘不了。” | "The voice said: never let them go, and not one can be forgotten." |
| 5 | say hero | 经上没有这一句。 | There is no such line in the book. |
| 6 | narrate | 他怀里滑出一卷拓本：石上拓下来的六行字，第六行是一块空白。 | A rolled rubbing slides from his coat: six lines taken from a stone, and the sixth is a blank. |
| 7 | whisper | 他听得比谁都清楚。只是从没问过，说话的是谁。 | He heard better than anyone. He never once asked who was speaking. |
| 8 | narrate | 你捡起他的手铃。它不响了。树上的歌，也停了。 | You pick up his handbell. It has stopped ringing. So has the singing in the trees. |

Aldwin never has a pronoun for the voice (not 祂: that is a real church's word, and his whole fault is that he never
asked who it was). He says 那声音.

**`cs_tf_hermit`** (kept id, rewritten) — trigger: `q_talk_hermit` turned in, which now happens **at the Dark
Lantern**. It tells the player only what the prologue did not: the name, how it survived, and whose words the
scripture uses.

| # | Step | zh-CN | en |
|---|---|---|---|
| 1 | focus npc `forest_hermit` | | |
| 2 | narrate | 老人把拓本摊在经台上，翻了过来。背面有一行小字，墨迹盖着墨迹。 | The old man spreads the rubbing on his lectern and turns it over. On the back is one small line, ink laid over older ink. |
| 3 | say `forest_hermit` | 最后一个月祭司上树之前，在背面写了个名字。后来的抄录人都照着描，没人敢念。 | Before they took the last moon-priestess to the tree she wrote a name on the back. Every copyist since has traced it. None dared read it. |
| 4 | say `forest_hermit` | 我是最后一个抄录人。我念了。当晚，我把灯熄了。 | I was the last copyist. I read it. That night I put my lamp out. |
| 5 | say `forest_hermit` | 伊格纳罗斯。第六个守火人。 | Ignaroth. The sixth keeper. |
| 6 | say `forest_hermit` | 我们的经，是九百年前有人从火堆里听来的。那声音管封印叫“灯”，管人叫“孩子”。 | Our scripture was heard in a pyre nine hundred years ago. That voice calls a seal a "lamp", and calls people "child". |
| 7 | say `forest_hermit` | 我照着那本经，吊了二十年人。 | I hung people by that book for twenty years. |
| 8 | shake | | |
| 9 | whisper | 他只漏了一句，孩子：我说的每一个字，都是真的。 | He left out one thing, child. Every word I said was true. |
| 10 | say `forest_hermit` | 它还在跟你说话，是不是？……别答我。去看看那条狼。它听了几百年。 | It is still talking to you, isn't it? ...Don't answer me. Go and look at the wolf. He has listened for hundreds of years. |

Bran does not say whose place the brand's empty space is; the bible keeps that for Chapter 4.

**`cs_boss_volgan`** (boss intro; `story.boss.volgan.name` / `.epithet`) — the boss scan at 9 tiles.

| # | Step | zh-CN | en |
|---|---|---|---|
| 1 | focus monster `volgan` (`anim: roar`) | | |
| 2 | narrate | 一声长嚎。林子里没有月亮，它却仍朝着天上叫。 | A long howl. There is no moon over the wood, and still he calls up at the sky. |
| 3 | title | 噬月者·沃尔甘 / 拴在绞架下的圣狼 | Volgan the Moon-Eater / The Sacred Wolf Under the Gallows |
| 4 | say `volgan` | ……月亮……不答应了。 | ...The moon... no longer answers. |
| 5 | say `volgan` | 他们喂我。我吃了。……我都吃了。 | They fed me. I ate. ...I ate all of it. |
| 6 | whisper | 瑟莲娜最忠的一条狼。忠心是好东西，孩子——换个主人就行。 | Selenne's most loyal wolf. Loyalty is a fine thing, child. You only have to change the master. |
| 7 | say hero | 链子，我替你解。 | I'll take the chain off you. |

**`cs_tf_moonfang`** (kept id, rewritten; grants `pet_storm_wolf`) — trigger: `monster_killed` `volgan` (no
`whileQuest`: the cub is granted on the first kill, as the web rule and `EmberTower.retroGrantPets` expect). The hero
keeps the promise of the intro's last line here.

| # | Step | zh-CN | en |
|---|---|---|---|
| 1 | focus tile (90,90) | | |
| 2 | narrate | 沃尔甘伏倒在树根上。齿缝间那点冷光，一丝一丝，沉进了土里。 | Volgan sinks onto the roots. The cold light between his teeth drains, thread by thread, into the ground. |
| 3 | say `volgan` | ……月亮……回来了吗？ | ...Has the moon... come back? |
| 4 | say hero | 快了。 | Soon. |
| 5 | narrate | 你掰开它颈上那只断了的项圈。几百年来，它第一次没有朝天嚎叫。 | You open the broken collar at his neck. For the first time in hundreds of years he does not howl at the sky. |
| 6 | narrate | 树根的阴影里，钻出一只银灰的幼狼，额上一弯白纹。 | Out of the shadow of the roots creeps a silver-grey cub with a pale curved mark on its brow. |
| 7 | title | 月牙 / 沃尔甘之后 | Sliver / Last of Volgan's Line |
| 8 | narrate | 它嗅了嗅你掌心的烙印，没有逃走。 | It sniffs the brand on your palm and does not run. |
| 9 | say hero | ……跟我走吧。 | ...Come with me. |
| 10 | whisper | 你看，孩子。连狼，也愿意跟着持火的人。 | You see, child. Even a wolf will follow the one who carries the fire. |

**`cs_hw_barrow`** (new) — trigger: `zone_entered` `sub_hw_barrow`. The keeper has a name by now (Bran's tree, the
notes at the Dark Lantern, M5's offer), and his two words here are the ledger's.

| # | Step | zh-CN | en |
|---|---|---|---|
| 1 | narrate | 橡树的根底下是空的。根须垂下来，每一条都系着东西。 | Under the oak the ground is hollow. The roots hang down, and something is tied to every one. |
| 2 | focus player | | |
| 3 | narrate | 深处有一盏灯。黄的，不暖。提灯的手，已经长进了树里。 | There is a lamp far in. Yellow, and not warm. The hand that holds it has grown into the tree. |
| 4 | narrate | 有人在念，一个字一个字：“已记，未放。已记，未放。” | Someone is reciting, a word at a time: "Recorded. Not released. Recorded. Not released." |
| 5 | whisper | 威斯坦怕死，我给了他一棵树。几百年了，他一个人也没放走。 | Wystan was afraid to die, so I gave him a tree. In hundreds of years he has not let one person go. |
| 6 | wait 800 ms | | |

**`cs_hw_seal`** (new, [R2]) — trigger: world flag `hw_barrow_cleansed` set (the keeper is dead and the Barrow is
clear; the hero stands in the Seal Chamber). It does not repeat Chapter 1's seal beat line for line (no "The Nth
Flame" title, no dead lying down "one after another"), and the hero does not answer the whisper: the saga's turn
("the hero has been carrying the key") and the refusal belong to Chapters 5 and the ending.

| # | Step | zh-CN | en |
|---|---|---|---|
| 1 | focus tile (24,21) | | |
| 2 | narrate | 提灯的手松开了。灯落进井里，没有声响。 | The hand on the lantern opens. The lamp drops into the well without a sound. |
| 3 | narrate | 你把烙着印的手按在井沿上。井水静下来，水里慢慢浮起一轮月亮。 | You lay your branded hand on the lip of the well. The water stills, and a moon rises slowly in it. |
| 4 | flash | | |
| 5 | title | 月辉之印 / 井中有月 | The Moon Seal / A Moon in the Well |
| 6 | narrate | 头顶的根须松了。裹着的人落下来，轻得像布。 | The roots overhead let go. The wrapped ones come down, light as cloth. |
| 7 | whisper | 两盏灯了。你走得真快，孩子。 | Two lamps. You walk so fast, child. |
| 8 | wait 800 ms | | |
| 9 | narrate | 你掌心的烙印，亮了第二处。你把手攥了起来。 | A second place on your brand is lit. You close your hand over it. |

Fallback without [R2]: steps 3, 5 and 6 are folded into the front of `cs_tf_finale` as recalled narration (10 + 3 = 13
steps), and nothing plays in the Barrow at the cleanse.

**`cs_tf_finale`** (kept id, rewritten) — trigger: `q_seal_dark_source` turned in, at the mill; this zone's chapter
finale. Only Gret speaks: she is the one standing there.

| # | Step | zh-CN | en |
|---|---|---|---|
| 1 | focus npc `quest_scout` | | |
| 2 | say `quest_scout` | 你回来了。……你看外面。 | You're back. ...Look outside. |
| 3 | narrate | 几百年来第一次，月光穿过枝叶，落在绳林的地上。 | For the first time in hundreds of years moonlight comes through the leaves and lies on the floor of the Hanging Wood. |
| 4 | narrate | 枝上的绳子都松了。地上的人，没有再站起来。 | Every rope in the boughs has let go. The ones on the ground have not got up again. |
| 5 | say `quest_scout` | 今晚我不用出门了。……我不知道夜里还能干什么。 | I don't have to go out tonight. ...I don't know what else there is to do at night. |
| 6 | say `quest_scout` | 布兰托人带话：那个名字，他抄了一遍，贴在门上了。 | Bran sent word. He wrote that name out and put it on his door. |
| 7 | whisper | 贴在门上？……好。那就让人人都念。 | On his door? ...Good. Then let everyone read it. |
| 8 | say `quest_scout` | 东边山里来了人，说锁砧山的炉子还在响，可打铁的早就死了。 | Someone came down from the eastern hills. He says the forges of the Chained Anvil are still ringing, and the smiths have been dead a long time. |
| 9 | say `quest_scout` | 我去你那座塔。带一桶井水，再带上这只小的。那里的绳子，只用来打水。 | I'll go to that tower of yours. I'll take a bucket of the well's water, and this little one. Rope there is only for drawing water. |
| 10 | narrate | 你握紧发烫的掌心，朝东边的山走去。 | You close your burning hand and walk toward the hills in the east. |

Step 7 is the chapter's sting: the villain is glad to be read, by anyone, for any reason. Step 9 is said whether or
not the tower is visible, as Chapter 1's finale already does (`story.cs_rf_finale.12`).

### Triggers

| On | Subject | Cutscene |
|---|---|---|
| zone_entered | `twilight_forest` | `cs_hw_arrival` |
| quest_turned_in | `q_kill_undead` | `cs_hw_empty_rope` |
| quest_turned_in | `q_defend_camp_forest` | `cs_hw_dream` |
| monster_killed | `miniboss_cantor` (`whileQuest` `q_talk_hermit`) | `cs_hw_cantor` |
| quest_turned_in | `q_talk_hermit` | `cs_tf_hermit` |
| monster_killed | `volgan` | `cs_tf_moonfang` (`grantPet` `pet_storm_wolf`) |
| zone_entered | `sub_hw_barrow` | `cs_hw_barrow` |
| flag_set [R2] | `hw_barrow_cleansed` | `cs_hw_seal` |
| quest_turned_in | `q_seal_dark_source` | `cs_tf_finale` |

### Saved world state

| Flag | Set by | Effect | Fields used |
|---|---|---|---|
| `rf_chapter_dawn` (Chapter 1's) | `q_rf_last_toll` turned in | the east gate of the Red Field opens [R1]; the barricade set piece shows its opened variant [R5] | sealed-gate condition |
| `hw_cantor_silenced` | M3 turned in | the chant bed over the north wood stops; Aldwin's handbell hangs on the mill door [R5]; Gret's line | UE audio reads `Snapshot::worldFlags`; K6 `lines` |
| `hw_volgan_down` | M4 turned in | the howling stops; the Wolf Run and Ashcoat's Den drop to three leaderless hounds; Hedda's line | spawn `variants` |
| `hw_barrow_cleansed` | M5's `clear_area` (the sub-dungeon's `cleansedFlag`) | **moonrise**: the Barrow is empty and silver-lit for good; `cs_hw_seal` [R2]; on the surface the mood `hangwood_dawn` ([R1] `dawnFlag`), mist thins, the Hanged and the Fenbound are gone (variants below), the hung bundles lie on the ground (set-piece variants [R5]); Mosswick thins; Volgan stays farmable once per visit (M7) | `cleansedFlag`, spawn `variants`, set-piece `when`, mood |
| `hw_tam_cut` / `hw_tam_left` | S4's choice [R3] | §6 S4 | `choiceFlags`, K6 `lines`, `visibleWhen`, set-piece `when` |

There is no separate turn-in flag: NPC lines that follow the finale use the quest state (`{quest:
q_seal_dark_source, state: turnedIn}`). That spawn variants, set pieces and the mood may **read** a sub-dungeon's
`cleansedFlag` is unverified (types.ts only forbids another sub-dungeon or a quest's `setsFlags` from naming it);
if it is refused, M5's turn-in sets `hw_chapter_moonrise` and everything keys on that, at the cost of the walk home.

Spawn variants (K6; one variant per flag, the first set flag in list order wins, so `hw_barrow_cleansed` is listed
first): the Low Boughs → `moon_hound` × 3 (needs [R4]: a variant must drop the entry's `dormant` block; fallback
count 0); the Middle Boughs → count 0; the Sunk Wheel → count 0 (the pond is still); Mosswick → `moonsick` × 3; the
Wolf Run and Ashcoat's Den → `moon_hound` × 3 on `hw_volgan_down`. The Kennel Ruins, the Lamp Line and the Chain Watch
do not change: the long-turned do not come back and the Lamp-Bearers still walk their line ("residents are scarred,
not cured"). **No quest can be stranded by a flag**: after the cleanse the only quests whose sources are gone are M2
(a prerequisite of everything that leads to the cleanse) and S7 (a prerequisite of M3); the content test pins it.

## 9. Sub-area and descent

### 9.1 The Gallows-Oak (surface region `gallows_oak`)

A 14-tile mood region round (90,90): darker, a moon-tarnish uplight from between the roots, no ambient chant, music
silent inside 16 tiles until the intro. No regular entry stands inside it (the Chain Watch is 17.5 tiles off and the
Kennel Ruins 20, both through `at`).

- **The oak blocks because it stands on a wall.** A story decoration never blocks (the generator keeps its tile
  walkable and the blocking pass skips story decorations), and a set piece does not change the grid. The trunk is
  therefore the set piece `setpiece_gallows_oak`, `onWall`, on the wall tile **(90,87)**, three tiles north of the
  dais, facing it; the crown is non-blocking with the occluder fade. No generator input is added for it. Unverified:
  that a wall overlay (authored for the crypt kit) is accepted on a generated zone; if not, the trunk is the
  `hangwood` mesh of that wall tile's own tall decoration.
- **The arena is not a clear disc.** On today's grid 15 wall tiles lie within 8 tiles of the dais (6 within 6). They
  are dressed as root knuckles and are part of the fight: cover from Chain Slam's line of approach, obstacles for a
  pounce. The content test pins that every walkable tile within 10 tiles of the dais is reachable from the dais.
- Other set pieces: a ring-bolt and **the chain** from the trunk to the dais tile (walkOver: a ground layer); the
  feeding ground, a 4-tile ring of torn cloth round a hollow worn into the earth; eleven bundles in the lower boughs.
- **The Barrow mouth** is at (79,95), 12.1 tiles from the dais, and its return tile at (80,96), 11.7: outside
  Volgan's aggro (9), so leaving the Barrow after the chapter does not start a fight with a farmable boss.
- **No camp inside the fight.** Volgan's bound fight reaches max(8, 10 + 2.6) = 12.6 tiles from the dais. The Crow
  Carts' safe circle (r4, §3.1) begins 14.0 tiles out. The content test pins the general rule: no camp's safe circle
  intersects a bound fight's reach (Chapter 1 moved Valdras's leash for the same class of defect).

### 9.2 根冢 / The Root Barrow (`sub_hw_barrow`)

A hand-authored 36 × 26 `subDungeons` record with `tiles` (K7), entrance `entrance_hw_barrow` at (79,95), exit back to
(80,96). `levelRange` [18, 19]. `presentationTheme: 'barrow'` (parent `hangwood`, [R7]), `cleansedFlag:
'hw_barrow_cleansed'`. No respawn, no random events, death returns to the Hollow Mill with the penalty permanent (the
sub-dungeon rule). It is on the main path: DECISIONS DR7 kept Chapter 1's critical path off the descent only because
the runtime did not exist; it exists now.

**What it is for.** The chapter's climax fight is Volgan, on the surface, because he is chained to the oak and that
is who he is. The Barrow is the **ending**: three rooms, one named man, the seal. It holds the story bible's third
figure, "the servant who refused an ending". It is short on purpose: a sub-dungeon's monsters stand at their own
levels (nothing is raised), so a long descent filled with the surface roster would repeat the Pit's measured defect.
The gallery is a set piece fought at the roster's level (Lv 14 against a Lv 18-19 hero), as the Pit's rooms are; **the
keeper is the fight**.

| Room | zh | Tiles (col, row) | Contents |
|---|---|---|---|
| The Root Stair | 根阶 | 2-10, 2-8; stair alcove 4-6, 9-10 | arrival (5,5); the exit (5,10) in a three-tile alcove two deep (the Pit's fix: its 1.5-tile trigger reaches no tile of the room). Nobody stands here: `cs_hw_barrow` plays to an empty room |
| passage | | 11-16, 5-6 | empty; root curtains on both walls |
| The Hanging Gallery | 悬根廊 | 17-31, 4-8 | six `hanged_one`, one dormant entry with `line {dCol: 2, dRow: 0}` from (20,6): they hang at (20,6) (22,6) (24,6) (26,6) (28,6) (30,6), `wakeRange` 2.5, `rise`. The way on is the door at (30-31, 9). Every tile of the gallery from col 19 east is at most 2.24 tiles from a member, so the gallery cannot be walked without the whole line dropping (the content test walks every tile; 2.24 against 2.5 leaves margin, unlike the Pit's first 3.000) |
| corridor | | 30-31, 9-13 | |
| The Seal Chamber | 月井室 | 16-32, 14-23 | `sub_rootbound` at (24,18), grown into the tap root that goes down into the well. His guard (the entry's `guard`): two `hanged_one`, dormant, `line {dCol: 6, dRow: 0}` from (21,17), so they hang at (21,17) and (27,17) and drop when he gives the alarm. The Moon Well `setpiece_barrow_moon_well` at (24,21) on a paved dais (22-26, 20-22), the far wall behind it. The lectern `story_hw_ledger` at (18,15) (§12). No tile of the chamber is more than 9.5 tiles from him |

Eight monsters and the keeper. **Cleanse**: keeper dead and at most 2 hostiles left → `clear_area` completes, the flag
is set, what still stands collapses (the Pit's rule), the root glow turns from moon-tarnish to silver, the well
lights, `cs_hw_seal` plays [R2]. The Barrow kit is reused for a Labyrinth floor theme later.

**The keeper: 缚根首灯·威斯坦 / Wystan, the Rootbound First Lamp (`sub_rootbound`).** The first of the Lamp-Bearers,
and the one who feared death. The voice told him there was room in the tree. He is still alive in the root, still
holding his lantern up, still saying each name and writing it in the Lamp Ledger: none forgotten, none released,
exactly as the scripture promised. Every Lamp-Bearer was taken down to stand before him on the day they took the cord
(Bran's tree); his own notes lie at the Dark Lantern (`lore_hw_03`); the ledger's two words are the first thing heard
in the Barrow. L19, 4000 HP (placeholder), `packAggro` 12, `corpse {leaves: 'remains'}`, elite with one affix,
`isSubDungeonMiniBoss`.

| Mechanic | Spec |
|---|---|
| Rooted | `tether {radius: 1.5}` on the definition [R6]: he is bound to his tile from the moment he notices the hero (speed 24 only lets him turn and lean), and M1 never resets a bound fight. Fallback without [R6]: phase row `{hpBelowPct: 100, leash {radius: 1.5}}`, which is in force only once his life is below 100 %; until the first hit he leans toward the hero at 0.72 tiles/s, which is accepted |
| 照 Lantern Beam `lantern_beam` | kind `line`, 10 × 1.2 toward `heroAtWindup`, `Cast01` at 900 ms, × 1.4, fire, `rangeMax` 10, cooldown 6 s |
| 根 Root Burst `root_burst` | kind `circle`, r 1.8 at `heroAtWindup`, clip **`Cast03`** at 800 ms (his free hand closes; the ground decal is the tell), × 1.2, slow 40 % for 2 s, `rangeMax` 10, cooldown 5 s, first after 2 s |
| 抽 Root Sweep `root_sweep` | kind `cone`, 160°, r 3, `Attack02` at 700 ms, × 1.3, cooldown 5 s. No `rangeMax` on purpose: it is used only on a hero inside 2.64 tiles |
| 灯灭 Lights Out `lights_out` | phase row `{hpBelowPct: 50, enterAction: 'roar', attackSpeedMul: 1.2, abilitiesOn: ['lights_out'], summons: [{monsterId: 'hanged_one', count: 2, at: 'anchors', anchors: [(20,20), (28,20)], delayMs: 1200, respawns: 0}]}`. Kind `ring`, outer **10**, inner 2.5, `Cast02` at 1000 ms, × 1.5, **`rangeMax` 10**, cooldown 10 s. The ring covers every tile of the chamber, so **every class has to come to him**: the only safe ground is the 2.5-tile disc at his root, which is where Root Sweep is, and where the two Hanged that drop at 50 % arrive. The Cantor's lesson, in the dark, with no way to stand off. UE dims the lamp pools for the windup (presentation) |
| Fight targets | §18 |

The draft's keeper could be shot from 7.5 tiles for the whole fight (its ring ended at 7 and had no `rangeMax`, so a
ranged hero never saw it at all).

## 10. Mini-boss: 绳林领唱 / the Cantor of the Hanged (`miniboss_cantor`, the Chantry of Lamps (55,30))

**Who he is.** Brother Aldwin. He said a prayer for every name until he feared he would forget whom it was for
(chapter1.md §3); the voice gave him an answer: do not let them go. He wears the same cassock, now with a stole of
knotted rope-ends, and rings the same handbell. He took the rubbing from the Chantry's lectern because it was a list
of names. Moon-tarnish pinpoint eyes; family `wooddead` (the marker rule, as the Corpseherd is `unburied`).

**Arena.** The roofless chantry: flagstones laid as ground dressing (walkOver) round him, seven lantern poles,
(`story_hw_lantern_row` is the row outside). It is **not a clear disc**: 21 of the 149 tiles within 7 tiles of him
are wall on today's grid, and they are dressed as the chantry's broken walls. Six shrouded bundles are laid in an arc
south of him as `corpseSeeds` of `hanged_one` at (52,32) (53,33) (54,34) **(55,34)** (57,33) (58,32): the ones Gret
buried, back again, waiting for the verse. (The draft's fourth seed, (56,34), is a wall tile; neither the exporter
nor the core checks a seed's walkability, so it would have raised a Hanged inside a wall. The exporter check is part
of 4A.)

**Pre-fight lines** (`data.miniBossDialogue.miniboss_cantor.line1..3`; hold him until dismissed):

| # | zh-CN | en |
|---|---|---|
| 1 | 是你啊，孩子。来，站近些，一起唱。 | It's you, child. Come, stand closer. Sing with us. |
| 2 | 我替每个名字念经，念到忘了念给谁。那声音说：那就别让他们走。 | I said a prayer for every name until I forgot whom it was for. The voice said: then do not let them leave. |
| 3 | 你听。钟垒的，绳林的，都在。一个也没有被忘记。 | Listen. Knellwatch's, the wood's: they are all here. Not one has been forgotten. |

**Fight.** L17, 2400 HP (placeholder), ranged (range 5.0), elite with one affix, once per visit.

| Mechanic | Spec |
|---|---|
| Bell bolt | the basic attack: a moon-tarnish ripple (`projectileColor` #9FE0C8) every 1400 ms; on hit slow 20 % for 2 s |
| 领唱 Lead the Verse `lead_verse` | kind `raise`, aim `corpse`, radius 7, `Cast01` at 900 ms, stands the corpse as a Raised `hanged_one` at 50 % HP (the `Rise` clip, §4.2); `capAlive` 3, cooldown 8 s. A second raise `open_verse` (two bodies, cooldown 60 s, no initial cooldown) is the opener, as Call the Flock as built |
| 和声 Chorus | `aura {stat: 'attackSpeed', mul: 1.2, radius: 6, family: 'wooddead'}`: the Raised near him strike faster; drawn as a ring of bell ripples (non-damaging: moon-tarnish, not crimson) |
| 止声 The Rest `the_rest` | below 50 % life: kind `ring`, outer 5, inner 2, `Cast02` at 900 ms, × 1.2 arcane, slow 30 % for 3 s, cooldown 12 s. Step **in** to him or out past 5. No `rangeMax`: his swing reach is 6.0 (range 5.0 × 1.2), which already covers the ring |
| Kite | `kite {min: 3, max: 5}` |
| Counterplay | kill him first; a fire killing blow or a fire skill over a bundle burns it (unraisable); on his death every Raised collapses |

## 11. Boss: 噬月者·沃尔甘 / Volgan the Moon-Eater (`volgan`, storyBoss, the Gallows-Oak)

**Look.** A wolf the size of a cart horse (scale × 2.2 on the hound rig, shoulder 1.65 m), starved long in the leg,
his silver coat gone to slate and black. A broken temple collar with the Well-Ring; from it **a chain** runs to the
ring-bolt in the oak. Between his teeth a cold moon-tarnish light: he swallowed the seal's light when the moon stopped
answering. No gore, no exposed bone; what he was fed is told (the offer, the dream, the echo, his two lines), never
shown.

**Numbers** (placeholders; 4T sets them from the measured hero, §4.0): L19, 14000 HP, damage 36, defence 20, speed
62, aggro 9, range 2.2, one bite every 1100 ms (`Attack01` has **one** Contact at 480 ms; the second snap in the clip
is follow-through with no notify, since a basic swing has one contact and one damage roll). Rules: no invulnerable
phase; hit cap 35 %; every special winds up ≥ 700 ms with a crimson decal; a bound fight, leashed to the dais over
max(8, 10 + 2.6) = 12.6 tiles.

**Nothing in this fight grows with time.** The draft's hound respawns (every 20 s while the phase held) and its 45 s
ring gave the slowest class the most of each mechanic: a rogue would have met nine hounds and fought most of the last
phase at the ring's smallest size, a warrior three hounds and a wide ring. Hound waves are keyed to his life and the
ring closes in a fixed 20 s, so every class meets the same seven hounds and the same ring.

| Act | HP | Core rows (K2 / K4 fields) |
|---|---|---|
| **锁链 / The Chain** | 100-60 % | Phase 0. `tether {radius: 5}` on the definition [R6]: he cannot leave the 5-tile circle of the chain, from the first moment. **锁扫 Chain Sweep** `chain_sweep`: `cone` 140°, r 3.2, `Attack02` at 800 ms, × 1.3, cooldown 5 s (no `rangeMax`: for a hero in reach). **咬合 Snap** `snap`: `line` 4 × 1.4 toward the hero, clip `Snap` at 700 ms, × 1.6, bleed `value` 8 per tick for 3 s, `rangeMax` 4, cooldown 7 s. **链砸 Chain Slam** `chain_slam`: `circle` r 2 at `heroAtWindup`, `Cast01` at 900 ms, × 1.5, slow 30 % for 2 s, `rangeMin` 4, `rangeMax` 9, cooldown 8 s: the chain whips out past the tether, so standing outside the circle is not safe ground |
| **断链 / The Chain Breaks** | 60-30 % | Three rows, phases 1-3. Row 1 `{hpBelowPct: 60, enterAction: 'roar', speedMul: 1.15, abilitiesOff: ['chain_slam'], abilitiesOn: ['pounce', 'howl'], summons: [{monsterId: 'moon_hound', count: 3, at: 'anchors', anchors: [(85,91), (91,95), (94,87)], delayMs: 1200, respawns: 0}], feed {radius: 4, healPct: 1.5, delayMs: 1500}}`. Row 2 `{hpBelowPct: 50, speedMul: 1.15, summons: [… count: 2, anchors: [(85,91), (94,87)] …], feed {…}}`. Row 3 `{hpBelowPct: 40, …}` the same with anchors [(91,95), (85,91)]. A row's multipliers and its `feed` hold only while it is the current phase, so each row repeats them; ability switches carry over. The roar snaps the chain (a phase row has no tether). **Seven hounds in all, for every class.** **扑杀 Pounce** `pounce`: `line` 6 × 1.6, `dash`, `rangeMin` 3, `rangeMax` 6, clip `Pounce` at 800 ms, × 1.6, cooldown 7 s. **嚎月 Howl** `howl`: `call`, radius 12, family `moonbitten`, `attackSpeed × 1.2` for 5 s, clip `Cast02` at 1000 ms, cooldown 14 s. **Feed**: a summoned hound that dies within 4 tiles of the dais is eaten 1.5 s later and heals him 1.5 % (at most 10.5 % over the fight): kill hounds away from the roots (M4's progress line says so) or burn the body first. Chain Sweep and Snap continue (the broken chain trails) |
| **月蚀 / Eclipse** | < 30 % | Row 4 `{hpBelowPct: 30, enterAction: 'roar', speedMul: 1.25, attackSpeedMul: 1.25, abilitiesOn: ['moonbite'], arena {kind: 'ringShrink', fromR: 10, toR: 6, overMs: 20000, dpsPctMaxHp: 4, damageType: 'arcane'}}`. The light in his mouth goes out and so does the wood: UE darkens everything outside the arena ring to black and the hero's own light radius becomes the main light (presentation on `EvMonsterPhase` 4 and `EvArena`; no core change). Outside the ring: 4 % of max life per second. **噬月 Moonbite** `moonbite`: kind **`circle`**, r 2.4 at `heroAtWindup`, `Cast01` at 900 ms, × 1.7, `rangeMax` 8, with a `hazard {durationMs: 4000, tickMs: 500, tickDmgMul: 0.15, damageType: 'arcane'}` block for the moon-tarnish pool it leaves (a ground strike that leaves a hazard, as Valdras's Oath-Brand Slam; a `hazard`-kind ability may not carry a contact hit). Cooldown 9 s. No new summons; hounds already up stay |
| Death | 0 | He lies down on the roots; the light drains into the ground → `cs_tf_moonfang`. `corpse {leaves: 'remains'}`. Loot: the story-boss floor |

`EvMonsterPhase` numbers as authored: 1 the chain breaks (stinger), 2 and 3 the second and third hound waves, 4 the
eclipse (stinger, darkening). Without [R6] the tether is a row at `hpBelowPct: 100`, every number moves up by one,
the bar shows a tick at 100 %, and until the first hit he is not tethered (aggro 9, speed 62): the fallback sets his
aggro to 5 so that he does not leave the chain's circle before he is touched, and accepts a melee hero who walks in
and out without striking.

Feed takes the body of a Raised hound although `moon_hound` leaves `remains` (the core's feed holds a corpse without
testing its kind). monsters.md's corpse rule says remains are never eaten; 4G amends that sentence ("except a phase
`feed`, which takes its own summons").

**The lesson chain.** The Low Boughs teach "look up"; the Cutting Night teaches holding ground while puddles close;
the Cantor teaches "kill the raiser, and the safe ground may be next to him"; the Lamp Line and the Chain Watch teach
"do not be held in a burning patch". Volgan's first act tests positioning against a tethered boss that still reaches
out; the second makes him the pack leader and punishes kills on the feeding ground; the third takes the light away
and leaves a pool to avoid inside a closing ring. Wystan then asks for the Cantor's answer with no room to refuse it.

## 12. Lore, echoes, hidden area, story decorations

| Id | Kind | Tile | Name | zh-CN | en |
|---|---|---|---|---|---|
| `lore_hw_01` | order post | (50,65) | 执灯令 / The Lamp-Bearers' Writ | 凡月下行礼者，以巫论处，悬于其所拜之树。——奉圣主印 | Whoever keeps a rite under the moon is a witch, to be hung from the tree she prayed at. — Under the Hierarch's seal |
| `lore_hw_02` | rune pillar, hidden | (109,24) | 被凿的石 / The Scraped Stone | 祠前的石头上刻着六行名字。第六行被凿平了。凿得很旧，也很仔细。石面上还留着拓过的墨痕。 | Six lines of names are cut in the stone before the shrine. The sixth is chiselled flat. The work is old, and careful. The face still carries the ink of a rubbing. |
| `lore_hw_03` | old scroll | **(96,58)**, at the Dark Lantern (moved from (25,105), hidden, in the far south-west) | 首灯手记 / The First Lamp's Notes | 我怕死。树不会死。那个声音说，树里还有地方。今夜我去母橡。——威斯坦 | I am afraid to die. A tree does not die. The voice says there is room in the tree. Tonight I go to the Mother Oak. — Wystan |
| `lore_hw_04` | crystal shard | (80,89) | 月井碎片 / Shard of the Moon Well | 一片冷的银。照得出你的脸，照不出月亮。 | A sliver of cold silver. It shows your face. It does not show the moon. |
| `lore_hw_echo_01` | echo | **(27,9)**, by the Low Boughs' explore spot, north of the pack | 她指的那棵树 / The Tree She Pointed To | 一个双手被反绑的影子仰着头：“他们问我拜的是哪棵树。我指给他们看了。” | A shade with bound hands looks up: "They asked which tree I prayed at. I showed them." |
| `lore_hw_echo_02` | echo | (57,28) | 举灯的人 / The One Holding the Lamp | 一个影子把灯举到一张不在那儿的脸前：“灯照着，就不算黑。灯照着，就不算错。” | A shade holds a lamp up to a face that is not there: "With the lamp on it, it isn't dark. With the lamp on it, it isn't wrong." |
| `lore_hw_echo_03` | echo | **(76,84)**, on the path from the kennels to the oak | 别吃 / Don't Eat | 一个跪着的影子，一只手平伸着，停在狼头的高度：“别吃，沃尔甘。……别吃。” | A kneeling shade holds one hand out flat, at the height of a wolf's head: "Don't eat, Volgan. ...Don't eat." |

Where the echoes stand: `echo_01` is reached after the first drop (the draft's tile was 2.2 tiles from the pack's
anchor, inside its wake range by accident); `echo_02` stands 2.8 tiles from the Cantor and is read after his fight;
`echo_03` is 15.2 tiles from the dais (the draft's was 3.6: inside Volgan's aggro, so the build-up could not be read
before the fight it builds up to).

**Two different objects, two different words.** 六行拓本 (the rubbing: six lines taken from the Scraped Stone, the
sixth a blank, a name on the back; Aldwin carries it, Bran keeps it) and 灯册 (the Lamp Ledger: Wystan's book of the
hanged, in the Barrow). No line calls either one 名册.

**Hidden area** `hidden_hw_moon_shrine` 月祠 / the Moon Shrine (110,15) r5: a rare chest and a lore reward at (112,14)
(unchanged reward kinds). Discovery: 「林子最密的地方，藏着一口干了的井和一圈石凳。石凳上还摆着九只碗。」 (Where the
wood is thickest there is a dry well and a ring of stone seats. Nine bowls still stand on the seats.)

**Story decorations** (the three web positions and `spriteType`s kept; `spriteType` is one of the web's ten values,
which the GameData type check enforces):

| Id | Tile | spriteType | Name | Description zh-CN / en |
|---|---|---|---|---|
| `story_hw_lantern_row` | (48,28) | skeletal_remains (resolves to `skeletal_remains@hangwood`: seven lantern poles, no bone; the art checklist names it) | 七盏灯 / Seven Lanterns | 七根灯杆插在路边，灯还亮着。提灯的人不在了。 / Seven lantern poles stand in the verge, still lit. Nobody is holding them. |
| `story_hw_dry_font` | (72,60) | broken_altar | 枯月盆 / The Dry Font | 石盆是干的，盆沿上的银环被刮掉了。有人在盆里放了一束干花。 / The stone basin is dry and the silver ring on its rim has been scraped off. Someone has left dried flowers in it. |
| `story_hw_first_stump` | (15,75) | charred_tree | 头桩 / The First Stump | 磨坊主的祖父砍倒了这棵橡树，免得有人被吊上去。树皮上的绳痕还在。 / The miller's grandfather cut this oak down so no one could be hung from it. The rope marks are still in the bark. |
| `story_hw_ledger` (in `sub_hw_barrow`) | (18,15) | broken_altar (`broken_altar@barrow`: a lectern with an open book) | 灯册 / The Lamp Ledger | 一页一个名字，几百年的名字。每一页的页脚都写着同一句：“已记，未放。” / One name to a page, hundreds of years of names. The foot of every page carries the same words: "Recorded. Not released." |

The Gallows-Oak is not a story decoration (§9.1).

## 13. Random events, achievements, homestead, pets

Same five kinds, weights, cooldown and frequency. Override `randomEvents.replace.twilight_forest` (the web row names
`wolf` and `bandit`, ids that do not exist).

| Kind | Remake | Content |
|---|---|---|
| ambush | 追上来的狗群 / The Pack on Your Trail | 4-6 `moon_hound`. An ambush is announced and spawned in one step, so there is no warning before it and its members appear standing: that suits hounds running in from the minimum distance and does not suit the Hanged, who are not in the pool. `ambushChampion` 10 %: **the Lamp Patrol** (`lamp_warden` + 3 `lamp_netter`, leader the warden, `sharedAffix`, `leaderLoot: 'magic'`). No `ambushVariants` |
| treasure_cache | 吊在高处的行囊 / A Pack Hung High | travellers hang their packs out of the hounds' reach; as today |
| wandering_merchant | 拾荒人的车 / a Gleaner's cart | as today |
| rescue | 网里的人 / the Netted Traveller | under a Lamp-Bearer net, freed before any harm; guards from [`lamp_netter`]; 60 gold / 50 exp |
| environmental_puzzle | 月相石 / The Moon-Phase Stones | 「五块石头刻着月相，从朔到望。」 Set them in order (the rune-pillar logic); 80 gold / 60 exp. Phases of the moon are a natural shape, not an emblem (§1) |

| Item | Remake |
|---|---|
| Achievements | no web achievement names a Chapter 2 id (checked against `Data/achievements.json`); none is added |
| Elite affixes | `twilight_forest` keeps exactly one affix; no new affix. The 不葬 Unburied affix stays `families: ['unburied']` (the Wood's Dead rise only through the Cantor and Wystan) |
| Homestead | `pet_house` (月井 / the Moon Well) keeps `unlockQuest: 'q_seal_dark_source'` and `allyNpc: 'tower_hermit'`; that NPC is re-cast as Gret (§3.2): the story bible sends Gret to the tower, not the hermit. **Not visible in Phase 4** while the tower stays hidden ([R1], §22 O7) |
| Pets | `pet_storm_wolf` 月牙 / Sliver (Volgan's cub, `cs_tf_moonfang`), `pet_owl` (S5), `pet_cat` (S6); the `pet_void_butterfly` spawns at (70,35) and (45,100) are untouched. The pets panel counts against `pets.json`'s slice list (`["pet_sprite"]` today): the content package adds the three ids. Ley-beasts are re-toned gently, never darkened |

## 14. Map and spawn changes

**Generator inputs kept**: seed 24690, theme `forest`, the 10 spawn tiles, the three camp tiles, playerStart (3,58),
both exits, the two field-NPC tiles, the hidden area, the three web story decorations (tile and `spriteType`), three
of the four lore tiles, the mini-boss tile (55,30), and the questAreas, objective locations, gather areas, clues and
defend target of §5-6 that the table below does not list.

**One planned re-baseline of `twilight_forest`** (inside the content switch, the counterpart of DR4). Its first step
in 4A is to run the generator with these inputs and re-check every tile this document names; until then every tile
below is provisional.

| Change | Kind |
|---|---|
| M5: the explore location (19,64) r8 is removed; questArea (19,64) → (80,94) | −1 landmark, one moved |
| `subDungeonEntrances` gains `entrance_hw_barrow` (79,95) | generator input |
| M1: the Chantry explore (58,26) r10 → the Lantern Row (44,27) r5; the Low Boughs explore keeps its tile, r10 → r4 | one landmark moved |
| M2: questArea (30,30) r15 → (26,31) r17 | one landmark moved |
| S6: questArea (40,88) → (70,50) | one landmark moved |
| S8: a fourth clue (30,90) | +1 |
| `lore_hw_03` (25,105) → (96,58) | one landmark moved |
| echo lore `lore_hw_echo_01..03` at (27,9) (57,28) (76,84) | +3 (margin 2) |
| camp 2 `safeZoneRadius` 9 → 4 | a generator input if the camp clearing reads it (to check) |

A dry run of the draft's (smaller) list against today's generator changed 7 tiles of 14400 and left every tile that
this document names as a wall still a wall; this list is longer and has not been run.

Re-pins: the exporter's `SPEC_GOLDEN` hash for the zone, `world_test` pins that name it, gather-spot hashes
(`q_spider_nest:0`, `q_pet_owl:0`), the bot's coordinates. Hunt tiles, spawn `at`, mixes, corpse seeds, regions, set
pieces and camp NPC lists are not generator inputs.

**Spawn table** (56 monsters in the web's 10 entries, same order). Every entry is on a quest's path or on the road
between two main objectives.

| # | Generator tile | Stands at | Area | Entry | Variants | On whose path |
|---|---|---|---|---|---|---|
| 1 | (25,15) | — | the Low Boughs | `hanged_one` × 8, `dormant {wakeRange: 2.5, rise: true}` | cleansed → `moon_hound` × 3 [R4] | M1 |
| 2 | (40,40) | (46,48) | Mosswick | `moonsick` × 6 | cleansed → `moonsick` × 3 | M3's road east |
| 3 | (15,45) | (14,40) | the Sunk Wheel | `fenbound` × 8 | cleansed → 0 | M2 |
| 4 | (80,18) | (38,22) | the Middle Boughs | `hanged_one` × 6, `dormant {wakeRange: 2.5, rise: true, line {dCol: 0, dRow: -1}}` from (38,22): a line across the lantern road, tiles (38,17)-(38,22) to be confirmed on the new grid | cleansed → 0 | M2 |
| 5 | (95,40) | (70,50) | the Lamp Line | `lamp_netter` × 5 as `mix [lamp_netter × 3, lamp_warden × 2]`, `leader lamp_warden` | — | M3's road east; S6 |
| 6 | (30,70) | — | the Wolf Run | `moon_hound` × 6 as `mix [moon_hound × 5, hound_packleader × 1]`, `leader hound_packleader` | `hw_volgan_down` → `moon_hound` × 3 | S2 |
| 7 | (20,95) | — | Ashcoat's Den | `moon_hound` × 6 as `mix [moon_hound × 4, hound_packleader × 2]`, `leader hound_packleader`: one leads, the second still howls when the first is dead, so one kill does not finish the pack | `hw_volgan_down` → `moon_hound` × 3 | S1 |
| 8 | (85,80) | (78,74) | the Kennel Ruins | `moonsick` × 6 as `mix [moonsick × 4, moon_hound × 2]` | — | the walk home; `lore_hw_echo_03` |
| 9 | (90,90) | — | the Gallows-Oak | `volgan` × 1 (storyBoss) | — | M4 |
| 10 | (100,65) | (97,74) | the Chain Watch | `lamp_netter` × 4 as `mix [lamp_netter × 3, lamp_warden × 1]`, `leader lamp_warden` | — | M4 (objective 1) |

Why `at`: entry 3's web tile is 7.6 tiles from the hub centre, entry 10's is 3 tiles from camp 3, entry 2's is 5 tiles
from S7's defend target, and entries 4, 5 and 10 stood on no quest's path (15 of the draft's 56 monsters were off the
route, the whole Lamp-Bearer family among them). The draft's `at` for entry 10, (108,74), is a wall tile, which the
exporter refuses.

Rules the content test pins: every `at` tile, formation tile, corpse seed, hunt tile, clue, lore and return tile is
walkable on the generated grid; every pack stands ≥ 12 tiles from every camp centre and from playerStart; none within
11 tiles of the defend target; none within 9 tiles of a field NPC or inside a gather area; no explore circle reaches
inside a mini-boss's aggro; no lore tile lies within a dormant entry's wake range of its anchor.

Respawn 15 s for regulars. **Dormant entries respawn as a group, dormant, and only out of sight** ([R4]): today
`MonsterSystem::Respawn` builds a plain instance, so a respawned Hanged would stand awake under its bough with
ordinary aggro and patrol, one at a time. M2 does not depend on it (fourteen Hanged stand in two entries; six are
asked for).

Mini-boss at (55,30). Hunts at (22,96) and (48,79) (the draft's (48,80) and (46,94) are wall tiles; the core would
have moved them to the nearest open ground, away from where the document measured). Corpse seeds: the Chantry's six
(§10).

## 15. Zone mood

Values for `GameData/world.ts` (`themeByZone.twilight_forest = 'hangwood'`, `sub_hw_barrow = 'barrow'`;
`themeParents` `hangwood → march`, `barrow → hangwood`). P14 applies 55 % of a mood. The post-cleanse mood keeps the
`<theme>_dawn` naming rule (DR12) and shows moonrise. **Mood and presentation themes are a C++ enum** (`MapTheme` in
`abyss/base/Enums.h`), not free strings: the five names below are [R7], and the two themes also have to be added to
`Art/blender/kit/theme.py` or the exporter's `themeParents` check fails.

| Setting | `hangwood` | `hangwood_dawn` (after `hw_barrow_cleansed`) | region `gallows_oak` | `barrow` | `barrow_cleansed` |
|---|---|---|---|---|---|
| ambient / α | #8FA3A8 / .34 | #B8C8E0 / .28 | #5E7A70 / .38 | #4E6660 / .42 | #C0CCE0 / .30 |
| vignette / α | #05080A / .62 | #080A10 / .48 | #040807 / .66 | #030505 / .74 | #080A10 / .48 |
| haze / α | #3E4A4A / .16 | #5A6678 / .10 | #1E2E2A / .16 | #182220 / .10 | #46506A / .08 |
| saturation / contrast | .62 / 1.14 | .74 / 1.08 | .56 / 1.16 | .56 / 1.16 | .76 / 1.08 |
| lift / gain | [-.012, -.004, .002] / [-.004, .006, .012] | [.002, .004, .010] / [.006, .012, .024] | [-.014, -.008, -.004] / [0, .010, .004] | [-.014, -.010, -.006] / [-.008, .004, .008] | [.004, .006, .012] / [.010, .014, .026] |
| fog colour / max / density | #2E3A3C / .48 / 1.30 | #46526A / .32 / 1.00 | #22302C / .52 / 1.35 | #141C1A / .52 / 1.30 | #2E3644 / .30 / 1.00 |
| weather / ambience | none / `wood_mist` (ground mist, slow leaf-fall), tint #AFC4C0 | none / `moon_motes` (silver motes in shafts) | none / `sparks` tint #9FE0C8 | none / none | none / `moon_motes` |
| light scale | 0.36 | 0.26 | 0.42 | 0.50 | 0.30 |
| wind-up tint | #D11F2E / #FF3A48 on all five (DR5) | | | | |

The draft's sixth mood (a region round the Chantry) is cut: the seven sallow lanterns light that place themselves.

**Reserved light.** Warm light (#FFB060) only at the mill fire, Harl's forge, the Crow Carts' cookfire and one candle
in Bran's hut (his lantern is never lit). Lamp-Bearer lanterns and the Chantry's seven poles are sallow (#E6D36A):
light that shows and does not warm. The seal's corruption is moon-tarnish (#9FE0C8) from below. True moonlight
(#E6ECF5, the zone's first top light) exists only after the cleanse and in the cleansed Barrow.

**Terrain and decor.** The generator theme stays `forest`; the presentation theme is `hangwood`. The forest theme's
decor game ids and footprints are unchanged and resolve to `<id>@hangwood` meshes first (the list of ids is read from
`Data/assets.json` in the art package, as Chapter 1's table was; it was not enumerated here). Direction: moon-oaks
with low, heavy horizontal boughs (the silhouette the chapter is named for), black water, pale fungus, root-ribbed
ground; undergrowth thinned so that a low dark hound stays readable. Terrain style `hangwood`: leaf mould, root
ridges, wet stone, black pond water with a silver-green sheen. Camp theme `hangwood`: wattle and cart-sides, no
banner.

## 16. Audio

| Where | Cue |
|---|---|
| The Hollow Mill | a slow bowed drone and a hand-drum of our own, the fire, the stopped wheel ticking against its pawl; no bell until M3 is turned in, then Aldwin's handbell on the door in the wind |
| Wilderness | near-silence, rope creak, leaves, distant hounds, and from time to time the far howl that is Volgan (until `hw_volgan_down`); no birdsong before the cleanse |
| The north wood (until `hw_cantor_silenced`) | the chant: one man's voice leading, many wordless voices answering, heard from 30 tiles and louder toward the Chantry (an emitter with distance layers) |
| Dormant tell | a rope creak when the hero comes within `wakeRange` + 3 tiles of a dormant entry. It is keyed on distance, not on a timer before the wake: the wake is immediate at `wakeRange` and the 900 ms Drop is the reaction time |
| Hound packs and ambushes | the leader's howl is the stinger of its `call`; whines before a pounce; an ambush pack gives tongue as it spawns (the same step: no earlier warning exists) |
| The Lamp Line, the Chain Watch | a shutter clack and 「照——」 before Lamplight |
| The Gallows-Oak | silence inside 16 tiles while Volgan lives (chain links shifting, slow breath); boss music from the intro; stingers on `EvMonsterPhase` 1 (the chain breaks) and 4 (every layer but a low pulse drops out for the eclipse) |
| The Root Barrow | dripping, root creak, a low tone from the lantern, a dry voice saying two words over and over; on the cleanse a single clear note, then water |
| The walk home and the finale | night birds for the first time; at the turn-in the mill wheel turns once |

Voice families: **Wooddead** (new), **Hound** (new), **Moonsick** (new, small, wordless), **Human** (the Lamp-Bearers).
Aldwin keeps his Chapter 1 handbell samples, detuned.

## 17. Art asset list

Per asset the existing ship pipeline applies (bake, FBX + LOD1, manifest, per-frame QA, ink gate, darkest-tile gate on
the `hangwood` root tile, previews, lineup). Budgets LOD0 / LOD1 in triangles.

### 17.1 Meshes

| Asset | Kind | Rig / source | Generator (new files) | Est. tasks | Budget |
|---|---|---|---|---|---|
| `Monsters_Hangwood`, `Props_Hangwood`, `Foliage_Hangwood` palette atlases; `theme.py` rows | new atlases | `kit/palette.py` registry (append) | `palettes/*_Hangwood.json` | 1 | — |
| **SKEL_Quadruped + clip library + `moon_hound`** | **new rig family, first member** | new | `kit/skel_quadruped.py`, `monsters/qclips.py`, `monsters/moon_hound.py` | 5.5 | 5k / 2.5k |
| `hound_packleader` + Ashcoat | variant | SKEL_Quadruped | `monsters/hound_packleader.py` | 1.5 | 5.5k / 2.5k |
| Wood's Dead shroud layer + `hanged_one` (with both `Drop` and `Rise`) | kit layer + variant | Unburied kit | `monsters/wooddead.py`, `monsters/hanged_one.py` | 2.25 | 7k / 3k |
| `fenbound` | variant | Unburied kit | `monsters/fenbound.py` | 1.25 | 7k / 3k |
| `moonsick` + Riftjaw | lean variant | Living kit | `monsters/moonsick.py` | 2.5 | 7k / 3k |
| `lamp_netter`, `lamp_warden` + net, lantern-pole | variants | Living kit | `monsters/lamp_bearers.py` | 3 | 7.5k / 3k |
| `miniboss_cantor` + portrait | re-dress of the Aldwin NPC look as a monster | SKEL_Human | `monsters/cantor.py` | 2.5 | 8k / 3.2k |
| `volgan` + chain + portrait | new mesh, large | SKEL_Quadruped × 2.2 | `monsters/volgan.py`, `monsters/volgan_clips.py` | 6.5 | 11k / 4k |
| `sub_rootbound` (Wystan) | new mesh (torso and arm out of a root mass; legs are root) | SKEL_Human upper body | `monsters/rootbound.py` | 3 | 9k / 3.5k |
| Family passes (budget, LOD1, lineup on `hangwood` terrain, husks for six bodies) | pass | all | `monsters/lineup_hangwood.py` | 3 | — |
| NPCs: Gret, Harl, Bess, Odo, Old Wick, Bran, Hedda, Nora, Jory; portraits for Gret, Bran, Hedda, Nora | re-dress | folk kit | `npcs/looks.py`, `npcs/npc_kit_parts.py` (append) | 4.25 | — |
| Foliage and rocks on the forest game ids, terrain style, camp kit (mill house in the tent slot, wattle fence, meal sacks) | re-theme | `envkit` | `environment/*_hangwood.py` | 6 | — |
| Set pieces the route passes: hung bundles (4 variants) and their on-the-ground variants, laid bundles, cut rope-ends, the Gallows-Oak (trunk on a wall tile, 6 × 6 crown), the chain and ring-bolt, the feeding ground, root knuckles for the arena's wall tiles, the Chantry (flagstones, seven poles, broken walls), the Cutting Oak with ladder and hook (three bough variants for S4), empty graves, the Kennel Ruins, Bran's hut and lectern, the Crow Cart, shroud lines and the spring, the hook wall, the Barrow mouth, the Moon Shrine and the Scraped Stone, the Writ Stone, the Red Field's opened barricade | new props | `envkit` | `environment/setpieces_hangwood.py` | 7 | — |
| Root Barrow kit (three rooms): root walls (overlays on the crypt blocks, DR17 rule 2), root curtains, hanging-root ceiling pieces, the lectern, the Moon Well (dark and lit), stair | new, on the crypt kit | `environment/crypt_pit.py` (reused) | `environment/barrow_hangwood.py`, `environment/terrain_barrow.py` | 3 | — |
| Pets: `pet_storm_wolf` (three stages, on SKEL_Quadruped small), `pet_owl`, `pet_cat` | new | SKEL_Quadruped / SKEL_Pet_Sprite / new small rig | `pets/moonfang.py`, `pets/owl.py`, `pets/cat.py` | 6 | 4k / 2k |
| Quest items and icons: `rope`, `feather` item kinds, the mill key, the shroud, gather nodes (rope-end, feather) | new | `items/`, `icons/` | — | 1.75 | — |
| **Total** (with §17.3's ~4: **~64**) | | | | **~60** | |

### 17.2 Clips with contact beats (ms from clip start; the specials' beats are the abilities' `windupMs`)

| Monster | Clip: total / beat |
|---|---|
| `hanged_one` | `Idle_Hang` 4000 · `Drop` 900: rope parts 200, lands 520 · `Rise` 900 (same length: the core's root time) · `Attack01` 1000 / Contact 600 · `Attack02` (Rope Lash) 1200 / Contact 700 · `Death` 900 |
| `fenbound` | `Attack01` 1700 / Contact 800 · `Cast01` (Mire Grasp) 1400 / Contact 800 · `Death` 1200 |
| `moon_hound` | `Attack01` 800 / Contact 320 · `Pounce` 1000 / leaves ground 380 / Contact 650 · `Walk` 700 and `Run` 460 with four foot notifies · `Death` 900 |
| `hound_packleader` | as the hound; `Cast01` (Howl, Long Howl) 1400 / Contact 1000 |
| `moonsick` | `Attack01` 900 / Contact 420 · `Attack02` (Rake) 1100 / Contact 650 · `Roar` 900 (the Turn) · `Cast01` (Riftjaw's Leap) 1500 / Contact 900 · `Death` 1000 |
| `lamp_netter` | `Attack01` 1500 / Release 600 · `Cast01` (Net) 1500 / net opens 600 / Contact 900 |
| `lamp_warden` | `Attack01` 1300 / Contact 620 · `Cast01` (Lamplight) 1500 / shutter 400 / Contact 900 |
| `miniboss_cantor` | `Attack01` 1300 / Release 550 · `Cast01` (Lead the Verse) 1500 / bell 300 / Contact 900 · `Cast02` (The Rest) 1600 / Contact 900 · `Taunt` 2400 · `Death` 1500 (kneels) |
| `sub_rootbound` | `Attack01` 1500 / Contact 650 · `Attack02` (Root Sweep) 1300 / Contact 700 · `Cast01` (Lantern Beam) 1600 / Contact 900 · `Cast02` (Lights Out) 1700 / Contact 1000 · `Cast03` (Root Burst) 1300 / Contact 800 · `Roar` 1200 · `Death` 1600 (the hand opens at 900) |
| `volgan` | `Attack01` 1100 / **one** Contact at 480 · `Attack02` (Chain Sweep) 1500 / Contact 800 · `Snap` 1300 / Contact 700 · `Cast01` (Chain Slam, Moonbite) 1600 / Contact 900 · `Pounce` 1400 / leaves ground 450 / Contact 800 · `Cast02` (Howl) 1400 / Contact 1000 · `Roar` 1400 (phase 1: the chain parts at 700) · `Death` 1800 |

Unverified on the UE side: the clip names `Pounce`, `Snap`, `Drop`, `Idle_Hang` and `Cast03` in the anim map, and a
quadruped `animCategory`.

### 17.3 VFX recipes (`vfx/fx_recipes_hangwood.py` → `vfx_recipes.json`; ~4 tasks)

| Recipe | Use |
|---|---|
| `monster.hanged.drop` | rope fibres and dust at the landing (`Drop` 520 ms) |
| `monster.fenbound.puddle` | the on-death bog pool (tarnish-black, a poison-green edge inside the crimson hazard rim) |
| `notify.net_throw`, `hazard.lamplight` | the net opening; the sallow flash then the burning patch under the DR5 decal |
| `monster.raise.mark@hangwood`, `notify.bell_ring@hangwood`, `aura.chorus` | the Cantor's raise marker, bell ripple and aura ring in moon-tarnish |
| `notify.howl` | a ground ripple from the howling leader (non-damaging) |
| `boss.volgan.maw`, `boss.volgan.chain_break`, `boss.volgan.eclipse`, `hazard.moonbite` | the light between the teeth (a socket loop), the chain parting, the eclipse's darkening contract (ambient scale and the hero light radius, as `dark.pit.lightRadius`), the pool |
| `prop.barrow_cleanse.*`, `ambient.moon_motes`, `ambient.wood_mist` | the Barrow's cleanse sequence, the two ambiences |
| `exit.sealed@hangwood` | the east road's seal toward the Chained Anvil |

### 17.4 UI

Portraits (four NPCs, the Cantor, Volgan); item-kind icons `rope`, `feather`; the zone banner and chapter card art;
three permanent-reward lines (existing widget). No new HUD element: the boss bar's phase ticks, the status row and the
labelled choice cards exist.

## 18. Balance bands (DR9 form)

**Anchors.** The population stays 56 in the web's 10 entries. Quest gold stays 1255. Quest exp is 5420, of which 3740
is on the main path.

**Levels, computed from the route** (expToNext(L) = 3L² + 25L, `hero_formulas.json`: Lv 16 starts at 6720, Lv 17 at
7888, Lv 18 at 9180, Lv 19 at 10602, Lv 20 at 12160, Lv 21 at 13860). Entry is Chapter 1's pinned band, Lv 13-14,
typically 4500-5300 exp.

| | Full route | Main path only (M1-M5 + S7) |
|---|---|---|
| Quest exp | 5420 + 80 | 3740 + 80 |
| Kills on the path, no respawn (count / exp) | 88 / 2393 | 60 / 1785 |
| At the Cantor | Lv 16 | Lv 15-16 |
| At Volgan | Lv 17-18 | **Lv 16-17** |
| At the end | **Lv 20** (12400-13200 exp, more with walk-through kills) | **Lv 18-19** (10100-10900) |
| Pinned end band | Lv 19-21 | Lv 18 or more |

The kill figures are a count of this document's spawn table along the route of §19, per step:

| Step | Kills | Exp |
|---|---|---|
| M1: the Low Boughs (8 Hanged) | 8 | 120 |
| M2: the Middle Boughs (6) and the Sunk Wheel (8) | 14 | 202 |
| S7: three waves (11 Hanged, 7 Fenbound; pays, wave scaling of exp unverified) | 18 | 263 |
| The south loop: the Wolf Run (6), Riftjaw, Ashcoat's Den (6), Ashcoat and two hounds | 16 | 384 |
| The Cantor (his Raised pay nothing) | 1 | 240 |
| Mosswick (6; a walk-by, optional) | 6 | 120 |
| The Lamp Line (5) and the Chain Watch (4) | 9 | 180 |
| Volgan (his hounds pay nothing) | 1 | 400 |
| The Barrow: the gallery (6), the guard (2), Wystan | 9 | 380 |
| The walk home: the Kennel Ruins (6) | 6 | 104 |
| **Total** | **88** | **2393** |

Not counted: packs that have respawned on a second pass (the route passes the Lamp Line's road and the Wolf Run once
each; the walk home passes Mosswick's variant). Chapter 1 measured what such passes do ("walking back from the
Crowmarket through the Lv 6-7 far half put every class at Lv 15"); the sweep reports walk-through kills as their own
number, expected at 10-40 kills and 150-600 exp, which keeps the full route inside Lv 20-21.

A main-path-only hero ends one to two levels under Chapter 3's stated start (Lv 20). That is recorded, not solved:
Chapter 3's entry band is its own chapter's decision.

**Rules carried over unchanged.** Raisers are capped (the Cantor: 3 alive); fuses ≥ 800 ms (the Fenbound puddle is a
hazard, capped at 20 %); no single enemy hit above 35 % of a same-level hero's maximum life on Normal; on-death and
1 s burst caps as built; no invulnerable phase.

**Fight-length and threat targets** (per class and per bot model, as the 2F review defined them; 4T sets HP and damage
to meet them, after measuring the hero, §4.0). Two routes are swept from day one, as Chapter 1's `ABYSS_PT_NO_PIT`
pattern: the full route and the main-path route.

| Fight | Hero | Median length | 90th percentile | Must be seen | Threat |
|---|---|---|---|---|---|
| A regular pack of 6 | Lv 14-18 | 6-25 s | < 45 s | — | largest hit taken in the zone at the median ≥ 6 % of max life for every class |
| The Low Boughs drop (8) | Lv 13-14 | 8-30 s | < 50 s | all eight awake in one step | the largest 3 s burst after the drop ≤ 50 % |
| `miniboss_cantor` | Lv 15-16 | 10-45 s | < 75 s | the opening raise in ≥ 95 % of fights; The Rest in ≥ 60 % | largest hit ≤ 35 % |
| `volgan`, full route | Lv 17-18 | 30-180 s for every class | < 240 s | every one of the five phase rows in 100 %; **seven hounds in 100 %, for every class**; each of the three acts at least 15 % of the fight for every class; no reset | deaths in ≤ 2 % of fights (frame-perfect bot), ≤ 10 % (reaction-limited: 400 ms, no dodge roll) |
| `volgan`, main path only | Lv 16-17 | 30-210 s | < 275 s (DR9's second ceiling, same reason) | as above | as above |
| The Barrow's gallery | Lv 18-19 | — | — | all six awake in one step | none asked (a set piece at the roster's level; stated in §9.2) |
| `sub_rootbound` | Lv 18-19 | 15-60 s | < 100 s | Lights Out in ≥ 90 % **for every class** (the draft's ≥ 60 % could not be met by a ranged hero); the hero inside 2.5 tiles of him at least once in ≥ 90 % | median largest hit ≥ 6 % (the surface floor); ≤ 35 % |

Every run: hits ≤ 35 %, 1 s bursts ≤ 50 %, no one-hit death, no story-boss reset, no failure.

**Unverified inputs to this section:** the hero's life and defence at Lv 13-19 (so the Low Boughs burst band is a
target, not a check); the hero's damage growth (§4.0); whether defend-wave scaling changes a wave monster's exp; the
mage's longest skill range against the Barrow corridor's far end (10.8 tiles from Wystan).

## 19. Acceptance checks

**`content_ch2_test.cpp`** (new; suites `quests` / `monsters` / `world` / `story`; Chapter 2 pins live only here):

- the 10 spawn entries sum to 56, in the web's order and on the web's generator tiles; the tile rules of §14 (every
  named tile walkable; distances to camps, the start, the defend target, field NPCs, gather areas; explore circles
  against mini-boss aggro; lore against wake ranges);
- **the first drop is M1's**: the straight path from the hub to the Low Boughs' explore spot passes within the pack's
  placement area (anchor ± jitter) before it enters the explore circle;
- no camp's safe circle intersects a bound fight's reach; every walkable tile within 10 tiles of Volgan's dais is
  reachable from it;
- 13 quests in the zone, ids as §0, 5 main and 8 side, totals 5420 / 1255, listing NPCs as §7, every quest has
  `progressKey`, `choiceLabels` and `choiceFlags` lengths match `choices`, the finale offers a gear choice, the eight
  side quests have eight different objective-type lists, M3's prerequisites are M2 and S7;
- **no quest is stranded by a flag**: for every combination of this zone's flags, every objective of every quest
  that can still be open has a source that the flag state leaves standing (the K7 soft-lock guard covers only
  objectives with a sub-dungeon `zone`);
- every Chapter 2 monster has `family` and `hitCapHeroMaxHpPct` 35; every ability names a clip unless its kind is
  `hazard` or `call`; no `hazard`-kind ability has `dmgMul` or `status`; every ability authors `windupMs` at or above
  its floor; on a tethered monster every ability either has `rangeMax` or is on the list of melee-only abilities
  (`root_sweep`, `chain_sweep`); every DoT status has an absolute `value`; `projectileColor` and telegraph data pass
  the hue rule;
- the deny-list across the zone's i18n keys and ids: "noose", "hoist", 上吊, 自缢, 吊死鬼, 找替身, 寻短, 自尽, 投缳, 缢
  (if §22 O1 stands), 祂, 礼拜, "Greymane", "Moonfang", "Duskwood", "worgen", and the story bible's avoid list;
- the cutscenes of §8: 5-14 steps, every speaker and `focus npc` target stands within 20 tiles of where the trigger
  fires (a new check: the zone-level check passed a speaker 82 tiles away), every key in zh-CN and en, zh limits;
- the Root Barrow: every walkable tile reached from the stair; the exit trigger reaches no tile of the Root Stair
  room; **the gallery walk** (every tile of the Hanging Gallery from col 19 east is within `wakeRange` of a member);
  every tile of the Seal Chamber is within Lights Out's outer radius of the keeper; the keeper's guard is one pack
  with him; eight monsters and the keeper;
- flags: the variants of §8 select as listed; with `hw_barrow_cleansed` set no entry spawns `hanged_one` or
  `fenbound`; the validator accepts every `lines` / `visibleWhen` condition, `knell_chaplain`'s included;
- [R1] the story boss of `twilight_forest` is `volgan`, Chapter 1's is still `valdras`; the Red Field's east gate is
  sealed without `rf_chapter_dawn` and open with it, whatever `milestone1` says; the exit to `anvil_mountains` is
  sealed.

The exporter gains the same walkability checks for corpse seeds and hunt tiles on generated maps (it has them for
`at` tiles only).

**AutoPilot route** `GameData/autopilot/ch2.ts` → `autopilot_routes.json`. It needs [R8]. Every turn-in is named:

| # | Steps | Turn-ins and accepts |
|---|---|---|
| 1 | `tree quest_scout [ration, work, accept_road]` | M1 taken |
| 2 | `explore` the Low Boughs (the first drop), the Empty Graves, the Lantern Row → mill | M1 in; tree: M2 (`accept_empty`), S2 (`accept_lost`) |
| 3 | `hunt [fenbound]` at the Sunk Wheel, `hunt [hanged_one]` at the Middle Boughs → mill | M2 in (`cs_hw_empty_rope`); card: S7; tree: S4 (`accept_tam`), S1 (`accept_bounty`) |
| 4 | `defend q_defend_camp_forest` → mill | S7 in (`cs_hw_dream`); tree: M3 (`accept_name`) |
| 5 | south loop: the Wolf Run pack, `clues` S2 × 3, `boss huntLeader` (Riftjaw) → `visit forest_spirit_medium accept [S3, S6, S8]` and S4's talk → Ashcoat's Den, `boss huntLeader` (Ashcoat) → `clues` S8 (15,60) → mill | S2's talk (the key to Gret) |
| 6 | `gather q_spider_nest`, `clues` S8 (20,25) → `boss miniBoss` (the Cantor, `cs_hw_cantor`) → `clues` S8 (50,40) → `visit forest_tracker` | S1 in, S2 in (at Hedda) |
| 7 | the Lamp Line (`hunt [lamp_netter, lamp_warden]`, S6's shards and its warden) → `talk forest_hermit` | S4's talk; M3 in (`cs_tf_hermit`); card: M4, S5 |
| 8 | `gather q_pet_owl`, `clues` S5 → Bran | S5 in |
| 9 | the Chain Watch (`hunt [lamp_warden]`) → `boss storyBoss` (`cs_tf_moonfang`) → Bran | M4 in; tree: M5 (`accept_seal`) |
| 10 | `dungeon entrance_hw_barrow` with the three rooms (the Pit's step kind; `cs_hw_barrow`, `cs_hw_seal`) | — |
| 11 | the walk home: the Kennel Ruins → `clues` S8 (30,90) → `visit forest_spirit_medium` → mill | S3, S6, S8 in (at Nora); S4 in with `choice` 0 on seeds ending in an even digit, 1 on odd; M5 in (`cs_tf_finale`) |

The town portal takes the nearest camp of the zone (W3), so "home" is a walk except from the west: the route has four
long legs and one short return: the mill to the Dark Lantern by way of the Empty Graves, the Chantry, the ford and
Hedda, about 130 tiles; the Owl Wood, the Chain Watch, the oak and back to Bran, about 95; Bran to the Barrow, 39;
the Barrow to Nora by the Kennel Ruins, about 80; Nora to the mill, 37. That is about 380 tiles, 105 s of walking at
3.6 tiles/s, and the only ground covered twice is the 30 tiles between the oak and Bran. The main path alone is about
270 tiles (75 s). The main-path route is steps 1-4, 6 (the Cantor only), 7 and 9-11 without their side-quest parts.

**Playthrough.** `playthrough_test.cpp` gains a Chapter 2 leg from **entry fixtures** (three saves, one per class,
dumped from the Chapter 1 runs of seeds 101 / 202 / 303 and committed under `CoreTests/fixtures/ch2_entry/`; loading
a fixture as a run's start is new test infrastructure, [R8]), so the Chapter 2 sweep does not replay Chapter 1; one
full Chapter 1 + 2 run per class stays in the sweep as the seam check, through the opened east gate. Pins: hero level
19-21 at the end of the full route and 18 or more on the main path, kills ≥ 85, embers and walk-through kills as the
first sweep measures them, achievements unaffected, the pets `pet_storm_wolf`, `pet_owl`, `pet_cat` owned (full
route), the flags `hw_cantor_silenced`, `hw_volgan_down`, `hw_barrow_cleansed` set, one of `hw_tam_cut` /
`hw_tam_left` set, the three permanent rewards claimed (full route; S7's on both).

**Sweeps** (before the package closes): `ABYSS_PT_SWEEP=1` over 20 seeds × 3 classes × both bot models × both routes
from at least three bases; the bands of §18; GCC and Clang final saves identical for one base. **Other checks**: core,
core-asan, core-shared, data (`--check`), web-untouched, PresentationTests (`./run.sh`), CompileCheck, scripts, kit,
coherence, the hue audit with the two new accents, audio; on the Mac: `-abyssautotest=chapter2` for three classes and
the look review (the darkest-tile gate in game for the hound, the eclipse on every quality tier, the bundles at the
W1 camera against §1's rules).

## 20. Core requests

Everything else in this document is data for K1-K7. The draft named three requests and listed eight behaviours as "to
verify, not to build"; the review read the code and five of those were things to build. Eight requests, each with the
smallest schema that does the job. Line references are to the working tree of 2026-11-13.

**[R1] Chapter table, conditional gates, and what `milestone1` means in Phase 4.** Today `monster_ai.json` holds one
`storyBossId`, one `storyBossNotAfterQuestTurnIn` and one `chapterCompleteQuest` (`MonsterData.h`), the boss windup
floor and the Q4 questArea fix name that one boss (`DataStore.cpp`), the UE dawn rule names that one quest
(`AbyssZoneActor.cpp`), and the one sealed gate applies only under `config.milestone1` (`Zone.cpp:115`), the same
switch that hides the Ember Tower (`Homestead.cpp:462`) and that the game instance sets to true. Smallest schema:
`GameData/config.ts` `chapters: [{zone, storyBoss {monsterId, notAfterQuestTurnIn}, chapterCompleteQuest,
chapterFinaleCutscene, dawnFlag}]` and `sealedGates: [{exitTo, messageKey, openWhen?: RemakeCondition}]` (no
`openWhen` = sealed). The gate rule reads the table and **no longer reads `milestone1`**; `milestone1` keeps one
meaning, "the Ember Tower is hidden", and stays true in Phase 4 (§22 O7). M7's rules, the windup floor, the questArea
fix and the UE dawn rule read the row of the hero's zone. Chapter 1's values become row 0, so its behaviour and pins
do not move. Rows: `{twilight_forest, volgan / q_kill_werewolf_alpha, q_seal_dark_source, cs_tf_finale,
hw_barrow_cleansed}`; gates `{twilight_forest, zone.exit.sealedChapter2, openWhen {flag: rf_chapter_dawn}}` and
`{anvil_mountains, zone.exit.sealedChapter3}`. The pets slice list in `pets.json` gains Chapter 2's three ids (data).
**No fallback**: a second story boss is impossible without it and the gate is all or nothing. Tests:
`world_state_test` (gate by flag, with `milestone1` on and off), `content_hooks_test` (M7 with two synthetic
chapters).

**[R2] A story trigger on a world flag.** `{on: 'flag_set', flag, cutscene, grantPet?}`, fired from `EvWorldFlag`
once (the `storySeen` rule). It is the trigger kind chapter1.md §19 recorded as missing for the Flame Niche. Used by
`cs_hw_seal`. Fallback: §8.

**[R3] Choice flags.** `rewards.choiceFlags: string[]`, aligned with `choices` like `choiceLabels`; the turn-in sets
the flag of the picked slot through the `setsFlags` path. Used by S4. Fallback: §6 S4.

**[R4] Dormant entries: respawn and variants.** (a) `MonsterSystem::Respawn` builds `MakeInstance(...)` and copies the
anchor, home, pack and entry fields but never `dormant`, `dormantWakeRange` or `dormantRise` (only the
zone-population path sets them), so a respawned member of a dormant entry stands awake. Wanted: a dormant entry's
members do not respawn one by one; when the last is dead the whole entry comes back together after the respawn time,
dormant, on its formation tiles, and only while the hero is more than 16 tiles from its anchor (retried every 5 s):
nothing is ever seen going back up (§1). (b) Zone population applies `spawn.dormant` to a variant's members as well,
so a variant of hounds would hang dormant and play a `Rise` they have no clip for. Wanted: a variant drops the
entry's `dormant` block. Fallback: Hanged entries are not respawned at all (a `respawn: false` on the entry, if that
exists, else accepted awake) and the Low Boughs' variant is count 0; no quest depends on either. Test: a synthetic
dormant entry killed, re-hung out of sight, woken as one; a variant on a dormant entry.

**[R5] Set-piece conditions.** `SetPieceDef` has no condition field and its loader is in the core. Wanted: `when
{flag?, notFlag?}` (flags only, so that UE can decide from `Snapshot::worldFlags`), validated against known flags.
Used by Aldwin's handbell, the bundles (hung / on the ground), the Cutting Oak's three variants, the Red Field's
opened barricade. Fallback: a table keyed by set-piece id in a UE Rules header, tested, recorded as debt. Blocking
set pieces are **not** requested (§9.1 puts the oak on a wall tile).

**[R6] A base tether.** A phase row's `leash` is in force only while that row is the phase, and a row is entered when
life is strictly below `hpBelowPct` (`Phases.cpp:29`), so a row at 100 does nothing at full life. Wanted:
`MonsterDef.tether {centre?, radius}`, the same goal clamp, in force in phase 0. Used by Volgan (5) and Wystan (1.5).
Fallback: §9.2 and §11.

**[R7] Five mood / theme names.** `MapTheme` gains `Hangwood`, `HangwoodDawn`, `GallowsOak`, `Barrow`,
`BarrowCleansed` with their strings; the UE theme rules and `Art/blender/kit/theme.py` gain the two themes. No
fallback short of reusing Chapter 1's moods.

**[R8] AutoPilot and playthrough.** The bot plays only the default map's route (`AutoPilot.cpp:2819`), has one
scenario (`Chapter1`), has no step that walks through a zone exit, and always takes the best reward choice
(`AutoPilot.cpp:1903`). Wanted: route selection by the hero's zone (or a `Chapter2` scenario) with a chapter-done
event per route; an `exit` step for the seam run; a `choice` field on a turn-in; starting a run from a committed
save; the UE `-abyssautotest=chapter2` scenario. Two bot behaviours are work, not checks: stepping **into** an
inner-radius ring's safe disc when that is nearer than its outer edge, and treating a dormant entry as one pull.

**Checked and found to work as data today** (no request): `rope_lash`, `mire_grasp`, the Fenbound's puddle,
`throw_net` (`dmgMul` 0 with a status), `lamplight` (a hazard as a ranged special), the dash lines, a one-row phase
table, the Cantor's raise / aura / ring / kite, phase summons at anchors, `ringShrink`, mixes and leaders, hunts with
`revealAfterPrevious` and minions, hunt abilities, a kill objective on a mini-boss and on a hunt, `clear_area` after
0, `zone_entered` on a sub-dungeon, `whileQuest`, dialogue `prereqQuests` / reward / `questTrigger`, a tree offering
another NPC's quest, defend waves with compositions and `approachDeg`, permanent rewards, a per-camp
`safeZoneRadius`, `prereqQuests` with two entries, K6 conditions with `any`.

**Still unverified** (each a first task of its package; a failure becomes a bug or a small request): `at` on six
entries; a dormant `line` on a generated map (entry 4); a `guard` entry that is itself dormant with a `line`; a ring
with `innerRadius` on a mini-boss; `count: 0` spawn variants; phase rows without an `enterAction` that carry summons;
reading a `cleansedFlag` from spawn variants and the mood; an `onWall` set piece on a generated zone; the exporter
hook for the Labyrinth row; that the stash works for a new NPC id by type; the quadruped `animCategory` and the new
clip names in the UE anim map.

**Not requested**: a `flank` chase goal (§4.4); an entrance that opens on a condition (the quest gate is enough,
§5 M5); a speed-0 monster (§4.5); a buff list per ability (the howl grants one stat); a stand-in giver (§3.4); a
portal that goes to the zone's hub (the route has no round trip); raising a sub-dungeon's roster to its label (the
Barrow is short instead); a warning delay before an ambush (§13); a withdrawal of a quest offer on a flag (S7 is a
prerequisite instead).

## 21. Work packages

Order: content data → core → art → UE, as implementation-plan.md §9; art and audio run beside the core. Owners are the
lanes; no two packages own a file at the same time.

| WP | Owner | Scope | Owns | Depends | Acceptance |
|---|---|---|---|---|---|
| **4A Content data** | content | first the re-baseline dry run and the tile re-check (§14); then the zone's remake modules: map, roster, quests, NPCs (with §3.4's Chapter 1 amendment), the two trees, mini-boss, sub-dungeon and its tiles, lore, story script, overrides (moods, events, themes, the Labyrinth row, the pets slice), set pieces, config rows, i18n (`ch2Quests`, `ch2Npcs`, `ch2Story`, `ch2World`), the bot route. Two new redirects (`src/data/maps/twilight_forest.ts`, `src/data/monsters/twilight_forest.ts`) and their snapshot digests. Exporter walkability checks for corpse seeds and hunt tiles | `unreal/GameData/**` (Chapter 2 entries and new files), `unreal/Tools/export-data/**` (redirects, the `abyssRun` override, registry rows for [R1]-[R7] behind `REMAKE_SCHEMA`) | — | data `--check`, web-untouched; fields the core cannot read yet are withheld |
| **4K Core** | core | [R1]-[R7]; the unverified list of §20 as tests | `unreal/Source/AbyssCore/**` (not `tools/`) | 4A (ids) | core, core-asan, core-shared; Chapter 1 playthrough dumps byte-identical before the Chapter 2 data is switched on |
| **4T Tests and bot** | core | [R8]; **measure the hero first** (§4.0); `content_ch2_test.cpp`; the entry fixtures; the Chapter 2 playthrough leg and pins on both routes; sweeps; setting §4.0's placeholders | `unreal/CoreTests/**`, `unreal/Source/AbyssCore/**/tools/**`, `GameData/autopilot/ch2.ts` (after 4A hands it over) | 4A, 4K; §22 O3 | §19; DR9-form numbers recorded in DECISIONS |
| **4R-1 Rig family** | art | SKEL_Quadruped, its clip library, `moon_hound`, the palettes and `theme.py` rows, the hue audit with the two new accents (first, so that a hue clash is found before any other asset) | `Art/blender/kit/skel_quadruped.py`, `kit/theme.py`, `monsters/qclips.py`, `monsters/moon_hound.py`, `palettes/*_Hangwood.json`, `vfx/hue_audit.py` | 4A (ids); §22 O1 (names on portraits and banners) | kit, ship, darkest-tile gate on the root tile, review |
| **4R-2 Roster** | art | the other monsters, the Cantor, Volgan, Wystan, husks, lineup | `Art/blender/monsters/<new files>` | 4R-1 | ship per asset, coherence, review against §1's rules |
| **4R-3 World** | art | NPCs, foliage, terrain, camp kit, set pieces, the Barrow kit, VFX, pets, icons | `Art/blender/environment/*_hangwood.py`, `npcs/*` (append), `vfx/fx_recipes_hangwood.py`, `pets/*`, `icons/*` | 4A | ship, coherence, hue audit |
| **4U UE** | UE | quadruped actors (anim map, four-foot notifies, single-view check); theme `hangwood` and the five moods; the `Drop` / `Rise` clip rule and the dormant hang pose; the dormant creak; the eclipse (ambient scale and light radius on `EvMonsterPhase` / `EvArena`, a Rules header with tests); set pieces with `when`; the Barrow presentation and cleanse; the chain as a cable between two sockets; the opened barricade in the Red Field | `unreal/Source/Abyssfire/Private/**` (new `Abyss*Rules.h` first, tested in `PresentationTests`) | 4K (events), 4R (assets; falls back to Chapter 1 assets until they land) | PresentationTests, CompileCheck; mac-content, mac-autotest `chapter2` |
| **4AU Audio** | audio | voice families Wooddead, Hound and Moonsick, the chant emitter, the mill piece, Volgan's music and stingers, the Barrow bed, the walk home's birds | `unreal/Audio/render/**` (new recipe files and score rows) | 4A (cue ids) | audio tests, `render_all.py --only` the new groups |
| **4G Gate** | gate | the switch: `REMAKE_SCHEMA` features on, the sealed gate rows flipped ([R1]), all checks, the sweep on both routes, DECISIONS rows (the re-baseline, the bands as measured, W7's amendment, DR7's amendment), docs: monsters.md §1.2 and §6 and its feed sentence; story-bible.md (the avoid-list additions, "the Gallowbones" → the Hanged, "the Tree of Life" → the Mother Oak, 缢林 → the name §22 O1 settles, Wystan in the servant's row) | commits; `unreal/Docs/**` | all | every check of §19 green; the Mac autotest for three classes |

**Order of work.** 1. 4A's dry run and tile re-check. 2. 4K [R1] (nothing ships without it) and [R7], with 4R-1 in
parallel. 3. 4A's data, 4K [R4]-[R6], 4T's hero measurement. 4. 4K [R2], [R3]; 4T [R8], the content test and the
first sweeps; 4R-2. 5. 4R-3, 4U, 4AU. 6. 4T's balance against the bands on both routes. 7. 4G.

**Size** (tasks of the size implementation-plan.md §12 counts; estimates, not commitments):

| Lane | Build | Note |
|---|---|---|
| 4A content | 9 | |
| 4K core | 10 | [R1] 3, [R4] 1.5, [R5] 1, [R7] 1, [R2] / [R3] / [R6] 0.5 each, the unverified list 2 |
| 4T tests, bot, balance | 9 | [R8] 3, the hero measurement 1, the content test 2, sweeps and retune 3 |
| Art (4R-1..3, VFX) | 64 | §17; Chapter 1's art was planned at ~55 for eleven regulars; this is seven regulars, a new rig family and a wolf boss |
| 4U UE | 10 | |
| 4AU audio | 5 | |
| 4G gate | 3 | |
| **Build total** | **~110** | Chapter 1's whole plan was ~105 |
| Review allowance | ~40 | Chapter 1's record: every package took a review round with fixes (chapter1.md §19 holds five amendment tables; Valdras's HP was set five times). About 35 % on top |
| **Total** | **~150** | |

**Cut order if the pass must shrink** (tasks saved): 1. the stretch monsters (0); 2. `pet_owl` and `pet_cat`, with S5
and S6 paying gear (~3.5 art; §22 O8); 3. the champion Lamp Patrol (0.5); 4. `hw_tam_*` consequences, [R3] and Old
Wick (~1.5); 5. `cs_hw_seal` in the Barrow, [R2] (~0.5); 6. set pieces off the route: the Moon Shrine's dressing, the
Kennel Ruins (~1.5); 7. Riftjaw's and Ashcoat's own abilities (~0.5). Already taken in this revision: two of four
hunts, the Rope-Binder and Fen-Mother variants, two of the Barrow's five rooms, the sixth mood, the door-bar rack and
the covered well (~6 tasks). **Never**: the dormant drops, the Cantor, Volgan's three acts, Wystan, [R1].

## 22. Open decisions for the project owner

Eight, each with the default this document was written to.

- **O1. The zh-CN names and a sensitivity read, before art starts.** To a native reader 缢 is in practice the word of
  自缢 (self-hanging), and the folk idiom about a hanged ghost seeking a substitute is a self-harm story; the story
  bible and the plan both name hanging and mainland rules as open risks. *Default: as written*: 绳林 / 绳上人 /
  绳林领唱 instead of the bible's 缢林 / 缢亡者 / 缢林领唱, the idiom not quoted, the rule in the wood's own words, the
  deny-list extended. A zh-CN reader should still rule on the tagline 绳不空悬, on 「拉人顶位子」 and on the chapter as
  a whole; English keeps "the Hanging Wood" and "the Hanged". The bible is updated to whatever is settled (4G).
- **O2. The climax is Volgan on the surface; the descent is a short ending with a named keeper** (DR7 says later act
  bosses go to the bottom of their descents). *Default: as written.* The alternative is to end the chapter at Volgan
  and make the Barrow a walk to the seal with no fight; it saves about three art tasks and loses the bible's third
  figure.
- **O3. The rogue's sustained damage is a precondition of the balance package.** Measured in Chapter 1: 75 per second
  at Lv 9-10 and 74 at Lv 13, against a warrior's 250-263. DR9 called it class balance and left it. No life total
  for Volgan puts every class in 30-180 s if that holds to Lv 18. *Default: fix it first* (a class-balance change
  outside this chapter), then set HP; otherwise accept a separate rogue ceiling, as DR9 did for Valdras.
- **O4. Two new enemy accents** (moon-tarnish #9FE0C8, sallow lamplight #E6D36A). *Default: accept if the hue audit
  passes against every hero recipe (frost in particular); otherwise shift moon-tarnish toward 150° and re-audit
  before art starts.*
- **O5. Tam's dilemma costs a named person** ([R3], Old Wick). *Default: build it.* The fallback is Chapter 1's
  labelled choice with one line.
- **O6. Aldwin in Knellwatch.** *Default: the data rule of §3.4* (he leaves when the hero has taken M1 and has
  nothing left with him), accepting that a hero with one of his side quests open can still find him at his stool
  after killing the Cantor. Closing it needs a stand-in giver in the core.
- **O7. Phase 4 keeps the Ember Tower hidden.** Then M5's Moon Well unlock, Gret's tower line and the wolf cub's home
  are data that nothing shows until the tower ships; the finale's line about the tower is said anyway, as Chapter 1's
  is. *Default: hidden*; [R1] separates the gate from that switch either way.
- **O8. `pet_owl` and `pet_cat` in Phase 4.** *Default: keep* (they are the web's rewards for S5 and S6 and the cub
  needs the pets panel anyway); they are second in the cut order.

**Defaults taken without asking; say so if one is wrong.** Quest ids kept, monster and lore ids new (DR3 used new
quest ids; nothing references Chapter 2 progress, so keeping them costs nothing). One golden re-baseline of
`twilight_forest` (§14), as DR4. Exit band Lv 19-21 with quest exp cut from 14700 to 5420. Gret, not the hermit,
keeps the tower's Moon Well (the bible says Gret; the web sends the hermit). Chapter 2 sweeps start from committed
entry fixtures, plus one two-chapter run per class. Bran asserts in Chapter 2 what Chapter 4 proves, now with a
stated source (the priestess's note on the rubbing).

## 23. i18n keys (zh-CN and en, parity asserted by the exporter)

| Namespace | Keys |
|---|---|
| `data.zone.twilight_forest` | 绳林 / The Hanging Wood |
| `data.quest.<id>.name / .desc / .offer / .progress / .complete / .permanent` | the 13 quests; `.permanent` on M5, S7, S8 |
| `data.questTarget.<id>`, `data.questObj.<questId>.<key>`, `data.questClue.<id>`, `data.questChoice.q_ancient_relic.<slot>` | §5-6 |
| `data.monster.<id>` | §4 (ten definitions, two hunts) |
| `data.npc.<id>.name / .dialogue.<n> / .after.<questId> / .flag.<flag>` | §3.2; one new key on `quest_elder` (§3.4) |
| `data.dialogue.quest_scout.<node>.*`, `data.dialogue.forest_hermit.<node>.*` | §3.3 |
| `data.miniBossDialogue.miniboss_cantor.line1..3` | §10 |
| `data.subDungeon.sub_hw_barrow.name` | 根冢 / The Root Barrow |
| `data.lore.<id>.*`, `data.hiddenArea.hidden_hw_moon_shrine.*`, `data.storyDeco.<id>.*`, `data.defendTarget.defend_camp_forest` | §12; 葛蕾的梯子 / Gret's Ladder |
| `data.randomEvent.<type>.*` per-zone variants, `zone.exit.sealedChapter3` | §13; 「东山路被落石封住了。山里的炉子还在响。」 / "The east road is blocked by a rockfall. The forges in the hills are still ringing." |
| `story.chapter.twilight_forest.*`, `story.<cutsceneId>.<step>`, `story.boss.volgan.name / .epithet` | §8 |

The orphan sweep drops the web's Chapter 2 keys whose ids no longer exist (`skeleton`, `zombie`, `werewolf*`,
`miniboss_shadow_weaver`, `lore_tf_*`, `cs_boss_werewolf_alpha`, `hunt_scout_ghoul`, `hunt_shade_binder`,
`hunt_rot_mother`). No native-speaker read of the zh-CN text has been done; only lengths were counted (§22 O1).

## 24. Review record (revision 2)

Three reviews of revision 1 (buildability B / M / m, story S, fights and pacing F). Each finding was checked against
the code, the golden grid and the shipped Chapter 1 data before it was applied.

| Finding | What revision 2 does |
|---|---|
| B1 clipless circle · B2 hazard with a contact hit · m17 percent DoT · m18 two contacts | `root_burst` has a clip; `moonbite` is a circle that leaves a hazard; DoTs carry absolute values; one Contact on `Attack01` (§4, §9.2, §11) |
| B3, B4, m16 wall tiles · m20 walls inside arenas | (108,74), (56,34), (48,80), (46,94) confirmed wall on the golden grid and replaced; arenas described as they are; exporter and test checks added (§10, §14, §19) |
| B5 `old_scroll` on a story decoration | the ledger is `broken_altar` (§12) |
| M1, M2 dormant respawn and variants | [R4] |
| M3 story decorations never block | the oak stands on a wall tile; no request (§9.1) |
| M4 moods are an enum · M5 set pieces have no condition · M8 a row at 100 is not in force at full life · M11 the bot | [R7], [R5], [R6], [R8] |
| M6, S11 S6 stranded after the finale; S7 offered after it | S6's shards come from Lamp-Bearers; S7 is a prerequisite of M3 and not reacceptable; the no-stranding test (§6, §19) |
| M7 the howl cannot hold a pack; two stats in one cast | the pack always breaks; the howl grants one stat (§4.4) |
| M9 abilities without `rangeMax` on tethered monsters | `rangeMax` on `lights_out`, `snap`, `moonbite`; melee-only ones listed (§9.2, §11, §19) |
| M10, F1 time-scaled hounds and ring | waves keyed to life, seven hounds for every class, a 20 s ring (§11) |
| M12 `milestone1` | [R1] says what it means; the pets slice; §22 O7 |
| M13 one clip for two motions | `Drop` and `Rise`, chosen in UE from the dormant state (§4.2) |
| m14 echoes · m15, F5 the first drop · F7 the Chantry explore point | echoes moved; the first drop is M1's by geometry; the Lantern Row spot (§5, §12) |
| m19 the ambush tell | the creak belongs to dormant entries; ambushes are hounds (§13, §16) |
| S1 self-harm reading | names, idiom, the Moon-Sick's direction, deny-list (§1); §22 O1 |
| S2 Aldwin still in Knellwatch | §3.4; §22 O6 |
| S3, S13, S14 the reveal, the index, small continuity | the rubbing and the priestess's note; the scene at the Dark Lantern; twenty and thirty years; the collar; the cub's title (§1, §8) |
| S4 three rules | one rule (§1); Old Wick (§6 S4) |
| S5 the Moon-Sick and quarantine imagery | the long-turned; no rack, no covered well; M4 no longer asks for six of them (§1, §3.1, §4.4, §5) |
| S6 two borrowed wolf names | Ashcoat, Sliver; avoid-list additions (§1) |
| S7 four beats | §8 (the howl at arrival, the dream) |
| S8, S10, S12 repeats of Chapter 1, the hero's early defiance, zh defects | `cs_hw_cantor`, `cs_hw_seal`, the finale rewritten (§8) |
| S9, F8 the keeper | Wystan: a name, a source, two words, a room he cannot be shot across (§9.2) |
| S15, S16, S17, S18 | the crescent rule restated; the lantern row and the feeding ground; the small lines; two side quests end without a kill |
| F2 levels | the roster at the arriving hero's level (§4.0) |
| F3, F10, F13 the main-path hero, the budget, the cadence | S7 on the main path, a gear choice on M3, the table rebuilt per step, a main-path sweep (§18) |
| F4 the route against the ramp | every entry on a path; the Lamp-Bearers on M3's road and in M4 (§4.7, §14) |
| F6 a camp inside the fight | safe r4 and reach 12.6 (§3.1, §9.1) |
| F9, F14 the route, the reveal 82 tiles from its speaker | turn-ins moved to Bran, trees offer across NPCs, the route written out (§0, §19) |
| F11, F12 two answers; repeats | the hound outruns the hero; the Moon-Sick has a cone and a turn; two hunts with their own abilities; the Den differs from the Run |
| F15 scope | cuts taken (~6 tasks), a size table with a review allowance (§21) |

**Not taken, or taken another way**, with the reason:

| Finding's proposal | Decision |
|---|---|
| M3: request blocking set pieces | not requested: a wall tile already blocks (§9.1) |
| F2: raise the Barrow's roster to its label | no `raiseToLevel` exists in the core runtime; the Barrow is three rooms and the keeper is the fight |
| F6: move the Crow Carts 24 tiles off | a camp tile is a generator input with a clearing; the per-camp safe radius does the same job |
| F8: end the chapter at Volgan | kept as §22 O2's alternative; the bible's third figure needs a place |
| F9: a portal that goes to the hub | not requested: the route has no round trip any more |
| F3: move about 600 exp from side to main | 160 moved and S7 (260) put on the main path; the rest left, with the main-path route swept and its ceilings stated |
| F11: a grab that roots the hero; puddles that block landing tiles | a hold is against §1's rule; the puddles matter in S7 instead |
| F15: defer `pet_owl` / `pet_cat`; cut one Lamp-Bearer | pets kept by default and placed second in the cut order (§22 O8); the Lamp-Bearers are now on the main path, so both stay |
| S2: remove Aldwin when the hero first enters the wood | there is no flag for entering a zone, and hiding a giver with open quests strands them; §3.4's rule instead |
| S15: change the cub's brow mark and the puzzle stones | the rule is restated as "no crescent as an emblem" and the natural shapes are listed |
| S1: rename now and for good | the safer names are the default; the ruling is the zh-CN reader's (§22 O1) |
