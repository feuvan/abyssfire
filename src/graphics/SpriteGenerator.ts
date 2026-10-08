import Phaser from 'phaser';
import { TEXTURE_SCALE } from '../config';
import { CAMP_THEMES } from '../data/camp-themes';
import { getActionFrameRate } from '../systems/CharacterAnimator';
import { DrawUtils } from './DrawUtils';
import { generateLegacyTiles } from './terrain/LegacyTiles';
import type { EntityAction, EntityDrawer, PlayerView } from './sprites/types';
import {
  PLAYER_ACTION_FRAME_COUNTS,
  PLAYER_ACTION_ORDER,
  PLAYER_VIEWS,
  getPlayerActionFrameRange,
  getPlayerViewFrameRange,
  playerAnimKey,
  computeSheetGrid,
  sheetFrameOrigin,
  type SheetGrid,
} from './sprites/types';
import { inkLegacyFrame } from './sprites/rig/Rig';
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
import { FIELD_NPC_DRAWERS } from './sprites/npcs/FieldNPCs';
import { ForestHermitDrawer } from './sprites/npcs/ForestHermit';
import { QuestDwarfDrawer } from './sprites/npcs/QuestDwarf';
import { QuestNomadDrawer } from './sprites/npcs/QuestNomad';
import { QuestWardenDrawer } from './sprites/npcs/QuestWarden';
import { WanderingMerchantDrawer } from './sprites/npcs/WanderingMerchant';
import { RescueNPCDrawer } from './sprites/npcs/RescueNPC';
import { EVENT_NPC_DRAWERS } from './sprites/npcs/EventNPCs';
import { TreeDrawer, TreeRoundDrawer, ForestTreeDrawer, ForestTreeTallDrawer, PineDrawer, DeadTreeDrawer } from './sprites/decorations/Tree';
import { BushDrawer, FernDrawer, DryShrubDrawer, GrassDrawer, GrassForestDrawer, GrassDryDrawer, GrassAshDrawer } from './sprites/decorations/Bush';
import { RockDrawer, RockMossDrawer, RockSandDrawer, RockSlateDrawer, RockBasaltDrawer } from './sprites/decorations/Rock';
import { FlowerDrawer } from './sprites/decorations/Flower';
import { MushroomDrawer, MushroomRedDrawer } from './sprites/decorations/Mushroom';
import { CactusDrawer, BarrelCactusDrawer } from './sprites/decorations/Cactus';
import { BoulderDrawer, BoulderSnowDrawer, BoulderSandDrawer, BoulderBasaltDrawer } from './sprites/decorations/Boulder';
import { CrystalDrawer, CrystalBlueDrawer } from './sprites/decorations/Crystal';
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
import type { DecorDrawer } from './sprites/decorations/DecorKit';
import { STATIC_CAMP_DRAWERS, makeTentDrawer, makeBannerDrawer, makeFlameDrawer, FLAME_FRAMES } from './sprites/decorations/CampProps';
import { EVENT_PROP_DRAWERS } from './sprites/decorations/EventProps';
import { PetSpriteDrawers } from './sprites/decorations/Pets';
import { TOWER_PROP_DRAWERS } from './sprites/decorations/TowerProps';
import { LootBagDrawer } from './sprites/effects/LootBag';
import { ExitPortalDrawer } from './sprites/effects/ExitPortal';
import { LORE_PROP_DRAWERS } from './sprites/decorations/LoreProps';
import { DUNGEON_GATE_DRAWERS } from './sprites/effects/DungeonGates';
import { PICKUP_DRAWERS } from './sprites/effects/Pickups';
import {
  PET_ACTION_ORDER,
  PET_ACTION_FRAME_COUNTS,
  PET_FRAME_RATES,
  PET_VIEWS,
  getPetDrawer,
  getPetSheetMeta,
  petActionFrameRange,
  petSheetKey,
  type PetSheetMeta,
  type PetStage,
} from './sprites/pets';

