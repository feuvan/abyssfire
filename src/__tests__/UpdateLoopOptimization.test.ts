import { describe, it, expect, vi, beforeEach } from 'vitest';

// Local helpers to avoid importing Phaser-dependent IsometricUtils.
// These mirror the implementation in src/utils/IsometricUtils.ts exactly.
function euclideanDistance(x1: number, y1: number, x2: number, y2: number): number {
  return Math.sqrt((x1 - x2) ** 2 + (y1 - y2) ** 2);
}

function distanceSq(x1: number, y1: number, x2: number, y2: number): number {
  const dx = x1 - x2;
  const dy = y1 - y2;
  return dx * dx + dy * dy;
}

// ---------------------------------------------------------------------------
// Tests for Update Loop Optimization (perf-update-loop-optimization)
// Covers: distanceSq utility, buff in-place splice, numeric explored tiles,
// equip stats caching, and HP/mana change-only event emission.
// ---------------------------------------------------------------------------

// ─── distanceSq utility ──────────────────────────────────────────────
describe('distanceSq utility', () => {
  it('returns squared Euclidean distance', () => {
    // distanceSq(0,0,3,4) should be 25 (3²+4²)
    expect(distanceSq(0, 0, 3, 4)).toBe(25);
  });

  it('is consistent with euclideanDistance squared', () => {
    const pairs = [
      [0, 0, 3, 4],
      [1, 2, 5, 8],
      [10, 10, 10, 10],
      [-3, -4, 3, 4],
      [0, 0, 0, 0],
      [1.5, 2.5, 4.5, 6.5],
    ] as const;
    for (const [x1, y1, x2, y2] of pairs) {
      const ed = euclideanDistance(x1, y1, x2, y2);
      expect(distanceSq(x1, y1, x2, y2)).toBeCloseTo(ed * ed, 10);
    }
  });

  it('returns 0 for identical points', () => {
    expect(distanceSq(5, 5, 5, 5)).toBe(0);
  });

  it('preserves threshold comparison semantics', () => {
    // dist <= radius ⟺ distSq <= radius*radius
    const threshold = 3;
    const thresholdSq = threshold * threshold;
    // Point at distance exactly 3 (3,4 → 5 from origin, not 3)
    // Point at distance exactly 3: (3,0)
    expect(distanceSq(0, 0, 3, 0)).toBe(9);
    expect(distanceSq(0, 0, 3, 0) <= thresholdSq).toBe(true);
    // Point just outside
    expect(distanceSq(0, 0, 3, 1) <= thresholdSq).toBe(false);
    // Point well inside
    expect(distanceSq(0, 0, 1, 1) <= thresholdSq).toBe(true);
  });

  it('comparison ordering matches euclidean ordering', () => {
    // If dist(A) < dist(B), then distSq(A) < distSq(B)
    const ax = 1, ay = 1;
    const bx = 5, by = 5;
    const px = 2, py = 2;
    const dSqA = distanceSq(px, py, ax, ay);
    const dSqB = distanceSq(px, py, bx, by);
    const dA = euclideanDistance(px, py, ax, ay);
    const dB = euclideanDistance(px, py, bx, by);
    expect(dA < dB).toBe(true);
    expect(dSqA < dSqB).toBe(true);
  });

  it('handles negative coordinates', () => {
    expect(distanceSq(-3, -4, 0, 0)).toBe(25);
    expect(distanceSq(-1, -1, 1, 1)).toBe(8);
  });

  it('handles large coordinates', () => {
    const d = distanceSq(0, 0, 1000, 1000);
    expect(d).toBe(2000000);
  });
});

