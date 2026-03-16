import Phaser from 'phaser';
import { TEXTURE_SCALE } from '../config';
import { CAMP_THEMES } from '../data/camp-themes';
import { DrawUtils } from './DrawUtils';
import type { EntityDrawer } from './sprites/types';
import { SlimeDrawer } from './sprites/monsters/Slime';
import { SkeletonDrawer } from './sprites/monsters/Skeleton';
import { WerewolfDrawer } from './sprites/monsters/Werewolf';
import { FireElementalDrawer } from './sprites/monsters/FireElemental';
import { DesertScorpionDrawer } from './sprites/monsters/DesertScorpion';
import { GoblinDrawer } from './sprites/monsters/Goblin';
import { GoblinChiefDrawer } from './sprites/monsters/GoblinChief';
import { ZombieDrawer } from './sprites/monsters/Zombie';
import { WerewolfAlphaDrawer } from './sprites/monsters/WerewolfAlpha';
import { GargoyleDrawer } from './sprites/monsters/Gargoyle';
import { StoneGolemDrawer } from './sprites/monsters/StoneGolem';
import { MountainTrollDrawer } from './sprites/monsters/MountainTroll';
import { SandwormDrawer } from './sprites/monsters/Sandworm';
import { PhoenixDrawer } from './sprites/monsters/Phoenix';
import { ImpDrawer } from './sprites/monsters/Imp';
import { LesserDemonDrawer } from './sprites/monsters/LesserDemon';
import { SuccubusDrawer } from './sprites/monsters/Succubus';
import { DemonLordDrawer } from './sprites/monsters/DemonLord';
import { PlayerWarriorDrawer } from './sprites/players/PlayerWarrior';
import { PlayerMageDrawer } from './sprites/players/PlayerMage';
import { PlayerRogueDrawer } from './sprites/players/PlayerRogue';
import { BlacksmithDrawer } from './sprites/npcs/Blacksmith';
import { BlacksmithAdvancedDrawer } from './sprites/npcs/BlacksmithAdvanced';
import { MerchantDrawer } from './sprites/npcs/Merchant';
import { MerchantDesertDrawer } from './sprites/npcs/MerchantDesert';
import { StashDrawer } from './sprites/npcs/Stash';
import { QuestElderDrawer } from './sprites/npcs/QuestElder';
import { QuestScoutDrawer } from './sprites/npcs/QuestScout';
import { ForestHermitDrawer } from './sprites/npcs/ForestHermit';
import { QuestDwarfDrawer } from './sprites/npcs/QuestDwarf';
import { QuestNomadDrawer } from './sprites/npcs/QuestNomad';
import { QuestWardenDrawer } from './sprites/npcs/QuestWarden';
import { TreeDrawer } from './sprites/decorations/Tree';
import { BushDrawer } from './sprites/decorations/Bush';
import { RockDrawer } from './sprites/decorations/Rock';
import { FlowerDrawer } from './sprites/decorations/Flower';
import { MushroomDrawer } from './sprites/decorations/Mushroom';
import { CactusDrawer } from './sprites/decorations/Cactus';
import { BoulderDrawer } from './sprites/decorations/Boulder';
import { CrystalDrawer } from './sprites/decorations/Crystal';
import { BonesDrawer } from './sprites/decorations/Bones';
import { LootBagDrawer } from './sprites/effects/LootBag';
import { ExitPortalDrawer } from './sprites/effects/ExitPortal';

// ── Frame Layout Constants ──────────────────────────────────────────────────
const IDLE_START = 0, IDLE_COUNT = 4;
const WALK_START = 4, WALK_COUNT = 6;
const ATK_START = 10, ATK_COUNT = 4;
const HURT_START = 14, HURT_COUNT = 2;
const DEATH_START = 16, DEATH_COUNT = 4;
const CAST_START = 20, CAST_COUNT = 4;
const MONSTER_FRAMES = 20;
const PLAYER_FRAMES = 24;

