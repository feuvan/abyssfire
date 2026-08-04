import { describe, expect, it } from 'vitest';
import { fontBundlesForLocale } from '../rendering/FontManager';

describe('FontManager locale routing', () => {
  it('loads only latin fonts for English', () => {
    expect(fontBundlesForLocale('en')).toEqual(['latin']);
  });

  it('uses the platform CJK stack without downloading locale font bundles', () => {
    expect(fontBundlesForLocale('zh-CN')).toEqual(['latin']);
    expect(fontBundlesForLocale('zh-TW')).toEqual(['latin']);
  });
});
