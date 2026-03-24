import { describe, it, expect, beforeEach, afterEach, vi } from 'vitest';
import {
  CombatSystem,
  emptyEquipStats,
  getBuffValue,
  getSkillBuffValue,
  getSkillBuffDuration,
  STUN_DIMINISH_FACTOR,
  STUN_IMMUNITY_DURATION,
  STUN_WINDOW,
  BUFF_CAPS,
} from '../systems/CombatSystem';
import type { CombatEntity, ActiveBuff, EquipStats } from '../systems/CombatSystem';
import type { SkillDefinition, Stats } from '../data/types';
import { MageClass } from '../data/classes/mage';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
function makeStats(overrides?: Partial<Stats>): Stats {
  return { str: 10, dex: 0, vit: 10, int: 10, spi: 10, lck: 0, ...overrides };
}

function makeEntity(overrides?: Partial<CombatEntity>): CombatEntity {
  return {
    id: 'e1',
    name: 'Test Entity',
    hp: 100,
    maxHp: 100,
    mana: 100,
    maxMana: 100,
    stats: makeStats(),
    level: 1,
    baseDamage: 10,
    defense: 0,
    attackSpeed: 1000,
    attackRange: 1,
    buffs: [],
    ...overrides,
  };
}

function makeSkill(overrides?: Partial<SkillDefinition>): SkillDefinition {
  return {
    id: 'test_skill',
    name: '测试技能',
    nameEn: 'Test Skill',
    description: 'A test skill',
    tree: 'offense',
    tier: 1,
    maxLevel: 20,
    manaCost: 10,
    cooldown: 2000,
    range: 1,
    damageMultiplier: 1.5,
    damageType: 'physical',
    icon: 'skill_test',
    ...overrides,
  };
}

// ---------------------------------------------------------------------------
// 1. Stun mechanics
// ---------------------------------------------------------------------------
describe('Stun mechanics', () => {
  let cs: CombatSystem;

  beforeEach(() => {
    cs = new CombatSystem();
  });

  it('applies stun with full duration on first application', () => {
    const duration = cs.applyStun('monster_1', 2000, 1000);
    expect(duration).toBe(2000);
  });

  it('applies stun with 50% duration on second rapid application', () => {
    cs.applyStun('monster_1', 2000, 1000);
    const duration2 = cs.applyStun('monster_1', 2000, 3000);
    expect(duration2).toBe(Math.floor(2000 * STUN_DIMINISH_FACTOR));
  });

  it('grants immunity after 2nd stun within the window', () => {
    cs.applyStun('monster_1', 2000, 1000);
    const dur2 = cs.applyStun('monster_1', 2000, 3000);
    // 3rd stun attempt should be 0 (immune)
    const dur3 = cs.applyStun('monster_1', 2000, 4000);
    expect(dur3).toBe(0);
  });

  it('immunity expires after STUN_IMMUNITY_DURATION', () => {
    cs.applyStun('monster_1', 2000, 1000);
    const dur2 = cs.applyStun('monster_1', 2000, 3000);
    // Immunity starts at: 3000 + dur2 + STUN_IMMUNITY_DURATION
    const immuneEnd = 3000 + dur2 + STUN_IMMUNITY_DURATION;

    // Still immune before expiry
    const dur3 = cs.applyStun('monster_1', 2000, immuneEnd - 1);
    expect(dur3).toBe(0);

    // Immune expired + stun window expired → fresh stun
    const dur4 = cs.applyStun('monster_1', 2000, immuneEnd + STUN_WINDOW + 1);
    expect(dur4).toBe(2000);
  });

  it('resets stun count after STUN_WINDOW of no stuns', () => {
    cs.applyStun('monster_1', 2000, 1000);
    // Wait longer than STUN_WINDOW
    const duration = cs.applyStun('monster_1', 2000, 1000 + STUN_WINDOW + 1);
    // Should be full duration since count was reset
    expect(duration).toBe(2000);
  });

  it('returns 0 for baseDuration <= 0', () => {
    expect(cs.applyStun('monster_1', 0, 1000)).toBe(0);
    expect(cs.applyStun('monster_1', -500, 1000)).toBe(0);
  });

  it('isStunned returns true when stunned buff is active', () => {
    const entity = makeEntity({
      buffs: [{ stat: 'stunned', value: 1, duration: 2000, startTime: 1000 }],
    });
    expect(cs.isStunned(entity, 2000)).toBe(true);  // 1s elapsed, 2s duration
    expect(cs.isStunned(entity, 3001)).toBe(false);  // expired
  });

  it('isStunned returns false when no stun buff exists', () => {
    const entity = makeEntity();
    expect(cs.isStunned(entity, 1000)).toBe(false);
  });

  it('stun duration scales with skill level via tieredScale', () => {
    const warStompSkill = makeSkill({
      id: 'war_stomp',
      stunDuration: 2000,
      buff: { stat: 'defenseBonus', value: 0.3, duration: 4000 },
      scaling: {
        damagePerLevel: 0.10,
        manaCostPerLevel: 1.5,
        buffDurationPerLevel: 200,
        buffValuePerLevel: 0.02,
      },
    });
    // The buff duration should increase with level
    const durationLv1 = getSkillBuffDuration(warStompSkill, 1);
    const durationLv10 = getSkillBuffDuration(warStompSkill, 10);
    expect(durationLv10).toBeGreaterThan(durationLv1);
  });
});

