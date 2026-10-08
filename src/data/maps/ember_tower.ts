import type { MapData } from '../types';

/**
 * 余烬之塔 (Ember Tower) — the homestead zone: the elven tower on the Emerald
 * Plains where the hero woke in the ashes. Hand-authored 48×48, no monsters.
 *
 * Layout (col, row):
 *   - the tower on a stone plaza in the middle (24, 17), the altar behind it (24, 6)
 *   - wings on plots around it: gem workshop NW, caravan post NE, herb garden W,
 *     moon well E, warehouse SW (TOWER_PLOTS; EmberTower draws each plot's stage)
 *   - the pet meadow and its pond in the south-east (TOWER_MEADOW)
 *   - the camp (stash keeper) in the south, the return portal just below it
 *
 * Tiles: 0 grass, 1 dirt, 2 stone, 3 water, 4 wall, 5 camp ground.
 */
export const TOWER_SIZE = 48;

/** The tower prop's tile (its base sits here). */
export const TOWER_CENTER = { col: 24, row: 18 };
/** The return portal (传送阵). */
export const TOWER_PORTAL = { col: 24, row: 44 };
/** Pet meadow: owned, resting ley-beasts wander inside this circle. */
export const TOWER_MEADOW = { col: 34, row: 35, radius: 4 };

/** Building plots: the wing's prop sits on `col,row`, its ally stands at `npc`. */
export const TOWER_PLOTS: Record<string, { col: number; row: number; npc: { col: number; row: number } }> = {
  gem_workshop: { col: 11, row: 11, npc: { col: 13, row: 14 } },
  training_ground: { col: 37, row: 11, npc: { col: 35, row: 14 } },
  altar: { col: 24, row: 6, npc: { col: 27, row: 8 } },
  herb_garden: { col: 10, row: 24, npc: { col: 13, row: 26 } },
  pet_house: { col: 38, row: 24, npc: { col: 35, row: 26 } },
  warehouse: { col: 12, row: 34, npc: { col: 15, row: 36 } },
};

const WALL = 4, GRASS = 0, DIRT = 1, STONE = 2, WATER = 3, CAMP = 5;

