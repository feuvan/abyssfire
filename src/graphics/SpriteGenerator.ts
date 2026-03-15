import Phaser from 'phaser';
import { TEXTURE_SCALE } from '../config';
import { CAMP_THEMES } from '../data/camp-themes';

// ── Frame Layout Constants ──────────────────────────────────────────────────
const IDLE_START = 0, IDLE_COUNT = 4;
const WALK_START = 4, WALK_COUNT = 6;
const ATK_START = 10, ATK_COUNT = 4;
const HURT_START = 14, HURT_COUNT = 2;
const DEATH_START = 16, DEATH_COUNT = 4;
const CAST_START = 20, CAST_COUNT = 4;
const MONSTER_FRAMES = 20;
const PLAYER_FRAMES = 24;

// ── Types ───────────────────────────────────────────────────────────────────
interface SpriteConfig {
  textureKey: string;
  baseW: number;
  baseH: number;
  bodyColor: number;
  bodyDark: number;
  bodyLight: number;
  skinColor: number;
  accentColor: number;
  secondColor: number;
  headgear: string;
  weaponR: string;
  weaponL: string;
  headScale: number;
  bodyScale: number;
  legScale: number;
  bodyType: string;
  hasWings?: boolean;
  hasTail?: boolean;
  isFire?: boolean;
}

interface AnimOffsets {
  bodyDY: number;
  legLDY: number;
  legRDY: number;
  armLRot: number;
  armRRot: number;
  torsoRot: number;
  weaponRot: number;
}

// ── Configs ─────────────────────────────────────────────────────────────────
const PLAYER_CONFIGS: SpriteConfig[] = [
  {
    textureKey: 'player_warrior', baseW: 64, baseH: 96,
    bodyColor: 0x3a4a5c, bodyDark: 0x252f3c, bodyLight: 0x566a80,
    skinColor: 0xb08960, accentColor: 0x8b3a1a, secondColor: 0x2c1a10,
    headgear: 'helm', weaponR: 'sword', weaponL: 'shield',
    headScale: 1, bodyScale: 1.1, legScale: 1, bodyType: 'humanoid',
  },
  {
    textureKey: 'player_mage', baseW: 64, baseH: 96,
    bodyColor: 0x2a1a3a, bodyDark: 0x150a25, bodyLight: 0x3f2a55,
    skinColor: 0xc4a882, accentColor: 0x8a5ac0, secondColor: 0x1a1a2e,
    headgear: 'hat', weaponR: 'staff', weaponL: 'none',
    headScale: 1, bodyScale: 0.95, legScale: 1, bodyType: 'humanoid',
  },
  {
    textureKey: 'player_rogue', baseW: 64, baseH: 96,
    bodyColor: 0x1a2a1a, bodyDark: 0x0f1a0f, bodyLight: 0x2a3a2a,
    skinColor: 0xa08060, accentColor: 0x4a6a4a, secondColor: 0x1a0f0a,
    headgear: 'hood', weaponR: 'dagger', weaponL: 'dagger',
    headScale: 0.95, bodyScale: 0.9, legScale: 1, bodyType: 'humanoid',
  },
];

const MONSTER_CONFIGS: SpriteConfig[] = [
  {
    textureKey: 'monster_slime', baseW: 48, baseH: 40,
    bodyColor: 0x1a7a30, bodyDark: 0x0f5520, bodyLight: 0x2aaa45,
    skinColor: 0x1a7a30, accentColor: 0x1a5a20, secondColor: 0x0a3a10,
    headgear: 'none', weaponR: 'none', weaponL: 'none',
    headScale: 1, bodyScale: 1, legScale: 1, bodyType: 'blob',
  },
  {
    textureKey: 'monster_goblin', baseW: 48, baseH: 56,
    bodyColor: 0x3a5a1a, bodyDark: 0x2a4010, bodyLight: 0x4a7a2a,
    skinColor: 0x5a8a30, accentColor: 0x4a3020, secondColor: 0x3a2a15,
    headgear: 'ears', weaponR: 'club', weaponL: 'none',
    headScale: 1.2, bodyScale: 0.85, legScale: 0.8, bodyType: 'humanoid',
  },
  {
    textureKey: 'monster_goblin_chief', baseW: 60, baseH: 68,
    bodyColor: 0x3a5a1a, bodyDark: 0x2a4010, bodyLight: 0x4a7a2a,
    skinColor: 0x5a8a30, accentColor: 0x5a3a20, secondColor: 0x3a2a15,
    headgear: 'crown', weaponR: 'axe', weaponL: 'none',
    headScale: 1.15, bodyScale: 1.0, legScale: 0.85, bodyType: 'humanoid',
  },
  {
    textureKey: 'monster_skeleton', baseW: 44, baseH: 60,
    bodyColor: 0xb0a890, bodyDark: 0x908870, bodyLight: 0xd0c8b0,
    skinColor: 0xc8c0a8, accentColor: 0x5a7050, secondColor: 0x606050,
    headgear: 'none', weaponR: 'sword', weaponL: 'none',
    headScale: 1.05, bodyScale: 0.7, legScale: 0.85, bodyType: 'humanoid',
  },
  {
    textureKey: 'monster_zombie', baseW: 44, baseH: 60,
    bodyColor: 0x4a6a44, bodyDark: 0x3a5034, bodyLight: 0x5a8a54,
    skinColor: 0x6a8a5a, accentColor: 0x3a2a1a, secondColor: 0x2a1a0a,
    headgear: 'none', weaponR: 'none', weaponL: 'none',
    headScale: 1.0, bodyScale: 0.95, legScale: 0.95, bodyType: 'humanoid',
  },
  {
    textureKey: 'monster_werewolf', baseW: 52, baseH: 64,
    bodyColor: 0x4a3020, bodyDark: 0x3a2010, bodyLight: 0x5a4030,
    skinColor: 0x5a3a20, accentColor: 0x3a2010, secondColor: 0x2a1a0a,
    headgear: 'ears', weaponR: 'claws', weaponL: 'none',
    headScale: 1.1, bodyScale: 1.25, legScale: 1.1, bodyType: 'humanoid',
  },
  // Generic monsters - use simplified configs
  ...[
    ['monster_werewolf_alpha', 0x2a1810, 56, 68, 'ears', 'claws', 1.15, 1.3],
    ['monster_gargoyle', 0x3a4a5a, 52, 60, 'horns', 'claws', 1.0, 1.1],
    ['monster_stone_golem', 0x4a4a4a, 60, 68, 'none', 'none', 0.9, 1.4],
    ['monster_mountain_troll', 0x3a5a2a, 64, 72, 'none', 'club', 1.2, 1.3],
    ['monster_fire_elemental', 0x8a3000, 48, 60, 'none', 'none', 1.0, 1.0],
    ['monster_desert_scorpion', 0x6a4a2a, 52, 44, 'none', 'claws', 0.8, 1.1],
    ['monster_sandworm', 0x8a7040, 56, 48, 'none', 'none', 1.0, 1.2],
    ['monster_phoenix', 0x9a4a00, 56, 56, 'none', 'none', 1.0, 1.0],
    ['monster_imp', 0x7a1010, 40, 48, 'horns', 'claws', 1.1, 0.85],
    ['monster_lesser_demon', 0x5a0a3a, 52, 64, 'horns', 'claws', 1.0, 1.1],
    ['monster_succubus', 0x7a1040, 48, 64, 'horns', 'none', 1.0, 0.95],
    ['monster_demon_lord', 0x2a0a3a, 72, 84, 'horns', 'none', 1.1, 1.4],
  ].map(([key, color, w, h, hg, wp, hs, bs]) => {
    const c = color as number;
    return {
      textureKey: key as string, baseW: w as number, baseH: h as number,
      bodyColor: c,
      bodyDark: darkenHex(c, 30),
      bodyLight: lightenHex(c, 25),
      skinColor: lightenHex(c, 15),
      accentColor: darkenHex(c, 15),
      secondColor: darkenHex(c, 40),
      headgear: hg as string, weaponR: wp as string, weaponL: 'none',
      headScale: hs as number, bodyScale: bs as number, legScale: 1,
      bodyType: (key as string).includes('slime') || (key as string).includes('worm') ? 'blob' : 'humanoid',
      hasWings: (key as string).includes('gargoyle') || (key as string).includes('phoenix') || (key as string).includes('demon_lord'),
      hasTail: (key as string).includes('demon') || (key as string).includes('succubus'),
      isFire: (key as string).includes('fire') || (key as string).includes('phoenix'),
    } as SpriteConfig;
  }),
];

interface NPCConfig {
  key: string;
  bodyColor: number;
  hatColor: number;
  itemColor: number;
  skinColor?: number;
  hairColor?: number;
  hairStyle?: 'none' | 'short' | 'long' | 'bald' | 'hood';
  beard?: boolean;
  accessory?: 'hammer' | 'coinbag' | 'scroll' | 'staff' | 'pickaxe' | 'sword' | 'lantern' | 'book' | 'none';
  cloakColor?: number;
  bulky?: boolean;
}

const NPC_CONFIGS: NPCConfig[] = [
  // ── Blacksmiths ──
  { key: 'npc_blacksmith', bodyColor: 0x5a3a1a, hatColor: 0x4a2a3a, itemColor: 0x6a6a6a,
    skinColor: 0xc09070, hairColor: 0x2a1a0a, hairStyle: 'short', beard: true, accessory: 'hammer', bulky: true },
  { key: 'npc_blacksmith_advanced', bodyColor: 0x3a3a4a, hatColor: 0x6a3050, itemColor: 0x8a8a9a,
    skinColor: 0xa08060, hairColor: 0x1a1a2a, hairStyle: 'bald', beard: true, accessory: 'hammer', bulky: true },
  // ── Merchants ──
  { key: 'npc_merchant', bodyColor: 0x1a3a5a, hatColor: 0x1a5a4a, itemColor: 0xb8860b,
    skinColor: 0xc09870, hairColor: 0x3a2a1a, hairStyle: 'short', accessory: 'coinbag' },
  { key: 'npc_merchant_desert', bodyColor: 0x6a4a2a, hatColor: 0xc09040, itemColor: 0xd4a030,
    skinColor: 0xb08050, hairColor: 0x1a1a0a, hairStyle: 'hood', accessory: 'coinbag', cloakColor: 0x8a6a3a },
  // ── Stash ──
  { key: 'npc_stash', bodyColor: 0x3a1a4a, hatColor: 0x2a1a3a, itemColor: 0x8a5ac0,
    skinColor: 0xb09070, hairColor: 0x5a4a3a, hairStyle: 'short', accessory: 'book' },
  // ── Quest givers (each unique) ──
  { key: 'npc_quest_elder', bodyColor: 0x5a5a2a, hatColor: 0x6a5a1a, itemColor: 0xb8860b,
    skinColor: 0xc09870, hairColor: 0x9a9090, hairStyle: 'long', beard: true, accessory: 'staff' },
  { key: 'npc_quest_scout', bodyColor: 0x2a4a2a, hatColor: 0x3a5a2a, itemColor: 0x4a6a3a,
    skinColor: 0xb09060, hairColor: 0x3a2a0a, hairStyle: 'short', accessory: 'sword', cloakColor: 0x2a3a1a },
  { key: 'npc_forest_hermit', bodyColor: 0x3a4a3a, hatColor: 0x2a3a2a, itemColor: 0x5a8a4a,
    skinColor: 0xa08a60, hairColor: 0x7a7a6a, hairStyle: 'long', beard: true, accessory: 'staff', cloakColor: 0x3a4a2a },
  { key: 'npc_quest_dwarf', bodyColor: 0x5a4a3a, hatColor: 0x7a5a3a, itemColor: 0x8a7a5a,
    skinColor: 0xc0a080, hairColor: 0x8a4a1a, hairStyle: 'short', beard: true, accessory: 'pickaxe', bulky: true },
  { key: 'npc_quest_nomad', bodyColor: 0x7a5a30, hatColor: 0xb08a40, itemColor: 0xc09a30,
    skinColor: 0xb08050, hairColor: 0x1a1a0a, hairStyle: 'hood', accessory: 'lantern', cloakColor: 0x8a6a2a },
  { key: 'npc_quest_warden', bodyColor: 0x2a1a2a, hatColor: 0x4a1a1a, itemColor: 0x6a2a3a,
    skinColor: 0x907060, hairColor: 0x1a1a1a, hairStyle: 'short', accessory: 'sword', cloakColor: 0x1a0a1a },
];

