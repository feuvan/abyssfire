/**
 * Comprehensive unit tests for the i18n core module.
 * TDD: These tests are written before the implementation.
 */
import { describe, it, expect, beforeEach, vi } from 'vitest';

// We'll mock localStorage for Node testing environment
const localStorageMock = (() => {
  let store: Record<string, string> = {};
  return {
    getItem: (key: string) => store[key] ?? null,
    setItem: (key: string, value: string) => { store[key] = value; },
    removeItem: (key: string) => { delete store[key]; },
    clear: () => { store = {}; },
  };
})();

Object.defineProperty(globalThis, 'localStorage', { value: localStorageMock });

// Dynamic imports so the module picks up our localStorage mock
let t: (key: string, params?: Record<string, string | number>) => string;
let setLocale: (locale: string) => void;
let getLocale: () => string;
let getLocales: () => string[];

// EventBus + GameEvents
let EventBus: { on: (event: string, fn: (...args: unknown[]) => void) => void; removeAllListeners: (event?: string) => void };
let GameEvents: Record<string, string>;

// Converter
let convertToTraditional: (text: string) => string;

beforeEach(async () => {
  // Clear localStorage before each test
  localStorageMock.clear();

  // Reset module cache so each test starts fresh
  vi.resetModules();

  const eventBusModule = await import('../utils/EventBus');
  EventBus = eventBusModule.EventBus;
  GameEvents = eventBusModule.GameEvents;

  // Clear event listeners
  EventBus.removeAllListeners();

  const i18nModule = await import('../i18n/index');
  t = i18nModule.t;
  setLocale = i18nModule.setLocale;
  getLocale = i18nModule.getLocale;
  getLocales = i18nModule.getLocales;

  const converterModule = await import('../i18n/converter');
  convertToTraditional = converterModule.convertToTraditional;
});

