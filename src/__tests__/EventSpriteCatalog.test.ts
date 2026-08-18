import { describe, expect, it } from 'vitest';
import { AllQuests } from '../data/quests/all_quests';
import { ZONE_EVENT_DATA } from '../systems/RandomEventSystem';
import { SpriteGenerator } from '../graphics/SpriteGenerator';
import { EVENT_NPC_DRAWERS } from '../graphics/sprites/npcs/EventNPCs';
import { EVENT_PROP_DRAWERS } from '../graphics/sprites/decorations/EventProps';
import { PetSpriteDrawers } from '../graphics/sprites/decorations/Pets';

describe('event sprite catalog', () => {
  it('keeps every event NPC drawer uniquely addressable', () => {
    const keys = EVENT_NPC_DRAWERS.map(drawer => drawer.key);
    expect(new Set(keys).size).toBe(keys.length);
    for (const key of keys) {
      expect(key.startsWith('npc_')).toBe(true);
      expect(SpriteGenerator.hasNPCSprite(key)).toBe(true);
    }
  });

  it('keeps every event prop drawer uniquely addressable', () => {
    const keys = EVENT_PROP_DRAWERS.map(drawer => drawer.key);
    expect(new Set(keys).size).toBe(keys.length);
    for (const key of keys) {
      expect(key.startsWith('decor_')).toBe(true);
      expect(SpriteGenerator.hasDecoration(key)).toBe(true);
    }
  });

  it('gives every supported pet its own decoration sprite', () => {
    const keys = PetSpriteDrawers.map(drawer => drawer.key);
    expect(new Set(keys).size).toBe(keys.length);
    expect(keys).toHaveLength(8);
    for (const key of keys) expect(SpriteGenerator.hasDecoration(key)).toBe(true);
  });

  it('maps each zone rescue and puzzle event to dedicated visuals', () => {
    const zones = Object.values(ZONE_EVENT_DATA);
    expect(zones).toHaveLength(5);
    expect(new Set(zones.map(zone => zone.rescueNpcSpriteKey)).size).toBe(zones.length);
    expect(new Set(zones.map(zone => zone.puzzleSpriteKey)).size).toBe(zones.length);
    for (const zone of zones) {
      expect(SpriteGenerator.hasNPCSprite(zone.rescueNpcSpriteKey)).toBe(true);
      expect(SpriteGenerator.hasDecoration(zone.puzzleSpriteKey)).toBe(true);
    }
  });

  it('maps escort and defend quest entities to registered visuals', () => {
    const escortQuests = AllQuests.filter(quest => quest.type === 'escort');
    const defendQuests = AllQuests.filter(quest => quest.type === 'defend');
    expect(escortQuests.length).toBeGreaterThan(0);
    expect(defendQuests.length).toBeGreaterThan(0);
    for (const quest of escortQuests) {
      expect(quest.escortNpc).toBeDefined();
      expect(SpriteGenerator.hasNPCSprite(quest.escortNpc!.spriteKey)).toBe(true);
    }
    for (const quest of defendQuests) {
      expect(quest.defendTarget).toBeDefined();
      expect(SpriteGenerator.hasDecoration(quest.defendTarget!.spriteKey)).toBe(true);
    }
  });
});
