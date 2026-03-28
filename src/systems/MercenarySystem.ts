import { EventBus, GameEvents } from '../utils/EventBus';
import type { Stats, MercenaryType, MercenaryDefinition, MercenarySaveData, ItemInstance, WeaponBase } from '../data/types';
import type { CombatEntity, ActiveBuff, EquipStats } from './CombatSystem';
import { emptyEquipStats } from './CombatSystem';
import { getItemBase } from '../data/items/bases';
import { t } from '../i18n';

/** Local euclidean distance to avoid importing IsometricUtils (which depends on Phaser config). */
function euclideanDistance(x1: number, y1: number, x2: number, y2: number): number {
  const dx = x2 - x1;
  const dy = y2 - y1;
  return Math.sqrt(dx * dx + dy * dy);
}

// ─── Mercenary Definitions ────────────────────────────────────────────────────

export const MERCENARY_DEFS: Record<MercenaryType, MercenaryDefinition> = {
  tank: {
    type: 'tank',
    name: '守护骑士',
    description: '高生命高防御，吸引敌人仇恨保护队友',
    hireCost: 500,
    reviveCost: 250,
    baseStats: { str: 12, dex: 6, vit: 18, int: 4, spi: 6, lck: 4 },
    statGrowth: { str: 1.5, dex: 0.5, vit: 2.5, int: 0.3, spi: 0.5, lck: 0.3 },
    baseHp: 120,
    baseMana: 30,
    baseDamage: 8,
    baseDefense: 12,
    attackRange: 1.5,
    attackSpeed: 1200,
    aiRole: 'tank',
    allowedWeaponTypes: ['sword', 'mace', 'shield'],
    allowedArmorSlots: ['armor', 'helmet'],
  },
  melee: {
    type: 'melee',
    name: '狂战士',
    description: '高攻击近战输出，造成大量近距离伤害',
    hireCost: 600,
    reviveCost: 300,
    baseStats: { str: 16, dex: 10, vit: 10, int: 4, spi: 4, lck: 6 },
    statGrowth: { str: 2.5, dex: 1.0, vit: 1.0, int: 0.2, spi: 0.3, lck: 0.5 },
    baseHp: 90,
    baseMana: 20,
    baseDamage: 14,
    baseDefense: 6,
    attackRange: 1.5,
    attackSpeed: 800,
    aiRole: 'melee_dps',
    allowedWeaponTypes: ['sword', 'axe', 'dagger'],
    allowedArmorSlots: ['armor'],
  },
  ranged: {
    type: 'ranged',
    name: '游侠射手',
    description: '远程攻击并保持距离，灵活走位',
    hireCost: 550,
    reviveCost: 275,
    baseStats: { str: 8, dex: 16, vit: 8, int: 6, spi: 6, lck: 8 },
    statGrowth: { str: 0.5, dex: 2.5, vit: 0.8, int: 0.5, spi: 0.5, lck: 0.8 },
    baseHp: 75,
    baseMana: 40,
    baseDamage: 12,
    baseDefense: 5,
    attackRange: 5,
    attackSpeed: 900,
    aiRole: 'ranged_dps',
    allowedWeaponTypes: ['bow', 'dagger'],
    allowedArmorSlots: ['armor'],
  },
  healer: {
    type: 'healer',
    name: '圣光牧师',
    description: '优先治疗队友，玩家血量低于60%时全力治疗',
    hireCost: 700,
    reviveCost: 350,
    baseStats: { str: 4, dex: 6, vit: 10, int: 14, spi: 16, lck: 4 },
    statGrowth: { str: 0.3, dex: 0.5, vit: 1.0, int: 1.5, spi: 2.0, lck: 0.3 },
    baseHp: 70,
    baseMana: 80,
    baseDamage: 6,
    baseDefense: 5,
    attackRange: 4,
    attackSpeed: 1500,
    aiRole: 'healer',
    allowedWeaponTypes: ['staff', 'wand'],
    allowedArmorSlots: ['armor'],
  },
  mage: {
    type: 'mage',
    name: '元素法师',
    description: '使用AoE魔法攻击，对群体敌人造成大量伤害',
    hireCost: 650,
    reviveCost: 325,
    baseStats: { str: 4, dex: 6, vit: 8, int: 18, spi: 12, lck: 4 },
    statGrowth: { str: 0.2, dex: 0.5, vit: 0.8, int: 2.5, spi: 1.5, lck: 0.3 },
    baseHp: 65,
    baseMana: 100,
    baseDamage: 10,
    baseDefense: 4,
    attackRange: 5,
    attackSpeed: 1300,
    aiRole: 'aoe_mage',
    allowedWeaponTypes: ['staff', 'wand'],
    allowedArmorSlots: ['armor'],
  },
};

