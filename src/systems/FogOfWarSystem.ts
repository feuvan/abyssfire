import Phaser from 'phaser';
import { TILE_WIDTH, TILE_HEIGHT } from '../config';
import { cartToIso } from '../utils/IsometricUtils';
import { FogOfWarCore } from './FogOfWarCore';

// Re-export FogOfWarCore for consumers that need the pure-logic class
export { FogOfWarCore } from './FogOfWarCore';

// ── Pre-rendered fog tile textures ──────────────────────────────────────
// We quantise alpha into discrete steps and pre-render an isometric diamond
// tile at each level during BootScene (via `generateFogTileTextures`).
// The renderer then stamps these textures via Image objects instead of
// calling Graphics.fillPoints() per tile, eliminating repeated Graphics
// overhead.

/**
 * Number of discrete alpha steps for pre-rendered fog tiles.
 * Higher = smoother gradients but more textures.  16 gives sub-6%
 * alpha steps which is imperceptible.
 */
export const FOG_ALPHA_STEPS = 16;

/** Texture key prefix for pre-rendered fog tiles. */
export const FOG_TILE_KEY_PREFIX = '_fog_tile_';

/**
 * Generate pre-rendered fog tile textures at discrete alpha levels.
 * Call once during BootScene.create().  Creates FOG_ALPHA_STEPS textures
 * named `_fog_tile_0` … `_fog_tile_15` where index 0 is fully transparent
 * (not drawn) and index 15 is alpha ≈ 0.85 (unexplored).
 */
export function generateFogTileTextures(scene: Phaser.Scene): void {
  const hw = TILE_WIDTH / 2;
  const hh = TILE_HEIGHT / 2;
  const texW = TILE_WIDTH + 2; // +2 for anti-alias margin
  const texH = TILE_HEIGHT + 2;
  const cx = texW / 2;
  const cy = texH / 2;

  for (let step = 1; step <= FOG_ALPHA_STEPS; step++) {
    const key = `${FOG_TILE_KEY_PREFIX}${step}`;
    if (scene.textures.exists(key)) continue;

    const alpha = (step / FOG_ALPHA_STEPS) * 0.85;
    const g = scene.add.graphics();
    g.fillStyle(0x000000, alpha);
    g.fillPoints([
      new Phaser.Geom.Point(cx, cy - hh),
      new Phaser.Geom.Point(cx + hw, cy),
      new Phaser.Geom.Point(cx, cy + hh),
      new Phaser.Geom.Point(cx - hw, cy),
    ], true);
    g.generateTexture(key, texW, texH);
    g.destroy();
  }
}

/**
 * Map a floating-point alpha (0..0.85) to the nearest pre-rendered
 * fog tile step index (0..FOG_ALPHA_STEPS).  0 means "no fog drawn".
 */
export function alphaToFogStep(alpha: number): number {
  if (alpha < 0.01) return 0;
  const step = Math.round((alpha / 0.85) * FOG_ALPHA_STEPS);
  return Math.min(FOG_ALPHA_STEPS, Math.max(1, step));
}

/**
 * Phaser-integrated fog of war renderer.
 *
 * Uses `FogOfWarCore` for the logic and pre-rendered fog tile textures
 * (generated via `generateFogTileTextures` in BootScene) for rendering.
 * Individual Image objects are recycled per tile — only dirty tiles are
 * updated.  This eliminates the per-frame Graphics.clear() + fillPoints()
 * overhead of the previous implementation.
 */
export class FogOfWarSystem {
  private scene: Phaser.Scene;
  readonly core: FogOfWarCore;
  /** Tracks whether a full render pass has been done yet. */
  private hasRendered = false;
  /** Per-tile fog Image objects (flat: row * cols + col). null if no fog sprite exists. */
  private fogSprites: (Phaser.GameObjects.Image | null)[];
  /** Per-tile last-rendered fog step, for diffing. */
  private renderedStep: Uint8Array;

  constructor(scene: Phaser.Scene, cols: number, rows: number, viewRadius = 10) {
    this.scene = scene;
    this.core = new FogOfWarCore(cols, rows, viewRadius);
    this.fogSprites = new Array(rows * cols).fill(null);
    this.renderedStep = new Uint8Array(rows * cols);
  }

  /** Convenience getters delegating to core */
  get cols(): number { return this.core.cols; }
  get rows(): number { return this.core.rows; }
  get viewRadius(): number { return this.core.viewRadius; }

