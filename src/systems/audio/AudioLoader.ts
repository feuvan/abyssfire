/**
 * AudioLoader — stores decoded AudioBuffer objects keyed by string.
 *
 * Actual file loading happens in BootScene following the project's asset
 * pipeline pattern (attempt external load -> silent loaderror catch ->
 * procedural fallback).  AudioLoader is the storage layer: BootScene decodes
 * raw ArrayBuffers here; SFXEngine and MusicEngine retrieve them at play-time.
 *
 * Expected file path conventions (loading handled by BootScene):
 *   BGM : assets/audio/bgm/{zoneId}_{state}.mp3
 *   SFX : assets/audio/sfx/{type}.mp3
 *
 * When a file is missing (the current default — no audio assets are shipped),
 * the buffer simply won't be present and callers receive null, then fall back
 * to procedural Web Audio synthesis.
 */
export class AudioLoader {
  private buffers: Map<string, AudioBuffer> = new Map();
  private pendingLoads: Map<string, Promise<AudioBuffer | null>> = new Map();
  private pendingControllers: Map<string, AbortController> = new Map();

  /** Returns true if a decoded buffer is stored for the given key. */
  has(key: string): boolean {
    return this.buffers.has(key);
  }

  /**
   * Returns the decoded AudioBuffer for the given key, or null if absent.
   * Callers should use procedural synthesis when null is returned.
   */
  getBuffer(key: string): AudioBuffer | null {
    return this.buffers.get(key) ?? null;
  }

  /** Stores an already-decoded AudioBuffer directly. */
  storeBuffer(key: string, buffer: AudioBuffer): void {
    this.buffers.set(key, buffer);
  }

  /** Remove a decoded buffer and cancel any in-flight request for the same key. */
  releaseBuffer(key: string): void {
    this.pendingControllers.get(key)?.abort();
    this.pendingControllers.delete(key);
    this.pendingLoads.delete(key);
    this.buffers.delete(key);
  }

  /** Remove all buffers matching the predicate and cancel matching in-flight loads. */
  releaseMatching(predicate: (key: string) => boolean): void {
    const keys = new Set<string>([
      ...this.buffers.keys(),
      ...this.pendingLoads.keys(),
      ...this.pendingControllers.keys(),
    ]);

    for (const key of keys) {
      if (predicate(key)) this.releaseBuffer(key);
    }
  }

  /**
   * Decodes the raw ArrayBuffer using the provided AudioContext and stores the
   * result under key.  Rejects if decoding fails (e.g. unsupported format).
   */
  async decodeAudio(ctx: AudioContext, key: string, arrayBuffer: ArrayBuffer): Promise<void> {
    const decoded = await ctx.decodeAudioData(arrayBuffer);
    this.buffers.set(key, decoded);
  }

  /**
   * Fetch and decode an audio file on demand.
   * Returns null if the file doesn't exist or decoding fails.
   */
  async loadAudioFromUrl(ctx: AudioContext, key: string, url: string): Promise<AudioBuffer | null> {
    const existing = this.buffers.get(key);
    if (existing) return existing;

    const pending = this.pendingLoads.get(key);
    if (pending) return pending;

    const controller = new AbortController();
    this.pendingControllers.set(key, controller);

    const loadPromise = fetch(url, { signal: controller.signal })
      .then(async (response) => {
        if (!response.ok) return null;
        const contentType = response.headers.get('content-type')?.toLowerCase() ?? '';
        if (!contentType.startsWith('audio/')) return null;
        const arrayBuffer = await response.arrayBuffer();
        const decoded = await ctx.decodeAudioData(arrayBuffer);
        if (!controller.signal.aborted) {
          this.buffers.set(key, decoded);
          return decoded;
        }
        return null;
      })
      .catch(() => null)
      .finally(() => {
        this.pendingLoads.delete(key);
        this.pendingControllers.delete(key);
      });

    this.pendingLoads.set(key, loadPromise);
    return loadPromise;
  }
}
