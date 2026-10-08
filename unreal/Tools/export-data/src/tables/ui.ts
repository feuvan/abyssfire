/**
 * ui_theme.json (UiKit colours, fonts, touch-control sizes) and terrain_styles.json (art inputs:
 * TERRAIN_THEMES ground / outcrop / palisade colours and ranks).
 */
import { UI_COLORS, QUALITY_HEX, UI_FONT, UI_TITLE_FONT } from '../../../../../src/ui/UiKit';
import * as MobileMod from '../../../../../src/systems/MobileControlsSystem';
import { TERRAIN_THEMES } from '../../../../../src/graphics/terrain/TerrainStyles';
import { GAME_WIDTH, GAME_HEIGHT } from '../../../../../src/config';
import { assert, assertSource, plain, type TableResult } from '../util';

const exp = <T>(mod: unknown, name: string): T => {
  const v = (mod as Record<string, unknown>)[`__expose_${name}`];
  assert(v !== undefined, `exposed binding ${name} missing`);
  return v as T;
};

export function exportUi(): TableResult[] {
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
      data: { themes: plain(TERRAIN_THEMES) },
      counts: { themes: Object.keys(TERRAIN_THEMES).length },
    },
  ];
}