// ── Frame Layout Constants ──────────────────────────────────────────────────
const IDLE_START = 0, IDLE_COUNT = 4;
const WALK_START = 4, WALK_COUNT = 6;
const ATK_START = 10, ATK_COUNT = 4;
const HURT_START = 14, HURT_COUNT = 2;
const DEATH_START = 16, DEATH_COUNT = 4;
/** Frames per view in a monster sheet (multi-view sheets repeat the block). */
const MONSTER_VIEW_FRAMES = 20;
const MONSTER_ACTIONS = ['idle', 'walk', 'attack', 'hurt', 'death'] as const;

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
  ...FIELD_NPC_DRAWERS,
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
  ...FIELD_NPC_DRAWERS.map(d => [d.key, 4] as [string, number]),
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
  // Zone variants (generated lazily per zone via ensureDecoration)
  TreeRoundDrawer,
  ForestTreeDrawer,
  ForestTreeTallDrawer,
  PineDrawer,
  DeadTreeDrawer,
  FernDrawer,
  DryShrubDrawer,
  GrassDrawer,
  GrassForestDrawer,
  GrassDryDrawer,
  GrassAshDrawer,
  RockMossDrawer,
  RockSandDrawer,
  RockSlateDrawer,
  RockBasaltDrawer,
  MushroomRedDrawer,
  BarrelCactusDrawer,
  BoulderSnowDrawer,
  BoulderSandDrawer,
  BoulderBasaltDrawer,
  CrystalBlueDrawer,
];

const EFFECT_DRAWERS: EntityDrawer[] = [
  LootBagDrawer,
  ExitPortalDrawer,
];

const PLAYER_DRAWER_BY_KEY = new Map<string, EntityDrawer>(PLAYER_DRAWERS.map(drawer => [drawer.key, drawer]));
const ENTITY_DRAWER_BY_KEY = new Map<string, EntityDrawer>([...PLAYER_DRAWERS, ...MONSTER_DRAWERS].map(drawer => [drawer.key, drawer]));
const NPC_DRAWER_BY_KEY = new Map<string, EntityDrawer>(
  [...NPC_DRAWERS, ...EVENT_NPC_DRAWERS].map(drawer => [drawer.key, drawer]),
);
const DECOR_DRAWER_BY_KEY = new Map<string, EntityDrawer>(
  [...DECOR_DRAWERS, ...EVENT_PROP_DRAWERS, ...PetSpriteDrawers, ...LORE_PROP_DRAWERS, ...TOWER_PROP_DRAWERS].map(drawer => [drawer.key, drawer]),
);
// Gates and pickups are generated lazily (ensureEffect) the first time a zone needs them.
const EFFECT_DRAWER_BY_KEY = new Map<string, EntityDrawer>(
  [...EFFECT_DRAWERS, ...DUNGEON_GATE_DRAWERS, ...PICKUP_DRAWERS].map(drawer => [drawer.key, drawer]),
);
const CAMP_DRAWER_META = new Map<string, EntityDrawer>([
  ...STATIC_CAMP_DRAWERS,
  makeTentDrawer('camp_tent', CAMP_THEMES.plains.tentColor),
  makeBannerDrawer('camp_banner', CAMP_THEMES.plains.bannerColor, CAMP_THEMES.plains.bannerDark),
  makeFlameDrawer('camp_flame', CAMP_THEMES.plains.torchFlame),
].map(drawer => [drawer.key, drawer]));

// ═══════════════════════════════════════════════════════════════════════════
// ██ SpriteGenerator ██
// ═══════════════════════════════════════════════════════════════════════════

export class SpriteGenerator {
  /**
   * Character sheets are the bulk of zone-load time, so they outlive the zone
   * that drew them: a sheet is released only after this many zone changes
   * without being used (1 on touch devices to keep memory low).
   */
  static sheetKeepZones = typeof navigator !== 'undefined' && navigator.maxTouchPoints > 0 ? 1 : 2;
  private static zoneEpoch = 0;
  private static sheetLastUsed = new Map<string, number>();

  private scene: Phaser.Scene;
  private utils: DrawUtils;

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

  /**
   * Lazily generate a ley-beast sheet (`beast_<petId>`, `_e1`, `_e2` for the
   * evolved stages) and register its animations: `<key>_<action>` for the se
   * view and `<key>_ne_<action>` for the back view, actions idle / walk /
   * attack / cast / hurt. Sheets follow the monster cache rules (kept for
   * `sheetKeepZones` zone changes after their last use). Returns the key.
   */
  static ensurePetSheet(scene: Phaser.Scene, petId: string, stage: 0 | 1 | 2): string {
    const drawer = getPetDrawer(petId, stage);
    const key = drawer?.key ?? petSheetKey(petId, stage);
    if (!drawer) return key;
    this.sheetLastUsed.set(key, this.zoneEpoch);
    const generator = new SpriteGenerator(scene);
    if (!scene.textures.exists(key)) {
      generator.generatePetSheet(drawer);
    }
    generator.registerPetAnimations(key);
    return key;
  }

