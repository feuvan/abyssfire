import Phaser from 'phaser';
import { TEXTURE_SCALE, DPR } from '../config';
import { cartToIso } from '../utils/IsometricUtils';
import { euclideanDistance } from '../utils/IsometricUtils';
import { randomInt } from '../utils/MathUtils';
import type { MonsterDefinition, Stats } from '../data/types';
import type { CombatEntity, ActiveBuff } from '../systems/CombatSystem';
import { CharacterAnimator, getAnimConfig } from '../systems/CharacterAnimator';
import { SpriteGenerator } from '../graphics/SpriteGenerator';
import type { EliteAffixInstance } from '../systems/EliteAffixSystem';

function fs(basePx: number): string {
  return `${Math.round(basePx * DPR)}px`;
}

type MonsterState = 'idle' | 'patrol' | 'chase' | 'attack' | 'dead';

export class Monster {
  scene: Phaser.Scene;
  sprite: Phaser.GameObjects.Container;
  body: Phaser.GameObjects.Rectangle;
  hpBar: Phaser.GameObjects.Rectangle;
  hpBarBg: Phaser.GameObjects.Rectangle;
  nameLabel: Phaser.GameObjects.Text;

  id: string;
  definition: MonsterDefinition;
  hp: number;
  maxHp: number;
  mana: number = 0;
  maxMana: number = 0;
  stats: Stats;
  buffs: ActiveBuff[] = [];

  /** Elite affixes assigned to this monster (empty for non-elite / non-affix monsters). */
  eliteAffixes: EliteAffixInstance[] = [];
  /** Affix aura visual (repeating tween or particle, destroyed on death). */
  private affixAuraVisuals: Phaser.GameObjects.GameObject[] = [];

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

  private currentMoveSpeed = 0;
  private readonly moveAccel = 6;

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

    // Use animated sprite sheet if available
    const spriteKey = definition.spriteKey;
    SpriteGenerator.ensureMonsterSheet(scene, spriteKey);
    const hasTexture = scene.textures.exists(spriteKey);
    const size = definition.elite ? 48 : 36;

    if (hasTexture) {
      const spr = scene.add.sprite(0, -24, spriteKey, 0).setScale(1 / TEXTURE_SCALE);
      this.sprite.add(spr);
      this.body = scene.add.rectangle(0, -20, size, size, 0x000000, 0).setVisible(false);
      // Play idle animation if registered
      const idleKey = `${spriteKey}_idle`;
      if (scene.anims.exists(idleKey)) spr.play(idleKey);
    } else {
      const color = definition.elite ? 0xe74c3c : this.getMonsterColor(definition.id);
      this.body = scene.add.rectangle(0, -20, size, size, color);
      this.body.setStrokeStyle(1, 0x000000);
      this.sprite.add(this.body);
      const shadow = scene.add.ellipse(0, 4, size, 10, 0x000000, 0.3);
      this.sprite.add(shadow);
      this.sprite.sendToBack(shadow);
    }

    // HP bar background (hidden until damaged)
    this.hpBarBg = scene.add.rectangle(-20, -size - 10, 40, 4, 0x333333).setOrigin(0, 0.5).setAlpha(0);
    this.sprite.add(this.hpBarBg);

    // HP bar (hidden until damaged, scales from left edge)
    this.hpBar = scene.add.rectangle(-20, -size - 10, 40, 4, 0x2ecc71).setOrigin(0, 0.5).setAlpha(0);
    this.sprite.add(this.hpBar);

    // Monster name label (visible on aggro/damage)
    const nameLabel = scene.add.text(0, -size - 18, definition.name, {
      fontSize: fs(12), color: definition.elite ? '#e74c3c' : '#cccccc',
      fontFamily: '"Cinzel", serif', stroke: '#000000', strokeThickness: Math.round(2 * DPR),
    }).setOrigin(0.5).setAlpha(0);
    this.sprite.add(nameLabel);
    this.nameLabel = nameLabel;

    // Elite crown indicator
    if (definition.elite) {
      const crown = scene.add.rectangle(0, -size - 16, 12, 5, 0xf1c40f);
      this.sprite.add(crown);
    }

