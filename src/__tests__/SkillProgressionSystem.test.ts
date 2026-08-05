import { describe, expect, it } from 'vitest';
import { WarriorClass } from '../data/classes/warrior';
import {
  getSkillInvestmentState,
  getLearnedSkillLoadout,
  getStarterSkillLevels,
  getTreeInvestedPoints,
  investSkillPoint,
} from '../systems/SkillProgressionSystem';

const skill = (id: string) => WarriorClass.skills.find(entry => entry.id === id)!;

describe('SkillProgressionSystem', () => {
  it('starts new characters with tier-one skills only', () => {
    const levels = getStarterSkillLevels(WarriorClass.skills);

    expect(levels.get('slash')).toBe(1);
    expect(levels.get('shield_wall')).toBe(1);
    expect(levels.get('frenzy')).toBe(1);
    expect(levels.get('whirlwind')).toBe(0);
    expect(levels.get('war_stomp')).toBe(0);
  });

  it('builds a bounded loadout from learned skills in class order', () => {
    const levels = getStarterSkillLevels(WarriorClass.skills);
    levels.set('whirlwind', 1);

    expect(getLearnedSkillLoadout(WarriorClass.skills, levels, 3).map(entry => entry.id))
      .toEqual(['slash', 'whirlwind', 'shield_wall']);
    expect(getLearnedSkillLoadout(WarriorClass.skills, levels, 0)).toEqual([]);
  });

  it('counts all invested ranks in a branch', () => {
    const levels = new Map<string, number>([
      ['slash', 4],
      ['whirlwind', 2],
      ['shield_wall', 10],
    ]);

    expect(getTreeInvestedPoints('combat_master', WarriorClass.skills, levels)).toBe(6);
    expect(getTreeInvestedPoints('guardian', WarriorClass.skills, levels)).toBe(10);
  });

  it('allows increasing a learned tier-one skill when a point is available', () => {
    const levels = getStarterSkillLevels(WarriorClass.skills);
    const state = getSkillInvestmentState(
      skill('slash'),
      WarriorClass.skills,
      levels,
      1,
      1,
    );

    expect(state.canInvest).toBe(true);
    expect(state.reason).toBe('ready');
  });

  it('blocks investment without free points or at maximum rank', () => {
    const levels = getStarterSkillLevels(WarriorClass.skills);
    expect(getSkillInvestmentState(skill('slash'), WarriorClass.skills, levels, 10, 0).reason)
      .toBe('no_points');

    levels.set('slash', skill('slash').maxLevel);
    expect(getSkillInvestmentState(skill('slash'), WarriorClass.skills, levels, 99, 1).reason)
      .toBe('maxed');
  });

  it('gates tier two by player level and branch investment', () => {
    const levels = getStarterSkillLevels(WarriorClass.skills);

    expect(getSkillInvestmentState(skill('whirlwind'), WarriorClass.skills, levels, 5, 5).reason)
      .toBe('player_level');
    expect(getSkillInvestmentState(skill('whirlwind'), WarriorClass.skills, levels, 6, 5).reason)
      .toBe('tree_points');

    levels.set('slash', 4);
    expect(getSkillInvestmentState(skill('whirlwind'), WarriorClass.skills, levels, 6, 5).canInvest)
      .toBe(true);
  });

  it('requires an earlier-tier skill before tier three can unlock', () => {
    const levels = getStarterSkillLevels(WarriorClass.skills);
    levels.set('slash', 10);

    const blocked = getSkillInvestmentState(
      skill('war_stomp'),
      WarriorClass.skills,
      levels,
      12,
      1,
    );
    expect(blocked.reason).toBe('previous_tier');

    levels.set('whirlwind', 1);
    const ready = getSkillInvestmentState(
      skill('war_stomp'),
      WarriorClass.skills,
      levels,
      12,
      1,
    );
    expect(ready.canInvest).toBe(true);
  });

  it('invests exactly one rank without mutating the original map', () => {
    const levels = getStarterSkillLevels(WarriorClass.skills);
    const before = levels.get('slash');
    const result = investSkillPoint(skill('slash'), WarriorClass.skills, levels, 1, 1);

    expect(result.invested).toBe(true);
    expect(result.skillLevels.get('slash')).toBe((before ?? 0) + 1);
    expect(result.freeSkillPoints).toBe(0);
    expect(levels.get('slash')).toBe(before);
  });

  it('leaves state unchanged when investment is blocked', () => {
    const levels = getStarterSkillLevels(WarriorClass.skills);
    const result = investSkillPoint(skill('war_stomp'), WarriorClass.skills, levels, 1, 2);

    expect(result.invested).toBe(false);
    expect(result.skillLevels).toEqual(levels);
    expect(result.freeSkillPoints).toBe(2);
  });
});
