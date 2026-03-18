import Phaser from 'phaser';

const EFFECT_DEPTH = 1500;

export class SkillEffectSystem {
  private scene: Phaser.Scene;

  constructor(scene: Phaser.Scene) {
    this.scene = scene;
  }

  // ── Generate particle textures (call from BootScene) ─────
  static generateTextures(scene: Phaser.Scene): void {
    const g = scene.add.graphics();

    // particle_circle (8x8 soft circle)
    g.clear();
    g.fillStyle(0xffffff, 0.3);
    g.fillCircle(4, 4, 4);
    g.fillStyle(0xffffff, 0.6);
    g.fillCircle(4, 4, 3);
    g.fillStyle(0xffffff, 1.0);
    g.fillCircle(4, 4, 1.5);
    g.generateTexture('particle_circle', 8, 8);

    // particle_spark (6x6 diamond/star)
    g.clear();
    g.fillStyle(0xffffff, 0.9);
    g.fillPoints([
      new Phaser.Geom.Point(3, 0),
      new Phaser.Geom.Point(4.5, 3),
      new Phaser.Geom.Point(6, 3),
      new Phaser.Geom.Point(3, 6),
      new Phaser.Geom.Point(0, 3),
      new Phaser.Geom.Point(1.5, 3),
    ], true);
    g.generateTexture('particle_spark', 6, 6);

    // particle_flame (8x12 tear/flame shape)
    g.clear();
    g.fillStyle(0xff6600, 0.8);
    g.fillTriangle(4, 0, 0, 10, 8, 10);
    g.fillStyle(0xffaa00, 0.6);
    g.fillTriangle(4, 2, 1.5, 9, 6.5, 9);
    g.fillStyle(0xffdd44, 0.5);
    g.fillTriangle(4, 4, 2.5, 8, 5.5, 8);
    g.fillCircle(4, 10, 3);
    g.generateTexture('particle_flame', 8, 12);

    // particle_ice (8x8 crystal/diamond)
    g.clear();
    g.fillStyle(0x88ccff, 0.7);
    g.fillPoints([
      new Phaser.Geom.Point(4, 0),
      new Phaser.Geom.Point(8, 4),
      new Phaser.Geom.Point(4, 8),
      new Phaser.Geom.Point(0, 4),
    ], true);
    g.fillStyle(0xaaddff, 0.5);
    g.fillPoints([
      new Phaser.Geom.Point(4, 1),
      new Phaser.Geom.Point(6, 4),
      new Phaser.Geom.Point(4, 6),
      new Phaser.Geom.Point(2, 4),
    ], true);
    g.fillStyle(0xffffff, 0.3);
    g.fillCircle(3, 3, 1);
    g.generateTexture('particle_ice', 8, 8);

    // particle_arrow (4x12 thin arrow)
    g.clear();
    g.fillStyle(0xcccccc, 0.9);
    g.fillRect(1, 3, 2, 9);
    g.fillStyle(0xeeeeee, 0.9);
    g.fillTriangle(2, 0, 0, 3, 4, 3);
    g.fillStyle(0x886644, 0.7);
    g.fillRect(0, 10, 4, 2);
    g.generateTexture('particle_arrow', 4, 12);

    // particle_slash (16x4 thin arc/line)
    g.clear();
    g.fillStyle(0xffffff, 0.9);
    g.fillRoundedRect(0, 1, 16, 2, 1);
    g.fillStyle(0xffffff, 0.5);
    g.fillRoundedRect(1, 0, 14, 4, 2);
    g.generateTexture('particle_slash', 16, 4);

    // particle_lightning (2x16 jagged line)
    g.clear();
    g.lineStyle(2, 0xaaddff, 0.9);
    g.beginPath();
    g.moveTo(1, 0);
    g.lineTo(0, 4);
    g.lineTo(2, 6);
    g.lineTo(0, 10);
    g.lineTo(2, 12);
    g.lineTo(1, 16);
    g.strokePath();
    g.generateTexture('particle_lightning', 2, 16);

    // particle_smoke (12x12 soft fuzzy circle)
    g.clear();
    g.fillStyle(0x888888, 0.15);
    g.fillCircle(6, 6, 6);
    g.fillStyle(0x999999, 0.25);
    g.fillCircle(6, 6, 4.5);
    g.fillStyle(0xaaaaaa, 0.3);
    g.fillCircle(6, 6, 3);
    g.fillStyle(0xbbbbbb, 0.2);
    g.fillCircle(5, 5, 2);
    g.generateTexture('particle_smoke', 12, 12);

    // particle_poison (6x6 green droplet)
    g.clear();
    g.fillStyle(0x33cc33, 0.8);
    g.fillTriangle(3, 0, 0, 4, 6, 4);
    g.fillCircle(3, 4, 2.5);
    g.fillStyle(0x66ff66, 0.4);
    g.fillCircle(2.5, 3.5, 1);
    g.generateTexture('particle_poison', 6, 6);

    // particle_star (10x10 4-pointed star)
    g.clear();
    g.fillStyle(0xffffff, 0.9);
    g.fillPoints([
      new Phaser.Geom.Point(5, 0),
      new Phaser.Geom.Point(6.2, 3.8),
      new Phaser.Geom.Point(10, 5),
      new Phaser.Geom.Point(6.2, 6.2),
      new Phaser.Geom.Point(5, 10),
      new Phaser.Geom.Point(3.8, 6.2),
      new Phaser.Geom.Point(0, 5),
      new Phaser.Geom.Point(3.8, 3.8),
    ], true);
    g.fillStyle(0xffffff, 0.5);
    g.fillCircle(5, 5, 2);
    g.generateTexture('particle_star', 10, 10);

    g.destroy();
  }

