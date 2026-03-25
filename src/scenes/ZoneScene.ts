import Phaser from 'phaser';
import { TILE_WIDTH, TILE_HEIGHT, GAME_WIDTH, GAME_HEIGHT, TEXTURE_SCALE, DPR } from '../config';
import { cartToIso, isoToCart, worldToTile, euclideanDistance } from '../utils/IsometricUtils';
import { randomInt } from '../utils/MathUtils';
import { EventBus, GameEvents } from '../utils/EventBus';
import { Player } from '../entities/Player';
import { Monster } from '../entities/Monster';
import { NPC } from '../entities/NPC';
import { PathfindingSystem } from '../systems/PathfindingSystem';
import { CombatSystem, getSkillManaCost, getSkillAoeRadius, getSkillBuffValue, getSkillBuffDuration, getSkillCooldown, type EquipStats } from '../systems/CombatSystem';
import { LootSystem } from '../systems/LootSystem';
import { InventorySystem } from '../systems/InventorySystem';
import { QuestSystem } from '../systems/QuestSystem';
import { HomesteadSystem } from '../systems/HomesteadSystem';
import { AchievementSystem } from '../systems/AchievementSystem';
import { SaveSystem, CURRENT_SAVE_VERSION, findNearestWalkablePosition } from '../systems/SaveSystem';
import { SkillEffectSystem } from '../systems/SkillEffectSystem';
import { MobileControlsSystem, isMobileDevice } from '../systems/MobileControlsSystem';
import { LightingSystem } from '../systems/LightingSystem';
import { VFXManager } from '../systems/VFXManager';
import { WeatherSystem } from '../systems/WeatherSystem';
import { TrailRenderer } from '../systems/TrailRenderer';
import { StatusEffectSystem } from '../systems/StatusEffectSystem';
import type { StatusEffectType } from '../systems/StatusEffectSystem';
import { EliteAffixSystem } from '../systems/EliteAffixSystem';
import { MercenarySystem, MERCENARY_DEFS } from '../systems/MercenarySystem';
import type { MercenaryAIAction } from '../systems/MercenarySystem';
import { RandomEventSystem, RANDOM_EVENT_DEFS, ZONE_EVENT_DATA } from '../systems/RandomEventSystem';
import type { ActiveEvent, RandomEventType } from '../systems/RandomEventSystem';
import { audioManager } from '../systems/audio/AudioManager';
import { applyColorGrading } from '../graphics/ColorGradePipeline';
import { SpriteGenerator } from '../graphics/SpriteGenerator';
import { setCurrentZonePalette } from '../graphics/ZonePalette';
import { AllClasses } from '../data/classes/index';
import { AllMaps } from '../data/maps/index';
import { MonstersByZone, getMonsterDef } from '../data/monsters/index';
import { MiniBossByZone, MiniBossDialogues, MiniBossSpawns } from '../data/miniBosses';
import { LoreByZone } from '../data/loreCollectibles';
import type { LoreEntry } from '../data/loreCollectibles';
import { NPCDefinitions } from '../data/npcs';
import { AllQuests } from '../data/quests/all_quests';
import type { MapData, ClassDefinition, ItemInstance, SaveData, HiddenArea, SubDungeonEntrance, StoryDecoration, SubDungeonMapData } from '../data/types';
import { AllSubDungeons, SubDungeonMiniBosses } from '../data/subDungeons';
import { DungeonSystem } from '../systems/DungeonSystem';
import type { DungeonRunState, DungeonFloorConfig } from '../systems/DungeonSystem';
import { DifficultySystem, DIFFICULTY_UNLOCK_MESSAGES } from '../systems/DifficultySystem';
import { DungeonBossDef, DungeonMidBossDef } from '../data/dungeonData';
import type { UIScene } from './UIScene';

const TILE_KEYS = ['tile_grass', 'tile_dirt', 'tile_stone', 'tile_water', 'tile_wall', 'tile_camp', 'tile_camp_wall'];
const CAMPFIRE_RECOVERY_RADIUS = 5;
const CAMPFIRE_HP_REGEN_MULTIPLIER = 50;
const CAMPFIRE_MANA_REGEN_MULTIPLIER = 50;
const ZONE_FLOATING_TEXT_DEPTH = 4500;
const ZONE_SCREEN_UI_DEPTH = 5000;

function fs(basePx: number): string {
  return `${Math.round(basePx * DPR)}px`;
}

const W = GAME_WIDTH * DPR;
const H = GAME_HEIGHT * DPR;

export class ZoneScene extends Phaser.Scene {
  player!: Player;
  private monsters: Monster[] = [];
  private npcs: NPC[] = [];
  private mapData!: MapData;
  private currentMapId!: string;
  private pathfinding!: PathfindingSystem;
  private combatSystem!: CombatSystem;
  private skillEffects!: SkillEffectSystem;
  lootSystem!: LootSystem;
  inventorySystem!: InventorySystem;
  questSystem!: QuestSystem;
  homesteadSystem!: HomesteadSystem;
  achievementSystem!: AchievementSystem;
  saveSystem!: SaveSystem;
  private tileSprites: (Phaser.GameObjects.Image | null)[][] = [];
  private decorSprites: Map<string, Phaser.GameObjects.Image> = new Map();
  private exitSprites: Map<string, Phaser.GameObjects.Image> = new Map();
  private campDecorSprites: Map<string, Phaser.GameObjects.GameObject> = new Map();
  private campParticles: Map<string, Phaser.GameObjects.Particles.ParticleEmitter> = new Map();
  private campDecorPositions: { col: number; row: number; type: string }[] = [];
  private tileWorldPositions: { x: number; y: number }[][] = [];
  private decorWorldPositions: Array<{ key: string; type: string; x: number; y: number }> = [];
  private campDecorWorldPositions: Array<{ key: string; type: string; x: number; y: number }> = [];
  private exitLookup: Map<string, MapData['exits'][number]> = new Map();
  private visibleTiles: Set<string> = new Set();
  private cursors!: Phaser.Types.Input.Keyboard.CursorKeys;
  private wasd!: Record<string, Phaser.Input.Keyboard.Key>;
  private campPositions: { col: number; row: number }[] = [];
  private lootDrops: { sprite: Phaser.GameObjects.Container; item: ItemInstance; col: number; row: number }[] = [];
  private potionDrops: { sprite: Phaser.GameObjects.Container; type: 'hp' | 'mp'; amount: number; col: number; row: number }[] = [];
  private difficulty: 'normal' | 'nightmare' | 'hell' = 'normal';
  private completedDifficulties: string[] = [];
  private cachedEquipStats: EquipStats | null = null;
  private _deathSaveUsed = false;
  private _dodgeCounterReady = false;
  private lastAutoLootCheck = 0;
  private totalKills = 0;
  private exploredZones: Set<string> = new Set();
  private fogData: Record<string, boolean[][]> = {};
  /** Set of tile coordinates (as "col,row") that the player has explored (within view radius). */
  private exploredTiles: Set<string> = new Set();
  /** View radius used for fog-of-war exploration tracking (matches FogOfWarSystem default). */
  private static readonly EXPLORE_VIEW_RADIUS = 10;
  private lastTileUpdate = 0;
  private _pendingSaveData: SaveData | null = null;
  private mobileControls: MobileControlsSystem | null = null;
  private lighting!: LightingSystem;
  private lights_playerLight: import('../systems/LightingSystem').LightSource | null = null;
  private vfx!: VFXManager;
  private weather!: WeatherSystem;
  private trails!: TrailRenderer;
  statusEffects!: StatusEffectSystem;
  eliteAffixSystem!: EliteAffixSystem;
  mercenarySystem!: MercenarySystem;
  private mercenarySprite: Phaser.GameObjects.Container | null = null;
  private mercenaryHpBar: Phaser.GameObjects.Rectangle | null = null;
  private mercenaryHpBarBg: Phaser.GameObjects.Rectangle | null = null;
  private mercenaryNameLabel: Phaser.GameObjects.Text | null = null;
  private petSprite: Phaser.GameObjects.Container | null = null;
  private petNameLabel: Phaser.GameObjects.Text | null = null;
  private petTileCol = 0;
  private petTileRow = 0;
  private petSpawnSprites: { sprite: Phaser.GameObjects.Container; col: number; row: number; petId: string }[] = [];
  private inCombat = false;
  private randomEventSystem!: RandomEventSystem;
  private isTransitioning = false;
  private isPortaling = false;
  /** Mini-boss monster reference per zone (spawned at a fixed position). */
  private miniBossMonster: Monster | null = null;
  /** Set of mini-boss IDs whose pre-fight dialogue has been seen (persisted in save). */
  private miniBossDialogueSeen: Set<string> = new Set();
  /** Whether the mini-boss dialogue is currently being shown. */
  private miniBossDialogueActive = false;
  /** Lore collectible sprites placed in the current zone. */
  private loreSprites: { sprite: Phaser.GameObjects.Container; entry: LoreEntry }[] = [];
  /** Set of lore entry IDs that have been collected (persisted in save). */
  private loreCollected: Set<string> = new Set();
  /** Track which entities have status tints applied (entityId -> Set of applied tint types) */
  private statusTintApplied: Map<string, Set<string>> = new Map();
  private combatDebounceTimer: ReturnType<typeof setTimeout> | null = null;
  private targetIndicator: Phaser.GameObjects.Ellipse | null = null;
  private currentTargetId: string | null = null;
  private ambientDustEmitter: Phaser.GameObjects.Particles.ParticleEmitter | null = null;
  // ─── Zone Content Wiring ───────────────────────────────────────────────
  /** Hidden areas that have been discovered in the current session (persisted via save). */
  private discoveredHiddenAreas: Set<string> = new Set();
  /** Sprites for hidden area reward chests/gold piles that have been revealed. */
  private hiddenAreaSprites: { sprite: Phaser.GameObjects.Container; area: HiddenArea; rewardIndex: number; col: number; row: number }[] = [];
  /** Sub-dungeon entrance portal sprites. */
  private subDungeonEntranceSprites: { sprite: Phaser.GameObjects.Container; entrance: SubDungeonEntrance; col: number; row: number }[] = [];
  /** Story decoration sprites with interaction tooltips. */
  private storyDecorationSprites: { sprite: Phaser.GameObjects.Container; decoration: StoryDecoration; col: number; row: number }[] = [];
  /** Active tooltip for story decorations. */
  private storyDecorationTooltip: Phaser.GameObjects.Container | null = null;
  // ─── Escort / Defend Quest Runtime ──────────────────────────────────
  /** Escort NPC sprite + state for active escort quests. */
  private escortNpcSprite: Phaser.GameObjects.Container | null = null;
  private escortNpcHpBar: Phaser.GameObjects.Rectangle | null = null;
  private escortNpcHpBarBg: Phaser.GameObjects.Rectangle | null = null;
  private escortNpcNameLabel: Phaser.GameObjects.Text | null = null;
  private escortNpcTileCol = 0;
  private escortNpcTileRow = 0;
  private escortNpcHp = 0;
  private escortNpcMaxHp = 0;
  private escortQuestId: string | null = null;
  private escortDestCol = 0;
  private escortDestRow = 0;

  /** Defend target sprite + state for active defend quests. */
  private defendTargetSprite: Phaser.GameObjects.Container | null = null;
  private defendTargetHpBar: Phaser.GameObjects.Rectangle | null = null;
  private defendTargetHpBarBg: Phaser.GameObjects.Rectangle | null = null;
  private defendTargetNameLabel: Phaser.GameObjects.Text | null = null;
  private defendTargetHp = 0;
  private defendTargetMaxHp = 0;
  private defendQuestId: string | null = null;
  private defendCurrentWave = 0;
  private defendTotalWaves = 0;
  private defendWaveTimer = 0;
  private defendWaveActive = false;
  private defendWaveMonsters: Monster[] = [];
  private defendTargetCol = 0;
  private defendTargetRow = 0;

  /** Whether we are currently inside a sub-dungeon. */
  private isInSubDungeon = false;
  /** The parent zone info for returning from a sub-dungeon. */
  private parentZoneInfo: { mapId: string; returnCol: number; returnRow: number } | null = null;

  // ─── Random Dungeon State ─────────────────────────────────────────────
  /** Whether we are currently inside a random dungeon floor. */
  private isInDungeon = false;
  /** Active dungeon run state (null when not in dungeon). */
  private dungeonRunState: DungeonRunState | null = null;
  /** Current dungeon floor config (null when not in dungeon). */
  private dungeonFloorConfig: DungeonFloorConfig | null = null;
  /** Dungeon portal sprite in Abyss Rift. */
  private dungeonPortalSprite: Phaser.GameObjects.Container | null = null;
  /** Dungeon portal position in Abyss Rift. */
  private static readonly DUNGEON_PORTAL_COL = 60;
  private static readonly DUNGEON_PORTAL_ROW = 60;
  /** Abyss Rift entrance (first camp) — used as save position when inside dungeon. */
  private static readonly ABYSS_ENTRANCE_COL = 15;
  private static readonly ABYSS_ENTRANCE_ROW = 22;

  private readonly contextMenuHandler = (e: Event): void => {
    e.preventDefault();
  };

  constructor() {
    super({ key: 'ZoneScene' });
  }

  init(data: { classId: string; mapId: string; saveData?: SaveData; playerStats?: any; subDungeon?: SubDungeonMapData; parentZoneInfo?: { mapId: string; returnCol: number; returnRow: number }; discoveredHiddenAreas?: string[]; targetCol?: number; targetRow?: number; dungeonRun?: DungeonRunState; dungeonFloor?: DungeonFloorConfig }): void {
    this.currentMapId = data.mapId || 'emerald_plains';
    this.isInSubDungeon = !!data.subDungeon;
    this.parentZoneInfo = data.parentZoneInfo ?? null;
    this.isInDungeon = !!data.dungeonRun;
    this.dungeonRunState = data.dungeonRun ?? null;
    this.dungeonFloorConfig = data.dungeonFloor ?? null;
    if (data.dungeonRun && data.dungeonFloor) {
      // For random dungeon floors, generate the floor map procedurally
      this.mapData = DungeonSystem.generateFloorMap(data.dungeonFloor);
      this.currentMapId = this.mapData.id;
    } else if (data.subDungeon) {
      // For sub-dungeons, we generate a simple map on the fly
      this.mapData = this.generateSubDungeonMap(data.subDungeon);
    } else {
      if (!AllMaps[this.currentMapId]) this.currentMapId = 'emerald_plains';
      this.mapData = AllMaps[this.currentMapId];
    }
    this.campPositions = this.mapData.camps.map(c => ({ col: c.col, row: c.row }));
    // Set the active zone color palette for sprite generation
    setCurrentZonePalette(this.mapData.theme ?? 'plains');
    this._pendingSaveData = data.saveData ?? null;
    if (data.discoveredHiddenAreas) {
      this.discoveredHiddenAreas = new Set(data.discoveredHiddenAreas);
    }
  }

