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

  /** Metallic slash — sawtooth sweep 200→80 Hz + highpass noise burst. 0.15 s */
  private sfxHit(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.15;

    // Sawtooth sweep
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'sawtooth';
    osc.frequency.setValueAtTime(200, t);
    osc.frequency.exponentialRampToValueAtTime(80, t + dur);
    this.createADSR(ctx, gain.gain, 0.003, 0.04, 0.2, 0.08, 0.3, t);
    osc.connect(gain);
    gain.connect(destination);
    osc.start(t);
    osc.stop(t + dur);

    // Highpass noise
    this.createNoiseBurst(ctx, destination, dur, 2000, 'highpass', t, 0.2);
  }

  /** Deeper metallic slash — sawtooth 150→50 Hz + square 100→40 Hz + noise. 0.25 s */
  private sfxHitHeavy(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.25;

    // Primary sawtooth
    const osc1 = ctx.createOscillator();
    const g1 = ctx.createGain();
    osc1.type = 'sawtooth';
    osc1.frequency.setValueAtTime(150, t);
    osc1.frequency.exponentialRampToValueAtTime(50, t + dur);
    this.createADSR(ctx, g1.gain, 0.003, 0.06, 0.2, 0.15, 0.28, t);
    osc1.connect(g1);
    g1.connect(destination);
    osc1.start(t);
    osc1.stop(t + dur);

    // Square harmonic
    const osc2 = ctx.createOscillator();
    const g2 = ctx.createGain();
    osc2.type = 'square';
    osc2.frequency.setValueAtTime(100, t);
    osc2.frequency.exponentialRampToValueAtTime(40, t + dur);
    this.createADSR(ctx, g2.gain, 0.003, 0.08, 0.15, 0.15, 0.15, t);
    osc2.connect(g2);
    g2.connect(destination);
    osc2.start(t);
    osc2.stop(t + dur);

    // Lowpass noise
    this.createNoiseBurst(ctx, destination, dur, 800, 'lowpass', t, 0.2);
  }

  /** Sharp high impact — square 600→100 Hz + sawtooth 900→200 Hz + noise. 0.25 s */
  private sfxCrit(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.25;

    // Square attack
    const osc1 = ctx.createOscillator();
    const g1 = ctx.createGain();
    osc1.type = 'square';
    osc1.frequency.setValueAtTime(600, t);
    osc1.frequency.exponentialRampToValueAtTime(100, t + dur);
    this.createADSR(ctx, g1.gain, 0.002, 0.05, 0.15, 0.15, 0.3, t);
    osc1.connect(g1);
    g1.connect(destination);
    osc1.start(t);
    osc1.stop(t + dur);

    // Sawtooth harmonic
    const osc2 = ctx.createOscillator();
    const g2 = ctx.createGain();
    osc2.type = 'sawtooth';
    osc2.frequency.setValueAtTime(900, t);
    osc2.frequency.exponentialRampToValueAtTime(200, t + dur);
    this.createADSR(ctx, g2.gain, 0.002, 0.04, 0.1, 0.18, 0.2, t);
    osc2.connect(g2);
    g2.connect(destination);
    osc2.start(t);
    osc2.stop(t + dur);

    // Highpass noise burst
    this.createNoiseBurst(ctx, destination, dur, 3000, 'highpass', t, 0.25);
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
    const dur = 0.15;

    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'square';
    osc.frequency.setValueAtTime(300, t);
    this.createADSR(ctx, gain.gain, 0.002, 0.03, 0.1, 0.1, 0.3, t);
    osc.connect(gain);
    gain.connect(destination);
    osc.start(t);
    osc.stop(t + dur);

    this.createNoiseBurst(ctx, destination, dur, 4000, 'highpass', t, 0.22);
  }

  /** Dull thud — sine 150→60 Hz + lowpass noise burst. 0.2 s */
  private sfxPlayerHurt(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.2;

    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'sine';
    osc.frequency.setValueAtTime(150, t);
    osc.frequency.exponentialRampToValueAtTime(60, t + dur);
    this.createADSR(ctx, gain.gain, 0.003, 0.05, 0.2, 0.12, 0.3, t);
    osc.connect(gain);
    gain.connect(destination);
    osc.start(t);
    osc.stop(t + dur);

    this.createNoiseBurst(ctx, destination, dur, 400, 'lowpass', t, 0.2);
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
    const dur = 0.3;

    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'triangle';
    osc.frequency.setValueAtTime(300, t);
    osc.frequency.linearRampToValueAtTime(600, t + 0.08);
    osc.frequency.exponentialRampToValueAtTime(200, t + dur);
    this.createADSR(ctx, gain.gain, 0.005, 0.06, 0.25, 0.18, 0.28, t);
    osc.connect(gain);
    gain.connect(destination);
    osc.start(t);
    osc.stop(t + dur);

    this.createNoiseBurst(ctx, destination, dur, 1500, 'bandpass', t, 0.18);
  }

  /** Crackling fire — sawtooth 200→400 Hz + bandpass noise at 1000 Hz. 0.4 s */
  private sfxSkillFire(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.4;

    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'sawtooth';
    osc.frequency.setValueAtTime(200, t);
    osc.frequency.exponentialRampToValueAtTime(400, t + dur);
    this.createADSR(ctx, gain.gain, 0.01, 0.08, 0.35, 0.22, 0.25, t);
    osc.connect(gain);
    gain.connect(destination);
    osc.start(t);
    osc.stop(t + dur);

    this.createNoiseBurst(ctx, destination, dur, 1000, 'bandpass', t, 0.3);
  }

  /** Crystal shimmer — sine 800→1200→600 Hz + highpass noise at 3000 Hz. 0.35 s */
  private sfxSkillIce(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.35;

    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'sine';
    osc.frequency.setValueAtTime(800, t);
    osc.frequency.linearRampToValueAtTime(1200, t + 0.1);
    osc.frequency.exponentialRampToValueAtTime(600, t + dur);
    this.createADSR(ctx, gain.gain, 0.008, 0.07, 0.3, 0.2, 0.25, t);
    osc.connect(gain);
    gain.connect(destination);
    osc.start(t);
    osc.stop(t + dur);

    this.createNoiseBurst(ctx, destination, dur, 3000, 'highpass', t, 0.15);
  }

  /** Electric zap — square 1000→200 Hz fast + white noise burst. 0.3 s */
  private sfxSkillLightning(ctx: AudioContext, destination: AudioNode, t: number): void {
    const dur = 0.3;

    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'square';
    osc.frequency.setValueAtTime(1000, t);
    osc.frequency.exponentialRampToValueAtTime(200, t + dur);
    this.createADSR(ctx, gain.gain, 0.002, 0.03, 0.2, 0.25, 0.28, t);
    osc.connect(gain);
    gain.connect(destination);
    osc.start(t);
    osc.stop(t + dur);

    // Broad white noise burst (use lowpass at very high freq = essentially flat)
    this.createNoiseBurst(ctx, destination, dur * 0.5, 8000, 'lowpass', t, 0.3);
  }

  /** Warm ascending — sine arpeggio [400, 500, 600, 800] with overlap. 0.5 s */
  private sfxSkillHeal(ctx: AudioContext, destination: AudioNode, t: number): void {
    const freqs = [400, 500, 600, 800];
    const stepDur = 0.15;
    freqs.forEach((freq, i) => {
      const noteStart = t + i * 0.1;
      const osc = ctx.createOscillator();
      const gain = ctx.createGain();
      osc.type = 'sine';
      osc.frequency.setValueAtTime(freq, noteStart);
      this.createADSR(ctx, gain.gain, 0.01, 0.03, 0.5, 0.1, 0.2, noteStart);
      osc.connect(gain);
      gain.connect(destination);
      osc.start(noteStart);
      osc.stop(noteStart + stepDur);
    });
  }

  /** Chime — triangle [523, 659, 784] chord with gentle ADSR. 0.4 s */
  private sfxSkillBuff(ctx: AudioContext, destination: AudioNode, t: number): void {
    const freqs = [523, 659, 784];
    const dur = 0.4;
    freqs.forEach(freq => {
      const osc = ctx.createOscillator();
      const gain = ctx.createGain();
      osc.type = 'triangle';
      osc.frequency.setValueAtTime(freq, t);
      this.createADSR(ctx, gain.gain, 0.01, 0.06, 0.4, 0.28, 0.12, t);
      osc.connect(gain);
      gain.connect(destination);
      osc.start(t);
      osc.stop(t + dur);
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
