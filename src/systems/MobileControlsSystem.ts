import Phaser from 'phaser';
import { GAME_WIDTH, GAME_HEIGHT } from '../config';
import { EventBus, GameEvents } from '../utils/EventBus';
import type { Player } from '../entities/Player';
import { t } from '../i18n';

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

interface JoystickState {
  active: boolean;
  pointerId: number;
  dx: number;
  dy: number;
}

export class MobileControlsSystem {
  private scene: Phaser.Scene;
  private player: Player;

  // Joystick elements
  private joystickBase!: Phaser.GameObjects.Arc;
  private joystickThumb!: Phaser.GameObjects.Arc;
  private joystickContainer!: Phaser.GameObjects.Container;
  private joystickState: JoystickState = { active: false, pointerId: -1, dx: 0, dy: 0 };
  private joystickRadius: number;
  private joystickCenterX = 0;
  private joystickCenterY = 0;

  // Skill buttons
  private skillButtons: Phaser.GameObjects.Container[] = [];

  // Panel buttons
  private panelButtons: Phaser.GameObjects.Container[] = [];

  // Auto-combat button
  private autoCombatBtn!: Phaser.GameObjects.Container;
  private autoCombatLabel!: Phaser.GameObjects.Text;

  // Responsive sizing
  private scale: number;
  private visible: boolean = true;
  private readonly pointerMoveHandler = (pointer: Phaser.Input.Pointer): void => {
    if (this.joystickState.active && pointer.id === this.joystickState.pointerId) {
      this.updateJoystickThumb(pointer.x, pointer.y, this.joystickCenterX, this.joystickCenterY);
    }
  };
  private readonly pointerUpHandler = (pointer: Phaser.Input.Pointer): void => {
    if (pointer.id === this.joystickState.pointerId) {
      this.joystickState.active = false;
      this.joystickState.pointerId = -1;
      this.joystickState.dx = 0;
      this.joystickState.dy = 0;
      this.joystickThumb.setPosition(this.joystickCenterX, this.joystickCenterY);
    }
  };

  constructor(scene: Phaser.Scene, player: Player) {
    this.scene = scene;
    this.player = player;
    this.scale = Math.min(GAME_WIDTH, GAME_HEIGHT) / 720;
    this.joystickRadius = 50 * this.scale;

    this.createJoystick();
    this.createSkillButtons();
    this.createAutoCombatButton();
    this.createPanelButtons();
  }

  /** Current movement direction from joystick (in tile-space dx/dy) */
  getDirection(): { dx: number; dy: number } {
    if (!this.joystickState.active) return { dx: 0, dy: 0 };
    // Convert screen-space joystick direction to isometric tile-space
    // Screen right = tile (+col, -row), Screen down = tile (+col, +row)
    // Iso: screenX = (col - row) * halfW, screenY = (col + row) * halfH
    // Inverse: col = screenX/(2*halfW) + screenY/(2*halfH), row = -screenX/(2*halfW) + screenY/(2*halfH)
    const jx = this.joystickState.dx;
    const jy = this.joystickState.dy;
    const dx = jx + jy;  // maps to col direction
    const dy = -jx + jy; // maps to row direction
    return { dx, dy };
  }

  private createJoystick(): void {
    const r = this.joystickRadius;
    const cx = 30 * this.scale + r;
    const cy = GAME_HEIGHT - 30 * this.scale - r;
    this.joystickCenterX = cx;
    this.joystickCenterY = cy;

    this.joystickContainer = this.scene.add.container(0, 0).setDepth(5000).setScrollFactor(0);

    // Base circle
    this.joystickBase = this.scene.add.circle(cx, cy, r, 0x222244, 0.35)
      .setStrokeStyle(2, 0x4444aa, 0.5);
    this.joystickContainer.add(this.joystickBase);

    // Thumb
    const thumbR = r * 0.45;
    this.joystickThumb = this.scene.add.circle(cx, cy, thumbR, 0x6666cc, 0.5)
      .setStrokeStyle(1.5, 0x8888ee, 0.6);
    this.joystickContainer.add(this.joystickThumb);

    // Touch zone (larger invisible area for easier grab)
    const touchZone = this.scene.add.circle(cx, cy, r * 1.4, 0x000000, 0)
      .setInteractive({ draggable: false });
    this.joystickContainer.add(touchZone);

    touchZone.on('pointerdown', (pointer: Phaser.Input.Pointer) => {
      this.joystickState.active = true;
      this.joystickState.pointerId = pointer.id;
      this.updateJoystickThumb(pointer.x, pointer.y, cx, cy);
    });

    this.scene.input.on('pointermove', this.pointerMoveHandler);
    this.scene.input.on('pointerup', this.pointerUpHandler);
  }

