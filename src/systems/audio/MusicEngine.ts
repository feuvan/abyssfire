/**
 * MusicEngine — dynamic zone-based music with a combat-aware state machine.
 *
 * Architecture:
 *   - Five zone themes (emerald_plains, twilight_forest, anvil_mountains,
 *     scorching_desert, abyss_rift) defined as ZONE_THEMES constant.
 *   - Three music states: 'explore', 'combat', 'victory'.
 *   - Four procedural layers: pad (all), melody (explore+combat),
 *     rhythm (combat only), chime (explore only).
 *   - External AudioBuffer override: if AudioLoader has a buffer keyed
 *     'bgm_{zoneId}_{state}' it is played instead of the procedural layers.
 *   - Crossfading: state transitions 1.5 s, zone transitions 2 s.
 *   - Victory stinger + 3 s auto-return to 'explore'.
 *   - All nodes and timeouts tracked; fully cleaned up on stop/transition.
 */

import { AudioLoader } from './AudioLoader';
import type { MusicState, ZoneTheme } from './types';

// ---------------------------------------------------------------------------
// Zone theme definitions
// ---------------------------------------------------------------------------

export const ZONE_THEMES: Record<string, ZoneTheme> = {
  emerald_plains: {
    id: 'emerald_plains',
    baseKey: 130.81,
    scale: [130.81, 146.83, 164.81, 196.00, 220.00],
    tempo: 72,
    padWaveform: 'sine',
    mood: 'pastoral',
  },
  twilight_forest: {
    id: 'twilight_forest',
    baseKey: 146.83,
    scale: [146.83, 164.81, 174.61, 196.00, 220.00, 246.94, 261.63],
    tempo: 60,
    padWaveform: 'triangle',
    mood: 'mysterious',
  },
  anvil_mountains: {
    id: 'anvil_mountains',
    baseKey: 82.41,
    scale: [82.41, 92.50, 98.00, 110.00, 123.47, 130.81, 146.83],
    tempo: 80,
    padWaveform: 'sawtooth',
    mood: 'epic',
  },
  scorching_desert: {
    id: 'scorching_desert',
    baseKey: 220.00,
    scale: [220.00, 233.08, 261.63, 293.66, 329.63, 349.23, 392.00],
    tempo: 68,
    padWaveform: 'triangle',
    mood: 'exotic',
  },
  abyss_rift: {
    id: 'abyss_rift',
    baseKey: 92.50,
    scale: [92.50, 103.83, 110.00, 123.47, 138.59, 146.83, 164.81],
    tempo: 90,
    padWaveform: 'sawtooth',
    mood: 'dark',
  },
  menu: {
    id: 'menu',
    baseKey: 65.41,       // C2 — deep fundamental
    scale: [65.41, 77.78, 87.31, 98.00, 116.54],  // C minor pentatonic
    tempo: 40,
    padWaveform: 'sawtooth',
    mood: 'dark',
    padFilterCutoff: 200,
    padLFORate: 0.03,
  },
};

// ---------------------------------------------------------------------------
// Internal types
// ---------------------------------------------------------------------------

/** A handle to a running audio node so we can stop and disconnect it later. */
interface ManagedNode {
  node: AudioNode;
  stop?: () => void;
}

/** A complete set of layer gain nodes and managed oscillator/source handles
 *  representing one "snapshot" of playing music. */
