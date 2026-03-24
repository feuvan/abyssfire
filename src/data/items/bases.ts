import type { WeaponBase, ArmorBase, AccessoryBase, ItemBase } from '../types';

export const Weapons: WeaponBase[] = [
  { id: 'w_rusty_sword', name: '生锈的剑', nameEn: 'Rusty Sword', description: '一把破旧的铁剑', type: 'weapon', slot: 'weapon', icon: 'w_sword', levelReq: 1, sellPrice: 5, stackable: false, maxStack: 1, baseDamage: [3, 7], attackSpeed: 1000, weaponType: 'sword', sockets: 0 },
  { id: 'w_short_sword', name: '短剑', nameEn: 'Short Sword', description: '轻便的短剑', type: 'weapon', slot: 'weapon', icon: 'w_sword', levelReq: 3, sellPrice: 15, stackable: false, maxStack: 1, baseDamage: [5, 10], attackSpeed: 900, weaponType: 'sword', sockets: 1 },
  { id: 'w_broad_sword', name: '阔剑', nameEn: 'Broad Sword', description: '厚重的双刃剑', type: 'weapon', slot: 'weapon', icon: 'w_sword', levelReq: 8, sellPrice: 40, stackable: false, maxStack: 1, baseDamage: [10, 18], attackSpeed: 1100, weaponType: 'sword', sockets: 1 },
  { id: 'w_battle_axe', name: '战斧', nameEn: 'Battle Axe', description: '沉重的双手战斧', type: 'weapon', slot: 'weapon', icon: 'w_axe', levelReq: 10, sellPrice: 55, stackable: false, maxStack: 1, baseDamage: [14, 22], attackSpeed: 1300, weaponType: 'axe', sockets: 1 },
  { id: 'w_dagger', name: '匕首', nameEn: 'Dagger', description: '小巧锋利的匕首', type: 'weapon', slot: 'weapon', icon: 'w_dagger', levelReq: 1, sellPrice: 8, stackable: false, maxStack: 1, baseDamage: [2, 6], attackSpeed: 700, weaponType: 'dagger', sockets: 0 },
  { id: 'w_stiletto', name: '细剑', nameEn: 'Stiletto', description: '精致的刺杀用匕首', type: 'weapon', slot: 'weapon', icon: 'w_dagger', levelReq: 6, sellPrice: 30, stackable: false, maxStack: 1, baseDamage: [4, 10], attackSpeed: 650, weaponType: 'dagger', sockets: 1 },
  { id: 'w_short_bow', name: '短弓', nameEn: 'Short Bow', description: '简单的木弓', type: 'weapon', slot: 'weapon', icon: 'w_bow', levelReq: 1, sellPrice: 10, stackable: false, maxStack: 1, baseDamage: [3, 8], attackSpeed: 1100, weaponType: 'bow', sockets: 0 },
  { id: 'w_long_bow', name: '长弓', nameEn: 'Long Bow', description: '强力的长弓', type: 'weapon', slot: 'weapon', icon: 'w_bow', levelReq: 10, sellPrice: 50, stackable: false, maxStack: 1, baseDamage: [8, 16], attackSpeed: 1200, weaponType: 'bow', sockets: 1 },
  { id: 'w_oak_staff', name: '橡木法杖', nameEn: 'Oak Staff', description: '蕴含微弱魔力的法杖', type: 'weapon', slot: 'weapon', icon: 'w_staff', levelReq: 1, sellPrice: 10, stackable: false, maxStack: 1, baseDamage: [4, 9], attackSpeed: 1200, weaponType: 'staff', sockets: 0 },
  { id: 'w_arcane_staff', name: '奥术法杖', nameEn: 'Arcane Staff', description: '闪烁着奥术光芒的法杖', type: 'weapon', slot: 'weapon', icon: 'w_staff', levelReq: 8, sellPrice: 45, stackable: false, maxStack: 1, baseDamage: [8, 16], attackSpeed: 1300, weaponType: 'staff', sockets: 2 },
  { id: 'w_wooden_shield', name: '木盾', nameEn: 'Wooden Shield', description: '简单的木质盾牌', type: 'weapon', slot: 'offhand', icon: 'w_shield', levelReq: 1, sellPrice: 8, stackable: false, maxStack: 1, baseDamage: [0, 0], attackSpeed: 0, weaponType: 'shield', sockets: 0 },
  { id: 'w_iron_shield', name: '铁盾', nameEn: 'Iron Shield', description: '坚固的铁盾', type: 'weapon', slot: 'offhand', icon: 'w_shield', levelReq: 8, sellPrice: 35, stackable: false, maxStack: 1, baseDamage: [0, 0], attackSpeed: 0, weaponType: 'shield', sockets: 1 },
  // Mid-tier weapons (lv 15)
  { id: 'w_militia_sword', name: '民兵剑', nameEn: 'Militia Sword', description: '结实耐用的军用剑', type: 'weapon', slot: 'weapon', icon: 'w_sword', levelReq: 15, sellPrice: 80, stackable: false, maxStack: 1, baseDamage: [12, 20], attackSpeed: 1000, weaponType: 'sword', sockets: 1 },
  { id: 'w_kris', name: '波纹短刀', nameEn: 'Kris', description: '波浪刃的利器', type: 'weapon', slot: 'weapon', icon: 'w_dagger', levelReq: 15, sellPrice: 75, stackable: false, maxStack: 1, baseDamage: [8, 16], attackSpeed: 600, weaponType: 'dagger', sockets: 1 },
  { id: 'w_composite_bow', name: '复合弓', nameEn: 'Composite Bow', description: '层压木制复合弓', type: 'weapon', slot: 'weapon', icon: 'w_bow', levelReq: 15, sellPrice: 78, stackable: false, maxStack: 1, baseDamage: [10, 18], attackSpeed: 1100, weaponType: 'bow', sockets: 1 },
  { id: 'w_rune_staff', name: '符文法杖', nameEn: 'Rune Staff', description: '刻有符文的法杖', type: 'weapon', slot: 'weapon', icon: 'w_staff', levelReq: 15, sellPrice: 82, stackable: false, maxStack: 1, baseDamage: [10, 20], attackSpeed: 1250, weaponType: 'staff', sockets: 1 },
  { id: 'w_tower_shield', name: '塔盾', nameEn: 'Tower Shield', description: '厚重的全身盾牌', type: 'weapon', slot: 'offhand', icon: 'w_shield', levelReq: 15, sellPrice: 70, stackable: false, maxStack: 1, baseDamage: [0, 0], attackSpeed: 0, weaponType: 'shield', sockets: 1 },
  // Higher level weapons (lv 20)
  { id: 'w_claymore', name: '双手巨剑', nameEn: 'Claymore', description: '沉重而致命的巨剑', type: 'weapon', slot: 'weapon', icon: 'w_sword', levelReq: 20, sellPrice: 150, stackable: false, maxStack: 1, baseDamage: [18, 30], attackSpeed: 1400, weaponType: 'sword', sockets: 2 },
  { id: 'w_assassin_blade', name: '刺客之刃', nameEn: 'Assassin Blade', description: '涂毒的弯刀', type: 'weapon', slot: 'weapon', icon: 'w_dagger', levelReq: 20, sellPrice: 140, stackable: false, maxStack: 1, baseDamage: [12, 22], attackSpeed: 600, weaponType: 'dagger', sockets: 2 },
  { id: 'w_war_bow', name: '战争之弓', nameEn: 'War Bow', description: '强力的复合弓', type: 'weapon', slot: 'weapon', icon: 'w_bow', levelReq: 20, sellPrice: 145, stackable: false, maxStack: 1, baseDamage: [14, 24], attackSpeed: 1100, weaponType: 'bow', sockets: 2 },
  { id: 'w_elder_staff', name: '长老法杖', nameEn: 'Elder Staff', description: '蕴含强大魔力', type: 'weapon', slot: 'weapon', icon: 'w_staff', levelReq: 20, sellPrice: 155, stackable: false, maxStack: 1, baseDamage: [16, 28], attackSpeed: 1300, weaponType: 'staff', sockets: 2 },
  // High-tier weapons (lv 28)
  { id: 'w_flamberge', name: '烈焰巨剑', nameEn: 'Flamberge', description: '波浪刃的大型剑', type: 'weapon', slot: 'weapon', icon: 'w_sword', levelReq: 28, sellPrice: 250, stackable: false, maxStack: 1, baseDamage: [25, 42], attackSpeed: 1300, weaponType: 'sword', sockets: 2 },
  { id: 'w_shadow_blade', name: '暗影之刃', nameEn: 'Shadow Blade', description: '隐匿于黑暗的刀刃', type: 'weapon', slot: 'weapon', icon: 'w_dagger', levelReq: 28, sellPrice: 240, stackable: false, maxStack: 1, baseDamage: [16, 30], attackSpeed: 550, weaponType: 'dagger', sockets: 2 },
  { id: 'w_eagle_bow', name: '苍鹰之弓', nameEn: 'Eagle Bow', description: '精准如鹰的利器', type: 'weapon', slot: 'weapon', icon: 'w_bow', levelReq: 28, sellPrice: 245, stackable: false, maxStack: 1, baseDamage: [20, 35], attackSpeed: 1050, weaponType: 'bow', sockets: 2 },
  { id: 'w_lich_staff', name: '巫妖法杖', nameEn: 'Lich Staff', description: '亡灵之力凝聚', type: 'weapon', slot: 'weapon', icon: 'w_staff', levelReq: 28, sellPrice: 255, stackable: false, maxStack: 1, baseDamage: [22, 38], attackSpeed: 1250, weaponType: 'staff', sockets: 2 },
  // Top-tier weapons (lv 35)
  { id: 'w_demon_blade', name: '恶魔之刃', nameEn: 'Demon Blade', description: '散发着黑暗气息', type: 'weapon', slot: 'weapon', icon: 'w_sword', levelReq: 35, sellPrice: 350, stackable: false, maxStack: 1, baseDamage: [32, 52], attackSpeed: 1200, weaponType: 'sword', sockets: 3 },
  { id: 'w_abyssal_staff', name: '深渊法杖', nameEn: 'Abyssal Staff', description: '深渊之力凝聚', type: 'weapon', slot: 'weapon', icon: 'w_staff', levelReq: 35, sellPrice: 360, stackable: false, maxStack: 1, baseDamage: [28, 46], attackSpeed: 1300, weaponType: 'staff', sockets: 3 },
  { id: 'w_abyssal_bow', name: '深渊之弓', nameEn: 'Abyssal Bow', description: '深渊能量注入的弓', type: 'weapon', slot: 'weapon', icon: 'w_bow', levelReq: 35, sellPrice: 355, stackable: false, maxStack: 1, baseDamage: [26, 44], attackSpeed: 1000, weaponType: 'bow', sockets: 3 },
  { id: 'w_abyssal_dagger', name: '深渊匕首', nameEn: 'Abyssal Dagger', description: '黑暗的刺杀利器', type: 'weapon', slot: 'weapon', icon: 'w_dagger', levelReq: 35, sellPrice: 345, stackable: false, maxStack: 1, baseDamage: [22, 38], attackSpeed: 500, weaponType: 'dagger', sockets: 3 },
];

