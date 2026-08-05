import { describe, expect, it } from 'vitest';
import {
  CURRENT_SAVE_VERSION,
  migrateSaveData,
  migrateV2toV3,
} from '../systems/SaveSystem';
import type { SaveData } from '../data/types';

function makeV2Save(): SaveData {
  return {
    id: 'spirit-migration',
    version: 2,
    timestamp: 123,
    classId: 'mage',
    player: {
      level: 8,
      exp: 42,
      gold: 100,
      hp: 80,
      maxHp: 100,
      mana: 60,
      maxMana: 90,
      stats: { str: 4, dex: 6, vit: 8, int: 20, spi: 16, lck: 5 },
      freeStatPoints: 0,
      freeSkillPoints: 2,
      skillLevels: { fireball: 5 },
      tileCol: 10,
      tileRow: 12,
      currentMap: 'emerald_plains',
    },
    inventory: [],
    equipment: {},
    stash: [],
    quests: [],
    exploration: {},
    homestead: { buildings: {}, pets: [] },
    achievements: {},
    settings: {
      autoCombat: false,
      musicVolume: 0.5,
      sfxVolume: 0.7,
      autoLootMode: 'off',
    },
    difficulty: 'normal',
    completedDifficulties: [],
  };
}

describe('Spirit save migration', () => {
  it('uses save format version 3', () => {
    expect(CURRENT_SAVE_VERSION).toBe(3);
  });

  it('adds a safe empty Spirit state to v2 saves', () => {
    const migrated = migrateV2toV3(makeV2Save());

    expect(migrated.version).toBe(3);
    expect(migrated.player.spirit).toEqual({
      value: 0,
      resonanceRemainingMs: 0,
    });
  });

  it('preserves existing Spirit progress during migration', () => {
    const save = makeV2Save();
    save.player.spirit = { value: 64, resonanceRemainingMs: 2500 };

    const migrated = migrateV2toV3(save);

    expect(migrated.player.spirit).toEqual({
      value: 64,
      resonanceRemainingMs: 2500,
    });
  });

  it('sanitizes malformed Spirit values', () => {
    const save = makeV2Save();
    save.player.spirit = { value: Number.NaN, resonanceRemainingMs: -10 };

    const migrated = migrateV2toV3(save);

    expect(migrated.player.spirit).toEqual({
      value: 0,
      resonanceRemainingMs: 0,
    });
  });

  it('clamps Resonance time to the saved class profile', () => {
    const save = makeV2Save();
    save.classId = 'rogue';
    save.player.spirit = { value: 100, resonanceRemainingMs: 9000 };

    expect(migrateV2toV3(save).player.spirit).toEqual({
      value: 100,
      resonanceRemainingMs: 5500,
    });
  });

  it('migrates a legacy v1 save through every version in one pass', () => {
    const legacy = makeV2Save();
    legacy.version = 1;
    delete (legacy as Partial<SaveData>).difficulty;
    delete (legacy as Partial<SaveData>).completedDifficulties;

    const migrated = migrateSaveData(legacy);

    expect(migrated.version).toBe(3);
    expect(migrated.difficulty).toBe('normal');
    expect(migrated.completedDifficulties).toEqual([]);
    expect(migrated.player.spirit).toEqual({
      value: 0,
      resonanceRemainingMs: 0,
    });
  });
});