interface LayerSet {
  masterGain: GainNode;
  nodes: ManagedNode[];
  timeouts: number[];
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Convert cents offset to a frequency multiplier. */
function centsToRatio(cents: number): number {
  return Math.pow(2, cents / 1200);
}

/** Return a random number in [min, max). */
function rand(min: number, max: number): number {
  return min + Math.random() * (max - min);
}

/** Pick a random element from an array. */
function pick<T>(arr: T[]): T {
  return arr[Math.floor(Math.random() * arr.length)];
}

/** Lowpass filter cutoff in Hz based on mood. */
function padCutoff(mood: ZoneTheme['mood']): number {
  switch (mood) {
    case 'pastoral':   return 500;
    case 'mysterious': return 350;
    case 'epic':       return 600;
    case 'exotic':     return 450;
    case 'dark':       return 300;
  }
}

/** LFO rate in Hz based on mood. */
function padLfoRate(mood: ZoneTheme['mood']): number {
  switch (mood) {
    case 'pastoral':   return 0.12;
    case 'mysterious': return 0.06;
    case 'epic':       return 0.18;
    case 'exotic':     return 0.10;
    case 'dark':       return 0.05;
  }
}

// ---------------------------------------------------------------------------
// MusicEngine
// ---------------------------------------------------------------------------

export class MusicEngine {
  private loader: AudioLoader;
  private currentZone: string | null = null;
  private currentState: MusicState = 'explore';
  private masterGain: GainNode | null = null;

  /** Currently playing layer set (fades out during transitions). */
  private activeSet: LayerSet | null = null;
  /** Transition fade-out timeout IDs so we can cancel mid-transition. */
  private transitionTimeouts: number[] = [];
  /** Victory auto-return timer. */
  private victoryTimer: number | null = null;

  constructor(loader: AudioLoader) {
    this.loader = loader;
  }

  // ---------------------------------------------------------------------------
  // Public API
  // ---------------------------------------------------------------------------

  /**
   * Switch to a new zone.  Crossfades over 2 s.
   * Call this instead of setState when changing zones.
   */
  setZone(ctx: AudioContext, destination: AudioNode, zoneId: string): void {
    if (this.currentZone === zoneId) return;
    this.currentZone = zoneId;
    this.currentState = 'explore';
    this._transition(ctx, destination, 2.0);
  }

  /**
   * Switch music state.  Crossfades over 1.5 s.
   * Victory auto-transitions back to explore after 3 s.
   */
  setState(ctx: AudioContext, destination: AudioNode, state: MusicState): void {
    if (this.currentState === state && this.activeSet !== null) return;
    this.currentState = state;

    // Cancel any pending victory auto-return.
    if (this.victoryTimer !== null) {
      clearTimeout(this.victoryTimer);
      this.victoryTimer = null;
    }

    this._transition(ctx, destination, 1.5);

    if (state === 'victory') {
      this.victoryTimer = window.setTimeout(() => {
        this.victoryTimer = null;
        this.setState(ctx, destination, 'explore');
      }, 3000);
    }
  }

  /** Fade out all layers and clean up. */
  stop(ctx: AudioContext): void {
    this._cancelTransitionTimers();

    if (this.victoryTimer !== null) {
      clearTimeout(this.victoryTimer);
      this.victoryTimer = null;
    }

    if (this.activeSet) {
      this._fadeOutAndDestroy(ctx, this.activeSet, 0.5);
      this.activeSet = null;
    }

    if (this.masterGain) {
      this.masterGain.gain.setTargetAtTime(0, ctx.currentTime, 0.3);
    }
  }

  /** Adjust master volume (0–1). */
  setVolume(v: number): void {
    if (this.masterGain) this.masterGain.gain.value = v;
  }

  // ---------------------------------------------------------------------------
  // Transition core
  // ---------------------------------------------------------------------------

  private _transition(ctx: AudioContext, destination: AudioNode, duration: number): void {
    // Ensure master gain exists.
    if (!this.masterGain) {
      this.masterGain = ctx.createGain();
      this.masterGain.gain.value = 1;
      this.masterGain.connect(destination);
    }

    // Cancel any in-progress transition fade-out timers (they reference an old set).
    this._cancelTransitionTimers();

    const oldSet = this.activeSet;
    this.activeSet = null;

    const zoneId = this.currentZone;
    if (!zoneId) {
      // No zone loaded yet — just destroy old if any.
      if (oldSet) this._fadeOutAndDestroy(ctx, oldSet, duration);
      return;
    }

    const theme = ZONE_THEMES[zoneId];
    if (!theme) {
      if (oldSet) this._fadeOutAndDestroy(ctx, oldSet, duration);
      return;
    }

    // Build new layer set.
    const newSet = this._buildLayerSet(ctx, this.masterGain, theme, this.currentState);
    this.activeSet = newSet;

    // Ramp new set in.
    newSet.masterGain.gain.setValueAtTime(0, ctx.currentTime);
    newSet.masterGain.gain.linearRampToValueAtTime(1, ctx.currentTime + duration);

    // Fade old set out, then destroy.
    if (oldSet) {
      this._fadeOutAndDestroy(ctx, oldSet, duration);
    }
  }

