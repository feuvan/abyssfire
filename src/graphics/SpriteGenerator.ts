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
import { DungeonShadeDrawer } from './sprites/monsters/DungeonShade';
import { DungeonFiendDrawer } from './sprites/monsters/DungeonFiend';
import { DungeonBossDrawer } from './sprites/monsters/DungeonBoss';
import { DungeonMidBossDrawer } from './sprites/monsters/DungeonMidBoss';
import { GoblinShamanDrawer } from './sprites/monsters/GoblinShaman';
import { ShadowWeaverDrawer } from './sprites/monsters/ShadowWeaver';
import { IronGuardianDrawer } from './sprites/monsters/IronGuardian';
import { SandWraithDrawer } from './sprites/monsters/SandWraith';
import { VoidHeraldDrawer } from './sprites/monsters/VoidHerald';
import { SubMineGuardianDrawer } from './sprites/monsters/SubMineGuardian';
import { SubAltarKeeperDrawer } from './sprites/monsters/SubAltarKeeper';
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
import { WanderingMerchantDrawer } from './sprites/npcs/WanderingMerchant';
import { RescueNPCDrawer } from './sprites/npcs/RescueNPC';
import { TreeDrawer } from './sprites/decorations/Tree';
import { BushDrawer } from './sprites/decorations/Bush';
import { RockDrawer } from './sprites/decorations/Rock';
import { FlowerDrawer } from './sprites/decorations/Flower';
import { MushroomDrawer } from './sprites/decorations/Mushroom';
import { CactusDrawer } from './sprites/decorations/Cactus';
import { BoulderDrawer } from './sprites/decorations/Boulder';
import { CrystalDrawer } from './sprites/decorations/Crystal';
import { BonesDrawer } from './sprites/decorations/Bones';
import { RuinsDrawer } from './sprites/decorations/Ruins';
import { SkeletalRemainsDrawer } from './sprites/decorations/SkeletalRemains';
import { AncientStatueDrawer } from './sprites/decorations/AncientStatue';
import { BrokenAltarDrawer } from './sprites/decorations/BrokenAltar';
import { WarBannerDrawer } from './sprites/decorations/WarBanner';
import { CharredTreeDrawer } from './sprites/decorations/CharredTree';
import { CollapsedPillarDrawer } from './sprites/decorations/CollapsedPillar';
import { RitualCircleDrawer } from './sprites/decorations/RitualCircle';
import { FrozenCorpseDrawer } from './sprites/decorations/FrozenCorpse';
import { SandBuriedStructureDrawer } from './sprites/decorations/SandBuriedStructure';
import { TreasureChestDrawer } from './sprites/decorations/TreasureChest';
import { GoldPileDrawer } from './sprites/decorations/GoldPile';
import { LoreScrollDrawer } from './sprites/decorations/LoreScroll';
import { PuzzleStoneDrawer } from './sprites/decorations/PuzzleStone';
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

// NPC frame layout (24 frames total per NPC)
const NPC_WORK_START = 0, NPC_WORK_COUNT = 8;
const NPC_ALERT_START = 8, NPC_ALERT_COUNT = 4;
const NPC_IDLE_START = 12, NPC_IDLE_COUNT = 6;
const NPC_TALK_START = 18, NPC_TALK_COUNT = 6;

const PLAYER_DRAWERS: EntityDrawer[] = [
  PlayerWarriorDrawer,
  PlayerMageDrawer,
  PlayerRogueDrawer,
];

const MONSTER_DRAWERS: EntityDrawer[] = [
  SlimeDrawer,
  SkeletonDrawer,
  WerewolfDrawer,
  FireElementalDrawer,
  DesertScorpionDrawer,
  GoblinDrawer,
  GoblinChiefDrawer,
  ZombieDrawer,
  WerewolfAlphaDrawer,
  GargoyleDrawer,
  StoneGolemDrawer,
  MountainTrollDrawer,
  SandwormDrawer,
  PhoenixDrawer,
  ImpDrawer,
  LesserDemonDrawer,
  SuccubusDrawer,
  DemonLordDrawer,
  DungeonShadeDrawer,
  DungeonFiendDrawer,
  DungeonBossDrawer,
  DungeonMidBossDrawer,
  GoblinShamanDrawer,
  ShadowWeaverDrawer,
  IronGuardianDrawer,
  SandWraithDrawer,
  VoidHeraldDrawer,
  SubMineGuardianDrawer,
  SubAltarKeeperDrawer,
];

const NPC_DRAWERS: EntityDrawer[] = [
  BlacksmithDrawer,
  BlacksmithAdvancedDrawer,
  MerchantDrawer,
  MerchantDesertDrawer,
  StashDrawer,
  QuestElderDrawer,
  QuestScoutDrawer,
  ForestHermitDrawer,
  QuestDwarfDrawer,
  QuestNomadDrawer,
  QuestWardenDrawer,
  WanderingMerchantDrawer,
  RescueNPCDrawer,
];

