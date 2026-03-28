import { EventBus, GameEvents } from '../utils/EventBus';
import type { AchievementDefinition } from '../data/types';
import { t } from '../i18n';

const ACHIEVEMENTS: AchievementDefinition[] = [
  { id: 'ach_first_kill', name: '初出茅庐', description: '击杀第一个怪物', type: 'kill', required: 1, title: '新手冒险者' },
  { id: 'ach_kill_100', name: '百人斩', description: '击杀100个怪物', type: 'kill', required: 100, reward: { stat: 'damage', value: 2 } },
  { id: 'ach_kill_500', name: '屠戮者', description: '击杀500个怪物', type: 'kill', required: 500, reward: { stat: 'damage', value: 5 }, title: '屠戮者' },
  { id: 'ach_kill_slime', name: '史莱姆克星', description: '击杀50个史莱姆', type: 'kill', targetId: 'slime_green', required: 50, reward: { stat: 'lck', value: 2 } },
  { id: 'ach_kill_goblin_chief', name: '哥布林终结者', description: '击杀哥布林首领', type: 'kill', targetId: 'goblin_chief', required: 1, title: '哥布林终结者' },
  { id: 'ach_kill_demon_lord', name: '恶魔猎人', description: '击杀深渊魔王', type: 'kill', targetId: 'demon_lord', required: 1, reward: { stat: 'str', value: 10 }, title: '深渊征服者' },
  { id: 'ach_level_10', name: '成长之路', description: '达到10级', type: 'level', required: 10 },
  { id: 'ach_level_25', name: '勇者', description: '达到25级', type: 'level', required: 25, title: '勇者' },
  { id: 'ach_level_50', name: '传说', description: '达到50级', type: 'level', required: 50, reward: { stat: 'str', value: 5 }, title: '传说' },
  { id: 'ach_explore_all', name: '探索者', description: '探索所有区域', type: 'explore', required: 5, reward: { stat: 'lck', value: 5 }, title: '大陆探索者' },
  { id: 'ach_quest_10', name: '任务达人', description: '完成10个任务', type: 'quest', required: 10, reward: { stat: 'lck', value: 3 } },
  { id: 'ach_collect_legendary', name: '传奇收藏家', description: '收集一件传奇装备', type: 'collect', required: 1, title: '传奇收藏家' },
];

export class AchievementSystem {
  progress: Record<string, number> = {};
  unlocked: Set<string> = new Set();

  update(type: string, targetId?: string, amount = 1): void {
    // Collect unique progress keys to increment (avoid double-counting when
    // multiple achievements share the same key, e.g. kill-100 and kill-500).
    const keysToIncrement = new Set<string>();
    for (const ach of ACHIEVEMENTS) {
      if (this.unlocked.has(ach.id)) continue;
      if (ach.type !== type) continue;
      if (ach.targetId && ach.targetId !== targetId) continue;
      const key = ach.targetId ? `${ach.type}:${ach.targetId}` : ach.type;
      keysToIncrement.add(key);
    }

    // Increment each unique key exactly once
    for (const key of keysToIncrement) {
      this.progress[key] = (this.progress[key] ?? 0) + amount;
    }

    // Check all matching achievements against updated progress
    for (const ach of ACHIEVEMENTS) {
      if (this.unlocked.has(ach.id)) continue;
      if (ach.type !== type) continue;
      if (ach.targetId && ach.targetId !== targetId) continue;
      const key = ach.targetId ? `${ach.type}:${ach.targetId}` : ach.type;
      if (this.progress[key] >= ach.required) {
        this.unlock(ach);
      }
    }
  }

  checkLevel(level: number): void {
    for (const ach of ACHIEVEMENTS) {
      if (this.unlocked.has(ach.id)) continue;
      if (ach.type !== 'level') continue;
      if (level >= ach.required) {
        this.unlock(ach);
      }
    }
  }

  private unlock(ach: AchievementDefinition): void {
    this.unlocked.add(ach.id);
    EventBus.emit(GameEvents.ACHIEVEMENT_UNLOCKED, { achievement: ach });
    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: ach.title
        ? t('sys.achievement.unlockedWithTitle', { name: ach.name, title: ach.title })
        : t('sys.achievement.unlocked', { name: ach.name }),
      type: 'system',
    });
  }

  getBonuses(): Record<string, number> {
    const bonuses: Record<string, number> = {};
    for (const ach of ACHIEVEMENTS) {
      if (!this.unlocked.has(ach.id) || !ach.reward) continue;
      bonuses[ach.reward.stat] = (bonuses[ach.reward.stat] ?? 0) + ach.reward.value;
    }
    return bonuses;
  }

  getAll(): (AchievementDefinition & { current: number; isUnlocked: boolean })[] {
    return ACHIEVEMENTS.map(ach => {
      const key = ach.targetId ? `${ach.type}:${ach.targetId}` : ach.type;
      return {
        ...ach,
        current: this.progress[key] ?? 0,
        isUnlocked: this.unlocked.has(ach.id),
      };
    });
  }

  getUnlockedData(): Record<string, number> {
    const data: Record<string, number> = {};
    for (const key of this.unlocked) data[key] = 1;
    return { ...this.progress, ...data };
  }

  loadData(data: Record<string, number>): void {
    this.progress = {};
    this.unlocked.clear();
    for (const [key, value] of Object.entries(data)) {
      if (ACHIEVEMENTS.find(a => a.id === key)) {
        this.unlocked.add(key);
      } else {
        this.progress[key] = value;
      }
    }
  }
}