  create(data: { classId: string; mapId: string; saveData?: SaveData; playerStats?: any; miniBossDialogueSeen?: string[]; loreCollected?: string[]; subDungeon?: SubDungeonMapData; parentZoneInfo?: { mapId: string; returnCol: number; returnRow: number }; discoveredHiddenAreas?: string[]; targetCol?: number; targetRow?: number; dungeonRun?: DungeonRunState; dungeonFloor?: DungeonFloorConfig }): void {
    this.events.once(Phaser.Scenes.Events.SHUTDOWN, this.shutdown, this);
    // `scene.restart()` reuses the same scene instance, so transient guards must
    // be cleared explicitly when entering a new zone.
    this.isTransitioning = false;
    this.isPortaling = false;
    this.miniBossDialogueActive = false;
    this.monsters = [];
    this.npcs = [];
    this.lootDrops = [];
    this.potionDrops = [];
    this.statusTintApplied.clear();
    // Clean up zone content wiring fields
    this.hiddenAreaSprites = [];
    this.subDungeonEntranceSprites = [];
    this.storyDecorationSprites = [];
    this.storyDecorationTooltip = null;
    this.exploredTiles = new Set();

    this.combatSystem = new CombatSystem();
    this.lootSystem = new LootSystem();
    this.skillEffects = new SkillEffectSystem(this);
    this.statusEffects = new StatusEffectSystem();
    this.eliteAffixSystem = new EliteAffixSystem();
    this.randomEventSystem = new RandomEventSystem(
      { zoneId: this.currentMapId, levelRange: this.mapData.levelRange as [number, number] },
      { safeZoneRadius: this.mapData.safeZoneRadius ?? 9 },
    );

    const isFirstLoad = !this.inventorySystem;
    if (isFirstLoad) {
      this.inventorySystem = new InventorySystem();
      this.questSystem = new QuestSystem();
      this.homesteadSystem = new HomesteadSystem();
      this.achievementSystem = new AchievementSystem();
      this.saveSystem = new SaveSystem();
      this.mercenarySystem = new MercenarySystem();
      this.questSystem.registerQuests(AllQuests);
    }

    // Initialize tile sprite grid
    this.tileSprites = [];
    for (let r = 0; r < this.mapData.rows; r++) {
      this.tileSprites.push(new Array(this.mapData.cols).fill(null));
    }
    this.visibleTiles = new Set();
    this.decorSprites = new Map();
    this.exitSprites = new Map();
    this.campDecorSprites = new Map();
    this.campParticles = new Map();
    this.ambientDustEmitter = null;

    // Create player — restore stats from zone transition if available
    const classData = AllClasses[data.classId] || AllClasses['warrior'];
    const spawnCol = data.targetCol ?? this.mapData.playerStart.col;
    const spawnRow = data.targetRow ?? this.mapData.playerStart.row;
    this.player = new Player(this, classData, spawnCol, spawnRow);
    if (data.playerStats) {
      const s = data.playerStats;
      this.player.level = s.level;
      this.player.exp = s.exp;
      this.player.gold = s.gold;
      this.player.hp = s.hp;
      this.player.mana = s.mana;
      this.player.stats = s.stats;
      this.player.freeStatPoints = s.freeStatPoints;
      this.player.freeSkillPoints = s.freeSkillPoints;
      const zsl = s.skillLevels;
      this.player.skillLevels = new Map(Array.isArray(zsl) ? zsl : Object.entries(zsl));
      this.player.buffs = s.buffs;
      this.player.autoCombat = s.autoCombat;
      if (s.autoLootMode) this.player.autoLootMode = s.autoLootMode;
    }
    this.player.recalcDerived();

    // Restore mini-boss/lore state from zone transitions
    if (data.miniBossDialogueSeen) {
      this.miniBossDialogueSeen = new Set(data.miniBossDialogueSeen);
    }
    if (data.loreCollected) {
      this.loreCollected = new Set(data.loreCollected);
    }

    // Restore from save (when loading from menu, not zone transitions)
    if (this._pendingSaveData) {
      this.restoreFromSave(this._pendingSaveData);
      this._pendingSaveData = null;
    }

    this.pathfinding = new PathfindingSystem(this.mapData.collisions, this.mapData.cols, this.mapData.rows);

    this.spawnMonsters();
    this.spawnNPCs();
    this.spawnFieldNPCs();
    this.spawnRarePets();
    this.spawnMiniBoss();
    this.spawnLoreCollectibles();
    this.spawnSubDungeonEntrances();
    this.spawnStoryDecorations();
    this.spawnDungeonPortal();
    this.spawnMercenarySprite();
    this.spawnPetSprite();
    this.spawnEscortNpc();
    this.spawnDefendTarget();
    this.buildCampDecorations();
    this.rebuildWorldCaches();
    for (const decor of this.campDecorPositions) {
      if (decor.type === 'barrel' || decor.type === 'crate') {
        const dr = Math.round(decor.row);
        const dc = Math.round(decor.col);
        if (dr >= 0 && dr < this.mapData.rows && dc >= 0 && dc < this.mapData.cols) {
          this.mapData.collisions[dr][dc] = false;
        }
      }
    }

    // Initial tile render
    this.updateVisibleTiles();

    // Camera
    this.cameras.main.startFollow(this.player.sprite, true, 0.08, 0.08);
    this.cameras.main.setZoom(1.8);

    // Lighting system — ambient darkness + point lights
    this.lighting = new LightingSystem(this);
    this.lighting.setZone(this.currentMapId);
    this.registerLightSources();

    // VFX Manager — camera effects, FX pipeline, combat juice
    this.vfx = new VFXManager(this);

    // Player ambient light — subtle glow under player for visibility on dark maps
    if (this.renderer.type === Phaser.WEBGL) {
      const playerSprite = this.player.sprite.list[0] as Phaser.GameObjects.Sprite;
      if (playerSprite?.preFX) {
        playerSprite.preFX.addGlow(0xccddff, 3, 0, false, 0.08);
      }
    }

    // Weather system — per-zone weather + environmental ambience
    this.weather = new WeatherSystem(this);
    this.weather.setZone(this.currentMapId);

    // Trail renderer — weapon trails, ground scorch marks
    this.trails = new TrailRenderer(this);

    // Camera PostFX — painterly mood (WebGL only)
    if (this.renderer.type === Phaser.WEBGL) {
      const cam = this.cameras.main;

      // Gentle bloom — brightens highlights (loot glow, magic, fire)
      cam.postFX.addBloom(0xffffff, 0.6, 0.6, 0.5, 0.8);

      // Very subtle vignette — barely visible edge darkening
      cam.postFX.addVignette(0.5, 0.5, 0.95, 0.12);
    }

    // Ambient dust particles
    this.createAmbientDust();

    // Fade in from black on zone entry
    this.cameras.main.fadeIn(400);

    // Color grading shader (WebGL only, no-op on Canvas)
    applyColorGrading(this);

    // Input
    if (this.input.keyboard) {
      this.cursors = this.input.keyboard.createCursorKeys();
      this.wasd = {
        W: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.W),
        A: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.A),
        S: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.S),
        D: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.D),
        ONE: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.ONE),
        TWO: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.TWO),
        THREE: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.THREE),
        FOUR: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.FOUR),
        FIVE: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.FIVE),
        SIX: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.SIX),
        TAB: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.TAB),
        I: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.I),
        K: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.K),
        M: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.M),
        H: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.H),
        C: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.C),
        J: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.J),
        O: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.O),
        R: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.R),
        P: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.P),
        V: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.V),
        ESC: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.ESC),
      };
    }

    if (import.meta.env.DEV) {
      const exportKey = this.input.keyboard!.addKey(
        Phaser.Input.Keyboard.KeyCodes.E
      );
      exportKey.on('down', async (event: KeyboardEvent) => {
        if (event.ctrlKey && event.shiftKey) {
          const { TextureExporter } = await import('../graphics/TextureExporter');
          new TextureExporter(this).exportAll();
        }
      });
    }

    // Mobile controls
    if (isMobileDevice()) {
      this.mobileControls = new MobileControlsSystem(this, this.player);
    }

    this.game.canvas.addEventListener('contextmenu', this.contextMenuHandler);
    this.input.on('pointerdown', this.handlePointerDown, this);

    if (!this.scene.isActive('UIScene')) {
      this.scene.launch('UIScene', { player: this.player, zone: this });
    } else {
      EventBus.emit('ui:refresh', { player: this.player, zone: this });
    }
    EventBus.on(GameEvents.PLAYER_DIED, this.handlePlayerDied, this);
    EventBus.on(GameEvents.PLAYER_LEVEL_UP, this.handlePlayerLevelUp, this);
    EventBus.on(GameEvents.QUEST_COMPLETED, this.handleQuestCompleted, this);
    EventBus.on(GameEvents.UI_SKILL_CLICK, this.handleUiSkillClick, this);

    this.exploredZones.add(this.currentMapId);
    this.achievementSystem.update('explore', this.currentMapId);

    EventBus.emit(GameEvents.ZONE_ENTERED, { mapId: this.currentMapId });
    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: `进入 ${this.mapData.name} (Lv.${this.mapData.levelRange[0]}-${this.mapData.levelRange[1]})`,
      type: 'system',
    });

    this.showZoneBanner();
    this.autoSave();
  }

  private handlePointerDown(pointer: Phaser.Input.Pointer): void {
    if (pointer.rightButtonDown()) {
      this.useTownPortal();
      return;
    }
    if (this.player.hp <= 0) return;
    const tile = worldToTile(pointer.worldX, pointer.worldY);

    const loot = this.findLootAt(tile.col, tile.row);
    if (loot) { this.pickupLoot(loot); return; }

    const npc = this.findNPCAt(tile.col, tile.row);
    if (npc && npc.isNearPlayer(this.player.tileCol, this.player.tileRow, 3)) {
      this.interactNPC(npc);
      return;
    }

    // Sub-dungeon entrance interaction
    const subEntrance = this.findSubDungeonEntranceAt(tile.col, tile.row);
    if (subEntrance && euclideanDistance(this.player.tileCol, this.player.tileRow, subEntrance.col, subEntrance.row) <= 3) {
      this.enterSubDungeon(subEntrance);
      return;
    }

    // Random dungeon portal interaction
    if (this.findDungeonPortalAt(tile.col, tile.row) && euclideanDistance(this.player.tileCol, this.player.tileRow, ZoneScene.DUNGEON_PORTAL_COL, ZoneScene.DUNGEON_PORTAL_ROW) <= 3) {
      this.enterDungeon();
      return;
    }

    // Hidden area reward chest interaction
    const hiddenChest = this.findHiddenAreaChestAt(tile.col, tile.row);
    if (hiddenChest && euclideanDistance(this.player.tileCol, this.player.tileRow, hiddenChest.col, hiddenChest.row) <= 2) {
      this.collectHiddenAreaReward(hiddenChest);
      return;
    }

    const monster = this.findMonsterAt(tile.col, tile.row);
    if (monster && monster.isAlive()) {
      this.player.attackTarget = monster.id;
      const path = this.pathfinding.findPath(
        Math.round(this.player.tileCol), Math.round(this.player.tileRow),
        Math.round(monster.tileCol), Math.round(monster.tileRow),
      );
      this.player.setPath(path);
      return;
    }

    const exit = this.findExitAt(tile.col, tile.row);
    if (exit) {
      if (this.isInDungeon) {
        if (this.dungeonFloorConfig?.isBossFloor) {
          this.exitDungeon();
        } else {
          this.advanceDungeonFloor();
        }
      } else if (this.isInSubDungeon) {
        this.exitSubDungeon();
      } else {
        this.changeZone(exit.targetMap, exit.targetCol, exit.targetRow);
      }
      return;
    }

    if (tile.col >= 0 && tile.col < this.mapData.cols && tile.row >= 0 && tile.row < this.mapData.rows) {
      const path = this.pathfinding.findPath(
        Math.round(this.player.tileCol), Math.round(this.player.tileRow),
        tile.col, tile.row,
      );
      if (path.length > 0) {
        this.player.setPath(path);
        this.player.attackTarget = null;
      }
    }
  }

  private handlePlayerDied(): void {
    // Clear status effects on player death
    this.statusEffects.clearEntity('player');
    this.isPortaling = false;

    if (this.vfx) {
      this.vfx.cameraFlash(80, 0.6, 0xffffff);
      this.vfx.deathBurst(this.player.sprite.x, this.player.sprite.y - 16, 0xcc2222);
    }
    const dp = this.screenPos(0.5, 0.4);
    const deathText = this.add.text(dp.x, dp.y, '你已死亡', {
      fontSize: fs(36), color: '#cc2222', fontFamily: '"Cinzel", serif',
      fontStyle: 'bold', stroke: '#000000', strokeThickness: Math.round(6 * DPR),
    }).setOrigin(0.5).setScrollFactor(0).setDepth(ZONE_SCREEN_UI_DEPTH).setAlpha(0);
    this.tweens.add({
      targets: deathText, alpha: 1, duration: 250, ease: 'Power2',
    });
    this.time.delayedCall(1100, () => {
      this.tweens.add({
        targets: deathText, alpha: 0, duration: 250, onComplete: () => deathText.destroy(),
      });

      if (this.isInDungeon || this.isInSubDungeon) {
        // Dying in a dungeon/sub-dungeon: exit back to parent zone's campfire
        const parentMapId = this.isInDungeon ? 'abyss_rift' : this.parentZoneInfo?.mapId ?? 'emerald_plains';
        const parentMap = AllMaps[parentMapId];
        const parentCamp = parentMap?.camps[0];
        const respawnCol = parentCamp?.col ?? parentMap?.playerStart?.col ?? 3;
        const respawnRow = parentCamp?.row ?? parentMap?.playerStart?.row ?? 3;

        if (this.isInDungeon) {
          this.dungeonRunState = null;
          this.dungeonFloorConfig = null;
          this.isInDungeon = false;
          EventBus.emit(GameEvents.DUNGEON_EXIT, {});
        } else {
          EventBus.emit(GameEvents.SUBDUNGEON_EXIT, { mapId: parentMapId });
        }

        EventBus.emit(GameEvents.LOG_MESSAGE, { text: '你已死亡，返回营地复活...', type: 'system' });
        this.isTransitioning = true;
        this.scene.restart({
          classId: this.player.classData.id,
          mapId: parentMapId,
          targetCol: respawnCol,
          targetRow: respawnRow,
          miniBossDialogueSeen: [...this.miniBossDialogueSeen],
          loreCollected: [...this.loreCollected],
          discoveredHiddenAreas: [...this.discoveredHiddenAreas],
          playerStats: {
            level: this.player.level,
            exp: this.player.exp,
            gold: this.player.gold,
            hp: this.player.maxHp,
            mana: this.player.maxMana,
            stats: { ...this.player.stats },
            freeStatPoints: this.player.freeStatPoints,
            freeSkillPoints: this.player.freeSkillPoints,
            skillLevels: Object.fromEntries(this.player.skillLevels),
            buffs: [...this.player.buffs],
            autoCombat: this.player.autoCombat,
            autoLootMode: this.player.autoLootMode,
          },
        });
      } else {
        const camp = this.campPositions[0];
        this.player.respawnAtCamp(camp.col, camp.row);
        this.cameras.main.fadeIn(300);
      }
    });
  }

  private handlePlayerLevelUp(data: { level: number }): void {
    this.showLevelUpBanner(data.level);
    // Check level achievements from ALL level-up sources (monster kills, quest rewards, dialogue exp, etc.)
    this.achievementSystem.checkLevel(data.level);
  }

  private handleQuestCompleted(data: { questName: string }): void {
    this.showQuestCompleteBanner(data.questName);
  }

  private handleUiSkillClick(data: { index: number; skillId: string }): void {
    this.tryUseSkill(data.skillId, this.time.now);
  }

  update(time: number, delta: number): void {
    this.cachedEquipStats = null;
    const recovery = this.getPlayerRecoveryModifiers();
    this.handleKeyboardMovement(delta);
    this.handleSkillInput(time);
    const eqStats = this.getEquipStats();
    this.player.recalcDerived(eqStats);

    // Poison heal reduction: halve HP regen while poisoned
    if (this.statusEffects && this.statusEffects.hasPoisonHealReduction('player')) {
      recovery.hpRegenMultiplier = (recovery.hpRegenMultiplier ?? 1) * 0.5;
    }

    // ── Passive skill wiring ────────────────────────────────
    // Life Regen passive: adds extra HP regen per second based on skill level
    const lifeRegenLevel = this.player.getSkillLevel('life_regen');
    if (lifeRegenLevel > 0) {
      const regenBonus = lifeRegenLevel * 2; // +2 HP/s per level
      if (this.player.hp < this.player.maxHp && this.player.hp > 0) {
        this.player.hp = Math.min(this.player.maxHp, this.player.hp + regenBonus * delta / 1000 * (recovery.hpRegenMultiplier ?? 1));
      }
    }

    // Unyielding passive: auto-trigger damage reduction when HP < 30%
    const unyieldingLevel = this.player.getSkillLevel('unyielding');
    if (unyieldingLevel > 0 && this.player.hp > 0) {
      const hpRatio = this.player.hp / this.player.maxHp;
      const unyieldingSkill = this.player.getSkill('unyielding');
      if (hpRatio < 0.3 && unyieldingSkill && this.player.isSkillReady('unyielding', time)) {
        const buffValue = getSkillBuffValue(unyieldingSkill, unyieldingLevel);
        const buffDuration = getSkillBuffDuration(unyieldingSkill, unyieldingLevel);
        this.player.buffs.push({ stat: 'damageReduction', value: buffValue, duration: buffDuration, startTime: time });
        this.player.skillCooldowns.set('unyielding', time + getSkillCooldown(unyieldingSkill, unyieldingLevel, eqStats.cooldownReduction));
        this.skillEffects.play('unyielding', this.player.sprite.x, this.player.sprite.y);
        EventBus.emit(GameEvents.LOG_MESSAGE, { text: '不屈触发! 获得减伤效果', type: 'combat' });
      }
    }

    // Dual Wield Mastery passive: damage bonus when weapon + offhand both equipped
    const dualWieldLevel = this.player.getSkillLevel('dual_wield_mastery');
    if (dualWieldLevel > 0) {
      const hasWeapon = !!this.inventorySystem.equipment['weapon'];
      const hasOffhand = !!this.inventorySystem.equipment['offhand'];
      if (hasWeapon && hasOffhand) {
        // Check if buff is already active (avoid stacking multiple copies)
        const hasDWBuff = this.player.buffs.some(b => b.tag === 'dualWieldMastery');
        if (!hasDWBuff) {
          // +3% damage per level via a persistent buff that refreshes
          const bonusValue = dualWieldLevel * 0.03;
          this.player.buffs.push({ stat: 'damageBonus', value: bonusValue, duration: 2000, startTime: time, tag: 'dualWieldMastery' });
        }
      }
    }

    this.player.update(time, delta, recovery, eqStats);

    // ── Mini-boss pre-fight dialogue check ──────────────────
    this.checkMiniBossDialogue();

    const safeRadius = this.mapData.safeZoneRadius ?? 9;
    for (const monster of this.monsters) {
      if (!monster.isAlive()) continue;

      // Freeze mini-boss during dialogue
      if (this.miniBossDialogueActive && monster === this.miniBossMonster) {
        monster.animator.update(delta);
        continue;
      }

      // Status effects: immobilized monsters skip update entirely
      if (this.statusEffects.isImmobilized(monster.id)) {
        // Still update animator for visual state
        monster.animator.update(delta);
        continue;
      }

      // Safe zone: repel aggro monsters
      let monsterInSafe = false;
      for (const camp of this.campPositions) {
        if (euclideanDistance(monster.tileCol, monster.tileRow, camp.col, camp.row) < safeRadius) {
          monsterInSafe = true;
          break;
        }
      }
      if (monsterInSafe && monster.isAggro()) {
        monster.state = 'idle';
      }
      // Player in safe zone: suppress aggro by passing fake coordinates
      let playerInSafe = false;
      for (const camp of this.campPositions) {
        if (euclideanDistance(this.player.tileCol, this.player.tileRow, camp.col, camp.row) < safeRadius) {
          playerInSafe = true;
          break;
        }
      }
      // Get speed multiplier from StatusEffectSystem (Slow effect)
      const speedMult = this.statusEffects.getSpeedMultiplier(monster.id);

      if (playerInSafe && !monster.isAggro()) {
        monster.update(time, delta, -999, -999, this.mapData.collisions, speedMult);
      } else {
        monster.update(time, delta, this.player.tileCol, this.player.tileRow, this.mapData.collisions, speedMult);
      }
    }

    // Update NPC state machines
    for (const npc of this.npcs) {
      npc.update(this.player.tileCol, this.player.tileRow);
    }

    this.handleCombat(time);
    this.handleMercenaryCombat(time);
    this.updateMercenary(time, delta);
    this.updatePetFollower(time, delta);
    this.handlePetCombat(time);
    this.updateEscortNpc(time, delta);
    this.updateDefendQuest(time, delta);
    this.updateEliteAffixBehaviors(time);
    this.updateStatusEffects(time);
    this.updateCombatState();
    this.checkRandomEvents(time, delta);
    this.updateTargetIndicator();
    if (this.vfx) this.vfx.updateDangerVignette(this.player.hp / this.player.maxHp);
    if (this.mobileControls) this.mobileControls.update(time, delta);

    if (this.player.autoCombat) this.handleAutoCombat(time);

    // Auto-collect potions
    for (let i = this.potionDrops.length - 1; i >= 0; i--) {
      const pot = this.potionDrops[i];
      const dist = euclideanDistance(this.player.tileCol, this.player.tileRow, pot.col, pot.row);
      if (dist <= 2) {
        if (pot.type === 'hp') {
          this.player.hp = Math.min(this.player.maxHp, this.player.hp + pot.amount);
          EventBus.emit(GameEvents.LOG_MESSAGE, { text: `恢复 ${pot.amount} 生命`, type: 'combat' });
        } else {
          this.player.mana = Math.min(this.player.maxMana, this.player.mana + pot.amount);
          EventBus.emit(GameEvents.LOG_MESSAGE, { text: `恢复 ${pot.amount} 法力`, type: 'info' });
        }
        this.tweens.killTweensOf(pot.sprite);
        this.tweens.add({
          targets: pot.sprite,
          x: this.player.sprite.x,
          y: this.player.sprite.y - 20,
          scale: 0.3, alpha: 0, duration: 250, ease: 'Power2',
          onComplete: () => pot.sprite.destroy(),
        });
        this.potionDrops.splice(i, 1);
      }
    }

    // Auto-loot
    if (this.player.autoLootMode !== 'off' && time - this.lastAutoLootCheck > 300) {
      this.lastAutoLootCheck = time;
      this.handleAutoLoot();
    }

    this.checkExitProximity();
    this.checkRarePetPickup();
    this.checkLorePickup();
    this.updateExploredTiles();
    this.checkHiddenAreaDiscovery();
    this.checkStoryDecorationProximity();
    this.checkSubDungeonEntranceProximity();

    // Throttled viewport tile update
    if (time - this.lastTileUpdate > 100) {
      this.lastTileUpdate = time;
      this.updateVisibleTiles();
    }

    // Throttled explore quest check + NPC quest marker update
    if (Math.floor(time / 500) !== Math.floor((time - delta) / 500)) {
      this.checkExploreQuests();
      this.updateNPCQuestMarkers();
    }

    // Update trail renderer (fade per frame)
    if (this.trails) this.trails.update();

    // Update lighting — player light follows player, then render
    if (this.lighting) {
      const playerLight = this.lights_playerLight;
      if (playerLight) {
        playerLight.x = this.player.sprite.x;
        playerLight.y = this.player.sprite.y;
      }
      this.lighting.update(delta);
    }

    EventBus.emit(GameEvents.PLAYER_HEALTH_CHANGED, { hp: this.player.hp, maxHp: this.player.maxHp });
    EventBus.emit(GameEvents.PLAYER_MANA_CHANGED, { mana: this.player.mana, maxMana: this.player.maxMana });
  }

  private getPlayerRecoveryModifiers(): { hpRegenMultiplier?: number; manaRegenMultiplier?: number } {
    for (const camp of this.campPositions) {
      if (euclideanDistance(this.player.tileCol, this.player.tileRow, camp.col, camp.row) <= CAMPFIRE_RECOVERY_RADIUS) {
        return {
          hpRegenMultiplier: CAMPFIRE_HP_REGEN_MULTIPLIER,
          manaRegenMultiplier: CAMPFIRE_MANA_REGEN_MULTIPLIER,
        };
      }
    }
    return {};
  }

  private rebuildWorldCaches(): void {
    this.tileWorldPositions = Array.from({ length: this.mapData.rows }, (_, row) =>
      Array.from({ length: this.mapData.cols }, (_, col) => cartToIso(col, row))
    );
    this.decorWorldPositions = (this.mapData.decorations ?? []).map((decor, index) => {
      const pos = cartToIso(decor.col, decor.row);
      return {
        key: `d_${index}`,
        type: decor.type,
        x: pos.x,
        y: pos.y,
      };
    });
    this.campDecorWorldPositions = this.campDecorPositions.map((decor) => {
      const pos = cartToIso(decor.col, decor.row);
      return {
        key: `camp_${decor.col}_${decor.row}_${decor.type}`,
        type: decor.type,
        x: pos.x,
        y: pos.y,
      };
    });
    this.exitLookup = new Map(this.mapData.exits.map(exit => [`${exit.col},${exit.row}`, exit]));
  }

  private getExpandedWorldBounds(marginX: number, marginY: number): { left: number; right: number; top: number; bottom: number } {
    const wv = this.cameras.main.worldView;
    return {
      left: wv.x - marginX,
      right: wv.x + wv.width + marginX,
      top: wv.y - marginY,
      bottom: wv.y + wv.height + marginY,
    };
  }

  private getVisibleTileBounds(marginTiles: number): {
    left: number;
    right: number;
    top: number;
    bottom: number;
    minCol: number;
    maxCol: number;
    minRow: number;
    maxRow: number;
  } {
    const marginX = TILE_WIDTH * marginTiles;
    const marginY = TILE_HEIGHT * marginTiles;
    const bounds = this.getExpandedWorldBounds(marginX, marginY);
    const corners = [
      isoToCart(bounds.left, bounds.top),
      isoToCart(bounds.right, bounds.top),
      isoToCart(bounds.left, bounds.bottom),
      isoToCart(bounds.right, bounds.bottom),
    ];
    const cols = corners.map(corner => corner.x);
    const rows = corners.map(corner => corner.y);

    return {
      ...bounds,
      minCol: Math.max(0, Math.floor(Math.min(...cols)) - 1),
      maxCol: Math.min(this.mapData.cols - 1, Math.ceil(Math.max(...cols)) + 1),
      minRow: Math.max(0, Math.floor(Math.min(...rows)) - 1),
      maxRow: Math.min(this.mapData.rows - 1, Math.ceil(Math.max(...rows)) + 1),
    };
  }

  // --- Viewport culling tile rendering ---
  private updateVisibleTiles(): void {
    const margin = 4;
    const { left, right, top, bottom, minCol, maxCol, minRow, maxRow } = this.getVisibleTileBounds(margin);
    const newVisible = new Set<string>();

    for (let row = minRow; row <= maxRow; row++) {
      for (let col = minCol; col <= maxCol; col++) {
        const pos = this.tileWorldPositions[row][col];
        if (pos.x < left - TILE_WIDTH || pos.x > right + TILE_WIDTH ||
            pos.y < top - TILE_HEIGHT || pos.y > bottom + TILE_HEIGHT) {
          continue;
        }

        const key = `${col},${row}`;
        newVisible.add(key);
        if (!this.tileSprites[row][col]) {
          const tileType = this.mapData.tiles[row][col];
          const tiles = this.mapData.tiles;
          const tr = row > 0 ? tiles[row - 1][col] : tileType;
          const tl = col > 0 ? tiles[row][col - 1] : tileType;
          const br = col < this.mapData.cols - 1 ? tiles[row][col + 1] : tileType;
          const bl = row < this.mapData.rows - 1 ? tiles[row + 1][col] : tileType;
          const needsBlend = tr !== tileType || tl !== tileType || br !== tileType || bl !== tileType;
          let tileKey: string;
          if (needsBlend) {
            tileKey = SpriteGenerator.generateTransitionTile(this, tileType, [tr, tl, br, bl]);
          } else if (tileType === 5 && this.mapData.theme) {
            tileKey = `tile_camp_ground_${this.mapData.theme}`;
            if (!this.textures.exists(tileKey)) tileKey = 'tile_camp';
          } else if (tileType === 6 && this.mapData.theme) {
            tileKey = `tile_camp_wall_${this.mapData.theme}`;
            if (!this.textures.exists(tileKey)) tileKey = 'tile_camp_wall';
          } else {
            const variantCount = SpriteGenerator.TILE_VARIANTS;
            const variant = ((col * 374761393 + row * 668265263) >>> 0) % variantCount;
            const variantKey = `${TILE_KEYS[tileType] || 'tile_grass'}_${variant}`;
            tileKey = this.textures.exists(variantKey) ? variantKey : (TILE_KEYS[tileType] || 'tile_grass');
          }
          const tile = this.add.image(pos.x, pos.y, tileKey).setScale(1 / TEXTURE_SCALE);
          tile.setDepth(pos.y);
          this.tileSprites[row][col] = tile;

          const exit = this.exitLookup.get(key);
          if (exit) {
            SpriteGenerator.ensureEffect(this, 'exit_portal');
            if (this.textures.exists('exit_portal')) {
              const portal = this.add.image(pos.x, pos.y - 8, 'exit_portal').setScale(1 / TEXTURE_SCALE);
              portal.setDepth(pos.y + 2);
              this.exitSprites.set(key, portal);
              if (this.vfx) {
                this.vfx.applyGlow(portal, 0x4488ff, 8, 0.1);
                this.vfx.applyBloom(portal, 0.8);
              }
              // Add floor label for dungeon exit portals
              if (this.isInDungeon && this.dungeonFloorConfig) {
                const labelText = this.dungeonFloorConfig.isBossFloor
                  ? '返回深渊裂谷'
                  : DungeonSystem.getFloorExitLabel(this.dungeonFloorConfig.floorNumber + 1);
                const exitLabel = this.add.text(pos.x, pos.y - 30 * DPR, labelText, {
                  fontSize: fs(9),
                  color: this.dungeonFloorConfig.isBossFloor ? '#66CCFF' : '#FF9933',
                  fontFamily: '"Noto Sans SC", sans-serif',
                  stroke: '#000000',
                  strokeThickness: Math.round(2 * DPR),
                }).setOrigin(0.5).setDepth(pos.y + 3);
              }
            }
          }
        }
      }
    }

    // Destroy tiles no longer visible
    for (const key of this.visibleTiles) {
      if (!newVisible.has(key)) {
        const [c, r] = key.split(',').map(Number);
        const sprite = this.tileSprites[r]?.[c];
        if (sprite) {
          sprite.destroy();
          this.tileSprites[r][c] = null;
        }
        const exitSprite = this.exitSprites.get(key);
        if (exitSprite) {
          exitSprite.destroy();
          this.exitSprites.delete(key);
        }
      }
    }
    this.visibleTiles = newVisible;

    // Update decorations visibility
    this.updateVisibleDecorations();
    this.updateCampDecorations();
  }

  private updateVisibleDecorations(): void {
    const { left, right, top, bottom } = this.getExpandedWorldBounds(TILE_WIDTH * 5, TILE_HEIGHT * 5);
    const visibleDecorKeys = new Set<string>();

    for (const decor of this.decorWorldPositions) {
      if (decor.x < left || decor.x > right || decor.y < top || decor.y > bottom) continue;

      visibleDecorKeys.add(decor.key);
      if (!this.decorSprites.has(decor.key)) {
        const texKey = `decor_${decor.type}`;
        SpriteGenerator.ensureDecoration(this, decor.type);
        if (this.textures.exists(texKey)) {
          const sprite = this.add.image(decor.x, decor.y - 6, texKey).setScale(1 / TEXTURE_SCALE);
          sprite.setDepth(decor.y + 20);
          this.decorSprites.set(decor.key, sprite);
        }
      }
    }

    // Remove out-of-view decorations
    for (const [key, sprite] of this.decorSprites) {
      if (!visibleDecorKeys.has(key)) {
        sprite.destroy();
        this.decorSprites.delete(key);
      }
    }
  }

  private buildCampDecorations(): void {
    this.campDecorPositions = [];
    for (const camp of this.mapData.camps) {
      const c = camp.col, r = camp.row;
      this.campDecorPositions.push({ col: c, row: r, type: 'campfire' });
      this.campDecorPositions.push({ col: c + 1, row: r - 1, type: 'well' });
      // Banners on walls
      this.campDecorPositions.push({ col: c - 1, row: r - 4, type: 'banner' });
      this.campDecorPositions.push({ col: c + 2, row: r - 4, type: 'banner' });
      // Tents
      this.campDecorPositions.push({ col: c - 3, row: r - 2, type: 'tent' });
      this.campDecorPositions.push({ col: c + 3, row: r - 2, type: 'tent' });
      this.campDecorPositions.push({ col: c - 2, row: r + 2, type: 'tent' });
      this.campDecorPositions.push({ col: c + 2, row: r + 2, type: 'tent' });
      // Barrels/crates
      this.campDecorPositions.push({ col: c - 2, row: r, type: 'barrel' });
      this.campDecorPositions.push({ col: c + 2, row: r, type: 'crate' });
      this.campDecorPositions.push({ col: c - 3, row: r - 3, type: 'crate' });
      this.campDecorPositions.push({ col: c + 3, row: r - 3, type: 'barrel' });
      // Entrance banners
      this.campDecorPositions.push({ col: c - 5, row: r + 4, type: 'banner' });
      this.campDecorPositions.push({ col: c + 5, row: r + 4, type: 'banner' });
      // Entrance torches (offset 1 row south from entrance banners)
      this.campDecorPositions.push({ col: c - 5, row: r + 5, type: 'torch' });
      this.campDecorPositions.push({ col: c + 5, row: r + 5, type: 'torch' });
      // Wall torches
      this.campDecorPositions.push({ col: c - 5, row: r - 2, type: 'torch' });
      this.campDecorPositions.push({ col: c - 5, row: r + 1, type: 'torch' });
      this.campDecorPositions.push({ col: c + 5, row: r - 2, type: 'torch' });
      this.campDecorPositions.push({ col: c + 5, row: r + 1, type: 'torch' });
      this.campDecorPositions.push({ col: c - 2, row: r - 5, type: 'torch' });
      this.campDecorPositions.push({ col: c + 3, row: r - 5, type: 'torch' });
    }
  }

  private createAmbientDust(): void {
    // Generate a small dust particle texture
    const dustKey = 'dust_particle';
    if (!this.textures.exists(dustKey)) {
      const c = document.createElement('canvas');
      c.width = 8; c.height = 8;
      const ctx = c.getContext('2d')!;
      const grad = ctx.createRadialGradient(4, 4, 0, 4, 4, 4);
      grad.addColorStop(0, 'rgba(200,190,170,0.3)');
      grad.addColorStop(1, 'rgba(200,190,170,0)');
      ctx.fillStyle = grad;
      ctx.fillRect(0, 0, 8, 8);
      this.textures.addCanvas(dustKey, c);
    }

    // Zone-specific tint
    const tints: Record<string, number> = {
      emerald_plains: 0x88cc88,
      twilight_forest: 0x66aa66,
      anvil_mountains: 0x998888,
      scorching_desert: 0xffaa44,
      abyss_rift: 0xff6622,
    };
    const tint = tints[this.currentMapId] || 0xccccaa;

    this.ambientDustEmitter?.destroy();
    const emitter = this.add.particles(0, 0, dustKey, {
      x: { min: -GAME_WIDTH, max: GAME_WIDTH * 2 },
      y: { min: -GAME_HEIGHT, max: GAME_HEIGHT * 2 },
      lifespan: { min: 6000, max: 12000 },
      speed: { min: 2, max: 8 },
      angle: { min: 200, max: 340 },
      scale: { start: 0.8, end: 1.5 },
      alpha: { start: 0.15, end: 0 },
      tint,
      frequency: 800,
      quantity: 1,
      blendMode: Phaser.BlendModes.ADD,
    });
    emitter.setScrollFactor(0.3);
    emitter.setDepth(998);
    this.ambientDustEmitter = emitter;
  }

  private registerLightSources(): void {
    this.lighting.clearLights();

    // Player subtle halo
    const playerLight = {
      x: this.player.sprite.x,
      y: this.player.sprite.y,
      radius: 80,
      color: 0xffeedd,
      intensity: 0.4,
      id: 'player',
    };
    this.lights_playerLight = playerLight;
    this.lighting.addLight(playerLight);

    // Camp lights: campfires + torches
    for (const decor of this.campDecorPositions) {
      const pos = cartToIso(decor.col, decor.row);
      if (decor.type === 'campfire') {
        this.lighting.addLight({
          x: pos.x,
          y: pos.y - 8,
          radius: 120,
          color: 0xff8800,
          intensity: 0.85,
          flicker: true,
          id: `campfire_${decor.col}_${decor.row}`,
        });
      } else if (decor.type === 'torch') {
        this.lighting.addLight({
          x: pos.x,
          y: pos.y - 12,
          radius: 60,
          color: 0xff6600,
          intensity: 0.65,
          flicker: true,
          id: `torch_${decor.col}_${decor.row}`,
        });
      }
    }
  }

  private updateCampDecorations(): void {
    const { left, right, top, bottom } = this.getExpandedWorldBounds(TILE_WIDTH * 4, TILE_HEIGHT * 4);
    const visibleKeys = new Set<string>();

    for (const decor of this.campDecorWorldPositions) {
      if (decor.x < left || decor.x > right || decor.y < top || decor.y > bottom) continue;

      const key = decor.key;
      visibleKeys.add(key);
      if (this.campDecorSprites.has(key)) continue;

      const texKey = `camp_${decor.type}`;
      if (!this.textures.exists(texKey)) continue;

      const sprite = this.add.image(decor.x, decor.y - 16, texKey).setScale(1 / TEXTURE_SCALE);
      sprite.setDepth(decor.y + 10);
      this.campDecorSprites.set(key, sprite);

      // Torch: small particle flame on top of pole
      if (decor.type === 'torch') {
        const torchFire = this.add.particles(decor.x, decor.y - 28, 'particle_flame', {
          speed: { min: 5, max: 20 },
          angle: { min: 255, max: 285 },
          scale: { start: 0.5, end: 0.05 },
          alpha: { start: 0.85, end: 0 },
          lifespan: { min: 250, max: 500 },
          frequency: 80,
          tint: [0xff6600, 0xff8800, 0xffaa00],
          blendMode: Phaser.BlendModes.ADD,
          emitting: true,
        });
        torchFire.setDepth(decor.y + 12);
        this.campParticles.set(key, torchFire);
      }
      // Campfire: particle fire + glow
      if (decor.type === 'campfire') {
        // Fire particles
        const fireEmitter = this.add.particles(decor.x, decor.y - 20, 'particle_flame', {
          speed: { min: 10, max: 40 },
          angle: { min: 250, max: 290 },
          scale: { start: 0.8, end: 0.1 },
          alpha: { start: 0.9, end: 0 },
          lifespan: { min: 400, max: 800 },
          frequency: 50,
          tint: [0xff6600, 0xff8800, 0xffaa00, 0xffcc22],
          blendMode: Phaser.BlendModes.ADD,
          emitting: true,
        });
        fireEmitter.setDepth(decor.y + 12);
        this.campParticles.set(key, fireEmitter);
        // Spark particles (smaller, faster)
        const sparkKey = `${key}_spark`;
        const sparkEmitter = this.add.particles(decor.x, decor.y - 18, 'particle_circle', {
          speed: { min: 15, max: 50 },
          angle: { min: 240, max: 300 },
          scale: { start: 0.4, end: 0 },
          alpha: { start: 1, end: 0 },
          lifespan: { min: 300, max: 600 },
          frequency: 150,
          tint: [0xffdd44, 0xff8800],
          blendMode: Phaser.BlendModes.ADD,
          emitting: true,
        });
        sparkEmitter.setDepth(decor.y + 13);
        this.campParticles.set(sparkKey, sparkEmitter);
        // Glow circle (pulsing)
        const glow = this.add.circle(decor.x, decor.y - 8, 60, 0xff8800, 0.08);
        glow.setBlendMode(Phaser.BlendModes.ADD);
        glow.setDepth(decor.y + 5);
        this.tweens.add({
          targets: glow,
          alpha: { from: 0.06, to: 0.14 },
          scaleX: { from: 0.9, to: 1.1 }, scaleY: { from: 0.9, to: 1.1 },
          duration: 800, yoyo: true, repeat: -1, ease: 'Sine.easeInOut',
        });
        const glowKey = `${key}_glow`;
        this.campDecorSprites.set(glowKey, glow as unknown as Phaser.GameObjects.Image);
      }
      // Banner sway
      if (decor.type === 'banner') {
        this.tweens.add({
          targets: sprite,
          angle: { from: -3, to: 3 },
          duration: 1500 + Math.random() * 500,
          yoyo: true, repeat: -1, ease: 'Sine.easeInOut',
        });
      }
    }

    for (const [key, sprite] of this.campDecorSprites) {
      const baseKey = key.replace(/_glow$/, '');
      if (!visibleKeys.has(baseKey)) {
        sprite.destroy();
        this.campDecorSprites.delete(key);
      }
    }
    // Clean up particle emitters for off-screen campfires
    for (const [key, emitter] of this.campParticles) {
      const baseKey = key.replace(/_spark$/, '');
      if (!visibleKeys.has(baseKey)) {
        emitter.destroy();
        this.campParticles.delete(key);
      }
    }
  }

  private handleKeyboardMovement(delta: number): void {
    if (this.player.hp <= 0) return;
    let dx = 0, dy = 0;

    // Keyboard input
    if (this.cursors && this.wasd) {
      if (this.cursors.up.isDown || this.wasd.W.isDown) { dx -= 1; dy -= 1; }
      if (this.cursors.down.isDown || this.wasd.S.isDown) { dx += 1; dy += 1; }
      if (this.cursors.left.isDown || this.wasd.A.isDown) { dx -= 1; dy += 1; }
      if (this.cursors.right.isDown || this.wasd.D.isDown) { dx += 1; dy -= 1; }
    }

    // Mobile joystick input (additive, so both can coexist)
    if (dx === 0 && dy === 0 && this.mobileControls) {
      const mobile = this.mobileControls.getDirection();
      dx = mobile.dx;
      dy = mobile.dy;
    }

    if (dx !== 0 || dy !== 0) {
      this.player.path = [];
      const speed = this.player.moveSpeed * (delta / 1000) * 0.015;
      const len = Math.sqrt(dx * dx + dy * dy);
      const newCol = this.player.tileCol + (dx / len) * speed;
      const newRow = this.player.tileRow + (dy / len) * speed;
      const checkCol = Math.round(newCol), checkRow = Math.round(newRow);
      if (checkCol >= 0 && checkCol < this.mapData.cols && checkRow >= 0 && checkRow < this.mapData.rows && this.mapData.collisions[checkRow][checkCol]) {
        this.player.moveTo(newCol, newRow);
      }
    }
  }

  private handleSkillInput(time: number): void {
    if (!this.wasd) return;
    if (Phaser.Input.Keyboard.JustDown(this.wasd.TAB)) {
      this.player.autoCombat = !this.player.autoCombat;
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: `自动战斗: ${this.player.autoCombat ? '开启' : '关闭'}`, type: 'system' });
    }
    if (Phaser.Input.Keyboard.JustDown(this.wasd.I)) {
      EventBus.emit(GameEvents.UI_TOGGLE_PANEL, { panel: 'inventory' });
    }
    if (Phaser.Input.Keyboard.JustDown(this.wasd.M)) {
      EventBus.emit(GameEvents.UI_TOGGLE_PANEL, { panel: 'map' });
    }
    if (Phaser.Input.Keyboard.JustDown(this.wasd.K)) {
      EventBus.emit(GameEvents.UI_TOGGLE_PANEL, { panel: 'skills' });
    }
    if (Phaser.Input.Keyboard.JustDown(this.wasd.H)) {
      EventBus.emit(GameEvents.UI_TOGGLE_PANEL, { panel: 'homestead' });
    }
    if (Phaser.Input.Keyboard.JustDown(this.wasd.C)) {
      EventBus.emit(GameEvents.UI_TOGGLE_PANEL, { panel: 'character' });
    }
    if (Phaser.Input.Keyboard.JustDown(this.wasd.J)) {
      EventBus.emit(GameEvents.UI_TOGGLE_PANEL, { panel: 'quest' });
    }
    if (Phaser.Input.Keyboard.JustDown(this.wasd.O)) {
      EventBus.emit(GameEvents.UI_TOGGLE_PANEL, { panel: 'audio' });
    }
    if (Phaser.Input.Keyboard.JustDown(this.wasd.R)) {
      this.useTownPortal();
    }
    if (Phaser.Input.Keyboard.JustDown(this.wasd.P)) {
      EventBus.emit(GameEvents.UI_TOGGLE_PANEL, { panel: 'companion' });
    }
    if (Phaser.Input.Keyboard.JustDown(this.wasd.V)) {
      EventBus.emit(GameEvents.UI_TOGGLE_PANEL, { panel: 'achievement' });
    }
    if (Phaser.Input.Keyboard.JustDown(this.wasd.ESC)) {
      this.returnToMenu();
    }

    const skillKeys = [this.wasd.ONE, this.wasd.TWO, this.wasd.THREE, this.wasd.FOUR, this.wasd.FIVE, this.wasd.SIX];
    const skills = this.player.classData.skills;
    for (let i = 0; i < Math.min(skillKeys.length, skills.length); i++) {
      if (Phaser.Input.Keyboard.JustDown(skillKeys[i])) this.tryUseSkill(skills[i].id, time);
    }
  }

  private tryUseSkill(skillId: string, time: number): void {
    if (this.player.hp <= 0) return;
    const skill = this.player.getSkill(skillId);
    if (!skill) return;
    if (!this.player.isSkillReady(skillId, time)) return;
    const level = this.player.getSkillLevel(skillId);
    const scaledManaCost = getSkillManaCost(skill, level);
    if (this.player.mana < scaledManaCost) {
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: `法力不足!`, type: 'combat' });
      return;
    }

    const target = this.findNearestAliveMonster();
    if (!target && !skill.buff) return;

    if (target && !skill.buff) {
      const dist = euclideanDistance(this.player.tileCol, this.player.tileRow, target.tileCol, target.tileRow);
      if (dist > skill.range + 1) return;
    }

    this.player.useSkill(skillId, time, level, this.getEquipStats().cooldownReduction);

    // freeCast: X% chance to not consume mana when casting a skill
    const eqFc = this.getEquipStats();
    if (eqFc.freeCast > 0 && this.combatSystem.checkFreeCast(eqFc.freeCast)) {
      // Refund the mana that was just spent
      this.player.mana = Math.min(this.player.maxMana, this.player.mana + scaledManaCost);
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: '免费施法！法力未消耗', type: 'combat' });
      EventBus.emit(GameEvents.PLAYER_MANA_CHANGED, { mana: this.player.mana, maxMana: this.player.maxMana });
    }

    if (skill.buff || skill.aoe || skill.range > 2) {
      this.player.playCast();
    } else {
      const animTarget = this.findNearestAliveMonster();
      if (animTarget) {
        this.player.playAttack(animTarget.sprite.x, animTarget.sprite.y);
      } else {
        this.player.playCast();
      }
    }


    // ── Teleport: instant reposition to walkable tile near target ──
    if (skillId === 'teleport') {
      // Block while stunned or frozen
      if (this.statusEffects.isImmobilized('player')) {
        EventBus.emit(GameEvents.LOG_MESSAGE, { text: '无法传送：被控制中!', type: 'combat' });
        return;
      }
      const pointer = this.input.activePointer;
      const tile = worldToTile(pointer.worldX, pointer.worldY);
      let destCol = Math.round(tile.col);
      let destRow = Math.round(tile.row);
      // Clamp to map bounds
      destCol = Math.max(1, Math.min(this.mapData.cols - 2, destCol));
      destRow = Math.max(1, Math.min(this.mapData.rows - 2, destRow));
      // Find nearest walkable tile if destination is blocked
      if (!this.mapData.collisions[destRow]?.[destCol]) {
        let found = false;
        for (let r = 1; r <= 3 && !found; r++) {
          for (let dr = -r; dr <= r && !found; dr++) {
            for (let dc = -r; dc <= r && !found; dc++) {
              const nr = destRow + dr, nc = destCol + dc;
              if (nr >= 1 && nr < this.mapData.rows - 1 && nc >= 1 && nc < this.mapData.cols - 1 && this.mapData.collisions[nr]?.[nc]) {
                destCol = nc; destRow = nr; found = true;
              }
            }
          }
        }
        // Abort teleport if no walkable tile found within search radius
        if (!found) {
          EventBus.emit(GameEvents.LOG_MESSAGE, { text: '传送失败：目标位置不可到达!', type: 'combat' });
          // Refund mana since teleport was already deducted
          this.player.mana = Math.min(this.player.maxMana, this.player.mana + scaledManaCost);
          return;
        }
      }
      const origX = this.player.sprite.x;
      const origY = this.player.sprite.y;
      this.player.moveTo(destCol, destRow);
      this.player.path = [];
      this.player.attackTarget = null;
      this.skillEffects.play(skillId, origX, origY, this.player.sprite.x, this.player.sprite.y);
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: `${skill.name} 激活!`, type: 'combat' });
      return;
    }

    // ── Shadow Step: teleport to target monster position ──
    if (skillId === 'shadow_step' && target) {
      const origX = this.player.sprite.x;
      const origY = this.player.sprite.y;
      // Move player behind the target (offset by 1 tile in the direction from target to player)
      const dx = this.player.tileCol - target.tileCol;
      const dy = this.player.tileRow - target.tileRow;
      const len = Math.sqrt(dx * dx + dy * dy) || 1;
      let behindCol = Math.round(target.tileCol - dx / len);
      let behindRow = Math.round(target.tileRow - dy / len);
      // Clamp to bounds and fallback to target pos if not walkable
      behindCol = Math.max(1, Math.min(this.mapData.cols - 2, behindCol));
      behindRow = Math.max(1, Math.min(this.mapData.rows - 2, behindRow));
      if (!this.mapData.collisions[behindRow]?.[behindCol]) {
        behindCol = Math.round(target.tileCol);
        behindRow = Math.round(target.tileRow);
      }
      this.player.moveTo(behindCol, behindRow);
      this.player.path = [];
      this.player.attackTarget = target.id;
      // Apply crit bonus buff to player
      if (skill.buff) {
        const buffValue = getSkillBuffValue(skill, level);
        const buffDuration = getSkillBuffDuration(skill, level);
        this.player.buffs.push({ stat: skill.buff.stat, value: buffValue, duration: buffDuration, startTime: time });
      }
      this.skillEffects.play(skillId, origX, origY, this.player.sprite.x, this.player.sprite.y);
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: `${skill.name} 激活!`, type: 'combat' });
      return;
    }

    // ── Death Mark: apply damageAmplify debuff to target monster ──
    if (skillId === 'death_mark' && target) {
      const buffValue = getSkillBuffValue(skill, level);
      const buffDuration = getSkillBuffDuration(skill, level);
      // Apply amplify debuff to the target monster, not to player
      target.buffs.push({ stat: 'damageAmplify', value: buffValue, duration: buffDuration, startTime: time });
      // Also deal the skill's base damage
      if (skill.damageMultiplier > 0) {
        const result = this.combatSystem.calculateDamage(this.player.toCombatEntity(this.getEquipStats()), target.toCombatEntity(), skill, level, this.player.skillLevels);
        target.takeDamage(result.damage, this.player.sprite.x, this.player.sprite.y);
        this.applySteal(result);
        this.showDamageText(target.sprite.x, target.sprite.y, result.damage, result.isCrit, false, false, skill.damageType);
        if (!target.isAlive()) this.onMonsterKilled(target);
      }
      this.skillEffects.play(skillId, this.player.sprite.x, this.player.sprite.y, target.sprite.x, target.sprite.y);
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: `${skill.name} 标记了 ${target.definition.name}!`, type: 'combat' });
      return;
    }

    // ── Slow Trap: apply slow status effect to enemies in AoE, not buff to player ──
    if (skillId === 'slow_trap') {
      const scaledRadius = getSkillAoeRadius(skill, level);
      const aoeTargets = this.monsters.filter(m =>
        m.isAlive() && euclideanDistance(this.player.tileCol, this.player.tileRow, m.tileCol, m.tileRow) <= scaledRadius
      );
      // Apply damage
      for (const t of aoeTargets) {
        if (skill.damageMultiplier > 0) {
          const result = this.combatSystem.calculateDamage(this.player.toCombatEntity(this.getEquipStats()), t.toCombatEntity(), skill, level, this.player.skillLevels);
          t.takeDamage(result.damage, this.player.sprite.x, this.player.sprite.y);
          this.applySteal(result);
          this.showDamageText(t.sprite.x, t.sprite.y, result.damage, result.isCrit, false, false, skill.damageType);
          if (!t.isAlive()) { this.onMonsterKilled(t); continue; }
        }
        // Apply Slow via StatusEffectSystem
        const slowValue = skill.buff ? Math.round(getSkillBuffValue(skill, level) * 100) : 40;
        const slowDuration = skill.buff ? getSkillBuffDuration(skill, level) : 5000;
        this.statusEffects.apply(t.id, 'slow', slowValue, slowDuration, 'player', time);
      }
      this.skillEffects.play(skillId, this.player.sprite.x, this.player.sprite.y, this.player.sprite.x, this.player.sprite.y);
      if (aoeTargets.length > 0) {
        EventBus.emit(GameEvents.LOG_MESSAGE, { text: `${skill.name} 减速了${aoeTargets.length}个敌人!`, type: 'combat' });
      }
      return;
    }

    if (skill.buff) {
      const buffValue = getSkillBuffValue(skill, level);
      const buffDuration = getSkillBuffDuration(skill, level);
      this.player.buffs.push({ stat: skill.buff.stat, value: buffValue, duration: buffDuration, startTime: time });
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: `${skill.name} 激活!`, type: 'combat' });
      this.skillEffects.play(skillId, this.player.sprite.x, this.player.sprite.y);
      if (this.vfx) {
        if (skill.buff.stat === 'hp' || skillId.includes('heal')) {
          this.vfx.healBurst(this.player.sprite.x, this.player.sprite.y - 16, 10);
        }
      }

      // Taunt aggro-forcing: when Taunt Roar (or similar taunt skill) activates,
      // monsters in AoE range switch aggro target to player
      if (skillId === 'taunt_roar' && skill.aoe && skill.aoeRadius) {
        const scaledTauntRadius = getSkillAoeRadius(skill, level);
        const tauntTargets = this.monsters.filter(m =>
          m.isAlive() && euclideanDistance(this.player.tileCol, this.player.tileRow, m.tileCol, m.tileRow) <= scaledTauntRadius
        );
        for (const m of tauntTargets) {
          // Add taunted buff to monster
          m.buffs.push({ stat: 'taunted', value: 1, duration: buffDuration, startTime: time });
          // Force monster into chase/attack state targeting the player
          if (m.state === 'idle' || m.state === 'patrol') {
            m.state = 'chase';
          }
        }
        if (tauntTargets.length > 0) {
          EventBus.emit(GameEvents.LOG_MESSAGE, {
            text: `嘲讽怒吼影响了${tauntTargets.length}个敌人`,
            type: 'combat',
          });
        }
      }

      return;
    }

    const scaledAoeRadius = getSkillAoeRadius(skill, level);
    if (skill.aoe && scaledAoeRadius > 0) {
      const aoeTargets = this.monsters.filter(m =>
        m.isAlive() && euclideanDistance(this.player.tileCol, this.player.tileRow, m.tileCol, m.tileRow) <= scaledAoeRadius
      );
      for (const t of aoeTargets) {
        const result = this.combatSystem.calculateDamage(this.player.toCombatEntity(this.getEquipStats()), t.toCombatEntity(), skill, level, this.player.skillLevels);
        // Combustion: +50% damage on burning targets
        let finalDmg = result.damage;
        if (skillId === 'combustion' && this.statusEffects.hasEffect(t.id, 'burn')) {
          finalDmg = Math.floor(finalDmg * 1.5);
        }
        t.takeDamage(finalDmg, this.player.sprite.x, this.player.sprite.y);
        this.applySteal(result);
        this.showDamageText(t.sprite.x, t.sprite.y, finalDmg, result.isCrit, false, false, skill.damageType);
        // Apply status effects from skill damage type
        this.applySkillStatusEffect(t, skill, finalDmg, time);
        if (!t.isAlive()) this.onMonsterKilled(t);
        if (this.vfx && skill.damageType !== 'physical') {
          const impactColor = skillId.includes('fire') || skillId === 'meteor' ? 0xff6600
            : skillId.includes('ice') || skillId === 'blizzard' ? 0x4488ff
            : skillId.includes('lightning') || skillId === 'chain_lightning' ? 0x5dade2
            : 0xf39c12;
          this.vfx.skillImpactBloom(t.sprite.x, t.sprite.y - 16, impactColor);
        }
      }
      if (this.vfx && aoeTargets.length > 0) {
        this.vfx.cameraShake(100, 0.004 + aoeTargets.length * 0.001);
      }
      if (skillId === 'chain_lightning') {
        this.skillEffects.play(skillId, this.player.sprite.x, this.player.sprite.y,
          undefined, undefined,
          aoeTargets.map(t => ({ x: t.sprite.x, y: t.sprite.y })));
      } else {
        this.skillEffects.play(skillId, this.player.sprite.x, this.player.sprite.y,
          this.player.sprite.x, this.player.sprite.y);
      }
    } else if (target) {
      const result = this.combatSystem.calculateDamage(this.player.toCombatEntity(this.getEquipStats()), target.toCombatEntity(), skill, level, this.player.skillLevels);
      // Combustion: +50% damage on burning targets
      let finalDmg = result.damage;
      if (skillId === 'combustion' && this.statusEffects.hasEffect(target.id, 'burn')) {
        finalDmg = Math.floor(finalDmg * 1.5);
      }
      target.takeDamage(finalDmg, this.player.sprite.x, this.player.sprite.y);
      this.applySteal(result);
      this.showDamageText(target.sprite.x, target.sprite.y, finalDmg, result.isCrit, false, false, skill.damageType);
      // Apply status effects from skill damage type
      this.applySkillStatusEffect(target, skill, finalDmg, time);
      if (!target.isAlive()) this.onMonsterKilled(target);
      this.skillEffects.play(skillId, this.player.sprite.x, this.player.sprite.y,
        target.sprite.x, target.sprite.y);
      if (this.vfx) {
        const impactColor = skill.damageType !== 'physical' ? 0xff6600 : 0xf1c40f;
        this.vfx.skillImpactBloom(target.sprite.x, target.sprite.y - 16, impactColor);
      }
      if (this.trails && (skill.damageType !== 'physical' || skill.damageMultiplier > 1.5)) {
        const scorchType = skillId.includes('fire') || skillId === 'meteor' ? 'fire'
          : skillId.includes('ice') || skillId === 'blizzard' ? 'ice'
          : 'lightning';
        this.trails.stampGround(target.sprite.x, target.sprite.y, scorchType);
      }
    }
  }

  private handleCombat(time: number): void {
    if (this.player.hp <= 0) return;
    this.player.buffs = this.player.buffs.filter(b => time - b.startTime < b.duration);

    for (const monster of this.monsters) {
      if (!monster.isAlive() || monster.state !== 'attack') continue;
      // Immobilized monsters cannot attack
      if (this.statusEffects.isImmobilized(monster.id)) continue;
      if (time - monster.lastAttackTime >= monster.definition.attackSpeed) {
        monster.lastAttackTime = time;
        monster.playAttack(this.player.sprite.x, this.player.sprite.y);
        const result = this.combatSystem.calculateDamage(monster.toCombatEntity(), this.player.toCombatEntity(this.getEquipStats()));
        if (result.isDodged) {
          this.showDamageText(this.player.sprite.x, this.player.sprite.y, 0, false, true);
          // dodgeCounter: after dodging, next attack is guaranteed crit
          const eqDc = this.getEquipStats();
          if (eqDc.dodgeCounter > 0) {
            this._dodgeCounterReady = true;
            EventBus.emit(GameEvents.LOG_MESSAGE, { text: '闪避反击就绪！下次攻击必定暴击', type: 'combat' });
          }
        } else {
          // Difficulty damage scaling is already applied at monster spawn time via DifficultySystem.scaleMonster
          const finalDmg = result.damage;
          this.player.hp = Math.max(0, this.player.hp - finalDmg);

          // Thorns heal (set bonus: recover % maxHp on hit taken)
          const eq = this.getEquipStats();
          if (eq.thornsHeal > 0 && this.player.hp > 0) {
            const heal = Math.floor(this.player.maxHp * eq.thornsHeal / 100);
            this.player.hp = Math.min(this.player.maxHp, this.player.hp + heal);
            EventBus.emit(GameEvents.PLAYER_HEALTH_CHANGED, { hp: this.player.hp, maxHp: this.player.maxHp });
          }

          this.player.playHurt(monster.sprite.x, monster.sprite.y);
          this.showDamageText(this.player.sprite.x, this.player.sprite.y, finalDmg, result.isCrit, false, true);
          if (monster.definition.attackRange > 2.5) {
            const projColor = monster.definition.spriteKey.includes('fire') || monster.definition.spriteKey.includes('phoenix')
              ? 0xff6600 : monster.definition.spriteKey.includes('ice') ? 0x4488ff : 0xcc44cc;
            this.skillEffects.playMonsterRangedAttack(
              monster.sprite.x, monster.sprite.y,
              this.player.sprite.x, this.player.sprite.y, projColor,
            );
          } else {
            this.skillEffects.playMonsterAttack(this.player.sprite.x, this.player.sprite.y);
          }
          EventBus.emit(GameEvents.COMBAT_DAMAGE, {
            targetId: 'player', damage: finalDmg, isDodged: false,
            isCrit: result.isCrit, isPlayerTarget: true,
            targetMaxHP: this.player.maxHp,
          });

          // Monster applies status effects to player based on monster type
          this.applyMonsterStatusEffect(monster, time);

          // ── Elite Affix: on-hit effects ──
          if (monster.eliteAffixes.length > 0) {
            const affixStats = this.eliteAffixSystem.getCombinedStats(monster.eliteAffixes);

            // Fire Enhanced: extra fire damage
            if (affixStats.extraFireDamage > 0) {
              const fireDmg = Math.floor(finalDmg * affixStats.extraFireDamage);
              if (fireDmg > 0) {
                this.player.hp = Math.max(0, this.player.hp - fireDmg);
                this.showDamageText(this.player.sprite.x + 10, this.player.sprite.y - 5, fireDmg, false, false, true, 'fire');
                EventBus.emit(GameEvents.COMBAT_DAMAGE, {
                  targetId: 'player', damage: fireDmg, isDodged: false,
                  isCrit: false, isPlayerTarget: true, targetMaxHP: this.player.maxHp,
                });
              }
            }

            // Vampiric: lifesteal on hit
            if (affixStats.lifestealFraction > 0) {
              const heal = Math.floor(finalDmg * affixStats.lifestealFraction);
              if (heal > 0) {
                monster.hp = Math.min(monster.maxHp, monster.hp + heal);
              }
            }

            // Frozen: chance to apply slow on hit
            if (affixStats.freezeChance > 0 && Math.random() < affixStats.freezeChance) {
              this.statusEffects.apply('player', 'slow', 30, 2500, monster.id, time);
              EventBus.emit(GameEvents.LOG_MESSAGE, { text: '冰封减速！', type: 'combat' });
            }
          }

          if (this.player.hp <= 0) {
            // Death save check (set bonus / legendary)
            const eqDs = this.getEquipStats();
            if (eqDs.deathSave > 0 && !this._deathSaveUsed) {
              this.player.hp = Math.floor(this.player.maxHp * 0.3);
              this._deathSaveUsed = true;
              this.time.delayedCall(60000, () => { this._deathSaveUsed = false; });
              EventBus.emit(GameEvents.LOG_MESSAGE, { text: '死亡豁免触发！恢复30%生命', type: 'system' });
              if (this.vfx) this.vfx.healBurst(this.player.sprite.x, this.player.sprite.y - 16, 20);
            } else {
              this.player.die(); break;
            }
          }
        }
      }
    }

    // Player auto-attack
    const target = this.player.attackTarget
      ? this.monsters.find(m => m.id === this.player.attackTarget && m.isAlive())
      : this.findNearestAggroMonster();

    if (target && target.isAlive()) {
      const dist = euclideanDistance(this.player.tileCol, this.player.tileRow, target.tileCol, target.tileRow);
      if (dist <= this.player.attackRange && time - this.player.lastAttackTime >= this.player.attackSpeed) {
        this.player.lastAttackTime = time;
        this.player.playAttack(target.sprite.x, target.sprite.y);
        const eq = this.getEquipStats();

        // dodgeCounter: guaranteed crit after dodge
        const forceCrit = this._dodgeCounterReady;
        if (forceCrit) {
          this._dodgeCounterReady = false;
          EventBus.emit(GameEvents.LOG_MESSAGE, { text: '闪避反击！暴击！', type: 'combat' });
        }

        const result = this.combatSystem.calculateDamage(
          this.player.toCombatEntity(eq), target.toCombatEntity(),
          undefined, 1, undefined, forceCrit,
        );
        target.takeDamage(result.damage, this.player.sprite.x, this.player.sprite.y);
        this.applySteal(result);
        this.showDamageText(target.sprite.x, target.sprite.y, result.damage, result.isCrit);
        this.skillEffects.playAttack(this.player.sprite.x, this.player.sprite.y, target.sprite.x, target.sprite.y, true);

        // Consume stealthDamage buff after attack (it multiplies next attack only)
        if (this.player.buffs.some(b => b.stat === 'stealthDamage')) {
          this.player.buffs = this.player.buffs.filter(b => b.stat !== 'stealthDamage');
        }

        // VFX for player attacks — crit flash + hit sparks
        if (result.isCrit || result.damage > 0) {
          EventBus.emit(GameEvents.COMBAT_DAMAGE, {
            targetId: target.id, damage: result.damage, isDodged: false,
            isCrit: result.isCrit, isPlayerTarget: false,
            targetMaxHP: target.maxHp,
          });
          // Hit-freeze and flash
          target.animator.triggerHitFreeze(35);
          this.player.animator.triggerHitFreeze(35);
          if (this.vfx) this.vfx.hitFlash(target.sprite);
          if (result.isCrit && this.vfx) {
            this.vfx.hitSparks(target.sprite.x, target.sprite.y - 16, 12);
          }
        }
        // Weapon slash trail on basic attack
        if (this.trails) {
          const angle = Math.atan2(target.sprite.y - this.player.sprite.y, target.sprite.x - this.player.sprite.x);
          this.trails.stampSlash(target.sprite.x, target.sprite.y - 16, angle, 0xffffcc);
        }

        // critDoubleStrike: on crit, X% chance for immediate extra attack
        if (result.isCrit && eq.critDoubleStrike > 0 && target.isAlive()) {
          if (this.combatSystem.checkCritDoubleStrike(eq.critDoubleStrike, true)) {
            const extraResult = this.combatSystem.calculateDamage(
              this.player.toCombatEntity(eq), target.toCombatEntity(),
            );
            target.takeDamage(extraResult.damage, this.player.sprite.x, this.player.sprite.y);
            this.applySteal(extraResult);
            this.showDamageText(target.sprite.x, target.sprite.y - 20, extraResult.damage, extraResult.isCrit);
            EventBus.emit(GameEvents.LOG_MESSAGE, { text: '连击触发！', type: 'combat' });
            if (this.vfx) this.vfx.hitSparks(target.sprite.x, target.sprite.y - 16, 8);
          }
        }

        // doubleShot: X% chance to fire double projectile on ranged auto-attack
        if (eq.doubleShot > 0 && target.isAlive()) {
          if (this.combatSystem.checkDoubleShot(eq.doubleShot, this.player.attackRange)) {
            const extraResult = this.combatSystem.calculateDamage(
              this.player.toCombatEntity(eq), target.toCombatEntity(),
            );
            target.takeDamage(extraResult.damage, this.player.sprite.x, this.player.sprite.y);
            this.applySteal(extraResult);
            this.showDamageText(target.sprite.x + 15, target.sprite.y - 15, extraResult.damage, extraResult.isCrit);
            EventBus.emit(GameEvents.LOG_MESSAGE, { text: '双倍箭矢！', type: 'combat' });
            this.skillEffects.playAttack(this.player.sprite.x, this.player.sprite.y, target.sprite.x, target.sprite.y, true);
          }
        }

        if (!target.isAlive()) { this.onMonsterKilled(target); this.player.attackTarget = null; }
      }
    }
  }

  private handleAutoCombat(time: number): void {
    if (this.player.hp <= 0) return;
    if (!this.player.attackTarget) {
      // Don't override user's click-to-move path
      if (this.player.isMoving) return;
      const nearest = this.findNearestAliveMonster();
      if (nearest) {
        const dist = euclideanDistance(this.player.tileCol, this.player.tileRow, nearest.tileCol, nearest.tileRow);
        if (dist <= nearest.definition.aggroRange) {
          this.player.attackTarget = nearest.id;
          if (dist > this.player.attackRange) {
            const path = this.pathfinding.findPath(
              Math.round(this.player.tileCol), Math.round(this.player.tileRow),
              Math.round(nearest.tileCol), Math.round(nearest.tileRow),
            );
            this.player.setPath(path);
          }
        }
      }
    }
    for (const skillId of this.player.autoSkillPriority) {
      const skill = this.player.getSkill(skillId);
      if (!skill) continue;
      const sLevel = this.player.getSkillLevel(skillId);
      if (this.player.isSkillReady(skillId, time) && this.player.mana >= getSkillManaCost(skill, sLevel)) {
        this.tryUseSkill(skillId, time);
        break;
      }
    }
  }

  private updateTargetIndicator(): void {
    // Find the active attack target
    const targetId = this.player.attackTarget;
    const target = targetId
      ? this.monsters.find(m => m.id === targetId && m.isAlive())
      : this.findNearestAggroMonster();

    if (target && target.isAlive()) {
      if (!this.targetIndicator) {
        this.targetIndicator = this.add.ellipse(0, 0, 36, 12, 0xff4444, 0)
          .setStrokeStyle(1.5, 0xff4444, 0.6);
      }
      this.targetIndicator.setPosition(target.sprite.x, target.sprite.y + 4);
      this.targetIndicator.setDepth(target.sprite.depth - 1);
      this.targetIndicator.setVisible(true);
      this.currentTargetId = target.id;
    } else if (this.targetIndicator) {
      this.targetIndicator.setVisible(false);
      this.currentTargetId = null;
    }
  }

  /** Lazily refresh and return the cached equipment stats. Invalidated on equip/unequip. */
  private getEquipStats(): EquipStats {
    if (!this.cachedEquipStats) {
      const eq = this.inventorySystem.getTypedEquipStats();
      // Merge achievement bonuses into equipment stats (idempotent — getBonuses() is pure)
      const achBonuses = this.achievementSystem.getBonuses();
      for (const [stat, value] of Object.entries(achBonuses)) {
        if (stat in eq) {
          eq[stat as keyof EquipStats] += value;
        }
      }
      this.cachedEquipStats = eq;
    }
    return this.cachedEquipStats;
  }

  /** Call whenever equipment changes to invalidate the cache. */
  invalidateEquipStats(): void {
    this.cachedEquipStats = null;
  }

  /** Apply life/mana steal from a damage result back to the player. */
  private applySteal(result: { lifeStolen: number; manaStolen: number }): void {
    if (result.lifeStolen > 0 && this.player.hp > 0) {
      this.player.hp = Math.min(this.player.maxHp, this.player.hp + result.lifeStolen);
      EventBus.emit(GameEvents.PLAYER_HEALTH_CHANGED, { hp: this.player.hp, maxHp: this.player.maxHp });
    }
    if (result.manaStolen > 0) {
      this.player.mana = Math.min(this.player.maxMana, this.player.mana + result.manaStolen);
      EventBus.emit(GameEvents.PLAYER_MANA_CHANGED, { mana: this.player.mana, maxMana: this.player.maxMana });
    }
  }

  private updateCombatState(): void {
    const fighting = this.monsters.some(m => m.isAlive() && m.state === 'attack')
      || (this.player.attackTarget != null && this.monsters.some(m => m.id === this.player.attackTarget && m.isAlive()));

    if (fighting && !this.inCombat) {
      this.inCombat = true;
      if (this.combatDebounceTimer) { clearTimeout(this.combatDebounceTimer); this.combatDebounceTimer = null; }
      EventBus.emit(GameEvents.COMBAT_STATE_CHANGED, { inCombat: true });
    } else if (!fighting && this.inCombat) {
      if (!this.combatDebounceTimer) {
        this.combatDebounceTimer = setTimeout(() => {
          this.inCombat = false;
          this.combatDebounceTimer = null;
          EventBus.emit(GameEvents.COMBAT_STATE_CHANGED, { inCombat: false });
        }, 1500);
      }
    }
  }

  // ── Random Event System ─────────────────────────────────────────────
  private checkRandomEvents(time: number, delta: number): void {
    if (this.player.hp <= 0) return;

    // Determine tile type at player position
    let tileType: number | undefined;
    if (this.mapData.tiles.length > 0 && this.player.tileRow >= 0 && this.player.tileRow < this.mapData.rows
        && this.player.tileCol >= 0 && this.player.tileCol < this.mapData.cols) {
      tileType = this.mapData.tiles[this.player.tileRow]?.[this.player.tileCol];
    }

    const event = this.randomEventSystem.update(
      time, delta,
      this.player.tileCol, this.player.tileRow,
      this.inCombat,
      this.campPositions,
      tileType,
    );

    if (!event) return;

    // Get event definition for the log message
    const eventDef = RandomEventSystem.getEventDef(event.type);
    if (eventDef) {
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: eventDef.message, type: 'info' });
    }
    EventBus.emit(GameEvents.RANDOM_EVENT_TRIGGERED, { event });

    // Handle the event based on type
    this.handleRandomEvent(event, time);
  }

  private handleRandomEvent(event: ActiveEvent, time: number): void {
    switch (event.type) {
      case 'ambush':
        this.handleAmbushEvent(event);
        break;
      case 'treasure_cache':
        this.handleTreasureCacheEvent(event);
        break;
      case 'wandering_merchant':
        this.handleWanderingMerchantEvent(event);
        break;
      case 'rescue':
        this.handleRescueEvent(event);
        break;
      case 'environmental_puzzle':
        this.handlePuzzleEvent(event);
        break;
    }
  }

  private handleAmbushEvent(event: ActiveEvent): void {
    const monsterIds = event.context.monsterIds as string[] | undefined;
    const count = (event.context.monsterCount as number) ?? 3;
    const monsterDefs = MonstersByZone[this.currentMapId] || [];

    for (let i = 0; i < count; i++) {
      // Pick a random monster type from the zone's ambush list
      const mId = monsterIds && monsterIds.length > 0
        ? monsterIds[Math.floor(Math.random() * monsterIds.length)]
        : (monsterDefs.length > 0 ? monsterDefs[0].id : null);

      if (!mId) continue;
      let def = monsterDefs.find(m => m.id === mId) || getMonsterDef(mId);
      if (!def) continue;
      // Apply difficulty scaling to ambush monsters
      def = DifficultySystem.scaleMonster(def, this.difficulty);

      // Spawn near the player (within 3-5 tiles)
      const angle = Math.random() * Math.PI * 2;
      const dist = 3 + Math.random() * 2;
      const sc = Math.round(event.col + Math.cos(angle) * dist);
      const sr = Math.round(event.row + Math.sin(angle) * dist);
      const preferredCol = Math.max(1, Math.min(this.mapData.cols - 2, sc));
      const preferredRow = Math.max(1, Math.min(this.mapData.rows - 2, sr));

      // Find walkable tile (with fallback search if preferred position is blocked)
      const walkable = RandomEventSystem.findWalkableTile(
        preferredCol, preferredRow, this.mapData.collisions, this.mapData.cols, this.mapData.rows,
      );
      if (walkable) {
        const monster = new Monster(this, def, walkable.col, walkable.row);
        // Immediately aggro to player
        monster.state = 'chase';
        this.monsters.push(monster);
      }
    }

    // Ambush auto-resolves (monsters are spawned)
    this.randomEventSystem.resolveActiveEvent();
    EventBus.emit(GameEvents.RANDOM_EVENT_RESOLVED, { type: 'ambush' });
  }

  private handleTreasureCacheEvent(event: ActiveEvent): void {
    // Generate zone-scaled loot and drop it at the event location
    const lootLevel = (event.context.lootLevel as number) ?? 1;
    const qualityBoost = (event.context.qualityBoost as number) ?? 0;

    // Create a fake elite monster definition for loot generation (guarantees better quality)
    const fakeDef = {
      id: 'treasure_cache',
      name: '宝箱',
      level: lootLevel,
      hp: 1, damage: 0, defense: 0, speed: 0,
      aggroRange: 0, attackRange: 0, attackSpeed: 1000,
      expReward: 0, goldReward: [10 + lootLevel * 5, 20 + lootLevel * 10] as [number, number],
      spriteKey: 'chest',
      elite: true,
    };

    const items = this.lootSystem.generateLoot(fakeDef, this.player.stats.lck, qualityBoost);

    // Drop gold
    const goldMin = fakeDef.goldReward[0];
    const goldMax = fakeDef.goldReward[1];
    const gold = randomInt(goldMin, goldMax);
    this.player.gold += gold;
    EventBus.emit(GameEvents.LOG_MESSAGE, { text: `从宝箱中获得 ${gold} 金币`, type: 'info' });

    // Drop items near the event position
    for (const item of items) {
      this.dropLootAtPosition(item, event.col, event.row);
    }

    this.randomEventSystem.resolveActiveEvent();
    EventBus.emit(GameEvents.RANDOM_EVENT_RESOLVED, { type: 'treasure_cache' });
  }

  private handleWanderingMerchantEvent(event: ActiveEvent): void {
    // Emit a shop event so UIScene can show the shop panel
    // The wandering merchant uses the correct SHOP_OPEN payload contract: {npcId, shopItems, type}
    const merchantItems = (event.context.merchantItems as string[]) ?? [];
    EventBus.emit(GameEvents.SHOP_OPEN, {
      npcId: 'wandering_merchant',
      shopItems: merchantItems,
      type: 'merchant',
    });
    EventBus.emit(GameEvents.LOG_MESSAGE, { text: '流浪商人出现了! 看看他的商品吧。', type: 'info' });

    this.randomEventSystem.resolveActiveEvent();
    EventBus.emit(GameEvents.RANDOM_EVENT_RESOLVED, { type: 'wandering_merchant' });
  }

  private handleRescueEvent(event: ActiveEvent): void {
    const monsterIds = event.context.monsterIds as string[] | undefined;
    const count = (event.context.monsterCount as number) ?? 2;
    const monsterDefs = MonstersByZone[this.currentMapId] || [];
    const rescueNpcName = (event.context.rescueNpcName as string) ?? '被困的旅人';
    const reward = event.context.reward as { gold: number; exp: number } | undefined;

    // Spawn a stranded NPC sprite at the event location
    const npcWalkable = RandomEventSystem.findWalkableTile(
      Math.round(event.col), Math.round(event.row),
      this.mapData.collisions, this.mapData.cols, this.mapData.rows,
    );
    const npcCol = npcWalkable?.col ?? Math.round(event.col);
    const npcRow = npcWalkable?.row ?? Math.round(event.row);
    const { x: npcX, y: npcY } = cartToIso(npcCol, npcRow);
    const rescueNpcSprite = this.add.container(npcX, npcY);
    rescueNpcSprite.setDepth(npcY + 10);
    // Simple visual: a circle with a help indicator
    const body = this.add.circle(0, -12 * DPR, 8 * DPR, 0x44aaff);
    const helpMark = this.add.text(0, -28 * DPR, '!', {
      fontSize: fs(14), color: '#ff4444', fontFamily: '"Noto Sans SC", sans-serif', fontStyle: 'bold',
    }).setOrigin(0.5);
    const nameLabel = this.add.text(0, 4 * DPR, rescueNpcName, {
      fontSize: fs(10), color: '#aaddff', fontFamily: '"Noto Sans SC", sans-serif',
    }).setOrigin(0.5, 0);
    rescueNpcSprite.add([body, helpMark, nameLabel]);

    // Spawn hostile monsters around the NPC
    const rescueMonsters: Monster[] = [];
    for (let i = 0; i < count; i++) {
      const mId = monsterIds && monsterIds.length > 0
        ? monsterIds[Math.floor(Math.random() * monsterIds.length)]
        : (monsterDefs.length > 0 ? monsterDefs[0].id : null);

      if (!mId) continue;
      let def = monsterDefs.find(m => m.id === mId) || getMonsterDef(mId);
      if (!def) continue;
      // Apply difficulty scaling to rescue event monsters
      def = DifficultySystem.scaleMonster(def, this.difficulty);

      const angle = Math.random() * Math.PI * 2;
      const dist = 2 + Math.random() * 3;
      const preferredCol = Math.max(1, Math.min(this.mapData.cols - 2, Math.round(npcCol + Math.cos(angle) * dist)));
      const preferredRow = Math.max(1, Math.min(this.mapData.rows - 2, Math.round(npcRow + Math.sin(angle) * dist)));

      const walkable = RandomEventSystem.findWalkableTile(
        preferredCol, preferredRow, this.mapData.collisions, this.mapData.cols, this.mapData.rows,
      );
      if (walkable) {
        const monster = new Monster(this, def, walkable.col, walkable.row);
        monster.state = 'chase';
        this.monsters.push(monster);
        rescueMonsters.push(monster);
      }
    }

    // Track rescue event: store monsters to track and check completion each frame
    // We do NOT resolve the event yet — only when all hostiles are defeated
    const monsterIds2 = new Set(rescueMonsters.map(m => m.id));
    event.context.rescueMonsterIds = Array.from(monsterIds2);
    event.context.rescueNpcSpriteRef = rescueNpcSprite;
    event.context.rescueNpcCol = npcCol;
    event.context.rescueNpcRow = npcRow;
    event.context.rescueNpcName = rescueNpcName;
    event.context.reward = reward;

    // Set up a periodic check for when all rescue hostiles are defeated
    this.time.addEvent({
      delay: 500,
      loop: true,
      callback: () => {
        if (event.resolved) return;
        const trackedIds = event.context.rescueMonsterIds as string[];
        if (!trackedIds || trackedIds.length === 0) {
          // No monsters spawned — auto-complete
          this.completeRescueEvent(event);
          return;
        }
        // Check if all tracked monsters are dead (no longer in this.monsters)
        const aliveIds = new Set(this.monsters.map(m => m.id));
        const allDefeated = trackedIds.every(id => !aliveIds.has(id));
        if (allDefeated) {
          this.completeRescueEvent(event);
        }
      },
    });
  }

  private completeRescueEvent(event: ActiveEvent): void {
    if (event.resolved) return;
    const reward = event.context.reward as { gold: number; exp: number } | undefined;
    const rescueNpcName = (event.context.rescueNpcName as string) ?? '被困的旅人';

    // Grant reward now that all hostiles are defeated
    if (reward) {
      this.player.gold += reward.gold;
      this.player.exp += reward.exp;
      EventBus.emit(GameEvents.LOG_MESSAGE, {
        text: `你救出了${rescueNpcName}! 获得 ${reward.gold} 金币和 ${reward.exp} 经验`,
        type: 'info',
      });
    }

    // Remove the NPC sprite
    const npcSprite = event.context.rescueNpcSpriteRef as Phaser.GameObjects.Container | undefined;
    if (npcSprite && npcSprite.scene) {
      npcSprite.destroy();
    }

    this.randomEventSystem.resolveActiveEvent();
    EventBus.emit(GameEvents.RANDOM_EVENT_RESOLVED, { type: 'rescue' });
  }

  private handlePuzzleEvent(event: ActiveEvent): void {
    const puzzle = event.context.puzzle as { prompt: string; solution: string; reward: string; rewardGold: number; rewardExp: number } | undefined;
    if (!puzzle) {
      this.randomEventSystem.resolveActiveEvent();
      return;
    }

    // Show puzzle prompt in combat log
    EventBus.emit(GameEvents.LOG_MESSAGE, { text: `谜题: ${puzzle.prompt}`, type: 'info' });

    // Spawn an interactable puzzle object near the event position
    const walkable = RandomEventSystem.findWalkableTile(
      Math.round(event.col), Math.round(event.row),
      this.mapData.collisions, this.mapData.cols, this.mapData.rows,
    );
    const puzzleCol = walkable?.col ?? Math.round(event.col);
    const puzzleRow = walkable?.row ?? Math.round(event.row);
    const { x: px2, y: py2 } = cartToIso(puzzleCol, puzzleRow);

    const puzzleSprite = this.add.container(px2, py2);
    puzzleSprite.setDepth(py2 + 10);
    // Visual: a glowing rune circle
    const glow = this.add.circle(0, -8 * DPR, 12 * DPR, 0xaa66ff, 0.4);
    const icon = this.add.text(0, -14 * DPR, '?', {
      fontSize: fs(16), color: '#ffcc00', fontFamily: '"Noto Sans SC", sans-serif', fontStyle: 'bold',
    }).setOrigin(0.5);
    const label = this.add.text(0, 8 * DPR, '谜题装置', {
      fontSize: fs(10), color: '#ccaaff', fontFamily: '"Noto Sans SC", sans-serif',
    }).setOrigin(0.5, 0);
    puzzleSprite.add([glow, icon, label]);
    puzzleSprite.setSize(24 * DPR, 32 * DPR);
    puzzleSprite.setInteractive({ useHandCursor: true });

    // Add pulsing animation to draw attention
    this.tweens.add({
      targets: glow,
      alpha: { from: 0.4, to: 0.8 },
      duration: 800,
      yoyo: true,
      repeat: -1,
    });

    // On click: show a choice/prompt overlay
    puzzleSprite.on('pointerdown', () => {
      if (event.resolved) return;
      this.showPuzzleChoicePrompt(event, puzzle, puzzleSprite);
    });
  }

  private showPuzzleChoicePrompt(
    event: ActiveEvent,
    puzzle: { prompt: string; solution: string; reward: string; rewardGold: number; rewardExp: number },
    puzzleSprite: Phaser.GameObjects.Container,
  ): void {
    // Emit a puzzle interaction event to the UI
    // We'll create a simple in-world popup with two choices
    const { x: sx, y: sy } = puzzleSprite;
    const popW = 200 * DPR;
    const popH = 100 * DPR;
    const popup = this.add.container(sx, sy - 50 * DPR);
    popup.setDepth(10000);

    const bg = this.add.rectangle(0, 0, popW, popH, 0x0a0a18, 0.95)
      .setOrigin(0.5)
      .setStrokeStyle(Math.round(1 * DPR), 0xc0934a);
    popup.add(bg);

    const promptText = this.add.text(0, -30 * DPR, puzzle.prompt, {
      fontSize: fs(11), color: '#e0d8cc', fontFamily: '"Noto Sans SC", sans-serif',
      wordWrap: { width: popW - 20 * DPR, useAdvancedWrap: true },
      align: 'center',
    }).setOrigin(0.5, 0.5);
    popup.add(promptText);

    // Correct choice button (the solution)
    const correctBtn = this.add.text(-40 * DPR, 20 * DPR, puzzle.solution, {
      fontSize: fs(10), color: '#44ff44', fontFamily: '"Noto Sans SC", sans-serif',
      wordWrap: { width: 80 * DPR, useAdvancedWrap: true },
      align: 'center',
    }).setOrigin(0.5).setInteractive({ useHandCursor: true });
    popup.add(correctBtn);

    // Wrong choice button
    const wrongBtn = this.add.text(40 * DPR, 20 * DPR, '离开', {
      fontSize: fs(10), color: '#ff4444', fontFamily: '"Noto Sans SC", sans-serif',
    }).setOrigin(0.5).setInteractive({ useHandCursor: true });
    popup.add(wrongBtn);

    correctBtn.on('pointerdown', () => {
      if (event.resolved) { popup.destroy(); return; }
      // Grant reward
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: `${puzzle.solution} — ${puzzle.reward}`, type: 'info' });
      this.player.gold += puzzle.rewardGold;
      this.player.exp += puzzle.rewardExp;
      EventBus.emit(GameEvents.LOG_MESSAGE, {
        text: `获得 ${puzzle.rewardGold} 金币和 ${puzzle.rewardExp} 经验`,
        type: 'info',
      });
      popup.destroy();
      puzzleSprite.destroy();
      this.randomEventSystem.resolveActiveEvent();
      EventBus.emit(GameEvents.RANDOM_EVENT_RESOLVED, { type: 'environmental_puzzle' });
    });

    wrongBtn.on('pointerdown', () => {
      // Dismiss without reward; puzzle remains for retry
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: '你离开了谜题装置。', type: 'info' });
      popup.destroy();
    });
  }

  /** Helper: drop a loot item at a specific tile position (used by treasure cache events). */
  private dropLootAtPosition(item: ItemInstance, col: number, row: number): void {
    const { x: wx, y: wy } = cartToIso(col, row);
    const offsetX = (Math.random() - 0.5) * 20 * DPR;
    const offsetY = (Math.random() - 0.5) * 10 * DPR;
    const finalX = wx + offsetX;
    const finalY = wy + offsetY;

    const container = this.add.container(finalX, finalY - 30 * DPR);
    container.setDepth(wy + 50);

    const colors: Record<string, number> = { normal: 0xffffff, magic: 0x4488ff, rare: 0xffff00, legendary: 0xff8800, set: 0x44ff44 };
    const color = colors[item.quality] ?? 0xffffff;

    const bag = this.add.rectangle(0, 0, 12 * DPR, 12 * DPR, color);
    container.add(bag);

    // Drop animation
    this.tweens.add({
      targets: container,
      y: finalY,
      duration: 400,
      ease: 'Bounce.easeOut',
    });

    this.lootDrops.push({ sprite: container, item, col, row });
  }

  private handleAutoLoot(): void {
    const qualityRank: Record<string, number> = { normal: 0, magic: 1, rare: 2, legendary: 3, set: 3 };
    const modeMinRank: Record<string, number> = { all: 0, magic: 1, rare: 2, legendary: 3 };
    const minRank = modeMinRank[this.player.autoLootMode] ?? 0;
    for (let i = this.lootDrops.length - 1; i >= 0; i--) {
      const loot = this.lootDrops[i];
      const rank = qualityRank[loot.item.quality] ?? 0;
      if (rank < minRank) continue;
      const dist = euclideanDistance(this.player.tileCol, this.player.tileRow, loot.col, loot.row);
      if (dist > 2) continue;
      if (this.inventorySystem.addItem(loot.item)) {
        EventBus.emit(GameEvents.ITEM_PICKED, { item: loot.item });
        if (loot.item.quality === 'legendary') this.achievementSystem.update('collect');
        // Fly-to-player animation
        this.tweens.killTweensOf(loot.sprite);
        this.tweens.add({
          targets: loot.sprite,
          x: this.player.sprite.x, y: this.player.sprite.y - 20,
          scale: 0.3, alpha: 0, duration: 250, ease: 'Power2',
          onComplete: () => loot.sprite.destroy(),
        });
        this.lootDrops.splice(i, 1);
      } else {
        break; // inventory full
      }
    }
  }

  private updateNPCQuestMarkers(): void {
    for (const npc of this.npcs) {
      const def = npc.definition;
      if (def.type !== 'quest' || !def.quests) continue;

      let hasCompletedQuest = false;
      let hasActiveQuest = false;
      let hasAvailableQuest = false;
      let isMainQuest = false;

      for (const qid of def.quests) {
        const prog = this.questSystem.progress.get(qid);
        if (prog) {
          if (prog.status === 'completed') hasCompletedQuest = true;
          else if (prog.status === 'active') hasActiveQuest = true;
        }
      }

      if (!hasCompletedQuest) {
        const available = this.questSystem.getAvailableQuests(def.quests, this.player.level);
        for (const q of available) {
          const qProg = this.questSystem.progress.get(q.id);
          if (!qProg || (qProg.status === 'failed' && q.reacceptable)) {
            hasAvailableQuest = true;
            if (q.category === 'main') isMainQuest = true;
            break;
          }
        }
      }

      if (hasCompletedQuest) {
        npc.setQuestMarker('?', '#f1c40f');
      } else if (hasAvailableQuest) {
        npc.setQuestMarker('!', isMainQuest ? '#f1c40f' : '#95a5a6');
      } else if (hasActiveQuest) {
        npc.setQuestMarker('?', '#888888');
      } else {
        npc.setQuestMarker('', '');
      }
    }
  }

  private checkExploreQuests(): void {
    const activeQuests = this.questSystem.getActiveQuests();
    for (const { quest, progress } of activeQuests) {
      if (progress.status !== 'active') continue;
      if (quest.zone !== this.currentMapId) continue;
      for (let i = 0; i < quest.objectives.length; i++) {
        const obj = quest.objectives[i];
        // Handle explore objectives
        if (obj.type === 'explore' && obj.location && progress.objectives[i].current < obj.required) {
          const dx = this.player.tileCol - obj.location.col;
          const dy = this.player.tileRow - obj.location.row;
          const dist = Math.sqrt(dx * dx + dy * dy);
          if (dist <= obj.location.radius) {
            this.questSystem.updateProgress('explore', obj.targetId);
            EventBus.emit(GameEvents.LOG_MESSAGE, {
              text: `发现: ${obj.targetName}`,
              type: 'system',
            });
          }
        }
        // Handle investigate clue objectives (location-based discovery)
        if (obj.type === 'investigate_clue' && obj.location && progress.objectives[i].current < obj.required) {
          const dx = this.player.tileCol - obj.location.col;
          const dy = this.player.tileRow - obj.location.row;
          const dist = Math.sqrt(dx * dx + dy * dy);
          if (dist <= obj.location.radius) {
            this.questSystem.updateProgress('investigate_clue', obj.targetId);
            EventBus.emit(GameEvents.LOG_MESSAGE, {
              text: `发现线索: ${obj.targetName}`,
              type: 'system',
            });
          }
        }
        // Escort destination check is handled by updateEscortNpc() —
        // completion is gated on the escort NPC arriving, not just the player.
      }
    }
  }

  private onMonsterKilled(monster: Monster): void {
    // Clear status effects on death
    this.statusEffects.clearEntity(monster.id);

    // Difficulty exp/gold scaling is already applied at monster spawn time via DifficultySystem.scaleMonster
    const homeBonus = this.homesteadSystem.getTotalBonuses();
    const eq = this.getEquipStats();
    const expBonus = 1 + (homeBonus['expBonus'] ?? 0) / 100 + (eq.expBonus ?? 0) / 100;
    const exp = Math.floor(monster.definition.expReward * expBonus);
    const gold = randomInt(monster.definition.goldReward[0], monster.definition.goldReward[1]);
    this.player.addExp(exp);
    this.player.gold += gold;

    // Mercenary exp share
    if (this.mercenarySystem?.isAlive()) {
      const trainingBonus = this.homesteadSystem.getTrainingGroundBonus();
      this.mercenarySystem.addExp(exp, trainingBonus);
    }

    // Kill heal (set bonus / legendary)
    if (eq.killHealPercent > 0 && this.player.hp > 0) {
      const heal = Math.floor(this.player.maxHp * eq.killHealPercent / 100);
      this.player.hp = Math.min(this.player.maxHp, this.player.hp + heal);
      EventBus.emit(GameEvents.PLAYER_HEALTH_CHANGED, { hp: this.player.hp, maxHp: this.player.maxHp });
      if (this.vfx) this.vfx.healBurst(this.player.sprite.x, this.player.sprite.y - 16, 8);
    }

    // Death particles + gold burst at monster death location
    if (this.vfx) {
      this.vfx.deathBurst(monster.sprite.x, monster.sprite.y - 16);
      this.vfx.goldBurst(monster.sprite.x, monster.sprite.y - 10, 6);
    }

    // Floating EXP/Gold text
    const expText = this.add.text(monster.sprite.x, monster.sprite.y - 40, `+${exp} EXP`, {
      fontSize: fs(13), color: '#b39ddb', fontFamily: '"Cinzel", serif',
      stroke: '#000000', strokeThickness: Math.round(2 * DPR),
    }).setOrigin(0.5).setDepth(ZONE_FLOATING_TEXT_DEPTH);
    this.tweens.add({ targets: expText, y: expText.y - 35, alpha: 0, duration: 1500, ease: 'Power2', onComplete: () => expText.destroy() });

    const goldText = this.add.text(monster.sprite.x + 15, monster.sprite.y - 28, `+${gold}G`, {
      fontSize: fs(13), color: '#ffd700', fontFamily: '"Cinzel", serif',
      stroke: '#000000', strokeThickness: Math.round(2 * DPR),
    }).setOrigin(0.5).setDepth(ZONE_FLOATING_TEXT_DEPTH);
    this.tweens.add({ targets: goldText, y: goldText.y - 30, alpha: 0, duration: 1200, ease: 'Power2', onComplete: () => goldText.destroy() });

    this.totalKills++;
    this.achievementSystem.update('kill', undefined, 1);
    this.achievementSystem.update('kill', monster.definition.id, 1);
    this.achievementSystem.checkLevel(this.player.level);

    this.questSystem.updateProgress('kill', monster.definition.id);

    // Difficulty completion check: killing demon_lord in Abyss Rift completes current difficulty
    if (!this.isInDungeon && DifficultySystem.shouldMarkCompleted(
      monster.definition.id, this.currentMapId, this.difficulty, this.completedDifficulties,
    )) {
      this.completedDifficulties.push(this.difficulty);
      // Persist difficulty completion immediately so it's not lost if game is closed
      this.autoSave();
      const unlocked = DifficultySystem.getNewlyUnlockedDifficulty(this.completedDifficulties);
      if (unlocked) {
        const msg = DIFFICULTY_UNLOCK_MESSAGES[unlocked];
        if (msg) {
          EventBus.emit(GameEvents.LOG_MESSAGE, { text: msg, type: 'system' });
          // Floating announcement text
          const cx = GAME_WIDTH * DPR / 2;
          const cy = GAME_HEIGHT * DPR / 3;
          const announce = this.add.text(cx, cy, msg, {
            fontSize: fs(24),
            color: '#ff8800',
            fontFamily: '"Noto Sans SC", sans-serif',
            fontStyle: 'bold',
            stroke: '#000000',
            strokeThickness: Math.round(3 * DPR),
          }).setOrigin(0.5).setDepth(ZONE_FLOATING_TEXT_DEPTH);
          this.tweens.add({
            targets: announce,
            y: cy - 60,
            alpha: 0,
            duration: 3000,
            ease: 'Power2',
            onComplete: () => announce.destroy(),
          });
        }
      }
    }

    // Boss pet drops: certain bosses have a chance to drop specific pets
    if (monster.definition.elite) {
      this.checkBossPetDrop(monster.definition.id);
    }

    // Progress collect quests: monsters in this zone drop quest collectibles
    const activeQuests = this.questSystem.getActiveQuests();
    for (const { quest, progress } of activeQuests) {
      if (progress.status !== 'active') continue;
      if (quest.zone !== this.currentMapId) continue;
      for (let i = 0; i < quest.objectives.length; i++) {
        const obj = quest.objectives[i];
        if (obj.type === 'collect' && progress.objectives[i].current < obj.required) {
          if (Math.random() < 0.4) {
            this.questSystem.updateProgress('collect', obj.targetId);
          }
          break;
        }
      }
    }

    const luckBonus = this.player.stats.lck + (homeBonus['magicFind'] ?? 0)
      + (this.isInDungeon && this.dungeonFloorConfig ? this.dungeonFloorConfig.magicFindBonus : 0);
    // Elite affix loot quality bonus + dungeon floor depth bonus
    const dungeonLootBonus = this.isInDungeon && this.dungeonFloorConfig ? this.dungeonFloorConfig.lootQualityBonus : 0;
    const affixLootBonus = (monster.eliteAffixes.length > 0
      ? this.eliteAffixSystem.getCombinedStats(monster.eliteAffixes).lootQualityBonus
      : 0) + dungeonLootBonus;
    const loot = this.lootSystem.generateLoot(monster.definition, luckBonus, affixLootBonus, this.difficulty);
    const potionAmounts: Record<string, { type: 'hp' | 'mp'; amount: number }> = {
      c_hp_potion_s: { type: 'hp', amount: 50 },
      c_hp_potion_m: { type: 'hp', amount: 150 },
      c_hp_potion_l: { type: 'hp', amount: 400 },
      c_mp_potion_s: { type: 'mp', amount: 30 },
      c_mp_potion_m: { type: 'mp', amount: 80 },
    };
    for (const item of loot) {
      const pot = potionAmounts[item.baseId];
      if (pot) {
        this.dropPotion(pot.type, pot.amount, monster.tileCol, monster.tileRow);
      } else {
        this.dropLoot(item, monster.tileCol, monster.tileRow);
      }
    }

    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: `击杀 ${monster.definition.name}! +${exp}EXP +${gold}G`,
      type: 'loot',
    });

    // Don't respawn mini-bosses
    if (this.miniBossMonster === monster) {
      this.miniBossMonster = null;
    } else {
      this.time.delayedCall(15000, () => this.respawnMonster(monster));
    }
  }

  private dropLoot(item: ItemInstance, col: number, row: number): void {
    const worldPos = cartToIso(col + Math.random() * 0.5, row + Math.random() * 0.5);
    const container = this.add.container(worldPos.x, worldPos.y);
    container.setDepth(worldPos.y + 30);

    SpriteGenerator.ensureEffect(this, 'loot_bag');
    if (this.textures.exists('loot_bag')) {
      const bag = this.add.image(0, 0, 'loot_bag').setScale(1 / TEXTURE_SCALE);
      const tintColor = this.getQualityColor(item.quality);
      if (item.quality !== 'normal') bag.setTint(tintColor);
      container.add(bag);
    } else {
      const color = this.getQualityColor(item.quality);
      const bg = this.add.rectangle(0, 0, 16, 16, color);
      bg.setStrokeStyle(1, 0xffffff);
      container.add(bg);
    }

    if (item.quality !== 'normal' && item.quality !== 'magic') {
      const glow = this.add.circle(0, 0, 14, this.getQualityColor(item.quality), 0.15);
      container.add(glow);
      container.sendToBack(glow);
    }

    // Apply FX glow based on quality
    if (this.vfx && item.quality !== 'normal') {
      this.vfx.applyLootGlow(container, item.quality);
    }

    // Floating item name label (D2-style)
    const qualityColors: Record<string, string> = {
      normal: '#cccccc', magic: '#6888ff', rare: '#f1c40f',
      legendary: '#ff8800', set: '#2ecc71',
    };
    const label = this.add.text(0, -18, item.name, {
      fontSize: item.quality === 'legendary' || item.quality === 'set' ? fs(13) : fs(12),
      color: qualityColors[item.quality] || '#cccccc',
      fontFamily: '"Cinzel", serif',
      stroke: '#000000', strokeThickness: Math.round(2 * DPR),
    }).setOrigin(0.5);
    container.add(label);

    // Emit for VFXManager camera effects (legendary/set flash)
    EventBus.emit(GameEvents.ITEM_DROPPED, { item });

    this.tweens.add({
      targets: container,
      y: container.y - 5,
      duration: 800,
      yoyo: true,
      repeat: -1,
      ease: 'Sine.easeInOut',
    });

    this.lootDrops.push({ sprite: container, item, col, row });

    this.time.delayedCall(60000, () => {
      const idx = this.lootDrops.findIndex(l => l.item.uid === item.uid);
      if (idx !== -1) {
        this.lootDrops[idx].sprite.destroy();
        this.lootDrops.splice(idx, 1);
      }
    });
  }

  private dropPotion(type: 'hp' | 'mp', amount: number, col: number, row: number): void {
    const worldPos = cartToIso(col + Math.random() * 0.5, row + Math.random() * 0.5);
    const container = this.add.container(worldPos.x, worldPos.y);
    container.setDepth(worldPos.y + 30);

    const color = type === 'hp' ? 0xcc2222 : 0x2244cc;
    const glowColor = type === 'hp' ? 0xff4444 : 0x4466ff;

    // Glow
    const glow = this.add.circle(0, 0, 12, glowColor, 0.25);
    container.add(glow);

    // Bottle body
    const body = this.add.rectangle(0, 1, 8, 10, color);
    body.setStrokeStyle(1, 0xffffff, 0.5);
    container.add(body);
    // Bottle cap
    const cap = this.add.circle(0, -5, 3, color);
    cap.setStrokeStyle(1, 0xffffff, 0.3);
    container.add(cap);

    // Bobbing animation
    this.tweens.add({
      targets: container,
      y: container.y - 5,
      duration: 600,
      yoyo: true,
      repeat: -1,
      ease: 'Sine.easeInOut',
    });

    this.potionDrops.push({ sprite: container, type, amount, col, row });

    this.time.delayedCall(30000, () => {
      const idx = this.potionDrops.findIndex(p => p.sprite === container);
      if (idx !== -1) {
        this.potionDrops[idx].sprite.destroy();
        this.potionDrops.splice(idx, 1);
      }
    });
  }

  private pickupLoot(lootDrop: { sprite: Phaser.GameObjects.Container; item: ItemInstance; col: number; row: number }): void {
    const dist = euclideanDistance(this.player.tileCol, this.player.tileRow, lootDrop.col, lootDrop.row);
    if (dist > 2) {
      const path = this.pathfinding.findPath(
        Math.round(this.player.tileCol), Math.round(this.player.tileRow),
        Math.round(lootDrop.col), Math.round(lootDrop.row),
      );
      this.player.setPath(path);
      return;
    }

    if (this.inventorySystem.addItem(lootDrop.item)) {
      EventBus.emit(GameEvents.ITEM_PICKED, { item: lootDrop.item });
      if (lootDrop.item.quality === 'legendary') {
        this.achievementSystem.update('collect');
      }
      // Particle burst on pickup
      if (this.vfx) {
        this.vfx.goldBurst(lootDrop.sprite.x, lootDrop.sprite.y, 8);
      }
      // Fly-to-player pickup animation
      const idx = this.lootDrops.indexOf(lootDrop);
      if (idx !== -1) this.lootDrops.splice(idx, 1);
      this.tweens.killTweensOf(lootDrop.sprite);
      this.tweens.add({
        targets: lootDrop.sprite,
        x: this.player.sprite.x,
        y: this.player.sprite.y - 20,
        scale: 0.3,
        alpha: 0,
        duration: 300,
        ease: 'Power2',
        onComplete: () => lootDrop.sprite.destroy(),
      });
    }
  }

  private interactNPC(npc: NPC): void {
    const def = npc.definition;
    EventBus.emit(GameEvents.LOG_MESSAGE, { text: def.dialogue[0], type: 'info' });

    // Progress talk quests
    this.questSystem.updateProgress('talk', def.id);

    // Wire craft quest phases: if NPC matches a craft quest's craftNpc or deliverNpc, advance progress
    this.advanceCraftQuestFromNpc(def.id);

    switch (def.type) {
      case 'blacksmith':
      case 'merchant':
        EventBus.emit(GameEvents.SHOP_OPEN, { npcId: def.id, shopItems: def.shopItems ?? [], type: def.type });
        break;
      case 'quest': {
        // Try to turn in completed quests first
        const turnedIn: string[] = [];
        if (def.quests) {
          for (const qid of def.quests) {
            const reward = this.questSystem.turnInQuest(qid);
            if (reward) {
              this.player.addExp(reward.exp);
              this.player.gold += reward.gold;
              this.achievementSystem.update('quest');
              turnedIn.push(qid);
              // Pet reward from quest
              if (reward.petReward) {
                this.homesteadSystem.addPet(reward.petReward);
              }
            }
          }
        }

        // If NPC has a dialogue tree, use the branching dialogue system
        if (def.dialogueTree) {
          // Collect turned-in quest IDs for the dialogue state
          const completedQuests: string[] = [];
          for (const [qid, prog] of this.questSystem.progress.entries()) {
            if (prog.status === 'turned_in') completedQuests.push(qid);
          }

          EventBus.emit(GameEvents.NPC_INTERACT, {
            npcId: def.id,
            npcName: def.name,
            dialogue: turnedIn.length > 0 ? '感谢你完成了任务！' : '',
            actions: [],
            dialogueTree: def.dialogueTree,
            completedQuests,
            questSystem: this.questSystem,
            player: this.player,
            homesteadSystem: this.homesteadSystem,
            achievementSystem: this.achievementSystem,
            turnedIn,
          });
          break;
        }

        // Fallback: linear dialogue (no dialogue tree defined)
        // Build dialogue actions for available quests
        const actions: { label: string; callback: () => void }[] = [];
        if (def.quests) {
          const available = this.questSystem.getAvailableQuests(def.quests, this.player.level);
          for (const q of available) {
            const prog = this.questSystem.progress.get(q.id);
            if (!prog || (prog.status === 'failed' && q.reacceptable)) {
              actions.push({
                label: `接受: ${q.name}`,
                callback: () => { this.questSystem.acceptQuest(q.id); },
              });
            }
          }
        }

        // Determine dialogue text
        let dialogueText = def.dialogue[0];
        if (turnedIn.length > 0) {
          dialogueText = '感谢你完成了任务！';
        } else if (actions.length === 0) {
          dialogueText = def.dialogue.length > 1 ? def.dialogue[1] : def.dialogue[0];
        }

        EventBus.emit(GameEvents.NPC_INTERACT, {
          npcId: def.id,
          npcName: def.name,
          dialogue: dialogueText,
          actions,
        });
        break;
      }
      case 'stash':
        EventBus.emit(GameEvents.UI_TOGGLE_PANEL, { panel: 'stash', npcId: def.id });
        break;
    }
  }

  private changeZone(targetMap: string, targetCol: number, targetRow: number): void {
    if (this.isTransitioning) return;
    this.isTransitioning = true;
    this.autoSave();

    const doRestart = () => {
      this.scene.restart({
        classId: this.player.classData.id,
        mapId: targetMap,
        targetCol,
        targetRow,
        miniBossDialogueSeen: [...this.miniBossDialogueSeen],
        loreCollected: [...this.loreCollected],
        discoveredHiddenAreas: [...this.discoveredHiddenAreas],
        playerStats: {
          level: this.player.level,
          exp: this.player.exp,
          gold: this.player.gold,
          hp: this.player.hp,
          mana: this.player.mana,
          stats: { ...this.player.stats },
          freeStatPoints: this.player.freeStatPoints,
          freeSkillPoints: this.player.freeSkillPoints,
          skillLevels: Object.fromEntries(this.player.skillLevels),
          buffs: [...this.player.buffs],
          autoCombat: this.player.autoCombat,
          autoLootMode: this.player.autoLootMode,
        },
      });
    };

    // Smooth fade-out transition
    if (this.vfx) {
      this.vfx.zoneTransition(doRestart);
    } else {
      doRestart();
    }
  }

  private checkExitProximity(): void {
    for (const exit of this.mapData.exits) {
      const dist = euclideanDistance(this.player.tileCol, this.player.tileRow, exit.col, exit.row);
      if (dist < 1.5) {
        if (this.isInDungeon) {
          // In dungeon: either advance to next floor or exit dungeon
          if (this.dungeonFloorConfig?.isBossFloor) {
            this.exitDungeon();
          } else {
            this.advanceDungeonFloor();
          }
        } else if (this.isInSubDungeon) {
          this.exitSubDungeon();
        } else {
          this.changeZone(exit.targetMap, exit.targetCol, exit.targetRow);
        }
        return;
      }
    }
  }

  private async returnToMenu(): Promise<void> {
    await this.autoSave();
    this.scene.stop('UIScene');
    this.scene.start('MenuScene');
    this.scene.stop();
  }

  private async autoSave(): Promise<void> {
    try {
      // When inside a random dungeon, save returns to Abyss Rift entrance (ephemeral runs)
      // Use the Abyss Rift camp entrance position, not the dungeon portal position
      const saveMapId = this.isInDungeon ? 'abyss_rift' : this.currentMapId;
      const saveTileCol = this.isInDungeon ? ZoneScene.ABYSS_ENTRANCE_COL : this.player.tileCol;
      const saveTileRow = this.isInDungeon ? ZoneScene.ABYSS_ENTRANCE_ROW : this.player.tileRow;

      await this.saveSystem.autoSave({
        id: 'autosave',
        version: CURRENT_SAVE_VERSION,
        timestamp: Date.now(),
        classId: this.player.classData.id,
        player: {
          level: this.player.level, exp: this.player.exp, gold: this.player.gold,
          hp: this.player.hp, maxHp: this.player.maxHp, mana: this.player.mana, maxMana: this.player.maxMana,
          stats: { ...this.player.stats }, freeStatPoints: this.player.freeStatPoints, freeSkillPoints: this.player.freeSkillPoints,
          skillLevels: Object.fromEntries(this.player.skillLevels),
          tileCol: saveTileCol, tileRow: saveTileRow, currentMap: saveMapId,
        },
        inventory: this.inventorySystem.inventory,
        equipment: this.inventorySystem.equipment as any,
        stash: this.inventorySystem.stash,
        quests: this.questSystem.getProgressData(),
        exploration: this.fogData,
        homestead: {
          buildings: this.homesteadSystem.buildings,
          pets: this.homesteadSystem.pets,
          activePet: this.homesteadSystem.activePet ?? undefined,
        },
        achievements: this.achievementSystem.getUnlockedData(),
        settings: { autoCombat: this.player.autoCombat, musicVolume: 0.5, sfxVolume: 0.7, autoLootMode: this.player.autoLootMode },
        difficulty: this.difficulty,
        completedDifficulties: [...this.completedDifficulties],
        mercenary: this.mercenarySystem?.toSaveData(),
        dialogueState: this.getDialogueState(),
        miniBossDialogueSeen: [...this.miniBossDialogueSeen],
        loreCollected: [...this.loreCollected],
        discoveredHiddenAreas: [...this.discoveredHiddenAreas],
      });
    } catch (_e) { /* silent fail */ }
  }

  private restoreFromSave(save: SaveData): void {
    // 1. Player stats
    this.player.level = save.player.level;
    this.player.exp = save.player.exp;
    this.player.gold = save.player.gold;
    this.player.stats = { ...save.player.stats };
    this.player.freeStatPoints = save.player.freeStatPoints;
    this.player.freeSkillPoints = save.player.freeSkillPoints;
    const sl = save.player.skillLevels;
    this.player.skillLevels = new Map(Array.isArray(sl) ? sl : Object.entries(sl));
    this.player.recalcDerived();
    this.player.hp = Math.min(save.player.hp, this.player.maxHp);
    this.player.mana = Math.min(save.player.mana, this.player.maxMana);

    // Position safety check: reset to nearest camp if saved position is unwalkable
    // (handles old 80x80 saves loaded in 120x120 maps, or positions that became walls)
    const resetPos = findNearestWalkablePosition(
      save.player.tileCol,
      save.player.tileRow,
      this.mapData.collisions,
      this.campPositions,
      this.mapData.cols,
      this.mapData.rows,
    );
    if (resetPos) {
      console.log('存档位置不可达，已重置至营地');
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: '存档位置不可达，已重置至营地', type: 'system' });
      this.player.moveTo(resetPos.col, resetPos.row);
    } else {
      this.player.moveTo(save.player.tileCol, save.player.tileRow);
    }

    // 2. Inventory (mark all items identified as temp fix)
    const identifyAll = (items: ItemInstance[]) => { for (const i of items) i.identified = true; };
    this.inventorySystem.inventory = save.inventory ?? [];
    identifyAll(this.inventorySystem.inventory);
    this.inventorySystem.equipment = (save.equipment ?? {}) as any;
    for (const item of Object.values(this.inventorySystem.equipment)) {
      if (item) item.identified = true;
    }
    this.inventorySystem.stash = save.stash ?? [];
    identifyAll(this.inventorySystem.stash);

    // 3. Quests
    if (save.quests) this.questSystem.loadProgress(save.quests);

    // 4. Homestead
    if (save.homestead) {
      this.homesteadSystem.buildings = save.homestead.buildings ?? {};
      this.homesteadSystem.pets = (save.homestead.pets ?? []).map(p => ({
        petId: p.petId,
        level: p.level,
        exp: p.exp,
        evolved: p.evolved ?? 0,
      }));
      this.homesteadSystem.activePet = save.homestead.activePet ?? null;
    }

    // 5. Achievements
    if (save.achievements) this.achievementSystem.loadData(save.achievements);

    // 6. Exploration fog data (restored into fogData, picked up by fog init)
    if (save.exploration) this.fogData = save.exploration;

    // 7. Settings
    this.player.autoCombat = save.settings?.autoCombat ?? false;
    this.player.autoLootMode = save.settings?.autoLootMode ?? 'off';
    this.difficulty = save.difficulty ?? 'normal';
    this.completedDifficulties = save.completedDifficulties ?? [];

    // 8. Mercenary
    if (save.mercenary) {
      this.mercenarySystem.loadFromSave(save.mercenary);
    }

    // 9. Dialogue tree state
    if (save.dialogueState) {
      this.setDialogueState(save.dialogueState);
    }

    // 10. Mini-boss dialogue seen
    if (save.miniBossDialogueSeen) {
      this.miniBossDialogueSeen = new Set(save.miniBossDialogueSeen);
    }

    // 11. Lore collectibles collected
    if (save.loreCollected) {
      this.loreCollected = new Set(save.loreCollected);
    }

    // 12. Discovered hidden areas
    if ((save as any).discoveredHiddenAreas) {
      this.discoveredHiddenAreas = new Set((save as any).discoveredHiddenAreas);
    }
  }

  /** Get dialogue state from UIScene for saving. */
  private getDialogueState(): Record<string, { visitedNodes: string[]; choicesMade: Record<string, string> }> | undefined {
    const uiScene = this.scene.get('UIScene') as UIScene | undefined;
    if (uiScene && typeof uiScene.getDialogueState === 'function') {
      return uiScene.getDialogueState();
    }
    return undefined;
  }

  /** Set dialogue state on UIScene from loaded save. */
  private setDialogueState(state: Record<string, { visitedNodes: string[]; choicesMade: Record<string, string> }>): void {
    const uiScene = this.scene.get('UIScene') as UIScene | undefined;
    if (uiScene && typeof uiScene.setDialogueState === 'function') {
      uiScene.setDialogueState(state);
    }
  }

  private spawnMonsters(): void {
    const monsterDefs = MonstersByZone[this.currentMapId] || [];
    const safeRadius = this.mapData.safeZoneRadius ?? 9;
    for (const spawn of this.mapData.spawns) {
      let def = monsterDefs.find(m => m.id === spawn.monsterId) || getMonsterDef(spawn.monsterId);
      if (!def) continue;

      // Apply dungeon depth scaling if inside random dungeon
      if (this.isInDungeon && this.dungeonFloorConfig && this.dungeonRunState) {
        def = DungeonSystem.scaleMonster(def, this.dungeonFloorConfig, this.dungeonRunState.difficulty);
      } else {
        // Apply overworld difficulty scaling (Nightmare/Hell HP, damage, defense, exp)
        def = DifficultySystem.scaleMonster(def, this.difficulty);
      }

      for (let i = 0; i < spawn.count; i++) {
        const c = Math.max(1, Math.min(this.mapData.cols - 2, spawn.col + randomInt(-3, 3)));
        const r = Math.max(1, Math.min(this.mapData.rows - 2, spawn.row + randomInt(-3, 3)));
        if (this.mapData.collisions[r][c]) {
          // Reject spawns inside camp safe zones
          let inSafeZone = false;
          for (const camp of this.campPositions) {
            if (euclideanDistance(c, r, camp.col, camp.row) < safeRadius) {
              inSafeZone = true;
              break;
            }
          }
          if (inSafeZone) continue;
          const monster = new Monster(this, def, c, r);
          // Apply elite affixes
          if (def.elite) {
            const affixes = this.eliteAffixSystem.rollAffixes(this.currentMapId, true);
            if (affixes.length > 0) {
              monster.applyEliteAffixes(affixes, this.eliteAffixSystem);
            }
          }
          this.monsters.push(monster);
        }
      }
    }
  }

  private spawnNPCs(): void {
    const npcOffsets: { dc: number; dr: number }[] = [
      { dc: -3, dr: -2 },  // Upper-left tent
      { dc: 3, dr: -2 },   // Upper-right tent
      { dc: -3, dr: 2 },   // Lower-left tent
      { dc: 3, dr: 2 },    // Lower-right tent
      { dc: 0, dr: -3 },   // Fallback: north center
      { dc: 0, dr: 3 },    // Fallback: south center
    ];
    for (const camp of this.mapData.camps) {
      camp.npcs.forEach((npcId, i) => {
        const def = NPCDefinitions[npcId];
        if (!def) return;
        const offset = npcOffsets[i % npcOffsets.length];
        const npc = new NPC(this, def, camp.col + offset.dc, camp.row + offset.dr);
        this.npcs.push(npc);
      });
    }
  }

  /** Spawn rare pet encounter points defined in map data. Each has a low probability
   *  of appearing; when present, a glowing visual indicator is rendered on the tile.
   *  Walking near the indicator auto-captures the pet. */
  private spawnRarePets(): void {
    // Destroy any leftover sprites from a previous zone
    for (const ps of this.petSpawnSprites) {
      ps.sprite.destroy();
    }
    this.petSpawnSprites = [];
    if (!this.mapData.petSpawns) return;

    for (const spawn of this.mapData.petSpawns) {
      // Low probability roll: skip if not spawned this session
      if (Math.random() >= spawn.chance) continue;
      // Already own this pet? skip
      if (this.homesteadSystem.pets.some(p => p.petId === spawn.petId)) continue;

      const { x: worldX, y: worldY } = cartToIso(spawn.col, spawn.row);
      const container = this.add.container(worldX, worldY);
      container.setDepth(worldY + 100);

      // Glow circle indicator
      const glow = this.add.ellipse(0, 0, 28 * DPR, 14 * DPR, 0xaa44ff, 0.5);
      container.add(glow);

      // Pulsing butterfly icon (small colored rectangle as a procedural sprite)
      const wing = this.add.rectangle(0, -10 * DPR, 12 * DPR, 8 * DPR, 0xcc66ff);
      container.add(wing);

      // Floating label
      const label = this.add.text(0, -22 * DPR, '✦ 虚空蝶', {
        fontFamily: 'serif',
        fontSize: fs(10),
        color: '#cc88ff',
        stroke: '#000000',
        strokeThickness: 2 * DPR,
      }).setOrigin(0.5, 1);
      container.add(label);

      // Pulsing animation on the glow
      this.tweens.add({
        targets: glow,
        alpha: { from: 0.3, to: 0.7 },
        scaleX: { from: 0.9, to: 1.1 },
        scaleY: { from: 0.9, to: 1.1 },
        duration: 1200,
        yoyo: true,
        repeat: -1,
        ease: 'Sine.easeInOut',
      });
      // Gentle float on the wing
      this.tweens.add({
        targets: wing,
        y: -14 * DPR,
        duration: 800,
        yoyo: true,
        repeat: -1,
        ease: 'Sine.easeInOut',
      });

      this.petSpawnSprites.push({ sprite: container, col: spawn.col, row: spawn.row, petId: spawn.petId });
    }
  }

  /** Check if the player is close enough to a rare pet spawn to capture it. */
  private checkRarePetPickup(): void {
    for (let i = this.petSpawnSprites.length - 1; i >= 0; i--) {
      const ps = this.petSpawnSprites[i];
      const dist = euclideanDistance(this.player.tileCol, this.player.tileRow, ps.col, ps.row);
      if (dist <= 2) {
        const success = this.homesteadSystem.addPet(ps.petId);
        if (success) {
          EventBus.emit(GameEvents.LOG_MESSAGE, {
            text: `发现了稀有宠物: 虚空蝶! 已收入宠物小屋。`,
            type: 'system',
          });
        }
        // Fade-out and destroy the spawn sprite
        this.tweens.add({
          targets: ps.sprite,
          alpha: 0,
          duration: 400,
          onComplete: () => { ps.sprite.destroy(); },
        });
        this.petSpawnSprites.splice(i, 1);
      }
    }
  }

  // ─── Mini-Boss System ─────────────────────────────────────────────────

  /** Spawn the zone's mini-boss at its fixed position. */
  private spawnMiniBoss(): void {
    this.miniBossMonster = null;

    // Sub-dungeon mini-boss
    if (this.isInSubDungeon) {
      const subDungeonId = this.currentMapId;
      const subDungeonData = AllSubDungeons[subDungeonId];
      if (subDungeonData) {
        const bossId = subDungeonData.miniBoss.monsterId;
        let bossDef = SubDungeonMiniBosses[bossId];
        if (bossDef) {
          // Apply difficulty scaling to sub-dungeon mini-boss
          bossDef = DifficultySystem.scaleMonster(bossDef, this.difficulty);
          const c = subDungeonData.miniBoss.col;
          const r = subDungeonData.miniBoss.row;
          if (c >= 0 && c < this.mapData.cols && r >= 0 && r < this.mapData.rows) {
            const monster = new Monster(this, bossDef, c, r);
            const affixes = this.eliteAffixSystem.rollAffixes(subDungeonData.parentZone, true);
            if (affixes.length > 0) {
              monster.applyEliteAffixes(affixes, this.eliteAffixSystem);
            }
            this.monsters.push(monster);
            this.miniBossMonster = monster;
          }
        }
      }
      return;
    }

    // Regular zone mini-boss
    let miniBossDef = MiniBossByZone[this.currentMapId];
    const spawnPos = MiniBossSpawns[this.currentMapId];
    if (!miniBossDef || !spawnPos) return;

    // Apply difficulty scaling to zone mini-boss
    miniBossDef = DifficultySystem.scaleMonster(miniBossDef, this.difficulty);

    const c = spawnPos.col;
    const r = spawnPos.row;
    if (c < 0 || c >= this.mapData.cols || r < 0 || r >= this.mapData.rows) return;

    const monster = new Monster(this, miniBossDef, c, r);
    // Apply elite affixes
    const affixes = this.eliteAffixSystem.rollAffixes(this.currentMapId, true);
    if (affixes.length > 0) {
      monster.applyEliteAffixes(affixes, this.eliteAffixSystem);
    }
    this.monsters.push(monster);
    this.miniBossMonster = monster;
  }

  /** Check if the player is within aggro range of the mini-boss and trigger dialogue. */
  private checkMiniBossDialogue(): void {
    if (this.miniBossDialogueActive) return;
    if (!this.miniBossMonster || !this.miniBossMonster.isAlive()) return;

    const mb = this.miniBossMonster;
    const dist = euclideanDistance(this.player.tileCol, this.player.tileRow, mb.tileCol, mb.tileRow);
    if (dist > mb.definition.aggroRange) return;

    // Already seen dialogue for this mini-boss?
    if (this.miniBossDialogueSeen.has(mb.definition.id)) return;

    // Freeze monster state — prevent chase/attack until dialogue dismissed
    mb.state = 'idle';
    this.miniBossDialogueActive = true;
    this.miniBossDialogueSeen.add(mb.definition.id);

    // Get dialogue tree
    const dialogueTree = MiniBossDialogues[mb.definition.id];
    if (!dialogueTree) {
      this.miniBossDialogueActive = false;
      return;
    }

    // Show cinematic dialogue via EventBus → UIScene
    EventBus.emit(GameEvents.MINIBOSS_DIALOGUE, {
      bossName: mb.definition.name,
      dialogueTree,
      onDismiss: () => {
        this.miniBossDialogueActive = false;
        // Force aggro after dialogue
        mb.state = 'chase';
      },
    });
  }

  // ─── Lore Collectibles System ─────────────────────────────────────────

  /** Spawn lore collectible visual sprites for the current zone. */
  private spawnLoreCollectibles(): void {
    // Clean up previous zone lore sprites
    for (const ls of this.loreSprites) {
      ls.sprite.destroy();
    }
    this.loreSprites = [];

    const loreEntries = LoreByZone[this.currentMapId];
    if (!loreEntries) return;

    for (const entry of loreEntries) {
      // Skip already-collected entries
      if (this.loreCollected.has(entry.id)) continue;

      const { x: worldX, y: worldY } = cartToIso(entry.col, entry.row);
      const container = this.add.container(worldX, worldY);
      container.setDepth(worldY + 80);

      // Distinct visual per sprite type
      const visual = this.createLoreVisual(entry.spriteType);
      container.add(visual.elements);

      // Floating label
      const label = this.add.text(0, -28 * DPR, `✦ ${entry.name}`, {
        fontFamily: '"Noto Sans SC", sans-serif',
        fontSize: fs(9),
        color: this.getLoreSpriteColor(entry.spriteType),
        stroke: '#000000',
        strokeThickness: 2 * DPR,
      }).setOrigin(0.5, 1);
      container.add(label);

      // Pulsing glow animation
      if (visual.glow) {
        this.tweens.add({
          targets: visual.glow,
          alpha: { from: 0.3, to: 0.7 },
          scaleX: { from: 0.9, to: 1.1 },
          scaleY: { from: 0.9, to: 1.1 },
          duration: 1500,
          yoyo: true,
          repeat: -1,
          ease: 'Sine.easeInOut',
        });
      }

      this.loreSprites.push({ sprite: container, entry });
    }
  }

  /** Create distinct visual elements for a lore sprite type. */
  private createLoreVisual(spriteType: string): { elements: Phaser.GameObjects.GameObject[]; glow: Phaser.GameObjects.GameObject | null } {
    const elements: Phaser.GameObjects.GameObject[] = [];
    let glow: Phaser.GameObjects.GameObject | null = null;

    switch (spriteType) {
      case 'ancient_tablet': {
        const base = this.add.rectangle(0, -6 * DPR, 16 * DPR, 20 * DPR, 0x8B7355);
        base.setStrokeStyle(1, 0x5C4033);
        elements.push(base);
        const rune = this.add.rectangle(0, -8 * DPR, 8 * DPR, 3 * DPR, 0xDAA520);
        elements.push(rune);
        glow = this.add.ellipse(0, 0, 24 * DPR, 12 * DPR, 0xDAA520, 0.4);
        elements.push(glow);
        break;
      }
      case 'old_scroll': {
        const scroll = this.add.rectangle(0, -6 * DPR, 18 * DPR, 12 * DPR, 0xF5DEB3);
        scroll.setStrokeStyle(1, 0xA0522D);
        elements.push(scroll);
        const rod1 = this.add.rectangle(-8 * DPR, -6 * DPR, 3 * DPR, 14 * DPR, 0x8B4513);
        elements.push(rod1);
        const rod2 = this.add.rectangle(8 * DPR, -6 * DPR, 3 * DPR, 14 * DPR, 0x8B4513);
        elements.push(rod2);
        glow = this.add.ellipse(0, 0, 24 * DPR, 12 * DPR, 0xF5DEB3, 0.3);
        elements.push(glow);
        break;
      }
      case 'crystal_shard': {
        const crystal = this.add.triangle(0, -10 * DPR, 0, 16 * DPR, 8 * DPR, 0, -8 * DPR, 0, 0x66CCFF);
        elements.push(crystal);
        glow = this.add.ellipse(0, 0, 22 * DPR, 11 * DPR, 0x66CCFF, 0.5);
        elements.push(glow);
        break;
      }
      case 'carved_stone': {
        const stone = this.add.rectangle(0, -4 * DPR, 20 * DPR, 14 * DPR, 0x808080);
        stone.setStrokeStyle(1, 0x505050);
        elements.push(stone);
        const carving = this.add.rectangle(0, -5 * DPR, 12 * DPR, 6 * DPR, 0xA9A9A9);
        elements.push(carving);
        glow = this.add.ellipse(0, 0, 26 * DPR, 13 * DPR, 0xB0C4DE, 0.3);
        elements.push(glow);
        break;
      }
      case 'torn_journal': {
        const book = this.add.rectangle(0, -6 * DPR, 14 * DPR, 16 * DPR, 0xDEB887);
        book.setStrokeStyle(1, 0x8B4513);
        elements.push(book);
        const spine = this.add.rectangle(-6 * DPR, -6 * DPR, 3 * DPR, 16 * DPR, 0x654321);
        elements.push(spine);
        glow = this.add.ellipse(0, 0, 20 * DPR, 10 * DPR, 0xDEB887, 0.3);
        elements.push(glow);
        break;
      }
      case 'rune_pillar': {
        const pillar = this.add.rectangle(0, -10 * DPR, 10 * DPR, 24 * DPR, 0x4B0082);
        pillar.setStrokeStyle(1, 0x2F0060);
        elements.push(pillar);
        const rune1 = this.add.rectangle(0, -14 * DPR, 6 * DPR, 3 * DPR, 0x9370DB);
        elements.push(rune1);
        const rune2 = this.add.rectangle(0, -6 * DPR, 6 * DPR, 3 * DPR, 0x9370DB);
        elements.push(rune2);
        glow = this.add.ellipse(0, 0, 20 * DPR, 10 * DPR, 0x9370DB, 0.5);
        elements.push(glow);
        break;
      }
      default: {
        const defaultObj = this.add.rectangle(0, -6 * DPR, 14 * DPR, 14 * DPR, 0xCCCCCC);
        elements.push(defaultObj);
        glow = this.add.ellipse(0, 0, 20 * DPR, 10 * DPR, 0xCCCCCC, 0.3);
        elements.push(glow);
        break;
      }
    }

    return { elements, glow };
  }

  /** Get display color for a lore sprite type label. */
  private getLoreSpriteColor(spriteType: string): string {
    switch (spriteType) {
      case 'ancient_tablet': return '#DAA520';
      case 'old_scroll': return '#DEB887';
      case 'crystal_shard': return '#66CCFF';
      case 'carved_stone': return '#B0C4DE';
      case 'torn_journal': return '#D2B48C';
      case 'rune_pillar': return '#9370DB';
      default: return '#CCCCCC';
    }
  }

  /** Check if the player is close enough to a lore collectible to interact. */
  private checkLorePickup(): void {
    for (let i = this.loreSprites.length - 1; i >= 0; i--) {
      const ls = this.loreSprites[i];
      const dist = euclideanDistance(this.player.tileCol, this.player.tileRow, ls.entry.col, ls.entry.row);
      if (dist <= 2) {
        // Collect the lore entry
        this.loreCollected.add(ls.entry.id);

        // Show lore text via EventBus → UIScene
        EventBus.emit(GameEvents.LORE_COLLECTED, {
          entry: ls.entry,
        });

        EventBus.emit(GameEvents.LOG_MESSAGE, {
          text: `发现传说: ${ls.entry.name}`,
          type: 'system',
        });

        // Fade out and destroy sprite
        this.tweens.add({
          targets: ls.sprite,
          alpha: 0,
          scaleX: 1.3, scaleY: 1.3,
          duration: 500,
          onComplete: () => { ls.sprite.destroy(); },
        });
        this.loreSprites.splice(i, 1);
      }
    }
  }

  /** Get lore collected IDs — used by UIScene for lore log. */
  getLoreCollected(): Set<string> {
    return this.loreCollected;
  }

  // ─── Zone Content Wiring: Field NPCs ─────────────────────────────────

  /** Spawn field NPCs defined in mapData.fieldNpcs at their map positions. */
  private spawnFieldNPCs(): void {
    if (!this.mapData.fieldNpcs) return;
    for (const fieldNpc of this.mapData.fieldNpcs) {
      const def = NPCDefinitions[fieldNpc.npcId];
      if (!def) continue;
      const npc = new NPC(this, def, fieldNpc.col, fieldNpc.row);
      this.npcs.push(npc);
    }
  }

  // ─── Zone Content Wiring: Hidden Areas ───────────────────────────────

  /** Update explored tiles based on player's current position and view radius. */
  private updateExploredTiles(): void {
    const pc = this.player.tileCol;
    const pr = this.player.tileRow;
    const vr = ZoneScene.EXPLORE_VIEW_RADIUS;
    const minC = Math.max(0, Math.floor(pc - vr));
    const maxC = Math.min(this.mapData.cols - 1, Math.ceil(pc + vr));
    const minR = Math.max(0, Math.floor(pr - vr));
    const maxR = Math.min(this.mapData.rows - 1, Math.ceil(pr + vr));
    for (let r = minR; r <= maxR; r++) {
      for (let c = minC; c <= maxC; c++) {
        const dist = Math.sqrt((c - pc) ** 2 + (r - pr) ** 2);
        if (dist <= vr) {
          this.exploredTiles.add(`${c},${r}`);
        }
      }
    }
  }

  /** Get hidden area bounds — uses explicit bounds or derives from center/radius. */
  private getHiddenAreaBounds(area: import('../data/types').HiddenArea): { startCol: number; startRow: number; endCol: number; endRow: number } {
    return {
      startCol: area.startCol ?? (area.col - area.radius),
      startRow: area.startRow ?? (area.row - area.radius),
      endCol: area.endCol ?? (area.col + area.radius),
      endRow: area.endRow ?? (area.row + area.radius),
    };
  }

  /** Check if the player has explored (cleared fog over) the rectangular bounds of a hidden area. */
  private isHiddenAreaExplored(area: import('../data/types').HiddenArea): boolean {
    const bounds = this.getHiddenAreaBounds(area);
    // Check if all four corners + center of the bounds have been explored
    const checkPoints = [
      { c: bounds.startCol, r: bounds.startRow },  // top-left
      { c: bounds.endCol, r: bounds.startRow },     // top-right
      { c: bounds.startCol, r: bounds.endRow },     // bottom-left
      { c: bounds.endCol, r: bounds.endRow },        // bottom-right
      { c: area.col, r: area.row },                  // center
    ];
    return checkPoints.every(p => this.exploredTiles.has(`${p.c},${p.r}`));
  }

  /** Check if player has explored the fog over any hidden area bounds and reveal rewards. */
  private checkHiddenAreaDiscovery(): void {
    if (!this.mapData.hiddenAreas) return;
    for (const area of this.mapData.hiddenAreas) {
      if (this.discoveredHiddenAreas.has(area.id)) continue;
      if (this.isHiddenAreaExplored(area)) {
        this.discoverHiddenArea(area);
      }
    }
  }

  /** Reveal a hidden area — show discovery text, spawn reward sprites. */
  private discoverHiddenArea(area: HiddenArea): void {
    this.discoveredHiddenAreas.add(area.id);

    // Emit discovery event
    EventBus.emit(GameEvents.HIDDEN_AREA_DISCOVERED, { area });

    // Show discovery floating text
    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: `发现隐藏区域: ${area.name}`,
      type: 'system',
    });

    // Show discovery text as banner
    const dp = this.screenPos(0.5, 0.3);
    const discoveryBanner = this.add.text(dp.x, dp.y, area.discoveryText, {
      fontSize: fs(14),
      color: '#FFD700',
      fontFamily: '"Cinzel", serif',
      stroke: '#000000',
      strokeThickness: Math.round(3 * DPR),
      wordWrap: { width: Math.round(400 * DPR), useAdvancedWrap: true },
      align: 'center',
    }).setOrigin(0.5).setScrollFactor(0).setDepth(ZONE_FLOATING_TEXT_DEPTH).setAlpha(0);
    this.tweens.add({
      targets: discoveryBanner, alpha: 1, duration: 300, ease: 'Power2',
      hold: 3000,
      yoyo: true,
      onComplete: () => discoveryBanner.destroy(),
    });

    // Spawn reward sprites
    for (let i = 0; i < area.rewards.length; i++) {
      const reward = area.rewards[i];
      this.spawnHiddenAreaRewardSprite(area, reward, i);
    }
  }

  /** Spawn a reward sprite (chest or gold pile) for a hidden area reward. */
  private spawnHiddenAreaRewardSprite(area: HiddenArea, reward: import('../data/types').HiddenAreaReward, rewardIndex: number): void {
    const { x: worldX, y: worldY } = cartToIso(reward.col, reward.row);
    const container = this.add.container(worldX, worldY);
    container.setDepth(worldY + 100);

    if (reward.type === 'chest') {
      // Treasure chest sprite (golden rectangle)
      const chest = this.add.rectangle(0, -12, Math.round(20 * DPR), Math.round(14 * DPR), 0xDAA520);
      chest.setStrokeStyle(Math.round(2 * DPR), 0x8B6914);
      container.add(chest);
      // Chest lid
      const lid = this.add.rectangle(0, -20, Math.round(22 * DPR), Math.round(6 * DPR), 0xCD853F);
      lid.setStrokeStyle(Math.round(1 * DPR), 0x8B6914);
      container.add(lid);
      // Glow effect
      const glow = this.add.ellipse(0, 0, Math.round(36 * DPR), Math.round(18 * DPR), 0xFFD700, 0.3);
      container.add(glow);
      this.tweens.add({ targets: glow, alpha: 0.1, duration: 800, yoyo: true, repeat: -1, ease: 'Sine.easeInOut' });
    } else if (reward.type === 'gold_pile') {
      // Gold pile (stacked gold circles)
      for (let j = 0; j < 5; j++) {
        const coin = this.add.ellipse(
          randomInt(-6, 6) * DPR, (-8 - j * 3) * DPR,
          Math.round(8 * DPR), Math.round(6 * DPR), 0xFFD700
        );
        coin.setStrokeStyle(Math.round(1 * DPR), 0xDAA520);
        container.add(coin);
      }
    } else if (reward.type === 'lore') {
      // Lore scroll visual
      const scroll = this.add.rectangle(0, -12, Math.round(14 * DPR), Math.round(18 * DPR), 0xDEB887);
      scroll.setStrokeStyle(Math.round(1 * DPR), 0x8B7355);
      container.add(scroll);
    }

    // Interactable label
    const label = this.add.text(0, -30 * DPR, reward.type === 'chest' ? '宝箱' : reward.type === 'gold_pile' ? '金币堆' : '卷轴', {
      fontSize: fs(9),
      color: '#FFD700',
      fontFamily: '"Noto Sans SC", sans-serif',
      stroke: '#000000',
      strokeThickness: Math.round(2 * DPR),
    }).setOrigin(0.5);
    container.add(label);

    container.setSize(Math.round(40 * DPR), Math.round(40 * DPR));
    container.setInteractive();

    this.hiddenAreaSprites.push({ sprite: container, area, rewardIndex, col: reward.col, row: reward.row });
  }

  /** Find a hidden area chest near a given tile coordinate. */
  private findHiddenAreaChestAt(col: number, row: number): { sprite: Phaser.GameObjects.Container; area: HiddenArea; rewardIndex: number; col: number; row: number } | null {
    for (const hs of this.hiddenAreaSprites) {
      if (Math.abs(hs.col - col) < 1.5 && Math.abs(hs.row - row) < 1.5) return hs;
    }
    return null;
  }

  /** Collect a hidden area reward — grant items/gold/exp and destroy the sprite. */
  private collectHiddenAreaReward(entry: { sprite: Phaser.GameObjects.Container; area: HiddenArea; rewardIndex: number; col: number; row: number }): void {
    const reward = entry.area.rewards[entry.rewardIndex];
    if (!reward) return;

    if (reward.type === 'chest') {
      // Generate a loot drop based on quality
      const quality = reward.value as string || 'magic';
      const item = this.lootSystem.generateEquipment(
        this.mapData.levelRange[1],
        quality === 'legendary' ? 'legendary' : quality === 'rare' ? 'rare' : 'magic',
      );
      if (item) {
        this.inventorySystem.addItem(item);
        EventBus.emit(GameEvents.LOG_MESSAGE, {
          text: `从宝箱中获得: ${item.name}`,
          type: 'loot',
        });
        EventBus.emit(GameEvents.INVENTORY_CHANGED, {});
      }
    } else if (reward.type === 'gold_pile') {
      const goldAmount = parseInt(reward.value || '100', 10);
      this.player.gold += goldAmount;
      EventBus.emit(GameEvents.LOG_MESSAGE, {
        text: `获得 ${goldAmount} 金币`,
        type: 'loot',
      });
    } else if (reward.type === 'lore') {
      EventBus.emit(GameEvents.LOG_MESSAGE, {
        text: `发现远古卷轴`,
        type: 'system',
      });
    }

    // Animate and destroy
    this.tweens.add({
      targets: entry.sprite,
      alpha: 0, scaleX: 1.3, scaleY: 1.3,
      duration: 400,
      onComplete: () => { entry.sprite.destroy(); },
    });

    const idx = this.hiddenAreaSprites.indexOf(entry);
    if (idx !== -1) this.hiddenAreaSprites.splice(idx, 1);
  }

  // ─── Random Dungeon Portal & Floor Transitions ───────────────────────

  /** Spawn the dungeon portal in Abyss Rift zone. */
  private spawnDungeonPortal(): void {
    // Only spawn portal in abyss_rift zone, not inside dungeons or sub-dungeons
    if (this.currentMapId !== 'abyss_rift' || this.isInSubDungeon || this.isInDungeon) return;

    const portalCol = ZoneScene.DUNGEON_PORTAL_COL;
    const portalRow = ZoneScene.DUNGEON_PORTAL_ROW;

    // Ensure portal area is walkable
    for (let dr = -2; dr <= 2; dr++) {
      for (let dc = -2; dc <= 2; dc++) {
        const r = portalRow + dr;
        const c = portalCol + dc;
        if (r > 0 && r < this.mapData.rows - 1 && c > 0 && c < this.mapData.cols - 1) {
          this.mapData.collisions[r][c] = true;
          if (this.mapData.tiles[r] && this.mapData.tiles[r][c] !== undefined) {
            this.mapData.tiles[r][c] = 2; // stone
          }
        }
      }
    }

    const { x: worldX, y: worldY } = cartToIso(portalCol, portalRow);
    const container = this.add.container(worldX, worldY);
    container.setDepth(worldY + 100);

    // Portal base — larger and visually distinct from exit/sub-dungeon portals
    const portalBase = this.add.ellipse(0, 0, Math.round(40 * DPR), Math.round(20 * DPR), 0x440000, 0.8);
    container.add(portalBase);

    // Outer ring — crimson/dark-red glow (distinct from purple sub-dungeon portals)
    const outerRing = this.add.ellipse(0, -20 * DPR, Math.round(36 * DPR), Math.round(44 * DPR));
    outerRing.setStrokeStyle(Math.round(4 * DPR), 0xFF3300);
    outerRing.setFillStyle(0x880000, 0.35);
    container.add(outerRing);

    // Inner glow — fiery red-orange
    const innerGlow = this.add.ellipse(0, -20 * DPR, Math.round(22 * DPR), Math.round(30 * DPR), 0xFF6600, 0.5);
    container.add(innerGlow);

    // Core — bright center
    const core = this.add.ellipse(0, -20 * DPR, Math.round(10 * DPR), Math.round(14 * DPR), 0xFFAA00, 0.6);
    container.add(core);

    // Pulsing animations
    this.tweens.add({ targets: innerGlow, alpha: 0.2, scaleX: 0.8, scaleY: 0.8, duration: 1000, yoyo: true, repeat: -1, ease: 'Sine.easeInOut' });
    this.tweens.add({ targets: outerRing, alpha: 0.5, duration: 1400, yoyo: true, repeat: -1, ease: 'Sine.easeInOut' });
    this.tweens.add({ targets: core, alpha: 0.3, scaleX: 0.6, scaleY: 0.6, duration: 800, yoyo: true, repeat: -1, ease: 'Sine.easeInOut' });

    // Label
    const label = this.add.text(0, -46 * DPR, DungeonSystem.getDungeonPortalLabel(), {
      fontSize: fs(11),
      color: '#FF6633',
      fontFamily: '"Noto Sans SC", sans-serif',
      stroke: '#000000',
      strokeThickness: Math.round(2 * DPR),
    }).setOrigin(0.5);
    container.add(label);

    container.setSize(Math.round(50 * DPR), Math.round(60 * DPR));
    container.setInteractive();

    this.dungeonPortalSprite = container;
  }

  /** Find dungeon portal at a tile position. */
  private findDungeonPortalAt(col: number, row: number): boolean {
    if (!this.dungeonPortalSprite) return false;
    return Math.abs(col - ZoneScene.DUNGEON_PORTAL_COL) < 2 && Math.abs(row - ZoneScene.DUNGEON_PORTAL_ROW) < 2;
  }

  /** Enter the random dungeon from Abyss Rift. */
  private enterDungeon(): void {
    if (this.isTransitioning) return;
    this.isTransitioning = true;
    this.autoSave();

    const run = DungeonSystem.createRun(this.difficulty);
    const floorConfig = DungeonSystem.getFloorConfig(run, 1);

    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: `进入深渊迷宫... (共${run.totalFloors}层)`,
      type: 'system',
    });
    EventBus.emit(GameEvents.DUNGEON_ENTER, { totalFloors: run.totalFloors });

    const doRestart = () => {
      this.scene.restart({
        classId: this.player.classData.id,
        mapId: `dungeon_floor_1`,
        dungeonRun: run,
        dungeonFloor: floorConfig,
        miniBossDialogueSeen: [...this.miniBossDialogueSeen],
        loreCollected: [...this.loreCollected],
        discoveredHiddenAreas: [...this.discoveredHiddenAreas],
        playerStats: {
          level: this.player.level,
          exp: this.player.exp,
          gold: this.player.gold,
          hp: this.player.hp,
          mana: this.player.mana,
          stats: { ...this.player.stats },
          freeStatPoints: this.player.freeStatPoints,
          freeSkillPoints: this.player.freeSkillPoints,
          skillLevels: Object.fromEntries(this.player.skillLevels),
          buffs: [...this.player.buffs],
          autoCombat: this.player.autoCombat,
          autoLootMode: this.player.autoLootMode,
        },
      });
    };

    if (this.vfx) {
      this.vfx.zoneTransition(doRestart);
    } else {
      doRestart();
    }
  }

  /** Advance to the next dungeon floor. */
  private advanceDungeonFloor(): void {
    if (!this.dungeonRunState || !this.dungeonFloorConfig || this.isTransitioning) return;
    this.isTransitioning = true;

    const nextFloor = this.dungeonRunState.currentFloor + 1;
    const nextRun = { ...this.dungeonRunState, currentFloor: nextFloor };
    const nextConfig = DungeonSystem.getFloorConfig(nextRun, nextFloor);

    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: `进入第${nextFloor}层...`,
      type: 'system',
    });
    EventBus.emit(GameEvents.DUNGEON_FLOOR_CHANGE, { floor: nextFloor, totalFloors: nextRun.totalFloors });

    const doRestart = () => {
      this.scene.restart({
        classId: this.player.classData.id,
        mapId: `dungeon_floor_${nextFloor}`,
        dungeonRun: nextRun,
        dungeonFloor: nextConfig,
        miniBossDialogueSeen: [...this.miniBossDialogueSeen],
        loreCollected: [...this.loreCollected],
        discoveredHiddenAreas: [...this.discoveredHiddenAreas],
        playerStats: {
          level: this.player.level,
          exp: this.player.exp,
          gold: this.player.gold,
          hp: this.player.hp,
          mana: this.player.mana,
          stats: { ...this.player.stats },
          freeStatPoints: this.player.freeStatPoints,
          freeSkillPoints: this.player.freeSkillPoints,
          skillLevels: Object.fromEntries(this.player.skillLevels),
          buffs: [...this.player.buffs],
          autoCombat: this.player.autoCombat,
          autoLootMode: this.player.autoLootMode,
        },
      });
    };

    if (this.vfx) {
      this.vfx.zoneTransition(doRestart);
    } else {
      doRestart();
    }
  }

  /** Exit the random dungeon and return to Abyss Rift. */
  private exitDungeon(): void {
    if (this.isTransitioning) return;
    this.isTransitioning = true;

    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: '离开深渊迷宫，返回深渊裂谷...',
      type: 'system',
    });
    EventBus.emit(GameEvents.DUNGEON_EXIT, {});

    this.dungeonRunState = null;
    this.dungeonFloorConfig = null;
    this.isInDungeon = false;

    const doRestart = () => {
      this.scene.restart({
        classId: this.player.classData.id,
        mapId: 'abyss_rift',
        targetCol: ZoneScene.ABYSS_ENTRANCE_COL,
        targetRow: ZoneScene.ABYSS_ENTRANCE_ROW,
        miniBossDialogueSeen: [...this.miniBossDialogueSeen],
        loreCollected: [...this.loreCollected],
        discoveredHiddenAreas: [...this.discoveredHiddenAreas],
        playerStats: {
          level: this.player.level,
          exp: this.player.exp,
          gold: this.player.gold,
          hp: this.player.hp,
          mana: this.player.mana,
          stats: { ...this.player.stats },
          freeStatPoints: this.player.freeStatPoints,
          freeSkillPoints: this.player.freeSkillPoints,
          skillLevels: Object.fromEntries(this.player.skillLevels),
          buffs: [...this.player.buffs],
          autoCombat: this.player.autoCombat,
          autoLootMode: this.player.autoLootMode,
        },
      });
    };

    if (this.vfx) {
      this.vfx.zoneTransition(doRestart);
    } else {
      doRestart();
    }
  }

  // ─── Zone Content Wiring: Sub-Dungeon Entrances ──────────────────────

  /** Spawn sub-dungeon entrance portal sprites. */
  private spawnSubDungeonEntrances(): void {
    if (!this.mapData.subDungeonEntrances || this.isInSubDungeon) return;
    for (const entrance of this.mapData.subDungeonEntrances) {
      const { x: worldX, y: worldY } = cartToIso(entrance.col, entrance.row);
      const container = this.add.container(worldX, worldY);
      container.setDepth(worldY + 100);

      // Portal base (dark ellipse)
      const portalBase = this.add.ellipse(0, 0, Math.round(32 * DPR), Math.round(16 * DPR), 0x220044, 0.7);
      container.add(portalBase);

      // Portal ring (purple glowing ring)
      const portalRing = this.add.ellipse(0, -16 * DPR, Math.round(28 * DPR), Math.round(36 * DPR));
      portalRing.setStrokeStyle(Math.round(3 * DPR), 0x9933FF);
      portalRing.setFillStyle(0x6600CC, 0.3);
      container.add(portalRing);

      // Inner glow
      const innerGlow = this.add.ellipse(0, -16 * DPR, Math.round(18 * DPR), Math.round(24 * DPR), 0xCC66FF, 0.4);
      container.add(innerGlow);

      // Pulsing animation
      this.tweens.add({ targets: innerGlow, alpha: 0.15, scaleX: 0.8, scaleY: 0.8, duration: 1200, yoyo: true, repeat: -1, ease: 'Sine.easeInOut' });
      this.tweens.add({ targets: portalRing, alpha: 0.6, duration: 1500, yoyo: true, repeat: -1, ease: 'Sine.easeInOut' });

      // Label
      const label = this.add.text(0, -40 * DPR, entrance.name, {
        fontSize: fs(10),
        color: '#CC88FF',
        fontFamily: '"Noto Sans SC", sans-serif',
        stroke: '#000000',
        strokeThickness: Math.round(2 * DPR),
      }).setOrigin(0.5);
      container.add(label);

      container.setSize(Math.round(40 * DPR), Math.round(50 * DPR));
      container.setInteractive();

      this.subDungeonEntranceSprites.push({ sprite: container, entrance, col: entrance.col, row: entrance.row });
    }
  }

  /** Find a sub-dungeon entrance near a tile position. */
  private findSubDungeonEntranceAt(col: number, row: number): SubDungeonEntrance | null {
    for (const se of this.subDungeonEntranceSprites) {
      if (Math.abs(se.col - col) < 1.5 && Math.abs(se.row - row) < 1.5) return se.entrance;
    }
    return null;
  }

  /** Check proximity to sub-dungeon entrances for auto-entry hint. */
  private checkSubDungeonEntranceProximity(): void {
    // No auto-entry — entrance is click-only. This is a placeholder for proximity visual cues.
    // When player is near, we could show a tooltip, but the portal glow + label are already visible.
  }

  /** Enter a sub-dungeon from the current zone. */
  private enterSubDungeon(entrance: SubDungeonEntrance): void {
    const subDungeonData = AllSubDungeons[entrance.targetSubDungeon];
    if (!subDungeonData) {
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: '此入口暂时无法进入', type: 'system' });
      return;
    }
    if (this.isTransitioning) return;
    this.isTransitioning = true;
    this.autoSave();

    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: `进入${subDungeonData.name}...`,
      type: 'system',
    });
    EventBus.emit(GameEvents.SUBDUNGEON_ENTER, { dungeonId: subDungeonData.id, name: subDungeonData.name });

    const doRestart = () => {
      this.scene.restart({
        classId: this.player.classData.id,
        mapId: subDungeonData.id,
        subDungeon: subDungeonData,
        parentZoneInfo: {
          mapId: this.currentMapId,
          returnCol: entrance.col,
          returnRow: entrance.row,
        },
        miniBossDialogueSeen: [...this.miniBossDialogueSeen],
        loreCollected: [...this.loreCollected],
        discoveredHiddenAreas: [...this.discoveredHiddenAreas],
        playerStats: {
          level: this.player.level,
          exp: this.player.exp,
          gold: this.player.gold,
          hp: this.player.hp,
          mana: this.player.mana,
          stats: { ...this.player.stats },
          freeStatPoints: this.player.freeStatPoints,
          freeSkillPoints: this.player.freeSkillPoints,
          skillLevels: Object.fromEntries(this.player.skillLevels),
          buffs: [...this.player.buffs],
          autoCombat: this.player.autoCombat,
          autoLootMode: this.player.autoLootMode,
        },
      });
    };

    if (this.vfx) {
      this.vfx.zoneTransition(doRestart);
    } else {
      doRestart();
    }
  }

  /** Exit the current sub-dungeon and return to the parent zone. */
  private exitSubDungeon(): void {
    if (!this.parentZoneInfo || this.isTransitioning) return;
    this.isTransitioning = true;

    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: '离开副本，返回主地图...',
      type: 'system',
    });
    EventBus.emit(GameEvents.SUBDUNGEON_EXIT, { mapId: this.parentZoneInfo.mapId });

    const parentInfo = this.parentZoneInfo;
    const doRestart = () => {
      this.scene.restart({
        classId: this.player.classData.id,
        mapId: parentInfo.mapId,
        targetCol: parentInfo.returnCol,
        targetRow: parentInfo.returnRow,
        miniBossDialogueSeen: [...this.miniBossDialogueSeen],
        loreCollected: [...this.loreCollected],
        discoveredHiddenAreas: [...this.discoveredHiddenAreas],
        playerStats: {
          level: this.player.level,
          exp: this.player.exp,
          gold: this.player.gold,
          hp: this.player.hp,
          mana: this.player.mana,
          stats: { ...this.player.stats },
          freeStatPoints: this.player.freeStatPoints,
          freeSkillPoints: this.player.freeSkillPoints,
          skillLevels: Object.fromEntries(this.player.skillLevels),
          buffs: [...this.player.buffs],
          autoCombat: this.player.autoCombat,
          autoLootMode: this.player.autoLootMode,
        },
      });
    };

    if (this.vfx) {
      this.vfx.zoneTransition(doRestart);
    } else {
      doRestart();
    }
  }

  /** Generate a MapData from a SubDungeonMapData definition. */
  private generateSubDungeonMap(subDungeon: SubDungeonMapData): MapData {
    const { cols, rows, seed } = subDungeon;
    // Generate simple tile grid with walls on borders, floor inside
    const tiles: number[][] = [];
    const collisions: boolean[][] = [];
    for (let r = 0; r < rows; r++) {
      const tileRow: number[] = [];
      const collRow: boolean[] = [];
      for (let c = 0; c < cols; c++) {
        if (r === 0 || r === rows - 1 || c === 0 || c === cols - 1) {
          tileRow.push(4); // wall
          collRow.push(false);
        } else {
          // Use seed-based pseudo-random for variation
          const hash = ((c * 374761393 + r * 668265263 + seed) >>> 0) % 100;
          if (hash < 10) {
            tileRow.push(2); // stone
          } else if (hash < 15) {
            tileRow.push(1); // dirt
          } else {
            tileRow.push(2); // mostly stone for dungeons
          }
          collRow.push(true);
        }
      }
      tiles.push(tileRow);
      collisions.push(collRow);
    }

    // Add some random walls for interior structure
    const rng = (x: number, y: number) => ((x * 374761393 + y * 668265263 + seed * 7) >>> 0) % 100;
    for (let r = 3; r < rows - 3; r++) {
      for (let c = 3; c < cols - 3; c++) {
        if (rng(c, r) < 8) {
          tiles[r][c] = 4;
          collisions[r][c] = false;
        }
      }
    }

    // Ensure playerStart and exit are walkable
    const ps = subDungeon.playerStart;
    tiles[ps.row][ps.col] = 2;
    collisions[ps.row][ps.col] = true;
    const ex = subDungeon.exit;
    tiles[ex.row][ex.col] = 2;
    collisions[ex.row][ex.col] = true;

    // Ensure spawn positions and mini-boss positions are walkable
    for (const spawn of subDungeon.spawns) {
      for (let dr = -2; dr <= 2; dr++) {
        for (let dc = -2; dc <= 2; dc++) {
          const sr = spawn.row + dr;
          const sc = spawn.col + dc;
          if (sr > 0 && sr < rows - 1 && sc > 0 && sc < cols - 1) {
            if (tiles[sr][sc] === 4) {
              tiles[sr][sc] = 2;
              collisions[sr][sc] = true;
            }
          }
        }
      }
    }
    const mb = subDungeon.miniBoss;
    for (let dr = -2; dr <= 2; dr++) {
      for (let dc = -2; dc <= 2; dc++) {
        const sr = mb.row + dr;
        const sc = mb.col + dc;
        if (sr > 0 && sr < rows - 1 && sc > 0 && sc < cols - 1) {
          if (tiles[sr][sc] === 4) {
            tiles[sr][sc] = 2;
            collisions[sr][sc] = true;
          }
        }
      }
    }

    // Create MapData structure
    const mapData: MapData = {
      id: subDungeon.id,
      name: subDungeon.name,
      cols,
      rows,
      tiles,
      collisions,
      spawns: subDungeon.spawns.map(s => ({ ...s })),
      camps: [], // No camps in sub-dungeons
      playerStart: { ...subDungeon.playerStart },
      exits: [{
        col: subDungeon.exit.col,
        row: subDungeon.exit.row,
        targetMap: subDungeon.parentZone,
        targetCol: subDungeon.exit.returnCol,
        targetRow: subDungeon.exit.returnRow,
      }],
      levelRange: [...subDungeon.levelRange] as [number, number],
      bgColor: subDungeon.bgColor,
      theme: subDungeon.theme,
      seed: subDungeon.seed,
    };

    return mapData;
  }

  // ─── Zone Content Wiring: Story Decorations ──────────────────────────

  /** Spawn story decoration sprites at their map positions. */
  private spawnStoryDecorations(): void {
    if (!this.mapData.storyDecorations) return;
    for (const decoration of this.mapData.storyDecorations) {
      const { x: worldX, y: worldY } = cartToIso(decoration.col, decoration.row);
      const container = this.add.container(worldX, worldY);
      container.setDepth(worldY + 50);

      // Try to use decoration sprite by type
      const texKey = `decor_${decoration.spriteType}`;
      SpriteGenerator.ensureDecoration(this, decoration.spriteType);
      if (this.textures.exists(texKey)) {
        const sprite = this.add.image(0, -8, texKey).setScale(1 / TEXTURE_SCALE);
        container.add(sprite);
      } else {
        // Fallback: colored rectangle
        const color = this.getStoryDecorationColor(decoration.spriteType);
        const rect = this.add.rectangle(0, -10, Math.round(18 * DPR), Math.round(22 * DPR), color, 0.8);
        rect.setStrokeStyle(Math.round(1 * DPR), 0x444444);
        container.add(rect);
      }

      // Name label
      const label = this.add.text(0, -28 * DPR, decoration.name, {
        fontSize: fs(8),
        color: '#CCCCAA',
        fontFamily: '"Noto Sans SC", sans-serif',
        stroke: '#000000',
        strokeThickness: Math.round(2 * DPR),
      }).setOrigin(0.5).setAlpha(0);
      container.add(label);

      // Interaction indicator (small sparkle)
      const sparkle = this.add.ellipse(8 * DPR, -20 * DPR, Math.round(4 * DPR), Math.round(4 * DPR), 0xFFFFCC, 0.6);
      container.add(sparkle);
      this.tweens.add({ targets: sparkle, alpha: 0.2, duration: 1000, yoyo: true, repeat: -1, ease: 'Sine.easeInOut' });

      this.storyDecorationSprites.push({ sprite: container, decoration, col: decoration.col, row: decoration.row });
    }
  }

  /** Get a fallback color for story decoration sprites based on type. */
  private getStoryDecorationColor(spriteType: string): number {
    switch (spriteType) {
      case 'ruins': return 0x888877;
      case 'skeletal_remains': return 0xCCBBAA;
      case 'ancient_statue': return 0xAABBCC;
      case 'broken_altar': return 0x998877;
      case 'war_banner': return 0xBB4444;
      case 'charred_tree': return 0x554433;
      case 'collapsed_pillar': return 0xBBBBAA;
      case 'ritual_circle': return 0x7744AA;
      case 'frozen_corpse': return 0x88BBDD;
      case 'sand_buried_structure': return 0xCCBB88;
      default: return 0x999999;
    }
  }

  /** Check proximity to story decorations — show/hide tooltip. */
  private checkStoryDecorationProximity(): void {
    let closestDecor: { sprite: Phaser.GameObjects.Container; decoration: StoryDecoration; col: number; row: number } | null = null;
    let closestDist = Infinity;

    for (const sd of this.storyDecorationSprites) {
      const dist = euclideanDistance(this.player.tileCol, this.player.tileRow, sd.col, sd.row);
      if (dist <= 3 && dist < closestDist) {
        closestDist = dist;
        closestDecor = sd;
      }
    }

    if (closestDecor) {
      // Show name label
      const label = closestDecor.sprite.getAt(1) as Phaser.GameObjects.Text;
      if (label && label.alpha < 1) label.setAlpha(1);

      // Show tooltip if close enough
      if (closestDist <= 2) {
        this.showStoryDecorationTooltip(closestDecor.decoration, closestDecor.sprite.x, closestDecor.sprite.y);
      } else {
        this.hideStoryDecorationTooltip();
      }
    } else {
      this.hideStoryDecorationTooltip();
      // Hide all name labels
      for (const sd of this.storyDecorationSprites) {
        const label = sd.sprite.getAt(1) as Phaser.GameObjects.Text;
        if (label && label.alpha > 0) label.setAlpha(0);
      }
    }
  }

  /** Show a tooltip with the story decoration's description text. */
  private showStoryDecorationTooltip(decoration: StoryDecoration, worldX: number, worldY: number): void {
    // Skip if already showing tooltip for same decoration
    if (this.storyDecorationTooltip && (this.storyDecorationTooltip.getData('decorId') === decoration.id)) return;
    this.hideStoryDecorationTooltip();

    const container = this.add.container(worldX, worldY - 50 * DPR);
    container.setDepth(ZONE_FLOATING_TEXT_DEPTH);
    container.setData('decorId', decoration.id);

    const tooltipWidth = Math.round(280 * DPR);
    const padding = Math.round(8 * DPR);

    // Title
    const title = this.add.text(0, 0, decoration.name, {
      fontSize: fs(11),
      color: '#FFD700',
      fontFamily: '"Noto Sans SC", sans-serif',
      fontStyle: 'bold',
      wordWrap: { width: tooltipWidth - padding * 2, useAdvancedWrap: true },
    }).setOrigin(0.5, 0);
    container.add(title);

    // Description
    const desc = this.add.text(0, title.height + Math.round(4 * DPR), decoration.description, {
      fontSize: fs(9),
      color: '#DDDDCC',
      fontFamily: '"Noto Sans SC", sans-serif',
      wordWrap: { width: tooltipWidth - padding * 2, useAdvancedWrap: true },
      lineSpacing: Math.round(2 * DPR),
    }).setOrigin(0.5, 0);
    container.add(desc);

    // Background
    const totalHeight = title.height + desc.height + Math.round(8 * DPR) + padding * 2;
    const bg = this.add.rectangle(0, -padding, tooltipWidth, totalHeight, 0x1a1a2e, 0.9);
    bg.setStrokeStyle(Math.round(1 * DPR), 0x555555);
    bg.setOrigin(0.5, 0);
    container.addAt(bg, 0);

    // Reposition text relative to background
    title.setY(-padding + Math.round(4 * DPR));
    desc.setY(title.y + title.height + Math.round(4 * DPR));

    this.storyDecorationTooltip = container;

    // Emit interaction event
    EventBus.emit(GameEvents.STORY_DECORATION_INTERACT, { decoration });
  }

  /** Hide the story decoration tooltip. */
  private hideStoryDecorationTooltip(): void {
    if (this.storyDecorationTooltip) {
      this.storyDecorationTooltip.destroy();
      this.storyDecorationTooltip = null;
    }
  }

  /** Get discovered hidden area IDs — for save data. */
  getDiscoveredHiddenAreas(): Set<string> {
    return this.discoveredHiddenAreas;
  }

  private respawnMonster(dead: Monster): void {
    const idx = this.monsters.indexOf(dead);
    if (idx === -1) return;
    const safeRadius = this.mapData.safeZoneRadius ?? 9;
    // Try random offsets, fall back to spawn point
    let c = dead.spawnCol;
    let r = dead.spawnRow;
    for (let attempt = 0; attempt < 8; attempt++) {
      const tc = dead.spawnCol + randomInt(-2, 2);
      const tr = dead.spawnRow + randomInt(-2, 2);
      if (tc >= 0 && tc < this.mapData.cols && tr >= 0 && tr < this.mapData.rows && this.mapData.collisions[tr][tc]) {
        let inSafeZone = false;
        for (const camp of this.campPositions) {
          if (euclideanDistance(tc, tr, camp.col, camp.row) < safeRadius) {
            inSafeZone = true;
            break;
          }
        }
        if (inSafeZone) continue;
        c = tc;
        r = tr;
        break;
      }
    }
    // Use originalDefinition to avoid compounding affix stat inflation
    this.monsters[idx] = new Monster(this, dead.originalDefinition, c, r);
    // Re-apply elite affixes on respawn
    if (dead.originalDefinition.elite) {
      const affixes = this.eliteAffixSystem.rollAffixes(this.currentMapId, true);
      if (affixes.length > 0) {
        this.monsters[idx].applyEliteAffixes(affixes, this.eliteAffixSystem);
      }
    }
  }

  private findNearestAliveMonster(): Monster | null {
    let best: Monster | null = null, bestDist = Infinity;
    for (const m of this.monsters) {
      if (!m.isAlive()) continue;
      const d = euclideanDistance(this.player.tileCol, this.player.tileRow, m.tileCol, m.tileRow);
      if (d < bestDist) { bestDist = d; best = m; }
    }
    return best;
  }

  private findNearestAggroMonster(): Monster | null {
    let best: Monster | null = null, bestDist = Infinity;
    for (const m of this.monsters) {
      if (!m.isAlive() || !m.isAggro()) continue;
      const d = euclideanDistance(this.player.tileCol, this.player.tileRow, m.tileCol, m.tileRow);
      if (d < bestDist) { bestDist = d; best = m; }
    }
    return best;
  }

  private findMonsterAt(col: number, row: number): Monster | null {
    for (const m of this.monsters) {
      if (!m.isAlive()) continue;
      if (Math.abs(m.tileCol - col) < 1.5 && Math.abs(m.tileRow - row) < 1.5) return m;
    }
    return null;
  }

  private findNPCAt(col: number, row: number): NPC | null {
    let best: NPC | null = null;
    let bestDist = Infinity;
    for (const npc of this.npcs) {
      const dist = Math.sqrt((npc.tileCol - col) ** 2 + (npc.tileRow - row) ** 2);
      if (dist < 1.8 && dist < bestDist) {
        bestDist = dist;
        best = npc;
      }
    }
    return best;
  }

  private findLootAt(col: number, row: number): { sprite: Phaser.GameObjects.Container; item: ItemInstance; col: number; row: number } | null {
    for (const l of this.lootDrops) {
      if (Math.abs(l.col - col) < 1.5 && Math.abs(l.row - row) < 1.5) return l;
    }
    return null;
  }

  private findExitAt(col: number, row: number): { targetMap: string; targetCol: number; targetRow: number } | null {
    for (const e of this.mapData.exits) {
      if (Math.abs(e.col - col) < 1.5 && Math.abs(e.row - row) < 1.5) return e;
    }
    return null;
  }

  private getQualityColor(quality: string): number {
    switch (quality) {
      case 'magic': return 0x3498db;
      case 'rare': return 0xf1c40f;
      case 'legendary': return 0xe67e22;
      case 'set': return 0x2ecc71;
      default: return 0xcccccc;
    }
  }

  private showDamageText(x: number, y: number, damage: number, isCrit: boolean, isDodged = false, isPlayer = false, damageType?: string): void {
    let text: string, color: string, size = fs(20);
    const elementColors: Record<string, string> = {
      fire: '#ff6600', ice: '#66ccff', lightning: '#a8e6ff',
      poison: '#66ff66', arcane: '#cc66ff',
    };
    if (isDodged) { text = 'MISS'; color = '#7f8c8d'; size = fs(14); }
    else if (isPlayer) { text = `-${damage}`; color = isCrit ? '#ff4444' : '#e74c3c'; if (isCrit) size = fs(28); }
    else {
      text = `${damage}`;
      color = isCrit ? '#ffd700' : (damageType && elementColors[damageType]) || '#ffffff';
      if (isCrit) size = fs(32);
    }
    const t = this.add.text(x + randomInt(-15, 15), y - 30, text, {
      fontSize: size, color, fontFamily: '"Cinzel", serif', fontStyle: isCrit ? 'bold' : 'normal',
      stroke: '#000000', strokeThickness: Math.round((isCrit ? 4 : 3) * DPR),
    }).setOrigin(0.5).setDepth(ZONE_FLOATING_TEXT_DEPTH);
    if (isCrit) {
      t.setScale(1.5);
      this.tweens.add({ targets: t, scale: 1, duration: 200, ease: 'Back.easeOut' });
      this.tweens.add({ targets: t, y: t.y - 70, alpha: 0, duration: 1500, ease: 'Power2', onComplete: () => t.destroy() });
    } else {
      this.tweens.add({ targets: t, y: t.y - 50, alpha: 0, duration: 1200, ease: 'Power2', onComplete: () => t.destroy() });
    }
  }

  /** Convert a desired screen-fraction position to scrollFactor(0) object position, accounting for camera zoom. */
  private screenPos(fracX: number, fracY: number): { x: number; y: number } {
    const cam = this.cameras.main;
    const ox = cam.width * cam.originX;
    const oy = cam.height * cam.originY;
    return {
      x: ox + (fracX * cam.width - ox) / cam.zoom,
      y: oy + (fracY * cam.height - oy) / cam.zoom,
    };
  }

  private useTownPortal(): void {
    if (this.player.hp <= 0 || this.isPortaling || this.isTransitioning) return;

    // Determine portal destination based on context
    let destCol: number;
    let destRow: number;
    let arrivalMsg: string;

    if (this.isInSubDungeon || this.isInDungeon) {
      // In a sub-dungeon or random dungeon: teleport to the exit tile
      const exit = this.mapData.exits[0];
      if (!exit) return;
      destCol = exit.col;
      destRow = exit.row;
      arrivalMsg = this.isInSubDungeon ? '已传送至副本入口。' : '已传送至楼层出口。';

      // Already near exit?
      if (euclideanDistance(this.player.tileCol, this.player.tileRow, destCol, destRow) < 3) {
        EventBus.emit(GameEvents.LOG_MESSAGE, { text: '你已在出口附近。', type: 'system' });
        return;
      }
    } else {
      const camp = this.campPositions[0];
      if (!camp) return;
      destCol = camp.col;
      destRow = camp.row;
      arrivalMsg = '已传送回营地。';

      // Already near camp?
      const safeRadius = this.mapData.safeZoneRadius ?? 9;
      if (euclideanDistance(this.player.tileCol, this.player.tileRow, destCol, destRow) < safeRadius) {
        EventBus.emit(GameEvents.LOG_MESSAGE, { text: '你已在营地中，无法使用传送门。', type: 'system' });
        return;
      }
    }

    this.isPortaling = true;
    this.player.path = [];
    this.player.isMoving = false;
    this.player.attackTarget = null;

    EventBus.emit(GameEvents.LOG_MESSAGE, { text: '正在开启传送门...', type: 'system' });

    // Portal VFX — expanding ring at player's feet
    const px = this.player.sprite.x;
    const py = this.player.sprite.y + 8;
    const ring = this.add.circle(px, py, 4, 0x4488ff, 0).setDepth(900);
    const ring2 = this.add.circle(px, py, 4, 0x66aaff, 0).setDepth(900);
    const glow = this.add.circle(px, py, 2, 0x2266cc, 0).setDepth(899);
    const portalFx = [ring, ring2, glow];

    const t1 = this.tweens.add({
      targets: ring, radius: 28, alpha: 0.7, duration: 1200, ease: 'Sine.easeOut',
    });
    const t2 = this.tweens.add({
      targets: ring2, radius: 20, alpha: 0.5, duration: 1200, ease: 'Sine.easeOut', delay: 200,
    });
    const t3 = this.tweens.add({
      targets: glow, radius: 30, alpha: 0.3, duration: 1200, ease: 'Sine.easeOut',
    });
    const portalTweens = [t1, t2, t3];

    // Player flicker during cast
    const flickerTween = this.tweens.add({
      targets: this.player.sprite, alpha: 0.5, duration: 200, yoyo: true, repeat: 3,
    });

    this.time.delayedCall(1500, () => {
      // Stop all portal tweens before destroying their targets
      for (const tw of portalTweens) tw.stop();
      flickerTween.stop();

      // Flash & teleport
      if (this.vfx) this.vfx.cameraFlash(200, 0.5, 0x4488ff);
      audioManager.playSFX('zone_transition');

      for (const fx of portalFx) if (fx.active) fx.destroy();

      this.player.moveTo(destCol, destRow);
      this.player.sprite.setAlpha(1);
      this.isPortaling = false;

      EventBus.emit(GameEvents.LOG_MESSAGE, { text: arrivalMsg, type: 'system' });
    });
  }

  private showLevelUpBanner(level: number): void {
    const p = this.screenPos(0.5, 0.28);
    const text = this.add.text(p.x, p.y, '升级!', {
      fontSize: fs(32), color: '#ffd700', fontFamily: '"Cinzel", serif',
      fontStyle: 'bold', stroke: '#000000', strokeThickness: Math.round(5 * DPR),
    }).setOrigin(0.5).setScrollFactor(0).setDepth(ZONE_SCREEN_UI_DEPTH).setAlpha(0).setScale(0.5);

    const lvlText = this.add.text(p.x, p.y + 38 * DPR / this.cameras.main.zoom, `等级 ${level}`, {
      fontSize: fs(20), color: '#ffcc00', fontFamily: '"Cinzel", serif',
      stroke: '#000000', strokeThickness: Math.round(3 * DPR),
    }).setOrigin(0.5).setScrollFactor(0).setDepth(ZONE_SCREEN_UI_DEPTH).setAlpha(0);

    this.tweens.add({
      targets: text, alpha: 1, scale: 1, duration: 400, ease: 'Back.easeOut',
    });
    this.tweens.add({
      targets: lvlText, alpha: 1, duration: 500, ease: 'Power2',
    });
    this.time.delayedCall(2500, () => {
      this.tweens.add({
        targets: [text, lvlText], alpha: 0, y: '-=20', duration: 600,
        ease: 'Power2', onComplete: () => { text.destroy(); lvlText.destroy(); },
      });
    });
  }

  private showQuestCompleteBanner(questName: string): void {
    const p = this.screenPos(0.5, 0.22);
    const z = this.cameras.main.zoom;
    const label = this.add.text(p.x, p.y, '任务完成!', {
      fontSize: fs(20), color: '#f1c40f', fontFamily: '"Cinzel", serif',
      fontStyle: 'bold', stroke: '#000000', strokeThickness: Math.round(4 * DPR),
    }).setOrigin(0.5).setScrollFactor(0).setDepth(ZONE_SCREEN_UI_DEPTH).setAlpha(0);

    const name = this.add.text(p.x, p.y + 28 * DPR / z, questName, {
      fontSize: fs(16), color: '#e0d8cc', fontFamily: '"Cinzel", serif',
      stroke: '#000000', strokeThickness: Math.round(3 * DPR),
    }).setOrigin(0.5).setScrollFactor(0).setDepth(ZONE_SCREEN_UI_DEPTH).setAlpha(0);

    this.tweens.add({ targets: [label, name], alpha: 1, duration: 500, ease: 'Power2' });
    this.time.delayedCall(2500, () => {
      this.tweens.add({
        targets: [label, name], alpha: 0, duration: 600,
        onComplete: () => { label.destroy(); name.destroy(); },
      });
    });
  }

  private showZoneBanner(): void {
    const p = this.screenPos(0.5, 0.32);
    const z = this.cameras.main.zoom;
    const banner = this.add.text(p.x, p.y, this.mapData.name, {
      fontSize: fs(28), color: '#c0934a', fontFamily: '"Cinzel", serif',
      fontStyle: 'bold', stroke: '#000000', strokeThickness: Math.round(5 * DPR),
    }).setOrigin(0.5).setScrollFactor(0).setDepth(ZONE_SCREEN_UI_DEPTH).setAlpha(0);

    const subtitle = this.add.text(p.x, p.y + 32 * DPR / z,
      `Lv.${this.mapData.levelRange[0]}-${this.mapData.levelRange[1]}`, {
      fontSize: fs(16), color: '#8a7a5a', fontFamily: '"Cinzel", serif',
      stroke: '#000000', strokeThickness: Math.round(3 * DPR),
    }).setOrigin(0.5).setScrollFactor(0).setDepth(ZONE_SCREEN_UI_DEPTH).setAlpha(0);

    // Decorative lines
    const lineW = 120 * DPR;
    const lineY = p.y + 20 * DPR / z;
    const lineL = this.add.rectangle(p.x - 80 * DPR / z, lineY, lineW, 1, 0xc0934a, 0).setScrollFactor(0).setDepth(ZONE_SCREEN_UI_DEPTH);
    const lineR = this.add.rectangle(p.x + 80 * DPR / z, lineY, lineW, 1, 0xc0934a, 0).setScrollFactor(0).setDepth(ZONE_SCREEN_UI_DEPTH);

    this.tweens.add({
      targets: [banner, subtitle, lineL, lineR],
      alpha: { from: 0, to: 1 },
      duration: 800,
      ease: 'Power2',
    });

    this.time.delayedCall(3000, () => {
      this.tweens.add({
        targets: [banner, subtitle, lineL, lineR],
        alpha: 0,
        duration: 800,
        ease: 'Power2',
        onComplete: () => {
          banner.destroy(); subtitle.destroy();
          lineL.destroy(); lineR.destroy();
        },
      });
    });
  }

  // ---------------------------------------------------------------------------
  // Elite Affix Behavior Updates
  // ---------------------------------------------------------------------------

  /**
   * Process per-tick elite affix behaviors: teleporting blink, curse aura debuff.
   */
  private updateEliteAffixBehaviors(time: number): void {
    for (const monster of this.monsters) {
      if (!monster.isAlive() || monster.eliteAffixes.length === 0) continue;

      // ── Teleporting: periodic blink near player ──
      const teleAffix = this.eliteAffixSystem.getTeleportingAffix(monster.eliteAffixes);
      if (teleAffix && monster.isAggro()) {
        if (this.eliteAffixSystem.shouldTeleport(teleAffix, time)) {
          teleAffix.lastTeleportTime = time;
          const dist = euclideanDistance(monster.tileCol, monster.tileRow, this.player.tileCol, this.player.tileRow);
          if (dist > 2 && dist < 15) {
            // Blink to a random walkable tile near the player
            const offsetCol = this.player.tileCol + (Math.random() < 0.5 ? -1 : 1) * (1 + Math.random());
            const offsetRow = this.player.tileRow + (Math.random() < 0.5 ? -1 : 1) * (1 + Math.random());
            const tc = Math.round(Math.max(1, Math.min(this.mapData.cols - 2, offsetCol)));
            const tr = Math.round(Math.max(1, Math.min(this.mapData.rows - 2, offsetRow)));
            if (this.mapData.collisions[tr]?.[tc]) {
              // VFX: vanish at old position
              const oldPos = cartToIso(monster.tileCol, monster.tileRow);
              if (this.vfx) {
                const puff = this.add.circle(oldPos.x, oldPos.y - 16, 12, 0xaa44ff, 0.6);
                puff.setDepth(oldPos.y + 100);
                this.tweens.add({
                  targets: puff, scaleX: 2.5, scaleY: 2.5, alpha: 0, duration: 400,
                  onComplete: () => puff.destroy(),
                });
              }
              // Move monster
              monster.tileCol = tc;
              monster.tileRow = tr;
              const newPos = cartToIso(tc, tr);
              monster.sprite.setPosition(newPos.x, newPos.y);
              monster.sprite.setDepth(newPos.y + 50);
              // VFX: appear at new position
              if (this.vfx) {
                const flash = this.add.circle(newPos.x, newPos.y - 16, 12, 0xaa44ff, 0.6);
                flash.setDepth(newPos.y + 100);
                this.tweens.add({
                  targets: flash, scaleX: 2.5, scaleY: 2.5, alpha: 0, duration: 400,
                  onComplete: () => flash.destroy(),
                });
              }
            }
          }
        }
      }

      // ── Curse Aura: debuff player stats when in proximity ──
      const curseAffix = this.eliteAffixSystem.getCurseAuraAffix(monster.eliteAffixes);
      if (curseAffix) {
        const dist = euclideanDistance(monster.tileCol, monster.tileRow, this.player.tileCol, this.player.tileRow);
        if (dist <= curseAffix.definition.curseAuraRadius) {
          // Apply curse debuff as a timed buff on the player (amplifies damage taken)
          const reduction = curseAffix.definition.curseAuraReduction;
          const existingCurse = this.player.buffs.find(
            b => b.stat === 'damageAmplify' && b.tag === 'curseAura',
          );
          if (!existingCurse) {
            this.player.buffs.push({
              stat: 'damageAmplify',
              value: reduction, // e.g. 0.15 = 15% more damage taken
              duration: 2000,
              startTime: time,
              tag: 'curseAura', // tag so we can find and refresh it
            });
          } else {
            // Refresh duration
            existingCurse.startTime = time;
          }
          // Visual: purple tint on player periodically
          if (time - (curseAffix.lastCurseTickTime ?? 0) > 2000) {
            curseAffix.lastCurseTickTime = time;
            EventBus.emit(GameEvents.LOG_MESSAGE, { text: '诅咒光环：属性降低！', type: 'combat' });
          }
        }
      }
    }
  }

  // ---------------------------------------------------------------------------
  // Status Effect System integration
  // ---------------------------------------------------------------------------

  /**
   * Tick and expire status effects on all tracked entities.
   * Apply DoT damage and visual indicators.
   */
  private updateStatusEffects(time: number): void {
    if (!this.statusEffects) return;

    const tracked = this.statusEffects.getTrackedEntities();
    for (const entityId of tracked) {
      // Apply visual status tints via VFXManager (only when newly needed)
      if (this.vfx) {
        const effects = this.statusEffects.getEffectsOnEntity(entityId);
        const sprite = this.getEntitySprite(entityId);
        if (sprite) {
          let appliedSet = this.statusTintApplied.get(entityId);
          if (!appliedSet) {
            appliedSet = new Set();
            this.statusTintApplied.set(entityId, appliedSet);
          }
          for (const effect of effects) {
            const tintType = effect.type === 'burn' ? 'burn'
              : (effect.type === 'freeze' || effect.type === 'stun') ? 'freeze'
              : effect.type === 'poison' ? 'poison'
              : null;
            if (tintType && !appliedSet.has(tintType)) {
              this.vfx.applyStatusTint(sprite, tintType);
              appliedSet.add(tintType);
            }
          }
        }
      }

      // Tick DoTs
      const ticks = this.statusEffects.tick(entityId, time);
      for (const tick of ticks) {
        if (entityId === 'player') {
          // DoT damage to player
          if (this.player.hp <= 0) continue;
          this.player.hp = Math.max(0, this.player.hp - tick.damage);
          this.showDamageText(
            this.player.sprite.x, this.player.sprite.y,
            tick.damage, false, false, true,
            tick.type === 'burn' ? 'fire' : tick.type === 'poison' ? 'poison' : undefined,
          );
          EventBus.emit(GameEvents.COMBAT_DAMAGE, {
            targetId: 'player', damage: tick.damage, isDodged: false,
            isCrit: false, isPlayerTarget: true, targetMaxHP: this.player.maxHp,
          });
          if (this.player.hp <= 0) {
            this.player.die();
          }
        } else {
          // DoT damage to monster
          const monster = this.monsters.find(m => m.id === entityId && m.isAlive());
          if (monster) {
            // Bleed ignores defense (damage applied directly via takeDamage)
            monster.takeDamage(tick.damage);
            this.showDamageText(
              monster.sprite.x, monster.sprite.y,
              tick.damage, false, false, false,
              tick.type === 'burn' ? 'fire' : tick.type === 'poison' ? 'poison' : undefined,
            );
            if (!monster.isAlive()) {
              this.onMonsterKilled(monster);
            }
          }
        }
      }

      // Expire effects
      const expired = this.statusEffects.expire(entityId, time);
      if (expired.length > 0) {
        const effectNames: Record<string, string> = {
          burn: '灼烧', freeze: '冰冻', poison: '中毒',
          bleed: '流血', slow: '减速', stun: '眩晕',
        };
        for (const type of expired) {
          EventBus.emit(GameEvents.LOG_MESSAGE, {
            text: `${effectNames[type] ?? type}效果已消失`,
          });
        }
        // Clear all tints and re-apply remaining active effects' tints
        const sprite = this.getEntitySprite(entityId);
        if (sprite && (sprite as any).preFX) {
          (sprite as any).preFX.clear();
          // Reset tint tracking for this entity
          this.statusTintApplied.delete(entityId);
          // Re-apply tints for any remaining active effects
          const remainingEffects = this.statusEffects.getEffectsOnEntity(entityId);
          if (this.vfx) {
            const newAppliedSet = new Set<string>();
            for (const effect of remainingEffects) {
              const tintType = effect.type === 'burn' ? 'burn'
                : (effect.type === 'freeze' || effect.type === 'stun') ? 'freeze'
                : effect.type === 'poison' ? 'poison'
                : null;
              if (tintType && !newAppliedSet.has(tintType)) {
                this.vfx.applyStatusTint(sprite, tintType);
                newAppliedSet.add(tintType);
              }
            }
            if (newAppliedSet.size > 0) {
              this.statusTintApplied.set(entityId, newAppliedSet);
            }
          }
        }
      }
    }
  }

  /** Helper to get a Phaser sprite for an entity by ID (for VFX tints). */
  private getEntitySprite(entityId: string): Phaser.GameObjects.Sprite | Phaser.GameObjects.Image | null {
    if (entityId === 'player') {
      // Player sprite is a Container — find the first Sprite child
      const children = this.player.sprite.list;
      for (const child of children) {
        if (child instanceof Phaser.GameObjects.Sprite) return child;
      }
      return null;
    }
    const monster = this.monsters.find(m => m.id === entityId && m.isAlive());
    if (monster) {
      const children = monster.sprite.list;
      for (const child of children) {
        if (child instanceof Phaser.GameObjects.Sprite) return child;
      }
    }
    return null;
  }

  /**
   * Apply status effects from monster attacks to the player.
   * Fire-type monsters apply Burn, poison-type apply Poison, ice-type apply Freeze, etc.
   */
  private applyMonsterStatusEffect(monster: Monster, time: number): void {
    const spriteKey = monster.definition.spriteKey;
    const monsterId = monster.definition.id;
    const damage = monster.definition.damage;

    // Fire monsters (fire_elemental, phoenix, lava_golem, etc.) apply Burn
    if (spriteKey.includes('fire') || spriteKey.includes('phoenix') ||
        spriteKey.includes('lava') || monsterId.includes('fire') ||
        monsterId.includes('phoenix') || monsterId.includes('lava')) {
      // 30% chance to apply burn on hit
      if (Math.random() < 0.3) {
        const burnDamage = Math.floor(damage * 0.2); // 20% of monster damage per tick
        this.statusEffects.apply('player', 'burn', Math.max(1, burnDamage), 3000, monster.id, time);
      }
    }

    // Poison monsters (poison_spider, venom types, etc.) apply Poison
    if (spriteKey.includes('poison') || spriteKey.includes('venom') ||
        spriteKey.includes('spider') || monsterId.includes('poison') ||
        monsterId.includes('venom') || monsterId.includes('spider')) {
      if (Math.random() < 0.25) {
        const poisonDamage = Math.floor(damage * 0.15);
        this.statusEffects.apply('player', 'poison', Math.max(1, poisonDamage), 4000, monster.id, time);
      }
    }

    // Ice monsters apply Slow
    if (spriteKey.includes('ice') || spriteKey.includes('frost') ||
        monsterId.includes('ice') || monsterId.includes('frost')) {
      if (Math.random() < 0.2) {
        this.statusEffects.apply('player', 'slow', 30, 3000, monster.id, time);
      }
    }
  }

  /**
   * Apply status effects from player skills to monsters.
   * Based on skill damage type: fire → Burn, ice → Freeze (for freeze-tagged skills),
   * poison → Poison, physical with bleed tag → Bleed.
   */
  private applySkillStatusEffect(
    target: Monster,
    skill: import('../data/types').SkillDefinition,
    damage: number,
    time: number,
  ): void {
    if (!target.isAlive()) return;
    const skillId = skill.id;
    const dmgType = skill.damageType;

    // Fire skills apply Burn (50% chance, or guaranteed for heavy fire skills)
    if (dmgType === 'fire') {
      const burnChance = skillId.includes('meteor') || skillId.includes('fireball') ? 0.6 : 0.4;
      if (Math.random() < burnChance) {
        const burnDamage = Math.max(1, Math.floor(damage * 0.15));
        this.statusEffects.apply(target.id, 'burn', burnDamage, 3000, 'player', time);
      }
    }

    // Ice skills: apply Freeze if skill has stunDuration or is a 'freeze' skill, otherwise Slow
    if (dmgType === 'ice') {
      if (skill.stunDuration || skillId.includes('freeze') || skillId === 'blizzard') {
        const freezeDuration = skill.stunDuration ?? 2000;
        this.statusEffects.apply(target.id, 'freeze', 1, freezeDuration, 'player', time);
      } else {
        if (Math.random() < 0.35) {
          this.statusEffects.apply(target.id, 'slow', 40, 3000, 'player', time);
        }
      }
    }

    // Poison skills apply Poison
    if (dmgType === 'poison') {
      const poisonDamage = Math.max(1, Math.floor(damage * 0.2));
      this.statusEffects.apply(target.id, 'poison', poisonDamage, 4000, 'player', time);
    }

    // Physical skills with 'bleed' in the name apply Bleed
    if (skillId.includes('bleed') || skillId.includes('lacerate') || skillId.includes('rend')) {
      const bleedDamage = Math.max(1, Math.floor(damage * 0.25));
      this.statusEffects.apply(target.id, 'bleed', bleedDamage, 5000, 'player', time);
    }

    // War Stomp stun uses StatusEffectSystem too (in addition to existing buff-based stun)
    if (skill.stunDuration && dmgType === 'physical') {
      this.statusEffects.apply(target.id, 'stun', 1, skill.stunDuration, 'player', time);
    }
  }

  // ---------------------------------------------------------------------------
  // Mercenary System integration
  // ---------------------------------------------------------------------------

  /** Create or update the mercenary sprite in the game world. */
  spawnMercenarySprite(): void {
    this.destroyMercenarySprite();
    if (!this.mercenarySystem?.isAlive()) return;
    const merc = this.mercenarySystem.getMercenary()!;
    const def = MERCENARY_DEFS[merc.type];

    // Position near player
    merc.tileCol = this.player.tileCol + 1;
    merc.tileRow = this.player.tileRow + 1;
    const worldPos = cartToIso(merc.tileCol, merc.tileRow);

    this.mercenarySprite = this.add.container(worldPos.x, worldPos.y);
    this.mercenarySprite.setDepth(worldPos.y + 60);

    // Simple colored rectangle body with role color
    const colorMap: Record<string, number> = {
      tank: 0x2471a3, melee: 0xc0392b, ranged: 0x27ae60, healer: 0xf1c40f, mage: 0x8e44ad,
    };
    const color = colorMap[merc.type] ?? 0x888888;
    const body = this.add.rectangle(0, -20, 32, 40, color);
    body.setStrokeStyle(1.5, 0xffffff, 0.6);
    this.mercenarySprite.add(body);

    // Shadow
    const shadow = this.add.ellipse(0, 4, 30, 8, 0x000000, 0.25);
    this.mercenarySprite.add(shadow);
    this.mercenarySprite.sendToBack(shadow);

    // Friendly indicator (small green diamond above)
    const indicator = this.add.rectangle(0, -48, 6, 6, 0x27ae60);
    indicator.setAngle(45);
    this.mercenarySprite.add(indicator);

    // HP bar
    this.mercenaryHpBarBg = this.add.rectangle(0, -46, 28, 4, 0x1a1a1a);
    this.mercenaryHpBarBg.setStrokeStyle(0.5, 0x333333);
    this.mercenarySprite.add(this.mercenaryHpBarBg);

    this.mercenaryHpBar = this.add.rectangle(-14, -46, 28, 4, 0x27ae60);
    this.mercenaryHpBar.setOrigin(0, 0.5);
    this.mercenarySprite.add(this.mercenaryHpBar);

    // Name label
    this.mercenaryNameLabel = this.add.text(0, -56, `${def.name} Lv.${merc.level}`, {
      fontSize: fs(10), color: '#88cc88', fontFamily: '"Noto Sans SC", sans-serif',
      stroke: '#000000', strokeThickness: Math.round(2 * DPR),
    }).setOrigin(0.5);
    this.mercenarySprite.add(this.mercenaryNameLabel);
  }

  destroyMercenarySprite(): void {
    if (this.mercenarySprite) {
      this.mercenarySprite.destroy();
      this.mercenarySprite = null;
      this.mercenaryHpBar = null;
      this.mercenaryHpBarBg = null;
      this.mercenaryNameLabel = null;
    }
  }

  /** Update mercenary following, combat AI, and sprite position each frame. */
  private updateMercenary(time: number, delta: number): void {
    if (!this.mercenarySystem?.isAlive() || !this.mercenarySprite) return;
    const merc = this.mercenarySystem.getMercenary()!;
    const def = MERCENARY_DEFS[merc.type];

    // Mana regen
    this.mercenarySystem.regenMana(delta);

    // Check safe zone
    const safeRadius = this.mapData.safeZoneRadius ?? 9;
    let inSafeZone = false;
    for (const camp of this.campPositions) {
      if (euclideanDistance(merc.tileCol, merc.tileRow, camp.col, camp.row) < safeRadius) {
        inSafeZone = true;
        break;
      }
    }

    // Gather nearby monster info
    let nearestMonsterCol: number | null = null;
    let nearestMonsterRow: number | null = null;
    let nearestMonsterDist = Infinity;
    const monstersInRange: { col: number; row: number; dist: number }[] = [];

    for (const m of this.monsters) {
      if (!m.isAlive()) continue;
      const dist = euclideanDistance(this.player.tileCol, this.player.tileRow, m.tileCol, m.tileRow);
      if (dist < 10) {
        monstersInRange.push({ col: m.tileCol, row: m.tileRow, dist });
        if (dist < nearestMonsterDist) {
          nearestMonsterDist = dist;
          nearestMonsterCol = m.tileCol;
          nearestMonsterRow = m.tileRow;
        }
      }
    }

    const playerHpRatio = this.player.hp / this.player.maxHp;
    const action = this.mercenarySystem.getAIAction(
      time, this.player.tileCol, this.player.tileRow, playerHpRatio,
      nearestMonsterCol, nearestMonsterRow, nearestMonsterDist,
      monstersInRange, inSafeZone,
    );

    this.executeMercenaryAction(action, time);

    // Update sprite position
    const worldPos = cartToIso(merc.tileCol, merc.tileRow);
    this.mercenarySprite.setPosition(worldPos.x, worldPos.y);
    this.mercenarySprite.setDepth(worldPos.y + 60);

    // Update HP bar
    if (this.mercenaryHpBar) {
      const hpRatio = merc.hp / merc.maxHp;
      this.mercenaryHpBar.width = Math.max(0, 28 * hpRatio);
      this.mercenaryHpBar.fillColor = hpRatio > 0.5 ? 0x27ae60 : hpRatio > 0.25 ? 0xf39c12 : 0xe74c3c;
    }

    // Update name label
    if (this.mercenaryNameLabel) {
      this.mercenaryNameLabel.setText(`${def.name} Lv.${merc.level}`);
    }

    // Update buffs
    merc.buffs = merc.buffs.filter(b => time - b.startTime < b.duration);
  }

  /** Execute the mercenary AI action. */
  private executeMercenaryAction(action: MercenaryAIAction, time: number): void {
    if (!this.mercenarySystem?.isAlive()) return;
    const merc = this.mercenarySystem.getMercenary()!;

    switch (action.type) {
      case 'follow':
      case 'move_to_target':
      case 'reposition': {
        if (action.targetCol === undefined || action.targetRow === undefined) break;
        // Simple movement toward target
        const dx = action.targetCol - merc.tileCol;
        const dy = action.targetRow - merc.tileRow;
        const dist = Math.sqrt(dx * dx + dy * dy);
        if (dist > 0.3) {
          const speed = 0.06; // tiles per frame
          const nx = dx / dist;
          const ny = dy / dist;
          let newCol = merc.tileCol + nx * speed;
          let newRow = merc.tileRow + ny * speed;
          // Clamp to map bounds
          newCol = Math.max(1, Math.min(this.mapData.cols - 2, newCol));
          newRow = Math.max(1, Math.min(this.mapData.rows - 2, newRow));
          const checkCol = Math.round(newCol);
          const checkRow = Math.round(newRow);
          if (this.mapData.collisions[checkRow]?.[checkCol]) {
            merc.tileCol = newCol;
            merc.tileRow = newRow;
          }
        }
        break;
      }
      case 'attack': {
        if (action.targetCol === undefined || action.targetRow === undefined) break;
        // Find the monster at that position
        const target = this.findMonsterNear(action.targetCol, action.targetRow);
        if (target && target.isAlive()) {
          merc.lastAttackTime = time;
          const entity = this.mercenarySystem.toCombatEntity();
          if (entity) {
            const result = this.combatSystem.calculateDamage(entity, target.toCombatEntity());
            target.takeDamage(result.damage, this.mercenarySprite?.x ?? 0, this.mercenarySprite?.y ?? 0);
            this.showDamageText(target.sprite.x, target.sprite.y, result.damage, result.isCrit);
            if (!target.isAlive()) {
              this.onMonsterKilled(target);
            }
          }
        }
        break;
      }
      case 'aoe_attack': {
        if (action.targetCol === undefined || action.targetRow === undefined) break;
        const radius = action.radius ?? 3;
        merc.lastAttackTime = time;
        merc.mana = Math.max(0, merc.mana - 10);
        const entity = this.mercenarySystem.toCombatEntity();
        if (entity) {
          for (const m of this.monsters) {
            if (!m.isAlive()) continue;
            const dist = euclideanDistance(action.targetCol, action.targetRow, m.tileCol, m.tileRow);
            if (dist <= radius) {
              const result = this.combatSystem.calculateDamage(entity, m.toCombatEntity());
              const dmg = Math.floor(result.damage * 0.7); // AoE damage reduction
              m.takeDamage(dmg, this.mercenarySprite?.x ?? 0, this.mercenarySprite?.y ?? 0);
              this.showDamageText(m.sprite.x, m.sprite.y, dmg, result.isCrit);
              if (!m.isAlive()) this.onMonsterKilled(m);
            }
          }
        }
        break;
      }
      case 'heal': {
        const healAmount = this.mercenarySystem.performHeal(this.player.maxHp);
        if (healAmount > 0) {
          merc.lastHealTime = time;
          this.player.hp = Math.min(this.player.maxHp, this.player.hp + healAmount);
          EventBus.emit(GameEvents.PLAYER_HEALTH_CHANGED, { hp: this.player.hp, maxHp: this.player.maxHp });
          EventBus.emit(GameEvents.LOG_MESSAGE, {
            text: `${MERCENARY_DEFS[merc.type].name} 治疗了你! +${healAmount} HP`,
            type: 'combat',
          });
          if (this.vfx) {
            this.vfx.healBurst(this.player.sprite.x, this.player.sprite.y - 16, 8);
          }
        }
        break;
      }
      case 'idle':
      default:
        break;
    }
  }

  /** Handle monsters attacking the mercenary (tank aggro). */
  private handleMercenaryCombat(time: number): void {
    if (!this.mercenarySystem?.isAlive() || !this.mercenarySprite) return;
    const merc = this.mercenarySystem.getMercenary()!;
    const def = MERCENARY_DEFS[merc.type];

    // Only tank mercenaries absorb hits
    if (def.aiRole !== 'tank') return;

    // Monsters that are close to the mercenary may target it
    for (const monster of this.monsters) {
      if (!monster.isAlive() || monster.state !== 'attack') continue;
      if (this.statusEffects.isImmobilized(monster.id)) continue;

      const distToMerc = euclideanDistance(monster.tileCol, monster.tileRow, merc.tileCol, merc.tileRow);
      const distToPlayer = euclideanDistance(monster.tileCol, monster.tileRow, this.player.tileCol, this.player.tileRow);

      // If mercenary is closer and within monster's attack range, monster hits mercenary
      if (distToMerc < distToPlayer && distToMerc <= monster.definition.attackRange + 0.5) {
        if (time - monster.lastAttackTime >= monster.definition.attackSpeed) {
          monster.lastAttackTime = time;
          const result = this.combatSystem.calculateDamage(monster.toCombatEntity(), this.mercenarySystem.toCombatEntity()!);
          // Difficulty damage scaling is already applied at monster spawn time via DifficultySystem.scaleMonster
          const finalDmg = result.damage;
          const { died } = this.mercenarySystem.takeDamage(finalDmg);
          this.showDamageText(
            this.mercenarySprite.x, this.mercenarySprite.y - 10,
            finalDmg, result.isCrit, false, true,
          );
          if (died) {
            this.handleMercenaryDeath();
          }
        }
      }
    }
  }

  /** Handle mercenary death: animation and removal from targeting. */
  private handleMercenaryDeath(): void {
    if (!this.mercenarySprite) return;
    // Death animation: fade out and scale down
    this.tweens.add({
      targets: this.mercenarySprite,
      alpha: 0,
      scaleX: 0.5,
      scaleY: 0.5,
      duration: 600,
      ease: 'Power2',
      onComplete: () => {
        this.destroyMercenarySprite();
      },
    });
    if (this.vfx) {
      this.vfx.deathBurst(this.mercenarySprite.x, this.mercenarySprite.y - 16, 0x666666);
    }
  }

  // ---------------------------------------------------------------------------
  // ─── Escort Quest Runtime ─────────────────────────────────────────────────
  // ---------------------------------------------------------------------------

  /** Spawn escort NPC if any escort quest is active in this zone. */
  private spawnEscortNpc(): void {
    this.destroyEscortNpc();
    const activeQuests = this.questSystem.getActiveQuests();
    for (const { quest, progress } of activeQuests) {
      if (quest.type !== 'escort' || progress.status !== 'active') continue;
      if (quest.zone !== this.currentMapId) continue;
      if (!quest.escortNpc) continue;

      const en = quest.escortNpc;
      this.escortNpcTileCol = en.startCol;
      this.escortNpcTileRow = en.startRow;
      this.escortDestCol = en.destCol;
      this.escortDestRow = en.destRow;
      this.escortQuestId = quest.id;

      // HP scales with zone level
      const baseHp = quest.level * 20 + 100;
      this.escortNpcHp = baseHp;
      this.escortNpcMaxHp = baseHp;

      const worldPos = cartToIso(this.escortNpcTileCol, this.escortNpcTileRow);
      this.escortNpcSprite = this.add.container(worldPos.x, worldPos.y);
      this.escortNpcSprite.setDepth(worldPos.y + 60);

      // Body — golden NPC color
      const body = this.add.rectangle(0, -20, 28, 36, 0xe67e22);
      body.setStrokeStyle(1.5, 0xffffff, 0.6);
      this.escortNpcSprite.add(body);

      // Shadow
      const shadow = this.add.ellipse(0, 4, 26, 7, 0x000000, 0.25);
      this.escortNpcSprite.add(shadow);
      this.escortNpcSprite.sendToBack(shadow);

      // Friendly indicator (orange diamond)
      const indicator = this.add.rectangle(0, -46, 6, 6, 0xe67e22);
      indicator.setAngle(45);
      this.escortNpcSprite.add(indicator);

      // HP bar
      this.escortNpcHpBarBg = this.add.rectangle(0, -44, 28, 4, 0x1a1a1a);
      this.escortNpcHpBarBg.setStrokeStyle(0.5, 0x333333);
      this.escortNpcSprite.add(this.escortNpcHpBarBg);

      this.escortNpcHpBar = this.add.rectangle(-14, -44, 28, 4, 0x27ae60);
      this.escortNpcHpBar.setOrigin(0, 0.5);
      this.escortNpcSprite.add(this.escortNpcHpBar);

      // Name label
      this.escortNpcNameLabel = this.add.text(0, -54, en.name, {
        fontSize: fs(10), color: '#e67e22', fontFamily: '"Noto Sans SC", sans-serif',
        stroke: '#000000', strokeThickness: Math.round(2 * DPR),
      }).setOrigin(0.5);
      this.escortNpcSprite.add(this.escortNpcNameLabel);

      EventBus.emit(GameEvents.LOG_MESSAGE, { text: `${en.name} 出现了！护送目标已标记。`, type: 'system' });
      break; // Only one escort quest at a time
    }
  }

  private destroyEscortNpc(): void {
    if (this.escortNpcSprite) {
      this.escortNpcSprite.destroy();
      this.escortNpcSprite = null;
      this.escortNpcHpBar = null;
      this.escortNpcHpBarBg = null;
      this.escortNpcNameLabel = null;
    }
    this.escortQuestId = null;
  }

  /** Update escort NPC: follow player, take damage from nearby monsters, check arrival. */
  private updateEscortNpc(time: number, _delta: number): void {
    if (!this.escortNpcSprite || !this.escortQuestId) return;

    // Follow player (stay 1-2 tiles behind)
    const dx = this.player.tileCol - this.escortNpcTileCol;
    const dy = this.player.tileRow - this.escortNpcTileRow;
    const dist = Math.sqrt(dx * dx + dy * dy);
    if (dist > 2) {
      const speed = 0.05; // tiles per frame (slightly slower than player)
      const nx = dx / dist;
      const ny = dy / dist;
      let newCol = this.escortNpcTileCol + nx * speed;
      let newRow = this.escortNpcTileRow + ny * speed;
      newCol = Math.max(1, Math.min(this.mapData.cols - 2, newCol));
      newRow = Math.max(1, Math.min(this.mapData.rows - 2, newRow));
      const checkCol = Math.round(newCol);
      const checkRow = Math.round(newRow);
      if (this.mapData.collisions[checkRow]?.[checkCol]) {
        this.escortNpcTileCol = newCol;
        this.escortNpcTileRow = newRow;
      }
    }

    // Nearby monsters attack escort NPC (aggro if within 4 tiles)
    for (const monster of this.monsters) {
      if (!monster.isAlive()) continue;
      const md = euclideanDistance(monster.tileCol, monster.tileRow, this.escortNpcTileCol, this.escortNpcTileRow);
      if (md < 4 && monster.isAggro() && time - (monster as unknown as { lastEscortAttack?: number }).lastEscortAttack! > 2000) {
        const dmg = Math.max(1, Math.floor(monster.definition.damage * 0.3));
        this.escortNpcHp -= dmg;
        (monster as unknown as { lastEscortAttack?: number }).lastEscortAttack = time;
        this.showDamageText(this.escortNpcSprite.x, this.escortNpcSprite.y - 10, dmg, false, false, true);
        if (this.escortNpcHp <= 0) {
          this.handleEscortNpcDeath();
          return;
        }
      }
    }

    // Update sprite position
    const worldPos = cartToIso(this.escortNpcTileCol, this.escortNpcTileRow);
    this.escortNpcSprite.setPosition(worldPos.x, worldPos.y);
    this.escortNpcSprite.setDepth(worldPos.y + 60);

    // Update HP bar
    if (this.escortNpcHpBar) {
      const hpRatio = this.escortNpcHp / this.escortNpcMaxHp;
      this.escortNpcHpBar.width = Math.max(0, 28 * hpRatio);
      this.escortNpcHpBar.fillColor = hpRatio > 0.5 ? 0x27ae60 : hpRatio > 0.25 ? 0xf39c12 : 0xe74c3c;
    }

    // Check if escort NPC has arrived at destination (gate on NPC proximity, not just player)
    const escortDx = this.escortNpcTileCol - this.escortDestCol;
    const escortDy = this.escortNpcTileRow - this.escortDestRow;
    const escortDist = Math.sqrt(escortDx * escortDx + escortDy * escortDy);
    if (escortDist <= 5) {
      // Also check player is nearby
      const playerDist = euclideanDistance(this.player.tileCol, this.player.tileRow, this.escortDestCol, this.escortDestRow);
      if (playerDist <= 6) {
        // Escort complete
        const quest = this.questSystem.quests.get(this.escortQuestId);
        if (quest) {
          for (const obj of quest.objectives) {
            if (obj.type === 'escort') {
              this.questSystem.updateProgress('escort', obj.targetId);
            }
          }
          EventBus.emit(GameEvents.LOG_MESSAGE, { text: `护送完成! ${quest.escortNpc?.name ?? ''}安全到达目的地。`, type: 'system' });
        }
        this.destroyEscortNpc();
      }
    }
  }

  /** Handle escort NPC death — fail the associated quest. */
  private handleEscortNpcDeath(): void {
    if (!this.escortNpcSprite || !this.escortQuestId) return;
    // Death animation
    if (this.vfx) {
      this.vfx.deathBurst(this.escortNpcSprite.x, this.escortNpcSprite.y - 16, 0xe67e22);
    }
    this.tweens.add({
      targets: this.escortNpcSprite,
      alpha: 0, scaleX: 0.5, scaleY: 0.5, duration: 600, ease: 'Power2',
      onComplete: () => { this.destroyEscortNpc(); },
    });
    // Fail the quest
    this.questSystem.failQuest(this.escortQuestId);
    EventBus.emit(GameEvents.LOG_MESSAGE, { text: '护送目标已死亡! 任务失败。', type: 'system' });
  }

  // ---------------------------------------------------------------------------
  // ─── Defend Quest Runtime ─────────────────────────────────────────────────
  // ---------------------------------------------------------------------------

  /** Spawn defend target if any defend quest is active in this zone. */
  private spawnDefendTarget(): void {
    this.destroyDefendTarget();
    const activeQuests = this.questSystem.getActiveQuests();
    for (const { quest, progress } of activeQuests) {
      if (quest.type !== 'defend' || progress.status !== 'active') continue;
      if (quest.zone !== this.currentMapId) continue;
      if (!quest.defendTarget) continue;

      const dt = quest.defendTarget;
      this.defendTargetCol = dt.col;
      this.defendTargetRow = dt.row;
      this.defendQuestId = quest.id;
      this.defendTotalWaves = dt.totalWaves;
      // Resume wave count from progress
      const waveObj = quest.objectives.find(o => o.type === 'defend_wave');
      const progressObj = waveObj ? progress.objectives[quest.objectives.indexOf(waveObj)] : undefined;
      this.defendCurrentWave = progressObj?.current ?? 0;
      this.defendWaveTimer = 0;
      this.defendWaveActive = false;
      this.defendWaveMonsters = [];

      // HP scales with zone level and waves
      const baseHp = quest.level * 30 + 200;
      this.defendTargetHp = baseHp;
      this.defendTargetMaxHp = baseHp;

      const worldPos = cartToIso(dt.col, dt.row);
      this.defendTargetSprite = this.add.container(worldPos.x, worldPos.y);
      this.defendTargetSprite.setDepth(worldPos.y + 50);

      // Defend target — large red/brown structure
      const base = this.add.rectangle(0, -16, 40, 48, 0x8b4513);
      base.setStrokeStyle(2, 0xc0392b);
      this.defendTargetSprite.add(base);

      // Glow indicator
      const glow = this.add.ellipse(0, 8, 48, 12, 0xe74c3c, 0.3);
      this.defendTargetSprite.add(glow);
      this.defendTargetSprite.sendToBack(glow);

      // HP bar
      this.defendTargetHpBarBg = this.add.rectangle(0, -48, 36, 5, 0x1a1a1a);
      this.defendTargetHpBarBg.setStrokeStyle(0.5, 0x333333);
      this.defendTargetSprite.add(this.defendTargetHpBarBg);

      this.defendTargetHpBar = this.add.rectangle(-18, -48, 36, 5, 0xe74c3c);
      this.defendTargetHpBar.setOrigin(0, 0.5);
      this.defendTargetSprite.add(this.defendTargetHpBar);

      // Name label
      this.defendTargetNameLabel = this.add.text(0, -58, dt.name, {
        fontSize: fs(10), color: '#e74c3c', fontFamily: '"Noto Sans SC", sans-serif',
        stroke: '#000000', strokeThickness: Math.round(2 * DPR),
      }).setOrigin(0.5);
      this.defendTargetSprite.add(this.defendTargetNameLabel);

      EventBus.emit(GameEvents.LOG_MESSAGE, { text: `${dt.name} 需要保护！准备迎接敌袭。`, type: 'system' });
      break;
    }
  }

  private destroyDefendTarget(): void {
    if (this.defendTargetSprite) {
      this.defendTargetSprite.destroy();
      this.defendTargetSprite = null;
      this.defendTargetHpBar = null;
      this.defendTargetHpBarBg = null;
      this.defendTargetNameLabel = null;
    }
    this.defendQuestId = null;
    this.defendWaveMonsters = [];
    this.defendWaveActive = false;
  }

  /** Update defend quest: spawn waves, track target HP, advance progress. */
  private updateDefendQuest(time: number, _delta: number): void {
    if (!this.defendTargetSprite || !this.defendQuestId) return;

    // Check if all waves are done
    if (this.defendCurrentWave >= this.defendTotalWaves && !this.defendWaveActive) {
      // All waves cleared, target still alive — quest complete!
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: '所有浪潮已击退！防守成功！', type: 'system' });
      this.destroyDefendTarget();
      return;
    }

    const playerDist = euclideanDistance(this.player.tileCol, this.player.tileRow, this.defendTargetCol, this.defendTargetRow);

    // Start next wave when player is within 15 tiles and no wave is active
    if (!this.defendWaveActive && playerDist < 15) {
      if (this.defendWaveTimer === 0) {
        this.defendWaveTimer = time;
      }
      // 5-second delay between waves
      if (time - this.defendWaveTimer > 5000) {
        this.spawnDefendWave(this.defendCurrentWave);
        this.defendWaveActive = true;
        this.defendWaveTimer = 0;
        EventBus.emit(GameEvents.LOG_MESSAGE, { text: `第 ${this.defendCurrentWave + 1}/${this.defendTotalWaves} 波敌人来袭!`, type: 'system' });
      }
    }

    // Check if current wave is cleared
    if (this.defendWaveActive) {
      const aliveWaveMonsters = this.defendWaveMonsters.filter(m => m.isAlive());
      if (aliveWaveMonsters.length === 0) {
        // Wave cleared
        this.defendCurrentWave++;
        this.defendWaveActive = false;
        this.defendWaveTimer = time; // Start timer for next wave

        // Update quest progress
        const quest = this.questSystem.quests.get(this.defendQuestId!);
        if (quest) {
          const waveObj = quest.objectives.find(o => o.type === 'defend_wave');
          if (waveObj) {
            this.questSystem.updateProgress('defend_wave', waveObj.targetId);
          }
        }
      } else {
        // Wave monsters attack defend target
        for (const monster of aliveWaveMonsters) {
          const md = euclideanDistance(monster.tileCol, monster.tileRow, this.defendTargetCol, this.defendTargetRow);
          if (md < 3 && time - ((monster as unknown as { lastDefendAttack?: number }).lastDefendAttack ?? 0) > 2000) {
            const dmg = Math.max(1, Math.floor(monster.definition.damage * 0.2));
            this.defendTargetHp -= dmg;
            (monster as unknown as { lastDefendAttack?: number }).lastDefendAttack = time;
            this.showDamageText(this.defendTargetSprite!.x, this.defendTargetSprite!.y - 10, dmg, false, false, true);
            if (this.defendTargetHp <= 0) {
              this.handleDefendTargetDestroyed();
              return;
            }
          }
        }
      }
    }

    // Update HP bar
    if (this.defendTargetHpBar) {
      const hpRatio = this.defendTargetHp / this.defendTargetMaxHp;
      this.defendTargetHpBar.width = Math.max(0, 36 * hpRatio);
      this.defendTargetHpBar.fillColor = hpRatio > 0.5 ? 0xe74c3c : hpRatio > 0.25 ? 0xf39c12 : 0x7f0000;
    }
  }

  /** Spawn a wave of enemies around the defend target. */
  private spawnDefendWave(waveIndex: number): void {
    const quest = this.questSystem.quests.get(this.defendQuestId ?? '');
    if (!quest) return;

    const zoneMonsters = MonstersByZone[this.currentMapId];
    if (!zoneMonsters || zoneMonsters.length === 0) return;

    const monstersPerWave = 3 + waveIndex; // Scale with wave number
    const spawnRadius = 8;

    for (let i = 0; i < monstersPerWave; i++) {
      const angle = (Math.PI * 2 * i) / monstersPerWave;
      const spawnCol = Math.round(this.defendTargetCol + Math.cos(angle) * spawnRadius);
      const spawnRow = Math.round(this.defendTargetRow + Math.sin(angle) * spawnRadius);

      // Pick a random monster definition from the zone
      const rawDef = zoneMonsters[randomInt(0, zoneMonsters.length - 1)];
      if (!rawDef) continue;
      // Apply difficulty scaling first, then wave scaling
      const diffDef = DifficultySystem.scaleMonster(rawDef, this.difficulty);

      // Scale monster stats with wave
      const scaledDef = { ...diffDef, hp: Math.floor(diffDef.hp * (1 + waveIndex * 0.3)), damage: Math.floor(diffDef.damage * (1 + waveIndex * 0.2)) };

      const clampedCol = Math.max(2, Math.min(this.mapData.cols - 3, spawnCol));
      const clampedRow = Math.max(2, Math.min(this.mapData.rows - 3, spawnRow));

      const monster = new Monster(this, scaledDef, clampedCol, clampedRow);
      monster.state = 'chase';
      this.monsters.push(monster);
      this.defendWaveMonsters.push(monster);
    }
  }

  /** Handle defend target destruction — fail the quest. */
  private handleDefendTargetDestroyed(): void {
    if (!this.defendTargetSprite || !this.defendQuestId) return;
    if (this.vfx) {
      this.vfx.deathBurst(this.defendTargetSprite.x, this.defendTargetSprite.y - 16, 0xe74c3c);
    }
    this.tweens.add({
      targets: this.defendTargetSprite,
      alpha: 0, scaleX: 0.3, scaleY: 0.3, duration: 800, ease: 'Power2',
      onComplete: () => { this.destroyDefendTarget(); },
    });
    // Fail the quest
    this.questSystem.failQuest(this.defendQuestId);
    EventBus.emit(GameEvents.LOG_MESSAGE, { text: '防守目标被摧毁! 任务失败。', type: 'system' });
  }

  // ---------------------------------------------------------------------------
  // ─── Craft Quest Wiring ───────────────────────────────────────────────────
  // ---------------------------------------------------------------------------

  /** Advance craft quest phases when player interacts with the appropriate NPC. */
  private advanceCraftQuestFromNpc(npcId: string): void {
    const activeQuests = this.questSystem.getActiveQuests();
    for (const { quest, progress } of activeQuests) {
      if (quest.type !== 'craft' || progress.status !== 'active') continue;
      if (!quest.craftPhases) continue;

      // Check if this NPC is the craft NPC and the collect phase is done
      if (quest.craftPhases.craftNpc === npcId) {
        // Check that all craft_collect objectives are satisfied
        const collectDone = quest.objectives
          .filter(o => o.type === 'craft_collect')
          .every(o => {
            const idx = quest.objectives.indexOf(o);
            return progress.objectives[idx].current >= o.required;
          });
        if (collectDone) {
          const craftObj = quest.objectives.find(o => o.type === 'craft_craft');
          if (craftObj) {
            const craftIdx = quest.objectives.indexOf(craftObj);
            if (progress.objectives[craftIdx].current < craftObj.required) {
              this.questSystem.updateProgress('craft_craft', craftObj.targetId);
              EventBus.emit(GameEvents.LOG_MESSAGE, { text: `制作完成: ${craftObj.targetName}`, type: 'system' });
            }
          }
        }
      }

      // Check if this NPC is the deliver NPC and the craft phase is done
      if (quest.craftPhases.deliverNpc === npcId) {
        const craftDone = quest.objectives
          .filter(o => o.type === 'craft_craft')
          .every(o => {
            const idx = quest.objectives.indexOf(o);
            return progress.objectives[idx].current >= o.required;
          });
        if (craftDone) {
          const deliverObj = quest.objectives.find(o => o.type === 'craft_deliver');
          if (deliverObj) {
            const deliverIdx = quest.objectives.indexOf(deliverObj);
            if (progress.objectives[deliverIdx].current < deliverObj.required) {
              this.questSystem.updateProgress('craft_deliver', deliverObj.targetId);
              EventBus.emit(GameEvents.LOG_MESSAGE, { text: `交付完成: ${deliverObj.targetName}`, type: 'system' });
            }
          }
        }
      }
    }
  }

  /** Find the nearest alive monster to a given tile position. */
  private findMonsterNear(col: number, row: number): Monster | null {
    let best: Monster | null = null;
    let bestDist = 3; // max search radius
    for (const m of this.monsters) {
      if (!m.isAlive()) continue;
      const d = euclideanDistance(m.tileCol, m.tileRow, col, row);
      if (d < bestDist) { bestDist = d; best = m; }
    }
    return best;
  }

  // ---------------------------------------------------------------------------
  // ─── Pet Visual Follower ────────────────────────────────────────────────────
  // ---------------------------------------------------------------------------

  /** Create or recreate the pet sprite following the player. */
  spawnPetSprite(): void {
    this.destroyPetSprite();
    const petInst = this.homesteadSystem.getActivePetInstance();
    const petDef = this.homesteadSystem.getActivePetDef();
    if (!petInst || !petDef) return;

    // Position 2-3 tiles offset from player
    this.petTileCol = this.player.tileCol - 2;
    this.petTileRow = this.player.tileRow + 1;
    const worldPos = cartToIso(this.petTileCol, this.petTileRow);

    this.petSprite = this.add.container(worldPos.x, worldPos.y);
    this.petSprite.setDepth(worldPos.y + 50);

    // Pet body — small colored circle based on rarity
    const rarityColors: Record<string, number> = {
      common: 0x88cc88,
      rare: 0x5599ff,
      epic: 0xcc66ff,
    };
    const color = rarityColors[petDef.rarity] ?? 0x88cc88;

    // Pet body (small circular shape)
    const body = this.add.circle(0, -12, 10, color);
    body.setStrokeStyle(1.5, 0xffffff, 0.5);
    this.petSprite.add(body);

    // Shadow
    const shadow = this.add.ellipse(0, 4, 16, 6, 0x000000, 0.2);
    this.petSprite.add(shadow);
    this.petSprite.sendToBack(shadow);

    // Friendly indicator (small diamond)
    const indicator = this.add.rectangle(0, -28, 4, 4, 0x88ccff);
    indicator.setAngle(45);
    this.petSprite.add(indicator);

    // Name label with evolution suffix
    const displayName = this.homesteadSystem.getPetDisplayName(petInst);
    this.petNameLabel = this.add.text(0, -36, `${displayName} Lv.${petInst.level}`, {
      fontSize: fs(9), color: '#aaddff', fontFamily: '"Noto Sans SC", sans-serif',
      stroke: '#000000', strokeThickness: Math.round(2 * DPR),
    }).setOrigin(0.5);
    this.petSprite.add(this.petNameLabel);

    // Idle floating animation
    this.tweens.add({
      targets: body,
      y: body.y - 3,
      duration: 1200,
      yoyo: true,
      repeat: -1,
      ease: 'Sine.easeInOut',
    });
  }

  destroyPetSprite(): void {
    if (this.petSprite) {
      this.tweens.killTweensOf(this.petSprite);
      // Kill tweens on children too
      for (const child of this.petSprite.list) {
        this.tweens.killTweensOf(child);
      }
      this.petSprite.destroy();
      this.petSprite = null;
      this.petNameLabel = null;
    }
  }

  /** Update the pet follower position — follows player with 2-3 tile offset. */
  private updatePetFollower(_time: number, _delta: number): void {
    if (!this.petSprite) {
      // Check if we should spawn a pet sprite (e.g., pet was activated mid-game)
      if (this.homesteadSystem.activePet) {
        this.spawnPetSprite();
      }
      return;
    }
    if (!this.homesteadSystem.activePet) {
      this.destroyPetSprite();
      return;
    }

    // Follow player with smooth interpolation at 2 tile offset
    const targetCol = this.player.tileCol - 2;
    const targetRow = this.player.tileRow + 1;
    const dx = targetCol - this.petTileCol;
    const dy = targetRow - this.petTileRow;
    const dist = Math.sqrt(dx * dx + dy * dy);

    if (dist > 0.2) {
      const speed = 0.05;
      const nx = dx / dist;
      const ny = dy / dist;
      this.petTileCol += nx * Math.min(speed * dist, speed * 3);
      this.petTileRow += ny * Math.min(speed * dist, speed * 3);
    }

    const worldPos = cartToIso(this.petTileCol, this.petTileRow);
    this.petSprite.setPosition(worldPos.x, worldPos.y);
    this.petSprite.setDepth(worldPos.y + 50);

    // Update name label if pet level changes
    const petInst = this.homesteadSystem.getActivePetInstance();
    if (petInst && this.petNameLabel) {
      const displayName = this.homesteadSystem.getPetDisplayName(petInst);
      const expected = `${displayName} Lv.${petInst.level}`;
      if (this.petNameLabel.text !== expected) {
        this.petNameLabel.setText(expected);
      }
    }
  }

  /** Pet periodic combat attack — 5-15% of player damage scaling with pet level. */
  private handlePetCombat(time: number): void {
    if (!this.homesteadSystem.activePet || !this.homesteadSystem.canPetAttack(time)) return;

    // Disable in safe zones
    const safeRadius = this.mapData.safeZoneRadius ?? 9;
    for (const camp of this.campPositions) {
      if (euclideanDistance(this.player.tileCol, this.player.tileRow, camp.col, camp.row) < safeRadius) {
        return;
      }
    }

    // Find player's current attack target or nearest aggroed monster
    const target = this.player.attackTarget
      ? this.monsters.find(m => m.id === this.player.attackTarget && m.isAlive())
      : this.findNearestAggroMonster();

    if (!target || !target.isAlive()) return;

    // Calculate pet damage based on player damage
    const eqStats = this.getEquipStats();
    const playerDamage = this.player.baseDamage + (eqStats.damage ?? 0);
    const petDamage = this.homesteadSystem.calculatePetDamage(playerDamage);

    if (petDamage <= 0) return;

    this.homesteadSystem.recordPetAttack(time);

    // Apply damage through CombatSystem for consistency
    target.takeDamage(petDamage, this.petSprite?.x ?? this.player.sprite.x, this.petSprite?.y ?? this.player.sprite.y);
    this.showDamageText(target.sprite.x + 10, target.sprite.y - 30, petDamage, false, false, false, 'physical');

    // Brief visual feedback on pet sprite
    if (this.petSprite) {
      this.tweens.add({
        targets: this.petSprite,
        scaleX: 1.2,
        scaleY: 1.2,
        duration: 100,
        yoyo: true,
        ease: 'Power2',
      });
    }

    if (!target.isAlive()) {
      this.onMonsterKilled(target);
    }
  }

  /** Boss pet drop table: elite bosses have a chance to drop specific pets. */
  private checkBossPetDrop(monsterId: string): void {
    const bossDrops: Record<string, { petId: string; chance: number }> = {
      'werewolf_alpha': { petId: 'pet_cat', chance: 0.15 },
      'mountain_troll': { petId: 'pet_storm_wolf', chance: 0.12 },
      'phoenix': { petId: 'pet_phoenix', chance: 0.10 },
      'demon_lord': { petId: 'pet_dragon', chance: 0.15 },
      'goblin_chief': { petId: 'pet_owl', chance: 0.20 },
    };
    const drop = bossDrops[monsterId];
    if (!drop) return;
    if (Math.random() < drop.chance) {
      this.homesteadSystem.addPet(drop.petId);
    }
  }

  shutdown(): void {
    this.isTransitioning = false;
    this.isPortaling = false;
    this.destroyMercenarySprite();
    this.destroyPetSprite();
    this.destroyEscortNpc();
    this.destroyDefendTarget();
    // Clean up rare pet spawn sprites
    for (const ps of this.petSpawnSprites) {
      ps.sprite.destroy();
    }
    this.petSpawnSprites = [];
    // Clean up lore sprites
    for (const ls of this.loreSprites) {
      ls.sprite.destroy();
    }
    this.loreSprites = [];
    // Clean up zone content wiring sprites
    for (const hs of this.hiddenAreaSprites) hs.sprite.destroy();
    this.hiddenAreaSprites = [];
    for (const se of this.subDungeonEntranceSprites) se.sprite.destroy();
    this.subDungeonEntranceSprites = [];
    for (const sd of this.storyDecorationSprites) sd.sprite.destroy();
    this.storyDecorationSprites = [];
    this.hideStoryDecorationTooltip();
    this.miniBossMonster = null;
    this.miniBossDialogueActive = false;
    this.input.off('pointerdown', this.handlePointerDown, this);
    EventBus.off(GameEvents.PLAYER_DIED, this.handlePlayerDied, this);
    EventBus.off(GameEvents.PLAYER_LEVEL_UP, this.handlePlayerLevelUp, this);
    EventBus.off(GameEvents.QUEST_COMPLETED, this.handleQuestCompleted, this);
    EventBus.off(GameEvents.UI_SKILL_CLICK, this.handleUiSkillClick, this);
    this.game.canvas.removeEventListener('contextmenu', this.contextMenuHandler);
    if (this.combatDebounceTimer) {
      clearTimeout(this.combatDebounceTimer);
      this.combatDebounceTimer = null;
    }
    if (this.mobileControls) {
      this.mobileControls.destroy();
      this.mobileControls = null;
    }
    if (this.ambientDustEmitter) {
      this.ambientDustEmitter.destroy();
      this.ambientDustEmitter = null;
    }
    for (const row of this.tileSprites) {
      for (const tile of row) tile?.destroy();
    }
    this.tileSprites = [];
    this.visibleTiles.clear();
    for (const sprite of this.decorSprites.values()) sprite.destroy();
    this.decorSprites.clear();
    for (const sprite of this.exitSprites.values()) sprite.destroy();
    this.exitSprites.clear();
    for (const npc of this.npcs) npc.destroy();
    this.npcs = [];
    for (const monster of this.monsters) {
      if (monster.sprite.active) monster.sprite.destroy();
    }
    this.monsters = [];
    if (this.player?.sprite?.active) this.player.sprite.destroy();
    for (const loot of this.lootDrops) loot.sprite.destroy();
    this.lootDrops = [];
    for (const potion of this.potionDrops) potion.sprite.destroy();
    this.potionDrops = [];
    for (const sprite of this.campDecorSprites.values()) sprite.destroy();
    this.campDecorSprites.clear();
    for (const emitter of this.campParticles.values()) emitter.destroy();
    this.campParticles.clear();
    if (this.targetIndicator) { this.targetIndicator.destroy(); this.targetIndicator = null; }
    if (this.vfx) this.vfx.destroy();
    if (this.lighting) this.lighting.destroy();
    if (this.weather) this.weather.destroy();
    if (this.trails) this.trails.destroy();
    if (this.statusEffects) this.statusEffects.clearAll();
    if (this.randomEventSystem) this.randomEventSystem.reset();
    this.statusTintApplied.clear();
    for (const key of this.textures.getTextureKeys()) {
      if (key.startsWith('tile_t_')) this.textures.remove(key);
    }
    SpriteGenerator.clearZoneTransientTextures(this);
    this.tileWorldPositions = [];
    this.decorWorldPositions = [];
    this.campDecorWorldPositions = [];
    this.exitLookup.clear();
  }
}
