import type { AffixDefinition } from '../types';

export const Prefixes: AffixDefinition[] = [
  // === DAMAGE (weapon, gloves) ===
  // Tier 1 (lv 1-10)
  { id: 'pre_sharp', name: '锋利的', nameEn: 'Sharp', type: 'prefix', tier: 1, stat: 'damage', minValue: 1, maxValue: 3, levelReq: 1, allowedSlots: ['weapon', 'offhand'] },
  { id: 'pre_quick', name: '迅捷的', nameEn: 'Quick', type: 'prefix', tier: 1, stat: 'attackSpeed', minValue: 3, maxValue: 5, levelReq: 1, allowedSlots: ['weapon', 'gloves'] },
  // Tier 2 (lv 10-20)
  { id: 'pre_keen', name: '敏锐的', nameEn: 'Keen', type: 'prefix', tier: 2, stat: 'damage', minValue: 4, maxValue: 8, levelReq: 10, allowedSlots: ['weapon', 'offhand'] },
  { id: 'pre_swift', name: '疾速的', nameEn: 'Swift', type: 'prefix', tier: 2, stat: 'attackSpeed', minValue: 6, maxValue: 10, levelReq: 10, allowedSlots: ['weapon', 'gloves'] },
  // Tier 3 (lv 20-30)
  { id: 'pre_deadly', name: '致命的', nameEn: 'Deadly', type: 'prefix', tier: 3, stat: 'damage', minValue: 10, maxValue: 18, levelReq: 20, allowedSlots: ['weapon', 'offhand'] },
  // Tier 4 (lv 30-40)
  { id: 'pre_cruel', name: '残忍的', nameEn: 'Cruel', type: 'prefix', tier: 4, stat: 'damage', minValue: 20, maxValue: 35, levelReq: 30, allowedSlots: ['weapon', 'offhand'] },
  // Tier 5 (lv 40-50)
  { id: 'pre_abyssal', name: '深渊的', nameEn: 'Abyssal', type: 'prefix', tier: 5, stat: 'damage', minValue: 35, maxValue: 55, levelReq: 40, allowedSlots: ['weapon', 'offhand'] },

  // === DEFENSE (armor, helmet, gloves, boots, belt, offhand) ===
  { id: 'pre_sturdy', name: '坚韧的', nameEn: 'Sturdy', type: 'prefix', tier: 1, stat: 'defense', minValue: 1, maxValue: 3, levelReq: 1, allowedSlots: ['helmet', 'armor', 'gloves', 'boots', 'belt', 'offhand'] },
  { id: 'pre_fortified', name: '强化的', nameEn: 'Fortified', type: 'prefix', tier: 2, stat: 'defense', minValue: 4, maxValue: 8, levelReq: 10, allowedSlots: ['helmet', 'armor', 'gloves', 'boots', 'belt', 'offhand'] },
  { id: 'pre_godly', name: '神圣的', nameEn: 'Godly', type: 'prefix', tier: 3, stat: 'defense', minValue: 10, maxValue: 18, levelReq: 20, allowedSlots: ['helmet', 'armor', 'gloves', 'boots', 'belt', 'offhand'] },
  { id: 'pre_mythic', name: '传说的', nameEn: 'Mythic', type: 'prefix', tier: 4, stat: 'defense', minValue: 20, maxValue: 30, levelReq: 30, allowedSlots: ['helmet', 'armor', 'gloves', 'boots', 'belt', 'offhand'] },

  // === PRIMARY STATS (any slot) ===
  { id: 'pre_strong', name: '强壮的', nameEn: 'Strong', type: 'prefix', tier: 1, stat: 'str', minValue: 2, maxValue: 4, levelReq: 1 },
  { id: 'pre_nimble', name: '灵巧的', nameEn: 'Nimble', type: 'prefix', tier: 1, stat: 'dex', minValue: 2, maxValue: 4, levelReq: 1 },
  { id: 'pre_wise', name: '睿智的', nameEn: 'Wise', type: 'prefix', tier: 1, stat: 'int', minValue: 2, maxValue: 4, levelReq: 1 },
  { id: 'pre_hardy', name: '健壮的', nameEn: 'Hardy', type: 'prefix', tier: 1, stat: 'vit', minValue: 2, maxValue: 4, levelReq: 1 },
  { id: 'pre_spiritual', name: '通灵的', nameEn: 'Spiritual', type: 'prefix', tier: 1, stat: 'spi', minValue: 2, maxValue: 4, levelReq: 1 },

  { id: 'pre_savage', name: '凶猛的', nameEn: 'Savage', type: 'prefix', tier: 2, stat: 'str', minValue: 5, maxValue: 10, levelReq: 10 },
  { id: 'pre_agile', name: '敏捷的', nameEn: 'Agile', type: 'prefix', tier: 2, stat: 'dex', minValue: 5, maxValue: 10, levelReq: 10 },
  { id: 'pre_arcane', name: '奥术的', nameEn: 'Arcane', type: 'prefix', tier: 2, stat: 'int', minValue: 5, maxValue: 10, levelReq: 10 },
  { id: 'pre_stalwart', name: '刚毅的', nameEn: 'Stalwart', type: 'prefix', tier: 2, stat: 'vit', minValue: 5, maxValue: 10, levelReq: 10 },

  { id: 'pre_titan', name: '泰坦的', nameEn: 'Titan\'s', type: 'prefix', tier: 3, stat: 'str', minValue: 12, maxValue: 20, levelReq: 20 },
  { id: 'pre_shadow', name: '暗影的', nameEn: 'Shadow', type: 'prefix', tier: 3, stat: 'dex', minValue: 12, maxValue: 20, levelReq: 20 },
  { id: 'pre_elder', name: '长老的', nameEn: 'Elder\'s', type: 'prefix', tier: 3, stat: 'int', minValue: 12, maxValue: 20, levelReq: 20 },

  // === PERCENT DAMAGE (weapon) ===
  { id: 'pre_empowered', name: '赋能的', nameEn: 'Empowered', type: 'prefix', tier: 2, stat: 'damagePercent', minValue: 10, maxValue: 20, levelReq: 10, allowedSlots: ['weapon'] },
  { id: 'pre_ferocious', name: '凶残的', nameEn: 'Ferocious', type: 'prefix', tier: 3, stat: 'damagePercent', minValue: 25, maxValue: 40, levelReq: 20, allowedSlots: ['weapon'] },
  { id: 'pre_merciless', name: '无情的', nameEn: 'Merciless', type: 'prefix', tier: 4, stat: 'damagePercent', minValue: 45, maxValue: 65, levelReq: 30, allowedSlots: ['weapon'] },
  { id: 'pre_annihilating', name: '歼灭的', nameEn: 'Annihilating', type: 'prefix', tier: 5, stat: 'damagePercent', minValue: 70, maxValue: 100, levelReq: 40, allowedSlots: ['weapon'] },

  // === PERCENT DEFENSE (armor, offhand) ===
  { id: 'pre_reinforced', name: '增幅的', nameEn: 'Reinforced', type: 'prefix', tier: 2, stat: 'defensePercent', minValue: 10, maxValue: 20, levelReq: 10, allowedSlots: ['helmet', 'armor', 'offhand'] },
  { id: 'pre_indestructible', name: '不灭的', nameEn: 'Indestructible', type: 'prefix', tier: 4, stat: 'defensePercent', minValue: 30, maxValue: 50, levelReq: 30, allowedSlots: ['helmet', 'armor', 'offhand'] },
];