describe('i18n Core Module', () => {
  describe('t() — basic key lookup', () => {
    it('returns correct zh-CN string for known key', () => {
      setLocale('zh-CN');
      const result = t('menu.newGame');
      expect(result).toBeTruthy();
      expect(typeof result).toBe('string');
      expect(result.length).toBeGreaterThan(0);
    });

    it('returns correct en string for known key', () => {
      setLocale('en');
      const result = t('menu.newGame');
      expect(result).toBeTruthy();
      expect(typeof result).toBe('string');
      // English string should not contain Chinese characters
      expect(result).not.toMatch(/[\u4e00-\u9fff]/);
    });

    it('returns correct zh-TW string for known key', () => {
      setLocale('zh-TW');
      const result = t('menu.newGame');
      expect(result).toBeTruthy();
      expect(typeof result).toBe('string');
      expect(result.length).toBeGreaterThan(0);
    });
  });

  describe('t() — parameter interpolation', () => {
    it('interpolates string parameters correctly', () => {
      setLocale('en');
      const result = t('menu.continue', { class: 'Warrior', level: 5 });
      expect(result).toContain('Warrior');
      expect(result).toContain('5');
      // No raw placeholders should remain
      expect(result).not.toContain('{class}');
      expect(result).not.toContain('{level}');
    });

    it('interpolates zh-CN parameters correctly', () => {
      setLocale('zh-CN');
      const result = t('menu.continue', { class: '战士', level: 10 });
      expect(result).toContain('战士');
      expect(result).toContain('10');
      expect(result).not.toContain('{class}');
      expect(result).not.toContain('{level}');
    });

    it('leaves template as-is when no params provided for template with placeholders', () => {
      setLocale('en');
      // If template has {class} but no params provided, placeholders remain
      const result = t('menu.continue');
      expect(typeof result).toBe('string');
    });

    it('handles multiple parameters', () => {
      setLocale('en');
      const result = t('menu.continue', { class: 'Mage', level: 20 });
      expect(result).toContain('Mage');
      expect(result).toContain('20');
    });
  });

  describe('t() — fallback chain', () => {
    it('falls back to en for missing zh-CN key', () => {
      setLocale('zh-CN');
      // Use a key that only exists in en
      const result = t('test.enOnly');
      // Should return the English value, not undefined
      expect(result).toBeTruthy();
      expect(typeof result).toBe('string');
    });

    it('returns key path string for completely missing key', () => {
      setLocale('zh-CN');
      const result = t('nonexistent.totally.fake.key');
      expect(result).toBe('nonexistent.totally.fake.key');
    });

    it('returns key path string for missing key in en locale too', () => {
      setLocale('en');
      const result = t('another.missing.key');
      expect(result).toBe('another.missing.key');
    });

    it('zh-TW falls back to zh-CN then to en', () => {
      setLocale('zh-TW');
      // For a key that exists in zh-CN, zh-TW should have the converted version
      const result = t('menu.newGame');
      expect(result).toBeTruthy();
      expect(typeof result).toBe('string');
    });

    it('zh-TW falls back to en for key missing in both zh-CN and zh-TW', () => {
      setLocale('zh-TW');
      const result = t('test.enOnly');
      expect(result).toBeTruthy();
      expect(typeof result).toBe('string');
    });

    it('never returns undefined for any key', () => {
      for (const locale of ['zh-CN', 'zh-TW', 'en']) {
        setLocale(locale);
        const result = t('totally.does.not.exist');
        expect(result).not.toBeUndefined();
        expect(typeof result).toBe('string');
      }
    });
  });

  describe('setLocale() / getLocale()', () => {
    it('setLocale changes t() output immediately', () => {
      setLocale('zh-CN');
      const zhResult = t('menu.newGame');
      setLocale('en');
      const enResult = t('menu.newGame');
      expect(zhResult).not.toBe(enResult);
    });

    it('getLocale returns current locale', () => {
      setLocale('en');
      expect(getLocale()).toBe('en');
      setLocale('zh-CN');
      expect(getLocale()).toBe('zh-CN');
      setLocale('zh-TW');
      expect(getLocale()).toBe('zh-TW');
    });
  });

  describe('getLocales()', () => {
    it('returns exactly three locales: zh-CN, zh-TW, en', () => {
      const locales = getLocales();
      expect(locales).toHaveLength(3);
      expect(locales).toContain('zh-CN');
      expect(locales).toContain('zh-TW');
      expect(locales).toContain('en');
    });
  });

  describe('localStorage persistence', () => {
    it('saves locale to localStorage on setLocale()', () => {
      setLocale('en');
      expect(localStorageMock.getItem('abyssfire_locale')).toBe('en');
    });

    it('saves zh-TW to localStorage', () => {
      setLocale('zh-TW');
      expect(localStorageMock.getItem('abyssfire_locale')).toBe('zh-TW');
    });

    it('restores locale from localStorage on module init', async () => {
      localStorageMock.setItem('abyssfire_locale', 'en');
      vi.resetModules();
      const freshModule = await import('../i18n/index');
      expect(freshModule.getLocale()).toBe('en');
    });

    it('defaults to zh-CN when localStorage is empty', async () => {
      localStorageMock.clear();
      vi.resetModules();
      const freshModule = await import('../i18n/index');
      expect(freshModule.getLocale()).toBe('zh-CN');
    });

    it('falls back to zh-CN for invalid localStorage value', async () => {
      localStorageMock.setItem('abyssfire_locale', 'xx-INVALID');
      vi.resetModules();
      const freshModule = await import('../i18n/index');
      expect(freshModule.getLocale()).toBe('zh-CN');
    });
  });

  describe('LOCALE_CHANGED event', () => {
    it('GameEvents has LOCALE_CHANGED entry', () => {
      expect(GameEvents.LOCALE_CHANGED).toBeDefined();
      expect(typeof GameEvents.LOCALE_CHANGED).toBe('string');
    });

    it('emits LOCALE_CHANGED event on setLocale()', () => {
      const handler = vi.fn();
      EventBus.on(GameEvents.LOCALE_CHANGED, handler);
      setLocale('en');
      expect(handler).toHaveBeenCalledWith('en');
    });

    it('emits LOCALE_CHANGED with the new locale value', () => {
      const handler = vi.fn();
      EventBus.on(GameEvents.LOCALE_CHANGED, handler);
      setLocale('zh-TW');
      expect(handler).toHaveBeenCalledWith('zh-TW');
    });

    it('does not emit if locale is already the current one', () => {
      setLocale('zh-CN');
      const handler = vi.fn();
      EventBus.on(GameEvents.LOCALE_CHANGED, handler);
      setLocale('zh-CN');
      expect(handler).not.toHaveBeenCalled();
    });
  });
});

