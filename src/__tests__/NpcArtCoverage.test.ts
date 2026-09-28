import { describe, expect, it } from 'vitest';
import { NPCDefinitions } from '../data/npcs';
import { SpriteGenerator } from '../graphics/SpriteGenerator';

describe('npc art coverage', () => {
  it('every NPC has a drawn character (never the flat colour-block fallback)', () => {
    for (const def of Object.values(NPCDefinitions)) {
      const drawn = SpriteGenerator.hasNPCSprite(`npc_${def.spriteId ?? def.id}`) || SpriteGenerator.hasNPCSprite(`npc_${def.type}`);
      expect(drawn, `${def.id} (${def.name})`).toBe(true);
    }
  });
});
