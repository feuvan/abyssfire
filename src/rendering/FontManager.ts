import type { LocaleId } from '../i18n/types';

export type FontBundle = 'latin';

const loadedBundles = new Map<FontBundle, Promise<void>>();

export function fontBundlesForLocale(locale: LocaleId): FontBundle[] {
  void locale;
  return ['latin'];
}

function loadBundle(bundle: FontBundle): Promise<void> {
  const existing = loadedBundles.get(bundle);
  if (existing) return existing;

  let promise: Promise<unknown>;
  switch (bundle) {
    case 'latin':
      promise = Promise.all([
        import('@fontsource/cinzel/latin-400.css'),
        import('@fontsource/cinzel/latin-700.css'),
      ]);
      break;
  }

  const load = promise.then(() => undefined);
  loadedBundles.set(bundle, load);
  return load;
}

export async function ensureFontsForLocale(locale: LocaleId): Promise<void> {
  await Promise.all(fontBundlesForLocale(locale).map(loadBundle));
}

export function initializeFontManager(initialLocale: LocaleId): Promise<void> {
  return ensureFontsForLocale(initialLocale);
}
