import Phaser from 'phaser';
import type { MonsterAnimCategory } from '../data/types';

// ── Types ──────────────────────────────────────────────────────────────────

export type AnimState = 'idle' | 'walk' | 'attack' | 'cast' | 'hurt' | 'death';

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

  hurtKnockback: number;
  hurtDuration: number;
  hurtFlash: boolean;

  deathStyle: 'collapse' | 'dissolve' | 'splat';
  deathDuration: number;
}

// ── Preset Configs ─────────────────────────────────────────────────────────

const HUMANOID_CONFIG: AnimConfig = {
  idleBobAmount: 2,
  idleBobSpeed: 1000,
  idleScalePulse: 0.02,
  idleSwayX: 0,

  walkBobAmount: 4,
  walkBobSpeed: 280,
  walkTilt: 5,
  walkSquash: 0.05,

  attackLunge: 12,
  attackDuration: 300,
  attackSquash: 0.15,
  attackWindup: 100,
  attackShake: false,

  castLean: 5,
  castDuration: 350,
  castGlow: false,

  hurtKnockback: 8,
  hurtDuration: 200,
  hurtFlash: true,

  deathStyle: 'collapse',
  deathDuration: 500,
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
    attackSquash: 0.2,
  },

  mage: {
    ...HUMANOID_CONFIG,
    attackLunge: 6,
    castDuration: 450,
    castGlow: true,
  },

  rogue: {
    ...HUMANOID_CONFIG,
    attackDuration: 200,
    walkTilt: 7,
  },
};

export function getAnimConfig(category: string): AnimConfig {
  const preset = PRESETS[category] ?? PRESETS['humanoid'];
  return { ...preset };
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

  constructor(scene: Phaser.Scene, container: Phaser.GameObjects.Container, config: AnimConfig, animPrefix?: string) {
    this.scene = scene;
    this.container = container;
    this.config = config;
    this.animPrefix = animPrefix ?? '';
    this.hasFrameAnims = !!animPrefix && scene.anims.exists(`${animPrefix}_idle`);
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
    if (this.state === 'attack' || this.state === 'cast' || this.state === 'hurt') return;
    this.cancelTweens();
    this.prevState = this.state;
    this.state = 'idle';
    this.playFrameAnim('idle');
    this.resetTransform(150);
  }

  /** Force idle state unconditionally — used after respawn to reset from death */
  forceIdle(): void {
    this.dead = false;
    this.cancelTweens();
    this.state = 'idle';
    this.playFrameAnim('idle');
    this.resetTransform(0);
  }

  setWalk(): void {
    if (this.dead || this.state === 'walk') return;
    if (this.state === 'attack' || this.state === 'cast' || this.state === 'hurt') return;
    this.cancelTweens();
    this.prevState = this.state;
    this.state = 'walk';
    this.animTime = 0;
    this.baseY = 0;
    this.baseX = 0;
    this.playFrameAnim('walk');
  }

  update(delta: number): void {
    if (this.dead) return;
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
  }

  // ── Light container transforms (complement frame animations) ────────

  private updateIdleLight(): void {
    const phase = (this.animTime / this.config.idleBobSpeed) * Math.PI * 2;
    // Very subtle Y bob
    const newBobY = Math.sin(phase) * this.config.idleBobAmount * 0.3;
    this.container.y += newBobY - this.baseY;
    this.baseY = newBobY;

    // Flying sway
    if (this.config.idleSwayX > 0) {
      const newSwayX = Math.sin(phase * 0.7) * this.config.idleSwayX;
      this.container.x += newSwayX - this.baseX;
      this.baseX = newSwayX;
    }
  }

  private updateWalkLight(): void {
    const phase = (this.animTime / this.config.walkBobSpeed) * Math.PI * 2;
    // Very subtle Y bounce
    const newBobY = -Math.abs(Math.sin(phase)) * this.config.walkBobAmount * 0.3;
    this.container.y += newBobY - this.baseY;
    this.baseY = newBobY;
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

    const newBobY = -Math.abs(Math.sin(phase)) * this.config.walkBobAmount;
    this.container.y += newBobY - this.baseY;
    this.baseY = newBobY;

    this.container.angle = Math.sin(phase) * this.config.walkTilt;

    const sinVal = Math.abs(Math.sin(phase));

    if (this.config.deathStyle === 'splat') {
      const stretch = sinVal * this.config.walkSquash;
      this.container.scaleX = 1 + stretch;
      this.container.scaleY = 1 - stretch;
    } else if (sinVal < 0.15) {
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
    this.cancelTweens();
    this.prevState = this.state;
    this.state = 'attack';

    // Play frame animation
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

    const windupMs = this.config.attackWindup;
    const lungeMs = 100;
    const impactMs = 50;
    const recoverMs = Math.max(50, this.config.attackDuration - windupMs - lungeMs - impactMs);

    const pullbackX = originX - nx * this.config.attackLunge * 0.3;
    const pullbackY = originY - ny * this.config.attackLunge * 0.3;

    const lungeX = originX + nx * this.config.attackLunge;
    const lungeY = originY + ny * this.config.attackLunge;

    // 1. Windup: pull back
    this.addTween({
      targets: this.container,
      x: pullbackX,
      y: pullbackY,
      scaleY: 0.92,
      angle: -tiltAngle,
      duration: windupMs,
      ease: 'Quad.easeOut',
      onComplete: () => {
        // 2. Lunge: snap toward target
        this.addTween({
          targets: this.container,
          x: lungeX,
          y: lungeY,
          angle: tiltAngle,
          duration: lungeMs,
          ease: 'Quad.easeIn',
          onComplete: () => {
            // 3. Impact
            this.container.scaleX = 1 + this.config.attackSquash;
            this.container.scaleY = 1 - this.config.attackSquash;

            if (this.config.attackShake && this.scene.cameras?.main) {
              this.scene.cameras.main.shake(80, 0.003);
            }

            // 4. Recover
            this.addTween({
              targets: this.container,
              x: originX,
              y: originY,
              scaleX: 1,
              scaleY: 1,
              angle: 0,
              duration: recoverMs + impactMs,
              ease: 'Quad.easeOut',
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

  // ── Cast Animation ────────────────────────────────────────────────────

  playCast(): void {
    if (this.dead) return;
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

  // ── Hurt Animation ────────────────────────────────────────────────────

  playHurt(sourceX: number, sourceY: number): void {
    if (this.dead) return;
    if (this.state === 'death') return;

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

  // ── Death Animation ───────────────────────────────────────────────────

  playDeath(onComplete?: () => void): void {
    this.cancelTweens();
    this.dead = true;
    this.state = 'death';

    this.playFrameAnim('death');

    const duration = this.config.deathDuration;

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
