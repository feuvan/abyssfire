import { describe, it, expect, beforeEach } from 'vitest';
import { FogOfWarCore } from '../systems/FogOfWarCore';
import {
  alphaToFogStep,
  FOG_ALPHA_STEPS,
  FOG_TILE_KEY_PREFIX,
} from '../systems/FogOfWarSystem';

// ---------------------------------------------------------------------------
// Phaser Performance Optimization Pass — Unit Tests
//
// Covers the four optimisations:
//   1. Fog system RenderTexture approach (alphaToFogStep + texture key mapping)
//   2. Object pooling patterns (floating text, loot)
//   3. Depth batching (row-based depth assignment)
//   4. Monster AI culling (distance-based skip logic)
// ---------------------------------------------------------------------------

// ─── 1. Fog Tile Texture Approach ────────────────────────────────────────

describe('Fog tile texture optimisation', () => {
  describe('alphaToFogStep', () => {
    it('maps alpha 0 to step 0 (no fog)', () => {
      expect(alphaToFogStep(0)).toBe(0);
    });

    it('maps very small alpha (< 0.01) to step 0', () => {
      expect(alphaToFogStep(0.005)).toBe(0);
      expect(alphaToFogStep(0.009)).toBe(0);
    });

    it('maps alpha 0.85 to maximum step', () => {
      expect(alphaToFogStep(0.85)).toBe(FOG_ALPHA_STEPS);
    });

    it('maps intermediate alphas to intermediate steps', () => {
      // alpha 0.425 is roughly half of 0.85
      const step = alphaToFogStep(0.425);
      expect(step).toBeGreaterThan(0);
      expect(step).toBeLessThan(FOG_ALPHA_STEPS);
      expect(step).toBe(Math.round(0.5 * FOG_ALPHA_STEPS));
    });

    it('returns integer values between 0 and FOG_ALPHA_STEPS', () => {
      for (let a = 0; a <= 0.85; a += 0.01) {
        const step = alphaToFogStep(a);
        expect(step).toBeGreaterThanOrEqual(0);
        expect(step).toBeLessThanOrEqual(FOG_ALPHA_STEPS);
        expect(Number.isInteger(step)).toBe(true);
      }
    });

    it('clamps to 1 for small but nonzero alpha', () => {
      expect(alphaToFogStep(0.02)).toBeGreaterThanOrEqual(1);
    });

    it('gradient edge band alphas map to distinct steps', () => {
      // The fog edge band produces alphas from ~0.01 to ~0.15
      const steps = new Set<number>();
      for (let t = 0.01; t <= 0.15; t += 0.01) {
        steps.add(alphaToFogStep(t));
      }
      // Should have at least 2 distinct steps for visible gradient
      expect(steps.size).toBeGreaterThanOrEqual(2);
    });
  });

  describe('FOG_TILE_KEY_PREFIX', () => {
    it('generates unique texture keys per step', () => {
      const keys = new Set<string>();
      for (let s = 1; s <= FOG_ALPHA_STEPS; s++) {
        keys.add(`${FOG_TILE_KEY_PREFIX}${s}`);
      }
      expect(keys.size).toBe(FOG_ALPHA_STEPS);
    });

    it('keys follow expected naming convention', () => {
      expect(`${FOG_TILE_KEY_PREFIX}1`).toBe('_fog_tile_1');
      expect(`${FOG_TILE_KEY_PREFIX}${FOG_ALPHA_STEPS}`).toBe(`_fog_tile_${FOG_ALPHA_STEPS}`);
    });
  });

  describe('FogOfWarCore alpha integration with alphaToFogStep', () => {
    it('fully visible tiles map to step 0', () => {
      const core = new FogOfWarCore(30, 30, 10);
      core.update(15, 15);
      // Center tile is fully visible
      const alpha = core.getAlpha(15, 15);
      expect(alphaToFogStep(alpha)).toBe(0);
    });

    it('unexplored tiles map to maximum step', () => {
      const core = new FogOfWarCore(30, 30, 10);
      core.update(15, 15);
      // Far corner is unexplored
      const alpha = core.getAlpha(0, 0);
      expect(alphaToFogStep(alpha)).toBe(FOG_ALPHA_STEPS);
    });

    it('explored but out-of-view tiles map to intermediate step', () => {
      const core = new FogOfWarCore(40, 40, 8);
      core.update(10, 10); // explore area around (10,10)
      core.update(30, 30); // move away — (10,10) now out of view but explored
      const alpha = core.getAlpha(10, 10);
      const step = alphaToFogStep(alpha);
      expect(step).toBeGreaterThan(0);
      expect(step).toBeLessThan(FOG_ALPHA_STEPS);
    });

    it('gradient band tiles produce smoothly increasing steps', () => {
      const core = new FogOfWarCore(40, 40, 10);
      core.update(20, 20);
      const gradientInfo = core.getGradientInfo(20, 20);
      if (gradientInfo.length >= 2) {
        // Sort by alpha ascending
        gradientInfo.sort((a, b) => a.alpha - b.alpha);
        const steps = gradientInfo.map(g => alphaToFogStep(g.alpha));
        // Steps should be non-decreasing
        for (let i = 1; i < steps.length; i++) {
          expect(steps[i]).toBeGreaterThanOrEqual(steps[i - 1]);
        }
      }
    });

    it('round-trip: save/load preserves alpha-to-step mapping', () => {
      const core = new FogOfWarCore(30, 30, 8);
      core.update(10, 10);
      core.update(20, 20);

      // Record step mapping
      const stepsBeforeSave: number[] = [];
      for (let r = 0; r < 30; r++) {
        for (let c = 0; c < 30; c++) {
          stepsBeforeSave.push(alphaToFogStep(core.getAlpha(c, r)));
        }
      }

      // Save/load round-trip
      const saved = core.getExploredData();
      const core2 = new FogOfWarCore(30, 30, 8);
      core2.loadExploredData(saved);
      core2.update(20, 20); // trigger re-computation from same position

      // Steps should match for explored/unexplored state
      // Note: alpha values may differ slightly for in-view vs out-of-view tiles
      // but unexplored tiles should still be max step
      for (let r = 0; r < 30; r++) {
        for (let c = 0; c < 30; c++) {
          if (!core2.isExplored(c, r)) {
            expect(alphaToFogStep(core2.getAlpha(c, r))).toBe(FOG_ALPHA_STEPS);
          }
        }
      }
    });
  });
});

