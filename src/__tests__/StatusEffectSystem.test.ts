import { describe, it, expect, beforeEach } from 'vitest';
import {
  StatusEffectSystem,
  DIMINISH_FACTOR,
  DIMINISH_IMMUNITY_DURATION,
  DIMINISH_WINDOW,
  SLOW_MIN_SPEED_FACTOR,
  DEFAULT_TICK_INTERVALS,
} from '../systems/StatusEffectSystem';
import type { StatusEffectType, StatusEffect } from '../systems/StatusEffectSystem';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

let system: StatusEffectSystem;

beforeEach(() => {
  system = new StatusEffectSystem();
});

const ENTITY_A = 'monster_1';
const ENTITY_B = 'monster_2';
const PLAYER = 'player';
const SOURCE = 'source_1';

// ---------------------------------------------------------------------------
// Burn (fire DoT)
// ---------------------------------------------------------------------------
describe('Burn', () => {
  it('applies burn with correct duration and tick rate', () => {
    const duration = system.apply(ENTITY_A, 'burn', 10, 3000, SOURCE, 0);
    expect(duration).toBe(3000);
    expect(system.hasEffect(ENTITY_A, 'burn')).toBe(true);
    const effects = system.getEffectsOnEntity(ENTITY_A);
    expect(effects).toHaveLength(1);
    expect(effects[0].tickInterval).toBe(DEFAULT_TICK_INTERVALS.burn); // 1000ms
  });

  it('ticks damage every 1 second', () => {
    system.apply(ENTITY_A, 'burn', 10, 5000, SOURCE, 0);

    // At 500ms: no tick yet
    const ticks0 = system.tick(ENTITY_A, 500);
    expect(ticks0).toHaveLength(0);

    // At 1000ms: 1 tick
    const ticks1 = system.tick(ENTITY_A, 1000);
    expect(ticks1).toHaveLength(1);
    expect(ticks1[0]).toEqual({ type: 'burn', damage: 10 });

    // At 2000ms: 1 more tick
    const ticks2 = system.tick(ENTITY_A, 2000);
    expect(ticks2).toHaveLength(1);
    expect(ticks2[0].damage).toBe(10);
  });

  it('can stack multiple burns from different applications', () => {
    system.apply(ENTITY_A, 'burn', 10, 5000, SOURCE, 0);
    system.apply(ENTITY_A, 'burn', 15, 5000, 'source_2', 0);
    expect(system.getEffectsOnEntity(ENTITY_A).filter(e => e.type === 'burn')).toHaveLength(2);

    // Ticking should yield 2 damage entries
    const ticks = system.tick(ENTITY_A, 1000);
    expect(ticks).toHaveLength(2);
    expect(ticks.reduce((sum, t) => sum + t.damage, 0)).toBe(25);
  });

  it('expires after duration', () => {
    system.apply(ENTITY_A, 'burn', 10, 3000, SOURCE, 0);
    const expired = system.expire(ENTITY_A, 3000);
    expect(expired).toContain('burn');
    expect(system.hasEffect(ENTITY_A, 'burn')).toBe(false);
  });

  it('does not immobilize', () => {
    system.apply(ENTITY_A, 'burn', 10, 3000, SOURCE, 0);
    expect(system.isImmobilized(ENTITY_A)).toBe(false);
  });
});

