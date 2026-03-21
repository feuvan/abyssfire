import Phaser from 'phaser';
import { EventBus, GameEvents } from '../utils/EventBus';

/**
 * Centralized VFX manager — camera effects, per-GameObject FX, combat juice.
 * Listens to EventBus for automatic triggers; also exposes manual API.
 */
export class VFXManager {
  private scene: Phaser.Scene;
  private isWebGL: boolean;

  // Throttle timestamps
  private lastShakeTime = 0;
  private lastFlashTime = 0;

  // Low HP vignette
  private dangerVignette: Phaser.FX.Vignette | null = null;
  private dangerActive = false;

  constructor(scene: Phaser.Scene) {
    this.scene = scene;
    this.isWebGL = scene.renderer.type === Phaser.WEBGL;
    this.setupEventListeners();
  }

  // ── Event-driven VFX ────────────────────────────────────

  private setupEventListeners(): void {
    // Combat damage — camera shake + flash on crits, scaled by damage ratio
    EventBus.on(GameEvents.COMBAT_DAMAGE, (data: {
      targetId: string;
      damage: number;
      isDodged: boolean;
      isCrit: boolean;
      isPlayerTarget?: boolean;
      damageType?: string;
      targetMaxHP?: number;
    }) => {
      if (data.isDodged) return;
      const maxHP = data.targetMaxHP || 100;
      const ratio = data.damage / maxHP;

      if (data.isPlayerTarget) {
        if (data.isCrit) {
          this.cameraShake(150, 0.008);
        } else if (data.damage > 0) {
          const intensity = Math.max(0.002, Math.min(0.006, ratio * 0.01));
          const duration = Math.max(50, Math.min(120, 50 + ratio * 100));
          this.cameraShake(duration, intensity);
        }
      } else {
        // Player dealing damage — scale with damage dealt
        const intensity = Math.max(0.001, Math.min(0.005, ratio * 0.008));
        const duration = Math.max(40, Math.min(100, 40 + ratio * 80));
        this.cameraShake(duration, intensity);
        if (data.isCrit) {
          this.cameraFlash(50, 0.3, 0xffffff);
        }
      }
    });

    // Level up — gold flash + zoom pulse + celebration particles
    EventBus.on(GameEvents.PLAYER_LEVEL_UP, () => {
      this.cameraFlash(200, 0.5, 0xffd700);
      this.cameraShake(100, 0.004);
      this.cameraZoomPulse(1.5, 200, 1.8);
      // Delayed particle burst (after flash starts)
      this.scene.time.delayedCall(100, () => {
        const cam = this.scene.cameras.main;
        const wv = cam.worldView;
        const centerX = wv.centerX;
        const centerY = wv.centerY;
        this.levelUpBurst(centerX, centerY);
      });
    });

    // Player death — fade to red
    EventBus.on(GameEvents.PLAYER_DIED, () => {
      this.cameraFade(600, 80, 10, 10);
    });

    // Legendary/set item drop — orange flash + shake
    EventBus.on(GameEvents.ITEM_DROPPED, (data: { item: { quality: string } }) => {
      if (data.item.quality === 'legendary' || data.item.quality === 'set') {
        this.cameraFlash(300, 0.6, 0xff8800);
        this.cameraShake(250, 0.015);
      }
    });
  }

  // ── Camera Effects ──────────────────────────────────────

  cameraShake(duration: number, intensity: number): void {
    const now = this.scene.time.now;
    if (now - this.lastShakeTime < 100) return;
    this.lastShakeTime = now;
    this.scene.cameras.main.shake(duration, intensity);
  }

  cameraFlash(duration: number, alpha: number, color: number = 0xffffff): void {
    const now = this.scene.time.now;
    if (now - this.lastFlashTime < 200) return;
    this.lastFlashTime = now;
    const r = (color >> 16) & 0xff;
    const g = (color >> 8) & 0xff;
    const b = color & 0xff;
    this.scene.cameras.main.flash(duration, r, g, b, false, (_cam: Phaser.Cameras.Scene2D.Camera, progress: number) => {
      // Custom alpha curve — fade out faster
      if (progress > 0.3) {
        // Already handled by Phaser's built-in flash
      }
    });
  }

  cameraFade(duration: number, r: number, g: number, b: number, onComplete?: () => void): void {
    this.scene.cameras.main.fade(duration, r, g, b, false, (_cam: Phaser.Cameras.Scene2D.Camera, progress: number) => {
      if (progress >= 1 && onComplete) onComplete();
    });
  }

  cameraFadeIn(duration: number): void {
    this.scene.cameras.main.fadeIn(duration);
  }

  cameraZoomPulse(targetZoom: number, duration: number, originalZoom: number): void {
    const cam = this.scene.cameras.main;
    cam.zoomTo(targetZoom, duration, 'Power2', false, (_cam: Phaser.Cameras.Scene2D.Camera, progress: number) => {
      if (progress >= 1) {
        cam.zoomTo(originalZoom, duration * 0.6, 'Power2');
      }
    });
  }