function build(): { tiles: number[][]; collisions: boolean[][]; decorations: { col: number; row: number; type: string }[] } {
  const n = TOWER_SIZE;
  const tiles: number[][] = Array.from({ length: n }, () => new Array<number>(n).fill(GRASS));
  const set = (c: number, r: number, t: number) => { if (c >= 0 && r >= 0 && c < n && r < n) tiles[r][c] = t; };
  const disc = (cc: number, cr: number, rx: number, ry: number, t: number) => {
    for (let r = Math.floor(cr - ry); r <= Math.ceil(cr + ry); r++) {
      for (let c = Math.floor(cc - rx); c <= Math.ceil(cc + rx); c++) {
        const dx = (c - cc) / rx, dy = (r - cr) / ry;
        if (dx * dx + dy * dy <= 1) set(c, r, t);
      }
    }
  };
  const line = (c0: number, r0: number, c1: number, r1: number, t: number) => {
    // L-shaped path: along the row first, then the column (2 tiles wide).
    const [ca, cb] = c0 < c1 ? [c0, c1] : [c1, c0];
    for (let c = ca; c <= cb; c++) { set(c, r0, t); set(c, r0 + 1, t); }
    const [ra, rb] = r0 < r1 ? [r0, r1] : [r1, r0];
    for (let r = ra; r <= rb; r++) { set(c1, r, t); set(c1 + 1, r, t); }
  };

  // Paths first, so plazas and plots paint over them.
  line(24, 40, 24, 22, DIRT); // camp → plaza
  line(24, 12, 11, 12, DIRT); // plaza → gem workshop
  line(24, 12, 37, 12, DIRT); // plaza → caravan post
  line(24, 24, 11, 24, DIRT); // plaza → herb garden
  line(24, 24, 37, 24, DIRT); // plaza → moon well
  line(24, 33, 13, 33, DIRT); // camp road → warehouse
  line(24, 33, 33, 33, DIRT); // camp road → meadow

  // Stone plaza around the tower, and the altar's terrace behind it.
  disc(24, 17, 7, 6, STONE);
  disc(24, 6, 3.5, 2.5, STONE);
  // Plots: tilled soil for the garden, stone footings for the others.
  disc(10, 24, 3.5, 2.8, DIRT);
  for (const id of ['gem_workshop', 'training_ground', 'pet_house', 'warehouse'] as const) {
    const p = TOWER_PLOTS[id];
    disc(p.col, p.row, 2.6, 2.2, STONE);
  }
  // Camp ground and the portal's ring.
  disc(24, 38, 5.5, 4.5, CAMP);
  disc(TOWER_PORTAL.col, TOWER_PORTAL.row, 1.6, 1.2, STONE);
  // The meadow pond (moon water), fed by a thin brook from the moon well.
  disc(41, 40, 4.6, 3.6, WATER);
  for (let r = 27; r <= 36; r++) set(41, r, WATER);

  // Walls: a 2-tile rim, open nowhere (the portal is the only way out).
  for (let r = 0; r < n; r++) for (let c = 0; c < n; c++) {
    if (r < 2 || c < 2 || r >= n - 2 || c >= n - 2) tiles[r][c] = WALL;
  }

  const collisions = tiles.map(row => row.map(t => t !== WALL && t !== WATER));
  // The tower's footprint and every plot's centre are solid.
  for (let r = TOWER_CENTER.row - 3; r <= TOWER_CENTER.row; r++) {
    for (let c = TOWER_CENTER.col - 2; c <= TOWER_CENTER.col + 2; c++) collisions[r][c] = false;
  }
  for (const p of Object.values(TOWER_PLOTS)) {
    for (let r = p.row - 1; r <= p.row; r++) for (let c = p.col - 1; c <= p.col + 1; c++) collisions[r][c] = false;
  }

  // Decorations: a grove along the rim, flowers in the meadow, bushes by the paths.
  const decorations: { col: number; row: number; type: string }[] = [];
  let seed = 4817;
  const rnd = () => { seed = (seed * 1103515245 + 12345) & 0x7fffffff; return seed / 0x7fffffff; };
  const free = (c: number, r: number) => tiles[r]?.[c] === GRASS && collisions[r][c];
  for (let r = 2; r < n - 2; r++) {
    for (let c = 2; c < n - 2; c++) {
      if (!free(c, r)) continue;
      const rim = Math.min(r - 2, c - 2, n - 3 - r, n - 3 - c);
      const inMeadow = Math.hypot(c - TOWER_MEADOW.col, r - TOWER_MEADOW.row) <= TOWER_MEADOW.radius + 2;
      const v = rnd();
      // Keep the arrival spot and the portal ring clear of the rim grove.
      const nearPortal = Math.abs(c - TOWER_PORTAL.col) <= 6 && r >= TOWER_PORTAL.row - 4;
      if (rim <= 1 && v < 0.45 && !nearPortal) {
        decorations.push({ col: c, row: r, type: v < 0.25 ? 'tree' : 'tree_round' });
        collisions[r][c] = false;
      } else if (inMeadow && v < 0.16) {
        decorations.push({ col: c, row: r, type: v < 0.11 ? 'flower' : 'grass' });
      } else if (rim > 1 && v < 0.035) {
        decorations.push({ col: c, row: r, type: v < 0.015 ? 'bush' : v < 0.028 ? 'flower' : 'grass' });
      }
    }
  }
  return { tiles, collisions, decorations };
}

const built = build();

export const EmberTowerMap: MapData = {
  id: 'ember_tower',
  name: '余烬之塔',
  cols: TOWER_SIZE,
  rows: TOWER_SIZE,
  tiles: built.tiles,
  collisions: built.collisions,
  theme: 'plains',
  seed: 4817,
  spawns: [],
  camps: [{ col: 24, row: 38, npcs: ['stash'] }],
  playerStart: { col: 24, row: 41 },
  exits: [],
  levelRange: [1, 50],
  // The whole tower grounds are a safe zone.
  safeZoneRadius: 40,
  decorations: built.decorations,
  // Every ally that can live here; EmberTower spawns only those whose wing is unlocked.
  fieldNpcs: [
    { col: 21, row: 23, npcId: 'tower_elder' },
    ...(['herb_garden', 'pet_house', 'gem_workshop', 'training_ground', 'altar'] as const).map(id => ({
      col: TOWER_PLOTS[id].npc.col,
      row: TOWER_PLOTS[id].npc.row,
      npcId: ({ herb_garden: 'tower_herbalist', pet_house: 'tower_hermit', gem_workshop: 'tower_dwarf', training_ground: 'tower_nomad', altar: 'tower_warden' } as const)[id],
    })),
  ],
};