  private _cancelTransitionTimers(): void {
    for (const id of this.transitionTimeouts) clearTimeout(id);
    this.transitionTimeouts = [];
  }

  /** Ramp gain of a set to 0 over duration, then stop all nodes and timeouts. */
  private _fadeOutAndDestroy(ctx: AudioContext, set: LayerSet, duration: number): void {
    set.masterGain.gain.setValueAtTime(set.masterGain.gain.value, ctx.currentTime);
    set.masterGain.gain.linearRampToValueAtTime(0.0001, ctx.currentTime + duration);

    // Clear melody/chime/rhythm scheduling timers so they don't fire after fade.
    for (const id of set.timeouts) clearTimeout(id);
    set.timeouts.length = 0;

    const destroyId = window.setTimeout(() => {
      // Remove this id from transitionTimeouts (cleanup).
      const idx = this.transitionTimeouts.indexOf(destroyId);
      if (idx !== -1) this.transitionTimeouts.splice(idx, 1);

      for (const mn of set.nodes) {
        try { mn.stop?.(); } catch (_) { /* already stopped */ }
        try { mn.node.disconnect(); } catch (_) { /* already disconnected */ }
      }
      set.nodes.length = 0;
      try { set.masterGain.disconnect(); } catch (_) { /* ok */ }
    }, Math.ceil(duration * 1000) + 100);

    this.transitionTimeouts.push(destroyId);
  }

  // ---------------------------------------------------------------------------
  // Layer set builder
  // ---------------------------------------------------------------------------

  private _buildLayerSet(
    ctx: AudioContext,
    destination: AudioNode,
    theme: ZoneTheme,
    state: MusicState,
  ): LayerSet {
    const set: LayerSet = {
      masterGain: ctx.createGain(),
      nodes: [],
      timeouts: [],
    };
    set.masterGain.connect(destination);

    // Check for external buffer override first.
    const bufferKey = `bgm_${theme.id}_${state}`;
    if (this.loader.has(bufferKey)) {
      this._buildBufferLayer(ctx, set, bufferKey);
      return set;
    }

    // Procedural layers.
    this._buildPadLayer(ctx, set, theme);

    if (state === 'explore' || state === 'combat') {
      this._scheduleMelodyLayer(ctx, set, theme, state);
    }

    if (state === 'combat') {
      this._scheduleRhythmLayer(ctx, set, theme);
    }

    if (state === 'explore') {
      this._scheduleChimeLayer(ctx, set, theme);
    }

    if (state === 'victory') {
      this._buildVictoryStinger(ctx, set, theme);
    }

    return set;
  }

  // ---------------------------------------------------------------------------
  // External buffer layer
  // ---------------------------------------------------------------------------

  private _buildBufferLayer(ctx: AudioContext, set: LayerSet, key: string): void {
    const buffer = this.loader.getBuffer(key);
    if (!buffer) return;

    const source = ctx.createBufferSource();
    source.buffer = buffer;
    source.loop = true;
    source.connect(set.masterGain);
    source.start();

    set.nodes.push({ node: source, stop: () => source.stop() });
  }

  // ---------------------------------------------------------------------------
  // Pad layer (all states)
  // ---------------------------------------------------------------------------

