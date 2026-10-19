/**
 * Remake strings, zh-CN (the primary locale). Merged by the exporter after the web table and PORT_STRINGS: a key here
 * replaces the web / port string, a new key is appended (unreal/Tools/export-data/src/tables/i18n.ts). zh-TW is converted
 * from the merged zh-CN.
 *
 * Every key here must also be in ./en.ts (the export fails otherwise). The table is split per area; each part holds
 * [zh-CN, en] pairs and is spread here and in ./en.ts in the same fixed order.
 */
import { CH1_NPC_STRINGS } from './ch1Npcs';
import { CH1_QUEST_STRINGS } from './ch1Quests';
import { CH1_STORY_STRINGS } from './ch1Story';
import { CH1_WORLD_STRINGS, localeColumn } from './ch1World';

const zhCN: Readonly<Record<string, string>> = {
  ...localeColumn(CH1_WORLD_STRINGS, 0),
  ...localeColumn(CH1_QUEST_STRINGS, 0),
  ...localeColumn(CH1_NPC_STRINGS, 0),
  ...localeColumn(CH1_STORY_STRINGS, 0),
};

export default zhCN;