const NPC_WORK_RATES = new Map<string, number>([
  [BlacksmithDrawer.key, 6],
  [BlacksmithAdvancedDrawer.key, 6],
  [MerchantDrawer.key, 5],
  [MerchantDesertDrawer.key, 5],
  [StashDrawer.key, 3],
  [QuestElderDrawer.key, 3],
  [QuestScoutDrawer.key, 5],
  [ForestHermitDrawer.key, 3],
  [QuestDwarfDrawer.key, 6],
  [QuestNomadDrawer.key, 4],
  [QuestWardenDrawer.key, 5],
  [WanderingMerchantDrawer.key, 5],
  [RescueNPCDrawer.key, 3],
]);

const DECOR_DRAWERS: EntityDrawer[] = [
  TreeDrawer,
  BushDrawer,
  RockDrawer,
  FlowerDrawer,
  MushroomDrawer,
  CactusDrawer,
  BoulderDrawer,
  CrystalDrawer,
  BonesDrawer,
  RuinsDrawer,
  SkeletalRemainsDrawer,
  AncientStatueDrawer,
  BrokenAltarDrawer,
  WarBannerDrawer,
  CharredTreeDrawer,
  CollapsedPillarDrawer,
  RitualCircleDrawer,
  FrozenCorpseDrawer,
  SandBuriedStructureDrawer,
  TreasureChestDrawer,
  GoldPileDrawer,
  LoreScrollDrawer,
  PuzzleStoneDrawer,
];

const EFFECT_DRAWERS: EntityDrawer[] = [
  LootBagDrawer,
  ExitPortalDrawer,
];

const PLAYER_DRAWER_BY_KEY = new Map<string, EntityDrawer>(PLAYER_DRAWERS.map(drawer => [drawer.key, drawer]));
const ENTITY_DRAWER_BY_KEY = new Map<string, EntityDrawer>([...PLAYER_DRAWERS, ...MONSTER_DRAWERS].map(drawer => [drawer.key, drawer]));
const NPC_DRAWER_BY_KEY = new Map<string, EntityDrawer>(NPC_DRAWERS.map(drawer => [drawer.key, drawer]));
const DECOR_DRAWER_BY_KEY = new Map<string, EntityDrawer>(DECOR_DRAWERS.map(drawer => [drawer.key, drawer]));
const EFFECT_DRAWER_BY_KEY = new Map<string, EntityDrawer>(EFFECT_DRAWERS.map(drawer => [drawer.key, drawer]));

// ═══════════════════════════════════════════════════════════════════════════
// ██ SpriteGenerator ██
// ═══════════════════════════════════════════════════════════════════════════

export class SpriteGenerator {
  private scene: Phaser.Scene;
  private utils: DrawUtils;

  // Terrain base colors for edge blending (indexed by tile type)
  static readonly TERRAIN_COLORS = [
    '#1b3715', // 0 = grass
    '#30221a', // 1 = dirt
    '#2a2e33', // 2 = stone
    '#0a161e', // 3 = water
    '#18181c', // 4 = wall
    '#281e10', // 5 = camp
    '#42301a', // 6 = camp_wall
  ];

  private static readonly TILE_NAMES = ['grass', 'dirt', 'stone', 'water', 'wall', 'camp', 'camp_wall'];

