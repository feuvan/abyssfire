import type { MapData, MapTheme } from '../data/types';

// Tile constants: 0=grass, 1=dirt, 2=stone, 3=water, 4=wall, 5=camp
const TILE_GRASS = 0;
const TILE_DIRT = 1;
const TILE_STONE = 2;
const TILE_WATER = 3;
const TILE_WALL = 4;
const TILE_CAMP = 5;
const TILE_CAMP_WALL = 6;

/** Simple LCG (Linear Congruential Generator) for seeded randomness */
class SeededRandom {
  private state: number;

  constructor(seed: number) {
    this.state = seed % 2147483647;
    if (this.state <= 0) this.state += 2147483646;
  }

  /** Returns a float in [0, 1) */
  next(): number {
    this.state = (this.state * 16807) % 2147483647;
    return (this.state - 1) / 2147483646;
  }

  /** Returns an integer in [min, max] inclusive */
  nextInt(min: number, max: number): number {
    return Math.floor(this.next() * (max - min + 1)) + min;
  }

  /** Returns a boolean with given probability */
  chance(probability: number): boolean {
    return this.next() < probability;
  }
}

/** Theme configuration for map generation */
interface ThemeConfig {
  primaryTile: number;
  secondaryTile: number;
  wallDensity: number;     // 0-1, probability of wall scatter
  waterLakeCount: [number, number]; // [min, max] number of lakes
  waterLakeSize: [number, number]; // [min, max] initial cells per lake seed
  decorTypes: string[];
  decorDensity: number;    // 0-1, probability of decoration at walkable tile
}

const THEME_CONFIGS: Record<MapTheme, ThemeConfig> = {
  plains: {
    primaryTile: TILE_GRASS,
    secondaryTile: TILE_DIRT,
    wallDensity: 0.03,
    waterLakeCount: [2, 4],
    waterLakeSize: [8, 16],
    decorTypes: ['tree', 'bush', 'flower', 'rock'],
    decorDensity: 0.06,
  },
  forest: {
    primaryTile: TILE_GRASS,
    secondaryTile: TILE_DIRT,
    wallDensity: 0.17,
    waterLakeCount: [2, 3],
    waterLakeSize: [6, 12],
    decorTypes: ['tree', 'bush', 'mushroom', 'rock'],
    decorDensity: 0.08,
  },
  mountain: {
    primaryTile: TILE_STONE,
    secondaryTile: TILE_DIRT,
    wallDensity: 0.12,
    waterLakeCount: [1, 3],
    waterLakeSize: [4, 10],
    decorTypes: ['rock', 'boulder', 'bush'],
    decorDensity: 0.05,
  },
  desert: {
    primaryTile: TILE_DIRT,
    secondaryTile: TILE_STONE,
    wallDensity: 0.04,
    waterLakeCount: [2, 4],
    waterLakeSize: [4, 8],
    decorTypes: ['rock', 'cactus', 'bones'],
    decorDensity: 0.04,
  },
  abyss: {
    primaryTile: TILE_STONE,
    secondaryTile: TILE_DIRT,
    wallDensity: 0.10,
    waterLakeCount: [3, 5],
    waterLakeSize: [6, 14],
    decorTypes: ['rock', 'crystal', 'bones', 'mushroom'],
    decorDensity: 0.05,
  },
};

/** Collect all key points from the map anchors */
function getKeyPoints(map: MapData): { col: number; row: number }[] {
  const points: { col: number; row: number }[] = [];
  points.push(map.playerStart);
  for (const camp of map.camps) {
    points.push({ col: camp.col, row: camp.row });
  }
  for (const spawn of map.spawns) {
    points.push({ col: spawn.col, row: spawn.row });
  }
  for (const exit of map.exits) {
    points.push({ col: exit.col, row: exit.row });
  }
  return points;
}

/** Create a 2D array filled with a value */
function create2D<T>(cols: number, rows: number, value: T): T[][] {
  const arr: T[][] = [];
  for (let r = 0; r < rows; r++) {
    arr.push(new Array(cols).fill(value));
  }
  return arr;
}

