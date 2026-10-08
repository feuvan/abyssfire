/**
 * The active ley-beast in the world: follows the hero, fights the hero's target
 * with its basic attack and abilities (decided by PetSystem.choosePetAction),
 * soaks the odd blow and, at 0 HP, is exhausted for a few seconds instead of dying.
 *
 * Created by ZoneScene (one per zone visit) through a small host interface so the
 * scene only has to forward update / monster swings / hero death.
 *
 * Art: uses `SpriteGenerator.ensurePetSheet(scene, petId, stage)` (4-way idle / walk /
 * attack / cast sheet, CharacterAnimator-compatible) when it exists, otherwise the
 * floating `decor_pet_<id>` image.
 */
import Phaser from 'phaser';
import { TEXTURE_SCALE, DPR } from '../config';
import { cartToIso } from '../utils/IsometricUtils';
import { EventBus, GameEvents } from '../utils/EventBus';
import { t } from '../i18n';
import { SpriteGenerator } from '../graphics/SpriteGenerator';
import { CharacterAnimator, getAnimConfig } from './CharacterAnimator';
import { HIT_PROFILES } from './HitFeedback';
import { tileDeltaToScreen, type Monster } from '../entities/Monster';
import type { Player } from '../entities/Player';
import type { VFXManager } from './VFXManager';
import type { SkillEffectSystem } from './SkillEffectSystem';
import type { StatusEffectSystem } from './StatusEffectSystem';
import {
  choosePetAction, shouldBondRescue, type PetSystem, type PetAction,
} from './PetSystem';
import { getPetDef, primaryAbility, unlockedAbilities, type PetAbilityDef, type PetDef } from '../data/pets';

export interface PetCompanionHost {
  scene: Phaser.Scene;
  pets: PetSystem;
  player: Player;
  vfx(): VFXManager | null;
  skillEffects: SkillEffectSystem;
  statusEffects: StatusEffectSystem;
  isWalkable(col: number, row: number): boolean;
  monstersNear(col: number, row: number, radius: number): Monster[];
  findMonster(id: string): Monster | undefined;
  /** Hero damage the pet's attack scales from. */
  heroDamage(): number;
  inSafeZone(col: number, row: number): boolean;
  /** Cinematic / zone transition: freeze the beast. */
  isPaused(): boolean;
  showDamage(x: number, y: number, amount: number, isCrit: boolean, onHero: boolean, damageType?: string): void;
  onMonsterKilled(monster: Monster): void;
}

/** Seconds a beast stays exhausted at 0 HP. */
export const PET_EXHAUST_MS = 5000;
/** Chance a monster in reach swings at the beast instead of the hero. */
const STRAY_SWING_CHANCE = 0.25;
const FOLLOW_SPEED = 4.2; // tiles / s
const DASH_SPEED = 8.5;
const TELEPORT_DIST = 16;
const TAG_MARK = 'petMark';
const TAG_SHIELD = 'petShield';
const TAG_HOWL = 'petHowl';

const FONT = '"Noto Sans SC", sans-serif';
function fs(basePx: number): string {
  return `${Math.round(basePx * DPR)}px`;
}

type PetSheetApi = { ensurePetSheet?: (scene: Phaser.Scene, petId: string, stage: 0 | 1 | 2) => string };

export class PetCompanion {
  private readonly host: PetCompanionHost;
  private container: Phaser.GameObjects.Container | null = null;
  private visual: Phaser.GameObjects.Sprite | Phaser.GameObjects.Image | null = null;
  private animator: CharacterAnimator | null = null;
  private nameLabel: Phaser.GameObjects.Text | null = null;
  private hpBar: Phaser.GameObjects.Rectangle | null = null;
  private hpBarBg: Phaser.GameObjects.Rectangle | null = null;
  private shownPet = '';
  private shownStage = -1;

  col = 0;
  row = 0;
  hp = 1;
  maxHp = 1;
  private exhaustedUntil = 0;
  private readyAt: Record<string, number> = {};
  private basicReadyAt = 0;
  /** Busy with an attack / ability until. */
  private lockedUntil = 0;
  private taunts = new Map<string, number>();
  private marks = new Map<Monster, number>();
  private reviveUsed = false;
  private lastRescueAt = -Infinity;
  private lastLabel = '';

  constructor(host: PetCompanionHost) {
    this.host = host;
  }

  private get scene(): Phaser.Scene { return this.host.scene; }
  private get player(): Player { return this.host.player; }

  // ─── Public API used by ZoneScene ────────────────────────────────────────

