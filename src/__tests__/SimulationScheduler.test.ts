import { describe, expect, it } from 'vitest';
import { SimulationScheduler } from '../systems/SimulationScheduler';

describe('SimulationScheduler', () => {
  it('runs each phase immediately and then at its own cadence', () => {
    const scheduler = new SimulationScheduler();
    expect(scheduler.due('ai', 0, 250)).toBe(true);
    expect(scheduler.due('ai', 249, 250)).toBe(false);
    expect(scheduler.due('ai', 250, 250)).toBe(true);
    expect(scheduler.due('quests', 100, 500)).toBe(true);
  });

  it('can be reset when a scene is restarted', () => {
    const scheduler = new SimulationScheduler();
    scheduler.due('ai', 100, 250);
    scheduler.reset();
    expect(scheduler.due('ai', 101, 250)).toBe(true);
  });
});