/** Bresenham-style drunk walk between two points, carving a path */
function drunkWalk(
  tiles: number[][],
  fromCol: number, fromRow: number,
  toCol: number, toRow: number,
  pathTile: number,
  rng: SeededRandom,
  cols: number, rows: number,
): void {
  let col = fromCol;
  let row = fromRow;
  const maxSteps = (cols + rows) * 3;
  let steps = 0;

  while ((col !== toCol || row !== toRow) && steps < maxSteps) {
    steps++;
    // Set current position to path tile (but not if it's a camp or border wall)
    if (col > 0 && col < cols - 1 && row > 0 && row < rows - 1) {
      if (!isCampTile(tiles[row][col])) {
        tiles[row][col] = pathTile;
      }
      // Widen the path slightly
      if (rng.chance(0.5)) {
        const adjacentCol = col + rng.nextInt(-1, 1);
        const adjacentRow = row + rng.nextInt(-1, 1);
        if (adjacentCol > 0 && adjacentCol < cols - 1 && adjacentRow > 0 && adjacentRow < rows - 1) {
          if (!isCampTile(tiles[adjacentRow][adjacentCol])) {
            tiles[adjacentRow][adjacentCol] = pathTile;
          }
        }
      }
    }

    // Move toward target with some randomness (drunk walk)
    const dx = toCol - col;
    const dy = toRow - row;

    if (rng.chance(0.7)) {
      // Move toward target
      if (Math.abs(dx) > Math.abs(dy)) {
        col += dx > 0 ? 1 : -1;
      } else {
        row += dy > 0 ? 1 : -1;
      }
    } else {
      // Random step
      const dir = rng.nextInt(0, 3);
      if (dir === 0 && col < cols - 2) col++;
      else if (dir === 1 && col > 1) col--;
      else if (dir === 2 && row < rows - 2) row++;
      else if (dir === 3 && row > 1) row--;
    }
  }

  // Ensure final position is set
  if (toCol > 0 && toCol < cols - 1 && toRow > 0 && toRow < rows - 1) {
    if (!isCampTile(tiles[toRow][toCol])) {
      tiles[toRow][toCol] = pathTile;
    }
  }
}

/** Clear area around a point to ensure walkability */
function clearArea(
  tiles: number[][],
  centerCol: number, centerRow: number,
  radius: number,
  fillTile: number,
  cols: number, rows: number,
): void {
  for (let dr = -radius; dr <= radius; dr++) {
    for (let dc = -radius; dc <= radius; dc++) {
      const r = centerRow + dr;
      const c = centerCol + dc;
      if (r > 0 && r < rows - 1 && c > 0 && c < cols - 1) {
        tiles[r][c] = fillTile;
      }
    }
  }
}

/** Returns true if tile is any camp tile (ground or wall) */
function isCampTile(tile: number): boolean {
  return tile === TILE_CAMP || tile === TILE_CAMP_WALL;
}

/** Run cellular automata to create organic shapes (used for water bodies) */
function cellularAutomata(
  grid: boolean[][],
  iterations: number,
  cols: number,
  rows: number,
  birthThreshold: number,
  deathThreshold: number,
): boolean[][] {
  let current = grid.map(row => [...row]);

  for (let iter = 0; iter < iterations; iter++) {
    const next = current.map(row => [...row]);
    for (let r = 1; r < rows - 1; r++) {
      for (let c = 1; c < cols - 1; c++) {
        let neighbors = 0;
        for (let dr = -1; dr <= 1; dr++) {
          for (let dc = -1; dc <= 1; dc++) {
            if (dr === 0 && dc === 0) continue;
            if (current[r + dr][c + dc]) neighbors++;
          }
        }
        if (current[r][c]) {
          next[r][c] = neighbors >= deathThreshold;
        } else {
          next[r][c] = neighbors >= birthThreshold;
        }
      }
    }
    current = next;
  }
  return current;
}

