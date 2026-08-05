import Phaser from 'phaser';
import type { MonsterAnimCategory } from '../data/types';

// ── Types ──────────────────────────────────────────────────────────────────

export type AnimState = 'idle' | 'walk' | 'attack' | 'cast' | 'hurt' | 'dodge' | 'death';

export interface AnimConfig {
  idleBobAmount: number;
  idleBobSpeed: number;
  idleScalePulse: number;
  idleSwayX: number;

  walkBobAmount: number;
  walkBobSpeed: number;
  walkTilt: number;
  walkSquash: number;

  attackLunge: number;
  attackDuration: number;
  attackSquash: number;
  attackWindup: number;
  attackShake: boolean;

  castLean: number;
  castDuration: number;
  castGlow: boolean;

  dodgeDuration: number;

  hurtKnockback: number;
  hurtDuration: number;
  hurtFlash: boolean;

  deathStyle: 'collapse' | 'dissolve' | 'splat';
  deathDuration: number;

  idleFrameRate: number;
  walkFrameRate: number;
  attackFrameRate: number;
  castFrameRate: number;
  hurtFrameRate: number;
  dodgeFrameRate: number;
  deathFrameRate: number;
}

// ── Preset Configs ─────────────────────────────────────────────────────────

const HUMANOID_CONFIG: AnimConfig = {
  idleBobAmount: 2,
  idleBobSpeed: 1000,
  idleScalePulse: 0.02,
  idleSwayX: 0,

  walkBobAmount: 6,
  walkBobSpeed: 240,
  walkTilt: 8,
  walkSquash: 0.10,

  attackLunge: 14,
  attackDuration: 500,
  attackSquash: 0.25,
  attackWindup: 150,
  attackShake: true,

  castLean: 5,
  castDuration: 350,
  castGlow: false,

  dodgeDuration: 260,

  hurtKnockback: 8,
  hurtDuration: 200,
  hurtFlash: true,

  deathStyle: 'collapse',
  deathDuration: 500,

  idleFrameRate: 6,
  walkFrameRate: 10,
  attackFrameRate: 12,
  castFrameRate: 10,
  hurtFrameRate: 10,
  dodgeFrameRate: 20,
  deathFrameRate: 6,
};

const PRESETS: Record<string, AnimConfig> = {
  humanoid: { ...HUMANOID_CONFIG },

  slime: {
    ...HUMANOID_CONFIG,
    idleScalePulse: 0.06,
    walkSquash: 0.12,
    deathStyle: 'splat',
  },

  beast: {
    ...HUMANOID_CONFIG,
    attackLunge: 16,
    attackDuration: 250,
    walkTilt: 8,
    deathStyle: 'collapse',
  },

  large: {
    ...HUMANOID_CONFIG,
    idleBobAmount: 1.5,
    idleBobSpeed: 1200,
    attackLunge: 10,
    attackDuration: 450,
    attackShake: true,
    deathStyle: 'collapse',
    deathDuration: 800,
  },

  flying: {
    ...HUMANOID_CONFIG,
    idleBobAmount: 4,
    idleBobSpeed: 700,
    idleSwayX: 3,
    deathStyle: 'collapse',
  },

  serpentine: {
    ...HUMANOID_CONFIG,
    idleSwayX: 4,
    idleBobAmount: 1,
    deathStyle: 'dissolve',
  },

  demonic: {
    ...HUMANOID_CONFIG,
    idleBobAmount: 2.5,
    idleBobSpeed: 600,
    idleScalePulse: 0.04,
    deathStyle: 'dissolve',
  },

  warrior: {
    ...HUMANOID_CONFIG,
    attackLunge: 16,
    attackDuration: 610,
    attackSquash: 0.2,
    castDuration: 725,
    dodgeDuration: 300,
    hurtDuration: 330,
    deathDuration: 750,
    idleFrameRate: 6,
    walkFrameRate: 10,
    attackFrameRate: 13,
    castFrameRate: 11,
    hurtFrameRate: 12,
    dodgeFrameRate: 20,
    deathFrameRate: 8,
  },

  mage: {
    ...HUMANOID_CONFIG,
    attackLunge: 6,
    attackDuration: 535,
    castDuration: 500,
    castGlow: true,
    dodgeDuration: 275,
    hurtDuration: 310,
    deathDuration: 670,
    idleFrameRate: 8,
    walkFrameRate: 11,
    attackFrameRate: 15,
    castFrameRate: 16,
    hurtFrameRate: 13,
    dodgeFrameRate: 22,
    deathFrameRate: 9,
  },

  rogue: {
    ...HUMANOID_CONFIG,
    attackDuration: 445,
    castDuration: 535,
    walkTilt: 7,
    dodgeDuration: 240,
    hurtDuration: 270,
    deathDuration: 550,
    idleFrameRate: 9,
    walkFrameRate: 14,
    attackFrameRate: 18,
    castFrameRate: 15,
    hurtFrameRate: 15,
    dodgeFrameRate: 25,
    deathFrameRate: 11,
  },
};

