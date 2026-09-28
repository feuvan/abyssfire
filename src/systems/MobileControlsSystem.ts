import Phaser from 'phaser';
import { GAME_WIDTH, GAME_HEIGHT, RENDER_SCALE, logicalPointer } from '../config';
import { EventBus, GameEvents } from '../utils/EventBus';
import type { Player } from '../entities/Player';
import { t } from '../i18n';
import { getSkillName } from '../i18n/gameAccessors';
import { getLearnedSkillLoadout } from './SkillProgressionSystem';
import { getSkillCooldown } from './CombatSystem';
import { addButton, joystickTextures, medallionTexture, type UiButton } from '../ui/UiKit';
import { ensureHudIcon, type HudIconId } from '../ui/HudIcons';
import { ensureGlyph } from '../graphics/icons/UiGlyphs';

const FONT = '"Noto Sans SC", sans-serif';

/** Detect touch-capable mobile/tablet devices */
export function isMobileDevice(): boolean {
  if (typeof navigator === 'undefined') return false;
  const ua = navigator.userAgent || '';
  const isTouchDevice = 'ontouchstart' in window || navigator.maxTouchPoints > 0;
  const isMobileUA = /Android|iPhone|iPad|iPod|webOS|BlackBerry|IEMobile|Opera Mini/i.test(ua);
  // Consider tablets too: small screen or mobile UA with touch
  const isSmallScreen = window.innerWidth <= 1024;
  return isTouchDevice && (isMobileUA || isSmallScreen);
}

/**
 * Touch-control sizes in CSS px (the unit fingers care about). They are converted to game px
 * with the live canvas scale, so a 56 px button stays 56 px on screen on any phone.
 */
const CSS = {
  margin: 12,
  joystickR: 58,
  cornerR: 36,
  skillR: 28,
  dodgeR: 25,
  /** Ring radii of the skill fan, measured from the corner button's centre. */
  ring1: 100,
  ring2: 162,
  panelBtn: 44,
  panelGap: 4,
  toggleW: 66,
  toggleH: 44,
  toggleGap: 6,
} as const;

/** Game px per CSS px is clamped so the layout still fits 1280×720 on small phones / big tablets. */
const MIN_K = 1;
const MAX_K = 2;

/** Touch pointers Phaser should track (joystick + skill + a spare). */
const TOUCH_POINTERS = 4;

/** Inner ring (skills 1-4) and outer ring (dodge, skills 5-6) angles, in degrees (0 = right, 90 = down). */
const RING1_ANGLES = [178, 210, 242, 274];
const RING2_DODGE = 190;
const RING2_ANGLES = [220, 250];

interface JoystickState {
  active: boolean;
  pointerId: number;
  dx: number;
  dy: number;
}

interface SkillButton {
  container: Phaser.GameObjects.Container;
  sweep: Phaser.GameObjects.Graphics;
  cdText: Phaser.GameObjects.Text;
  radius: number;
  lastFrac: number;
}

/**
 * On-screen touch controls (phones / tablets), laid out for landscape play:
 *   bottom-left  – virtual joystick
 *   bottom-right – big lock-on / attack button in the corner with the skills fanned around it
 *                  (skills 1-4 on the inner ring, dodge + skills 5-6 on the outer ring)
 *   top-left     – auto-combat, auto-loot and combat-log toggles
 *   top-right    – panel buttons (bag, character, skills, map, homestead, ley-beasts, quests)
 * Everything lives in ZoneScene under one counter-zoomed root container; UIScene lays out
 * its HUD around these regions.
 */
export class MobileControlsSystem {
  private scene: Phaser.Scene;
  private player: Player;

  // Joystick elements
  private joystickThumb: Phaser.GameObjects.Image | null = null;
  private joystickContainer: Phaser.GameObjects.Container | null = null;
  private joystickState: JoystickState = { active: false, pointerId: -1, dx: 0, dy: 0 };
  private joystickRadius = 0;
  private joystickCenterX = 0;
  private joystickCenterY = 0;

  // Skill fan
  private skillLoadout: Player['classData']['skills'] = [];
  private skillButtons: SkillButton[] = [];
  private dodge: SkillButton | null = null;
  private dodgeReady = true;

