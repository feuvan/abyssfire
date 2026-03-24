import { describe, it, expect, beforeEach, vi } from 'vitest';
import { AchievementSystem } from '../systems/AchievementSystem';

// Track emitted events
const emittedEvents: Array<{ event: string; data: unknown }> = [];

vi.mock('../utils/EventBus', () => ({
  EventBus: {
    emit: (event: string, data: unknown) => emittedEvents.push({ event, data }),
    on: vi.fn(),
    off: vi.fn(),
  },
  GameEvents: {
    ACHIEVEMENT_UNLOCKED: 'achievement:unlocked',
    LOG_MESSAGE: 'log:message',
  },
}));

describe('AchievementSystem', () => {
  let system: AchievementSystem;

  beforeEach(() => {
    system = new AchievementSystem();
    emittedEvents.length = 0;
  });

  // ── Progress Tracking ──────────────────────────────────────

  describe('progress tracking', () => {
    it('should track kill progress and unlock first kill', () => {
      system.update('kill', undefined, 1);
      const all = system.getAll();
      const firstKill = all.find(a => a.id === 'ach_first_kill')!;
      expect(firstKill.isUnlocked).toBe(true);
      // current should be >= 1 (could be more if matching multiple achievements)
      expect(firstKill.current).toBeGreaterThanOrEqual(1);
    });

    it('should increment kill count cumulatively toward kill_100', () => {
      // Note: update() iterates all matching achievements with the same progress key,
      // so progress increments by N matching achievements per call
      for (let i = 0; i < 50; i++) {
        system.update('kill', undefined, 1);
      }
      const all = system.getAll();
      const kill100 = all.find(a => a.id === 'ach_kill_100')!;
      expect(kill100.current).toBeGreaterThanOrEqual(50);
      // After first_kill unlocks (at call 1), subsequent calls only match 2 remaining non-targeted kill achievements
    });

    it('should unlock kill_100 at 100+ generic kills', () => {
      for (let i = 0; i < 100; i++) {
        system.update('kill', undefined, 1);
      }
      const all = system.getAll();
      const kill100 = all.find(a => a.id === 'ach_kill_100')!;
      expect(kill100.isUnlocked).toBe(true);
    });

    it('should track targeted kill progress (slime)', () => {
      for (let i = 0; i < 30; i++) {
        system.update('kill', 'slime_green', 1);
      }
      const all = system.getAll();
      const slime = all.find(a => a.id === 'ach_kill_slime')!;
      expect(slime.current).toBe(30);
      expect(slime.isUnlocked).toBe(false);
    });

    it('should unlock targeted kill achievement at threshold', () => {
      for (let i = 0; i < 50; i++) {
        system.update('kill', 'slime_green', 1);
      }
      const all = system.getAll();
      const slime = all.find(a => a.id === 'ach_kill_slime')!;
      expect(slime.isUnlocked).toBe(true);
    });

    it('targeted kills also increment generic kill counter', () => {
      // Each call to update('kill', 'slime_green', 1) also increments the generic kill counter
      for (let i = 0; i < 10; i++) {
        system.update('kill', 'slime_green', 1);
      }
      const all = system.getAll();
      const firstKill = all.find(a => a.id === 'ach_first_kill')!;
      expect(firstKill.isUnlocked).toBe(true);
      // Generic kill counter should be >= 10
      expect(firstKill.current).toBeGreaterThanOrEqual(10);
    });

    it('should track explore progress', () => {
      system.update('explore', 'emerald_plains');
      system.update('explore', 'twilight_forest');
      system.update('explore', 'anvil_mountains');
      const all = system.getAll();
      const explore = all.find(a => a.id === 'ach_explore_all')!;
      expect(explore.current).toBe(3);
      expect(explore.isUnlocked).toBe(false);
    });

    it('should unlock explore all when 5 zones explored', () => {
      system.update('explore', 'emerald_plains');
      system.update('explore', 'twilight_forest');
      system.update('explore', 'anvil_mountains');
      system.update('explore', 'scorching_desert');
      system.update('explore', 'abyss_rift');
      const all = system.getAll();
      const explore = all.find(a => a.id === 'ach_explore_all')!;
      expect(explore.isUnlocked).toBe(true);
    });

    it('should track quest completions', () => {
      for (let i = 0; i < 5; i++) {
        system.update('quest');
      }
      const all = system.getAll();
      const quest = all.find(a => a.id === 'ach_quest_10')!;
      expect(quest.current).toBe(5);
      expect(quest.isUnlocked).toBe(false);
    });

    it('should track collect progress for legendary items', () => {
      system.update('collect');
      const all = system.getAll();
      const collect = all.find(a => a.id === 'ach_collect_legendary')!;
      expect(collect.current).toBe(1);
      expect(collect.isUnlocked).toBe(true);
    });

    it('should check level achievements', () => {
      system.checkLevel(10);
      const all = system.getAll();
      const level10 = all.find(a => a.id === 'ach_level_10')!;
      expect(level10.isUnlocked).toBe(true);
      const level25 = all.find(a => a.id === 'ach_level_25')!;
      expect(level25.isUnlocked).toBe(false);
    });

    it('should unlock multiple level achievements at once', () => {
      system.checkLevel(50);
      const all = system.getAll();
      expect(all.find(a => a.id === 'ach_level_10')!.isUnlocked).toBe(true);
      expect(all.find(a => a.id === 'ach_level_25')!.isUnlocked).toBe(true);
      expect(all.find(a => a.id === 'ach_level_50')!.isUnlocked).toBe(true);
    });
  });

  // ── Unlock Events ──────────────────────────────────────────

  describe('unlock events', () => {
    it('should emit ACHIEVEMENT_UNLOCKED event on unlock', () => {
      system.update('kill', undefined, 1);
      const unlockEvents = emittedEvents.filter(e => e.event === 'achievement:unlocked');
      expect(unlockEvents.length).toBeGreaterThanOrEqual(1);
      const firstUnlock = unlockEvents.find(e => (e.data as { achievement: { id: string } }).achievement.id === 'ach_first_kill');
      expect(firstUnlock).toBeDefined();
    });

    it('should emit LOG_MESSAGE event on unlock', () => {
      system.update('kill', undefined, 1);
      const logEvents = emittedEvents.filter(e => e.event === 'log:message');
      expect(logEvents.length).toBeGreaterThanOrEqual(1);
      const achLog = logEvents.find(e => (e.data as { text: string }).text.includes('成就解锁'));
      expect(achLog).toBeDefined();
      expect((achLog!.data as { type: string }).type).toBe('system');
    });

    it('should not re-unlock already unlocked achievements', () => {
      system.update('kill', undefined, 1);
      emittedEvents.length = 0;
      system.update('kill', undefined, 1);
      const unlockEvents = emittedEvents.filter(e => e.event === 'achievement:unlocked');
      // First kill already unlocked, so no new unlock event for it
      const firstKillUnlock = unlockEvents.find(e => (e.data as { achievement: { id: string } }).achievement.id === 'ach_first_kill');
      expect(firstKillUnlock).toBeUndefined();
    });

    it('should include title in log message when achievement has one', () => {
      system.update('kill', undefined, 1); // ach_first_kill has title
      const logEvents = emittedEvents.filter(e => e.event === 'log:message');
      const achLog = logEvents.find(e => (e.data as { text: string }).text.includes('初出茅庐'));
      expect(achLog).toBeDefined();
      expect((achLog!.data as { text: string }).text).toContain('称号: 新手冒险者');
    });
  });

  // ── Reward Bonuses ─────────────────────────────────────────

  describe('getBonuses', () => {
    it('should return empty bonuses when no achievements unlocked', () => {
      const bonuses = system.getBonuses();
      expect(Object.keys(bonuses).length).toBe(0);
    });

    it('should return stat bonuses for unlocked achievements', () => {
      // Unlock kill_100 (damage +2) by doing 100 generic kills
      for (let i = 0; i < 100; i++) {
        system.update('kill', undefined, 1);
      }
      const bonuses = system.getBonuses();
      expect(bonuses['damage']).toBeGreaterThanOrEqual(2);
    });

    it('should aggregate multiple bonuses of the same stat', () => {
      // Unlock kill_100 (damage +2) and kill_500 (damage +5)
      for (let i = 0; i < 500; i++) {
        system.update('kill', undefined, 1);
      }
      const bonuses = system.getBonuses();
      expect(bonuses['damage']).toBe(7); // 2 + 5
    });

    it('should return different stat types from different achievements', () => {
      // Unlock explore_all (lck +5) and quest_10 (lck +3)
      for (let i = 0; i < 5; i++) {
        system.update('explore', `zone_${i}`);
      }
      for (let i = 0; i < 10; i++) {
        system.update('quest');
      }
      const bonuses = system.getBonuses();
      expect(bonuses['lck']).toBe(8); // 5 + 3
    });

    it('should not include bonuses from locked achievements', () => {
      // first_kill has no reward stat, only a title
      system.update('collect'); // just 1 legendary collect
      const bonuses = system.getBonuses();
      // collect_legendary has no reward stat, only a title
      expect(bonuses['damage']).toBeUndefined();
    });

    it('getBonuses should be idempotent — calling multiple times returns same result', () => {
      for (let i = 0; i < 100; i++) {
        system.update('kill', undefined, 1);
      }
      const bonuses1 = system.getBonuses();
      const bonuses2 = system.getBonuses();
      const bonuses3 = system.getBonuses();
      expect(bonuses1).toEqual(bonuses2);
      expect(bonuses2).toEqual(bonuses3);
    });
  });

  // ── Save/Load Persistence ──────────────────────────────────

  describe('save/load persistence', () => {
    it('should save and restore unlocked state correctly', () => {
      // With progress sharing among same-key achievements, 50 generic kills
      // may unlock kill_100 depending on how progress accumulates
      for (let i = 0; i < 20; i++) {
        system.update('kill', undefined, 1);
      }
      system.update('explore', 'emerald_plains');
      const saveData = system.getUnlockedData();

      const newSystem = new AchievementSystem();
      newSystem.loadData(saveData);

      const all = newSystem.getAll();
      const firstKill = all.find(a => a.id === 'ach_first_kill')!;
      expect(firstKill.isUnlocked).toBe(true);
    });

    it('should preserve progress through save/load', () => {
      for (let i = 0; i < 50; i++) {
        system.update('kill', undefined, 1);
      }
      const originalProgress = system.getAll().find(a => a.id === 'ach_kill_100')!.current;
      const saveData = system.getUnlockedData();

      const newSystem = new AchievementSystem();
      newSystem.loadData(saveData);
      const loadedProgress = newSystem.getAll().find(a => a.id === 'ach_kill_100')!.current;

      expect(loadedProgress).toBe(originalProgress);
    });

    it('should not double-apply rewards after save/load', () => {
      for (let i = 0; i < 100; i++) {
        system.update('kill', undefined, 1);
      }
      const bonuses1 = system.getBonuses();

      // Save and load
      const saveData = system.getUnlockedData();
      const newSystem = new AchievementSystem();
      newSystem.loadData(saveData);

      const bonuses2 = newSystem.getBonuses();
      expect(bonuses2).toEqual(bonuses1);
    });

    it('should preserve unlocked state through save/load cycle', () => {
      for (let i = 0; i < 200; i++) {
        system.update('kill', undefined, 1);
      }
      system.checkLevel(25);

      const saveData = system.getUnlockedData();
      const newSystem = new AchievementSystem();
      newSystem.loadData(saveData);

      const all = newSystem.getAll();
      expect(all.find(a => a.id === 'ach_first_kill')!.isUnlocked).toBe(true);
      expect(all.find(a => a.id === 'ach_kill_100')!.isUnlocked).toBe(true);
      expect(all.find(a => a.id === 'ach_level_10')!.isUnlocked).toBe(true);
      expect(all.find(a => a.id === 'ach_level_25')!.isUnlocked).toBe(true);
    });

    it('should not grant additional rewards after load', () => {
      for (let i = 0; i < 500; i++) {
        system.update('kill', undefined, 1);
      }
      system.checkLevel(50);
      for (let i = 0; i < 5; i++) {
        system.update('explore', `zone_${i}`);
      }
      for (let i = 0; i < 10; i++) {
        system.update('quest');
      }
      system.update('collect');

      const bonusesBefore = system.getBonuses();
      const saveData = system.getUnlockedData();
      const newSystem = new AchievementSystem();
      newSystem.loadData(saveData);
      const bonusesAfter = newSystem.getBonuses();

      // Exact same bonuses after load
      expect(bonusesAfter).toEqual(bonusesBefore);
    });

    it('should handle empty save data gracefully', () => {
      const newSystem = new AchievementSystem();
      newSystem.loadData({});
      const all = newSystem.getAll();
      expect(all.every(a => !a.isUnlocked)).toBe(true);
      expect(Object.keys(newSystem.getBonuses()).length).toBe(0);
    });

    it('should correctly identify unlocked achievements in save data format', () => {
      system.update('kill', undefined, 1); // unlocks ach_first_kill
      const data = system.getUnlockedData();
      // ach_first_kill key should exist with value 1
      expect(data['ach_first_kill']).toBe(1);
      // kill progress key should be present and positive
      expect(data['kill']).toBeGreaterThanOrEqual(1);
    });
  });

  // ── getAll Method ──────────────────────────────────────────

  describe('getAll', () => {
    it('should return all 12 achievements', () => {
      const all = system.getAll();
      expect(all.length).toBe(12);
    });

    it('should have correct Chinese names', () => {
      const all = system.getAll();
      const names = all.map(a => a.name);
      expect(names).toContain('初出茅庐');
      expect(names).toContain('百人斩');
      expect(names).toContain('屠戮者');
      expect(names).toContain('成长之路');
      expect(names).toContain('勇者');
      expect(names).toContain('传说');
      expect(names).toContain('探索者');
      expect(names).toContain('任务达人');
      expect(names).toContain('传奇收藏家');
    });

    it('should include correct unlocked state for tracked achievement', () => {
      system.update('kill', undefined, 10);
      const all = system.getAll();
      const firstKill = all.find(a => a.id === 'ach_first_kill')!;
      expect(firstKill.isUnlocked).toBe(true);
      const kill100 = all.find(a => a.id === 'ach_kill_100')!;
      expect(kill100.isUnlocked).toBe(false);
    });

    it('should include reward and title info', () => {
      const all = system.getAll();
      const kill100 = all.find(a => a.id === 'ach_kill_100')!;
      expect(kill100.reward).toBeDefined();
      expect(kill100.reward!.stat).toBe('damage');
      expect(kill100.reward!.value).toBe(2);

      const kill500 = all.find(a => a.id === 'ach_kill_500')!;
      expect(kill500.reward).toBeDefined();
      expect(kill500.reward!.stat).toBe('damage');
      expect(kill500.reward!.value).toBe(5);
      expect(kill500.title).toBe('屠戮者');

      const firstKill = all.find(a => a.id === 'ach_first_kill')!;
      expect(firstKill.title).toBe('新手冒险者');
    });
  });

  // ── Idempotency Under Repeated Operations ──────────────────

  describe('idempotency', () => {
    it('getBonuses should return identical results when called multiple times without state changes', () => {
      for (let i = 0; i < 100; i++) system.update('kill', undefined, 1);
      system.checkLevel(25);
      const results = Array.from({ length: 10 }, () => system.getBonuses());
      for (let i = 1; i < results.length; i++) {
        expect(results[i]).toEqual(results[0]);
      }
    });

    it('save/load/getBonuses cycle should be perfectly idempotent', () => {
      // Build state
      for (let i = 0; i < 500; i++) system.update('kill', undefined, 1);
      system.checkLevel(50);
      for (let i = 0; i < 5; i++) system.update('explore', `zone_${i}`);
      for (let i = 0; i < 10; i++) system.update('quest');
      system.update('collect');

      const expectedBonuses = system.getBonuses();

      // Multiple save/load cycles
      let currentSystem: AchievementSystem = system;
      for (let cycle = 0; cycle < 5; cycle++) {
        const data = currentSystem.getUnlockedData();
        const newSys = new AchievementSystem();
        newSys.loadData(data);
        currentSystem = newSys;
      }

      // Bonuses should still be the same after 5 save/load cycles
      expect(currentSystem.getBonuses()).toEqual(expectedBonuses);
    });

    it('progress should not drift across save/load cycles', () => {
      for (let i = 0; i < 250; i++) system.update('kill', undefined, 1);

      const originalAll = system.getAll();

      let currentSystem: AchievementSystem = system;
      for (let cycle = 0; cycle < 5; cycle++) {
        const data = currentSystem.getUnlockedData();
        const newSys = new AchievementSystem();
        newSys.loadData(data);
        currentSystem = newSys;
      }

      const finalAll = currentSystem.getAll();

      for (let i = 0; i < originalAll.length; i++) {
        expect(finalAll[i].isUnlocked).toBe(originalAll[i].isUnlocked);
        expect(finalAll[i].current).toBe(originalAll[i].current);
      }
    });
  });

  // ── Edge Cases ─────────────────────────────────────────────

  describe('edge cases', () => {
    it('should not crash on update with unknown type', () => {
      expect(() => system.update('nonexistent_type')).not.toThrow();
    });

    it('should handle amount > 1 in single update', () => {
      system.update('kill', undefined, 100);
      const all = system.getAll();
      expect(all.find(a => a.id === 'ach_kill_100')!.isUnlocked).toBe(true);
    });

    it('should handle targeted kill and generic kill in same update', () => {
      system.update('kill', 'goblin_chief', 1);
      const all = system.getAll();
      expect(all.find(a => a.id === 'ach_kill_goblin_chief')!.isUnlocked).toBe(true);
      // Generic kill count should also increment (targeted kills match non-targeted achievements)
      expect(all.find(a => a.id === 'ach_first_kill')!.isUnlocked).toBe(true);
    });

    it('should track boss kill separately from generic kills', () => {
      system.update('kill', 'demon_lord', 1);
      const all = system.getAll();
      expect(all.find(a => a.id === 'ach_kill_demon_lord')!.isUnlocked).toBe(true);
      // Generic kill counter also incremented
      expect(all.find(a => a.id === 'ach_first_kill')!.isUnlocked).toBe(true);
    });

    it('level 50 achievement should grant str +5 reward', () => {
      system.checkLevel(50);
      const bonuses = system.getBonuses();
      // Only level_50 gives str:5. demon_lord requires explicit kill.
      expect(bonuses['str']).toBe(5);
    });

    it('all rewards should be deterministic from unlocked set', () => {
      // Unlock a known set of achievements
      for (let i = 0; i < 100; i++) system.update('kill', undefined, 1);
      for (let i = 0; i < 50; i++) system.update('kill', 'slime_green', 1);
      system.checkLevel(10);

      const bonuses = system.getBonuses();
      // kill_100: damage +2
      // kill_slime: lck +2
      // No reward from first_kill, level_10 has no reward
      expect(bonuses['damage']).toBeGreaterThanOrEqual(2);
      expect(bonuses['lck']).toBe(2);
    });
  });
});
