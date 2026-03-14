import Phaser from 'phaser';

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
    const loadingText = this.add.text(width / 2, barY - 24, '锻造暗烬大陆...', {
      fontSize: '14px', color: '#c0934a', fontFamily: '"Cinzel", serif',
    }).setOrigin(0.5);
    this.load.on('progress', (v: number) => { fill.width = (barW - 4) * v; });
    this.load.on('complete', () => { loadingText.setText('准备就绪!'); });
  }

  create(): void {
    this.generateTiles();
    this.generatePlayerSprites();
    this.generateMonsterSprites();
    this.generateEffects();
    this.scene.start('MenuScene');
  }

  // ── HD Isometric Tiles (128x64) ──────────────────────────
  private generateTiles(): void {
    this.makeIsoTile('tile_grass', (g, w, h) => {
      // Base gradient
      this.fillDiamond(g, w, h, 0x4a8c3f);
      const cx = w / 2, cy = h / 2;
      // Organic color patches
      const patches = [
        { x: cx - 25, y: cy - 8, r: 18, c: 0x5ca04e },
        { x: cx + 15, y: cy + 5, r: 14, c: 0x3d7a34 },
        { x: cx - 10, y: cy + 10, r: 12, c: 0x68b85e },
        { x: cx + 28, y: cy - 4, r: 10, c: 0x448839 },
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
        const bh = 3 + Math.random() * 5;
        g.fillRect(bx, by - bh, 1.5, bh);
        g.fillRect(bx + 1, by - bh + 1, 1, bh - 1);
      }
      // Small flowers scattered
      for (let i = 0; i < 4; i++) {
        const fx = cx + (Math.random() - 0.5) * w * 0.5;
        const fy = cy + (Math.random() - 0.5) * h * 0.35;
        if (!this.isInsideDiamond(fx, fy, w, h)) continue;
        const fc = [0xf1c40f, 0xe8daf0, 0xf5b7b1, 0xa9dfbf][Math.floor(Math.random() * 4)];
        g.fillStyle(fc, 0.9);
        g.fillCircle(fx, fy, 2);
        g.fillStyle(fc, 0.5);
        g.fillCircle(fx - 1.5, fy - 1, 1.5);
        g.fillCircle(fx + 1.5, fy - 1, 1.5);
      }
      this.strokeDiamond(g, w, h, 0x2d6b25, 0.3);
    });

    this.makeIsoTile('tile_dirt', (g, w, h) => {
      this.fillDiamond(g, w, h, 0x8b7355);
      const cx = w / 2, cy = h / 2;
      // Weathered earth texture
      for (let i = 0; i < 20; i++) {
        const px = cx + (Math.random() - 0.5) * w * 0.6;
        const py = cy + (Math.random() - 0.5) * h * 0.4;
        if (!this.isInsideDiamond(px, py, w, h)) continue;
        const c = [0x7a6548, 0x9c8468, 0x6b5a42, 0xa89070, 0x776040][Math.floor(Math.random() * 5)];
        g.fillStyle(c, 0.4);
        g.fillEllipse(px, py, 4 + Math.random() * 6, 2 + Math.random() * 3);
      }
      // Pebbles
      for (let i = 0; i < 8; i++) {
        const px = cx + (Math.random() - 0.5) * w * 0.5;
        const py = cy + (Math.random() - 0.5) * h * 0.3;
        if (!this.isInsideDiamond(px, py, w, h)) continue;
        g.fillStyle(0x999080, 0.6);
        g.fillEllipse(px, py, 3, 2);
        g.fillStyle(0xb0a890, 0.4);
        g.fillEllipse(px - 0.5, py - 0.5, 2, 1.5);
      }
      // Worn path cracks
      g.lineStyle(1, 0x6b5a42, 0.25);
      g.beginPath();
      g.moveTo(cx - 30, cy - 2);
      g.lineTo(cx - 10, cy + 3);
      g.lineTo(cx + 15, cy - 1);
      g.lineTo(cx + 35, cy + 2);
      g.strokePath();
      this.strokeDiamond(g, w, h, 0x5a4a35, 0.35);
    });

    this.makeIsoTile('tile_stone', (g, w, h) => {
      this.fillDiamond(g, w, h, 0x6a6a6a);
      const cx = w / 2, cy = h / 2;
      // Stone texture with cracks
      g.fillStyle(0x7a7a7a, 0.4);
      g.fillEllipse(cx - 15, cy - 5, 24, 12);
      g.fillStyle(0x5a5a5a, 0.4);
      g.fillEllipse(cx + 18, cy + 3, 20, 10);
      g.fillStyle(0x808080, 0.3);
      g.fillEllipse(cx + 5, cy - 8, 16, 8);
      // Brick mortar lines
      g.lineStyle(1, 0x4a4a4a, 0.5);
      g.beginPath(); g.moveTo(cx - 32, cy); g.lineTo(cx + 32, cy); g.strokePath();
      g.beginPath(); g.moveTo(cx - 24, cy - 10); g.lineTo(cx + 24, cy - 10); g.strokePath();
      g.beginPath(); g.moveTo(cx - 20, cy + 10); g.lineTo(cx + 20, cy + 10); g.strokePath();
      // Vertical mortar
      g.beginPath(); g.moveTo(cx, cy - 10); g.lineTo(cx, cy); g.strokePath();
      g.beginPath(); g.moveTo(cx - 16, cy); g.lineTo(cx - 16, cy + 10); g.strokePath();
      g.beginPath(); g.moveTo(cx + 16, cy); g.lineTo(cx + 16, cy + 10); g.strokePath();
      // Highlight chips
      for (let i = 0; i < 6; i++) {
        const hx = cx + (Math.random() - 0.5) * w * 0.4;
        const hy = cy + (Math.random() - 0.5) * h * 0.3;
        if (!this.isInsideDiamond(hx, hy, w, h)) continue;
        g.fillStyle(0x8a8a8a, 0.5);
        g.fillRect(hx, hy, 3, 1.5);
      }
      this.strokeDiamond(g, w, h, 0x444444, 0.45);
    });

    this.makeIsoTile('tile_water', (g, w, h) => {
      this.fillDiamond(g, w, h, 0x1a5276);
      const cx = w / 2, cy = h / 2;
      // Deep water gradient
      g.fillStyle(0x2471a3, 0.6);
      this.fillDiamondInset(g, w, h, 8);
      g.fillStyle(0x2e86c1, 0.4);
      this.fillDiamondInset(g, w, h, 16);
      // Ripples
      g.lineStyle(1.5, 0x5dade2, 0.5);
      g.beginPath(); g.arc(cx - 15, cy - 4, 10, 0.3, 2.6, false); g.strokePath();
      g.beginPath(); g.arc(cx + 12, cy + 3, 8, 0.5, 2.8, false); g.strokePath();
      g.lineStyle(1, 0x85c1e9, 0.35);
      g.beginPath(); g.arc(cx + 5, cy - 8, 6, 0.2, 2.5, false); g.strokePath();
      // Shimmer highlights
      g.fillStyle(0xaed6f1, 0.5);
      g.fillEllipse(cx + 6, cy - 6, 6, 2);
      g.fillEllipse(cx - 18, cy + 2, 4, 1.5);
      g.fillStyle(0xd4efff, 0.3);
      g.fillEllipse(cx - 5, cy + 8, 5, 2);
      this.strokeDiamond(g, w, h, 0x154360, 0.5);
    });

    this.makeIsoTile('tile_wall', (g, w, h) => {
      // Wall with 3D height
      const wallHeight = 14;
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
      g.lineStyle(1, 0x2a2a2a, 0.6);
      const faceBottom = h;
      g.beginPath(); g.moveTo(8, faceBottom / 2 - 3); g.lineTo(w / 2 - 5, faceBottom - 6); g.strokePath();
      g.beginPath(); g.moveTo(4, faceBottom / 2 - 8); g.lineTo(w / 2 - 8, faceBottom - 14); g.strokePath();
      // Brick lines on right face
      g.beginPath(); g.moveTo(w / 2 + 5, faceBottom - 6); g.lineTo(w - 8, faceBottom / 2 - 3); g.strokePath();
      g.beginPath(); g.moveTo(w / 2 + 8, faceBottom - 14); g.lineTo(w - 4, faceBottom / 2 - 8); g.strokePath();
      // Edge highlights
      g.lineStyle(1, 0x666666, 0.3);
      g.beginPath();
      g.moveTo(w / 2, h - wallHeight);
      g.lineTo(w, h / 2 - wallHeight);
      g.strokePath();
    });

    this.makeIsoTile('tile_camp', (g, w, h) => {
      this.fillDiamond(g, w, h, 0x9e7c52);
      const cx = w / 2, cy = h / 2;
      // Wooden plank texture
      for (let i = -5; i <= 5; i++) {
        const ly = cy + i * 4;
        const inset = Math.abs(i) * 5;
        g.lineStyle(1, 0x7a5c34, 0.3);
        g.beginPath(); g.moveTo(cx - 40 + inset, ly); g.lineTo(cx + 40 - inset, ly); g.strokePath();
      }
      // Wood grain
      for (let i = 0; i < 12; i++) {
        const gx = cx + (Math.random() - 0.5) * w * 0.5;
        const gy = cy + (Math.random() - 0.5) * h * 0.4;
        if (!this.isInsideDiamond(gx, gy, w, h)) continue;
        g.fillStyle(0x8b6f47, 0.25);
        g.fillEllipse(gx, gy, 6, 1.5);
      }
      // Campfire glow
      g.fillStyle(0xf39c12, 0.2);
      g.fillCircle(cx, cy, 12);
      g.fillStyle(0xe74c3c, 0.25);
      g.fillCircle(cx, cy, 8);
      // Flame
      g.fillStyle(0xf39c12, 0.7);
      g.fillTriangle(cx - 3, cy + 3, cx + 3, cy + 3, cx, cy - 8);
      g.fillStyle(0xf1c40f, 0.8);
      g.fillTriangle(cx - 2, cy + 2, cx + 2, cy + 2, cx, cy - 5);
      g.fillStyle(0xffffff, 0.4);
      g.fillTriangle(cx - 1, cy + 1, cx + 1, cy + 1, cx, cy - 3);
      // Fire base - logs
      g.fillStyle(0x5d4037);
      g.fillEllipse(cx - 4, cy + 4, 8, 3);
      g.fillEllipse(cx + 4, cy + 5, 8, 3);
      this.strokeDiamond(g, w, h, 0x6b5a42, 0.4);
    });
  }

  // ── HD Player Sprites (64x96) ────────────────────────────
  private generatePlayerSprites(): void {
    const classes: [string, number, number, number, string][] = [
      ['player_warrior', 0x3a7bd5, 0x2a5fa8, 0x78909c, 'sword'],
      ['player_mage',    0x9b59b6, 0x7d3c98, 0x7b1fa2, 'staff'],
      ['player_rogue',   0x27ae60, 0x1e8449, 0x2e7d32, 'dagger'],
    ];
    for (const [key, bodyColor, darkColor, accentColor, weapon] of classes) {
      this.makeSprite(key, 64, 96, (g) => {
        const cx = 32, baseY = 88;
        // Shadow
        g.fillStyle(0x000000, 0.3);
        g.fillEllipse(cx, baseY, 36, 10);

        // Boots
        g.fillStyle(0x4e342e);
        g.fillRoundedRect(18, 74, 10, 10, 2);
        g.fillRoundedRect(34, 74, 10, 10, 2);
        g.fillStyle(0x3e2723);
        g.fillRect(18, 78, 10, 3);
        g.fillRect(34, 78, 10, 3);
        // Boot straps
        g.fillStyle(0x6d4c41, 0.6);
        g.fillRect(19, 76, 8, 1);
        g.fillRect(35, 76, 8, 1);

        // Legs
        g.fillStyle(0x546e7a);
        g.fillRoundedRect(20, 60, 8, 16, 2);
        g.fillRoundedRect(34, 60, 8, 16, 2);
        // Knee guards
        g.fillStyle(darkColor, 0.5);
        g.fillRoundedRect(20, 64, 8, 5, 1);
        g.fillRoundedRect(34, 64, 8, 5, 1);

        // Body armor
        g.fillStyle(bodyColor);
        g.fillRoundedRect(16, 38, 30, 24, 4);
        // Chest plate detail
        g.fillStyle(darkColor);
        g.fillRoundedRect(16, 38, 30, 4, { tl: 4, tr: 4, bl: 0, br: 0 });
        g.fillRoundedRect(16, 58, 30, 4, { tl: 0, tr: 0, bl: 4, br: 4 });
        // Center line
        g.fillStyle(darkColor, 0.4);
        g.fillRect(30, 42, 2, 16);
        // Emblem
        g.fillStyle(0xfdd835, 0.6);
        g.fillCircle(cx, 50, 3);

        // Belt
        g.fillStyle(0x6d4c41);
        g.fillRect(16, 58, 30, 4);
        g.fillStyle(0xfdd835, 0.8);
        g.fillRoundedRect(28, 58, 6, 4, 1);

        // Arms
        g.fillStyle(bodyColor);
        g.fillRoundedRect(10, 40, 7, 16, 3);
        g.fillRoundedRect(45, 40, 7, 16, 3);
        // Pauldrons
        g.fillStyle(darkColor);
        g.fillEllipse(13.5, 40, 10, 6);
        g.fillEllipse(48.5, 40, 10, 6);
        g.fillStyle(accentColor, 0.3);
        g.fillEllipse(13.5, 39, 8, 4);
        g.fillEllipse(48.5, 39, 8, 4);

        // Hands
        g.fillStyle(0xffcc80);
        g.fillCircle(13, 57, 4);
        g.fillCircle(49, 57, 4);

        // Neck
        g.fillStyle(0xffcc80);
        g.fillRect(27, 30, 8, 8);

        // Head
        g.fillStyle(0xffcc80);
        g.fillRoundedRect(21, 14, 20, 20, 6);
        // Eyes
        g.fillStyle(0x2c3e50);
        g.fillEllipse(26, 24, 3.5, 4);
        g.fillEllipse(36, 24, 3.5, 4);
        g.fillStyle(0xffffff);
        g.fillCircle(26.5, 23, 1);
        g.fillCircle(36.5, 23, 1);
        // Nose
        g.fillStyle(0xf0bb7a, 0.5);
        g.fillEllipse(31, 27, 3, 2);
        // Mouth
        g.fillStyle(0xc0946a, 0.6);
        g.fillEllipse(31, 30, 4, 1.5);

        // Class-specific headgear + weapon
        if (weapon === 'sword') {
          // Steel helm
          g.fillStyle(0x78909c);
          g.fillRoundedRect(19, 10, 24, 14, 5);
          g.fillStyle(0x90a4ae);
          g.fillRoundedRect(21, 8, 20, 6, 4);
          g.fillStyle(0x607d8b);
          g.fillRect(19, 22, 24, 2);
          // Nose guard
          g.fillStyle(0x78909c);
          g.fillRect(29, 22, 4, 6);
          // Helm crest
          g.fillStyle(0xe74c3c, 0.7);
          g.fillRect(29, 5, 4, 6);
          g.fillRect(28, 5, 6, 2);
          // Sword (right side)
          g.fillStyle(0xbdc3c7);
          g.fillRect(52, 20, 3, 34);
          g.fillStyle(0xecf0f1);
          g.fillRect(52.5, 20, 1.5, 30);
          // Crossguard
          g.fillStyle(0x8d6e63);
          g.fillRoundedRect(49, 50, 9, 4, 1);
          // Pommel
          g.fillStyle(0xfdd835);
          g.fillCircle(53.5, 56, 2.5);
          // Shield (left side)
          g.fillStyle(0x2c3e50);
          g.fillRoundedRect(2, 42, 12, 16, 3);
          g.fillStyle(0x3498db, 0.5);
          g.fillRoundedRect(4, 44, 8, 12, 2);
          g.fillStyle(0xfdd835, 0.6);
          g.fillRect(7, 45, 2, 10);
          g.fillRect(4, 49, 8, 2);
        } else if (weapon === 'staff') {
          // Wizard hat
          g.fillStyle(0x6a1b9a);
          g.fillRoundedRect(17, 12, 28, 10, 4);
          g.fillTriangle(31, -4, 21, 15, 41, 15);
          g.fillStyle(0x8e24aa, 0.5);
          g.fillTriangle(31, -2, 24, 14, 38, 14);
          // Hat brim
          g.fillStyle(0x4a148c);
          g.fillEllipse(31, 18, 30, 6);
          // Star on hat
          g.fillStyle(0xfdd835, 0.9);
          g.fillCircle(31, 4, 3);
          g.fillRect(30, 1, 2, 1);
          // Staff
          g.fillStyle(0x5d4037);
          g.fillRoundedRect(52, 8, 3, 52, 1);
          g.fillStyle(0x4e342e);
          g.fillRect(52.5, 10, 2, 48);
          // Orb on top
          g.fillStyle(0x7e57c2);
          g.fillCircle(53.5, 8, 6);
          g.fillStyle(0xce93d8, 0.6);
          g.fillCircle(52, 6, 3);
          g.fillStyle(0xffffff, 0.3);
          g.fillCircle(51, 5, 1.5);
          // Glow
          g.fillStyle(0xce93d8, 0.15);
          g.fillCircle(53.5, 8, 10);
        } else {
          // Rogue hood
          g.fillStyle(0x2e7d32);
          g.fillRoundedRect(18, 10, 26, 16, 5);
          g.fillStyle(0x1b5e20);
          g.fillTriangle(31, 6, 20, 14, 42, 14);
          // Hood shadow
          g.fillStyle(0x1a5c1f, 0.4);
          g.fillRoundedRect(20, 20, 22, 4, 1);
          // Mask
          g.fillStyle(0x1b5e20, 0.7);
          g.fillRect(22, 27, 18, 4);
          // Dual daggers
          g.fillStyle(0xbdc3c7);
          g.fillRect(52, 38, 2, 18);
          g.fillStyle(0xecf0f1);
          g.fillRect(52.3, 38, 1, 16);
          g.fillStyle(0x6d4c41);
          g.fillRoundedRect(50, 54, 6, 3, 1);
          // Left dagger
          g.fillStyle(0xbdc3c7);
          g.fillRect(8, 38, 2, 18);
          g.fillStyle(0xecf0f1);
          g.fillRect(8.3, 38, 1, 16);
          g.fillStyle(0x6d4c41);
          g.fillRoundedRect(6, 54, 6, 3, 1);
          // Quiver on back
          g.fillStyle(0x5d4037);
          g.fillRoundedRect(40, 32, 6, 18, 2);
          g.fillStyle(0x8d6e63, 0.5);
          g.fillRect(41, 33, 4, 2);
          // Arrow feathers
          g.fillStyle(0xfdd835, 0.6);
          g.fillRect(42, 30, 1, 4);
          g.fillRect(44, 31, 1, 3);
        }
      });
    }
  }

  // ── HD Monster Sprites ───────────────────────────────────
  private generateMonsterSprites(): void {
    // Slime (48x40)
    this.makeSprite('monster_slime', 48, 40, (g) => {
      g.fillStyle(0x000000, 0.2);
      g.fillEllipse(24, 36, 32, 8);
      // Body
      g.fillStyle(0x27ae60);
      g.fillEllipse(24, 24, 36, 28);
      // Highlight
      g.fillStyle(0x58d68d, 0.5);
      g.fillEllipse(20, 18, 14, 10);
      g.fillStyle(0x82e0aa, 0.3);
      g.fillEllipse(18, 15, 8, 6);
      // Drips
      g.fillStyle(0x27ae60, 0.7);
      g.fillEllipse(14, 36, 6, 4);
      g.fillEllipse(34, 35, 5, 3);
      // Eyes
      g.fillStyle(0xffffff);
      g.fillEllipse(18, 20, 7, 8);
      g.fillEllipse(30, 20, 7, 8);
      g.fillStyle(0x1a5c1f);
      g.fillCircle(19, 21, 2.5);
      g.fillCircle(31, 21, 2.5);
      g.fillStyle(0x000000);
      g.fillCircle(19.5, 21.5, 1);
      g.fillCircle(31.5, 21.5, 1);
      // Mouth
      g.lineStyle(1.5, 0x1a6e30, 0.6);
      g.beginPath(); g.arc(24, 27, 5, 0.2, 2.9, false); g.strokePath();
    });

    // Goblin (48x56)
    this.makeSprite('monster_goblin', 48, 56, (g) => {
      g.fillStyle(0x000000, 0.2);
      g.fillEllipse(24, 52, 28, 8);
      // Feet
      g.fillStyle(0x4e6b3a);
      g.fillEllipse(17, 50, 8, 5);
      g.fillEllipse(31, 50, 8, 5);
      // Legs
      g.fillStyle(0x558b2f);
      g.fillRoundedRect(14, 38, 7, 14, 2);
      g.fillRoundedRect(27, 38, 7, 14, 2);
      // Body - tattered vest
      g.fillStyle(0x689f38);
      g.fillRoundedRect(12, 22, 24, 18, 4);
      g.fillStyle(0x5d4037, 0.6);
      g.fillRoundedRect(14, 24, 20, 14, 2);
      g.fillStyle(0x4e342e, 0.4);
      g.fillRect(23, 26, 2, 10);
      // Arms
      g.fillStyle(0x7cb342);
      g.fillRoundedRect(8, 24, 6, 14, 2);
      g.fillRoundedRect(34, 26, 6, 12, 2);
      // Hands
      g.fillStyle(0x8bc34a);
      g.fillCircle(11, 38, 3.5);
      g.fillCircle(37, 38, 3.5);
      // Head
      g.fillStyle(0x7cb342);
      g.fillRoundedRect(14, 6, 20, 18, 6);
      // Ears (big pointy)
      g.fillStyle(0x8bc34a);
      g.fillTriangle(8, 14, 14, 8, 14, 18);
      g.fillTriangle(40, 14, 34, 8, 34, 18);
      g.fillStyle(0xaed581, 0.4);
      g.fillTriangle(10, 14, 14, 10, 14, 16);
      g.fillTriangle(38, 14, 34, 10, 34, 16);
      // Eyes
      g.fillStyle(0xffeb3b);
      g.fillEllipse(20, 14, 6, 7);
      g.fillEllipse(30, 14, 6, 7);
      g.fillStyle(0x000000);
      g.fillCircle(21, 15, 2);
      g.fillCircle(31, 15, 2);
      // Nose
      g.fillStyle(0x689f38);
      g.fillEllipse(25, 18, 5, 3);
      // Mouth
      g.lineStyle(1, 0x33691e, 0.7);
      g.beginPath(); g.arc(25, 21, 4, 0.1, 3.0, false); g.strokePath();
      // Club
      g.fillStyle(0x5d4037);
      g.fillRoundedRect(38, 20, 4, 20, 1);
      g.fillStyle(0x795548);
      g.fillRoundedRect(36, 16, 8, 6, 2);
      // Nails in club
      g.fillStyle(0xbdbdbd, 0.7);
      g.fillCircle(38, 18, 1);
      g.fillCircle(42, 19, 1);
    });

    // Goblin Chief (60x68)
    this.makeSprite('monster_goblin_chief', 60, 68, (g) => {
      g.fillStyle(0x000000, 0.25);
      g.fillEllipse(30, 64, 36, 10);
      // Feet
      g.fillStyle(0x3e5228);
      g.fillEllipse(21, 62, 10, 6);
      g.fillEllipse(39, 62, 10, 6);
      // Legs
      g.fillStyle(0x4a7023);
      g.fillRoundedRect(17, 46, 10, 18, 3);
      g.fillRoundedRect(33, 46, 10, 18, 3);
      // Body - armored
      g.fillStyle(0x558b2f);
      g.fillRoundedRect(13, 26, 34, 22, 5);
      g.fillStyle(0x795548);
      g.fillRoundedRect(16, 28, 28, 18, 3);
      g.fillStyle(0x8d6e63, 0.5);
      g.fillRect(29, 30, 2, 14);
      // Skull belt buckle
      g.fillStyle(0xe0e0e0);
      g.fillCircle(30, 46, 4);
      g.fillStyle(0x000000);
      g.fillCircle(28, 45, 1); g.fillCircle(32, 45, 1);
      // Arms
      g.fillStyle(0x689f38);
      g.fillRoundedRect(7, 28, 8, 18, 3);
      g.fillRoundedRect(45, 28, 8, 18, 3);
      // Pauldrons
      g.fillStyle(0x795548);
      g.fillEllipse(11, 28, 12, 7);
      g.fillEllipse(49, 28, 12, 7);
      // Head
      g.fillStyle(0x7cb342);
      g.fillRoundedRect(16, 6, 28, 22, 7);
      // Ears
      g.fillStyle(0x8bc34a);
      g.fillTriangle(8, 16, 16, 8, 16, 22);
      g.fillTriangle(52, 16, 44, 8, 44, 22);
      // Eyes (angry red)
      g.fillStyle(0xff1744);
      g.fillEllipse(24, 16, 6, 7);
      g.fillEllipse(38, 16, 6, 7);
      g.fillStyle(0x000000);
      g.fillCircle(25, 17, 2);
      g.fillCircle(39, 17, 2);
      // Crown
      g.fillStyle(0xfdd835);
      g.fillRoundedRect(17, 2, 26, 6, 2);
      g.fillTriangle(20, 2, 22, -4, 24, 2);
      g.fillTriangle(28, 2, 30, -6, 32, 2);
      g.fillTriangle(36, 2, 38, -4, 40, 2);
      g.fillStyle(0xe53935);
      g.fillCircle(30, -2, 2);
      // Battle axe
      g.fillStyle(0x5d4037);
      g.fillRoundedRect(52, 10, 3, 40, 1);
      g.fillStyle(0x757575);
      g.beginPath();
      g.moveTo(55, 10); g.lineTo(62, 16); g.lineTo(62, 22); g.lineTo(55, 26);
      g.closePath(); g.fillPath();
      g.fillStyle(0x9e9e9e, 0.5);
      g.beginPath();
      g.moveTo(55, 12); g.lineTo(60, 16); g.lineTo(60, 22); g.lineTo(55, 24);
      g.closePath(); g.fillPath();
    });

    // Skeleton (44x60)
    this.makeSprite('monster_skeleton', 44, 60, (g) => {
      g.fillStyle(0x000000, 0.2);
      g.fillEllipse(22, 56, 28, 7);
      // Feet
      g.fillStyle(0xbdbdbd);
      g.fillEllipse(15, 55, 7, 4);
      g.fillEllipse(29, 55, 7, 4);
      // Leg bones
      g.fillStyle(0xe0e0e0);
      g.fillRoundedRect(14, 40, 4, 16, 1);
      g.fillRoundedRect(26, 40, 4, 16, 1);
      // Pelvis
      g.fillStyle(0xd0d0d0);
      g.fillEllipse(22, 40, 16, 6);
      // Ribcage
      g.fillStyle(0xe0e0e0);
      g.fillRoundedRect(14, 22, 16, 18, 4);
      // Ribs
      for (let i = 0; i < 4; i++) {
        const ry = 26 + i * 4;
        g.lineStyle(1.5, 0xbdbdbd, 0.7);
        g.beginPath(); g.arc(22, ry, 6, 0.3, 2.8, false); g.strokePath();
      }
      // Spine
      g.fillStyle(0xc8c8c8);
      g.fillRect(21, 22, 2, 18);
      // Arms
      g.fillStyle(0xe0e0e0);
      g.fillRoundedRect(8, 24, 4, 16, 1);
      g.fillRoundedRect(32, 24, 4, 16, 1);
      // Hands
      g.fillStyle(0xd0d0d0);
      g.fillCircle(10, 40, 3);
      g.fillCircle(34, 40, 3);
      // Skull
      g.fillStyle(0xeeeeee);
      g.fillRoundedRect(13, 6, 18, 18, 6);
      // Eye sockets
      g.fillStyle(0x1a1a1a);
      g.fillEllipse(18, 14, 5, 6);
      g.fillEllipse(28, 14, 5, 6);
      // Glowing eyes
      g.fillStyle(0x66bb6a, 0.8);
      g.fillCircle(18, 14, 1.5);
      g.fillCircle(28, 14, 1.5);
      // Nose hole
      g.fillStyle(0x2a2a2a);
      g.fillTriangle(22, 18, 24, 18, 23, 20);
      // Jaw
      g.fillStyle(0xd0d0d0);
      g.fillRoundedRect(15, 22, 14, 4, 1);
      g.fillStyle(0xe0e0e0);
      for (let i = 0; i < 5; i++) g.fillRect(16 + i * 2.5, 22, 1.5, 2);
      // Rusty sword
      g.fillStyle(0x8d6e63);
      g.fillRoundedRect(36, 18, 3, 24, 1);
      g.fillStyle(0xa1887f, 0.5);
      g.fillRect(36.5, 18, 1.5, 20);
      g.fillStyle(0x5d4037);
      g.fillRoundedRect(34, 40, 7, 3, 1);
    });

    // Zombie (44x60)
    this.makeSprite('monster_zombie', 44, 60, (g) => {
      g.fillStyle(0x000000, 0.2);
      g.fillEllipse(22, 56, 28, 7);
      // Ragged feet
      g.fillStyle(0x5d4037, 0.7);
      g.fillEllipse(15, 55, 8, 4);
      g.fillEllipse(29, 54, 7, 5);
      // Legs (torn pants)
      g.fillStyle(0x4e342e);
      g.fillRoundedRect(13, 40, 7, 16, 2);
      g.fillRoundedRect(25, 40, 7, 16, 2);
      g.fillStyle(0x6d8b74, 0.4);
      g.fillRect(14, 48, 5, 4);
      // Body (rotting)
      g.fillStyle(0x6d8b74);
      g.fillRoundedRect(12, 22, 20, 20, 4);
      g.fillStyle(0x4e342e, 0.6);
      g.fillRoundedRect(14, 24, 16, 16, 2);
      // Exposed ribs
      g.lineStyle(1, 0x8fad8b, 0.4);
      g.beginPath(); g.moveTo(16, 30); g.lineTo(28, 30); g.strokePath();
      g.beginPath(); g.moveTo(16, 34); g.lineTo(28, 34); g.strokePath();
      // Arms (asymmetric - zombie!)
      g.fillStyle(0x7da17a);
      g.fillRoundedRect(6, 24, 7, 18, 2);
      g.fillStyle(0x6d8b74);
      g.fillRoundedRect(33, 28, 6, 14, 2);
      // Hands
      g.fillStyle(0x8fad8b);
      g.fillCircle(9, 42, 4);
      g.fillCircle(36, 42, 3);
      // Head
      g.fillStyle(0x8fad8b);
      g.fillRoundedRect(14, 6, 18, 18, 5);
      // Messy hair
      g.fillStyle(0x4a4a4a);
      g.fillRoundedRect(14, 4, 18, 8, 3);
      g.fillRect(12, 8, 4, 6);
      // Eyes (one droopy)
      g.fillStyle(0xc62828);
      g.fillEllipse(19, 14, 4, 5);
      g.fillStyle(0xb71c1c);
      g.fillEllipse(29, 15, 4, 4);
      g.fillStyle(0x000000);
      g.fillCircle(19, 14, 1.5);
      g.fillCircle(29, 15, 1);
      // Mouth (open, moaning)
      g.fillStyle(0x3e2723);
      g.fillEllipse(23, 20, 8, 4);
      g.fillStyle(0xeeeeee, 0.5);
      g.fillRect(20, 19, 2, 2); g.fillRect(25, 19, 2, 2);
    });

    // Werewolf (52x64)
    this.makeSprite('monster_werewolf', 52, 64, (g) => {
      g.fillStyle(0x000000, 0.25);
      g.fillEllipse(26, 60, 36, 10);
      // Feet (paws)
      g.fillStyle(0x4e342e);
      g.fillEllipse(17, 58, 10, 6);
      g.fillEllipse(35, 58, 10, 6);
      // Claws on feet
      g.fillStyle(0xe0e0e0);
      g.fillRect(13, 55, 1.5, 3); g.fillRect(16, 55, 1.5, 3); g.fillRect(19, 55, 1.5, 3);
      g.fillRect(31, 55, 1.5, 3); g.fillRect(34, 55, 1.5, 3); g.fillRect(37, 55, 1.5, 3);
      // Legs (muscular)
      g.fillStyle(0x5d4037);
      g.fillRoundedRect(13, 42, 10, 18, 3);
      g.fillRoundedRect(29, 42, 10, 18, 3);
      // Body (broad, furry)
      g.fillStyle(0x6d4c41);
      g.fillRoundedRect(10, 22, 32, 22, 6);
      // Chest tuft
      g.fillStyle(0x8d6e63, 0.6);
      g.fillEllipse(26, 30, 14, 10);
      // Arms (powerful)
      g.fillStyle(0x5d4037);
      g.fillRoundedRect(4, 24, 8, 20, 3);
      g.fillRoundedRect(40, 24, 8, 20, 3);
      // Claws
      g.fillStyle(0xe0e0e0);
      for (let i = 0; i < 3; i++) {
        g.fillRect(4 + i * 2.5, 43, 1.5, 4);
        g.fillRect(40 + i * 2.5, 43, 1.5, 4);
      }
      // Head (wolf)
      g.fillStyle(0x6d4c41);
      g.fillRoundedRect(14, 4, 24, 20, 7);
      // Ears
      g.fillStyle(0x5d4037);
      g.fillTriangle(14, 8, 18, -2, 22, 8);
      g.fillTriangle(38, 8, 34, -2, 30, 8);
      g.fillStyle(0xa1887f, 0.4);
      g.fillTriangle(16, 7, 18, 0, 20, 7);
      g.fillTriangle(36, 7, 34, 0, 32, 7);
      // Snout
      g.fillStyle(0x8d6e63);
      g.fillRoundedRect(20, 16, 12, 8, 3);
      g.fillStyle(0x3e2723);
      g.fillEllipse(26, 17, 4, 2);
      // Mouth
      g.lineStyle(1, 0x3e2723, 0.8);
      g.beginPath(); g.moveTo(20, 22); g.lineTo(32, 22); g.strokePath();
      // Fangs
      g.fillStyle(0xeeeeee);
      g.fillTriangle(22, 22, 23, 25, 24, 22);
      g.fillTriangle(28, 22, 29, 25, 30, 22);
      // Eyes (glowing)
      g.fillStyle(0xffeb3b);
      g.fillEllipse(20, 12, 5, 5);
      g.fillEllipse(32, 12, 5, 5);
      g.fillStyle(0xff8f00);
      g.fillCircle(20, 12, 1.5);
      g.fillCircle(32, 12, 1.5);
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
    for (const [key, baseColor, w, h, type] of defs) {
      this.makeSprite(key, w, h, (g) => {
        const cx = w / 2, cy = h / 2;
        const light = Phaser.Display.Color.IntegerToColor(baseColor).lighten(20).color;
        const dark = Phaser.Display.Color.IntegerToColor(baseColor).darken(15).color;

        // Shadow
        g.fillStyle(0x000000, 0.25);
        g.fillEllipse(cx, h - 4, w * 0.65, h * 0.08);

        // Body
        g.fillStyle(baseColor);
        g.fillRoundedRect(w * 0.2, h * 0.32, w * 0.6, h * 0.42, w * 0.08);
        g.fillStyle(light, 0.3);
        g.fillEllipse(cx, h * 0.45, w * 0.35, h * 0.15);

        // Legs
        g.fillStyle(dark);
        g.fillRoundedRect(w * 0.25, h * 0.7, w * 0.15, h * 0.22, 3);
        g.fillRoundedRect(w * 0.55, h * 0.7, w * 0.15, h * 0.22, 3);

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
        g.fillRoundedRect(w * 0.06, h * 0.35, w * 0.14, h * 0.25, 3);
        g.fillRoundedRect(w * 0.78, h * 0.35, w * 0.14, h * 0.25, 3);

        // Type-specific details
        if (type === 'fire') {
          // Flame particles
          g.fillStyle(0xff6f00, 0.6);
          g.fillTriangle(cx - 8, h * 0.15, cx - 4, h * 0.02, cx, h * 0.15);
          g.fillTriangle(cx + 2, h * 0.12, cx + 6, -2, cx + 10, h * 0.12);
          g.fillStyle(0xffab00, 0.4);
          g.fillTriangle(cx - 3, h * 0.13, cx, h * 0.04, cx + 3, h * 0.13);
          g.fillStyle(0xff6f00, 0.3);
          g.fillEllipse(w * 0.15, h * 0.3, 6, 8);
          g.fillEllipse(w * 0.85, h * 0.35, 6, 8);
        }
        if (type === 'demon') {
          // Horns
          g.fillStyle(0x212121);
          g.fillTriangle(w * 0.2, h * 0.12, w * 0.12, -2, w * 0.28, h * 0.12);
          g.fillTriangle(w * 0.72, h * 0.12, w * 0.8, -2, w * 0.64, h * 0.12);
          // Tail
          g.lineStyle(2, dark, 0.7);
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
          g.fillEllipse(cx - 6, h * 0.4, 8, 5);
          g.fillEllipse(cx + 8, h * 0.45, 6, 4);
          g.lineStyle(1, 0x555555, 0.4);
          g.beginPath(); g.moveTo(cx - 10, h * 0.42); g.lineTo(cx + 10, h * 0.48); g.strokePath();
        }
        if (type === 'scorpion') {
          // Claws
          g.fillStyle(dark);
          g.fillEllipse(w * 0.08, h * 0.35, 8, 5);
          g.fillEllipse(w * 0.92, h * 0.35, 8, 5);
          // Tail
          g.lineStyle(3, baseColor, 0.8);
          g.beginPath();
          g.moveTo(cx, h * 0.3);
          g.lineTo(cx + 5, h * 0.15);
          g.lineTo(cx + 8, h * 0.05);
          g.strokePath();
          g.fillStyle(0xc62828);
          g.fillCircle(cx + 8, h * 0.04, 3);
        }
        if (type === 'worm') {
          // Segmented body
          for (let i = 0; i < 4; i++) {
            g.lineStyle(1, dark, 0.4);
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
          g.fillTriangle(cx - 6, h * 0.08, cx - 3, -3, cx, h * 0.08);
          g.fillTriangle(cx, h * 0.06, cx + 3, -5, cx + 6, h * 0.06);
          g.fillTriangle(cx + 6, h * 0.08, cx + 9, -3, cx + 12, h * 0.08);
          // Aura
          g.lineStyle(1.5, 0xffd700, 0.25);
          g.strokeCircle(cx, cy * 0.8, w * 0.42);
        }
        if (key === 'monster_demon_lord') {
          // Wings
          g.fillStyle(0x311b92, 0.6);
          g.beginPath();
          g.moveTo(w * 0.15, h * 0.3);
          g.lineTo(0, h * 0.08);
          g.lineTo(-4, h * 0.2);
          g.lineTo(w * 0.05, h * 0.45);
          g.closePath(); g.fillPath();
          g.beginPath();
          g.moveTo(w * 0.85, h * 0.3);
          g.lineTo(w, h * 0.08);
          g.lineTo(w + 4, h * 0.2);
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
    this.makeSprite('loot_bag', 24, 24, (g) => {
      g.fillStyle(0x000000, 0.2);
      g.fillEllipse(12, 22, 16, 5);
      g.fillStyle(0x8d6e63);
      g.fillRoundedRect(5, 6, 14, 16, 3);
      g.fillStyle(0xa1887f);
      g.fillRoundedRect(7, 3, 10, 5, 2);
      g.fillStyle(0x6d4c41);
      g.fillRect(6, 8, 12, 2);
      g.fillStyle(0xfdd835, 0.8);
      g.fillCircle(12, 14, 3);
      g.fillStyle(0xf9a825, 0.5);
      g.fillCircle(11, 13, 1.5);
    });

    // Exit portal (32x32)
    this.makeSprite('exit_portal', 32, 32, (g) => {
      g.fillStyle(0x00e676, 0.2);
      g.fillCircle(16, 16, 14);
      g.fillStyle(0x00e676, 0.4);
      g.fillCircle(16, 16, 10);
      g.fillStyle(0x69f0ae, 0.6);
      g.fillCircle(16, 16, 6);
      g.fillStyle(0xffffff, 0.3);
      g.fillCircle(16, 16, 3);
      g.lineStyle(1.5, 0x00e676, 0.5);
      g.strokeCircle(16, 16, 12);
    });
  }

  // ── Helpers ──────────────────────────────────────────────
  private makeIsoTile(key: string, draw: (g: Phaser.GameObjects.Graphics, w: number, h: number) => void): void {
    const w = 128, h = 64;
    const g = this.add.graphics();
    draw(g, w, h);
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
    g.lineStyle(1, color, alpha);
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

  private makeSprite(key: string, w: number, h: number, draw: (g: Phaser.GameObjects.Graphics) => void): void {
    const g = this.add.graphics();
    draw(g);
    g.generateTexture(key, w, h);
    g.destroy();
  }
}
