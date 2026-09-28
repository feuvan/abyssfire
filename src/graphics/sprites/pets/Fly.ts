/**
 * Hovering ley-beasts (owl, ember chick, ley sprite, void butterfly):
 * shared pose model and tracks. Wings flap in both idle and walk; the
 * attack is a rise-and-dive that connects on the last frame; the cast holds
 * the wings wide at the peak (frame 2).
 */
import type { PetAction, PetStage } from './PetKit';

export interface FlyPose {
  /** Body centre (x forward, y negative up). */
  bx: number;
  by: number;
  pitch: number;
  /** Wing spread (0 up … π/2 out … > π/2 down), near and far. */
  wn: number;
  wf: number;
  /** Wing sweep back 0..1. */
  sweep: number;
  /** Head offset / pitch. */
  hx: number;
  hy: number;
  hp: number;
  /** Beak / mouth open, talons forward, tail raise. */
  beak: number;
  talon: number;
  tail: number;
  lid: number;
  glow: number;
  fx: number;
}

export interface FlyStyle {
  hover: number;
  /** Flap amplitude and centre (radians of spread). */
  flapMid: number;
  flapAmp: number;
  /** Flaps per idle loop (walk doubles the amplitude, not the rate). */
  flapsIdle: number;
  flapsWalk: number;
  bob: number;
}

const lerp = (a: number, b: number, k: number): number => a + (b - a) * k;

export function lerpFly(a: FlyPose, b: FlyPose, k: number): FlyPose {
  const o = { ...a };
  for (const key of Object.keys(a) as (keyof FlyPose)[]) o[key] = lerp(a[key], b[key], k);
  return o;
}

export function flyTrack(keys: readonly [number, FlyPose][], t: number): FlyPose {
  if (t <= keys[0][0]) return keys[0][1];
  for (let i = 1; i < keys.length; i++) {
    if (t <= keys[i][0]) {
      const u = (t - keys[i - 1][0]) / Math.max(1e-4, keys[i][0] - keys[i - 1][0]);
      return lerpFly(keys[i - 1][1], keys[i][1], u * u * (3 - 2 * u));
    }
  }
  return keys[keys.length - 1][1];
}

export function flyRest(s: FlyStyle): FlyPose {
  return {
    bx: 0, by: -s.hover, pitch: 0, wn: s.flapMid, wf: s.flapMid, sweep: 0,
    hx: 0, hy: 0, hp: 0, beak: 0, talon: 0, tail: 0, lid: 1, glow: 0, fx: 0,
  };
}

export function flyPose(s: FlyStyle, action: PetAction, t: number, _stage: PetStage): FlyPose {
  const R = flyRest(s);
  const ph = t * Math.PI * 2;
  switch (action) {
    case 'idle': {
      const f = Math.cos(ph * s.flapsIdle);
      return {
        ...R,
        by: R.by + Math.sin(ph) * s.bob,
        wn: s.flapMid + f * s.flapAmp,
        wf: s.flapMid + Math.cos(ph * s.flapsIdle - 0.35) * s.flapAmp,
        hp: Math.sin(ph - 0.8) * 0.05,
        tail: Math.sin(ph + 1) * 0.15,
        lid: t > 0.74 && t < 0.76 ? 0.2 : 1,
      };
    }
    case 'walk': {
      const f = Math.cos(ph * s.flapsWalk);
      return {
        ...R,
        bx: 0.5,
        by: R.by + Math.sin(ph * s.flapsWalk + 0.8) * s.bob * 0.8,
        pitch: 0.14,
        wn: s.flapMid + f * s.flapAmp * 1.25,
        wf: s.flapMid + Math.cos(ph * s.flapsWalk - 0.35) * s.flapAmp * 1.25,
        sweep: 0.15,
        tail: -0.2,
      };
    }
    case 'attack': {
      const up = { ...R, bx: -3, by: R.by - 4, pitch: -0.3, wn: 0.2, wf: 0.25, hp: -0.25, talon: 0.3, tail: 0.3, fx: 0.2 };
      const dive = { ...R, bx: 4, by: R.by + 3, pitch: 0.35, wn: 1.3, wf: 1.25, sweep: 0.6, hp: 0.2, beak: 0.6, talon: 0.7, tail: -0.3, fx: 0.7 };
      const hit = { ...R, bx: 8, by: R.by + 5, pitch: 0.2, wn: 0.5, wf: 0.55, sweep: 0.2, hp: 0.35, hx: 1, beak: 1, talon: 1, tail: -0.2, fx: 1 };
      return flyTrack([[0, R], [0.33, up], [0.67, dive], [1, hit]], t);
    }
    case 'cast': {
      const gather = { ...R, by: R.by + 1.5, pitch: 0.05, wn: 1.9, wf: 1.9, hp: 0.15, lid: 0.5, glow: 0.4 };
      const peak = { ...R, by: R.by - 4, pitch: -0.12, wn: 1.35, wf: 1.35, hp: -0.25, beak: 0.6, lid: 0.3, glow: 1, tail: 0.3 };
      const settle = { ...R, by: R.by - 2.5, pitch: -0.05, wn: 0.9, wf: 0.95, hp: -0.1, beak: 0.2, lid: 0.8, glow: 0.7 };
      return flyTrack([[0, R], [0.33, gather], [0.67, peak], [1, settle]], t);
    }
    case 'hurt': {
      const hit = { ...R, bx: -4, by: R.by - 1, pitch: -0.4, wn: 0.4, wf: 2, hp: -0.4, lid: 0.15, tail: 0.4 };
      return flyTrack([[0, hit], [1, { ...R, bx: -2, pitch: -0.15, wn: 1, wf: 1.3, hp: -0.15, lid: 0.6 }]], t);
    }
  }
}