export const Suffixes: AffixDefinition[] = [
  // === LIFE / MANA (any slot) ===
  { id: 'suf_life', name: '生命', nameEn: 'of Life', type: 'suffix', tier: 1, stat: 'maxHp', minValue: 5, maxValue: 15, levelReq: 1 },
  { id: 'suf_vitality', name: '活力', nameEn: 'of Vitality', type: 'suffix', tier: 2, stat: 'maxHp', minValue: 20, maxValue: 40, levelReq: 10 },
  { id: 'suf_titan_life', name: '巨人生命', nameEn: 'of the Titan', type: 'suffix', tier: 3, stat: 'maxHp', minValue: 50, maxValue: 80, levelReq: 20 },
  { id: 'suf_immortal', name: '不朽', nameEn: 'of Immortality', type: 'suffix', tier: 4, stat: 'maxHp', minValue: 100, maxValue: 160, levelReq: 30 },
  { id: 'suf_mana', name: '法力', nameEn: 'of Mana', type: 'suffix', tier: 1, stat: 'maxMana', minValue: 3, maxValue: 10, levelReq: 1 },
  { id: 'suf_energy', name: '魔能', nameEn: 'of Energy', type: 'suffix', tier: 2, stat: 'maxMana', minValue: 15, maxValue: 30, levelReq: 10 },

  // === PERCENT LIFE / MANA (belt, armor, necklace) ===
  { id: 'suf_vigor', name: '精力', nameEn: 'of Vigor', type: 'suffix', tier: 2, stat: 'maxHpPercent', minValue: 5, maxValue: 10, levelReq: 10, allowedSlots: ['belt', 'armor', 'necklace'] },
  { id: 'suf_colossus', name: '巨像', nameEn: 'of the Colossus', type: 'suffix', tier: 4, stat: 'maxHpPercent', minValue: 15, maxValue: 25, levelReq: 30, allowedSlots: ['belt', 'armor', 'necklace'] },
  { id: 'suf_brilliance', name: '光辉', nameEn: 'of Brilliance', type: 'suffix', tier: 3, stat: 'maxManaPercent', minValue: 10, maxValue: 20, levelReq: 20, allowedSlots: ['helmet', 'necklace', 'ring1', 'ring2'] },

  // === LIFE STEAL / MANA STEAL (weapon, ring) ===
  { id: 'suf_leech', name: '吸血', nameEn: 'of Leech', type: 'suffix', tier: 1, stat: 'lifeSteal', minValue: 1, maxValue: 3, levelReq: 1, allowedSlots: ['weapon', 'ring1', 'ring2'] },
  { id: 'suf_bloodthirst', name: '嗜血', nameEn: 'of Bloodthirst', type: 'suffix', tier: 3, stat: 'lifeSteal', minValue: 4, maxValue: 8, levelReq: 20, allowedSlots: ['weapon', 'ring1', 'ring2'] },
  { id: 'suf_manaleech', name: '噬魔', nameEn: 'of Sorcery', type: 'suffix', tier: 2, stat: 'manaSteal', minValue: 2, maxValue: 5, levelReq: 10, allowedSlots: ['weapon', 'ring1', 'ring2'] },

  // === REGEN (belt, ring, necklace) ===
  { id: 'suf_regen', name: '恢复', nameEn: 'of Regeneration', type: 'suffix', tier: 1, stat: 'hpRegen', minValue: 1, maxValue: 3, levelReq: 1, allowedSlots: ['belt', 'ring1', 'ring2', 'necklace'] },
  { id: 'suf_rejuv', name: '焕新', nameEn: 'of Rejuvenation', type: 'suffix', tier: 3, stat: 'hpRegen', minValue: 5, maxValue: 10, levelReq: 20, allowedSlots: ['belt', 'ring1', 'ring2', 'necklace'] },
  { id: 'suf_meditation', name: '冥想', nameEn: 'of Meditation', type: 'suffix', tier: 2, stat: 'manaRegen', minValue: 2, maxValue: 5, levelReq: 10, allowedSlots: ['helmet', 'ring1', 'ring2', 'necklace'] },

  // === CRIT (weapon, gloves, ring, necklace) ===
  { id: 'suf_crit', name: '暴击', nameEn: 'of Precision', type: 'suffix', tier: 2, stat: 'critRate', minValue: 3, maxValue: 6, levelReq: 10, allowedSlots: ['weapon', 'gloves', 'ring1', 'ring2', 'necklace'] },
  { id: 'suf_crit_high', name: '精准', nameEn: 'of Accuracy', type: 'suffix', tier: 4, stat: 'critRate', minValue: 8, maxValue: 12, levelReq: 30, allowedSlots: ['weapon', 'gloves', 'ring1', 'ring2', 'necklace'] },
  { id: 'suf_devastation', name: '毁灭', nameEn: 'of Devastation', type: 'suffix', tier: 3, stat: 'critDamage', minValue: 10, maxValue: 25, levelReq: 20, allowedSlots: ['weapon', 'gloves', 'necklace'] },
  { id: 'suf_oblivion', name: '湮灭', nameEn: 'of Oblivion', type: 'suffix', tier: 5, stat: 'critDamage', minValue: 30, maxValue: 50, levelReq: 40, allowedSlots: ['weapon', 'gloves', 'necklace'] },

  // === ELEMENTAL DAMAGE (weapon, ring, necklace) ===
  { id: 'suf_flame', name: '火焰', nameEn: 'of Flame', type: 'suffix', tier: 1, stat: 'fireDamage', minValue: 1, maxValue: 4, levelReq: 1, allowedSlots: ['weapon', 'ring1', 'ring2', 'necklace'] },
  { id: 'suf_inferno', name: '地狱火', nameEn: 'of Inferno', type: 'suffix', tier: 2, stat: 'fireDamage', minValue: 5, maxValue: 12, levelReq: 10, allowedSlots: ['weapon', 'ring1', 'ring2', 'necklace'] },
  { id: 'suf_frost', name: '冰霜', nameEn: 'of Frost', type: 'suffix', tier: 1, stat: 'iceDamage', minValue: 1, maxValue: 4, levelReq: 1, allowedSlots: ['weapon', 'ring1', 'ring2', 'necklace'] },
  { id: 'suf_blizzard', name: '暴风雪', nameEn: 'of the Blizzard', type: 'suffix', tier: 3, stat: 'iceDamage', minValue: 8, maxValue: 18, levelReq: 20, allowedSlots: ['weapon', 'ring1', 'ring2', 'necklace'] },
  { id: 'suf_spark', name: '雷击', nameEn: 'of Shock', type: 'suffix', tier: 1, stat: 'lightningDamage', minValue: 1, maxValue: 5, levelReq: 1, allowedSlots: ['weapon', 'ring1', 'ring2', 'necklace'] },
  { id: 'suf_thunder', name: '雷霆', nameEn: 'of Thunder', type: 'suffix', tier: 3, stat: 'lightningDamage', minValue: 6, maxValue: 16, levelReq: 20, allowedSlots: ['weapon', 'ring1', 'ring2', 'necklace'] },
  { id: 'suf_venom', name: '剧毒', nameEn: 'of Venom', type: 'suffix', tier: 2, stat: 'poisonDamage', minValue: 3, maxValue: 8, levelReq: 10, allowedSlots: ['weapon', 'ring1', 'ring2', 'necklace'] },

  // === ELEMENTAL RESIST (helmet, armor, boots, belt, offhand, ring) ===
  { id: 'suf_fire_res', name: '抗火', nameEn: 'of Fire Resistance', type: 'suffix', tier: 1, stat: 'fireResist', minValue: 5, maxValue: 10, levelReq: 1, allowedSlots: ['helmet', 'armor', 'boots', 'belt', 'offhand', 'ring1', 'ring2'] },
  { id: 'suf_fire_res2', name: '烈焰庇护', nameEn: 'of Fire Ward', type: 'suffix', tier: 3, stat: 'fireResist', minValue: 15, maxValue: 25, levelReq: 20, allowedSlots: ['helmet', 'armor', 'boots', 'belt', 'offhand', 'ring1', 'ring2'] },
  { id: 'suf_ice_res', name: '抗冰', nameEn: 'of Cold Resistance', type: 'suffix', tier: 1, stat: 'iceResist', minValue: 5, maxValue: 10, levelReq: 1, allowedSlots: ['helmet', 'armor', 'boots', 'belt', 'offhand', 'ring1', 'ring2'] },
  { id: 'suf_ice_res2', name: '冰霜庇护', nameEn: 'of Cold Ward', type: 'suffix', tier: 3, stat: 'iceResist', minValue: 15, maxValue: 25, levelReq: 20, allowedSlots: ['helmet', 'armor', 'boots', 'belt', 'offhand', 'ring1', 'ring2'] },
  { id: 'suf_lightning_res', name: '抗雷', nameEn: 'of Lightning Resistance', type: 'suffix', tier: 1, stat: 'lightningResist', minValue: 5, maxValue: 10, levelReq: 1, allowedSlots: ['helmet', 'armor', 'boots', 'belt', 'offhand', 'ring1', 'ring2'] },
  { id: 'suf_poison_res', name: '抗毒', nameEn: 'of Poison Resistance', type: 'suffix', tier: 2, stat: 'poisonResist', minValue: 8, maxValue: 15, levelReq: 10, allowedSlots: ['helmet', 'armor', 'boots', 'belt', 'offhand', 'ring1', 'ring2'] },
  { id: 'suf_all_res', name: '全抗', nameEn: 'of the Zodiac', type: 'suffix', tier: 4, stat: 'allResist', minValue: 5, maxValue: 12, levelReq: 30, allowedSlots: ['helmet', 'armor', 'offhand', 'ring1', 'ring2', 'necklace'] },

  // === MOVE SPEED (boots only) ===
  { id: 'suf_speed', name: '疾行', nameEn: 'of Speed', type: 'suffix', tier: 1, stat: 'moveSpeed', minValue: 5, maxValue: 10, levelReq: 1, allowedSlots: ['boots'] },
  { id: 'suf_haste', name: '神速', nameEn: 'of Haste', type: 'suffix', tier: 3, stat: 'moveSpeed', minValue: 15, maxValue: 25, levelReq: 20, allowedSlots: ['boots'] },

  // === MAGIC FIND / EXP (any slot) ===
  { id: 'suf_luck', name: '幸运', nameEn: 'of Fortune', type: 'suffix', tier: 1, stat: 'lck', minValue: 2, maxValue: 4, levelReq: 1 },
  { id: 'suf_fortune', name: '财运', nameEn: 'of Wealth', type: 'suffix', tier: 2, stat: 'magicFind', minValue: 5, maxValue: 15, levelReq: 10, allowedSlots: ['helmet', 'boots', 'ring1', 'ring2', 'necklace'] },
  { id: 'suf_exp', name: '历练', nameEn: 'of Experience', type: 'suffix', tier: 2, stat: 'expBonus', minValue: 3, maxValue: 8, levelReq: 10, allowedSlots: ['helmet', 'necklace', 'ring1', 'ring2'] },

  // === COOLDOWN REDUCTION (helmet, weapon, necklace) ===
  { id: 'suf_honing', name: '磨砺', nameEn: 'of Honing', type: 'suffix', tier: 3, stat: 'cooldownReduction', minValue: 5, maxValue: 10, levelReq: 20, allowedSlots: ['helmet', 'weapon', 'necklace'] },
];