  /**
   * Generate a transition tile using bitmask-based boundary with noise displacement.
   * Called lazily by ZoneScene when a tile borders a different terrain type.
   * Results are cached as Phaser textures.
   *
   * neighbors order: [TR, TL, BR, BL] — the 4 edge-sharing neighbors in iso space.
   * Each bit: 1 = same terrain as base, 0 = different.
   */
  static generateTransitionTile(
    scene: Phaser.Scene,
    baseTileType: number,
    neighbors: [number, number, number, number],
  ): string {
    const key = `tile_t_${baseTileType}_${neighbors.join('')}`;
    if (scene.textures.exists(key)) return key;

    const s = TEXTURE_SCALE;
    const w = 64 * s, h = 32 * s;
    const canvas = document.createElement('canvas');
    canvas.width = w; canvas.height = h;
    const ctx = canvas.getContext('2d', { willReadFrequently: true })!;

    // Draw the base tile
    const baseTexKey = `tile_${SpriteGenerator.TILE_NAMES[baseTileType]}`;
    if (scene.textures.exists(baseTexKey)) {
      ctx.drawImage(scene.textures.get(baseTexKey).getSourceImage() as CanvasImageSource, 0, 0);
    }

    // Diamond clip
    const cx = w / 2, cy = h / 2;
    ctx.save();
    ctx.beginPath();
    ctx.moveTo(cx, 0); ctx.lineTo(w, cy); ctx.lineTo(cx, h); ctx.lineTo(0, cy);
    ctx.closePath();
    ctx.clip();

    const utils = new DrawUtils();

    // For each edge with a different neighbor, paint the neighbor's texture with a noisy boundary
    const edgeRegions: { startAngle: number; endAngle: number; dirX: number; dirY: number }[] = [
      { startAngle: -Math.PI / 2, endAngle: 0,            dirX: 0.5, dirY: -0.5 }, // TR
      { startAngle: -Math.PI,     endAngle: -Math.PI / 2, dirX: -0.5, dirY: -0.5 }, // TL
      { startAngle: 0,            endAngle: Math.PI / 2,   dirX: 0.5, dirY: 0.5 },  // BR
      { startAngle: Math.PI / 2,  endAngle: Math.PI,       dirX: -0.5, dirY: 0.5 }, // BL
    ];

    for (let i = 0; i < 4; i++) {
      const nType = neighbors[i];
      if (nType === baseTileType) continue;
      if (nType < 0 || nType > 6) continue;
      // Skip blending into/from walls (they have 3D height)
      if (nType === 4 || baseTileType === 4) continue;

      const region = edgeRegions[i];

      // Get neighbor tile texture for sampling
      const nTexKey = `tile_${SpriteGenerator.TILE_NAMES[nType]}`;
      let nCanvas: HTMLCanvasElement | null = null;
      if (scene.textures.exists(nTexKey)) {
        const srcImg = scene.textures.get(nTexKey).getSourceImage();
        const tc = document.createElement('canvas');
        tc.width = w; tc.height = h;
        const tctx = tc.getContext('2d', { willReadFrequently: true })!;
        tctx.drawImage(srcImg as CanvasImageSource, 0, 0);
        nCanvas = tc;
      }

      // Paint neighbor's texture in edge zone with bezier+noise boundary via per-pixel masking
      const imgData = ctx.getImageData(0, 0, w, h);
      const d = imgData.data;
      let nData: Uint8ClampedArray | null = null;
      if (nCanvas) {
        nData = nCanvas.getContext('2d', { willReadFrequently: true })!.getImageData(0, 0, w, h).data;
      }

      for (let py = 0; py < h; py++) {
        for (let px = 0; px < w; px++) {
          // Normalized position relative to diamond center
          const nx = (px - cx) / (w / 2);
          const ny = (py - cy) / (h / 2);

          // Check if pixel is in this edge's quadrant
          const angle = Math.atan2(ny, nx);
          let inQuadrant = false;
          if (region.startAngle < region.endAngle) {
            inQuadrant = angle >= region.startAngle && angle < region.endAngle;
          } else {
            inQuadrant = angle >= region.startAngle || angle < region.endAngle;
          }
          if (!inQuadrant) continue;

          // Distance from center toward edge (0 = center, 1 = diamond edge)
          const distFromCenter = Math.abs(nx) + Math.abs(ny);
          if (distFromCenter < 0.01) continue;

          // Noise displacement for organic boundary
          const noiseVal = utils.fbm(px * 0.08 + i * 100, py * 0.08 + i * 100, 3);
          const noiseDisp = (noiseVal - 0.5) * 0.35;

          // Transition boundary: starts at ~40% from center, fully neighbor at ~70%
          const boundary = 0.4 + noiseDisp;
          const fadeEnd = boundary + 0.3;

          if (distFromCenter < boundary) continue;

          const blend = Math.min(1, (distFromCenter - boundary) / (fadeEnd - boundary));

          const pi = (py * w + px) * 4;
          if (d[pi + 3] === 0) continue;

          if (nData) {
            // Blend with neighbor's actual texture
            d[pi]     = Math.round(d[pi] * (1 - blend) + nData[pi] * blend);
            d[pi + 1] = Math.round(d[pi + 1] * (1 - blend) + nData[pi + 1] * blend);
            d[pi + 2] = Math.round(d[pi + 2] * (1 - blend) + nData[pi + 2] * blend);
          } else {
            // Fallback: blend with flat color
            const nColor = SpriteGenerator.TERRAIN_COLORS[nType];
            const nr = parseInt(nColor.slice(1, 3), 16);
            const ng = parseInt(nColor.slice(3, 5), 16);
            const nb = parseInt(nColor.slice(5, 7), 16);
            d[pi]     = Math.round(d[pi] * (1 - blend) + nr * blend);
            d[pi + 1] = Math.round(d[pi + 1] * (1 - blend) + ng * blend);
            d[pi + 2] = Math.round(d[pi + 2] * (1 - blend) + nb * blend);
          }
        }
      }
      ctx.putImageData(imgData, 0, 0);
    }

    ctx.restore();
    scene.textures.addCanvas(key, canvas);
    return key;
  }

  constructor(scene: Phaser.Scene) {
    this.scene = scene;
    this.utils = new DrawUtils();
  }

  static ensurePlayerSheet(scene: Phaser.Scene, classId: string): void {
    this.ensureEntitySheet(scene, `player_${classId}`);
  }

  static ensureMonsterSheet(scene: Phaser.Scene, spriteKey: string): void {
    this.ensureEntitySheet(scene, spriteKey);
  }

  static ensureNPCSheet(scene: Phaser.Scene, npcId: string, npcType: string): void {
    const uniqueKey = `npc_${npcId}`;
    if (NPC_DRAWER_BY_KEY.has(uniqueKey)) {
      this.ensureNPCKey(scene, uniqueKey);
      return;
    }
    this.ensureNPCKey(scene, `npc_${npcType}`);
  }

  static ensureDecoration(scene: Phaser.Scene, decorType: string): void {
    const key = `decor_${decorType}`;
    const drawer = DECOR_DRAWER_BY_KEY.get(key);
    if (!drawer || scene.textures.exists(key)) return;
    new SpriteGenerator(scene).generateFromStaticDrawer(drawer);
  }

  static ensureEffect(scene: Phaser.Scene, effectKey: string): void {
    const drawer = EFFECT_DRAWER_BY_KEY.get(effectKey);
    if (!drawer || scene.textures.exists(effectKey)) return;
    new SpriteGenerator(scene).generateFromStaticDrawer(drawer);
  }