  isExhausted(now = this.scene.time.now): boolean {
    return now < this.exhaustedUntil;
  }

  getSprite(): Phaser.GameObjects.Container | null {
    return this.container;
  }

  /** Rebuild the visual (active pet changed / evolved). */
  refresh(): void {
    this.destroyVisual();
  }

  /**
   * A monster in the attack state is ready to swing. Returns true if it swings at
   * the beast instead (taunted, or a stray swing when the beast is closer).
   */
  interceptMonsterAttack(monster: Monster, time: number): boolean {
    if (!this.container || this.isExhausted(time) || !this.host.pets.activePet) return false;
    const until = this.taunts.get(monster.id);
    let redirect = until !== undefined && until > time;
    if (!redirect) {
      const reach = monster.definition.attackRange + 0.5;
      const dPet = Math.hypot(monster.tileCol - this.col, monster.tileRow - this.row);
      const dHero = Math.hypot(monster.tileCol - this.player.tileCol, monster.tileRow - this.player.tileRow);
      redirect = dPet <= reach && dPet < dHero && Math.random() < STRAY_SWING_CHANCE;
    }
    if (!redirect) return false;
    monster.lastAttackTime = time;
    const delay = monster.playAttack(this.container.x, this.container.y);
    this.scene.time.delayedCall(delay, () => this.takeHit(monster));
    return true;
  }