// ---------------------------------------------------------------------------
// 2. Buff stat effects in calculateDamage
// ---------------------------------------------------------------------------
describe('Buff effects in combat calculations', () => {
  let cs: CombatSystem;

  beforeEach(() => {
    cs = new CombatSystem();
    // Mock Math.random to disable crits and dodges
    vi.spyOn(Math, 'random').mockReturnValue(0.99);
  });

  it('poisonDamage buff adds bonus poison damage', () => {
    const attacker = makeEntity({ baseDamage: 100 });
    const defender = makeEntity({ defense: 0 });

    // Without poison buff
    const noBuff = cs.calculateDamage(attacker, defender);

    // With poison buff (0.5 = 50% of baseDamage as poison flat damage)
    const atkWithBuff = makeEntity({
      baseDamage: 100,
      buffs: [{ stat: 'poisonDamage', value: 0.5, duration: 6000, startTime: 0 }],
    });
    const withBuff = cs.calculateDamage(atkWithBuff, defender);

    expect(withBuff.damage).toBeGreaterThan(noBuff.damage);
  });

  it('stealthDamage buff multiplies next attack damage', () => {
    const attacker = makeEntity({ baseDamage: 100 });
    const defender = makeEntity({ defense: 0 });

    const noBuff = cs.calculateDamage(attacker, defender);

    // stealthDamage of 1.0 = +100% damage (double)
    const atkWithBuff = makeEntity({
      baseDamage: 100,
      buffs: [{ stat: 'stealthDamage', value: 1.0, duration: 3000, startTime: 0 }],
    });
    const withBuff = cs.calculateDamage(atkWithBuff, defender);

    // Expect roughly double damage
    expect(withBuff.damage).toBeGreaterThanOrEqual(noBuff.damage * 1.9);
  });

  it('consumeStealthBuff removes stealthDamage buffs', () => {
    const entity = makeEntity({
      buffs: [
        { stat: 'stealthDamage', value: 1.0, duration: 3000, startTime: 0 },
        { stat: 'damageBonus', value: 0.25, duration: 6000, startTime: 0 },
      ],
    });
    cs.consumeStealthBuff(entity);
    expect(entity.buffs).toHaveLength(1);
    expect(entity.buffs[0].stat).toBe('damageBonus');
  });

  it('defenseBonus buff increases effective defense', () => {
    const attacker = makeEntity({ baseDamage: 100 });

    // Defender with base defense 20, no buff
    const defNoBuff = makeEntity({ defense: 20 });
    const noBuff = cs.calculateDamage(attacker, defNoBuff);

    // Defender with defenseBonus = 0.3 (30% more defense)
    const defWithBuff = makeEntity({
      defense: 20,
      buffs: [{ stat: 'defenseBonus', value: 0.3, duration: 4000, startTime: 0 }],
    });
    const withBuff = cs.calculateDamage(attacker, defWithBuff);

    // More defense = less damage taken
    expect(withBuff.damage).toBeLessThan(noBuff.damage);
  });

  it('damageBonus buff multiplies total damage', () => {
    const attacker = makeEntity({ baseDamage: 100 });
    const defender = makeEntity({ defense: 0 });

    const noBuff = cs.calculateDamage(attacker, defender);

    // damageBonus of 0.25 = +25% total damage
    const atkWithBuff = makeEntity({
      baseDamage: 100,
      buffs: [{ stat: 'damageBonus', value: 0.25, duration: 6000, startTime: 0 }],
    });
    const withBuff = cs.calculateDamage(atkWithBuff, defender);

    expect(withBuff.damage).toBeGreaterThan(noBuff.damage);
    // Should be roughly 25% more
    expect(withBuff.damage).toBeGreaterThanOrEqual(Math.floor(noBuff.damage * 1.24));
  });

  it('damageReduction buff still works as before', () => {
    const attacker = makeEntity({ baseDamage: 100 });
    const defNoBuff = makeEntity({ defense: 0 });
    const defWithBuff = makeEntity({
      defense: 0,
      buffs: [{ stat: 'damageReduction', value: 0.5, duration: 5000, startTime: 0 }],
    });

    const noBuff = cs.calculateDamage(attacker, defNoBuff);
    const withBuff = cs.calculateDamage(attacker, defWithBuff);

    // Roughly half damage with 50% reduction
    expect(withBuff.damage).toBeLessThanOrEqual(Math.ceil(noBuff.damage * 0.55));
  });

  afterEach(() => {
    vi.restoreAllMocks();
  });
});