  static clearZoneTransientTextures(scene: Phaser.Scene): void {
    for (const key of scene.textures.getTextureKeys()) {
      const shouldRelease =
        key.startsWith('monster_') ||
        key.startsWith('npc_') ||
        key.startsWith('decor_') ||
        key === 'loot_bag' ||
        key === 'exit_portal';
      if (!shouldRelease) continue;
      if (this.isExternalTexture(scene, key)) continue;
      if (key.startsWith('monster_')) this.clearEntityAnimations(scene, key, false);
      if (key.startsWith('npc_')) this.clearNPCAnimations(scene, key);
      scene.textures.remove(key);
    }
  }

  private static ensureEntitySheet(scene: Phaser.Scene, key: string): void {
    const drawer = ENTITY_DRAWER_BY_KEY.get(key);
    if (!drawer) return;
    const generator = new SpriteGenerator(scene);
    const generatedNow = !scene.textures.exists(key);
    if (generatedNow) {
      generator.generateFromDrawer(drawer);
    }
    if (generatedNow) {
      generator.registerEntityAnimations(key, PLAYER_DRAWER_BY_KEY.has(key));
      return;
    }
    generator.ensureEntityAnimationsRegistered(key, PLAYER_DRAWER_BY_KEY.has(key));
  }

  private static ensureNPCKey(scene: Phaser.Scene, key: string): void {
    const drawer = NPC_DRAWER_BY_KEY.get(key);
    if (!drawer) return;
    const generator = new SpriteGenerator(scene);
    const generatedNow = !scene.textures.exists(key);
    if (generatedNow) {
      generator.generateFromNPCDrawer(drawer);
    }
    if (generatedNow) {
      generator.registerNPCAnimations(key, NPC_WORK_RATES.get(key) ?? 4);
      return;
    }
    generator.ensureNPCAnimationsRegistered(key, NPC_WORK_RATES.get(key) ?? 4);
  }

  private static isExternalTexture(scene: Phaser.Scene, key: string): boolean {
    if (!scene.textures.exists(key)) return false;
    const tex = scene.textures.get(key);
    return tex.source[0]?.source instanceof HTMLImageElement;
  }

  private static clearEntityAnimations(scene: Phaser.Scene, key: string, isPlayer: boolean): void {
    const actions = isPlayer
      ? ['idle', 'walk', 'attack', 'hurt', 'death', 'cast']
      : ['idle', 'walk', 'attack', 'hurt', 'death'];
    for (const action of actions) {
      const animKey = `${key}_${action}`;
      if (scene.anims.exists(animKey)) scene.anims.remove(animKey);
    }
  }

  private static clearNPCAnimations(scene: Phaser.Scene, key: string): void {
    for (const action of ['working', 'alert', 'idle', 'talking']) {
      const animKey = `${key}_${action}`;
      if (scene.anims.exists(animKey)) scene.anims.remove(animKey);
    }
  }

  /** Returns true if the texture was loaded from an external image file (HTMLImageElement),
   *  meaning procedural generation should be skipped in favour of the loaded asset. */
  private shouldSkipGeneration(key: string): boolean {
    if (!this.scene.textures.exists(key)) return false;
    const tex = this.scene.textures.get(key);
    return tex.source[0]?.source instanceof HTMLImageElement;
  }

  generateAll(): void {
    this.generateBootTextures();
    this.generatePlayerSheets();
    this.generateMonsterSheets();
    this.generateNPCSprites();
    this.generateDecorations();
  }

  generateBootTextures(): void {
    this.generateTiles();
    this.generateCampDecorations();
    this.generateEffects();
  }

  // ── Drawing Utilities (delegates to DrawUtils) ───────────────────────────

  private hash2d(x: number, y: number): number { return this.utils.hash2d(x, y); }
  private createCanvas(w: number, h: number): [HTMLCanvasElement, CanvasRenderingContext2D] { return this.utils.createCanvas(w, h); }
  private roundRect(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, r: number): void { return this.utils.roundRect(ctx, x, y, w, h, r); }
  private fillEllipse(ctx: CanvasRenderingContext2D, cx: number, cy: number, rx: number, ry: number): void { return this.utils.fillEllipse(ctx, cx, cy, rx, ry); }
  private fillCircle(ctx: CanvasRenderingContext2D, cx: number, cy: number, r: number): void { return this.utils.fillCircle(ctx, cx, cy, r); }
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

  /** Number of visual variants per ground tile type */
  static readonly TILE_VARIANTS = 3;