  // ── Play a skill effect ──────────────────────────────────
  play(
    skillId: string,
    casterX: number,
    casterY: number,
    targetX?: number,
    targetY?: number,
    targets?: { x: number; y: number }[],
  ): void {
    switch (skillId) {
      // Warrior
      case 'slash': this.effectSlash(casterX, casterY); break;
      case 'whirlwind': this.effectWhirlwind(casterX, casterY); break;
      case 'shield_wall': this.effectShieldWall(casterX, casterY); break;
      // Mage
      case 'fireball': this.effectFireball(casterX, casterY, targetX ?? casterX, targetY ?? casterY); break;
      case 'blizzard': this.effectBlizzard(targetX ?? casterX, targetY ?? casterY); break;
      case 'mana_shield': this.effectManaShield(casterX, casterY); break;
      case 'meteor': this.effectMeteor(targetX ?? casterX, targetY ?? casterY); break;
      case 'ice_armor': this.effectIceArmor(casterX, casterY); break;
      case 'chain_lightning': this.effectChainLightning(casterX, casterY, targets ?? []); break;
      // Rogue
      case 'backstab': this.effectBackstab(targetX ?? casterX, targetY ?? casterY); break;
      case 'poison_blade': this.effectPoisonBlade(casterX, casterY); break;
      case 'multishot': this.effectMultishot(casterX, casterY); break;
      case 'vanish': this.effectVanish(casterX, casterY); break;
      case 'explosive_trap': this.effectExplosiveTrap(targetX ?? casterX, targetY ?? casterY); break;
      case 'arrow_rain': this.effectArrowRain(targetX ?? casterX, targetY ?? casterY); break;
      default:
        this.effectGeneric(targetX ?? casterX, targetY ?? casterY, 0xf39c12);
        break;
    }
  }

  // ── Play basic attack effect ─────────────────────────────
  playAttack(attackerX: number, attackerY: number, targetX: number, targetY: number, isPlayer: boolean): void {
    const x = targetX;
    const y = targetY - 16;
    const angle = Math.atan2(targetY - attackerY, targetX - attackerX);
    const scale = isPlayer ? 1 : 0.7;

    // Slash line
    const slash = this.scene.add.graphics().setDepth(EFFECT_DEPTH);
    slash.lineStyle(3 * scale, 0xffffff, 0.9);
    slash.beginPath();
    const len = 20 * scale;
    slash.moveTo(x - Math.cos(angle) * len, y - Math.sin(angle) * len);
    slash.lineTo(x + Math.cos(angle) * len, y + Math.sin(angle) * len);
    slash.strokePath();

    this.scene.tweens.add({
      targets: slash,
      alpha: 0,
      duration: 200,
      ease: 'Power2',
      onComplete: () => slash.destroy(),
    });

    // Small spark burst
    for (let i = 0; i < 4; i++) {
      const spark = this.scene.add.image(x, y, 'particle_spark')
        .setDepth(EFFECT_DEPTH).setTint(0xffffaa).setScale(0.8 * scale).setAlpha(0.9);
      const a = Math.random() * Math.PI * 2;
      const dist = 8 + Math.random() * 12;
      this.scene.tweens.add({
        targets: spark,
        x: x + Math.cos(a) * dist,
        y: y + Math.sin(a) * dist,
        alpha: 0,
        scale: 0.2,
        duration: 200,
        ease: 'Power2',
        onComplete: () => spark.destroy(),
      });
    }
  }

  // ── Play monster attack effect ───────────────────────────
  playMonsterAttack(x: number, y: number): void {
    const cy = y - 16;

    // 3 parallel claw marks
    const claw = this.scene.add.graphics().setDepth(EFFECT_DEPTH);
    for (let i = -1; i <= 1; i++) {
      const ox = i * 6;
      claw.lineStyle(2, 0xe74c3c, 0.9);
      claw.beginPath();
      claw.moveTo(x + ox - 4, cy - 10);
      claw.lineTo(x + ox + 4, cy + 10);
      claw.strokePath();
    }

    this.scene.tweens.add({
      targets: claw,
      alpha: 0,
      duration: 250,
      ease: 'Power2',
      onComplete: () => claw.destroy(),
    });

    // Small red particles
    for (let i = 0; i < 5; i++) {
      const p = this.scene.add.image(x, cy, 'particle_circle')
        .setDepth(EFFECT_DEPTH).setTint(0xff4444).setScale(0.6).setAlpha(0.8);
      const a = Math.random() * Math.PI * 2;
      const dist = 6 + Math.random() * 10;
      this.scene.tweens.add({
        targets: p,
        x: x + Math.cos(a) * dist,
        y: cy + Math.sin(a) * dist,
        alpha: 0,
        scale: 0.1,
        duration: 250,
        ease: 'Power2',
        onComplete: () => p.destroy(),
      });
    }
  }

