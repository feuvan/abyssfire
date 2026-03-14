import { EventBus, GameEvents } from '../utils/EventBus';

type SFXType = 'hit' | 'crit' | 'miss' | 'levelup' | 'loot' | 'equip' | 'skill' | 'death' | 'buy' | 'quest' | 'click';

export class AudioSystem {
  private ctx: AudioContext | null = null;
  private musicGain: GainNode | null = null;
  private sfxGain: GainNode | null = null;
  private ambientOsc: OscillatorNode[] = [];
  private musicPlaying = false;
  private musicVolume = 0.15;
  private sfxVolume = 0.3;

  constructor() {
    this.setupEventListeners();
  }

  private getCtx(): AudioContext {
    if (!this.ctx) {
      this.ctx = new AudioContext();
      this.musicGain = this.ctx.createGain();
      this.musicGain.gain.value = this.musicVolume;
      this.musicGain.connect(this.ctx.destination);
      this.sfxGain = this.ctx.createGain();
      this.sfxGain.gain.value = this.sfxVolume;
      this.sfxGain.connect(this.ctx.destination);
    }
    if (this.ctx.state === 'suspended') this.ctx.resume();
    return this.ctx;
  }

  private setupEventListeners(): void {
    EventBus.on(GameEvents.COMBAT_DAMAGE, (d: { isCrit: boolean; isDodged: boolean; isPlayer: boolean }) => {
      if (d.isDodged) this.playSFX('miss');
      else if (d.isCrit) this.playSFX('crit');
      else this.playSFX('hit');
    });
    EventBus.on(GameEvents.PLAYER_LEVEL_UP, () => this.playSFX('levelup'));
    EventBus.on(GameEvents.ITEM_PICKED, () => this.playSFX('loot'));
    EventBus.on(GameEvents.QUEST_COMPLETED, () => this.playSFX('quest'));
    EventBus.on(GameEvents.PLAYER_DIED, () => this.playSFX('death'));
    EventBus.on(GameEvents.ZONE_ENTERED, () => {
      if (!this.musicPlaying) this.startAmbientMusic();
    });
  }

