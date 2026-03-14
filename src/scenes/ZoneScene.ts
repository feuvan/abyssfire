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
import { audioSystem } from '../systems/AudioSystem';
import { AllClasses } from '../data/classes/index';
import { AllMaps } from '../data/maps/index';
import { MonstersByZone, getMonsterDef } from '../data/monsters/index';
import { NPCDefinitions } from '../data/npcs';
import { AllQuests } from '../data/quests/all_quests';
import type { MapData, ClassDefinition, ItemInstance } from '../data/types';

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
  lootSystem!: LootSystem;
  inventorySystem!: InventorySystem;
  questSystem!: QuestSystem;
  homesteadSystem!: HomesteadSystem;
  achievementSystem!: AchievementSystem;
  saveSystem!: SaveSystem;
  private mapLayer!: Phaser.GameObjects.Container;
  private cursors!: Phaser.Types.Input.Keyboard.CursorKeys;
  private wasd!: Record<string, Phaser.Input.Keyboard.Key>;
  private campPositions: { col: number; row: number }[] = [];
  private lootDrops: { sprite: Phaser.GameObjects.Container; item: ItemInstance; col: number; row: number }[] = [];
  private difficulty: 'normal' | 'nightmare' | 'hell' = 'normal';
  private totalKills = 0;
  private exploredZones: Set<string> = new Set();
  private fogData: Record<string, boolean[][]> = {};

  constructor() {
    super({ key: 'ZoneScene' });
  }

  init(data: { classId: string; mapId: string }): void {
    this.currentMapId = data.mapId || 'emerald_plains';
    this.mapData = AllMaps[this.currentMapId];
    this.campPositions = this.mapData.camps.map(c => ({ col: c.col, row: c.row }));
  }

  create(data: { classId: string; mapId: string }): void {
    // Clean up previous scene
    this.monsters = [];
    this.npcs = [];
    this.lootDrops = [];

    this.combatSystem = new CombatSystem();
    this.lootSystem = new LootSystem();

    // Initialize systems (only on first load, not zone transitions)
    if (!this.inventorySystem) {
      this.inventorySystem = new InventorySystem();
      this.questSystem = new QuestSystem();
      this.homesteadSystem = new HomesteadSystem();
      this.achievementSystem = new AchievementSystem();
      this.saveSystem = new SaveSystem();
      this.questSystem.registerQuests(AllQuests);
    }

    // Create map
    this.mapLayer = this.add.container(0, 0);
    this.renderMap();

    // Create or reposition player
    if (!this.player || !this.player.sprite.scene) {
      const classData = AllClasses[data.classId] || AllClasses['warrior'];
      this.player = new Player(this, classData, this.mapData.playerStart.col, this.mapData.playerStart.row);
      this.player.recalcDerived();
    } else {
      // Re-create sprite in new scene
      const oldStats = {
        level: this.player.level, exp: this.player.exp, gold: this.player.gold,
        hp: this.player.hp, mana: this.player.mana, stats: { ...this.player.stats },
        freeStatPoints: this.player.freeStatPoints, freeSkillPoints: this.player.freeSkillPoints,
        skillLevels: new Map(this.player.skillLevels), buffs: [...this.player.buffs],
        autoCombat: this.player.autoCombat,
      };
      this.player = new Player(this, this.player.classData, this.mapData.playerStart.col, this.mapData.playerStart.row);
      Object.assign(this.player, oldStats);
      this.player.recalcDerived();
    }

    // Pathfinding
    this.pathfinding = new PathfindingSystem(this.mapData.collisions, this.mapData.cols, this.mapData.rows);

    // Fog of war
    this.fogOfWar = new FogOfWarSystem(this, this.mapData.cols, this.mapData.rows, 6);
    if (this.fogData[this.currentMapId]) {
      this.fogOfWar.loadExploredData(this.fogData[this.currentMapId]);
    }
    this.fogOfWar.update(this.player.tileCol, this.player.tileRow);

    // Spawn monsters + NPCs
    this.spawnMonsters();
    this.spawnNPCs();

    // Camera
    this.cameras.main.startFollow(this.player.sprite, true, 0.08, 0.08);
    this.cameras.main.setZoom(1.2);

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
      };
    }

    // Click to move / interact
    this.input.on('pointerdown', (pointer: Phaser.Input.Pointer) => {
      if (pointer.rightButtonDown()) return;
      const tile = worldToTile(pointer.worldX, pointer.worldY);

      // Check loot pickup
      const loot = this.findLootAt(tile.col, tile.row);
      if (loot) { this.pickupLoot(loot); return; }

      // Check NPC interaction
      const npc = this.findNPCAt(tile.col, tile.row);
      if (npc && npc.isNearPlayer(this.player.tileCol, this.player.tileRow, 3)) {
        this.interactNPC(npc);
        return;
      }

      // Check monster click
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

      // Check exit
      const exit = this.findExitAt(tile.col, tile.row);
      if (exit) {
        this.changeZone(exit.targetMap, exit.targetCol, exit.targetRow);
        return;
      }

      // Move
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

    // Start UI scene
    if (!this.scene.isActive('UIScene')) {
      this.scene.launch('UIScene', { player: this.player, zone: this });
    } else {
      EventBus.emit('ui:refresh', { player: this.player, zone: this });
    }

    // Listen for events
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

    // Track explored zone
    this.exploredZones.add(this.currentMapId);
    this.achievementSystem.update('explore', this.currentMapId);

    EventBus.emit(GameEvents.ZONE_ENTERED, { mapId: this.currentMapId });
    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: `进入 ${this.mapData.name} (Lv.${this.mapData.levelRange[0]}-${this.mapData.levelRange[1]})`,
      type: 'system',
    });

    // Auto save
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

    // Check exits proximity
    this.checkExitProximity();

    // Throttled fog update
    if (Math.floor(time / 200) !== Math.floor((time - delta) / 200)) {
      this.fogOfWar.update(Math.round(this.player.tileCol), Math.round(this.player.tileRow));
    }

    // HUD update
    EventBus.emit(GameEvents.PLAYER_HEALTH_CHANGED, { hp: this.player.hp, maxHp: this.player.maxHp });
    EventBus.emit(GameEvents.PLAYER_MANA_CHANGED, { mana: this.player.mana, maxMana: this.player.maxMana });
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

    if (skill.buff) {
      this.player.buffs.push({ stat: skill.buff.stat, value: skill.buff.value + level * 0.02, duration: skill.buff.duration, startTime: time });
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: `${skill.name} 激活!`, type: 'combat' });
      this.showSkillEffect(this.player.sprite.x, this.player.sprite.y, 0x3498db);
      return;
    }

    if (skill.aoe && skill.aoeRadius) {
      const targets = this.monsters.filter(m =>
        m.isAlive() && euclideanDistance(this.player.tileCol, this.player.tileRow, m.tileCol, m.tileRow) <= skill.aoeRadius!
      );
      for (const t of targets) {
        const result = this.combatSystem.calculateDamage(this.player.toCombatEntity(), t.toCombatEntity(), skill, level);
        t.takeDamage(result.damage);
        this.showDamageText(t.sprite.x, t.sprite.y, result.damage, result.isCrit);
        if (!t.isAlive()) this.onMonsterKilled(t);
      }
      this.showSkillEffect(this.player.sprite.x, this.player.sprite.y, 0xe74c3c);
    } else if (target) {
      const result = this.combatSystem.calculateDamage(this.player.toCombatEntity(), target.toCombatEntity(), skill, level);
      target.takeDamage(result.damage);
      this.showDamageText(target.sprite.x, target.sprite.y, result.damage, result.isCrit);
      if (!target.isAlive()) this.onMonsterKilled(target);
      this.showSkillEffect(target.sprite.x, target.sprite.y, 0xf39c12);
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

  private onMonsterKilled(monster: Monster): void {
    const diffMult = this.difficulty === 'hell' ? 3 : this.difficulty === 'nightmare' ? 2 : 1;
    const homeBonus = this.homesteadSystem.getTotalBonuses();
    const expBonus = 1 + (homeBonus['expBonus'] ?? 0) / 100;
    const exp = Math.floor(monster.definition.expReward * diffMult * expBonus);
    const gold = randomInt(monster.definition.goldReward[0], monster.definition.goldReward[1]) * diffMult;
    this.player.addExp(exp);
    this.player.gold += gold;

    // Achievements
    this.totalKills++;
    this.achievementSystem.update('kill', undefined, 1);
    this.achievementSystem.update('kill', monster.definition.id, 1);
    this.achievementSystem.checkLevel(this.player.level);

    // Quest progress
    this.questSystem.updateProgress('kill', monster.definition.id);

    // Loot
    const luckBonus = this.player.stats.lck + (homeBonus['magicFind'] ?? 0);
    const loot = this.lootSystem.generateLoot(monster.definition, luckBonus);
    for (const item of loot) {
      this.dropLoot(item, monster.tileCol, monster.tileRow);
    }

    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: `击杀 ${monster.definition.name}! +${exp}EXP +${gold}G`,
      type: 'loot',
    });

    // Respawn
    this.time.delayedCall(15000, () => this.respawnMonster(monster));
  }

  private dropLoot(item: ItemInstance, col: number, row: number): void {
    const worldPos = cartToIso(col + Math.random() * 0.5, row + Math.random() * 0.5);
    const container = this.add.container(worldPos.x, worldPos.y);
    container.setDepth(worldPos.y + 30);

    // Use loot bag sprite or colored rectangle
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

    // Quality glow for rare+
    if (item.quality !== 'normal' && item.quality !== 'magic') {
      const glow = this.add.circle(0, 0, 14, this.getQualityColor(item.quality), 0.15);
      container.add(glow);
      container.sendToBack(glow);
    }

    // Floating animation
    this.tweens.add({
      targets: container,
      y: container.y - 5,
      duration: 800,
      yoyo: true,
      repeat: -1,
      ease: 'Sine.easeInOut',
    });

    this.lootDrops.push({ sprite: container, item, col, row });

    // Auto-destroy after 60s
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

    switch (def.type) {
      case 'blacksmith':
      case 'merchant':
        EventBus.emit(GameEvents.SHOP_OPEN, { npcId: def.id, shopItems: def.shopItems ?? [], type: def.type });
        break;
      case 'quest':
        if (def.quests) {
          // Try to turn in completed quests first
          for (const qid of def.quests) {
            const reward = this.questSystem.turnInQuest(qid);
            if (reward) {
              this.player.addExp(reward.exp);
              this.player.gold += reward.gold;
              this.achievementSystem.update('quest');
            }
          }
          // Offer available quests
          const available = this.questSystem.getAvailableQuests(def.quests, this.player.level);
          for (const q of available) {
            const prog = this.questSystem.progress.get(q.id);
            if (!prog) {
              this.questSystem.acceptQuest(q.id);
              break; // Accept one at a time
            }
          }
        }
        break;
      case 'stash':
        EventBus.emit(GameEvents.UI_TOGGLE_PANEL, { panel: 'stash' });
        break;
    }
  }

  private changeZone(targetMap: string, targetCol: number, targetRow: number): void {
    // Save fog data
    this.fogData[this.currentMapId] = this.fogOfWar.getExploredData();
    this.autoSave();

    // Restart scene with new map
    this.scene.restart({ classId: this.player.classData.id, mapId: targetMap });
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
        settings: { autoCombat: this.player.autoCombat, musicVolume: 0.5, sfxVolume: 0.7 },
        difficulty: this.difficulty,
        completedDifficulties: [],
      });
    } catch (_e) { /* silent fail */ }
  }

  // --- Rendering helpers ---
  private renderMap(): void {
    for (let row = 0; row < this.mapData.rows; row++) {
      for (let col = 0; col < this.mapData.cols; col++) {
        const tileType = this.mapData.tiles[row][col];
        const tileKey = TILE_KEYS[tileType] || 'tile_grass';
        const pos = cartToIso(col, row);
        const tile = this.add.image(pos.x, pos.y, tileKey);
        tile.setDepth(pos.y);
        this.mapLayer.add(tile);
        if (tileType === 5) {
          const flag = this.add.rectangle(pos.x, pos.y - 24, 5, 18, 0xf1c40f);
          flag.setDepth(pos.y + 1);
          this.mapLayer.add(flag);
        }
        // Exit markers
        const exit = this.mapData.exits.find(e => e.col === col && e.row === row);
        if (exit) {
          if (this.textures.exists('exit_portal')) {
            const portal = this.add.image(pos.x, pos.y - 16, 'exit_portal');
            portal.setDepth(pos.y + 2);
            this.mapLayer.add(portal);
          } else {
            const marker = this.add.text(pos.x, pos.y - 16, '>>>', {
              fontSize: '12px', color: '#00ff00', fontFamily: '"Cinzel", serif',
            }).setOrigin(0.5).setDepth(pos.y + 2);
            this.mapLayer.add(marker);
          }
        }
      }
    }
  }

  private spawnMonsters(): void {
    const monsterDefs = MonstersByZone[this.currentMapId] || [];
    for (const spawn of this.mapData.spawns) {
      const def = monsterDefs.find(m => m.id === spawn.monsterId) || getMonsterDef(spawn.monsterId);
      if (!def) continue;
      for (let i = 0; i < spawn.count; i++) {
        const c = Math.max(1, Math.min(this.mapData.cols - 2, spawn.col + randomInt(-2, 2)));
        const r = Math.max(1, Math.min(this.mapData.rows - 2, spawn.row + randomInt(-2, 2)));
        if (this.mapData.collisions[r][c]) {
          this.monsters.push(new Monster(this, def, c, r));
        }
      }
    }
  }

  private spawnNPCs(): void {
    for (const camp of this.mapData.camps) {
      camp.npcs.forEach((npcId, i) => {
        const def = NPCDefinitions[npcId];
        if (!def) return;
        const npc = new NPC(this, def, camp.col + (i % 2), camp.row + Math.floor(i / 2));
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

  // --- Finders ---
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
    for (const npc of this.npcs) {
      if (Math.abs(npc.tileCol - col) < 1.5 && Math.abs(npc.tileRow - row) < 1.5) return npc;
    }
    return null;
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
    // Emit combat event for audio
    EventBus.emit(GameEvents.COMBAT_DAMAGE, { isCrit, isDodged, isPlayer });
    this.tweens.add({ targets: t, y: t.y - 50, alpha: 0, duration: 1200, ease: 'Power2', onComplete: () => t.destroy() });
  }

  private showSkillEffect(x: number, y: number, color: number): void {
    audioSystem.playSFX('skill');
    const c = this.add.circle(x, y - 16, 8, color, 0.7).setDepth(1500);
    this.tweens.add({ targets: c, scaleX: 5, scaleY: 5, alpha: 0, duration: 500, ease: 'Power2', onComplete: () => c.destroy() });
  }
}