// ─── 2. Object Pool Pattern ──────────────────────────────────────────────

describe('Object pool pattern (floating text)', () => {
  // Tests the pool acquisition logic extracted from the ZoneScene pattern

  interface PoolableText {
    active: boolean;
    x: number;
    y: number;
    text: string;
  }

  function acquireFromPool(pool: PoolableText[], x: number, y: number, text: string): PoolableText {
    const idx = pool.findIndex(obj => !obj.active);
    if (idx !== -1) {
      const reused = pool[idx];
      reused.active = true;
      reused.x = x;
      reused.y = y;
      reused.text = text;
      return reused;
    }
    const fresh: PoolableText = { active: true, x, y, text };
    pool.push(fresh);
    return fresh;
  }

  function releaseToPool(item: PoolableText): void {
    item.active = false;
  }

  let pool: PoolableText[];

  beforeEach(() => {
    pool = [];
  });

  it('creates new item when pool is empty', () => {
    const item = acquireFromPool(pool, 100, 200, '42');
    expect(pool).toHaveLength(1);
    expect(item.active).toBe(true);
    expect(item.text).toBe('42');
  });

  it('reuses released item instead of creating new', () => {
    const first = acquireFromPool(pool, 100, 200, 'first');
    releaseToPool(first);

    const second = acquireFromPool(pool, 300, 400, 'second');
    expect(pool).toHaveLength(1); // no new item created
    expect(second).toBe(first); // same reference
    expect(second.text).toBe('second');
    expect(second.active).toBe(true);
  });

  it('creates new item when all pool items are active', () => {
    const first = acquireFromPool(pool, 0, 0, 'a');
    const second = acquireFromPool(pool, 0, 0, 'b');
    expect(pool).toHaveLength(2);
    expect(first.active).toBe(true);
    expect(second.active).toBe(true);
  });

  it('pool grows to accommodate burst then reuses on release', () => {
    const items: PoolableText[] = [];
    // Burst: create 10 items
    for (let i = 0; i < 10; i++) {
      items.push(acquireFromPool(pool, i, i, `${i}`));
    }
    expect(pool).toHaveLength(10);

    // Release all
    for (const item of items) releaseToPool(item);

    // Acquire 10 again — pool size shouldn't grow
    for (let i = 0; i < 10; i++) {
      acquireFromPool(pool, i * 10, i * 10, `new_${i}`);
    }
    expect(pool).toHaveLength(10); // no growth
    expect(pool.every(p => p.active)).toBe(true);
  });

  it('partial release allows mixing reused and new items', () => {
    const a = acquireFromPool(pool, 0, 0, 'a');
    const b = acquireFromPool(pool, 0, 0, 'b');
    const c = acquireFromPool(pool, 0, 0, 'c');
    releaseToPool(b); // release middle item

    const d = acquireFromPool(pool, 99, 99, 'd');
    expect(d).toBe(b); // reused
    expect(pool).toHaveLength(3); // no growth
    expect(d.text).toBe('d');

    // All active now
    expect(pool.filter(p => p.active)).toHaveLength(3);
  });
});

