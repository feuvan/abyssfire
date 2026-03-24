import { EventBus, GameEvents } from '../utils/EventBus';
import { clamp } from '../utils/MathUtils';

// ---------------------------------------------------------------------------
// Status Effect Types & Interfaces
// ---------------------------------------------------------------------------

export type StatusEffectType = 'burn' | 'freeze' | 'poison' | 'bleed' | 'slow' | 'stun';

/**
 * Represents a single active status effect on an entity.
 */
export interface StatusEffect {
  type: StatusEffectType;
  /** Damage per tick (for DoTs), slow percentage (for slow), etc. */
  value: number;
  /** Total duration in ms. */
  duration: number;
  /** Tick interval in ms (for DoTs). 0 = no ticking (freeze/stun/slow). */
  tickInterval: number;
  /** Timestamp when effect was applied. */
  startTime: number;
  /** Timestamp of last tick (for DoTs). */
  lastTickTime: number;
  /** ID of the entity that applied this effect (for attribution). */
  sourceId: string;
}

/**
 * Tracks diminishing returns for CC effects (freeze/stun) per-entity.
 */
export interface DiminishingReturnRecord {
  /** Number of times applied within the window. */
  applyCount: number;
  /** Timestamp of last application. */
  lastApplyTime: number;
  /** If > 0, entity is immune until this timestamp. */
  immuneUntil: number;
}

/** Config for diminishing returns. */
export const DIMINISH_FACTOR = 0.5;         // 50% duration on 2nd application
export const DIMINISH_IMMUNITY_DURATION = 3000; // 3s immunity after 2nd
export const DIMINISH_WINDOW = 6000;        // Reset count after 6s of no applies

/** Minimum speed floor for Slow effect: 20% of base. */
export const SLOW_MIN_SPEED_FACTOR = 0.2;

/** Default tick intervals per effect type (ms). */
export const DEFAULT_TICK_INTERVALS: Record<StatusEffectType, number> = {
  burn: 1000,
  freeze: 0,
  poison: 1000,
  bleed: 1000,
  slow: 0,
  stun: 0,
};

/**
 * Pure-logic StatusEffectSystem.
 * Manages status effects on entities identified by string IDs.
 * No Phaser dependency — visual indicators are applied by the caller
 * based on events emitted by this system.
 */
export class StatusEffectSystem {
  /** Active effects per entity. Key = entity ID, value = array of effects. */
  private effects = new Map<string, StatusEffect[]>();

  /** Diminishing returns tracking for freeze/stun. Key = `${entityId}:${effectType}`. */
  private diminishRecords = new Map<string, DiminishingReturnRecord>();

  // ── Apply ─────────────────────────────────────────────