// ── Color Utilities (module-level) ──────────────────────────────────────────
function hexRgb(c: number): [number, number, number] {
  return [(c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff];
}

function darkenHex(c: number, amt: number): number {
  const [r, g, b] = hexRgb(c);
  return ((Math.max(0, r - amt) << 16) | (Math.max(0, g - amt) << 8) | Math.max(0, b - amt));
}

function lightenHex(c: number, amt: number): number {
  const [r, g, b] = hexRgb(c);
  return ((Math.min(255, r + amt) << 16) | (Math.min(255, g + amt) << 8) | Math.min(255, b + amt));
}

// ═══════════════════════════════════════════════════════════════════════════
// ██ SpriteGenerator ██
// ═══════════════════════════════════════════════════════════════════════════

export class SpriteGenerator {
  private scene: Phaser.Scene;

  // Terrain base colors for edge blending (indexed by tile type)
  static readonly TERRAIN_COLORS = [
    '#1f3d18', // 0 = grass
    '#35251a', // 1 = dirt
    '#2e3338', // 2 = stone
    '#0b1820', // 3 = water
    '#1a1a1e', // 4 = wall
    '#2c2010', // 5 = camp
    '#4a3520', // 6 = camp_wall
  ];

  private static readonly TILE_NAMES = ['grass', 'dirt', 'stone', 'water', 'wall', 'camp', 'camp_wall'];

  /**
   * Generate a blended tile that smoothly transitions to neighboring terrain types.
   * Called lazily by ZoneScene when a tile borders a different terrain type.
   * Results are cached as Phaser textures.
   *
   * neighbors order: [TR, TL, BR, BL] — the 4 edge-sharing neighbors in iso space.
   */
  static generateBlendedTile(
    scene: Phaser.Scene,
    baseTileType: number,
    neighbors: [number, number, number, number],
  ): string {
    const key = `tile_b_${baseTileType}_${neighbors.join('')}`;
    if (scene.textures.exists(key)) return key;

    const s = TEXTURE_SCALE;
    const w = 64 * s, h = 32 * s;
    const canvas = document.createElement('canvas');
    canvas.width = w; canvas.height = h;
    const ctx = canvas.getContext('2d')!;

    // Draw the base tile onto our canvas
    const baseTexKey = `tile_${SpriteGenerator.TILE_NAMES[baseTileType]}`;
    if (scene.textures.exists(baseTexKey)) {
      ctx.drawImage(scene.textures.get(baseTexKey).getSourceImage() as CanvasImageSource, 0, 0);
    }

    // Diamond clip so blends stay within tile boundary
    const cx = w / 2, cy = h / 2;
    ctx.save();
    ctx.beginPath();
    ctx.moveTo(cx, 0); ctx.lineTo(w, cy); ctx.lineTo(cx, h); ctx.lineTo(0, cy);
    ctx.closePath();
    ctx.clip();

    // Edge midpoints (gradient origins) for each diamond edge
    const edgeMids: [number, number][] = [
      [w * 0.75, h * 0.25], // TR edge midpoint
      [w * 0.25, h * 0.25], // TL edge midpoint
      [w * 0.75, h * 0.75], // BR edge midpoint
      [w * 0.25, h * 0.75], // BL edge midpoint
    ];

    for (let i = 0; i < 4; i++) {
      const nType = neighbors[i];
      if (nType === baseTileType) continue;
      if (nType < 0 || nType > 5) continue;
      // Skip blending into/from walls (they have 3D height)
      if (nType === 4 || baseTileType === 4) continue;

      const nColor = SpriteGenerator.TERRAIN_COLORS[nType];
      const [mx, my] = edgeMids[i];

      // Gradient from edge midpoint toward center — fades neighbor color in
      const grad = ctx.createLinearGradient(mx, my, cx, cy);
      grad.addColorStop(0, nColor + 'aa');   // ~67% alpha at edge
      grad.addColorStop(0.35, nColor + '55'); // ~33% at 35%
      grad.addColorStop(0.6, nColor + '00');  // transparent at 60%
      ctx.fillStyle = grad;
      ctx.fillRect(0, 0, w, h);
    }

    ctx.restore();
    scene.textures.addCanvas(key, canvas);
    return key;
  }

  constructor(scene: Phaser.Scene) {
    this.scene = scene;
  }

  generateAll(): void {
    this.generateTiles();
    this.generatePlayerSheets();
    this.generateMonsterSheets();
    this.generateNPCSprites();
    this.generateDecorations();
    this.generateCampDecorations();
    this.generateEffects();
    this.registerAnimations();
  }

  // ── Noise & Drawing Utilities ───────────────────────────────────────────

  private hash2d(x: number, y: number): number {
    let n = (x | 0) * 374761393 + (y | 0) * 668265263;
    n = ((n >> 13) ^ n) * 1274126177;
    return ((n >> 16) ^ n & 0x7fffffff) / 0x7fffffff;
  }

  private noise2d(x: number, y: number): number {
    const ix = Math.floor(x), iy = Math.floor(y);
    const fx = x - ix, fy = y - iy;
    const sx = fx * fx * (3 - 2 * fx);
    const sy = fy * fy * (3 - 2 * fy);
    const n00 = this.hash2d(ix, iy), n10 = this.hash2d(ix + 1, iy);
    const n01 = this.hash2d(ix, iy + 1), n11 = this.hash2d(ix + 1, iy + 1);
    return (n00 + (n10 - n00) * sx) + ((n01 + (n11 - n01) * sx) - (n00 + (n10 - n00) * sx)) * sy;
  }

  private fbm(x: number, y: number, octaves: number): number {
    let v = 0, amp = 0.5, freq = 1;
    for (let i = 0; i < octaves; i++) {
      v += this.noise2d(x * freq, y * freq) * amp;
      amp *= 0.5;
      freq *= 2;
    }
    return v;
  }

  private clamp(v: number): number { return Math.max(0, Math.min(255, v | 0)); }

  private rgb(c: number, alpha?: number): string {
    const [r, g, b] = hexRgb(c);
    return alpha !== undefined ? `rgba(${r},${g},${b},${alpha})` : `rgb(${r},${g},${b})`;
  }

  private lerp(a: number, b: number, t: number): number { return a + (b - a) * t; }

  private createCanvas(w: number, h: number): [HTMLCanvasElement, CanvasRenderingContext2D] {
    const c = document.createElement('canvas');
    c.width = w; c.height = h;
    return [c, c.getContext('2d')!];
  }

  private roundRect(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, r: number): void {
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

  private fillEllipse(ctx: CanvasRenderingContext2D, cx: number, cy: number, rx: number, ry: number): void {
    ctx.beginPath();
    ctx.ellipse(cx, cy, Math.max(0.5, rx), Math.max(0.5, ry), 0, 0, Math.PI * 2);
    ctx.fill();
  }

  private fillCircle(ctx: CanvasRenderingContext2D, cx: number, cy: number, r: number): void {
    ctx.beginPath();
    ctx.arc(cx, cy, Math.max(0.5, r), 0, Math.PI * 2);
    ctx.fill();
  }

  private clipDiamond(ctx: CanvasRenderingContext2D, w: number, h: number): void {
    ctx.beginPath();
    ctx.moveTo(w / 2, 0);
    ctx.lineTo(w, h / 2);
    ctx.lineTo(w / 2, h);
    ctx.lineTo(0, h / 2);
    ctx.closePath();
    ctx.clip();
  }

  private drawPart(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, color: number, radius: number = 0): void {
    const [r, g, b] = hexRgb(color);
    const grad = ctx.createLinearGradient(x, y, x, y + h);
    grad.addColorStop(0, `rgb(${this.clamp(r + 15)},${this.clamp(g + 15)},${this.clamp(b + 15)})`);
    grad.addColorStop(1, `rgb(${this.clamp(r - 20)},${this.clamp(g - 20)},${this.clamp(b - 20)})`);
    ctx.fillStyle = grad;
    if (radius > 0) {
      this.roundRect(ctx, x, y, w, h, radius);
      ctx.fill();
    } else {
      ctx.fillRect(x, y, w, h);
    }
  }

  private applyNoiseToRegion(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, intensity: number): void {
    const imageData = ctx.getImageData(x, y, w, h);
    const d = imageData.data;
    for (let py = 0; py < h; py++) {
      for (let px = 0; px < w; px++) {
        const i = (py * w + px) * 4;
        if (d[i + 3] === 0) continue;
        // Low-frequency smooth noise for subtle color variation
        const n = (this.fbm((x + px) * 0.025, (y + py) * 0.025, 4) - 0.5) * 2;
        // Very faint grain — barely perceptible
        const grain = (this.hash2d(px * 131 + py, py * 97 + px) - 0.5) * 0.08;
        const val = (n + grain) * intensity;
        d[i] = this.clamp(d[i] + val);
        d[i + 1] = this.clamp(d[i + 1] + val);
        d[i + 2] = this.clamp(d[i + 2] + val);
      }
    }
    ctx.putImageData(imageData, x, y);
  }

  // ═══════════════════════════════════════════════════════════════════════
  // ██ TILE GENERATION ██
  // ═══════════════════════════════════════════════════════════════════════

  private generateTiles(): void {
    this.makeTile('tile_grass', this.drawGrass.bind(this));
    this.makeTile('tile_dirt', this.drawDirt.bind(this));
    this.makeTile('tile_stone', this.drawStone.bind(this));
    this.makeTile('tile_water', this.drawWater.bind(this));
    this.makeTile('tile_wall', this.drawWall.bind(this));
    this.makeTile('tile_camp', this.drawCamp.bind(this));

    // Default camp wall (plains theme)
    const plainsTheme = CAMP_THEMES['plains'];
    this.makeTile('tile_camp_wall', (ctx, w, h) =>
      this.drawCampWall(ctx, w, h, plainsTheme.wallColor, plainsTheme.wallDark, plainsTheme.wallLight, plainsTheme.wallTop),
    );

    // Themed variants for each camp theme
    for (const [themeName, theme] of Object.entries(CAMP_THEMES)) {
      const tWall = theme.wallColor;
      const tWallDark = theme.wallDark;
      const tWallLight = theme.wallLight;
      const tWallTop = theme.wallTop;
      const tGround = theme.groundColor;

      this.makeTile(`tile_camp_wall_${themeName}`, (ctx, w, h) =>
        this.drawCampWall(ctx, w, h, tWall, tWallDark, tWallLight, tWallTop),
      );
      this.makeTile(`tile_camp_ground_${themeName}`, (ctx, w, h) =>
        this.drawCampGroundThemed(ctx, w, h, tGround),
      );
    }
  }

  private makeTile(key: string, drawFn: (ctx: CanvasRenderingContext2D, w: number, h: number) => void): void {
    const s = TEXTURE_SCALE;
    const w = 64 * s, h = 32 * s;
    const [canvas, ctx] = this.createCanvas(w, h);
    ctx.save();
    this.clipDiamond(ctx, w, h);
    drawFn(ctx, w, h);
    ctx.restore();

    if (this.scene.textures.exists(key)) this.scene.textures.remove(key);
    this.scene.textures.addCanvas(key, canvas);
  }

  private drawGrass(ctx: CanvasRenderingContext2D, w: number, h: number): void {
    // Uniform dark forest green base
    ctx.fillStyle = '#1f3d18';
    ctx.fillRect(0, 0, w, h);

    // Very subtle color variation
    this.applyNoiseToRegion(ctx, 0, 0, w, h, 6);

    // Faint grass blade hints
    for (let i = 0; i < 20; i++) {
      const gx = this.hash2d(i * 7, 31) * w;
      const gy = this.hash2d(i * 13, 47) * h;
      const green = 55 + this.hash2d(i, 91) * 40;
      ctx.strokeStyle = `rgba(25,${green | 0},18,0.1)`;
      ctx.lineWidth = 0.6;
      const lean = (this.hash2d(i, 53) - 0.5) * 2;
      ctx.beginPath();
      ctx.moveTo(gx, gy);
      ctx.lineTo(gx + lean, gy - 2 - this.hash2d(i, 71) * 3);
      ctx.stroke();
    }
  }

  private drawDirt(ctx: CanvasRenderingContext2D, w: number, h: number): void {
    // Uniform dark earth base
    ctx.fillStyle = '#35251a';
    ctx.fillRect(0, 0, w, h);

    this.applyNoiseToRegion(ctx, 0, 0, w, h, 7);

    // Faint pebble hints
    for (let i = 0; i < 5; i++) {
      const px = this.hash2d(i * 11, 23) * w;
      const py = this.hash2d(i * 17, 29) * h;
      const r = this.hash2d(i, 59);
      ctx.fillStyle = `rgba(${70 + r * 25 | 0},${60 + r * 20 | 0},${45 + r * 15 | 0},0.15)`;
      this.fillEllipse(ctx, px, py, 1.5 + r * 2, 1 + r * 1.5);
    }

    // Very faint cracks
    ctx.strokeStyle = 'rgba(25,18,10,0.12)';
    ctx.lineWidth = 0.5;
    for (let i = 0; i < 2; i++) {
      const x1 = this.hash2d(i * 37, 41) * w;
      const y1 = this.hash2d(i * 43, 53) * h;
      const x2 = this.hash2d(i * 61, 67) * w;
      const y2 = this.hash2d(i * 71, 79) * h;
      ctx.beginPath(); ctx.moveTo(x1, y1); ctx.lineTo(x2, y2); ctx.stroke();
    }
  }

  private drawStone(ctx: CanvasRenderingContext2D, w: number, h: number): void {
    // Uniform dark grey base
    ctx.fillStyle = '#2e3338';
    ctx.fillRect(0, 0, w, h);

    this.applyNoiseToRegion(ctx, 0, 0, w, h, 5);

    // Very faint mortar hints
    ctx.strokeStyle = 'rgba(18,20,22,0.15)';
    ctx.lineWidth = 0.5;
    const cx = w / 2, cy = h / 2;
    ctx.beginPath(); ctx.moveTo(cx - w * 0.35, cy); ctx.lineTo(cx + w * 0.35, cy); ctx.stroke();

    // Faint moss patches
    for (let i = 0; i < 2; i++) {
      const mx = this.hash2d(i * 19, 83) * w;
      const my = this.hash2d(i * 23, 89) * h;
      ctx.fillStyle = `rgba(28,45,22,0.08)`;
      const rx = 3 + this.hash2d(i, 103) * 4;
      const ry = 2 + this.hash2d(i, 107) * 2;
      this.fillEllipse(ctx, mx, my, rx, ry);
    }
  }

  private drawWater(ctx: CanvasRenderingContext2D, w: number, h: number): void {
    // Uniform dark water base
    ctx.fillStyle = '#0b1820';
    ctx.fillRect(0, 0, w, h);

    this.applyNoiseToRegion(ctx, 0, 0, w, h, 4);

    // Faint ripple highlights
    ctx.strokeStyle = 'rgba(45,90,130,0.1)';
    ctx.lineWidth = 0.6;
    const cx = w / 2, cy = h / 2;
    ctx.beginPath(); ctx.arc(cx - w * 0.1, cy - h * 0.06, w * 0.1, 0.3, 2.6, false); ctx.stroke();
    ctx.beginPath(); ctx.arc(cx + w * 0.08, cy + h * 0.06, w * 0.07, 0.5, 2.5, false); ctx.stroke();

    // Very faint shimmer
    ctx.fillStyle = 'rgba(70,130,170,0.06)';
    this.fillEllipse(ctx, cx + w * 0.04, cy - h * 0.1, w * 0.05, h * 0.04);
  }

  private drawWall(ctx: CanvasRenderingContext2D, w: number, h: number): void {
    // The wall is NOT diamond-clipped — it has 3D height.
    // We re-draw over the clipped area with wall faces.
    ctx.fillStyle = '#1a1a1e';
    ctx.fillRect(0, 0, w, h);

    const wallH = h * 0.45;
    // Front face (dark)
    const fGrad = ctx.createLinearGradient(0, h / 2, 0, h);
    fGrad.addColorStop(0, '#2a2a30'); fGrad.addColorStop(1, '#181820');
    ctx.fillStyle = fGrad;
    ctx.beginPath();
    ctx.moveTo(0, h / 2); ctx.lineTo(w / 2, h);
    ctx.lineTo(w / 2, h - wallH); ctx.lineTo(0, h / 2 - wallH);
    ctx.closePath(); ctx.fill();

    // Right face (lighter)
    const rGrad = ctx.createLinearGradient(w / 2, h / 2, w, h / 2);
    rGrad.addColorStop(0, '#323238'); rGrad.addColorStop(1, '#28282e');
    ctx.fillStyle = rGrad;
    ctx.beginPath();
    ctx.moveTo(w / 2, h); ctx.lineTo(w, h / 2);
    ctx.lineTo(w, h / 2 - wallH); ctx.lineTo(w / 2, h - wallH);
    ctx.closePath(); ctx.fill();

    // Top face
    ctx.fillStyle = '#3a3a42';
    ctx.beginPath();
    ctx.moveTo(w / 2, h / 2 - wallH); ctx.lineTo(w, h / 2 - wallH);
    ctx.lineTo(w / 2, h - wallH); ctx.lineTo(0, h / 2 - wallH);
    ctx.closePath(); ctx.fill();

    // Brick lines
    ctx.strokeStyle = 'rgba(10,10,12,0.5)';
    ctx.lineWidth = 0.8;
    for (let i = 1; i < 3; i++) {
      const ly = h / 2 + (h / 2 - wallH) * 0 + i * (wallH / 3);
      ctx.beginPath();
      ctx.moveTo(0 + i * 2, h / 2 - wallH + ly * 0.3);
      ctx.lineTo(w / 2 - i * 2, h - wallH + ly * 0.3);
      ctx.stroke();
    }

    // Edge highlight
    ctx.strokeStyle = 'rgba(80,80,90,0.3)';
    ctx.lineWidth = 0.5;
    ctx.beginPath();
    ctx.moveTo(0, h / 2 - wallH); ctx.lineTo(w / 2, h / 2 - wallH);
    ctx.lineTo(w, h / 2 - wallH);
    ctx.stroke();

    this.applyNoiseToRegion(ctx, 0, 0, w, h, 5);
  }

  private drawCamp(ctx: CanvasRenderingContext2D, w: number, h: number): void {
    // Uniform warm wood base
    ctx.fillStyle = '#2c2010';
    ctx.fillRect(0, 0, w, h);

    this.applyNoiseToRegion(ctx, 0, 0, w, h, 6);

    // Plank lines
    const cx = w / 2, cy = h / 2;
    ctx.strokeStyle = 'rgba(35,22,10,0.3)';
    ctx.lineWidth = 0.6;
    for (let i = -4; i <= 4; i++) {
      const ly = cy + i * h * 0.1;
      const inset = Math.abs(i) * w * 0.06;
      ctx.beginPath(); ctx.moveTo(cx - w * 0.4 + inset, ly); ctx.lineTo(cx + w * 0.4 - inset, ly); ctx.stroke();
    }

    // Subtle warm glow (very faint, no hard edges)
    const glow = ctx.createRadialGradient(cx, cy, 0, cx, cy, w * 0.4);
    glow.addColorStop(0, 'rgba(160,90,20,0.1)');
    glow.addColorStop(0.6, 'rgba(100,45,10,0.05)');
    glow.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = glow;
    ctx.fillRect(0, 0, w, h);
  }

  private drawCampWall(
    ctx: CanvasRenderingContext2D, w: number, h: number,
    wallColor: string, wallDark: string, wallLight: string, wallTop: string,
  ): void {
    ctx.fillStyle = wallDark;
    ctx.fillRect(0, 0, w, h);

    const wallH = h * 0.45;

    // Front face (left-side isometric face) with gradient
    const fGrad = ctx.createLinearGradient(0, h / 2, w / 2, h);
    fGrad.addColorStop(0, wallLight);
    fGrad.addColorStop(1, wallDark);
    ctx.fillStyle = fGrad;
    ctx.beginPath();
    ctx.moveTo(0, h / 2); ctx.lineTo(w / 2, h);
    ctx.lineTo(w / 2, h - wallH); ctx.lineTo(0, h / 2 - wallH);
    ctx.closePath(); ctx.fill();

    // Right face with gradient
    const rGrad = ctx.createLinearGradient(w / 2, h / 2, w, h / 2);
    rGrad.addColorStop(0, wallLight);
    rGrad.addColorStop(1, wallColor);
    ctx.fillStyle = rGrad;
    ctx.beginPath();
    ctx.moveTo(w / 2, h); ctx.lineTo(w, h / 2);
    ctx.lineTo(w, h / 2 - wallH); ctx.lineTo(w / 2, h - wallH);
    ctx.closePath(); ctx.fill();

    // Top face
    ctx.fillStyle = wallTop;
    ctx.beginPath();
    ctx.moveTo(w / 2, h / 2 - wallH); ctx.lineTo(w, h / 2 - wallH);
    ctx.lineTo(w / 2, h - wallH); ctx.lineTo(0, h / 2 - wallH);
    ctx.closePath(); ctx.fill();

    // Vertical plank/line details
    ctx.strokeStyle = 'rgba(0,0,0,0.25)';
    ctx.lineWidth = 0.7;
    for (let i = 1; i < 3; i++) {
      const ly = h / 2 + i * (wallH / 3);
      ctx.beginPath();
      ctx.moveTo(0 + i * 2, h / 2 - wallH + ly * 0.3);
      ctx.lineTo(w / 2 - i * 2, h - wallH + ly * 0.3);
      ctx.stroke();
    }

    // Edge highlight along top ridge
    ctx.strokeStyle = 'rgba(255,255,255,0.18)';
    ctx.lineWidth = 0.5;
    ctx.beginPath();
    ctx.moveTo(0, h / 2 - wallH); ctx.lineTo(w / 2, h / 2 - wallH);
    ctx.lineTo(w, h / 2 - wallH);
    ctx.stroke();

    this.applyNoiseToRegion(ctx, 0, 0, w, h, 5);
  }

  private drawCampGroundThemed(
    ctx: CanvasRenderingContext2D, w: number, h: number,
    groundColor: string,
  ): void {
    ctx.fillStyle = groundColor;
    ctx.fillRect(0, 0, w, h);

    this.applyNoiseToRegion(ctx, 0, 0, w, h, 6);

    // Plank lines
    const cx = w / 2, cy = h / 2;
    ctx.strokeStyle = 'rgba(0,0,0,0.2)';
    ctx.lineWidth = 0.6;
    for (let i = -4; i <= 4; i++) {
      const ly = cy + i * h * 0.1;
      const inset = Math.abs(i) * w * 0.06;
      ctx.beginPath(); ctx.moveTo(cx - w * 0.4 + inset, ly); ctx.lineTo(cx + w * 0.4 - inset, ly); ctx.stroke();
    }

    // Subtle warm glow
    const glow = ctx.createRadialGradient(cx, cy, 0, cx, cy, w * 0.4);
    glow.addColorStop(0, 'rgba(160,90,20,0.08)');
    glow.addColorStop(0.6, 'rgba(100,45,10,0.04)');
    glow.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = glow;
    ctx.fillRect(0, 0, w, h);
  }

  // ═══════════════════════════════════════════════════════════════════════
  // ██ CHARACTER SPRITE SHEETS ██
  // ═══════════════════════════════════════════════════════════════════════

  private generatePlayerSheets(): void {
    for (const cfg of PLAYER_CONFIGS) {
      this.makeCharSheet(cfg, PLAYER_FRAMES);
    }
  }

  private generateMonsterSheets(): void {
    for (const cfg of MONSTER_CONFIGS) {
      this.makeCharSheet(cfg, MONSTER_FRAMES);
    }
  }

  private makeCharSheet(cfg: SpriteConfig, totalFrames: number): void {
    const s = TEXTURE_SCALE;
    const fw = cfg.baseW * s, fh = cfg.baseH * s;
    const [canvas, ctx] = this.createCanvas(fw * totalFrames, fh);

    const actions: [string, number, number][] = [
      ['idle', IDLE_START, IDLE_COUNT],
      ['walk', WALK_START, WALK_COUNT],
      ['attack', ATK_START, ATK_COUNT],
      ['hurt', HURT_START, HURT_COUNT],
      ['death', DEATH_START, DEATH_COUNT],
    ];
    if (totalFrames > MONSTER_FRAMES) {
      actions.push(['cast', CAST_START, CAST_COUNT]);
    }

    for (const [action, start, count] of actions) {
      for (let f = 0; f < count; f++) {
        const ox = (start + f) * fw;
        const offsets = this.getAnimOffsets(action, f, count);
        if (cfg.bodyType === 'blob') {
          this.drawBlobFrame(ctx, ox, 0, fw, fh, cfg, offsets, action, f, count);
        } else {
          this.drawHumanoidFrame(ctx, ox, 0, fw, fh, cfg, offsets, action);
        }
      }
    }

    // Very light noise for texture depth
    this.applyNoiseToRegion(ctx, 0, 0, canvas.width, canvas.height, 4);

    const key = cfg.textureKey;
    if (this.scene.textures.exists(key)) this.scene.textures.remove(key);
    const canvasTex = this.scene.textures.addCanvas(key, canvas)!;
    const frameTotal = Math.floor(canvas.width / fw);
    for (let i = 0; i < frameTotal; i++) {
      canvasTex.add(i, 0, i * fw, 0, fw, fh);
    }
  }

  // ── Animation Offsets ────────────────────────────────────────────────

  private getAnimOffsets(action: string, frame: number, total: number): AnimOffsets {
    const t = total > 1 ? frame / (total - 1) : 0;
    const phase = (frame / total) * Math.PI * 2;

    switch (action) {
      case 'idle':
        return {
          bodyDY: Math.sin(phase) * 1.5,
          legLDY: 0, legRDY: 0,
          armLRot: Math.sin(phase) * 4,
          armRRot: -Math.sin(phase) * 4,
          torsoRot: 0,
          weaponRot: Math.sin(phase) * 3,
        };
      case 'walk':
        return {
          bodyDY: -Math.abs(Math.sin(phase)) * 2.5,
          legLDY: Math.sin(phase) * 5,
          legRDY: -Math.sin(phase) * 5,
          armLRot: -Math.sin(phase) * 20,
          armRRot: Math.sin(phase) * 20,
          torsoRot: Math.sin(phase) * 2,
          weaponRot: Math.sin(phase) * 8,
        };
      case 'attack': {
        let wRot: number;
        if (t < 0.3) wRot = this.lerp(0, -50, t / 0.3);
        else if (t < 0.55) wRot = this.lerp(-50, 85, (t - 0.3) / 0.25);
        else wRot = this.lerp(85, 0, (t - 0.55) / 0.45);
        return {
          bodyDY: t < 0.3 ? -1.5 : t < 0.55 ? 2 : this.lerp(2, 0, (t - 0.55) / 0.45),
          legLDY: 0,
          legRDY: t < 0.55 ? 3 : this.lerp(3, 0, (t - 0.55) / 0.45),
          armLRot: -5,
          armRRot: wRot * 0.4,
          torsoRot: t < 0.55 ? this.lerp(0, -8, t / 0.55) : this.lerp(-8, 0, (t - 0.55) / 0.45),
          weaponRot: wRot,
        };
      }
      case 'cast': {
        const ct = t;
        const armUp = ct < 0.5 ? this.lerp(0, -45, ct * 2) : this.lerp(-45, 0, (ct - 0.5) * 2);
        return {
          bodyDY: ct < 0.5 ? this.lerp(0, -3, ct * 2) : this.lerp(-3, 0, (ct - 0.5) * 2),
          legLDY: 0, legRDY: 0,
          armLRot: armUp,
          armRRot: armUp,
          torsoRot: 0,
          weaponRot: armUp * 0.7,
        };
      }
      case 'hurt':
        return {
          bodyDY: 2,
          legLDY: 0, legRDY: 0,
          armLRot: frame === 0 ? 18 : 8,
          armRRot: frame === 0 ? 20 : 10,
          torsoRot: frame === 0 ? 10 : 5,
          weaponRot: frame === 0 ? 25 : 12,
        };
      case 'death':
        return {
          bodyDY: this.lerp(0, 12, t),
          legLDY: this.lerp(0, 3, t),
          legRDY: this.lerp(0, -2, t),
          armLRot: this.lerp(0, 35, t),
          armRRot: this.lerp(0, 45, t),
          torsoRot: this.lerp(0, 30, t),
          weaponRot: this.lerp(0, 60, t),
        };
      default:
        return { bodyDY: 0, legLDY: 0, legRDY: 0, armLRot: 0, armRRot: 0, torsoRot: 0, weaponRot: 0 };
    }
  }

  // ── Humanoid Frame Drawing ───────────────────────────────────────────

  private drawHumanoidFrame(
    ctx: CanvasRenderingContext2D, ox: number, oy: number,
    fw: number, fh: number, cfg: SpriteConfig, anim: AnimOffsets, action: string,
  ): void {
    ctx.save();
    ctx.translate(ox, oy);

    const s = TEXTURE_SCALE;
    const cx = fw / 2;
    const ground = fh - 6 * s;
    const by = ground + anim.bodyDY * s;

    const bw = cfg.bodyScale;
    const ls = cfg.legScale;
    const hs = cfg.headScale;

    // Shadow
    ctx.fillStyle = 'rgba(0,0,0,0.35)';
    this.fillEllipse(ctx, cx, ground + 2 * s, 16 * s * bw, 5 * s);

    // ── Legs ──
    const legW = 6 * s * bw, legH = 14 * s * ls;
    const legY = by - 18 * s;
    this.drawPart(ctx, cx - 8 * s * bw, legY + anim.legLDY * s, legW, legH, cfg.bodyDark, 2 * s);
    this.drawPart(ctx, cx + 2 * s * bw, legY + anim.legRDY * s, legW, legH, cfg.bodyDark, 2 * s);

    // Boots
    const bootY = by - 5 * s;
    this.drawPart(ctx, cx - 9 * s * bw, bootY + anim.legLDY * s, 8 * s * bw, 6 * s, cfg.secondColor, 2 * s);
    this.drawPart(ctx, cx + 1 * s * bw, bootY + anim.legRDY * s, 8 * s * bw, 6 * s, cfg.secondColor, 2 * s);

    // ── Back arm ──
    ctx.save();
    ctx.translate(cx - 14 * s * bw, by - 42 * s);
    ctx.rotate(anim.armLRot * Math.PI / 180);
    this.drawPart(ctx, -3 * s, 0, 6 * s, 14 * s, cfg.bodyColor, 2 * s);
    // Hand
    ctx.fillStyle = this.rgb(cfg.skinColor);
    this.fillCircle(ctx, 0, 15 * s, 3 * s);
    // Left weapon
    if (cfg.weaponL === 'shield') {
      this.drawShield(ctx, -4 * s, 4 * s, s, cfg);
    } else if (cfg.weaponL === 'dagger') {
      this.drawWeapon(ctx, 0, 15 * s, anim.weaponRot * 0.5, 'dagger', s);
    }
    ctx.restore();

    // ── Torso ──
    ctx.save();
    ctx.translate(cx, by - 38 * s);
    ctx.rotate(anim.torsoRot * Math.PI / 180);
    const tw = 26 * s * bw, th = 20 * s;
    this.drawPart(ctx, -tw / 2, 0, tw, th, cfg.bodyColor, 3 * s);
    // Armor detail - center line
    ctx.fillStyle = this.rgb(cfg.bodyDark, 0.35);
    ctx.fillRect(-0.5 * s, 2 * s, 1 * s, th - 4 * s);
    // Belt
    this.drawPart(ctx, -tw / 2, th - 4 * s, tw, 4 * s, cfg.secondColor, 1 * s);
    // Buckle
    ctx.fillStyle = this.rgb(0x8b7d2a, 0.7);
    ctx.fillRect(-2 * s, th - 3.5 * s, 4 * s, 3 * s);
    ctx.restore();

    // ── Front arm + weapon ──
    ctx.save();
    ctx.translate(cx + 14 * s * bw, by - 42 * s);
    ctx.rotate(anim.armRRot * Math.PI / 180);
    this.drawPart(ctx, -3 * s, 0, 6 * s, 14 * s, cfg.bodyColor, 2 * s);
    // Hand
    ctx.fillStyle = this.rgb(cfg.skinColor);
    this.fillCircle(ctx, 0, 15 * s, 3 * s);
    // Weapon
    if (cfg.weaponR !== 'none') {
      this.drawWeapon(ctx, 0, 14 * s, anim.weaponRot, cfg.weaponR, s);
    }
    ctx.restore();

    // ── Pauldrons ──
    ctx.fillStyle = this.rgb(cfg.bodyDark);
    this.fillEllipse(ctx, cx - 13 * s * bw, by - 42 * s, 5 * s * bw, 3 * s);
    this.fillEllipse(ctx, cx + 13 * s * bw, by - 42 * s, 5 * s * bw, 3 * s);
    ctx.fillStyle = this.rgb(cfg.bodyLight, 0.3);
    this.fillEllipse(ctx, cx - 13 * s * bw, by - 43 * s, 4 * s * bw, 2 * s);
    this.fillEllipse(ctx, cx + 13 * s * bw, by - 43 * s, 4 * s * bw, 2 * s);

    // ── Neck ──
    ctx.fillStyle = this.rgb(cfg.skinColor);
    ctx.fillRect(cx - 3 * s, by - 50 * s, 6 * s, 6 * s);

    // ── Head ──
    const headY = by - 62 * s;
    const headR = 9 * s * hs;
    ctx.fillStyle = this.rgb(cfg.skinColor);
    this.roundRect(ctx, cx - headR, headY, headR * 2, headR * 2, headR * 0.5);
    ctx.fill();
    // Darker chin shadow
    ctx.fillStyle = this.rgb(darkenHex(cfg.skinColor, 15), 0.3);
    this.fillEllipse(ctx, cx, headY + headR * 1.6, headR * 0.8, headR * 0.3);

    // Eyes
    const eyeY = headY + headR * 0.7;
    ctx.fillStyle = '#1a1a22';
    this.fillEllipse(ctx, cx - 3.5 * s * hs, eyeY, 2 * s, 2.5 * s);
    this.fillEllipse(ctx, cx + 3.5 * s * hs, eyeY, 2 * s, 2.5 * s);
    // Eye shine
    ctx.fillStyle = 'rgba(255,255,255,0.5)';
    this.fillCircle(ctx, cx - 3 * s * hs, eyeY - 0.5 * s, 0.7 * s);
    this.fillCircle(ctx, cx + 4 * s * hs, eyeY - 0.5 * s, 0.7 * s);

    // ── Headgear ──
    this.drawHeadgear(ctx, cx, headY, headR, cfg, s);

    // ── Wings (optional) ──
    if (cfg.hasWings) {
      ctx.fillStyle = this.rgb(cfg.bodyDark, 0.5);
      // Left wing
      ctx.beginPath();
      ctx.moveTo(cx - 12 * s * bw, by - 38 * s);
      ctx.lineTo(cx - 28 * s * bw, by - 55 * s);
      ctx.lineTo(cx - 20 * s * bw, by - 28 * s);
      ctx.closePath(); ctx.fill();
      // Right wing
      ctx.beginPath();
      ctx.moveTo(cx + 12 * s * bw, by - 38 * s);
      ctx.lineTo(cx + 28 * s * bw, by - 55 * s);
      ctx.lineTo(cx + 20 * s * bw, by - 28 * s);
      ctx.closePath(); ctx.fill();
    }

    // ── Tail (optional) ──
    if (cfg.hasTail) {
      ctx.strokeStyle = this.rgb(cfg.bodyDark, 0.6);
      ctx.lineWidth = 2 * s;
      ctx.beginPath();
      ctx.moveTo(cx, by - 20 * s);
      ctx.quadraticCurveTo(cx + 15 * s, by - 10 * s, cx + 20 * s, by - 18 * s);
      ctx.stroke();
      // Tail tip
      ctx.fillStyle = this.rgb(cfg.accentColor);
      this.fillCircle(ctx, cx + 20 * s, by - 18 * s, 2 * s);
    }

    // ── Fire aura (optional) ──
    if (cfg.isFire) {
      for (let i = 0; i < 5; i++) {
        const fx = cx + (Math.random() - 0.5) * 20 * s;
        const fy = by - 20 * s - Math.random() * 40 * s;
        ctx.fillStyle = `rgba(${200 + Math.random() * 55 | 0},${80 + Math.random() * 80 | 0},0,${0.15 + Math.random() * 0.15})`;
        this.fillEllipse(ctx, fx, fy, 3 * s + Math.random() * 4 * s, 5 * s + Math.random() * 6 * s);
      }
    }

    // Death: fade with alpha
    if (action === 'death') {
      const alpha = 1.0 - (anim.bodyDY / 12) * 0.5;
      ctx.globalAlpha = Math.max(0, alpha);
    }

    ctx.restore();
  }

  // ── Blob Frame Drawing (slimes, worms) ──────────────────────────────

  private drawBlobFrame(
    ctx: CanvasRenderingContext2D, ox: number, oy: number,
    fw: number, fh: number, cfg: SpriteConfig, anim: AnimOffsets,
    action: string, frame: number, total: number,
  ): void {
    ctx.save();
    ctx.translate(ox, oy);

    const s = TEXTURE_SCALE;
    const cx = fw / 2;
    const ground = fh - 4 * s;
    const by = ground + anim.bodyDY * s;

    // Compute blob squash/stretch
    let sx = 1.0, sy = 1.0;
    if (action === 'idle') {
      const phase = (frame / total) * Math.PI * 2;
      sx = 1 + Math.sin(phase) * 0.08;
      sy = 1 - Math.sin(phase) * 0.08;
    } else if (action === 'walk') {
      const phase = (frame / total) * Math.PI * 2;
      sx = 1 - Math.sin(phase) * 0.12;
      sy = 1 + Math.sin(phase) * 0.12;
    } else if (action === 'attack') {
      const t = frame / Math.max(1, total - 1);
      if (t < 0.5) { sx = 1.15; sy = 0.85; } else { sx = 0.9; sy = 1.1; }
    } else if (action === 'death') {
      const t = frame / Math.max(1, total - 1);
      sx = 1 + t * 0.8; sy = 1 - t * 0.6;
    }

    // Shadow
    ctx.fillStyle = 'rgba(0,0,0,0.3)';
    this.fillEllipse(ctx, cx, ground + 2 * s, 14 * s * sx, 4 * s);

    // Body blob
    const blobRx = fw * 0.35 * sx, blobRy = fh * 0.35 * sy;
    const blobCy = by - fh * 0.35;

    const grad = ctx.createRadialGradient(cx - blobRx * 0.2, blobCy - blobRy * 0.3, 0, cx, blobCy, Math.max(blobRx, blobRy));
    grad.addColorStop(0, this.rgb(cfg.bodyLight));
    grad.addColorStop(0.5, this.rgb(cfg.bodyColor));
    grad.addColorStop(1, this.rgb(cfg.bodyDark));
    ctx.fillStyle = grad;
    this.fillEllipse(ctx, cx, blobCy, blobRx, blobRy);

    // Highlight
    ctx.fillStyle = this.rgb(cfg.bodyLight, 0.35);
    this.fillEllipse(ctx, cx - blobRx * 0.25, blobCy - blobRy * 0.3, blobRx * 0.4, blobRy * 0.3);

    // Drips at base
    ctx.fillStyle = this.rgb(cfg.bodyColor, 0.5);
    this.fillEllipse(ctx, cx - blobRx * 0.4, ground, blobRx * 0.2, 2 * s);
    this.fillEllipse(ctx, cx + blobRx * 0.3, ground - s, blobRx * 0.15, 1.5 * s);

    // Eyes
    const eyeY = blobCy - blobRy * 0.1;
    ctx.fillStyle = '#e8e8e0';
    this.fillEllipse(ctx, cx - blobRx * 0.25, eyeY, blobRx * 0.18, blobRy * 0.2);
    this.fillEllipse(ctx, cx + blobRx * 0.25, eyeY, blobRx * 0.18, blobRy * 0.2);
    ctx.fillStyle = '#0a1a0a';
    this.fillCircle(ctx, cx - blobRx * 0.22, eyeY + blobRy * 0.02, blobRx * 0.08);
    this.fillCircle(ctx, cx + blobRx * 0.22, eyeY + blobRy * 0.02, blobRx * 0.08);

    // Mouth
    if (action === 'attack') {
      ctx.fillStyle = this.rgb(cfg.bodyDark, 0.7);
      this.fillEllipse(ctx, cx, blobCy + blobRy * 0.3, blobRx * 0.25, blobRy * 0.12);
    }

    ctx.restore();
  }

  // ── Headgear Drawing ─────────────────────────────────────────────────

  private drawHeadgear(ctx: CanvasRenderingContext2D, cx: number, headY: number, headR: number, cfg: SpriteConfig, s: number): void {
    switch (cfg.headgear) {
      case 'helm': {
        // Steel helm
        const hGrad = ctx.createLinearGradient(cx - headR, headY - headR * 0.3, cx + headR, headY + headR);
        hGrad.addColorStop(0, '#5a6070');
        hGrad.addColorStop(0.5, '#3a4050');
        hGrad.addColorStop(1, '#2a3040');
        ctx.fillStyle = hGrad;
        this.roundRect(ctx, cx - headR * 1.1, headY - headR * 0.3, headR * 2.2, headR * 1.3, headR * 0.4);
        ctx.fill();
        // Visor slit
        ctx.fillStyle = '#0a0a12';
        ctx.fillRect(cx - headR * 0.7, headY + headR * 0.5, headR * 1.4, 2 * s);
        // Nose guard
        ctx.fillStyle = '#4a5060';
        ctx.fillRect(cx - 1.5 * s, headY + headR * 0.5, 3 * s, headR * 0.5);
        // Crest
        ctx.fillStyle = this.rgb(cfg.accentColor, 0.8);
        ctx.fillRect(cx - 1.5 * s, headY - headR * 0.6, 3 * s, headR * 0.5);
        break;
      }
      case 'hat': {
        // Wizard hat
        ctx.fillStyle = this.rgb(cfg.bodyDark);
        this.fillEllipse(ctx, cx, headY + headR * 0.3, headR * 1.5, headR * 0.35);
        ctx.beginPath();
        ctx.moveTo(cx, headY - headR * 1.5);
        ctx.lineTo(cx - headR * 1.0, headY + headR * 0.2);
        ctx.lineTo(cx + headR * 1.0, headY + headR * 0.2);
        ctx.closePath();
        ctx.fillStyle = this.rgb(cfg.bodyColor);
        ctx.fill();
        // Star
        ctx.fillStyle = this.rgb(0xb8860b, 0.9);
        this.fillCircle(ctx, cx, headY - headR * 1.1, 2 * s);
        break;
      }
      case 'hood': {
        ctx.fillStyle = this.rgb(cfg.bodyDark);
        this.roundRect(ctx, cx - headR * 1.1, headY - headR * 0.5, headR * 2.2, headR * 1.6, headR * 0.5);
        ctx.fill();
        // Hood point
        ctx.beginPath();
        ctx.moveTo(cx, headY - headR * 0.9);
        ctx.lineTo(cx - headR * 0.8, headY - headR * 0.2);
        ctx.lineTo(cx + headR * 0.8, headY - headR * 0.2);
        ctx.closePath();
        ctx.fillStyle = this.rgb(cfg.bodyColor);
        ctx.fill();
        // Shadow under hood
        ctx.fillStyle = 'rgba(0,0,0,0.3)';
        this.fillEllipse(ctx, cx, headY + headR * 0.5, headR * 0.9, headR * 0.25);
        break;
      }
      case 'crown': {
        ctx.fillStyle = '#8a7020';
        ctx.fillRect(cx - headR * 0.8, headY - headR * 0.3, headR * 1.6, headR * 0.35);
        // Points
        for (let i = -1; i <= 1; i++) {
          ctx.beginPath();
          const px = cx + i * headR * 0.5;
          ctx.moveTo(px - 2 * s, headY - headR * 0.3);
          ctx.lineTo(px, headY - headR * 0.7);
          ctx.lineTo(px + 2 * s, headY - headR * 0.3);
          ctx.closePath(); ctx.fill();
        }
        // Gem
        ctx.fillStyle = '#8a1a1a';
        this.fillCircle(ctx, cx, headY - headR * 0.5, 1.5 * s);
        break;
      }
      case 'horns': {
        ctx.fillStyle = '#2a2020';
        // Left horn
        ctx.beginPath();
        ctx.moveTo(cx - headR * 0.7, headY);
        ctx.quadraticCurveTo(cx - headR * 1.2, headY - headR * 1.0, cx - headR * 0.9, headY - headR * 1.3);
        ctx.lineTo(cx - headR * 0.5, headY);
        ctx.closePath(); ctx.fill();
        // Right horn
        ctx.beginPath();
        ctx.moveTo(cx + headR * 0.7, headY);
        ctx.quadraticCurveTo(cx + headR * 1.2, headY - headR * 1.0, cx + headR * 0.9, headY - headR * 1.3);
        ctx.lineTo(cx + headR * 0.5, headY);
        ctx.closePath(); ctx.fill();
        break;
      }
      case 'ears': {
        ctx.fillStyle = this.rgb(cfg.skinColor);
        // Pointy ears
        ctx.beginPath();
        ctx.moveTo(cx - headR * 1.0, headY + headR * 0.4);
        ctx.lineTo(cx - headR * 1.5, headY - headR * 0.2);
        ctx.lineTo(cx - headR * 0.8, headY + headR * 0.1);
        ctx.closePath(); ctx.fill();
        ctx.beginPath();
        ctx.moveTo(cx + headR * 1.0, headY + headR * 0.4);
        ctx.lineTo(cx + headR * 1.5, headY - headR * 0.2);
        ctx.lineTo(cx + headR * 0.8, headY + headR * 0.1);
        ctx.closePath(); ctx.fill();
        // Inner ear
        ctx.fillStyle = this.rgb(lightenHex(cfg.skinColor, 20), 0.4);
        ctx.beginPath();
        ctx.moveTo(cx - headR * 0.95, headY + headR * 0.3);
        ctx.lineTo(cx - headR * 1.3, headY - headR * 0.05);
        ctx.lineTo(cx - headR * 0.85, headY + headR * 0.15);
        ctx.closePath(); ctx.fill();
        ctx.beginPath();
        ctx.moveTo(cx + headR * 0.95, headY + headR * 0.3);
        ctx.lineTo(cx + headR * 1.3, headY - headR * 0.05);
        ctx.lineTo(cx + headR * 0.85, headY + headR * 0.15);
        ctx.closePath(); ctx.fill();
        break;
      }
    }
  }

  // ── Weapon Drawing ───────────────────────────────────────────────────

  private drawWeapon(ctx: CanvasRenderingContext2D, px: number, py: number, angle: number, type: string, s: number): void {
    ctx.save();
    ctx.translate(px, py);
    ctx.rotate(angle * Math.PI / 180);

    switch (type) {
      case 'sword':
        // Blade
        this.drawPart(ctx, -1.5 * s, -28 * s, 3 * s, 26 * s, 0x7a8a9a, s);
        ctx.fillStyle = 'rgba(200,220,240,0.25)';
        ctx.fillRect(-0.5 * s, -26 * s, 1 * s, 22 * s);
        // Guard
        this.drawPart(ctx, -4 * s, -3 * s, 8 * s, 3 * s, 0x5a3a1a, s);
        // Grip
        this.drawPart(ctx, -1.5 * s, 0, 3 * s, 6 * s, 0x2a1a0a, s);
        // Pommel
        ctx.fillStyle = this.rgb(0x8b7020);
        this.fillCircle(ctx, 0, 7 * s, 2 * s);
        break;
      case 'staff':
        this.drawPart(ctx, -1.5 * s, -32 * s, 3 * s, 38 * s, 0x2a1a0a, s);
        // Orb
        ctx.fillStyle = this.rgb(0x5a2a7a);
        this.fillCircle(ctx, 0, -35 * s, 4.5 * s);
        ctx.fillStyle = 'rgba(160,100,220,0.25)';
        this.fillCircle(ctx, 0, -35 * s, 7 * s);
        ctx.fillStyle = 'rgba(200,160,255,0.5)';
        this.fillCircle(ctx, -1.5 * s, -37 * s, 1.5 * s);
        break;
      case 'dagger':
        this.drawPart(ctx, -1 * s, -10 * s, 2 * s, 10 * s, 0x8a9aaa, 0.5 * s);
        ctx.fillStyle = 'rgba(200,220,240,0.2)';
        ctx.fillRect(-0.3 * s, -9 * s, 0.6 * s, 8 * s);
        this.drawPart(ctx, -2.5 * s, -1 * s, 5 * s, 2 * s, 0x4a2a10, 0.5 * s);
        break;
      case 'club':
        this.drawPart(ctx, -2 * s, -16 * s, 4 * s, 18 * s, 0x3a2010, s);
        this.drawPart(ctx, -3.5 * s, -20 * s, 7 * s, 5 * s, 0x4a3020, 2 * s);
        // Nails
        ctx.fillStyle = '#808080';
        this.fillCircle(ctx, -2 * s, -18 * s, 0.8 * s);
        this.fillCircle(ctx, 2 * s, -17 * s, 0.8 * s);
        break;
      case 'axe':
        this.drawPart(ctx, -1.5 * s, -22 * s, 3 * s, 26 * s, 0x2a1a0a, s);
        ctx.fillStyle = this.rgb(0x5a5a60);
        ctx.beginPath();
        ctx.moveTo(1.5 * s, -22 * s);
        ctx.lineTo(7 * s, -18 * s);
        ctx.lineTo(7 * s, -12 * s);
        ctx.lineTo(1.5 * s, -8 * s);
        ctx.closePath(); ctx.fill();
        // Edge highlight
        ctx.fillStyle = 'rgba(180,180,190,0.3)';
        ctx.beginPath();
        ctx.moveTo(5 * s, -20 * s); ctx.lineTo(7 * s, -18 * s);
        ctx.lineTo(7 * s, -12 * s); ctx.lineTo(5 * s, -10 * s);
        ctx.closePath(); ctx.fill();
        break;
      case 'claws':
        for (let i = -1; i <= 1; i++) {
          ctx.fillStyle = '#c0c0c0';
          ctx.beginPath();
          ctx.moveTo(i * 2.5 * s - 0.5 * s, 0);
          ctx.lineTo(i * 2.5 * s, -8 * s);
          ctx.lineTo(i * 2.5 * s + 0.5 * s, 0);
          ctx.closePath(); ctx.fill();
        }
        break;
    }
    ctx.restore();
  }

  private drawShield(ctx: CanvasRenderingContext2D, x: number, y: number, s: number, cfg: SpriteConfig): void {
    const sw = 10 * s, sh = 14 * s;
    this.drawPart(ctx, x - sw / 2, y, sw, sh, cfg.bodyDark, 2 * s);
    // Emblem
    ctx.fillStyle = this.rgb(cfg.accentColor, 0.6);
    ctx.fillRect(x - 1 * s, y + 2 * s, 2 * s, sh - 4 * s);
    ctx.fillRect(x - sw / 2 + 2 * s, y + sh / 2 - 1 * s, sw - 4 * s, 2 * s);
    // Rim
    ctx.strokeStyle = this.rgb(cfg.bodyLight, 0.3);
    ctx.lineWidth = 0.5 * s;
    this.roundRect(ctx, x - sw / 2, y, sw, sh, 2 * s);
    ctx.stroke();
  }

  // ═══════════════════════════════════════════════════════════════════════
  // ██ NPC SPRITES ██
  // ═══════════════════════════════════════════════════════════════════════

  private generateNPCSprites(): void {
    for (const npc of NPC_CONFIGS) {
      this.makeNPCSheet(npc);
    }
  }

  private makeNPCSheet(npc: NPCConfig): void {
    const s = TEXTURE_SCALE;
    const fw = 48 * s, fh = 80 * s;
    const frames = 8;
    const [canvas, ctx] = this.createCanvas(fw * frames, fh);
    const skin = npc.skinColor ?? 0xb08960;
    const hair = npc.hairColor ?? 0x3a2a1a;
    const acc = npc.accessory ?? 'none';
    const bw = npc.bulky ? 1.15 : 1;

    for (let f = 0; f < frames; f++) {
      const ox = f * fw;
      const phase = (f / frames) * Math.PI * 2;

      // Per-accessory animation curves
      let bob = Math.sin(phase) * 1.0 * s;
      let leftArmY = 0;   // offset from default arm position
      let rightArmY = 0;
      let headTilt = 0;    // head rotation in radians
      let bodyLean = 0;    // horizontal lean

      switch (acc) {
        case 'hammer': {
          // Hammering: right arm raises then strikes down
          bob = Math.sin(phase) * 0.5 * s;
          const strike = Math.sin(phase);
          rightArmY = strike > 0 ? -strike * 10 : strike * 4; // raise high, come down hard
          leftArmY = Math.sin(phase + 0.5) * 1.5;
          bodyLean = strike > 0 ? -0.5 : 0.5;
          break;
        }
        case 'pickaxe': {
          // Similar to hammer but with more body lean
          bob = Math.sin(phase) * 0.5 * s;
          const dig = Math.sin(phase);
          rightArmY = dig > 0 ? -dig * 12 : dig * 3;
          leftArmY = Math.sin(phase + 0.3) * 2;
          bodyLean = dig > 0 ? -1 : 1;
          headTilt = dig * 0.05;
          break;
        }
        case 'coinbag': {
          // Weighing coins: gentle bounce of right hand, shift weight
          bob = Math.sin(phase) * 1.5 * s;
          rightArmY = Math.sin(phase * 2) * 3;
          leftArmY = Math.sin(phase + 1) * 1;
          bodyLean = Math.sin(phase) * 0.5;
          break;
        }
        case 'staff': {
          // Leaning on staff: gentle sway, slight lean
          bob = Math.sin(phase) * 0.8 * s;
          rightArmY = Math.sin(phase) * 1;
          leftArmY = Math.sin(phase + 2) * 2;
          bodyLean = Math.sin(phase) * 0.8;
          headTilt = Math.sin(phase + 1) * 0.03;
          break;
        }
        case 'sword': {
          // Alert stance: subtle weight shift, sword steady
          bob = Math.sin(phase) * 0.5 * s;
          rightArmY = Math.sin(phase) * 1;
          leftArmY = Math.sin(phase + Math.PI) * 2;
          bodyLean = Math.sin(phase) * 0.3;
          headTilt = Math.sin(phase * 2) * 0.02;
          break;
        }
        case 'lantern': {
          // Lantern sway: right arm swings gently, body shifts
          bob = Math.sin(phase) * 1.0 * s;
          rightArmY = Math.sin(phase) * 4;
          leftArmY = Math.sin(phase + 1.5) * 1.5;
          bodyLean = Math.sin(phase + 0.5) * 0.6;
          break;
        }
        case 'book': {
          // Reading: looking down, subtle page turn motion
          bob = Math.sin(phase) * 0.6 * s;
          rightArmY = Math.sin(phase * 2) * 1.5;
          leftArmY = Math.sin(phase) * 0.5;
          headTilt = 0.06 + Math.sin(phase) * 0.02; // always slightly tilted down
          break;
        }
        default: {
          // Generic idle
          bob = Math.sin(phase) * 1.0 * s;
          leftArmY = Math.sin(phase) * 2;
          rightArmY = Math.sin(phase + Math.PI) * 2;
          break;
        }
      }

      ctx.save();
      ctx.translate(ox, 0);

      const cx = fw / 2;
      const ground = fh - 6 * s;
      const by = ground + bob;
      const bodyW = Math.round(12 * bw);

      // Shadow
      ctx.fillStyle = 'rgba(0,0,0,0.3)';
      this.fillEllipse(ctx, cx, ground + 2 * s, 14 * s, 4 * s);

      // Legs
      this.drawPart(ctx, cx - 7 * s, by - 16 * s, 5 * s, 14 * s, darkenHex(npc.bodyColor, 15), 2 * s);
      this.drawPart(ctx, cx + 2 * s, by - 16 * s, 5 * s, 14 * s, darkenHex(npc.bodyColor, 15), 2 * s);
      // Boots
      this.drawPart(ctx, cx - 8 * s, by - 4 * s, 7 * s, 5 * s, 0x2a1a0a, 2 * s);
      this.drawPart(ctx, cx + 1 * s, by - 4 * s, 7 * s, 5 * s, 0x2a1a0a, 2 * s);

      // Cloak (behind body)
      if (npc.cloakColor) {
        this.drawPart(ctx, cx - (bodyW + 2) * s + bodyLean * s, by - 38 * s, (bodyW * 2 + 4) * s, 28 * s, npc.cloakColor, 3 * s);
      }

      // Body (with lean)
      this.drawPart(ctx, cx - bodyW * s + bodyLean * s, by - 36 * s, bodyW * 2 * s, 22 * s, npc.bodyColor, 3 * s);
      // Belt
      this.drawPart(ctx, cx - bodyW * s + bodyLean * s, by - 18 * s, bodyW * 2 * s, 4 * s, darkenHex(npc.bodyColor, 20), 1 * s);

      // Left arm
      const laX = cx - (bodyW + 4) * s + bodyLean * s;
      const laY = by - 34 * s;
      this.drawPart(ctx, laX, laY, 5 * s, 12 * s, npc.bodyColor, 2 * s);
      // Left hand
      ctx.fillStyle = this.rgb(skin);
      this.fillCircle(ctx, laX + 2.5 * s, laY + 12 * s + leftArmY * s, 2.5 * s);

      // Right arm
      const raX = cx + (bodyW - 1) * s + bodyLean * s;
      const raY = by - 34 * s;
      this.drawPart(ctx, raX, raY, 5 * s, 12 * s, npc.bodyColor, 2 * s);
      // Right hand
      ctx.fillStyle = this.rgb(skin);
      const rhX = raX + 2.5 * s;
      const rhY = raY + 12 * s + rightArmY * s;
      this.fillCircle(ctx, rhX, rhY, 2.5 * s);

      // Accessory in right hand
      ctx.fillStyle = this.rgb(npc.itemColor);
      switch (acc) {
        case 'hammer':
          ctx.fillRect(rhX - 1 * s, rhY - 8 * s, 2 * s, 14 * s);
          this.drawPart(ctx, rhX - 3 * s, rhY - 11 * s, 6 * s, 4 * s, npc.itemColor, 1 * s);
          break;
        case 'coinbag':
          this.fillCircle(ctx, rhX, rhY - 4 * s, 4 * s);
          ctx.fillStyle = this.rgb(0x8a7020);
          this.fillCircle(ctx, rhX, rhY - 4 * s, 2 * s);
          break;
        case 'staff':
          ctx.fillStyle = this.rgb(0x4a3018);
          ctx.fillRect(rhX - 1 * s, rhY - 16 * s, 2.5 * s, 22 * s);
          ctx.fillStyle = this.rgb(npc.itemColor);
          this.fillCircle(ctx, rhX + 0.5 * s, rhY - 17 * s, 3 * s);
          break;
        case 'sword':
          ctx.fillStyle = this.rgb(0x888898);
          ctx.fillRect(rhX - 0.5 * s, rhY - 14 * s, 2 * s, 16 * s);
          ctx.fillStyle = this.rgb(0x5a4020);
          ctx.fillRect(rhX - 2 * s, rhY, 5 * s, 2 * s);
          break;
        case 'pickaxe':
          ctx.fillStyle = this.rgb(0x4a3018);
          ctx.fillRect(rhX - 1 * s, rhY - 10 * s, 2 * s, 16 * s);
          ctx.fillStyle = this.rgb(npc.itemColor);
          ctx.beginPath();
          ctx.moveTo(rhX - 4 * s, rhY - 10 * s);
          ctx.lineTo(rhX + 4 * s, rhY - 12 * s);
          ctx.lineTo(rhX + 4 * s, rhY - 9 * s);
          ctx.closePath(); ctx.fill();
          break;
        case 'lantern': {
          ctx.fillStyle = this.rgb(0x4a4a50);
          ctx.fillRect(rhX - 1.5 * s, rhY - 8 * s, 3 * s, 2 * s);
          ctx.fillStyle = this.rgb(0xffaa30);
          this.roundRect(ctx, rhX - 2.5 * s, rhY - 6 * s, 5 * s, 6 * s, 1 * s);
          ctx.fill();
          // Lantern glow flickers per frame
          const glowAlpha = 0.2 + Math.sin(phase * 2) * 0.1;
          ctx.fillStyle = `rgba(255,200,50,${glowAlpha})`;
          this.fillCircle(ctx, rhX, rhY - 3 * s, 5 * s);
          break;
        }
        case 'scroll':
          ctx.fillStyle = this.rgb(0xd4c8a0);
          this.roundRect(ctx, rhX - 2 * s, rhY - 6 * s, 5 * s, 8 * s, 1.5 * s);
          ctx.fill();
          ctx.fillStyle = this.rgb(0x8a3020);
          ctx.fillRect(rhX - 2.5 * s, rhY - 6 * s, 6 * s, 1.5 * s);
          ctx.fillRect(rhX - 2.5 * s, rhY + 1 * s, 6 * s, 1.5 * s);
          break;
        case 'book':
          ctx.fillStyle = this.rgb(npc.itemColor);
          this.roundRect(ctx, rhX - 3 * s, rhY - 5 * s, 6 * s, 7 * s, 1 * s);
          ctx.fill();
          ctx.fillStyle = this.rgb(lightenHex(npc.itemColor, 40));
          ctx.fillRect(rhX - 2 * s, rhY - 4 * s, 4 * s, 5 * s);
          break;
      }

      // Neck
      ctx.fillStyle = this.rgb(skin);
      ctx.fillRect(cx - 3 * s + bodyLean * s, by - 42 * s, 6 * s, 6 * s);

      // Head (with tilt)
      ctx.save();
      ctx.translate(cx + bodyLean * s, by - 46 * s);
      ctx.rotate(headTilt);
      ctx.fillStyle = this.rgb(skin);
      this.roundRect(ctx, -8 * s, -8 * s, 16 * s, 16 * s, 5 * s);
      ctx.fill();

      // Eyes
      ctx.fillStyle = '#1a1a22';
      this.fillEllipse(ctx, -3 * s, 0, 1.5 * s, 2 * s);
      this.fillEllipse(ctx, 3 * s, 0, 1.5 * s, 2 * s);

      // Hair (drawn in head-local rotated space, origin = head center)
      const hs = npc.hairStyle ?? 'none';
      ctx.fillStyle = this.rgb(hair);
      if (hs === 'short') {
        this.roundRect(ctx, -8 * s, -10 * s, 16 * s, 8 * s, 3 * s);
        ctx.fill();
      } else if (hs === 'long') {
        this.roundRect(ctx, -9 * s, -11 * s, 18 * s, 10 * s, 3 * s);
        ctx.fill();
        // Side hair
        ctx.fillRect(-9 * s, -4 * s, 3 * s, 12 * s);
        ctx.fillRect(6 * s, -4 * s, 3 * s, 12 * s);
      } else if (hs === 'hood') {
        ctx.fillStyle = this.rgb(npc.cloakColor ?? hair);
        ctx.beginPath();
        ctx.moveTo(0, -14 * s);
        ctx.lineTo(-11 * s, 4 * s);
        ctx.lineTo(11 * s, 4 * s);
        ctx.closePath(); ctx.fill();
        // Hood rim
        ctx.fillStyle = this.rgb(darkenHex(npc.cloakColor ?? hair, 15));
        ctx.beginPath();
        ctx.arc(0, 2 * s, 10 * s, Math.PI, 0);
        ctx.closePath(); ctx.fill();
      }

      // Beard (head-local space)
      if (npc.beard) {
        ctx.fillStyle = this.rgb(hair);
        ctx.beginPath();
        ctx.moveTo(-5 * s, 4 * s);
        ctx.lineTo(5 * s, 4 * s);
        ctx.lineTo(3 * s, 12 * s);
        ctx.lineTo(0, 14 * s);
        ctx.lineTo(-3 * s, 12 * s);
        ctx.closePath(); ctx.fill();
      }

      // Hat (on top of hair, non-hood only; head-local space)
      if (hs !== 'hood') {
        this.drawPart(ctx, -9 * s, -12 * s, 18 * s, 6 * s, npc.hatColor, 2 * s);
      }

      ctx.restore(); // end head rotation

      ctx.restore();
    }

    this.applyNoiseToRegion(ctx, 0, 0, canvas.width, canvas.height, 3);

    const key = npc.key;
    if (this.scene.textures.exists(key)) this.scene.textures.remove(key);
    const canvasTex = this.scene.textures.addCanvas(key, canvas)!;
    const frameTotal = Math.floor(canvas.width / fw);
    for (let i = 0; i < frameTotal; i++) {
      canvasTex.add(i, 0, i * fw, 0, fw, fh);
    }
  }

  // ═══════════════════════════════════════════════════════════════════════
  // ██ DECORATIONS & EFFECTS ██
  // ═══════════════════════════════════════════════════════════════════════

  private generateDecorations(): void {
    const s = TEXTURE_SCALE;
    const defs: [string, number, number, (ctx: CanvasRenderingContext2D, w: number, h: number) => void][] = [
      ['decor_tree', 24, 36, (ctx, w, h) => {
        ctx.fillStyle = 'rgba(0,0,0,0.2)'; this.fillEllipse(ctx, w / 2, h - 2 * s, 8 * s, 2 * s);
        this.drawPart(ctx, w / 2 - 2 * s, h * 0.55, 4 * s, h * 0.4, 0x2a1a0a, s);
        const grad = ctx.createRadialGradient(w / 2, h * 0.35, 0, w / 2, h * 0.35, w * 0.45);
        grad.addColorStop(0, '#1a4a18'); grad.addColorStop(1, '#0f2a0e');
        ctx.fillStyle = grad;
        this.fillEllipse(ctx, w / 2, h * 0.35, w * 0.45, h * 0.35);
        ctx.fillStyle = 'rgba(30,70,25,0.4)';
        this.fillEllipse(ctx, w * 0.4, h * 0.28, w * 0.25, h * 0.22);
      }],
      ['decor_bush', 16, 12, (ctx, w, h) => {
        ctx.fillStyle = 'rgba(0,0,0,0.15)'; this.fillEllipse(ctx, w / 2, h - s, 7 * s, 1.5 * s);
        const grad = ctx.createRadialGradient(w / 2, h * 0.5, 0, w / 2, h * 0.5, w * 0.45);
        grad.addColorStop(0, '#1a4a18'); grad.addColorStop(1, '#0f2a0e');
        ctx.fillStyle = grad;
        this.fillEllipse(ctx, w / 2, h * 0.5, w * 0.45, h * 0.45);
      }],
      ['decor_rock', 16, 12, (ctx, w, h) => {
        ctx.fillStyle = 'rgba(0,0,0,0.15)'; this.fillEllipse(ctx, w / 2, h - s, 6 * s, 1.5 * s);
        const grad = ctx.createRadialGradient(w * 0.4, h * 0.4, 0, w / 2, h * 0.5, w * 0.45);
        grad.addColorStop(0, '#505558'); grad.addColorStop(1, '#303538');
        ctx.fillStyle = grad;
        this.fillEllipse(ctx, w / 2, h * 0.5, w * 0.45, h * 0.42);
      }],
      ['decor_flower', 8, 10, (ctx, w, h) => {
        ctx.fillStyle = '#1a3a18'; ctx.fillRect(w / 2 - s, h * 0.5, 2 * s, h * 0.45);
        const colors = ['#6a1a1a', '#6a5a0a', '#5a1a4a', '#4a1a5a'];
        ctx.fillStyle = colors[Math.floor(Math.random() * colors.length)];
        this.fillCircle(ctx, w / 2, h * 0.35, 3 * s);
        ctx.fillStyle = 'rgba(180,160,40,0.6)';
        this.fillCircle(ctx, w / 2, h * 0.35, 1.5 * s);
      }],
      ['decor_mushroom', 10, 12, (ctx, w, h) => {
        ctx.fillStyle = '#706050'; ctx.fillRect(w / 2 - 1.5 * s, h * 0.5, 3 * s, h * 0.4);
        ctx.fillStyle = '#6a1a10';
        this.fillEllipse(ctx, w / 2, h * 0.38, 5 * s, 4 * s);
        ctx.fillStyle = 'rgba(200,200,180,0.4)';
        this.fillCircle(ctx, w * 0.35, h * 0.32, 1.2 * s);
        this.fillCircle(ctx, w * 0.6, h * 0.28, 0.8 * s);
      }],
      ['decor_cactus', 12, 20, (ctx, w, h) => {
        ctx.fillStyle = 'rgba(0,0,0,0.15)'; this.fillEllipse(ctx, w / 2, h - s, 5 * s, 1.5 * s);
        this.drawPart(ctx, w / 2 - 2.5 * s, h * 0.2, 5 * s, h * 0.7, 0x1a4a1a, 2 * s);
        this.drawPart(ctx, 0, h * 0.4, 5 * s, h * 0.15, 0x1a4a1a, 2 * s);
        this.drawPart(ctx, w - 4 * s, h * 0.3, 4 * s, h * 0.15, 0x1a4a1a, 2 * s);
      }],
      ['decor_boulder', 20, 16, (ctx, w, h) => {
        ctx.fillStyle = 'rgba(0,0,0,0.2)'; this.fillEllipse(ctx, w / 2, h - s, 9 * s, 2 * s);
        const grad = ctx.createRadialGradient(w * 0.4, h * 0.35, 0, w / 2, h * 0.5, w * 0.45);
        grad.addColorStop(0, '#4a4a50'); grad.addColorStop(1, '#2a2a30');
        ctx.fillStyle = grad;
        this.fillEllipse(ctx, w / 2, h * 0.5, w * 0.45, h * 0.45);
        ctx.fillStyle = 'rgba(70,70,80,0.3)';
        this.fillEllipse(ctx, w * 0.38, h * 0.4, w * 0.2, h * 0.18);
      }],
      ['decor_crystal', 10, 16, (ctx, w, h) => {
        ctx.fillStyle = 'rgba(0,0,0,0.15)'; this.fillEllipse(ctx, w / 2, h - s, 4 * s, 1.5 * s);
        ctx.fillStyle = '#3a1a5a';
        ctx.beginPath();
        ctx.moveTo(w / 2, 0); ctx.lineTo(w - s, h * 0.85); ctx.lineTo(s, h * 0.85);
        ctx.closePath(); ctx.fill();
        ctx.fillStyle = 'rgba(140,80,200,0.4)';
        ctx.beginPath();
        ctx.moveTo(w / 2, h * 0.1); ctx.lineTo(w * 0.65, h * 0.75); ctx.lineTo(w * 0.25, h * 0.75);
        ctx.closePath(); ctx.fill();
        ctx.fillStyle = 'rgba(255,255,255,0.2)';
        ctx.beginPath();
        ctx.moveTo(w * 0.4, h * 0.25); ctx.lineTo(w * 0.5, h * 0.55); ctx.lineTo(w * 0.3, h * 0.55);
        ctx.closePath(); ctx.fill();
      }],
      ['decor_bones', 14, 10, (ctx, w, h) => {
        ctx.fillStyle = 'rgba(180,170,150,0.7)';
        this.roundRect(ctx, s, h * 0.3, w - 2 * s, 2 * s, s);
        ctx.fill();
        this.roundRect(ctx, w * 0.3, s, 2 * s, h - 2 * s, s);
        ctx.fill();
        this.fillCircle(ctx, s + s, h * 0.35, 1.5 * s);
        this.fillCircle(ctx, w - 2 * s, h * 0.35, 1.5 * s);
        this.fillCircle(ctx, w * 0.35, s + s, 1.2 * s);
        this.fillCircle(ctx, w * 0.35, h - 2 * s, 1.2 * s);
      }],
    ];

    for (const [key, bw, bh, drawFn] of defs) {
      const w = bw * s, h = bh * s;
      const [canvas, ctx] = this.createCanvas(w, h);
      drawFn(ctx, w, h);
      this.applyNoiseToRegion(ctx, 0, 0, w, h, 3);
      if (this.scene.textures.exists(key)) this.scene.textures.remove(key);
      this.scene.textures.addCanvas(key, canvas);
    }
  }

  // ── Sprite Helper ────────────────────────────────────────────────────

  private makeSprite(
    key: string, w: number, h: number,
    drawFn: (ctx: CanvasRenderingContext2D, w: number, h: number) => void,
  ): void {
    if (this.scene.textures.exists(key)) return;
    const [canvas, ctx] = this.createCanvas(w, h);
    drawFn(ctx, w, h);
    this.scene.textures.addCanvas(key, canvas);
  }

  // ═══════════════════════════════════════════════════════════════════════
  // ██ CAMP DECORATION SPRITES ██
  // ═══════════════════════════════════════════════════════════════════════

  private generateCampDecorations(): void {
    const s = TEXTURE_SCALE;

    // camp_campfire — 48x40: stone ring base + crossed logs (fire rendered as particles in ZoneScene)
    this.makeSprite('camp_campfire', 48 * s, 40 * s, (ctx, w, h) => {
      // Shadow
      ctx.fillStyle = 'rgba(0,0,0,0.25)';
      this.fillEllipse(ctx, w / 2, h - 2 * s, 16 * s, 5 * s);
      // Stone ring base
      ctx.fillStyle = '#555560';
      this.fillEllipse(ctx, w / 2, h * 0.72, 16 * s, 6 * s);
      ctx.fillStyle = '#404045';
      this.fillEllipse(ctx, w / 2, h * 0.72, 11 * s, 4 * s);
      // Crossed logs
      ctx.strokeStyle = '#3a2010';
      ctx.lineWidth = 3 * s;
      ctx.lineCap = 'round';
      ctx.beginPath(); ctx.moveTo(w * 0.25, h * 0.78); ctx.lineTo(w * 0.75, h * 0.55); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(w * 0.75, h * 0.78); ctx.lineTo(w * 0.25, h * 0.55); ctx.stroke();
      // Embers glow (static hint — particles do the real fire)
      ctx.fillStyle = 'rgba(220,100,20,0.3)';
      this.fillEllipse(ctx, w / 2, h * 0.58, 6 * s, 3 * s);
    });

    // camp_torch — 24x40: vertical pole + bracket (flame rendered as particles in ZoneScene)
    this.makeSprite('camp_torch', 24 * s, 40 * s, (ctx, w, h) => {
      // Pole
      ctx.fillStyle = '#3a2010';
      ctx.fillRect(w / 2 - 2 * s, h * 0.35, 4 * s, h * 0.62);
      // Bracket at top
      ctx.fillStyle = '#555560';
      ctx.fillRect(w / 2 - 3.5 * s, h * 0.32, 7 * s, 2.5 * s);
      // Ember glow hint (static — particles do the real flame)
      ctx.fillStyle = 'rgba(220,100,20,0.25)';
      this.fillEllipse(ctx, w / 2, h * 0.28, 4 * s, 3 * s);
    });

    // camp_tent — 64x56: triangular pitched tent shape (roughly 1 tile footprint)
    this.makeSprite('camp_tent', 64 * s, 56 * s, (ctx, w, h) => {
      // Shadow
      ctx.fillStyle = 'rgba(0,0,0,0.2)';
      this.fillEllipse(ctx, w / 2, h - 2 * s, 26 * s, 5 * s);
      // Tent body (dark base)
      ctx.fillStyle = '#3a2a1a';
      ctx.beginPath();
      ctx.moveTo(w / 2, h * 0.08);
      ctx.lineTo(w * 0.95, h * 0.85);
      ctx.lineTo(w * 0.05, h * 0.85);
      ctx.closePath(); ctx.fill();
      // Tent facing panel (lighter)
      ctx.fillStyle = '#5a4028';
      ctx.beginPath();
      ctx.moveTo(w / 2, h * 0.15);
      ctx.lineTo(w * 0.78, h * 0.85);
      ctx.lineTo(w * 0.22, h * 0.85);
      ctx.closePath(); ctx.fill();
      // Inner flap / door shadow
      ctx.fillStyle = 'rgba(0,0,0,0.4)';
      ctx.beginPath();
      ctx.moveTo(w / 2, h * 0.45);
      ctx.lineTo(w * 0.60, h * 0.85);
      ctx.lineTo(w * 0.40, h * 0.85);
      ctx.closePath(); ctx.fill();
      // Ridge seam highlight
      ctx.strokeStyle = 'rgba(200,170,120,0.3)';
      ctx.lineWidth = s;
      ctx.beginPath();
      ctx.moveTo(w / 2, h * 0.08); ctx.lineTo(w / 2, h * 0.45);
      ctx.stroke();
      // Pole tip
      ctx.fillStyle = '#808080';
      this.fillCircle(ctx, w / 2, h * 0.05, 1.5 * s);
    });

    // camp_barrel — 24x32: rectangular body, horizontal band lines, elliptical top
    this.makeSprite('camp_barrel', 24 * s, 32 * s, (ctx, w, h) => {
      // Shadow
      ctx.fillStyle = 'rgba(0,0,0,0.2)';
      this.fillEllipse(ctx, w / 2, h - s, 9 * s, 2.5 * s);
      // Body
      const bGrad = ctx.createLinearGradient(w * 0.15, 0, w * 0.85, 0);
      bGrad.addColorStop(0, '#3a2a18');
      bGrad.addColorStop(0.4, '#5a4028');
      bGrad.addColorStop(1, '#2a1a0a');
      ctx.fillStyle = bGrad;
      this.roundRect(ctx, w * 0.15, h * 0.12, w * 0.7, h * 0.78, 2 * s);
      ctx.fill();
      // Metal band lines
      ctx.strokeStyle = '#707880';
      ctx.lineWidth = 1.2 * s;
      for (const band of [0.25, 0.5, 0.75]) {
        ctx.beginPath();
        ctx.moveTo(w * 0.15, h * band);
        ctx.lineTo(w * 0.85, h * band);
        ctx.stroke();
      }
      // Elliptical top
      ctx.fillStyle = '#4a3820';
      this.fillEllipse(ctx, w / 2, h * 0.14, w * 0.35, h * 0.07);
      ctx.strokeStyle = '#707880';
      ctx.lineWidth = 0.8 * s;
      ctx.beginPath();
      ctx.ellipse(w / 2, h * 0.14, w * 0.35, h * 0.07, 0, 0, Math.PI * 2);
      ctx.stroke();
    });

    // camp_crate — 28x24: box with cross-plank detail, flat top
    this.makeSprite('camp_crate', 28 * s, 24 * s, (ctx, w, h) => {
      // Shadow
      ctx.fillStyle = 'rgba(0,0,0,0.2)';
      this.fillEllipse(ctx, w / 2, h - s, 11 * s, 2.5 * s);
      // Box body
      const cGrad = ctx.createLinearGradient(0, h * 0.1, 0, h * 0.9);
      cGrad.addColorStop(0, '#7a6040');
      cGrad.addColorStop(1, '#4a3820');
      ctx.fillStyle = cGrad;
      this.roundRect(ctx, w * 0.05, h * 0.1, w * 0.9, h * 0.82, s);
      ctx.fill();
      // Cross-plank detail
      ctx.strokeStyle = 'rgba(30,18,8,0.5)';
      ctx.lineWidth = s;
      // Horizontal center line
      ctx.beginPath(); ctx.moveTo(w * 0.05, h * 0.5); ctx.lineTo(w * 0.95, h * 0.5); ctx.stroke();
      // Vertical center line
      ctx.beginPath(); ctx.moveTo(w / 2, h * 0.1); ctx.lineTo(w / 2, h * 0.92); ctx.stroke();
      // Flat top face
      ctx.fillStyle = '#8a7050';
      this.roundRect(ctx, w * 0.05, h * 0.06, w * 0.9, h * 0.1, s);
      ctx.fill();
    });

    // camp_banner — 18x48: vertical pole, triangular flag hanging from top
    this.makeSprite('camp_banner', 18 * s, 48 * s, (ctx, w, h) => {
      // Pole
      ctx.fillStyle = '#3a2a10';
      ctx.fillRect(w / 2 - s, 0, 2 * s, h);
      // Metal tip
      ctx.fillStyle = '#909090';
      ctx.beginPath();
      ctx.moveTo(w / 2, 0);
      ctx.lineTo(w / 2 - 1.5 * s, 3 * s);
      ctx.lineTo(w / 2 + 1.5 * s, 3 * s);
      ctx.closePath(); ctx.fill();
      // Flag / banner (triangular, hangs from top)
      ctx.fillStyle = '#2a6a1a';
      ctx.beginPath();
      ctx.moveTo(w / 2 + s, h * 0.06);
      ctx.lineTo(w * 0.92, h * 0.38);
      ctx.lineTo(w / 2 + s, h * 0.38);
      ctx.closePath(); ctx.fill();
      // Banner highlight
      ctx.fillStyle = 'rgba(255,255,255,0.12)';
      ctx.beginPath();
      ctx.moveTo(w / 2 + s, h * 0.06);
      ctx.lineTo(w * 0.75, h * 0.22);
      ctx.lineTo(w / 2 + s, h * 0.22);
      ctx.closePath(); ctx.fill();
    });

    // camp_well — 44x36: stone ring base, dark inner hole, two posts, crossbar
    this.makeSprite('camp_well', 44 * s, 36 * s, (ctx, w, h) => {
      // Shadow
      ctx.fillStyle = 'rgba(0,0,0,0.2)';
      this.fillEllipse(ctx, w / 2, h - 1.5 * s, 16 * s, 4 * s);
      // Stone ring base
      ctx.fillStyle = '#585860';
      this.fillEllipse(ctx, w / 2, h * 0.62, 11 * s, 5 * s);
      // Inner dark hole
      ctx.fillStyle = '#0a0a12';
      this.fillEllipse(ctx, w / 2, h * 0.60, 7 * s, 3 * s);
      // Subtle water sheen inside
      ctx.fillStyle = 'rgba(30,70,120,0.25)';
      this.fillEllipse(ctx, w / 2, h * 0.60, 5 * s, 2 * s);
      // Left post
      ctx.fillStyle = '#3a2a10';
      ctx.fillRect(w * 0.18, h * 0.15, 2.5 * s, h * 0.5);
      // Right post
      ctx.fillRect(w * 0.72, h * 0.15, 2.5 * s, h * 0.5);
      // Crossbar
      ctx.fillStyle = '#4a3a18';
      ctx.fillRect(w * 0.14, h * 0.12, w * 0.72, 2 * s);
      // Rope
      ctx.strokeStyle = '#8a7040';
      ctx.lineWidth = s;
      ctx.beginPath();
      ctx.moveTo(w / 2, h * 0.14); ctx.lineTo(w / 2, h * 0.58);
      ctx.stroke();
      // Bucket
      ctx.fillStyle = '#4a3820';
      this.roundRect(ctx, w / 2 - 2 * s, h * 0.5, 4 * s, 4 * s, s);
      ctx.fill();
    });
  }

  private generateEffects(): void {
    const s = TEXTURE_SCALE;

    // Loot bag
    const lbW = 24 * s, lbH = 24 * s;
    const [lbCanvas, lbCtx] = this.createCanvas(lbW, lbH);
    lbCtx.fillStyle = 'rgba(0,0,0,0.2)';
    this.fillEllipse(lbCtx, lbW / 2, lbH - 2 * s, 8 * s, 2.5 * s);
    this.drawPart(lbCtx, lbW / 2 - 6 * s, 4 * s, 12 * s, 14 * s, 0x4a3020, 2 * s);
    this.drawPart(lbCtx, lbW / 2 - 5 * s, 2 * s, 10 * s, 4 * s, 0x5a4030, 2 * s);
    lbCtx.fillStyle = this.rgb(0x3a2010);
    lbCtx.fillRect(lbW / 2 - 5.5 * s, 6 * s, 11 * s, 2 * s);
    lbCtx.fillStyle = this.rgb(0x8a7020, 0.8);
    this.fillCircle(lbCtx, lbW / 2, 12 * s, 2.5 * s);
    if (this.scene.textures.exists('loot_bag')) this.scene.textures.remove('loot_bag');
    this.scene.textures.addCanvas('loot_bag', lbCanvas);

    // Exit portal
    const pW = 32 * s, pH = 32 * s;
    const [pCanvas, pCtx] = this.createCanvas(pW, pH);
    const pGrad = pCtx.createRadialGradient(pW / 2, pH / 2, 0, pW / 2, pH / 2, pW * 0.45);
    pGrad.addColorStop(0, 'rgba(255,255,255,0.3)');
    pGrad.addColorStop(0.3, 'rgba(100,220,140,0.5)');
    pGrad.addColorStop(0.6, 'rgba(0,180,80,0.3)');
    pGrad.addColorStop(1, 'rgba(0,80,40,0)');
    pCtx.fillStyle = pGrad;
    this.fillCircle(pCtx, pW / 2, pH / 2, pW * 0.45);
    pCtx.strokeStyle = 'rgba(0,220,100,0.4)';
    pCtx.lineWidth = 1.5 * s;
    pCtx.beginPath(); pCtx.arc(pW / 2, pH / 2, pW * 0.35, 0, Math.PI * 2); pCtx.stroke();
    if (this.scene.textures.exists('exit_portal')) this.scene.textures.remove('exit_portal');
    this.scene.textures.addCanvas('exit_portal', pCanvas);
  }

  // ═══════════════════════════════════════════════════════════════════════
  // ██ ANIMATION REGISTRATION ██
  // ═══════════════════════════════════════════════════════════════════════

  private registerAnimations(): void {
    const anims = this.scene.anims;

    const registerSet = (key: string, isPlayer: boolean) => {
      const defs: [string, number, number, number, number][] = [
        ['idle', IDLE_START, IDLE_COUNT, 6, -1],
        ['walk', WALK_START, WALK_COUNT, 10, -1],
        ['attack', ATK_START, ATK_COUNT, 12, 0],
        ['hurt', HURT_START, HURT_COUNT, 10, 0],
        ['death', DEATH_START, DEATH_COUNT, 6, 0],
      ];
      if (isPlayer) {
        defs.push(['cast', CAST_START, CAST_COUNT, 8, 0]);
      }

      for (const [action, start, count, rate, repeat] of defs) {
        const animKey = `${key}_${action}`;
        if (anims.exists(animKey)) anims.remove(animKey);
        anims.create({
          key: animKey,
          frames: anims.generateFrameNumbers(key, { start, end: start + count - 1 }),
          frameRate: rate,
          repeat,
        });
      }
    };

    for (const cfg of PLAYER_CONFIGS) {
      registerSet(cfg.textureKey, true);
    }
    for (const cfg of MONSTER_CONFIGS) {
      registerSet(cfg.textureKey, false);
    }
    // NPC idle animations (8 frames, varied frameRate per accessory)
    for (const npc of NPC_CONFIGS) {
      const animKey = `${npc.key}_idle`;
      if (anims.exists(animKey)) anims.remove(animKey);
      // Hammering/pickaxe: faster; staff/book: slower; others: moderate
      let rate = 5;
      if (npc.accessory === 'hammer' || npc.accessory === 'pickaxe') rate = 6;
      else if (npc.accessory === 'staff' || npc.accessory === 'book') rate = 3;
      else if (npc.accessory === 'lantern') rate = 4;
      anims.create({
        key: animKey,
        frames: anims.generateFrameNumbers(npc.key, { start: 0, end: 7 }),
        frameRate: rate,
        repeat: -1,
      });
    }
  }
}
