import Phaser from 'phaser';
import { TEXTURE_SCALE } from '../config';
import { SkillEffectSystem } from '../systems/SkillEffectSystem';

export class BootScene extends Phaser.Scene {
  constructor() {
    super({ key: 'BootScene' });
  }

  preload(): void {
    const { width, height } = this.cameras.main;
    const barW = 400, barH = 12;
    const barY = height / 2;
    this.add.rectangle(width / 2, barY, barW, barH, 0x1a1a2e).setStrokeStyle(1, 0x333344);
    const fill = this.add.rectangle((width - barW) / 2 + 2, barY, 0, barH - 4, 0xc0934a).setOrigin(0, 0.5);
    const loadingText = this.add.text(width / 2, barY - 24, '锻造渊火...', {
      fontSize: '14px', color: '#c0934a', fontFamily: '"Cinzel", serif',
    }).setOrigin(0.5);
    this.load.on('progress', (v: number) => { fill.width = (barW - 4) * v; });
    this.load.on('complete', () => { loadingText.setText('准备就绪!'); });

    // Silently handle missing asset files — procedural fallbacks will fill gaps
    this.load.on('loaderror', (file: Phaser.Loader.File) => {
      console.debug(`[BootScene] Asset not found, will use fallback: ${file.key}`);
    });

    // ── External assets (cartoon-style) ─────────────────────
    // Tiles
    const tiles = ['grass', 'dirt', 'stone', 'water', 'wall', 'camp'];
    for (const t of tiles) {
      this.load.image(`tile_${t}`, `assets/tiles/tile_${t}.png`);
    }

    // Player sprites
    const classes = ['warrior', 'mage', 'rogue'];
    for (const c of classes) {
      this.load.image(`player_${c}`, `assets/sprites/players/player_${c}.png`);
    }

    // Monster sprites
    const monsters = [
      'slime', 'goblin', 'goblin_chief', 'skeleton', 'zombie', 'werewolf',
      'werewolf_alpha', 'gargoyle', 'stone_golem', 'mountain_troll',
      'fire_elemental', 'desert_scorpion', 'sandworm', 'phoenix',
      'imp', 'lesser_demon', 'succubus', 'demon_lord',
    ];
    for (const m of monsters) {
      this.load.image(`monster_${m}`, `assets/sprites/monsters/monster_${m}.png`);
    }

    // NPC sprites
    const npcTypes = ['blacksmith', 'merchant', 'quest', 'stash'];
    for (const n of npcTypes) {
      this.load.image(`npc_${n}`, `assets/sprites/npcs/npc_${n}.png`);
    }

    // Decorations
    const decors = ['tree', 'bush', 'rock', 'flower', 'mushroom', 'cactus', 'boulder', 'crystal', 'bones'];
    for (const d of decors) {
      this.load.image(`decor_${d}`, `assets/sprites/decorations/decor_${d}.png`);
    }

    // Effects
    this.load.image('loot_bag', 'assets/sprites/effects/loot_bag.png');
    this.load.image('exit_portal', 'assets/sprites/effects/exit_portal.png');
  }

  create(): void {
    this.generateTiles();
    this.generatePlayerSprites();
    this.generateMonsterSprites();
    this.generateEffects();
    this.generateDecorations();
    SkillEffectSystem.generateTextures(this);
    this.scene.start('MenuScene');
  }

