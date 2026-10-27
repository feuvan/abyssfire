# Chapter 1 · 灰烬中醒来 · 赤幡原 / Woken from Ash · The Red Field

> Tagline: 亡者不眠 / The Dead Do Not Sleep. Zone id `emerald_plains` (display 赤幡原), 120 × 120, Lv 1-10, the
> playable 3D chapter. Implementation tags: **[W1]** ships on today's core plus the small content hooks (wave 1),
> **[W2]** needs the wave-2 monster systems, **[W3]** needs sub-dungeon travel. Every [W2]/[W3] item has a stated
> [W1] fallback. All zh-CN lines below were counted against the `quests_test` limits.

## 0. At a glance

| | Today | Remake |
|---|---|---|
| Hub | the village elder's camp | 丧钟堡 Knellwatch, the crusade's last fort |
| Main chain | 史莱姆之灾 → 火光之眼 → 篝火营地 → 碎牙之王 → 灵脉之印 | 收尸队 → 啃食之野 → 灰麦村 → 不眠的元帅 → 最后的钟声 |
| Mini-boss | goblin shaman (ranged) | 尸牧婆 the Corpseherd (raiser), on the main path (M3) |
| Act boss | goblin chief, 160 HP, one phase | 焚旗元帅·瓦德拉斯 Lord-Marshal Valdras, ~1100 HP, three phases |
| Finale | explore two points | a dawn siege at the Boneyard Gate, then the bell |
| Descent | none | 信者之坑 the Pit of the Faithful [W3] |
| Quests | 11 (5 main, 6 side), 2660 exp / 505 gold | 11 in W1 (5 main, 6 side), 2740 / 510; +1 side in W3 |
| Population | 55 in 10 spawn entries | 55 in the same 10 entries |

### Id map (old slot → remake id)

Quests keep their slot's order in `GameData/quests/all_quests.ts` and every map-generator input of their slot
(§17), so only M5's change moves the golden map.

| Slot (web id) | Remake id | Name | Giver |
|---|---|---|---|
| q_kill_slimes | `q_rf_burial_detail` | 收尸队 / The Burial Detail | `quest_elder` |
| q_kill_goblins | `q_rf_gnawed_field` | 啃食之野 / The Gnawed Field | `quest_elder` |
| q_explore_goblin_camp | `q_rf_ashwheat` | 灰麦村 / Ashwheat | `quest_elder` |
| q_find_goblin_chief | `q_rf_marshal` | 不眠的元帅 / The Sleepless Marshal | `quest_elder` |
| q_secure_plains | `q_rf_last_toll` | 最后的钟声 / The Last Toll | `quest_elder` |
| q_collect_slime_gel | `q_rf_black_blood` | 黑血 / Black Blood | `quest_elder` |
| q_herb_gathering | `q_rf_barrowmoss` | 苦冢苔 / Barrowmoss | `knell_chaplain` |
| q_lost_pendant | `q_rf_tag_thief` | 名牌窃贼 / The Tag-Thief | `knell_chaplain` |
| q_bandit_trouble | `q_rf_oathless` | 悬赏：断誓者哈克 / Bounty: Harke the Oathless | `plains_wanderer` |
| q_pet_sprite_friend | `q_rf_lanternwisp` | 灯魂 / The Lanternwisp | `knell_chaplain` |
| q_escort_merchant_plains | `q_rf_hallowed_ground` | 最后的圣地 / Hallowed Ground | `knell_chaplain` |
| (new, W3) | `q_rf_the_pit` | 信者之坑 / The Pit of the Faithful | `knell_chaplain` |

| Old monster / beat | Remake |
|---|---|
| slime_green | `breachclot` (and `cinderbloat`) |
| goblin | `risen_footman`, `gravegnawer`, `oath_zealot`, `deserter`, `risen_arbalest` |
| goblin_chief | `valdras` (storyBoss) |
| miniboss_goblin_shaman | `miniboss_corpseherd` |
| hunt_pendant_thief / hunt_redcap_gruk | `hunt_dreck` / `hunt_harke`; new `hunt_hungerbell` |
| cs_ep_mark / cs_ep_whisper / cs_ep_finale / cs_boss_goblin_chief | `cs_rf_brand` / `cs_rf_whisper` / `cs_rf_finale` / `cs_boss_valdras`; new `cs_rf_herd`, `cs_rf_marshal_falls`, `cs_rf_siege`, `cs_rf_pit` [W3] |
| lore_ep_01..04, hidden_ep_elven_cache, story_ep_* | `lore_rf_01..04` + `lore_rf_echo_01..03`, `hidden_rf_paymaster`, `story_rf_belfry` / `story_rf_red_banner` / `story_rf_penitents_altar` |
| ach_kill_slime / ach_kill_goblin_chief | `ach_kill_breachclot` / `ach_kill_valdras` |

## 1. The zone: a gradient from the walls outward

North-west is the fort and the dead you knew; the south-west is the battlefield proper and the breach. Mechanics arrive
one at a time (monsters.md §1.4).

| Area | zh | Tile | Lv | What is there |
|---|---|---|---|---|
| Knellwatch (hub) | 丧钟堡 | camp (15,15), safe r9 | — | the hearth, the bell frame, the Name Wall, six NPC slots |
| The burn pit | 焚尸坑 | playerStart (15,22) | — | where the hero wakes; the Pit entrance at (17,26) [sealed until W3] |
| The Boneyard Gate | 埋骨门 | (10,32) | — | where the burial road leaves the fort; M5's siege |
| Burial Trenches | 埋骨壕 | spawn (25,20), M1 area (32,12) r12 | 1-2 | risen footmen (the burial detail); barrowmoss along the edge (22,26) |
| The Seep | 渗血洼 | spawn (20,45) | 2 | breachclots in black pools |
| The Gnawed Field | 啃食之野 | spawn (35,40), M2 area (34,40) r15 | 3 | gravegnawers; the Marshal's Last Order on a post (45,35); Hungerbell prowls there |
| Marl's dressing station | 救护站 | field NPC (50,30) | — | a roofless toll-house, cots, a lantern |
| Ashwheat | 灰麦村 | (40,52) r8 | 5 | the burned hamlet across the burned bridge (38,48); doors barred from inside, knocking behind them; the split bell (55,45) |
| The Sheepfold | 尸栏 | mini-boss (60,55) | 6 | the Corpseherd's ring of overturned crusade wagons |
| The Weeping Ditch | 泣沟 | spawn (85,18) | 4 | cinderbloats; the lantern graves (78,12) (92,10) (100,22) (86,28); a deserter's journal (105,15, hidden) |
| The Penitents' Camp | 赎罪营 | spawn (95,35) | 6 | Red-Oath zealots; the Penitents' Altar (88,55) |
| The Broken Redoubt | 断垒 | spawn (75,50) | 5 | the dead crossbow company still manning the wall |
| Corran's culvert | 逃兵沟 | field NPC (70,65) | — | Corran the deserter in a dry culvert |
| Deserters' Hollow | 逃兵洼 | spawn (80,75); Harke (82,72) | 6 | deserters; the Catechism Stone (85,80) |
| Crowmarket | 鸦集 | camp 2 (95,100) | — | the Gleaners' camp; the midden (95,90): bonegorgers |
| The Paymaster's Wagon | 军饷车 | hidden area (108,108) r6 | — | a chained strongbox |
| The Red Banner | 赤幡 | (40,70) | 7 | a banner on a mound of shields facing Knellwatch |
| The Dead Ranks | 亡阵 | spawn (30,80) | 7 | the shield wall that never broke |
| The last hallowed ground | 最后的圣地 | (50,90) | — | escort destination, by the black lake |
| The Breach Rim | 渊裂之缘 | storyBoss (15,95) | 9 | the cracked dais (the Seal of Veins), banner ring, violet uplight; shard of the seal (15,108, hidden) |
| East road | 辎重垒 | exit (119,60) | — | sealed by a barricade of wagons and shields, breach fire beyond (W7) |

## 2. Hub: 丧钟堡 / Knellwatch (camp 1, (15,15))

