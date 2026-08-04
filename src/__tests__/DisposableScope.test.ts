import { describe, expect, it, vi } from 'vitest';
import { DisposableScope } from '../utils/DisposableScope';

describe('DisposableScope', () => {
  it('disposes resources in reverse registration order exactly once', () => {
    const calls: number[] = [];
    const scope = new DisposableScope();
    scope.add(() => calls.push(1));
    scope.add(() => calls.push(2));
    scope.dispose();
    scope.dispose();
    expect(calls).toEqual([2, 1]);
  });

  it('pairs emitter subscriptions with their original context', () => {
    const emitter = { on: vi.fn(), off: vi.fn() };
    const listener = vi.fn();
    const context = {};
    const scope = new DisposableScope();
    scope.on(emitter, 'changed', listener, context);
    scope.dispose();
    expect(emitter.on).toHaveBeenCalledWith('changed', listener, context);
    expect(emitter.off).toHaveBeenCalledWith('changed', listener, context);
  });

  it('immediately disposes resources registered after shutdown', () => {
    const disposer = vi.fn();
    const scope = new DisposableScope();
    scope.dispose();
    scope.add(disposer);
    expect(disposer).toHaveBeenCalledOnce();
  });
});
