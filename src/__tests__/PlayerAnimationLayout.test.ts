import { describe, expect, it } from 'vitest';
import {
  PLAYER_ACTION_FRAME_COUNTS,
  PLAYER_ACTION_ORDER,
  PLAYER_TOTAL_FRAMES,
  getPlayerActionFrameRange,
} from '../graphics/sprites/types';
import { getAnimConfig } from '../systems/CharacterAnimator';

describe('player animation frame layout', () => {
  it('provides a contiguous range for every action, including dodge', () => {
    let expectedStart = 0;

    for (const action of PLAYER_ACTION_ORDER) {
      const range = getPlayerActionFrameRange(action);
      expect(range.start).toBe(expectedStart);
      expect(range.end).toBe(expectedStart + PLAYER_ACTION_FRAME_COUNTS[action] - 1);
      expectedStart = range.end + 1;
    }

    expect(expectedStart).toBe(PLAYER_TOTAL_FRAMES);
  });

  it('uses enough poses for smooth looping and combat silhouettes', () => {
    expect(PLAYER_ACTION_FRAME_COUNTS.idle).toBeGreaterThanOrEqual(6);
    expect(PLAYER_ACTION_FRAME_COUNTS.walk).toBeGreaterThanOrEqual(8);
    expect(PLAYER_ACTION_FRAME_COUNTS.attack).toBeGreaterThanOrEqual(8);
    expect(PLAYER_ACTION_FRAME_COUNTS.cast).toBeGreaterThanOrEqual(8);
    expect(PLAYER_ACTION_FRAME_COUNTS.dodge).toBeGreaterThanOrEqual(6);
    expect(PLAYER_ACTION_FRAME_COUNTS.hurt).toBeGreaterThanOrEqual(4);
    expect(PLAYER_ACTION_FRAME_COUNTS.death).toBeGreaterThanOrEqual(6);
  });

  it.each(['warrior', 'mage', 'rogue'])(
    'defines dodge timing and class-specific cadence for %s',
    classId => {
      const config = getAnimConfig(classId);
      expect(config.dodgeDuration).toBeGreaterThan(0);
      expect(config.idleFrameRate).toBeGreaterThan(0);
      expect(config.walkFrameRate).toBeGreaterThan(0);
      expect(config.attackFrameRate).toBeGreaterThan(0);
      expect(config.castFrameRate).toBeGreaterThan(0);
      expect(config.deathFrameRate).toBeGreaterThan(0);

      const expectedDuration = (
        action: 'attack' | 'cast' | 'hurt' | 'dodge' | 'death',
        frameRate: number,
      ) => PLAYER_ACTION_FRAME_COUNTS[action] / frameRate * 1000;
      expect(Math.abs(config.attackDuration - expectedDuration('attack', config.attackFrameRate))).toBeLessThan(25);
      expect(Math.abs(config.castDuration - expectedDuration('cast', config.castFrameRate))).toBeLessThan(25);
      expect(Math.abs(config.hurtDuration - expectedDuration('hurt', config.hurtFrameRate))).toBeLessThan(25);
      expect(Math.abs(config.dodgeDuration - expectedDuration('dodge', config.dodgeFrameRate))).toBeLessThan(25);
      expect(Math.abs(config.deathDuration - expectedDuration('death', config.deathFrameRate))).toBeLessThan(25);
    },
  );

  it('keeps each class animation cadence distinct', () => {
    const warrior = getAnimConfig('warrior');
    const mage = getAnimConfig('mage');
    const rogue = getAnimConfig('rogue');

    expect(new Set([
      warrior.attackFrameRate,
      mage.attackFrameRate,
      rogue.attackFrameRate,
    ]).size).toBe(3);
    expect(rogue.attackFrameRate).toBeGreaterThan(warrior.attackFrameRate);
    expect(mage.castFrameRate).toBeGreaterThan(warrior.castFrameRate);
  });
});
