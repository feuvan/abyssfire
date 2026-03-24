import Phaser from 'phaser';
import { TILE_WIDTH, TILE_HEIGHT, TEXTURE_SCALE } from '../config';
import { cartToIso } from '../utils/IsometricUtils';
import { EventBus, GameEvents } from '../utils/EventBus';
import type { ClassDefinition, SkillDefinition, Stats } from '../data/types';
import type { CombatEntity, ActiveBuff, EquipStats } from '../systems/CombatSystem';
import { getSkillManaCost, getSkillCooldown } from '../systems/CombatSystem';
import { CharacterAnimator, getAnimConfig } from '../systems/CharacterAnimator';
import { SpriteGenerator } from '../graphics/SpriteGenerator';

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
  animator!: CharacterAnimator;

  tileCol: number;
  tileRow: number;
  path: { col: number; row: number }[] = [];
  isMoving: boolean = false;
  moveSpeed: number = 120;
  private currentSpeed = 0;
  private readonly acceleration = 8;
  private readonly deceleration = 12;

  attackTarget: string | null = null;
  lastAttackTime: number = 0;
  attackSpeed: number = 1000;
  attackRange: number = 1.5;
  baseDamage: number = 10;
  defense: number = 5;

  autoCombat: boolean = false;
  autoLootMode: 'off' | 'all' | 'magic' | 'rare' = 'off';
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

    // Use animated sprite sheet
    const spriteKey = `player_${classData.id}`;
    SpriteGenerator.ensurePlayerSheet(scene, classData.id);
    const hasTexture = scene.textures.exists(spriteKey);
    if (hasTexture) {
      const spr = scene.add.sprite(0, -32, spriteKey, 0).setScale(1 / TEXTURE_SCALE);
      this.sprite.add(spr);
      this.body = scene.add.rectangle(0, -24, 40, 48, 0x000000, 0).setVisible(false);
      // Play idle animation if registered
      const idleKey = `${spriteKey}_idle`;
      if (scene.anims.exists(idleKey)) spr.play(idleKey);
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
    this.animator = new CharacterAnimator(scene, this.sprite, getAnimConfig(classData.id), spriteKey);
  }

  private calcMaxHp(): number {
    return 50 + this.stats.vit * 10 + (this.level - 1) * 15;
  }

  private calcMaxMana(): number {
    return 30 + this.stats.spi * 8 + this.stats.int * 3 + (this.level - 1) * 8;
  }

  recalcDerived(equipStats?: EquipStats): void {
    // Effective primary stats include gear/gem bonuses
    const eStr = this.stats.str + (equipStats?.str ?? 0);
    const eVit = this.stats.vit + (equipStats?.vit ?? 0);
    const eSpi = this.stats.spi + (equipStats?.spi ?? 0);
    const eInt = this.stats.int + (equipStats?.int ?? 0);

    let hp = 50 + eVit * 10 + (this.level - 1) * 15;
    let mp = 30 + eSpi * 8 + eInt * 3 + (this.level - 1) * 8;
    let dmg = 8 + eStr * 0.8 + this.level * 2;
    let def = 3 + eVit * 0.5 + this.level;
    let spd = 120;
    let aspd = 1000;

    if (equipStats) {
      // Flat bonuses from primary stats on gear
      hp += equipStats.maxHp;
      hp = Math.floor(hp * (1 + equipStats.maxHpPercent / 100));
      mp += equipStats.maxMana;
      mp = Math.floor(mp * (1 + equipStats.maxManaPercent / 100));
      // Move speed percent bonus
      spd = Math.floor(spd * (1 + equipStats.moveSpeed / 100));
      // Attack speed percent bonus (lower is faster, so reduce)
      aspd = Math.max(200, Math.floor(aspd * (1 - equipStats.attackSpeed / 100)));
    }

    this.maxHp = hp;
    this.maxMana = mp;
    this.baseDamage = dmg;
    this.defense = def;
    this.moveSpeed = spd;
    this.attackSpeed = aspd;
  }

  getManaRegenPerSecond(): number {
    return 1 + this.stats.spi * 0.1;
  }

  getHpRegenPerSecond(): number {
    return 0.5 + this.stats.vit * 0.05;
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
    // Polynomial curve: ~15-25 kills per level in the current zone
    return Math.floor(this.level * this.level * 3 + this.level * 25);
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

  update(
    time: number,
    delta: number,
    recovery: { hpRegenMultiplier?: number; manaRegenMultiplier?: number } = {},
    equipStats?: EquipStats,
  ): void {
    if (this.hp <= 0) {
      this.path = [];
      this.isMoving = false;
      return;
    }
    this.updateMovement(delta);
    const manaRegenMultiplier = recovery.manaRegenMultiplier ?? 1;
    const hpRegenMultiplier = recovery.hpRegenMultiplier ?? 1;
    const bonusHpRegen = equipStats?.hpRegen ?? 0;
    const bonusManaRegen = equipStats?.manaRegen ?? 0;

    // Mana regen
    if (this.mana < this.maxMana) {
      this.mana = Math.min(this.maxMana, this.mana + (this.getManaRegenPerSecond() + bonusManaRegen) * manaRegenMultiplier * delta / 1000);
    }
    // Hp regen (slow)
    if (this.hp < this.maxHp && this.hp > 0) {
      this.hp = Math.min(this.maxHp, this.hp + (this.getHpRegenPerSecond() + bonusHpRegen) * hpRegenMultiplier * delta / 1000);
    }
    this.animator.update(delta);
  }

  private updateMovement(delta: number): void {
    const dt = delta / 1000;

    if (this.path.length === 0) {
      // Decelerate to stop
      this.currentSpeed = this.currentSpeed * (1 - this.deceleration * dt);
      if (this.currentSpeed < 0.5) {
        this.currentSpeed = 0;
        this.isMoving = false;
        this.animator.setIdle();
      }
      return;
    }
    this.animator.setWalk();

    // Accelerate toward target speed
    this.currentSpeed += (this.moveSpeed - this.currentSpeed) * this.acceleration * dt;

    const target = this.path[0];
    const targetWorld = cartToIso(target.col, target.row);
    const dx = targetWorld.x - this.sprite.x;
    const dy = targetWorld.y - this.sprite.y;
    const dist = Math.sqrt(dx * dx + dy * dy);

    const step = this.currentSpeed * dt;

    if (dist <= step) {
      this.tileCol = target.col;
      this.tileRow = target.row;
      this.sprite.setPosition(targetWorld.x, targetWorld.y);
      this.sprite.setDepth(targetWorld.y + 100);
      this.spawnFootDust(targetWorld.x, targetWorld.y);
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

      this.tileCol += (target.col - this.tileCol) * (step / dist);
      this.tileRow += (target.row - this.tileRow) * (step / dist);
    }
  }

  private spawnFootDust(x: number, y: number): void {
    for (let i = 0; i < 5; i++) {
      const p = this.scene.add.circle(
        x + (Math.random() - 0.5) * 16,
        y + 2 + Math.random() * 5,
        1.5 + Math.random() * 2,
        0x888877, 0.3,
      ).setDepth(this.sprite.depth - 1);
      this.scene.tweens.add({
        targets: p,
        alpha: 0, y: p.y - 8 - Math.random() * 6,
        x: p.x + (Math.random() - 0.5) * 12,
        scale: 0.2,
        duration: 350 + Math.random() * 250,
        ease: 'Power2',
        onComplete: () => p.destroy(),
      });
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

  useSkill(id: string, now: number, level?: number, cdr = 0): void {
    const skill = this.getSkill(id);
    if (!skill) return;
    const slevel = level ?? this.getSkillLevel(id);
    this.skillCooldowns.set(id, now + getSkillCooldown(skill, slevel, cdr));
    this.mana = Math.max(0, this.mana - getSkillManaCost(skill, slevel));
    EventBus.emit(GameEvents.SKILL_USED, { skillId: id, damageType: skill.damageType });
    EventBus.emit(GameEvents.PLAYER_MANA_CHANGED, { mana: this.mana, maxMana: this.maxMana });
  }

  toCombatEntity(equipStats?: EquipStats): CombatEntity {
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
      equipStats,
    };
  }

  die(): void {
    this.playDeath();
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
    this.sprite.setAlpha(1);
    this.sprite.setScale(1);
    this.sprite.setAngle(0);
    // Reset child alpha/angle that death animation may have modified
    // Note: do NOT reset scale — children have intentional scales (e.g. 1/TEXTURE_SCALE)
    for (const child of this.sprite.list) {
      const go = child as any;
      if (go.setAlpha) go.setAlpha(1);
      if (go.setAngle) go.setAngle(0);
    }
    this.animator.cleanup();
    this.animator = new CharacterAnimator(this.scene, this.sprite, getAnimConfig(this.classData.id), `player_${this.classData.id}`);
    this.animator.forceIdle();
    EventBus.emit(GameEvents.PLAYER_HEALTH_CHANGED, { hp: this.hp, maxHp: this.maxHp });
    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: '你在营地复活了。',
      type: 'system',
    });
  }

  playAttack(targetX: number, targetY: number): void {
    this.animator.playAttack(targetX, targetY);
  }

  playCast(): void {
    this.animator.playCast();
  }

  playHurt(sourceX: number, sourceY: number): void {
    this.animator.playHurt(sourceX, sourceY);
  }

  playDeath(): void {
    this.animator.playDeath();
  }
}