  // Everything else that gets rebuilt on layout changes
  private controls: Phaser.GameObjects.GameObject[] = [];
  private autoCombatBtn: UiButton | null = null;
  private autoLootBtn: UiButton | null = null;

  /**
   * Root container for every touch control. The gameplay camera is zoomed, and
   * scroll-factor-0 objects are still scaled around the camera centre, so the
   * root counter-scales by 1/zoom to keep controls at their screen positions.
   */
  private root!: Phaser.GameObjects.Container;
  private appliedZoom = 0;

  /** Game px per CSS px the current layout was built for. */
  private k = 0;
  private visible = true;
  /** The touch that most recently landed on a control (see {@link claimsPointer}). */
  private claimed: { id: number; downTime: number } | null = null;

  private readonly pointerMoveHandler = (pointer: Phaser.Input.Pointer): void => {
    if (this.joystickState.active && pointer.id === this.joystickState.pointerId) {
      { const lp = logicalPointer(pointer); this.updateJoystickThumb(lp.x, lp.y); }
    }
  };
  private readonly pointerUpHandler = (pointer: Phaser.Input.Pointer): void => {
    if (pointer.id === this.joystickState.pointerId) this.releaseJoystick();
  };
  private readonly resizeHandler = (): void => {
    // The display size settles after the resize event; rebuild on the next tick.
    this.scene.time.delayedCall(0, () => this.relayout());
  };

  constructor(scene: Phaser.Scene, player: Player) {
    this.scene = scene;
    this.player = player;
    this.refreshSkillLoadout();

    // Joystick + skill + spare finger: Phaser tracks one touch by default.
    const input = scene.input;
    const extra = TOUCH_POINTERS - (input.manager.pointersTotal - 1);
    if (extra > 0) input.addPointer(extra);

    this.root = this.scene.add.container(0, 0).setDepth(5000).setScrollFactor(0);
    this.build();
    this.applyCameraZoom();
    this.scene.input.on('pointermove', this.pointerMoveHandler);
    this.scene.input.on('pointerup', this.pointerUpHandler);
    this.scene.scale.on(Phaser.Scale.Events.RESIZE, this.resizeHandler);
  }

  /** Game px per CSS px of the canvas right now (clamped). */
  private measureK(): number {
    const ds = this.scene.scale.displayScale;
    const k = Math.max(ds?.x || 1, ds?.y || 1);
    return Phaser.Math.Clamp(Number.isFinite(k) ? k : 1, MIN_K, MAX_K);
  }

  /**
   * True when `pointer`'s current press started on a touch control, so ZoneScene must not
   * also treat it as a tap-to-move / tap-to-attack on the world.
   */
  claimsPointer(pointer: Phaser.Input.Pointer): boolean {
    return !!this.claimed && this.claimed.id === pointer.id && this.claimed.downTime === pointer.downTime;
  }

  private claim(pointer: Phaser.Input.Pointer): void {
    this.claimed = { id: pointer.id, downTime: pointer.downTime };
  }

  /** Put an object under the root and make it (and its children) screen-fixed. */
  private attach<T extends Phaser.GameObjects.GameObject>(obj: T): T {
    this.root.add(obj);
    const fix = (o: Phaser.GameObjects.GameObject) => {
      (o as unknown as Phaser.GameObjects.Components.ScrollFactor).setScrollFactor?.(0);
      if (o instanceof Phaser.GameObjects.Container) o.list.forEach(fix);
    };
    fix(obj);
    this.controls.push(obj);
    return obj;
  }

  /** Counter the gameplay camera zoom so controls render at 1:1 screen coordinates. */
  private applyCameraZoom(): void {
    const cam = this.scene.cameras.main;
    const z = cam.zoom || 1;
    if (z === this.appliedZoom) return;
    this.appliedZoom = z;
    const cx = cam.width * cam.originX, cy = cam.height * cam.originY;
    // Lay the controls out in logical pixels: undo the gameplay zoom, keep the render scale.
    this.root.setScale(RENDER_SCALE / z);
    this.root.setPosition(cx - cx / z, cy - cy / z);
  }

  private refreshSkillLoadout(): void {
    this.skillLoadout = getLearnedSkillLoadout(
      this.player.classData.skills,
      this.player.skillLevels,
    );
  }