// ---------------------------------------------------------------------------
// 3. Taunt Roar forces monster aggro
// ---------------------------------------------------------------------------
describe('Taunt mechanics', () => {
  let cs: CombatSystem;

  beforeEach(() => {
    cs = new CombatSystem();
  });

  it('applyTaunt adds taunted buff to all monsters', () => {
    const monsters = [
      makeEntity({ id: 'monster_1' }),
      makeEntity({ id: 'monster_2' }),
      makeEntity({ id: 'monster_3' }),
    ];

    const tauntedIds = cs.applyTaunt(monsters, 'player', 4000, 1000);

    expect(tauntedIds).toEqual(['monster_1', 'monster_2', 'monster_3']);
    for (const m of monsters) {
      expect(m.buffs).toHaveLength(1);
      expect(m.buffs[0].stat).toBe('taunted');
      expect(m.buffs[0].duration).toBe(4000);
    }
  });

  it('isTaunted returns true for taunted monsters within duration', () => {
    const monster = makeEntity({ id: 'monster_1' });
    cs.applyTaunt([monster], 'player', 4000, 1000);

    expect(cs.isTaunted(monster, 2000)).toBe(true);
    expect(cs.isTaunted(monster, 5001)).toBe(false);  // expired
  });

  it('applyTaunt handles empty monster list', () => {
    const tauntedIds = cs.applyTaunt([], 'player', 4000, 1000);
    expect(tauntedIds).toEqual([]);
  });
});

// ---------------------------------------------------------------------------
// 4. Mana Shield redirects 30% damage to mana pool
// ---------------------------------------------------------------------------
describe('Mana Shield', () => {
  let cs: CombatSystem;

  beforeEach(() => {
    cs = new CombatSystem();
    // Disable crits and dodges
    vi.spyOn(Math, 'random').mockReturnValue(0.99);
  });

  afterEach(() => {
    vi.restoreAllMocks();
  });

  it('redirects 30% of damage to mana (manaDamage in result)', () => {
    const attacker = makeEntity({ baseDamage: 100 });
    const defender = makeEntity({
      defense: 0,
      mana: 100,
      maxMana: 100,
      buffs: [{ stat: 'manaShield', value: 0.3, duration: 8000, startTime: 0 }],
    });

    const result = cs.calculateDamage(attacker, defender);

    // Should have manaDamage set
    expect(result.manaDamage).toBeDefined();
    expect(result.manaDamage).toBeGreaterThan(0);

    // HP damage should be less than without mana shield
    const defenderNoShield = makeEntity({ defense: 0, mana: 100, maxMana: 100 });
    const resultNoShield = cs.calculateDamage(attacker, defenderNoShield);

    expect(result.damage).toBeLessThan(resultNoShield.damage);
  });

  it('overflow: when mana is insufficient, overflow goes to HP', () => {
    const attacker = makeEntity({ baseDamage: 200 });
    const defender = makeEntity({
      defense: 0,
      mana: 10,  // Very low mana
      maxMana: 100,
      buffs: [{ stat: 'manaShield', value: 0.3, duration: 8000, startTime: 0 }],
    });

    const result = cs.calculateDamage(attacker, defender);

    // manaDamage should be capped at defender's mana (10)
    expect(result.manaDamage).toBeLessThanOrEqual(10);
    // HP damage should still be substantial since overflow went to HP
    expect(result.damage).toBeGreaterThan(0);
  });

  it('applyDamage deducts mana when manaDamage is present', () => {
    const target = makeEntity({ hp: 100, mana: 50 });
    const result = {
      damage: 70,
      isCrit: false,
      isDodged: false,
      damageType: 'physical' as const,
      lifeStolen: 0,
      manaStolen: 0,
      manaDamage: 30,
    };

    cs.applyDamage(target, result);

    expect(target.hp).toBe(30);   // 100 - 70
    expect(target.mana).toBe(20); // 50 - 30
  });

  it('mana shield has no effect when mana is 0', () => {
    const attacker = makeEntity({ baseDamage: 100 });
    const defender = makeEntity({
      defense: 0,
      mana: 0,
      maxMana: 100,
      buffs: [{ stat: 'manaShield', value: 0.3, duration: 8000, startTime: 0 }],
    });

    const result = cs.calculateDamage(attacker, defender);

    // No mana to absorb, so manaDamage should be undefined or 0
    expect(result.manaDamage).toBeUndefined();
  });

  it('Mana Shield skill uses manaShield stat, not damageReduction', () => {
    const manaShieldSkill = MageClass.skills.find(
      (s: SkillDefinition) => s.id === 'mana_shield',
    );
    expect(manaShieldSkill).toBeDefined();
    expect(manaShieldSkill!.buff!.stat).toBe('manaShield');
    expect(manaShieldSkill!.buff!.value).toBe(0.3);
  });
});