export function getAnimConfig(category: string): AnimConfig {
  const preset = PRESETS[category] ?? PRESETS['humanoid'];
  return { ...preset };
}

export function getActionFrameRate(category: string, action: AnimState): number {
  const config = getAnimConfig(category);
  const rateKey: Record<AnimState, keyof AnimConfig> = {
    idle: 'idleFrameRate',
    walk: 'walkFrameRate',
    attack: 'attackFrameRate',
    cast: 'castFrameRate',
    hurt: 'hurtFrameRate',
    dodge: 'dodgeFrameRate',
    death: 'deathFrameRate',
  };
  return config[rateKey[action]] as number;
}

// ── CharacterAnimator Class ────────────────────────────────────────────────

export class CharacterAnimator {
  private scene: Phaser.Scene;
  private container: Phaser.GameObjects.Container;
  private config: AnimConfig;
  private animPrefix: string;
  private hasFrameAnims: boolean;

  private state: AnimState = 'idle';
  private prevState: AnimState = 'idle';
  private tweens: Phaser.Tweens.Tween[] = [];
  private baseY: number = 0;
  private baseX: number = 0;
  private animTime: number = 0;
  private dead: boolean = false;
  private frameBaseX = 0;
  private frameBaseY = 0;
  private frameBaseScaleX = 1;
  private frameBaseScaleY = 1;
  private frameBaseAngle = 0;

  // Transition blending
  private transitionProgress = 1; // 1 = fully in current state
  private transitionDuration = 0;
  private prevBobY = 0;
  private prevScaleX = 1;
  private prevScaleY = 1;
  private prevAngle = 0;

  // Hit-freeze
  private hitFreezeTimer = 0;

  private static readonly TRANSITION_MS: Record<string, number> = {
    'idle->walk': 90,
    'walk->idle': 110,
    'idle->attack': 45,
    'walk->attack': 55,
    'attack->idle': 75,
    'attack->walk': 70,
    'idle->cast': 65,
    'walk->cast': 70,
    'attack->cast': 55,
    'cast->attack': 55,
    'cast->idle': 85,
    'cast->walk': 75,
    'idle->dodge': 35,
    'walk->dodge': 30,
    'attack->dodge': 25,
    'cast->dodge': 25,
    'hurt->dodge': 40,
    'dodge->idle': 65,
    'dodge->walk': 55,
    'hurt->idle': 105,
    'hurt->walk': 90,
  };

  constructor(scene: Phaser.Scene, container: Phaser.GameObjects.Container, config: AnimConfig, animPrefix?: string) {
    this.scene = scene;
    this.container = container;
    this.config = config;
    this.animPrefix = animPrefix ?? '';
    this.hasFrameAnims = !!animPrefix && scene.anims.exists(`${animPrefix}_idle`);
    const sprite = this.getSpriteChild();
    if (sprite) {
      this.frameBaseX = sprite.x;
      this.frameBaseY = sprite.y;
      this.frameBaseScaleX = sprite.scaleX;
      this.frameBaseScaleY = sprite.scaleY;
      this.frameBaseAngle = sprite.angle;
    }
  }

  getState(): AnimState {
    return this.state;
  }

  private getSpriteChild(): Phaser.GameObjects.Sprite | null {
    for (const child of this.container.list) {
      if (child instanceof Phaser.GameObjects.Sprite) return child;
    }
    return null;
  }

  private applyFrameMotion(
    x: number,
    y: number,
    scaleX: number,
    scaleY: number,
    angle: number,
  ): void {
    const sprite = this.getSpriteChild();
    if (!sprite) return;

    sprite.x = this.frameBaseX + x;
    sprite.y = this.frameBaseY + y;
    sprite.scaleX = this.frameBaseScaleX * scaleX;
    sprite.scaleY = this.frameBaseScaleY * scaleY;
    sprite.angle = this.frameBaseAngle + angle;
  }

  private clearFrameMotion(): void {
    this.applyFrameMotion(0, 0, 1, 1, 0);
    const sprite = this.getSpriteChild();
    if (sprite) sprite.setAlpha(1);
  }

