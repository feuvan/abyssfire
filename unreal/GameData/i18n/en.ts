/**
 * Remake strings, en. Same keys as ./zh-CN.ts (the export fails otherwise); see that file for the merge rules.
 */
import { CH1_NPC_STRINGS } from './ch1Npcs';
import { CH1_QUEST_STRINGS } from './ch1Quests';
import { CH1_STORY_STRINGS } from './ch1Story';
import { CH1_WORLD_STRINGS, localeColumn } from './ch1World';

const en: Readonly<Record<string, string>> = {
  ...localeColumn(CH1_WORLD_STRINGS, 1),
  ...localeColumn(CH1_QUEST_STRINGS, 1),
  ...localeColumn(CH1_NPC_STRINGS, 1),
  ...localeColumn(CH1_STORY_STRINGS, 1),
};

export default en;
