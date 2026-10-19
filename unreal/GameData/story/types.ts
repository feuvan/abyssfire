/**
 * Remake story types: the web story schema (src/data/story/types.ts), re-exported, plus the remake-only step fields
 * (implementation-plan.md §2.6). `story/script.ts` imports from here. See ../types.ts for the rules.
 */
import type * as Web from '@web/data/story/types';

export type * from '@web/data/story/types';

/** Monster actions a story `focus` step can play (K1 AnimAction additions; core enum strings are lowerCamel). */
export type RemakeStoryAnim = 'roar' | 'taunt';

export type CutsceneStep =
  | Exclude<Web.CutsceneStep, { kind: 'focus' }>
  /** [W1] `anim` plays on the focused monster (feature `story.focusAnim`). */
  | (Extract<Web.CutsceneStep, { kind: 'focus' }> & { anim?: RemakeStoryAnim });

export interface Cutscene extends Omit<Web.Cutscene, 'steps'> {
  steps: CutsceneStep[];
}

/**
 * [W1] `whileQuest` (feature `story.triggerWhileQuest`): a `monster_killed` trigger fires only while that quest is
 * accepted and not yet turned in, so a kill the M7 respawn rules allow before the quest does not spend its beat.
 */
export type StoryTrigger =
  | Exclude<Web.StoryTrigger, { on: 'monster_killed' }>
  | (Extract<Web.StoryTrigger, { on: 'monster_killed' }> & { whileQuest?: string });