  private playFrameAnim(action: AnimState): void {
    if (!this.hasFrameAnims) return;
    const spr = this.getSpriteChild();
    if (!spr) return;
    const key = `${this.animPrefix}_${action}`;
    if (this.scene.anims.exists(key)) {
      spr.play(key, true);
    }
  }

  setIdle(): void {
    if (this.dead || this.state === 'idle') return;
    if (
      this.state === 'attack'
      || this.state === 'cast'
      || this.state === 'hurt'
      || this.state === 'dodge'
    ) return;
    this.cancelTweens();
    this.startTransition('idle');
    this.prevState = this.state;
    this.state = 'idle';
    this.animTime = 0;
    this.playFrameAnim('idle');
    if (this.hasFrameAnims) this.clearFrameMotion();
    else this.resetTransform(150);
  }

  /** Force idle state unconditionally — used after respawn to reset from death */
  forceIdle(): void {
    this.dead = false;
    this.cancelTweens();
    this.state = 'idle';
    this.animTime = 0;
    this.playFrameAnim('idle');
    this.clearFrameMotion();
    this.resetTransform(0);
  }

  setWalk(): void {
    if (this.dead || this.state === 'walk') return;
    if (
      this.state === 'attack'
      || this.state === 'cast'
      || this.state === 'hurt'
      || this.state === 'dodge'
    ) return;
    this.cancelTweens();
    this.startTransition('walk');
    this.prevState = this.state;
    this.state = 'walk';
    this.animTime = 0;
    this.baseY = 0;
    this.baseX = 0;
    this.playFrameAnim('walk');
    if (this.hasFrameAnims) this.clearFrameMotion();
  }

  /** Freeze animation for the given duration (ms). Called on damage. */
  triggerHitFreeze(durationMs: number = 35): void {
    this.hitFreezeTimer = Math.max(this.hitFreezeTimer, durationMs);
    this.getSpriteChild()?.anims.pause();
  }

  private startTransition(toState: string): void {
    const key = `${this.state}->${toState}`;
    const ms = CharacterAnimator.TRANSITION_MS[key] ?? 80;
    this.transitionDuration = ms;
    this.transitionProgress = 0;
    this.prevBobY = this.baseY;
    this.prevScaleX = this.container.scaleX;
    this.prevScaleY = this.container.scaleY;
    this.prevAngle = this.container.angle;
  }

  update(delta: number): void {
    if (this.dead) return;

    // Hit-freeze: skip animation updates
    if (this.hitFreezeTimer > 0) {
      this.hitFreezeTimer -= delta;
      if (this.hitFreezeTimer <= 0) {
        this.hitFreezeTimer = 0;
        this.getSpriteChild()?.anims.resume();
      }
      return;
    }

    this.animTime += delta;

    // Frame-based animations handle the visual; we only do light container transforms
    if (this.hasFrameAnims) {
      // Minimal container movement to complement frame animation
      if (this.state === 'idle') {
        this.updateIdleLight();
      } else if (this.state === 'walk') {
        this.updateWalkLight();
      }
    } else {
      // Legacy: full procedural animation for entities without sprite sheets
      if (this.state === 'idle') {
        this.updateIdle();
      } else if (this.state === 'walk') {
        this.updateWalk();
      }
    }

    // Blend transforms if transitioning
    if (this.transitionProgress < 1 && this.transitionDuration > 0) {
      this.transitionProgress = Math.min(1, this.transitionProgress + delta / this.transitionDuration);
    }
    if (this.transitionProgress < 1) {
      const t = this.transitionProgress;
      const eased = t * t * (3 - 2 * t); // smoothstep

      const currentY = this.container.y;
      const currentScaleX = this.container.scaleX;
      const currentScaleY = this.container.scaleY;
      const currentAngle = this.container.angle;

      const baseContainerY = currentY - this.baseY;
      const prevY = baseContainerY + this.prevBobY;

      this.container.y = prevY + (currentY - prevY) * eased;
      this.container.scaleX = this.prevScaleX + (currentScaleX - this.prevScaleX) * eased;
      this.container.scaleY = this.prevScaleY + (currentScaleY - this.prevScaleY) * eased;
      this.container.angle = this.prevAngle + (currentAngle - this.prevAngle) * eased;
    }
  }

  // ── Light container transforms (complement frame animations) ────────

  private updateIdleLight(): void {
    const phase = (this.animTime / this.config.idleBobSpeed) * Math.PI * 2;
    const breath = Math.sin(phase);
    const bobY = breath * this.config.idleBobAmount * 0.32;
    const swayX = Math.sin(phase * 0.7) * this.config.idleSwayX * 0.35;
    const pulse = breath * this.config.idleScalePulse * 0.35;
    this.applyFrameMotion(swayX, bobY, 1 - pulse * 0.25, 1 + pulse, breath * 0.2);
  }