  playSFX(type: SFXType): void {
    const ctx = this.getCtx();
    const now = ctx.currentTime;
    const gain = ctx.createGain();
    gain.connect(this.sfxGain!);

    switch (type) {
      case 'hit': {
        // Metallic slash
        const osc = ctx.createOscillator();
        osc.type = 'sawtooth';
        osc.frequency.setValueAtTime(200, now);
        osc.frequency.exponentialRampToValueAtTime(80, now + 0.1);
        gain.gain.setValueAtTime(0.3, now);
        gain.gain.exponentialRampToValueAtTime(0.01, now + 0.15);
        osc.connect(gain);
        osc.start(now);
        osc.stop(now + 0.15);
        // Noise burst for impact
        this.playNoiseBurst(ctx, now, 0.06, 0.15);
        break;
      }
      case 'crit': {
        // Sharp high impact + reverb feel
        const osc = ctx.createOscillator();
        osc.type = 'square';
        osc.frequency.setValueAtTime(600, now);
        osc.frequency.exponentialRampToValueAtTime(100, now + 0.2);
        gain.gain.setValueAtTime(0.35, now);
        gain.gain.exponentialRampToValueAtTime(0.01, now + 0.25);
        osc.connect(gain);
        osc.start(now);
        osc.stop(now + 0.25);
        // Second harmonic
        const osc2 = ctx.createOscillator();
        osc2.type = 'sawtooth';
        osc2.frequency.setValueAtTime(900, now);
        osc2.frequency.exponentialRampToValueAtTime(200, now + 0.15);
        const g2 = ctx.createGain();
        g2.gain.setValueAtTime(0.15, now);
        g2.gain.exponentialRampToValueAtTime(0.01, now + 0.15);
        g2.connect(this.sfxGain!);
        osc2.connect(g2);
        osc2.start(now);
        osc2.stop(now + 0.15);
        this.playNoiseBurst(ctx, now, 0.1, 0.1);
        break;
      }
      case 'miss': {
        // Whoosh
        const osc = ctx.createOscillator();
        osc.type = 'sine';
        osc.frequency.setValueAtTime(400, now);
        osc.frequency.exponentialRampToValueAtTime(150, now + 0.2);
        gain.gain.setValueAtTime(0.1, now);
        gain.gain.exponentialRampToValueAtTime(0.01, now + 0.2);
        osc.connect(gain);
        osc.start(now);
        osc.stop(now + 0.2);
        break;
      }
      case 'levelup': {
        // Triumphant ascending arpeggio
        const notes = [261, 329, 392, 523, 659]; // C4-E5
        notes.forEach((freq, i) => {
          const osc = ctx.createOscillator();
          osc.type = 'sine';
          osc.frequency.value = freq;
          const g = ctx.createGain();
          const t = now + i * 0.1;
          g.gain.setValueAtTime(0, t);
          g.gain.linearRampToValueAtTime(0.2, t + 0.05);
          g.gain.exponentialRampToValueAtTime(0.01, t + 0.5);
          g.connect(this.sfxGain!);
          osc.connect(g);
          osc.start(t);
          osc.stop(t + 0.5);
        });
        break;
      }
      case 'loot': {
        // Coin/pickup sparkle
        const osc = ctx.createOscillator();
        osc.type = 'sine';
        osc.frequency.setValueAtTime(800, now);
        osc.frequency.exponentialRampToValueAtTime(1200, now + 0.08);
        osc.frequency.exponentialRampToValueAtTime(600, now + 0.2);
        gain.gain.setValueAtTime(0.15, now);
        gain.gain.exponentialRampToValueAtTime(0.01, now + 0.25);
        osc.connect(gain);
        osc.start(now);
        osc.stop(now + 0.25);
        break;
      }
      case 'skill': {
        // Magic cast
        const osc = ctx.createOscillator();
        osc.type = 'triangle';
        osc.frequency.setValueAtTime(300, now);
        osc.frequency.exponentialRampToValueAtTime(600, now + 0.15);
        osc.frequency.exponentialRampToValueAtTime(200, now + 0.3);
        gain.gain.setValueAtTime(0.2, now);
        gain.gain.exponentialRampToValueAtTime(0.01, now + 0.35);
        osc.connect(gain);
        osc.start(now);
        osc.stop(now + 0.35);
        break;
      }
      case 'death': {
        // Dark descending tone
        const osc = ctx.createOscillator();
        osc.type = 'sawtooth';
        osc.frequency.setValueAtTime(300, now);
        osc.frequency.exponentialRampToValueAtTime(40, now + 1.0);
        gain.gain.setValueAtTime(0.2, now);
        gain.gain.exponentialRampToValueAtTime(0.01, now + 1.2);
        osc.connect(gain);
        osc.start(now);
        osc.stop(now + 1.2);
        break;
      }
      case 'buy': {
        // Register cha-ching
        const notes = [523, 659];
        notes.forEach((freq, i) => {
          const osc = ctx.createOscillator();
          osc.type = 'sine';
          osc.frequency.value = freq;
          const g = ctx.createGain();
          g.gain.setValueAtTime(0.15, now + i * 0.08);
          g.gain.exponentialRampToValueAtTime(0.01, now + i * 0.08 + 0.2);
          g.connect(this.sfxGain!);
          osc.connect(g);
          osc.start(now + i * 0.08);
          osc.stop(now + i * 0.08 + 0.2);
        });
        break;
      }
      case 'quest': {
        // Completed fanfare
        const notes = [392, 523, 659, 784];
        notes.forEach((freq, i) => {
          const osc = ctx.createOscillator();
          osc.type = 'triangle';
          osc.frequency.value = freq;
          const g = ctx.createGain();
          const t = now + i * 0.12;
          g.gain.setValueAtTime(0.2, t);
          g.gain.exponentialRampToValueAtTime(0.01, t + 0.4);
          g.connect(this.sfxGain!);
          osc.connect(g);
          osc.start(t);
          osc.stop(t + 0.4);
        });
        break;
      }
      case 'click': {
        const osc = ctx.createOscillator();
        osc.type = 'sine';
        osc.frequency.value = 600;
        gain.gain.setValueAtTime(0.08, now);
        gain.gain.exponentialRampToValueAtTime(0.01, now + 0.05);
        osc.connect(gain);
        osc.start(now);
        osc.stop(now + 0.05);
        break;
      }
    }
  }

