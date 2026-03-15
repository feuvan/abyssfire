import Phaser from 'phaser';
import { TEXTURE_SCALE } from '../config';
import { cartToIso } from '../utils/IsometricUtils';
import type { NPCDefinition } from '../data/types';

export class NPC {
  scene: Phaser.Scene;
  sprite: Phaser.GameObjects.Container;
  definition: NPCDefinition;
  tileCol: number;
  tileRow: number;
  nameLabel: Phaser.GameObjects.Text;
  questMarker: Phaser.GameObjects.Text | null = null;
  private questMarkerTween: Phaser.Tweens.Tween | null = null;

  constructor(scene: Phaser.Scene, definition: NPCDefinition, col: number, row: number) {
    this.scene = scene;
    this.definition = definition;
    this.tileCol = col;
    this.tileRow = row;

    const worldPos = cartToIso(col, row);
    this.sprite = scene.add.container(worldPos.x, worldPos.y);
    this.sprite.setDepth(worldPos.y + 80);

    // Use loaded sprite texture if available, otherwise draw procedural fallback
    const spriteKey = `npc_${definition.type}`;
    if (scene.textures.exists(spriteKey)) {
      const img = scene.add.image(0, -32, spriteKey).setScale(1 / TEXTURE_SCALE);
      this.sprite.add(img);
    } else {
      this.drawProceduralNPC(scene);
    }

    // Quest marker (created for quest NPCs, updated dynamically)
    if (definition.type === 'quest') {
      this.questMarker = scene.add.text(0, -68, '!', {
        fontSize: '18px',
        color: '#f1c40f',
        fontFamily: '"Cinzel", serif',
        fontStyle: 'bold',
        stroke: '#000000',
        strokeThickness: 3,
      }).setOrigin(0.5);
      this.sprite.add(this.questMarker);
      this.questMarkerTween = scene.tweens.add({
        targets: this.questMarker, y: this.questMarker.y - 4, duration: 600,
        yoyo: true, repeat: -1, ease: 'Sine.easeInOut',
      });
    }

    // Name label
    this.nameLabel = scene.add.text(0, 12, definition.name, {
      fontSize: '11px',
      color: '#ffffff',
      fontFamily: '"Noto Sans SC", sans-serif',
      stroke: '#000000',
      strokeThickness: 3,
    }).setOrigin(0.5);
    this.sprite.add(this.nameLabel);

    // Interactive hit area covering the full NPC visual
    const hitZone = scene.add.rectangle(0, -20, 40, 70, 0xffffff, 0);
    hitZone.setInteractive({ useHandCursor: true });
    this.sprite.add(hitZone);
    this.sprite.setSize(40, 70);
    this.sprite.setInteractive(new Phaser.Geom.Rectangle(-20, -60, 40, 80), Phaser.Geom.Rectangle.Contains);
  }

  private getNPCColor(): number {
    switch (this.definition.type) {
      case 'blacksmith': return 0x8b4513;
      case 'merchant': return 0x2e86c1;
      case 'quest': return 0xd4a017;
      case 'stash': return 0x7d3c98;
      default: return 0x95a5a6;
    }
  }

  private getHatColor(): number {
    switch (this.definition.type) {
      case 'blacksmith': return 0x6c3483;
      case 'merchant': return 0x1abc9c;
      case 'quest': return 0xe67e22;
      case 'stash': return 0x5b2c6f;
      default: return 0x7f8c8d;
    }
  }

  private drawProceduralNPC(scene: Phaser.Scene): void {
    const color = this.getNPCColor();
    // Shadow
    const shadow = scene.add.ellipse(0, 6, 32, 10, 0x000000, 0.3);
    this.sprite.add(shadow);

    // Body
    const body = scene.add.rectangle(0, -20, 28, 38, color);
    body.setStrokeStyle(1.5, Phaser.Display.Color.IntegerToColor(color).darken(20).color);
    this.sprite.add(body);

    // Head
    const head = scene.add.circle(0, -44, 12, 0xffcc80);
    this.sprite.add(head);

    // Hat/identifier
    const hatColor = this.getHatColor();
    const hat = scene.add.rectangle(0, -54, 20, 8, hatColor);
    hat.setStrokeStyle(1, Phaser.Display.Color.IntegerToColor(hatColor).darken(15).color);
    this.sprite.add(hat);

    // Eyes
    const eye1 = scene.add.circle(-4, -46, 1.5, 0x2c3e50);
    const eye2 = scene.add.circle(4, -46, 1.5, 0x2c3e50);
    this.sprite.add(eye1);
    this.sprite.add(eye2);
  }

  setQuestMarker(text: string, color: string): void {
    if (!this.questMarker) return;
    if (text) {
      this.questMarker.setText(text).setColor(color).setVisible(true);
    } else {
      this.questMarker.setVisible(false);
    }
  }

  isNearPlayer(playerCol: number, playerRow: number, range = 2): boolean {
    const dist = Math.sqrt((this.tileCol - playerCol) ** 2 + (this.tileRow - playerRow) ** 2);
    return dist <= range;
  }
}