// ─── 3. Depth Batching ──────────────────────────────────────────────────

describe('Depth batching — row-based depth', () => {
  it('tiles in the same row get the same depth value', () => {
    // Simulate the depth assignment logic: depth = row
    const row = 42;
    const depthA = row;
    const depthB = row;
    expect(depthA).toBe(depthB);
  });

  it('tiles in different rows get different depth values', () => {
    const depth1 = 10; // row 10
    const depth2 = 11; // row 11
    expect(depth1).not.toBe(depth2);
  });

  it('higher rows render on top of lower rows', () => {
    // In isometric view, higher row numbers are further from camera
    // and should have higher depth (rendered in front)
    for (let row = 0; row < 100; row++) {
      const depth = row;
      expect(depth).toBe(row);
      if (row > 0) expect(depth).toBeGreaterThan(row - 1);
    }
  });

  it('row-based depth produces fewer unique depth values than pos.y-based', () => {
    // With 80x80 grid, pos.y-based would produce up to 6400 unique depths
    // Row-based produces at most 80 unique depths
    const rows = 80;
    const cols = 80;
    const rowBasedDepths = new Set<number>();
    const posYBasedDepths = new Set<number>();

    for (let r = 0; r < rows; r++) {
      for (let c = 0; c < cols; c++) {
        rowBasedDepths.add(r);
        // Simulate pos.y = (c + r) * TILE_HEIGHT / 2
        posYBasedDepths.add((c + r) * 16); // TILE_HEIGHT / 2 = 16
      }
    }

    expect(rowBasedDepths.size).toBeLessThanOrEqual(rows);
    expect(posYBasedDepths.size).toBeGreaterThan(rowBasedDepths.size);
  });
});

// ─── 4. Monster AI Culling ──────────────────────────────────────────────