  // ── HD Isometric Tiles (64x32 base, scaled by TEXTURE_SCALE) ───────────────────────────
  private generateTiles(): void {
    this.makeIsoTile('tile_grass', (g, w, h, s) => {
      // Base gradient
      this.fillDiamond(g, w, h, 0x4a8c3f);
      const cx = w / 2, cy = h / 2;
      // Organic color patches
      const patches = [
        { x: cx - 25*s, y: cy - 8*s, r: 18*s, c: 0x5ca04e },
        { x: cx + 15*s, y: cy + 5*s, r: 14*s, c: 0x3d7a34 },
        { x: cx - 10*s, y: cy + 10*s, r: 12*s, c: 0x68b85e },
        { x: cx + 28*s, y: cy - 4*s, r: 10*s, c: 0x448839 },
      ];
      for (const p of patches) {
        g.fillStyle(p.c, 0.5);
        g.fillEllipse(p.x, p.y, p.r, p.r * 0.6);
      }
      // Grass blades - thin strokes
      for (let i = 0; i < 30; i++) {
        const bx = cx + (Math.random() - 0.5) * w * 0.65;
        const by = cy + (Math.random() - 0.5) * h * 0.5;
        if (!this.isInsideDiamond(bx, by, w, h)) continue;
        const shade = [0x5ca04e, 0x3d7a34, 0x68b85e, 0x4a9441, 0x357a2b][Math.floor(Math.random() * 5)];
        g.fillStyle(shade, 0.8);
        const bh = (3 + Math.random() * 5) * s;
        g.fillRect(bx, by - bh, 1.5*s, bh);
        g.fillRect(bx + 1*s, by - bh + 1*s, 1*s, bh - 1*s);
      }
      // Small flowers scattered
      for (let i = 0; i < 4; i++) {
        const fx = cx + (Math.random() - 0.5) * w * 0.5;
        const fy = cy + (Math.random() - 0.5) * h * 0.35;
        if (!this.isInsideDiamond(fx, fy, w, h)) continue;
        const fc = [0xf1c40f, 0xe8daf0, 0xf5b7b1, 0xa9dfbf][Math.floor(Math.random() * 4)];
        g.fillStyle(fc, 0.9);
        g.fillCircle(fx, fy, 2*s);
        g.fillStyle(fc, 0.5);
        g.fillCircle(fx - 1.5*s, fy - 1*s, 1.5*s);
        g.fillCircle(fx + 1.5*s, fy - 1*s, 1.5*s);
      }
      this.strokeDiamond(g, w, h, 0x2d6b25, 0.3);
    });

    this.makeIsoTile('tile_dirt', (g, w, h, s) => {
      this.fillDiamond(g, w, h, 0x8b7355);
      const cx = w / 2, cy = h / 2;
      // Weathered earth texture
      for (let i = 0; i < 20; i++) {
        const px = cx + (Math.random() - 0.5) * w * 0.6;
        const py = cy + (Math.random() - 0.5) * h * 0.4;
        if (!this.isInsideDiamond(px, py, w, h)) continue;
        const c = [0x7a6548, 0x9c8468, 0x6b5a42, 0xa89070, 0x776040][Math.floor(Math.random() * 5)];
        g.fillStyle(c, 0.4);
        g.fillEllipse(px, py, (4 + Math.random() * 6)*s, (2 + Math.random() * 3)*s);
      }
      // Pebbles
      for (let i = 0; i < 8; i++) {
        const px = cx + (Math.random() - 0.5) * w * 0.5;
        const py = cy + (Math.random() - 0.5) * h * 0.3;
        if (!this.isInsideDiamond(px, py, w, h)) continue;
        g.fillStyle(0x999080, 0.6);
        g.fillEllipse(px, py, 3*s, 2*s);
        g.fillStyle(0xb0a890, 0.4);
        g.fillEllipse(px - 0.5*s, py - 0.5*s, 2*s, 1.5*s);
      }
      // Worn path cracks
      g.lineStyle(1*s, 0x6b5a42, 0.25);
      g.beginPath();
      g.moveTo(cx - 30*s, cy - 2*s);
      g.lineTo(cx - 10*s, cy + 3*s);
      g.lineTo(cx + 15*s, cy - 1*s);
      g.lineTo(cx + 35*s, cy + 2*s);
      g.strokePath();
      this.strokeDiamond(g, w, h, 0x5a4a35, 0.35);
    });

    this.makeIsoTile('tile_stone', (g, w, h, s) => {
      this.fillDiamond(g, w, h, 0x6a6a6a);
      const cx = w / 2, cy = h / 2;
      // Stone texture with cracks
      g.fillStyle(0x7a7a7a, 0.4);
      g.fillEllipse(cx - 15*s, cy - 5*s, 24*s, 12*s);
      g.fillStyle(0x5a5a5a, 0.4);
      g.fillEllipse(cx + 18*s, cy + 3*s, 20*s, 10*s);
      g.fillStyle(0x808080, 0.3);
      g.fillEllipse(cx + 5*s, cy - 8*s, 16*s, 8*s);
      // Brick mortar lines
      g.lineStyle(1*s, 0x4a4a4a, 0.5);
      g.beginPath(); g.moveTo(cx - 32*s, cy); g.lineTo(cx + 32*s, cy); g.strokePath();
      g.beginPath(); g.moveTo(cx - 24*s, cy - 10*s); g.lineTo(cx + 24*s, cy - 10*s); g.strokePath();
      g.beginPath(); g.moveTo(cx - 20*s, cy + 10*s); g.lineTo(cx + 20*s, cy + 10*s); g.strokePath();
      // Vertical mortar
      g.beginPath(); g.moveTo(cx, cy - 10*s); g.lineTo(cx, cy); g.strokePath();
      g.beginPath(); g.moveTo(cx - 16*s, cy); g.lineTo(cx - 16*s, cy + 10*s); g.strokePath();
      g.beginPath(); g.moveTo(cx + 16*s, cy); g.lineTo(cx + 16*s, cy + 10*s); g.strokePath();
      // Highlight chips
      for (let i = 0; i < 6; i++) {
        const hx = cx + (Math.random() - 0.5) * w * 0.4;
        const hy = cy + (Math.random() - 0.5) * h * 0.3;
        if (!this.isInsideDiamond(hx, hy, w, h)) continue;
        g.fillStyle(0x8a8a8a, 0.5);
        g.fillRect(hx, hy, 3*s, 1.5*s);
      }
      this.strokeDiamond(g, w, h, 0x444444, 0.45);
    });

    this.makeIsoTile('tile_water', (g, w, h, s) => {
      this.fillDiamond(g, w, h, 0x1a5276);
      const cx = w / 2, cy = h / 2;
      // Deep water gradient
      g.fillStyle(0x2471a3, 0.6);
      this.fillDiamondInset(g, w, h, 8*s);
      g.fillStyle(0x2e86c1, 0.4);
      this.fillDiamondInset(g, w, h, 16*s);
      // Ripples
      g.lineStyle(1.5*s, 0x5dade2, 0.5);
      g.beginPath(); g.arc(cx - 15*s, cy - 4*s, 10*s, 0.3, 2.6, false); g.strokePath();
      g.beginPath(); g.arc(cx + 12*s, cy + 3*s, 8*s, 0.5, 2.8, false); g.strokePath();
      g.lineStyle(1*s, 0x85c1e9, 0.35);
      g.beginPath(); g.arc(cx + 5*s, cy - 8*s, 6*s, 0.2, 2.5, false); g.strokePath();
      // Shimmer highlights
      g.fillStyle(0xaed6f1, 0.5);
      g.fillEllipse(cx + 6*s, cy - 6*s, 6*s, 2*s);
      g.fillEllipse(cx - 18*s, cy + 2*s, 4*s, 1.5*s);
      g.fillStyle(0xd4efff, 0.3);
      g.fillEllipse(cx - 5*s, cy + 8*s, 5*s, 2*s);
      this.strokeDiamond(g, w, h, 0x154360, 0.5);
    });

    this.makeIsoTile('tile_wall', (g, w, h, s) => {
      // Wall with 3D height
      const wallHeight = 14 * s;
      // Front face
      g.fillStyle(0x3a3a3a);
      g.fillPoints([
        new Phaser.Geom.Point(0, h / 2),
        new Phaser.Geom.Point(w / 2, h),
        new Phaser.Geom.Point(w / 2, h - wallHeight),
        new Phaser.Geom.Point(0, h / 2 - wallHeight),
      ], true);
      // Right face
      g.fillStyle(0x4a4a4a);
      g.fillPoints([
        new Phaser.Geom.Point(w / 2, h),
        new Phaser.Geom.Point(w, h / 2),
        new Phaser.Geom.Point(w, h / 2 - wallHeight),
        new Phaser.Geom.Point(w / 2, h - wallHeight),
      ], true);
      // Top face
      g.fillStyle(0x555555);
      g.fillPoints([
        new Phaser.Geom.Point(w / 2, 0 - wallHeight + h / 2),
        new Phaser.Geom.Point(w, h / 2 - wallHeight),
        new Phaser.Geom.Point(w / 2, h - wallHeight),
        new Phaser.Geom.Point(0, h / 2 - wallHeight),
      ], true);
      // Brick lines on front
      g.lineStyle(1*s, 0x2a2a2a, 0.6);
      const faceBottom = h;
      g.beginPath(); g.moveTo(8*s, faceBottom / 2 - 3*s); g.lineTo(w / 2 - 5*s, faceBottom - 6*s); g.strokePath();
      g.beginPath(); g.moveTo(4*s, faceBottom / 2 - 8*s); g.lineTo(w / 2 - 8*s, faceBottom - 14*s); g.strokePath();
      // Brick lines on right face
      g.beginPath(); g.moveTo(w / 2 + 5*s, faceBottom - 6*s); g.lineTo(w - 8*s, faceBottom / 2 - 3*s); g.strokePath();
      g.beginPath(); g.moveTo(w / 2 + 8*s, faceBottom - 14*s); g.lineTo(w - 4*s, faceBottom / 2 - 8*s); g.strokePath();
      // Edge highlights
      g.lineStyle(1*s, 0x666666, 0.3);
      g.beginPath();
      g.moveTo(w / 2, h - wallHeight);
      g.lineTo(w, h / 2 - wallHeight);
      g.strokePath();
    });

    this.makeIsoTile('tile_camp', (g, w, h, s) => {
      this.fillDiamond(g, w, h, 0x9e7c52);
      const cx = w / 2, cy = h / 2;
      // Wooden plank texture
      for (let i = -5; i <= 5; i++) {
        const ly = cy + i * 4*s;
        const inset = Math.abs(i) * 5*s;
        g.lineStyle(1*s, 0x7a5c34, 0.3);
        g.beginPath(); g.moveTo(cx - 40*s + inset, ly); g.lineTo(cx + 40*s - inset, ly); g.strokePath();
      }
      // Wood grain
      for (let i = 0; i < 12; i++) {
        const gx = cx + (Math.random() - 0.5) * w * 0.5;
        const gy = cy + (Math.random() - 0.5) * h * 0.4;
        if (!this.isInsideDiamond(gx, gy, w, h)) continue;
        g.fillStyle(0x8b6f47, 0.25);
        g.fillEllipse(gx, gy, 6*s, 1.5*s);
      }
      // Campfire glow
      g.fillStyle(0xf39c12, 0.2);
      g.fillCircle(cx, cy, 12*s);
      g.fillStyle(0xe74c3c, 0.25);
      g.fillCircle(cx, cy, 8*s);
      // Flame
      g.fillStyle(0xf39c12, 0.7);
      g.fillTriangle(cx - 3*s, cy + 3*s, cx + 3*s, cy + 3*s, cx, cy - 8*s);
      g.fillStyle(0xf1c40f, 0.8);
      g.fillTriangle(cx - 2*s, cy + 2*s, cx + 2*s, cy + 2*s, cx, cy - 5*s);
      g.fillStyle(0xffffff, 0.4);
      g.fillTriangle(cx - 1*s, cy + 1*s, cx + 1*s, cy + 1*s, cx, cy - 3*s);
      // Fire base - logs
      g.fillStyle(0x5d4037);
      g.fillEllipse(cx - 4*s, cy + 4*s, 8*s, 3*s);
      g.fillEllipse(cx + 4*s, cy + 5*s, 8*s, 3*s);
      this.strokeDiamond(g, w, h, 0x6b5a42, 0.4);
    });
  }