  /** Current movement direction from joystick (in tile-space dx/dy) */
  getDirection(): { dx: number; dy: number } {
    if (!this.joystickState.active) return { dx: 0, dy: 0 };
    // Convert screen-space joystick direction to isometric tile-space
    // Screen right = tile (+col, -row), Screen down = tile (+col, +row)
    const jx = this.joystickState.dx;
    const jy = this.joystickState.dy;
    return { dx: jx + jy, dy: -jx + jy };
  }

  // ── Layout ────────────────────────────────────────────────────────────

  private build(): void {
    this.k = this.measureK();
    this.createJoystick();
    this.createSkillFan();
    this.createToggles();
    this.createPanelButtons();
    this.setVisible(this.visible);
  }

  private clear(): void {
    this.releaseJoystick();
    for (const o of this.controls) o.destroy();
    this.controls = [];
    this.skillButtons = [];
    this.dodge = null;
    this.joystickContainer = null;
    this.joystickThumb = null;
    this.autoCombatBtn = null;
    this.autoLootBtn = null;
  }

  private relayout(force = false): void {
    if (!this.root.active) return;
    if (!force && Math.abs(this.measureK() - this.k) < 0.01) return;
    this.clear();
    this.build();
  }

  private px(cssPx: number): number {
    return Math.round(cssPx * this.k);
  }

  private createJoystick(): void {
    const r = this.px(CSS.joystickR);
    const m = this.px(CSS.margin);
    const cx = m + r + this.px(6);
    const cy = GAME_HEIGHT - m - r;
    this.joystickRadius = r;
    this.joystickCenterX = cx;
    this.joystickCenterY = cy;

    const container = this.scene.add.container(0, 0);
    const tex = joystickTextures(this.scene, r);
    container.add(this.scene.add.image(cx, cy, tex.base));
    this.joystickThumb = this.scene.add.image(cx, cy, tex.thumb);
    container.add(this.joystickThumb);

    // Generous invisible grab area around the stick (thumbs rarely land dead centre)
    const grab = r * 1.35;
    const zone = this.scene.add.rectangle(cx, cy, grab * 2, grab * 2, 0x000000, 0)
      .setInteractive({ hitArea: new Phaser.Geom.Circle(grab, grab, grab), hitAreaCallback: Phaser.Geom.Circle.Contains });
    container.add(zone);
    zone.on('pointerdown', (pointer: Phaser.Input.Pointer) => {
      this.claim(pointer);
      this.joystickState.active = true;
      this.joystickState.pointerId = pointer.id;
      { const lp = logicalPointer(pointer); this.updateJoystickThumb(lp.x, lp.y); }
    });
    this.joystickContainer = container;
    this.attach(container);
  }

  private releaseJoystick(): void {
    this.joystickState.active = false;
    this.joystickState.pointerId = -1;
    this.joystickState.dx = 0;
    this.joystickState.dy = 0;
    this.joystickThumb?.setPosition(this.joystickCenterX, this.joystickCenterY);
  }

  private updateJoystickThumb(px: number, py: number): void {
    const r = this.joystickRadius;
    const cx = this.joystickCenterX, cy = this.joystickCenterY;
    let dx = px - cx;
    let dy = py - cy;
    const dist = Math.sqrt(dx * dx + dy * dy);
    if (dist > r) {
      dx = (dx / dist) * r;
      dy = (dy / dist) * r;
    }
    this.joystickThumb?.setPosition(cx + dx, cy + dy);
    // Normalize to -1..1
    this.joystickState.dx = dx / r;
    this.joystickState.dy = dy / r;
  }

  /** Centre of the big corner button; the skill fan is laid out around it. */
  private cornerCenter(): { x: number; y: number } {
    const off = this.px(CSS.margin + CSS.cornerR);
    return { x: GAME_WIDTH - off, y: GAME_HEIGHT - off };
  }

  private ringPos(radiusCss: number, deg: number): { x: number; y: number } {
    const c = this.cornerCenter();
    const a = Phaser.Math.DegToRad(deg);
    return { x: c.x + Math.cos(a) * this.px(radiusCss), y: c.y + Math.sin(a) * this.px(radiusCss) };
  }

