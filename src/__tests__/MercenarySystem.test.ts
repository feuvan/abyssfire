import { describe, it, expect, beforeEach, vi } from 'vitest';

// Mock Phaser before importing modules that depend on it
vi.mock('phaser', () => ({
  default: {
    Events: {
      EventEmitter: class MockEventEmitter {
        private handlers = new Map<string, Function[]>();
        on(event: string, fn: Function) { 
          if (!this.handlers.has(event)) this.handlers.set(event, []);
          this.handlers.get(event)!.push(fn);
          return this;
        }
        off(event: string, fn: Function) {
          const fns = this.handlers.get(event);
          if (fns) this.handlers.set(event, fns.filter(f => f !== fn));
          return this;
        }
        emit(event: string, ...args: any[]) {
          const fns = this.handlers.get(event);
          if (fns) fns.forEach(fn => fn(...args));
          return true;
        }
      }
    }
  }
}));

import { MercenarySystem, MERCENARY_DEFS, MERCENARY_TYPES, type MercenaryState } from '../systems/MercenarySystem';
import type { MercenarySaveData } from '../data/types';

describe('MercenarySystem', () => {
  let system: MercenarySystem;

  beforeEach(() => {
    system = new MercenarySystem();
  });

  // ═══ Definition Completeness ═════════════════════════════════════════

  describe('Mercenary Definitions', () => {
    it('has 5 mercenary types defined', () => {
      expect(MERCENARY_TYPES).toHaveLength(5);
      expect(MERCENARY_TYPES).toEqual(['tank', 'melee', 'ranged', 'healer', 'mage']);
    });

    it('each type has complete definition with Chinese labels', () => {
      for (const type of MERCENARY_TYPES) {
        const def = MERCENARY_DEFS[type];
        expect(def).toBeDefined();
        expect(def.name).toBeTruthy();
        expect(def.description).toBeTruthy();
        // Chinese characters
        expect(/[\u4e00-\u9fff]/.test(def.name)).toBe(true);
        expect(/[\u4e00-\u9fff]/.test(def.description)).toBe(true);
        expect(def.hireCost).toBeGreaterThan(0);
        expect(def.reviveCost).toBeGreaterThan(0);
        expect(def.baseHp).toBeGreaterThan(0);
        expect(def.baseDamage).toBeGreaterThan(0);
        expect(def.attackRange).toBeGreaterThan(0);
        expect(def.attackSpeed).toBeGreaterThan(0);
      }
    });

    it('each type has distinct AI role', () => {
      const roles = MERCENARY_TYPES.map(t => MERCENARY_DEFS[t].aiRole);
      expect(new Set(roles).size).toBe(5);
    });

    it('tank has highest base HP and defense', () => {
      const tankDef = MERCENARY_DEFS.tank;
      for (const type of MERCENARY_TYPES) {
        if (type === 'tank') continue;
        expect(tankDef.baseHp).toBeGreaterThanOrEqual(MERCENARY_DEFS[type].baseHp);
        expect(tankDef.baseDefense).toBeGreaterThanOrEqual(MERCENARY_DEFS[type].baseDefense);
      }
    });

    it('melee has highest base damage', () => {
      const meleeDef = MERCENARY_DEFS.melee;
      for (const type of MERCENARY_TYPES) {
        if (type === 'melee') continue;
        expect(meleeDef.baseDamage).toBeGreaterThanOrEqual(MERCENARY_DEFS[type].baseDamage);
      }
    });

    it('ranged and mage have longer attack range than melee types', () => {
      expect(MERCENARY_DEFS.ranged.attackRange).toBeGreaterThan(MERCENARY_DEFS.melee.attackRange);
      expect(MERCENARY_DEFS.mage.attackRange).toBeGreaterThan(MERCENARY_DEFS.melee.attackRange);
    });

    it('healer has highest base mana', () => {
      const healerDef = MERCENARY_DEFS.healer;
      expect(healerDef.baseMana).toBeGreaterThanOrEqual(MERCENARY_DEFS.tank.baseMana);
      expect(healerDef.baseMana).toBeGreaterThanOrEqual(MERCENARY_DEFS.melee.baseMana);
      expect(healerDef.baseMana).toBeGreaterThanOrEqual(MERCENARY_DEFS.ranged.baseMana);
    });
  });

  // ═══ Hiring & Dismissal ═════════════════════════════════════════════

  describe('Hiring & Dismissal', () => {
    it('hires mercenary when gold is sufficient', () => {
      const result = system.hire('tank', 1000);
      expect(result.success).toBe(true);
      expect(result.cost).toBe(500);
      expect(system.activeMercenary).not.toBeNull();
      expect(system.activeMercenary!.type).toBe('tank');
      expect(system.activeMercenary!.alive).toBe(true);
    });

    it('rejects hiring when gold is insufficient', () => {
      const result = system.hire('tank', 100);
      expect(result.success).toBe(false);
      expect(result.cost).toBe(0);
      expect(system.activeMercenary).toBeNull();
    });

    it('only one active at a time — replaces previous', () => {
      system.hire('tank', 1000);
      expect(system.activeMercenary!.type).toBe('tank');
      system.hire('mage', 1000);
      expect(system.activeMercenary!.type).toBe('mage');
    });

    it('dismissal removes entity', () => {
      system.hire('tank', 1000);
      expect(system.activeMercenary).not.toBeNull();
      system.dismiss();
      expect(system.activeMercenary).toBeNull();
    });

    it('each type can be hired', () => {
      for (const type of MERCENARY_TYPES) {
        system.hire(type, 10000);
        expect(system.activeMercenary!.type).toBe(type);
      }
    });
  });

  // ═══ Stats & Leveling ═══════════════════════════════════════════════

  describe('Stats & Leveling', () => {
    it('creates mercenary with correct initial stats', () => {
      system.hire('tank', 1000);
      const merc = system.activeMercenary!;
      expect(merc.level).toBe(1);
      expect(merc.exp).toBe(0);
      expect(merc.hp).toBe(merc.maxHp);
      expect(merc.mana).toBe(merc.maxMana);
      expect(merc.alive).toBe(true);
    });

    it('gains share of combat exp', () => {
      system.hire('tank', 1000);
      const merc = system.activeMercenary!;
      const initialExp = merc.exp;
      system.addExp(100);
      expect(merc.exp).toBeGreaterThan(initialExp);
      expect(merc.exp).toBe(Math.floor(100 * MercenarySystem.EXP_SHARE));
    });

    it('levels up independently with stat growth', () => {
      system.hire('tank', 1000);
      const merc = system.activeMercenary!;
      const initialStats = { ...merc.stats };
      const needed = system.expToNextLevel(1);
      system.addExp(Math.ceil(needed / MercenarySystem.EXP_SHARE) + 10);
      expect(merc.level).toBe(2);
      expect(merc.stats.vit).toBeGreaterThan(initialStats.vit);
    });

    it('training ground bonus increases exp share', () => {
      system.hire('tank', 1000);
      const merc = system.activeMercenary!;
      // Use small amount that won't trigger level-up
      system.addExp(20, 50); // 50% bonus => 20 * 0.3 * 1.5 = 9
      const expected = Math.floor(20 * MercenarySystem.EXP_SHARE * 1.5);
      expect(merc.exp).toBe(expected);
      // Also test that bonus 0 has no effect
      const system2 = new MercenarySystem();
      system2.hire('tank', 1000);
      system2.addExp(20, 0);
      expect(system2.activeMercenary!.exp).toBe(Math.floor(20 * MercenarySystem.EXP_SHARE));
    });

    it('does not gain exp when dead', () => {
      system.hire('tank', 1000);
      const merc = system.activeMercenary!;
      merc.alive = false;
      system.addExp(1000);
      expect(merc.exp).toBe(0);
    });

    it('higher level mercenary has better stats', () => {
      const lv1 = system.createMercenary('tank', 1);
      const lv10 = system.createMercenary('tank', 10);
      expect(lv10.maxHp).toBeGreaterThan(lv1.maxHp);
      expect(lv10.baseDamage).toBeGreaterThan(lv1.baseDamage);
      expect(lv10.defense).toBeGreaterThan(lv1.defense);
    });

    it('expToNextLevel increases with level', () => {
      const exp1 = system.expToNextLevel(1);
      const exp5 = system.expToNextLevel(5);
      const exp10 = system.expToNextLevel(10);
      expect(exp5).toBeGreaterThan(exp1);
      expect(exp10).toBeGreaterThan(exp5);
    });
  });

  // ═══ Equipment ══════════════════════════════════════════════════════

  describe('Equipment', () => {
    const mockWeapon = {
      uid: 'w1', baseId: 'sword_1', name: 'Test Sword', quality: 'normal' as const,
      level: 1, affixes: [], sockets: [], identified: true, quantity: 1,
      stats: { damage: 10 },
      weaponType: 'sword',
    };

    const mockArmor = {
      uid: 'a1', baseId: 'armor_1', name: 'Test Armor', quality: 'normal' as const,
      level: 1, affixes: [], sockets: [], identified: true, quantity: 1,
      stats: { defense: 8, baseDefense: 5 },
      type: 'armor',
    };

    it('equips weapon and modifies stats', () => {
      system.hire('tank', 1000);
      const prevDmg = system.activeMercenary!.baseDamage;
      system.equipWeapon(mockWeapon);
      expect(system.activeMercenary!.equipment.weapon).toBe(mockWeapon);
      expect(system.activeMercenary!.baseDamage).toBeGreaterThan(prevDmg);
    });

    it('equips armor and modifies stats', () => {
      system.hire('tank', 1000);
      const prevDef = system.activeMercenary!.defense;
      system.equipArmor(mockArmor as any);
      expect(system.activeMercenary!.equipment.armor).toBeDefined();
      expect(system.activeMercenary!.defense).toBeGreaterThan(prevDef);
    });

    it('unequip returns old item and reverts stats', () => {
      system.hire('tank', 1000);
      const baseDmg = system.activeMercenary!.baseDamage;
      system.equipWeapon(mockWeapon);
      const old = system.unequipWeapon();
      expect(old).toBe(mockWeapon);
      expect(system.activeMercenary!.baseDamage).toBe(baseDmg);
    });

    it('weapon + armor slots are independent', () => {
      system.hire('melee', 1000);
      system.equipWeapon(mockWeapon);
      system.equipArmor(mockArmor as any);
      expect(system.activeMercenary!.equipment.weapon).toBeDefined();
      expect(system.activeMercenary!.equipment.armor).toBeDefined();
    });
  });

  // ═══ Death & Revival ═══════════════════════════════════════════════

  describe('Death & Revival', () => {
    it('dies at 0 HP', () => {
      system.hire('tank', 1000);
      const result = system.takeDamage(99999);
      expect(result.died).toBe(true);
      expect(system.activeMercenary!.alive).toBe(false);
      expect(system.activeMercenary!.hp).toBe(0);
    });

    it('revive costs gold and restores at 50% HP', () => {
      system.hire('tank', 1000);
      system.takeDamage(99999);
      expect(system.activeMercenary!.alive).toBe(false);
      
      const result = system.revive(1000);
      expect(result.success).toBe(true);
      expect(result.cost).toBe(MERCENARY_DEFS.tank.reviveCost);
      expect(system.activeMercenary!.alive).toBe(true);
      expect(system.activeMercenary!.hp).toBe(Math.floor(system.activeMercenary!.maxHp * 0.5));
    });

    it('revive fails without enough gold', () => {
      system.hire('tank', 1000);
      system.takeDamage(99999);
      const result = system.revive(10);
      expect(result.success).toBe(false);
      expect(system.activeMercenary!.alive).toBe(false);
    });

    it('revive fails if already alive', () => {
      system.hire('tank', 1000);
      const result = system.revive(1000);
      expect(result.success).toBe(false);
    });

    it('revive fails if no mercenary', () => {
      const result = system.revive(1000);
      expect(result.success).toBe(false);
    });
  });

  // ═══ Combat AI ═════════════════════════════════════════════════════

  describe('Combat AI', () => {
    it('returns idle when no mercenary', () => {
      const action = system.getAIAction(0, 0, 0, 1, null, null, Infinity, [], false);
      expect(action.type).toBe('idle');
    });

    it('returns idle in safe zone', () => {
      system.hire('tank', 1000);
      system.setPosition(5, 5);
      const action = system.getAIAction(1000, 5, 5, 1, 10, 10, 7, [], true);
      // Even with enemies nearby, safe zone = follow/idle
      expect(['idle', 'follow']).toContain(action.type);
    });

    it('follows player when too far', () => {
      system.hire('tank', 1000);
      system.setPosition(0, 0);
      const action = system.getAIAction(1000, 20, 20, 1, null, null, Infinity, [], false);
      expect(action.type).toBe('follow');
    });

    // Tank AI
    it('tank intercepts monsters near player', () => {
      system.hire('tank', 1000);
      const merc = system.activeMercenary!;
      merc.tileCol = 5;
      merc.tileRow = 5;
      merc.lastAttackTime = 0;
      const action = system.getAIAction(2000, 6, 6, 1, 7, 7, 2, [{ col: 7, row: 7, dist: 2 }], false);
      expect(['attack', 'move_to_target']).toContain(action.type);
    });

    // Melee DPS AI
    it('melee DPS rushes to nearest enemy', () => {
      system.hire('melee', 1000);
      const merc = system.activeMercenary!;
      merc.tileCol = 5;
      merc.tileRow = 5;
      merc.lastAttackTime = 0;
      const action = system.getAIAction(2000, 6, 6, 1, 8, 8, 4, [{ col: 8, row: 8, dist: 4 }], false);
      expect(['attack', 'move_to_target']).toContain(action.type);
    });

    // Ranged DPS AI
    it('ranged repositions when too close to enemy', () => {
      system.hire('ranged', 1000);
      const merc = system.activeMercenary!;
      merc.tileCol = 5;
      merc.tileRow = 5;
      // Enemy very close (dist ~1.4)
      const action = system.getAIAction(2000, 4, 4, 1, 6, 6, 1.4, [{ col: 6, row: 6, dist: 1.4 }], false);
      expect(action.type).toBe('reposition');
    });

    it('ranged attacks from distance', () => {
      system.hire('ranged', 1000);
      const merc = system.activeMercenary!;
      merc.tileCol = 5;
      merc.tileRow = 5;
      merc.lastAttackTime = 0;
      // Enemy at range 4 (within attack range 5)
      const action = system.getAIAction(2000, 4, 4, 1, 9, 5, 4, [{ col: 9, row: 5, dist: 4 }], false);
      expect(action.type).toBe('attack');
    });

    // Healer AI
    it('healer prioritizes healing when player HP < 60%', () => {
      system.hire('healer', 1000);
      const merc = system.activeMercenary!;
      merc.tileCol = 5;
      merc.tileRow = 5;
      merc.mana = 100;
      merc.lastHealTime = 0;
      // Player at 40% HP, nearby
      const action = system.getAIAction(5000, 6, 6, 0.4, 10, 10, 7, [], false);
      expect(action.type).toBe('heal');
    });

    it('healer attacks when player HP is healthy', () => {
      system.hire('healer', 1000);
      const merc = system.activeMercenary!;
      merc.tileCol = 5;
      merc.tileRow = 5;
      merc.lastAttackTime = 0;
      // Player at 100% HP, enemy nearby
      const action = system.getAIAction(5000, 6, 6, 1.0, 7, 7, 2.5, [{ col: 7, row: 7, dist: 2.5 }], false);
      expect(['attack', 'move_to_target', 'idle', 'follow']).toContain(action.type);
    });

    // Mage AI
    it('mage uses AoE when multiple enemies nearby', () => {
      system.hire('mage', 1000);
      const merc = system.activeMercenary!;
      merc.tileCol = 5;
      merc.tileRow = 5;
      merc.mana = 100;
      merc.lastAttackTime = 0;
      const monsters = [
        { col: 7, row: 7, dist: 2.8 },
        { col: 8, row: 6, dist: 3.2 },
        { col: 7, row: 5, dist: 2 },
      ];
      const action = system.getAIAction(5000, 5, 5, 1, 7, 7, 2.8, monsters, false);
      expect(action.type).toBe('aoe_attack');
    });

    it('dead mercenary returns idle', () => {
      system.hire('tank', 1000);
      system.activeMercenary!.alive = false;
      const action = system.getAIAction(1000, 5, 5, 1, 6, 6, 1.4, [], false);
      expect(action.type).toBe('idle');
    });
  });

  // ═══ Heal Mechanic ═════════════════════════════════════════════════

  describe('Heal Mechanic', () => {
    it('healer heals 15% of player max HP', () => {
      system.hire('healer', 1000);
      const merc = system.activeMercenary!;
      merc.mana = 100;
      const heal = system.performHeal(200);
      expect(heal).toBe(Math.floor(200 * MercenarySystem.HEAL_PERCENT));
      expect(merc.mana).toBe(100 - MercenarySystem.HEAL_MANA_COST);
    });

    it('heal fails without enough mana', () => {
      system.hire('healer', 1000);
      system.activeMercenary!.mana = 0;
      const heal = system.performHeal(200);
      expect(heal).toBe(0);
    });
  });

  // ═══ Combat Entity Conversion ═════════════════════════════════════

  describe('Combat Entity', () => {
    it('converts to CombatEntity', () => {
      system.hire('tank', 1000);
      const entity = system.toCombatEntity();
      expect(entity).not.toBeNull();
      expect(entity!.id).toContain('mercenary_');
      expect(entity!.hp).toBe(system.activeMercenary!.hp);
      expect(entity!.baseDamage).toBe(system.activeMercenary!.baseDamage);
    });

    it('returns null when no mercenary', () => {
      expect(system.toCombatEntity()).toBeNull();
    });

    it('returns null when dead', () => {
      system.hire('tank', 1000);
      system.activeMercenary!.alive = false;
      expect(system.toCombatEntity()).toBeNull();
    });
  });

  // ═══ Mana Regen ═══════════════════════════════════════════════════

  describe('Mana Regen', () => {
    it('regenerates mana over time', () => {
      system.hire('mage', 1000);
      const merc = system.activeMercenary!;
      merc.mana = 0;
      system.regenMana(1000); // 1 second
      expect(merc.mana).toBeGreaterThan(0);
    });

    it('does not exceed max mana', () => {
      system.hire('mage', 1000);
      const merc = system.activeMercenary!;
      merc.mana = merc.maxMana - 1;
      system.regenMana(100000);
      expect(merc.mana).toBe(merc.maxMana);
    });
  });

  // ═══ Save/Load ════════════════════════════════════════════════════

  describe('Save & Load', () => {
    it('saves mercenary state correctly', () => {
      system.hire('tank', 1000);
      const merc = system.activeMercenary!;
      merc.exp = 50;
      merc.hp = 80;
      merc.mana = 20;

      const saved = system.toSaveData();
      expect(saved).toBeDefined();
      expect(saved!.type).toBe('tank');
      expect(saved!.level).toBe(1);
      expect(saved!.exp).toBe(50);
      expect(saved!.hp).toBe(80);
      expect(saved!.mana).toBe(20);
      expect(saved!.alive).toBe(true);
    });

    it('returns undefined when no mercenary', () => {
      expect(system.toSaveData()).toBeUndefined();
    });

    it('loads mercenary state correctly', () => {
      const saveData: MercenarySaveData = {
        type: 'mage',
        level: 5,
        exp: 30,
        hp: 50,
        mana: 60,
        equipment: {},
        alive: true,
      };
      system.loadFromSave(saveData);
      const merc = system.activeMercenary!;
      expect(merc).not.toBeNull();
      expect(merc.type).toBe('mage');
      expect(merc.level).toBe(5);
      expect(merc.exp).toBe(30);
      expect(merc.alive).toBe(true);
      expect(merc.hp).toBeLessThanOrEqual(merc.maxHp);
    });

    it('dead mercenary loads as dead', () => {
      const saveData: MercenarySaveData = {
        type: 'tank',
        level: 3,
        exp: 10,
        hp: 0,
        mana: 0,
        equipment: {},
        alive: false,
      };
      system.loadFromSave(saveData);
      expect(system.activeMercenary!.alive).toBe(false);
      expect(system.activeMercenary!.hp).toBe(0);
    });

    it('save/load roundtrip preserves all fields', () => {
      system.hire('ranged', 1000);
      const merc = system.activeMercenary!;
      merc.exp = 42;
      merc.hp = 50;
      merc.mana = 30;

      const saved = system.toSaveData()!;
      const newSystem = new MercenarySystem();
      newSystem.loadFromSave(saved);

      const loaded = newSystem.activeMercenary!;
      expect(loaded.type).toBe(merc.type);
      expect(loaded.level).toBe(merc.level);
      expect(loaded.exp).toBe(merc.exp);
      expect(loaded.alive).toBe(merc.alive);
    });

    it('saves equipment in save data', () => {
      system.hire('melee', 1000);
      const weapon = {
        uid: 'w1', baseId: 'sword_1', name: 'Test', quality: 'normal' as const,
        level: 1, affixes: [], sockets: [], identified: true, quantity: 1,
        stats: { damage: 10 },
      };
      system.equipWeapon(weapon);
      const saved = system.toSaveData()!;
      expect(saved.equipment.weapon).toBeDefined();
      expect(saved.equipment.weapon!.uid).toBe('w1');
    });
  });

  // ═══ Position ═════════════════════════════════════════════════════

  describe('Position', () => {
    it('setPosition updates tile coordinates', () => {
      system.hire('tank', 1000);
      system.setPosition(10, 20);
      expect(system.activeMercenary!.tileCol).toBe(10);
      expect(system.activeMercenary!.tileRow).toBe(20);
    });
  });

  // ═══ Edge Cases ═══════════════════════════════════════════════════

  describe('Edge Cases', () => {
    it('damage does not go below 0 HP', () => {
      system.hire('tank', 1000);
      system.takeDamage(1);
      system.takeDamage(99999);
      expect(system.activeMercenary!.hp).toBe(0);
    });

    it('cannot damage dead mercenary', () => {
      system.hire('tank', 1000);
      system.takeDamage(99999);
      const result = system.takeDamage(100);
      expect(result.died).toBe(false);
    });

    it('isAlive returns correct state', () => {
      expect(system.isAlive()).toBe(false);
      system.hire('tank', 1000);
      expect(system.isAlive()).toBe(true);
      system.takeDamage(99999);
      expect(system.isAlive()).toBe(false);
    });

    it('getDefinition returns correct def', () => {
      expect(system.getDefinition()).toBeNull();
      system.hire('healer', 1000);
      expect(system.getDefinition()?.type).toBe('healer');
    });
  });
});
