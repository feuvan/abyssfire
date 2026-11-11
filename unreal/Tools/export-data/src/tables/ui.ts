/**
 * ui_theme.json (UiKit colours, fonts, touch-control sizes) and terrain_styles.json (art inputs:
 * TERRAIN_THEMES ground / outcrop / palisade colours and ranks; `themeParents`, the remake's presentation theme ->
 * parent theme table).
 */
import { UI_COLORS, QUALITY_HEX, UI_FONT, UI_TITLE_FONT } from '../../../../../src/ui/UiKit';
import * as MobileMod from '../../../../../src/systems/MobileControlsSystem';
import { TERRAIN_THEMES } from '../../../../../src/graphics/terrain/TerrainStyles';
import { GAME_WIDTH, GAME_HEIGHT } from '../../../../../src/config';
import { assert, assertSource, plain, type TableResult } from '../util';
import { overrideEntries } from '../remake';

const exp = <T>(mod: unknown, name: string): T => {
  const v = (mod as Record<string, unknown>)[`__expose_${name}`];
  assert(v !== undefined, `exposed binding ${name} missing`);
  return v as T;
};

export function exportUi(): TableResult[] {
  const terrainThemes = overrideEntries('terrainStyles', ['terrain_styles.json'], TERRAIN_THEMES);
  // Presentation theme -> parent theme (the remake's; the web has none). Both ends are terrain styles, no chain loops.
  const themeParents = overrideEntries<string>('themeParents', ['terrain_styles.json'], {});
  for (const [theme, parent] of Object.entries(themeParents)) {
    const where = `GameData/world.ts: REMAKE_OVERRIDES.themeParents.${theme}`;
    assert(typeof parent === 'string' && parent !== '', `${where}: the parent must be a theme name`);
    assert(theme in terrainThemes, `${where}: '${theme}' is not a terrain style (terrain_styles.json themes)`);
    assert(parent in terrainThemes, `${where}: parent '${parent}' is not a terrain style (terrain_styles.json themes)`);
    const seen = new Set<string>([theme]);
    for (let t: string | undefined = parent; t !== undefined; t = themeParents[t]) {
      assert(!seen.has(t), `${where}: the parent chain loops at '${t}'`);
      seen.add(t);
    }
  }
  assertSource('src/config.ts', "backgroundColor: '#0f0f1a',");
  assertSource('src/i18n/index.ts', "const DEFAULT_LOCALE: LocaleId = 'zh-CN';", "const SUPPORTED_LOCALES: LocaleId[] = ['zh-CN', 'zh-TW', 'en'];");
  return [
    {
      file: 'ui_theme.json',
      source: ['src/ui/UiKit.ts', 'src/systems/MobileControlsSystem.ts', 'src/config.ts', 'src/i18n/index.ts'],
      data: {
        display: { designWidth: GAME_WIDTH, designHeight: GAME_HEIGHT, background: '#0f0f1a', defaultLocale: 'zh-CN', locales: ['zh-CN', 'zh-TW', 'en'] },
        fonts: { body: UI_FONT, title: UI_TITLE_FONT },
        colors: plain(UI_COLORS),
        qualityHex: plain(QUALITY_HEX),
        touch: {
          cssPx: plain(exp<Record<string, number>>(MobileMod, 'CSS')),
          scaleClamp: [exp<number>(MobileMod, 'MIN_K'), exp<number>(MobileMod, 'MAX_K')],
          pointers: exp<number>(MobileMod, 'TOUCH_POINTERS'),
          minTouchTargetPt: 44,
        },
      },
      counts: { colors: Object.keys(UI_COLORS).length },
    },
    {
      file: 'terrain_styles.json',
      source: ['src/graphics/terrain/TerrainStyles.ts'],
      data: { themes: plain(terrainThemes), themeParents: plain(themeParents) },
      counts: { themes: Object.keys(terrainThemes).length, themeParents: Object.keys(themeParents).length },
    },
  ];
}
