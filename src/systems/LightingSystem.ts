import Phaser from 'phaser';

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
  emerald_plains:    { color: 0x040610, alpha: 0.10, fogColor: 0x112211, fogAlpha: 0.03 },
  twilight_forest:   { color: 0x020408, alpha: 0.22, fogColor: 0x0a1010, fogAlpha: 0.05 },
  anvil_mountains:   { color: 0x080608, alpha: 0.18, fogColor: 0x100808, fogAlpha: 0.04 },
  scorching_desert:  { color: 0x0c0804, alpha: 0.08, fogColor: 0x120e04, fogAlpha: 0.02 },
  abyss_rift:        { color: 0x040004, alpha: 0.32, fogColor: 0x100010, fogAlpha: 0.06 },
};

/**
 * Fixed-viewport lighting overlay using setScrollFactor(0).
 *
 * The canvas is dynamically resized to match cam.width/2 × cam.height/2
 * each frame (Phaser's Scale manager may resize the game after construction).
 *
 * Position and scale counter the camera zoom transform so the overlay always
 * fills the entire viewport exactly.
 */
export class LightingSystem {
  private scene: Phaser.Scene;
  private canvas: HTMLCanvasElement;
  private ctx: CanvasRenderingContext2D;
  private overlay: Phaser.GameObjects.Image;
  private lights: LightSource[] = [];
  private ambientColor: number = 0x040610;
  private ambientAlpha: number = 0.35;
  private fogColor: number = 0x111111;
  private fogAlpha: number = 0.03;
  private time: number = 0;
  private flickerSeeds: Map<string, number> = new Map();
  private debugEl: HTMLDivElement | null = null;
  private debugVisible = false;
  private debugGfx!: Phaser.GameObjects.Graphics;

  private renderW = 0;
  private renderH = 0;
  private readonly texKey = 'lighting_overlay';

  constructor(scene: Phaser.Scene) {
    this.scene = scene;

    this.canvas = document.createElement('canvas');
    this.ctx = this.canvas.getContext('2d')!;

    // Initial sizing (will be corrected in first update if cam resized)
    const cam = scene.cameras.main;
    this.resizeCanvas(cam.width, cam.height);

    if (scene.textures.exists(this.texKey)) scene.textures.remove(this.texKey);
    scene.textures.addCanvas(this.texKey, this.canvas);

    this.overlay = scene.add.image(0, 0, this.texKey);
    this.overlay.setOrigin(0, 0);
    this.overlay.setScrollFactor(0);
    this.overlay.setDepth(999);
    this.overlay.setBlendMode(Phaser.BlendModes.MULTIPLY);

    // Debug graphics object (NORMAL blend) to visualize overlay bounds
    this.debugGfx = scene.add.graphics();
    this.debugGfx.setScrollFactor(0);
    this.debugGfx.setDepth(1001);
    this.debugGfx.setVisible(false);

    // Debug HUD toggled by backtick
    this.debugEl = document.createElement('div');
    Object.assign(this.debugEl.style, {
      position: 'fixed', top: '4px', left: '4px', zIndex: '9999',
      background: 'rgba(0,0,0,0.75)', color: '#0f0', fontSize: '11px',
      fontFamily: 'monospace', padding: '6px 8px', pointerEvents: 'none',
      whiteSpace: 'pre', display: 'none', lineHeight: '1.4',
    });
    document.body.appendChild(this.debugEl);
    window.addEventListener('keydown', (e) => {
      if (e.code === 'Backquote') {
        e.preventDefault();
        this.debugVisible = !this.debugVisible;
        if (this.debugEl) this.debugEl.style.display = this.debugVisible ? 'block' : 'none';
        this.debugGfx.setVisible(this.debugVisible);
      }
    });
  }

  private resizeCanvas(camW: number, camH: number): void {
    this.renderW = Math.ceil(camW / 2);
    this.renderH = Math.ceil(camH / 2);
    this.canvas.width = this.renderW;
    this.canvas.height = this.renderH;
  }

  setZone(zoneId: string): void {
    const ambient = ZONE_AMBIENTS[zoneId];
    if (ambient) {
      this.ambientColor = ambient.color;
      this.ambientAlpha = ambient.alpha;
      this.fogColor = ambient.fogColor ?? 0x111111;
      this.fogAlpha = ambient.fogAlpha ?? 0.03;
    }
  }

  addLight(light: LightSource): void {
    this.lights.push(light);
    if (light.id && light.flicker) {
      this.flickerSeeds.set(light.id, Math.random() * 1000);
    }
  }

  removeLight(id: string): void {
    this.lights = this.lights.filter(l => l.id !== id);
    this.flickerSeeds.delete(id);
  }

  clearLights(): void {
    this.lights = [];
    this.flickerSeeds.clear();
  }