  // ── HD Player Sprites (64x96 base, scaled by TEXTURE_SCALE) ────────────────────────────
  private generatePlayerSprites(): void {
    const classes: [string, number, number, number, string][] = [
      ['player_warrior', 0x3a7bd5, 0x2a5fa8, 0x78909c, 'sword'],
      ['player_mage',    0x9b59b6, 0x7d3c98, 0x7b1fa2, 'staff'],
      ['player_rogue',   0x27ae60, 0x1e8449, 0x2e7d32, 'dagger'],
    ];
    for (const [key, bodyColor, darkColor, accentColor, weapon] of classes) {
      this.makeSprite(key, 64, 96, (g, s) => {
        const cx = 32*s, baseY = 88*s;
        // Shadow
        g.fillStyle(0x000000, 0.3);
        g.fillEllipse(cx, baseY, 36*s, 10*s);

        // Boots
        g.fillStyle(0x4e342e);
        g.fillRoundedRect(18*s, 74*s, 10*s, 10*s, 2*s);
        g.fillRoundedRect(34*s, 74*s, 10*s, 10*s, 2*s);
        g.fillStyle(0x3e2723);
        g.fillRect(18*s, 78*s, 10*s, 3*s);
        g.fillRect(34*s, 78*s, 10*s, 3*s);
        // Boot straps
        g.fillStyle(0x6d4c41, 0.6);
        g.fillRect(19*s, 76*s, 8*s, 1*s);
        g.fillRect(35*s, 76*s, 8*s, 1*s);

        // Legs
        g.fillStyle(0x546e7a);
        g.fillRoundedRect(20*s, 60*s, 8*s, 16*s, 2*s);
        g.fillRoundedRect(34*s, 60*s, 8*s, 16*s, 2*s);
        // Knee guards
        g.fillStyle(darkColor, 0.5);
        g.fillRoundedRect(20*s, 64*s, 8*s, 5*s, 1*s);
        g.fillRoundedRect(34*s, 64*s, 8*s, 5*s, 1*s);

        // Body armor
        g.fillStyle(bodyColor);
        g.fillRoundedRect(16*s, 38*s, 30*s, 24*s, 4*s);
        // Chest plate detail
        g.fillStyle(darkColor);
        g.fillRoundedRect(16*s, 38*s, 30*s, 4*s, { tl: 4*s, tr: 4*s, bl: 0, br: 0 });
        g.fillRoundedRect(16*s, 58*s, 30*s, 4*s, { tl: 0, tr: 0, bl: 4*s, br: 4*s });
        // Center line
        g.fillStyle(darkColor, 0.4);
        g.fillRect(30*s, 42*s, 2*s, 16*s);
        // Emblem
        g.fillStyle(0xfdd835, 0.6);
        g.fillCircle(cx, 50*s, 3*s);

        // Belt
        g.fillStyle(0x6d4c41);
        g.fillRect(16*s, 58*s, 30*s, 4*s);
        g.fillStyle(0xfdd835, 0.8);
        g.fillRoundedRect(28*s, 58*s, 6*s, 4*s, 1*s);

        // Arms
        g.fillStyle(bodyColor);
        g.fillRoundedRect(10*s, 40*s, 7*s, 16*s, 3*s);
        g.fillRoundedRect(45*s, 40*s, 7*s, 16*s, 3*s);
        // Pauldrons
        g.fillStyle(darkColor);
        g.fillEllipse(13.5*s, 40*s, 10*s, 6*s);
        g.fillEllipse(48.5*s, 40*s, 10*s, 6*s);
        g.fillStyle(accentColor, 0.3);
        g.fillEllipse(13.5*s, 39*s, 8*s, 4*s);
        g.fillEllipse(48.5*s, 39*s, 8*s, 4*s);

        // Hands
        g.fillStyle(0xffcc80);
        g.fillCircle(13*s, 57*s, 4*s);
        g.fillCircle(49*s, 57*s, 4*s);

        // Neck
        g.fillStyle(0xffcc80);
        g.fillRect(27*s, 30*s, 8*s, 8*s);

        // Head
        g.fillStyle(0xffcc80);
        g.fillRoundedRect(21*s, 14*s, 20*s, 20*s, 6*s);
        // Eyes
        g.fillStyle(0x2c3e50);
        g.fillEllipse(26*s, 24*s, 3.5*s, 4*s);
        g.fillEllipse(36*s, 24*s, 3.5*s, 4*s);
        g.fillStyle(0xffffff);
        g.fillCircle(26.5*s, 23*s, 1*s);
        g.fillCircle(36.5*s, 23*s, 1*s);
        // Nose
        g.fillStyle(0xf0bb7a, 0.5);
        g.fillEllipse(31*s, 27*s, 3*s, 2*s);
        // Mouth
        g.fillStyle(0xc0946a, 0.6);
        g.fillEllipse(31*s, 30*s, 4*s, 1.5*s);

        // Class-specific headgear + weapon
        if (weapon === 'sword') {
          // Steel helm
          g.fillStyle(0x78909c);
          g.fillRoundedRect(19*s, 10*s, 24*s, 14*s, 5*s);
          g.fillStyle(0x90a4ae);
          g.fillRoundedRect(21*s, 8*s, 20*s, 6*s, 4*s);
          g.fillStyle(0x607d8b);
          g.fillRect(19*s, 22*s, 24*s, 2*s);
          // Nose guard
          g.fillStyle(0x78909c);
          g.fillRect(29*s, 22*s, 4*s, 6*s);
          // Helm crest
          g.fillStyle(0xe74c3c, 0.7);
          g.fillRect(29*s, 5*s, 4*s, 6*s);
          g.fillRect(28*s, 5*s, 6*s, 2*s);
          // Sword (right side)
          g.fillStyle(0xbdc3c7);
          g.fillRect(52*s, 20*s, 3*s, 34*s);
          g.fillStyle(0xecf0f1);
          g.fillRect(52.5*s, 20*s, 1.5*s, 30*s);
          // Crossguard
          g.fillStyle(0x8d6e63);
          g.fillRoundedRect(49*s, 50*s, 9*s, 4*s, 1*s);
          // Pommel
          g.fillStyle(0xfdd835);
          g.fillCircle(53.5*s, 56*s, 2.5*s);
          // Shield (left side)
          g.fillStyle(0x2c3e50);
          g.fillRoundedRect(2*s, 42*s, 12*s, 16*s, 3*s);
          g.fillStyle(0x3498db, 0.5);
          g.fillRoundedRect(4*s, 44*s, 8*s, 12*s, 2*s);
          g.fillStyle(0xfdd835, 0.6);
          g.fillRect(7*s, 45*s, 2*s, 10*s);
          g.fillRect(4*s, 49*s, 8*s, 2*s);
        } else if (weapon === 'staff') {
          // Wizard hat
          g.fillStyle(0x6a1b9a);
          g.fillRoundedRect(17*s, 12*s, 28*s, 10*s, 4*s);
          g.fillTriangle(31*s, -4*s, 21*s, 15*s, 41*s, 15*s);
          g.fillStyle(0x8e24aa, 0.5);
          g.fillTriangle(31*s, -2*s, 24*s, 14*s, 38*s, 14*s);
          // Hat brim
          g.fillStyle(0x4a148c);
          g.fillEllipse(31*s, 18*s, 30*s, 6*s);
          // Star on hat
          g.fillStyle(0xfdd835, 0.9);
          g.fillCircle(31*s, 4*s, 3*s);
          g.fillRect(30*s, 1*s, 2*s, 1*s);
          // Staff
          g.fillStyle(0x5d4037);
          g.fillRoundedRect(52*s, 8*s, 3*s, 52*s, 1*s);
          g.fillStyle(0x4e342e);
          g.fillRect(52.5*s, 10*s, 2*s, 48*s);
          // Orb on top
          g.fillStyle(0x7e57c2);
          g.fillCircle(53.5*s, 8*s, 6*s);
          g.fillStyle(0xce93d8, 0.6);
          g.fillCircle(52*s, 6*s, 3*s);
          g.fillStyle(0xffffff, 0.3);
          g.fillCircle(51*s, 5*s, 1.5*s);
          // Glow
          g.fillStyle(0xce93d8, 0.15);
          g.fillCircle(53.5*s, 8*s, 10*s);
        } else {
          // Rogue hood
          g.fillStyle(0x2e7d32);
          g.fillRoundedRect(18*s, 10*s, 26*s, 16*s, 5*s);
          g.fillStyle(0x1b5e20);
          g.fillTriangle(31*s, 6*s, 20*s, 14*s, 42*s, 14*s);
          // Hood shadow
          g.fillStyle(0x1a5c1f, 0.4);
          g.fillRoundedRect(20*s, 20*s, 22*s, 4*s, 1*s);
          // Mask
          g.fillStyle(0x1b5e20, 0.7);
          g.fillRect(22*s, 27*s, 18*s, 4*s);
          // Dual daggers
          g.fillStyle(0xbdc3c7);
          g.fillRect(52*s, 38*s, 2*s, 18*s);
          g.fillStyle(0xecf0f1);
          g.fillRect(52.3*s, 38*s, 1*s, 16*s);
          g.fillStyle(0x6d4c41);
          g.fillRoundedRect(50*s, 54*s, 6*s, 3*s, 1*s);
          // Left dagger
          g.fillStyle(0xbdc3c7);
          g.fillRect(8*s, 38*s, 2*s, 18*s);
          g.fillStyle(0xecf0f1);
          g.fillRect(8.3*s, 38*s, 1*s, 16*s);
          g.fillStyle(0x6d4c41);
          g.fillRoundedRect(6*s, 54*s, 6*s, 3*s, 1*s);
          // Quiver on back
          g.fillStyle(0x5d4037);
          g.fillRoundedRect(40*s, 32*s, 6*s, 18*s, 2*s);
          g.fillStyle(0x8d6e63, 0.5);
          g.fillRect(41*s, 33*s, 4*s, 2*s);
          // Arrow feathers
          g.fillStyle(0xfdd835, 0.6);
          g.fillRect(42*s, 30*s, 1*s, 4*s);
          g.fillRect(44*s, 31*s, 1*s, 3*s);
        }
      });
    }
  }