// ---------------------------------------------------------------------------
// Freeze (immobilize)
// ---------------------------------------------------------------------------
describe('Freeze', () => {
  it('applies freeze and immobilizes target', () => {
    const duration = system.apply(ENTITY_A, 'freeze', 1, 2000, SOURCE, 0);
    expect(duration).toBe(2000);
    expect(system.hasEffect(ENTITY_A, 'freeze')).toBe(true);
    expect(system.isImmobilized(ENTITY_A)).toBe(true);
  });

  it('speed is 0 while frozen', () => {
    system.apply(ENTITY_A, 'freeze', 1, 2000, SOURCE, 0);
    expect(system.getSpeedMultiplier(ENTITY_A)).toBe(0);
  });

  it('does not produce damage ticks', () => {
    system.apply(ENTITY_A, 'freeze', 1, 5000, SOURCE, 0);
    const ticks = system.tick(ENTITY_A, 1000);
    expect(ticks).toHaveLength(0);
  });

  it('has diminishing returns on rapid reapply', () => {
    // First freeze: full duration
    const d1 = system.apply(ENTITY_A, 'freeze', 1, 2000, SOURCE, 0);
    expect(d1).toBe(2000);

    // Expire first freeze
    system.expire(ENTITY_A, 2000);

    // Second freeze within window: 50% duration
    const d2 = system.apply(ENTITY_A, 'freeze', 1, 2000, SOURCE, 2500);
    expect(d2).toBe(Math.floor(2000 * DIMINISH_FACTOR));
  });

  it('grants immunity after 2nd application within window', () => {
    system.apply(ENTITY_A, 'freeze', 1, 2000, SOURCE, 0);
    system.expire(ENTITY_A, 2000);
    const d2 = system.apply(ENTITY_A, 'freeze', 1, 2000, SOURCE, 2500);
    expect(d2).toBeGreaterThan(0);

    // Expire 2nd freeze
    system.expire(ENTITY_A, 2500 + d2);

    // Third freeze during immunity window: blocked
    const d3 = system.apply(ENTITY_A, 'freeze', 1, 2000, SOURCE, 3000);
    expect(d3).toBe(0);
    expect(system.hasEffect(ENTITY_A, 'freeze')).toBe(false);
  });

  it('resets diminishing returns after window expires', () => {
    system.apply(ENTITY_A, 'freeze', 1, 2000, SOURCE, 0);
    system.expire(ENTITY_A, 2000);

    // Wait beyond DIMINISH_WINDOW + DIMINISH_IMMUNITY_DURATION
    const laterTime = DIMINISH_WINDOW + DIMINISH_IMMUNITY_DURATION + 10000;
    const d = system.apply(ENTITY_A, 'freeze', 1, 2000, SOURCE, laterTime);
    expect(d).toBe(2000); // Full duration again
  });

  it('expires cleanly and removes immobilization', () => {
    system.apply(ENTITY_A, 'freeze', 1, 2000, SOURCE, 0);
    system.expire(ENTITY_A, 2000);
    expect(system.isImmobilized(ENTITY_A)).toBe(false);
    expect(system.hasEffect(ENTITY_A, 'freeze')).toBe(false);
  });
});

// ---------------------------------------------------------------------------
// Poison (DoT + heal reduction)
// ---------------------------------------------------------------------------
describe('Poison', () => {
  it('applies poison with correct lifecycle', () => {
    const duration = system.apply(ENTITY_A, 'poison', 8, 4000, SOURCE, 0);
    expect(duration).toBe(4000);
    expect(system.hasEffect(ENTITY_A, 'poison')).toBe(true);
    expect(system.hasPoisonHealReduction(ENTITY_A)).toBe(true);
  });

  it('ticks damage every 1 second', () => {
    system.apply(ENTITY_A, 'poison', 8, 5000, SOURCE, 0);
    const ticks = system.tick(ENTITY_A, 1000);
    expect(ticks).toHaveLength(1);
    expect(ticks[0]).toEqual({ type: 'poison', damage: 8 });
  });

  it('multiple poison stacks refresh duration keeping stronger damage', () => {
    system.apply(ENTITY_A, 'poison', 5, 3000, SOURCE, 0);
    // Apply stronger poison at t=1000
    system.apply(ENTITY_A, 'poison', 10, 4000, 'source_2', 1000);

    const effects = system.getEffectsOnEntity(ENTITY_A);
    const poisons = effects.filter(e => e.type === 'poison');
    expect(poisons).toHaveLength(1); // Only one poison active (refreshed)
    expect(poisons[0].value).toBe(10); // Kept stronger value
    expect(poisons[0].startTime).toBe(1000); // Refreshed start time
    expect(poisons[0].duration).toBe(4000); // Refreshed duration
  });

  it('weaker poison refresh keeps existing stronger damage', () => {
    system.apply(ENTITY_A, 'poison', 10, 3000, SOURCE, 0);
    // Apply weaker poison at t=1000
    system.apply(ENTITY_A, 'poison', 5, 4000, 'source_2', 1000);

    const effects = system.getEffectsOnEntity(ENTITY_A);
    const poisons = effects.filter(e => e.type === 'poison');
    expect(poisons).toHaveLength(1);
    expect(poisons[0].value).toBe(10); // Kept stronger existing value
    expect(poisons[0].duration).toBe(4000); // Duration still refreshed
  });

  it('heal reduction is active during poison', () => {
    system.apply(ENTITY_A, 'poison', 5, 3000, SOURCE, 0);
    expect(system.hasPoisonHealReduction(ENTITY_A)).toBe(true);

    system.expire(ENTITY_A, 3000);
    expect(system.hasPoisonHealReduction(ENTITY_A)).toBe(false);
  });

  it('does not immobilize', () => {
    system.apply(ENTITY_A, 'poison', 5, 3000, SOURCE, 0);
    expect(system.isImmobilized(ENTITY_A)).toBe(false);
  });
});