  playMonsterRangedAttack(sx: number, sy: number, tx: number, ty: number, color: number = 0xff6600): void {
    const startX = sx, startY = sy - 16;
    const endX = tx, endY = ty - 16;
    const dist = Phaser.Math.Distance.Between(startX, startY, endX, endY);
    const duration = Math.max(150, Math.min(400, dist * 2));

    // Projectile
    const proj = this.scene.add.circle(startX, startY, 5, color, 0.9).setDepth(EFFECT_DEPTH);
    const glow = this.scene.add.circle(startX, startY, 9, color, 0.3).setDepth(EFFECT_DEPTH);

    // Trail particles during flight
    const trailTimer = this.scene.time.addEvent({
      delay: 30,
      repeat: Math.floor(duration / 30),
      callback: () => {
        const p = this.scene.add.circle(proj.x, proj.y, 3, color, 0.6).setDepth(EFFECT_DEPTH);
        this.scene.tweens.add({
          targets: p, alpha: 0, scale: 0.1, duration: 200,
          onComplete: () => p.destroy(),
        });
      },
    });

    // Flight
    this.scene.tweens.add({
      targets: [proj, glow],
      x: endX, y: endY,
      duration,
      ease: 'Power1',
      onComplete: () => {
        proj.destroy();
        glow.destroy();
        trailTimer.destroy();
        // Impact burst
        for (let i = 0; i < 6; i++) {
          const a = Math.random() * Math.PI * 2;
          const d = 5 + Math.random() * 10;
          const p = this.scene.add.circle(endX, endY, 3, color, 0.8).setDepth(EFFECT_DEPTH);
          this.scene.tweens.add({
            targets: p,
            x: endX + Math.cos(a) * d, y: endY + Math.sin(a) * d,
            alpha: 0, scale: 0.1, duration: 250,
            onComplete: () => p.destroy(),
          });
        }
      },
    });
  }

  // ══════════════════════════════════════════════════════════
  // WARRIOR EFFECTS
  // ══════════════════════════════════════════════════════════

  private effectSlash(cx: number, cy: number): void {
    const y = cy - 16;

    // 3 arc segments expanding and fading
    for (let i = 0; i < 3; i++) {
      const arc = this.scene.add.graphics().setDepth(EFFECT_DEPTH);
      const startAngle = -0.8 + i * 0.5;
      const endAngle = startAngle + 0.6;
      const radius = 16 + i * 6;
      arc.lineStyle(3 - i * 0.5, Phaser.Display.Color.Interpolate.ColorWithColor(
        Phaser.Display.Color.ValueToColor(0xffffff),
        Phaser.Display.Color.ValueToColor(0xf1c40f),
        3, i
      ).color as unknown as number, 0.9);

      // Workaround: draw arc manually with line segments
      const steps = 12;
      arc.beginPath();
      for (let s = 0; s <= steps; s++) {
        const a = startAngle + (endAngle - startAngle) * (s / steps);
        const px = cx + Math.cos(a) * radius;
        const py = y + Math.sin(a) * radius;
        if (s === 0) arc.moveTo(px, py);
        else arc.lineTo(px, py);
      }
      arc.strokePath();

      this.scene.tweens.add({
        targets: arc,
        scaleX: 2,
        scaleY: 2,
        alpha: 0,
        duration: 400,
        delay: i * 40,
        ease: 'Power2',
        onComplete: () => arc.destroy(),
      });
    }

    // Spark particles along the arc
    for (let i = 0; i < 8; i++) {
      const a = -0.8 + Math.random() * 2.0;
      const r = 20 + Math.random() * 20;
      const px = cx + Math.cos(a) * r;
      const py = y + Math.sin(a) * r;
      const spark = this.scene.add.image(cx, y, 'particle_spark')
        .setDepth(EFFECT_DEPTH).setTint(0xf1c40f).setScale(0.8).setAlpha(0.9);
      this.scene.tweens.add({
        targets: spark,
        x: px,
        y: py,
        alpha: 0,
        scale: 0.2,
        duration: 400,
        ease: 'Power2',
        onComplete: () => spark.destroy(),
      });
    }
  }

  private effectWhirlwind(cx: number, cy: number): void {
    const y = cy - 16;

    // Multiple concentric arcs rotating
    for (let ring = 0; ring < 3; ring++) {
      const arc = this.scene.add.graphics().setDepth(EFFECT_DEPTH);
      const radius = 18 + ring * 14;
      const color = Phaser.Display.Color.Interpolate.ColorWithColor(
        Phaser.Display.Color.ValueToColor(0xffffff),
        Phaser.Display.Color.ValueToColor(0xaaaaaa),
        3, ring
      );
      arc.lineStyle(2.5, (color.r << 16) | (color.g << 8) | color.b, 0.7);

      const steps = 16;
      const arcLen = Math.PI * 1.2;
      arc.beginPath();
      for (let s = 0; s <= steps; s++) {
        const a = (s / steps) * arcLen;
        const px = Math.cos(a) * radius;
        const py = Math.sin(a) * radius * 0.5;
        if (s === 0) arc.moveTo(px, py);
        else arc.lineTo(px, py);
      }
      arc.strokePath();
      arc.setPosition(cx, y);

      // Animate rotation using angle
      this.scene.tweens.add({
        targets: arc,
        angle: 360 + ring * 120,
        scaleX: 1.5,
        scaleY: 1.5,
        alpha: 0,
        duration: 600,
        ease: 'Power1',
        onComplete: () => arc.destroy(),
      });
    }

    // Wind/dust particles spiraling outward
    for (let i = 0; i < 12; i++) {
      const angle = (i / 12) * Math.PI * 2;
      const particle = this.scene.add.image(cx, y, 'particle_smoke')
        .setDepth(EFFECT_DEPTH).setTint(0xccbb99).setScale(0.5).setAlpha(0.6);
      const dist = 40 + Math.random() * 30;
      this.scene.tweens.add({
        targets: particle,
        x: cx + Math.cos(angle + 1) * dist,
        y: y + Math.sin(angle + 1) * dist * 0.5,
        alpha: 0,
        scale: 0.8,
        angle: 180,
        duration: 600,
        delay: i * 30,
        ease: 'Power1',
        onComplete: () => particle.destroy(),
      });
    }
  }

