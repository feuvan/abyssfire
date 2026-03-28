/**
 * English locale.
 * Final fallback in the fallback chain: zh-TW → zh-CN → en → key path.
 */
import type { LocaleData } from '../types';

const en: LocaleData = {
  // ─── Menu Scene ───
  'menu.newGame': 'New Journey',
  'menu.continue': 'Continue - {class} Lv.{level}',
  'menu.continueSubtitle': 'Continue your adventure',
  'menu.help': 'Controls',
  'menu.ost': 'Soundtrack',
  'menu.credits': 'Credits',
  'menu.language': 'Language',
  'menu.back': '← Back',
  'menu.classSelect.title': 'Choose a Class',
  'menu.classSelect.confirm': 'Start Adventure',

  // ─── Test-only keys (for fallback testing) ───
  'test.enOnly': 'This key only exists in English',
};

export default en;
