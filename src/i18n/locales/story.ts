/**
 * Story text (prologue, chapter cards, cutscenes, boss intros, epilogue, credits).
 * Keys are referenced from src/data/story/script.ts; see docs/story.md for the story bible.
 * zh-CN is the source of truth.
 */
import type { LocaleData } from '../types';

export const STORY_ZH: LocaleData = {
  // ─── ui ───
  'story.ui.skip': 'Esc 跳过',
  'story.ui.skipTouch': '跳过 ▸▸',

  // ─── speaker ───
  'story.speaker.villain': '伊格纳罗斯',
  'story.speaker.hero': '你',

  // ─── prologue ───
  'story.prologue.1.heading': '太初',
  'story.prologue.1.title': '渊火',
  'story.prologue.1.text': '太初之时，虚空无光。\n深渊之底燃起一簇火焰，世界自火中锻成——\n山岳是它冷却的铁，江河是它未干的淬水。\n后人称之为：渊火。',
  'story.prologue.2.heading': '灵脉纪元',
  'story.prologue.2.title': '六贤守火',
  'story.prologue.2.text': '渊火的余温沿着灵脉流遍大地，万物由此而生。\n六位贤者立誓守护炉火：精灵贤者、月之祭司、矮人王、日冕女王、守望者——\n以及六人中最耀眼的一位，守火人伊格纳罗斯。',
  'story.prologue.3.heading': '千年之前',
  'story.prologue.3.title': '焚誓',
  'story.prologue.3.text': '千年前，灵脉渐冷。伊格纳罗斯不忍看世界熄灭，\n他焚毁誓言，打开深渊，要将这残缺的世界投回炉中重铸。\n火海漫过大地，精灵高塔崩塌——史称大灾变。',
  'story.prologue.4.heading': '五印',
  'story.prologue.4.title': '以血为锁',
  'story.prologue.4.text': '五位贤者以血为锁、以魂为钥，将他缚于深渊之底，立下五道封印。\n他们凿去了他的名字，从此世人只知有五贤者。\n而守望者艾瑟琳，从渊火中取走了最纯净的一缕心焰，藏于人间。',
  'story.prologue.5.heading': '低语',
  'story.prologue.5.title': '渊火复燃',
  'story.prologue.5.text': '千年来，他在深渊中低语。\n大祭司、巫师、萨满……贪求永恒者一一应声，封印接连碎裂。\n如今，渊火再度燃起。',
  'story.prologue.6.heading': '翡翠平原',
  'story.prologue.6.title': '烙印',
  'story.prologue.6.text': '那一夜，平原上的精灵塔废墟中火柱冲天。\n村民在灰烬里找到了你——毫发无伤，\n掌心却烙着一道印记：五焰环绕，中央一空。',

  // ─── chapter ───
  'story.chapter.emerald_plains.number': '第一章',
  'story.chapter.emerald_plains.title': '灰烬中醒来',
  'story.chapter.emerald_plains.subtitle': '翡翠平原',
  'story.chapter.emerald_plains.text': '精灵的灵脉在草原下低鸣，哥布林的篝火彻夜不熄。\n你带着一道不明的烙印醒来，而大地正在颤抖。',
  'story.chapter.twilight_forest.number': '第二章',
  'story.chapter.twilight_forest.title': '被抹去的名字',
  'story.chapter.twilight_forest.subtitle': '暮色森林',
  'story.chapter.twilight_forest.text': '月光已数百年未曾落地，亡者在林间徘徊。\n一段被刻意遗忘的历史，在森林最深处等你。',
  'story.chapter.anvil_mountains.number': '第三章',
  'story.chapter.anvil_mountains.title': '命运之锤',
  'story.chapter.anvil_mountains.subtitle': '铁砧山脉',
  'story.chapter.anvil_mountains.text': '矮人的熔炉冷了一百年。\n山腹深处，锻出五道封印的神锤，仍在等一双能唤醒它的手。',
  'story.chapter.scorching_desert.number': '第四章',
  'story.chapter.scorching_desert.title': '日冕余烬',
  'story.chapter.scorching_desert.subtitle': '灼热沙漠',
  'story.chapter.scorching_desert.text': '一个王国在一夜之间化为焦土，它的圣鸟至今仍在火中哀鸣。\n沙海记得一切——包括那场背叛。',
  'story.chapter.abyss_rift.number': '终章',
  'story.chapter.abyss_rift.title': '渊火',
  'story.chapter.abyss_rift.subtitle': '深渊裂隙',
  'story.chapter.abyss_rift.text': '五地之火，皆汇于此。\n门后之人已等了你一千年，而你掌心的空缺，终将被填满。',

  // ─── boss ───
  'story.boss.goblin_chief.name': '碎牙·格罗克',
  'story.boss.goblin_chief.epithet': '灰烬部落之主',
  'story.boss.werewolf_alpha.name': '沃尔甘',
  'story.boss.werewolf_alpha.epithet': '噬月之狼',
  'story.boss.mountain_troll.name': '戈尔姆',
  'story.boss.mountain_troll.epithet': '王座篡夺者',
  'story.boss.phoenix.name': '赫莉娅',
  'story.boss.phoenix.epithet': '被缚的日冕圣鸟',
  'story.boss.demon_lord.name': '伊格纳罗斯',
  'story.boss.demon_lord.epithet': '焚誓者·渊火化身',
  'story.boss.dungeon_abyss_lord.name': '卡萨诺尔',
  'story.boss.dungeon_abyss_lord.epithet': '深渊之主·迷宫的囚王',
  'story.cs_boss_kassanor.1': '迷宫最深处，碎裂的记忆沉成一片黑色的海。海面下，有什么睁开了眼。',
  'story.cs_boss_kassanor.2': '伊格纳罗斯倒下了？好。那张王座空着，这座迷宫只剩一个主人。',
  'story.cs_boss_kassanor.3': '你身上那团火，每一层记忆都在向它低语。把它交出来，我放你回到阳光下。',
  'story.cs_boss_kassanor.4': '你守着的，只是别人的残梦。',
  'story.cs_boss_kassanor.5': '那就在这残梦里，陪我一同沉没吧！',

  // ─── cs_ep_mark ───
  'story.cs_ep_mark.2': '别动，孩子。把手伸过来……让我看看你掌心那道烙印。',
  'story.cs_ep_mark.4': '烙印在暮色中泛起微光：五道火焰环成一圈，中央空无一物。',
  'story.cs_ep_mark.5': '五焰环一空……我家世代为精灵塔守碑，碑上刻的正是这个印记。',
  'story.cs_ep_mark.6': '碑文写道：“渊火再燃之夜，灰烬中将有人醒来。其身负五焰，其心藏一空。”',
  'story.cs_ep_mark.7': '“他将行过五地，重燃五印。开门者是他，闭门者亦是他。”',
  'story.cs_ep_mark.8': '……开门，还是闭门？',
  'story.cs_ep_mark.9': '我不知道，孩子。我只知道那夜的火烧光了半片草场，却唯独没伤到你。',
  'story.cs_ep_mark.10': '平原上这些怪事，恐怕都和你手上这团火有关。去吧，替我看着点。',

  // ─── cs_ep_whisper ───
  'story.cs_ep_whisper.2': '当夜，你梦回哥布林营地。篝火早已熄灭，灰烬里却仍有火在低语。',
  'story.cs_ep_whisper.4': '你醒了。我在火里看了你很久。',
  'story.cs_ep_whisper.5': '别怕。那烙印是一件礼物——只不过，是从我这里偷走的。',
  'story.cs_ep_whisper.6': '哥布林替我叩门，而你，替我点灯。走吧，一直走下去。',
  'story.cs_ep_whisper.8': '你是谁？',
  'story.cs_ep_whisper.9': '一个被抹去名字的人。等你走到门前，自会想起来。',
  'story.cs_ep_whisper.11': '你猛然惊醒。掌心的烙印烫得像一块刚出炉的炭。',

  // ─── cs_ep_finale ───
  'story.cs_ep_finale.2': '你踏过平原的最后一道山丘，地底那持续了数日的颤动，忽然止息。',
  'story.cs_ep_finale.4': '烙印上的第一道火焰亮了起来——翠绿、温和，像初春的草尖。',
  'story.cs_ep_finale.6': '灵脉之印稳住了……精灵贤者艾兰迪尔若泉下有知，也该安心了。',
  'story.cs_ep_finale.7': '可碑文说的是五印。另一道就在东边的暮色森林，几百年前就碎了。',
  'story.cs_ep_finale.8': '我写了封信，托人捎给森林营地的侦察兵。孩子，一路当心，记得回来。',
  'story.cs_ep_finale.9': '一盏灯亮了。还有四盏。',
  'story.cs_ep_finale.10': '向东，是终年不见天日的森林。你的旅程，才刚刚开始。',
  'story.cs_ep_finale.11': '村长托药师带着种子动身，去往精灵塔的废墟。那片灰烬里，将长出一座药草园。',

  // ─── cs_tf_hermit ───
  'story.cs_tf_hermit.1': '营地的篝火噼啪作响，你却仍听得见隐士那沙哑的声音……',
  'story.cs_tf_hermit.3': '五贤者？孩子，那是后人的谎话。当年立誓守火的，是六个人。',
  'story.cs_tf_hermit.4': '第六位叫伊格纳罗斯，守火人，六人中最耀眼的一个。灵脉变冷时，他第一个慌了。',
  'story.cs_tf_hermit.5': '他说世界是一件铸坏的器物，该投回渊火里重铸。于是他焚了誓，打开了深渊。',
  'story.cs_tf_hermit.6': '五贤者拿命把他锁进深渊，又凿去了他的名字。可锁会锈，而他很有耐心。',
  'story.cs_tf_hermit.7': '那个想与生命之树同生的巫师，就是听了他的低语。月辉之印由此而碎，森林再不见天日。',
  'story.cs_tf_hermit.8': '你掌心那片空缺，原本是他的座位。艾瑟琳藏起的心焰，选中了你。',
  'story.cs_tf_hermit.10': '老东西说得不全对。心焰本就是我的。而你，终将回到我这里。',
  'story.cs_tf_hermit.12': '……这些老故事我听不懂。我只知道狼王守着去黑暗之源的路，得先解决它。',

  // ─── cs_tf_finale ───
  'story.cs_tf_finale.2': '你把烙印按进黑暗之源。腐烂的树根之间，一道银白的光缓缓亮起。',
  'story.cs_tf_finale.4': '数百年来第一次，月光穿透暮色，落在了林间的空地上。',
  'story.cs_tf_finale.5': '烙印上的第二道火焰亮了——银白如霜，带着月之祭司瑟莲娜的一声叹息。',
  'story.cs_tf_finale.7': '你听见了吗？……亡灵们在道谢。它们终于可以睡了。',
  'story.cs_tf_finale.9': '月辉之印重燃了。矮人那边刚来了信：铁砧山脉的地底，有东西在翻身。',
  'story.cs_tf_finale.10': '两盏灯了。你走得真快——比她当年还快。',
  'story.cs_tf_finale.11': '你没有回答，只是握紧了发烫的掌心，朝东方的群山走去。',
  'story.cs_tf_finale.12': '森林隐士收起竹杖，说要去精灵塔下守一口月井，给流离的灵兽留个歇脚处。',

  // ─── cs_am_hammer ───
  'story.cs_am_hammer.2': '秘银核心、先祖的符文、布鲁恩王的锤头……齐了！站到炉前来，烙印者。',
  'story.cs_am_hammer.3': '冷了一百年的熔炉，在你走近的那一刻，自己燃了起来。',
  'story.cs_am_hammer.6': '炉火认得你！当年布鲁恩王就是用渊火的余烬锻出这把锤，又用这把锤锻出了五道封印。',
  'story.cs_am_hammer.7.title': '命运之锤',
  'story.cs_am_hammer.7.subtitle': '布鲁恩王的遗物',
  'story.cs_am_hammer.8': '锤起锤落，火星如雨。第九声落下时，锤身的符文与你的烙印一同亮起。',
  'story.cs_am_hammer.9': '它是你的了。记住：锤能锻印，也能锻钥匙。总有一天你会用得上。',
  'story.cs_am_hammer.10': '布鲁恩的锤子……他就是用它，钉死了我的锁。真好，现在它在你手里。',
  'story.cs_am_hammer.11': '脸色怎么这么难看？哼，不管是什么在跟你说话，先把王座上那头巨魔轰下来！',

  // ─── cs_am_finale ───
  'story.cs_am_finale.2': '巨魔的尸体从王座上滚落，露出底下一座刻满符文的古老铁砧。',
  'story.cs_am_finale.3': '你举起命运之锤，敲下第一击。铁砧应声长鸣，响彻整座山脉。',
  'story.cs_am_finale.6': '烙印上的第三道火焰亮了——赤红如铁水，沉稳如山岩。',
  'story.cs_am_finale.8': '铁砧又响了！一百年了……先祖在上，我这把老骨头还能听到这一声！',
  'story.cs_am_finale.9': '可你得知道，孩子：第四印在南边沙海，碎得最早，也碎得最惨——是被人亲手砸碎的。',
  'story.cs_am_finale.10': '三盏。你知道那个大祭司为什么砸碎封印吗？因为我许了他永恒。',
  'story.cs_am_finale.11': '我从不食言。去看看吧，看看他如今有多永恒。',
  'story.cs_am_finale.12': '南方的地平线上，一道烟柱直冲天际。',
  'story.cs_am_finale.13': '矮人长老挑了一座小炉，派工匠翻山送往余烬之塔：守火人的家，得有座像样的炉台。',

  // ─── cs_sd_oasis ───
  'story.cs_sd_oasis.2': '泉水映出的，是千年前的沙海——不，那是一座王国。',
  'story.cs_sd_oasis.4.title': '日冕王国',
  'story.cs_sd_oasis.4.subtitle': '千年之前·沙海之心',
  'story.cs_sd_oasis.5': '尖塔林立，泉流如织。日冕女王纳芙莎立于高台，圣鸟赫莉娅在她肩头燃着金色的火。',
  'story.cs_sd_oasis.6': '女王以身为锁之后，大祭司接过了守印之责。他守到须发皆白，也守出了对死亡的恐惧。',
  'story.cs_sd_oasis.7': '然后，你听见一个熟悉的声音，在千年前对他说：',
  'story.cs_sd_oasis.8': '你不必死。打开它，你将与火同寿。',
  'story.cs_sd_oasis.10': '大祭司砸碎了日冕之印。那一夜，烈火从地底喷涌而出，整个王国化为焦土。',
  'story.cs_sd_oasis.11': '赫莉娅被渊火锁住，从此只为裂隙燃烧。大祭司也得到了永恒——化作沙中不死的亡灵。',
  'story.cs_sd_oasis.13': '你看见的，是我们祖先的国。游牧民……不过是那场大火烧剩下的人。',
  'story.cs_sd_oasis.14': '赫莉娅不是怪物。放她自由吧，烙印者。她自由了，火才会回家。',

  // ─── cs_sd_finale ───
  'story.cs_sd_finale.2': '赫莉娅的身躯在烈焰中化为灰烬。片刻寂静之后，灰烬里传出一声清亮的鸣叫。',
  'story.cs_sd_finale.4': '新生的圣鸟振翅而起，金色的火焰掠过裂隙，将它一寸一寸地缝合。',
  'story.cs_sd_finale.5': '烙印上的第四道火焰亮了——金黄如正午的太阳。',
  'story.cs_sd_finale.7': '今夜的风是凉的……祖先们，你们看见了吗？',
  'story.cs_sd_finale.8': '可沙底下的火都在往一个地方流——深渊裂隙。门后的那个人，醒了。',
  'story.cs_sd_finale.9': '四盏灯。够了。来吧，孩子——到门前来，把最后一盏也带来。',
  'story.cs_sd_finale.11': '烙印上四焰齐燃。中央那片空缺，第一次隐隐作痛。',
  'story.cs_sd_finale.12': '游牧民的驼铃掉头向西。从此余烬之塔下，多了一座商队驿站。',

  // ─── cs_ar_gate ───
  'story.cs_ar_gate.2': '你也听见了，对吗？门后那个声音。守望者听了一千年，听着听着，就不再是自己了。',
  'story.cs_ar_gate.4.title': '混沌王座',
  'story.cs_ar_gate.4.subtitle': '渊火之门',
  'story.cs_ar_gate.5': '王座之后，是一道由五重符文锁住的巨门。四道符文已经重燃，最后一道在风中明灭。',
  'story.cs_ar_gate.6': '你终于来了。看看你的手——四盏灯亮着，还有一盏将熄。',
  'story.cs_ar_gate.7': '你以为自己在修补封印？锁是他们的魂，钥匙也是。你一路收下的，正是开门的钥匙。',
  'story.cs_ar_gate.10': '烙印骤然灼痛。四道火焰朝着巨门的方向微微倾斜，仿佛被什么牵引着。',
  'story.cs_ar_gate.11': '我是伊格纳罗斯，第六位贤者。你掌心那片空缺，是我的座位。',
  'story.cs_ar_gate.13': '开门者是他，闭门者亦是他——碑文从没说错。钥匙在你手里，门往哪边关，你来定。',

  // ─── cs_ar_seal ───
  'story.cs_ar_seal.2': '碎片齐了。命运之锤给我——不，你来锻。只有烙印者的手，这把锤才肯听。',
  'story.cs_ar_seal.3': '锤起锤落。五片封印碎片在深渊的紫火中熔为一体，化作一柄暗银色的钥匙。',
  'story.cs_ar_seal.5.title': '终焉之钥',
  'story.cs_ar_seal.5.subtitle': '以五魂为锁，以一心为钥',
  'story.cs_ar_seal.6': '烙印上的第五道火焰亮了——暗银如夜，那是守望者艾瑟琳留下的最后一道光。',
  'story.cs_ar_seal.7': '五道火焰能开门，也能锁门。可要让门永远合上，钥匙还缺一颗心——心焰。',
  'story.cs_ar_seal.8': '艾瑟琳是我这一脉的先祖，也是他的亲妹妹。她留下的话是：心焰归渊之时，持焰者将随火而去。',
  'story.cs_ar_seal.10': '你低头看着掌心。那片空缺之下，原来一直有什么在跳动——温暖，安静，像一颗心。',
  'story.cs_ar_seal.11': '我知道。',
  'story.cs_ar_seal.12': '你会把它交给我的。所有人最后都会。',
  'story.cs_ar_seal.13': '守望者等了一千年，等来一个肯去的人。去吧，孩子。愿渊火照你回家。',

  // ─── cs_ar_fall ───
  'story.cs_ar_fall.4': '渊火化身轰然崩裂。烈焰褪去之后，王座前只剩一个苍老的人影。',
  'story.cs_ar_fall.5': '……一千年了。我只是不想看它熄灭。灵脉在冷，世界在死，你们都看不见吗？',
  'story.cs_ar_fall.7': '你举起掌心。五道火焰依次亮起，而在那片空缺之中，心焰第一次显出了形状。',
  'story.cs_ar_fall.8': '艾瑟琳……原来你把它藏在了这里。你还是老样子，宁可把火分给别人。',
  'story.cs_ar_fall.9': '你将终焉之钥插入巨门，把心焰交还渊火——不是献给他，而是还给这个世界。',
  'story.cs_ar_fall.11': '渊火没有吞噬你。它平静下来，像炉膛里的余烬，把暖意沿着灵脉送往五方大地。',
  'story.cs_ar_fall.12': '它……在重燃。不必焚尽一切，也能重燃……原来，她才是对的。',
  'story.cs_ar_fall.13': '伊格纳罗斯化作一缕灰烬，随风散去。那扇巨门，无声地合拢了。',
  'story.cs_ar_fall.14': '你掌心的五焰缓缓黯淡，化作一道银环。环的中央，留着一点小小的、温暖的光。',

  // ─── cs_boss_goblin_chief ───
  'story.cs_boss_goblin_chief.3': '烙印！就是那只手！火说了，谁砍下那只手，谁就永远不死！',
  'story.cs_boss_goblin_chief.5': '别当真，他不过是条听话的狗。……不过，让我看看你怎么对付狗。',

  // ─── cs_boss_werewolf_alpha ───
  'story.cs_boss_werewolf_alpha.2': '一声狼嚎撕开夜幕。月亮早已死去，它却仍在对着天空长啸。',
  'story.cs_boss_werewolf_alpha.4': '吼……月亮……不回答了。现在，只有那个声音……在回答我。',
  'story.cs_boss_werewolf_alpha.5': '瑟莲娜的圣狼，最忠诚的守卫。忠诚真是好东西——换个主人就行。',

  // ─── cs_boss_mountain_troll ───
  'story.cs_boss_mountain_troll.3': '小东西！这椅子是戈尔姆的！椅子底下有火在唱歌，只唱给戈尔姆听！',
  'story.cs_boss_mountain_troll.5': '巨魔的脑子装不下多少东西。正好，只装得下我。',

  // ─── cs_boss_phoenix ───
  'story.cs_boss_phoenix.2': '热浪扭曲了空气。一只燃烧的巨鸟从裂隙中升起，每一次振翅都洒下火雨。',
  'story.cs_boss_phoenix.4': '看看她。纳芙莎女王最心爱的鸟，如今只为我燃烧。',
  'story.cs_boss_phoenix.5': '圣鸟发出一声凄厉的长鸣。那鸣声里有愤怒，也有哀求。',

  // ─── cs_boss_demon_lord ───
  'story.cs_boss_demon_lord.2': '混沌王座上，渊火凝成一具巨大的躯体。千年的低语，终于有了面孔。',
  'story.cs_boss_demon_lord.4': '你把五盏灯都带来了，还有她藏起的心焰。很好，今天一切都将回到炉中。',
  'story.cs_boss_demon_lord.5': '我要重铸一个永不熄灭的世界。不会冷，不会死，不会再有谁被遗忘。',
  'story.cs_boss_demon_lord.6': '这火，不是你的。',
  'story.cs_boss_demon_lord.7': '那就让火来裁决吧——守火人的继承者！',

  // ─── epilogue ───
  'story.epilogue.1.heading': '尾声',
  'story.epilogue.1.title': '门已合拢',
  'story.epilogue.1.text': '深渊之门合拢的那一刻，五方大地同时感到一阵暖意。\n不是烈焰，而像冬夜里，有人往炉中添了一把柴。',
  'story.epilogue.2.title': '翡翠平原',
  'story.epilogue.2.text': '灵脉之印重焕新绿，精灵塔的废墟上开满了野花。\n村长在古碑旁添了一行新字，字迹歪歪扭扭，却刻得很深。',
  'story.epilogue.3.title': '森林与群山',
  'story.epilogue.3.text': '暮色森林迎来了数百年来第一个清晨，亡者安眠，狼群重新对月而歌。\n铁砧山脉的熔炉日夜不熄，矮人们回到了先祖的厅堂。',
  'story.epilogue.4.title': '沙海与深渊',
  'story.epilogue.4.text': '赫莉娅在绿洲上空盘旋，干涸的泉眼一个接一个涌出清水。\n深渊裂隙渐渐冷却，守望者卸下了一千年的重担。',
  'story.epilogue.5.title': '守火人',
  'story.epilogue.5.text': '你活了下来。掌心的烙印褪成一道银环，\n环心留着一点微光——那是渊火的谢意，也是一份新的誓言。\n人们开始称你为：守火人。',
  'story.epilogue.6.title': '渊火',
  'story.epilogue.6.text': '渊火从未熄灭。\n它只是回到了炉膛里，温暖着这个不完美、却值得守护的世界。',

  // ─── credits ───
  'story.credits.1.title': '渊火',
  'story.credits.1.text': '一个关于火、誓言与归乡的故事',
  'story.credits.2.heading': '设计与开发',
  'story.credits.2.title': 'feuvan',
  'story.credits.3.heading': '技术',
  'story.credits.3.text': 'Built with Phaser 3 · TypeScript · Vite',
  'story.credits.4.heading': '特别鸣谢',
  'story.credits.4.text': '每一位走到这里的守火人',
  'story.credits.5.title': '感谢游玩',
  'story.credits.6.text': '深渊之下，还有更深的黑暗。\n噩梦难度，正在等你。',
  // ─── cs_tf_moonfang (Volgan's cub) ───
  'story.cs_tf_moonfang.2': '狼王倒下，眼中的渊火一点点熄灭。千年来，它第一次安静下来。',
  'story.cs_tf_moonfang.3': '尸身旁的蕨丛里钻出一只银灰的幼狼，额上一弯新月似的白纹。',
  'story.cs_tf_moonfang.4.title': '月牙',
  'story.cs_tf_moonfang.4.subtitle': '沃尔甘之子',
  'story.cs_tf_moonfang.5': '它嗅了嗅你掌心的烙印，没有逃走。',
  'story.cs_tf_moonfang.6': '……跟我走吧。',
  'story.cs_tf_moonfang.7': '幼狼低低呜了一声，跟在了你的脚边。',
  // ─── cs_sd_helia (Helia's ember) ───
  'story.cs_sd_helia.2': '重生的火光散去，沙地上还留着一小团余烬，迟迟不肯熄灭。',
  'story.cs_sd_helia.4': '余烬里探出一只雏鸟，羽尖燃着金红的火，怯生生地望着你。',
  'story.cs_sd_helia.5.title': '赫莉娅之烬',
  'story.cs_sd_helia.5.subtitle': '日冕圣鸟的遗火',
  'story.cs_sd_helia.7': '圣鸟把最后一点火留给了你。老辈说，这样的雏鸟，一生只认一个人。',
  'story.cs_sd_helia.8': '雏鸟扑腾着落上你的肩头，暖意透过衣衫，一直暖到掌心。',
  // ─── cs_ar_altar (the warden's lamp goes to the tower) ───
  'story.cs_ar_altar.2': '深渊的精华，我收下了。它曾是火，终究也要回到火里。',
  'story.cs_ar_altar.3': '心焰在你掌中，我替你守不住它。可它的余温，我能替你守。',
  'story.cs_ar_altar.4': '守望者取下颈间的银灯交给信使，送往余烬之塔，去点亮塔下那座荒了千年的祭坛。',
  'story.cs_ar_altar.5': '回塔时献上余烬，祭坛会护你一程。这是艾瑟琳的血脉能给你的全部了。',
  // ─── cs_tower_home (first visit to the Ember Tower) ───
  'story.cs_tower_home.2.title': '余烬之塔',
  'story.cs_tower_home.2.subtitle': '守火人的炉膛',
  'story.cs_tower_home.3': '你在这里的灰烬中醒来。那时塔已倾颓，只剩焦黑的石阶和一地冷灰。',
  'story.cs_tower_home.5': '孩子，你回来了。老头子先来一步，替大家暖暖屋子。',
  'story.cs_tower_home.6': '这里曾经是灰烬，现在是你的家。',
  'story.cs_tower_home.7': '你每点亮一道封印，就会有人循着火光来到塔下，修好一翼。',
  'story.cs_tower_home.8': '塔基之下，灵脉的余温轻轻一颤，像是在回应。',
};

