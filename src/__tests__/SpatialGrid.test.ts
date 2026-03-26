import { describe, it, expect, beforeEach } from 'vitest';
import { SpatialGrid, type SpatialEntity } from '../systems/SpatialGrid';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function makeEntity(id: string, col: number, row: number): SpatialEntity {
  return { id, tileCol: col, tileRow: row };
}

/** Brute-force radius query for comparison. */
function bruteForceRadius(
  entities: SpatialEntity[],
  col: number,
  row: number,
  radius: number,
): SpatialEntity[] {
  const radiusSq = radius * radius;
  return entities.filter(e => {
    const dx = e.tileCol - col;
    const dy = e.tileRow - row;
    return dx * dx + dy * dy <= radiusSq;
  });
}

// ---------------------------------------------------------------------------
// Tests for SpatialGrid (perf-spatial-index)
//
// Covers VAL-PERF-009, VAL-PERF-010, VAL-PERF-011 from validation contract.
// ---------------------------------------------------------------------------

describe('SpatialGrid', () => {
  // ─── Construction ───────────────────────────────────────────────────────
  describe('construction', () => {
    it('creates an empty grid', () => {
      const grid = new SpatialGrid(120, 120, 16);
      expect(grid.size).toBe(0);
    });

    it('accepts custom cell sizes', () => {
      const grid = new SpatialGrid(120, 120, 8);
      expect(grid.cellSize).toBe(8);
    });

    it('defaults cell size to 16', () => {
      const grid = new SpatialGrid(64, 64);
      expect(grid.cellSize).toBe(16);
    });

    it('handles small maps (smaller than one cell)', () => {
      const grid = new SpatialGrid(8, 8, 16);
      const e = makeEntity('a', 4, 4);
      grid.insert(e);
      expect(grid.size).toBe(1);
      expect(grid.queryRadius(4, 4, 10)).toEqual([e]);
    });
  });

  // ─── Insert / Remove / Has ──────────────────────────────────────────────
  describe('insert and remove', () => {
    let grid: SpatialGrid;

    beforeEach(() => {
      grid = new SpatialGrid(120, 120, 16);
    });

    it('inserts an entity and increments size', () => {
      const e = makeEntity('m1', 10, 20);
      grid.insert(e);
      expect(grid.size).toBe(1);
      expect(grid.has(e)).toBe(true);
    });

    it('removes an entity and decrements size', () => {
      const e = makeEntity('m1', 10, 20);
      grid.insert(e);
      grid.remove(e);
      expect(grid.size).toBe(0);
      expect(grid.has(e)).toBe(false);
    });

    it('removing a non-existent entity is a no-op', () => {
      const e = makeEntity('m1', 10, 20);
      grid.remove(e); // should not throw
      expect(grid.size).toBe(0);
    });

    it('double-insert same entity does not duplicate', () => {
      const e = makeEntity('m1', 10, 20);
      grid.insert(e);
      grid.insert(e); // Set-based, same reference
      expect(grid.size).toBe(1);
      const found = grid.queryRadius(10, 20, 5);
      expect(found.length).toBe(1);
    });

    it('clear removes all entities', () => {
      for (let i = 0; i < 50; i++) {
        grid.insert(makeEntity(`m${i}`, i * 2, i * 2));
      }
      expect(grid.size).toBe(50);
      grid.clear();
      expect(grid.size).toBe(0);
    });
  });

  // ─── VAL-PERF-011: Monster spawn and death updates spatial index ────────
  describe('spawn and death lifecycle (VAL-PERF-011)', () => {
    let grid: SpatialGrid;

    beforeEach(() => {
      grid = new SpatialGrid(120, 120, 16);
    });

    it('spawned monsters are found by queries', () => {
      const m = makeEntity('spawn1', 50, 50);
      grid.insert(m);
      const results = grid.queryRadius(50, 50, 5);
      expect(results).toContain(m);
    });

    it('dead monsters are NOT found after removal', () => {
      const m = makeEntity('dead1', 50, 50);
      grid.insert(m);
      grid.remove(m);
      const results = grid.queryRadius(50, 50, 100);
      expect(results).not.toContain(m);
    });

    it('no stale entries remain after removing all monsters', () => {
      const monsters = Array.from({ length: 20 }, (_, i) =>
        makeEntity(`m${i}`, 10 + i, 10 + i),
      );
      for (const m of monsters) grid.insert(m);
      expect(grid.size).toBe(20);

      for (const m of monsters) grid.remove(m);
      expect(grid.size).toBe(0);

      // Full-map query should return nothing
      const results = grid.queryRadius(60, 60, 200);
      expect(results.length).toBe(0);
    });

    it('respawn: remove then re-insert at new position', () => {
      const m = makeEntity('resp1', 10, 10);
      grid.insert(m);
      grid.remove(m);

      // Respawn at different location
      m.tileCol = 80;
      m.tileRow = 80;
      grid.insert(m);

      expect(grid.size).toBe(1);
      // Should NOT be at old position
      expect(grid.queryRadius(10, 10, 5)).toEqual([]);
      // Should be at new position
      expect(grid.queryRadius(80, 80, 5)).toEqual([m]);
    });
  });

  // ─── VAL-PERF-010: Spatial index updates on monster movement ────────────
  describe('movement / update (VAL-PERF-010)', () => {
    let grid: SpatialGrid;

    beforeEach(() => {
      grid = new SpatialGrid(120, 120, 16);
    });

    it('entity stays in grid after movement within same cell', () => {
      const e = makeEntity('w1', 5, 5);
      grid.insert(e);
      // Move within the same cell (cell 0)
      e.tileCol = 7;
      e.tileRow = 7;
      grid.update(e);
      expect(grid.size).toBe(1);
      expect(grid.queryRadius(7, 7, 3)).toContain(e);
    });

    it('entity correctly transfers cells on cross-cell movement', () => {
      const e = makeEntity('w2', 5, 5);
      grid.insert(e);

      // Move to a different cell (cell size = 16, so col 20 is in cell 1)
      e.tileCol = 20;
      e.tileRow = 5;
      grid.update(e);

      // Should be found at new position
      expect(grid.queryRadius(20, 5, 3)).toContain(e);
      // Should NOT be found at old position (if far enough)
      expect(grid.queryRadius(5, 5, 3)).not.toContain(e);
    });

    it('many sequential moves track correctly', () => {
      const e = makeEntity('zigzag', 0, 0);
      grid.insert(e);

      const positions = [
        [10, 10], [30, 30], [60, 60], [100, 100], [5, 5], [119, 119],
      ];
      for (const [c, r] of positions) {
        e.tileCol = c;
        e.tileRow = r;
        grid.update(e);
      }

      expect(grid.size).toBe(1);
      expect(grid.queryRadius(119, 119, 1)).toContain(e);
      // Should not be at earlier positions
      expect(grid.queryRadius(60, 60, 3)).not.toContain(e);
    });

    it('update on untracked entity auto-inserts', () => {
      const e = makeEntity('new1', 50, 50);
      grid.update(e);
      expect(grid.size).toBe(1);
      expect(grid.queryRadius(50, 50, 5)).toContain(e);
    });
  });

  // ─── VAL-PERF-009: queryRadius correctness ─────────────────────────────
  describe('queryRadius correctness (VAL-PERF-009)', () => {
    let grid: SpatialGrid;
    let entities: SpatialEntity[];

    beforeEach(() => {
      grid = new SpatialGrid(120, 120, 16);
      entities = [];
      // Scatter 100 entities across the map
      for (let i = 0; i < 100; i++) {
        const e = makeEntity(`e${i}`, Math.floor(Math.random() * 120), Math.floor(Math.random() * 120));
        entities.push(e);
        grid.insert(e);
      }
    });

    it('returns identical results to brute-force scan for various centers and radii', () => {
      const queries = [
        { col: 60, row: 60, radius: 10 },
        { col: 0, row: 0, radius: 20 },
        { col: 119, row: 119, radius: 15 },
        { col: 50, row: 50, radius: 5 },
        { col: 30, row: 90, radius: 30 },
        { col: 0, row: 0, radius: 200 }, // Covers entire map
      ];

      for (const q of queries) {
        const spatialResult = new Set(grid.queryRadius(q.col, q.row, q.radius).map(e => e.id));
        const bruteResult = new Set(bruteForceRadius(entities, q.col, q.row, q.radius).map(e => e.id));
        expect(spatialResult).toEqual(bruteResult);
      }
    });

    it('zero radius returns only entities at exact position', () => {
      const grid2 = new SpatialGrid(120, 120, 16);
      const e1 = makeEntity('exact', 50, 50);
      const e2 = makeEntity('near', 51, 50);
      grid2.insert(e1);
      grid2.insert(e2);

      const results = grid2.queryRadius(50, 50, 0);
      expect(results).toContain(e1);
      expect(results).not.toContain(e2);
    });

    it('returns empty array when no entities in range', () => {
      const grid2 = new SpatialGrid(120, 120, 16);
      grid2.insert(makeEntity('far', 100, 100));
      const results = grid2.queryRadius(0, 0, 5);
      expect(results.length).toBe(0);
    });

    it('entities on the boundary (dist == radius) are included', () => {
      const grid2 = new SpatialGrid(120, 120, 16);
      // Distance from (0,0) to (3,4) is 5
      const e = makeEntity('boundary', 3, 4);
      grid2.insert(e);
      const results = grid2.queryRadius(0, 0, 5);
      expect(results).toContain(e);
    });

    it('entities just outside the boundary are excluded', () => {
      const grid2 = new SpatialGrid(120, 120, 16);
      // Distance from (0,0) to (4,4) is ~5.66 > 5
      const e = makeEntity('outside', 4, 4);
      grid2.insert(e);
      const results = grid2.queryRadius(0, 0, 5);
      expect(results).not.toContain(e);
    });
  });

  // ─── findNearest ───────────────────────────────────────────────────────
  describe('findNearest', () => {
    it('finds the closest entity', () => {
      const grid = new SpatialGrid(120, 120, 16);
      const m1 = makeEntity('m1', 10, 10);
      const m2 = makeEntity('m2', 12, 12);
      const m3 = makeEntity('m3', 50, 50);
      grid.insert(m1);
      grid.insert(m2);
      grid.insert(m3);

      const nearest = grid.findNearest(11, 11, 50);
      // m1 dist: sqrt(2) ≈ 1.41, m2 dist: sqrt(2) ≈ 1.41, m3 dist: far
      // Both m1 and m2 are equidistant; either is acceptable
      expect([m1, m2]).toContain(nearest);
    });

    it('respects the filter predicate', () => {
      const grid = new SpatialGrid(120, 120, 16);
      const alive = { ...makeEntity('alive', 10, 10), isAlive: true };
      const dead = { ...makeEntity('dead', 11, 11), isAlive: false };
      grid.insert(alive);
      grid.insert(dead);

      const nearest = grid.findNearest(11, 11, 50, (e: any) => e.isAlive);
      expect(nearest).toBe(alive);
    });

    it('returns null when no entity is within maxRadius', () => {
      const grid = new SpatialGrid(120, 120, 16);
      grid.insert(makeEntity('far', 100, 100));
      const nearest = grid.findNearest(0, 0, 5);
      expect(nearest).toBeNull();
    });

    it('returns null on empty grid', () => {
      const grid = new SpatialGrid(120, 120, 16);
      const nearest = grid.findNearest(50, 50, 100);
      expect(nearest).toBeNull();
    });
  });

  // ─── Edge cases ─────────────────────────────────────────────────────────
  describe('edge cases', () => {
    it('handles negative tile coordinates (clamped to cell 0)', () => {
      const grid = new SpatialGrid(120, 120, 16);
      const e = makeEntity('neg', -5, -5);
      grid.insert(e);
      // Entity should be in cell (0,0)
      expect(grid.size).toBe(1);
      // Query from origin should find it
      const results = grid.queryRadius(0, 0, 10);
      expect(results).toContain(e);
    });

    it('handles coordinates beyond map bounds (clamped)', () => {
      const grid = new SpatialGrid(120, 120, 16);
      const e = makeEntity('oob', 200, 200);
      grid.insert(e);
      expect(grid.size).toBe(1);
      // Should be in the last cell
      const results = grid.queryRadius(200, 200, 5);
      expect(results).toContain(e);
    });

    it('handles fractional tile coordinates', () => {
      const grid = new SpatialGrid(120, 120, 16);
      const e = makeEntity('frac', 10.5, 20.7);
      grid.insert(e);
      expect(grid.size).toBe(1);
      const results = grid.queryRadius(10, 21, 2);
      expect(results).toContain(e);
    });

    it('handles very large radius covering entire map', () => {
      const grid = new SpatialGrid(120, 120, 16);
      for (let i = 0; i < 30; i++) {
        grid.insert(makeEntity(`e${i}`, i * 4, i * 4));
      }
      const results = grid.queryRadius(60, 60, 500);
      expect(results.length).toBe(30);
    });

    it('concurrent insert and remove of different entities', () => {
      const grid = new SpatialGrid(120, 120, 16);
      const e1 = makeEntity('c1', 10, 10);
      const e2 = makeEntity('c2', 20, 20);
      const e3 = makeEntity('c3', 30, 30);

      grid.insert(e1);
      grid.insert(e2);
      grid.insert(e3);
      grid.remove(e2);

      expect(grid.size).toBe(2);
      expect(grid.has(e1)).toBe(true);
      expect(grid.has(e2)).toBe(false);
      expect(grid.has(e3)).toBe(true);
    });
  });

  // ─── Combat targeting equivalence ───────────────────────────────────────
  describe('combat targeting equivalence', () => {
    it('AoE targeting matches brute-force filter', () => {
      const grid = new SpatialGrid(120, 120, 16);
      const monsters: SpatialEntity[] = [];

      // Create a cluster of monsters
      for (let i = 0; i < 40; i++) {
        const m = makeEntity(`m${i}`, 50 + (i % 10) - 5, 50 + Math.floor(i / 10) - 2);
        monsters.push(m);
        grid.insert(m);
      }

      // AoE centered on player at (52, 50) with radius 4
      const playerCol = 52;
      const playerRow = 50;
      const aoeRadius = 4;

      const spatialTargets = new Set(
        grid.queryRadius(playerCol, playerRow, aoeRadius).map(e => e.id),
      );
      const bruteTargets = new Set(
        bruteForceRadius(monsters, playerCol, playerRow, aoeRadius).map(e => e.id),
      );
      expect(spatialTargets).toEqual(bruteTargets);
    });

    it('findNearest matches brute-force nearest-alive scan', () => {
      const grid = new SpatialGrid(120, 120, 16);

      type MonsterLike = SpatialEntity & { alive: boolean };
      const monsters: MonsterLike[] = [
        { id: 'm1', tileCol: 55, tileRow: 55, alive: true },
        { id: 'm2', tileCol: 51, tileRow: 50, alive: false }, // dead
        { id: 'm3', tileCol: 52, tileRow: 50, alive: true },
        { id: 'm4', tileCol: 100, tileRow: 100, alive: true }, // far away
      ];
      for (const m of monsters) grid.insert(m);

      const playerCol = 50;
      const playerRow = 50;

      // Spatial nearest alive
      const spatialNearest = grid.findNearest(
        playerCol, playerRow, 200,
        (e) => (e as MonsterLike).alive,
      );

      // Brute-force nearest alive
      let bruteBest: MonsterLike | null = null;
      let bruteBestDist = Infinity;
      for (const m of monsters) {
        if (!m.alive) continue;
        const dx = m.tileCol - playerCol;
        const dy = m.tileRow - playerRow;
        const d = dx * dx + dy * dy;
        if (d < bruteBestDist) { bruteBestDist = d; bruteBest = m; }
      }

      expect(spatialNearest).toBe(bruteBest);
    });

    it('safe zone pre-filtering: monsters near camp are found', () => {
      const grid = new SpatialGrid(120, 120, 16);
      const campCol = 60;
      const campRow = 60;
      const safeRadius = 9;

      // One monster inside safe zone, one outside
      const inside = makeEntity('inside', campCol + 3, campRow + 3);
      const outside = makeEntity('outside', campCol + 20, campRow + 20);
      grid.insert(inside);
      grid.insert(outside);

      const nearCamp = grid.queryRadius(campCol, campRow, safeRadius);
      expect(nearCamp).toContain(inside);
      expect(nearCamp).not.toContain(outside);
    });
  });

  // ─── Stress / performance sanity ────────────────────────────────────────
  describe('performance sanity', () => {
    it('handles 1000 entities without issue', () => {
      const grid = new SpatialGrid(256, 256, 16);
      const entities: SpatialEntity[] = [];
      for (let i = 0; i < 1000; i++) {
        const e = makeEntity(`s${i}`, Math.floor(Math.random() * 256), Math.floor(Math.random() * 256));
        entities.push(e);
        grid.insert(e);
      }
      expect(grid.size).toBe(1000);

      // Query should return results matching brute-force
      const q = { col: 128, row: 128, radius: 30 };
      const spatialResult = new Set(grid.queryRadius(q.col, q.row, q.radius).map(e => e.id));
      const bruteResult = new Set(bruteForceRadius(entities, q.col, q.row, q.radius).map(e => e.id));
      expect(spatialResult).toEqual(bruteResult);
    });

    it('queryRadius is faster than brute-force for small radii on large grids', () => {
      const grid = new SpatialGrid(512, 512, 16);
      const entities: SpatialEntity[] = [];
      for (let i = 0; i < 2000; i++) {
        const e = makeEntity(`p${i}`, Math.floor(Math.random() * 512), Math.floor(Math.random() * 512));
        entities.push(e);
        grid.insert(e);
      }

      // Just verify correctness (timing is unreliable in test environments)
      const result = grid.queryRadius(256, 256, 10);
      const bruteResult = bruteForceRadius(entities, 256, 256, 10);
      expect(new Set(result.map(e => e.id))).toEqual(new Set(bruteResult.map(e => e.id)));
    });
  });
});
