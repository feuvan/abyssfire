/**
 * SFXEngine — procedural sound effects synthesis via Web Audio API.
 *
 * Each SFX is synthesized in real-time from oscillators, noise bursts, and
 * gain envelopes.  When an external AudioBuffer for the effect is present in
 * AudioLoader the buffer is played directly instead (asset-override pipeline).
 *
 * All gain peaks are kept in the 0.1-0.4 range; the master SFX gain node
 * (owned by AudioManager) handles overall volume.
 */

import { AudioLoader } from './AudioLoader';
import type { SFXType } from './types';

export class SFXEngine {
  private loader: AudioLoader;

  constructor(loader: AudioLoader) {
    this.loader = loader;
  }

  // ---------------------------------------------------------------------------
  // Public API
  // ---------------------------------------------------------------------------

  play(ctx: AudioContext, destination: AudioNode, type: SFXType): void {
    const buffer = this.loader.getBuffer(`sfx_${type}`);
    if (buffer) {
      const source = ctx.createBufferSource();
      source.buffer = buffer;
      source.connect(destination);
      source.start();
      return;
    }
    this.playProcedural(ctx, destination, type);
  }

  // ---------------------------------------------------------------------------
  // Dispatch
  // ---------------------------------------------------------------------------

  private playProcedural(ctx: AudioContext, destination: AudioNode, type: SFXType): void {
    const t = ctx.currentTime;
    switch (type) {
      // --- Combat ---
      case 'hit':           this.sfxHit(ctx, destination, t); break;
      case 'hit_heavy':     this.sfxHitHeavy(ctx, destination, t); break;
      case 'crit':          this.sfxCrit(ctx, destination, t); break;
      case 'miss':          this.sfxMiss(ctx, destination, t); break;
      case 'block':         this.sfxBlock(ctx, destination, t); break;
      case 'player_hurt':   this.sfxPlayerHurt(ctx, destination, t); break;
      case 'monster_death': this.sfxMonsterDeath(ctx, destination, t); break;
      case 'player_death':  this.sfxPlayerDeath(ctx, destination, t); break;
      case 'dodge':          this.sfxDodge(ctx, destination, t); break;
      case 'resonance':      this.sfxResonance(ctx, destination, t); break;
      // --- Skills ---
      case 'skill_melee':     this.sfxSkillMelee(ctx, destination, t); break;
      case 'skill_fire':      this.sfxSkillFire(ctx, destination, t); break;
      case 'skill_ice':       this.sfxSkillIce(ctx, destination, t); break;
      case 'skill_lightning': this.sfxSkillLightning(ctx, destination, t); break;
      case 'skill_heal':      this.sfxSkillHeal(ctx, destination, t); break;
      case 'skill_buff':      this.sfxSkillBuff(ctx, destination, t); break;
      // --- Loot / Items ---
      case 'loot_common':    this.sfxLootCommon(ctx, destination, t); break;
      case 'loot_magic':     this.sfxLootMagic(ctx, destination, t); break;
      case 'loot_rare':      this.sfxLootRare(ctx, destination, t); break;
      case 'loot_legendary': this.sfxLootLegendary(ctx, destination, t); break;
      case 'equip':          this.sfxEquip(ctx, destination, t); break;
      case 'potion':         this.sfxPotion(ctx, destination, t); break;
      // --- UI ---
      case 'click':       this.sfxClick(ctx, destination, t); break;
      case 'panel_open':  this.sfxPanelOpen(ctx, destination, t); break;
      case 'panel_close': this.sfxPanelClose(ctx, destination, t); break;
      case 'error':       this.sfxError(ctx, destination, t); break;
      // --- World ---
      case 'zone_transition': this.sfxZoneTransition(ctx, destination, t); break;
      case 'quest_complete':  this.sfxQuestComplete(ctx, destination, t); break;
      case 'levelup':         this.sfxLevelup(ctx, destination, t); break;
      case 'npc_interact':    this.sfxNpcInteract(ctx, destination, t); break;
    }
  }

  // ---------------------------------------------------------------------------
  // Core utilities
  // ---------------------------------------------------------------------------

  /**
   * Apply an ADSR envelope to an AudioParam (typically a GainNode.gain).
   * sustain is expressed as a ratio of peak (0-1).
   */
  private createADSR(
    ctx: AudioContext,
    param: AudioParam,
    a: number,
    d: number,
    s: number,
    r: number,
    peak: number,
    startTime: number,
  ): void {
    const sustainLevel = Math.max(s * peak, 0.001);
    param.setValueAtTime(0, startTime);
    param.linearRampToValueAtTime(peak, startTime + a);
    param.exponentialRampToValueAtTime(sustainLevel, startTime + a + d);
    param.exponentialRampToValueAtTime(0.001, startTime + a + d + r);
  }

  /**
   * Filtered white-noise burst.  The noise connects through a BiquadFilter
   * then a GainNode to destination and auto-stops after duration.
   */
  private createNoiseBurst(
    ctx: AudioContext,
    destination: AudioNode,
    duration: number,
    filterFreq: number,
    filterType: BiquadFilterType,
    startTime: number,
    peak: number = 0.25,
  ): void {
    const sampleRate = ctx.sampleRate;
    const frameCount = Math.ceil(sampleRate * duration);
    const buffer = ctx.createBuffer(1, frameCount, sampleRate);
    const data = buffer.getChannelData(0);
    for (let i = 0; i < frameCount; i++) {
      data[i] = Math.random() * 2 - 1;
    }

    const source = ctx.createBufferSource();
    source.buffer = buffer;

    const filter = ctx.createBiquadFilter();
    filter.type = filterType;
    filter.frequency.setValueAtTime(filterFreq, startTime);

    const gainNode = ctx.createGain();
    this.createADSR(ctx, gainNode.gain, 0.005, duration * 0.3, 0.3, duration * 0.5, peak, startTime);

    source.connect(filter);
    filter.connect(gainNode);
    gainNode.connect(destination);

    source.start(startTime);
    source.stop(startTime + duration);
  }