  // ── HD Monster Sprites ───────────────────────────────────
  private generateMonsterSprites(): void {
    // Slime (48x40)
    this.makeSprite('monster_slime', 48, 40, (g, s) => {
      g.fillStyle(0x000000, 0.2);
      g.fillEllipse(24*s, 36*s, 32*s, 8*s);
      // Body
      g.fillStyle(0x27ae60);
      g.fillEllipse(24*s, 24*s, 36*s, 28*s);
      // Highlight
      g.fillStyle(0x58d68d, 0.5);
      g.fillEllipse(20*s, 18*s, 14*s, 10*s);
      g.fillStyle(0x82e0aa, 0.3);
      g.fillEllipse(18*s, 15*s, 8*s, 6*s);
      // Drips
      g.fillStyle(0x27ae60, 0.7);
      g.fillEllipse(14*s, 36*s, 6*s, 4*s);
      g.fillEllipse(34*s, 35*s, 5*s, 3*s);
      // Eyes
      g.fillStyle(0xffffff);
      g.fillEllipse(18*s, 20*s, 7*s, 8*s);
      g.fillEllipse(30*s, 20*s, 7*s, 8*s);
      g.fillStyle(0x1a5c1f);
      g.fillCircle(19*s, 21*s, 2.5*s);
      g.fillCircle(31*s, 21*s, 2.5*s);
      g.fillStyle(0x000000);
      g.fillCircle(19.5*s, 21.5*s, 1*s);
      g.fillCircle(31.5*s, 21.5*s, 1*s);
      // Mouth
      g.lineStyle(1.5*s, 0x1a6e30, 0.6);
      g.beginPath(); g.arc(24*s, 27*s, 5*s, 0.2, 2.9, false); g.strokePath();
    });

    // Goblin (48x56)
    this.makeSprite('monster_goblin', 48, 56, (g, s) => {
      g.fillStyle(0x000000, 0.2);
      g.fillEllipse(24*s, 52*s, 28*s, 8*s);
      // Feet
      g.fillStyle(0x4e6b3a);
      g.fillEllipse(17*s, 50*s, 8*s, 5*s);
      g.fillEllipse(31*s, 50*s, 8*s, 5*s);
      // Legs
      g.fillStyle(0x558b2f);
      g.fillRoundedRect(14*s, 38*s, 7*s, 14*s, 2*s);
      g.fillRoundedRect(27*s, 38*s, 7*s, 14*s, 2*s);
      // Body - tattered vest
      g.fillStyle(0x689f38);
      g.fillRoundedRect(12*s, 22*s, 24*s, 18*s, 4*s);
      g.fillStyle(0x5d4037, 0.6);
      g.fillRoundedRect(14*s, 24*s, 20*s, 14*s, 2*s);
      g.fillStyle(0x4e342e, 0.4);
      g.fillRect(23*s, 26*s, 2*s, 10*s);
      // Arms
      g.fillStyle(0x7cb342);
      g.fillRoundedRect(8*s, 24*s, 6*s, 14*s, 2*s);
      g.fillRoundedRect(34*s, 26*s, 6*s, 12*s, 2*s);
      // Hands
      g.fillStyle(0x8bc34a);
      g.fillCircle(11*s, 38*s, 3.5*s);
      g.fillCircle(37*s, 38*s, 3.5*s);
      // Head
      g.fillStyle(0x7cb342);
      g.fillRoundedRect(14*s, 6*s, 20*s, 18*s, 6*s);
      // Ears (big pointy)
      g.fillStyle(0x8bc34a);
      g.fillTriangle(8*s, 14*s, 14*s, 8*s, 14*s, 18*s);
      g.fillTriangle(40*s, 14*s, 34*s, 8*s, 34*s, 18*s);
      g.fillStyle(0xaed581, 0.4);
      g.fillTriangle(10*s, 14*s, 14*s, 10*s, 14*s, 16*s);
      g.fillTriangle(38*s, 14*s, 34*s, 10*s, 34*s, 16*s);
      // Eyes
      g.fillStyle(0xffeb3b);
      g.fillEllipse(20*s, 14*s, 6*s, 7*s);
      g.fillEllipse(30*s, 14*s, 6*s, 7*s);
      g.fillStyle(0x000000);
      g.fillCircle(21*s, 15*s, 2*s);
      g.fillCircle(31*s, 15*s, 2*s);
      // Nose
      g.fillStyle(0x689f38);
      g.fillEllipse(25*s, 18*s, 5*s, 3*s);
      // Mouth
      g.lineStyle(1*s, 0x33691e, 0.7);
      g.beginPath(); g.arc(25*s, 21*s, 4*s, 0.1, 3.0, false); g.strokePath();
      // Club
      g.fillStyle(0x5d4037);
      g.fillRoundedRect(38*s, 20*s, 4*s, 20*s, 1*s);
      g.fillStyle(0x795548);
      g.fillRoundedRect(36*s, 16*s, 8*s, 6*s, 2*s);
      // Nails in club
      g.fillStyle(0xbdbdbd, 0.7);
      g.fillCircle(38*s, 18*s, 1*s);
      g.fillCircle(42*s, 19*s, 1*s);
    });

    // Goblin Chief (60x68)
    this.makeSprite('monster_goblin_chief', 60, 68, (g, s) => {
      g.fillStyle(0x000000, 0.25);
      g.fillEllipse(30*s, 64*s, 36*s, 10*s);
      // Feet
      g.fillStyle(0x3e5228);
      g.fillEllipse(21*s, 62*s, 10*s, 6*s);
      g.fillEllipse(39*s, 62*s, 10*s, 6*s);
      // Legs
      g.fillStyle(0x4a7023);
      g.fillRoundedRect(17*s, 46*s, 10*s, 18*s, 3*s);
      g.fillRoundedRect(33*s, 46*s, 10*s, 18*s, 3*s);
      // Body - armored
      g.fillStyle(0x558b2f);
      g.fillRoundedRect(13*s, 26*s, 34*s, 22*s, 5*s);
      g.fillStyle(0x795548);
      g.fillRoundedRect(16*s, 28*s, 28*s, 18*s, 3*s);
      g.fillStyle(0x8d6e63, 0.5);
      g.fillRect(29*s, 30*s, 2*s, 14*s);
      // Skull belt buckle
      g.fillStyle(0xe0e0e0);
      g.fillCircle(30*s, 46*s, 4*s);
      g.fillStyle(0x000000);
      g.fillCircle(28*s, 45*s, 1*s); g.fillCircle(32*s, 45*s, 1*s);
      // Arms
      g.fillStyle(0x689f38);
      g.fillRoundedRect(7*s, 28*s, 8*s, 18*s, 3*s);
      g.fillRoundedRect(45*s, 28*s, 8*s, 18*s, 3*s);
      // Pauldrons
      g.fillStyle(0x795548);
      g.fillEllipse(11*s, 28*s, 12*s, 7*s);
      g.fillEllipse(49*s, 28*s, 12*s, 7*s);
      // Head
      g.fillStyle(0x7cb342);
      g.fillRoundedRect(16*s, 6*s, 28*s, 22*s, 7*s);
      // Ears
      g.fillStyle(0x8bc34a);
      g.fillTriangle(8*s, 16*s, 16*s, 8*s, 16*s, 22*s);
      g.fillTriangle(52*s, 16*s, 44*s, 8*s, 44*s, 22*s);
      // Eyes (angry red)
      g.fillStyle(0xff1744);
      g.fillEllipse(24*s, 16*s, 6*s, 7*s);
      g.fillEllipse(38*s, 16*s, 6*s, 7*s);
      g.fillStyle(0x000000);
      g.fillCircle(25*s, 17*s, 2*s);
      g.fillCircle(39*s, 17*s, 2*s);
      // Crown
      g.fillStyle(0xfdd835);
      g.fillRoundedRect(17*s, 2*s, 26*s, 6*s, 2*s);
      g.fillTriangle(20*s, 2*s, 22*s, -4*s, 24*s, 2*s);
      g.fillTriangle(28*s, 2*s, 30*s, -6*s, 32*s, 2*s);
      g.fillTriangle(36*s, 2*s, 38*s, -4*s, 40*s, 2*s);
      g.fillStyle(0xe53935);
      g.fillCircle(30*s, -2*s, 2*s);
      // Battle axe
      g.fillStyle(0x5d4037);
      g.fillRoundedRect(52*s, 10*s, 3*s, 40*s, 1*s);
      g.fillStyle(0x757575);
      g.beginPath();
      g.moveTo(55*s, 10*s); g.lineTo(62*s, 16*s); g.lineTo(62*s, 22*s); g.lineTo(55*s, 26*s);
      g.closePath(); g.fillPath();
      g.fillStyle(0x9e9e9e, 0.5);
      g.beginPath();
      g.moveTo(55*s, 12*s); g.lineTo(60*s, 16*s); g.lineTo(60*s, 22*s); g.lineTo(55*s, 24*s);
      g.closePath(); g.fillPath();
    });

    // Skeleton (44x60)
    this.makeSprite('monster_skeleton', 44, 60, (g, s) => {
      g.fillStyle(0x000000, 0.2);
      g.fillEllipse(22*s, 56*s, 28*s, 7*s);
      // Feet
      g.fillStyle(0xbdbdbd);
      g.fillEllipse(15*s, 55*s, 7*s, 4*s);
      g.fillEllipse(29*s, 55*s, 7*s, 4*s);
      // Leg bones
      g.fillStyle(0xe0e0e0);
      g.fillRoundedRect(14*s, 40*s, 4*s, 16*s, 1*s);
      g.fillRoundedRect(26*s, 40*s, 4*s, 16*s, 1*s);
      // Pelvis
      g.fillStyle(0xd0d0d0);
      g.fillEllipse(22*s, 40*s, 16*s, 6*s);
      // Ribcage
      g.fillStyle(0xe0e0e0);
      g.fillRoundedRect(14*s, 22*s, 16*s, 18*s, 4*s);
      // Ribs
      for (let i = 0; i < 4; i++) {
        const ry = (26 + i * 4)*s;
        g.lineStyle(1.5*s, 0xbdbdbd, 0.7);
        g.beginPath(); g.arc(22*s, ry, 6*s, 0.3, 2.8, false); g.strokePath();
      }
      // Spine
      g.fillStyle(0xc8c8c8);
      g.fillRect(21*s, 22*s, 2*s, 18*s);
      // Arms
      g.fillStyle(0xe0e0e0);
      g.fillRoundedRect(8*s, 24*s, 4*s, 16*s, 1*s);
      g.fillRoundedRect(32*s, 24*s, 4*s, 16*s, 1*s);
      // Hands
      g.fillStyle(0xd0d0d0);
      g.fillCircle(10*s, 40*s, 3*s);
      g.fillCircle(34*s, 40*s, 3*s);
      // Skull
      g.fillStyle(0xeeeeee);
      g.fillRoundedRect(13*s, 6*s, 18*s, 18*s, 6*s);
      // Eye sockets
      g.fillStyle(0x1a1a1a);
      g.fillEllipse(18*s, 14*s, 5*s, 6*s);
      g.fillEllipse(28*s, 14*s, 5*s, 6*s);
      // Glowing eyes
      g.fillStyle(0x66bb6a, 0.8);
      g.fillCircle(18*s, 14*s, 1.5*s);
      g.fillCircle(28*s, 14*s, 1.5*s);
      // Nose hole
      g.fillStyle(0x2a2a2a);
      g.fillTriangle(22*s, 18*s, 24*s, 18*s, 23*s, 20*s);
      // Jaw
      g.fillStyle(0xd0d0d0);
      g.fillRoundedRect(15*s, 22*s, 14*s, 4*s, 1*s);
      g.fillStyle(0xe0e0e0);
      for (let i = 0; i < 5; i++) g.fillRect((16 + i * 2.5)*s, 22*s, 1.5*s, 2*s);
      // Rusty sword
      g.fillStyle(0x8d6e63);
      g.fillRoundedRect(36*s, 18*s, 3*s, 24*s, 1*s);
      g.fillStyle(0xa1887f, 0.5);
      g.fillRect(36.5*s, 18*s, 1.5*s, 20*s);
      g.fillStyle(0x5d4037);
      g.fillRoundedRect(34*s, 40*s, 7*s, 3*s, 1*s);
    });

    // Zombie (44x60)
    this.makeSprite('monster_zombie', 44, 60, (g, s) => {
      g.fillStyle(0x000000, 0.2);
      g.fillEllipse(22*s, 56*s, 28*s, 7*s);
      // Ragged feet
      g.fillStyle(0x5d4037, 0.7);
      g.fillEllipse(15*s, 55*s, 8*s, 4*s);
      g.fillEllipse(29*s, 54*s, 7*s, 5*s);
      // Legs (torn pants)
      g.fillStyle(0x4e342e);
      g.fillRoundedRect(13*s, 40*s, 7*s, 16*s, 2*s);
      g.fillRoundedRect(25*s, 40*s, 7*s, 16*s, 2*s);
      g.fillStyle(0x6d8b74, 0.4);
      g.fillRect(14*s, 48*s, 5*s, 4*s);
      // Body (rotting)
      g.fillStyle(0x6d8b74);
      g.fillRoundedRect(12*s, 22*s, 20*s, 20*s, 4*s);
      g.fillStyle(0x4e342e, 0.6);
      g.fillRoundedRect(14*s, 24*s, 16*s, 16*s, 2*s);
      // Exposed ribs
      g.lineStyle(1*s, 0x8fad8b, 0.4);
      g.beginPath(); g.moveTo(16*s, 30*s); g.lineTo(28*s, 30*s); g.strokePath();
      g.beginPath(); g.moveTo(16*s, 34*s); g.lineTo(28*s, 34*s); g.strokePath();
      // Arms (asymmetric - zombie!)
      g.fillStyle(0x7da17a);
      g.fillRoundedRect(6*s, 24*s, 7*s, 18*s, 2*s);
      g.fillStyle(0x6d8b74);
      g.fillRoundedRect(33*s, 28*s, 6*s, 14*s, 2*s);
      // Hands
      g.fillStyle(0x8fad8b);
      g.fillCircle(9*s, 42*s, 4*s);
      g.fillCircle(36*s, 42*s, 3*s);
      // Head
      g.fillStyle(0x8fad8b);
      g.fillRoundedRect(14*s, 6*s, 18*s, 18*s, 5*s);
      // Messy hair
      g.fillStyle(0x4a4a4a);
      g.fillRoundedRect(14*s, 4*s, 18*s, 8*s, 3*s);
      g.fillRect(12*s, 8*s, 4*s, 6*s);
      // Eyes (one droopy)
      g.fillStyle(0xc62828);
      g.fillEllipse(19*s, 14*s, 4*s, 5*s);
      g.fillStyle(0xb71c1c);
      g.fillEllipse(29*s, 15*s, 4*s, 4*s);
      g.fillStyle(0x000000);
      g.fillCircle(19*s, 14*s, 1.5*s);
      g.fillCircle(29*s, 15*s, 1*s);
      // Mouth (open, moaning)
      g.fillStyle(0x3e2723);
      g.fillEllipse(23*s, 20*s, 8*s, 4*s);
      g.fillStyle(0xeeeeee, 0.5);
      g.fillRect(20*s, 19*s, 2*s, 2*s); g.fillRect(25*s, 19*s, 2*s, 2*s);
    });

    // Werewolf (52x64)
    this.makeSprite('monster_werewolf', 52, 64, (g, s) => {
      g.fillStyle(0x000000, 0.25);
      g.fillEllipse(26*s, 60*s, 36*s, 10*s);
      // Feet (paws)
      g.fillStyle(0x4e342e);
      g.fillEllipse(17*s, 58*s, 10*s, 6*s);
      g.fillEllipse(35*s, 58*s, 10*s, 6*s);
      // Claws on feet
      g.fillStyle(0xe0e0e0);
      g.fillRect(13*s, 55*s, 1.5*s, 3*s); g.fillRect(16*s, 55*s, 1.5*s, 3*s); g.fillRect(19*s, 55*s, 1.5*s, 3*s);
      g.fillRect(31*s, 55*s, 1.5*s, 3*s); g.fillRect(34*s, 55*s, 1.5*s, 3*s); g.fillRect(37*s, 55*s, 1.5*s, 3*s);
      // Legs (muscular)
      g.fillStyle(0x5d4037);
      g.fillRoundedRect(13*s, 42*s, 10*s, 18*s, 3*s);
      g.fillRoundedRect(29*s, 42*s, 10*s, 18*s, 3*s);
      // Body (broad, furry)
      g.fillStyle(0x6d4c41);
      g.fillRoundedRect(10*s, 22*s, 32*s, 22*s, 6*s);
      // Chest tuft
      g.fillStyle(0x8d6e63, 0.6);
      g.fillEllipse(26*s, 30*s, 14*s, 10*s);
      // Arms (powerful)
      g.fillStyle(0x5d4037);
      g.fillRoundedRect(4*s, 24*s, 8*s, 20*s, 3*s);
      g.fillRoundedRect(40*s, 24*s, 8*s, 20*s, 3*s);
      // Claws
      g.fillStyle(0xe0e0e0);
      for (let i = 0; i < 3; i++) {
        g.fillRect((4 + i * 2.5)*s, 43*s, 1.5*s, 4*s);
        g.fillRect((40 + i * 2.5)*s, 43*s, 1.5*s, 4*s);
      }
      // Head (wolf)
      g.fillStyle(0x6d4c41);
      g.fillRoundedRect(14*s, 4*s, 24*s, 20*s, 7*s);
      // Ears
      g.fillStyle(0x5d4037);
      g.fillTriangle(14*s, 8*s, 18*s, -2*s, 22*s, 8*s);
      g.fillTriangle(38*s, 8*s, 34*s, -2*s, 30*s, 8*s);
      g.fillStyle(0xa1887f, 0.4);
      g.fillTriangle(16*s, 7*s, 18*s, 0, 20*s, 7*s);
      g.fillTriangle(36*s, 7*s, 34*s, 0, 32*s, 7*s);
      // Snout
      g.fillStyle(0x8d6e63);
      g.fillRoundedRect(20*s, 16*s, 12*s, 8*s, 3*s);
      g.fillStyle(0x3e2723);
      g.fillEllipse(26*s, 17*s, 4*s, 2*s);
      // Mouth
      g.lineStyle(1*s, 0x3e2723, 0.8);
      g.beginPath(); g.moveTo(20*s, 22*s); g.lineTo(32*s, 22*s); g.strokePath();
      // Fangs
      g.fillStyle(0xeeeeee);
      g.fillTriangle(22*s, 22*s, 23*s, 25*s, 24*s, 22*s);
      g.fillTriangle(28*s, 22*s, 29*s, 25*s, 30*s, 22*s);
      // Eyes (glowing)
      g.fillStyle(0xffeb3b);
      g.fillEllipse(20*s, 12*s, 5*s, 5*s);
      g.fillEllipse(32*s, 12*s, 5*s, 5*s);
      g.fillStyle(0xff8f00);
      g.fillCircle(20*s, 12*s, 1.5*s);
      g.fillCircle(32*s, 12*s, 1.5*s);
    });

    // Generic monsters for other zones
    this.generateGenericMonsters();
  }

