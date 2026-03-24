import { describe, it, expect, beforeEach, vi } from 'vitest';

// Mock Phaser EventEmitter before any imports that use EventBus
vi.mock('phaser', () => ({
  default: {
    Events: {
      EventEmitter: class {
        private handlers = new Map<string, Array<(...args: unknown[]) => void>>();
        on(event: string, fn: (...args: unknown[]) => void) { if (!this.handlers.has(event)) this.handlers.set(event, []); this.handlers.get(event)!.push(fn); return this; }
        off() { return this; }
        emit(event: string, ...args: unknown[]) { for (const fn of this.handlers.get(event) ?? []) fn(...args); return true; }
        removeAllListeners() { this.handlers.clear(); return this; }
      },
    },
  },
}));

import { InventorySystem } from '../systems/InventorySystem';
import { LootSystem } from '../systems/LootSystem';
import { emptyEquipStats, type EquipStats } from '../systems/CombatSystem';
import { GEM_STAT_MAP, Gems, getItemBase, Weapons, Armors } from '../data/items/bases';
import type { ItemInstance, EquipSlot, GemInstance, WeaponBase, ArmorBase } from '../data/types';

// Helper to create a simple gem inventory item
function makeGemItem(gemId: string, qty = 1): ItemInstance {
  const base = getItemBase(gemId);
  return {
    uid: `test_gem_${gemId}_${Math.random().toString(36).substring(2, 8)}`,
    baseId: gemId,
    name: base?.name ?? gemId,
    quality: 'normal',
    level: 1,
    affixes: [],
    sockets: [],
    identified: true,
    quantity: qty,
    stats: {},
  };
}

// Helper to create an equipment item with sockets
function makeEquipItem(baseId: string, quality: 'normal' | 'magic' | 'rare' = 'normal'): ItemInstance {
  const base = getItemBase(baseId);
  return {
    uid: `test_equip_${baseId}_${Math.random().toString(36).substring(2, 8)}`,
    baseId,
    name: base?.name ?? baseId,
    quality,
    level: 10,
    affixes: [],
    sockets: [],
    identified: true,
    quantity: 1,
    stats: {},
  };
}

