import { clamp } from '../utils/MathUtils';
import type { SpiritSaveState } from '../data/types';

export type { SpiritSaveState } from '../data/types';

export type SpiritCombatSource = 'hit' | 'kill' | 'dodge';

export interface SpiritProfile {
  id: 'emberheart' | 'astral_focus' | 'shadow_rhythm';
  classId: 'warrior' | 'mage' | 'rogue';
  visualColor: number;
  maxValue: number;
  hitGain: number;
  killGain: number;
  dodgeGain: number;
  critBonusGain: number;
  resonanceDurationMs: number;
  resonanceDamageBonus: number;
  resonanceManaCostMultiplier: number;
  resonanceMoveSpeedBonus: number;
}

export interface SpiritGainResult {
  gained: number;
  startedResonance: boolean;
}

export interface SpiritUpdateResult {
  endedResonance: boolean;
}

const SPIRIT_PROFILES: Record<SpiritProfile['classId'], SpiritProfile> = {
  warrior: {
    id: 'emberheart',
    classId: 'warrior',
    visualColor: 0xffb45c,
    maxValue: 100,
    hitGain: 8,
    killGain: 15,
    dodgeGain: 12,
    critBonusGain: 4,
    resonanceDurationMs: 6000,
    resonanceDamageBonus: 0.3,
    resonanceManaCostMultiplier: 0.85,
    resonanceMoveSpeedBonus: 0.12,
  },
  mage: {
    id: 'astral_focus',
    classId: 'mage',
    visualColor: 0xa98bff,
    maxValue: 100,
    hitGain: 7,
    killGain: 12,
    dodgeGain: 16,
    critBonusGain: 5,
    resonanceDurationMs: 7000,
    resonanceDamageBonus: 0.2,
    resonanceManaCostMultiplier: 0.6,
    resonanceMoveSpeedBonus: 0.08,
  },
  rogue: {
    id: 'shadow_rhythm',
    classId: 'rogue',
    visualColor: 0x66e58a,
    maxValue: 100,
    hitGain: 6,
    killGain: 13,
    dodgeGain: 20,
    critBonusGain: 7,
    resonanceDurationMs: 5500,
    resonanceDamageBonus: 0.25,
    resonanceManaCostMultiplier: 0.75,
    resonanceMoveSpeedBonus: 0.18,
  },
};

export function getSpiritProfile(classId: string): SpiritProfile {
  return SPIRIT_PROFILES[classId as SpiritProfile['classId']] ?? SPIRIT_PROFILES.warrior;
}

/**
 * Pure class-aware Spirit resource model.
 *
 * Combat actions charge the meter. Reaching maximum automatically starts a
 * short Resonance window, during which the meter drains and exposes combat
 * multipliers for the runtime to apply.
 */
export class SpiritSystem {
  readonly profile: SpiritProfile;
  value = 0;
  resonanceRemainingMs = 0;

  constructor(classId: string, savedState?: SpiritSaveState) {
    this.profile = getSpiritProfile(classId);
    if (savedState) this.restore(savedState);
  }

  get maxValue(): number {
    return this.profile.maxValue;
  }

  get isResonating(): boolean {
    return this.resonanceRemainingMs > 0 && this.value > 0;
  }

  get ratio(): number {
    return this.maxValue > 0 ? this.value / this.maxValue : 0;
  }

  get damageMultiplier(): number {
    return this.isResonating ? 1 + this.profile.resonanceDamageBonus : 1;
  }

  get manaCostMultiplier(): number {
    return this.isResonating ? this.profile.resonanceManaCostMultiplier : 1;
  }

  get moveSpeedMultiplier(): number {
    return this.isResonating ? 1 + this.profile.resonanceMoveSpeedBonus : 1;
  }

  gainFromCombat(source: SpiritCombatSource, spi: number, isCrit = false): SpiritGainResult {
    const baseGain = source === 'kill'
      ? this.profile.killGain
      : source === 'dodge'
        ? this.profile.dodgeGain
        : this.profile.hitGain;
    const critGain = isCrit ? this.profile.critBonusGain : 0;
    const spiritStatMultiplier = 1 + clamp(spi, 0, 200) * 0.015;
    return this.gain((baseGain + critGain) * spiritStatMultiplier);
  }

  gain(amount: number): SpiritGainResult {
    if (!Number.isFinite(amount) || amount <= 0 || this.isResonating) {
      return { gained: 0, startedResonance: false };
    }

    const previous = this.value;
    this.value = clamp(previous + amount, 0, this.maxValue);
    const gained = this.value - previous;
    const startedResonance = previous < this.maxValue && this.value >= this.maxValue;

    if (startedResonance) {
      this.resonanceRemainingMs = this.profile.resonanceDurationMs;
    }

    return { gained, startedResonance };
  }

  update(deltaMs: number): SpiritUpdateResult {
    if (!this.isResonating || !Number.isFinite(deltaMs) || deltaMs <= 0) {
      return { endedResonance: false };
    }

    const elapsed = Math.min(deltaMs, this.resonanceRemainingMs);
    this.resonanceRemainingMs = Math.max(0, this.resonanceRemainingMs - elapsed);
    this.value = Math.max(
      0,
      this.value - (this.maxValue / this.profile.resonanceDurationMs) * elapsed,
    );

    const endedResonance = this.resonanceRemainingMs <= 0 || this.value <= 0;
    if (endedResonance) {
      this.value = 0;
      this.resonanceRemainingMs = 0;
    }
    return { endedResonance };
  }

  restore(state: SpiritSaveState): void {
    if (!state || typeof state !== 'object') {
      this.reset();
      return;
    }
    const value = Number.isFinite(state.value) ? state.value : 0;
    const remaining = Number.isFinite(state.resonanceRemainingMs)
      ? state.resonanceRemainingMs
      : 0;
    this.value = clamp(value, 0, this.maxValue);
    this.resonanceRemainingMs = clamp(
      remaining,
      0,
      this.profile.resonanceDurationMs,
    );
    if (this.value <= 0 || this.resonanceRemainingMs <= 0) {
      this.resonanceRemainingMs = 0;
    }
  }

  reset(): void {
    this.value = 0;
    this.resonanceRemainingMs = 0;
  }

  toSaveState(): SpiritSaveState {
    return {
      value: this.value,
      resonanceRemainingMs: this.resonanceRemainingMs,
    };
  }
}