  private effectShieldWall(cx: number, cy: number): void {
    const y = cy - 16;

    // Hexagonal shield segments materialize
    for (let i = 0; i < 6; i++) {
      const angle = (i / 6) * Math.PI * 2 - Math.PI / 2;
      const radius = 28;
      const hx = cx + Math.cos(angle) * radius;
      const hy = y + Math.sin(angle) * radius * 0.6;

      const hex = this.scene.add.graphics().setDepth(EFFECT_DEPTH);
      hex.fillStyle(0xf1c40f, 0.3);
      hex.lineStyle(2, 0xf1c40f, 0.8);
      // Small hex shape
      const hexPts: Phaser.Geom.Point[] = [];
      for (let h = 0; h < 6; h++) {
        const ha = (h / 6) * Math.PI * 2;
        hexPts.push(new Phaser.Geom.Point(hx + Math.cos(ha) * 10, hy + Math.sin(ha) * 10));
      }
      hex.fillPoints(hexPts, true);
      hex.strokePoints(hexPts, true);

      hex.setAlpha(0).setScale(0.3);
      this.scene.tweens.add({
        targets: hex,
        alpha: 1,
        scaleX: 1,
        scaleY: 1,
        duration: 300,
        delay: i * 50,
        ease: 'Back.easeOut',
        onComplete: () => {
          // Lingering glow
          this.scene.tweens.add({
            targets: hex,
            alpha: 0,
            duration: 2000,
            ease: 'Power1',
            onComplete: () => hex.destroy(),
          });
        },
      });
    }

    // Golden pulse ring
    const ring = this.scene.add.circle(cx, y, 10, 0xf1c40f, 0.4).setDepth(EFFECT_DEPTH);
    this.scene.tweens.add({
      targets: ring,
      scaleX: 5,
      scaleY: 3,
      alpha: 0,
      duration: 500,
      ease: 'Power2',
      onComplete: () => ring.destroy(),
    });
  }

  // ══════════════════════════════════════════════════════════
  // MAGE EFFECTS
  // ══════════════════════════════════════════════════════════

  private effectFireball(cx: number, cy: number, tx: number, ty: number): void {
    const sx = cx;
    const sy = cy - 16;
    const ex = tx;
    const ey = ty - 16;
    const dist = Phaser.Math.Distance.Between(sx, sy, ex, ey);
    const flightDuration = Math.max(300, Math.min(500, dist * 1.5));

    // Projectile: orange circle
    const fireball = this.scene.add.circle(sx, sy, 6, 0xff6600, 0.9).setDepth(EFFECT_DEPTH);
    const glow = this.scene.add.circle(sx, sy, 10, 0xff4400, 0.3).setDepth(EFFECT_DEPTH);

    // Flame trail during flight
    const trailTimer = this.scene.time.addEvent({
      delay: 30,
      repeat: Math.floor(flightDuration / 30),
      callback: () => {
        const flame = this.scene.add.image(fireball.x, fireball.y, 'particle_flame')
          .setDepth(EFFECT_DEPTH).setTint(0xff6600).setScale(0.6).setAlpha(0.7)
          .setAngle(Math.random() * 360);
        this.scene.tweens.add({
          targets: flame,
          alpha: 0,
          scale: 0.1,
          duration: 200,
          onComplete: () => flame.destroy(),
        });
      },
    });

    // Flight tween
    this.scene.tweens.add({
      targets: [fireball, glow],
      x: ex,
      y: ey,
      duration: flightDuration,
      ease: 'Power1',
      onComplete: () => {
        fireball.destroy();
        glow.destroy();
        trailTimer.destroy();
        this.fireballExplosion(ex, ey);
      },
    });
  }

  private fireballExplosion(x: number, y: number): void {
    // Expanding ring
    const ring = this.scene.add.circle(x, y, 8, 0xff4400, 0.6).setDepth(EFFECT_DEPTH);
    this.scene.tweens.add({
      targets: ring,
      scaleX: 5,
      scaleY: 5,
      alpha: 0,
      duration: 300,
      ease: 'Power2',
      onComplete: () => ring.destroy(),
    });

    // Inner flash
    const flash = this.scene.add.circle(x, y, 12, 0xffaa00, 0.8).setDepth(EFFECT_DEPTH);
    this.scene.tweens.add({
      targets: flash,
      scaleX: 3,
      scaleY: 3,
      alpha: 0,
      duration: 200,
      ease: 'Power3',
      onComplete: () => flash.destroy(),
    });

    // Scattered fire particles
    for (let i = 0; i < 10; i++) {
      const a = Math.random() * Math.PI * 2;
      const dist = 15 + Math.random() * 25;
      const flame = this.scene.add.image(x, y, 'particle_flame')
        .setDepth(EFFECT_DEPTH).setTint(0xff6600).setScale(0.5 + Math.random() * 0.5).setAlpha(0.9)
        .setAngle(Math.random() * 360);
      this.scene.tweens.add({
        targets: flame,
        x: x + Math.cos(a) * dist,
        y: y + Math.sin(a) * dist,
        alpha: 0,
        scale: 0.1,
        duration: 300,
        ease: 'Power2',
        onComplete: () => flame.destroy(),
      });
    }
  }