  /**
   * Simple oscillator helper — creates one oscillator + gain and auto-stops.
   */
  private createTone(
    ctx: AudioContext,
    destination: AudioNode,
    freq: number,
    waveform: OscillatorType,
    duration: number,
    gain: number,
    startTime: number,
  ): void {
    const osc = ctx.createOscillator();
    const gainNode = ctx.createGain();

    osc.type = waveform;
    osc.frequency.setValueAtTime(freq, startTime);

    gainNode.gain.setValueAtTime(gain, startTime);

    osc.connect(gainNode);
    gainNode.connect(destination);

    osc.start(startTime);
    osc.stop(startTime + duration);
  }

  // ---------------------------------------------------------------------------
  // Combat SFX
  // ---------------------------------------------------------------------------

  /** Airy phase-shift whoosh for a directional dodge. */
  private sfxDodge(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.22;
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'triangle';
    osc.frequency.setValueAtTime(180, t);
    osc.frequency.exponentialRampToValueAtTime(920, t + dur * 0.7);
    osc.frequency.exponentialRampToValueAtTime(420, t + dur);
    this.createADSR(ctx, gain.gain, 0.004, 0.035, 0.18, 0.12, 0.16, t);
    osc.connect(gain);
    gain.connect(destination);
    osc.start(t);
    osc.stop(t + dur);
    this.createNoiseBurst(ctx, destination, dur * 0.8, 2600, 'highpass', t, 0.12);
  }

  /** Short three-note harmonic flare when a Spirit meter enters Resonance. */
  private sfxResonance(ctx: AudioContext, destination: AudioNode, t: number): void {
    const notes = [330, 495, 660];
    notes.forEach((frequency, index) => {
      this.createTone(
        ctx,
        destination,
        frequency,
        'sine',
        0.5,
        0.09 - index * 0.015,
        t + index * 0.055,
      );
    });
    this.createNoiseBurst(ctx, destination, 0.28, 3200, 'bandpass', t, 0.08);
  }

  /** Metallic slash — sawtooth sweep 200→80 Hz + highpass noise burst. 0.15 s */
  private sfxHit(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.2;

    // Layer 1: metallic slash — sawtooth through bandpass for body
    const osc1 = ctx.createOscillator();
    const bp = ctx.createBiquadFilter();
    bp.type = 'bandpass'; bp.frequency.value = 1200; bp.Q.value = 2;
    const g1 = ctx.createGain();
    osc1.type = 'sawtooth';
    osc1.frequency.setValueAtTime(250, t);
    osc1.frequency.exponentialRampToValueAtTime(80, t + dur);
    this.createADSR(ctx, g1.gain, 0.002, 0.04, 0.15, 0.12, 0.25, t);
    osc1.connect(bp); bp.connect(g1); g1.connect(destination);
    osc1.start(t); osc1.stop(t + dur);

    // Layer 2: sub-bass thump for weight
    const sub = ctx.createOscillator();
    const gSub = ctx.createGain();
    sub.type = 'sine';
    sub.frequency.setValueAtTime(100, t);
    sub.frequency.exponentialRampToValueAtTime(40, t + 0.08);
    this.createADSR(ctx, gSub.gain, 0.001, 0.03, 0.1, 0.06, 0.2, t);
    sub.connect(gSub); gSub.connect(destination);
    sub.start(t); sub.stop(t + 0.1);

    // Layer 3: metallic ring overtone
    const ring = ctx.createOscillator();
    const gRing = ctx.createGain();
    const ringFilter = ctx.createBiquadFilter();
    ringFilter.type = 'bandpass'; ringFilter.frequency.value = 3500; ringFilter.Q.value = 8;
    ring.type = 'square';
    ring.frequency.setValueAtTime(2200, t);
    ring.frequency.exponentialRampToValueAtTime(1800, t + dur);
    this.createADSR(ctx, gRing.gain, 0.001, 0.02, 0.08, 0.15, 0.06, t);
    ring.connect(ringFilter); ringFilter.connect(gRing); gRing.connect(destination);
    ring.start(t); ring.stop(t + dur);

    // Layer 4: impact noise
    this.createNoiseBurst(ctx, destination, 0.08, 2500, 'highpass', t, 0.2);
  }

  /** Deeper metallic slash — sawtooth 150→50 Hz + square 100→40 Hz + noise. 0.25 s */
  private sfxHitHeavy(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.35;

    // Layer 1: deep sawtooth body through lowpass
    const osc1 = ctx.createOscillator();
    const lp = ctx.createBiquadFilter();
    lp.type = 'lowpass'; lp.frequency.setValueAtTime(600, t); lp.frequency.exponentialRampToValueAtTime(200, t + dur); lp.Q.value = 3;
    const g1 = ctx.createGain();
    osc1.type = 'sawtooth';
    osc1.frequency.setValueAtTime(150, t);
    osc1.frequency.exponentialRampToValueAtTime(40, t + dur);
    this.createADSR(ctx, g1.gain, 0.002, 0.06, 0.25, 0.2, 0.3, t);
    osc1.connect(lp); lp.connect(g1); g1.connect(destination);
    osc1.start(t); osc1.stop(t + dur);

    // Layer 2: distorted square mid-range
    const osc2 = ctx.createOscillator();
    const shaper = ctx.createWaveShaper();
    const curve = new Float32Array(256);
    for (let i = 0; i < 256; i++) { const x = (i / 128) - 1; curve[i] = Math.tanh(x * 3); }
    shaper.curve = curve;
    const g2 = ctx.createGain();
    osc2.type = 'square';
    osc2.frequency.setValueAtTime(120, t);
    osc2.frequency.exponentialRampToValueAtTime(35, t + dur);
    this.createADSR(ctx, g2.gain, 0.002, 0.08, 0.2, 0.2, 0.12, t);
    osc2.connect(shaper); shaper.connect(g2); g2.connect(destination);
    osc2.start(t); osc2.stop(t + dur);

    // Layer 3: sub-bass sine punch
    const sub = ctx.createOscillator();
    const gSub = ctx.createGain();
    sub.type = 'sine';
    sub.frequency.setValueAtTime(80, t);
    sub.frequency.exponentialRampToValueAtTime(25, t + 0.12);
    this.createADSR(ctx, gSub.gain, 0.001, 0.04, 0.1, 0.08, 0.25, t);
    sub.connect(gSub); gSub.connect(destination);
    sub.start(t); sub.stop(t + 0.15);

    // Layer 4: crunchy noise impact
    this.createNoiseBurst(ctx, destination, 0.1, 800, 'lowpass', t, 0.25);
    this.createNoiseBurst(ctx, destination, 0.06, 3000, 'highpass', t, 0.12);
  }

