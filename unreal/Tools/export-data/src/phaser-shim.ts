/**
 * Phaser stand-in for the exporter (aliased to `phaser` by run.mjs).
 *
 * The data modules do not need Phaser, but some systems the exporter reads constants from import
 * it at module level (classes extending Phaser.Scene / GameObjects.Container, enums, ...). This
 * shim keeps those modules loadable:
 *   - `Events.EventEmitter` is the repo's own test mock (EventBus works),
 *   - `Math.Distance.Between` is the real formula (SkillEffectSystem.getProjectileTravelMs is probed),
 *   - every other property is an inert, constructible stub (never used for data).
 */
import MockPhaser, { EventEmitter } from '../../../../src/__mocks__/phaser';

type Stub = ((...args: unknown[]) => unknown) & Record<string | symbol, unknown>;

function stub(path: string): Stub {
  const target = function stubFn(): void { /* inert */ } as unknown as Stub;
  const cache = new Map<string | symbol, unknown>();
  return new Proxy(target, {
    get(t, prop) {
      if (prop === 'prototype') return t.prototype;
      if (prop === Symbol.toPrimitive) return () => 0;
      if (prop === 'toString') return () => `[phaser-shim ${path}]`;
      if (prop === 'then') return undefined; // never look like a promise
      if (!cache.has(prop)) cache.set(prop, stub(`${path}.${String(prop)}`));
      return cache.get(prop);
    },
    construct() { return {}; },
    apply() { return undefined; },
  });
}

const real: Record<string, unknown> = {
  ...MockPhaser,
  Events: { EventEmitter },
  Math: new Proxy({
    // Phaser.Math.Distance.Between (phaser/src/math/distance/DistanceBetween.js)
    Distance: {
      Between(x1: number, y1: number, x2: number, y2: number): number {
        const dx = x1 - x2;
        const dy = y1 - y2;
        return Math.sqrt(dx * dx + dy * dy);
      },
    },
  } as Record<string | symbol, unknown>, {
    get(t, prop) { return prop in t ? t[prop] : stub(`Math.${String(prop)}`); },
  }),
};

const Phaser = new Proxy(real, {
  get(t, prop) {
    if (prop in t) return t[prop as string];
    if (prop === 'then' || typeof prop === 'symbol') return undefined;
    const s = stub(String(prop));
    t[prop] = s;
    return s;
  },
}) as unknown as typeof MockPhaser;

export default Phaser;
export { EventEmitter };