  private effectBlizzard(cx: number, cy: number): void {
    const y = cy - 16;

    // Ground frost circle expanding
    const frost = this.scene.add.circle(cx, y, 6, 0x88ccff, 0.3).setDepth(EFFECT_DEPTH);
    this.scene.tweens.add({
      targets: frost,
      scaleX: 6,
      scaleY: 4,
      alpha: 0,
      duration: 800,
      ease: 'Power1',
      onComplete: () => frost.destroy(),
    });

    // Ice crystal particles falling from above
    for (let i = 0; i < 16; i++) {
      const startX = cx + (Math.random() - 0.5) * 80;
      const startY = y - 60 - Math.random() * 30;
      const endX = startX + (Math.random() - 0.5) * 20;
      const endY = y + (Math.random() - 0.5) * 20;
      const ice = this.scene.add.image(startX, startY, 'particle_ice')
        .setDepth(EFFECT_DEPTH).setScale(0.5 + Math.random() * 0.5).setAlpha(0.8)
        .setAngle(Math.random() * 360);
      this.scene.tweens.add({
        targets: ice,
        x: endX,
        y: endY,
        angle: ice.angle + 180,
        alpha: 0,
        duration: 600 + Math.random() * 400,
        delay: Math.random() * 300,
        ease: 'Power1',
        onComplete: () => ice.destroy(),
      });
    }
  }

  private effectManaShield(cx: number, cy: number): void {
    const y = cy - 16;

    // Blue translucent sphere
    const sphere = this.scene.add.circle(cx, y, 4, 0x3498db, 0.2).setDepth(EFFECT_DEPTH);
    this.scene.tweens.add({
      targets: sphere,
      scaleX: 6,
      scaleY: 5,
      alpha: 0.35,
      duration: 400,
      ease: 'Back.easeOut',
      onComplete: () => {
        // Lingering
        this.scene.tweens.add({
          targets: sphere,
          alpha: 0,
          duration: 2000,
          ease: 'Power1',
          onComplete: () => sphere.destroy(),
        });
      },
    });

    // Sphere outline
    const outline = this.scene.add.graphics().setDepth(EFFECT_DEPTH);
    outline.lineStyle(1.5, 0x5dade2, 0.6);
    outline.strokeCircle(cx, y, 4);
    this.scene.tweens.add({
      targets: outline,
      scaleX: 6,
      scaleY: 5,
      alpha: 0.4,
      duration: 400,
      ease: 'Back.easeOut',
      onComplete: () => {
        this.scene.tweens.add({
          targets: outline,
          alpha: 0,
          duration: 2000,
          ease: 'Power1',
          onComplete: () => outline.destroy(),
        });
      },
    });

    // Arcane rune particles orbiting
    for (let i = 0; i < 8; i++) {
      const a = (i / 8) * Math.PI * 2;
      const r = 24;
      const rune = this.scene.add.image(cx, y, 'particle_star')
        .setDepth(EFFECT_DEPTH).setTint(0x5dade2).setScale(0.5).setAlpha(0);
      this.scene.tweens.add({
        targets: rune,
        x: cx + Math.cos(a) * r,
        y: y + Math.sin(a) * r * 0.6,
        alpha: 0.8,
        angle: 360,
        duration: 400,
        delay: i * 40,
        ease: 'Power2',
        onComplete: () => {
          this.scene.tweens.add({
            targets: rune,
            alpha: 0,
            duration: 1000,
            onComplete: () => rune.destroy(),
          });
        },
      });
    }
  }

  private effectMeteor(tx: number, ty: number): void {
    const y = ty - 16;

    // Camera shake
    this.scene.cameras.main.shake(400, 0.008);

    // Large fireball drops from screen top
    const startY = y - 200;
    const meteor = this.scene.add.circle(tx - 40, startY, 14, 0xff4400, 0.9).setDepth(EFFECT_DEPTH);
    const meteorGlow = this.scene.add.circle(tx - 40, startY, 22, 0xff6600, 0.3).setDepth(EFFECT_DEPTH);

    // Flame trail during fall
    const fallTimer = this.scene.time.addEvent({
      delay: 25,
      repeat: 24,
      callback: () => {
        const flame = this.scene.add.image(meteor.x, meteor.y, 'particle_flame')
          .setDepth(EFFECT_DEPTH).setTint(0xff4400).setScale(0.8 + Math.random() * 0.5).setAlpha(0.7)
          .setAngle(Math.random() * 360);
        this.scene.tweens.add({
          targets: flame,
          alpha: 0,
          scale: 0.1,
          y: flame.y - 10,
          duration: 300,
          onComplete: () => flame.destroy(),
        });
      },
    });

    this.scene.tweens.add({
      targets: [meteor, meteorGlow],
      x: tx,
      y: y,
      duration: 600,
      ease: 'Power2.easeIn',
      onComplete: () => {
        meteor.destroy();
        meteorGlow.destroy();
        fallTimer.destroy();
        this.meteorExplosion(tx, y);
      },
    });
  }

