import Phaser from 'phaser';
import type { RenderQualityProfile } from '../rendering/RenderQuality';

export interface LightSource {
  x: number;
  y: number;
  radius: number;
  color: number;
  intensity: number;
  flicker?: boolean;
  id?: string;
}

interface ZoneAmbient {
  color: number;
  alpha: number;
  fogColor?: number;
  fogAlpha?: number;
}

const ZONE_AMBIENTS: Record<string, ZoneAmbient> = {
  emerald_plains: { color: 0x040610, alpha: 0.10, fogColor: 0x112211, fogAlpha: 0.03 },
  twilight_forest: { color: 0x020408, alpha: 0.22, fogColor: 0x0a1010, fogAlpha: 0.05 },
  anvil_mountains: { color: 0x080608, alpha: 0.18, fogColor: 0x100808, fogAlpha: 0.04 },
  scorching_desert: { color: 0x0c0804, alpha: 0.08, fogColor: 0x120e04, fogAlpha: 0.02 },
  abyss_rift: { color: 0x040004, alpha: 0.32, fogColor: 0x100010, fogAlpha: 0.06 },
};

const OVERLAY_DEPTH = 3000;
const LIGHT_TEXTURE = 'lighting_radial_gpu';

/**
 * GPU-composited viewport lighting.
 *
 * The former implementation rasterized every gradient into a Canvas2D texture
 * and uploaded that texture repeatedly. This implementation uploads one radial
 * falloff at startup and thereafter only changes batched sprite transforms.
 */
export class LightingSystem {
  private readonly scene: Phaser.Scene;
  private readonly quality: RenderQualityProfile;
  private readonly ambient: Phaser.GameObjects.Rectangle;
  private readonly fog: Phaser.GameObjects.Image;
  private readonly lightSprites: Phaser.GameObjects.Image[] = [];
  private readonly lights: LightSource[] = [];
  private readonly flickerSeeds = new Map<string, number>();
  private ambientAlpha = 0.35;
  private time = 0;
  private lastUpdate = Number.NEGATIVE_INFINITY;

  constructor(scene: Phaser.Scene, quality: RenderQualityProfile) {
    this.scene = scene;
    this.quality = quality;
    this.ensureRadialTexture();
    const cam = scene.cameras.main;
    this.ambient = scene.add.rectangle(0, 0, cam.width, cam.height, 0x040610, 1)
      .setOrigin(0).setScrollFactor(0).setDepth(OVERLAY_DEPTH)
      .setBlendMode(Phaser.BlendModes.MULTIPLY);
    this.fog = scene.add.image(cam.width / 2, cam.height / 2, LIGHT_TEXTURE)
      .setScrollFactor(0).setDepth(OVERLAY_DEPTH + 1)
      .setBlendMode(Phaser.BlendModes.MULTIPLY).setAlpha(0.03);
    this.resizeViewport();
  }

  private ensureRadialTexture(): void {
    if (this.scene.textures.exists(LIGHT_TEXTURE)) return;
    const size = 128;
    const canvas = document.createElement('canvas');
    canvas.width = size;
    canvas.height = size;
    const ctx = canvas.getContext('2d');
    if (!ctx) throw new Error('Canvas2D is required to initialize the lighting falloff texture');
    const gradient = ctx.createRadialGradient(size / 2, size / 2, 0, size / 2, size / 2, size / 2);
    gradient.addColorStop(0, 'rgba(255,255,255,1)');
    gradient.addColorStop(0.15, 'rgba(255,255,255,0.9)');
    gradient.addColorStop(0.4, 'rgba(255,255,255,0.6)');
    gradient.addColorStop(0.7, 'rgba(255,255,255,0.25)');
    gradient.addColorStop(1, 'rgba(255,255,255,0)');
    ctx.fillStyle = gradient;
    ctx.fillRect(0, 0, size, size);
    this.scene.textures.addCanvas(LIGHT_TEXTURE, canvas);
  }

  private resizeViewport(): void {
    const cam = this.scene.cameras.main;
    this.ambient.setSize(cam.width, cam.height).setDisplaySize(cam.width, cam.height);
    this.fog.setPosition(cam.width / 2, cam.height / 2).setDisplaySize(cam.width * 1.25, cam.height * 1.25);
  }