  /** Placement / animation metadata for a ley-beast sheet (see `PetSheetMeta`). */
  static getPetSheetMeta(petId: string, stage: 0 | 1 | 2): PetSheetMeta | null {
    return getPetSheetMeta(petId, stage as PetStage);
  }

  static ensureNPCSheet(scene: Phaser.Scene, npcId: string, npcType: string): void {
    const uniqueKey = `npc_${npcId}`;
    if (NPC_DRAWER_BY_KEY.has(uniqueKey)) {
      this.ensureNPCKey(scene, uniqueKey);
      return;
    }
    this.ensureNPCKey(scene, `npc_${npcType}`);
  }

  /** Lazily generate an NPC sheet referenced directly by its data-defined texture key. */
  static ensureNPCSprite(scene: Phaser.Scene, spriteKey: string): void {
    this.ensureNPCKey(scene, spriteKey);
  }

  /** Placement metadata for a decoration / camp prop texture (origin Y, layering). */
  static getDecorMeta(key: string): { anchorY: number; tall: boolean; flat: boolean } | null {
    const drawer = (DECOR_DRAWER_BY_KEY.get(key) ?? CAMP_DRAWER_META.get(key.replace(/_(plains|forest|mountain|desert|abyss)$/, ''))) as DecorDrawer | undefined;
    if (!drawer || typeof drawer.anchorY !== 'number') return null;
    return { anchorY: drawer.anchorY, tall: !!drawer.tall, flat: !!drawer.flat };
  }

  /**
   * Lazily generate a camp prop. Tents, banners and flames are tinted by the
   * zone's camp theme; returns the texture key to use.
   */
  static ensureCampDecoration(scene: Phaser.Scene, type: string, themeName?: string): string {
    const theme = themeName ? CAMP_THEMES[themeName as keyof typeof CAMP_THEMES] : undefined;
    const themed = theme && (type === 'tent' || type === 'banner' || type === 'flame');
    const key = themed ? `camp_${type}_${themeName}` : `camp_${type}`;
    if (scene.textures.exists(key)) return key;
    let drawer: EntityDrawer | undefined;
    if (type === 'tent') drawer = makeTentDrawer(key, theme?.tentColor ?? CAMP_THEMES.plains.tentColor);
    else if (type === 'banner') drawer = makeBannerDrawer(key, theme?.bannerColor ?? CAMP_THEMES.plains.bannerColor, theme?.bannerDark ?? CAMP_THEMES.plains.bannerDark);
    else if (type === 'flame') drawer = makeFlameDrawer(key, theme?.torchFlame ?? 0xff8800);
    else drawer = STATIC_CAMP_DRAWERS.find(d => d.key === key);
    if (!drawer) return key;
    new SpriteGenerator(scene).generateFromStaticDrawer(drawer);
    if (type === 'flame') {
      const animKey = `${key}_anim`;
      if (!scene.anims.exists(animKey)) {
        scene.anims.create({
          key: animKey,
          frames: scene.anims.generateFrameNumbers(key, { start: 0, end: FLAME_FRAMES - 1 }),
          frameRate: 10,
          repeat: -1,
        });
      }
    }
    return key;
  }

  static ensureDecoration(scene: Phaser.Scene, decorType: string): void {
    const key = decorType.startsWith('decor_') ? decorType : `decor_${decorType}`;
    const drawer = DECOR_DRAWER_BY_KEY.get(key);
    if (!drawer) return;
    if (!scene.textures.exists(key)) new SpriteGenerator(scene).generateFromStaticDrawer(drawer);
    this.ensureLoopAnimation(scene, drawer);
  }

  /**
   * Register `<key>_anim` for a static prop that declares an idle loop
   * (chest glint, portal swirl). Returns the animation key, or null.
   */
  static getLoopAnimKey(scene: Phaser.Scene, key: string): string | null {
    const animKey = `${key}_anim`;
    return scene.anims.exists(animKey) ? animKey : null;
  }

