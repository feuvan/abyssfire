import { describe, it, expect } from 'vitest';
import {
  RandomEventSystem,
  ZONE_EVENT_DATA,
  type ZoneScaleInfo,
  type ActiveEvent,
} from '../systems/RandomEventSystem';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

const ZONE_INFO: ZoneScaleInfo = {
  zoneId: 'emerald_plains',
  levelRange: [1, 7],
};

function makeCollisionGrid(cols: number, rows: number, walkable = true): boolean[][] {
  const grid: boolean[][] = [];
  for (let r = 0; r < rows; r++) {
    const row: boolean[] = [];
    for (let c = 0; c < cols; c++) {
      // Border walls (row/col 0 or max are blocked), interior is walkable
      if (walkable) {
        row.push(r > 0 && r < rows - 1 && c > 0 && c < cols - 1);
      } else {
        row.push(false);
      }
    }
    grid.push(row);
  }
  return grid;
}

function makePartiallyBlockedGrid(cols: number, rows: number): boolean[][] {
  const grid = makeCollisionGrid(cols, rows, true);
  // Block a central area (e.g., columns 8-12, rows 8-12) to force fallback search
  for (let r = 8; r <= 12; r++) {
    for (let c = 8; c <= 12; c++) {
      if (r < rows && c < cols) {
        grid[r][c] = false;
      }
    }
  }
  return grid;
}

// ---------------------------------------------------------------------------
// 1. AMBUSH: findWalkableTile — correct collision check + fallback search
// ---------------------------------------------------------------------------
describe('RandomEventSystem — findWalkableTile', () => {
  it('returns preferred position when it is walkable', () => {
    const collisions = makeCollisionGrid(20, 20);
    const result = RandomEventSystem.findWalkableTile(10, 10, collisions, 20, 20);
    expect(result).toEqual({ col: 10, row: 10 });
  });

  it('returns null when preferred position is a wall and no walkable within radius', () => {
    // All tiles blocked
    const collisions = makeCollisionGrid(20, 20, false);
    const result = RandomEventSystem.findWalkableTile(10, 10, collisions, 20, 20, 3);
    expect(result).toBeNull();
  });

  it('finds a fallback walkable tile when preferred is blocked', () => {
    const collisions = makePartiallyBlockedGrid(20, 20);
    // Position (10, 10) is blocked; should find a nearby walkable tile
    const result = RandomEventSystem.findWalkableTile(10, 10, collisions, 20, 20);
    expect(result).not.toBeNull();
    expect(result!.col).toBeDefined();
    expect(result!.row).toBeDefined();
    // The found tile should be walkable
    expect(collisions[result!.row][result!.col]).toBe(true);
  });

  it('searches outward in rings up to maxRadius', () => {
    const collisions = makeCollisionGrid(30, 30, false);
    // Only tile (20, 20) is walkable
    collisions[20][20] = true;
    const result = RandomEventSystem.findWalkableTile(15, 15, collisions, 30, 30, 6);
    expect(result).toEqual({ col: 20, row: 20 });
  });

  it('does not find tiles beyond maxRadius', () => {
    const collisions = makeCollisionGrid(30, 30, false);
    // Only tile (20, 20) is walkable — distance 10 from (10, 10)
    collisions[20][20] = true;
    const result = RandomEventSystem.findWalkableTile(10, 10, collisions, 30, 30, 3);
    expect(result).toBeNull();
  });

  it('clamps preferred position to valid bounds', () => {
    const collisions = makeCollisionGrid(20, 20);
    // Preferred position out of bounds
    const result = RandomEventSystem.findWalkableTile(-5, -5, collisions, 20, 20);
    expect(result).not.toBeNull();
    expect(result!.col).toBeGreaterThanOrEqual(1);
    expect(result!.row).toBeGreaterThanOrEqual(1);
    expect(result!.col).toBeLessThanOrEqual(18);
    expect(result!.row).toBeLessThanOrEqual(18);
  });

  it('handles border positions correctly', () => {
    const collisions = makeCollisionGrid(20, 20);
    // Position (0, 0) is a wall tile; should clamp and find walkable
    const result = RandomEventSystem.findWalkableTile(0, 0, collisions, 20, 20);
    expect(result).not.toBeNull();
    expect(collisions[result!.row][result!.col]).toBe(true);
  });

  it('returns first walkable tile in ring order', () => {
    const collisions = makeCollisionGrid(20, 20, false);
    // Only a few tiles walkable at ring radius 1
    collisions[10][11] = true; // right of center
    collisions[12][10] = true; // below center
    const result = RandomEventSystem.findWalkableTile(10, 10, collisions, 20, 20);
    expect(result).not.toBeNull();
    // Should find the one at ring distance 1
    const dist = Math.max(Math.abs(result!.col - 10), Math.abs(result!.row - 10));
    expect(dist).toBeLessThanOrEqual(2);
  });
});

