import Phaser from 'phaser';
import { TEXTURE_SCALE } from '../config';
import { cartToIso } from '../utils/IsometricUtils';
import { euclideanDistance } from '../utils/IsometricUtils';
import { randomInt } from '../utils/MathUtils';
import type { MonsterDefinition, Stats } from '../data/types';
import type { CombatEntity, ActiveBuff } from '../systems/CombatSystem';
import { CharacterAnimator, getAnimConfig } from '../systems/CharacterAnimator';

type MonsterState = 'idle' | 'patrol' | 'chase' | 'attack' | 'dead';

export class Monster {
  scene: Phaser.Scene;
  sprite: Phaser.GameObjects.Container;
  body: Phaser.GameObjects.Rectangle;
  hpBar: Phaser.GameObjects.Rectangle;
  hpBarBg: Phaser.GameObjects.Rectangle;

  id: string;
  definition: MonsterDefinition;
  hp: number;
  maxHp: number;
  mana: number = 0;
  maxMana: number = 0;
  stats: Stats;
  buffs: ActiveBuff[] = [];

  tileCol: number;
  tileRow: number;
  spawnCol: number;
  spawnRow: number;
  state: MonsterState = 'idle';
  lastAttackTime: number = 0;

  patrolTarget: { col: number; row: number } | null = null;
  patrolTimer: number = 0;
  animator: CharacterAnimator;
  leashRange: number = 8;

  private static idCounter = 0;

  constructor(scene: Phaser.Scene, definition: MonsterDefinition, col: number, row: number) {
    this.scene = scene;
    this.definition = definition;
    this.id = `monster_${Monster.idCounter++}`;
    this.tileCol = col;
    this.tileRow = row;
    this.spawnCol = col;
    this.spawnRow = row;
    this.maxHp = definition.hp;
    this.hp = this.maxHp;
    this.stats = {
      str: Math.floor(definition.damage * 0.8),
      dex: Math.floor(definition.speed * 0.1),
      vit: Math.floor(definition.hp * 0.1),
      int: 3,
      spi: 3,
      lck: 3,
    };

    const worldPos = cartToIso(col, row);
    this.sprite = scene.add.container(worldPos.x, worldPos.y);
    this.sprite.setDepth(worldPos.y + 50);

    // Use generated HD sprite if available
    const spriteKey = definition.spriteKey;
    const hasTexture = scene.textures.exists(spriteKey);
    const size = definition.elite ? 48 : 36;

    if (hasTexture) {
      const img = scene.add.image(0, -24, spriteKey).setScale(1 / TEXTURE_SCALE);
      this.sprite.add(img);
      this.body = scene.add.rectangle(0, -20, size, size, 0x000000, 0).setVisible(false);
    } else {
      const color = definition.elite ? 0xe74c3c : this.getMonsterColor(definition.id);
      this.body = scene.add.rectangle(0, -20, size, size, color);
      this.body.setStrokeStyle(1, 0x000000);
      this.sprite.add(this.body);
      const shadow = scene.add.ellipse(0, 4, size, 10, 0x000000, 0.3);
      this.sprite.add(shadow);
      this.sprite.sendToBack(shadow);
    }

    // HP bar background
    this.hpBarBg = scene.add.rectangle(0, -size - 10, 40, 4, 0x333333);
    this.sprite.add(this.hpBarBg);

    // HP bar
    this.hpBar = scene.add.rectangle(0, -size - 10, 40, 4, 0x2ecc71);
    this.sprite.add(this.hpBar);

    // Elite crown indicator
    if (definition.elite) {
      const crown = scene.add.rectangle(0, -size - 16, 12, 5, 0xf1c40f);
      this.sprite.add(crown);
    }

    const animCategory = definition.animCategory ?? 'humanoid';
    this.animator = new CharacterAnimator(scene, this.sprite, getAnimConfig(animCategory));
  }

  private getMonsterColor(id: string): number {
    if (id.includes('slime')) return 0x2ecc71;
    if (id.includes('goblin')) return 0x8e44ad;
    return 0x95a5a6;
  }