// NPC frame layout (24 frames total per NPC)
const NPC_FW = 80;   // frame width (before TEXTURE_SCALE)
const NPC_FH = 120;  // frame height (before TEXTURE_SCALE)
const NPC_WORK_START = 0, NPC_WORK_COUNT = 8;
const NPC_ALERT_START = 8, NPC_ALERT_COUNT = 4;
const NPC_IDLE_START = 12, NPC_IDLE_COUNT = 6;
const NPC_TALK_START = 18, NPC_TALK_COUNT = 6;
const NPC_TOTAL_FRAMES = 24;

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
  private utils: DrawUtils;

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
    this.utils = new DrawUtils();
  }

  /** Returns true if the texture was loaded from an external image file (HTMLImageElement),
   *  meaning procedural generation should be skipped in favour of the loaded asset. */
  private shouldSkipGeneration(key: string): boolean {
    if (!this.scene.textures.exists(key)) return false;
    const tex = this.scene.textures.get(key);
    return tex.source[0]?.source instanceof HTMLImageElement;
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

  private hash2d(x: number, y: number): number { return this.utils.hash2d(x, y); }
  private noise2d(x: number, y: number): number { return this.utils.noise2d(x, y); }
  private fbm(x: number, y: number, octaves: number): number { return this.utils.fbm(x, y, octaves); }
  private clamp(v: number): number { return this.utils.clamp(v); }
  private rgb(c: number, alpha?: number): string { return this.utils.rgb(c, alpha); }
  private lerp(a: number, b: number, t: number): number { return this.utils.lerp(a, b, t); }
  private createCanvas(w: number, h: number): [HTMLCanvasElement, CanvasRenderingContext2D] { return this.utils.createCanvas(w, h); }
  private roundRect(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, r: number): void { return this.utils.roundRect(ctx, x, y, w, h, r); }
  private fillEllipse(ctx: CanvasRenderingContext2D, cx: number, cy: number, rx: number, ry: number): void { return this.utils.fillEllipse(ctx, cx, cy, rx, ry); }
  private fillCircle(ctx: CanvasRenderingContext2D, cx: number, cy: number, r: number): void { return this.utils.fillCircle(ctx, cx, cy, r); }
  private drawPart(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, color: number, radius: number = 0): void { return this.utils.drawPart(ctx, x, y, w, h, color, radius); }
  private applyNoiseToRegion(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, intensity: number): void { return this.utils.applyNoiseToRegion(ctx, x, y, w, h, intensity); }

  private clipDiamond(ctx: CanvasRenderingContext2D, w: number, h: number): void {
    ctx.beginPath();
    ctx.moveTo(w / 2, 0);
    ctx.lineTo(w, h / 2);
    ctx.lineTo(w / 2, h);
    ctx.lineTo(0, h / 2);
    ctx.closePath();
    ctx.clip();
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
    this.generateFromDrawer(PlayerWarriorDrawer);
    this.generateFromDrawer(PlayerMageDrawer);
    this.generateFromDrawer(PlayerRogueDrawer);
    // Existing template fallback
    for (const cfg of PLAYER_CONFIGS) {
      if (!this.scene.textures.exists(cfg.textureKey)) {
        this.makeCharSheet(cfg, PLAYER_FRAMES);
      }
    }
  }

  private generateFromDrawer(drawer: EntityDrawer): void {
    if (this.shouldSkipGeneration(drawer.key)) return;

    const s = TEXTURE_SCALE;
    const fw = drawer.frameW * s, fh = drawer.frameH * s;
    const [canvas, ctx] = this.utils.createCanvas(fw * drawer.totalFrames, fh);

    const actions: [string, number, number][] = [
      ['idle', IDLE_START, IDLE_COUNT],
      ['walk', WALK_START, WALK_COUNT],
      ['attack', ATK_START, ATK_COUNT],
      ['hurt', HURT_START, HURT_COUNT],
      ['death', DEATH_START, DEATH_COUNT],
    ];
    if (drawer.totalFrames > MONSTER_FRAMES) {
      actions.push(['cast', CAST_START, CAST_COUNT]);
    }

    for (const [action, start, count] of actions) {
      for (let f = 0; f < count; f++) {
        const ox = (start + f) * fw;
        ctx.save();
        ctx.translate(ox, 0);
        drawer.drawFrame(ctx, f, action as any, fw, fh, this.utils);
        ctx.restore();
      }
    }

    this.utils.applyNoiseToRegion(ctx, 0, 0, canvas.width, canvas.height, 4);

    const key = drawer.key;
    if (this.scene.textures.exists(key)) this.scene.textures.remove(key);
    const canvasTex = this.scene.textures.addCanvas(key, canvas)!;
    for (let i = 0; i < drawer.totalFrames; i++) {
      canvasTex.add(i, 0, i * fw, 0, fw, fh);
    }
  }

  private generateMonsterSheets(): void {
    // New per-entity drawers
    this.generateFromDrawer(SlimeDrawer);
    this.generateFromDrawer(SkeletonDrawer);
    this.generateFromDrawer(WerewolfDrawer);
    this.generateFromDrawer(FireElementalDrawer);
    this.generateFromDrawer(DesertScorpionDrawer);
    this.generateFromDrawer(GoblinDrawer);
    this.generateFromDrawer(GoblinChiefDrawer);
    this.generateFromDrawer(ZombieDrawer);
    this.generateFromDrawer(WerewolfAlphaDrawer);
    this.generateFromDrawer(GargoyleDrawer);
    this.generateFromDrawer(StoneGolemDrawer);
    this.generateFromDrawer(MountainTrollDrawer);
    this.generateFromDrawer(SandwormDrawer);
    this.generateFromDrawer(PhoenixDrawer);
    this.generateFromDrawer(ImpDrawer);
    this.generateFromDrawer(LesserDemonDrawer);
    this.generateFromDrawer(SuccubusDrawer);
    this.generateFromDrawer(DemonLordDrawer);

    // Existing template-based generation (skip if drawer already handled it)
    for (const cfg of MONSTER_CONFIGS) {
      if (!this.scene.textures.exists(cfg.textureKey)) {
        this.makeCharSheet(cfg, MONSTER_FRAMES);
      }
    }
  }

  private makeCharSheet(cfg: SpriteConfig, totalFrames: number): void {
    if (this.shouldSkipGeneration(cfg.textureKey)) return;
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

  private generateFromNPCDrawer(drawer: EntityDrawer): void {
    if (this.shouldSkipGeneration(drawer.key)) return;

    const s = TEXTURE_SCALE;
    const fw = drawer.frameW * s, fh = drawer.frameH * s;
    const [canvas, ctx] = this.utils.createCanvas(fw * drawer.totalFrames, fh);

    const actions: [string, number, number][] = [
      ['working', NPC_WORK_START, NPC_WORK_COUNT],
      ['alert', NPC_ALERT_START, NPC_ALERT_COUNT],
      ['idle', NPC_IDLE_START, NPC_IDLE_COUNT],
      ['talking', NPC_TALK_START, NPC_TALK_COUNT],
    ];

    for (const [action, start, count] of actions) {
      for (let f = 0; f < count; f++) {
        const ox = (start + f) * fw;
        ctx.save();
        ctx.translate(ox, 0);
        drawer.drawFrame(ctx, f, action as any, fw, fh, this.utils);
        ctx.restore();
      }
    }

    this.utils.applyNoiseToRegion(ctx, 0, 0, canvas.width, canvas.height, 3);

    const key = drawer.key;
    if (this.scene.textures.exists(key)) this.scene.textures.remove(key);
    const canvasTex = this.scene.textures.addCanvas(key, canvas)!;
    for (let i = 0; i < drawer.totalFrames; i++) {
      canvasTex.add(i, 0, i * fw, 0, fw, fh);
    }
  }

  private generateNPCSprites(): void {
    const npcDrawers = [
      BlacksmithDrawer, BlacksmithAdvancedDrawer, MerchantDrawer,
      MerchantDesertDrawer, StashDrawer, QuestElderDrawer,
      QuestScoutDrawer, ForestHermitDrawer, QuestDwarfDrawer,
      QuestNomadDrawer, QuestWardenDrawer,
    ];
    for (const drawer of npcDrawers) {
      this.generateFromNPCDrawer(drawer);
    }
    // Template fallback for any NPC not covered by a custom drawer
    for (const npc of NPC_CONFIGS) {
      if (!this.scene.textures.exists(npc.key)) {
        this.makeNPCSheet(npc);
      }
    }
  }

  /** Generate a single-frame static texture from an EntityDrawer (decorations, effects). */
  private generateFromStaticDrawer(drawer: EntityDrawer): void {
    if (this.shouldSkipGeneration(drawer.key)) return;

    const s = TEXTURE_SCALE;
    const w = drawer.frameW * s, h = drawer.frameH * s;
    const [canvas, ctx] = this.utils.createCanvas(w, h);

    drawer.drawFrame(ctx, 0, 'idle', w, h, this.utils);

    if (this.scene.textures.exists(drawer.key)) this.scene.textures.remove(drawer.key);
    this.scene.textures.addCanvas(drawer.key, canvas);
  }

  private makeNPCSheet(npc: NPCConfig): void {
    if (this.shouldSkipGeneration(npc.key)) return;
    const s = TEXTURE_SCALE;
    const fw = NPC_FW * s, fh = NPC_FH * s;
    const [canvas, ctx] = this.createCanvas(fw * NPC_TOTAL_FRAMES, fh);
    const skin = npc.skinColor ?? 0xb08960;
    const hair = npc.hairColor ?? 0x3a2a1a;
    const acc = npc.accessory ?? 'none';
    const bw = npc.bulky ? 1.15 : 1;

    for (let f = 0; f < NPC_TOTAL_FRAMES; f++) {
      const ox = f * fw;
      ctx.save();
      ctx.translate(ox, 0);

      let state: 'working' | 'alert' | 'idle' | 'talking';
      let stateFrame: number;
      let stateCount: number;
      if (f < NPC_ALERT_START) {
        state = 'working'; stateFrame = f - NPC_WORK_START; stateCount = NPC_WORK_COUNT;
      } else if (f < NPC_IDLE_START) {
        state = 'alert'; stateFrame = f - NPC_ALERT_START; stateCount = NPC_ALERT_COUNT;
      } else if (f < NPC_TALK_START) {
        state = 'idle'; stateFrame = f - NPC_IDLE_START; stateCount = NPC_IDLE_COUNT;
      } else {
        state = 'talking'; stateFrame = f - NPC_TALK_START; stateCount = NPC_TALK_COUNT;
      }

      const phase = (stateFrame / stateCount) * Math.PI * 2;
      const pose = this.calcNPCPose(state, phase, acc);
      this.drawNPCFrame(ctx, fw, fh, s, npc, skin, hair, bw, pose);

      ctx.restore();
    }

    this.applyNoiseToRegion(ctx, 0, 0, canvas.width, canvas.height, 3);

    const key = npc.key;
    if (this.scene.textures.exists(key)) this.scene.textures.remove(key);
    const canvasTex = this.scene.textures.addCanvas(key, canvas)!;
    for (let i = 0; i < NPC_TOTAL_FRAMES; i++) {
      canvasTex.add(i, 0, i * fw, 0, fw, fh);
    }
  }

  private calcNPCPose(
    state: 'working' | 'alert' | 'idle' | 'talking',
    phase: number,
    acc: string,
  ): {
    bob: number; leftArmY: number; rightArmY: number; headTilt: number;
    bodyLean: number; mouthOpen: boolean; eyebrowRaise: number;
    gestureHand: 'none' | 'left' | 'right';
  } {
    let bob = 0;
    let leftArmY = 0;
    let rightArmY = 0;
    let headTilt = 0;
    let bodyLean = 0;
    let mouthOpen = false;
    let eyebrowRaise = 0;
    let gestureHand: 'none' | 'left' | 'right' = 'none';

    switch (state) {
      case 'working': {
        // Role-specific working animations
        switch (acc) {
          case 'hammer': {
            bob = Math.sin(phase) * 0.5;
            const strike = Math.sin(phase);
            rightArmY = strike > 0 ? -strike * 12 : strike * 5;
            leftArmY = Math.sin(phase + 0.5) * 2;
            bodyLean = strike > 0 ? -0.8 : 0.6;
            headTilt = strike * 0.04;
            break;
          }
          case 'pickaxe': {
            bob = Math.sin(phase) * 0.5;
            const dig = Math.sin(phase);
            rightArmY = dig > 0 ? -dig * 14 : dig * 4;
            leftArmY = Math.sin(phase + 0.3) * 2.5;
            bodyLean = dig > 0 ? -1.2 : 1;
            headTilt = dig * 0.06;
            break;
          }
          case 'coinbag': {
            bob = Math.sin(phase) * 1.5;
            rightArmY = Math.sin(phase * 2) * 4;
            leftArmY = Math.sin(phase + 1) * 1.5;
            bodyLean = Math.sin(phase) * 0.6;
            break;
          }
          case 'staff': {
            bob = Math.sin(phase) * 0.8;
            rightArmY = Math.sin(phase) * 1.5;
            leftArmY = Math.sin(phase + 2) * 2.5;
            bodyLean = Math.sin(phase) * 1;
            headTilt = Math.sin(phase + 1) * 0.04;
            break;
          }
          case 'sword': {
            bob = Math.sin(phase) * 0.5;
            rightArmY = Math.sin(phase) * 1.5;
            leftArmY = Math.sin(phase + Math.PI) * 2.5;
            bodyLean = Math.sin(phase) * 0.4;
            headTilt = Math.sin(phase * 2) * 0.03;
            break;
          }
          case 'lantern': {
            bob = Math.sin(phase) * 1;
            rightArmY = Math.sin(phase) * 5;
            leftArmY = Math.sin(phase + 1.5) * 2;
            bodyLean = Math.sin(phase + 0.5) * 0.7;
            break;
          }
          case 'book': {
            bob = Math.sin(phase) * 0.6;
            rightArmY = Math.sin(phase * 2) * 2;
            leftArmY = Math.sin(phase) * 0.8;
            headTilt = 0.07 + Math.sin(phase) * 0.03;
            break;
          }
          default: {
            bob = Math.sin(phase) * 1;
            leftArmY = Math.sin(phase) * 2.5;
            rightArmY = Math.sin(phase + Math.PI) * 2.5;
            break;
          }
        }
        break;
      }
      case 'alert': {
        // Body straightens, head turns toward camera, eyebrows raised
        const t = Math.sin(phase * 0.5); // slow transition
        bob = t * 0.3;
        bodyLean = 0;
        headTilt = t * -0.02; // slight head turn
        eyebrowRaise = 2 + t * 1.5;
        rightArmY = -1;
        leftArmY = -1;
        break;
      }
      case 'idle': {
        // Gentle breathing, weight shift
        bob = Math.sin(phase) * 0.8;
        bodyLean = Math.sin(phase * 0.5) * 0.3;
        leftArmY = Math.sin(phase) * 1.5;
        rightArmY = Math.sin(phase + Math.PI) * 1.5;
        headTilt = Math.sin(phase + 1) * 0.015;
        break;
      }
      case 'talking': {
        // Left hand gestures, mouth opens, slight lean forward
        bob = Math.sin(phase) * 0.5;
        bodyLean = -0.4 + Math.sin(phase) * 0.3;
        leftArmY = Math.sin(phase * 2) * 4;
        rightArmY = Math.sin(phase) * 1;
        headTilt = Math.sin(phase * 1.5) * 0.03;
        mouthOpen = Math.sin(phase * 3) > 0.2;
        gestureHand = 'left';
        eyebrowRaise = Math.sin(phase * 2) > 0 ? 1.5 : 0;
        break;
      }
    }

    return { bob, leftArmY, rightArmY, headTilt, bodyLean, mouthOpen, eyebrowRaise, gestureHand };
  }

  private drawNPCFrame(
    ctx: CanvasRenderingContext2D,
    fw: number, fh: number, s: number,
    npc: NPCConfig,
    skin: number, hair: number, bw: number,
    pose: {
      bob: number; leftArmY: number; rightArmY: number; headTilt: number;
      bodyLean: number; mouthOpen: boolean; eyebrowRaise: number;
      gestureHand: 'none' | 'left' | 'right';
    },
  ): void {
    const acc = npc.accessory ?? 'none';
    const cx = fw / 2;
    const ground = fh - 8 * s;
    const by = ground + pose.bob * s;
    const bodyW = Math.round(16 * bw);

    // ── 1. Shadow ──
    ctx.fillStyle = 'rgba(0,0,0,0.25)';
    this.fillEllipse(ctx, cx, ground + 3 * s, 18 * s, 5 * s);

    // ── 2. Cloak (behind body) ──
    if (npc.cloakColor) {
      this.drawPart(ctx, cx - (bodyW + 3) * s + pose.bodyLean * s, by - 52 * s, (bodyW * 2 + 6) * s, 40 * s, npc.cloakColor, 4 * s);
      // Cloak bottom drape
      ctx.fillStyle = this.rgb(darkenHex(npc.cloakColor, 10));
      this.roundRect(ctx, cx - (bodyW + 2) * s + pose.bodyLean * s, by - 14 * s, (bodyW * 2 + 4) * s, 6 * s, 2 * s);
      ctx.fill();
    }

    // ── 3. Legs + boots ──
    const legColor = darkenHex(npc.bodyColor, 15);
    const bootColor = 0x2a1a0a;
    // Left leg
    this.drawPart(ctx, cx - 9 * s, by - 22 * s, 7 * s, 20 * s, legColor, 3 * s);
    // Right leg
    this.drawPart(ctx, cx + 2 * s, by - 22 * s, 7 * s, 20 * s, legColor, 3 * s);
    // Left boot
    this.drawPart(ctx, cx - 10 * s, by - 4 * s, 9 * s, 6 * s, bootColor, 2 * s);
    // Boot sole
    ctx.fillStyle = this.rgb(darkenHex(bootColor, 15));
    ctx.fillRect(cx - 10 * s, by + 1 * s, 9 * s, 1.5 * s);
    // Right boot
    this.drawPart(ctx, cx + 1 * s, by - 4 * s, 9 * s, 6 * s, bootColor, 2 * s);
    // Boot sole
    ctx.fillStyle = this.rgb(darkenHex(bootColor, 15));
    ctx.fillRect(cx + 1 * s, by + 1 * s, 9 * s, 1.5 * s);

    // ── 4. Body torso ──
    const bodyTop = by - 48 * s;
    const bodyH = 28 * s;
    // Body with gradient shading
    const bodyX = cx - bodyW * s + pose.bodyLean * s;
    this.drawPart(ctx, bodyX, bodyTop, bodyW * 2 * s, bodyH, npc.bodyColor, 4 * s);
    // Body outline
    ctx.strokeStyle = this.rgb(darkenHex(npc.bodyColor, 30));
    ctx.lineWidth = 1;
    this.roundRect(ctx, bodyX, bodyTop, bodyW * 2 * s, bodyH, 4 * s);
    ctx.stroke();
    // Collar detail
    ctx.fillStyle = this.rgb(lightenHex(npc.bodyColor, 20));
    this.roundRect(ctx, cx - 6 * s + pose.bodyLean * s, bodyTop, 12 * s, 4 * s, 2 * s);
    ctx.fill();

    // ── 5. Belt + buckle ──
    const beltY = by - 22 * s;
    this.drawPart(ctx, cx - bodyW * s + pose.bodyLean * s, beltY, bodyW * 2 * s, 4 * s, darkenHex(npc.bodyColor, 25), 1 * s);
    // Belt buckle
    ctx.fillStyle = this.rgb(0x8a7a40);
    this.roundRect(ctx, cx - 2 * s + pose.bodyLean * s, beltY + 0.5 * s, 4 * s, 3 * s, 1 * s);
    ctx.fill();

    // ── 6. Left arm + hand ──
    const laX = cx - (bodyW + 5) * s + pose.bodyLean * s;
    const laY = by - 46 * s;
    const laH = 16 * s;
    // Arm with gradient and outline
    this.drawPart(ctx, laX, laY, 6 * s, laH, npc.bodyColor, 3 * s);
    ctx.strokeStyle = this.rgb(darkenHex(npc.bodyColor, 25));
    ctx.lineWidth = 0.8;
    this.roundRect(ctx, laX, laY, 6 * s, laH, 3 * s);
    ctx.stroke();
    // Left hand
    const lhX = laX + 3 * s;
    const lhY = laY + laH + pose.leftArmY * s;
    ctx.fillStyle = this.rgb(skin);
    this.fillCircle(ctx, lhX, lhY, 3 * s);
    // Hand outline
    ctx.strokeStyle = this.rgb(darkenHex(skin, 30));
    ctx.lineWidth = 0.6;
    ctx.beginPath();
    ctx.arc(lhX, lhY, 3 * s, 0, Math.PI * 2);
    ctx.stroke();

    // ── 7. Right arm + hand ──
    const raX = cx + (bodyW - 1) * s + pose.bodyLean * s;
    const raY = by - 46 * s;
    const raH = 16 * s;
    this.drawPart(ctx, raX, raY, 6 * s, raH, npc.bodyColor, 3 * s);
    ctx.strokeStyle = this.rgb(darkenHex(npc.bodyColor, 25));
    ctx.lineWidth = 0.8;
    this.roundRect(ctx, raX, raY, 6 * s, raH, 3 * s);
    ctx.stroke();
    // Right hand
    const rhX = raX + 3 * s;
    const rhY = raY + raH + pose.rightArmY * s;
    ctx.fillStyle = this.rgb(skin);
    this.fillCircle(ctx, rhX, rhY, 3 * s);
    ctx.strokeStyle = this.rgb(darkenHex(skin, 30));
    ctx.lineWidth = 0.6;
    ctx.beginPath();
    ctx.arc(rhX, rhY, 3 * s, 0, Math.PI * 2);
    ctx.stroke();

    // ── 8. Accessory in right hand ──
    this.drawNPCAccessory(ctx, acc, npc, rhX, rhY, s, pose);

    // ── 9. Neck ──
    ctx.fillStyle = this.rgb(skin);
    this.roundRect(ctx, cx - 4 * s + pose.bodyLean * s, by - 54 * s, 8 * s, 8 * s, 2 * s);
    ctx.fill();

    // ── 10. Head with tilt rotation ──
    const headCX = cx + pose.bodyLean * s;
    const headCY = by - 58 * s;
    const headR = 10 * s;
    ctx.save();
    ctx.translate(headCX, headCY);
    ctx.rotate(pose.headTilt);

    // Head shape (rounded rect with outline)
    ctx.fillStyle = this.rgb(skin);
    this.roundRect(ctx, -headR, -headR, headR * 2, headR * 2, 6 * s);
    ctx.fill();
    // Head outline
    ctx.strokeStyle = this.rgb(darkenHex(skin, 25));
    ctx.lineWidth = 1;
    this.roundRect(ctx, -headR, -headR, headR * 2, headR * 2, 6 * s);
    ctx.stroke();
    // Jaw definition
    ctx.fillStyle = this.rgb(darkenHex(skin, 8));
    this.roundRect(ctx, -7 * s, 3 * s, 14 * s, 5 * s, 3 * s);
    ctx.fill();

    // ── 11. Eyes ──
    const eyeY = -1 * s;
    // Eye whites
    ctx.fillStyle = '#e8e4e0';
    this.fillEllipse(ctx, -4 * s, eyeY, 2.5 * s, 2 * s);
    this.fillEllipse(ctx, 4 * s, eyeY, 2.5 * s, 2 * s);
    // Iris
    ctx.fillStyle = '#2a2420';
    this.fillEllipse(ctx, -4 * s, eyeY, 1.5 * s, 1.8 * s);
    this.fillEllipse(ctx, 4 * s, eyeY, 1.5 * s, 1.8 * s);
    // Highlight dot
    ctx.fillStyle = '#ffffff';
    this.fillCircle(ctx, -3.2 * s, eyeY - 0.5 * s, 0.6 * s);
    this.fillCircle(ctx, 4.8 * s, eyeY - 0.5 * s, 0.6 * s);

    // ── 12. Eyebrows, nose, mouth ──
    // Eyebrows
    const browY = eyeY - 3.5 * s - pose.eyebrowRaise * s;
    ctx.strokeStyle = this.rgb(darkenHex(hair, 10));
    ctx.lineWidth = 1.5 * s;
    ctx.beginPath();
    ctx.moveTo(-6 * s, browY); ctx.lineTo(-2 * s, browY - 0.5 * s);
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(2 * s, browY - 0.5 * s); ctx.lineTo(6 * s, browY);
    ctx.stroke();

    // Nose
    ctx.fillStyle = this.rgb(darkenHex(skin, 12));
    ctx.beginPath();
    ctx.moveTo(0, 0);
    ctx.lineTo(-1.5 * s, 3 * s);
    ctx.lineTo(1.5 * s, 3 * s);
    ctx.closePath();
    ctx.fill();

    // Mouth
    if (pose.mouthOpen) {
      ctx.fillStyle = '#3a1a1a';
      this.fillEllipse(ctx, 0, 5.5 * s, 2.5 * s, 1.5 * s);
    } else {
      ctx.strokeStyle = this.rgb(darkenHex(skin, 20));
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(-2.5 * s, 5 * s);
      ctx.quadraticCurveTo(0, 6.5 * s, 2.5 * s, 5 * s);
      ctx.stroke();
    }

    // ── 13. Hair + hat ──
    const hs = npc.hairStyle ?? 'none';
    ctx.fillStyle = this.rgb(hair);
    if (hs === 'short') {
      this.roundRect(ctx, -headR, -headR - 2 * s, headR * 2, 10 * s, 4 * s);
      ctx.fill();
      // Side tufts
      ctx.fillRect(-headR - 1 * s, -headR + 2 * s, 3 * s, 5 * s);
      ctx.fillRect(headR - 2 * s, -headR + 2 * s, 3 * s, 5 * s);
    } else if (hs === 'long') {
      this.roundRect(ctx, -headR - 1 * s, -headR - 3 * s, headR * 2 + 2 * s, 12 * s, 4 * s);
      ctx.fill();
      // Side hair falls
      ctx.fillRect(-headR - 2 * s, -5 * s, 4 * s, 16 * s);
      ctx.fillRect(headR - 2 * s, -5 * s, 4 * s, 16 * s);
    } else if (hs === 'hood') {
      ctx.fillStyle = this.rgb(npc.cloakColor ?? hair);
      ctx.beginPath();
      ctx.moveTo(0, -headR - 5 * s);
      ctx.lineTo(-headR - 3 * s, 5 * s);
      ctx.lineTo(headR + 3 * s, 5 * s);
      ctx.closePath();
      ctx.fill();
      // Hood rim
      ctx.fillStyle = this.rgb(darkenHex(npc.cloakColor ?? hair, 18));
      ctx.beginPath();
      ctx.arc(0, 3 * s, headR + 2 * s, Math.PI, 0);
      ctx.closePath();
      ctx.fill();
    }
    // else 'bald' or 'none' — no hair drawn

    // Hat (on top of hair, non-hood only)
    if (hs !== 'hood') {
      this.drawPart(ctx, -headR - 1 * s, -headR - 4 * s, headR * 2 + 2 * s, 7 * s, npc.hatColor, 3 * s);
      // Hat brim
      ctx.fillStyle = this.rgb(darkenHex(npc.hatColor, 12));
      this.roundRect(ctx, -headR - 2 * s, -headR + 2 * s, headR * 2 + 4 * s, 2 * s, 1 * s);
      ctx.fill();
    }

    // ── 14. Beard ──
    if (npc.beard) {
      ctx.fillStyle = this.rgb(hair);
      ctx.beginPath();
      ctx.moveTo(-6 * s, 5 * s);
      ctx.lineTo(6 * s, 5 * s);
      ctx.lineTo(4 * s, 14 * s);
      ctx.lineTo(0, 17 * s);
      ctx.lineTo(-4 * s, 14 * s);
      ctx.closePath();
      ctx.fill();
      // Beard highlight
      ctx.fillStyle = this.rgb(lightenHex(hair, 15), 0.3);
      ctx.beginPath();
      ctx.moveTo(-2 * s, 6 * s);
      ctx.lineTo(2 * s, 6 * s);
      ctx.lineTo(1 * s, 12 * s);
      ctx.lineTo(-1 * s, 12 * s);
      ctx.closePath();
      ctx.fill();
    }

    ctx.restore(); // end head rotation
  }

  private drawNPCAccessory(
    ctx: CanvasRenderingContext2D,
    acc: string,
    npc: NPCConfig,
    rhX: number, rhY: number,
    s: number,
    pose: { bob: number; leftArmY: number; rightArmY: number; headTilt: number; bodyLean: number; mouthOpen: boolean; eyebrowRaise: number; gestureHand: 'none' | 'left' | 'right' },
  ): void {
    ctx.fillStyle = this.rgb(npc.itemColor);
    switch (acc) {
      case 'hammer': {
        // Handle
        ctx.fillStyle = this.rgb(0x4a3018);
        ctx.fillRect(rhX - 1.5 * s, rhY - 10 * s, 3 * s, 18 * s);
        // Hammer head with metallic gradient
        const hhY = rhY - 14 * s;
        const hamGrad = ctx.createLinearGradient(rhX - 4 * s, hhY, rhX + 4 * s, hhY);
        hamGrad.addColorStop(0, this.rgb(lightenHex(npc.itemColor, 30)));
        hamGrad.addColorStop(0.5, this.rgb(npc.itemColor));
        hamGrad.addColorStop(1, this.rgb(darkenHex(npc.itemColor, 20)));
        ctx.fillStyle = hamGrad;
        this.roundRect(ctx, rhX - 5 * s, hhY, 10 * s, 6 * s, 1.5 * s);
        ctx.fill();
        break;
      }
      case 'staff': {
        // Wooden shaft
        ctx.fillStyle = this.rgb(0x4a3018);
        ctx.fillRect(rhX - 1.5 * s, rhY - 22 * s, 3 * s, 28 * s);
        // Glowing orb
        const orbY = rhY - 23 * s;
        const orbGrad = ctx.createRadialGradient(rhX, orbY, 0, rhX, orbY, 4 * s);
        orbGrad.addColorStop(0, this.rgb(lightenHex(npc.itemColor, 60)));
        orbGrad.addColorStop(0.6, this.rgb(npc.itemColor));
        orbGrad.addColorStop(1, this.rgb(darkenHex(npc.itemColor, 20)));
        ctx.fillStyle = orbGrad;
        this.fillCircle(ctx, rhX, orbY, 4 * s);
        // Glow aura
        ctx.fillStyle = this.rgb(npc.itemColor, 0.15);
        this.fillCircle(ctx, rhX, orbY, 7 * s);
        break;
      }
      case 'sword': {
        // Blade with metallic gradient
        const bladeGrad = ctx.createLinearGradient(rhX - 1 * s, rhY - 18 * s, rhX + 3 * s, rhY - 18 * s);
        bladeGrad.addColorStop(0, this.rgb(0xaaaabc));
        bladeGrad.addColorStop(0.4, this.rgb(0xd0d0e0));
        bladeGrad.addColorStop(1, this.rgb(0x8888a0));
        ctx.fillStyle = bladeGrad;
        ctx.fillRect(rhX - 1 * s, rhY - 18 * s, 3 * s, 20 * s);
        // Blade tip
        ctx.beginPath();
        ctx.moveTo(rhX - 1 * s, rhY - 18 * s);
        ctx.lineTo(rhX + 0.5 * s, rhY - 21 * s);
        ctx.lineTo(rhX + 2 * s, rhY - 18 * s);
        ctx.closePath();
        ctx.fill();
        // Crossguard
        ctx.fillStyle = this.rgb(0x5a4020);
        this.roundRect(ctx, rhX - 3.5 * s, rhY, 8 * s, 2.5 * s, 1 * s);
        ctx.fill();
        // Pommel
        ctx.fillStyle = this.rgb(0x8a6a30);
        this.fillCircle(ctx, rhX + 0.5 * s, rhY + 5 * s, 2 * s);
        break;
      }
      case 'coinbag': {
        // Bag body
        ctx.fillStyle = this.rgb(npc.itemColor);
        this.fillCircle(ctx, rhX, rhY - 5 * s, 5 * s);
        // Drawstring
        ctx.strokeStyle = this.rgb(0x8a7020);
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(rhX - 3 * s, rhY - 8 * s);
        ctx.quadraticCurveTo(rhX, rhY - 10 * s, rhX + 3 * s, rhY - 8 * s);
        ctx.stroke();
        // Coin gleam
        ctx.fillStyle = this.rgb(0xd4aa30);
        this.fillCircle(ctx, rhX, rhY - 5 * s, 2.5 * s);
        break;
      }
      case 'lantern': {
        // Top hook
        ctx.fillStyle = this.rgb(0x4a4a50);
        ctx.fillRect(rhX - 1.5 * s, rhY - 10 * s, 3 * s, 3 * s);
        // Lantern body
        ctx.fillStyle = this.rgb(0xffaa30);
        this.roundRect(ctx, rhX - 3.5 * s, rhY - 7 * s, 7 * s, 8 * s, 1.5 * s);
        ctx.fill();
        // Frame bars
        ctx.strokeStyle = this.rgb(0x4a4a50);
        ctx.lineWidth = 0.8;
        ctx.beginPath();
        ctx.moveTo(rhX, rhY - 7 * s); ctx.lineTo(rhX, rhY + 1 * s);
        ctx.stroke();
        // Radial glow
        const glowGrad = ctx.createRadialGradient(rhX, rhY - 3 * s, 0, rhX, rhY - 3 * s, 8 * s);
        glowGrad.addColorStop(0, 'rgba(255,200,50,0.25)');
        glowGrad.addColorStop(1, 'rgba(255,200,50,0)');
        ctx.fillStyle = glowGrad;
        this.fillCircle(ctx, rhX, rhY - 3 * s, 8 * s);
        break;
      }
      case 'book': {
        // Book cover
        ctx.fillStyle = this.rgb(npc.itemColor);
        this.roundRect(ctx, rhX - 4 * s, rhY - 6 * s, 8 * s, 9 * s, 1.5 * s);
        ctx.fill();
        // Spine
        ctx.fillStyle = this.rgb(darkenHex(npc.itemColor, 20));
        ctx.fillRect(rhX - 4 * s, rhY - 6 * s, 2 * s, 9 * s);
        // Pages
        ctx.fillStyle = this.rgb(0xe8e0d0);
        ctx.fillRect(rhX - 1.5 * s, rhY - 5 * s, 5 * s, 7 * s);
        // Page lines
        ctx.strokeStyle = this.rgb(0xc0b8a8);
        ctx.lineWidth = 0.5;
        for (let i = 0; i < 4; i++) {
          const ly = rhY - 4 * s + i * 2 * s;
          ctx.beginPath();
          ctx.moveTo(rhX - 0.5 * s, ly);
          ctx.lineTo(rhX + 3 * s, ly);
          ctx.stroke();
        }
        break;
      }
      case 'pickaxe': {
        // Handle
        ctx.fillStyle = this.rgb(0x4a3018);
        ctx.fillRect(rhX - 1.5 * s, rhY - 12 * s, 3 * s, 20 * s);
        // Pick head
        ctx.fillStyle = this.rgb(npc.itemColor);
        ctx.beginPath();
        ctx.moveTo(rhX - 6 * s, rhY - 12 * s);
        ctx.lineTo(rhX + 6 * s, rhY - 15 * s);
        ctx.lineTo(rhX + 6 * s, rhY - 11 * s);
        ctx.lineTo(rhX - 5 * s, rhY - 9 * s);
        ctx.closePath();
        ctx.fill();
        // Metallic sheen on pick head
        ctx.fillStyle = this.rgb(lightenHex(npc.itemColor, 25), 0.4);
        ctx.beginPath();
        ctx.moveTo(rhX - 3 * s, rhY - 11.5 * s);
        ctx.lineTo(rhX + 5 * s, rhY - 14 * s);
        ctx.lineTo(rhX + 5 * s, rhY - 12.5 * s);
        ctx.lineTo(rhX - 3 * s, rhY - 10.5 * s);
        ctx.closePath();
        ctx.fill();
        break;
      }
      case 'scroll': {
        // Parchment
        ctx.fillStyle = this.rgb(0xd4c8a0);
        this.roundRect(ctx, rhX - 3 * s, rhY - 7 * s, 7 * s, 10 * s, 2 * s);
        ctx.fill();
        // End caps
        ctx.fillStyle = this.rgb(0x8a3020);
        this.roundRect(ctx, rhX - 3.5 * s, rhY - 7 * s, 8 * s, 2 * s, 1 * s);
        ctx.fill();
        this.roundRect(ctx, rhX - 3.5 * s, rhY + 1.5 * s, 8 * s, 2 * s, 1 * s);
        ctx.fill();
        break;
      }
    }
  }

  // ═══════════════════════════════════════════════════════════════════════
  // ██ DECORATIONS & EFFECTS ██
  // ═══════════════════════════════════════════════════════════════════════

  private generateDecorations(): void {
    // Custom drawers — richer art, replace old template implementations
    const decorDrawers = [
      TreeDrawer, BushDrawer, RockDrawer, FlowerDrawer, MushroomDrawer,
      CactusDrawer, BoulderDrawer, CrystalDrawer, BonesDrawer,
    ];
    for (const drawer of decorDrawers) {
      this.generateFromStaticDrawer(drawer);
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
    // Custom drawers — richer art, replace old template implementations
    this.generateFromStaticDrawer(LootBagDrawer);
    this.generateFromStaticDrawer(ExitPortalDrawer);
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
    // NPC state animations (4 per NPC: working, alert, idle, talking)
    for (const npc of NPC_CONFIGS) {
      const key = npc.key;
      const workKey = `${key}_working`;
      if (anims.exists(workKey)) anims.remove(workKey);
      let workRate = 5;
      if (npc.accessory === 'hammer' || npc.accessory === 'pickaxe') workRate = 6;
      else if (npc.accessory === 'staff' || npc.accessory === 'book') workRate = 3;
      else if (npc.accessory === 'lantern') workRate = 4;
      anims.create({ key: workKey, frames: anims.generateFrameNumbers(key, { start: NPC_WORK_START, end: NPC_WORK_START + NPC_WORK_COUNT - 1 }), frameRate: workRate, repeat: -1 });

      const alertKey = `${key}_alert`;
      if (anims.exists(alertKey)) anims.remove(alertKey);
      anims.create({ key: alertKey, frames: anims.generateFrameNumbers(key, { start: NPC_ALERT_START, end: NPC_ALERT_START + NPC_ALERT_COUNT - 1 }), frameRate: 6, repeat: 0 });

      const idleKey = `${key}_idle`;
      if (anims.exists(idleKey)) anims.remove(idleKey);
      anims.create({ key: idleKey, frames: anims.generateFrameNumbers(key, { start: NPC_IDLE_START, end: NPC_IDLE_START + NPC_IDLE_COUNT - 1 }), frameRate: 4, repeat: -1 });

      const talkKey = `${key}_talking`;
      if (anims.exists(talkKey)) anims.remove(talkKey);
      anims.create({ key: talkKey, frames: anims.generateFrameNumbers(key, { start: NPC_TALK_START, end: NPC_TALK_START + NPC_TALK_COUNT - 1 }), frameRate: 5, repeat: -1 });
    }
  }
}
