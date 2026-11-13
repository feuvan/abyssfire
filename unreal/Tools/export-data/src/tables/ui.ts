/**
 * ui_theme.json (UiKit colours, fonts, touch-control sizes) and terrain_styles.json (art inputs:
 * TERRAIN_THEMES ground / outcrop / palisade colours and ranks; `themeParents`, the remake's presentation theme ->
 * parent theme table).
 */
import { UI_COLORS, QUALITY_HEX, UI_FONT, UI_TITLE_FONT } from '../../../../../src/ui/UiKit';
import * as MobileMod from '../../../../../src/systems/MobileControlsSystem';
import { TERRAIN_THEMES } from '../../../../../src/graphics/terrain/TerrainStyles';
import { GAME_WIDTH, GAME_HEIGHT } from '../../../../../src/config';
import { assert, assertSource, plain, readSource, type TableResult } from '../util';
import { overrideEntries } from '../remake';
import { KIT_THEME_FILE, kitParentMismatches, kitThemeParents, themeParentProblems } from '../themeParents';

const exp = <T>(mod: unknown, name: string): T => {
  const v = (mod as Record<string, unknown>)[`__expose_${name}`];
  assert(v !== undefined, `exposed binding ${name} missing`);
  return v as T;
};

export function exportUi(): TableResult[] {
  const terrainThemes = overrideEntries('terrainStyles', ['terrain_styles.json'], TERRAIN_THEMES);
  // Presentation theme -> parent theme (the remake's; the web has none). Both ends are terrain styles, no chain loops,
  // at most MAX_THEME_CHAIN themes in a chain (themeParents.ts).
  const themeParents = overrideEntries<string>('themeParents', ['terrain_styles.json'], {});
  const where = 'GameData/world.ts: REMAKE_OVERRIDES.themeParents';
  const problems = themeParentProblems(themeParents, new Set(Object.keys(terrainThemes)), where);
  assert(problems.length === 0, problems.join('\n'));
  // The Blender kit keeps its own table (kit/theme.py `parent`): the art ships along that chain, the game looks up along
  // this one, so they are the same table.
  const kit = kitThemeParents(readSource(KIT_THEME_FILE));
  const mismatches = kitParentMismatches(themeParents, kit.parents);
  assert(mismatches.length === 0, `${where} differs from the art kit's theme parents:\n${mismatches.join('\n')}`);
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