export class MapGenerator {
  /**
   * Generate tiles, collisions, and decorations for a MapData
   * that has anchors (camps, spawns, exits, playerStart) but empty tiles/collisions.
   * The map must have theme and seed set.
   */
  static generate(map: MapData): MapData {
    const theme = map.theme ?? 'plains';
    const seed = map.seed ?? 42;
    const rng = new SeededRandom(seed);
    const { cols, rows } = map;
    const config = THEME_CONFIGS[theme];

    // (a) Initialize grid with primary tile
    const tiles = create2D(cols, rows, config.primaryTile);

    // (b) Place walls on all borders
    for (let c = 0; c < cols; c++) {
      tiles[0][c] = TILE_WALL;
      tiles[rows - 1][c] = TILE_WALL;
    }
    for (let r = 0; r < rows; r++) {
      tiles[r][0] = TILE_WALL;
      tiles[r][cols - 1] = TILE_WALL;
    }

    // Scatter secondary tile for variety
    for (let r = 1; r < rows - 1; r++) {
      for (let c = 1; c < cols - 1; c++) {
        if (rng.chance(0.08)) {
          tiles[r][c] = config.secondaryTile;
        }
      }
    }

    // (f-i) Theme-specific wall/obstacle scattering
    for (let r = 2; r < rows - 2; r++) {
      for (let c = 2; c < cols - 2; c++) {
        if (rng.chance(config.wallDensity)) {
          tiles[r][c] = TILE_WALL;
        }
      }
    }

    // For mountain theme: add stone clusters and wall ridges
    if (theme === 'mountain') {
      const ridgeCount = rng.nextInt(3, 6);
      for (let i = 0; i < ridgeCount; i++) {
        let rc = rng.nextInt(5, cols - 6);
        let rr = rng.nextInt(5, rows - 6);
        const length = rng.nextInt(6, 15);
        const dirCol = rng.chance(0.5) ? 1 : 0;
        const dirRow = dirCol === 1 ? 0 : 1;
        for (let s = 0; s < length; s++) {
          if (rc > 1 && rc < cols - 2 && rr > 1 && rr < rows - 2) {
            tiles[rr][rc] = TILE_WALL;
            // Add some width
            if (rng.chance(0.4)) {
              const offR = rr + (dirCol === 1 ? rng.nextInt(-1, 1) : 0);
              const offC = rc + (dirRow === 1 ? rng.nextInt(-1, 1) : 0);
              if (offR > 1 && offR < rows - 2 && offC > 1 && offC < cols - 2) {
                tiles[offR][offC] = TILE_WALL;
              }
            }
          }
          rc += dirCol + (rng.chance(0.3) ? rng.nextInt(-1, 1) : 0);
          rr += dirRow + (rng.chance(0.3) ? rng.nextInt(-1, 1) : 0);
        }
      }
    }

    // (e) Generate water bodies using cellular automata
    const lakeCount = rng.nextInt(config.waterLakeCount[0], config.waterLakeCount[1]);
    const waterGrid = create2D(cols, rows, false);

    // Seed water cells for each lake
    for (let lake = 0; lake < lakeCount; lake++) {
      const lakeCol = rng.nextInt(8, cols - 9);
      const lakeRow = rng.nextInt(8, rows - 9);
      const lakeSize = rng.nextInt(config.waterLakeSize[0], config.waterLakeSize[1]);

      for (let i = 0; i < lakeSize; i++) {
        const wc = lakeCol + rng.nextInt(-3, 3);
        const wr = lakeRow + rng.nextInt(-3, 3);
        if (wc > 2 && wc < cols - 3 && wr > 2 && wr < rows - 3) {
          waterGrid[wr][wc] = true;
        }
      }
    }

    // Run cellular automata to make water organic
    const refinedWater = cellularAutomata(waterGrid, 4, cols, rows, 5, 3);

    // Apply water to tiles
    for (let r = 2; r < rows - 2; r++) {
      for (let c = 2; c < cols - 2; c++) {
        if (refinedWater[r][c]) {
          tiles[r][c] = TILE_WATER;
        }
      }
    }

    // (c) Place camp tiles at camp positions (11x11 encampment with palisade walls)
    for (const camp of map.camps) {
      const halfSize = 5; // half of 11
      // Clear a 13x13 walkable area around camp center first
      clearArea(tiles, camp.col, camp.row, halfSize + 1, config.primaryTile, cols, rows);
      // Place 11x11 camp ground tiles
      for (let dr = -halfSize; dr <= halfSize; dr++) {
        for (let dc = -halfSize; dc <= halfSize; dc++) {
          const r = camp.row + dr;
          const c = camp.col + dc;
          if (r > 0 && r < rows - 1 && c > 0 && c < cols - 1) {
            tiles[r][c] = TILE_CAMP;
          }
        }
      }
      // Place palisade walls: top row (with 2-tile gate gap at dc=-1 and dc=0)
      for (let dc = -halfSize; dc <= halfSize; dc++) {
        if (dc === -1 || dc === 0) continue; // gate opening
        const r = camp.row - halfSize;
        const c = camp.col + dc;
        if (r > 0 && r < rows - 1 && c > 0 && c < cols - 1) {
          tiles[r][c] = TILE_CAMP_WALL;
        }
      }
      // Place palisade walls: left column from top row down to (halfSize-2) leaving bottom 2 rows open
      for (let dr = -halfSize; dr <= -(halfSize - 2); dr++) {
        const r = camp.row + dr;
        const c = camp.col - halfSize;
        if (r > 0 && r < rows - 1 && c > 0 && c < cols - 1) {
          tiles[r][c] = TILE_CAMP_WALL;
        }
      }
      // Place palisade walls: right column from top row down to (halfSize-2) leaving bottom 2 rows open
      for (let dr = -halfSize; dr <= -(halfSize - 2); dr++) {
        const r = camp.row + dr;
        const c = camp.col + halfSize;
        if (r > 0 && r < rows - 1 && c > 0 && c < cols - 1) {
          tiles[r][c] = TILE_CAMP_WALL;
        }
      }
    }

    // Clear areas around spawns, exits, and playerStart
    for (const spawn of map.spawns) {
      clearArea(tiles, spawn.col, spawn.row, 2, config.primaryTile, cols, rows);
    }
    for (const exit of map.exits) {
      clearArea(tiles, exit.col, exit.row, 1, config.primaryTile, cols, rows);
    }
    clearArea(tiles, map.playerStart.col, map.playerStart.row, 2, config.primaryTile, cols, rows);

    // (d) Generate paths connecting key points using drunk walk
    const keyPoints = getKeyPoints(map);
    const pathTile = TILE_DIRT;

    // Connect playerStart to first camp
    if (map.camps.length > 0) {
      drunkWalk(tiles, map.playerStart.col, map.playerStart.row,
        map.camps[0].col, map.camps[0].row, pathTile, rng, cols, rows);
    }

    // Connect camps to each other
    for (let i = 0; i < map.camps.length - 1; i++) {
      drunkWalk(tiles, map.camps[i].col, map.camps[i].row,
        map.camps[i + 1].col, map.camps[i + 1].row, pathTile, rng, cols, rows);
    }

    // Connect last camp to exits
    const lastCamp = map.camps[map.camps.length - 1];
    if (lastCamp) {
      for (const exit of map.exits) {
        drunkWalk(tiles, lastCamp.col, lastCamp.row,
          exit.col, exit.row, pathTile, rng, cols, rows);
      }
    }

    // Connect playerStart to exits
    for (const exit of map.exits) {
      drunkWalk(tiles, map.playerStart.col, map.playerStart.row,
        exit.col, exit.row, pathTile, rng, cols, rows);
    }

    // Connect spawn areas to nearest camp or playerStart
    for (const spawn of map.spawns) {
      // Find nearest key anchor
      let nearest = map.playerStart;
      let bestDist = Math.abs(spawn.col - nearest.col) + Math.abs(spawn.row - nearest.row);
      for (const camp of map.camps) {
        const d = Math.abs(spawn.col - camp.col) + Math.abs(spawn.row - camp.row);
        if (d < bestDist) {
          bestDist = d;
          nearest = camp;
        }
      }
      drunkWalk(tiles, spawn.col, spawn.row, nearest.col, nearest.row, pathTile, rng, cols, rows);
    }

    // Ensure border walls are intact after path carving
    for (let c = 0; c < cols; c++) {
      tiles[0][c] = TILE_WALL;
      tiles[rows - 1][c] = TILE_WALL;
    }
    for (let r = 0; r < rows; r++) {
      tiles[r][0] = TILE_WALL;
      tiles[r][cols - 1] = TILE_WALL;
    }

    // Re-ensure exits on border are walkable (set adjacent inner tile)
    for (const exit of map.exits) {
      // If exit is on border, make sure the tile next to it (inward) is walkable
      if (exit.col === 0 || exit.col === cols - 1 || exit.row === 0 || exit.row === rows - 1) {
        const innerCol = exit.col === 0 ? 1 : exit.col === cols - 1 ? cols - 2 : exit.col;
        const innerRow = exit.row === 0 ? 1 : exit.row === rows - 1 ? rows - 2 : exit.row;
        tiles[innerRow][innerCol] = pathTile;
        // Also clear a small area around the inner point
        clearArea(tiles, innerCol, innerRow, 1, config.primaryTile, cols, rows);
      }
    }

    // (j) Generate collisions array
    const collisions = create2D(cols, rows, true);
    for (let r = 0; r < rows; r++) {
      for (let c = 0; c < cols; c++) {
        const tile = tiles[r][c];
        collisions[r][c] = tile !== TILE_WALL && tile !== TILE_WATER && tile !== TILE_CAMP_WALL;
      }
    }

    // (k) Place decorations
    const decorations: { col: number; row: number; type: string }[] = [];
    for (let r = 2; r < rows - 2; r++) {
      for (let c = 2; c < cols - 2; c++) {
        if (collisions[r][c] && !isCampTile(tiles[r][c]) && tiles[r][c] !== TILE_DIRT) {
          if (rng.chance(config.decorDensity)) {
            const decorType = config.decorTypes[rng.nextInt(0, config.decorTypes.length - 1)];
            decorations.push({ col: c, row: r, type: decorType });
          }
        }
      }
    }

    return {
      ...map,
      tiles,
      collisions,
      decorations,
    };
  }
}
