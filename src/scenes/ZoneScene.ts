import Phaser from 'phaser';
import { TILE_WIDTH, TILE_HEIGHT, GAME_WIDTH, GAME_HEIGHT, TEXTURE_SCALE, DPR, RENDER_SCALE } from '../config';
import { cartToIso, isoToCart, worldToTile, euclideanDistance, distanceSq } from '../utils/IsometricUtils';
import { randomInt } from '../utils/MathUtils';
import { EventBus, GameEvents } from '../utils/EventBus';
import { DisposableScope } from '../utils/DisposableScope';
import { t } from '../i18n';
import { getMonsterName, getSkillName, getZoneName, getHiddenAreaName, getHiddenAreaDiscoveryText, getStatusEffectName, getEliteAffixName, getPetName, getLoreName, getQuestName, getQuestTargetName, getMercenaryName, getSubDungeonName, getRescueNpcName, getItemDisplayName as getLocalizedItemName, getNpcName, getSubDungeonEntranceName } from '../i18n/gameAccessors';
import { Player } from '../entities/Player';
import { Monster } from '../entities/Monster';
import { NPC } from '../entities/NPC';
import { PathfindingSystem } from '../systems/PathfindingSystem';
import { CombatSystem, getSkillAoeRadius, getSkillBuffValue, getSkillBuffDuration, getSkillCooldown, type EquipStats } from '../systems/CombatSystem';
import {
  CombatInputBuffer,
  DodgeController,
  cycleTargetId,
} from '../systems/CombatInputSystem';
import { getLearnedSkillLoadout } from '../systems/SkillProgressionSystem';
import { LootSystem } from '../systems/LootSystem';
import { InventorySystem } from '../systems/InventorySystem';
import { QuestSystem } from '../systems/QuestSystem';
import { HomesteadSystem } from '../systems/HomesteadSystem';
import { EmberTower } from '../systems/EmberTower';
import { PetSystem, mergeBonuses, leyFruitDropChance } from '../systems/PetSystem';
import { PetCompanion } from '../systems/PetCompanion';
import { LEY_FRUIT_ID } from '../data/pets';
import { AchievementSystem } from '../systems/AchievementSystem';
import { SaveSystem, CURRENT_SAVE_VERSION, findNearestWalkablePosition } from '../systems/SaveSystem';
import { SkillEffectSystem } from '../systems/SkillEffectSystem';
import { MobileControlsSystem, isMobileDevice } from '../systems/MobileControlsSystem';
import { LightingSystem } from '../systems/LightingSystem';
import { VFXManager } from '../systems/VFXManager';
import { classifyHit, HIT_PROFILES, type HitWeight } from '../systems/HitFeedback';
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
import { profileForQuality, resolveRenderQuality } from '../rendering/RenderQuality';
import { SpriteGenerator } from '../graphics/SpriteGenerator';
import { CHEST_OPEN_FRAME } from '../graphics/sprites/decorations/TreasureChest';
import { subDungeonGateKey } from '../graphics/sprites/effects/DungeonGates';
import { CAMP_THEMES } from '../data/camp-themes';
import { setCurrentZonePalette } from '../graphics/ZonePalette';
import { AllClasses } from '../data/classes/index';
import { AllMaps } from '../data/maps/index';
import { MonstersByZone, getMonsterDef } from '../data/monsters/index';
import { MiniBossByZone, MiniBossDialogues, MiniBossSpawns } from '../data/miniBosses';
import { LoreByZone } from '../data/loreCollectibles';
import type { LoreEntry } from '../data/loreCollectibles';
import { NPCDefinitions } from '../data/npcs';
import { AllQuests } from '../data/quests/all_quests';
import type { MapData, ClassDefinition, ItemInstance, SaveData, HiddenArea, SubDungeonEntrance, StoryDecoration, SubDungeonMapData, SkillDefinition } from '../data/types';
import { AllSubDungeons, SubDungeonMiniBosses } from '../data/subDungeons';
import { DungeonSystem } from '../systems/DungeonSystem';
import type { DungeonRunState, DungeonFloorConfig } from '../systems/DungeonSystem';
import { DifficultySystem } from '../systems/DifficultySystem';
import { SpatialGrid } from '../systems/SpatialGrid';
import { SimulationScheduler } from '../systems/SimulationScheduler';
import { DungeonBossDef, DungeonMidBossDef } from '../data/dungeonData';
import { computeNPCIndicator } from '../ui/QuestNPCIndicators';
import type { UIScene } from './UIScene';
import { GameSession } from '../game/GameSession';
import { ZoneTerrain } from '../graphics/terrain/ZoneTerrain';
import { QuestWorld, questGiverOf } from '../systems/QuestWorld';
import { StoryDirector } from '../systems/StoryDirector';
import { generateRewardChoices, isCollectObjective, questDropChance, FALLBACK_COLLECT_CHANCE } from '../systems/QuestRewards';
import { huntsToSpawn, makeHuntDefinition } from '../systems/QuestHunts';
import { computeDeathPenalty } from '../systems/SoulEcho';
import { GATEKEEPER_ID } from '../systems/DungeonSystem';
import { BOONS } from '../data/abyssRun';
import type { DungeonHudPayload } from '../utils/EventBus';
import type { SoulEchoData, MonsterDefinition } from '../data/types';

const TILE_KEYS = ['tile_grass', 'tile_dirt', 'tile_stone', 'tile_water', 'tile_wall', 'tile_camp', 'tile_camp_wall'];
const CAMPFIRE_RECOVERY_RADIUS = 5;
const CAMPFIRE_RECOVERY_RADIUS_SQ = CAMPFIRE_RECOVERY_RADIUS * CAMPFIRE_RECOVERY_RADIUS;
const CAMPFIRE_HP_REGEN_MULTIPLIER = 50;
const CAMPFIRE_MANA_REGEN_MULTIPLIER = 50;
const ZONE_FLOATING_TEXT_DEPTH = 4500;
/** Spark/flash tint for each class's basic-attack impacts. */
const CLASS_IMPACT_COLORS: Record<string, number> = {
  warrior: 0xffd98a,
  mage: 0xc7a6ff,
  rogue: 0x9dffc8,
};
/** AoE skills that drop onto the enemy instead of centring on the caster. */
const GROUND_AOE_SKILLS = new Set(['meteor', 'blizzard', 'fire_wall', 'arcane_torrent', 'arrow_rain', 'poison_cloud']);

/** Element tint for a skill's per-target impact burst. */
function skillImpactColor(skillId: string, damageType: string): number {
  if (skillId.includes('fire') || skillId === 'meteor' || skillId === 'combustion') return 0xff6600;
  if (skillId.includes('ice') || skillId === 'blizzard' || skillId === 'freeze') return 0x4488ff;
  if (skillId.includes('lightning')) return 0x5dade2;
  if (skillId.includes('poison')) return 0x7ed957;
  if (skillId === 'arcane_torrent') return 0xb07cff;
  return damageType === 'physical' ? 0xf1c40f : 0xf39c12;
}

const ZONE_SCREEN_UI_DEPTH = 5000;
/** An escort left this far behind (tiles) catches up next to the player. */
const ESCORT_CATCH_UP_TILES = 14;
/** World camera zoom in logical pixels (× RENDER_SCALE on the real camera). */
export const ZONE_CAMERA_ZOOM = 1.8;
/** Hold-to-move re-plans its path at most this often while the pointer stays on one tile. */
const HOLD_MOVE_REPATH_MS = 120;

function fs(basePx: number): string {
  return `${Math.round(basePx * DPR)}px`;
}

const W = GAME_WIDTH * DPR;
const H = GAME_HEIGHT * DPR;

export class ZoneScene extends Phaser.Scene {
  private session: GameSession | null = null;
  private subscriptions = new DisposableScope();
  player!: Player;
  private monsters: Monster[] = [];
  /** Grid-based spatial index for efficient proximity queries on monsters. */
  private monsterGrid!: SpatialGrid<Monster>;
  private activeMonsters: Monster[] = [];
  private simulationScheduler = new SimulationScheduler();
  private npcs: NPC[] = [];
  private mapData!: MapData;
  currentMapId!: string;
  private pathfinding!: PathfindingSystem;
  private combatSystem!: CombatSystem;
  private skillEffects!: SkillEffectSystem;
  lootSystem!: LootSystem;
  inventorySystem!: InventorySystem;
  questSystem!: QuestSystem;
  /** Gather nodes, quest pickups and the guide arrow (null in dungeons). */
  questWorld: QuestWorld | null = null;
  /** Story beats: prologue, chapter cards, cutscenes, boss intros (null in dungeons). */
  storyDirector: StoryDirector | null = null;
  private chapterCardPending = false;
  homesteadSystem!: HomesteadSystem;
  /** The Ember Tower in this zone (hearthstones, or the tower itself) and the homestead's actions. */
  emberTower: EmberTower | null = null;
  petSystem!: PetSystem;
  achievementSystem!: AchievementSystem;
  saveSystem!: SaveSystem;
  private tileSprites: (Phaser.GameObjects.Image | null)[][] = [];
  /** Zone-themed ground / wall textures and wall overlays. */
  private terrain: ZoneTerrain | null = null;
  private decorSprites: Map<string, Phaser.GameObjects.Image> = new Map();
  private exitSprites: Map<string, Phaser.GameObjects.Image> = new Map();
  private campDecorSprites: Map<string, Phaser.GameObjects.GameObject> = new Map();
  private campParticles: Map<string, Phaser.GameObjects.Particles.ParticleEmitter> = new Map();
  private campDecorPositions: { col: number; row: number; type: string }[] = [];
  private tileWorldPositions: { x: number; y: number }[][] = [];
  private decorWorldPositions: Array<{ key: string; type: string; x: number; y: number }> = [];
  /** Visible tall decorations (trees, tents, statues) checked each frame for player occlusion. */
  private occluderDecor: Set<Phaser.GameObjects.Image> = new Set();
  private campDecorWorldPositions: Array<{ key: string; type: string; x: number; y: number }> = [];
  private exitLookup: Map<string, MapData['exits'][number]> = new Map();
  private visibleTiles: Set<number> = new Set();
  private tilePool: Phaser.GameObjects.Image[] = [];
  private lastVisibleTileBounds = '';
  private exitLabels: Map<string, Phaser.GameObjects.Text> = new Map();
  private cursors!: Phaser.Types.Input.Keyboard.CursorKeys;
  private wasd!: Record<string, Phaser.Input.Keyboard.Key>;
  private readonly combatInput = new CombatInputBuffer(180);
  private readonly dodgeController = new DodgeController();
  private skillLoadout: ClassDefinition['skills'] = [];
  private lastMoveDirection = { dx: 1, dy: -1 };
  private gamepadButtonState = {
    dodge: false,
    target: false,
    skills: [false, false, false, false],
  };
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
  /** Previous HP/mana values for change-only event emission. */
  private _lastEmittedHp = -1;
  private _lastEmittedMaxHp = -1;
  private _lastEmittedMana = -1;
  private _lastEmittedMaxMana = -1;
  private exploredZones: Set<string> = new Set();
  private fogData: Record<string, boolean[][]> = {};
  /** Flat Uint8Array tracking explored tiles (row * mapCols + col → 1 if explored). */
  private exploredTiles!: Uint8Array;
  /** Column count used to compute flat index for exploredTiles. */
  private exploredTilesCols = 0;
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
  /** The active ley-beast in this zone (PetCompanion). */
  private petCompanion: PetCompanion | null = null;
  private petSpawnSprites: { sprite: Phaser.GameObjects.Container; col: number; row: number; petId: string }[] = [];
  private inCombat = false;
  private randomEventSystem!: RandomEventSystem;
  private isTransitioning = false;
  private isPortaling = false;
  /** Mini-boss monster reference per zone (spawned at a fixed position). */
  private miniBossMonster: Monster | null = null;
  /** Named quest monsters alive in this zone, by hunt id. */
  private questHuntMonsters = new Map<string, Monster>();
  /** The ghost left at the hero's last death, if it lies in this zone. */
  private soulEchoVisual: Phaser.GameObjects.Container | null = null;
  /** Mouse / finger held on the ground: keep walking toward it (Diablo-style hold-to-move). */
  private holdMove: { pointerId: number; col: number; row: number; repathAt: number } | null = null;
  /** Labyrinth floor: the exit opens once its seal keeper falls. */
  private dungeonSealOpen = false;
  /** Labyrinth: a boon choice is on screen (the world waits). */
  private dungeonChoosing = false;
  private dungeonSealNagAt = 0;
  private dungeonRegenTimer = 0;
  /** Quest-spawned monsters (hunts and their packs) never respawn. */
  private questSpawned = new WeakSet<Monster>();
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
  private escortPath: { col: number; row: number }[] = [];
  /** The charge waits where the quest says until the hero comes to fetch them. */
  private escortJoined = false;
  private escortRepathAt = 0;

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

  // ─── Performance Pools ─────────────────────────────────────────────
  /** Pool of floating damage text objects to avoid per-hit allocation. */
  private floatingTextPool: Phaser.GameObjects.Text[] = [];
  private damageTextStacks = new Map<string, { time: number; index: number }>();

  /** Squared distance beyond which monster AI updates are skipped (monsters still render). */
  private static readonly MONSTER_AI_CULL_DIST_SQ = 30 * 30;

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
    this.dungeonSealOpen = false;
    this.dungeonChoosing = false;
    this.holdMove = null;
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
    this.subscriptions = new DisposableScope();
    this.events.once(Phaser.Scenes.Events.SHUTDOWN, this.shutdown, this);
    // `scene.restart()` reuses the same scene instance, so transient guards must
    // be cleared explicitly when entering a new zone.
    this.isTransitioning = false;
    this.isPortaling = false;
    this.miniBossDialogueActive = false;
    this.combatInput.clear();
    this.dodgeController.reset();
    this.lastMoveDirection = { dx: 1, dy: -1 };
    this.gamepadButtonState = {
      dodge: false,
      target: false,
      skills: [false, false, false, false],
    };
    this.monsters = [];
    this.activeMonsters = [];
    this.simulationScheduler.reset();
    this.monsterGrid = new SpatialGrid<Monster>(this.mapData.cols, this.mapData.rows, 16);
    this.npcs = [];
    this.lootDrops = [];
    this.potionDrops = [];
    this.statusTintApplied.clear();
    // Clean up zone content wiring fields
    this.hiddenAreaSprites = [];
    this.subDungeonEntranceSprites = [];
    this.storyDecorationSprites = [];
    this.storyDecorationTooltip = null;
    this.exploredTilesCols = this.mapData.cols;
    this.exploredTiles = new Uint8Array(this.mapData.rows * this.mapData.cols);

    if (!this.session) this.session = new GameSession();
    const zoneRuntime = this.session.beginZone(
      this.currentMapId,
      this.mapData.levelRange as [number, number],
      this.mapData.safeZoneRadius ?? 9,
    );
    this.combatSystem = zoneRuntime.combat;
    this.lootSystem = zoneRuntime.loot;
    this.skillEffects = new SkillEffectSystem(this);
    this.statusEffects = zoneRuntime.statusEffects;
    this.eliteAffixSystem = zoneRuntime.eliteAffixes;
    this.randomEventSystem = zoneRuntime.randomEvents;
    this.inventorySystem = this.session.inventory;
    this.questSystem = this.session.quests;
    this.homesteadSystem = this.session.homestead;
    this.petSystem = this.session.pets;
    this.achievementSystem = this.session.achievements;
    this.saveSystem = this.session.saves;
    this.mercenarySystem = this.session.mercenaries;