  private generateGenericMonsters(): void {
    const defs: [string, number, number, number, string][] = [
      ['monster_werewolf_alpha', 0x3e2723, 56, 68, 'boss'],
      ['monster_gargoyle',       0x546e7a, 52, 60, 'wings'],
      ['monster_stone_golem',    0x757575, 60, 68, 'golem'],
      ['monster_mountain_troll', 0x4e6b3a, 64, 72, 'boss'],
      ['monster_fire_elemental', 0xe65100, 48, 60, 'fire'],
      ['monster_desert_scorpion',0x8d6e63, 52, 44, 'scorpion'],
      ['monster_sandworm',       0xc9a96e, 56, 48, 'worm'],
      ['monster_phoenix',        0xff6f00, 56, 56, 'fire'],
      ['monster_imp',            0xb71c1c, 40, 48, 'demon'],
      ['monster_lesser_demon',   0x880e4f, 52, 64, 'demon'],
      ['monster_succubus',       0xad1457, 48, 64, 'demon'],
      ['monster_demon_lord',     0x4a148c, 72, 84, 'boss'],
    ];
    for (const [key, baseColor, bw, bh, type] of defs) {
      this.makeSprite(key, bw, bh, (g, s) => {
        const w = bw * s, h = bh * s;
        const cx = w / 2, cy = h / 2;
        const light = Phaser.Display.Color.IntegerToColor(baseColor).lighten(20).color;
        const dark = Phaser.Display.Color.IntegerToColor(baseColor).darken(15).color;

        // Shadow
        g.fillStyle(0x000000, 0.25);
        g.fillEllipse(cx, h - 4*s, w * 0.65, h * 0.08);

        // Body
        g.fillStyle(baseColor);
        g.fillRoundedRect(w * 0.2, h * 0.32, w * 0.6, h * 0.42, w * 0.08);
        g.fillStyle(light, 0.3);
        g.fillEllipse(cx, h * 0.45, w * 0.35, h * 0.15);

        // Legs
        g.fillStyle(dark);
        g.fillRoundedRect(w * 0.25, h * 0.7, w * 0.15, h * 0.22, 3*s);
        g.fillRoundedRect(w * 0.55, h * 0.7, w * 0.15, h * 0.22, 3*s);

        // Head
        g.fillStyle(light);
        g.fillRoundedRect(w * 0.22, h * 0.08, w * 0.56, h * 0.28, w * 0.1);

        // Eyes
        const eyeColor = type === 'fire' ? 0xffeb3b : type === 'demon' ? 0xff1744 : 0xffffff;
        g.fillStyle(eyeColor);
        g.fillEllipse(cx - w * 0.1, h * 0.2, w * 0.08, h * 0.06);
        g.fillEllipse(cx + w * 0.1, h * 0.2, w * 0.08, h * 0.06);
        g.fillStyle(0x000000);
        g.fillCircle(cx - w * 0.09, h * 0.21, w * 0.025);
        g.fillCircle(cx + w * 0.09, h * 0.21, w * 0.025);

        // Arms
        g.fillStyle(baseColor);
        g.fillRoundedRect(w * 0.06, h * 0.35, w * 0.14, h * 0.25, 3*s);
        g.fillRoundedRect(w * 0.78, h * 0.35, w * 0.14, h * 0.25, 3*s);

        // Type-specific details
        if (type === 'fire') {
          // Flame particles
          g.fillStyle(0xff6f00, 0.6);
          g.fillTriangle(cx - 8*s, h * 0.15, cx - 4*s, h * 0.02, cx, h * 0.15);
          g.fillTriangle(cx + 2*s, h * 0.12, cx + 6*s, -2*s, cx + 10*s, h * 0.12);
          g.fillStyle(0xffab00, 0.4);
          g.fillTriangle(cx - 3*s, h * 0.13, cx, h * 0.04, cx + 3*s, h * 0.13);
          g.fillStyle(0xff6f00, 0.3);
          g.fillEllipse(w * 0.15, h * 0.3, 6*s, 8*s);
          g.fillEllipse(w * 0.85, h * 0.35, 6*s, 8*s);
        }
        if (type === 'demon') {
          // Horns
          g.fillStyle(0x212121);
          g.fillTriangle(w * 0.2, h * 0.12, w * 0.12, -2*s, w * 0.28, h * 0.12);
          g.fillTriangle(w * 0.72, h * 0.12, w * 0.8, -2*s, w * 0.64, h * 0.12);
          // Tail
          g.lineStyle(2*s, dark, 0.7);
          g.beginPath();
          g.moveTo(cx, h * 0.72);
          g.lineTo(cx + w * 0.2, h * 0.82);
          g.lineTo(cx + w * 0.3, h * 0.78);
          g.strokePath();
          g.fillStyle(dark);
          g.fillTriangle(cx + w * 0.28, h * 0.76, cx + w * 0.34, h * 0.78, cx + w * 0.3, h * 0.82);
        }
        if (type === 'wings') {
          g.fillStyle(baseColor, 0.6);
          // Left wing
          g.beginPath();
          g.moveTo(w * 0.2, h * 0.35);
          g.lineTo(0, h * 0.15);
          g.lineTo(w * 0.05, h * 0.45);
          g.closePath(); g.fillPath();
          // Right wing
          g.beginPath();
          g.moveTo(w * 0.8, h * 0.35);
          g.lineTo(w, h * 0.15);
          g.lineTo(w * 0.95, h * 0.45);
          g.closePath(); g.fillPath();
        }
        if (type === 'golem') {
          // Rock texture
          g.fillStyle(0x999999, 0.3);
          g.fillEllipse(cx - 6*s, h * 0.4, 8*s, 5*s);
          g.fillEllipse(cx + 8*s, h * 0.45, 6*s, 4*s);
          g.lineStyle(1*s, 0x555555, 0.4);
          g.beginPath(); g.moveTo(cx - 10*s, h * 0.42); g.lineTo(cx + 10*s, h * 0.48); g.strokePath();
        }
        if (type === 'scorpion') {
          // Claws
          g.fillStyle(dark);
          g.fillEllipse(w * 0.08, h * 0.35, 8*s, 5*s);
          g.fillEllipse(w * 0.92, h * 0.35, 8*s, 5*s);
          // Tail
          g.lineStyle(3*s, baseColor, 0.8);
          g.beginPath();
          g.moveTo(cx, h * 0.3);
          g.lineTo(cx + 5*s, h * 0.15);
          g.lineTo(cx + 8*s, h * 0.05);
          g.strokePath();
          g.fillStyle(0xc62828);
          g.fillCircle(cx + 8*s, h * 0.04, 3*s);
        }
        if (type === 'worm') {
          // Segmented body
          for (let i = 0; i < 4; i++) {
            g.lineStyle(1*s, dark, 0.4);
            g.beginPath();
            g.moveTo(w * 0.25, h * 0.35 + i * h * 0.1);
            g.lineTo(w * 0.75, h * 0.35 + i * h * 0.1);
            g.strokePath();
          }
          // Open mouth
          g.fillStyle(0x8b0000);
          g.fillEllipse(cx, h * 0.12, w * 0.3, h * 0.08);
        }
        if (type === 'boss') {
          // Crown/crest
          g.fillStyle(0xfdd835, 0.6);
          g.fillTriangle(cx - 6*s, h * 0.08, cx - 3*s, -3*s, cx, h * 0.08);
          g.fillTriangle(cx, h * 0.06, cx + 3*s, -5*s, cx + 6*s, h * 0.06);
          g.fillTriangle(cx + 6*s, h * 0.08, cx + 9*s, -3*s, cx + 12*s, h * 0.08);
          // Aura
          g.lineStyle(1.5*s, 0xffd700, 0.25);
          g.strokeCircle(cx, cy * 0.8, w * 0.42);
        }
        if (key === 'monster_demon_lord') {
          // Wings
          g.fillStyle(0x311b92, 0.6);
          g.beginPath();
          g.moveTo(w * 0.15, h * 0.3);
          g.lineTo(0, h * 0.08);
          g.lineTo(-4*s, h * 0.2);
          g.lineTo(w * 0.05, h * 0.45);
          g.closePath(); g.fillPath();
          g.beginPath();
          g.moveTo(w * 0.85, h * 0.3);
          g.lineTo(w, h * 0.08);
          g.lineTo(w + 4*s, h * 0.2);
          g.lineTo(w * 0.95, h * 0.45);
          g.closePath(); g.fillPath();
          // Dark aura
          g.fillStyle(0xea80fc, 0.1);
          g.fillCircle(cx, cy * 0.8, w * 0.45);
        }
      });
    }
  }