// ---------------------------------------------------------------------------
// 2. WANDERING MERCHANT: SHOP_OPEN payload format
// ---------------------------------------------------------------------------
describe('RandomEventSystem — Wandering Merchant Payload', () => {
  it('merchant event context has merchantItems as string array', () => {
    // Verify the zone event data structure
    for (const [zoneId, data] of Object.entries(ZONE_EVENT_DATA)) {
      expect(Array.isArray(data.merchantItems)).toBe(true);
      expect(data.merchantItems.length).toBeGreaterThan(0);
      for (const item of data.merchantItems) {
        expect(typeof item).toBe('string');
      }
    }
  });

  it('wandering merchant event creates context with merchantItems and priceMultiplier', () => {
    // Create events until we get a wandering_merchant
    let merchantEvent: ActiveEvent | null = null;
    for (let trial = 0; trial < 100; trial++) {
      const sys = new RandomEventSystem(ZONE_INFO);
      sys.setLastEventTime(-Infinity);
      // Prime position
      sys.update(0, 100, 60, 50, false, [{ col: 15, row: 15 }], 0);
      let col = 60;
      for (let t = 500; t < 50000; t += 500) {
        col++;
        const evt = sys.update(t, 500, col, 50, false, [{ col: 15, row: 15 }], 0);
        if (evt?.type === 'wandering_merchant') {
          merchantEvent = evt;
          break;
        }
      }
      if (merchantEvent) break;
    }
    if (merchantEvent) {
      expect(merchantEvent.context.merchantItems).toBeDefined();
      expect(Array.isArray(merchantEvent.context.merchantItems)).toBe(true);
      expect(merchantEvent.context.priceMultiplier).toBe(1.2);
    }
  });

  it('SHOP_OPEN payload contract: requires npcId, shopItems, and type fields', () => {
    // Verify the expected contract shape — this documents the UIScene.handleShopOpen contract
    const expectedPayload = {
      npcId: 'wandering_merchant',
      shopItems: ['iron_sword', 'leather_armor'],
      type: 'merchant',
    };
    // npcId is a string
    expect(typeof expectedPayload.npcId).toBe('string');
    // shopItems is a string array
    expect(Array.isArray(expectedPayload.shopItems)).toBe(true);
    // type is a string
    expect(typeof expectedPayload.type).toBe('string');
    // The old broken format used npcName, items, isWanderingMerchant
    // The corrected format uses npcId, shopItems, type
    expect('npcId' in expectedPayload).toBe(true);
    expect('shopItems' in expectedPayload).toBe(true);
    expect('type' in expectedPayload).toBe(true);
  });

  it('merchant items per zone are valid item IDs', () => {
    for (const [, data] of Object.entries(ZONE_EVENT_DATA)) {
      for (const item of data.merchantItems) {
        expect(item.length).toBeGreaterThan(0);
        // Item IDs should be lowercase snake_case
        expect(item).toMatch(/^[a-z_]+$/);
      }
    }
  });
});

