/**
 * i18n core module.
 * Provides t(), setLocale(), getLocale(), getLocales() for internationalization.
 *
 * Fallback chain: zh-TW → zh-CN → en → key path
 * Persistence: localStorage key 'abyssfire_locale', default 'zh-CN'
 */
import type { LocaleId, LocaleData } from './types';
import zhCN from './locales/zh-CN';
import en from './locales/en';
import zhTW from './locales/zh-TW';
import { EventBus, GameEvents } from '../utils/EventBus';

const STORAGE_KEY = 'abyssfire_locale';
const DEFAULT_LOCALE: LocaleId = 'zh-CN';
const SUPPORTED_LOCALES: LocaleId[] = ['zh-CN', 'zh-TW', 'en'];

const localeMap: Record<LocaleId, LocaleData> = {
  'zh-CN': zhCN,
  'zh-TW': zhTW,
  'en': en,
};

/** Resolve initial locale from localStorage */
function resolveInitialLocale(): LocaleId {
  try {
    const stored = localStorage.getItem(STORAGE_KEY);
    if (stored && SUPPORTED_LOCALES.includes(stored as LocaleId)) {
      return stored as LocaleId;
    }
  } catch {
    // localStorage may not be available (e.g., in some test environments)
  }
  return DEFAULT_LOCALE;
}

let currentLocale: LocaleId = resolveInitialLocale();

/**
 * Translate a key to the current locale string.
 * Supports {paramName} parameter interpolation.
 * Fallback chain: current locale → zh-CN → en → key path string.
 */
export function t(key: string, params?: Record<string, string | number>): string {
  let value: string | undefined;

  // Try current locale
  value = localeMap[currentLocale]?.[key];

  // Fallback: zh-TW → zh-CN
  if (value === undefined && currentLocale === 'zh-TW') {
    value = localeMap['zh-CN']?.[key];
  }

  // Fallback: any locale → en
  if (value === undefined && currentLocale !== 'en') {
    value = localeMap['en']?.[key];
  }

  // Final fallback: return key path
  if (value === undefined) {
    return key;
  }

  // Parameter interpolation
  if (params) {
    for (const [paramKey, paramValue] of Object.entries(params)) {
      value = value.replace(new RegExp(`\\{${paramKey}\\}`, 'g'), String(paramValue));
    }
  }

  return value;
}

/**
 * Set the active locale. Persists to localStorage and emits LOCALE_CHANGED event.
 * No-op if the locale is already the current one.
 */
export function setLocale(locale: string): void {
  if (!SUPPORTED_LOCALES.includes(locale as LocaleId)) {
    return; // Invalid locale, ignore
  }
  if (locale === currentLocale) {
    return; // Already set, no-op
  }
  currentLocale = locale as LocaleId;
  try {
    localStorage.setItem(STORAGE_KEY, locale);
  } catch {
    // localStorage may not be available
  }
  EventBus.emit(GameEvents.LOCALE_CHANGED, locale);
}

/** Get the current active locale identifier */
export function getLocale(): LocaleId {
  return currentLocale;
}

/** Get the list of all supported locale identifiers */
export function getLocales(): LocaleId[] {
  return [...SUPPORTED_LOCALES];
}