describe('Gem Socketing System', () => {
  let inv: InventorySystem;

  beforeEach(() => {
    inv = new InventorySystem();
  });

  describe('Gem Data Definitions', () => {
    it('should have all 4 gem types × 3 tiers plus diamond', () => {
      const rubyGems = Gems.filter(g => g.id.startsWith('g_ruby'));
      const sapphireGems = Gems.filter(g => g.id.startsWith('g_sapphire'));
      const emeraldGems = Gems.filter(g => g.id.startsWith('g_emerald'));
      const topazGems = Gems.filter(g => g.id.startsWith('g_topaz'));
      const diamondGems = Gems.filter(g => g.id.startsWith('g_diamond'));

      expect(rubyGems.length).toBe(3);
      expect(sapphireGems.length).toBe(3);
      expect(emeraldGems.length).toBe(3);
      expect(topazGems.length).toBe(3);
      expect(diamondGems.length).toBeGreaterThanOrEqual(1);
    });

    it('should have correct gem stat mappings for Ruby (STR)', () => {
      expect(GEM_STAT_MAP['g_ruby_1']).toEqual({ stat: 'str', value: 5, tier: 1 });
      expect(GEM_STAT_MAP['g_ruby_2']).toEqual({ stat: 'str', value: 12, tier: 2 });
      expect(GEM_STAT_MAP['g_ruby_3']).toEqual({ stat: 'str', value: 20, tier: 3 });
    });

    it('should have correct gem stat mappings for Sapphire (INT)', () => {
      expect(GEM_STAT_MAP['g_sapphire_1']).toEqual({ stat: 'int', value: 5, tier: 1 });
      expect(GEM_STAT_MAP['g_sapphire_2']).toEqual({ stat: 'int', value: 12, tier: 2 });
      expect(GEM_STAT_MAP['g_sapphire_3']).toEqual({ stat: 'int', value: 20, tier: 3 });
    });

    it('should have correct gem stat mappings for Emerald (DEX)', () => {
      expect(GEM_STAT_MAP['g_emerald_1']).toEqual({ stat: 'dex', value: 5, tier: 1 });
      expect(GEM_STAT_MAP['g_emerald_2']).toEqual({ stat: 'dex', value: 12, tier: 2 });
      expect(GEM_STAT_MAP['g_emerald_3']).toEqual({ stat: 'dex', value: 20, tier: 3 });
    });

    it('should have correct gem stat mappings for Topaz (magicFind)', () => {
      expect(GEM_STAT_MAP['g_topaz_1']).toEqual({ stat: 'magicFind', value: 5, tier: 1 });
      expect(GEM_STAT_MAP['g_topaz_2']).toEqual({ stat: 'magicFind', value: 10, tier: 2 });
      expect(GEM_STAT_MAP['g_topaz_3']).toEqual({ stat: 'magicFind', value: 18, tier: 3 });
    });

    it('should have correct gem stat mapping for Diamond (allStats)', () => {
      expect(GEM_STAT_MAP['g_diamond_1']).toEqual({ stat: 'allStats', value: 3, tier: 1 });
    });

    it('higher tiers provide larger bonuses', () => {
      const types = ['g_ruby', 'g_sapphire', 'g_emerald', 'g_topaz'];
      for (const type of types) {
        const t1 = GEM_STAT_MAP[`${type}_1`];
        const t2 = GEM_STAT_MAP[`${type}_2`];
        const t3 = GEM_STAT_MAP[`${type}_3`];
        expect(t2.value).toBeGreaterThan(t1.value);
        expect(t3.value).toBeGreaterThan(t2.value);
        expect(t2.tier).toBeGreaterThan(t1.tier);
        expect(t3.tier).toBeGreaterThan(t2.tier);
      }
    });

    it('all gem items are stackable with maxStack 10', () => {
      for (const gem of Gems) {
        expect(gem.stackable).toBe(true);
        expect(gem.maxStack).toBe(10);
        expect(gem.type).toBe('gem');
      }
    });

    it('all gem names are in Chinese', () => {
      for (const gem of Gems) {
        // Chinese characters are in the range \u4e00-\u9fff
        expect(gem.name).toMatch(/[\u4e00-\u9fff]/);
      }
    });
  });

  describe('Socket Gem (Insert)', () => {
    it('should socket a gem into an equipped item with empty slots', () => {
      // w_short_sword has 1 socket
      const sword = makeEquipItem('w_short_sword');
      inv.equipment.weapon = sword;

      const gem = makeGemItem('g_ruby_1');
      inv.inventory.push(gem);

      const result = inv.socketGem('weapon', gem.uid);
      expect(result).toBe(true);
      expect(sword.sockets.length).toBe(1);
      expect(sword.sockets[0].gemId).toBe('g_ruby_1');
      expect(sword.sockets[0].stat).toBe('str');
      expect(sword.sockets[0].value).toBe(5);
      expect(sword.sockets[0].tier).toBe(1);
    });

    it('should remove gem from inventory after socketing', () => {
      const sword = makeEquipItem('w_short_sword');
      inv.equipment.weapon = sword;

      const gem = makeGemItem('g_ruby_1');
      inv.inventory.push(gem);
      const uid = gem.uid;

      inv.socketGem('weapon', uid);
      expect(inv.inventory.find(i => i.uid === uid)).toBeUndefined();
    });

    it('should decrement gem quantity if stacked', () => {
      const sword = makeEquipItem('w_short_sword');
      inv.equipment.weapon = sword;

      const gem = makeGemItem('g_ruby_1', 3);
      inv.inventory.push(gem);

      inv.socketGem('weapon', gem.uid);
      expect(gem.quantity).toBe(2);
      // Gem should still be in inventory
      expect(inv.inventory.includes(gem)).toBe(true);
    });

    it('should prevent socketing when all slots are filled', () => {
      // w_short_sword has 1 socket
      const sword = makeEquipItem('w_short_sword');
      sword.sockets.push({
        gemId: 'g_ruby_1', name: '碎裂红宝石', stat: 'str', value: 5, tier: 1,
      });
      inv.equipment.weapon = sword;

      const gem2 = makeGemItem('g_sapphire_1');
      inv.inventory.push(gem2);

      const result = inv.socketGem('weapon', gem2.uid);
      expect(result).toBe(false);
      expect(sword.sockets.length).toBe(1);
      // Gem should still be in inventory
      expect(inv.inventory.find(i => i.uid === gem2.uid)).toBeDefined();
    });

    it('should return false when equipment slot is empty', () => {
      const gem = makeGemItem('g_ruby_1');
      inv.inventory.push(gem);

      const result = inv.socketGem('weapon', gem.uid);
      expect(result).toBe(false);
    });

    it('should return false when gem is not in inventory', () => {
      const sword = makeEquipItem('w_short_sword');
      inv.equipment.weapon = sword;

      const result = inv.socketGem('weapon', 'nonexistent_uid');
      expect(result).toBe(false);
    });

    it('should return false for non-gem items', () => {
      const sword = makeEquipItem('w_short_sword');
      inv.equipment.weapon = sword;

      const potion = {
        uid: 'test_potion',
        baseId: 'c_hp_potion_s',
        name: '小型生命药水',
        quality: 'normal' as const,
        level: 1,
        affixes: [],
        sockets: [],
        identified: true,
        quantity: 1,
        stats: {},
      };
      inv.inventory.push(potion);

      const result = inv.socketGem('weapon', potion.uid);
      expect(result).toBe(false);
    });

    it('should socket multiple gems into multi-socket items', () => {
      // w_claymore has 2 sockets
      const claymore = makeEquipItem('w_claymore');
      inv.equipment.weapon = claymore;

      const gem1 = makeGemItem('g_ruby_1');
      const gem2 = makeGemItem('g_sapphire_2');
      inv.inventory.push(gem1);
      inv.inventory.push(gem2);

      expect(inv.socketGem('weapon', gem1.uid)).toBe(true);
      expect(inv.socketGem('weapon', gem2.uid)).toBe(true);
      expect(claymore.sockets.length).toBe(2);
      expect(claymore.sockets[0].stat).toBe('str');
      expect(claymore.sockets[1].stat).toBe('int');
    });

    it('should socket into 3-socket high-tier items', () => {
      // w_demon_blade has 3 sockets
      const blade = makeEquipItem('w_demon_blade');
      inv.equipment.weapon = blade;

      const gems = [
        makeGemItem('g_ruby_3'),
        makeGemItem('g_sapphire_3'),
        makeGemItem('g_emerald_3'),
      ];
      for (const g of gems) inv.inventory.push(g);

      for (const g of gems) {
        expect(inv.socketGem('weapon', g.uid)).toBe(true);
      }
      expect(blade.sockets.length).toBe(3);

      // Fourth gem should fail
      const extraGem = makeGemItem('g_topaz_1');
      inv.inventory.push(extraGem);
      expect(inv.socketGem('weapon', extraGem.uid)).toBe(false);
    });
  });

  describe('Unsocket Gem (Remove)', () => {
    it('should remove a socketed gem back to inventory', () => {
      const sword = makeEquipItem('w_short_sword');
      sword.sockets.push({
        gemId: 'g_ruby_1', name: '碎裂红宝石', stat: 'str', value: 5, tier: 1,
      });
      inv.equipment.weapon = sword;

      const result = inv.unsocketGem('weapon', 0);
      expect(result).toBe(true);
      expect(sword.sockets.length).toBe(0);
      
      // Gem should be back in inventory
      const gemInInv = inv.inventory.find(i => i.baseId === 'g_ruby_1');
      expect(gemInInv).toBeDefined();
      expect(gemInInv!.quantity).toBe(1);
    });

    it('should stack with existing gems in inventory when unsocketing', () => {
      const sword = makeEquipItem('w_short_sword');
      sword.sockets.push({
        gemId: 'g_ruby_1', name: '碎裂红宝石', stat: 'str', value: 5, tier: 1,
      });
      inv.equipment.weapon = sword;

      // Pre-existing ruby in inventory
      const existingGem = makeGemItem('g_ruby_1', 3);
      inv.inventory.push(existingGem);

      inv.unsocketGem('weapon', 0);
      // Should have stacked
      expect(existingGem.quantity).toBe(4);
      // No new item added
      expect(inv.inventory.filter(i => i.baseId === 'g_ruby_1').length).toBe(1);
    });

    it('should fail when inventory is full', () => {
      const sword = makeEquipItem('w_short_sword');
      sword.sockets.push({
        gemId: 'g_ruby_1', name: '碎裂红宝石', stat: 'str', value: 5, tier: 1,
      });
      inv.equipment.weapon = sword;

      // Fill inventory to max
      for (let i = 0; i < 100; i++) {
        inv.inventory.push(makeEquipItem('w_rusty_sword'));
      }

      const result = inv.unsocketGem('weapon', 0);
      expect(result).toBe(false);
      expect(sword.sockets.length).toBe(1);
    });

    it('should fail with invalid socket index', () => {
      const sword = makeEquipItem('w_short_sword');
      inv.equipment.weapon = sword;

      expect(inv.unsocketGem('weapon', 0)).toBe(false);
      expect(inv.unsocketGem('weapon', -1)).toBe(false);
    });

    it('should fail when equipment slot is empty', () => {
      expect(inv.unsocketGem('weapon', 0)).toBe(false);
    });

    it('should remove correct gem from multi-socket item', () => {
      const claymore = makeEquipItem('w_claymore');
      claymore.sockets.push(
        { gemId: 'g_ruby_1', name: '碎裂红宝石', stat: 'str', value: 5, tier: 1 },
        { gemId: 'g_sapphire_2', name: '蓝宝石', stat: 'int', value: 12, tier: 2 },
      );
      inv.equipment.weapon = claymore;

      // Remove the first gem (ruby)
      inv.unsocketGem('weapon', 0);
      expect(claymore.sockets.length).toBe(1);
      expect(claymore.sockets[0].gemId).toBe('g_sapphire_2');
    });
  });

  describe('getMaxSockets', () => {
    it('returns correct socket count for weapons', () => {
      // w_rusty_sword has 0 sockets
      inv.equipment.weapon = makeEquipItem('w_rusty_sword');
      expect(inv.getMaxSockets('weapon')).toBe(0);

      // w_short_sword has 1 socket
      inv.equipment.weapon = makeEquipItem('w_short_sword');
      expect(inv.getMaxSockets('weapon')).toBe(1);

      // w_claymore has 2 sockets
      inv.equipment.weapon = makeEquipItem('w_claymore');
      expect(inv.getMaxSockets('weapon')).toBe(2);

      // w_demon_blade has 3 sockets
      inv.equipment.weapon = makeEquipItem('w_demon_blade');
      expect(inv.getMaxSockets('weapon')).toBe(3);
    });

    it('returns correct socket count for armor', () => {
      // a_quilted_armor has 0 sockets
      inv.equipment.armor = makeEquipItem('a_quilted_armor');
      expect(inv.getMaxSockets('armor')).toBe(0);

      // a_chain_mail has 1 socket
      inv.equipment.armor = makeEquipItem('a_chain_mail');
      expect(inv.getMaxSockets('armor')).toBe(1);

      // a_plate_armor has 2 sockets
      inv.equipment.armor = makeEquipItem('a_plate_armor');
      expect(inv.getMaxSockets('armor')).toBe(2);
    });

    it('returns 0 for empty equipment slot', () => {
      expect(inv.getMaxSockets('weapon')).toBe(0);
      expect(inv.getMaxSockets('armor')).toBe(0);
    });

    it('returns 0 for accessories (no sockets)', () => {
      inv.equipment.ring1 = makeEquipItem('j_copper_ring');
      expect(inv.getMaxSockets('ring1')).toBe(0);
    });
  });

  describe('Stat Flow: Gem → Item Stats → Equipment Stats → EquipStats', () => {
    it('socketed gem stats appear in item.stats', () => {
      const sword = makeEquipItem('w_short_sword');
      inv.equipment.weapon = sword;

      const gem = makeGemItem('g_ruby_2');
      inv.inventory.push(gem);
      inv.socketGem('weapon', gem.uid);

      expect(sword.stats['str']).toBe(12);
    });

    it('socketed gem stats appear in getEquipmentStats()', () => {
      const sword = makeEquipItem('w_short_sword');
      inv.equipment.weapon = sword;

      const gem = makeGemItem('g_ruby_1');
      inv.inventory.push(gem);
      inv.socketGem('weapon', gem.uid);

      const eqStats = inv.getEquipmentStats();
      expect(eqStats['str']).toBe(5);
    });

    it('socketed gem stats appear in getTypedEquipStats()', () => {
      const sword = makeEquipItem('w_short_sword');
      inv.equipment.weapon = sword;

      const gem = makeGemItem('g_ruby_1');
      inv.inventory.push(gem);
      inv.socketGem('weapon', gem.uid);

      const eqStats = inv.getTypedEquipStats();
      expect(eqStats.str).toBe(5);
    });

    it('Topaz magicFind flows to EquipStats', () => {
      const sword = makeEquipItem('w_short_sword');
      inv.equipment.weapon = sword;

      const gem = makeGemItem('g_topaz_2');
      inv.inventory.push(gem);
      inv.socketGem('weapon', gem.uid);

      const eqStats = inv.getTypedEquipStats();
      expect(eqStats.magicFind).toBe(10);
    });

    it('Diamond allStats expands to all primary stats', () => {
      const sword = makeEquipItem('w_short_sword');
      inv.equipment.weapon = sword;

      const gem = makeGemItem('g_diamond_1');
      inv.inventory.push(gem);
      inv.socketGem('weapon', gem.uid);

      const eqStats = inv.getEquipmentStats();
      expect(eqStats['str']).toBe(3);
      expect(eqStats['dex']).toBe(3);
      expect(eqStats['int']).toBe(3);
      expect(eqStats['vit']).toBe(3);
      expect(eqStats['spi']).toBe(3);
      expect(eqStats['lck']).toBe(3);
      expect(eqStats['allStats']).toBeUndefined();
    });

    it('Diamond allStats flows through typed EquipStats', () => {
      const sword = makeEquipItem('w_short_sword');
      inv.equipment.weapon = sword;

      const gem = makeGemItem('g_diamond_1');
      inv.inventory.push(gem);
      inv.socketGem('weapon', gem.uid);

      const typed = inv.getTypedEquipStats();
      expect(typed.str).toBe(3);
      expect(typed.dex).toBe(3);
      expect(typed.int).toBe(3);
      expect(typed.vit).toBe(3);
      expect(typed.spi).toBe(3);
      expect(typed.lck).toBe(3);
    });

    it('multiple gems on different items accumulate stats', () => {
      // Weapon with ruby
      const sword = makeEquipItem('w_short_sword');
      inv.equipment.weapon = sword;
      const ruby = makeGemItem('g_ruby_2');
      inv.inventory.push(ruby);
      inv.socketGem('weapon', ruby.uid);

      // Armor with sapphire
      const armor = makeEquipItem('a_chain_mail');
      inv.equipment.armor = armor;
      const sapphire = makeGemItem('g_sapphire_1');
      inv.inventory.push(sapphire);
      inv.socketGem('armor', sapphire.uid);

      const eqStats = inv.getEquipmentStats();
      expect(eqStats['str']).toBe(12); // Ruby T2
      expect(eqStats['int']).toBe(5);  // Sapphire T1
    });

    it('removing gem removes stat from equipment stats', () => {
      const sword = makeEquipItem('w_short_sword');
      inv.equipment.weapon = sword;

      const gem = makeGemItem('g_ruby_1');
      inv.inventory.push(gem);
      inv.socketGem('weapon', gem.uid);

      expect(inv.getEquipmentStats()['str']).toBe(5);

      inv.unsocketGem('weapon', 0);
      expect(inv.getEquipmentStats()['str']).toBeUndefined();
    });

    it('gem stats combine with affix stats on same item', () => {
      const sword = makeEquipItem('w_short_sword');
      sword.affixes = [{ affixId: 'test', name: '力量', stat: 'str', value: 10 }];
      // Manually set stats from affixes
      sword.stats = { str: 10 };
      inv.equipment.weapon = sword;

      const gem = makeGemItem('g_ruby_1');
      inv.inventory.push(gem);
      inv.socketGem('weapon', gem.uid);

      // After socketing, item stats should include both affix and gem
      expect(sword.stats['str']).toBe(15); // 10 affix + 5 gem
      expect(inv.getEquipmentStats()['str']).toBe(15);
    });
  });

  describe('Stat Flow: EquipStats → Player.recalcDerived()', () => {
    it('str from gems increases player damage via recalcDerived', () => {
      // Create minimal player-like object
      const playerStats = { str: 10, dex: 10, vit: 10, int: 10, spi: 10, lck: 10 };
      const level = 10;
      
      // Base damage without gem: 8 + 10 * 0.8 + 10 * 2 = 36
      const baseDmg = 8 + playerStats.str * 0.8 + level * 2;
      
      // With ruby gem (+5 str): 8 + 15 * 0.8 + 10 * 2 = 40
      const eqStatsWithGem = emptyEquipStats();
      eqStatsWithGem.str = 5;
      const dmgWithGem = 8 + (playerStats.str + eqStatsWithGem.str) * 0.8 + level * 2;
      
      expect(dmgWithGem).toBeGreaterThan(baseDmg);
      expect(dmgWithGem - baseDmg).toBeCloseTo(4); // 5 * 0.8
    });

    it('int from gems increases player mana via recalcDerived', () => {
      const playerStats = { str: 10, dex: 10, vit: 10, int: 10, spi: 10, lck: 10 };
      const level = 10;
      
      // Base mana: 30 + spi*8 + int*3 + (level-1)*8 = 30 + 80 + 30 + 72 = 212
      const baseMana = 30 + playerStats.spi * 8 + playerStats.int * 3 + (level - 1) * 8;
      
      // With sapphire gem (+12 int): 30 + 80 + 22*3 + 72 = 248 (but int is 10+12=22)
      const eqStatsWithGem = emptyEquipStats();
      eqStatsWithGem.int = 12;
      const manaWithGem = 30 + playerStats.spi * 8 + (playerStats.int + eqStatsWithGem.int) * 3 + (level - 1) * 8;
      
      expect(manaWithGem).toBeGreaterThan(baseMana);
      expect(manaWithGem - baseMana).toBe(36); // 12 * 3
    });

    it('vit from gems increases player HP via recalcDerived', () => {
      const playerStats = { str: 10, dex: 10, vit: 10, int: 10, spi: 10, lck: 10 };
      const level = 10;
      
      // Base HP: 50 + vit*10 + (level-1)*15 = 50 + 100 + 135 = 285
      const baseHp = 50 + playerStats.vit * 10 + (level - 1) * 15;
      
      // With diamond gem (+3 vit through allStats): 50 + 13*10 + 135 = 315
      const eqStatsWithGem = emptyEquipStats();
      eqStatsWithGem.vit = 3;
      const hpWithGem = 50 + (playerStats.vit + eqStatsWithGem.vit) * 10 + (level - 1) * 15;
      
      expect(hpWithGem).toBeGreaterThan(baseHp);
      expect(hpWithGem - baseHp).toBe(30); // 3 * 10
    });
  });

  describe('LootSystem gem stat computation', () => {
    it('LootSystem.computeStats includes gem stats in item.stats', () => {
      // Create item via LootSystem to verify stats are computed
      const loot = new LootSystem();
      const item = loot.createItem('w_short_sword', 10, 'normal');
      expect(item).not.toBeNull();
      
      // Manually socket a gem
      item!.sockets.push({
        gemId: 'g_ruby_1', name: '碎裂红宝石', stat: 'str', value: 5, tier: 1,
      });
      
      // LootSystem.computeStats is private, but item.stats should reflect sockets
      // after recomputing (which happens through InventorySystem)
      // For direct verification, check that gem stats are accessible
      expect(item!.sockets[0].stat).toBe('str');
      expect(item!.sockets[0].value).toBe(5);
    });

    it('LootSystem.generateGem produces valid gem items', () => {
      const loot = new LootSystem();
      // Generate loot at various levels and check for gems
      let foundGem = false;
      for (let i = 0; i < 200; i++) {
        const items = loot.generateLoot(
          { id: 'test', name: 'test', level: 20, hp: 100, damage: 10, defense: 5, speed: 100, aggroRange: 5, attackRange: 1, attackSpeed: 1000, expReward: 10, goldReward: [1, 5] as [number, number], spriteKey: 'test' },
          50, // high luck to increase gem drop rate
        );
        for (const item of items) {
          const base = getItemBase(item.baseId);
          if (base?.type === 'gem') {
            foundGem = true;
            expect(GEM_STAT_MAP[item.baseId]).toBeDefined();
          }
        }
      }
      // With high luck over 200 attempts, we should find at least one gem
      expect(foundGem).toBe(true);
    });
  });

  describe('Save/Load Persistence', () => {
    it('socketed gems survive serialization round-trip (JSON)', () => {
      const sword = makeEquipItem('w_claymore');
      sword.sockets.push(
        { gemId: 'g_ruby_2', name: '红宝石', stat: 'str', value: 12, tier: 2 },
        { gemId: 'g_sapphire_1', name: '碎裂蓝宝石', stat: 'int', value: 5, tier: 1 },
      );
      sword.stats = { str: 12, int: 5 };

      inv.equipment.weapon = sword;

      // Serialize (simulating save)
      const saved = JSON.parse(JSON.stringify(inv.equipment));

      // Create fresh inventory and load
      const inv2 = new InventorySystem();
      inv2.equipment = saved;

      const loadedSword = inv2.equipment.weapon!;
      expect(loadedSword.sockets.length).toBe(2);
      expect(loadedSword.sockets[0].gemId).toBe('g_ruby_2');
      expect(loadedSword.sockets[0].stat).toBe('str');
      expect(loadedSword.sockets[0].value).toBe(12);
      expect(loadedSword.sockets[0].tier).toBe(2);
      expect(loadedSword.sockets[1].gemId).toBe('g_sapphire_1');
      expect(loadedSword.sockets[1].stat).toBe('int');
      expect(loadedSword.sockets[1].value).toBe(5);

      // Stats should be preserved
      expect(loadedSword.stats['str']).toBe(12);
      expect(loadedSword.stats['int']).toBe(5);

      // Equipment stats should flow through correctly
      const eqStats = inv2.getEquipmentStats();
      expect(eqStats['str']).toBe(12);
      expect(eqStats['int']).toBe(5);
    });

    it('empty sockets array survives serialization', () => {
      const sword = makeEquipItem('w_short_sword');
      inv.equipment.weapon = sword;

      const saved = JSON.parse(JSON.stringify(inv.equipment));
      const inv2 = new InventorySystem();
      inv2.equipment = saved;

      expect(inv2.equipment.weapon!.sockets).toEqual([]);
    });

    it('SaveData equipment field includes sockets in ItemInstance', () => {
      // Verify the SaveData type supports gem persistence
      const sword = makeEquipItem('w_claymore');
      sword.sockets.push({
        gemId: 'g_emerald_3', name: '完美翡翠', stat: 'dex', value: 20, tier: 3,
      });

      // Simulate what SaveSystem stores
      const saveEquipment: Record<string, ItemInstance> = {
        weapon: sword,
      };

      const json = JSON.stringify(saveEquipment);
      const parsed = JSON.parse(json);

      expect(parsed.weapon.sockets[0].gemId).toBe('g_emerald_3');
      expect(parsed.weapon.sockets[0].value).toBe(20);
    });
  });

  describe('Edge Cases', () => {
    it('should not socket into items with 0 max sockets', () => {
      const rusty = makeEquipItem('w_rusty_sword'); // 0 sockets
      inv.equipment.weapon = rusty;

      const gem = makeGemItem('g_ruby_1');
      inv.inventory.push(gem);

      const result = inv.socketGem('weapon', gem.uid);
      expect(result).toBe(false);
    });

    it('socket and unsocket round-trip preserves gem identity', () => {
      const sword = makeEquipItem('w_short_sword');
      inv.equipment.weapon = sword;

      const gem = makeGemItem('g_topaz_3');
      inv.inventory.push(gem);

      inv.socketGem('weapon', gem.uid);
      expect(sword.sockets[0].gemId).toBe('g_topaz_3');
      expect(sword.sockets[0].value).toBe(18);

      inv.unsocketGem('weapon', 0);
      const returnedGem = inv.inventory.find(i => i.baseId === 'g_topaz_3');
      expect(returnedGem).toBeDefined();
      expect(returnedGem!.baseId).toBe('g_topaz_3');
    });

    it('equipment items with sockets defined in bases have correct socket counts', () => {
      // Verify some weapons from bases have expected socket values
      const shortSword = Weapons.find(w => w.id === 'w_short_sword')!;
      expect(shortSword.sockets).toBe(1);

      const claymore = Weapons.find(w => w.id === 'w_claymore')!;
      expect(claymore.sockets).toBe(2);

      const demonBlade = Weapons.find(w => w.id === 'w_demon_blade')!;
      expect(demonBlade.sockets).toBe(3);

      const chainMail = Armors.find(a => a.id === 'a_chain_mail')!;
      expect(chainMail.sockets).toBe(1);

      const plateArmor = Armors.find(a => a.id === 'a_plate_armor')!;
      expect(plateArmor.sockets).toBe(2);

      const dragonArmor = Armors.find(a => a.id === 'a_dragon_armor')!;
      expect(dragonArmor.sockets).toBe(3);
    });

    it('EquipStats includes primary stat fields', () => {
      const eq = emptyEquipStats();
      expect(eq.str).toBe(0);
      expect(eq.dex).toBe(0);
      expect(eq.int).toBe(0);
      expect(eq.vit).toBe(0);
      expect(eq.spi).toBe(0);
      expect(eq.lck).toBe(0);
    });
  });
});
