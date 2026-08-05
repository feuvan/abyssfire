import Phaser from 'phaser';
import { GAME_WIDTH, GAME_HEIGHT } from '../config';

/**
 * Render-texture based trail effects: weapon slash trails, ground scorch marks, dash ghosts.
 * Uses two RenderTextures: one for weapon trails (entity depth), one for ground marks.
 */
export class TrailRenderer {
  private scene: Phaser.Scene;
  private trailRT: Phaser.GameObjects.RenderTexture;
  private groundRT: Phaser.GameObjects.RenderTexture;
  private trailImage: Phaser.GameObjects.Image;
  private groundImage: Phaser.GameObjects.Image;
  private slashStamp: Phaser.GameObjects.Image;
  private groundStamp: Phaser.GameObjects.Image;
  private ghostStamp: Phaser.GameObjects.Image;
  private fadeCounter = 0;

  constructor(scene: Phaser.Scene) {
    this.scene = scene;

    // Weapon trail layer — between entities and UI
    this.trailRT = scene.make.renderTexture({ x: 0, y: 0, width: GAME_WIDTH, height: GAME_HEIGHT }, false);
    this.trailImage = scene.add.image(0, 0, '__DEFAULT').setOrigin(0, 0);
    this.trailImage.setTexture(this.trailRT.texture.key);
    this.trailImage.setScrollFactor(0);
    this.trailImage.setDepth(1499);
    this.trailImage.setBlendMode(Phaser.BlendModes.ADD);

    // Ground scorch layer — below entities
    this.groundRT = scene.make.renderTexture({ x: 0, y: 0, width: GAME_WIDTH, height: GAME_HEIGHT }, false);
    this.groundImage = scene.add.image(0, 0, '__DEFAULT').setOrigin(0, 0);
    this.groundImage.setTexture(this.groundRT.texture.key);
    this.groundImage.setScrollFactor(0);
    this.groundImage.setDepth(1);
    this.groundImage.setAlpha(0.7);

    this.slashStamp = scene.make.image({ x: 0, y: 0, key: 'particle_slash' }, false);
    this.groundStamp = scene.make.image({ x: 0, y: 0, key: 'particle_smoke' }, false);
    this.ghostStamp = scene.make.image({ x: 0, y: 0, key: 'particle_circle' }, false);
  }

  // ── Weapon Slash Trail ──────────────────────────────────

  stampSlash(worldX: number, worldY: number, angle: number, color: number = 0xffffff, length: number = 30): void {
    const cam = this.scene.cameras.main;
    const screenX = (worldX - cam.scrollX) * cam.zoom;
    const screenY = (worldY - cam.scrollY) * cam.zoom;
    const scaleX = Math.max(0.4, (length * 2) / 32);

    this.slashStamp
      .setPosition(screenX, screenY)
      .setRotation(angle)
      .setTint(color)
      .setAlpha(0.28)
      .setScale(scaleX * 1.25, 1.1);
    this.trailRT.draw(this.slashStamp);

    this.slashStamp
      .setAlpha(0.85)
      .setScale(scaleX, 0.45);
    this.trailRT.draw(this.slashStamp);
  }

  // ── Ground Scorch Mark ──────────────────────────────────

  stampGround(worldX: number, worldY: number, type: 'fire' | 'ice' | 'lightning' = 'fire', radius: number = 20): void {
    const cam = this.scene.cameras.main;
    const screenX = (worldX - cam.scrollX) * cam.zoom;
    const screenY = (worldY - cam.scrollY) * cam.zoom;

    const colors: Record<string, number> = {
      fire: 0x331100,
      ice: 0x112233,
      lightning: 0x111133,
    };

    const r = radius * cam.zoom;
    const scale = Math.max(0.2, (r * 2) / 24);

    this.groundStamp
      .setPosition(screenX, screenY)
      .setTint(colors[type] || 0x222222)
      .setAlpha(0.28)
      .setScale(scale);
    this.groundRT.draw(this.groundStamp);

    this.groundStamp
      .setAlpha(0.16)
      .setScale(scale * 0.6);
    this.groundRT.draw(this.groundStamp);
  }

  // ── Dash Ghost Trail ────────────────────────────────────

  stampGhost(
    worldX: number,
    worldY: number,
    textureKey: string,
    options: {
      frame?: string | number;
      alpha?: number;
      tint?: number;
      scaleX?: number;
      scaleY?: number;
      flipX?: boolean;
      angle?: number;
    } = {},
  ): void {
    const cam = this.scene.cameras.main;
    const screenX = (worldX - cam.scrollX) * cam.zoom;
    const screenY = (worldY - cam.scrollY) * cam.zoom;

    if (this.scene.textures.exists(textureKey)) {
      this.ghostStamp
        .setTexture(textureKey)
        .setFrame(options.frame ?? 0)
        .setPosition(screenX, screenY)
        .setAlpha(options.alpha ?? 0.4)
        .setTint(options.tint ?? 0x4444ff)
        .setScale(
          (options.scaleX ?? 1) * cam.zoom,
          (options.scaleY ?? options.scaleX ?? 1) * cam.zoom,
        )
        .setFlipX(options.flipX ?? false)
        .setAngle(options.angle ?? 0);
      this.trailRT.draw(this.ghostStamp);
    }
  }

  // ── Per-Frame Fade ──────────────────────────────────────

  update(): void {
    this.fadeCounter++;
    // Weapon trails fade fast (every 2 frames)
    if (this.fadeCounter % 2 === 0) {
      this.trailRT.fill(0x000000, 0.15);
    }
    // Ground marks fade very slowly (every 10 frames)
    if (this.fadeCounter % 10 === 0) {
      this.groundRT.fill(0x000000, 0.01);
    }
  }

  // ── Cleanup ─────────────────────────────────────────────

  destroy(): void {
    this.trailRT.destroy();
    this.groundRT.destroy();
    this.trailImage.destroy();
    this.groundImage.destroy();
    this.slashStamp.destroy();
    this.groundStamp.destroy();
    this.ghostStamp.destroy();
  }
}