  update(time: number, delta: number, playerCol: number, playerRow: number, collisions: boolean[][]): void {
    if (this.state === 'dead') return;

    const distToPlayer = euclideanDistance(this.tileCol, this.tileRow, playerCol, playerRow);
    const distToSpawn = euclideanDistance(this.tileCol, this.tileRow, this.spawnCol, this.spawnRow);

    // Leash: return to spawn if too far
    if (distToSpawn > this.leashRange && this.state !== 'idle') {
      this.state = 'idle';
      this.moveToward(this.spawnCol, this.spawnRow, delta, collisions);
      this.hp = Math.min(this.maxHp, this.hp + this.maxHp * 0.01);
      this.updateHpBar();
      return;
    }

    switch (this.state) {
      case 'idle':
        this.patrolTimer += delta;
        if (distToPlayer <= this.definition.aggroRange) {
          this.state = 'chase';
        } else if (this.patrolTimer > 3000) {
          this.patrolTimer = 0;
          const pc = this.spawnCol + randomInt(-2, 2);
          const pr = this.spawnRow + randomInt(-2, 2);
          if (pc >= 0 && pc < collisions[0].length && pr >= 0 && pr < collisions.length && collisions[pr][pc]) {
            this.state = 'patrol';
            this.patrolTarget = { col: pc, row: pr };
          }
        }
        break;

      case 'patrol':
        if (distToPlayer <= this.definition.aggroRange) {
          this.state = 'chase';
          this.patrolTarget = null;
        } else if (this.patrolTarget) {
          const arrived = this.moveToward(this.patrolTarget.col, this.patrolTarget.row, delta, collisions);
          if (arrived) {
            this.state = 'idle';
            this.patrolTarget = null;
          }
        } else {
          this.state = 'idle';
        }
        break;

      case 'chase':
        if (distToPlayer > this.definition.aggroRange * 1.5) {
          this.state = 'idle';
        } else if (distToPlayer <= this.definition.attackRange) {
          this.state = 'attack';
        } else {
          this.moveToward(playerCol, playerRow, delta, collisions);
        }
        break;

      case 'attack':
        if (distToPlayer > this.definition.attackRange * 1.2) {
          this.state = 'chase';
        }
        // Attack timing handled by ZoneScene
        break;
    }

    // Drive animation states
    if (this.state === 'idle') {
      this.animator.setIdle();
    } else if (this.state === 'patrol' || this.state === 'chase') {
      this.animator.setWalk();
    }
    this.animator.update(delta);
  }

  private moveToward(targetCol: number, targetRow: number, delta: number, collisions: boolean[][]): boolean {
    const dx = targetCol - this.tileCol;
    const dy = targetRow - this.tileRow;
    const dist = Math.sqrt(dx * dx + dy * dy);

    if (dist < 0.1) return true;

    const speed = this.definition.speed * (delta / 1000) * 0.03;
    const nx = dx / dist;
    const ny = dy / dist;
    const newCol = this.tileCol + nx * speed;
    const newRow = this.tileRow + ny * speed;

    // Simple collision check
    const checkCol = Math.round(newCol);
    const checkRow = Math.round(newRow);
    if (checkCol >= 0 && checkCol < collisions[0].length &&
        checkRow >= 0 && checkRow < collisions.length &&
        collisions[checkRow][checkCol]) {
      this.tileCol = newCol;
      this.tileRow = newRow;
    }

    const worldPos = cartToIso(this.tileCol, this.tileRow);
    this.sprite.setPosition(worldPos.x, worldPos.y);
    this.sprite.setDepth(worldPos.y + 50);

    return false;
  }

  takeDamage(amount: number, sourceX?: number, sourceY?: number): void {
    this.hp = Math.max(0, this.hp - amount);
    this.updateHpBar();

    this.animator.playHurt(sourceX ?? this.sprite.x, sourceY ?? (this.sprite.y - 40));

    if (this.hp <= 0) {
      this.die();
    }
  }

  private updateHpBar(): void {
    const ratio = this.hp / this.maxHp;
    this.hpBar.scaleX = ratio;
    const color = ratio > 0.5 ? 0x2ecc71 : ratio > 0.25 ? 0xf39c12 : 0xe74c3c;
    this.hpBar.setFillStyle(color);
  }

  die(): void {
    this.state = 'dead';
    this.animator.playDeath(() => {
      this.sprite.destroy();
    });
  }

  playAttack(targetX: number, targetY: number): void {
    this.animator.playAttack(targetX, targetY);
  }

  toCombatEntity(): CombatEntity {
    return {
      id: this.id,
      name: this.definition.name,
      hp: this.hp,
      maxHp: this.maxHp,
      mana: this.mana,
      maxMana: this.maxMana,
      stats: { ...this.stats },
      level: this.definition.level,
      baseDamage: this.definition.damage,
      defense: this.definition.defense,
      attackSpeed: this.definition.attackSpeed,
      attackRange: this.definition.attackRange,
      buffs: this.buffs,
    };
  }

  isAlive(): boolean {
    return this.state !== 'dead' && this.hp > 0;
  }

  isAggro(): boolean {
    return this.state === 'chase' || this.state === 'attack';
  }
}