  /**
   * The hero is about to die: a beast with `revive` (赫莉娅之烬) rekindles them once
   * per zone. Returns true if the hero was saved.
   */
  tryReviveHero(): boolean {
    const def = this.host.pets.getActivePetDef();
    const pet = this.host.pets.getActivePetInstance();
    if (!def || !pet || this.reviveUsed) return false;
    const revive = unlockedAbilities(def, pet.evolved).find(a => a.kind === 'revive');
    if (!revive) return false;
    this.reviveUsed = true;
    const p = this.player;
    p.hp = Math.max(1, Math.floor(p.maxHp * (revive.value ?? 0.4)));
    EventBus.emit(GameEvents.PLAYER_HEALTH_CHANGED, { hp: p.hp, maxHp: p.maxHp });
    EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('sys.pet.revive', { name: this.host.pets.getPetDisplayName(pet) }), type: 'system' });
    const vfx = this.host.vfx();
    if (vfx) {
      vfx.healBurst(p.sprite.x, p.sprite.y - 16, 24);
      vfx.cameraFlash(220, 0.35, 0xffb040);
    }
    this.host.skillEffects.play('combustion', p.sprite.x, p.sprite.y, p.sprite.x, p.sprite.y);
    this.floatText(p.sprite.x, p.sprite.y - 40, `+${p.hp}`, '#7dff9a');
    return true;
  }

  update(time: number, delta: number): void {
    const pets = this.host.pets;
    const def = pets.getActivePetDef();
    const inst = pets.getActivePetInstance();
    if (!def || !inst) { this.destroyVisual(); return; }
    if (!this.container || this.shownPet !== def.id || this.shownStage !== inst.evolved) this.buildVisual(def, inst.evolved);
    if (!this.container) return;
    this.pruneMarks(time);
    this.maxHp = Math.max(1, Math.round(this.player.maxHp * def.hpFraction));
    if (this.hp > this.maxHp) this.hp = this.maxHp;
    if (this.host.isPaused()) { this.animator?.update(delta); return; }

    const exhausted = this.isExhausted(time);
    if (!exhausted && this.container.alpha < 1 && this.exhaustedUntil > 0) this.recover();
    const peaceful = this.host.inSafeZone(this.player.tileCol, this.player.tileRow);
    if (!peaceful) pets.tickActive(delta);
    if (!exhausted) this.hp = Math.min(this.maxHp, this.hp + this.maxHp * (peaceful ? 0.08 : 0.01) * delta / 1000);

    const target = peaceful ? null : this.pickTarget();
    const heroDist = Math.hypot(this.player.tileCol - this.col, this.player.tileRow - this.row);
    const heroHpRatio = this.player.maxHp > 0 ? this.player.hp / this.player.maxHp : 1;

    // Max bond: the beast answers once when the hero is in danger.
    if (!exhausted && shouldBondRescue(inst.bond, heroHpRatio, time, this.lastRescueAt)) {
      const sig = primaryAbility(def);
      if (sig) {
        this.lastRescueAt = time;
        EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('sys.pet.rescue', { name: pets.getPetDisplayName(inst) }), type: 'combat' });
        this.useAbility(sig, target, time, def, true);
      }
    }

    let action: PetAction = { type: 'follow' };
    if (time >= this.lockedUntil) {
      const targetDist = target ? Math.hypot(target.tileCol - this.col, target.tileRow - this.row) : null;
      action = choosePetAction({
        now: time,
        abilities: unlockedAbilities(def, inst.evolved),
        readyAt: this.readyAt,
        exhausted,
        peaceful,
        heroDist,
        heroHpRatio,
        heroAttackers: this.countHeroAttackers(),
        targetDist,
        targetMarked: !!target && (this.marks.get(target) ?? 0) > time,
        enemiesNearTarget: target ? this.host.monstersNear(target.tileCol, target.tileRow, 3).filter(m => m.isAlive()).length : 0,
        basicRange: def.combat.range,
        basicReadyAt: this.basicReadyAt,
      });
    } else {
      action = { type: 'rest' };
    }

    switch (action.type) {
      case 'ability':
        this.useAbility(action.ability, target, time, def, false);
        break;
      case 'attack':
        if (target) this.basicAttack(target, time, def);
        break;
      case 'approach':
        if (target) this.moveToward(target.tileCol, target.tileRow, Math.max(0.8, def.combat.range * 0.85), delta, def, FOLLOW_SPEED * 1.4);
        break;
      case 'follow':
        this.follow(delta, def, exhausted);
        break;
      case 'rest':
        if (exhausted) this.follow(delta, def, true);
        else if (time >= this.lockedUntil) this.animator?.setIdle();
        break;
    }
    if (target && time >= this.lockedUntil && action.type !== 'follow') this.face(target.tileCol - this.col, target.tileRow - this.row);

    this.syncPosition();
    this.updateLabel(time);
    this.animator?.update(delta);
  }

  destroy(): void {
    this.destroyVisual();
    for (const m of this.marks.keys()) this.clearMark(m);
    this.marks.clear();
    this.taunts.clear();
  }

  // ─── Visual ──────────────────────────────────────────────────────────────

  private buildVisual(def: PetDef, stage: number): void {
    this.destroyVisual();
    const scene = this.scene;
    // Spawn beside the hero (fresh zone / new pet); keep position on evolution.
    if (this.shownPet !== def.id || this.shownStage < 0) {
      this.col = this.player.tileCol - 1.2;
      this.row = this.player.tileRow + 1.2;
      if (!this.host.isWalkable(Math.round(this.col), Math.round(this.row))) {
        this.col = this.player.tileCol;
        this.row = this.player.tileRow;
      }
      this.hp = Math.round(this.player.maxHp * def.hpFraction);
    }
    this.shownPet = def.id;
    this.shownStage = stage;
    const pos = cartToIso(this.col, this.row);
    const c = scene.add.container(pos.x, pos.y).setDepth(pos.y + 55);
    this.container = c;

    const shadow = scene.add.ellipse(0, 3, def.flying ? 16 : 22, def.flying ? 6 : 8, 0x000000, 0.22);
    c.add(shadow);

    const api = SpriteGenerator as unknown as PetSheetApi;
    let sheetKey: string | null = null;
    if (typeof api.ensurePetSheet === 'function') {
      try {
        sheetKey = api.ensurePetSheet(scene, def.id, Math.max(0, Math.min(2, stage)) as 0 | 1 | 2);
      } catch (e) {
        console.warn('[PetCompanion] ensurePetSheet failed', e);
        sheetKey = null;
      }
    }
    if (sheetKey && scene.textures.exists(sheetKey)) {
      const spr = scene.add.sprite(0, -18, sheetKey, 0).setScale(1 / TEXTURE_SCALE);
      c.add(spr);
      this.visual = spr;
      this.animator = new CharacterAnimator(scene, c, getAnimConfig(def.animCategory), sheetKey);
      if (scene.anims.exists(`${sheetKey}_idle`)) spr.play(`${sheetKey}_idle`);
    } else {
      const key = `decor_pet_${def.id}`;
      SpriteGenerator.ensureDecoration(scene, key);
      if (scene.textures.exists(key)) {
        const img = scene.add.image(0, def.flying ? -20 : -8, key).setScale((1 / TEXTURE_SCALE) * (1 + stage * 0.15));
        c.add(img);
        this.visual = img;
        scene.tweens.add({ targets: img, y: img.y - 3, duration: 1100, yoyo: true, repeat: -1, ease: 'Sine.easeInOut' });
      } else {
        const body = scene.add.circle(0, -12, 9, def.combat.color).setStrokeStyle(1.5, 0xffffff, 0.5);
        c.add(body);
      }
    }
    if (stage > 0) {
      // Evolved aura (觉醒 / 至尊)
      const aura = scene.add.image(0, -14, 'fx_glow');
      if (scene.textures.exists('fx_glow')) {
        aura.setTint(def.combat.color).setBlendMode(Phaser.BlendModes.ADD).setScale(0.35 + stage * 0.08, 0.2 + stage * 0.05).setAlpha(0.35);
        c.addAt(aura, 1);
        scene.tweens.add({ targets: aura, alpha: { from: 0.2, to: 0.45 }, duration: 1400, yoyo: true, repeat: -1 });
      } else aura.destroy();
    }

    const top = def.flying ? -46 : -40;
    this.hpBarBg = scene.add.rectangle(-12, top + 8, 24, 3, 0x1a1a1a).setOrigin(0, 0.5).setAlpha(0);
    this.hpBar = scene.add.rectangle(-12, top + 8, 24, 3, 0x6fd35a).setOrigin(0, 0.5).setAlpha(0);
    this.nameLabel = scene.add.text(0, top, '', {
      fontSize: fs(9), color: '#aaddff', fontFamily: FONT,
      stroke: '#000000', strokeThickness: Math.round(2 * DPR),
    }).setOrigin(0.5);
    c.add([this.hpBarBg, this.hpBar, this.nameLabel]);
    this.lastLabel = '';
  }

  private destroyVisual(): void {
    if (!this.container) return;
    this.scene.tweens.killTweensOf(this.container);
    for (const child of this.container.list) this.scene.tweens.killTweensOf(child);
    this.animator?.cleanup();
    this.container.destroy();
    this.container = null;
    this.visual = null;
    this.animator = null;
    this.nameLabel = null;
    this.hpBar = null;
    this.hpBarBg = null;
    this.shownStage = -1;
  }

  private updateLabel(time: number): void {
    const inst = this.host.pets.getActivePetInstance();
    if (!inst || !this.nameLabel) return;
    const tag = this.isExhausted(time) ? ` [${t('zone.pet.exhaustedTag')}]` : '';
    const text = `${this.host.pets.getPetDisplayName(inst)} Lv.${inst.level}${tag}`;
    if (text !== this.lastLabel) { this.nameLabel.setText(text); this.lastLabel = text; }
    if (this.hpBar && this.hpBarBg) {
      const ratio = Math.max(0, this.hp / this.maxHp);
      const show = ratio < 0.999 ? 1 : 0;
      this.hpBarBg.setAlpha(show * 0.8);
      this.hpBar.setAlpha(show).setScale(ratio, 1);
      this.hpBar.setFillStyle(ratio > 0.5 ? 0x6fd35a : ratio > 0.25 ? 0xf39c12 : 0xe74c3c);
    }
  }

  private syncPosition(): void {
    if (!this.container) return;
    const pos = cartToIso(this.col, this.row);
    this.container.setPosition(pos.x, pos.y);
    this.container.setDepth(pos.y + 55);
  }

  private face(dCol: number, dRow: number): void {
    const v = tileDeltaToScreen(dCol, dRow);
    if (this.animator) this.animator.faceToward(v.x, v.y);
    else if (this.visual && Math.abs(v.x) > 0.5) this.visual.setFlipX(v.x < 0);
  }

  private floatText(x: number, y: number, text: string, color: string): void {
    const txt = this.scene.add.text(x, y, text, {
      fontSize: fs(14), color, fontFamily: '"Cinzel", serif', fontStyle: 'bold',
      stroke: '#000000', strokeThickness: Math.round(2 * DPR),
    }).setOrigin(0.5).setDepth(4500);
    this.scene.tweens.add({ targets: txt, y: y - 30, alpha: 0, duration: 1100, ease: 'Power2', onComplete: () => txt.destroy() });
  }

  // ─── Movement ────────────────────────────────────────────────────────────

  private follow(delta: number, def: PetDef, exhausted: boolean): void {
    const p = this.player;
    const tc = p.tileCol - 1.3;
    const tr = p.tileRow + 1.3;
    const d = Math.hypot(tc - this.col, tr - this.row);
    if (d > TELEPORT_DIST) {
      this.col = p.tileCol; this.row = p.tileRow;
      return;
    }
    const speed = d > 4 ? DASH_SPEED : FOLLOW_SPEED;
    this.moveToward(tc, tr, 0.35, delta, def, exhausted ? speed * 0.7 : speed);
  }

  /** Step toward (col,row) until within `stopAt` tiles. Returns true when arrived. */
  private moveToward(col: number, row: number, stopAt: number, delta: number, def: PetDef, speed: number): boolean {
    const dx = col - this.col;
    const dy = row - this.row;
    const d = Math.hypot(dx, dy);
    if (d <= stopAt) {
      if (this.scene.time.now >= this.lockedUntil) this.animator?.setIdle();
      return true;
    }
    const step = Math.min(d - stopAt, speed * delta / 1000);
    const nx = this.col + (dx / d) * step;
    const ny = this.row + (dy / d) * step;
    if (def.flying || this.host.isWalkable(Math.round(nx), Math.round(ny))) {
      this.col = nx; this.row = ny;
    } else if (this.host.isWalkable(Math.round(nx), Math.round(this.row))) {
      this.col = nx;
    } else if (this.host.isWalkable(Math.round(this.col), Math.round(ny))) {
      this.row = ny;
    } else if (d > 6) {
      // Stuck behind something: hop back to the hero.
      this.col = this.player.tileCol; this.row = this.player.tileRow;
    }
    this.face(dx, dy);
    this.animator?.setWalk();
    return false;
  }

  // ─── Targeting ───────────────────────────────────────────────────────────

  private pickTarget(): Monster | null {
    const p = this.player;
    if (p.attackTarget) {
      const m = this.host.findMonster(p.attackTarget);
      if (m && m.isAlive() && Math.hypot(m.tileCol - p.tileCol, m.tileRow - p.tileRow) <= 10) return m;
    }
    let best: Monster | null = null;
    let bestD = Infinity;
    for (const m of this.host.monstersNear(p.tileCol, p.tileRow, 7)) {
      if (!m.isAlive() || !m.isAggro()) continue;
      const d = Math.hypot(m.tileCol - p.tileCol, m.tileRow - p.tileRow);
      if (d < bestD) { bestD = d; best = m; }
    }
    return best;
  }

  private countHeroAttackers(): number {
    const p = this.player;
    let n = 0;
    for (const m of this.host.monstersNear(p.tileCol, p.tileRow, 4)) {
      if (m.isAlive() && m.state === 'attack') n++;
    }
    return n;
  }

  // ─── Damage ──────────────────────────────────────────────────────────────

  private attackDamage(): number {
    return this.host.pets.calculatePetDamage(this.host.heroDamage());
  }

  private petXY(): { x: number; y: number } {
    return this.container ? { x: this.container.x, y: this.container.y } : { x: this.player.sprite.x, y: this.player.sprite.y };
  }

  /** Deal `mult` × pet attack to a monster through Monster.takeDamage. */
  private hit(target: Monster, mult: number, opts: { crit?: boolean; element?: string; color?: number } = {}): number {
    if (!target.isAlive()) return 0;
    const base = this.attackDamage();
    if (base <= 0) return 0;
    const isCrit = !!opts.crit || Math.random() < 0.08;
    const amp = (this.marks.get(target) ?? 0) > this.scene.time.now ? (this.markValue(target)) : 0;
    const dmg = Math.max(1, Math.round(base * mult * (isCrit ? 1.6 : 1) * (1 + amp)));
    const from = this.petXY();
    const weight = target.takeDamage(dmg, from.x, from.y, { isCrit });
    this.host.showDamage(target.sprite.x + 8, target.sprite.y - 10, dmg, isCrit, false, opts.element === 'physical' ? undefined : opts.element);
    const vfx = this.host.vfx();
    if (vfx && weight !== 'tick') {
      const angle = Math.atan2(target.sprite.y - from.y, target.sprite.x - from.x);
      vfx.impactBurst(target.sprite.x, target.sprite.y - 18, angle, weight === 'kill' ? 'heavy' : weight, opts.color ?? 0xfff2c0);
    }
    this.animator?.triggerHitFreeze(Math.round(HIT_PROFILES[weight].attackerStopMs * 0.6));
    if (!target.isAlive()) this.host.onMonsterKilled(target);
    return dmg;
  }

  private markValue(target: Monster): number {
    let v = 0;
    for (const b of target.buffs) if (b.tag === TAG_MARK) v = Math.max(v, b.value);
    return v;
  }

  private takeHit(monster: Monster): void {
    if (!this.container || !monster.isAlive() || this.isExhausted()) return;
    const raw = monster.definition.damage * (0.85 + Math.random() * 0.3);
    const dmg = Math.max(1, Math.round(raw * 0.8));
    this.hp -= dmg;
    this.host.showDamage(this.container.x, this.container.y - 12, dmg, false, true);
    this.host.skillEffects.playMonsterAttack(this.container.x, this.container.y);
    if (this.animator) {
      this.animator.flashWhite(70);
      this.animator.playHurt(monster.sprite.x, monster.sprite.y, 0.6);
    }
    if (this.hp <= 0) this.exhaust();
  }

  private exhaust(): void {
    const now = this.scene.time.now;
    this.hp = 0;
    this.exhaustedUntil = now + PET_EXHAUST_MS;
    this.lockedUntil = 0;
    for (const id of this.taunts.keys()) this.taunts.set(id, 0);
    this.container?.setAlpha(0.45);
    const inst = this.host.pets.getActivePetInstance();
    if (inst) EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('sys.pet.exhausted', { name: this.host.pets.getPetDisplayName(inst) }), type: 'combat' });
  }

  private recover(): void {
    this.hp = this.maxHp;
    this.exhaustedUntil = 0;
    this.container?.setAlpha(1);
    const vfx = this.host.vfx();
    if (vfx && this.container) vfx.healBurst(this.container.x, this.container.y - 12, 6);
  }

  private pruneMarks(now: number): void {
    for (const [m, until] of this.marks) {
      if (until <= now || !m.isAlive()) { this.clearMark(m); this.marks.delete(m); }
    }
  }

  private clearMark(m: Monster): void {
    m.buffs = m.buffs.filter(b => b.tag !== TAG_MARK);
  }

  // ─── Actions ─────────────────────────────────────────────────────────────

  private basicAttack(target: Monster, time: number, def: PetDef): void {
    this.basicReadyAt = time + def.combat.attackMs;
    const from = this.petXY();
    const tx = target.sprite.x, ty = target.sprite.y;
    this.face(target.tileCol - this.col, target.tileRow - this.row);
    if (def.combat.style === 'melee') {
      const contact = this.animator ? this.animator.playAttack(tx, ty, { attackIntervalMs: def.combat.attackMs }) : this.lunge(tx, ty);
      this.lockedUntil = time + contact + 80;
      this.scene.time.delayedCall(contact, () => {
        if (!target.isAlive()) return;
        this.hit(target, 1, { element: def.combat.element, color: def.combat.color });
        this.host.skillEffects.playAttack(from.x, from.y, target.sprite.x, target.sprite.y, false);
      });
    } else {
      const release = this.animator ? this.animator.playCast(tx, ty) : this.lunge(tx, ty, 0.25);
      this.lockedUntil = time + release + 60;
      this.scene.time.delayedCall(release, () => {
        if (!target.isAlive() || !this.container) return;
        const p = this.petXY();
        this.host.skillEffects.playMonsterRangedAttack(p.x, p.y - 8, target.sprite.x, target.sprite.y, def.combat.color, () => {
          if (target.isAlive()) this.hit(target, 1, { element: def.combat.element, color: def.combat.color });
        });
      });
    }
  }

  /** Fallback attack motion for the floating image: a quick hop toward the target. */
  private lunge(tx: number, ty: number, amount = 0.45): number {
    if (!this.visual) return 120;
    const v = this.visual;
    const base = { x: v.x, y: v.y };
    const from = this.petXY();
    const dx = (tx - from.x) * 0.12 * amount, dy = (ty - from.y) * 0.12 * amount;
    this.scene.tweens.add({ targets: v, x: base.x + Math.max(-14, Math.min(14, dx)), y: base.y + Math.max(-10, Math.min(10, dy)), duration: 110, yoyo: true, ease: 'Quad.easeOut', onComplete: () => v.setPosition(base.x, v.y) });
    return 110;
  }

  private startCooldown(a: PetAbilityDef, time: number): void {
    this.readyAt[a.id] = time + a.cooldownMs;
  }

  /** Play one ability. `forced` (bond rescue) ignores range and uses the hero's surroundings. */
  private useAbility(a: PetAbilityDef, target: Monster | null, time: number, def: PetDef, forced: boolean): void {
    this.startCooldown(a, time);
    const fx = this.host.skillEffects;
    const vfx = this.host.vfx();
    const p = this.player;
    const from = this.petXY();
    const castMs = this.animator
      ? (target ? this.animator.playCast(target.sprite.x, target.sprite.y) : this.animator.playCast())
      : this.lunge(target?.sprite.x ?? p.sprite.x, target?.sprite.y ?? p.sprite.y, 0.2);
    this.lockedUntil = time + castMs + 120;
    const element = a.element ?? def.combat.element;
    const color = element === 'fire' ? 0xff7a2a : element === 'arcane' ? 0xcc44cc : def.combat.color;

    const later = (fn: () => void): void => { this.scene.time.delayedCall(castMs, fn); };

    switch (a.kind) {
      case 'heal': {
        later(() => {
          const amount = Math.max(1, Math.floor(p.maxHp * (a.value ?? 0.08)));
          p.hp = Math.min(p.maxHp, p.hp + amount);
          EventBus.emit(GameEvents.PLAYER_HEALTH_CHANGED, { hp: p.hp, maxHp: p.maxHp });
          fx.play('life_regen', p.sprite.x, p.sprite.y);
          vfx?.healBurst(p.sprite.x, p.sprite.y - 16, 10);
          this.floatText(p.sprite.x, p.sprite.y - 44, `+${amount}`, '#7dff9a');
        });
        break;
      }
      case 'shield': {
        later(() => {
          this.heroBuff('damageReduction', a.value ?? 0.2, a.durationMs ?? 5000, TAG_SHIELD);
          fx.play('shield_wall', p.sprite.x, p.sprite.y);
        });
        break;
      }
      case 'buff': {
        later(() => {
          this.heroBuff('damageBonus', a.value ?? 0.15, a.durationMs ?? 6000, TAG_HOWL);
          fx.play('frenzy', p.sprite.x, p.sprite.y);
          if (this.container) fx.play('taunt_roar', this.container.x, this.container.y);
        });
        break;
      }
      case 'taunt': {
        later(() => {
          const until = this.scene.time.now + (a.durationMs ?? 5000);
          for (const m of this.host.monstersNear(Math.round(this.col), Math.round(this.row), a.radius ?? 4)) {
            if (m.isAlive()) this.taunts.set(m.id, until);
          }
          this.heroBuff('damageReduction', a.value ?? 0.25, a.durationMs ?? 5000, TAG_SHIELD);
          if (this.container) fx.play('taunt_roar', this.container.x, this.container.y);
          fx.play('shield_wall', p.sprite.x, p.sprite.y);
        });
        break;
      }
      case 'mark': {
        if (!target) break;
        later(() => {
          if (!target.isAlive()) return;
          const dur = a.durationMs ?? 6000;
          this.clearMark(target);
          target.buffs.push({ stat: 'damageAmplify', value: a.value ?? 0.15, duration: dur, startTime: this.scene.time.now, tag: TAG_MARK });
          this.marks.set(target, this.scene.time.now + dur);
          fx.play('death_mark', from.x, from.y, target.sprite.x, target.sprite.y);
        });
        break;
      }
      case 'strike': {
        if (!target) break;
        if (a.leap) {
          // Leap beside the target before the blow lands.
          const d = Math.hypot(target.tileCol - this.col, target.tileRow - this.row) || 1;
          const dc = (target.tileCol - this.col) / d, dr = (target.tileRow - this.row) / d;
          const endCol = target.tileCol - dc * 0.9, endRow = target.tileRow - dr * 0.9;
          const startCol = this.col, startRow = this.row;
          const leapMs = Math.max(120, castMs);
          this.scene.tweens.addCounter({
            from: 0, to: 1, duration: leapMs, ease: 'Quad.easeOut',
            onUpdate: (tw) => {
              const u = tw.getValue() ?? 0;
              this.col = startCol + (endCol - startCol) * u;
              this.row = startRow + (endRow - startRow) * u;
              this.syncPosition();
            },
          });
          fx.play('charge', from.x, from.y, target.sprite.x, target.sprite.y);
        }
        const hits = Math.max(1, a.hits ?? 1);
        for (let i = 0; i < hits; i++) {
          this.scene.time.delayedCall(castMs + i * 140, () => {
            if (!target.isAlive()) return;
            const dmg = this.hit(target, a.damage ?? 1.5, { crit: a.crit, element, color });
            if (a.crit) fx.play('backstab', this.petXY().x, this.petXY().y, target.sprite.x, target.sprite.y);
            else if (a.bleed) fx.play('bleed_strike', this.petXY().x, this.petXY().y, target.sprite.x, target.sprite.y);
            else fx.play('slash', this.petXY().x, this.petXY().y, target.sprite.x, target.sprite.y);
            if (a.bleed && target.isAlive() && dmg > 0) {
              this.host.statusEffects.apply(target.id, 'bleed', Math.max(1, Math.round(dmg * a.bleed)), a.durationMs ?? 4000, 'pet', this.scene.time.now);
            }
          });
        }
        this.lockedUntil = time + castMs + hits * 140 + 120;
        break;
      }
      case 'bolt': {
        if (!target) break;
        later(() => {
          if (!target.isAlive() || !this.container) return;
          const s = this.petXY();
          fx.playMonsterRangedAttack(s.x, s.y - 8, target.sprite.x, target.sprite.y, color, () => {
            if (a.radius) {
              for (const m of this.host.monstersNear(target.tileCol, target.tileRow, a.radius)) {
                if (m !== target && m.isAlive()) this.hit(m, (a.damage ?? 1.5) * 0.5, { element, color });
              }
              fx.play('combustion', target.sprite.x, target.sprite.y, target.sprite.x, target.sprite.y);
            }
            if (target.isAlive()) this.hit(target, a.damage ?? 1.5, { element, color });
            if (a.mana) {
              const gain = Math.max(1, Math.floor(p.maxMana * a.mana));
              p.mana = Math.min(p.maxMana, p.mana + gain);
              EventBus.emit(GameEvents.PLAYER_MANA_CHANGED, { mana: p.mana, maxMana: p.maxMana });
              this.floatText(p.sprite.x, p.sprite.y - 44, `+${gain}`, '#6fb6ff');
            }
          });
        });
        break;
      }
      case 'cone': {
        if (!target) break;
        later(() => {
          const ox = this.col, oy = this.row;
          const aim = Math.atan2(target.tileRow - oy, target.tileCol - ox);
          const half = (a.arc ?? Math.PI / 3) / 2;
          const len = a.radius ?? 4;
          for (const m of this.host.monstersNear(Math.round(ox), Math.round(oy), len + 1)) {
            if (!m.isAlive()) continue;
            const dx = m.tileCol - ox, dy = m.tileRow - oy;
            const dist = Math.hypot(dx, dy);
            if (dist > len) continue;
            let da = Math.abs(Math.atan2(dy, dx) - aim);
            if (da > Math.PI) da = Math.PI * 2 - da;
            if (dist > 0.6 && da > half) continue;
            const dmg = this.hit(m, a.damage ?? 1.3, { element, color });
            if (a.burn && dmg > 0) this.host.statusEffects.apply(m.id, 'burn', Math.max(1, Math.round(dmg * a.burn)), a.durationMs ?? 3000, 'pet', this.scene.time.now);
          }
          // Flame tongue along the cone's axis.
          const s = this.petXY();
          for (let i = 1; i <= 3; i++) {
            const pt = cartToIso(ox + Math.cos(aim) * len * i / 3, oy + Math.sin(aim) * len * i / 3);
            this.scene.time.delayedCall(i * 60, () => vfx?.impactBurst(pt.x, pt.y - 14, Math.atan2(pt.y - s.y, pt.x - s.x), 'heavy', color));
          }
          fx.play('combustion', target.sprite.x, target.sprite.y, target.sprite.x, target.sprite.y);
        });
        break;
      }
      case 'nova': {
        const center = a.self || !target ? null : target;
        later(() => {
          const cc = center ? center.tileCol : this.col;
          const cr = center ? center.tileRow : this.row;
          const cx = center ? center.sprite.x : this.petXY().x;
          const cy = center ? center.sprite.y : this.petXY().y;
          if (a.self) fx.play('war_stomp', cx, cy);
          else if (element === 'fire') fx.play('combustion', cx, cy, cx, cy);
          else fx.play('arcane_torrent', cx, cy, cx, cy);
          const now = this.scene.time.now;
          for (const m of this.host.monstersNear(Math.round(cc), Math.round(cr), (a.radius ?? 2.5) + 1)) {
            if (!m.isAlive() || Math.hypot(m.tileCol - cc, m.tileRow - cr) > (a.radius ?? 2.5)) continue;
            const dmg = this.hit(m, a.damage ?? 1, { element, color });
            if (!m.isAlive()) continue;
            if (a.stunMs) this.host.statusEffects.apply(m.id, 'stun', 1, a.stunMs, 'pet', now);
            if (a.slow) this.host.statusEffects.apply(m.id, 'slow', a.slow, a.durationMs ?? 3000, 'pet', now);
            if (a.burn && dmg > 0) this.host.statusEffects.apply(m.id, 'burn', Math.max(1, Math.round(dmg * a.burn)), a.durationMs ?? 3000, 'pet', now);
          }
        });
        break;
      }
      case 'revive':
        break;
    }
    if (forced) this.lockedUntil = Math.max(this.lockedUntil, time + 400);
  }

  private heroBuff(stat: string, value: number, duration: number, tag: string): void {
    const p = this.player;
    p.buffs = p.buffs.filter(b => b.tag !== tag);
    p.buffs.push({ stat, value, duration, startTime: this.scene.time.now, tag });
  }
}