  /** Sharp high impact — square 600→100 Hz + sawtooth 900→200 Hz + noise. 0.25 s */
  private sfxCrit(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.35;

    // Layer 1: aggressive square attack through distortion
    const osc1 = ctx.createOscillator();
    const shaper = ctx.createWaveShaper();
    const curve = new Float32Array(256);
    for (let i = 0; i < 256; i++) { const x = (i / 128) - 1; curve[i] = Math.tanh(x * 4); }
    shaper.curve = curve;
    const bp1 = ctx.createBiquadFilter();
    bp1.type = 'bandpass'; bp1.frequency.value = 1800; bp1.Q.value = 1.5;
    const g1 = ctx.createGain();
    osc1.type = 'square';
    osc1.frequency.setValueAtTime(700, t);
    osc1.frequency.exponentialRampToValueAtTime(100, t + 0.15);
    this.createADSR(ctx, g1.gain, 0.001, 0.04, 0.15, 0.2, 0.3, t);
    osc1.connect(shaper); shaper.connect(bp1); bp1.connect(g1); g1.connect(destination);
    osc1.start(t); osc1.stop(t + dur);

    // Layer 2: bright sawtooth harmonic screech
    const osc2 = ctx.createOscillator();
    const hp = ctx.createBiquadFilter();
    hp.type = 'highpass'; hp.frequency.value = 800;
    const g2 = ctx.createGain();
    osc2.type = 'sawtooth';
    osc2.frequency.setValueAtTime(1100, t);
    osc2.frequency.exponentialRampToValueAtTime(250, t + 0.12);
    this.createADSR(ctx, g2.gain, 0.001, 0.03, 0.1, 0.15, 0.18, t);
    osc2.connect(hp); hp.connect(g2); g2.connect(destination);
    osc2.start(t); osc2.stop(t + 0.2);

    // Layer 3: sub-bass impact
    const sub = ctx.createOscillator();
    const gSub = ctx.createGain();
    sub.type = 'sine';
    sub.frequency.setValueAtTime(120, t);
    sub.frequency.exponentialRampToValueAtTime(30, t + 0.1);
    this.createADSR(ctx, gSub.gain, 0.001, 0.03, 0.08, 0.07, 0.25, t);
    sub.connect(gSub); gSub.connect(destination);
    sub.start(t); sub.stop(t + 0.12);

    // Layer 4: metallic ring resonance
    const ring = ctx.createOscillator();
    const gRing = ctx.createGain();
    const ringBp = ctx.createBiquadFilter();
    ringBp.type = 'bandpass'; ringBp.frequency.value = 4000; ringBp.Q.value = 12;
    ring.type = 'square';
    ring.frequency.setValueAtTime(3200, t);
    ring.frequency.exponentialRampToValueAtTime(2400, t + dur);
    this.createADSR(ctx, gRing.gain, 0.001, 0.02, 0.06, 0.25, 0.04, t);
    ring.connect(ringBp); ringBp.connect(gRing); gRing.connect(destination);
    ring.start(t); ring.stop(t + dur);

    // Layer 5: sharp noise crack + tail
    this.createNoiseBurst(ctx, destination, 0.06, 4000, 'highpass', t, 0.3);
    this.createNoiseBurst(ctx, destination, 0.15, 1500, 'bandpass', t + 0.05, 0.08);
  }

  /** Whoosh — sine 400→150 Hz gentle sweep. Quiet. 0.2 s */
  private sfxMiss(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.2;
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'sine';
    osc.frequency.setValueAtTime(400, t);
    osc.frequency.exponentialRampToValueAtTime(150, t + dur);
    this.createADSR(ctx, gain.gain, 0.01, 0.05, 0.3, 0.1, 0.1, t);
    osc.connect(gain);
    gain.connect(destination);
    osc.start(t);
    osc.stop(t + dur);
  }

  /** Hard clang — square 300 Hz short burst + highpass noise. 0.15 s */
  private sfxBlock(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.2;

    // Layer 1: metallic clang with resonant bandpass
    const osc = ctx.createOscillator();
    const bp = ctx.createBiquadFilter();
    bp.type = 'bandpass'; bp.frequency.value = 2000; bp.Q.value = 8;
    const g = ctx.createGain();
    osc.type = 'square';
    osc.frequency.setValueAtTime(350, t);
    osc.frequency.exponentialRampToValueAtTime(250, t + dur);
    this.createADSR(ctx, g.gain, 0.001, 0.03, 0.12, 0.12, 0.25, t);
    osc.connect(bp); bp.connect(g); g.connect(destination);
    osc.start(t); osc.stop(t + dur);

    // Layer 2: shield ring — high resonant tone
    const ring = ctx.createOscillator();
    const gRing = ctx.createGain();
    const ringBp = ctx.createBiquadFilter();
    ringBp.type = 'bandpass'; ringBp.frequency.value = 3500; ringBp.Q.value = 12;
    ring.type = 'triangle';
    ring.frequency.setValueAtTime(3200, t);
    ring.frequency.exponentialRampToValueAtTime(2800, t + dur);
    this.createADSR(ctx, gRing.gain, 0.001, 0.02, 0.08, 0.15, 0.06, t);
    ring.connect(ringBp); ringBp.connect(gRing); gRing.connect(destination);
    ring.start(t); ring.stop(t + dur);

    // Layer 3: sharp impact noise
    this.createNoiseBurst(ctx, destination, 0.04, 4000, 'highpass', t, 0.25);
  }