    const animCategory = definition.animCategory ?? 'humanoid';
    this.animator = new CharacterAnimator(scene, this.sprite, getAnimConfig(animCategory), spriteKey);
  }

  private getMonsterColor(id: string): number {
    if (id.includes('slime')) return 0x2ecc71;
    if (id.includes('goblin')) return 0x8e44ad;
    return 0x95a5a6;
  }

  update(time: number, delta: number, playerCol: number, playerRow: number, collisions: boolean[][], speedMultiplier = 1): void {
    if (this.state === 'dead') return;

    const distToPlayer = euclideanDistance(this.tileCol, this.tileRow, playerCol, playerRow);
    const distToSpawn = euclideanDistance(this.tileCol, this.tileRow, this.spawnCol, this.spawnRow);

    // Leash: return to spawn if too far
    if (distToSpawn > this.leashRange && this.state !== 'idle') {
      this.state = 'idle';
      this.currentMoveSpeed = 0;
      this.moveToward(this.spawnCol, this.spawnRow, delta, collisions, speedMultiplier);
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
          const arrived = this.moveToward(this.patrolTarget.col, this.patrolTarget.row, delta, collisions, speedMultiplier);
          if (arrived) {
            this.state = 'idle';
            this.currentMoveSpeed = 0;
            this.patrolTarget = null;
          }
        } else {
          this.state = 'idle';
          this.currentMoveSpeed = 0;
        }
        break;

      case 'chase':
        if (distToPlayer > this.definition.aggroRange * 1.5) {
          this.state = 'idle';
          this.currentMoveSpeed = 0;
        } else if (distToPlayer <= this.definition.attackRange) {
          this.state = 'attack';
        } else {
          this.moveToward(playerCol, playerRow, delta, collisions, speedMultiplier);
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

  private moveToward(targetCol: number, targetRow: number, delta: number, collisions: boolean[][], speedMultiplier = 1): boolean {
    const dx = targetCol - this.tileCol;
    const dy = targetRow - this.tileRow;
    const dist = Math.sqrt(dx * dx + dy * dy);

    if (dist < 0.1) return true;

    const targetSpeed = this.definition.speed * speedMultiplier * (delta / 1000) * 0.03;
    this.currentMoveSpeed += (targetSpeed - this.currentMoveSpeed) * this.moveAccel * (delta / 1000);
    const nx = dx / dist;
    const ny = dy / dist;
    const newCol = this.tileCol + nx * this.currentMoveSpeed;
    const newRow = this.tileRow + ny * this.currentMoveSpeed;

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

    // Knockback recoil: push sprite away from damage source briefly
    const sx = sourceX ?? this.sprite.x;
    const sy = sourceY ?? this.sprite.y;
    const dx = this.sprite.x - sx;
    const dy = this.sprite.y - sy;
    const dist = Math.sqrt(dx * dx + dy * dy) || 1;
    const knockDist = 10;
    this.scene.tweens.add({
      targets: this.sprite,
      x: this.sprite.x + (dx / dist) * knockDist,
      y: this.sprite.y + (dy / dist) * knockDist,
      duration: 200,
      yoyo: true,
      ease: 'Back.easeOut',
    });

    if (this.hp <= 0) {
      this.die();
    }
  }

  private updateHpBar(): void {
    const ratio = this.hp / this.maxHp;
    this.hpBar.scaleX = ratio;
    const color = ratio > 0.5 ? 0x2ecc71 : ratio > 0.25 ? 0xf39c12 : 0xe74c3c;
    this.hpBar.setFillStyle(color);
    // Show HP bar + name only when damaged
    const show = ratio < 1;
    this.hpBarBg.setAlpha(show ? 0.8 : 0);
    this.hpBar.setAlpha(show ? 1 : 0);
    this.nameLabel.setAlpha(show ? 1 : 0);
  }

  die(): void {
    this.state = 'dead';
    this.hpBar.setAlpha(0);
    this.hpBarBg.setAlpha(0);
    this.nameLabel.setAlpha(0);
    this.cleanupAffixVisuals();
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
      name: this.nameLabel.text,
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

  // ---------------------------------------------------------------------------
  // Elite Affix Support
  // ---------------------------------------------------------------------------

  /**
   * Apply elite affixes to this monster: modify stats, update name label,
   * and add visual aura indicators.
   */
  applyEliteAffixes(affixes: EliteAffixInstance[], affixSystem: import('../systems/EliteAffixSystem').EliteAffixSystem): void {
    this.eliteAffixes = affixes;
    if (affixes.length === 0) return;

    const stats = affixSystem.getCombinedStats(affixes);

    // Apply stat multipliers
    this.maxHp = Math.floor(this.maxHp * stats.hpMult);
    this.hp = this.maxHp;
    this.definition = {
      ...this.definition,
      damage: Math.floor(this.definition.damage * stats.damageMult),
      speed: Math.floor(this.definition.speed * stats.speedMult),
      defense: Math.floor(this.definition.defense * stats.defenseMult),
    };
    // Update internal stats
    this.stats.str = Math.floor(this.definition.damage * 0.8);

    // Update name label to show affix names in Chinese
    const newName = affixSystem.buildAffixName(this.definition.name, affixes);
    this.nameLabel.setText(newName);
    this.nameLabel.setColor('#ff6600'); // Orange for affix elites
    // Always show name label for affix elites
    this.nameLabel.setAlpha(1);

    // Add visual aura indicators per affix
    this.createAffixVisuals(affixes);
  }

  /**
   * Create visual indicators for each affix on this monster.
   */
  private createAffixVisuals(affixes: EliteAffixInstance[]): void {
    const size = this.definition.elite ? 48 : 36;

    for (const affix of affixes) {
      const color = affix.definition.vfxColor;
      const type = affix.definition.type;

      // Aura ring underneath the monster
      const aura = this.scene.add.ellipse(0, 4, size + 8, (size + 8) / 3, color, 0.25);
      aura.setStrokeStyle(1, color, 0.6);
      this.sprite.add(aura);
      this.sprite.sendToBack(aura);
      this.affixAuraVisuals.push(aura);

      // Pulsing aura animation
      this.scene.tweens.add({
        targets: aura,
        scaleX: 1.2,
        scaleY: 1.2,
        alpha: 0.1,
        duration: 1200,
        yoyo: true,
        repeat: -1,
        ease: 'Sine.easeInOut',
      });

      // Type-specific extra visuals
      if (type === 'fire_enhanced') {
        // Fire particles above the monster
        const fireGlow = this.scene.add.circle(0, -size / 2, 6, 0xff4400, 0.4);
        this.sprite.add(fireGlow);
        this.affixAuraVisuals.push(fireGlow);
        this.scene.tweens.add({
          targets: fireGlow,
          y: -size / 2 - 8,
          alpha: 0.1,
          scaleX: 0.5,
          scaleY: 0.5,
          duration: 800,
          yoyo: true,
          repeat: -1,
        });
      } else if (type === 'swift') {
        // Speed lines behind the monster
        const speedLine = this.scene.add.rectangle(-size / 2, -16, 8, 2, 0x44ff88, 0.5);
        this.sprite.add(speedLine);
        this.sprite.sendToBack(speedLine);
        this.affixAuraVisuals.push(speedLine);
        this.scene.tweens.add({
          targets: speedLine,
          x: -size / 2 - 10,
          alpha: 0,
          duration: 600,
          yoyo: true,
          repeat: -1,
        });
      } else if (type === 'teleporting') {
        // Arcane shimmer
        const shimmer = this.scene.add.circle(0, -20, 4, 0xaa44ff, 0.5);
        this.sprite.add(shimmer);
        this.affixAuraVisuals.push(shimmer);
        this.scene.tweens.add({
          targets: shimmer,
          scaleX: 2,
          scaleY: 2,
          alpha: 0,
          duration: 1000,
          yoyo: true,
          repeat: -1,
        });
      } else if (type === 'curse_aura') {
        // Dark purple mist
        const mist = this.scene.add.ellipse(0, 0, size * 2, size, 0x8844aa, 0.08);
        this.sprite.add(mist);
        this.sprite.sendToBack(mist);
        this.affixAuraVisuals.push(mist);
        this.scene.tweens.add({
          targets: mist,
          scaleX: 1.3,
          scaleY: 1.3,
          alpha: 0.03,
          duration: 1500,
          yoyo: true,
          repeat: -1,
        });
      } else if (type === 'vampiric') {
        // Blood drip indicator
        const bloodGlow = this.scene.add.circle(0, -size / 2, 5, 0xcc0000, 0.4);
        this.sprite.add(bloodGlow);
        this.affixAuraVisuals.push(bloodGlow);
        this.scene.tweens.add({
          targets: bloodGlow,
          y: -size / 2 + 4,
          alpha: 0.15,
          duration: 1000,
          yoyo: true,
          repeat: -1,
        });
      } else if (type === 'frozen') {
        // Ice crystals
        const ice = this.scene.add.circle(6, -size / 2, 4, 0x4488ff, 0.5);
        this.sprite.add(ice);
        this.affixAuraVisuals.push(ice);
        this.scene.tweens.add({
          targets: ice,
          scaleX: 1.5,
          scaleY: 1.5,
          alpha: 0.2,
          duration: 900,
          yoyo: true,
          repeat: -1,
        });
      } else if (type === 'extra_strong') {
        // Red power glow
        const powerGlow = this.scene.add.circle(0, -20, 8, 0xff2222, 0.2);
        this.sprite.add(powerGlow);
        this.affixAuraVisuals.push(powerGlow);
        this.scene.tweens.add({
          targets: powerGlow,
          scaleX: 1.8,
          scaleY: 1.8,
          alpha: 0.05,
          duration: 700,
          yoyo: true,
          repeat: -1,
        });
      }
    }
  }

  /**
   * Clean up all affix-related visuals (called on death or despawn).
   */
  private cleanupAffixVisuals(): void {
    for (const visual of this.affixAuraVisuals) {
      if (visual && (visual as any).destroy) {
        this.scene.tweens.killTweensOf(visual);
        (visual as any).destroy();
      }
    }
    this.affixAuraVisuals = [];
  }
}
