import { describe, expect, it } from 'vitest';
import {
  SpiritSystem,
  getSpiritProfile,
  type SpiritSaveState,
} from '../systems/SpiritSystem';

describe('SpiritSystem', () => {
  it('provides a distinct combat identity for each class', () => {
    const warrior = getSpiritProfile('warrior');
    const mage = getSpiritProfile('mage');
    const rogue = getSpiritProfile('rogue');

    expect(new Set([warrior.id, mage.id, rogue.id]).size).toBe(3);
    expect(new Set([warrior.visualColor, mage.visualColor, rogue.visualColor]).size).toBe(3);
    expect(warrior.resonanceDamageBonus).toBeGreaterThan(0);
    expect(mage.resonanceManaCostMultiplier).toBeLessThan(1);
    expect(rogue.dodgeGain).toBeGreaterThan(rogue.hitGain);
  });

  it('falls back to the warrior profile for an unknown class', () => {
    expect(getSpiritProfile('unknown').id).toBe(getSpiritProfile('warrior').id);
  });

  it('builds Spirit from combat and scales gain with SPI', () => {
    const spirit = new SpiritSystem('warrior');

    const baseGain = spirit.gainFromCombat('hit', 0);
    const scaledGain = spirit.gainFromCombat('hit', 20);

    expect(baseGain.gained).toBeGreaterThan(0);
    expect(scaledGain.gained).toBeGreaterThan(baseGain.gained);
    expect(spirit.value).toBe(baseGain.gained + scaledGain.gained);
  });

  it('adds the profile crit bonus on critical hits', () => {
    const normal = new SpiritSystem('mage');
    const critical = new SpiritSystem('mage');

    normal.gainFromCombat('hit', 10, false);
    critical.gainFromCombat('hit', 10, true);

    expect(critical.value).toBeGreaterThan(normal.value);
  });

  it('clamps at maximum and automatically starts Resonance', () => {
    const spirit = new SpiritSystem('rogue');
    const result = spirit.gain(999);

    expect(result.startedResonance).toBe(true);
    expect(spirit.isResonating).toBe(true);
    expect(spirit.value).toBe(spirit.maxValue);
    expect(spirit.resonanceRemainingMs).toBe(spirit.profile.resonanceDurationMs);
  });

  it('ignores zero, negative, and in-Resonance gain', () => {
    const spirit = new SpiritSystem('warrior');

    expect(spirit.gain(0).gained).toBe(0);
    expect(spirit.gain(-5).gained).toBe(0);
    spirit.gain(spirit.maxValue);
    expect(spirit.gain(20).gained).toBe(0);
  });

  it('drains linearly during Resonance and resets when it ends', () => {
    const spirit = new SpiritSystem('warrior');
    spirit.gain(spirit.maxValue);
    const halfDuration = spirit.profile.resonanceDurationMs / 2;

    const halfway = spirit.update(halfDuration);
    expect(halfway.endedResonance).toBe(false);
    expect(spirit.value).toBeCloseTo(spirit.maxValue / 2, 4);

    const ended = spirit.update(halfDuration);
    expect(ended.endedResonance).toBe(true);
    expect(spirit.isResonating).toBe(false);
    expect(spirit.value).toBe(0);
    expect(spirit.resonanceRemainingMs).toBe(0);
  });

  it('exposes Resonance combat multipliers only while active', () => {
    const spirit = new SpiritSystem('mage');

    expect(spirit.damageMultiplier).toBe(1);
    expect(spirit.manaCostMultiplier).toBe(1);

    spirit.gain(spirit.maxValue);
    expect(spirit.damageMultiplier).toBe(1 + spirit.profile.resonanceDamageBonus);
    expect(spirit.manaCostMultiplier).toBe(spirit.profile.resonanceManaCostMultiplier);
  });

  it('round-trips a charging state through save data', () => {
    const original = new SpiritSystem('warrior');
    original.gain(37);

    const restored = new SpiritSystem('warrior', original.toSaveState());

    expect(restored.toSaveState()).toEqual(original.toSaveState());
    expect(restored.isResonating).toBe(false);
  });

  it('restores and clamps malformed save values safely', () => {
    const malformed = {
      value: 999,
      resonanceRemainingMs: -100,
    } satisfies SpiritSaveState;
    const spirit = new SpiritSystem('rogue', malformed);

    expect(spirit.value).toBe(spirit.maxValue);
    expect(spirit.resonanceRemainingMs).toBe(0);
    expect(spirit.isResonating).toBe(false);
  });
});