  private meteorExplosion(x: number, y: number): void {
    // Stronger camera shake on impact
    this.scene.cameras.main.shake(300, 0.015);

    // Massive explosion ring
    const ring = this.scene.add.circle(x, y, 10, 0xff4400, 0.7).setDepth(EFFECT_DEPTH);
    this.scene.tweens.add({
      targets: ring,
      scaleX: 8,
      scaleY: 6,
      alpha: 0,
      duration: 500,
      ease: 'Power2',
      onComplete: () => ring.destroy(),
    });

    // Inner white flash
    const flash = this.scene.add.circle(x, y, 16, 0xffffff, 0.9).setDepth(EFFECT_DEPTH);
    this.scene.tweens.add({
      targets: flash,
      scaleX: 4,
      scaleY: 4,
      alpha: 0,
      duration: 250,
      ease: 'Power3',
      onComplete: () => flash.destroy(),
    });

    // Ground scorchmark (lingering)
    const scorch = this.scene.add.circle(x, y, 24, 0x333333, 0.3).setDepth(EFFECT_DEPTH - 1);
    scorch.setScale(1, 0.6);
    this.scene.tweens.add({
      targets: scorch,
      alpha: 0,
      duration: 3000,
      delay: 500,
      ease: 'Power1',
      onComplete: () => scorch.destroy(),
    });

    // Debris particles
    for (let i = 0; i < 14; i++) {
      const a = Math.random() * Math.PI * 2;
      const dist = 20 + Math.random() * 40;
      const tex = Math.random() > 0.5 ? 'particle_flame' : 'particle_spark';
      const debris = this.scene.add.image(x, y, tex)
        .setDepth(EFFECT_DEPTH).setTint(Math.random() > 0.5 ? 0xff4400 : 0xff8800)
        .setScale(0.5 + Math.random() * 0.6).setAlpha(0.9);
      this.scene.tweens.add({
        targets: debris,
        x: x + Math.cos(a) * dist,
        y: y + Math.sin(a) * dist - Math.random() * 20,
        alpha: 0,
        scale: 0.1,
        duration: 400 + Math.random() * 200,
        ease: 'Power2',
        onComplete: () => debris.destroy(),
      });
    }
  }

  private effectIceArmor(cx: number, cy: number): void {
    const y = cy - 16;

    // Ice crystals orbiting around caster
    for (let i = 0; i < 8; i++) {
      const startAngle = (i / 8) * Math.PI * 2;
      const crystal = this.scene.add.image(cx, y, 'particle_ice')
        .setDepth(EFFECT_DEPTH).setScale(0.6).setAlpha(0);

      // Orbit animation - position update via multiple stages
      const r = 22;
      const endAngle = startAngle + Math.PI * 2;
      const steps = 10;
      let step = 0;

      const moveNext = () => {
        step++;
        if (step > steps) {
          this.scene.tweens.add({
            targets: crystal,
            alpha: 0,
            duration: 300,
            onComplete: () => crystal.destroy(),
          });
          return;
        }
        const a = startAngle + (endAngle - startAngle) * (step / steps);
        this.scene.tweens.add({
          targets: crystal,
          x: cx + Math.cos(a) * r,
          y: y + Math.sin(a) * r * 0.5,
          alpha: step < steps * 0.7 ? 0.9 : 0.5,
          duration: 500 / steps,
          onComplete: moveNext,
        });
      };

      // Start with fade-in
      this.scene.tweens.add({
        targets: crystal,
        x: cx + Math.cos(startAngle) * r,
        y: y + Math.sin(startAngle) * r * 0.5,
        alpha: 0.9,
        duration: 100,
        delay: i * 30,
        onComplete: moveNext,
      });
    }

    // Frost particles
    for (let i = 0; i < 6; i++) {
      const a = Math.random() * Math.PI * 2;
      const frost = this.scene.add.image(cx, y, 'particle_circle')
        .setDepth(EFFECT_DEPTH).setTint(0x88ccff).setScale(0.4).setAlpha(0.6);
      this.scene.tweens.add({
        targets: frost,
        x: cx + Math.cos(a) * 18,
        y: y + Math.sin(a) * 12,
        alpha: 0,
        duration: 500,
        delay: Math.random() * 200,
        ease: 'Power1',
        onComplete: () => frost.destroy(),
      });
    }
  }

  private effectChainLightning(cx: number, cy: number, targets: { x: number; y: number }[]): void {
    const sy = cy - 16;

    if (targets.length === 0) {
      // Single target fallback: lightning at caster
      this.drawLightningBolt(cx, sy - 30, cx, sy, 0x5dade2);
      return;
    }

    // Draw jagged lightning from caster to each target
    let prevX = cx;
    let prevY = sy;
    for (const t of targets) {
      const tx = t.x;
      const ty = t.y - 16;
      this.drawLightningBolt(prevX, prevY, tx, ty, 0x5dade2);
      // Electric spark particles at endpoint
      for (let i = 0; i < 4; i++) {
        const spark = this.scene.add.image(tx, ty, 'particle_spark')
          .setDepth(EFFECT_DEPTH).setTint(0x5dade2).setScale(0.6).setAlpha(0.9);
        const a = Math.random() * Math.PI * 2;
        this.scene.tweens.add({
          targets: spark,
          x: tx + Math.cos(a) * 12,
          y: ty + Math.sin(a) * 12,
          alpha: 0,
          scale: 0.1,
          duration: 300,
          onComplete: () => spark.destroy(),
        });
      }
      prevX = tx;
      prevY = ty;
    }
  }