  /**
   * Apply a status effect to an entity.
   * Returns the effective duration (0 if immune / blocked).
   */
  apply(
    targetId: string,
    type: StatusEffectType,
    value: number,
    duration: number,
    sourceId: string,
    now: number,
  ): number {
    if (duration <= 0 || value <= 0) return 0;

    // Freeze/Stun use diminishing returns
    if (type === 'freeze' || type === 'stun') {
      duration = this.applyDiminishingReturns(targetId, type, duration, now);
      if (duration <= 0) return 0;
    }

    // Poison: refresh duration, keep stronger value (don't stack count)
    if (type === 'poison') {
      const existing = this.getEffectsOnEntity(targetId);
      const existingPoison = existing.find(e => e.type === 'poison');
      if (existingPoison) {
        // Refresh duration, keep the stronger damage
        existingPoison.startTime = now;
        existingPoison.duration = duration;
        if (value > existingPoison.value) {
          existingPoison.value = value;
        }
        existingPoison.sourceId = sourceId;
        EventBus.emit(GameEvents.LOG_MESSAGE, {
          text: `中毒效果已刷新`,
        });
        return duration;
      }
    }

    // Slow: don't stack, replace with stronger or refresh
    if (type === 'slow') {
      const existing = this.getEffectsOnEntity(targetId);
      const existingSlow = existing.find(e => e.type === 'slow');
      if (existingSlow) {
        existingSlow.startTime = now;
        existingSlow.duration = duration;
        if (value > existingSlow.value) {
          existingSlow.value = value;
        }
        existingSlow.sourceId = sourceId;
        return duration;
      }
    }

    // Freeze/Stun: replace existing (only one active at a time)
    if (type === 'freeze' || type === 'stun') {
      const existing = this.getEffectsOnEntity(targetId);
      const idx = existing.findIndex(e => e.type === type);
      if (idx !== -1) {
        existing.splice(idx, 1);
      }
    }

    const effect: StatusEffect = {
      type,
      value,
      duration,
      tickInterval: DEFAULT_TICK_INTERVALS[type],
      startTime: now,
      lastTickTime: now,
      sourceId,
    };

    if (!this.effects.has(targetId)) {
      this.effects.set(targetId, []);
    }
    this.effects.get(targetId)!.push(effect);

    const effectNames: Record<StatusEffectType, string> = {
      burn: '灼烧',
      freeze: '冰冻',
      poison: '中毒',
      bleed: '流血',
      slow: '减速',
      stun: '眩晕',
    };
    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: `${effectNames[type]}效果已施加`,
    });

    return duration;
  }

  // ── Tick ──────────────────────────────────────────────

  /**
   * Tick all effects on a specific entity.
   * Returns an array of damage ticks that occurred.
   * The caller is responsible for applying the damage to the entity.
   */
  tick(targetId: string, now: number): { type: StatusEffectType; damage: number }[] {
    const entityEffects = this.effects.get(targetId);
    if (!entityEffects || entityEffects.length === 0) return [];

    const ticks: { type: StatusEffectType; damage: number }[] = [];

    for (const effect of entityEffects) {
      if (effect.tickInterval <= 0) continue;

      // Check if it's time for a tick
      const elapsed = now - effect.lastTickTime;
      if (elapsed >= effect.tickInterval) {
        // How many ticks should fire (could be multiple if delta > interval)
        const tickCount = Math.floor(elapsed / effect.tickInterval);
        for (let i = 0; i < tickCount; i++) {
          ticks.push({ type: effect.type, damage: effect.value });
        }
        effect.lastTickTime = now;
      }
    }

    return ticks;
  }

  // ── Expire ────────────────────────────────────────────

  /**
   * Remove expired effects from an entity.
   * Returns the list of effect types that expired.
   */
  expire(targetId: string, now: number): StatusEffectType[] {
    const entityEffects = this.effects.get(targetId);
    if (!entityEffects || entityEffects.length === 0) return [];

    const expired: StatusEffectType[] = [];
    const remaining: StatusEffect[] = [];

    for (const effect of entityEffects) {
      if (now - effect.startTime >= effect.duration) {
        expired.push(effect.type);
      } else {
        remaining.push(effect);
      }
    }

    if (remaining.length === 0) {
      this.effects.delete(targetId);
    } else {
      this.effects.set(targetId, remaining);
    }

    return expired;
  }

  // ── Entity Death Cleanup ──────────────────────────────

  /**
   * Clear all effects on an entity (called on death).
   * Returns the cleared effect types for visual cleanup.
   */
  clearEntity(targetId: string): StatusEffectType[] {
    const entityEffects = this.effects.get(targetId);
    if (!entityEffects) return [];

    const types = entityEffects.map(e => e.type);
    this.effects.delete(targetId);
    this.clearDiminishRecords(targetId);

    return types;
  }

  // ── Query Methods ─────────────────────────────────────

  /** Get all active effects on an entity. */
  getEffectsOnEntity(targetId: string): StatusEffect[] {
    return this.effects.get(targetId) ?? [];
  }

  /** Check if an entity has a specific effect type active. */
  hasEffect(targetId: string, type: StatusEffectType): boolean {
    const effects = this.effects.get(targetId);
    if (!effects) return false;
    return effects.some(e => e.type === type);
  }

  /** Check if an entity is immobilized (freeze or stun). */
  isImmobilized(targetId: string): boolean {
    return this.hasEffect(targetId, 'freeze') || this.hasEffect(targetId, 'stun');
  }

  /**
   * Get the effective speed multiplier for an entity.
   * Returns 1.0 if no slow, or the reduced multiplier (min SLOW_MIN_SPEED_FACTOR).
   */
  getSpeedMultiplier(targetId: string): number {
    const effects = this.effects.get(targetId);
    if (!effects) return 1;

    // If immobilized, speed = 0
    if (effects.some(e => e.type === 'freeze' || e.type === 'stun')) return 0;

    // Find the strongest slow (only one active due to no-stack policy)
    const slow = effects.find(e => e.type === 'slow');
    if (!slow) return 1;

    // Value is the % reduction (e.g. 50 = 50% slower)
    const multiplier = 1 - clamp(slow.value, 0, 100) / 100;
    return Math.max(SLOW_MIN_SPEED_FACTOR, multiplier);
  }

  /**
   * Check if poison heal reduction should apply.
   * Returns true if the entity has an active poison effect.
   */
  hasPoisonHealReduction(targetId: string): boolean {
    return this.hasEffect(targetId, 'poison');
  }

  /** Get the diminishing return record for an entity+effect. */
  getDiminishRecord(targetId: string, type: StatusEffectType): DiminishingReturnRecord | undefined {
    return this.diminishRecords.get(`${targetId}:${type}`);
  }

  /** Get all tracked entity IDs. */
  getTrackedEntities(): string[] {
    return Array.from(this.effects.keys());
  }

  // ── Diminishing Returns (private) ─────────────────────

  private applyDiminishingReturns(
    targetId: string,
    type: StatusEffectType,
    baseDuration: number,
    now: number,
  ): number {
    const key = `${targetId}:${type}`;
    let record = this.diminishRecords.get(key);
    if (!record) {
      record = { applyCount: 0, lastApplyTime: 0, immuneUntil: 0 };
      this.diminishRecords.set(key, record);
    }

    // Check immunity
    if (now < record.immuneUntil) {
      return 0;
    }

    // Reset count if outside the window
    if (now - record.lastApplyTime > DIMINISH_WINDOW) {
      record.applyCount = 0;
    }

    record.applyCount++;
    record.lastApplyTime = now;

    let effectiveDuration = baseDuration;

    if (record.applyCount === 2) {
      // Second application: 50% duration, then grant immunity after it expires
      effectiveDuration = Math.floor(baseDuration * DIMINISH_FACTOR);
      record.immuneUntil = now + effectiveDuration + DIMINISH_IMMUNITY_DURATION;
    } else if (record.applyCount > 2) {
      // Should not happen if immunity works, but safety: immune
      record.immuneUntil = now + DIMINISH_IMMUNITY_DURATION;
      return 0;
    }

    return effectiveDuration;
  }

  private clearDiminishRecords(targetId: string): void {
    for (const key of this.diminishRecords.keys()) {
      if (key.startsWith(`${targetId}:`)) {
        this.diminishRecords.delete(key);
      }
    }
  }

  /** Clear all state (e.g. on zone change). */
  clearAll(): void {
    this.effects.clear();
    this.diminishRecords.clear();
  }
}