**What it is.** A toll-fort on the border road, older than the church, raised around a bronze bell nobody remembers
casting (the keepers' ley-iron bell). Two hundred people live inside: the ones the Captain's retreat saved and
Ashwheat's few survivors.

**Dressing** (camp layout v2 slots re-pointed by the presentation theme `march`, same footprints; extra pieces through
the UE set-piece layer, implementation-plan.md §4.4):

| Camp slot / piece | Remake look |
|---|---|
| palisade, gate posts | chapel pews, cart wheels and crusade shields nailed points-out; helmets on the gate posts |
| tent | the **bell frame**: a squat timber belfry, the cracked bell hung low enough to touch |
| campfire | **the hearth**: the only warm light in the zone (#FFB060), stools, a blanket line |
| banner | Knellwatch's banner: a black field with a bell |
| barrels, crates | crusade supply crates, ration sacks counted in chalk |
| well | the well, buckets, a ladle chained to it |
| (set piece) the Name Wall | hundreds of bronze name-tags nailed in rows on the inner palisade |
| (set piece) the forge | Hald's anvil stump beside a heap of swords waiting to be melted |
| (set piece) the chapel crypt door | where the stash lives |

**Life.** Children play "burial detail" with sticks in the safe area (ambient props only, never in combat). Every dusk,
and at the end of every main turn-in cutscene, the bell tolls once per name. Barks are short: 「又少了三份口粮。」 (Three
fewer rations again.) 「别盯着坑看。」 (Don't stare at the pit.)

**Second camp: 鸦集 / Crowmarket (95,100).** The Gleaners: carts of salvage, boots strung on lines, crows on the cart
rails, a cookfire that is not quite warm. Mag Crowe trades here.

## 3. NPCs

NPC ids are kept as role keys; looks are re-dressed from the folk kit (`npcs/looks.py`); speaking NPCs get portraits.
Camp 1's list becomes `['blacksmith', 'merchant', 'quest_elder', 'knell_chaplain']` (+ the stash keeper the port appends,
I7): five of the six camp-layout NPC slots. Camp NPC lists are not map-generator inputs.

| Id | Name | Type | Where | Role | Look brief | Line (`data.npc.<id>.dialogue.1`) |
|---|---|---|---|---|---|---|
| `quest_elder` | 瑟琳·哈洛队长 / Captain Serin Harrow | quest | camp 1 | main giver, S1; the 13-node tree | dented half-plate, the church sigil cut out of her tabard, cropped grey hair, a burn scar on the neck | 坑里爬出来的。能站着，就能拿刀。 / Out of the pit and on your feet. Then you can hold a blade. |
| `knell_chaplain` (new) | 奥德温修士 / Brother Aldwin | quest | camp 1 | reads the brand; gives S2, S3, S5, S6, S7 | threadbare cassock, ash on the brow, a handbell on a cord | 钟声是给活人听的。死人……另有人在跟他们说话。 / The bell is for the living. The dead... someone else is talking to them. |
| `blacksmith` | 哈尔德·熔剑 / Hald Swordmelter | blacksmith | camp 1 | forge, crafting | soot apron, both sons lost to the crusade | 每把剑的柄脚都刻着名字。剑我熔了，名字留给队长。 / Every tang has a name on it. I melt the sword and give the name to the Captain. |
| `merchant` | 军需官薇拉 / Quartermaster Vira | merchant | camp 1 | wares (as today, minus I2/I4 items) | a ledger, a ration-tally cord | 一个名字，省下一份口粮。今晚又省了三份。 / One name, one ration saved. Three saved tonight. |
| `stash` | 守冢人塔姆欣 / Tamsin the Gravewarden | stash | camp 1 | stash ("the belongings nobody claimed") | grave-digger's coat, a ring of crypt keys | 死人的东西我替他们收着。活人的，也行。 / I keep the dead's things for them. The living's too. |
| `plains_herbalist` | 军医玛勒 / Mother Marl | merchant | field (50,30) | potions; S1 talk target; leaves for the tower in the finale | leather apron, rolled sleeves, a lantern, stained gloves | 别碰那黑血。碰过的人都不喊疼了——那才最疼。 / Don't touch the black blood. The ones who did stopped hurting. That's the worst of it. |
| `plains_wanderer` | 逃兵柯兰 / Corran the deserter | quest | field (70,65) | S4 bounty | torn-off insignia, a hood, a looted pack | 我跑了，我活下来了。这两件事，哪件更丢人？ / I ran and I lived. Which is the greater shame? |
| `crow_trader` (new; camp 2 used the shared `merchant`) | 鸦嫂玛格 / Mag Crowe | merchant | camp 2 | Gleaner wares (same shop table as camp 2 today) | crow-feather collar, boots slung over a shoulder | 别问靴子是谁的。问价钱。 / Don't ask whose boots. Ask the price. |
| escort (`data.escortNpc.rf_hallowed_ground`) | 掘墓人伯恩 / Old Bern the Gravedigger | escort | (30,40) → (50,90) | S6 | pushes a barrow of shrouded dead (attachment) | 一车十二个，都有名字。我一个个念给土听。 / Twelve to a cart, all named. I read each one to the earth. |
| rescue event | 被缚的朝圣者 / the Bound Pilgrim | event | random | rescue | pilgrim's cloak, rope at the wrists | 他们说我走得太慢，跟不上圣火。 / They said I walked too slowly to keep up with the Flame. |
| wandering merchant | 圣骨贩子 / the Relic Peddler | event | random | merchant | a tray of bone-white relics | 圣骨，真货。至少有一块是真的。 / Saint's bones, genuine. At least one of them is. |

Quest-gated lines [W2 hook; W1 fallback: the first line only]:

| NPC | After | zh-CN | en |
|---|---|---|---|
| `blacksmith` | `q_rf_tag_thief` | 那孩子的名牌，我钉在炉边了。打铁的时候，看得见。 | I nailed the boy's tag by the forge. I can see it while I work. |
| `quest_elder` | `q_rf_oathless` | 逃兵就是逃兵。……我也是。 | A deserter is a deserter. ...So am I. |
| `knell_chaplain` | `q_rf_last_toll` | 愿钟声护你。……我会一直听着它。 | May the bell keep you. ...I will always be listening to it. |
| `knell_chaplain` | (always, second line) | 我替每个名字念一遍经。念到第三百个，我开始怕自己忘了念给谁。 | I say a prayer for every name. By the three hundredth I began to fear I'd forget who it was for. |

*K6 as built (2E, pending review):* the hook is npcs.json `lines [{key, when?}]` (`when`: `{quest: <id>, state:
turnedIn}`, or a flag): the lines that hold replace `dialogue`; the first is logged on interaction, the linear panel shows
the second (else the first); none holding keeps the W1 lines. Author the "after" line first, gate the W1 line it replaces
with the opposite state (`{quest: <id>, state: notTurnedIn}`) and keep the other W1 lines without a `when`, so the same
slots hold before and after: the chaplain is `[{after, when: turnedIn}, {line 0, when: notTurnedIn}, {line 1}]` (logged:
line 0, then the after line; the panel keeps his always-on second line). Listing the after line in front of both W1 lines
would push line 1 off the panel once the quest is turned in (implementation-plan.md *2E outcomes*; 2E review R6).

## 4. The Captain's dialogue tree (`quest_elder`, 13 nodes)

New node ids; the tree keeps 13 nodes, three quest triggers and the one-time reward (Q1).

| Node | zh-CN | en | Choices → |
|---|---|---|---|
| `root` | 坑里爬出来的。能站着，就能拿刀。 | Out of the pit and on your feet. Then you can hold a blade. | 你是谁？ Who are you? → `who` · 那口钟…… That bell... → `bell` · 有什么要我做的？ What needs doing? → `help` · 有饷吗？ Is there pay? → `reward_ask` · 告辞。 Farewell. → `farewell` |
| `who` | 瑟琳·哈洛，第三营队长。现在是这座堡里军衔最高的活人。 | Serin Harrow, Third Company. Now the highest-ranking living soldier in this fort. | 元帅呢？ And the Marshal? → `marshal` · 回去 Back → `root` |
| `bell` | 钟比教会还老，是守火人留下的。黄昏时，一个名字敲一下。 | The bell is older than the church; the keepers left it. At dusk, one toll for each name. | 回去 Back → `root` |
| `marshal` | 元帅下令死守，我下令撤退。我们俩，总有一个是叛徒。他的弩手死了都不放下弩。 | The Marshal ordered us to hold. I ordered the retreat. One of us is a traitor. His crossbowmen are dead and still won't put their bows down. | 回去 Back → `root` |
| `help` | 北边埋骨壕。收尸队没回来。 | The burial trenches, north. The burial detail hasn't come back. | 我去。 I'll go. → `accept_burial` · 城下的麦田呢？ And the wheat below the walls? (requires `q_rf_burial_detail` turned in) → `gnawers_first` · 还有别的吗？ Anything else? → `other_work` |
| `accept_burial` | 带上灯。别带希望。 | Take a lantern. Leave hope. | questTrigger `q_rf_burial_detail` |
| `gnawers_first` | 城下的麦田里，有东西在吃我们的人。 | In the wheat below the walls, something is eating our people. | 交给我。 Leave it to me. → `accept_gnawers` |
| `accept_gnawers` | 十二只。还有那个敲钟的。 | Twelve of them. And the one ringing the bell. | questTrigger `q_rf_gnawed_field` |
| `reward_ask` | 饷？这是死人的饷。拿去吧，他用不上了。 | Pay? This is a dead man's pay. Take it; he won't need it. | one-time 30 gold / 50 exp (Q1) |
| `other_work` | 玛勒要血样。神父那边也缺人手。南边沟里有个逃兵，在等人听他说话。 | Marl wants blood samples. The chaplain needs hands too. There's a deserter in the southern culvert waiting for someone to listen. | 血样？ Blood samples? → `accept_ichor` · 神父？ The chaplain? → `chaplain_pointer` |
| `accept_ichor` | 别用手碰。玛勒会教你。 | Don't touch it bare-handed. Marl will show you. | questTrigger `q_rf_black_blood` |
| `chaplain_pointer` | 奥德温还在祈祷。我不知道，还有谁在听。 | Aldwin still prays. I don't know who's still listening. | 回去 Back → `root` |
| `farewell` | 天黑前回来。天黑以后，城门不开。 | Be back before dark. After dark the gate stays shut. | — |

The AutoPilot's tree walk becomes `reward_ask → help → other_work → accept_ichor` (read from data,
implementation-plan.md §6).

## 5. Main quests

Rewards: exp / gold; main gear choices are rare by default; embers default 2 per main turn-in. Lines: offer ≤ 70,
progress ≤ 50 (new `progressKey`, [W1 hook]), complete ≤ 50.

### M1 `q_rf_burial_detail` 收尸队 / The Burial Detail — kill, L1

| Field | Value |
|---|---|
| Giver | `quest_elder` (tree `accept_burial` or the quest card) |
| Objectives | 1. kill `risen_footman` ×10 |
| questArea | (32,12) r12 (slot kept) |
| Rewards | 120 exp / 25 gold |
| Triggers | turn-in → `cs_rf_brand` |

| Line | zh-CN | en |
|---|---|---|
| offer | 收尸队黄昏时出了门，带着一盏灯。一个时辰前，那盏灯不动了。去北边的埋骨壕。要是他们站起来了……别犹豫。 | The burial detail went out at dusk with one lantern. An hour ago the lantern stopped moving. Go north to the trenches. If they're standing... don't hesitate. |
| progress | 灯还在壕沟里。人，一个都没回来。 | The lantern's still in the trench. Not one of them came back. |
| complete | 十个人，我弟弟是第七个。……去找神父吧，他想看看你的手。 | Ten men. My brother was the seventh. ...Go to the chaplain. He wants to see your hand. |

Beat: the footmen at the trenches are still digging (idle variant `Work`); the dropped lantern lies at (29,15) beside
the echo `lore_rf_echo_01` (the seventh man).

### M2 `q_rf_gnawed_field` 啃食之野 / The Gnawed Field — kill, L3, prereq M1

| Field | Value |
|---|---|
| Objectives | 1. kill `gravegnawer` ×12 · 2. kill `hunt_hungerbell` ×1 |
| Hunt | `hunt_hungerbell` 饿钟 / Hungerbell: base `bonegorger`, hp ×3, dmg ×1.3, at (45,35), `revealAfterPrevious`, 3 `gravegnawer` minions |
| questArea | (34,40) r15 (slot kept) |
| Rewards | 220 exp / 40 gold |
| World state | turn-in sets `rf_hungerbell_silenced`: the distant night ringing over the Gnawed Field stops [W2 hook; W1: the ringing loop ends when the hunt dies] |

| Line | zh-CN | en |
|---|---|---|
| offer | 后卫营死在城下的麦田里。现在有东西在吃他们。被啃掉的，就再也埋不了了。 | The rearguard died in the wheat below our walls. Now something is eating them. What gets eaten can never be buried. |
| progress | 听见那钟声没有？不是我们的钟。 | Do you hear that bell? It isn't ours. |
| complete | 那钟锤是灰麦村的。它每夜都在敲——原来是在叫它们开饭。 | That clapper is from Ashwheat's bell. It rang every night. It was calling them to supper. |

### M3 `q_rf_ashwheat` 灰麦村 / Ashwheat — explore, L5, prereq M2

| Field | Value |
|---|---|
| Objectives | 1. explore `zone_rf_ashwheat` (40,52) r8 · 2. kill `miniboss_corpseherd` ×1 |
| Rewards | 260 exp / 40 gold; unlocks the Ember Tower (`towerUnlockQuest`; tower UI hidden in milestone 1 per Q3) |
| Triggers | turn-in → `cs_rf_whisper` |

Ashwheat is empty. The bridge north of it (38,48) is burned to the stumps over a dry ditch. Candles burn in every
window; the doors are barred **from inside** and something knocks behind them; nothing comes out (audio emitters on
the set pieces). The villagers' dead were led away: wagon ruts and bare footprints run east to the Sheepfold, where the
Corpseherd keeps them walking. This puts the raiser lesson on the main path before the boss.

| Line | zh-CN | en |
|---|---|---|
| offer | 斥候说灰麦村有火光。灰麦村不该还有人。去看看是谁点的灯，再看看村里的人，被领去了哪儿。 | Scouts saw lights in Ashwheat. There shouldn't be anyone left there. Go and see who lit them, and where the villagers were led. |
| progress | 灰麦村……你到了就知道了。 | Ashwheat... you'll understand when you get there. |
| complete | 桥是我烧的。他们在对岸喊，我听见了。我还是烧了。 | I burned the bridge. They were calling from the far bank. I heard them. I burned it anyway. |

### M4 `q_rf_marshal` 不眠的元帅 / The Sleepless Marshal — kill, L7, prereq M3

| Field | Value |
|---|---|
| Objectives | 1. kill `valdras` ×1 (storyBoss; M7 spawn rules kept, configured in `GameData/config.ts`) |
| questArea | data (25,65) r10 (slot kept for the golden map); re-centred on the boss spawn at runtime by Q4 |
| Rewards | 500 exp / 80 gold + choice [weapon, armor] |
| Triggers | boss intro `cs_boss_valdras` at 9 tiles; kill → `cs_rf_marshal_falls` |

| Line | zh-CN | en |
|---|---|---|
| offer | 我下令撤退，带回两百人。他下令死守，死了三千。他还守在裂口边上。去告诉他，仗打完了。 | I ordered the retreat and brought back two hundred. He ordered us to hold, and three thousand died. He's still holding, at the edge of the breach. Go and tell him the war is over. |
| progress | 他不会退。你也别退。 | He won't fall back. Don't you either. |
| complete | 他问是谁下的令？……你该告诉他，是我。 | He asked who gave the order? ...You should have told him. It was me. |

### M5 `q_rf_last_toll` 最后的钟声 / The Last Toll — defend, L8, prereq M4 (chapter-complete quest)

| Field | Value |
|---|---|
| Objectives | 1. `defend_wave` at `defendTarget` 埋骨门 / the Boneyard Gate (10,32), `totalWaves` 3, `approachDeg` pointing away from the camp [W1 hook] |
| Waves [W1 hook: `waves[]`] | 1: `risen_footman` ×5 · 2: `gravegnawer` ×4 + `cinderbloat` ×2 · 3: `risen_shieldbearer` ×2 + `dead_standard_bearer` ×1 + `oath_zealot` ×2 (scaled per wave by the existing defend rules) |
| Rewards | 450 exp / 60 gold + choice [jewelry, boots, gloves] + 5 embers; unlocks `herb_garden` |
| Triggers | accept → `cs_rf_siege`; turn-in → `cs_rf_finale` (`chapterCompleteQuest`) |

The gate is 18 tiles from the camp centre, so neither the waves (spawned on the far half-ring, r8) nor a hero defending
the gate can use the camp's safe radius (9). The gate's HP uses today's defend formula; a defend wave never respawns
(M4). The regular trench spawn at (25,20) sits 13 tiles away; its footmen may join, which is intended.

| Line | zh-CN | en |
|---|---|---|
| offer | 元帅倒了，裂口在喘。今夜，赤幡原上的死人都会朝这边走。去埋骨门，守到天亮。 | The Marshal is down and the breach is gasping. Tonight every dead man on the Red Field walks this way. Go to the Boneyard Gate and hold it till dawn. |
| progress | 门还在？那就再守一会儿。天快亮了。 | The gate still stands? Then hold a little longer. It's nearly dawn. |
| complete | 天亮了，钟还挂着。……东边的路，该有人去走了。 | Dawn, and the bell still hangs. ...Someone should take the east road now. |

## 6. Side quests

Side gear choices are magic by default; embers default 1 per side turn-in. Each side quest has a distinct shape (the
`quests_test` rule) and a loss in it.

### S1 `q_rf_black_blood` 黑血 / Black Blood — talk, L2, giver `quest_elder`

| Field | Value |
|---|---|
| Objectives | 1. collect `mat_breach_ichor` ×6 (drop from `breachclot`, chance 0.5, itemKind `ichor`) · 2. talk `plains_herbalist` |
| questArea | (25,20) (slot kept) |
| Rewards | 110 exp / 35 gold + 2 × `c_hp_potion_s`; [W2] + permanent +15 max life, 「玛勒的苦药」 Marl's Bitter Draught (once per difficulty) |

| Line | zh-CN | en |
|---|---|---|
| offer | 玛勒在城外的救护站。沾了渊血的伤员会变灰，然后不再喊疼。她要六团还温着的血块。 | Marl keeps a dressing station outside the walls. The wounded who touch breach-blood turn grey and stop crying out. She needs six clots, still warm. |
| progress | 别用手碰。玛勒会教你。 | Don't touch it bare-handed. Marl will show you. |
| complete (Marl) | 血里有声音……是死人的声音。它记得他们。 | There are voices in the blood... the dead's. It remembers them. |

### S2 `q_rf_barrowmoss` 苦冢苔 / Barrowmoss — collect, L2, giver `knell_chaplain`

| Field | Value |
|---|---|
| Objectives | 1. collect `mat_barrowmoss` ×5 (gather, area (22,26) r9, 7 nodes, itemKind `moss`) |
| questArea | (20,25) (slot kept) |
| Rewards | 70 exp / 20 gold + `c_hp_potion_s` |

Gather spots are seeded by `q_rf_barrowmoss:0`, so they move once; the gather-hash pin is re-pinned.

| Line | zh-CN | en |
|---|---|---|
| offer | 旧礼说：在坟上烧苦冢苔，死人就睡得沉些。埋骨壕边长着一些。采五把来——在他们醒来之前。 | The old rite says: burn barrowmoss over a grave and the dead sleep deeper. Some grows by the trenches. Bring five handfuls, before they wake. |
| progress | 苔要连根拔。根里有灰，灰才压得住。 | Pull it by the root. The ash is in the root; the ash is what holds them down. |
| complete | 闻到了吗？像雨后的灰。今晚，他们会睡得安稳些。 | Smell that? Like ash after rain. Tonight they'll sleep a little better. |

### S3 `q_rf_tag_thief` 名牌窃贼 / The Tag-Thief — investigate, L4, prereq M1, giver `knell_chaplain`

| Field | Value |
|---|---|
| Objectives | 1-3. investigate `clue_rf_tag_cord` (30,24), `clue_rf_tag_cloth` (42,34), `clue_rf_tag_ashes` (54,44) · 4. kill `hunt_dreck` · 5. collect `mat_tag_pouch` ×1 (drop from `hunt_dreck`, chance 1, itemKind `tags`) |
| Hunt | `hunt_dreck` 拾银者·德雷克 / Dreck the Silverpicker: base `deserter`, hp ×3, dmg ×1.2, at (60,48), `revealAfterPrevious`, 2 `deserter` minions |
| questArea | (54,44) (slot kept) |
| Rewards | 180 exp / 40 gold + choice [jewelry, weapon] with labels [W1 hook `choiceLabels`]: jewelry = 「留下熔银」 Keep the melted silver (a ring cast from it); weapon = 「交给哈尔德」 Give it to Hald (he forges you a blade). Hald's line changes after the turn-in |

| Clue | zh-CN | en |
|---|---|---|
| `clue_rf_tag_cord` | 一根割断的名牌绳，切口很新。 | A cut tag-cord. The cut is fresh. |
| `clue_rf_tag_cloth` | 铁蒺藜上挂着一片圣战军的罩袍。 | A scrap of crusade tabard on the caltrops. |
| `clue_rf_tag_ashes` | 一堆余火，灰里有熔化的银。 | A banked fire. Melted silver in the ash. |

| Line | zh-CN | en |
|---|---|---|
| offer | 有人在割死人脖子上的名牌。没有名牌，钟就不知道该为谁敲。找到他。 | Someone is cutting the name-tags off the dead. Without a tag, the bell doesn't know whom it tolls for. Find him. |
| progress | 一块名牌，一个名字。少一块，就少敲一下。 | One tag, one name. One tag missing, one toll missing. |
| complete | 四十一个名字。哈尔德的小儿子也在里面。……银子，你自己拿主意。 | Forty-one names. Hald's youngest is among them. ...The silver is yours to decide. |

### S4 `q_rf_oathless` 悬赏：断誓者哈克 / Bounty: Harke the Oathless — kill (hunt), L7, prereq M2, giver `plains_wanderer`

| Field | Value |
|---|---|
| Objectives | 1. kill `hunt_harke` |
| Hunt | `hunt_harke` 断誓者·哈克 / Harke the Oathless: base `deserter`, hp ×5, dmg ×1.6, at (82,72), spawns on accept, 4 `deserter` minions; [W2] his men break and flee 5 s when he falls |
| questArea | (80,75) (slot kept) |
| Rewards | 260 exp / 60 gold + choice [weapon, offhand] with labels: weapon = 「人头给柯兰」 Give the head to Corran (he gives you his old sword); offhand = 「人头给队长」 Give it to the Captain (she gives you a crusade shield). The Captain's line after either: 「逃兵就是逃兵。……我也是。」 |

Harke sells captured pilgrims to the Red-Oath. The dilemma is Corran's: his old sergeant's head is the price he means to
pay for Knellwatch's gate.

| Line | zh-CN | en |
|---|---|---|
| offer | 哈克是我的老军士，他把抓来的朝圣者卖给赤誓会。我要他的人头——拿它换丧钟堡的门。 | Harke was my sergeant. He sells captured pilgrims to the Red Oath. I want his head. It's my price for Knellwatch's gate. |
| progress | 他在逃兵洼。他认得我的脸，不认得你的。 | He's in Deserters' Hollow. He knows my face. He doesn't know yours. |
| complete | 头给我，还是给队长？……你定。我认了。 | The head to me, or to the Captain? ...You choose. I'll live with it. |

### S5 `q_rf_lanternwisp` 灯魂 / The Lanternwisp — investigate, L3, giver `knell_chaplain`

| Field | Value |
|---|---|
| Objectives | investigate `clue_rf_lantern_1..4` at (78,12) (92,10) (100,22) (86,28): fresh graves, each with a guttering candle stub |
| questArea | (88,18) (slot kept) |
| Rewards | 220 exp / 50 gold + `petReward` `pet_sprite` (re-dressed as 灯魂 / the Lanternwisp: a lantern-gold core, soot wisps, a tiny bronze bell) + 2 embers |

The graves are in the Weeping Ditch among cinderbloats. [Stretch] cold false lights (`false_lantern`, monsters.md §2.6)
drift among the graves; the true lantern is warm.

| Line | zh-CN | en |
|---|---|---|
| offer | 孩子们说，每夜有一点灯光在泣沟一带走，从一座坟走到另一座。去看看它最后停在哪儿。别跟错了灯。 | The children say a light walks the Weeping Ditch each night, from grave to grave. See where it stops. Don't follow the wrong light. |
| progress | 真灯是暖的。冷的那些，别伸手。 | The true one is warm. The cold ones, keep your hands off. |
| complete | 是佩尔的灯。收尸队里最小的那个。它愿意跟你，就带它走吧。 | That's Pell's lantern. The youngest of the burial detail. If it wants to follow you, take it. |

### S6 `q_rf_hallowed_ground` 最后的圣地 / Hallowed Ground — escort, L5, prereq M2, reacceptable, giver `knell_chaplain`

| Field | Value |
|---|---|
| Objectives | escort Old Bern and his barrow from (30,40) to the last hallowed ground (50,90) |
| Escort | HP = level × 20 + 100 = 200 and the chip rules unchanged (monsters within 4 tiles chip max(1, floor(dmg × 0.3)) every 2 s; presented as the dead tugging at the cart) |
| Rewards | 350 exp / 60 gold + `c_hp_potion_m`; [W2] + 3 permanent stat points (once per difficulty) |

| Line | zh-CN | en |
|---|---|---|
| offer | 伯恩要把车上的人送进南边的圣地。天黑前入不了土，他们就会自己走路。护着他。 | Bern is taking a cart of the dead to the hallowed ground in the south. If they're not in the earth by nightfall, they'll walk on their own. Guard him. |
| progress | 车轮响一下，就是一个名字。 | Every turn of the wheel is a name. |
| complete | 圣地还肯收他们。……也许是最后一次了。 | The ground still takes them. ...Perhaps for the last time. |

### S7 `q_rf_the_pit` 信者之坑 / The Pit of the Faithful — [W3], L5, prereq M2, giver `knell_chaplain`

| Field | Value |
|---|---|
| Objectives | 1. kill `sub_pitwarden` ×1 (in `sub_rf_pit`) · 2. `clear_area` `sub_rf_pit` (≤ 5 hostiles left, new objective type) |
| Rewards | 300 exp / 50 gold + permanent +1 skill point (once per difficulty; a free respec once the respec UI exists) |
| Triggers | entering `sub_rf_pit` → `cs_rf_pit` |
| Before W3 | the entrance at (17,26) shows a smoking, sealed pit mouth: 「坑口还在冒烟。等火熄了，再下去。」 (The pit mouth is still smoking. Wait for the fire to die.) The quest is not offered |

| Line | zh-CN | en |
|---|---|---|
| offer | 你醒来的那个坑，底下还有一层。火熄了，可坑底有东西在挖。下去看看——那是你的来处。 | The pit you woke in has another floor beneath it. The fire's out, but something down there is digging. Go and look. That's where you came from. |
| progress | 坑底不见天。带着你掌心的光。 | There's no sky down there. Take the light in your palm. |
| complete | 坑底的石龛空了，五道焦痕围着它。……那团火，在下面等了你一千年。 | The niche at the bottom is empty, with five burn marks round it. ...That fire waited a thousand years for you down there. |

### S8 `q_rf_reliquary` 黑匣圣骸 / The Black Reliquary — stretch, outline only

Collect 3 relic fragments from `oath_zealot`; at the hearth choose: keep (a unique amulet 赤誓护符, +damage, −life
regeneration) or burn (a permanent small blessing). Needs a non-gear choice kind; not scheduled.

## 7. Totals

| | Exp | Gold | Embers |
|---|---|---|---|
| Main (M1-M5) | 1550 | 245 | 2 × 5 + 5 = 15 |
| Side (S1-S6) | 1190 | 265 | 1 × 6 + 2 = 8 |
| **Wave 1 total** | **2740** | **510** | 23 from turn-ins (kills add the rest; the playthrough pin is ≥ 40) |
| S7 [W3] | +300 | +50 | +1 |

Today: 2660 / 505. Quest counts per giver: `quest_elder` 6, `knell_chaplain` 4 (5 with S7), `plains_wanderer` 1.

## 8. Story beats

Boss build-up in four beats: (1) the Captain names Valdras in the tree (`marshal`) and in M4's offer; (2) his Last Order
on the post at (45,35) and the oath-scar on Risen and zealots; (3) the dream in `cs_rf_whisper`; (4) the boss intro
at 9 tiles, with the music cut inside 16 tiles of the rim.

### Prologue (6 slides)

| # | Mood | Title | zh-CN | en |
|---|---|---|---|---|
| 1 | embers | 渊火 / Abyssfire | 世界在深渊之底的火中锻成。六位守火人立誓看护这团火：让它温暖，而不焚烧。 | The world was forged in the fire at the bottom of the Abyss. Six keepers swore to tend it: to let it warm, and never burn. |
| 2 | night | 焚誓 / The Burned Oath | 六人中最耀眼的一位，最怕火灭。他焚毁誓言，要把世界投回火中重铸。其余五人以命为锁，将他缚于深渊，立下五道封印，并凿去了他的名字。 | The brightest of the six feared most that the fire would go out. He burned his oath to throw the world back into the fire and recast it. The other five made locks of their lives, bound him in the deep under five seals, and chiselled away his name. |
| 3 | abyss | 低语 / The Whisper | 千年来，他只能说话。他在黑暗里寻找害怕终结的人，许给他们一团永不熄灭的火。 | For a thousand years he could only speak. In the dark he looked for people who feared the end, and promised them a fire that would never go out. |
| 4 | embers | 灰誓圣战 / The Ashen Crusade | 今年秋天，守焰教会听见了“圣火的召唤”，灰誓圣战开赴东境。最后一战前，元帅瓦德拉斯下令：死守赤幡原，一步不许退。 | This autumn the Church of the Kept Flame heard "the Flame's call", and the Ashen Crusade marched to the eastern border. Before the last battle Lord-Marshal Valdras gave his order: hold the Red Field. Not one step back. |
| 5 | abyss | 渊裂 / The Breach | 那一战的血流得太多。赤幡原下的古印裂开了：裂口涌出黑血与魔物。战死的人没有躺下，他们还在执行最后的军令。 | Too much blood was spilled. The old seal beneath the Red Field cracked: black blood and worse welled out of the breach. The slain did not lie down. They are still obeying their last order. |
| 6 | night | 灰中醒来 / Woken in Ash | 你在丧钟堡外的焚尸坑里醒来，身上是灰，身下也是灰。掌心烙着一个印记：五焰环绕，中央一空。坑沿之上，一口钟正在敲。 | You wake in the burn pit outside Knellwatch, ash above you and ash below. On your palm is a brand: five flames around an empty heart. Above the lip of the pit, a bell is tolling. |

### Chapter card (`story.chapter.emerald_plains.*`, mood night)

| Field | zh-CN | en |
|---|---|---|
| number | 第一章 | Chapter One |
| title | 灰烬中醒来 | Woken from Ash |
| subtitle | 赤幡原 | The Red Field |
| text | 亡者不眠。败军的最后一座堡垒里，每到黄昏，钟为每一个新的名字敲一下。 | The dead do not sleep. In a beaten army's last fort, every dusk, the bell tolls once for every new name. |

### Cutscenes

Kinds: `say` (speaker), `whisper`, `narrate`, `focus` (target), `title`, `shake`, `flash`, `wait`. Every cutscene has
5-14 steps; every speaker and focus target stands in the zone (`quest_elder`, `knell_chaplain`, `plains_herbalist`,
`valdras` are there; the mini-boss is not a map spawn, so `cs_rf_herd` narrates her and focuses a tile).

**`cs_rf_brand`** — trigger: `q_rf_burial_detail` turned in.

| # | Step | zh-CN | en |
|---|---|---|---|
| 1 | focus npc `quest_elder` | | |
| 2 | say `quest_elder` | 十个人出去收尸。你替他们合上了眼。 | Ten went out for the dead. You closed their eyes. |
| 3 | say `quest_elder` | 敲钟。十下。一下一个名字。 | Ring the bell. Ten times. One for each name. |
| 4 | shake (500 ms, low) | | |
| 5 | narrate | 钟声沉沉地滚过赤幡原。远处，有什么东西停下来，侧耳倾听。 | The toll rolls slow across the Red Field. Far off, something stops and listens. |
| 6 | focus npc `knell_chaplain` | | |
| 7 | say `knell_chaplain` | 把手给我……这个烙印，我在古碑的拓本上见过。 | Give me your hand... I have seen this brand, on a rubbing of the old tablet. |
| 8 | say `knell_chaplain` | “渊火再燃之夜，灰烬中将有人醒来。其身负五焰，其心藏一空。” | "On the night the Abyssfire burns again, one shall wake from the ashes, bearing five flames, an emptiness at the heart." |
| 9 | say `knell_chaplain` | “开门者是他，闭门者亦是他。”……你是从焚尸坑里爬出来的，孩子。 | "He who opens the gate shall close it." ...You climbed out of the burn pit, child. |
| 10 | say hero | 那个坑里，烧的是谁？ | Who was burned in that pit? |
| 11 | say `quest_elder` | 所有人。 | Everyone. |

Aldwin calls the hero 孩子 (child), the whisper's word: the first seed of his fall.

**`cs_rf_whisper`** — trigger: `q_rf_ashwheat` turned in.

| # | Step | zh-CN | en |
|---|---|---|---|
| 1 | flash | | |
| 2 | narrate | 你梦见了最后一战。赤幡原上，三千面旗帜烧成了一片。 | You dream of the last battle. On the Red Field three thousand banners burn as one. |
| 3 | focus tile (15,95) | | |
| 4 | say `valdras` | 一步不许退。退者，以叛论处，就地正法。 | Not one step back. Whoever retreats is a traitor, executed where he stands. |
| 5 | narrate | 逃兵被押上古老的石台。刀落下的时候，石头裂开了。 | The deserters are dragged onto the ancient dais. When the blades fall, the stone splits. |
| 6 | shake | | |
| 7 | whisper | 他们用血，叩响了我的门。 | They knocked on my door with blood. |
| 8 | whisper | 而你，孩子——你会替我点灯。 | And you, child, will light my lamps. |
| 9 | say hero | 你是谁？ | Who are you? |
| 10 | whisper | 一个和你一样，被埋掉的人。 | Someone who was buried. Like you. |
| 11 | narrate | 你在丧钟堡的火塘边惊醒，掌心滚烫。没有风，钟却在轻轻摇晃。 | You wake by Knellwatch's hearth, your palm burning. There is no wind, yet the bell sways. |

**`cs_rf_herd`** (new) — trigger: `monster_killed` `miniboss_corpseherd`.

| # | Step | zh-CN | en |
|---|---|---|---|
| 1 | focus tile (60,55) | | |
| 2 | narrate | 尸牧婆倒在车圈中央，手还攥着铃铛。 | The Corpseherd falls in the middle of the wagon ring, her hand still on the bell. |
| 3 | narrate | 她轻轻摇了一下：“回家吧，孩子们……天黑了。” | She rings it once, softly: "Go home, children... it's dark." |
| 4 | narrate | 围着她的死人，一个接一个，坐了下来。 | Around her, one by one, the dead sit down. |
| 5 | whisper | 她只是想领他们回家。我只是告诉了她，家在哪儿。 | She only wanted to lead them home. I only told her where home was. |

**`cs_boss_valdras`** (boss intro; `story.boss.valdras.name` / `.epithet`) — the boss scan at 9 tiles.

| # | Step | zh-CN | en |
|---|---|---|---|
| 1 | focus monster `valdras` | | |
| 2 | title | 焚旗元帅·瓦德拉斯 / 不倒之旗 | Lord-Marshal Valdras / The Banner That Will Not Fall |
| 3 | say `valdras` | 又一个逃兵。 | Another deserter. |
| 4 | say `valdras` | 我下过令：一步不许退。我还在执行。 | I gave the order: not one step back. I am still carrying it out. |
| 5 | whisper | 他守得很好，孩子。可惜，守的是我的门。 | He held well, child. Pity it was my door he held. |
| 6 | say hero | 仗打完了。 | The war is over. |

Under step 2 his `Roar` plays (the shipped-but-unused clip family, now driven by AnimAction, implementation-plan.md K1).

**`cs_rf_marshal_falls`** (new) — trigger: `monster_killed` `valdras`.

| # | Step | zh-CN | en |
|---|---|---|---|
| 1 | focus tile (15,95) | | |
| 2 | say `valdras` | ……是谁……下的撤退令？ | Who... gave the order... to retreat? |
| 3 | narrate | 他跪倒在石台上。背上那面烧焦的旗，终于垂了下来。 | He kneels on the dais. The scorched banner on his back finally droops. |
| 4 | whisper | 现在，孩子——灯。 | Now, child. The lamp. |
| 5 | wait 1200 ms | | |

**`cs_rf_siege`** (new) — trigger: `quest_accepted` `q_rf_last_toll`.

| # | Step | zh-CN | en |
|---|---|---|---|
| 1 | shake | | |
| 2 | narrate | 钟无人自鸣。 | The bell rings with no hand on it. |
| 3 | say `quest_elder` | 上墙！能拿刀的都上墙！ | To the walls! Everyone who can hold a blade! |
| 4 | focus tile (10,32) | | |
| 5 | whisper | 听，孩子。他们在回家。 | Listen, child. They're coming home. |

**`cs_rf_finale`** — trigger: `q_rf_last_toll` turned in; `chapterCompleteQuest`.

| # | Step | zh-CN | en |
|---|---|---|---|
| 1 | focus npc `quest_elder` | | |
| 2 | say `quest_elder` | 天亮了。它们……停下了。 | Dawn. They've... stopped. |
| 3 | narrate | 你把烙着印的手按在冰冷的钟上。 | You lay your branded hand on the cold bell. |
| 4 | flash | | |
| 5 | narrate | 钟声沿着灵脉沉下去，一直沉到石台。裂口里的黑火，第一次退了。 | The toll sinks down the ley line, all the way to the dais. For the first time, the black fire in the breach draws back. |
| 6 | title | 第一焰 / 灵脉之印，重燃 | The First Flame / The Seal of Veins, relit |
| 7 | narrate | 赤幡原上，那些还站着的死人，一个接一个，躺了下去。 | Across the Red Field the dead who still stood lie down, one by one. |
| 8 | whisper | 他们为每一个名字敲钟。从来没有人，为我敲过。 | They toll a bell for every name. No one ever tolled one for me. |
| 9 | whisper | 第一盏灯。很好，孩子。一直走下去。 | The first lamp. Good, child. Keep walking. |
| 10 | say `quest_elder` | 东边的林子里，还有活人在等消息。 | There are living people past the eastern wood, waiting for word. |
| 11 | focus npc `plains_herbalist` | | |
| 12 | say `plains_herbalist` | 我去那座塔。那里的火，不要人偿。伤员要药圃，药圃得有人守。 | I'm going to that tower. Its fire asks no one to pay. The wounded need a herb garden, and a garden needs a keeper. |
| 13 | say `knell_chaplain` | 愿钟声护你。……我会一直听着它。 | May the bell keep you. ...I will always be listening to it. |

Epilogue hook: step 13 seeds Chapter 2 (Aldwin follows the voice east). The east road stays sealed in milestone 1 (W7).

**`cs_rf_pit`** [W3] — trigger: `zone_entered` `sub_rf_pit`.

| # | Step | zh-CN | en |
|---|---|---|---|
| 1 | narrate | 坑底还有一层。越往下，越暖。暖得不对。 | There is another floor under the pit. The deeper you go, the warmer it gets. Wrongly warm. |
| 2 | focus player | | |
| 3 | narrate | 墙上嵌着一排排头盔，每一顶都朝着上面，像在等人点名。 | Rows of helmets are set into the walls, every one facing up, as if waiting for roll call. |
| 4 | whisper | 你在这里醒过一次。再醒一次，也不难。 | You woke here once. Waking again won't be hard. |
| 5 | wait 800 ms | | |

### Triggers

| On | Subject | Cutscene |
|---|---|---|
| quest_turned_in | `q_rf_burial_detail` | `cs_rf_brand` |
| quest_turned_in | `q_rf_ashwheat` | `cs_rf_whisper` |
| quest_accepted | `q_rf_last_toll` | `cs_rf_siege` |
| quest_turned_in | `q_rf_last_toll` | `cs_rf_finale` |
| monster_killed | `miniboss_corpseherd` | `cs_rf_herd` |
| monster_killed | `valdras` | `cs_rf_marshal_falls` |
| zone_entered | `sub_rf_pit` [W3] | `cs_rf_pit` |

### Saved world state

| Flag | Set by | Effect | Wave |
|---|---|---|---|
| `rf_hungerbell_silenced` | M2 turned in | the distant night ringing over the Gnawed Field stops | W2 (W1: the loop is tied to the hunt being alive) |
| `rf_chapter_dawn` | M5 turned in | ashfall stops; the grade warms (`march_dawn` mood); the trench and Dead Ranks Unburied spawns are replaced by Gravegnawers ("the dead lie down; the scavengers remain"); Valdras stays farmable once per visit (M7) | W2 (W1: mood switch only, via `chapterCompleteQuest`) |
| `rf_pit_cleansed` | S7 cleared | the Pit's embers rise; the Pit stays lit; no respawn inside | W3 |

*K6 as built (2E, pending review):* quest `setsFlags` sets a flag at the turn-in (saved `worldFlags`, never cleared;
`EvWorldFlag`, `Snapshot::worldFlags` for UE's mood and audio); the Gravegnawer replacement is map spawn `variants
[{whenFlag: rf_chapter_dawn, monsterId: gravegnawer, count}]` on the trench and Dead Ranks entries, read at the zone's
spawn and at every respawn. `rf_pit_cleansed` (set by a clear, not a turn-in) is K7's (implementation-plan.md *2E
outcomes*).

## 9. Sub-areas

### 9.1 The Breach Rim (surface region, [W1])

A 16-tile region around (15,95): a region mood override (darker, violet uplight from the crack, rising embers), no
regular respawn while Valdras lives, music silent inside 16 tiles until the intro. Set pieces (UE layer, not generator
inputs): the cracked dais at the centre (the Seal of Veins), a chasm decal of the breach on the west side (visual only),
two ash ritual circles at (11,91) and (19,99), a ring of 12 scorched banners at radius 9, helmets on spears along the
Dead Ranks approach.

### 9.2 信者之坑 / The Pit of the Faithful (`sub_rf_pit`, [W3])

Hand-authored 40 × 40 map (tiles present, so the generator is skipped, as for `ember_tower`); entrance at (17,26), four
tiles below where the hero woke, outside the camp's safe radius. Lv 8-9. No respawn inside; the hero's light radius is
the main light; death returns the hero to Knellwatch.

| Room | zh | Where | Contents |
|---|---|---|---|
| The Ash Steps | 灰阶 | (5,5) entry | ash-choked stair, 4 `breachclot` |
| The Helm Wall | 盔墙 | (24,8) | helmets set in niches facing up; 6 `risen_footman` standing at attention, dormant until the hero is within 3 tiles |
| The Tag Cellar | 名牌窖 | (10,24) | thousands of tags on strings; 2 `risen_arbalest` + 4 `gravegnawer` come down to feed |
| The Pitwarden's Cut | 掘坑官之室 | (28,26) | `sub_pitwarden` (subDungeonMiniBoss) with 2 `risen_shieldbearer` |
| The Flame Niche | 火龛 | (20,35) | no monsters: an empty niche shaped like a flame, five burn marks round it; the S7 turn-in beat |

**Cleanse.** When the Pitwarden is dead and 5 or fewer hostiles remain, `clear_area` completes, `EvAreaCleansed` fires:
embers rise from the floor, the grade warms, the niche lamps light one by one. The Pit kit is reused for the Labyrinth's
crypt floor.

## 10. Mini-boss: 尸牧婆 / the Corpseherd (`miniboss_corpseherd`, the Sheepfold (60,55))

**Who she is.** A washerwoman from the crusade's baggage train whose two sons died on the field. Nobody at Knellwatch
knows her name, so the bell has never tolled for her. She keeps the dead walking in circles with a bell tied to a bone
crook, leading them "home". Human: the hooded shaman mesh with a human palette (monsters.md §2.4).

**Arena.** A ring (r6) of overturned crusade wagons with gaps north and south; six dead crusaders seated facing the
breach (pre-seeded corpses [W2]); ash pens.

**Pre-fight lines** (`data.miniBossDialogue.miniboss_corpseherd.line1..3`; hold her until dismissed):

| # | zh-CN | en |
|---|---|---|
| 1 | 嘘……别惊了我的羊群。 | Hush... don't startle my flock. |
| 2 | 我的两个儿子也在里面。我领着他们走，一圈，又一圈。 | My two boys are in there. I lead them round, and round. |
| 3 | 天快黑了，他们得回家。你挡着路了。 | It's nearly dark. They have to get home. You're in the way. |

**Fight.** L6, 300 HP, ranged (range 3.0, keeps decision M5's role), elite with one affix, once per visit.

| Mechanic | Spec | Wave |
|---|---|---|
| Bell-toll bolt | a breach-violet ripple projectile every 1300 ms; on hit slow 20 % for 2 s | W1 |
| 召羊 Call the Flock | every 8 s: Cast01 (her bell), a breach-violet marker ring under one corpse within 6 tiles for 900 ms, then it stands as a Raised `risen_footman` at 40 % HP that gives no exp or loot and never respawns. At most 3 Raised alive, at most one raise per 8 s; she raises two at once when the fight starts so the lesson is immediate | W2 |
| Counterplay | kill her first; a fire killing blow or a fire skill over a corpse burns it to ash (unraisable); on her death every Raised collapses | W2 |
| Kite | holds 3-5 tiles from the hero | W2 |
| Keen (stretch) | below 50 %: a crimson ring telegraph r4, 800 ms, slow 25 % for 3 s | W2 |

W1 fallback: a plain ranged elite with the slowing bolt in the dressed arena.

## 11. Boss: 焚旗元帅·瓦德拉斯 / Lord-Marshal Valdras (`valdras`, storyBoss, the Breach Rim)

**Look.** A head and a half taller than the hero. Blackened plate with pale breach-violet light leaking from every seam
(the seized fire inside him); a great helm with a burned plume and a single glowing visor slit; never a face. The
crusade's scorched standard has grown through his spine and rises above his head: the silhouette is "the banner that
will not fall". A two-handed oath-sword whose grip has melted into his gauntlet. No crown, throne, crypt or skull.

**Numbers** (tuning targets; final values set by the playthrough gate): L9, ~1100 HP, damage 20, defence 12, speed 48,
aggro 8, range 2.0, attack every 1300 ms (a two-hit combo), scale ×1.4. Rules: no invulnerable phase; on Normal no single
hit exceeds 35 % of a level-9 hero's maximum life; every special winds up ≥ 700 ms with a crimson ground decal; he never
leaves the 9-tile banner ring (leash to the dais).

| Phase | HP | Behaviour |
|---|---|---|
| **P1 死守 / Hold the Line** | 100-65 % | Two-hit combo. **旗扫 Banner Sweep**: cone 120°, r2.6, windup 700 ms, ×1.3, cooldown 5 s (Attack02). **誓印 Oath-Brand Slam**: circle r1.8 at the hero's position when the windup starts, windup 900 ms, ×1.8, leaves a burning brand decal for 4 s (fire ticks ×0.15 every 500 ms), cooldown 8 s (Cast01). |
| **P2 点卯 / Roll Call** | 65-30 % | Enters with `Roar` (1.2 s; still takes damage). He holds the dais: move ×0.6, leash r3. A `dead_standard_bearer` rises at one ash circle; while it lives, both circles raise a `risen_footman` every 10 s (cap 4 alive; Raised: no exp, no loot). Killing the bearer stops the muster; one new bearer rises 20 s later, once. **Feed**: a Raised that dies within 4 tiles of the dais is drawn into his standard and heals him 3 % (corpse consumed); kill adds away from the dais or burn their corpses. **渗裂 Breach-seep**: every 6 s three crack lines (length 8, width 1) pulse from the dais after a 700 ms warning: 2 fire ticks of ×0.5 and slow 30 % for 2 s. Banner Sweep and Oath-Brand continue. |
| **P3 不许退 / No Retreat** | < 30 % | `Roar`: the standard on his back ignites with breach fire. Attack speed ×1.25, move ×1.15, leaves the dais. Adds **冲锋 Lunge**: line 5 × 1.2, windup 800 ms, ×1.5, cooldown 7 s. The banner ring ignites and closes from radius 9 to 5 over 40 s; outside it, 4 % max life per second of breach fire (soft enrage). |
| Death | 0 | He kneels on the dais, sword point-down; the standard droops → `cs_rf_marshal_falls`. Loot: the existing story-boss floor. |

**The lesson chain.** The Corpseherd teaches "kill the raiser"; the standard-bearer (regular, in the Dead Ranks and the
siege) teaches "kill the banner"; Valdras P2 tests both, and Feed punishes fighting adds on the dais.

**W1 fallback.** Single phase, ~900 HP: the two-hit combo with an authored long contact (≈ 650 ms) so the clip-derived
wind-up tint is the telegraph; Roar under the intro title.

## 12. Lore, echoes, hidden area, story decorations

| Id | Kind | Tile | Name | zh-CN | en |
|---|---|---|---|---|---|
| `lore_rf_01` | order post (falls back to the tablet look) | (45,35) | 元帅的最后军令 / The Marshal's Last Order | 全军死守赤幡原。退一步者，以叛论处，就地正法。——瓦德拉斯 | The whole army holds the Red Field. Whoever takes one step back is a traitor, executed where he stands. — Valdras |
| `lore_rf_02` | torn journal, hidden | (105,15) | 逃兵的日记 / A Deserter's Journal | 开战前三天，元帅让我们挖坑。很多坑。我问给谁挖。军士说：给不肯死守的人。 | Three days before the battle the Marshal had us dig pits. Many. I asked who for. The sergeant said: for whoever won't hold. |
| `lore_rf_03` | crystal shard, hidden | (15,108) | 封印碎片 / Shard of the Seal | 一块温热的石片，像刚被人握过。纹路和你掌心的烙印一模一样。 | A shard of stone, warm as if someone had just held it. Its lines match the brand on your palm. |
| `lore_rf_04` | carved stone | (85,80) | 教会问答石 / The Catechism Stone | 问：圣火为何必须守？答：因世界会冷，人会被忘。 | Q: Why must the Flame be kept? A: Because the world grows cold, and people are forgotten. |
| `lore_rf_echo_01` | echo (new kind) | (29,15) | 第七个人 / The Seventh Man | 一个提灯的影子跪在壕沟边，灯早灭了：“告诉瑟琳……灯是我弄丢的。” | A shade with a lantern kneels by the trench; the lantern is long out: "Tell Serin... I'm the one who lost the lantern." |
| `lore_rf_echo_02` | echo | (38,47) | 桥那边 / The Far Bank | 烧断的桥头坐着一家人的影子：“桥着火了。我们就在这边等……等她回来。” | A family's shades sit at the burned bridge: "The bridge is on fire. We'll wait on this side... until she comes back." |
| `lore_rf_echo_03` | echo | (20,92) | 石台边 / By the Dais | 一个跪着的影子，双手还被反绑：“我只是想回家种地。” | A kneeling shade, hands still bound behind: "I only wanted to go home and farm." |

The catechism quotes Ignaroth's own motive; the echo lore spots are new map landmarks (margin 2), part of the one
re-baseline (§17). Echo tiles are provisional until that re-baseline confirms them walkable.

**Hidden area** `hidden_rf_paymaster` 军饷车 / The Paymaster's Wagon (108,108) r6: a rare chest and a 200-gold pile
(unchanged rewards). Discovery: 「车夫还被铁链锁在钱箱上。没有人来救他，也没有人来拿钱。」 (The driver is still chained to
the strongbox. No one came to save him, or for the money.)

**Story decorations** (positions and `spriteType` kept; meshes re-pointed by the presentation theme):

| Id | Tile | spriteType | Name | Description zh-CN / en |
|---|---|---|---|---|
| `story_rf_belfry` | (55,45) | ruins | 灰麦村钟楼 / Ashwheat's Fallen Belfry | 灰麦村的钟裂成两半，陷在泥里。钟锤不见了。 / Ashwheat's bell lies split in the mud. Its clapper is gone. |
| `story_rf_red_banner` | (40,70) | war_banner | 赤幡 / The Red Banner | 一面红旗插在盾牌堆成的土丘上。每一面盾，都朝着丧钟堡。 / A red banner planted in a mound of shields. Every shield faces Knellwatch. |
| `story_rf_penitents_altar` | (88,55) | broken_altar | 赎罪坛 / The Penitents' Altar | 石盆里的沟槽一路斜向裂口。槽里积着灰，和干了的黑。 / The basin's groove runs downhill toward the breach. Ash has settled in it, and something dried black. |

## 13. Random events

Same five kinds, weights, cooldown and frequency (ambush 30, treasure 25, merchant 15, rescue 20, puzzle 10; W6).

| Kind | Remake | Content |
|---|---|---|
| ambush | 泥里伸出的手 / Hands from the Mud | 3-5 of [`risen_footman`, `gravegnawer`]. A distant toll plays 1.5 s before the hands rise (presentation). [W2] 10 % chance of a champion **Muster**: `dead_standard_bearer` + 3 `risen_footman` sharing one affix; the standard-bearer drops one guaranteed magic item (K6 `ambushChampion.leaderLoot`) |
| treasure_cache | 阵亡骑士的铁箱 / A Dead Knight's Strongbox | as today |
| wandering_merchant | 圣骨贩子 / the Relic Peddler | as today (Mag's Gleaners' wagon in later chapters) |
| rescue | 被缚的朝圣者 / the Bound Pilgrim | tied to a stake by zealots and freed before any harm; guards from [`oath_zealot`]; 30 gold / 25 exp |
| environmental_puzzle | 五口小钟 / Five Handbells | 「五口小钟挂在断桩上。钟舌上刻着数，从一到五。」 Ring them in order (the rune-pillar logic); 50 gold / 30 exp |

## 14. Achievements, homestead, pets

| Item | Remake |
|---|---|
| `ach_kill_breachclot` (was `ach_kill_slime`) | 止血 / Stanch the Seep: kill 50 `breachclot`; reward kept (lck +2) |
| `ach_kill_valdras` (was `ach_kill_goblin_chief`) | 撤退令 / The Order to Retreat: kill `valdras` |
| Homestead | `towerUnlockQuest` = `q_rf_ashwheat`; `herb_garden.unlockQuest` = `q_rf_last_toll`; embers rules unchanged |
| Pet | `pet_sprite` → 灯魂 / the Lanternwisp (S5); the only Chapter 1 companion (Q3) |

## 15. Zone mood

The art does most of the darkening (desaturated palettes, lower terrain value, emissive accents); the grade only
nudges (P14 applies 55 % of the mood). Values below go into `GameData/world.ts` overrides → `zone_moods.json`.

| Setting | `march` (Chapter 1) | `march_dawn` (after `rf_chapter_dawn`) | Breach Rim region |
|---|---|---|---|
| ambient / α | #A8AEBB / .30 | #C8B8A0 / .26 | #6A4A8A / .35 |
| vignette / α | #08070A / .58 | #120E0A / .45 | #0A0610 / .62 |
| haze / α | #5E5A57 / .12 | #7A7064 / .08 | #2A1A36 / .14 |
| saturation / contrast | .68 / 1.12 | .78 / 1.08 | .60 / 1.15 |
| lift / gain | [-.010, -.006, .004] / [0, .004, .020] | [.004, .002, -.004] / [.020, .012, 0] | [-.012, -.014, .010] / [.010, 0, .030] |
| fog colour / max / density | #4E525C / .40 / 1.15 | #6A6660 / .30 / .95 | #3A2A48 / .45 / 1.25 |
| weather / ambience | none / `ashfall` (falling ash; ember motes near the breach), tint #CFCAC0 | none / `dust_motes` | none / `sparks` (violet) |
| light scale (R18 override) | 0.32 | 0.22 | 0.40 |

**Reserved light.** Warm light (#FFB060) only at the hearth, Hald's forge, the bell-frame lantern and Crowmarket's
cookfire; the hero halo (#FFE2C0) is the hero's own light. The breach glows breach-violet (#B02FD0) from below.
Enemy telegraphs are crimson with a bone-white fill (DR5).

**Terrain and decor.** The map generator theme stays `plains` (a generator input); the **presentation theme** becomes
`march` for `emerald_plains` only (`ember_tower` keeps the warm plains look). Decor game ids and footprints are unchanged
and resolve to march meshes first:

| Game id | Plains mesh (today) | March mesh |
|---|---|---|
| `tree` | oak | dead oak, bark split by fire |
| `tree_round` | fruit tree | charred orchard tree, a few black apples |
| `bush` | berry bush | briar with rags caught on it |
| `grass` | grass tuft | ash-grey dead grass |
| `flower` | flower patch | ash and dull poppies |
| `mushroom_red` | red mushrooms | pale ash-cap fungus |
| `rock` | rock cluster | broken stone with tool marks |
| `boulder` | moss boulder | ash-crusted boulder |

Terrain style `march`: trampled mud, ash-grey dead grass, scorched patches, wheel ruts, still black water with an oily
sheen. Camp theme `march`: charred timber, pew palisade, Knellwatch banner (black field, bell), braziers.

## 16. Audio

| Where | Cue |
|---|---|
| Knellwatch | a sparse, slightly detuned plucked lute of our own (never a 12-string town theme), hearth crackle, the bell at dusk and at the end of every main turn-in (single toll) |
| Wilderness | low drones, wind, crows, distant bells, long silences; the Hungerbell's ringing over the Gnawed Field at night until M2 |
| Ashwheat | knocking behind barred doors; the cracked note of the split bell |
| The Red Field (south-west) | a slow dead drum and a wordless choir |
| The Breach Rim | silence inside 16 tiles while Valdras lives (wind and a low breach hum only); boss music from the intro; stingers on phase change |
| Siege | the bell ringing by itself (self-ring variant), drums from the field |
| Finale | the dawn toll, then silence, then the lute |
| Stingers | hunts, mini-boss, boss: choir and percussion |
| Ambush tell | a distant toll 1.5 s before the hands rise |

## 17. Map and spawn changes

**Map-generator inputs kept** (zero churn): 10 spawn positions, both camps, playerStart (15,22), the exit (119,60),
field-NPC tiles (50,30) (70,65), the hidden area (108,108) r6, the three story decorations (positions and `spriteType`),
the four lore positions, the mini-boss spawn (60,55), and for the ten kept quest slots (in their `AllQuests` order) every
questArea, objective location, gather area and count, clue and escort start / destination listed in §5-6.

**The one re-baseline** (inside the content switch, implementation-plan.md C1):

| Change | Landmarks (avoid list) |
|---|---|
| M5: explore (65,30) and (50,55) replaced by `defendTarget` (10,32) | −2 (margin 3), +1 (margin 6) |
| Pit entrance (17,26) added to `subDungeonEntrances` (inert, sealed until W3) | generator input |
| Echo lore `lore_rf_echo_01..03` at (29,15) (38,47) (20,92) | +3 (margin 2) |

Expected re-pins: `world_test` avoid-list size (28 → 30), RNG trace checkpoints, A* golden routes, the exporter's
`SPEC_GOLDEN` hashes, the gather-spot hash (`q_rf_barrowmoss:0`) and AutoPilot coordinates.

**Spawn table** (55 monsters; `monsterId` and `count` are not generator inputs):

| Anchor | Area | W1 entry (one id) | W2 mix (same count) |
|---|---|---|---|
| (25,20) | Burial Trenches | `risen_footman` ×8 | unchanged |
| (35,40) | Gnawed Field | `gravegnawer` ×8 | unchanged |
| (20,45) | The Seep | `breachclot` ×7 | unchanged |
| (85,18) | Weeping Ditch | `cinderbloat` ×6 | `cinderbloat` ×3 + `breachclot` ×3 |
| (95,35) | Penitents' Camp | `oath_zealot` ×5 | unchanged |
| (75,50) | Broken Redoubt | `risen_arbalest` ×5 | `risen_arbalest` ×3 + `risen_shieldbearer` ×2 |
| (30,80) | Dead Ranks | `risen_shieldbearer` ×6 | `risen_shieldbearer` ×3 + `risen_footman` ×2 + `dead_standard_bearer` ×1 (leader) |
| (15,95) | Breach Rim | `valdras` ×1 (storyBoss) | unchanged |
| (80,75) | Deserters' Hollow | `deserter` ×6 | `deserter` ×4 (one elite leader) + `deserter_drummer` ×1 + `deserter` ×1 |
| (95,90) | Crowmarket midden | `bonegorger` ×3 | `bonegorger` ×1 + `gravegnawer` ×2 |

Mini-boss `miniboss_corpseherd` at (60,55); hunts at (45,35), (60,48), (82,72). Respawn 15 s for regulars outside the
Breach Rim; the Rim's regulars do not respawn while Valdras lives. `emerald_plains` keeps exactly one elite affix.

## 18. i18n keys (zh-CN and en, parity asserted by the exporter)

| Namespace | Keys |
|---|---|
| `data.zone.emerald_plains` | 赤幡原 / The Red Field |
| `data.quest.<id>.name / .desc / .offer / .progress / .complete` | the 11 (12 in W3) quests above |
| `data.questTarget.<id>` | `risen_footman`, `gravegnawer`, `hunt_hungerbell`, `zone_rf_ashwheat`, `miniboss_corpseherd`, `valdras`, `mat_breach_ichor`, `mat_barrowmoss`, `mat_tag_pouch`, `hunt_dreck`, `hunt_harke`, `sub_pitwarden`, `sub_rf_pit` |
| `data.questClue.<id>` | `clue_rf_tag_cord/cloth/ashes`, `clue_rf_lantern_1..4` |
| `data.questChoice.<questId>.<slot>` (new) | the S3 and S4 labels |
| `data.monster.<id>` | every monster and hunt in monsters.md §2 |
| `data.npc.<id>.name / .dialogue.<n>` | §3, including the quest-gated lines |
| `data.dialogue.quest_elder.<node>.text / .choice.<i>` | §4 |
| `data.miniBossDialogue.miniboss_corpseherd.line1..3` | §10 |
| `data.lore.<id>.name / .text` | §12 |
| `data.hiddenArea.hidden_rf_paymaster.name / .discovery`, `data.storyDeco.<id>.name / .desc` | §12 |
| `data.escortNpc.rf_hallowed_ground`, `data.defendTarget.rf_last_toll` | 掘墓人伯恩 / Old Bern; 埋骨门 / the Boneyard Gate |
| `data.randomEvent.<type>.*`, `data.achievement.<id>.*`, `data.affix.<id>` | §13, §14, monsters.md §3 |
| `story.prologue.<n>.*`, `story.chapter.emerald_plains.*`, `story.<cutsceneId>.<step>`, `story.boss.valdras.name / .epithet` | §8 |

## 19. Amendments as built (1B review, 2026-10, pending review)

The data follows this document except where listed here (DECISIONS DR3 / DR5 / DR9 / M7 record the same changes).

| Where | As built | Why |
|---|---|---|
| §5 M2 Hunt | `hunt_hungerbell` hp ×2, dmg ×1.0 (420 HP, 17 damage per 1.7 s; exp stays 3 × base) | M2 is the Lv 3 quest; ×3 / ×1.3 made it the hardest elite before Valdras for a Lv 4-5 hero |
| §5-6 exp, §11 Valdras | every regular gives round(0.75 × monsters.md §2.0 Exp); Valdras 110 (was 220); quest rewards unchanged | DR9's band (Lv 12-14 at the end of the chapter): with §2.0's exp a full run ended at Lv 14-15 |
| §6 S1 complete | the Captain's line 「玛勒说，血里有声音。……别问我是谁的。」 / "Marl says the blood has voices in it. ...Don't ask me whose." | the Captain turns S1 in; Marl's line 「血里有声音……」 waits for K6 as `data.npc.plains_herbalist.after.q_rf_black_blood` |
| §6 S5 complete | 「是佩尔的灯火。收尸队里最小的那个，灯是他提的。它愿意跟你，就带它走吧。」 / "That's Pell's light. The youngest of the burial detail; he carried the lantern. ..." | the detail's one lantern lies dark in the trench (M1, `lore_rf_echo_01`); what walks the Weeping Ditch is its light (story-bible §8-9) |
| §12 `lore_rf_echo_01` | 「……灯已经灭了」 / "the lantern has gone out" (was 早灭了 / long out) | M1: it stopped moving an hour ago |
| §12 `story_rf_belfry` | en "Ashwheat's Broken Belfry" | "Fallen" is on story-bible §12's never-use list |
| §7 Embers | data now matches the 23: M5 `embers: 7`, S5 `embers: 3` (`embers` replaces the default) | the data had 5 / 2 (total 20) |
| §13 Events | per-zone lines (`sys.event.msg.<type>.emerald_plains`, `zone.event.merchant.announce.emerald_plains` with the Relic Peddler's line, `zone.event.rescue.complete.emerald_plains` with the Bound Pilgrim's); the pilgrim's guards are `oath_zealot` (`rescueMonsters`) | the core logs a zone's own variant when it exists; the handbell prop and the pilgrim's look still use the plains art ids until the art lane ships them |
| §3 / §2 lines | Old Bern's line `data.escortNpc.rf_hallowed_ground.line` and the hub barks `data.zoneBark.emerald_plains.0/1` are authored and parked for K6 / U4 (no Wave 1 reader) | |
| monsters.md §3 | elite affixes 焦灼 Scorched (fire_enhanced), 蛮力 Brutish (extra_strong), 哀咒 Dirgecursed (curse_aura) | 焚誓 / 赤誓 / 丧钟 are story terms (the Burned Oath, the Red-Oath, 丧钟堡) the elite label put before monster names |
| §14 | every achievement re-voiced (e.g. 第一个名字 / The First Name, 十件差事 / Ten Errands; `ach_kill_demon_lord` neutral as 火的尽头 / The Fire's End until Chapter 5's pass) | the panel lists them all |
| §8 triggers | `cs_rf_herd` / `cs_rf_marshal_falls` fire only while M3 / M4 is open (`whileQuest`); a Corpseherd or Valdras killed before its quest comes back on the accept (M7) | the climax beat plays on the kill that counts |
| §8 epilogue slide 2 | 赤幡原 / The Red Field, re-voiced (the web's Emerald Plains slide) | the ending is Chapter 5's, but this slide is Chapter 1's land |
| Jukebox | 赤幡原 · 探索 / 战斗, and 渊火 · 低语 / Abyssfire · The Whisper (the prologue's abyss slide) for the abyss_rift recording the prologue plays; the title-menu track keeps 渊火 · 序章 / Abyssfire · Prologue | the web plains names showed in the title menu; no two rows share a title |

Amendments as built in the Wave 2 content (2F, 2026-10, pending review; `implementation-plan.md` "2F outcomes",
DECISIONS DR9 / M5 / DR8 *2F*):

| Where | As built | Why |
|---|---|---|
| §11 Numbers | Valdras 9000 HP, damage 20 (2F review: the design's damage; 2F had 14000 / 32; defence, speed, range, attack interval unchanged); every special winds up 700-900 ms | the gate sets the numbers: at ~1100 HP the bot warrior killed the three-phase boss in about 2 s. 14000 / 32 fitted the bot warrior alone (mage 136 s, rogue 229 s; a reaction-limited bot died in 66-86 % of mage / rogue runs); 9000 HP puts every class in DR9's per-class band (*2F review*), and the design's damage 20 keeps a Brutish Valdras (x1.35) from killing a late-reacting rogue in most of its fights (it did in 10 of 18 at 24) |
| §11 P2 Breach-seep | one crack line (8 x 1) from the dais toward the hero every 6 s, P2 only (700 ms, two fire ticks x0.5, slow 30 % 2 s); 2F review: no caster clip (the cracks are the tell; he is not rooted) and a ranged special (`rangeMax` 10): it comes between swings at a hero up to 10 tiles off while he is held on the dais | an ability has one shape and the schema no `count`; three abilities would fire as one. As 2F built it the seep fired only on a due swing, so a hero 5.4+ tiles from the dais took nothing in P2 |
| §11 P2 muster | the bearer that rises at the west ash circle (11, 91) is `rollcall_standard_bearer` (亡旗手: the standard-bearer's numbers, look and banner aura), whose `roll_call` summons a footman at each circle ((11, 91), (19, 99)) every 10 s, at most 4 alive | the zone's `dead_standard_bearer` raises from a corpse within 6 tiles (its Muster); the circles need a summon at fixed anchors |
| §10 Kite | the Corpseherd holds 2-3 tiles (range 3.0 kept, DECISIONS M5 *2F note*) | a kite band must end inside the attack range (K5) |
| §10 Call the Flock | two Raised at the pull (a separate raise with a long cooldown), then one per 8 s, 900 ms, at most 3 alive | as specified; the pull is its own ability |
| §5 M2 Hungerbell | its call (`supper_bell`, every 9 s, 800 ms: every gnawer within 10 tiles is provoked) is a hunt ability next to the bonegorger's slam, not part of every slam | one ability cannot both slam and call; hunts gained `abilities` for it (remake feature `quest.huntAbilities`) |
| §17 Dawn | `rf_chapter_dawn` turns the trenches into 8 gravegnawers and the Dead Ranks into 6 | the entries keep their counts |
| §3 lines | the blacksmith, the Captain and Marl say their after line once its quest is turned in, their Wave 1 line kept behind it (the panel shows it); Brother Aldwin's line 0 gives way to his blessing after M5, his line 1 stays | K6's slots: the log takes the first holding line, the panel the second |
| monsters.md §3 | the 不葬 Unburied affix is not rolled for a monster that can never rise (a boss, a Raised body, a body that leaves no corpse; 2F review core rule) | 2F let it roll on the Corpseherd and Valdras, where its label promised a rise that never came |

Amendments as built in the 2F review (2026-10, pending review; `implementation-plan.md` "2F review outcomes", DECISIONS
M1 / M7 / DR8 / DR9 *2F review*):

| Where | As built | Why |
|---|---|---|
| §10 Fight | the Corpseherd has 600 HP (was 300) | at the main path's Lv 10 she died in 0.4-2.3 s, before Keen; at 600 her opening raise shows in every fight and Keen in most (500 left the warrior's Keen at 55-80 %); the 8 s repeat raise is for a slower or earlier hero. She now outlasts the web's zone-2 mini-boss (520), which stays behind the sealed east gate (W7) until Chapter 2 is remade |
| §10 Keen | a status-only strike (`dmgMul 0`: no damage, slow 25 % 3 s), every 12 s below 50 % | the design's ring slows only (2F gave it x0.4 damage); the design names no cooldown |
| §11 Banner Sweep, Lunge | fire (`damageType`), like Oath-Brand and the seep | monsters.md §2.6 "specials fire": the resistance and the corpse burn (Feed's counterplay) follow |
| §11 P3 Lunge | 冲锋 a charge: at a hero 2.5-5 tiles off (a ranged special), the line 5 x 1.2 hits, then he stands at its end (`dash`: the farthest open point, never past his leash, out of a camp); 800 ms, x1.5, 7 s | 2F's Lunge was a swing pick that never closed distance |
| §11 leash | his leash is the dais (the authored tile) over his whole fight's reach: max(8, the ring's 9 + his reach 2.4) = 11.4 tiles (DECISIONS M1 *2F review*) | the M1 leash measured 8 tiles from his jittered spawn, inside the 9-tile ring: a hero obeying the ring could walk him home, healed, to P1 |
| §8 / §17 Dawn | the Broken Redoubt also turns to 5 gravegnawers at dawn; after dawn an ambush is the gnawers' (`ambushVariants`) and the champion Muster no longer comes (`notFlag`) | cs_rf_finale step 7: the dead who still stood lie down across the Red Field |
| §3 Marl | Mother Marl leaves the dressing station once M5 is turned in (the finale), unless S1 is still open (`visibleWhen`); S1's reward is named 玛勒的苦药 / Marl's Bitter Draught (`data.quest.q_rf_black_blood.permanent`) | §3: she goes to the tower in the finale; the reward's name had no string |
| §5 M2 Hungerbell, §11 seep | Hungerbell's call plays no clip (its ripple is the tell; it is not rooted); the seep plays none on Valdras | they reused the slam clips of a damaging special at the same beat |
| monsters.md §2.3 | the zealot's frenzy has no Roar enter action (it surges at once; the frenzy look rides on EvMonsterPhase) | a rooting 1.2 s Roar read as a stagger at the moment it should surge |
| monsters.md §2.3 | the drummer's +10 % attack speed is an aura while he lives (not only while he drums); its look follows 2G's no-glow rule for the Living | a buff tied to the call would end with each 6 s drum; kept as built, the look is 2G's |

Amendments as built in the UE presentation (1C review, 2026-10, pending review; `implementation-plan.md` "1C review
outcomes", DECISIONS DR15):

| Where | As built | Why |
|---|---|---|
| §2 Crowmarket | the march camp kit's generic pieces (well, supply crates, ration sacks, braziers, the hearth as its cookfire) but **no banners**: only Knellwatch (the map's first camp) flies the fort's bell standard (`AbyssThemeRules::PlacesCampProp`). The chapel-pew palisade of the march wall art stays. The Gleaners' salvage carts, boot lines, crows on the cart rails and a cold cookfire are not built yet (art lane A10; the U4 "Not placed" list) | `camp_banner@march` is Knellwatch's own emblem; the camp kit theme is zone-wide |
| §5 M5 / §16 Siege | the bell that rings by itself is **Knellwatch's bell frame**: `bell_self_ring` ripples it like every other toll of the fort's bell; the Boneyard Gate's bell (the art lane's "self-ringing siege bell") answers it as an echo, so at the gate the gate's bell moves too | story-bible §2: the keepers' bell is the one the fort was built around; the gate is where the player stands during the siege |
| §10 Sheepfold | six wagons on a 10-slot ring of r 5.8 about (60, 54.5): gaps north and south (one slot each) and **west** (two slots, where the wagon ruts come in from Ashwheat) | the bot's Chapter 1 route walks through both west slots (CoreTests `setpieces_test`: a wagon on either slot covers a route tile); set pieces are not walkability, so a wagon there would stand on the road |
| §9.1 `lore_rf_echo_03` | the kneeling figure's group is turned 135° about the dais (`echoTurnDeg`): it kneels by the north-east kerb, facing the seal, ~2.9 tiles from the lore spot (20,92) instead of 8.5 (the screen edge) | U1 / U4 handoff; **pending A10 review**: art review r1 placed it in the west gap with its back to the camera and rejected a kneeler on the kerb; the alternatives are moving the lore spot or an A10 re-pose on the lore's side. The contract's `poseOverride` (arms bound behind) is not applied in UE: the figure kneels with the escort rig's Death frame 22 until A9's `Kneel_Bound` clip ships |
| §1 east exit (119,60) | the march wagon barricade with **no** ground loop: in a themed zone the sealed exit's web loop (crimson ring + runes, palette `blood`) is not played (`AbyssThemeRules::ThemedOnlyRecipe`) until A12 ships `exit.sealed@march` (smoke / ash at the barricade, breach violet beyond) | DR5 keeps crimson ground decals for enemy telegraphs |
| §13 Rescue label | the bound pilgrim's ground label (被缚的朝圣者 / The Bound Pilgrim) shows in emerald_plains only; chapters 2-5 keep their unlabelled rescue prop | "other zones look exactly as today" |

Still open from the review: 「那里的火，不要人偿。」 (cs_rf_finale.12, story-bible §8) reads as dropping 偿's object; the story owner decides (「不用拿命来换」 or keep).