export const Armors: ArmorBase[] = [
  { id: 'a_cloth_cap', name: '布帽', nameEn: 'Cloth Cap', description: '简单的布帽', type: 'armor', slot: 'helmet', icon: 'a_helm', levelReq: 1, sellPrice: 3, stackable: false, maxStack: 1, baseDefense: 2, sockets: 0 },
  { id: 'a_leather_helm', name: '皮盔', nameEn: 'Leather Helm', description: '皮革头盔', type: 'armor', slot: 'helmet', icon: 'a_helm', levelReq: 5, sellPrice: 15, stackable: false, maxStack: 1, baseDefense: 5, sockets: 0 },
  { id: 'a_iron_helm', name: '铁盔', nameEn: 'Iron Helm', description: '铸铁头盔', type: 'armor', slot: 'helmet', icon: 'a_helm', levelReq: 10, sellPrice: 35, stackable: false, maxStack: 1, baseDefense: 10, sockets: 1 },
  { id: 'a_quilted_armor', name: '绗缝甲', nameEn: 'Quilted Armor', description: '简单的布甲', type: 'armor', slot: 'armor', icon: 'a_armor', levelReq: 1, sellPrice: 5, stackable: false, maxStack: 1, baseDefense: 4, sockets: 0 },
  { id: 'a_leather_armor', name: '皮甲', nameEn: 'Leather Armor', description: '柔韧的皮甲', type: 'armor', slot: 'armor', icon: 'a_armor', levelReq: 5, sellPrice: 20, stackable: false, maxStack: 1, baseDefense: 8, sockets: 0 },
  { id: 'a_chain_mail', name: '锁子甲', nameEn: 'Chain Mail', description: '链环编织的铠甲', type: 'armor', slot: 'armor', icon: 'a_armor', levelReq: 10, sellPrice: 50, stackable: false, maxStack: 1, baseDefense: 15, sockets: 1 },
  { id: 'a_plate_armor', name: '板甲', nameEn: 'Plate Armor', description: '厚重的全身板甲', type: 'armor', slot: 'armor', icon: 'a_armor', levelReq: 20, sellPrice: 120, stackable: false, maxStack: 1, baseDefense: 25, sockets: 2 },
  { id: 'a_leather_gloves', name: '皮手套', nameEn: 'Leather Gloves', description: '皮革手套', type: 'armor', slot: 'gloves', icon: 'a_gloves', levelReq: 1, sellPrice: 3, stackable: false, maxStack: 1, baseDefense: 1, sockets: 0 },
  { id: 'a_chain_gloves', name: '锁链手套', nameEn: 'Chain Gloves', description: '链甲手套', type: 'armor', slot: 'gloves', icon: 'a_gloves', levelReq: 10, sellPrice: 25, stackable: false, maxStack: 1, baseDefense: 5, sockets: 1 },
  { id: 'a_leather_boots', name: '皮靴', nameEn: 'Leather Boots', description: '轻便的皮靴', type: 'armor', slot: 'boots', icon: 'a_boots', levelReq: 1, sellPrice: 3, stackable: false, maxStack: 1, baseDefense: 1, sockets: 0 },
  { id: 'a_chain_boots', name: '锁链靴', nameEn: 'Chain Boots', description: '链甲靴子', type: 'armor', slot: 'boots', icon: 'a_boots', levelReq: 10, sellPrice: 25, stackable: false, maxStack: 1, baseDefense: 4, sockets: 0 },
  { id: 'a_leather_belt', name: '皮带', nameEn: 'Leather Belt', description: '简单的皮带', type: 'armor', slot: 'belt', icon: 'a_belt', levelReq: 1, sellPrice: 2, stackable: false, maxStack: 1, baseDefense: 1, sockets: 0 },
  { id: 'a_heavy_belt', name: '重型腰带', nameEn: 'Heavy Belt', description: '宽厚的腰带', type: 'armor', slot: 'belt', icon: 'a_belt', levelReq: 10, sellPrice: 20, stackable: false, maxStack: 1, baseDefense: 3, sockets: 0 },
  // Mid-tier armor (lv 15)
  { id: 'a_full_helm', name: '全盔', nameEn: 'Full Helm', description: '覆盖全脸的头盔', type: 'armor', slot: 'helmet', icon: 'a_helm', levelReq: 15, sellPrice: 55, stackable: false, maxStack: 1, baseDefense: 12, sockets: 1 },
  { id: 'a_scale_mail', name: '鳞甲', nameEn: 'Scale Mail', description: '鳞片编织的铠甲', type: 'armor', slot: 'armor', icon: 'a_armor', levelReq: 15, sellPrice: 65, stackable: false, maxStack: 1, baseDefense: 18, sockets: 1 },
  { id: 'a_gauntlets', name: '铁护手', nameEn: 'Gauntlets', description: '铁制护手', type: 'armor', slot: 'gloves', icon: 'a_gloves', levelReq: 15, sellPrice: 40, stackable: false, maxStack: 1, baseDefense: 6, sockets: 0 },
  { id: 'a_greaves', name: '胫甲', nameEn: 'Greaves', description: '金属胫甲', type: 'armor', slot: 'boots', icon: 'a_boots', levelReq: 15, sellPrice: 40, stackable: false, maxStack: 1, baseDefense: 5, sockets: 0 },
  { id: 'a_war_belt', name: '战斗腰带', nameEn: 'War Belt', description: '镶甲的腰带', type: 'armor', slot: 'belt', icon: 'a_belt', levelReq: 15, sellPrice: 35, stackable: false, maxStack: 1, baseDefense: 4, sockets: 0 },
  // Upper-tier armor (lv 20)
  { id: 'a_heavy_plate_armor', name: '重型板甲', nameEn: 'Heavy Plate Armor', description: '加强型全身板甲', type: 'armor', slot: 'armor', icon: 'a_armor', levelReq: 20, sellPrice: 120, stackable: false, maxStack: 1, baseDefense: 20, sockets: 2 },
  { id: 'a_plate_helm', name: '板甲头盔', nameEn: 'Plate Helm', description: '全板甲头盔', type: 'armor', slot: 'helmet', icon: 'a_helm', levelReq: 20, sellPrice: 90, stackable: false, maxStack: 1, baseDefense: 14, sockets: 1 },
  { id: 'a_plate_gloves', name: '板甲手套', nameEn: 'Plate Gloves', description: '板甲护手', type: 'armor', slot: 'gloves', icon: 'a_gloves', levelReq: 20, sellPrice: 70, stackable: false, maxStack: 1, baseDefense: 7, sockets: 1 },
  { id: 'a_plate_boots', name: '板甲靴', nameEn: 'Plate Boots', description: '重装靴子', type: 'armor', slot: 'boots', icon: 'a_boots', levelReq: 20, sellPrice: 70, stackable: false, maxStack: 1, baseDefense: 6, sockets: 1 },
  { id: 'a_plated_belt', name: '镶板腰带', nameEn: 'Plated Belt', description: '镶钢板的腰带', type: 'armor', slot: 'belt', icon: 'a_belt', levelReq: 20, sellPrice: 50, stackable: false, maxStack: 1, baseDefense: 4, sockets: 0 },
  // Higher tier (lv 30)
  { id: 'a_demon_helm', name: '恶魔头冠', nameEn: 'Demon Crown', description: '恶魔角装饰的头冠', type: 'armor', slot: 'helmet', icon: 'a_helm', levelReq: 30, sellPrice: 200, stackable: false, maxStack: 1, baseDefense: 20, sockets: 2 },
  { id: 'a_demon_armor', name: '恶魔铠甲', nameEn: 'Demon Plate', description: '恶魔骨制铠甲', type: 'armor', slot: 'armor', icon: 'a_armor', levelReq: 30, sellPrice: 280, stackable: false, maxStack: 1, baseDefense: 30, sockets: 2 },
  { id: 'a_demon_gloves', name: '恶魔护手', nameEn: 'Demon Gauntlets', description: '恶魔甲制护手', type: 'armor', slot: 'gloves', icon: 'a_gloves', levelReq: 30, sellPrice: 150, stackable: false, maxStack: 1, baseDefense: 10, sockets: 1 },
  { id: 'a_demon_boots', name: '恶魔战靴', nameEn: 'Demon Greaves', description: '恶魔铠甲战靴', type: 'armor', slot: 'boots', icon: 'a_boots', levelReq: 30, sellPrice: 150, stackable: false, maxStack: 1, baseDefense: 8, sockets: 1 },
  // Top tier (lv 35+)
  { id: 'a_dragon_armor', name: '龙鳞甲', nameEn: 'Dragon Scale', description: '龙鳞打造的铠甲', type: 'armor', slot: 'armor', icon: 'a_armor', levelReq: 35, sellPrice: 350, stackable: false, maxStack: 1, baseDefense: 38, sockets: 3 },
  { id: 'a_dragon_helm', name: '龙角头盔', nameEn: 'Dragon Helm', description: '龙角装饰的头盔', type: 'armor', slot: 'helmet', icon: 'a_helm', levelReq: 35, sellPrice: 300, stackable: false, maxStack: 1, baseDefense: 26, sockets: 2 },
];