  private updateWalkLight(): void {
    const phase = (this.animTime / this.config.walkBobSpeed) * Math.PI * 2;
    const stride = Math.sin(phase);
    const contact = Math.abs(stride);
    const bobY = -contact * this.config.walkBobAmount * 0.28;
    const lean = stride * this.config.walkTilt * 0.12;
    this.applyFrameMotion(
      0,
      bobY,
      1 + (1 - contact) * this.config.walkSquash * 0.18,
      1 - (1 - contact) * this.config.walkSquash * 0.12,
      lean,
    );
  }

  // ── Full procedural animation (fallback for no sprite sheet) ────────

  private updateIdle(): void {
    const phase = (this.animTime / this.config.idleBobSpeed) * Math.PI * 2;

    const newBobY = Math.sin(phase) * this.config.idleBobAmount;
    this.container.y += newBobY - this.baseY;
    this.baseY = newBobY;

    const pulse = Math.sin(phase) * this.config.idleScalePulse;
    this.container.scaleY = 1 + pulse;

    if (this.config.idleSwayX > 0) {
      const newSwayX = Math.sin(phase * 0.7) * this.config.idleSwayX;
      this.container.x += newSwayX - this.baseX;
      this.baseX = newSwayX;
    }

    if (this.config.deathStyle === 'splat') {
      this.container.scaleX = 1 - pulse;
      this.container.scaleY = 1 + pulse;
    } else {
      this.container.scaleX = 1;
    }
  }

  private updateWalk(): void {
    const phase = (this.animTime / this.config.walkBobSpeed) * Math.PI * 2;

    // Asymmetric bob: sharp drop, slow rise
    const rawBob = Math.sin(phase);
    const asymBob = rawBob < 0 ? rawBob : rawBob * 0.6;
    const newBobY = -Math.abs(asymBob) * this.config.walkBobAmount;
    this.container.y += newBobY - this.baseY;
    this.baseY = newBobY;

    // Body tilt with direction lean
    this.container.angle = Math.sin(phase) * this.config.walkTilt;

    // Squash/stretch on contact
    const sinVal = Math.abs(Math.sin(phase));
    if (this.config.deathStyle === 'splat') {
      const stretch = sinVal * this.config.walkSquash;
      this.container.scaleX = 1 + stretch;
      this.container.scaleY = 1 - stretch;
    } else if (sinVal < 0.2) {
      this.container.scaleX = 1 + this.config.walkSquash;
      this.container.scaleY = 1 - this.config.walkSquash;
    } else {
      this.container.scaleX = 1;
      this.container.scaleY = 1;
    }
  }

  // ── Attack Animation ─────────────────────────────────────────────────

  playAttack(targetX: number, targetY: number): void {
    if (this.dead) return;
    if (this.hasFrameAnims) {
      this.playFrameAttack(targetX, targetY);
      return;
    }
    this.cancelTweens();
    this.prevState = this.state;
    this.state = 'attack';
    this.playFrameAnim('attack');

    const originX = this.container.x;
    const originY = this.container.y;

    const dx = targetX - originX;
    const dy = targetY - originY;
    const dist = Math.sqrt(dx * dx + dy * dy);
    const nx = dist > 0 ? dx / dist : 0;
    const ny = dist > 0 ? dy / dist : 0;

    const targetAngle = Math.atan2(dy, dx) * (180 / Math.PI);
    const tiltAngle = targetAngle * 0.05;

    const total = this.config.attackDuration;
    const anticipateMs = this.config.attackWindup;
    const strikeMs = 80;
    const impactMs = 40;
    const followMs = Math.max(60, (total - anticipateMs - strikeMs - impactMs) * 0.5);
    const settleMs = Math.max(50, total - anticipateMs - strikeMs - impactMs - followMs);

    const pullbackX = originX - nx * this.config.attackLunge * 0.4;
    const pullbackY = originY - ny * this.config.attackLunge * 0.4;
    const lungeX = originX + nx * this.config.attackLunge;
    const lungeY = originY + ny * this.config.attackLunge;
    const overshootX = originX + nx * this.config.attackLunge * 0.3;
    const overshootY = originY + ny * this.config.attackLunge * 0.3;

    // Phase 1: Anticipation — pull back, compress
    this.addTween({
      targets: this.container,
      x: pullbackX,
      y: pullbackY,
      scaleY: 0.88,
      scaleX: 1.06,
      angle: -tiltAngle,
      duration: anticipateMs,
      ease: 'Back.easeIn',
      onComplete: () => {
        // Phase 2: Strike — fast snap forward
        this.addTween({
          targets: this.container,
          x: lungeX,
          y: lungeY,
          scaleY: 1.05,
          scaleX: 0.95,
          angle: tiltAngle * 1.5,
          duration: strikeMs,
          ease: 'Expo.easeOut',
          onComplete: () => {
            // Phase 3: Impact — squash + screen shake
            this.container.scaleX = 1 + this.config.attackSquash;
            this.container.scaleY = 1 - this.config.attackSquash;

            if (this.config.attackShake && this.scene.cameras?.main) {
              this.scene.cameras.main.shake(60, 0.004);
            }

            // Phase 4: Follow-through — overshoot
            this.scene.time.delayedCall(impactMs, () => {
              this.addTween({
                targets: this.container,
                x: overshootX,
                y: overshootY,
                scaleX: 1.02,
                scaleY: 0.98,
                angle: tiltAngle * 0.5,
                duration: followMs,
                ease: 'Quad.easeOut',
                onComplete: () => {
                  // Phase 5: Settle — elastic return
                  this.addTween({
                    targets: this.container,
                    x: originX,
                    y: originY,
                    scaleX: 1,
                    scaleY: 1,
                    angle: 0,
                    duration: settleMs,
                    ease: 'Elastic.easeOut',
                    onComplete: () => {
                      this.state = 'idle';
                      this.animTime = 0;
                      this.baseY = 0;
                      this.baseX = 0;
                      this.playFrameAnim('idle');
                    },
                  });
                },
              });
            });
          },
        });
      },
    });
  }