  update(delta: number): void {
    this.time += delta;
    const cam = this.scene.cameras.main;
    const zoom = cam.zoom;

    // Resize canvas if the game was resized by Scale manager
    const needW = Math.ceil(cam.width / 2);
    const needH = Math.ceil(cam.height / 2);
    if (needW !== this.renderW || needH !== this.renderH) {
      this.resizeCanvas(cam.width, cam.height);
      // Re-register the canvas texture so Phaser picks up the new size
      if (this.scene.textures.exists(this.texKey)) this.scene.textures.remove(this.texKey);
      this.scene.textures.addCanvas(this.texKey, this.canvas);
      this.overlay.setTexture(this.texKey);
    }

    const ctx = this.ctx;
    const w = this.renderW;
    const h = this.renderH;

    // Position & scale to fill viewport despite camera zoom.
    // Camera zoom transforms scrollFactor(0) objects around originPx:
    //   screenPos = (objectPos - originPx) * zoom + originPx
    // We want screen TL at (0,0) and BR at (cam.width, cam.height).
    const originPxX = cam.width * cam.originX;
    const originPxY = cam.height * cam.originY;
    this.overlay.setPosition(
      originPxX * (zoom - 1) / zoom,
      originPxY * (zoom - 1) / zoom,
    );
    this.overlay.setScale(cam.width / (w * zoom), cam.height / (h * zoom));

    // --- Ambient fill ---
    const ar = (this.ambientColor >> 16) & 0xff;
    const ag = (this.ambientColor >> 8) & 0xff;
    const ab = this.ambientColor & 0xff;

    const breathe = Math.sin(this.time * 0.0015) * 0.015;
    const effectiveAlpha = Math.max(0, Math.min(1, this.ambientAlpha + breathe));

    const baseR = Math.round(255 - (255 - ar) * effectiveAlpha);
    const baseG = Math.round(255 - (255 - ag) * effectiveAlpha);
    const baseB = Math.round(255 - (255 - ab) * effectiveAlpha);

    ctx.fillStyle = `rgb(${baseR},${baseG},${baseB})`;
    ctx.fillRect(0, 0, w, h);

    // --- Atmospheric fog ---
    if (this.fogAlpha > 0) {
      const fr = (this.fogColor >> 16) & 0xff;
      const fg = (this.fogColor >> 8) & 0xff;
      const fb = this.fogColor & 0xff;
      const fogPhase = this.time * 0.0008;
      const fogX = Math.sin(fogPhase) * w * 0.15;
      const fogY = Math.cos(fogPhase * 0.7) * h * 0.1;
      const fogGrad = ctx.createRadialGradient(
        w / 2 + fogX, h / 2 + fogY, 0,
        w / 2 + fogX, h / 2 + fogY, w * 0.6,
      );
      fogGrad.addColorStop(0, `rgba(${fr},${fg},${fb},0)`);
      fogGrad.addColorStop(0.5, `rgba(${fr},${fg},${fb},${this.fogAlpha})`);
      fogGrad.addColorStop(1, `rgba(${fr},${fg},${fb},0)`);
      ctx.globalCompositeOperation = 'source-over';
      ctx.fillStyle = fogGrad;
      ctx.fillRect(0, 0, w, h);
    }

    // --- Light holes (additive) ---
    // World→screen: screen = (world - scrollX - originPx) * zoom + originPx
    // Screen→canvas: canvas = screen * (w / cam.width)
    ctx.globalCompositeOperation = 'lighter';
    const canvasPerScreenX = w / cam.width;
    const canvasPerScreenY = h / cam.height;

    for (const light of this.lights) {
      const screenLX = (light.x - cam.scrollX - originPxX) * zoom + originPxX;
      const screenLY = (light.y - cam.scrollY - originPxY) * zoom + originPxY;
      const cx = screenLX * canvasPerScreenX;
      const cy = screenLY * canvasPerScreenY;
      const radius = light.radius * zoom * canvasPerScreenX;

      if (cx + radius < 0 || cx - radius > w ||
          cy + radius < 0 || cy - radius > h) continue;

      let intensity = light.intensity;
      if (light.flicker) {
        const seed = this.flickerSeeds.get(light.id ?? '') ?? 0;
        const t = this.time;
        const f1 = Math.sin(t * 0.007 + seed) * 0.05;
        const f2 = Math.sin(t * 0.013 + seed * 2.3) * 0.03;
        const f3 = Math.sin(t * 0.031 + seed * 0.7) * 0.02;
        const pop = Math.sin(t * 0.0037 + seed * 1.7);
        const popIntensity = pop > 0.92 ? (pop - 0.92) * 1.5 : 0;
        intensity = Math.max(0, Math.min(1, intensity + f1 + f2 + f3 + popIntensity));
      }

      const lr = (light.color >> 16) & 0xff;
      const lg = (light.color >> 8) & 0xff;
      const lb = light.color & 0xff;

      const addR = Math.round((255 - baseR) * intensity * lr / 255);
      const addG = Math.round((255 - baseG) * intensity * lg / 255);
      const addB = Math.round((255 - baseB) * intensity * lb / 255);

      const grad = ctx.createRadialGradient(cx, cy, 0, cx, cy, radius);
      grad.addColorStop(0, `rgb(${addR},${addG},${addB})`);
      grad.addColorStop(0.15, `rgb(${addR * 0.9 | 0},${addG * 0.9 | 0},${addB * 0.9 | 0})`);
      grad.addColorStop(0.4, `rgb(${addR * 0.6 | 0},${addG * 0.6 | 0},${addB * 0.6 | 0})`);
      grad.addColorStop(0.7, `rgb(${addR * 0.25 | 0},${addG * 0.25 | 0},${addB * 0.25 | 0})`);
      grad.addColorStop(1, 'rgb(0,0,0)');

      ctx.fillStyle = grad;
      ctx.fillRect(cx - radius, cy - radius, radius * 2, radius * 2);
    }

    ctx.globalCompositeOperation = 'source-over';

    const tex = this.scene.textures.get(this.texKey);
    if (tex) {
      const src = tex.source[0];
      if (src) src.update();
    }

    // Debug: draw visible border using a separate Graphics object (NORMAL blend)
    if (this.debugVisible) {
      this.debugGfx.clear();
      const ox = this.overlay.x;
      const oy = this.overlay.y;
      const ow = w * this.overlay.scaleX;
      const oh = h * this.overlay.scaleY;
      // Red border around overlay bounds
      this.debugGfx.lineStyle(3, 0xff0000, 1);
      this.debugGfx.strokeRect(ox, oy, ow, oh);
      // Yellow crosshairs at center
      this.debugGfx.lineStyle(1, 0xffff00, 1);
      this.debugGfx.lineBetween(ox + ow / 2, oy, ox + ow / 2, oy + oh);
      this.debugGfx.lineBetween(ox, oy + oh / 2, ox + ow, oy + oh / 2);
      // Cyan border at actual viewport edges for comparison
      this.debugGfx.lineStyle(2, 0x00ffff, 1);
      this.debugGfx.strokeRect(0, 0, cam.width, cam.height);
      // Show each light's screen position as a small circle
      for (const light of this.lights) {
        const sx = (light.x - cam.scrollX - originPxX) * zoom + originPxX;
        const sy = (light.y - cam.scrollY - originPxY) * zoom + originPxY;
        this.debugGfx.lineStyle(1, 0xff00ff, 1);
        this.debugGfx.strokeCircle(sx, sy, 8);
      }
    }

    // Debug HUD
    if (this.debugVisible && this.debugEl) {
      const wv = cam.worldView;
      const ovX = this.overlay.x.toFixed(1);
      const ovY = this.overlay.y.toFixed(1);
      const ovSX = this.overlay.scaleX.toFixed(4);
      const ovSY = this.overlay.scaleY.toFixed(4);
      const displayW = (w * this.overlay.scaleX * zoom).toFixed(1);
      const displayH = (h * this.overlay.scaleY * zoom).toFixed(1);
      const screenTLx = ((this.overlay.x - originPxX) * zoom + originPxX).toFixed(1);
      const screenTLy = ((this.overlay.y - originPxY) * zoom + originPxY).toFixed(1);
      const screenBRx = (((this.overlay.x + w * this.overlay.scaleX) - originPxX) * zoom + originPxX).toFixed(1);
      const screenBRy = (((this.overlay.y + h * this.overlay.scaleY) - originPxY) * zoom + originPxY).toFixed(1);
      const gameCanvas = this.scene.game.canvas;
      this.debugEl.textContent =
        `[Lighting Debug - \` toggle]\n` +
        `game canvas : ${gameCanvas.width}x${gameCanvas.height} (CSS ${gameCanvas.style.width}x${gameCanvas.style.height})\n` +
        `cam         : ${cam.width}x${cam.height}  zoom=${zoom}  origin=(${cam.originX},${cam.originY})\n` +
        `cam.scroll  : (${cam.scrollX.toFixed(1)}, ${cam.scrollY.toFixed(1)})\n` +
        `worldView   : (${wv.x.toFixed(1)}, ${wv.y.toFixed(1)}, ${wv.width.toFixed(1)}, ${wv.height.toFixed(1)})\n` +
        `canvas res  : ${w}x${h}\n` +
        `overlay pos : (${ovX}, ${ovY})  scale=(${ovSX}, ${ovSY})\n` +
        `overlay scrn: TL=(${screenTLx}, ${screenTLy})  BR=(${screenBRx}, ${screenBRy})\n` +
        `display size: ${displayW}x${displayH} (should be ${cam.width}x${cam.height})\n` +
        `DPR         : ${window.devicePixelRatio}\n` +
        `lights      : ${this.lights.length}`;
    }
  }

  destroy(): void {
    this.overlay?.destroy();
    this.debugGfx?.destroy();
    this.lights = [];
    this.flickerSeeds.clear();
    if (this.debugEl) {
      this.debugEl.remove();
      this.debugEl = null;
    }
  }
}
