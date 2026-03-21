/** Shared drawing primitives for entity sprite generation. */
export class DrawUtils {

  // ── Noise ──

  hash2d(x: number, y: number): number {
    let n = (x | 0) * 374761393 + (y | 0) * 668265263;
    n = ((n >> 13) ^ n) * 1274126177;
    return ((n >> 16) ^ n & 0x7fffffff) / 0x7fffffff;
  }

  noise2d(x: number, y: number): number {
    const ix = Math.floor(x), iy = Math.floor(y);
    const fx = x - ix, fy = y - iy;
    const sx = fx * fx * (3 - 2 * fx);
    const sy = fy * fy * (3 - 2 * fy);
    const n00 = this.hash2d(ix, iy), n10 = this.hash2d(ix + 1, iy);
    const n01 = this.hash2d(ix, iy + 1), n11 = this.hash2d(ix + 1, iy + 1);
    return (n00 + (n10 - n00) * sx) + ((n01 + (n11 - n01) * sx) - (n00 + (n10 - n00) * sx)) * sy;
  }

  fbm(x: number, y: number, octaves: number): number {
    let v = 0, amp = 0.5, freq = 1;
    for (let i = 0; i < octaves; i++) {
      v += this.noise2d(x * freq, y * freq) * amp;
      amp *= 0.5; freq *= 2;
    }
    return v;
  }

  // ── Color ──

  clamp(v: number): number { return Math.max(0, Math.min(255, v | 0)); }

  rgb(c: number, alpha?: number): string {
    const r = (c >> 16) & 0xff, g = (c >> 8) & 0xff, b = c & 0xff;
    return alpha !== undefined ? `rgba(${r},${g},${b},${alpha})` : `rgb(${r},${g},${b})`;
  }