  // ── Effects ──────────────────────────────────────────────
  private generateEffects(): void {
    // Loot bag (24x24)
    this.makeSprite('loot_bag', 24, 24, (g, s) => {
      g.fillStyle(0x000000, 0.2);
      g.fillEllipse(12*s, 22*s, 16*s, 5*s);
      g.fillStyle(0x8d6e63);
      g.fillRoundedRect(5*s, 6*s, 14*s, 16*s, 3*s);
      g.fillStyle(0xa1887f);
      g.fillRoundedRect(7*s, 3*s, 10*s, 5*s, 2*s);
      g.fillStyle(0x6d4c41);
      g.fillRect(6*s, 8*s, 12*s, 2*s);
      g.fillStyle(0xfdd835, 0.8);
      g.fillCircle(12*s, 14*s, 3*s);
      g.fillStyle(0xf9a825, 0.5);
      g.fillCircle(11*s, 13*s, 1.5*s);
    });

    // Exit portal (32x32)
    this.makeSprite('exit_portal', 32, 32, (g, s) => {
      g.fillStyle(0x00e676, 0.2);
      g.fillCircle(16*s, 16*s, 14*s);
      g.fillStyle(0x00e676, 0.4);
      g.fillCircle(16*s, 16*s, 10*s);
      g.fillStyle(0x69f0ae, 0.6);
      g.fillCircle(16*s, 16*s, 6*s);
      g.fillStyle(0xffffff, 0.3);
      g.fillCircle(16*s, 16*s, 3*s);
      g.lineStyle(1.5*s, 0x00e676, 0.5);
      g.strokeCircle(16*s, 16*s, 12*s);
    });
  }

