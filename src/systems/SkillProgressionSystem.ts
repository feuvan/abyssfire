import type { SkillDefinition } from '../data/types';

export type SkillInvestmentBlockReason =
  | 'ready'
  | 'no_points'
  | 'maxed'
  | 'player_level'
  | 'tree_points'
  | 'previous_tier';

export interface SkillInvestmentState {
  canInvest: boolean;
  reason: SkillInvestmentBlockReason;
  requiredPlayerLevel: number;
  requiredTreePoints: number;
  investedTreePoints: number;
}

export interface SkillInvestmentResult {
  invested: boolean;
  freeSkillPoints: number;
  skillLevels: Map<string, number>;
  state: SkillInvestmentState;
}

const TIER_PLAYER_LEVEL: Record<number, number> = {
  1: 1,
  2: 6,
  3: 12,
};

const TIER_TREE_POINTS: Record<number, number> = {
  1: 0,
  2: 4,
  3: 9,
};

function getSkillLevel(
  skillLevels: Map<string, number> | Record<string, number>,
  skillId: string,
): number {
  return skillLevels instanceof Map
    ? (skillLevels.get(skillId) ?? 0)
    : (skillLevels[skillId] ?? 0);
}

/** New characters learn every tier-one foundation skill, higher tiers start locked. */
export function getStarterSkillLevels(skills: SkillDefinition[]): Map<string, number> {
  return new Map(skills.map(skill => [skill.id, skill.tier === 1 ? 1 : 0]));
}

export function getLearnedSkillLoadout(
  skills: SkillDefinition[],
  skillLevels: Map<string, number> | Record<string, number>,
  limit = 6,
): SkillDefinition[] {
  return skills
    .filter(skill => getSkillLevel(skillLevels, skill.id) > 0)
    .slice(0, Math.max(0, limit));
}

export function getTreeInvestedPoints(
  treeId: string,
  skills: SkillDefinition[],
  skillLevels: Map<string, number> | Record<string, number>,
): number {
  return skills
    .filter(skill => skill.tree === treeId)
    .reduce((total, skill) => total + Math.max(0, getSkillLevel(skillLevels, skill.id)), 0);
}

export function getSkillInvestmentState(
  skill: SkillDefinition,
  allSkills: SkillDefinition[],
  skillLevels: Map<string, number> | Record<string, number>,
  playerLevel: number,
  freeSkillPoints: number,
): SkillInvestmentState {
  const currentLevel = Math.max(0, getSkillLevel(skillLevels, skill.id));
  const requiredPlayerLevel = TIER_PLAYER_LEVEL[skill.tier] ?? 1 + (skill.tier - 1) * 6;
  const requiredTreePoints = TIER_TREE_POINTS[skill.tier] ?? Math.max(0, (skill.tier - 1) * 5);
  const investedTreePoints = getTreeInvestedPoints(skill.tree, allSkills, skillLevels);

  const state = (reason: SkillInvestmentBlockReason): SkillInvestmentState => ({
    canInvest: reason === 'ready',
    reason,
    requiredPlayerLevel,
    requiredTreePoints,
    investedTreePoints,
  });

  if (currentLevel >= skill.maxLevel) return state('maxed');
  if (freeSkillPoints <= 0) return state('no_points');
  if (playerLevel < requiredPlayerLevel) return state('player_level');
  if (investedTreePoints < requiredTreePoints) return state('tree_points');

  if (skill.tier > 1) {
    const hasPreviousTier = allSkills.some(candidate =>
      candidate.tree === skill.tree
      && candidate.tier === skill.tier - 1
      && getSkillLevel(skillLevels, candidate.id) > 0
    );
    if (!hasPreviousTier) return state('previous_tier');
  }

  return state('ready');
}

export function investSkillPoint(
  skill: SkillDefinition,
  allSkills: SkillDefinition[],
  skillLevels: Map<string, number>,
  playerLevel: number,
  freeSkillPoints: number,
): SkillInvestmentResult {
  const state = getSkillInvestmentState(
    skill,
    allSkills,
    skillLevels,
    playerLevel,
    freeSkillPoints,
  );
  const nextLevels = new Map(skillLevels);
  if (!state.canInvest) {
    return {
      invested: false,
      freeSkillPoints,
      skillLevels: nextLevels,
      state,
    };
  }

  nextLevels.set(skill.id, (nextLevels.get(skill.id) ?? 0) + 1);
  return {
    invested: true,
    freeSkillPoints: freeSkillPoints - 1,
    skillLevels: nextLevels,
    state,
  };
}
