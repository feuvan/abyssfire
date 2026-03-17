import Phaser from 'phaser';
import { TILE_WIDTH, TILE_HEIGHT } from '../config';
import { cartToIso } from '../utils/IsometricUtils';

export class FogOfWarSystem {
  private scene: Phaser.Scene;
  private fogLayer: Phaser.GameObjects.Graphics;
  private explored: boolean[][];
  private cols: number;
  private rows: number;
  private viewRadius: number;

  constructor(scene: Phaser.Scene, cols: number, rows: number, viewRadius = 10) {
    this.scene = scene;
    this.cols = cols;
    this.rows = rows;
    this.viewRadius = viewRadius;
    this.explored = Array.from({ length: rows }, () => Array(cols).fill(false));
    this.fogLayer = scene.add.graphics();
    this.fogLayer.setDepth(1000);
  }

  update(playerCol: number, playerRow: number): void {
    for (let r = 0; r < this.rows; r++) {
      for (let c = 0; c < this.cols; c++) {
        const dist = Math.sqrt((c - playerCol) ** 2 + (r - playerRow) ** 2);
        if (dist <= this.viewRadius) {
          this.explored[r][c] = true;
        }
      }
    }
    this.render(playerCol, playerRow);
  }

  private render(playerCol: number, playerRow: number): void {
    this.fogLayer.clear();

    // Only render fog for tiles within camera viewport + margin
    const cam = this.scene.cameras.main;
    const camCX = cam.scrollX + cam.width / 2 / cam.zoom;
    const camCY = cam.scrollY + cam.height / 2 / cam.zoom;
    const viewW = cam.width / cam.zoom / 2;
    const viewH = cam.height / cam.zoom / 2;
    const margin = 4;

    // Gradient edge band: tiles within this range of the view radius get partial fog
    const edgeBand = 3;
    const innerEdge = this.viewRadius - edgeBand;

    for (let r = 0; r < this.rows; r++) {
      for (let c = 0; c < this.cols; c++) {
        const pos = cartToIso(c, r);
        const dx = Math.abs(pos.x - camCX);
        const dy = Math.abs(pos.y - camCY);

        if (dx > viewW + TILE_WIDTH * margin || dy > viewH + TILE_HEIGHT * margin) continue;

        const dist = Math.sqrt((c - playerCol) ** 2 + (r - playerRow) ** 2);

        if (dist > this.viewRadius) {
          if (!this.explored[r][c]) {
            // Unexplored: near-opaque
            this.fogLayer.fillStyle(0x000000, 0.85);
            this.drawIsoTile(pos.x, pos.y);
          } else {
            // Explored but out of view: semi-transparent with gradient near edge
            const edgeFade = dist < this.viewRadius + edgeBand
              ? 0.25 + (dist - this.viewRadius) / edgeBand * 0.2
              : 0.45;
            this.fogLayer.fillStyle(0x000000, edgeFade);
            this.drawIsoTile(pos.x, pos.y);
          }
        } else if (dist > innerEdge) {
          // Gradient edge: partial fog fading from 0 to light fog
          const t = (dist - innerEdge) / edgeBand;
          const alpha = t * 0.15;
          if (alpha > 0.01) {
            this.fogLayer.fillStyle(0x000000, alpha);
            this.drawIsoTile(pos.x, pos.y);
          }
        }
      }
    }
  }

  private drawIsoTile(x: number, y: number): void {
    const hw = TILE_WIDTH / 2;
    const hh = TILE_HEIGHT / 2;
    this.fogLayer.fillPoints([
      new Phaser.Geom.Point(x, y - hh),
      new Phaser.Geom.Point(x + hw, y),
      new Phaser.Geom.Point(x, y + hh),
      new Phaser.Geom.Point(x - hw, y),
    ], true);
  }

  isExplored(col: number, row: number): boolean {
    if (col < 0 || col >= this.cols || row < 0 || row >= this.rows) return false;
    return this.explored[row][col];
  }

  getExploredData(): boolean[][] {
    return this.explored.map(row => [...row]);
  }

  loadExploredData(data: boolean[][]): void {
    if (data.length === this.rows && data[0]?.length === this.cols) {
      this.explored = data.map(row => [...row]);
    }
  }
}