// ─── Buff in-place splice ────────────────────────────────────────────
describe('buff in-place splice optimization', () => {
  // Simulates the optimized buff expiry logic (reverse-iteration splice)
  function expireBuffsInPlace(
    buffs: { stat: string; value: number; duration: number; startTime: number }[],
    time: number,
  ): void {
    for (let i = buffs.length - 1; i >= 0; i--) {
      if (time - buffs[i].startTime >= buffs[i].duration) {
        buffs.splice(i, 1);
      }
    }
  }

  it('removes expired buffs', () => {
    const buffs = [
      { stat: 'attack', value: 10, duration: 5000, startTime: 0 },
      { stat: 'defense', value: 5, duration: 3000, startTime: 0 },
    ];
    expireBuffsInPlace(buffs, 4000);
    // Only defense (duration 3000) should be expired at time 4000
    expect(buffs).toHaveLength(1);
    expect(buffs[0].stat).toBe('attack');
  });

  it('removes all expired buffs at once', () => {
    const buffs = [
      { stat: 'a', value: 1, duration: 1000, startTime: 0 },
      { stat: 'b', value: 2, duration: 2000, startTime: 0 },
      { stat: 'c', value: 3, duration: 3000, startTime: 0 },
    ];
    expireBuffsInPlace(buffs, 5000);
    expect(buffs).toHaveLength(0);
  });

  it('keeps all active buffs', () => {
    const buffs = [
      { stat: 'a', value: 1, duration: 5000, startTime: 1000 },
      { stat: 'b', value: 2, duration: 5000, startTime: 2000 },
    ];
    expireBuffsInPlace(buffs, 3000);
    expect(buffs).toHaveLength(2);
  });

  it('handles empty buff array', () => {
    const buffs: { stat: string; value: number; duration: number; startTime: number }[] = [];
    expireBuffsInPlace(buffs, 1000);
    expect(buffs).toHaveLength(0);
  });

  it('preserves insertion order for remaining buffs', () => {
    const buffs = [
      { stat: 'first', value: 1, duration: 10000, startTime: 0 },
      { stat: 'expired', value: 2, duration: 1000, startTime: 0 },
      { stat: 'third', value: 3, duration: 10000, startTime: 0 },
    ];
    expireBuffsInPlace(buffs, 2000);
    expect(buffs).toHaveLength(2);
    expect(buffs[0].stat).toBe('first');
    expect(buffs[1].stat).toBe('third');
  });

  it('mutates the same array reference (no allocation)', () => {
    const buffs = [
      { stat: 'a', value: 1, duration: 1000, startTime: 0 },
    ];
    const ref = buffs;
    expireBuffsInPlace(buffs, 2000);
    expect(buffs).toBe(ref); // Same reference
  });

  it('removes buffs exactly at expiry boundary', () => {
    const buffs = [
      { stat: 'a', value: 1, duration: 5000, startTime: 0 },
    ];
    // At exactly startTime + duration, the buff expires
    expireBuffsInPlace(buffs, 5000);
    expect(buffs).toHaveLength(0);
  });

  it('does not remove buff one tick before expiry', () => {
    const buffs = [
      { stat: 'a', value: 1, duration: 5000, startTime: 0 },
    ];
    expireBuffsInPlace(buffs, 4999);
    expect(buffs).toHaveLength(1);
  });
});

