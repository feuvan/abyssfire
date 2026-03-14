import Phaser from 'phaser';
import { TILE_WIDTH, TILE_HEIGHT, GAME_WIDTH, GAME_HEIGHT } from '../config';
import { cartToIso, worldToTile, euclideanDistance } from '../utils/IsometricUtils';
import { randomInt } from '../utils/MathUtils';
import { EventBus, GameEvents } from '../utils/EventBus';
import { Player } from '../entities/Player';
import { Monster } from '../entities/Monster';
import { NPC } from '../entities/NPC';
import { PathfindingSystem } from '../systems/PathfindingSystem';
import { FogOfWarSystem } from '../systems/FogOfWarSystem';
import { CombatSystem } from '../systems/CombatSystem';
import { LootSystem } from '../systems/LootSystem';
import { InventorySystem } from '../systems/InventorySystem';
import { QuestSystem } from '../systems/QuestSystem';
import { HomesteadSystem } from '../systems/HomesteadSystem';
import { AchievementSystem } from '../systems/AchievementSystem';
import { SaveSystem } from '../systems/SaveSystem';
import { SkillEffectSystem } from '../systems/SkillEffectSystem';
import { audioSystem } from '../systems/AudioSystem';
import { AllClasses } from '../data/classes/index';
import { AllMaps } from '../data/maps/index';
import { MonstersByZone, getMonsterDef } from '../data/monsters/index';
import { NPCDefinitions } from '../data/npcs';
import { AllQuests } from '../data/quests/all_quests';
import type { MapData, ClassDefinition, ItemInstance, SaveData } from '../data/types';

const TILE_KEYS = ['tile_grass', 'tile_dirt', 'tile_stone', 'tile_water', 'tile_wall', 'tile_camp'];

