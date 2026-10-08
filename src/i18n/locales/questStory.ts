/**
 * Quest story lines — what the quest giver says when offering a quest
 * (`data.quest.<id>.offer`) and when the player turns it in (`.complete`).
 *
 * zh-CN is the source of truth (zh-TW is auto-converted from it).
 */
import type { LocaleData } from '../types';

/** NPC lines for each quest: `.offer` (when offering it) and `.complete` (when the player turns it in). */
export const QUEST_STORY_ZH: LocaleData = {
  // ─── Zone 1: 翡翠平原 — 村长 (main chain) ───
  'data.quest.q_kill_slimes.offer': '你在灰烬里睡了三天，总算醒了。那夜火柱冲天之后，北边湿地的史莱姆就疯长起来。帮老头子清掉十只吧，当心酸液。',
  'data.quest.q_kill_slimes.complete': '田里清静了，谢谢你。……等等，把手伸过来。你掌心这道烙印，我在精灵石碑上见过。',
  'data.quest.q_kill_goblins.offer': '哥布林昨夜又摸进村子了。怪的是，它们眼里都烧着和你烙印一样的火光。去南边狠狠教训它们，也看看那火是哪来的。',
  'data.quest.q_kill_goblins.complete': '它们退了，可眼里的火没灭。猎户说南边深处有座营地，萨满夜夜对着地底跳火。',
  'data.quest.q_explore_goblin_camp.offer': '营地在平原南边深处，篝火七天七夜没灭过，萨满对着地底念咒。去摸摸底，看他们在唤醒什么。千万别逞强。',
  'data.quest.q_explore_goblin_camp.complete': '图腾上刻着灵脉的纹路……它们想撬开平原底下的封印！首领就是那只撬棍。',
  'data.quest.q_find_goblin_chief.offer': '首领叫格罗克，藏在平原西南深处。它向部落许诺，砍下“烙印之手”就能得到火的恩赐。孩子，它要的是你。活着回来。',
  'data.quest.q_find_goblin_chief.complete': '格罗克倒了，部落散了！可我这心还悬着：灵脉还在地底发抖。',
  'data.quest.q_secure_plains.offer': '灵脉还在发抖，我脚底都感觉得到。去东边和南边走一趟吧。碑上说，烙印者所到之处，灵脉自会安宁。',
  'data.quest.q_secure_plains.complete': '地底的颤动停了。第一道封印稳住了……孩子，你手上的火，好像亮了一点。',
  // ─── Zone 1: 翡翠平原 — 支线 ───
  'data.quest.q_collect_slime_gel.offer': '药师调伤药缺史莱姆凝胶，她那小摊又离不开人。你打史莱姆时收六份凝胶，亲手给她送去吧。',
  'data.quest.q_collect_slime_gel.complete': '药师托人捎话来了，说凝胶成色好，伤药够用一冬。这几瓶是她谢你的。',
  'data.quest.q_herb_gathering.offer': '伤员躺了一屋子，草药早就见底了。西边草坡上的翡翠草药泛着微光，采五株回来，救人要紧。',
  'data.quest.q_herb_gathering.complete': '有了这些草药，今晚就不会再有人咽气了。你这孩子，心肠真好。',
  'data.quest.q_lost_pendant.offer': '昨夜有只小个子哥布林钻进村，把王婆婆的传家挂坠偷走了。它跑得慌，一路留了不少痕迹，从篱笆那儿追起吧。',
  'data.quest.q_lost_pendant.complete': '就是这枚！婆婆攥着它又哭又笑。那小贼叫斯尼克？哼，平原上再也听不到这名字了。',
  'data.quest.q_escort_merchant_plains.offer': '有位商人要去南方营地，可一路上全是哥布林。村里的补给全指望他了，你陪他走一趟，别让他出事。',
  'data.quest.q_escort_merchant_plains.complete': '商人平安到了，还特地托人捎话谢你。一路辛苦了。',
  'data.quest.q_pet_sprite_friend.offer': '东北池塘边住着个小精灵，最爱捉迷藏。它说谁能找到它四次，就跟谁做朋友。留神草丛里一闪一闪的光。',
  'data.quest.q_pet_sprite_friend.complete': '瞧，那小家伙赖在你肩头不肯走了。捉迷藏输给你，它倒是服气得很。好好待它。',

  // ─── Zone 1: 翡翠平原 — 流浪剑客 ───
  'data.quest.q_bandit_trouble.offer': '悬赏令是我亲手贴的：红帽格鲁克，东南土坡上的劫匪头子。我这把钝剑追不上他了……把他的红帽带回来。',
  'data.quest.q_bandit_trouble.complete': '红帽……沾着血，也沾着车夫们的眼泪。赏金是你的。那条路，今晚能睡个安稳觉了。',

  // ─── Zone 2: 暮色森林 — 侦察兵 (main chain) ───
  'data.quest.q_explore_forest.offer': '你就是平原来的烙印者？村长的信到了。森林不对劲，北部密林、废弃墓地、古老遗迹，三处都去看看。看完就回。',
  'data.quest.q_explore_forest.complete': '墓地的亡灵最多，而且都朝着同一个方向跪着——东边。比我想的还糟。',
  'data.quest.q_kill_undead.offer': '骷髅和腐尸断了所有巡逻线。骷髅十二，腐尸八，送它们回土里。它们生前也是人，别让它们再受折磨。',
  'data.quest.q_kill_undead.complete': '干得利落。可死人还在往外爬。东边住着个隐士，比这林子里的树还老，也许他知道缘由。',
  'data.quest.q_talk_hermit.offer': '隐士住在森林最东边，脾气古怪，不见外人。可他说过：若有人手上带着火来，就放他进门。去吧。',
  'data.quest.q_talk_hermit.complete': '隐士肯见你，说明你手上的火是真的。坐下，喝口水，慢慢说。',
  'data.quest.q_kill_werewolf_alpha.offer': '隐士说狼王曾是月之祭司的圣狼，如今带着狼群守着黑暗之源的路。先宰六头狼人逼它现身，再……给它个痛快。',
  'data.quest.q_kill_werewolf_alpha.complete': '狼王倒下时朝着天空嚎了最后一声，不像恨，倒像道谢。路开了。',
  'data.quest.q_seal_dark_source.offer': '黑暗之源在西南角，就是被污染的月辉之印，腐尸正从那里源源不断地爬出来。杀穿尸群，把你的火按进去。',
  'data.quest.q_seal_dark_source.complete': '林子里……亮了？几百年了，我头一回看见月光落地。',
  'data.quest.q_defend_camp_forest.offer': '情报确认，亡灵今夜攻营，分三波。篝火一灭，营地就完了。守住它。',
  'data.quest.q_defend_camp_forest.complete': '三波都挡住了，篝火还亮着。这一营人的命，是你保下的。',

  // ─── Zone 2: 暮色森林 — 森林猎人 ───
  'data.quest.q_collect_wolf_pelts.offer': '嘘……灰鬃，独眼，住在西南狼穴。三个冬天，它咬死了我两个徒弟。我老了，追不动了。替我剥了它的皮。',
  'data.quest.q_collect_wolf_pelts.complete': '是它的皮，左眼那道疤我认得。……两个孩子，可以闭眼了。',
  'data.quest.q_lost_scout.offer': '三天前派出去的斥候没回来。我在营地东南找到他的脚印，一路往坡下去了。去找他……不管找到的是什么。',
  'data.quest.q_lost_scout.complete': '队长把徽章给我看了。裂颚也死了。他是个好斥候，箭射光了才倒下……谢谢你把他带回来。',

  // ─── Zone 2: 暮色森林 — 通灵巫女 ───
  'data.quest.q_spider_nest.offer': '墓地里的亡魂不是要害人……它们只是迷了路，找不到回去的方向。去点亮那些魂灯吧，为它们照一条路。',
  'data.quest.q_spider_nest.complete': '我看见了……一盏接一盏，它们顺着灯火走了。墓地从没这么安静过。',
  'data.quest.q_investigate_corruption_forest.offer': '腐化不止一处……树根、水源、暗影祭坛，三股气息都在喂养同一个东西。顺着它们找下去，把那个源头除掉。',
  'data.quest.q_investigate_corruption_forest.complete': '那个母体死了，林子的呼吸顺畅了些。可它不是自己长出来的……是有人在召唤。',
  'data.quest.q_ancient_relic.offer': '亡者托我带一句话给隐士：“月没有熄，它在等火。”我走不出这片墓地……替我送去吧，他也会有话要你转告。',
  'data.quest.q_ancient_relic.complete': '隐士的话，侦察兵队长也听到了。亡者、隐士、活人……这片林子总算又连在一起了。',

  // ─── Zone 3: 铁砧山脉 — 矮人长老 (main chain) ───
  'data.quest.q_explore_dwarf_ruins.offer': '哼，又一个说能救山的。不过你手上的火，先祖的碑文里提过。矿洞入口、锻造大厅、王座，去看看，回来跟我说。',
  'data.quest.q_explore_dwarf_ruins.complete': '石像鬼占了高处，巨魔占了王座——那王座底下压着的，是铁砧之印。先祖在上！',
  'data.quest.q_kill_gargoyles.offer': '那些石翼畜生是百年前那场地震惊醒的。可那不是地震，是有东西在山底下翻身。砸碎十五只，把路打通！',
  'data.quest.q_kill_gargoyles.complete': '痛快！高处清净了。现在把先祖散落的遗物找回来——我要重铸一件东西。',
  'data.quest.q_collect_dwarf_relics.offer': '布鲁恩王的秘银锭进了石魔像的肚子，符文碎片被石像鬼叼得满山都是。给我抢回来，一件都不能少！',
  'data.quest.q_collect_dwarf_relics.complete': '秘银、符文……齐了一半。孩子，你知道我要重铸的是什么吗？命运之锤。',
  'data.quest.q_reforge_artifact.offer': '命运之锤——当年锻出五道封印的就是它！只差秘银核心了，巨魔和石魔像体内会结出这东西。取三颗来！',
  'data.quest.q_reforge_artifact.complete': '锤心在跳……不，是跟着你的烙印在跳！快，站到炉前来。',
  'data.quest.q_kill_stone_guardian.offer': '戈尔姆那头蠢巨魔赖在王座上不走——王座底下的铁砧之印在对它低语。把它轰下来，用神锤和你的火重燃封印！',
  'data.quest.q_kill_stone_guardian.complete': '王座夺回来了！矮人会记你一辈子。只是南边沙海的烟柱，又高了。',
  'data.quest.q_dragon_egg.offer': '哼，龙蛋？矿工喝多了麦酒什么都敢说。可北峰那窝石像鬼确实护着个东西不放……去把守巢的宰了，把那颗“蛋”拿给符文学者瞧瞧。',
  'data.quest.q_dragon_egg.complete': '符文学者说了，是石像鬼的卵，石头壳里还在跳。哼，我就说没有龙！……不过这东西，最好别让它孵出来。',
  'data.quest.q_craft_dwarf_weapon.offer': '我要重铸先祖卫队的战锤！秘银锭找石巨人，符文碎片找石像鬼，凑齐了去高级铁匠那儿锻造，再拿来给我。',
  'data.quest.q_craft_dwarf_weapon.complete': '就是这个分量！先祖卫队的战锤回到矮人手里了。这把好兵器赏你，别给我丢人。',

  // ─── Zone 3: 铁砧山脉 — 矿工老汉 ───
  'data.quest.q_crystal_mining.offer': '咳咳……北坡塌出一条水晶矿脉，亮得晃眼。可石像鬼就在头顶打转，老汉我这腿爬不上去。你手脚麻利，趁它们不注意凿六块下来。',
  'data.quest.q_crystal_mining.complete': '好水晶！拿去附魔，刀刃都能多三分锋利。咳，这块蓝宝石你收着。',
  'data.quest.q_trapped_miners.offer': '南边矿道塌了！小石头压伤了腿，困在石魔像出没的地方。求你了，把他护送回来！',
  'data.quest.q_trapped_miners.complete': '小石头回来了，腿也保住了……咳咳，这份恩情，老汉我一辈子忘不了。',
  'data.quest.q_pet_jade_tortoise.offer': '老辈人讲，山里睡着一只远古玄武的魂。古神龛边散着它的魂片，拾齐了，守魂像就会醒来考你。赢了，它就跟你走。',
  'data.quest.q_pet_jade_tortoise.complete': '守魂像碎了，玄武倒像是笑了——你看它背上的玉光！往后它就跟着你了，好好待它。',

  // ─── Zone 3: 铁砧山脉 — 符文学者 ───
  'data.quest.q_mountain_bandits.offer': '碎岩者！一尊失控的远古石魔像，已经砸塌三座矿井了。它核心里可能刻着最后一段完整的守护铭文——请打碎它，但千万别打碎核心！',
  'data.quest.q_mountain_bandits.complete': '核心完好！看这符文……它不是失控，是被人改写了：“护山”被刻成了“碎山”。有意思，也很可怕。',
  'data.quest.q_investigate_ruins_mountains.offer': '山里藏着四块符文石碑，分别在北方、东方、矿洞和山顶。请帮我找到它们，把铭文记下来。',
  'data.quest.q_investigate_ruins_mountains.complete': '四段铭文拼上了！这是一段封印咒文……矮人当年在封印什么？太迷人了。',

  // ─── Zone 4: 灼热沙漠 — 沙漠游牧民 (main chain) ───
  'data.quest.q_explore_desert.offer': '沙会说话，只是外乡人听不懂。可你手上的火，沙子认得。去烈焰荒地和蝎谷走一走，认一认这片沙海的脸。',
  'data.quest.q_explore_desert.complete': '你带回了风的消息。荒地里的火是从地底往上烧的，像一口永不熄灭的炉。',
  'data.quest.q_kill_fire_elementals.offer': '火焰元素是裂隙里漏出来的渊火碎屑，走过之处沙子都烧成了琉璃。扑灭十二团，让沙海喘口气。',
  'data.quest.q_kill_fire_elementals.complete': '火势小了。老人们说绿洲的泉水记得王国的往事，也许它能告诉你，火从哪里来。',
  'data.quest.q_explore_oasis.offer': '绿洲是沙海藏起来的一滴眼泪，曾是女王的花园。它在北边的沙丘间，去找到它，听听泉水怎么说。',
  'data.quest.q_explore_oasis.complete': '你在泉边昏睡了半日，醒来时眼里还映着火。告诉我，你看见了什么？',
  'data.quest.q_kill_sandworms.offer': '沙虫被地底的热逼上了地面，一整支驼队就这么没了。杀八条，给我们，也给你，开一条去裂隙的路。',
  'data.quest.q_kill_sandworms.complete': '沙面静下来了。只剩最南边的裂隙，和守着它的赫莉娅——女王的圣鸟。',
  'data.quest.q_seal_fire_rift.offer': '赫莉娅守着裂隙，死了也会浴火重生。别恨她，她只是被火绑住了。放她自由，让新生的火把裂隙合上。',
  'data.quest.q_seal_fire_rift.complete': '裂隙合上了，今夜的风是凉的。你手上已经亮了四团火……最后一处在哪，你心里清楚。',
  'data.quest.q_scorpion_venom.offer': '死去的驼队身上都开着同一种紫黑色的花，那是赛丝的毒。蝎群之母住在东边的岩谷。割下她的毒囊——毒能杀人，也能救人。',
  'data.quest.q_scorpion_venom.complete': '毒囊还在跳。沙漠里的东西都这样，越致命，越珍贵。今晚我就熬解药，驼队可以再上路了。',
  'data.quest.q_escort_survivor_desert.offer': '蝎谷有个受伤的探险者，撑不过下一次日落。带他穿过蝎谷回绿洲营地，蝎子和火灵可不会客气。',
  'data.quest.q_escort_survivor_desert.complete': '他活下来了。沙海今天少吞了一个人，这是你的功劳。',
  'data.quest.q_craft_fire_ward.offer': '我要一枚挡烈日的火焰护符。带三份净化水囊、三份蝎毒去沙漠商人那儿制成护符，再拿来给我。',
  'data.quest.q_craft_fire_ward.complete': '护符凉得像井水。有它在身上，我就能走进烈焰荒地了。谢谢你，朋友。',

  // ─── Zone 4: 灼热沙漠 — 沙漠考古学家 ───
  'data.quest.q_buried_treasure.offer': '看这石板！古字，完整的句子——可惜碎了，碎片全散在遗迹周围的沙里。帮我找齐，拿给游牧民首领，只有他还认得这种字！',
  'data.quest.q_buried_treasure.complete': '游牧民念出来了，是日冕女王的遗令：“把火还给炉膛。”……这句话，我好像在哪儿听过。这块石板值得我再挖十年！',

  // ─── Zone 4: 灼热沙漠 — 寻水者 ───
  'data.quest.q_water_supply.offer': '我的杖在西边沙地里颤个不停，下面有泉。跟着杖尖走，三处湿沙，泉在最后一处。只是……水声里还夹着别的东西在喘气。',
  'data.quest.q_water_supply.complete': '吞泉者死了，泉水涌上来了。我听得见，它在笑。今晚营地里的孩子，不用再舔露水了。',
  'data.quest.q_mirage_beasts.offer': '北边热浪里那两团火光不是幻觉，是一对共生的火灵，一昼一夜，形影不离。走进蜃景，把它们一起熄灭。',
  'data.quest.q_mirage_beasts.complete': '热浪里再没有那两团火了。被引走的旅人回不来了……可往后，不会再有人被引走。',

  // ─── Zone 5: 深渊裂隙 — 深渊守望者 (main chain) ───
  'data.quest.q_explore_abyss.offer': '烙印者……守望者等了你一千年。裂隙入口、恶魔尖塔、混沌王座，去看清敌人的阵地。活着回来，才算数。',
  'data.quest.q_explore_abyss.complete': '你回来了，很少有人做到。你的脸色告诉我，你已经见过那扇门了。',
  'data.quest.q_kill_demons.offer': '门缝每宽一寸，就多涌出一群恶魔。小恶魔二十，次级恶魔十。数字不重要，别让一只越过裂隙。',
  'data.quest.q_kill_demons.complete': '它们退了一步，只是一步。要锻终焉之钥，得用它们自己的精华作引。',
  'data.quest.q_collect_demon_essence.offer': '以渊火之物锁渊火之门，这是艾瑟琳留下的法子。小恶魔、次级恶魔、魅魔身上都有精华，十五份。去收。',
  'data.quest.q_collect_demon_essence.complete': '精华够了。最后一步，把守护者们用命换来的封印碎片锻成钥匙。',
  'data.quest.q_forge_seal.offer': '碎片散在英雄纪念碑附近，每一片都是我同袍的命。捡回五片，用你带来的命运之锤，锻出终焉之钥。',
  'data.quest.q_forge_seal.complete': '钥匙成了。可它还缺一样东西……你心里已经明白了，对吗？',
  'data.quest.q_kill_abyss_lord.offer': '他就在混沌王座上，等着你，也等着你的火。拿上钥匙去吧。守望者等了一千年，别让我们白等。',
  'data.quest.q_kill_abyss_lord.complete': '门合上了。渊火没有熄，它回到了炉膛里。回家吧，守火人。',
  'data.quest.q_demon_weaponry.offer': '恶魔的兵器都出自南边的军械营，看营的是军械官莫拉克斯。杀了他，把兵符带回来。读懂它们的调兵令，我们就能抢先一步。',
  'data.quest.q_demon_weaponry.complete': '兵符上的调令……它们要冲击封印石。好，现在我们知道它们会从哪来了。',
  'data.quest.q_defend_seal_abyss.offer': '恶魔军团在冲击封印石，五波。封印石一碎，深渊就会再次扩张。守住，不许退。',
  'data.quest.q_defend_seal_abyss.complete': '封印石还立着。你也还站着。够了。',

  // ─── Zone 5: 深渊裂隙 — 堕落骑士 ───
  'data.quest.q_corrupted_souls.offer': '卡隆、莉丝、吉格……我的三个部下，向恶魔献了誓。我教会他们握剑，却没教会他们守誓。我下不了手……求你，替我了结他们。',
  'data.quest.q_corrupted_souls.complete': '三个都……走了吗。他们最后可曾说什么？……不，别告诉我。他们背弃的誓，由我来背。',
  'data.quest.q_fallen_hero.offer': '神殿和纪念碑上各刻着半句遗言，是三百年前封印深渊的人留下的。我读不了……那些字会在我眼前烧起来。拓下来，交给守望者吧。',
  'data.quest.q_fallen_hero.complete': '守望者念给我听了：“以心为炉。”……原来他们早就知道。也许，我还配握剑。',

  // ─── Zone 5: 深渊裂隙 — 虚空研究者 ───
  'data.quest.q_void_crystals.offer': '裂隙边长出了会“呼吸”的结晶！一呼一吸，空间就皱一下。趁它们还没长成下一道裂口，采五块回来——要活的！',
  'data.quest.q_void_crystals.complete': '看，还在呼吸！频率和裂隙完全同步……我敢打赌，那扇门的心跳就藏在这里面。剩下的交给我。',
  // ─── Ley-beast quests (灵兽) ───
  'data.quest.q_pet_owl.offer': '月印碎的那夜，我的老伙计月鸮吓得飞走了。它掉的羽毛会映月光，你顺着找找，它多半躲在哪棵空心老树里。',
  'data.quest.q_pet_owl.complete': '是它！还是这么爱瞪眼。它认准你了，就跟你去吧。',
  'data.quest.q_pet_cat.offer': '有只猫的影子被亡灵拖进了生死之间，夜夜在我耳边叫。夺回它的影子碎片，再打倒看守它的缚影者。',
  'data.quest.q_pet_cat.complete': '影子回来了，它却不肯回阴间，只肯跟着你。随它吧。',
  'data.quest.q_pet_dragon.offer': '篡座者倒下后，熔炉底下滚出一枚热乎乎的龙蛋！要孵它得有新火。去石魔心口取三团熔炉余烬，送进铁匠的炉子。',
  'data.quest.q_pet_dragon.complete': '炉火一旺，壳就裂了！瞧这小家伙，一张嘴就喷火星子。它是你的了。',
};