  private playFrameAttack(targetX: number, targetY: number): void {
    const sprite = this.getSpriteChild();
    if (!sprite) return;

    this.cancelTweens();
    this.clearFrameMotion();
    this.startTransition('attack');
    this.prevState = this.state;
    this.state = 'attack';
    this.animTime = 0;
    this.playFrameAnim('attack');

    const dx = targetX - this.container.x;
    const dy = targetY - this.container.y;
    const distance = Math.hypot(dx, dy) || 1;
    const nx = dx / distance;
    const ny = dy / distance;
    sprite.setFlipX(dx < 0);

    const total = this.config.attackDuration;
    const windupMs = Math.min(this.config.attackWindup, total * 0.35);
    const strikeMs = Math.max(55, total * 0.2);
    const recoverMs = Math.max(80, total - windupMs - strikeMs);
    const localLunge = Math.min(10, this.config.attackLunge * 0.55);

    this.addTween({
      targets: sprite,
      x: this.frameBaseX - nx * localLunge * 0.35,
      y: this.frameBaseY - ny * localLunge * 0.2 + 1,
      scaleX: this.frameBaseScaleX * 1.04,
      scaleY: this.frameBaseScaleY * 0.94,
      angle: this.frameBaseAngle - Math.sign(dx || 1) * 3,
      duration: windupMs,
      ease: 'Sine.easeInOut',
      onComplete: () => {
        if (this.state !== 'attack') return;
        this.addTween({
          targets: sprite,
          x: this.frameBaseX + nx * localLunge,
          y: this.frameBaseY + ny * localLunge * 0.45 - 1,
          scaleX: this.frameBaseScaleX * 0.97,
          scaleY: this.frameBaseScaleY * 1.04,
          angle: this.frameBaseAngle + Math.sign(dx || 1) * 4,
          duration: strikeMs,
          ease: 'Expo.easeOut',
          onComplete: () => {
            if (this.state !== 'attack') return;
            if (this.config.attackShake && this.scene.cameras?.main) {
              this.scene.cameras.main.shake(45, 0.0025);
            }
            this.addTween({
              targets: sprite,
              x: this.frameBaseX,
              y: this.frameBaseY,
              scaleX: this.frameBaseScaleX,
              scaleY: this.frameBaseScaleY,
              angle: this.frameBaseAngle,
              duration: recoverMs,
              ease: 'Cubic.easeOut',
              onComplete: () => this.finishFrameAction('attack'),
            });
          },
        });
      },
    });
  }

  // ── Cast Animation ────────────────────────────────────────────────────

