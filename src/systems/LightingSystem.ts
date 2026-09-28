import Phaser from 'phaser';
import { RENDER_SCALE } from '../config';
import type { RenderQualityProfile } from '../rendering/RenderQuality';
import { ZONE_MOODS, getCurrentZoneMood } from '../graphics/ZonePalette';
import type { ZoneMood } from '../graphics/ZonePalette';
import type { MapTheme } from '../data/types';

/** Map ids → theme, for callers that pass a zone id. Unknown ids use the active zone theme. */
const ZONE_THEME_BY_ID: Record<string, MapTheme> = {
  emerald_plains: 'plains',
  // The homestead sits on the plains.
  ember_tower: 'plains',
  twilight_forest: 'forest',
  anvil_mountains: 'mountain',
  scorching_desert: 'desert',
  abyss_rift: 'abyss',
};

export interface LightSource {
  x: number;
  y: number;
  radius: number;
  color: number;
  intensity: number;
  flicker?: boolean;
  id?: string;
}

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
  /** Drifting additive haze (zone mood). */
  private readonly fog: Phaser.GameObjects.Image;
  /** Screen-edge vignette tinted per zone (multiply). */
  private readonly vignette: Phaser.GameObjects.Image;
  private hazeAlpha = 0.05;
  /** How much luminance the ambient layer removes; lights add back about this much. */
  private lightScale = 0.2;
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
    // Opaque backdrop (game background colour) so the additive haze and the
    // colour grade treat the empty space around the map uniformly.
    if (cam.transparent) cam.setBackgroundColor(0x0f0f1a);
    this.ambient = scene.add.rectangle(0, 0, cam.width, cam.height, 0x040610, 1)
      .setOrigin(0).setScrollFactor(0).setDepth(OVERLAY_DEPTH)
      .setBlendMode(Phaser.BlendModes.MULTIPLY);
    this.fog = scene.add.image(cam.width / 2, cam.height / 2, LIGHT_TEXTURE)
      .setScrollFactor(0).setDepth(OVERLAY_DEPTH + 1)
      .setBlendMode(Phaser.BlendModes.ADD).setAlpha(0.03);
    this.vignette = scene.add.image(cam.width / 2, cam.height / 2, LIGHT_TEXTURE)
      .setScrollFactor(0).setDepth(OVERLAY_DEPTH)
      .setBlendMode(Phaser.BlendModes.MULTIPLY).setVisible(false);
    this.resizeViewport();
  }

  private ensureRadialTexture(): void {
    if (this.scene.textures.exists(LIGHT_TEXTURE)) return;
    const size = 128;
    const canvas = document.createElement('canvas');
    canvas.width = size;
    canvas.height = size;
    const ctx = canvas.getContext('2d', { willReadFrequently: true });
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

  /**
   * The overlay is laid out in logical screen pixels (1280×720) and placed as
   * screen-fixed objects around the camera centre, so it looks the same at
   * every render scale (the camera zoom includes RENDER_SCALE).
   */
  private view(): { cam: Phaser.Cameras.Scene2D.Camera; w: number; h: number; z: number; ox: number; oy: number; at: (sx: number, sy: number) => [number, number] } {
    const cam = this.scene.cameras.main;
    const w = cam.width / RENDER_SCALE, h = cam.height / RENDER_SCALE;
    const ox = cam.width * cam.originX, oy = cam.height * cam.originY;
    return {
      cam, w, h, z: cam.zoom / RENDER_SCALE, ox, oy,
      at: (sx, sy) => [ox + (sx - w * cam.originX), oy + (sy - h * cam.originY)],
    };
  }

  private resizeViewport(): void {
    const v = this.view();
    this.ambient.setPosition(...v.at(0, 0)).setSize(v.w, v.h).setDisplaySize(v.w, v.h);
    this.fog.setPosition(...v.at(v.w / 2, v.h / 2)).setDisplaySize(v.w * 1.25, v.h * 1.25);
    this.vignette.setPosition(...v.at(v.w / 2, v.h / 2)).setDisplaySize(v.w, v.h);
  }

  /** Vignette texture: transparent centre fading to the zone colour at the edges. */
  private vignetteTexture(color: number, alpha: number): string {
    const key = `lighting_vignette_${color.toString(16)}_${Math.round(alpha * 100)}`;
    if (this.scene.textures.exists(key)) return key;
    const w = 256, h = 144;
    const canvas = document.createElement('canvas');
    canvas.width = w;
    canvas.height = h;
    const ctx = canvas.getContext('2d', { willReadFrequently: true });
    if (!ctx) return LIGHT_TEXTURE;
    const r = (color >> 16) & 255, g = (color >> 8) & 255, b = color & 255;
    ctx.save();
    ctx.translate(w / 2, h / 2);
    ctx.scale(1, h / w);
    const grad = ctx.createRadialGradient(0, 0, w * 0.2, 0, 0, w * 0.62);
    grad.addColorStop(0, `rgba(${r},${g},${b},0)`);
    grad.addColorStop(0.55, `rgba(${r},${g},${b},${alpha * 0.35})`);
    grad.addColorStop(1, `rgba(${r},${g},${b},${alpha})`);
    ctx.fillStyle = grad;
    ctx.fillRect(-w, -w, w * 2, w * 2);
    ctx.restore();
    this.scene.textures.addCanvas(key, canvas);
    return key;
  }

  /** Apply the zone mood. Accepts a map id or theme; unknown ids use the active zone theme. */
  setZone(zoneId: string): void {
    const theme = ZONE_THEME_BY_ID[zoneId] ?? (zoneId in ZONE_MOODS ? zoneId as MapTheme : null);
    const mood: ZoneMood = theme ? ZONE_MOODS[theme] : getCurrentZoneMood();
    this.ambientAlpha = mood.ambientAlpha;
    this.ambient.setFillStyle(mood.ambient, 1).setAlpha(mood.ambientAlpha);
    const lum = (((mood.ambient >> 16) & 255) * 0.299 + ((mood.ambient >> 8) & 255) * 0.587 + (mood.ambient & 255) * 0.114) / 255;
    this.lightScale = Math.max(0.12, mood.ambientAlpha * (1 - lum) * 1.4);
    this.hazeAlpha = mood.hazeAlpha;
    this.fog.setTint(mood.haze).setAlpha(mood.hazeAlpha);
    this.vignette.setTexture(this.vignetteTexture(mood.vignette, mood.vignetteAlpha)).setVisible(true);
    this.resizeViewport();
  }

  /** Darken the scene beyond the zone mood (0..1 of the remaining light), e.g. a gloom curse. */
  deepen(amount: number): void {
    this.ambientAlpha = Math.min(0.92, this.ambientAlpha + (1 - this.ambientAlpha) * Math.max(0, Math.min(1, amount)));
    this.ambient.setAlpha(this.ambientAlpha);
    this.lightScale = Math.max(this.lightScale, this.ambientAlpha * 0.9);
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

    const v = this.view();
    const cam = v.cam;
    this.resizeViewport();
    this.ambient.setAlpha(Math.max(0, Math.min(1, this.ambientAlpha + Math.sin(this.time * 0.0015) * 0.015)));
    this.fog.setAlpha(Math.max(0, this.hazeAlpha * (0.8 + Math.sin(this.time * 0.0007) * 0.2)));
    this.fog.setPosition(...v.at(
      v.w / 2 + Math.sin(this.time * 0.0008) * v.w * 0.15,
      v.h / 2 + Math.cos(this.time * 0.00056) * v.h * 0.1,
    ));

    // Logical screen position of each light (camera centre = world scroll + origin).
    const originX = v.w * cam.originX;
    const originY = v.h * cam.originY;
    const visible = this.lights
      .map(light => {
        const x = (light.x - cam.scrollX - v.ox) * v.z + originX;
        const y = (light.y - cam.scrollY - v.oy) * v.z + originY;
        return { light, x, y, distance: (x - originX) ** 2 + (y - originY) ** 2 };
      })
      .filter(({ light, x, y }) => {
        const radius = light.radius * v.z;
        return Number.isFinite(radius) && radius > 0 && x + radius >= 0 && x - radius <= v.w
          && y + radius >= 0 && y - radius <= v.h;
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
        .setVisible(true).setPosition(...v.at(x, y))
        .setDisplaySize(light.radius * v.z * 2, light.radius * v.z * 2)
        // A light restores only the luminance removed by the ambient layer.
        // Mapping raw intensity directly to ADD alpha overexposes the scene.
        .setTint(light.color).setAlpha(Math.max(0, Math.min(1, intensity * this.lightScale)));
    });
    for (let i = visible.length; i < this.lightSprites.length; i++) this.lightSprites[i].setVisible(false);
  }

  destroy(): void {
    this.ambient.destroy();
    this.fog.destroy();
    this.vignette.destroy();
    this.lightSprites.forEach(sprite => sprite.destroy());
    this.lightSprites.length = 0;
    this.lights.length = 0;
    this.flickerSeeds.clear();
  }
}