export const MERCENARY_TYPES: MercenaryType[] = ['tank', 'melee', 'ranged', 'healer', 'mage'];

// ─── Mercenary State Class ────────────────────────────────────────────────────

export interface MercenaryState {
  type: MercenaryType;
  level: number;
  exp: number;
  hp: number;
  maxHp: number;
  mana: number;
  maxMana: number;
  stats: Stats;
  baseDamage: number;
  defense: number;
  attackRange: number;
  attackSpeed: number;
  alive: boolean;
  equipment: {
    weapon?: ItemInstance;
    armor?: ItemInstance;
  };
  buffs: ActiveBuff[];
  lastAttackTime: number;
  lastHealTime: number;
  tileCol: number;
  tileRow: number;
}

// ─── MercenarySystem ──────────────────────────────────────────────────────────

export class MercenarySystem {
  activeMercenary: MercenaryState | null = null;

  /** Experience share ratio (mercenary gets this fraction of combat exp). */
  static readonly EXP_SHARE = 0.3;

  /** Follow distance range in tiles. */
  static readonly FOLLOW_MIN = 1;
  static readonly FOLLOW_MAX = 2;

  /** Healer threshold: heal when player HP below this ratio. */
  static readonly HEAL_THRESHOLD = 0.6;

  /** Heal amount as percentage of player max HP. */
  static readonly HEAL_PERCENT = 0.15;

  /** Mana cost for healer heal. */
  static readonly HEAL_MANA_COST = 15;

  /** Tank intercept range: how far tank will pursue aggroed enemies. */
  static readonly TANK_INTERCEPT_RANGE = 6;

  /** Ranged reposition threshold: if closer than this, back up. */
  static readonly RANGED_MIN_DISTANCE = 3;

  /** AoE mage attack range. */
  static readonly MAGE_AOE_RADIUS = 3;

  // ─── Hiring/Dismissal ─────────────────────────────────────────────────