  private drawLightningBolt(x1: number, y1: number, x2: number, y2: number, color: number): void {
    const bolt = this.scene.add.graphics().setDepth(EFFECT_DEPTH);
    const dx = x2 - x1;
    const dy = y2 - y1;
    const segments = 8;

    // Main bolt
    bolt.lineStyle(2.5, color, 0.9);
    bolt.beginPath();
    bolt.moveTo(x1, y1);
    for (let s = 1; s < segments; s++) {
      const t = s / segments;
      const mx = x1 + dx * t + (Math.random() - 0.5) * 16;
      const my = y1 + dy * t + (Math.random() - 0.5) * 16;
      bolt.lineTo(mx, my);
    }
    bolt.lineTo(x2, y2);
    bolt.strokePath();

    // Glow bolt (thicker, transparent)
    bolt.lineStyle(5, color, 0.25);
    bolt.beginPath();
    bolt.moveTo(x1, y1);
    for (let s = 1; s < segments; s++) {
      const t = s / segments;
      const mx = x1 + dx * t + (Math.random() - 0.5) * 12;
      const my = y1 + dy * t + (Math.random() - 0.5) * 12;
      bolt.lineTo(mx, my);
    }
    bolt.lineTo(x2, y2);
    bolt.strokePath();

    this.scene.tweens.add({
      targets: bolt,
      alpha: 0,
      duration: 300,
      ease: 'Power2',
      onComplete: () => bolt.destroy(),
    });
  }

  // ══════════════════════════════════════════════════════════
  // ROGUE EFFECTS
  // ══════════════════════════════════════════════════════════

  private effectBackstab(tx: number, ty: number): void {
    const y = ty - 16;

    // Quick flash
    const flash = this.scene.add.circle(tx, y, 20, 0xffffff, 0.6).setDepth(EFFECT_DEPTH);
    this.scene.tweens.add({
      targets: flash,
      alpha: 0,
      scaleX: 1.5,
      scaleY: 1.5,
      duration: 120,
      ease: 'Power3',
      onComplete: () => flash.destroy(),
    });

    // Crossed blade slash marks
    const slashes = this.scene.add.graphics().setDepth(EFFECT_DEPTH);
    slashes.lineStyle(2.5, 0xffffff, 0.9);
    // X slash
    slashes.beginPath();
    slashes.moveTo(tx - 14, y - 14);
    slashes.lineTo(tx + 14, y + 14);
    slashes.strokePath();
    slashes.beginPath();
    slashes.moveTo(tx + 14, y - 14);
    slashes.lineTo(tx - 14, y + 14);
    slashes.strokePath();

    this.scene.tweens.add({
      targets: slashes,
      alpha: 0,
      scaleX: 1.3,
      scaleY: 1.3,
      duration: 250,
      ease: 'Power2',
      onComplete: () => slashes.destroy(),
    });

    // Afterimage effect (brief duplicate shadow)
    const afterimage = this.scene.add.circle(tx - 8, y, 12, 0x444444, 0.4).setDepth(EFFECT_DEPTH - 1);
    this.scene.tweens.add({
      targets: afterimage,
      alpha: 0,
      x: tx - 16,
      duration: 250,
      ease: 'Power2',
      onComplete: () => afterimage.destroy(),
    });
  }

  private effectPoisonBlade(cx: number, cy: number): void {
    const y = cy - 16;

    // Green mist particles rising
    for (let i = 0; i < 10; i++) {
      const px = cx + (Math.random() - 0.5) * 24;
      const py = y + (Math.random() - 0.5) * 10;
      const poison = this.scene.add.image(px, py, 'particle_poison')
        .setDepth(EFFECT_DEPTH).setScale(0.5 + Math.random() * 0.5).setAlpha(0.7)
        .setAngle(Math.random() * 360);
      this.scene.tweens.add({
        targets: poison,
        y: py - 20 - Math.random() * 20,
        x: px + (Math.random() - 0.5) * 16,
        alpha: 0,
        scale: 0.8,
        angle: poison.angle + 90,
        duration: 400 + Math.random() * 200,
        delay: Math.random() * 100,
        ease: 'Power1',
        onComplete: () => poison.destroy(),
      });
    }

    // Green glow around blade area
    const glow = this.scene.add.circle(cx, y, 8, 0x27ae60, 0.3).setDepth(EFFECT_DEPTH);
    this.scene.tweens.add({
      targets: glow,
      scaleX: 3,
      scaleY: 2,
      alpha: 0,
      duration: 400,
      ease: 'Power2',
      onComplete: () => glow.destroy(),
    });
  }

  private effectMultishot(cx: number, cy: number): void {
    const y = cy - 16;

    // Fan of arrow projectiles in 60° arc
    const arrowCount = 5;
    const spreadAngle = Math.PI / 3; // 60°
    const startAngle = -Math.PI / 2 - spreadAngle / 2;

    for (let i = 0; i < arrowCount; i++) {
      const a = startAngle + (i / (arrowCount - 1)) * spreadAngle;
      const arrow = this.scene.add.image(cx, y, 'particle_arrow')
        .setDepth(EFFECT_DEPTH).setTint(0xcccccc).setScale(0.8).setAlpha(0.9)
        .setAngle(Phaser.Math.RadToDeg(a) + 90);
      const dist = 60;
      this.scene.tweens.add({
        targets: arrow,
        x: cx + Math.cos(a) * dist,
        y: y + Math.sin(a) * dist,
        alpha: 0,
        duration: 400,
        ease: 'Power1',
        onComplete: () => arrow.destroy(),
      });
    }
  }