  playCast(): void {
    if (this.dead) return;
    if (this.hasFrameAnims) {
      this.playFrameCast();
      return;
    }
    this.cancelTweens();
    this.prevState = this.state;
    this.state = 'cast';

    this.playFrameAnim('cast');

    const originY = this.container.y;
    const chargeMs = this.config.castDuration * 0.4;
    const releaseMs = this.config.castDuration * 0.3;
    const settleMs = this.config.castDuration * 0.3;

    this.addTween({
      targets: this.container,
      y: originY + this.config.castLean,
      scaleX: 1.05,
      scaleY: 1.05,
      duration: chargeMs,
      ease: 'Quad.easeOut',
      onComplete: () => {
        if (this.config.castGlow) {
          this.tintFlash(0xaaaaff, 120);
        }

        this.addTween({
          targets: this.container,
          y: originY - this.config.castLean * 1.5,
          scaleX: 1,
          scaleY: 1,
          duration: releaseMs,
          ease: 'Quad.easeIn',
          onComplete: () => {
            this.addTween({
              targets: this.container,
              y: originY,
              scaleX: 1,
              scaleY: 1,
              angle: 0,
              duration: settleMs,
              ease: 'Sine.easeOut',
              onComplete: () => {
                this.state = 'idle';
                this.animTime = 0;
                this.baseY = 0;
                this.baseX = 0;
                this.playFrameAnim('idle');
              },
            });
          },
        });
      },
    });
  }

  private playFrameCast(): void {
    const sprite = this.getSpriteChild();
    if (!sprite) return;

    this.cancelTweens();
    this.clearFrameMotion();
    this.startTransition('cast');
    this.prevState = this.state;
    this.state = 'cast';
    this.animTime = 0;
    this.playFrameAnim('cast');

    const total = this.config.castDuration;
    const chargeMs = total * 0.46;
    const releaseMs = total * 0.2;
    const recoverMs = total - chargeMs - releaseMs;

    this.addTween({
      targets: sprite,
      y: this.frameBaseY + this.config.castLean * 0.45,
      scaleX: this.frameBaseScaleX * 1.035,
      scaleY: this.frameBaseScaleY * 0.965,
      angle: this.frameBaseAngle - 1.5,
      duration: chargeMs,
      ease: 'Sine.easeInOut',
      onComplete: () => {
        if (this.state !== 'cast') return;
        if (this.config.castGlow) this.tintFlash(0xb9a5ff, 120);
        this.addTween({
          targets: sprite,
          y: this.frameBaseY - this.config.castLean * 0.8,
          scaleX: this.frameBaseScaleX * 0.98,
          scaleY: this.frameBaseScaleY * 1.05,
          angle: this.frameBaseAngle + 1,
          duration: releaseMs,
          ease: 'Expo.easeOut',
          onComplete: () => {
            if (this.state !== 'cast') return;
            this.addTween({
              targets: sprite,
              x: this.frameBaseX,
              y: this.frameBaseY,
              scaleX: this.frameBaseScaleX,
              scaleY: this.frameBaseScaleY,
              angle: this.frameBaseAngle,
              duration: recoverMs,
              ease: 'Cubic.easeOut',
              onComplete: () => this.finishFrameAction('cast'),
            });
          },
        });
      },
    });
  }

  playDodge(directionX: number, directionY: number): void {
    if (this.dead) return;
    const sprite = this.getSpriteChild();
    if (!this.hasFrameAnims || !sprite) {
      this.playFallbackDodge(directionX, directionY);
      return;
    }

    this.cancelTweens();
    this.clearFrameMotion();
    this.startTransition('dodge');
    this.prevState = this.state;
    this.state = 'dodge';
    this.animTime = 0;
    this.playFrameAnim('dodge');

    const distance = Math.hypot(directionX, directionY) || 1;
    const nx = directionX / distance;
    const ny = directionY / distance;
    sprite.setFlipX(nx < 0);

    const tuckMs = this.config.dodgeDuration * 0.3;
    const releaseMs = this.config.dodgeDuration - tuckMs;
    this.addTween({
      targets: sprite,
      x: this.frameBaseX + nx * 7,
      y: this.frameBaseY + ny * 3 + 3,
      scaleX: this.frameBaseScaleX * 1.12,
      scaleY: this.frameBaseScaleY * 0.72,
      angle: this.frameBaseAngle + Math.sign(nx || 1) * 7,
      alpha: 0.68,
      duration: tuckMs,
      ease: 'Expo.easeOut',
      onComplete: () => {
        if (this.state !== 'dodge') return;
        this.addTween({
          targets: sprite,
          x: this.frameBaseX,
          y: this.frameBaseY,
          scaleX: this.frameBaseScaleX,
          scaleY: this.frameBaseScaleY,
          angle: this.frameBaseAngle,
          alpha: 1,
          duration: releaseMs,
          ease: 'Cubic.easeOut',
          onComplete: () => this.finishFrameAction('dodge'),
        });
      },
    });
  }

