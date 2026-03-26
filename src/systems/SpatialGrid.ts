/**
 * Grid-based spatial index for fast proximity queries.
 *
 * Divides the world into a 2D grid of cells (configurable size, default 16×16
 * tiles).  Entities are tracked by their tile position so that radius queries
 * only visit the cells that overlap the search region, eliminating O(n) full
 * array scans.
 *
 * IMPORTANT: This is internal to ZoneScene — it does NOT modify Monster or
 * Player class interfaces.
 */

/** Minimal entity interface required by the spatial index. */
export interface SpatialEntity {
  id: string;
  tileCol: number;
  tileRow: number;
}

export class SpatialGrid<T extends SpatialEntity = SpatialEntity> {
  /** Number of tile columns per cell. */
  readonly cellSize: number;
  /** Grid width in cells. */
  private readonly gridCols: number;
  /** Grid height in cells. */
  private readonly gridRows: number;
  /** Total map tile columns — for clamping. */
  private readonly mapCols: number;
  /** Total map tile rows — for clamping. */
  private readonly mapRows: number;
  /** Flat array of cells; each cell is a Set<T>. */
  private readonly cells: Set<T>[];
  /** Reverse lookup: entity id → cell index. */
  private readonly entityCell: Map<string, number>;

  /**
   * @param mapCols  Total tile columns in the map.
   * @param mapRows  Total tile rows in the map.
   * @param cellSize Tile size of each cell (default 16).
   */
  constructor(mapCols: number, mapRows: number, cellSize = 16) {
    this.cellSize = cellSize;
    this.mapCols = mapCols;
    this.mapRows = mapRows;
    this.gridCols = Math.ceil(mapCols / cellSize);
    this.gridRows = Math.ceil(mapRows / cellSize);
    this.cells = new Array(this.gridCols * this.gridRows);
    for (let i = 0; i < this.cells.length; i++) {
      this.cells[i] = new Set<T>();
    }
    this.entityCell = new Map();
  }

  // ─── Helpers ──────────────────────────────────────────────────────────────

  /** Convert tile coordinate to cell index (clamped). */
  private cellIndex(col: number, row: number): number {
    const cx = Math.max(0, Math.min(this.gridCols - 1, Math.floor(col / this.cellSize)));
    const cy = Math.max(0, Math.min(this.gridRows - 1, Math.floor(row / this.cellSize)));
    return cy * this.gridCols + cx;
  }

  // ─── Mutation ─────────────────────────────────────────────────────────────

  /** Insert an entity into the grid. */
  insert(entity: T): void {
    const idx = this.cellIndex(entity.tileCol, entity.tileRow);
    this.cells[idx].add(entity);
    this.entityCell.set(entity.id, idx);
  }

  /** Remove an entity from the grid. */
  remove(entity: T): void {
    const idx = this.entityCell.get(entity.id);
    if (idx !== undefined) {
      this.cells[idx].delete(entity);
      this.entityCell.delete(entity.id);
    }
  }

  /**
   * Notify the grid that an entity has moved.
   *
   * Only performs a cell transfer when the entity crossed a cell boundary.
   */
  update(entity: T): void {
    const newIdx = this.cellIndex(entity.tileCol, entity.tileRow);
    const oldIdx = this.entityCell.get(entity.id);
    if (oldIdx === undefined) {
      // Not tracked yet — insert.
      this.cells[newIdx].add(entity);
      this.entityCell.set(entity.id, newIdx);
      return;
    }
    if (oldIdx !== newIdx) {
      this.cells[oldIdx].delete(entity);
      this.cells[newIdx].add(entity);
      this.entityCell.set(entity.id, newIdx);
    }
  }

  /** Remove all entities and reset the grid. */
  clear(): void {
    for (const cell of this.cells) cell.clear();
    this.entityCell.clear();
  }

  // ─── Queries ──────────────────────────────────────────────────────────────

  /**
   * Return all entities whose tile position is within `radius` tiles of
   * (col, row) using **squared-distance** comparison (no sqrt).
   *
   * The result is a plain array; callers can apply further filters (alive,
   * aggro, etc.) as needed.
   */
  queryRadius(col: number, row: number, radius: number): T[] {
    const radiusSq = radius * radius;
    const result: T[] = [];

    // Determine cell range to scan (clamp to valid grid bounds).
    const minCx = Math.max(0, Math.min(this.gridCols - 1, Math.floor((col - radius) / this.cellSize)));
    const maxCx = Math.max(0, Math.min(this.gridCols - 1, Math.floor((col + radius) / this.cellSize)));
    const minCy = Math.max(0, Math.min(this.gridRows - 1, Math.floor((row - radius) / this.cellSize)));
    const maxCy = Math.max(0, Math.min(this.gridRows - 1, Math.floor((row + radius) / this.cellSize)));

    for (let cy = minCy; cy <= maxCy; cy++) {
      for (let cx = minCx; cx <= maxCx; cx++) {
        const cell = this.cells[cy * this.gridCols + cx];
        for (const entity of cell) {
          const dx = entity.tileCol - col;
          const dy = entity.tileRow - row;
          if (dx * dx + dy * dy <= radiusSq) {
            result.push(entity);
          }
        }
      }
    }
    return result;
  }

  /**
   * Find the single nearest entity to (col, row) within `maxRadius` tiles.
   * Returns `null` when no entity is found.
   *
   * An optional `filter` predicate can exclude entities (e.g. dead monsters).
   */
  findNearest(
    col: number,
    row: number,
    maxRadius: number,
    filter?: (entity: T) => boolean,
  ): T | null {
    const maxRadiusSq = maxRadius * maxRadius;
    let best: T | null = null;
    let bestDistSq = Infinity;

    const minCx = Math.max(0, Math.min(this.gridCols - 1, Math.floor((col - maxRadius) / this.cellSize)));
    const maxCx = Math.max(0, Math.min(this.gridCols - 1, Math.floor((col + maxRadius) / this.cellSize)));
    const minCy = Math.max(0, Math.min(this.gridRows - 1, Math.floor((row - maxRadius) / this.cellSize)));
    const maxCy = Math.max(0, Math.min(this.gridRows - 1, Math.floor((row + maxRadius) / this.cellSize)));

    for (let cy = minCy; cy <= maxCy; cy++) {
      for (let cx = minCx; cx <= maxCx; cx++) {
        const cell = this.cells[cy * this.gridCols + cx];
        for (const entity of cell) {
          if (filter && !filter(entity)) continue;
          const dx = entity.tileCol - col;
          const dy = entity.tileRow - row;
          const dSq = dx * dx + dy * dy;
          if (dSq <= maxRadiusSq && dSq < bestDistSq) {
            bestDistSq = dSq;
            best = entity;
          }
        }
      }
    }
    return best;
  }

  // ─── Debug / Testing helpers ──────────────────────────────────────────────

  /** Total number of tracked entities. */
  get size(): number {
    return this.entityCell.size;
  }

  /** Check whether an entity is tracked. */
  has(entity: T): boolean {
    return this.entityCell.has(entity.id);
  }
}
