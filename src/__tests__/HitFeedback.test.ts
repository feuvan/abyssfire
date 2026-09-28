import { describe, expect, it } from 'vitest';
import {
  HIT_PROFILES,
  attackSpeedScale,
  classifyHit,
  computeImpactDelay,
  type HitWeight,
} from '../systems/HitFeedback';
import {
  MAX_SHEET_DIMENSION,
  PLAYER_TOTAL_FRAMES,
  buildFrameSizeRegistry,
  computeSheetGrid,
  sheetFrameOrigin,
} from '../graphics/sprites/types';
import { TEXTURE_SCALE } from '../config';
import { walkBob, walkStride } from '../graphics/sprites/players/PlayerMotion';
import { getAnimConfig } from '../systems/CharacterAnimator';

describe('classifyHit', () => {
  it('ranks kill > crit > raw damage share', () => {
    expect(classifyHit({ damage: 1, maxHp: 100, killed: true, isCrit: true })).toBe('kill');
    expect(classifyHit({ damage: 1, maxHp: 100, isCrit: true })).toBe('crit');
    expect(classifyHit({ damage: 30, maxHp: 100 })).toBe('heavy');
    expect(classifyHit({ damage: 10, maxHp: 100 })).toBe('normal');
    expect(classifyHit({ damage: 2, maxHp: 100 })).toBe('light');
  });

  it('treats damage-over-time ticks as ticks even when they kill', () => {
    expect(classifyHit({ damage: 500, maxHp: 100, killed: true, isTick: true })).toBe('tick');
  });

  it('never divides by zero on bad max HP', () => {
    expect(classifyHit({ damage: 10, maxHp: 0 })).toBe('light');
  });
});

describe('HIT_PROFILES', () => {
  const order: HitWeight[] = ['tick', 'light', 'normal', 'heavy', 'crit', 'kill'];

  it('escalates hit-stop and recoil monotonically with weight', () => {
    for (let i = 1; i < order.length; i++) {
      const prev = HIT_PROFILES[order[i - 1]];
      const cur = HIT_PROFILES[order[i]];
      expect(cur.targetStopMs).toBeGreaterThanOrEqual(prev.targetStopMs);
      expect(cur.attackerStopMs).toBeGreaterThanOrEqual(prev.attackerStopMs);
      expect(cur.recoil).toBeGreaterThanOrEqual(prev.recoil);
      expect(cur.shakeIntensity).toBeGreaterThanOrEqual(prev.shakeIntensity);
    }
  });

  it('keeps hit-stop short enough not to feel like lag', () => {
    for (const weight of order) {
      expect(HIT_PROFILES[weight].targetStopMs).toBeLessThanOrEqual(150);
    }
  });

  it('gives DoT ticks no flinch or freeze', () => {
    expect(HIT_PROFILES.tick.targetStopMs).toBe(0);
    expect(HIT_PROFILES.tick.recoil).toBe(0);
  });
});

describe('attack timing', () => {
  it('compresses animations to fit fast attack intervals', () => {
    expect(attackSpeedScale(600, 1000)).toBe(1);
    expect(attackSpeedScale(600, 400)).toBeCloseTo(0.6);
    expect(attackSpeedScale(600, 50)).toBe(0.35);
    expect(attackSpeedScale(600)).toBe(1);
  });

  it('lands the fallback impact after the wind-up', () => {
    expect(computeImpactDelay(150, 100)).toBe(210);
    expect(computeImpactDelay(150, 100, 0.5)).toBe(105);
  });

  it('times player blows mid-sequence and monster blows on their final frame', () => {
    for (const cls of ['warrior', 'mage', 'rogue']) {
      const contact = getAnimConfig(cls).attackContact;
      expect(contact).toBeGreaterThan(0.3);
      expect(contact).toBeLessThan(0.8);
    }
    expect(getAnimConfig('beast').attackContact).toBe(1);
  });
});

describe('sprite sheet grid', () => {
  it('wraps every generated sheet under the conservative GPU texture limit', () => {
    const registry = buildFrameSizeRegistry();
    for (const [key, { frameWidth, frameHeight }] of Object.entries(registry)) {
      const frames = key.startsWith('player_') ? PLAYER_TOTAL_FRAMES : 24;
      const grid = computeSheetGrid(frameWidth * TEXTURE_SCALE, frameHeight * TEXTURE_SCALE, frames);
      expect(grid.width, key).toBeLessThanOrEqual(MAX_SHEET_DIMENSION);
      expect(grid.height, key).toBeLessThanOrEqual(MAX_SHEET_DIMENSION);
      expect(grid.cols * grid.rows, key).toBeGreaterThanOrEqual(frames);
    }
  });

  it('maps frame indices row-major without overlap', () => {
    const grid = computeSheetGrid(300, 100, 30);
    // 13 fit per row → 3 rows, balanced to 10 per row (no empty tail).
    expect(grid.cols).toBe(10);
    expect(grid.rows).toBe(3);
    expect(grid.width).toBe(3000);
    expect(sheetFrameOrigin(grid, 0)).toEqual({ x: 0, y: 0 });
    expect(sheetFrameOrigin(grid, 9)).toEqual({ x: 2700, y: 0 });
    expect(sheetFrameOrigin(grid, 10)).toEqual({ x: 0, y: 100 });
    expect(sheetFrameOrigin(grid, 29)).toEqual({ x: 2700, y: 200 });
  });

  it('keeps short sheets as a single row', () => {
    const grid = computeSheetGrid(100, 50, 4);
    expect(grid).toMatchObject({ cols: 4, rows: 1, width: 400, height: 50 });
  });
});

describe('walk cycle kinematics', () => {
  it('only lifts the foot while it swings forward', () => {
    // Moving forward (cos > 0) → lifted; moving back (cos < 0) → planted.
    expect(walkStride(0).lift).toBe(1);
    expect(walkStride(Math.PI).lift).toBe(0);
    expect(walkStride(Math.PI / 2).swing).toBeCloseTo(1);
  });

  it('rides highest when the legs pass and lowest at full stride', () => {
    expect(walkBob(0)).toBe(1);
    expect(walkBob(Math.PI / 2)).toBeCloseTo(0);
  });
});

describe('frame size registry', () => {
  it('matches every generated character drawer (external art must use the same layout)', async () => {
    const { SpriteGenerator } = await import('../graphics/SpriteGenerator');
    for (const [key, { frameWidth, frameHeight }] of Object.entries(buildFrameSizeRegistry())) {
      const size = SpriteGenerator.getCharacterFrameSize(key);
      expect(size, key).not.toBeNull();
      expect({ w: size!.frameW, h: size!.frameH }, key).toEqual({ w: frameWidth, h: frameHeight });
    }
  }, 20_000); // importing SpriteGenerator pulls in every drawer; slow under a parallel run
});