  // ── Helpers ──────────────────────────────────────────────
  private generateDecorations(): void {
    // Tree (24x36)
    this.makeSprite('decor_tree', 24, 36, (g, s) => {
      g.fillStyle(0x000000, 0.2); g.fillEllipse(12*s, 34*s, 16*s, 4*s);
      g.fillStyle(0x5d4037); g.fillRect(10*s, 22*s, 4*s, 14*s);
      g.fillStyle(0x4e342e); g.fillRect(11*s, 22*s, 2*s, 12*s);
      g.fillStyle(0x2e7d32); g.fillEllipse(12*s, 14*s, 20*s, 22*s);
      g.fillStyle(0x388e3c, 0.6); g.fillEllipse(10*s, 11*s, 12*s, 14*s);
      g.fillStyle(0x43a047, 0.4); g.fillEllipse(14*s, 8*s, 8*s, 8*s);
    });
    // Bush (16x12)
    this.makeSprite('decor_bush', 16, 12, (g, s) => {
      g.fillStyle(0x000000, 0.15); g.fillEllipse(8*s, 11*s, 14*s, 3*s);
      g.fillStyle(0x388e3c); g.fillEllipse(8*s, 7*s, 14*s, 10*s);
      g.fillStyle(0x4caf50, 0.5); g.fillEllipse(6*s, 5*s, 8*s, 6*s);
    });
    // Rock (16x12)
    this.makeSprite('decor_rock', 16, 12, (g, s) => {
      g.fillStyle(0x000000, 0.15); g.fillEllipse(8*s, 11*s, 12*s, 3*s);
      g.fillStyle(0x757575); g.fillEllipse(8*s, 7*s, 14*s, 10*s);
      g.fillStyle(0x9e9e9e, 0.5); g.fillEllipse(6*s, 5*s, 6*s, 4*s);
    });
    // Flower (8x10)
    this.makeSprite('decor_flower', 8, 10, (g, s) => {
      g.fillStyle(0x388e3c); g.fillRect(3*s, 5*s, 2*s, 5*s);
      const fc = [0xf44336, 0xffc107, 0xe91e63, 0x9c27b0][Math.floor(Math.random() * 4)];
      g.fillStyle(fc); g.fillCircle(4*s, 4*s, 3*s);
      g.fillStyle(0xffeb3b, 0.7); g.fillCircle(4*s, 4*s, 1.5*s);
    });
    // Mushroom (10x12)
    this.makeSprite('decor_mushroom', 10, 12, (g, s) => {
      g.fillStyle(0xbcaaa4); g.fillRect(4*s, 6*s, 3*s, 6*s);
      g.fillStyle(0xe53935); g.fillEllipse(5*s, 5*s, 10*s, 8*s);
      g.fillStyle(0xffffff, 0.6);
      g.fillCircle(3*s, 4*s, 1.5*s); g.fillCircle(6*s, 3*s, 1*s); g.fillCircle(7*s, 5*s, 1*s);
    });
    // Cactus (12x20)
    this.makeSprite('decor_cactus', 12, 20, (g, s) => {
      g.fillStyle(0x000000, 0.15); g.fillEllipse(6*s, 19*s, 10*s, 3*s);
      g.fillStyle(0x2e7d32); g.fillRoundedRect(4*s, 4*s, 5*s, 16*s, 2*s);
      g.fillStyle(0x2e7d32); g.fillRoundedRect(0, 8*s, 5*s, 4*s, 2*s);
      g.fillStyle(0x2e7d32); g.fillRoundedRect(8*s, 6*s, 4*s, 4*s, 2*s);
      g.fillStyle(0x43a047, 0.4); g.fillRect(5*s, 5*s, 2*s, 14*s);
    });
    // Boulder (20x16)
    this.makeSprite('decor_boulder', 20, 16, (g, s) => {
      g.fillStyle(0x000000, 0.2); g.fillEllipse(10*s, 15*s, 18*s, 4*s);
      g.fillStyle(0x616161); g.fillEllipse(10*s, 9*s, 18*s, 14*s);
      g.fillStyle(0x757575, 0.5); g.fillEllipse(8*s, 7*s, 10*s, 8*s);
      g.fillStyle(0x9e9e9e, 0.3); g.fillEllipse(6*s, 5*s, 6*s, 4*s);
    });
    // Crystal (10x16)
    this.makeSprite('decor_crystal', 10, 16, (g, s) => {
      g.fillStyle(0x000000, 0.15); g.fillEllipse(5*s, 15*s, 8*s, 3*s);
      g.fillStyle(0x7b1fa2); g.fillTriangle(5*s, 0, 0, 14*s, 10*s, 14*s);
      g.fillStyle(0xce93d8, 0.5); g.fillTriangle(5*s, 2*s, 2*s, 12*s, 6*s, 12*s);
      g.fillStyle(0xffffff, 0.3); g.fillTriangle(4*s, 4*s, 3*s, 8*s, 5*s, 8*s);
    });
    // Bones (14x10)
    this.makeSprite('decor_bones', 14, 10, (g, s) => {
      g.fillStyle(0xe0e0e0, 0.8);
      g.fillRoundedRect(1*s, 3*s, 12*s, 2*s, 1*s);
      g.fillRoundedRect(3*s, 1*s, 2*s, 8*s, 1*s);
      g.fillCircle(2*s, 4*s, 2*s); g.fillCircle(12*s, 4*s, 2*s);
      g.fillCircle(4*s, 1*s, 1.5*s); g.fillCircle(4*s, 9*s, 1.5*s);
    });
  }