export const Accessories: AccessoryBase[] = [
  { id: 'j_copper_ring', name: '铜戒指', nameEn: 'Copper Ring', description: '铜制戒指', type: 'accessory', slot: 'ring1', icon: 'j_ring', levelReq: 1, sellPrice: 5, stackable: false, maxStack: 1 },
  { id: 'j_silver_ring', name: '银戒指', nameEn: 'Silver Ring', description: '银制戒指', type: 'accessory', slot: 'ring1', icon: 'j_ring', levelReq: 10, sellPrice: 25, stackable: false, maxStack: 1 },
  { id: 'j_gold_ring', name: '金戒指', nameEn: 'Gold Ring', description: '金制戒指', type: 'accessory', slot: 'ring1', icon: 'j_ring', levelReq: 20, sellPrice: 60, stackable: false, maxStack: 1 },
  { id: 'j_platinum_ring', name: '白金戒指', nameEn: 'Platinum Ring', description: '白金制戒指', type: 'accessory', slot: 'ring1', icon: 'j_ring', levelReq: 30, sellPrice: 120, stackable: false, maxStack: 1 },
  { id: 'j_bone_amulet', name: '骨项链', nameEn: 'Bone Amulet', description: '骨制项链', type: 'accessory', slot: 'necklace', icon: 'j_amulet', levelReq: 1, sellPrice: 8, stackable: false, maxStack: 1 },
  { id: 'j_jade_amulet', name: '翡翠项链', nameEn: 'Jade Amulet', description: '翡翠项链', type: 'accessory', slot: 'necklace', icon: 'j_amulet', levelReq: 15, sellPrice: 45, stackable: false, maxStack: 1 },
  { id: 'j_arcane_amulet', name: '奥术项链', nameEn: 'Arcane Amulet', description: '散发魔力的项链', type: 'accessory', slot: 'necklace', icon: 'j_amulet', levelReq: 30, sellPrice: 100, stackable: false, maxStack: 1 },
];

