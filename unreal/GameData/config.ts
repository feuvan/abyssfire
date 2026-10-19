/**
 * Chapter configuration the exporter used to hard-code (implementation-plan.md §2.1 / §2.3): which monster is the
 * chapter's story boss (DECISIONS M7), which quest completes the chapter, the game's final boss and the sealed gate
 * (DECISIONS W7). The exporter checks every id against the exported content, so a stale value fails the export.
 *
 * Phase 0 kept today's (web) values; the content switch (C1) points them at the remake's ids: 1B part 1 (world data)
 * made Lord-Marshal Valdras the story boss (Docs/dark-remake/chapter1.md §11), 1B part 2 (narrative) the quests and the
 * finale: M4 `q_rf_marshal` kills him, M5 `q_rf_last_toll` completes the chapter and plays `cs_rf_finale`.
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
  /**
   * DECISIONS DR3 (saves): beats whose id the remake kept but whose content it rewrote. A save older than `version`
   * (its `contentVersion`) forgets them on load, so they play again (story.json `contentUpgrade`; the core's
   * ApplyContentVersion). `version` is the save content version that brought the rewrite (2: the dark remake).
   */
  readonly contentUpgrade?: { readonly version: number; readonly replayBeats: readonly string[] };
}

export const CHAPTER_CONFIG: ChapterConfig = {
  storyBoss: { monsterId: 'valdras', notAfterQuestTurnIn: 'q_rf_marshal' },
  chapterCompleteQuest: 'q_rf_last_toll',
  chapterFinaleCutscene: 'cs_rf_finale',
  sealedGate: { exitTo: 'twilight_forest', messageKey: 'zone.exit.sealedChapter2' },
  // The remade prologue (six slides) and the Red Field's chapter card keep their web beat ids.
  contentUpgrade: { version: 2, replayBeats: ['prologue', 'chapter_emerald_plains'] },
};