  private _buildPadLayer(ctx: AudioContext, set: LayerSet, theme: ZoneTheme): void {
    const root = theme.baseKey;
    const padGain = ctx.createGain();
    padGain.gain.value = 0.15;

    // Lowpass filter.
    const filter = ctx.createBiquadFilter();
    filter.type = 'lowpass';
    const baseCutoff = theme.padFilterCutoff ?? padCutoff(theme.mood);
    filter.frequency.setValueAtTime(baseCutoff, ctx.currentTime);
    filter.Q.value = 0.8;

    // LFO modulates filter cutoff.
    const lfo = ctx.createOscillator();
    const lfoGain = ctx.createGain();
    lfo.type = 'sine';
    lfo.frequency.value = theme.padLFORate ?? padLfoRate(theme.mood);
    lfoGain.gain.value = baseCutoff * 0.3; // modulation depth ±30 % of cutoff
    lfo.connect(lfoGain);
    lfoGain.connect(filter.frequency);
    lfo.start();

    padGain.connect(filter);
    filter.connect(set.masterGain);

    set.nodes.push({ node: lfoGain });
    set.nodes.push({ node: lfo, stop: () => { try { lfo.stop(); } catch (_) { /* ok */ } } });
    set.nodes.push({ node: filter });
    set.nodes.push({ node: padGain });

    // Three detuned pad oscillators: root, fifth, octave.
    const padFreqs = [root, root * 1.5, root * 2];
    const detunesA = [rand(3, 8), rand(3, 8), rand(3, 8)];   // positive detune (cents)
    const detunesB = [-rand(3, 8), -rand(3, 8), -rand(3, 8)]; // negative detune (cents)

    for (let i = 0; i < padFreqs.length; i++) {
      const freq = padFreqs[i];

      // Voice A (slightly sharp).
      const oscA = ctx.createOscillator();
      const gainA = ctx.createGain();
      oscA.type = theme.padWaveform;
      oscA.frequency.value = freq * centsToRatio(detunesA[i]);
      gainA.gain.value = 0.04 + Math.random() * 0.02;
      oscA.connect(gainA);
      gainA.connect(padGain);
      oscA.start();
      set.nodes.push({ node: gainA });
      set.nodes.push({ node: oscA, stop: () => { try { oscA.stop(); } catch (_) { /* ok */ } } });

      // Voice B (slightly flat).
      const oscB = ctx.createOscillator();
      const gainB = ctx.createGain();
      oscB.type = theme.padWaveform;
      oscB.frequency.value = freq * centsToRatio(detunesB[i]);
      gainB.gain.value = 0.04 + Math.random() * 0.02;
      oscB.connect(gainB);
      gainB.connect(padGain);
      oscB.start();
      set.nodes.push({ node: gainB });
      set.nodes.push({ node: oscB, stop: () => { try { oscB.stop(); } catch (_) { /* ok */ } } });
    }
  }

  // ---------------------------------------------------------------------------
  // Melody layer (explore + combat)
  // ---------------------------------------------------------------------------

  private _scheduleMelodyLayer(
    ctx: AudioContext,
    set: LayerSet,
    theme: ZoneTheme,
    state: 'explore' | 'combat',
  ): void {
    // Capture a generation counter so stale timeouts can detect invalidation.
    const capturedSet = set;

    const scheduleNext = (): void => {
      // If this set is no longer active, stop scheduling.
      if (capturedSet !== this.activeSet) return;
      if (capturedSet.timeouts.length === 0 && capturedSet.nodes.length === 0) return;

      const isMenu = theme.id === 'menu';
      const intervalMin = state === 'combat' ? 0.5 : isMenu ? 6.0 : 2.0;
      const intervalMax = state === 'combat' ? 2.0 : isMenu ? 15.0 : 8.0;
      const intervalMs = rand(intervalMin, intervalMax) * 1000;

      const id = window.setTimeout(() => {
        // Remove this id from the set's timeout list.
        const idx = capturedSet.timeouts.indexOf(id);
        if (idx !== -1) capturedSet.timeouts.splice(idx, 1);

        // Check the set is still active before playing.
        if (capturedSet !== this.activeSet) return;

        this._playMelodyNote(ctx, capturedSet, theme, state);
        scheduleNext();
      }, intervalMs);

      capturedSet.timeouts.push(id);
    };

    scheduleNext();
  }