export const QUEST_STORY_EN: LocaleData = {
  // ─── Zone 1: Emerald Plains — Village Elder (main chain) ───
  'data.quest.q_kill_slimes.offer': 'Three days you slept in those ashes, and now you\'re awake. Since the fire pillar, the slimes up in the northern wetlands have run wild. Clear ten for an old man, and mind their acid.',
  'data.quest.q_kill_slimes.complete': 'The fields are quiet, thank you. Wait... give me your hand. That brand on your palm. I\'ve seen it on the elven stele.',
  'data.quest.q_kill_goblins.offer': 'Goblins crept into the village again last night. Strange thing: their eyes burn with the same fire as your brand. Go south, teach them a lesson, and learn where that fire comes from.',
  'data.quest.q_kill_goblins.complete': 'They fell back, but the fire in their eyes hasn\'t gone out. The hunters say there\'s a camp deep in the south where a shaman dances over the fire every night.',
  'data.quest.q_explore_goblin_camp.offer': 'Their camp lies deep in the southern plains. The bonfires haven\'t died in seven days, and the shaman chants at the ground. Scout it out, see what they\'re waking. No heroics.',
  'data.quest.q_explore_goblin_camp.complete': 'Ley-line patterns carved on the totems... they mean to pry open the seal under the plains! And the chief is the crowbar.',
  'data.quest.q_find_goblin_chief.offer': 'The chief is called Grokk, hiding deep in the southwest. He\'s promised his tribe the fire\'s blessing for the "branded hand". Child, he wants you. Come back alive.',
  'data.quest.q_find_goblin_chief.complete': 'Grokk is dead and the tribe has scattered! But my heart\'s still in my throat. The ley lines are trembling underground.',
  'data.quest.q_secure_plains.offer': 'The ley lines are still shaking. I can feel it through my boots. Walk the east and the south. The stele says the veins grow calm wherever the brand-bearer treads.',
  'data.quest.q_secure_plains.complete': 'The trembling underground has stopped. The first seal holds... Child, the fire in your hand looks a little brighter.',
  // ─── Zone 1: Emerald Plains — side quests ───
  'data.quest.q_collect_slime_gel.offer': 'The herbalist is short of slime gel for her salves, and she can\'t leave her stall. Collect six portions from the slimes and take them to her yourself.',
  'data.quest.q_collect_slime_gel.complete': 'The herbalist sent word: fine gel, enough salve for the whole winter. These potions are her thanks.',
  'data.quest.q_herb_gathering.offer': 'The wounded fill every bed and the herbs ran out days ago. Emerald herbs glow faintly on the western slope. Pick five, and hurry.',
  'data.quest.q_herb_gathering.complete': 'With these, no one else dies tonight. You have a good heart, child.',
  'data.quest.q_lost_pendant.offer': 'Last night a little goblin slipped into the village and made off with Granny Wang\'s heirloom pendant. It ran in a panic and left a messy trail. Start at the fence.',
  'data.quest.q_lost_pendant.complete': 'That\'s the one! Granny is laughing and crying at once. Sneek, was it? Well, the plains won\'t hear that name again.',
  'data.quest.q_escort_merchant_plains.offer': 'A merchant must reach the southern camp, but the road crawls with goblins. Our supplies depend on him. Go with him and keep him safe.',
  'data.quest.q_escort_merchant_plains.complete': 'The merchant arrived safely and sent his thanks. It was a hard road.',
  'data.quest.q_pet_sprite_friend.offer': 'A little sprite lives by the northeastern pond, and it adores hide-and-seek. Find it four times and it will be your friend. Watch for twinkling in the grass.',
  'data.quest.q_pet_sprite_friend.complete': 'Look, the little thing won\'t leave your shoulder. It lost fair and square, and it seems rather proud of you. Be good to it.',

  // ─── Zone 1: Emerald Plains — Wandering Swordsman ───
  'data.quest.q_bandit_trouble.offer': 'I posted that bounty myself: Gruk Redcap, bandit boss of the southeastern ridge. This dull blade of mine can\'t catch him anymore... bring me his red cap.',
  'data.quest.q_bandit_trouble.complete': 'The red cap... stained with blood, and with the tears of every wagoner he robbed. The bounty is yours. That road sleeps easy tonight.',

  // ─── Zone 2: Twilight Forest — Scout (main chain) ───
  'data.quest.q_explore_forest.offer': 'So you\'re the brand-bearer from the plains? The elder\'s letter came. The forest is wrong. Northern woods, old graveyard, ancient ruins: look at all three, then come back.',
  'data.quest.q_explore_forest.complete': 'The graveyard is the worst, and the dead there all kneel facing the same way: east. Worse than I thought.',
  'data.quest.q_kill_undead.offer': 'Skeletons and ghouls have cut every patrol route. Twelve skeletons, eight ghouls. Put them back in the ground. They were people once. Don\'t let them suffer more.',
  'data.quest.q_kill_undead.complete': 'Clean work. But the dead keep crawling out. A hermit lives in the east, older than the trees. He may know why.',
  'data.quest.q_talk_hermit.offer': 'The hermit lives at the far east of the forest. Odd man, shuns strangers. But he once said: if someone comes with fire in their hand, let them in. Go.',
  'data.quest.q_talk_hermit.complete': 'If the hermit let you in, the fire in your hand is real. Sit, have some water, and tell it slowly.',
  'data.quest.q_kill_werewolf_alpha.offer': 'The hermit says the wolf king was the Moon Priestess\'s sacred wolf. Now it guards the road to the dark source. Kill six werewolves to draw it out, then... make it quick.',
  'data.quest.q_kill_werewolf_alpha.complete': 'When the wolf king fell, it howled once more at the sky. Not in hate. More like thanks. The road is open.',
  'data.quest.q_seal_dark_source.offer': 'The dark source is in the southwest corner: the fouled Moon Seal. Ghouls pour out of it without end. Cut through them and press your fire into it.',
  'data.quest.q_seal_dark_source.complete': 'The forest is... bright? Centuries, and this is the first moonlight I\'ve seen touch the ground.',
  'data.quest.q_defend_camp_forest.offer': 'Confirmed: the dead hit the camp tonight, three waves. If the campfire dies, the camp dies. Hold it.',
  'data.quest.q_defend_camp_forest.complete': 'Three waves held, fire still burning. Every soul in this camp owes you.',

  // ─── Zone 2: Twilight Forest — Forest Hunter ───
  'data.quest.q_collect_wolf_pelts.offer': 'Shh... Greymane. One eye. The den to the southwest. Three winters, and it killed two of my apprentices. I\'m too old to run it down now. Skin it for me.',
  'data.quest.q_collect_wolf_pelts.complete': 'That\'s its hide; I know the scar over that eye... My two boys can close their eyes now.',
  'data.quest.q_lost_scout.offer': 'The scout we sent out three days ago never came back. I found his tracks southeast of camp, heading downhill. Go find him... whatever you find.',
  'data.quest.q_lost_scout.complete': 'The captain showed me the badge. Rendjaw is dead too. He was a good scout; he didn\'t fall until his quiver was empty... Thank you for bringing him home.',

  // ─── Zone 2: Twilight Forest — Spirit Medium ───
  'data.quest.q_spider_nest.offer': 'The ghosts in the graveyard mean no harm... they are only lost, and cannot find the way back. Light the soul lanterns and show them the path.',
  'data.quest.q_spider_nest.complete': 'I saw them... lantern after lantern, they followed the light home. The graveyard has never been so quiet.',
  'data.quest.q_investigate_corruption_forest.offer': 'The corruption is in three places... the roots, the water, a shadow altar, and all three feed the same thing. Follow them to it, and root it out.',
  'data.quest.q_investigate_corruption_forest.complete': 'The Rot Mother is dead, and the forest breathes easier. But it did not grow on its own... someone summoned it.',
  'data.quest.q_ancient_relic.offer': 'The dead gave me words for the hermit: "The moon has not gone out; it waits for the fire." I cannot leave this graveyard... carry them for me. He will have words of his own for you to pass on.',
  'data.quest.q_ancient_relic.complete': 'The hermit\'s words have reached the scout captain too. The dead, the hermit, the living... this forest is bound together again.',

  // ─── Zone 3: Anvil Mountains — Dwarven Elder (main chain) ───
  'data.quest.q_explore_dwarf_ruins.offer': 'Hmph. Another one who says they\'ll save the mountain. Still, the fire in your hand is in our ancestors\' inscriptions. Mine entrance, forge hall, throne. Go look, then report.',
  'data.quest.q_explore_dwarf_ruins.complete': 'Gargoyles on the heights, a troll on the throne, and beneath that throne lies the Anvil Seal. By my ancestors!',
  'data.quest.q_kill_gargoyles.offer': 'Those stone-winged vermin woke in the quake a hundred years ago. Only it was no quake: something turned over beneath the mountain. Smash fifteen and clear the road!',
  'data.quest.q_kill_gargoyles.complete': 'Ha! The heights are clear. Now bring back our ancestors\' scattered relics. There\'s something I mean to reforge.',
  'data.quest.q_collect_dwarf_relics.offer': 'King Bruun\'s mithril ingots went down the golems\' gullets, and the gargoyles carried his rune fragments all over the mountain. Take them back. Every last one!',
  'data.quest.q_collect_dwarf_relics.complete': 'Mithril, runes... half of what we need. Child, do you know what I mean to reforge? The Hammer of Fate.',
  'data.quest.q_reforge_artifact.offer': 'The Hammer of Fate: the very hammer that forged the five seals! All it lacks is mithril cores. They grow inside trolls and golems. Bring me three!',
  'data.quest.q_reforge_artifact.complete': 'The hammer\'s heart is beating... no, it\'s beating in time with your brand! Quick, stand at the forge.',
  'data.quest.q_kill_stone_guardian.offer': 'That dim-witted troll Gorm won\'t budge from our throne. The Anvil Seal beneath it is whispering to him. Knock him off and rekindle the seal with the Hammer and your fire!',
  'data.quest.q_kill_stone_guardian.complete': 'The throne is ours again! No dwarf will ever forget you. But the smoke over the southern desert has grown taller.',
  'data.quest.q_dragon_egg.offer': 'Hmph, a dragon egg? Miners will say anything after enough ale. But that gargoyle roost on the north peak is guarding something fiercely... kill the brood mother and take the "egg" to the rune scholar.',
  'data.quest.q_dragon_egg.complete': 'The scholar says it\'s a gargoyle egg, and something is still beating inside the stone. Hah, I said there was no dragon! ...Still, better that thing never hatches.',
  'data.quest.q_craft_dwarf_weapon.offer': 'I will reforge the warhammer of my ancestors\' guard! Ingots from golems, rune fragments from gargoyles. Take them to the master smith, then bring it to me.',
  'data.quest.q_craft_dwarf_weapon.complete': 'That\'s the weight! The old guard\'s hammer is back in dwarven hands. Take this fine weapon, and don\'t shame it.',

  // ─── Zone 3: Anvil Mountains — Old Miner ───
  'data.quest.q_crystal_mining.offer': '*cough* A crystal vein\'s broken open on the north slope, bright enough to blind you. But gargoyles circle right over it, and my old legs won\'t make the climb. You\'re quick. Chip off six while they\'re not looking.',
  'data.quest.q_crystal_mining.complete': 'Fine crystals! Enchant a blade with these and it\'ll bite deeper. *cough* Keep this sapphire for your trouble.',
  'data.quest.q_trapped_miners.offer': 'The southern tunnel\'s caved in! Young Pebble\'s got a crushed leg and he\'s stuck where the golems roam. Please, get him out and bring him home safe!',
  'data.quest.q_trapped_miners.complete': 'Pebble\'s back, and the leg will mend... *cough* I\'ll not forget this as long as I live.',
  'data.quest.q_pet_jade_tortoise.offer': 'The old folk say the soul of an ancient tortoise sleeps in these mountains. Its soul shards lie scattered by the old shrine. Gather them, and its guardian statue will wake to test you. Win, and it\'s yours.',
  'data.quest.q_pet_jade_tortoise.complete': 'The guardian crumbled, and the tortoise... smiled? Look at the jade glow on its shell! It follows you now. Treat it well.',

  // ─── Zone 3: Anvil Mountains — Rune Scholar ───
  'data.quest.q_mountain_bandits.offer': 'Rockbreaker! A rogue golem of ancient make, and it has already brought down three mine shafts. Its core may hold the last intact guardian inscription. Please smash it, but do not smash the core!',
  'data.quest.q_mountain_bandits.complete': 'The core is intact! Look at these runes... it didn\'t go rogue, it was rewritten: "guard the mountain" carved over into "break the mountain". Fascinating. And terrifying.',
  'data.quest.q_investigate_ruins_mountains.offer': 'Four rune steles are hidden in these mountains: north, east, in the mine, and on the summit. Find them and record the inscriptions.',
  'data.quest.q_investigate_ruins_mountains.complete': 'The four pieces fit! It\'s a sealing incantation... what were the dwarves sealing? Fascinating.',

  // ─── Zone 4: Scorching Desert — Desert Nomad (main chain) ───
  'data.quest.q_explore_desert.offer': 'The sand speaks, but strangers cannot hear it. The fire in your hand, though, the sand knows. Walk the Fire Wastes and Scorpion Valley. Learn the face of this desert.',
  'data.quest.q_explore_desert.complete': 'You bring word from the wind. The fire in the wastes burns upward from below, like a furnace that never goes out.',
  'data.quest.q_kill_fire_elementals.offer': 'The fire elementals are shards of Abyssfire leaking from the rift. Sand turns to glass where they walk. Snuff out twelve and let the desert breathe.',
  'data.quest.q_kill_fire_elementals.complete': 'The flames are weaker. The elders say the oasis spring remembers the kingdom. Perhaps it can tell you where the fire comes from.',
  'data.quest.q_explore_oasis.offer': 'The oasis is a tear the desert keeps hidden. Once it was the queen\'s garden. It lies among the northern dunes. Find it, and listen to what the spring says.',
  'data.quest.q_explore_oasis.complete': 'You slept half a day by the spring, and woke with fire still in your eyes. Tell me what you saw.',
  'data.quest.q_kill_sandworms.offer': 'The heat below drove the sandworms to the surface, and a whole caravan vanished. Kill eight and open a road to the rift, for us and for you.',
  'data.quest.q_kill_sandworms.complete': 'The sand lies still. Only the rift in the far south remains, and Helia who guards it: the queen\'s sacred bird.',
  'data.quest.q_seal_fire_rift.offer': 'Helia guards the rift and rises again from her own ashes. Don\'t hate her; the fire has bound her. Set her free, and let her new flame close the rift.',
  'data.quest.q_seal_fire_rift.complete': 'The rift is closed, and tonight the wind is cool. Four fires burn in your hand now... you know where the last one is.',
  'data.quest.q_scorpion_venom.offer': 'Every dead caravan bears the same purple-black blossoms: the mark of Syss. The mother of scorpions dwells in the eastern valley. Cut out her venom sac. Venom kills, and venom heals.',
  'data.quest.q_scorpion_venom.complete': 'The sac still pulses. So it is in the desert: the deadlier, the more precious. Tonight I brew the antidote, and the caravans ride again.',
  'data.quest.q_escort_survivor_desert.offer': 'A wounded explorer lies in Scorpion Valley; he won\'t last another sunset. Lead him through to the oasis camp. The scorpions and flames will show no mercy.',
  'data.quest.q_escort_survivor_desert.complete': 'He lives. The desert swallowed one fewer today, thanks to you.',
  'data.quest.q_craft_fire_ward.offer': 'I need a fire ward against the sun. Take three purified waterskins and three venoms to the desert merchant, have it made, and bring it to me.',
  'data.quest.q_craft_fire_ward.complete': 'The ward is cool as well water. With it, I can walk into the Fire Wastes. Thank you, friend.',

  // ─── Zone 4: Scorching Desert — Desert Archaeologist ───
  'data.quest.q_buried_treasure.offer': 'Look at this tablet! Ancient script, whole sentences, and it\'s shattered, the pieces scattered in the sand around the ruins. Help me find them all and take them to the nomad chief. He\'s the only one who can still read it!',
  'data.quest.q_buried_treasure.complete': 'The nomad read it aloud: the Sun Queen\'s last decree, "Return the fire to its hearth." ...Where have I heard that before? This tablet was worth ten more years of digging!',

  // ─── Zone 4: Scorching Desert — Water Diviner ───
  'data.quest.q_water_supply.offer': 'My rod won\'t stop trembling over the western sands. There\'s a spring below. Follow the rod to three patches of damp sand; the spring is at the last. Only... something else is breathing beneath the sound of the water.',
  'data.quest.q_water_supply.complete': 'The Spring-Eater is dead and the water rises. I can hear it laughing. Tonight the children won\'t have to lick the dew.',
  'data.quest.q_mirage_beasts.offer': 'The two flames in the northern haze are no illusion. They are bonded fire spirits, one of day and one of night, never apart. Walk into the mirage and put them out together.',
  'data.quest.q_mirage_beasts.complete': 'The haze holds no flames now. The travelers they lured away won\'t return... but no one else will be lured.',

  // ─── Zone 5: Abyss Rift — Abyss Warden (main chain) ───
  'data.quest.q_explore_abyss.offer': 'Brand-bearer... the wardens have waited a thousand years for you. The rift entrance, the Demon Spire, the Throne of Chaos: go learn the enemy\'s ground. It counts only if you return.',
  'data.quest.q_explore_abyss.complete': 'You came back. Few do. Your face tells me you\'ve already seen the gate.',
  'data.quest.q_kill_demons.offer': 'Every inch the gate widens, another horde pours through. Twenty imps, ten lesser demons. The numbers don\'t matter. Let none of them cross the rift.',
  'data.quest.q_kill_demons.complete': 'They fell back a step. Only a step. To forge the Final Key, we need their own essence as the binding.',
  'data.quest.q_collect_demon_essence.offer': 'Lock the fire\'s gate with things of the fire: that was Aethelyn\'s way. Imps, lesser demons, succubi all carry essence. Fifteen. Go.',
  'data.quest.q_collect_demon_essence.complete': 'Enough essence. Last of all, the seal fragments the wardens paid for with their lives must be forged into a key.',
  'data.quest.q_forge_seal.offer': 'The fragments lie near the Heroes\' Memorial, and each one cost a comrade\'s life. Bring back five, and forge the Final Key with the Hammer you carry.',
  'data.quest.q_forge_seal.complete': 'The key is made. But it still lacks one thing... you already know, don\'t you?',
  'data.quest.q_kill_abyss_lord.offer': 'He sits on the Throne of Chaos, waiting for you, and for your fire. Take the key and go. The wardens have waited a thousand years. Don\'t let it be for nothing.',
  'data.quest.q_kill_abyss_lord.complete': 'The gate is closed. The Abyssfire isn\'t out; it has gone back to the hearth. Go home, Flamekeeper.',
  'data.quest.q_demon_weaponry.offer': 'Every demon blade comes from the armory camp to the south, kept by Quartermaster Morax. Kill him and bring me his war sigil. If we can read their orders, we can move first.',
  'data.quest.q_demon_weaponry.complete': 'The orders on this sigil... they mean to strike the seal stone. Good. Now we know where they\'ll come from.',
  'data.quest.q_defend_seal_abyss.offer': 'The demon legion is assaulting the seal stone. Five waves. If it breaks, the abyss spreads again. Hold. Do not fall back.',
  'data.quest.q_defend_seal_abyss.complete': 'The seal stone stands. So do you. That\'s enough.',

  // ─── Zone 5: Abyss Rift — Fallen Knight ───
  'data.quest.q_corrupted_souls.offer': 'Karon, Lyss, Gig... my three soldiers, who swore themselves to the demons. I taught them to hold a sword, but not to hold an oath. I cannot raise my hand against them... please, end it for me.',
  'data.quest.q_corrupted_souls.complete': 'All three... gone. Did they say anything at the end? ...No. Don\'t tell me. The oaths they broke are mine to carry now.',
  'data.quest.q_fallen_hero.offer': 'Half a last testament is carved at the shrine, half on the memorial, left by those who sealed the abyss three hundred years ago. I can\'t read them... the letters burn before my eyes. Take rubbings and bring them to the warden.',
  'data.quest.q_fallen_hero.complete': 'The warden read it to me: "make a hearth of the heart." ...So they knew, all along. Perhaps I\'m still fit to hold a sword.',

  // ─── Zone 5: Abyss Rift — Void Researcher ───
  'data.quest.q_void_crystals.offer': 'Crystals that breathe, growing on the rift\'s edge! With every breath, space wrinkles. Harvest five before they grow into the next tear, and bring them back alive!',
  'data.quest.q_void_crystals.complete': 'Look, still breathing! Perfectly in step with the rift... I\'d wager the gate\'s own heartbeat is in here. Leave the rest to me.',
  // ─── Ley-beast quests ───
  'data.quest.q_pet_owl.offer': 'The night the Moon Seal broke, my old friend Moonwing fled in fright. Its feathers catch the moonlight. Follow them, and you\'ll likely find it in some hollow tree.',
  'data.quest.q_pet_owl.complete': 'That\'s him! Still glaring at everything. He has chosen you. Go on, take him along.',
  'data.quest.q_pet_cat.offer': 'The dead dragged a cat\'s shadow in between life and death, and it cries in my ear every night. Take back its shadow shards, then defeat the Shade Binder who keeps it.',
  'data.quest.q_pet_cat.complete': 'The shadow is whole again, and it won\'t go back to the dark. It only follows you. Let it.',
  'data.quest.q_pet_dragon.offer': 'With the usurper gone, a warm dragon egg rolled out from under the old forge! It needs fresh fire to hatch. Take three forge embers from the stone golems\' hearts and feed them to the blacksmith\'s furnace.',
  'data.quest.q_pet_dragon.complete': 'The fire roared and the shell cracked! Look at the little one, sneezing sparks already. It\'s yours.',
};