    // Initialize tile sprite grid
    this.tileSprites = [];
    for (let r = 0; r < this.mapData.rows; r++) {
      this.tileSprites.push(new Array(this.mapData.cols).fill(null));
    }
    this.visibleTiles = new Set();
    this.tilePool = [];
    this.lastVisibleTileBounds = '';
    this.exitLabels = new Map();
    this.decorSprites = new Map();
    this.occluderDecor = new Set();
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
      if (s.spirit) this.player.spirit.restore(s.spirit);
      this.player.buffs = s.buffs;
      this.player.autoCombat = s.autoCombat;
      if (s.autoLootMode) this.player.autoLootMode = s.autoLootMode;
    }
    this.refreshSkillLoadout();
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
    if (!this.isInDungeon) {
      this.questWorld = new QuestWorld({
        scene: this,
        quests: this.questSystem,
        mapId: this.currentMapId,
        mapData: this.mapData,
        player: () => this.player,
        monsters: () => this.monsters,
        npcs: () => this.npcs,
        escortTile: () => this.getEscortTile(),
      });
      if (this.session) {
        this.storyDirector = new StoryDirector({
          scene: this,
          story: this.session.story,
          mapId: this.currentMapId,
          player: () => this.player,
          npcs: () => this.npcs,
          monsters: () => this.monsters,
          setCinematic: (on) => this.setCinematic(on),
          save: () => this.autoSave(),
          grantPet: (petId) => { this.session?.pets.addPet(petId); },
        });
      }
    }
    this.createPetCompanion();
    this.spawnEscortNpc();
    this.spawnDefendTarget();
    this.questHuntMonsters.clear();
    this.spawnQuestHunts(false);
    this.soulEchoVisual = null;
    this.spawnSoulEchoVisual();
    this.buildCampDecorations();
    this.emberTower?.destroy();
    this.emberTower = new EmberTower({
      scene: this,
      mapId: this.currentMapId,
      mapData: this.mapData,
      regularZone: !this.isInDungeon && !this.isInSubDungeon,
      homestead: this.homesteadSystem,
      pets: this.session.pets,
      quests: this.questSystem,
      inventory: this.inventorySystem,
      player: () => this.player,
      spendGold: (n) => { this.player.gold = Math.max(0, this.player.gold - n); },
      addGold: (n) => { this.player.gold += n; },
      createItem: (baseId) => {
        const item = this.lootSystem.createItem(baseId, this.player.level, 'normal');
        if (item) item.identified = true;
        return item;
      },
      isSafe: () => !this.inCombat && this.player.hp > 0 && !this.storyDirector?.cinematic && !this.isTransitioning,
      changeZone: (mapId, col, row) => this.changeZone(mapId, col, row),
      save: () => { void this.autoSave(); },
      statsChanged: () => this.invalidateEquipStats(),
      floatText: (x, y, text, color) => {
        const label = this.add.text(x, y, text, {
          fontSize: fs(12), color, fontFamily: '"Cinzel", serif', stroke: '#000000', strokeThickness: Math.round(2 * DPR),
        }).setOrigin(0.5).setDepth(ZONE_FLOATING_TEXT_DEPTH);
        this.tweens.add({ targets: label, y: y - 30, alpha: 0, duration: 1400, ease: 'Power2', onComplete: () => label.destroy() });
      },
    });
    this.emberTower.retroGrantPets();
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

    // Initial tile render (zone-themed terrain; on-screen transitions built now, the rest time-sliced)
    this.terrain?.destroy();
    this.terrain = new ZoneTerrain(this, this.mapData);
    this.updateVisibleTiles();

    // Camera
    this.cameras.main.startFollow(this.player.sprite, true, 0.08, 0.08);
    this.cameras.main.setZoom(ZONE_CAMERA_ZOOM * RENDER_SCALE);

    // Lighting system — ambient darkness + point lights
    const renderQuality = profileForQuality(resolveRenderQuality());
    this.lighting = new LightingSystem(this, renderQuality);
    this.lighting.setZone(this.isInDungeon && this.dungeonFloorConfig ? DungeonSystem.getTheme(this.dungeonFloorConfig).mapTheme : this.currentMapId);
    this.setupDungeonFloor();
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
    this.weather = new WeatherSystem(this, renderQuality);
    this.weather.setZone(this.currentMapId);

    // Trail renderer — weapon trails, ground scorch marks
    this.trails = new TrailRenderer(this);

    // Camera PostFX — painterly mood (WebGL only)
    if (this.renderer.type === Phaser.WEBGL) {
      const cam = this.cameras.main;

      // Gentle bloom — brightens highlights (loot glow, magic, fire)
      if (renderQuality.bloom) cam.postFX.addBloom(0xffffff, 0.6, 0.6, 0.5, 0.8);

      // Very subtle vignette — barely visible edge darkening
      cam.postFX.addVignette(0.5, 0.5, 0.95, 0.12);
    }

    // Ambient dust particles
    this.createAmbientDust();

    // Fade in from black on zone entry
    this.cameras.main.fadeIn(400);

    // Color grading shader (WebGL only, no-op on Canvas)
    if (renderQuality.colorGrading) applyColorGrading(this);

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
        Q: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.Q),
        SPACE: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.SPACE),
        I: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.I),
        K: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.K),
        M: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.M),
        H: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.H),
        C: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.C),
        J: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.J),
        O: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.O),
        R: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.R),
        P: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.P),
        U: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.U),
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

    this.subscriptions.onDom(this.game.canvas, 'contextmenu', this.contextMenuHandler);
    this.subscriptions.on(this.input, 'pointerdown', this.handlePointerDown, this);

    if (!this.scene.isActive('UIScene')) {
      this.scene.launch('UIScene', { player: this.player, zone: this });
    } else {
      EventBus.emit('ui:refresh', { player: this.player, zone: this });
    }
    this.subscriptions.on(EventBus, GameEvents.PLAYER_DIED, this.handlePlayerDied, this);
    // Ley-beast passives feed the equipment stat cache.
    this.subscriptions.on(EventBus, GameEvents.PET_CHANGED, this.invalidateEquipStats, this);
    this.subscriptions.on(EventBus, GameEvents.PLAYER_LEVEL_UP, this.handlePlayerLevelUp, this);
    this.subscriptions.on(EventBus, GameEvents.QUEST_COMPLETED, this.handleQuestCompleted, this);
    this.subscriptions.on(EventBus, GameEvents.QUEST_PROGRESS, this.handleQuestProgress, this);
    this.subscriptions.on(EventBus, GameEvents.UI_SKILL_CLICK, this.handleUiSkillClick, this);
    this.subscriptions.on(EventBus, GameEvents.UI_DODGE_REQUEST, this.handleUiDodgeRequest, this);
    this.subscriptions.on(EventBus, GameEvents.UI_TARGET_CYCLE, this.cycleCombatTarget, this);
    this.subscriptions.on(EventBus, GameEvents.SKILL_LEVEL_CHANGED, this.handleSkillLevelChanged, this);
    // Update NPC quest indicators immediately when quest state changes
    this.subscriptions.on(EventBus, GameEvents.QUEST_ACCEPTED, this.updateNPCQuestMarkers, this);
    this.subscriptions.on(EventBus, GameEvents.QUEST_ACCEPTED, this.handleQuestAcceptedWorld, this);
    this.subscriptions.on(EventBus, GameEvents.QUEST_PROGRESS, this.handleQuestProgressWorld, this);
    this.subscriptions.on(EventBus, GameEvents.DUNGEON_TIER_CHOSEN, this.handleDungeonTierChosen, this);
    this.subscriptions.on(EventBus, GameEvents.DUNGEON_BOON_CHOSEN, this.handleDungeonBoonChosen, this);
    this.subscriptions.on(EventBus, GameEvents.QUEST_TURNED_IN, this.updateNPCQuestMarkers, this);
    // React to locale changes for persistent UI elements
    this.subscriptions.on(EventBus, GameEvents.LOCALE_CHANGED, this.handleLocaleChanged, this);

    this.exploredZones.add(this.currentMapId);
    this.achievementSystem.update('explore', this.currentMapId);

    EventBus.emit(GameEvents.ZONE_ENTERED, { mapId: this.currentMapId });
    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: t('zone.enterZone', { zoneName: getZoneName(this.currentMapId, this.mapData.name), min: this.mapData.levelRange[0], max: this.mapData.levelRange[1] }),
      type: 'system',
    });

    // A first visit opens with the chapter card instead of the plain banner.
    this.chapterCardPending = this.storyDirector?.start() ?? false;
    if (!this.chapterCardPending) this.showZoneBanner();
    this.autoSave();
  }

  /**
   * Freeze the world for a story beat: update() stops input, AI and incoming
   * hits while `storyDirector.cinematic` is set, and UIScene hides the HUD.
   */
  private setCinematic(on: boolean): void {
    if (on) {
      this.player.path = [];
      this.player.attackTarget = null;
    }
  }

  /** Update persistent text labels when locale changes. */
  private handleLocaleChanged(): void {
    this.mobileControls?.refreshLocale();
    // Update sub-dungeon entrance labels
    for (const se of this.subDungeonEntranceSprites) {
      const container = se.sprite;
      const children = container.list;
      for (const child of children) {
        if (child instanceof Phaser.GameObjects.Text && (child as Phaser.GameObjects.Text).style.color === '#CC88FF') {
          (child as Phaser.GameObjects.Text).setText(getSubDungeonEntranceName(se.entrance.id, se.entrance.name));
        }
      }
    }
    // Update rare ley-beast spawn labels
    for (const ps of this.petSpawnSprites) {
      for (const child of ps.sprite.list) {
        if (child instanceof Phaser.GameObjects.Text) {
          child.setText(t('zone.pet.rareLabel', { name: getPetName(ps.petId, ps.petId) }));
        }
      }
    }
    // Update hidden area reward labels
    for (const hs of this.hiddenAreaSprites) {
      const reward = hs.area.rewards[hs.rewardIndex];
      if (!reward) continue;
      const container = hs.sprite;
      const children = container.list;
      for (const child of children) {
        if (child instanceof Phaser.GameObjects.Text) {
          (child as Phaser.GameObjects.Text).setText(
            reward.type === 'chest' ? t('zone.hiddenArea.rewardChest') : reward.type === 'gold_pile' ? t('zone.hiddenArea.rewardGoldPile') : t('zone.hiddenArea.rewardScroll'),
          );
        }
      }
    }
    // Mercenary name label
    if (this.mercenaryNameLabel && this.mercenarySystem?.getMercenary()) {
      const merc = this.mercenarySystem.getMercenary()!;
      const def = MERCENARY_DEFS[merc.type];
      this.mercenaryNameLabel.setText(`${getMercenaryName(merc.type, def.name)} Lv.${merc.level}`);
    }
    // Escort NPC name label
    if (this.escortNpcNameLabel && this.escortQuestId) {
      const quest = this.questSystem.quests.get(this.escortQuestId);
      if (quest?.escortNpc) {
        this.escortNpcNameLabel.setText(quest.escortNpc.name);
      }
    }
    // Defend target name label
    if (this.defendTargetNameLabel && this.defendQuestId) {
      const quest = this.questSystem.quests.get(this.defendQuestId);
      if (quest?.defendTarget) {
        this.defendTargetNameLabel.setText(quest.defendTarget.name);
      }
    }
  }

  private handlePointerDown(pointer: Phaser.Input.Pointer): void {
    if (this.storyDirector?.cinematic) return;
    // A touch on the joystick / a touch button is not also a tap on the world.
    if (this.mobileControls?.claimsPointer(pointer)) return;
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

    // Ember Tower: 归炉 hearthstones, the return portal, the wings' plots
    if (this.emberTower?.handleClick(tile.col, tile.row)) return;

    // Sub-dungeon entrance interaction
    const subEntrance = this.findSubDungeonEntranceAt(tile.col, tile.row);
    if (subEntrance && distanceSq(this.player.tileCol, this.player.tileRow, subEntrance.col, subEntrance.row) <= 9) {
      this.enterSubDungeon(subEntrance);
      return;
    }

    // Random dungeon portal interaction
    if (this.findDungeonPortalAt(tile.col, tile.row) && distanceSq(this.player.tileCol, this.player.tileRow, ZoneScene.DUNGEON_PORTAL_COL, ZoneScene.DUNGEON_PORTAL_ROW) <= 9) {
      this.openDungeonTierPicker();
      return;
    }

    // Hidden area reward chest interaction
    const hiddenChest = this.findHiddenAreaChestAt(tile.col, tile.row);
    if (hiddenChest && distanceSq(this.player.tileCol, this.player.tileRow, hiddenChest.col, hiddenChest.row) <= 4) {
      this.collectHiddenAreaReward(hiddenChest);
      return;
    }

    const monster = this.findMonsterAt(tile.col, tile.row);
    if (monster && monster.isAlive()) {
      this.player.attackTarget = monster.id;
      EventBus.emit(GameEvents.TARGET_CHANGED, {
        targetId: monster.id,
        targetName: getMonsterName(monster.definition.id, monster.definition.name),
      });
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
        this.tryLeaveDungeonFloor();
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
        EventBus.emit(GameEvents.TARGET_CHANGED, { targetId: null, targetName: null });
      }
      // Keep steering toward the pointer for as long as it stays pressed.
      this.holdMove = { pointerId: pointer.id, col: tile.col, row: tile.row, repathAt: this.time.now + HOLD_MOVE_REPATH_MS };
    }
  }

  /**
   * Hold-to-move: while the press that started on open ground is held, the
   * hero keeps walking toward wherever it points now. The pointer's world
   * position is recomputed every frame because the camera follows the hero
   * (a still mouse over a moving view points at new ground).
   */
  private updateHoldMove(): void {
    const hold = this.holdMove;
    if (!hold) return;
    const pointer = this.input.manager.pointers.find(p => p.id === hold.pointerId);
    if (!pointer || !pointer.isDown || this.player.hp <= 0) {
      this.holdMove = null; // released: the hero finishes the last path
      return;
    }
    const world = this.cameras.main.getWorldPoint(pointer.x, pointer.y);
    const target = worldToTile(world.x, world.y);
    const col = Math.max(0, Math.min(this.mapData.cols - 1, target.col));
    const row = Math.max(0, Math.min(this.mapData.rows - 1, target.row));
    // Close enough: stand still instead of jittering around the cursor.
    if (Math.hypot(col - this.player.tileCol, row - this.player.tileRow) < 0.6) {
      this.player.path = [];
      return;
    }
    const moved = col !== hold.col || row !== hold.row;
    if (!moved && this.time.now < hold.repathAt && this.player.path.length > 0) return;
    hold.col = col;
    hold.row = row;
    hold.repathAt = this.time.now + HOLD_MOVE_REPATH_MS;
    // Pointing into a wall or water: head for the nearest ground beside it.
    const goal = this.mapData.collisions[row]?.[col] ? { col, row } : this.findWalkableNear(col, row, 3);
    if (!goal) return;
    const path = this.pathfinding.findPath(
      Math.round(this.player.tileCol), Math.round(this.player.tileRow), goal.col, goal.row,
    );
    if (path.length > 0) this.player.setPath(path);
  }

  private handlePlayerDied(): void {
    // Clear status effects on player death
    this.statusEffects.clearEntity('player');
    this.isPortaling = false;
    this.applyDeathPenalty();

    if (this.vfx) {
      this.vfx.cameraFlash(80, 0.6, 0xffffff);
      this.vfx.deathBurst(this.player.sprite.x, this.player.sprite.y - 16, 0xcc2222);
    }
    const dp = this.screenPos(0.5, 0.4);
    const deathText = this.add.text(dp.x, dp.y, t('zone.death.text'), {
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
          this.finishDungeonRun('fallen');
          this.dungeonRunState = null;
          this.dungeonFloorConfig = null;
          this.isInDungeon = false;
          EventBus.emit(GameEvents.DUNGEON_EXIT, {});
        } else {
          EventBus.emit(GameEvents.SUBDUNGEON_EXIT, { mapId: parentMapId });
        }

        EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.death.logMessage'), type: 'system' });
        this.isTransitioning = true;
        this.scene.restart({
          classId: this.player.classData.id,
          mapId: parentMapId,
          targetCol: respawnCol,
          targetRow: respawnRow,
          miniBossDialogueSeen: [...this.miniBossDialogueSeen],
          loreCollected: [...this.loreCollected],
          discoveredHiddenAreas: [...this.discoveredHiddenAreas],
          playerStats: this.getPlayerTransitionStats(this.player.maxHp, this.player.maxMana),
        });
      } else {
        const camp = this.campPositions[0];
        this.player.respawnAtCamp(camp.col, camp.row);
        this.cameras.main.fadeIn(300);
      }
    });
  }

  // ── Abyss Labyrinth (Zone 6) ───────────────────────────────

  /** A run's final result waits here until we are back in the rift and the UI can show it. */
  private static pendingRunEnd: import('../utils/EventBus').DungeonRunEndPayload | null = null;

  private abyssModalOpen(): boolean {
    return (this.scene.get('UIScene') as UIScene | undefined)?.isAbyssModalOpen?.() ?? false;
  }

  private openDungeonTierPicker(): void {
    const rec = this.session?.abyss ?? { unlockedTier: 1, bestTier: 0 };
    EventBus.emit(GameEvents.DUNGEON_TIER_PICK, { unlockedTier: rec.unlockedTier, bestTier: rec.bestTier, heroLevel: this.player.level });
  }

  private handleDungeonTierChosen(data: { tier: number }): void {
    if (!data || !(data.tier > 0) || this.isInDungeon || this.isInSubDungeon) return;
    const unlocked = this.session?.abyss.unlockedTier ?? 1;
    this.enterDungeon(Math.min(unlocked, Math.max(1, Math.floor(data.tier))));
  }

  /** Floor setup after spawning: the seal keeper, curse lighting, HUD. Outside the labyrinth, show a finished run's summary. */
  private setupDungeonFloor(): void {
    if (!this.isInDungeon || !this.dungeonFloorConfig || !this.dungeonRunState) {
      EventBus.emit(GameEvents.DUNGEON_HUD, null);
      const ended = ZoneScene.pendingRunEnd;
      if (ended) {
        ZoneScene.pendingRunEnd = null;
        // Wait out any story beat (a chapter card, a cutscene) so the summary isn't hidden under it.
        const show = (): void => {
          if (this.storyDirector?.busy || this.storyDirector?.cinematic) this.time.delayedCall(500, show);
          else EventBus.emit(GameEvents.DUNGEON_RUN_END, ended);
        };
        this.time.delayedCall(700, show);
        this.time.delayedCall(800, () => { void this.autoSave(); });
      }
      return;
    }
    const cfg = this.dungeonFloorConfig;
    this.ensureSealKeeper();
    const curse = DungeonSystem.getCurse(cfg);
    if (curse?.visionMul) this.lighting.deepen(1 - curse.visionMul);
    if (curse?.eliteChance) {
      for (const m of this.monsters) {
        if (m.definition.elite || Math.random() >= curse.eliteChance) continue;
        const affixes = this.eliteAffixSystem.rollAffixes(this.currentMapId, true);
        if (affixes.length > 0) m.applyEliteAffixes(affixes, this.eliteAffixSystem);
      }
    }
    const themeName = t(`dungeon.theme.${cfg.themeId}`);
    EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.dungeon.floorTheme', { theme: themeName }), type: 'system' });
    if (curse) {
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.dungeon.curseLog', { curse: t(`dungeon.curse.${curse.id}.name`) }), type: 'system' });
    }
    this.dungeonSealOpen = !this.sealKeeperAlive();
    this.time.delayedCall(120, () => this.emitDungeonHud());
  }

  /** Monster id holding this floor's exit seal. */
  private sealKeeperId(): string {
    const k = this.dungeonFloorConfig?.sealKeeper;
    return k === 'boss' ? 'dungeon_abyss_lord' : k === 'mid_boss' ? 'dungeon_mid_boss' : GATEKEEPER_ID;
  }

  private sealKeeperAlive(): Monster | undefined {
    const id = this.sealKeeperId();
    return this.monsters.find(m => m.definition.id === id && m.isAlive());
  }

  /** Make sure the seal keeper stands near the exit (spawn lists can drop it on a wall). */
  private ensureSealKeeper(): void {
    const cfg = this.dungeonFloorConfig;
    const run = this.dungeonRunState;
    if (!cfg || !run) return;
    const id = this.sealKeeperId();
    for (const m of this.monsters) if (m.definition.id === id) this.questSpawned.add(m);
    if (this.sealKeeperAlive()) return;
    const exit = this.mapData.exits[0];
    const start = this.mapData.playerStart;
    const gc = Math.round(exit.col + (start.col - exit.col) * 0.22);
    const gr = Math.round(exit.row + (start.row - exit.row) * 0.22);
    const spot = this.mapData.collisions[gr]?.[gc] ? { col: gc, row: gr } : this.findWalkableNear(gc, gr, 10);
    if (!spot) return;
    let def: MonsterDefinition | undefined;
    if (cfg.sealKeeper === 'gatekeeper') {
      const base = getMonsterDef(DungeonSystem.getTheme(cfg).gatekeeper);
      if (base) def = DungeonSystem.makeGatekeeper(cfg, base, run.difficulty);
    } else {
      const base = getMonsterDef(id);
      if (base) def = DungeonSystem.scaleMonster(base, cfg, run.difficulty);
    }
    if (!def) return;
    const keeper = new Monster(this, def, spot.col, spot.row);
    const affixes = this.eliteAffixSystem.rollAffixes(this.currentMapId, true);
    if (affixes.length > 0) keeper.applyEliteAffixes(affixes, this.eliteAffixSystem);
    if (cfg.sealKeeper === 'gatekeeper') {
      const body = keeper.sprite.list.find(o => o instanceof Phaser.GameObjects.Sprite) as Phaser.GameObjects.Sprite | undefined;
      body?.setScale(body.scaleX * 1.3);
    }
    this.monsters.push(keeper);
    this.monsterGrid.insert(keeper);
    this.questSpawned.add(keeper);
  }

  private emitDungeonHud(): void {
    const run = this.dungeonRunState;
    const cfg = this.dungeonFloorConfig;
    if (!this.isInDungeon || !run || !cfg) return;
    const keeper = this.dungeonSealOpen ? undefined : this.sealKeeperAlive();
    const payload: DungeonHudPayload = {
      tier: run.tier ?? 1,
      floor: cfg.floorNumber,
      totalFloors: run.totalFloors,
      theme: cfg.themeId,
      curse: cfg.curseId,
      boons: { ...(run.boons ?? {}) },
      sealOpen: this.dungeonSealOpen,
      sealKeeper: keeper ? getMonsterName(keeper.definition.id, keeper.definition.name) : null,
      kills: run.kills ?? 0,
      startedAt: run.startedAt ?? Date.now(),
    };
    EventBus.emit(GameEvents.DUNGEON_HUD, payload);
  }

  private onDungeonKill(monster: Monster): void {
    const run = this.dungeonRunState;
    if (!run) return;
    run.kills = (run.kills ?? 0) + 1;
    const curse = this.dungeonFloorConfig ? DungeonSystem.getCurse(this.dungeonFloorConfig) : null;
    if (curse?.deathBurst) this.volatileBurst(monster, curse.deathBurst);
    if (!this.dungeonSealOpen && monster.definition.id === this.sealKeeperId() && !this.sealKeeperAlive()) {
      this.openDungeonSeal();
    }
    this.emitDungeonHud();
  }

  /** The keeper fell: the exit wakes. */
  private openDungeonSeal(): void {
    this.dungeonSealOpen = true;
    const cfg = this.dungeonFloorConfig;
    EventBus.emit(GameEvents.LOG_MESSAGE, { text: t(cfg?.isBossFloor ? 'zone.dungeon.sealBrokenBoss' : 'zone.dungeon.sealBroken'), type: 'system' });
    audioManager.playSFX('quest_objective');
    this.vfx?.cameraFlash(160, 0.35, 0x9fd8ff);
    for (const [key, portal] of this.exitSprites) {
      portal.clearTint().setAlpha(1);
      const label = this.exitLabels.get(key);
      if (label && cfg) {
        label.setText(cfg.isBossFloor ? t('zone.returnToAbyssRift') : DungeonSystem.getFloorExitLabel(cfg.floorNumber + 1));
      }
      this.tweens.add({ targets: portal, scale: { from: portal.scale * 1.35, to: portal.scale }, duration: 500, ease: 'Back.easeOut' });
    }
  }

  /** Called when the hero steps onto the floor exit. */
  private tryLeaveDungeonFloor(): void {
    const cfg = this.dungeonFloorConfig;
    const run = this.dungeonRunState;
    if (!cfg || !run || this.isTransitioning || this.dungeonChoosing) return;
    if (!this.dungeonSealOpen) {
      if (this.time.now - this.dungeonSealNagAt > 3000) {
        this.dungeonSealNagAt = this.time.now;
        const keeper = this.sealKeeperAlive();
        EventBus.emit(GameEvents.LOG_MESSAGE, {
          text: t('zone.dungeon.sealedNag', { keeper: keeper ? getMonsterName(keeper.definition.id, keeper.definition.name) : '?' }),
          type: 'system',
        });
      }
      return;
    }
    if (cfg.isBossFloor) {
      this.finishDungeonRun('cleared');
      this.exitDungeon();
      return;
    }
    const options = DungeonSystem.rollBoonOffer(run.seed + cfg.floorNumber * 131, run.boons ?? {});
    if (options.length === 0) {
      this.advanceDungeonFloor();
      return;
    }
    this.dungeonChoosing = true;
    this.player.setPath([]);
    EventBus.emit(GameEvents.DUNGEON_BOON_OFFER, { floor: cfg.floorNumber, options, held: { ...(run.boons ?? {}) } });
  }

  private handleDungeonBoonChosen(data: { boonId: string }): void {
    const run = this.dungeonRunState;
    if (!this.dungeonChoosing || !run) return;
    this.dungeonChoosing = false;
    if (data?.boonId && BOONS.some(b => b.id === data.boonId)) {
      run.boons = { ...(run.boons ?? {}), [data.boonId]: (run.boons?.[data.boonId] ?? 0) + 1 };
      this.invalidateEquipStats();
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.dungeon.boonTaken', { boon: t(`dungeon.boon.${data.boonId}.name`) }), type: 'system' });
    }
    this.advanceDungeonFloor();
  }

  /** Record the run's result; the summary shows once back in the rift. */
  private finishDungeonRun(result: 'cleared' | 'fallen' | 'abandoned'): void {
    const run = this.dungeonRunState;
    const cfg = this.dungeonFloorConfig;
    if (!run || !cfg) return;
    const tier = run.tier ?? 1;
    const timeMs = Date.now() - (run.startedAt ?? Date.now());
    let newBest = false;
    if (result === 'cleared' && this.session) {
      const r = DungeonSystem.recordClear(this.session.abyss, tier, timeMs);
      this.session.abyss = r.record;
      newBest = r.newBest;
    }
    ZoneScene.pendingRunEnd = {
      result,
      tier,
      floorsCleared: result === 'cleared' ? run.totalFloors : cfg.floorNumber - 1,
      totalFloors: run.totalFloors,
      kills: run.kills ?? 0,
      timeMs,
      boons: { ...(run.boons ?? {}) },
      newBest,
      unlockedTier: this.session?.abyss.unlockedTier ?? tier,
    };
  }

  /** Curses that act every frame: regenerating monsters. */
  private updateDungeonCurse(delta: number): void {
    const curse = this.dungeonFloorConfig ? DungeonSystem.getCurse(this.dungeonFloorConfig) : null;
    if (!curse?.regenPerSec) return;
    this.dungeonRegenTimer += delta;
    if (this.dungeonRegenTimer < 500) return;
    const dt = this.dungeonRegenTimer / 1000;
    this.dungeonRegenTimer = 0;
    const now = this.time.now;
    for (const m of this.monsters) {
      if (m.isAlive() && now - m.lastDamagedAt > 3000) m.heal(m.maxHp * curse.regenPerSec * dt);
    }
  }

  /** Volatile curse: the corpse bursts after a short, visible fuse. */
  private volatileBurst(monster: Monster, fraction: number): void {
    const x = monster.sprite.x;
    const y = monster.sprite.y;
    const col = monster.tileCol;
    const row = monster.tileRow;
    const dmg = Math.round(monster.maxHp * fraction);
    const ring = this.add.ellipse(x, y, 40, 20, 0xff3a2a, 0.25).setStrokeStyle(2, 0xff6040, 0.9).setDepth(y + 5);
    this.tweens.add({ targets: ring, scaleX: 3.2, scaleY: 3.2, alpha: 0.55, duration: 650, ease: 'Quad.easeIn' });
    this.time.delayedCall(650, () => {
      ring.destroy();
      this.vfx?.deathBurst(x, y - 10, 0xff5030);
      if (this.player.hp <= 0) return;
      if (Math.hypot(this.player.tileCol - col, this.player.tileRow - row) > 2) return;
      this.player.hp = Math.max(0, this.player.hp - dmg);
      this.showDamageText(this.player.sprite.x, this.player.sprite.y, dmg, false, false, true, 'fire');
      EventBus.emit(GameEvents.COMBAT_DAMAGE, {
        targetId: 'player', damage: dmg, isDodged: false, isCrit: false, isPlayerTarget: true, targetMaxHP: this.player.maxHp,
      });
      if (this.player.hp <= 0) this.killPlayer();
    });
  }

  // ── Soul echo (death penalty) ──────────────────────────────

  /** Take the death's toll and leave it where the hero fell. */
  private applyDeathPenalty(): void {
    const echoes = this.session?.soulEcho;
    if (!echoes) return;
    const p = this.player;
    const toll = computeDeathPenalty({
      level: p.level, gold: p.gold, exp: p.exp, expToNext: p.expToNextLevel(), difficulty: this.difficulty,
    });
    p.gold -= toll.gold;
    p.exp -= toll.exp;
    if (toll.exp > 0) EventBus.emit(GameEvents.PLAYER_EXP_CHANGED, { exp: p.exp, needed: p.expToNextLevel() });
    // Dungeons are rebuilt on every run, so there is nowhere to come back to.
    if (this.isInDungeon || this.isInSubDungeon) {
      if (toll.gold > 0) EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.soulEcho.lostInDungeon', { gold: toll.gold }), type: 'system' });
      return;
    }
    const lost = echoes.leave({
      mapId: this.currentMapId, col: Math.round(p.tileCol), row: Math.round(p.tileRow), gold: toll.gold, exp: toll.exp,
    });
    if (lost) {
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.soulEcho.faded', { gold: lost.gold }), type: 'system' });
    }
    this.soulEchoVisual?.destroy();
    this.soulEchoVisual = null;
    if (echoes.echo) {
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.soulEcho.left', { gold: toll.gold }), type: 'system' });
      this.spawnSoulEchoVisual();
    }
  }

  /** The hero's pale double, kneeling where they fell. */
  private spawnSoulEchoVisual(): void {
    const e = this.session?.soulEcho.echo;
    if (!e || e.mapId !== this.currentMapId || this.isInDungeon || this.isInSubDungeon || this.soulEchoVisual) return;
    const w = cartToIso(e.col, e.row);
    const ring = this.add.image(0, 2, 'fx_glow').setTint(0x7fd8ff).setBlendMode(Phaser.BlendModes.ADD).setScale(0.9, 0.4).setAlpha(0.6);
    const key = `player_${this.player.classData.id}`;
    const ghost = this.textures.exists(key)
      ? this.add.image(0, -24, key, 0).setScale(1 / TEXTURE_SCALE).setTint(0x9fe6ff).setAlpha(0.55)
      : this.add.ellipse(0, -24, 22, 40, 0x9fe6ff, 0.5);
    const label = this.add.text(0, -70, t('zone.soulEcho.label', { gold: e.gold }), {
      fontSize: fs(12), color: '#bfefff', fontFamily: '"Noto Sans SC", sans-serif',
      stroke: '#08141c', strokeThickness: Math.round(3 * DPR),
    }).setOrigin(0.5);
    const c = this.add.container(w.x, w.y, [ring, ghost, label]).setDepth(w.y + 60);
    this.tweens.add({ targets: ghost, y: -30, alpha: 0.35, duration: 1400, yoyo: true, repeat: -1, ease: 'Sine.easeInOut' });
    this.tweens.add({ targets: ring, alpha: 0.25, scaleX: 1.15, duration: 1400, yoyo: true, repeat: -1, ease: 'Sine.easeInOut' });
    this.soulEchoVisual = c;
  }

  private checkSoulEchoClaim(): void {
    // The fallen hero lies on the echo until respawning; only the living reclaim it.
    if (this.player.hp <= 0) return;
    const claimed = this.session?.soulEcho.tryClaim(this.currentMapId, this.player.tileCol, this.player.tileRow);
    if (claimed) this.claimSoulEcho(claimed);
  }

  private claimSoulEcho(e: SoulEchoData): void {
    const c = this.soulEchoVisual;
    this.soulEchoVisual = null;
    this.player.gold += e.gold;
    if (e.exp > 0) this.player.addExp(e.exp);
    EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.soulEcho.claimed', { gold: e.gold }), type: 'loot' });
    audioManager.playSFX('resonance');
    if (c) {
      this.vfx?.deathBurst(c.x, c.y - 24, 0x7fd8ff);
      this.tweens.killTweensOf(c.list);
      this.tweens.add({ targets: c, alpha: 0, y: c.y - 30, duration: 500, onComplete: () => c.destroy() });
    }
    this.autoSave();
  }

  private handlePlayerLevelUp(data: { level: number }): void {
    this.showLevelUpBanner(data.level);
    // Check level achievements from ALL level-up sources (monster kills, quest rewards, dialogue exp, etc.)
    this.achievementSystem.checkLevel(data.level);
  }

  private handleQuestCompleted(data: { questId: string; questName: string }): void {
    const giver = questGiverOf(data.questId);
    const giverDef = giver ? NPCDefinitions[giver] : undefined;
    this.showQuestCompleteBanner(
      getQuestName(data.questId, data.questName),
      giverDef ? t('zone.quest.returnTo', { npc: getNpcName(giverDef.id, giverDef.name) }) : '',
    );
    this.updateNPCQuestMarkers();
  }

  /** Recent progress popups, so several in one moment stack instead of overlapping. */
  private questPopupSlots: number[] = [];

  /** Float "+1 Herb 3/5" (or "✓ …" when the objective is done) above the player. */
  private handleQuestProgress(data: { questId: string; objectiveIndex: number; current: number; required: number; completesQuest: boolean }): void {
    const quest = this.questSystem.quests.get(data.questId);
    const obj = quest?.objectives[data.objectiveIndex];
    if (!quest || !obj || !this.player?.sprite) return;
    const name = getQuestTargetName(obj.targetId, obj.targetName);
    const done = data.current >= data.required;
    // Kill counts only surface at milestones to avoid spamming every kill.
    if (obj.type === 'kill' && !done && data.required > 3 && data.current % Math.ceil(data.required / 4) !== 0) return;
    const text = done ? `✓ ${name}` : `${obj.type === 'kill' ? '' : '+1 '}${name}  ${data.current}/${data.required}`;
    const now = this.time.now;
    this.questPopupSlots = this.questPopupSlots.filter(t0 => now - t0 < 900);
    const slot = this.questPopupSlots.length;
    this.questPopupSlots.push(now);
    const popup = this.add.text(this.player.sprite.x, this.player.sprite.y - 70 - slot * 16, text, {
      fontSize: fs(done ? 14 : 12.5), color: done ? '#9dff8a' : '#ffe08a', fontFamily: '"Noto Sans SC", sans-serif',
      fontStyle: 'bold', stroke: '#1a0f04', strokeThickness: Math.round(3 * DPR),
    }).setOrigin(0.5).setDepth(ZONE_FLOATING_TEXT_DEPTH).setAlpha(0).setScale(0.8);
    this.tweens.add({ targets: popup, alpha: 1, scale: 1, duration: 160, ease: 'Back.easeOut' });
    this.tweens.add({
      targets: popup, y: popup.y - 26, alpha: 0, delay: done ? 1300 : 800, duration: 700, ease: 'Sine.easeIn',
      onComplete: () => popup.destroy(),
    });
  }

  private handleUiSkillClick(data: { index: number; skillId: string }): void {
    this.requestSkill(data.skillId, this.time.now);
  }

  private handleUiDodgeRequest(data?: { dx?: number; dy?: number }): void {
    this.performDodge(this.time.now, data?.dx, data?.dy);
  }

  private handleSkillLevelChanged(): void {
    this.refreshSkillLoadout();
    this.mobileControls?.refreshSkills();
  }

  update(time: number, delta: number): void {
    // A labyrinth panel (tier picker, boon cards) holds the world and its keys.
    if (this.storyDirector?.cinematic || this.dungeonChoosing || this.abyssModalOpen()) {
      this.holdMove = null;
      return;
    }
    if (this.isInDungeon) this.updateDungeonCurse(delta);
    if (this.emberTower) {
      this.emberTower.tick(delta);
      this.emberTower.update(time);
    }
    const recovery = this.getPlayerRecoveryModifiers();
    this.handleKeyboardMovement(delta);
    this.updateHoldMove();
    this.handleSkillInput(time);
    this.handleGamepadInput(time);
    this.consumeBufferedSkill(time);
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
        EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.combat.unyieldingProc'), type: 'combat' });
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

    if (this.simulationScheduler.due('active-monsters', time, 250)) {
      const nearby = this.monsterGrid.queryRadius(
        this.player.tileCol,
        this.player.tileRow,
        Math.sqrt(ZoneScene.MONSTER_AI_CULL_DIST_SQ),
      );
      const activeIds = new Set(nearby.map(monster => monster.id));
      this.activeMonsters = nearby;
      for (const monster of this.monsters) {
        if (monster.isAggro() && !activeIds.has(monster.id)) this.activeMonsters.push(monster);
      }
    }

    const safeRadius = this.mapData.safeZoneRadius ?? 9;
    const safeRadiusSq = safeRadius * safeRadius;
    const playerInSafe = this.campPositions.some(camp =>
      distanceSq(this.player.tileCol, this.player.tileRow, camp.col, camp.row) < safeRadiusSq
    );
    for (const monster of this.activeMonsters) {
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
        if (distanceSq(monster.tileCol, monster.tileRow, camp.col, camp.row) < safeRadiusSq) {
          monsterInSafe = true;
          break;
        }
      }
      if (monsterInSafe && monster.isAggro()) {
        monster.state = 'idle';
      }
      // Player in safe zone: suppress aggro by passing fake coordinates
      // Get speed multiplier from StatusEffectSystem (Slow effect)
      const speedMult = this.statusEffects.getSpeedMultiplier(monster.id);

      if (playerInSafe && !monster.isAggro()) {
        monster.update(time, delta, -999, -999, this.mapData.collisions, speedMult);
      } else {
        monster.update(time, delta, this.player.tileCol, this.player.tileRow, this.mapData.collisions, speedMult);
      }
      // Keep spatial index in sync after movement
      this.monsterGrid.update(monster);
    }

    // Update NPC state machines
    for (const npc of this.npcs) {
      npc.update(this.player.tileCol, this.player.tileRow);
    }

    this.handleCombat(time);
    this.handleMercenaryCombat(time);
    this.updateMercenary(time, delta);
    this.petCompanion?.update(time, delta);
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
      const distSq = distanceSq(this.player.tileCol, this.player.tileRow, pot.col, pot.row);
      if (distSq <= 4) {
        if (pot.type === 'hp') {
          this.player.hp = Math.min(this.player.maxHp, this.player.hp + pot.amount);
          EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.combat.restoreHp', { amount: pot.amount }), type: 'combat' });
        } else {
          this.player.mana = Math.min(this.player.maxMana, this.player.mana + pot.amount);
          EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.combat.restoreMana', { amount: pot.amount }), type: 'info' });
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

    this.collectOcclusionTargets();
    this.updateDecorOcclusion(delta);
    this.questWorld?.update(delta);
    if (this.soulEchoVisual) this.checkSoulEchoClaim();
    this.storyDirector?.update(delta);
    if (this.terrain) {
      if (this.terrain.hasPending()) {
        this.terrain.flush(4, (c, r) => !!this.tileSprites[r]?.[c], (c, r, key) => { this.tileSprites[r]?.[c]?.setTexture(key); });
      }
      this.terrain.updateOcclusion(this.occlusionTargets, delta);
    }

    // Throttled viewport tile update
    if (this.simulationScheduler.due('world-visibility', time, 100)) {
      this.lastTileUpdate = time;
      this.updateVisibleTiles();
    }

    // Throttled explore quest check + NPC quest marker update
    if (this.simulationScheduler.due('quest-observers', time, 500)) {
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

    // Only emit HP/mana events when values actually change
    if (this.player.hp !== this._lastEmittedHp || this.player.maxHp !== this._lastEmittedMaxHp) {
      this._lastEmittedHp = this.player.hp;
      this._lastEmittedMaxHp = this.player.maxHp;
      EventBus.emit(GameEvents.PLAYER_HEALTH_CHANGED, { hp: this.player.hp, maxHp: this.player.maxHp });
    }
    if (this.player.mana !== this._lastEmittedMana || this.player.maxMana !== this._lastEmittedMaxMana) {
      this._lastEmittedMana = this.player.mana;
      this._lastEmittedMaxMana = this.player.maxMana;
      EventBus.emit(GameEvents.PLAYER_MANA_CHANGED, { mana: this.player.mana, maxMana: this.player.maxMana });
    }
  }

  private getPlayerRecoveryModifiers(): { hpRegenMultiplier?: number; manaRegenMultiplier?: number } {
    for (const camp of this.campPositions) {
      if (distanceSq(this.player.tileCol, this.player.tileRow, camp.col, camp.row) <= CAMPFIRE_RECOVERY_RADIUS_SQ) {
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
    const { minCol, maxCol, minRow, maxRow, left, right, top, bottom } = this.getVisibleTileBounds(margin);
    const boundsKey = `${minCol}:${maxCol}:${minRow}:${maxRow}`;
    if (boundsKey === this.lastVisibleTileBounds) return;
    this.lastVisibleTileBounds = boundsKey;
    const newVisible = new Set<number>();
    if (this.terrain && this.player) {
      this.terrain.beginPass(Math.round(this.player.tileCol), Math.round(this.player.tileRow), 2);
    }

    for (let row = minRow; row <= maxRow; row++) {
      for (let col = minCol; col <= maxCol; col++) {
        const pos = this.tileWorldPositions[row][col];
        // The cart-space bounds cover about twice the iso view; skip tiles outside
        // the expanded world rect (extra room below for tall wall overlays).
        if (pos.x < left - 32 || pos.x > right + 32 || pos.y < top - 16 || pos.y > bottom + 64) continue;
        const tileIndex = row * this.mapData.cols + col;
        const exitKey = `${col},${row}`;
        newVisible.add(tileIndex);
        if (!this.tileSprites[row][col]) {
          let tileKey: string;
          if (this.terrain) {
            tileKey = this.terrain.groundKey(col, row);
            this.terrain.showOverlay(col, row, pos.x, pos.y);
          } else {
            const tileType = this.mapData.tiles[row][col];
            const variant = ((col * 374761393 + row * 668265263) >>> 0) % SpriteGenerator.TILE_VARIANTS;
            const variantKey = `${TILE_KEYS[tileType] || 'tile_grass'}_${variant}`;
            tileKey = this.textures.exists(variantKey) ? variantKey : (TILE_KEYS[tileType] || 'tile_grass');
          }
          const tile = this.acquireTileImage(pos.x, pos.y, tileKey);
          if (this.terrain) tile.setScale(this.terrain.tileScale(tileKey));
          // Depth batching: use row-based depth so all tiles in the same row
          // share the same depth value, reducing Phaser's depth sort overhead.
          tile.setDepth(row);
          this.tileSprites[row][col] = tile;

          const exit = this.exitLookup.get(exitKey);
          if (exit) {
            SpriteGenerator.ensureEffect(this, 'exit_portal');
            if (this.textures.exists('exit_portal')) {
              const portal = this.add.image(pos.x, pos.y - 8, 'exit_portal').setScale(1 / TEXTURE_SCALE);
              portal.setDepth(pos.y + 2);
              this.exitSprites.set(exitKey, portal);
              if (this.isInDungeon && !this.dungeonSealOpen) portal.setTint(0x6a2230).setAlpha(0.75);
              if (this.vfx) {
                this.vfx.applyGlow(portal, 0x4488ff, 8, 0.1);
                this.vfx.applyBloom(portal, 0.8);
              }
              // Add floor label for dungeon exit portals
              if (this.isInDungeon && this.dungeonFloorConfig) {
                const labelText = !this.dungeonSealOpen
                  ? t('zone.dungeon.exitSealed')
                  : this.dungeonFloorConfig.isBossFloor
                    ? t('zone.returnToAbyssRift')
                    : DungeonSystem.getFloorExitLabel(this.dungeonFloorConfig.floorNumber + 1);
                const exitLabel = this.add.text(pos.x, pos.y - 30 * DPR, labelText, {
                  fontSize: fs(9),
                  color: this.dungeonFloorConfig.isBossFloor ? '#66CCFF' : '#FF9933',
                  fontFamily: '"Noto Sans SC", sans-serif',
                  stroke: '#000000',
                  strokeThickness: Math.round(2 * DPR),
                }).setOrigin(0.5).setDepth(pos.y + 3);
                this.exitLabels.set(exitKey, exitLabel);
              }
            }
          }
        }
      }
    }

    // Destroy tiles no longer visible
    for (const tileIndex of this.visibleTiles) {
      if (!newVisible.has(tileIndex)) {
        const r = Math.floor(tileIndex / this.mapData.cols);
        const c = tileIndex % this.mapData.cols;
        const exitKey = `${c},${r}`;
        const sprite = this.tileSprites[r]?.[c];
        if (sprite) {
          this.releaseTileImage(sprite);
          this.tileSprites[r][c] = null;
        }
        this.terrain?.hideOverlay(c, r);
        const exitSprite = this.exitSprites.get(exitKey);
        if (exitSprite) {
          exitSprite.destroy();
          this.exitSprites.delete(exitKey);
        }
        const exitLabel = this.exitLabels.get(exitKey);
        if (exitLabel) {
          exitLabel.destroy();
          this.exitLabels.delete(exitKey);
        }
      }
    }
    this.visibleTiles = newVisible;

    // Update decorations visibility
    this.updateVisibleDecorations();
    this.updateCampDecorations();
  }

  private acquireTileImage(x: number, y: number, texture: string): Phaser.GameObjects.Image {
    const tile = this.tilePool.pop();
    if (tile) {
      return tile.setTexture(texture).setPosition(x, y).setScale(1 / TEXTURE_SCALE).setVisible(true).setActive(true);
    }
    return this.add.image(x, y, texture).setScale(1 / TEXTURE_SCALE);
  }

  private releaseTileImage(tile: Phaser.GameObjects.Image): void {
    tile.setVisible(false).setActive(false);
    this.tilePool.push(tile);
  }

  private updateVisibleDecorations(): void {
    const { left, right, top, bottom } = this.getExpandedWorldBounds(TILE_WIDTH * 5, TILE_HEIGHT * 7);
    const visibleDecorKeys = new Set<string>();

    for (let i = 0; i < this.decorWorldPositions.length; i++) {
      const decor = this.decorWorldPositions[i];
      if (decor.x < left || decor.x > right || decor.y < top || decor.y > bottom) continue;

      visibleDecorKeys.add(decor.key);
      if (!this.decorSprites.has(decor.key)) {
        const texKey = `decor_${decor.type}`;
        SpriteGenerator.ensureDecoration(this, decor.type);
        if (this.textures.exists(texKey)) {
          const sprite = this.placeDecorSprite(decor.x, decor.y, texKey, i + 1);
          this.decorSprites.set(decor.key, sprite);
        }
      }
    }

    // Remove out-of-view decorations
    for (const [key, sprite] of this.decorSprites) {
      if (!visibleDecorKeys.has(key)) {
        this.occluderDecor.delete(sprite);
        sprite.destroy();
        this.decorSprites.delete(key);
      }
    }
  }

  /**
   * Place a decoration so its base sits on the tile: origin at the drawer's
   * ground line, small deterministic jitter/scale for variety, and depth by
   * layer (flat ground cover under everything, upright props sorted with
   * characters by their base).
   */
  private placeDecorSprite(x: number, y: number, texKey: string, seed: number): Phaser.GameObjects.Image {
    const meta = SpriteGenerator.getDecorMeta(texKey);
    const h = ((seed * 2654435761) >>> 0) / 4294967296;
    const h2 = ((seed * 1597334677 + 12345) >>> 0) / 4294967296;
    const jx = meta ? (h - 0.5) * 18 : 0;
    const jy = meta ? (h2 - 0.5) * 8 : -6;
    const scale = meta ? 0.9 + ((h + h2) % 1) * 0.2 : 1;
    const sprite = this.add.image(x + jx, y + jy, texKey)
      .setOrigin(0.5, meta ? meta.anchorY : 0.5)
      .setScale(scale / TEXTURE_SCALE);
    if (!meta) {
      sprite.setDepth(y + 20);
    } else if (meta.flat) {
      sprite.setDepth(y + jy + 5);
    } else {
      // Player depth is y+100, monsters y+50: +70 keeps a character one tile
      // in front drawn over the prop and one tile behind drawn under it.
      sprite.setDepth(y + jy + 70);
      if (meta.tall) this.occluderDecor.add(sprite);
    }
    return sprite;
  }

  /** Reused scratch list of occlusion targets (player + nearby living monsters). */
  private occlusionTargets: number[] = [];

  /** Refresh `occlusionTargets`: the player plus living monsters within ~6 tiles. */
  private collectOcclusionTargets(): void {
    const targets = this.occlusionTargets;
    targets.length = 0;
    if (!this.player?.sprite) return;
    const px = this.player.sprite.x;
    const py = this.player.sprite.y;
    targets.push(px, py);
    // iso: 6 tiles ≈ 384 × 192 px.
    for (const m of this.monsters) {
      if (!m.isAlive() || !m.sprite) continue;
      const mx = m.sprite.x;
      const my = m.sprite.y;
      if (Math.abs(mx - px) < 384 && Math.abs(my - py) < 192) targets.push(mx, my);
    }
  }

  /**
   * Fade tall props that hide the player, or a living monster near the player,
   * standing behind them; restore smoothly when clear.
   */
  private updateDecorOcclusion(delta: number): void {
    if (this.occluderDecor.size === 0) return;
    const targets = this.occlusionTargets;
    const k = Math.min(1, delta / 110);
    for (const sprite of this.occluderDecor) {
      const halfW = sprite.displayWidth * 0.42;
      const baseY = sprite.y;
      const topY = baseY - sprite.displayHeight * sprite.originY;
      let target = 1;
      for (let i = 0; i < targets.length; i += 2) {
        const tx = targets[i];
        const ty = targets[i + 1];
        // Behind the prop (depth-sorted under it) and the body (~60px above the
        // feet) overlaps the sprite's bounds.
        if (Math.abs(sprite.x - tx) < halfW && ty < baseY - 28 && ty > topY + 12) {
          target = 0.25;
          break;
        }
      }
      if (sprite.alpha !== target) {
        const a = sprite.alpha + (target - sprite.alpha) * k;
        sprite.setAlpha(Math.abs(a - target) < 0.02 ? target : a);
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
      const ctx = c.getContext('2d', { willReadFrequently: true })!;
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
          y: pos.y - 40,
          radius: 70,
          color: 0xff6600,
          intensity: 0.65,
          flicker: true,
          id: `torch_${decor.col}_${decor.row}`,
        });
      }
    }
  }

  private updateCampDecorations(): void {
    const { left, right, top, bottom } = this.getExpandedWorldBounds(TILE_WIDTH * 4, TILE_HEIGHT * 6);
    const visibleKeys = new Set<string>();
    const theme = this.mapData.theme;

    for (const decor of this.campDecorWorldPositions) {
      if (decor.x < left || decor.x > right || decor.y < top || decor.y > bottom) continue;

      const key = decor.key;
      visibleKeys.add(key);
      if (this.campDecorSprites.has(key)) continue;

      const texKey = SpriteGenerator.ensureCampDecoration(this, decor.type, theme);
      if (!this.textures.exists(texKey)) continue;

      const meta = SpriteGenerator.getDecorMeta(texKey);
      const sprite = this.add.image(decor.x, decor.y, texKey)
        .setOrigin(0.5, meta ? meta.anchorY : 0.5)
        .setScale(1 / TEXTURE_SCALE);
      sprite.setDepth(meta?.flat ? decor.y + 5 : decor.y + 70);
      if (meta?.tall && decor.type === 'tent') this.occluderDecor.add(sprite);
      this.campDecorSprites.set(key, sprite);

      if (decor.type === 'torch' || decor.type === 'campfire') {
        const isFire = decor.type === 'campfire';
        const flameKey = SpriteGenerator.ensureCampDecoration(this, 'flame', theme);
        const flameY = isFire ? decor.y - 6 : decor.y - 62;
        const flameScale = isFire ? 1 : 0.5;
        const flameColor = this.getCampFlameColor();
        // Warm pulsing glow behind the flame.
        const glow = this.add.circle(decor.x, flameY - (isFire ? 12 : 6), isFire ? 46 : 18, flameColor, isFire ? 0.12 : 0.14);
        glow.setBlendMode(Phaser.BlendModes.ADD);
        glow.setDepth(decor.y + 69);
        this.tweens.add({
          targets: glow,
          alpha: { from: isFire ? 0.08 : 0.1, to: isFire ? 0.18 : 0.22 },
          scaleX: { from: 0.9, to: 1.1 }, scaleY: { from: 0.9, to: 1.1 },
          duration: isFire ? 700 : 450 + Math.random() * 200, yoyo: true, repeat: -1, ease: 'Sine.easeInOut',
        });
        this.campDecorSprites.set(`${key}|glow`, glow);
        if (this.textures.exists(flameKey)) {
          const meta2 = SpriteGenerator.getDecorMeta(flameKey);
          const flame = this.add.sprite(decor.x, flameY, flameKey)
            .setOrigin(0.5, meta2 ? meta2.anchorY : 0.9)
            .setScale(flameScale / TEXTURE_SCALE)
            .setDepth(decor.y + 71);
          if (this.anims.exists(`${flameKey}_anim`)) {
            flame.play({ key: `${flameKey}_anim`, startFrame: Math.floor(Math.random() * 6) });
          }
          // Cheap flicker on top of the frame animation.
          this.tweens.add({
            targets: flame,
            scaleY: { from: flameScale / TEXTURE_SCALE * 0.94, to: flameScale / TEXTURE_SCALE * 1.08 },
            duration: 180 + Math.random() * 120, yoyo: true, repeat: -1, ease: 'Sine.easeInOut',
          });
          this.campDecorSprites.set(`${key}|flame`, flame);
        }
      }
      if (decor.type === 'campfire') {
        // Sparks drifting up from the fire.
        const sparkEmitter = this.add.particles(decor.x, decor.y - 14, 'particle_circle', {
          speed: { min: 15, max: 45 },
          angle: { min: 245, max: 295 },
          scale: { start: 0.3, end: 0 },
          alpha: { start: 1, end: 0 },
          lifespan: { min: 400, max: 900 },
          frequency: 180,
          tint: [0xffdd44, 0xff8800],
          blendMode: Phaser.BlendModes.ADD,
          emitting: true,
        });
        sparkEmitter.setDepth(decor.y + 72);
        this.campParticles.set(`${key}|spark`, sparkEmitter);
      }
      // Banner sway (pivot at the pole base)
      if (decor.type === 'banner') {
        this.tweens.add({
          targets: sprite,
          angle: { from: -1.5, to: 1.5 },
          duration: 1500 + Math.random() * 500,
          yoyo: true, repeat: -1, ease: 'Sine.easeInOut',
        });
      }
    }

    for (const [key, sprite] of this.campDecorSprites) {
      if (!visibleKeys.has(key.split('|')[0])) {
        this.occluderDecor.delete(sprite as Phaser.GameObjects.Image);
        sprite.destroy();
        this.campDecorSprites.delete(key);
      }
    }
    for (const [key, emitter] of this.campParticles) {
      if (!visibleKeys.has(key.split('|')[0])) {
        emitter.destroy();
        this.campParticles.delete(key);
      }
    }
  }

  private getCampFlameColor(): number {
    const theme = this.mapData.theme;
    return theme ? CAMP_THEMES[theme]?.torchFlame ?? 0xff8800 : 0xff8800;
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

    // Standard gamepad left stick, converted from screen-space to tile-space.
    if (dx === 0 && dy === 0) {
      const pad = this.input.gamepad?.getPad(0);
      const stickX = pad?.leftStick?.x ?? 0;
      const stickY = pad?.leftStick?.y ?? 0;
      if (Math.hypot(stickX, stickY) > 0.18) {
        dx = stickX + stickY;
        dy = -stickX + stickY;
      }
    }

    if (dx !== 0 || dy !== 0) {
      this.holdMove = null;
      this.player.path = [];
      const speed = this.player.moveSpeed * (delta / 1000) * 0.015;
      const len = Math.sqrt(dx * dx + dy * dy);
      this.lastMoveDirection = { dx: dx / len, dy: dy / len };
      const newCol = this.player.tileCol + (dx / len) * speed;
      const newRow = this.player.tileRow + (dy / len) * speed;
      const checkCol = Math.round(newCol), checkRow = Math.round(newRow);
      if (checkCol >= 0 && checkCol < this.mapData.cols && checkRow >= 0 && checkRow < this.mapData.rows && this.mapData.collisions[checkRow][checkCol]) {
        this.player.moveDirect(newCol, newRow);
      }
    }
  }

  private handleSkillInput(time: number): void {
    if (!this.wasd) return;
    if (Phaser.Input.Keyboard.JustDown(this.wasd.TAB)) {
      this.player.autoCombat = !this.player.autoCombat;
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.combat.autoCombat', { state: this.player.autoCombat ? t('zone.combat.autoCombatOn') : t('zone.combat.autoCombatOff') }), type: 'system' });
    }
    if (Phaser.Input.Keyboard.JustDown(this.wasd.Q)) {
      this.cycleCombatTarget();
    }
    if (Phaser.Input.Keyboard.JustDown(this.wasd.SPACE)) {
      this.performDodge(time);
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
      EventBus.emit(GameEvents.UI_TOGGLE_PANEL, { panel: 'pets' });
    }
    if (Phaser.Input.Keyboard.JustDown(this.wasd.U)) {
      EventBus.emit(GameEvents.UI_TOGGLE_PANEL, { panel: 'companion' });
    }
    if (Phaser.Input.Keyboard.JustDown(this.wasd.V)) {
      EventBus.emit(GameEvents.UI_TOGGLE_PANEL, { panel: 'achievement' });
    }
    if (Phaser.Input.Keyboard.JustDown(this.wasd.ESC)) {
      this.returnToMenu();
    }

    const skillKeys = [this.wasd.ONE, this.wasd.TWO, this.wasd.THREE, this.wasd.FOUR, this.wasd.FIVE, this.wasd.SIX];
    const skills = this.getSkillLoadout();
    for (let i = 0; i < Math.min(skillKeys.length, skills.length); i++) {
      if (Phaser.Input.Keyboard.JustDown(skillKeys[i])) this.requestSkill(skills[i].id, time);
    }
  }

  private getSkillLoadout(): ClassDefinition['skills'] {
    return this.skillLoadout;
  }

  private refreshSkillLoadout(): void {
    this.skillLoadout = getLearnedSkillLoadout(
      this.player.classData.skills,
      this.player.skillLevels,
    );
  }

  private handleGamepadInput(time: number): void {
    const pad = this.input.gamepad?.getPad(0);
    if (!pad) return;

    const dodgePressed = pad.buttons?.[1]?.pressed ?? false;
    if (dodgePressed && !this.gamepadButtonState.dodge) this.performDodge(time);
    this.gamepadButtonState.dodge = dodgePressed;

    const targetPressed = pad.buttons?.[4]?.pressed ?? false;
    if (targetPressed && !this.gamepadButtonState.target) this.cycleCombatTarget();
    this.gamepadButtonState.target = targetPressed;

    const skillButtonIndices = [0, 2, 3, 5];
    const skills = this.getSkillLoadout();
    for (let i = 0; i < skillButtonIndices.length; i++) {
      const pressed = pad.buttons?.[skillButtonIndices[i]]?.pressed ?? false;
      if (pressed && !this.gamepadButtonState.skills[i] && skills[i]) {
        this.requestSkill(skills[i].id, time);
      }
      this.gamepadButtonState.skills[i] = pressed;
    }
  }

  private canExecuteSkill(skillId: string, time: number): boolean {
    if (this.player.hp <= 0) return false;
    const skill = this.player.getSkill(skillId);
    if (!skill) return false;
    const level = this.player.getSkillLevel(skillId);
    if (level <= 0) return false;
    if (!this.player.isSkillReady(skillId, time)) return false;
    if (this.player.mana < this.player.getSkillManaCost(skillId, level)) return false;
    if (skillId === 'teleport' && this.statusEffects.isImmobilized('player')) return false;

    const target = this.findPreferredSkillTarget();
    if (!target) return !!skill.buff || !!skill.aoe || skillId === 'teleport';
    if (skill.buff || skill.aoe || skillId === 'teleport') return true;
    return distanceSq(
      this.player.tileCol,
      this.player.tileRow,
      target.tileCol,
      target.tileRow,
    ) <= (skill.range + 1) * (skill.range + 1);
  }

  private requestSkill(skillId: string, time: number): void {
    const skill = this.player.getSkill(skillId);
    if (!skill || this.player.getSkillLevel(skillId) <= 0) {
      EventBus.emit(GameEvents.LOG_MESSAGE, {
        text: t('zone.combat.skillLocked'),
        type: 'combat',
      });
      return;
    }

    const decision = this.combatInput.request(
      skillId,
      time,
      this.canExecuteSkill(skillId, time),
    );
    if (decision === 'execute') {
      this.tryUseSkill(skillId, time);
      return;
    }

    const buffered = this.combatInput.peek();
    EventBus.emit(GameEvents.SKILL_BUFFERED, {
      skillId,
      expiresAt: buffered?.expiresAt ?? time,
    });
  }

  private consumeBufferedSkill(time: number): void {
    const buffered = this.combatInput.consumeReady(
      time,
      skillId => this.canExecuteSkill(skillId, time),
    );
    if (buffered) this.tryUseSkill(buffered.actionId, time);
  }

  /** Where a ground-targeted AoE lands: the chosen target if in reach, else the nearest enemy in range. */
  private findGroundAoeAnchor(target: Monster | null, range: number): Monster | null {
    const reach = (range + 1) * (range + 1);
    const inReach = (m: Monster): boolean =>
      distanceSq(this.player.tileCol, this.player.tileRow, m.tileCol, m.tileRow) <= reach;
    if (target && target.isAlive() && inReach(target)) return target;
    let best: Monster | null = null;
    let bestD = Infinity;
    for (const m of this.monsterGrid.queryRadius(this.player.tileCol, this.player.tileRow, range + 1)) {
      if (!m.isAlive()) continue;
      const d = distanceSq(this.player.tileCol, this.player.tileRow, m.tileCol, m.tileRow);
      if (d <= reach && d < bestD) { best = m; bestD = d; }
    }
    return best;
  }

  /** Living monsters within `halfWidth` tiles of the player→target ray, out to `range` tiles. */
  private monstersAlongLine(target: Monster, range: number, halfWidth: number): Monster[] {
    const ox = this.player.tileCol, oy = this.player.tileRow;
    let dx = target.tileCol - ox, dy = target.tileRow - oy;
    const len = Math.hypot(dx, dy);
    if (len < 0.001) return target.isAlive() ? [target] : [];
    dx /= len; dy /= len;
    return this.monsterGrid.queryRadius(ox, oy, range + halfWidth).filter(m => {
      if (!m.isAlive()) return false;
      const rx = m.tileCol - ox, ry = m.tileRow - oy;
      const along = rx * dx + ry * dy;
      return along >= 0 && along <= range && Math.abs(rx * dy - ry * dx) <= halfWidth;
    });
  }

  private findPreferredSkillTarget(): Monster | null {
    if (this.player.attackTarget) {
      const selected = this.monsters.find(
        monster => monster.id === this.player.attackTarget && monster.isAlive(),
      );
      if (selected) return selected;
      this.player.attackTarget = null;
    }
    return this.findNearestAliveMonster();
  }

  private cycleCombatTarget(): void {
    const targetId = cycleTargetId(
      this.player.attackTarget,
      this.monsters.map(monster => ({
        id: monster.id,
        alive: monster.isAlive(),
        distanceSq: distanceSq(
          this.player.tileCol,
          this.player.tileRow,
          monster.tileCol,
          monster.tileRow,
        ),
      })),
      14,
    );
    this.player.attackTarget = targetId;
    const target = targetId
      ? this.monsters.find(monster => monster.id === targetId) ?? null
      : null;
    EventBus.emit(GameEvents.TARGET_CHANGED, {
      targetId,
      targetName: target
        ? getMonsterName(target.definition.id, target.definition.name)
        : null,
    });
  }

  getDodgeCooldownRemaining(): number {
    return this.dodgeController.cooldownRemaining(this.time.now);
  }

  private performDodge(time: number, requestedDx?: number, requestedDy?: number): void {
    if (
      this.player.hp <= 0
      || !this.dodgeController.isReady(time)
      || this.statusEffects.isImmobilized('player')
    ) return;

    let dx = requestedDx ?? this.lastMoveDirection.dx;
    let dy = requestedDy ?? this.lastMoveDirection.dy;
    const length = Math.hypot(dx, dy);
    if (length <= 0.001) {
      dx = 1;
      dy = -1;
    } else {
      dx /= length;
      dy /= length;
    }

    const dodgeDistance = this.player.classData.id === 'rogue' ? 2.6
      : this.player.classData.id === 'mage' ? 2.25
        : 1.8;
    let destination: { col: number; row: number } | null = null;
    for (let distance = dodgeDistance; distance >= 0.5; distance -= 0.25) {
      const col = this.player.tileCol + dx * distance;
      const row = this.player.tileRow + dy * distance;
      const checkCol = Math.round(col);
      const checkRow = Math.round(row);
      if (
        checkCol >= 0
        && checkCol < this.mapData.cols
        && checkRow >= 0
        && checkRow < this.mapData.rows
        && this.mapData.collisions[checkRow]?.[checkCol]
      ) {
        destination = { col, row };
        break;
      }
    }
    if (!destination || !this.dodgeController.tryStart(time)) return;

    const originX = this.player.sprite.x;
    const originY = this.player.sprite.y;
    this.player.path = [];
    this.player.moveTo(destination.col, destination.row);
    const screenDx = this.player.sprite.x - originX;
    const screenDy = this.player.sprite.y - originY;
    this.player.playDodge(screenDx, screenDy);
    this.spawnDodgeAfterimages(originX, originY, this.player.sprite.x, this.player.sprite.y);

    EventBus.emit(GameEvents.DODGE_STARTED, {
      cooldownMs: this.dodgeController.config.cooldownMs,
      invulnerabilityMs: this.dodgeController.config.invulnerabilityMs,
    });
  }

  private spawnDodgeAfterimages(
    originX: number,
    originY: number,
    destinationX: number,
    destinationY: number,
  ): void {
    const source = this.player.sprite.list.find(
      child => child instanceof Phaser.GameObjects.Sprite,
    );
    if (!(source instanceof Phaser.GameObjects.Sprite)) return;

    for (let i = 0; i < 2; i++) {
      const ratio = (i + 1) / 3;
      this.trails.stampGhost(
        Phaser.Math.Linear(originX, destinationX, ratio),
        Phaser.Math.Linear(originY, destinationY, ratio) + source.y,
        source.texture.key,
        {
          frame: source.frame.name,
          alpha: 0.28 - i * 0.07,
          tint: this.player.spirit.profile.visualColor,
          scaleX: source.scaleX * (1 + i * 0.04),
          scaleY: source.scaleY * (1 - i * 0.05),
          flipX: source.flipX,
          angle: source.angle,
        },
      );
    }
  }

  private tryUseSkill(skillId: string, time: number): void {
    if (this.player.hp <= 0) return;
    const skill = this.player.getSkill(skillId);
    if (!skill) return;
    if (this.player.getSkillLevel(skillId) <= 0) {
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.combat.skillLocked'), type: 'combat' });
      return;
    }
    if (!this.player.isSkillReady(skillId, time)) return;
    const level = this.player.getSkillLevel(skillId);
    const scaledManaCost = this.player.getSkillManaCost(skillId, level);
    if (this.player.mana < scaledManaCost) {
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.combat.manaInsufficient'), type: 'combat' });
      return;
    }

    if (skillId === 'teleport' && this.statusEffects.isImmobilized('player')) {
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.teleport.blockedByCC'), type: 'combat' });
      return;
    }

    const target = this.findPreferredSkillTarget();
    if (!target && !skill.buff && !skill.aoe && skillId !== 'teleport') return;

    if (target && !skill.buff && skillId !== 'teleport') {
      const dSq = distanceSq(this.player.tileCol, this.player.tileRow, target.tileCol, target.tileRow);
      if (!skill.aoe && dSq > (skill.range + 1) * (skill.range + 1)) return;
    }

    this.player.useSkill(skillId, time, level, this.getEquipStats().cooldownReduction);

    // freeCast: X% chance to not consume mana when casting a skill
    const eqFc = this.getEquipStats();
    if (eqFc.freeCast > 0 && this.combatSystem.checkFreeCast(eqFc.freeCast)) {
      // Refund the mana that was just spent
      this.player.mana = Math.min(this.player.maxMana, this.player.mana + scaledManaCost);
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.combat.freeCast'), type: 'combat' });
      EventBus.emit(GameEvents.PLAYER_MANA_CHANGED, { mana: this.player.mana, maxMana: this.player.maxMana });
    }

    let releaseDelay: number;
    if (skill.buff || skill.aoe || skill.range > 2) {
      releaseDelay = target && !skill.buff
        ? this.player.playCast(target.sprite.x, target.sprite.y)
        : this.player.playCast();
    } else {
      const animTarget = this.findPreferredSkillTarget();
      if (animTarget) {
        releaseDelay = this.player.playAttack(animTarget.sprite.x, animTarget.sprite.y);
      } else {
        releaseDelay = this.player.playCast();
      }
    }

    // Blinks stay instant so they work as escapes; everything else resolves
    // on the animation's release/contact beat so VFX and damage land together.
    if (skillId === 'teleport' || skillId === 'shadow_step' || releaseDelay <= 0) {
      this.releaseSkill(skillId, skill, level, target, time, scaledManaCost);
      return;
    }
    this.time.delayedCall(releaseDelay, () => {
      if (this.player.hp <= 0 || this.isTransitioning) return;
      // The original target may have died during the wind-up; retarget.
      const liveTarget = target && target.isAlive() ? target : this.findPreferredSkillTarget();
      this.releaseSkill(skillId, skill, level, liveTarget, this.time.now, scaledManaCost);
    });
  }

  private releaseSkill(
    skillId: string,
    skill: SkillDefinition,
    level: number,
    target: Monster | null,
    time: number,
    scaledManaCost: number,
  ): void {
    // ── Teleport: instant reposition to walkable tile near target ──
    if (skillId === 'teleport') {
      const pointer = this.input.activePointer;
      let tile = worldToTile(pointer.worldX, pointer.worldY);
      // Cast from a touch button: the finger is on the button, not the destination — blink
      // along the joystick direction, else to the current target.
      if (this.mobileControls?.claimsPointer(pointer)) {
        const dir = this.mobileControls.getDirection();
        const len = Math.hypot(dir.dx, dir.dy);
        tile = len > 0.2
          ? { col: this.player.tileCol + (dir.dx / len) * 6, row: this.player.tileRow + (dir.dy / len) * 6 }
          : target ? { col: target.tileCol, row: target.tileRow } : { col: this.player.tileCol, row: this.player.tileRow };
      }
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
          EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.teleport.unreachable'), type: 'combat' });
          // Refund the Spirit-adjusted mana since teleport was already deducted
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
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.combat.skillActivated', { skillName: getSkillName(skillId, skill.name) }), type: 'combat' });
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
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.combat.skillActivated', { skillName: getSkillName(skillId, skill.name) }), type: 'combat' });
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
        target.takeDamage(result.damage, this.player.sprite.x, this.player.sprite.y, { isCrit: result.isCrit });
        this.applySteal(result);
        this.showDamageText(target.sprite.x, target.sprite.y, result.damage, result.isCrit, false, false, skill.damageType);
        if (!target.isAlive()) this.onMonsterKilled(target);
      }
      this.skillEffects.play(skillId, this.player.sprite.x, this.player.sprite.y, target.sprite.x, target.sprite.y);
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.combat.deathMarkApplied', { skillName: getSkillName(skillId, skill.name), targetName: getMonsterName(target.definition.id, target.definition.name) }), type: 'combat' });
      return;
    }

    // ── Slow Trap: apply slow status effect to enemies in AoE, not buff to player ──
    if (skillId === 'slow_trap') {
      const scaledRadius = getSkillAoeRadius(skill, level);
      const aoeTargets = this.monsterGrid.queryRadius(this.player.tileCol, this.player.tileRow, scaledRadius)
        .filter(m => m.isAlive());
      // Apply damage
      for (const t of aoeTargets) {
        if (skill.damageMultiplier > 0) {
          const result = this.combatSystem.calculateDamage(this.player.toCombatEntity(this.getEquipStats()), t.toCombatEntity(), skill, level, this.player.skillLevels);
          t.takeDamage(result.damage, this.player.sprite.x, this.player.sprite.y, { isCrit: result.isCrit });
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
        EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.combat.slowTrapHit', { skillName: getSkillName(skillId, skill.name), count: aoeTargets.length }), type: 'combat' });
      }
      return;
    }

    if (skill.buff) {
      const buffValue = getSkillBuffValue(skill, level);
      const buffDuration = getSkillBuffDuration(skill, level);
      this.player.buffs.push({ stat: skill.buff.stat, value: buffValue, duration: buffDuration, startTime: time });
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.combat.skillActivated', { skillName: getSkillName(skillId, skill.name) }), type: 'combat' });
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
        const tauntTargets = this.monsterGrid.queryRadius(this.player.tileCol, this.player.tileRow, scaledTauntRadius)
          .filter(m => m.isAlive());
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
            text: t('zone.combat.tauntRoar', { count: tauntTargets.length }),
            type: 'combat',
          });
        }
      }

      return;
    }

    const scaledAoeRadius = getSkillAoeRadius(skill, level);
    if (skill.aoe && scaledAoeRadius > 0) {
      const px = this.player.sprite.x, py = this.player.sprite.y;
      // Ground-targeted spells land on the enemy (within cast range) rather
      // than at the caster's feet; everything else stays caster-centred.
      const anchor = GROUND_AOE_SKILLS.has(skillId) ? this.findGroundAoeAnchor(target, skill.range) : null;
      const cx = anchor ? anchor.sprite.x : px;
      const cy = anchor ? anchor.sprite.y : py;
      const aoeTargets = skillId === 'piercing_arrow' && target
        ? this.monstersAlongLine(target, skill.range, scaledAoeRadius * 0.6)
        : this.monsterGrid.queryRadius(anchor ? anchor.tileCol : this.player.tileCol, anchor ? anchor.tileRow : this.player.tileRow, scaledAoeRadius)
          .filter(m => m.isAlive());
      const targetPoints = aoeTargets.map(t => ({ x: t.sprite.x, y: t.sprite.y }));
      if (skillId === 'chain_lightning' || skillId === 'multishot') {
        this.skillEffects.play(skillId, px, py, undefined, undefined, targetPoints);
      } else if (skillId === 'piercing_arrow') {
        this.skillEffects.play(skillId, px, py, target?.sprite.x, target?.sprite.y,
          target ? [{ x: target.sprite.x, y: target.sprite.y }] : undefined);
      } else {
        this.skillEffects.play(skillId, px, py, cx, cy);
      }
      // Effects with a fall/travel time (meteor) land their damage on impact;
      // arrows hit each target as they reach it.
      const aoeDelay = this.skillEffects.getProjectileTravelMs(skillId, px, py, cx, cy);
      const hitDelay = (t: Monster): number => {
        if (skillId !== 'piercing_arrow' && skillId !== 'multishot') return aoeDelay;
        const d = Phaser.Math.Distance.Between(px, py, t.sprite.x, t.sprite.y);
        return Math.min(260, d * 1.1);
      };
      const impactColor = skillImpactColor(skillId, skill.damageType);
      // Blast skills push outward from their centre; arrows from the archer.
      const blastFrom = anchor && skillId !== 'multishot' && skillId !== 'piercing_arrow';
      const applyHit = (t: Monster): number => {
        // Something else may have killed it during the fall/flight.
        if (!t.isAlive()) return 0;
        const fromX = blastFrom && Math.abs(t.sprite.x - cx) + Math.abs(t.sprite.y - cy) > 4 ? cx : px;
        const fromY = blastFrom && Math.abs(t.sprite.x - cx) + Math.abs(t.sprite.y - cy) > 4 ? cy : py;
        const result = this.combatSystem.calculateDamage(this.player.toCombatEntity(this.getEquipStats()), t.toCombatEntity(), skill, level, this.player.skillLevels);
        // Combustion: +50% damage on burning targets
        let finalDmg = result.damage;
        if (skillId === 'combustion' && this.statusEffects.hasEffect(t.id, 'burn')) {
          finalDmg = Math.floor(finalDmg * 1.5);
        }
        const weight = t.takeDamage(finalDmg, fromX, fromY, { isCrit: result.isCrit });
        this.applySteal(result);
        this.showDamageText(t.sprite.x, t.sprite.y, finalDmg, result.isCrit, false, false, skill.damageType);
        // Apply status effects from skill damage type
        this.applySkillStatusEffect(t, skill, finalDmg, this.time.now);
        if (!t.isAlive()) this.onMonsterKilled(t);
        this.vfx?.impactBurst(t.sprite.x, t.sprite.y - 18, Math.atan2(t.sprite.y - fromY, t.sprite.x - fromX), weight, impactColor);
        return 1;
      };
      const stillCasting = (): boolean => this.player.hp > 0 && !this.isTransitioning;
      if (aoeDelay > 0) {
        this.time.delayedCall(aoeDelay, () => {
          if (!stillCasting()) return;
          let hits = 0;
          for (const t of aoeTargets) hits += applyHit(t);
          if (this.vfx && hits > 0) this.vfx.cameraShake(100, 0.004 + hits * 0.001);
        });
      } else {
        let immediate = 0;
        for (const t of aoeTargets) {
          const delay = hitDelay(t);
          if (delay > 0) this.time.delayedCall(delay, () => { if (stillCasting()) applyHit(t); });
          else immediate += applyHit(t);
        }
        if (this.vfx && immediate > 0) this.vfx.cameraShake(100, 0.004 + immediate * 0.001);
      }
    } else if (target) {
      const fromX = this.player.sprite.x;
      const fromY = this.player.sprite.y;
      this.skillEffects.play(skillId, fromX, fromY, target.sprite.x, target.sprite.y);
      const applyHit = (): void => {
        const result = this.combatSystem.calculateDamage(this.player.toCombatEntity(this.getEquipStats()), target.toCombatEntity(), skill, level, this.player.skillLevels);
        // Combustion: +50% damage on burning targets
        let finalDmg = result.damage;
        if (skillId === 'combustion' && this.statusEffects.hasEffect(target.id, 'burn')) {
          finalDmg = Math.floor(finalDmg * 1.5);
        }
        const weight = target.takeDamage(finalDmg, fromX, fromY, { isCrit: result.isCrit });
        this.applySteal(result);
        this.showDamageText(target.sprite.x, target.sprite.y, finalDmg, result.isCrit, false, false, skill.damageType);
        // Apply status effects from skill damage type
        this.applySkillStatusEffect(target, skill, finalDmg, this.time.now);
        if (!target.isAlive()) this.onMonsterKilled(target);
        this.vfx?.impactBurst(target.sprite.x, target.sprite.y - 18,
          Math.atan2(target.sprite.y - fromY, target.sprite.x - fromX), weight, skillImpactColor(skillId, skill.damageType));
        if (this.trails && (skill.damageType !== 'physical' || skill.damageMultiplier > 1.5)) {
          const scorchType = skillId.includes('fire') || skillId === 'meteor' ? 'fire'
            : skillId.includes('ice') || skillId === 'blizzard' ? 'ice'
            : 'lightning';
          this.trails.stampGround(target.sprite.x, target.sprite.y, scorchType);
        }
      };
      // Projectiles deal damage on arrival, not on launch.
      const travelMs = this.skillEffects.getProjectileTravelMs(skillId, fromX, fromY, target.sprite.x, target.sprite.y);
      if (travelMs > 0) {
        this.time.delayedCall(travelMs, () => {
          if (!target.isAlive() || this.player.hp <= 0 || this.isTransitioning) return;
          applyHit();
        });
      } else {
        applyHit();
      }
    }
  }

  private handleCombat(time: number): void {
    if (this.player.hp <= 0) return;
    // In-place reverse-iteration splice to avoid per-frame array allocation
    for (let i = this.player.buffs.length - 1; i >= 0; i--) {
      if (time - this.player.buffs[i].startTime >= this.player.buffs[i].duration) {
        this.player.buffs.splice(i, 1);
      }
    }

    // Use spatial pre-filter: only check monsters near the player (max aggro range ~10 tiles)
    const nearbyAttackers = this.monsterGrid.queryRadius(this.player.tileCol, this.player.tileRow, 12);
    for (const monster of nearbyAttackers) {
      if (!monster.isAlive() || monster.state !== 'attack') continue;
      // Immobilized monsters cannot attack
      if (this.statusEffects.isImmobilized(monster.id)) continue;
      if (time - monster.lastAttackTime >= monster.definition.attackSpeed) {
        // Taunted by (or closer to) the ley-beast: the swing goes to it instead.
        if (this.petCompanion?.interceptMonsterAttack(monster, time)) continue;
        monster.lastAttackTime = time;
        const impactDelay = monster.playAttack(this.player.sprite.x, this.player.sprite.y);
        this.time.delayedCall(impactDelay, () => this.resolveMonsterStrike(monster));
      }
    }

    // Player auto-attack (paused while the player holds to move: they are walking away on purpose)
    const target = this.holdMove ? undefined : this.player.attackTarget
      ? this.monsters.find(m => m.id === this.player.attackTarget && m.isAlive())
      : this.findNearestAggroMonster();

    if (target && target.isAlive()) {
      const dSq = distanceSq(this.player.tileCol, this.player.tileRow, target.tileCol, target.tileRow);
      if (dSq <= this.player.attackRange * this.player.attackRange && time - this.player.lastAttackTime >= this.player.attackSpeed) {
        this.player.lastAttackTime = time;
        // Damage lands on the swing's contact beat, not at wind-up start.
        const impactDelay = this.player.playAttack(target.sprite.x, target.sprite.y);
        this.time.delayedCall(impactDelay, () => this.resolvePlayerStrike(target));
      }
    }
  }

  /** Resolve a basic attack at the moment the swing connects. */
  private resolvePlayerStrike(target: Monster): void {
    if (this.player.hp <= 0 || !target.isAlive() || this.isTransitioning) return;
    const eq = this.getEquipStats();

    // dodgeCounter: guaranteed crit after dodge
    const forceCrit = this._dodgeCounterReady;
    if (forceCrit) {
      this._dodgeCounterReady = false;
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.combat.dodgeCounterCrit'), type: 'combat' });
    }

    const fromX = this.player.sprite.x;
    const fromY = this.player.sprite.y;
    const result = this.combatSystem.calculateDamage(
      this.player.toCombatEntity(eq), target.toCombatEntity(),
      undefined, 1, undefined, forceCrit,
    );
    if (result.isDodged) {
      // Target sidestepped: show a whiff, no flash or impact.
      this.showDamageText(target.sprite.x, target.sprite.y, 0, false, true);
      return;
    }
    const weight = target.takeDamage(result.damage, fromX, fromY, { isCrit: result.isCrit });
    this.applySteal(result);
    this.showDamageText(target.sprite.x, target.sprite.y, result.damage, result.isCrit);

    // Consume stealthDamage buff after attack (it multiplies next attack only)
    if (this.player.buffs.some(b => b.stat === 'stealthDamage')) {
      this.player.buffs = this.player.buffs.filter(b => b.stat !== 'stealthDamage');
    }

    if (result.isCrit || result.damage > 0) {
      EventBus.emit(GameEvents.COMBAT_DAMAGE, {
        targetId: target.id, damage: result.damage, isDodged: false,
        isCrit: result.isCrit, isPlayerTarget: false,
        targetMaxHP: target.maxHp,
      });
      this.playHitImpact(target, weight, fromX, fromY);
    }
    // Weapon slash trail on basic attack
    if (this.trails) {
      const angle = Math.atan2(target.sprite.y - fromY, target.sprite.x - fromX);
      this.trails.stampSlash(target.sprite.x, target.sprite.y - 16, angle, 0xffffcc);
    }

    // critDoubleStrike: on crit, X% chance for immediate extra attack
    if (result.isCrit && eq.critDoubleStrike > 0 && target.isAlive()) {
      if (this.combatSystem.checkCritDoubleStrike(eq.critDoubleStrike, true)) {
        const extraResult = this.combatSystem.calculateDamage(
          this.player.toCombatEntity(eq), target.toCombatEntity(),
        );
        const extraWeight = target.takeDamage(extraResult.damage, fromX, fromY, { isCrit: extraResult.isCrit });
        this.applySteal(extraResult);
        this.showDamageText(target.sprite.x, target.sprite.y - 20, extraResult.damage, extraResult.isCrit);
        EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.combat.comboTrigger'), type: 'combat' });
        this.playHitImpact(target, extraWeight, fromX, fromY);
      }
    }

    // doubleShot: X% chance to fire double projectile on ranged auto-attack
    if (eq.doubleShot > 0 && target.isAlive()) {
      if (this.combatSystem.checkDoubleShot(eq.doubleShot, this.player.attackRange)) {
        const extraResult = this.combatSystem.calculateDamage(
          this.player.toCombatEntity(eq), target.toCombatEntity(),
        );
        const extraWeight = target.takeDamage(extraResult.damage, fromX, fromY, { isCrit: extraResult.isCrit });
        this.applySteal(extraResult);
        this.showDamageText(target.sprite.x + 15, target.sprite.y - 15, extraResult.damage, extraResult.isCrit);
        EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.combat.doubleArrow'), type: 'combat' });
        this.skillEffects.playAttack(fromX, fromY, target.sprite.x, target.sprite.y, true);
        this.playHitImpact(target, extraWeight, fromX, fromY);
      }
    }

    if (!target.isAlive()) {
      this.onMonsterKilled(target);
      if (this.player.attackTarget === target.id) this.player.attackTarget = null;
      EventBus.emit(GameEvents.TARGET_CHANGED, { targetId: null, targetName: null });
    }
  }

  /** Attacker-side hit feedback: hit-stop on the player, impact burst, shake. */
  private playHitImpact(target: Monster, weight: HitWeight, fromX: number, fromY: number): void {
    const profile = HIT_PROFILES[weight];
    this.player.animator.triggerHitFreeze(profile.attackerStopMs);
    if (!this.vfx) return;
    const angle = Math.atan2(target.sprite.y - fromY, target.sprite.x - fromX);
    const color = CLASS_IMPACT_COLORS[this.player.classData.id] ?? 0xfff2c0;
    this.vfx.impactBurst(target.sprite.x, target.sprite.y - 18, angle, weight, color);
    if (weight === 'kill' && target.definition.elite) {
      this.vfx.slowMotion(200, 0.4);
    }
  }

  /** Launch a monster's attack; its damage resolves when the blow (or projectile) lands. */
  private resolveMonsterStrike(monster: Monster): void {
    if (!monster.isAlive() || this.player.hp <= 0 || this.isTransitioning) return;
    if (this.storyDirector?.cinematic) return;
    // Stunned/rooted mid-swing: the attack is interrupted.
    if (this.statusEffects.isImmobilized(monster.id)) return;
    const ranged = monster.definition.attackRange > 2.5;
    if (ranged) {
      const spriteKey = monster.definition.spriteKey;
      const projColor = spriteKey.includes('fire') || spriteKey.includes('phoenix')
        ? 0xff6600 : spriteKey.includes('ice') ? 0x4488ff : 0xcc44cc;
      this.skillEffects.playMonsterRangedAttack(
        monster.sprite.x, monster.sprite.y,
        this.player.sprite.x, this.player.sprite.y, projColor,
        () => {
          if (!monster.isAlive() || this.player.hp <= 0 || this.isTransitioning) return;
          this.applyMonsterHit(monster, true);
        },
      );
      return;
    }
    // Melee whiffs if the player stepped out of reach during the wind-up.
    const reach = monster.definition.attackRange * 1.35 + 0.5;
    if (distanceSq(this.player.tileCol, this.player.tileRow, monster.tileCol, monster.tileRow) > reach * reach) return;
    this.applyMonsterHit(monster, false);
  }

  private applyMonsterHit(monster: Monster, ranged: boolean): void {
    const time = this.time.now;
    const result = this.combatSystem.calculateDamage(monster.toCombatEntity(), this.player.toCombatEntity(this.getEquipStats()));
    if (this.dodgeController.isInvulnerable(time)) {
      if (this.dodgeController.claimAvoidanceReward(time)) {
        this.player.gainSpirit('dodge');
      }
      this.showDamageText(this.player.sprite.x, this.player.sprite.y, 0, false, true);
      EventBus.emit(GameEvents.COMBAT_DAMAGE, {
        targetId: 'player', damage: 0, isDodged: true,
        isCrit: false, isPlayerTarget: true,
        targetMaxHP: this.player.maxHp,
      });
      if (this.vfx) {
        this.vfx.hitSparks(this.player.sprite.x, this.player.sprite.y - 16, 6);
      }
    } else if (result.isDodged) {
      this.player.gainSpirit('dodge');
      this.showDamageText(this.player.sprite.x, this.player.sprite.y, 0, false, true);
      // dodgeCounter: after dodging, next attack is guaranteed crit
      const eqDc = this.getEquipStats();
      if (eqDc.dodgeCounter > 0) {
        this._dodgeCounterReady = true;
        EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.combat.dodgeCounterReady'), type: 'combat' });
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

      const hurtWeight = classifyHit({ damage: finalDmg, maxHp: this.player.maxHp, isCrit: result.isCrit });
      this.player.playHurt(monster.sprite.x, monster.sprite.y, HIT_PROFILES[hurtWeight].recoil);
      monster.animator.triggerHitFreeze(Math.round(HIT_PROFILES[hurtWeight].attackerStopMs * 0.6));
      this.showDamageText(this.player.sprite.x, this.player.sprite.y, finalDmg, result.isCrit, false, true);
      // Ranged hits already showed their projectile burst on arrival.
      if (!ranged) this.skillEffects.playMonsterAttack(this.player.sprite.x, this.player.sprite.y);
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
          EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.combat.freezeSlow'), type: 'combat' });
        }
      }

      if (this.player.hp <= 0) {
        // Death save check (set bonus / legendary)
        const eqDs = this.getEquipStats();
        if (eqDs.deathSave > 0 && !this._deathSaveUsed) {
          this.player.hp = Math.floor(this.player.maxHp * 0.3);
          this._deathSaveUsed = true;
          this.time.delayedCall(60000, () => { this._deathSaveUsed = false; });
          EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.combat.deathImmunity'), type: 'system' });
          if (this.vfx) this.vfx.healBurst(this.player.sprite.x, this.player.sprite.y - 16, 20);
        } else if (this.killPlayer()) {
          return;
        }
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
        const dSq = distanceSq(this.player.tileCol, this.player.tileRow, nearest.tileCol, nearest.tileRow);
        if (dSq <= nearest.definition.aggroRange * nearest.definition.aggroRange) {
          this.player.attackTarget = nearest.id;
          EventBus.emit(GameEvents.TARGET_CHANGED, {
            targetId: nearest.id,
            targetName: getMonsterName(nearest.definition.id, nearest.definition.name),
          });
          if (dSq > this.player.attackRange * this.player.attackRange) {
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
      if (
        sLevel > 0
        && this.player.isSkillReady(skillId, time)
        && this.player.mana >= this.player.getSkillManaCost(skillId, sLevel)
      ) {
        this.requestSkill(skillId, time);
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
    if (targetId && !target) this.player.attackTarget = null;

    const nextTargetId = target?.id ?? null;
    if (nextTargetId !== this.currentTargetId) {
      this.currentTargetId = nextTargetId;
      EventBus.emit(GameEvents.TARGET_CHANGED, {
        targetId: nextTargetId,
        targetName: target
          ? getMonsterName(target.definition.id, target.definition.name)
          : null,
      });
    }

    if (target && target.isAlive()) {
      if (!this.targetIndicator) {
        this.targetIndicator = this.add.ellipse(0, 0, 36, 12, 0xff4444, 0)
          .setStrokeStyle(1.5, 0xff4444, 0.6);
      }
      this.targetIndicator.setPosition(target.sprite.x, target.sprite.y + 4);
      this.targetIndicator.setDepth(target.sprite.depth - 1);
      this.targetIndicator.setVisible(true);
    } else if (this.targetIndicator) {
      this.targetIndicator.setVisible(false);
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
      // Active ley-beast passive (exp / magic find are applied at the kill instead).
      for (const [stat, value] of Object.entries(this.petSystem?.getBonuses() ?? {})) {
        if (stat === 'expBonus' || stat === 'magicFind') continue;
        if (stat in eq) eq[stat as keyof EquipStats] += value;
      }
      // Altar blessing (心焰祭坛) lasts until the next return to the tower.
      for (const [stat, value] of Object.entries(this.homesteadSystem.tower.blessingStats())) {
        if (stat in eq) eq[stat as keyof EquipStats] += value as number;
      }
      // Abyss Labyrinth boons last the run.
      if (this.isInDungeon && this.dungeonRunState?.boons) {
        for (const [stat, value] of Object.entries(DungeonSystem.boonStats(this.dungeonRunState.boons))) {
          if (stat in eq) eq[stat as keyof EquipStats] += value as number;
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
  private applySteal(result: { damage: number; isCrit: boolean; lifeStolen: number; manaStolen: number }): void {
    if (result.damage > 0) {
      this.player.gainSpirit('hit', result.isCrit);
    }
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
        this.monsterGrid.insert(monster);
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
      name: t('zone.event.treasureChest.label'),
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
    EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.event.treasureChest.goldReward', { gold }), type: 'info' });

    // The cache itself: a chest that has just burst open, fading once looted.
    this.spawnOpenedCacheChest(event.col, event.row);

    // Drop items near the event position
    for (const item of items) {
      this.dropLootAtPosition(item, event.col, event.row);
    }

    this.randomEventSystem.resolveActiveEvent();
    EventBus.emit(GameEvents.RANDOM_EVENT_RESOLVED, { type: 'treasure_cache' });
  }

  private addGeneratedNPCVisual(
    container: Phaser.GameObjects.Container,
    spriteKey: string,
    action: 'working' | 'alert' | 'idle' | 'talking' = 'idle',
  ): Phaser.GameObjects.Sprite | null {
    SpriteGenerator.ensureNPCSprite(this, spriteKey);
    if (!this.textures.exists(spriteKey)) return null;

    const sprite = this.add.sprite(0, -40, spriteKey, 0).setScale(1 / TEXTURE_SCALE);
    const animationKey = `${spriteKey}_${action}`;
    if (this.anims.exists(animationKey)) sprite.play(animationKey);
    container.add(sprite);
    return sprite;
  }

  private handleWanderingMerchantEvent(event: ActiveEvent): void {
    // Spawn a visible wandering merchant NPC sprite at the event location
    const merchantCol = Math.round(event.col);
    const merchantRow = Math.round(event.row);
    const walkable = RandomEventSystem.findWalkableTile(
      merchantCol, merchantRow,
      this.mapData.collisions, this.mapData.cols, this.mapData.rows,
    );
    const spawnCol = walkable?.col ?? merchantCol;
    const spawnRow = walkable?.row ?? merchantRow;
    const { x: merchantX, y: merchantY } = cartToIso(spawnCol, spawnRow);

    const merchantContainer = this.add.container(merchantX, merchantY);
    merchantContainer.setDepth(merchantY + 10);

    if (!this.addGeneratedNPCVisual(merchantContainer, 'npc_wandering_merchant', 'talking')) {
      // Fallback: simple circle if texture generation failed
      const body = this.add.circle(0, -12 * DPR, 8 * DPR, 0x8a7a60);
      merchantContainer.add(body);
    }

    // Name label
    const nameLabel = this.add.text(0, 14 * DPR, t('zone.event.merchant.label'), {
      fontSize: fs(10),
      color: '#c0934a',
      fontFamily: '"Noto Sans SC", sans-serif',
      stroke: '#000000',
      strokeThickness: Math.round(2 * DPR),
    }).setOrigin(0.5, 0);
    merchantContainer.add(nameLabel);

    // Emit a shop event so UIScene can show the shop panel
    // The wandering merchant uses the correct SHOP_OPEN payload contract: {npcId, shopItems, type}
    const merchantItems = (event.context.merchantItems as string[]) ?? [];
    EventBus.emit(GameEvents.SHOP_OPEN, {
      npcId: 'wandering_merchant',
      shopItems: merchantItems,
      type: 'merchant',
    });
    EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.event.merchant.announce'), type: 'info' });

    // Despawn the merchant sprite when the shop closes
    const despawnMerchant = () => {
      if (!merchantContainer.scene) return;
      this.tweens.add({
        targets: merchantContainer,
        alpha: 0,
        duration: 500,
        onComplete: () => {
          merchantContainer.destroy();
        },
      });
    };
    // Only despawn when the wandering merchant's own shop panel closes
    const filteredDespawn = (data?: { npcId?: string }) => {
      if (data?.npcId === 'wandering_merchant') {
        despawnMerchant();
        EventBus.off(GameEvents.SHOP_CLOSE, filteredDespawn);
      }
    };
    EventBus.on(GameEvents.SHOP_CLOSE, filteredDespawn);
    // Also despawn on zone exit to avoid stale sprites
    EventBus.once(GameEvents.ZONE_EXIT, () => {
      EventBus.off(GameEvents.SHOP_CLOSE, filteredDespawn);
      if (merchantContainer.scene) merchantContainer.destroy();
    });

    this.randomEventSystem.resolveActiveEvent();
    EventBus.emit(GameEvents.RANDOM_EVENT_RESOLVED, { type: 'wandering_merchant' });
  }

  private handleRescueEvent(event: ActiveEvent): void {
    const monsterIds = event.context.monsterIds as string[] | undefined;
    const count = (event.context.monsterCount as number) ?? 2;
    const monsterDefs = MonstersByZone[this.currentMapId] || [];
    const rescueNpcName = (event.context.rescueNpcName as string) ?? t('zone.event.rescue.fallbackName');
    const rescueNpcSpriteKey = (event.context.rescueNpcSpriteKey as string) ?? 'npc_rescue';
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
    rescueNpcSprite.setDepth(npcY + 60);
    if (!this.addGeneratedNPCVisual(rescueNpcSprite, rescueNpcSpriteKey, 'alert')) {
      const body = this.add.circle(0, -12 * DPR, 8 * DPR, 0x44aaff);
      rescueNpcSprite.add(body);
    }
    const helpMark = this.add.text(0, -78 * DPR, '!', {
      fontSize: fs(14), color: '#ff4444', fontFamily: '"Noto Sans SC", sans-serif', fontStyle: 'bold',
    }).setOrigin(0.5);
    const nameLabel2 = this.add.text(0, 14 * DPR, rescueNpcName, {
      fontSize: fs(10), color: '#aaddff', fontFamily: '"Noto Sans SC", sans-serif',
      stroke: '#000000', strokeThickness: Math.round(2 * DPR),
    }).setOrigin(0.5, 0);
    rescueNpcSprite.add([helpMark, nameLabel2]);

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
        this.monsterGrid.insert(monster);
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
    const rescueNpcName = (event.context.rescueNpcName as string) ?? t('zone.event.rescue.fallbackName');

    // Grant reward now that all hostiles are defeated
    if (reward) {
      this.player.gold += reward.gold;
      this.player.exp += reward.exp;
      EventBus.emit(GameEvents.LOG_MESSAGE, {
        text: t('zone.event.rescue.complete', { npcName: rescueNpcName, gold: reward.gold, exp: reward.exp }),
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
    EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.event.puzzle.prompt', { prompt: puzzle.prompt }), type: 'info' });

    // Spawn an interactable puzzle object near the event position
    const walkable = RandomEventSystem.findWalkableTile(
      Math.round(event.col), Math.round(event.row),
      this.mapData.collisions, this.mapData.cols, this.mapData.rows,
    );
    const puzzleCol = walkable?.col ?? Math.round(event.col);
    const puzzleRow = walkable?.row ?? Math.round(event.row);
    const { x: px2, y: py2 } = cartToIso(puzzleCol, puzzleRow);
    const puzzleSpriteKey = (event.context.puzzleSpriteKey as string) ?? 'decor_puzzle_stone';

    const puzzleSprite = this.add.container(px2, py2);
    puzzleSprite.setDepth(py2 + 10);
    const glow = this.add.ellipse(0, -8 * DPR, 52 * DPR, 22 * DPR, 0x8c62d6, 0.28);
    puzzleSprite.add(glow);
    SpriteGenerator.ensureDecoration(this, puzzleSpriteKey);
    if (this.textures.exists(puzzleSpriteKey)) {
      const prop = this.add.image(0, -30, puzzleSpriteKey).setScale(1 / TEXTURE_SCALE);
      puzzleSprite.add(prop);
    } else {
      const icon = this.add.text(0, -14 * DPR, '?', {
        fontSize: fs(16), color: '#ffcc00', fontFamily: '"Noto Sans SC", sans-serif', fontStyle: 'bold',
      }).setOrigin(0.5);
      puzzleSprite.add(icon);
    }
    const label = this.add.text(0, 10 * DPR, t('zone.event.puzzle.label'), {
      fontSize: fs(10), color: '#ccaaff', fontFamily: '"Noto Sans SC", sans-serif',
      stroke: '#000000', strokeThickness: Math.round(2 * DPR),
    }).setOrigin(0.5, 0);
    puzzleSprite.add(label);
    puzzleSprite.setSize(64 * DPR, 72 * DPR);
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
    const wrongBtn = this.add.text(40 * DPR, 20 * DPR, t('zone.event.puzzle.leave'), {
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
        text: t('zone.event.puzzle.rewardGoldExp', { gold: puzzle.rewardGold, exp: puzzle.rewardExp }),
        type: 'info',
      });
      popup.destroy();
      puzzleSprite.destroy();
      this.randomEventSystem.resolveActiveEvent();
      EventBus.emit(GameEvents.RANDOM_EVENT_RESOLVED, { type: 'environmental_puzzle' });
    });

    wrongBtn.on('pointerdown', () => {
      // Dismiss without reward; puzzle remains for retry
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.event.puzzle.left'), type: 'info' });
      popup.destroy();
    });
  }

  /** Opened treasure-cache chest prop; purely visual, fades after a while. */
  private spawnOpenedCacheChest(col: number, row: number): void {
    const { x, y } = cartToIso(col, row);
    SpriteGenerator.ensureDecoration(this, 'decor_treasure_chest');
    if (!this.textures.exists('decor_treasure_chest')) return;
    const meta = SpriteGenerator.getDecorMeta('decor_treasure_chest');
    const chest = this.add.image(x, y, 'decor_treasure_chest', CHEST_OPEN_FRAME)
      .setOrigin(0.5, meta?.anchorY ?? 0.85)
      .setScale(1 / TEXTURE_SCALE)
      .setDepth(y + 20);
    this.tweens.add({ targets: chest, alpha: 0, delay: 8000, duration: 1200, onComplete: () => chest.destroy() });
    EventBus.once(GameEvents.ZONE_EXIT, () => { if (chest.scene) chest.destroy(); });
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

    // Same cel-shaded loot bag (quality tint + name) as monster drops.
    SpriteGenerator.ensureEffect(this, 'loot_bag');
    const bag = this.add.image(0, 0, 'loot_bag').setScale(1 / TEXTURE_SCALE);
    if (item.quality !== 'normal') bag.setTint(this.getQualityColor(item.quality));
    container.add(bag);
    if (this.vfx && item.quality !== 'normal') this.vfx.applyLootGlow(container, item.quality);
    const qualityColors: Record<string, string> = {
      normal: '#cccccc', magic: '#6888ff', rare: '#f1c40f', legendary: '#ff8800', set: '#2ecc71',
    };
    container.add(this.add.text(0, -18, item.name, {
      fontSize: fs(12), color: qualityColors[item.quality] || '#cccccc',
      fontFamily: '"Cinzel", serif', stroke: '#000000', strokeThickness: Math.round(2 * DPR),
    }).setOrigin(0.5));

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
      const dSq = distanceSq(this.player.tileCol, this.player.tileRow, loot.col, loot.row);
      if (dSq > 4) continue;
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
      if (!def.quests || def.quests.length === 0) continue;

      const indicator = computeNPCIndicator(
        def,
        this.questSystem.quests,
        this.questSystem.progress,
        this.player.level,
      );
      npc.setQuestMarker(indicator.text, indicator.color);
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
              text: t('zone.quest.exploreFound', { targetName: getQuestTargetName(obj.targetId, obj.targetName) }),
              type: 'system',
            });
          }
        }
        // Investigate clues are examined at their marks (QuestWorld).
        // Escort destination check is handled by updateEscortNpc() —
        // completion is gated on the escort NPC arriving, not just the player.
      }
    }
  }

  private onMonsterKilled(monster: Monster): void {
    if (this.isInDungeon) this.onDungeonKill(monster);
    // Clear status effects on death
    this.statusEffects.clearEntity(monster.id);
    this.player.gainSpirit('kill');

    // Difficulty exp/gold scaling is already applied at monster spawn time via DifficultySystem.scaleMonster
    const homeBonus = mergeBonuses(this.homesteadSystem.getTotalBonuses(), this.petSystem.getBonuses());
    const eq = this.getEquipStats();
    const expBonus = 1 + (homeBonus['expBonus'] ?? 0) / 100 + (eq.expBonus ?? 0) / 100;
    const exp = Math.floor(monster.definition.expReward * expBonus);
    const gold = randomInt(monster.definition.goldReward[0], monster.definition.goldReward[1]);
    this.player.addExp(exp);
    this.player.gold += gold;

    // Ley-beasts learn from the kill (active beast + resting ones at the 月井)
    this.petSystem.onKill(monster.definition.level);

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
    this.storyDirector?.onMonsterKilled(monster.definition.id);
    this.emberTower?.onKill(monster.definition, monster.eliteAffixes.length, monster.sprite);

    // Difficulty completion check: killing demon_lord in Abyss Rift completes current difficulty
    if (!this.isInDungeon && DifficultySystem.shouldMarkCompleted(
      monster.definition.id, this.currentMapId, this.difficulty, this.completedDifficulties,
    )) {
      this.completedDifficulties.push(this.difficulty);
      // Persist difficulty completion immediately so it's not lost if game is closed
      this.autoSave();
      const unlocked = DifficultySystem.getNewlyUnlockedDifficulty(this.completedDifficulties);
      if (unlocked) {
        const msg = t(`data.difficulty.${unlocked}.unlock`);
        if (msg && msg !== `data.difficulty.${unlocked}.unlock`) {
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

    // 灵脉果 (ley-beast food): rare drop, likelier from elites
    if (Math.random() < leyFruitDropChance(!!monster.definition.elite)) {
      const fruit = this.lootSystem.createItem(LEY_FRUIT_ID, this.player.level, 'normal');
      if (fruit) { fruit.identified = true; this.dropLoot(fruit, monster.tileCol, monster.tileRow); }
    }

    this.rollQuestDrops(monster);

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
      text: t('zone.monsterKill', { monsterName: getMonsterName(monster.definition.id, monster.definition.name), exp, gold }),
      type: 'loot',
    });

    // Don't respawn mini-bosses
    if (this.miniBossMonster === monster) {
      this.miniBossMonster = null;
    } else if (this.questSpawned.has(monster) || this.isInDungeon) {
      if (this.questHuntMonsters.get(monster.definition.id) === monster) this.questHuntMonsters.delete(monster.definition.id);
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

    // Cel-shaded potion flask standing on the ground point.
    const potionKey = type === 'hp' ? 'potion_drop_hp' : 'potion_drop_mp';
    SpriteGenerator.ensureEffect(this, potionKey);
    const flask = this.add.image(0, 4, potionKey)
      .setOrigin(0.5, SpriteGenerator.getEffectAnchorY(potionKey) ?? 0.5)
      .setScale(1 / TEXTURE_SCALE);
    container.add(flask);

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
    const dSq = distanceSq(this.player.tileCol, this.player.tileRow, lootDrop.col, lootDrop.row);
    if (dSq > 4) {
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

  /**
   * Quest item drops: each unfinished collect objective whose source lists this
   * monster rolls its chance; items pop out and fly to the player. Objectives
   * without a source keep the legacy rule (any monster in the zone, first
   * unfinished one per quest).
   */
  private rollQuestDrops(monster: Monster): void {
    for (const { quest, progress } of this.questSystem.getActiveQuests()) {
      if (progress.status !== 'active' || quest.zone !== this.currentMapId) continue;
      let legacyRolled = false;
      for (let i = 0; i < quest.objectives.length; i++) {
        const obj = quest.objectives[i];
        if (!isCollectObjective(obj) || progress.objectives[i].current >= obj.required) continue;
        if (obj.source?.kind === 'gather') continue;
        // Craft materials only drop from explicit sources.
        if (!obj.source && obj.type === 'craft_collect') continue;
        const chance = questDropChance(obj, monster.definition.id);
        if (!obj.source) {
          if (legacyRolled) continue;
          legacyRolled = true;
        }
        if (chance <= 0 || Math.random() >= (obj.source ? chance : FALLBACK_COLLECT_CHANCE)) continue;
        this.questWorld?.dropToPlayer(monster.sprite.x, monster.sprite.y, obj);
        this.questSystem.updateProgress(obj.type, obj.targetId);
      }
    }
  }

  /** Pick-one equipment rewards for a quest, generated once per session so re-opening the card can't reroll them. */
  getQuestRewardChoices(questId: string): ItemInstance[] {
    const cached = this.questSystem.rewardChoiceCache.get(questId);
    if (cached) return cached;
    const quest = this.questSystem.quests.get(questId);
    if (!quest) return [];
    const choices = generateRewardChoices(quest, this.player.classData.id, this.player.level,
      (baseId, level, quality) => this.lootSystem.createItem(baseId, level, quality));
    this.questSystem.rewardChoiceCache.set(questId, choices);
    return choices;
  }

  /**
   * Turn in a completed quest and grant everything it pays: exp, gold, fixed
   * items, the chosen equipment reward (default: the first choice) and pets.
   * Returns false if the quest wasn't ready.
   */
  turnInQuest(questId: string, choiceIndex = 0): boolean {
    const choices = this.getQuestRewardChoices(questId);
    const reward = this.questSystem.turnInQuest(questId);
    if (!reward) return false;
    this.player.addExp(reward.exp);
    this.player.gold += reward.gold;
    const granted: ItemInstance[] = [];
    for (const itemId of reward.items ?? []) {
      const item = this.lootSystem.createItem(itemId, this.player.level, 'normal');
      if (item) { item.identified = true; granted.push(item); }
    }
    const chosen = choices[Math.max(0, Math.min(choices.length - 1, choiceIndex))];
    if (chosen) granted.push(chosen);
    for (const item of granted) {
      if (!this.inventorySystem.addItem(item)) this.inventorySystem.stash.push(item);
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.quest.rewardItem', { name: getLocalizedItemName(item) }), type: 'loot' });
    }
    this.questSystem.rewardChoiceCache.delete(questId);
    if (reward.petReward) this.petSystem.addPet(reward.petReward);
    this.achievementSystem.update('quest');
    this.autoSave();
    return true;
  }

  private interactNPC(npc: NPC): void {
    const def = npc.definition;
    if (this.emberTower?.interactNpc(def.id)) return;
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
        // Quest NPCs open the quest card (accept / turn in with reward choice);
        // with nothing to hand over they fall back to their dialogue tree or lines.
        const completedQuests: string[] = [];
        for (const [qid, prog] of this.questSystem.progress.entries()) {
          if (prog.status === 'turned_in') completedQuests.push(qid);
        }
        EventBus.emit(GameEvents.NPC_INTERACT, {
          npcId: def.id,
          npcName: getNpcName(def.id, def.name),
          dialogue: def.dialogue.length > 1 ? def.dialogue[1] : def.dialogue[0],
          actions: [],
          dialogueTree: def.dialogueTree,
          completedQuests,
          questSystem: this.questSystem,
          player: this.player,
          homesteadSystem: this.homesteadSystem,
          achievementSystem: this.achievementSystem,
          turnedIn: [],
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
        playerStats: this.getPlayerTransitionStats(),
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
      const dSq = distanceSq(this.player.tileCol, this.player.tileRow, exit.col, exit.row);
      if (dSq < 2.25) {
        if (this.isInDungeon) {
          // In dungeon: either advance to next floor or exit dungeon
          this.tryLeaveDungeonFloor();
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

  private getPlayerTransitionStats(
    hp = this.player.hp,
    mana = this.player.mana,
  ) {
    return {
      level: this.player.level,
      exp: this.player.exp,
      gold: this.player.gold,
      hp,
      mana,
      stats: { ...this.player.stats },
      freeStatPoints: this.player.freeStatPoints,
      freeSkillPoints: this.player.freeSkillPoints,
      skillLevels: Object.fromEntries(this.player.skillLevels),
      spirit: this.player.spirit.toSaveState(),
      buffs: [...this.player.buffs],
      autoCombat: this.player.autoCombat,
      autoLootMode: this.player.autoLootMode,
    };
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
          spirit: this.player.spirit.toSaveState(),
          tileCol: saveTileCol, tileRow: saveTileRow, currentMap: saveMapId,
        },
        inventory: this.inventorySystem.inventory,
        equipment: this.inventorySystem.equipment as any,
        stash: this.inventorySystem.stash,
        quests: this.questSystem.getProgressData(),
        exploration: this.fogData,
        homestead: {
          buildings: this.homesteadSystem.buildings,
          ...this.homesteadSystem.tower.toSave(),
        },
        pets: this.petSystem.toSave(),
        achievements: this.achievementSystem.getUnlockedData(),
        settings: { autoCombat: this.player.autoCombat, musicVolume: 0.5, sfxVolume: 0.7, autoLootMode: this.player.autoLootMode },
        difficulty: this.difficulty,
        completedDifficulties: [...this.completedDifficulties],
        mercenary: this.mercenarySystem?.toSaveData(),
        dialogueState: this.getDialogueState(),
        miniBossDialogueSeen: [...this.miniBossDialogueSeen],
        loreCollected: [...this.loreCollected],
        discoveredHiddenAreas: [...this.discoveredHiddenAreas],
        storySeen: this.session?.story.toSave(),
        soulEcho: this.session?.soulEcho.toSave() ?? null,
        abyss: this.session ? { ...this.session.abyss } : undefined,
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
    this.player.spirit.restore(save.player.spirit ?? {
      value: 0,
      resonanceRemainingMs: 0,
    });
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
      console.log(t('zone.save.positionReset'));
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.save.positionReset'), type: 'system' });
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
    // Saves from before the story existed: don't replay the prologue for veterans.
    this.session?.story.load(save.storySeen ?? ['prologue']);
    this.session?.soulEcho.load(save.soulEcho);
    if (save.abyss && this.session) {
      this.session.abyss = {
        unlockedTier: Math.max(1, save.abyss.unlockedTier ?? 1),
        bestTier: Math.max(0, save.abyss.bestTier ?? 0),
        bestTimeMs: save.abyss.bestTimeMs,
      };
    }

    // 4. Homestead
    if (save.homestead) {
      this.homesteadSystem.buildings = save.homestead.buildings ?? {};
    }
    // Ember Tower state (embers, garden, expedition, blessing); old saves get defaults.
    this.homesteadSystem.tower.load(save.homestead);

    // 4b. Ley-beasts (new `pets` field, or migrated from the old homestead block)
    this.petSystem.loadSave(save);

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
    const safeRadiusSq = safeRadius * safeRadius;
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
            if (distanceSq(c, r, camp.col, camp.row) < safeRadiusSq) {
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
          this.monsterGrid.insert(monster);
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
      if (this.petSystem.hasPet(spawn.petId)) continue;

      const { x: worldX, y: worldY } = cartToIso(spawn.col, spawn.row);
      const container = this.add.container(worldX, worldY);
      container.setDepth(worldY + 100);

      // Soft violet pool of light on the ground
      const glow = this.add.image(0, 0, 'fx_glow').setTint(0xaa44ff).setBlendMode(Phaser.BlendModes.ADD)
        .setScale(0.6, 0.28).setAlpha(0.5);
      container.add(glow);

      // The rare pet itself (same cel-shaded art as the follower), hovering
      const petKey = `decor_pet_${spawn.petId}`;
      SpriteGenerator.ensureDecoration(this, petKey);
      const wing = this.add.image(0, -4 * DPR, petKey)
        .setOrigin(0.5, SpriteGenerator.getDecorMeta(petKey)?.anchorY ?? 0.9)
        .setScale(1 / TEXTURE_SCALE);
      container.add(wing);

      // Floating label
      const label = this.add.text(0, -40 * DPR, t('zone.pet.rareLabel', { name: getPetName(spawn.petId, spawn.petId) }), {
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
        scaleX: { from: 0.54, to: 0.66 },
        scaleY: { from: 0.25, to: 0.31 },
        duration: 1200,
        yoyo: true,
        repeat: -1,
        ease: 'Sine.easeInOut',
      });
      // Gentle float on the wing
      this.tweens.add({
        targets: wing,
        y: -10 * DPR,
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
      const dSq = distanceSq(this.player.tileCol, this.player.tileRow, ps.col, ps.row);
      if (dSq <= 4) {
        const success = this.petSystem.addPet(ps.petId, { silent: true });
        if (success) {
          EventBus.emit(GameEvents.LOG_MESSAGE, {
            text: t('zone.pet.found', { name: getPetName(ps.petId, ps.petId) }),
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
            this.monsterGrid.insert(monster);
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
    this.monsterGrid.insert(monster);
    this.miniBossMonster = monster;
  }
  /** Check if the player is within aggro range of the mini-boss and trigger dialogue. */
  private checkMiniBossDialogue(): void {
    if (this.miniBossDialogueActive) return;
    if (!this.miniBossMonster || !this.miniBossMonster.isAlive()) return;

    const mb = this.miniBossMonster;
    const dSq = distanceSq(this.player.tileCol, this.player.tileRow, mb.tileCol, mb.tileRow);
    if (dSq > mb.definition.aggroRange * mb.definition.aggroRange) return;

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
      const label = this.add.text(0, -46 * DPR, `✦ ${entry.name}`, {
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
    // Cel-shaded prop per lore type (src/graphics/sprites/decorations/LoreProps.ts),
    // standing on the tile's ground point, over a soft pulsing pool of light.
    const glowTint = Phaser.Display.Color.HexStringToColor(this.getLoreSpriteColor(spriteType)).color;
    const pool = this.add.image(0, 0, 'fx_glow').setTint(glowTint).setBlendMode(Phaser.BlendModes.ADD)
      .setScale(0.55, 0.26).setAlpha(0.8);
    const glow = this.add.container(0, 0, [pool]);
    let key = `decor_lore_${spriteType}`;
    if (!SpriteGenerator.hasDecoration(key)) key = 'decor_lore_scroll';
    SpriteGenerator.ensureDecoration(this, key);
    const prop = this.add.image(0, 0, key)
      .setOrigin(0.5, SpriteGenerator.getDecorMeta(key)?.anchorY ?? 0.9)
      .setScale(1 / TEXTURE_SCALE);
    return { elements: [glow, prop], glow };
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
      const dSq = distanceSq(this.player.tileCol, this.player.tileRow, ls.entry.col, ls.entry.row);
      if (dSq <= 4) {
        // Collect the lore entry
        this.loreCollected.add(ls.entry.id);

        // Show lore text via EventBus → UIScene
        EventBus.emit(GameEvents.LORE_COLLECTED, {
          entry: ls.entry,
        });

        EventBus.emit(GameEvents.LOG_MESSAGE, {
          text: t('zone.lore.discovered', { loreName: getLoreName(ls.entry.id, ls.entry.name) }),
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
      if (!EmberTower.npcPresent(this.currentMapId, def.id, this.homesteadSystem)) continue;
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
    const vrSq = vr * vr;
    const cols = this.exploredTilesCols;
    const minC = Math.max(0, Math.floor(pc - vr));
    const maxC = Math.min(this.mapData.cols - 1, Math.ceil(pc + vr));
    const minR = Math.max(0, Math.floor(pr - vr));
    const maxR = Math.min(this.mapData.rows - 1, Math.ceil(pr + vr));
    for (let r = minR; r <= maxR; r++) {
      for (let c = minC; c <= maxC; c++) {
        const dc = c - pc;
        const dr = r - pr;
        if (dc * dc + dr * dr <= vrSq) {
          this.exploredTiles[r * cols + c] = 1;
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
    return checkPoints.every(p => this.exploredTiles[p.r * this.exploredTilesCols + p.c] === 1);
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
      text: t('zone.hiddenArea.discovered', { areaName: getHiddenAreaName(area.id, area.name) }),
      type: 'system',
    });

    // Show discovery text as banner
    const dp = this.screenPos(0.5, 0.3);
    const discoveryBanner = this.add.text(dp.x, dp.y, getHiddenAreaDiscoveryText(area.id, area.discoveryText), {
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

    const rewardSpriteKey = reward.type === 'chest'
      ? 'decor_treasure_chest'
      : reward.type === 'gold_pile'
        ? 'decor_gold_pile'
        : 'decor_lore_scroll';
    SpriteGenerator.ensureDecoration(this, rewardSpriteKey);
    let propTop = -30 * DPR;
    if (this.textures.exists(rewardSpriteKey)) {
      // Stand the prop on its ground-contact point; chests loop their glint.
      const rewardVisual = this.add.sprite(0, 0, rewardSpriteKey, 0)
        .setOrigin(0.5, SpriteGenerator.getDecorMeta(rewardSpriteKey)?.anchorY ?? 0.5)
        .setScale(1 / TEXTURE_SCALE);
      const loopAnim = SpriteGenerator.getLoopAnimKey(this, rewardSpriteKey);
      if (loopAnim) rewardVisual.play({ key: loopAnim, startFrame: rewardIndex % 8 });
      container.add(rewardVisual);
      propTop = Math.min(propTop, -rewardVisual.displayHeight * rewardVisual.originY - 8);
    } else if (reward.type === 'chest') {
      // Keep a minimal resilience fallback for a failed texture context.
      const chest = this.add.rectangle(0, -12, Math.round(20 * DPR), Math.round(14 * DPR), 0xDAA520);
      chest.setStrokeStyle(Math.round(2 * DPR), 0x8B6914);
      container.add(chest);
    } else if (reward.type === 'gold_pile') {
      const gold = this.add.ellipse(0, -8, Math.round(18 * DPR), Math.round(8 * DPR), 0xFFD700);
      gold.setStrokeStyle(Math.round(1 * DPR), 0xDAA520);
      container.add(gold);
    } else {
      const scroll = this.add.rectangle(0, -12, Math.round(14 * DPR), Math.round(18 * DPR), 0xDEB887);
      scroll.setStrokeStyle(Math.round(1 * DPR), 0x8B7355);
      container.add(scroll);
    }

    if (reward.type === 'chest') {
      // Warm pool of light under the chest
      const glow = this.add.image(0, 0, 'fx_glow').setTint(0xffd060).setBlendMode(Phaser.BlendModes.ADD)
        .setScale(0.7, 0.3).setAlpha(0.6);
      container.add(glow);
      container.sendToBack(glow);
      this.tweens.add({ targets: glow, alpha: 0.2, duration: 800, yoyo: true, repeat: -1, ease: 'Sine.easeInOut' });
    }

    // Interactable label
    const label = this.add.text(0, propTop, reward.type === 'chest' ? t('zone.hiddenArea.rewardChest') : reward.type === 'gold_pile' ? t('zone.hiddenArea.rewardGoldPile') : t('zone.hiddenArea.rewardScroll'), {
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
          text: t('zone.hiddenArea.gotItem', { itemName: getLocalizedItemName(item) }),
          type: 'loot',
        });
        EventBus.emit(GameEvents.INVENTORY_CHANGED, {});
      }
    } else if (reward.type === 'gold_pile') {
      const goldAmount = parseInt(reward.value || '100', 10);
      this.player.gold += goldAmount;
      EventBus.emit(GameEvents.LOG_MESSAGE, {
        text: t('zone.hiddenArea.gotGold', { amount: goldAmount }),
        type: 'loot',
      });
    } else if (reward.type === 'lore') {
      EventBus.emit(GameEvents.LOG_MESSAGE, {
        text: t('zone.hiddenArea.gotScroll'),
        type: 'system',
      });
    }

    // Chests pop open before fading; other rewards fade straight away.
    let fadeDelay = 0;
    if (reward.type === 'chest') {
      for (const child of entry.sprite.list) {
        if (child instanceof Phaser.GameObjects.Sprite && child.texture.key === 'decor_treasure_chest'
          && child.texture.has(String(CHEST_OPEN_FRAME))) {
          child.stop();
          child.setFrame(CHEST_OPEN_FRAME);
          fadeDelay = 700;
        }
      }
    }

    // Animate and destroy
    this.tweens.add({
      targets: entry.sprite,
      alpha: 0, scaleX: 1.3, scaleY: 1.3,
      delay: fadeDelay,
      duration: 400,
      onComplete: () => { entry.sprite.destroy(); },
    });

    const idx = this.hiddenAreaSprites.indexOf(entry);
    if (idx !== -1) this.hiddenAreaSprites.splice(idx, 1);
  }

  // ─── Random Dungeon Portal & Floor Transitions ───────────────────────

  /**
   * Add an animated gate sprite (dungeon / sub-dungeon entrance) standing on
   * the container's ground point. Returns the label Y just above its top.
   */
  private addGateVisual(container: Phaser.GameObjects.Container, key: string, glowTint: number): number {
    const pool = this.add.image(0, 0, 'fx_glow').setTint(glowTint).setBlendMode(Phaser.BlendModes.ADD)
      .setScale(1.1, 0.45).setAlpha(0.45);
    container.add(pool);
    this.tweens.add({ targets: pool, alpha: 0.2, duration: 1400, yoyo: true, repeat: -1, ease: 'Sine.easeInOut' });
    SpriteGenerator.ensureEffect(this, key);
    if (!this.textures.exists(key)) return -46 * DPR;
    const gate = this.add.sprite(0, 0, key, 0)
      .setOrigin(0.5, SpriteGenerator.getEffectAnchorY(key) ?? 0.8)
      .setScale(1 / TEXTURE_SCALE);
    const anim = SpriteGenerator.getLoopAnimKey(this, key);
    if (anim) gate.play(anim);
    container.add(gate);
    return -gate.displayHeight * gate.originY - 8;
  }

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

    // Crimson basalt gate with a swirling vortex (effects/DungeonGates.ts)
    const gateTop = this.addGateVisual(container, 'dungeon_portal', 0xff5a2a);

    // Label
    const label = this.add.text(0, gateTop, DungeonSystem.getDungeonPortalLabel(), {
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
  private enterDungeon(tier = 1): void {
    if (this.isTransitioning) return;
    this.isTransitioning = true;
    this.autoSave();

    const run = DungeonSystem.createRun(this.difficulty, undefined, tier);
    const floorConfig = DungeonSystem.getFloorConfig(run, 1);

    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: t('zone.dungeon.enter', { floors: run.totalFloors }),
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
        playerStats: this.getPlayerTransitionStats(),
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
      text: t('zone.dungeon.floorEnter', { floor: nextFloor }),
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
        playerStats: this.getPlayerTransitionStats(),
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
      text: t('zone.dungeon.exit'),
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
        playerStats: this.getPlayerTransitionStats(),
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

      // Themed entrance (mine shaft / demon ring gate) from effects/DungeonGates.ts
      const gateTop = this.addGateVisual(container, subDungeonGateKey(entrance.targetSubDungeon), 0xb070ff);

      // Label
      const label = this.add.text(0, gateTop, getSubDungeonEntranceName(entrance.id, entrance.name), {
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
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.dungeon.entranceBlocked'), type: 'system' });
      return;
    }
    if (this.isTransitioning) return;
    this.isTransitioning = true;
    this.autoSave();

    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: t('zone.subDungeon.enter', { name: getSubDungeonName(subDungeonData.id, subDungeonData.name) }),
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
        playerStats: this.getPlayerTransitionStats(),
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
      text: t('zone.subDungeon.exit'),
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
        playerStats: this.getPlayerTransitionStats(),
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
      container.setDepth(worldY + 70);

      // Try to use decoration sprite by type
      const texKey = `decor_${decoration.spriteType}`;
      SpriteGenerator.ensureDecoration(this, decoration.spriteType);
      let propTop = -28 * DPR;
      if (this.textures.exists(texKey)) {
        const meta = SpriteGenerator.getDecorMeta(texKey);
        const sprite = this.add.image(0, meta ? 0 : -8, texKey)
          .setOrigin(0.5, meta ? meta.anchorY : 0.5)
          .setScale(1 / TEXTURE_SCALE);
        container.add(sprite);
        if (meta) {
          if (meta.flat) container.setDepth(worldY + 5);
          propTop = Math.min(propTop, -sprite.displayHeight * meta.anchorY - 6);
        }
      } else {
        // Fallback: colored rectangle
        const color = this.getStoryDecorationColor(decoration.spriteType);
        const rect = this.add.rectangle(0, -10, Math.round(18 * DPR), Math.round(22 * DPR), color, 0.8);
        rect.setStrokeStyle(Math.round(1 * DPR), 0x444444);
        container.add(rect);
      }

      // Name label
      const label = this.add.text(0, propTop, decoration.name, {
        fontSize: fs(8),
        color: '#CCCCAA',
        fontFamily: '"Noto Sans SC", sans-serif',
        stroke: '#000000',
        strokeThickness: Math.round(2 * DPR),
      }).setOrigin(0.5).setAlpha(0);
      container.add(label);

      // Interaction indicator (small sparkle)
      const sparkle = this.add.ellipse(8 * DPR, Math.max(propTop + 12, -40), Math.round(4 * DPR), Math.round(4 * DPR), 0xFFFFCC, 0.6);
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
      const dSq = distanceSq(this.player.tileCol, this.player.tileRow, sd.col, sd.row);
      if (dSq <= 9 && dSq < closestDist) {
        closestDist = dSq;
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
    const safeRadiusSq = safeRadius * safeRadius;
    // Try random offsets, fall back to spawn point
    let c = dead.spawnCol;
    let r = dead.spawnRow;
    for (let attempt = 0; attempt < 8; attempt++) {
      const tc = dead.spawnCol + randomInt(-2, 2);
      const tr = dead.spawnRow + randomInt(-2, 2);
      if (tc >= 0 && tc < this.mapData.cols && tr >= 0 && tr < this.mapData.rows && this.mapData.collisions[tr][tc]) {
        let inSafeZone = false;
        for (const camp of this.campPositions) {
          if (distanceSq(tc, tr, camp.col, camp.row) < safeRadiusSq) {
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
    this.monsterGrid.remove(dead);
    this.monsters[idx] = new Monster(this, dead.originalDefinition, c, r);
    this.monsterGrid.insert(this.monsters[idx]);
    // Re-apply elite affixes on respawn
    if (dead.originalDefinition.elite) {
      const affixes = this.eliteAffixSystem.rollAffixes(this.currentMapId, true);
      if (affixes.length > 0) {
        this.monsters[idx].applyEliteAffixes(affixes, this.eliteAffixSystem);
      }
    }
  }

  private findNearestAliveMonster(): Monster | null {
    return this.monsterGrid.findNearest(
      this.player.tileCol, this.player.tileRow,
      Math.max(this.mapData.cols, this.mapData.rows),
      m => m.isAlive(),
    );
  }

  private findNearestAggroMonster(): Monster | null {
    return this.monsterGrid.findNearest(
      this.player.tileCol, this.player.tileRow,
      Math.max(this.mapData.cols, this.mapData.rows),
      m => m.isAlive() && m.isAggro(),
    );
  }

  private findMonsterAt(col: number, row: number): Monster | null {
    const nearby = this.monsterGrid.queryRadius(col, row, 2);
    for (const m of nearby) {
      if (!m.isAlive()) continue;
      if (Math.abs(m.tileCol - col) < 1.5 && Math.abs(m.tileRow - row) < 1.5) return m;
    }
    return null;
  }

  private findNPCAt(col: number, row: number): NPC | null {
    let best: NPC | null = null;
    let bestDist = Infinity;
    for (const npc of this.npcs) {
      const dSq = (npc.tileCol - col) ** 2 + (npc.tileRow - row) ** 2;
      if (dSq < 3.24 && dSq < bestDist) {
        bestDist = dSq;
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
    else if (isPlayer) { text = `-${damage}`; color = isCrit ? '#ff4444' : '#e74c3c'; if (isCrit) size = fs(24); }
    else {
      text = `${damage}`;
      color = isCrit ? '#ffd700' : (damageType && elementColors[damageType]) || '#ffffff';
      if (isCrit) size = fs(26);
    }

    // Stack numbers that spawn on the same spot in quick succession so
    // multi-hits and AoE ticks stay readable instead of overprinting.
    const now = this.time.now;
    const stackKey = `${Math.round(x / 28)},${Math.round(y / 28)}`;
    const prev = this.damageTextStacks.get(stackKey);
    const stackIndex = prev && now - prev.time < 320 ? Math.min(prev.index + 1, 4) : 0;
    this.damageTextStacks.set(stackKey, { time: now, index: stackIndex });
    if (this.damageTextStacks.size > 64) {
      for (const [key, entry] of this.damageTextStacks) {
        if (now - entry.time > 1000) this.damageTextStacks.delete(key);
      }
    }
    const drift = (stackIndex % 2 === 0 ? 1 : -1) * (isCrit ? 14 : 10) + randomInt(-4, 4);
    const startX = x + randomInt(-6, 6);
    const startY = y - 30 - stackIndex * 11;

    // Acquire from pool or create new
    let t: Phaser.GameObjects.Text;
    const poolIdx = this.floatingTextPool.findIndex(obj => !obj.active);
    if (poolIdx !== -1) {
      t = this.floatingTextPool[poolIdx];
      this.tweens.killTweensOf(t);
      t.setActive(true).setVisible(true);
      t.setPosition(startX, startY);
      t.setText(text);
      t.setStyle({ fontSize: size, color, fontFamily: '"Cinzel", serif', fontStyle: isCrit ? 'bold' : 'normal', stroke: '#000000', strokeThickness: Math.round((isCrit ? 4 : 3) * DPR) });
    } else {
      t = this.add.text(startX, startY, text, {
        fontSize: size, color, fontFamily: '"Cinzel", serif', fontStyle: isCrit ? 'bold' : 'normal',
        stroke: '#000000', strokeThickness: Math.round((isCrit ? 4 : 3) * DPR),
      });
      this.floatingTextPool.push(t);
    }
    t.setOrigin(0.5).setDepth(ZONE_FLOATING_TEXT_DEPTH + stackIndex);
    t.setAlpha(1).setAngle(0);

    const releaseToPool = (): void => { t.setActive(false).setVisible(false); };

    if (isDodged) {
      t.setScale(0.8);
      this.tweens.add({ targets: t, scale: 1, duration: 90, ease: 'Quad.easeOut' });
      this.tweens.add({ targets: t, y: startY - 22, alpha: 0, duration: 650, delay: 120, ease: 'Quad.easeOut', onComplete: releaseToPool });
      return;
    }

    // Pop: overshoot then settle — sells the impact of the number itself.
    const peak = isCrit ? 1.5 : 1.2;
    const rest = isCrit ? 1.1 : 1;
    t.setScale(isCrit ? 0.35 : 0.5);
    if (isCrit) t.setAngle(-8);
    this.tweens.add({
      targets: t, scale: peak, angle: 0, duration: isCrit ? 90 : 70, ease: 'Quad.easeOut',
      onComplete: () => {
        if (!t.active) return;
        this.tweens.add({ targets: t, scale: rest, duration: isCrit ? 160 : 110, ease: 'Back.easeOut' });
      },
    });
    // Arc: drift sideways while rising, then sink slightly as it fades.
    const rise = isCrit ? 40 : 28;
    const life = isCrit ? 1050 : 780;
    this.tweens.add({ targets: t, x: startX + drift, duration: life, ease: 'Sine.easeOut' });
    this.tweens.add({
      targets: t, y: startY - rise, duration: life * 0.45, ease: 'Quad.easeOut',
      onComplete: () => {
        if (!t.active) return;
        this.tweens.add({
          targets: t, y: startY - rise + 8, alpha: 0, duration: life * 0.55, ease: 'Quad.easeIn',
          onComplete: releaseToPool,
        });
      },
    });
  }

  /** Convert a desired screen-fraction position to scrollFactor(0) object position, accounting for camera zoom. */
  /** Camera zoom in logical pixels (the render scale divided out): offsets for screen-fixed text use this. */
  private viewZoom(): number {
    return this.cameras.main.zoom / RENDER_SCALE;
  }

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
      arrivalMsg = this.isInSubDungeon ? t('zone.teleport.toSubDungeonEntrance') : t('zone.teleport.toDungeonExit');

      // Already near exit?
      if (distanceSq(this.player.tileCol, this.player.tileRow, destCol, destRow) < 9) {
        EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.teleport.alreadyAtExit'), type: 'system' });
        return;
      }
    } else {
      const camp = this.campPositions[0];
      if (!camp) return;
      destCol = camp.col;
      destRow = camp.row;
      arrivalMsg = t('zone.teleport.toCamp');

      // Already near camp?
      const safeRadius = this.mapData.safeZoneRadius ?? 9;
      if (distanceSq(this.player.tileCol, this.player.tileRow, destCol, destRow) < safeRadius * safeRadius) {
        EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.teleport.alreadyAtCamp'), type: 'system' });
        return;
      }
    }

    this.isPortaling = true;
    this.player.path = [];
    this.player.isMoving = false;
    this.player.attackTarget = null;

    EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.teleport.opening'), type: 'system' });

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
    const text = this.add.text(p.x, p.y, t('zone.levelUp.text'), {
      fontSize: fs(32), color: '#ffd700', fontFamily: '"Cinzel", serif',
      fontStyle: 'bold', stroke: '#000000', strokeThickness: Math.round(5 * DPR),
    }).setOrigin(0.5).setScrollFactor(0).setDepth(ZONE_SCREEN_UI_DEPTH).setAlpha(0).setScale(0.5);

    const lvlText = this.add.text(p.x, p.y + 38 * DPR / this.viewZoom(), t('zone.levelUp.level', { level }), {
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

  private showQuestCompleteBanner(questName: string, hint = ''): void {
    const p = this.screenPos(0.5, 0.22);
    const z = this.viewZoom();
    const label = this.add.text(p.x, p.y, t('zone.questComplete'), {
      fontSize: fs(20), color: '#f1c40f', fontFamily: '"Cinzel", serif',
      fontStyle: 'bold', stroke: '#000000', strokeThickness: Math.round(4 * DPR),
    }).setOrigin(0.5).setScrollFactor(0).setDepth(ZONE_SCREEN_UI_DEPTH).setAlpha(0);

    const name = this.add.text(p.x, p.y + 28 * DPR / z, questName, {
      fontSize: fs(16), color: '#e0d8cc', fontFamily: '"Cinzel", serif',
      stroke: '#000000', strokeThickness: Math.round(3 * DPR),
    }).setOrigin(0.5).setScrollFactor(0).setDepth(ZONE_SCREEN_UI_DEPTH).setAlpha(0);

    const parts: Phaser.GameObjects.Text[] = [label, name];
    if (hint) {
      parts.push(this.add.text(p.x, p.y + 52 * DPR / z, hint, {
        fontSize: fs(13), color: '#ffd98a', fontFamily: '"Noto Sans SC", sans-serif',
        stroke: '#000000', strokeThickness: Math.round(3 * DPR),
      }).setOrigin(0.5).setScrollFactor(0).setDepth(ZONE_SCREEN_UI_DEPTH).setAlpha(0));
    }
    this.tweens.add({ targets: parts, alpha: 1, duration: 500, ease: 'Power2' });
    this.time.delayedCall(3000, () => {
      this.tweens.add({
        targets: parts, alpha: 0, duration: 600,
        onComplete: () => { for (const o of parts) o.destroy(); },
      });
    });
  }

  private showZoneBanner(): void {
    const p = this.screenPos(0.5, 0.32);
    const z = this.viewZoom();
    const banner = this.add.text(p.x, p.y, getZoneName(this.currentMapId, this.mapData.name), {
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
          const dSq = distanceSq(monster.tileCol, monster.tileRow, this.player.tileCol, this.player.tileRow);
          if (dSq > 4 && dSq < 225) {
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
              this.monsterGrid.update(monster);
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
        const dSq = distanceSq(monster.tileCol, monster.tileRow, this.player.tileCol, this.player.tileRow);
        if (dSq <= curseAffix.definition.curseAuraRadius * curseAffix.definition.curseAuraRadius) {
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
            EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.combat.curseAura'), type: 'combat' });
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
            this.killPlayer();
          }
        } else {
          // DoT damage to monster
          const monster = this.monsters.find(m => m.id === entityId && m.isAlive());
          if (monster) {
            // Bleed ignores defense (damage applied directly via takeDamage)
            monster.takeDamage(tick.damage, undefined, undefined, { isTick: true });
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
        for (const type of expired) {
          EventBus.emit(GameEvents.LOG_MESSAGE, {
            text: t('zone.statusEffect.expired', { effectName: getStatusEffectName(type) }),
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

    const spriteKey = `npc_mercenary_${merc.type}`;
    const hasGeneratedVisual = this.addGeneratedNPCVisual(this.mercenarySprite, spriteKey, 'working') !== null;

    const colorMap: Record<string, number> = {
      tank: 0x2471a3, melee: 0xc0392b, ranged: 0x27ae60, healer: 0xf1c40f, mage: 0x8e44ad,
    };
    const color = colorMap[merc.type] ?? 0x888888;
    if (!hasGeneratedVisual) {
      const body = this.add.rectangle(0, -20, 32, 40, color);
      body.setStrokeStyle(1.5, 0xffffff, 0.6);
      const shadow = this.add.ellipse(0, 4, 30, 8, 0x000000, 0.25);
      this.mercenarySprite.add([shadow, body]);
      this.mercenarySprite.sendToBack(shadow);
    }

    // Friendly indicator (small green diamond above)
    const indicator = this.add.rectangle(0, -64, 6, 6, 0x27ae60);
    indicator.setAngle(45);
    this.mercenarySprite.add(indicator);

    // HP bar
    this.mercenaryHpBarBg = this.add.rectangle(0, -74, 32, 4, 0x1a1a1a);
    this.mercenaryHpBarBg.setStrokeStyle(0.5, 0x333333);
    this.mercenarySprite.add(this.mercenaryHpBarBg);

    this.mercenaryHpBar = this.add.rectangle(-16, -74, 32, 4, 0x27ae60);
    this.mercenaryHpBar.setOrigin(0, 0.5);
    this.mercenarySprite.add(this.mercenaryHpBar);

    // Name label
    this.mercenaryNameLabel = this.add.text(0, -86, `${def.name} Lv.${merc.level}`, {
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
    const safeRadiusSq = safeRadius * safeRadius;
    let inSafeZone = false;
    for (const camp of this.campPositions) {
      if (distanceSq(merc.tileCol, merc.tileRow, camp.col, camp.row) < safeRadiusSq) {
        inSafeZone = true;
        break;
      }
    }

    // Gather nearby monster info via spatial index
    let nearestMonsterCol: number | null = null;
    let nearestMonsterRow: number | null = null;
    let nearestMonsterDist = Infinity;
    const monstersInRange: { col: number; row: number; dist: number }[] = [];

    const nearbyMonsters = this.monsterGrid.queryRadius(this.player.tileCol, this.player.tileRow, 10);
    for (const m of nearbyMonsters) {
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
      this.mercenaryHpBar.width = Math.max(0, 32 * hpRatio);
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
            target.takeDamage(result.damage, this.mercenarySprite?.x ?? 0, this.mercenarySprite?.y ?? 0, { isCrit: result.isCrit });
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
          const aoeMonsters = this.monsterGrid.queryRadius(action.targetCol, action.targetRow, radius);
          for (const m of aoeMonsters) {
            if (!m.isAlive()) continue;
            const result = this.combatSystem.calculateDamage(entity, m.toCombatEntity());
            const dmg = Math.floor(result.damage * 0.7); // AoE damage reduction
            m.takeDamage(dmg, this.mercenarySprite?.x ?? 0, this.mercenarySprite?.y ?? 0);
            this.showDamageText(m.sprite.x, m.sprite.y, dmg, result.isCrit);
            if (!m.isAlive()) this.onMonsterKilled(m);
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
            text: t('zone.mercenary.heal', { mercName: getMercenaryName(merc.type, MERCENARY_DEFS[merc.type].name), amount: healAmount }),
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

    // Use spatial pre-filter: only check monsters near the mercenary (within ~12 tiles)
    const nearbyMercMonsters = this.monsterGrid.queryRadius(merc.tileCol, merc.tileRow, 12);
    for (const monster of nearbyMercMonsters) {
      if (!monster.isAlive() || monster.state !== 'attack') continue;
      if (this.statusEffects.isImmobilized(monster.id)) continue;

      const distToMercSq = distanceSq(monster.tileCol, monster.tileRow, merc.tileCol, merc.tileRow);
      const distToPlayerSq = distanceSq(monster.tileCol, monster.tileRow, this.player.tileCol, this.player.tileRow);

      // If mercenary is closer and within monster's attack range, monster hits mercenary
      if (distToMercSq < distToPlayerSq && distToMercSq <= (monster.definition.attackRange + 0.5) * (monster.definition.attackRange + 0.5)) {
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

  /**
   * Escort NPCs and defend targets used to spawn only on zone entry, so a
   * quest accepted in camp had nothing to escort/defend until the player left
   * and came back — the quests could not be finished. Spawn them on accept.
   */
  private handleQuestAcceptedWorld(data: { questId: string }): void {
    if (this.isInDungeon) return;
    const quest = this.questSystem.quests.get(data.questId);
    if (!quest || quest.zone !== this.currentMapId) return;
    if (quest.type === 'escort' && !this.escortQuestId) this.spawnEscortNpc();
    if (quest.type === 'defend' && !this.defendQuestId) this.spawnDefendTarget();
    this.spawnQuestHunts(false);
  }

  /** A tracked-down hunt appears once the objectives before it are done. */
  private handleQuestProgressWorld(): void {
    if (this.isInDungeon) return;
    this.spawnQuestHunts(true);
  }

  /** Put every due quest hunt (and its pack) into the world. */
  private spawnQuestHunts(announce: boolean): void {
    if (this.isInDungeon) return;
    for (const [id, m] of this.questHuntMonsters) {
      if (!m.isAlive()) this.questHuntMonsters.delete(id);
    }
    const due = huntsToSpawn(this.questSystem.getActiveQuests(), this.currentMapId, new Set(this.questHuntMonsters.keys()));
    const zoneDefs = MonstersByZone[this.currentMapId] || [];
    const baseOf = (id: string) => zoneDefs.find(m => m.id === id) || getMonsterDef(id);
    for (const { hunt } of due) {
      const base = baseOf(hunt.monsterId);
      if (!base) continue;
      const spot = this.mapData.collisions[hunt.row]?.[hunt.col]
        ? { col: hunt.col, row: hunt.row }
        : this.findWalkableNear(hunt.col, hunt.row, 6);
      if (!spot) continue;
      const def = DifficultySystem.scaleMonster(makeHuntDefinition(base, hunt, getMonsterName(hunt.huntId, hunt.name)), this.difficulty);
      const monster = new Monster(this, def, spot.col, spot.row);
      const affixes = this.eliteAffixSystem.rollAffixes(this.currentMapId, true);
      if (affixes.length > 0) monster.applyEliteAffixes(affixes, this.eliteAffixSystem);
      // A head taller than its kin.
      const body = monster.sprite.list.find(o => o instanceof Phaser.GameObjects.Sprite) as Phaser.GameObjects.Sprite | undefined;
      body?.setScale(body.scaleX * 1.25);
      this.monsters.push(monster);
      this.monsterGrid.insert(monster);
      this.questSpawned.add(monster);
      this.questHuntMonsters.set(hunt.huntId, monster);

      const minionBase = hunt.minions ? baseOf(hunt.minions.monsterId) : undefined;
      if (hunt.minions && minionBase) {
        const minionDef = DifficultySystem.scaleMonster(minionBase, this.difficulty);
        for (let i = 0; i < hunt.minions.count; i++) {
          const c = spot.col + randomInt(-3, 3);
          const r = spot.row + randomInt(-3, 3);
          if (!this.mapData.collisions[r]?.[c]) continue;
          const minion = new Monster(this, minionDef, c, r);
          this.monsters.push(minion);
          this.monsterGrid.insert(minion);
          this.questSpawned.add(minion);
        }
      }

      if (announce) {
        EventBus.emit(GameEvents.LOG_MESSAGE, {
          text: t('zone.quest.huntRevealed', { name: def.name }),
          type: 'system',
        });
        this.cameras.main.shake(260, 0.004);
      }
    }
  }

  /** Nearest walkable tile within `radius` rings of (col, row), skipping the tile itself. */
  private findWalkableNear(col: number, row: number, radius: number): { col: number; row: number } | null {
    for (let r = 1; r <= radius; r++) {
      for (let dr = -r; dr <= r; dr++) {
        for (let dc = -r; dc <= r; dc++) {
          if (Math.max(Math.abs(dc), Math.abs(dr)) !== r) continue;
          if (this.mapData.collisions[row + dr]?.[col + dc]) return { col: col + dc, row: row + dr };
        }
      }
    }
    return null;
  }

  /** Tile the escort NPC stands on (for the quest guide), or null. */
  getEscortTile(): { col: number; row: number } | null {
    return this.escortQuestId ? { col: this.escortNpcTileCol, row: this.escortNpcTileRow } : null;
  }

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
      this.escortJoined = false;
      this.escortPath = [];

      // HP scales with zone level
      const baseHp = quest.level * 20 + 100;
      this.escortNpcHp = baseHp;
      this.escortNpcMaxHp = baseHp;

      const worldPos = cartToIso(this.escortNpcTileCol, this.escortNpcTileRow);
      this.escortNpcSprite = this.add.container(worldPos.x, worldPos.y);
      this.escortNpcSprite.setDepth(worldPos.y + 60);

      if (!this.addGeneratedNPCVisual(this.escortNpcSprite, en.spriteKey, 'working')) {
        const shadow = this.add.ellipse(0, 4, 26, 7, 0x000000, 0.25);
        const body = this.add.rectangle(0, -20, 28, 36, 0xe67e22);
        body.setStrokeStyle(1.5, 0xffffff, 0.6);
        this.escortNpcSprite.add([shadow, body]);
      }

      // HP bar
      this.escortNpcHpBarBg = this.add.rectangle(0, -74, 32, 4, 0x1a1a1a);
      this.escortNpcHpBarBg.setStrokeStyle(0.5, 0x333333);
      this.escortNpcSprite.add(this.escortNpcHpBarBg);

      this.escortNpcHpBar = this.add.rectangle(-16, -74, 32, 4, 0x27ae60);
      this.escortNpcHpBar.setOrigin(0, 0.5);
      this.escortNpcSprite.add(this.escortNpcHpBar);

      // Name label
      this.escortNpcNameLabel = this.add.text(0, -86, en.name, {
        fontSize: fs(10), color: '#e67e22', fontFamily: '"Noto Sans SC", sans-serif',
        stroke: '#000000', strokeThickness: Math.round(2 * DPR),
      }).setOrigin(0.5);
      this.escortNpcSprite.add(this.escortNpcNameLabel);

      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.escort.npcAppeared', { npcName: en.name }), type: 'system' });
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
  private updateEscortNpc(time: number, delta: number): void {
    if (!this.escortNpcSprite || !this.escortQuestId) return;

    // Follow the player along a real path (a straight line got stuck on
    // walls, water and rocks). Re-plan a few times a second.
    const dx = this.player.tileCol - this.escortNpcTileCol;
    const dy = this.player.tileRow - this.escortNpcTileRow;
    const dist = Math.sqrt(dx * dx + dy * dy);
    if (!this.escortJoined) {
      if (dist > 5) {
        this.syncEscortSprite();
        return;
      }
      this.escortJoined = true;
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.escort.joined', { npcName: this.questSystem.quests.get(this.escortQuestId)?.escortNpc?.name ?? '' }), type: 'system' });
    }
    if (dist > ESCORT_CATCH_UP_TILES) {
      // Left far behind (player ran off, got blocked): catch up out of sight.
      const spot = this.findWalkableNear(Math.round(this.player.tileCol), Math.round(this.player.tileRow), 2);
      if (spot) { this.escortNpcTileCol = spot.col; this.escortNpcTileRow = spot.row; this.escortPath = []; }
    } else if (dist > 2) {
      if (time >= this.escortRepathAt || this.escortPath.length === 0) {
        this.escortRepathAt = time + 400;
        const path = this.pathfinding.findPath(
          Math.round(this.escortNpcTileCol), Math.round(this.escortNpcTileRow),
          Math.round(this.player.tileCol), Math.round(this.player.tileRow),
        );
        // Stop a tile or two short of the player.
        this.escortPath = path.slice(1, Math.max(1, path.length - 1));
      }
      // Tiles per second, a touch slower than the hero.
      let budget = (this.player.moveSpeed / 38) * 0.9 * (delta / 1000);
      while (budget > 0 && this.escortPath.length > 0) {
        const next = this.escortPath[0];
        const sx = next.col - this.escortNpcTileCol;
        const sy = next.row - this.escortNpcTileRow;
        const sd = Math.sqrt(sx * sx + sy * sy);
        if (sd <= budget) {
          this.escortNpcTileCol = next.col;
          this.escortNpcTileRow = next.row;
          this.escortPath.shift();
          budget -= sd;
        } else {
          this.escortNpcTileCol += (sx / sd) * budget;
          this.escortNpcTileRow += (sy / sd) * budget;
          budget = 0;
        }
      }
    } else {
      this.escortPath = [];
    }

    // Nearby monsters attack escort NPC (aggro if within 4 tiles)
    for (const monster of this.monsters) {
      if (!monster.isAlive()) continue;
      const md = distanceSq(monster.tileCol, monster.tileRow, this.escortNpcTileCol, this.escortNpcTileRow);
      // (lastEscortAttack starts undefined: `time - undefined` is NaN and never passed, so escorts were invulnerable.)
      if (md < 16 && monster.isAggro() && time - ((monster as unknown as { lastEscortAttack?: number }).lastEscortAttack ?? -Infinity) > 2000) {
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

    this.syncEscortSprite();

    // Update HP bar
    if (this.escortNpcHpBar) {
      const hpRatio = this.escortNpcHp / this.escortNpcMaxHp;
      this.escortNpcHpBar.width = Math.max(0, 32 * hpRatio);
      this.escortNpcHpBar.fillColor = hpRatio > 0.5 ? 0x27ae60 : hpRatio > 0.25 ? 0xf39c12 : 0xe74c3c;
    }

    // Check if escort NPC has arrived at destination (gate on NPC proximity, not just player)
    const escortDx = this.escortNpcTileCol - this.escortDestCol;
    const escortDy = this.escortNpcTileRow - this.escortDestRow;
    const escortDistSq = escortDx * escortDx + escortDy * escortDy;
    if (escortDistSq <= 25) {
      // Also check player is nearby
      const playerDistSq = distanceSq(this.player.tileCol, this.player.tileRow, this.escortDestCol, this.escortDestRow);
      if (playerDistSq <= 36) {
        // Escort complete
        const quest = this.questSystem.quests.get(this.escortQuestId);
        if (quest) {
          for (const obj of quest.objectives) {
            if (obj.type === 'escort') {
              this.questSystem.updateProgress('escort', obj.targetId);
            }
          }
          EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.escort.complete', { npcName: quest.escortNpc?.name ?? '' }), type: 'system' });
        }
        this.destroyEscortNpc();
      }
    }
  }

  private syncEscortSprite(): void {
    if (!this.escortNpcSprite) return;
    const worldPos = cartToIso(this.escortNpcTileCol, this.escortNpcTileRow);
    this.escortNpcSprite.setPosition(worldPos.x, worldPos.y);
    this.escortNpcSprite.setDepth(worldPos.y + 60);
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
    EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.escort.npcDied'), type: 'system' });
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

      const glow = this.add.ellipse(0, 2, 68, 22, 0xe74c3c, 0.24);
      this.defendTargetSprite.add(glow);
      this.defendTargetSprite.sendToBack(glow);
      SpriteGenerator.ensureDecoration(this, dt.spriteKey);
      if (this.textures.exists(dt.spriteKey)) {
        const targetVisual = this.add.image(0, -34, dt.spriteKey).setScale(1 / TEXTURE_SCALE);
        this.defendTargetSprite.add(targetVisual);
      } else {
        const base = this.add.rectangle(0, -16, 40, 48, 0x8b4513);
        base.setStrokeStyle(2, 0xc0392b);
        this.defendTargetSprite.add(base);
      }

      // HP bar
      this.defendTargetHpBarBg = this.add.rectangle(0, -78, 40, 5, 0x1a1a1a);
      this.defendTargetHpBarBg.setStrokeStyle(0.5, 0x333333);
      this.defendTargetSprite.add(this.defendTargetHpBarBg);

      this.defendTargetHpBar = this.add.rectangle(-20, -78, 40, 5, 0xe74c3c);
      this.defendTargetHpBar.setOrigin(0, 0.5);
      this.defendTargetSprite.add(this.defendTargetHpBar);

      // Name label
      this.defendTargetNameLabel = this.add.text(0, -90, dt.name, {
        fontSize: fs(10), color: '#e74c3c', fontFamily: '"Noto Sans SC", sans-serif',
        stroke: '#000000', strokeThickness: Math.round(2 * DPR),
      }).setOrigin(0.5);
      this.defendTargetSprite.add(this.defendTargetNameLabel);

      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.defend.targetNeedsProtection', { targetName: dt.name }), type: 'system' });
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
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.defend.allWavesCleared'), type: 'system' });
      this.destroyDefendTarget();
      return;
    }

    const playerDistSq = distanceSq(this.player.tileCol, this.player.tileRow, this.defendTargetCol, this.defendTargetRow);

    // Start next wave when player is within 15 tiles and no wave is active
    if (!this.defendWaveActive && playerDistSq < 225) {
      if (this.defendWaveTimer === 0) {
        this.defendWaveTimer = time;
      }
      // 5-second delay between waves
      if (time - this.defendWaveTimer > 5000) {
        this.spawnDefendWave(this.defendCurrentWave);
        this.defendWaveActive = true;
        this.defendWaveTimer = 0;
        EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.defend.waveIncoming', { current: this.defendCurrentWave + 1, total: this.defendTotalWaves }), type: 'system' });
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
          const md = distanceSq(monster.tileCol, monster.tileRow, this.defendTargetCol, this.defendTargetRow);
          if (md < 9 && time - ((monster as unknown as { lastDefendAttack?: number }).lastDefendAttack ?? 0) > 2000) {
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
      this.defendTargetHpBar.width = Math.max(0, 40 * hpRatio);
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
      this.monsterGrid.insert(monster);
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
    EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.defend.targetDestroyed'), type: 'system' });
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
              EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.craft.complete', { targetName: getQuestTargetName(craftObj.targetId, craftObj.targetName) }), type: 'system' });
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
              EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('zone.deliver.complete', { targetName: getQuestTargetName(deliverObj.targetId, deliverObj.targetName) }), type: 'system' });
            }
          }
        }
      }
    }
  }

  /** Find the nearest alive monster to a given tile position. */
  private findMonsterNear(col: number, row: number): Monster | null {
    return this.monsterGrid.findNearest(col, row, 3, m => m.isAlive());
  }

  // ---------------------------------------------------------------------------
  // ─── Ley-beast (PetCompanion) ───────────────────────────────────────────────
  // ---------------------------------------------------------------------------

  /** Create this zone's ley-beast runtime (follows, fights, soaks, revives). */
  private createPetCompanion(): void {
    this.petCompanion?.destroy();
    this.petCompanion = new PetCompanion({
      scene: this,
      pets: this.petSystem,
      player: this.player,
      vfx: () => this.vfx ?? null,
      skillEffects: this.skillEffects,
      statusEffects: this.statusEffects,
      isWalkable: (col, row) => !!this.mapData.collisions[row]?.[col],
      monstersNear: (col, row, radius) => this.monsterGrid.queryRadius(col, row, radius),
      findMonster: (id) => this.monsters.find(m => m.id === id),
      heroDamage: () => this.player.baseDamage + (this.getEquipStats().damage ?? 0),
      inSafeZone: (col, row) => {
        const r = this.mapData.safeZoneRadius ?? 9;
        return this.campPositions.some(c => distanceSq(col, row, c.col, c.row) < r * r);
      },
      isPaused: () => this.isTransitioning || !!this.storyDirector?.cinematic,
      showDamage: (x, y, amount, isCrit, onHero, damageType) => this.showDamageText(x, y, amount, isCrit, false, onHero, damageType),
      onMonsterKilled: (m) => {
        this.onMonsterKilled(m);
        if (this.player.attackTarget === m.id) {
          this.player.attackTarget = null;
          EventBus.emit(GameEvents.TARGET_CHANGED, { targetId: null, targetName: null });
        }
      },
    });
  }

  /** Rebuild the ley-beast's look (active beast changed / evolved). */
  spawnPetSprite(): void {
    this.petCompanion?.refresh();
  }

  /**
   * The hero hit 0 HP: a beast with 濒死复燃 may rekindle them (once per zone).
   * Returns true if the hero actually died.
   */
  private killPlayer(): boolean {
    if (this.petCompanion?.tryReviveHero()) return false;
    this.player.die();
    return true;
  }

  shutdown(): void {
    this.storyDirector?.destroy();
    this.storyDirector = null;
    this.emberTower?.destroy();
    this.emberTower = null;
    this.questWorld?.destroy();
    this.questWorld = null;
    this.isTransitioning = false;
    this.isPortaling = false;
    this.destroyMercenarySprite();
    this.petCompanion?.destroy();
    this.petCompanion = null;
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
    this.subscriptions.dispose();
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
    this.terrain?.destroy();
    this.terrain = null;
    this.visibleTiles.clear();
    for (const tile of this.tilePool) tile.destroy();
    this.tilePool = [];
    this.lastVisibleTileBounds = '';
    for (const sprite of this.decorSprites.values()) sprite.destroy();
    this.decorSprites.clear();
    this.occluderDecor.clear();
    for (const sprite of this.exitSprites.values()) sprite.destroy();
    this.exitSprites.clear();
    for (const label of this.exitLabels.values()) label.destroy();
    this.exitLabels.clear();
    for (const npc of this.npcs) npc.destroy();
    this.npcs = [];
    for (const monster of this.monsters) {
      if (monster.sprite.active) monster.sprite.destroy();
    }
    this.monsters = [];
    this.monsterGrid.clear();
    if (this.player?.sprite?.active) this.player.sprite.destroy();
    for (const loot of this.lootDrops) loot.sprite.destroy();
    this.lootDrops = [];
    for (const potion of this.potionDrops) potion.sprite.destroy();
    this.potionDrops = [];
    // Clean up floating text pool
    for (const t of this.floatingTextPool) t.destroy();
    this.floatingTextPool = [];
    this.damageTextStacks.clear();
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