export const Consumables: ItemBase[] = [
  { id: 'c_hp_potion_s', name: '小型生命药水', nameEn: 'Minor HP Potion', description: '恢复50生命', type: 'consumable', icon: 'c_hp', levelReq: 1, sellPrice: 5, stackable: true, maxStack: 20 },
  { id: 'c_hp_potion_m', name: '中型生命药水', nameEn: 'HP Potion', description: '恢复150生命', type: 'consumable', icon: 'c_hp', levelReq: 10, sellPrice: 15, stackable: true, maxStack: 20 },
  { id: 'c_hp_potion_l', name: '大型生命药水', nameEn: 'Greater HP Potion', description: '恢复400生命', type: 'consumable', icon: 'c_hp', levelReq: 25, sellPrice: 40, stackable: true, maxStack: 20 },
  { id: 'c_mp_potion_s', name: '小型法力药水', nameEn: 'Minor MP Potion', description: '恢复30法力', type: 'consumable', icon: 'c_mp', levelReq: 1, sellPrice: 5, stackable: true, maxStack: 20 },
  { id: 'c_mp_potion_m', name: '中型法力药水', nameEn: 'MP Potion', description: '恢复80法力', type: 'consumable', icon: 'c_mp', levelReq: 10, sellPrice: 15, stackable: true, maxStack: 20 },
  { id: 'c_antidote', name: '解毒药水', nameEn: 'Antidote', description: '解除毒性状态', type: 'consumable', icon: 'c_antidote', levelReq: 1, sellPrice: 8, stackable: true, maxStack: 10 },
  { id: 'c_tp_scroll', name: '传送卷轴', nameEn: 'TP Scroll', description: '传送回营地', type: 'scroll', icon: 'c_scroll', levelReq: 1, sellPrice: 10, stackable: true, maxStack: 20 },
  { id: 'c_id_scroll', name: '鉴定卷轴', nameEn: 'ID Scroll', description: '鉴定未知装备', type: 'scroll', icon: 'c_scroll', levelReq: 1, sellPrice: 5, stackable: true, maxStack: 20 },
];

