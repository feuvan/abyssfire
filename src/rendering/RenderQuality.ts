export type RenderQuality = 'low' | 'balanced' | 'high';

export interface RenderQualityProfile {
  quality: RenderQuality;
  maxDynamicLights: number;
  lightingUpdateIntervalMs: number;
  particleFrequencyMultiplier: number;
  bloom: boolean;
  colorGrading: boolean;
}

const PROFILES: Record<RenderQuality, RenderQualityProfile> = {
  low: {
    quality: 'low', maxDynamicLights: 8, lightingUpdateIntervalMs: 100,
    particleFrequencyMultiplier: 2, bloom: false, colorGrading: false,
  },
  balanced: {
    quality: 'balanced', maxDynamicLights: 16, lightingUpdateIntervalMs: 50,
    particleFrequencyMultiplier: 1.5, bloom: true, colorGrading: true,
  },
  high: {
    quality: 'high', maxDynamicLights: 32, lightingUpdateIntervalMs: 16,
    particleFrequencyMultiplier: 1, bloom: true, colorGrading: true,
  },
};

export function profileForQuality(quality: RenderQuality): RenderQualityProfile {
  return PROFILES[quality];
}

export interface RenderEnvironment {
  devicePixelRatio: number;
  viewportWidth: number;
  viewportHeight: number;
  mobile: boolean;
  hardwareConcurrency?: number;
  deviceMemory?: number;
}

const RESOLUTION_BY_QUALITY: Record<RenderQuality, number> = {
  low: 1,
  balanced: 1.5,
  high: 2,
};

function clampResolution(value: number): number {
  return Math.max(1, Math.min(2, Math.round(value * 2) / 2));
}

export function selectRenderQuality(env: RenderEnvironment): RenderQuality {
  const pixelBudget = env.viewportWidth * env.viewportHeight * env.devicePixelRatio ** 2;
  const constrainedCpu = env.hardwareConcurrency !== undefined && env.hardwareConcurrency <= 4;
  const constrainedMemory = env.deviceMemory !== undefined && env.deviceMemory <= 4;

  if (env.mobile || constrainedCpu || constrainedMemory || pixelBudget > 5_000_000) {
    return 'low';
  }
  if (env.devicePixelRatio >= 2 && pixelBudget <= 3_700_000) {
    return 'high';
  }
  return 'balanced';
}

export function resolutionForQuality(quality: RenderQuality, devicePixelRatio: number): number {
  return clampResolution(Math.min(devicePixelRatio || 1, RESOLUTION_BY_QUALITY[quality]));
}

function readQualityOverride(): RenderQuality | null {
  if (typeof window === 'undefined') return null;

  const queryValue = new URLSearchParams(window.location.search).get('quality');
  if (queryValue === 'low' || queryValue === 'balanced' || queryValue === 'high') return queryValue;

  try {
    const stored = window.localStorage.getItem('abyssfire_render_quality');
    if (stored === 'low' || stored === 'balanced' || stored === 'high') return stored;
  } catch {
    // Storage can be unavailable in privacy modes; automatic selection still works.
  }
  return null;
}

export function detectRenderEnvironment(): RenderEnvironment {
  if (typeof window === 'undefined' || typeof navigator === 'undefined') {
    return { devicePixelRatio: 1, viewportWidth: 1280, viewportHeight: 720, mobile: false };
  }

  const nav = navigator as Navigator & { deviceMemory?: number };
  return {
    devicePixelRatio: window.devicePixelRatio || 1,
    viewportWidth: window.innerWidth || 1280,
    viewportHeight: window.innerHeight || 720,
    mobile: navigator.maxTouchPoints > 0 && Math.min(window.innerWidth, window.innerHeight) <= 820,
    hardwareConcurrency: navigator.hardwareConcurrency,
    deviceMemory: nav.deviceMemory,
  };
}

export function resolveRenderQuality(): RenderQuality {
  const env = detectRenderEnvironment();
  return readQualityOverride() ?? selectRenderQuality(env);
}
