import Phaser from 'phaser';
import { TILE_WIDTH, TILE_HEIGHT, GAME_WIDTH, GAME_HEIGHT, TEXTURE_SCALE, DPR } from '../config';
import { cartToIso, isoToCart, worldToTile, euclideanDistance } from '../utils/IsometricUtils';
import { randomInt } from '../utils/MathUtils';
import { EventBus, GameEvents } from '../utils/EventBus';
import { Player } from '../entities/Player';
import { Monster } from '../entities/Monster';
import { NPC } from '../entities/NPC';
import { PathfindingSystem } from '../systems/PathfindingSystem';
import { CombatSystem } from '../systems/CombatSystem';
import { LootSystem } from '../systems/LootSystem';
import { InventorySystem } from '../systems/InventorySystem';
import { QuestSystem } from '../systems/QuestSystem';
import { HomesteadSystem } from '../systems/HomesteadSystem';
import { AchievementSystem } from '../systems/AchievementSystem';
import { SaveSystem } from '../systems/SaveSystem';
import { SkillEffectSystem } from '../systems/SkillEffectSystem';
import { MobileControlsSystem, isMobileDevice } from '../systems/MobileControlsSystem';
import { LightingSystem } from '../systems/LightingSystem';
import { VFXManager } from '../systems/VFXManager';
import { WeatherSystem } from '../systems/WeatherSystem';
import { TrailRenderer } from '../systems/TrailRenderer';
import { audioManager } from '../systems/audio/AudioManager';
import { applyColorGrading } from '../graphics/ColorGradePipeline';
import { SpriteGenerator } from '../graphics/SpriteGenerator';
import { AllClasses } from '../data/classes/index';
import { AllMaps } from '../data/maps/index';
import { MonstersByZone, getMonsterDef } from '../data/monsters/index';
import { NPCDefinitions } from '../data/npcs';
import { AllQuests } from '../data/quests/all_quests';
import type { MapData, ClassDefinition, ItemInstance, SaveData } from '../data/types';

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
  private lastAutoLootCheck = 0;
  private totalKills = 0;
  private exploredZones: Set<string> = new Set();
  private fogData: Record<string, boolean[][]> = {};
  private lastTileUpdate = 0;
  private _pendingSaveData: SaveData | null = null;
  private mobileControls: MobileControlsSystem | null = null;
  private lighting!: LightingSystem;
  private lights_playerLight: import('../systems/LightingSystem').LightSource | null = null;
  private vfx!: VFXManager;
  private weather!: WeatherSystem;
  private trails!: TrailRenderer;
  private inCombat = false;
  private isTransitioning = false;
  private isPortaling = false;
  private combatDebounceTimer: ReturnType<typeof setTimeout> | null = null;
  private targetIndicator: Phaser.GameObjects.Ellipse | null = null;
  private currentTargetId: string | null = null;
  private ambientDustEmitter: Phaser.GameObjects.Particles.ParticleEmitter | null = null;
  private readonly contextMenuHandler = (e: Event): void => {
    e.preventDefault();
  };

  constructor() {
    super({ key: 'ZoneScene' });
  }

  init(data: { classId: string; mapId: string; saveData?: SaveData; playerStats?: any }): void {
    this.currentMapId = data.mapId || 'emerald_plains';
    if (!AllMaps[this.currentMapId]) this.currentMapId = 'emerald_plains';
    this.mapData = AllMaps[this.currentMapId];
    this.campPositions = this.mapData.camps.map(c => ({ col: c.col, row: c.row }));
    this._pendingSaveData = data.saveData ?? null;
  }

  create(data: { classId: string; mapId: string; saveData?: SaveData; playerStats?: any }): void {
    this.events.once(Phaser.Scenes.Events.SHUTDOWN, this.shutdown, this);
    // `scene.restart()` reuses the same scene instance, so transient guards must
    // be cleared explicitly when entering a new zone.
    this.isTransitioning = false;
    this.isPortaling = false;
    this.monsters = [];
    this.npcs = [];
    this.lootDrops = [];
    this.potionDrops = [];

    this.combatSystem = new CombatSystem();
    this.lootSystem = new LootSystem();
    this.skillEffects = new SkillEffectSystem(this);

    const isFirstLoad = !this.inventorySystem;
    if (isFirstLoad) {
      this.inventorySystem = new InventorySystem();
      this.questSystem = new QuestSystem();
      this.homesteadSystem = new HomesteadSystem();
      this.achievementSystem = new AchievementSystem();
      this.saveSystem = new SaveSystem();
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
    this.player = new Player(this, classData, this.mapData.playerStart.col, this.mapData.playerStart.row);
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

    // Restore from save (when loading from menu, not zone transitions)
    if (this._pendingSaveData) {
      this.restoreFromSave(this._pendingSaveData);
      this._pendingSaveData = null;
    }

    this.pathfinding = new PathfindingSystem(this.mapData.collisions, this.mapData.cols, this.mapData.rows);

    this.spawnMonsters();
    this.spawnNPCs();
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
      this.changeZone(exit.targetMap, exit.targetCol, exit.targetRow);
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
      const camp = this.campPositions[0];
      this.player.respawnAtCamp(camp.col, camp.row);
      this.cameras.main.fadeIn(300);
    });
  }

  private handlePlayerLevelUp(data: { level: number }): void {
    this.showLevelUpBanner(data.level);
  }

  private handleQuestCompleted(data: { questName: string }): void {
    this.showQuestCompleteBanner(data.questName);
  }

  private handleUiSkillClick(data: { index: number; skillId: string }): void {
    this.tryUseSkill(data.skillId, this.time.now);
  }

  update(time: number, delta: number): void {
    const recovery = this.getPlayerRecoveryModifiers();
    this.handleKeyboardMovement(delta);
    this.handleSkillInput(time);
    this.player.update(time, delta, recovery);

    const safeRadius = this.mapData.safeZoneRadius ?? 9;
    for (const monster of this.monsters) {
      if (!monster.isAlive()) continue;
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
      if (playerInSafe && !monster.isAggro()) {
        monster.update(time, delta, -999, -999, this.mapData.collisions);
      } else {
        monster.update(time, delta, this.player.tileCol, this.player.tileRow, this.mapData.collisions);
      }
    }

    // Update NPC state machines
    for (const npc of this.npcs) {
      npc.update(this.player.tileCol, this.player.tileRow);
    }

    this.handleCombat(time);
    this.updateCombatState();
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
    if (this.player.mana < skill.manaCost) {
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: `法力不足!`, type: 'combat' });
      return;
    }

    const target = this.findNearestAliveMonster();
    if (!target && !skill.buff) return;

    if (target && !skill.buff) {
      const dist = euclideanDistance(this.player.tileCol, this.player.tileRow, target.tileCol, target.tileRow);
      if (dist > skill.range + 1) return;
    }

    this.player.useSkill(skillId, time);
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
    const level = this.player.getSkillLevel(skillId);


    if (skill.buff) {
      this.player.buffs.push({ stat: skill.buff.stat, value: skill.buff.value + level * 0.02, duration: skill.buff.duration, startTime: time });
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: `${skill.name} 激活!`, type: 'combat' });
      this.skillEffects.play(skillId, this.player.sprite.x, this.player.sprite.y);
      // Heal burst for healing buffs, otherwise generic buff flash
      if (this.vfx) {
        if (skill.buff.stat === 'hp' || skillId.includes('heal')) {
          this.vfx.healBurst(this.player.sprite.x, this.player.sprite.y - 16, 10);
        }
      }
      return;
    }

    if (skill.aoe && skill.aoeRadius) {
      const aoeTargets = this.monsters.filter(m =>
        m.isAlive() && euclideanDistance(this.player.tileCol, this.player.tileRow, m.tileCol, m.tileRow) <= skill.aoeRadius!
      );
      for (const t of aoeTargets) {
        const result = this.combatSystem.calculateDamage(this.player.toCombatEntity(), t.toCombatEntity(), skill, level);
        t.takeDamage(result.damage, this.player.sprite.x, this.player.sprite.y);
        this.showDamageText(t.sprite.x, t.sprite.y, result.damage, result.isCrit, false, false, skill.damageType);
        if (!t.isAlive()) this.onMonsterKilled(t);
        // Bloom impact on AoE skill hit
        if (this.vfx && skill.damageType !== 'physical') {
          const impactColor = skillId.includes('fire') || skillId === 'meteor' ? 0xff6600
            : skillId.includes('ice') || skillId === 'blizzard' ? 0x4488ff
            : skillId.includes('lightning') || skillId === 'chain_lightning' ? 0x5dade2
            : 0xf39c12;
          this.vfx.skillImpactBloom(t.sprite.x, t.sprite.y - 16, impactColor);
        }
      }
      // Screen shake on AoE impact
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
      const result = this.combatSystem.calculateDamage(this.player.toCombatEntity(), target.toCombatEntity(), skill, level);
      target.takeDamage(result.damage, this.player.sprite.x, this.player.sprite.y);
      this.showDamageText(target.sprite.x, target.sprite.y, result.damage, result.isCrit, false, false, skill.damageType);
      if (!target.isAlive()) this.onMonsterKilled(target);
      this.skillEffects.play(skillId, this.player.sprite.x, this.player.sprite.y,
        target.sprite.x, target.sprite.y);
      // Bloom + ground scorch on skill hit
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
      if (time - monster.lastAttackTime >= monster.definition.attackSpeed) {
        monster.lastAttackTime = time;
        monster.playAttack(this.player.sprite.x, this.player.sprite.y);
        const result = this.combatSystem.calculateDamage(monster.toCombatEntity(), this.player.toCombatEntity());
        if (result.isDodged) {
          this.showDamageText(this.player.sprite.x, this.player.sprite.y, 0, false, true);
        } else {
          const diffMult = this.difficulty === 'hell' ? 2 : this.difficulty === 'nightmare' ? 1.5 : 1;
          const finalDmg = Math.floor(result.damage * diffMult);
          this.player.hp = Math.max(0, this.player.hp - finalDmg);
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
          if (this.player.hp <= 0) { this.player.die(); break; }
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
        const result = this.combatSystem.calculateDamage(this.player.toCombatEntity(), target.toCombatEntity());
        target.takeDamage(result.damage, this.player.sprite.x, this.player.sprite.y);
        this.showDamageText(target.sprite.x, target.sprite.y, result.damage, result.isCrit);
        this.skillEffects.playAttack(this.player.sprite.x, this.player.sprite.y, target.sprite.x, target.sprite.y, true);
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
      if (this.player.isSkillReady(skillId, time) && this.player.mana >= skill.manaCost) {
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

  private handleAutoLoot(): void {
    const qualityRank: Record<string, number> = { normal: 0, magic: 1, rare: 2, legendary: 3, set: 3 };
    const minRank = this.player.autoLootMode === 'all' ? 0 : this.player.autoLootMode === 'magic' ? 1 : 2;
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
          if (!this.questSystem.progress.has(q.id)) {
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
      }
    }
  }

  private onMonsterKilled(monster: Monster): void {
    const diffMult = this.difficulty === 'hell' ? 3 : this.difficulty === 'nightmare' ? 2 : 1;
    const homeBonus = this.homesteadSystem.getTotalBonuses();
    const expBonus = 1 + (homeBonus['expBonus'] ?? 0) / 100;
    const exp = Math.floor(monster.definition.expReward * diffMult * expBonus);
    const gold = randomInt(monster.definition.goldReward[0], monster.definition.goldReward[1]) * diffMult;
    this.player.addExp(exp);
    this.player.gold += gold;

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

    const luckBonus = this.player.stats.lck + (homeBonus['magicFind'] ?? 0);
    const loot = this.lootSystem.generateLoot(monster.definition, luckBonus);
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

    this.time.delayedCall(15000, () => this.respawnMonster(monster));
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
            }
          }
        }

        // Build dialogue actions for available quests
        const actions: { label: string; callback: () => void }[] = [];
        if (def.quests) {
          const available = this.questSystem.getAvailableQuests(def.quests, this.player.level);
          for (const q of available) {
            const prog = this.questSystem.progress.get(q.id);
            if (!prog) {
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
        this.changeZone(exit.targetMap, exit.targetCol, exit.targetRow);
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
      await this.saveSystem.autoSave({
        id: 'autosave',
        version: 1,
        timestamp: Date.now(),
        classId: this.player.classData.id,
        player: {
          level: this.player.level, exp: this.player.exp, gold: this.player.gold,
          hp: this.player.hp, maxHp: this.player.maxHp, mana: this.player.mana, maxMana: this.player.maxMana,
          stats: { ...this.player.stats }, freeStatPoints: this.player.freeStatPoints, freeSkillPoints: this.player.freeSkillPoints,
          skillLevels: Object.fromEntries(this.player.skillLevels),
          tileCol: this.player.tileCol, tileRow: this.player.tileRow, currentMap: this.currentMapId,
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
        completedDifficulties: [],
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
    this.player.moveTo(save.player.tileCol, save.player.tileRow);

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
      this.homesteadSystem.pets = save.homestead.pets ?? [];
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
  }

  private spawnMonsters(): void {
    const monsterDefs = MonstersByZone[this.currentMapId] || [];
    const safeRadius = this.mapData.safeZoneRadius ?? 9;
    for (const spawn of this.mapData.spawns) {
      const def = monsterDefs.find(m => m.id === spawn.monsterId) || getMonsterDef(spawn.monsterId);
      if (!def) continue;
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
          this.monsters.push(new Monster(this, def, c, r));
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
    this.monsters[idx] = new Monster(this, dead.definition, c, r);
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
    const camp = this.campPositions[0];
    if (!camp) return;

    // Check if player is already inside the main camp safe zone
    const safeRadius = this.mapData.safeZoneRadius ?? 9;
    if (euclideanDistance(this.player.tileCol, this.player.tileRow, camp.col, camp.row) < safeRadius) {
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: '你已在营地中，无法使用传送门。', type: 'system' });
      return;
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

    this.tweens.add({
      targets: ring, radius: 28, alpha: 0.7, duration: 1200, ease: 'Sine.easeOut',
    });
    this.tweens.add({
      targets: ring2, radius: 20, alpha: 0.5, duration: 1200, ease: 'Sine.easeOut', delay: 200,
    });
    this.tweens.add({
      targets: glow, radius: 30, alpha: 0.3, duration: 1200, ease: 'Sine.easeOut',
    });

    // Player flicker during cast
    this.tweens.add({
      targets: this.player.sprite, alpha: 0.5, duration: 200, yoyo: true, repeat: 3,
    });

    this.time.delayedCall(1500, () => {
      // Flash & teleport
      if (this.vfx) this.vfx.cameraFlash(200, 0.5, 0x4488ff);
      audioManager.playSFX('zone_transition');

      ring.destroy();
      ring2.destroy();
      glow.destroy();

      this.player.moveTo(camp.col, camp.row);
      this.player.sprite.setAlpha(1);
      this.isPortaling = false;

      EventBus.emit(GameEvents.LOG_MESSAGE, { text: '已传送回营地。', type: 'system' });
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

  shutdown(): void {
    this.isTransitioning = false;
    this.isPortaling = false;
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