export const Gems: ItemBase[] = [
  // Ruby — +STR
  { id: 'g_ruby_1', name: '碎裂红宝石', nameEn: 'Chipped Ruby', description: '+5 力量', type: 'gem', icon: 'g_ruby', levelReq: 1, sellPrice: 10, stackable: true, maxStack: 10 },
  { id: 'g_ruby_2', name: '红宝石', nameEn: 'Ruby', description: '+12 力量', type: 'gem', icon: 'g_ruby', levelReq: 15, sellPrice: 30, stackable: true, maxStack: 10 },
  { id: 'g_ruby_3', name: '完美红宝石', nameEn: 'Perfect Ruby', description: '+20 力量', type: 'gem', icon: 'g_ruby', levelReq: 30, sellPrice: 60, stackable: true, maxStack: 10 },
  // Sapphire — +INT
  { id: 'g_sapphire_1', name: '碎裂蓝宝石', nameEn: 'Chipped Sapphire', description: '+5 智力', type: 'gem', icon: 'g_sapphire', levelReq: 1, sellPrice: 10, stackable: true, maxStack: 10 },
  { id: 'g_sapphire_2', name: '蓝宝石', nameEn: 'Sapphire', description: '+12 智力', type: 'gem', icon: 'g_sapphire', levelReq: 15, sellPrice: 30, stackable: true, maxStack: 10 },
  { id: 'g_sapphire_3', name: '完美蓝宝石', nameEn: 'Perfect Sapphire', description: '+20 智力', type: 'gem', icon: 'g_sapphire', levelReq: 30, sellPrice: 60, stackable: true, maxStack: 10 },
  // Emerald — +DEX
  { id: 'g_emerald_1', name: '碎裂翡翠', nameEn: 'Chipped Emerald', description: '+5 敏捷', type: 'gem', icon: 'g_emerald', levelReq: 1, sellPrice: 10, stackable: true, maxStack: 10 },
  { id: 'g_emerald_2', name: '翡翠', nameEn: 'Emerald', description: '+12 敏捷', type: 'gem', icon: 'g_emerald', levelReq: 15, sellPrice: 30, stackable: true, maxStack: 10 },
  { id: 'g_emerald_3', name: '完美翡翠', nameEn: 'Perfect Emerald', description: '+20 敏捷', type: 'gem', icon: 'g_emerald', levelReq: 30, sellPrice: 60, stackable: true, maxStack: 10 },
  // Topaz — +MF (magicFind)
  { id: 'g_topaz_1', name: '碎裂黄玉', nameEn: 'Chipped Topaz', description: '+5% 掉宝率', type: 'gem', icon: 'g_topaz', levelReq: 1, sellPrice: 10, stackable: true, maxStack: 10 },
  { id: 'g_topaz_2', name: '黄玉', nameEn: 'Topaz', description: '+10% 掉宝率', type: 'gem', icon: 'g_topaz', levelReq: 15, sellPrice: 30, stackable: true, maxStack: 10 },
  { id: 'g_topaz_3', name: '完美黄玉', nameEn: 'Perfect Topaz', description: '+18% 掉宝率', type: 'gem', icon: 'g_topaz', levelReq: 30, sellPrice: 60, stackable: true, maxStack: 10 },
  // Diamond — +all stats
  { id: 'g_diamond_1', name: '碎裂钻石', nameEn: 'Chipped Diamond', description: '+3 全属性', type: 'gem', icon: 'g_diamond', levelReq: 10, sellPrice: 20, stackable: true, maxStack: 10 },
];