  private createSkillFan(): void {
    // Corner: lock-on / attack
    const c = this.cornerCenter();
    this.createRoundButton(c.x, c.y, this.px(CSS.cornerR), 0x6a2a24, {
      icon: ensureGlyph(this.scene, 'atk'),
      label: t('sys.mobile.target'),
      onPress: () => EventBus.emit(GameEvents.UI_TARGET_CYCLE, {}),
    });

    // Dodge on the outer ring, nearest the bottom edge (easy thumb roll)
    const dp = this.ringPos(CSS.ring2, RING2_DODGE);
    this.dodge = this.createRoundButton(dp.x, dp.y, this.px(CSS.dodgeR), 0x1f4a6a, {
      label: t('sys.mobile.dodge'),
      onPress: () => EventBus.emit(GameEvents.UI_DODGE_REQUEST, this.getDirection()),
    });
    this.dodgeReady = true;

    const skills = this.skillLoadout.slice(0, RING1_ANGLES.length + RING2_ANGLES.length);
    skills.forEach((skill, i) => {
      const pos = i < RING1_ANGLES.length
        ? this.ringPos(CSS.ring1, RING1_ANGLES[i])
        : this.ringPos(CSS.ring2, RING2_ANGLES[i - RING1_ANGLES.length]);
      const iconKey = `skill_icon_${skill.id}`;
      const btn = this.createRoundButton(pos.x, pos.y, this.px(CSS.skillR), 0x2a2430, {
        icon: this.scene.textures.exists(iconKey) ? iconKey : undefined,
        label: this.scene.textures.exists(iconKey) ? undefined : getSkillName(skill.id, skill.name).slice(0, 2),
        onPress: () => EventBus.emit(GameEvents.UI_SKILL_CLICK, { index: i, skillId: skill.id }),
      });
      this.skillButtons.push(btn);
    });
  }

  /** Round medallion with an icon and/or label, a radial cooldown sweep and seconds text. */
  private createRoundButton(
    x: number, y: number, r: number, color: number,
    opts: { icon?: string; label?: string; onPress: () => void },
  ): SkillButton {
    const container = this.scene.add.container(x, y);
    const bg = this.scene.add.image(0, 0, medallionTexture(this.scene, r * 2, color));
    container.add(bg);
    if (opts.icon) {
      const s = opts.label ? r * 1.05 : r * 1.45;
      container.add(this.scene.add.image(0, opts.label ? -r * 0.18 : 0, opts.icon).setDisplaySize(s, s));
    }
    if (opts.label) {
      container.add(this.scene.add.text(0, opts.icon ? r * 0.5 : 0, opts.label, {
        fontSize: `${Math.round(opts.icon ? Math.max(this.px(10), 18) : Math.max(this.px(12), 20))}px`,
        color: '#f0e6d6',
        fontFamily: FONT,
        fontStyle: 'bold',
        align: 'center',
        stroke: '#000000',
        strokeThickness: 3,
      }).setOrigin(0.5));
    }
    const sweep = this.scene.add.graphics();
    container.add(sweep);
    const cdText = this.scene.add.text(0, 0, '', {
      fontSize: `${Math.round(r * 0.8)}px`,
      color: '#ffffff',
      fontFamily: FONT,
      fontStyle: 'bold',
      stroke: '#000000',
      strokeThickness: 4,
    }).setOrigin(0.5).setVisible(false);
    container.add(cdText);

    // Hit area slightly larger than the art so near misses still count
    bg.setInteractive({
      hitArea: new Phaser.Geom.Circle(bg.width / 2, bg.height / 2, r * 1.12),
      hitAreaCallback: Phaser.Geom.Circle.Contains,
      useHandCursor: false,
    });
    bg.on('pointerdown', (pointer: Phaser.Input.Pointer) => {
      this.claim(pointer);
      container.setScale(0.92);
      opts.onPress();
    });
    const release = (): void => { container.setScale(1); };
    bg.on('pointerup', release);
    bg.on('pointerout', release);

    this.attach(container);
    return { container, sweep, cdText, radius: r, lastFrac: -1 };
  }