  // ── Per-GameObject FX (WebGL only) ──────────────────────

  applyBloom(gameObject: Phaser.GameObjects.GameObject, intensity: number = 1): Phaser.FX.Bloom | null {
    if (!this.isWebGL) return null;
    const go = gameObject as any;
    if (!go.postFX) return null;
    return go.postFX.addBloom(0xffffff, intensity, intensity, intensity, 1.2);
  }

  applyGlow(gameObject: Phaser.GameObjects.GameObject, color: number = 0xffffff, distance: number = 4, quality: number = 0.1): Phaser.FX.Glow | null {
    if (!this.isWebGL) return null;
    const go = gameObject as any;
    if (!go.preFX) return null;
    return go.preFX.addGlow(color, distance, 0, false, quality);
  }

  applyShine(gameObject: Phaser.GameObjects.GameObject, speed: number = 0.5, intensity: number = 0.5): Phaser.FX.Shine | null {
    if (!this.isWebGL) return null;
    const go = gameObject as any;
    if (!go.postFX) return null;
    return go.postFX.addShine(speed, intensity, 5);
  }

  applyColorMatrix(gameObject: Phaser.GameObjects.GameObject): Phaser.FX.ColorMatrix | null {
    if (!this.isWebGL) return null;
    const go = gameObject as any;
    if (!go.postFX) return null;
    return go.postFX.addColorMatrix();
  }

  // ── Loot Glow by Quality ───────────────────────────────

  applyLootGlow(container: Phaser.GameObjects.Container, quality: string): void {
    if (!this.isWebGL) return;
    const colors: Record<string, number> = {
      magic: 0x4488ff,
      rare: 0xffff00,
      legendary: 0xff8800,
      set: 0x00ff00,
    };
    const color = colors[quality];
    if (!color) return;

    // Apply glow to the first visible child (the loot bag image)
    const children = container.list as Phaser.GameObjects.GameObject[];
    for (const child of children) {
      const go = child as any;
      if (go.preFX) {
        go.preFX.addGlow(color, 6, 0, false, 0.1);
        // Legendary/set also get a shine sweep
        if (quality === 'legendary' || quality === 'set') {
          go.postFX?.addShine(0.3, 0.5, 5);
        }
        break;
      }
    }
  }

  // ── Hit Flash (Color Matrix) ────────────────────────────

  /** Flash a game object white for one frame */
  hitFlash(target: Phaser.GameObjects.Sprite | Phaser.GameObjects.Image | Phaser.GameObjects.Container): void {
    if (!this.isWebGL) return;
    if (target instanceof Phaser.GameObjects.Container) {
      // Flash all sprite/image children in the container
      const children = target.list as Phaser.GameObjects.GameObject[];
      for (const child of children) {
        if (child instanceof Phaser.GameObjects.Sprite || child instanceof Phaser.GameObjects.Image) {
          this.hitFlash(child);
        }
      }
      return;
    }
    const fx = (target as any).preFX?.addColorMatrix();
    if (!fx) return;
    fx.brightness(1.8);
    this.scene.time.delayedCall(50, () => {
      (target as any).preFX?.remove(fx);
    });
  }

  // ── Status Effect Tints ─────────────────────────────────

  applyStatusTint(sprite: Phaser.GameObjects.Sprite | Phaser.GameObjects.Image, status: 'poison' | 'freeze' | 'burn'): Phaser.FX.ColorMatrix | null {
    if (!this.isWebGL) return null;
    const fx = (sprite as any).preFX?.addColorMatrix();
    if (!fx) return null;
    switch (status) {
      case 'poison': fx.hue(90); fx.saturate(0.5); break;
      case 'freeze': fx.hue(200); fx.saturate(0.8); fx.brightness(1.2); break;
      case 'burn': fx.hue(-20); fx.saturate(0.6); break;
    }
    return fx;
  }

  removeStatusTint(sprite: Phaser.GameObjects.Sprite | Phaser.GameObjects.Image, fx: Phaser.FX.ColorMatrix): void {
    (sprite as any).preFX?.remove(fx);
  }

  // ── Zone Transition (fade out, restart, fade in) ────────

  zoneTransition(callback: () => void): void {
    this.scene.cameras.main.fade(400, 0, 0, 0, false, (_cam: Phaser.Cameras.Scene2D.Camera, progress: number) => {
      if (progress >= 1) {
        callback();
      }
    });
  }

  // ── Skill Impact Effects ────────────────────────────────

