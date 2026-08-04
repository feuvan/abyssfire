import { describe, expect, it } from 'vitest';
import { profileForQuality, resolutionForQuality, selectRenderQuality } from '../rendering/RenderQuality';

describe('RenderQuality', () => {
  it('scales expensive effects monotonically across quality levels', () => {
    const low = profileForQuality('low');
    const balanced = profileForQuality('balanced');
    const high = profileForQuality('high');
    expect(low.maxDynamicLights).toBeLessThan(balanced.maxDynamicLights);
    expect(balanced.maxDynamicLights).toBeLessThan(high.maxDynamicLights);
    expect(low.particleFrequencyMultiplier).toBeGreaterThan(balanced.particleFrequencyMultiplier);
    expect(balanced.particleFrequencyMultiplier).toBeGreaterThan(high.particleFrequencyMultiplier);
    expect(low.bloom).toBe(false);
  });
  it('keeps mobile devices on the low quality profile', () => {
    expect(selectRenderQuality({
      devicePixelRatio: 3,
      viewportWidth: 390,
      viewportHeight: 844,
      mobile: true,
      hardwareConcurrency: 8,
      deviceMemory: 8,
    })).toBe('low');
  });

  it('uses a balanced profile for ordinary desktop displays', () => {
    expect(selectRenderQuality({
      devicePixelRatio: 1.5,
      viewportWidth: 1440,
      viewportHeight: 900,
      mobile: false,
      hardwareConcurrency: 8,
      deviceMemory: 8,
    })).toBe('balanced');
  });

  it('allows high quality only when the resulting pixel budget is bounded', () => {
    expect(selectRenderQuality({
      devicePixelRatio: 2,
      viewportWidth: 1280,
      viewportHeight: 720,
      mobile: false,
      hardwareConcurrency: 10,
      deviceMemory: 16,
    })).toBe('high');

    expect(selectRenderQuality({
      devicePixelRatio: 2,
      viewportWidth: 2560,
      viewportHeight: 1440,
      mobile: false,
      hardwareConcurrency: 10,
      deviceMemory: 16,
    })).toBe('low');
  });

  it('caps profiles by the physical device pixel ratio', () => {
    expect(resolutionForQuality('high', 1)).toBe(1);
    expect(resolutionForQuality('balanced', 2)).toBe(1.5);
    expect(resolutionForQuality('high', 3)).toBe(2);
  });
});