  /** Dull thud — sine 150→60 Hz + lowpass noise burst. 0.2 s */
  private sfxPlayerHurt(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.25;

    // Layer 1: body impact — sine thump with lowpass filter
    const osc = ctx.createOscillator();
    const lp = ctx.createBiquadFilter();
    lp.type = 'lowpass'; lp.frequency.value = 300; lp.Q.value = 3;
    const g = ctx.createGain();
    osc.type = 'sine';
    osc.frequency.setValueAtTime(180, t);
    osc.frequency.exponentialRampToValueAtTime(50, t + dur);
    this.createADSR(ctx, g.gain, 0.002, 0.05, 0.2, 0.15, 0.3, t);
    osc.connect(lp); lp.connect(g); g.connect(destination);
    osc.start(t); osc.stop(t + dur);

    // Layer 2: pain — distorted mid tone
    const osc2 = ctx.createOscillator();
    const shaper = ctx.createWaveShaper();
    const curve = new Float32Array(256);
    for (let i = 0; i < 256; i++) { const x = (i / 128) - 1; curve[i] = Math.tanh(x * 2); }
    shaper.curve = curve;
    const g2 = ctx.createGain();
    osc2.type = 'sawtooth';
    osc2.frequency.setValueAtTime(250, t);
    osc2.frequency.exponentialRampToValueAtTime(100, t + dur);
    this.createADSR(ctx, g2.gain, 0.002, 0.04, 0.15, 0.15, 0.1, t);
    osc2.connect(shaper); shaper.connect(g2); g2.connect(destination);
    osc2.start(t); osc2.stop(t + dur);

    // Layer 3: dull impact noise
    this.createNoiseBurst(ctx, destination, 0.08, 400, 'lowpass', t, 0.22);
  }

  /** Descending groan — sawtooth 200→40 Hz slow sweep + noise fade. 0.6 s */
  private sfxMonsterDeath(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.6;

    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'sawtooth';
    osc.frequency.setValueAtTime(200, t);
    osc.frequency.exponentialRampToValueAtTime(40, t + dur);
    this.createADSR(ctx, gain.gain, 0.01, 0.1, 0.4, 0.4, 0.25, t);
    osc.connect(gain);
    gain.connect(destination);
    osc.start(t);
    osc.stop(t + dur);

    this.createNoiseBurst(ctx, destination, dur, 600, 'lowpass', t, 0.18);
  }

  /** Dark descending — sawtooth 300→40 Hz very slow + detuned second oscillator. 1.2 s */
  private sfxPlayerDeath(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 1.2;

    // Primary
    const osc1 = ctx.createOscillator();
    const g1 = ctx.createGain();
    osc1.type = 'sawtooth';
    osc1.frequency.setValueAtTime(300, t);
    osc1.frequency.exponentialRampToValueAtTime(40, t + dur);
    this.createADSR(ctx, g1.gain, 0.02, 0.15, 0.5, 0.8, 0.25, t);
    osc1.connect(g1);
    g1.connect(destination);
    osc1.start(t);
    osc1.stop(t + dur);

    // Detuned second
    const osc2 = ctx.createOscillator();
    const g2 = ctx.createGain();
    osc2.type = 'sawtooth';
    osc2.frequency.setValueAtTime(285, t);
    osc2.frequency.exponentialRampToValueAtTime(38, t + dur);
    this.createADSR(ctx, g2.gain, 0.02, 0.2, 0.4, 0.9, 0.18, t);
    osc2.connect(g2);
    g2.connect(destination);
    osc2.start(t);
    osc2.stop(t + dur);
  }

  // ---------------------------------------------------------------------------
  // Skill SFX
  // ---------------------------------------------------------------------------

  /** Sword swing — triangle 300→600→200 Hz sweep + noise. 0.3 s */
  private sfxSkillMelee(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.35;

    // Layer 1: whoosh sweep — triangle through bandpass
    const osc1 = ctx.createOscillator();
    const bp = ctx.createBiquadFilter();
    bp.type = 'bandpass'; bp.frequency.setValueAtTime(800, t); bp.frequency.exponentialRampToValueAtTime(400, t + dur); bp.Q.value = 2;
    const g1 = ctx.createGain();
    osc1.type = 'triangle';
    osc1.frequency.setValueAtTime(250, t);
    osc1.frequency.linearRampToValueAtTime(700, t + 0.06);
    osc1.frequency.exponentialRampToValueAtTime(150, t + dur);
    this.createADSR(ctx, g1.gain, 0.003, 0.05, 0.2, 0.2, 0.28, t);
    osc1.connect(bp); bp.connect(g1); g1.connect(destination);
    osc1.start(t); osc1.stop(t + dur);

    // Layer 2: metallic impact at peak of swing
    const impact = ctx.createOscillator();
    const gImp = ctx.createGain();
    const impBp = ctx.createBiquadFilter();
    impBp.type = 'bandpass'; impBp.frequency.value = 2500; impBp.Q.value = 5;
    impact.type = 'sawtooth';
    impact.frequency.setValueAtTime(1500, t + 0.05);
    impact.frequency.exponentialRampToValueAtTime(400, t + 0.15);
    this.createADSR(ctx, gImp.gain, 0.001, 0.03, 0.1, 0.12, 0.12, t + 0.05);
    impact.connect(impBp); impBp.connect(gImp); gImp.connect(destination);
    impact.start(t + 0.05); impact.stop(t + 0.2);

    // Layer 3: wind noise whoosh
    this.createNoiseBurst(ctx, destination, 0.15, 1200, 'bandpass', t, 0.2);
    // Layer 4: sharp impact noise
    this.createNoiseBurst(ctx, destination, 0.04, 3000, 'highpass', t + 0.05, 0.15);
  }

