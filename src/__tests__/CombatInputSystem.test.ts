import { describe, expect, it } from 'vitest';
import {
  CombatInputBuffer,
  DodgeController,
  cycleTargetId,
} from '../systems/CombatInputSystem';

describe('CombatInputBuffer', () => {
  it('returns an immediately executable action without queueing it', () => {
    const buffer = new CombatInputBuffer(180);
    const result = buffer.request('fireball', 1000, true);

    expect(result).toBe('execute');
    expect(buffer.peek()).toBeNull();
  });

  it('buffers an unavailable action for the configured grace window', () => {
    const buffer = new CombatInputBuffer(180);
    expect(buffer.request('fireball', 1000, false)).toBe('buffered');

    expect(buffer.consumeReady(1179, () => true)?.actionId).toBe('fireball');
    expect(buffer.peek()).toBeNull();
  });

  it('keeps a buffered action while its execution condition is still false', () => {
    const buffer = new CombatInputBuffer(180);
    buffer.request('fireball', 1000, false);

    expect(buffer.consumeReady(1100, () => false)).toBeNull();
    expect(buffer.peek()?.actionId).toBe('fireball');
  });

  it('expires stale actions and lets the newest request replace the old one', () => {
    const buffer = new CombatInputBuffer(180);
    buffer.request('fireball', 1000, false);
    buffer.request('teleport', 1050, false);

    expect(buffer.peek()?.actionId).toBe('teleport');
    expect(buffer.consumeReady(1231, () => true)).toBeNull();
    expect(buffer.peek()).toBeNull();
  });

  it('can be cleared when changing zones or dying', () => {
    const buffer = new CombatInputBuffer();
    buffer.request('slash', 100, false);
    buffer.clear();

    expect(buffer.peek()).toBeNull();
  });
});

describe('DodgeController', () => {
  it('starts a dodge with cooldown and invulnerability windows', () => {
    const dodge = new DodgeController({
      cooldownMs: 900,
      invulnerabilityMs: 220,
    });

    expect(dodge.tryStart(1000)).toBe(true);
    expect(dodge.isInvulnerable(1219)).toBe(true);
    expect(dodge.isInvulnerable(1220)).toBe(false);
    expect(dodge.isReady(1899)).toBe(false);
    expect(dodge.isReady(1900)).toBe(true);
  });

  it('rejects repeated dodge attempts during cooldown', () => {
    const dodge = new DodgeController({ cooldownMs: 900, invulnerabilityMs: 220 });

    expect(dodge.tryStart(1000)).toBe(true);
    expect(dodge.tryStart(1500)).toBe(false);
    expect(dodge.tryStart(1900)).toBe(true);
  });

  it('reports normalized cooldown progress', () => {
    const dodge = new DodgeController({ cooldownMs: 1000, invulnerabilityMs: 200 });
    dodge.tryStart(1000);

    expect(dodge.cooldownProgress(1000)).toBe(0);
    expect(dodge.cooldownProgress(1500)).toBeCloseTo(0.5);
    expect(dodge.cooldownProgress(2000)).toBe(1);
    expect(dodge.cooldownRemaining(1500)).toBe(500);
    expect(dodge.cooldownRemaining(2500)).toBe(0);
  });

  it('allows only one Spirit reward per invulnerability window', () => {
    const dodge = new DodgeController({ cooldownMs: 900, invulnerabilityMs: 220 });
    dodge.tryStart(1000);

    expect(dodge.claimAvoidanceReward(1100)).toBe(true);
    expect(dodge.claimAvoidanceReward(1150)).toBe(false);
    expect(dodge.claimAvoidanceReward(1220)).toBe(false);

    expect(dodge.tryStart(1900)).toBe(true);
    expect(dodge.claimAvoidanceReward(1901)).toBe(true);
  });
});

describe('cycleTargetId', () => {
  const targets = [
    { id: 'near', distanceSq: 4, alive: true },
    { id: 'far', distanceSq: 25, alive: true },
    { id: 'dead', distanceSq: 1, alive: false },
    { id: 'outside', distanceSq: 121, alive: true },
  ];

  it('selects the nearest valid target when none is selected', () => {
    expect(cycleTargetId(null, targets, 10)).toBe('near');
  });

  it('cycles through valid targets by distance and wraps', () => {
    expect(cycleTargetId('near', targets, 10)).toBe('far');
    expect(cycleTargetId('far', targets, 10)).toBe('near');
  });

  it('returns null when there are no living targets in range', () => {
    expect(cycleTargetId(null, targets, 1)).toBeNull();
  });
});
