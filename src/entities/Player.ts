import Phaser from 'phaser';
import { TILE_WIDTH, TILE_HEIGHT } from '../config';
import { cartToIso } from '../utils/IsometricUtils';
import { EventBus, GameEvents } from '../utils/EventBus';
import type { ClassDefinition, SkillDefinition, Stats } from '../data/types';
import type { CombatEntity, ActiveBuff } from '../systems/CombatSystem';

export class Player {
  scene: Phaser.Scene;
  sprite: Phaser.GameObjects.Container;
  body: Phaser.GameObjects.Rectangle;

  classData: ClassDefinition;
  level: number = 1;
  exp: number = 0;
  gold: number = 0;
  hp: number;
  maxHp: number;
  mana: number;
  maxMana: number;
  stats: Stats;
  freeStatPoints: number = 0;
  freeSkillPoints: number = 0;
  skillLevels: Map<string, number> = new Map();
  skillCooldowns: Map<string, number> = new Map();
  buffs: ActiveBuff[] = [];

  tileCol: number;
  tileRow: number;
  path: { col: number; row: number }[] = [];
  isMoving: boolean = false;
  moveSpeed: number = 120;

  attackTarget: string | null = null;
  lastAttackTime: number = 0;
  attackSpeed: number = 1000;
  attackRange: number = 1.5;
  baseDamage: number = 10;
  defense: number = 5;

  autoCombat: boolean = false;
  autoSkillPriority: string[] = [];

  constructor(scene: Phaser.Scene, classData: ClassDefinition, col: number, row: number) {
    this.scene = scene;
    this.classData = classData;
    this.tileCol = col;
    this.tileRow = row;

    this.stats = { ...classData.baseStats };
    this.maxHp = this.calcMaxHp();
    this.hp = this.maxHp;
    this.maxMana = this.calcMaxMana();
    this.mana = this.maxMana;

    // Create sprite container
    const worldPos = cartToIso(col, row);
    this.sprite = scene.add.container(worldPos.x, worldPos.y);
    this.sprite.setDepth(worldPos.y + 100);

    // Use generated HD sprite
    const spriteKey = `player_${classData.id}`;
    const hasTexture = scene.textures.exists(spriteKey);
    if (hasTexture) {
      const img = scene.add.image(0, -32, spriteKey);
      this.sprite.add(img);
      this.body = scene.add.rectangle(0, -24, 40, 48, 0x000000, 0).setVisible(false);
    } else {
      this.body = scene.add.rectangle(0, -24, 40, 48, 0x3498db);
      this.body.setStrokeStyle(1, 0x2980b9);
      this.sprite.add(this.body);
      const shadow = scene.add.ellipse(0, 4, 36, 10, 0x000000, 0.3);
      this.sprite.add(shadow);
      this.sprite.sendToBack(shadow);
    }

    // Set default skill levels and auto priorities
    for (const skill of classData.skills) {
      this.skillLevels.set(skill.id, 1);
      this.skillCooldowns.set(skill.id, 0);
    }
    this.autoSkillPriority = classData.skills.map(s => s.id);
  }

  private calcMaxHp(): number {
    return 50 + this.stats.vit * 10 + (this.level - 1) * 15;
  }

  private calcMaxMana(): number {
    return 30 + this.stats.spi * 8 + this.stats.int * 3 + (this.level - 1) * 8;
  }

  recalcDerived(): void {
    this.maxHp = this.calcMaxHp();
    this.maxMana = this.calcMaxMana();
    this.baseDamage = 8 + this.stats.str * 0.8 + this.level * 2;
    this.defense = 3 + this.stats.vit * 0.5 + this.level;
  }

  addExp(amount: number): void {
    this.exp += amount;
    const needed = this.expToNextLevel();
    if (this.exp >= needed) {
      this.exp -= needed;
      this.level++;
      this.freeStatPoints += 5;
      this.freeSkillPoints += 1;
      this.recalcDerived();
      this.hp = this.maxHp;
      this.mana = this.maxMana;
      EventBus.emit(GameEvents.PLAYER_LEVEL_UP, { level: this.level });
      EventBus.emit(GameEvents.LOG_MESSAGE, {
        text: `升级! 等级 ${this.level}`,
        type: 'system',
      });
    }
    EventBus.emit(GameEvents.PLAYER_EXP_CHANGED, {
      exp: this.exp,
      needed: this.expToNextLevel(),
    });
  }