  private _playMelodyNote(
    ctx: AudioContext,
    set: LayerSet,
    theme: ZoneTheme,
    state: 'explore' | 'combat',
  ): void {
    const t = ctx.currentTime;
    const isMenu = theme.id === 'menu';
    const freq = isMenu ? pick(theme.scale) * 4 : pick(theme.scale); // two octaves up for menu
    const waveform: OscillatorType = Math.random() < 0.5 ? 'sine' : 'triangle';
    const duration = isMenu ? rand(2.0, 5.0) : rand(0.3, 1.5);
    const peakGain = state === 'combat' ? rand(0.06, 0.12) : isMenu ? rand(0.06, 0.10) : rand(0.04, 0.08);

    const osc = ctx.createOscillator();
    const gainNode = ctx.createGain();
    osc.type = waveform;
    osc.frequency.setValueAtTime(freq, t);

    // ADSR envelope.
    const attack = isMenu ? 0.3 : 0.02;
    const decay = duration * 0.2;
    const sustain = 0.5;
    const release = isMenu ? duration * 0.6 : duration * 0.4;
    const sustainLevel = Math.max(sustain * peakGain, 0.001);

    gainNode.gain.setValueAtTime(0, t);
    gainNode.gain.linearRampToValueAtTime(peakGain, t + attack);
    gainNode.gain.exponentialRampToValueAtTime(sustainLevel, t + attack + decay);
    gainNode.gain.exponentialRampToValueAtTime(0.0001, t + attack + decay + release);

    osc.connect(gainNode);
    gainNode.connect(set.masterGain);

    osc.start(t);
    osc.stop(t + duration);

    // These are transient — we don't track them in set.nodes since they
    // auto-stop and auto-GC, but we do need to handle forced cleanup.
    // Add them briefly; they'll be stopped automatically by the browser.
    const mn: ManagedNode = { node: osc, stop: () => { try { osc.stop(); } catch (_) { /* ok */ } } };
    set.nodes.push(mn);
    // Remove from tracking after the note finishes (avoid accumulation).
    const cleanupId = window.setTimeout(() => {
      const idx2 = set.timeouts.indexOf(cleanupId);
      if (idx2 !== -1) set.timeouts.splice(idx2, 1);
      const ni = set.nodes.indexOf(mn);
      if (ni !== -1) set.nodes.splice(ni, 1);
    }, Math.ceil(duration * 1000) + 100);
    set.timeouts.push(cleanupId);
  }

  // ---------------------------------------------------------------------------
  // Rhythm layer (combat only)
  // ---------------------------------------------------------------------------

  private _scheduleRhythmLayer(
    ctx: AudioContext,
    set: LayerSet,
    theme: ZoneTheme,
  ): void {
    const capturedSet = set;
    const beatMs = (60 / theme.tempo) * 1000;
    let beatCount = 0;

    const scheduleBeat = (): void => {
      if (capturedSet !== this.activeSet) return;

      const id = window.setTimeout(() => {
        const idx = capturedSet.timeouts.indexOf(id);
        if (idx !== -1) capturedSet.timeouts.splice(idx, 1);

        if (capturedSet !== this.activeSet) return;

        this._playRhythmBeat(ctx, capturedSet, beatCount);
        beatCount++;
        scheduleBeat();
      }, beatMs);

      capturedSet.timeouts.push(id);
    };

    scheduleBeat();
  }

