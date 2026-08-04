import { describe, expect, it } from 'vitest';
import { GameSession } from '../game/GameSession';

describe('GameSession', () => {
  it('keeps persistent systems stable while replacing zone runtime systems', () => {
    const session = new GameSession();
    const inventory = session.inventory;
    const first = session.beginZone('emerald_plains', [1, 10], 9);
    const second = session.beginZone('twilight_forest', [10, 20], 8);

    expect(session.inventory).toBe(inventory);
    expect(second.combat).not.toBe(first.combat);
    expect(second.statusEffects).not.toBe(first.statusEffects);
  });
});