  private effectVanish(cx: number, cy: number): void {
    const y = cy - 16;

    // Smoke bomb burst - expanding gray particles
    for (let i = 0; i < 12; i++) {
      const a = (i / 12) * Math.PI * 2;
      const smoke = this.scene.add.image(cx, y, 'particle_smoke')
        .setDepth(EFFECT_DEPTH).setScale(0.4).setAlpha(0.7)
        .setTint(0x666666);
      const dist = 15 + Math.random() * 20;
      this.scene.tweens.add({
        targets: smoke,
        x: cx + Math.cos(a) * dist,
        y: y + Math.sin(a) * dist,
        alpha: 0,
        scale: 1.2,
        duration: 350,
        ease: 'Power1',
        onComplete: () => smoke.destroy(),
      });
    }

    // Central smoke puff
    const puff = this.scene.add.circle(cx, y, 12, 0x555555, 0.5).setDepth(EFFECT_DEPTH);
    this.scene.tweens.add({
      targets: puff,
      scaleX: 3,
      scaleY: 3,
      alpha: 0,
      duration: 350,
      ease: 'Power2',
      onComplete: () => puff.destroy(),
    });
  }

  private effectExplosiveTrap(tx: number, ty: number): void {
    const y = ty - 16;

    // Place glowing circle on ground
    const trap = this.scene.add.circle(tx, y, 10, 0xff4400, 0.4).setDepth(EFFECT_DEPTH);
    const trapRing = this.scene.add.graphics().setDepth(EFFECT_DEPTH);
    trapRing.lineStyle(1.5, 0xff6600, 0.7);
    trapRing.strokeCircle(tx, y, 10);

    // Pulsing glow
    this.scene.tweens.add({
      targets: trap,
      alpha: 0.7,
      scaleX: 1.2,
      scaleY: 1.2,
      yoyo: true,
      duration: 100,
      repeat: 1,
      onComplete: () => {
        trap.destroy();
        trapRing.destroy();
        // Explosion after delay
        this.trapExplosion(tx, y);
      },
    });
  }

  private trapExplosion(x: number, y: number): void {
    // Explosion ring
    const ring = this.scene.add.circle(x, y, 8, 0xff4400, 0.6).setDepth(EFFECT_DEPTH);
    this.scene.tweens.add({
      targets: ring,
      scaleX: 5,
      scaleY: 4,
      alpha: 0,
      duration: 400,
      ease: 'Power2',
      onComplete: () => ring.destroy(),
    });

    // Fire particles
    for (let i = 0; i < 10; i++) {
      const a = Math.random() * Math.PI * 2;
      const dist = 10 + Math.random() * 25;
      const flame = this.scene.add.image(x, y, 'particle_flame')
        .setDepth(EFFECT_DEPTH).setTint(0xff6600).setScale(0.5).setAlpha(0.8)
        .setAngle(Math.random() * 360);
      this.scene.tweens.add({
        targets: flame,
        x: x + Math.cos(a) * dist,
        y: y + Math.sin(a) * dist,
        alpha: 0,
        scale: 0.1,
        duration: 400 + Math.random() * 200,
        ease: 'Power2',
        onComplete: () => flame.destroy(),
      });
    }

    // Smoke aftermath
    for (let i = 0; i < 4; i++) {
      const smoke = this.scene.add.image(
        x + (Math.random() - 0.5) * 20,
        y + (Math.random() - 0.5) * 10,
        'particle_smoke',
      ).setDepth(EFFECT_DEPTH).setScale(0.5).setAlpha(0.4).setTint(0x444444);
      this.scene.tweens.add({
        targets: smoke,
        y: smoke.y - 20,
        alpha: 0,
        scale: 1,
        duration: 600,
        delay: 100 + Math.random() * 100,
        ease: 'Power1',
        onComplete: () => smoke.destroy(),
      });
    }
  }

  private effectArrowRain(tx: number, ty: number): void {
    const y = ty - 16;

    // Many arrow particles falling from above across a large area
    for (let i = 0; i < 14; i++) {
      const startX = tx + (Math.random() - 0.5) * 90;
      const startY = y - 70 - Math.random() * 30;
      const endX = startX + (Math.random() - 0.5) * 10;
      const endY = y + (Math.random() - 0.5) * 30;
      const arrow = this.scene.add.image(startX, startY, 'particle_arrow')
        .setDepth(EFFECT_DEPTH).setScale(0.7).setAlpha(0.8)
        .setAngle(170 + Math.random() * 20); // Pointing roughly downward
      this.scene.tweens.add({
        targets: arrow,
        x: endX,
        y: endY,
        alpha: 0.3,
        duration: 400 + Math.random() * 200,
        delay: Math.random() * 300,
        ease: 'Power2.easeIn',
        onComplete: () => {
          // Small impact spark
          const spark = this.scene.add.image(endX, endY, 'particle_spark')
            .setDepth(EFFECT_DEPTH).setTint(0xcccccc).setScale(0.4).setAlpha(0.6);
          this.scene.tweens.add({
            targets: spark,
            alpha: 0,
            scale: 0.1,
            duration: 200,
            onComplete: () => spark.destroy(),
          });
          arrow.destroy();
        },
      });
    }
  }

  // ══════════════════════════════════════════════════════════
  // GENERIC FALLBACK
  // ══════════════════════════════════════════════════════════

  private effectGeneric(x: number, y: number, color: number): void {
    const cy = y - 16;
    const c = this.scene.add.circle(x, cy, 8, color, 0.7).setDepth(EFFECT_DEPTH);
    this.scene.tweens.add({
      targets: c,
      scaleX: 5,
      scaleY: 5,
      alpha: 0,
      duration: 500,
      ease: 'Power2',
      onComplete: () => c.destroy(),
    });
  }
}