/**
 * Gem stat mapping: base ID → { stat, value, tier, name }.
 * Used when socketing a gem to create a GemInstance.
 */
export const GEM_STAT_MAP: Record<string, { stat: string; value: number; tier: number }> = {
  g_ruby_1:    { stat: 'str', value: 5, tier: 1 },
  g_ruby_2:    { stat: 'str', value: 12, tier: 2 },
  g_ruby_3:    { stat: 'str', value: 20, tier: 3 },
  g_sapphire_1: { stat: 'int', value: 5, tier: 1 },
  g_sapphire_2: { stat: 'int', value: 12, tier: 2 },
  g_sapphire_3: { stat: 'int', value: 20, tier: 3 },
  g_emerald_1: { stat: 'dex', value: 5, tier: 1 },
  g_emerald_2: { stat: 'dex', value: 12, tier: 2 },
  g_emerald_3: { stat: 'dex', value: 20, tier: 3 },
  g_topaz_1:   { stat: 'magicFind', value: 5, tier: 1 },
  g_topaz_2:   { stat: 'magicFind', value: 10, tier: 2 },
  g_topaz_3:   { stat: 'magicFind', value: 18, tier: 3 },
  g_diamond_1: { stat: 'allStats', value: 3, tier: 1 },
};

export const AllItemBases: (ItemBase | WeaponBase | ArmorBase | AccessoryBase)[] = [
  ...Weapons, ...Armors, ...Accessories, ...Consumables, ...Gems,
];

export function getItemBase(id: string): ItemBase | WeaponBase | ArmorBase | AccessoryBase | undefined {
  return AllItemBases.find(i => i.id === id);
}