describe('zh-TW Converter', () => {
  it('converts basic Simplified characters to Traditional', () => {
    const result = convertToTraditional('铁匠');
    expect(result).toBe('鐵匠');
  });

  it('converts multiple characters in a string', () => {
    const result = convertToTraditional('装备');
    expect(result).toBe('裝備');
  });

  it('preserves ASCII characters unchanged', () => {
    const result = convertToTraditional('Lv.5');
    expect(result).toBe('Lv.5');
  });

  it('preserves numbers unchanged', () => {
    const result = convertToTraditional('12345');
    expect(result).toBe('12345');
  });

  it('preserves punctuation unchanged', () => {
    const result = convertToTraditional('你好！世界？');
    // 你 and 好 don't change; 世 and 界 don't change
    // Just verify punctuation is intact
    expect(result).toContain('！');
    expect(result).toContain('？');
  });

  it('handles mixed CJK and ASCII text', () => {
    const input = '背包 (5/100)';
    const result = convertToTraditional(input);
    // 背包 stays as 背包 (no traditional difference)
    // Parentheses and numbers stay
    expect(result).toContain('(5/100)');
  });

  it('handles empty string', () => {
    expect(convertToTraditional('')).toBe('');
  });

  it('converts characters that have Traditional counterparts', () => {
    // 战 → 戰, 选 → 選, 游 → 遊
    expect(convertToTraditional('战')).toBe('戰');
    expect(convertToTraditional('选')).toBe('選');
    expect(convertToTraditional('游')).toBe('遊');
  });

  it('has at least 800 character mappings', async () => {
    const converterModule = await import('../i18n/converter');
    // The converter should expose or have at least 800 mappings
    expect(converterModule.MAPPING_COUNT).toBeGreaterThanOrEqual(800);
  });

  it('produces different output for strings with convertible characters', () => {
    // Strings that should definitely change between zh-CN and zh-TW
    const simplified = '继续游戏';
    const traditional = convertToTraditional(simplified);
    expect(traditional).not.toBe(simplified);
  });

  it('preserves characters not in the mapping table', () => {
    // Characters that are same in Simplified and Traditional
    const input = '你好世界';
    const result = convertToTraditional(input);
    // These characters are the same in both variants
    expect(result).toBe('你好世界');
  });
});

describe('Locale Data Integrity', () => {
  it('zh-CN locale has required skeleton keys', async () => {
    const zhCN = (await import('../i18n/locales/zh-CN')).default;
    expect(zhCN['menu.newGame']).toBeDefined();
    expect(zhCN['menu.continue']).toBeDefined();
  });

  it('en locale has required skeleton keys', async () => {
    const en = (await import('../i18n/locales/en')).default;
    expect(en['menu.newGame']).toBeDefined();
    expect(en['menu.continue']).toBeDefined();
  });

  it('en locale has a test-only key for fallback testing', async () => {
    const en = (await import('../i18n/locales/en')).default;
    expect(en['test.enOnly']).toBeDefined();
  });

  it('zh-CN does NOT have the test.enOnly key', async () => {
    const zhCN = (await import('../i18n/locales/zh-CN')).default;
    expect(zhCN['test.enOnly']).toBeUndefined();
  });

  it('all en keys exist in zh-CN (except test-only keys)', async () => {
    const zhCN = (await import('../i18n/locales/zh-CN')).default;
    const en = (await import('../i18n/locales/en')).default;
    const missingInZhCN: string[] = [];
    for (const key of Object.keys(en)) {
      if (key.startsWith('test.')) continue; // Skip test-only keys
      if (!(key in zhCN)) {
        missingInZhCN.push(key);
      }
    }
    expect(missingInZhCN).toEqual([]);
  });

  it('zh-TW locale has the same keys as zh-CN', async () => {
    const zhCN = (await import('../i18n/locales/zh-CN')).default;
    const zhTW = (await import('../i18n/locales/zh-TW')).default;
    const zhCNKeys = Object.keys(zhCN).sort();
    const zhTWKeys = Object.keys(zhTW).sort();
    expect(zhTWKeys).toEqual(zhCNKeys);
  });
});
