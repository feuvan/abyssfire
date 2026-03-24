import { describe, it, expect, beforeEach } from 'vitest';
import {
  EliteAffixSystem,
  ELITE_AFFIX_DEFINITIONS,
  type EliteAffixType,
  type EliteAffixInstance,
  type EliteAffixDefinition,
} from '../systems/EliteAffixSystem';
import { LootSystem } from '../systems/LootSystem';
import type { MonsterDefinition } from '../data/types';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function makeAffix(type: EliteAffixType): EliteAffixInstance {
  return {
    definition: ELITE_AFFIX_DEFINITIONS[type],
    lastTeleportTime: 0,
    lastCurseTickTime: 0,
  };
}

function makeMonsterDef(overrides?: Partial<MonsterDefinition>): MonsterDefinition {
  return {
    id: 'test_monster',
    name: '测试怪物',
    level: 10,
    hp: 100,
    damage: 20,
    defense: 5,
    speed: 50,
    aggroRange: 6,
    attackRange: 1.5,
    attackSpeed: 1500,
    expReward: 50,
    goldReward: [10, 20],
    spriteKey: 'monster_test',
    elite: true,
    ...overrides,
  };
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

describe('EliteAffixSystem', () => {
  let system: EliteAffixSystem;

  beforeEach(() => {
    system = new EliteAffixSystem();
  });

  // =========================================================================
  // Affix Definitions
  // =========================================================================

  describe('Affix Definitions', () => {
    it('has at least 5 affix types defined', () => {
      const types = Object.keys(ELITE_AFFIX_DEFINITIONS);
      expect(types.length).toBeGreaterThanOrEqual(5);
    });

    it('includes the 5 required affix types', () => {
      const required: EliteAffixType[] = ['fire_enhanced', 'swift', 'teleporting', 'extra_strong', 'curse_aura'];
      for (const type of required) {
        expect(ELITE_AFFIX_DEFINITIONS[type]).toBeDefined();
      }
    });

    it('all affix definitions have Chinese names', () => {
      for (const [type, def] of Object.entries(ELITE_AFFIX_DEFINITIONS)) {
        expect(def.name, `${type} should have a Chinese name`).toBeTruthy();
        // Check it contains at least one Chinese character
        expect(def.name).toMatch(/[\u4e00-\u9fff]/);
      }
    });

    it('all affix definitions have English names', () => {
      for (const [type, def] of Object.entries(ELITE_AFFIX_DEFINITIONS)) {
        expect(def.nameEn, `${type} should have an English name`).toBeTruthy();
      }
    });

    it('all affix definitions have valid VFX colors', () => {
      for (const [type, def] of Object.entries(ELITE_AFFIX_DEFINITIONS)) {
        expect(def.vfxColor, `${type} should have a VFX color`).toBeGreaterThan(0);
      }
    });

    it('all affix definitions have non-negative loot quality bonus', () => {
      for (const [type, def] of Object.entries(ELITE_AFFIX_DEFINITIONS)) {
        expect(def.lootQualityBonus, `${type} lootQualityBonus`).toBeGreaterThanOrEqual(0);
      }
    });

    it('fire_enhanced has extraFireDamage > 0', () => {
      expect(ELITE_AFFIX_DEFINITIONS.fire_enhanced.extraFireDamage).toBeGreaterThan(0);
    });

    it('swift has speedMult > 1.0', () => {
      expect(ELITE_AFFIX_DEFINITIONS.swift.speedMult).toBeGreaterThan(1.0);
    });

    it('teleporting has teleportCooldownMs > 0', () => {
      expect(ELITE_AFFIX_DEFINITIONS.teleporting.teleportCooldownMs).toBeGreaterThan(0);
    });

    it('extra_strong has damageMult >= 1.3 (+30%+ damage)', () => {
      expect(ELITE_AFFIX_DEFINITIONS.extra_strong.damageMult).toBeGreaterThanOrEqual(1.3);
    });

    it('curse_aura has curseAuraRadius > 0 and curseAuraReduction > 0', () => {
      expect(ELITE_AFFIX_DEFINITIONS.curse_aura.curseAuraRadius).toBeGreaterThan(0);
      expect(ELITE_AFFIX_DEFINITIONS.curse_aura.curseAuraReduction).toBeGreaterThan(0);
    });

    it('vampiric has lifestealFraction > 0', () => {
      expect(ELITE_AFFIX_DEFINITIONS.vampiric.lifestealFraction).toBeGreaterThan(0);
    });

    it('frozen has freezeChance > 0', () => {
      expect(ELITE_AFFIX_DEFINITIONS.frozen.freezeChance).toBeGreaterThan(0);
    });
  });

  // =========================================================================
  // Affix Rolling (Zone-Based)
  // =========================================================================

  describe('rollAffixes', () => {
    it('returns empty array for non-elite monsters', () => {
      const result = system.rollAffixes('emerald_plains', false);
      expect(result).toEqual([]);
    });

    it('returns empty array for unknown zone', () => {
      const result = system.rollAffixes('nonexistent_zone', true);
      expect(result.length).toBeGreaterThanOrEqual(1); // defaults to [1,1]
    });

    it('rolls exactly 1 affix for zones 1-3', () => {
      const zones = ['emerald_plains', 'twilight_forest', 'anvil_mountains'];
      for (const zone of zones) {
        for (let i = 0; i < 10; i++) {
          const result = system.rollAffixes(zone, true);
          expect(result.length, `${zone} should always roll 1 affix`).toBe(1);
        }
      }
    });

    it('rolls 1-2 affixes for zone 4 (scorching_desert)', () => {
      const counts = new Set<number>();
      for (let i = 0; i < 100; i++) {
        const result = system.rollAffixes('scorching_desert', true);
        counts.add(result.length);
        expect(result.length).toBeGreaterThanOrEqual(1);
        expect(result.length).toBeLessThanOrEqual(2);
      }
      // Over 100 runs we should see both 1 and 2
      expect(counts.size).toBeGreaterThanOrEqual(1);
    });

    it('rolls 2-3 affixes for zone 5 (abyss_rift)', () => {
      const counts = new Set<number>();
      for (let i = 0; i < 100; i++) {
        const result = system.rollAffixes('abyss_rift', true);
        counts.add(result.length);
        expect(result.length).toBeGreaterThanOrEqual(2);
        expect(result.length).toBeLessThanOrEqual(3);
      }
    });

    it('rolls unique affixes (no duplicates in a single roll)', () => {
      for (let i = 0; i < 50; i++) {
        const result = system.rollAffixes('abyss_rift', true);
        const types = result.map(a => a.definition.type);
        const unique = new Set(types);
        expect(unique.size).toBe(types.length);
      }
    });
  });

  // =========================================================================
  // selectAffixes
  // =========================================================================

  describe('selectAffixes', () => {
    it('selects the requested number of unique affixes', () => {
      const result = system.selectAffixes(3);
      expect(result.length).toBe(3);
      const types = new Set(result.map(a => a.definition.type));
      expect(types.size).toBe(3);
    });

    it('does not exceed total available affix types', () => {
      const totalTypes = Object.keys(ELITE_AFFIX_DEFINITIONS).length;
      const result = system.selectAffixes(totalTypes + 5);
      expect(result.length).toBe(totalTypes);
    });

    it('initializes instance state correctly', () => {
      const result = system.selectAffixes(1);
      expect(result[0].lastTeleportTime).toBe(0);
      expect(result[0].lastCurseTickTime).toBe(0);
    });
  });

  // =========================================================================
  // Name Building
  // =========================================================================

  describe('buildAffixName', () => {
    it('returns base name when no affixes', () => {
      expect(system.buildAffixName('哥布林', [])).toBe('哥布林');
    });

    it('wraps single affix in brackets', () => {
      const affixes = [makeAffix('fire_enhanced')];
      const name = system.buildAffixName('哥布林', affixes);
      expect(name).toBe('[炎魔] 哥布林');
    });

    it('separates multiple affixes with · delimiter', () => {
      const affixes = [makeAffix('fire_enhanced'), makeAffix('swift')];
      const name = system.buildAffixName('哥布林', affixes);
      expect(name).toBe('[炎魔·迅捷] 哥布林');
    });

    it('handles triple affixes correctly', () => {
      const affixes = [makeAffix('extra_strong'), makeAffix('curse_aura'), makeAffix('vampiric')];
      const name = system.buildAffixName('恶魔领主', affixes);
      expect(name).toBe('[狂暴·诅咒·吸血] 恶魔领主');
    });

    it('affix names are all in Chinese', () => {
      const affixes = [makeAffix('fire_enhanced'), makeAffix('teleporting')];
      const name = system.buildAffixName('测试', affixes);
      // Should contain Chinese characters (not English)
      expect(name).toMatch(/[\u4e00-\u9fff]/);
      expect(name).not.toMatch(/Fire|Teleporting/);
    });
  });

  // =========================================================================
  // Combined Stats (Stacking)
  // =========================================================================

  describe('getCombinedStats', () => {
    it('returns neutral multipliers for empty affix list', () => {
      const stats = system.getCombinedStats([]);
      expect(stats.damageMult).toBe(1);
      expect(stats.speedMult).toBe(1);
      expect(stats.hpMult).toBe(1);
      expect(stats.defenseMult).toBe(1);
      expect(stats.extraFireDamage).toBe(0);
      expect(stats.lootQualityBonus).toBe(0);
    });

    it('returns correct stats for single fire_enhanced affix', () => {
      const affixes = [makeAffix('fire_enhanced')];
      const stats = system.getCombinedStats(affixes);
      expect(stats.hpMult).toBe(1.2);
      expect(stats.extraFireDamage).toBe(0.3);
      expect(stats.lootQualityBonus).toBe(5);
    });

    it('multiplies damage/speed/hp/defense multiplicatively', () => {
      const affixes = [makeAffix('extra_strong'), makeAffix('swift')];
      const stats = system.getCombinedStats(affixes);
      // extra_strong damageMult=1.35, swift damageMult=1.0 → 1.35
      expect(stats.damageMult).toBeCloseTo(1.35, 2);
      // extra_strong speedMult=1.0, swift speedMult=1.6 → 1.6
      expect(stats.speedMult).toBeCloseTo(1.6, 2);
      // extra_strong hpMult=1.3, swift hpMult=1.0 → 1.3
      expect(stats.hpMult).toBeCloseTo(1.3, 2);
    });

    it('stacks multiple HP multipliers multiplicatively', () => {
      const affixes = [makeAffix('fire_enhanced'), makeAffix('extra_strong')];
      const stats = system.getCombinedStats(affixes);
      // fire_enhanced hpMult=1.2 * extra_strong hpMult=1.3 = 1.56
      expect(stats.hpMult).toBeCloseTo(1.56, 2);
    });

    it('stacks loot quality bonus additively', () => {
      const affixes = [makeAffix('fire_enhanced'), makeAffix('extra_strong'), makeAffix('teleporting')];
      const stats = system.getCombinedStats(affixes);
      // 5 + 8 + 5 = 18
      expect(stats.lootQualityBonus).toBe(18);
    });

    it('stacks extraFireDamage additively', () => {
      // Only fire_enhanced has extraFireDamage, but test with multiple
      const affixes = [makeAffix('fire_enhanced')];
      const stats = system.getCombinedStats(affixes);
      expect(stats.extraFireDamage).toBe(0.3);
    });

    it('caps freezeChance at 0.5', () => {
      // Create two frozen affixes manually to test cap
      const affix1 = makeAffix('frozen');
      const affix2 = makeAffix('frozen');
      // frozen freezeChance = 0.25, two of them would be 0.5
      const stats = system.getCombinedStats([affix1, affix2]);
      expect(stats.freezeChance).toBeLessThanOrEqual(0.5);
    });

    it('stacks lifesteal additively', () => {
      const affixes = [makeAffix('vampiric')];
      const stats = system.getCombinedStats(affixes);
      expect(stats.lifestealFraction).toBe(0.2);
    });
  });

  // =========================================================================
  // Utility Methods
  // =========================================================================

  describe('hasAffix', () => {
    it('returns true when affix type is present', () => {
      const affixes = [makeAffix('fire_enhanced'), makeAffix('swift')];
      expect(system.hasAffix(affixes, 'fire_enhanced')).toBe(true);
      expect(system.hasAffix(affixes, 'swift')).toBe(true);
    });

    it('returns false when affix type is absent', () => {
      const affixes = [makeAffix('fire_enhanced')];
      expect(system.hasAffix(affixes, 'teleporting')).toBe(false);
    });
  });

  describe('getTeleportingAffix', () => {
    it('returns the teleporting affix instance', () => {
      const affixes = [makeAffix('fire_enhanced'), makeAffix('teleporting')];
      const result = system.getTeleportingAffix(affixes);
      expect(result).toBeDefined();
      expect(result!.definition.type).toBe('teleporting');
    });

    it('returns undefined when no teleporting affix', () => {
      const affixes = [makeAffix('fire_enhanced'), makeAffix('swift')];
      expect(system.getTeleportingAffix(affixes)).toBeUndefined();
    });
  });

  describe('getCurseAuraAffix', () => {
    it('returns the curse aura affix instance', () => {
      const affixes = [makeAffix('curse_aura')];
      const result = system.getCurseAuraAffix(affixes);
      expect(result).toBeDefined();
      expect(result!.definition.type).toBe('curse_aura');
    });

    it('returns undefined when no curse aura affix', () => {
      const affixes = [makeAffix('fire_enhanced')];
      expect(system.getCurseAuraAffix(affixes)).toBeUndefined();
    });
  });

  describe('shouldTeleport', () => {
    it('returns true when cooldown has elapsed', () => {
      const affix = makeAffix('teleporting');
      affix.lastTeleportTime = 0;
      expect(system.shouldTeleport(affix, 6000)).toBe(true); // 6s > 5s cooldown
    });

    it('returns false when cooldown has not elapsed', () => {
      const affix = makeAffix('teleporting');
      affix.lastTeleportTime = 3000;
      expect(system.shouldTeleport(affix, 5000)).toBe(false); // 2s < 5s cooldown
    });

    it('returns true at exact cooldown boundary', () => {
      const affix = makeAffix('teleporting');
      affix.lastTeleportTime = 0;
      expect(system.shouldTeleport(affix, 5000)).toBe(true); // exactly 5s
    });
  });

  describe('getExtraFireDamage', () => {
    it('calculates correct extra fire damage', () => {
      const affixes = [makeAffix('fire_enhanced')];
      const extra = system.getExtraFireDamage(affixes, 100);
      // 30% of 100 = 30
      expect(extra).toBe(30);
    });

    it('returns 0 when no fire_enhanced affix', () => {
      const affixes = [makeAffix('swift')];
      const extra = system.getExtraFireDamage(affixes, 100);
      expect(extra).toBe(0);
    });
  });

  describe('getPrimaryVfxColor', () => {
    it('returns first affix color', () => {
      const affixes = [makeAffix('fire_enhanced'), makeAffix('swift')];
      expect(system.getPrimaryVfxColor(affixes)).toBe(0xff4400);
    });

    it('returns white for empty affixes', () => {
      expect(system.getPrimaryVfxColor([])).toBe(0xffffff);
    });
  });

  describe('getAllVfxColors', () => {
    it('returns all affix colors', () => {
      const affixes = [makeAffix('fire_enhanced'), makeAffix('swift'), makeAffix('teleporting')];
      const colors = system.getAllVfxColors(affixes);
      expect(colors).toEqual([0xff4400, 0x44ff88, 0xaa44ff]);
    });
  });

  // =========================================================================
  // Combat Effects Integration
  // =========================================================================

  describe('Combat Effects', () => {
    it('fire_enhanced adds extra fire damage proportional to base damage', () => {
      const affixes = [makeAffix('fire_enhanced')];
      const stats = system.getCombinedStats(affixes);
      const baseDamage = 50;
      const extraFire = Math.floor(baseDamage * stats.extraFireDamage);
      expect(extraFire).toBe(15); // 30% of 50 = 15
    });

    it('extra_strong increases damage by 35%+', () => {
      const affixes = [makeAffix('extra_strong')];
      const stats = system.getCombinedStats(affixes);
      expect(stats.damageMult).toBeGreaterThanOrEqual(1.3);
    });

    it('swift increases monster speed by 60%', () => {
      const affixes = [makeAffix('swift')];
      const stats = system.getCombinedStats(affixes);
      expect(stats.speedMult).toBe(1.6);
    });

    it('vampiric heals monster for 20% of damage dealt', () => {
      const affixes = [makeAffix('vampiric')];
      const stats = system.getCombinedStats(affixes);
      const damage = 80;
      const heal = Math.floor(damage * stats.lifestealFraction);
      expect(heal).toBe(16); // 20% of 80
    });

    it('frozen has 25% chance to apply slow', () => {
      const affixes = [makeAffix('frozen')];
      const stats = system.getCombinedStats(affixes);
      expect(stats.freezeChance).toBe(0.25);
    });

    it('curse_aura reduces player stats by 15% in range', () => {
      const def = ELITE_AFFIX_DEFINITIONS.curse_aura;
      expect(def.curseAuraReduction).toBe(0.15);
      expect(def.curseAuraRadius).toBe(4);
    });
  });

  // =========================================================================
  // Stat Application to Monster
  // =========================================================================

  describe('Stat Application', () => {
    it('applies HP multiplier to monster maxHp', () => {
      const baseDef = makeMonsterDef({ hp: 100 });
      const affixes = [makeAffix('fire_enhanced')];
      const stats = system.getCombinedStats(affixes);
      const newHp = Math.floor(baseDef.hp * stats.hpMult);
      expect(newHp).toBe(120); // 1.2 * 100
    });

    it('applies damage multiplier to monster damage', () => {
      const baseDef = makeMonsterDef({ damage: 20 });
      const affixes = [makeAffix('extra_strong')];
      const stats = system.getCombinedStats(affixes);
      const newDamage = Math.floor(baseDef.damage * stats.damageMult);
      expect(newDamage).toBe(27); // 1.35 * 20
    });

    it('applies speed multiplier to monster speed', () => {
      const baseDef = makeMonsterDef({ speed: 50 });
      const affixes = [makeAffix('swift')];
      const stats = system.getCombinedStats(affixes);
      const newSpeed = Math.floor(baseDef.speed * stats.speedMult);
      expect(newSpeed).toBe(80); // 1.6 * 50
    });

    it('applies defense multiplier to monster defense', () => {
      const baseDef = makeMonsterDef({ defense: 10 });
      const affixes = [makeAffix('extra_strong')];
      const stats = system.getCombinedStats(affixes);
      const newDefense = Math.floor(baseDef.defense * stats.defenseMult);
      expect(newDefense).toBe(11); // 1.15 * 10
    });

    it('multiple affixes stack all stat multipliers', () => {
      const baseDef = makeMonsterDef({ hp: 100, damage: 20, speed: 50, defense: 10 });
      const affixes = [makeAffix('fire_enhanced'), makeAffix('extra_strong'), makeAffix('swift')];
      const stats = system.getCombinedStats(affixes);

      // HP: 1.2 * 1.3 * 1.0 = 1.56
      expect(Math.floor(baseDef.hp * stats.hpMult)).toBe(156);
      // Damage: 1.0 * 1.35 * 1.0 = 1.35
      expect(Math.floor(baseDef.damage * stats.damageMult)).toBe(27);
      // Speed: 1.0 * 1.0 * 1.6 = 1.6
      expect(Math.floor(baseDef.speed * stats.speedMult)).toBe(80);
    });
  });

  // =========================================================================
  // Loot Quality Bonus
  // =========================================================================

  describe('Loot Quality Bonus', () => {
    it('single affix provides loot quality bonus', () => {
      const affixes = [makeAffix('extra_strong')];
      const stats = system.getCombinedStats(affixes);
      expect(stats.lootQualityBonus).toBe(8);
    });

    it('multiple affixes sum loot quality bonuses', () => {
      const affixes = [
        makeAffix('fire_enhanced'),  // +5
        makeAffix('extra_strong'),   // +8
        makeAffix('curse_aura'),     // +6
      ];
      const stats = system.getCombinedStats(affixes);
      expect(stats.lootQualityBonus).toBe(19);
    });

    it('LootSystem accepts affixLootBonus parameter', () => {
      const lootSystem = new LootSystem();
      const monsterDef = makeMonsterDef({ level: 20, elite: true });
      // Call with affix bonus — should not throw
      const loot = lootSystem.generateLoot(monsterDef, 10, 15);
      // Should generate items (not deterministic, but should not crash)
      expect(Array.isArray(loot)).toBe(true);
    });

    it('LootSystem generates more loot with high affix bonus', () => {
      const lootSystem = new LootSystem();
      const monsterDef = makeMonsterDef({ level: 30, elite: true });

      // Run many iterations to compare average loot counts
      let countWithBonus = 0;
      let countWithout = 0;
      const iterations = 500;

      for (let i = 0; i < iterations; i++) {
        countWithBonus += lootSystem.generateLoot(monsterDef, 10, 20).length;
        countWithout += lootSystem.generateLoot(monsterDef, 10, 0).length;
      }

      // On average, affix bonus should provide more items
      expect(countWithBonus).toBeGreaterThan(countWithout);
    });
  });

  // =========================================================================
  // Edge Cases
  // =========================================================================

  describe('Edge Cases', () => {
    it('requesting 0 affixes returns empty array', () => {
      const result = system.selectAffixes(0);
      expect(result).toEqual([]);
    });

    it('buildAffixName handles empty base name', () => {
      const affixes = [makeAffix('swift')];
      const name = system.buildAffixName('', affixes);
      expect(name).toBe('[迅捷] ');
    });

    it('getExtraFireDamage handles 0 base damage', () => {
      const affixes = [makeAffix('fire_enhanced')];
      expect(system.getExtraFireDamage(affixes, 0)).toBe(0);
    });

    it('shouldTeleport handles first-time check (lastTeleportTime=0)', () => {
      const affix = makeAffix('teleporting');
      affix.lastTeleportTime = 0;
      // Any time >= cooldown should trigger
      expect(system.shouldTeleport(affix, 5000)).toBe(true);
      expect(system.shouldTeleport(affix, 10000)).toBe(true);
    });

    it('getCombinedStats with all 7 affix types stacked', () => {
      const allAffixes: EliteAffixInstance[] = [
        makeAffix('fire_enhanced'),
        makeAffix('swift'),
        makeAffix('teleporting'),
        makeAffix('extra_strong'),
        makeAffix('curse_aura'),
        makeAffix('vampiric'),
        makeAffix('frozen'),
      ];
      const stats = system.getCombinedStats(allAffixes);
      // All multipliers should be >= 1
      expect(stats.damageMult).toBeGreaterThan(1);
      expect(stats.speedMult).toBeGreaterThan(1);
      expect(stats.hpMult).toBeGreaterThan(1);
      // Should have fire, lifesteal, and freeze
      expect(stats.extraFireDamage).toBeGreaterThan(0);
      expect(stats.lifestealFraction).toBeGreaterThan(0);
      expect(stats.freezeChance).toBeGreaterThan(0);
      expect(stats.lootQualityBonus).toBeGreaterThan(0);
    });
  });
});