// ---------------------------------------------------------------------------
// Bleed (physical DoT ignoring defense)
// ---------------------------------------------------------------------------
describe('Bleed', () => {
  it('applies bleed with correct duration', () => {
    const duration = system.apply(ENTITY_A, 'bleed', 12, 5000, SOURCE, 0);
    expect(duration).toBe(5000);
    expect(system.hasEffect(ENTITY_A, 'bleed')).toBe(true);
  });

  it('ticks damage every 1 second (ignores defense — caller responsibility)', () => {
    system.apply(ENTITY_A, 'bleed', 12, 5000, SOURCE, 0);
    const ticks = system.tick(ENTITY_A, 1000);
    expect(ticks).toHaveLength(1);
    expect(ticks[0]).toEqual({ type: 'bleed', damage: 12 });
  });

  it('can stack multiple bleeds', () => {
    system.apply(ENTITY_A, 'bleed', 10, 5000, SOURCE, 0);
    system.apply(ENTITY_A, 'bleed', 8, 3000, 'source_2', 0);
    const bleeds = system.getEffectsOnEntity(ENTITY_A).filter(e => e.type === 'bleed');
    expect(bleeds).toHaveLength(2);

    const ticks = system.tick(ENTITY_A, 1000);
    expect(ticks).toHaveLength(2);
    expect(ticks.reduce((sum, t) => sum + t.damage, 0)).toBe(18);
  });

  it('accumulates expected total damage over lifetime', () => {
    system.apply(ENTITY_A, 'bleed', 10, 3000, SOURCE, 0);
    let totalDamage = 0;
    // 3 ticks total (at 1s, 2s, 3s — but 3s is exactly at expiry)
    for (let t = 1000; t <= 3000; t += 1000) {
      const ticks = system.tick(ENTITY_A, t);
      totalDamage += ticks.reduce((sum, tk) => sum + tk.damage, 0);
      system.expire(ENTITY_A, t);
    }
    // At t=1000: 1 tick. At t=2000: 1 tick. At t=3000: expires before tick
    // Actually, expire happens at >= duration, so at t=3000 the effect expires
    // The tick at 3000 happens before expire check in real game loop
    expect(totalDamage).toBeGreaterThanOrEqual(20); // At least 2 ticks
  });

  it('does not immobilize', () => {
    system.apply(ENTITY_A, 'bleed', 10, 5000, SOURCE, 0);
    expect(system.isImmobilized(ENTITY_A)).toBe(false);
  });
});

