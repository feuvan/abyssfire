/**
 * Remake strings, zh-CN (the primary locale). Merged by the exporter after the web table and PORT_STRINGS: a key here
 * replaces the web / port string, a new key is appended (unreal/Tools/export-data/src/tables/i18n.ts). zh-TW is converted
 * from the merged zh-CN.
 *
 * Every key here must also be in ./en.ts (the export fails otherwise). Split the table per area as it grows
 * (`ch1.ts`, `story.ts`, `affixes.ts`, ...) and spread the parts here in a fixed order.
 */
const zhCN: Readonly<Record<string, string>> = {};

export default zhCN;