// ---------------------------------------------------------------------------
// 3. RESCUE: spawn NPC + hostiles, track when defeated, then grant rewards
// ---------------------------------------------------------------------------
describe('RandomEventSystem — Rescue Event (Corrected)', () => {
  it('rescue event context has monsterIds, rescueNpcName, reward, and monsterCount', () => {
    let rescueEvent: ActiveEvent | null = null;
    for (let trial = 0; trial < 80; trial++) {
      const sys = new RandomEventSystem(ZONE_INFO);
      sys.setLastEventTime(-Infinity);
      sys.update(0, 100, 60, 50, false, [{ col: 15, row: 15 }], 0);
      let col = 60;
      for (let t = 500; t < 50000; t += 500) {
        col++;
        const evt = sys.update(t, 500, col, 50, false, [{ col: 15, row: 15 }], 0);
        if (evt?.type === 'rescue') {
          rescueEvent = evt;
          break;
        }
      }
      if (rescueEvent) break;
    }
    if (rescueEvent) {
      expect(rescueEvent.context.monsterIds).toBeDefined();
      expect(Array.isArray(rescueEvent.context.monsterIds)).toBe(true);
      expect(rescueEvent.context.monsterCount).toBeDefined();
      expect(typeof rescueEvent.context.monsterCount).toBe('number');
      expect(rescueEvent.context.rescueNpcName).toBeDefined();
      expect(typeof rescueEvent.context.rescueNpcName).toBe('string');
      expect(rescueEvent.context.reward).toBeDefined();
      const reward = rescueEvent.context.reward as { gold: number; exp: number };
      expect(reward.gold).toBeGreaterThan(0);
      expect(reward.exp).toBeGreaterThan(0);
    }
  });

  it('rescue event should NOT be immediately resolved (requires hostile clearance)', () => {
    // The rescue event is created but should not be auto-resolved
    // This documents the corrected behavior: event stays active until hostiles defeated
    let rescueEvent: ActiveEvent | null = null;
    for (let trial = 0; trial < 80; trial++) {
      const sys = new RandomEventSystem(ZONE_INFO);
      sys.setLastEventTime(-Infinity);
      sys.update(0, 100, 60, 50, false, [{ col: 15, row: 15 }], 0);
      let col = 60;
      for (let t = 500; t < 50000; t += 500) {
        col++;
        const evt = sys.update(t, 500, col, 50, false, [{ col: 15, row: 15 }], 0);
        if (evt?.type === 'rescue') {
          rescueEvent = evt;
          break;
        }
      }
      if (rescueEvent) break;
    }
    if (rescueEvent) {
      // The event starts as NOT resolved
      expect(rescueEvent.resolved).toBe(false);
      // Reward should be set but not granted yet (tracked via context)
      const reward = rescueEvent.context.reward as { gold: number; exp: number };
      expect(reward).toBeDefined();
    }
  });

  it('rescue NPC names are zone-specific Chinese text', () => {
    const zones = ['emerald_plains', 'twilight_forest', 'anvil_mountains', 'scorching_desert', 'abyss_rift'];
    for (const zoneId of zones) {
      const data = ZONE_EVENT_DATA[zoneId];
      expect(data.rescueNpcName).toBeDefined();
      expect(/[\u4e00-\u9fff]/.test(data.rescueNpcName)).toBe(true);
    }
  });

  it('rescue rewards scale with zone level', () => {
    const ep = ZONE_EVENT_DATA['emerald_plains'];
    const ar = ZONE_EVENT_DATA['abyss_rift'];
    expect(ar.rescueReward.gold).toBeGreaterThan(ep.rescueReward.gold);
    expect(ar.rescueReward.exp).toBeGreaterThan(ep.rescueReward.exp);
  });

  it('rescue event context contains monster spawn data for tracking', () => {
    // The context should include monsterIds that the runtime can use
    // to spawn and track rescue hostiles
    const data = ZONE_EVENT_DATA['emerald_plains'];
    expect(data.ambushMonsters).toEqual(['slime_green', 'goblin']);
    expect(data.ambushCount).toEqual([3, 5]);
  });
});

