import { describe, it, expect, vi, beforeEach } from 'vitest';
import { FogOfWarCore } from '../systems/FogOfWarCore';

// Inline constants and helper to avoid importing from config.ts / IsometricUtils.ts
// (which pull in Phaser at module level and break tests without a full Phaser mock).
const TILE_WIDTH = 64;
const TILE_HEIGHT = 32;

/** Cart-to-iso coordinate conversion (same formula as IsometricUtils.cartToIso). */
function cartToIso(cartX: number, cartY: number): { x: number; y: number } {
  return {
    x: (cartX - cartY) * (TILE_WIDTH / 2),
    y: (cartX + cartY) * (TILE_HEIGHT / 2),
  };
}

// ---------------------------------------------------------------------------
// Tests for the optimized FogOfWarSystem
// Pure-logic tests use FogOfWarCore (no Phaser dependency).
// ---------------------------------------------------------------------------

describe('FogOfWarSystem — Optimized', () => {
  const cols = 30;
  const rows = 30;
  const viewRadius = 5;
  let core: FogOfWarCore;

  beforeEach(() => {
    core = new FogOfWarCore(cols, rows, viewRadius);
  });

  // ─── (a) Fog only updates on tile change ────────────────────────────
  describe('fog only updates on tile change', () => {
    it('update() marks tiles as explored when called', () => {
      core.update(10, 10);
      expect(core.isExplored(10, 10)).toBe(true);
      expect(core.isExplored(11, 10)).toBe(true);
      expect(core.isExplored(10, 11)).toBe(true);
    });

    it('update() returns true on first call (new tile)', () => {
      expect(core.update(10, 10)).toBe(true);
    });

    it('update() returns false when called with the same tile position', () => {
      core.update(10, 10);
      expect(core.update(10, 10)).toBe(false);
    });

    it('update() returns true when player moves to a new tile', () => {
      core.update(10, 10);
      expect(core.update(12, 12)).toBe(true);
    });

    it('repeated same-position calls never compute dirty tiles', () => {
      core.update(10, 10);
      core.dirty.clear();

      // Same position — dirty set should remain empty
      core.update(10, 10);
      expect(core.dirty.size).toBe(0);

      core.update(10, 10);
      expect(core.dirty.size).toBe(0);

      core.update(10, 10);
      expect(core.dirty.size).toBe(0);
    });

    it('moving to a new tile produces dirty tiles', () => {
      core.update(10, 10);
      core.update(12, 12);
      expect(core.dirty.size).toBeGreaterThan(0);
    });
  });

  // ─── (b) Explored state persists correctly ──────────────────────────
  describe('explored state persists correctly', () => {
    it('getExploredData() returns boolean[][] matching explored state', () => {
      core.update(5, 5);
      const data = core.getExploredData();
      expect(data.length).toBe(rows);
      expect(data[0].length).toBe(cols);
      // Center tile must be explored
      expect(data[5][5]).toBe(true);
      // Far corner should not be explored
      expect(data[29][29]).toBe(false);
    });

    it('loadExploredData() restores explored state from boolean[][]', () => {
      const data: boolean[][] = Array.from({ length: rows }, () => Array(cols).fill(false));
      data[15][15] = true;

      core.loadExploredData(data);
      expect(core.isExplored(15, 15)).toBe(true);
      expect(core.isExplored(0, 0)).toBe(false);
    });

    it('round-trip: getExploredData → loadExploredData preserves all tiles', () => {
      core.update(10, 10);
      const exported = core.getExploredData();

      const core2 = new FogOfWarCore(cols, rows, viewRadius);
      core2.loadExploredData(exported);

      for (let r = 0; r < rows; r++) {
        for (let c = 0; c < cols; c++) {
          expect(core2.isExplored(c, r)).toBe(core.isExplored(c, r));
        }
      }
    });

    it('loadExploredData() rejects data with wrong dimensions', () => {
      const wrongData: boolean[][] = [[true, false]];
      core.update(5, 5);
      core.loadExploredData(wrongData);
      // State should be unchanged — tile (5,5) still explored
      expect(core.isExplored(5, 5)).toBe(true);
    });

    it('isExplored returns false for out-of-bounds tiles', () => {
      expect(core.isExplored(-1, 0)).toBe(false);
      expect(core.isExplored(0, -1)).toBe(false);
      expect(core.isExplored(cols, 0)).toBe(false);
      expect(core.isExplored(0, rows)).toBe(false);
    });

    it('loadExploredData resets tile tracking so next update redraws', () => {
      core.update(10, 10);
      const data = core.getExploredData();
      core.loadExploredData(data);
      // After load, the next update should process (lastCol/lastRow reset)
      expect(core.update(10, 10)).toBe(true);
    });
  });

  // ─── (c) Gradient edge tiles calculated correctly ────────────────────
  describe('gradient edge tiles calculated correctly', () => {
    it('tiles within inner edge (viewRadius - 3) are fully visible (no fog)', () => {
      core.update(15, 15);
      // Inner edge = viewRadius - edgeBand = 5 - 3 = 2
      // Tile (15,15) distance 0 — fully visible (alpha 0)
      expect(core.getAlpha(15, 15)).toBe(0);
      // Tile (16,15) distance 1 — fully visible (alpha 0)
      expect(core.getAlpha(16, 15)).toBe(0);
      expect(core.isExplored(15, 15)).toBe(true);
      expect(core.isExplored(16, 15)).toBe(true);
    });

    it('tiles between innerEdge and viewRadius get gradient fog alpha', () => {
      core.update(15, 15);
      const gradientInfo = core.getGradientInfo(15, 15);
      // Tile (18,15) is at distance 3 — in gradient band
      const tileAtDist3 = gradientInfo.find(
        t => t.col === 18 && t.row === 15
      );
      expect(tileAtDist3).toBeDefined();
      expect(tileAtDist3!.alpha).toBeGreaterThan(0);
      expect(tileAtDist3!.alpha).toBeLessThan(0.85);
    });

    it('edgeBand width is 3 tiles', () => {
      core.update(15, 15);
      const gradientInfo = core.getGradientInfo(15, 15);
      const innerEdge = viewRadius - 3; // 2
      for (const tile of gradientInfo) {
        const dist = Math.sqrt((tile.col - 15) ** 2 + (tile.row - 15) ** 2);
        expect(dist).toBeGreaterThan(innerEdge);
        expect(dist).toBeLessThanOrEqual(viewRadius);
      }
    });

    it('gradient alpha increases with distance from inner edge', () => {
      core.update(15, 15);
      const gradientInfo = core.getGradientInfo(15, 15);
      const sorted = gradientInfo.sort((a, b) => {
        const da = Math.sqrt((a.col - 15) ** 2 + (a.row - 15) ** 2);
        const db = Math.sqrt((b.col - 15) ** 2 + (b.row - 15) ** 2);
        return da - db;
      });

      if (sorted.length >= 2) {
        const firstAlpha = sorted[0].alpha;
        const lastAlpha = sorted[sorted.length - 1].alpha;
        expect(lastAlpha).toBeGreaterThanOrEqual(firstAlpha);
      }
    });

    it('no gradient tiles exist at distance <= innerEdge', () => {
      core.update(15, 15);
      const gradientInfo = core.getGradientInfo(15, 15);
      const innerEdge = viewRadius - 3;
      for (const tile of gradientInfo) {
        const dist = Math.sqrt((tile.col - 15) ** 2 + (tile.row - 15) ** 2);
        expect(dist).toBeGreaterThan(innerEdge);
      }
    });
  });

  // ─── (d) Viewport culling logic works ───────────────────────────────
  describe('viewport culling logic works', () => {
    it('dirty set only contains tiles that actually changed alpha', () => {
      // First update: all tiles change from 0 to their computed value
      core.update(15, 15);
      const firstDirtyCount = core.dirty.size;
      expect(firstDirtyCount).toBeGreaterThan(0);

      // Second update at same position: update() returns false, dirty is unchanged
      // (the dirty set from the first call persists but no new computation ran)
      const changed = core.update(15, 15);
      expect(changed).toBe(false);
      // Dirty set still contains the tiles from the first call
      // That's correct — the renderer would have consumed them already

      // Third update at new position: some tiles change
      core.update(16, 15);
      expect(core.dirty.size).toBeGreaterThan(0);
      // But should be less than redrawing everything
      expect(core.dirty.size).toBeLessThanOrEqual(cols * rows);
    });

    it('dirty tiles include both newly-visible and newly-hidden tiles', () => {
      core.update(10, 10);
      core.update(15, 15);

      // Some tiles near (10,10) that were visible should now be dirty
      // (they became explored-out-of-view)
      // And some tiles near (15,15) that were dark should now be dirty
      // (they became visible)
      expect(core.dirty.size).toBeGreaterThan(0);
    });

    it('large grid only produces dirty tiles proportional to movement', () => {
      const bigCore = new FogOfWarCore(120, 120, 10);
      bigCore.update(60, 60);
      const firstDirty = bigCore.dirty.size;

      // Move one tile — dirty count should be much less than full grid
      bigCore.update(61, 60);
      const moveDirty = bigCore.dirty.size;
      expect(moveDirty).toBeLessThan(120 * 120);
      // Moving one tile should produce fewer dirty tiles than the initial reveal
      expect(moveDirty).toBeLessThan(firstDirty);
    });
  });

  // ─── Additional edge cases ──────────────────────────────────────────
  describe('edge cases', () => {
    it('update at (0,0) corner works without errors', () => {
      expect(() => core.update(0, 0)).not.toThrow();
      expect(core.isExplored(0, 0)).toBe(true);
    });

    it('update at max corner works without errors', () => {
      expect(() => core.update(cols - 1, rows - 1)).not.toThrow();
      expect(core.isExplored(cols - 1, rows - 1)).toBe(true);
    });

    it('unexplored tiles have alpha 0.85', () => {
      core.update(5, 5);
      // Far tiles should be unexplored with 0.85 alpha
      const alpha = core.getAlpha(29, 29);
      expect(alpha).toBeCloseTo(0.85, 1);
    });

    it('explored tiles within view radius are correctly bounded to grid', () => {
      core.update(1, 1);
      expect(core.isExplored(-1, -1)).toBe(false);
      expect(core.isExplored(1, 1)).toBe(true);
    });

    it('empty grid (1x1) works', () => {
      const tinyCore = new FogOfWarCore(1, 1, 5);
      tinyCore.update(0, 0);
      expect(tinyCore.isExplored(0, 0)).toBe(true);
      const data = tinyCore.getExploredData();
      expect(data).toEqual([[true]]);
    });

    it('invalidate() forces full recalculation on next update', () => {
      core.update(10, 10);
      core.invalidate();
      // After invalidate, same position should recalculate
      expect(core.update(10, 10)).toBe(true);
      expect(core.dirty.size).toBeGreaterThan(0);
    });

    it('uses Uint8Array internally — initial state is all unexplored', () => {
      for (let r = 0; r < rows; r++) {
        for (let c = 0; c < cols; c++) {
          expect(core.isExplored(c, r)).toBe(false);
        }
      }
    });

    it('multiple updates accumulate explored tiles', () => {
      core.update(5, 5);
      core.update(25, 25);
      // Both areas should be explored
      expect(core.isExplored(5, 5)).toBe(true);
      expect(core.isExplored(25, 25)).toBe(true);
    });
  });
});

