/**
 * Chapter 1 story strings of the dark remake (package 1B part 2, narrative): the prologue, the chapter card and the
 * cutscenes `cs_rf_brand`, `cs_rf_whisper`, `cs_rf_herd`, `cs_rf_marshal_falls`, `cs_rf_siege` and `cs_rf_finale`
 * (GameData/story/script.ts; the boss intro `cs_boss_valdras` is in ./ch1World.ts). Keys are `story.<id>.<step>`, the
 * 1-based step index. Source: Docs/dark-remake/chapter1.md §8; voice: story-bible.md §11 (zh: say / whisper <= 60,
 * narrate <= 70, slide <= 110, title <= 12, subtitle <= 14 code points, checked by quests_test).
 *
 * Each key maps to [zh-CN, en]; ./zh-CN.ts and ./en.ts take one column each (see ./ch1World.ts).
 */
export const CH1_STORY_STRINGS: Readonly<Record<string, readonly [zhCN: string, en: string]>> = {
  // ── Prologue: six slides, title + text (chapter1.md §8; slide <= 110, title <= 12) ──
  'story.prologue.1.title': ['渊火', 'Abyssfire'],
  'story.prologue.1.text': ['世界在深渊之底的火中锻成。六位守火人立誓看护这团火：让它温暖，而不焚烧。',
    'The world was forged in the fire at the bottom of the Abyss. Six keepers swore to tend it: to let it warm, and never burn.'],
  'story.prologue.2.title': ['焚誓', 'The Burned Oath'],
  'story.prologue.2.text': ['六人中最耀眼的一位，最怕火灭。他焚毁誓言，要把世界投回火中重铸。其余五人以命为锁，将他缚于深渊，立下五道封印，并凿去了他的名字。',
    'The brightest of the six feared most that the fire would go out. He burned his oath to throw the world back into the fire and recast it. The other five made locks of their lives, bound him in the deep under five seals, and chiselled away his name.'],
  'story.prologue.3.title': ['低语', 'The Whisper'],
  'story.prologue.3.text': ['千年来，他只能说话。他在黑暗里寻找害怕终结的人，许给他们一团永不熄灭的火。',
    'For a thousand years he could only speak. In the dark he looked for people who feared the end, and promised them a fire that would never go out.'],
  'story.prologue.4.title': ['灰誓圣战', 'The Ashen Crusade'],
  'story.prologue.4.text': ['今年秋天，守焰教会听见了“圣火的召唤”，灰誓圣战开赴东境。最后一战前，元帅瓦德拉斯下令：死守赤幡原，一步不许退。',
    `This autumn the Church of the Kept Flame heard "the Flame's call", and the Ashen Crusade marched to the eastern border. Before the last battle Lord-Marshal Valdras gave his order: hold the Red Field. Not one step back.`],
  'story.prologue.5.title': ['渊裂', 'The Breach'],
  'story.prologue.5.text': ['那一战的血流得太多。赤幡原下的古印裂开了：裂口涌出黑血与魔物。战死的人没有躺下，他们还在执行最后的军令。',
    'Too much blood was spilled. The old seal beneath the Red Field cracked: black blood and worse welled out of the Breach. The slain did not lie down. They are still obeying their last order.'],
  'story.prologue.6.title': ['灰中醒来', 'Woken in Ash'],
  'story.prologue.6.text': ['你在丧钟堡外的焚尸坑里醒来，身上是灰，身下也是灰。掌心烙着一个印记：五焰环绕，中央一空。坑沿之上，一口钟正在敲。',
    'You wake in the burn pit outside Knellwatch, ash above you and ash below. On your palm is a brand: five flames around an empty heart. Above the lip of the pit, a bell is tolling.'],

  // ── Chapter card (mood night) ──
  // ── The epilogue's Chapter 1 slide (the ending is Chapter 5's, DR11, but this slide is the Red Field's): the web's
  // Emerald Plains text re-voiced ──
  'story.epilogue.2.title': ['赤幡原', 'The Red Field'],
  'story.epilogue.2.text': ['赤幡原上的死人都躺下了，再没有站起来。\n丧钟堡的名墙上，又添了一行名字。黄昏时，钟还是一个名字敲一下。',
    'The dead of the Red Field have lain down and not risen again.\nOn Knellwatch\'s Name Wall, a new row of names. At dusk the bell still tolls once for each.'],

  'story.chapter.emerald_plains.number': ['第一章', 'Chapter One'],
  'story.chapter.emerald_plains.title': ['灰烬中醒来', 'Woken from Ash'],
  'story.chapter.emerald_plains.subtitle': ['赤幡原', 'The Red Field'],
  'story.chapter.emerald_plains.text': ['亡者不眠。败军的最后一座堡垒里，每到黄昏，钟为每一个新的名字敲一下。',
    "The dead do not sleep. In a beaten army's last fort, every dusk, the bell tolls once for every new name."],

  // ── cs_rf_brand: The Captain rings the bell for the burial detail; Aldwin reads the brand (after q_rf_burial_detail) ──
  'story.cs_rf_brand.2': ['十个人出去收尸。你替他们合上了眼。', 'Ten went out for the dead. You closed their eyes.'],
  'story.cs_rf_brand.3': ['敲钟。十下。一下一个名字。', 'Ring the bell. Ten times. One for each name.'],
  'story.cs_rf_brand.5': ['钟声沉沉地滚过赤幡原。远处，有什么东西停下来，侧耳倾听。',
    'The toll rolls slow across the Red Field. Far off, something stops and listens.'],
  'story.cs_rf_brand.7': ['把手给我……这个烙印，我在古碑的拓本上见过。',
    'Give me your hand... I have seen this brand, on a rubbing of the old tablet.'],
  'story.cs_rf_brand.8': ['“渊火再燃之夜，灰烬中将有人醒来。其身负五焰，其心藏一空。”',
    '"On the night the Abyssfire burns again, one shall wake from the ashes, bearing five flames, an emptiness at the heart."'],
  'story.cs_rf_brand.9': ['“开门者是他，闭门者亦是他。”……你是从焚尸坑里爬出来的，孩子。',
    '"He who opens the gate shall close it." ...You climbed out of the burn pit, child.'],
  'story.cs_rf_brand.10': ['那个坑里，烧的是谁？', 'Who was burned in that pit?'],
  'story.cs_rf_brand.11': ['所有人。', 'Everyone.'],

  // ── cs_rf_whisper: The dream of the last battle and the first whisper (after q_rf_ashwheat) ──
  'story.cs_rf_whisper.2': ['你梦见了最后一战。赤幡原上，三千面旗帜烧成了一片。',
    'You dream of the last battle. On the Red Field three thousand banners burn as one.'],
  'story.cs_rf_whisper.4': ['一步不许退。退者，以叛论处，就地正法。',
    'Not one step back. Whoever retreats is a traitor, executed where he stands.'],
  'story.cs_rf_whisper.5': ['逃兵被押上古老的石台。刀落下的时候，石头裂开了。',
    'The deserters are dragged onto the ancient dais. When the blades fall, the stone splits.'],
  'story.cs_rf_whisper.7': ['他们用血，叩响了我的门。', 'They knocked on my door with blood.'],
  'story.cs_rf_whisper.8': ['而你，孩子——你会替我点灯。', 'And you, child, will light my lamps.'],
  'story.cs_rf_whisper.9': ['你是谁？', 'Who are you?'],
  'story.cs_rf_whisper.10': ['一个和你一样，被埋掉的人。', 'Someone who was buried. Like you.'],
  'story.cs_rf_whisper.11': ['你在丧钟堡的火塘边惊醒，掌心滚烫。没有风，钟却在轻轻摇晃。',
    "You wake by Knellwatch's hearth, your palm burning. There is no wind, yet the bell sways."],

  // ── cs_rf_herd: The Corpseherd falls; the dead sit down (her first kill) ──
  'story.cs_rf_herd.2': ['尸牧婆倒在车圈中央，手还攥着铃铛。',
    'The Corpseherd falls in the middle of the wagon ring, her hand still on the bell.'],
  'story.cs_rf_herd.3': ['她轻轻摇了一下：“回家吧，孩子们……天黑了。”', `She rings it once, softly: "Go home, children... it's dark."`],
  'story.cs_rf_herd.4': ['围着她的死人，一个接一个，坐了下来。', 'Around her, one by one, the dead sit down.'],
  'story.cs_rf_herd.5': ['她只是想领他们回家。我只是告诉了她，家在哪儿。',
    'She only wanted to lead them home. I only told her where home was.'],

  // ── cs_rf_marshal_falls: Valdras kneels on the dais (his first kill) ──
  'story.cs_rf_marshal_falls.2': ['……是谁……下的撤退令？', 'Who... gave the order... to retreat?'],
  'story.cs_rf_marshal_falls.3': ['他跪倒在石台上。背上那面烧焦的旗，终于垂了下来。',
    'He kneels on the dais. The scorched banner on his back finally droops.'],
  'story.cs_rf_marshal_falls.4': ['现在，孩子——灯。', 'Now, child. The lamp.'],

  // ── cs_rf_siege: The bell rings by itself; the siege begins (on accepting q_rf_last_toll) ──
  'story.cs_rf_siege.2': ['钟无人自鸣。', 'The bell rings with no hand on it.'],
  'story.cs_rf_siege.3': ['上墙！能拿刀的都上墙！', 'To the walls! Everyone who can hold a blade!'],
  'story.cs_rf_siege.5': ['听，孩子。他们在回家。', "Listen, child. They're coming home."],

  // ── cs_rf_finale: Dawn: the first flame, the dead lie down, Marl leaves for the tower (after q_rf_last_toll) ──
  'story.cs_rf_finale.2': ['天亮了。它们……停下了。', "Dawn. They've... stopped."],
  'story.cs_rf_finale.3': ['你把烙着印的手按在冰冷的钟上。', 'You lay your branded hand on the cold bell.'],
  'story.cs_rf_finale.5': ['钟声沿着灵脉沉下去，一直沉到石台。裂口里的黑火，第一次退了。',
    'The toll sinks down the ley line, all the way to the dais. For the first time, the black fire in the Breach draws back.'],
  'story.cs_rf_finale.6.title': ['第一焰', 'The First Flame'],
  'story.cs_rf_finale.6.subtitle': ['灵脉之印，重燃', 'The Seal of Veins, relit'],
  'story.cs_rf_finale.7': ['赤幡原上，那些还站着的死人，一个接一个，躺了下去。',
    'Across the Red Field the dead who still stood lie down, one by one.'],
  'story.cs_rf_finale.8': ['他们为每一个名字敲钟。从来没有人，为我敲过。', 'They toll a bell for every name. No one ever tolled one for me.'],
  'story.cs_rf_finale.9': ['第一盏灯。很好，孩子。一直走下去。', 'The first lamp. Good, child. Keep walking.'],
  'story.cs_rf_finale.10': ['东边的林子里，还有活人在等消息。', 'There are living people past the eastern wood, waiting for word.'],
  'story.cs_rf_finale.12': ['我去那座塔。那里的火，不要人偿。伤员要药圃，药圃得有人守。',
    "I'm going to that tower. Its fire asks no one to pay. The wounded need a herb garden, and a garden needs a keeper."],
  'story.cs_rf_finale.13': ['愿钟声护你。……我会一直听着它。', 'May the bell keep you. ...I will always be listening to it.'],
};
