/**
 * MusicEngine — dynamic zone-based music with a combat-aware state machine.
 *
 * Architecture:
 *   - Five zone themes (emerald_plains, twilight_forest, anvil_mountains,
 *     scorching_desert, abyss_rift) defined as ZONE_THEMES constant.
 *   - Three music states: 'explore', 'combat', 'victory'.
 *   - A composed score per zone (ZONE_SCORES → ScorePlayer): chord
 *     progression pad, light plucked bass, arpeggios, melodic phrases and a
 *     breathing 16-bar section cycle; combat adds tempo and percussion.
 *   - External AudioBuffer override: if AudioLoader has a buffer keyed
 *     'bgm_{zoneId}_{state}' it is played instead of the procedural layers.
 *   - Crossfading: state transitions 1.5 s, zone transitions 2 s.
 *   - Victory stinger + 3 s auto-return to 'explore'.
 *   - All nodes and timeouts tracked; fully cleaned up on stop/transition.
 */

import { AudioLoader } from './AudioLoader';
import { ScorePlayer, type ScoreSpec } from './ScorePlayer';
import type { EffectsChainConfig, MusicState, ZoneTheme } from './types';

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
    // Warm, natural reverb for pastoral feel
    reverbMix: 0.22,
    reverbDecay: 1.2,
  },
  twilight_forest: {
    id: 'twilight_forest',
    baseKey: 146.83,
    scale: [146.83, 164.81, 174.61, 196.00, 220.00, 246.94, 261.63],
    tempo: 60,
    padWaveform: 'triangle',
    mood: 'mysterious',
    // Longer, ethereal reverb for mystery
    reverbMix: 0.35,
    reverbDecay: 2.0,
    reverbPreDelay: 0.025,
  },
  anvil_mountains: {
    id: 'anvil_mountains',
    baseKey: 82.41,
    scale: [82.41, 92.50, 98.00, 110.00, 123.47, 130.81, 146.83],
    tempo: 80,
    padWaveform: 'sawtooth',
    mood: 'epic',
    // Large hall reverb for epic scale
    reverbMix: 0.30,
    reverbDecay: 1.8,
  },
  scorching_desert: {
    id: 'scorching_desert',
    baseKey: 220.00,
    scale: [220.00, 233.08, 261.63, 293.66, 329.63, 349.23, 392.00],
    tempo: 68,
    padWaveform: 'triangle',
    mood: 'exotic',
    // Medium reverb with character
    reverbMix: 0.25,
    reverbDecay: 1.4,
  },
  abyss_rift: {
    id: 'abyss_rift',
    baseKey: 92.50,
    scale: [92.50, 103.83, 110.00, 123.47, 138.59, 146.83, 164.81],
    tempo: 90,
    padWaveform: 'sawtooth',
    mood: 'dark',
    // Deep, dark reverb for atmosphere
    reverbMix: 0.38,
    reverbDecay: 2.5,
    reverbPreDelay: 0.030,
  },
  menu: {
    id: 'menu',
    baseKey: 65.41,       // C2 — deep fundamental
    scale: [65.41, 77.78, 87.31, 98.00, 116.54],  // C minor pentatonic
    tempo: 40,
    padWaveform: 'sawtooth',
    mood: 'dark',
    // Higher cutoff for clearer, more audible sound (was 200)
    padFilterCutoff: 550,
    padLFORate: 0.03,
    // Higher pad gain for menu presence
    padGain: 0.28,
    // Higher melody/chime gain for clarity
    melodyPeakGainMin: 0.12,
    melodyPeakGainMax: 0.18,
    chimePeakGainMin: 0.05,
    chimePeakGainMax: 0.08,
    // Balanced reverb for menu - clearer but still atmospheric
    reverbMix: 0.22,
    reverbDecay: 1.8,
    reverbPreDelay: 0.015,
  },
};

// ---------------------------------------------------------------------------
// Scores — what each zone actually plays (see ScorePlayer / Composer)
// ---------------------------------------------------------------------------

/**
 * Per-zone compositions. Registers are chosen so nothing sustains below the
 * mid range: pads sit around G3–C5, the bass only plucks on strong beats.
 */
/** Output trim for the composed score (≈ −8 dB) so it matches the old mix level. */
const SCORE_TRIM = 0.4;