  private playFallbackDodge(directionX: number, directionY: number): void {
    this.cancelTweens();
    this.prevState = this.state;
    this.state = 'dodge';
    const direction = Math.sign(directionX || directionY || 1);
    this.addTween({
      targets: this.container,
      angle: direction * 8,
      scaleX: 1.12,
      scaleY: 0.72,
      alpha: 0.7,
      duration: this.config.dodgeDuration * 0.3,
      ease: 'Expo.easeOut',
      yoyo: true,
      onComplete: () => {
        if (this.state !== 'dodge') return;
        this.container.setAlpha(1).setAngle(0).setScale(1);
        this.state = 'idle';
      },
    });
  }

  playResonance(color = 0xffc15a): void {
    if (this.dead) return;
    const sprite = this.getSpriteChild();
    if (sprite) this.tintFlash(color, 180);

    const ring = this.scene.add.ellipse(
      this.container.x,
      this.container.y + 2,
      22,
      8,
      color,
      0.22,
    ).setDepth(this.container.depth - 1);
    this.scene.tweens.add({
      targets: ring,
      scaleX: 1.8,
      scaleY: 1.5,
      alpha: 0,
      duration: 360,
      ease: 'Cubic.easeOut',
      onComplete: () => ring.destroy(),
    });
  }

  // ── Hurt Animation ────────────────────────────────────────────────────

  playHurt(sourceX: number, sourceY: number): void {
    if (this.dead) return;
    if (this.state === 'death') return;
    if (this.hasFrameAnims) {
      this.playFrameHurt(sourceX, sourceY);
      return;
    }

    const savedState = this.state;
    this.cancelTweens();
    this.state = 'hurt';

    this.playFrameAnim('hurt');

    const originX = this.container.x;
    const originY = this.container.y;

    const dx = originX - sourceX;
    const dy = originY - sourceY;
    const dist = Math.sqrt(dx * dx + dy * dy);
    const nx = dist > 0 ? dx / dist : 0;
    const ny = dist > 0 ? dy / dist : -1;

    if (this.config.hurtFlash) {
      this.tintFlash(0xff4444, 100);
    }

    this.container.x = originX + nx * this.config.hurtKnockback;
    this.container.y = originY + ny * this.config.hurtKnockback;
    this.container.scaleX = 0.9;
    this.container.scaleY = 1.1;

    this.addTween({
      targets: this.container,
      x: originX,
      y: originY,
      scaleX: 1,
      scaleY: 1,
      duration: this.config.hurtDuration,
      ease: 'Quad.easeOut',
      onComplete: () => {
        this.state = savedState;
        this.animTime = 0;
        this.baseY = 0;
        this.baseX = 0;
        if (savedState === 'idle') this.playFrameAnim('idle');
        else if (savedState === 'walk') this.playFrameAnim('walk');
      },
    });
  }

  private playFrameHurt(sourceX: number, sourceY: number): void {
    const sprite = this.getSpriteChild();
    if (!sprite) return;

    const savedState = this.state === 'walk' ? 'walk' : 'idle';
    this.cancelTweens();
    this.clearFrameMotion();
    this.prevState = this.state;
    this.state = 'hurt';
    this.animTime = 0;
    this.playFrameAnim('hurt');

    const dx = this.container.x - sourceX;
    const dy = this.container.y - sourceY;
    const distance = Math.hypot(dx, dy) || 1;
    const nx = dx / distance;
    const ny = dy / distance;
    const recoil = Math.min(8, this.config.hurtKnockback * 0.65);
    if (this.config.hurtFlash) this.tintFlash(0xff5b5b, 90);

    this.addTween({
      targets: sprite,
      x: this.frameBaseX + nx * recoil,
      y: this.frameBaseY + ny * recoil * 0.45 + 2,
      scaleX: this.frameBaseScaleX * 0.9,
      scaleY: this.frameBaseScaleY * 1.08,
      angle: this.frameBaseAngle + Math.sign(nx || 1) * 4,
      duration: this.config.hurtDuration * 0.38,
      ease: 'Expo.easeOut',
      onComplete: () => {
        if (this.state !== 'hurt') return;
        this.addTween({
          targets: sprite,
          x: this.frameBaseX,
          y: this.frameBaseY,
          scaleX: this.frameBaseScaleX,
          scaleY: this.frameBaseScaleY,
          angle: this.frameBaseAngle,
          duration: this.config.hurtDuration * 0.62,
          ease: 'Cubic.easeOut',
          onComplete: () => {
            if (this.state !== 'hurt') return;
            this.state = savedState;
            this.animTime = 0;
            this.playFrameAnim(savedState);
          },
        });
      },
    });
  }