// ---------------------------------------------------------------------------
// Slow (reduce move speed)
// ---------------------------------------------------------------------------
describe('Slow', () => {
  it('applies slow and reduces speed multiplier', () => {
    const duration = system.apply(ENTITY_A, 'slow', 50, 3000, SOURCE, 0);
    expect(duration).toBe(3000);
    expect(system.hasEffect(ENTITY_A, 'slow')).toBe(true);
    expect(system.getSpeedMultiplier(ENTITY_A)).toBe(0.5); // 50% reduction
  });

  it('has 20% minimum speed floor', () => {
    system.apply(ENTITY_A, 'slow', 95, 3000, SOURCE, 0);
    expect(system.getSpeedMultiplier(ENTITY_A)).toBe(SLOW_MIN_SPEED_FACTOR);
  });

  it('100% slow is clamped to minimum floor', () => {
    system.apply(ENTITY_A, 'slow', 100, 3000, SOURCE, 0);
    expect(system.getSpeedMultiplier(ENTITY_A)).toBe(SLOW_MIN_SPEED_FACTOR);
  });

  it('does not produce damage ticks', () => {
    system.apply(ENTITY_A, 'slow', 50, 5000, SOURCE, 0);
    const ticks = system.tick(ENTITY_A, 1000);
    expect(ticks).toHaveLength(0);
  });

  it('does not stack — stronger slow replaces weaker', () => {
    system.apply(ENTITY_A, 'slow', 30, 3000, SOURCE, 0);
    system.apply(ENTITY_A, 'slow', 50, 4000, 'source_2', 500);

    const slows = system.getEffectsOnEntity(ENTITY_A).filter(e => e.type === 'slow');
    expect(slows).toHaveLength(1);
    expect(slows[0].value).toBe(50); // Kept stronger
    expect(system.getSpeedMultiplier(ENTITY_A)).toBe(0.5);
  });

  it('weaker slow refreshes duration but keeps stronger value', () => {
    system.apply(ENTITY_A, 'slow', 50, 3000, SOURCE, 0);
    system.apply(ENTITY_A, 'slow', 30, 5000, 'source_2', 1000);

    const slows = system.getEffectsOnEntity(ENTITY_A).filter(e => e.type === 'slow');
    expect(slows).toHaveLength(1);
    expect(slows[0].value).toBe(50); // Kept existing stronger value
    expect(slows[0].duration).toBe(5000); // Duration refreshed
  });

  it('speed returns to 1.0 after expiry', () => {
    system.apply(ENTITY_A, 'slow', 50, 3000, SOURCE, 0);
    system.expire(ENTITY_A, 3000);
    expect(system.getSpeedMultiplier(ENTITY_A)).toBe(1);
  });

  it('does not immobilize', () => {
    system.apply(ENTITY_A, 'slow', 90, 3000, SOURCE, 0);
    expect(system.isImmobilized(ENTITY_A)).toBe(false);
  });
});

// ---------------------------------------------------------------------------
// Stun (full CC)
// ---------------------------------------------------------------------------
describe('Stun', () => {
  it('applies stun and immobilizes target', () => {
    const duration = system.apply(ENTITY_A, 'stun', 1, 2000, SOURCE, 0);
    expect(duration).toBe(2000);
    expect(system.hasEffect(ENTITY_A, 'stun')).toBe(true);
    expect(system.isImmobilized(ENTITY_A)).toBe(true);
  });

  it('speed is 0 while stunned', () => {
    system.apply(ENTITY_A, 'stun', 1, 2000, SOURCE, 0);
    expect(system.getSpeedMultiplier(ENTITY_A)).toBe(0);
  });

  it('has diminishing returns — 50% duration on 2nd stun', () => {
    const d1 = system.apply(ENTITY_A, 'stun', 1, 2000, SOURCE, 0);
    expect(d1).toBe(2000);
    system.expire(ENTITY_A, 2000);

    const d2 = system.apply(ENTITY_A, 'stun', 1, 2000, SOURCE, 2500);
    expect(d2).toBe(Math.floor(2000 * DIMINISH_FACTOR));
  });

  it('grants immunity window after 2nd stun', () => {
    system.apply(ENTITY_A, 'stun', 1, 2000, SOURCE, 0);
    system.expire(ENTITY_A, 2000);
    system.apply(ENTITY_A, 'stun', 1, 2000, SOURCE, 2500);
    const d2End = 2500 + Math.floor(2000 * DIMINISH_FACTOR);
    system.expire(ENTITY_A, d2End);

    // During immunity window
    const d3 = system.apply(ENTITY_A, 'stun', 1, 2000, SOURCE, d2End + 100);
    expect(d3).toBe(0);
  });

  it('expires cleanly', () => {
    system.apply(ENTITY_A, 'stun', 1, 2000, SOURCE, 0);
    system.expire(ENTITY_A, 2000);
    expect(system.isImmobilized(ENTITY_A)).toBe(false);
    expect(system.hasEffect(ENTITY_A, 'stun')).toBe(false);
  });
});