export const AllAffixes: AffixDefinition[] = [...Prefixes, ...Suffixes];

/** Stat key -> Chinese display name and whether it's a percent stat */
export const STAT_DISPLAY: Record<string, { label: string; isPercent: boolean }> = {
  damage: { label: '伤害', isPercent: false },
  damagePercent: { label: '伤害', isPercent: true },
  defense: { label: '防御', isPercent: false },
  defensePercent: { label: '防御', isPercent: true },
  str: { label: '力量', isPercent: false },
  dex: { label: '敏捷', isPercent: false },
  int: { label: '智力', isPercent: false },
  vit: { label: '体力', isPercent: false },
  spi: { label: '精神', isPercent: false },
  lck: { label: '幸运', isPercent: false },
  maxHp: { label: '生命', isPercent: false },
  maxHpPercent: { label: '生命', isPercent: true },
  maxMana: { label: '法力', isPercent: false },
  maxManaPercent: { label: '法力', isPercent: true },
  lifeSteal: { label: '生命偷取', isPercent: true },
  manaSteal: { label: '法力偷取', isPercent: true },
  hpRegen: { label: '生命回复/秒', isPercent: false },
  manaRegen: { label: '法力回复/秒', isPercent: false },
  critRate: { label: '暴击率', isPercent: true },
  critDamage: { label: '暴击伤害', isPercent: true },
  attackSpeed: { label: '攻击速度', isPercent: true },
  fireDamage: { label: '火焰伤害', isPercent: false },
  iceDamage: { label: '冰霜伤害', isPercent: false },
  lightningDamage: { label: '闪电伤害', isPercent: false },
  poisonDamage: { label: '毒素伤害', isPercent: false },
  fireResist: { label: '火焰抗性', isPercent: true },
  iceResist: { label: '冰霜抗性', isPercent: true },
  lightningResist: { label: '闪电抗性', isPercent: true },
  poisonResist: { label: '毒素抗性', isPercent: true },
  allResist: { label: '全部抗性', isPercent: true },
  moveSpeed: { label: '移动速度', isPercent: true },
  magicFind: { label: '掉宝率', isPercent: true },
  expBonus: { label: '经验加成', isPercent: true },
  cooldownReduction: { label: '冷却缩减', isPercent: true },
  knockback: { label: '击退', isPercent: false },
};
