/**
 * i18n_zh-CN.json, i18n_en.json, i18n_zh-TW.json — the complete flat string tables.
 *
 * zh-CN.ts / en.ts already spread the sub-tables (story, questStory, abyssRun, homestead, pets), so their
 * default exports are the full tables the game's t() reads. zh-TW is generated exactly like
 * src/i18n/index.ts does at runtime (convertToTraditional over every zh-CN value). Key order = TS order.
 * Runtime fallback chain (t()): zh-TW → zh-CN → en → the key itself; `{name}` placeholders.
 */
import zhCN from '../../../../../src/i18n/locales/zh-CN';
import en from '../../../../../src/i18n/locales/en';
import { convertToTraditional } from '../../../../../src/i18n/converter';
import { assert, type TableResult } from '../util';

const SOURCES = ['src/i18n/locales/zh-CN.ts', 'src/i18n/locales/en.ts', 'src/i18n/locales/story.ts', 'src/i18n/locales/questStory.ts',
  'src/i18n/locales/abyssRun.ts', 'src/i18n/locales/homestead.ts', 'src/i18n/locales/pets.ts'];

/**
 * Port-only strings (decisions that add player-facing text the web never had). Merged after the web tables, so a web
 * key with the same name would win; the export fails instead (assert below). zh-TW is converted like every other key.
 */
const PORT_STRINGS: Record<string, { 'zh-CN': string; en: string; decision: string }> = {
  'zone.exit.sealedChapter2': { 'zh-CN': '第二章即将开放', en: 'Chapter 2 is coming soon', decision: 'W7' },
};

export function exportI18n(): TableResult[] {
  for (const k of Object.keys(PORT_STRINGS)) {
    assert(!(k in zhCN) && !(k in en), `PORT_STRINGS key ${k} now exists in the web tables — remove it from the exporter`);
  }
  const zh: Record<string, string> = { ...zhCN };
  const enAll: Record<string, string> = { ...en };
  for (const [k, v] of Object.entries(PORT_STRINGS)) {
    zh[k] = v['zh-CN'];
    enAll[k] = v.en;
  }
  const portKeys = Object.fromEntries(Object.entries(PORT_STRINGS).map(([k, v]) => [k, v.decision]));
  const zhTW: Record<string, string> = {};
  for (const [k, v] of Object.entries(zh)) zhTW[k] = convertToTraditional(v);
  for (const [name, table] of [['zh-CN', zhCN], ['en', en]] as const) {
    for (const [k, v] of Object.entries(table)) assert(typeof v === 'string', `${name}: ${k} is not a string`);
  }
  const zhKeys = new Set(Object.keys(zhCN));
  const enKeys = new Set(Object.keys(en));
  const missingInEn = [...zhKeys].filter(k => !enKeys.has(k));
  const missingInZh = [...enKeys].filter(k => !zhKeys.has(k));
  const table = (locale: string, strings: Record<string, string>, source: string[], extra: Record<string, unknown>): TableResult => ({
    file: `i18n_${locale}.json`,
    source,
    data: { locale, fallback: locale === 'zh-TW' ? ['zh-CN', 'en'] : locale === 'zh-CN' ? ['en'] : [], placeholder: '{name}', ...extra,
      keyCount: Object.keys(strings).length, strings },
    counts: { keys: Object.keys(strings).length },
  });
  return [
    table('zh-CN', zh, SOURCES.filter(s => !s.endsWith('/en.ts')),
      { missingInOtherLocale: missingInZh.length ? { keysOnlyInEn: missingInZh } : {}, portKeys }),
    table('en', enAll, SOURCES.filter(s => !s.endsWith('/zh-CN.ts')),
      { missingInOtherLocale: missingInEn.length ? { keysOnlyInZhCN: missingInEn } : {}, portKeys }),
    table('zh-TW', zhTW, [...SOURCES.filter(s => !s.endsWith('/en.ts')), 'src/i18n/converter.ts', 'src/i18n/index.ts'],
      { generatedFrom: 'zh-CN via convertToTraditional', portKeys }),
  ];
}