  private createToggles(): void {
    const m = this.px(CSS.margin);
    const w = this.px(CSS.toggleW), h = this.px(CSS.toggleH), gap = this.px(CSS.toggleGap);
    const y = m + h / 2;
    const fontSize = Math.max(this.px(11), 18);
    let x = m;

    this.autoCombatBtn = this.addToggle(x + w / 2, y, w, h, 'auto', fontSize, () => {
      this.player.autoCombat = !this.player.autoCombat;
      EventBus.emit(GameEvents.LOG_MESSAGE, {
        text: t('sys.mobile.autoCombat.log', { state: this.player.autoCombat ? t('zone.combat.autoCombatOn') : t('zone.combat.autoCombatOff') }),
        type: 'system',
      });
    });
    x += w + gap;
    this.autoLootBtn = this.addToggle(x + w / 2, y, w, h, 'loot', fontSize, () => {
      const modes: Array<'off' | 'all' | 'magic' | 'rare' | 'legendary'> = ['off', 'all', 'magic', 'rare', 'legendary'];
      const idx = modes.indexOf(this.player.autoLootMode);
      this.player.autoLootMode = modes[(idx + 1) % modes.length];
    });
    x += w + gap;
    // Combat log expand / collapse (UIScene owns the log)
    this.addToggle(x + h / 2, y, h, h, 'log', fontSize, () => EventBus.emit('ui:toggleCombatLog'), t('sys.mobile.log'));
    this.updateToggleLabels(true);
  }

  /** Framed toggle: icon on the left (or on top when square), state text beside / under it. */
  private addToggle(
    x: number, y: number, w: number, h: number, icon: HudIconId, fontSize: number,
    onClick: () => void, fixedLabel?: string,
  ): UiButton {
    const square = w <= h;
    const btn = addButton(this.scene, x, y, w, h, fixedLabel ?? '', {
      variant: 'secondary',
      fontSize: square ? Math.max(this.px(9), 16) : fontSize,
      onClick: (pointer) => { this.claim(pointer); onClick(); },
    });
    btn.bg.setAlpha(0.9);
    btn.label.setLineSpacing(-2);
    const iconSize = square ? h * 0.52 : h * 0.62;
    const img = this.scene.add.image(square ? 0 : -w / 2 + iconSize / 2 + this.px(4), square ? -h * 0.14 : 0, ensureHudIcon(this.scene, icon))
      .setDisplaySize(iconSize, iconSize);
    btn.addAt(img, 1);
    if (square) btn.setLabelY(h * 0.3);
    else btn.label.setX(iconSize / 2 + this.px(2));
    this.attach(btn);
    return btn;
  }

  private updateToggleLabels(force = false): void {
    const setLabel = (btn: UiButton | null, text: string, color: string): void => {
      if (!btn?.active) return;
      const label = btn.label;
      if (force || label.text !== text) label.setText(text);
      if (force || label.style.color !== color) label.setColor(color);
    };
    const on = this.player.autoCombat;
    setLabel(this.autoCombatBtn, on ? t('sys.mobile.autoCombat.on') : t('sys.mobile.autoCombat.off'), on ? '#8ff07a' : '#b0a8b4');
    const mode = this.player.autoLootMode;
    const lootColors: Record<string, string> = { off: '#b0a8b4', all: '#e0d8cc', magic: '#4f8cff', rare: '#ffd84a', legendary: '#ff8a2a' };
    setLabel(this.autoLootBtn, t(`ui.hud.autoLoot.${mode}`), lootColors[mode] ?? '#b0a8b4');
  }

