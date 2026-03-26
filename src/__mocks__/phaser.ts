/**
 * Lightweight Phaser mock for unit testing.
 * Only stubs the APIs actually used by non-visual systems
 * (primarily Phaser.Events.EventEmitter via EventBus).
 */

class EventEmitter {
  private _listeners: Record<string, Array<(...args: unknown[]) => void>> = {};

  on(event: string, fn: (...args: unknown[]) => void): this {
    (this._listeners[event] ??= []).push(fn);
    return this;
  }

  once(event: string, fn: (...args: unknown[]) => void): this {
    const wrapper = (...args: unknown[]) => {
      this.off(event, wrapper);
      fn(...args);
    };
    return this.on(event, wrapper);
  }

  off(event: string, fn: (...args: unknown[]) => void): this {
    const arr = this._listeners[event];
    if (arr) {
      const idx = arr.indexOf(fn);
      if (idx !== -1) arr.splice(idx, 1);
    }
    return this;
  }

  emit(event: string, ...args: unknown[]): boolean {
    const arr = this._listeners[event];
    if (arr) {
      for (const fn of arr) fn(...args);
      return true;
    }
    return false;
  }

  removeAllListeners(event?: string): this {
    if (event) {
      delete this._listeners[event];
    } else {
      this._listeners = {};
    }
    return this;
  }

  listenerCount(event: string): number {
    return this._listeners[event]?.length ?? 0;
  }
}

const Phaser = {
  Events: { EventEmitter },
  AUTO: 0,
  Scale: {
    FIT: 1,
    CENTER_BOTH: 1,
  },
  Geom: {
    Point: class {
      x: number;
      y: number;
      constructor(x: number, y: number) { this.x = x; this.y = y; }
    },
  },
};

export default Phaser;
export { EventEmitter };