  private static ensureLoopAnimation(scene: Phaser.Scene, drawer: EntityDrawer): void {
    const loop = (drawer as DecorDrawer).loop;
    if (!loop || !scene.textures.exists(drawer.key)) return;
    const animKey = `${drawer.key}_anim`;
    if (scene.anims.exists(animKey)) return;
    // Externally supplied single-image textures have no numbered frames.
    if (!scene.textures.get(drawer.key).has(String(loop.frames - 1))) return;
    scene.anims.create({
      key: animKey,
      frames: scene.anims.generateFrameNumbers(drawer.key, { start: 0, end: loop.frames - 1 }),
      frameRate: loop.fps,
      repeat: -1,
    });
  }

  /** Frame size (pre-TEXTURE_SCALE) of a generated character sheet, if known. */
  static getCharacterFrameSize(key: string): { frameW: number; frameH: number } | null {
    const drawer = ENTITY_DRAWER_BY_KEY.get(key) ?? NPC_DRAWER_BY_KEY.get(key);
    return drawer ? { frameW: drawer.frameW, frameH: drawer.frameH } : null;
  }

  static hasNPCSprite(spriteKey: string): boolean {
    return NPC_DRAWER_BY_KEY.has(spriteKey);
  }

  static hasDecoration(spriteKey: string): boolean {
    return DECOR_DRAWER_BY_KEY.has(spriteKey);
  }

  static ensureEffect(scene: Phaser.Scene, effectKey: string): void {
    const drawer = EFFECT_DRAWER_BY_KEY.get(effectKey);
    if (!drawer) return;
    if (!scene.textures.exists(effectKey)) new SpriteGenerator(scene).generateFromStaticDrawer(drawer);
    this.ensureLoopAnimation(scene, drawer);
  }

  /** Placement metadata (ground-contact origin Y) for an effect / pickup texture. */
  static getEffectAnchorY(key: string): number | null {
    const drawer = EFFECT_DRAWER_BY_KEY.get(key) as DecorDrawer | undefined;
    return drawer && typeof drawer.anchorY === 'number' ? drawer.anchorY : null;
  }

  static clearZoneTransientTextures(scene: Phaser.Scene): void {
    for (const key of scene.textures.getTextureKeys()) {
      const shouldRelease =
        key.startsWith('monster_') ||
        key.startsWith('beast_') ||
        key.startsWith('npc_') ||
        key.startsWith('decor_') ||
        key === 'loot_bag' ||
        key === 'exit_portal';
      if (!shouldRelease) continue;
      if (this.isExternalTexture(scene, key)) continue;
      if (key.startsWith('monster_') || key.startsWith('npc_') || key.startsWith('beast_')) {
        const used = this.sheetLastUsed.get(key) ?? -Infinity;
        if (this.zoneEpoch - used < this.sheetKeepZones) continue;
        this.sheetLastUsed.delete(key);
      }
      if (key.startsWith('monster_')) this.clearEntityAnimations(scene, key, false);
      if (key.startsWith('npc_')) this.clearNPCAnimations(scene, key);
      if (key.startsWith('beast_')) this.clearPetAnimations(scene, key);
      if (key.startsWith('decor_') && scene.anims.exists(`${key}_anim`)) scene.anims.remove(`${key}_anim`);
      scene.textures.remove(key);
    }
    this.zoneEpoch++;
  }

  private static ensureEntitySheet(scene: Phaser.Scene, key: string): void {
    const drawer = ENTITY_DRAWER_BY_KEY.get(key);
    if (!drawer) return;
    this.sheetLastUsed.set(key, this.zoneEpoch);
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
    this.sheetLastUsed.set(key, this.zoneEpoch);
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
    if (isPlayer) {
      for (const view of PLAYER_VIEWS) {
        for (const action of PLAYER_ACTION_ORDER) {
          const animKey = playerAnimKey(key, view, action);
          if (scene.anims.exists(animKey)) scene.anims.remove(animKey);
        }
      }
      return;
    }
    for (const view of PLAYER_VIEWS) {
      for (const action of MONSTER_ACTIONS) {
        const animKey = playerAnimKey(key, view, action);
        if (scene.anims.exists(animKey)) scene.anims.remove(animKey);
      }
    }
  }