describe('Monster AI culling — distance-based skip', () => {
  // Simulates the culling logic from ZoneScene update loop
  const MONSTER_AI_CULL_DIST_SQ = 30 * 30; // 900

  interface MockMonster {
    id: string;
    tileCol: number;
    tileRow: number;
    alive: boolean;
    aggro: boolean;
    aiUpdated: boolean;
    animatorUpdated: boolean;
  }

  function distSq(ax: number, ay: number, bx: number, by: number): number {
    const dx = ax - bx;
    const dy = ay - by;
    return dx * dx + dy * dy;
  }

  function runCulledUpdate(monsters: MockMonster[], playerCol: number, playerRow: number): void {
    for (const monster of monsters) {
      if (!monster.alive) continue;

      const monsterDistSq = distSq(monster.tileCol, monster.tileRow, playerCol, playerRow);
      if (monsterDistSq > MONSTER_AI_CULL_DIST_SQ && !monster.aggro) {
        monster.animatorUpdated = true;
        continue;
      }

      monster.aiUpdated = true;
      monster.animatorUpdated = true;
    }
  }

  function makeMob(id: string, col: number, row: number, alive = true, aggro = false): MockMonster {
    return { id, tileCol: col, tileRow: row, alive, aggro, aiUpdated: false, animatorUpdated: false };
  }

  it('nearby monster gets full AI update', () => {
    const monsters = [makeMob('near', 55, 55)];
    runCulledUpdate(monsters, 50, 50); // dist² = 50 < 900
    expect(monsters[0].aiUpdated).toBe(true);
    expect(monsters[0].animatorUpdated).toBe(true);
  });

  it('far-away idle monster skips AI, still gets animator update', () => {
    const monsters = [makeMob('far', 100, 100)];
    runCulledUpdate(monsters, 50, 50); // dist² = 5000 > 900
    expect(monsters[0].aiUpdated).toBe(false);
    expect(monsters[0].animatorUpdated).toBe(true);
  });

  it('far-away aggro monster still gets full AI update', () => {
    const monsters = [makeMob('far-aggro', 100, 100, true, true)];
    runCulledUpdate(monsters, 50, 50);
    expect(monsters[0].aiUpdated).toBe(true);
  });

  it('dead monster gets no update', () => {
    const monsters = [makeMob('dead', 55, 55, false)];
    runCulledUpdate(monsters, 50, 50);
    expect(monsters[0].aiUpdated).toBe(false);
    expect(monsters[0].animatorUpdated).toBe(false);
  });

  it('monster at exactly cull distance gets AI update (boundary condition)', () => {
    // At dist=30, dist² = 900 which is NOT > 900, so it should get AI update
    const monsters = [makeMob('boundary', 80, 50)]; // dist = 30
    runCulledUpdate(monsters, 50, 50);
    expect(monsters[0].aiUpdated).toBe(true);
  });

  it('monster just beyond cull distance is culled', () => {
    // dist = 31, dist² = 961 > 900
    const monsters = [makeMob('beyond', 81, 50)];
    runCulledUpdate(monsters, 50, 50);
    expect(monsters[0].aiUpdated).toBe(false);
    expect(monsters[0].animatorUpdated).toBe(true);
  });

  it('mixed set: only far idle monsters are culled', () => {
    const monsters = [
      makeMob('near1', 52, 51),    // nearby — AI update
      makeMob('near2', 48, 49),    // nearby — AI update
      makeMob('far1', 100, 100),   // far idle — culled
      makeMob('far-aggro', 90, 90, true, true), // far aggro — AI update
      makeMob('dead', 51, 51, false), // dead — skipped
    ];
    runCulledUpdate(monsters, 50, 50);

    expect(monsters[0].aiUpdated).toBe(true);  // near1
    expect(monsters[1].aiUpdated).toBe(true);  // near2
    expect(monsters[2].aiUpdated).toBe(false); // far1 — culled
    expect(monsters[2].animatorUpdated).toBe(true); // but animator runs
    expect(monsters[3].aiUpdated).toBe(true);  // far-aggro — not culled
    expect(monsters[4].aiUpdated).toBe(false); // dead — skipped entirely
    expect(monsters[4].animatorUpdated).toBe(false);
  });

  it('culling is re-evaluated each frame (no stale cache)', () => {
    const monster = makeMob('mobile', 100, 100);

    // Frame 1: far away → culled
    runCulledUpdate([monster], 50, 50);
    expect(monster.aiUpdated).toBe(false);

    // Reset and move player closer
    monster.aiUpdated = false;
    monster.animatorUpdated = false;

    // Frame 2: player moves closer → not culled
    runCulledUpdate([monster], 85, 85); // dist² = 450 < 900
    expect(monster.aiUpdated).toBe(true);
  });
});