  update(playerCol: number, playerRow: number): void {
    const changed = this.core.update(playerCol, playerRow);
    if (!changed) return;

    if (!this.hasRendered) {
      this.renderFull();
      this.hasRendered = true;
    } else {
      this.renderDirty();
    }
  }

  /**
   * Full render pass — stamps fog Images for all viewport-visible tiles.
   * Used only on the very first render.
   */
  private renderFull(): void {
    const cam = this.scene.cameras.main;
    const camCX = cam.scrollX + cam.width / 2 / cam.zoom;
    const camCY = cam.scrollY + cam.height / 2 / cam.zoom;
    const viewW = cam.width / cam.zoom / 2;
    const viewH = cam.height / cam.zoom / 2;
    const margin = 4;

    const cols = this.core.cols;
    const rows = this.core.rows;

    for (let r = 0; r < rows; r++) {
      for (let c = 0; c < cols; c++) {
        const pos = cartToIso(c, r);
        const dx = Math.abs(pos.x - camCX);
        const dy = Math.abs(pos.y - camCY);

        // Viewport culling
        if (dx > viewW + TILE_WIDTH * margin || dy > viewH + TILE_HEIGHT * margin) continue;

        this.stampFogTile(c, r, pos);
      }
    }
  }

  /**
   * Incremental render — only updates tiles whose visibility
   * state changed (dirty tiles from FogOfWarCore).
   */
  private renderDirty(): void {
    const dirty = this.core.dirty;
    if (dirty.size === 0) return;

    const cam = this.scene.cameras.main;
    const camCX = cam.scrollX + cam.width / 2 / cam.zoom;
    const camCY = cam.scrollY + cam.height / 2 / cam.zoom;
    const viewW = cam.width / cam.zoom / 2;
    const viewH = cam.height / cam.zoom / 2;
    const margin = 4;

    const cols = this.core.cols;

    for (const idx of dirty) {
      const c = idx % cols;
      const r = (idx - c) / cols;
      const pos = cartToIso(c, r);
      const dx = Math.abs(pos.x - camCX);
      const dy = Math.abs(pos.y - camCY);

      // Viewport culling — skip tiles outside camera bounds
      if (dx > viewW + TILE_WIDTH * margin || dy > viewH + TILE_HEIGHT * margin) continue;

      this.stampFogTile(c, r, pos);
    }
  }

  /**
   * Stamp or update the fog Image for a single tile.
   * Uses pre-rendered fog tile textures keyed by alpha step.
   */
  private stampFogTile(c: number, r: number, pos: { x: number; y: number }): void {
    const alpha = this.core.getAlpha(c, r);
    const step = alphaToFogStep(alpha);
    const idx = r * this.core.cols + c;

    // If the step hasn't changed, skip
    if (this.renderedStep[idx] === step && this.fogSprites[idx]) return;

    if (step === 0) {
      // No fog — destroy existing sprite if any
      if (this.fogSprites[idx]) {
        this.fogSprites[idx]!.destroy();
        this.fogSprites[idx] = null;
      }
      this.renderedStep[idx] = 0;
      return;
    }

    const texKey = `${FOG_TILE_KEY_PREFIX}${step}`;
    const existing = this.fogSprites[idx];

    if (existing) {
      // Reuse existing sprite, just swap texture
      existing.setTexture(texKey);
    } else {
      // Create new Image at tile world position
      const img = this.scene.add.image(pos.x, pos.y, texKey);
      img.setDepth(1000);
      this.fogSprites[idx] = img;
    }

    this.renderedStep[idx] = step;
  }

  isExplored(col: number, row: number): boolean {
    return this.core.isExplored(col, row);
  }

  getExploredData(): boolean[][] {
    return this.core.getExploredData();
  }

  loadExploredData(data: boolean[][]): void {
    this.core.loadExploredData(data);
    this.hasRendered = false;
    // Reset rendered state so all tiles re-render on next update
    this.renderedStep.fill(0);
  }

  /** Expose gradient info for testing */
  getGradientInfo(playerCol: number, playerRow: number): { col: number; row: number; alpha: number }[] {
    return this.core.getGradientInfo(playerCol, playerRow);
  }

  /** Clean up all fog sprites. Call on zone cleanup. */
  destroy(): void {
    for (let i = 0; i < this.fogSprites.length; i++) {
      if (this.fogSprites[i]) {
        this.fogSprites[i]!.destroy();
        this.fogSprites[i] = null;
      }
    }
    this.renderedStep.fill(0);
  }
}