  hire(type: MercenaryType, playerGold: number): { success: boolean; cost: number; message: string } {
    const def = MERCENARY_DEFS[type];
    if (!def) {
      return { success: false, cost: 0, message: t('sys.mercenary.unknownType') };
    }
    if (playerGold < def.hireCost) {
      return { success: false, cost: 0, message: t('sys.mercenary.notEnoughGold', { cost: def.hireCost }) };
    }
    // Dismiss existing mercenary
    if (this.activeMercenary) {
      this.dismiss();
    }
    this.activeMercenary = this.createMercenary(type, 1);
    const hireMsg = t('sys.mercenary.hire', { name: def.name });
    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: hireMsg,
      type: 'system',
    });
    return { success: true, cost: def.hireCost, message: hireMsg };
  }

  dismiss(): void {
    if (!this.activeMercenary) return;
    const def = MERCENARY_DEFS[this.activeMercenary.type];
    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: t('sys.mercenary.dismiss', { name: def.name }),
      type: 'system',
    });
    this.activeMercenary = null;
  }

  revive(playerGold: number): { success: boolean; cost: number; message: string } {
    if (!this.activeMercenary) {
      return { success: false, cost: 0, message: t('sys.mercenary.noMercToRevive') };
    }
    if (this.activeMercenary.alive) {
      return { success: false, cost: 0, message: t('sys.mercenary.mercAlive') };
    }
    const def = MERCENARY_DEFS[this.activeMercenary.type];
    if (playerGold < def.reviveCost) {
      return { success: false, cost: 0, message: t('sys.mercenary.notEnoughGold', { cost: def.reviveCost }) };
    }
    this.activeMercenary.alive = true;
    this.activeMercenary.hp = Math.floor(this.activeMercenary.maxHp * 0.5);
    this.activeMercenary.mana = Math.floor(this.activeMercenary.maxMana * 0.5);
    const reviveMsg = t('sys.mercenary.revive', { name: def.name });
    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: reviveMsg,
      type: 'system',
    });
    return { success: true, cost: def.reviveCost, message: reviveMsg };
  }

  // ─── Creation & Level ─────────────────────────────────────────────────

  createMercenary(type: MercenaryType, level: number): MercenaryState {
    const def = MERCENARY_DEFS[type];
    const stats = { ...def.baseStats };
    // Apply stat growth for level
    for (let i = 1; i < level; i++) {
      stats.str += def.statGrowth.str;
      stats.dex += def.statGrowth.dex;
      stats.vit += def.statGrowth.vit;
      stats.int += def.statGrowth.int;
      stats.spi += def.statGrowth.spi;
      stats.lck += def.statGrowth.lck;
    }
    // Round stats
    stats.str = Math.floor(stats.str);
    stats.dex = Math.floor(stats.dex);
    stats.vit = Math.floor(stats.vit);
    stats.int = Math.floor(stats.int);
    stats.spi = Math.floor(stats.spi);
    stats.lck = Math.floor(stats.lck);

    const maxHp = this.calcMaxHp(def, stats, level);
    const maxMana = this.calcMaxMana(def, stats, level);
    const baseDamage = this.calcBaseDamage(def, stats, level);
    const defense = this.calcDefense(def, stats, level);

    return {
      type,
      level,
      exp: 0,
      hp: maxHp,
      maxHp,
      mana: maxMana,
      maxMana,
      stats,
      baseDamage,
      defense,
      attackRange: def.attackRange,
      attackSpeed: def.attackSpeed,
      alive: true,
      equipment: {},
      buffs: [],
      lastAttackTime: 0,
      lastHealTime: 0,
      tileCol: 0,
      tileRow: 0,
    };
  }

  calcMaxHp(def: MercenaryDefinition, stats: Stats, level: number): number {
    return Math.floor(def.baseHp + stats.vit * 8 + (level - 1) * 12);
  }

  calcMaxMana(def: MercenaryDefinition, stats: Stats, level: number): number {
    return Math.floor(def.baseMana + stats.spi * 5 + stats.int * 2 + (level - 1) * 6);
  }

  calcBaseDamage(def: MercenaryDefinition, stats: Stats, level: number): number {
    return Math.floor(def.baseDamage + stats.str * 0.6 + stats.int * 0.4 + level * 1.5);
  }

  calcDefense(def: MercenaryDefinition, stats: Stats, level: number): number {
    return Math.floor(def.baseDefense + stats.vit * 0.4 + level * 0.8);
  }

  recalcDerived(): void {
    if (!this.activeMercenary) return;
    const merc = this.activeMercenary;
    const def = MERCENARY_DEFS[merc.type];
    merc.maxHp = this.calcMaxHp(def, merc.stats, merc.level);
    merc.maxMana = this.calcMaxMana(def, merc.stats, merc.level);
    merc.baseDamage = this.calcBaseDamage(def, merc.stats, merc.level);
    merc.defense = this.calcDefense(def, merc.stats, merc.level);

    // Equipment bonuses
    const eqStats = this.getEquipmentBonuses();
    merc.baseDamage += eqStats.damage;
    merc.defense += eqStats.defense;
    merc.maxHp += eqStats.maxHp;
    merc.maxMana += eqStats.maxMana;
  }

  expToNextLevel(level: number): number {
    return Math.floor(level * level * 4 + level * 30);
  }

  addExp(amount: number, trainingGroundBonus: number = 0): void {
    if (!this.activeMercenary || !this.activeMercenary.alive) return;
    const merc = this.activeMercenary;
    const share = Math.floor(amount * MercenarySystem.EXP_SHARE * (1 + trainingGroundBonus / 100));
    merc.exp += share;
    const needed = this.expToNextLevel(merc.level);
    if (merc.exp >= needed) {
      merc.exp -= needed;
      merc.level++;
      const def = MERCENARY_DEFS[merc.type];
      // Apply stat growth
      merc.stats.str += def.statGrowth.str;
      merc.stats.dex += def.statGrowth.dex;
      merc.stats.vit += def.statGrowth.vit;
      merc.stats.int += def.statGrowth.int;
      merc.stats.spi += def.statGrowth.spi;
      merc.stats.lck += def.statGrowth.lck;
      // Round
      merc.stats.str = Math.floor(merc.stats.str);
      merc.stats.dex = Math.floor(merc.stats.dex);
      merc.stats.vit = Math.floor(merc.stats.vit);
      merc.stats.int = Math.floor(merc.stats.int);
      merc.stats.spi = Math.floor(merc.stats.spi);
      merc.stats.lck = Math.floor(merc.stats.lck);

      this.recalcDerived();
      merc.hp = merc.maxHp;
      merc.mana = merc.maxMana;
      EventBus.emit(GameEvents.LOG_MESSAGE, {
        text: t('sys.mercenary.levelUp', { name: def.name, level: merc.level }),
        type: 'system',
      });
    }
  }

  // ─── Equipment ────────────────────────────────────────────────────────

  canEquipWeapon(item: ItemInstance): boolean {
    if (!this.activeMercenary) return false;
    const def = MERCENARY_DEFS[this.activeMercenary.type];
    const base = getItemBase(item.baseId);
    if (!base || base.type !== 'weapon') return false;
    const weaponBase = base as WeaponBase;
    if (weaponBase.weaponType) {
      return def.allowedWeaponTypes.includes(weaponBase.weaponType);
    }
    return true;
  }

  canEquipArmor(item: ItemInstance): boolean {
    if (!this.activeMercenary) return false;
    const base = getItemBase(item.baseId);
    if (!base || base.type !== 'armor') return false;
    return true;
  }

  equipWeapon(item: ItemInstance): ItemInstance | null {
    if (!this.activeMercenary) return null;
    const old = this.activeMercenary.equipment.weapon ?? null;
    this.activeMercenary.equipment.weapon = item;
    this.recalcDerived();
    return old;
  }

  equipArmor(item: ItemInstance): ItemInstance | null {
    if (!this.activeMercenary) return null;
    const old = this.activeMercenary.equipment.armor ?? null;
    this.activeMercenary.equipment.armor = item;
    this.recalcDerived();
    return old;
  }

  unequipWeapon(): ItemInstance | null {
    if (!this.activeMercenary) return null;
    const old = this.activeMercenary.equipment.weapon ?? null;
    this.activeMercenary.equipment.weapon = undefined;
    this.recalcDerived();
    return old;
  }

  unequipArmor(): ItemInstance | null {
    if (!this.activeMercenary) return null;
    const old = this.activeMercenary.equipment.armor ?? null;
    this.activeMercenary.equipment.armor = undefined;
    this.recalcDerived();
    return old;
  }

  getEquipmentBonuses(): { damage: number; defense: number; maxHp: number; maxMana: number } {
    const result = { damage: 0, defense: 0, maxHp: 0, maxMana: 0 };
    if (!this.activeMercenary) return result;
    const { weapon, armor } = this.activeMercenary.equipment;
    if (weapon) {
      result.damage += (weapon.stats?.damage as number) ?? 0;
      result.damage += (weapon.stats?.baseDamage as number) ?? 0;
    }
    if (armor) {
      result.defense += (armor.stats?.defense as number) ?? 0;
      result.defense += (armor.stats?.baseDefense as number) ?? 0;
    }
    return result;
  }

  // ─── Combat Entity Conversion ─────────────────────────────────────────

  toCombatEntity(): CombatEntity | null {
    if (!this.activeMercenary || !this.activeMercenary.alive) return null;
    const merc = this.activeMercenary;
    return {
      id: `mercenary_${merc.type}`,
      name: MERCENARY_DEFS[merc.type].name,
      hp: merc.hp,
      maxHp: merc.maxHp,
      mana: merc.mana,
      maxMana: merc.maxMana,
      stats: { ...merc.stats },
      level: merc.level,
      baseDamage: merc.baseDamage,
      defense: merc.defense,
      attackSpeed: merc.attackSpeed,
      attackRange: merc.attackRange,
      buffs: merc.buffs,
      equipStats: emptyEquipStats(),
    };
  }

  // ─── Combat AI ────────────────────────────────────────────────────────

  /**
   * Determine the AI action for the mercenary this tick.
   * Returns an action object describing what the mercenary should do.
   */
  getAIAction(
    time: number,
    playerCol: number,
    playerRow: number,
    playerHpRatio: number,
    nearestMonsterCol: number | null,
    nearestMonsterRow: number | null,
    nearestMonsterDist: number,
    monstersInRange: { col: number; row: number; dist: number }[],
    isInSafeZone: boolean,
  ): MercenaryAIAction {
    if (!this.activeMercenary || !this.activeMercenary.alive) {
      return { type: 'idle' };
    }

    const merc = this.activeMercenary;
    const def = MERCENARY_DEFS[merc.type];

    // In safe zone — no combat
    if (isInSafeZone) {
      return this.getFollowAction(merc, playerCol, playerRow);
    }

    // Follow player if too far
    const distToPlayer = euclideanDistance(merc.tileCol, merc.tileRow, playerCol, playerRow);
    if (distToPlayer > MercenarySystem.FOLLOW_MAX + 3) {
      return { type: 'follow', targetCol: playerCol, targetRow: playerRow };
    }

    // Role-specific AI
    switch (def.aiRole) {
      case 'tank':
        return this.tankAI(merc, time, playerCol, playerRow, monstersInRange, nearestMonsterCol, nearestMonsterRow, nearestMonsterDist);
      case 'melee_dps':
        return this.meleeDpsAI(merc, time, nearestMonsterCol, nearestMonsterRow, nearestMonsterDist, playerCol, playerRow);
      case 'ranged_dps':
        return this.rangedDpsAI(merc, time, nearestMonsterCol, nearestMonsterRow, nearestMonsterDist, playerCol, playerRow);
      case 'healer':
        return this.healerAI(merc, time, playerCol, playerRow, playerHpRatio, nearestMonsterCol, nearestMonsterRow, nearestMonsterDist);
      case 'aoe_mage':
        return this.mageAI(merc, time, monstersInRange, nearestMonsterCol, nearestMonsterRow, nearestMonsterDist, playerCol, playerRow);
      default:
        return this.getFollowAction(merc, playerCol, playerRow);
    }
  }

  private tankAI(
    merc: MercenaryState,
    time: number,
    playerCol: number,
    playerRow: number,
    monstersInRange: { col: number; row: number; dist: number }[],
    nearestCol: number | null,
    nearestRow: number | null,
    nearestDist: number,
  ): MercenaryAIAction {
    // Tank intercepts closest monster to player within range
    if (nearestCol !== null && nearestRow !== null && nearestDist <= MercenarySystem.TANK_INTERCEPT_RANGE) {
      const distToTarget = euclideanDistance(merc.tileCol, merc.tileRow, nearestCol, nearestRow);
      if (distToTarget <= merc.attackRange && time - merc.lastAttackTime >= merc.attackSpeed) {
        return { type: 'attack', targetCol: nearestCol, targetRow: nearestRow };
      }
      return { type: 'move_to_target', targetCol: nearestCol, targetRow: nearestRow };
    }
    return this.getFollowAction(merc, playerCol, playerRow);
  }

  private meleeDpsAI(
    merc: MercenaryState,
    time: number,
    nearestCol: number | null,
    nearestRow: number | null,
    nearestDist: number,
    playerCol: number,
    playerRow: number,
  ): MercenaryAIAction {
    // Melee DPS rushes to nearest enemy and attacks
    if (nearestCol !== null && nearestRow !== null && nearestDist <= 8) {
      const distToTarget = euclideanDistance(merc.tileCol, merc.tileRow, nearestCol, nearestRow);
      if (distToTarget <= merc.attackRange && time - merc.lastAttackTime >= merc.attackSpeed) {
        return { type: 'attack', targetCol: nearestCol, targetRow: nearestRow };
      }
      return { type: 'move_to_target', targetCol: nearestCol, targetRow: nearestRow };
    }
    return this.getFollowAction(merc, playerCol, playerRow);
  }

  private rangedDpsAI(
    merc: MercenaryState,
    time: number,
    nearestCol: number | null,
    nearestRow: number | null,
    nearestDist: number,
    playerCol: number,
    playerRow: number,
  ): MercenaryAIAction {
    if (nearestCol !== null && nearestRow !== null && nearestDist <= 8) {
      const distToTarget = euclideanDistance(merc.tileCol, merc.tileRow, nearestCol, nearestRow);

      // Reposition: if too close, back away
      if (distToTarget < MercenarySystem.RANGED_MIN_DISTANCE) {
        const dx = merc.tileCol - nearestCol;
        const dy = merc.tileRow - nearestRow;
        const len = Math.sqrt(dx * dx + dy * dy) || 1;
        const retreatCol = merc.tileCol + (dx / len) * 2;
        const retreatRow = merc.tileRow + (dy / len) * 2;
        return { type: 'reposition', targetCol: retreatCol, targetRow: retreatRow };
      }

      // Attack from distance
      if (distToTarget <= merc.attackRange && time - merc.lastAttackTime >= merc.attackSpeed) {
        return { type: 'attack', targetCol: nearestCol, targetRow: nearestRow };
      }
      // Move into range
      if (distToTarget > merc.attackRange) {
        return { type: 'move_to_target', targetCol: nearestCol, targetRow: nearestRow };
      }

      return { type: 'idle' };
    }
    return this.getFollowAction(merc, playerCol, playerRow);
  }

  private healerAI(
    merc: MercenaryState,
    time: number,
    playerCol: number,
    playerRow: number,
    playerHpRatio: number,
    nearestCol: number | null,
    nearestRow: number | null,
    nearestDist: number,
  ): MercenaryAIAction {
    // Priority: heal player if below threshold and enough mana
    if (playerHpRatio < MercenarySystem.HEAL_THRESHOLD && merc.mana >= MercenarySystem.HEAL_MANA_COST) {
      const distToPlayer = euclideanDistance(merc.tileCol, merc.tileRow, playerCol, playerRow);
      if (distToPlayer <= merc.attackRange && time - merc.lastHealTime >= 2000) {
        return { type: 'heal', targetCol: playerCol, targetRow: playerRow };
      }
      if (distToPlayer > merc.attackRange) {
        return { type: 'follow', targetCol: playerCol, targetRow: playerRow };
      }
    }
    // Otherwise attack nearest enemy
    if (nearestCol !== null && nearestRow !== null && nearestDist <= 6) {
      const distToTarget = euclideanDistance(merc.tileCol, merc.tileRow, nearestCol, nearestRow);
      if (distToTarget <= merc.attackRange && time - merc.lastAttackTime >= merc.attackSpeed) {
        return { type: 'attack', targetCol: nearestCol, targetRow: nearestRow };
      }
    }
    return this.getFollowAction(merc, playerCol, playerRow);
  }

  private mageAI(
    merc: MercenaryState,
    time: number,
    monstersInRange: { col: number; row: number; dist: number }[],
    nearestCol: number | null,
    nearestRow: number | null,
    nearestDist: number,
    playerCol: number,
    playerRow: number,
  ): MercenaryAIAction {
    // AoE attack if multiple enemies are nearby
    const aoeTargets = monstersInRange.filter(m => m.dist <= MercenarySystem.MAGE_AOE_RADIUS + merc.attackRange);
    if (aoeTargets.length >= 2 && merc.mana >= 10 && time - merc.lastAttackTime >= merc.attackSpeed) {
      // Target center of enemies
      const avgCol = aoeTargets.reduce((s, m) => s + m.col, 0) / aoeTargets.length;
      const avgRow = aoeTargets.reduce((s, m) => s + m.row, 0) / aoeTargets.length;
      return { type: 'aoe_attack', targetCol: avgCol, targetRow: avgRow, radius: MercenarySystem.MAGE_AOE_RADIUS };
    }
    // Single target
    if (nearestCol !== null && nearestRow !== null && nearestDist <= 8) {
      const distToTarget = euclideanDistance(merc.tileCol, merc.tileRow, nearestCol, nearestRow);
      if (distToTarget <= merc.attackRange && time - merc.lastAttackTime >= merc.attackSpeed) {
        return { type: 'attack', targetCol: nearestCol, targetRow: nearestRow };
      }
      if (distToTarget > merc.attackRange) {
        return { type: 'move_to_target', targetCol: nearestCol, targetRow: nearestRow };
      }
    }
    return this.getFollowAction(merc, playerCol, playerRow);
  }

  private getFollowAction(merc: MercenaryState, playerCol: number, playerRow: number): MercenaryAIAction {
    const dist = euclideanDistance(merc.tileCol, merc.tileRow, playerCol, playerRow);
    if (dist > MercenarySystem.FOLLOW_MAX) {
      return { type: 'follow', targetCol: playerCol, targetRow: playerRow };
    }
    return { type: 'idle' };
  }

  // ─── Damage Handling ──────────────────────────────────────────────────

  takeDamage(amount: number): { died: boolean } {
    if (!this.activeMercenary || !this.activeMercenary.alive) return { died: false };
    this.activeMercenary.hp = Math.max(0, this.activeMercenary.hp - amount);
    if (this.activeMercenary.hp <= 0) {
      this.activeMercenary.alive = false;
      const def = MERCENARY_DEFS[this.activeMercenary.type];
      EventBus.emit(GameEvents.LOG_MESSAGE, {
        text: t('sys.mercenary.death', { name: def.name }),
        type: 'combat',
      });
      return { died: true };
    }
    return { died: false };
  }

  performHeal(playerMaxHp: number): number {
    if (!this.activeMercenary || !this.activeMercenary.alive) return 0;
    if (this.activeMercenary.mana < MercenarySystem.HEAL_MANA_COST) return 0;
    this.activeMercenary.mana -= MercenarySystem.HEAL_MANA_COST;
    const healAmount = Math.floor(playerMaxHp * MercenarySystem.HEAL_PERCENT);
    return healAmount;
  }

  // ─── Mana Regen ───────────────────────────────────────────────────────

  regenMana(delta: number): void {
    if (!this.activeMercenary || !this.activeMercenary.alive) return;
    const merc = this.activeMercenary;
    if (merc.mana < merc.maxMana) {
      const regen = (1 + merc.stats.spi * 0.08) * delta / 1000;
      merc.mana = Math.min(merc.maxMana, merc.mana + regen);
    }
  }

  // ─── Save/Load ────────────────────────────────────────────────────────

  toSaveData(): MercenarySaveData | undefined {
    if (!this.activeMercenary) return undefined;
    const merc = this.activeMercenary;
    return {
      type: merc.type,
      level: merc.level,
      exp: merc.exp,
      hp: merc.hp,
      mana: merc.mana,
      equipment: {
        weapon: merc.equipment.weapon,
        armor: merc.equipment.armor,
      },
      alive: merc.alive,
    };
  }

  loadFromSave(data: MercenarySaveData): void {
    const merc = this.createMercenary(data.type, data.level);
    merc.exp = data.exp;
    merc.alive = data.alive;
    merc.equipment = {
      weapon: data.equipment?.weapon,
      armor: data.equipment?.armor,
    };
    this.activeMercenary = merc;
    this.recalcDerived();
    // Restore HP/mana after recalc (respects equipment bonuses)
    merc.hp = data.alive ? Math.min(data.hp, merc.maxHp) : 0;
    merc.mana = Math.min(data.mana, merc.maxMana);
  }

  // ─── Position ─────────────────────────────────────────────────────────

  setPosition(col: number, row: number): void {
    if (this.activeMercenary) {
      this.activeMercenary.tileCol = col;
      this.activeMercenary.tileRow = row;
    }
  }

  getDefinition(): MercenaryDefinition | null {
    if (!this.activeMercenary) return null;
    return MERCENARY_DEFS[this.activeMercenary.type];
  }

  isAlive(): boolean {
    return this.activeMercenary !== null && this.activeMercenary.alive;
  }

  getMercenary(): MercenaryState | null {
    return this.activeMercenary;
  }
}

// ─── Action Types ───────────────────────────────────────────────────────────

export interface MercenaryAIAction {
  type: 'idle' | 'follow' | 'attack' | 'move_to_target' | 'reposition' | 'heal' | 'aoe_attack';
  targetCol?: number;
  targetRow?: number;
  radius?: number;
}