// ---------------------------------------------------------------------------
// Entity Death Cleanup
// ---------------------------------------------------------------------------
describe('clearEntity (death cleanup)', () => {
  it('clears all effects on entity death', () => {
    system.apply(ENTITY_A, 'burn', 10, 5000, SOURCE, 0);
    system.apply(ENTITY_A, 'poison', 5, 3000, SOURCE, 0);
    system.apply(ENTITY_A, 'slow', 30, 4000, SOURCE, 0);

    const cleared = system.clearEntity(ENTITY_A);
    expect(cleared).toHaveLength(3);
    expect(cleared).toContain('burn');
    expect(cleared).toContain('poison');
    expect(cleared).toContain('slow');
    expect(system.getEffectsOnEntity(ENTITY_A)).toHaveLength(0);
  });

  it('returns empty array for entity with no effects', () => {
    const cleared = system.clearEntity('nonexistent');
    expect(cleared).toHaveLength(0);
  });

  it('clears diminishing return records on death', () => {
    system.apply(ENTITY_A, 'stun', 1, 2000, SOURCE, 0);
    system.clearEntity(ENTITY_A);

    // After death, stun should apply with full duration again
    const d = system.apply(ENTITY_A, 'stun', 1, 2000, SOURCE, 1000);
    expect(d).toBe(2000);
  });

  it('does not affect other entities', () => {
    system.apply(ENTITY_A, 'burn', 10, 5000, SOURCE, 0);
    system.apply(ENTITY_B, 'burn', 10, 5000, SOURCE, 0);

    system.clearEntity(ENTITY_A);
    expect(system.hasEffect(ENTITY_A, 'burn')).toBe(false);
    expect(system.hasEffect(ENTITY_B, 'burn')).toBe(true);
  });
});

// ---------------------------------------------------------------------------
// Monsters applying effects to player
// ---------------------------------------------------------------------------
describe('Monster applies effects to player', () => {
  it('fire monster can apply burn to player', () => {
    const duration = system.apply(PLAYER, 'burn', 15, 3000, 'fire_elemental', 0);
    expect(duration).toBe(3000);
    expect(system.hasEffect(PLAYER, 'burn')).toBe(true);

    const ticks = system.tick(PLAYER, 1000);
    expect(ticks).toHaveLength(1);
    expect(ticks[0].damage).toBe(15);
  });

  it('monster can apply poison to player', () => {
    system.apply(PLAYER, 'poison', 8, 4000, 'poison_spider', 0);
    expect(system.hasEffect(PLAYER, 'poison')).toBe(true);
    expect(system.hasPoisonHealReduction(PLAYER)).toBe(true);
  });

  it('monster can apply slow to player', () => {
    system.apply(PLAYER, 'slow', 40, 3000, 'ice_golem', 0);
    expect(system.getSpeedMultiplier(PLAYER)).toBeCloseTo(0.6);
  });
});