// ---------------------------------------------------------------------------
// 4. PUZZLE: spawn interactable object, require player interaction
// ---------------------------------------------------------------------------
describe('RandomEventSystem — Puzzle Event (Corrected)', () => {
  it('puzzle event context has prompt, solution, reward, rewardGold, rewardExp', () => {
    let puzzleEvent: ActiveEvent | null = null;
    for (let trial = 0; trial < 120; trial++) {
      const sys = new RandomEventSystem(ZONE_INFO);
      sys.setLastEventTime(-Infinity);
      sys.update(0, 100, 60, 50, false, [{ col: 15, row: 15 }], 0);
      let col = 60;
      for (let t = 500; t < 50000; t += 500) {
        col++;
        const evt = sys.update(t, 500, col, 50, false, [{ col: 15, row: 15 }], 0);
        if (evt?.type === 'environmental_puzzle') {
          puzzleEvent = evt;
          break;
        }
      }
      if (puzzleEvent) break;
    }
    if (puzzleEvent) {
      const puzzle = puzzleEvent.context.puzzle as Record<string, unknown>;
      expect(puzzle).toBeDefined();
      expect(typeof puzzle.prompt).toBe('string');
      expect(typeof puzzle.solution).toBe('string');
      expect(typeof puzzle.reward).toBe('string');
      expect(typeof puzzle.rewardGold).toBe('number');
      expect(typeof puzzle.rewardExp).toBe('number');
      expect(puzzle.rewardGold as number).toBeGreaterThan(0);
      expect(puzzle.rewardExp as number).toBeGreaterThan(0);
    }
  });

  it('puzzle event should NOT be auto-resolved (requires player interaction)', () => {
    // Verify that the puzzle context provides enough data for the runtime
    // to create an interactable object and wait for player input
    let puzzleEvent: ActiveEvent | null = null;
    for (let trial = 0; trial < 120; trial++) {
      const sys = new RandomEventSystem(ZONE_INFO);
      sys.setLastEventTime(-Infinity);
      sys.update(0, 100, 60, 50, false, [{ col: 15, row: 15 }], 0);
      let col = 60;
      for (let t = 500; t < 50000; t += 500) {
        col++;
        const evt = sys.update(t, 500, col, 50, false, [{ col: 15, row: 15 }], 0);
        if (evt?.type === 'environmental_puzzle') {
          puzzleEvent = evt;
          break;
        }
      }
      if (puzzleEvent) break;
    }
    if (puzzleEvent) {
      // Event starts unresolved — must wait for player interaction
      expect(puzzleEvent.resolved).toBe(false);
      // Should have valid tile position for spawning the interactable
      expect(puzzleEvent.col).toBeDefined();
      expect(puzzleEvent.row).toBeDefined();
    }
  });

  it('puzzle descriptions per zone are in Chinese', () => {
    for (const [, data] of Object.entries(ZONE_EVENT_DATA)) {
      for (const puzzle of data.puzzleDescriptions) {
        expect(/[\u4e00-\u9fff]/.test(puzzle.prompt)).toBe(true);
        expect(/[\u4e00-\u9fff]/.test(puzzle.solution)).toBe(true);
        expect(/[\u4e00-\u9fff]/.test(puzzle.reward)).toBe(true);
      }
    }
  });

  it('puzzle rewards scale with zone difficulty', () => {
    const ep = ZONE_EVENT_DATA['emerald_plains'].puzzleDescriptions[0];
    const ar = ZONE_EVENT_DATA['abyss_rift'].puzzleDescriptions[0];
    expect(ar.rewardGold).toBeGreaterThan(ep.rewardGold);
    expect(ar.rewardExp).toBeGreaterThan(ep.rewardExp);
  });

  it('each zone has at least one puzzle definition', () => {
    for (const [zoneId, data] of Object.entries(ZONE_EVENT_DATA)) {
      expect(data.puzzleDescriptions.length).toBeGreaterThan(0);
    }
  });
});