  private _playRhythmBeat(ctx: AudioContext, set: LayerSet, beatIndex: number): void {
    const t = ctx.currentTime;
    const isDownbeat = beatIndex % 2 === 0;

    // Create a short noise burst — lowpass for kick feel on downbeats,
    // highpass for hi-hat feel on upbeats.
    const dur = isDownbeat ? 0.12 : 0.07;
    const filterFreq = isDownbeat ? 200 : 4000;
    const filterType: BiquadFilterType = isDownbeat ? 'lowpass' : 'highpass';
    const peakGain = isDownbeat ? 0.18 : 0.10;

    const sampleRate = ctx.sampleRate;
    const frameCount = Math.ceil(sampleRate * dur);
    const noiseBuffer = ctx.createBuffer(1, frameCount, sampleRate);
    const data = noiseBuffer.getChannelData(0);
    for (let i = 0; i < frameCount; i++) {
      data[i] = Math.random() * 2 - 1;
    }

    const source = ctx.createBufferSource();
    source.buffer = noiseBuffer;

    const filter = ctx.createBiquadFilter();
    filter.type = filterType;
    filter.frequency.setValueAtTime(filterFreq, t);

    const gainNode = ctx.createGain();
    gainNode.gain.setValueAtTime(0, t);
    gainNode.gain.linearRampToValueAtTime(peakGain, t + 0.005);
    gainNode.gain.exponentialRampToValueAtTime(0.0001, t + dur);

    source.connect(filter);
    filter.connect(gainNode);
    gainNode.connect(set.masterGain);

    source.start(t);
    source.stop(t + dur);

    // Transient — auto-stops, track briefly for forced cleanup.
    const mn: ManagedNode = { node: source, stop: () => { try { source.stop(); } catch (_) { /* ok */ } } };
    set.nodes.push(mn);
    const cleanupId = window.setTimeout(() => {
      const idx = set.timeouts.indexOf(cleanupId);
      if (idx !== -1) set.timeouts.splice(idx, 1);
      const ni = set.nodes.indexOf(mn);
      if (ni !== -1) set.nodes.splice(ni, 1);
    }, Math.ceil(dur * 1000) + 100);
    set.timeouts.push(cleanupId);
  }

  // ---------------------------------------------------------------------------
  // Chime layer (explore only)
  // ---------------------------------------------------------------------------

  private _scheduleChimeLayer(
    ctx: AudioContext,
    set: LayerSet,
    theme: ZoneTheme,
  ): void {
    const capturedSet = set;

    const isMenu = theme.id === 'menu';

    const scheduleNext = (): void => {
      if (capturedSet !== this.activeSet) return;

      const intervalMs = rand(isMenu ? 15 : 4, isMenu ? 30 : 12) * 1000;

      const id = window.setTimeout(() => {
        const idx = capturedSet.timeouts.indexOf(id);
        if (idx !== -1) capturedSet.timeouts.splice(idx, 1);

        if (capturedSet !== this.activeSet) return;

        this._playChime(ctx, capturedSet, theme);
        scheduleNext();
      }, intervalMs);

      capturedSet.timeouts.push(id);
    };

    // Initial delay before first chime.
    const initialId = window.setTimeout(() => {
      const idx = capturedSet.timeouts.indexOf(initialId);
      if (idx !== -1) capturedSet.timeouts.splice(idx, 1);
      if (capturedSet !== this.activeSet) return;
      this._playChime(ctx, capturedSet, theme);
      scheduleNext();
    }, rand(isMenu ? 5 : 2, isMenu ? 10 : 5) * 1000);

    capturedSet.timeouts.push(initialId);
  }