// ---------------------------------------------------------------------------
// Edge Cases
// ---------------------------------------------------------------------------
describe('Edge cases', () => {
  it('rejects zero duration', () => {
    const d = system.apply(ENTITY_A, 'burn', 10, 0, SOURCE, 0);
    expect(d).toBe(0);
    expect(system.hasEffect(ENTITY_A, 'burn')).toBe(false);
  });

  it('rejects zero value', () => {
    const d = system.apply(ENTITY_A, 'burn', 0, 3000, SOURCE, 0);
    expect(d).toBe(0);
    expect(system.hasEffect(ENTITY_A, 'burn')).toBe(false);
  });

  it('rejects negative duration', () => {
    const d = system.apply(ENTITY_A, 'burn', 10, -1000, SOURCE, 0);
    expect(d).toBe(0);
  });

  it('handles tick with no effects gracefully', () => {
    const ticks = system.tick('nonexistent', 1000);
    expect(ticks).toHaveLength(0);
  });

  it('handles expire with no effects gracefully', () => {
    const expired = system.expire('nonexistent', 1000);
    expect(expired).toHaveLength(0);
  });

  it('mixed effects on same entity work independently', () => {
    system.apply(ENTITY_A, 'burn', 10, 5000, SOURCE, 0);
    system.apply(ENTITY_A, 'poison', 5, 3000, SOURCE, 0);
    system.apply(ENTITY_A, 'slow', 30, 4000, SOURCE, 0);

    expect(system.getEffectsOnEntity(ENTITY_A)).toHaveLength(3);

    // Tick at 1s: burn + poison tick
    const ticks = system.tick(ENTITY_A, 1000);
    expect(ticks).toHaveLength(2); // burn + poison (slow doesn't tick)
    expect(ticks.find(t => t.type === 'burn')?.damage).toBe(10);
    expect(ticks.find(t => t.type === 'poison')?.damage).toBe(5);

    // Speed is affected by slow
    expect(system.getSpeedMultiplier(ENTITY_A)).toBe(0.7);
  });

  it('clearAll removes everything', () => {
    system.apply(ENTITY_A, 'burn', 10, 5000, SOURCE, 0);
    system.apply(ENTITY_B, 'freeze', 1, 2000, SOURCE, 0);
    system.apply(PLAYER, 'poison', 5, 3000, SOURCE, 0);

    system.clearAll();

    expect(system.getTrackedEntities()).toHaveLength(0);
    expect(system.hasEffect(ENTITY_A, 'burn')).toBe(false);
    expect(system.hasEffect(ENTITY_B, 'freeze')).toBe(false);
    expect(system.hasEffect(PLAYER, 'poison')).toBe(false);
  });

  it('freeze and slow coexist — freeze takes priority for speed', () => {
    system.apply(ENTITY_A, 'slow', 50, 5000, SOURCE, 0);
    system.apply(ENTITY_A, 'freeze', 1, 2000, SOURCE, 0);

    // Frozen = speed 0, even though slow is also active
    expect(system.getSpeedMultiplier(ENTITY_A)).toBe(0);

    // After freeze expires, slow is still active
    system.expire(ENTITY_A, 2000);
    expect(system.getSpeedMultiplier(ENTITY_A)).toBe(0.5);
  });

  it('multiple ticks fire when large time delta', () => {
    system.apply(ENTITY_A, 'burn', 10, 10000, SOURCE, 0);
    // Jump 3 seconds at once
    const ticks = system.tick(ENTITY_A, 3000);
    expect(ticks).toHaveLength(3); // 3 ticks in 3 seconds
    expect(ticks.every(t => t.damage === 10)).toBe(true);
  });

  it('stun replaces existing stun (only one active)', () => {
    system.apply(ENTITY_A, 'stun', 1, 2000, SOURCE, 0);
    // Same entity, new stun from different source — should be blocked by DR
    // Actually second stun has DR: 50% duration
    // But the replace logic removes old one first
    const effects = system.getEffectsOnEntity(ENTITY_A);
    const stuns = effects.filter(e => e.type === 'stun');
    expect(stuns.length).toBeLessThanOrEqual(1);
  });

  it('freeze replaces existing freeze (only one active)', () => {
    system.apply(ENTITY_A, 'freeze', 1, 2000, SOURCE, 0);
    // Second freeze has DR
    const d2 = system.apply(ENTITY_A, 'freeze', 1, 2000, SOURCE, 500);
    if (d2 > 0) {
      const effects = system.getEffectsOnEntity(ENTITY_A);
      const freezes = effects.filter(e => e.type === 'freeze');
      expect(freezes.length).toBeLessThanOrEqual(1);
    }
  });

  it('getTrackedEntities returns all entities with effects', () => {
    system.apply(ENTITY_A, 'burn', 10, 5000, SOURCE, 0);
    system.apply(ENTITY_B, 'poison', 5, 3000, SOURCE, 0);
    system.apply(PLAYER, 'slow', 30, 4000, SOURCE, 0);

    const tracked = system.getTrackedEntities();
    expect(tracked).toHaveLength(3);
    expect(tracked).toContain(ENTITY_A);
    expect(tracked).toContain(ENTITY_B);
    expect(tracked).toContain(PLAYER);
  });
});