  private updateJoystickThumb(px: number, py: number, cx: number, cy: number): void {
    const r = this.joystickRadius;
    let dx = px - cx;
    let dy = py - cy;
    const dist = Math.sqrt(dx * dx + dy * dy);
    if (dist > r) {
      dx = (dx / dist) * r;
      dy = (dy / dist) * r;
    }
    this.joystickThumb.setPosition(cx + dx, cy + dy);
    // Normalize to -1..1
    this.joystickState.dx = dx / r;
    this.joystickState.dy = dy / r;
  }

  private createSkillButtons(): void {
    const btnSize = 44 * this.scale;
    const gap = 6 * this.scale;
    const skills = this.player.classData.skills;
    const count = Math.min(skills.length, 6);

    // Layout: 2 columns x 3 rows on right side
    const cols = 2;
    const rows = Math.ceil(count / cols);
    const startX = GAME_WIDTH - (cols * (btnSize + gap)) - 20 * this.scale;
    const startY = GAME_HEIGHT - (rows * (btnSize + gap)) - 20 * this.scale;

    for (let i = 0; i < count; i++) {
      const skill = skills[i];
      const col = i % cols;
      const row = Math.floor(i / cols);
      const x = startX + col * (btnSize + gap) + btnSize / 2;
      const y = startY + row * (btnSize + gap) + btnSize / 2;

      const container = this.scene.add.container(x, y).setDepth(5000).setScrollFactor(0);

      const bg = this.scene.add.rectangle(0, 0, btnSize, btnSize, 0x1a1a2e, 0.6)
        .setStrokeStyle(1.5, 0x555588, 0.7);
      container.add(bg);

      // Skill icon text (number + short name)
      const label = this.scene.add.text(0, -6, `${i + 1}`, {
        fontSize: `${Math.round(10 * this.scale)}px`,
        color: '#8888bb',
        fontFamily: FONT,
      }).setOrigin(0.5);
      container.add(label);

      const nameLabel = this.scene.add.text(0, 8, skill.name.slice(0, 2), {
        fontSize: `${Math.round(9 * this.scale)}px`,
        color: '#aaaacc',
        fontFamily: FONT,
      }).setOrigin(0.5);
      container.add(nameLabel);

      // Cooldown overlay
      const cdOverlay = this.scene.add.rectangle(0, 0, btnSize, btnSize, 0x000000, 0.6).setVisible(false);
      container.add(cdOverlay);

      bg.setInteractive({ useHandCursor: false });
      bg.on('pointerdown', () => {
        EventBus.emit(GameEvents.UI_SKILL_CLICK, { index: i, skillId: skill.id });
      });

      this.skillButtons.push(container);
    }
  }

