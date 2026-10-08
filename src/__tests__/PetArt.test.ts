import { describe, expect, it } from 'vitest';
import { SpriteGenerator } from '../graphics/SpriteGenerator';
import { MAX_SHEET_DIMENSION, computeSheetGrid, playerAnimKey } from '../graphics/sprites/types';
import {
  PET_ACTION_FRAME_COUNTS,
  PET_ACTION_ORDER,
  PET_IDS,
  PET_SHEET_FRAMES,
  PET_STAGES,
  PET_VIEWS,
  PET_VIEW_FRAMES,
  allPetDrawers,
  getPetDrawer,
  getPetSheetMeta,
  petActionFrameRange,
  petSheetKey,
} from '../graphics/sprites/pets';
import { TOWER_BUILDING_IDS, TOWER_PROP_DRAWERS, towerBuildingKey } from '../graphics/sprites/decorations/TowerProps';

describe('ley-beast sheets', () => {
  it('has a drawer for every pet × evolution stage with the contract keys', () => {
    expect(PET_IDS).toHaveLength(8);
    const keys = allPetDrawers().map(d => d.key);
    expect(new Set(keys).size).toBe(PET_IDS.length * PET_STAGES.length);
    for (const id of PET_IDS) {
      expect(getPetDrawer(id, 0)!.key).toBe(`beast_${id}`);
      expect(getPetDrawer(id, 1)!.key).toBe(`beast_${id}_e1`);
      expect(getPetDrawer(id, 2)!.key).toBe(`beast_${id}_e2`);
    }
    expect(getPetDrawer('pet_unknown', 0)).toBeNull();
  });

  it('fits every sheet inside the texture limit at the largest texture scale', () => {
    for (const d of allPetDrawers()) {
      for (const scale of [2, 3]) {
        const grid = computeSheetGrid(d.frameW * scale, d.frameH * scale, d.totalFrames);
        expect(grid.width).toBeLessThanOrEqual(MAX_SHEET_DIMENSION);
        expect(grid.height).toBeLessThanOrEqual(MAX_SHEET_DIMENSION);
        expect(grid.cols * grid.rows).toBeGreaterThanOrEqual(d.totalFrames);
      }
    }
  });

  it('lays out every action once per view without gaps or overlaps', () => {
    expect(PET_VIEWS).toEqual(['se', 'ne']);
    expect(PET_ACTION_ORDER).toEqual(expect.arrayContaining(['idle', 'walk', 'attack', 'cast']));
    const seen = new Set<number>();
    for (const view of PET_VIEWS) {
      for (const action of PET_ACTION_ORDER) {
        const { start, end } = petActionFrameRange(view, action);
        expect(end - start + 1).toBe(PET_ACTION_FRAME_COUNTS[action]);
        for (let f = start; f <= end; f++) {
          expect(seen.has(f)).toBe(false);
          seen.add(f);
        }
        // Animation keys follow the monster / player view scheme.
        expect(playerAnimKey(petSheetKey('pet_cat', 0), view, action)).toBe(view === 'se' ? `beast_pet_cat_${action}` : `beast_pet_cat_ne_${action}`);
      }
    }
    expect(seen.size).toBe(PET_SHEET_FRAMES);
    expect(PET_SHEET_FRAMES).toBe(PET_VIEW_FRAMES * PET_VIEWS.length);
    for (const d of allPetDrawers()) {
      expect(d.totalFrames).toBe(PET_SHEET_FRAMES);
      expect(d.views).toEqual(PET_VIEWS);
    }
  });

  it('exposes placement metadata that grows with evolution', () => {
    for (const id of PET_IDS) {
      const metas = PET_STAGES.map(st => getPetSheetMeta(id, st)!);
      for (const m of metas) {
        expect(m.originY).toBeGreaterThan(0.5);
        expect(m.originY).toBeLessThan(1);
        expect(m.attackContact).toBe(1);
        expect(m.heightPx).toBeGreaterThan(8);
        expect(m.heightPx).toBeLessThan(m.frameH);
      }
      expect(metas[1].heightPx).toBeGreaterThan(metas[0].heightPx);
      expect(metas[2].heightPx).toBeGreaterThan(metas[1].heightPx);
      expect(SpriteGenerator.getPetSheetMeta(id, 2)?.key).toBe(`beast_${id}_e2`);
    }
  });

  it('keeps the legacy decor_pet_<id> portraits registered', () => {
    for (const id of PET_IDS) expect(SpriteGenerator.hasDecoration(`decor_pet_${id}`)).toBe(true);
  });
});

describe('Ember Tower decorations', () => {
  it('registers the tower, portal, hearthstone and every building stage', () => {
    const keys = [
      'decor_tower_main',
      'decor_tower_portal',
      'decor_tower_hearthstone',
      ...TOWER_BUILDING_IDS.flatMap(id => [0, 1, 2].map(st => towerBuildingKey(id, st))),
    ];
    expect(TOWER_BUILDING_IDS).toEqual(['herb_garden', 'pet_house', 'gem_workshop', 'training_ground', 'altar', 'warehouse']);
    expect(new Set(TOWER_PROP_DRAWERS.map(d => d.key)).size).toBe(TOWER_PROP_DRAWERS.length);
    for (const key of keys) {
      expect(SpriteGenerator.hasDecoration(key)).toBe(true);
      const meta = SpriteGenerator.getDecorMeta(key);
      expect(meta).not.toBeNull();
      expect(meta!.anchorY).toBeGreaterThan(0.3);
      expect(meta!.anchorY).toBeLessThan(1);
    }
    expect(towerBuildingKey('altar', 7)).toBe('decor_tower_altar_2');
  });

  it('keeps decoration textures within the texture limit', () => {
    for (const d of TOWER_PROP_DRAWERS) {
      expect(d.frameW * 3 * Math.max(1, d.totalFrames)).toBeLessThanOrEqual(MAX_SHEET_DIMENSION);
      expect(d.frameH * 3).toBeLessThanOrEqual(MAX_SHEET_DIMENSION);
    }
  });
});
