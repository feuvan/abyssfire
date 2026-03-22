import type { SetDefinition, LegendaryDefinition } from '../types';

/**
 * D2-style set design philosophy:
 * - Each piece has fixed individual affixes (like D2 unique items within a set)
 * - 2-piece bonus: utility / defensive
 * - 3-piece bonus: offensive / build-defining
 * - 4-piece bonus: powerful capstone that changes gameplay
 * - Set bonuses stack (you get 2pc + 3pc + 4pc at full set)
 */
export const SetDefinitions: SetDefinition[] = [
  // === WARRIOR SET: 铁壁守护者 ===
  {
    id: 'set_iron_guardian',
    name: '铁壁守护者',
    nameEn: 'Iron Guardian',
    pieces: ['set_iron_helm', 'set_iron_armor', 'set_iron_shield', 'set_iron_belt'],
    pieceAffixes: {
      set_iron_helm: [
        { affixId: 'set_ig_1', name: '守护者之坚', stat: 'defense', value: 15 },
        { affixId: 'set_ig_2', name: '坚定意志', stat: 'allResist', value: 8 },
      ],
      set_iron_armor: [
        { affixId: 'set_ig_3', name: '铁壁之心', stat: 'maxHp', value: 80 },
        { affixId: 'set_ig_4', name: '铜皮铁骨', stat: 'defense', value: 25 },
      ],
      set_iron_shield: [
        { affixId: 'set_ig_5', name: '不动如山', stat: 'defense', value: 20 },
        { affixId: 'set_ig_6', name: '磐石之力', stat: 'str', value: 10 },
      ],
      set_iron_belt: [
        { affixId: 'set_ig_7', name: '守卫之韧', stat: 'maxHp', value: 40 },
        { affixId: 'set_ig_8', name: '体魄强健', stat: 'vit', value: 8 },
      ],
    },
    bonuses: [
      { count: 2, description: '+30% 最大生命', stats: { maxHpPercent: 30 } },
      { count: 3, description: '受击回复2%最大生命，+15 全抗', stats: { thornsHeal: 2, allResist: 15 } },
      { count: 4, description: '生命低于30%时免疫一次致死伤害（60秒冷却）', stats: { deathSave: 1 } },
    ],
  },

  // === ROGUE SET: 暗影刺客 ===
  {
    id: 'set_shadow_assassin',
    name: '暗影刺客',
    nameEn: 'Shadow Assassin',
    pieces: ['set_shadow_helm', 'set_shadow_armor', 'set_shadow_gloves', 'set_shadow_boots'],
    pieceAffixes: {
      set_shadow_helm: [
        { affixId: 'set_sa_1', name: '鹰眼', stat: 'critRate', value: 8 },
        { affixId: 'set_sa_2', name: '暗影潜行', stat: 'dex', value: 12 },
      ],
      set_shadow_armor: [
        { affixId: 'set_sa_3', name: '影遁之衣', stat: 'dex', value: 10 },
        { affixId: 'set_sa_4', name: '暗杀本能', stat: 'critDamage', value: 15 },
      ],
      set_shadow_gloves: [
        { affixId: 'set_sa_5', name: '毒刃', stat: 'poisonDamage', value: 10 },
        { affixId: 'set_sa_6', name: '迅捷之手', stat: 'attackSpeed', value: 8 },
      ],
      set_shadow_boots: [
        { affixId: 'set_sa_7', name: '无影步', stat: 'moveSpeed', value: 15 },
        { affixId: 'set_sa_8', name: '幻影闪避', stat: 'dex', value: 8 },
      ],
    },
    bonuses: [
      { count: 2, description: '+20% 暴击率，+10% 攻击速度', stats: { critRate: 20, attackSpeed: 10 } },
      { count: 3, description: '暴击伤害 +50%，击杀回复5%生命', stats: { critDamage: 50, killHealPercent: 5 } },
      { count: 4, description: '暴击时25%概率连击（立即发动额外一次攻击）', stats: { critDoubleStrike: 25 } },
    ],
  },

  // === MAGE SET: 大法师 ===
  {
    id: 'set_archmage',
    name: '大法师',
    nameEn: 'Archmage',
    pieces: ['set_archmage_hat', 'set_archmage_robe', 'set_archmage_staff', 'set_archmage_ring'],
    pieceAffixes: {
      set_archmage_hat: [
        { affixId: 'set_am_1', name: '渊博学识', stat: 'int', value: 15 },
        { affixId: 'set_am_2', name: '冥思苦想', stat: 'manaRegen', value: 5 },
      ],
      set_archmage_robe: [
        { affixId: 'set_am_3', name: '魔力之泉', stat: 'maxMana', value: 60 },
        { affixId: 'set_am_4', name: '奥术屏障', stat: 'allResist', value: 10 },
      ],
      set_archmage_staff: [
        { affixId: 'set_am_5', name: '魔力增幅', stat: 'damagePercent', value: 30 },
        { affixId: 'set_am_6', name: '元素精通', stat: 'fireDamage', value: 12 },
      ],
      set_archmage_ring: [
        { affixId: 'set_am_7', name: '法力虹吸', stat: 'manaSteal', value: 5 },
        { affixId: 'set_am_8', name: '智者之环', stat: 'int', value: 10 },
      ],
    },
    bonuses: [
      { count: 2, description: '+25% 法力上限，法力回复 +3/秒', stats: { maxManaPercent: 25, manaRegen: 3 } },
      { count: 3, description: '技能冷却减少20%，+20 火焰/冰霜伤害', stats: { cooldownReduction: 20, fireDamage: 20, iceDamage: 20 } },
      { count: 4, description: '施法时15%概率不消耗法力，所有元素伤害+30%', stats: { freeCast: 15, elementalDamagePercent: 30 } },
    ],
  },

  // === RANGER SET: 猎手 ===
  {
    id: 'set_hunter',
    name: '荒野猎手',
    nameEn: 'Wilds Hunter',
    pieces: ['set_hunter_helm', 'set_hunter_armor', 'set_hunter_boots', 'set_hunter_bow'],
    pieceAffixes: {
      set_hunter_helm: [
        { affixId: 'set_hu_1', name: '鹰眼视野', stat: 'critRate', value: 5 },
        { affixId: 'set_hu_2', name: '猎手直觉', stat: 'dex', value: 10 },
      ],
      set_hunter_armor: [
        { affixId: 'set_hu_3', name: '野性皮甲', stat: 'defense', value: 15 },
        { affixId: 'set_hu_4', name: '自然恩赐', stat: 'hpRegen', value: 4 },
      ],
      set_hunter_boots: [
        { affixId: 'set_hu_5', name: '追风步', stat: 'moveSpeed', value: 20 },
        { affixId: 'set_hu_6', name: '丛林行者', stat: 'dex', value: 8 },
      ],
      set_hunter_bow: [
        { affixId: 'set_hu_7', name: '穿甲之矢', stat: 'damage', value: 20 },
        { affixId: 'set_hu_8', name: '连珠箭法', stat: 'attackSpeed', value: 12 },
      ],
    },
    bonuses: [
      { count: 2, description: '+15% 攻击速度，+20% 移动速度', stats: { attackSpeed: 15, moveSpeed: 20 } },
      { count: 3, description: '+30% 掉宝率，击杀回复3%生命', stats: { magicFind: 30, killHealPercent: 3 } },
      { count: 4, description: '普攻30%概率发射双倍箭矢，+20% 暴击伤害', stats: { doubleShot: 30, critDamage: 20 } },
    ],
  },

  // === ENDGAME UNIVERSAL SET: 渊火之誓 ===
  {
    id: 'set_abyssfire',
    name: '渊火之誓',
    nameEn: 'Abyssfire Oath',
    pieces: ['set_abyss_ring', 'set_abyss_amulet', 'set_abyss_belt'],
    pieceAffixes: {
      set_abyss_ring: [
        { affixId: 'set_af_1', name: '深渊共鸣', stat: 'damagePercent', value: 15 },
        { affixId: 'set_af_2', name: '渊火灼烧', stat: 'fireDamage', value: 15 },
      ],
      set_abyss_amulet: [
        { affixId: 'set_af_3', name: '渊火庇护', stat: 'allResist', value: 12 },
        { affixId: 'set_af_4', name: '不灭意志', stat: 'maxHpPercent', value: 10 },
      ],
      set_abyss_belt: [
        { affixId: 'set_af_5', name: '深渊束缚', stat: 'defense', value: 18 },
        { affixId: 'set_af_6', name: '渊火之力', stat: 'str', value: 8 },
      ],
    },
    bonuses: [
      { count: 2, description: '+10% 全抗，+15% 伤害', stats: { allResist: 10, damagePercent: 15 } },
      { count: 3, description: '击杀恢复8%最大生命，+10% 冷却缩减', stats: { killHealPercent: 8, cooldownReduction: 10 } },
    ],
  },
];