  /** Crackling fire — sawtooth 200→400 Hz + bandpass noise at 1000 Hz. 0.4 s */
  private sfxSkillFire(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.5;

    // Layer 1: roaring low flame — sawtooth through lowpass with rising cutoff
    const osc1 = ctx.createOscillator();
    const lp = ctx.createBiquadFilter();
    lp.type = 'lowpass'; lp.frequency.setValueAtTime(300, t); lp.frequency.linearRampToValueAtTime(1200, t + 0.15); lp.frequency.exponentialRampToValueAtTime(400, t + dur); lp.Q.value = 4;
    const g1 = ctx.createGain();
    osc1.type = 'sawtooth';
    osc1.frequency.setValueAtTime(120, t);
    osc1.frequency.linearRampToValueAtTime(300, t + 0.1);
    osc1.frequency.exponentialRampToValueAtTime(80, t + dur);
    this.createADSR(ctx, g1.gain, 0.005, 0.08, 0.3, 0.3, 0.22, t);
    osc1.connect(lp); lp.connect(g1); g1.connect(destination);
    osc1.start(t); osc1.stop(t + dur);

    // Layer 2: crackle — high sawtooth with fast LFO modulation
    const osc2 = ctx.createOscillator();
    const lfo = ctx.createOscillator();
    const lfoGain = ctx.createGain();
    lfo.type = 'square'; lfo.frequency.value = 30;
    lfoGain.gain.value = 200;
    lfo.connect(lfoGain); lfoGain.connect(osc2.frequency);
    const bp = ctx.createBiquadFilter();
    bp.type = 'bandpass'; bp.frequency.value = 2000; bp.Q.value = 3;
    const g2 = ctx.createGain();
    osc2.type = 'sawtooth';
    osc2.frequency.setValueAtTime(400, t);
    this.createADSR(ctx, g2.gain, 0.01, 0.1, 0.2, 0.3, 0.1, t);
    osc2.connect(bp); bp.connect(g2); g2.connect(destination);
    osc2.start(t); osc2.stop(t + dur);
    lfo.start(t); lfo.stop(t + dur);

    // Layer 3: fire whoosh noise
    this.createNoiseBurst(ctx, destination, dur * 0.8, 800, 'bandpass', t, 0.25);
    // Layer 4: bright crackle noise
    this.createNoiseBurst(ctx, destination, 0.15, 4000, 'highpass', t + 0.05, 0.1);
  }

  /** Crystal shimmer — sine 800→1200→600 Hz + highpass noise at 3000 Hz. 0.35 s */
  private sfxSkillIce(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.45;

    // Layer 1: crystalline shimmer — detuned sine pair through highpass
    const freqs = [900, 907]; // slight detune for shimmer
    for (const freq of freqs) {
      const osc = ctx.createOscillator();
      const hp = ctx.createBiquadFilter();
      hp.type = 'highpass'; hp.frequency.value = 600;
      const g = ctx.createGain();
      osc.type = 'sine';
      osc.frequency.setValueAtTime(freq, t);
      osc.frequency.linearRampToValueAtTime(freq * 1.4, t + 0.12);
      osc.frequency.exponentialRampToValueAtTime(freq * 0.7, t + dur);
      this.createADSR(ctx, g.gain, 0.005, 0.06, 0.3, 0.25, 0.15, t);
      osc.connect(hp); hp.connect(g); g.connect(destination);
      osc.start(t); osc.stop(t + dur);
    }

    // Layer 2: glass resonance — triangle at high frequency with narrow bandpass
    const glass = ctx.createOscillator();
    const glassBp = ctx.createBiquadFilter();
    glassBp.type = 'bandpass'; glassBp.frequency.value = 5000; glassBp.Q.value = 15;
    const gGlass = ctx.createGain();
    glass.type = 'triangle';
    glass.frequency.setValueAtTime(4500, t);
    glass.frequency.exponentialRampToValueAtTime(3000, t + dur);
    this.createADSR(ctx, gGlass.gain, 0.002, 0.04, 0.15, 0.3, 0.04, t);
    glass.connect(glassBp); glassBp.connect(gGlass); gGlass.connect(destination);
    glass.start(t); glass.stop(t + dur);

    // Layer 3: ice crack noise
    this.createNoiseBurst(ctx, destination, 0.08, 5000, 'highpass', t + 0.02, 0.18);
    // Layer 4: sustained frost hiss
    this.createNoiseBurst(ctx, destination, dur * 0.6, 3000, 'highpass', t + 0.05, 0.08);
  }

