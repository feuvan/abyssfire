import type { NPCDefinition } from './types';
import { DialogueTrees } from './dialogueTrees';

// Chapter 1 of the dark remake (Docs/dark-remake/chapter1.md §2-3, package 1B part 2): the kept ids are role keys
// (DECISIONS DR3) re-cast for Knellwatch (丧钟堡): blacksmith = Hald Swordmelter, merchant = Quartermaster Vira, stash =
// Tamsin the Gravewarden, quest_elder = Captain Serin Harrow, plains_herbalist = Mother Marl, plains_wanderer = Corran
// the deserter; knell_chaplain (Brother Aldwin) and crow_trader (Mag Crowe) are new. `blacksmith`, `merchant` and `stash`
// also stand in later chapters' camps, which stay sealed in milestone 1 (DECISIONS W7) until their own dark pass. Each
// keeps one line in Wave 1 (chapter1.md §3: the quest-gated lines need the Wave 2 NPC `lines`; their strings are ready as
// `data.npc.<id>.after.<questId>`, GameData/i18n/ch1Npcs.ts). The words live in i18n; `name` / `dialogue` here are the
// zh-CN data copies.
export const NPCDefinitions: Record<string, NPCDefinition> = {
  blacksmith: {
    id: 'blacksmith',
    name: '哈尔德·熔剑',
    type: 'blacksmith',
    dialogue: ['每把剑的柄脚都刻着名字。剑我熔了，名字留给队长。'],
    shopItems: ['w_short_sword', 'w_broad_sword', 'w_dagger', 'w_stiletto', 'w_short_bow', 'w_oak_staff', 'w_wooden_shield', 'a_leather_helm', 'a_leather_armor', 'a_leather_gloves', 'a_leather_boots', 'a_leather_belt'],
  },
  merchant: {
    id: 'merchant',
    name: '军需官薇拉',
    type: 'merchant',
    dialogue: ['一个名字，省下一份口粮。今晚又省了三份。'],
    shopItems: ['c_hp_potion_s', 'c_hp_potion_m', 'c_mp_potion_s', 'c_mp_potion_m', 'c_antidote', 'c_tp_scroll', 'c_id_scroll', 'g_ruby_1', 'g_sapphire_1', 'g_emerald_1', 'g_topaz_1'],
  },
  stash: {
    id: 'stash',
    name: '守冢人塔姆欣',
    type: 'stash',
    dialogue: ['死人的东西我替他们收着。活人的，也行。'],
  },
  quest_elder: {
    id: 'quest_elder',
    name: '瑟琳·哈洛队长',
    type: 'quest',
    dialogue: ['坑里爬出来的。能站着，就能拿刀。'],
    // The main line and S1 (chapter1.md §7: six quests).
    quests: ['q_rf_burial_detail', 'q_rf_black_blood', 'q_rf_gnawed_field', 'q_rf_ashwheat', 'q_rf_marshal', 'q_rf_last_toll'],
    dialogueTree: DialogueTrees['quest_elder'],
  },
  quest_scout: {
    id: 'quest_scout',
    name: '侦察兵',
    type: 'quest',
    dialogue: ['暮色森林中有不祥的动静...', '你愿意去调查一下吗？'],
    quests: ['q_explore_forest', 'q_kill_undead', 'q_talk_hermit', 'q_kill_werewolf_alpha', 'q_seal_dark_source', 'q_defend_camp_forest'],
    dialogueTree: DialogueTrees['quest_scout'],
  },
  forest_hermit: {
    id: 'forest_hermit',
    name: '森林隐士',
    type: 'quest',
    dialogue: ['你终于找到我了...亡灵复苏的根源在森林深处的黑暗能量。', '去找到那个黑暗之源，否则整片森林都将沦陷。'],
    quests: ['q_pet_owl'],
  },
  blacksmith_advanced: {
    id: 'blacksmith_advanced',
    name: '高级铁匠',
    type: 'blacksmith',
    dialogue: ['只有最好的材料才配得上我的手艺。'],
    shopItems: ['w_battle_axe', 'w_arcane_staff', 'w_iron_shield', 'a_chain_mail', 'a_iron_helm', 'a_chain_gloves', 'a_chain_boots', 'a_heavy_belt'],
  },
  quest_dwarf: {
    id: 'quest_dwarf',
    name: '矮人长老',
    type: 'quest',
    dialogue: ['这些山脉曾是我族的家园...', '帮助我们夺回先祖的遗迹吧。'],
    quests: ['q_explore_dwarf_ruins', 'q_kill_gargoyles', 'q_collect_dwarf_relics', 'q_dragon_egg', 'q_reforge_artifact', 'q_kill_stone_guardian', 'q_craft_dwarf_weapon', 'q_pet_dragon'],
    dialogueTree: DialogueTrees['quest_dwarf'],
  },
  quest_nomad: {
    id: 'quest_nomad',
    name: '沙漠游牧民',
    type: 'quest',
    dialogue: ['灼热的沙漠中危机四伏...', '只有最勇敢的人才能在这里生存。'],
    quests: ['q_explore_desert', 'q_kill_fire_elementals', 'q_scorpion_venom', 'q_explore_oasis', 'q_kill_sandworms', 'q_seal_fire_rift', 'q_escort_survivor_desert', 'q_craft_fire_ward'],
    dialogueTree: DialogueTrees['quest_nomad'],
  },
  quest_warden: {
    id: 'quest_warden',
    name: '深渊守望者',
    type: 'quest',
    dialogue: ['深渊裂隙正在扩大，恶魔即将涌入...', '我们需要你的力量来封印它。'],
    quests: ['q_explore_abyss', 'q_kill_demons', 'q_collect_demon_essence', 'q_demon_weaponry', 'q_forge_seal', 'q_kill_abyss_lord', 'q_defend_seal_abyss'],
    dialogueTree: DialogueTrees['quest_warden'],
  },
  merchant_desert: {
    id: 'merchant_desert',
    name: '沙漠商人',
    type: 'merchant',
    dialogue: ['沙漠里水比金子还贵...', '不过我有你需要的一切。'],
    shopItems: ['c_hp_potion_m', 'c_hp_potion_l', 'c_mp_potion_m', 'c_antidote', 'c_tp_scroll', 'c_id_scroll', 'g_ruby_2', 'g_sapphire_2', 'g_diamond_1'],
  },

  // ─── Zone 1: the Red Field — field NPCs: Marl's dressing station (50,30), Corran's culvert (70,65) ─────────────────
  plains_herbalist: {
    id: 'plains_herbalist',
    name: '军医玛勒',
    type: 'merchant',
    // S1's talk target (the breach-blood samples); leaves for the Ember Tower in the finale (tower_herbalist).
    dialogue: ['别碰那黑血。碰过的人都不喊疼了——那才最疼。'],
    shopItems: ['c_hp_potion_s', 'c_hp_potion_m', 'c_mp_potion_s', 'c_antidote'],
  },
  plains_wanderer: {
    id: 'plains_wanderer',
    name: '逃兵柯兰',
    type: 'quest',
    dialogue: ['我跑了，我活下来了。这两件事，哪件更丢人？'],
    // S4: the bounty on his old sergeant.
    quests: ['q_rf_oathless'],
  },

  // ─── Zone 2: Twilight Forest — New Field NPCs ────────────────────────
  // ─── Chapter 1 (dark remake, Docs/dark-remake/chapter1.md §3): new NPCs; the kept ids above are role keys ───
  // Brother Aldwin stands in Knellwatch (camp 1, the 4th slot) and gives S2, S3, S5 and S6 (S7 with Wave 3). His second
  // line is always on (chapter1.md §3); no dialogue tree.
  knell_chaplain: {
    id: 'knell_chaplain',
    name: '奥德温修士',
    type: 'quest',
    dialogue: ['钟声是给活人听的。死人……另有人在跟他们说话。', '我替每个名字念一遍经。念到第三百个，我开始怕自己忘了念给谁。'],
    quests: ['q_rf_barrowmoss', 'q_rf_tag_thief', 'q_rf_lanternwisp', 'q_rf_hallowed_ground'],
  },
  // Mag Crowe trades in Crowmarket (camp 2), which used the shared `merchant` before: same wares.
  crow_trader: {
    id: 'crow_trader',
    name: '鸦嫂玛格',
    type: 'merchant',
    dialogue: ['别问靴子是谁的。问价钱。'],
    shopItems: ['c_hp_potion_s', 'c_hp_potion_m', 'c_mp_potion_s', 'c_mp_potion_m', 'c_antidote', 'c_tp_scroll', 'c_id_scroll', 'g_ruby_1', 'g_sapphire_1', 'g_emerald_1', 'g_topaz_1'],
  },

  forest_tracker: {
    id: 'forest_tracker',
    name: '森林猎人',
    type: 'quest',
    dialogue: [
      '嘘！小声点……暮色森林里到处都是捕食者的耳目。',
      '自从黑暗能量蔓延开来，连普通的狼都变成了凶残的狼人。',
      '我在追踪一头巨大的暗影狼，它的巢穴就在南边的幽暗洞窟中。如果你胆子够大，可以去看看。',
    ],
    quests: ['q_collect_wolf_pelts', 'q_lost_scout'],
  },
  forest_spirit_medium: {
    id: 'forest_spirit_medium',
    name: '通灵巫女',
    type: 'quest',
    dialogue: [
      '你感受到了吗？……那些逝去的灵魂在哭泣。',
      '暮色森林的亡灵并非邪恶之物，它们是被黑暗力量困住的可怜灵魂。',
      '在森林深处有一座被遗忘的月光祭坛，也许能净化它们……但那里被强大的亡灵守卫着。',
    ],
    quests: ['q_spider_nest', 'q_investigate_corruption_forest', 'q_ancient_relic', 'q_pet_cat'],
  },

  // ─── Zone 3: Anvil Mountains — New Field NPCs ────────────────────────
  mountain_miner: {
    id: 'mountain_miner',
    name: '矿工老汉',
    type: 'quest',
    dialogue: [
      '咳咳……这矿洞里的灰尘越来越多了。',
      '年轻人，铁砧山脉的深处藏着矮人王朝的宝库。',
      '但自从石像鬼占据了上层矿道，就没人敢深入了。我在西边发现了一条废弃的矿道入口，也许能通向宝库……',
    ],
    quests: ['q_crystal_mining', 'q_trapped_miners', 'q_pet_jade_tortoise'],
  },
  mountain_rune_scholar: {
    id: 'mountain_rune_scholar',
    name: '符文学者',
    type: 'quest',
    dialogue: [
      '矮人的符文铭刻技术堪称世间一绝。',
      '这些山脉中的每一块石头都可能刻有远古符文。我在研究一种能够激活矮人机关的古老密码。',
      '如果你在探索中发现任何刻有符文的石板，请告诉我，那对我的研究至关重要。',
    ],
    quests: ['q_mountain_bandits', 'q_investigate_ruins_mountains'],
  },

  // ─── Zone 4: Scorching Desert — New Field NPCs ───────────────────────
  desert_archaeologist: {
    id: 'desert_archaeologist',
    name: '沙漠考古学家',
    type: 'quest',
    dialogue: [
      '这片沙漠下面埋着一个完整的古代文明！',
      '我已经发掘了数十件文物，但最重要的发现是一座半埋在沙丘下的地下神殿。',
      '神殿入口就在东南方向，里面可能藏有沙漠王国最后的秘宝……但也充满了不死守卫。',
    ],
    quests: ['q_buried_treasure'],
  },
  desert_water_diviner: {
    id: 'desert_water_diviner',
    name: '寻水者',
    type: 'quest',
    dialogue: [
      '在灼热荒漠中，水就是生命。',
      '我能感应到地下水脉的流向。这片沙漠并非一直如此荒芜——千年前这里曾有绿洲和花园。',
      '在北方有一处被沙暴掩埋的古老绿洲遗址，如果你仔细寻找，也许能找到远古泉眼的遗迹。',
    ],
    quests: ['q_water_supply', 'q_mirage_beasts'],
  },

  // ─── Zone 5: Abyss Rift — New Field NPCs ─────────────────────────────
  abyss_fallen_knight: {
    id: 'abyss_fallen_knight',
    name: '堕落骑士',
    type: 'quest',
    dialogue: [
      '不要靠近我……我已经被虚空腐蚀了。',
      '我曾是封印守护者之一。深渊的低语日夜不停，侵蚀着每一个守护者的意志。',
      '在裂谷的最深处，有一座恶魔祭坛。如果不摧毁它，封印永远无法修复。但去那里……等于送死。',
    ],
    quests: ['q_corrupted_souls', 'q_fallen_hero'],
  },
  abyss_void_researcher: {
    id: 'abyss_void_researcher',
    name: '虚空研究者',
    type: 'quest',
    dialogue: [
      '迷人……又可怕。虚空的能量遵循着完全不同的法则。',
      '我冒着生命危险来到这里收集虚空结晶样本。这些结晶中蕴含着扭曲空间的力量。',
      '东边有一处虚空能量特别集中的区域，那里的空间已经开始扭曲。我放了一些标记，你可以去探索。',
    ],
    quests: ['q_void_crystals'],
  },

  // ─── 余烬之塔 (Ember Tower): each chapter's ally tends one wing ───────────
  // Standing in `ember_tower` only once their wing is unlocked (EmberTower);
  // talking to them opens that wing's page of the homestead panel.
  tower_elder: {
    id: 'tower_elder',
    name: '村长',
    type: 'quest',
    spriteId: 'quest_elder',
    dialogue: ['塔还是那座塔，可如今有了炉火。', '想修哪一翼，跟老头子说。'],
    quests: [],
  },
  // Mother Marl, sent by the Chapter 1 finale (cs_rf_finale; homestead.ts herb_garden). Hidden with the tower in
  // milestone 1 (DECISIONS Q3).
  tower_herbalist: {
    id: 'tower_herbalist',
    name: '军医玛勒',
    type: 'quest',
    spriteId: 'plains_herbalist',
    dialogue: ['这里的火，不要人偿。药圃我守着。', '你在外头每放倒一个，园子就多长一寸。别问我为什么。'],
    quests: [],
  },
  tower_hermit: {
    id: 'tower_hermit',
    name: '森林隐士',
    type: 'quest',
    spriteId: 'forest_hermit',
    dialogue: ['月井的水映着月光，灵兽们爱在这里歇脚。'],
    quests: [],
  },
  tower_dwarf: {
    id: 'tower_dwarf',
    name: '矮人工匠',
    type: 'quest',
    spriteId: 'quest_dwarf',
    dialogue: ['三颗碎石头，炉火一吹，就是一颗好宝石。'],
    quests: [],
  },
  tower_nomad: {
    id: 'tower_nomad',
    name: '游牧民',
    type: 'quest',
    spriteId: 'quest_nomad',
    dialogue: ['商队的骆驼已经喂饱了。让你的灵兽跟我们走一趟吧。'],
    quests: [],
  },
  tower_warden: {
    id: 'tower_warden',
    name: '深渊守望者',
    type: 'quest',
    spriteId: 'quest_warden',
    dialogue: ['心焰的余温还在祭坛里。献上余烬，它会护你一程。'],
    quests: [],
  },
};