  expToNextLevel(): number {
    return Math.floor(100 * Math.pow(1.15, this.level - 1));
  }

  moveTo(col: number, row: number): void {
    const worldPos = cartToIso(col, row);
    this.tileCol = col;
    this.tileRow = row;
    this.sprite.setPosition(worldPos.x, worldPos.y);
    this.sprite.setDepth(worldPos.y + 100);
  }

  setPath(newPath: { col: number; row: number }[]): void {
    this.path = newPath;
    this.isMoving = newPath.length > 0;
  }

  update(time: number, delta: number): void {
    this.updateMovement(delta);
    // Mana regen
    if (this.mana < this.maxMana) {
      this.mana = Math.min(this.maxMana, this.mana + (1 + this.stats.spi * 0.1) * delta / 1000);
    }
    // Hp regen (slow)
    if (this.hp < this.maxHp && this.hp > 0) {
      this.hp = Math.min(this.maxHp, this.hp + (0.5 + this.stats.vit * 0.05) * delta / 1000);
    }
  }

  private updateMovement(delta: number): void {
    if (this.path.length === 0) {
      this.isMoving = false;
      return;
    }

    const target = this.path[0];
    const targetWorld = cartToIso(target.col, target.row);
    const dx = targetWorld.x - this.sprite.x;
    const dy = targetWorld.y - this.sprite.y;
    const dist = Math.sqrt(dx * dx + dy * dy);

    const step = this.moveSpeed * (delta / 1000);

    if (dist <= step) {
      this.tileCol = target.col;
      this.tileRow = target.row;
      this.sprite.setPosition(targetWorld.x, targetWorld.y);
      this.sprite.setDepth(targetWorld.y + 100);
      this.path.shift();
      if (this.path.length === 0) {
        this.isMoving = false;
      }
    } else {
      const nx = dx / dist;
      const ny = dy / dist;
      this.sprite.x += nx * step;
      this.sprite.y += ny * step;
      this.sprite.setDepth(this.sprite.y + 100);

      // Approximate tile position
      this.tileCol += (target.col - this.tileCol) * (step / dist);
      this.tileRow += (target.row - this.tileRow) * (step / dist);
    }
  }

  getSkill(id: string): SkillDefinition | undefined {
    return this.classData.skills.find(s => s.id === id);
  }

  getSkillLevel(id: string): number {
    return this.skillLevels.get(id) ?? 0;
  }

  isSkillReady(id: string, now: number): boolean {
    const cd = this.skillCooldowns.get(id) ?? 0;
    return now >= cd;
  }

  useSkill(id: string, now: number): void {
    const skill = this.getSkill(id);
    if (!skill) return;
    this.skillCooldowns.set(id, now + skill.cooldown);
    this.mana = Math.max(0, this.mana - skill.manaCost);
    EventBus.emit(GameEvents.SKILL_USED, { skillId: id });
    EventBus.emit(GameEvents.PLAYER_MANA_CHANGED, { mana: this.mana, maxMana: this.maxMana });
  }

  toCombatEntity(): CombatEntity {
    return {
      id: 'player',
      name: this.classData.name,
      hp: this.hp,
      maxHp: this.maxHp,
      mana: this.mana,
      maxMana: this.maxMana,
      stats: { ...this.stats },
      level: this.level,
      baseDamage: this.baseDamage,
      defense: this.defense,
      attackSpeed: this.attackSpeed,
      attackRange: this.attackRange,
      buffs: this.buffs,
    };
  }

  die(): void {
    EventBus.emit(GameEvents.PLAYER_DIED, {});
    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: '你已死亡，将在营地复活...',
      type: 'system',
    });
  }

  respawnAtCamp(col: number, row: number): void {
    this.hp = this.maxHp;
    this.mana = this.maxMana;
    this.moveTo(col, row);
    this.path = [];
    this.isMoving = false;
    this.attackTarget = null;
    EventBus.emit(GameEvents.PLAYER_HEALTH_CHANGED, { hp: this.hp, maxHp: this.maxHp });
    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: '你在营地复活了。',
      type: 'system',
    });
  }
}
