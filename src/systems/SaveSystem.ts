import Dexie from 'dexie';
import type { SaveData, ItemInstance } from '../data/types';
import { SpiritSystem } from './SpiritSystem';

/** Current save format version. New saves are stamped with this. */
export const CURRENT_SAVE_VERSION = 3;

class AbyssfireDB extends Dexie {
  saves!: Dexie.Table<SaveData, string>;

  constructor() {
    super('AbyssfireDB');
    this.version(1).stores({
      saves: 'id, timestamp',
    });
  }
}

const db = new AbyssfireDB();

// ---------------------------------------------------------------------------
// Migration helpers
// ---------------------------------------------------------------------------

/** Ensure every ItemInstance has a `sockets` array (added in v2 for gem socketing). */
function ensureItemSockets(items: ItemInstance[]): void {
  for (const item of items) {
    if (!item.sockets) {
      item.sockets = [];
    }
  }
}

/**
 * Migrate a v1 (v0.10.0) save to v2 format in-place.
 *
 * Guarantees:
 * - Missing fields initialised to safe defaults
 * - Existing data preserved unchanged
 * - Version bumped to 2
 */
export function migrateV1toV2(save: SaveData): SaveData {
  // Defensive: ensure top-level collections exist
  if (!save.inventory) save.inventory = [];
  if (!save.equipment) save.equipment = {};
  if (!save.stash) save.stash = [];
  if (!save.quests) save.quests = [];
  if (!save.exploration) save.exploration = {};
  if (!save.achievements) save.achievements = {};

  // Homestead defaults
  if (!save.homestead) {
    save.homestead = { buildings: {}, pets: [] };
  } else {
    if (!save.homestead.buildings) save.homestead.buildings = {};
    if (!save.homestead.pets) save.homestead.pets = [];
  }

  // Settings defaults
  if (!save.settings) {
    save.settings = { autoCombat: false, musicVolume: 0.5, sfxVolume: 0.7, autoLootMode: 'off' };
  }

  // --- V2-specific fields ---

  // Difficulty system (Normal by default, no completed difficulties)
  if (!save.difficulty) save.difficulty = 'normal';
  if (!save.completedDifficulties) save.completedDifficulties = [];

  // Mercenary — undefined means no mercenary hired (safe default)
  // We intentionally do NOT set a default; undefined is the correct "no mercenary" state.
  // Explicitly ensure no stale/corrupt mercenary data from partial saves
  if (save.mercenary && typeof save.mercenary !== 'object') {
    save.mercenary = undefined;
  }

  // Companion defaults: ensure homestead pets array and activePet exist
  // These comprise the companion state (mercenary + pets) for v2 saves
  if (!save.homestead.pets) save.homestead.pets = [];
  if (save.homestead.activePet === undefined) save.homestead.activePet = undefined;

  // Dialogue tree state
  if (!save.dialogueState) save.dialogueState = {};

  // Mini-boss dialogue tracking
  if (!save.miniBossDialogueSeen) save.miniBossDialogueSeen = [];

  // Lore collectibles
  if (!save.loreCollected) save.loreCollected = [];

  // Hidden area discovery
  if (!save.discoveredHiddenAreas) save.discoveredHiddenAreas = [];

  // Ensure gem socket arrays on all items
  ensureItemSockets(save.inventory);
  ensureItemSockets(save.stash);
  for (const item of Object.values(save.equipment)) {
    if (item && !item.sockets) item.sockets = [];
  }

  // Stamp the exact target version so chained migrations can continue.
  save.version = 2;

  return save;
}

/**
 * Migrate v2 saves to v3 by adding the class-specific Spirit resource state.
 * Existing progress is preserved and malformed values are clamped safely.
 */
export function migrateV2toV3(save: SaveData): SaveData {
  save.player.spirit = new SpiritSystem(
    save.classId,
    save.player?.spirit,
  ).toSaveState();
  save.version = 3;
  return save;
}

/** Run every required migration in order and return a current-version save. */
export function migrateSaveData(save: SaveData): SaveData {
  if (save.version < 2) migrateV1toV2(save);
  if (save.version < 3) migrateV2toV3(save);
  return save;
}

/**
 * Given a player tile position and the map's collision/camp data, determine
 * whether the position is unwalkable. If so, return the nearest camp position.
 * Returns `null` if the position is already walkable.
 */
export function findNearestWalkablePosition(
  tileCol: number,
  tileRow: number,
  collisions: boolean[][],
  camps: { col: number; row: number }[],
  cols: number,
  rows: number,
): { col: number; row: number } | null {
  const roundCol = Math.round(tileCol);
  const roundRow = Math.round(tileRow);

  // Out of bounds or on unwalkable tile
  const outOfBounds = roundCol < 0 || roundCol >= cols || roundRow < 0 || roundRow >= rows;
  const unwalkable = outOfBounds || !collisions[roundRow]?.[roundCol];

  if (!unwalkable) return null;

  // Find nearest camp position
  if (camps.length === 0) return null;

  let bestCamp = camps[0];
  let bestDist = Infinity;
  for (const camp of camps) {
    const dx = camp.col - tileCol;
    const dy = camp.row - tileRow;
    const dist = dx * dx + dy * dy;
    if (dist < bestDist) {
      bestDist = dist;
      bestCamp = camp;
    }
  }

  return { col: bestCamp.col, row: bestCamp.row };
}

// ---------------------------------------------------------------------------
// SaveSystem class
// ---------------------------------------------------------------------------

export class SaveSystem {
  async save(data: SaveData): Promise<void> {
    data.timestamp = Date.now();
    data.version = CURRENT_SAVE_VERSION;
    await db.saves.put(data);
  }

  async load(id: string): Promise<SaveData | undefined> {
    const data = await db.saves.get(id);
    if (data && data.version < CURRENT_SAVE_VERSION) {
      migrateSaveData(data);
      // Persist the migrated record so migration only runs once
      await db.saves.put(data);
    }
    return data;
  }

  async listSaves(): Promise<SaveData[]> {
    return await db.saves.orderBy('timestamp').reverse().toArray();
  }

  async deleteSave(id: string): Promise<void> {
    await db.saves.delete(id);
  }

  async autoSave(data: SaveData): Promise<void> {
    data.id = 'autosave';
    await this.save(data);
  }

  async loadAutoSave(): Promise<SaveData | undefined> {
    return await this.load('autosave');
  }

  async quickSave(data: SaveData, slot: number): Promise<void> {
    data.id = `save_${slot}`;
    await this.save(data);
  }
}
