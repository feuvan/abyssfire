import { clamp } from '../../../utils/MathUtils';

export interface PoseStop {
  at: number;
  value: number;
}

export function smoothstep(value: number): number {
  const t = clamp(value, 0, 1);
  return t * t * (3 - 2 * t);
}

export function sinePulse(value: number): number {
  return Math.sin(clamp(value, 0, 1) * Math.PI);
}

/** Smoothly sample a numeric pose curve from ordered keyframes. */
export function samplePose(value: number, stops: readonly PoseStop[]): number {
  if (stops.length === 0) return 0;
  const t = clamp(value, 0, 1);
  if (t <= stops[0].at) return stops[0].value;

  for (let i = 1; i < stops.length; i++) {
    const previous = stops[i - 1];
    const next = stops[i];
    if (t <= next.at) {
      const span = Math.max(0.0001, next.at - previous.at);
      const localT = smoothstep((t - previous.at) / span);
      return previous.value + (next.value - previous.value) * localT;
    }
  }

  return stops[stops.length - 1].value;
}