  private playNoiseBurst(ctx: AudioContext, when: number, volume: number, duration: number): void {
    const bufferSize = ctx.sampleRate * duration;
    const buffer = ctx.createBuffer(1, bufferSize, ctx.sampleRate);
    const data = buffer.getChannelData(0);
    for (let i = 0; i < bufferSize; i++) data[i] = (Math.random() * 2 - 1) * 0.5;
    const noise = ctx.createBufferSource();
    noise.buffer = buffer;
    const g = ctx.createGain();
    g.gain.setValueAtTime(volume, when);
    g.gain.exponentialRampToValueAtTime(0.001, when + duration);
    const filter = ctx.createBiquadFilter();
    filter.type = 'highpass';
    filter.frequency.value = 2000;
    noise.connect(filter);
    filter.connect(g);
    g.connect(this.sfxGain!);
    noise.start(when);
    noise.stop(when + duration);
  }

  startAmbientMusic(): void {
    const ctx = this.getCtx();
    this.stopAmbientMusic();
    this.musicPlaying = true;

    // Dark ambient pad - layered sine waves with slow LFO
    const fundamentals = [65.41, 98.0, 130.81]; // C2, G2, C3
    for (const freq of fundamentals) {
      const osc = ctx.createOscillator();
      osc.type = 'sine';
      osc.frequency.value = freq;
      // Slow detune wobble
      const lfo = ctx.createOscillator();
      lfo.type = 'sine';
      lfo.frequency.value = 0.1 + Math.random() * 0.15;
      const lfoGain = ctx.createGain();
      lfoGain.gain.value = 2 + Math.random() * 2;
      lfo.connect(lfoGain);
      lfoGain.connect(osc.detune);
      lfo.start();

      const gain = ctx.createGain();
      gain.gain.value = 0;
      gain.gain.linearRampToValueAtTime(0.06, ctx.currentTime + 3);

      // Low pass filter for warmth
      const filter = ctx.createBiquadFilter();
      filter.type = 'lowpass';
      filter.frequency.value = 300;
      filter.Q.value = 1;

      osc.connect(filter);
      filter.connect(gain);
      gain.connect(this.musicGain!);
      osc.start();

      this.ambientOsc.push(osc, lfo);
    }

    // Occasional ethereal chime
    this.scheduleChime(ctx);
  }

  private scheduleChime(ctx: AudioContext): void {
    if (!this.musicPlaying) return;
    const delay = 4000 + Math.random() * 8000;
    setTimeout(() => {
      if (!this.musicPlaying || !this.ctx) return;
      const notes = [523, 659, 784, 988, 1047, 1319];
      const freq = notes[Math.floor(Math.random() * notes.length)];
      const osc = ctx.createOscillator();
      osc.type = 'sine';
      osc.frequency.value = freq;
      const g = ctx.createGain();
      const now = ctx.currentTime;
      g.gain.setValueAtTime(0, now);
      g.gain.linearRampToValueAtTime(0.03, now + 0.3);
      g.gain.exponentialRampToValueAtTime(0.001, now + 2.5);

      const reverb = ctx.createBiquadFilter();
      reverb.type = 'lowpass';
      reverb.frequency.value = 2000;
      reverb.Q.value = 5;

      osc.connect(reverb);
      reverb.connect(g);
      g.connect(this.musicGain!);
      osc.start(now);
      osc.stop(now + 2.5);

      this.scheduleChime(ctx);
    }, delay);
  }

  stopAmbientMusic(): void {
    this.musicPlaying = false;
    for (const osc of this.ambientOsc) {
      try { osc.stop(); } catch (_) { /* already stopped */ }
    }
    this.ambientOsc = [];
  }

  setMusicVolume(v: number): void {
    this.musicVolume = v;
    if (this.musicGain) this.musicGain.gain.value = v;
  }

  setSFXVolume(v: number): void {
    this.sfxVolume = v;
    if (this.sfxGain) this.sfxGain.gain.value = v;
  }
}

export const audioSystem = new AudioSystem();