export const STORY_EN: LocaleData = {
  // ─── ui ───
  'story.ui.skip': 'Esc to skip',
  'story.ui.skipTouch': 'Skip ▸▸',

  // ─── speaker ───
  'story.speaker.villain': 'Ignaroth',
  'story.speaker.hero': 'You',

  // ─── prologue ───
  'story.prologue.1.heading': 'In the Beginning',
  'story.prologue.1.title': 'The Abyssfire',
  'story.prologue.1.text': 'In the beginning, the void held no light.\nThen a flame was kindled at the bottom of the Abyss, and the world was forged in it:\nthe mountains its cooling iron, the rivers its quenching water.\nThey called it the Abyssfire.',
  'story.prologue.2.heading': 'The Age of Veins',
  'story.prologue.2.title': 'Six Keepers of the Flame',
  'story.prologue.2.text': 'The fire\'s warmth ran through the land along the ley lines, and all things lived by it.\nSix sages swore to tend the flame: an elf, a moon priestess, a dwarf king, a sun queen, a warden,\nand the brightest of them all, Ignaroth the Flamekeeper.',
  'story.prologue.3.heading': 'A Thousand Years Ago',
  'story.prologue.3.title': 'The Burning of the Oath',
  'story.prologue.3.text': 'A thousand years ago, the ley lines began to cool. Ignaroth could not bear to watch the world go dark.\nHe burned his oath and opened the Abyss, to cast this flawed world back into the forge.\nFire swept the land and the elven towers fell. They call it the Great Cataclysm.',
  'story.prologue.4.heading': 'The Five Seals',
  'story.prologue.4.title': 'Blood for a Lock',
  'story.prologue.4.text': 'The five gave their blood for a lock and their souls for a key, bound him in the depths, and set five seals.\nThey chiselled his name from every stone, and the world remembered only five sages.\nAnd Aethelyn the Warden stole the purest spark of the fire, the Heartflame, and hid it among mortals.',
  'story.prologue.5.heading': 'The Whispers',
  'story.prologue.5.title': 'The Fire Stirs',
  'story.prologue.5.text': 'For a thousand years he whispered from the deep.\nA high priest, a sorcerer, a shaman... one by one the hungry answered, and the seals broke.\nNow the Abyssfire burns again.',
  'story.prologue.6.heading': 'The Emerald Plains',
  'story.prologue.6.title': 'The Brand',
  'story.prologue.6.text': 'That night, a pillar of fire rose from the elven ruins on the plains.\nThe villagers found you in the ashes, unburned,\nwith a brand on your palm: five flames in a ring around an empty heart.',

  // ─── chapter ───
  'story.chapter.emerald_plains.number': 'Chapter One',
  'story.chapter.emerald_plains.title': 'Wake from the Ashes',
  'story.chapter.emerald_plains.subtitle': 'The Emerald Plains',
  'story.chapter.emerald_plains.text': 'Elven ley lines hum beneath the grass, and goblin fires burn all night.\nYou wake with a brand you do not understand, and the earth is trembling.',
  'story.chapter.twilight_forest.number': 'Chapter Two',
  'story.chapter.twilight_forest.title': 'The Erased Name',
  'story.chapter.twilight_forest.subtitle': 'The Twilight Forest',
  'story.chapter.twilight_forest.text': 'No moonlight has touched this ground in centuries, and the dead walk the woods.\nA history someone chose to forget waits for you in the deepest shade.',
  'story.chapter.anvil_mountains.number': 'Chapter Three',
  'story.chapter.anvil_mountains.title': 'The Hammer of Fate',
  'story.chapter.anvil_mountains.subtitle': 'The Anvil Mountains',
  'story.chapter.anvil_mountains.text': 'The dwarven forges have been cold for a hundred years.\nDeep in the mountain, the hammer that forged the five seals waits for a hand to wake it.',
  'story.chapter.scorching_desert.number': 'Chapter Four',
  'story.chapter.scorching_desert.title': 'Embers of the Sun Crown',
  'story.chapter.scorching_desert.subtitle': 'The Scorching Desert',
  'story.chapter.scorching_desert.text': 'A kingdom burned to glass in a single night, and its sacred bird still cries out in the flames.\nThe sands remember everything, even the betrayal.',
  'story.chapter.abyss_rift.number': 'Final Chapter',
  'story.chapter.abyss_rift.title': 'Abyssfire',
  'story.chapter.abyss_rift.subtitle': 'The Abyss Rift',
  'story.chapter.abyss_rift.text': 'The fires of five lands all flow here.\nThe one behind the gate has waited a thousand years for you, and the emptiness in your palm will at last be filled.',

  // ─── boss ───
  'story.boss.goblin_chief.name': 'Grokk Brokentooth',
  'story.boss.goblin_chief.epithet': 'Chief of the Ashen Tribe',
  'story.boss.werewolf_alpha.name': 'Volgan',
  'story.boss.werewolf_alpha.epithet': 'The Moon-Eater',
  'story.boss.mountain_troll.name': 'Gorm',
  'story.boss.mountain_troll.epithet': 'Usurper of the Throne',
  'story.boss.phoenix.name': 'Helia',
  'story.boss.phoenix.epithet': 'The Bound Sunbird',
  'story.boss.demon_lord.name': 'Ignaroth',
  'story.boss.demon_lord.epithet': 'The Oathburner, Abyssfire Incarnate',
  'story.boss.dungeon_abyss_lord.name': 'Kassanor',
  'story.boss.dungeon_abyss_lord.epithet': 'Lord of the Abyss, Prisoner-King of the Labyrinth',
  'story.cs_boss_kassanor.1': 'At the labyrinth\'s heart, shattered memories settle into a black sea. Beneath its surface, something opens its eyes.',
  'story.cs_boss_kassanor.2': 'Ignaroth has fallen? Good. His throne stands empty, and this labyrinth has but one master now.',
  'story.cs_boss_kassanor.3': 'Every memory down here whispers to the fire you carry. Give it to me, and I will let you walk back into the sun.',
  'story.cs_boss_kassanor.4': 'All you guard is someone else\'s broken dream.',
  'story.cs_boss_kassanor.5': 'Then drown in it with me!',

  // ─── cs_ep_mark ───
  'story.cs_ep_mark.2': 'Hold still, child. Give me your hand... let me see that brand on your palm.',
  'story.cs_ep_mark.4': 'The brand glows faintly in the dusk: five flames in a ring, and nothing at its heart.',
  'story.cs_ep_mark.5': 'Five flames around an emptiness... My family has kept the elven tower\'s stele for generations. This is the sign carved on it.',
  'story.cs_ep_mark.6': 'The stele reads: "On the night the Abyssfire wakes, one shall rise from the ashes, bearing five flames and an empty heart."',
  'story.cs_ep_mark.7': '"They shall walk five lands and rekindle five seals. The one who opens the gate is the one who closes it."',
  'story.cs_ep_mark.8': '...Open it, or close it?',
  'story.cs_ep_mark.9': 'I don\'t know, child. I only know that fire burned half the meadow that night, and did not touch you.',
  'story.cs_ep_mark.10': 'I fear every strange thing on these plains is tied to the fire in your hand. Go on. Keep an eye out for me.',

  // ─── cs_ep_whisper ───
  'story.cs_ep_whisper.2': 'That night you dream of the goblin camp. The bonfires are long dead, yet something in the ashes still whispers.',
  'story.cs_ep_whisper.4': 'You\'re awake. I have watched you from the fire for a long time.',
  'story.cs_ep_whisper.5': 'Don\'t be afraid. That brand is a gift. It was only ever stolen from me.',
  'story.cs_ep_whisper.6': 'The goblins knock on my door. You will light my lamps. Walk on. Keep walking.',
  'story.cs_ep_whisper.8': 'Who are you?',
  'story.cs_ep_whisper.9': 'Someone whose name was erased. When you reach the gate, you will remember.',
  'story.cs_ep_whisper.11': 'You wake with a start. The brand on your palm burns like a coal fresh from the fire.',

  // ─── cs_ep_finale ───
  'story.cs_ep_finale.2': 'As you crest the last hill of the plains, the tremor that has shaken the ground for days falls silent.',
  'story.cs_ep_finale.4': 'The first flame of the brand lights up: green and gentle, like new spring grass.',
  'story.cs_ep_finale.6': 'The Seal of Veins holds... If the elven sage Elandir can see us, he may finally rest.',
  'story.cs_ep_finale.7': 'But the stele speaks of five seals. Another lies in the Twilight Forest to the east, and it broke centuries ago.',
  'story.cs_ep_finale.8': 'I\'ve sent a letter to the scout at the forest camp. Be careful on the road, child, and come home.',
  'story.cs_ep_finale.9': 'One lamp lit. Four to go.',
  'story.cs_ep_finale.10': 'To the east lies a forest that never sees the sun. Your journey has only begun.',
  'story.cs_ep_finale.11': 'The elder sends the herbalist off with a bag of seeds, to the ruined elven tower. A garden will grow from those ashes.',

  // ─── cs_tf_hermit ───
  'story.cs_tf_hermit.1': 'The campfire crackles, but you can still hear the hermit\'s rasping voice...',
  'story.cs_tf_hermit.3': 'Five sages? That\'s a lie the living tell, child. Six swore to keep the flame.',
  'story.cs_tf_hermit.4': 'The sixth was Ignaroth, the Flamekeeper, brightest of them all. When the ley lines cooled, he was the first to panic.',
  'story.cs_tf_hermit.5': 'He said the world was a flawed casting, fit only for the furnace. So he burned his oath and opened the Abyss.',
  'story.cs_tf_hermit.6': 'The five paid with their lives to lock him in the deep, and chiselled out his name. But locks rust, and he is patient.',
  'story.cs_tf_hermit.7': 'The sorcerer who fused himself with the Tree of Life was listening to his whispers. The Moon Seal broke, and the sun never returned.',
  'story.cs_tf_hermit.8': 'That emptiness in your palm was his seat. The Heartflame Aethelyn hid has chosen you.',
  'story.cs_tf_hermit.10': 'The old man leaves things out. The Heartflame was always mine. And you will come back to me in the end.',
  'story.cs_tf_hermit.12': '...I don\'t follow old stories. I know the wolf king guards the road to the dark source. It goes first.',

  // ─── cs_tf_finale ───
  'story.cs_tf_finale.2': 'You press the brand into the dark source. Between the rotting roots, a silver light slowly rises.',
  'story.cs_tf_finale.4': 'For the first time in centuries, moonlight pierces the gloom and falls on the forest floor.',
  'story.cs_tf_finale.5': 'The second flame of the brand lights up, frost-white, carrying a sigh from Selenne the Moon Priestess.',
  'story.cs_tf_finale.7': 'Do you hear it...? The dead are thanking you. At last they can sleep.',
  'story.cs_tf_finale.9': 'The Moon Seal burns again. A letter just came from the dwarves: something is stirring beneath the Anvil Mountains.',
  'story.cs_tf_finale.10': 'Two lamps. You walk so quickly. Faster than she ever did.',
  'story.cs_tf_finale.11': 'You say nothing. You close your burning hand and walk toward the mountains in the east.',
  'story.cs_tf_finale.12': 'The forest hermit takes up his staff. He will keep a moon well beneath the elven tower, a resting place for stray ley-beasts.',

  // ─── cs_am_hammer ───
  'story.cs_am_hammer.2': 'Mithril cores, our ancestors\' runes, King Bruun\'s hammerhead... all here! Step up to the forge, brand-bearer.',
  'story.cs_am_hammer.3': 'The forge, cold for a hundred years, bursts into flame the moment you draw near.',
  'story.cs_am_hammer.6': 'The fire knows you! King Bruun forged this hammer from embers of the Abyssfire, then forged the five seals with it.',
  'story.cs_am_hammer.7.title': 'The Hammer of Fate',
  'story.cs_am_hammer.7.subtitle': 'Relic of King Bruun',
  'story.cs_am_hammer.8': 'The hammer rises and falls in a rain of sparks. On the ninth blow, its runes and your brand flare as one.',
  'story.cs_am_hammer.9': 'It\'s yours now. Remember: a hammer that forges seals can forge keys. One day you\'ll need that.',
  'story.cs_am_hammer.10': 'Bruun\'s hammer... he used it to nail my locks shut. How lovely. Now it is in your hands.',
  'story.cs_am_hammer.11': 'Why so pale? Hmph. Whatever is talking to you, first knock that troll off our throne!',

  // ─── cs_am_finale ───
  'story.cs_am_finale.2': 'The troll\'s body rolls from the throne, revealing an ancient anvil carved with runes beneath it.',
  'story.cs_am_finale.3': 'You raise the Hammer of Fate and strike. The anvil rings out, and the whole mountain answers.',
  'story.cs_am_finale.6': 'The third flame of the brand lights up, red as molten iron and steady as stone.',
  'story.cs_am_finale.8': 'The anvil sings again! A hundred years... By my ancestors, these old bones lived to hear it!',
  'story.cs_am_finale.9': 'But know this, child: the fourth seal, in the southern desert, broke first and worst. Someone smashed it by hand.',
  'story.cs_am_finale.10': 'Three. Do you know why the high priest broke his seal? Because I promised him forever.',
  'story.cs_am_finale.11': 'I always keep my word. Go and see how eternal he is now.',
  'story.cs_am_finale.12': 'On the southern horizon, a column of smoke climbs into the sky.',
  'story.cs_am_finale.13': 'The dwarf elder picks out a small forge and sends an artisan over the mountains to the Ember Tower: a flamekeeper\'s home needs a proper hearth.',

  // ─── cs_sd_oasis ───
  'story.cs_sd_oasis.2': 'The spring shows you the desert of a thousand years ago. No: a kingdom.',
  'story.cs_sd_oasis.4.title': 'The Sun Crown Kingdom',
  'story.cs_sd_oasis.4.subtitle': 'A Thousand Years Ago',
  'story.cs_sd_oasis.5': 'Spires everywhere, streams like woven silk. Queen Nafsha stands on the high terrace, the sunbird Helia burning gold upon her shoulder.',
  'story.cs_sd_oasis.6': 'When the queen gave herself to the seal, her high priest took up its watch. He kept it until his hair turned white, and learned to fear death.',
  'story.cs_sd_oasis.7': 'Then you hear a familiar voice, a thousand years ago, speaking to him:',
  'story.cs_sd_oasis.8': 'You need not die. Open it, and you will live as long as the fire.',
  'story.cs_sd_oasis.10': 'The high priest shattered the Sun Seal. That night fire burst from the earth, and the kingdom became ash.',
  'story.cs_sd_oasis.11': 'Helia was chained by the Abyssfire, to burn for the rift forever. The high priest got his eternity: an undying wraith in the sand.',
  'story.cs_sd_oasis.13': 'What you saw was our ancestors\' kingdom. We nomads are only what that fire left behind.',
  'story.cs_sd_oasis.14': 'Helia is no monster. Set her free, brand-bearer. When she is free, the fire can go home.',

  // ─── cs_sd_finale ───
  'story.cs_sd_finale.2': 'Helia\'s body crumbles to ash in the blaze. After a moment of silence, a clear cry rings out from the ashes.',
  'story.cs_sd_finale.4': 'The reborn sunbird takes wing, and its golden fire sweeps across the rift, stitching it shut inch by inch.',
  'story.cs_sd_finale.5': 'The fourth flame of the brand lights up, golden as the noonday sun.',
  'story.cs_sd_finale.7': 'The wind is cool tonight... Ancestors, do you see?',
  'story.cs_sd_finale.8': 'But every fire under the sand is flowing to one place: the Abyss Rift. The one behind the gate is awake.',
  'story.cs_sd_finale.9': 'Four lamps. Enough. Come, child. Come to the gate, and bring the last one with you.',
  'story.cs_sd_finale.11': 'Four flames burn on the brand. For the first time, the empty heart of it aches.',
  'story.cs_sd_finale.12': 'The nomads\' camel bells turn west. From now on, a caravan post stands beneath the Ember Tower.',

  // ─── cs_ar_gate ───
  'story.cs_ar_gate.2': 'You heard it too, didn\'t you? The voice behind the gate. Wardens have listened for a thousand years, and listening, stopped being themselves.',
  'story.cs_ar_gate.4.title': 'The Throne of Chaos',
  'story.cs_ar_gate.4.subtitle': 'Gate of the Abyssfire',
  'story.cs_ar_gate.5': 'Behind the throne stands a vast gate bound by five runes. Four burn anew. The last one gutters in the wind.',
  'story.cs_ar_gate.6': 'At last you\'ve come. Look at your hand: four lamps lit, and one about to go out.',
  'story.cs_ar_gate.7': 'You thought you were mending seals? Their souls are the locks, and the keys. What you gathered on the road is the key to this gate.',
  'story.cs_ar_gate.10': 'The brand sears. Its four flames lean toward the gate, as if something were pulling them.',
  'story.cs_ar_gate.11': 'I am Ignaroth, the sixth sage. The emptiness in your palm is my seat.',
  'story.cs_ar_gate.13': 'The one who opens is the one who closes. The stele was right. The key is in your hand. Which way the gate swings is yours to choose.',

  // ─── cs_ar_seal ───
  'story.cs_ar_seal.2': 'The fragments are all here. Give me the Hammer. No, you forge it. It answers only to the brand-bearer\'s hand.',
  'story.cs_ar_seal.3': 'The hammer rises and falls. Five seal fragments melt together in the violet fire of the Abyss and become a key of dark silver.',
  'story.cs_ar_seal.5.title': 'The Final Key',
  'story.cs_ar_seal.5.subtitle': 'Five Souls Lock, One Heart Turns',
  'story.cs_ar_seal.6': 'The fifth flame of the brand lights up, dark silver as night: the last light left by Aethelyn the Warden.',
  'story.cs_ar_seal.7': 'Five flames can open the gate or lock it. But to shut it forever, the key needs a heart: the Heartflame.',
  'story.cs_ar_seal.8': 'Aethelyn was the first of my line, and his own sister. Her words were: when the Heartflame returns to the deep, its bearer goes with the fire.',
  'story.cs_ar_seal.10': 'You look down at your palm. Beneath the emptiness, something has been beating all along: warm, quiet, like a heart.',
  'story.cs_ar_seal.11': 'I know.',
  'story.cs_ar_seal.12': 'You will give it to me. Everyone does, in the end.',
  'story.cs_ar_seal.13': 'The wardens waited a thousand years for someone willing to go. Go, child. May the Abyssfire light your way home.',

  // ─── cs_ar_fall ───
  'story.cs_ar_fall.4': 'The Abyssfire incarnate shatters. When the flames fall away, only an old man remains before the throne.',
  'story.cs_ar_fall.5': '...A thousand years. I only wanted to keep it from going out. The veins were cooling, the world was dying. Could none of you see?',
  'story.cs_ar_fall.7': 'You raise your palm. The five flames light one by one, and in the empty heart the Heartflame takes shape at last.',
  'story.cs_ar_fall.8': 'Aethelyn... so this is where you hid it. Always the same. You would rather give the fire away.',
  'story.cs_ar_fall.9': 'You set the Final Key in the gate and return the Heartflame to the Abyssfire. Not to him. To the world.',
  'story.cs_ar_fall.11': 'The fire does not devour you. It settles like embers in a hearth and sends its warmth along the ley lines to all five lands.',
  'story.cs_ar_fall.12': 'It is... rekindling. Without burning everything... it rekindles. So she was right all along.',
  'story.cs_ar_fall.13': 'Ignaroth crumbles into a wisp of ash and is carried off on the wind. Without a sound, the great gate closes.',
  'story.cs_ar_fall.14': 'The five flames on your palm fade into a ring of silver. At its centre remains one small, warm light.',

  // ─── cs_boss_goblin_chief ───
  'story.cs_boss_goblin_chief.3': 'The brand! That\'s the hand! The fire said whoever cuts off that hand lives forever!',
  'story.cs_boss_goblin_chief.5': 'Don\'t take him seriously. He is only an obedient dog. Still... show me how you deal with dogs.',

  // ─── cs_boss_werewolf_alpha ───
  'story.cs_boss_werewolf_alpha.2': 'A howl tears the night apart. The moon died long ago, yet it still howls at the sky.',
  'story.cs_boss_werewolf_alpha.4': 'Grrh... the moon... no longer answers. Now only that voice... answers me.',
  'story.cs_boss_werewolf_alpha.5': 'Selenne\'s sacred wolf, her most loyal guardian. Loyalty is a fine thing. You need only change the master.',

  // ─── cs_boss_mountain_troll ───
  'story.cs_boss_mountain_troll.3': 'Little thing! This chair is Gorm\'s! There\'s fire singing under the chair, and it sings only for Gorm!',
  'story.cs_boss_mountain_troll.5': 'A troll\'s head holds very little. Just enough room for me.',

  // ─── cs_boss_phoenix ───
  'story.cs_boss_phoenix.2': 'Heat bends the air. A burning bird rises from the rift, and every beat of its wings rains fire.',
  'story.cs_boss_phoenix.4': 'Look at her. Queen Nafsha\'s beloved bird, and now she burns only for me.',
  'story.cs_boss_phoenix.5': 'The sunbird lets out a piercing cry, full of rage, and of pleading.',

  // ─── cs_boss_demon_lord ───
  'story.cs_boss_demon_lord.2': 'Upon the Throne of Chaos, the Abyssfire gathers into a colossal form. A thousand years of whispers finally have a face.',
  'story.cs_boss_demon_lord.4': 'You brought all five lamps, and the Heartflame she hid. Good. Today, everything goes back into the forge.',
  'story.cs_boss_demon_lord.5': 'I will forge a world that never goes out. No cold, no death. No one forgotten, ever again.',
  'story.cs_boss_demon_lord.6': 'This fire isn\'t yours.',
  'story.cs_boss_demon_lord.7': 'Then let the fire judge us, heir of the Flamekeeper!',

  // ─── epilogue ───
  'story.epilogue.1.heading': 'Epilogue',
  'story.epilogue.1.title': 'The Gate Is Closed',
  'story.epilogue.1.text': 'The moment the gate closed, all five lands felt a sudden warmth.\nNot a blaze. More like someone adding wood to the hearth on a winter night.',
  'story.epilogue.2.title': 'The Emerald Plains',
  'story.epilogue.2.text': 'The Seal of Veins glows green again, and wildflowers cover the ruins of the elven tower.\nThe elder has added a new line beneath the old stele: crooked letters, carved deep.',
  'story.epilogue.3.title': 'Forest and Mountain',
  'story.epilogue.3.text': 'The Twilight Forest greets its first morning in centuries; the dead sleep, and wolves sing to the moon again.\nIn the Anvil Mountains the forges burn day and night, and the dwarves have come home to their halls.',
  'story.epilogue.4.title': 'Sand and Abyss',
  'story.epilogue.4.text': 'Helia circles above the oasis, and one by one the dry springs run clear.\nThe Abyss Rift slowly cools, and the wardens lay down a burden they carried for a thousand years.',
  'story.epilogue.5.title': 'The Flamekeeper',
  'story.epilogue.5.text': 'You lived. The brand on your palm faded to a ring of silver,\nwith one small light at its heart: the fire\'s thanks, and a new oath.\nPeople have begun to call you the Flamekeeper.',
  'story.epilogue.6.title': 'Abyssfire',
  'story.epilogue.6.text': 'The Abyssfire never went out.\nIt only returned to the hearth, to warm a world that is flawed, and worth keeping.',

  // ─── credits ───
  'story.credits.1.title': 'Abyssfire',
  'story.credits.1.text': 'A tale of fire, oaths, and the road home',
  'story.credits.2.heading': 'Design & Development',
  'story.credits.2.title': 'feuvan',
  'story.credits.3.heading': 'Technology',
  'story.credits.3.text': 'Built with Phaser 3 · TypeScript · Vite',
  'story.credits.4.heading': 'Special Thanks',
  'story.credits.4.text': 'Every Flamekeeper who made it this far',
  'story.credits.5.title': 'Thank You for Playing',
  'story.credits.6.text': 'Beneath the Abyss lies a deeper dark.\nNightmare difficulty awaits.',
  // ─── cs_tf_moonfang (Volgan's cub) ───
  'story.cs_tf_moonfang.2': 'The wolf king falls, and the Abyssfire in its eyes gutters out. For the first time in a thousand years, it is still.',
  'story.cs_tf_moonfang.3': 'From the ferns beside the body creeps a silver-grey cub, a white crescent on its brow.',
  'story.cs_tf_moonfang.4.title': 'Moonfang',
  'story.cs_tf_moonfang.4.subtitle': 'Son of Volgan',
  'story.cs_tf_moonfang.5': 'It sniffs the brand on your palm, and does not run.',
  'story.cs_tf_moonfang.6': '...Come with me.',
  'story.cs_tf_moonfang.7': 'The cub gives a low whine and falls in at your heel.',
  // ─── cs_sd_helia (Helia's ember) ───
  'story.cs_sd_helia.2': 'The light of the rebirth fades, but a small ember lingers on the sand and will not go out.',
  'story.cs_sd_helia.4': 'A chick peeks out of the ember, its feather tips burning red-gold, and looks up at you shyly.',
  'story.cs_sd_helia.5.title': 'Helia\'s Ember',
  'story.cs_sd_helia.5.subtitle': 'The sunbird\'s last fire',
  'story.cs_sd_helia.7': 'The sunbird left its last spark to you. The old ones say a chick like that chooses one person for life.',
  'story.cs_sd_helia.8': 'The chick flutters onto your shoulder. Its warmth seeps through your clothes, all the way to your palm.',
  // ─── cs_ar_altar (the warden's lamp goes to the tower) ───
  'story.cs_ar_altar.2': 'The essence of the abyss is mine to keep now. It was fire once, and it will return to fire.',
  'story.cs_ar_altar.3': 'I cannot guard the Heartflame for you. But its warmth, that I can keep.',
  'story.cs_ar_altar.4': 'The warden hands her silver lamp to a messenger, bound for the Ember Tower, to light the altar that has lain cold for a thousand years.',
  'story.cs_ar_altar.5': 'Offer embers when you are home, and the altar will guard you for a while. It is all Aethelyn\'s blood can give you.',
  // ─── cs_tower_home (first visit to the Ember Tower) ───
  'story.cs_tower_home.2.title': 'The Ember Tower',
  'story.cs_tower_home.2.subtitle': 'The Flamekeeper\'s hearth',
  'story.cs_tower_home.3': 'You woke in these ashes. Back then the tower lay broken: scorched steps and cold grey ash.',
  'story.cs_tower_home.5': 'You\'re back, child. This old man came ahead to warm the place up for everyone.',
  'story.cs_tower_home.6': 'This place was ashes once. Now it is your home.',
  'story.cs_tower_home.7': 'Every seal you light, someone will follow the glow here and restore a wing.',
  'story.cs_tower_home.8': 'Beneath the tower, the ley lines\' warmth stirs, as if in answer.',
};