export const LegendaryItems: LegendaryDefinition[] = [
  // === WEAPONS ===
  {
    id: 'leg_soulreaver',
    baseId: 'w_demon_blade',
    name: '灵魂收割者',
    nameEn: 'Soulreaver',
    fixedAffixes: [
      { affixId: 'leg_1', name: '灵魂收割', stat: 'damage', value: 45 },
      { affixId: 'leg_2', name: '噬灵', stat: 'lifeSteal', value: 8 },
      { affixId: 'leg_2b', name: '亡灵之力', stat: 'critDamage', value: 20 },
    ],
    specialEffect: 'killHealPercent',
    specialEffectValue: 5,
    specialEffectDescription: '击杀回复5%最大生命',
  },
  {
    id: 'leg_frostburn',
    baseId: 'w_arcane_staff',
    name: '霜火之杖',
    nameEn: 'Frostburn',
    fixedAffixes: [
      { affixId: 'leg_3', name: '霜火交融', stat: 'fireDamage', value: 20 },
      { affixId: 'leg_4', name: '极寒之触', stat: 'iceDamage', value: 20 },
      { affixId: 'leg_4b', name: '元素亲和', stat: 'int', value: 15 },
    ],
    specialEffect: 'elementalDamagePercent',
    specialEffectValue: 25,
    specialEffectDescription: '所有元素伤害 +25%',
  },
  {
    id: 'leg_windforce',
    baseId: 'w_war_bow',
    name: '风之力',
    nameEn: 'Windforce',
    fixedAffixes: [
      { affixId: 'leg_9', name: '疾风箭矢', stat: 'damage', value: 35 },
      { affixId: 'leg_10', name: '击退之风', stat: 'knockback', value: 2 },
      { affixId: 'leg_10b', name: '风暴之速', stat: 'attackSpeed', value: 15 },
    ],
    specialEffect: 'doubleShot',
    specialEffectValue: 25,
    specialEffectDescription: '普攻25%概率发射双倍箭矢',
  },
  {
    id: 'leg_grief',
    baseId: 'w_broad_sword',
    name: '悲伤',
    nameEn: 'Grief',
    fixedAffixes: [
      { affixId: 'leg_gr1', name: '悲痛之刃', stat: 'damage', value: 30 },
      { affixId: 'leg_gr2', name: '嗜杀如狂', stat: 'attackSpeed', value: 20 },
      { affixId: 'leg_gr3', name: '绝望之触', stat: 'lifeSteal', value: 5 },
    ],
    specialEffect: 'ignoreDefense',
    specialEffectValue: 20,
    specialEffectDescription: '攻击忽略目标20%防御',
  },

  // === ARMOR ===
  {
    id: 'leg_shadowstep',
    baseId: 'a_leather_boots',
    name: '暗影之履',
    nameEn: 'Shadowstep',
    fixedAffixes: [
      { affixId: 'leg_5', name: '幻影疾步', stat: 'moveSpeed', value: 25 },
      { affixId: 'leg_6', name: '暗影闪避', stat: 'dex', value: 15 },
    ],
    specialEffect: 'dodgeCounter',
    specialEffectValue: 1,
    specialEffectDescription: '闪避后下次攻击必定暴击',
  },
  {
    id: 'leg_aegis',
    baseId: 'w_iron_shield',
    name: '不灭之盾',
    nameEn: 'Aegis',
    fixedAffixes: [
      { affixId: 'leg_7', name: '永恒守护', stat: 'defense', value: 30 },
      { affixId: 'leg_8', name: '不屈之心', stat: 'maxHp', value: 100 },
      { affixId: 'leg_8b', name: '圣光庇护', stat: 'allResist', value: 10 },
    ],
    specialEffect: 'deathDefiance',
    specialEffectValue: 10,
    specialEffectDescription: '受到致命伤害时10%概率免死并回复30%生命',
  },
  {
    id: 'leg_tyrael',
    baseId: 'a_plate_armor',
    name: '大天使之铠',
    nameEn: 'Tyrael\'s Might',
    fixedAffixes: [
      { affixId: 'leg_ty1', name: '神圣之力', stat: 'defense', value: 35 },
      { affixId: 'leg_ty2', name: '天使庇佑', stat: 'allResist', value: 15 },
      { affixId: 'leg_ty3', name: '光之祝福', stat: 'hpRegen', value: 8 },
    ],
    specialEffect: 'damageReduction',
    specialEffectValue: 10,
    specialEffectDescription: '受到的所有伤害减少10%',
  },

  // === ACCESSORIES ===
  {
    id: 'leg_soj',
    baseId: 'j_gold_ring',
    name: '乔丹之石',
    nameEn: 'Stone of Jordan',
    fixedAffixes: [
      { affixId: 'leg_soj1', name: '无尽魔力', stat: 'maxManaPercent', value: 25 },
      { affixId: 'leg_soj2', name: '技能增幅', stat: 'damagePercent', value: 20 },
      { affixId: 'leg_soj3', name: '智慧之石', stat: 'int', value: 12 },
    ],
    specialEffect: 'cooldownReduction',
    specialEffectValue: 10,
    specialEffectDescription: '所有技能冷却减少10%',
  },
  {
    id: 'leg_maras',
    baseId: 'j_jade_amulet',
    name: '玛拉的万花筒',
    nameEn: 'Mara\'s Kaleidoscope',
    fixedAffixes: [
      { affixId: 'leg_mk1', name: '万花绽放', stat: 'allResist', value: 20 },
      { affixId: 'leg_mk2', name: '全能之力', stat: 'str', value: 5 },
      { affixId: 'leg_mk3', name: '全能之智', stat: 'int', value: 5 },
      { affixId: 'leg_mk4', name: '全能之捷', stat: 'dex', value: 5 },
    ],
    specialEffect: 'allStatsBonus',
    specialEffectValue: 2,
    specialEffectDescription: '每次升级额外获得+2全属性',
  },
];