  private generateTiles(): void {
    // Generate 3 variants per ground tile for visual variety
    const groundDrawers: [string, (ctx: CanvasRenderingContext2D, w: number, h: number, seed: number) => void][] = [
      ['grass', this.drawGrass.bind(this)],
      ['dirt', this.drawDirt.bind(this)],
      ['stone', this.drawStone.bind(this)],
      ['water', this.drawWater.bind(this)],
      ['camp', this.drawCamp.bind(this)],
    ];
    for (const [name, drawFn] of groundDrawers) {
      for (let v = 0; v < SpriteGenerator.TILE_VARIANTS; v++) {
        const variantKey = `tile_${name}_${v}`;
        this.makeTile(variantKey, (ctx, w, h) => drawFn(ctx, w, h, v * 1000));
      }
      // Alias base key to variant 0 for backward compatibility
      if (!this.scene.textures.exists(`tile_${name}`)) {
        const srcTex = this.scene.textures.get(`tile_${name}_0`);
        if (srcTex) {
          const srcImg = srcTex.getSourceImage() as HTMLCanvasElement;
          const [canvas, ctx] = this.createCanvas(srcImg.width, srcImg.height);
          ctx.drawImage(srcImg, 0, 0);
          this.scene.textures.addCanvas(`tile_${name}`, canvas);
        }
      }
    }

    // Wall stays single variant (3D block, no need for variety)
    this.makeTile('tile_wall', (ctx, w, h) => this.drawWall(ctx, w, h));

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
    if (this.shouldSkipGeneration(key)) return;
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

  private drawGrass(ctx: CanvasRenderingContext2D, w: number, h: number, seed: number = 0): void {
    ctx.fillStyle = '#1b3715';
    ctx.fillRect(0, 0, w, h);

    // Multi-octave fbm noise for natural variation
    const imgData = ctx.getImageData(0, 0, w, h);
    const d = imgData.data;
    for (let py = 0; py < h; py++) {
      for (let px = 0; px < w; px++) {
        const i = (py * w + px) * 4;
        if (d[i + 3] === 0) continue;
        const n = this.utils.fbm((px + seed) * 0.04, (py + seed) * 0.04, 5);
        const val = (n - 0.5) * 18;
        d[i] = this.utils.clamp(d[i] + val * 0.3);
        d[i + 1] = this.utils.clamp(d[i + 1] + val);
        d[i + 2] = this.utils.clamp(d[i + 2] + val * 0.2);
      }
    }
    ctx.putImageData(imgData, 0, 0);

    // Grass blade details
    for (let i = 0; i < 30; i++) {
      const gx = this.hash2d(i * 7 + seed, 31) * w;
      const gy = this.hash2d(i * 13 + seed, 47) * h;
      const green = 35 + this.hash2d(i + seed, 91) * 30;
      ctx.strokeStyle = `rgba(18,${green | 0},12,0.18)`;
      ctx.lineWidth = 0.5 + this.hash2d(i + seed, 61) * 0.4;
      const lean = (this.hash2d(i + seed, 53) - 0.5) * 3;
      const bladeH = 2.5 + this.hash2d(i + seed, 71) * 4;
      ctx.beginPath();
      ctx.moveTo(gx, gy);
      ctx.quadraticCurveTo(gx + lean * 0.5, gy - bladeH * 0.6, gx + lean, gy - bladeH);
      ctx.stroke();
    }

    // Small shadow spots (ground undulation)
    for (let i = 0; i < 4; i++) {
      const sx = this.hash2d(i * 19 + seed, 113) * w;
      const sy = this.hash2d(i * 23 + seed, 127) * h;
      ctx.fillStyle = 'rgba(5,12,3,0.12)';
      this.fillEllipse(ctx, sx, sy, 3 + this.hash2d(i + seed, 131) * 4, 1.5 + this.hash2d(i + seed, 137) * 2);
    }

    // Sparse wildflower dots (1-2 per tile)
    for (let i = 0; i < 2; i++) {
      if (this.hash2d(i + seed, 200) > 0.6) continue;
      const fx = this.hash2d(i * 31 + seed, 141) * w;
      const fy = this.hash2d(i * 37 + seed, 149) * h;
      const colors = ['rgba(80,30,50,0.3)', 'rgba(70,60,20,0.25)', 'rgba(40,40,70,0.2)'];
      ctx.fillStyle = colors[i % colors.length];
      this.fillCircle(ctx, fx, fy, 0.8);
    }
  }

  private drawDirt(ctx: CanvasRenderingContext2D, w: number, h: number, seed: number = 0): void {
    ctx.fillStyle = '#30221a';
    ctx.fillRect(0, 0, w, h);

    // Warm-to-cool variation via fbm
    const imgData = ctx.getImageData(0, 0, w, h);
    const d = imgData.data;
    for (let py = 0; py < h; py++) {
      for (let px = 0; px < w; px++) {
        const i = (py * w + px) * 4;
        if (d[i + 3] === 0) continue;
        const n = this.utils.fbm((px + seed) * 0.035, (py + seed) * 0.035, 4);
        const warm = (n - 0.5) * 14;
        d[i] = this.utils.clamp(d[i] + warm * 1.2);
        d[i + 1] = this.utils.clamp(d[i + 1] + warm * 0.8);
        d[i + 2] = this.utils.clamp(d[i + 2] + warm * 0.3);
      }
    }
    ctx.putImageData(imgData, 0, 0);

    // Pebble clusters with highlight/shadow
    for (let i = 0; i < 7; i++) {
      const px = this.hash2d(i * 11 + seed, 23) * w;
      const py = this.hash2d(i * 17 + seed, 29) * h;
      const r = this.hash2d(i + seed, 59);
      const rx = 1.2 + r * 2.5, ry = 0.8 + r * 1.8;
      // Shadow side
      ctx.fillStyle = `rgba(${15 + r * 10 | 0},${10 + r * 8 | 0},${5 + r * 4 | 0},0.25)`;
      this.fillEllipse(ctx, px + 0.3, py + 0.3, rx, ry);
      // Pebble body
      ctx.fillStyle = `rgba(${50 + r * 25 | 0},${40 + r * 18 | 0},${28 + r * 12 | 0},0.3)`;
      this.fillEllipse(ctx, px, py, rx, ry);
      // Highlight
      ctx.fillStyle = `rgba(${70 + r * 20 | 0},${55 + r * 15 | 0},${38 + r * 10 | 0},0.12)`;
      this.fillEllipse(ctx, px - 0.3, py - 0.3, rx * 0.6, ry * 0.6);
    }

    // Branching crack network
    ctx.strokeStyle = 'rgba(15,10,5,0.22)';
    ctx.lineWidth = 0.5;
    ctx.lineCap = 'round';
    for (let i = 0; i < 3; i++) {
      let cx = this.hash2d(i * 37 + seed, 41) * w;
      let cy = this.hash2d(i * 43 + seed, 53) * h;
      ctx.beginPath();
      ctx.moveTo(cx, cy);
      for (let j = 0; j < 3 + (this.hash2d(i + seed, 201) * 3 | 0); j++) {
        const angle = this.hash2d(i * 7 + j + seed, 67) * Math.PI * 2;
        const len = 3 + this.hash2d(i + j + seed, 79) * 6;
        cx += Math.cos(angle) * len;
        cy += Math.sin(angle) * len;
        ctx.lineTo(cx, cy);
      }
      ctx.stroke();
    }
  }

  private drawStone(ctx: CanvasRenderingContext2D, w: number, h: number, seed: number = 0): void {
    ctx.fillStyle = '#2a2e33';
    ctx.fillRect(0, 0, w, h);

    // Per-slab color variation via noise
    const imgData = ctx.getImageData(0, 0, w, h);
    const d = imgData.data;
    for (let py = 0; py < h; py++) {
      for (let px = 0; px < w; px++) {
        const i = (py * w + px) * 4;
        if (d[i + 3] === 0) continue;
        const n = this.utils.fbm((px + seed) * 0.05, (py + seed) * 0.05, 3);
        const val = (n - 0.5) * 12;
        d[i] = this.utils.clamp(d[i] + val);
        d[i + 1] = this.utils.clamp(d[i + 1] + val);
        d[i + 2] = this.utils.clamp(d[i + 2] + val * 1.1);
      }
    }
    ctx.putImageData(imgData, 0, 0);

    // Distinct slab mortar lines
    ctx.strokeStyle = 'rgba(10,12,14,0.3)';
    ctx.lineWidth = 0.7;
    const cx = w / 2, cy = h / 2;
    // Horizontal mortar
    ctx.beginPath();
    ctx.moveTo(cx - w * 0.4, cy);
    ctx.lineTo(cx + w * 0.4, cy);
    ctx.stroke();
    // Vertical mortar segments (offset per variant)
    const vx1 = cx + (this.hash2d(seed, 151) - 0.5) * w * 0.3;
    ctx.beginPath();
    ctx.moveTo(vx1, cy - h * 0.35);
    ctx.lineTo(vx1, cy);
    ctx.stroke();
    const vx2 = cx + (this.hash2d(seed + 1, 157) - 0.5) * w * 0.3;
    ctx.beginPath();
    ctx.moveTo(vx2, cy);
    ctx.lineTo(vx2, cy + h * 0.35);
    ctx.stroke();

    // Defined moss patches
    for (let i = 0; i < 2; i++) {
      const mx = this.hash2d(i * 19 + seed, 83) * w;
      const my = this.hash2d(i * 23 + seed, 89) * h;
      ctx.fillStyle = 'rgba(18,35,14,0.15)';
      const rx = 3 + this.hash2d(i + seed, 103) * 5;
      const ry = 1.5 + this.hash2d(i + seed, 107) * 2.5;
      this.fillEllipse(ctx, mx, my, rx, ry);
      // Moss edge detail
      for (let j = 0; j < 3; j++) {
        const angle = this.hash2d(i * 3 + j + seed, 163) * Math.PI * 2;
        const dist = rx * 0.7;
        ctx.fillStyle = 'rgba(14,28,10,0.1)';
        this.fillCircle(ctx, mx + Math.cos(angle) * dist, my + Math.sin(angle) * dist * 0.5, 1.2);
      }
    }
  }

  private drawWater(ctx: CanvasRenderingContext2D, w: number, h: number, seed: number = 0): void {
    ctx.fillStyle = '#0a161e';
    ctx.fillRect(0, 0, w, h);

    // Subtle depth variation via noise
    const imgData = ctx.getImageData(0, 0, w, h);
    const d = imgData.data;
    for (let py = 0; py < h; py++) {
      for (let px = 0; px < w; px++) {
        const i = (py * w + px) * 4;
        if (d[i + 3] === 0) continue;
        const n = this.utils.fbm((px + seed) * 0.06, (py + seed) * 0.06, 3);
        const val = (n - 0.5) * 8;
        d[i] = this.utils.clamp(d[i] + val * 0.3);
        d[i + 1] = this.utils.clamp(d[i + 1] + val * 0.7);
        d[i + 2] = this.utils.clamp(d[i + 2] + val);
      }
    }
    ctx.putImageData(imgData, 0, 0);

    // Subtle caustic patterns
    ctx.strokeStyle = 'rgba(30,70,100,0.12)';
    ctx.lineWidth = 0.5;
    const cx = w / 2, cy = h / 2;
    for (let i = 0; i < 3; i++) {
      const rx = Math.max(0.5, w * (0.06 + this.hash2d(i + seed, 171) * 0.08));
      const ry = Math.max(0.5, h * (0.04 + this.hash2d(i + seed, 173) * 0.05));
      const ox = (this.hash2d(i * 3 + seed, 181) - 0.5) * w * 0.4;
      const oy = (this.hash2d(i * 5 + seed, 183) - 0.5) * h * 0.4;
      ctx.beginPath();
      ctx.ellipse(cx + ox, cy + oy, rx, ry, this.hash2d(i + seed, 191) * Math.PI, 0, Math.PI * 1.5);
      ctx.stroke();
    }

    // Faint reflection highlights
    ctx.fillStyle = 'rgba(50,100,140,0.08)';
    this.fillEllipse(ctx, cx + (this.hash2d(seed, 201) - 0.5) * w * 0.3, cy - h * 0.1, w * 0.06, h * 0.03);
  }

  private drawWall(ctx: CanvasRenderingContext2D, w: number, h: number): void {
    // The wall is NOT diamond-clipped — it has 3D height.
    ctx.fillStyle = '#18181c';
    ctx.fillRect(0, 0, w, h);

    const wallH = h * 0.45;

    // Front face (dark) with improved gradient
    const fGrad = ctx.createLinearGradient(0, h / 2, 0, h);
    fGrad.addColorStop(0, '#2a2a32');
    fGrad.addColorStop(0.5, '#22222a');
    fGrad.addColorStop(1, '#181822');
    ctx.fillStyle = fGrad;
    ctx.beginPath();
    ctx.moveTo(0, h / 2); ctx.lineTo(w / 2, h);
    ctx.lineTo(w / 2, h - wallH); ctx.lineTo(0, h / 2 - wallH);
    ctx.closePath(); ctx.fill();

    // Right face (lighter)
    const rGrad = ctx.createLinearGradient(w / 2, h / 2, w, h / 2);
    rGrad.addColorStop(0, '#33333a');
    rGrad.addColorStop(0.5, '#2c2c35');
    rGrad.addColorStop(1, '#252530');
    ctx.fillStyle = rGrad;
    ctx.beginPath();
    ctx.moveTo(w / 2, h); ctx.lineTo(w, h / 2);
    ctx.lineTo(w, h / 2 - wallH); ctx.lineTo(w / 2, h - wallH);
    ctx.closePath(); ctx.fill();

    // Top face
    ctx.fillStyle = '#383842';
    ctx.beginPath();
    ctx.moveTo(w / 2, h / 2 - wallH); ctx.lineTo(w, h / 2 - wallH);
    ctx.lineTo(w / 2, h - wallH); ctx.lineTo(0, h / 2 - wallH);
    ctx.closePath(); ctx.fill();

    // Brick lines on front face (improved stone pattern)
    ctx.strokeStyle = 'rgba(6,6,10,0.6)';
    ctx.lineWidth = 0.7;
    for (let i = 1; i <= 3; i++) {
      const t = i / 4;
      const y1 = (h / 2 - wallH) + t * wallH;
      const y2 = (h - wallH) + t * wallH * 0.01;
      ctx.beginPath();
      ctx.moveTo(0, h / 2 - wallH + t * wallH);
      ctx.lineTo(w / 2, h - wallH + t * wallH * 0.01);
      ctx.stroke();
    }
    // Vertical brick offsets on front
    for (let i = 1; i < 3; i++) {
      const bx = i * (w / 2) / 3;
      const by1 = (h / 2 - wallH) + wallH * 0.25;
      const by2 = (h / 2 - wallH) + wallH * 0.75;
      ctx.beginPath();
      ctx.moveTo(bx * 0.8, by1 + (h / 2 - by1) * (bx / (w / 2)));
      ctx.lineTo(bx * 0.8, by2 + (h / 2 - by2) * (bx / (w / 2)));
      ctx.stroke();
    }

    // Brick lines on right face
    for (let i = 1; i <= 3; i++) {
      const t = i / 4;
      ctx.beginPath();
      ctx.moveTo(w / 2, h - wallH + t * wallH * 0.01);
      ctx.lineTo(w, h / 2 - wallH + t * wallH);
      ctx.stroke();
    }

    // Base shadow where wall meets ground
    ctx.fillStyle = 'rgba(0,0,0,0.35)';
    ctx.beginPath();
    ctx.moveTo(0, h / 2);
    ctx.lineTo(w / 2, h);
    ctx.lineTo(w / 2, h + 2);
    ctx.lineTo(0, h / 2 + 2);
    ctx.closePath(); ctx.fill();
    ctx.beginPath();
    ctx.moveTo(w / 2, h);
    ctx.lineTo(w, h / 2);
    ctx.lineTo(w, h / 2 + 2);
    ctx.lineTo(w / 2, h + 2);
    ctx.closePath(); ctx.fill();

    // Edge highlight on top ridge
    ctx.strokeStyle = 'rgba(60,60,75,0.25)';
    ctx.lineWidth = 0.5;
    ctx.beginPath();
    ctx.moveTo(0, h / 2 - wallH); ctx.lineTo(w / 2, h / 2 - wallH);
    ctx.lineTo(w, h / 2 - wallH);
    ctx.stroke();

    this.applyNoiseToRegion(ctx, 0, 0, w, h, 4);
  }

  private drawCamp(ctx: CanvasRenderingContext2D, w: number, h: number, seed: number = 0): void {
    ctx.fillStyle = '#281e10';
    ctx.fillRect(0, 0, w, h);

    this.applyNoiseToRegion(ctx, 0, 0, w, h, 5);

    // Wood grain direction (diagonal planks)
    const cx = w / 2, cy = h / 2;
    ctx.strokeStyle = 'rgba(25,16,6,0.35)';
    ctx.lineWidth = 0.6;
    for (let i = -4; i <= 4; i++) {
      const ly = cy + i * h * 0.1;
      const inset = Math.abs(i) * w * 0.06;
      ctx.beginPath();
      ctx.moveTo(cx - w * 0.4 + inset, ly);
      ctx.lineTo(cx + w * 0.4 - inset, ly);
      ctx.stroke();
    }

    // Nail holes / knots
    for (let i = 0; i < 3; i++) {
      const nx = this.hash2d(i * 11 + seed, 211) * w;
      const ny = this.hash2d(i * 13 + seed, 217) * h;
      ctx.fillStyle = 'rgba(10,6,2,0.3)';
      this.fillCircle(ctx, nx, ny, 0.6 + this.hash2d(i + seed, 223) * 0.4);
    }

    // Worn center (lighter from foot traffic)
    const wear = ctx.createRadialGradient(cx, cy, 0, cx, cy, w * 0.25);
    wear.addColorStop(0, 'rgba(50,35,18,0.1)');
    wear.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = wear;
    ctx.fillRect(0, 0, w, h);

    // Subtle warm glow
    const glow = ctx.createRadialGradient(cx, cy, 0, cx, cy, w * 0.4);
    glow.addColorStop(0, 'rgba(120,65,15,0.08)');
    glow.addColorStop(0.6, 'rgba(70,30,8,0.04)');
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
    for (const drawer of PLAYER_DRAWERS) {
      this.generateFromDrawer(drawer);
      this.ensureEntityAnimationsRegistered(drawer.key, true);
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
    for (const drawer of MONSTER_DRAWERS) {
      this.generateFromDrawer(drawer);
      this.ensureEntityAnimationsRegistered(drawer.key, false);
    }
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
    for (const drawer of NPC_DRAWERS) {
      this.generateFromNPCDrawer(drawer);
      this.ensureNPCAnimationsRegistered(drawer.key, NPC_WORK_RATES.get(drawer.key) ?? 4);
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

  // ═══════════════════════════════════════════════════════════════════════
  // ██ DECORATIONS & EFFECTS ██
  // ═══════════════════════════════════════════════════════════════════════

  private generateDecorations(): void {
    for (const drawer of DECOR_DRAWERS) {
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
    for (const drawer of EFFECT_DRAWERS) {
      this.generateFromStaticDrawer(drawer);
    }
  }

  // ═══════════════════════════════════════════════════════════════════════
  // ██ ANIMATION REGISTRATION ██
  // ═══════════════════════════════════════════════════════════════════════

  private hasEntityAnimationsRegistered(key: string, isPlayer: boolean): boolean {
    const actions = isPlayer
      ? ['idle', 'walk', 'attack', 'hurt', 'death', 'cast']
      : ['idle', 'walk', 'attack', 'hurt', 'death'];
    return actions.every(action => this.scene.anims.exists(`${key}_${action}`));
  }

  private ensureEntityAnimationsRegistered(key: string, isPlayer: boolean): void {
    if (!this.hasEntityAnimationsRegistered(key, isPlayer)) {
      this.registerEntityAnimations(key, isPlayer);
    }
  }

  private registerEntityAnimations(key: string, isPlayer: boolean): void {
    const anims = this.scene.anims;
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
  }

  private hasNPCAnimationsRegistered(key: string): boolean {
    return ['working', 'alert', 'idle', 'talking'].every(action => this.scene.anims.exists(`${key}_${action}`));
  }

  private ensureNPCAnimationsRegistered(key: string, workRate: number): void {
    if (!this.hasNPCAnimationsRegistered(key)) {
      this.registerNPCAnimations(key, workRate);
    }
  }

  private registerNPCAnimations(key: string, workRate: number): void {
    const anims = this.scene.anims;

    const workKey = `${key}_working`;
    if (anims.exists(workKey)) anims.remove(workKey);
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

  private registerAnimations(): void {
    for (const drawer of PLAYER_DRAWERS) {
      this.registerEntityAnimations(drawer.key, true);
    }
    for (const drawer of MONSTER_DRAWERS) {
      this.registerEntityAnimations(drawer.key, false);
    }
    for (const drawer of NPC_DRAWERS) {
      this.registerNPCAnimations(drawer.key, NPC_WORK_RATES.get(drawer.key) ?? 4);
    }
  }
}