export const ZONE_SCORES: Record<string, ScoreSpec> = {
  // Sunny, lilting: D major, I–V–vi–IV, flute melody over a harp arpeggio.
  emerald_plains: {
    tonic: 62, mode: 'major', progression: [0, 4, 5, 3], barsPerChord: 2, beatsPerBar: 4, tempo: 84,
    pad: { wave: 'triangle', gain: 0.05, cutoff: 1800 },
    bass: { gain: 0.05, beats: [0] },
    lead: { wave: 'sine', gain: 0.07, low: 69, high: 86, density: 0.6, vibrato: 10 },
    arp: { wave: 'triangle', gain: 0.035, low: 62, high: 79, perBeat: 2, decay: 0.5 },
    bell: { gain: 0.02 },
    combatTempo: 1.3,
  },
  // Hushed waltz: A dorian in 3/4, bell arpeggios and a soft wandering line.
  twilight_forest: {
    tonic: 57, mode: 'dorian', progression: [0, 3, 6, 4], barsPerChord: 2, beatsPerBar: 3, tempo: 76,
    pad: { wave: 'sine', gain: 0.055, cutoff: 1400 },
    bass: { gain: 0.04, beats: [0] },
    lead: { wave: 'triangle', gain: 0.055, low: 64, high: 81, density: 0.45, vibrato: 6 },
    arp: { wave: 'sine', gain: 0.03, low: 64, high: 83, perBeat: 1, decay: 1.2 },
    bell: { gain: 0.025 },
    combatTempo: 1.35,
  },
  // Dwarven march: D minor i–VI–III–VII, horn-like lead, measured bass.
  anvil_mountains: {
    tonic: 50, mode: 'minor', progression: [0, 5, 2, 6], barsPerChord: 2, beatsPerBar: 4, tempo: 76,
    pad: { wave: 'sawtooth', gain: 0.035, cutoff: 1100 },
    bass: { gain: 0.055, beats: [0, 2] },
    lead: { wave: 'sawtooth', gain: 0.045, low: 62, high: 79, density: 0.5, vibrato: 8 },
    arp: { wave: 'triangle', gain: 0.03, low: 57, high: 74, perBeat: 1, decay: 0.7 },
    bell: null,
    combatTempo: 1.3,
  },
  // Caravan song: E phrygian dominant, plucked oud arpeggios, ornamented line.
  scorching_desert: {
    tonic: 52, mode: 'phrygianDominant', progression: [0, 1, 0, 6], barsPerChord: 2, beatsPerBar: 4, tempo: 90,
    pad: { wave: 'triangle', gain: 0.045, cutoff: 1500 },
    bass: { gain: 0.05, beats: [0, 2] },
    lead: { wave: 'sawtooth', gain: 0.04, low: 64, high: 83, density: 0.7, vibrato: 14 },
    arp: { wave: 'sawtooth', gain: 0.022, low: 59, high: 76, perBeat: 2, decay: 0.35 },
    bell: null,
    combatTempo: 1.25,
  },
  // Dread and grandeur: C harmonic minor i–VI–iv–V, slow bells, sparse line.
  abyss_rift: {
    tonic: 48, mode: 'harmonicMinor', progression: [0, 5, 3, 4], barsPerChord: 2, beatsPerBar: 4, tempo: 66,
    pad: { wave: 'sawtooth', gain: 0.035, cutoff: 950 },
    bass: { gain: 0.045, beats: [0] },
    lead: { wave: 'triangle', gain: 0.05, low: 60, high: 77, density: 0.35, vibrato: 6 },
    arp: { wave: 'sine', gain: 0.028, low: 60, high: 79, perBeat: 1, decay: 1.5 },
    bell: { gain: 0.02 },
    combatTempo: 1.35,
  },
  // Title theme: A minor, slow and wistful.
  menu: {
    tonic: 57, mode: 'minor', progression: [0, 5, 2, 6], barsPerChord: 2, beatsPerBar: 4, tempo: 62,
    pad: { wave: 'triangle', gain: 0.06, cutoff: 1300 },
    bass: { gain: 0.035, beats: [0] },
    lead: { wave: 'triangle', gain: 0.055, low: 64, high: 81, density: 0.4, vibrato: 5 },
    arp: { wave: 'sine', gain: 0.022, low: 64, high: 83, perBeat: 1, decay: 1.6 },
    bell: { gain: 0.02 },
    combatTempo: 1.2,
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
// Effects chain helpers
// ---------------------------------------------------------------------------

/** Default reverb parameters based on mood. */
function reverbParams(mood: ZoneTheme['mood']): { mix: number; decay: number; preDelay: number } {
  switch (mood) {
    case 'pastoral':   return { mix: 0.22, decay: 1.2, preDelay: 0.015 };
    case 'mysterious': return { mix: 0.35, decay: 2.0, preDelay: 0.025 };
    case 'epic':       return { mix: 0.30, decay: 1.8, preDelay: 0.020 };
    case 'exotic':     return { mix: 0.25, decay: 1.4, preDelay: 0.018 };
    case 'dark':       return { mix: 0.38, decay: 2.5, preDelay: 0.030 };
  }
}

/** Generate a procedural impulse response for reverb. */
function generateReverbIR(
  ctx: AudioContext,
  duration: number,
  decay: number,
): AudioBuffer {
  const sampleRate = ctx.sampleRate;
  const length = Math.ceil(sampleRate * duration);
  const buffer = ctx.createBuffer(2, length, sampleRate);

  for (let channel = 0; channel < 2; channel++) {
    const data = buffer.getChannelData(channel);
    for (let i = 0; i < length; i++) {
      // Exponential decay envelope with some randomness
      const t = i / sampleRate;
      const envelope = Math.exp(-t * (3.0 / decay));
      // Add filtered noise for smoother reverb tail
      const noise = (Math.random() * 2 - 1) * envelope;
      data[i] = noise * (1 - (i / length) * 0.3);
    }
  }

  return buffer;
}

/** Build the effects chain: Reverb → Compressor → Limiter. */
function buildEffectsChain(
  ctx: AudioContext,
  destination: AudioNode,
  config: EffectsChainConfig,
): { input: GainNode; nodes: AudioNode[] } | null {
  try {
    const nodes: AudioNode[] = [];

    // Input gain node for dry signal
    const inputGain = ctx.createGain();
    inputGain.gain.value = 1;
    nodes.push(inputGain);

    // Low cut: nothing in the score should rumble — trims sub-bass and mud.
    const lowCut = ctx.createBiquadFilter();
    lowCut.type = 'highpass';
    lowCut.frequency.value = 75;
    lowCut.Q.value = 0.7;
    nodes.push(lowCut);
    const lowShelf = ctx.createBiquadFilter();
    lowShelf.type = 'lowshelf';
    lowShelf.frequency.value = 180;
    lowShelf.gain.value = -4;
    nodes.push(lowShelf);
    inputGain.connect(lowCut);
    lowCut.connect(lowShelf);

    // Create reverb (convolver with generated IR)
    const reverbIR = generateReverbIR(ctx, config.reverb.decay + 0.2, config.reverb.decay);
    const convolver = ctx.createConvolver();
    convolver.buffer = reverbIR;
    nodes.push(convolver);

    // Wet/dry mix using a parallel path
    const dryGain = ctx.createGain();
    dryGain.gain.value = 1 - config.reverb.mix;
    nodes.push(dryGain);

    const wetGain = ctx.createGain();
    wetGain.gain.value = config.reverb.mix;
    nodes.push(wetGain);

    // Pre-delay for reverb
    const preDelayNode = ctx.createDelay(config.reverb.preDelay + 0.1);
    preDelayNode.delayTime.value = config.reverb.preDelay;
    nodes.push(preDelayNode);

    // Compressor for loudness stabilization
    const compressor = ctx.createDynamicsCompressor();
    compressor.threshold.value = config.compressor.threshold;
    compressor.knee.value = config.compressor.knee;
    compressor.ratio.value = config.compressor.ratio;
    compressor.attack.value = config.compressor.attack;
    compressor.release.value = config.compressor.release;
    nodes.push(compressor);

    // Limiter (high-ratio compressor) to prevent clipping
    const limiter = ctx.createDynamicsCompressor();
    limiter.threshold.value = config.limiter.threshold;
    limiter.knee.value = 0;
    limiter.ratio.value = 20;  // High ratio for limiting
    limiter.attack.value = 0.001;
    limiter.release.value = config.limiter.release;
    nodes.push(limiter);

    // Connect the chain:
    // inputGain → lowCut → lowShelf → dryGain ──────────┐
    //           → preDelay → convolver → wetGain → compressor → limiter → destination
    lowShelf.connect(dryGain);
    lowShelf.connect(preDelayNode);
    preDelayNode.connect(convolver);
    convolver.connect(wetGain);
    wetGain.connect(compressor);
    dryGain.connect(compressor);
    compressor.connect(limiter);
    limiter.connect(destination);

    return { input: inputGain, nodes };
  } catch {
    // Effects creation failed - return null for fallback
    return null;
  }
}

/** Build effects config from ZoneTheme with defaults. */
function buildEffectsConfig(theme: ZoneTheme): EffectsChainConfig {
  const baseReverb = reverbParams(theme.mood);
  return {
    reverb: {
      mix: theme.reverbMix ?? baseReverb.mix,
      decay: theme.reverbDecay ?? baseReverb.decay,
      preDelay: theme.reverbPreDelay ?? baseReverb.preDelay,
    },
    compressor: {
      threshold: theme.compressorThreshold ?? -18,
      knee: theme.compressorKnee ?? 6,
      ratio: theme.compressorRatio ?? 4,
      attack: theme.compressorAttack ?? 0.003,
      release: theme.compressorRelease ?? 0.25,
    },
    limiter: {
      threshold: -1,
      release: 0.1,
    },
  };
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
  /** Effects chain nodes for cleanup. */
  private effectsNodes: AudioNode[] = [];
  /** Effects chain input gain. */
  private effectsInput: GainNode | null = null;

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
  setZone(ctx: AudioContext, destination: AudioNode, zoneId: string, force: boolean = false): void {
    const zoneChanged = this.currentZone !== zoneId;
    if (!force && !zoneChanged) return;
    this.currentZone = zoneId;
    if (zoneChanged) this.currentState = 'explore';
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

    // Clean up effects chain
    this._cleanupEffectsChain();
  }

  /** Adjust master volume (0–1). */
  setVolume(v: number): void {
    if (this.masterGain) this.masterGain.gain.value = v;
  }

  /** Rebuild the current zone + state layers after external buffers finish loading. */
  refresh(ctx: AudioContext, destination: AudioNode): void {
    if (!this.currentZone) return;
    this._transition(ctx, destination, 1.0);
  }

  /** Play a specific zone+state combination in one transition (for jukebox). */
  playZoneState(ctx: AudioContext, destination: AudioNode, zoneId: string, state: MusicState): void {
    if (this.victoryTimer !== null) {
      clearTimeout(this.victoryTimer);
      this.victoryTimer = null;
    }
    this.currentZone = zoneId;
    this.currentState = state;
    this._transition(ctx, destination, 1.5);
  }

  // ---------------------------------------------------------------------------
  // Transition core
  // ---------------------------------------------------------------------------

  private _transition(ctx: AudioContext, destination: AudioNode, duration: number): void {
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

    // Abyss Labyrinth floors are generated per run; they share the rift's score.
    // The Ember Tower (homestead) stands on the plains and plays their score.
    const theme = ZONE_THEMES[zoneId] ?? (zoneId.startsWith('dungeon_floor_') ? ZONE_THEMES.abyss_rift
      : zoneId === 'ember_tower' ? ZONE_THEMES.emerald_plains : undefined);
    if (!theme) {
      if (oldSet) this._fadeOutAndDestroy(ctx, oldSet, duration);
      return;
    }

    // Build effects chain for this zone (with fallback)
    const config = buildEffectsConfig(theme);
    let finalDestination: AudioNode = destination;

    // Clean up old effects chain
    this._cleanupEffectsChain();

    // Build new effects chain
    const effects = buildEffectsChain(ctx, destination, config);
    if (effects) {
      this.effectsInput = effects.input;
      this.effectsNodes = effects.nodes;
      finalDestination = effects.input;
    }

    // Ensure master gain exists and connect to effects chain or destination.
    if (!this.masterGain) {
      this.masterGain = ctx.createGain();
      this.masterGain.gain.value = 1;
    }
    this.masterGain.connect(finalDestination);

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

  /** Clean up effects chain nodes. */
  private _cleanupEffectsChain(): void {
    for (const node of this.effectsNodes) {
      try { node.disconnect(); } catch { /* already disconnected */ }
    }
    this.effectsNodes = [];
    this.effectsInput = null;
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

    if (state === 'victory') {
      this._buildVictoryStinger(ctx, set, theme);
      return set;
    }

    // The composed score, scheduled with lookahead on the audio clock.
    const score = ZONE_SCORES[theme.id] ?? ZONE_SCORES.menu;
    // The score is fuller than the old drone; trim it to sit at the same level under the SFX.
    const trim = ctx.createGain();
    trim.gain.value = SCORE_TRIM;
    trim.connect(set.masterGain);
    set.nodes.push({ node: trim });
    const player = new ScorePlayer(ctx, trim, score, state, ctx.currentTime + 0.1, Math.floor(Math.random() * 1e9));
    const tick = (): void => player.scheduleUntil(ctx.currentTime + 0.4);
    tick();
    // clearTimeout() cancels intervals too, so the existing fade-out cleanup applies.
    set.timeouts.push(window.setInterval(tick, 100));
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
