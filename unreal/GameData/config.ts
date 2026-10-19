/**
 * Chapter configuration the exporter used to hard-code (implementation-plan.md §2.1 / §2.3): which monster is the
 * chapter's story boss (DECISIONS M7), which quest completes the chapter, the game's final boss and the sealed gate
 * (DECISIONS W7). The exporter checks every id against the exported content, so a stale value fails the export.
 *
 * Phase 0: today's (web) values; the content switch (C1) points them at the remake's ids.
 */
export interface ChapterConfig {
  /** DECISIONS M7: spawns once per zone visit and not after `notAfterQuestTurnIn` (the quest that kills it). */
  readonly storyBoss: { readonly monsterId: string; readonly notAfterQuestTurnIn: string };
  /** The quest whose turn-in completes the chapter; its `quest_turned_in` trigger plays `chapterFinaleCutscene`. */
  readonly chapterCompleteQuest: string;
  readonly chapterFinaleCutscene: string;
  /** The game's final boss (story.json / monster_ai.json `finalBoss`). Unset: the web StoryDirector's FINAL_BOSS. */
  readonly finalBoss?: string;
  /** DECISIONS W7: the exit that stays sealed in milestone 1 and the toast it shows. */
  readonly sealedGate: { readonly exitTo: string; readonly messageKey: string };
}

export const CHAPTER_CONFIG: ChapterConfig = {
  storyBoss: { monsterId: 'goblin_chief', notAfterQuestTurnIn: 'q_find_goblin_chief' },
  chapterCompleteQuest: 'q_secure_plains',
  chapterFinaleCutscene: 'cs_ep_finale',
  sealedGate: { exitTo: 'twilight_forest', messageKey: 'zone.exit.sealedChapter2' },
};
