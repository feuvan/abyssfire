import { clamp } from '../utils/MathUtils';

export interface BufferedCombatAction {
  actionId: string;
  requestedAt: number;
  expiresAt: number;
}

/**
 * Single-slot input buffer for combat actions.
 *
 * A newly pressed skill replaces an older buffered skill, matching the
 * player's latest intent. Cooldown/mana/range checks remain runtime-owned.
 */
export class CombatInputBuffer {
  private queued: BufferedCombatAction | null = null;
  readonly windowMs: number;

  constructor(windowMs = 180) {
    this.windowMs = windowMs;
  }

  request(actionId: string, now: number, canExecute: boolean): 'execute' | 'buffered' {
    if (canExecute) {
      this.queued = null;
      return 'execute';
    }

    this.queued = {
      actionId,
      requestedAt: now,
      expiresAt: now + Math.max(0, this.windowMs),
    };
    return 'buffered';
  }

  consumeReady(
    now: number,
    canExecute: (actionId: string) => boolean,
  ): BufferedCombatAction | null {
    const queued = this.queued;
    if (!queued) return null;

    if (now > queued.expiresAt) {
      this.queued = null;
      return null;
    }
    if (!canExecute(queued.actionId)) return null;

    this.queued = null;
    return queued;
  }

  peek(): BufferedCombatAction | null {
    return this.queued ? { ...this.queued } : null;
  }

  clear(): void {
    this.queued = null;
  }
}

export interface DodgeConfig {
  cooldownMs: number;
  invulnerabilityMs: number;
}

const DEFAULT_DODGE_CONFIG: DodgeConfig = {
  cooldownMs: 900,
  invulnerabilityMs: 220,
};

/** Tracks dodge cooldown and invulnerability independently of Phaser input. */
export class DodgeController {
  readonly config: DodgeConfig;
  private cooldownEndsAt = 0;
  private invulnerabilityEndsAt = 0;
  private lastStartedAt = -Infinity;
  private avoidanceRewardClaimed = false;

  constructor(config: Partial<DodgeConfig> = {}) {
    this.config = {
      cooldownMs: Math.max(0, config.cooldownMs ?? DEFAULT_DODGE_CONFIG.cooldownMs),
      invulnerabilityMs: Math.max(
        0,
        config.invulnerabilityMs ?? DEFAULT_DODGE_CONFIG.invulnerabilityMs,
      ),
    };
  }

  tryStart(now: number): boolean {
    if (!this.isReady(now)) return false;
    this.lastStartedAt = now;
    this.cooldownEndsAt = now + this.config.cooldownMs;
    this.invulnerabilityEndsAt = now + this.config.invulnerabilityMs;
    this.avoidanceRewardClaimed = false;
    return true;
  }

  isReady(now: number): boolean {
    return now >= this.cooldownEndsAt;
  }

  isInvulnerable(now: number): boolean {
    return now < this.invulnerabilityEndsAt;
  }

  /** Claim the single combat-resource reward granted by this iframe window. */
  claimAvoidanceReward(now: number): boolean {
    if (!this.isInvulnerable(now) || this.avoidanceRewardClaimed) return false;
    this.avoidanceRewardClaimed = true;
    return true;
  }

  cooldownProgress(now: number): number {
    if (this.config.cooldownMs <= 0 || !Number.isFinite(this.lastStartedAt)) return 1;
    return clamp((now - this.lastStartedAt) / this.config.cooldownMs, 0, 1);
  }

  cooldownRemaining(now: number): number {
    return Math.max(0, this.cooldownEndsAt - now);
  }

  reset(): void {
    this.cooldownEndsAt = 0;
    this.invulnerabilityEndsAt = 0;
    this.lastStartedAt = -Infinity;
    this.avoidanceRewardClaimed = false;
  }
}

export interface TargetCandidate {
  id: string;
  distanceSq: number;
  alive: boolean;
}

/** Select the next living target in range, nearest-first, with wraparound. */
export function cycleTargetId(
  currentTargetId: string | null,
  candidates: TargetCandidate[],
  maxRange: number,
): string | null {
  const rangeSq = Math.max(0, maxRange) ** 2;
  const valid = candidates
    .filter(candidate => candidate.alive && candidate.distanceSq <= rangeSq)
    .sort((a, b) => a.distanceSq - b.distanceSq || a.id.localeCompare(b.id));

  if (valid.length === 0) return null;
  const currentIndex = valid.findIndex(candidate => candidate.id === currentTargetId);
  return valid[(currentIndex + 1) % valid.length].id;
}