  setZone(zoneId: string): void {
    const config = ZONE_AMBIENTS[zoneId];
    if (!config) return;
    this.ambientAlpha = config.alpha;
    this.ambient.setFillStyle(config.color, 1).setAlpha(config.alpha);
    this.fog.setTint(config.fogColor ?? 0x111111).setAlpha(config.fogAlpha ?? 0.03);
  }

  addLight(light: LightSource): void {
    this.lights.push(light);
    if (light.id && light.flicker) this.flickerSeeds.set(light.id, Math.random() * 1000);
  }

  removeLight(id: string): void {
    const index = this.lights.findIndex(light => light.id === id);
    if (index >= 0) this.lights.splice(index, 1);
    this.flickerSeeds.delete(id);
  }

  clearLights(): void {
    this.lights.length = 0;
    this.flickerSeeds.clear();
    this.lightSprites.forEach(sprite => sprite.setVisible(false));
  }

  private spriteAt(index: number): Phaser.GameObjects.Image {
    let sprite = this.lightSprites[index];
    if (!sprite) {
      sprite = this.scene.add.image(0, 0, LIGHT_TEXTURE)
        .setScrollFactor(0).setDepth(OVERLAY_DEPTH + 2)
        .setBlendMode(Phaser.BlendModes.ADD);
      this.lightSprites.push(sprite);
    }
    return sprite;
  }

  update(delta: number): void {
    this.time += delta;
    if (this.time - this.lastUpdate < this.quality.lightingUpdateIntervalMs) return;
    this.lastUpdate = this.time;

    const cam = this.scene.cameras.main;
    this.resizeViewport();
    this.ambient.setAlpha(Math.max(0, Math.min(1, this.ambientAlpha + Math.sin(this.time * 0.0015) * 0.015)));
    this.fog.setPosition(
      cam.width / 2 + Math.sin(this.time * 0.0008) * cam.width * 0.15,
      cam.height / 2 + Math.cos(this.time * 0.00056) * cam.height * 0.1,
    );

    const originX = cam.width * cam.originX;
    const originY = cam.height * cam.originY;
    const visible = this.lights
      .map(light => {
        const x = (light.x - cam.scrollX - originX) * cam.zoom + originX;
        const y = (light.y - cam.scrollY - originY) * cam.zoom + originY;
        return { light, x, y, distance: (x - originX) ** 2 + (y - originY) ** 2 };
      })
      .filter(({ light, x, y }) => {
        const radius = light.radius * cam.zoom;
        return Number.isFinite(radius) && radius > 0 && x + radius >= 0 && x - radius <= cam.width
          && y + radius >= 0 && y - radius <= cam.height;
      })
      .sort((a, b) => a.distance - b.distance)
      .slice(0, this.quality.maxDynamicLights);

    visible.forEach(({ light, x, y }, index) => {
      let intensity = light.intensity;
      if (light.flicker) {
        const seed = this.flickerSeeds.get(light.id ?? '') ?? 0;
        intensity += Math.sin(this.time * 0.007 + seed) * 0.05
          + Math.sin(this.time * 0.013 + seed * 2.3) * 0.03;
      }
      this.spriteAt(index)
        .setVisible(true).setPosition(x, y)
        .setDisplaySize(light.radius * cam.zoom * 2, light.radius * cam.zoom * 2)
        // A light restores only the luminance removed by the ambient layer.
        // Mapping raw intensity directly to ADD alpha overexposes the scene.
        .setTint(light.color).setAlpha(Math.max(0, Math.min(1, intensity * this.ambientAlpha)));
    });
    for (let i = visible.length; i < this.lightSprites.length; i++) this.lightSprites[i].setVisible(false);
  }

  destroy(): void {
    this.ambient.destroy();
    this.fog.destroy();
    this.lightSprites.forEach(sprite => sprite.destroy());
    this.lightSprites.length = 0;
    this.lights.length = 0;
    this.flickerSeeds.clear();
  }
}