  // ── Death Animation ───────────────────────────────────────────────────

  playDeath(onComplete?: () => void): void {
    this.cancelTweens();
    this.dead = true;
    this.state = 'death';

    this.playFrameAnim('death');

    const duration = this.config.deathDuration;
    const sprite = this.getSpriteChild();
    if (this.hasFrameAnims && sprite) {
      this.clearFrameMotion();
      this.addTween({
        targets: sprite,
        y: this.frameBaseY + 5,
        alpha: 0.12,
        duration: duration * 0.75,
        delay: duration * 0.25,
        ease: 'Cubic.easeIn',
        onComplete,
      });
      return;
    }

    switch (this.config.deathStyle) {
      case 'collapse':
        this.addTween({
          targets: this.container,
          angle: 90,
          scaleY: 0.2,
          y: this.container.y + 10,
          alpha: 0,
          duration,
          ease: 'Quad.easeIn',
          onComplete: () => {
            if (onComplete) onComplete();
          },
        });
        break;

      case 'splat':
        this.addTween({
          targets: this.container,
          scaleX: 2,
          scaleY: 0.1,
          alpha: 0,
          duration,
          ease: 'Quad.easeIn',
          onComplete: () => {
            if (onComplete) onComplete();
          },
        });
        break;

      case 'dissolve': {
        const flickerDuration = Math.min(400, duration * 0.6);
        const flickerInterval = flickerDuration / 8;
        const remainingDuration = duration - flickerDuration;

        for (let i = 0; i < 8; i++) {
          this.scene.time.delayedCall(i * flickerInterval, () => {
            if (this.container && this.container.active) {
              this.container.alpha = i % 2 === 0 ? 0.2 : 0.8;
            }
          });
        }

        this.scene.time.delayedCall(flickerDuration, () => {
          if (!this.container || !this.container.active) {
            if (onComplete) onComplete();
            return;
          }
          this.addTween({
            targets: this.container,
            scaleX: 0.3,
            scaleY: 0.3,
            alpha: 0,
            duration: remainingDuration,
            ease: 'Quad.easeIn',
            onComplete: () => {
              if (onComplete) onComplete();
            },
          });
        });
        break;
      }
    }
  }

  // ── Private Helpers ──────────────────────────────────────────────────

  private finishFrameAction(expectedState: AnimState): void {
    if (this.dead || this.state !== expectedState) return;
    this.clearFrameMotion();
    this.prevState = this.state;
    this.state = 'idle';
    this.animTime = 0;
    this.playFrameAnim('idle');
  }

  private addTween(config: Phaser.Types.Tweens.TweenBuilderConfig): Phaser.Tweens.Tween {
    const tween = this.scene.tweens.add({
      ...config,
      onComplete: (...args: unknown[]) => {
        const idx = this.tweens.indexOf(tween);
        if (idx !== -1) this.tweens.splice(idx, 1);
        if (config.onComplete) {
          (config.onComplete as (...a: unknown[]) => void)(...args);
        }
      },
    });
    this.tweens.push(tween);
    return tween;
  }

  private cancelTweens(): void {
    for (const tween of this.tweens) {
      if (tween && tween.isPlaying()) {
        tween.stop();
        tween.destroy();
      }
    }
    this.tweens = [];
  }

  private resetTransform(duration: number): void {
    this.addTween({
      targets: this.container,
      scaleX: 1,
      scaleY: 1,
      angle: 0,
      duration,
      ease: 'Sine.easeOut',
      onComplete: () => {
        this.container.y -= this.baseY;
        this.container.x -= this.baseX;
        this.baseY = 0;
        this.baseX = 0;
      },
    });
  }

  private tintFlash(color: number, duration: number): void {
    const children = this.container.list;
    for (const child of children) {
      if (child instanceof Phaser.GameObjects.Sprite) {
        child.setTint(color);
      } else if (child instanceof Phaser.GameObjects.Image) {
        child.setTint(color);
      } else if (child instanceof Phaser.GameObjects.Rectangle && child.visible) {
        child.setFillStyle(color);
      }
    }
    this.scene.time.delayedCall(duration, () => {
      if (!this.container || !this.container.active) return;
      for (const child of this.container.list) {
        if (child instanceof Phaser.GameObjects.Sprite) {
          child.clearTint();
        } else if (child instanceof Phaser.GameObjects.Image) {
          child.clearTint();
        }
      }
    });
  }

  cleanup(): void {
    this.cancelTweens();
    this.dead = true;
  }
}