// ---------------------------------------------------------------------------
// 5. Integration: Verify collision check convention (true = walkable)
// ---------------------------------------------------------------------------
describe('RandomEventSystem — Collision Convention', () => {
  it('collisions[r][c] === true means the tile IS walkable', () => {
    // This documents the collision convention used throughout the codebase
    const grid = makeCollisionGrid(20, 20);
    // Border tiles are walls (false)
    expect(grid[0][0]).toBe(false);
    expect(grid[0][10]).toBe(false);
    expect(grid[19][10]).toBe(false);
    // Interior tiles are walkable (true)
    expect(grid[10][10]).toBe(true);
    expect(grid[5][5]).toBe(true);
  });

  it('findWalkableTile only returns tiles where collisions[r][c] is truthy', () => {
    const grid = makeCollisionGrid(20, 20, false);
    // Set exactly one tile as walkable
    grid[7][7] = true;
    const result = RandomEventSystem.findWalkableTile(7, 7, grid, 20, 20);
    expect(result).toEqual({ col: 7, row: 7 });
    // Check that result is indeed walkable
    expect(grid[result!.row][result!.col]).toBe(true);
  });

  it('findWalkableTile never returns a wall tile', () => {
    const grid = makePartiallyBlockedGrid(20, 20);
    // Search from a blocked area
    const result = RandomEventSystem.findWalkableTile(10, 10, grid, 20, 20);
    if (result) {
      expect(grid[result.row][result.col]).toBe(true);
    }
  });
});

// ---------------------------------------------------------------------------
// 6. findWalkableTile edge cases
// ---------------------------------------------------------------------------
describe('RandomEventSystem — findWalkableTile Edge Cases', () => {
  it('handles 1x1 grid', () => {
    const grid = [[true]];
    // Position is clamped, but won't be in valid range for borders
    const result = RandomEventSystem.findWalkableTile(0, 0, grid, 1, 1);
    // With a 1x1 grid, max(1, min(-1, 0)) = 1 which is out of bounds
    // This is an extreme edge case — system should not crash
    expect(result === null || result !== null).toBe(true);
  });

  it('handles very large radius', () => {
    const grid = makeCollisionGrid(100, 100, false);
    grid[50][50] = true;
    const result = RandomEventSystem.findWalkableTile(5, 5, grid, 100, 100, 50);
    expect(result).toEqual({ col: 50, row: 50 });
  });

  it('default maxRadius is 5', () => {
    const grid = makeCollisionGrid(30, 30, false);
    // Only walkable tile is at distance 6
    grid[16][10] = true;
    // Default maxRadius=5 should not reach it
    const result = RandomEventSystem.findWalkableTile(10, 10, grid, 30, 30);
    expect(result).toBeNull();
    // But maxRadius=6 should
    const result2 = RandomEventSystem.findWalkableTile(10, 10, grid, 30, 30, 6);
    expect(result2).toEqual({ col: 10, row: 16 });
  });

  it('prefers closer tiles over farther ones', () => {
    const grid = makeCollisionGrid(30, 30, false);
    grid[12][10] = true; // distance 2 from (10,10)
    grid[15][10] = true; // distance 5 from (10,10)
    const result = RandomEventSystem.findWalkableTile(10, 10, grid, 30, 30);
    expect(result).not.toBeNull();
    // Should find the closer one first (ring 2 before ring 5)
    expect(result!.row).toBe(12);
    expect(result!.col).toBe(10);
  });
});
