/**
 * DifficultySystem — Manages difficulty unlock flow and monster scaling.
 *
 * Difficulty progression:
 *   Normal → Nightmare (unlocked by defeating demon_lord in Abyss Rift on Normal)
 *   Nightmare → Hell (unlocked by defeating demon_lord in Abyss Rift on Nightmare)
 *
 * Scaling multipliers:
 *   Normal:    1.0x HP, 1.0x damage, 1.0x defense, 1.0x exp
 *   Nightmare: 1.5x HP, 1.5x damage, 1.3x defense, 2.0x exp
 *   Hell:      2.0x HP, 2.0x damage, 1.6x defense, 3.0x exp
 *
 * Applied to overworld monsters and random dungeons (dungeon also layers
 * depth-based scaling on top of difficulty scaling).
 */

import type { MonsterDefinition } from '../data/types';

export type Difficulty = 'normal' | 'nightmare' | 'hell';

/** Ordered list of difficulties for unlock progression. */
export const DIFFICULTY_ORDER: readonly Difficulty[] = ['normal', 'nightmare', 'hell'] as const;

/** Chinese labels for difficulty names. */
export const DIFFICULTY_LABELS: Record<Difficulty, string> = {
  normal: '普通',
  nightmare: '噩梦',
  hell: '地狱',
};

/** Chinese labels for unlock messages. */
export const DIFFICULTY_UNLOCK_MESSAGES: Record<Difficulty, string> = {
  normal: '',
  nightmare: '噩梦难度已解锁',
  hell: '地狱难度已解锁',
};

/** Monster stat multipliers per difficulty. */
export interface DifficultyMultipliers {
  hp: number;
  damage: number;
  defense: number;
  exp: number;
}

export class DifficultySystem {
  // ---------------------------------------------------------------------------
  // Multipliers
  // ---------------------------------------------------------------------------

  /** Get stat multipliers for a given difficulty. */
  static getMultipliers(difficulty: Difficulty): DifficultyMultipliers {
    switch (difficulty) {
      case 'nightmare':
        return { hp: 1.5, damage: 1.5, defense: 1.3, exp: 2.0 };
      case 'hell':
        return { hp: 2.0, damage: 2.0, defense: 1.6, exp: 3.0 };
      default:
        return { hp: 1.0, damage: 1.0, defense: 1.0, exp: 1.0 };
    }
  }

  // ---------------------------------------------------------------------------
  // Unlock logic
  // ---------------------------------------------------------------------------

  /**
   * Check whether killing demon_lord in Abyss Rift on the current difficulty
   * should mark the current difficulty as completed.
   */
  static shouldMarkCompleted(
    monsterId: string,
    zoneId: string,
    currentDifficulty: Difficulty,
    completedDifficulties: string[],
  ): boolean {
    if (monsterId !== 'demon_lord') return false;
    if (zoneId !== 'abyss_rift') return false;
    // Don't mark if already completed
    if (completedDifficulties.includes(currentDifficulty)) return false;
    return true;
  }

  /**
   * After marking a difficulty as completed, return the next difficulty that
   * should be unlocked (or null if already at max).
   */
  static getNewlyUnlockedDifficulty(completedDifficulties: string[]): Difficulty | null {
    if (completedDifficulties.includes('normal') && !completedDifficulties.includes('nightmare')) {
      return 'nightmare';
    }
    if (completedDifficulties.includes('nightmare') && !completedDifficulties.includes('hell')) {
      return 'hell';
    }
    return null;
  }

  /**
   * Derive completedDifficulties from the persisted difficulty when the array
   * is empty or missing. Migrated saves may have `difficulty: 'hell'` but
   * `completedDifficulties: []`. In that case we infer the prerequisite
   * difficulties must have been completed.
   *
   * Returns a new array (never mutates the input).
   */
  static deriveCompletedDifficulties(
    difficulty: Difficulty,
    completedDifficulties: string[] | undefined | null,
  ): string[] {
    const existing = Array.isArray(completedDifficulties) ? [...completedDifficulties] : [];
    if (existing.length > 0) return existing;

    // Infer from persisted difficulty
    if (difficulty === 'nightmare') {
      return ['normal'];
    }
    if (difficulty === 'hell') {
      return ['normal', 'nightmare'];
    }
    return existing;
  }

  /**
   * Determine whether the difficulty selector should be shown for a save.
   * Shows when the save has a non-normal difficulty or has completed difficulties.
   */
  static shouldShowDifficultySelector(
    difficulty: string | undefined,
    completedDifficulties: string[] | undefined | null,
  ): boolean {
    if (difficulty && difficulty !== 'normal') return true;
    if (Array.isArray(completedDifficulties) && completedDifficulties.length > 0) return true;
    return false;
  }

  /**
   * Get the availability state for each difficulty based on completed difficulties.
   * 'completed' — difficulty cleared (checkmark ✓)
   * 'available' — selectable but not yet cleared
   * 'locked'    — prerequisites not met
   */
  static getDifficultyStates(
    completedDifficulties: string[],
  ): Record<Difficulty, 'completed' | 'available' | 'locked'> {
    const states: Record<Difficulty, 'completed' | 'available' | 'locked'> = {
      normal: 'available',
      nightmare: 'locked',
      hell: 'locked',
    };

    // Normal is always at least available
    if (completedDifficulties.includes('normal')) {
      states.normal = 'completed';
      states.nightmare = 'available';
    }
    if (completedDifficulties.includes('nightmare')) {
      states.nightmare = 'completed';
      states.hell = 'available';
    }
    if (completedDifficulties.includes('hell')) {
      states.hell = 'completed';
    }

    return states;
  }

  /**
   * Check whether a difficulty is selectable (available or completed).
   */
  static isSelectable(
    difficulty: Difficulty,
    completedDifficulties: string[],
  ): boolean {
    const states = DifficultySystem.getDifficultyStates(completedDifficulties);
    return states[difficulty] !== 'locked';
  }

  // ---------------------------------------------------------------------------
  // Monster scaling
  // ---------------------------------------------------------------------------

  /**
   * Apply difficulty-based scaling to a monster definition (overworld).
   * Returns a new MonsterDefinition — does not mutate the original.
   */
  static scaleMonster(baseDef: MonsterDefinition, difficulty: Difficulty): MonsterDefinition {
    if (difficulty === 'normal') return baseDef;
    const mult = DifficultySystem.getMultipliers(difficulty);
    return {
      ...baseDef,
      hp: Math.round(baseDef.hp * mult.hp),
      damage: Math.round(baseDef.damage * mult.damage),
      defense: Math.round(baseDef.defense * mult.defense),
      expReward: Math.round(baseDef.expReward * mult.exp),
      goldReward: [
        Math.round(baseDef.goldReward[0] * mult.exp),
        Math.round(baseDef.goldReward[1] * mult.exp),
      ],
    };
  }
}