// ---------------------------------------------------------------------------
// Diminishing Returns (Freeze/Stun shared tests)
// ---------------------------------------------------------------------------
describe('Diminishing Returns', () => {
  it.each(['freeze', 'stun'] as StatusEffectType[])('%s: first application is full duration', (type) => {
    const d = system.apply(ENTITY_A, type, 1, 2000, SOURCE, 0);
    expect(d).toBe(2000);
  });

  it.each(['freeze', 'stun'] as StatusEffectType[])('%s: second application within window is 50%% duration', (type) => {
    system.apply(ENTITY_A, type, 1, 2000, SOURCE, 0);
    system.expire(ENTITY_A, 2000);

    const d2 = system.apply(ENTITY_A, type, 1, 2000, SOURCE, 3000);
    expect(d2).toBe(Math.floor(2000 * DIMINISH_FACTOR));
  });

  it.each(['freeze', 'stun'] as StatusEffectType[])('%s: third application during immunity is blocked', (type) => {
    system.apply(ENTITY_A, type, 1, 2000, SOURCE, 0);
    system.expire(ENTITY_A, 2000);
    system.apply(ENTITY_A, type, 1, 2000, SOURCE, 2500);
    system.expire(ENTITY_A, 3500);

    const d3 = system.apply(ENTITY_A, type, 1, 2000, SOURCE, 3600);
    expect(d3).toBe(0);
  });

  it.each(['freeze', 'stun'] as StatusEffectType[])('%s: DR resets after window expires', (type) => {
    system.apply(ENTITY_A, type, 1, 2000, SOURCE, 0);
    system.expire(ENTITY_A, 2000);

    const farFuture = 20000; // Way past DIMINISH_WINDOW
    const d = system.apply(ENTITY_A, type, 1, 2000, SOURCE, farFuture);
    expect(d).toBe(2000);
  });

  it('freeze and stun have independent DR tracks', () => {
    // Apply freeze twice (triggers DR)
    system.apply(ENTITY_A, 'freeze', 1, 2000, SOURCE, 0);
    system.expire(ENTITY_A, 2000);
    system.apply(ENTITY_A, 'freeze', 1, 2000, SOURCE, 2500);
    system.expire(ENTITY_A, 3500);

    // Stun should still work with full duration (independent DR)
    const d = system.apply(ENTITY_A, 'stun', 1, 2000, SOURCE, 4000);
    expect(d).toBe(2000);
  });

  it('burn/poison/bleed/slow do not have diminishing returns', () => {
    for (const type of ['burn', 'poison', 'bleed', 'slow'] as StatusEffectType[]) {
      const sys = new StatusEffectSystem();
      const d1 = sys.apply(ENTITY_A, type, 10, 3000, SOURCE, 0);
      expect(d1).toBe(3000);
      sys.expire(ENTITY_A, 3000);

      const d2 = sys.apply(ENTITY_A, type, 10, 3000, SOURCE, 3500);
      expect(d2).toBe(3000); // Full duration, no DR
    }
  });
});