  hexRgb(c: number): [number, number, number] {
    return [(c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff];
  }

  darken(c: number, amt: number): number {
    const [r, g, b] = this.hexRgb(c);
    return ((Math.max(0, r - amt) << 16) | (Math.max(0, g - amt) << 8) | Math.max(0, b - amt));
  }

  lighten(c: number, amt: number): number {
    const [r, g, b] = this.hexRgb(c);
    return ((Math.min(255, r + amt) << 16) | (Math.min(255, g + amt) << 8) | Math.min(255, b + amt));
  }

  lerp(a: number, b: number, t: number): number { return a + (b - a) * t; }

  // ── Canvas Primitives ──

  createCanvas(w: number, h: number): [HTMLCanvasElement, CanvasRenderingContext2D] {
    const c = document.createElement('canvas');
    c.width = w; c.height = h;
    return [c, c.getContext('2d', { willReadFrequently: true })!];
  }

  roundRect(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, r: number): void {
    r = Math.min(r, w / 2, h / 2);
    ctx.beginPath();
    ctx.moveTo(x + r, y);
    ctx.lineTo(x + w - r, y);
    ctx.quadraticCurveTo(x + w, y, x + w, y + r);
    ctx.lineTo(x + w, y + h - r);
    ctx.quadraticCurveTo(x + w, y + h, x + w - r, y + h);
    ctx.lineTo(x + r, y + h);
    ctx.quadraticCurveTo(x, y + h, x, y + h - r);
    ctx.lineTo(x, y + r);
    ctx.quadraticCurveTo(x, y, x + r, y);
    ctx.closePath();
  }

  fillEllipse(ctx: CanvasRenderingContext2D, cx: number, cy: number, rx: number, ry: number): void {
    ctx.beginPath();
    ctx.ellipse(cx, cy, Math.max(0.5, rx), Math.max(0.5, ry), 0, 0, Math.PI * 2);
    ctx.fill();
  }

  fillCircle(ctx: CanvasRenderingContext2D, cx: number, cy: number, r: number): void {
    ctx.beginPath();
    ctx.arc(cx, cy, Math.max(0.5, r), 0, Math.PI * 2);
    ctx.fill();
  }

  drawPart(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, color: number, radius: number = 0): void {
    const [r, g, b] = this.hexRgb(color);
    const grad = ctx.createLinearGradient(x, y, x, y + h);
    grad.addColorStop(0, `rgb(${this.clamp(r + 15)},${this.clamp(g + 15)},${this.clamp(b + 15)})`);
    grad.addColorStop(1, `rgb(${this.clamp(r - 20)},${this.clamp(g - 20)},${this.clamp(b - 20)})`);
    ctx.fillStyle = grad;
    if (radius > 0) { this.roundRect(ctx, x, y, w, h, radius); ctx.fill(); }
    else { ctx.fillRect(x, y, w, h); }
  }

  applyNoiseToRegion(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, intensity: number): void {
    const imageData = ctx.getImageData(x, y, w, h);
    const d = imageData.data;
    for (let py = 0; py < h; py++) {
      for (let px = 0; px < w; px++) {
        const i = (py * w + px) * 4;
        if (d[i + 3] === 0) continue;
        const n = (this.fbm((x + px) * 0.025, (y + py) * 0.025, 4) - 0.5) * 2;
        const grain = (this.hash2d(px * 131 + py, py * 97 + px) - 0.5) * 0.08;
        const val = (n + grain) * intensity;
        d[i] = this.clamp(d[i] + val);
        d[i + 1] = this.clamp(d[i + 1] + val);
        d[i + 2] = this.clamp(d[i + 2] + val);
      }
    }
    ctx.putImageData(imageData, x, y);
  }

  // ── Gradient Fills ──

  /** Create and fill a linear or radial gradient over a rectangular region */
  gradientFill(ctx: CanvasRenderingContext2D, type: 'linear' | 'radial', colors: { stop: number; color: string }[], x: number, y: number, w: number, h: number): void {
    const grad = type === 'radial'
      ? ctx.createRadialGradient(x + w / 2, y + h / 2, 0, x + w / 2, y + h / 2, Math.max(w, h) / 2)
      : ctx.createLinearGradient(x, y, x, y + h);
    for (const { stop, color } of colors) grad.addColorStop(stop, color);
    ctx.fillStyle = grad;
    ctx.fillRect(x, y, w, h);
  }

  // ── Material Shaders ──

  /** Draw a gradient-filled shape with top-left highlight for metallic look */
  drawMetalSurface(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, baseColor: number): void {
    const [r, g, b] = this.hexRgb(baseColor);
    const grad = ctx.createLinearGradient(x, y, x + w * 0.3, y + h);
    grad.addColorStop(0, `rgb(${this.clamp(r + 40)},${this.clamp(g + 40)},${this.clamp(b + 40)})`);
    grad.addColorStop(0.4, `rgb(${r},${g},${b})`);
    grad.addColorStop(1, `rgb(${this.clamp(r - 30)},${this.clamp(g - 30)},${this.clamp(b - 30)})`);
    ctx.fillStyle = grad;
    ctx.fillRect(x, y, w, h);
  }

  /** Draw worn leather texture with subtle stitching hint */
  drawLeatherTexture(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, baseColor: number): void {
    this.drawPart(ctx, x, y, w, h, baseColor, 2);
    ctx.fillStyle = this.rgb(this.darken(baseColor, 20), 0.15);
    for (let i = 0; i < 5; i++) {
      const lx = x + this.hash2d(i * 3, 7) * w;
      const ly = y + this.hash2d(i * 5, 11) * h;
      ctx.fillRect(lx, ly, w * 0.3, 0.5);
    }
  }

  /** Draw cracked stone texture */
  drawStoneTexture(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, baseColor: number): void {
    this.drawPart(ctx, x, y, w, h, baseColor, 1);
    ctx.strokeStyle = this.rgb(this.darken(baseColor, 30), 0.4);
    ctx.lineWidth = 0.5;
    for (let i = 0; i < 3; i++) {
      const sx = x + this.hash2d(i * 17, 3) * w;
      const sy = y + this.hash2d(i * 23, 7) * h * 0.3;
      ctx.beginPath();
      ctx.moveTo(sx, sy);
      ctx.lineTo(sx + this.hash2d(i, 13) * w * 0.4, sy + this.hash2d(i, 19) * h * 0.6);
      ctx.stroke();
    }
  }

  /** Draw a layered flame shape */
  drawFlameLayer(ctx: CanvasRenderingContext2D, cx: number, baseY: number, w: number, h: number, color: string, flicker: number): void {
    const wobble = Math.sin(flicker) * w * 0.1;
    ctx.fillStyle = color;
    ctx.beginPath();
    ctx.moveTo(cx, baseY - h);
    ctx.quadraticCurveTo(cx + w * 0.5 + wobble, baseY - h * 0.6, cx + w * 0.4, baseY);
    ctx.lineTo(cx - w * 0.4, baseY);
    ctx.quadraticCurveTo(cx - w * 0.5 - wobble, baseY - h * 0.6, cx, baseY - h);
    ctx.fill();
  }

  /** Draw an anatomical bone segment between two points */
  drawBoneSegment(ctx: CanvasRenderingContext2D, x1: number, y1: number, x2: number, y2: number, width: number, color: number): void {
    ctx.strokeStyle = this.rgb(color);
    ctx.lineWidth = width;
    ctx.lineCap = 'round';
    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(x2, y2);
    ctx.stroke();
    ctx.fillStyle = this.rgb(this.lighten(color, 15));
    this.fillCircle(ctx, x1, y1, width * 0.6);
    this.fillCircle(ctx, x2, y2, width * 0.6);
  }

  /** Draw directional fur strokes over a region */
  drawFurTexture(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, baseColor: number, angle: number = 0): void {
    const count = Math.floor(w * h / 20);
    for (let i = 0; i < count; i++) {
      const fx = x + this.hash2d(i * 7, 31) * w;
      const fy = y + this.hash2d(i * 13, 47) * h;
      const len = 2 + this.hash2d(i, 71) * 3;
      const shade = this.hash2d(i, 91) > 0.5 ? this.lighten(baseColor, 10) : this.darken(baseColor, 10);
      ctx.strokeStyle = this.rgb(shade, 0.3);
      ctx.lineWidth = 0.5;
      ctx.beginPath();
      ctx.moveTo(fx, fy);
      ctx.lineTo(fx + Math.cos(angle) * len, fy + Math.sin(angle) * len);
      ctx.stroke();
    }
  }

  /** Draw a jointed limb through a series of points with tapering width */
  drawLimb(ctx: CanvasRenderingContext2D, joints: { x: number; y: number }[], baseWidth: number, color: number): void {
    if (joints.length < 2) return;
    for (let i = 0; i < joints.length - 1; i++) {
      const taper = 1 - (i / joints.length) * 0.3;
      const w = baseWidth * taper;
      ctx.strokeStyle = this.rgb(color);
      ctx.lineWidth = w;
      ctx.lineCap = 'round';
      ctx.lineJoin = 'round';
      ctx.beginPath();
      ctx.moveTo(joints[i].x, joints[i].y);
      ctx.lineTo(joints[i + 1].x, joints[i + 1].y);
      ctx.stroke();
    }
  }

  // ── Directional Lighting & Palette Helpers (D2 Visual Overhaul) ──

  /** Apply upper-left warm highlight + lower-right cold shadow (pixel-level, skips transparent) */
  applyDirectionalLighting(ctx: CanvasRenderingContext2D, w: number, h: number): void {
    const imgData = ctx.getImageData(0, 0, w, h);
    const d = imgData.data;
    for (let py = 0; py < h; py++) {
      for (let px = 0; px < w; px++) {
        const i = (py * w + px) * 4;
        if (d[i + 3] === 0) continue;
        const t = (px / w + py / h) / 2;
        const warm = Math.max(0, 1 - t * 2.5) * 4;
        const cool = Math.max(0, t * 2.5 - 1.5) * 3;
        d[i]     = this.clamp(d[i] + warm - cool * 0.5);
        d[i + 1] = this.clamp(d[i + 1] + warm * 0.4);
        d[i + 2] = this.clamp(d[i + 2] - warm * 0.6 + cool);
      }
    }
    ctx.putImageData(imgData, 0, 0);
  }

  /** Enhanced ground shadow beneath entity */
  drawGroundShadow(ctx: CanvasRenderingContext2D, cx: number, baseY: number, rx: number, ry: number): void {
    ctx.fillStyle = 'rgba(0,0,0,0.35)';
    this.fillEllipse(ctx, cx, baseY + ry * 0.3, rx * 1.2, ry);
  }

  /** 3-5 translucent brown dirt splotches for wear/grime */
  drawDirtSplatter(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, intensity: number): void {
    const count = 3 + Math.floor(intensity * 2);
    for (let i = 0; i < count; i++) {
      const sx = x + this.hash2d(i * 17, 251) * w;
      const sy = y + this.hash2d(i * 23, 257) * h;
      const sr = 1 + this.hash2d(i, 263) * 2 * intensity;
      const alpha = 0.05 + this.hash2d(i, 269) * 0.08 * intensity;
      ctx.fillStyle = `rgba(40,28,15,${alpha})`;
      this.fillEllipse(ctx, sx, sy, sr, sr * 0.7);
    }
  }

  /** Desaturate an RGB hex color by amount (0-1) */
  desaturate(color: number, amount: number): number {
    const [r, g, b] = this.hexRgb(color);
    const gray = Math.round(r * 0.299 + g * 0.587 + b * 0.114);
    const nr = Math.round(r + (gray - r) * amount);
    const ng = Math.round(g + (gray - g) * amount);
    const nb = Math.round(b + (gray - b) * amount);
    return (this.clamp(nr) << 16) | (this.clamp(ng) << 8) | this.clamp(nb);
  }

  /** Shift color toward warm (add red, reduce blue) */
  warmTint(color: number, amount: number): number {
    const [r, g, b] = this.hexRgb(color);
    return (this.clamp(r + amount * 15) << 16) | (this.clamp(g + amount * 5) << 8) | this.clamp(b - amount * 8);
  }

  /** Shift color toward cold (add blue, reduce red) */
  coldTint(color: number, amount: number): number {
    const [r, g, b] = this.hexRgb(color);
    return (this.clamp(r - amount * 8) << 16) | (this.clamp(g + amount * 2) << 8) | this.clamp(b + amount * 15);
  }

  // ── Painterly Effect Helpers ──

  /**
   * Enable soft (blurred) outline on subsequent draws.
   * Call before drawing a shape fill for an outer glow effect.
   */
  softOutline(
    ctx: CanvasRenderingContext2D,
    color: string,
    blur: number = 4,
    offsetX: number = 0,
    offsetY: number = 0,
  ): void {
    ctx.save();
    ctx.shadowColor = color;
    ctx.shadowBlur = blur;
    ctx.shadowOffsetX = offsetX;
    ctx.shadowOffsetY = offsetY;
  }

  /** End soft outline — restore context state */
  softOutlineEnd(ctx: CanvasRenderingContext2D): void {
    ctx.restore();
  }

  /**
   * Draw a rim light on the edge of a circular/elliptical shape.
   * Creates a subtle bright edge highlight opposite to the light direction.
   */
  rimLight(
    ctx: CanvasRenderingContext2D,
    cx: number, cy: number,
    rx: number, ry: number,
    color: string = 'rgba(255,255,255,0.15)',
    lightAngle: number = -0.7,
  ): void {
    const edgeX = cx + Math.cos(lightAngle + Math.PI) * rx * 0.8;
    const edgeY = cy + Math.sin(lightAngle + Math.PI) * ry * 0.8;
    const grad = ctx.createRadialGradient(edgeX, edgeY, 0, edgeX, edgeY, Math.max(rx, ry) * 0.6);
    grad.addColorStop(0, color);
    grad.addColorStop(1, 'rgba(255,255,255,0)');
    ctx.save();
    ctx.globalCompositeOperation = 'screen';
    ctx.fillStyle = grad;
    ctx.beginPath();
    ctx.ellipse(cx, cy, rx, ry, 0, 0, Math.PI * 2);
    ctx.fill();
    ctx.restore();
  }

  /**
   * Fill a rectangle with a multi-layer gradient for volumetric depth.
   * Applies base → shadow → highlight in 3 passes.
   */
  volumeGradient(
    ctx: CanvasRenderingContext2D,
    x: number, y: number, w: number, h: number,
    baseColor: string,
    shadowColor: string,
    highlightColor: string,
  ): void {
    ctx.fillStyle = baseColor;
    ctx.fillRect(x, y, w, h);

    const shadowGrad = ctx.createLinearGradient(x, y, x, y + h);
    shadowGrad.addColorStop(0, 'rgba(0,0,0,0)');
    shadowGrad.addColorStop(1, shadowColor);
    ctx.fillStyle = shadowGrad;
    ctx.fillRect(x, y, w, h);

    const hlGrad = ctx.createLinearGradient(x, y, x + w * 0.6, y + h * 0.6);
    hlGrad.addColorStop(0, highlightColor);
    hlGrad.addColorStop(1, 'rgba(255,255,255,0)');
    ctx.fillStyle = hlGrad;
    ctx.fillRect(x, y, w, h);
  }
}