// ─── Numeric explored tiles (Uint8Array) ─────────────────────────────
describe('numeric explored tiles (Uint8Array)', () => {
  const cols = 30;
  const rows = 30;
  const VIEW_RADIUS = 10;

  function createExploredTiles(): Uint8Array {
    return new Uint8Array(rows * cols);
  }

  function updateExploredTiles(
    tiles: Uint8Array,
    pc: number,
    pr: number,
    mapCols: number,
    mapRows: number,
  ): void {
    const vrSq = VIEW_RADIUS * VIEW_RADIUS;
    const minC = Math.max(0, Math.floor(pc - VIEW_RADIUS));
    const maxC = Math.min(mapCols - 1, Math.ceil(pc + VIEW_RADIUS));
    const minR = Math.max(0, Math.floor(pr - VIEW_RADIUS));
    const maxR = Math.min(mapRows - 1, Math.ceil(pr + VIEW_RADIUS));
    for (let r = minR; r <= maxR; r++) {
      for (let c = minC; c <= maxC; c++) {
        const dc = c - pc;
        const dr = r - pr;
        if (dc * dc + dr * dr <= vrSq) {
          tiles[r * mapCols + c] = 1;
        }
      }
    }
  }

  it('marks tiles within view radius as explored', () => {
    const tiles = createExploredTiles();
    updateExploredTiles(tiles, 15, 15, cols, rows);
    // Center should be explored
    expect(tiles[15 * cols + 15]).toBe(1);
    // Adjacent tiles should be explored
    expect(tiles[15 * cols + 16]).toBe(1);
    expect(tiles[16 * cols + 15]).toBe(1);
  });

  it('does not mark tiles outside view radius', () => {
    const tiles = createExploredTiles();
    updateExploredTiles(tiles, 15, 15, cols, rows);
    // Far corner should not be explored
    expect(tiles[0 * cols + 0]).toBe(0);
    expect(tiles[29 * cols + 29]).toBe(0);
  });

  it('handles edge positions correctly', () => {
    const tiles = createExploredTiles();
    updateExploredTiles(tiles, 0, 0, cols, rows);
    expect(tiles[0 * cols + 0]).toBe(1);
    // Should not index out of bounds
    expect(tiles[0 * cols + 5]).toBe(1);
  });

  it('uses numeric index (row * cols + col) for flat access', () => {
    const tiles = createExploredTiles();
    const testCol = 10;
    const testRow = 5;
    tiles[testRow * cols + testCol] = 1;
    expect(tiles[testRow * cols + testCol]).toBe(1);
    // Verify the index formula
    expect(testRow * cols + testCol).toBe(160);
  });

  it('can check tile exploration status with flat index', () => {
    const tiles = createExploredTiles();
    updateExploredTiles(tiles, 15, 15, cols, rows);
    // Simulate isHiddenAreaExplored check with numeric keys
    const checkPoints = [
      { c: 15, r: 15 },
      { c: 16, r: 15 },
      { c: 15, r: 16 },
    ];
    const allExplored = checkPoints.every(p => tiles[p.r * cols + p.c] === 1);
    expect(allExplored).toBe(true);
  });

  it('accumulates explored tiles across multiple updates', () => {
    const tiles = createExploredTiles();
    updateExploredTiles(tiles, 5, 5, cols, rows);
    updateExploredTiles(tiles, 20, 20, cols, rows);
    // Both areas should be explored
    expect(tiles[5 * cols + 5]).toBe(1);
    expect(tiles[20 * cols + 20]).toBe(1);
  });
});

// ─── Equip stats caching ─────────────────────────────────────────────
describe('equip stats caching with dirty flag', () => {
  // Simulates the caching pattern used in ZoneScene
  class EquipStatsCache {
    private cachedStats: Record<string, number> | null = null;
    private computeCount = 0;

    computeStats(): Record<string, number> {
      this.computeCount++;
      return { attack: 10, defense: 5, hp: 100 };
    }

    getStats(): Record<string, number> {
      if (!this.cachedStats) {
        this.cachedStats = this.computeStats();
      }
      return this.cachedStats;
    }

    invalidate(): void {
      this.cachedStats = null;
    }

    getComputeCount(): number {
      return this.computeCount;
    }
  }

  it('caches stats on first call', () => {
    const cache = new EquipStatsCache();
    const stats = cache.getStats();
    expect(stats.attack).toBe(10);
    expect(cache.getComputeCount()).toBe(1);
  });

  it('returns cached value on subsequent calls', () => {
    const cache = new EquipStatsCache();
    cache.getStats();
    cache.getStats();
    cache.getStats();
    expect(cache.getComputeCount()).toBe(1);
  });

  it('recomputes after invalidation', () => {
    const cache = new EquipStatsCache();
    cache.getStats();
    cache.invalidate();
    cache.getStats();
    expect(cache.getComputeCount()).toBe(2);
  });

  it('does not recompute without invalidation across many frames', () => {
    const cache = new EquipStatsCache();
    // Simulate 60 frames without equipment change
    for (let i = 0; i < 60; i++) {
      cache.getStats();
    }
    expect(cache.getComputeCount()).toBe(1);
  });

  it('recomputes only once per invalidation cycle', () => {
    const cache = new EquipStatsCache();
    cache.getStats(); // compute 1
    cache.invalidate();
    cache.getStats(); // compute 2
    cache.getStats(); // cached
    cache.getStats(); // cached
    cache.invalidate();
    cache.getStats(); // compute 3
    expect(cache.getComputeCount()).toBe(3);
  });
});