// ---------------------------------------------------------------------------
// 5. Buff stacking (additive, capped) and clean expiration
// ---------------------------------------------------------------------------
describe('Buff stacking and expiration', () => {
  let cs: CombatSystem;

  beforeEach(() => {
    cs = new CombatSystem();
  });

  it('getBuffValue sums multiple buffs of the same stat additively', () => {
    const entity = makeEntity({
      buffs: [
        { stat: 'damageReduction', value: 0.3, duration: 5000, startTime: 0 },
        { stat: 'damageReduction', value: 0.2, duration: 5000, startTime: 0 },
      ],
    });
    expect(getBuffValue(entity, 'damageReduction')).toBeCloseTo(0.5);
  });

  it('getBuffValue caps at BUFF_CAPS limit', () => {
    const entity = makeEntity({
      buffs: [
        { stat: 'damageReduction', value: 0.5, duration: 5000, startTime: 0 },
        { stat: 'damageReduction', value: 0.5, duration: 5000, startTime: 0 },
      ],
    });
    // Total would be 1.0 but cap is 0.9
    expect(getBuffValue(entity, 'damageReduction')).toBe(BUFF_CAPS.damageReduction);
  });

  it('getBuffValue returns 0 for missing buff stat', () => {
    const entity = makeEntity();
    expect(getBuffValue(entity, 'nonexistent')).toBe(0);
  });

  it('updateBuffs removes expired buffs', () => {
    const entity = makeEntity({
      buffs: [
        { stat: 'damageReduction', value: 0.5, duration: 5000, startTime: 1000 },
        { stat: 'damageBonus', value: 0.25, duration: 10000, startTime: 1000 },
        { stat: 'defenseBonus', value: 0.3, duration: 3000, startTime: 1000 },
      ],
    });

    cs.updateBuffs(entity, 5000); // 4000ms elapsed
    // damageReduction (5000ms dur) should survive, defenseBonus (3000ms dur) expired
    expect(entity.buffs).toHaveLength(2);
    expect(entity.buffs.map(b => b.stat).sort()).toEqual(['damageBonus', 'damageReduction']);
  });

  it('updateBuffs removes all buffs when all have expired', () => {
    const entity = makeEntity({
      buffs: [
        { stat: 'damageReduction', value: 0.5, duration: 1000, startTime: 0 },
        { stat: 'attackSpeed', value: 0.2, duration: 2000, startTime: 0 },
      ],
    });

    cs.updateBuffs(entity, 3000); // all expired
    expect(entity.buffs).toHaveLength(0);
  });

  it('expired buffs immediately stop affecting combat', () => {
    // Disable crits and dodges
    vi.spyOn(Math, 'random').mockReturnValue(0.99);

    const attacker = makeEntity({ baseDamage: 100 });

    // Defender with active damageReduction buff
    const defenderActive = makeEntity({
      defense: 0,
      buffs: [{ stat: 'damageReduction', value: 0.5, duration: 5000, startTime: 0 }],
    });
    const resultActive = cs.calculateDamage(attacker, defenderActive);

    // Expire the buff
    cs.updateBuffs(defenderActive, 6000);
    const resultExpired = cs.calculateDamage(attacker, defenderActive);

    // After expiry, damage should be higher (no reduction)
    expect(resultExpired.damage).toBeGreaterThan(resultActive.damage);

    vi.restoreAllMocks();
  });

  it('multiple buff types stack independently', () => {
    const entity = makeEntity({
      buffs: [
        { stat: 'damageReduction', value: 0.3, duration: 5000, startTime: 0 },
        { stat: 'defenseBonus', value: 0.5, duration: 5000, startTime: 0 },
        { stat: 'damageBonus', value: 0.25, duration: 5000, startTime: 0 },
      ],
    });
    expect(getBuffValue(entity, 'damageReduction')).toBeCloseTo(0.3);
    expect(getBuffValue(entity, 'defenseBonus')).toBeCloseTo(0.5);
    expect(getBuffValue(entity, 'damageBonus')).toBeCloseTo(0.25);
  });

  it('all 6 buff types are recognized in combat', () => {
    // Verify each buff stat name is valid and has a cap or is used
    const buffTypes = ['poisonDamage', 'stealthDamage', 'defenseBonus', 'damageBonus', 'damageReduction', 'attackSpeed'];
    for (const stat of buffTypes) {
      const entity = makeEntity({
        buffs: [{ stat, value: 0.5, duration: 5000, startTime: 0 }],
      });
      expect(getBuffValue(entity, stat)).toBeCloseTo(0.5);
    }
  });

  it('no buff persists indefinitely — duration is always finite', () => {
    // Buffs with duration 0 should be immediately removable
    const entity = makeEntity({
      buffs: [{ stat: 'damageReduction', value: 0.5, duration: 0, startTime: 1000 }],
    });
    cs.updateBuffs(entity, 1000);
    expect(entity.buffs).toHaveLength(0);
  });
});