export class ZoneScene extends Phaser.Scene {
  player!: Player;
  private monsters: Monster[] = [];
  private npcs: NPC[] = [];
  private mapData!: MapData;
  private currentMapId!: string;
  private pathfinding!: PathfindingSystem;
  private fogOfWar!: FogOfWarSystem;
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
  private visibleTiles: Set<string> = new Set();
  private cursors!: Phaser.Types.Input.Keyboard.CursorKeys;
  private wasd!: Record<string, Phaser.Input.Keyboard.Key>;
  private campPositions: { col: number; row: number }[] = [];
  private lootDrops: { sprite: Phaser.GameObjects.Container; item: ItemInstance; col: number; row: number }[] = [];
  private difficulty: 'normal' | 'nightmare' | 'hell' = 'normal';
  private autoLootMode: 'off' | 'all' | 'magic' | 'rare' = 'off';
  private lastAutoLootCheck = 0;
  private totalKills = 0;
  private exploredZones: Set<string> = new Set();
  private fogData: Record<string, boolean[][]> = {};
  private lastTileUpdate = 0;
  private _pendingSaveData: SaveData | null = null;

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
    this.monsters = [];
    this.npcs = [];
    this.lootDrops = [];

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
      this.player.skillLevels = new Map(s.skillLevels);
      this.player.buffs = s.buffs;
      this.player.autoCombat = s.autoCombat;
    }
    this.player.recalcDerived();

    // Restore from save (first load only, not zone transitions)
    if (isFirstLoad && this._pendingSaveData) {
      this.restoreFromSave(this._pendingSaveData);
      this._pendingSaveData = null;
    }

    this.pathfinding = new PathfindingSystem(this.mapData.collisions, this.mapData.cols, this.mapData.rows);

    this.fogOfWar = new FogOfWarSystem(this, this.mapData.cols, this.mapData.rows, 10);
    if (this.fogData[this.currentMapId]) {
      this.fogOfWar.loadExploredData(this.fogData[this.currentMapId]);
    }
    this.fogOfWar.update(this.player.tileCol, this.player.tileRow);

    this.spawnMonsters();
    this.spawnNPCs();

    // Initial tile render
    this.updateVisibleTiles();

    // Camera
    this.cameras.main.startFollow(this.player.sprite, true, 0.08, 0.08);
    this.cameras.main.setZoom(1.8);

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
      };
    }

    this.input.on('pointerdown', (pointer: Phaser.Input.Pointer) => {
      if (pointer.rightButtonDown()) return;
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
    });

    if (!this.scene.isActive('UIScene')) {
      this.scene.launch('UIScene', { player: this.player, zone: this });
    } else {
      EventBus.emit('ui:refresh', { player: this.player, zone: this });
    }

    EventBus.removeAllListeners(GameEvents.PLAYER_DIED);
    EventBus.on(GameEvents.PLAYER_DIED, () => {
      this.time.delayedCall(2000, () => {
        const camp = this.campPositions[0];
        this.player.respawnAtCamp(camp.col, camp.row);
      });
    });

    EventBus.removeAllListeners(GameEvents.UI_SKILL_CLICK);
    EventBus.on(GameEvents.UI_SKILL_CLICK, (data: { index: number; skillId: string }) => {
      this.tryUseSkill(data.skillId, this.time.now);
    });

    EventBus.removeAllListeners(GameEvents.AUTOLOOT_CHANGED);
    EventBus.on(GameEvents.AUTOLOOT_CHANGED, (data: { mode: 'off' | 'all' | 'magic' | 'rare' }) => {
      this.autoLootMode = data.mode;
    });

    this.exploredZones.add(this.currentMapId);
    this.achievementSystem.update('explore', this.currentMapId);

    EventBus.emit(GameEvents.ZONE_ENTERED, { mapId: this.currentMapId });
    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: `进入 ${this.mapData.name} (Lv.${this.mapData.levelRange[0]}-${this.mapData.levelRange[1]})`,
      type: 'system',
    });

    this.autoSave();
  }

  update(time: number, delta: number): void {
    this.handleKeyboardMovement(delta);
    this.handleSkillInput(time);
    this.player.update(time, delta);

    for (const monster of this.monsters) {
      if (monster.isAlive()) {
        monster.update(time, delta, this.player.tileCol, this.player.tileRow, this.mapData.collisions);
      }
    }

    this.handleCombat(time);

    if (this.player.autoCombat) this.handleAutoCombat(time);

    // Auto-loot
    if (this.autoLootMode !== 'off' && time - this.lastAutoLootCheck > 300) {
      this.lastAutoLootCheck = time;
      this.handleAutoLoot();
    }

    this.checkExitProximity();

    // Throttled viewport tile update
    if (time - this.lastTileUpdate > 100) {
      this.lastTileUpdate = time;
      this.updateVisibleTiles();
    }

    // Throttled fog update
    if (Math.floor(time / 200) !== Math.floor((time - delta) / 200)) {
      this.fogOfWar.update(Math.round(this.player.tileCol), Math.round(this.player.tileRow));
    }

    // Throttled explore quest check + NPC quest marker update
    if (Math.floor(time / 500) !== Math.floor((time - delta) / 500)) {
      this.checkExploreQuests();
      this.updateNPCQuestMarkers();
    }

    EventBus.emit(GameEvents.PLAYER_HEALTH_CHANGED, { hp: this.player.hp, maxHp: this.player.maxHp });
    EventBus.emit(GameEvents.PLAYER_MANA_CHANGED, { mana: this.player.mana, maxMana: this.player.maxMana });
  }

  // --- Viewport culling tile rendering ---
  private updateVisibleTiles(): void {
    const cam = this.cameras.main;
    const camCX = cam.scrollX + cam.width / 2 / cam.zoom;
    const camCY = cam.scrollY + cam.height / 2 / cam.zoom;
    const viewW = cam.width / cam.zoom / 2;
    const viewH = cam.height / cam.zoom / 2;
    const margin = 4;

    const newVisible = new Set<string>();

    for (let row = 0; row < this.mapData.rows; row++) {
      for (let col = 0; col < this.mapData.cols; col++) {
        const pos = cartToIso(col, row);
        const dx = Math.abs(pos.x - camCX);
        const dy = Math.abs(pos.y - camCY);
        if (dx < viewW + TILE_WIDTH * margin && dy < viewH + TILE_HEIGHT * margin) {
          const key = `${col},${row}`;
          newVisible.add(key);
          if (!this.tileSprites[row][col]) {
            const tileType = this.mapData.tiles[row][col];
            const tileKey = TILE_KEYS[tileType] || 'tile_grass';
            const tile = this.add.image(pos.x, pos.y, tileKey);
            tile.setDepth(pos.y);
            this.tileSprites[row][col] = tile;

            if (tileType === 5) {
              const flag = this.add.rectangle(pos.x, pos.y - 12, 3, 10, 0xf1c40f);
              flag.setDepth(pos.y + 1);
            }

            const exit = this.mapData.exits.find(e => e.col === col && e.row === row);
            if (exit) {
              if (this.textures.exists('exit_portal')) {
                const portal = this.add.image(pos.x, pos.y - 8, 'exit_portal');
                portal.setDepth(pos.y + 2);
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
      }
    }
    this.visibleTiles = newVisible;

    // Update decorations visibility
    this.updateVisibleDecorations();
  }

  private updateVisibleDecorations(): void {
    const decorations = this.mapData.decorations ?? [];
    const cam = this.cameras.main;
    const camCX = cam.scrollX + cam.width / 2 / cam.zoom;
    const camCY = cam.scrollY + cam.height / 2 / cam.zoom;
    const viewW = cam.width / cam.zoom / 2;
    const viewH = cam.height / cam.zoom / 2;
    const margin = 5;

    const visibleDecorKeys = new Set<string>();

    for (let i = 0; i < decorations.length; i++) {
      const decor = decorations[i];
      const pos = cartToIso(decor.col, decor.row);
      const dx = Math.abs(pos.x - camCX);
      const dy = Math.abs(pos.y - camCY);
      const key = `d_${i}`;

      if (dx < viewW + TILE_WIDTH * margin && dy < viewH + TILE_HEIGHT * margin) {
        visibleDecorKeys.add(key);
        if (!this.decorSprites.has(key)) {
          const texKey = `decor_${decor.type}`;
          if (this.textures.exists(texKey)) {
            const sprite = this.add.image(pos.x, pos.y - 6, texKey);
            sprite.setDepth(pos.y + 20);
            this.decorSprites.set(key, sprite);
          }
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

  private handleKeyboardMovement(delta: number): void {
    if (!this.cursors || !this.wasd) return;
    let dx = 0, dy = 0;
    if (this.cursors.up.isDown || this.wasd.W.isDown) { dx -= 1; dy -= 1; }
    if (this.cursors.down.isDown || this.wasd.S.isDown) { dx += 1; dy += 1; }
    if (this.cursors.left.isDown || this.wasd.A.isDown) { dx -= 1; dy += 1; }
    if (this.cursors.right.isDown || this.wasd.D.isDown) { dx += 1; dy -= 1; }

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

    const skillKeys = [this.wasd.ONE, this.wasd.TWO, this.wasd.THREE, this.wasd.FOUR, this.wasd.FIVE, this.wasd.SIX];
    const skills = this.player.classData.skills;
    for (let i = 0; i < Math.min(skillKeys.length, skills.length); i++) {
      if (Phaser.Input.Keyboard.JustDown(skillKeys[i])) this.tryUseSkill(skills[i].id, time);
    }
  }

  private tryUseSkill(skillId: string, time: number): void {
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
    const level = this.player.getSkillLevel(skillId);
    audioSystem.playSFX('skill');

    if (skill.buff) {
      this.player.buffs.push({ stat: skill.buff.stat, value: skill.buff.value + level * 0.02, duration: skill.buff.duration, startTime: time });
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: `${skill.name} 激活!`, type: 'combat' });
      this.skillEffects.play(skillId, this.player.sprite.x, this.player.sprite.y);
      return;
    }

    if (skill.aoe && skill.aoeRadius) {
      const aoeTargets = this.monsters.filter(m =>
        m.isAlive() && euclideanDistance(this.player.tileCol, this.player.tileRow, m.tileCol, m.tileRow) <= skill.aoeRadius!
      );
      for (const t of aoeTargets) {
        const result = this.combatSystem.calculateDamage(this.player.toCombatEntity(), t.toCombatEntity(), skill, level);
        t.takeDamage(result.damage);
        this.showDamageText(t.sprite.x, t.sprite.y, result.damage, result.isCrit);
        if (!t.isAlive()) this.onMonsterKilled(t);
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
      target.takeDamage(result.damage);
      this.showDamageText(target.sprite.x, target.sprite.y, result.damage, result.isCrit);
      if (!target.isAlive()) this.onMonsterKilled(target);
      this.skillEffects.play(skillId, this.player.sprite.x, this.player.sprite.y,
        target.sprite.x, target.sprite.y);
    }
  }

  private handleCombat(time: number): void {
    this.player.buffs = this.player.buffs.filter(b => time - b.startTime < b.duration);

    for (const monster of this.monsters) {
      if (!monster.isAlive() || monster.state !== 'attack') continue;
      if (time - monster.lastAttackTime >= monster.definition.attackSpeed) {
        monster.lastAttackTime = time;
        const result = this.combatSystem.calculateDamage(monster.toCombatEntity(), this.player.toCombatEntity());
        if (result.isDodged) {
          this.showDamageText(this.player.sprite.x, this.player.sprite.y, 0, false, true);
        } else {
          const diffMult = this.difficulty === 'hell' ? 2 : this.difficulty === 'nightmare' ? 1.5 : 1;
          const finalDmg = Math.floor(result.damage * diffMult);
          this.player.hp = Math.max(0, this.player.hp - finalDmg);
          this.showDamageText(this.player.sprite.x, this.player.sprite.y, finalDmg, result.isCrit, false, true);
          this.skillEffects.playMonsterAttack(this.player.sprite.x, this.player.sprite.y);
          if (this.player.hp <= 0) this.player.die();
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
        const result = this.combatSystem.calculateDamage(this.player.toCombatEntity(), target.toCombatEntity());
        target.takeDamage(result.damage);
        this.showDamageText(target.sprite.x, target.sprite.y, result.damage, result.isCrit);
        this.skillEffects.playAttack(this.player.sprite.x, this.player.sprite.y, target.sprite.x, target.sprite.y, true);
        if (!target.isAlive()) { this.onMonsterKilled(target); this.player.attackTarget = null; }
      }
    }
  }

  private handleAutoCombat(time: number): void {
    if (!this.player.attackTarget) {
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

  private handleAutoLoot(): void {
    const qualityRank: Record<string, number> = { normal: 0, magic: 1, rare: 2, legendary: 3, set: 3 };
    const minRank = this.autoLootMode === 'all' ? 0 : this.autoLootMode === 'magic' ? 1 : 2;
    for (let i = this.lootDrops.length - 1; i >= 0; i--) {
      const loot = this.lootDrops[i];
      const rank = qualityRank[loot.item.quality] ?? 0;
      if (rank < minRank) continue;
      const dist = euclideanDistance(this.player.tileCol, this.player.tileRow, loot.col, loot.row);
      if (dist > 2) continue;
      if (this.inventorySystem.addItem(loot.item)) {
        EventBus.emit(GameEvents.ITEM_PICKED, { item: loot.item });
        if (loot.item.quality === 'legendary') this.achievementSystem.update('collect');
        loot.sprite.destroy();
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
    for (const item of loot) {
      this.dropLoot(item, monster.tileCol, monster.tileRow);
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

    if (this.textures.exists('loot_bag')) {
      const bag = this.add.image(0, 0, 'loot_bag');
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
      lootDrop.sprite.destroy();
      const idx = this.lootDrops.indexOf(lootDrop);
      if (idx !== -1) this.lootDrops.splice(idx, 1);
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
          npcName: def.name,
          dialogue: dialogueText,
          actions,
        });
        break;
      }
      case 'stash':
        EventBus.emit(GameEvents.UI_TOGGLE_PANEL, { panel: 'stash' });
        break;
    }
  }

  private changeZone(targetMap: string, targetCol: number, targetRow: number): void {
    this.fogData[this.currentMapId] = this.fogOfWar.getExploredData();
    this.autoSave();
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
        skillLevels: Array.from(this.player.skillLevels.entries()),
        buffs: [...this.player.buffs],
        autoCombat: this.player.autoCombat,
      },
    });
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
        settings: { autoCombat: this.player.autoCombat, musicVolume: 0.5, sfxVolume: 0.7, autoLootMode: this.autoLootMode },
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
    this.player.skillLevels = new Map(Object.entries(save.player.skillLevels));
    this.player.recalcDerived();
    this.player.hp = Math.min(save.player.hp, this.player.maxHp);
    this.player.mana = Math.min(save.player.mana, this.player.maxMana);
    this.player.moveTo(save.player.tileCol, save.player.tileRow);

    // 2. Inventory
    this.inventorySystem.inventory = save.inventory ?? [];
    this.inventorySystem.equipment = (save.equipment ?? {}) as any;
    this.inventorySystem.stash = save.stash ?? [];

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
    this.autoLootMode = save.settings?.autoLootMode ?? 'off';
    this.difficulty = save.difficulty ?? 'normal';
  }

  private spawnMonsters(): void {
    const monsterDefs = MonstersByZone[this.currentMapId] || [];
    for (const spawn of this.mapData.spawns) {
      const def = monsterDefs.find(m => m.id === spawn.monsterId) || getMonsterDef(spawn.monsterId);
      if (!def) continue;
      for (let i = 0; i < spawn.count; i++) {
        const c = Math.max(1, Math.min(this.mapData.cols - 2, spawn.col + randomInt(-3, 3)));
        const r = Math.max(1, Math.min(this.mapData.rows - 2, spawn.row + randomInt(-3, 3)));
        if (this.mapData.collisions[r][c]) {
          this.monsters.push(new Monster(this, def, c, r));
        }
      }
    }
  }

  private spawnNPCs(): void {
    // Pre-computed offsets to spread NPCs around the camp center
    const npcOffsets: { dc: number; dr: number }[] = [
      { dc: -2, dr: 0 },
      { dc: 2, dr: 0 },
      { dc: 0, dr: -2 },
      { dc: 0, dr: 2 },
      { dc: -2, dr: -2 },
      { dc: 2, dr: 2 },
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
    const c = dead.spawnCol + randomInt(-1, 1);
    const r = dead.spawnRow + randomInt(-1, 1);
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

  private showDamageText(x: number, y: number, damage: number, isCrit: boolean, isDodged = false, isPlayer = false): void {
    let text: string, color: string, size = '16px';
    if (isDodged) { text = 'MISS'; color = '#7f8c8d'; size = '14px'; }
    else if (isPlayer) { text = `-${damage}`; color = isCrit ? '#ff6b6b' : '#e74c3c'; if (isCrit) size = '22px'; }
    else { text = `${damage}`; color = isCrit ? '#ffd700' : '#ffffff'; if (isCrit) size = '22px'; }
    const t = this.add.text(x + randomInt(-12, 12), y - 30, text, {
      fontSize: size, color, fontFamily: '"Cinzel", serif', fontStyle: isCrit ? 'bold' : 'normal', stroke: '#000000', strokeThickness: 3,
    }).setOrigin(0.5).setDepth(2000);
    EventBus.emit(GameEvents.COMBAT_DAMAGE, { isCrit, isDodged, isPlayer });
    this.tweens.add({ targets: t, y: t.y - 50, alpha: 0, duration: 1200, ease: 'Power2', onComplete: () => t.destroy() });
  }
}