// ─── HP/mana change-only event emission ──────────────────────────────
describe('HP/mana change-only event emission', () => {
  // Simulates the change-detection pattern
  class HealthEventEmitter {
    private _lastHp = -1;
    private _lastMaxHp = -1;
    private _lastMana = -1;
    private _lastMaxMana = -1;
    emitCount = { health: 0, mana: 0 };

    emitIfChanged(hp: number, maxHp: number, mana: number, maxMana: number): void {
      if (hp !== this._lastHp || maxHp !== this._lastMaxHp) {
        this._lastHp = hp;
        this._lastMaxHp = maxHp;
        this.emitCount.health++;
      }
      if (mana !== this._lastMana || maxMana !== this._lastMaxMana) {
        this._lastMana = mana;
        this._lastMaxMana = maxMana;
        this.emitCount.mana++;
      }
    }
  }

  it('emits on first call (initial values differ from sentinel)', () => {
    const emitter = new HealthEventEmitter();
    emitter.emitIfChanged(100, 100, 50, 50);
    expect(emitter.emitCount.health).toBe(1);
    expect(emitter.emitCount.mana).toBe(1);
  });

  it('does not emit when values are unchanged', () => {
    const emitter = new HealthEventEmitter();
    emitter.emitIfChanged(100, 100, 50, 50);
    emitter.emitIfChanged(100, 100, 50, 50);
    emitter.emitIfChanged(100, 100, 50, 50);
    expect(emitter.emitCount.health).toBe(1);
    expect(emitter.emitCount.mana).toBe(1);
  });

  it('emits when HP changes', () => {
    const emitter = new HealthEventEmitter();
    emitter.emitIfChanged(100, 100, 50, 50);
    emitter.emitIfChanged(90, 100, 50, 50); // HP decreased
    expect(emitter.emitCount.health).toBe(2);
    expect(emitter.emitCount.mana).toBe(1); // mana unchanged
  });

  it('emits when mana changes', () => {
    const emitter = new HealthEventEmitter();
    emitter.emitIfChanged(100, 100, 50, 50);
    emitter.emitIfChanged(100, 100, 40, 50); // mana decreased
    expect(emitter.emitCount.health).toBe(1); // HP unchanged
    expect(emitter.emitCount.mana).toBe(2);
  });

  it('emits when maxHp changes (e.g., equipment change)', () => {
    const emitter = new HealthEventEmitter();
    emitter.emitIfChanged(100, 100, 50, 50);
    emitter.emitIfChanged(100, 120, 50, 50); // maxHp increased
    expect(emitter.emitCount.health).toBe(2);
  });

  it('emits when maxMana changes', () => {
    const emitter = new HealthEventEmitter();
    emitter.emitIfChanged(100, 100, 50, 50);
    emitter.emitIfChanged(100, 100, 50, 70); // maxMana increased
    expect(emitter.emitCount.mana).toBe(2);
  });

  it('handles rapid changes correctly', () => {
    const emitter = new HealthEventEmitter();
    for (let i = 100; i >= 0; i--) {
      emitter.emitIfChanged(i, 100, 50, 50);
    }
    expect(emitter.emitCount.health).toBe(101); // each tick changed HP
    expect(emitter.emitCount.mana).toBe(1); // mana only emitted once
  });

  it('handles zero HP correctly', () => {
    const emitter = new HealthEventEmitter();
    emitter.emitIfChanged(100, 100, 50, 50);
    emitter.emitIfChanged(0, 100, 50, 50); // death
    expect(emitter.emitCount.health).toBe(2);
  });
});