  private createAutoCombatButton(): void {
    const btnSize = 44 * this.scale;
    const gap = 6 * this.scale;
    // Place above skill buttons, right-aligned
    const x = GAME_WIDTH - btnSize / 2 - 20 * this.scale - (btnSize + gap);
    const rows = Math.ceil(Math.min(this.player.classData.skills.length, 6) / 2);
    const skillBlockHeight = rows * (btnSize + gap);
    const y = GAME_HEIGHT - skillBlockHeight - 20 * this.scale - btnSize / 2 - gap;

    this.autoCombatBtn = this.scene.add.container(x, y).setDepth(5000).setScrollFactor(0);

    const bg = this.scene.add.rectangle(0, 0, btnSize * 2 + gap, btnSize, 0x1a1a2e, 0.6)
      .setStrokeStyle(1.5, 0x555588, 0.7);
    this.autoCombatBtn.add(bg);

    this.autoCombatLabel = this.scene.add.text(0, 0, t('sys.mobile.autoCombat.off'), {
      fontSize: `${Math.round(10 * this.scale)}px`,
      color: '#666680',
      fontFamily: FONT,
      align: 'center',
      lineSpacing: 2,
    }).setOrigin(0.5);
    this.autoCombatBtn.add(this.autoCombatLabel);

    bg.setInteractive({ useHandCursor: false });
    bg.on('pointerdown', () => {
      this.player.autoCombat = !this.player.autoCombat;
      EventBus.emit(GameEvents.LOG_MESSAGE, {
        text: t('sys.mobile.autoCombat.log', { state: this.player.autoCombat ? t('zone.combat.autoCombatOn') : t('zone.combat.autoCombatOff') }),
        type: 'system',
      });
    });
  }

  private createPanelButtons(): void {
    const btnSize = 36 * this.scale;
    const gap = 4 * this.scale;
    const panels: { label: string; panel: string }[] = [
      { label: t('sys.mobile.panel.inventory'), panel: 'inventory' },
      { label: t('sys.mobile.panel.character'), panel: 'character' },
      { label: t('sys.mobile.panel.skills'), panel: 'skills' },
      { label: t('sys.mobile.panel.map'), panel: 'map' },
      { label: t('sys.mobile.panel.homestead'), panel: 'homestead' },
      { label: t('sys.mobile.panel.quest'), panel: 'quest' },
    ];

    // Top-right horizontal row
    const startX = GAME_WIDTH - panels.length * (btnSize + gap) - 10 * this.scale;
    const y = 10 * this.scale + btnSize / 2;

    for (let i = 0; i < panels.length; i++) {
      const p = panels[i];
      const x = startX + i * (btnSize + gap) + btnSize / 2;
      const container = this.scene.add.container(x, y).setDepth(5000).setScrollFactor(0);

      const bg = this.scene.add.rectangle(0, 0, btnSize, btnSize, 0x1a1a2e, 0.5)
        .setStrokeStyle(1, 0x444466, 0.6);
      container.add(bg);

      const label = this.scene.add.text(0, 0, p.label, {
        fontSize: `${Math.round(9 * this.scale)}px`,
        color: '#9999bb',
        fontFamily: FONT,
      }).setOrigin(0.5);
      container.add(label);

      bg.setInteractive({ useHandCursor: false });
      bg.on('pointerdown', () => {
        EventBus.emit(GameEvents.UI_TOGGLE_PANEL, { panel: p.panel });
      });

      this.panelButtons.push(container);
    }
  }

  update(_time: number, _delta: number): void {
    // Update auto-combat label
    if (this.autoCombatLabel) {
      const on = this.player.autoCombat;
      this.autoCombatLabel.setText(on ? t('sys.mobile.autoCombat.on') : t('sys.mobile.autoCombat.off'));
      this.autoCombatLabel.setColor(on ? '#27ae60' : '#666680');
    }

    // Update skill cooldown overlays
    const now = this.scene.time.now;
    const skills = this.player.classData.skills;
    for (let i = 0; i < this.skillButtons.length; i++) {
      if (i >= skills.length) break;
      const cd = this.player.skillCooldowns.get(skills[i].id) ?? 0;
      const overlay = this.skillButtons[i].getAt(3) as Phaser.GameObjects.Rectangle;
      overlay.setVisible(now < cd);
    }
  }

  setVisible(v: boolean): void {
    this.visible = v;
    this.joystickContainer.setVisible(v);
    this.autoCombatBtn.setVisible(v);
    for (const btn of this.skillButtons) btn.setVisible(v);
    for (const btn of this.panelButtons) btn.setVisible(v);
  }

  destroy(): void {
    this.scene.input.off('pointermove', this.pointerMoveHandler);
    this.scene.input.off('pointerup', this.pointerUpHandler);
    this.joystickContainer.destroy();
    this.autoCombatBtn.destroy();
    for (const btn of this.skillButtons) btn.destroy();
    for (const btn of this.panelButtons) btn.destroy();
  }
}