  /** Electric zap — square 1000→200 Hz fast + white noise burst. 0.3 s */
  private sfxSkillLightning(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.4;

    // Layer 1: electric buzz — square with rapid frequency jitter via LFO
    const osc1 = ctx.createOscillator();
    const lfo = ctx.createOscillator();
    const lfoG = ctx.createGain();
    lfo.type = 'sawtooth'; lfo.frequency.value = 60;
    lfoG.gain.value = 500;
    lfo.connect(lfoG); lfoG.connect(osc1.frequency);
    const shaper = ctx.createWaveShaper();
    const curve = new Float32Array(256);
    for (let i = 0; i < 256; i++) { const x = (i / 128) - 1; curve[i] = Math.sign(x) * Math.pow(Math.abs(x), 0.5); }
    shaper.curve = curve;
    const g1 = ctx.createGain();
    osc1.type = 'square';
    osc1.frequency.setValueAtTime(1200, t);
    osc1.frequency.exponentialRampToValueAtTime(150, t + dur);
    this.createADSR(ctx, g1.gain, 0.001, 0.03, 0.2, 0.3, 0.2, t);
    osc1.connect(shaper); shaper.connect(g1); g1.connect(destination);
    osc1.start(t); osc1.stop(t + dur);
    lfo.start(t); lfo.stop(t + dur);

    // Layer 2: bright zap — high frequency descending
    const osc2 = ctx.createOscillator();
    const hp = ctx.createBiquadFilter();
    hp.type = 'highpass'; hp.frequency.value = 2000;
    const g2 = ctx.createGain();
    osc2.type = 'sawtooth';
    osc2.frequency.setValueAtTime(3000, t);
    osc2.frequency.exponentialRampToValueAtTime(500, t + 0.1);
    this.createADSR(ctx, g2.gain, 0.001, 0.02, 0.05, 0.08, 0.12, t);
    osc2.connect(hp); hp.connect(g2); g2.connect(destination);
    osc2.start(t); osc2.stop(t + 0.15);

    // Layer 3: thunder sub
    const sub = ctx.createOscillator();
    const gSub = ctx.createGain();
    sub.type = 'sine';
    sub.frequency.setValueAtTime(80, t + 0.02);
    sub.frequency.exponentialRampToValueAtTime(30, t + 0.15);
    this.createADSR(ctx, gSub.gain, 0.002, 0.04, 0.1, 0.1, 0.2, t + 0.02);
    sub.connect(gSub); gSub.connect(destination);
    sub.start(t + 0.02); sub.stop(t + 0.18);

    // Layer 4: crackling noise
    this.createNoiseBurst(ctx, destination, 0.08, 6000, 'highpass', t, 0.3);
    this.createNoiseBurst(ctx, destination, 0.15, 2000, 'bandpass', t + 0.05, 0.12);
  }

  /** Warm ascending — sine arpeggio [400, 500, 600, 800] with overlap. 0.5 s */
  private sfxSkillHeal(ctx: AudioContext, destination: AudioNode, t: number): void {
    const freqs = [400, 500, 600, 800];
    // Warm ascending with detuned pairs and filtered harmonics
    freqs.forEach((freq, i) => {
      const noteStart = t + i * 0.1;
      const noteDur = 0.25;
      // Main tone
      const osc = ctx.createOscillator();
      const lp = ctx.createBiquadFilter();
      lp.type = 'lowpass'; lp.frequency.value = freq * 3; lp.Q.value = 2;
      const g = ctx.createGain();
      osc.type = 'sine';
      osc.frequency.setValueAtTime(freq, noteStart);
      this.createADSR(ctx, g.gain, 0.01, 0.04, 0.5, 0.15, 0.15, noteStart);
      osc.connect(lp); lp.connect(g); g.connect(destination);
      osc.start(noteStart); osc.stop(noteStart + noteDur);
      // Detuned shimmer
      const osc2 = ctx.createOscillator();
      const g2 = ctx.createGain();
      osc2.type = 'sine';
      osc2.frequency.setValueAtTime(freq * 1.005, noteStart);
      this.createADSR(ctx, g2.gain, 0.015, 0.04, 0.4, 0.15, 0.1, noteStart);
      osc2.connect(g2); g2.connect(destination);
      osc2.start(noteStart); osc2.stop(noteStart + noteDur);
    });
    // Soft sparkle noise
    this.createNoiseBurst(ctx, destination, 0.3, 4000, 'highpass', t + 0.15, 0.04);
  }

  /** Chime — triangle [523, 659, 784] chord with gentle ADSR. 0.4 s */
  private sfxSkillBuff(ctx: AudioContext, destination: AudioNode, t: number): void {
    const freqs = [523, 659, 784];
    const dur = 0.5;
    // Rich chord with filtered triangle + sine harmonics
    freqs.forEach((freq, i) => {
      const osc = ctx.createOscillator();
      const lp = ctx.createBiquadFilter();
      lp.type = 'lowpass'; lp.frequency.value = 3000; lp.Q.value = 1;
      const g = ctx.createGain();
      osc.type = 'triangle';
      osc.frequency.setValueAtTime(freq, t);
      this.createADSR(ctx, g.gain, 0.01 + i * 0.01, 0.06, 0.4, 0.3, 0.12, t);
      osc.connect(lp); lp.connect(g); g.connect(destination);
      osc.start(t); osc.stop(t + dur);
      // Octave harmonic
      const oct = ctx.createOscillator();
      const gOct = ctx.createGain();
      oct.type = 'sine';
      oct.frequency.setValueAtTime(freq * 2, t);
      this.createADSR(ctx, gOct.gain, 0.02, 0.05, 0.3, 0.3, 0.04, t);
      oct.connect(gOct); gOct.connect(destination);
      oct.start(t); oct.stop(t + dur);
    });
  }

  // ---------------------------------------------------------------------------
  // Loot / Item SFX
  // ---------------------------------------------------------------------------

  /** Simple pickup — sine 800→1200→600 Hz quick. 0.2 s */
  private sfxLootCommon(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.2;
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'sine';
    osc.frequency.setValueAtTime(800, t);
    osc.frequency.linearRampToValueAtTime(1200, t + 0.07);
    osc.frequency.exponentialRampToValueAtTime(600, t + dur);
    this.createADSR(ctx, gain.gain, 0.005, 0.04, 0.3, 0.12, 0.2, t);
    osc.connect(gain);
    gain.connect(destination);
    osc.start(t);
    osc.stop(t + dur);
  }