  skillImpactBloom(x: number, y: number, color: number = 0xff6600, duration: number = 300): void {
    if (!this.isWebGL) return;
    // Create a brief bloom circle at impact point
    const circle = this.scene.add.circle(x, y, 20, color, 0.6).setDepth(1500);
    const bloomFx = (circle as any).postFX?.addBloom(color, 1.5, 1.5, 1.5, 1.5);
    this.scene.tweens.add({
      targets: circle,
      scaleX: 3,
      scaleY: 3,
      alpha: 0,
      duration,
      ease: 'Power2',
      onComplete: () => {
        if (bloomFx) (circle as any).postFX?.remove(bloomFx);
        circle.destroy();
      },
    });
  }

  // ── Particle Burst Effects (tween-based, reliable) ───────

  /** Burst particles outward from a point using tweened sprites */
  private burstParticles(
    x: number, y: number, count: number,
    texture: string, tints: number[],
    opts: { speedMin?: number; speedMax?: number; scaleStart?: number; duration?: number; gravityY?: number; blend?: number } = {},
  ): void {
    const { speedMin = 40, speedMax = 100, scaleStart = 0.8, duration = 400, gravityY = 0, blend = Phaser.BlendModes.ADD } = opts;
    for (let i = 0; i < count; i++) {
      const tint = tints[Math.floor(Math.random() * tints.length)];
      const p = this.scene.add.image(x, y, texture)
        .setDepth(1501).setTint(tint).setScale(scaleStart).setAlpha(0.9)
        .setBlendMode(blend);
      const angle = Math.random() * Math.PI * 2;
      const speed = speedMin + Math.random() * (speedMax - speedMin);
      const endX = x + Math.cos(angle) * speed;
      const endY = y + Math.sin(angle) * speed + gravityY * (duration / 1000);
      this.scene.tweens.add({
        targets: p,
        x: endX, y: endY,
        alpha: 0, scale: 0.05,
        duration: duration * (0.7 + Math.random() * 0.6),
        ease: 'Power2',
        onComplete: () => p.destroy(),
      });
    }
  }

  /** Hit sparks at a world position (player crit) */
  hitSparks(x: number, y: number, count: number = 12): void {
    this.burstParticles(x, y, count, 'particle_spark',
      [0xffffff, 0xffffaa, 0xffd700],
      { speedMin: 20, speedMax: 55, scaleStart: 0.6, duration: 400 });
  }

  /** Gold particles (loot/gold pickup, monster kill) */
  goldBurst(x: number, y: number, count: number = 6): void {
    this.burstParticles(x, y, count, 'particle_circle',
      [0xffd700, 0xffaa00, 0xffcc33],
      { speedMin: 15, speedMax: 35, duration: 500, gravityY: 80 });
  }

  /** Heal particles at a world position */
  healBurst(x: number, y: number, count: number = 8): void {
    this.burstParticles(x, y, count, 'particle_circle',
      [0x2ecc71, 0x27ae60, 0x66ff66],
      { speedMin: 10, speedMax: 30, duration: 600 });
  }

  /** Death particles at monster kill location */
  deathBurst(x: number, y: number, color: number = 0xff4444): void {
    this.burstParticles(x, y, 10, 'particle_circle',
      [color, 0x888888, 0x444444],
      { speedMin: 20, speedMax: 50, scaleStart: 0.4, duration: 500, gravityY: 60 });
  }

  /** Level-up celebration particles */
  levelUpBurst(x: number, y: number): void {
    this.burstParticles(x, y, 20, 'particle_star',
      [0xffd700, 0xff8800, 0xffcc33, 0xffffff],
      { speedMin: 30, speedMax: 70, scaleStart: 0.5, duration: 800, gravityY: 40 });
  }

  // ── Low HP Danger Vignette ──────────────────────────────

  updateDangerVignette(hpRatio: number): void {
    if (!this.isWebGL) return;
    const cam = this.scene.cameras.main;

    if (hpRatio < 0.3 && hpRatio > 0) {
      if (!this.dangerActive) {
        this.dangerActive = true;
        // Add a red-tinted vignette that intensifies as HP drops
        this.dangerVignette = cam.postFX.addVignette(0.5, 0.5, 0.85, 0.35);
      }
      if (this.dangerVignette) {
        // Pulse the vignette strength based on HP
        const severity = 1 - (hpRatio / 0.3); // 0 at 30%, 1 at 0%
        const pulse = Math.sin(this.scene.time.now * 0.005) * 0.05;
        this.dangerVignette.strength = 0.2 + severity * 0.25 + pulse;
        this.dangerVignette.radius = 0.7 + severity * 0.15;
      }
    } else if (this.dangerActive) {
      this.dangerActive = false;
      if (this.dangerVignette) {
        cam.postFX.remove(this.dangerVignette);
        this.dangerVignette = null;
      }
    }
  }

  // ── Cleanup ─────────────────────────────────────────────

  destroy(): void {
    EventBus.off(GameEvents.COMBAT_DAMAGE);
    EventBus.off(GameEvents.PLAYER_LEVEL_UP);
    EventBus.off(GameEvents.PLAYER_DIED);
    EventBus.off(GameEvents.ITEM_DROPPED);
  }
}