  private createPanelButtons(): void {
    const size = this.px(CSS.panelBtn);
    const gap = this.px(CSS.panelGap);
    const m = this.px(CSS.margin);
    const panels: { key: string; panel: string; icon: HudIconId }[] = [
      { key: 'sys.mobile.panel.inventory', panel: 'inventory', icon: 'inventory' },
      { key: 'sys.mobile.panel.character', panel: 'character', icon: 'character' },
      { key: 'sys.mobile.panel.skills', panel: 'skills', icon: 'skills' },
      { key: 'sys.mobile.panel.map', panel: 'map', icon: 'map' },
      { key: 'sys.mobile.panel.homestead', panel: 'homestead', icon: 'homestead' },
      { key: 'sys.mobile.panel.pets', panel: 'pets', icon: 'pets' },
      { key: 'sys.mobile.panel.quest', panel: 'quest', icon: 'quest' },
    ];

    // Top-right horizontal row: icon with a small caption
    const startX = GAME_WIDTH - m - panels.length * size - (panels.length - 1) * gap;
    const y = m + size / 2;
    panels.forEach((p, i) => {
      const x = startX + i * (size + gap) + size / 2;
      const btn = addButton(this.scene, x, y, size, size, t(p.key), {
        variant: 'ghost',
        fontSize: Math.max(this.px(9), 16),
        onClick: (pointer) => {
          this.claim(pointer);
          EventBus.emit(GameEvents.UI_TOGGLE_PANEL, { panel: p.panel });
        },
      });
      btn.bg.setAlpha(0.88);
      const iconSize = size * 0.56;
      btn.addAt(this.scene.add.image(0, -size * 0.13, ensureHudIcon(this.scene, p.icon)).setDisplaySize(iconSize, iconSize), 1);
      btn.setLabelY(size * 0.3);
      if (btn.label.width > size - 4) btn.label.setScale((size - 4) / btn.label.width);
      this.attach(btn);
    });
  }

  // ── Per-frame ─────────────────────────────────────────────────────────

  update(time: number, _delta: number): void {
    this.applyCameraZoom();
    this.updateToggleLabels();

    // Skill cooldown sweeps
    for (let i = 0; i < this.skillButtons.length; i++) {
      const skill = this.skillLoadout[i];
      if (!skill) break;
      const cdEnd = this.player.skillCooldowns.get(skill.id) ?? 0;
      const remaining = cdEnd - time;
      const total = getSkillCooldown(skill, this.player.getSkillLevel(skill.id));
      this.updateCooldown(this.skillButtons[i], remaining, total);
    }

    // Dodge cooldown (the desktop "闪避 [SPACE]" plate is hidden on touch devices)
    const zone = this.scene as unknown as { getDodgeCooldownRemaining?: () => number };
    if (this.dodge && zone.getDodgeCooldownRemaining) {
      const remaining = zone.getDodgeCooldownRemaining();
      const ready = remaining <= 0;
      if (ready !== this.dodgeReady) {
        this.dodgeReady = ready;
        this.dodge.container.setAlpha(ready ? 1 : 0.7);
      }
      this.updateCooldown(this.dodge, remaining, 0);
    }
  }

  private updateCooldown(btn: SkillButton, remaining: number, total: number): void {
    const onCd = remaining > 0;
    if (!onCd) {
      if (btn.lastFrac !== -1) {
        btn.lastFrac = -1;
        btn.sweep.clear();
        btn.cdText.setVisible(false);
        this.scene.tweens.add({ targets: btn.container, scale: { from: 1.12, to: 1 }, duration: 180, ease: 'Quad.easeOut' });
      }
      return;
    }
    const frac = total > 0 ? Phaser.Math.Clamp(remaining / total, 0, 1) : 1;
    if (Math.abs(frac - btn.lastFrac) > 0.01) {
      btn.lastFrac = frac;
      btn.sweep.clear();
      btn.sweep.fillStyle(0x000000, 0.62);
      const start = -Math.PI / 2;
      btn.sweep.slice(0, 0, btn.radius - 2, start + (1 - frac) * Math.PI * 2, start + Math.PI * 2, false);
      btn.sweep.fillPath();
    }
    const secs = remaining >= 1000 ? `${Math.ceil(remaining / 1000)}` : (remaining / 1000).toFixed(1);
    if (!btn.cdText.visible) btn.cdText.setVisible(true);
    if (btn.cdText.text !== secs) btn.cdText.setText(secs);
  }

  // ── Lifecycle ─────────────────────────────────────────────────────────

  refreshSkills(): void {
    this.refreshSkillLoadout();
    this.relayout(true);
  }

  refreshLocale(): void {
    this.relayout(true);
  }

  setVisible(v: boolean): void {
    this.visible = v;
    this.root.setVisible(v);
    if (!v) this.releaseJoystick();
  }

  destroy(): void {
    this.scene.input.off('pointermove', this.pointerMoveHandler);
    this.scene.input.off('pointerup', this.pointerUpHandler);
    this.scene.scale.off(Phaser.Scale.Events.RESIZE, this.resizeHandler);
    this.clear();
    this.root.destroy();
  }
}