  private makeIsoTile(key: string, draw: (g: Phaser.GameObjects.Graphics, w: number, h: number, s: number) => void): void {
    if (this.textures.exists(key)) this.textures.remove(key);
    const s = TEXTURE_SCALE;
    const w = 64 * s, h = 32 * s;
    const g = this.add.graphics();
    draw(g, w, h, s);
    g.generateTexture(key, w, h);
    g.destroy();
  }

  private fillDiamond(g: Phaser.GameObjects.Graphics, w: number, h: number, color: number): void {
    const hw = w / 2, hh = h / 2;
    g.fillStyle(color);
    g.fillPoints([
      new Phaser.Geom.Point(hw, 0),
      new Phaser.Geom.Point(w, hh),
      new Phaser.Geom.Point(hw, h),
      new Phaser.Geom.Point(0, hh),
    ], true);
  }

  private fillDiamondInset(g: Phaser.GameObjects.Graphics, w: number, h: number, inset: number): void {
    const hw = w / 2, hh = h / 2;
    const r = inset / w;
    g.fillPoints([
      new Phaser.Geom.Point(hw, hh * r * 2),
      new Phaser.Geom.Point(w - inset, hh),
      new Phaser.Geom.Point(hw, h - hh * r * 2),
      new Phaser.Geom.Point(inset, hh),
    ], true);
  }

  private strokeDiamond(g: Phaser.GameObjects.Graphics, w: number, h: number, color: number, alpha: number): void {
    const hw = w / 2, hh = h / 2;
    g.lineStyle(1 * TEXTURE_SCALE, color, alpha);
    g.beginPath();
    g.moveTo(hw, 0);
    g.lineTo(w, hh);
    g.lineTo(hw, h);
    g.lineTo(0, hh);
    g.closePath();
    g.strokePath();
  }

  private isInsideDiamond(x: number, y: number, w: number, h: number): boolean {
    const hw = w / 2, hh = h / 2;
    const dx = Math.abs(x - hw) / hw;
    const dy = Math.abs(y - hh) / hh;
    return dx + dy <= 1;
  }

  private makeSprite(key: string, baseW: number, baseH: number, draw: (g: Phaser.GameObjects.Graphics, s: number) => void): void {
    if (this.textures.exists(key)) this.textures.remove(key);
    const s = TEXTURE_SCALE;
    const g = this.add.graphics();
    draw(g, s);
    g.generateTexture(key, baseW * s, baseH * s);
    g.destroy();
  }
}
