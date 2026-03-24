/**
 * Tests for the difficulty selector edge-case fix.
 *
 * Covers:
 * - deriveCompletedDifficulties: infers prerequisite difficulties from
 *   persisted difficulty when completedDifficulties is empty/missing.
 * - shouldShowDifficultySelector: shows selector when difficulty is non-normal
 *   even with empty completedDifficulties.
 * - Migrated saves with difficulty='nightmare'/'hell' but no completedDifficulties
 *   still see the difficulty selector and get completedDifficulties populated.
 */

import { describe, it, expect } from 'vitest';
import { DifficultySystem } from '../systems/DifficultySystem';
import type { Difficulty } from '../systems/DifficultySystem';

// =============================================================================
// deriveCompletedDifficulties
// =============================================================================

describe('DifficultySystem.deriveCompletedDifficulties', () => {
  it('returns existing completedDifficulties when non-empty', () => {
    const result = DifficultySystem.deriveCompletedDifficulties('nightmare', ['normal']);
    expect(result).toEqual(['normal']);
  });

  it('does not mutate the input array', () => {
    const input = ['normal'];
    const result = DifficultySystem.deriveCompletedDifficulties('nightmare', input);
    expect(result).not.toBe(input); // different reference
    expect(result).toEqual(['normal']);
  });

  it('infers ["normal"] when difficulty is nightmare and completedDifficulties is empty', () => {
    const result = DifficultySystem.deriveCompletedDifficulties('nightmare', []);
    expect(result).toEqual(['normal']);
  });

  it('infers ["normal", "nightmare"] when difficulty is hell and completedDifficulties is empty', () => {
    const result = DifficultySystem.deriveCompletedDifficulties('hell', []);
    expect(result).toEqual(['normal', 'nightmare']);
  });

  it('returns empty array when difficulty is normal and completedDifficulties is empty', () => {
    const result = DifficultySystem.deriveCompletedDifficulties('normal', []);
    expect(result).toEqual([]);
  });

  it('handles undefined completedDifficulties', () => {
    const result = DifficultySystem.deriveCompletedDifficulties('nightmare', undefined);
    expect(result).toEqual(['normal']);
  });

  it('handles null completedDifficulties', () => {
    const result = DifficultySystem.deriveCompletedDifficulties('hell', null);
    expect(result).toEqual(['normal', 'nightmare']);
  });

  it('infers correctly for normal with undefined', () => {
    const result = DifficultySystem.deriveCompletedDifficulties('normal', undefined);
    expect(result).toEqual([]);
  });

  it('preserves existing completedDifficulties over inference', () => {
    // Even if difficulty is hell, if completedDifficulties has data, trust it
    const result = DifficultySystem.deriveCompletedDifficulties('hell', ['normal', 'nightmare']);
    expect(result).toEqual(['normal', 'nightmare']);
  });
});

// =============================================================================
// shouldShowDifficultySelector
// =============================================================================

describe('DifficultySystem.shouldShowDifficultySelector', () => {
  it('returns false for normal difficulty with empty completedDifficulties', () => {
    expect(DifficultySystem.shouldShowDifficultySelector('normal', [])).toBe(false);
  });

  it('returns true for nightmare difficulty even with empty completedDifficulties', () => {
    expect(DifficultySystem.shouldShowDifficultySelector('nightmare', [])).toBe(true);
  });

  it('returns true for hell difficulty even with empty completedDifficulties', () => {
    expect(DifficultySystem.shouldShowDifficultySelector('hell', [])).toBe(true);
  });

  it('returns true when completedDifficulties has entries regardless of difficulty', () => {
    expect(DifficultySystem.shouldShowDifficultySelector('normal', ['normal'])).toBe(true);
  });

  it('returns true for nightmare with populated completedDifficulties', () => {
    expect(DifficultySystem.shouldShowDifficultySelector('nightmare', ['normal'])).toBe(true);
  });

  it('handles undefined difficulty', () => {
    expect(DifficultySystem.shouldShowDifficultySelector(undefined, [])).toBe(false);
  });

  it('handles undefined completedDifficulties', () => {
    expect(DifficultySystem.shouldShowDifficultySelector('hell', undefined)).toBe(true);
  });

  it('handles null completedDifficulties', () => {
    expect(DifficultySystem.shouldShowDifficultySelector('nightmare', null)).toBe(true);
  });

  it('returns false for normal with undefined completedDifficulties', () => {
    expect(DifficultySystem.shouldShowDifficultySelector('normal', undefined)).toBe(false);
  });
});

// =============================================================================
// Integration: Migrated save scenario
// =============================================================================

describe('Migrated save: difficulty selector and derived completedDifficulties', () => {
  it('save with difficulty=nightmare, empty completedDifficulties gets selector shown and completedDifficulties derived', () => {
    const save = {
      difficulty: 'nightmare' as Difficulty,
      completedDifficulties: [] as string[],
    };

    // shouldShowDifficultySelector triggers
    expect(DifficultySystem.shouldShowDifficultySelector(save.difficulty, save.completedDifficulties)).toBe(true);

    // Derive completedDifficulties
    save.completedDifficulties = DifficultySystem.deriveCompletedDifficulties(save.difficulty, save.completedDifficulties);
    expect(save.completedDifficulties).toEqual(['normal']);

    // getDifficultyStates now shows correct states
    const states = DifficultySystem.getDifficultyStates(save.completedDifficulties);
    expect(states.normal).toBe('completed');
    expect(states.nightmare).toBe('available');
    expect(states.hell).toBe('locked');
  });

  it('save with difficulty=hell, missing completedDifficulties gets selector shown and completedDifficulties derived', () => {
    const save = {
      difficulty: 'hell' as Difficulty,
      completedDifficulties: undefined as string[] | undefined,
    };

    // shouldShowDifficultySelector triggers
    expect(DifficultySystem.shouldShowDifficultySelector(save.difficulty, save.completedDifficulties)).toBe(true);

    // Derive completedDifficulties
    save.completedDifficulties = DifficultySystem.deriveCompletedDifficulties(save.difficulty, save.completedDifficulties);
    expect(save.completedDifficulties).toEqual(['normal', 'nightmare']);

    // getDifficultyStates now shows correct states
    const states = DifficultySystem.getDifficultyStates(save.completedDifficulties);
    expect(states.normal).toBe('completed');
    expect(states.nightmare).toBe('completed');
    expect(states.hell).toBe('available');
  });

  it('save with difficulty=normal, empty completedDifficulties skips selector', () => {
    const save = {
      difficulty: 'normal' as Difficulty,
      completedDifficulties: [] as string[],
    };

    expect(DifficultySystem.shouldShowDifficultySelector(save.difficulty, save.completedDifficulties)).toBe(false);
  });

  it('normal save with completedDifficulties=["normal"] shows selector', () => {
    const save = {
      difficulty: 'normal' as Difficulty,
      completedDifficulties: ['normal'],
    };

    expect(DifficultySystem.shouldShowDifficultySelector(save.difficulty, save.completedDifficulties)).toBe(true);
  });
});