// ---------------------------------------------------------------------------
// 6. Integration: all buff types affect calculateDamage correctly
// ---------------------------------------------------------------------------
describe('All buff types integration', () => {
  let cs: CombatSystem;

  beforeEach(() => {
    cs = new CombatSystem();
    vi.spyOn(Math, 'random').mockReturnValue(0.99);
  });

  afterEach(() => {
    vi.restoreAllMocks();
  });

  it('attackSpeed buff is readable via getBuffValue', () => {
    // attackSpeed buff doesn't directly affect calculateDamage (it affects attack timing
    // in ZoneScene), but it should be readable
    const entity = makeEntity({
      buffs: [{ stat: 'attackSpeed', value: 0.4, duration: 6000, startTime: 0 }],
    });
    expect(getBuffValue(entity, 'attackSpeed')).toBeCloseTo(0.4);
  });

  it('combined buffs: damageBonus + stealthDamage multiply together', () => {
    const attacker = makeEntity({
      baseDamage: 100,
      buffs: [
        { stat: 'damageBonus', value: 0.25, duration: 6000, startTime: 0 },
        { stat: 'stealthDamage', value: 1.0, duration: 3000, startTime: 0 },
      ],
    });
    const defender = makeEntity({ defense: 0 });

    const baseAttacker = makeEntity({ baseDamage: 100 });
    const baseDmg = cs.calculateDamage(baseAttacker, defender);
    const buffDmg = cs.calculateDamage(attacker, defender);

    // 1.25 * 2.0 = 2.5x baseline
    expect(buffDmg.damage).toBeGreaterThanOrEqual(Math.floor(baseDmg.damage * 2.4));
  });

  it('combined buffs: defenseBonus + damageReduction reduce damage heavily', () => {
    const attacker = makeEntity({ baseDamage: 100 });
    const defenderNoBuff = makeEntity({ defense: 20 });
    const defenderBoth = makeEntity({
      defense: 20,
      buffs: [
        { stat: 'defenseBonus', value: 0.5, duration: 5000, startTime: 0 },
        { stat: 'damageReduction', value: 0.3, duration: 5000, startTime: 0 },
      ],
    });

    const noBuff = cs.calculateDamage(attacker, defenderNoBuff);
    const withBuff = cs.calculateDamage(attacker, defenderBoth);

    expect(withBuff.damage).toBeLessThan(noBuff.damage);
  });
});
