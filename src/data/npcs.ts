import type { NPCDefinition } from './types';
import { DialogueTrees } from './dialogueTrees';

export const NPCDefinitions: Record<string, NPCDefinition> = {
  blacksmith: {
    id: 'blacksmith',
    name: '铁匠',
    type: 'blacksmith',
    dialogue: ['欢迎来到我的铁匠铺!', '需要修理装备或者打造新武器吗？'],
    shopItems: ['w_short_sword', 'w_broad_sword', 'w_dagger', 'w_stiletto', 'w_short_bow', 'w_oak_staff', 'w_wooden_shield', 'a_leather_helm', 'a_leather_armor', 'a_leather_gloves', 'a_leather_boots', 'a_leather_belt'],
  },
  merchant: {
    id: 'merchant',
    name: '商人',
    type: 'merchant',
    dialogue: ['需要补给吗？我这里应有尽有!', '药水、卷轴、宝石，随你挑选。'],
    shopItems: ['c_hp_potion_s', 'c_hp_potion_m', 'c_mp_potion_s', 'c_mp_potion_m', 'c_antidote', 'c_tp_scroll', 'c_id_scroll', 'g_ruby_1', 'g_sapphire_1', 'g_emerald_1', 'g_topaz_1'],
  },
  stash: {
    id: 'stash',
    name: '仓库管理员',
    type: 'stash',
    dialogue: ['需要存放物品吗？', '您的仓库安全可靠。'],
  },
  quest_elder: {
    id: 'quest_elder',
    name: '村长',
    type: 'quest',
    dialogue: ['勇士，翡翠平原上的怪物越来越多了...', '请帮助我们清除这些威胁!'],
    quests: ['q_kill_slimes', 'q_collect_slime_gel', 'q_herb_gathering', 'q_kill_goblins', 'q_explore_goblin_camp', 'q_lost_pendant', 'q_find_goblin_chief', 'q_secure_plains', 'q_escort_merchant_plains', 'q_pet_sprite_friend'],
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

  // ─── Zone 1: Emerald Plains — New Field NPCs ─────────────────────────
  plains_herbalist: {
    id: 'plains_herbalist',
    name: '平原药师',
    type: 'merchant',
    dialogue: [
      '你好，旅行者！我在这片平原上采集草药已有二十年了。',
      '翡翠平原的灵脉之力滋养着这里的一切，连草药都比别处更有药效。',
      '需要补给吗？我的药水都是用灵脉草药亲手调配的。',
    ],
    shopItems: ['c_hp_potion_s', 'c_hp_potion_m', 'c_mp_potion_s', 'c_antidote'],
  },
  plains_wanderer: {
    id: 'plains_wanderer',
    name: '流浪剑客',
    type: 'quest',
    dialogue: [
      '我曾是王国的骑士，如今在这平原上漫无目的地游荡。',
      '你知道吗？这片看似平和的草原下面，埋藏着精灵族的古老遗迹。',
      '我在东边的小丘附近发现了一处隐蔽的入口，但那里面太危险了，我一个人不敢进去。',
    ],
    quests: ['q_bandit_trouble'],
  },

  // ─── Zone 2: Twilight Forest — New Field NPCs ────────────────────────
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
  tower_herbalist: {
    id: 'tower_herbalist',
    name: '药师',
    type: 'quest',
    spriteId: 'plains_herbalist',
    dialogue: ['灵脉回暖，药草长得比平原上还旺。', '你在外头每斩一只怪，园子就多一分生气。'],
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
