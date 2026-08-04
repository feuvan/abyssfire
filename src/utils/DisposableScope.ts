export type Disposer = () => void;

interface EmitterLike {
  on(event: string, listener: (...args: any[]) => void, context?: unknown): unknown;
  off(event: string, listener: (...args: any[]) => void, context?: unknown): unknown;
}

export class DisposableScope {
  private disposers: Disposer[] = [];
  private disposed = false;

  add(disposer: Disposer): Disposer {
    if (this.disposed) {
      disposer();
    } else {
      this.disposers.push(disposer);
    }
    return disposer;
  }

  on(emitter: EmitterLike, event: string, listener: (...args: any[]) => void, context?: unknown): void {
    emitter.on(event, listener, context);
    this.add(() => emitter.off(event, listener, context));
  }

  onDom(
    target: EventTarget,
    event: string,
    listener: EventListenerOrEventListenerObject,
    options?: boolean | AddEventListenerOptions,
  ): void {
    target.addEventListener(event, listener, options);
    this.add(() => target.removeEventListener(event, listener, options));
  }

  dispose(): void {
    if (this.disposed) return;
    this.disposed = true;
    for (let i = this.disposers.length - 1; i >= 0; i--) this.disposers[i]();
    this.disposers = [];
  }
}