  /** Sparkle — sine 800→1200 + triangle 1000→1400. 0.3 s */
  private sfxLootMagic(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.3;

    const osc1 = ctx.createOscillator();
    const g1 = ctx.createGain();
    osc1.type = 'sine';
    osc1.frequency.setValueAtTime(800, t);
    osc1.frequency.exponentialRampToValueAtTime(1200, t + dur);
    this.createADSR(ctx, g1.gain, 0.005, 0.05, 0.35, 0.2, 0.18, t);
    osc1.connect(g1);
    g1.connect(destination);
    osc1.start(t);
    osc1.stop(t + dur);

    const osc2 = ctx.createOscillator();
    const g2 = ctx.createGain();
    osc2.type = 'triangle';
    osc2.frequency.setValueAtTime(1000, t);
    osc2.frequency.exponentialRampToValueAtTime(1400, t + dur);
    this.createADSR(ctx, g2.gain, 0.005, 0.06, 0.3, 0.2, 0.13, t);
    osc2.connect(g2);
    g2.connect(destination);
    osc2.start(t);
    osc2.stop(t + dur);
  }

  /** Rich sparkle — 3 detuned sines ascending + shimmer. 0.4 s */
  private sfxLootRare(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.4;
    const baseFreqs = [900, 1100, 1350];
    baseFreqs.forEach((freq, i) => {
      const noteStart = t + i * 0.06;
      const osc = ctx.createOscillator();
      const gain = ctx.createGain();
      osc.type = 'sine';
      osc.frequency.setValueAtTime(freq, noteStart);
      osc.frequency.exponentialRampToValueAtTime(freq * 1.3, noteStart + dur * 0.6);
      this.createADSR(ctx, gain.gain, 0.008, 0.06, 0.35, 0.25, 0.15, noteStart);
      osc.connect(gain);
      gain.connect(destination);
      osc.start(noteStart);
      osc.stop(noteStart + dur);
    });

    // Shimmer noise
    this.createNoiseBurst(ctx, destination, dur, 5000, 'highpass', t, 0.1);
  }

  /** Grand — ascending arpeggio [523,659,784,1047] + delay tail. 0.8 s */
  private sfxLootLegendary(ctx: AudioContext, destination: AudioNode, t: number): void {
    const freqs = [523, 659, 784, 1047];
    const noteDur = 0.25;
    freqs.forEach((freq, i) => {
      const noteStart = t + i * 0.12;
      // Direct note
      const osc = ctx.createOscillator();
      const gain = ctx.createGain();
      osc.type = 'sine';
      osc.frequency.setValueAtTime(freq, noteStart);
      this.createADSR(ctx, gain.gain, 0.008, 0.05, 0.5, 0.18, 0.22, noteStart);
      osc.connect(gain);
      gain.connect(destination);
      osc.start(noteStart);
      osc.stop(noteStart + noteDur);

      // Delay tail (echo at lower volume)
      const delay = 0.18;
      const oscD = ctx.createOscillator();
      const gainD = ctx.createGain();
      oscD.type = 'sine';
      oscD.frequency.setValueAtTime(freq, noteStart + delay);
      this.createADSR(ctx, gainD.gain, 0.008, 0.05, 0.3, 0.25, 0.1, noteStart + delay);
      oscD.connect(gainD);
      gainD.connect(destination);
      oscD.start(noteStart + delay);
      oscD.stop(noteStart + delay + noteDur);
    });
  }

  /** Metallic click — square 400 Hz very short + noise tick. 0.1 s */
  private sfxEquip(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.1;

    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'square';
    osc.frequency.setValueAtTime(400, t);
    this.createADSR(ctx, gain.gain, 0.002, 0.02, 0.1, 0.07, 0.3, t);
    osc.connect(gain);
    gain.connect(destination);
    osc.start(t);
    osc.stop(t + dur);

    this.createNoiseBurst(ctx, destination, dur, 3000, 'highpass', t, 0.15);
  }

  /** Liquid bubble — sine with vibrato at 300 Hz, gentle. 0.25 s */
  private sfxPotion(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.25;

    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'sine';
    osc.frequency.setValueAtTime(300, t);

    // Simple vibrato using frequency modulation
    const lfo = ctx.createOscillator();
    const lfoGain = ctx.createGain();
    lfo.type = 'sine';
    lfo.frequency.setValueAtTime(12, t);
    lfoGain.gain.setValueAtTime(25, t);
    lfo.connect(lfoGain);
    lfoGain.connect(osc.frequency);
    lfo.start(t);
    lfo.stop(t + dur);

    this.createADSR(ctx, gain.gain, 0.01, 0.06, 0.5, 0.15, 0.2, t);
    osc.connect(gain);
    gain.connect(destination);
    osc.start(t);
    osc.stop(t + dur);
  }

  // ---------------------------------------------------------------------------
  // UI SFX
  // ---------------------------------------------------------------------------

  /** Simple — sine 600 Hz, very short (0.05 s), quiet. */
  private sfxClick(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.05;
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'sine';
    osc.frequency.setValueAtTime(600, t);
    this.createADSR(ctx, gain.gain, 0.002, 0.01, 0.1, 0.035, 0.12, t);
    osc.connect(gain);
    gain.connect(destination);
    osc.start(t);
    osc.stop(t + dur);
  }

  /** Rising — sine 400→600 Hz. 0.1 s */
  private sfxPanelOpen(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.1;
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'sine';
    osc.frequency.setValueAtTime(400, t);
    osc.frequency.exponentialRampToValueAtTime(600, t + dur);
    this.createADSR(ctx, gain.gain, 0.005, 0.02, 0.4, 0.06, 0.15, t);
    osc.connect(gain);
    gain.connect(destination);
    osc.start(t);
    osc.stop(t + dur);
  }