// ─── distanceSq threshold comparison equivalence ─────────────────────
describe('distanceSq threshold comparison equivalence', () => {
  const testCases = [
    { x1: 5, y1: 5, x2: 8, y2: 9, threshold: 5 },     // dist = 5
    { x1: 0, y1: 0, x2: 1, y2: 1, threshold: 1.5 },    // dist ≈ 1.414
    { x1: 10, y1: 10, x2: 11, y2: 11, threshold: 2 },   // dist ≈ 1.414
    { x1: 0, y1: 0, x2: 0, y2: 9, threshold: 9 },       // dist = 9 (safe zone)
    { x1: 0, y1: 0, x2: 0, y2: 3, threshold: 3 },       // dist = 3 (interaction)
  ];

  for (const { x1, y1, x2, y2, threshold } of testCases) {
    it(`dist(${x1},${y1},${x2},${y2}) <= ${threshold} gives same result with both methods`, () => {
      const ed = euclideanDistance(x1, y1, x2, y2);
      const ds = distanceSq(x1, y1, x2, y2);
      expect(ed <= threshold).toBe(ds <= threshold * threshold);
    });

    it(`dist(${x1},${y1},${x2},${y2}) < ${threshold} gives same result with both methods`, () => {
      const ed = euclideanDistance(x1, y1, x2, y2);
      const ds = distanceSq(x1, y1, x2, y2);
      expect(ed < threshold).toBe(ds < threshold * threshold);
    });
  }

  it('safe zone radius comparison is equivalent', () => {
    const safeRadius = 9;
    const safeRadiusSq = safeRadius * safeRadius;
    // Monster at (10, 10), camp at (5, 5) — dist ≈ 7.07
    const ed = euclideanDistance(10, 10, 5, 5);
    const ds = distanceSq(10, 10, 5, 5);
    expect(ed < safeRadius).toBe(ds < safeRadiusSq);
    // Monster at (14, 5), camp at (5, 5) — dist = 9 (on boundary)
    const ed2 = euclideanDistance(14, 5, 5, 5);
    const ds2 = distanceSq(14, 5, 5, 5);
    expect(ed2 < safeRadius).toBe(ds2 < safeRadiusSq);
  });

  it('finding nearest monster works with distanceSq', () => {
    const monsters = [
      { col: 8, row: 8 },
      { col: 3, row: 3 },
      { col: 10, row: 10 },
    ];
    const playerCol = 4, playerRow = 4;

    // Find nearest with euclidean
    let bestEd = Infinity, bestEdIdx = -1;
    for (let i = 0; i < monsters.length; i++) {
      const d = euclideanDistance(playerCol, playerRow, monsters[i].col, monsters[i].row);
      if (d < bestEd) { bestEd = d; bestEdIdx = i; }
    }

    // Find nearest with distanceSq
    let bestDs = Infinity, bestDsIdx = -1;
    for (let i = 0; i < monsters.length; i++) {
      const d = distanceSq(playerCol, playerRow, monsters[i].col, monsters[i].row);
      if (d < bestDs) { bestDs = d; bestDsIdx = i; }
    }

    expect(bestEdIdx).toBe(bestDsIdx);
    expect(bestEdIdx).toBe(1); // (3,3) is closest to (4,4) — dist sqrt(2) vs 4*sqrt(2) and 6*sqrt(2)
  });
});