  private _playChime(ctx: AudioContext, set: LayerSet, theme: ZoneTheme): void {
    const t = ctx.currentTime;
    const isMenu = theme.id === 'menu';
    // Use top end of scale for ethereal feel.
    const topScale = theme.scale.slice(Math.max(0, theme.scale.length - 3));
    const freq = pick(topScale) * 2; // one octave up for chime brightness
    const duration = isMenu ? rand(1.5, 3.0) : rand(0.6, 1.2);
    const peakGain = isMenu ? rand(0.025, 0.045) : rand(0.02, 0.03);

    const osc = ctx.createOscillator();
    const gainNode = ctx.createGain();
    const filter = ctx.createBiquadFilter();

    osc.type = 'sine';
    osc.frequency.setValueAtTime(freq, t);

    filter.type = 'lowpass';
    filter.frequency.setValueAtTime(3000, t);

    const attack = 0.015;
    const decay = duration * 0.25;
    const sustain = 0.4;
    const release = duration * 0.6;
    const sustainLevel = Math.max(sustain * peakGain, 0.0001);

    gainNode.gain.setValueAtTime(0, t);
    gainNode.gain.linearRampToValueAtTime(peakGain, t + attack);
    gainNode.gain.exponentialRampToValueAtTime(sustainLevel, t + attack + decay);
    gainNode.gain.exponentialRampToValueAtTime(0.0001, t + attack + decay + release);

    osc.connect(gainNode);
    gainNode.connect(filter);
    filter.connect(set.masterGain);

    osc.start(t);
    osc.stop(t + duration);

    const mn: ManagedNode = { node: osc, stop: () => { try { osc.stop(); } catch (_) { /* ok */ } } };
    set.nodes.push(mn);
    const cleanupId = window.setTimeout(() => {
      const idx = set.timeouts.indexOf(cleanupId);
      if (idx !== -1) set.timeouts.splice(idx, 1);
      const ni = set.nodes.indexOf(mn);
      if (ni !== -1) set.nodes.splice(ni, 1);
    }, Math.ceil(duration * 1000) + 100);
    set.timeouts.push(cleanupId);
  }

  // ---------------------------------------------------------------------------
  // Victory stinger
  // ---------------------------------------------------------------------------

  private _buildVictoryStinger(
    ctx: AudioContext,
    set: LayerSet,
    theme: ZoneTheme,
  ): void {
    // Play 3-4 notes from the top of the scale simultaneously (chord),
    // ascending over 0.5 s each with staggered starts.
    const scale = theme.scale;
    const chordNotes = scale.slice(Math.max(0, scale.length - 4));
    const noteDur = 2.5;
    const staggerStep = 0.5;

    chordNotes.forEach((freq, i) => {
      const noteStart = ctx.currentTime + i * staggerStep;
      const osc = ctx.createOscillator();
      const gainNode = ctx.createGain();

      osc.type = 'sine';
      // Ascend: start one octave below, sweep to the target frequency.
      osc.frequency.setValueAtTime(freq * 0.5, noteStart);
      osc.frequency.linearRampToValueAtTime(freq, noteStart + staggerStep);

      const peakGain = 0.08;
      const attack = 0.02;
      const decay = noteDur * 0.15;
      const sustain = 0.6;
      const release = noteDur * 0.5;
      const sustainLevel = Math.max(sustain * peakGain, 0.001);

      gainNode.gain.setValueAtTime(0, noteStart);
      gainNode.gain.linearRampToValueAtTime(peakGain, noteStart + attack);
      gainNode.gain.exponentialRampToValueAtTime(sustainLevel, noteStart + attack + decay);
      gainNode.gain.exponentialRampToValueAtTime(0.0001, noteStart + attack + decay + release);

      osc.connect(gainNode);
      gainNode.connect(set.masterGain);

      osc.start(noteStart);
      osc.stop(noteStart + noteDur);

      const mn: ManagedNode = { node: osc, stop: () => { try { osc.stop(); } catch (_) { /* ok */ } } };
      set.nodes.push(mn);
      const cleanupId = window.setTimeout(() => {
        const idx = set.timeouts.indexOf(cleanupId);
        if (idx !== -1) set.timeouts.splice(idx, 1);
        const ni = set.nodes.indexOf(mn);
        if (ni !== -1) set.nodes.splice(ni, 1);
      }, Math.ceil((noteStart - ctx.currentTime + noteDur) * 1000) + 100);
      set.timeouts.push(cleanupId);
    });
  }
}