  /** Falling — sine 600→400 Hz. 0.1 s */
  private sfxPanelClose(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.1;
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'sine';
    osc.frequency.setValueAtTime(600, t);
    osc.frequency.exponentialRampToValueAtTime(400, t + dur);
    this.createADSR(ctx, gain.gain, 0.005, 0.02, 0.4, 0.06, 0.15, t);
    osc.connect(gain);
    gain.connect(destination);
    osc.start(t);
    osc.stop(t + dur);
  }

  /** Harsh buzz — square 200 Hz + square 150 Hz. 0.2 s */
  private sfxError(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.2;

    [200, 150].forEach(freq => {
      const osc = ctx.createOscillator();
      const gain = ctx.createGain();
      osc.type = 'square';
      osc.frequency.setValueAtTime(freq, t);
      this.createADSR(ctx, gain.gain, 0.003, 0.04, 0.5, 0.13, 0.15, t);
      osc.connect(gain);
      gain.connect(destination);
      osc.start(t);
      osc.stop(t + dur);
    });
  }

  // ---------------------------------------------------------------------------
  // World SFX
  // ---------------------------------------------------------------------------

  /** Mystical sweep — sine 200→800→400 Hz with delay tail. 0.8 s */
  private sfxZoneTransition(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.8;

    // Primary sweep
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'sine';
    osc.frequency.setValueAtTime(200, t);
    osc.frequency.exponentialRampToValueAtTime(800, t + 0.35);
    osc.frequency.exponentialRampToValueAtTime(400, t + dur);
    this.createADSR(ctx, gain.gain, 0.02, 0.1, 0.5, 0.5, 0.25, t);
    osc.connect(gain);
    gain.connect(destination);
    osc.start(t);
    osc.stop(t + dur);

    // Delay echo
    const delay = 0.22;
    const oscD = ctx.createOscillator();
    const gainD = ctx.createGain();
    oscD.type = 'sine';
    oscD.frequency.setValueAtTime(200, t + delay);
    oscD.frequency.exponentialRampToValueAtTime(800, t + delay + 0.35);
    oscD.frequency.exponentialRampToValueAtTime(400, t + delay + dur * 0.8);
    this.createADSR(ctx, gainD.gain, 0.02, 0.12, 0.35, 0.45, 0.12, t + delay);
    oscD.connect(gainD);
    gainD.connect(destination);
    oscD.start(t + delay);
    oscD.stop(t + delay + dur);
  }

  /** Fanfare — triangle arpeggio [392,523,659,784] with delay feedback. 0.8 s */
  private sfxQuestComplete(ctx: AudioContext, destination: AudioNode, t: number): void {
    const freqs = [392, 523, 659, 784];
    const noteDur = 0.25;
    freqs.forEach((freq, i) => {
      const noteStart = t + i * 0.11;

      // Direct note
      const osc = ctx.createOscillator();
      const gain = ctx.createGain();
      osc.type = 'triangle';
      osc.frequency.setValueAtTime(freq, noteStart);
      this.createADSR(ctx, gain.gain, 0.008, 0.05, 0.5, 0.18, 0.2, noteStart);
      osc.connect(gain);
      gain.connect(destination);
      osc.start(noteStart);
      osc.stop(noteStart + noteDur);

      // Feedback echo
      const delay = 0.17;
      const oscD = ctx.createOscillator();
      const gainD = ctx.createGain();
      oscD.type = 'triangle';
      oscD.frequency.setValueAtTime(freq, noteStart + delay);
      this.createADSR(ctx, gainD.gain, 0.008, 0.05, 0.3, 0.22, 0.1, noteStart + delay);
      oscD.connect(gainD);
      gainD.connect(destination);
      oscD.start(noteStart + delay);
      oscD.stop(noteStart + delay + noteDur);
    });
  }

  /** Grand arpeggio — sine [261,329,392,523,659] ascending with overlap + delay tail. 1.0 s */
  private sfxLevelup(ctx: AudioContext, destination: AudioNode, t: number): void {
    const freqs = [261, 329, 392, 523, 659];
    const noteDur = 0.3;
    freqs.forEach((freq, i) => {
      const noteStart = t + i * 0.13;

      // Direct note
      const osc = ctx.createOscillator();
      const gain = ctx.createGain();
      osc.type = 'sine';
      osc.frequency.setValueAtTime(freq, noteStart);
      this.createADSR(ctx, gain.gain, 0.01, 0.05, 0.55, 0.2, 0.2, noteStart);
      osc.connect(gain);
      gain.connect(destination);
      osc.start(noteStart);
      osc.stop(noteStart + noteDur);

      // Delay tail echo
      const delay = 0.2;
      const oscD = ctx.createOscillator();
      const gainD = ctx.createGain();
      oscD.type = 'sine';
      oscD.frequency.setValueAtTime(freq, noteStart + delay);
      this.createADSR(ctx, gainD.gain, 0.01, 0.06, 0.35, 0.28, 0.1, noteStart + delay);
      oscD.connect(gainD);
      gainD.connect(destination);
      oscD.start(noteStart + delay);
      oscD.stop(noteStart + delay + noteDur);
    });
  }

  /** Gentle chime — sine 523 Hz + 659 Hz soft. 0.15 s */
  private sfxNpcInteract(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.15;
    [523, 659].forEach((freq, i) => {
      const noteStart = t + i * 0.04;
      const osc = ctx.createOscillator();
      const gain = ctx.createGain();
      osc.type = 'sine';
      osc.frequency.setValueAtTime(freq, noteStart);
      this.createADSR(ctx, gain.gain, 0.008, 0.03, 0.4, 0.1, 0.15, noteStart);
      osc.connect(gain);
      gain.connect(destination);
      osc.start(noteStart);
      osc.stop(noteStart + dur);
    });
  }
}
