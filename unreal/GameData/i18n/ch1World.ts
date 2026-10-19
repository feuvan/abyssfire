/**
 * Chapter 1 world strings of the dark remake (package 1B part 1): the zone, the roster, the mini-boss and its lines,
 * the hunts, the story boss's intro, lore and echoes, the hidden area, story decorations, the sealed Pit, the two new
 * NPCs, the Chapter 1 achievements and events, and the re-voiced elite affixes. Sources: Docs/dark-remake/chapter1.md
 * §1-3, §8, §10-14, §18 and monsters.md §2-3; the voice guide is story-bible.md §11.
 *
 * Each key maps to [zh-CN, en]; ./zh-CN.ts and ./en.ts take one column each, so the two locales cannot drift apart.
 * Quest, NPC-line, dialogue-tree and cutscene strings are the quest switch's (package 1B part 2): ./ch1Quests.ts,
 * ./ch1Npcs.ts and ./ch1Story.ts (the M5 gate's name moved there as `data.defendTarget.rf_last_toll`).
 */
export const CH1_WORLD_STRINGS: Readonly<Record<string, readonly [zhCN: string, en: string]>> = {
  // ── The zone ──
  'data.zone.emerald_plains': ['赤幡原', 'The Red Field'],

  // ── The roster (monsters.md §2.0) ──
  'data.monster.breachclot': ['渊血凝块', 'Breachclot'],
  'data.monster.risen_footman': ['复起步卒', 'Risen Footman'],
  'data.monster.gravegnawer': ['啃冢者', 'Gravegnawer'],
  'data.monster.cinderbloat': ['烬胀囊', 'Cinderbloat'],
  'data.monster.risen_arbalest': ['复起弩手', 'Risen Arbalester'],
  'data.monster.oath_zealot': ['赤誓狂信徒', 'Red-Oath Zealot'],
  'data.monster.deserter': ['逃兵', 'Deserter'],
  'data.monster.deserter_drummer': ['军鼓手', 'Deserter Drummer'],
  'data.monster.risen_shieldbearer': ['复起盾卒', 'Risen Shieldbearer'],
  'data.monster.dead_standard_bearer': ['亡旗手', 'Dead Standard-Bearer'],
  'data.monster.bonegorger': ['饕骸者', 'Bonegorger'],
  'data.monster.miniboss_corpseherd': ['尸牧婆', 'The Corpseherd'],
  'data.monster.valdras': ['焚旗元帅·瓦德拉斯', 'Lord-Marshal Valdras'],
  // Hunts (quest_hunts.json).
  'data.monster.hunt_hungerbell': ['饿钟', 'Hungerbell'],
  'data.monster.hunt_dreck': ['拾银者·德雷克', 'Dreck the Silverpicker'],
  'data.monster.hunt_harke': ['断誓者·哈克', 'Harke the Oathless'],
  // Quest objective targets that name them (getQuestTargetName).
  'data.questTarget.risen_footman': ['复起步卒', 'Risen Footman'],
  'data.questTarget.gravegnawer': ['啃冢者', 'Gravegnawer'],
  'data.questTarget.hunt_hungerbell': ['饿钟', 'Hungerbell'],
  'data.questTarget.valdras': ['焚旗元帅·瓦德拉斯', 'Lord-Marshal Valdras'],
  'data.questTarget.hunt_dreck': ['拾银者·德雷克', 'Dreck the Silverpicker'],
  'data.questTarget.hunt_harke': ['断誓者·哈克', 'Harke the Oathless'],
  'data.questTarget.defend_rf_boneyard_gate': ['守住埋骨门', 'Hold the Boneyard Gate'],

  // ── The Corpseherd's pre-fight lines (chapter1.md §10) ──
  'data.miniBossDialogue.miniboss_corpseherd.line1': ['嘘……别惊了我的羊群。', "Hush... don't startle my flock."],
  'data.miniBossDialogue.miniboss_corpseherd.line2': ['我的两个儿子也在里面。我领着他们走，一圈，又一圈。',
    'My two boys are in there. I lead them round, and round.'],
  'data.miniBossDialogue.miniboss_corpseherd.line3': ['天快黑了，他们得回家。你挡着路了。',
    "It's nearly dark. They have to get home. You're in the way."],

  // ── The story boss's intro (chapter1.md §8, cs_boss_valdras) ──
  'story.boss.valdras.name': ['焚旗元帅·瓦德拉斯', 'Lord-Marshal Valdras'],
  'story.boss.valdras.epithet': ['不倒之旗', 'The Banner That Will Not Fall'],
  'story.cs_boss_valdras.3': ['又一个逃兵。', 'Another deserter.'],
  'story.cs_boss_valdras.4': ['我下过令：一步不许退。我还在执行。', 'I gave the order: not one step back. I am still carrying it out.'],
  'story.cs_boss_valdras.5': ['他守得很好，孩子。可惜，守的是我的门。', 'He held well, child. Pity it was my door he held.'],
  'story.cs_boss_valdras.6': ['仗打完了。', 'The war is over.'],

  // ── Lore and echoes (chapter1.md §12) ──
  'data.lore.lore_rf_01.name': ['元帅的最后军令', "The Marshal's Last Order"],
  'data.lore.lore_rf_01.text': ['全军死守赤幡原。退一步者，以叛论处，就地正法。——瓦德拉斯',
    'The whole army holds the Red Field. Whoever takes one step back is a traitor, executed where he stands. — Valdras'],
  'data.lore.lore_rf_02.name': ['逃兵的日记', "A Deserter's Journal"],
  'data.lore.lore_rf_02.text': ['开战前三天，元帅让我们挖坑。很多坑。我问给谁挖。军士说：给不肯死守的人。',
    "Three days before the battle the Marshal had us dig pits. Many. I asked who for. The sergeant said: for whoever won't hold."],
  'data.lore.lore_rf_03.name': ['封印碎片', 'Shard of the Seal'],
  'data.lore.lore_rf_03.text': ['一块温热的石片，像刚被人握过。纹路和你掌心的烙印一模一样。',
    'A shard of stone, warm as if someone had just held it. Its lines match the brand on your palm.'],
  'data.lore.lore_rf_04.name': ['教会问答石', 'The Catechism Stone'],
  'data.lore.lore_rf_04.text': ['问：圣火为何必须守？答：因世界会冷，人会被忘。',
    'Q: Why must the Flame be kept? A: Because the world grows cold, and people are forgotten.'],
  'data.lore.lore_rf_echo_01.name': ['第七个人', 'The Seventh Man'],
  'data.lore.lore_rf_echo_01.text': ['一个提灯的影子跪在壕沟边，灯已经灭了：“告诉瑟琳……灯是我弄丢的。”',
    `A shade with a lantern kneels by the trench; the lantern has gone out: "Tell Serin... I'm the one who lost the lantern."`],
  'data.lore.lore_rf_echo_02.name': ['桥那边', 'The Far Bank'],
  'data.lore.lore_rf_echo_02.text': ['烧断的桥头坐着一家人的影子：“桥着火了。我们就在这边等……等她回来。”',
    `A family's shades sit at the burned bridge: "The bridge is on fire. We'll wait on this side... until she comes back."`],
  'data.lore.lore_rf_echo_03.name': ['石台边', 'By the Dais'],
  'data.lore.lore_rf_echo_03.text': ['一个跪着的影子，双手还被反绑：“我只是想回家种地。”',
    'A kneeling shade, hands still bound behind: "I only wanted to go home and farm."'],

  // ── The hidden area and the story decorations (chapter1.md §12) ──
  'data.hiddenArea.hidden_rf_paymaster.name': ['军饷车', "The Paymaster's Wagon"],
  'data.hiddenArea.hidden_rf_paymaster.discovery': ['车夫还被铁链锁在钱箱上。没有人来救他，也没有人来拿钱。',
    'The driver is still chained to the strongbox. No one came to save him, or for the money.'],
  'data.storyDeco.story_rf_belfry.name': ['灰麦村钟楼', "Ashwheat's Broken Belfry"],  // not 'Fallen' (story-bible §12)
  'data.storyDeco.story_rf_belfry.desc': ['灰麦村的钟裂成两半，陷在泥里。钟锤不见了。', "Ashwheat's bell lies split in the mud. Its clapper is gone."],
  'data.storyDeco.story_rf_red_banner.name': ['赤幡', 'The Red Banner'],
  'data.storyDeco.story_rf_red_banner.desc': ['一面红旗插在盾牌堆成的土丘上。每一面盾，都朝着丧钟堡。',
    'A red banner planted in a mound of shields. Every shield faces Knellwatch.'],
  'data.storyDeco.story_rf_penitents_altar.name': ['赎罪坛', "The Penitents' Altar"],
  'data.storyDeco.story_rf_penitents_altar.desc': ['石盆里的沟槽一路斜向裂口。槽里积着灰，和干了的黑。',
    "The basin's groove runs downhill toward the Breach. Ash has settled in it, and something dried black."],

  // ── The Pit of the Faithful, sealed until Wave 3 (chapter1.md S7 "Before W3") ──
  'data.subDungeonEntrance.entrance_rf_pit': ['信者之坑', 'The Pit of the Faithful'],
  'data.subDungeonSealed.entrance_rf_pit': ['坑口还在冒烟。等火熄了，再下去。', 'The pit mouth is still smoking. Wait for the fire to die.'],

  // ── New NPCs (chapter1.md §3); the kept role keys are re-voiced in ./ch1Npcs.ts ──
  'data.npc.knell_chaplain.name': ['奥德温修士', 'Brother Aldwin'],
  'data.npc.knell_chaplain.dialogue.0': ['钟声是给活人听的。死人……另有人在跟他们说话。',
    'The bell is for the living. The dead... someone else is talking to them.'],
  'data.npc.knell_chaplain.dialogue.1': ['我替每个名字念一遍经。念到第三百个，我开始怕自己忘了念给谁。',
    "I say a prayer for every name. By the three hundredth I began to fear I'd forget who it was for."],
  'data.npc.crow_trader.name': ['鸦嫂玛格', 'Mag Crowe'],
  'data.npc.crow_trader.dialogue.0': ['别问靴子是谁的。问价钱。', "Don't ask whose boots. Ask the price."],

  // ── Lines with no Wave 1 reader yet, parked for K6 / U4 (chapter1.md §2-3): Old Bern's line while he pushes his
  // barrow (the escort NPC's bark) and Knellwatch's two hub barks ──
  'data.escortNpc.rf_hallowed_ground.line': ['一车十二个，都有名字。我一个个念给土听。', 'Twelve to a cart, all named. I read each one to the earth.'],
  'data.zoneBark.emerald_plains.0': ['又少了三份口粮。', 'Three fewer rations again.'],
  'data.zoneBark.emerald_plains.1': ['别盯着坑看。', "Don't stare at the pit."],

  // ── The jukebox rows of the Chapter 1 soundtrack (the title menu; Source/Abyssfire AbyssAudioManifest). The abyss_rift
  // explore recording plays under the prologue (StoryDirector's sequence music); the title-menu track is already
  // 渊火 · 序章 / Abyssfire · Prologue (menu.jukebox.track.menu), so it takes the name of the prologue's abyss slide that
  // speaks of Ignaroth, 低语 / The Whisper (chapter1.md §8). Every row reads differently (content_ch1_test) ──
  'menu.jukebox.track.emerald_plains.explore': ['赤幡原 · 探索', 'The Red Field · Explore'],
  'menu.jukebox.track.emerald_plains.combat': ['赤幡原 · 战斗', 'The Red Field · Combat'],
  'menu.jukebox.track.abyss_rift.explore': ['渊火 · 低语', 'Abyssfire · The Whisper'],

  // ── Achievements (chapter1.md §14) ──
  'data.achievement.ach_kill_breachclot.name': ['止血', 'Stanch the Seep'],
  'data.achievement.ach_kill_breachclot.desc': ['击杀50个渊血凝块', 'Kill 50 Breachclots'],
  'data.achievement.ach_kill_valdras.name': ['撤退令', 'The Order to Retreat'],
  'data.achievement.ach_kill_valdras.desc': ['击败焚旗元帅·瓦德拉斯', 'Defeat Lord-Marshal Valdras'],
  'data.achievement.ach_kill_valdras.title': ['撤退令', 'The Order to Retreat'],
  // Every achievement is listed (locked ones too), so the web names re-voice in the remake's burial-and-bell words
  // (1B review; story-bible.md §11-12: no 达人, no Diablo class names). ach_kill_demon_lord waits for Chapter 5's pass
  // with a neutral name (DR11).
  'data.achievement.ach_first_kill.name': ['第一个名字', 'The First Name'],
  'data.achievement.ach_first_kill.desc': ['放倒第一个敌人', 'Put down your first foe'],
  'data.achievement.ach_first_kill.title': ['灰中人', 'From the Ash'],
  'data.achievement.ach_kill_100.name': ['一百下钟声', 'A Hundred Tolls'],
  'data.achievement.ach_kill_100.desc': ['放倒100个敌人', 'Put down 100 foes'],
  'data.achievement.ach_kill_500.name': ['掘墓人', 'Gravedigger'],
  'data.achievement.ach_kill_500.desc': ['放倒500个敌人', 'Put down 500 foes'],
  'data.achievement.ach_kill_500.title': ['掘墓人', 'Gravedigger'],
  'data.achievement.ach_kill_demon_lord.name': ['火的尽头', "The Fire's End"],
  'data.achievement.ach_kill_demon_lord.desc': ['在深渊之底，结束这一切', 'End it at the bottom of the Abyss'],
  'data.achievement.ach_kill_demon_lord.title': ['守火人', 'Flamekeeper'],
  'data.achievement.ach_level_10.name': ['还站着', 'Still Standing'],
  'data.achievement.ach_level_10.desc': ['达到10级', 'Reach level 10'],
  'data.achievement.ach_level_25.name': ['老兵', 'Veteran'],
  'data.achievement.ach_level_25.desc': ['达到25级', 'Reach level 25'],
  'data.achievement.ach_level_25.title': ['老兵', 'Veteran'],
  'data.achievement.ach_level_50.name': ['不熄', 'Unquenched'],
  'data.achievement.ach_level_50.desc': ['达到50级', 'Reach level 50'],
  'data.achievement.ach_level_50.title': ['不熄者', 'The Unquenched'],
  'data.achievement.ach_explore_all.name': ['走遍焚境', 'Walked the Burning March'],
  'data.achievement.ach_explore_all.desc': ['探索所有区域', 'Explore every zone'],
  'data.achievement.ach_explore_all.title': ['行路人', 'Wayfarer'],
  'data.achievement.ach_quest_10.name': ['十件差事', 'Ten Errands'],
  'data.achievement.ach_quest_10.desc': ['完成10个任务', 'Complete 10 quests'],
  'data.achievement.ach_collect_legendary.name': ['死者的遗物', "A Dead Man's Heirloom"],
  'data.achievement.ach_collect_legendary.desc': ['收集一件传奇装备', 'Collect a legendary item'],
  'data.achievement.ach_collect_legendary.title': ['拾遗人', 'Gleaner'],

  // ── Random events in the Red Field (chapter1.md §13). The core logs `<key>.<zone>` when the strings have it
  // (RandomEvents.cpp RevZoneKey), so the Red Field's events speak in its own voice and the Relic Peddler's and the
  // Bound Pilgrim's lines (chapter1.md §3) have a home; the other chapters keep the shared lines until their pass ──
  'sys.event.msg.ambush.emerald_plains': ['远处，钟敲了一下。泥里伸出了手。', 'Far off, the bell tolls once. Hands come up out of the mud.'],
  'sys.event.msg.treasure_cache.emerald_plains': ['泥里半埋着一只铁箱。箱子的主人是个骑士，用不上了。',
    "An iron strongbox lies half-sunk in the mud. The knight who owned it has no more use for it."],
  'sys.event.msg.wandering_merchant.emerald_plains': ['路边有人摆开一盘圣骨，垫着块旧布。', 'By the road someone has laid out a tray of relics on an old cloth.'],
  'sys.event.msg.rescue.emerald_plains': ['一个朝圣者被绑在桩上。看着他的，是赤誓会的人。',
    'A pilgrim is tied to a stake. The Red-Oath are watching him.'],
  'sys.event.msg.environmental_puzzle.emerald_plains': ['断桩上挂着五口小钟。', 'Five handbells hang from a broken post.'],
  'zone.event.merchant.announce.emerald_plains': ['圣骨贩子：“圣骨，真货。至少有一块是真的。”',
    'The Relic Peddler: "Saint\'s bones, genuine. At least one of them is."'],
  // The pilgrim is named by the event line before it, so the completion line does not splice {npcName} in (the core
  // still passes it; an unused placeholder is fine).
  'zone.event.rescue.complete.emerald_plains': ['你割断了朝圣者身上的绳子。“他们说我走得太慢，跟不上圣火。”获得 {gold} 金币和 {exp} 经验',
    'You cut the pilgrim loose. "They said I walked too slowly to keep up with the Flame." Gained {gold} gold and {exp} EXP'],
  // ── Random events in the Red Field (chapter1.md §13) ──
  'sys.event.rescue.emerald_plains': ['被缚的朝圣者', 'The Bound Pilgrim'],
  'sys.event.puzzle.emerald_plains.prompt': ['五口小钟挂在断桩上。钟舌上刻着数，从一到五。',
    'Five handbells hang from a broken post. Numbers are cut into the clappers, one to five.'],
  'sys.event.puzzle.emerald_plains.solution': ['照钟舌上的数，从一敲到五', 'Ring them by their numbers, one to five'],
  // The solved line ("<solution> — <reward>", zone.event.puzzle.solved); the gold and exp follow in their own lines.
  'sys.event.puzzle.emerald_plains.reward': ['五口钟依次响过。断桩下压着一个油布包。',
    'The five bells ring in turn. Under the post lies an oilcloth bundle.'],

  // ── Elite affixes, re-voiced (monsters.md §3; the elite name label and the affix list). 1B review: Scorched /
  // Brutish / Dirgecursed replace §3's 焚誓 Oathfire (the prologue's Burned Oath), 赤誓 Red-Sworn (the Red-Oath faction)
  // and 丧钟 Knelled (the hub, 丧钟堡), which the elite label put before a monster's name ──
  'data.eliteAffix.fire_enhanced': ['焦灼', 'Scorched'],
  'data.eliteAffix.swift': ['驱策', 'Driven'],
  'data.eliteAffix.teleporting': ['渊行', 'Breachwalker'],
  'data.eliteAffix.extra_strong': ['蛮力', 'Brutish'],
  'data.eliteAffix.curse_aura': ['哀咒', 'Dirgecursed'],
  'data.eliteAffix.vampiric': ['噬渊', 'Breach-Drinker'],
  'data.eliteAffix.frozen': ['墓寒', 'Gravecold'],
  'sys.eliteAffix.name.fire_enhanced': ['焦灼', 'Scorched'],
  'sys.eliteAffix.name.swift': ['驱策', 'Driven'],
  'sys.eliteAffix.name.teleporting': ['渊行', 'Breachwalker'],
  'sys.eliteAffix.name.extra_strong': ['蛮力', 'Brutish'],
  'sys.eliteAffix.name.curse_aura': ['哀咒', 'Dirgecursed'],
  'sys.eliteAffix.name.vampiric': ['噬渊', 'Breach-Drinker'],
  'sys.eliteAffix.name.frozen': ['墓寒', 'Gravecold'],
};

/** One locale column of a [zh-CN, en] string table. */
export function localeColumn(table: Readonly<Record<string, readonly [string, string]>>, column: 0 | 1): Record<string, string> {
  return Object.fromEntries(Object.entries(table).map(([k, v]) => [k, v[column]]));
}