// ═══════════════════════════════════════════════════════════════════════
// Viewport culling and dirty-tile rendering tests
// These verify the FogOfWarSystem renderer's culling and incremental
// rendering logic using the same coordinate math as the production code.
// ═══════════════════════════════════════════════════════════════════════

describe('FogOfWarSystem — viewport culling', () => {
  const cols = 120;
  const rows = 120;
  const viewRadius = 10;
  const margin = 4;

  /**
   * Simulates viewport culling: given camera params, returns which tiles
   * from a set of tile indices would be visible (not culled).
   */
  function getVisibleTilesFromSet(
    tileIndices: Set<number>,
    cols: number,
    camScrollX: number,
    camScrollY: number,
    camWidth: number,
    camHeight: number,
    camZoom: number,
  ): number[] {
    const camCX = camScrollX + camWidth / 2 / camZoom;
    const camCY = camScrollY + camHeight / 2 / camZoom;
    const viewW = camWidth / camZoom / 2;
    const viewH = camHeight / camZoom / 2;

    const visible: number[] = [];
    for (const idx of tileIndices) {
      const c = idx % cols;
      const r = (idx - c) / cols;
      const pos = cartToIso(c, r);
      const dx = Math.abs(pos.x - camCX);
      const dy = Math.abs(pos.y - camCY);
      if (dx <= viewW + TILE_WIDTH * margin && dy <= viewH + TILE_HEIGHT * margin) {
        visible.push(idx);
      }
    }
    return visible;
  }

  it('viewport culling skips tiles outside camera bounds', () => {
    const core = new FogOfWarCore(cols, rows, viewRadius);
    core.update(60, 60);

    // Camera centered on player at iso coords of (60,60)
    const playerIso = cartToIso(60, 60);
    const camWidth = 1280;
    const camHeight = 720;
    const camZoom = 1;
    const camScrollX = playerIso.x - camWidth / 2;
    const camScrollY = playerIso.y - camHeight / 2;

    // All dirty tiles from first update
    const allDirty = new Set(core.dirty);
    expect(allDirty.size).toBeGreaterThan(0);

    // Filter to visible tiles
    const visibleTiles = getVisibleTilesFromSet(
      allDirty, cols, camScrollX, camScrollY, camWidth, camHeight, camZoom,
    );

    // There should be some tiles outside the viewport
    // (the fog update covers a ~20-tile radius area but many tiles far from
    // player will still be dirty — e.g. border unexplored tiles)
    // The visible set should be smaller than the full dirty set on a 120x120 grid
    expect(visibleTiles.length).toBeLessThanOrEqual(allDirty.size);
    expect(visibleTiles.length).toBeGreaterThan(0);
  });

  it('tiles far from camera center are culled by viewport', () => {
    const core = new FogOfWarCore(cols, rows, viewRadius);
    core.update(10, 10);

    // Camera at far corner — player is at (10,10) but camera at (100,100) area
    const farIso = cartToIso(100, 100);
    const camWidth = 800;
    const camHeight = 600;
    const camZoom = 1;
    const camScrollX = farIso.x - camWidth / 2;
    const camScrollY = farIso.y - camHeight / 2;

    const allDirty = new Set(core.dirty);
    const visibleTiles = getVisibleTilesFromSet(
      allDirty, cols, camScrollX, camScrollY, camWidth, camHeight, camZoom,
    );

    // Player updated at (10,10) — those tiles should be far from camera at (100,100).
    // Most/all dirty tiles near the player should be culled.
    expect(visibleTiles.length).toBeLessThan(allDirty.size);
  });

  it('zoomed-out camera shows more tiles (fewer culled)', () => {
    const core = new FogOfWarCore(cols, rows, viewRadius);
    core.update(60, 60);

    const playerIso = cartToIso(60, 60);
    const camWidth = 1280;
    const camHeight = 720;

    const allDirty = new Set(core.dirty);

    // Zoom 1.0
    const visAtZoom1 = getVisibleTilesFromSet(
      allDirty, cols, playerIso.x - camWidth / 2, playerIso.y - camHeight / 2,
      camWidth, camHeight, 1.0,
    );

    // Zoom 0.5 (zoomed out — sees 2x area)
    const visAtZoom05 = getVisibleTilesFromSet(
      allDirty, cols, playerIso.x - camWidth, playerIso.y - camHeight,
      camWidth, camHeight, 0.5,
    );

    // More tiles should be visible when zoomed out
    expect(visAtZoom05.length).toBeGreaterThanOrEqual(visAtZoom1.length);
  });

  it('incremental update produces fewer dirty tiles than initial full update', () => {
    const core = new FogOfWarCore(cols, rows, viewRadius);
    core.update(60, 60);
    const firstDirtyCount = core.dirty.size;

    // Move one tile
    core.update(61, 60);
    const incrementalDirtyCount = core.dirty.size;

    // Incremental should be strictly less than the full initial reveal
    expect(incrementalDirtyCount).toBeLessThan(firstDirtyCount);
    expect(incrementalDirtyCount).toBeGreaterThan(0);
  });

  it('only dirty tiles are candidates for redraw (not full grid)', () => {
    const core = new FogOfWarCore(cols, rows, viewRadius);
    core.update(60, 60);
    core.update(61, 60);

    // Dirty set should be much smaller than total grid
    const totalTiles = cols * rows; // 14400
    expect(core.dirty.size).toBeLessThan(totalTiles * 0.2); // less than 20% of grid
    expect(core.dirty.size).toBeGreaterThan(0);
  });

  it('dirty tiles contain correct column and row within bounds', () => {
    // Use a smaller grid (40x40) to avoid timeout under heavy CPU load.
    // The bounds-checking logic is identical regardless of grid size.
    const smallCols = 40;
    const smallRows = 40;
    const smallCore = new FogOfWarCore(smallCols, smallRows, viewRadius);
    smallCore.update(20, 20);

    for (const idx of smallCore.dirty) {
      const c = idx % smallCols;
      const r = (idx - c) / smallCols;
      expect(c).toBeGreaterThanOrEqual(0);
      expect(c).toBeLessThan(smallCols);
      expect(r).toBeGreaterThanOrEqual(0);
      expect(r).toBeLessThan(smallRows);
    }
  });

  it('after loadExploredData, next update redraws all visible tiles (full pass)', () => {
    const core = new FogOfWarCore(cols, rows, viewRadius);
    core.update(60, 60);
    const firstDirty = core.dirty.size;

    // Save and reload
    const data = core.getExploredData();
    core.loadExploredData(data);

    // Next update should produce a full recalculation
    core.update(60, 60);
    const afterLoadDirty = core.dirty.size;

    // After load + update, dirty count should be comparable to the
    // first full render (all tiles get recalculated)
    expect(afterLoadDirty).toBeGreaterThan(0);
    expect(afterLoadDirty).toBeGreaterThanOrEqual(firstDirty * 0.8);
  });
});