  private static clearPetAnimations(scene: Phaser.Scene, key: string): void {
    for (const view of PET_VIEWS) {
      for (const action of PET_ACTION_ORDER) {
        const animKey = playerAnimKey(key, view, action);
        if (scene.anims.exists(animKey)) scene.anims.remove(animKey);
      }
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

  // ═══════════════════════════════════════════════════════════════════════
  // ██ TILE GENERATION ██
  // ═══════════════════════════════════════════════════════════════════════

  /** Number of visual variants per legacy ground tile key */
  static readonly TILE_VARIANTS = 3;

  /**
   * Legacy tile keys only — zones use the themed terrain built by
   * `ZoneTerrain` (src/graphics/terrain/) on zone entry.
   */
  private generateTiles(): void {
    generateLegacyTiles(this.scene, SpriteGenerator.TILE_VARIANTS, key => this.shouldSkipGeneration(key));
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

  /** Draw one sheet cell, clipped to its bounds, optionally through the ink pass. */
  private drawCell(
    ctx: CanvasRenderingContext2D,
    grid: SheetGrid,
    index: number,
    fw: number,
    fh: number,
    ink: boolean,
    draw: (c: CanvasRenderingContext2D) => void,
  ): void {
    const cell = sheetFrameOrigin(grid, index);
    if (ink) {
      const [tmp, tctx] = this.utils.createCanvas(fw, fh);
      tctx.save();
      draw(tctx);
      tctx.restore();
      ctx.save();
      ctx.translate(cell.x, cell.y);
      inkLegacyFrame(ctx, tmp, fw, fh, { inkPx: Math.max(2, Math.min(3, fh / 60)) });
      ctx.restore();
      return;
    }
    ctx.save();
    ctx.translate(cell.x, cell.y);
    // Clip to the cell so strokes can't bleed into neighbouring frames.
    ctx.beginPath();
    ctx.rect(0, 0, fw, fh);
    ctx.clip();
    draw(ctx);
    ctx.restore();
  }

  private generateFromDrawer(drawer: EntityDrawer): void {
    if (this.shouldSkipGeneration(drawer.key)) return;

    const s = TEXTURE_SCALE;
    const fw = drawer.frameW * s, fh = drawer.frameH * s;
    const grid = computeSheetGrid(fw, fh, drawer.totalFrames);
    const [canvas, ctx] = this.utils.createCanvas(grid.width, grid.height);

    const isPlayer = PLAYER_DRAWER_BY_KEY.has(drawer.key);
    // Player heroes carry every action once per isometric view (view-major).
    const actions: [EntityAction, number, number, PlayerView | undefined][] = isPlayer
      ? (drawer.views ?? [undefined]).flatMap(view => PLAYER_ACTION_ORDER.map((action): [EntityAction, number, number, PlayerView | undefined] => {
          const range = view ? getPlayerViewFrameRange(view, action) : getPlayerActionFrameRange(action);
          return [action, range.start, PLAYER_ACTION_FRAME_COUNTS[action], view];
        }))
      // Monsters: 20 frames per view, view-major (se first).
      : (drawer.views ?? [undefined]).flatMap((view, vi): [EntityAction, number, number, PlayerView | undefined][] => [
          ['idle', vi * MONSTER_VIEW_FRAMES + IDLE_START, IDLE_COUNT, view],
          ['walk', vi * MONSTER_VIEW_FRAMES + WALK_START, WALK_COUNT, view],
          ['attack', vi * MONSTER_VIEW_FRAMES + ATK_START, ATK_COUNT, view],
          ['hurt', vi * MONSTER_VIEW_FRAMES + HURT_START, HURT_COUNT, view],
          ['death', vi * MONSTER_VIEW_FRAMES + DEATH_START, DEATH_COUNT, view],
        ]);

    // Rigged drawers ink themselves; legacy ones get the shared ink pass.
    const legacy = !drawer.inked;
    for (const [action, start, count, view] of actions) {
      for (let f = 0; f < count; f++) {
        this.drawCell(ctx, grid, start + f, fw, fh, legacy,
          c => drawer.drawFrame(c, f, action, fw, fh, this.utils, view));
      }
    }

    // Surface grain only suits legacy art; cel-shaded rigs stay clean (and
    // the per-pixel fbm pass costs ~50 ms per sheet).
    if (legacy) this.utils.applyNoiseToRegion(ctx, 0, 0, canvas.width, canvas.height, 4);

    const key = drawer.key;
    if (this.scene.textures.exists(key)) this.scene.textures.remove(key);
    const canvasTex = this.scene.textures.addCanvas(key, canvas)!;
    for (let i = 0; i < drawer.totalFrames; i++) {
      const cell = sheetFrameOrigin(grid, i);
      canvasTex.add(i, 0, cell.x, cell.y, fw, fh);
    }
  }

  /** Ley-beast sheet: every pet action once per view (view-major). */
  private generatePetSheet(drawer: NonNullable<ReturnType<typeof getPetDrawer>>): void {
    if (this.shouldSkipGeneration(drawer.key)) return;
    const s = TEXTURE_SCALE;
    const fw = drawer.frameW * s, fh = drawer.frameH * s;
    const grid = computeSheetGrid(fw, fh, drawer.totalFrames);
    const [canvas, ctx] = this.utils.createCanvas(grid.width, grid.height);
    for (const view of PET_VIEWS) {
      for (const action of PET_ACTION_ORDER) {
        const { start } = petActionFrameRange(view, action);
        for (let f = 0; f < PET_ACTION_FRAME_COUNTS[action]; f++) {
          this.drawCell(ctx, grid, start + f, fw, fh, false, c => drawer.drawPose(c, action, f, fw, fh, view));
        }
      }
    }
    if (this.scene.textures.exists(drawer.key)) this.scene.textures.remove(drawer.key);
    const tex = this.scene.textures.addCanvas(drawer.key, canvas)!;
    for (let i = 0; i < drawer.totalFrames; i++) {
      const cell = sheetFrameOrigin(grid, i);
      tex.add(i, 0, cell.x, cell.y, fw, fh);
    }
  }

  private registerPetAnimations(key: string): void {
    const anims = this.scene.anims;
    // An external single-view PNG override only holds the first view.
    const frames = this.scene.textures.get(key).frameTotal - 1;
    for (const view of PET_VIEWS) {
      for (const action of PET_ACTION_ORDER) {
        const { start, end } = petActionFrameRange(view, action);
        if (end >= frames) continue;
        const animKey = playerAnimKey(key, view, action);
        if (anims.exists(animKey)) continue;
        anims.create({
          key: animKey,
          frames: anims.generateFrameNumbers(key, { start, end }),
          frameRate: PET_FRAME_RATES[action],
          repeat: action === 'idle' || action === 'walk' ? -1 : 0,
        });
      }
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
    const grid = computeSheetGrid(fw, fh, drawer.totalFrames);
    const [canvas, ctx] = this.utils.createCanvas(grid.width, grid.height);

    const actions: [string, number, number][] = [
      ['working', NPC_WORK_START, NPC_WORK_COUNT],
      ['alert', NPC_ALERT_START, NPC_ALERT_COUNT],
      ['idle', NPC_IDLE_START, NPC_IDLE_COUNT],
      ['talking', NPC_TALK_START, NPC_TALK_COUNT],
    ];

    const legacy = !drawer.inked;
    for (const [action, start, count] of actions) {
      for (let f = 0; f < count; f++) {
        this.drawCell(ctx, grid, start + f, fw, fh, legacy,
          c => drawer.drawFrame(c, f, action as any, fw, fh, this.utils));
      }
    }

    if (legacy) this.utils.applyNoiseToRegion(ctx, 0, 0, canvas.width, canvas.height, 3);

    const key = drawer.key;
    if (this.scene.textures.exists(key)) this.scene.textures.remove(key);
    const canvasTex = this.scene.textures.addCanvas(key, canvas)!;
    for (let i = 0; i < drawer.totalFrames; i++) {
      const cell = sheetFrameOrigin(grid, i);
      canvasTex.add(i, 0, cell.x, cell.y, fw, fh);
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
    const frames = Math.max(1, drawer.totalFrames);
    const [canvas, ctx] = this.utils.createCanvas(w * frames, h);

    for (let f = 0; f < frames; f++) {
      ctx.save();
      ctx.translate(f * w, 0);
      if (frames > 1) {
        ctx.beginPath();
        ctx.rect(0, 0, w, h);
        ctx.clip();
      }
      drawer.drawFrame(ctx, f, 'idle', w, h, this.utils);
      ctx.restore();
    }

    if (this.scene.textures.exists(drawer.key)) this.scene.textures.remove(drawer.key);
    const tex = this.scene.textures.addCanvas(drawer.key, canvas);
    if (tex && frames > 1) {
      for (let f = 0; f < frames; f++) tex.add(f, 0, f * w, 0, w, h);
    }
  }

  // ═══════════════════════════════════════════════════════════════════════
  // ██ DECORATIONS & EFFECTS ██
  // ═══════════════════════════════════════════════════════════════════════

  private generateDecorations(): void {
    for (const drawer of DECOR_DRAWERS) {
      this.generateFromStaticDrawer(drawer);
    }
  }

  // ═══════════════════════════════════════════════════════════════════════
  // ██ CAMP DECORATION SPRITES ██
  // ═══════════════════════════════════════════════════════════════════════

  private generateCampDecorations(): void {
    // Default (plains-themed) camp props; themed tents/banners/flames are made
    // lazily by ensureCampDecoration when a zone needs them.
    for (const type of ['campfire', 'torch', 'tent', 'barrel', 'crate', 'banner', 'well', 'flame']) {
      SpriteGenerator.ensureCampDecoration(this.scene, type);
    }
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
    if (isPlayer) {
      return this.playerSheetViews(key).every(view =>
        PLAYER_ACTION_ORDER.every(action => this.scene.anims.exists(playerAnimKey(key, view, action))));
    }
    return this.monsterSheetViews(key).every(view =>
      MONSTER_ACTIONS.every(action => this.scene.anims.exists(playerAnimKey(key, view, action))));
  }

  /** Views a monster sheet holds (an external PNG override only has the first). */
  private monsterSheetViews(key: string): readonly PlayerView[] {
    const views = ENTITY_DRAWER_BY_KEY.get(key)?.views ?? [PLAYER_VIEWS[0]];
    if (!this.scene.textures.exists(key)) return views;
    const frames = this.scene.textures.get(key).frameTotal - 1; // minus __BASE
    return views.filter((_, vi) => (vi + 1) * MONSTER_VIEW_FRAMES <= frames);
  }

  /**
   * Views the loaded sheet actually holds. An external single-view PNG
   * override only has the first view's frames.
   */
  private playerSheetViews(key: string): readonly PlayerView[] {
    const views = PLAYER_DRAWER_BY_KEY.get(key)?.views ?? [PLAYER_VIEWS[0]];
    if (!this.scene.textures.exists(key)) return views;
    const frames = this.scene.textures.get(key).frameTotal - 1; // minus __BASE
    return views.filter(view => getPlayerViewFrameRange(view, 'cast').end < frames);
  }

  private ensureEntityAnimationsRegistered(key: string, isPlayer: boolean): void {
    if (!this.hasEntityAnimationsRegistered(key, isPlayer)) {
      this.registerEntityAnimations(key, isPlayer);
    }
  }

  private registerEntityAnimations(key: string, isPlayer: boolean): void {
    const anims = this.scene.anims;
    const defs: [string, number, number, number, number][] = isPlayer
      ? this.playerSheetViews(key).flatMap(view => PLAYER_ACTION_ORDER.map((action): [string, number, number, number, number] => {
          const range = getPlayerViewFrameRange(view, action);
          const rate = getActionFrameRate(key.replace(/^player_/, ''), action);
          const repeat = action === 'idle' || action === 'walk' ? -1 : 0;
          return [playerAnimKey('', view, action).slice(1), range.start, PLAYER_ACTION_FRAME_COUNTS[action], rate, repeat];
        }))
      : this.monsterSheetViews(key).flatMap((view, vi): [string, number, number, number, number][] => {
          const base = vi * MONSTER_VIEW_FRAMES;
          const name = (action: string): string => playerAnimKey('', view, action).slice(1);
          return [
            [name('idle'), base + IDLE_START, IDLE_COUNT, 6, -1],
            [name('walk'), base + WALK_START, WALK_COUNT, 10, -1],
            [name('attack'), base + ATK_START, ATK_COUNT, 12, 0],
            [name('hurt'), base + HURT_START, HURT_COUNT, 10, 0],
            [name('death'), base + DEATH_START, DEATH_COUNT, 6, 0],
          ];
        });
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
